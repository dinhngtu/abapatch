#pragma once

#include <functional>

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
