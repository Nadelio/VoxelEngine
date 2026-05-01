# To Contributors

Hey, HUGE thanks for being interested enough to help me out!\
Here are a few constraints and some extra information you will need to help out!

## Toolchain and Dependencies:
- C++17
- Clang
- CMake
- Ninja
- OpenGL 3.3 Support
- GLAD (included in [include/](include/))
- SDL3 Runtime and Dev Kit (this should be handled by [CMakeLists.txt](CMakeLists.txt))
- GLM 1.0.3
- Dear ImGUI 1.92.7
- stb_image.h 2.30

## Compiling:
```bash
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Formatting Rules:
- Tab indention instead of spaces
    - I use 2 space-width tabs, so using spaces messes with it
- Stick to C++ naming convention
- Add doc comments to `.hpp` and `.h` files 
    - This helps when needing to reference your code later on without needing to interpret your code directly
- Local compilation needs to output no errors or warnings before PRs will be merged
    - Compilation checks also need to succeed with the `master` branch Github Actions
- Avoid adding dependencies without consulting me first
    - Preferrably via an issue
- Avoid editing `CMakeLists.txt`
    - Unless you can convince me you know what you are doing and can make it better
- Add any config files specific to your local environment to the [.gitignore](.gitignore)
- Do not edit [main.yml](.github/workflows/main.yml)
- Don't edit [README](README.md)
    - I will add your Github profile to the [Contributors section](README.md#contributors) of the [README](README.md)
- No AI-generated source code