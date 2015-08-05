/*
    Event system for RenderWare objects.
    Allows you to register event handlers to objects which can be triggered by the runtime.
*/

typedef uint32 event_t;

#define RW_ANY_EVENT    0xFFFFFFFF

typedef void (__cdecl*EventHandler_t)( RwObject *obj, event_t triggeredEvent, void *callbackData, void *ud );

void RegisterEventHandler( RwObject *obj, event_t eventID, EventHandler_t theHandler, void *ud = NULL );
void UnregisterEventHandler( RwObject *obj, event_t eventID, EventHandler_t theHandler );

bool TriggerEvent( RwObject *obj, event_t eventID, void *ud );