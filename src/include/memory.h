#pragma once
#include <Windows.h>
#include <TlHelp32.h>

#include <cstdint>
#include <string_view>
#include <iostream>
#include <string>

#define SeDebugPriv 20
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#define NtCurrentProcess ((HANDLE)(LONG_PTR)-1) 
#define ProcessHandleType 0x7
#define SystemHandleInformation 16 

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWCH   Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
	ULONG           Length;
	HANDLE          RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG           Attributes;
	PVOID           SecurityDescriptor;
	PVOID           SecurityQualityOfService;
}  OBJECT_ATTRIBUTES, * POBJECT_ATTRIBUTES;

typedef struct _CLIENT_ID
{
	PVOID UniqueProcess;
	PVOID UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO
{
	ULONG ProcessId;
	BYTE ObjectTypeNumber;
	BYTE Flags;
	USHORT Handle;
	PVOID Object;
	ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, * PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
	ULONG HandleCount;
	SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

typedef NTSTATUS(NTAPI* _NtDuplicateObject)(
	HANDLE SourceProcessHandle,
	HANDLE SourceHandle,
	HANDLE TargetProcessHandle,
	PHANDLE TargetHandle,
	ACCESS_MASK DesiredAccess,
	ULONG Attributes,
	ULONG Options
	);

typedef NTSTATUS(NTAPI* _RtlAdjustPrivilege)(
	ULONG Privilege,
	BOOLEAN Enable,
	BOOLEAN CurrentThread,
	PBOOLEAN Enabled
	);

typedef NTSYSAPI NTSTATUS(NTAPI* _NtOpenProcess)(
	PHANDLE            ProcessHandle,
	ACCESS_MASK        DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PCLIENT_ID         ClientId
	);

typedef NTSTATUS(NTAPI* _NtQuerySystemInformation)(
	ULONG SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
	);

SYSTEM_HANDLE_INFORMATION* hInfo;

namespace hj
{
	HANDLE procHandle = NULL;
	HANDLE hProcess = NULL;
	HANDLE HijackedHandle = NULL;

	OBJECT_ATTRIBUTES InitObjectAttributes(PUNICODE_STRING name, ULONG attributes, HANDLE hRoot, PSECURITY_DESCRIPTOR security)
	{
		OBJECT_ATTRIBUTES object;

		object.Length = sizeof(OBJECT_ATTRIBUTES);
		object.ObjectName = name;
		object.Attributes = attributes;
		object.RootDirectory = hRoot;
		object.SecurityDescriptor = security;

		return object;
	}

	bool IsHandleValid(HANDLE handle)
	{
		return handle && handle != INVALID_HANDLE_VALUE;
	}

	HANDLE HijackExistingHandle(DWORD dwTargetProcessId)
	{
		HMODULE Ntdll = GetModuleHandleA("ntdll");
		_RtlAdjustPrivilege RtlAdjustPrivilege = (_RtlAdjustPrivilege)GetProcAddress(Ntdll, "RtlAdjustPrivilege");
		boolean OldPriv;

		RtlAdjustPrivilege(SeDebugPriv, TRUE, FALSE, &OldPriv);

		_NtQuerySystemInformation NtQuerySystemInformation = (_NtQuerySystemInformation)GetProcAddress(Ntdll, "NtQuerySystemInformation");
		_NtDuplicateObject NtDuplicateObject = (_NtDuplicateObject)GetProcAddress(Ntdll, "NtDuplicateObject");
		_NtOpenProcess NtOpenProcess = (_NtOpenProcess)GetProcAddress(Ntdll, "NtOpenProcess");

		OBJECT_ATTRIBUTES Obj_Attribute = InitObjectAttributes(NULL, NULL, NULL, NULL);
		CLIENT_ID clientID = { 0 };
		DWORD size = sizeof(SYSTEM_HANDLE_INFORMATION);

		hInfo = (SYSTEM_HANDLE_INFORMATION*)new byte[size];

		ZeroMemory(hInfo, size);

		NTSTATUS NtRet = NULL;

		do
		{
			delete[] hInfo;
			size *= 1.5;

			try
			{
				hInfo = (PSYSTEM_HANDLE_INFORMATION)new byte[size];
			}
			catch (std::bad_alloc)
			{
				procHandle ? CloseHandle(procHandle) : 0;
			}

			Sleep(1);
		} while ((NtRet = NtQuerySystemInformation(SystemHandleInformation, hInfo, size, NULL)) == STATUS_INFO_LENGTH_MISMATCH);

		if (!NT_SUCCESS(NtRet))
			procHandle ? CloseHandle(procHandle) : 0;

		for (unsigned int i = 0; i < hInfo->HandleCount; ++i)
		{
			static DWORD NumOfOpenHandles;

			GetProcessHandleCount(GetCurrentProcess(), &NumOfOpenHandles);

			if (NumOfOpenHandles > 50)
				procHandle ? CloseHandle(procHandle) : 0;

			if (!IsHandleValid((HANDLE)hInfo->Handles[i].Handle))
				continue;

			if (hInfo->Handles[i].ObjectTypeNumber != ProcessHandleType)
				continue;

			clientID.UniqueProcess = (DWORD*)hInfo->Handles[i].ProcessId;
			procHandle ? CloseHandle(procHandle) : 0;

			NtRet = NtOpenProcess(&procHandle, PROCESS_DUP_HANDLE, &Obj_Attribute, &clientID);
			if (!IsHandleValid(procHandle) || !NT_SUCCESS(NtRet))
				continue;

			NtRet = NtDuplicateObject(procHandle, (HANDLE)hInfo->Handles[i].Handle, NtCurrentProcess, &HijackedHandle, PROCESS_ALL_ACCESS, 0, 0);
			if (!IsHandleValid(HijackedHandle) || !NT_SUCCESS(NtRet))
				continue;

			if (GetProcessId(HijackedHandle) != dwTargetProcessId) {
				CloseHandle(HijackedHandle);
				continue;
			}

			hProcess = HijackedHandle;

			break;
		}

		procHandle ? CloseHandle(procHandle) : 0;

		return hProcess;
	}
}

class Memory
{
private:
    std::uintptr_t processId = 0;
    void* processHandle = nullptr;

public:
    Memory(const std::string_view processName) noexcept
    {
        ::PROCESSENTRY32 entry = { };
        entry.dwSize = sizeof(::PROCESSENTRY32);

        const auto snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        while (::Process32Next(snapShot, &entry))
        {
            if (!processName.compare(entry.szExeFile))
            {
                processId = entry.th32ProcessID;
				processHandle = hj::HijackExistingHandle(processId);

				HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

				if (!processHandle)
				{
					processHandle = OpenProcess(PROCESS_ALL_ACCESS, 0, processId);
					std::cout << "VAC Status: ";
					SetConsoleTextAttribute(hConsole, 12);
					std::cout << "Insecure" << std::endl;
					Beep(523, 500);
					Beep(523, 500);
				}

				std::cout << "VAC Status: ";
				SetConsoleTextAttribute(hConsole, 10);
				std::cout << "Bypassed" << std::endl;
				SetConsoleTextAttribute(hConsole, 15);
				Beep(523, 500);

                break;
            }
        }

        if (snapShot)
            ::CloseHandle(snapShot);

		if (processHandle == 0)
			return;

		std::cout << "Got HANDLE to Counter-Strike 2: " << processHandle << std::endl;
    }

    ~Memory()
    {
        if (processHandle)
            ::CloseHandle(processHandle);
    }

    const std::uintptr_t GetModuleAddress(const std::string_view moduleName) const noexcept
    {
        ::MODULEENTRY32 entry = { };
        entry.dwSize = sizeof(::MODULEENTRY32);

        const auto snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

        std::uintptr_t result = 0;

        while (::Module32Next(snapShot, &entry))
        {
            if (!moduleName.compare(entry.szModule))
            {
                result = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                break;
            }
        }

        if (snapShot)
            ::CloseHandle(snapShot);

        return result;
    }

    template <typename T>
    constexpr const T Read(const std::uintptr_t& address) const noexcept
    {
        T value = { };

		::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(address), &value, sizeof(T), NULL);
        return value;
    }

    template <typename T>
    constexpr void Write(const std::uintptr_t& address, const T& value) const noexcept
    {
        ::WriteProcessMemory(processHandle, reinterpret_cast<void*>(address), &value, sizeof(T), NULL);
    }

    bool InForeground()
    {
        HWND current = GetForegroundWindow();

        char title[256];
        GetWindowText(current, title, sizeof(title));

        if (strstr(title, "Counter-Strike 2") != nullptr)
            return true;

        return false;
    }
};
