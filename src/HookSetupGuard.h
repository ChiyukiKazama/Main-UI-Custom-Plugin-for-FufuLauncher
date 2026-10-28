#pragma once
#include <Windows.h>
#include <cwchar>

namespace Support
{
    // Each DLL statically links its own MinHook lock. Serialize setup across
    // DLLs too: enabling/removing hooks suspends other process threads.
    class HookSetupGuard
    {
        HANDLE mutexHandle=nullptr;
        bool lockAcquired=false;
    public:
        HookSetupGuard()
        {
            wchar_t mutexName[80]{};
            swprintf_s(mutexName,L"Local\\FufuDetachedHookSetup-%lu",GetCurrentProcessId());
            mutexHandle=CreateMutexW(nullptr,FALSE,mutexName);
            if(mutexHandle)
            {
                const auto result=WaitForSingleObject(mutexHandle,30000);
                lockAcquired=result==WAIT_OBJECT_0 || result==WAIT_ABANDONED;
            }
        }
        ~HookSetupGuard() { if(lockAcquired) ReleaseMutex(mutexHandle); if(mutexHandle) CloseHandle(mutexHandle); }
        explicit operator bool() const { return lockAcquired; }
        HookSetupGuard(const HookSetupGuard&)=delete;
        HookSetupGuard& operator=(const HookSetupGuard&)=delete;
    };
}
