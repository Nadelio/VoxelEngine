# To Contributors

Hey, HUGE thanks for being interested enough to help me out!\
Here are a few constraints and some extra information you will need to help out!

## Toolchain and Dependencies:
- C++17
- Clang 19.0.0 <=
- CMake
- Ninja
- OpenGL 3.3 Support
- GLAD (included in [include/](include/) and [src/](src/))
- SDL3 Runtime and Dev Kit (handled by [CMakeLists.txt](CMakeLists.txt))
- GLM 1.0.3
- stb_image.h 2.30
- Dear ImGUI 1.92.7 (handled by [CMakeLists.txt](CMakeLists.txt))
- TinyGTLF v2.9.0 (handled by [CMakeLists.txt](CMakeLists.txt))

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
- While preferred, it is not necessary to comment code in `.cpp` or `.c` files
    - UNLESS there isn't a matching `.hpp` or `.h` file for it, then please document the code :pray:
- Local compilation needs to output no errors or warnings before PRs will be merged
    - Compilation checks also need to succeed with the `master` branch Github Actions
- Avoid adding dependencies without consulting me first
    - Preferrably via an issue
- Avoid editing `CMakeLists.txt`
    - Unless you can convince me you know what you are doing and can make it better
- Add any config files specific to your local environment to the [.gitignore](.gitignore)
- Do not edit [main.yml](.github/workflows/main.yml)
- Do not edit [README](README.md)
    - I will add your Github profile to the [Contributors section](README.md#contributors) of the [README](README.md)
- Do not edit [CONTRIBUTING](CONTRIBUTING.md)
	- I will update TODO list as part of merge request chores
- No AI-generated source code

## TODO:
To see what currently needs to be done, review the `./TODO/` folder, prioritize the [BEFORE SURVIVAL list](./TODO/BEFORE%20SURVIVAL.md), then the [ANYTIME list](./TODO/ANYTIME.md).
Once both of those are finished, move on to the [SURVIVAL MODE list](./TODO/SURVIVAL%20MODE.md).