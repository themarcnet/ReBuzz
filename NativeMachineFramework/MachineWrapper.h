#pragma once

#include "RefClassWrapper.h"
#include "RebuzzBuzzLookup.h"
#include "BuzzDataTypes.h"
#include "Buzz\MachineInterface.h"
#include "MachineCallbackWrapper.h"

using System::Windows::Forms::UserControl;
using System::String;

using BuzzGUI::Interfaces::IPattern;
using BuzzGUI::Interfaces::IParameter;
using BuzzGUI::Interfaces::BuzzCommand;
using Buzz::MachineInterface::IBuzzMachine;


namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        typedef OnNewBuzzLookupItemCallback OnNewPatternCallback;

        public ref class MachineWrapper
        {
        public:
            MachineWrapper( void * machine, IBuzzMachineHost^ host, IBuzzMachine^ buzzmachine,
                            void * callbackparam,
                            OnNewPatternCallback onNewPatternCallback);
            ~MachineWrapper();

            void Init();


            UserControl^ PatternEditorControl();

            void SetEditorPattern(IPattern^ pattern);

            void RecordControlChange(IParameter^ parameter, int track, int value);

            void SetTargetMachine(IMachine^ machine);

            void SetPatternName(String^ machine, String^ oldName, String^ newName);

            void * GetCPattern(IPattern^ p);

            IPattern^ GetReBuzzPattern(void* pat);

            void* GetCMachine(IMachine^ m);

            IMachine^ GetReBuzzMachine(void* mach);

            CMachineData* GetBuzzMachineData(void* mach);

            CPatternData* GetBuzzPatternData(void* pat);
            
            CMachine * GetCMachineByName(const char * name);

            void ControlChange(IMachine^ machine, int group, int track, int param, int value);

            void SetModifiedFlag();

            bool CanExecuteCommand(BuzzCommand cmd);

            void ExecuteCommand(BuzzCommand cmd);

            CMachineInterfaceEx * GetExInterface();

            void MidiNote(int channel, int value, int velocity);

            cli::array<byte>^ Save();

            void MidiControlChange(int ctrl, int channel, int value);

            cli::array<int>^ GetPatternEditorMachineMIDIEvents(IPattern^ pattern);

            void SetPatternEditorMachineMIDIEvents(IPattern^ pattern, cli::array<int>^ data);

            void Activate();

            void Release();

            void CreatePatternCopy(IPattern^ pnew, IPattern^ p);

            void UpdateMasterInfo();

        private:
            static IntPtr RebuzzWindowAttachCallback(IntPtr hwnd, void* callbackParam);
            static void RebuzzWindowDettachCallback(IntPtr cwnd, void* callbackParam);
            static void RebuzzWindowSizeCallback(IntPtr patternEditorHwnd, void* callbackParam, int left, int top, int width, int height);

            void OnMachineCreatedByReBuzz(IMachine^ machine);
            void OnMachineRemovedByReBuzz(IMachine^ machine);

            RebuzzBuzzLookup< IMachine, CMachineData, CMachine> * m_machineMap;
            RebuzzBuzzLookup<IPattern, CPatternData, CPattern> * m_patternMap;
            RefClassWrapper< MachineWrapper> * m_thisref;


            MachineCallbackWrapper * m_callback;
            CMachineInterface* m_machine;
            uint64_t m_thisMachineId;
            IBuzzMachineHost^ m_host;
            HWND m_hwndEditor;
            bool m_initialised;
            IBuzzMachine^ m_buzzmachine;
            CMasterInfo* m_masterInfo;
            void* m_machineCallbackData;

            OnNewPatternCallback m_onNewPatternCallback;
            void* m_callbackParam;

            CPattern * m_patternEditorPattern;
            CMachine* m_patternEditorMachine;

            System::Action<IMachine^>^ m_machineAddedAction;
            System::Action<IMachine^>^ m_machineRemovedAction;
            UserControl^ m_control;
        };
    }
}
