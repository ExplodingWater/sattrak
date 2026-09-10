#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <filesystem>

namespace aero::utils {

/**
 * @brief Returns current system date formatted as YYYY-MM-DD.
 */
inline std::string getDateString() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm tmStruct{};
#if defined(_WIN32)
    localtime_s(&tmStruct, &nowTime);
#else
    localtime_r(&nowTime, &tmStruct);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmStruct, "%Y-%m-%d");
    return oss.str();
}

/**
 * @brief Creates directory and any parent directories if they do not exist.
 */
inline void ensureDirectoryExists(const std::string& dirPath) {
    std::filesystem::create_directories(dirPath);
}

} // namespace aero::utils
