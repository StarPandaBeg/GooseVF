#pragma once

#include <string>
#include <vector>

namespace GooseVF {
    std::vector<std::string> splitPath(const std::string& s);
    std::string buildPath(const std::vector<std::string>& s);
}  // namespace GooseVF