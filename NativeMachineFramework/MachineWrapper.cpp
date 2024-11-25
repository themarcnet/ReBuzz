#include <Windows.h>
#include <commctrl.h>
#include <map>

#include "Buzz\MachineInterface.h"

#include "MachineWrapper.h"
#include "NativeMFCMachineControl.h"
#include "Utils.h"
#include "NativeMachineWriter.h"
#include "NativeMachineReader.h"

#include <sstream>

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
        //static LRESULT CALLBACK OverriddenWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        static const UINT_PTR s_uiSubClassId = 0x07eb0220; //ID I made up for use with WndProc sub classing routines

        static LRESULT CALLBACK OverriddenWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
        {
            //Get class instance
            RefClassWrapper<MachineWrapper>* classRef = reinterpret_cast<RefClassWrapper<MachineWrapper> *>(dwRefData);
            if (classRef == NULL)
            {
                return 0;
            }

            /*
            std::ostringstream msg;
            msg << "uMsg = " << uMsg << " \r\n";
            OutputDebugStringA(msg.str().c_str());
            */

            //Get override
            void* callbackParam = NULL;
            OnWindowsMessage callbackProc = classRef->GetRef()->GetEditorOverrideCallback(uMsg, &callbackParam);
            if (callbackProc != NULL)
            {
                bool block = false;
                LRESULT res = callbackProc(hWnd, uMsg, wParam, lParam, callbackParam, &block);
                
                //If callback specified to not pass the message onto the actual window, then return now.
                if (block)
                    return res;
            }

            //Call the real window proc
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

       
        void MachineWrapper::OnSequenceCreatedByReBuzz(int seq)
        {

        }

        void MachineWrapper::OnSequecneRemovedByReBuzz(int seq)
        {

        }

        void MachineWrapper::OnMachineCreatedByReBuzz(IMachine^ machine)
        {
            //Update pattern manager with patterns from this machine
            m_patternMgr->ScanMachineForPatterns(machine);

            //Machine Manager registers its own event handler for this, so we don't
            //need to explicitly call it here.
        }

        static void updateWaveLevel(CWaveLevel* buzzwavlevel, IWaveLayer^ rebuzzWaveLayer)
        {
            //Populate the buzz wave level
            buzzwavlevel->LoopEnd = rebuzzWaveLayer->LoopEnd;
            buzzwavlevel->LoopStart = rebuzzWaveLayer->LoopStart;
            buzzwavlevel->numSamples = rebuzzWaveLayer->SampleCount;
            buzzwavlevel->SamplesPerSec = rebuzzWaveLayer->SampleRate;
            buzzwavlevel->pSamples = (short*)rebuzzWaveLayer->RawSamples.ToPointer();
        }

        static void  OnNewBuzzWaveLevel(void* item, void* param)
        {
            /*MachineCreateCallbackData* machCallbackData = reinterpret_cast<MachineCreateCallbackData*>(param);

            //Get the rebuzz class
            CWaveLevel* buzzwavlevel = reinterpret_cast<CWaveLevel*>(item);
            IWaveLayer^ rebuzzWaveLayer = machCallbackData->machineWrapper.GetRef()->GetReBuzzWaveLevel(buzzwavlevel);
            if (rebuzzWaveLayer == nullptr)
                return;

            updateWaveLevel(buzzwavlevel, rebuzzWaveLayer);*/
        }

        static void OnNewSequence(void* item, void* param)
        {
        }

        MachineWrapper::MachineWrapper(void * machine,
                                       IBuzzMachineHost^ host,
                                       IBuzzMachine^ buzzmachine,
                                        void* callbackparam,
                                        OnPatternEditorCreateCallback editorCreateCallback,
                                        KeyboardFocusWindowHandleCallback kbcallback,
                                        OnPatternEditorRedrawCallback redrawcallback) : 
                                                                     m_thisref(new RefClassWrapper<MachineWrapper>(this)),
                                                                     m_machine((CMachineInterface *)machine),
                                                                     m_thisCMachine(NULL),
                                                                     m_host(host),
                                                                     m_hwndEditor(NULL),
                                                                     m_initialised(false),
                                                                     m_buzzmachine(buzzmachine),
                                                                     m_patternEditorPattern(NULL),
                                                                     m_patternEditorMachine(NULL),
                                                                     m_control(nullptr),
                                                                     m_editorCreateCallback(editorCreateCallback),
                                                                     m_kbFocusWndcallback(kbcallback),
                                                                     m_kbFocusCallbackParam(callbackparam),
                                                                     m_onKeyDownHandler(nullptr),
                                                                     m_onKeyupHandler(nullptr),
                                                                     m_editorMessageMap(new std::unordered_map<UINT, OnWindowsMessage>()),
                                                                     m_editorMessageParamMap( new std::unordered_map<UINT, void *>())
        {
            //Create machine manager
            m_machineMgr = gcnew MachineManager();

            //Create pattern manager
            m_patternMgr = gcnew PatternManager(NULL, redrawcallback, callbackparam);

            m_waveLevelsMap = new RebuzzBuzzLookup<IWaveLayer, int, CWaveLevel>(OnNewBuzzWaveLevel, m_mapCallbackData);
            m_sequenceMap = new RebuzzBuzzLookup<ISequence, int, CSequence>(OnNewSequence, m_mapCallbackData);

            //Register add this machine to the machine map
            if (host->Machine != nullptr)
            {
                Init();
            }

            //Allocate some master info
            m_masterInfo = new CMasterInfo();

            //Ask ReBuzz to tell us when a sequence has been added
            m_seqAddedAction = gcnew System::Action<int>(this, &MachineWrapper::OnSequenceCreatedByReBuzz);
            Global::Buzz->Song->SequenceAdded += m_seqAddedAction;

            //Ask ReBuzz to tell us when a sequence has been removed
            m_seqRemovedAction = gcnew System::Action<int>(this, &MachineWrapper::OnSequecneRemovedByReBuzz);
            Global::Buzz->Song->SequenceRemoved += m_seqRemovedAction;

            //Ask ReBuzz to tell us when a machine has been added
            m_machineAddedAction = gcnew System::Action<IMachine^>(this, &MachineWrapper::OnMachineCreatedByReBuzz);
            Global::Buzz->Song->MachineAdded += m_machineAddedAction;
        }

        MachineWrapper::~MachineWrapper()
        {
            Release();

            m_machine->pCB = NULL; //Callbacks no longer available...
            delete m_callbackWrapper;
            delete m_waveLevelsMap;
            delete m_sequenceMap;
            delete m_masterInfo;
            
        }

        void MachineWrapper::Init()
        {
            if (!m_initialised && (m_host->Machine != nullptr))
            {
                //Store this machine
                m_thisCMachine = m_machineMgr->GetOrStoreMachine(m_host->Machine);
                m_rebuzzMachine = m_host->Machine;

                //populate master info
                m_machine->pMasterInfo = m_masterInfo;
                
                //Create callback wrapper class
                m_callbackWrapper = new MachineCallbackWrapper(this, m_machineMgr, m_buzzmachine, m_host, m_machine, m_thisCMachine, m_masterInfo);

                //Set the callback instance on the machine interface 
                m_machine->pCB = (CMICallbacks*)m_callbackWrapper;

                //Collect the patterns
                for each (IPattern ^ p in m_host->Machine->Patterns)
                {
                    m_patternMgr->GetOrStorePattern(p);
                }

                //Finally init the actual machine
                m_machine->Init(NULL);

                //We should have an ExInterface at this point, so tell the patten manager
                CMachineInterfaceEx* exiface = m_callbackWrapper->GetExInterface();
                m_patternMgr->SetExInterface(exiface);

                m_initialised = true;
            }
        }

        void MachineWrapper::Release()
        {
            if (m_hwndEditor != NULL)
            {
                //Restore the window proc first!
                RemoveWindowSubclass(m_hwndEditor, OverriddenWindowProc, s_uiSubClassId);
                
                //Destroy the window
                CloseWindow(m_hwndEditor);
                DestroyWindow(m_hwndEditor);
                m_hwndEditor = NULL;
            }

            if (m_callbackWrapper != NULL)
            {
                m_callbackWrapper->Release();
            }

            Global::Buzz->Song->MachineAdded -= m_machineAddedAction;
            Global::Buzz->Song->SequenceAdded -= m_seqAddedAction;
            Global::Buzz->Song->SequenceRemoved -= m_seqRemovedAction;
            delete m_seqAddedAction;
            delete m_seqRemovedAction;
            delete m_machineAddedAction;

            if (m_control != nullptr)
            {
                if (m_onKeyDownHandler != nullptr)
                {
                    m_control->KeyDown -= this->m_onKeyDownHandler;
                    delete m_onKeyDownHandler;
                    m_onKeyDownHandler = nullptr;
                }

                if (m_onKeyupHandler != nullptr)
                {
                    m_control->KeyUp -= this->m_onKeyupHandler;
                    delete m_onKeyupHandler;
                    m_onKeyupHandler = nullptr;
                }

                delete m_control;
                m_control = nullptr;
            }

            //Remove all machines from the machine manager
            if (m_machineMgr != nullptr)
            {
                m_machineMgr->Release();
                delete m_machineMgr; 
                m_machineMgr = nullptr;
            }
            
            if (m_patternMgr != nullptr)
            {
                m_patternMgr->Release();
                delete m_patternMgr;
                m_patternMgr = nullptr;
            }

            if (m_editorMessageMap != NULL)
            {
                delete m_editorMessageMap;
                m_editorMessageMap = NULL;
            }

            if (m_editorMessageParamMap != NULL)
            {
                delete m_editorMessageParamMap;
                m_editorMessageParamMap = NULL;
            }
        }


        CMachineInterfaceEx* MachineWrapper::GetExInterface()
        {
            return m_callbackWrapper->GetExInterface();
        }

        IMachine^ MachineWrapper::GetThisReBuzzMachine()
        {
            return m_rebuzzMachine;
        }

        void MachineWrapper::SetEditorPattern(IPattern^ pattern)
        {
            //Make sure we're initialised
            Init();

            //Store pattern ref (if not already stored)
            CPattern * pat = m_patternMgr->GetOrStorePattern(pattern);

            //Store the machine ref (if not already stored)
            CMachine* patMach = m_machineMgr->GetOrStoreMachine(pattern->Machine);

            //Get ex interface
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();
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
                m_callbackWrapper->SetDelayedEditorPattern(patMach, pat);
            }
        }

        void MachineWrapper::SendMessageToKeyboardWindow(UINT msg,WPARAM wparam, LPARAM lparam)
        {   
            //If a keyboard window callback has been specified, then call it
            //to get the window that we should be fowarding the windows message to
            HWND hwndSendMsg = m_hwndEditor;
            if (m_kbFocusWndcallback != NULL)
            {
                HWND hwnd = (HWND)m_kbFocusWndcallback(m_kbFocusCallbackParam);
                if (hwnd != NULL)
                    hwndSendMsg = hwnd;
            }
            
            //Set focus on the keyboard focus window
            SetForegroundWindow(m_hwndEditor);
            SetFocus(hwndSendMsg);
            SetActiveWindow(hwndSendMsg);

            //Send the windows message
            SendMessage(hwndSendMsg, msg, wparam,lparam);
        }

     
        void MachineWrapper::OnKeyDown(Object^ sender, KeyEventArgs^ args)
        {
            SendMessageToKeyboardWindow(WM_KEYDOWN , (WPARAM)args->KeyValue, 0);
        }

        void MachineWrapper::OnKeyUp(Object^ sender, KeyEventArgs^ args)
        {
            SendMessageToKeyboardWindow(WM_KEYUP, (WPARAM)args->KeyValue, 0);
        }

        IntPtr MachineWrapper::RebuzzWindowAttachCallback(IntPtr hwnd, void* callbackParam)
        {
            //Get machine wrapper
            RefClassWrapper<MachineWrapper>* classRef = reinterpret_cast<RefClassWrapper<MachineWrapper> *>(callbackParam);
            classRef->GetRef()->Init();

            //Get ex interface
            CMachineInterfaceEx* exInterface = (CMachineInterfaceEx*)classRef->GetRef()->GetExInterface();
            
            //Tell PatterXP to create the pattern editor using the .NET user control as its parent
            void* patternEditorHwnd = exInterface->CreatePatternEditor(hwnd.ToPointer());

            //Store the HWND in the class for sending window messages
            classRef->GetRef()->m_hwndEditor = (HWND)patternEditorHwnd;

            //Overide the wndproc so we can intercept window events
            SetWindowSubclass((HWND)patternEditorHwnd, OverriddenWindowProc, s_uiSubClassId, (DWORD_PTR)classRef);

            //If we have a pattern to set, then do that now
            //(it was deferred from earlier)
            if( (classRef->GetRef()->m_initialised) &&
                (classRef->GetRef()->m_patternEditorMachine != NULL) && 
                (classRef->GetRef()->m_patternEditorPattern != NULL))
            {
                exInterface->SetPatternTargetMachine(classRef->GetRef()->m_patternEditorPattern, 
                                                     classRef->GetRef()->m_patternEditorMachine);
                exInterface->SetEditorPattern(classRef->GetRef()->m_patternEditorPattern);

                classRef->GetRef()->m_patternEditorPattern = NULL;
                classRef->GetRef()->m_patternEditorMachine = NULL;
            }

            //Create and register window event
            classRef->GetRef()->m_onKeyDownHandler = gcnew KeyEventHandler(classRef->GetRef(), &MachineWrapper::OnKeyDown);
            classRef->GetRef()->m_control->KeyDown += classRef->GetRef()->m_onKeyDownHandler;

            classRef->GetRef()->m_onKeyupHandler = gcnew KeyEventHandler(classRef->GetRef(), &MachineWrapper::OnKeyUp);
            classRef->GetRef()->m_control->KeyUp += classRef->GetRef()->m_onKeyupHandler;

            //Tell the caller that the control has now been created and set up
            if(classRef->GetRef()->m_editorCreateCallback != NULL)
                classRef->GetRef()->m_editorCreateCallback(classRef->GetRef()->m_kbFocusCallbackParam);

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

                //Register for events
                m_control->KeyDown += m_onKeyDownHandler;

                //Set focus on the keyboard focus window
                SetForegroundWindow(m_hwndEditor);
                SetFocus(m_hwndEditor);
                SetActiveWindow(m_hwndEditor);
            }

            return m_control;
        }

        void MachineWrapper::OverridePatternEditorWindowsMessage(UINT msg, IntPtr callback, void* param)
        {
            (*m_editorMessageMap)[msg] = reinterpret_cast<OnWindowsMessage>( callback.ToPointer());
            (*m_editorMessageParamMap)[msg] = param;
        }

        OnWindowsMessage MachineWrapper::GetEditorOverrideCallback(UINT msg, void** param)
        {
            const auto& msgHandler = m_editorMessageMap->find(msg);
            if (msgHandler == m_editorMessageMap->end())
                return NULL;

            const auto& msgHandlerParam = m_editorMessageParamMap->find(msg);
            if (msgHandlerParam == m_editorMessageParamMap->end())
            {
                *param = NULL;
            }
            else
            {
                *param = (*msgHandlerParam).second;
            }

            return (*msgHandler).second;
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
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();

            if (m_thisCMachine != NULL)
            {
                //Get our CMachine * 
                CMachine* mach = m_thisCMachine;

                //Find the parameter group and parameter number values
                int paramNum = -1;
                int groupNum = FindParameterGroupAndParam(m_host->Machine, parameter, &paramNum);
                if (groupNum >= 0)
                {
                    //Call the machine
                    exInterface->RecordControlChange(mach, groupNum, track, paramNum, value);
                }
            }
        }

        void MachineWrapper::SetTargetMachine(IMachine^ machine)
        {
            //Get Ex Interface for calling the machine
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();

            //MC: I'm guessing here that setting the target machine ALSO sets
            //    the pattern as well
            // As we don't have the pattern, just pick the first

            //Get machine
            CMachine* mach =  m_machineMgr->GetOrStoreMachine(machine);

            //Get first pattern (is this the correct thing to do? - we're not told the pattern otherwise)
            IPattern^ pattern = machine->Patterns[0];
            CPattern* pat = m_patternMgr->GetOrStorePattern(pattern);

            exInterface->SetPatternTargetMachine(pat, mach);
        }
        void *  MachineWrapper::GetCPattern(IPattern^ p)
        {
            return m_patternMgr->GetOrStorePattern(p);
        }

        IPattern^ MachineWrapper::GetReBuzzPattern(void* pat)
        {
            return m_patternMgr->GetReBuzzPattern(reinterpret_cast<CPattern *>(pat));
        }

        IMachine^ MachineWrapper::GetReBuzzMachine(void* mach)
        {
            return m_machineMgr->GetReBuzzMachine(reinterpret_cast<CMachine*>(mach));
        }

        CMachineData* MachineWrapper::GetBuzzMachineData(void* mach)
        {
            return m_machineMgr->GetBuzzMachineData(reinterpret_cast<CMachine*>(mach));
        }

        CPatternData* MachineWrapper::GetBuzzPatternData(void* pat)
        {
            return m_patternMgr->GetBuzzPatternData(reinterpret_cast<CPattern*>(pat));
        }

        void * MachineWrapper::GetCPatternByName(IMachine^ rebuzzmac, const char* name)
        {
            return m_patternMgr->GetPatternByName(rebuzzmac, name);
        }

        void MachineWrapper::UpdatePattern(CPattern* pat, int newLen, const char * newName)
        {
            m_patternMgr->OnNativePatternChange(pat, newLen, newName);
        }

        void* MachineWrapper::GetCMachine(IMachine^ m)
        {
            return m_machineMgr->GetOrStoreMachine(m);
        }

        CMachine * MachineWrapper::GetCMachineByName(const char* name)
        {
            return m_machineMgr->GetCMachineByName(name);
        }

        CWaveLevel* MachineWrapper::GetWaveLevel(IWaveLayer^ wavelayer)
        {
            if (wavelayer == nullptr)
                return NULL;

            uint64_t id = wavelayer->GetHashCode();
            CWaveLevel* ret =  m_waveLevelsMap->GetOrStoreReBuzzTypeById(id, wavelayer);

            //ReBuzz does not notify us of changes to IWaveLayer, so we need to manually update the return data
            updateWaveLevel(ret, wavelayer);
            return ret;
        }

        IWaveLayer^ MachineWrapper::GetReBuzzWaveLevel(CWaveLevel* wavelevel)
        {
            if (wavelevel == NULL)
                return nullptr;

            return m_waveLevelsMap->GetReBuzzTypeByBuzzType(wavelevel);
        }

        CSequence* MachineWrapper::GetSequence(ISequence^ seq)
        {
            uint64_t id = seq->GetHashCode();
            return m_sequenceMap->GetOrStoreReBuzzTypeById(id, seq);
        }

        ISequence^ MachineWrapper::GetReBuzzSequence(CSequence* seq)
        {
            return m_sequenceMap->GetReBuzzTypeByBuzzType(seq);
        }


        void MachineWrapper::ControlChange(IMachine^ machine, int group, int track, int param, int value)
        {
            //Get machine
            CMachine* mach = m_machineMgr->GetOrStoreMachine(machine);

            //Not sure how to do this one?
            //
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();
            //exInterface->RecordControlChange(mach, group, track, param, value);
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
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();
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
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();
            exInterface->MidiControlChange(ctrl, channel, value);
        }

        cli::array<byte>^ MachineWrapper::Save()
        {
            if (!m_initialised ||   (m_machine == NULL))
                return nullptr;
            

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
            CPattern* pat = m_patternMgr->GetOrStorePattern(pattern);

            //Save data 
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();
            NativeMachineWriter output;
            exInterface->ExportMidiEvents(pat, &output);

            //Get data 
            const unsigned char* srcdata = output.dataPtr();
            if (srcdata == NULL)
                return gcnew cli::array<int>(0); //empty array. Returning null crashes sequence editor

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
            CPattern* pat = m_patternMgr->GetOrStorePattern(pattern);

            //Convert data from .NET to native
            std::vector<unsigned char> nativeData(data->Length * sizeof(int));
            pin_ptr<int> srcPtr = &data[0];
            memcpy(&nativeData[0], srcPtr, data->Length * sizeof(int));
            NativeMachineReader input(&nativeData[0], nativeData.size());

            //Load data
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();
            exInterface->ImportMidiEvents(pat, &input);
        }

        void MachineWrapper::Activate()
        {
            if (m_hwndEditor != NULL)
            {
                SetForegroundWindow(m_hwndEditor);
                SetActiveWindow(m_hwndEditor);
                SetFocus(m_hwndEditor);
            }
        }

        
        void MachineWrapper::CreatePatternCopy(IPattern^ pnew, IPattern^ p)
        {
            //Get old pattern
            CPattern* oldPat = m_patternMgr->GetOrStorePattern(p);

            ///Get new pattern
            CPattern* newPat = m_patternMgr->GetOrStorePattern(pnew);

            if ((oldPat == NULL) || (newPat == NULL))
                return;

            //Get old pattern data
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();
            exInterface->CreatePatternCopy(newPat, oldPat);
        }

        void * MachineWrapper::CreatePattern(IMachine^ machine, const char* name, int len)
        {
            //Create pattern in rebuzz
            String^ patname = Utils::stdStringToCLRString(name);
            machine->CreatePattern(patname, len);

            //Get the CPattern *
            CPattern* cpat = m_patternMgr->GetPatternByName(machine, name);
            return cpat;
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

        void MachineWrapper::Tick()
        {
            //Update master info
            //This copies the master info from ReBuzz into the
            //CMasterInfo pointer attached to the native machine
            UpdateMasterInfo();

            //Call tick on machine on the stroke of every tick
            if (m_initialised && (m_machine != NULL) && m_masterInfo->PosInTick == 0)
            {   
                //Tell the machine to tick
                m_machine->Tick();
            }
        }

        void MachineWrapper::NotifyOfPlayingPattern()
        {
            CMachineInterfaceEx* exInterface = m_callbackWrapper->GetExInterface();

            //Get sequences and tell machine about them
            for each (ISequence ^ s in Global::Buzz->Song->Sequences)
            {
                if (!s->IsDisabled)
                {
                    //Ignore any sequence that does not have a playing sequence
                    IPattern^ playingPat = s->PlayingPattern;
                    if((playingPat != nullptr) && (playingPat->PlayPosition >= 0))
                    {
                        //Get CPattern * for this pattern
                        CPattern* cpat = m_patternMgr->GetOrStorePattern(playingPat);
                        if (cpat != NULL)
                        {
                            //Get CSequence * for this sequence
                            uint64_t seqid = s->GetHashCode();
                            CSequence* cseq = m_sequenceMap->GetOrStoreReBuzzTypeById(seqid, s);
                            if (cseq != NULL)
                            {
                                //Tell interface about this pattern and the current play position within
                                //that pattern.
                                int playpos = s->PlayingPatternPosition;
                                exInterface->PlayPattern(cpat, cseq, playpos);
                            }
                        }
                    }
                }
            }
        }

        
    }
}