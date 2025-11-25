# Driver/Client API Reference

This document describes the contract between the kernel driver (`ModuleInspector.sys`) and the user-mode client (`ModuleInspector.exe`). The intent is to keep the binary layout stable for defensive tooling and automated tests.

## Device Names
- NT device name: `\\Device\\ModuleInspector`
- DOS symbolic link: `\\\\.\\ModuleInspector`

## IOCTLs

### `IOCTL_GET_MODULE_LIST`
- **Code:** `CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)`
- **Purpose:** Returns the list of loaded kernel modules to user mode.
- **I/O Method:** `METHOD_BUFFERED` — the I/O manager creates a system buffer and copies user input/output to reduce pointer risk.
- **Input Buffer:** None.
- **Output Buffer:** Caller-provided buffer receiving a `MODULE_LIST_PAYLOAD`.

#### Output Payload (`MODULE_LIST_PAYLOAD`)
```
typedef struct _MODULE_LIST_PAYLOAD {
    ULONG Count;                   // Number of entries in Modules
    AUX_MODULE_EXTENDED_INFO Modules[1]; // Flexible array of module entries
} MODULE_LIST_PAYLOAD, *PMODULE_LIST_PAYLOAD;
```

Each `AUX_MODULE_EXTENDED_INFO` entry contains:
- `PVOID ImageBase` – base address of the loaded image.
- `ULONG ImageSize` – size in bytes.
- `USHORT FileNameOffset` – offset into `FullPathName` where the file name begins.
- `UCHAR FullPathName[512]` – NT-style path of the image.

### Return Semantics
- On success, `IoStatus.Information` is set to the number of bytes written.
- If the output buffer is too small, the driver returns `STATUS_BUFFER_TOO_SMALL` and writes the required byte count to `IoStatus.Information`. The caller should resize the buffer accordingly and retry.

## Versioning Guidance
- The IOCTL function code (`0x800`) and structure layouts are treated as stable. Additive changes should introduce new IOCTL codes to preserve compatibility.
- Keep the flexible array member at the end of the payload to avoid ABI breaks.
