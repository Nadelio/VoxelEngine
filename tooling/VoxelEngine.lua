-- EXTERNAL NAMESPACES
local engine = {}
local registry = {}
local world = {}
local physics = {}
local entity = {}
local player = {}
local particle = {}
local audio = {}
local events = {}
local keybind = {}
local chord = {}
local key = {}


-- TYPES
local Player = {}
local Keybind = {}
local Keychord = {}

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