#include "tickline/version.hpp"

#include <cassert>
#include <string>

int main()
{
    constexpr auto version = tickline::project_version();

    static_assert(version.major == 0);
    static_assert(version.minor == 5);
    static_assert(version.patch == 0);
    static_assert(version.channel.empty());

    assert(tickline::version_string() == std::string{"0.5.0"});

    return 0;
}
