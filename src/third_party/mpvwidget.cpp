// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "third_party/mpvwidget.h"
#include "third_party/common/qthelper.hpp"

#include <QOpenGLContext>
#include <stdexcept>

MpvWidget::MpvWidget(QWidget *parent, Qt::WindowFlags f)
    : QOpenGLWidget(parent, f)
{
    mpv = mpv_create();
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    mpv_set_option_string(mpv, "vo", "libmpv");

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");

    mpv::qt::set_option_variant(mpv, "hwdec", "auto");

#ifndef ENABLE_AUDIO_OUTPUT
    mpv::qt::set_option_variant(mpv, "volume", 0);
#endif

    mpv::qt::set_option_variant(mpv, "loop", "inf");

    // Default: fill the widget with no black bars.
    // keepaspect and panscan are set per-video via setProperty() based on user scale mode.
    mpv::qt::set_option_variant(mpv, "keepaspect", "no");
    mpv::qt::set_option_variant(mpv, "video-unscaled", "no");

    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_set_wakeup_callback(mpv, MpvWidget::wakeup, this);
}

MpvWidget::~MpvWidget()
{
    shutdown();
    if (mpv) {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
    }
}

void MpvWidget::shutdown()
{
    if (mpv)
        mpv_command_string(mpv, "stop");
    if (mpv_gl) {
        mpv_render_context_free(mpv_gl);
        mpv_gl = nullptr;
    }
}

void MpvWidget::command(const QVariant &params)
{
    if (mpv)
        mpv::qt::command_variant(mpv, params);
}

void MpvWidget::setProperty(const QString &name, const QVariant &value)
{
    if (mpv)
        mpv::qt::set_property_variant(mpv, name, value);
}

QVariant MpvWidget::getProperty(const QString &name) const
{
    if (!mpv)
        return QVariant();
    return mpv::qt::get_property_variant(mpv, name);
}

void MpvWidget::initializeGL()
{
    mpv_opengl_init_params gl_init_params[1] = {{ MpvWidget::get_proc_address, nullptr }};
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE,            const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL) },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,  &gl_init_params },
        { MPV_RENDER_PARAM_INVALID,             nullptr }
    };

    if (mpv_render_context_create(&mpv_gl, mpv, params) < 0)
        throw std::runtime_error("failed to initialize mpv GL context");

    mpv_render_context_set_update_callback(mpv_gl, MpvWidget::on_update,
                                           reinterpret_cast<void *>(this));
}

void MpvWidget::paintGL()
{
    if (!mpv_gl)
        return;

    mpv_opengl_fbo mpfbo { static_cast<int>(defaultFramebufferObject()), width(), height(), 0 };
    int flip_y = 1;
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo },
        { MPV_RENDER_PARAM_FLIP_Y,     &flip_y },
        { MPV_RENDER_PARAM_INVALID,    nullptr }
    };
    mpv_render_context_render(mpv_gl, params);
}

void MpvWidget::on_mpv_events()
{
    while (mpv) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        handle_mpv_event(event);
    }
}

void MpvWidget::handle_mpv_event(mpv_event *event)
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

void MpvWidget::maybeUpdate()
{
    if (window()->isMinimized()) {
        makeCurrent();
        paintGL();
        context()->swapBuffers(context()->surface());
        doneCurrent();
    } else {
        update();
    }
}

void MpvWidget::on_update(void *ctx)
{
    QMetaObject::invokeMethod(reinterpret_cast<MpvWidget *>(ctx), &MpvWidget::maybeUpdate);
}

void MpvWidget::wakeup(void *ctx)
{
    QMetaObject::invokeMethod(reinterpret_cast<MpvWidget *>(ctx),
                              &MpvWidget::on_mpv_events,
                              Qt::QueuedConnection);
}

void *MpvWidget::get_proc_address(void *ctx, const char *name)
{
    Q_UNUSED(ctx)
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx)
        return nullptr;
    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}
