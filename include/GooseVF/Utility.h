#pragma once

#include <string>
#include <vector>

#define ENTRYDATA_TYPE_FILE 0
#define ENTRYDATA_TYPE_DIR 1
#define BYTE unsigned char

namespace GooseVF {
    std::vector<std::string> splitPath(const std::string& s);
    std::string buildPath(const std::vector<std::string>& s);
}  // namespace GooseVF