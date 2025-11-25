#include "Common.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

int main()
{
    static_assert(sizeof(((AUX_MODULE_EXTENDED_INFO*)0)->FullPathName) == 512, "FullPathName must remain 512 bytes");
    static_assert(sizeof(AUX_MODULE_BASIC_INFO) >= sizeof(PVOID) + sizeof(ULONG),
                  "AUX_MODULE_BASIC_INFO layout mismatch");

    constexpr ULONG expectedFunction = 0x800;
    constexpr ULONG extractedFunction = (IOCTL_GET_MODULE_LIST >> 2) & 0xFFF;
    if (extractedFunction != expectedFunction)
    {
        std::cerr << "IOCTL function code changed unexpectedly" << std::endl;
        return 1;
    }

    const size_t count = 4;
    const size_t expectedBytes = sizeof(MODULE_LIST_PAYLOAD) + (count - 1) * sizeof(AUX_MODULE_EXTENDED_INFO);
    if (expectedBytes <= sizeof(MODULE_LIST_PAYLOAD))
    {
        std::cerr << "Payload sizing math is incorrect" << std::endl;
        return 1;
    }

    if (expectedBytes >= std::numeric_limits<ULONG>::max())
    {
        std::cerr << "Payload size exceeds ULONG and would break IOCTL bounds" << std::endl;
        return 1;
    }

    const std::wstring deviceName = MODULE_DEVICE_NAME;
    if (deviceName.find(L"\\\\.\\") != 0)
    {
        std::cerr << "Device name is missing Win32 namespace prefix" << std::endl;
        return 1;
    }

    const std::wstring ntName = MODULE_NT_DEVICE_NAME;
    const bool hasNtPrefix = ntName.rfind(L"\\\\Device\\", 0) == 0 || ntName.rfind(L"\\Device\\", 0) == 0;
    if (!hasNtPrefix)
    {
        std::cerr << "NT device name is missing expected prefix" << std::endl;
        return 1;
    }

    std::cout << "Payload layout checks passed (" << expectedBytes << " bytes for " << count << " modules)."
              << std::endl;
    return 0;
}
