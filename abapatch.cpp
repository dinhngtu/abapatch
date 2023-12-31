#include "stdafx.h"
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include "cleanup.hpp"
#include "p47.hpp"
#include "a7xpg.hpp"
#include "rr.hpp"
#include "inject.hpp"

AbaPatchInfo abaPatchInfo{};

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

static const std::map<std::wstring, std::shared_ptr<Game>> supported_games = {
    {std::wstring(L"p47"), std::make_shared<Parsec47>()},
    {std::wstring(L"a7xpg"), std::make_shared<A7xpg>()},
    {std::wstring(L"rr"), std::make_shared<RRootage>()},
};

static void parse_args(int argc, wchar_t** _argv, std::wstring& gamename, std::shared_ptr<Game>& game, std::vector<std::unique_ptr<GamePatch>>& patches, std::vector<std::wstring>& gameargs)
{
    std::vector<wchar_t*> argv(_argv, _argv + argc);
    gamename = std::wstring(argv.at(1));
    auto g = supported_games.find(gamename);
    if (g == supported_games.end())
        throw std::runtime_error("game not supported");
    int i = 2;
    bool needs_inject = false;
    while (i < argv.size()) {
        auto arg = std::wstring(argv.at(i));
        if (arg == L"-scale") {
            arg = std::wstring(argv.at(++i));
            patches.push_back(g->second->get_patch_scale(std::stof(arg)));
        }
        else if (arg == L"-lwscale") {
            arg = std::wstring(argv.at(++i));
            patches.push_back(g->second->get_patch_lwscale(std::stof(arg)));
        }
        else if (arg == L"-nosmooth") {
            patches.push_back(g->second->get_patch_nosmooth(true));
        }
        else if (arg == L"-nobgm") {
            patches.push_back(g->second->get_patch_nobgm(true));
        }
        else if (arg == L"-high") {
            patches.push_back(g->second->get_patch_priority(HIGH_PRIORITY_CLASS));
        }
        else if (arg == L"-randbgm") {
            needs_inject = true;
            abaPatchInfo.randbgm = true;
        }
        else if (arg == L"-volume") {
            needs_inject = true;
            abaPatchInfo.volume = _wtoi(argv.at(++i));
        }
        else if (arg == L"-bgmvol") {
            needs_inject = true;
            abaPatchInfo.bgmvol = _wtoi(argv.at(++i));
        }
        else {
            gameargs.push_back(arg);
        }
        i++;
    }
    if (needs_inject)
        patches.push_back(g->second->get_patch_inject());
}

int wmain(int argc, wchar_t** argv) {
    try {
        std::wstring gamename;
        std::shared_ptr<Game> game;
        std::vector<std::unique_ptr<GamePatch>> patches;
        std::vector<std::wstring> gameargs;
        try {
            parse_args(argc, argv, gamename, game, patches, gameargs);
        }
        catch (const std::exception&) {
            throw std::runtime_error("Usage: abapatch [game] [patch_args] [game_args]");
        }

        std::wstringstream _exe;
        _exe << gamename << L".exe";
        auto exe = _exe.str();
        auto cmdline = make_cmdline(exe, gameargs);

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

            GameState gamestate{
                .procinfo = pi,
                .entrypoint = static_cast<uint32_t>(ctx.Eax),
            };

            for (auto& patch : patches) {
                patch->apply(gamestate);
            }

            ResumeThread(pi.hThread);
        }
        catch (const std::exception&) {
            TerminateProcess(pi.hProcess, 1);
            throw;
        }
    }
    catch (const std::exception& e) {
        MessageBoxA(NULL, e.what(), "abapatch", MB_OK | MB_ICONERROR);
        return 1;
    }

    return 0;
}
