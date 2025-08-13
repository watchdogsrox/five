#ifndef INC_EVENT_DECISION_MAKER_RESPONSE_H
#define INC_EVENT_DECISION_MAKER_RESPONSE_H

// Rage headers
#include "atl/array.h"
#include "fwtl/pool.h"
#include "vector/vector3.h"

// Game headers
#include "Event/Event.h"
#include "Event/System/EventData.h"
#include "Event/decision/EventResponseDecisionFlags.h"

// Forward declarations
class CEventEditableResponse;
class CPed;

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerResponse
//////////////////////////////////////////////////////////////////////////

class CEventDataResponse_Decision 
{
public:
	CEventDataResponse_Decision();

	bool IsResponseDecisionFlagSet(CEventResponseDecisionFlags::Flags flag) const					{ return EventResponseDecisionFlags.BitSet().IsSet(flag); }
	void SetResponseDecisionFlag(CEventResponseDecisionFlags::Flags flag)							{ EventResponseDecisionFlags.BitSet().Set(flag); }

	// PURPOSE:	Post-load callback to set TaskType from TaskRef.
	void PostLoadCB();

	rage::atHashString										TaskRef;				// Note: basically only used for loading, should be possible to get rid of.
	rage::f32												Chance_SourcePlayer;
	rage::f32												Chance_SourceFriend;
	rage::f32												Chance_SourceThreat;
	rage::f32												Chance_SourceOther;
	rage::f32												DistanceMinSq;
	rage::f32												DistanceMaxSq;
	rage::atHashString										SpeechHash;
	rage::s16/*eTaskType*/									TaskType;				// The eTaskType derived from TaskRef, set by PostLoadCB().
	CEventResponseDecisionFlags::FlagsBitSet				EventResponseDecisionFlags;
	rage::u8												EventResponseFleeFlags;
	PAR_SIMPLE_PARSABLE;
};



//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerResponse
//////////////////////////////////////////////////////////////////////////

class CEventDecisionMakerResponse
{
public:

	// Constructor
	CEventDecisionMakerResponse();

	// Destructor
	virtual ~CEventDecisionMakerResponse() {}

	const rage::atHashString & GetName() const								{ return m_Name; }

	// 
	virtual void MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, CEventResponseTheDecision &theDecision ) const;

	virtual eEventType GetEventType() const									{ return m_Event; }

	// Cooldown
	struct sCooldown
	{
		sCooldown() : Time(-1.0f), MaxDistance(-1.0f)  { /* ... */  }

		float Time;
		float MaxDistance;
		PAR_SIMPLE_PARSABLE;
	};

	const sCooldown& GetCooldown() const { return m_Cooldown; }

protected:

	typedef rage::atArray<CEventDataResponse_Decision> tDecisionArray;

	//
	void MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, const tDecisionArray & rDecisions, CEventResponseTheDecision &theDecision ) const;

	// 
	struct sDecisionResult
	{
		sDecisionResult() : fChance(0.0f), iTaskType(-1), uFleeFlags(0) {}

		// Chance of this decision being used
		float fChance;

		// The task result (really a CTaskTypes::eTaskType).
		s16/*eTaskType*/ iTaskType;

		rage::atHashString		SpeechHash;

		// Copy of all event decision maker response flags
		rage::u8	uFleeFlags;

	};

	// 
	float GetDecisionResult(const CPed* pPed, const CEventDataResponse_Decision& decision, EventSourceType eventSourceType, sDecisionResult& result, const CEvent& rEvent) const;

	bool DoesPedHaveGun(const CPed& rPed) const;

	//
	// Members
	//

	// Metadata
	rage::atHashString			m_Name;

	eEventType					m_Event;

	sCooldown					m_Cooldown;

	tDecisionArray				m_Decision;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerResponseDynamic
//////////////////////////////////////////////////////////////////////////

class CEventDecisionMakerResponseDynamic : public CEventDecisionMakerResponse
{
public:
	FW_REGISTER_CLASS_POOL(CEventDecisionMakerResponseDynamic);

	// The maximum we can allocate
	static const s32 MAX_STORAGE = 4;

	// Constructor
	CEventDecisionMakerResponseDynamic(eEventType eventType);

	// Copy constructor
	CEventDecisionMakerResponseDynamic(const CEventDecisionMakerResponseDynamic& other);

	// Destructor
	virtual ~CEventDecisionMakerResponseDynamic() {}

	// 
	virtual void MakeDecision(const CPed* pPed, const CEventEditableResponse* pEvent, EventSourceType eventSourceType, CEventResponseTheDecision &theDecision ) const;

	// 
	virtual eEventType GetEventType() const { return m_eventType != EVENT_INVALID ? m_eventType : CEventDecisionMakerResponse::GetEventType(); }

	//
	bool AddDecision(const CEventDataResponse_Decision& decision);

	//
	void ClearDecisions();

private:

	//
	// Members
	//

	// Event type
	eEventType m_eventType;

	// Storage for user defined decisions
	typedef atArray<CEventDataResponse_Decision> Decisions;
	Decisions m_decisions;
};

#endif // INC_DECISION_EVENT_RESPONSE_H
