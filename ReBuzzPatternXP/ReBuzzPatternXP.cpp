
#include "ReBuzzPatternXP.h"
#include "MachineCallbackWrapper.h"

extern "C"
{
    __declspec(dllexport) CMachineInterface* __cdecl CreateMachine();
}


ReBuzzPatternXpMachine::ReBuzzPatternXpMachine(IBuzzMachineHost^ host) : m_host(host),
                                                                         m_dummyParam(false)
{
    m_thisMachine = new CMachineData();
    m_interface = CreateMachine();

    //Create callback wrapper class - PatternXP will be calling this, and 
    //the wrapper will forward calls to ReBuzz
    m_callbackWrapper = new MachineCallbackWrapper(m_thisMachine, m_interface, host);

    
    m_interface->pCB = (CMICallbacks*)m_callbackWrapper;

}

ReBuzzPatternXpMachine::~ReBuzzPatternXpMachine()
{
    delete m_interface;
    delete m_callbackWrapper;
}

void ReBuzzPatternXpMachine::Work()
{
    if (m_interface != NULL)
    {
        //The parameters are not used by 'Work', so just put anything in....
        m_interface->Work(NULL, 0, 0);
    }
}

void ReBuzzPatternXpMachine::ImportFinished(IDictionary<String^, String^>^ machineNameMap)
{}

