#include "MenuSession.hpp"
#include "WorldSession.hpp"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <array>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "AssetLoader.hpp"
#include "GameUI.hpp"
#include "Grid.hpp"
#include "Hotbar.hpp"
#include "Keybinds.hpp"
#include "TerrainGen.hpp"
#include "StructureFile.hpp"
#include "WorldFile.hpp"

namespace {
	enum BLOCKS { GRASS = 0, DIRT, STONE, ANDESITE, SAND, SNOW, ICE, OAK_LEAVES, OAK_LOGS };

	PFNGLGENFRAMEBUFFERSPROC mglGenFramebuffers = nullptr;
	PFNGLBINDFRAMEBUFFERPROC mglBindFramebuffer = nullptr;
	PFNGLFRAMEBUFFERTEXTURE2DPROC mglFramebufferTexture2D = nullptr;
	PFNGLCHECKFRAMEBUFFERSTATUSPROC mglCheckFramebufferStatus = nullptr;
	PFNGLDELETEFRAMEBUFFERSPROC mglDeleteFramebuffers = nullptr;
	PFNGLGENRENDERBUFFERSPROC mglGenRenderbuffers = nullptr;
	PFNGLBINDRENDERBUFFERPROC mglBindRenderbuffer = nullptr;
	PFNGLRENDERBUFFERSTORAGEPROC mglRenderbufferStorage = nullptr;
	PFNGLFRAMEBUFFERRENDERBUFFERPROC mglFramebufferRenderbuffer = nullptr;
	PFNGLDELETERENDERBUFFERSPROC mglDeleteRenderbuffers = nullptr;
	PFNGLACTIVETEXTUREPROC mglActiveTexture = nullptr;
	bool gMenuPreviewGlLoaded = false;

	bool LoadMenuPreviewGlFunctions() {
		if(gMenuPreviewGlLoaded) {
			return true;
		}
		mglGenFramebuffers = reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(SDL_GL_GetProcAddress("glGenFramebuffers"));
		mglBindFramebuffer = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBindFramebuffer"));
		mglFramebufferTexture2D = reinterpret_cast<PFNGLFRAMEBUFFERTEXTURE2DPROC>(SDL_GL_GetProcAddress("glFramebufferTexture2D"));
		mglCheckFramebufferStatus = reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(SDL_GL_GetProcAddress("glCheckFramebufferStatus"));
		mglDeleteFramebuffers = reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(SDL_GL_GetProcAddress("glDeleteFramebuffers"));
		mglGenRenderbuffers = reinterpret_cast<PFNGLGENRENDERBUFFERSPROC>(SDL_GL_GetProcAddress("glGenRenderbuffers"));
		mglBindRenderbuffer = reinterpret_cast<PFNGLBINDRENDERBUFFERPROC>(SDL_GL_GetProcAddress("glBindRenderbuffer"));
		mglRenderbufferStorage = reinterpret_cast<PFNGLRENDERBUFFERSTORAGEPROC>(SDL_GL_GetProcAddress("glRenderbufferStorage"));
		mglFramebufferRenderbuffer = reinterpret_cast<PFNGLFRAMEBUFFERRENDERBUFFERPROC>(SDL_GL_GetProcAddress("glFramebufferRenderbuffer"));
		mglDeleteRenderbuffers = reinterpret_cast<PFNGLDELETERENDERBUFFERSPROC>(SDL_GL_GetProcAddress("glDeleteRenderbuffers"));
		mglActiveTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(SDL_GL_GetProcAddress("glActiveTexture"));
		gMenuPreviewGlLoaded = mglGenFramebuffers && mglBindFramebuffer && mglFramebufferTexture2D &&
			mglCheckFramebufferStatus && mglDeleteFramebuffers && mglGenRenderbuffers &&
			mglBindRenderbuffer && mglRenderbufferStorage && mglFramebufferRenderbuffer &&
			mglDeleteRenderbuffers && mglActiveTexture;
		return gMenuPreviewGlLoaded;
	}

	std::string ResolveAssetPath(std::filesystem::path relativePath) {
		if(std::filesystem::is_regular_file(relativePath)) {
			return relativePath.string();
		}

		if(const char* const basePathRaw = SDL_GetBasePath()) {
			const std::filesystem::path basePath(basePathRaw);
			const std::array<std::filesystem::path, 3> prefixes = {
				"",
				"assets",
				"../assets",
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

	void RememberWindowedSize(AppContext& ctx) {
		if(ctx.windowMode != WindowMode::WINDOWED) return;
		SDL_GetWindowSize(ctx.window, &ctx.windowedWidth, &ctx.windowedHeight);
	}

	void ApplyWindowMode(AppContext& ctx, WindowMode mode) {
		RememberWindowedSize(ctx);

		if(!SDL_SetWindowFullscreen(ctx.window, false)) {
			std::fprintf(stderr, "Warning: could not leave fullscreen: %s\n", SDL_GetError());
		}
		if(!SDL_RestoreWindow(ctx.window)) {
			std::fprintf(stderr, "Warning: could not restore window state: %s\n", SDL_GetError());
		}

		if(mode == WindowMode::WINDOWED) {
			if(!SDL_SetWindowBordered(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not restore window border: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowResizable(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not set window resizable: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowSize(ctx.window, ctx.windowedWidth, ctx.windowedHeight)) {
				std::fprintf(stderr, "Warning: could not set window size: %s\n", SDL_GetError());
			}
		} else if(mode == WindowMode::WINDOWED_FULLSCREEN) {
			if(!SDL_SetWindowBordered(ctx.window, false)) {
				std::fprintf(stderr, "Warning: could not remove window border: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowResizable(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not set window resizable: %s\n", SDL_GetError());
			}
			if(!SDL_MaximizeWindow(ctx.window)) {
				std::fprintf(stderr, "Warning: could not maximize window: %s\n", SDL_GetError());
			}
		} else {
			if(!SDL_SetWindowBordered(ctx.window, false)) {
				std::fprintf(stderr, "Warning: could not remove window border: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowResizable(ctx.window, false)) {
				std::fprintf(stderr, "Warning: could not set window non-resizable: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowFullscreen(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not enter fullscreen: %s\n", SDL_GetError());
			}
		}
		ctx.windowMode = mode;
	}
}

MenuSession::~MenuSession() {
	DestroySkinPreviewResources();
}

void MenuSession::DestroySkinPreviewResources() {
	if(skinPreviewColorTex_ != 0) {
		glDeleteTextures(1, &skinPreviewColorTex_);
		skinPreviewColorTex_ = 0;
	}
	if(skinPreviewShadowTex_ != 0) {
		glDeleteTextures(1, &skinPreviewShadowTex_);
		skinPreviewShadowTex_ = 0;
	}
	if(skinPreviewDepthRbo_ != 0 && mglDeleteRenderbuffers != nullptr) {
		mglDeleteRenderbuffers(1, &skinPreviewDepthRbo_);
		skinPreviewDepthRbo_ = 0;
	}
	if(skinPreviewFbo_ != 0 && mglDeleteFramebuffers != nullptr) {
		mglDeleteFramebuffers(1, &skinPreviewFbo_);
		skinPreviewFbo_ = 0;
	}
	skinPreviewReady_ = false;
	loadedPreviewSkinPath_.clear();
}

void MenuSession::EnsureSkinList(AppContext& ctx) {
	if(skinListLoaded_) {
		return;
	}

	skinListLoaded_ = true;
	skinPaths.clear();
	skinNames.clear();

	const std::filesystem::path skinsDir = std::filesystem::path(ResolveAssetPath("assets/textures/skins"));
	if(std::filesystem::is_directory(skinsDir)) {
		for(const auto& entry : std::filesystem::directory_iterator(skinsDir)) {
			if(!entry.is_regular_file()) {
				continue;
			}
			const std::filesystem::path ext = entry.path().extension();
			if(ext != ".png" && ext != ".PNG") {
				continue;
			}
			skinPaths.push_back(entry.path().string());
			skinNames.push_back(entry.path().stem().string());
		}
	}

	if(skinPaths.empty()) {
		const std::string fallback = ResolveAssetPath("assets/textures/skins/Debug.png");
		if(std::filesystem::is_regular_file(fallback)) {
			skinPaths.push_back(fallback);
			skinNames.push_back("Debug");
		}
	}

	std::vector<std::size_t> order(skinPaths.size());
	for(std::size_t i = 0; i < order.size(); ++i) {
		order[i] = i;
	}
	std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
		return skinNames[a] < skinNames[b];
	});

	std::vector<std::string> sortedPaths;
	std::vector<std::string> sortedNames;
	sortedPaths.reserve(order.size());
	sortedNames.reserve(order.size());
	for(std::size_t idx : order) {
		sortedPaths.push_back(std::move(skinPaths[idx]));
		sortedNames.push_back(std::move(skinNames[idx]));
	}
	skinPaths = std::move(sortedPaths);
	skinNames = std::move(sortedNames);

	selectedSkinIdx = -1;
	if(!ctx.selectedSkinPath.empty()) {
		for(int i = 0; i < static_cast<int>(skinPaths.size()); ++i) {
			if(skinPaths[static_cast<std::size_t>(i)] == ctx.selectedSkinPath) {
				selectedSkinIdx = i;
				break;
			}
		}
	}
	if(selectedSkinIdx < 0 && !skinPaths.empty()) {
		selectedSkinIdx = 0;
	}

	ApplySelectedSkin(ctx);
}

void MenuSession::ApplySelectedSkin(AppContext& ctx) {
	if(selectedSkinIdx < 0 || selectedSkinIdx >= static_cast<int>(skinPaths.size())) {
		return;
	}

	const std::string& chosenPath = skinPaths[static_cast<std::size_t>(selectedSkinIdx)];
	const std::string& chosenName = skinNames[static_cast<std::size_t>(selectedSkinIdx)];
	ctx.selectedSkinPath = chosenPath;
	ctx.selectedSkinName = chosenName;

	if(skinPreviewReady_ && loadedPreviewSkinPath_ != chosenPath) {
		if(skinPreviewModel_.LoadSkin(chosenPath)) {
			loadedPreviewSkinPath_ = chosenPath;
		} else {
			std::fprintf(stderr, "Warning: failed to load preview skin '%s'\n", chosenPath.c_str());
		}
	}
}

void MenuSession::EnsureCapeList(AppContext& ctx) {
	capePaths.clear();
	capeNames.clear();

	const std::filesystem::path capesDir = std::filesystem::path(ResolveAssetPath("assets/textures/capes"));
	if(std::filesystem::is_directory(capesDir)) {
		for(const auto& entry : std::filesystem::directory_iterator(capesDir)) {
			if(!entry.is_regular_file()) {
				continue;
			}
			const std::filesystem::path ext = entry.path().extension();
			if(ext != ".png" && ext != ".PNG") {
				continue;
			}
			capePaths.push_back(entry.path().string());
			capeNames.push_back(entry.path().stem().string());
		}
	}

	std::vector<std::size_t> order(capePaths.size());
	for(std::size_t i = 0; i < order.size(); ++i) {
		order[i] = i;
	}
	std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
		return capeNames[a] < capeNames[b];
	});

	std::vector<std::string> sortedPaths;
	std::vector<std::string> sortedNames;
	sortedPaths.reserve(order.size());
	sortedNames.reserve(order.size());
	for(std::size_t idx : order) {
		sortedPaths.push_back(std::move(capePaths[idx]));
		sortedNames.push_back(std::move(capeNames[idx]));
	}
	capePaths = std::move(sortedPaths);
	capeNames = std::move(sortedNames);

	selectedCapeIdx = -1;
	if(!ctx.selectedCapePath.empty()) {
		for(int i = 0; i < static_cast<int>(capePaths.size()); ++i) {
			if(capePaths[static_cast<std::size_t>(i)] == ctx.selectedCapePath) {
				selectedCapeIdx = i;
				break;
			}
		}
	}
	
	if(selectedCapeIdx < 0 && !capePaths.empty()) {
		selectedCapeIdx = 0;
		capeEnabled = true;
	}

	ApplySelectedCape(ctx);
}

void MenuSession::ApplySelectedCape(AppContext& ctx) {
	if(selectedCapeIdx < 0 || selectedCapeIdx >= static_cast<int>(capePaths.size())) {
		ctx.selectedCapePath.clear();
		ctx.selectedCapeName.clear();
		ctx.capeEnabled = false;
		return;
	}

	const std::string& chosenPath = capePaths[static_cast<std::size_t>(selectedCapeIdx)];
	const std::string& chosenName = capeNames[static_cast<std::size_t>(selectedCapeIdx)];
	ctx.selectedCapePath = chosenPath;
	ctx.selectedCapeName = chosenName;
	ctx.capeEnabled = capeEnabled;

	if(skinPreviewReady_) {
		if(skinPreviewModel_.LoadCape(chosenPath)) {
			skinPreviewModel_.SetCapeEnabled(capeEnabled);
		} else {
			std::fprintf(stderr, "Warning: failed to load preview cape '%s'\n", chosenPath.c_str());
		}
	}
}

bool MenuSession::EnsureSkinPreviewResources(AppContext& ctx) {
	if(skinPreviewReady_) {
		return true;
	}
	if(!LoadMenuPreviewGlFunctions()) {
		std::fprintf(stderr, "Warning: preview GL functions unavailable.\n");
		return false;
	}

	if(!skinPreviewModel_.Initialize()) {
		std::fprintf(stderr, "Warning: skin preview player model initialization failed.\n");
		return false;
	}

	const std::string skinVert = ResolveAssetPath("assets/shaders/skin.vert");
	const std::string skinFrag = ResolveAssetPath("assets/shaders/skin.frag");
	if(!skinPreviewShader_.LoadFromFiles(skinVert, skinFrag)) {
		std::fprintf(stderr, "Warning: skin preview shader load failed.\n");
		return false;
	}

	glGenTextures(1, &skinPreviewColorTex_);
	glBindTexture(GL_TEXTURE_2D, skinPreviewColorTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, skinPreviewSize_, skinPreviewSize_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	mglGenRenderbuffers(1, &skinPreviewDepthRbo_);
	mglBindRenderbuffer(GL_RENDERBUFFER, skinPreviewDepthRbo_);
	mglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, skinPreviewSize_, skinPreviewSize_);

	mglGenFramebuffers(1, &skinPreviewFbo_);
	mglBindFramebuffer(GL_FRAMEBUFFER, skinPreviewFbo_);
	mglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, skinPreviewColorTex_, 0);
	mglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, skinPreviewDepthRbo_);

	float oneDepth = 1.0f;
	glGenTextures(1, &skinPreviewShadowTex_);
	glBindTexture(GL_TEXTURE_2D, skinPreviewShadowTex_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &oneDepth);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	const bool complete = mglCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	mglBindFramebuffer(GL_FRAMEBUFFER, 0);
	mglBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	if(!complete) {
		std::fprintf(stderr, "Warning: skin preview framebuffer is incomplete.\n");
		DestroySkinPreviewResources();
		return false;
	}

	if(!ctx.selectedSkinPath.empty() && skinPreviewModel_.LoadSkin(ctx.selectedSkinPath)) {
		loadedPreviewSkinPath_ = ctx.selectedSkinPath;
	} else {
		const std::string fallbackSkin = ResolveAssetPath("assets/textures/skins/Debug.png");
		if(skinPreviewModel_.LoadSkin(fallbackSkin)) {
			loadedPreviewSkinPath_ = fallbackSkin;
		}
	}

	skinPreviewReady_ = true;
	return true;
}

void MenuSession::RenderSkinPreview(int winW, int winH, AppContext& ctx) {
	if(!EnsureSkinPreviewResources(ctx)) {
		return;
	}

	GLint priorFramebuffer = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &priorFramebuffer);
	GLint priorViewport[4] = {0, 0, winW, winH};
	glGetIntegerv(GL_VIEWPORT, priorViewport);

	mglBindFramebuffer(GL_FRAMEBUFFER, skinPreviewFbo_);
	glViewport(0, 0, skinPreviewSize_, skinPreviewSize_);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.09f, 0.12f, 0.16f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Physics::Entity previewPlayer;
	previewPlayer.position = glm::vec3(0.0f, 1.72f, 0.0f);
	previewPlayer.velocity = glm::vec3(0.0f);
	previewPlayer.eyeFromFeet = 1.62f;
	previewPlayer.height = 1.8f;
	previewPlayer.radius = 0.3f;
	previewPlayer.onGround = true;
	previewPlayer.posture = Physics::PostureState::STANDING;

	const glm::vec3 previewCameraPos(0.0f, 1.15f, 2.55f);
	const glm::vec3 previewCameraTarget(0.0f, 1.15f, 0.0f);
	const glm::vec3 cameraLook = glm::normalize(previewCameraTarget - previewCameraPos);
	constexpr float previewYawOffset = glm::radians(135.0f);
	const float yawCos = std::cos(previewYawOffset);
	const float yawSin = std::sin(previewYawOffset);
	const glm::vec3 cameraForward = glm::normalize(glm::vec3(
		cameraLook.x * yawCos + cameraLook.z * yawSin,
		0.0f,
		-cameraLook.x * yawSin + cameraLook.z * yawCos
	));
	const glm::mat4 projection = glm::perspective(glm::radians(30.0f), 1.0f, 0.1f, 100.0f);
	const glm::mat4 view = glm::lookAt(
		previewCameraPos,
		previewCameraTarget,
		glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 lightSpace = glm::mat4(1.0f);

	mglActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, skinPreviewShadowTex_);
	mglActiveTexture(GL_TEXTURE0);

	skinPreviewModel_.UpdateAnimation(previewPlayer, 1.0f / 60.0f, false);
	skinPreviewModel_.Draw(skinPreviewShader_, projection, view, lightSpace, previewPlayer, cameraForward, false);

	mglBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(priorFramebuffer));
	glViewport(priorViewport[0], priorViewport[1], priorViewport[2], priorViewport[3]);
}

bool MenuSession::Frame(int winW, int winH, AppContext& ctx, WorldSession& worldSession) {
	ctx.debugOverlay->NewFrame();

	bool wantQuit = false;

	if(ctx.gameState == GameState::MAIN_MENU) {
		EnsureSkinList(ctx);
		EnsureCapeList(ctx);
		ApplySelectedSkin(ctx);
		ApplySelectedCape(ctx);
		RenderSkinPreview(winW, winH, ctx);

		const int previousSkinIdx = selectedSkinIdx;
		const int previousCapeIdx = selectedCapeIdx;
		const bool previousCapeEnabled = capeEnabled;
		GameState nextState = GameState::MAIN_MENU;
		DrawMainMenu(nextState, wantQuit, winW, winH, skinNames, selectedSkinIdx, capeNames, selectedCapeIdx, capeEnabled, skinPreviewColorTex_);
		if(selectedSkinIdx != previousSkinIdx) {
			ApplySelectedSkin(ctx);
		}
		if(selectedCapeIdx != previousCapeIdx || capeEnabled != previousCapeEnabled) {
			ApplySelectedCape(ctx);
		}
		ctx.gameState = nextState;
		if(ctx.gameState == GameState::WORLDS_MENU) {
			worldEntries.clear();
		}

	} else if(ctx.gameState == GameState::SETTINGS_MENU) {
		if (settingsPage == SettingsPage::MAIN) { 
			bool wantBack      = false;
			bool wantControls  = false;
			bool windowModeChanged = false;
			int windowModeIdx = static_cast<int>(ctx.windowMode);
			std::string pickedBlockAtlas, pickedItemAtlas, pickedResPack, pickedDataPack;
			DrawSettingsMenu(wantBack, wantControls,
			                 pickedBlockAtlas, pickedItemAtlas,
			                 pickedResPack, pickedDataPack,
			                 windowModeIdx, windowModeChanged,
			                 ctx.toggleSprint,
			                 winW, winH);

			if(windowModeChanged && windowModeIdx >= 0 && windowModeIdx <= 2) {
				ApplyWindowMode(ctx, static_cast<WindowMode>(windowModeIdx));
			}

			if (wantBack) {
				ctx.gameState = settingsReturnState;
				settingsReturnState = GameState::MAIN_MENU;
			}
			if (wantControls) {
				settingsPage        = SettingsPage::CONTROLS;
				editedKeybinds      = *ctx.keybinds;
				listeningKeybindIdx = -1;
			}

			if (!pickedBlockAtlas.empty()) {
				if (!ctx.blockAtlas->LoadFromFile(pickedBlockAtlas))
					std::fprintf(stderr, "Warning: failed to load block atlas from '%s'\n",
					             pickedBlockAtlas.c_str());
			}
			if (!pickedItemAtlas.empty()) {
				if (!ctx.itemAtlas->LoadFromFile(pickedItemAtlas))
					std::fprintf(stderr, "Warning: failed to load item atlas from '%s'\n",
					             pickedItemAtlas.c_str());
			}

			if (!pickedResPack.empty()) {
				namespace fs = std::filesystem;
				const std::string blockPath = (fs::path(pickedResPack) / "block_atlas.png").string();
				const std::string itemPath  = (fs::path(pickedResPack) / "item_atlas.png").string();
				if (!ctx.blockAtlas->LoadFromFile(blockPath))
					std::fprintf(stderr, "Warning: resource pack missing block_atlas.png at '%s'\n",
					             blockPath.c_str());
				if (!ctx.itemAtlas->LoadFromFile(itemPath))
					std::fprintf(stderr, "Warning: resource pack missing item_atlas.png at '%s'\n",
					             itemPath.c_str());
			}

			if (!pickedDataPack.empty()) {
				namespace fs = std::filesystem;
				const std::string blocksPath  = (fs::path(pickedDataPack) / "blocks.data").string();
				const std::string physicsPath = (fs::path(pickedDataPack) / "physics_constants.data").string();
				ctx.blockRegistry->Clear();
				if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
					std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n",
					             blocksPath.c_str());
				if (!LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
					std::fprintf(stderr, "Warning: data pack missing physics_constants.data at '%s'\n",
					             physicsPath.c_str());
				else
					ctx.physics->SetConstants(*ctx.physicsConstants);
			}

		} else if (settingsPage == SettingsPage::CONTROLS) {
			bool wantBack = false;
			DrawControlsMenu(editedKeybinds, listeningKeybindIdx, wantBack, winW, winH);
			if (wantBack) {
				if (!SaveKeybinds(ctx.keybindsDataPath, editedKeybinds))
					std::fprintf(stderr, "Warning: could not save keybinds to '%s'\n",
					             ctx.keybindsDataPath.c_str());
				LoadKeybinds(ctx.keybindsDataPath, *ctx.keybinds);
				settingsPage        = SettingsPage::MAIN;
				listeningKeybindIdx = -1;
			}
		}

	} else if(ctx.gameState == GameState::WORLDS_MENU) {
		bool wantLoad = false,
             wantNew = false,
             wantDelete = false,
             wantRename = false,
             wantBack = false;
		std::string  loadPath, loadName, renameNewName;
		WorldFile::Header loadHeader;

		const std::string structuresDir =
		    (std::filesystem::path(ctx.blocksDataPath).parent_path() / "structures").string();
		DrawWorldsMenu(ctx.worldsDir, structuresDir, worldEntries, selectedIdx,
		               wantLoad, loadPath, loadName, loadHeader,
		               wantNew, wantDelete, wantRename, renameNewName, wantBack,
		               winW, winH);

		if(wantBack) { ctx.gameState = GameState::MAIN_MENU; }
		if(wantNew)  { ctx.gameState = GameState::NEW_WORLD_MENU; newWorldParams = {}; }

		if(wantDelete && selectedIdx >= 0 && selectedIdx < static_cast<int>(worldEntries.size())) {
			std::filesystem::remove(worldEntries[selectedIdx].path);
			worldEntries.clear();
			selectedIdx = -1;
		}

		if(wantRename && selectedIdx >= 0 && selectedIdx < static_cast<int>(worldEntries.size())
		   && !worldEntries[selectedIdx].isStructure) {
			namespace fs = std::filesystem;
			const fs::path oldFsPath(worldEntries[selectedIdx].path);
			const fs::path newFsPath = oldFsPath.parent_path() / (renameNewName + ".world");
			if(!fs::exists(newFsPath)) {
				std::error_code ec;
				fs::rename(oldFsPath, newFsPath, ec);
				if(ec) std::fprintf(stderr, "Warning: failed to rename world: %s\n", ec.message().c_str());
			} else {
				std::fprintf(stderr, "Warning: a world named '%s' already exists\n", renameNewName.c_str());
			}
			worldEntries.clear();
			selectedIdx = -1;
		}

		if(wantLoad) {
			namespace fs = std::filesystem;
			const bool loadingStruct =
			    selectedIdx >= 0 && selectedIdx < static_cast<int>(worldEntries.size())
			    && worldEntries[selectedIdx].isStructure;
			if(loadingStruct) {
				ctx.grid->Clear();
				StructureFile::Header sh;
				if(StructureFile::Load(loadPath, sh, *ctx.grid)) {
					ctx.isStructureSession = true;
					ctx.structureOrigin    = sh.origin;
					ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
					ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
					ctx.hotbar->SetSlot(4, SAND);     ctx.hotbar->SetSlot(5, SNOW);
					ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);
					WorldFile::Header dummy;
					worldSession.Enter(ctx, dummy, loadPath);
					ctx.gameState = GameState::PLAYING;
				} else {
					std::fprintf(stderr, "Warning: failed to load structure '%s'\n", loadPath.c_str());
				}
			} else {
				ctx.isStructureSession = false;
				ctx.grid->Clear();
				WorldFile::Header h;
				if(WorldFile::Load(loadPath, h, *ctx.grid)) {
					{
						for (const auto& dp : h.datapacks) {
							const std::string blocksPath  = (fs::path(dp) / "blocks.data").string();
							const std::string physicsPath = (fs::path(dp) / "physics_constants.data").string();
							ctx.blockRegistry->Clear();
							if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
								std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n", blocksPath.c_str());
							if (LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
								ctx.physics->SetConstants(*ctx.physicsConstants);
						}
					}
					ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
					ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
					ctx.hotbar->SetSlot(4, SAND);     ctx.hotbar->SetSlot(5, SNOW);
					ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);
					worldSession.Enter(ctx, h, loadPath);
					ctx.gameState = GameState::PLAYING;
				} else {
					std::fprintf(stderr, "Warning: failed to load world '%s'\n", loadPath.c_str());
				}
			}
		}

	} else if(ctx.gameState == GameState::NEW_WORLD_MENU) {
		bool wantCreate = false, wantBack = false;
		DrawNewWorldMenu(newWorldParams, wantCreate, wantBack, *ctx.biomeRegistry, *ctx.blockRegistry, winW, winH);

		if(wantBack) { ctx.gameState = GameState::WORLDS_MENU; }

		if(wantCreate) {
			namespace fs = std::filesystem;
			if(newWorldParams.worldTypeIdx == 3) {
				const std::string structDir =
				    (fs::path(ctx.blocksDataPath).parent_path() / "structures").string();
				fs::create_directories(structDir);
				const std::string structPath =
				    (fs::path(structDir) / (std::string(newWorldParams.structureNameBuf) + ".struct")).string();

				ctx.grid->Clear();
				for (const auto& dp : newWorldParams.datapacks) {
					const std::string blocksPath  = (fs::path(dp) / "blocks.data").string();
					const std::string physicsPath = (fs::path(dp) / "physics_constants.data").string();
					ctx.blockRegistry->Clear();
					if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
						std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n", blocksPath.c_str());
					if (LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
						ctx.physics->SetConstants(*ctx.physicsConstants);
				}

				ctx.isStructureSession = true;
				ctx.structureOrigin    = {
				    newWorldParams.structureOriginBuf[0],
				    newWorldParams.structureOriginBuf[1],
				    newWorldParams.structureOriginBuf[2]
				};

				ctx.hotbar->SetSlot(0, GRASS);      ctx.hotbar->SetSlot(1, DIRT);
				ctx.hotbar->SetSlot(2, STONE);      ctx.hotbar->SetSlot(3, ANDESITE);
				ctx.hotbar->SetSlot(4, SAND);       ctx.hotbar->SetSlot(5, SNOW);
				ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);

				for(int dx = -2; dx <= 2; ++dx) {
					for(int dz = -2; dz <= 2; ++dz) {
						ctx.grid->AddBlock(dx, ctx.structureOrigin.y, dz, GRASS);
					}
				}

				WorldFile::Header dummy;
				worldSession.Enter(ctx, dummy, structPath);
				ctx.gameState = GameState::PLAYING;
			} else {
				WorldFile::Header h = newWorldParams.MakeHeader(ctx.biomeRegistry);
				if(h.seed == 0) {
					h.seed = static_cast<int64_t>(SDL_GetPerformanceCounter());
					if(h.seed == 0) h.seed = 1;
				}

				ctx.grid->Clear();

				{
					for (const auto& dp : h.datapacks) {
						const std::string blocksPath  = (fs::path(dp) / "blocks.data").string();
						const std::string physicsPath = (fs::path(dp) / "physics_constants.data").string();
						ctx.blockRegistry->Clear();
						if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
							std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n", blocksPath.c_str());
						if (LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
							ctx.physics->SetConstants(*ctx.physicsConstants);
					}
				}

				TerrainGen::Params genParams;
				genParams.seed = h.seed;
				if(h.worldType == WorldFile::WorldType::Superflat) {
					genParams.superflatLayers = h.superflatLayers;
					if(genParams.superflatLayers.empty()) {
						genParams.noiseScale      = 0.0001f;
						genParams.heightAmplitude = 0;
						genParams.baseHeight      = 0;
					}
				} else if(h.worldType == WorldFile::WorldType::SingleBiome) {
					genParams.forceBiome = h.singleBiome;
				}
				genParams.structuresDir =
				    (fs::path(ctx.blocksDataPath).parent_path() / "structures").string();
				TerrainGen::Generate(*ctx.grid, *ctx.blockRegistry, ctx.biomeRegistry, genParams);

				ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
				ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
				ctx.hotbar->SetSlot(4, SAND);     ctx.hotbar->SetSlot(5, SNOW);
				ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);

				fs::create_directories(ctx.worldsDir);
				const std::string newPath = ctx.worldsDir + std::to_string(h.seed) + ".world";
				WorldFile::Save(newPath, h, *ctx.grid);

				worldSession.Enter(ctx, h, newPath);
				ctx.gameState = GameState::PLAYING;
			}
		}
	}

	ctx.debugOverlay->Render();
	return wantQuit;
}
