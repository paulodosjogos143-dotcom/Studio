// ROXBOX - adaptive quality manager
// Keeps a smooth 60 FPS on hardware from 15 GFLOPS phones up to desktop GPUs
// by auto-scaling: render distance, AO, resolution scale, meshing budget, fog.
#pragma once
#include <cstdint>

namespace rb {

struct QualityTier {
    int renderDistance;   // chunks around the player (16^3 each)
    bool ao;              // per-vertex ambient occlusion
    float resolutionScale; // internal resolution multiplier (0.5..1.5)
    int meshChunksPerFrame; // CPU meshing budget per frame
    float fogFar;
    int particles;        // particle budget
    const char* name;
};

// tier 0 must hold 60 FPS on 15 GFLOPS / 30k AnTuTu hardware (GLES2)
inline const QualityTier qualityTiers[4] = {
    { 2, false, 0.50f, 1, 60.f, 0,  "Minimum (15 GFLOPS)" },
    { 4, true,  0.65f, 2, 90.f, 24, "Low" },
    { 6, true,  0.85f, 4, 160.f, 64, "Medium" },
    { 9, true,  1.00f, 8, 260.f, 160, "Maximum" },
};

class QualityManager {
public:
    enum class Mode { Auto, Manual };

    Mode mode = Mode::Auto;
    int manualTier = 3;

    int tier = 3;
    bool ao() const { return qualityTiers[tier].ao; }
    float resolutionScale() const { return qualityTiers[tier].resolutionScale; }
    int renderDistance() const { return qualityTiers[tier].renderDistance; }
    int meshChunksPerFrame() const { return qualityTiers[tier].meshChunksPerFrame; }
    float fogFar() const { return qualityTiers[tier].fogFar; }
    int particleBudget() const { return qualityTiers[tier].particles; }
    const char* tierName() const { return qualityTiers[tier].name; }

    void setManual(int t) { mode = Mode::Manual; manualTier = clampTier(t); tier = manualTier; }
    void setAuto() { mode = Mode::Auto; }

    // feed the smoothed frame time (ms); auto-adjusts the tier to hold 60 FPS
    void update(float smoothedFrameMs);

    int clampTier(int t) const { return t < 0 ? 0 : (t > 3 ? 3 : t); }

private:
    float m_downCooldown = 0.f;
    float m_upCooldown = 1.5f;
};

} // namespace rb
