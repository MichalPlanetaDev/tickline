#include "tickline/version.hpp"

#include <sstream>

namespace tickline {

std::string version_string()
{
    const auto version = project_version();

    std::ostringstream output;
    output << version.major << '.'
           << version.minor << '.'
           << version.patch;

    if (!version.channel.empty()) {
        output << '-' << version.channel;
    }

    return output.str();
}

}
