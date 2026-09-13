# Core and platform foundation

This document describes the ownership and lifecycle rules introduced by Phase 1A.

## Core contracts

The runtime uses fixed-width integer aliases and `Status` values instead of exceptions for expected failures. `Status` carries an `ErrorCode`; successful operations return `ErrorCode::none`.

The core does not create a custom allocator in this phase. Allocator policies will be added only after a concrete allocation workload is measured.

Diagnostics write explicit messages to `stderr`. Logging is intended for setup, failure and development paths, not for per-frame hot paths.

`Clock` uses `std::chrono::steady_clock`. Its `tick()` operation does not allocate and clamps a frame delta to 0.25 seconds so a pause or debugger break does not create an uncontrolled simulation step.

## Platform ownership

`Platform` is non-copyable and owns the native display connection and the single initial window. Window destruction happens before the display connection is closed. `shutdown()` is safe to call repeatedly and clears input and window state.

The shared platform header contains only engine types and opaque native handles. Xlib headers and X11-specific event translation remain in the Linux backend source.

The Linux backend is selected by CMake and links Xlib privately through `gameengine_platform`. Non-Linux builds use an unsupported-platform stub so the core and runtime remain compilable while the Win32 backend is developed separately.

## Events and input

Platform events are value types with no dynamic storage. The backend updates `InputState` before invoking the optional callback. The initial input contract contains raw key, mouse-button, mouse-motion, resize and quit events; action mapping is intentionally outside this phase.
