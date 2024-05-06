#include "util.hpp"
#include <algorithm>
#include <ranges>

namespace util {

    bool is_newer_version(const std::string& lhs, const std::string& rhs)
    {
        if (lhs.empty() || rhs.empty() || lhs[0] != 'v' || rhs[0] != 'v') {
            throw std::exception("is_newer_version: bad arguments");
        }

        auto lhs_substrs = split_string(lhs.substr(1), '.');
        auto rhs_substrs = split_string(rhs.substr(1), '.');

        if (lhs_substrs.size() != 3 || rhs_substrs.size() != 3) {
            throw std::exception("is_newer_version: bad arguments");
        }

        std::vector<int> lhs_vec, rhs_vec;
        auto to_int = [&](const std::string& s) {
            return std::stoi(s);
            };
        std::ranges::transform(lhs_substrs, std::back_inserter(lhs_vec), to_int);
        std::ranges::transform(rhs_substrs, std::back_inserter(rhs_vec), to_int);

        for (size_t i = 0; i < lhs_vec.size(); i++)
        {
            if (lhs_vec[i] == rhs_vec[i]) {
                continue;
            }
            else {
                return lhs_vec[i] > rhs_vec[i];
            }
        }

        return false;
    }

    // empty substring will be ignored
    std::vector<std::string> split_string(const std::string& src, char delimiter)
    {
        std::vector<std::string> result;
        size_t begin = 0, end = 0;
        while (begin < src.length()) {
            end = src.find(delimiter, begin);
            if (end == src.npos) {
                result.push_back(src.substr(begin, src.npos));
                break;
            }
            if (end - begin > 0) {
                result.push_back(src.substr(begin, end - begin));
            }
            begin = end + 1;
        }

        return result;
    }
}