#ifndef INC_EVENT_EDITABLE_H
#define INC_EVENT_EDITABLE_H

// Game headers
#include "Event/Event.h"

// Forward declarations
class CPed;
class CWeightedTaskSet;

class CEventEditableResponse : public CEvent
{
public:

	CEventEditableResponse();
	virtual ~CEventEditableResponse() {}

	virtual CEvent* Clone() const;
	virtual CEventEditableResponse* CloneEditable() const = 0;

	virtual bool HasEditableResponse() const { return true; }

	//
	// Decision making
	//

	// Use the decision maker to decide what our response task type will be
	// Sets m_iTaskType
	void ComputeResponseTaskType(CPed* pPed);

	// Do we have a response task?
	bool HasCalculatedResponse() const { return m_iTaskType != CTaskTypes::TASK_INVALID_ID; }
	bool WillRespond() const;

	// Get/Set the response task type
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return m_iTaskType; }
	void SetResponseTaskType(const CTaskTypes::eTaskType iTaskType) { m_iTaskType = iTaskType; }

	// Returns whether the event was witnessed by this ped, false if the event
	// was passed onto this ped by a friendly ped
	bool GetWitnessedFirstHand() const { return m_bWitnessedFirstHand; }

	// PURPOSE:	Given the name of a decision-maker response (such as "RESPONSE_TASK_GUN_AIMED_AT"),
	//			determine the task type of the response.
	// NOTES:	This effectively used to be done from ComputeResponseTaskType(), but it's done at load time now.
	static CTaskTypes::eTaskType FindTaskTypeForResponse(u32 taskHash);

	int GetFleeFlags() const { return static_cast<int>(m_FleeFlags); }

	u32 GetSpeechHash() { return m_SpeechHash;}

protected:

	// Helper clone function
	CEventEditableResponse* CloneWithDefaultParams() const;

	//
	// Members
	//

	// Our computed response type type
	CTaskTypes::eTaskType m_iTaskType;
	atHashString m_SpeechHash;

	// Flee flags from our computed response type type
	u8 m_FleeFlags;
	// Has ped witnessed the event, or told about it
	bool m_bWitnessedFirstHand : 1;
};

#endif // INC_EVENT_EDITABLE_H
