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

InputHandle<bool> InputManager::AddMouseHouseListener(MouseButtonType type)
{
    auto handle = CreateHandle<bool>();

    MouseHoldEntry entry{};
    entry.Id = *handle.m_id;
    entry.Value = handle.m_value;

    m_mouseHoldEntries[static_cast<int>(type)].push_back(entry);

    return handle;
}

void InputManager::HandleKeyDown(UINT keyCode)
{
    TraverseEntries(m_keyHoldEntries[keyCode], [](KeyHoldEntry& entry) {
        *entry.Value = true;
    });
}

void InputManager::HandleKeyUp(UINT keyCode)
{
    TraverseEntries(m_keyHoldEntries[keyCode], [](KeyHoldEntry& entry) {
        *entry.Value = false;
    });
}

void InputManager::HandleMouseDown(MouseButtonType type)
{
    TraverseEntries(m_mouseHoldEntries[static_cast<int>(type)], [](MouseHoldEntry& entry) {
        *entry.Value = true;
    });
}

void InputManager::HandleMouseUp(MouseButtonType type)
{
    TraverseEntries(m_mouseHoldEntries[static_cast<int>(type)], [](MouseHoldEntry& entry) {
        *entry.Value = true;
    });
}

void InputManager::RemoveId(InputId id)
{
    assert(m_activeIds.contains(id));
    m_activeIds.erase(id);
}
