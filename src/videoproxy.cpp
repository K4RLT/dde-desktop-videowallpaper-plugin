// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videoproxy.h"

#ifdef USE_LIBMPV
#include "third_party/mpvwidget.h"
#include "third_party/mpvnativewidget.h"
#include <QVBoxLayout>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#else
#include <QPainter>
#endif

using namespace ddplugin_videowallpaper;

#ifdef USE_LIBMPV

static bool isNvidiaGpu()
{
    // Run once and cache — creating a temporary GL context per VideoProxy (once per screen) is wasteful
    static int cached = -1;
    if (cached != -1)
        return cached == 1;

    QOffscreenSurface surface;
    surface.create();
    QOpenGLContext ctx;
    if (ctx.create() && ctx.makeCurrent(&surface)) {
        const QString vendor = QString::fromUtf8(
            reinterpret_cast<const char *>(ctx.functions()->glGetString(GL_VENDOR))).toLower();
        ctx.doneCurrent();
        cached = vendor.contains("nvidia") ? 1 : 0;
    } else {
        cached = 0;
    }
    return cached == 1;
}

VideoProxy::VideoProxy(QWidget *parent)
    : QWidget(parent)
{
    if (isNvidiaGpu()) {
        qInfo() << "[VideoWallpaper] NVIDIA GPU detected, using native Vulkan renderer";
        nativeWidget = new MpvNativeWidget(this, Qt::FramelessWindowHint);
    } else {
        widget = new MpvWidget(this, Qt::FramelessWindowHint);
    }
    initUI();
}

void VideoProxy::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    QWidget *w = nativeWidget ? static_cast<QWidget*>(nativeWidget) : static_cast<QWidget*>(widget);
    layout->addWidget(w);
    setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoProxy::command(const QVariant &params)
{
    if (nativeWidget)
        nativeWidget->command(params);
    else if (widget)
        widget->command(params);
}

void VideoProxy::setMpvProperty(const QString &name, const QVariant &value)
{
    if (nativeWidget)
        nativeWidget->setProperty(name, value);
    else if (widget)
        widget->setProperty(name, value);
}

QVariant VideoProxy::getMpvProperty(const QString &name) const
{
    if (nativeWidget)
        return nativeWidget->getProperty(name);
    if (widget)
        return widget->getProperty(name);
    return QVariant();
}

void VideoProxy::shutdownMpv()
{
    if (nativeWidget)
        nativeWidget->shutdown();
    else if (widget)
        widget->shutdown();
}

#else

VideoProxy::VideoProxy(QWidget *parent)
    : QWidget(parent)
{
    auto pal = palette();
    pal.setColor(backgroundRole(), Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);
}

VideoProxy::~VideoProxy() = default;

void VideoProxy::updateImage(const QImage &img)
{
    image = img.scaled(img.size().boundedTo(QSize(1920, 1280)) * devicePixelRatioF(),
                       Qt::KeepAspectRatio, Qt::FastTransformation);
    image.setDevicePixelRatio(devicePixelRatioF());
    update();
}

void VideoProxy::clear()
{
    image = QImage();
    update();
}

void VideoProxy::paintEvent(QPaintEvent *e)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    if (!image.isNull()) {
        QSize tar = image.size() / devicePixelRatioF();
        int x = (rect().width() - tar.width()) / 2;
        int y = (rect().height() - tar.height()) / 2;
        painter.drawImage(x, y, image);
    }

    QWidget::paintEvent(e);
}

#endif
