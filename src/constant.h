#include "platform/windows.hpp"
#include "platform/unix.hpp"
#include <filesystem>

namespace engine_constant {

inline std::filesystem::path abs_exe_directory() {
#if defined(_WIN32)
  wchar_t path[FILENAME_MAX] = {0};
  GetModuleFileNameW(nullptr, path, FILENAME_MAX);
  return std::filesystem::path(path).parent_path();
#elif defined(__linux__) 
  char path[FILENAME_MAX];
  ssize_t count = readlink("/proc/self/exe", path, FILENAME_MAX);
  return std::filesystem::path(std::string(path, (count > 0) ? count : 0));
#elif defined(__APPLE__)
  char path[FILENAME_MAX];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    return std::filesystem::path(path).parent_path();
  }
  return {};

#endif
}

using namespace std::filesystem;
inline auto GetResourceRoot() -> path {
  static path const kResourceRoot = abs_exe_directory() / "resources";
  return kResourceRoot;
}

inline auto GetShaderRoot() -> path { return GetResourceRoot() / "shaders"; }

inline auto GetAssetRoot() -> path { return GetResourceRoot() / "assets"; }

} // namespace engine_constant
