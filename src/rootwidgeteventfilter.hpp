#ifndef ROOTWIDGETEVENTFILTER_HPP
#define ROOTWIDGETEVENTFILTER_HPP

#include "ddplugin_videowallpaper_global.h"

#include <DPlatformWindowHandle>

#include <QObject>
#include <QEvent>
#include <QWidget>

DDP_VIDEOWALLPAPER_BEGIN_NAMESPACE

class RootWidgetEventFilter : public QObject
{
    Q_OBJECT

public:
    RootWidgetEventFilter(QWidget *parent)
        : QObject(parent)
        , rootWidget(parent)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        /**
         * @bug QOpenGLWidget in Qt6 will recreate window handle
         * @ref https://github.com/linuxdeepin/deepin-deepinid-client/commit/2a305926a9047c699cf4d12e3e64aae17e8c367b
         */
        // Use qobject_cast (preferred for QObject-derived types — no RTTI, faster)
        QWidget *widget = qobject_cast<QWidget *>(watched);
        if (widget == nullptr) {
            return QObject::eventFilter(watched, event);
        }

        if (watched == rootWidget && event->type() == QEvent::WinIdChange) {
            Dtk::Widget::DPlatformWindowHandle handle(rootWidget);
        }

        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *rootWidget = nullptr;
};

DDP_VIDEOWALLPAPER_END_NAMESPACE

#endif // ROOTWIDGETEVENTFILTER_HPP
