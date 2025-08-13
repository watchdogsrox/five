#ifndef INC_EVENT_H
#define INC_EVENT_H

// Rage headers
#include "atl/string.h"
#include "fwtl/poolcriticalsection.h"
#include "vector/vector3.h"
#include "vectormath/vec3v.h"

// Framework headers
#include "ai/event/event.h"

// Game headers
#include "Event/EventDefines.h"
#include "Event/EventNames.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/Tuning.h"			// For CTuning, used by CEvent subclasses for metadata.
#include "atl/string.h"

// Forward declarations
class CEventResponse;
class CPed;
class CVehicle;
class CEntity;
class CPhysical;
struct sVehicleMissionParams;

namespace VehicleEventPriority
{
	typedef enum
	{
		VEHICLE_EVENT_PRIORITY_NONE,
		VEHICLE_EVENT_PRIORITY_RESPOND_TO_SIREN,
		VEHICLE_EVENT_PRIORITY_RESPOND_TO_THREAT
	} VehicleEventPriority;
};

#define CEventCriticalSection fwPoolCriticalSection<fwKnownRefHolder>

class CEvent : public aiEvent
{
public:
	// Event allocation is handled via an event pool
	FW_REGISTER_LOCKING_CLASS_POOL(CEvent, CEventCriticalSection);

	CEvent();
	virtual ~CEvent();

	static bool CanCreateEvent(u32 nFreeSpaces = 1);

#if !__NO_OUTPUT
	// Debug function to return event name as a string for error output
	virtual atString GetName() const { return atString(CEventNames::GetEventName(static_cast<eEventType>(GetEventType()))); }
	virtual atString GetDescription() const { return GetName(); }
#endif // !__FINAL
	
	virtual bool operator==(const fwEvent&) const { return false; }
		
	virtual bool RetrieveData(u8* UNUSED_PARAM(data), const unsigned UNUSED_PARAM(sizeOfData)) const { return true; }

	//
	// Event querying
	//

	virtual bool HasExpired() const
	{
		return (GetFlagForDeletion() || (!ShouldDelay() && aiEvent::HasExpired()));
	}

	virtual bool IsValidForPed(CPed* UNUSED_PARAM(pPed)) const { return !HasExpired(); }
	virtual bool IsEventReady(CPed* pPed);
	virtual bool IsTemporaryEvent() const { return false; }

	// Global events that have this set will be added to the event queues for peds that are processing events
	virtual bool IsExposedToPed(CPed *UNUSED_PARAM(pPed)) const { return true; }

	virtual bool IsExposedToVehicles() const {return false; }

	// Global events that have this set will be added to the script queue and accessed by the scripts
	virtual bool IsExposedToScripts() const { return false; }

	// True if this is a "shocking event", i.e. derived from CEventShocking.
	inline bool IsShockingEvent() const { return m_bIsShockingEvent; }
	inline void SetIsShockingEvent(bool bIsShocking) { m_bIsShockingEvent = bIsShocking; }
	virtual void DisableSenseChecks() {}

	// Returns true if this event will result in a rag dolling ped
	virtual bool RequiresAbortForRagdoll() const { return false; }

	// Returns true if this event will result in a melee reaction
	virtual bool RequiresAbortForMelee(const CPed* UNUSED_PARAM(pPed)) const {return false;}

	// Does this event use decision makers?
	virtual bool HasEditableResponse() const { return false; }

	// Can this event be interrupted by an event of the same type?
	// Usually an event won't replace one of the same type
	virtual bool CanBeInterruptedBySameEvent() const { return false; } 

	// Does this event have priority over the otherEvent?
	virtual bool HasPriority(const fwEvent& otherEvent) const;

	//
	//
	//

	// Should the event generate a response task for the specific ped?
	virtual bool ShouldCreateResponseTaskForPed(const CPed* UNUSED_PARAM(pPed)) const { return true; }

	// Query the event for the type of task to create
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_INVALID_ID; }

	virtual VehicleEventPriority::VehicleEventPriority GetVehicleResponsePriority() const {return VehicleEventPriority::VEHICLE_EVENT_PRIORITY_NONE;}

	virtual CTask* CreateVehicleReponseTaskWithParams(sVehicleMissionParams& paramsOut) const;

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	//
	// 
	//

	// Get the source of this event
	virtual CEntity* GetSourceEntity() const { return NULL; }

	// Get the source ped of this event
	virtual CPed* GetSourcePed() const;

	virtual const Vector3& GetSourcePos() const { return VEC3_ZERO; }

	virtual CEntity* GetTargetEntity() const { return GetSourceEntity(); }

	virtual CPed* GetTargetPed() const;

	// If the source entity is a valid pointer, return its position.
	// Otherwise, return the source position of the event.
	virtual Vec3V_Out GetEntityOrSourcePosition() const;

	//
	// Global queue (for "shocking events", etc)
	//

	virtual bool ProcessInGlobalGroup() { return false; }

	//
	//
	//

	// Checks both core and game specific
	bool AffectsPed(CPed* pPed) const;

	bool AffectsVehicle(CVehicle* pVehicle) const;

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* UNUSED_PARAM(pPed)) const { return true; }

	virtual bool AffectsVehicleCore(CVehicle* UNUSED_PARAM(pVehicle)) const { return false;	}

	virtual void ClearRagdollResponse() {}

	// Sends the event to all peds who can hear
	void CommunicateEvent(CPed* pPed, bool bInformRespectedPedsOnly = false, bool bUseRadioIfPossible = true) const;

	// Return true if an event centered at pSourceEntity (or vSourcePosition if the event has no associated source entity)
	// occured outdoors with the sensing ped indoors.
	static bool CanEventBeIgnoredBecauseOfInteriorStatus(const CPed& sensingPed, const CEntity* pSourceEntity, Vec3V_In vSourcePosition);
	static bool CanEventWithSourceBeIgnoredBecauseOfInteriorStatus(const CPed& sensingPed, const CEntity& sourceEntity);
	static bool CanEventWithNoSourceBeIgnoredBecauseOfInteriorStatus(const CPed& sensingPed, Vec3V_In vPos);
	
	struct CommunicateEventInput
	{
		CommunicateEventInput()
		: m_bInformRespectedPedsOnly(false)
		, m_bInformRespectedPedsDueToScenarios(false)
		, m_bInformRespectedPedsDueToSameVehicle(false)
		, m_bUseRadioIfPossible(true)
		, m_fMaxRangeWithoutRadio(50.0f)
		, m_fMaxRangeWithRadio(900.0f)
		{}
		
		bool	m_bInformRespectedPedsOnly;
		bool	m_bInformRespectedPedsDueToScenarios;
		bool	m_bInformRespectedPedsDueToSameVehicle;
		bool	m_bUseRadioIfPossible;
		float	m_fMaxRangeWithoutRadio;
		float	m_fMaxRangeWithRadio;
	};
	virtual void CommunicateEvent(CPed* pPed, const CommunicateEventInput& rInput) const;

	// Crime
	virtual CPed* GetPlayerCriminal() const { return NULL; }
	virtual void OnEventAdded(CPed* pPed) const;

	virtual bool CanDeleteActiveTask() const { return false; }

	float GetDelayTimer() const { return m_fDelayTimer; }
	void SetDelayTimer(float fValue) { m_fDelayTimer = fValue; }
	
#if !__FINAL
	// PURPOSE
	//  Verifies that we don't have any orphaned tasks
	static void VerifyEventCounts();
	static bool VerifyEventCountCallback(void* pItem, void* );
#else
	inline static void VerifyEventCounts() {}
#endif

	DEV_ONLY(float m_fInitialDelay;)

protected:
	virtual bool TakesPriorityOver(const CEvent& otherEvent) const { return fwEvent::HasPriority(otherEvent); }

	// PURPOSE:  Return true if this event should have staggered perception.
	// Sets the min and max values (in seconds) for how long this event should be delayed in the queue before being processed.
	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = 0.0f; fMax = 0.0f; return false; }
private:

	bool IsDelayInitialized() const
	{
		return m_bIsDelayInitialized;
	}

	bool ShouldDelay() const
	{
		return (m_fDelayTimer > 0.0f);
	}

private:

	void InitializeDelay(CPed* pPed);
	void UpdateDelay();


private:

	bool	m_bIsDelayInitialized : 1;
	bool	m_bIsShockingEvent : 1;
	float	m_fDelayTimer;

public:
#if __BANK
	static bool sm_bInPoolFullCallback;
#endif // __BANK
};

typedef	eventRegdRef<CEvent>									RegdEvent;
typedef	eventRegdRef<const CEvent>								RegdConstEvent;

#if __DEV
// Forward declare pool full callback so we don't get two versions of it
namespace rage { template<> void fwPool<CEvent>::PoolFullCallback(); }
#endif // __DEV

#endif // INC_EVENT_H
