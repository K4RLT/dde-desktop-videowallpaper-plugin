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
- 🔊 Volume control (0–100)
- ⚡ Playback speed control (0.25×–2×)
- 🖥️ Per-screen video assignment for multi-monitor setups
- 💾 All settings persist across reboots
- ⚡ Hardware accelerated via mpv — near-zero CPU usage at idle

---

## Requirements

- Deepin / UOS desktop environment
- Place your video files in `~/Videos/video-wallpaper/`

---

## Installation

Download the latest `.deb` from the [Releases](../../releases) page and install it:
```bash
sudo dpkg -i dd-videowallpaper-plugin-*.deb
```

Then log out and back in, or restart the shell:
```bash
pkill dde-shell
```

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
  Volume ▶
  Speed ▶
  ─────────────
  video1.mp4 ✓
  video2.mp4
  ...
```

---

## Building from Source

### Option 1 — Docker (recommended, no host deps needed)

```bash
git clone https://github.com/K4RLT/dde-desktop-videowallpaper-plugin.git
cd dde-desktop-videowallpaper-plugin

# Build the image (only needed once)
docker build -t dde-videowallpaper-builder .

# Compile the plugin
docker run --rm -v "$(pwd)":/src dde-videowallpaper-builder

# Output .so will be at:
# build/src/libdd-videowallpaper-plugin.so
```

Then install manually:
```bash
sudo cp build/src/libdd-videowallpaper-plugin.so \
    /usr/lib/x86_64-linux-gnu/dde-file-manager/plugins/desktop-edge/
sudo cp assets/configs/org.deepin.dde.file-manager.desktop.videowallpaper.json \
    /usr/share/dsg/configs/org.deepin.dde.file-manager/
```

---

### Option 2 — Native build

#### Install build dependencies
```bash
sudo apt install \
    cmake \
    libmpv-dev \
    libdtk6core-dev \
    libdtk6widget-dev \
    dde-file-manager-dev \
    qt6-base-dev \
    qt6-base-private-dev \
    libdde-shell-dev \
    libxcb-ewmh-dev
```

#### Build
```bash
git clone https://github.com/K4RLT/dde-desktop-videowallpaper-plugin.git
cd dde-desktop-videowallpaper-plugin
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo cp src/libdd-videowallpaper-plugin.so \
    /usr/lib/x86_64-linux-gnu/dde-file-manager/plugins/desktop-edge/
sudo cp ../assets/configs/org.deepin.dde.file-manager.desktop.videowallpaper.json \
    /usr/share/dsg/configs/org.deepin.dde.file-manager/
```

---

## Credits

- Original plugin by [zty199](https://github.com/zty199/dde-desktop-videowallpaper-plugin)
- Forked and extended by [K4RLT](https://github.com/K4RLT)
- HiDPI wallpaper sizing fix contributed by [@DonkeyKongG3me](https://t.me/DonkeyKongG3me) on Telegram

---

## License

GPL-3.0 — see [LICENSE](LICENSE)
