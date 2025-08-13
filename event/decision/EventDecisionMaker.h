#ifndef INC_EVENT_DECISION_MAKER_H
#define INC_EVENT_DECISION_MAKER_H

// Rage headers
#include "atl/array.h"
#include "fwtl/pool.h"

// Game headers
#include "Event/System/EventData.h"

// Forward declarations
class CEventDataDecisionMaker;
class CEventDataResponse_Decision;
class CEventDecisionMakerResponseDynamic;
class CEventEditableResponse;
class CPed;

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerModifiableComponent
// Dynamically allocated/modified data at runtime
//////////////////////////////////////////////////////////////////////////

class CEventDecisionMakerModifiableComponent
{
public:
	FW_REGISTER_CLASS_POOL(CEventDecisionMakerModifiableComponent);

	// The maximum we can allocate
	static const s32 MAX_STORAGE = 3;

	// Constructor
	CEventDecisionMakerModifiableComponent();

	// Copy constructor
	CEventDecisionMakerModifiableComponent(const CEventDecisionMakerModifiableComponent& other);

	// Destructor
	~CEventDecisionMakerModifiableComponent();

	//
	void MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, CEventResponseTheDecision &decision ) const;

	//
	void AddResponse(eEventType eventType, const CEventDataResponse_Decision& decision);

	//
	void ClearResponse(eEventType eventType);

	// 
	void ClearAllResponses();

	//
	void BlockEvent(eEventType eventType);

	// 
	void UnblockEvent(eEventType eventType);

	//
	void UnblockAllEvents() { m_blockedEvents.Reset(); }

	// 
	bool GetIsEventBlocked(eEventType eventType) const;

#if !__FINAL
	// Get the name
	const char* GetName() const { return m_name; }

	// Set the name
	void SetName(const char* pName) { strncpy(m_name, pName, 32); }
#endif // !__FINAL

private:

	//
	// Members
	//

	// Storage for user defined responses
	static const s32 MAX_RESPONSES = 4;
	typedef atFixedArray<CEventDecisionMakerResponseDynamic*, MAX_RESPONSES> Responses;
	Responses m_responses;

	// Storage for blocked events
	static const s32 MAX_BLOCKED_EVENTS = 4;
	typedef atFixedArray<eEventType, MAX_BLOCKED_EVENTS> BlockedEvents;
	BlockedEvents m_blockedEvents;

#if !__FINAL
	// String name of cloned component
	char m_name[32];
#endif // !__FINAL
};

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMaker
//////////////////////////////////////////////////////////////////////////

class CEventDecisionMaker
{
public:
	FW_REGISTER_CLASS_POOL(CEventDecisionMaker);

	// The maximum we can allocate
	static const s32 MAX_STORAGE = 25;

	// Constructor
	CEventDecisionMaker(u32 uId, const CEventDataDecisionMaker* pData);

	// Destructor
	~CEventDecisionMaker();

	// 
	u32 GetId() const { return m_uId; }

	// 
	void MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, CEventResponseTheDecision &theDecision) const;

	//
	//
	//

	// 
	CEventDecisionMaker* Clone(const char* pNewName) const;

	//
	void AddResponse(eEventType eventType, const CEventDataResponse_Decision& decision);

	//
	void ClearResponse(eEventType eventType) { if(m_pDecisionMakerComponent) m_pDecisionMakerComponent->ClearResponse(eventType); }

	//
	void ClearAllResponses() { if(m_pDecisionMakerComponent) m_pDecisionMakerComponent->ClearAllResponses(); }

	//
	void BlockEvent(eEventType eventType);

	//
	void UnblockEvent(eEventType eventType) { if(m_pDecisionMakerComponent) m_pDecisionMakerComponent->UnblockEvent(eventType); }

	//
	void UnblockAllEvents() { if(m_pDecisionMakerComponent) m_pDecisionMakerComponent->UnblockAllEvents(); }

	// returns true if this decision maker handles the event of a given type (specified in EventData.h)
	bool HandlesEventOfType(int iEventType) const;

#if !__FINAL
	// Get the decision maker name
	const char* GetName() const { return m_pDecisionMakerComponent && m_pDecisionMakerComponent->GetName() ? m_pDecisionMakerComponent->GetName() : m_pName; }
#endif // !__FINAL

private:

	//
	EventSourceType GetEventSourceType(const CPed* pPed, const CEventEditableResponse* pEvent) const;

	// 
	void MakeDecisionFromMetadata(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, const CEventDataDecisionMaker* pDecisionMakerData, CEventResponseTheDecision &theDecision ) const;

	// 
	bool CreateComponent();

	// 
	void DestroyComponent();

	//
	// Members
	//

	// The id
	u32 m_uId;

	// Pointer to the metadata
	const CEventDataDecisionMaker* m_pDecisionMakerData;

	// Pointer to any dynamic component
	CEventDecisionMakerModifiableComponent* m_pDecisionMakerComponent;

#if !__FINAL
	// String name for easier debugging
	const char* m_pName;
#endif // !__FINAL
};

#endif // INC_EVENT_DECISION_MAKER_H
