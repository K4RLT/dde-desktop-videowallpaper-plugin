// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wallpaperconfig_p.h"

#include <QApplication>

DCORE_USE_NAMESPACE
using namespace ddplugin_videowallpaper;

// Global singleton
class WallpaperConfigGlobal : public WallpaperConfig {};
Q_GLOBAL_STATIC(WallpaperConfigGlobal, wallpaperConfig)

// DConfig keys
static constexpr char kConfName[]              = "org.deepin.dde.file-manager.desktop.videowallpaper";
static constexpr char kKeyEnable[]             = "enable";
static constexpr char kKeyVideoPath[]          = "videoPath";
static constexpr char kKeyPauseOnFullscreen[]  = "pauseOnFullscreen";
static constexpr char kKeyPauseIdleSeconds[]   = "pauseIdleSeconds";
static constexpr char kKeyScaleMode[]          = "scaleMode";
static constexpr char kKeyEnableAudio[]        = "enableAudio";

// ── Private ──────────────────────────────────────────────────────────────────

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

bool WallpaperConfigPrivate::getEnableAudio() const
{
    return settings ? settings->value(kKeyEnableAudio, false).toBool() : false;
}

QString WallpaperConfigPrivate::getScaleMode() const
{
    return settings ? settings->value(kKeyScaleMode, "fill").toString() : "fill";
}

// ── Public ────────────────────────────────────────────────────────────────────

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
    d->enableAudio       = d->getEnableAudio();

    if (d->settings)
        connect(d->settings, &DConfig::valueChanged,
                this, &WallpaperConfig::configChanged, Qt::UniqueConnection);
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

QString WallpaperConfig::videoPath() const { return d->videoPath; }
void WallpaperConfig::setVideoPath(const QString &path)
{
    if (d->videoPath == path) return;
    d->videoPath = path;
    if (d->settings) d->settings->setValue(kKeyVideoPath, path);
    emit changeVideoPath(path);
}

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

bool WallpaperConfig::enableAudio() const { return d->enableAudio; }
void WallpaperConfig::setEnableAudio(bool v)
{
    if (d->enableAudio == v) return;
    d->enableAudio = v;
    if (d->settings) d->settings->setValue(kKeyEnableAudio, v);
    emit changeEnableAudio(v);
}

QString WallpaperConfig::scaleMode() const { return d->scaleMode; }
void WallpaperConfig::setScaleMode(const QString &mode)
{
    if (d->scaleMode == mode) return;
    d->scaleMode = mode;
    if (d->settings) d->settings->setValue(kKeyScaleMode, mode);
    emit changeScaleMode(mode);
}

// ── Config change handler ─────────────────────────────────────────────────────

void WallpaperConfig::configChanged(const QString &key)
{
    if (key == kKeyEnable) {
        bool e = d->getEnable();
        if (e != d->enable) emit changeEnableState(e);
    } else if (key == kKeyVideoPath) {
        QString p = d->getVideoPath();
        if (p != d->videoPath) emit changeVideoPath(p);
    } else if (key == kKeyPauseOnFullscreen) {
        bool v = d->getPauseOnFullscreen();
        if (v != d->pauseOnFullscreen) emit changePauseOnFullscreen(v);
    } else if (key == kKeyPauseIdleSeconds) {
        int s = d->getPauseIdleSeconds();
        if (s != d->pauseIdleSeconds) emit changePauseIdleSeconds(s);
    } else if (key == kKeyEnableAudio) {
        bool a = d->getEnableAudio();
        if (a != d->enableAudio) emit changeEnableAudio(a);
    } else if (key == kKeyScaleMode) {
        QString m = d->getScaleMode();
        if (m != d->scaleMode) emit changeScaleMode(m);
    }
}
