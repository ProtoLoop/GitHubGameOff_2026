# Development guide

C++ bootstrap for GitHub Game Off 2026. The current targets are macOS and
the browser through Emscripten. Windows and Linux support is planned.

The smoke test opens a resizable window and renders animated sprites and geometry
against a dark blue background. Close the native window or press Escape to quit.

## Daily workflow

After completing the [setup](#prerequisites), use VS Code's Command Palette
(**Cmd+Shift+P** on macOS):

1. Select your platform and Debug/Release configuration with **CMake: Select Variant**.
   Use the **[Unspecified]** kit for Web or Apple Clang for macOS.
2. Run **CMake: Configure** after switching platforms, then **CMake: Build**.
3. For Web, start the server from the project root and leave it running:

   ```sh
   python3 HttpServer.py
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
| Git | Cloning the project and initializing the pinned SDL and stb submodules |
| CMake 3.27 or newer | Configuring native and web builds |
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
`clang-tidy` on `PATH`.

Emscripten 5.0.7 and Node.js 22.16.0 are downloaded and configured automatically
on the first web configure. No separate SDK manager, Node installation, or npm
install is needed. That first configure needs internet access or the
[cached toolchain archives](#toolchain-cache-and-offline-builds).

Initialize the pinned SDL and stb submodules after cloning:

```sh
git submodule update --init --recursive
```

## Build structure

The root [CMakeLists.txt](../CMakeLists.txt) builds one C++ executable,
`gameoff2026`, and its static SDL dependency. CMake also enables C because SDL
contains C sources. The reasons for this structure are recorded in
[architecture decisions](architecture/decisions.md#bootstrap-platform-and-build-foundation).

Use Ninja Multi-Config. Each platform has its own configure step, CMake cache,
and compilation database. Debug and Release share the platform's build tree.
There are no presets or custom sanitizer configurations. Windows and Linux
native targets are planned but their toolchains and runtime behavior are not
validated.

| Build directory | Toolchain | Status |
| --- | --- | --- |
| `build/native/mac` | Host Apple Clang | Initial target |
| `build/web` | Pinned Emscripten, selected by root `Emscripten.cmake` | Initial target |
| `build/native/windows` | To be selected | Planned |
| `build/native/linux` | To be selected | Planned |

## Dependency ownership

`thirdparty/SDL` is a Git submodule of the official SDL repository, pinned to
release 3.4.18 at `829a65d769d935c4852f8159e964312c0957260a`. The Git entry records
the exact commit. CMake does not fetch SDL or use a system SDL.
An uninitialized submodule produces a configure error with recovery instructions.

SDL is added as a system subdirectory and linked through `SDL3::SDL3-static`.
Its tests, test library, examples, and shared library are disabled. SDL owns
selection of platform libraries and renderer backends. Bundled dependency
notices belong in [ThirdPartyLicenses.txt](../ThirdPartyLicenses.txt).

`thirdparty/stb` is a Git submodule of the official stb repository at
`https://github.com/nothings/stb.git`, pinned to
`d14d46f6b44456eb88beac58cba833a4135d4c89`. The Git entry records the exact
commit. CMake does not fetch stb or use a system installation. A missing
`stb_image.h` produces a configure error with recovery instructions.

The game uses `thirdparty/stb/stb_image.h` (v2.30) directly to decode images.
[src/stb_image.cpp](../src/stb_image.cpp) compiles its implementation once,
and the stb include directory is treated as a system include directory. The
existing stb MIT notice covers both this direct use and SDL's bundled copy.

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
Run from the repository root so texture paths resolve to the `assets/` directory.
Native texture loading strips leading slashes from asset paths and reads from disk.

## Browser

The root `Emscripten.cmake` downloads and configures pinned Emscripten 5.0.7 and
Node.js 22.16.0 for your development machine. No Emscripten/Node installation or
SDK activation is needed. Run:

```sh
cmake -S . -B build/web -G "Ninja Multi-Config" -DCMAKE_TOOLCHAIN_FILE=Emscripten.cmake
cmake --build build/web --config Debug
cmake --build build/web --config Release
python3 HttpServer.py
```

Open `http://localhost:8000/build/web/Debug/` or
`http://localhost:8000/build/web/Release/`.
Serve the generated files over HTTP rather than opening the HTML as a local
file. Each configuration produces `index.html`, `index.js`, and `index.wasm`.
Keep these files together when hosting the game, and include
`ThirdPartyLicenses.txt` with distributions.

The game requests textures over HTTP using Emscripten Fetch. The current sprite
sheet URL is `/assets/male_hero-walk.png`, relative to the server root. The
development server exposes the repository's `assets/` directory at that URL.
When deploying, publish the required assets under `/assets/` on the same origin
as the game, even if the HTML is hosted in a subdirectory. The build does not
copy assets or package them into the WebAssembly filesystem.

The bootstrap uses Emscripten's default HTML shell and has no worker threads.
Keep native and browser build trees separate. When changing toolchains, use a
fresh build directory.

When migrating an existing web build from a system Emscripten installation,
remove the generated `build/web` directory once and configure again. Normal
Debug/Release or native/web switching does not require deleting build directories.
Do not share CMake build caches between machines or filesystem locations.

### Web compiler provisioning

The root [Emscripten.cmake](../Emscripten.cmake) calls
[ProvisionEmscripten.cmake](../ProvisionEmscripten.cmake), selects the local tools,
and loads Emscripten's upstream toolchain. Provisioning owns the version pins,
official archive mappings, SHA-256 verification, extraction, and local tool
configuration. The Emscripten archive supplies LLVM/Clang, Binaryen, runtime
libraries, and JavaScript build helpers.

Archive mappings cover macOS and Linux on arm64/x64 and Windows on x64.
Other hosts fail explicitly. These web compiler hosts are independent of the
planned native platform support. Host-specific executable validation requires
the corresponding operating system.

A setup lock serializes extraction. Archives are extracted into temporary
directories before being marked complete. The generated Emscripten configuration
uses paths relative to the extracted installation. Configure and compile/link
commands explicitly select local LLVM, Binaryen, Node, Python, and the compiler
cache, independently of existing system Emscripten environment settings. Native
builds do not load this toolchain or download web tools.

See the [toolchain decision](architecture/decisions.md#project-local-web-toolchain)
for the rationale. Container validation uses external build and tool-cache
directories.

### Toolchain cache and offline builds

Downloads live in `build/toolchains/archives`. Extracted tools live in
`build/toolchains/<host>/emscripten-5.0.7-node-22.16.0`. The host distinguishes
macOS, Linux, and Windows and their CPU architectures. These files are ignored
by Git. Reconfiguring reuses the extracted tools, and deleting `build/web`
does not remove the toolchain cache.

Use `-DGAMEOFF_TOOLCHAIN_CACHE=/absolute/path` to select another cache location.
This setting is propagated into CMake compiler checks.
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

## Compiler policy and analysis

The game requires C++23 without compiler extensions. Exceptions and RTTI are
disabled for Clang, Apple Clang, and GCC. Compiler warnings and warnings-as-errors
apply to the game target. SDL keeps its own compiler policy. The root
`.clang-tidy` treats analysis diagnostics as errors, and `.clang-format` defines
formatting.

CMake exports `compile_commands.json` in each platform build directory.
After a successful configure in VS Code, CMake Tools copies the active platform's
database to `build/compile_commands.json`, matching the existing `.clangd`.
Command-line configuration does not perform this copy. Reconfigure through
CMake Tools after switching platforms to refresh editor analysis.
Multi-config databases contain entries for both configurations, so editor
analysis is not a substitute for building each configuration.

Clang-tidy runs by default on project C/C++ targets and is required when enabled.
Analysis is initialized after third-party targets and before project targets.
Add future third-party subdirectories before that setup so they do not inherit
analysis. Existing CMake caches may still
contain `ENABLE_CLANG_TIDY=OFF`, so reconfigure those with
`-DENABLE_CLANG_TIDY=ON`. To opt out, reconfigure with `-DENABLE_CLANG_TIDY=OFF`.
The CMake option's `ON` default applies to fresh caches. The web variant also
explicitly sets `ENABLE_CLANG_TIDY` to `ON`, overriding the cache on VS Code
configuration. Change that variant setting to `OFF` to opt out for that workflow.
The `src/stb_image.cpp` implementation wrapper is excluded from linting using
CMake 3.27's `SKIP_LINTING` property. Web clang-tidy receives
`--target=wasm32-unknown-emscripten`, the provisioned sysroot, and its
`include/compat` directory explicitly. It does not execute the `em++` wrapper to
discover those settings.

The editor uses clangd separately. Workspace settings allow it to query the local
`em++` driver for target and include paths. The root `.clangd` suppresses editor
diagnostics outside `src/`, including opened dependency files. If web symbols
remain unresolved after configuring, run **clangd: Restart language server** and
check **Output → clangd** for compiler-query failures.

Use `clang-format -i src/main.cpp` to apply the checked-in style.

## VS Code workflow

[Workspace settings](../.vscode/settings.json) select Ninja Multi-Config and
`build/${variant:platform}` as the build directory. Platform keys in
[variants](../.vscode/cmake-variants.json) select `native/mac` or `web`.
Debug/Release is a separate dimension and does not create another build tree.

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
The project's [HttpServer.py](../HttpServer.py) uses Python 3's standard library
to serve the repository root on `http://localhost:8000`. It sends
`Cache-Control: no-cache` on every response, requiring browsers to revalidate
cached HTML, JavaScript, WebAssembly, and assets before reusing them.

Use **CMake: Select a Kit** to select Apple Clang for macOS, and
**CMake: Select Variant** to select macOS Debug or Release. Run **CMake: Configure**,
then **CMake: Build**. Select `gameoff2026` as the CMake launch target and use the
**Game (LLDB, macOS)** launch configuration to debug. The executable path and
configuration come from the selected CMake target through
`cmake.launchTargetPath` in [launch.json](../.vscode/launch.json).

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
python3 HttpServer.py
```

Leave that terminal running between browser launches. Press **Ctrl+C** in the
terminal when you want to stop the server.

Click the **Run and Debug** icon on the left sidebar, select **Game (Browser)**
from the dropdown beside the green play button, and press **F5**. This launches
Chrome with debugging attached, using CMake's currently selected Debug or Release
configuration at `http://localhost:8000/build/web/Debug/index.html` or
`http://localhost:8000/build/web/Release/index.html`.
F5 does not build the game or start the server. Build again after code changes.
The launch expects port 8000 and the repository root as the server root, matching the
command above, with `webRoot` set to the repository root. The browser URL uses
`cmake.buildType` for the configuration. There are no pre-launch tasks or
`tasks.json`. Native LLDB launch does not debug WebAssembly.

For C++ browser breakpoints, select **Web Debug** and build. CMake's Debug flags
pass `-g` during compilation and linking, preserving DWARF debug information in
`index.wasm`. Set a breakpoint beside an executable line in `main.cpp`, then
launch **Game (Browser)** with F5. After installing the DWARF extension, restart
any active debug session. Breakpoints, stepping, and variable inspection happen
in VS Code while the game runs in Chrome. See
[VS Code's WebAssembly debugging documentation](https://code.visualstudio.com/docs/nodejs/nodejs-debugging#_debugging-webassembly).

## Runtime boundary

[main.cpp](../src/main.cpp) uses SDL's application callbacks for initialization,
events, frames, and shutdown. SDL drives the lifecycle on both native and browser
targets, allowing the browser event loop to schedule frames. Application state
owns the window and renderer, and the shutdown callback releases them before
SDL shuts itself down. Initialization and rendering failures are logged and
reported with `SDL_APP_FAILURE`. This is the SDL entry-point contract. Project
helpers use [Result](../src/Result.h) for recoverable failures and
[Diag](../src/Diag.h) for logging and invariant checks.

The smoke test uses SDL's 2D renderer with a fixed logical canvas and letterboxing.
Animation uses elapsed time rather than frame count. Vsync is requested but is
not required for startup. The browser build is single-threaded, with SDL pthread
support disabled and WebAssembly memory growth enabled.

## Texture loading

[TextureLoader](../src/TextureLoader.h) borrows a collection of
`TextureLoadRequest` objects and a renderer. `Load()` starts the requests and
`IsPending()` reports whether any browser downloads remain in progress. Browser
builds enable `-sFETCH=1` and fetch each path into memory. Callbacks record
success or failure. Native builds mark requests ready immediately and defer
disk reads until `Take()`, stripping leading slashes from the path.

For a successful request, `Take()` decodes pixels with stb_image, creates an SDL
texture, uploads the pixels, and returns an owning `TexturePtr`. Its scope guards
release decoded pixels and, on web, the completed fetch. HTTP completion does
not guarantee that decoding or texture creation will succeed. The frame loop
collects completed requests and creates sprites that borrow the resulting
textures.

Browser callbacks retain request addresses, so requests must remain at stable
addresses until their callbacks finish. The renderer must remain alive while
textures are created, used, and destroyed.

## Maintaining this guide

Update this guide when targets, dependencies, toolchains, platforms,
configurations, output paths, compiler/linker policy, application lifecycle, or
development procedures change. Record durable architectural choices and their
reasons in [decisions](architecture/decisions.md). Keep temporary review findings
out of the system description.
