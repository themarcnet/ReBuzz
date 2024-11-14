#include <map>

#include "PatternManager.h"
#include "Utils.h"

using BuzzGUI::Common::Global;


namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        struct PatternCreateCallbackData
        {
            RebuzzBuzzLookup<IPattern, CPatternData, CPattern> * patternMap;
            std::vector<CPattern*> callbacksRequired;
            RefClassWrapper < System::Action<IPatternColumn^>> patternChangeAction;
            RefClassWrapper<PropertyChangedEventHandler> propChangeEventHandler;

            std::map<int64_t, std::map<std::string, uint64_t>> patternNameMap;
        };

        void PatternManager::OnReBuzzPatternChange(IPattern^ rebuzzpat, bool lock)
        {
            bool notifyRename = false;
            bool notifyLength = false;
            const char* newnameptr = NULL;
            CMachineInterfaceEx* notifyInterface;
            CPattern* buzzPat;
            int newLen = 0;
            OnPatternEditorRedrawCallback redrawcallback = NULL;
            void* cbparam = NULL;

            {
                std::unique_lock<std::mutex> lg(*m_lock, std::defer_lock);
                if (lock)
                    lg.lock();

                int64_t patid = Utils::ObjectToInt64(rebuzzpat);
                buzzPat = m_patternMap->GetBuzzTypeById(patid);
                if (buzzPat == NULL)
                    return;

                CPatternData* patdata = m_patternMap->GetBuzzEmulationType(buzzPat);
                if (patdata == NULL)
                    return;

                notifyInterface = m_exInterface;
                redrawcallback = m_onPatternEditorRedrawCallback;
                cbparam = m_callbackParam;

                //Get the patten name, and store into the pattern data
                std::string newname;
                Utils::CLRStringToStdString(rebuzzpat->Name, newname);
                if (newname != patdata->name)
                {
                    patdata->name = newname;
                    notifyRename = true;
                    newnameptr = patdata->name.c_str();
                }

                //Check length
                if (patdata->length != rebuzzpat->Length)
                {
                    notifyLength = true;
                    newLen = rebuzzpat->Length;
                    patdata->length = newLen;
                }
            }

            //Notify the native machine of changes
            if (notifyInterface != NULL)
            {
                if (notifyRename)
                {
                    notifyInterface->RenamePattern(buzzPat, newnameptr);
                }

                if (notifyLength)
                {
                    notifyInterface->SetPatternLength(buzzPat, newLen);
                }

                if ((redrawcallback != NULL) && (notifyLength || notifyRename))
                {
                    //Redraw pattern editor
                    redrawcallback(cbparam);
                }
            }
        }

        void PatternManager::PatternChangeCheckCallback(IPattern^ rebuzzpat, const CPattern* buzzpat, const CPatternData* patdata, void* param)
        {
            RefClassWrapper<PatternManager>* me = reinterpret_cast<RefClassWrapper<PatternManager> *>(param);
            me->GetRef()->OnReBuzzPatternChange(rebuzzpat, false);
        }

        void PatternManager::OnReBuzzPatternColumnChange(IPatternColumn^ patcol)
        {
            if ((patcol == nullptr) || (patcol->Pattern == nullptr))
            {
                //We don't know what pattern as changed.  Assume any/all
                std::lock_guard<std::mutex> lg(*m_lock);
                RefClassWrapper<PatternManager> me(this);
                m_patternMap->ForEachItem(PatternChangeCheckCallback, &me);
            }
            else
            {
                OnReBuzzPatternChange(patcol->Pattern, true);
            }
        }

        //Property changed handler
        void PatternManager::OnPropertyChangedCallback(System::Object^ sender, PropertyChangedEventArgs^ args)
        {
            OnReBuzzPatternChange((IPattern^)sender, true);
        }

        static void CreatePatternCallback(void* pat, void* param)
        {
            PatternCreateCallbackData* callbackData = reinterpret_cast<PatternCreateCallbackData*>(param);

            //Get the emulation type, as this contains info about the pattern
            CPattern* buzzPat = reinterpret_cast<CPattern*>(pat);
            CPatternData* patdata = callbackData->patternMap->GetBuzzEmulationType(buzzPat);
            if (patdata == NULL)
                return;

            //Get the ReBuzz pattern
            IPattern^ rebuzzPattern = callbackData->patternMap->GetReBuzzTypeByBuzzType(buzzPat);

            //Get the patten name, and store into the pattern data
            Utils::CLRStringToStdString(rebuzzPattern->Name, patdata->name);

            //Get length
            patdata->length = rebuzzPattern->Length;

            //Store pattern against name and ReBuzz CMachineDataPtr
            int64_t machineId = Utils::ObjectToInt64(rebuzzPattern->Machine);
            int64_t patId = Utils::ObjectToInt64(rebuzzPattern);
            callbackData->patternNameMap[machineId][patdata->name] = patId;

            //Mark this pattern needs to have the callback called for it
            callbackData->callbacksRequired.push_back(buzzPat);

            //Register for events
            rebuzzPattern->PatternChanged += callbackData->patternChangeAction.GetRef();
            rebuzzPattern->PropertyChanged += callbackData->propChangeEventHandler.GetRef();
        }


        PatternManager::PatternManager(OnNewPatternCallback onNewPatCallback, 
                                       OnPatternEditorRedrawCallback onPatternEditorRedrawCallback,
                                        void * callbackData)
        {
            m_lock = new std::mutex();

            //Store callbacks to notify changes made by native machine
            m_onNewPatternCallback = onNewPatCallback;
            m_onPatternEditorRedrawCallback = onPatternEditorRedrawCallback;
            m_callbackParam = callbackData;

            PatternCreateCallbackData* patternCallbackData = new PatternCreateCallbackData();
            m_patternCallbackData = patternCallbackData;

            m_patternMap = new RebuzzBuzzLookup< IPattern, CPatternData, CPattern>(CreatePatternCallback, m_patternCallbackData);

            //Set up action for being notified on pattern change
            m_onPatternChangeAction = gcnew System::Action<IPatternColumn^>(this, &PatternManager::OnReBuzzPatternColumnChange);

            //Set up action for property changes
            m_onPropChangeEventHandler = gcnew PropertyChangedEventHandler(this, &PatternManager::OnPropertyChangedCallback);

            //Populate callback data
            patternCallbackData->patternMap = m_patternMap;
            patternCallbackData->patternChangeAction.Assign(m_onPatternChangeAction);
            patternCallbackData->propChangeEventHandler.Assign(m_onPropChangeEventHandler);
        }

        static void RemovePatChangeAction(IPattern^ rebuzzpat, const CPattern* buzzpat, const CPatternData* patdata, void* param)
        {
            RefClassWrapper<System::Action<IPatternColumn^>>* tmpact = reinterpret_cast<RefClassWrapper<System::Action<IPatternColumn^>> *>(param);
            rebuzzpat->PatternChanged -= tmpact->GetRef();
        }

        static void RemovePropChangeHandler(IPattern^ rebuzzpat, const CPattern* buzzpat, const CPatternData* patdata, void* param)
        {
            RefClassWrapper<PropertyChangedEventHandler>* tmpact = reinterpret_cast<RefClassWrapper<PropertyChangedEventHandler> *>(param);
            rebuzzpat->PropertyChanged -= tmpact->GetRef();
        }

        PatternManager::~PatternManager()
        {
            Release();

            PatternCreateCallbackData* patternCallbackData = reinterpret_cast<PatternCreateCallbackData*>(m_patternCallbackData);
           
            delete patternCallbackData;
            
            if (m_lock != NULL)
            {
                delete m_lock;
                m_lock = NULL;
            }
        }

        void PatternManager::Release()
        {
            if (m_lock == NULL)
                return;

            std::lock_guard<std::mutex> lg(*m_lock);

            //Unreigster the action from all patterns
            if (m_onPatternChangeAction != nullptr)
            {
                RefClassWrapper<System::Action<IPatternColumn^>> tmpact(m_onPatternChangeAction);
                if(m_patternMap != NULL)
                    m_patternMap->ForEachItem(RemovePatChangeAction, &tmpact);
                
                delete m_onPatternChangeAction;
            }

            if (m_onPropChangeEventHandler != nullptr)
            {
                RefClassWrapper<PropertyChangedEventHandler> tmphdlr(m_onPropChangeEventHandler);
                if(m_patternMap != NULL)
                    m_patternMap->ForEachItem(RemovePropChangeHandler, &tmphdlr);
                
                delete m_onPropChangeEventHandler;
            }

            if (m_patternMap != NULL)
            {
                m_patternMap->Release();
                delete m_patternMap;
                m_patternMap = NULL;
            }
        }

        void PatternManager::SetExInterface(CMachineInterfaceEx* iface)
        {
            std::lock_guard<std::mutex> lg(*m_lock);
            m_exInterface = iface;
        }

        CPattern* PatternManager::GetPatternByName(IMachine^ rebuzzmac, const char* name)
        {
            std::lock_guard<std::mutex> lg(*m_lock);

            //We need the machine CMachineDataPtr as an int64 to use the name map
            int64_t cmachineid = Utils::ObjectToInt64(rebuzzmac);
            
            //Get the CPattern * from the name map
            PatternCreateCallbackData* patternCallbackData = reinterpret_cast<PatternCreateCallbackData*>(m_patternCallbackData);
            const auto& foundmach = patternCallbackData->patternNameMap.find(cmachineid);
            bool foundMach = (foundmach != patternCallbackData->patternNameMap.end());
            

            if (foundMach)
            {
                const std::map<std::string, uint64_t>& patnameIdMap = (*foundmach).second;
                const auto& foundname = patnameIdMap.find(name);
                if (foundname != patnameIdMap.end())
                {
                    return m_patternMap->GetBuzzTypeById((*foundname).second);
                }
            }

            String^ clrPatName = Utils::stdStringToCLRString(name);
            try
            {
                //Machine / Pattern do not exist. 
                for each (IPattern ^ pat in rebuzzmac->Patterns)
                {
                    if (clrPatName == pat->Name)
                    {
                        int64_t patId = Utils::ObjectToInt64(pat);
                        patternCallbackData->patternNameMap[cmachineid][name] = patId;
                        return m_patternMap->GetOrStoreReBuzzTypeById(patId, pat);
                    }
                }
            }
            finally
            {
                delete clrPatName;
            }

            return NULL;
        }

        CPattern* PatternManager::GetOrStorePattern(IPattern^ p)
        {
            if (p == nullptr)
                return NULL;

            OnNewPatternCallback callback = NULL;
            std::vector<CPattern*> callbackPats;
            std::vector<int> callbackPatLens;
            void* cbparam = NULL;
            CPattern* cRetPat = NULL;
            CMachineInterfaceEx* exIface = NULL;
            {
                std::lock_guard<std::mutex> lg(*m_lock);

                //Get/Store pattern
                uint64_t id = Utils::ObjectToInt64(p);
                cRetPat = m_patternMap->GetOrStoreReBuzzTypeById(id, p);
                exIface = m_exInterface;


                //Set up for calling any deferred callbacks outside the lock
                //The callback list gets populated when  GetOrStoreReBuzzTypeById() is called (above)
                PatternCreateCallbackData* patternCallbackData = reinterpret_cast<PatternCreateCallbackData*>(m_patternCallbackData);
                if (patternCallbackData != NULL)
                {   
                    for (const auto& p : patternCallbackData->callbacksRequired)
                    {
                        CPatternData* data = m_patternMap->GetBuzzEmulationType(p);
                        if (data != NULL)
                        {
                            callbackPats.push_back(p);
                            callbackPatLens.push_back(data->length);
                        }
                    }

                    patternCallbackData->callbacksRequired.clear();

                    callback = m_onNewPatternCallback;
                    cbparam = m_callbackParam;
                }
            }

            //Call the callbacks, if required, outside of the lock
            int idx = 0;
            for (const auto& cb : callbackPats)
            {
                if(callback != NULL)
                    callback(cb, cbparam);

                //Notify the machine
                if (exIface != NULL)
                {   
                    exIface->CreatePattern(cb, callbackPatLens[idx]);
                }

                ++idx;
            }
            

            return cRetPat;
        }

        IPattern^ PatternManager::GetReBuzzPattern(CPattern* pat)
        {
            if (pat == NULL)
                return nullptr;

            std::lock_guard<std::mutex> lg(*m_lock);

            return m_patternMap->GetReBuzzTypeByBuzzType(pat);
        }

        CPatternData* PatternManager::GetBuzzPatternData(CPattern* pat)
        {
            if (pat == NULL)
                return NULL;

            std::lock_guard<std::mutex> lg(*m_lock);

            return m_patternMap->GetBuzzEmulationType(pat);
        }

        void PatternManager::OnNativePatternChange(CPattern* pat, int newLen, const char * newName )
        {
            bool notifyNativeLength = false;
            bool hasChanged = false;
            IPattern^ rebuzzPat = nullptr;
           
            {
                std::lock_guard<std::mutex> lg(*m_lock);

                //Get Rebuzz pattern
                rebuzzPat = m_patternMap->GetReBuzzTypeByBuzzType(pat);
            }
            
            //Update info in ReBuzz
            if ((newLen > 0) && (rebuzzPat->Length != newLen))
            {
                rebuzzPat->Length = newLen;
                hasChanged = true;
            }
            
            if (newName != NULL)
            {   
                rebuzzPat->Name =  Utils::stdStringToCLRString(newName);
                hasChanged = true;
            }

            if(hasChanged)
                rebuzzPat->NotifyPatternChanged();
        }
    }

}