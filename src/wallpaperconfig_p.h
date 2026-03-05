// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERCONFIG_P_H
#define WALLPAPERCONFIG_P_H

#include "ddplugin_videowallpaper_global.h"
#include "wallpaperconfig.h"

#include <DConfig>

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class WallpaperConfigPrivate
{
public:
    explicit WallpaperConfigPrivate(WallpaperConfig *qq);

    // Read raw values from DConfig
    bool getEnable() const;
    QString getVideoPath() const;
    bool getPauseOnFullscreen() const;
    int getPauseIdleSeconds() const;
    QString getScaleMode() const;
    bool getEnableAudio() const;

private:
    // Cached values
    bool enable = false;
    QString videoPath;
    bool pauseOnFullscreen = false;
    int pauseIdleSeconds = 0;       // 0 = disabled
    QString scaleMode = "fill";     // "fill" | "fit" | "crop"
    bool enableAudio  = false;

    DTK_CORE_NAMESPACE::DConfig *settings = nullptr;

    friend class WallpaperConfig;
    WallpaperConfig *q;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // WALLPAPERCONFIG_P_H
