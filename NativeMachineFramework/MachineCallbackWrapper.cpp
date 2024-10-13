#include "MachineCallbackWrapper.h"

//<Windows.h> must be after MachineCallbackWrapper.h, to avoide <Windows.h> redefining some of the 
//callback methods by suffixing a W
#include <Windows.h>

#include "MachineEventWrapper.h"
#include "MachineWrapper.h"
#include "Utils.h"

using BuzzGUI::Common::Global;
using BuzzGUI::Interfaces::IIndex;

using Buzz::MachineInterface::SubTickInfo;


namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        MachineCallbackWrapper::MachineCallbackWrapper(MachineWrapper^ mw, 
                                                        IBuzzMachine^  netmach,
                                                        IBuzzMachineHost^ host,
                                                        CMachineInterface* iface, 
                                                        CMachine * machine,
                                                        CMasterInfo * masterinfo) :  
                                                                                m_netmcahine(netmach),
                                                                                m_machinehost(host),
                                                                                m_machineWrapper(mw),
                                                                                m_thisMachine(machine),
                                                                                m_exInterface(NULL),
                                                                                m_interface(iface),
                                                                                m_masterinfo(masterinfo),
                                                                                m_setPatternEditorPattern(NULL),
                                                                                m_setPatternEditorMachine(NULL)
        {
            //Update master info
            m_machineWrapper.GetRef()->UpdateMasterInfo();
        }

        MachineCallbackWrapper::~MachineCallbackWrapper()
        {
            if (!m_addedMachineEventHandler.isNull())
            {
                MachineEventWrapper^ evtWrapper = m_addedMachineEventHandler.GetRef();
                Global::Buzz->Song->MachineAdded -= evtWrapper->GetAction();
                delete evtWrapper;
                m_addedMachineEventHandler.Free();
            }

            if (!m_deleteMachineEventHandler.isNull())
            {
                MachineEventWrapper^ evtWrapper = m_deleteMachineEventHandler.GetRef();
                Global::Buzz->Song->MachineRemoved -= evtWrapper->GetAction();
                delete evtWrapper;
                m_deleteMachineEventHandler.Free();
            }

            m_netmcahine.Free();
        }

        CMachineInterfaceEx* MachineCallbackWrapper::GetExInterface() const
        {
            return m_exInterface;
        }

        CMachine* MachineCallbackWrapper::GetThisMachine()
        {
            return m_thisMachine;
        }

        void MachineCallbackWrapper::SetMachineInterfaceEx(CMachineInterfaceEx* pex)
        {
            m_exInterface = pex;

            //Anything that we need to do that was delayed becuase the exInterface was not set in time?
            if ((m_setPatternEditorMachine != NULL) && (m_setPatternEditorPattern != NULL))
            {
                pex->SetEditorPattern(m_setPatternEditorPattern);
                pex->SetPatternTargetMachine(m_setPatternEditorPattern, m_setPatternEditorMachine);
            }

            m_setPatternEditorMachine = NULL;
            m_setPatternEditorPattern = NULL;
        }

       
        void MachineCallbackWrapper::SetEventHandler(CMachine* pmac, BEventType et, EVENT_HANDLER_PTR p, void* param)
        {
            //Make sure this is for us
            if (pmac != (CMachine*)m_thisMachine)
                return;


            switch (et)
            {
                case gAddMachine:
                    //Ask ReBuzz to call us back if a machine is added to the song
                    if (m_addedMachineEventHandler.isNull())
                    {
                        m_addedMachineEventHandler = gcnew MachineEventWrapper(m_interface);

                        //Tell ReBuzz to call the event handler on event, which will call all the registered callbacks
                        MachineEventWrapper^ eventHandler = m_addedMachineEventHandler.GetRef();
                        System::Action<IMachine^>^ action = gcnew System::Action<IMachine^>(eventHandler, &MachineEventWrapper::OnEvent);
                        eventHandler->SetAction(action);
                        Global::Buzz->Song->MachineAdded += action;
                    }

                    //Add callback and param to event handler
                    m_addedMachineEventHandler.GetRef()->AddEvent(p, param);
                    break;
                case gDeleteMachine:
                    //Ask ReBuzz to call us back if a machine is added to the song
                    if (m_deleteMachineEventHandler.isNull())
                    {
                        m_deleteMachineEventHandler = gcnew MachineEventWrapper(m_interface);

                        //Tell ReBuzz to call our method on event, which will call all the registered callbacks
                        MachineEventWrapper^ eventHandler = m_deleteMachineEventHandler.GetRef();
                        System::Action<IMachine^>^ action = gcnew System::Action<IMachine^>(eventHandler, &MachineEventWrapper::OnEvent);
                        eventHandler->SetAction(action);
                        Global::Buzz->Song->MachineRemoved += action;
                    }

                    //Add callback and param to event handler
                    m_deleteMachineEventHandler.GetRef()->AddEvent(p, param);
                    break;

                case gUndeleteMachine:
                    //TODO
                    break;
                case gWaveChanged:
                    //TODO
                    break;
                case gRenameMachine:
                    //TODO
                    break;
            }
        }

        void MachineCallbackWrapper::SetModifiedFlag()
        {
            Global::Buzz->SetModifiedFlag();
        }

        CSubTickInfo const* MachineCallbackWrapper::GetSubTickInfo()
        {
            //Update master info
            m_machineWrapper.GetRef()->UpdateMasterInfo();

            //Get subtick info from host
            SubTickInfo^ subtickInfo = m_machinehost.GetRef()->SubTickInfo;

            //Translate into native speak, stored in our class
            m_subtickInfo.CurrentSubTick = subtickInfo->CurrentSubTick;
            m_subtickInfo.PosInSubTick = subtickInfo->PosInSubTick;
            m_subtickInfo.SamplesPerSubTick = subtickInfo->SamplesPerSubTick;
            m_subtickInfo.SubTicksPerTick = subtickInfo->SubTicksPerTick;
            
            //return the pointer inside our class
            return &m_subtickInfo;
        }

        CMachineInfo const* MachineCallbackWrapper::GetMachineInfo(CMachine* pmac)
        {  
            //Get the emulation data. This contains the buzz machine info in native form
            CMachineData* machdata = m_machineWrapper.GetRef()->GetBuzzMachineData(pmac);
            if (machdata == NULL)
                return NULL;
            
            //Info is already 
            return &machdata->m_info;
        }

        char const* MachineCallbackWrapper::GetMachineName(CMachine* pmac)
        {
            //Get the emulation data. This contains the buzz machine info in native form
            CMachineData* machdata = m_machineWrapper.GetRef()->GetBuzzMachineData(pmac);
            if (machdata == NULL)
                return NULL;

            return machdata->name.c_str();
        }

        CMachine* MachineCallbackWrapper::GetMachine(char const* name)
        {
            return m_machineWrapper.GetRef()->GetCMachineByName(name);
        }

        dword MachineCallbackWrapper::GetThemeColor(char const* name)
        {
            //Convert name
            String^ clrname = Utils::stdStringToCLRString(name);

            //Get colour
            System::Drawing::Color^ colour =  Global::Buzz->GetThemeColour(clrname);

            //Convert the colour
            // Native machines (well, PatternXP) set alpha to 0xFF, which won't work since it is transparent.
            // So set alpha to be always zero here.
            dword ret = 0; //Alpha
            ret = (ret << 8) | ((colour->B) & 0xFF);
            ret = (ret << 8) | ((colour->G) & 0xFF);
            ret = (ret << 8) | ((colour->R) & 0xFF);
            return ret;
        }


        int MachineCallbackWrapper::GetNumTracks(CMachine* pmac) 
        {
            //Get the machine
            IMachine^ rebuzzMach =  m_machineWrapper.GetRef()->GetReBuzzMachine(pmac);
            if (rebuzzMach == nullptr)
                return 0;

            //Ask the machine for the number of tracks
            return rebuzzMach->TrackCount;
        }

        char const* MachineCallbackWrapper::GetPatternName(CPattern* ppat)
        {
            //Get the pattern data
            CPatternData* patdata = m_machineWrapper.GetRef()->GetBuzzPatternData(ppat);
            if (patdata == NULL)
                return NULL;

            return patdata->name.c_str();
        }

        int MachineCallbackWrapper::GetPatternLength(CPattern* p)
        {
            //Get the rebuzz pattern
            IPattern^ pat = m_machineWrapper.GetRef()->GetReBuzzPattern(p);
            if (pat == nullptr)
                return 0;

            return pat->Length;
        }

        void MachineCallbackWrapper::SetPatternEditorStatusText(int pane, char const* text)
        {
            switch (pane)
            {
                case 0:
                    m_statusBarText0 = text;
                    break;
                case 1:
                    m_statusBarText1 = text;
                    break;
            }
            
            std::string outputText = "{statusbar}"; // write to debug console. prefix with "{statusbar}" to also set the status bar text.
            outputText.append(m_statusBarText0); 

            if (!m_statusBarText1.empty())
            {
                outputText.append(" | ");
                outputText.append(m_statusBarText1);
            }

            //Convert the CLR string
            String^ clrText = Utils::stdStringToCLRString(outputText);
            Global::Buzz->DCWriteLine(clrText);
            //Global.Buzz.DCWriteLine("{statusbar}" + e.ToString());
        }
    }
}
