#include "pch.h"
#include "detours.h"

struct Mix_Music;
int my_Mix_PlayMusic(Mix_Music* music, int loops) {
    return 0;
}

static void install(bool value) {
    auto playmusic = DetourFindFunction("SDL_mixer.dll", "Mix_PlayMusic");
    if (!playmusic)
        return;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (value)
        DetourAttach(&playmusic, my_Mix_PlayMusic);
    else
        DetourDetach(&playmusic, my_Mix_PlayMusic);
    DetourTransactionCommit();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        install(true);
        break;
    case DLL_PROCESS_DETACH:
        install(false);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

