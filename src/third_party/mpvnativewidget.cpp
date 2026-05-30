// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvnativewidget.h"
#include "common/qthelper.hpp"

#include <QApplication>
#include <QResizeEvent>
#include <stdexcept>

MpvNativeWidget::MpvNativeWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    // Required so mpv can embed into this widget via X11 window ID
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Ensure the native window handle exists before we read winId()
    winId();

    mpv = mpv_create();
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    // Embed mpv into this X11 window BEFORE mpv_initialize — wid is an init-time option
    int64_t wid = static_cast<int64_t>(this->winId());
    mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);

    // Use Vulkan GPU rendering — NVIDIA handles this much better than OpenGL context sharing.
    // hwdec=auto-safe lets mpv pick nvdec/nvdec-copy/vaapi-vk depending on what's available,
    // which is safer than hardcoding nvdec (requires CUDA, breaks Nouveau).
    mpv::qt::set_option_variant(mpv, "vo",      "gpu");
    mpv::qt::set_option_variant(mpv, "gpu-api", "vulkan");
    mpv::qt::set_option_variant(mpv, "hwdec",   "auto-safe");

    mpv::qt::set_option_variant(mpv, "volume", 0);
    mpv::qt::set_option_variant(mpv, "loop",   "inf");
    mpv::qt::set_option_variant(mpv, "keepaspect",     "no");
    mpv::qt::set_option_variant(mpv, "video-unscaled", "no");

    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_set_wakeup_callback(mpv, MpvNativeWidget::wakeup, this);

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");

    m_embedded = true;
}

MpvNativeWidget::~MpvNativeWidget()
{
    shutdown();
    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
}

void MpvNativeWidget::shutdown()
{
    if (mpv)
        mpv_command_string(mpv, "stop");
}

void MpvNativeWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // mpv handles resize automatically when embedded via wid
}

void MpvNativeWidget::command(const QVariant &params)
{
    if (mpv)
        mpv::qt::command_variant(mpv, params);
}

void MpvNativeWidget::setProperty(const QString &name, const QVariant &value)
{
    if (mpv)
        mpv::qt::set_property_variant(mpv, name, value);
}

QVariant MpvNativeWidget::getProperty(const QString &name) const
{
    if (!mpv)
        return QVariant();
    return mpv::qt::get_property_variant(mpv, name);
}

void MpvNativeWidget::on_mpv_events()
{
    while (mpv) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        handle_mpv_event(event);
    }
}

void MpvNativeWidget::handle_mpv_event(mpv_event *event)
{
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = reinterpret_cast<mpv_event_property *>(event->data);
        if (strcmp(prop->name, "time-pos") == 0 && prop->format == MPV_FORMAT_DOUBLE)
            emit positionChanged(*reinterpret_cast<double *>(prop->data));
        else if (strcmp(prop->name, "duration") == 0 && prop->format == MPV_FORMAT_DOUBLE)
            emit durationChanged(*reinterpret_cast<double *>(prop->data));
        break;
    }
    default:
        break;
    }
}

void MpvNativeWidget::wakeup(void *ctx)
{
    QMetaObject::invokeMethod(reinterpret_cast<MpvNativeWidget *>(ctx),
                              &MpvNativeWidget::on_mpv_events,
                              Qt::QueuedConnection);
}
