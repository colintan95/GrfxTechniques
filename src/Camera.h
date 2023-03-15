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

    InputHandle<bool> m_upKeyDown;
    InputHandle<bool> m_downKeyDown;
    InputHandle<bool> m_leftKeyDown;
    InputHandle<bool> m_rightKeyDown;

    InputHandle<bool> m_middleMouseDown;

    std::optional<int> m_prevMouseX;
    std::optional<int> m_prevMouseY;
};
