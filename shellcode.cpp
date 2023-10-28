#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winternl.h>
#include <cstddef>
#include <cstdint>
#include "aba_pi.hpp"

typedef HMODULE(_Ret_maybenull_ WINAPI* LoadLibraryFunc)(_In_ LPCSTR lpLibFileName);
typedef FARPROC(WINAPI* GetProcAddressFunc)(_In_ HMODULE hModule, _In_ LPCSTR lpProcName);
extern "C" typedef void(__cdecl* AbapatchFunc)(AbaPatchInfo* pi);

template <typename T, size_t N>
static __declspec(safebuffers) __forceinline bool sc_streq(T(&a)[N], const T* b) {
    if (!b)
        return false;
    bool flag = true;
    for (auto i = 0; i < N; i++)
        if (a[i] != b[i]) {
            flag = false;
            break;
        }
    return flag;
}

template <typename T, size_t N>
static __declspec(safebuffers) __forceinline bool sc_strcaseeq(T(&a)[N], const T* b) {
    if (!b)
        return false;
    bool flag = true;
    for (auto i = 0; i < N; i++)
        if ((a[i] | 32) != (b[i] | 32)) {
            flag = false;
            break;
        }
    return flag;
}

#pragma section("shcode", read, execute)

__declspec(safebuffers, code_seg("shcode"))VOID NTAPI shellcode(
    _In_opt_ PVOID ApcArgument1,
    _In_opt_ PVOID ApcArgument2,
    _In_opt_ PVOID ApcArgument3) {
    wchar_t s_kernel32_dll[] = { 'k', 'e', 'r', 'n', 'e', 'l', '3', '2', '.', 'd', 'l', 'l', 0 };
    char s_loadlibrary[] = { 'L', 'o', 'a', 'd', 'L', 'i', 'b', 'r', 'a', 'r', 'y', 'A', 0 };
    char s_getprocaddress[] = { 'G', 'e', 't', 'P', 'r', 'o', 'c', 'A', 'd', 'd', 'r', 'e', 's', 's', 0 };
    char s_abapatch_dll[] = { 'a', 'b', 'a', 'p', 'a', 't', 'c', 'h', '.', 'd', 'l', 'l', 0 };
    char s_abapatch[] = { 'a', 'b', 'a', 'p', 'a', 't', 'c', 'h', 0 };

    if (!ApcArgument1)
        return;

    auto ppeb = reinterpret_cast<PPEB>(__readfsdword(offsetof(TEB, ProcessEnvironmentBlock)));
    auto link = ppeb->Ldr->InMemoryOrderModuleList.Flink;

    IMAGE_DOS_HEADER* k32Base = NULL;
    do {
        auto entry = reinterpret_cast<LDR_DATA_TABLE_ENTRY*>(CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks));
        if (sc_strcaseeq(s_kernel32_dll, (&entry->FullDllName)[1].Buffer)) {
            k32Base = static_cast<IMAGE_DOS_HEADER*>(entry->DllBase);
            break;
        }
        link = link->Flink;
        if (!link || link == ppeb->Ldr->InMemoryOrderModuleList.Flink)
            break;
    } while (1);
    if (!k32Base || k32Base->e_magic != IMAGE_DOS_SIGNATURE)
        return;

    LoadLibraryFunc f_loadLibrary = NULL;
    GetProcAddressFunc f_getprocaddress = NULL;
    auto k32NtHdr = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(k32Base) + k32Base->e_lfanew);
    auto k32DirExport = &(k32NtHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]);
    auto k32Exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(reinterpret_cast<uint8_t*>(k32Base) + k32DirExport->VirtualAddress);
    auto k32NameTable = reinterpret_cast<DWORD*>(reinterpret_cast<uint8_t*>(k32Base) + k32Exports->AddressOfNames);
    auto k32NameOrdTable = reinterpret_cast<WORD*>(reinterpret_cast<uint8_t*>(k32Base) + k32Exports->AddressOfNameOrdinals);
    auto k32FuncTable = reinterpret_cast<DWORD*>(reinterpret_cast<uint8_t*>(k32Base) + k32Exports->AddressOfFunctions);
    for (DWORD i = 0; i < k32Exports->NumberOfNames; i++) {
        auto name = reinterpret_cast<char*>(k32Base) + k32NameTable[i];
        if (!f_loadLibrary && sc_streq(s_loadlibrary, name)) {
            auto ord = k32NameOrdTable[i];
            auto func = k32FuncTable[ord];
            f_loadLibrary = reinterpret_cast<LoadLibraryFunc>(reinterpret_cast<uint8_t*>(k32Base) + func);
        }
        else if (!f_getprocaddress && sc_streq(s_getprocaddress, name)) {
            auto ord = k32NameOrdTable[i];
            auto func = k32FuncTable[ord];
            f_getprocaddress = reinterpret_cast<GetProcAddressFunc>(reinterpret_cast<uint8_t*>(k32Base) + func);
        }
    }

    if (!f_loadLibrary || !f_getprocaddress)
        return;

    auto hAba = f_loadLibrary(&s_abapatch_dll[0]);
    if (!hAba)
        return;
    auto abapatchFunc = reinterpret_cast<AbapatchFunc>(f_getprocaddress(hAba, &s_abapatch[0]));
    if (!abapatchFunc)
        return;
    abapatchFunc(static_cast<AbaPatchInfo*>(ApcArgument1));
}
