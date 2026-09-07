#pragma once

// Public API snapshot of Demons In Between 1.7.1.
// Kept locally because Geode's dependency build step installs the binary
// dependency but does not add API headers to this target's include path.
// The upstream project publishes this header under its API include directory.

#include <Geode/loader/Event.hpp>
#include <map>

struct LadderDemon {
    int id = 0;
    double tier = 0.0;
    double enjoyment = 0.0;
    int difficulty = 0;
};

class DemonsInBetweenLoadedEvent : public geode::Event<DemonsInBetweenLoadedEvent, bool(const std::map<int, LadderDemon>&)> {
public:
    using Event::Event;
};
