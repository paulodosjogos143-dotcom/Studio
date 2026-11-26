#include "quality.h"
#include <algorithm>

namespace rb {

void QualityManager::update(float smoothedFrameMs) {
    if (mode != Mode::Auto) return;

    // hysteresis: degrade fast when under target, upgrade slowly when headroom
    m_downCooldown -= 1.f / 60.f;
    m_upCooldown -= 1.f / 60.f;

    if (smoothedFrameMs > 20.f && m_downCooldown <= 0.f) {
        if (tier > 0) {
            --tier;
            m_downCooldown = 1.0f; // wait a second before further degradation
        }
    } else if (smoothedFrameMs < 12.5f && m_upCooldown <= 0.f && tier < 3) {
        ++tier;
        m_upCooldown = 4.0f; // upgrade slowly to avoid oscillation
    }
}

} // namespace rb
