# To Contributors

Hey, HUGE thanks for being interested enough to help me out!\
Here are a few constraints and some extra information you will need to help out!

## Toolchain and Dependencies:
- C++17
- Clang
- CMake
- Ninja
- OpenGL 3.3 Support
- GLAD (included in [include/](include/) and [src/](src/))
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
- Do not edit [CONTRIBUTING.md](CONTRIBUTING.md)
	- I will update TODO list as part of merge request chores
- No AI-generated source code

## TODO:
- UI
	- Main menu
		- Settings
			- Controls
				- Edit keybinds
			- Texture pack
				- Load custom block_atlas.png and/or item_atlas.png
			- Resource pack
				- Load custom assets folder
			- Data pack
				- Load custom assets/data folder
	- Pause menu
		- Settings
	- Worlds menu
		- Rename world button
	- New world menu
		- Presets
			- Custom superflat
				- Choose blocks for each layer, choose how thick each layer is, choose how many layers there is
		- Data pack
			- Load custom assets/data folder
- Hand model
	- Held block models
	- Held item models
	- Interaction animations
		- Breaking block (swing hand)
		- Placing block (swing hand)
		- Picking block (point)
- Player model
	- Skins
	- Capes
	- Movement animations (first person and third person)
		- Walking animation
		- Sprinting animation
		- Jumping animation
		- Crouching animation
		- Crawling animation
- New blocks
	- Water
		- Fluids
	- Wood
		- Saplings
			- Tree/crop growth
		- Leaves
			- Decay
	- Clay
- Survival mode
	- Crafting
		- Crafting table
			- Block UI
			- Block interaction
	- Items
		- Dropped item and block models
		- Stone and Wood Tools
			- Pickaxe
			- Shovel
			- Axe
			- Hoe
		- Food
			- Dough
				- Flour
					- Bread
						- Wheat
							- Wheat seeds
						- Furnace
							- Block inventories
							- Block processing
				- Water
					- Water container
						- Clay
						- Item inventories
	- Hunger
	- Thirst
	- Health
		- Damage sources
			- Starvation
			- Dehydration
			- Drowning
			- Suffocating
			- Burning
				- Fire block
					- Blocks with no collision
					- Blocks that place blocks (fire spreading)
	- Mining
	- Farming
		- Use hoe to plow ground
		- Right click to place sapling or wheat seeds
		- Right click with dirt to cover
		- Right click with filled clay bowl to water
- UI
	- Add inventory texture
	- Add hotbar texture
	- Add crafting texture
	- Add furnace texture
	- Health, Hunger, Thirst UIs
		- Two layers
			- Empty hearts/hunger/thirst layer
			- Full/Half hearts/hunger/thirst layer