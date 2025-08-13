//
// event/EventSound.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef EVENT_EVENT_SOUND_H
#define EVENT_EVENT_SOUND_H

#include "event/EventEditable.h"
#include "event/EventPriorities.h"
#include "scene/RegdRefTypes.h"
#include "Task/System/Tuning.h"

//-----------------------------------------------------------------------------

/*
PURPOSE
	Base class for sound events with decision-maker response.
NOTES
	This class currently contains commented out fragments of code for finding
	an "audio path" using the navigation system, in case we want to do that.
*/
class CEventSoundBase : public CEventEditableResponse
{
public:

	CEventSoundBase(CEntity* pEntityMakingSound, float soundRange);	// , const int iTimeHeard=-1, const Vector3& vTargetPosAtHeardTime=Vector3(0,0,0));
	virtual ~CEventSoundBase();

//	virtual bool IsEventReady( CPed *pPed );
//	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventSoundDynamic(m_pEntityMakingSound, m_SoundRange, m_iTimeHeard, m_vTargetPosAtHeardTime);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;
	virtual CEntity* GetSourceEntity() const {return m_pEntityMakingSound;}
	virtual bool operator==(const fwEvent& otherEvent) const;

//	int GetTimeHeard() const {return m_iTimeHeard;}
//	const Vector3& GetTargetPosAtHeardTime() const {return m_vTargetPosAtHeardTime;}

	static bool CanPedHearSound(const CPed& listener, Vec3V_In soundPos, const float& soundRange);

protected:

//	Vector3		m_vTargetPosAtHeardTime;
	RegdEnt 	m_pEntityMakingSound;
	float		m_SoundRange;
//	int			m_iTimeHeard;
//	TPathHandle m_audioPathRequest;

};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Sound event that gets generated when a ped walks.
NOTES
	If it turns out that we don't need to separate reactions from footsteps
	from reactions to other types of sounds, we may be able to just
	replace use of this with CEventSoundDynamic.
*/
class CEventFootStepHeard : public CEventSoundBase
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinDelayTimer;
		float m_MaxDelayTimer;

		PAR_PARSABLE;
	};

	CEventFootStepHeard(CPed* pPedHeardWalking, float soundRange);
	~CEventFootStepHeard() {}

	// Required event functions
	virtual CEventEditableResponse* CloneEditable() const;
	virtual int			GetEventType(void) const		{ return EVENT_FOOT_STEP_HEARD; }
	virtual bool		CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual int			GetEventPriority(void) const	{ return E_PRIORITY_FOOT_STEP_HEARD; }
	virtual bool		AffectsPedCore(CPed* pPed) const;

private:

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = sm_Tunables.m_MinDelayTimer; fMax = sm_Tunables.m_MaxDelayTimer; return true; }

private:

	static Tunables sm_Tunables;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Communicates an event audibly to other peds in the area.
	E.g. Shout about a gunshot, or being injured, or seeing an enemy etc...

	Enemies may react by shooting you, friends may react by helping... or
	shooting you.
*/
class CEventCommunicateEvent : public CEvent
{
public:

	CEventCommunicateEvent(CEntity* pEntityMakingSound, CEvent* pEvent, float soundRange);	//, const int iTimeHeard=-1, const Vector3& vTargetPosAtHeardTime=Vector3(0,0,0));
	virtual  ~CEventCommunicateEvent();

	virtual CEvent* Clone() const;
	virtual int GetEventType() const {return EVENT_COMMUNICATE_EVENT;}
	virtual int GetEventPriority() const {return E_PRIORITY_COMMUNICATE_EVENT;}
	virtual bool IsEventReady( CPed *pPed );
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CEntity* GetSourceEntity() const {return m_pEntityMakingSound;}
	virtual bool CanBeInterruptedBySameEvent() const {return true;}
	virtual bool operator==(const fwEvent& otherEvent) const;

	CEvent* GetEventToBeCommunicated( void ) const { return m_pEvent; }

	bool CanBeCommunicatedTo(const CPed* pPed) const;
	bool CanBeHeardBy(const CPed* pPed) const;

private:

	// The source ped who is communicating the event
	RegdEnt 	m_pEntityMakingSound;
	// The event that's being communicated
	RegdEvent	m_pEvent;
	float		m_SoundRange;		// Range of the sound in meters
//	int			m_iTimeHeard;
//	Vector3		m_vTargetPosAtHeardTime;
//	TPathHandle m_audioPathRequest;

	bool		m_bFinished;

};

//-----------------------------------------------------------------------------

#endif	// EVENT_EVENT_SOUND_H

// End of file 'event/EventSound.h'
