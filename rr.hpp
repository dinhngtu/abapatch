#pragma once

#include "game.hpp"
#include "gamepatch.hpp"

struct RRootage : public Game {
    static constexpr uint32_t entrypoint = 0x00401000;

    std::unique_ptr<GamePatch> get_patch_scale(float scale) override {
        std::vector<std::unique_ptr<GamePatch>> patches;
        patches.emplace_back(std::make_unique<MemoryPatch<uint32_t>>(0x40BD3Du - entrypoint, 640, static_cast<uint32_t>(640 * scale)));
        patches.emplace_back(std::make_unique<MemoryPatch<uint32_t>>(0x40BD47u - entrypoint, 480, static_cast<uint32_t>(480 * scale)));
        return std::make_unique<MultiPatch>(std::move(patches));
    }
    std::unique_ptr<GamePatch> get_patch_nobgm(bool nobgm) override {
        return std::make_unique<MemoryPatch<uint8_t>>(0x0040f4c0u - entrypoint, 0x55, 0xc3);
    }
};
