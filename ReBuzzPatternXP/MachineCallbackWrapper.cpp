#include "MachineCallbackWrapper.h"
#include "MachineEventWrapper.h"

using BuzzGUI::Common::Global;

MachineCallbackWrapper::MachineCallbackWrapper(CMachineData * mach, 
                                               CMachineInterface * iface,
                                               IBuzzMachineHost^ host) : m_exInterface(NULL),
                                                                         m_interface(iface),
                                                                         m_thisMachine(mach),
                                                                         m_addedMachineEventHandler(NULL),
                                                                         m_deleteMachineEventHandler(NULL)
{
    GCHandle hostHandle =  GCHandle::Alloc(host);
    m_hostHandle =  GCHandle::ToIntPtr(hostHandle).ToPointer();

}

MachineCallbackWrapper::~MachineCallbackWrapper()
{
    GCHandle hostHandle  = GCHandle::FromIntPtr(IntPtr(m_hostHandle));
    hostHandle.Free();
    m_hostHandle = NULL;

    if (m_addedMachineEventHandler != NULL)
    {
        GCHandle eventHandlerHandle = GCHandle::FromIntPtr(IntPtr(m_addedMachineEventHandler));
        MachineEventWrapper^ evtWrapper = (MachineEventWrapper^)eventHandlerHandle.Target;
        Global::Buzz->Song->MachineAdded -= evtWrapper->GetAction();
        eventHandlerHandle.Free();
        m_addedMachineEventHandler = NULL;
    }

    if (m_deleteMachineEventHandler != NULL)
    {
        GCHandle eventHandlerHandle = GCHandle::FromIntPtr(IntPtr(m_deleteMachineEventHandler));
        MachineEventWrapper^ evtWrapper = (MachineEventWrapper^)eventHandlerHandle.Target;
        Global::Buzz->Song->MachineRemoved -= evtWrapper->GetAction();
        eventHandlerHandle.Free();
        m_deleteMachineEventHandler = NULL;
    }
}

IBuzzMachineHost^ MachineCallbackWrapper::GetHost()
{
    GCHandle hostHandle = GCHandle::FromIntPtr(IntPtr(m_hostHandle));
    IBuzzMachineHost^ rethost = (IBuzzMachineHost^)hostHandle.Target;
    return rethost;
}

void MachineCallbackWrapper::SetMachineInterfaceEx(CMachineInterfaceEx* pex)
{
    m_exInterface = pex;
}

CMachine* MachineCallbackWrapper::GetThisMachine()
{
    return (CMachine*)m_thisMachine;
}

void* MachineCallbackWrapper::AllocEventHandler()
{
    MachineEventWrapper^ eventWrapper = gcnew MachineEventWrapper(m_interface);
    GCHandle eventWrapperHandle = GCHandle::Alloc(eventWrapper);
    return GCHandle::ToIntPtr(eventWrapperHandle).ToPointer();
}

MachineEventWrapper^ GetEventHandler(void* p)
{
    GCHandle wrapperHandle = GCHandle::FromIntPtr(IntPtr(p));
    MachineEventWrapper^ retWrapper = (MachineEventWrapper^)wrapperHandle.Target;
    return retWrapper;
}
    
void MachineCallbackWrapper::SetEventHandler(CMachine* pmac, BEventType et, EVENT_HANDLER_PTR p, void* param)
{
    //Make sure this is for us
    if (pmac != (CMachine *)m_thisMachine)
        return;


    switch (et)
    {
        case gAddMachine:
        case gUndeleteMachine:
            //Ask ReBuzz to call us back if a machine is added to the song
            if (m_addedMachineEventHandler == NULL)
            {
                m_addedMachineEventHandler = AllocEventHandler();

                //Tell ReBuzz to call the event handler on event, which will call all the registered callbacks
                MachineEventWrapper^ eventHandler = GetEventHandler(m_addedMachineEventHandler);
                System::Action<IMachine^>^ action = gcnew System::Action<IMachine^>(eventHandler, &MachineEventWrapper::OnEvent);
                eventHandler->SetAction(action);
                Global::Buzz->Song->MachineAdded += action;
            }

            //Add callback and param to event handler
            GetEventHandler(m_addedMachineEventHandler)->AddEvent(p, param);
            break;
        case gDeleteMachine:
            //Ask ReBuzz to call us back if a machine is added to the song
            if (m_deleteMachineEventHandler == NULL)
            {
                m_deleteMachineEventHandler = AllocEventHandler();

                //Tell ReBuzz to call our method on event, which will call all the registered callbacks
                MachineEventWrapper^ eventHandler = GetEventHandler(m_deleteMachineEventHandler);
                System::Action<IMachine^>^ action = gcnew System::Action<IMachine^>(eventHandler, &MachineEventWrapper::OnEvent);
                eventHandler->SetAction(action);
                Global::Buzz->Song->MachineRemoved += action;
            }

            //Add callback and param to event handler
            GetEventHandler(m_deleteMachineEventHandler)->AddEvent(p, param);
            break;
    }
}
