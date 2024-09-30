#pragma once

using Buzz::MachineInterface::MachineDecl;
using Buzz::MachineInterface::IBuzzMachine;
using Buzz::MachineInterface::IBuzzMachineHost;
using Buzz::MachineInterface::ParameterDecl;
using System::ComponentModel::INotifyPropertyChanged;
using  System::ComponentModel::PropertyChangedEventHandler;
using System::String;
using System::Collections::Generic::IDictionary;

#include "..\..\Buzz\MachineInterface.h"
#include "MachineCallbackWrapper.h"

[MachineDecl(Name = "Pattern XP Editor", ShortName = "PXP", Author = "WDE/MarCNeT", MaxTracks = 8, InputCount = 0, OutputCount = 0)]
public ref class ReBuzzPatternXpMachine : IBuzzMachine, INotifyPropertyChanged
{
public:

    //Constructor
    ReBuzzPatternXpMachine(IBuzzMachineHost^ host);

    //Destructor
    ~ReBuzzPatternXpMachine();

    void Work();

    void ImportFinished(IDictionary<String^, String^>^ machineNameMap);

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
    IBuzzMachineHost^ m_host;
    CMachineInterface* m_interface;
    bool m_dummyParam;
    MachineCallbackWrapper* m_callbackWrapper;
    CMachineData * m_thisMachine;
};