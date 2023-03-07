#include "InputManager.h"

InputHandle<bool> InputManager::AddKeyHoldListener(UINT keyCode)
{
    auto handle = CreateHandle<bool>();

    KeyHoldEntry entry{};
    entry.Id = *handle.m_id;
    entry.Value = handle.m_value.get();

    m_keyHoldEntries[keyCode].push_back(entry);

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

void InputManager::RemoveId(InputId id)
{
    assert(m_activeIds.contains(id));
    m_activeIds.erase(id);
}
