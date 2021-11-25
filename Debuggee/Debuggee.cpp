#include <iostream>
#include <windows.h>

#define IO_UNSET_THREADHIDEFROMDEBUGGER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8B2, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
typedef NTSTATUS(*_NtSetInformationThread)(HANDLE, ULONG, PULONG, ULONG);
typedef NTSTATUS(*_NtQueryInformationThread)(HANDLE, ULONG, PULONG, ULONG, PULONG);
_NtSetInformationThread NtSetInformationThread;
_NtQueryInformationThread NtQueryInformationThread;


void SetThreadDebuggable(HANDLE hDriver, DWORD threadId) {
    DeviceIoControl(hDriver, IO_UNSET_THREADHIDEFROMDEBUGGER, &threadId, sizeof(threadId), NULL, 0, NULL, NULL);
}

void SetThreadNotDebuggable(HANDLE thread) {
    (NtSetInformationThread)(thread, 0x11, 0, 0);
}

ULONG GetThreadDebuggableStatus(HANDLE thread) {
    ULONG hideThread = 0;
    ULONG outputSize = sizeof(ULONG);
    (NtQueryInformationThread)(thread, 0x11, &hideThread, sizeof(BOOLEAN), &outputSize);

    return hideThread;
}

int main()
{
    HANDLE hDriver = CreateFile(L"\\\\.\\armswideopen", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

    if (hDriver == INVALID_HANDLE_VALUE)
    {
        std::cout << "Couldn't connect to driver" << std::endl;
        return 0;
    }

    HMODULE ntDll = LoadLibrary(L"ntdll.dll");
    NtSetInformationThread = (_NtSetInformationThread)GetProcAddress(ntDll, "NtSetInformationThread");
    NtQueryInformationThread = (_NtQueryInformationThread)GetProcAddress(ntDll, "NtQueryInformationThread");

    NTSTATUS result;
    
    // initial state
    std::cout << "isDebuggerBlocked: " << GetThreadDebuggableStatus(GetCurrentThread()) << std::endl;

    // disable debugging
    SetThreadNotDebuggable(GetCurrentThread());
    std::cout << "Set ThreadHideFromDebugger" << std::endl;

    // should be blocked now
    std::cout << "isDebuggerBlocked: " << GetThreadDebuggableStatus(GetCurrentThread()) << std::endl;

    // enable debugging
    SetThreadDebuggable(hDriver, GetCurrentThreadId());
    std::cout << "Reset ThreadHideFromDebugger" << std::endl;

    // should be unblocked now
    std::cout << "isDebuggerBlocked: " << GetThreadDebuggableStatus(GetCurrentThread()) << std::endl;

    // wait
    std::cin >> result;
}