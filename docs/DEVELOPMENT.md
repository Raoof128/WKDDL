# Development Guide

This document explains how to work on the Windows Kernel Driver Detection Lab with a repeatable toolchain, formatting rules, and validation steps. The project is designed for **lab use inside virtual machines only**.

## Tooling and Environments
- **Dev Container (recommended):** Open the repository in VS Code and use the included `.devcontainer/devcontainer.json`. It provisions a C++ toolchain with CMake, Ninja, and clang-format. The container runs `cmake -S . -B build -DENABLE_HOST_TESTS=ON` on creation.
- **Windows host:** Install Visual Studio 2022 with the matching WDK for driver builds. Run commands from a Developer Command Prompt so `WDK_INCLUDE_PATH` and `WDK_LIB_PATH` are set.
- **Linux/macOS host:** Portable tests and documentation builds work without the WDK. Driver compilation is skipped automatically when the environment is missing the required SDKs.

## Formatting and Linting
- **clang-format:** Enforced via CI; run `clang-format -i $(git ls-files "*.c" "*.cpp" "*.h")` before committing.
- **EditorConfig:** See `.editorconfig` for whitespace, encoding, and indentation rules.
- **Warnings as errors:** Both driver and client targets compile with `/W4 /WX` (or `-Wall -Wextra -Wpedantic -Werror` on non-MSVC builds) to prevent regressions.

## Build Targets
- **Portable tests (default in CI):**
  ```bash
  cmake -S . -B build -DENABLE_HOST_TESTS=ON
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
- **User-mode client (Windows):**
  ```powershell
  cmake -S . -B build -DENABLE_CLIENT=ON -DENABLE_HOST_TESTS=OFF
  cmake --build build --config Release
  ```
- **Kernel driver (Windows with WDK):**
  ```powershell
  set WDK_INCLUDE_PATH=C:\Program Files (x86)\Windows Kits\10\Include\<version>\km
  set WDK_LIB_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\<version>\km\x64
  cmake -S . -B build -DENABLE_DRIVER=ON -DENABLE_CLIENT=ON -DENABLE_HOST_TESTS=OFF
  cmake --build build --config Release
  ```

## Testing Expectations
- **Host-side:** Keep `ctest` passing to guarantee IOCTL layout stability and naming conventions for device paths.
- **Windows integration:** Manually validate IOCTL responses, signature verification, and colorized output in a VM. Always snapshot before loading unsigned drivers and remove the driver after testing.

## Contribution Checklist
1. Write or update documentation alongside code changes.
2. Keep driver code defensive: buffered I/O, explicit size checks, and minimal pointer arithmetic.
3. Use RAII and explicit error handling in user-mode code.
4. Run `clang-format` and `ctest` before submitting a PR.
5. Describe testing results and any risks in the PR body.

## Security Hygiene
- Never commit signing keys or certificates.
- Rotate test certificates regularly and remove them from build artifacts.
- Treat driver crashes as security-relevant until root-caused; capture WinDbg traces when possible.
