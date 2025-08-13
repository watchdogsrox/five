#ifndef INC_EVENT_WEAPON
#define INC_EVENT_WEAPON

// Game headers
#include "Event/EventEditable.h"
#include "Event/EventPriorities.h"
#include "scene/RegdRefTypes.h"

class CPed;

//////////////////////////////////////////////////////////////////////////
// CEventGunAimedAt
//////////////////////////////////////////////////////////////////////////

// Ped is aimed at with gun 
class CEventGunAimedAt : public CEventEditableResponse
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_MinDelayTimer;
		float	m_MaxDelayTimer;

		PAR_PARSABLE;
	};

	// PURPOSE: Checks if the target ped can see the ped aiming at him
	static bool IsAimingInTargetSeeingRange(const CPed& rTargetPed, const CPed& rAimingPed, const CWeaponInfo& rWeaponInfo);

	// Constants
	static const float INCREASED_PERCEPTION_DIST;
	static const float INCREASED_PERCEPTION_MAX_ANGLE;
	static const float MAX_PERCEPTION_DIST;
	static const float MAX_PERCEPTION_DIST_ANGLE;

	CEventGunAimedAt(CPed* pAggressorPed);
	virtual ~CEventGunAimedAt();

	virtual bool operator==(const fwEvent& otherEvent) const;

	virtual CEventEditableResponse* CloneEditable() const
	{
		CEventGunAimedAt* pClone = rage_new CEventGunAimedAt(m_pAggressorPed);
		return pClone;
	}

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_GUN_AIMED_AT; }
	virtual int GetEventPriority() const { return E_PRIORITY_GUN_AIMED_AT; }

	//
	//
	//
	
	virtual bool IsEventReady(CPed* pPed);

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;

	//
	// 
	//

	// Get the source of this event
	virtual CEntity* GetSourceEntity() const;

	// Crime
	virtual CPed* GetPlayerCriminal() const;

	// Allow scripts to query this event
	virtual bool IsExposedToScripts() const { return true; }

protected:

	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = sm_Tunables.m_MinDelayTimer; fMax = sm_Tunables.m_MaxDelayTimer; return true; }

	RegdPed m_pAggressorPed;

	static Tunables sm_Tunables;
};

//////////////////////////////////////////////////////////////////////////
// CEventGunShot
//////////////////////////////////////////////////////////////////////////

// Ped has witnessed a gunshot
class CEventGunShot : public CEventEditableResponse
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_MinDelayTimer;
		float	m_MaxDelayTimer;
		float	m_GunShotThresholdDistance;

		PAR_PARSABLE;
	};

	static Tunables sm_Tunables;

	static CPed* GetDriverOfFiringVehicleIfLeaderIsLocalPlayer(const CVehicle& rFiringVeh, const CPed& rPed);

	CEventGunShot(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, bool bSilent, u32 weaponHash, float maxSoundRange = LARGE_FLOAT);
	virtual ~CEventGunShot();

	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventGunShot(m_pFiringEntity, m_vShotOrigin, m_vShotTarget, m_bSilent, m_weaponHash, m_fMaxSoundRange); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_SHOT_FIRED; }
	virtual int GetEventPriority() const { return E_PRIORITY_SHOT_FIRED; }

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = sm_Tunables.m_MinDelayTimer; fMax = sm_Tunables.m_MaxDelayTimer; return true; }

	virtual bool IsValidForPed(CPed* pPed) const;
	virtual bool IsEventReady(CPed* pPed);

	// Used by certain events to make sure duplicate events aren't added
	virtual bool operator==(const fwEvent& otherEvent) const;

	// Can this event be interrupted by an event of the same type?
	// Usually an event won't replace one of the same type
	virtual bool CanBeInterruptedBySameEvent() const { return true; }

	//
	//
	//

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;

	//
	// 
	//

	// Get the source of this event
	virtual CEntity*	GetSourceEntity() const { return m_pFiringEntity; }
	virtual CPed*		GetSourcePed() const;

	//Return the shot origin.
	virtual const Vector3& GetSourcePos() const;

	// Crime
	virtual CPed* GetPlayerCriminal() const;

	virtual VehicleEventPriority::VehicleEventPriority GetVehicleResponsePriority() const {return VehicleEventPriority::VEHICLE_EVENT_PRIORITY_RESPOND_TO_THREAT;}
	virtual CTask* CreateVehicleReponseTaskWithParams(sVehicleMissionParams& paramsOut) const;
	virtual bool AffectsVehicleCore(CVehicle* pVehicle) const;

	virtual bool IsExposedToVehicles() const {return true; }

    // The gunshot target position
    const Vector3& GetShotTarget() const { return m_vShotTarget; }

    // Whether the gunshot was silenced
    bool IsSilent() const { return m_bSilent; }

	// Weapon type that caused the gun shot
	u32 GetWeaponHash() const { return m_weaponHash; }

	// Get/Set GunShot sense range
	static float GetGunShotSenseRangeForRiot2()            { return ms_fGunShotSenseRangeForRiot2; }
	static void SetGunShotSenseRangeForRiot2(float fRange) { ms_fGunShotSenseRangeForRiot2 = fRange; }

	// Get last time GunShot event was created
	static u32 GetLastTimeShotFired() { return ms_iLastTimeShotFired; }
	static void ResetLastTimeShotFired() { ms_iLastTimeShotFired = 0; }
	static bool WasShotFiredWithinTime(u32 timeMs);

protected:

	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;

	//
	// Members
	//

	RegdEnt     m_pFiringEntity;
	Vector3     m_vShotOrigin;
	Vector3     m_vShotTarget;
	bool        m_bSilent;
	bool        m_bFirstCheck;
	bool        m_bIsReady;
	bool        m_bIsJustReady;
	float		m_fMaxSoundRange;
	u32		m_weaponHash;

	// Global gun shot sense range
	static float ms_fGunShotSenseRangeForRiot2;

	// Store time last CEventGunShot was created
	static u32 ms_iLastTimeShotFired;
};

//////////////////////////////////////////////////////////////////////////
// CEventGunShotWhizzedBy
//////////////////////////////////////////////////////////////////////////

// Gun shot whizzed by ped head
class CEventGunShotWhizzedBy : public CEventGunShot
{
public:

	CEventGunShotWhizzedBy(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, const bool bIsSilent, u32 weaponHash, bool bFromNetwork = false);
	virtual ~CEventGunShotWhizzedBy() {}

	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventGunShotWhizzedBy(m_pFiringEntity, m_vShotOrigin, m_vShotTarget, m_bSilent, m_weaponHash, m_bFromNetwork); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_SHOT_FIRED_WHIZZED_BY; }
	virtual int GetEventPriority() const { return E_PRIORITY_SHOT_FIRED_WHIZZED_BY; }

	virtual bool AffectsVehicleCore(CVehicle* pVehicle) const;

	// Used by certain events to make sure duplicate events aren't added
	virtual bool operator==(const fwEvent& otherEvent) const;

	//
	//
	//

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;

	// Gets the closest whizzed by position
	bool IsBulletInRange(const Vector3& vTestPosition, Vec3V_InOut vClosestPosition) const;

private:

    bool m_bFromNetwork;
};

//////////////////////////////////////////////////////////////////////////
// CEventGunShotBulletImpact
//////////////////////////////////////////////////////////////////////////

// Generated when a bullet impacts on something, passed onto nearby peds
class CEventGunShotBulletImpact : public CEventGunShotWhizzedBy
{
public:

	CEventGunShotBulletImpact(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, const bool bIsSilent, u32 weaponHash, bool bMustBeSeenWhenInVehicle = false, bool fromNetwork = false);
	virtual ~CEventGunShotBulletImpact() {}

	virtual CEventEditableResponse* CloneEditable() const
	{
		CEventGunShotBulletImpact* pTask = rage_new CEventGunShotBulletImpact(m_pFiringEntity, m_vShotOrigin, m_vShotTarget, m_bSilent, m_weaponHash, m_bMustBeSeenWhenInVehicle, m_bFromNetwork);
		pTask->SetHitPed(m_pHitPed);

		return pTask;
	}

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_SHOT_FIRED_BULLET_IMPACT; }
	virtual int GetEventPriority() const { return E_PRIORITY_SHOT_FIRED_BULLET_IMPACT; }

	// Used by certain events to make sure duplicate events aren't added
	virtual bool operator==(const fwEvent& otherEvent) const;

    bool MustBeSeenWhenInVehicle() const { return m_bMustBeSeenWhenInVehicle; }

	//
	//
	//

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool AffectsVehicleCore(CVehicle* pVehicle) const;
	virtual bool IsEventReady(CPed* pPed);

	void SetHitPed(CPed* pPed) { m_pHitPed = pPed; }

private:

	RegdPed	m_pHitPed;
	bool	m_bMustBeSeenWhenInVehicle;
    bool    m_bFromNetwork;
};

//////////////////////////////////////////////////////////////////////////
// CEventMeleeAction
//////////////////////////////////////////////////////////////////////////

class CEventMeleeAction : public CEventEditableResponse
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float	m_MinDelayTimer;
		float	m_MaxDelayTimer;

		PAR_PARSABLE;
	};

public:

	// Constants
	static const float SENSE_RANGE;

	CEventMeleeAction(CEntity* pActingEntity, CEntity* pHitEntity, bool isOffensive);
	virtual ~CEventMeleeAction();

	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventMeleeAction(m_pActingEntity, m_pHitEntity, m_isOffensive); }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_MELEE_ACTION; }
	virtual int GetEventPriority() const { return E_PRIORITY_MELEE_ACTION; }

	// Can this event be interrupted by an event of the same type?
	// Usually an event won't replace one of the same type
	virtual bool CanBeInterruptedBySameEvent() const { return true; }

	//
	//
	//

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;

	//
	//
	//

	// Should the event generate a response task for the specific ped?
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	//
	// 
	//

	// Get the source of this event
	virtual CEntity* GetSourceEntity() const { return m_pActingEntity; }

	// Crime
	virtual CPed* GetPlayerCriminal() const;

	virtual bool operator==(const fwEvent& rEvent) const;

protected:

	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;

	bool GetDoesResponseToAttackOnLeader(const CPed* pPed) const;

private:

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const
	{
		fMin = sm_Tunables.m_MinDelayTimer;
		fMax = sm_Tunables.m_MaxDelayTimer;
		return true;
	}

private:

	RegdEnt m_pActingEntity;
	RegdEnt m_pHitEntity;
	bool     m_isOffensive;

private:

	static Tunables sm_Tunables;

};


//////////////////////////////////////////////////////////////////////////
// CEventFriendlyAimedAt
//////////////////////////////////////////////////////////////////////////
class CEventFriendlyAimedAt : public CEventGunAimedAt
{
public:
	CEventFriendlyAimedAt(CPed* pAggressorPed);
	virtual ~CEventFriendlyAimedAt() {}

	virtual CEventEditableResponse* CloneEditable() const
	{
		CEventFriendlyAimedAt* pClone = rage_new CEventFriendlyAimedAt(m_pAggressorPed);
		return pClone;
	}

	virtual bool IsTemporaryEvent() const { return true; }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_FRIENDLY_AIMED_AT; }
	virtual int GetEventPriority() const { return E_PRIORITY_FRIENDLY_AIMED_AT; }

	// Used by certain events to make sure duplicate events aren't added
	virtual bool operator==(const fwEvent& otherEvent) const;

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;

};

//////////////////////////////////////////////////////////////////////////
// CEventFriendlyFireNearMiss
//////////////////////////////////////////////////////////////////////////
// Friendly aimed at or nearly missed me with a gunshot
class CEventFriendlyFireNearMiss : public CEventGunShot
{
public:
	enum ENearMissType 
	{
		FNM_BULLET_IMPACT,
		FNM_BULLET_WHIZZED_BY
	};

	CEventFriendlyFireNearMiss(CEntity* pFiringEntity, const Vector3& vShotOrigin, const Vector3& vShotTarget, bool bSilent, u32 weaponHash, ENearMissType type);
	virtual ~CEventFriendlyFireNearMiss() {}

	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventFriendlyFireNearMiss(m_pFiringEntity, m_vShotOrigin, m_vShotTarget, m_bSilent, m_weaponHash, m_type); }

	virtual bool IsTemporaryEvent() const { return true; }

	//
	// Event querying
	//

	virtual int GetEventType() const { return EVENT_FRIENDLY_FIRE_NEAR_MISS; }
	virtual int GetEventPriority() const { return E_PRIORITY_SHOT_FRIENDLY_NEAR_MISS; }

	// Used by certain events to make sure duplicate events aren't added
	virtual bool operator==(const fwEvent& otherEvent) const;

	// Core implementation of affects ped
	virtual bool AffectsPedCore(CPed* pPed) const;

	virtual bool AffectsVehicleCore(CVehicle* UNUSED_PARAM(pVehicle)) const { return false; }
	virtual bool IsExposedToVehicles() const { return false; }

	// Returns if the bullet was close enough to be considered.
	bool IsBulletInRange(const CPed& rPed) const;

	ENearMissType m_type;
};

#endif // INC_EVENT_WEAPON
