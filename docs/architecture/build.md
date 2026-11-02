# Build architecture

The root [CMakeLists.txt](../../CMakeLists.txt) builds one C++ executable,
`gameoff2026`, and its static SDL dependency. CMake also enables C because SDL
contains C sources. [Build and run commands](../../README.md) live in the root
README. The reasons for the structure are recorded in
[decisions](decisions.md#bootstrap-platform-and-build-foundation).

## Platforms and configurations

macOS and Emscripten are the initial supported targets. Windows and Linux are
planned, but their toolchain policies and runtime behavior are not validated.

Use Ninja Multi-Config. Each platform gets its own configure step, CMake cache,
and compilation database. Debug and Release share that platform's build tree.
Select the configuration with `cmake --build ... --config Debug` or `Release`.
There are no presets or custom sanitizer configurations in the bootstrap.

| Build directory | Toolchain | Status |
| --- | --- | --- |
| `build/native/mac` | Host Apple Clang | Initial target |
| `build/web` | Emscripten, selected by `emcmake` | Initial target |
| `build/native/windows` | To be selected | Planned |
| `build/native/linux` | To be selected | Planned |

The native executable goes into the configuration subdirectory, for example
`build/native/mac/Debug/gameoff2026`. macOS builds target the host architecture
and do not produce an app bundle. Browser output goes into
`build/web/Debug` or `build/web/Release`, containing `index.html`, `index.js`,
and `index.wasm`. Emscripten supplies the HTML shell. There is no asset pipeline.

## Dependency ownership

`thirdparty/SDL` is a Git submodule of the official SDL repository, pinned to
release 3.4.18 at `829a65d769d935c4852f8159e964312c0957260a`. The Git entry records
the exact commit. CMake does not fetch dependencies or use a system SDL.
An uninitialized submodule produces a configure error with recovery instructions.

SDL is added as a system subdirectory and linked through `SDL3::SDL3-static`.
Its tests, test library, examples, and shared library are disabled. SDL owns
selection of platform libraries and renderer backends. Bundled dependency
notices belong in [ThirdPartyLicenses.txt](../../ThirdPartyLicenses.txt).

## Compiler and tooling policy

The game requires C++23 without compiler extensions. Exceptions and RTTI are
disabled for Clang, Apple Clang, and GCC. Warning options and warnings-as-errors
apply to the game target. SDL keeps its own compiler policy.

`ENABLE_CLANG_TIDY` defaults to `ON` and requires clang-tidy when enabled.
It initializes C and C++ analysis after third-party targets have been added and
before project targets are created. Add future third-party subdirectories before
this setup so they do not inherit analysis. Project targets use the root
`.clang-tidy`, which treats diagnostics as errors. The option remains available
for opting out. Existing caches retain their value until explicitly reconfigured.
Formatting uses the root `.clang-format`. CMake exports a
compilation database in each platform build directory. Multi-config databases
contain commands for multiple configurations.

## VS Code integration

[Workspace settings](../../.vscode/settings.json) select Ninja Multi-Config and
use `build/${variant:platform}` as the build directory. The platform choice keys
in [variants](../../.vscode/cmake-variants.json) supply `native/mac` or `web`.
Debug and Release are a separate variant dimension, so selecting a configuration
does not create another build tree. Platform paths are centralized in the
variant choices, and presets are disabled for this editor workflow.

For macOS, the user selects an Apple Clang kit. For web builds, the user selects
the Unspecified kit and the variant supplies the toolchain from the activated
SDK's `EMSDK` environment variable. This prevents a native compiler kit from
overriding Emscripten. Web variants explicitly set `ENABLE_CLANG_TIDY` to `ON`.
This setting overrides the CMake default and cached value on configuration, and
can be changed to `OFF` independently of the CMake option's default.

After successful configuration, CMake Tools copies the platform compilation
database to `build/compile_commands.json`. This matches the existing `.clangd`
without selecting a platform there. The copy represents the last platform
configured through CMake Tools. Command-line configuration does not refresh it.

The [macOS CodeLLDB launch](../../.vscode/launch.json) resolves the executable
through `cmake.launchTargetPath`, using the selected CMake target and configuration.
Live Preview serves `build/web`, allowing either configuration to be opened in
an external browser. Extension prerequisites and operating steps are in the
[README](../../README.md#vs-code-workflow).

## Runtime boundary

[main.cpp](../../main.cpp) uses SDL's application callbacks for initialization,
events, frames, and shutdown. SDL drives the lifecycle on both native and browser
targets, allowing the browser event loop to schedule frames. Application state
owns the window and renderer, and the shutdown callback releases them before
SDL shuts itself down. Initialization and rendering failures are logged and
reported with `SDL_APP_FAILURE`. This is the SDL entry-point contract. The
project's documented `Result` helpers have not yet been introduced.

The smoke test uses SDL's 2D renderer with a fixed logical canvas and letterboxing.
Animation uses elapsed time rather than frame count. Vsync is requested but is
not required for startup. The browser build is single-threaded, with SDL pthread
support disabled and WebAssembly memory growth enabled.

## Maintaining this document

Check this description when changing targets, dependencies, toolchains, platforms,
configurations, output paths, compiler/linker policy, or application lifecycle.
Keep runnable instructions in the root README synchronized. Record durable
architectural changes and their reasons in decisions, rather than accumulating
implementation history here.
