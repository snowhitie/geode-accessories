# Accessories

A Geometry Dash Geode mod that lets you create, import, and customize visual accessories for player icons.

## 🚀 Quick Download

Pre-built `.geode` files for each supported platform are available in the [`.geode_files`](.geode_files/) folder.

| Platform | Download |
|---|---|
| 🪟 Windows 64-bit | [Download `snowhitie.win64.accessories.geode`](.geode_files/snowhitie.win64.accessories.geode) |
| 📱 Android 32-bit | [Download `snowhitie.android32.accessories.geode`](.geode_files/snowhitie.android32.accessories.geode) |
| 📱 Android 64-bit | [Download `snowhitie.android64.accessories.geode`](.geode_files/snowhitie.android64.accessories.geode) |

Choose the file that matches your platform. You do not need to build the mod yourself if you only want to use it.

> **Note:** The files in `.geode_files` are compiled builds of the mod. Source code and build files are available in the repository for anyone who wants to inspect or build the project.

## ✨ Features

- Custom accessories for **Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing, and Jetpack**.
- Windows and Android support.
- Import accessory folders with the in-game **+ ADD** button.
- Separate accessory folders and parts.
- Enable or disable folders and individual parts.
- Edit **X, Y, Scale, and Rotation**.
- Put accessories in front of or behind the player icon.
- Reset individual part settings.
- Live **Player 1 / Player 2** preview.
- Player 2 uses Player 1 placement/configuration with swapped player colors.
- Paginated folder and part lists.
- Accessory settings are saved.
- Optional `accessory.json` metadata.

## 📋 Requirements

- **Geometry Dash 2.2081**
- **Geode 5.10.1**
- Windows 64-bit or Android

## 📥 Installation

### Windows

1. Download the **Windows 64-bit** `.geode` file from the [`.geode_files`](.geode_files/) folder above.
2. Install Geode for Geometry Dash 2.2081 if you have not already.
3. Install the downloaded `snowhitie.win64.accessories.geode` file through Geode.
4. Start Geometry Dash.
5. Open the **Accessories** menu.

### Android

1. Download the `.geode` file for your Android architecture:
   - **Android 32-bit** — `snowhitie.android32.accessories.geode`
   - **Android 64-bit** — `snowhitie.android64.accessories.geode`
2. Install Geode for Geometry Dash 2.2081.
3. Install the downloaded Accessories `.geode` file through Geode.
4. Start Geometry Dash.
5. Open the **Accessories** menu.

If you are not sure which Android build you need, check which architecture your Geometry Dash/Geode installation uses.

## 📂 Adding Accessories

The easiest way to add an accessory is the in-game **+ ADD** button.

1. Open the **Accessories** menu.
2. Press **+ ADD**.
3. Select the folder containing your accessory.
4. The mod checks the folder for PNG files.
5. The folder is copied into the mod's accessory library.
6. The accessory appears in the folder list.

This method is especially useful on Android because you do not need to manually find the mod's private save directory.

## 🎨 Creating an Accessory

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

You do not need to include every file. The PNG layers that are present determine what the accessory can use.

### Supported files

| File | Purpose |
|---|---|
| `p1.png` | Player 1 colored layer |
| `p2.png` | Player 2 colored layer |
| `line.png` | Line / outline layer |
| `glow.png` | Glow layer |
| `selfcolor.png` | Self-colored layer |
| `accessory.json` | Optional accessory metadata/configuration |

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

Each part can be enabled, disabled, moved, scaled, rotated, and placed in front of or behind the icon independently.

If there are no part subfolders, the accessory folder can be used as a single part.

## 🛠️ Editing Accessories

After importing an accessory:

1. Select a game mode.
2. Select an accessory folder.
3. Select a part.
4. Turn the folder/part **ON** or **OFF**.
5. Adjust its position, scale, and rotation.
6. Choose **Front** or **Back**.
7. Use **Reset** if you want to restore the part's default settings.

### Control Steps

| Setting | Step |
|---|---:|
| X | 1 |
| Y | 1 |
| Scale | 0.1 |
| Rotation | 5° |

## 👤 Player 1 and Player 2

The editor shows Player 1 and Player 2 previews together.

Player 2 uses the same placement/configuration as Player 1, while the two player colors are swapped. This allows one accessory setup to work consistently for both players.

For example:

```text
Player 1:
Color 1 = White
Color 2 = Cyan

Player 2:
Color 1 = Cyan
Color 2 = White
```

## ↔️ Front and Back

Every accessory part can be rendered on a different layer:

- **Front** — renders in front of the player icon.
- **Back** — renders behind the player icon.

This can be used for accessories such as wings, halos, decorations, or parts that wrap around the icon.

## 💾 Accessory Library

Imported accessories are stored by the mod in its save directory under:

```text
Accessories/
```

The exact location depends on the platform. On Android, using **+ ADD** is recommended instead of manually searching for the private mod directory.

## ❓ Troubleshooting

### The accessory does not appear

Check that:

- The accessory folder contains at least one `.png` file.
- The PNG names use supported layer names such as `p1`, `p2`, `line`, `glow`, or `selfcolor`.
- The accessory was imported with **+ ADD** or is already inside the accessory library.
- The correct game mode is selected.
- The accessory folder and part are enabled.

### The + ADD button is missing

Make sure you installed a build that includes the folder-import feature and that you are using the current Accessories build from `.geode_files`.

### The accessory is in the wrong position

Select the part and adjust **X**, **Y**, **Scale**, and **Rotation**. Use **Reset** if necessary.

### The accessory is behind or in front of the wrong object

Change the part between **Front** and **Back**.

### Player 2 looks different

This is intentional. Player 2 uses Player 1's placement/configuration and swaps the two player colors.

## 🧩 Project Structure

```text
Accessories/
├── .github/
│   └── workflows/
├── .geode_files/
│   ├── snowhitie.android32.accessories.geode
│   ├── snowhitie.android64.accessories.geode
│   └── snowhitie.win64.accessories.geode
├── src/
│   └── main.cpp
├── CMakeLists.txt
├── mod.json
├── logo.png
└── README.md
```

The mod uses a single main C++ source file.

## 🔨 Building From Source

The project uses **CMake** and the **Geode SDK**.

Current configuration:

```text
Geometry Dash: 2.2081
Geode SDK:     5.10.1
```

GitHub Actions is configured to build:

- Windows 64-bit
- Android 32-bit
- Android 64-bit

The resulting `.geode` files can be placed in `.geode_files` so users can download the builds directly from the repository.

## 🔄 Updating the Downloads

When a new version of Accessories is released:

1. Build the mod for all supported platforms.
2. Replace the old `.geode` files in `.geode_files`.
3. Keep the filenames consistent with the platform names.
4. Update the version in `mod.json`.
5. Update this README if installation or features have changed.

For larger releases, GitHub Releases can also be used to distribute compiled `.geode` files alongside release notes.

## 🧑‍💻 Development Overview

```text
Accessory folder
      ↓
    Parts
      ↓
   PNG layers
      ↓
Per-part configuration
      ↓
Runtime rendering
```

The accessory manager scans the library, detects supported PNG layers, stores configuration values, and rebuilds the rendered accessory when the selected mode or configuration changes.

## 🐛 Bug Reports

If you find a bug, open a GitHub issue and include:

- Geometry Dash version
- Geode version
- Accessories version
- Platform
- Steps to reproduce the problem
- Screenshots or relevant build logs

## 👨‍💻 Credits

**Accessories**  
Developer: **Snowhitie**

Built with the **Geode SDK** for Geometry Dash.

## ⚡ Quick Start

```text
1. Download the correct .geode file from .geode_files.
2. Install Geode 5.10.1 for Geometry Dash 2.2081.
3. Install Accessories.
4. Open the Accessories menu.
5. Press + ADD.
6. Select your accessory folder.
7. Select a game mode.
8. Select a folder and part.
9. Enable the part.
10. Adjust X, Y, Scale, and Rotation.
11. Choose Front or Back.
12. Play Geometry Dash with your custom accessory.
```

---

**Accessories — Custom accessories for Geometry Dash**
