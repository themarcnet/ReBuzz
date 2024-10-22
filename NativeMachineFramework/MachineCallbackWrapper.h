#pragma once

#include "Buzz\MachineInterface.h"
#include "RefClassWrapper.h"
#include "RebuzzBuzzLookup.h"

using namespace  System::Runtime::InteropServices;
using Buzz::MachineInterface::IBuzzMachineHost;
using Buzz::MachineInterface::IBuzzMachine;
using BuzzGUI::Interfaces::IMachine;
using BuzzGUI::Interfaces::IWaveLayer;
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
            void GetMachineNames(CMachineDataOutput* pout) override;
            dword GetThemeColor(char const* name) override;
            int GetNumTracks(CMachine* pmac) override;
            char const* GetPatternName(CPattern* ppat) override;
            CPattern* GetPatternByName(CMachine* pmac, const char* name) override;
            void SetPatternEditorStatusText(int pane, char const* text) override;
            int GetPatternLength(CPattern* p) override;
            void SetPatternLength(CPattern* p, int length) override;
            void SetPatternName(CPattern* p, char const* name) override;

            CMachine* GetPatternOwner(CPattern* p) override;
            void RemapLoadedMachineName(char* name, int bufsize) override;
            CWaveLevel const* GetNearestWaveLevel(int const i, int const note) override;
            int GetHostVersion() override;
            CSequence* GetPlayingSequence(CMachine* pmac) override;
            CPattern* GetPlayingPattern(CSequence* pseq) override;
            int GetSequenceColumn(CSequence* s) override;
            int GetStateFlags() override;
            void ControlChange(CMachine* pmac, int group, int track, int param, int value) override;
            void SendControlChanges(CMachine* pmac) override;
            

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