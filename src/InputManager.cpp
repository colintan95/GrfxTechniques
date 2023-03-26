#include "InputManager.h"

InputHandle::~InputHandle()
{
    if (m_id && m_id.use_count() == 1)
    {
        assert(m_manager);
        m_manager->RemoveId(*m_id);
    }
}

InputHandle InputManager::AddKeyHoldListener(UINT keyCode, bool* value)
{
    auto handle = CreateHandle();

    KeyHoldEntry entry{};
    entry.Id = *handle.m_id;
    entry.Value = value;

    m_keyHoldEntries[keyCode].push_back(entry);

    return handle;
}

InputHandle InputManager::AddKeyPressListener(UINT keyCode, std::function<void()> callback)
{
    auto handle = CreateHandle();

    KeyPressEntry entry{};
    entry.Id = *handle.m_id;
    entry.Callback = std::move(callback);

    m_keyPressEntries[keyCode].push_back(entry);

    return handle;
}

InputHandle InputManager::AddMouseHoldListener(MouseButton type, bool* value, int modifier)
{
    auto handle = CreateHandle();

    MouseHoldEntry entry{};
    entry.Id = *handle.m_id;
    entry.Value = value;
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

    TraverseEntries(m_keyPressEntries[keyCode], [](KeyPressEntry& entry) {
        entry.Callback();
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

InputHandle InputManager::CreateHandle()
{
    InputHandle handle;
    handle.m_manager = this;
    handle.m_id = std::make_shared<InputId>(m_currentId);
    m_activeIds.insert(m_currentId);

    ++m_currentId;

    return handle;
}


void InputManager::RemoveId(InputId id)
{
    assert(m_activeIds.contains(id));
    m_activeIds.erase(id);
}
