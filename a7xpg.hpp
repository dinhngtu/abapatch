#pragma once

#include "game.hpp"
#include "gamepatch.hpp"

struct A7xpg : public Game {
    static constexpr uint32_t entrypoint = 0x004110b4;

    std::unique_ptr<GamePatch> get_patch_scale(float scale) override {
        std::vector<std::unique_ptr<GamePatch>> patches;
        patches.emplace_back(std::make_unique<MemoryPatch<uint32_t>>(0x00523c70u - entrypoint, 640, static_cast<uint32_t>(640 * scale)));
        patches.emplace_back(std::make_unique<MemoryPatch<uint32_t>>(0x00523c74u - entrypoint, 480, static_cast<uint32_t>(480 * scale)));
        return std::make_unique<MultiPatch>(std::move(patches));
    }
    /*
    std::unique_ptr<GamePatch> get_patch_lwscale(float lwscale) override {
        std::vector<std::unique_ptr<GamePatch>> patches;
        patches.emplace_back(std::make_unique<MemoryPatch<float>>(0x00403adfu - entrypoint, 1.0f, lwscale));
        // inGameDrawLuminous does use linewidth but don't patch these
        //patches.emplace_back(std::make_unique<MemoryPatch<float>>(0x00403276u - entrypoint, 2.0f, 2.0f * lwscale));
        //patches.emplace_back(std::make_unique<MemoryPatch<float>>(0x00403294u - entrypoint, 1.0f, lwscale));
        return std::make_unique<MultiPatch>(std::move(patches));
    }
    */
    std::unique_ptr<GamePatch> get_patch_nosmooth(bool nosmooth) override {
        return std::make_unique<MemoryPatch<uint32_t>>(0x00403aee - entrypoint, 0x0001a1b4, 0x0001a1a2);
    }
    std::unique_ptr<GamePatch> get_patch_nobgm(bool nobgm) override {
        // lazy patch
        return std::make_unique<MemoryPatch<uint8_t>>(0x0040a8e0u - entrypoint, 0x50, 0xc3);
    }
};
