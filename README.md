# gameoff2026

C++ bootstrap for GitHub Game Off 2026. The current targets are macOS and
the browser through Emscripten. Windows and Linux support is planned.

The smoke test opens a resizable window and animates a cyan square against a
dark blue background. Close the native window or press Escape to quit.

## Prerequisites

- CMake 3.25 or newer and Ninja
- For macOS, Xcode Command Line Tools with a C++23-capable Apple Clang
- For the browser, an activated [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
- Python 3 for the local browser server
- clang-tidy compatible with the checked-in configuration and selected compiler
- Optional clang-format 21 or newer for the checked-in formatting configuration

Initialize the pinned SDL submodule after cloning:

```sh
git submodule update --init --recursive
```

## macOS

Run from the repository root:

```sh
cmake -S . -B build/native/mac -G "Ninja Multi-Config"
cmake --build build/native/mac --config Debug
./build/native/mac/Debug/gameoff2026

cmake --build build/native/mac --config Release
./build/native/mac/Release/gameoff2026
```

This builds an executable for the host architecture, without app-bundle packaging.

## Browser

Activate your Emscripten SDK in the current shell first. Then run:

```sh
emcmake cmake -S . -B build/web -G "Ninja Multi-Config"
cmake --build build/web --config Debug
cmake --build build/web --config Release
python3 -m http.server 8000 --directory build/web
```

Open `http://localhost:8000/Debug/` or `http://localhost:8000/Release/`.
Serve the generated files over HTTP rather than opening the HTML as a local
file. Each configuration produces `index.html`, `index.js`, and `index.wasm`.
Keep these files together when hosting the game, and include
`ThirdPartyLicenses.txt` with distributions.

The bootstrap uses Emscripten's default HTML shell and has no asset files or
worker threads. Keep native and browser build trees separate. When changing
toolchains, use a fresh build directory.

## Editor and analysis

CMake exports `compile_commands.json` in each platform build directory.
After a successful configure in VS Code, CMake Tools copies the active platform's
database to `build/compile_commands.json`, matching the existing `.clangd`.
Command-line configuration does not perform this copy. Reconfigure through
CMake Tools after switching platforms to refresh editor analysis.
Multi-config databases contain entries for both configurations, so editor
analysis is not a substitute for building each configuration.

Clang-tidy runs by default on project C/C++ targets and is required when enabled.
Third-party targets do not inherit this setting. Existing CMake caches may still
contain `ENABLE_CLANG_TIDY=OFF`, so reconfigure those with
`-DENABLE_CLANG_TIDY=ON`. To opt out, reconfigure with `-DENABLE_CLANG_TIDY=OFF`.
The CMake option's `ON` default applies to fresh caches. The web variant also
explicitly sets `ENABLE_CLANG_TIDY` to `ON`, overriding the cache on VS Code
configuration. Change that variant setting to `OFF` to opt out for that workflow.
Use `clang-format -i main.cpp` to apply the checked-in style.

### VS Code workflow

Install CMake Tools (`ms-vscode.cmake-tools`), clangd (`llvm-vs-code-extensions.vscode-clangd`),
and CodeLLDB (`vadimcn.vscode-lldb`) for native debugging. For browser preview,
install Live Preview (`ms-vscode.live-server`). The workspace disables Microsoft's
C/C++ IntelliSense so clangd supplies editor analysis.

Use **CMake: Select a Kit** to select Apple Clang for macOS, and
**CMake: Select Variant** to select macOS Debug or Release. Run **CMake: Configure**,
then **CMake: Build**. Select `gameoff2026` as the CMake launch target and use the
**Game (LLDB, macOS)** launch configuration to debug. The executable path and
configuration come from the selected CMake target.

For web builds, activate the Emscripten SDK and launch VS Code from that shell
so it inherits `EMSDK` and the SDK's executable search path. Fully quit existing
VS Code processes first if they were started without that environment.
The web variant expects the standard SDK layout under
`$EMSDK/upstream/emscripten`.

Select the **[Unspecified]** kit and a Web Debug or Release variant, then configure
and build. The variant supplies the Emscripten toolchain, so a native compiler
kit must not override it. Web variants explicitly enable build-time clang-tidy.
Switch back to the Apple Clang kit and a macOS variant before native debugging.

Live Preview serves `build/web`. Open the generated `Debug/index.html` or
`Release/index.html` with Live Preview in an external browser. Native LLDB launch
does not debug WebAssembly. The command-line HTTP server above remains available.

The platform variant determines the build directory. Debug and Release share
the same platform tree. These settings use variants, without CMake presets.

See [build architecture](docs/architecture/build.md) for target ownership,
compiler policy, and planned platform directories, and
[architecture decisions](docs/architecture/decisions.md) for the rationale.
