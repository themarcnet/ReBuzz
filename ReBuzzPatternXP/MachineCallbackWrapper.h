#pragma once

#include "..\..\buzz\MachineInterface.h"


using namespace  System::Runtime::InteropServices;
using Buzz::MachineInterface::IBuzzMachineHost;
using BuzzGUI::Interfaces::IMachine;
using System::IntPtr;

struct CMachineData
{
    //This is dummy data that we return to the machine as CMachine *
    //The machine is not supposed to know what this data is, or what the values represent,
    //just that the address is a unique identifier for a specific machine.
    unsigned char m_machineBytes[512];
};


//NOTE: Not everycallback is implemented - only those that PatternXP requires!
//      Everything else is stubbed out.
class MachineCallbackWrapper : CMICallbacks
{
public:
    MachineCallbackWrapper(CMachineData* machine, CMachineInterface* iface,
        IBuzzMachineHost^ host);

    ~MachineCallbackWrapper();

    //Implementation of CMICallbacks (in no particular order)
    void SetMachineInterfaceEx(CMachineInterfaceEx* pex) override;
    CMachine* GetThisMachine() override;
    void SetEventHandler(CMachine* pmac, BEventType et, EVENT_HANDLER_PTR p, void* param) override;

private:

    IBuzzMachineHost^ GetHost();
    void* AllocEventHandler();

    void* m_hostHandle;
    CMachineInterfaceEx* m_exInterface;
    CMachineInterface* m_interface;
    CMachineData* m_thisMachine;
    void* m_addedMachineEventHandler;
    void* m_deleteMachineEventHandler;
};