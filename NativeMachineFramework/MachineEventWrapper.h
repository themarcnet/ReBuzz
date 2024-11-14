#pragma once

#include "Buzz\MachineInterface.h"
#include <vector>

using BuzzGUI::Interfaces::IMachine;
using BuzzGUI::Common::Global;

namespace ReBuzz
{
    namespace NativeMachineFramework
    {

        public ref class MachineEventWrapper
        {
        public:
            MachineEventWrapper(IMachine^ self, CMachineInterface* machineIface);

            ~MachineEventWrapper();

            void OnEvent(IMachine^ machine);

            void AddEvent(EVENT_HANDLER_PTR p, void* param);

            void SetAction(System::Action<IMachine^>^ action);

            System::Action<IMachine^>^ GetAction();

        private:

            std::vector<EVENT_HANDLER_PTR>* m_callbacks;
            std::vector<void*>* m_callbackParams;
            System::Action<IMachine^>^ m_action;
            CMachineInterface* m_machineInterface;
            int64_t m_selfId;
        };
    }
}