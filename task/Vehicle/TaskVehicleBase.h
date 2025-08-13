//-------------------------------------------------------------------------
// TaskVehicleBase.h
// Contains the base vehicle task that the high level FSM vehicle tasks inherit from
//
// Created on: 12/09/08
//
// Author: Phil Hooker
//-------------------------------------------------------------------------


#ifndef TASK_VEHICLE_BASE_H
#define TASK_VEHICLE_BASE_H

#include "modelinfo/VehicleModelInfo.h"
#include "network/General/NetworkUtil.h"
#include "Peds/Ped.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskTypes.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleEnterExitFlags.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"

class CVehicle;
class CPed;
class CComponentReservation;
class CCarDoor;
class CTurret;
struct sEntryPointDebug;
struct sEntryPointsDebug;

// Must change enums in generic.sch if changing order of enterexitflags
CompileTimeAssert(CVehicleEnterExitFlags::ResumeIfInterupted	== 0);  
CompileTimeAssert(CVehicleEnterExitFlags::WarpToEntryPosition	== 1);  
CompileTimeAssert(CVehicleEnterExitFlags::JackIfOccupied		== 3);		
CompileTimeAssert(CVehicleEnterExitFlags::Warp					== 4);	
CompileTimeAssert(CVehicleEnterExitFlags::DontWaitForCarToStop	== 6);	
CompileTimeAssert(CVehicleEnterExitFlags::DontCloseDoor			== 8);	
CompileTimeAssert(CVehicleEnterExitFlags::WarpIfDoorBlocked		== 9);	
CompileTimeAssert(CVehicleEnterExitFlags::JumpOut				== 12);	
CompileTimeAssert(CVehicleEnterExitFlags::DontDefaultWarpIfDoorBlocked == 16);	
CompileTimeAssert(CVehicleEnterExitFlags::PreferLeftEntry		== 17);	
CompileTimeAssert(CVehicleEnterExitFlags::PreferRightEntry		== 18);	
CompileTimeAssert(CVehicleEnterExitFlags::JustPullPedOut		== 19);	
CompileTimeAssert(CVehicleEnterExitFlags::UseDirectEntryOnly	== 20);	
CompileTimeAssert(CVehicleEnterExitFlags::WarpIfShuffleLinkIsBlocked == 22);	
CompileTimeAssert(CVehicleEnterExitFlags::DontJackAnyone		== 23);	
CompileTimeAssert(CVehicleEnterExitFlags::WaitForEntryToBeClearOfPeds == 24);	

// Typedefs
typedef CVehicleEnterExitFlags::FlagsBitSet VehicleEnterExitFlags;

class CClonedVehicleFSMInfoBase
{
public:

    CClonedVehicleFSMInfoBase() :
      m_pVehicle(0)
    , m_iTargetEntryPoint(0)
    , m_iSeatRequestType(SR_Any)
	, m_iSeat(-1)
    {
        m_iRunningFlags.BitSet().Reset();
    }
    CClonedVehicleFSMInfoBase(CVehicle* pVehicle, SeatRequestType iSeatRequestType, s32 iSeat, s32 iTargetEntryPoint, VehicleEnterExitFlags iRunningFlags) :
    m_pVehicle(pVehicle)
    , m_iTargetEntryPoint(iTargetEntryPoint)
    , m_iRunningFlags(iRunningFlags)
    , m_iSeatRequestType(iSeatRequestType)
	, m_iSeat(iSeat)
    {
    }

    CVehicle*	          GetVehicle()					{ return m_pVehicle.GetVehicle(); }
	s32		              GetTargetEntryPoint()	const	{ return m_iTargetEntryPoint; }
	VehicleEnterExitFlags GetFlags()					{ return m_iRunningFlags; }
	SeatRequestType	      GetSeatRequestType()			{ return m_iSeatRequestType; }
	s32		              GetSeat()	const				{ return m_iSeat; }
protected:

    CSyncedVehicle	      m_pVehicle;          // target vehicle
	s32			          m_iTargetEntryPoint; // Which seat is the current target
	VehicleEnterExitFlags m_iRunningFlags;     // Flags that change the behaviour of the task
	SeatRequestType	      m_iSeatRequestType;  // The seat request type (e.g. any, specific, prefer)
	s32			          m_iSeat;			   // Which seat has been requested
};

//-------------------------------------------------------------------------
// Task info for being in cover
//-------------------------------------------------------------------------
class CClonedVehicleFSMInfo : public CSerialisedFSMTaskInfo, public CClonedVehicleFSMInfoBase
{
public:
    CClonedVehicleFSMInfo()
    {
    }

	CClonedVehicleFSMInfo(s32 enterState, CVehicle* pVehicle, SeatRequestType iSeatRequestType, s32 iSeat, s32 iTargetEntryPoint, const VehicleEnterExitFlags &iRunningFlags) :
    CClonedVehicleFSMInfoBase(pVehicle, iSeatRequestType, iSeat, iTargetEntryPoint, iRunningFlags)
    {
        CSerialisedFSMTaskInfo::SetStatusFromMainTaskState(enterState);
    }
    ~CClonedVehicleFSMInfo() {}

	virtual const CEntity*	GetTarget()	const		{ return (const CEntity*)m_pVehicle.GetVehicle(); }
	virtual CEntity*		GetTarget()				{ return (CEntity*)m_pVehicle.GetVehicle(); }

    void Serialise(CSyncDataBase& serialiser)
    {
        CSerialisedFSMTaskInfo::Serialise(serialiser);

        ObjectId vehicleID = m_pVehicle.GetVehicleID();

        SERIALISE_OBJECTID(serialiser, vehicleID, "Vehicle");
        SERIALISE_INTEGER(serialiser, m_iTargetEntryPoint, SIZEOF_ENTRY_POINT,	"Target Entry Point");
        SERIALISE_VEHICLE_ENTER_EXIT_FLAGS(serialiser, m_iRunningFlags,	"Running Flags");
        SERIALISE_INTEGER(serialiser, reinterpret_cast<s32&>(m_iSeatRequestType),      SIZEOF_ENTRY_POINT,"Seat Request Type");
		SERIALISE_INTEGER(serialiser, m_iSeat,      SIZEOF_ENTRY_POINT,			"Seat");

		m_pVehicle.SetVehicleID(vehicleID);
    }

private:

    static const unsigned int SIZEOF_ENTRY_POINT = 5;
};

#define NUM_CAR_DOORS (CAR_DOOR_PSIDE_R-CAR_DOOR_DSIDE_F+1)

typedef enum
{
	SV_Invalid = 0,
	SV_Valid,
	SV_Preferred
} SeatValidity;

//-------------------------------------------------------------------------
// Baseclass for all vehicle subtasks, implements a basic state system
//-------------------------------------------------------------------------
class CTaskVehicleFSM : public CTaskFSMClone
{
public:

#if __ASSERT
	// Keep in sync with X:\gta5\script\dev_network\core\common\native\generic.sch
	static const char* GetScriptVehicleSeatEnumString(s32 iScriptedSeatEnum);
#endif // __ASSERT

	static const float INVALID_GROUND_HEIGHT_FIXUP;
	static fwMvClipSetVarId ms_CommonVarClipSetId;
	static fwMvClipSetVarId ms_EntryVarClipSetId;
	static fwMvClipSetVarId ms_InsideVarClipSetId;
	static fwMvClipSetVarId ms_ExitVarClipSetId;

	static fwMvClipId	 ms_Clip0Id;
	static fwMvClipId	 ms_Clip1Id;
	static fwMvClipId	 ms_Clip2Id;
	static fwMvClipId	 ms_Clip3Id;
	static fwMvClipId	 ms_Clip4Id;

	static fwMvClipId	 ms_Clip0OutId;
	static fwMvClipId	 ms_Clip1OutId;
	static fwMvClipId	 ms_Clip2OutId;
	static fwMvClipId	 ms_Clip3OutId;
	static fwMvClipId	 ms_Clip4OutId;

	static fwMvFloatId	 ms_Clip0PhaseId;
	static fwMvFloatId	 ms_Clip1PhaseId;
	static fwMvFloatId	 ms_Clip2PhaseId;
	static fwMvFloatId	 ms_Clip3PhaseId;
	static fwMvFloatId	 ms_Clip4PhaseId;

	static fwMvFloatId	 ms_Clip0BlendWeightId;
	static fwMvFloatId	 ms_Clip1BlendWeightId;
	static fwMvFloatId	 ms_Clip2BlendWeightId;
	static fwMvFloatId	 ms_Clip3BlendWeightId;
	static fwMvFloatId	 ms_Clip4BlendWeightId;

	static const fwMvBooleanId ms_BlockRagdollActivationId;
	static const fwMvBooleanId ms_LegIkBlendOutId;
	static const fwMvBooleanId ms_LegIkBlendInId;
	static const fwMvBooleanId ms_LerpTargetFromClimbUpToSeatId;

	static float MIN_TIME_BEFORE_PLAYER_CAN_ABORT;

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		bool m_AllowEntryToMPWarpInSeats;
		bool  m_ForceStreamingFailure;
		float m_PushAngleDotTolerance;
		float m_TowardsDoorPushAngleDotTolerance;
		float m_DeadZoneAnyInputDirection;
		float m_DisallowGroundProbeVelocity;
		float m_MinPedSpeedToActivateRagdoll;
		float m_MinPhysSpeedToActivateRagdoll;
		float m_MaxHoverHeightDistToWarpIntoHeli;
		float m_MinTimeToConsiderPedGoingToDoorPriority;
		float m_MaxTimeToConsiderPedGoingToDoorPriority;
		float m_MaxDistToConsiderPedGoingToDoorPriority;
		float m_BikeEntryCapsuleRadiusScale;
		float m_VehEntryCapsuleRadiusScale;
		float m_MinRollToDoExtraTest;
		float m_MinPitchToDoExtraTest;
		u32 m_TimeToConsiderEnterInputValid;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	enum eVehicleClipFlags
	{
		// Optional fixups
		VF_InitialXOffsetFixup		= BIT0,				// Apply a translational fixup to make up the initial offset from the ideal playing position
		VF_InitialRotOffsetFixup	= BIT1,				// Apply a rotational fixup to make up the initial offset from the ideal playing orientation
		VF_ApplyToTargetFixup		= BIT2,				// Apply a translational fixup to get us to the target even if the clip doesn't
		VF_ApplyInitialOffset		= BIT3,				// Apply the initial offset to the reference point from when we first started playing the clip

		// Reference node type (one must be active)
		VF_EntryPoint				= BIT4,				// Playing the clip relative to the entry point
		VF_Seat						= BIT5,				// Playing the clip relative to the seat

		// Door flags
		VF_OpenDoor					= BIT6,				// Drive door open from clip
		VF_CloseDoor				= BIT7,				// Drive door closed from clip
		VF_FrontEntryUsed			= BIT8,				// The front entry is being used, don't drive the door fully
		VF_FromOutside				= BIT9,				// Open door from outside
		VF_UseOscillation			= BIT10,			// Make the door wiggle

		// Ik flags
		VF_No_IK_Clip				= BIT11,			// No IK tags in the anim clip
		VF_IK_Running				= BIT12,			// IK is running from the clip
		VF_IK_Blend_Out				= BIT13				// Whether the blend out handles Ik
	};

	static atHashWithStringNotFinal emptyAmbientContext;

#if __BANK
	static void SpewStreamingDebugToTTY(const CPed& rPed, const CVehicle& rVeh, s32 iSeatOrEntryIndex, const fwMvClipSetId clipSetId, const float fTimeInState);
#endif // __BANK

	static bool IsVehicleLockedForPlayer(const CVehicle& rVeh, const CPed& rPed);
	static bool ShouldDestroyVehicleWeaponManager(const CVehicle& rVeh, bool& bIsTurretedVehicle);

	// Computes the best seat position, based on where we want the turret to spin around to from our approach position
	static Vector3 ComputeWorldSpacePosFromLocal(const CVehicle& rVeh, const Vector3& vLocalRefPos);
	static Matrix34 ComputeIdealSeatMatrixForPosition(const CTurret& rTurret, const CVehicle& rVeh, const Vector3& vLocalRefPos, s32 iSeatIndex);
	static bool CheckIfPedWentThroughSomething(CPed& rPed, const CVehicle& rVeh);
	static bool PedShouldWarpBecauseVehicleAtAwkwardAngle(const CVehicle& rVeh);
	static bool PedShouldWarpBecauseHangingPedInTheWay(const CVehicle& rVeh, s32 iEntryIndex);
	static float GetEntryRadiusMultiplier(const CEntity& rEnt);
	static bool ProcessBlockRagdollActivation(CPed& rPed, CMoveNetworkHelper& rMoveNetworkHelper);
	static bool IsPedHeadingToLeftSideOfVehicle(const CPed& rPed, const CVehicle& rVeh);
	static bool IsPedOnLeftSideOfVehicle(const CPed& rPed, const CVehicle& rVeh);
	static bool CanPedWarpIntoVehicle(const CPed& rPed, const CVehicle& rVeh);
	static void ProcessAutoCloseDoor(const CPed& rPed, CVehicle& rVeh, s32 iSeatIndex);
	static void ProcessLegIkBlendOut(CPed& rPed, float fBlendOutPhase, float fCurrentPhase);
	static void ProcessLegIkBlendIn(CPed& rPed, float fBlendInPhase, float fCurrentPhase);
	static bool ShouldClipSetBeStreamedOut(fwMvClipSetId clipSetId);
	static inline void RequestVehicleClipSet(fwMvClipSetId clipSetId, eStreamingPriority priority = SP_Invalid, float fLifeTime = -1.0f)
	{
		if (clipSetId != CLIP_SET_ID_INVALID && !ms_Tunables.m_ForceStreamingFailure)
		{
			CPrioritizedClipSetRequestManager::GetInstance().Request(CPrioritizedClipSetRequestManager::RC_Vehicle, clipSetId, priority, fLifeTime, false);
		}
	}
	static inline bool RequestVehicleClipSetReturnIsLoaded(fwMvClipSetId clipSetId, eStreamingPriority priority = SP_Invalid, float fLifeTime = -1.0f)
	{
		if (clipSetId != CLIP_SET_ID_INVALID && !ms_Tunables.m_ForceStreamingFailure)
		{
			return CPrioritizedClipSetRequestManager::GetInstance().Request(CPrioritizedClipSetRequestManager::RC_Vehicle, clipSetId, priority, fLifeTime, true);
		}
		return false;
	}
	static inline bool IsVehicleClipSetLoaded(fwMvClipSetId clipSetId)
	{
		if (clipSetId != CLIP_SET_ID_INVALID && !ms_Tunables.m_ForceStreamingFailure)
		{
			return CPrioritizedClipSetRequestManager::GetInstance().IsLoaded(clipSetId);
		}
		return false;
	}

	// Conversion from script entry/exit flags to bitset
	static void SetScriptedVehicleEntryExitFlags(VehicleEnterExitFlags& vehicleFlags, s32 scriptedFlags);
	static void SetBitSetFlagIfSet(VehicleEnterExitFlags& vehicleFlags, s32 iScriptBit, s32 scriptedFlags)
	{
		if (IsScriptBitSet(iScriptBit, scriptedFlags))
			vehicleFlags.BitSet().Set(iScriptBit);
	}
	static bool IsScriptBitSet(s32 iScriptBit, s32 iFlags)
	{
		aiAssertf(iScriptBit <= 31, "Invalid bit for script");
		if (1<<iScriptBit & iFlags)
		{
			return true;
		}
		return false;
	}

	static CPed*	GetPedInOrUsingSeat(const CVehicle& rVeh, s32 iSeatIndex, const CPed* pPedToExclude = NULL);
	static bool		IsOpenSeatIsBlocked(const CVehicle& rVeh, s32 iSeatIndex);
	static bool		ShouldWarpInAndOutInMP(const CVehicle& rVeh, s32 iEntryPointIndex);
	static fwMvClipSetId GetExitToAimClipSetIdForPed(const CPed& ped);
	static void		SetDoorDrivingFlags(CVehicle* pVehicle, s32 iEntryPointIndex);
	static void		ClearDoorDrivingFlags(CVehicle* pVehicle, s32 iEntryPointIndex);
	static bool		CheckForEnterExitVehicle(CPed& ped);
	static bool		ComputeTargetTransformInWorldSpace(const CVehicle& vehicle, Vector3& vTargetPos, Quaternion& qTargetOrientation, s32 iEntryPointIndex, CExtraVehiclePoint::ePointType pointType);

	// PURPOSE: Turns on the arm ik to reach for the door handle
	static void		ProcessDoorHandleArmIkHelper(const CVehicle& vehicle, CPed& ped, s32 iEntryPointIndex, const crClip* pClip, float fPhase, bool& bIkTurnedOn, bool& bIkTurnedOff);
	static void		ProcessDoorHandleArmIk(const CVehicle& vehicle, CPed& ped, s32 iEntryPointIndex, bool bRight = false, float fBlendInDuration = NORMAL_BLEND_DURATION, float fBlendOutDuration = NORMAL_BLEND_DURATION);
	static void		ForceBikeHandleArmIk(const CVehicle& vehicle, CPed& ped, bool bOn = true, bool bRight = false, float fBlendDuration = INSTANT_BLEND_DURATION, bool bUseOrientation = false);
	static bool		ProcessSeatBoneArmIk(const CVehicle& vehicle, CPed& ped, s32 iSeatIndex, const crClip* pClip, float fPhase, bool bOn = true, bool bRight = false);
	static eVehicleClipFlags	ProcessBikeHandleArmIk(const CVehicle& vehicle, CPed& ped, const crClip* pClip, float fPhase, bool bOn = true, bool bRight = false, bool bUseOrientation = false);
	static bool		SeatRequiresHandGripIk(const CVehicle& rVeh, s32 iSeat);
	static void     ProcessTurretHandGripIk(const CVehicle& rVeh, CPed& rPed, const crClip* pClip, float fPhase, const CTurret& rTurret);	
	static void		IkHandToTurret( CPed &rPed, const CVehicle& rVeh, const crSkeletonData& rSkelData, bool bRightHand, float fBlendDuration, const CTurret& rTurret );
	static void		PointTurretInNeturalPosition(CVehicle& rVeh, const s32 iSeat);
	static bool		CanPedShuffleToOrFromTurretSeat(const CVehicle* pVehicle, const CPed* pPed, s32& iTargetShuffleSeat);
	static bool		CanPedManualShuffleToTargetSeat(const CVehicle* pVehicle, const CPed* pPed, const s32 iTargetSeat);
	static bool		CanPedShuffleToOrFromExtraSeat(const CVehicle* pVehicle, const CPed* pPed, s32& iTargetShuffleSeat);
	static bool		CanPerformOnVehicleTurretJack(const CVehicle& rVeh, s32 iSeatIndex);

	static void		UpdatePedsMover(CPed* pPed, const Vector3 &vNewPos, const Quaternion& qNewOrientation);
	static bool		ComputePedGlobalTransformFromAttachOffsets(CPed* pPed, Vec3V_Ref vGlobalNewPos, QuatV_Ref qGlobalNewOrientation);
	static void		UpdatePedVehicleAttachment(CPed* pPed, Vec3V_ConstRef vGlobalNewPos, QuatV_ConstRef qGlobalNewOrientation, bool bDontSetTranslation = false);
	static bool		ComputeGlobalTransformFromLocal(CPed& rPed, Vec3V_ConstRef vLocalPos, QuatV_ConstRef qLocalOrientation, Vec3V_Ref vGlobalPos, QuatV_Ref qGlobalOrientation);
	static bool		ComputeLocalTransformFromGlobal(CPed& rPed, Vec3V_ConstRef vGlobalPos, QuatV_ConstRef qGlobalOrientation, Vec3V_Ref vLocalNewPos, QuatV_Ref qLocalOrientation);
 	static bool		FindGroundPos(const CEntity* pEntity, const CPed *pPed, const Vector3 &vStart, float fTestDepth, Vector3& vIntersection, bool bIgnoreEntity = true, bool bIgnoreVehicleCheck = false);

	static void		DriveBikeAngleFromClip(CVehicle* pVehicle, s32 iFlags, const float fCurrentPhase, const float fStartPhase, const float fEndPhase, float fOriginalAngle, float fTargetLeanAngle = 1.0f);
	static void		ProcessOpenDoor(const crClip& clip, float fPhase, CVehicle& vehicle, s32 iEntryPointIndex, bool bProcessOnlyIfTagsFound, bool bBlockRearDoorDriving = false, CPed* pEvaluatingPed = NULL, bool bDoorInFrontBeingUsed = false);
	static void		DriveDoorFromClip(CVehicle& vehicle, CCarDoor* pDoor, s32 iFlags, const float fCurrentPhase, const float fStartPhase, const float fEndPhase);
	static CCarDoor* GetDoorForEntryPointIndex(CVehicle* pVehicle, s32 iEntryPointIndex);
	static const CCarDoor* GetDoorForEntryPointIndex(const CVehicle* pVehicle, s32 iEntryPointIndex);
	static bool		RequestClipSetFromSeat(const CVehicle* pVehicle, s32 iSeatIndex);
	static bool		RequestClipSetFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex);
	static fwMvClipSetId GetClipSetIdFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex);
	static s32 FindRearEntryForEntryPoint(CVehicle* pVehicle, s32 iEntryPointIndex);
	static s32 FindFrontEntryForEntryPoint(CVehicle* pVehicle, s32 iEntryPointIndex);
	static s32 GetEntryPointInFrontIfBeingUsed(CVehicle* pVehicle, s32 iThisEntryPointIndex, CPed* pEvaluatingPed = NULL);
	static void		SetRemotePedUsingDoorsTrue(const CPed& rPed, CVehicle* pVeh, s32 iEntryPointIndex) { SetRemotePedUsingDoors(rPed, pVeh, iEntryPointIndex, true); }
	static void		SetRemotePedUsingDoorsFalse(const CPed& rPed, CVehicle* pVeh, s32 iEntryPointIndex) { SetRemotePedUsingDoors(rPed, pVeh, iEntryPointIndex, false); }
	static void		SetRemotePedUsingDoors(const CPed& rPed, CVehicle* pVeh, s32 iEntryPointIndex, bool bTrue);
	static bool		ShouldAlterEntryPointForOpenDoor(const CPed& rPed, const CVehicle& rVeh, s32 iEntryPointIndex, s32 iEntryFlags);
	static float	GetDoorRatioForEntryPoint(const CVehicle& rVeh, s32 iEntryPointIndex);
	static float	GetTargetDoorRatioForEntryPoint(const CVehicle& rVeh, s32 iEntryPointIndex);
	static bool		CanDriveRearDoorPartially(const CVehicle& rVeh);
	static bool		DoesVehicleHaveDoubleBackDoors(const CVehicle& rVeh);
	static s32		FindBestTurretSeatIndexForVehicle(const CVehicle& rVeh, const CPed& rPed, const CPed* pDriver, s32 iDriversSeat, bool bConsiderFriendlyOccupyingPeds, bool bOnlyConsiderIfPreferEnterTurretAfterDriver);
	static bool		CanDisplacePedInTurretSeat(const CPed* pTurretOccupier, bool bConsiderFriendlyOccupyingPeds);
	static float	ComputeSquaredDistanceFromPedPosToEntryPoint(const CPed &rPed, const Vector3& vPedPos, const CVehicle& rVeh, s32 iEntryPointIndex);
	static float	ComputeSquaredDistanceFromPedPosToSeat(const Vector3& vPedPos, const CVehicle& rVeh, s32 iEntryPointIndex, s32 iSeatIndex);
	static bool		CanForcePedToShuffleAcross(const CPed& rPed, const CPed& rOtherPed, const CVehicle& rVeh);

	// New attach helpers
	struct sClipBlendParams
	{
		atArray<float>			afBlendWeights;
		atArray<float>			afPhases;
		atArray<const crClip*>	apClips;
	};

	struct sMoverFixupInfo
	{
		sMoverFixupInfo()
			: fXFixUpStartPhase(0.0f)
			, fXFixUpEndPhase(1.0f)
			, fYFixUpStartPhase(0.0f)
			, fYFixUpEndPhase(1.0f)
			, fZFixUpStartPhase(0.0f)
			, fZFixUpEndPhase(1.0f)
			, fRotFixUpStartPhase(0.0f)
			, fRotFixUpEndPhase(1.0f)
			, bFoundXFixup(false)
			, bFoundYFixup(false)
			, bFoundZFixup(false)
			, bFoundRotFixup(false)
		{

		}

		float fXFixUpStartPhase;
		float fXFixUpEndPhase;
		float fYFixUpStartPhase;
		float fYFixUpEndPhase;
		float fZFixUpStartPhase;
		float fZFixUpEndPhase;
		float fRotFixUpStartPhase;
		float fRotFixUpEndPhase;

		bool bFoundXFixup	: 1;
		bool bFoundYFixup	: 1;
		bool bFoundZFixup	: 1;
		bool bFoundRotFixup : 1;
	};

	static fwMvClipId GetGenericClipId(s32 i);
	static const crClip* GetGenericClip(const CMoveNetworkHelper& moveNetworkHelper, s32 i);
	static float GetGenericClipPhaseClamped(const CMoveNetworkHelper& moveNetworkHelper, s32 i);
	static float GetGenericBlendWeightClamped(const CMoveNetworkHelper& moveNetworkHelper, s32 i);
	static bool GetGenericClipBlendParams(sClipBlendParams& clipBlendParams, const CMoveNetworkHelper& moveNetworkHelper, u32 uNumClips, bool bZeroPhases);

	// Compute what blend of anims we will get based on the signal we pass in
	// NOTE: If the curves on the move network change this will screw up
	static void PreComputeStaticBlendWeightsFromSignal(sClipBlendParams& clipBlendParams, float fBlendSignal);
	static bool ComputeIdealEndTransformRelative(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset);
	static bool ComputeIdealEndTransformRelative(const sClipBlendParams& clipBlendParams, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset);
	static bool ComputeCurrentAnimTransformRelative(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset, float fCurrentPhase);
	static bool ComputeRemainingAnimTransform(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset, float fCurrentPhase);
	static bool ComputeRemainingAnimTransform(const sClipBlendParams& clipBlendParams, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset);
	static void TransformRelativeOffsetsToWorldSpace(Vec3V_ConstRef vTargetPosWorldIn, QuatV_ConstRef qTargetRotWorldIn, Vec3V_Ref vPosOffsetInWorldPosOut, QuatV_Ref qRotOffsetInWorldRotOut);
	static void UnTransformFromWorldSpaceToRelative(const CVehicle& vehicle,Vec3V_ConstRef vTargetPosWorldIn, QuatV_ConstRef qTargetRotWorldIn, Vec3V_Ref vWorldInPosOffsetOut, QuatV_Ref qWorldInRotOffsetOut);
	static bool ApplyGroundAndOrientationFixup(const CVehicle& vehicle, const CPed &ped, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut, float& fCachedGroundFixUpHeight, float fOverrideFixupHeight);
	static bool ApplyGroundFixup(const CVehicle& vehicle, const CPed& ped, Vec3V_Ref vIdealPosOut, float& fCachedGroundFixUpHeight, float fOverrideFixupHeight);
	static bool ApplyMoverFixup(CPed& ped, const CVehicle& vehicle, s32 iEntryPointIndex, const sClipBlendParams& clipBlendParams, float fTimeStep, Vec3V_Ref vTargetOffset, QuatV_Ref qTargetRotOffset, bool bFixUpIfTagsNotFound = false);
	static s32 FindClipIndexWithHighestBlendWeight(const sClipBlendParams& clipBlendParams);
	static void GetMoverFixupInfoFromClip(sMoverFixupInfo& moverFixupInfo, const crClip* pClip, u32 uVehicleModelHash);
	static float ComputeTranslationFixUpThisFrame(float fTotalFixUp, float fClipPhase, float fStartPhase, float fEndPhase);
	static QuatV ComputeOrientationFixUpThisFrame(QuatV_ConstRef qTotalRotationalFixUp, float fClipPhase, float fStartPhase, float fEndPhase);
	static void GetAttachWorldTransform(const CVehicle& vehicle, s32 iEntryPointIndex, Vec3V_Ref vAttachWorld, QuatV_Ref qAttachWorld);
	static bool CanPedWarpIntoHoveringVehicle(const CPed* pPed, const CVehicle& rVeh, s32 iEntryIndex = -1, bool bTestPedHeightToSeat = false);

	CTaskVehicleFSM( CVehicle* pVehicle, SeatRequestType iSeatRequestType, s32 iSeat, const VehicleEnterExitFlags& iRunningFlags, s32 iTargetEntryPoint );
	virtual ~CTaskVehicleFSM();

	CVehicle* GetVehicle() { return m_pVehicle; }
	const CVehicle* GetVehicle() const { return m_pVehicle; }
	void SetTimerRanOut( bool bTimerRanOut );
	bool GetTimerRanOut() { return m_bTimerRanOut; }

	// Ped motion 
	void SetPedMoveBlendRatio(float fMoveBlendRatio) {m_fMoveBlendRatio = fMoveBlendRatio;}
	float GetPedMoveBlendRatio() const {return m_fMoveBlendRatio;}

	s32 GetTargetSeat() const { return m_iSeat; }
	s32 GetTargetEntryPoint() const		{ return m_iTargetEntryPoint; }
	s32 GetTargetSeatRequestType() const	{ return m_iSeatRequestType; }

	void SetWaitForSeatToBeEmpty( bool bWait ) {m_bWaitForSeatToBeEmpty = bWait;}
	void SetWaitForSeatToBeOccupied( bool bWait ) {m_bWaitForSeatToBeOccupied = bWait;}

	bool IsFlagSet(CVehicleEnterExitFlags::Flags iFlag) const { return m_iRunningFlags.BitSet().IsSet(iFlag); }
	VehicleEnterExitFlags GetRunningFlags() { return m_iRunningFlags; }
	void SetRunningFlag(CVehicleEnterExitFlags::Flags iFlag ) { m_iRunningFlags.BitSet().Set(iFlag); }
	void ClearRunningFlag(CVehicleEnterExitFlags::Flags iFlag ) { m_iRunningFlags.BitSet().Clear(iFlag); }

	bool GetWillUseDoor( CPed* pPed );
	SeatRequestType GetSeatRequestType() const { return m_iSeatRequestType; }

	CCarDoor*		GetDoorForEntryPointIndex(s32 iEntryPointIndex);
	const CCarDoor*	GetDoorForEntryPointIndex(s32 iEntryPointIndex) const;

	static bool		ProcessApplyForce(CMoveNetworkHelper& moveNetworkHelper, CVehicle& vehicle, const CPed& ped);
	static bool		ApplyForceForDoorClosed(CVehicle& vehicle, const CPed& ped, s32 iEntryPointIndex, float fForceMultiplier);
	static bool		ApplyForceForDoorClosed(CVehicle& vehicle, s32 iBoneIndex, float fForceMultiplier);
	// PURPOSE: Ped unreserve door
	static void		UnreserveDoor(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex);
	static void		UnreserveSeat(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex);

	// Clone task implementation
	// Default implementations that do nothing
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);

	// Static query functions used by all vehicle tasks	
	static u32 TIME_LAST_PLAYER_GROUP_MEMBER_ENTERED;
	static bool VehicleContainsFriendlyPassenger( const CPed* pPed, const CVehicle* pVehicle );
    static bool CanUsePedEntryPoint( const CPed* pPed, const CPed* pOtherPed, VehicleEnterExitFlags iFlags = VehicleEnterExitFlags());
    static bool CanJackPed( const CPed* pPed, const CPed* pOtherPed, VehicleEnterExitFlags iFlags = VehicleEnterExitFlags());
	static SeatValidity GetSeatValidity( SeatRequestType seatRequestType, s32 iSeatRequested, s32 iSeatToValidate );		
	static bool IsAnyGroupMemberUsingEntrypoint( const CPed* pPed, const CVehicle* pVehicle, s32 iEntryPoint, VehicleEnterExitFlags iFlags, const bool bOnlyConsiderIfCloser, const Vector3& vEntryPos, int* piActualEntryPointUsed = NULL );
	static bool IsAnyPlayerEnteringDirectlyAsDriver( const CPed* pPed, const CVehicle* pVehicle );
	// PURPOSE: Check for quitting the enter/exit task
	bool CheckPlayerExitConditions(CPed& ped, CVehicle& vehicle, VehicleEnterExitFlags iFlags, float fTimeRunning, bool bExiting = false, Vector3 *pvTestPosOverride = NULL);

	static bool CheckEarlyExitConditions(CPed& ped, CVehicle& vehicle, VehicleEnterExitFlags iFlags, bool& bWalkStart, bool bUseWiderAngleTolerance = false);
	
	enum eExitInterruptFlags
	{
		EF_TryingToSprintAlwaysInterrupts		= BIT0,
		EF_OnlyAllowInterruptIfTryingToSprint	= BIT1,
		EF_AnyStickInputInterrupts				= BIT2,
		EF_IgnoreDoorTest						= BIT3
	};

	static bool CheckForPlayerExitInterrupt(const CPed& rPed, const CVehicle& rVeh, VehicleEnterExitFlags iFlags, Vec3V_ConstRef vSeatOffset, s32 iExitInterruptFlags);
	static bool CheckForPlayerPullingAwayFromVehicle(const CPed& rPed, const CVehicle& rVeh, Vec3V_ConstRef vSeatOffset, s32 iExitInterruptFlags);

	static bool CheckForPlayerMovement(CPed& ped);

	// PURPOSE: Check if peds are friendly, or are in the same custody group
	static bool ArePedsFriendly(const CPed* pPed, const CPed* pOtherPed);

	static const CVehicleClipRequestHelper *GetVehicleClipRequestHelper(const CPed *pPed);

	static bool CanPassengerLookAtPlayer(CPed* pPed);
	static void SetPassengersToLookAtPlayer(const CPed* pPlayerPed, CVehicle* pVehicle, bool bIncludeDriver = false);
	static s32 GetSeatPedIsTryingToEnter(const CPed* pPed);
	static s32 GetEntryPedIsTryingToEnter(const CPed* pPed);
	static const CPed* GetPedsGroupPlayerLeader(const CPed& rPed);
	static const CPed* GetPedThatHasDoorReservation(const CVehicle& rVeh, s32 iEntryPointIndex);
	static bool AnotherPedHasDoorReservation(const CVehicle& rVeh, const CPed& rPed, s32 iEntryPointIndex);
	static bool PedHasSeatReservation(const CVehicle& rVeh, const CPed& rPed, s32 iEntryPointIndex, s32 iTargetSeatIndex);

	static bool ShouldUseClimbUpAndClimbDown(const CVehicle& rVeh, s32 iEntryPointIndex);

protected:

    //PURPOSE: Checks whether the ped can displace another ped
    static bool CanDisplacePed( const CPed* pPed, const CPed* pOtherPed, VehicleEnterExitFlags iFlags, bool doEntryExitChecks);

	// PURPOSE: Ped unreserve door
	void UnreserveDoor(CPed* pPed);

	// PURPOSE: Ped unreserve seat
	void UnreserveSeat(CPed* pPed);

	VehicleEnterExitFlags	m_iRunningFlags;
	RegdVeh					m_pVehicle;
	bool					m_bRepeatStage;
	s32						m_iTargetEntryPoint;
	SeatRequestType			m_iSeatRequestType;
	s32						m_iSeat;
	s32						m_iSpecificStage;
	float					m_fMoveBlendRatio;
	bool					m_bWarping;
	bool					m_bWaitForSeatToBeEmpty;
	bool					m_bWaitForSeatToBeOccupied;

private:
	bool					m_bTimerRanOut;

};

#if __BANK
////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Test Task For Playing Back Anims Which Should Be Attached
////////////////////////////////////////////////////////////////////////////////

class CTaskAttachedVehicleAnimDebug : public CTask
{
public:

#if DEBUG_DRAW
	static void RenderMatrixFromVecQuat(Vec3V_Ref vPosition, QuatV_Ref qOrientation, s32 iDuration, u32 uKey);
#endif // DEBUG_DRAW

	static fwMvRequestId ms_AlignRequestId;
	static fwMvRequestId ms_ClimbUpRequestId;
	static fwMvRequestId ms_PreGetInTransitionRequestId;
	static fwMvRequestId ms_ForcedEntryRequestId;
	static fwMvRequestId ms_GetInRequestId;
	static fwMvRequestId ms_IdleRequestId;
	static fwMvRequestId ms_CloseDoorFromInsideRequestId;
	static fwMvRequestId ms_MoveToNextSeatRequestId;
	static fwMvRequestId ms_GetOutRequestId;
	static fwMvRequestId ms_PostGetOutTransitionRequestId;
	static fwMvRequestId ms_ClimbDownRequestId;
	static fwMvRequestId ms_OnBikeRequestId;

	static fwMvBooleanId ms_AlignOnEnterId;
	static fwMvBooleanId ms_ClimbUpOnEnterId;
	static fwMvBooleanId ms_PreGetInTransitionOnEnterId;
	static fwMvBooleanId ms_ForcedEntryOnEnterId;
	static fwMvBooleanId ms_GetInOnEnterId;
	static fwMvBooleanId ms_IdleOnEnterId;
	static fwMvBooleanId ms_CloseDoorFromInsideOnEnterId;
	static fwMvBooleanId ms_MoveToNextSeatOnEnterId;
	static fwMvBooleanId ms_GetOutOnEnterId;
	static fwMvBooleanId ms_PostGetOutTransitionOnEnterId;
	static fwMvBooleanId ms_ClimbDownOnEnterId;
	static fwMvBooleanId ms_OnBikeOnEnterId;

	static fwMvFlagId	 ms_ToAimFlagId;
	static fwMvFlagId	 ms_FastFlagId;

	static fwMvBooleanId ms_Clip0OnLoopId;
	static fwMvBooleanId ms_Clip0OnEndedId;
	static fwMvStateId	 ms_InvalidStateId;
	static fwMvFloatId	 ms_InvalidPhaseId;
	static fwMvFloatId	 ms_BlendDurationId;

	static fwMvFloatId	 ms_PreGetInPhaseId;
	static fwMvFloatId	 ms_ForcedEntryPhaseId;
	static fwMvFloatId	 ms_GetInPhaseId;
	static fwMvFloatId	 ms_GetOutPhaseId;
	static fwMvFloatId	 ms_ClimbDownPhaseId;
	static fwMvFloatId	 ms_LeanId;
	static fwMvFloatId	 ms_AimBlendDirectionId;

	static fwMvStateId	 ms_AlignStateId;
	static fwMvStateId	 ms_ClimbUpStateId;
	static fwMvStateId	 ms_PreGetInTransitionStateId;
	static fwMvStateId	 ms_ForcedEntryStateId;
	static fwMvStateId	 ms_GetInStateId;
	static fwMvStateId	 ms_IdleStateId;
	static fwMvStateId	 ms_CloseDoorFromInsideStateId;
	static fwMvStateId	 ms_MoveToNextSeatStateId;
	static fwMvStateId	 ms_GetOutStateId;
	static fwMvStateId	 ms_PostGetOutTransitionStateId;
	static fwMvStateId	 ms_ClimbDownStateId;
	static fwMvStateId	 ms_OnBikeStateId;

	static float ms_fAlignBlendInDuration;
	static float ms_fPreGetInBlendInDuration;
	static float ms_fGetInBlendInDuration;
	static float ms_fIdleInBlendInDuration;
	static float ms_fCloseDoorFromInsideBlendInDuration;
	static float ms_fGetOutBlendInDuration;
	static float ms_fPostGetOutBlendInDuration;

	// PURPOSE: Macro to allow us to define the state enum and string lists once
	#define ATTACHED_VEHICLE_ANIM_DEBUG_STATES(X)	X(State_Start),						\
													X(State_StreamAssets),				\
													X(State_Align),						\
													X(State_ClimbUp),					\
													X(State_OpenDoor),					\
													X(State_TryLockedDoor),				\
													X(State_ForcedEntry),				\
													X(State_AlignToGetIn),				\
													X(State_GetIn),						\
													X(State_Idle),						\
													X(State_CloseDoorFromInside),		\
													X(State_Shuffle),					\
													X(State_GetOut),					\
													X(State_GetOutToAim),				\
													X(State_CloseDoorFromOutside),		\
													X(State_GetOutToIdle),				\
													X(State_ClimbDown),					\
													X(State_OnBike),					\
													X(State_Finish)

	// PURPOSE: FSM states
	enum eAttachedVehicleDebugState
	{
		ATTACHED_VEHICLE_ANIM_DEBUG_STATES(DEFINE_TASK_ENUM)
	};

	CTaskAttachedVehicleAnimDebug(CVehicle* pVehicle, s32 iSeat, s32 iTargetEntryPoint, const atArray<s32>& aDesiredStates);
	virtual ~CTaskAttachedVehicleAnimDebug();

	// RETURNS: The task Id. Each task class has an Id for quick identification
	virtual s32	GetTaskTypeInternal() const	{ return CTaskTypes::TASK_ATTACHED_VEHICLE_ANIM_DEBUG; }

	// RETURNS: Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	virtual s32	GetDefaultStateAfterAbort() const { return State_Finish; }

	// PURPOSE: Used to produce a copy of this task
	// RETURNS: A copy of this task
	virtual aiTask* Copy() const { return rage_new CTaskAttachedVehicleAnimDebug(m_pVehicle, m_iSeatIndex, m_iEntryPointIndex, m_aDesiredStates); }
	
	// PURPOSE: Generic cleanup function, called when the task quits itself or is aborted
	virtual	void CleanUp();

	// PURPOSE: Task update that is called before FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPreFSM();

	// PURPOSE: FSM implementation
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	// PURPOSE: Task update that is called after FSM update, once per frame.
	// RETURNS: If FSM_Quit is returned the current state will be terminated
	virtual FSM_Return ProcessPostFSM();

	// PURPOSE: Manipulate the animated velocity
	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// PURPOSE: Get the tasks main move network helper
	virtual const CMoveNetworkHelper* GetMoveNetworkHelper() const { return &m_moveNetworkHelper; }
	virtual CMoveNetworkHelper* GetMoveNetworkHelper() { return &m_moveNetworkHelper; }

	// PURPOSE: Is there a valid move network transition to the next ai state from our current state?
	virtual bool IsMoveTransitionAvailable(s32 iNextState) const;

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Local state functions, called on fsm events within main UpdateFSM function
	FSM_Return 	Start_OnUpdate();
	FSM_Return 	StreamAssets_OnUpdate();
	FSM_Return	Align_OnEnter();
	FSM_Return	Align_OnUpdate();
	FSM_Return	ClimbUp_OnEnter();
	FSM_Return	ClimbUp_OnUpdate();
	FSM_Return	OpenDoor_OnEnter();
	FSM_Return	OpenDoor_OnUpdate();
	FSM_Return	AlignToGetIn_OnEnter();
	FSM_Return	AlignToGetIn_OnUpdate();
	FSM_Return	TryLockedDoor_OnEnter();
	FSM_Return	TryLockedDoor_OnUpdate();
	FSM_Return	ForcedEntry_OnEnter();
	FSM_Return	ForcedEntry_OnUpdate();
	FSM_Return	GetIn_OnEnter();
	FSM_Return	GetIn_OnUpdate();
	FSM_Return	Idle_OnEnter();
	FSM_Return	Idle_OnUpdate();
	FSM_Return	CloseDoorFromInside_OnEnter();
	FSM_Return	CloseDoorFromInside_OnUpdate();
	FSM_Return	Shuffle_OnEnter();
	FSM_Return	Shuffle_OnUpdate();
	FSM_Return	GetOut_OnEnter();
	FSM_Return	GetOut_OnUpdate();
	FSM_Return	GetOutToAim_OnEnter();
	FSM_Return	GetOutToAim_OnUpdate();
	FSM_Return	CloseDoorFromOutside_OnEnter();
	FSM_Return	CloseDoorFromOutside_OnUpdate();
	FSM_Return	GetOutToIdle_OnEnter();
	FSM_Return	GetOutToIdle_OnUpdate();
	FSM_Return	ClimbDown_OnEnter();
	FSM_Return	ClimbDown_OnUpdate();
	FSM_Return	OnBike_OnEnter();
	FSM_Return	OnBike_OnUpdate();

	////////////////////////////////////////////////////////////////////////////////

	static float GetBlendDurationForState(s32 iState);
	static fwMvClipId GetClipIdForState(s32 iState);
	static bool GetClipIdArrayForState(s32 iState, atArray<fwMvClipId>& aClipIds);
	fwMvClipSetId GetClipSetIdForState(s32 iState) const;
	const crClip* GetClipForState() const { return GetClipForState(GetState()); }
	const crClip* GetClipForState(s32 iState) const;
	bool GetClipArrayForState(atArray<const crClip*>& aClips) const { return GetClipArrayForState(GetState(), aClips); }
	bool GetClipArrayForState(s32 iState, atArray<const crClip*>& aClips) const;
	float GetClipPhaseForState() const;
	bool GetClipBlendParamsForState(s32 iState, CTaskVehicleFSM::sClipBlendParams& clipBlendParams, bool bStartedClips);

	bool ComputeIdealStartTransformForState(s32 iState, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut);
	bool ComputeIdealEndTransformForState(s32 iState, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut);
	bool ShouldApplyGroundAndOrientationFixupForState(s32 iState);
	void ApplyGroundAndOrientationFixupForState(s32 iState, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut);
	bool ComputeIdealStartTransformRelative(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset);

	void AttachPedForState(s32 iState, s32 iAttachState, bool bJustSetAttachStateIfAttached = true);
	void SetTaskStateAndUpdateDesiredState();
	void SetUpMoveParamsAndAttachState(s32 iAttachState, bool bForceAttachUpdate = false);
	void UpdateClipSets();

	////////////////////////////////////////////////////////////////////////////////

	static void ProcessOpenDoor(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase);
	static void ProcessCloseDoor(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase);
	static void ProcessDoorAngle(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase, bool bClose);
	static void ProcessSmashWindow(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase);

	static CCarDoor* GetDoorForEntryPointIndex(CVehicle& vehicle, s32 iEntryPointIndex);

	////////////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW
	void RenderTargetMatrixForState();
#endif // DEBUG_DRAW

	////////////////////////////////////////////////////////////////////////////////

	DEFINE_MOVE_NETWORK_HELPER_FUNCTIONS

	////////////////////////////////////////////////////////////////////////////////

private:

	////////////////////////////////////////////////////////////////////////////////

	CMoveNetworkHelper		m_moveNetworkHelper;
	RegdVeh 				m_pVehicle;
	s32						m_iSeatIndex;
	s32						m_iEntryPointIndex;
	fwMvClipSetId			m_EntryClipSetId;
	fwMvClipSetId			m_SeatClipSetId;
	fwClipSetRequestHelper	m_EntryClipSetRequestHelper;
	fwClipSetRequestHelper	m_SeatClipSetRequestHelper;
	atArray<s32>			m_aDesiredStates;
	s32						m_iDesiredStateIndex;
	bool					m_bAppliedOnce;
	float					m_fLeanParameter;

public:

	////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
	// PURPOSE: Get the name of the specified state
	DEFINE_STATIC_STATE_NAME_STRINGS(State_Start, State_Finish, ATTACHED_VEHICLE_ANIM_DEBUG_STATES)
#endif // !__FINAL

	////////////////////////////////////////////////////////////////////////////////

};

#endif // __BANK


#endif // TASK_VEHICLE_BASE_H

