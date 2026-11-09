#pragma once
#ifndef _WIN32
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
namespace gui::nativeupdate {
const char* platform();
std::filesystem::path executable();
std::filesystem::path installedFolder();
std::filesystem::path cacheFolder();
// A managed install keeps mutable files outside immutable release payloads.
// Also holds a shared session lock until this process exits.
void configureRuntime();
bool verify(const std::filesystem::path& file,const std::string& digest,uint64_t size);
void launchOnExit(const std::filesystem::path& root,const std::filesystem::path& archive,
                  const std::string& digest,uint64_t size,const std::string& tag);
}
#endif
