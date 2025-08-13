#ifndef EVENT_H
#define EVENT_H

// framework includes
#include "ai/AITarget.h"

// game includes
#include "event/event.h"
#include "event/EventEditable.h"
#include "event/EventPriorities.h"
#include "fwevent/eventregdref.h"
#include "Peds/pedDefines.h"
#include "scene/RegdRefTypes.h"
#include "task/Response/Info/AgitatedEnums.h"
#include "task/System/TaskHelpers.h"
#include "task/scenario/info/ScenarioInfoConditions.h"

namespace rage { class aiTask; }
class CEntity;
class CVehicle;
class CObject;
class CPedGroup;
class CTaskAgitated;
class CTaskSet;
class CTaskNavBase;

#define __CATCH_EVENT_LEAK	__DEV

// Disabled, no current code would ever modify the data here, and it had the potential
// to get in the way of making event types more data driven.
#if 0

class CScriptedPriorities
{
public:

	enum
	{
		MAX_NUM_PRIORITIES_PER_EVENT=4
	};

	CScriptedPriorities();
	~CScriptedPriorities();

	static void Init();
	static void Shutdown();

	//Clear all the scripted priorities.
	static void Clear();
	//Add a scripted priority.
	static bool Add(const int eventType, const int takesPriorityOverEventType);
	//Remove a scripted priority.
	static bool Remove(const int eventType, const int takesPriorityOverEventType);
	//Test if an event has been scripted to take priority over another event.
	static bool TakesPriorityOver(const int eventType, const int takesPriorityOverEventType);

private:

	static int ms_priorities[NUM_AI_EVENTTYPE][MAX_NUM_PRIORITIES_PER_EVENT];
};

#endif	// 0

class CInformFriendsEvent
{
public:

	friend class CInformFriendsEventQueue;

	CInformFriendsEvent();
	virtual ~CInformFriendsEvent();

	void Process();
	void Set(CPed* pPed, CEvent* pEvent, const int iTime);
	void Flush();
	const CEvent* GetEvent() const { return m_pEvent; }

private:

	void ProcessNetworkCommunicationEvent(const CEvent* pEventToBeCommunicated);

	RegdPed m_pPedToInform;
	RegdEvent m_pEvent;
	u32 m_informTime;
};

class CInformFriendsEventQueue
{
public:

	enum
	{
		MAX_QUEUE_SIZE=64,
		MIN_INFORM_TIME=300,
		MAX_INFORM_TIME=800
	};

	CInformFriendsEventQueue();
	virtual ~CInformFriendsEventQueue();

	static void Init();
	static bool Add(CPed* pPedToInform, CEvent* pEvent);
	static void Process();
	static void Flush();
	static bool HasEvent(const CEvent* pEvent);

private:

	static int ms_iQueueSize;
	static CInformFriendsEvent ms_informFriendsEvents[MAX_QUEUE_SIZE];

};


class CInformGroupEvent
{
public:

	friend class CInformGroupEventQueue;

	CInformGroupEvent();
	virtual ~CInformGroupEvent();

	void Process();
	void Set(CPed* pPed, CPedGroup* pPedGroup, CEvent* pEvent, const int iTime);
	void Flush();

private:

	RegdPed m_pPedWhoInformed;
	CPedGroup* m_pPedWhoInformedGroup;
	RegdEvent m_pEvent;
	u32 m_informTime;
};

class CInformGroupEventQueue
{
public:

	enum
	{
		MAX_QUEUE_SIZE=8,
	};

	CInformGroupEventQueue();
	virtual ~CInformGroupEventQueue();

	static void Init();
	static bool Add(CPed* pPedWhoInformed, CPedGroup* pPedGroup, CEvent* pEvent);
	static void Process();
	static void Flush();

private:

	static int ms_iQueueSize;
	static CInformGroupEvent ms_informGroupEvents[MAX_QUEUE_SIZE];

};

class CEventRanOverPed :public CEvent
{
public:
	CEventRanOverPed(CVehicle* pVehicle, CPed* pPedRanOver, float fDamagedCaused);
	virtual ~CEventRanOverPed();

	virtual int			GetEventType() const {return EVENT_RAN_OVER_PED;}
	virtual int			GetEventPriority() const {return E_PRIORITY_RAN_OVER_PED;}
	virtual CEvent*		Clone() const {return rage_new CEventRanOverPed(m_pVehicle,m_pPedRanOver,m_fDamageCaused);}
	virtual bool		AffectsPedCore(CPed* pPed) const;
	//bool				AffectsPed(CPed* pPed) const;
	virtual bool		TakesPriorityOver(const CEvent&  otherEvent) const;
	virtual bool		CanBeInterruptedBySameEvent() const { return true; }

	virtual bool		ShouldCreateResponseTaskForPed(const CPed* pPed) const;
	virtual bool		CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	CVehicle*			GetVehicle() const		{return m_pVehicle;}
	CPed*				GetPedRanOver()	const	{return m_pPedRanOver;}
	float				GetDamageCaused() const	{return m_fDamageCaused;}

private:
	RegdVeh		m_pVehicle;
	RegdPed		m_pPedRanOver;
	float		m_fDamageCaused;
};

class CEventOpenDoor : public CEvent
{
public:
	CEventOpenDoor(CDoor* pDoor);
	virtual ~CEventOpenDoor();

	virtual int			GetEventType() const {return EVENT_OPEN_DOOR;}
	virtual int			GetEventPriority() const {return E_PRIORITY_OPEN_DOOR;}
	virtual CEvent*		Clone() const {return rage_new CEventOpenDoor(m_pDoor);}
	virtual bool		TakesPriorityOver(const CEvent&  otherEvent) const;
	virtual bool		CanBeInterruptedBySameEvent() const { return true; }
	virtual bool		CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool		IsTemporaryEvent() const { return true; }

	CDoor*				GetDoor() const		{return m_pDoor;}

private:
	RegdDoor		m_pDoor;
};

class CEventShovePed : public CEvent
{
public:
	CEventShovePed(CPed* pPedToShove);
	virtual ~CEventShovePed();

	virtual int			GetEventType() const {return EVENT_SHOVE_PED;}
	virtual int			GetEventPriority() const {return E_PRIORITY_SHOVE_PED;}
	virtual CEvent*		Clone() const {return rage_new CEventShovePed(m_pPedToShove);}
	virtual bool		AffectsPedCore(CPed* pPed) const;
	virtual bool		TakesPriorityOver(const CEvent&  otherEvent) const;
	virtual bool		CanBeInterruptedBySameEvent() const { return true; }
	virtual bool		CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool		IsTemporaryEvent() const { return true; }

	CPed*				GetPedToShove() const		{return m_pPedToShove;}

private:
	RegdPed				m_pPedToShove;
};

//The ped has bumped into a vehicle.
class CEventVehicleCollision : public CEvent
{
public:
	enum eForceReaction
	{
		PED_FORCE_EVADE_STEP = 1,	// force an evasive step move using EVADE_STEP anim
		PED_FORCE_EVADE_COWER		// use HANDS_COWER anim
	};

	static const float ms_fDamageThresholdSpeed;
	static const float ms_fDamageThresholdSpeedAlongContact;
	static const float ms_fMaxPlayerImpulse;
	static const float ms_fHighDamageImpulseThreshold;
	static const float ms_fLowDamageImpulseThreshold;

	CEventVehicleCollision(const CVehicle* pVehicle, const Vector3& vNormal, const Vector3& vPos, const float fImpulseMagnitude, const u16 iVehicleComponent, const float fMoveBlendRatio, u16 nForceReaction = 0);
	virtual ~CEventVehicleCollision();

	virtual bool operator==( const fwEvent& otherEvent ) const;
	virtual int GetEventType() const {return EVENT_VEHICLE_COLLISION;}
	virtual int GetEventPriority() const {return E_PRIORITY_VEHICLE_COLLISION;}
	virtual CEvent* Clone() const {
		CEventVehicleCollision * pEvent = rage_new CEventVehicleCollision(m_pVehicle,m_vNormal,m_vPos,m_fImpulseMagnitude,m_iVehicleComponent,m_fMoveBlendRatio);
		if(m_bForcedDoorCollision)
			pEvent->SetIsForcedDoorCollision();
		pEvent->SetDirectionToWalkRoundCar(GetDirectionToWalkRoundCar());	// unused now I think
		pEvent->SetVehicleStationary(m_bVehicleStationary);
		pEvent->m_fVehicleSpeedSq = m_fVehicleSpeedSq;
		return pEvent;
	}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	//inline virtual bool CanBeInterruptedBySameEvent() const { return true; }
	inline virtual bool IsTemporaryEvent() const {return true;}
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	u16 GetForceReaction() const {return m_nForceReaction;}
	float GetDamageImpulseMagnitude() const {return m_fImpulseMagnitude;}
	const CVehicle* GetVehicle() const {return m_pVehicle;}
	const Vector3& GetNormal() const {return m_vNormal;}
	const Vector3& GetPos() const {return m_vPos;}
	u16 GetVehicleComponent() const {return m_iVehicleComponent;}
	float GetMoveBlendRatio() const { return m_fMoveBlendRatio; }

	void SetIsForcedDoorCollision() { m_bForcedDoorCollision = true; }
	bool GetIsForcedDoorCollision() const { return m_bForcedDoorCollision; }

	void SetDirectionToWalkRoundCar(s32 d) { m_iDirectionToWalkRoundCar = static_cast<s8>(d); }
	s32 GetDirectionToWalkRoundCar() const { return m_iDirectionToWalkRoundCar; }

	void SetVehicleStationary( bool bStationary) { m_bVehicleStationary = bStationary; }

private:

	// HACK GTA
	bool CreateHackResponseTaskToAvoidStuntPlaneWingTips(CVehicle * pVehicle, CPed * pPed, CEventResponse& response) const;

	u16 m_iVehicleComponent;
	u16 m_nForceReaction;
	float m_fImpulseMagnitude;
	RegdVeh m_pVehicle;
	Vector3 m_vNormal;
	Vector3 m_vPos;
	float m_fMoveBlendRatio;
	float m_fVehicleSpeedSq;
	s8 m_iDirectionToWalkRoundCar;
	bool m_bForcedDoorCollision;
	bool m_bVehicleStationary;
};

//The ped has bumped into another ped.
class CEventPedCollisionWithPed : public CEvent
{
public:
	static const float ms_fPedBrushKnockdown;

	CEventPedCollisionWithPed(const u16 nPieceType, const float fImpulseMagnitude, const CPed* pPed, const Vector3& vNormal, const Vector3& vPos, const float fPedMoveBlendRatio, const float fOtherPedMoveBlendRatio);
	virtual  ~CEventPedCollisionWithPed();

	inline virtual int GetEventType() const {return EVENT_PED_COLLISION_WITH_PED;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_COLLISION_WITH_PED;}
	inline virtual CEvent* Clone() const {return rage_new CEventPedCollisionWithPed(m_iPieceType,m_fImpulseMagnitude,m_pOtherPed,m_vNormal,m_vPos,m_fPedMoveBlendRatio,m_fOtherPedMoveBlendRatio);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	inline virtual bool IsTemporaryEvent() const {return true;}
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_NONE; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	inline const Vector3& GetNormal() const {return m_vNormal;}

	inline float GetPedMoveBlendRatio() const { return m_fPedMoveBlendRatio; }

	inline CPed* GetOtherPed() const {return m_pOtherPed;}
	inline float GetOtherPedMoveBlendRatio() const { return m_fOtherPedMoveBlendRatio; }

protected:
	u16 m_iPieceType;
	float m_fImpulseMagnitude;
	RegdPed m_pOtherPed;
	Vector3 m_vNormal;
	Vector3 m_vPos;
	float m_fPedMoveBlendRatio;
	float m_fOtherPedMoveBlendRatio;
};

//This ped has bumped into the player.
class CEventPedCollisionWithPlayer : public CEventPedCollisionWithPed
{
public:
	CEventPedCollisionWithPlayer(const u16 nPieceType, const float fImpulseMagnitude, const CPed* pOtherPed, const Vector3& vNormal, const Vector3& vPos, const float fPedMoveBlendRatio, const float fOtherPedMoveBlendRatio);
	virtual ~CEventPedCollisionWithPlayer(){}

	virtual int GetEventType() const {return EVENT_PED_COLLISION_WITH_PLAYER;}
	virtual CEvent* Clone() const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool RequiresAbortForRagdoll() const {return false;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool IsTemporaryEvent() const { return true; }

	static bool CNC_ShouldPreventEvasiveStepDueToPlayerShoulderBarging(const CPed* pPed, const CPed* pOtherPed);

private:

	float m_fOtherPedSpeedSq;

};

//The player has bumped into a ped.
class CEventPlayerCollisionWithPed : public CEventPedCollisionWithPed
{
public:
	CEventPlayerCollisionWithPed(const u16 nPieceType, const float fImpulseMagnitude, const CPed* pOtherPed, const Vector3& vNormal, const Vector3& vPos, const float fPedMoveBlendRatio, const float fOtherPedMoveBlendRatio)
		: CEventPedCollisionWithPed(nPieceType,fImpulseMagnitude,pOtherPed,vNormal,vPos,fPedMoveBlendRatio,fOtherPedMoveBlendRatio){}
	virtual ~CEventPlayerCollisionWithPed(){}

	virtual int GetEventType() const {return EVENT_PLAYER_COLLISION_WITH_PED;}
	virtual CEvent* Clone() const {return rage_new CEventPlayerCollisionWithPed(m_iPieceType,m_fImpulseMagnitude,m_pOtherPed,m_vNormal,m_vPos,m_fPedMoveBlendRatio,m_fOtherPedMoveBlendRatio);}
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool AffectsPedCore(CPed* pPed) const;

};

//The ped has bumped into an object.
class CEventObjectCollision : public CEvent
{
public:
	CEventObjectCollision(const u16 nPieceType, const CObject* pObject, const Vector3& vNormal, const Vector3& vPos, const float fMoveBlendRatio);
	virtual ~CEventObjectCollision();

	inline virtual CEvent* Clone() const { return rage_new CEventObjectCollision(m_iPieceType, m_pObject, m_vNormal, m_vPos, m_fMoveBlendRatio); }

	inline virtual int GetEventType() const {return EVENT_OBJECT_COLLISION;}
	virtual int GetEventPriority() const {return E_PRIORITY_BUILDING_COLLISION;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	inline virtual bool IsTemporaryEvent() const {return true;}
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	inline CObject* GetCObject() const { return m_pObject; }
	inline const Vector3& GetNormal() const { return m_vNormal; }
	inline float GetMoveBlendRatio() const { return m_fMoveBlendRatio; }

	bool IsHeadOnCollision(const CPed& ped) const;
	
	static bool CanIgnoreCollision(const CPed * pPed, const Vector3 & vCollisionNormal, const Vector3 & vCollisionPos, const Vector3 & vTargetPos);
	static CTaskNavBase * GetRouteTarget(const CPed * pPed, const Vector3 & vGoToPointTarget, Vector3 & vOut_RouteTarget);

	static const float ms_fStraightAheadDotProduct;

	bool GetJustPushDoor() const { return m_bJustPushDoor; }
	void SetJustPushDoor(bool val) { m_bJustPushDoor = val; }

private:
	float   m_fMoveBlendRatio;    
	RegdObj m_pObject;
	Vector3 m_vNormal;
	Vector3 m_vPos;
	u16     m_iPieceType;
	bool	m_bJustPushDoor;
};

//The ped has been dragged out of a car.
class CEventDraggedOutCar : public CEventEditableResponse
{
public:
	CEventDraggedOutCar(const CVehicle* pVehicle, const CPed* pDraggingPed, const bool bIsDriver, const bool bForceFlee = false);
	virtual ~CEventDraggedOutCar();

	virtual int GetEventType() const {return EVENT_DRAGGED_OUT_CAR;}
	virtual int GetEventPriority() const;
	virtual CEventEditableResponse* CloneEditable() const 
	{
		CEventDraggedOutCar* pDraggedEvent = rage_new CEventDraggedOutCar(m_pVehicle, m_pDraggingPed, m_bWasDriver, m_bForceFlee);
		pDraggedEvent->SetSwitchToNM(m_bSwitchToNM);
		return pDraggedEvent;
	}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual CEntity* GetSourceEntity() const;

	virtual bool RequiresAbortForRagdoll() const {return m_bSwitchToNM;}
	virtual void ClearRagdollResponse() { m_bSwitchToNM = false; }

	CVehicle* GetVehicle() const {return m_pVehicle;}
	CPed* GetDraggingPed() const {return m_pDraggingPed;}
	bool GetWasDriver() const {return m_bWasDriver;}
	bool GetForceFlee() const { return m_bForceFlee; }
	bool GetSwitchToNM() const { return m_bSwitchToNM; }
	void SetForceFlee(bool val) { m_bForceFlee = val; }
	void SetSwitchToNM(bool val) { m_bSwitchToNM = val; }

private:
	RegdPed m_pDraggingPed;
	RegdVeh m_pVehicle;
	bool m_bWasDriver;
	bool m_bForceFlee;
	mutable bool m_bSwitchToNM; // mutable so we can turn it off in CreateResponseTask if the ragdoll has become blocked
};

#define MAX_EVENT_EXPLOSION_DELAY .1f

class CEventExplosion : public CEventEditableResponse
{
public:	
	CEventExplosion(Vec3V_In vPosition, CEntity* pOwner, float fRadius, bool bIsMinor);
	virtual  ~CEventExplosion();

	virtual int GetEventType() const { return EVENT_EXPLOSION; }
	virtual int GetEventPriority() const { return E_PRIORITY_EXPLOSION; }
	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventExplosion(m_vPosition, m_pOwner, m_fRadius, m_bIsMinor); }
	virtual bool AffectsPedCore(CPed* pPed) const; 
	virtual bool IsEventReady(CPed* pPed);
	virtual const Vector3& GetSourcePos() const { return RCC_VECTOR3(m_vPosition); }
	virtual bool IsTemporaryEvent() const { return m_bIsTemporaryEvent; }
	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;

public:

	CEntity*	GetOwner()		const { return m_pOwner; }
	Vec3V_Out	GetPosition()	const { return m_vPosition; }
	float		GetRadius()		const { return m_fRadius; }

private:

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = 0.0f; fMax = MAX_EVENT_EXPLOSION_DELAY; return true; }

	Vec3V	m_vPosition;
	RegdEnt	m_pOwner;
	float	m_fRadius;
	bool	m_bIsMinor;
	bool	m_bIsTemporaryEvent;
	
};

class CEventPedJackingMyVehicle : public CEvent
{

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinDelayTimer;
		float m_MaxDelayTimer;

		PAR_PARSABLE;
	};

public:

	CEventPedJackingMyVehicle(CVehicle* pVehicle, CPed* pJackedPed, const CPed* pJackingPed);
	virtual  ~CEventPedJackingMyVehicle();

	virtual int GetEventType() const { return EVENT_PED_JACKING_MY_VEHICLE; }
	virtual int GetEventPriority() const { return E_PRIORITY_PED_JACKING_MY_VEHICLE; }
	virtual CEvent* Clone() const { return rage_new CEventPedJackingMyVehicle(m_pVehicle, m_pJackedPed, m_pJackingPed); }
	virtual bool AffectsPedCore(CPed* pPed) const; 
	virtual CEntity* GetSourceEntity() const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

private:

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = sm_Tunables.m_MinDelayTimer; fMax = sm_Tunables.m_MaxDelayTimer; return true; }

protected:

	RegdVeh			m_pVehicle;
	RegdPed			m_pJackedPed;
	RegdConstPed	m_pJackingPed;

private:

	static Tunables sm_Tunables;
	
};

//A ped has just got in a car 
class CEventPedEnteredMyVehicle : public CEventEditableResponse
{
public:
	CEventPedEnteredMyVehicle(const CPed* pPedThatEnteredVehicle, const CVehicle* pVehicle);
	virtual  ~CEventPedEnteredMyVehicle();

	virtual int GetEventType() const {return EVENT_PED_ENTERED_MY_VEHICLE;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_ENTERED_MY_VEHICLE;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventPedEnteredMyVehicle(m_pPedThatEnteredVehicle,m_pVehicle);}
	virtual bool AffectsPedCore(CPed* pPed) const; 
	virtual CEntity* GetSourceEntity() const;

	CVehicle* GetVehicle() const {return m_pVehicle;}
	CPed* GetPedThatEnteredVehicle() const {return m_pPedThatEnteredVehicle;}
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

protected:
	RegdPed m_pPedThatEnteredVehicle;
	RegdVeh m_pVehicle;
};

//Ped revived
class CEventRevived : public CEvent
{
public:
	CEventRevived(CPed* pMedic, CVehicle* pAmbulance, bool bRevived = false);
	virtual ~CEventRevived();

	virtual int GetEventType() const {return EVENT_REVIVED;}
	virtual int GetLifeTime() const {return 0;}
	virtual CEvent* Clone() const {return rage_new CEventRevived(m_pMedic, m_pAmbulance, m_bRevived);}
	virtual bool AffectsPedCore(CPed* pPed) const; 
	int	GetEventPriority() const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;


	CVehicle*	GetAmbulance()	 const 	{return m_pAmbulance;}
	CPed*		GetTreatingMedic() const 	{return m_pMedic;}
	void		SetRevivied(bool bRevived) {m_bRevived = bRevived;}

#if !__FINAL
	virtual atString GetName() const {return atString("Revived");}
#endif 


private:
	RegdPed		m_pMedic;
	RegdVeh		m_pAmbulance;
	bool		m_bRevived;
};

//A ped to chase
class CEventPedToChase : public CEvent
{
public:
	CEventPedToChase(CPed* pPedToChase);
	virtual ~CEventPedToChase();

	virtual int GetEventType() const {return EVENT_PED_TO_CHASE;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_TO_CHASE;}
	virtual CEvent* Clone() const {return rage_new CEventPedToChase(m_pPedToChase);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_COMBAT; }

	CPed* GetPedToChase() const {return m_pPedToChase;}
	virtual CEntity* GetSourceEntity() const;

private:
	RegdPed m_pPedToChase;
};

//A ped to flee
class CEventPedToFlee : public CEvent
{
public:
	CEventPedToFlee(CPed* pPedToFlee);
	virtual ~CEventPedToFlee();

	virtual int GetEventType() const {return EVENT_PED_TO_FLEE;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_TO_FLEE;}
	virtual CEvent* Clone() const {return rage_new CEventPedToFlee(m_pPedToFlee);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_SMART_FLEE; }

	CPed* GetPedToFlee() const {return m_pPedToFlee;}
	virtual CEntity* GetSourceEntity() const;

private:
	RegdPed m_pPedToFlee;
};

//Acquaintance ped nearby.
class CEventAcquaintancePed : public CEventEditableResponse
{
public:

	CEventAcquaintancePed(CPed* pAcquaintancePed);
	virtual  ~CEventAcquaintancePed();

	virtual int GetEventType() const = 0;
	virtual int GetEventPriority() const = 0;
	virtual CEventEditableResponse* CloneEditable() const = 0; 
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;

	virtual CEntity* GetSourceEntity() const;
	virtual bool CanBeInterruptedBySameEvent() const {return true;}    
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;

	CPed* GetAcquaintancePed() const {return m_pAcquaintancePed;}

protected:

	RegdPed					m_pAcquaintancePed;
	eEventType				m_eventType;
};

class CEventAcquaintancePedRespect : public CEventAcquaintancePed
{
public:

	CEventAcquaintancePedRespect(CPed* pAcquaintancePed)
		: CEventAcquaintancePed(pAcquaintancePed)
	{
#if !__FINAL
		m_EventType = EVENT_ACQUAINTANCE_PED_RESPECT;
#endif
	}
	virtual ~CEventAcquaintancePedRespect(){}

	virtual int GetEventType() const {return EVENT_ACQUAINTANCE_PED_RESPECT;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_RESPECT;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventAcquaintancePedRespect(m_pAcquaintancePed);} 

};

class CEventAcquaintancePedLike : public CEventAcquaintancePed
{
public:

	CEventAcquaintancePedLike(CPed* pAcquaintancePed)
		: CEventAcquaintancePed(pAcquaintancePed)
	{
#if !__FINAL
		m_EventType = EVENT_ACQUAINTANCE_PED_LIKE;
#endif
	}
	virtual ~CEventAcquaintancePedLike(){}

	virtual int GetEventType() const {return EVENT_ACQUAINTANCE_PED_LIKE;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_LIKE;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventAcquaintancePedLike(m_pAcquaintancePed);} 

};

class CEventAcquaintancePedDislike : public CEventAcquaintancePed
{
public:

	CEventAcquaintancePedDislike(CPed* pAcquaintancePed)
		: CEventAcquaintancePed(pAcquaintancePed)
	{
#if !__FINAL
		m_EventType = EVENT_ACQUAINTANCE_PED_DISLIKE;
#endif
	}
	virtual ~CEventAcquaintancePedDislike(){}

	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual int GetEventType() const {return EVENT_ACQUAINTANCE_PED_DISLIKE;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_DISLIKE;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventAcquaintancePedDislike(m_pAcquaintancePed);} 

};

class CEventAcquaintancePedHate : public CEventAcquaintancePed
{
public:

	CEventAcquaintancePedHate(CPed* pAcquaintancePed)
		: CEventAcquaintancePed(pAcquaintancePed)
	{
#if !__FINAL
		m_EventType = EVENT_ACQUAINTANCE_PED_HATE;
#endif
	}
	virtual ~CEventAcquaintancePedHate(){}

	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual int GetEventType() const {return EVENT_ACQUAINTANCE_PED_HATE;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_HATE;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventAcquaintancePedHate(m_pAcquaintancePed);} 
};

class CEventAcquaintancePedDead : public CEventAcquaintancePed
{
public:

	CEventAcquaintancePedDead(CPed* pAcquaintancePed, bool bFirstHand)
		: CEventAcquaintancePed(pAcquaintancePed),
		  m_bFirstHand(bFirstHand)
	{
#if !__FINAL
		m_EventType = EVENT_ACQUAINTANCE_PED_DEAD;
#endif
	}
	virtual ~CEventAcquaintancePedDead(){}

	// I've put this here so that create response task gets called, may want to take it out
	// when a pass is done over the event system so that designers can change responses in rave
	virtual bool HasEditableResponse() const { return false; }
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual int GetEventType() const {return EVENT_ACQUAINTANCE_PED_DEAD;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_DEAD;}
	virtual bool CreateResponseTask( CPed* UNUSED_PARAM(pPed), CEventResponse& response ) const;
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventAcquaintancePedDead(m_pAcquaintancePed,m_bFirstHand);} 

private:

	bool m_bFirstHand;
};

//-------------------------------------------------------------------------
// Event generated when this ped sees a ped it considers as wanted, primarily
// used by cops
//-------------------------------------------------------------------------
class CEventAcquaintancePedWanted : public CEventAcquaintancePed
{
public:

	CEventAcquaintancePedWanted(CPed* pAcquaintancePed)
		: CEventAcquaintancePed(pAcquaintancePed)
	{
#if !__FINAL
		m_EventType = EVENT_ACQUAINTANCE_PED_WANTED;
#endif	
	}
	virtual ~CEventAcquaintancePedWanted() {}

	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual int GetEventType() const {return EVENT_ACQUAINTANCE_PED_WANTED;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_WANTED;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventAcquaintancePedWanted(m_pAcquaintancePed);} 
	virtual void OnEventAdded(CPed* pPed) const;
};

// A ped is encroaching (or has encroached) into our "personal space"
class CEventEncroachingPed : public CEventEditableResponse
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// Nothing here anymore, but having the tuning struct is probably good to leave in as we'll probably need it in the future...

		PAR_PARSABLE;
	};

	CEventEncroachingPed(CPed *pClosePed);
	virtual ~CEventEncroachingPed();

	virtual int GetEventType() const { return EVENT_ENCROACHING_PED; }
	virtual int GetEventPriority() const {return E_PRIORITY_PED_TO_FLEE;}
	virtual CEventEditableResponse *CloneEditable() const { return rage_new CEventEncroachingPed(m_pClosePed); }
	virtual bool AffectsPedCore(CPed* pPed) const;

	CPed *GetEncroachingPed() const { return m_pClosePed; }
	virtual CEntity* GetSourceEntity() const;

private:
	RegdPed m_pClosePed;

	static Tunables sm_Tunables;
};

class CEventScriptCommand : public CEvent
{
	friend class CEventGroup;
	friend class CEventHandler;
	friend class CInformFriendsEventQueue;
	friend class CInformGroupEventQueue;
	friend class CPedDebugVisualiserMenu;
	friend class CPedScriptedTaskRecord;
	friend class CScriptPeds;
	friend class CPed;
    friend class CNetObjPed;
	// Constructor only available to scripts, to prevent code generating these events,
	// code points can now use CEventGivePedTaskCommand
private:
	CEventScriptCommand(const int iPriority, aiTask* pTask, const bool bAcceptWhenDead=false);
	bool IsHighPriorityTask(const CTask* pTask) const;
	bool IsHighPriorityTaskNoNM(const CTask* pTask) const;
public:
	virtual  ~CEventScriptCommand();

	virtual int GetEventType() const {return EVENT_SCRIPT_COMMAND;}
	virtual int GetEventPriority() const;
	virtual CEvent* Clone() const;
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool IsValidForPed(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	inline virtual bool IsTemporaryEvent() const {return true;}
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual aiTask* CloneScriptTask() const;
	int GetTaskPriority() const {return m_iTaskPriority;}
	aiTask* GetTask() const {return m_pTask;}

#if !__NO_OUTPUT
	virtual atString GetDescription() const;
#endif // !__NO_OUTPUT

#if __DEV
	const char* GetScriptName() const { return m_ScriptName; }
	s32 GetScriptProgramCounter() const { return m_iProgramCounter; }
	void SetScriptNameAndCounter(const char* szScriptName, s32 iProgramCounter );
#endif
protected:
	int m_iTaskPriority;
	RegdaiTask m_pTask;
	bool m_bAcceptWhenDead;
public:
#if __DEV
	char m_ScriptName[32];
	s32 m_iProgramCounter;
#endif
};

class CEventGivePedTask : public CEvent
{
public:
	friend class CEventGroup;
	friend class CEventHandler;
	friend class CInformFriendsEventQueue;
	friend class CInformGroupEventQueue;

	CEventGivePedTask(const int iPriority, aiTask* pTask, 
		const bool bAcceptWhenDead=false,const 
		int iCustomEventPriority = E_PRIORITY_GIVE_PED_TASK, 
		const bool bRequiresAbortForRagdoll = false,
		const bool bDeleteActiveTask = false,
		const bool bBlockWhenInRagdoll = false);
	virtual  ~CEventGivePedTask();

	virtual int GetEventType() const {return EVENT_GIVE_PED_TASK;}
	virtual int GetEventPriority() const {return m_iEventPriority;}
	virtual CEvent* Clone() const;
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool IsValidForPed(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	virtual bool CanBeInterruptedBySameEvent() const {return true;}
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool RequiresAbortForMelee(const CPed* pPed) const;
	virtual bool RequiresAbortForRagdoll() const { return m_bRequiresAbortForRagdoll; }
	inline virtual bool IsTemporaryEvent() const { return m_iTaskPriority == PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP; }
	//virtual int GetEventPriority() const;

	int GetTaskPriority() const {return m_iTaskPriority;}
	virtual aiTask* CloneTask() const;
	aiTask* GetTask() const {return m_pTask;}
	bool GetAcceptWhenDead() const {return m_bAcceptWhenDead;}
	bool CanDeleteActiveTask() const {return m_bDeleteActiveTask;}

	void SetTunableDelay(float fMin, float fMax) { m_fMinDelay = fMin; m_fMaxDelay = fMax; }

private:

	virtual bool GetTunableDelay(const CPed* pPed, float& fMin, float& fMax) const;

protected:
	float m_fMinDelay;
	float m_fMaxDelay;
	int m_iTaskPriority;
	int m_iEventPriority;
	RegdaiTask m_pTask;
	bool m_bAcceptWhenDead;
	bool m_bRequiresAbortForRagdoll;
	bool m_bDeleteActiveTask;
	bool m_bBlockWhenInRagdoll;
};


//The ped is in the air.
class CEventInAir : public CEvent
{
public:
	CEventInAir(CPed* pPed);
	virtual ~CEventInAir();

	virtual int GetEventType() const {return EVENT_IN_AIR;}
	virtual int GetEventPriority() const {return E_PRIORITY_IN_AIR;}
	virtual CEvent* Clone() const;
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool IsTemporaryEvent() const {return true;}
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_FALL; }
	virtual CEntity* GetSourceEntity() const {return (CEntity*)m_pPed.Get();}
	virtual bool RequiresAbortForMelee(const CPed* UNUSED_PARAM(pPed)) const {return true;}	

	bool GetDisableSkydiveTransition() const { return m_bDisableSkydiveTransition; }
	void SetDisableSkydiveTransition(bool bValue) { m_bDisableSkydiveTransition = bValue; }

	bool GetCheckForDive() const { return m_bCheckForDive; }
	void SetCheckForDive(bool bValue) { m_bCheckForDive = bValue; }

	bool GetForceNMFall() const { return m_bForceNMFall; }
	void SetForceNMFall(bool bValue) { m_bForceNMFall = bValue; }

	bool GetForceInterrupt() const { return m_bForceInterrupt; }
	void SetForceInterrupt(bool bValue) { m_bForceInterrupt = bValue; }

private:

	bool CanInterruptNMForParachute() const;
	bool IsLandingWithParachute() const;
	
private:

	virtual bool CanBeInterruptedBySameEvent() const;
	virtual bool HasExpired() const;
	virtual bool RequiresAbortForRagdoll() const;
	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;

private:
	RegdPed m_pPed;
	bool	m_bDisableSkydiveTransition;
	bool	m_bCheckForDive;
	bool	m_bForceNMFall;
	bool	m_bForceInterrupt;
};

class CEventMustLeaveBoat : public CEvent
{
public:
	CEventMustLeaveBoat(CBoat * pBoat);
	virtual ~CEventMustLeaveBoat();

	virtual int GetEventType() const {return EVENT_MUST_LEAVE_BOAT;}
	virtual int GetEventPriority() const {return E_PRIORITY_MUST_LEAVE_BOAT;}
	virtual CEvent* Clone() const {return rage_new CEventMustLeaveBoat(m_pBoat);}
	virtual bool AffectsPedCore(CPed*) const;
	bool IsValidForPed(CPed* pPed) const;
	inline virtual bool IsTemporaryEvent() const {return true;}
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_COMPLEX_GET_OFF_BOAT; }

	RegdBoat m_pBoat;

private:
};

//Ped on fire.

class CEventOnFire : public CEvent
{
public:
	CEventOnFire(u32 weaponHash) : CEvent(), m_weaponHash(weaponHash)
	{
#if !__FINAL
		m_EventType = EVENT_ON_FIRE;
#endif
	}
	virtual ~CEventOnFire(){}

	virtual int GetEventType() const {return EVENT_ON_FIRE;}
	virtual int GetEventPriority() const {return E_PRIORITY_ON_FIRE;}
	virtual CEvent* Clone() const {return rage_new CEventOnFire(m_weaponHash);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_COMPLEX_ON_FIRE; }

	//Get the potential weapon that started this event on fire - 0 if unknown.
	u32 GetWeaponHash() const {return m_weaponHash;}

protected:
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;

private:
	u32 m_weaponHash; //weapon hash that caused the fire.
};

//-------------------------------------------------------------------------
// Cry for help event, created by a ped who needs help from its respected 
// friends, peds will not necessarily respond to this request
//-------------------------------------------------------------------------
class CEventCallForCover : public CEventEditableResponse
{
public:
	CEventCallForCover( CPed* pPedCallingForCover);
	virtual ~CEventCallForCover();

	virtual int GetEventType() const {return EVENT_CALL_FOR_COVER;}
	virtual int GetEventPriority() const {return E_PRIORITY_CALL_FOR_COVER;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CEntity* GetSourceEntity() const {return (CEntity*)m_pPedCallingForCover.Get();}
	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventCallForCover( m_pPedCallingForCover ); }

	CPed* GetPedCallingForCover() const { return m_pPedCallingForCover; }

private:
	RegdPed m_pPedCallingForCover;
};

class CEventProvidingCover : public CEventEditableResponse
{
public:
	CEventProvidingCover( CPed* pPedProvidingCover);
	virtual ~CEventProvidingCover();

	virtual int GetEventType() const {return EVENT_PROVIDING_COVER;}
	virtual int GetEventPriority() const {return E_PRIORITY_PROVIDING_COVER;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CEntity* GetSourceEntity() const {return (CEntity*)m_pPedProvidingCover.Get();}
	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventProvidingCover( m_pPedProvidingCover ); }

	CPed* GetPedProvidingCover() const { return m_pPedProvidingCover; }

private:
	RegdPed m_pPedProvidingCover;
};

//-------------------------------------------------------------------------
// Request help from buddies
//-------------------------------------------------------------------------
class CEventRequestHelp : public CEventEditableResponse
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_MaxRangeWithoutRadioForFistFights;
		float m_MinDelayTimer;
		float m_MaxDelayTimer;

		PAR_PARSABLE;
	};

public:

	CEventRequestHelp(CPed* pPedThatNeedsHelp, CPed* pTarget);
	virtual ~CEventRequestHelp();
	
public:

	CPed* GetPedThatNeedsHelp() const { return m_pPedThatNeedsHelp; }
	CPed* GetTarget()			const { return m_pTarget; }
	
public:

	virtual bool		AffectsPedCore(CPed* pPed) const;
	virtual void		CommunicateEvent(CPed* pPed, const CommunicateEventInput& rInput) const;
	virtual CEntity*	GetSourceEntity() const { return (CEntity*)m_pPedThatNeedsHelp.Get(); }
	virtual CEntity*	GetTargetEntity() const { return (CEntity*)m_pTarget.Get(); }
	
private:
	
	bool IsPedArmedWithGun(const CPed* pPed) const;

private:

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const { fMin = sm_Tunables.m_MinDelayTimer; fMax = sm_Tunables.m_MaxDelayTimer; return true; }

protected:

	RegdPed m_pPedThatNeedsHelp;
	RegdPed m_pTarget;

private:

	static Tunables sm_Tunables;
	
};

//-------------------------------------------------------------------------
// Cry for help event, created by a ped who needs help from its respected 
// friends, peds will not necessarily respond to this request
//-------------------------------------------------------------------------
class CEventInjuredCryForHelp : public CEventRequestHelp
{
public:
	CEventInjuredCryForHelp( CPed* pInjuredPed, CPed* pTarget);
	virtual ~CEventInjuredCryForHelp();

	virtual int GetEventType() const {return EVENT_INJURED_CRY_FOR_HELP;}
	virtual int GetEventPriority() const {return E_PRIORITY_INJURED_CALL_FOR_HELP;}
	virtual bool AffectsPedCore(CPed* pPed) const;

	virtual CEventEditableResponse* CloneEditable() const
	{
		CEventInjuredCryForHelp* pEvent = rage_new CEventInjuredCryForHelp( m_pPedThatNeedsHelp, m_pTarget );
		pEvent->SetIgnoreIfLaw(m_bIgnoreIfLaw);

		return pEvent;
	}

	void SetIgnoreIfLaw(bool bIgnoreIfLaw) { m_bIgnoreIfLaw = bIgnoreIfLaw; }

private:

	bool m_bIgnoreIfLaw;
};

class CEventRequestHelpWithConfrontation : public CEventRequestHelp
{
public:
	CEventRequestHelpWithConfrontation(CPed* pPedThatNeedsHelp, CPed* pTarget);
	virtual ~CEventRequestHelpWithConfrontation();

	atHashWithStringNotFinal GetAgitatedContext() const;

	virtual int GetEventType() const {return EVENT_REQUEST_HELP_WITH_CONFRONTATION;}
	virtual int GetEventPriority() const {return E_PRIORITY_REQUEST_HELP_WITH_CONFRONTATION;}

	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventRequestHelpWithConfrontation(m_pPedThatNeedsHelp, m_pTarget); }
	
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

private:

	bool IsFriendlyWithPedThatNeedsHelp(const CPed& rPed) const;
	bool IsIntervening(const CPed& rPed) const;
	bool ShouldIntervene(const CPed& rPed) const;

};

//----------------------------------------------------------------------------
// Cry for help event, caused by peds being the victim of some crime.
//----------------------------------------------------------------------------
class CEventCrimeCryForHelp : public CEventEditableResponse
{

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_MaxDistance;
		float m_MinDelayDistance;
		float m_MaxDelayDistance;
		float m_MaxDistanceDelay;
		float m_MinRandomDelay;
		float m_MaxRandomDelay;

		PAR_PARSABLE;
	};

public:
	CEventCrimeCryForHelp( const CPed* pAssaultedPed, const CPed* pAssaultingPed);
	virtual ~CEventCrimeCryForHelp();

	virtual int GetEventType() const { return EVENT_CRIME_CRY_FOR_HELP; }
	virtual int GetEventPriority() const { return E_PRIORITY_CRIME_CRY_FOR_HELP; }

	virtual bool AffectsPedCore(CPed* pPed) const;

	//the aggressor ped.
	virtual CEntity* GetSourceEntity() const { return (CEntity*)m_pAssaultingPed.Get(); }

	//the victim ped
	virtual CEntity* GetOtherEntity() const { return (CEntity*)m_pAssaultingPed.Get(); }

	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventCrimeCryForHelp( m_pAssaultedPed, m_pAssaultingPed); }

	virtual bool GetTunableDelay(const CPed* pPed, float& fMin, float& fMax) const;

	virtual bool operator==(const fwEvent& rOtherEvent) const;

	static void CreateAndCommunicateEvent(CPed* pPed, const CPed* pAggressor);

private:
	RegdConstPed m_pAssaultedPed;
	RegdConstPed m_pAssaultingPed;

public:

	static Tunables	sm_Tunables;

};

//----------------------------------------------------------------------------
// Respond to your friend getting attacked.
// Used for dog-owner relations.
//----------------------------------------------------------------------------
class CEventHelpAmbientFriend : public CEvent
{
public:

	enum eHelpAmbientReaction
	{
		HAR_FOLLOW = 0,
		HAR_FLEE,
		HAR_THREAT,
	};

	CEventHelpAmbientFriend(const CPed* pFriend, const CPed* pAssaultingPed, eHelpAmbientReaction iResponseLevel);
	virtual ~CEventHelpAmbientFriend() {}

	virtual CEvent*	Clone() const {return rage_new CEventHelpAmbientFriend(m_pFriend, m_pAssaultingPed, m_iResponseLevel);}

	virtual int GetEventType() const { return EVENT_HELP_AMBIENT_FRIEND; }
	virtual int GetEventPriority() const { return E_PRIORITY_HELP_AMBIENT_FRIEND; }

	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CanBeInterruptedBySameEvent() const { return true; } 
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;

	// Who you are helping.
	virtual CEntity* GetSourceEntity() const { return (CEntity*)m_pFriend.Get(); }

	// The threat to the ped you are helping, if any.
	virtual CEntity* GetOtherEntity() const { return (CEntity*)m_pAssaultingPed.Get(); }

	virtual bool operator==(const fwEvent& rOtherEvent) const;

	eHelpAmbientReaction GetHelpResponse() const { return m_iResponseLevel; }
	static eHelpAmbientReaction DetermineNewHelpResponse(CPed& responderPed, CPed& friendPed, CEvent* friendEvent);

private:
	RegdConstPed m_pFriend;
	RegdConstPed m_pAssaultingPed;
	eHelpAmbientReaction m_iResponseLevel;
};

//-------------------------------------------------------------------------
// Cry for help event, created by a ped who needs help from its respected 
// friends, peds will not necessarily respond to this request
//-------------------------------------------------------------------------
class CEventShoutTargetPosition : public CEventRequestHelp
{
public:
	CEventShoutTargetPosition( CPed* pPedShouting, CPed* pTarget );
	virtual ~CEventShoutTargetPosition();

	virtual int GetEventType() const {return EVENT_SHOUT_TARGET_POSITION;}
	virtual int GetEventPriority() const {return E_PRIORITY_SHOUT_TARGET_POSITION;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;
	virtual void OnEventAdded(CPed* pPed) const;
	
	virtual CEventEditableResponse* CloneEditable() const { return rage_new CEventShoutTargetPosition( m_pPedThatNeedsHelp, m_pTarget ); }
};

//-------------------------------------------------------------------------
// Radio for help event, created by a ped who needs help from its respected 
// friends, peds will not necessarily respond to this request
//-------------------------------------------------------------------------
class CEventRadioTargetPosition : public CEvent
{
public:
	CEventRadioTargetPosition( CPed* pPedRadioing, const CAITarget& pTarget);
	virtual ~CEventRadioTargetPosition();

	virtual int GetEventType() const {return EVENT_RADIO_TARGET_POSITION;}
	virtual int GetEventPriority() const {return E_PRIORITY_RADIO_TARGET_POSITION;}
	virtual CEvent* Clone() const { return rage_new CEventRadioTargetPosition( m_pPedRadioing, m_pTarget ); }
	//virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventRadioTargetPosition(m_pPedRadioing,m_pTarget);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool	CreateResponseTask(CPed* pPed, CEventResponse& response) const;

private:
	RegdPed		m_pPedRadioing;
	CAITarget	m_pTarget;
};

//-------------------------------------------------------------------------
// Shout event for when a ped blocks another ped's los
//-------------------------------------------------------------------------
class CEventShoutBlockingLos : public CEvent
{
public:
	CEventShoutBlockingLos( CPed* pPedShouting, CEntity* pTarget );
	virtual ~CEventShoutBlockingLos();

	virtual int GetEventType() const {return EVENT_SHOUT_BLOCKING_LOS;}
	virtual int GetEventPriority() const {return E_PRIORITY_SHOUT_BLOCKING_LOS;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CEntity* GetSourceEntity() const {return (CEntity *)m_pPedShouting.Get();}
	virtual CEntity* GetTargetEntity() const {return m_pTarget;}
	
	virtual CEvent* Clone() const { return rage_new CEventShoutBlockingLos( m_pPedShouting, m_pTarget ); }
	
	CPed* GetPedShouting() const { return m_pPedShouting; }

private:
	RegdPed		m_pPedShouting;
	RegdEnt		m_pTarget;
};

class CEventVehicleOnFire : public CEventEditableResponse
{
public:

	CEventVehicleOnFire(CVehicle* pVehicleOnFire);
	virtual ~CEventVehicleOnFire();

	virtual int GetEventType() const {return EVENT_VEHICLE_ON_FIRE;}
	virtual int GetEventPriority() const {return E_PRIORITY_VEHICLE_ON_FIRE;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventVehicleOnFire(m_pVehicleOnFire);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	inline virtual bool IsTemporaryEvent() const {return true;}

	CVehicle* GetVehicleOnFire() const {return m_pVehicleOnFire;}

private:

	RegdVeh m_pVehicleOnFire;

};

class CEventInWater : public CEvent
{
public:
	CEventInWater(float fBuoyancyFraction=0.75f);
	virtual ~CEventInWater();

	virtual int GetEventType() const {return EVENT_IN_WATER;}
	virtual int GetEventPriority() const {return E_PRIORITY_IN_WATER;}
	virtual CEvent* Clone() const {return rage_new CEventInWater(m_fBuoyancyFraction);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool RequiresAbortForMelee(const CPed* UNUSED_PARAM(pPed)) const {return true;}	

	float GetBuoyancyFraction() {return m_fBuoyancyFraction;}

private:
	float m_fBuoyancyFraction;
};

class CEventGetOutOfWater : public CEvent
{
public:
	CEventGetOutOfWater();
	virtual ~CEventGetOutOfWater();

	virtual int GetEventType() const {return EVENT_GET_OUT_OF_WATER;}
	virtual int GetEventPriority() const {return E_PRIORITY_GET_OUT_OF_WATER;}
	virtual CEvent* Clone() const {return rage_new CEventGetOutOfWater();}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual bool IsTemporaryEvent() const {return true;}
};

class CEventStuckInAir : public CEvent
{
public:
	CEventStuckInAir(CPed *pPed);
	virtual ~CEventStuckInAir();

	virtual int GetEventType() const {return EVENT_STUCK_IN_AIR;}
	virtual int GetEventPriority() const;
	virtual CEvent* Clone() const {return rage_new CEventStuckInAir(m_pPed);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	virtual bool CanBeInterruptedBySameEvent() const {return true;}
	virtual bool IsTemporaryEvent() const {return true;}
	virtual CTaskTypes::eTaskType GetResponseTaskType() const { return CTaskTypes::TASK_COMPLEX_STUCK_IN_AIR; }

	enum
	{
		NONE = 0,
		ABORT_TASK,
		SET_STANDING,
		FALL_DOWN
	};
private:

	// this event needs a pointer to it's ped so that it can calculate it's priority!
	RegdPed m_pPed;
};

class CEventCopCarBeingStolen : public CEvent
{
public:

	CEventCopCarBeingStolen(CPed* pCriminal, CVehicle* pTargetVehicle);
	virtual ~CEventCopCarBeingStolen();

	virtual int GetEventType() const {return EVENT_COP_CAR_BEING_STOLEN;}
	virtual int GetEventPriority() const {return E_PRIORITY_COP_CAR_BEING_STOLEN;}
	virtual CEvent* Clone() const {return rage_new CEventCopCarBeingStolen(m_pCriminal, m_pTargetVehicle);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	CPed* GetCriminal() const {return m_pCriminal;}
	CVehicle* GetTargetVehicle() const {return m_pTargetVehicle;}

private:

	RegdPed	m_pCriminal;
	RegdVeh	m_pTargetVehicle;
};

class CEventVehicleDamage : public CEventEditableResponse
{
public:

	CEventVehicleDamage(CVehicle* pVehicle, CEntity* pInflictor, u32 nWeaponUsedHash, const float fDamageCaused, const Vector3& vDamagePos, const Vector3& vDamageDir);
	virtual ~CEventVehicleDamage();

	virtual int GetEventPriority() const {return E_PRIORITY_VEHICLE_DAMAGE;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	// Used by certain events to make sure duplicate events aren't added
	virtual bool operator==( const fwEvent& otherEvent ) const;
	virtual CEntity* GetSourceEntity() const;

	virtual CPed* GetPlayerCriminal() const;

	CVehicle* GetVehicle() const {return m_pVehicle;}
	CEntity* GetInflictor() const {return m_pInflictor;}
	u32 GetWeaponUsed() const {return m_nWeaponUsedHash;}

	float GetDamageCaused() const {return m_fDamageCaused;}
	const Vector3& GetDamagePos() const {return m_vDamagePos;}
	const Vector3& GetDamageDir() const {return m_vDamageDir;}

protected:

	RegdVeh m_pVehicle;
	RegdEnt m_pInflictor;
	u32	m_nWeaponUsedHash;
	float	m_fDamageCaused;
	Vector3 m_vDamagePos;
	Vector3 m_vDamageDir;
};

class CEventVehicleDamageWeapon : public CEventVehicleDamage
{
public:

	CEventVehicleDamageWeapon(CVehicle* pVehicle, CEntity* pInflictor, u32 nWeaponUsedHash, const float fDamageCaused, const Vector3& vDamagePos, const Vector3& vDamageDir)
	: CEventVehicleDamage(pVehicle, pInflictor, nWeaponUsedHash, fDamageCaused, vDamagePos, vDamageDir) 
	, m_fSpeedOfContinuousCollider(0.0f)
	, m_bIsContinuouslyColliding(false)
	{
#if !__FINAL
		m_EventType = EVENT_VEHICLE_DAMAGE_WEAPON;
#endif
	}
	virtual ~CEventVehicleDamageWeapon() {}

	virtual int GetEventType() const {return EVENT_VEHICLE_DAMAGE_WEAPON;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool ShouldCreateResponseTaskForPed(const CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual CEventEditableResponse* CloneEditable() const
	{
		CEventVehicleDamageWeapon* pEvent = rage_new CEventVehicleDamageWeapon(m_pVehicle, m_pInflictor, m_nWeaponUsedHash, m_fDamageCaused, m_vDamagePos, m_vDamageDir);
		pEvent->SetIsContinuouslyColliding(m_bIsContinuouslyColliding, m_fSpeedOfContinuousCollider);
		return pEvent;
	}

	void SetIsContinuouslyColliding(bool bValue, float fSpeed) { m_bIsContinuouslyColliding = bValue; m_fSpeedOfContinuousCollider = fSpeed; }

private:

	float m_fSpeedOfContinuousCollider;
	bool m_bIsContinuouslyColliding;

};

//Car upsidedown
class CEventCarUndriveable : public CEvent
{
public:

	CEventCarUndriveable(CVehicle* pVehicle);
	virtual ~CEventCarUndriveable();

	virtual int GetEventType() const {return EVENT_CAR_UNDRIVEABLE;}
	virtual int GetEventPriority() const {return E_PRIORITY_CAR_UPSIDE_DOWN;}
	virtual CEvent* Clone() const {return rage_new CEventCarUndriveable(m_pVehicle);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool IsTemporaryEvent() const { return m_bIsTemporaryEvent; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	CVehicle* GetVehicle() const {return m_pVehicle;}

	static bool ShouldPedUseFleeResponseDueToStuck(const CPed* pPed, const CVehicle* pVehicle);

	static dev_u16 ms_uMinStuckTimeMS;

private:

	RegdVeh m_pVehicle;
	bool	m_bIsTemporaryEvent;

};

//Fire nearby
class CEventFireNearby : public CEventEditableResponse
{
public:

	CEventFireNearby(const Vector3& vFirePos);
	virtual ~CEventFireNearby();

	virtual int GetEventType() const {return EVENT_FIRE_NEARBY;}
	virtual int GetEventPriority() const {return E_PRIORITY_FIRE_NEARBY;}
	virtual CEventEditableResponse* CloneEditable() const {return rage_new CEventFireNearby(m_vFirePos);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent&  otherEvent) const;
	inline virtual bool IsTemporaryEvent() const {return true;}
	const Vector3& GetFirePos() const {return m_vFirePos;}

private:

	Vector3 m_vFirePos;

};


//Ped on car roof.
class CEventPedOnCarRoof : public CEvent
{
public:

	CEventPedOnCarRoof(CPed* pPedOnRoof, CVehicle* pVehicle);
	virtual ~CEventPedOnCarRoof();

	virtual int GetEventType() const {return EVENT_PED_ON_CAR_ROOF;}
	virtual int GetEventPriority() const {return E_PRIORITY_PED_ON_CAR_ROOF;}
	virtual CEvent* Clone() const {return rage_new CEventPedOnCarRoof(m_pPed,m_pVehicle);}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CEntity* GetSourceEntity() const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	CVehicle* GetVehicle() const {return m_pVehicle;}

private:

	RegdPed m_pPed;
	RegdVeh	m_pVehicle;
};


// Ped has switched to Ragdoll
class CEventSwitch2NM : public CEvent
{
public:
	CEventSwitch2NM(u32 nMaxTime, aiTask* pTaskNM, bool bScriptEvent = false, u32 nMinTime = 1000);
	virtual ~CEventSwitch2NM();

	virtual int GetEventType() const {return EVENT_SWITCH_2_NM_TASK;}
	virtual int GetEventPriority() const {
		if (m_bRagdollFinished)
		{
			return E_PRIORITY_GETUP;
		}
		else
		{
			return m_bScriptEvent ? E_PRIORITY_SWITCH_2_NM_SCRIPT : E_PRIORITY_SWITCH_2_NM;
		}

	}
#if __CATCH_EVENT_LEAK
	virtual CEvent* Clone() const
	{
		CEventSwitch2NM * pEvent = rage_new CEventSwitch2NM(m_nMaxTime, CloneTaskNM(), m_bScriptEvent);
		if(ms_bCatchEventLeak)
		{
			if(pEvent->m_pCreationStackTrace)
				StringFree(pEvent->m_pCreationStackTrace);
			pEvent->m_pCreationStackTrace = StringDuplicate(m_pCreationStackTrace);
		}
		return pEvent;
	}
#else
	virtual CEvent* Clone() const {return rage_new CEventSwitch2NM(m_nMaxTime, CloneTaskNM(), m_bScriptEvent);}
#endif
	virtual bool operator==(const fwEvent& otherEvent) const;
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool RequiresAbortForRagdoll() const {return true;}
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual bool CanBeInterruptedBySameEvent() const {return true;}
	inline virtual bool IsTemporaryEvent() const {return true;}

	virtual void ClearRagdollResponse() { m_bRagdollFinished = true; }

	// Used by the task counter only - const pointer
	const aiTask* GetTaskNM() const {return m_pTaskNM;}
	aiTask* GetTaskNM() {return m_pTaskNM;}
	// Use to be able to actually use the task
	aiTask* CloneTaskNM() const;

	u32 GetMinTime() const {return m_nMinTime;}
	u32 GetMaxTime() const {return m_nMaxTime;}

	virtual CTaskTypes::eTaskType GetResponseTaskType() const 
	{ 
		return m_pTaskNM ? (CTaskTypes::eTaskType)(m_pTaskNM->GetTaskType()) : CTaskTypes::TASK_INVALID_ID;
	}

	bool GetIsScriptEvent() const { return m_bScriptEvent; }

#if __CATCH_EVENT_LEAK
	static bool ms_bCatchEventLeak;
	char * m_pCreationStackTrace;
#endif

private:
	u32 m_nMinTime;
	u32 m_nMaxTime;
	RegdaiTask m_pTaskNM;
	// Generated from script, making it higher priority.
	bool	m_bScriptEvent;
	// This is set to true on switching back to animated, allowing us to lower the event priority at that point
	// This allows other damage events to override it whilst playing a getup animation, etc.
	bool	m_bRagdollFinished;
};


//-------------------------------------------------------------------------
// Event used in MakeAbortable to allow tasks to be aware
// of when they are being replaced with new tasks
//-------------------------------------------------------------------------
class CEventNewTask : public CEvent
{
public:
	CEventNewTask(aiTask* pNewTask, s32 iPriority, bool bRagdollResponse = false)
		: m_pNewTask(pNewTask), m_iPriority(iPriority), m_bRagdollResponse(bRagdollResponse)
	{
#if !__FINAL
		m_EventType = EVENT_NEW_TASK;
#endif
	}
	virtual ~CEventNewTask() {}

	virtual int GetEventType() const {return EVENT_NEW_TASK;}
	virtual int GetEventPriority() const {return m_iPriority;}
	virtual CEvent* Clone() const {return rage_new CEventNewTask(m_pNewTask, m_iPriority);}
	virtual bool AffectsPedCore(CPed* UNUSED_PARAM(pPed) ) const {Assertf(0, "Not implemented, see PHIL");return false;}

	aiTask* GetNewTask() const { return m_pNewTask; }

	virtual bool RequiresAbortForRagdoll() const {return m_bRagdollResponse;}
	virtual bool CanBeInterruptedBySameEvent() const {return true;}    

private:
	bool m_bRagdollResponse;

	RegdaiTask m_pNewTask;
	s32	m_iPriority;
};


//-------------------------------------------------------------------------
// Event used in MakeAbortable to allow tasks to be aware
// of when they are being replaced with new tasks
//-------------------------------------------------------------------------
class CEventDummyConversion : public CEvent
{
public:
	CEventDummyConversion()
	{
#if !__FINAL
		m_EventType = EVENT_DUMMY_CONVERSION;
#endif
	}
	virtual ~CEventDummyConversion() {}

	virtual int GetEventType() const {return EVENT_DUMMY_CONVERSION;}
	virtual int GetEventPriority() const {return 0;}
	virtual CEvent* Clone() const {return rage_new CEventDummyConversion();}
	virtual bool AffectsPedCore(CPed* UNUSED_PARAM(pPed) ) const {Assertf(0, "Not implemented, see PHIL");return false;}
	virtual bool CanBeInterruptedBySameEvent() const {return true;}    
};

//-------------------------------------------------------------------------
// Event used in MakeAbortable to allow tasks to be aware
// of when they are being replaced with new tasks
//-------------------------------------------------------------------------
class CEventFlushTasks : public CEvent
{
public:
	CEventFlushTasks( s32 iPriority )
		: m_iPriority(iPriority)
	{
#if !__FINAL
		m_EventType = EVENT_FLUSH_TASKS;
#endif
	}
	virtual ~CEventFlushTasks() {}

	virtual int GetEventType() const {return EVENT_FLUSH_TASKS;}
	virtual int GetEventPriority() const {return m_iPriority;}
	virtual CEvent* Clone() const {return rage_new CEventFlushTasks( m_iPriority);}
	virtual bool AffectsPedCore(CPed* UNUSED_PARAM(pPed) ) const {Assertf(0, "Not implemented, see PHIL");return false;}
	virtual bool CanBeInterruptedBySameEvent() const {return true;}    

private:

	s32	m_iPriority;
};


//-------------------------------------------------------------------------
// Event generated by various movement tasks if the ped is supposed to be
// moving, but it actually staying on the spot.  The response will be to
// make the ped jump backwards or in some way alter their position.
//-------------------------------------------------------------------------
class CEventStaticCountReachedMax : public CEvent
{
public:
	CEventStaticCountReachedMax(int iMovementTaskType);
	virtual ~CEventStaticCountReachedMax(void);

	inline virtual int GetEventType(void) const { return EVENT_STATIC_COUNT_REACHED_MAX; }
	virtual int GetEventPriority(void) const {return E_PRIORITY_STATIC_COUNT_REACHED_MAX;}
	inline virtual CEvent* Clone(void) const { return rage_new CEventStaticCountReachedMax(m_iMovementTaskType); }
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	inline s32 GetMovementTaskType(void) const { return m_iMovementTaskType; }

	inline virtual bool CanBeInterruptedBySameEvent() const { return false; }
	inline virtual bool IsTemporaryEvent() const {return true;}

#if __BANK
	static bool ms_bDebugClimbAttempts;
#endif

private:

	bool TestForClimb(CPed * pPed, float & fOut_HeadingToClimb) const;
	CTaskNavBase * FindNavTask(CPed * pPed, Vector3 & vOut_FirstRouteTarget, float & fOut_HeadingToTarget, Matrix34 & mOut_ClimbMat) const;
	bool TestCapsule(const Vector3 & v1, const Vector3 & v2, float fRadius) const;
	CPhysical * IsStuckInsideProp(CPed * pPed) const;
	const CPhysical* IsStuckOnVehicle(const CPed * pPed) const;

	s32 m_iMovementTaskType;
};

class CEventSuspiciousActivity : public CEvent
{
public:
	struct Tunables : public CTuning
	{
		Tunables();

		// Minimum distance for two CEventSuspiciousActivity made by a ped to be comibined.
		float fMinDistanceToBeConsideredSameEvent;

		PAR_PARSABLE;
	};

	CEventSuspiciousActivity( CPed* pPed, const Vector3 &vPosition, const float fRange);
	~CEventSuspiciousActivity();

	inline virtual CEvent* Clone(void) const { return rage_new CEventSuspiciousActivity(m_pPed, m_vPosition, m_fRange); }

	inline virtual int GetEventType(void) const { return EVENT_SUSPICIOUS_ACTIVITY; }
	virtual int GetEventPriority(void) const {return E_PRIORITY_SUSPICIOUS_ACTIVITY;}
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	virtual bool CanBeInterruptedBySameEvent() const { return true; }

	bool TryToCombine(const CEventSuspiciousActivity& other);

private:

	RegdPed		m_pPed; //Originating ped, used for determining Friendliness
	Vector3		m_vPosition;
	float			m_fRange;

	static Tunables sm_Tunables;
};

class CEventUnidentifiedPed : public CEvent
{
public:
	CEventUnidentifiedPed(CPed* pPed);
	~CEventUnidentifiedPed();

	virtual CPed* GetSourcePed() const { return m_pUnidentifiedPed; }
	inline virtual CEvent* Clone(void) const { return rage_new CEventUnidentifiedPed(m_pUnidentifiedPed); }

	inline virtual int GetEventType(void) const { return EVENT_UNIDENTIFIED_PED; }
	virtual int GetEventPriority(void) const {return E_PRIORITY_UNIDENTIFIED_PED;}
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
private:
	RegdPed		m_pUnidentifiedPed;
};


class CEventDeadPedFound : public CEvent
{
public:
	CEventDeadPedFound(CPed* pPed, bool bWitnessedFirstHand);
	~CEventDeadPedFound() {};

	// Required pure virtual CEvent functions
	virtual CEvent* Clone() const { return rage_new CEventDeadPedFound(m_pDeadPed,m_bWitnessedFirstHand); }
	virtual int GetEventType(void) const	 { return EVENT_DEAD_PED_FOUND; }
	virtual int GetEventPriority(void) const { return E_PRIORITY_PED_DEAD; }
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual CPed* GetSourcePed() const		 { return m_pDeadPed; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;

	// Returns whether the event was witnessed by this ped, false if the event
	// was passed onto this ped by a friendly ped
	bool GetWitnessedFirstHand() const { return m_bWitnessedFirstHand; }

	// Clone the event with specific witnessed parameter
	CEventDeadPedFound* CloneEventSpecific(bool bWitnessedFirstHand) const;
private:
	RegdPed		m_pDeadPed;
	// Has ped witnessed the event, or told about it
	bool m_bWitnessedFirstHand;
};

class CEventAgitated : public CEventEditableResponse
{
public:

	struct Tunables : public CTuning
	{
		Tunables();

		// PURPOSE:  How long in seconds this agitation remains valid for.
		u32		m_TimeToLive;

		// PURPOSE: Min time until the ambient event grows stale.
		float	m_AmbientEventLifetime;

		// PURPOSE: Chances of triggering an ambient reaction.
		float	m_TriggerAmbientReactionChances;

		// PURPOSE: Min distance for the ambient reaction.
		float	m_MinDistanceForAmbientReaction;

		// PURPOSE: Max distance for the ambient reaction.
		float	m_MaxDistanceForAmbientReaction;

		// PURPOSE: Min time to play the ambient reaction.
		float	m_MinTimeForAmbientReaction;

		// PURPOSE: Max time to play the ambient reaction.
		float	m_MaxTimeForAmbientReaction;

		// PURPOSE:  Which kind of AmbientEvent this shocking event can create.
		AmbientEventType m_AmbientEventType;


		PAR_PARSABLE;
	};

	CEventAgitated(CPed* pAgitator, AgitatedType nType);
	~CEventAgitated();

	// Required pure virtual CEvent functions
	virtual CEventEditableResponse* CloneEditable() const;
	virtual int GetEventType(void) const	 { return EVENT_AGITATED; }
	virtual int GetEventPriority() const;
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool TakesPriorityOver(const CEvent& otherEvent) const;
	virtual CPed* GetSourcePed() const { return m_pAgitator; }
	
	CPed* GetAgitator() const { return m_pAgitator; }
	AgitatedType GetType() const { return m_nType; }

	void SetMaxDistance(float fDistance) { m_fMaxDistance = fDistance; }

	//do the logical checks to see if the referenced ped can get an ambient reaction
	bool ShouldTriggerAmbientReaction(const CPed& rPed) const;

	//assumes that ShouldTriggerAmbientReaction was called first
	void TriggerAmbientReaction(const CPed& rPed) const;

	bool ShouldSetFriendlyException(const CPed& rPed) const;

	bool ShouldIgnore(const CPed& rPed) const;
	
private:

	RegdPed			m_pAgitator;
	AgitatedType	m_nType;
	u32				m_uTimeStamp;
	float			m_fMaxDistance;
	
public:

	static Tunables	sm_Tunables;
	
};

class CEventAgitatedAction : public CEvent
{
public:
	CEventAgitatedAction(const CPed* pAgitator, aiTask* pTask);
	~CEventAgitatedAction();
	
public:

	const CPed*		GetAgitator()	const { return m_pAgitator; }
	const aiTask*	GetTask()		const { return m_pTask; }

public:

	// Required pure virtual CEvent functions
	virtual CEvent* Clone() const { return rage_new CEventAgitatedAction(m_pAgitator, m_pTask ? m_pTask->Copy() : NULL); }
	virtual int GetEventType(void) const	 { return EVENT_AGITATED_ACTION; }
	virtual int GetEventPriority(void) const { return E_PRIORITY_AGITATED_ACTION; }
	
	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CanBeInterruptedBySameEvent() const { return true; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual CEntity* GetSourceEntity() const;
private:
	RegdConstPed	m_pAgitator;
	RegdaiTask		m_pTask;
};


class CEventScenarioForceAction : public CEvent
{
public:

	CEventScenarioForceAction();
	CEventScenarioForceAction(const CEventScenarioForceAction& in_rhs);
	virtual ~CEventScenarioForceAction();
	
	virtual fwEvent* Clone() const { return rage_new CEventScenarioForceAction(*this); }
	virtual int GetEventType() const {return EVENT_SCENARIO_FORCE_ACTION;}
	virtual int GetEventPriority() const {return E_PRIORITY_SCENARIO_FORCE_ACTION; }
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;	

	void SetTask(aiTask* in_Task);
	int GetForceActionType() const { return m_ForceActionType; }
	void SetForceActionType(int forceActionType) { m_ForceActionType = forceActionType; }
	aiTask* CloneScriptTask() const;

	virtual const Vector3& GetSourcePos() const { return m_Position; }
	void SetSourcePos(const Vector3& in_Position) { m_Position = in_Position; }

private:

	RegdaiTask m_pTask;
	Vector3 m_Position;
	int m_ForceActionType;
};

class CEventCombatTaunt : public CEvent
{

public:

	CEventCombatTaunt(const CPed* pTaunter);
	~CEventCombatTaunt();

public:

	const CPed*	GetTaunter() const { return m_pTaunter; }

private:

	bool IsTaunterTalking() const;
	bool ShouldPedIgnoreEvent(const CPed* pPed) const;

private:

	virtual CEvent* Clone() const { return rage_new CEventCombatTaunt(m_pTaunter); }
	virtual int GetEventType(void) const	 { return EVENT_COMBAT_TAUNT; }
	virtual int GetEventPriority(void) const { return E_PRIORITY_COMBAT_TAUNT; }
	virtual bool IsTemporaryEvent() const { return true; }

	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool HasExpired() const;
	virtual bool IsEventReady(CPed* pPed);

private:

	RegdConstPed	m_pTaunter;
	float			m_fTotalTime;
	float			m_fTimeSinceTaunterStoppedTalking;
	bool			m_bHasRegisteredThreat;

};

class CEventRespondedToThreat : public CEvent
{

public:

	struct Tunables : public CTuning
	{
		Tunables();

		float m_MinDelayTimer;
		float m_MaxDelayTimer;

		PAR_PARSABLE;
	};

public:

	CEventRespondedToThreat(const CPed* pPedBeingThreatened, const CPed* pPedBeingThreatening);
	virtual ~CEventRespondedToThreat();

public:

	const CPed* GetPedBeingThreatened()		const { return m_pPedBeingThreatened; }
	const CPed* GetPedBeingThreatening()	const { return m_pPedBeingThreatening; }

private:

	virtual CEvent* Clone() const
	{
		CEventRespondedToThreat* pCloneEvent = rage_new CEventRespondedToThreat(m_pPedBeingThreatened, m_pPedBeingThreatening);
		pCloneEvent->m_bThreateningPedWasWanted = this->m_bThreateningPedWasWanted;
		return pCloneEvent;
	}

	virtual int GetEventPriority() const
	{
		return E_PRIORITY_RESPONDED_TO_THREAT;
	}

	virtual int GetEventType(void) const
	{
		return EVENT_RESPONDED_TO_THREAT;
	}

	virtual bool GetTunableDelay(const CPed* UNUSED_PARAM(pPed), float& fMin, float& fMax) const
	{
		fMin = sm_Tunables.m_MinDelayTimer;
		fMax = sm_Tunables.m_MaxDelayTimer;
		return true;
	}

	virtual CEntity* GetSourceEntity() const		{ return (CEntity*)(m_pPedBeingThreatened.Get()); }

	virtual CEntity* GetTargetEntity() const		{ return (CEntity*)(m_pPedBeingThreatening.Get()); }

private:

	virtual bool AffectsPedCore(CPed* pPed) const;
	virtual bool CreateResponseTask(CPed* pPed, CEventResponse& response) const;
	virtual void OnEventAdded(CPed* pPed) const;

	bool ThreateningPedIsWanted() const;

private:

	RegdConstPed m_pPedBeingThreatened;
	RegdConstPed m_pPedBeingThreatening;

	bool m_bThreateningPedWasWanted;

public:

	static Tunables	sm_Tunables;

};


class CEventPlayerDeath : public CEventEditableResponse
{
public:
	CEventPlayerDeath(CPed* pDyingPed);
	~CEventPlayerDeath() { }

	ePedType GetDeadPedType() const { return m_eDeadPedType; }

	virtual CPed* GetSourcePed() const { return m_pDyingPed; }
	virtual CEntity* GetSourceEntity() const { return (CEntity*)(m_pDyingPed.Get()); }

	const char* GetReactionContext() const;

private:
	virtual CEventEditableResponse* CloneEditable() const
	{
		return rage_new CEventPlayerDeath(m_pDyingPed);
	}

	virtual int GetEventPriority() const
	{
		return E_PRIORITY_PLAYER_DEATH;
	}

	virtual int GetEventType(void) const
	{
		return EVENT_PLAYER_DEATH;
	}

	virtual bool IsTemporaryEvent() const { return true; }

	virtual bool AffectsPedCore(CPed* pPed) const; 

private:
	RegdPed  m_pDyingPed;
	ePedType m_eDeadPedType;

	static const int		sm_iNumContexts = 3;
	static const char*	sm_caReactionContexts[sm_iNumContexts];

};

#endif
