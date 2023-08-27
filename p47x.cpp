#define WIN32_LEAN_AND_MEAN

#include <system_error>
#include <vector>
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
static constexpr uint32_t P47_VAL_LINESMOOTH_ENABLE = 0x000212fa;
static constexpr uint32_t P47_VAL_LINESMOOTH_DISABLE = 0x00021300;

static constexpr uint32_t P47_MEM_PLAYBGM = 0x0041000c;
static constexpr uint8_t P47_VAL_PLAYBGM_ENABLE = 0x50;
static constexpr uint8_t P47_VAL_PLAYBGM_DISABLE = 0xc3;

struct p47x_options {
    float scale = 1.0f;
    float lwscale = 1.0f;
    bool nosmooth = false;
    bool nobgm = false;
};

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
static bool proc_cmpxchg(HANDLE hProcess, LPVOID lpBaseAddress, T expected, T desired) {
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

static std::wstring make_cmdline(const std::wstring& exe, const std::vector<std::wstring>& args) {
    std::wstringstream cmdbuf;
    cmdbuf << exe;
    for (const auto& arg : args) {
        cmdbuf << L" ";
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

static p47x_options parse_args(int argc, wchar_t** argv, std::vector<std::wstring>& pargs)
{
    p47x_options opts;
    int i = 1;
    while (i < argc) {
        auto arg = std::wstring(argv[i]);
        if (arg == L"-scale" && i < argc - 1) {
            arg = std::wstring(argv[++i]);
            opts.scale = std::stof(arg);
        }
        else if (arg == L"-lwscale" && i < argc - 1) {
            arg = std::wstring(argv[++i]);
            opts.lwscale = std::stof(arg);
        }
        else if (arg == L"-nosmooth") {
            opts.nosmooth = true;
        }
        else if (arg == L"-nobgm") {
            opts.nobgm = true;
        }
        else {
            pargs.push_back(arg);
        }
        i++;
    }
    return opts;
}

int wmain(int argc, wchar_t** argv) {
    try {
        std::vector<std::wstring> pargs;
        p47x_options opts;
        try {
            opts = parse_args(argc, argv, pargs);
        }
        catch (const std::exception&) {
            throw std::runtime_error("Usage: p47x [-scale scale] [-lwscale lwscale] [-nosmooth] [-nobgm] [p47_args]");
        }

        std::wstring exe = L"p47.exe";
        auto cmdline = make_cmdline(exe, pargs);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
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

            if (opts.scale != 1.0f) {
                if (!proc_cmpxchg<uint32_t>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_WIDTH), P47_VAL_WIDTH_DEFAULT, static_cast<uint32_t>(P47_VAL_WIDTH_DEFAULT * opts.scale))) {
                    throw std::runtime_error("unexpected value");
                }
                if (!proc_cmpxchg<uint32_t>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_HEIGHT), P47_VAL_HEIGHT_DEFAULT, static_cast<uint32_t>(P47_VAL_HEIGHT_DEFAULT * opts.scale))) {
                    throw std::runtime_error("unexpected value");
                }
            }
            if (opts.lwscale != 1.0f) {
                if (!proc_cmpxchg<float>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_LINEWIDTH), P47_VAL_LINEWIDTH_DEFAULT, P47_VAL_LINEWIDTH_DEFAULT * opts.lwscale)) {
                    throw std::runtime_error("unexpected value");
                }
            }
            if (opts.nosmooth) {
                if (!proc_cmpxchg<uint32_t>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_LINESMOOTH_CALL), P47_VAL_LINESMOOTH_ENABLE, P47_VAL_LINESMOOTH_DISABLE)) {
                    throw std::runtime_error("unexpected value");
                }
            }
            if (opts.nobgm) {
                if (!proc_cmpxchg<uint8_t>(pi.hProcess, reinterpret_cast<LPVOID>(loff + P47_MEM_PLAYBGM), P47_VAL_PLAYBGM_ENABLE, P47_VAL_PLAYBGM_DISABLE)) {
                    throw std::runtime_error("unexpected value");
                }
            }

            ResumeThread(pi.hThread);
        }
        catch (const std::exception&) {
            TerminateProcess(pi.hProcess, 1);
            throw;
        }
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "p47x", MB_OK | MB_ICONERROR);
        return 1;
    }

    return 0;
}
