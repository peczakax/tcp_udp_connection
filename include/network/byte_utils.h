#ifndef BYTE_UTILS_H
#define BYTE_UTILS_H

#include <string>
#include <vector>
#include <cstddef> // For std::byte

namespace NetworkUtils {
    // Helper function to convert string to vector of bytes
    inline std::vector<std::byte> StringToBytes(const std::string& str) {
        std::vector<std::byte> bytes;
        bytes.reserve(str.size());
        for (char c : str) {
            bytes.push_back(std::byte(c));
        }
        return bytes;
    }

    // Helper function to convert vector of bytes to string
    inline std::string BytesToString(const std::vector<std::byte>& bytes) {
        std::string str;
        str.reserve(bytes.size());
        for (std::byte b : bytes) {
            str.push_back(static_cast<char>(std::to_integer<int>(b)));
        }
        return str;
    }
    
    // Helper function to convert std::vector<char> to std::vector<std::byte>
    inline std::vector<std::byte> CharsToBytes(const std::vector<char>& chars) {
        std::vector<std::byte> bytes;
        bytes.reserve(chars.size());
        for (char c : chars) {
            bytes.push_back(std::byte(c));
        }
        return bytes;
    }

    // Helper function to convert std::vector<std::byte> to std::vector<char>
    inline std::vector<char> BytesToChars(const std::vector<std::byte>& bytes) {
        std::vector<char> chars;
        chars.reserve(bytes.size());
        for (std::byte b : bytes) {
            chars.push_back(static_cast<char>(std::to_integer<int>(b)));
        }
        return chars;
    }
}

#endif // BYTE_UTILS_H