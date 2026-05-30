// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MPVNATIVEWIDGET_H
#define MPVNATIVEWIDGET_H

#include <QWidget>
#include <mpv/client.h>

// Native X11 window embedding for NVIDIA GPUs.
// Instead of using QOpenGLWidget + mpv OpenGL rendering (which conflicts with
// NVIDIA's strict GL context ownership), we pass the X11 window ID directly
// to mpv and let it render via Vulkan into a plain QWidget.
class MpvNativeWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MpvNativeWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
    ~MpvNativeWidget() override;

    void command(const QVariant &params);
    void setProperty(const QString &name, const QVariant &value);
    QVariant getProperty(const QString &name) const;
    void shutdown();

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void durationChanged(double value);
    void positionChanged(double value);

private slots:
    void on_mpv_events();

private:
    void handle_mpv_event(mpv_event *event);
    static void wakeup(void *ctx);

    mpv_handle *mpv = nullptr;
    bool m_embedded = false;
};

#endif // MPVNATIVEWIDGET_H
