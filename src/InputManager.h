#pragma once

#include <windows.h>

#include <cassert>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using InputId = int;

class InputHandle
{
public:
    InputHandle() = default;
    ~InputHandle();

private:
    friend class InputManager;

    InputManager* m_manager;
    std::shared_ptr<InputId> m_id;
};

enum class MouseButton
{
    Left,
    Middle,
    Right
};

struct ModifierKey
{
    enum
    {
        None = 0,
        Shift = 1,
        Ctrl = 2
    };
};

class InputManager
{
public:
    InputHandle AddKeyHoldListener(UINT keyCode, bool* value);

    InputHandle AddMouseHoldListener(MouseButton button, bool* value,
                                     int modifier = ModifierKey::None);

    void HandleKeyDown(UINT keyCode);
    void HandleKeyUp(UINT keyCode);

    void HandleMouseDown(MouseButton button);
    void HandleMouseUp(MouseButton button);

private:
    friend class InputHandle;

    template<typename Entry, typename Fn>
    void TraverseEntries(std::vector<Entry>& entries, Fn fn)
    {
        auto it = entries.begin();
        while (it != entries.end())
        {
            if (m_activeIds.contains(it->Id))
            {
                fn(*it);
                ++it;
            }
            else
            {
                it = entries.erase(it);
            }
        }
    }

    InputHandle CreateHandle();

    void RemoveId(InputId id);

    struct KeyHoldEntry
    {
        InputId Id;
        bool* Value;
    };

    std::unordered_map<UINT, std::vector<KeyHoldEntry>> m_keyHoldEntries;

    struct MouseHoldEntry
    {
        InputId Id;
        bool* Value;

        int Modifier = ModifierKey::None;
    };

    std::unordered_map<int, std::vector<MouseHoldEntry>> m_mouseHoldEntries;

    int m_currentId = 1;
    std::unordered_set<InputId> m_activeIds;

    bool m_shiftDown = false;
};
