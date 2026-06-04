local VoxelEngine = require("VoxelEngine")
local Vec3 = VoxelEngine.Vec3

local STONE = 1
local DIRT  = 2
local Team = require("team")
local redTeam = Team.new("Red")
local blueTeam = Team.new("Blue")

--- GLOBAL ENGINE FUNCTIONS ---

function GlobalStart() -- called upon server/world starting
    -- query the registry for blocks by ID
    local stone_block_data = registry.getBlock(STONE)
    if stone_block_data then
        engine.log("Block name: " .. stone_block_data.name)
        engine.log("Gravity:    " .. tostring(stone_block_data.affectedByGravity))
    end

    -- placing/removing blocks
    for x = -2, 2 do
        for z = -2, 2 do
            world.addBlock(x, 0, z, STONE)
        end
    end
    world.removeBlock(0, 0, 0)
    world.addBlock(0, 0, 0, DIRT)

    -- checking for blocks
    if world.hasBlock(0, 0, 0) then
        local id = world.getBlockID(0, 0, 0)
        engine.log("Block at origin: " .. id)
    end

    -- create custom keybind
    local custom_keybind = Keybind.new("goto_spawn", Keychord.new("<LS+P>"))
    registry.registerKeybind(custom_keybind)
end

function FixedGlobalUpdate() -- updates for the entire world at a fixed rate (60hz)
    if events["NewPlayer"].occured then
        if blueTeam:size() > redTeam:size() then
            redTeam:add(events["NewPlayer"].playerID)
        else
            blueTeam:add(events["NewPlayer"].playerID)
        end
    end

    if events["PlayerLeft"].occured then
        if blueTeam:has(events["PlayerLeft"].playerID) then
            blueTeam:remove(events["PlayerLeft"].playerID)
        else
            redTeam:remove(events["PlayerLeft"].playerID)
        end
    end

    if events["PlayerKilled"].occured then
        if blueTeam:has(events["PlayerKilled"].killedBy) then
            blueTeam:setScore(blueTeam.score + 1)
        else
            redTeam:setScore(redTeam.score + 1)
        end
    end
end

--- LOCAL ENGINE FUNCTIONS ---

function LocalStart(player) -- called upon player joining server/world
    player.registerState("DASHING");
    player.loadModel("./models/custom_player_model.gltf") -- all paths are restricted to the ./server/assets/ folder to avoid security issues
    player.loadTexture("./textures/models/custom_player_texture.png")

    player.hotbar.setSlot(0, STONE)
    player.hotbar.setSlot(1, DIRT)
    player.hotbar.selectSlot(0)
    engine.log(player.hotbar.currentSlot().isItem()) -- returns true/false based on if the slot contains an item
    engine.log(player.hotbar.currentSlot().isBlock()) -- returns true/false based on if the slot contains an block
    engine.log(player.hotbar.currentSlot().name()) -- returns the name of the block or item as a string
    player.hotbar.lock() -- prevent changes to the player's hotbar (like picking up, dropping items/blocks, or swapping items/blocks around in slots)

    player.teleportTo(Vec3.new(0, 5, 0))
    player.addVelocity(Vec3.new(0, 1, 0)) -- Add small upwards impulse force

    player.loadUI("./data/ui/player_ui.data") -- feed players custom UI
end

function LocalUpdate(player) -- updates for each individual player every frame
    -- do actions based on a keypress
    if key.isPressed["F"] then
        player.changeState("DASHING")
        player.playAnimation("DASH");
        audio.playLocalSound("dash_sound", player.getPosition());
        player.addVelocity(player.getForward() * 2) -- addVelocity() and Vec3 custom operator
    end

    -- do actions based on a keybind
    if keybind.isPressed["hotbar_1"] then
        player.hotbar.setSlot(0)
        player.teleportTo(world.spawn) -- teleport player to world spawn
    end

    -- do actions based on custom keybind
    if keybind.isPressed["goto_spawn"] then
        player.hotbar.setSlot(0)
        player.teleportTo(world.spawn)
    end

    -- do actions based on keychord
    if chord.isPressed("<LC,SP>") then
        player.addVelocity(Vec3.new(0, 5, 0)) -- "super jump"
    end
end

function FixedLocalUpdate(player) -- updates for each individual player at a fixed rate (60hz)
    -- query player position, velocity, and looking direction
    local cx, cy, cz = player.getPosition()
    local vx, vy, vz = player.getVelocity()
    local fx, fy, fz = player.getForward()
    engine.log(string.format("Looking from (%.1f, %.1f, %.1f) toward (%.2f, %.2f, %.2f)",
        cx, cy, cz, fx, fy, fz))
    engine.log("Player speed is: " .. vx .. ", " .. vy .. ", " .. vz)
        
    -- query player hotbar
    engine.log("Selected slot : " .. player.hotbar.getSelectedSlot())
    engine.log("Selected Slot Block/Item ID : " .. player.hotbar.currentSlot().id())

    -- query player state
    local state = player.getState()
    engine.log("Current player state: " .. state)
end
