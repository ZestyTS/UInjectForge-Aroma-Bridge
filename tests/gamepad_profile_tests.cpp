#include "gamepad_profile.h"
#include <algorithm>
#include <cassert>
#include <cstdio>

using namespace uif::gamepad;
int main() {
    Profile profile;
    assert(profile.valid() && !profile.enabled);
    assert(profile.encode() == "1D0123456789ABCDEF");
    Profile decoded;
    assert(Profile::decode(profile.encode(), decoded) && decoded.encode() == profile.encode());
    for (auto invalid : {"", "1E", "2E0123456789ABCDEF", "1X0123456789ABCDEF", "1E0123456789ABCDEe",
                         "1E0123456789ABCDEG", "1E0123456789ABCDE0", "1E0123456789ABCDEF0"}) {
        decoded.enabled = true;
        const auto before = decoded.encode();
        assert(!Profile::decode(invalid, decoded) && decoded.encode() == before);
    }
    auto invalid = profile;
    invalid.enabled = true;
    invalid.destinations[0] = 16;
    assert(!invalid.valid() && invalid.map(0xFFFFFFFF) == 0xFFFFFFFF && invalid.encode().empty());
    invalid.destinations[0] = 1;
    assert(!invalid.valid());
    assert(application_title(0x0005000012345678ULL));
    assert(application_title(0x00050002F1234567ULL));
    for (auto title : {0ULL, 0x0005000000000000ULL, 0x0005001010040000ULL, 0x0005000E12345678ULL,
                       0x0005000C12345678ULL, 0x0000000012345678ULL}) assert(!application_title(title));

    profile.enabled = true;
    std::rotate(profile.destinations.begin(), profile.destinations.begin() + 3, profile.destinations.end());
    assert(profile.valid());
    const uint32_t protected_bits = ~button_mask;
    for (uint32_t subset = 0; subset < 65536; ++subset) {
        uint32_t input = protected_bits, expected = protected_bits;
        for (size_t i = 0; i < buttons.size(); ++i) if (subset & (1U << i)) {
            input |= buttons[i]; expected |= buttons[profile.destinations[i]];
        }
        assert(profile.map(input) == expected);
        assert((profile.map(input) & protected_bits) == protected_bits);
    }

    Mapper mapper;
    assert(!mapper.needs_processing());
    mapper.set(Profile{});
    assert(!mapper.needs_processing());
    mapper.set(profile);
    assert(mapper.needs_processing());
    auto sample = mapper.apply({buttons[0] | 2, buttons[0] | 2, 1});
    assert(sample.hold == 2 && sample.trigger == 2 && sample.release == 1);
    sample = mapper.apply({0, 0, buttons[0]});
    assert(sample.hold == 0 && sample.release == 0);
    sample = mapper.apply({buttons[0], buttons[0], buttons[1] | 0x10000});
    assert(sample.hold == buttons[3] && sample.trigger == buttons[3]);
    assert(sample.release == (buttons[4] | 0x10000));
    mapper.set(Profile{});
    assert(mapper.needs_processing());
    sample = mapper.apply({buttons[0], 0, 2});
    assert(sample.hold == 0 && sample.release == (buttons[3] | 2));
    sample = mapper.apply({buttons[0], 0, 0});
    assert(sample.hold == 0 && sample.release == 0);
    mapper.apply({0, 0, buttons[0]});
    assert(!mapper.needs_processing());
    mapper.reset();
    assert(!mapper.needs_processing());
    mapper.observe_passthrough(buttons[0] | 2);
    assert(!mapper.needs_processing());
    mapper.set(profile);
    sample = mapper.apply({buttons[0] | 2, 0, 0});
    assert(sample.hold == 2 && sample.release == buttons[0]);
    sample = mapper.apply({buttons[0], 0, 0});
    assert(sample.hold == 0 && sample.release == 0);
    mapper.apply({0, 0, buttons[0]});
    sample = mapper.apply({buttons[0], buttons[0], 0});
    assert(sample.hold == buttons[3] && sample.trigger == buttons[3]);
    std::puts("GamePad profiles: exhaustive 65536 masks, strict storage, title isolation, protected bits and neutral transitions PASS.");
}
