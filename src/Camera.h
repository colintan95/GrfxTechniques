#pragma once

#include "InputManager.h"

#include <glm/glm.hpp>

#include <optional>

class Camera
{
public:
    Camera(InputManager* inputManager);

    void Tick(double elapsedSec);

    glm::mat4 GetViewMat();

private:
    InputManager* m_inputManager;

    glm::vec3 m_position;

    float m_yaw = 0.f;
    float m_pitch = 0.f;

    bool m_upKeyDown = false;
    bool m_downKeyDown = false;
    bool m_leftKeyDown = false;
    bool m_rightKeyDown = false;

    bool m_middleMouseDown = false;

    std::optional<int> m_prevMouseX;
    std::optional<int> m_prevMouseY;

    InputHandle m_upKeyDownHandle;
    InputHandle m_downKeyDownHandle;
    InputHandle m_leftKeyDownHandle;
    InputHandle m_rightKeyDownHandle;

    InputHandle m_middleMouseDownHandle;
};
