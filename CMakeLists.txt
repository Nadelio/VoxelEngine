cmake_minimum_required(VERSION 3.20)
project(VoxelEngine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

if(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(VOXEL_USER_HOME "$ENV{HOME}")
elseif(WIN32 AND DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
    set(VOXEL_USER_HOME "$ENV{USERPROFILE}")
else()
    set(VOXEL_USER_HOME "")
endif()

set(VOXEL_LIBRARIES_ROOT "${VOXEL_USER_HOME}/Libraries" CACHE PATH "Base folder containing third-party libraries")

find_package(OpenGL REQUIRED)

find_package(SDL3 CONFIG QUIET)

if(NOT TARGET SDL3::SDL3)
    set(SDL3_ROOT "${VOXEL_LIBRARIES_ROOT}/SDL" CACHE PATH "Path to local SDL3 SDK root")
    set(SDL3_LINK_TARGET "")
    set(SDL3_SOURCE_ROOT "${SDL3_ROOT}")

    if(NOT (EXISTS "${SDL3_SOURCE_ROOT}/CMakeLists.txt" AND EXISTS "${SDL3_SOURCE_ROOT}/include/SDL3/SDL.h"))
        file(GLOB SDL3_CHILD_CANDIDATES LIST_DIRECTORIES true "${SDL3_ROOT}/SDL3*")
        foreach(SDL3_CHILD_DIR IN LISTS SDL3_CHILD_CANDIDATES)
            if(EXISTS "${SDL3_CHILD_DIR}/CMakeLists.txt" AND EXISTS "${SDL3_CHILD_DIR}/include/SDL3/SDL.h")
                set(SDL3_SOURCE_ROOT "${SDL3_CHILD_DIR}")
                break()
            endif()
        endforeach()
    endif()

    if(EXISTS "${SDL3_SOURCE_ROOT}/CMakeLists.txt" AND EXISTS "${SDL3_SOURCE_ROOT}/include/SDL3/SDL.h")
        add_subdirectory("${SDL3_SOURCE_ROOT}" "${CMAKE_BINARY_DIR}/_deps/sdl3-build")

        if(TARGET SDL3::SDL3)
            set(SDL3_LINK_TARGET SDL3::SDL3)
        elseif(TARGET SDL3-shared)
            set(SDL3_LINK_TARGET SDL3-shared)
        elseif(TARGET SDL3-static)
            set(SDL3_LINK_TARGET SDL3-static)
        endif()
    endif()

    if(NOT SDL3_LINK_TARGET)
        find_path(SDL3_INCLUDE_DIR
            NAMES SDL3/SDL.h
            PATHS
                "${SDL3_ROOT}/include"
        )

        find_library(SDL3_LIBRARY
            NAMES SDL3 SDL3-static
            PATHS
                "${SDL3_ROOT}/lib"
                "${SDL3_ROOT}/lib/x64"
        )

        if(SDL3_INCLUDE_DIR AND SDL3_LIBRARY)
            add_library(SDL3::SDL3 UNKNOWN IMPORTED)
            set_target_properties(SDL3::SDL3 PROPERTIES
                IMPORTED_LOCATION "${SDL3_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIR}"
            )
            set(SDL3_LINK_TARGET SDL3::SDL3)
        endif()
    endif()

    if(NOT SDL3_LINK_TARGET)
        message(FATAL_ERROR
            "SDL3 was not found. Use vcpkg, provide an SDL3 SDK, or point SDL3_ROOT to an SDL3 source tree.\n"
            "Current SDL3_ROOT='${SDL3_ROOT}'."
        )
    endif()
else()
    set(SDL3_LINK_TARGET SDL3::SDL3)
endif()

add_executable(VoxelEngine
    main.cpp
    src/AtlasTexture.cpp
    src/Camera.cpp
    src/Chunk.cpp
    src/CubeMesh.cpp
    src/DataFormat.cpp
    src/DebugOverlay.cpp
    src/Grid.cpp
    src/Hotbar.cpp
    src/Physics.cpp
    src/Shader.cpp
)

set(GLM_ROOT "${VOXEL_LIBRARIES_ROOT}/glm-1.0.3" CACHE PATH "Path to glm root folder")
set(STB_ROOT "${VOXEL_LIBRARIES_ROOT}/stb_image" CACHE PATH "Path to stb root folder")

if(NOT EXISTS "${GLM_ROOT}/glm/glm.hpp")
    message(FATAL_ERROR "glm not found at '${GLM_ROOT}'. Expected ${GLM_ROOT}/glm/glm.hpp")
endif()

if(NOT EXISTS "${STB_ROOT}/stb_image.h")
    message(FATAL_ERROR "stb_image.h not found at '${STB_ROOT}'.")
endif()

FetchContent_Declare(
    imgui_src
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.7.zip
)
FetchContent_MakeAvailable(imgui_src)

set(IMGUI_REQUIRED_SOURCES
    "${imgui_src_SOURCE_DIR}/imgui.cpp"
    "${imgui_src_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_src_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_src_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_src_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)

foreach(IMGUI_SOURCE IN LISTS IMGUI_REQUIRED_SOURCES)
    if(NOT EXISTS "${IMGUI_SOURCE}")
        message(FATAL_ERROR "Missing Dear ImGui source file: ${IMGUI_SOURCE}")
    endif()
endforeach()

add_library(imgui STATIC
    ${IMGUI_REQUIRED_SOURCES}
)

target_include_directories(imgui PUBLIC
    "${imgui_src_SOURCE_DIR}"
    "${imgui_src_SOURCE_DIR}/backends"
)

target_link_libraries(imgui PUBLIC
    ${SDL3_LINK_TARGET}
    OpenGL::GL
)

target_include_directories(VoxelEngine PRIVATE
    include
    ${GLM_ROOT}
    ${STB_ROOT}
)

target_link_libraries(VoxelEngine PRIVATE ${SDL3_LINK_TARGET} OpenGL::GL)
target_link_libraries(VoxelEngine PRIVATE imgui)

if(WIN32)
    set(SDL3_RUNTIME_DLL "${VOXEL_LIBRARIES_ROOT}/SDL/SDL3.dll" CACHE FILEPATH "Path to SDL3.dll for standalone/runtime copy")
elseif(UNIX AND NOT APPLE)
    set(SDL3_RUNTIME_SO "${VOXEL_LIBRARIES_ROOT}/SDL/libSDL3.so" CACHE FILEPATH "Path to libSDL3.so for standalone/runtime copy")
    set_target_properties(VoxelEngine PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()

if(WIN32)
    if(EXISTS "${SDL3_RUNTIME_DLL}")
        add_custom_command(TARGET VoxelEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL3_RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:VoxelEngine>/SDL3.dll"
            COMMENT "Copying SDL3.dll to output directory"
        )
    else()
        message(STATUS "SDL3 runtime DLL not found at '${SDL3_RUNTIME_DLL}'.")
        message(STATUS "If needed, set -DSDL3_RUNTIME_DLL=/full/path/to/SDL3.dll")
    endif()
elseif(UNIX AND NOT APPLE)
    if(EXISTS "${SDL3_RUNTIME_SO}")
        add_custom_command(TARGET VoxelEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL3_RUNTIME_SO}"
                "$<TARGET_FILE_DIR:VoxelEngine>/libSDL3.so"
            COMMENT "Copying libSDL3.so to output directory"
        )
    else()
        message(STATUS "SDL3 runtime SO not found at '${SDL3_RUNTIME_SO}'.")
        message(STATUS "If needed, set -DSDL3_RUNTIME_SO=/full/path/to/libSDL3.so")
    endif()
endif()

add_custom_command(TARGET VoxelEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:VoxelEngine>/assets"
    COMMENT "Copying assets directory to output directory"
)

set(STANDALONE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/standalone/$<CONFIG>")
add_custom_target(standalone
    COMMAND ${CMAKE_COMMAND} -E make_directory "${STANDALONE_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:VoxelEngine>"
        "${STANDALONE_OUTPUT_DIR}/$<TARGET_FILE_NAME:VoxelEngine>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "${STANDALONE_OUTPUT_DIR}/assets"
    DEPENDS VoxelEngine
    COMMENT "Creating standalone runtime folder"
)

if(WIN32 AND EXISTS "${SDL3_RUNTIME_DLL}")
    add_custom_command(TARGET standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_RUNTIME_DLL}"
            "${STANDALONE_OUTPUT_DIR}/SDL3.dll"
        COMMENT "Adding SDL3.dll to standalone runtime folder"
    )
elseif(UNIX AND NOT APPLE AND EXISTS "${SDL3_RUNTIME_SO}")
    add_custom_command(TARGET standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL3_RUNTIME_SO}"
            "${STANDALONE_OUTPUT_DIR}/libSDL3.so"
        COMMENT "Adding libSDL3.so to standalone runtime folder"
    )
endif()

install(TARGETS VoxelEngine
    RUNTIME DESTINATION .
)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION .)

if(WIN32 AND EXISTS "${SDL3_RUNTIME_DLL}")
    install(FILES "${SDL3_RUNTIME_DLL}" DESTINATION . RENAME SDL3.dll)
elseif(UNIX AND NOT APPLE AND EXISTS "${SDL3_RUNTIME_SO}")
    install(FILES "${SDL3_RUNTIME_SO}" DESTINATION . RENAME libSDL3.so)
endif()

set(CPACK_PACKAGE_NAME "VoxelEngine")
set(CPACK_PACKAGE_VENDOR "VoxelEngine")
set(CPACK_PACKAGE_VERSION "0.1.0")
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)

set(VOXEL_WINDOWS_TOOLCHAIN_FILE "" CACHE FILEPATH "Toolchain file used to cross-compile Windows build when host is not Windows")
set(VOXEL_LINUX_TOOLCHAIN_FILE "" CACHE FILEPATH "Toolchain file used to cross-compile Linux build when host is not Linux")

set(VOXEL_WINDOWS_BUILD_DIR "${CMAKE_BINARY_DIR}/cross/windows")
set(VOXEL_LINUX_BUILD_DIR "${CMAKE_BINARY_DIR}/cross/linux")

if(WIN32)
    add_custom_target(standalone-windows
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config Release --target standalone
        COMMENT "Building standalone Windows bundle"
    )
else()
    if(NOT VOXEL_WINDOWS_TOOLCHAIN_FILE)
        add_custom_target(standalone-windows
            COMMAND ${CMAKE_COMMAND} -E echo "Error: VOXEL_WINDOWS_TOOLCHAIN_FILE is required to build Windows from a non-Windows host."
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "Windows cross-build unavailable without toolchain"
        )
    else()
        add_custom_target(standalone-windows
            COMMAND ${CMAKE_COMMAND} -S "${CMAKE_SOURCE_DIR}" -B "${VOXEL_WINDOWS_BUILD_DIR}"
                -DCMAKE_TOOLCHAIN_FILE:FILEPATH="${VOXEL_WINDOWS_TOOLCHAIN_FILE}"
                -DCMAKE_SYSTEM_NAME=Windows
                -DVOXEL_WINDOWS_TOOLCHAIN_FILE:FILEPATH="${VOXEL_WINDOWS_TOOLCHAIN_FILE}"
                -DVOXEL_LINUX_TOOLCHAIN_FILE:FILEPATH="${VOXEL_LINUX_TOOLCHAIN_FILE}"
                -DCMAKE_BUILD_TYPE=Release
            COMMAND ${CMAKE_COMMAND} --build "${VOXEL_WINDOWS_BUILD_DIR}" --config Release --target standalone
            COMMENT "Cross-building standalone Windows bundle"
        )
    endif()
endif()

if(UNIX AND NOT APPLE)
    add_custom_target(standalone-linux
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config Release --target standalone
        COMMENT "Building standalone Linux bundle"
    )
else()
    if(NOT VOXEL_LINUX_TOOLCHAIN_FILE)
        add_custom_target(standalone-linux
            COMMAND ${CMAKE_COMMAND} -E echo "Error: VOXEL_LINUX_TOOLCHAIN_FILE is required to build Linux from a non-Linux host."
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "Linux cross-build unavailable without toolchain"
        )
    else()
        add_custom_target(standalone-linux
            COMMAND ${CMAKE_COMMAND} -S "${CMAKE_SOURCE_DIR}" -B "${VOXEL_LINUX_BUILD_DIR}"
                -DCMAKE_TOOLCHAIN_FILE:FILEPATH="${VOXEL_LINUX_TOOLCHAIN_FILE}"
                -DCMAKE_SYSTEM_NAME=Linux
                -DVOXEL_WINDOWS_TOOLCHAIN_FILE:FILEPATH="${VOXEL_WINDOWS_TOOLCHAIN_FILE}"
                -DVOXEL_LINUX_TOOLCHAIN_FILE:FILEPATH="${VOXEL_LINUX_TOOLCHAIN_FILE}"
                -DCMAKE_BUILD_TYPE=Release
            COMMAND ${CMAKE_COMMAND} --build "${VOXEL_LINUX_BUILD_DIR}" --config Release --target standalone
            COMMENT "Cross-building standalone Linux bundle"
        )
    endif()
endif()

add_custom_target(standalone-all-platforms
    DEPENDS standalone-windows standalone-linux
    COMMENT "Building standalone bundles for Windows and Linux"
)

if(MSVC)
    target_compile_options(VoxelEngine PRIVATE /W4 /permissive-)
else()
    target_compile_options(VoxelEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND WIN32)
    target_compile_definitions(VoxelEngine PRIVATE _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
endif()
