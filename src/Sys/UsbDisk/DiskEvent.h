#ifndef DiskEvent_H
#define DiskEvent_H

#include <string>

namespace Hippo {

class DiskEvent{
public:
    DiskEvent(const std::string &event);
	DiskEvent(){}
    ~DiskEvent(){}
    int report();
    int buildEvent(const char* desc, const char* dev, int pvrFlag);
    void doUsbEvent();
    
private:
    std::string m_event;    
};//DiskEvent

};//namespace Hippo

#endif //DiskEvent_H

