#include "InputManager.h"

InputHandle<bool> InputManager::AddKeyHoldListener(UINT keyCode)
{
    auto handle = CreateHandle<bool>();

    KeyHoldEntry entry{};
    entry.Id = *handle.m_id;
    entry.Value = handle.m_value;

    m_keyHoldEntries[keyCode].push_back(entry);

    return handle;
}

InputHandle<bool> InputManager::AddMouseHoldListener(MouseButton type, int modifier)
{
    auto handle = CreateHandle<bool>();

    MouseHoldEntry entry{};
    entry.Id = *handle.m_id;
    entry.Value = handle.m_value;
    entry.Modifier = modifier;

    m_mouseHoldEntries[static_cast<int>(type)].push_back(entry);

    return handle;
}

void InputManager::HandleKeyDown(UINT keyCode)
{
    if (keyCode == VK_SHIFT)
    {
        m_shiftDown = true;
        return;
    }

    TraverseEntries(m_keyHoldEntries[keyCode], [](KeyHoldEntry& entry) {
        *entry.Value = true;
    });
}

void InputManager::HandleKeyUp(UINT keyCode)
{
    if (keyCode == VK_SHIFT)
    {
        m_shiftDown = false;
        return;
    }

    TraverseEntries(m_keyHoldEntries[keyCode], [](KeyHoldEntry& entry) {
        *entry.Value = false;
    });
}

void InputManager::HandleMouseDown(MouseButton type)
{
    TraverseEntries(m_mouseHoldEntries[static_cast<int>(type)], [this](MouseHoldEntry& entry) {
        if ((entry.Modifier & ModifierKey::Shift) == ModifierKey::Shift)
        {
            if (!m_shiftDown)
                return;
        }
        else
        {
            if (m_shiftDown)
                return;
        }

        *entry.Value = true;
    });
}

void InputManager::HandleMouseUp(MouseButton type)
{
    TraverseEntries(m_mouseHoldEntries[static_cast<int>(type)], [](MouseHoldEntry& entry) {
        *entry.Value = false;
    });
}

void InputManager::RemoveId(InputId id)
{
    assert(m_activeIds.contains(id));
    m_activeIds.erase(id);
}
