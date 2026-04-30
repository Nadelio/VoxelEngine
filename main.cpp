// c/c++ stdlib
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

// sdl
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

// glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
- Terrain Generation
	- Saving maps to file
- Rendering
	- [BUG] Greedy meshing doesn't run every frame/tick
*/

namespace {
	bool FileExists(const std::string& path) {
		std::ifstream file(path);
		return file.good();
	}

	std::string ResolveAssetPath(const std::string& relativePath) {
		if(FileExists(relativePath)) {
			return relativePath;
		}

		const char* basePathRaw = SDL_GetBasePath();
		if(basePathRaw) {
			const std::string basePath(basePathRaw);

			const std::array<std::string, 3> prefixes = {
				"",
				"assets/",
				"../assets/",
			};

			for(const std::string& prefix : prefixes) {
				const std::string candidate = basePath + prefix + relativePath;
				if(FileExists(candidate)) {
					return candidate;
				}
			}
		}

		return relativePath;
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
		SDL_Quit();
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("Voxel Engine", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if(!window) {
		std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	SDL_GLContext glContext = SDL_GL_CreateContext(window);
	if(!glContext) {
		std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	if(!SDL_GL_SetSwapInterval(1)) {
		std::fprintf(stderr, "Warning: could not enable VSync: %s\n", SDL_GetError());
	}

	if(!SDL_SetWindowRelativeMouseMode(window, true)) {
		std::fprintf(stderr, "Warning: could not enable relative mouse mode: %s\n", SDL_GetError());
	}

	{

	// grab all the assets
	const std::string vertShaderPath = ResolveAssetPath("assets/shaders/voxel.vert");
	const std::string fragShaderPath = ResolveAssetPath("assets/shaders/voxel.frag");
	const std::string uiVertShaderPath = ResolveAssetPath("assets/shaders/ui.vert");
	const std::string uiFragShaderPath = ResolveAssetPath("assets/shaders/ui.frag");
	const std::string wireframeVertShaderPath = ResolveAssetPath("assets/shaders/wireframe.vert");
	const std::string wireframeFragShaderPath = ResolveAssetPath("assets/shaders/wireframe.frag");
	const std::string hotbarVertShaderPath = ResolveAssetPath("assets/shaders/hotbar.vert");
	const std::string hotbarFragShaderPath = ResolveAssetPath("assets/shaders/hotbar.frag");
	const std::string atlasPngPath = ResolveAssetPath("assets/atlas.png");
	const std::string atlasBmpPath = ResolveAssetPath("assets/atlas.bmp"); // NOT CURRENTLY USED
	const std::string physicsConstantsPath = ResolveAssetPath("assets/data/physics_constants.data");
	const std::string blocksDataPath = ResolveAssetPath("assets/data/blocks.data");

	// init default shader
	Shader defaultShader;
	if(!defaultShader.LoadFromFiles(vertShaderPath, fragShaderPath)) {
		std::fprintf(stderr, "Tried vertex shader at: %s\n", vertShaderPath.c_str());
		std::fprintf(stderr, "Tried fragment shader at: %s\n", fragShaderPath.c_str());
		std::fprintf(stderr, "Shader load failed.\n");
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// init wireframe shader
	Shader wireframeShader;
	if(!wireframeShader.LoadFromFiles(wireframeVertShaderPath, wireframeFragShaderPath)) {
		std::fprintf(stderr, "Tried wireframe vertex shader at: %s\n", wireframeVertShaderPath.c_str());
		std::fprintf(stderr, "Tried wireframe fragment shader at: %s\n", wireframeFragShaderPath.c_str());
		std::fprintf(stderr, "Wireframe shader load failed.\n");
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// init atlas
	AtlasTexture atlas;
	if(!atlas.LoadFromFile(atlasPngPath) && !atlas.LoadFromFile(atlasBmpPath)) {
		std::fprintf(stderr, "Tried atlas PNG at: %s\n", atlasPngPath.c_str());
		std::fprintf(stderr, "Tried atlas BMP at: %s\n", atlasBmpPath.c_str());
		std::fprintf(stderr, "Atlas load failed. Add assets/atlas.png or assets/atlas.bmp.\n");
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// init debug UI
	DebugOverlay debugOverlay;
	if(!debugOverlay.Initialize(uiVertShaderPath, uiFragShaderPath)) {
		std::fprintf(stderr, "Tried UI vertex shader at: %s\n", uiVertShaderPath.c_str());
		std::fprintf(stderr, "Tried UI fragment shader at: %s\n", uiFragShaderPath.c_str());
		std::fprintf(stderr, "DebugOverlay initialization failed.\n");
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// init hotbar
	Hotbar hotbar;
	if(!hotbar.Initialize(hotbarVertShaderPath, hotbarFragShaderPath)) {
		std::fprintf(stderr, "Tried hotbar vertex shader at: %s\n", hotbarVertShaderPath.c_str());
		std::fprintf(stderr, "Tried hotbar fragment shader at: %s\n", hotbarFragShaderPath.c_str());
		std::fprintf(stderr, "Hotbar initialization failed.\n");
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// load physics constants (use defaults if file not present)
	PhysicsConstants physicsConstants;
	if(!LoadPhysicsConstants(physicsConstantsPath, physicsConstants)) {
		std::fprintf(stderr, "Warning: could not load '%s', using defaults.\n", physicsConstantsPath.c_str());
	}

	// block IDs (must match blocks.data)
	enum BLOCKS { GRASS = 0, DIRT = 1, STONE = 2, SAND = 3 };

	// init block registry from blocks.data
	BlockRegistry blockRegistry;
	if(!LoadBlocks(blocksDataPath, &atlas, blockRegistry)) {
		std::fprintf(stderr, "Tried blocks.data at: %s\n", blocksDataPath.c_str());
		std::fprintf(stderr, "Block data load failed.\n");
		SDL_GL_DestroyContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	// 5x5x2 flat platform at y = -1
	Grid grid(&blockRegistry);

	// grass layer
	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, 0, z, GRASS)) {
				std::fprintf(stderr, "Grid::AddBlock<Grass> failed at (%d, 0, %d).\n", x, z);
				SDL_GL_DestroyContext(glContext);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 1;
			}
		}
	}
 
	// dirt layer
	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, -1, z, DIRT)) {
				std::fprintf(stderr, "Grid::AddBlock<Dirt> failed at (%d, 1, %d).\n", x, z);
				SDL_GL_DestroyContext(glContext);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 1;
			}
		}
	}

	// stone layer
	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, -2, z, STONE)) {
				std::fprintf(stderr, "Grid::AddBlock<Stone> failed at (%d, 1, %d).\n", x, z);
				SDL_GL_DestroyContext(glContext);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 1;
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

	// various face culling optimizations
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	// initialize player (dimensions from physics constants)
	Physics::Entity player;
	player.radius      = 0.30f;
	player.height      = physicsConstants.standHeight;
	player.eyeFromFeet = physicsConstants.standEyeFromFeet;
	
	// runtime variables
	Camera camera;
	Physics physics(grid, blockRegistry);
	physics.SetConstants(physicsConstants);
	bool running = true;
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
	player.position = camera.position;
	physics.teleportTo(player, {0.5f, 3.0f, 0.5f}, &camera);

	// main loop
	while(running) {
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
			fpsAccumulatedSeconds = 0.0;
			fpsFrameCount = 0;
		}

		// handle player inputs
		SDL_Event event;
		while(SDL_PollEvent(&event)) {
			if(event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
				running = false;
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
				const bool* kbState = SDL_GetKeyboardState(nullptr);
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
						if(LoadBlocks(blocksDataPath, &atlas, blockRegistry)) {
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
		}

		camera.UpdateFromMouseDelta(mouseDeltaX, mouseDeltaY);

		const bool* keys = SDL_GetKeyboardState(nullptr);
		const bool ctrlDown = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];
		const bool shiftDown = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
		const bool crawlComboDown = ctrlDown && shiftDown;
		crawlToggleThisFrame = crawlComboDown && !prevCrawlComboDown;
		prevCrawlComboDown = crawlComboDown;

		glm::vec3 forward = camera.Forward();
		forward.y = 0.0f;
		if(glm::length(forward) > 0.0001f) {
			forward = glm::normalize(forward);
		}
		glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
		if(glm::length(right) > 0.0001f) {
			right = glm::normalize(right);
		}

		glm::vec3 moveDir(0.0f);
		if(keys[SDL_SCANCODE_W]) moveDir += forward;
		if(keys[SDL_SCANCODE_S]) moveDir -= forward;
		if(keys[SDL_SCANCODE_D]) moveDir += right;
		if(keys[SDL_SCANCODE_A]) moveDir -= right;
		if(glm::length(moveDir) > 0.0001f) {
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


		// window resizing
		int width = 0;
		int height = 0;
		SDL_GetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		// bg color
		glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// projection stuff
		const float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
		const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
		const glm::mat4 view = camera.View();

		// debug
		if(!(debug_view && debug_wireframe_only)) {
			grid.Draw(defaultShader, atlas, projection, view);

			// Render sand blocks that are currently mid-fall at their float positions.
			std::vector<Grid::FloatBlock> fallingVisual;
			for (const Physics::FallingBlock& fb : physics.GetFallingBlocks()) {
				fallingVisual.push_back({fb.pos, fb.blockID});
			}
			grid.DrawFloatBlocks(fallingVisual, defaultShader, atlas, projection, view);
		}

		// more debug
		if(debug_view) {
			debugOverlay.DrawFps(displayedFps, width, height);
			if(debug_wireframe || debug_wireframe_only) {
				grid.DrawWireframe(wireframeShader, projection, view);
			}
			if(debug_looked_at_block) { grid.DrawLookedAtBlock(wireframeShader, camera, projection, view); }
			if(debug_looked_at_face)  { grid.DrawLookedAtFace(wireframeShader, camera, projection, view); }

			if(debug_looked_at_data) {
				const Grid::LookedAtResult hit = grid.QueryLookedAt(camera);
				std::ostringstream oss;
				if(hit.hit) {
					const char* blockName = (hit.blockData != nullptr) ? hit.blockData->name.c_str() : "UNKNOWN";

					oss << "CURRENT BLOCK POS: [" << hit.blockPos.x
						<< " " << hit.blockPos.y
						<< " " << hit.blockPos.z
						<< "] FACE: " << hit.faceIndex
						<< " NAME: " << blockName
						<< " ID: " << hit.blockID;
				} else {
					oss << "NO BLOCK SEEN";
				}
				debugOverlay.DrawText(oss.str(), 16.0f, 44.0f, 4.0f, width, height);
			}

			if(debug_stance) {
				std::ostringstream oss;
				oss << "CURRENT STANCE: " << player.getPosture();
				debugOverlay.DrawText(oss.str(), 16.0f, 72.0f, 4.0f, width, height);
			}

			if(debug_velocity) {
				std::ostringstream oss;
				oss << "VELOCITY: " << player.velocity.x 
					<< " " <<  player.velocity.y 
					<< " " << player.velocity.z;
				debugOverlay.DrawText(oss.str(), 16.0f, 100.0f, 4.0, width, height);
			}
		}

		// UI
		hotbar.Draw(blockRegistry, width, height);

		// swap
		SDL_GL_SwapWindow(window);
	}

	// cleanup shared debug meshes created inside Grid.cpp while context is still alive.
	Grid::ReleaseSharedGLResources();
	}

	SDL_GL_DestroyContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

