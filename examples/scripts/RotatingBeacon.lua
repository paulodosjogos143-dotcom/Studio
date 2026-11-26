-- Example ROXBOX script: rotating beacon light.
-- Attach to a Part in Studio (or set as scriptSource) to see it run.
-- Demonstrates: script.Parent, task.wait, math, property writes.

local beacon = script.Parent
local t = 0

while true do
    t = t + 0.05
    -- pulse transparency between clear and half-transparent
    beacon.Transparency = 0.0 + (math.sin(t) * 0.5 + 0.5) * 0.6
    task.wait(0.05)
end
