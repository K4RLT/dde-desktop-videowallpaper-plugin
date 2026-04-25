// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERCONFIG_H
#define WALLPAPERCONFIG_H

#include "ddplugin_videowallpaper_global.h"

#include <QObject>

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class WallpaperConfigPrivate;
class WallpaperConfig : public QObject
{
    Q_OBJECT

public:
    static WallpaperConfig *instance();
    void initialize();

    // ── Enable ──────────────────────────────────────────────────────────────
    bool enable() const;
    void setEnable(bool);

    // ── Global video path ────────────────────────────────────────────────────
    QString videoPath() const;
    void setVideoPath(const QString &path);

    // ── Per-screen video paths ───────────────────────────────────────────────
    // Returns the per-screen override for @screen, falling back to videoPath().
    QString videoPathForScreen(const QString &screen) const;
    // Returns the per-screen override only; empty string if none is set.
    QString screenVideoOverride(const QString &screen) const;
    bool hasScreenOverride(const QString &screen) const;
    // Assign a specific video to one screen (overrides global for that screen).
    void setVideoPathForScreen(const QString &screen, const QString &path);
    // Clear the per-screen override; the screen falls back to the global path.
    void clearScreenOverride(const QString &screen);
    // Set the global path AND clear all per-screen overrides (apply to all).
    void setVideoPathForAll(const QString &path);

    // ── Pause behaviour ──────────────────────────────────────────────────────
    bool pauseOnFullscreen() const;
    void setPauseOnFullscreen(bool);

    int pauseIdleSeconds() const;   // 0 = disabled
    void setPauseIdleSeconds(int);

    // ── Scale mode ───────────────────────────────────────────────────────────
    QString scaleMode() const;      // "fill" | "fit" | "crop"
    void setScaleMode(const QString &mode);

    // ── Audio / volume ───────────────────────────────────────────────────────
    int volume() const;             // 0-100; 0 = muted
    void setVolume(int vol);

    // ── Playback speed ───────────────────────────────────────────────────────
    double playbackSpeed() const;   // 0.5 | 0.75 | 1.0 | 1.25 | 1.5 | 2.0
    void setPlaybackSpeed(double speed);

signals:
    void changeEnableState(bool enable);
    void changeVideoPath(const QString &path);
    void changeScreenVideoPath(const QString &screen, const QString &path);
    void changePauseOnFullscreen(bool);
    void changePauseIdleSeconds(int);
    void changeScaleMode(const QString &mode);
    void changeVolume(int vol);
    void changePlaybackSpeed(double speed);

    // Kept for ABI compat (was emitted by the old enableAudio toggle)
    void changeEnableAudio(bool);

private slots:
    void configChanged(const QString &key);

protected:
    explicit WallpaperConfig(QObject *parent = nullptr);

private:
    void persistScreenPaths();

    friend class WallpaperConfigPrivate;
    WallpaperConfigPrivate *d;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#define WpCfg WallpaperConfig::instance()

#endif // WALLPAPERCONFIG_H
