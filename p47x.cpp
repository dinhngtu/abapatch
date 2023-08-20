#define WIN32_LEAN_AND_MEAN

#include <system_error>
#include <string>
#include <sstream>
#include <functional>
#include <Windows.h>
#include <Psapi.h>

static constexpr uint32_t P47_ENTRYPOINT = 0x00420e2c;

static constexpr uint32_t P47_MEM_WIDTH = 0x00475a44;
static constexpr uint32_t P47_VAL_WIDTH_DEFAULT = 0x00000280;

static constexpr uint32_t P47_MEM_HEIGHT = 0x00475a48;
static constexpr uint32_t P47_VAL_HEIGHT_DEFAULT = 0x000001e0;

static constexpr uint32_t P47_MEM_LINEWIDTH = 0x0040bc75;
static constexpr float P47_VAL_LINEWIDTH_DEFAULT = 1.0f;

static constexpr uint32_t P47_MEM_LINESMOOTH_CALL = 0x0040bc84;
static constexpr uint32_t P47_VAL_GLENABLE = 0x000212fa;
static constexpr uint32_t P47_VAL_GLDISABLE = 0x00021300;

class cleanup {
public:
    using cb_type = std::function<void()>;

    explicit cleanup(cb_type cleaner) : _cleaner(cleaner) {
    }
    cleanup(const cleanup&) = delete;
    cleanup& operator=(const cleanup&) = delete;
    cleanup(cleanup&&) = default;
    cleanup& operator=(cleanup&&) = default;
    ~cleanup() {
        _cleaner();
    }

private:
    cb_type _cleaner;
};

template <typename T>
bool proc_cmpxchg(HANDLE hProcess, LPVOID lpBaseAddress, T expected, T desired) {
    T buf{};
    if (!ReadProcessMemory(hProcess, lpBaseAddress, &buf, sizeof(T), NULL)) {
        throw std::system_error(GetLastError(), std::system_category(), "ReadProcessMemory");
    }
    if (buf != expected) {
        return false;
    }
    buf = desired;
    if (!WriteProcessMemory(hProcess, lpBaseAddress, &buf, sizeof(T), NULL)) {
        throw std::system_error(GetLastError(), std::system_category(), "WriteProcessMemory");
    }
    return true;
}

std::wstring make_cmdline(const std::wstring& exe, int argc, wchar_t** argv) {
    std::wstringstream cmdbuf;
    cmdbuf << exe;
    for (int i = 1; i < argc; i++) {
        cmdbuf << L" ";
        auto arg = std::wstring(argv[i]);
        auto space = arg.find(L' ') != arg.npos;
        if (space) {
            cmdbuf << L"\"";
        }
        for (auto c : arg) {
            if (c == L'"')
                cmdbuf << L"\"\"";
            else
                cmdbuf << c;
        }
        if (space) {
            cmdbuf << L"\"";
        }
    }
    return cmdbuf.str();
}

int wmain(int argc, wchar_t** argv) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring exe{ L"p47.exe" };
    auto cmdline = make_cmdline(exe, argc, argv);
    auto res = CreateProcessW(exe.c_str(), cmdline.data(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    if (!res) {
        throw std::system_error(GetLastError(), std::system_category(), "CreateProcess");
    }
    auto c_pi = cleanup([&] {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        });

    try {
#if _WIN64
        WOW64_CONTEXT ctx{};
        ctx.ContextFlags = WOW64_CONTEXT_INTEGER;
        if (!Wow64GetThreadContext(pi.hThread, &ctx)) {
            throw std::system_error(GetLastError(), std::system_category(), "Wow64GetThreadContext");
        }
#else
        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_INTEGER;
        if (!GetThreadContext(pi.hThread, &ctx)) {
            throw std::system_error(GetLastError(), std::system_category(), "GetThreadContext");
        }
#endif

        auto real_ep = static_cast<uint32_t>(ctx.Eax);
        auto loff = real_ep - P47_ENTRYPOINT;

        if (!proc_cmpxchg<uint32_t>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_WIDTH), P47_VAL_WIDTH_DEFAULT, P47_VAL_WIDTH_DEFAULT * 2)) {
            throw std::runtime_error("unexpected value");
        }
        if (!proc_cmpxchg<uint32_t>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_HEIGHT), P47_VAL_HEIGHT_DEFAULT, P47_VAL_HEIGHT_DEFAULT * 2)) {
            throw std::runtime_error("unexpected value");
        }
        if (!proc_cmpxchg<float>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_LINEWIDTH), P47_VAL_LINEWIDTH_DEFAULT, P47_VAL_LINEWIDTH_DEFAULT * 2)) {
            throw std::runtime_error("unexpected value");
        }
        if (!proc_cmpxchg<uint32_t>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_LINESMOOTH_CALL), P47_VAL_GLENABLE, P47_VAL_GLDISABLE)) {
            throw std::runtime_error("unexpected value");
        }

        ResumeThread(pi.hThread);
    }
    catch (const std::exception& e) {
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    return 0;
}
