local Team = {}
Team.__index = Team

function Team.new(name)
    local self = setmetatable({}, Team)
    self.name = name
    self.players = {}
    return self
end

function Team:add(playerID)
    table.insert(self.players, playerID)
end

function Team:remove(playerID)
    for i, p in ipairs(self.players) do
        if p == playerID then
            table.remove(self.players, i)
            return
        end
    end
end

function Team:has(playerID)
    for _, p in ipairs(self.players) do
        if p == playerID then return true end
    end
    return false
end

function Team:size()
    return #self.players
end

return Team