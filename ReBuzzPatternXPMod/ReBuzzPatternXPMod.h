#pragma once

using Buzz::MachineInterface::MachineDecl;
using Buzz::MachineInterface::IBuzzMachine;
using Buzz::MachineInterface::IBuzzMachineHost;
using Buzz::MachineInterface::ParameterDecl;


using BuzzGUI::Interfaces::IParameter;
using BuzzGUI::Interfaces::IPattern;
using BuzzGUI::Interfaces::IMachineDLL;
using BuzzGUI::Interfaces::BuzzCommand;
using BuzzGUI::Interfaces::IPatternEditorColumn;

using System::ComponentModel::INotifyPropertyChanged;
using System::ComponentModel::PropertyChangedEventHandler;
using System::String;
using System::Collections::Generic::IDictionary;
using System::Windows::Forms::UserControl;
using System::Collections::Generic::IEnumerable;

using ReBuzz::NativeMachineFramework::MachineWrapper;


#include "Buzz-PatternXP\MachineInterface.h"
#include <MachineCallbackWrapper.h>
#include "RefClassWrapper.h"


[MachineDecl(Name = "Jeskola Pattern XP mod", ShortName = "Pattern XP mod", Author = "WDE / chahur / MarCNeT", MaxTracks = 8, InputCount = 0, OutputCount = 0)]
public ref class ReBuzzPatternXpMachine : IBuzzMachine, INotifyPropertyChanged
{
public:

    //Constructor
    ReBuzzPatternXpMachine(IBuzzMachineHost^ host);

    //Destructor
    ~ReBuzzPatternXpMachine();

    void Work();

    void ImportFinished(IDictionary<String^, String^>^ machineNameMap);

    UserControl^ PatternEditorControl();

    void SetEditorPattern(IPattern^ pattern);

    void RecordControlChange(IParameter^ parameter, int track, int value);

    void SetTargetMachine(IMachine^ machine);

    String^ GetEditorMachine();

    void SetPatternEditorMachine(IMachineDLL^ editorMachine);

    int GetTicksPerBeatDelegate(IPattern^ pattern, int playPosition);
   

    void SetModifiedFlag();

    property Object^ MachineState
    {
        Object^ get()
        {
            return nullptr;
        }

        void set(Object^ val)
        {}
    }

    bool CanExecuteCommand(BuzzCommand cmd);

    void ExecuteCommand(BuzzCommand cmd);

    void MidiNote(int channel, int value, int velocity);

    void MidiControlChange(int ctrl, int channel, int value);

    cli::array<byte>^ GetPatternEditorData();

    void SetPatternEditorData(cli::array<byte>^ data);

    cli::array<int>^ GetPatternEditorMachineMIDIEvents(IPattern^ pattern);

    void SetPatternEditorMachineMIDIEvents(IPattern^ pattern, cli::array<int>^ data);

    //NOT SUPPORTED
    //IEnumerable<IPatternEditorColumn^>^ GetPatternEditorEvents(IPattern^ pattern, int tbegin, int tend);

    void Activate();

    void Release();

    void CreatePatternCopy(IPattern^ pnew, IPattern^ p);

    //======================= Property Changed Events ======================
    event PropertyChangedEventHandler^ PropertyChanged
    {
        virtual void add(PropertyChangedEventHandler^ value) sealed =
            INotifyPropertyChanged::PropertyChanged::add
        {
            this->PropertyChanged += value;
        }

        virtual void remove(PropertyChangedEventHandler^ value) sealed =
            INotifyPropertyChanged::PropertyChanged::remove
        {
            // Remove from the event defined in the C# class.
            this->PropertyChanged -= value;
        }
    };

    [ParameterDecl()]
    property bool Dummy 
    { 
        bool get()
        {
            return m_dummyParam;
        }
        
        void set(bool val)
        {
            m_dummyParam = val;
        }
     }

private:
    void Initialise();

    IBuzzMachineHost^ m_host;
    CMachineInterface* m_interface;
    bool m_dummyParam;
    MachineWrapper^ m_machineWrapper;
    bool m_initialised;
    void* m_callbackdata;
    ReBuzz::NativeMachineFramework::RefClassWrapper<UserControl> * m_patternEditor;
};