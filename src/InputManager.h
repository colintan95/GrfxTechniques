#pragma once

#include <windows.h>

#include <cassert>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using InputId = int;

template<typename T>
class InputHandle
{
public:
    InputHandle() = default;

    ~InputHandle()
    {
        if (m_id && m_id.use_count() == 1)
        {
            assert(m_manager);
            m_manager->RemoveId(*m_id);
        }
    }

    const T& GetValue() const
    {
        return *m_value;
    }

private:
    friend class InputManager;

    InputManager* m_manager;
    std::shared_ptr<InputId> m_id;

    std::shared_ptr<T> m_value;
};

class InputManager
{
public:
    InputHandle<bool> AddKeyHoldListener(UINT keyCode);

    void HandleKeyDown(UINT keyCode);

    void HandleKeyUp(UINT keyCode);

private:
    template<typename T>
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

    template<typename T>
    InputHandle<T> CreateHandle()
    {
        InputHandle<T> handle;
        handle.m_manager = this;
        handle.m_id = std::make_shared<InputId>(m_currentId);
        m_activeIds.insert(m_currentId);

        ++m_currentId;

        handle.m_value = std::make_shared<T>();

        return handle;
    }

    void RemoveId(InputId id);

    struct KeyHoldEntry
    {
        InputId Id;
        std::shared_ptr<bool> Value;
    };

    std::unordered_map<UINT, std::vector<KeyHoldEntry>> m_keyHoldEntries;

    int m_currentId = 1;
    std::unordered_set<InputId> m_activeIds;
};
