//For some calls, we need access to the raw mi class members.
//In order to do that, because mi is self contained within a single source file, we have to 
//include PatternXp.cpp
//This compiles PatternXp.cpp with our source file, allowing access to the public members of mi,
//without needing to change PatternXp.cpp
#include "PatternXPRE/patternxp/PatternXp.cpp"


#include <RefClassWrapper.h>
#include <NativeMachineReader.h>
#include <NativeMachineWriter.h>
#include <Utils.h>

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
                                            NULL,
                                            GetKeyboardFocusWindow, 
                                            RedrawEditorWindow);

    cbdata->machineWrapper.Assign(m_machineWrapper);
}

ReBuzzPatternXpMachine::~ReBuzzPatternXpMachine()
{   
    Release();

    delete m_interface;
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
    //Make sure native machine wrapper is initialised
    m_machineWrapper->Init();

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
    
    //This could be Modern Pattern Editor data, or it could be pattern XP data
    if (version == 255)
    {
        //Modern Pattern Editor data
        inputReader->Read(version);
        if (version != 1)
        {
            AfxMessageBox("Modern Pattern Editor data is unknown version ");
            return;
        }

        int dataSize;
        inputReader->Read(dataSize);

        int numpat;
        inputReader->Read(numpat);
        for (int i = 0; i < numpat; i++)
        {
            CString patname = inputReader->ReadString();
            shared_ptr<CMachinePattern> p(new CMachinePattern());
            p->name = patname;

            //Read pattern info
            int mpeBeats, rowsPerBeat, columnCount;
            inputReader->Read(mpeBeats);
            inputReader->Read(rowsPerBeat);
            inputReader->Read(columnCount);

            p->SetRowsPerBeat(rowsPerBeat);
            p->SetLength(mpeBeats * rowsPerBeat, pmi->pCB);

            p->columns.clear();
            for (int c = 0; c < columnCount; ++c)
            {
                std::shared_ptr<CColumn> newColumn = std::make_shared<CColumn>();

                //Modern pattern editor writes the event rows as :
                //  eventTime * PatternEvent.TimeBase * 4 / pat.RowsPerBeat;
                //
                // where PatternEvent.TimeBase = 240
                //       pat.RowsPerBeat = rowsPerBeat
                //
                //So we need to do the reverse to convert to PatternXP event times
                //
                //The members are not directly accessible to us, so we need to 
                //read the column data, perform the conversion, and write the converted data 
                //to a temporary stream, and get the PatternXP column to read from the converted 
                //data
                NativeMachineWriter tempConvertedColumnData;

                //Machine name
                CString machineName = inputReader->ReadString();
                tempConvertedColumnData.Write(machineName.GetBuffer(), machineName.GetLength());
                byte zero = 0;
                tempConvertedColumnData.Write(&zero, 1);

                //Param index and track
                int paramIndex,  paramTrack;
                inputReader->Read(paramIndex);
                inputReader->Read(paramTrack);
                tempConvertedColumnData.Write(&paramIndex, 4);
                tempConvertedColumnData.Write(&paramTrack, 4);

                //Graphical flag
                byte graphical;
                inputReader->Read(graphical);
                tempConvertedColumnData.Write(&graphical, 1);
                
                //Event count
                int eventCount;
                inputReader->Read(eventCount);
                tempConvertedColumnData.Write(&eventCount, 4);

                //Events
                for (int e = 0; e < eventCount; ++e)
                {
                    //Time and value
                    int time, value;
                    inputReader->Read(time);
                    inputReader->Read(value);

                    //Convert the time
                    time *= rowsPerBeat;
                    time /= 960;

                    tempConvertedColumnData.Write(&time, 4);
                    tempConvertedColumnData.Write(&value, 4);
                }

                //Read the converted column data
                NativeMachineReader tempConvertedColDataReader(tempConvertedColumnData.dataPtr(), tempConvertedColumnData.size());
                newColumn->Read(&tempConvertedColDataReader, 3);

                //Read 'beats' (no idea what to do with this)
                for (int b = 0; b < mpeBeats; ++b)
                {
                    int index;
                    inputReader->Read(index);
                }

                p->columns.push_back(newColumn);
            }

            //Cannot do this here - Modern Pattern Editor still has event hooks into ReBuzz
            //and has not yet been fully switched over to Pattern XP, so MPE will get triggered if we create a pattern here.
            /*IMachine^ thisMachine = m_host->Machine;
            p->pPattern = reinterpret_cast<CPattern*>(m_machineWrapper->GetCPatternByName(thisMachine, patname));
            if (p->pPattern == NULL)
            {
                //Create the pattern in ReBuzz
                p->pPattern = reinterpret_cast<CPattern *>(m_machineWrapper->CreatePattern(thisMachine, patname, mpeBeats * rowsPerBeat ));
            }
            */

            pmi->loadedPatterns[patname] = p;
        }
    }
    else if (version >= 1 && version <= PATTERNXP_DATA_VERSION)
    {
        //Pattern XP data
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
    else
    {
        //Not known
        AfxMessageBox("invalid data");
        return;
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
    if (m_patternEditor != NULL)
    {
        m_patternEditor->Free();
        delete m_patternEditor;
        m_patternEditor = NULL;
    }

    if (m_machineWrapper != nullptr)
    {
        m_machineWrapper->Release();
        delete m_machineWrapper;
        m_machineWrapper = nullptr;
    }

    if (m_callbackdata != NULL)
    {
        ReBuzzPatternXpCallbackData* callbackData = reinterpret_cast<ReBuzzPatternXpCallbackData*>(m_callbackdata);
        delete callbackData;
        m_callbackdata = NULL;
    }

    m_initialised = false;
}

void ReBuzzPatternXpMachine::CreatePatternCopy(IPattern^ pnew, IPattern^ p)
{
    m_machineWrapper->CreatePatternCopy(pnew, p);
}