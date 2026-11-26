# Development guide

Mosaic has a separate browser build. See its [user guide](../Mosaic/README.md).
CMake provisions Deno 2.9.8 and bundles the TypeScript application into
`Mosaic/build/dist`. Python 3.9 or newer runs the server and server tests.
The app has no third-party runtime JavaScript dependencies. The distribution
includes `ThirdPartyLicenses.txt`.

Run `cmake --build Mosaic/build --target check` for the application and HTTP
server tests. Start the app with
`python3 Mosaic/server.py --projects-root projects` from the repository root and visit
`http://localhost:8000/`. The server serves the built app at `/` and ordinary
project directories at the HTTP route `/projects/`. The default filesystem location
is `projects/` at the repository root, alongside `Mosaic/`, resolved relative to
the server script regardless of the working directory. `--projects-root` overrides
that location. It accepts PUT only for a project's
`level.json`. Asset changes need Reload Project, with no build required.
See the [file formats](../Mosaic/docs/FILE_FORMATS.md) for the saved level format
and rendering rules.

C++ bootstrap for GitHub Game Off 2026. The current targets are macOS and
the browser through Emscripten. Windows and Linux support is planned.

## Daily workflow

After completing the [setup](#prerequisites), use VS Code's Command Palette:

1. Select your platform and Debug/Release configuration with **CMake: Select Variant**.
   Select the kit described in the [platform-specific editor steps](#configure-build-and-debug).
2. Run **CMake: Configure** after switching platforms, then **CMake: Build**.
3. For Web, start the server from the project root and leave it running:

   ```sh
   python3 HttpServer.py
   ```

4. In Run and Debug, select the launch configuration for your target, then press
   **F5**. Use Web Debug for C++ browser breakpoints.

After code changes, build again and restart the debug session. F5 does not build
the game. The browser server can stay running between launches.

## Prerequisites

Install the tools needed for your workflow and make command-line tools available
on `PATH`, including in VS Code's environment.

| Tool | Used for |
| --- | --- |
| Git | Cloning the project and initializing the pinned SDL, SDL_mixer, stb, jsmn, Dawn, and GLM submodules |
| CMake 3.27 or newer | Configuring native and web builds |
| Ninja | Building with Ninja Multi-Config |
| Python 3 | Running the web compiler and serving browser builds locally |
| clang-tidy | Required by default for project build-time analysis, compatible with the checked-in configuration and selected compiler |
| clangd | C++ editor analysis through the VS Code clangd extension |
| clang-format 21 or newer | Optional command-line formatting with the checked-in configuration |
| Google Chrome | Running the configured browser debugging session |
| Visual Studio Code | Using the checked-in editor and F5 launch configurations |

Native builds also need the compiler described in the platform setup below.
The clangd extension can help install its language-server executable if one is
not already available.
Build-time clang-tidy still needs its own executable: CMake looks for
`clang-tidy` on `PATH`.

Emscripten 5.0.7 and Node.js 22.16.0 are downloaded and configured automatically
on the first web configure. No separate SDK manager, Node installation, or npm
install is needed. That first configure needs internet access or the
[cached toolchain archives](#toolchain-cache-and-offline-builds).

Initialize the pinned submodules after cloning:

```sh
git submodule update --init --recursive
```

## Build structure

The root [CMakeLists.txt](../CMakeLists.txt) builds one C++ executable,
`gameoff2026`, with static SDL, SDL_mixer, and native Dawn dependencies. CMake
also enables C because SDL contains C sources. The reasons for this structure are recorded in
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

`thirdparty/SDL_mixer` is a Git submodule of the official SDL_mixer repository at
`https://github.com/libsdl-org/SDL_mixer.git`, pinned to release `3.2.4` at
`72a81869b45e249e8e67102db4e98dd2441f05a1`. Initialize it with the other submodules.
A missing checkout produces a configure error with recovery instructions.
SDL_mixer requires SDL 3.4.0 or newer, satisfied by the pinned SDL version.

Native and browser builds link `SDL3_mixer::SDL3_mixer-static`. Enabled formats
are WAV, MP3 through bundled `dr_mp3`, and Ogg Vorbis through bundled
`stb_vorbis`. Other optional formats and alternate decoder libraries are disabled.
These decoders require no external codec downloads or system codec packages.
Vendored dependency selection and strict dependency checks are enabled, and
codec dynamic loading is disabled. Tests, examples, installation rules, and
upstream warnings-as-errors are disabled. SDL_mixer is added before project
clang-tidy setup. Its bundled-code notices, including minimp3 within `dr_mp3`,
are included in [ThirdPartyLicenses.txt](../ThirdPartyLicenses.txt).

`thirdparty/stb` is a Git submodule of the official stb repository at
`https://github.com/nothings/stb.git`, pinned to
`d14d46f6b44456eb88beac58cba833a4135d4c89`. The Git entry records the exact
commit. CMake does not fetch stb or use a system installation. A missing
`stb_image.h` produces a configure error with recovery instructions.

The game uses `thirdparty/stb/stb_image.h` (v2.30) directly to decode images.
[src/stb_image.cpp](../src/stb_image.cpp) compiles its implementation once,
and the stb include directory is treated as a system include directory. The
existing stb MIT notice covers both this direct use and SDL's bundled copy.

`thirdparty/jsmn` is a Git submodule of the official jsmn repository at
`https://github.com/zserge/jsmn.git`, pinned to
`25647e692c7906b96ffd2b05ca54c097948e879c`. The Git entry records the exact
commit. CMake does not fetch jsmn or use a system installation. Initialize it
with `git submodule update --init --recursive`. A missing `jsmn.h` produces a
configure error with those recovery instructions.

The game uses `jsmn.h` directly for JSON tokenization, and the jsmn include
directory is treated as a system include directory. jsmn has no third-party
runtime dependencies. Its MIT notice is included in
[ThirdPartyLicenses.txt](../ThirdPartyLicenses.txt).

`thirdparty/dawn` is a Git submodule of `https://github.com/google/dawn.git`,
pinned to release `v20261106.204713` at
`797946823414be58054116de563d9ec4636dcb2e`. Initialize it with the other submodules.
CMake fetches Dawn's dependencies at the revisions in its `DEPS` file on the
first configure of a build directory and when the checked-out Dawn commit changes.
The completed revision is stored as `DAWN_FETCHED_REVISION` in the CMake cache.
Other configures skip the dependency fetch script. Deleting the build directory
clears the cached revision and triggers fetching again. A fetch needs network
access when required dependencies are not already available locally. Native builds link static
`dawn::webgpu_dawn`. Browser builds use `emdawnwebgpu_cpp`. Bundled notices are
included in [ThirdPartyLicenses.txt](../ThirdPartyLicenses.txt).

`thirdparty/glm` is a Git submodule of `https://github.com/g-truc/glm.git`,
pinned to release `1.0.3` at `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed`.
Initialize it with the other submodules. A missing GLM checkout produces a
configure error with recovery instructions.

Native and browser builds use `glm::glm-header-only`, with system include
directories and no compiled GLM library. GLM tests and installation rules are
disabled. The game retains its C++23 settings and uses GLM's default configuration
macros. This target introduces no additional linked dependencies. GLM's MIT
notice is included in [ThirdPartyLicenses.txt](../ThirdPartyLicenses.txt).

## Asset locations

Native builds read assets from disk relative to the working directory, stripping
leading slashes from asset paths. Run from `projects/project1` so level and
asset paths resolve within that project. The native debugger uses this working
directory automatically.

Browser builds load assets over HTTP. The development server serves `/build/`
from the repository root and all other paths from `projects/project1`.
Requests for `/level.json`, `/assets/`, and `/audio/` therefore resolve within
the project directory. Both filesystem roots are relative to the server script,
independent of the working directory used to start it.

When deploying, publish the project's level and assets at their root-relative
URLs on the same origin as the game, even if the HTML is hosted in a
subdirectory. The build does not copy assets or package them into the
WebAssembly filesystem.

Sprite sequence names must be unique within a level’s catalog. The C++ loader
uses explicit `name` values, falling back to the texture filename without its
directory or final extension. Distinct names may share a texture.

Frame dimensions use JSON `size` and C++ `SpriteFrameSequenceDef::Size`.
Omission uses the resolved source rectangle’s dimensions. The loader reads
RGBA `tint` components in `[0, 1]`. Omitted, `null`, or empty-object tints
default to white.

Mosaic resolves texture paths against each project's own directory. Explicit
sequence textures can live anywhere under its `assets/`, while automatic prop
discovery stays under `assets/props/`. Mosaic version 25 stores named `sequences`
and `sprites`, with props referencing sprites by name. Each sprite groups ordered
facing sequences, and the editor displays frame zero of its first sequence.
The C++ level loader also reads `sequences`, `sprites`, and named sprite references
in `props`. Sprites reference sequences from the level’s shared catalog. Decorative
props currently display the first frame of the first sequence, with the prop’s
transform applied.
Project2 contains 48 sequences grouped into 27 sprites, with its referenced
actor textures present in the project.
No texture copying or fallback to repository assets happens automatically.

## macOS

Native macOS builds require Xcode Command Line Tools with a C++23-capable Apple
Clang. Install them with `xcode-select --install`. Homebrew users can install
Chrome for browser debugging with `brew install --cask google-chrome`.

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
python3 HttpServer.py
```

Open `http://localhost:8000/build/web/Debug/` or
`http://localhost:8000/build/web/Release/`.
Serve the generated files over HTTP rather than opening the HTML as a local
file. Each configuration produces `index.html`, `index.js`, and `index.wasm`.
Keep these files together when hosting the game, and include
`ThirdPartyLicenses.txt` with distributions.

Publish the required assets as described in [Asset locations](#asset-locations).

The browser build uses the custom [HTML shell](../web/index.html) and displays
rendering in its WebGPU canvas. It has no worker threads.

Browser builds link Emscripten’s Embind library with `-lembind`. `WebGpu.cpp`
uses `emscripten::val` to call `navigator.gpu.getPreferredCanvasFormat()` when
selecting the canvas format. CMake supplies this link option only for Emscripten
builds. No additional installation is required.

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

Install these extensions from VS Code's Extensions view.
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
to serve build output and project content on `http://localhost:8000`, using the
routing described in [Asset locations](#asset-locations). It sends
`Cache-Control: no-cache` on every response, requiring browsers to revalidate
cached HTML, JavaScript, WebAssembly, and assets before reusing them.

### Configure, build, and debug

Use **CMake: Select a Kit** and **CMake: Select Variant** to choose the kit and
configuration for your target as described below. Run **CMake: Configure**, then
**CMake: Build**. Select the corresponding launch configuration in Run and Debug
and press **F5**.

#### Native macOS

Select the Apple Clang kit and a macOS Debug or Release variant. Select
`gameoff2026` as the CMake launch target and use the
**Game (LLDB, macOS)** launch configuration to debug. The executable path and
configuration come from the selected CMake target through
`cmake.launchTargetPath` in [launch.json](../.vscode/launch.json).

#### Browser

For web builds, VS Code needs CMake, Ninja, Python, and clang-tidy available.
The web variant selects the root `Emscripten.cmake`, which manages the local
toolchain cache. It does not use `EMSDK` or require an activated Emscripten shell.

Select the **[Unspecified]** kit and a Web Debug or Release variant, then configure
and build. The variant supplies the Emscripten toolchain, so a native compiler
kit must not override it. Web variants explicitly enable build-time clang-tidy.
When switching targets, select the kit and variant for the new target before
configuring and building.

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
The launch expects port 8000 and build output under `/build/`, matching the
server routing above. `webRoot` remains set to the repository root for debugger
source mapping. The browser URL uses
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

SDL handles windows, input, and application callbacks on native and browser
targets. WebGPU handles rendering and presentation, and SDL_mixer provides
audio playback on both targets. `App` manages the SDL lifecycle, initializes
WebGPU asynchronously, and creates the audio player before loading the level.
When loading completes, it transfers the level, renderer, and audio player to
`Game`.

Level startup separates parsing from resource loading. `LevelParser` fetches the
level file and returns a `ParsedLevel` that owns the parsed definitions. Its
`LevelDef` view borrows that storage. `LoadingState` retains the parsed data while
`LevelLoader` validates resource references and builds the level. `Game::Create`
then uses the retained placement definitions to spawn initial props and actors.
The parsed definitions are released when the loading state is replaced by the
created game.

`Game` owns the level, renderer, audio player, actors, and props. Entities
own their transforms and animation playback state. `Level` owns the cell map,
shared sequence catalog, sprites, and audio clips. Sprites
borrow sequences from the catalog. Gameplay actors and decorative props hold
`Sprite::Instance` values that borrow level-owned sprites.

Actors and props are destroyed before the level resources they borrow. Within
the level, sprites are destroyed before sequences.

Mosaic writes version 30 with explicit `propTypes` and `actorTypes` catalogs. The
C++ loader parses both catalogs and their placements. Each placement's `type`
field identifies an entry in the corresponding catalog. `Game::Create` resolves
these definitions and applies each placement's position and rotation. Both Mosaic
and the C++ runtime use the required type scale unless an optional absolute instance
scale override is present. Sprite sequences no longer own scale.

Actor definitions identify their runtime implementation through `class`. The
game currently supports `Tombstone`. Unsupported classes are logged and their
placements skipped, allowing startup to continue. Missing type definitions or
failures to create supported actors cause startup to fail.

Gameplay actors use the same level-defined sprite catalog as decorative props.
They resolve required resources from the level and request audio playback
through `AudioPlayer`. Startup fails when a required resource cannot be resolved.

`AudioPlayer` owns the mixer and a reusable pool of playback tracks. Its teardown
releases tracks before destroying the mixer, then balances mixer initialization
with `MIX_Quit()`. Final SDL_mixer shutdown also releases any audio resources
still registered with the library. In the current `Game` member order, that
happens before the level's clip wrappers are destroyed.

Each frame, `Game` accumulates elapsed time and advances entities in fixed
1/60-second simulation steps. A frame can run zero or more steps. Elapsed time
contributed by one frame is capped at 0.25 seconds, discarding excess time
after long pauses to bound catch-up work. Any remainder smaller than one step
carries into the next frame.

After simulation, `Game` updates the camera, queues entity sprites, and advances
audio playback bookkeeping once per frame to return finished tracks to the pool.
`App` then calls the renderer and presents the frame.

The ownership choices are recorded in
[Entity-owned state and sprite submission](architecture/decisions.md#entity-owned-state-and-sprite-submission),
[Game-owned props and initial population](architecture/decisions.md#game-owned-props-and-initial-population),
and [Level-owned audio resources and game-owned playback](architecture/decisions.md#level-owned-audio-resources-and-game-owned-playback).

Rendering uses an 800×600 logical canvas, scaled to fit the window while
preserving its aspect ratio. Unused space forms black borders.

The browser build is single-threaded, with SDL pthread support disabled and
WebAssembly memory growth enabled.

## Maintaining this guide

Keep this guide focused on building, running, debugging, and deploying the
applications. Document API contracts beside their declarations and shared
implementation patterns in the [architecture documentation](architecture/patterns.md).

Update this guide when targets, dependencies, toolchains, platforms,
configurations, output paths, compiler/linker policy, application lifecycle, or
development procedures change. Record durable architectural choices and their
reasons in [decisions](architecture/decisions.md). Keep temporary review findings
out of the system description.
