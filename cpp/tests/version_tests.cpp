#include "tickline/version.hpp"

#include <cassert>
#include <string>

int main()
{
    constexpr auto version = tickline::project_version();

    static_assert(version.major == 0);
    static_assert(version.minor == 1);
    static_assert(version.patch == 0);
    static_assert(version.channel == "blueprint");

    assert(tickline::version_string() == std::string{"0.1.0-blueprint"});

    return 0;
}
