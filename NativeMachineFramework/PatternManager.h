#pragma once

#include "Buzz\MachineInterface.h"
#include "RebuzzBuzzLookup.h"
#include "BuzzDataTypes.h"

using BuzzGUI::Interfaces::IPattern;
using BuzzGUI::Interfaces::IMachine;
using BuzzGUI::Interfaces::IPatternColumn;

using System::ComponentModel::PropertyChangedEventHandler;
using System::ComponentModel::PropertyChangedEventArgs;
using System::Action;

namespace ReBuzz
{
    namespace NativeMachineFramework
    {
        typedef OnNewBuzzLookupItemCallback OnNewPatternCallback;

        typedef void(*OnPatternEditorRedrawCallback)(void * param);
     
        public ref class PatternManager : System::IDisposable
        {
        public:
            PatternManager(OnNewPatternCallback onNewPatternCallback, 
                           OnPatternEditorRedrawCallback onPatternEditorRedrawCallback,
                           void* callbackData);
            ~PatternManager();

            void Release();

            void SetExInterface(CMachineInterfaceEx* iface);

            CPattern* GetPatternByName(IMachine^ rebuzzmac, const char* name);

            CPattern* GetOrStorePattern(IPattern^ p);

            IPattern^ GetReBuzzPattern(CPattern * pat);

            CPatternData* GetBuzzPatternData(CPattern * pat);

            void OnNativePatternChange(CPattern* pat, int newLen, const char * newName );

            void ScanMachineForPatterns(IMachine^ mach);
            
        private:
            
            void OnReBuzzPatternColumnChange(IPatternColumn^ patcol);
            void OnReBuzzPatternChange(IPattern^ patcol, bool lock);

            static void PatternChangeCheckCallback(IPattern^ rebuzzpat, const CPattern* buzzpat, const CPatternData* patdata, void* param);


            void OnPropertyChangedCallback(System::Object^ sender, PropertyChangedEventArgs^ args);

            CMachineInterfaceEx* m_exInterface;
            RebuzzBuzzLookup<IPattern, CPatternData, CPattern>* m_patternMap;
            void* m_patternCallbackData;
            std::mutex* m_lock;

            OnNewPatternCallback m_onNewPatternCallback;
            OnPatternEditorRedrawCallback m_onPatternEditorRedrawCallback;
            void* m_callbackParam;

            System::Action<IPatternColumn^>^ m_onPatternChangeAction;
            PropertyChangedEventHandler^ m_onPropChangeEventHandler;
        };
    }
}