---@class Team
---@field name string
---@field players integer[]
---@field score integer
local Team = {}
Team.__index = Team

---@param name string
function Team.new(name)
    local self = setmetatable({}, Team)
    self.name = name
    self.players = {}
    self.score = 0
    return self
end

---@param playerID integer
function Team:add(playerID)
    table.insert(self.players, playerID)
end

---@param playerID integer
function Team:remove(playerID)
    for i, p in ipairs(self.players) do
        if p == playerID then
            table.remove(self.players, i)
            return
        end
    end
end

---@param playerID integer
---@return boolean
function Team:has(playerID)
    for _, p in ipairs(self.players) do
        if p == playerID then return true end
    end
    return false
end

---@return integer
function Team:size()
    return #self.players
end

---@param newScore integer
function Team:setScore(newScore)
    self.score = newScore
end

return Team