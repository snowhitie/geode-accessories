# Accessories

A Geometry Dash Geode mod for creating, importing, and customizing visual accessories for player icons.

## Features

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

## Requirements

- Geometry Dash 2.2081
- Geode 5.10.1
- Windows 64-bit or Android

## Installation

### Windows

1. Install Geode for Geometry Dash 2.2081.
2. Install the compiled `Accessories.geode` file.
3. Start Geometry Dash.
4. Open the Accessories interface.

### Android

1. Install Geode for Geometry Dash 2.2081.
2. Install `Accessories.geode`.
3. Start Geometry Dash.
4. Open Accessories.
5. Press **+ ADD** and select an accessory folder.

Using **+ ADD** is recommended on Android because the mod copies the selected folder into its own accessory library.

## Creating an accessory

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

Prefixed names are also supported, for example `crown_p1.png`, `crown_p2.png`, and `crown_glow.png`.

## Multiple parts

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

If there are no part subfolders, the accessory folder itself can be treated as a single part.

## Importing accessories

### Android and Windows

Open Accessories and press:

```text
+ ADD
```

Select a folder containing at least one PNG. The mod imports the folder into its accessory library, rescans the library, and selects the imported accessory.

## Editing

1. Select a game mode at the top.
2. Select an accessory folder.
3. Select a part.
4. Enable or disable the part.
5. Adjust its position, scale, and rotation.
6. Choose **Front** or **Back**.
7. Use **Reset** when needed.

### Control steps

| Setting | Step |
|---|---:|
| X | 1 |
| Y | 1 |
| Scale | 0.1 |
| Rotation | 5° |

## Player 1 and Player 2

The editor previews Player 1 and Player 2 together.

Player 2 intentionally uses the same accessory placement/configuration as Player 1, while swapping the two player colors.

Example:

```text
Player 1: Color 1 = White, Color 2 = Cyan
Player 2: Color 1 = Cyan,  Color 2 = White
```

## Front and Back

Each part can be rendered:

- **Front** — in front of the player icon.
- **Back** — behind the player icon.

This makes it possible to create accessories that wrap around the icon.

## Accessory library

Imported accessories are stored by the mod under its save directory in:

```text
Accessories/
```

The exact save location depends on the platform. On Android, use **+ ADD** instead of manually searching for the mod's private directory.

## Troubleshooting

### The accessory does not appear

Check that:

- The folder contains at least one `.png` file.
- The PNG names use supported suffixes such as `p1`, `p2`, `line`, `glow`, or `selfcolor`.
- The folder was imported with **+ ADD** or is inside the accessory library.
- The correct game mode is selected.
- The folder and part are enabled.

### The + ADD button is missing

Make sure you installed a build that includes the folder-import feature. The current source contains the native Geode folder picker and the **+ ADD** button.

### The accessory is in the wrong position

Select the part and adjust X, Y, Scale, and Rotation. Use **Reset** to start again.

### The accessory is behind/in front of the wrong object

Change the part between **Front** and **Back**.

### Player 2 looks different

This is intentional: Player 2 uses Player 1's placement and swaps the two player colors.

## Project structure

```text
Accessories/
├── .github/
│   └── workflows/
├── src/
│   └── main.cpp
├── CMakeLists.txt
├── mod.json
├── icon.png
└── README.md
```

The mod uses a single main C++ source file.

## Building from source

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

Build artifacts are produced by GitHub Actions.

## Development overview

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

The accessory manager scans the library, detects PNG layers, stores configuration values, and rebuilds the rendered accessory when the selected mode or configuration changes.

## Credits

**Accessories**  
Developer: **Snowhitie**

Built with the **Geode SDK** for Geometry Dash.

## Support

If you find a bug, open a GitHub issue and include:

- Geometry Dash version
- Geode version
- Platform
- Accessories version
- Steps to reproduce the problem
- Screenshots or relevant build logs

## Quick start

```text
1. Install Geode 5.10.1 for Geometry Dash 2.2081.
2. Install Accessories.
3. Open the Accessories menu.
4. Press + ADD.
5. Select your accessory folder.
6. Select a game mode.
7. Select a folder and part.
8. Enable the part.
9. Adjust X, Y, Scale, and Rotation.
10. Choose Front or Back.
11. Play Geometry Dash with your custom accessory.
```
