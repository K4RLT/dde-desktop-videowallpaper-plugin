# Live Wallpaper for Deepin

A video wallpaper plugin for the Deepin Desktop Environment (DDE), letting you set any video file as your live desktop wallpaper — powered by mpv.

> Based on the original work by [zty199](https://github.com/zty199/dde-desktop-videowallpaper-plugin)

---

## Features

- 🎬 Play any video file as your desktop wallpaper
- 📁 Multi-video selection from a dedicated folder
- ⏸ Pause when a window goes fullscreen or maximized
- 💤 Pause on idle (30s / 1min / 5min / 10min)
- 🔲 Scale modes: Fill, Fit, Crop
- 🔊 Audio toggle
- 💾 All settings persist across reboots
- ⚡ Hardware accelerated via mpv — near-zero CPU usage at idle

---

## Requirements

- Deepin / UOS desktop environment
- `libmpv` installed
- Place your video files in `~/Videos/video-wallpaper/`

---

## Installation

Download the latest `.deb` from the [Releases](../../releases) page and install it:
```bash
sudo dpkg -i dd-videowallpaper-plugin-v1.0.deb
```

Then restart the desktop or log out and back in.

---

## Usage

Right-click the desktop → **Video wallpaper** to access the menu:
```
Video wallpaper ▶
  Disable
  ─────────────
  Pause when fullscreen
  Pause on idle ▶
  Scale mode ▶
  Enable audio
  ─────────────
  video1.mp4 ✓
  video2.mp4
  ...
```

---

## Building from Source
```bash
git clone https://github.com/K4RLT/dde-desktop-videowallpaper-plugin.git
cd dde-desktop-videowallpaper-plugin
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo cp src/libdd-videowallpaper-plugin.so /usr/lib/x86_64-linux-gnu/dde-file-manager/plugins/desktop-edge/
sudo cp assets/configs/org.deepin.dde.file-manager.desktop.videowallpaper.json \
    /usr/share/dsg/configs/org.deepin.dde.file-manager/
```

---

## Credits

- Original plugin by [zty199](https://github.com/zty199/dde-desktop-videowallpaper-plugin)
- Forked and extended by [K4RLT](https://github.com/K4RLT)

---

## License

GPL-3.0 — see [LICENSE](LICENSE)
