// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WALLPAPERENGINE_P_H
#define WALLPAPERENGINE_P_H

#include "ddplugin_videowallpaper_global.h"
#include "videoproxy.h"

#include <QFileSystemWatcher>
#include <QMap>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QUrl>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class WallpaperEngine;

class WallpaperEnginePrivate
{
public:
    explicit WallpaperEnginePrivate(WallpaperEngine *qq);

    // Geometry helper
    inline QRect relativeGeometry(const QRect &geometry) const
    {
        return QRect(QPoint(0, 0), geometry.size());
    }

    // Video source
    static QList<QUrl> getVideos(const QString &path);
    QString sourcePath() const;

    // Widget management
    VideoProxyPointer createWidget(QWidget *root);
    void clearWidgets();

    // Desktop coverage detection via XCB
    bool isDesktopCovered() const;
    void openXcb();
    void closeXcb();

    // Background layer
    void setBackgroundVisible(bool v);

    // Apply current scale mode to all widgets
    void applyScaleMode(const QString &mode);

private:
    QMap<QString, VideoProxyPointer> widgets;

    // File watcher + check timer
    QFileSystemWatcher *watcher       = nullptr;
    QTimer             *windowCheckTimer = nullptr;

    // Pause state
    bool pausedByWindow = false;
    bool pausedByIdle   = false;
    int  idleSeconds    = 0;
    QPoint lastCursorPos;

    QList<QUrl> videos;

    // Persistent XCB connection (opened once, reused every timer tick)
    xcb_connection_t      *xcbConn  = nullptr;
    xcb_ewmh_connection_t  xcbEwmh  = {};
    bool                   xcbReady = false;

    friend class WallpaperEngine;
    WallpaperEngine *q;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // WALLPAPERENGINE_P_H
