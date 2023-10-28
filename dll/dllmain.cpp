#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <winternl.h>
#include <cstdlib>
#include <vector>
#include <string>
#include "detours.h"
#include "aba_pi.hpp"

#pragma comment(lib, "bcrypt.lib")

static constexpr int MIX_MAX_VOLUME = 128;

struct Mix_Music;

typedef int(__cdecl* Mix_OpenAudioFunc)(int frequency, uint16_t format, int channels, int chunksize);
typedef Mix_Music* (__cdecl* Mix_LoadMUSFunc)(const char* file);
typedef int(__cdecl* Mix_PlayMusicFunc)(Mix_Music* music, int loops);
typedef void(__cdecl* Mix_FreeMusicFunc)(Mix_Music* music);
typedef int(__cdecl* Mix_VolumeFunc)(int channel, int volume);
typedef int(__cdecl* Mix_VolumeMusicFunc)(int volume);

static AbaPatchInfo* patchinfo = NULL;
static Mix_OpenAudioFunc openaudio = NULL;
static Mix_LoadMUSFunc loadmusic = NULL;
static Mix_PlayMusicFunc playmusic = NULL;
static std::vector<Mix_Music*> musics;

static void find_musics() {
    WIN32_FIND_DATAA f;
    auto hf = FindFirstFileA("allmusics\\*.ogg", &f);
    if (hf == INVALID_HANDLE_VALUE)
        return;
    do {
        //MessageBoxA(NULL, f.cFileName, "", MB_OK);
        auto path = std::string("allmusics\\") + f.cFileName;
        auto mus = loadmusic(path.c_str());
        if (mus)
            musics.push_back(mus);
    } while (FindNextFileA(hf, &f));
    FindClose(hf);
}

static void free_musics() {
    auto freemusic = static_cast<Mix_FreeMusicFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_FreeMusic"));
    if (!freemusic)
        return;
    for (auto mus : musics)
        freemusic(mus);
}

int __cdecl my_Mix_OpenAudio(int frequency, uint16_t format, int channels, int chunksize) {
    static bool inited = false;
    auto ret = openaudio(frequency, format, channels, chunksize);
    if (!inited && patchinfo) {
        if (patchinfo->volume >= 0 && patchinfo->volume < MIX_MAX_VOLUME) {
            auto volume = static_cast<Mix_VolumeFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_Volume"));
            if (volume)
                volume(-1, patchinfo->volume);
        }
        if (patchinfo->bgmvol >= 0 && patchinfo->bgmvol < MIX_MAX_VOLUME) {
            auto volmusic = static_cast<Mix_VolumeMusicFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_VolumeMusic"));
            if (volmusic)
                volmusic(patchinfo->bgmvol);
        }
        inited = true;
    }
    return ret;
}

int __cdecl my_Mix_PlayMusic(Mix_Music* music, int loops) {
    if (musics.size()) {
        uint64_t r;
        if (!NT_SUCCESS(BCryptGenRandom(NULL, reinterpret_cast<PUCHAR>(&r), sizeof(r), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
            r = rand();
        return playmusic(musics[r % musics.size()], loops);
    }
    else {
        return playmusic(music, loops);
    }
}

Mix_Music* __cdecl my_Mix_LoadMUS(const char* file) {
    static bool inited = false;
    if (!inited) {
        find_musics();
        inited = true;
    }
    return loadmusic(file);
}

extern "C" __declspec(dllexport) void __cdecl abapatch(AbaPatchInfo * pi) {
    patchinfo = pi;
    if (!patchinfo)
        return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    openaudio = static_cast<Mix_OpenAudioFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_OpenAudio"));
    DetourAttach(&openaudio, my_Mix_OpenAudio);
    if (patchinfo->randbgm) {
        loadmusic = static_cast<Mix_LoadMUSFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_LoadMUS"));
        if (!loadmusic)
            return;
        playmusic = static_cast<Mix_PlayMusicFunc>(DetourFindFunction("SDL_mixer.dll", "Mix_PlayMusic"));
        if (!playmusic)
            return;
        DetourAttach(&loadmusic, my_Mix_LoadMUS);
        DetourAttach(&playmusic, my_Mix_PlayMusic);
    }
    DetourTransactionCommit();
}

extern "C" __declspec(dllexport) void __cdecl abaunpatch() {
    if (!patchinfo)
        return;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (playmusic)
        DetourDetach(&playmusic, my_Mix_PlayMusic);
    if (loadmusic)
        DetourDetach(&loadmusic, my_Mix_LoadMUS);
    if (openaudio)
        DetourDetach(&openaudio, my_Mix_OpenAudio);
    DetourTransactionCommit();
    free_musics();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

