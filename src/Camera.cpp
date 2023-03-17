#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <winrt/base.h>

#pragma warning(push)
#pragma warning(disable:4201)
#include <glm/gtx/euler_angles.hpp>
#pragma warning(pop)

using winrt::check_bool;

Camera::Camera(InputManager* inputManager)
    : m_inputManager(inputManager)
{
    m_position = glm::vec3(0.f, 0.f, -4.f);

    m_upKeyDown = m_inputManager->AddKeyHoldListener('W');
    m_downKeyDown = m_inputManager->AddKeyHoldListener('S');
    m_leftKeyDown = m_inputManager->AddKeyHoldListener('A');
    m_rightKeyDown = m_inputManager->AddKeyHoldListener('D');

    m_middleMouseDown = m_inputManager->AddMouseHouseListener(MouseButtonType::Middle);
}

void Camera::Tick(double elapsedSec)
{
    POINT currentMousePos{};
    check_bool(GetCursorPos(&currentMousePos));

    float mouseXDiff = 0.f;
    float mouseYDiff = 0.f;

    if (m_prevMouseX)
    {
        mouseXDiff = static_cast<float>(currentMousePos.x - *m_prevMouseX);
    }

    if (m_prevMouseY)
    {
        mouseYDiff = static_cast<float>(currentMousePos.y - *m_prevMouseY);
    }

    m_prevMouseX = currentMousePos.x;
    m_prevMouseY = currentMousePos.y;

    if (m_middleMouseDown.GetValue())
    {
        glm::mat4 rotateMat = glm::yawPitchRoll(m_yaw, m_pitch, 0.f);

        glm::vec3 upVec = glm::vec3(rotateMat * glm::vec4(0.f, 1.f, 0.f, 1.f));
        glm::vec3 rightVec = glm::vec3(rotateMat * glm::vec4(1.f, 0.f, 0.f, 1.f));

        static constexpr float lookSpeed = 0.005f;

        m_position += mouseXDiff * lookSpeed * rightVec;
        m_position -= mouseYDiff * lookSpeed * upVec;
    }
    else
    {
        glm::mat4 rotateMat = glm::yawPitchRoll(m_yaw, m_pitch, 0.f);

        glm::vec3 forwardVec = glm::vec3(rotateMat * glm::vec4(0.f, 0.f, 1.f, 0.f));
        glm::vec3 rightVec = glm::vec3(rotateMat * glm::vec4(1.f, 0.f, 0.f, 1.f));

        static constexpr float moveSpeed = 2.0f;

        float moveDist = static_cast<float>(elapsedSec) * moveSpeed;
        float moveDiagDist = static_cast<float>(elapsedSec) * moveSpeed / std::sqrt(2.f);

        bool upKeyDown = m_upKeyDown.GetValue();
        bool downKeyDown = m_downKeyDown.GetValue();
        bool leftKeyDown = m_leftKeyDown.GetValue();
        bool rightKeyDown = m_rightKeyDown.GetValue();

        if (upKeyDown && !downKeyDown)
        {
            if (leftKeyDown && !rightKeyDown)
            {
                m_position += moveDiagDist * forwardVec;
                m_position -= moveDiagDist * rightVec;
            }
            else if (rightKeyDown)
            {
                m_position += moveDiagDist * forwardVec;
                m_position += moveDiagDist * rightVec;
            }
            else
            {
                m_position += moveDist * forwardVec;
            }
        }
        else if (downKeyDown)
        {
            if (leftKeyDown && !rightKeyDown)
            {
                m_position -= moveDiagDist * forwardVec;
                m_position -= moveDiagDist * rightVec;
            }
            else if (rightKeyDown)
            {
                m_position -= moveDiagDist * forwardVec;
                m_position += moveDiagDist * rightVec;
            }
            else
            {
                m_position -= moveDist * forwardVec;
            }
        }
        else
        {
            if (leftKeyDown && !rightKeyDown)
            {
                m_position -= moveDist * rightVec;
            }
            else if (rightKeyDown)
            {
                m_position += moveDist * rightVec;
            }
        }

        static constexpr float lookSpeed = 0.001f;

        if (m_prevMouseX)
        {
            m_yaw += mouseXDiff * lookSpeed;
        }

        if (m_prevMouseY)
        {
            m_pitch += mouseYDiff * lookSpeed;
        }
    }
}

glm::mat4 Camera::GetViewMat()
{
    return glm::eulerAngleXY(-m_pitch, -m_yaw) * glm::translate(glm::mat4(1.f), -m_position);
}
