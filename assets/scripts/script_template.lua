require("VoxelEngine")

-- called when the world/server starts
function GlobalStart() end
-- called server-side at a fixed rate (60hz)
function FixedGlobalUpdate() end

-- called when a player joins world/server (client-side)
function LocalStart(player) end
-- called every frame (client-side)
function LocalUpdate(player) end
-- called at a fixed rate (60hz) (client-side)
function FixedLocalUpdate(player) end