#include <stdio.h>

#include "DiskEvent.h"

#include "BrowserAgent.h"
#include "MessageTypes.h"
#include "Assertions.h"

extern "C"{
#include "app_a2Event.h"  
#include "json/json.h"
#include "json/json_object.h"
#include "json/json_public.h"

};

namespace Hippo{

DiskEvent::DiskEvent(const std::string &event)
{
   m_event = event;     
}


int DiskEvent::buildEvent(const char* desc, const char* dev, int pvrFlag)
{
    if ((!desc) || (!dev)) {
        LogUserOperError("DiskEvent::build error\n");
        return -1;
    }
    json_object *eventInfo = json_object_new_object();
    json_object_object_add(eventInfo, "type", json_object_new_string("EVENT_STORAGE_EVENT"));
    json_object_object_add(eventInfo, "event_description", json_object_new_string(desc));
    json_object_object_add(eventInfo, "device", json_object_new_string(dev));
    if (pvrFlag)
        json_object_object_add(eventInfo, "used_for_PVR", json_object_new_string("1"));
    else
        json_object_object_add(eventInfo, "used_for_PVR", json_object_new_string("0"));
    m_event = (char *)json_object_to_json_string(eventInfo);
    json_object_put(eventInfo);
    return 0;
}

void DiskEvent::doUsbEvent()
{
    if (m_event.find("HDD_PARTITION_MOUNTED") != m_event.npos || m_event.find("HDD_REMOVED") != m_event.npos) {
        Hippo::epgBrowserAgent().mPrompt->setUsbMessage(m_event);
        Hippo::epgBrowserAgent().removeMessages(MessageType_Prompt, Hippo::BrowserAgent::PromptUsbInfo, 0);
        sendMessageToEPGBrowser(MessageType_Prompt, Hippo::BrowserAgent::PromptUsbInfo, 1, 300);
        sendMessageToEPGBrowser(MessageType_Prompt, Hippo::BrowserAgent::PromptUsbInfo, 0, 3300);
    }
}

int DiskEvent::report() 
{
    LogUserOperDebug("Event = %s \n", m_event.c_str());
    doUsbEvent();
    app_a2_set_info(m_event.c_str());
    return 0;    
}   

}//namespace Hippo
