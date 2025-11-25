#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#elif defined(_WIN32)
#include <Windows.h>
#include <winioctl.h>
#else
// Minimal type definitions to allow host-side tooling (unit tests, static
// analysis) to build on non-Windows environments. The concrete values mirror
// the Windows SDK to keep the IOCTL layout stable across platforms.
#include <stdint.h>

typedef void* PVOID;
typedef uint32_t ULONG;
typedef uint16_t USHORT;
typedef uint8_t UCHAR;
typedef uintptr_t ULONG_PTR;

#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif

#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif

#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0x0000
#endif

#ifndef FILE_READ_DATA
#define FILE_READ_DATA 0x0001
#endif

#ifndef FILE_WRITE_DATA
#define FILE_WRITE_DATA 0x0002
#endif

#ifndef CTL_CODE
#define CTL_CODE(DeviceType, Function, Method, Access)                                                                 \
    ((ULONG)((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif

#endif

// Define AUX_MODULE_EXTENDED_INFO manually so that user-mode code does not need
// kernel headers. The layout matches the WDK definition to ensure the payload
// shared between kernel and user mode is identical.
typedef struct _AUX_MODULE_BASIC_INFO
{
    PVOID ImageBase;
    ULONG ImageSize;
} AUX_MODULE_BASIC_INFO, *PAUX_MODULE_BASIC_INFO;

typedef struct _AUX_MODULE_EXTENDED_INFO
{
    AUX_MODULE_BASIC_INFO BasicInfo;
    ULONG ImageSize;         // duplicate of BasicInfo.ImageSize in newer WDKs
    USHORT FileNameOffset;   // offset into FullPathName
    UCHAR FullPathName[512]; // NT-style path string
} AUX_MODULE_EXTENDED_INFO, *PAUX_MODULE_EXTENDED_INFO;

// Shared IOCTL code for fetching kernel module information.
// METHOD_BUFFERED is used for simplicity and safety; the kernel copies data
// between user buffers and a system buffer, reducing the likelihood of
// invalid pointer dereferences in kernel mode.
#define IOCTL_GET_MODULE_LIST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

// Payload format returned by the driver. The Count field indicates how many
// AUX_MODULE_EXTENDED_INFO entries follow directly in memory. The user-mode
// client allocates a buffer large enough to hold Count entries.
typedef struct _MODULE_LIST_PAYLOAD
{
    ULONG Count;
    // Flexible array member; actual size is Count entries.
    // Each entry includes the image name, full path, and base address.
    AUX_MODULE_EXTENDED_INFO Modules[1];
} MODULE_LIST_PAYLOAD, *PMODULE_LIST_PAYLOAD;

// Symbolic link used by both kernel-mode and user-mode components.
#define MODULE_DEVICE_NAME L"\\\\.\\ModuleInspector"
#define MODULE_NT_DEVICE_NAME L"\\Device\\ModuleInspector"
#define MODULE_DOS_DEVICE_NAME L"\\DosDevices\\ModuleInspector"
