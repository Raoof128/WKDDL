#include <Softpub.h>
#include <Windows.h>
#include <Winternl.h>
#include <wintrust.h>

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Common.h"

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

namespace
{
using HandlePtr = std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&CloseHandle)>;

[[nodiscard]] HandlePtr OpenDriverHandle()
{
    HANDLE rawHandle = CreateFileW(MODULE_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);

    return HandlePtr(rawHandle, &CloseHandle);
}

[[nodiscard]] std::wstring ToWide(const UCHAR* utf8Path)
{
    if (utf8Path == nullptr)
    {
        return L"";
    }

    // AuxKlib returns NT-style ANSI paths. CP_ACP provides best-effort
    // conversion without assuming UTF-8 input from kernel space.
    int required = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(utf8Path), -1, nullptr, 0);
    if (required <= 0)
    {
        return L"";
    }

    std::wstring wide(static_cast<size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(utf8Path), -1, wide.data(), required) == 0)
    {
        return L"";
    }

    if (!wide.empty() && wide.back() == L'\0')
    {
        wide.pop_back();
    }
    return wide;
}

[[nodiscard]] bool IsFileSigned(const std::wstring& path)
{
    if (path.empty())
    {
        return false;
    }

    WINTRUST_FILE_INFO fileInfo = {};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = path.c_str();

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA trustData = {};
    trustData.cbStruct = sizeof(trustData);
    trustData.dwUIChoice = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    trustData.dwUnionChoice = WTD_CHOICE_FILE;
    trustData.pFile = &fileInfo;
    trustData.dwStateAction = WTD_STATEACTION_VERIFY;
    trustData.dwProvFlags = WTD_REVOCATION_CHECK_NONE | WTD_CACHE_ONLY_URL_RETRIEVAL;

    LONG status = WinVerifyTrust(nullptr, &policyGUID, &trustData);

    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGUID, &trustData);

    return status == ERROR_SUCCESS;
}

void SetConsoleColor(WORD attributes)
{
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(stdoutHandle, attributes);
}

[[nodiscard]] bool ValidatePayloadSize(DWORD bytesReturned, ULONG count)
{
    if (bytesReturned < sizeof(MODULE_LIST_PAYLOAD))
    {
        return false;
    }

    const size_t entryBytes = static_cast<size_t>(count) * sizeof(AUX_MODULE_EXTENDED_INFO);
    if (count > 0 && entryBytes / sizeof(AUX_MODULE_EXTENDED_INFO) != count)
    {
        return false; // overflow
    }

    const size_t expected =
        sizeof(MODULE_LIST_PAYLOAD) + (count == 0 ? 0 : (entryBytes - sizeof(AUX_MODULE_EXTENDED_INFO)));
    return expected <= bytesReturned;
}

[[nodiscard]] bool FetchModules(const HandlePtr& device, std::vector<BYTE>& buffer, DWORD& bytesReturned)
{
    DWORD localBytes = 0;

    while (true)
    {
        localBytes = 0;
        const BOOL ioctlResult = DeviceIoControl(device.get(), IOCTL_GET_MODULE_LIST, nullptr, 0, buffer.data(),
                                                 static_cast<DWORD>(buffer.size()), &localBytes, nullptr);

        if (ioctlResult)
        {
            bytesReturned = localBytes;
            return true;
        }

        const DWORD error = GetLastError();
        if (error == ERROR_INSUFFICIENT_BUFFER && localBytes > buffer.size())
        {
            buffer.resize(localBytes);
            continue;
        }

        std::wcerr << L"[-] IOCTL_GET_MODULE_LIST failed. Error: " << error << std::endl;
        return false;
    }
}
} // namespace

int wmain()
{
    std::wcout << L"*** Run inside an isolated VM with test-signing enabled to avoid impacting host stability.***\n\n";

    HandlePtr device = OpenDriverHandle();
    if (!device || device.get() == INVALID_HANDLE_VALUE)
    {
        std::wcerr << L"[-] Failed to open driver device. Error: " << GetLastError() << std::endl;
        return 1;
    }

    std::vector<BYTE> buffer(512 * 1024); // start with 512KB, resize if driver reports larger
    DWORD bytesReturned = 0;

    if (!FetchModules(device, buffer, bytesReturned))
    {
        return 1;
    }

    if (bytesReturned < sizeof(MODULE_LIST_PAYLOAD))
    {
        std::wcerr << L"[-] Driver returned an unexpectedly small payload." << std::endl;
        return 1;
    }

    auto payload = reinterpret_cast<PMODULE_LIST_PAYLOAD>(buffer.data());
    const ULONG count = payload->Count;

    if (!ValidatePayloadSize(bytesReturned, count))
    {
        std::wcerr << L"[-] Payload size does not match expected module count." << std::endl;
        return 1;
    }

    std::wcout << std::left << std::setw(18) << L"Base Address" << std::setw(40) << L"Driver Name" << std::setw(8)
               << L"Signed" << L"Path" << std::endl;
    std::wcout << std::wstring(120, L'-') << std::endl;

    for (ULONG i = 0; i < count; ++i)
    {
        const auto& module = payload->Modules[i];
        std::wstring fullPath = ToWide(module.FullPathName);
        std::wstring fileName = fullPath;
        if (module.FileNameOffset < sizeof(module.FullPathName))
        {
            fileName = ToWide(module.FullPathName + module.FileNameOffset);
        }

        const bool signedDriver = IsFileSigned(fullPath);
        const WORD color =
            signedDriver ? FOREGROUND_GREEN | FOREGROUND_INTENSITY : FOREGROUND_RED | FOREGROUND_INTENSITY;
        SetConsoleColor(color);

        std::wcout << std::left << std::setw(18) << module.BasicInfo.ImageBase << std::setw(40) << fileName
                   << std::setw(8) << (signedDriver ? L"Yes" : L"No") << fullPath << std::endl;
    }

    SetConsoleColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    return 0;
}
