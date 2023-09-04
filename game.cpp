#include "game.hpp"
#include "gamepatch.hpp"

std::unique_ptr<GamePatch> Game::get_patch_priority(DWORD priorityClass) {
    return std::make_unique<PriorityPatch>(priorityClass);
}
