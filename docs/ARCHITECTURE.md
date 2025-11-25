# Architecture Overview

This lab separates kernel-mode data collection from user-mode analysis to reduce risk and simplify debugging.

## Components
- **Kernel Driver (`ModuleInspector.sys`)**
  - Initializes with `DriverEntry` and exposes a device at `\\Device\\ModuleInspector` with a DOS link `\\\\.\\ModuleInspector`.
  - Handles `IRP_MJ_DEVICE_CONTROL` for `IOCTL_GET_MODULE_LIST` and uses buffered I/O to copy data safely between user and kernel space.
  - Calls `AuxKlibQueryModuleInformation` twice: first to determine the required buffer size, and again to populate the module array.
  - Returns a `MODULE_LIST_PAYLOAD` that starts with a count followed by `AUX_MODULE_EXTENDED_INFO` entries containing base address, name offsets, and NT path.
  - Uses a private pool tag and zeroes buffers before use to limit information leakage.

- **User-Mode Client (`ModuleInspector.exe`)**
  - Opens the driver via `CreateFile` using the DOS symbolic link.
  - Issues `IOCTL_GET_MODULE_LIST` to fetch kernel module metadata.
  - Converts NT-style paths to wide strings and uses `WinVerifyTrust` to validate digital signatures.
  - Prints a table with color-coded rows (green for signed, red for unsigned) to flag suspicious modules quickly.

## Data Flow
1. User mode sends `IOCTL_GET_MODULE_LIST` with a sufficiently sized output buffer.
2. The driver queries the kernel for module information using `AuxKlibQueryModuleInformation`.
3. The driver copies the module metadata into the IOCTL's system buffer and returns the byte count.
4. The user-mode client parses the payload, performs signature verification for each module path, and renders results in the console.

## Safety Considerations
- **Buffered I/O** reduces the chance of dereferencing user pointers in kernel mode.
- **Size Checks** ensure the driver never writes beyond the provided buffer and reports the required size to callers.
- **Test-Signing Only**: The driver is intended for lab environments; enable `bcdedit /set testsigning on` before loading and revert after testing.
- **VM Isolation**: Always run the lab inside a VM so that misbehaving drivers or crashes do not impact host stability.

