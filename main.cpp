// c/c++ stdlib
#include <cstdio>
#include <string>
#include <memory>
#include <filesystem>
#include <type_traits>

// sdl
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

// imgui
#include <imgui.h>

// voxel engine
#include "AtlasTexture.hpp"
#include "BlockRegistry.hpp"
#include "Camera.hpp"
#include "DebugOverlay.hpp"
#include "Grid.hpp"
#include "Hotbar.hpp"
#include "Keybinds.hpp"
#include "Physics.hpp"
#include "PhysicsConstants.hpp"
#include "Shader.hpp"
#include "WorldFile.hpp"
#include "AppContext.hpp"
#include "AssetLoader.hpp"
#include "MenuSession.hpp"
#include "WorldSession.hpp"

/*
TODO:
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
*/

using namespace std::literals::string_view_literals;
namespace {
	std::string ResolveAssetPath(std::filesystem::path relativePath) {
		if(std::filesystem::is_regular_file(relativePath)) {
			return relativePath.string();
		}

		if(const char* const basePathRaw = SDL_GetBasePath()) {
			const std::filesystem::path basePath(basePathRaw);

			const std::array<std::filesystem::path, 3> prefixes = {
				""sv,
				"assets"sv,
				"../assets"sv,
			};

			for(const std::filesystem::path& prefix : prefixes) {
				const std::filesystem::path candidate = basePath / prefix / relativePath;
				if(std::filesystem::is_regular_file(candidate)) {
					return candidate.string();
				}
			}
		}

		return relativePath.string();
	}

	template<typename T, auto delfn>
	struct managed_ptr_helper {
		struct deleter {
			void operator()(T handle) const {
				if(handle) delfn(handle);
			}
		};
		static_assert(std::is_pointer_v<T>, "managed_ptr must manage a pointer type");
		using type = std::unique_ptr<std::remove_pointer_t<T>, deleter>;
	};
	template<typename T, auto delfn>
	using managed_ptr = typename managed_ptr_helper<T, delfn>::type;

	[[noreturn]] void quit(int code) {
		SDL_Quit();
		std::exit(code);
	}
}

int main() {
	// init SDL and OpenGL
	if(!SDL_Init(SDL_INIT_VIDEO)) {
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	if(!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) ||
		!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
		!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
		!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
		!SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) {
		std::fprintf(stderr, "SDL_GL_SetAttribute failed: %s\n", SDL_GetError());
		quit(1);
	}

	managed_ptr<SDL_Window*, SDL_DestroyWindow> window(
		SDL_CreateWindow("Voxel Engine", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
	if(!window) {
		std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		quit(1);
	}

	managed_ptr<SDL_GLContext, SDL_GL_DestroyContext> glContext{
		SDL_GL_CreateContext(window.get())};
	if(!glContext) {
		std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		quit(1);
	}

	if(!SDL_GL_SetSwapInterval(1)) {
		std::fprintf(stderr, "Warning: could not enable VSync: %s\n", SDL_GetError());
	}

	// grab  all the assets
	const std::string vertShaderPath          = ResolveAssetPath("assets/shaders/voxel.vert"sv);
	const std::string fragShaderPath          = ResolveAssetPath("assets/shaders/voxel.frag"sv);
	const std::string wireframeVertShaderPath = ResolveAssetPath("assets/shaders/wireframe.vert"sv);
	const std::string wireframeFragShaderPath = ResolveAssetPath("assets/shaders/wireframe.frag"sv);
	const std::string blockAtlasPNGPath       = ResolveAssetPath("assets/block_atlas.png"sv);
	const std::string itemAtlasPNGPath        = ResolveAssetPath("assets/item_atlas.png"sv);
	const std::string physicsConstantsPath    = ResolveAssetPath("assets/data/physics_constants.data"sv);
	const std::string blocksDataPath          = ResolveAssetPath("assets/data/blocks.data"sv);
	const std::string keybindsDataPath        = ResolveAssetPath("assets/data/keybinds.data"sv);

	// init default shader
	Shader defaultShader;
	if(!defaultShader.LoadFromFiles(vertShaderPath, fragShaderPath)) {
		std::fprintf(stderr, "Tried vertex shader at: %s\n", vertShaderPath.c_str());
		std::fprintf(stderr, "Tried fragment shader at: %s\n", fragShaderPath.c_str());
		std::fprintf(stderr, "Shader load failed.\n");
		quit(1);
	}

	// init wireframe shader
	Shader wireframeShader;
	if(!wireframeShader.LoadFromFiles(wireframeVertShaderPath, wireframeFragShaderPath)) {
		std::fprintf(stderr, "Tried wireframe vertex shader at: %s\n", wireframeVertShaderPath.c_str());
		std::fprintf(stderr, "Tried wireframe fragment shader at: %s\n", wireframeFragShaderPath.c_str());
		std::fprintf(stderr, "Wireframe shader load failed.\n");
		quit(1);
	}

	// init block atlas
	AtlasTexture blockAtlas;
	if(!blockAtlas.LoadFromFile(blockAtlasPNGPath)) {
		std::fprintf(stderr, "Tried block atlas PNG at: %s\n", blockAtlasPNGPath.c_str());
		std::fprintf(stderr, "Block atlas load failed. Add assets/block_atlas.png.\n");
		quit(1);
	}

	// init item atlas (currently unused)
	AtlasTexture itemAtlas;
	if(!itemAtlas.LoadFromFile(itemAtlasPNGPath)) {
		std::fprintf(stderr, "Tried item atlas PNG at: %s\n", itemAtlasPNGPath.c_str());
		std::fprintf(stderr, "Item atlas load failed. Add assets/item_atlas.png.\n");
		quit(1);
	}

	// init Dear ImGui debug overlay
	DebugOverlay debugOverlay;
	if(!debugOverlay.Initialize(window.get(), glContext.get())) {
		std::fprintf(stderr, "DebugOverlay (ImGui) initialization failed.\n");
		quit(1);
	}
	const ImGuiStyle imguiDefaultStyle = ImGui::GetStyle(); // snapshot clean defaults for scaling

	// init hotbar
	Hotbar hotbar;

	// load physics constants (use defaults if file not present)
	PhysicsConstants physicsConstants;
	if(!LoadPhysicsConstants(physicsConstantsPath, physicsConstants)) {
		std::fprintf(stderr, "Warning: could not load '%s', using defaults.\n", physicsConstantsPath.c_str());
	}

	// load keybinds (use defaults if file not present)
	Keybinds keybinds;
	if(!LoadKeybinds(keybindsDataPath, keybinds)) {
		std::fprintf(stderr, "Warning: could not load '%s', using defaults.\n", keybindsDataPath.c_str());
	}

	// init block registry from blocks.data
	BlockRegistry blockRegistry;
	if(!LoadBlocks(blocksDataPath, &blockAtlas, blockRegistry)) {
		std::fprintf(stderr, "Tried blocks.data at: %s\n", blocksDataPath.c_str());
		std::fprintf(stderr, "Block data load failed.\n");
		quit(1);
	}

	// get world dir
	const std::string worldsDir = [&]() -> std::string {
		const std::string rel = "worlds/";
		if(const char* base = SDL_GetBasePath())
			return std::string(base) + rel;
		return rel;
	}();

	// init grid
	Grid grid(&blockRegistry);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// initialize player (dimensions from physics constants)
	Physics::Entity player;
	player.radius = 0.30f;
	player.height = physicsConstants.standHeight;
	player.eyeFromFeet = physicsConstants.standEyeFromFeet;

	// runtime variables
	Camera camera;
	Physics physics(grid, blockRegistry);
	physics.SetConstants(physicsConstants);

	// init global context
	AppContext ctx;
	ctx.window = window.get();
	ctx.defaultShader = &defaultShader;
	ctx.wireframeShader = &wireframeShader;
	ctx.blockAtlas = &blockAtlas;
	ctx.blockRegistry = &blockRegistry;
	ctx.grid = &grid;
	ctx.physics = &physics;
	ctx.physicsConstants = &physicsConstants;
	ctx.camera = &camera;
	ctx.player = &player;
	ctx.hotbar = &hotbar;
	ctx.keybinds = &keybinds;
	ctx.debugOverlay = &debugOverlay;
	ctx.worldsDir = worldsDir;
	ctx.blocksDataPath = blocksDataPath;
	ctx.physicsConstantsPath = physicsConstantsPath;
	ctx.keybindsDataPath = keybindsDataPath;

	// init handlers
	WorldSession worldSession;
	MenuSession  menuSession;

	// main loop
	Uint64 previousCounter = SDL_GetPerformanceCounter();
	double fpsAccumulatedSeconds = 0.0;
	int fpsFrameCount = 0;
	int displayedFps = 0;
	int winWidth = 0;
	int winHeight = 0;

	while(true) {
		// fps calculation
		const Uint64 counter = SDL_GetPerformanceCounter();
		const double dt      = static_cast<double>(counter - previousCounter)
		                       / static_cast<double>(SDL_GetPerformanceFrequency());
		previousCounter = counter;
		fpsAccumulatedSeconds += dt;
		++fpsFrameCount;
		if(fpsAccumulatedSeconds >= 0.25) {
			displayedFps = static_cast<int>(
				static_cast<double>(fpsFrameCount) / fpsAccumulatedSeconds + 0.5);
			fpsAccumulatedSeconds -= 0.25;
			fpsFrameCount = 0;
		}

		worldSession.mouseDeltaX = 0.0f;
		worldSession.mouseDeltaY = 0.0f;

		// events
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			debugOverlay.ProcessEvent(event);

			if(event.type == SDL_EVENT_QUIT) { goto stop_mainloop; }

			if(event.type == SDL_EVENT_KEY_DOWN) {
				const bool* const kbs = SDL_GetKeyboardState(nullptr);
				if(ChordPressed(event.key.scancode, kbs, keybinds.quit)) { goto stop_mainloop; }
			}

			if(ctx.gameState == GameState::PLAYING || ctx.gameState == GameState::PAUSE_MENU) {
				worldSession.ProcessEvent(event, ctx);
			}

			if(event.type == SDL_EVENT_WINDOW_RESIZED) {
				winWidth  = event.window.data1;
				winHeight = event.window.data2;
			}
		}

		// sync window size and viewport
		SDL_GetWindowSize(window.get(), &winWidth, &winHeight);
		glViewport(0, 0, winWidth, winHeight);

		// scale ImGui proportionally to window height (900 px = 1x reference)
		if (winHeight > 0) {
			static float prevUiScale = -1.0f;
			const float uiScale = std::max(0.5f, std::min(2.0f,
			                          static_cast<float>(winHeight) / 900.0f));
			if (uiScale != prevUiScale) {
				ImGui::GetStyle() = imguiDefaultStyle; // restore clean baseline
				ImGui::GetStyle().ScaleAllSizes(uiScale);
				ImGuiStyle& s = ImGui::GetStyle();
				s.SeparatorSize          = std::max(1.0f, s.SeparatorSize);
				s.SeparatorTextBorderSize = std::max(1.0f, s.SeparatorTextBorderSize);
				s.WindowBorderSize       = std::max(1.0f, s.WindowBorderSize);
				s.PopupBorderSize        = std::max(1.0f, s.PopupBorderSize);
				ImGui::GetIO().FontGlobalScale = uiScale;
				prevUiScale = uiScale;
			}
		}

		// clear
		glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if(ctx.gameState == GameState::PLAYING || ctx.gameState == GameState::PAUSE_MENU) {
			if(worldSession.Frame(dt, displayedFps, winWidth, winHeight, ctx)) {
				menuSession.worldEntries.clear();
				menuSession.selectedIdx = -1;
			}
		} else {
			if(menuSession.Frame(winWidth, winHeight, ctx, worldSession)) {
				goto stop_mainloop;
			}
		}

		SDL_GL_SwapWindow(window.get());
	}

stop_mainloop:
	
	Grid::ReleaseSharedGLResources();
	return 0;
}
