// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videowallpapermenuscene.h"
#include "wallpaperconfig.h"
#include "ddplugin_videowallpaper_global.h"
#include "dfm-base/dfm_menu_defines.h"

#include <DDialog>

#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QProcess>
#include <QStandardPaths>
#include <QVariantHash>

using namespace ddplugin_videowallpaper;
DFMBASE_USE_NAMESPACE

// ── Creator ───────────────────────────────────────────────────────────────────

AbstractMenuScene *VideoWallpaerMenuCreator::create()
{
    return new VideoWallpaperMenuScene();
}

// ── Scene ─────────────────────────────────────────────────────────────────────

VideoWallpaperMenuScene::VideoWallpaperMenuScene(QObject *parent)
    : AbstractMenuScene(parent)
{
    predicateName[ActionID::kVideoWallpaper] = tr("Video wallpaper");
}

QString VideoWallpaperMenuScene::name() const
{
    return VideoWallpaerMenuCreator::name();
}

bool VideoWallpaperMenuScene::initialize(const QVariantHash &params)
{
    turnOn     = WpCfg->enable();
    isEmptyArea = params.value(MenuParamKey::kIsEmptyArea).toBool();
    onDesktop  = params.value(MenuParamKey::kOnDesktop).toBool();
    return isEmptyArea && onDesktop;
}

AbstractMenuScene *VideoWallpaperMenuScene::scene(QAction *action) const
{
    if (!action)
        return nullptr;
    if (predicateAction.values().contains(action))
        return const_cast<VideoWallpaperMenuScene *>(this);
    return AbstractMenuScene::scene(action);
}

bool VideoWallpaperMenuScene::create(QMenu *parent)
{
    const QString folderPath =
        QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).first()
        + "/video-wallpaper";

    const QFileInfoList files = QDir(folderPath).entryInfoList(
        { "*.mp4", "*.mkv", "*.webm", "*.avi", "*.mov" }, QDir::Files, QDir::Name);

    // No videos — show a simple toggle action
    if (files.isEmpty()) {
        QAction *action = parent->addAction(predicateName.value(ActionID::kVideoWallpaper));
        predicateAction[ActionID::kVideoWallpaper] = action;
        action->setProperty(ActionPropertyKey::kActionID, QString(ActionID::kVideoWallpaper));
        action->setCheckable(true);
        action->setChecked(turnOn);
        return true;
    }

    videoSubMenu = new QMenu(predicateName.value(ActionID::kVideoWallpaper), parent);

    // ── Disable ───────────────────────────────────────────────────────────────
    QAction *disableAction = videoSubMenu->addAction(tr("Disable"));
    disableAction->setCheckable(true);
    disableAction->setChecked(!turnOn);
    connect(disableAction, &QAction::triggered, this, [=]() {
        WpCfg->setPauseOnFullscreen(false);
        WpCfg->setPauseIdleSeconds(0);
        emit WpCfg->changeEnableState(false);
    });

    videoSubMenu->addSeparator();

    // ── Pause when fullscreen ─────────────────────────────────────────────────
    QAction *pauseFsAction = videoSubMenu->addAction(tr("Pause when fullscreen"));
    pauseFsAction->setCheckable(true);
    pauseFsAction->setChecked(WpCfg->pauseOnFullscreen());
    pauseFsAction->setEnabled(turnOn);
    connect(pauseFsAction, &QAction::triggered, this, [=](bool checked) {
        WpCfg->setPauseOnFullscreen(checked);
    });

    // ── Pause on idle ─────────────────────────────────────────────────────────
    QMenu *idleMenu = new QMenu(tr("Pause on idle"), videoSubMenu);
    idleMenu->setEnabled(turnOn);

    struct IdleOption { QString label; int seconds; };
    const QList<IdleOption> idleOptions = {
        { tr("Disable"),     0    },
        { tr("30 seconds"),  30   },
        { tr("1 minute"),    60   },
        { tr("2 minutes"),   120  },
        { tr("5 minutes"),   300  },
        { tr("10 minutes"),  600  },
        { tr("15 minutes"),  900  },
        { tr("30 minutes"),  1800 },
    };

    const int currentIdle = WpCfg->pauseIdleSeconds();
    for (const IdleOption &opt : idleOptions) {
        QAction *a = idleMenu->addAction(opt.label);
        a->setCheckable(true);
        a->setChecked(currentIdle == opt.seconds);
        const int secs = opt.seconds;
        connect(a, &QAction::triggered, this, [=]() {
            WpCfg->setPauseIdleSeconds(secs);
        });
    }
    videoSubMenu->addMenu(idleMenu);

    videoSubMenu->addSeparator();

    // ── Scale mode ────────────────────────────────────────────────────────────
    QMenu *scaleMenu = new QMenu(tr("Scale mode"), videoSubMenu);
    scaleMenu->setEnabled(turnOn);

    struct ScaleOption { QString label; QString mode; };
    const QList<ScaleOption> scaleOptions = {
        { tr("Fill"), "fill" },
        { tr("Fit"),  "fit"  },
        { tr("Crop"), "crop" },
    };

    const QString currentScale = WpCfg->scaleMode();
    for (const ScaleOption &opt : scaleOptions) {
        QAction *a = scaleMenu->addAction(opt.label);
        a->setCheckable(true);
        a->setChecked(currentScale == opt.mode);
        const QString mode = opt.mode;
        connect(a, &QAction::triggered, this, [=]() {
            WpCfg->setScaleMode(mode);
        });
    }
    videoSubMenu->addMenu(scaleMenu);

    QAction *audioAction = videoSubMenu->addAction(tr("Enable audio"));
    audioAction->setCheckable(true);
    audioAction->setChecked(WpCfg->enableAudio());
    audioAction->setEnabled(turnOn);
    connect(audioAction, &QAction::triggered, this, [=](bool checked) {
        WpCfg->setEnableAudio(checked);
    });

    videoSubMenu->addSeparator();

    // ── Video list ────────────────────────────────────────────────────────────
    const QString currentPath = WpCfg->videoPath();
    for (const QFileInfo &file : files) {
        QAction *a = videoSubMenu->addAction(file.fileName());
        a->setCheckable(true);
        a->setChecked(turnOn && file.absoluteFilePath() == currentPath);
        connect(a, &QAction::triggered, this, [=]() {
            // Warn on high-resolution videos
            QProcess ffprobe;
            ffprobe.start("ffprobe", {
                "-v", "error",
                "-select_streams", "v:0",
                "-show_entries", "stream=width,height",
                "-of", "csv=s=x:p=0",
                file.absoluteFilePath()
            });
            ffprobe.waitForFinished(3000);
            const QStringList parts = QString(ffprobe.readAllStandardOutput().trimmed()).split('x');
            if (parts.size() == 2) {
                const int w = parts[0].toInt();
                const int h = parts[1].toInt();
                QString resLabel;
                if      (w >= 7680 || h >= 4320) resLabel = "8K";
                else if (w >= 3840 || h >= 2160) resLabel = "4K";
                else if (w >= 2560 || h >= 1440) resLabel = "2K";

                if (!resLabel.isEmpty()) {
                    DTK_WIDGET_NAMESPACE::DDialog warn;
                    warn.setWindowTitle(tr("High Resolution Warning"));
                    warn.setMessage(tr("This video is %1 (%2x%3).\n\n"
                                       "High resolution videos may cause high CPU/GPU usage "
                                       "and affect system performance.\n\n"
                                       "Do you want to continue?")
                                    .arg(resLabel).arg(w).arg(h));
                    warn.setIcon(QIcon::fromTheme("dialog-warning"));
                    warn.addButton(tr("Cancel"),   false, DTK_WIDGET_NAMESPACE::DDialog::ButtonWarning);
                    warn.addButton(tr("Continue"), true,  DTK_WIDGET_NAMESPACE::DDialog::ButtonRecommend);
                    if (warn.exec() != 1)
                        return;
                }
            }

            WpCfg->setVideoPath(file.absoluteFilePath());
            emit WpCfg->changeEnableState(true);
        });
    }

    // Hidden placeholder action used for menu positioning
    QAction *placeholder = parent->addAction(predicateName.value(ActionID::kVideoWallpaper));
    predicateAction[ActionID::kVideoWallpaper] = placeholder;
    placeholder->setProperty(ActionPropertyKey::kActionID, QString(ActionID::kVideoWallpaper));
    placeholder->setVisible(false);

    return true;
}

void VideoWallpaperMenuScene::updateState(QMenu *parent)
{
    const auto actions = parent->actions();
    auto it = std::find_if(actions.begin(), actions.end(), [](const QAction *ac) {
        return ac->property(ActionPropertyKey::kActionID).toString() == "wallpaper-settings";
    });

    if (it == actions.end()) {
        fmWarning() << "cannot find wallpaper-settings action";
        return;
    }

    if (videoSubMenu) {
        parent->insertMenu(*it, videoSubMenu);
        parent->removeAction(predicateAction[ActionID::kVideoWallpaper]);
    } else {
        parent->insertAction(*it, predicateAction[ActionID::kVideoWallpaper]);
    }

    AbstractMenuScene::updateState(parent);
}

bool VideoWallpaperMenuScene::triggered(QAction *action)
{
    if (predicateAction.values().contains(action)) {
        if (action->property(ActionPropertyKey::kActionID).toString() == ActionID::kVideoWallpaper)
            emit WpCfg->changeEnableState(action->isChecked());
        return true;
    }
    return AbstractMenuScene::triggered(action);
}
