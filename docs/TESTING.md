# Testing Strategy

Because kernel driver compilation and signing require a Windows environment, the repository provides two test layers:

1. **Host-side layout tests (portable):** A small C++ test target validates that shared data structures (`MODULE_LIST_PAYLOAD` and `AUX_MODULE_EXTENDED_INFO`) remain ABI-compatible across platforms. These tests run in CI on Linux to catch accidental struct changes.
2. **Windows integration tests (manual):** Build and run the driver and client inside a Windows 10/11 VM with test-signing enabled. Validate IOCTL round-trips and signature verification using both signed and unsigned drivers.

## Running Portable Tests (Linux/macOS)
```bash
cmake -S . -B build -DENABLE_HOST_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Manual Windows Validation Checklist
1. Enable test signing and reboot: `bcdedit /set testsigning on && shutdown /r /t 0`.
2. Build `ModuleInspector.sys` and `ModuleInspector.exe` from Visual Studio 2022 with the matching WDK.
3. Register the driver service: `sc create ModuleInspector type= kernel binPath= C:\\Path\\ModuleInspector.sys start= demand`.
4. Start the service and run the client: `sc start ModuleInspector` then `ModuleInspector.exe`.
5. Confirm the client prints a module table and highlights any unsigned drivers in red.
6. Stop and delete the service when finished: `sc stop ModuleInspector && sc delete ModuleInspector`.

> Always snapshot the VM before running unsigned or experimental drivers.
