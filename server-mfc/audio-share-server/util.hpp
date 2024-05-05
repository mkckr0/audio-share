#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>
#include <vector>

namespace util
{
    bool is_newer_version(const std::string& lhs, const std::string& rhs);

    // empty substring will be removed
    std::vector<std::string> split_string(const std::string& src, char delimiter);
}

#endif // !UTIL_HPP
