#pragma once

#include "Buzz\MachineInterface.h"
#include "RefClassWrapper.h"
#include "RebuzzBuzzLookup.h"

using namespace  System::Runtime::InteropServices;
using Buzz::MachineInterface::IBuzzMachineHost;
using Buzz::MachineInterface::IBuzzMachine;
using BuzzGUI::Interfaces::IMachine;
using System::IntPtr;


#ifdef _BUILD_NATIVEFW_DLL
    #define NATIVE_MACHINE_FW_EXPORT  __declspec(dllexport)
#else
    #define NATIVE_MACHINE_FW_EXPORT  __declspec(dllimport)
#endif 
namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        //NOTE: Not everycallback is implemented - only those that PatternXP requires!
        //      Everything else is stubbed out.
        class  MachineCallbackWrapper : CMICallbacks
        {
        public:
            MachineCallbackWrapper(ref class MachineWrapper^ mw,
                                   IBuzzMachine^ netmach,
                                   IBuzzMachineHost^ host,
                                   CMachineInterface* iface, 
                                   CMachine * machine,
                                   CMasterInfo * masterInfo);

            ~MachineCallbackWrapper();

            CMachineInterfaceEx* GetExInterface() const;

            
            //------------------------------------------------------------------------------------

            //Implementation of CMICallbacks (in no particular order)
            void SetMachineInterfaceEx(CMachineInterfaceEx* pex) override;
            CMachine* GetThisMachine() override;
            void SetEventHandler(CMachine* pmac, BEventType et, EVENT_HANDLER_PTR p, void* param) override;
            void SetModifiedFlag() override;
            CSubTickInfo const* GetSubTickInfo() override;
            CMachineInfo const* GetMachineInfo(CMachine* pmac) override;
            char const* GetMachineName(CMachine* pmac) override;
            CMachine* GetMachine(char const* name) override;
            dword GetThemeColor(char const* name) override;
            int GetNumTracks(CMachine* pmac) override;
            char const* GetPatternName(CPattern* ppat) override;
            void SetPatternEditorStatusText(int pane, char const* text) override;
            int GetPatternLength(CPattern* p) override;

            //------------------------------------------------------------------------------------
            //API that is delayed - to be called API when exInterface is set
            void SetDelayedEditorPattern(CMachine* pmach, CPattern* ppat)
            {
                m_setPatternEditorPattern = ppat;
                m_setPatternEditorMachine = pmach;
            }

        private:

            void UpdateMasterInfo();


            RefClassWrapper<IBuzzMachine> m_netmcahine;
            RefClassWrapper<IBuzzMachineHost> m_machinehost;
            RefClassWrapper<ref class MachineWrapper> m_machineWrapper;
            CMachine* m_thisMachine;
            CSubTickInfo m_subtickInfo;

            CMachineInterfaceEx* m_exInterface;
            CMachineInterface* m_interface;
            CMasterInfo* m_masterinfo;
            
            
            RefClassWrapper<ref class MachineEventWrapper> m_addedMachineEventHandler;
            RefClassWrapper<ref class MachineEventWrapper> m_deleteMachineEventHandler;

            CPattern* m_setPatternEditorPattern;
            CMachine* m_setPatternEditorMachine;

            std::string m_statusBarText0;
            std::string m_statusBarText1;
        };

    }
}