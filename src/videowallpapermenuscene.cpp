// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videowallpapermenuscene.h"
#include "wallpaperconfig.h"
#include "ddplugin_videowallpaper_global.h"
#include "dfm-base/dfm_menu_defines.h"

#include <DDialog>

#include <QActionGroup>
#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QVariantHash>

using namespace ddplugin_videowallpaper;
DFMBASE_USE_NAMESPACE

// ── Creator ───────────────────────────────────────────────────────────────────

AbstractMenuScene *VideoWallpaerMenuCreator::create()
{
    return new VideoWallpaperMenuScene();
}

// ── Scene ─────────────────────────────────────────────────────────────────────

VideoWallpaperMenuScene::VideoWallpaperMenuScene(QObject *parent)
    : AbstractMenuScene(parent)
{
    predicateName[ActionID::kVideoWallpaper] = tr("Video wallpaper");
}

QString VideoWallpaperMenuScene::name() const
{
    return VideoWallpaerMenuCreator::name();
}

bool VideoWallpaperMenuScene::initialize(const QVariantHash &params)
{
    turnOn      = WpCfg->enable();
    isEmptyArea = params.value(MenuParamKey::kIsEmptyArea).toBool();
    onDesktop   = params.value(MenuParamKey::kOnDesktop).toBool();
    return isEmptyArea && onDesktop;
}

AbstractMenuScene *VideoWallpaperMenuScene::scene(QAction *action) const
{
    if (!action)
        return nullptr;
    if (!predicateAction.key(action).isEmpty())
        return const_cast<VideoWallpaperMenuScene *>(this);
    return AbstractMenuScene::scene(action);
}

bool VideoWallpaperMenuScene::create(QMenu *parent)
{
    const QString folderPath =
        QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first()
        + "/video-wallpaper";

    const QFileInfoList files = QDir(folderPath).entryInfoList(
        { "*.mp4", "*.mkv", "*.webm", "*.avi", "*.mov" }, QDir::Files, QDir::Name);

    // No videos — show a simple toggle action
    if (files.isEmpty()) {
        QAction *action = parent->addAction(predicateName.value(ActionID::kVideoWallpaper));
        predicateAction[ActionID::kVideoWallpaper] = action;
        action->setProperty(ActionPropertyKey::kActionID, QString(ActionID::kVideoWallpaper));
        action->setCheckable(true);
        action->setChecked(turnOn);
        return true;
    }

    videoSubMenu = new QMenu(predicateName.value(ActionID::kVideoWallpaper), parent);

    // ── Disable ───────────────────────────────────────────────────────────────
    QAction *disableAction = videoSubMenu->addAction(tr("Disable"));
    disableAction->setCheckable(true);
    disableAction->setChecked(!turnOn);
    connect(disableAction, &QAction::triggered, this, [=]() {
        emit WpCfg->changeEnableState(false);
    });

    videoSubMenu->addSeparator();

    // ── Pause when fullscreen ─────────────────────────────────────────────────
    QAction *pauseFsAction = videoSubMenu->addAction(tr("Pause when fullscreen"));
    pauseFsAction->setCheckable(true);
    pauseFsAction->setChecked(WpCfg->pauseOnFullscreen());
    pauseFsAction->setEnabled(turnOn);
    connect(pauseFsAction, &QAction::triggered, this, [=](bool checked) {
        WpCfg->setPauseOnFullscreen(checked);
    });

    // ── Pause on idle ─────────────────────────────────────────────────────────
    QMenu *idleMenu = new QMenu(tr("Pause on idle"), videoSubMenu);
    idleMenu->setEnabled(turnOn);

    struct IdleOption { QString label; int seconds; };
    const QList<IdleOption> idleOptions = {
        { tr("Disable"),     0    },
        { tr("30 seconds"),  30   },
        { tr("1 minute"),    60   },
        { tr("2 minutes"),   120  },
        { tr("5 minutes"),   300  },
        { tr("10 minutes"),  600  },
        { tr("15 minutes"),  900  },
        { tr("30 minutes"),  1800 },
    };

    const int currentIdle = WpCfg->pauseIdleSeconds();
    auto *idleGroup = new QActionGroup(idleMenu);
    idleGroup->setExclusive(true);
    for (const IdleOption &opt : idleOptions) {
        QAction *a = idleMenu->addAction(opt.label);
        a->setCheckable(true);
        a->setChecked(currentIdle == opt.seconds);
        idleGroup->addAction(a);
        const int secs = opt.seconds;
        connect(a, &QAction::triggered, this, [=]() {
            WpCfg->setPauseIdleSeconds(secs);
        });
    }
    videoSubMenu->addMenu(idleMenu);

    videoSubMenu->addSeparator();

    // ── Scale mode ────────────────────────────────────────────────────────────
    QMenu *scaleMenu = new QMenu(tr("Scale mode"), videoSubMenu);
    scaleMenu->setEnabled(turnOn);

    struct ScaleOption { QString label; QString mode; };
    const QList<ScaleOption> scaleOptions = {
        { tr("Fill"), "fill" },
        { tr("Fit"),  "fit"  },
        { tr("Crop"), "crop" },
    };

    const QString currentScale = WpCfg->scaleMode();
    auto *scaleGroup = new QActionGroup(scaleMenu);
    scaleGroup->setExclusive(true);
    for (const ScaleOption &opt : scaleOptions) {
        QAction *a = scaleMenu->addAction(opt.label);
        a->setCheckable(true);
        a->setChecked(currentScale == opt.mode);
        scaleGroup->addAction(a);
        const QString mode = opt.mode;
        connect(a, &QAction::triggered, this, [=]() {
            WpCfg->setScaleMode(mode);
        });
    }
    videoSubMenu->addMenu(scaleMenu);

    // ── Playback speed ────────────────────────────────────────────────────────
    QMenu *speedMenu = new QMenu(tr("Playback speed"), videoSubMenu);
    speedMenu->setEnabled(turnOn);

    struct SpeedOption { QString label; double speed; };
    const QList<SpeedOption> speedOptions = {
        { tr("0.25×"), 0.25 },
        { tr("0.5×"),  0.5  },
        { tr("0.75×"), 0.75 },
        { tr("1×"),    1.0  },
        { tr("1.25×"), 1.25 },
        { tr("1.5×"),  1.5  },
        { tr("2×"),    2.0  },
    };

    const double currentSpeed = WpCfg->playbackSpeed();
    auto *speedGroup = new QActionGroup(speedMenu);
    speedGroup->setExclusive(true);
    for (const SpeedOption &opt : speedOptions) {
        QAction *a = speedMenu->addAction(opt.label);
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(currentSpeed, opt.speed));
        speedGroup->addAction(a);
        const double spd = opt.speed;
        connect(a, &QAction::triggered, this, [=]() {
            WpCfg->setPlaybackSpeed(spd);
        });
    }
    videoSubMenu->addMenu(speedMenu);

    // ── Volume ────────────────────────────────────────────────────────────────
    QMenu *volumeMenu = new QMenu(tr("Volume"), videoSubMenu);
    volumeMenu->setEnabled(turnOn);

    struct VolumeOption { QString label; int level; };
    const QList<VolumeOption> volumeOptions = {
        { tr("Mute"),  0   },
        { tr("25%"),   25  },
        { tr("50%"),   50  },
        { tr("75%"),   75  },
        { tr("100%"),  100 },
    };

    const int currentVolume = WpCfg->volume();
    auto *volumeGroup = new QActionGroup(volumeMenu);
    volumeGroup->setExclusive(true);
    for (const VolumeOption &opt : volumeOptions) {
        QAction *a = volumeMenu->addAction(opt.label);
        a->setCheckable(true);
        a->setChecked(currentVolume == opt.level);
        volumeGroup->addAction(a);
        const int level = opt.level;
        connect(a, &QAction::triggered, this, [=]() {
            WpCfg->setVolume(level);
        });
    }
    videoSubMenu->addMenu(volumeMenu);

    videoSubMenu->addSeparator();

    // ── Global video list ─────────────────────────────────────────────────────
    // Identify the screen under the cursor for per-screen assignment
    const QList<QScreen *> screens      = QApplication::screens();
    const bool             multiMonitor = screens.count() > 1;
    QScreen               *cursorScreen = QApplication::screenAt(QCursor::pos());
    const QString cursorScreenName      = cursorScreen ? cursorScreen->name() : QString();

    const QString currentPath = WpCfg->videoPath();

    for (const QFileInfo &file : files) {
        const QString filePath = file.absoluteFilePath();
        QAction *a = videoSubMenu->addAction(file.fileName());
        a->setCheckable(true);
        // Checked when this is the active global video and no per-screen override exists
        a->setChecked(turnOn && filePath == currentPath
                      && !WpCfg->hasScreenOverride(cursorScreenName));

        connect(a, &QAction::triggered, this, [=]() {
            // Async resolution check via ffprobe, then apply video
            auto *ffprobe = new QProcess();
            ffprobe->start("ffprobe", {
                "-v", "error",
                "-select_streams", "v:0",
                "-show_entries", "stream=width,height",
                "-of", "csv=s=x:p=0",
                filePath
            });

            connect(ffprobe, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    ffprobe, [=](int, QProcess::ExitStatus) {
                // Read stdout BEFORE scheduling deletion
                const QByteArray output = ffprobe->readAllStandardOutput().trimmed();
                ffprobe->deleteLater();

                const QStringList parts = QString(output).split('x');
                if (parts.size() == 2) {
                    const int w = parts[0].toInt();
                    const int h = parts[1].toInt();
                    QString resLabel;
                    if      (w >= 7680 || h >= 4320) resLabel = "8K";
                    else if (w >= 3840 || h >= 2160) resLabel = "4K";
                    else if (w >= 2560 || h >= 1440) resLabel = "2K";

                    if (!resLabel.isEmpty()) {
                        DTK_WIDGET_NAMESPACE::DDialog warn;
                        warn.setWindowTitle(tr("High Resolution Warning"));
                        warn.setMessage(tr("This video is %1 (%2x%3).\n\n"
                                           "High resolution videos may cause high CPU/GPU usage "
                                           "and affect system performance.\n\n"
                                           "Do you want to continue?")
                                        .arg(resLabel).arg(w).arg(h));
                        warn.setIcon(QIcon::fromTheme("dialog-warning"));
                        warn.addButton(tr("Cancel"),   false, DTK_WIDGET_NAMESPACE::DDialog::ButtonWarning);
                        warn.addButton(tr("Continue"), true,  DTK_WIDGET_NAMESPACE::DDialog::ButtonRecommend);
                        if (warn.exec() != 1)
                            return;
                    }
                }

                // Apply to all screens (clears any per-screen overrides)
                WpCfg->setVideoPathForAll(filePath);
                emit WpCfg->changeEnableState(true);
            });
        });
    }

    // ── Per-screen video (multi-monitor only) ─────────────────────────────────
    if (multiMonitor) {
        videoSubMenu->addSeparator();
        QMenu *perScreenMenu = new QMenu(tr("Per-screen video"), videoSubMenu);
        perScreenMenu->setEnabled(turnOn);

        for (QScreen *screen : screens) {
            const QString screenName = screen->name();
            QMenu *screenMenu = new QMenu(
                QString("%1  (%2×%3)")
                    .arg(screenName)
                    .arg(screen->size().width())
                    .arg(screen->size().height()),
                perScreenMenu);

            // "Use global" resets this screen to the global video
            QAction *globalAct = screenMenu->addAction(tr("Same as global"));
            globalAct->setCheckable(true);
            globalAct->setChecked(!WpCfg->hasScreenOverride(screenName));
            connect(globalAct, &QAction::triggered, this, [=]() {
                WpCfg->clearScreenOverride(screenName);
            });

            screenMenu->addSeparator();

            const QString screenOverride = WpCfg->screenVideoOverride(screenName);
            for (const QFileInfo &file : files) {
                const QString fp = file.absoluteFilePath();
                QAction *a = screenMenu->addAction(file.fileName());
                a->setCheckable(true);
                a->setChecked(fp == screenOverride);
                connect(a, &QAction::triggered, this, [=]() {
                    WpCfg->setVideoPathForScreen(screenName, fp);
                    emit WpCfg->changeEnableState(true);
                });
            }

            perScreenMenu->addMenu(screenMenu);
        }

        videoSubMenu->addMenu(perScreenMenu);
    }

    // Hidden placeholder action used for menu positioning
    QAction *placeholder = parent->addAction(predicateName.value(ActionID::kVideoWallpaper));
    predicateAction[ActionID::kVideoWallpaper] = placeholder;
    placeholder->setProperty(ActionPropertyKey::kActionID, QString(ActionID::kVideoWallpaper));
    placeholder->setVisible(false);

    return true;
}

void VideoWallpaperMenuScene::updateState(QMenu *parent)
{
    const auto actions = parent->actions();
    auto it = std::find_if(actions.begin(), actions.end(), [](const QAction *ac) {
        return ac->property(ActionPropertyKey::kActionID).toString() == "wallpaper-settings";
    });

    if (it == actions.end()) {
        fmWarning() << "cannot find wallpaper-settings action";
        return;
    }

    if (videoSubMenu) {
        parent->insertMenu(*it, videoSubMenu);
        parent->removeAction(predicateAction[ActionID::kVideoWallpaper]);
    } else {
        parent->insertAction(*it, predicateAction[ActionID::kVideoWallpaper]);
    }

    AbstractMenuScene::updateState(parent);
}

bool VideoWallpaperMenuScene::triggered(QAction *action)
{
    if (!predicateAction.key(action).isEmpty()) {
        if (action->property(ActionPropertyKey::kActionID).toString() == ActionID::kVideoWallpaper)
            emit WpCfg->changeEnableState(action->isChecked());
        return true;
    }
    return AbstractMenuScene::triggered(action);
}
