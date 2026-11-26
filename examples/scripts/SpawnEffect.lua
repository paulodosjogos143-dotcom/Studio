-- Example ROXBOX script: spawn a small glowing block above the part on a loop.
-- Demonstrates: Instance.new, properties, task.wait, Destroy.

local anchor = script.Parent

while true do
    local block = Instance.new("Part")
    block.Position = anchor.Position + Vector3.new(0, 4, 0)
    block.Size = Vector3.new(1, 1, 1)
    block.Color = Color3.new(1, 0.6, 0.1)
    block.Material = Enum.Material.Neon
    block.Parent = workspace

    -- float up and fade, then remove
    local lifetime = 0
    while lifetime < 2 do
        block.Position = block.Position + Vector3.new(0, 0.08, 0)
        lifetime = lifetime + 0.05
        task.wait(0.05)
    end
    Destroy(block)
    task.wait(1)
end
