#include "stdafx.h"
#include <array>
#include <winternl.h>
#include "game.hpp"
#include "gamepatch.hpp"
#include "inject.hpp"

VOID NTAPI shellcode(_In_ ULONG_PTR arg);

class InjectPatch : public GamePatch {
public:
    void apply(GameState& lg) override {
        auto shellcodeSize = getShellcodeSize();
        if (!shellcodeSize)
            throw std::runtime_error("cannot find shellcode size");

        auto shellcodeMem = VirtualAllocEx(lg.procinfo.hProcess, NULL, shellcodeSize, MEM_COMMIT, PAGE_EXECUTE_READ);
        if (!shellcodeMem)
            throw std::system_error(GetLastError(), std::system_category(), "VirtualAllocEx (shellcode)");

        SIZE_T written;
        if (!WriteProcessMemory(lg.procinfo.hProcess, shellcodeMem, shellcode, shellcodeSize, &written))
            throw std::system_error(GetLastError(), std::system_category(), "WriteProcessMemory (shellcode)");
        if (written != shellcodeSize)
            throw std::runtime_error("WriteProcessMemory didn't write enough data (shellcode)");

        auto argMem = VirtualAllocEx(lg.procinfo.hProcess, NULL, shellcodeSize, MEM_COMMIT, PAGE_READWRITE);
        if (!argMem)
            throw std::system_error(GetLastError(), std::system_category(), "VirtualAllocEx (arg)");

        if (!WriteProcessMemory(lg.procinfo.hProcess, argMem, &abaPatchInfo, sizeof(abaPatchInfo), &written))
            throw std::system_error(GetLastError(), std::system_category(), "WriteProcessMemory (arg)");
        if (written != sizeof(abaPatchInfo))
            throw std::runtime_error("WriteProcessMemory didn't write enough data (arg)");

        auto ret = QueueUserAPC(static_cast<PAPCFUNC>(shellcodeMem), lg.procinfo.hThread, reinterpret_cast<ULONG_PTR>(argMem));
        if (!ret)
            throw std::system_error(GetLastError(), std::system_category(), "QueueUserAPC");
    }

private:
    DWORD getShellcodeSize() {
        auto mh = GetModuleHandleW(NULL);
        if (!mh)
            throw std::system_error(GetLastError(), std::system_category(), "GetModuleHandleW");

        auto k32Base = reinterpret_cast<IMAGE_DOS_HEADER*>(mh);
        auto k32NtHdr = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(k32Base) + k32Base->e_lfanew);
        auto sec = IMAGE_FIRST_SECTION(k32NtHdr);
        for (auto i = 0; i < k32NtHdr->FileHeader.NumberOfSections; i++)
            if (!strcmp("shcode", reinterpret_cast<const char*>(sec[i].Name)))
                return sec[i].SizeOfRawData;
        return 0;
    }
};

std::unique_ptr<GamePatch> Game::get_patch_priority(DWORD priorityClass) {
    return std::make_unique<PriorityPatch>(priorityClass);
}

std::unique_ptr<GamePatch> Game::get_patch_inject() {
    return std::make_unique<InjectPatch>();
}
