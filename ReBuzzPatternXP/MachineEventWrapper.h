#pragma once

#include "..\..\Buzz\MachineInterface.h"
#include <vector>

using BuzzGUI::Interfaces::IMachine;
using BuzzGUI::Common::Global;

public ref class MachineEventWrapper
{
public:
    MachineEventWrapper(CMachineInterface * machineIface) : m_machineInterface(machineIface),
                                                            m_action(nullptr)
    {
        m_callbacks = new std::vector<EVENT_HANDLER_PTR>();
        m_callbackParams = new std::vector<void*>();
    }

    ~MachineEventWrapper()
    {
        delete m_callbacks;
        delete m_callbackParams;
    }

    void OnEvent(IMachine^ machine)
    {
        if ((m_callbacks == NULL) || (m_callbackParams == NULL))
            return;

        //Call all the callbacks
        for (size_t x = 0; x < m_callbacks->size(); ++x)
        {
            EVENT_HANDLER_PTR callback = (*m_callbacks)[x];
            void* param = (*m_callbackParams)[x];
            (*m_machineInterface.*callback)(param);
        }
    }

    void AddEvent(EVENT_HANDLER_PTR p, void* param)
    {
        m_callbacks->push_back(p);
        m_callbackParams->push_back(param);
    }

    void SetAction(System::Action<IMachine^>^ action)
    {
        m_action = action;
    }

    System::Action<IMachine^>^ GetAction()
    {
        return m_action;
    }

private:

    std::vector<EVENT_HANDLER_PTR> * m_callbacks;
    std::vector<void*> * m_callbackParams;
    System::Action<IMachine^>^ m_action;
    CMachineInterface* m_machineInterface;
};