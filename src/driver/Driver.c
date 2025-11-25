#include "Common.h"
#include <aux_klib.h>
#include <ntddk.h>
#include <ntintsafe.h>
#include <ntstrsafe.h>

#define MODULE_POOL_TAG 'dLmM' // MMld
#define MODULE_LOG_PREFIX "[ModuleInspector] "

static VOID LogInfo(_In_z_ PCSTR format, ...)
{
    va_list args;
    va_start(args, format);
    vDbgPrintExWithPrefix(MODULE_LOG_PREFIX, DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, format, args);
    va_end(args);
}

DRIVER_UNLOAD ModuleInspectorUnload;
DRIVER_DISPATCH ModuleInspectorCreateClose;
DRIVER_DISPATCH ModuleInspectorDeviceControl;

//
// CompleteRequest
//
// Centralized helper that stamps the status and information bytes on an IRP
// before completing it. Keeps all exit paths consistent.
//
static VOID CompleteRequest(_Inout_ PIRP Irp, _In_ NTSTATUS Status, _In_ ULONG_PTR Information)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

//
// CreateDevice
//
// Configures the device object and DOS symbolic link used for user-mode
// communication. The device is flagged for buffered I/O to limit exposure to
// user-mode pointers.
//
static NTSTATUS CreateDevice(_In_ PDRIVER_OBJECT DriverObject, _Outptr_ PDEVICE_OBJECT* DeviceObject)
{
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(MODULE_NT_DEVICE_NAME);
    UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(MODULE_DOS_DEVICE_NAME);
    NTSTATUS status =
        IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, DeviceObject);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    (*DeviceObject)->Flags |= DO_BUFFERED_IO;

    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status))
    {
        IoDeleteDevice(*DeviceObject);
        *DeviceObject = NULL;
        return status;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = AuxKlibInitialize();
    if (!NT_SUCCESS(status))
    {
        LogInfo("AuxKlibInitialize failed: 0x%X\n", status);
        return status;
    }

    PDEVICE_OBJECT deviceObject = NULL;
    status = CreateDevice(DriverObject, &deviceObject);
    if (!NT_SUCCESS(status))
    {
        LogInfo("CreateDevice failed: 0x%X\n", status);
        return status;
    }

    DriverObject->DriverUnload = ModuleInspectorUnload;

    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        DriverObject->MajorFunction[i] = ModuleInspectorCreateClose;
    }
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ModuleInspectorDeviceControl;

    LogInfo("Driver initialized successfully.\n");
    return STATUS_SUCCESS;
}

//
// ModuleInspectorUnload
//
// Cleans up the DOS symbolic link and device object. Kept deliberately simple
// to minimize teardown edge cases.
//
_Use_decl_annotations_ VOID ModuleInspectorUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symbolicLink = RTL_CONSTANT_STRING(MODULE_DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&symbolicLink);

    if (DriverObject->DeviceObject != NULL)
    {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
}

//
// ModuleInspectorCreateClose
//
// Handles create/close requests by immediately completing the IRP. This driver
// does not require per-handle state.
//
_Use_decl_annotations_ NTSTATUS ModuleInspectorCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    CompleteRequest(Irp, STATUS_SUCCESS, 0);
    return STATUS_SUCCESS;
}

static NTSTATUS CalculatePayloadSize(_In_ ULONG ModuleCount, _Out_ SIZE_T* BytesNeeded)
{
    SIZE_T calculated = 0;
    NTSTATUS status = RtlSizeTMult(ModuleCount, sizeof(AUX_MODULE_EXTENDED_INFO), &calculated);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (calculated < sizeof(AUX_MODULE_EXTENDED_INFO))
    {
        return STATUS_INVALID_PARAMETER;
    }

    calculated = calculated - sizeof(AUX_MODULE_EXTENDED_INFO);
    status = RtlSizeTAdd(calculated, sizeof(MODULE_LIST_PAYLOAD), &calculated);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    *BytesNeeded = calculated;
    return STATUS_SUCCESS;
}

//
// ModuleInspectorDeviceControl
//
// Responds to IOCTL_GET_MODULE_LIST by querying module metadata via
// AuxKlibQueryModuleInformation and copying it into the caller's buffered
// output. The function never trusts caller-supplied sizes and returns
// STATUS_BUFFER_TOO_SMALL with the required byte count when resizing is
// necessary.
//
_Use_decl_annotations_ NTSTATUS ModuleInspectorDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG outBufferLength = stack->Parameters.DeviceIoControl.OutputBufferLength;

    if (stack->Parameters.DeviceIoControl.IoControlCode != IOCTL_GET_MODULE_LIST)
    {
        CompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST, 0);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (outBufferLength < sizeof(MODULE_LIST_PAYLOAD))
    {
        CompleteRequest(Irp, STATUS_BUFFER_TOO_SMALL, sizeof(MODULE_LIST_PAYLOAD));
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Query required size first to avoid buffer overruns.
    ULONG bufferSize = 0;
    NTSTATUS status = AuxKlibQueryModuleInformation(&bufferSize, sizeof(AUX_MODULE_EXTENDED_INFO), NULL);
    if (status != STATUS_BUFFER_TOO_SMALL && !NT_SUCCESS(status))
    {
        CompleteRequest(Irp, status, 0);
        return status;
    }

    if (bufferSize == 0 || (bufferSize / sizeof(AUX_MODULE_EXTENDED_INFO)) == 0)
    {
        CompleteRequest(Irp, STATUS_UNSUCCESSFUL, 0);
        return STATUS_UNSUCCESSFUL;
    }

    ULONG moduleCount = bufferSize / sizeof(AUX_MODULE_EXTENDED_INFO);
    SIZE_T bytesNeeded = 0;
    status = CalculatePayloadSize(moduleCount, &bytesNeeded);
    if (!NT_SUCCESS(status))
    {
        CompleteRequest(Irp, status, 0);
        return status;
    }

    if (bytesNeeded > ULONG_MAX)
    {
        CompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (outBufferLength < bytesNeeded)
    {
        // Return the required size so user mode can retry safely.
        CompleteRequest(Irp, STATUS_BUFFER_TOO_SMALL, (ULONG_PTR)bytesNeeded);
        return STATUS_BUFFER_TOO_SMALL;
    }

    PAUX_MODULE_EXTENDED_INFO moduleInfo =
        (PAUX_MODULE_EXTENDED_INFO)ExAllocatePoolWithTag(NonPagedPoolNx, bufferSize, MODULE_POOL_TAG);
    if (moduleInfo == NULL)
    {
        CompleteRequest(Irp, STATUS_INSUFFICIENT_RESOURCES, 0);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(moduleInfo, bufferSize);
    status = AuxKlibQueryModuleInformation(&bufferSize, sizeof(AUX_MODULE_EXTENDED_INFO), moduleInfo);
    if (!NT_SUCCESS(status))
    {
        ExFreePoolWithTag(moduleInfo, MODULE_POOL_TAG);
        CompleteRequest(Irp, status, 0);
        return status;
    }

    PMODULE_LIST_PAYLOAD payload = (PMODULE_LIST_PAYLOAD)Irp->AssociatedIrp.SystemBuffer;
    SIZE_T writableBytes = bytesNeeded < outBufferLength ? bytesNeeded : outBufferLength;
    RtlZeroMemory(payload, writableBytes);

    payload->Count = moduleCount;
    RtlCopyMemory(payload->Modules, moduleInfo, bufferSize);

    ExFreePoolWithTag(moduleInfo, MODULE_POOL_TAG);

    CompleteRequest(Irp, STATUS_SUCCESS, bytesNeeded);
    return STATUS_SUCCESS;
}
