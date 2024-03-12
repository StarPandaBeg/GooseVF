#include "Utility.h"

#include <sstream>

#include "pch.h"

std::vector<std::string> GooseVF::splitPath(const std::string& s) {
    std::stringstream stream(s);
    std::string segment;
    std::vector<std::string> parts;

    while (std::getline(stream, segment, '\\')) {
        parts.push_back(segment);
    }
    return parts;
}

std::string GooseVF::buildPath(const std::vector<std::string>& s) {
    std::stringstream stream;
    auto it = s.begin();
    stream << *it++;
    for (; it != s.end(); it++) {
        stream << '\\';
        stream << *it;
    }
    return stream.str();
}
