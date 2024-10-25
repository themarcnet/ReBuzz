#pragma once

#include "RefClassWrapper.h"
#include "RebuzzBuzzLookup.h"
#include "BuzzDataTypes.h"
#include "Buzz\MachineInterface.h"
#include "MachineCallbackWrapper.h"
#include "MachineManager.h"
#include "PatternManager.h"

using System::Windows::Forms::UserControl;
using System::Windows::Forms::KeyEventArgs;
using System::Windows::Forms::KeyEventHandler;
using System::String;

using BuzzGUI::Interfaces::IPattern;
using BuzzGUI::Interfaces::IParameter;
using BuzzGUI::Interfaces::BuzzCommand;
using BuzzGUI::Interfaces::ISequence;
using Buzz::MachineInterface::IBuzzMachine;


namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        typedef OnNewBuzzLookupItemCallback OnNewPatternCallback;

        typedef void* (*KeyboardFocusWindowHandleCallback)(void* param);

        typedef OnPatternEditorRedrawCallback OnPatEditorRedrawCallback(void* param);

        public ref class MachineWrapper
        {
        public:
            MachineWrapper( void * machine, IBuzzMachineHost^ host, IBuzzMachine^ buzzmachine,
                            void * callbackparam,
                            KeyboardFocusWindowHandleCallback kbcallback,
                            OnPatternEditorRedrawCallback redrawcallback);

            ~MachineWrapper();

            void Init();

            IMachine^ GetThisReBuzzMachine();

            UserControl^ PatternEditorControl();

            void SetEditorPattern(IPattern^ pattern);

            void RecordControlChange(IParameter^ parameter, int track, int value);

            void SetTargetMachine(IMachine^ machine);

            void * GetCPattern(IPattern^ p);

            IPattern^ GetReBuzzPattern(void* pat);

            void* GetCMachine(IMachine^ m);

            IMachine^ GetReBuzzMachine(void* mach);

            CMachineData* GetBuzzMachineData(void* mach);

            CPatternData* GetBuzzPatternData(void* pat);

            void* GetCPatternByName(IMachine^ rebuzzmac, const char* name);
            
            CMachine * GetCMachineByName(const char * name);

            CWaveLevel* GetWaveLevel(IWaveLayer^ wavelayer);

            IWaveLayer^ GetReBuzzWaveLevel(CWaveLevel* wavelevel);

            CSequence* GetSequence(ISequence^ seq);

            ISequence^ GetReBuzzSequence(CSequence* seq);

            void UpdatePattern(CPattern* pat, int newLen, const char * newName);

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

            void * CreatePattern(IMachine^ machine, const char * name, int len);

            void CreatePatternCopy(IPattern^ pnew, IPattern^ p);

            void UpdateMasterInfo();

            void NotifyOfPlayingPattern();

            void Tick();

            
        private:
            static IntPtr RebuzzWindowAttachCallback(IntPtr hwnd, void* callbackParam);
            static void RebuzzWindowDettachCallback(IntPtr cwnd, void* callbackParam);
            static void RebuzzWindowSizeCallback(IntPtr patternEditorHwnd, void* callbackParam, int left, int top, int width, int height);
            
            void OnSequenceCreatedByReBuzz(int seq);
            void OnSequecneRemovedByReBuzz(int seq);

            void OnKeyDown(Object^ sender, KeyEventArgs^ args);
            void OnKeyUp(Object^ sender, KeyEventArgs^ args);

            void SendMessageToKeyboardWindow(UINT msg, WPARAM wparam, LPARAM lparam);

            
            
            RebuzzBuzzLookup<IWaveLayer, int, CWaveLevel> * m_waveLevelsMap;
            RebuzzBuzzLookup<ISequence, int, CSequence>* m_sequenceMap;
            
            MachineManager^ m_machineMgr;
            PatternManager^ m_patternMgr;

            RefClassWrapper<MachineWrapper> * m_thisref;
            MachineCallbackWrapper * m_callbackWrapper;
            CMachineInterface* m_machine;
            CMachine* m_thisCMachine;
            IBuzzMachineHost^ m_host;
            HWND m_hwndEditor;
            bool m_initialised;
            IBuzzMachine^ m_buzzmachine;
            CMasterInfo* m_masterInfo;
            void* m_mapCallbackData;
            IMachine^ m_rebuzzMachine;

           
            CPattern * m_patternEditorPattern;
            CMachine* m_patternEditorMachine;

           
            System::Action<int>^ m_seqAddedAction;
            System::Action<int>^ m_seqRemovedAction;
            UserControl^ m_control;

            KeyboardFocusWindowHandleCallback m_kbFocusWndcallback;
            void* m_kbFocusCallbackParam;
            KeyEventHandler^ m_onKeyDownHandler;
            KeyEventHandler^ m_onKeyupHandler;
        
        };
    }
}
