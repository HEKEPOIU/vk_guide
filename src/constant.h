#include "platform/windows.hpp"
#include <filesystem>

namespace engine_constant {

inline std::filesystem::path abs_exe_directory() {
#if defined(_MSC_VER)
  wchar_t path[FILENAME_MAX] = {0};
  GetModuleFileNameW(nullptr, path, FILENAME_MAX);
  return std::filesystem::path(path).parent_path();
#else
  char path[FILENAME_MAX];
  ssize_t count = readlink("/proc/self/exe", path, FILENAME_MAX);
  return std::filesystem::path(path).parent_path()
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
