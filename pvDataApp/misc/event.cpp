/* event.cpp */
#include <cstddef>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <cstdio>

#include <memory>
#include <vector>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include "noDefaultMethods.h"
#include "pvType.h"
#include "lock.h"
#include "event.h"

namespace epics { namespace pvData { 

static volatile int64 totalConstruct = 0;
static volatile int64 totalDestruct = 0;
static Mutex *globalMutex = 0;
static String alreadyOn("already on list");

class ConstructDestructCallbackEvent : public ConstructDestructCallback {
public:
    ConstructDestructCallbackEvent();
    virtual String getConstructName();
    virtual int64 getTotalConstruct();
    virtual int64 getTotalDestruct();
    virtual int64 getTotalReferenceCount();
private:
    String name;
};

ConstructDestructCallbackEvent::ConstructDestructCallbackEvent()
: name("event")
{
    getShowConstructDestruct()->registerCallback(this);
}

String ConstructDestructCallbackEvent::getConstructName() {return name;}

int64 ConstructDestructCallbackEvent::getTotalConstruct()
{
    Lock xx(globalMutex);
    return totalConstruct;
}

int64 ConstructDestructCallbackEvent::getTotalDestruct()
{
    Lock xx(globalMutex);
    return totalDestruct;
}

int64 ConstructDestructCallbackEvent::getTotalReferenceCount()
{
    return 0;
}

static ConstructDestructCallback *pConstructDestructCallback;

static void init()
{
     static Mutex mutex = Mutex();
     Lock xx(&mutex);
     if(globalMutex==0) {
        globalMutex = new Mutex();
        pConstructDestructCallback = new ConstructDestructCallbackEvent();
     }
}

Event::~Event() {
    epicsEventDestroy(id);
    totalDestruct++;
}

Event::Event(EventInitialState initial)
: id(epicsEventCreate((initial==eventEmpty)?epicsEventEmpty : epicsEventFull))
{
    init();
    totalConstruct++;
}

ConstructDestructCallback *Event::getConstructDestructCallback()
{
    init();
    return pConstructDestructCallback;
}

void Event::signal()
{
    epicsEventSignal(id);
}

bool Event::wait ()
{
    epicsEventWaitStatus status = epicsEventWait(id);
    return status==epicsEventWaitOK ? true : false;
}

bool Event::wait ( double timeOut )
{
    epicsEventWaitStatus status = epicsEventWaitWithTimeout(id,timeOut);
    return status==epicsEventWaitOK ? true : false;
}

bool Event::tryWait ()
{
    epicsEventWaitStatus status = epicsEventTryWait(id);
    return status==epicsEventWaitOK ? true : false;
}

}}