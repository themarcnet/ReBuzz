//For some calls, we need access to the raw mi class members.
//In order to do that, because mi is self contained within a single source file, we have to 
//include PatternXp.cpp
//This compiles PatternXp.cpp with our source file, allowing access to the public members of mi,
//without needing to change PatternXp.cpp
#include "PatternXPRE/patternxp/PatternXp.cpp"


#include <RefClassWrapper.h>
#include <NativeMachineReader.h>

#include "ReBuzzPatternXP.h"
#include <memory>
#include "stdafx.h"


using namespace ReBuzz::NativeMachineFramework;

using BuzzGUI::Interfaces::PatternEvent;
using BuzzGUI::Interfaces::IParameterGroup;
using BuzzGUI::Common::Global;

using System::IntPtr;
using System::Collections::Generic::IEnumerable;
using System::Collections::Generic::List;

//Callback data
struct ReBuzzPatternXpCallbackData
{
    CMachineInterface* machineInterface;
    CMachineInterfaceEx* machineInterfaceEx;
    RefClassWrapper<MachineWrapper> machineWrapper;
    mi* machine;
};

//Callbacks

static void * GetKeyboardFocusWindow(void* param)
{
    ReBuzzPatternXpCallbackData* callbackData = reinterpret_cast<ReBuzzPatternXpCallbackData*>(param);

    mi* pmi = reinterpret_cast<mi*>(callbackData->machineInterface);
    return pmi->patEd->pe.GetSafeHwnd();
}

static void RedrawEditorWindow(void* param)
{
    ReBuzzPatternXpCallbackData* callbackData = reinterpret_cast<ReBuzzPatternXpCallbackData*>(param);

    mi* pmi = reinterpret_cast<mi*>(callbackData->machineInterface);
    pmi->patEd->RedrawWindow();
}

//=====================================================
ReBuzzPatternXpMachine::ReBuzzPatternXpMachine(IBuzzMachineHost^ host) : m_host(host),
                                                                         m_dummyParam(false),
                                                                         m_initialised(false),
                                                                         m_patternEditor(NULL)
{
    m_interface = CreateMachine();
    
    //Set up callback data
    ReBuzzPatternXpCallbackData* cbdata = new ReBuzzPatternXpCallbackData();
    cbdata->machineInterface = m_interface;
    
    mi* pmi = reinterpret_cast<mi*>(m_interface);
    cbdata->machineInterfaceEx = &pmi->ex;
    cbdata->machine = pmi;
    m_callbackdata = cbdata;

    //Create machine wrapper
    m_machineWrapper = gcnew MachineWrapper(m_interface, host, (IBuzzMachine^)this, cbdata, 
                                            GetKeyboardFocusWindow, 
                                            RedrawEditorWindow);

    cbdata->machineWrapper.Assign(m_machineWrapper);
}

ReBuzzPatternXpMachine::~ReBuzzPatternXpMachine()
{   
    delete m_machineWrapper;
    delete m_interface;
    
    ReBuzzPatternXpCallbackData* callbackData = reinterpret_cast<ReBuzzPatternXpCallbackData*>(m_callbackdata);
    delete callbackData;

    delete m_patternEditor;
}

void ReBuzzPatternXpMachine::Work()
{
    //Make sure we're initialised before working...
    if(m_initialised && (m_patternEditor != NULL) && (m_interface != NULL))
    {
        //Tick the machine / native buzz machine wrapper
        m_machineWrapper->Tick();

        //If we're currently playing, Make sure the machine is told to play a pattern
        if (Global::Buzz->Playing && (m_host->MasterInfo->PosInTick == 0))
        {   
            //Tell native wrapper to tell the pattern editor about the playing pattern
            m_machineWrapper->NotifyOfPlayingPattern();
        }

        //The parameters are not used by 'Work', so just put anything in....
        m_interface->Work(NULL, 0, 0);
    }
}

void ReBuzzPatternXpMachine::ImportFinished(IDictionary<String^, String^>^ machineNameMap)
{
    if (!m_initialised)
    {
        //Initialise the native machine wrapper
        m_machineWrapper->Init();
        m_initialised = true;
    }
}



UserControl^ ReBuzzPatternXpMachine::PatternEditorControl()
{
    //Make sure we're initialised
    if (!m_initialised)
    {
        m_machineWrapper->Init();
        m_initialised = true;
    }

    if (m_patternEditor != NULL)
    {
        return m_patternEditor->GetRef();
    }

    m_patternEditor = new RefClassWrapper<UserControl>(m_machineWrapper->PatternEditorControl());
    return m_patternEditor->GetRef();
}

void ReBuzzPatternXpMachine::SetEditorPattern(IPattern^ pattern)
{
    m_machineWrapper->SetEditorPattern(pattern);
}

void ReBuzzPatternXpMachine::RecordControlChange(IParameter^ parameter, int track, int value)
{
    m_machineWrapper->RecordControlChange(parameter, track, value);
}

void ReBuzzPatternXpMachine::SetTargetMachine(IMachine^ machine)
{
    m_machineWrapper->SetTargetMachine(machine);
}

String^ ReBuzzPatternXpMachine::GetEditorMachine()
{
    return gcnew String("PatternXP");
}

void ReBuzzPatternXpMachine::SetPatternEditorMachine(IMachineDLL^ editorMachine)
{
    //No idea what to do here
}

void ReBuzzPatternXpMachine::SetPatternName(String^ machine, String^ oldName, String^ newName)
{
    return m_machineWrapper->SetPatternName(machine, oldName, newName);
}

int ReBuzzPatternXpMachine::GetTicksPerBeatDelegate(IPattern^ pattern, int playPosition)
{
    //Get CPattern
    CPattern* cpat = (CPattern *)m_machineWrapper->GetCPattern(pattern);
    if (cpat == NULL)
        return BUZZ_TICKS_PER_BEAT;

    //We don't have direct access to Pattern XP's data, and there is no way to query 
    //the 'rowsPerBeat' val evia the existing CMachineInterface * or CMachineInterfaceEx * 
    //interfaces.
    //So this is why PatternXp.cpp is included at the top of this source file - 
    //it allows access to the public members of the mi class, where the CPattern * to
    //PatternXP patter map is located.  From that map, we can then turn a CPattern * value
    //to a PatternXP pattern and query the 'rowsPerBeat' value.
    const mi* pmi = reinterpret_cast<const mi*>(m_interface);
    const auto& foundPattern = pmi->patterns.find(cpat);
    if (foundPattern == pmi->patterns.end())
        return BUZZ_TICKS_PER_BEAT;

    return (*foundPattern).second->rowsPerBeat;
}


void ReBuzzPatternXpMachine::SetModifiedFlag()
{
    m_machineWrapper->SetModifiedFlag();
}

bool ReBuzzPatternXpMachine::CanExecuteCommand(BuzzCommand cmd) 
{
    return m_machineWrapper->CanExecuteCommand(cmd);
}

void ReBuzzPatternXpMachine::ExecuteCommand(BuzzCommand cmd)
{
    return m_machineWrapper->ExecuteCommand(cmd);
}

void ReBuzzPatternXpMachine::MidiNote(int channel, int value, int velocity)
{
    m_machineWrapper->MidiNote(channel, value, velocity);
}

void ReBuzzPatternXpMachine::MidiControlChange(int ctrl, int channel, int value)
{
    m_machineWrapper->MidiControlChange(ctrl, channel, value);
}

cli::array<byte>^ ReBuzzPatternXpMachine::GetPatternEditorData()
{
    return m_machineWrapper->Save();
}

void ReBuzzPatternXpMachine::SetPatternEditorData(cli::array<byte>^ data)
{
    //Native buzz machines don't directly support 'loading' (only via Init - but that does other stuff)
    //So we'll copy the load implentation from PatterXp here
    mi* pmi = reinterpret_cast<mi*>(m_interface);
    pmi->loadedPatterns.clear();
    
    if ((data == nullptr) || (data->Length == 0))
        return;

    pin_ptr<byte> dataptr = &data[0];
    ReBuzz::NativeMachineFramework::NativeMachineReader input(dataptr, data->Length);
    CMachineDataInput* inputReader = &input;

    byte version;
    inputReader->Read(version);
    if (version < 1 || version > PATTERNXP_DATA_VERSION)
    {
        AfxMessageBox("invalid data");
        return;
    }

    int numpat;
    inputReader->Read(numpat);

    for (int i = 0; i < numpat; i++)
    {
        CString name = inputReader->ReadString();
        shared_ptr<CMachinePattern> p(new CMachinePattern());
        p->Read(inputReader, version);
        pmi->loadedPatterns[name] = p;
    }
}

cli::array<int>^ ReBuzzPatternXpMachine::GetPatternEditorMachineMIDIEvents(IPattern^ pattern)
{
    return m_machineWrapper->GetPatternEditorMachineMIDIEvents(pattern);
}

void ReBuzzPatternXpMachine::SetPatternEditorMachineMIDIEvents(IPattern^ pattern, cli::array<int>^ data)
{
    m_machineWrapper->SetPatternEditorMachineMIDIEvents(pattern, data);
}

void ReBuzzPatternXpMachine::Activate()
{
    m_machineWrapper->Activate();
}

void ReBuzzPatternXpMachine::Release()
{
    m_machineWrapper->Release();
    m_initialised = false;
}

void ReBuzzPatternXpMachine::CreatePatternCopy(IPattern^ pnew, IPattern^ p)
{
    m_machineWrapper->CreatePatternCopy(pnew, p);
}