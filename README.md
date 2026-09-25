# CivVIReplay

CivVIReplay is a modern C++ application designed to parse and visualize **Civilization VI multiplayer replay data**.
It reconstructs game state information from replay files and presents it through a real-time graphical interface powered by OpenGL and ImGui.

This tool is specifically designed for the competitive multiplayer scene and relies on commonly used multiplayer balance and gameplay mods such as **BBG (Better Balanced Game)**, **BBS (Better Balanced Starts)**, and **MPH (Multiplayer Helper)**. Replays generated without these mods may not be fully supported.

CivVIReplay is built with the multiplayer community in mind, aiming to provide better analytical tools for competitive matches, tournaments, and post-game review.

If you are interested in Civilization VI multiplayer, you can explore community hubs such as:

- **CivFR** (French multiplayer community)
- **CPL (Civilization Players League)** — one of the largest international competitive communities

The project was developed as both a practical tool for the multiplayer ecosystem and a technical exploration of binary parsing, structured data modeling, and rendering architecture using modern C++ (C++17).

![Replay Visualization](assets/screenshot/mainWindow.jpg "CivVIReplay in action")

---

## ✨ Features

- Replay file parsing
- Structured game state reconstruction
- OpenGL-based rendering
- ImGui-driven graphical interface
- Automatic runtime asset deployment

---

## 🧱 Prerequisites

To build the project, you need:

- **CMake ≥ 3.16**
- **C++17 compatible compiler**
  - Recommended: Visual Studio 2022 (MSVC)
- Windows x64
- OpenGL-compatible GPU

The project targets Windows and Linux (see [Linux](#-linux)).

### Third-Party Dependencies

The following libraries are included in the repository:

- **Dear ImGui** – Immediate mode graphical user interface
- **GLFW** – Window and OpenGL context management
- **OpenGL** – Rendering API
- **zlib** – Data compression
- **Cereal** – C++11 serialization library
- **Lua** – Embedded scripting language
- **jsoncpp** – JSON parsing and serialization library

Each dependency remains under its respective license. Please refer to their official repositories for licensing details.

## 🐧 Linux

The app also builds natively on Linux (tested on Ubuntu 24.04 / Pop!_OS with Civ VI running through Proton).

```bash
sudo apt install build-essential cmake libglfw3-dev libgl-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/CivVIReplay
```

Install for the current user (binary and assets in `~/.local/share/civvireplay`, launcher `~/.local/bin/civvireplay`, and a desktop menu entry):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build -j && cmake --install build
```

Paths are detected automatically:

- **Civ VI install**: from Steam's `libraryfolders.vdf` (`~/.steam/steam`, `~/.local/share/Steam` or Flatpak Steam)
- **Saves / mods**: the Proton prefix (`steamapps/compatdata/289070/pfx/.../Documents/My Games/Sid Meier's Civilization VI`), otherwise the native port's `~/.local/share/aspyr-media/Sid Meier's Civilization VI`
- **Game database**: the Proton prefix's `AppData/Local/Firaxis Games/.../Cache/DebugGameplay.sqlite`, falling back to the bundled `image/database/backup.sqlite`
- **Settings** (`imgui.ini`) and `crash.log`: `~/.config/civvireplay/`

To launch it from Steam (e.g. in Big Picture), use *Games → Add a Non-Steam Game* and pick `~/.local/share/civvireplay/CivVIReplay`.

`CivVIReplay --dump <save>...` parses saves without opening a window and prints a summary (turn, map, players), which is handy for checking the parser.

## 🚧 Current Limitations

- Tested on specific Civilization VI versions
- No automated test suite yet
- Limited replay compatibility validation

---

## 🗺 Roadmap

Planned improvements include:

- Enhanced replay visualization
- Linux build support
- Introduction of automated testing
- Improved error handling and validation
- Refactoring of the parsing layer for greater extensibility

---

## 🤝 Contributing

Contributions are welcome.

If you would like to contribute:

1. Open an issue to discuss significant changes before implementation.
2. Follow the existing project structure and conventions.
3. Keep changes modular and well-scoped.
4. Ensure the project builds successfully before submitting a pull request.

---

## 📦 License

This project is licensed under the MIT License.
See the `LICENSE` file for details.

---

## 🙏 Acknowledgments

Special thanks to **Vodairo**, who created and designed the entire `civmodutilities` library.
The parsing architecture, schema handling, and core replay reconstruction logic are built upon this foundational work.

This project would not exist in its current form without that contribution.

Additional thanks to the maintainers of the following open-source projects:

- Dear ImGui
- GLFW
- zlib
- Cereal
- Lua
- jsoncpp
- OpenGL (Khronos Group)

All credit goes to the respective authors and contributors.