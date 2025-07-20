#include "vk_engine.h"
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vk_loader.h>

std::optional<std::vector<std::shared_ptr<MeshAsset>>>
loadGltfMeshes(VulkanEngine *engine, const std::filesystem::path &path) {
  fmt::println("Loading meshes from {}", path.string());

  fastgltf::GltfDataBuffer buffer;
  {
    auto dataResult = fastgltf::GltfDataBuffer::FromPath(path);
    if (!dataResult) {
      fmt::println("Failed to read glTF file buffer from: {}", path.string());
      return std::nullopt;
    }
    buffer = std::move(dataResult.get());
  }

  constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;

  fastgltf::Asset asset;
  fastgltf::Parser parser{};

  auto load = parser.loadGltf(buffer, path.parent_path(), gltfOptions);
  if (load) {
    asset = std::move(load.get());
  } else {
    fmt::println("Failed to load gltf: {}",
                 fastgltf::getErrorMessage(load.error()));
    return std::nullopt;
  }
  std::vector<std::shared_ptr<MeshAsset>> meshes;

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;
  for (auto &mesh : asset.meshes) {
    MeshAsset newMesh;
    newMesh.name = mesh.name;

    indices.clear();
    vertices.clear();

    for (auto &&p : mesh.primitives) {
      GeoSurface newSurface;
      newSurface.startIndex = (uint32_t)vertices.size();
      newSurface.count =
          (uint32_t)asset.accessors[p.indicesAccessor.value()].count;

      size_t initial_vtx = vertices.size();

      {
        fastgltf::Accessor &indexaccessor =
            asset.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);
        fastgltf::iterateAccessor<std::uint32_t>(
            asset, indexaccessor,
            [&](auto index) { indices.push_back(index); });
      }

      {
        fastgltf::Accessor &posAccessor =
            asset.accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, posAccessor, [&](glm::vec3 v, size_t index) {
              Vertex vertex;
              vertex.position = v;
              vertex.normal = {1, 0, 0};
              vertex.color = glm::vec4(1);
              vertex.uv_x = 0;
              vertex.uv_y = 0;
              vertices[initial_vtx + index] = vertex;
            });
      }
      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, asset.accessors[(*normals).accessorIndex],
            [&](glm::vec3 v, size_t index) {
              vertices[initial_vtx + index].normal = v;
            });
      }

      auto uv = p.findAttribute("TEXCOORD_0");
      if (uv != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset, asset.accessors[(*uv).accessorIndex],
            [&](glm::vec2 v, size_t index) {
              vertices[initial_vtx + index].uv_x = v.x;
              vertices[initial_vtx + index].uv_y = v.y;
            });
      }

      auto color = p.findAttribute("COLOR_0");
      if (color != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            asset, asset.accessors[(*color).accessorIndex],
            [&](glm::vec4 v, size_t index) {
              vertices[initial_vtx + index].color = v;
            });
      }
      newMesh.surfaces.push_back(newSurface);
    }

    constexpr bool OverrideColor = true;
    if (OverrideColor) {
      for (auto &vertex : vertices) {
        vertex.color = glm::vec4(vertex.normal, 1.f);
      }
    }
    newMesh.meshBuffer = engine->uploadMesh(indices, vertices);
    meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
  }
  return meshes;
}
