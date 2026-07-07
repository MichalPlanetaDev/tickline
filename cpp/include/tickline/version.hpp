#pragma once

#include <string>
#include <string_view>

namespace tickline {

struct Version {
    int major;
    int minor;
    int patch;
    std::string_view channel;
};

[[nodiscard]] constexpr Version project_version() noexcept
{
    return Version{
        .major = 1,
        .minor = 0,
        .patch = 0,
        .channel = "",
    };
}

[[nodiscard]] std::string version_string();

}
