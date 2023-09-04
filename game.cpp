#include "stdafx.h"
#include <array>
#include <winternl.h>
#include "game.hpp"
#include "gamepatch.hpp"

typedef VOID(NTAPI* PPS_APC_ROUTINE)(
    _In_opt_ PVOID ApcArgument1,
    _In_opt_ PVOID ApcArgument2,
    _In_opt_ PVOID ApcArgument3
    );

typedef NTSYSCALLAPI
NTSTATUS
(NTAPI* NtQueueApcThreadFunc)(
    _In_ HANDLE ThreadHandle,
    _In_ PPS_APC_ROUTINE ApcRoutine,
    _In_opt_ PVOID ApcArgument1,
    _In_opt_ PVOID ApcArgument2,
    _In_opt_ PVOID ApcArgument3
    );

__declspec(safebuffers)VOID NTAPI shellcode(
    _In_opt_ PVOID ApcArgument1,
    _In_opt_ PVOID ApcArgument2,
    _In_opt_ PVOID ApcArgument3);

class InjectPatch : public GamePatch {
public:
    void apply(GameState& lg) override {
        auto ntdll = GetModuleHandleW(L"ntdll.dll");
        if (!ntdll)
            throw std::system_error(GetLastError(), std::system_category(), "GetModuleHandleW");
        auto ntQueueApcThread = reinterpret_cast<NtQueueApcThreadFunc>(GetProcAddress(ntdll, "NtQueueApcThread"));
        if (!ntQueueApcThread)
            throw std::system_error(GetLastError(), std::system_category(), "GetProcAddress");

        auto shellcodeMem = VirtualAllocEx(lg.procinfo.hProcess, NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!shellcodeMem)
            throw std::system_error(GetLastError(), std::system_category(), "VirtualAllocEx");

        SIZE_T written;
        if (!WriteProcessMemory(lg.procinfo.hProcess, shellcodeMem, shellcode, 4096, &written))
            throw std::system_error(GetLastError(), std::system_category(), "WriteProcessMemory");
        if (written != 4096)
            throw std::runtime_error("WriteProcessMemory didn't write enough data");

        auto ret = ntQueueApcThread(lg.procinfo.hThread, reinterpret_cast<PPS_APC_ROUTINE>(shellcodeMem), NULL, NULL, NULL);
        if (!NT_SUCCESS(ret))
            throw std::runtime_error("NtQueueApcThread");
    }
};

std::unique_ptr<GamePatch> Game::get_patch_priority(DWORD priorityClass) {
    return std::make_unique<PriorityPatch>(priorityClass);
}

std::unique_ptr<GamePatch> Game::get_patch_inject() {
    return std::make_unique<InjectPatch>();
}
