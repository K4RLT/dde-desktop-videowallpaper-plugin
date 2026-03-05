// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wallpaperengine.h"
#include "wallpaperengine_p.h"
#include "wallpaperconfig.h"
#include "videowallpapermenuscene.h"

#include <dfm-base/dfm_desktop_defines.h>
#include <dfm-base/utils/universalutils.h>
#include <desktoputils/ddpugin_eventinterface_helper.h>
#include <desktoputils/menu_eventinterface_helper.h>
#include <desktoputils/widgetutil.h>

#include <DPlatformWindowHandle>
#include <DNotifySender>

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>

#include <malloc.h>

using namespace ddplugin_videowallpaper;
DFMBASE_USE_NAMESPACE

#define CanvasCoreSubscribe(topic, func) \
    dpfSignalDispatcher->subscribe("ddplugin_core", QT_STRINGIFY2(topic), this, func)

#define CanvasCoreUnsubscribe(topic, func) \
    dpfSignalDispatcher->unsubscribe("ddplugin_core", QT_STRINGIFY2(topic), this, func)

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString getScreenName(QWidget *win)
{
    Q_ASSERT(win);
    return win->property(DesktopFrameProperty::kPropScreenName).toString();
}

static QMap<QString, QWidget *> rootMap()
{
    QMap<QString, QWidget *> ret;
    for (QWidget *win : ddplugin_desktop_util::desktopFrameRootWindows()) {
        const QString name = getScreenName(win);
        if (!name.isEmpty())
            ret.insert(name, win);
    }
    return ret;
}

static QUrl resolveVideoUrl(const QList<QUrl> &videos)
{
    const QString selected = WpCfg->videoPath();
    if (!selected.isEmpty()) {
        for (const QUrl &v : videos) {
            if (v.toLocalFile() == selected)
                return v;
        }
    }
    if (!videos.isEmpty()) {
        WpCfg->setVideoPath(videos.constFirst().toLocalFile());
        return videos.constFirst();
    }
    return QUrl();
}

// ── Private implementation ────────────────────────────────────────────────────

WallpaperEnginePrivate::WallpaperEnginePrivate(WallpaperEngine *qq)
    : q(qq)
{
    openXcb();
}

void WallpaperEnginePrivate::openXcb()
{
    if (xcbReady)
        return;
    xcbConn = xcb_connect(nullptr, nullptr);
    if (xcbConn && !xcb_connection_has_error(xcbConn)) {
        xcb_intern_atom_cookie_t *cookie = xcb_ewmh_init_atoms(xcbConn, &xcbEwmh);
        xcb_ewmh_init_atoms_replies(&xcbEwmh, cookie, nullptr);
        xcbReady = true;
    }
}

void WallpaperEnginePrivate::closeXcb()
{
    if (!xcbReady)
        return;
    xcb_ewmh_connection_wipe(&xcbEwmh);
    xcb_disconnect(xcbConn);
    xcbConn  = nullptr;
    xcbReady = false;
}

QList<QUrl> WallpaperEnginePrivate::getVideos(const QString &path)
{
    QList<QUrl> ret;
    for (const QFileInfo &file : QDir(path).entryInfoList(QDir::Files))
        ret << QUrl::fromLocalFile(file.absoluteFilePath());
    return ret;
}

QString WallpaperEnginePrivate::sourcePath() const
{
    return QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first()
           + "/video-wallpaper";
}

VideoProxyPointer WallpaperEnginePrivate::createWidget(QWidget *root)
{
    root->windowHandle()->setSurfaceType(QSurface::OpenGLSurface);

    VideoProxyPointer bwp(new VideoProxy(root));
    bwp->setProperty(DesktopFrameProperty::kPropScreenName, getScreenName(root));
    bwp->setProperty(DesktopFrameProperty::kPropWidgetName, "videowallpaper");
    bwp->setProperty(DesktopFrameProperty::kPropWidgetLevel, 5.1);
    bwp->setGeometry(relativeGeometry(root->geometry()));
    bwp->hide();

    fmDebug() << "created widget for screen" << getScreenName(root);
    return bwp;
}

void WallpaperEnginePrivate::clearWidgets()
{
    for (const VideoProxyPointer &bwp : widgets.values()) {
        if (bwp)
            bwp->shutdownMpv();
    }
    widgets.clear();
}

void WallpaperEnginePrivate::setBackgroundVisible(bool v)
{
    for (QWidget *root : ddplugin_desktop_util::desktopFrameRootWindows()) {
        for (QObject *obj : root->children()) {
            if (QWidget *wid = qobject_cast<QWidget *>(obj)) {
                if (wid->property(DesktopFrameProperty::kPropWidgetName).toString() == "background")
                    wid->setVisible(v);
            }
        }
    }
}

void WallpaperEnginePrivate::applyScaleMode(const QString &mode)
{
    // fill  → keepaspect=no,  panscan=0  (stretch to widget, may distort)
    // fit   → keepaspect=yes, panscan=0  (letterbox / pillarbox)
    // crop  → keepaspect=yes, panscan=1  (fill screen, crop edges)
    const bool keepAspect = (mode != "fill");
    const double panscan  = (mode == "crop") ? 1.0 : 0.0;

    for (const VideoProxyPointer &bwp : widgets.values()) {
        bwp->setMpvProperty("keepaspect", keepAspect);
        bwp->setMpvProperty("panscan",    panscan);
    }
}

// Pause when any app window is fullscreen or fully maximized (both axes).
bool WallpaperEnginePrivate::isDesktopCovered() const
{
    if (!xcbReady || xcb_connection_has_error(xcbConn))
        return false;

    xcb_ewmh_connection_t *ewmh = const_cast<xcb_ewmh_connection_t *>(&xcbEwmh);

    xcb_ewmh_get_windows_reply_t clients;
    bool covered = false;

    if (!xcb_ewmh_get_client_list_reply(ewmh,
            xcb_ewmh_get_client_list(ewmh, 0), &clients, nullptr))
        return false;

    for (uint32_t i = 0; i < clients.windows_len && !covered; ++i) {
        xcb_window_t win = clients.windows[i];

        // Skip desktop and dock windows
        xcb_ewmh_get_atoms_reply_t winTypes;
        bool isSystem = false;
        if (xcb_ewmh_get_wm_window_type_reply(ewmh,
                xcb_ewmh_get_wm_window_type(ewmh, win), &winTypes, nullptr)) {
            for (uint32_t t = 0; t < winTypes.atoms_len; ++t) {
                if (winTypes.atoms[t] == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP ||
                    winTypes.atoms[t] == ewmh->_NET_WM_WINDOW_TYPE_DOCK) {
                    isSystem = true;
                    break;
                }
            }
            xcb_ewmh_get_atoms_reply_wipe(&winTypes);
        }
        if (isSystem) continue;

        // Check for fullscreen or both-axis maximize
        xcb_ewmh_get_atoms_reply_t states;
        if (xcb_ewmh_get_wm_state_reply(ewmh,
                xcb_ewmh_get_wm_state(ewmh, win), &states, nullptr)) {
            bool maxH = false, maxV = false, fs = false;
            for (uint32_t s = 0; s < states.atoms_len; ++s) {
                if (states.atoms[s] == ewmh->_NET_WM_STATE_MAXIMIZED_VERT) maxV = true;
                if (states.atoms[s] == ewmh->_NET_WM_STATE_MAXIMIZED_HORZ) maxH = true;
                if (states.atoms[s] == ewmh->_NET_WM_STATE_FULLSCREEN)     fs   = true;
            }
            xcb_ewmh_get_atoms_reply_wipe(&states);
            if ((maxH && maxV) || fs)
                covered = true;
        }
    }

    xcb_ewmh_get_windows_reply_wipe(&clients);
    return covered;
}

// ── Engine ────────────────────────────────────────────────────────────────────

WallpaperEngine::WallpaperEngine(QObject *parent)
    : QObject(parent)
    , d(new WallpaperEnginePrivate(this))
{
}

WallpaperEngine::~WallpaperEngine()
{
    turnOff();
}

bool WallpaperEngine::init()
{
    WpCfg->initialize();

    // Ensure source directory exists
    QFileInfo source(d->sourcePath());
    if (!source.exists())
        source.absoluteDir().mkpath(source.fileName());

    fmInfo() << "video wallpaper source:" << source.absoluteFilePath();

    // Register right-click menu
    if (!registerMenu())
        dpfSignalDispatcher->subscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded",
                                       this, &WallpaperEngine::registerMenu);

    // Enable/disable toggle
    connect(WpCfg, &WallpaperConfig::changeEnableState, this, [this](bool e) {
        if (WpCfg->enable() == e) return;
        WpCfg->setEnable(e);
        e ? turnOn() : turnOff();
    });

    // Video path change → reload
    connect(WpCfg, &WallpaperConfig::changeVideoPath, this, [this](const QString &) {
        if (WpCfg->enable()) refreshSource();
    });

    // Scale mode change → apply immediately without reloading
    connect(WpCfg, &WallpaperConfig::changeScaleMode, this, [this](const QString &mode) {
        if (WpCfg->enable()) d->applyScaleMode(mode);
    });

    // Audio toggle → apply immediately
    connect(WpCfg, &WallpaperConfig::changeEnableAudio, this, [this](bool enabled) {
        if (!WpCfg->enable()) return;
        int volume = enabled ? 100 : 0;
        for (const VideoProxyPointer &bwp : d->widgets.values())
            bwp->setMpvProperty("volume", volume);
    });

    if (WpCfg->enable())
        turnOn();

    return true;
}

void WallpaperEngine::turnOn(bool buildNow)
{
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowAboutToBeBuilded, &WallpaperEngine::onDetachWindows);
    CanvasCoreSubscribe(signal_DesktopFrame_WindowBuilded,  &WallpaperEngine::build);
    CanvasCoreSubscribe(signal_DesktopFrame_WindowShowed,   &WallpaperEngine::play);
    CanvasCoreSubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);

    // Reset all pause/idle state
    d->pausedByWindow = false;
    d->pausedByIdle   = false;
    d->idleSeconds    = 0;
    d->lastCursorPos  = QCursor::pos();

    // Reopen XCB if it was closed by turnOff
    d->openXcb();

    // Watch for new/removed video files
    d->watcher = new QFileSystemWatcher(this);
    d->watcher->addPath(d->sourcePath());
    connect(d->watcher, &QFileSystemWatcher::directoryChanged,
            this, &WallpaperEngine::refreshSource);

    // 2-second timer for fullscreen and idle detection
    d->windowCheckTimer = new QTimer(this);
    d->windowCheckTimer->setInterval(2000);
    connect(d->windowCheckTimer, &QTimer::timeout,
            this, &WallpaperEngine::checkWindowStates);
    d->windowCheckTimer->start();

    if (buildNow) {
        build();
        refreshSource();
        show();
    }
}

void WallpaperEngine::turnOff()
{
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowAboutToBeBuilded, &WallpaperEngine::onDetachWindows);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowBuilded,  &WallpaperEngine::build);
    CanvasCoreUnsubscribe(signal_DesktopFrame_WindowShowed,   &WallpaperEngine::play);
    CanvasCoreUnsubscribe(signal_DesktopFrame_GeometryChanged, &WallpaperEngine::geometryChanged);

    if (d->windowCheckTimer) {
        d->windowCheckTimer->stop();
        delete d->windowCheckTimer;
        d->windowCheckTimer = nullptr;
    }

    delete d->watcher;
    d->watcher = nullptr;

    d->closeXcb();
    d->clearWidgets();

    d->videos.clear();
    d->pausedByWindow = false;
    d->pausedByIdle   = false;
    d->idleSeconds    = 0;

    d->setBackgroundVisible(true);
    releaseMemory();
}

// Called every 2 seconds — fullscreen detection + idle detection, fully independent.
void WallpaperEngine::checkWindowStates()
{
    if (!WpCfg->enable())
        return;

    const bool wasPaused = d->pausedByWindow || d->pausedByIdle;

    // ── Fullscreen check ──────────────────────────────────────────────────────
    if (WpCfg->pauseOnFullscreen()) {
        const bool covered = d->isDesktopCovered();
        if (covered && !d->pausedByWindow) {
            d->pausedByWindow = true;
            fmInfo() << "wallpaper paused (fullscreen/maximized)";
        } else if (!covered && d->pausedByWindow) {
            d->pausedByWindow = false;
            fmInfo() << "wallpaper resumed (fullscreen cleared)";
        }
    } else {
        d->pausedByWindow = false;
    }

    // ── Idle check ────────────────────────────────────────────────────────────
    const int idleSecs = WpCfg->pauseIdleSeconds();
    if (idleSecs > 0) {
        const QPoint cursor = QCursor::pos();
        if (cursor != d->lastCursorPos) {
            d->lastCursorPos = cursor;
            d->idleSeconds   = 0;
            if (d->pausedByIdle) {
                d->pausedByIdle = false;
                fmInfo() << "wallpaper resumed (cursor moved)";
            }
        } else {
            d->idleSeconds += 2;
            if (d->idleSeconds >= idleSecs && !d->pausedByIdle) {
                d->pausedByIdle = true;
                fmInfo() << "wallpaper paused (idle)";
            }
        }
    } else {
        if (d->pausedByIdle) {
            d->pausedByIdle = false;
            d->idleSeconds  = 0;
        }
    }

    // ── Apply pause / resume ──────────────────────────────────────────────────
    const bool nowPaused = d->pausedByWindow || d->pausedByIdle;
    if (nowPaused != wasPaused) {
        for (const VideoProxyPointer &bwp : d->widgets.values())
            bwp->setMpvProperty("pause", nowPaused);
    }
}

void WallpaperEngine::refreshSource()
{
    d->videos = d->getVideos(d->sourcePath());
    checkResource();

    if (d->videos.isEmpty()) {
        for (const VideoProxyPointer &bwp : d->widgets.values())
            bwp->command(QVariantList { "stop" });
        releaseMemory();
        return;
    }

    const QUrl sourceUrl = resolveVideoUrl(d->videos);
    releaseMemory();

    // Apply current scale mode and audio, then load video
    d->applyScaleMode(WpCfg->scaleMode());
    const int volume = WpCfg->enableAudio() ? 100 : 0;
    for (const VideoProxyPointer &bwp : d->widgets.values()) {
        bwp->setMpvProperty("pause", false);
        bwp->setMpvProperty("volume", volume);
        bwp->command(QVariantList { "loadfile", sourceUrl.toLocalFile() });
    }
}

void WallpaperEngine::build()
{
    auto cleanupStale = [this] {
        const auto winMap = rootMap();
        for (const QString &sp : d->widgets.keys()) {
            if (!winMap.contains(sp)) {
                d->widgets.take(sp);
                fmInfo() << "removed stale screen widget:" << sp;
            }
        }
    };

    const QList<QWidget *> root = ddplugin_desktop_util::desktopFrameRootWindows();
    for (QWidget *win : root) {
        if (!win) continue;
        const QString screenName = getScreenName(win);
        if (screenName.isEmpty()) {
            fmWarning() << "cannot get screen name from root window";
            continue;
        }

        VideoProxyPointer bwp = d->widgets.value(screenName);
        if (!bwp.isNull()) {
            bwp->setParent(win);
            bwp->setGeometry(d->relativeGeometry(win->geometry()));
        } else {
            fmInfo() << "screen added, creating widget:" << screenName;
            bwp = d->createWidget(win);
            d->widgets.insert(screenName, bwp);
        }
    }

    cleanupStale();
}

void WallpaperEngine::onDetachWindows()
{
    for (const VideoProxyPointer &bwp : d->widgets.values())
        bwp->setParent(nullptr);
}

void WallpaperEngine::geometryChanged()
{
    const auto winMap = rootMap();
    for (auto it = d->widgets.begin(); it != d->widgets.end(); ++it) {
        QWidget *win = winMap.value(it.key());
        if (!win) {
            fmCritical() << "cannot find root for screen:" << it.key();
            continue;
        }
        if (it.value())
            it.value()->setGeometry(d->relativeGeometry(win->geometry()));
    }
}

void WallpaperEngine::play()
{
    if (!WpCfg->enable() || d->videos.isEmpty())
        return;

    const QUrl sourceUrl = resolveVideoUrl(d->videos);
    for (const VideoProxyPointer &bwp : d->widgets.values())
        bwp->command(QVariantList { "loadfile", sourceUrl.toLocalFile() });

    d->setBackgroundVisible(false);
    show();
}

void WallpaperEngine::show()
{
    dpfSlotChannel->push("ddplugin_core", "slot_DesktopFrame_LayoutWidget");
    for (const VideoProxyPointer &bwp : d->widgets.values())
        bwp->show();
}

void WallpaperEngine::releaseMemory()
{
    QTimer::singleShot(800, this, [] { malloc_trim(0); });
}

bool WallpaperEngine::registerMenu()
{
    if (!dfmplugin_menu_util::menuSceneContains("CanvasMenu"))
        return false;

    dfmplugin_menu_util::menuSceneRegisterScene(VideoWallpaerMenuCreator::name(),
                                                new VideoWallpaerMenuCreator());
    dfmplugin_menu_util::menuSceneBind(VideoWallpaerMenuCreator::name(), "CanvasMenu");
    dpfSignalDispatcher->unsubscribe("dfmplugin_menu", "signal_MenuScene_SceneAdded",
                                     this, &WallpaperEngine::registerMenu);
    return true;
}

void WallpaperEngine::checkResource()
{
    if (d->videos.isEmpty()) {
        Dtk::Core::DUtil::DNotifySender(tr("Please add the video file to %0").arg(d->sourcePath()))
            .appName(tr("Video Wallpaper"))
            .appIcon("deepin-toggle-desktop")
            .timeOut(5000)
            .call();
    }
}
