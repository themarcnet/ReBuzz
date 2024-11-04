#include "MachineEventWrapper.h"

namespace ReBuzz
{

    namespace NativeMachineFramework
    {

        MachineEventWrapper::MachineEventWrapper(CMachineInterface* machineIface) : m_machineInterface(machineIface),
            m_action(nullptr)
        {
            m_callbacks = new std::vector<EVENT_HANDLER_PTR>();
            m_callbackParams = new std::vector<void*>();
        }

        MachineEventWrapper::~MachineEventWrapper()
        {
            delete m_callbacks;
            delete m_callbackParams;
        }

        void MachineEventWrapper::OnEvent(IMachine^ machine)
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

        void MachineEventWrapper::AddEvent(EVENT_HANDLER_PTR p, void* param)
        {
            m_callbacks->push_back(p);
            m_callbackParams->push_back(param);
        }

        void MachineEventWrapper::SetAction(System::Action<IMachine^>^ action)
        {
            m_action = action;
        }

        System::Action<IMachine^>^ MachineEventWrapper::GetAction()
        {
            return m_action;
        }
    }
}