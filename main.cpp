// c/c++ stdlib
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

// sdl
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

// glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// voxel engine
#include "AtlasTexture.hpp"
#include "Camera.hpp"
#include "CubeMesh.hpp"
#include "DebugOverlay.hpp"
#include "Grid.hpp"
#include "Hotbar.hpp"
#include "Shader.hpp"

/*
TODO:
- Physics Engine
	- Gravity
	- Collisions
	- Falling blocks (sand)
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
}

int main() {
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

	const std::string vertShaderPath = ResolveAssetPath("assets/shaders/voxel.vert");
	const std::string fragShaderPath = ResolveAssetPath("assets/shaders/voxel.frag");
	const std::string uiVertShaderPath = ResolveAssetPath("assets/shaders/ui.vert");
	const std::string uiFragShaderPath = ResolveAssetPath("assets/shaders/ui.frag");
	const std::string wireframeVertShaderPath = ResolveAssetPath("assets/shaders/wireframe.vert");
	const std::string wireframeFragShaderPath = ResolveAssetPath("assets/shaders/wireframe.frag");
	const std::string hotbarVertShaderPath = ResolveAssetPath("assets/shaders/hotbar.vert");
	const std::string hotbarFragShaderPath = ResolveAssetPath("assets/shaders/hotbar.frag");
	const std::string atlasPngPath = ResolveAssetPath("assets/atlas.png");
	const std::string atlasBmpPath = ResolveAssetPath("assets/atlas.bmp");

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

	const FaceTileMap grassBlock = {{
		FaceTile{1, 0},  // front
		FaceTile{1, 0},  // back
		FaceTile{1, 0},  // left
		FaceTile{1, 0},  // right
		FaceTile{0, 0},  // top
		FaceTile{2, 0},  // bottom
	}};

	const FaceTileMap dirtBlock = {{ // only use the grass bottom texture
		FaceTile{2,0}, // front
		FaceTile{2,0}, // back
		FaceTile{2,0}, // left
		FaceTile{2,0}, // right
		FaceTile{2,0}, // top
		FaceTile{2,0}  // bottom
	}};

	const FaceTileMap stoneBlock = {{ // only one stone texture
		FaceTile{0,1}, // front
		FaceTile{0,1}, // back
		FaceTile{0,1}, // left
		FaceTile{0,1}, // right
		FaceTile{0,1}, // top
		FaceTile{0,1}  // bottom
	}};

	const FaceTileMap sandBlock = {{ // only one sand texture
		FaceTile{1,1}, // front
		FaceTile{1,1}, // back
		FaceTile{1,1}, // left
		FaceTile{1,1}, // right
		FaceTile{1,1}, // top
		FaceTile{1,1}  // bottom
	}};

	auto FaceMapsEqual = [](const FaceTileMap& a, const FaceTileMap& b) {
		for(size_t i = 0; i < a.size(); ++i) {
			if(a[i].x != b[i].x || a[i].y != b[i].y) {
				return false;
			}
		}
		return true;
	};

	// 5x5x2 flat platform at y = -1
	Grid grid;

	// grass layer
	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, 0, z, atlas, grassBlock)) {
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
			if(!grid.AddBlock(x, -1, z, atlas, dirtBlock)) {
				std::fprintf(stderr, "Grid::AddBlock<Dirt> failed at (%d, 1, %d).\n", x, z);
				SDL_GL_DestroyContext(glContext);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 1;
			}
		}
	}

	for(int z = 0; z < 5; ++z) {
		for(int x = 0; x < 5; ++x) {
			if(!grid.AddBlock(x, -2, z, atlas, stoneBlock)) {
				std::fprintf(stderr, "Grid::AddBlock<Stone> failed at (%d, 1, %d).\n", x, z);
				SDL_GL_DestroyContext(glContext);
				SDL_DestroyWindow(window);
				SDL_Quit();
				return 1;
			}
		}
	}

	// sand tower in center of the platform.
	grid.AddBlock(2, 1, 2, atlas, sandBlock);
	grid.AddBlock(2, 2, 2, atlas, sandBlock);

	// init hotbar
	hotbar.SetSlot(0, grassBlock);
	hotbar.SetSlot(1, dirtBlock);
	hotbar.SetSlot(2, stoneBlock);
	hotbar.SetSlot(3, sandBlock);

	// cull faces that are hidden between adjacent solid blocks.
	grid.RebuildVisibility();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	Camera camera;
	bool running = true;
	bool debug_view = true;
	bool debug_wireframe = false;
	bool debug_looked_at_block = false;
	bool debug_looked_at_face = true;
	bool debug_looked_at_data = true;
	bool debug_wireframe_only = false;
	Uint64 previousCounter = SDL_GetPerformanceCounter();
	double fpsAccumulatedSeconds = 0.0;
	int fpsFrameCount = 0;
	int displayedFps = 0;

	while(running) {
		float mouseDeltaX = 0.0f;
		float mouseDeltaY = 0.0f;

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
						hotbar.SetSlot(hotbar.SelectedSlot(), hit.faceTiles);
					}
				} else if(event.button.button == SDL_BUTTON_RIGHT) {
					if(hit.hit && hit.faceIndex >= 0) {
						const glm::ivec3 placePos = hit.blockPos + kFaceOffset[hit.faceIndex];
						if(!grid.HasBlockAt(placePos)) {
							grid.AddBlock(placePos.x, placePos.y, placePos.z, atlas, hotbar.CurrentTiles());
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

		const bool* keys = SDL_GetKeyboardState(nullptr);
		camera.UpdateFromKeyboard(keys, static_cast<float>(dt));
		camera.UpdateFromMouseDelta(mouseDeltaX, mouseDeltaY);

		int width = 0;
		int height = 0;
		SDL_GetWindowSize(window, &width, &height);
		glViewport(0, 0, width, height);

		glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const float aspect = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
		const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
		const glm::mat4 view = camera.View();

		if(!(debug_view && debug_wireframe_only)) {
			grid.Draw(defaultShader, atlas, projection, view);
		}

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
					const char* blockName = "CUSTOM";
					if(FaceMapsEqual(hit.faceTiles, grassBlock)) {
						blockName = "GRASS";
					} else if(FaceMapsEqual(hit.faceTiles, dirtBlock)) {
						blockName = "DIRT";
					} else if(FaceMapsEqual(hit.faceTiles, stoneBlock)) {
						blockName = "STONE";
					} else if(FaceMapsEqual(hit.faceTiles, sandBlock)) {
						blockName = "SAND";
					}

					oss << "CURRENT BLOCK POS: [" << hit.blockPos.x
						<< " " << hit.blockPos.y
						<< " " << hit.blockPos.z
						<< "] FACE: " << hit.faceIndex
						<< " NAME: " << blockName;
				} else {
					oss << "NO BLOCK SEEN";
				}
				debugOverlay.DrawText(oss.str(), 16.0f, 44.0f, 4.0f, width, height);
			}
		}

		hotbar.Draw(atlas, width, height);

		SDL_GL_SwapWindow(window);
	}

	SDL_GL_DestroyContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}

