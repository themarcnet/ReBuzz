#include <Windows.h>

#include "Buzz\MachineInterface.h"

#include "MachineWrapper.h"
#include "NativeMFCMachineControl.h"
#include "Utils.h"
#include "NativeMachineWriter.h"
#include "NativeMachineReader.h"

using BuzzGUI::Common::Global;

using BuzzGUI::Interfaces::IParameterGroup;
using BuzzGUI::Interfaces::IAttribute;
using BuzzGUI::Interfaces::IMenuItem;
using BuzzGUI::Interfaces::MachineType;
using BuzzGUI::Interfaces::MachineInfoFlags;
using BuzzGUI::Interfaces::ParameterType;
using BuzzGUI::Interfaces::ParameterFlags;

namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        struct MachineCreateCallbackData
        {
            RefClassWrapper< MachineWrapper> machineWrapper;
            std::map<std::string, uint64_t> machineNameMap;
            OnNewPatternCallback onNewPatternCallback;
            void* callbackParam;
        };

        //This callback is called when a new CMachine * map entry is created.
        //The purpose of this is to populate the machine info, so that the 
        static void CreateMachineCallback(void* mach, void* param)
        {
            MachineCreateCallbackData* machCallbackData = reinterpret_cast<MachineCreateCallbackData*>(param);

            //Use machine wrapper to convert from CMachine * to ReBuzz machine
            CMachine* buzzMach = reinterpret_cast<CMachine*>(mach);
            IMachine^ rebuzzMach = machCallbackData->machineWrapper.GetRef()->GetReBuzzMachine(buzzMach);
            if (rebuzzMach == nullptr)
                return;

            //Get the emulation type, as this contains info about the machine
            CMachineData* machdata = machCallbackData->machineWrapper.GetRef()->GetBuzzMachineData(buzzMach);
            if (machdata == NULL)
                return;

            //Use ReBuzz to get info about the machine, and populate the CMachineInfo
            Utils::CLRStringToStdString(rebuzzMach->DLL->Info->Author, machdata->author);
            machdata->m_info.Author = machdata->author.c_str();
            Utils::CLRStringToStdString(rebuzzMach->DLL->Info->Name, machdata->name);
            machdata->m_info.Name = machdata->name.c_str();
            Utils::CLRStringToStdString(rebuzzMach->DLL->Info->ShortName, machdata->shortname);
            machdata->m_info.ShortName = machdata->shortname.c_str();

            machdata->m_info.Version = MI_VERSION;
            machdata->m_info.minTracks = rebuzzMach->DLL->Info->MinTracks;
            machdata->m_info.maxTracks = rebuzzMach->DLL->Info->MaxTracks;
            switch (rebuzzMach->DLL->Info->Type)
            {
                case MachineType::Master:
                    machdata->m_info.Type = MT_MASTER;
                    break;
                case MachineType::Effect:
                    machdata->m_info.Type = MT_EFFECT;
                    break;
                case MachineType::Generator:
                    machdata->m_info.Type = MT_GENERATOR;
                    break;
            }
           
            //Flags
            machdata->m_info.Flags = 0;
            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::ALWAYS_SHOW_PLUGS) == MachineInfoFlags::ALWAYS_SHOW_PLUGS)
                machdata->m_info.Flags |= MIF_ALWAYS_SHOW_PLUGS;
            
            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::MONO_TO_STEREO) == MachineInfoFlags::MONO_TO_STEREO)
                machdata->m_info.Flags |= MIF_MONO_TO_STEREO;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::PLAYS_WAVES) == MachineInfoFlags::PLAYS_WAVES)
                machdata->m_info.Flags |= MIF_PLAYS_WAVES;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::USES_LIB_INTERFACE) == MachineInfoFlags::USES_LIB_INTERFACE)
                machdata->m_info.Flags |= MIF_USES_LIB_INTERFACE;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::USES_INSTRUMENTS) == MachineInfoFlags::USES_INSTRUMENTS)
                machdata->m_info.Flags |= MIF_USES_INSTRUMENTS;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::DOES_INPUT_MIXING) == MachineInfoFlags::DOES_INPUT_MIXING)
                machdata->m_info.Flags |= MIF_DOES_INPUT_MIXING;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::NO_OUTPUT) == MachineInfoFlags::NO_OUTPUT)
                machdata->m_info.Flags |= MIF_NO_OUTPUT;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::CONTROL_MACHINE) == MachineInfoFlags::CONTROL_MACHINE)
                machdata->m_info.Flags |= MIF_CONTROL_MACHINE;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::INTERNAL_AUX) == MachineInfoFlags::INTERNAL_AUX)
                machdata->m_info.Flags |= MIF_INTERNAL_AUX;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::EXTENDED_MENUS) == MachineInfoFlags::EXTENDED_MENUS)
                machdata->m_info.Flags |= MIF_EXTENDED_MENUS;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::PATTERN_EDITOR) == MachineInfoFlags::PATTERN_EDITOR)
                machdata->m_info.Flags |= MIF_PATTERN_EDITOR;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::PE_NO_CLIENT_EDGE) == MachineInfoFlags::PE_NO_CLIENT_EDGE)
                machdata->m_info.Flags |= MIF_PE_NO_CLIENT_EDGE;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::GROOVE_CONTROL) == MachineInfoFlags::GROOVE_CONTROL)
                machdata->m_info.Flags |= MIF_GROOVE_CONTROL;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::DRAW_PATTERN_BOX) == MachineInfoFlags::DRAW_PATTERN_BOX)
                machdata->m_info.Flags |= MIF_DRAW_PATTERN_BOX;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::STEREO_EFFECT) == MachineInfoFlags::STEREO_EFFECT)
                machdata->m_info.Flags |= MIF_STEREO_EFFECT;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::MULTI_IO) == MachineInfoFlags::MULTI_IO)
                machdata->m_info.Flags |= MIF_MULTI_IO;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::PREFER_MIDI_NOTES) == MachineInfoFlags::PREFER_MIDI_NOTES)
                machdata->m_info.Flags |= MIF_PREFER_MIDI_NOTES;

            if ((rebuzzMach->DLL->Info->Flags & MachineInfoFlags::LOAD_DATA_RUNTIME) == MachineInfoFlags::LOAD_DATA_RUNTIME)
                machdata->m_info.Flags |= MIF_LOAD_DATA_RUNTIME;

            //Get and convert attributes
            machdata->m_info.numAttributes = 0;
            for each (IAttribute^ attr in rebuzzMach->Attributes)
            {
                std::shared_ptr<CMachineAttribute> buzzAttr = std::make_shared<CMachineAttribute>();
                buzzAttr->DefValue = attr->DefValue;
                buzzAttr->MaxValue = attr->MaxValue;
                buzzAttr->MinValue = attr->MinValue;
                
                std::shared_ptr<std::string> buzzAttrName = std::make_shared<std::string>();
                Utils::CLRStringToStdString(attr->Name, *buzzAttrName);
                buzzAttr->Name = buzzAttrName->c_str();

                //Store attribute
                machdata->attributes.push_back(buzzAttr);
                machdata->attributePointers.push_back(buzzAttr.get());
                machdata->attributeNames.push_back(buzzAttrName);

                //Increase attribute count
                machdata->m_info.numAttributes += 1;
            }

            if (machdata->attributePointers.empty())
                machdata->m_info.Attributes = NULL;
            else
                machdata->m_info.Attributes = machdata->attributePointers.data();
            
            //Get and convert commands
            for each (IMenuItem ^ cmd in rebuzzMach->Commands)
            {
                //Native Buzz stored commands in a char * array, separated with a \n
                std::string text;
                Utils::CLRStringToStdString(cmd->Text, text);

                if (!machdata->commands.empty())
                    machdata->commands.append("\n");

                machdata->commands.append(text);
            }

            if (machdata->commands.empty())
                machdata->m_info.Commands = NULL;
            else
                machdata->m_info.Commands = machdata->commands.c_str();
            
            
            //Okay, now do parameters
            //From what I can gather - Parameter group 1 are global parameters
            //and Parameter group 2 are track parameters
            //I've no idea what parameter group 0 is for.
            //What could have been useful here , is a 'type' enum on each group, so I 
            //don't have to hard-code group numbers here, and can just go by the 'Type' enum value.
            machdata->m_info.numGlobalParameters = 0;
            machdata->m_info.numTrackParameters = 0;
            int grpNum = 0;
            for each (IParameterGroup ^ grp in rebuzzMach->ParameterGroups)
            {
                if((grpNum != 1) && (grpNum != 2))
                {
                    ++grpNum;
                    continue;
                }

                for each (IParameter ^ param in grp->Parameters)
                {
                    std::shared_ptr<CMachineParameter> buzzParam = std::make_shared<CMachineParameter>();
                    buzzParam->DefValue = param->DefValue;
                    buzzParam->MaxValue = param->MaxValue;
                    buzzParam->MinValue = param->MinValue;
                    buzzParam->NoValue = param->NoValue;
                    switch (param->Type)
                    {
                        case ParameterType::Note:
                            buzzParam->Type = pt_note;
                            break;
                        case ParameterType::Byte:
                            buzzParam->Type = pt_byte;
                            break;
                        case ParameterType::Internal:
                            buzzParam->Type = pt_internal;
                            break;
                        case ParameterType::Switch:
                            buzzParam->Type = pt_switch;
                            break;
                        case ParameterType::Word:
                            buzzParam->Type = pt_word;
                            break;
                    }

                    buzzParam->Flags = 0;
                    if ((param->Flags & ParameterFlags::Ascii) == ParameterFlags::Ascii)
                        buzzParam->Flags |= MPF_ASCII;

                    if ((param->Flags & ParameterFlags::State) == ParameterFlags::State)
                        buzzParam->Flags |= MPF_STATE;

                    if ((param->Flags & ParameterFlags::TickOnEdit) == ParameterFlags::TickOnEdit)
                        buzzParam->Flags |= MPF_TICK_ON_EDIT;

                    if ((param->Flags & ParameterFlags::TiedToNext) == ParameterFlags::TiedToNext)
                        buzzParam->Flags |= MPF_TIE_TO_NEXT;

                    if ((param->Flags & ParameterFlags::Wave) == ParameterFlags::Wave)
                        buzzParam->Flags |= MPF_WAVE;

                    std::shared_ptr<std::string> desc = std::make_shared<std::string>();
                    Utils::CLRStringToStdString(param->Description, *desc);
                    buzzParam->Description = desc->c_str();

                    std::shared_ptr<std::string> name = std::make_shared<std::string>();
                    Utils::CLRStringToStdString(param->Name, *name);
                    buzzParam->Name = name->c_str();


                    machdata->parameters.push_back(buzzParam);
                    machdata->parameterPtrs.push_back(buzzParam.get());
                    machdata->paramDescriptions.push_back(desc);
                    machdata->paramDescriptions.push_back(name);

                    if (grpNum == 1)
                        machdata->m_info.numGlobalParameters += 1;
                    else if(grpNum == 2)
                        machdata->m_info.numTrackParameters += 1;
                }

                ++grpNum;
            }

            if (machdata->parameterPtrs.empty())
                machdata->m_info.Parameters = NULL;
            else
                machdata->m_info.Parameters = machdata->parameterPtrs.data();

            //Store the internal id against the machine name
            uint64_t id = rebuzzMach->GetHashCode();
            machCallbackData->machineNameMap[machdata->name] = id;
        }

        static void CreatePatternCallback(void* pat, void* param)
        {
            MachineCreateCallbackData* machCallbackData = reinterpret_cast<MachineCreateCallbackData*>(param);

            //Get the emulation type, as this contains info about the pattern
            CPattern* buzzPat = reinterpret_cast<CPattern*>(pat);
            CPatternData* patdata = machCallbackData->machineWrapper.GetRef()->GetBuzzPatternData(buzzPat);
            if (patdata == NULL)
                return;

            //Get the ReBuzz pattern
            IPattern^ rebuzzPattern = machCallbackData->machineWrapper.GetRef()->GetReBuzzPattern(buzzPat);

            //Get the patten name, and store into the pattern data
            Utils::CLRStringToStdString(rebuzzPattern->Name, patdata->name);

            //Call the callback that was set on construction
            machCallbackData->onNewPatternCallback(buzzPat, machCallbackData->callbackParam);
        }

        void MachineWrapper::OnMachineCreatedByReBuzz(IMachine^ machine)
        {
            //Do we have this machine in our map?
            uint64_t id = machine->GetHashCode();
            CMachine* pmach = m_machineMap->GetBuzzTypeById(id);
            if (pmach == NULL)
            {
                //Create machine. This will also trigger the above 'CreateMachineCallback'
                m_machineMap->GetOrStoreReBuzzTypeById(id, machine);
            }
        }

        void MachineWrapper::OnMachineRemovedByReBuzz(IMachine^ machine)
        {
        }

        MachineWrapper::MachineWrapper(void * machine,
                                       IBuzzMachineHost^ host,
                                       IBuzzMachine^ buzzmachine,
                                        void* callbackparam,
                                        OnNewPatternCallback onNewPatternCallback) : 
                                                                     m_machine((CMachineInterface *)machine),
                                                                     m_host(host),
                                                                     m_hwndEditor(NULL),
                                                                     m_initialised(false),
                                                                     m_buzzmachine(buzzmachine),
                                                                     m_onNewPatternCallback(onNewPatternCallback),
                                                                     m_callbackParam(callbackparam),
                                                                     m_patternEditorPattern(NULL),
                                                                     m_patternEditorMachine(NULL),
                                                                     m_control(nullptr)
        {
            //Create callback data for our machine creation callback
            MachineCreateCallbackData* machCallbackData = new MachineCreateCallbackData();
            m_machineCallbackData = machCallbackData;
            machCallbackData->machineWrapper.Assign(this);
            machCallbackData->onNewPatternCallback = onNewPatternCallback;
            machCallbackData->callbackParam = callbackparam;

            m_patternMap = new RebuzzBuzzLookup< IPattern, CPatternData, CPattern>(CreatePatternCallback, m_machineCallbackData);
            m_machineMap = new RebuzzBuzzLookup< IMachine, CMachineData, CMachine>(CreateMachineCallback, m_machineCallbackData);

            //Create ref wrapper around this for window callbacks
            m_thisref = new RefClassWrapper<MachineWrapper>(this);

            //Register add this machine to the machine map
            if (host->Machine != nullptr)
            {
                Init();
            }

            //Allocate some master info
            m_masterInfo = new CMasterInfo();

            //Ask ReBuzz to tell us when a machine has been added
            m_machineAddedAction = gcnew System::Action<IMachine^>(this, &MachineWrapper::OnMachineCreatedByReBuzz);
            Global::Buzz->Song->MachineAdded += m_machineAddedAction;

            //Ask ReBuzz to tell us when a machine has been deleted
            m_machineRemovedAction = gcnew System::Action<IMachine^>(this, &MachineWrapper::OnMachineRemovedByReBuzz);
            Global::Buzz->Song->MachineRemoved += m_machineRemovedAction;
        }

        MachineWrapper::~MachineWrapper()
        {
            //Unregister events
            Global::Buzz->Song->MachineAdded -= m_machineAddedAction;
            Global::Buzz->Song->MachineRemoved -= m_machineRemovedAction;
            delete m_machineAddedAction;
            delete m_machineRemovedAction;

            m_machine->pCB = NULL; //Callbacks no longer available...
            delete m_callback;
            delete m_patternMap;
            delete m_machineMap;
            delete m_thisref;
            delete m_masterInfo;
            
            MachineCreateCallbackData* machCallbackData = reinterpret_cast<MachineCreateCallbackData*>(m_machineCallbackData);
            delete machCallbackData;
        }

        void MachineWrapper::Init()
        {
            if (!m_initialised && (m_host->Machine != nullptr))
            {
                uint64_t id = m_host->Machine->GetHashCode();
                CMachine* m = m_machineMap->GetOrStoreReBuzzTypeById(id, m_host->Machine);

                //populate master info
                m_machine->pMasterInfo = m_masterInfo;
                
                //Create callback wrapper class
                m_callback = new MachineCallbackWrapper(this, m_buzzmachine, m_host, m_machine, m, m_masterInfo);

                //Set the callback instance on the machine interface 
                m_machine->pCB = (CMICallbacks*)m_callback;

                //Collect the patterns
                for each (IPattern ^ p in m_host->Machine->Patterns)
                {
                    m_patternMap->GetOrStoreReBuzzTypeById(p->GetHashCode(), p);
                }

                //Finally init the actual machine
                m_machine->Init(NULL);

                m_initialised = true;
            }
        }

        CMachineInterfaceEx* MachineWrapper::GetExInterface()
        {
            return m_callback->GetExInterface();
        }

        void MachineWrapper::SetEditorPattern(IPattern^ pattern)
        {
            //Make sure we're initialised
            Init();

            //Store pattern ref (if not already stored)
            uint64_t patid = pattern->GetHashCode();
            CPattern * pat = m_patternMap->GetOrStoreReBuzzTypeById(patid, pattern);

            uint64_t machid = pattern->Machine->GetHashCode();
            CMachine* patMach = m_machineMap->GetOrStoreReBuzzTypeById(machid, pattern->Machine);

            //Get ex interface
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();
            if (exInterface != NULL)
            {
                //Tell pattern editor, if the pattern editor is active
                if (m_hwndEditor != NULL)
                {
                    exInterface->SetPatternTargetMachine(pat, patMach);
                    exInterface->SetEditorPattern(pat);
                }
                else
                {
                    //Store the pattern and machine for later, when the editor is created
                    m_patternEditorMachine = patMach;
                    m_patternEditorPattern = pat;
                }
            }
            else
            {
                //Tell callback to call the mathods when the exInterface has been set
                m_callback->SetDelayedEditorPattern(patMach, pat);
            }
        }

        IntPtr MachineWrapper::RebuzzWindowAttachCallback(IntPtr hwnd, void* callbackParam)
        {
            //Get machine wrapper
            RefClassWrapper<MachineWrapper>* classRef = reinterpret_cast<RefClassWrapper<MachineWrapper> *>(callbackParam);

            //Get ex interface
            CMachineInterfaceEx* exInterface = (CMachineInterfaceEx*)classRef->GetRef()->GetExInterface();
            
            //Tell PatterXP to create the pattern editor using the .NET user control as its parent
            void* patternEditorHwnd = exInterface->CreatePatternEditor(hwnd.ToPointer());

            //Store the HWND in the class for sending window messages
            classRef->GetRef()->m_hwndEditor = (HWND)patternEditorHwnd;

            //If we have a pattern to set, then do that now
            //(it was deferred from earlier)
            if( (classRef->GetRef()->m_patternEditorMachine != NULL) && 
                (classRef->GetRef()->m_patternEditorPattern != NULL))
            {
                exInterface->SetPatternTargetMachine(classRef->GetRef()->m_patternEditorPattern, 
                                                     classRef->GetRef()->m_patternEditorMachine);
                exInterface->SetEditorPattern(classRef->GetRef()->m_patternEditorPattern);

                classRef->GetRef()->m_patternEditorPattern = NULL;
                classRef->GetRef()->m_patternEditorMachine = NULL;
            }

            return IntPtr(patternEditorHwnd);
        }

        void MachineWrapper::RebuzzWindowDettachCallback(IntPtr patternEditorHwnd, void* callbackParam)
        {
            if (patternEditorHwnd != IntPtr::Zero)
            {
                //Get machine wrapper
                RefClassWrapper<MachineWrapper>* classRef = reinterpret_cast<RefClassWrapper<MachineWrapper> *>(callbackParam);

                //Destroy the pattern editor window
                DestroyWindow((HWND)patternEditorHwnd.ToPointer());
                classRef->GetRef()->m_hwndEditor = NULL;
            }
        }


        void MachineWrapper::RebuzzWindowSizeCallback(IntPtr patternEditorHwnd, void* callbackParam, int left, int top, int width, int height)
        {
            if (patternEditorHwnd != IntPtr::Zero)
            {
                SetWindowPos((HWND)patternEditorHwnd.ToPointer(), NULL, 0, 0, width, height, SWP_NOZORDER);
                InvalidateRect((HWND)patternEditorHwnd.ToPointer(), NULL, TRUE);
            }
        }


        UserControl^ MachineWrapper::PatternEditorControl()
        {
            if (m_control == nullptr)
            {
                //Create MFC wrapper
                AttachCallback^ onAttach = gcnew AttachCallback(RebuzzWindowAttachCallback);
                DetatchCallback^ onDetatch = gcnew DetatchCallback(RebuzzWindowDettachCallback);
                SizeChangedCallback^ onSzChanged = gcnew SizeChangedCallback(RebuzzWindowSizeCallback);
                m_control = gcnew NativeMFCMachineControl(onAttach, onDetatch, onSzChanged, m_thisref);
            }

            return m_control;
        }

        static int FindParameterGroupAndParam(IMachine^ mach, IParameter^ param, int * retParamNum)
        {
            int group = 0;
            for each (IParameterGroup ^ g in mach->ParameterGroups)
            {
                *retParamNum = g->Parameters->IndexOf(param);
                if (*retParamNum >= 0)
                {
                    return group;
                }

                ++group;
            }

            return -1;
        }

        void MachineWrapper::RecordControlChange(IParameter^ parameter, int track, int value)
        {
            //Get Ex Interface for calling the machine
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();

            //Get our CMachine * 
            CMachine* mach = m_machineMap->GetBuzzTypeById(m_thisMachineId);

            //Find the parameter group and parameter number values
            int paramNum = -1;
            int groupNum = FindParameterGroupAndParam(m_host->Machine, parameter, &paramNum);
            if (groupNum >= 0)
            {
                //Call the machine
                exInterface->RecordControlChange(mach, groupNum, track, paramNum, value);
            }
        }

        void MachineWrapper::SetTargetMachine(IMachine^ machine)
        {
            //Get Ex Interface for calling the machine
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();

            //MC: I'm guessing here that setting the target machine ALSO sets
            //    the pattern as well
            // As we don't have the pattern, just pick the first

            //Get machine
            uint64_t machid = machine->GetHashCode();
            CMachine* mach = m_machineMap->GetOrStoreReBuzzTypeById(machid, machine);

            //Get first pattern (is this the correct thing to do? - we're not told the pattern otherwise)
            IPattern^ pattern = machine->Patterns[0];
            uint64_t patid = pattern->GetHashCode();
            CPattern* pat = m_patternMap->GetOrStoreReBuzzTypeById(patid, pattern);

            exInterface->SetPatternTargetMachine(pat, mach);
        }

        void MachineWrapper::SetPatternName(String^ machine, String^ oldName, String^ newName)
        {
            //Get Ex Interface for calling the machine
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();

            //Get the machine
            for each (IMachine ^ m in Global::Buzz->Song->Machines)
            {
                if (m->Name == machine)
                {
                    //Get the pattern
                    for each (IPattern^ p in  m->Patterns)
                    {
                        if((p->Name == newName) || (p->Name == oldName))
                        {
                            //Get the CPattern for this patter
                            uint64_t id = p->GetHashCode();
                            CPattern* pat = m_patternMap->GetOrStoreReBuzzTypeById(id, p);

                            //Convert the string to c
                            std::string cstrName;
                            Utils::CLRStringToStdString(newName, cstrName);

                            //Tell the machine
                            exInterface->RenamePattern(pat, cstrName.c_str());

                            //End
                            return;
                        }
                    }

                }
            }
        }

        void * MachineWrapper::GetCPattern(IPattern^ p)
        {
            if (p == nullptr)
                return NULL;

            uint64_t patid = p->GetHashCode();
            return m_patternMap->GetBuzzTypeById(patid);
        }

        IPattern^ MachineWrapper::GetReBuzzPattern(void* pat)
        {
            if (pat == NULL)
                return nullptr;

            return m_patternMap->GetReBuzzTypeByBuzzType(reinterpret_cast<CPattern *>( pat));
        }

        IMachine^ MachineWrapper::GetReBuzzMachine(void* mach)
        {
            if (mach == NULL)
                return nullptr;

            return m_machineMap->GetReBuzzTypeByBuzzType(reinterpret_cast<CMachine*>(mach));
        }

        CMachineData* MachineWrapper::GetBuzzMachineData(void* mach)
        {
            if (mach == NULL)
                return NULL;

            return m_machineMap->GetBuzzEmulationType(reinterpret_cast<CMachine*>(mach));
        }

        CPatternData* MachineWrapper::GetBuzzPatternData(void* pat)
        {
            if (pat == NULL)
                return NULL;

            return m_patternMap->GetBuzzEmulationType(reinterpret_cast<CPattern*>(pat));
        }

        void* MachineWrapper::GetCMachine(IMachine^ m)
        {
            if (m == nullptr)
                return NULL;

            uint64_t machid = m->GetHashCode();
            return m_machineMap->GetOrStoreReBuzzTypeById(machid, m);
        }

        CMachine * MachineWrapper::GetCMachineByName(const char* name)
        {
            //Names are stored in callback data
            MachineCreateCallbackData* machCallbackData = reinterpret_cast<MachineCreateCallbackData*>(m_machineCallbackData);
            const auto& found = machCallbackData->machineNameMap.find(name);
            if (found == machCallbackData->machineNameMap.end())
                return NULL;

            return m_machineMap->GetBuzzTypeById((*found).second);
        }

        void MachineWrapper::ControlChange(IMachine^ machine, int group, int track, int param, int value)
        {
            //Get machine
            uint64_t machid = machine->GetHashCode();
            CMachine* mach = m_machineMap->GetOrStoreReBuzzTypeById(machid, machine);

            //Not sure how to do this one?
            //
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();
            exInterface->RecordControlChange(mach, group, track, param, value);
        }

        void MachineWrapper::SetModifiedFlag()
        {
            Global::Buzz->SetModifiedFlag();
        }

        static int ConvertBuzzCommandToNative(BuzzCommand cmd)
        {
            int nativeCmd = -1;
            switch (cmd)
            {
            case BuzzCommand::Cut:
                return 0xE123; // ID_EDIT_CUT;
            case BuzzCommand::Copy:
                return 0xE122; // ID_EDIT_COPY;
            case BuzzCommand::Paste:
                return 0xE125; // ID_EDIT_PASTE;
            case BuzzCommand::Undo:
                return 0xE12B; // ID_EDIT_UNDO;
            case BuzzCommand::Redo:
                return 0xE1CB; // ID_EDIT_REDO;
            default:
                return -1;
            }
        }

        bool MachineWrapper::CanExecuteCommand(BuzzCommand cmd)
        {
            //Convert command to native
            int nativeCmd = ConvertBuzzCommandToNative(cmd);
            if (nativeCmd == -1)
                return false; //not supported.

            //Ask buzz machine
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();
            return exInterface->EnableCommandUI(nativeCmd);
        }

        void MachineWrapper::ExecuteCommand(BuzzCommand cmd)
        {
            //Convert command to native
            int nativeCmd = ConvertBuzzCommandToNative(cmd);
            if (nativeCmd == -1)
                return; //not supported.

            //Send command to editor window
            PostMessage(m_hwndEditor, WM_COMMAND, nativeCmd, 0);
        }

        void MachineWrapper::MidiNote(int channel, int value, int velocity)
        {
            m_machine->MidiNote(channel, value, velocity);
        }

        void MachineWrapper::MidiControlChange(int ctrl, int channel, int value)
        {
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();
            exInterface->MidiControlChange(ctrl, channel, value);
        }

        cli::array<byte>^ MachineWrapper::Save()
        {
            //Save data 
            NativeMachineWriter output;
            m_machine->Save(&output);
            
            //Get data 
            const unsigned char* srcdata = output.dataPtr();
            if (srcdata == NULL)
                return nullptr;

            //Convert to .NET array
            cli::array<byte>^ retArray = gcnew cli::array<byte>(output.size());

            //Copy data
            pin_ptr<byte> destPtr = &retArray[0];
            memcpy(destPtr, srcdata, output.size());

            return retArray;
        }

        cli::array<int>^ MachineWrapper::GetPatternEditorMachineMIDIEvents(IPattern^ pattern)
        {
            //Get pattern
            uint64_t patid = pattern->GetHashCode();
            CPattern* pat = m_patternMap->GetOrStoreReBuzzTypeById(patid, pattern);

            //Save data 
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();
            NativeMachineWriter output;
            exInterface->ExportMidiEvents(pat, &output);

            //Get data 
            const unsigned char* srcdata = output.dataPtr();
            if (srcdata == NULL)
                return nullptr;

            //Convert to .NET array
            cli::array<int>^ retArray = gcnew cli::array<int>(output.size() / sizeof(int));
            pin_ptr<int> destPtr = &retArray[0];
            memcpy(destPtr, srcdata, output.size());
            return retArray;
        }

        void MachineWrapper::SetPatternEditorMachineMIDIEvents(IPattern^ pattern, cli::array<int>^ data)
        {
            if ((data == nullptr) || (data->Length == 0))
                return;

            //Get pattern
            uint64_t patid = pattern->GetHashCode();
            CPattern* pat = m_patternMap->GetOrStoreReBuzzTypeById(patid, pattern);

            //Convert data from .NET to native
            std::vector<unsigned char> nativeData(data->Length * sizeof(int));
            pin_ptr<int> srcPtr = &data[0];
            memcpy(&nativeData[0], srcPtr, data->Length * sizeof(int));
            NativeMachineReader input(&nativeData[0], nativeData.size());

            //Load data
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();
            exInterface->ImportMidiEvents(pat, &input);
        }

        void MachineWrapper::Activate()
        {
            if (m_hwndEditor)
            {
                SetForegroundWindow(m_hwndEditor);
                SetActiveWindow(m_hwndEditor);
            }
        }

        void MachineWrapper::Release()
        {
            if (m_hwndEditor != NULL)
            {
                CloseWindow(m_hwndEditor);
                DestroyWindow(m_hwndEditor);
            }
        }

        void MachineWrapper::CreatePatternCopy(IPattern^ pnew, IPattern^ p)
        {
            //Get old pattern
            uint64_t oldpatid = p->GetHashCode();
            CPattern* oldPat = m_patternMap->GetOrStoreReBuzzTypeById(oldpatid, p);

            uint64_t newpatid = p->GetHashCode();
            CPattern* newPat = m_patternMap->GetOrStoreReBuzzTypeById(newpatid, p);

            if ((oldPat == NULL) || (newPat == NULL))
                return;

            //Get old pattern data
            CMachineInterfaceEx* exInterface = m_callback->GetExInterface();
            exInterface->CreatePatternCopy(newPat, oldPat);
        }

        void MachineWrapper::UpdateMasterInfo()
        {  
            //populate master info
            m_masterInfo->BeatsPerMin = m_host->MasterInfo->BeatsPerMin;
            //m_mastirInfo->GrooveData = m_host->MasterInfo->GrooveData; //No idea
            m_masterInfo->GrooveSize = m_host->MasterInfo->GrooveSize;
            m_masterInfo->PosInGroove = m_host->MasterInfo->PosInGroove;
            m_masterInfo->PosInTick = m_host->MasterInfo->PosInTick;
            m_masterInfo->SamplesPerSec = m_host->MasterInfo->SamplesPerSec;
            m_masterInfo->SamplesPerTick = m_host->MasterInfo->SamplesPerTick;
            m_masterInfo->TicksPerBeat = m_host->MasterInfo->TicksPerBeat;
            m_masterInfo->TicksPerSec = m_host->MasterInfo->TicksPerSec;
        }
    }
}