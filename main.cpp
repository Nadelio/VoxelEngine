// c/c++ stdlib
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <iostream>
#include <filesystem>
#include <type_traits>

// sdl
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

// glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

// voxel engine
#include "AtlasTexture.hpp"
#include "BlockRegistry.hpp"
#include "CubeMesh.hpp"
#include "Grid.hpp"
#include "Camera.hpp"
#include "Physics.hpp"
#include "PhysicsConstants.hpp"
#include "Shader.hpp"
#include "DebugOverlay.hpp"
#include "Hotbar.hpp"

/*
TODO:
- Refactor UI to use Dear ImGUI
	- Reformat debug overlay to have block data span multiple lines instead of a single long line
- Rendering
	- [BUG] Greedy meshing doesn't run every frame/tick, so wireframe view doesn't update all the time (placing/removing blocks doesn't update greedy mesh immediately)
- Formalize data format
	- numeric ranges (n..m)
	- typed arrays (<type>[...])
	- strings ("...")
	- integers
	- floats
	- booleans
	- identifiers ([A-Za-z]+)
	- key-value pairs ({ <key> : <value> })
- Terrain Generation
	- Saving maps to file (name.world)
		- need to create compact data format for world save files
			- `<blockID> <position> <block instance data>` maybe?
	- Is data driven (external data files) to determine terrain generation
	- Utilizes the blocks.data information
		- Add terrain generation tags to each block to help determine where they should be used in generation
			- Temperature (float range)
			- Elevation (integer range)
			- World Depth (integer range)
			- Biome (string array)
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

	// Parses blocks.data and registers each block into registry.
	// Returns false if the file cannot be opened or a registration fails.
	bool LoadBlocks(const std::string& path, const AtlasTexture* atlas, BlockRegistry& registry) {
		std::ifstream file(path);
		if(!file.is_open()) {
			return false;
		}

		std::string line;
		while(std::getline(file, line)) {
			if(line.empty() || line[0] == '#') { continue; }
			std::istringstream iss(line);
			std::string keyword;
			if(!(iss >> keyword) || keyword != "block") { continue; }

			uint32_t id = 0;
			std::string name;
			std::string gravStr;
			if(!(iss >> id >> name >> gravStr)) { continue; }

			const bool affectedByGravity = (gravStr == "true");

			FaceTileMap ftm{};
			bool valid = true;
			for(int i = 0; i < 6; ++i) {
				int fx = 0, fy = 0;
				if(!(iss >> fx >> fy)) { valid = false; break; }
				ftm[i] = FaceTile{fx, fy};
			}
			if(!valid) { continue; }

			if(!registry.Register({id, name, ftm, atlas, affectedByGravity})) {
				std::fprintf(stderr, "Block registration failed for ID %u (%s).\n", id, name.c_str());
				return false;
			}
		}
		return true;
	}
	

	template<typename T,auto delfn>
	struct managed_ptr_helper {
		struct deleter {
			void operator()(T handle) const{
				if(handle) delfn(handle);
			}
		};
		static_assert(std::is_pointer_v<T>,"managed_ptr must manage a pointer type");
		using type = std::unique_ptr<std::remove_pointer_t<T>, deleter>;
	};
	template<typename T,auto delfn>
	using managed_ptr = typename managed_ptr_helper<T,delfn>::type;


	[[noreturn]] void quit(int code){
		SDL_Quit();
		std::exit(code);
	}
}

int main() {

	std::cout << "TODO:\n"
			  << "- Refactor UI to use Dear ImGUI\n"
			  << "\t- Reformat debug overlay to have block data span multiple lines instead of a single long line\n"
			  << "- Terrain Generation\n"
			  << "\t- Saving maps to file\n"
			  << "- Rendering\n"
			  << "\t- [BUG] Greedy meshing doesn't run every frame/tick or wireframe only view doesn't update meshes every frame/tick\n";

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

	managed_ptr<SDL_Window*, SDL_DestroyWindow> window(SDL_CreateWindow("Voxel Engine", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE));
	if(!window) {
		std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		quit(1);
	}

	managed_ptr<SDL_GLContext, SDL_GL_DestroyContext> glContext{SDL_GL_CreateContext(window.get())};
	if(!glContext) {
		std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		quit(1);
	}

	if(!SDL_GL_SetSwapInterval(1)) {
		std::fprintf(stderr, "Warning: could not enable VSync: %s\n", SDL_GetError());
	}

	if(!SDL_SetWindowRelativeMouseMode(window.get(), true)) {
		std::fprintf(stderr, "Warning: could not enable relative mouse mode: %s\n", SDL_GetError());
	}

	// grab all the assets
	const std::string vertShaderPath = ResolveAssetPath("assets/shaders/voxel.vert"sv);
	const std::string fragShaderPath = ResolveAssetPath("assets/shaders/voxel.frag"sv);
	const std::string uiVertShaderPath = ResolveAssetPath("assets/shaders/ui.vert"sv);
	const std::string uiFragShaderPath = ResolveAssetPath("assets/shaders/ui.frag"sv);
	const std::string wireframeVertShaderPath = ResolveAssetPath("assets/shaders/wireframe.vert"sv);
	const std::string wireframeFragShaderPath = ResolveAssetPath("assets/shaders/wireframe.frag"sv);
	const std::string hotbarVertShaderPath = ResolveAssetPath("assets/shaders/hotbar.vert"sv);
	const std::string hotbarFragShaderPath = ResolveAssetPath("assets/shaders/hotbar.frag"sv);
	const std::string blockAtlasPNGPath = ResolveAssetPath("assets/block_atlas.png"sv);
	const std::string itemAtlasPNGPath = ResolveAssetPath("assets/item_atlas.png"sv);
	const std::string physicsConstantsPath = ResolveAssetPath("assets/data/physics_constants.data"sv);
	const std::string blocksDataPath = ResolveAssetPath("assets/data/blocks.data"sv);

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

	// init item atlas
	AtlasTexture itemAtlas;
	if(!itemAtlas.LoadFromFile(itemAtlasPNGPath)) {
		std::fprintf(stderr, "Tried item atlas PNG at: %s\n", itemAtlasPNGPath.c_str());
		std::fprintf(stderr, "Item atlas load failed. Add assets/item_atlas.png.\n");
		quit(1);
	}

	// init debug UI
	DebugOverlay debugOverlay;
	if(!debugOverlay.Initialize(uiVertShaderPath, uiFragShaderPath)) {
		std::fprintf(stderr, "Tried UI vertex shader at: %s\n", uiVertShaderPath.c_str());
		std::fprintf(stderr, "Tried UI fragment shader at: %s\n", uiFragShaderPath.c_str());
		std::fprintf(stderr, "DebugOverlay initialization failed.\n");
		quit(1);
	}

	// init hotbar
	Hotbar hotbar;
	if(!hotbar.Initialize(hotbarVertShaderPath, hotbarFragShaderPath)) {
		std::fprintf(stderr, "Tried hotbar vertex shader at: %s\n", hotbarVertShaderPath.c_str());
		std::fprintf(stderr, "Tried hotbar fragment shader at: %s\n", hotbarFragShaderPath.c_str());
		std::fprintf(stderr, "Hotbar initialization failed.\n");
		quit(1);
	}

	// load physics constants (use defaults if file not present)
	PhysicsConstants physicsConstants;
	if(!LoadPhysicsConstants(physicsConstantsPath, physicsConstants)) {
		std::fprintf(stderr, "Warning: could not load '%s', using defaults.\n", physicsConstantsPath.c_str());
	}

	// block IDs (used for convienence, will change when implementing terrain generator)
	enum BLOCKS { GRASS = 0, DIRT = 1, STONE = 2, SAND = 3 };

	// init block registry from blocks.data
	BlockRegistry blockRegistry;
	if(!LoadBlocks(blocksDataPath, &blockAtlas, blockRegistry)) {
		std::fprintf(stderr, "Tried blocks.data at: %s\n", blocksDataPath.c_str());
		std::fprintf(stderr, "Block data load failed.\n");
		quit(1);
	}

	// 5x5x2 flat platform at y = -1
	Grid grid(&blockRegistry);

	// grass layer
	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, 0, z, GRASS)) {
				std::fprintf(stderr, "Grid::AddBlock<Grass> failed at (%d, 0, %d).\n", x, z);
				quit(1);
			}
		}
	}
 
	// dirt layer
	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, -1, z, DIRT)) {
				std::fprintf(stderr, "Grid::AddBlock<Dirt> failed at (%d, 1, %d).\n", x, z);
				quit(1);
			}
		}
	}

	// stone layer
	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, -2, z, STONE)) {
				std::fprintf(stderr, "Grid::AddBlock<Stone> failed at (%d, 1, %d).\n", x, z);
				quit(1);
			}
		}
	}

	// sand tower in center of the platform.
	grid.AddBlock(2, 1, 2, SAND);
	grid.AddBlock(2, 2, 2, SAND);

	// init hotbar
	hotbar.SetSlot(0, GRASS);
	hotbar.SetSlot(1, DIRT);
	hotbar.SetSlot(2, STONE);
	hotbar.SetSlot(3, SAND);

	// cull faces that are hidden between adjacent solid blocks.
	grid.RebuildVisibility();

	glEnable(GL_DEPTH_TEST);
	// various face culling optimizations
	glEnable(GL_CULL_FACE);

	// initialize player (dimensions from physics constants)
	Physics::Entity player;
	player.radius      = 0.30f;
	player.height      = physicsConstants.standHeight;
	player.eyeFromFeet = physicsConstants.standEyeFromFeet;
	
	// runtime variables
	Camera camera;
	Physics physics(grid, blockRegistry);
	physics.SetConstants(physicsConstants);
	bool debug_view = true;
	bool debug_wireframe = false;
	bool debug_looked_at_block = false;
	bool debug_looked_at_face = true;
	bool debug_looked_at_data = true;
	bool debug_wireframe_only = false;
	bool debug_stance = true;
	bool debug_velocity = true;
	bool crawlToggleThisFrame = false;
	bool prevCrawlComboDown = false;
	Uint64 previousCounter = SDL_GetPerformanceCounter();
	double fpsAccumulatedSeconds = 0.0;
	int fpsFrameCount = 0;
	int displayedFps = 0;
	
	// place player
	physics.teleportTo(player, {0.5f, 3.0f, 0.5f}, &camera);

	int winWidth = 0;
	int winHeight = 0;
	// main loop
	while(true) {
		float mouseDeltaX = 0.0f;
		float mouseDeltaY = 0.0f;
		crawlToggleThisFrame = false;

		// calculate fps
		const Uint64 counter = SDL_GetPerformanceCounter();
		const double dt = static_cast<double>(counter - previousCounter) / static_cast<double>(SDL_GetPerformanceFrequency());
		previousCounter = counter;
		fpsAccumulatedSeconds += dt;
		++fpsFrameCount;
		if(fpsAccumulatedSeconds >= 0.25) {
			displayedFps = static_cast<int>(static_cast<double>(fpsFrameCount) / fpsAccumulatedSeconds + 0.5);
			fpsAccumulatedSeconds -= 0.25;
			fpsFrameCount = 0;
		}

		// handle player inputs
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_EVENT_QUIT) {
				goto stop_mainloop;
			}
			if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
				goto stop_mainloop;
			}

			if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				// face neighbour offsets matching faceIndex: 0 = +X, 1 = -X, 2 = +Y, 3 = -Y, 4 = +Z, 5 = -Z
				constexpr glm::ivec3 kFaceOffset[6] = {
					{ 1,  0,  0}, {-1,  0,  0},
					{ 0,  1,  0}, { 0, -1,  0},
					{ 0,  0,  1}, { 0,  0, -1},
				};
				const Grid::LookedAtResult hit = grid.QueryLookedAt(camera);

				if(event.button.button == SDL_BUTTON_LEFT) {
					if(hit.hit) {
						grid.RemoveBlock(hit.blockPos);
					}
				} else if(event.button.button == SDL_BUTTON_MIDDLE) {
					if(hit.hit) {
						hotbar.SetSlot(hotbar.SelectedSlot(), hit.blockID);
					}
				} else if(event.button.button == SDL_BUTTON_RIGHT) {
					if(hit.hit && hit.faceIndex >= 0) {
						const glm::ivec3 placePos = hit.blockPos + kFaceOffset[hit.faceIndex];
						if(!grid.HasBlockAt(placePos)) {
							const uint32_t selectedBlockID = hotbar.CurrentBlockID();
							if(selectedBlockID != 0u || hotbar.SlotHasBlock(hotbar.SelectedSlot())) {
								if (physics.CanPlaceBlockAt(player, camera, placePos)) {
									grid.AddBlock(placePos.x, placePos.y, placePos.z, selectedBlockID);
								}
							}
						}
					}
				}
			}
			if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F3) {
				debug_view = !debug_view;
			}
			if(event.type == SDL_EVENT_KEY_DOWN) {
				const bool* const kbState = SDL_GetKeyboardState(nullptr);
				if(kbState[SDL_SCANCODE_F3]) {
					if(event.key.key == SDLK_W) { debug_wireframe = !debug_wireframe; }
					if(event.key.key == SDLK_B) { debug_looked_at_block = !debug_looked_at_block; }
					if(event.key.key == SDLK_F) { debug_looked_at_face = !debug_looked_at_face; }
					if(event.key.key == SDLK_D) { debug_looked_at_data = !debug_looked_at_data; }
					if(event.key.key == SDLK_T) { debug_wireframe_only = !debug_wireframe_only; }
					if(event.key.key == SDLK_S) { debug_stance = !debug_stance; }
					if(event.key.key == SDLK_V) { debug_velocity = !debug_velocity; }
					if(event.key.key == SDLK_H) {
						// Hot-reload physics constants
						PhysicsConstants reloaded;
						if(LoadPhysicsConstants(physicsConstantsPath, reloaded)) {
							physicsConstants = reloaded;
							physics.SetConstants(physicsConstants);
							std::fprintf(stderr, "Hot-reloaded physics_constants.data\n");
						} else {
							std::fprintf(stderr, "Warning: hot-reload failed for '%s'\n", physicsConstantsPath.c_str());
						}
						// Hot-reload blocks
						blockRegistry.Clear();
						if(LoadBlocks(blocksDataPath, &blockAtlas, blockRegistry)) {
							grid.RebuildVisibility();
							std::fprintf(stderr, "Hot-reloaded blocks.data\n");
						} else {
							std::fprintf(stderr, "Warning: hot-reload failed for '%s'\n", blocksDataPath.c_str());
						}
					}
				} else {
					if(event.key.key >= SDLK_1 && event.key.key <= SDLK_9) {
						hotbar.SelectSlot(static_cast<int>(event.key.key - SDLK_1));
					}
				}
			}
			if(event.type == SDL_EVENT_MOUSE_WHEEL) {
				if(event.wheel.y > 0) { hotbar.SelectPrev(); }
				else if(event.wheel.y < 0) { hotbar.SelectNext(); }
			}
			if(event.type == SDL_EVENT_MOUSE_MOTION) {
				mouseDeltaX += event.motion.xrel;
				mouseDeltaY += event.motion.yrel;
			}
			if(event.type == SDL_EVENT_WINDOW_RESIZED) {
				winWidth = event.window.data1;
				winHeight = event.window.data2;
				glViewport(0, 0, winWidth, winHeight);
			}
		}

		camera.UpdateFromMouseDelta(mouseDeltaX, mouseDeltaY);

		const bool* const keys = SDL_GetKeyboardState(nullptr);
		const bool ctrlDown = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
		const bool shiftDown = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
		const bool crawlComboDown = ctrlDown && shiftDown;
		crawlToggleThisFrame = crawlComboDown && !prevCrawlComboDown;
		prevCrawlComboDown = crawlComboDown;

		// avoid perspective errors
		glm::vec3 forward = camera.Forward();
		glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));

		glm::vec3 moveDir(0.0f);
		if(keys[SDL_SCANCODE_W]) moveDir += forward;
		if(keys[SDL_SCANCODE_S]) moveDir -= forward;
		if(keys[SDL_SCANCODE_D]) moveDir += right;
		if(keys[SDL_SCANCODE_A]) moveDir -= right;
		if(glm::length2(moveDir) > 0.0000001f) {
			moveDir = glm::normalize(moveDir);
		}
		const glm::vec3 desiredHorizontalVelocity = moveDir * physicsConstants.moveSpeed;

		const bool crouchHeld = ctrlDown;

		physics.StepEntityEuler(
			player,
			static_cast<float>(dt),
			desiredHorizontalVelocity,
			keys[SDL_SCANCODE_SPACE],
			crouchHeld,
			crawlToggleThisFrame,
			physicsConstants
		);
		physics.StepBlockGravity(static_cast<float>(dt));
		physics.UpdateFallingBlocks(static_cast<float>(dt));
		physics.ForceEntityUpIfInsideBlock(player);

		camera.position = player.position;

		// this prevents weird errors with the perspective
		SDL_GetWindowSize(window.get(), &winWidth, &winHeight);
		glViewport(0, 0, winWidth, winHeight);

		// bg color
		glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// projection stuff
		const float aspect = (winHeight > 0) ? static_cast<float>(winWidth) / static_cast<float>(winHeight) : 1.0f;
		const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
		const glm::mat4 view = camera.View();

		// debug
		if(!(debug_view && debug_wireframe_only)) {
			grid.Draw(defaultShader, blockAtlas, projection, view);

			// Render sand blocks that are currently mid-fall at their float positions.
			std::vector<Grid::FloatBlock> fallingVisual;
			for (const Physics::FallingBlock& fb : physics.GetFallingBlocks()) {
				fallingVisual.push_back({fb.pos, fb.blockID});
			}
			grid.DrawFloatBlocks(fallingVisual, defaultShader, blockAtlas, projection, view);
		}

		// more debug
		if(debug_view) {
			debugOverlay.DrawFps(displayedFps, winWidth, winHeight);
			if(debug_wireframe || debug_wireframe_only) {
				grid.DrawWireframe(wireframeShader, projection, view);
			}
			if(debug_looked_at_block) { grid.DrawLookedAtBlock(wireframeShader, camera, projection, view); }
			if(debug_looked_at_face)  { grid.DrawLookedAtFace(wireframeShader, camera, projection, view); }

			if(debug_looked_at_data) {
				const Grid::LookedAtResult hit = grid.QueryLookedAt(camera);
				std::ostringstream oss;
				if(hit.hit) {
					const char* const blockName = (hit.blockData != nullptr) ? hit.blockData->name.c_str() : "UNKNOWN";

					oss << "CURRENT BLOCK POS: [" << hit.blockPos.x
						<< " " << hit.blockPos.y
						<< " " << hit.blockPos.z
						<< "] FACE: " << hit.faceIndex
						<< " NAME: " << blockName
						<< " ID: " << hit.blockID;
				} else {
					oss << "NO BLOCK SEEN";
				}
				debugOverlay.DrawText(oss.str(), 16.0f, 44.0f, 4.0f, winWidth, winHeight);
			}

			if(debug_stance) {
				std::ostringstream oss;
				oss << "CURRENT STANCE: " << player.getPosture();
				debugOverlay.DrawText(oss.str(), 16.0f, 72.0f, 4.0f, winWidth, winHeight);
			}

			if(debug_velocity) {
				std::ostringstream oss;
				oss << "VELOCITY: " << player.velocity.x 
					<< " " <<  player.velocity.y 
					<< " " << player.velocity.z;
				debugOverlay.DrawText(oss.str(), 16.0f, 100.0f, 4.0, winWidth, winHeight);
			}
		}

		// UI
		hotbar.Draw(blockRegistry, winWidth, winHeight);

		// swap
		SDL_GL_SwapWindow(window.get());
	}
	stop_mainloop:

	// cleanup shared debug meshes created inside Grid.cpp while context is still alive.
	Grid::ReleaseSharedGLResources();

	return 0;
}

