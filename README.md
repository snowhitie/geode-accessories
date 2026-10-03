# Accessories

A Geometry Dash Geode mod for creating, importing, and customizing visual accessories for player icons.

## 🔗 Related Repositories

### 🧩 Accessories Mod
This repository contains the **Accessories Geode mod**, its source code, build configuration, and ready-to-download `.geode` builds.

### 🎨 AccessoriesLibrary
[**snowhitie/AccessoriesLibrary**](https://github.com/snowhitie/AccessoriesLibrary)

This is the **official accessory library** for the mod. It contains ready-made accessories that you can download and use with Accessories.

If you are looking for accessories rather than the mod itself, go to the **AccessoriesLibrary** repository.

If you want your own accessory to be added to the library, see the contribution section below.

---

## 📥 Quick Download

### Install the Accessories mod

Open the `.geode_files` folder in this repository and download the build for your platform:

| Platform | File |
|---|---|
| 🪟 Windows 64-bit | `snowhitie.win64.accessories.geode` |
| 📱 Android 32-bit | `snowhitie.android32.accessories.geode` |
| 📱 Android 64-bit | `snowhitie.android64.accessories.geode` |

Choose the file that matches your device and install it through Geode.

### Download ready-made accessories

Ready-made accessories are **not stored in this repository**. They are available in the separate AccessoriesLibrary:

➡️ **[Open AccessoriesLibrary](https://github.com/snowhitie/AccessoriesLibrary)**

---

## ✨ Features

- Custom accessories for Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing, and Jetpack.
- Windows and Android support.
- Import accessory folders with the in-game **+ ADD** button.
- Separate accessory folders and parts.
- Enable/disable folders and parts.
- X/Y position, scale, and rotation editing.
- Front/Back layer control.
- Reset controls.
- Live Player 1 and Player 2 preview.
- Player 2 uses Player 1 placement/configuration with swapped player colors.
- Paged folder and part lists.
- Saved accessory settings.
- Optional `accessory.json` metadata.

---

## 📋 Requirements

- Geometry Dash **2.2081**
- Geode **5.10.1**
- Windows 64-bit or Android

---

## 📦 Installation

### Windows

1. Install Geode for Geometry Dash 2.2081.
2. Download `snowhitie.win64.accessories.geode` from the `.geode_files` folder in this repository.
3. Install the `.geode` file with Geode.
4. Start Geometry Dash.
5. Open the Accessories interface.

### Android

1. Install Geode for Geometry Dash 2.2081.
2. Choose the correct Android build:
   - `snowhitie.android32.accessories.geode` for 32-bit Android.
   - `snowhitie.android64.accessories.geode` for 64-bit Android.
3. Install the `.geode` file with Geode.
4. Start Geometry Dash.
5. Open Accessories.
6. Use **+ ADD** to import an accessory folder.

---

## 🎨 Getting Accessories

There are two ways to get accessories:

### 1. Download existing accessories

Go to the official **[AccessoriesLibrary](https://github.com/snowhitie/AccessoriesLibrary)** repository.

Download the accessory you want and import it into the Accessories mod.

### 2. Create your own accessory

You can create your own accessory using PNG files and the folder structure described below.

---

## 📁 Creating an Accessory

A simple accessory can look like this:

```text
MyAccessory/
├── p1.png
├── p2.png
├── line.png
├── glow.png
├── selfcolor.png
└── accessory.json
```

You do not need every file. The files that are present determine the available visual layers.

| File | Purpose |
|---|---|
| `p1.png` | Player 1 colored layer |
| `p2.png` | Player 2 colored layer |
| `line.png` | Line/outline layer |
| `glow.png` | Glow layer |
| `selfcolor.png` | Self-colored layer |
| `accessory.json` | Optional metadata/configuration |

Prefixed names are also supported, for example:

```text
crown_p1.png
crown_p2.png
crown_glow.png
```

### Multiple Parts

An accessory can contain multiple parts:

```text
MyAccessory/
├── Crown/
│   ├── p1.png
│   └── p2.png
├── Glasses/
│   ├── p1.png
│   └── p2.png
└── Wings/
    ├── p1.png
    └── p2.png
```

Each part can be enabled and edited separately.

---

## ➕ Importing an Accessory

### Windows and Android

1. Open the Accessories menu.
2. Press **+ ADD**.
3. Select the accessory folder.
4. The mod imports the folder into its accessory library.
5. Select the imported accessory from the folder list.

The selected folder must contain at least one PNG file.

---

## 🛠️ Editing Accessories

1. Select a game mode at the top.
2. Select an accessory folder.
3. Select a part.
4. Enable or disable the part.
5. Adjust its position, scale, and rotation.
6. Choose **Front** or **Back**.
7. Use **Reset** when needed.

### Control Steps

| Setting | Step |
|---|---:|
| X | 1 |
| Y | 1 |
| Scale | 0.1 |
| Rotation | 5° |

---

## 👥 Player 1 and Player 2

The editor previews Player 1 and Player 2 together.

Player 2 uses the same placement/configuration as Player 1, while the two player colors are swapped automatically.

For example:

```text
Player 1:
Color 1 → white
Color 2 → cyan

Player 2:
Color 1 → cyan
Color 2 → white
```

---

## 🔲 Front and Back

Each part can be rendered in two layers:

- **Front** — in front of the player icon.
- **Back** — behind the player icon.

This allows accessories to appear either over or behind the icon.

---

## 📚 Official AccessoriesLibrary

The separate **[AccessoriesLibrary](https://github.com/snowhitie/AccessoriesLibrary)** repository is intended for sharing ready-made accessories for the mod.

It currently contains accessories submitted for use with Accessories.

### Want to submit an accessory?

According to the library repository, if you want your accessory to be added:

1. Contact **@snowhitie** on Discord.
2. Provide your Discord username.
3. Send the PNG files for your accessory.
4. Organize the files using a folder tree similar to the accessories already in the library.

The accessory can then be reviewed for inclusion in the library.

---

## 📂 Accessory Library on the Device

Imported accessories are stored by the mod in its own `Accessories/` save directory.

The exact location depends on the platform. On Android, using **+ ADD** is recommended instead of manually searching for the mod's private directory.

---

## ❗ Troubleshooting

### The accessory does not appear

Check that:

- The folder contains at least one `.png` file.
- The PNG names use supported suffixes such as `p1`, `p2`, `line`, `glow`, or `selfcolor`.
- The folder was imported with **+ ADD** or is inside the accessory library.
- The correct game mode is selected.
- The folder and part are enabled.

### The + ADD button is missing

Make sure you installed a build that includes the folder-import feature.

### The accessory is in the wrong position

Select the part and adjust X, Y, Scale, and Rotation. Use **Reset** to start again.

### The accessory is behind/in front of the wrong object

Change the part between **Front** and **Back**.

### Player 2 looks different

This is intentional. Player 2 uses Player 1's placement and swaps the two player colors.

---

## 🏗️ Project Structure

```text
Accessories/
├── .github/
│   └── workflows/
├── .geode_files/
│   ├── snowhitie.win64.accessories.geode
│   ├── snowhitie.android32.accessories.geode
│   └── snowhitie.android64.accessories.geode
├── src/
│   └── main.cpp
├── CMakeLists.txt
├── mod.json
├── logo.png
└── README.md
```

The mod uses a single main C++ source file.

---

## 🔨 Building from Source

The project uses CMake and the Geode SDK.

GitHub Actions builds:

- Windows 64-bit
- Android 32-bit
- Android 64-bit

Configuration:

```text
Geometry Dash: 2.2081
Geode SDK:     5.10.1
```

The resulting `.geode` files are placed in the `.geode_files` folder for convenient downloading.

---

## 🔄 Repository Overview

```text
snowhitie/geode-accessories
│
├── Accessories mod source code
├── Build configuration
├── GitHub Actions
├── Ready-to-download .geode files
└── Mod documentation

snowhitie/AccessoriesLibrary
│
└── Ready-made accessories for the Accessories mod
```

**Mod:** [snowhitie/geode-accessories](https://github.com/snowhitie/geode-accessories)  
**Accessory Library:** [snowhitie/AccessoriesLibrary](https://github.com/snowhitie/AccessoriesLibrary)

---

## 👤 Credits

**Accessories**  
Developer: **Snowhitie**

Built with the **Geode SDK** for Geometry Dash.

---

## 🐛 Support

If you find a bug, open a GitHub issue and include:

- Geometry Dash version
- Geode version
- Platform
- Accessories version
- Steps to reproduce the problem
- Screenshots or relevant build logs

---

## ⚡ Quick Start

```text
1. Install Geode 5.10.1 for Geometry Dash 2.2081.
2. Download the correct Accessories .geode file from .geode_files.
3. Install the mod.
4. Open Geometry Dash.
5. Open the Accessories menu.
6. Download an accessory from the AccessoriesLibrary or create your own.
7. Press + ADD and select the accessory folder.
8. Select a game mode.
9. Select a folder and part.
10. Enable the part.
11. Adjust X, Y, Scale, and Rotation.
12. Choose Front or Back.
13. Play Geometry Dash with your custom accessory.
```
