//
// event/EventShocking.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef EVENT_EVENT_SHOCKING_H
#define EVENT_EVENT_SHOCKING_H

#include "event/EventEditable.h"
#include "event/EventPriorities.h"
#include "scene/RegdRefTypes.h"
#include "task/System/TaskHelpers.h"

class CEventShocking : public CEventEditableResponse
{
public:

	// Keep these in synch with the corresponding enum in EventShocking.psc.
	enum ShockingEventReactionMode
	{
		NO_REACTION=0,
		SMALL_REACTION,
		BIG_REACTION
	};

protected:
	// Protected accessor for now, to make sure the proper subclasses get used.
	explicit CEventShocking();

public:
	class Tunables : public CTuning
	{
	public:
		Tunables() : CTuning(NULL, 0, NULL, NULL) {}
		Tunables(const char* pFilename, const u32 uHash, const char* pGroupName, const char* pBankName)
				: CTuning(pFilename, uHash, pGroupName, pBankName, false, true)
		{}

		// PURPOSE:	Life time for this event (in seconds), if not controlled by special rules.
		float	m_LifeTime;

		// PURPOSE:	Range at which this event can be visually observed (in meters).
		float	m_VisualReactionRange;

		// PURPOSE:	Range at which this event can be visually observed by a cop in a vehicle(in meters).
		float	m_CopInVehicleVisualReactionRange;

		// PURPOSE:	Range at which this event can be heard (in meters).
		float	m_AudioReactionRange;

		// PURPOSE:  Factor that this event, if it does not involve the player, will have its perception distances scaled by.  -1 means to not scale.
		float	m_AIOnlyReactionRangeScaleFactor;

		// PURPOSE:	This event may trigger a duck-and-cover reaction if the player presses the right button within this time from the event start.
		float	m_DuckAndCoverCanTriggerForPlayerTime;

		// PURPOSE:	If approaching to watch, this is the range to stand and watch at (in meters).
		float	m_GotoWatchRange;

		// PURPOSE:  The range used to determine if we are close enough to keep watching a shocking event (in meters).
		float	m_StopWatchDistance;

		// PURPOSE:  If hurrying away from this event, this is how long to wait in seconds before changing the MBR.
		float	m_HurryAwayMBRChangeDelay;

		// PURPOSE:  If hurrying away from this event, this is the distance threshold for determining if the MBR should be changed.
		float	m_HurryAwayMBRChangeRange;

		// PURPOSE:  If hurrying away from this event, this is the move blend ratio to start the navmesh task with.
		float	m_HurryAwayInitialMBR;

		// PURPOSE:	If hurrying away from this event and not within 10 meters, this is the speed (move blend ratio) to use.
		float	m_HurryAwayMoveBlendRatioWhenFar;

		// PURPOSE:	If hurrying away from this event and within 10 meters of it, this is the speed (move blend ratio) to use.
		float	m_HurryAwayMoveBlendRatioWhenNear;

		// PURPOSE:	Min time (in seconds) to watch this event, if deciding to watch.
		float	m_MinWatchTime;

		// PURPOSE:	Max time (in seconds) to watch this event, if deciding to watch.
		float	m_MaxWatchTime;

		// PURPOSE:	Min time (in seconds) to watch this event, if deciding to watch and hurrying away afterwards.
		float m_MinWatchTimeHurryAway;

		// PURPOSE:	Max time (in seconds) to watch this event, if deciding to watch and hurrying away afterwards.
		float m_MaxWatchTimeHurryAway;

		// PURPOSE:	Max time (in seconds) to watch this event, if deciding to watch.
		float	m_ChanceOfWatchRatherThanHurryAway;

		// PURPOSE: Min time (in seconds) to film this event with a phone, if deciding to.
		float m_MinPhoneFilmTime;

		// PURPOSE: Max time (in seconds) to film this event with a phone, if deciding to.
		float m_MaxPhoneFilmTime;

		// PURPOSE: Min time (in seconds) to film this event with a phone, if deciding to and hurrying away afterwards.
		float m_MinPhoneFilmTimeHurryAway;

		// PURPOSE: Max time (in seconds) to film this event with a phone, if deciding to and hurrying away afterwards.
		float m_MaxPhoneFilmTimeHurryAway;

		// PURPOSE: If the response is to watch the event, the chance the ped will pull out a phone and film it.
		float m_ChanceOfFilmingEventOnPhoneIfWatching;

		// PURPOSE:	If m_AddPedGenBlockedArea is set, this is a lower limit to how small the
		//			blocking area can get. Used to make sure we prevent spawning within a large
		//			enough area during gunfights, etc.
		float	m_PedGenBlockedAreaMinRadius;

		// PURPOSE:	Radius of influence sphere around this event, affecting wandering peds.
		float	m_WanderInfluenceSphereRadius;
		
		// PURPOSE: Chances of triggering an ambient reaction.
		float	m_TriggerAmbientReactionChances;
		
		// PURPOSE: Min distance for the ambient reaction.
		float	m_MinDistanceForAmbientReaction;
		
		// PURPOSE: Max distance for the ambient reaction.
		float	m_MaxDistanceForAmbientReaction;

		// PURPOSE: Min time until the ambient event grows stale.
		float	m_AmbientEventLifetime;
		
		// PURPOSE: Min time to play the ambient reaction.
		float	m_MinTimeForAmbientReaction;
		
		// PURPOSE: Max time to play the ambient reaction.
		float	m_MaxTimeForAmbientReaction;

		// PURPOSE: How much (bound to [-1, 1]) this event should impact the fear motivation of a ped that responds to it.
		float	m_PedFearImpact;

		// PURPOSE:	Chance of saying the shocking speech hash when reacting to this event with certain tasks.
		float	m_ShockingSpeechChance;

		// PURPOSE: Minimum delay for peds to react to this event
		float	m_MinDelayTimer;

		// PURPOSE: Maximum delay for peds to react to this event
		float	m_MaxDelayTimer;

		// PURPOSE:  How far away between react positions is sufficient to call a non-entity shocking event of the same type as unique.
		float	m_DuplicateDistanceCheck;

		// PURPOSE:  How long after an event occurs can we play an audio reaction.
		float	m_MaxTimeForAudioReaction;

		// PURPOSE:  When ReactAndFlee is run as a response task to this event, use the gunfire set of animations if the distance is smaller than this value (-1 means never).
		float m_DistanceToUseGunfireReactAndFleeAnimations;

		// PURPOSE: Time in milleseconds to create a blocking area for if m_AddPedGenBlockedArea is set.
		u32 m_PedGenBlockingAreaLifeTimeMS;

		// PURPOSE:  How long between start times is sufficient to call an event of the same type as unique.
		u32		m_DuplicateTimeCheck;

		// PURPOSE:  What to say when a shocking event reaction starts to this event.
		atHashWithStringNotFinal m_ShockingSpeechHash;

		// PURPOSE: What to say when a shocking event reaction films the event on a phone
		atHashWithStringNotFinal m_ShockingFilmSpeechHash;

		// PURPOSE:	Priority value used if the global group fills up, and for prioritizing reactions.
		// NOTES:	This currently corresponds to eSEventShockLevels, but it would be better if the parser knew about eEventPriorityList.
		int		m_Priority;

		// PURPOSE:  Which kind of AmbientEvent this shocking event can create.
		AmbientEventType m_AmbientEventType;

		// PURPOSE:	If true, ped spawning will be blocked near the event.
		bool	m_AddPedGenBlockedArea;

		// PURPOSE: If true, vehicles will steer to avoid this event.
		bool	m_CausesVehicleAvoidance;

		// PURPOSE:	If true, this event will be ignored if SET_IGNORE_LOW_PRIORITY_SHOCKING_EVENTS is active.
		bool	m_AllowIgnoreAsLowPriority;

		// PURPOSE:	If true, when debug displaying the event, always use the position stored in the event, rather than the position of the source entity if available.
		bool	m_DebugDisplayAlwaysUseEventPosition;

		// PURPOSE:	If true, display PLAYER in the shocking event debug display list, if the source is the player.
		bool	m_DebugDisplayListPlayerInfo;

		// PURPOSE:	If hurrying away from this event, the ped may watch it briefly first if this is set.
		bool	m_HurryAwayWatchFirst;

		// PURPOSE:	If a ped is running CTaskMobileChatScenario and receives this event, this must be true for the scenario to abort.
		bool	m_MobileChatScenarioMayAbort;

		// PURPOSE:	When watching this event, if this is set, fight cheers may be said.
		bool	m_WatchSayFightCheers;

		// PURPOSE:	If true, when watching this event, speech like SHOCKED and SURPRISED may be generated.
		bool	m_WatchSayShocked;
		
		// PURPOSE:	If true, and in a vehicle, slow down the vehicle when near the event.
		bool	m_VehicleSlowDown;

		// PURPOSE: Peds should ignore this event if they are the "other" entity.
		bool	m_IgnoreIfSensingPedIsOtherEntity;

		// PURPOSE: Peds should ignore the default behavior in CTaskShockingEventHurryAway of always resuming if there is not pavement.
		bool	m_IgnorePavementChecks;

		// PURPOSE: This event can be added to the ped's event queue even if they have previously reacted to it.
		bool	m_AllowScanningEvenIfPreviouslyReacted;

		// PURPOSE:  Determines the intensity of the animated response to the shocking event.
		CEventShocking::ShockingEventReactionMode	m_ReactionMode;

		// PURPOSE:  If true, then the response task should try and quit if the event has expired.
		bool	m_StopResponseWhenExpired;

		// PURPOSE: If true, then the ped will flee if the m_pEntityOther gets too close.
		bool	m_FleeIfApproachedByOtherEntity;

		// PURPOSE: If true, then the ped will flee if the m_pEntitySource gets too close.
		bool	m_FleeIfApproachedBySourceEntity;

		bool	m_CanCallPolice;

		bool	m_IgnoreFovForHeadIk;

		bool	m_ReactToOtherEntity;

		PAR_PARSABLE;
	};

	virtual bool		ProcessInGlobalGroup();
	virtual void		ApplySpecialBehaviors();
	virtual bool		CheckHasExpired() const;

	virtual int			GetEventPriority() const {return m_Priority;}
	virtual bool		AffectsPedCore(CPed* pPed) const;
	virtual bool		IsExposedToPed(CPed *pPed) const;
	virtual bool		IsExposedToScripts() const;

	virtual CEntity*	GetSourceEntity() const { return m_pEntitySource; }
	virtual const Vector3& GetSourcePos() const { return RCC_VECTOR3(m_vEventPosition); }

	virtual const Tunables&	GetTunables() const = 0;

	void Set(Vec3V_In posV, CEntity* pSource, CEntity* pOther);
	void InitClone(CEventShocking& clone, bool cloneId = true) const;

	void UpdateLifeTime(u32 nLifeTime, u32 nCurrentTime);
	void RemoveEntities();

	bool GetUseSourcePosition() const
	{	return m_bUseSourcePosition;	}

	void SetUseSourcePosition(bool val)
	{	m_bUseSourcePosition = val; }

	CEntity* GetOtherEntity() const
	{	return m_pEntityOther;	}

	const Vector3& GetSourcePosition() const
	{	return RCC_VECTOR3(m_vEventPosition);	}

	void GetEntityOrSourcePosition(Vector3& v) const
	{	v = VEC3V_TO_VECTOR3(GetEntityOrSourcePosition());	}

	virtual Vec3V_Out GetEntityOrSourcePosition() const;

	void SetSourcePosition(Vec3V_In pos)
	{	m_vEventPosition = pos;	}

	virtual void DisableSenseChecks() { m_bDisableSenseChecks = true; } 
	bool CanBeSensedBy(const CPed& ped, float rangeMultiplier = 1.0f, bool bIgnoreDir = false) const;

	// The range override values allow the user to restrict this shocking event to different perception parameters other than the tunable values.
	static bool CanSense(const CPed& ped, const Tunables& tunables, Vector3::Vector3Param vecEventPos, bool bAudio = true, bool bVisual = true, float fRangeMult = 1.0f, bool bIgnoreDir = false, float fVisualSenseRangeOverride=-1.0f, float fAudioSenseRangeOverride=-1.0f);

	bool SanityCheckEntities() const { return (m_bHasSourceEntity && m_pEntitySource) || (!m_bHasSourceEntity && !m_pEntitySource && RCC_VECTOR3(m_vEventPosition).Mag2() > 0.05f); }

	u32 GetId() const { return m_iId; }
	void SetId(u32 id) { m_iId = id; }

	u32 GetStartTime() const { return m_iStartTime; }

	void SetCreatedByScript(bool b) { m_CreatedByScript = b; }
	bool GetCreatedByScript() const { return m_CreatedByScript; }

	bool GetShouldVehicleSlowDown() const { return GetTunables().m_VehicleSlowDown; }

	virtual bool CanBeInterruptedBySameEvent() const {return true;}

	//  Returns the position of the event, or if this explosion was merged from previous explosions, the most recently exploded instance
	Vec3V_Out GetReactPosition() const;

	// Construct a task in response to this event
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	// Enable to the shocking event to have different perception ranges than the tuning file specifies.
	void SetVisualReactionRangeOverride(float fVisualReactionRange) { m_fVisualReactionRangeOverride = fVisualReactionRange; }
	void SetAudioReactionRangeOverride(float fAudioReactionRange) { m_fAudioReactionRangeOverride = fAudioReactionRange; }

	virtual bool operator==(const fwEvent& rEvent) const;

	virtual atHashWithStringNotFinal GetShockingSpeechHash() const;
	virtual atHashWithStringNotFinal GetShockingFilmSpeechHash() const;
	void SetShouldUseGenericAudio(bool bUseGeneric) { m_bUseGenericAudio = bUseGeneric; }

	// Currently the same as what the pedgen blocking areas for shocking events use.  This does not necessarily need to be the case, it just
	// seemed safer not to change what was already in game.
	float GetVehicleAvoidanceRadius() const { return Max(m_fEventRadius, GetTunables().m_PedGenBlockedAreaMinRadius); }

	void SetIgnoreIfLaw(bool bIgnoreIfLaw) { m_bIgnoreIfLaw = bIgnoreIfLaw; }

	void SetCanBeMerged(bool bAllowMerge)	{ m_bCanBeMerged = bAllowMerge; }
	bool GetCanBeMerged() const				{ return m_bCanBeMerged; }

public:

	// Static helpers.

	static bool	ReadyForShockingEvents(const CPed& rPed);

	static bool DoesEventInvolvePlayer(const CEntity* pSource, const CEntity* pInflictor);

	static bool IsEntityRelatedToPlayer(const CEntity* pEntity);

	static bool ShouldUseGunfireReactsWhenFleeing(const CEventShocking& rEvent, const CPed& rRespondingPed);

private:

	virtual void ApplyMotivationalImpact(CPed& rPed) const;

	bool ShouldTriggerAmbientReaction(const CPed& rPed) const;

protected:
	friend class CShockingEventsManager;

	u32				m_iStartTime;			// time at which this event was created
	u32				m_iLifeTime;			// in milliseconds, 0 means until m_pEntitySource is no-more

	Vec3V			m_vEventPosition;		// some events can be defined by a position rather than entity
	Vec3V			m_vReactPosition;		// where peds should react from, see GetReactPosition
	float			m_fEventRadius;			// some events, ie explosions, gun fights, need an exclusion zone for peds		
	RegdEnt			m_pEntitySource;		// every shocking event has at least one source entity
	RegdEnt			m_pEntityOther;			// some Shock events have a 2nd

	s32 m_iPedGenBlockingAreaIndex;

	eEventPriorityList	m_Priority;			// priority to use (E_PRIORITY_SHOCKING_EVENT_INTERESTING, E_PRIORITY_SHOCKING_EVENT_SERIOUS_DANGER, etc)

	// PURPOSE:	ID number assigned to the event.
	u32				m_iId;

	// If >= 0, this is the visual reaction range to use for this shocking event instead of the tunable value.
	float			m_fVisualReactionRangeOverride;

	// If >= 0, this is the audio reaction range to use for this shocking event instead of the tunable value.
	float			m_fAudioReactionRangeOverride;

	u16				m_nCountAdds;			// count how many times this event has been added to

	bool			m_bHasSourceEntity : 1;
	bool			m_bUseSourcePosition : 1;

	// PURPOSE:	True if this event was explicitly created by a script command.
	bool			m_CreatedByScript : 1;

	// PURPOSE:  Whether this explosion event was merged with other explosions events
	bool			m_bHasReactPosition : 1;

	// PURPOSE: Whether this event has had its motivational impacts applied already.
	mutable bool	m_bHasAppliedMotivationalImpacts : 1;

	// PURPOSE:  Fall back to using generic audio.
	bool			m_bUseGenericAudio : 1;

	bool			m_bIgnoreIfLaw : 1;

	bool			m_bCanBeMerged : 1;

	bool			m_bDisableSenseChecks : 1;

	fwFlags8 m_DerivedClassFlags; // Flags reserved for use in derived event classes.

	// PURPOSE:	Counter used for assignment of ID numbers.
	static u32		sm_IdCounter;
};

#define DECLARE_SHOCKING_EVENT_TUNABLES(x)													\
	public:																					\
		virtual const Tunables&	GetTunables() const											\
		{ return sm_Tunables; }																\
	protected:																				\
		static Tunables	sm_Tunables;														\
		virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = sm_Tunables.m_MinDelayTimer; fMax = sm_Tunables.m_MaxDelayTimer; return true; }	\

class CEventShockingBrokenGlass : public CEventShocking
{
public:
	explicit CEventShockingBrokenGlass()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_BROKEN_GLASS;
#endif
	}

	explicit CEventShockingBrokenGlass(CEntity& rSmasher, Vec3V_In vPos)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_BROKEN_GLASS;
#endif
		Set(vPos, &rSmasher, nullptr);
		SetUseSourcePosition(true);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{
		CEventShocking* dest = rage_new CEventShockingBrokenGlass;
		InitClone(*dest);
		dest->SetUseSourcePosition(GetUseSourcePosition());
		return dest;
	}

	virtual int GetEventType() const
	{
		return EVENT_SHOCKING_BROKEN_GLASS;
	}

	virtual bool AffectsPedCore(CPed* pPed) const;

	virtual Vec3V_Out GetEntityorSourcePosition() const { return m_vEventPosition; }

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingBrokenGlass);
};

class CEventShockingCarChase : public CEventShocking
{
public:
	explicit CEventShockingCarChase()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_CHASE;
#endif
	}

	explicit CEventShockingCarChase(CEntity& chaser)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_CHASE;
#endif
		Set(chaser.GetTransform().GetPosition(), &chaser, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingCarChase;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_CAR_CHASE;	}

	// Return the driver if it exists otherwise return null.
	virtual CPed* GetSourcePed() const;

	// Return the driver if it exists otherwise return the vehicle.
	virtual CEntity* GetSourceEntity() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingCarChase);
};


class CEventShockingCarCrash : public CEventShocking
{
public:
	explicit CEventShockingCarCrash()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_CRASH;
#endif
	}

	explicit CEventShockingCarCrash(CEntity& crashedEntity, CEntity* pInflictor)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_CRASH;
#endif
		Set(crashedEntity.GetTransform().GetPosition(), &crashedEntity, pInflictor);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingCarCrash;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_CAR_CRASH;	}

	// Return the driver if it exists otherwise return null.
	virtual CPed* GetSourcePed() const;

	// Return the driver if it exists otherwise return the vehicle.
	virtual CEntity* GetSourceEntity() const;

	bool AffectsPedCore(CPed* pPed) const;

	virtual atHashWithStringNotFinal GetShockingSpeechHash() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingCarCrash);
};


class CEventShockingBicycleCrash : public CEventShocking
{
public:
	explicit CEventShockingBicycleCrash()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_BICYCLE_CRASH;
#endif
	}

	explicit CEventShockingBicycleCrash(CEntity& crashedEntity, CEntity* pInflictor)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_BICYCLE_CRASH;
#endif
		Set(crashedEntity.GetTransform().GetPosition(), &crashedEntity, pInflictor);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingBicycleCrash;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_BICYCLE_CRASH;	}

	// Return the driver if it exists otherwise return null.
	virtual CPed* GetSourcePed() const;

	// Return the driver if it exists otherwise return the vehicle.
	virtual CEntity* GetSourceEntity() const;

	bool AffectsPedCore(CPed* pPed) const;

	virtual atHashWithStringNotFinal GetShockingSpeechHash() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingBicycleCrash);
};


class CEventShockingCarPileUp : public CEventShocking
{
public:
	explicit CEventShockingCarPileUp()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_PILE_UP;
#endif
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingCarPileUp;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_CAR_PILE_UP;	}

	virtual bool						AffectsPedCore(CPed* pPed) const;
	virtual atHashWithStringNotFinal	GetShockingSpeechHash() const;

	// Return the driver if it exists otherwise return null.
	virtual CPed* GetSourcePed() const;

	// Return the driver if it exists otherwise return the vehicle.
	virtual CEntity* GetSourceEntity() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingCarPileUp);
};


class CEventShockingCarOnCar : public CEventShocking
{
public:

	explicit CEventShockingCarOnCar()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_ON_CAR;
#endif
	}

	explicit CEventShockingCarOnCar(CEntity& driver, CEntity* vehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_ON_CAR;
#endif
		Set(driver.GetTransform().GetPosition(), &driver, vehicle);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingCarOnCar;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_CAR_ON_CAR;	}

	bool AffectsPedCore(CPed* pPed) const;

	virtual atHashWithStringNotFinal GetShockingSpeechHash() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingCarOnCar);
};


class CEventShockingDeadBody : public CEventShocking
{
public:
	explicit CEventShockingDeadBody()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_DEAD_BODY;
#endif
	}

	explicit CEventShockingDeadBody(CEntity& deadBody)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_DEAD_BODY;
#endif
		Set(deadBody.GetTransform().GetPosition(), &deadBody, NULL);
	}

	virtual bool CheckHasExpired() const;

	virtual bool AffectsPedCore(CPed* pPed) const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingDeadBody;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_DEAD_BODY;	}

protected:

	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingDeadBody);
};


class CEventShockingDrivingOnPavement : public CEventShocking
{
public:
	explicit CEventShockingDrivingOnPavement()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_DRIVING_ON_PAVEMENT;
#endif
	}

	explicit CEventShockingDrivingOnPavement(CEntity& vehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_DRIVING_ON_PAVEMENT;
#endif
		Set(vehicle.GetTransform().GetPosition(), &vehicle, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingDrivingOnPavement;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_DRIVING_ON_PAVEMENT;	}

	virtual bool	AffectsPedCore(CPed* pPed) const;
	virtual CPed*	GetSourcePed() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingDrivingOnPavement);
};

class CEventShockingBicycleOnPavement : public CEventShocking
{
public:
	explicit CEventShockingBicycleOnPavement()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_BICYCLE_ON_PAVEMENT;
#endif
	}

	explicit CEventShockingBicycleOnPavement(CEntity& vehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_BICYCLE_ON_PAVEMENT;
#endif
		Set(vehicle.GetTransform().GetPosition(), &vehicle, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{
		CEventShocking* dest = rage_new CEventShockingBicycleOnPavement;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_BICYCLE_ON_PAVEMENT;	}

	virtual bool	AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingBicycleOnPavement);
};

class CEventShockingExplosion : public CEventShocking
{
public:
	explicit CEventShockingExplosion()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_EXPLOSION;
#endif
	}

	explicit CEventShockingExplosion(CEntity& explosionOwner)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_EXPLOSION;
#endif
		Set(explosionOwner.GetTransform().GetPosition(), &explosionOwner, NULL);
	}

	explicit CEventShockingExplosion(Vec3V_In explosionPos)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_EXPLOSION;
#endif
		Set(explosionPos, NULL, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShockingExplosion* dest = rage_new CEventShockingExplosion;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_EXPLOSION;	}

	bool AffectsPedCore(CPed* pPed) const;

	virtual Vec3V_Out GetEntityOrSourcePosition() const;

	// Return the driver if it exists otherwise return null.
	virtual CPed* GetSourcePed() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingExplosion);
};


class CEventShockingFire : public CEventShocking
{
public:
	explicit CEventShockingFire()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_FIRE;
#endif
	}

	explicit CEventShockingFire(CEntity& entityOnFire)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_FIRE;
#endif
		Set(entityOnFire.GetTransform().GetPosition(), &entityOnFire, NULL);
	}

	explicit CEventShockingFire(Vec3V_In firePos, CEntity* pEntityOnFire)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_FIRE;
#endif
		Set(firePos, pEntityOnFire, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingFire;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_FIRE;	}

	// Return the driver if it exists otherwise return null.
	virtual CPed* GetSourcePed() const;

	bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingFire);
};


class CEventShockingGunFight : public CEventShocking
{
public:
	explicit CEventShockingGunFight()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_GUN_FIGHT;
#endif
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingGunFight;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_GUN_FIGHT;	}

	static const Tunables& StaticGetTunables()
	{	return sm_Tunables;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingGunFight);
};


class CEventShockingGunshotFired : public CEventShocking
{
public:
	explicit CEventShockingGunshotFired()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_GUNSHOT_FIRED;
#endif
	}

	explicit CEventShockingGunshotFired(CEntity& firingEntity)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_GUNSHOT_FIRED;
#endif
		Set(firingEntity.GetTransform().GetPosition(), &firingEntity, NULL);
	}
	
	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingGunshotFired;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_GUNSHOT_FIRED;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingGunshotFired);
};


class CEventShockingHelicopterOverhead : public CEventShocking
{
public:
	explicit CEventShockingHelicopterOverhead()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_HELICOPTER_OVERHEAD;
#endif
	}

	explicit CEventShockingHelicopterOverhead(CEntity& heli)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_HELICOPTER_OVERHEAD;
#endif
		Set(heli.GetTransform().GetPosition(), &heli, NULL);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingHelicopterOverhead;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_HELICOPTER_OVERHEAD;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingHelicopterOverhead);
};


class CEventShockingParachuterOverhead : public CEventShocking
{
public:
	explicit CEventShockingParachuterOverhead()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PARACHUTER_OVERHEAD;
#endif
	}

	explicit CEventShockingParachuterOverhead(CEntity& parachuter)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PARACHUTER_OVERHEAD;
#endif
		Set(parachuter.GetTransform().GetPosition(), &parachuter, NULL);
	}

	virtual bool operator==(const fwEvent& rEvent) const;

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingParachuterOverhead;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_PARACHUTER_OVERHEAD;	}

	virtual atHashWithStringNotFinal GetShockingSpeechHash() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingParachuterOverhead);
};

class CEventShockingDangerousAnimal : public CEventShocking
{
public:
	explicit CEventShockingDangerousAnimal()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_DANGEROUS_ANIMAL;
#endif
	}

	explicit CEventShockingDangerousAnimal(CEntity& dangerousAnimal)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_DANGEROUS_ANIMAL;
#endif
		Set(dangerousAnimal.GetTransform().GetPosition(), &dangerousAnimal, NULL);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingDangerousAnimal;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const { return EVENT_SHOCKING_DANGEROUS_ANIMAL; }

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingDangerousAnimal);
};

class CEventShockingEngineRevved : public CEventShocking
{
public:
	explicit CEventShockingEngineRevved()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_ENGINE_REVVED;
#endif
	}

	explicit CEventShockingEngineRevved(CEntity& vehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_ENGINE_REVVED;
#endif
		Set(vehicle.GetTransform().GetPosition(), &vehicle, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingEngineRevved;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_ENGINE_REVVED;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingEngineRevved);
};

class CEventShockingHornSounded : public CEventShocking
{
public:
	explicit CEventShockingHornSounded()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_HORN_SOUNDED;
#endif
	}

	explicit CEventShockingHornSounded(CEntity& vehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_HORN_SOUNDED;
#endif
		Set(vehicle.GetTransform().GetPosition(), &vehicle, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingHornSounded;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_HORN_SOUNDED;	}

	virtual bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingHornSounded);
};

class CEventShockingInDangerousVehicle : public CEventShocking
{
public:
	explicit CEventShockingInDangerousVehicle()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_IN_DANGEROUS_VEHICLE;
#endif
	}

	explicit CEventShockingInDangerousVehicle(CEntity& dangerousVehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_IN_DANGEROUS_VEHICLE;
#endif
		Set(dangerousVehicle.GetTransform().GetPosition(), &dangerousVehicle, NULL);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const;

	virtual int GetEventType() const { return EVENT_SHOCKING_IN_DANGEROUS_VEHICLE; }

	// Return the driver.
	virtual CPed* GetSourcePed() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingInDangerousVehicle);
};


class CEventShockingInjuredPed : public CEventShocking
{
public:
	explicit CEventShockingInjuredPed()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_INJURED_PED;
#endif
	}

	explicit CEventShockingInjuredPed(CEntity& pedInjured, CEntity* pInflictor)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_INJURED_PED;
#endif
		Set(pedInjured.GetTransform().GetPosition(), pInflictor, &pedInjured);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingInjuredPed;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_INJURED_PED;	}

	bool AffectsPedCore(CPed* pPed) const;

	// Return the source entity if it is a ped, the driver if it is a vehicle, otherwise return null.
	virtual CPed* GetSourcePed() const;

	// Return the driver if it exists otherwise return the vehicle.
	virtual CEntity* GetSourceEntity() const;

	virtual CPed* GetTargetPed() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingInjuredPed);
};


class CEventShockingMadDriver : public CEventShocking
{
public:
	explicit CEventShockingMadDriver()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MAD_DRIVER;
#endif
	}

	explicit CEventShockingMadDriver(CEntity& madDriver)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MAD_DRIVER;
#endif
		Set(madDriver.GetTransform().GetPosition(), &madDriver, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingMadDriver;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_MAD_DRIVER;	}


	bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingMadDriver);
};


class CEventShockingMadDriverExtreme : public CEventShocking
{
public:
	explicit CEventShockingMadDriverExtreme()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MAD_DRIVER_EXTREME;
#endif
	}

	explicit CEventShockingMadDriverExtreme(CEntity& madDriver)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MAD_DRIVER_EXTREME;
#endif
		Set(madDriver.GetTransform().GetPosition(), &madDriver, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingMadDriverExtreme;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_MAD_DRIVER_EXTREME;	}

	bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingMadDriverExtreme);
};


class CEventShockingMadDriverBicycle : public CEventShocking
{
public:
	explicit CEventShockingMadDriverBicycle()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MAD_DRIVER_BICYCLE;
#endif
	}

	explicit CEventShockingMadDriverBicycle(CEntity& madDriver)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MAD_DRIVER_BICYCLE;
#endif
		Set(madDriver.GetTransform().GetPosition(), &madDriver, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingMadDriverBicycle;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_MAD_DRIVER_BICYCLE;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingMadDriverBicycle);
};


// Fired from script during random mugging events.
class CEventShockingMugging : public CEventShocking
{
public:
	explicit CEventShockingMugging()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MUGGING;
#endif
	}

	explicit CEventShockingMugging(CEntity& mugger)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_MUGGING;
#endif
		Set(mugger.GetTransform().GetPosition(), &mugger, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingMugging;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_MUGGING;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingMugging);
};

// Fired when pointing a non violent weapon at a ped.
class CEventShockingNonViolentWeaponAimedAt : public CEventShocking
{
public:
	explicit CEventShockingNonViolentWeaponAimedAt()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT;
#endif
	}

	explicit CEventShockingNonViolentWeaponAimedAt(CEntity& aimer)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT;
#endif
		Set(aimer.GetTransform().GetPosition(), &aimer, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingNonViolentWeaponAimedAt;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingNonViolentWeaponAimedAt);
};

class CEventShockingPedKnockedIntoByPlayer : public CEventShocking
{
public:
	explicit CEventShockingPedKnockedIntoByPlayer()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PED_KNOCKED_INTO_BY_PLAYER;
#endif
	}

	explicit CEventShockingPedKnockedIntoByPlayer(CEntity& knockingPed, CEntity* pPedKnockedInto)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PED_KNOCKED_INTO_BY_PLAYER;
#endif
		Set(knockingPed.GetTransform().GetPosition(), &knockingPed, pPedKnockedInto);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingPedKnockedIntoByPlayer;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_PED_KNOCKED_INTO_BY_PLAYER;	}

	virtual bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingPedKnockedIntoByPlayer);
};

class CEventShockingPedRunOver : public CEventShocking
{
public:
	explicit CEventShockingPedRunOver()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PED_RUN_OVER;
#endif
	}

	explicit CEventShockingPedRunOver(CEntity& pedRunOver, CEntity* pInflictor)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PED_RUN_OVER;
#endif
		Set(pedRunOver.GetTransform().GetPosition(), pInflictor, &pedRunOver);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingPedRunOver;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_PED_RUN_OVER;	}

	bool AffectsPedCore(CPed* pPed) const;

	// Return the source entity if it is a ped, the driver if it is a vehicle, otherwise return null.
	virtual CPed* GetSourcePed() const;

	// Return the driver if it exists otherwise return the vehicle.
	virtual CEntity* GetSourceEntity() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingPedRunOver);
};


class CEventShockingPedShot : public CEventShocking
{
public:
	explicit CEventShockingPedShot()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PED_SHOT;
#endif
	}

	explicit CEventShockingPedShot(CEntity& shooter, CEntity* pVictim)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PED_SHOT;
#endif
		Set(shooter.GetTransform().GetPosition(), &shooter, pVictim);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingPedShot;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_PED_SHOT;	}

	virtual Vec3V_Out GetEntityOrSourcePosition() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingPedShot);
};


class CEventShockingPlaneFlyby : public CEventShocking
{
public:
	explicit CEventShockingPlaneFlyby()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PLANE_FLY_BY;
#endif
	}

	explicit CEventShockingPlaneFlyby(CEntity& plane)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PLANE_FLY_BY;
#endif
		Set(plane.GetTransform().GetPosition(), &plane, NULL);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingPlaneFlyby;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_PLANE_FLY_BY;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingPlaneFlyby);
};

class CEventShockingPropertyDamage : public CEventShocking
{
public:
	explicit CEventShockingPropertyDamage()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PROPERTY_DAMAGE;
#endif
	}

	explicit CEventShockingPropertyDamage(CEntity& culprit, CEntity* pProperty)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_PROPERTY_DAMAGE;
#endif
		Set(culprit.GetTransform().GetPosition(), &culprit, pProperty);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* dest = rage_new CEventShockingPropertyDamage;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_PROPERTY_DAMAGE;	}

	virtual bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingPropertyDamage);
};

class CEventShockingRunningPed : public CEventShocking
{
public:
	explicit CEventShockingRunningPed()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_RUNNING_PED;
#endif
	}

	explicit CEventShockingRunningPed(CEntity& runner)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_RUNNING_PED;
#endif
		Set(runner.GetTransform().GetPosition(), &runner, NULL);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingRunningPed;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_RUNNING_PED;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingRunningPed);
};


class CEventShockingRunningStampede : public CEventShocking
{
public:
	explicit CEventShockingRunningStampede()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_RUNNING_STAMPEDE;
#endif
	}

	virtual bool CheckHasExpired() const;
	virtual bool ProcessInGlobalGroup();

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingRunningStampede;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_RUNNING_STAMPEDE;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingRunningStampede);
};


class CEventShockingSeenCarStolen : public CEventShocking
{
public:
	enum {
		SCSFlag_RequireNearbyAlertFlagOnPed = BIT0
	};

	explicit CEventShockingSeenCarStolen(bool bRequireNearbyAlertFlag = false)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_CAR_STOLEN;
#endif
		if (bRequireNearbyAlertFlag)
		{
			m_DerivedClassFlags.SetFlag(SCSFlag_RequireNearbyAlertFlagOnPed);
		}
	}

	explicit CEventShockingSeenCarStolen(CEntity& thief, CEntity* vehicle, bool bRequireNearbyAlertFlag = false)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_CAR_STOLEN;
#endif
		if (bRequireNearbyAlertFlag)
		{
			m_DerivedClassFlags.SetFlag(SCSFlag_RequireNearbyAlertFlagOnPed);
		}

		Set(thief.GetTransform().GetPosition(), &thief, vehicle);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingSeenCarStolen(RequiresNearbyAlertFlagOnPed());
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_CAR_STOLEN;	}

	bool AffectsPedCore(CPed* pPed) const;

	virtual CPed* GetPlayerCriminal() const;

	bool RequiresNearbyAlertFlagOnPed() const { return m_DerivedClassFlags.IsFlagSet(SCSFlag_RequireNearbyAlertFlagOnPed); }
protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenCarStolen);
};


class CEventShockingSeenConfrontation : public CEventShocking
{
public:
	explicit CEventShockingSeenConfrontation()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_CONFRONTATION;
#endif
	}

	explicit CEventShockingSeenConfrontation(CEntity& confronter, CEntity* victim)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_CONFRONTATION;
#endif
		Set(confronter.GetTransform().GetPosition(), &confronter, victim);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingSeenConfrontation;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_CONFRONTATION;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenConfrontation);
};


class CEventShockingSeenGangFight : public CEventShocking
{
public:
	explicit CEventShockingSeenGangFight()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_GANG_FIGHT;
#endif
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingSeenGangFight;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_GANG_FIGHT;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenGangFight);
};


class CEventShockingSeenInsult : public CEventShocking
{
public:
	explicit CEventShockingSeenInsult()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_INSULT;
#endif
	}
	
	explicit CEventShockingSeenInsult(CEntity& insulter, CEntity* victim)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_INSULT;
#endif
		Set(insulter.GetTransform().GetPosition(), &insulter, victim);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingSeenInsult;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_INSULT;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenInsult);
};


class CEventShockingSeenMeleeAction : public CEventShocking
{
public:
	explicit CEventShockingSeenMeleeAction()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_MELEE_ACTION;
#endif
	}

	explicit CEventShockingSeenMeleeAction(CEntity& attacker, CEntity* victim)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_MELEE_ACTION;
#endif
		Set(attacker.GetTransform().GetPosition(), &attacker, victim);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingSeenMeleeAction;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_MELEE_ACTION;	}

	virtual bool	AffectsPedCore(CPed* pPed) const;
	virtual CPed*	GetTargetPed() const;
	
	virtual bool	IsExposedToPed(CPed *pPed) const;
	virtual bool	CreateResponseTask(CPed* pPed, CEventResponse& response) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenMeleeAction);
};


class CEventShockingSeenNiceCar : public CEventShocking
{
public:
	explicit CEventShockingSeenNiceCar()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_NICE_CAR;
#endif
	}

	explicit CEventShockingSeenNiceCar(CEntity& vehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_NICE_CAR;
#endif
		Set(vehicle.GetTransform().GetPosition(), &vehicle, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingSeenNiceCar;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_NICE_CAR;	}

	bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenNiceCar);

};

class CEventShockingSeenPedKilled : public CEventShocking
{
public:
	explicit CEventShockingSeenPedKilled()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_PED_KILLED;
#endif
	}

	explicit CEventShockingSeenPedKilled(CEntity& attacker, CEntity* victim)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_PED_KILLED;
#endif
		Set(attacker.GetTransform().GetPosition(), &attacker, victim);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingSeenPedKilled;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_PED_KILLED;	}

	virtual CPed* GetTargetPed() const;

	virtual bool IsExposedToPed(CPed *pPed) const;
	virtual bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSeenPedKilled);
};

///////////////////////////////////////////////////////////////////////////////////
// CEventShockingSiren
// Police sirens, etc.
///////////////////////////////////////////////////////////////////////////////////

class CEventShockingSiren : public CEventShocking
{
public:

	explicit CEventShockingSiren()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SIREN;
#endif
	}

	explicit CEventShockingSiren(CEntity& source)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SIREN;
#endif
		Set(source.GetTransform().GetPosition(), &source, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* pClone = rage_new CEventShockingSiren;
		InitClone(*pClone);
		return pClone;
	}

	virtual bool CheckHasExpired() const;

	virtual int GetEventType() const
	{	
		return EVENT_SHOCKING_SIREN;
	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingSiren);
};

///////////////////////////////////////////////////////////////////////////////////
// CEventShockingCarAlarm
///////////////////////////////////////////////////////////////////////////////////

class CEventShockingCarAlarm : public CEventShocking
{
public:

	explicit CEventShockingCarAlarm()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_ALARM;
#endif
	}

	explicit CEventShockingCarAlarm(CEntity& source)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_CAR_ALARM;
#endif
		Set(source.GetTransform().GetPosition(), &source, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* pClone = rage_new CEventShockingCarAlarm;
		InitClone(*pClone);
		return pClone;
	}

	virtual bool CheckHasExpired() const;

	// Return the driver if it exists otherwise return null.
	virtual CPed* GetSourcePed() const;

	// Return the driver if it exists otherwise return the vehicle.
	virtual CEntity* GetSourceEntity() const;

	virtual int GetEventType() const
	{	
		return EVENT_SHOCKING_CAR_ALARM;
	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingCarAlarm);
};

///////////////////////////////////////////////////////////////////////////////////
// CEventShockingStudioBomb
// Custom shocking event for script to use in some situations.
///////////////////////////////////////////////////////////////////////////////////

class CEventShockingStudioBomb : public CEventShocking
{
public:
	explicit CEventShockingStudioBomb()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_STUDIO_BOMB;
#endif
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* pClone = rage_new CEventShockingStudioBomb;
		InitClone(*pClone);
		return pClone;
	}

	virtual int GetEventType() const
	{	
		return EVENT_SHOCKING_STUDIO_BOMB;
	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingStudioBomb);
};

class CEventShockingVehicleTowed : public CEventShocking
{
public:
	explicit CEventShockingVehicleTowed()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_VEHICLE_TOWED;
#endif
	}

	explicit CEventShockingVehicleTowed(CEntity& source, CEntity& targetVehicle)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_VEHICLE_TOWED;
#endif
		Set(source.GetTransform().GetPosition(), &source, &targetVehicle);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingVehicleTowed;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_VEHICLE_TOWED;	}

	bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingVehicleTowed);

};

class CEventShockingWeaponThreat : public CEventShocking
{
public:
	explicit CEventShockingWeaponThreat()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_WEAPON_THREAT;
#endif
	}

	explicit CEventShockingWeaponThreat(CEntity& aggressor, CEntity* victim)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_WEAPON_THREAT;
#endif
		Set(aggressor.GetTransform().GetPosition(), &aggressor, victim);
	}
	
	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingWeaponThreat;
	InitClone(*dest);
	return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_SEEN_WEAPON_THREAT;	}

	static float GetVisualReactionRange() { return sm_Tunables.m_VisualReactionRange; }

	virtual CPed* GetTargetPed() const;
	virtual bool AffectsPedCore(CPed* pPed) const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingWeaponThreat);
};

class CEventShockingVisibleWeapon : public CEventShocking
{
public:
	explicit CEventShockingVisibleWeapon()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_VISIBLE_WEAPON;
#endif
	}

	explicit CEventShockingVisibleWeapon(CEntity& weaponHolder)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_VISIBLE_WEAPON;
#endif
		Set(weaponHolder.GetTransform().GetPosition(), &weaponHolder, NULL);
	}

	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	CEventShocking* dest = rage_new CEventShockingVisibleWeapon;
		InitClone(*dest);
		return dest;
	}

	virtual int GetEventType() const
	{	return EVENT_SHOCKING_VISIBLE_WEAPON;	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingVisibleWeapon);
};

///////////////////////////////////////////////////////////////////////////////////
// CEventShockingWeirdPed
// Generated when the player is odd in some way.
///////////////////////////////////////////////////////////////////////////////////

class CEventShockingWeirdPed : public CEventShocking
{
public:
	explicit CEventShockingWeirdPed()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_WEIRD_PED;
#endif
	}

	explicit CEventShockingWeirdPed(CEntity& rWeirdo)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_WEIRD_PED;
#endif
		Set(rWeirdo.GetTransform().GetPosition(), &rWeirdo, NULL);
	}

	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CheckHasExpired() const;

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* pClone = rage_new CEventShockingWeirdPed;
		InitClone(*pClone);
		return pClone;
	}

	virtual int GetEventType() const
	{	
		return EVENT_SHOCKING_SEEN_WEIRD_PED;
	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingWeirdPed);
};

///////////////////////////////////////////////////////////////////////////////////
// CEventShockingWeirdPedApproaching
// Generated when the player is odd in some way and getting closer.
///////////////////////////////////////////////////////////////////////////////////

class CEventShockingWeirdPedApproaching : public CEventShockingWeirdPed
{
public:
	explicit CEventShockingWeirdPedApproaching()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING;
#endif
	}

	explicit CEventShockingWeirdPedApproaching(CEntity& rWeirdo)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING;
#endif
		Set(rWeirdo.GetTransform().GetPosition(), &rWeirdo, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* pClone = rage_new CEventShockingWeirdPedApproaching;
		InitClone(*pClone);
		return pClone;
	}

	virtual bool AffectsPedCore(CPed* pPed) const;

	virtual int GetEventType() const
	{	
		return EVENT_SHOCKING_SEEN_WEIRD_PED_APPROACHING;
	}

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingWeirdPedApproaching);
};

///////////////////////////////////////////////////////////////////////////////////
// CEventShockingPotentialBlast
///////////////////////////////////////////////////////////////////////////////////

class CEventShockingPotentialBlast : public CEventShocking
{
public:
	explicit CEventShockingPotentialBlast()
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_POTENTIAL_BLAST;
#endif
	}

	explicit CEventShockingPotentialBlast(CEntity& rEntity)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_POTENTIAL_BLAST;
#endif
		Set(rEntity.GetTransform().GetPosition(), &rEntity, NULL);
	}

	explicit CEventShockingPotentialBlast(Vec3V_In vPosition)
	{
#if !__FINAL
		m_EventType = EVENT_SHOCKING_POTENTIAL_BLAST;
#endif
		Set(vPosition, NULL, NULL);
	}

	virtual CEventEditableResponse* CloneEditable() const
	{	
		CEventShocking* pClone = rage_new CEventShockingPotentialBlast;
		InitClone(*pClone);
		return pClone;
	}

	virtual int GetEventType() const
	{	
		return EVENT_SHOCKING_POTENTIAL_BLAST;
	}

	virtual bool	AffectsPedCore(CPed* pPed) const;
	virtual CPed*	GetSourcePed() const;
	CProjectile*	GetProjectile() const;

protected:
	DECLARE_SHOCKING_EVENT_TUNABLES(CEventShockingPotentialBlast);
};

#endif	// EVENT_EVENT_SHOCKING_H

// End of file 'event/EventShocking.h'
