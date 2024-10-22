#pragma once

#include "Buzz\MachineInterface.h"

#include "RebuzzBuzzLookup.h"
#include "BuzzDataTypes.h"

using BuzzGUI::Interfaces::IMachine;

namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        public ref class MachineManager
        {
        public:
            MachineManager();

            ~MachineManager();

            IMachine^ GetReBuzzMachine(CMachine * mach);

            CMachineData* GetBuzzMachineData(CMachine* mach);

            CMachine* GetCMachineByName(const char* name);

            CMachine * GetOrStoreMachine(IMachine^ m);

       

        private:
            void OnMachineCreatedByReBuzz(IMachine^ machine);
            void OnMachineRemovedByReBuzz(IMachine^ machine);

            RebuzzBuzzLookup< IMachine, CMachineData, CMachine>* m_machineMap;
            void* m_machineCallbackData;
            std::mutex* m_lock;
            System::Action<IMachine^>^ m_machineAddedAction;
            System::Action<IMachine^>^ m_machineRemovedAction;
        };
    }
}
