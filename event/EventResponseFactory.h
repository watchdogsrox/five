#ifndef INC_EVENT_RESPONSE_FACTORY_H
#define INC_EVENT_RESPONSE_FACTORY_H

// Game headers
#include "Task/System/TaskTypes.h"

// Forward declarations
class CEvent;
class CEventResponse;
class CPed;
class CScenarioPoint;

class CEventResponseFactory
{
public:

	static void CreateEventResponse(CPed* pPed, const CEvent* pEvent, CEventResponse& response);
	static void CreateEventResponse(CPed* pPed, const CEvent* pEvent, CTaskTypes::eTaskType iTaskType, CEventResponse& response);

	static void ResetNextNiceCarPictureTimeMS() { m_uNextNiceCarPictureTimeMS = 0; }

	static const char* sm_FriendlyFireContextLines[];

private:

	static s32 PickEventResponseTaskTypeFromFlags(const CScenarioPoint& rPoint);
	static u32 m_uNextNiceCarPictureTimeMS;
	static const u32 sm_uMinMSBetweenNiceCarPictures = 5000;
};

#endif // INC_EVENT_RESPONSE_FACTORY_H
