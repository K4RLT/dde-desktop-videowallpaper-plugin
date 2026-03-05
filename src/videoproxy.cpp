// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videoproxy.h"

#ifdef USE_LIBMPV
#include "third_party/mpvwidget.h"
#include <QResizeEvent>
#include <QVBoxLayout>
#else
#include <QPainter>
#endif

using namespace ddplugin_videowallpaper;

#ifdef USE_LIBMPV

VideoProxy::VideoProxy(QWidget *parent)
    : QWidget(parent)
    , widget(new MpvWidget(this, Qt::FramelessWindowHint))
{
    initUI();
}

void VideoProxy::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget);
    setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void VideoProxy::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (widget)
        widget->setGeometry(0, 0, e->size().width(), e->size().height());
}

void VideoProxy::command(const QVariant &params)
{
    if (widget)
        widget->command(params);
}

void VideoProxy::setMpvProperty(const QString &name, const QVariant &value)
{
    if (widget)
        widget->setProperty(name, value);
}

void VideoProxy::shutdownMpv()
{
    if (widget)
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
