#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace uif::gamepad {
inline constexpr std::array<uint32_t, 16> buttons = {
    0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100,
    0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x00040000, 0x00020000};
inline constexpr uint32_t button_mask = 0x0006FFFC;

struct Profile {
    bool enabled = false;
    std::array<uint8_t, 16> destinations = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

    bool valid() const {
        uint32_t seen = 0;
        for (auto destination : destinations) {
            if (destination >= buttons.size() || (seen & (1U << destination))) return false;
            seen |= 1U << destination;
        }
        return seen == 0xFFFF;
    }

    uint32_t map(uint32_t value) const {
        if (!enabled || !valid()) return value;
        auto output = value & ~button_mask;
        for (size_t i = 0; i < buttons.size(); ++i)
            if (value & buttons[i]) output |= buttons[destinations[i]];
        return output;
    }

    std::string encode() const {
        if (!valid()) return {};
        std::string result = enabled ? "1E" : "1D";
        for (auto destination : destinations) result += "0123456789ABCDEF"[destination];
        return result;
    }

    static bool decode(std::string_view text, Profile &output) {
        if (text.size() != 18 || text[0] != '1' || (text[1] != 'E' && text[1] != 'D')) return false;
        Profile parsed;
        parsed.enabled = text[1] == 'E';
        for (size_t i = 0; i < parsed.destinations.size(); ++i) {
            const auto value = std::string_view("0123456789ABCDEF").find(text[i + 2]);
            if (value == std::string_view::npos) return false;
            parsed.destinations[i] = static_cast<uint8_t>(value);
        }
        if (!parsed.valid()) return false;
        output = parsed;
        return true;
    }
};

inline bool application_title(uint64_t title) {
    const auto category = static_cast<uint32_t>(title >> 32);
    return (category == 0x00050000 || category == 0x00050002) && static_cast<uint32_t>(title) != 0;
}

struct Buttons { uint32_t hold, trigger, release; };

// Switching profiles releases the previously delivered keys and waits for neutral.
// Analog, touch, motion, system keys and virtual stick bits are never modified.
class Mapper {
    Profile profile;
    uint32_t previous = 0;
    bool armed = false, flush = false, transitioning = false;
public:
    void set(const Profile &next) {
        const auto was_active = profile.enabled || previous != 0 || transitioning;
        profile = next.valid() ? next : Profile{};
        armed = false;
        flush = was_active;
        transitioning = was_active || profile.enabled;
    }
    bool needs_processing() const { return profile.enabled || transitioning; }
    void observe_passthrough(uint32_t held) {
        if (!needs_processing()) previous = held & button_mask;
    }
    void reset() { profile = {}; previous = 0; armed = false; flush = false; transitioning = false; }
    Buttons apply(Buttons sample) {
        const auto release = flush ? previous : 0;
        flush = false;
        if (!armed) {
            const bool neutral = ((sample.hold | sample.trigger) & button_mask) == 0;
            sample.hold &= ~button_mask;
            sample.trigger &= ~button_mask;
            sample.release = (sample.release & ~button_mask) | release;
            previous = 0;
            armed = neutral;
            if (neutral) transitioning = false;
            return sample;
        }
        sample = {profile.map(sample.hold), profile.map(sample.trigger), profile.map(sample.release)};
        previous = sample.hold & button_mask;
        return sample;
    }
};
}
