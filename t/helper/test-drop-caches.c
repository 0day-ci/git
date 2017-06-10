#include "git-compat-util.h"
#include <stdio.h>

typedef DWORD NTSTATUS;

#ifdef GIT_WINDOWS_NATIVE
#include <tchar.h>

#define STATUS_SUCCESS			(0x00000000L)
#define STATUS_PRIVILEGE_NOT_HELD	(0xC0000061L)

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemMemoryListInformation = 80, // 80, q: SYSTEM_MEMORY_LIST_INFORMATION; s: SYSTEM_MEMORY_LIST_COMMAND (requires SeProfileSingleProcessPrivilege)
} SYSTEM_INFORMATION_CLASS;

// private
typedef enum _SYSTEM_MEMORY_LIST_COMMAND
{
	MemoryCaptureAccessedBits,
	MemoryCaptureAndResetAccessedBits,
	MemoryEmptyWorkingSets,
	MemoryFlushModifiedList,
	MemoryPurgeStandbyList,
	MemoryPurgeLowPriorityStandbyList,
	MemoryCommandMax
} SYSTEM_MEMORY_LIST_COMMAND;

BOOL GetPrivilege(HANDLE TokenHandle, LPCSTR lpName, int flags)
{
	BOOL bResult;
	DWORD dwBufferLength;
	LUID luid;
	TOKEN_PRIVILEGES tpPreviousState;
	TOKEN_PRIVILEGES tpNewState;

	dwBufferLength = 16;
	bResult = LookupPrivilegeValueA(0, lpName, &luid);
	if (bResult)
	{
		tpNewState.PrivilegeCount = 1;
		tpNewState.Privileges[0].Luid = luid;
		tpNewState.Privileges[0].Attributes = 0;
		bResult = AdjustTokenPrivileges(TokenHandle, 0, &tpNewState, (DWORD)((LPBYTE)&(tpNewState.Privileges[1]) - (LPBYTE)&tpNewState), &tpPreviousState, &dwBufferLength);
		if (bResult)
		{
			tpPreviousState.PrivilegeCount = 1;
			tpPreviousState.Privileges[0].Luid = luid;
			tpPreviousState.Privileges[0].Attributes = flags != 0 ? 2 : 0;
			bResult = AdjustTokenPrivileges(TokenHandle, 0, &tpPreviousState, dwBufferLength, 0, 0);
		}
	}
	return bResult;
}
#endif

int cmd_main(int argc, const char **argv)
{
	NTSTATUS status = 1;
#ifdef GIT_WINDOWS_NATIVE
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hToken;
	if (!OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		_ftprintf(stderr, _T("Can't open current process token\n"));
		return 1;
	}

	if (!GetPrivilege(hToken, "SeProfileSingleProcessPrivilege", 1))
	{
		_ftprintf(stderr, _T("Can't get SeProfileSingleProcessPrivilege\n"));
		return 1;
	}

	CloseHandle(hToken);

	HMODULE ntdll = LoadLibrary(_T("ntdll.dll"));
	if (!ntdll)
	{
		_ftprintf(stderr, _T("Can't load ntdll.dll, wrong Windows version?\n"));
		return 1;
	}

	NTSTATUS(WINAPI *NtSetSystemInformation)(INT, PVOID, ULONG) = (NTSTATUS(WINAPI *)(INT, PVOID, ULONG))GetProcAddress(ntdll, "NtSetSystemInformation");
	if (!NtSetSystemInformation)
	{
		_ftprintf(stderr, _T("Can't get function addresses, wrong Windows version?\n"));
		return 1;
	}

	SYSTEM_MEMORY_LIST_COMMAND command = MemoryPurgeStandbyList;
	status = NtSetSystemInformation(
		SystemMemoryListInformation,
		&command,
		sizeof(SYSTEM_MEMORY_LIST_COMMAND)
	);
	if (status == STATUS_PRIVILEGE_NOT_HELD)
	{
		_ftprintf(stderr, _T("Insufficient privileges to execute the memory list command"));
	}
	else if (status != STATUS_SUCCESS)
	{
		_ftprintf(stderr, _T("Unable to execute the memory list command %lX"), status);
	}
#endif

	return status;
}
