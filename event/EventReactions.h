#ifndef INC_EVENT_REACTIONS_H
#define INC_EVENT_REACTIONS_H

// Game Headers
#include "Event/EventEditable.h"
#include "Event/EventPriorities.h"
#include "AI/AITarget.h"
#include "Scene/RegdRefTypes.h"

class CEventReactionEnemyPed : public CEvent
{
public:
	CEventReactionEnemyPed(CPed* pThreatPed, bool bForceThreatResponse = false);

	~CEventReactionEnemyPed() {}

	// Required event functions
	virtual CEvent* Clone(void) const			{ return rage_new CEventReactionEnemyPed(m_pThreatPed,m_bForceThreatResponse); }
	virtual int GetEventType(void) const		{ return EVENT_REACTION_ENEMY_PED; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual CPed* GetSourcePed() const			{ return m_pThreatPed; }
	virtual int GetEventPriority(void) const	{return E_PRIORITY_REACTION_ENEMY_PED; }

private:
	RegdPed		m_pThreatPed;
	bool		m_bForceThreatResponse;
};

class CEventReactionInvestigateThreat : public CEvent
{
public:
	
	enum 
	{
		LOW_THREAT,
		HIGH_THREAT
	};

	CEventReactionInvestigateThreat(const CAITarget& threatTarget, s32 iEventType, s32 iThreatType);
	~CEventReactionInvestigateThreat() {}

	s32 GetInitialEventType() const { return m_iEventType; }

	// Required event functions
	virtual CEvent* Clone(void) const			{ return rage_new CEventReactionInvestigateThreat(m_threatTarget,m_iEventType,m_iThreatType); }
	virtual int GetEventType(void) const		{ return EVENT_REACTION_INVESTIGATE_THREAT; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual int GetEventPriority(void) const 
	{
		if (m_iThreatType == LOW_THREAT)
		{
			return E_PRIORITY_REACTION_INVESTIGATE_LOW_THREAT;
		}
		else
		{
			return E_PRIORITY_REACTION_INVESTIGATE_HIGH_THREAT;
		}
	}

private:
	CAITarget		m_threatTarget;
	s32			m_iEventType;
	s32			m_iThreatType;
};

class CEventReactionInvestigateDeadPed : public CEvent
{
public:
	// TODO: Take just a position of the source threat as we do not know what is the cause of the threat yet?
	CEventReactionInvestigateDeadPed(CPed* pDeadPed) : m_pDeadPed(pDeadPed)
	{
#if !__FINAL
		m_EventType = EVENT_REACTION_INVESTIGATE_DEAD_PED;
#endif
	}
	~CEventReactionInvestigateDeadPed() {}

	// Required event functions
	virtual CEvent* Clone(void) const			{ return rage_new CEventReactionInvestigateDeadPed(m_pDeadPed); }
	virtual int GetEventType(void) const		{ return EVENT_REACTION_INVESTIGATE_DEAD_PED; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual CPed* GetSourcePed() const			{ return m_pDeadPed; }
	virtual int GetEventPriority(void) const	{ return E_PRIORITY_REACTION_INVESTIGATE_DEAD_PED; }

private:
	RegdPed		m_pDeadPed;
};

class CEventReactionCombatVictory : public CEvent
{
public:
	CEventReactionCombatVictory(CPed* pDeadPed) : m_pDeadPed(pDeadPed)
	{
#if !__FINAL
		m_EventType = EVENT_REACTION_COMBAT_VICTORY;
#endif
	}
	~CEventReactionCombatVictory() {}

	// Required event functions
	virtual CEvent*		Clone(void) const				{ return rage_new CEventReactionCombatVictory(m_pDeadPed); }
	virtual int			GetEventType(void) const		{ return EVENT_REACTION_COMBAT_VICTORY; }
	virtual bool		CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual CPed*		GetSourcePed() const			{ return m_pDeadPed; }
	virtual int			GetEventPriority(void) const	{ return E_PRIORITY_REACTION_COMBAT_VICTORY; }

private:
	RegdPed		m_pDeadPed;
};

class CEventWhistlingHeard : public CEvent
{
public:
	static bank_float ms_fRange;

	CEventWhistlingHeard(CPed* pPedHeardWhistling);
	~CEventWhistlingHeard() {}

	// Required event functions
	virtual CEvent*		Clone(void) const				{ return rage_new CEventWhistlingHeard(m_pPedHeardWhistling); }
	virtual int			GetEventType(void) const		{ return EVENT_WHISTLING_HEARD; }
	virtual bool		CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual CPed*		GetSourcePed() const			{ return m_pPedHeardWhistling; }
	virtual int			GetEventPriority(void) const	{ return E_PRIORITY_WHISTLING_HEARD; }
	virtual bool		AffectsPedCore(CPed* pPed) const;

private:
	RegdPed		m_pPedHeardWhistling;
};

class CEventExplosionHeard : public CEvent
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:	Square of the max distance between two explosion events for combining them.
		float	m_MaxCombineDistThresholdSquared;

		PAR_PARSABLE;
	};

	static bank_float ms_fMaxLoudness;

	CEventExplosionHeard(const Vector3 &vExplosionPosition, float fLoudness)
		:	m_fLoudness(fLoudness),
			m_vExplosionPosition(vExplosionPosition)
	{
#if !__FINAL
		m_EventType = EVENT_EXPLOSION_HEARD;
#endif
	}
	~CEventExplosionHeard() {}

	// Required event functions
	virtual CEvent*			Clone(void) const				{ return rage_new CEventExplosionHeard(m_vExplosionPosition,m_fLoudness); }
	virtual int				GetEventType(void) const		{ return EVENT_EXPLOSION_HEARD; }
	virtual bool			CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual const Vector3& 	GetSourcePos() const	{ return m_vExplosionPosition; }
	virtual int				GetEventPriority(void) const	{ return E_PRIORITY_EXPLOSION_HEARD; }
	virtual bool			AffectsPedCore(CPed* pPed) const;

	virtual VehicleEventPriority::VehicleEventPriority GetVehicleResponsePriority() const {return VehicleEventPriority::VEHICLE_EVENT_PRIORITY_RESPOND_TO_THREAT;}
	virtual CTask* CreateVehicleReponseTaskWithParams(sVehicleMissionParams& paramsOut) const;
	virtual bool AffectsVehicleCore(CVehicle* pVehicle) const;

	virtual bool IsExposedToVehicles() const {return true; }

	bool TryToCombine(const CEventExplosionHeard& other);

	virtual bool operator==( const fwEvent& otherEvent ) const;

private:
	bool		CanHearExplosion(const CEntity* pEntity) const;
	Vector3		m_vExplosionPosition;
	float		m_fLoudness;

	static Tunables	sm_Tunables;
};

class CEventDisturbance : public CEvent
{
public:

	// Disturbance types
	typedef enum 
	{
		ePedKilledByStealthKill = 0,
		ePedHeardFalling,
		ePedInNearbyCar
	} eDisturbanceType;

	CEventDisturbance(CPed* pedBeingDisturbed, CPed* pedDoingDisturbance, eDisturbanceType disturbanceType);

	~CEventDisturbance() {}

	// Required event functions
	virtual CEvent*			Clone(void) const				{ return rage_new CEventDisturbance(m_pPedBeingDisturbed,m_pPedDoingDisturbance,m_eDisturbanceType); }
	virtual int				GetEventType(void) const		{ return EVENT_DISTURBANCE; }
	virtual bool			CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual int				GetEventPriority(void) const	{ return E_PRIORITY_DISTURBANCE; }
	virtual bool			AffectsPedCore(CPed* pPed) const;
	eDisturbanceType		GetDisturbanceType()const  { return m_eDisturbanceType; }

private:
	RegdPed			m_pPedBeingDisturbed;
	RegdPed			m_pPedDoingDisturbance;
	eDisturbanceType	m_eDisturbanceType;

};

#if __BANK
#include "Debug/DebugDrawStore.h"

class CEventDebug
{
public:

#if DEBUG_DRAW
	static CDebugDrawStore ms_debugDraw;
#endif // DEBUG_DRAW

	static void InitWidgets();

	static void Debug();

	static void	DrawCircle( Vec3V_In vCentre, const float fRadius, const Color32 colour );

#if __DEV
	static void SpewPoolUsage();
#endif // __DEV

private:

	static bool ms_bRenderEventDebug;

};
#endif // __BANK

#endif // INC_EVENT_REACTIONS_H
