#pragma once
#include <cstdint>

using EntityID = std::uint32_t;

struct Entity {
    EntityID id{};
    explicit operator bool() const { return id != 0; }
};
