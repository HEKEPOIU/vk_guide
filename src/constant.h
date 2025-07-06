#include <filesystem>
namespace engine_constant {

using namespace std::filesystem;
inline auto GetResourceRoot() -> path {
  static path const kResourceRoot = path(RESOURCEROOT);
  return kResourceRoot;
}

inline auto GetShaderRoot() -> path { return GetResourceRoot() / "shaders"; }

inline auto GetAssetRoot() -> path { return GetResourceRoot() / "assets"; }

} // namespace engine_constant
