# Development guide

C++ bootstrap for GitHub Game Off 2026. The current targets are macOS and
the browser through Emscripten. Windows and Linux support is planned.

The smoke test opens a resizable window and animates a cyan square against a
dark blue background. Close the native window or press Escape to quit.

## Daily workflow

After completing the [setup](#prerequisites), use VS Code's Command Palette
(**Cmd+Shift+P** on macOS):

1. Select your platform and Debug/Release configuration with **CMake: Select Variant**.
   Use the **[Unspecified]** kit for Web or Apple Clang for macOS.
2. Run **CMake: Configure** after switching platforms, then **CMake: Build**.
3. For Web, start the server from the project root and leave it running:

   ```sh
   python3 -m http.server 8000 --directory build/web
   ```

4. In Run and Debug, select **Game (Browser)** for Web or **Game (LLDB, macOS)**
   for native, then press **F5**. Use Web Debug for C++ browser breakpoints.

After code changes, build again and restart the debug session. F5 does not build
the game. The browser server can stay running between launches.

## Prerequisites

Install the tools needed for your workflow and make command-line tools available
on `PATH`, including in VS Code's environment.

| Tool | Used for |
| --- | --- |
| Git | Cloning the project and initializing the pinned SDL submodule |
| CMake 3.25 or newer | Configuring native and web builds |
| Ninja | Building with Ninja Multi-Config |
| Xcode Command Line Tools with a C++23-capable Apple Clang | Native macOS builds |
| Python 3 | Running the web compiler and serving browser builds locally |
| clang-tidy | Required by default for project build-time analysis, compatible with the checked-in configuration and selected compiler |
| clangd | C++ editor analysis through the VS Code clangd extension |
| clang-format 21 or newer | Optional command-line formatting with the checked-in configuration |
| Google Chrome | Running the configured browser debugging session |
| Visual Studio Code | Using the checked-in editor and F5 launch configurations |

On macOS, `xcode-select --install` installs the Command Line Tools. Homebrew users
can install Chrome with `brew install --cask google-chrome`. The clangd extension
can help install its language-server executable if one is not already available.
Build-time clang-tidy still needs its own executable: CMake looks for
`clang-tidy-22` or `clang-tidy`.

Emscripten 5.0.7 and Node.js 22.16.0 are downloaded and configured automatically
on the first web configure. No separate SDK manager, Node installation, or npm
install is needed. That first configure needs internet access or the
[cached toolchain archives](#toolchain-cache-and-offline-builds).

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

The root `Emscripten.cmake` downloads and configures pinned Emscripten 5.0.7 and
Node.js 22.16.0 for your development machine. No Emscripten/Node installation or
SDK activation is needed. Run:

```sh
cmake -S . -B build/web -G "Ninja Multi-Config" -DCMAKE_TOOLCHAIN_FILE=Emscripten.cmake
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

When migrating an existing web build from a system Emscripten installation,
remove the generated `build/web` directory once and configure again. Normal
Debug/Release or native/web switching does not require deleting build directories.
Do not share CMake build caches between machines or filesystem locations.

### Toolchain cache and offline builds

Downloads live in `build/toolchains/archives`. Extracted tools live in
`build/toolchains/<host>/emscripten-5.0.7-node-22.16.0`. The host distinguishes
macOS, Linux, and Windows and their CPU architectures. These files are ignored
by Git. Reconfiguring reuses the extracted tools, and deleting `build/web`
does not remove the toolchain cache.

Use `-DGAMEOFF_TOOLCHAIN_CACHE=/absolute/path` to select another cache location.
For offline setup, copy the required archives into that cache's `archives`
directory and configure with `-DGAMEOFF_TOOLCHAIN_OFFLINE=ON`. Missing archives
produce an error naming the required file and its download URL. Both archives
are verified against SHA-256 hashes pinned in `ProvisionEmscripten.cmake` before extraction.

For example, an Apple Silicon Mac needs:

- `emscripten-5.0.7-Darwin-arm64.tar.xz`
- `node-v22.16.0-darwin-arm64.tar.xz`

The Emscripten archive includes its JavaScript build helpers, so no npm install
is needed. Offline support covers this bootstrap. Adding Emscripten ports or
other dependencies that download during compilation requires caching those too.

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

Install these extensions from VS Code's Extensions view (**Cmd+Shift+X** on macOS).
Search by the exact extension ID to distinguish similarly named extensions.
The same list is available as workspace recommendations in
[.vscode/extensions.json](../.vscode/extensions.json). Recommendations do not install
extensions automatically or pin their versions.

| Extension | Extension ID | Used for |
| --- | --- | --- |
| CMake Tools | `ms-vscode.cmake-tools` | Configure, build, and select platforms and Debug/Release variants |
| clangd | `llvm-vs-code-extensions.vscode-clangd` | C++ completion, navigation, and editor diagnostics |
| CodeLLDB | `vadimcn.vscode-lldb` | Native macOS C++ debugging with Game (LLDB, macOS) |
| WebAssembly DWARF Debugging | `ms-vscode.wasm-dwarf-debugging` | C++ source breakpoints and variable inspection in Game (Browser) |
| C/C++ Extension Pack | `ms-vscode.cpptools-extension-pack` | Optional but recommended bundle containing C/C++, C/C++ Themes, C/C++ DevTools, and CMake Tools |

VS Code supplies the Chrome JavaScript debugger. The DWARF extension adds C++
WebAssembly support. The workspace disables Microsoft's C/C++ IntelliSense so
clangd supplies editor analysis. Live Server and Live Preview are not required:
Python 3 supplies the local HTTP server.

Use **CMake: Select a Kit** to select Apple Clang for macOS, and
**CMake: Select Variant** to select macOS Debug or Release. Run **CMake: Configure**,
then **CMake: Build**. Select `gameoff2026` as the CMake launch target and use the
**Game (LLDB, macOS)** launch configuration to debug. The executable path and
configuration come from the selected CMake target.

For web builds, VS Code needs CMake, Ninja, Python, and clang-tidy available.
The web variant selects the root `Emscripten.cmake`, which manages the local
toolchain cache. It does not use `EMSDK` or require an activated Emscripten shell.

Select the **[Unspecified]** kit and a Web Debug or Release variant, then configure
and build. The variant supplies the Emscripten toolchain, so a native compiler
kit must not override it. Web variants explicitly enable build-time clang-tidy.
Switch back to the Apple Clang kit and a macOS variant before native debugging.

After building the web configuration, run this command from the project root
in a terminal:

```sh
python3 -m http.server 8000 --directory build/web
```

Leave that terminal running between browser launches. Press **Ctrl+C** in the
terminal when you want to stop the server.

Click the **Run and Debug** icon on the left sidebar, select **Game (Browser)**
from the dropdown beside the green play button, and press **F5**. This launches
Chrome with debugging attached, using CMake's currently selected Debug or Release
configuration at `http://localhost:8000/Debug/index.html` or `/Release/index.html`.
F5 does not build the game or start the server. Build again after code changes.
The launch expects port 8000 and `build/web` as the server root, matching the
command above. Native LLDB launch does not debug WebAssembly.

For C++ browser breakpoints, select **Web Debug** and build. CMake's Debug flags
pass `-g` during compilation and linking, preserving DWARF debug information in
`index.wasm`. Set a breakpoint beside an executable line in `main.cpp`, then
launch **Game (Browser)** with F5. After installing the DWARF extension, restart
any active debug session. Breakpoints, stepping, and variable inspection happen
in VS Code while the game runs in Chrome. See
[VS Code's WebAssembly debugging documentation](https://code.visualstudio.com/docs/nodejs/nodejs-debugging#_debugging-webassembly).

The platform variant determines the build directory. Debug and Release share
the same platform tree. These settings use variants, without CMake presets.

See [build architecture](architecture/build.md) for target ownership,
compiler policy, and planned platform directories, and
[architecture decisions](architecture/decisions.md) for the rationale.
