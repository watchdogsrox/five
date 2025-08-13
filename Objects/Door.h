#ifndef DOOR_H
#define DOOR_H

// Game headers
//#include "network/Objects/entities/NetObjDoor.h"
#include "audio/dooraudioentity.h"
#include "Objects/Object.h"
#include "network/objects/entities/NetObjDoor.h"

class CDoorTuning;
class CDoorSystemData;

namespace rage
{
	class bkBank;
}

typedef u64 DoorBrokenFlags;
typedef u64 DoorDamagedFlags;

class CDoorExtension : public fwExtension
{
public:
	FW_REGISTER_CLASS_POOL(CDoorExtension);
	EXTENSIONID_DECL(CDoorExtension, fwExtension);

	virtual void InitEntityExtensionFromDefinition(const fwExtensionDef* definition, fwEntity* entity);

	bool			m_enableLimitAngle;
	bool			m_startsLocked;
	float			m_limitAngle;
	float			m_doorTargetRatio;
	atHashString	m_audioHash;
};

//////////////////////////////////////////////////////////////////////////
// CDoor
//////////////////////////////////////////////////////////////////////////


class CDoor : public CObject
{
	//friend class CNetObjDoor;

public:

	enum
	{
		DOOR_TYPE_NOT_A_DOOR = 0,
		// auto door types
		DOOR_TYPE_STD,
		DOOR_TYPE_SLIDING_HORIZONTAL,
		DOOR_TYPE_SLIDING_VERTICAL,
		DOOR_TYPE_GARAGE,
		DOOR_TYPE_BARRIER_ARM,
		DOOR_TYPE_RAIL_CROSSING_BARRIER,
		// script controlled door types
		DOOR_TYPE_STD_SC,
		DOOR_TYPE_SLIDING_HORIZONTAL_SC,
		DOOR_TYPE_SLIDING_VERTICAL_SC,
		DOOR_TYPE_GARAGE_SC,
		DOOR_TYPE_BARRIER_ARM_SC,
		DOOR_TYPE_RAIL_CROSSING_BARRIER_SC,

		NUM_DOOR_TYPES
	};

	enum
	{
		DOOR_CONTROL_FLAGS_UNSPRUNG = 0,				// for doors that swing, turn off the spring trying to return them to their TargetRatio.  Can still open doors by pushing them.
		DOOR_CONTROL_FLAGS_LOCK_WHEN_REMOVED = 1,		// when the door is removed, lock it
		DOOR_CONTROL_FLAGS_LOCK_WHEN_CLOSED = 2,		// when the door has shut, lock it
		DOOR_CONTROL_FLAGS_CHECKED_FOR_DOUBLE = 3,		// set if we have checked if this is a double door (TODO: maybe move - arguably not really a "control flag")
		DOOR_CONTROL_FLAGS_DOUBLE_DOOR_SHOULD_OPEN = 4,	// used as a signal from the other half of a double door, telling us to open (or remain open) on the next update
		DOOR_CONTROL_FLAGS_CUTSCENE_DONT_SHUT = 5,		// avoid shutting this door if a cutscene is handling this door
		DOOR_CONTROL_FLAGS_SMOOTH_STATUS_TRANSITION = 6,// the door has just been created and we're in the "smooth" transition phase
		DOOR_CONTROL_FLAGS_LATCHED_SHUT = 7,			// Flag used to tell the code that it has latched shut (prevents the door from getting latched shut multiple times causing a door pop)
		DOOR_CONTROL_FLAGS_STUCK = 8
	};

	enum ChangeDoorStateResult
	{
		CHANGE_DOOR_STATE_SUCCESS,
		CHANGE_DOOR_STATE_NO_MODEL,
		CHANGE_DOOR_STATE_NOT_DOOR,
		CHANGE_DOOR_STATE_INVALID_OBJECT_TYPE,
		CHANGE_DOOR_STATE_NO_OBJECT,
		CHANGE_DOOR_STATE_IS_BUILDING
	};

	enum PlayerDoorState
	{
		PDS_NONE = -1,
		PDS_BACK,
		PDS_FRONT,
	};

	CDoor(const eEntityOwnedBy ownedBy);
	CDoor(const eEntityOwnedBy ownedBy, s32 nModelIndex, bool bCreateDrawable, bool bInitPhys);
	CDoor(const eEntityOwnedBy ownedBy, class CDummyObject* pDummy);
	virtual ~CDoor();

	//static CDoor* Create(class CDummyObject* pDummy, const eEntityOwnedBy ownedBy);

	virtual void AddPhysics();

	void Init();
#if GTA_REPLAY
	void InitReplay(const Matrix34& matOriginal);
#endif
	void Update();
#if __BANK
	virtual void SetFixedPhysics(bool bFixed, bool bNetwork = false);
#endif
	void UpdateEntityFromPhysics(phInst *pInst, int nLoop);	// don't need this now we're using constraints

	PPUVIRTUAL void OnActivate(phInst* pInst, phInst* pOtherInst);
	virtual void ProcessPreComputeImpacts(phContactIterator impacts);
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	virtual void ProcessPostPhysics();
	virtual void ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const* hitInst, const Vector3& vMyHitPos,
		const Vector3& vOtherHitPos, float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact);
	virtual bool ProcessControl();
	virtual bool RequiresProcessControl() const;
	void ProcessControlLogic();

	virtual bool ObjectDamage(float fImpulse, const Vector3* pColPos, const Vector3* pColNormal, CEntity *pEntityResponsible, u32 nWeaponUsedHash, phMaterialMgr::Id hitMaterial = 0);

	// Sometimes we swap out doors that are blocking interiors. It is very important that those doors collide with peds/vehicles instantly.
	virtual bool ShouldAvoidCollision(const CPhysical*) { return false; } 

	audDoorAudioEntity *GetDoorAudioEntity();
	const audDoorAudioEntity *GetDoorAudioEntity() const;

	int GetDoorType() const {return m_nDoorType;}
	bool IsCreakingDoorType() const		{ return m_nDoorType==DOOR_TYPE_STD || m_nDoorType==DOOR_TYPE_STD_SC; }
	bool IsHingedDoorType() const		{ return m_nDoorType==DOOR_TYPE_STD || m_nDoorType==DOOR_TYPE_STD_SC; }
	bool IsDoorShut() const;
	bool IsDoorShut(float transThreshold, float orienThreshold) const;
	bool IsDoorOpening() const;
	bool IsDoorFullyOpen(float fRatioEpsilon) const;
    const Vector3 &GetDoorStartPosition() const { return m_vecDoorStartPos; }
	float GetDoorOcclusion(float transThreshold, float orienThreshold, float transThreshold1, float orienThreshold1) const;
	float GetTargetDoorRatio() const { return m_fDoorTargetRatio; }
	float GetLimitAngle() const;
	bool CalcOpenFullyOpen(Matrix34& matFullyOpen) const;
	bool IsShutForPortalPurposes() const;
	bool IsOnActivationUpdate() const { return m_bOnActivationUpdate; }
	bool IsAttachedToARoom0Portal() const { return m_bAttachedToARoom0Portal; }

	// Notify the portal system about visibility changes.
	void UpdatePortalState(bool force = false);

	//PURPOSE: When the door is below certain speed and its on some limits it shuts instead of swing.
	void LatchShut();
	void SetTargetDoorRatio(float fRatio, bool bForce, bool bWarp = false);
	void ScriptDoorControl(bool bLock, float fOpenRatio) {ScriptDoorControl(bLock, fOpenRatio, false); }
	void ScriptDoorControl(bool bLock, float fOpenRatio, bool bForceOpenRatio, bool allowScriptControlConversion = true);
	void ProcessDoorBehaviour();
	void SetOpenIfObstructed();
	bool DetermineShouldAutoOpen();
	Vec3V_Out CalcAngleThresholdCheckPos() const;
	void ProcessPortalDisabling();
	void InitDoorPhys(const Matrix34& matOrginal);
	bool GetDoorIsAtLimit(Vector3::Vector3Param vPos, Vector3::Vector3Param vDir, const float fHeadingThreshold = 0.4f);
	float GetDoorOpenRatio() const;
#if GTA_REPLAY
	void SetOpenDoorRatio(float val) { m_ReplayDoorOpenRatio = val; }
	void SetDoorStartRot(const Quaternion& quat) { m_qDoorStartRot = quat; }
	Quaternion GetDoorStartRot() const { return m_qDoorStartRot; }
#endif
	void SetPlayerDoorState( PlayerDoorState ePlayerDoorState )					{ m_ePlayerDoorState = ePlayerDoorState; }
	PlayerDoorState GetPlayerDoorState()										{ return m_ePlayerDoorState; }
	void ProcessDoorImpact(CEntity* pOtherEntity, Vec3V_In vHitPos, const Vector3 & vNormal);
	void SetDoorControlFlag(int flag, bool b)								{ m_DoorControlFlags.Set(flag, b); }
	bool GetDoorControlFlag(int flag)										{ return m_DoorControlFlags.IsSet(flag); }
	bool GetAndClearDoorControlFlag(int flag)								{ return m_DoorControlFlags.GetAndClear(flag); }
	void BreakOffDoor();
	bool CheckIfCanBeDoubleWithOtherDoor(const CDoor& otherDoor) const;		// Used internally by CheckForDoubleDoor().
	float GetAutomaticDist() const											{ return m_fAutomaticDist; }
	float GetAutomaticRate() const											{ return m_fAutomaticRate; }
	void ClearUnderCodeControl()											{ m_bUnderCodeControl = false; m_bHoldingOpen = false; }
	bool GetIsDoubleDoor() const											{ return m_OtherDoorIfDouble != NULL; }
	const CDoor* GetOtherDoubleDoor() const									{ return m_OtherDoorIfDouble; }

	void CalcAutoOpenInfo(Vec3V_Ref testCenterOut, float& testRadiusOut) const;
	void CalcAutoOpenInfoBox(spdAABB& rLocalBBox, Mat34V_Ref posMatOut) const;
	bool EntityCausesAutoDoorOpen( const CEntity* entity, const fwInteriorLocation doorRoom1Loc, const fwInteriorLocation doorRoom2Loc, fwFlags32 doorFlags ) const;
	bool PedTriggersAutoOpen( const CPed* ped, fwFlags32 doorFlags, bool bInVehicle ) const;
	bool WantsToBeAwake() { return m_wantToBeAwake; }

	static bool DoorTypeAutoOpens(int doorType);
	static bool DoorTypeOpensForPedsWhenNotLocked(int doorType);
	static bool DoorTypeAutoOpensForVehicles(int doorType);

	static ChangeDoorStateResult ChangeDoorStateForScript(u32 doorHash, const Vector3 &doorPos, CDoor* pDoor, bool addToLockedMap, bool lockState, float openRatio, bool removeSpring, bool smoothLock = false, 
														  float automaticRate = 0.0f, float automaticDist = 0.0f, bool useOldOverrides = true, bool bForceOpenRatio = false);

	static u8 DetermineDoorType(const CBaseModelInfo* pModelInfo, const Vector3& bndBoxMin, const Vector3& bndBoxMax);
	static float GetScriptSearchRadius(const CBaseModelInfo& modelInfo);

	const CDoorTuning* GetTuning() const
	{	return m_Tuning;	}

	CDoorSystemData* GetDoorSystemData() const { return m_DoorSystemData; }
	void SetDoorSystemData(CDoorSystemData* pData) { m_DoorSystemData = pData; if (!pData) ClearUnderCodeControl(); }

	//used to open or close the door from code
	void CloseDoor()		{ m_bHoldingOpen = false; m_bUnderCodeControl = true;}
	void OpenDoor()			{ m_bHoldingOpen = true; m_bUnderCodeControl = true; }

	// See if cutscenes control the door
	void SetRegisteredWithCutscene(bool state) { m_bRegisteredWithCutscene = state; }
	bool IsRegisteredWithCutscene() { return m_bRegisteredWithCutscene; }

	void SetHoldingOpen(bool holdingOpen) { m_bHoldingOpen = holdingOpen; }
	bool GetHoldingOpen() const { return m_bHoldingOpen; }
	
	bool GetUnderCodeControl() const { return m_bUnderCodeControl; }

	void SetCloseDelay(float delay) { m_bDoorCloseDelayActive = true; m_fDoorCloseDelay = delay; }

	void SetAlwaysPush( bool alwaysPush );
	bool GetAlwaysPush() { return m_alwaysPush; }
	bool GetAlwaysPushVehicles() { return m_alwaysPushVehicles; }
	void SetAlwaysPushVehicles(bool pushVehicles) { m_alwaysPushVehicles = pushVehicles; }

	static void AddDoorToList(CEntity *pDoor);

	static void RemoveDoorFromList(CEntity *pDoor);
	static CEntity* FindClosestDoor(CBaseModelInfo *pModelInfo, const Vector3 &position, float radius);
	static void OpenAllBarriersForRace(bool snapOpen);
	static void CloseAllBarriersOpenedForRace();
	static void ProcessRaceOpenings(CDoor& door, const bool snapOpen);
	static bool AreDoorsOpenForRacing() { return ms_openDoorsForRacing; }

    static void ForceOpenIntersectingDoors(CPhysical **objectsToCheck, unsigned numObjectsToCheck, bool allowScriptControlConversion = true);

#if __BANK
	static void AddClassWidgets(bkBank& bank);
	static void LockAllDoors() { SetLockedStateOfAllDoors(true); }
	static void UnlockAllDoors() { SetLockedStateOfAllDoors(false); }
	static void SetLockedStateOfAllDoors(bool locked);

	static void DebugLockSelectedDoor(bool lock, bool force);

	static void LockSelectedDoor()   { DebugLockSelectedDoor(true, false); }
	static void UnlockSelectedDoor() { DebugLockSelectedDoor(false, false); }

	static void ForceLockSelectedDoor()   { DebugLockSelectedDoor(true, true); }
	static void ForceUnlockSelectedDoor() { DebugLockSelectedDoor(false, true); }

	static void DebugSetOpenRatio(float fRatio);
	static void SetOpenRatioCallBack();
	static void AddSelectedToDoorSystem();
	static void SetSelectedDoorToOpenForRaces();
	static void SetSelectedDoorToNotOpenForRaces();

	static bool sm_ForceAllAutoOpenToOpen;
	static bool sm_ForceAllAutoOpenToClose;
	static bool sm_ForceAllToBreak;
	static bool sm_ForceAllTargetRatio;
	static float sm_ForcedTargetRatio;
	static bool sm_IngnoreScriptSetDoorStateCommands;
	enum {DEBUG_DOOR_STRING_LENGTH = 128};
	static char sm_AddDoorString[DEBUG_DOOR_STRING_LENGTH];
	static bool sm_SnapOpenBarriersForRaceToggle;
#endif	// __BANK

	// PURPOSE:	Get the rate this door would use if auto-opening, taking
	//			the door type and scripted overrides into account.
	float GetEffectiveAutoOpenRate() const;

protected:
	virtual void UpdatePhysicsFromEntity(bool bWarp=false);
	static ChangeDoorStateResult GetDoorForScriptHelper(u32 doorHash, const Vector3 &doorPos, CEntity **doorLocked);

	// PURPOSE:	Perform a check to see if this is a double door, and if so, store a reference to the
	//			other door object in m_OtherDoorIfDouble.
	void CheckForDoubleDoor();

	// PURPOSE:	If set, clear out m_OtherDoorIfDouble, and do the same in the reverse direction.
	void ClearDoubleDoor();

	// PURPOSE:	For a sliding door, get the direction vector it slides in when opening in the
	//			positive direction, and the distance it opens.
	void GetSlidingDoorAxisAndLimit(Vec3V_Ref axisOut, float &limitOut) const;

	void EnforceLockedObjectStatus(const bool useSmoothTransitionHash = false);

	void UpdateNavigation(const bool bStateChanged);

private:
	void CalcAndApplyTorqueSLD(float fDoorTarget, float fDoorTargetOld, float fTimeStep); //Sliding doors
	void CalcAndApplyTorqueGRG(float fDoorTarget, float fDoorTargetOld, float fTimeStep); //Garage doors
	void CalcAndApplyTorqueBAR(float fDoorTarget, float fDoorTargetOld, float fTimeStep); //Barrier doors
	void CalcAndApplyTorqueSTD(float fDoorTarget, float fDoorTargetOld, float fTimeStep); //Standard doors

	bool IsDoorAudioEntityValid() const;
	void ResetCenterOfGravityForFragDoor(fragInst * pFragDoor);

protected:
	Quaternion			m_qDoorStartRot;
	Vector3				m_vecDoorStartPos;		//						(also used as velocity variable for attached objects)
	audDoorAudioEntity m_DoorAudioEntity;

	static float sm_fMaxStuckDoorRatio;
	float	m_fAutomaticDist;		// distance in meters within which an automatic sliding door or barrier opens, or 0.0f for the default behavior.
	float	m_fAutomaticRate;		// rate an automatic sliding door or barrier moves, uses default value for door type if this is 0.0f
	float	m_fDoorTargetRatio;		// target door ratio
	float   m_fDoorCloseDelay;		// when evaluating CDoorTuning::FullyOpenWhenTouched this is the delay countdown var before the door attempts to close
#if  GTA_REPLAY
	float m_ReplayDoorOpenRatio;
#endif	
	bool	m_alwaysPushVehicles;	// B*1991845: Makes the associated door tweak all impacts with vehicles so the door can effectively push a vehicle blocking it out of the way with ease.

	// PURPOSE:	If we have detected that this is a double door, this will be a reference to the other
	//			door.
	// NOTES:	Set by CheckForDoubleDoor(), only valid if DOOR_CONTROL_FLAGS_CHECKED_FOR_DOUBLE is set,
	//			and currently only used for sliding doors.
	RegdDoor			m_OtherDoorIfDouble;

	// PURPOSE:	Extra pointer to a phArchetype that we own, because we created a copy when the door broke off.
	// NOTES:	We hold a reference count on this, and the object itself is meant to be destroyed
	//			when the CDoor object gets destroyed.
	phArchetype*		m_ClonedArchetype;

	const CDoorTuning*	m_Tuning;

	CDoorSystemData*	m_DoorSystemData; // a ptr to the door system data the door is linked with

	u32					m_SmoothTransitionPhysicsFlags;
	u32					m_SmoothTransitionPosHash;
	s16					m_nDoorShot;	// need to store when door has been shot to make it swing open

	atFixedBitSet16		m_DoorControlFlags;

	u8					m_nDoorType;
	bool				m_bDoorCloseDelayActive;// Used in conjunction with the m_fDoorCloseDelay so we dont continue to apply a delay.
	s8					m_nDoorInstanceDataIdx;	// 
	s8					m_garageZeroVelocityFrameCount;

	PlayerDoorState		m_ePlayerDoorState;
	u32					m_uNextScheduledDetermineShouldAutoOpenTimeMS; // used to time throttle expensive DetermineShouldAutoOpen checks

	bool				m_bCachedShouldAutoOpenStatus : 1; // store the last DetermineShouldAutoOpen result
	bool				m_alwaysPush : 1;
	bool				m_bCountsTowardOpenPortals : 1;

	// PURPOSE:	Keep track of whether the portal system thinks this door is closed
	//			or not. Used by UpdatePortalState() to only notify the portal system
	//			when something changes.
	bool	m_WasShutForPortalPurposes : 1;
	bool	m_wantToBeAwake : 1;

#if __BANK
public:
	u8 m_processedPhysicsThisFrame;
protected:
#endif
	bool m_bHoldingOpen      :1;
	bool m_bUnderCodeControl :1;
	bool m_bOpening          :1;
	bool m_bOnActivationUpdate : 1;
	bool m_bAttachedToARoom0Portal : 1;
	bool m_bRegisteredWithCutscene : 1;

	static bool ms_openDoorsForRacing;
	static bool ms_snapOpenDoorsForRacing;

public:
	// Statics
	static bank_u32 ms_uMaxRagdollDoorContactTime;
	static bool ms_bUpdatingDoorRatio;
	static fwPtrListSingleLink ms_doorsList;
	static dev_float ms_fPlayerDoorCloseDelay;
	static dev_float ms_fEntityDoorCloseDelay;
	static bank_float ms_fStdDoorDamping;
	static bank_float ms_fStdFragDoorDamping;
	static f32	sm_fLatchResetThreshold;

	// network hooks
	NETWORK_OBJECT_TYPE_DECL( CNetObjDoor, NET_OBJ_TYPE_DOOR );
};



enum DoorScriptState
{
	DOORSTATE_INVALID = -1,
	DOORSTATE_UNLOCKED,
	DOORSTATE_LOCKED,
	DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA,
	DOORSTATE_FORCE_UNLOCKED_THIS_FRAME,
	DOORSTATE_FORCE_LOCKED_THIS_FRAME,
	DOORSTATE_FORCE_OPEN_THIS_FRAME,
	DOORSTATE_FORCE_CLOSED_THIS_FRAME,
};

enum DoorFlags
{
	DOORFLAG_IS_SAVEHOUSE_DOOR = 0x1
};

class CDoorScriptHandlerObject : public CGameScriptHandlerObject
{
public:

	CDoorScriptHandlerObject() : m_pHandler(NULL) {}
	~CDoorScriptHandlerObject() { Assert(!m_pHandler); }

	virtual unsigned					GetType() const { return SCRIPT_HANDLER_OBJECT_TYPE_DOOR; }
	virtual void						CreateScriptInfo(const scriptIdBase& scrId, const ScriptObjectId& objectId, const HostToken hostToken);
	virtual void						SetScriptInfo(const scriptObjInfoBase& info);
	virtual scriptObjInfoBase*			GetScriptInfo();
	virtual const scriptObjInfoBase*	GetScriptInfo() const;
	virtual void						SetScriptHandler(scriptHandler* handler);
	virtual scriptHandler*				GetScriptHandler() const { return m_pHandler; }
	virtual void						ClearScriptHandler() { m_pHandler = NULL; }
	virtual void						OnRegistration(bool newObject, bool hostObject);
	virtual void						OnUnregistration();
	virtual void						Cleanup();
	virtual netObject*					GetNetObject() const;
	virtual fwEntity*					GetEntity() const { return NULL; }
	virtual bool						HostObjectCanBeCreatedByClient() const;

#if __BANK
	virtual void SpewDebugInfo(const char* scriptName) const;
#endif

	virtual bool GetNoLongerNeeded() const { return false; }
	virtual const char* GetLogName() const { return "SCRIPT_DOOR"; }
		
	void SetScriptObjInfo(scriptObjInfoBase* info);

	void Invalidate() 
	{
		m_ScriptInfo.Invalidate();
		m_pHandler = NULL;
	}

private:

	scriptHandler*		m_pHandler;			// the script handler that this door is registered with
	CGameScriptObjInfo	m_ScriptInfo;		// the script that created this door
};

class CDoorSystemData : public CDoorScriptHandlerObject
{
public:

	CDoorSystemData() : 
		m_position(0.0f, 0.0f, 0.0f), 
		m_state(DOORSTATE_UNLOCKED),
		m_preNetworkedState(DOORSTATE_INVALID),
		m_doorEnumHash(0),
		m_modelInfoHash(0),
		m_pDoor(NULL),
		m_pNetworkObject(NULL),
		m_pendingState(DOORSTATE_INVALID),
		m_targetRatio(0.0f),
		m_unsprung(false),
		m_automaticRate(0.0f),
		m_automaticDistance(0.0f),
		m_useOldOverrides(true),
		m_holdOpen(false),
		m_flaggedForRemoval(false),
		m_persistsWithoutNetworkObject(false),
		m_pendingNetworking(false),
		m_stateNeedsToBeFlushed(false),
		m_openForRaces(false),
		m_permanentState(false),	
		m_brokenFlags(0),
		m_damagedFlags(0)
		{
#if __DEV
			AllocDebugMemForName();
#endif
		}

	CDoorSystemData(int doorEnumHash, int modelInfoHash, const Vector3 &postion) : 
		m_position(postion), 
		m_state(DOORSTATE_UNLOCKED),
		m_preNetworkedState(DOORSTATE_INVALID),
		m_doorEnumHash(doorEnumHash),
		m_modelInfoHash(modelInfoHash), 
		m_pDoor(NULL),
		m_pNetworkObject(NULL),
		m_pendingState(DOORSTATE_INVALID),
		m_targetRatio(0.0f),
		m_unsprung(false),
		m_automaticRate(0.0f),
		m_automaticDistance(0.0f),
		m_useOldOverrides(true),
		m_holdOpen(false),
		m_flaggedForRemoval(false),
		m_persistsWithoutNetworkObject(false),
		m_pendingNetworking(false),
		m_stateNeedsToBeFlushed(false),
		m_openForRaces(false),
		m_permanentState(false),	
		m_brokenFlags(0),
		m_damagedFlags(0)
		{
#if __DEV
			AllocDebugMemForName();
#endif
		}

	~CDoorSystemData()
	{
		if (m_pNetworkObject && AssertVerify(m_pNetworkObject->GetDoorSystemData() == this))
		{
			Assertf(0, "Destroying a door which still has a network object (%s)", m_pNetworkObject->GetLogName());

			m_pNetworkObject->SetDoorSystemData(NULL);
		}

		if (m_pDoor && AssertVerify(m_pDoor->GetDoorSystemData() == this))
		{
			m_pDoor->SetDoorSystemData(NULL);
		}

#if __DEV
		FreeDebugMemForName();
#endif
	}

	void SetState(DoorScriptState doorState);
	void SetPendingState(DoorScriptState pendingState);
	void SetEnumHash(int enumHash) { m_doorEnumHash = enumHash; }
	void SetModelInfoHash(int modelInfoHash) { m_modelInfoHash = modelInfoHash; }
	void SetPosition(const Vector3& position) { m_position = position; }
		
	void SetLocked(bool locked) { m_state = locked ? DOORSTATE_LOCKED : DOORSTATE_UNLOCKED; } 
	void SetTargetRatio(float targetRatio) { m_targetRatio = targetRatio; m_stateNeedsToBeFlushed  = true; } 
	void SetUnsprung(bool unSprung) { m_unsprung = unSprung; m_stateNeedsToBeFlushed  = true; } 
	void SetAutomaticRate(float automaticRate) { m_automaticRate = automaticRate;  m_stateNeedsToBeFlushed  = true; } 
	void SetAutomaticDistance(float automaticDistance) { m_automaticDistance = automaticDistance;  m_stateNeedsToBeFlushed  = true; } 
	void SetBrokenFlags(DoorBrokenFlags flags) { m_brokenFlags = flags; }
	void SetDamagedFlags(DoorDamagedFlags flags) { m_damagedFlags = flags; }
	void SetHoldOpen(bool holdOpen, bool bNetCall = false);
	void SetFlagForRemoval();
	void SetPersistsWithoutNetworkObject() { m_persistsWithoutNetworkObject = true; }
	void StateHasBeenFlushed() { m_stateNeedsToBeFlushed = false; }
	void SetOpenForRaces(bool open) { m_openForRaces = open; }
	void SetPermanentState() { m_permanentState = true; }
	void SetPendingNetworking(bool set) { m_pendingNetworking = set; }

	void SetNetworkObject(CNetObjDoor* pNetObj)
	{
		if (AssertVerify(!pNetObj || !pNetObj->GetDoorSystemData() || pNetObj->GetDoorSystemData()==this))
		{
			if (m_pNetworkObject != pNetObj && m_pNetworkObject && pNetObj)
			{
				FatalAssertf(0, "Trying to assign %s to door system entry %u, which is already assigned to %s", pNetObj->GetLogName(), m_doorEnumHash, m_pNetworkObject->GetLogName());
			}
			else
			{
				if (!m_pNetworkObject && pNetObj)
				{
					if (m_preNetworkedState == DOORSTATE_INVALID)
					{
						if (m_pendingState != DOORSTATE_INVALID)
							m_preNetworkedState = m_pendingState;
						else
							m_preNetworkedState = m_state;
					}
				}
				else if (m_persistsWithoutNetworkObject && m_pNetworkObject && !pNetObj)
				{
					m_pendingState = m_preNetworkedState;
				}

				m_pNetworkObject = pNetObj; 
			}
		}
	}

	void SetDoor(CDoor *pDoor) 
	{ 
		if (pDoor != m_pDoor)
		{
			if (m_pDoor)
			{
				m_pDoor->SetDoorSystemData(NULL);
			}

			if (pDoor)
			{
				Assertf(!pDoor->GetDoorSystemData(), "Trying to assign a door to the door system at %f, %f, %f" 
													 "which already has a door system entry at %f, %f, %f."
													 "Model name %s."
													 "It was added by %s.", 
													 m_position.x, m_position.y, m_position.z, 
													 pDoor->GetDoorSystemData()->GetPosition().x, 
													 pDoor->GetDoorSystemData()->GetPosition().y, 
													 pDoor->GetDoorSystemData()->GetPosition().z,
													 pDoor->GetModelName(),
													 pDoor->GetDoorSystemData()->m_pScriptName);

				if (pDoor->GetDoorSystemData())
				{
					pDoor->GetDoorSystemData()->SetDoor(NULL);
				}
			}

			m_pDoor = pDoor; 

			if (m_pDoor)
			{
				m_pDoor->SetDoorSystemData(this);
			}

			if (m_pNetworkObject)
			{
				m_pNetworkObject->AssignDoor(pDoor); 
			}
		}
	}
	void SetUseOldOverrides(bool useOldOverrides) { m_useOldOverrides = useOldOverrides; }

	DoorScriptState GetState() const { return m_state; }
	DoorScriptState GetInitialState() const { return m_preNetworkedState; }
	DoorScriptState GetPendingState() const { return m_pendingState; }
	int GetEnumHash() const { return m_doorEnumHash; }
	int GetModelInfoHash() const { return m_modelInfoHash; }
	const Vector3& GetPosition() const { return m_position; }
	CDoor *GetDoor() const { return m_pDoor; }
	CNetObjDoor *GetNetworkObject() const { return m_pNetworkObject; }

	bool				GetLocked() const;
	float				GetTargetRatio() const { return m_targetRatio; } 
	bool				GetUnsprung() const { return m_unsprung; } 
	float				GetAutomaticRate() const { return m_automaticRate; } 
	float				GetAutomaticDistance() const { return m_automaticDistance; } 
	DoorBrokenFlags		GetBrokenFlags() const { return m_brokenFlags; }
	DoorDamagedFlags	GetDamagedFlags() const { return m_damagedFlags; }
	bool				GetUseOldOverrides() const { return m_useOldOverrides; }
	bool				GetHoldOpen() const { return m_holdOpen; }
	bool				GetFlaggedForRemoval() const { return m_flaggedForRemoval; }
	bool				GetPersistsWithoutNetworkObject() const { return m_persistsWithoutNetworkObject; }
	bool				GetStateNeedsToBeFlushed() const { return m_stateNeedsToBeFlushed; }
	bool				GetOpenForRaces() const { return m_openForRaces; }
	bool				GetPermanentState() const { return m_permanentState; }
	bool				GetPendingNetworking() const { return m_pendingNetworking; }

	bool CanStopNetworking();
#if __DEV
	void				AllocDebugMemForName();
	void				FreeDebugMemForName();
#endif

	static const char* GetDoorStateName(DoorScriptState state);

private:

	DoorScriptState m_state;
	Vector3 m_position;
	DoorScriptState m_preNetworkedState;
	int m_doorEnumHash;
	int m_modelInfoHash;
	CDoor *m_pDoor;
	CNetObjDoor* m_pNetworkObject;

	DoorScriptState m_pendingState;

	float			m_targetRatio;		
	float			m_automaticRate;
	float			m_automaticDistance;
	DoorBrokenFlags	m_brokenFlags; 
	DoorDamagedFlags m_damagedFlags;
#if __DEV
public:
	enum { DOOR_SCRIPT_NAME_LENGTH = 128};
	char *			m_pScriptName;
private:
#endif

	bool			m_unsprung : 1;	
	bool			m_useOldOverrides : 1;
	bool            m_holdOpen : 1;
	bool			m_flaggedForRemoval : 1;
	bool			m_persistsWithoutNetworkObject : 1; // if true the door data can exist without a network object during MP
	bool			m_pendingNetworking : 1;			
	bool			m_stateNeedsToBeFlushed : 1;
	bool            m_openForRaces : 1;
	bool            m_permanentState: 1;
};

inline bool CDoorSystemData::GetLocked() const 
{
	return m_state == DOORSTATE_LOCKED || 
		   m_state == DOORSTATE_FORCE_LOCKED_UNTIL_OUT_OF_AREA || 
		   m_state == DOORSTATE_FORCE_LOCKED_THIS_FRAME;
}

class CDoorSystem
{
public:
	typedef atMap<u32, CDoorSystemData*> DoorSystemMap;
	typedef atMap<u32, CDoorSystemData*> LockedMap;

	static void Init();
	static void Shutdown();

	static CDoorSystemData* AddDoor(int doorEnumHash, int modelInfoHash, const Vector3& position);
	static CDoorSystemData* AddDoor(const Vector3 &pos, u32 modelHash);
	static CDoorSystemData* AddSavedDoor(int doorEnumHash, int modelInfoHash, DoorScriptState state, const Vector3& position);
	static void				RemoveDoor(int doorEnumHash);

	static CDoorSystemData* GetDoorData(int doorEnumHash);
	static CDoorSystemData* GetDoorData(CEntity* pEntity);
	static CDoorSystemData* GetDoorData(const Vector3 &pos);
	static CDoorSystemData* GetDoorData(u32 posHash);

	static void LinkDoorObject(CDoor& door);
	static void UnlinkDoorObject(CDoor& door);

	static void SetPendingStateOnDoor(CDoorSystemData &doorData);
	static void BreakDoor(CDoor& door, DoorBrokenFlags brokenFlags, bool bRemoveFragments);
	static void DamageDoor(CDoor& door, DoorDamagedFlags damagedFlags);

	static void Update();
	static int GetDoorCount() { return ms_doorMap.GetNumUsed(); }

	static DoorSystemMap &GetDoorMap() { return ms_doorMap; }
	static LockedMap &GetLockedMap() { return ms_lockedMap; }

	static CDoorSystemData *SetLocked(const Vector3 &position, bool bIsLocked, float targetRatio, bool bUnsprung, u32 modelHash, float automaticRate, float automaticDist, CDoor* pDoor = NULL, bool useOldOverrides = true);
	static void SwapModel(u32 oldModelHash, u32 newModelHash, Vector3& swapCentre, float swapRadiusSqr);

	static u32 GenerateHash32(const Vector3 &position);

	static void DebugDraw();
	static CDoorSystemData *FindDoorSystemData(const Vector3 &position, int modelHash, float radius);

#if __BANK
	static bool ms_logDoorPhysics;
#endif

private:

	static CDoorSystemData* CreateNewDoorSystemData(int doorEnumHash, int modelInfoHash, const Vector3 &position);

	static void	RemoveDoor(CDoorSystemData *pDoorData);

#if __BANK
	static bool ms_logDebugDraw;
#endif

	static DoorSystemMap ms_doorMap;
	static LockedMap     ms_lockedMap;

};

#endif // DOOR_H
