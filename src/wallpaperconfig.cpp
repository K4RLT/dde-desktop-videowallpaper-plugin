// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wallpaperconfig_p.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>

DCORE_USE_NAMESPACE
using namespace ddplugin_videowallpaper;

// Global singleton
class WallpaperConfigGlobal : public WallpaperConfig {};
Q_GLOBAL_STATIC(WallpaperConfigGlobal, wallpaperConfig)

// DConfig keys
static constexpr char kConfName[]             = "org.deepin.dde.file-manager.desktop.videowallpaper";
static constexpr char kKeyEnable[]            = "enable";
static constexpr char kKeyVideoPath[]         = "videoPath";
static constexpr char kKeyPauseOnFullscreen[] = "pauseOnFullscreen";
static constexpr char kKeyPauseIdleSeconds[]  = "pauseIdleSeconds";
static constexpr char kKeyScaleMode[]         = "scaleMode";
static constexpr char kKeyEnableAudio[]       = "enableAudio";    // legacy — migration only
static constexpr char kKeyVolume[]            = "volume";
static constexpr char kKeyPlaybackSpeed[]     = "playbackSpeed";
static constexpr char kKeyScreenVideoPaths[]  = "screenVideoPaths";

// ── Private helpers ───────────────────────────────────────────────────────────

WallpaperConfigPrivate::WallpaperConfigPrivate(WallpaperConfig *qq)
    : q(qq)
{
}

bool WallpaperConfigPrivate::getEnable() const
{
    return settings ? settings->value(kKeyEnable, false).toBool() : false;
}

QString WallpaperConfigPrivate::getVideoPath() const
{
    return settings ? settings->value(kKeyVideoPath, QString()).toString() : QString();
}

bool WallpaperConfigPrivate::getPauseOnFullscreen() const
{
    return settings ? settings->value(kKeyPauseOnFullscreen, false).toBool() : false;
}

int WallpaperConfigPrivate::getPauseIdleSeconds() const
{
    return settings ? settings->value(kKeyPauseIdleSeconds, 0).toInt() : 0;
}

QString WallpaperConfigPrivate::getScaleMode() const
{
    return settings ? settings->value(kKeyScaleMode, "fill").toString() : "fill";
}

bool WallpaperConfigPrivate::getEnableAudio() const
{
    return settings ? settings->value(kKeyEnableAudio, false).toBool() : false;
}

int WallpaperConfigPrivate::getVolume() const
{
    return settings ? qBound(0, settings->value(kKeyVolume, 0).toInt(), 100) : 0;
}

double WallpaperConfigPrivate::getPlaybackSpeed() const
{
    if (!settings) return 1.0;
    const double v = settings->value(kKeyPlaybackSpeed, 1.0).toDouble();
    // Clamp to the supported preset range
    return qBound(0.25, v, 4.0);
}

QMap<QString, QString> WallpaperConfigPrivate::getScreenVideoPaths() const
{
    if (!settings) return {};
    return deserialiseScreenPaths(settings->value(kKeyScreenVideoPaths, "{}").toString());
}

QString WallpaperConfigPrivate::serialiseScreenPaths(const QMap<QString, QString> &map)
{
    QJsonObject obj;
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        obj[it.key()] = it.value();
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QMap<QString, QString> WallpaperConfigPrivate::deserialiseScreenPaths(const QString &json)
{
    QMap<QString, QString> map;
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        map[it.key()] = it.value().toString();
    return map;
}

// ── Public singleton ──────────────────────────────────────────────────────────

WallpaperConfig *WallpaperConfig::instance()
{
    return wallpaperConfig;
}

WallpaperConfig::WallpaperConfig(QObject *parent)
    : QObject(parent)
    , d(new WallpaperConfigPrivate(this))
{
    Q_ASSERT(qApp->thread() == thread());
    d->settings = DConfig::create("org.deepin.dde.file-manager", kConfName, "", this);
    if (!d->settings)
        qCritical() << "cannot create DConfig for" << kConfName;
}

void WallpaperConfig::initialize()
{
    d->enable            = d->getEnable();
    d->videoPath         = d->getVideoPath();
    d->pauseOnFullscreen = d->getPauseOnFullscreen();
    d->pauseIdleSeconds  = d->getPauseIdleSeconds();
    d->scaleMode         = d->getScaleMode();
    d->screenVideoPaths  = d->getScreenVideoPaths();
    d->playbackSpeed     = d->getPlaybackSpeed();

    // Volume: migrate from legacy enableAudio if volume key still reads 0
    const int storedVolume = d->getVolume();
    if (storedVolume == 0 && d->getEnableAudio()) {
        d->volume = 50;
        if (d->settings) d->settings->setValue(kKeyVolume, 50); // persist so we don't re-migrate
    } else {
        d->volume = storedVolume;
    }

    if (d->settings)
        connect(d->settings, &DConfig::valueChanged,
                this, &WallpaperConfig::configChanged, Qt::UniqueConnection);
}

// ── Private helper ────────────────────────────────────────────────────────────

void WallpaperConfig::persistScreenPaths()
{
    if (d->settings)
        d->settings->setValue(kKeyScreenVideoPaths,
                              WallpaperConfigPrivate::serialiseScreenPaths(d->screenVideoPaths));
}

// ── Getters / Setters ─────────────────────────────────────────────────────────

bool WallpaperConfig::enable() const { return d->enable; }
void WallpaperConfig::setEnable(bool e)
{
    if (d->enable == e) return;
    d->enable = e;
    if (d->settings && d->getEnable() != e)
        d->settings->setValue(kKeyEnable, e);
}

// ── Global video path ─────────────────────────────────────────────────────────

QString WallpaperConfig::videoPath() const { return d->videoPath; }
void WallpaperConfig::setVideoPath(const QString &path)
{
    if (d->videoPath == path) return;
    d->videoPath = path;
    if (d->settings) d->settings->setValue(kKeyVideoPath, path);
    emit changeVideoPath(path);
}

// ── Per-screen video paths ────────────────────────────────────────────────────

QString WallpaperConfig::videoPathForScreen(const QString &screen) const
{
    const QString override = d->screenVideoPaths.value(screen);
    return override.isEmpty() ? d->videoPath : override;
}

QString WallpaperConfig::screenVideoOverride(const QString &screen) const
{
    return d->screenVideoPaths.value(screen);
}

bool WallpaperConfig::hasScreenOverride(const QString &screen) const
{
    return d->screenVideoPaths.contains(screen);
}

void WallpaperConfig::setVideoPathForScreen(const QString &screen, const QString &path)
{
    if (screen.isEmpty() || path.isEmpty()) return;
    if (d->screenVideoPaths.value(screen) == path) return;
    d->screenVideoPaths.insert(screen, path);
    persistScreenPaths();
    emit changeScreenVideoPath(screen, path);
}

void WallpaperConfig::clearScreenOverride(const QString &screen)
{
    if (!d->screenVideoPaths.contains(screen)) return;
    d->screenVideoPaths.remove(screen);
    persistScreenPaths();
    // Signal with empty path so the engine falls back to global
    emit changeScreenVideoPath(screen, QString());
}

void WallpaperConfig::setVideoPathForAll(const QString &path)
{
    // Wipe all per-screen overrides first
    if (!d->screenVideoPaths.isEmpty()) {
        d->screenVideoPaths.clear();
        persistScreenPaths();
    }
    // Then update global (emits changeVideoPath → refreshSource)
    setVideoPath(path);
}

// ── Pause behaviour ───────────────────────────────────────────────────────────

bool WallpaperConfig::pauseOnFullscreen() const { return d->pauseOnFullscreen; }
void WallpaperConfig::setPauseOnFullscreen(bool v)
{
    if (d->pauseOnFullscreen == v) return;
    d->pauseOnFullscreen = v;
    if (d->settings) d->settings->setValue(kKeyPauseOnFullscreen, v);
    emit changePauseOnFullscreen(v);
}

int WallpaperConfig::pauseIdleSeconds() const { return d->pauseIdleSeconds; }
void WallpaperConfig::setPauseIdleSeconds(int secs)
{
    if (d->pauseIdleSeconds == secs) return;
    d->pauseIdleSeconds = secs;
    if (d->settings) d->settings->setValue(kKeyPauseIdleSeconds, secs);
    emit changePauseIdleSeconds(secs);
}

// ── Scale mode ────────────────────────────────────────────────────────────────

QString WallpaperConfig::scaleMode() const { return d->scaleMode; }
void WallpaperConfig::setScaleMode(const QString &mode)
{
    if (d->scaleMode == mode) return;
    d->scaleMode = mode;
    if (d->settings) d->settings->setValue(kKeyScaleMode, mode);
    emit changeScaleMode(mode);
}

// ── Audio / volume ────────────────────────────────────────────────────────────

int WallpaperConfig::volume() const { return d->volume; }
void WallpaperConfig::setVolume(int vol)
{
    vol = qBound(0, vol, 100);
    if (d->volume == vol) return;
    d->volume = vol;
    if (d->settings) d->settings->setValue(kKeyVolume, vol);
    emit changeVolume(vol);
    // Keep legacy signal in sync so any old consumers don't break
    emit changeEnableAudio(vol > 0);
}

// ── Playback speed ────────────────────────────────────────────────────────────

double WallpaperConfig::playbackSpeed() const { return d->playbackSpeed; }
void WallpaperConfig::setPlaybackSpeed(double speed)
{
    speed = qBound(0.25, speed, 4.0);
    if (qFuzzyCompare(d->playbackSpeed, speed)) return;
    d->playbackSpeed = speed;
    if (d->settings) d->settings->setValue(kKeyPlaybackSpeed, speed);
    emit changePlaybackSpeed(speed);
}

// ── DConfig change handler ────────────────────────────────────────────────────

void WallpaperConfig::configChanged(const QString &key)
{
    if (key == kKeyEnable) {
        const bool e = d->getEnable();
        if (e != d->enable) { d->enable = e; emit changeEnableState(e); }

    } else if (key == kKeyVideoPath) {
        const QString p = d->getVideoPath();
        if (p != d->videoPath) { d->videoPath = p; emit changeVideoPath(p); }

    } else if (key == kKeyPauseOnFullscreen) {
        const bool v = d->getPauseOnFullscreen();
        if (v != d->pauseOnFullscreen) { d->pauseOnFullscreen = v; emit changePauseOnFullscreen(v); }

    } else if (key == kKeyPauseIdleSeconds) {
        const int s = d->getPauseIdleSeconds();
        if (s != d->pauseIdleSeconds) { d->pauseIdleSeconds = s; emit changePauseIdleSeconds(s); }

    } else if (key == kKeyScaleMode) {
        const QString m = d->getScaleMode();
        if (m != d->scaleMode) { d->scaleMode = m; emit changeScaleMode(m); }

    } else if (key == kKeyVolume) {
        const int v = d->getVolume();
        if (v != d->volume) { d->volume = v; emit changeVolume(v); emit changeEnableAudio(v > 0); }

    } else if (key == kKeyPlaybackSpeed) {
        const double sp = d->getPlaybackSpeed();
        if (!qFuzzyCompare(sp, d->playbackSpeed)) { d->playbackSpeed = sp; emit changePlaybackSpeed(sp); }

    } else if (key == kKeyScreenVideoPaths) {
        const QMap<QString, QString> newMap = d->getScreenVideoPaths();
        if (newMap != d->screenVideoPaths) {
            // Emit changeScreenVideoPath for each changed screen
            for (auto it = newMap.cbegin(); it != newMap.cend(); ++it) {
                if (d->screenVideoPaths.value(it.key()) != it.value())
                    emit changeScreenVideoPath(it.key(), it.value());
            }
            // Emit empty-path signal for screens that lost their override
            for (const QString &removedScreen : d->screenVideoPaths.keys()) {
                if (!newMap.contains(removedScreen))
                    emit changeScreenVideoPath(removedScreen, QString());
            }
            d->screenVideoPaths = newMap;
        }
    }
}
