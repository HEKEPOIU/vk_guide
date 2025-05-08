find_package(Vulkan REQUIRED)
include(FetchContent)

FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-2.32.6
    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/SDL
)
FetchContent_Declare(
    vkbootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v1.4.314
    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/vkbootstrap
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY	https://github.com/g-truc/glm.git
    GIT_TAG 	1.0.1
    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/glm
)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY	https://github.com/fmtlib/fmt.git
    GIT_TAG 	11.2.0
    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/fmt
)

FetchContent_Declare(
    vma
    GIT_REPOSITORY  https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.2.1
    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/vma
)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY  https://github.com/spnda/fastgltf.git
    GIT_TAG v0.8.0
    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/fastgltf
)

FetchContent_MakeAvailable(SDL vkbootstrap glm fmt vma fastgltf)


add_library(stb_image INTERFACE)

#add_library(sdl2 INTERFACE)

#target_include_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Include/SDL2 )
#target_link_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Lib )

#target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)
set_property(TARGET imgui PROPERTY CXX_STANDARD 20)


target_sources(imgui PRIVATE
  imgui/imgui.h
  imgui/imgui.cpp

  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_impl_vulkan.cpp
  imgui/imgui_impl_sdl2.cpp
  )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)

target_include_directories(stb_image INTERFACE stb_image)
