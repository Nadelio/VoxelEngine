-- EXTERNAL NAMESPACES
local engine = {
    log = function (message) end
}
local registry = {
    registerKeybind = function (keybind) end,
    getBlock = function (blockID) end, -- returns Block type
    getItem = function (itemID) end,
    getByID = function (absoluteID) end, -- returns block/item based on absoluteID
}
local world = {
    addBlock = function (position, blockID) end,
    removeBlock = function (position) end,
    getBlockID = function (position) end,
    hasBlock = function (position) end,
    assertBlockAt = function (position, blockID) end,
    spawn = Vec3.new(0, 0, 0) -- this can change if world spawn gets changed by command 
}
local physics = {} -- currently unimplemented
local entity = {} -- currently unimplemented
local particle = {} -- currently unimplemented
local audio = {
    playLocalSound = function (sound, position) end
}
local events = {
    NewPlayer = {
        occured = false,
        playerID = 0,
    },
    PlayerJoined = {
        occured = false,
        playerID = 0,
        timestamp = 0,
    },
    PlayerLeft = {
        occured = false,
        playerID = 0,
        timestamp = 0,
    },
    PlayerSpawned = {
        occured = false,
        playerID = 0,
        timestamp = 0
    },
    PlayerKilled = {
        occured = false,
        playerID = 0,
        killedBy = 0,
        timestamp = 0
    }
}
local keybind = {
    isPressed = {
        -- all the keyboard keys with a boolean value
    },
    isReleased = {
        -- all the keyboard keys with a boolean value
    },
    isHeld = {
        -- all the keyboard keys with a boolean value
    }
}
local chord = {
    isPressed = {
        -- all the keyboard keys with a boolean value
    },
    isReleased = {
        -- all the keyboard keys with a boolean value
    },
    isHeld = {
        -- all the keyboard keys with a boolean value
    }
}
local key = {
    isPressed = {
        -- all the keyboard keys with a boolean value
    },
    isReleased = {
        -- all the keyboard keys with a boolean value
    },
    isHeld = {
        -- all the keyboard keys with a boolean value
    }
}


-- TYPES
local Player = {
    hotbar = {
        setSlot = function (absoluteID) end, -- absoluteID means both block and item ids
        selectSlot = function (slotIndex) end, -- returns true if successful
        lock = function () end, -- returns true if successful
        currentSlot = function () end, -- returns a Slot type
        getSelectedSlot = function () end, -- returns slotIndex
    },
    teleportTo = function (position) end, -- returns true if successful
    addVelocity = function (forceVector) end, -- returns true if successful
    registerState = function (newState) end, -- returns true if successful
    loadModel = function (modelPath) end, -- returns true if successful
    loadTexture = function (texturePath) end, -- returns true if successful
    loadUI = function (UIPath) end, -- returns true if successful
    playAnimation = function (animationName) end, -- returns true if successful
    changeState = function (stateName) end, -- returns true if successful
    getVelocity = function () end, -- returns Vec3
    getPosition = function () end, -- returns Vec3
    getForward = function () end, -- returns Vec3
    getState = function () end, -- returns integer
}
local Keybind = {
    new = function (keybindName, chord) end
}
local Keychord = {
    new = function (chordAsString) end
}
local Slot = {
    _id = 0, -- absoluteID
    _isBlock = false,
    _name = "",
    isItem = function () end, -- returns !isBlock
    isBlock = function () end, -- returns isBlock
    name = function () end, -- returns name
    id = function () end, -- returns absoluteID
    relativeID = function () end, -- returns relative id (if block, return blockID, if item, return itemID)
}
---@class Block
local Block = {
    blockID = 0,
    name = "",
    affectedByGravity = false,
    can_rotate = false,
    texture = {
        front = { 0, 0 },
        back = { 0, 0 },
        left = { 0, 0 },
        right = { 0, 0 },
        top = { 0, 0 },
        bttom = { 0, 0 },
    },
    temp = { -1.0, 1.0 },
    elevation = { -64, 256 },
    depth = { 0, 512 },
    biome = { "all" },
    gen_tag = { "mix" },
    group = { "none" }
}

---@class Vec3
---@field x number
---@field y number
---@field z number
local Vec3 = {}
Vec3.__index = Vec3

---@param x number
---@param y number
---@param z number
---@return Vec3
function Vec3.new(x, y, z)
    return setmetatable({ x = x, y = y, z = z }, Vec3)
end

-- Arithmetic
function Vec3.__add(a, b) return Vec3.new(a.x+b.x, a.y+b.y, a.z+b.z) end
function Vec3.__sub(a, b) return Vec3.new(a.x-b.x, a.y-b.y, a.z-b.z) end
function Vec3.__unm(a)    return Vec3.new(-a.x, -a.y, -a.z) end

-- Scalar multiply/divide
function Vec3.__mul(a, b)
    if type(a) == "number" then return Vec3.new(a*b.x, a*b.y, a*b.z) end
    if type(b) == "number" then return Vec3.new(a.x*b, a.y*b, a.z*b) end
end
function Vec3.__div(a, b) return Vec3.new(a.x/b, a.y/b, a.z/b) end

-- Equality
function Vec3.__eq(a, b) return a.x==b.x and a.y==b.y and a.z==b.z end

-- String
function Vec3.__tostring(v)
    return string.format("(%.3f, %.3f, %.3f)", v.x, v.y, v.z)
end

---@return number
function Vec3:length()
    return math.sqrt(self.x^2 + self.y^2 + self.z^2)
end

---@return number
function Vec3:lengthSq()
    return self.x^2 + self.y^2 + self.z^2
end

---@return Vec3
function Vec3:normalize()
    local len = self:length()
    if len == 0 then return Vec3.new(0, 0, 0) end
    return self / len
end

---@param other Vec3
---@return number
function Vec3:dot(other)
    return self.x*other.x + self.y*other.y + self.z*other.z
end

---@param other Vec3
---@return Vec3
function Vec3:cross(other)
    return Vec3.new(
        self.y*other.z - self.z*other.y,
        self.z*other.x - self.x*other.z,
        self.x*other.y - self.y*other.x
    )
end

---@param other Vec3
---@return number
function Vec3:distance(other)
    return (self - other):length()
end

---@param other Vec3
---@param t number
---@return Vec3
function Vec3:lerp(other, t)
    return self + (other - self) * t
end

Vec3.zero = Vec3.new(0, 0, 0)

return { Vec3 = Vec3 }