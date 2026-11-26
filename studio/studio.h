// ROXBOX Studio - editor application
#pragma once
#include "../engine/engine.h"
#include "../engine/game/character.h"
#include <string>

namespace rb {

class StudioApp : public Engine {
public:
    bool init(const EngineConfig& cfg) override;

    void onUpdate(float dt) override;
    void onRenderUI(UI& ui, const InputState& in) override;

    // tool state
    enum class Tool { Select, Build, Paint, Erase, Move };
    Tool tool() const { return m_tool; }

private:
    void updateEditMode(float dt);
    void updatePlayMode(float dt);
    void handleViewportInput(const InputState& in, float dt);
    void handleToolAction(const InputState& in);
    void drawViewportDecorations(UI& ui);
    void drawToolbar(UI& ui, const InputState& in);
    void drawWorldPanel(UI& ui, const InputState& in, float px, float pw);
    void drawPropertiesPanel(UI& ui, const InputState& in, float px, float pw);
    void drawScriptPanel(UI& ui, const InputState& in, float px, float pw);
    void drawSettingsPanel(UI& ui, const InputState& in, float px, float pw);
    void drawStatusBar(UI& ui);

    vec3 unprojectRay(const InputState& in, vec3& dirOut);
    vec2 projectToScreen(const vec3& worldPos);

    Tool m_tool = Tool::Select;
    int m_material = 0;               // Material index
    int m_color = 0;                  // palette index (0 = default)
    int m_selectedPartId = -1;
    bool m_playMode = false;
    Character m_character;

    // editing state
    bool m_draggingMove = false;
    vec3 m_dragStartPos{0, 0, 0};
    vec2 m_dragStartScreen{0, 0};

    // panels
    std::string m_scriptText;
    std::string m_savePath = "saves/world.roxmap";
    std::string m_loadPath = "saves/world.roxmap";
    bool m_showGrid = true;

    // viewport interaction state
    float m_lastMouseX = 0.f, m_lastMouseY = 0.f;
    bool m_wasMouseDown = false;
    RayHit m_lastHit;
};

} // namespace rb
