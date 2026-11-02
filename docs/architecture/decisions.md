# Architecture Decisions

This document records important architectural choices when the reasons behind
them will be useful later.

Add a decision when it affects several parts of the system, limits future
choices, or is likely to be questioned again after the original context has
been forgotten. Routine implementation choices belong in the code or in a
pattern.

Each decision should include:

- a title, date, and status.
- the situation that required a choice.
- what we decided.
- the alternatives we considered.
- the good and bad consequences.
- links to related principles, patterns, and older decisions.

An accepted decision is part of the project's history. If the decision changes,
add a new entry and mark the old one as superseded instead of rewriting it.

## Bootstrap platform and build foundation

Date: 2026-11-02. Status: Accepted.

The game is primarily browser-based C++, with native development on macOS and
future Windows/Linux support. It needs a small build that produces consistent
SDL behavior across toolchains without bringing in an engine framework.

We use CMake with Ninja Multi-Config and separate platform build trees. Debug
and Release share a platform tree. Commands are documented explicitly rather
than stored in presets, matching the project's existing workflow. Separate
configuration trees were considered, but would duplicate platform setup.

SDL supplies platform services and the initial 2D renderer. It is built statically
from the official Git submodule, pinned to release 3.4.18. This makes dependency
selection explicit and avoids requiring a matching system SDL installation or
shipping a separate SDL dynamic library. It adds submodule initialization and
dependency compilation to the development workflow. Dependency updates require
an intentional Git pointer change and a notice review.

The application uses SDL callbacks instead of a blocking main loop. SDL schedules
the same lifecycle on native and browser targets, satisfying the browser's need
to regain control between frames. A platform-specific Emscripten loop was an
alternative, but would duplicate lifecycle wiring in the bootstrap. Browser
threading is deferred until there is a concrete need.

These choices follow [thin abstractions and clear ownership](principles.md).
The current structure and limitations are described in [build architecture](build.md).
