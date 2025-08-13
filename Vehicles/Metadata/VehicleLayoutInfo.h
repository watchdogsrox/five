#ifndef INC_VEHICLE_LAYOUT_H
#define INC_VEHICLE_LAYOUT_H

// Rage headers
#include "atl/hashstring.h"
#include "fwanimation/animdefines.h"
#include "fwtl/Pool.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "Vehicles/vehicle_channel.h"

class CVehicleSeatInfo;
class CVehicleEntryPointInfo;
class CVehicleSeatAnimInfo;
class CVehicleEntryPointAnimInfo;
class CVehicle;

//-------------------------------------------------------------------------
// Anim rate modifiers
//-------------------------------------------------------------------------
class CAnimRateSet
{
public:

	// PURPOSE: Used by the PARSER.  Gets a name from a CAnimRateSet info pointer
	static const char* GetNameFromInfo(const CAnimRateSet* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CAnimRateSet info pointer from a name
	static const CAnimRateSet* GetInfoFromName(const char* name);	

	atHashWithStringNotFinal GetName() const { return m_Name; }

	struct sAnimRateTuningPair
	{
		float GetRate() const;

	private:

		float m_SPRate;
		float m_MPRate;

		PAR_SIMPLE_PARSABLE
	};

	sAnimRateTuningPair	m_NormalEntry;
	sAnimRateTuningPair	m_AnimCombatEntry;
	sAnimRateTuningPair	m_NoAnimCombatEntry;
	sAnimRateTuningPair m_CombatJackEntry;
	sAnimRateTuningPair m_ForcedEntry;
	bool				m_UseInVehicleCombatRates;
	sAnimRateTuningPair	m_NormalInVehicle;
	sAnimRateTuningPair	m_NoAnimCombatInVehicle;
	sAnimRateTuningPair	m_NormalExit;
	sAnimRateTuningPair	m_NoAnimCombatExit;

private:

	atHashWithStringNotFinal m_Name;

	PAR_SIMPLE_PARSABLE
};

//-------------------------------------------------------------------------
// Cover Bound Determined by position relative to vehicle position,
// and length / width / height values.
//-------------------------------------------------------------------------
class CVehicleCoverBoundInfo
{
public:

	static bool IsBoundActive(const CVehicleCoverBoundInfo& rActiveBound, const CVehicleCoverBoundInfo& rOtherBound);

#if __BANK
	static void RenderCoverBound(const CVehicle& rVeh, const atArray<CVehicleCoverBoundInfo>& rCoverBoundArray, s32 iIndex, bool bTreatAsActive, bool bRenderInGreen);
#endif // __BANK

	atHashWithStringNotFinal			m_Name;
	Vector3								m_Position;
	float								m_Length;
	float								m_Width;
	float								m_Height;
	atArray<atHashWithStringNotFinal>	m_ActiveBoundExclusions;	// Bounds to ignore when we're using this bound

	PAR_SIMPLE_PARSABLE
};

//-------------------------------------------------------------------------
// Cover bounding box determined by applying offsets to the vehicle dummy bound
//-------------------------------------------------------------------------
class CVehicleCoverBoundOffsetInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleCoverBoundOffsetInfo info pointer
	static const char* GetNameFromInfo(const CVehicleCoverBoundOffsetInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleCoverBoundOffsetInfo info pointer from a name
	static const CVehicleCoverBoundOffsetInfo* GetInfoFromName(const char* name);	

	atHashWithStringNotFinal GetName() const { return m_Name; }
	float GetExtraSideOffset() const { return m_ExtraSideOffset; }
	float GetExtraForwardOffset() const { return m_ExtraForwardOffset; }
	float GetExtraBackwardOffset() const { return m_ExtraBackwardOffset; }
	float GetExtraZOffset() const { return m_ExtraZOffset; }
	const atArray<CVehicleCoverBoundInfo>& GetCoverBoundInfoArray() const { return m_CoverBoundInfos; }

	////////////////////////////////////////////////////////////////////////////////

private:

	atHashWithStringNotFinal			m_Name;
	float								m_ExtraSideOffset;
	float								m_ExtraForwardOffset;
	float								m_ExtraBackwardOffset;
	float								m_ExtraZOffset;
	atArray<CVehicleCoverBoundInfo>		m_CoverBoundInfos;

	PAR_SIMPLE_PARSABLE
};

//-------------------------------------------------------------------------
// Bicycle Specific Information
//-------------------------------------------------------------------------
class CBicycleInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////
	struct sGearClipSpeeds
	{
		float m_Speed;
		float m_ClipRate;

		PAR_SIMPLE_PARSABLE
	};

	// PURPOSE: Used by the PARSER.  Gets a name from a CBicycleInfo info pointer
	static const char* GetNameFromInfo(const CBicycleInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CBicycleInfo info pointer from a name
	static const CBicycleInfo* GetInfoFromName(const char* name);	

	atHashWithStringNotFinal GetName() const { return m_Name; }
	const atArray<sGearClipSpeeds>& GetCruiseGearClipSpeeds() const { return m_CruiseGearClipSpeeds; }
	const atArray<sGearClipSpeeds>& GetFastGearClipSpeeds() const { return m_FastGearClipSpeeds; }
	float GetSpeedToTriggerBicycleLean() const { return m_SpeedToTriggerBicycleLean; }
	float GetSpeedToTriggerStillTransition() const { return m_SpeedToTriggerStillTransition; }
	float GetDesiredSitRateMult() const { return m_DesiredSitRateMult; }
	float GetDesiredStandingRateMult() const { return m_DesiredStandingRateMult; }
	float GetDesiredStandingInAirRateMult() const { return m_DesiredStandingInAirRateMult; }
	float GetSitToStandPedalAccelerationScalar() const { return m_SitToStandPedalAccelerationScalar; }
	float GetSitToStandPedalMaxRate() const { return m_SitToStandPedalMaxRate; }
	float GetStillToSitPedalGearApproachRate() const { return m_StillToSitPedalGearApproachRate; }
	float GetSitPedalGearApproachRate() const { return m_SitPedalGearApproachRate; }
	float GetStandPedGearApproachRate() const { return m_StandPedalGearApproachRate; }
	float GetPedalToFreewheelBlendDuration() const { return m_PedalToFreewheelBlendDuration; }
	float GetFreewheelToPedalBlendDuration() const { return m_FreewheelToPedalBlendDuration; }
	float GetStillToSitToSitBlendOutDuration() const { return m_StillToSitToSitBlendOutDuration; }
	float GetSitTransitionJumpPrepBlendDuration() const { return m_SitTransitionJumpPrepBlendDuration; }
	float GetMinForcedInitialBrakeTime() const { return m_MinForcedInitialBrakeTime; }
	float GetMinForcedStillToSitTime() const { return m_MinForcedStillToSitTime; }
	float GetMinTimeInStandFreewheelState() const { return m_MinTimeInStandFreewheelState; }
	bool  GetIsFixieBike() const { return m_IsFixie; }
	bool  GetHasImpactAnims() const { return m_HasImpactAnims; }
	bool  GetUseHelmet() const { return m_UseHelmet; }
	bool  GetCanTrackStand() const { return m_CanTrackStand; }
	
	////////////////////////////////////////////////////////////////////////////////

private:

	atHashWithStringNotFinal	m_Name;
	atArray<sGearClipSpeeds>	m_CruiseGearClipSpeeds;
	atArray<sGearClipSpeeds>	m_FastGearClipSpeeds;
	float						m_SpeedToTriggerBicycleLean;
	float						m_SpeedToTriggerStillTransition;
	float						m_DesiredSitRateMult;
	float						m_DesiredStandingInAirRateMult;
	float						m_DesiredStandingRateMult;
	float						m_StillToSitPedalGearApproachRate;
	float						m_SitPedalGearApproachRate;
	float						m_StandPedalGearApproachRate;
	float						m_SitToStandPedalAccelerationScalar;
	float						m_SitToStandPedalMaxRate;
	float						m_PedalToFreewheelBlendDuration;
	float						m_FreewheelToPedalBlendDuration;
	float						m_StillToSitToSitBlendOutDuration;
	float						m_SitTransitionJumpPrepBlendDuration;
	float						m_MinForcedInitialBrakeTime;
	float						m_MinForcedStillToSitTime;
	float						m_MinTimeInStandFreewheelState;
	bool						m_IsFixie;
	bool						m_HasImpactAnims;
	bool						m_UseHelmet;
	bool						m_CanTrackStand;

	PAR_SIMPLE_PARSABLE
};

//-------------------------------------------------------------------------
// CPOVTuningInfo
//-------------------------------------------------------------------------

class CPOVTuningInfo
{
public:

	static const char* GetNameFromInfo(const CPOVTuningInfo* pInfo);
	static const CPOVTuningInfo* GetInfoFromName(const char* name);
	
	// Remaps the current value between the limits passed in, 0 or 1 either side of the limit
	static float SqueezeWithinLimits(float fVal, float fMin, float fMax);

	atHashWithStringNotFinal GetName() const { return m_Name; }

	float ComputeRestrictedDesiredLeanAngle(float fDesiredLeanAngle) const;
	void ComputeMinMaxPitchAngle(float fSpeedRatio, float& fMinPitch, float& fMaxPitch) const;
	float ComputeStickCenteredBodyLeanY(float fLeanAngle, float fSpeed) const;

private:

	atHashWithStringNotFinal	m_Name;

	float						m_StickCenteredBodyLeanYSlowMin;
	float						m_StickCenteredBodyLeanYSlowMax;
	float						m_StickCenteredBodyLeanYFastMin;
	float						m_StickCenteredBodyLeanYFastMax;
	float						m_BodyLeanXDeltaFromCenterMinBikeLean;
	float						m_BodyLeanXDeltaFromCenterMaxBikeLean;
	float						m_MinForwardsPitchSlow;
	float						m_MaxForwardsPitchSlow;
	float						m_MinForwardsPitchFast;
	float						m_MaxForwardsPitchFast;

	PAR_SIMPLE_PARSABLE
};

//-------------------------------------------------------------------------
// CClipSetMap
//-------------------------------------------------------------------------

class CClipSetMap
{
public:

	enum eConditionFlags
	{
		CF_CommonAnims		= BIT0,		
		CF_EntryAnims		= BIT1,		
		CF_InVehicleAnims	= BIT2,		
		CF_ExitAnims		= BIT3,		
		CF_BreakInAnims		= BIT4,
		CF_JackingAnims		= BIT5
	};

	enum eInformationFlags
	{
		IF_JackedPedExitsWillingly	= BIT0,
		IF_ScriptFlagOnly			= BIT1
	};

	struct sVariableClipSetMap
	{
		fwMvClipSetId			m_ClipSetId;
		fwMvClipSetVarId		m_VarClipSetId;
		fwFlags32				m_ConditionFlags;
		fwFlags32				m_InformationFlags;

		PAR_SIMPLE_PARSABLE
	};

	// Returns the name of this layout
	atHashWithStringNotFinal GetName() const { return m_Name; }

	const atArray<sVariableClipSetMap>& GetMaps() const { return m_Maps; }

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleTaskAnimInfo info pointer
	static const char* GetNameFromInfo(const CClipSetMap* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleTaskAnimInfo info pointer from a name
	static const CClipSetMap* GetInfoFromName(const char* name);	

	////////////////////////////////////////////////////////////////////////////////

private:

	atHashWithStringNotFinal		m_Name;
	atArray<sVariableClipSetMap>	m_Maps;

	PAR_SIMPLE_PARSABLE

};

//-------------------------------------------------------------------------
// CFirstPersonDriveByAxisData
//-------------------------------------------------------------------------

class CFirstPersonDriveByLookAroundSideData
{
public:

	class CAxisData
	{
	public:

		float		m_Offset;
		Vector2		m_AngleToBlendInOffset;

		PAR_SIMPLE_PARSABLE;
	};

	const CAxisData *GetAxis(int i) const 
	{ 
		if(vehicleVerifyf(i >= 0 && i < m_Offsets.GetCount(),"Invalid axis index! %d", i))
		{
			return &m_Offsets[i]; 
		}

		return NULL;
	}

	//! Offsets.
	atFixedArray<CAxisData, 3>	m_Offsets;

	//! Pitch.
	Vector2		m_ExtraRelativePitch;
	Vector2		m_AngleToBlendInExtraPitch;

	PAR_SIMPLE_PARSABLE;
};

class CFirstPersonDriveByWheelClipData
{
public:
	Vector2		m_HeadingLimitsLeft;
	Vector2		m_HeadingLimitsRight;
	Vector2		m_PitchLimits;
	Vector2		m_PitchOffset;
	Vector2		m_WheelAngleLimits;
	Vector2		m_WheelAngleOffset;

	float		m_MaxWheelOffsetY;
	float		m_WheelClipLerpInRate;
	float		m_WheelClipLerpOutRate;

	PAR_SIMPLE_PARSABLE;
};

class CFirstPersonDriveByLookAroundData
{
public:

	// Get the name of this seat override info
	atHashWithStringNotFinal	GetName() const { return m_Name; }

	bool GetAllowLookback() const { return m_AllowLookback; }

	const CFirstPersonDriveByLookAroundSideData &GetDataLeft() const { return m_DataLeft; }
	const CFirstPersonDriveByLookAroundSideData &GetDataRight() const { return m_DataRight; }

	const CFirstPersonDriveByWheelClipData &GetWheelClipInfo() const { return m_WheelClipInfo; }

	atHashWithStringNotFinal	m_Name;
	bool m_AllowLookback;
	//! Angles.
	Vector2 m_HeadingLimits;

	CFirstPersonDriveByLookAroundSideData m_DataLeft;
	CFirstPersonDriveByLookAroundSideData m_DataRight;

	CFirstPersonDriveByWheelClipData m_WheelClipInfo;

	PAR_SIMPLE_PARSABLE;
};

//-------------------------------------------------------------------------
// Class which contains a vehicle's seat and entry point layout
// this defines how a ped interacts with a vehicle in terms of animations 
// and actions
//-------------------------------------------------------------------------
class CVehicleLayoutInfo
{
public:

	// Seat structure containing hashes to seat and seat anim infos
	struct sSeat
	{
		CVehicleSeatInfo*			m_SeatInfo;
		CVehicleSeatAnimInfo*		m_SeatAnimInfo;

		PAR_SIMPLE_PARSABLE
	};

	// Entry structure containing hashes to entry point and entry anim infos
	struct sEntryPoint
	{
		CVehicleEntryPointInfo*		m_EntryPointInfo;
		CVehicleEntryPointAnimInfo*	m_EntryPointAnimInfo;

		PAR_SIMPLE_PARSABLE
	};

	struct sCellphoneClipsets
	{
		fwMvClipSetId m_CellphoneClipsetDS;
		fwMvClipSetId m_CellphoneClipsetPS;
		fwMvClipSetId m_CellphoneClipsetDS_FPS;
		fwMvClipSetId m_CellphoneClipsetPS_FPS;

		PAR_SIMPLE_PARSABLE
	};

	// Layout Flags
	enum eLayoutFlags
	{
		NoDriver							= BIT0,		// Vehicle is not drivable (e.g. trailer)
		StreamAnims							= BIT1,		// Attempt to stream anims on the fly rather than when created
		WaitForRerservationInGroup 			= BIT2,		// Wait for group leader's reservation before entering yourself
		WarpIntoAndOut						= BIT3,		// Ignores entry points, and warps entering peds in and out
		BikeLeansUnlessMoving				= BIT4,		// Use system to lean when stationary (bikes / bicycles)
		UsePickUpPullUp						= BIT5,		// Use pick up / pull up system when entering vehicle (bikes / bicycles)
		AllowEarlyDoorAndSeatUnreservation	= BIT6,		// Allow peds to unreserve the door and seat early
		UseVanOpenDoorBlendParams			= BIT7,		// Signal mapping for van open door anims is different
		UseStillToSitTransition				= BIT8,		// Has transition states between sit and still animations (bikes / bicycles)
		MustCloseDoor						= BIT9,		// Forces ped to use close door, blocking interrupts from input
		UseDoorOscillation					= BIT10,	// Adds a small spring-back to the vehicle door when opened fully
		NoArmIkOnInsideCloseDoor			= BIT11,	// Disables use of arm IK during inside close door animations
		NoArmIkOnOutsideCloseDoor			= BIT12,	// Disables use of arm IK during outside close door animations
		UseLeanSteerAnims					= BIT13,	// Blends between three steering anims based on player input
		NoArmIkOnOutsideOpenDoor			= BIT14,	// Disables use of arm IK during outside open door animations
		UseSteeringWheelIk					= BIT15,	// IK arm position for peds using female skeletons
		PreventJustPullingOut				= BIT16,	// Block interrupt during jack anim
		DontOrientateAttachWithWorldUp		= BIT17,	// Rotate entry with vehicle, not world aligned
		OnlyExitIfDoorIsClosed				= BIT18,	// Block exit on open door, for vehicles where ped is hidden
		DisableJackingAndBusting			= BIT19,	// Disable vehicle actions by police during wanted level
		ClimbUpAfterOpenDoor				= BIT20,	// Entry state order will be "Open Door" -> "Climb Up" -> "Get In"
		UseFinerAlignTolerance				= BIT21,	// Force a more accurate align before entering
		Use2DBodyBlend						= BIT22,	// Blends between five steering anims based on player input (aircraft sticks)
		IgnoreFrontSeatsWhenOnVehicle		= BIT23,	// Or inside vehicle, like the cargobob or titan
		AutomaticCloseDoor					= BIT24,	// Skip close door state, drive door shut instead (scissor doors)
		WarpInWhenStoodOnTop				= BIT25,	// Warps into driver seat when on top (blimp)
		DisableFastPoseWhenDrivebying		= BIT26,	// Bring player down to normal sit pose before performing driveby
		DisableTargetRearDoorOpenRatio		= BIT27,	// Allow rear door to open 100% if someone is using front door at same time
		PreventInterruptAfterClimbUp		= BIT28,	// Block interrupt between climb up and enter vehicle
		PreventInterruptAfterOpenDoor		= BIT29,	// Block interrupt between open door and enter vehicle
		UseLowerDoorBlockTest				= BIT30,	// Lowers door collision check when entering vehicle
		LockedForSpecialEdition				= BIT31,	// Prevents entry unless allowed by script native
	};

public:

	// Returns the name of this layout
	atHashWithStringNotFinal GetName() const { return m_Name; }

	// Seat info accessors
	//////////////////////

	// Get the number of seats in this layout
	s32 GetNumSeats() const					{ return m_Seats.GetCount(); }
	// Get a seat info by index
	const CVehicleSeatInfo*		GetSeatInfo(s32 iIndex) const;
	// Get a seat anim info by index
	const CVehicleSeatAnimInfo* GetSeatAnimationInfo(s32 iIndex) const;

	// Entry point info accessors
	/////////////////////////////

	// Get the number of entry points in this layout
	s32 GetNumEntryPoints() const			{ return m_EntryPoints.GetCount(); }
	// Get a entry point info by index
	const CVehicleEntryPointInfo* GetEntryPointInfo(s32 iIndex) const;
	// Get a entry point anim info by index
	const CVehicleEntryPointAnimInfo* GetEntryPointAnimInfo(int iIndex) const;

	bool ValidateEntryPoints();

	const CAnimRateSet& GetAnimRateSet() const;
	const CBicycleInfo* GetBicycleInfo() const { return m_BicycleInfo; }

	// Layout Flags (see above enum comments for usage)
	///////////////////////////////////////////////////

	// Returns if this vehicle layout has a driver
	bool GetHasDriver() const { return !m_LayoutFlags.IsFlagSet(NoDriver); }

	// Returns if this layout should try to stream in the anims
	bool GetStreamAnims() const { return m_LayoutFlags.IsFlagSet(StreamAnims); }

	bool GetWaitForRerservationInGroup() const { return m_LayoutFlags.IsFlagSet(WaitForRerservationInGroup); }

	bool GetWarpInAndOut() const { return m_LayoutFlags.IsFlagSet(WarpIntoAndOut); }

	bool GetBikeLeansUnlessMoving() const { return m_LayoutFlags.IsFlagSet(BikeLeansUnlessMoving); }

	bool GetUsePickUpPullUp() const { return m_LayoutFlags.IsFlagSet(UsePickUpPullUp); }

	bool GetAllowEarlyDoorAndSeatUnreservation() const { return m_LayoutFlags.IsFlagSet(AllowEarlyDoorAndSeatUnreservation); }
	bool GetUseVanOpenDoorBlendParams() const { return m_LayoutFlags.IsFlagSet(UseVanOpenDoorBlendParams); }
	bool GetUseStillToSitTransition() const { return m_LayoutFlags.IsFlagSet(UseStillToSitTransition); }
	bool GetMustCloseDoor() const { return m_LayoutFlags.IsFlagSet(MustCloseDoor); }
	bool GetUseDoorOscillation() const { return m_LayoutFlags.IsFlagSet(UseDoorOscillation); }
	bool GetNoArmIkOnInsideCloseDoor() const { return m_LayoutFlags.IsFlagSet(NoArmIkOnInsideCloseDoor); }
	bool GetNoArmIkOnOutsideCloseDoor() const { return m_LayoutFlags.IsFlagSet(NoArmIkOnOutsideCloseDoor); }
	bool GetNoArmIkOnOutsideOpenDoor() const { return m_LayoutFlags.IsFlagSet(NoArmIkOnOutsideOpenDoor); }
	bool GetUseLeanSteerAnims() const { return m_LayoutFlags.IsFlagSet(UseLeanSteerAnims); }
	bool GetUseSteeringWheelIk() const { return m_LayoutFlags.IsFlagSet(UseSteeringWheelIk); }
	bool GetPreventJustPullingOut() const { return m_LayoutFlags.IsFlagSet(PreventJustPullingOut); }
	bool GetDontOrientateAttachWithWorldUp() const { return m_LayoutFlags.IsFlagSet(DontOrientateAttachWithWorldUp); }
	bool GetOnlyExitIfDoorIsClosed() const { return m_LayoutFlags.IsFlagSet(OnlyExitIfDoorIsClosed); }
	bool GetDisableJackingAndBusting() const { return m_LayoutFlags.IsFlagSet(DisableJackingAndBusting); }
	bool GetClimbUpAfterOpenDoor() const { return m_LayoutFlags.IsFlagSet(ClimbUpAfterOpenDoor); }
	bool GetUseFinerAlignTolerance() const { return m_LayoutFlags.IsFlagSet(UseFinerAlignTolerance); }
	bool GetUse2DBodyBlend() const { return m_LayoutFlags.IsFlagSet(Use2DBodyBlend); }
	bool GetIgnoreFrontSeatsWhenOnVehicle() const { return m_LayoutFlags.IsFlagSet(IgnoreFrontSeatsWhenOnVehicle); }
	bool GetAutomaticCloseDoor() const { return m_LayoutFlags.IsFlagSet(AutomaticCloseDoor); }
	bool GetWarpInWhenStoodOnTop() const { return m_LayoutFlags.IsFlagSet(WarpInWhenStoodOnTop); }
	bool GetDisableFastPoseWhenDrivebying() const { return m_LayoutFlags.IsFlagSet(DisableFastPoseWhenDrivebying); }
	bool GetDisableTargetRearDoorOpenRatio() const { return m_LayoutFlags.IsFlagSet(DisableTargetRearDoorOpenRatio); }
	bool GetPreventInterruptAfterClimbUp() const { return m_LayoutFlags.IsFlagSet(PreventInterruptAfterClimbUp); } 
	bool GetPreventInterruptAfterOpenDoor() const { return m_LayoutFlags.IsFlagSet(PreventInterruptAfterOpenDoor); }
	bool GetUseLowerDoorBlockTest() const { return m_LayoutFlags.IsFlagSet(UseLowerDoorBlockTest); }
	bool GetLockedForSpecialEdition() const { return m_LayoutFlags.IsFlagSet(LockedForSpecialEdition); }

	// Should be a bone?
	Vector3 GetSteeringWheelOffset() const { return m_SteeringWheelOffset; }

	// Temp: should remove this when we can stream in clipsets as and when needed
	// Returns number of streamed anim dictionaries referenced within this vehicle layout info
	// And fills out pAnimDictIndicesList with these anim dict indices
	int GetAllStreamedAnimDictionaries(s32* pAnimDictIndicesList, int iAnimDictIndicesListSize) const;

	// Returns the clip set name for our hands up busted anims
	atHashWithStringNotFinal GetHandsUpClipSetId() const { return m_HandsUpClipSetId; }

	float GetMaxXAcceleration() const { return m_MaxXAcceleration; }
	float GetBodyLeanXApproachSpeed() const { return m_BodyLeanXApproachSpeed; }
	float GetBodyLeanXSmallDelta() const { return m_BodyLeanXSmallDelta; }
	float GetLookBackApproachSpeedScale() const { return m_LookBackApproachSpeedScale; }

	s32 GetNumFirstPersonAddiveClipSets() const			{ return m_FirstPersonAdditiveIdleClipSets.GetCount(); }
	fwMvClipSetId GetFirstPersonAddiveClipSet(s32 iIndex) const 
	{
		Assertf(iIndex >= 0 && iIndex < GetNumFirstPersonAddiveClipSets(),"Invalid additive idle index!");
		return m_FirstPersonAdditiveIdleClipSets[iIndex]; 
	}

	s32 GetNumFirstPersonRoadRageClipSets() const			{ return m_FirstPersonRoadRageClipSets.GetCount(); }
	fwMvClipSetId GetFirstPersonRoadRageClipSet(s32 iIndex) const 
	{
		Assertf(iIndex >= 0 && iIndex < GetNumFirstPersonRoadRageClipSets(),"Invalid road rage index!");
		return m_FirstPersonRoadRageClipSets[iIndex]; 
	}

	fwMvClipSetId GetCellphoneClipsetDS() const { return m_CellphoneClipsets.m_CellphoneClipsetDS; }
	fwMvClipSetId GetCellphoneClipsetPS() const { return m_CellphoneClipsets.m_CellphoneClipsetPS; }
	fwMvClipSetId GetCellphoneClipsetDS_FPS() const { return m_CellphoneClipsets.m_CellphoneClipsetDS_FPS; }
	fwMvClipSetId GetCellphoneClipsetPS_FPS() const { return m_CellphoneClipsets.m_CellphoneClipsetPS_FPS; }

private:

	// Layout name
	atHashWithStringNotFinal			m_Name;
	// Seat and seat anim info
	atArray<sSeat>						m_Seats;
	// Entry and entry anim info
	atArray<sEntryPoint>				m_EntryPoints;
	fwFlags32							m_LayoutFlags;
	CBicycleInfo*						m_BicycleInfo;
	CAnimRateSet*						m_AnimRateSet;
	// This is the clip set we want to use when getting busted in a vehicle and not able to exit the vehicle
	atHashWithStringNotFinal			m_HandsUpClipSetId;
	// Offset from seat to steering wheel centre
	Vector3								m_SteeringWheelOffset;
	float								m_MaxXAcceleration;
	float								m_BodyLeanXApproachSpeed;
	float								m_BodyLeanXSmallDelta;
	float								m_LookBackApproachSpeedScale;

	atArray<fwMvClipSetId>				m_FirstPersonAdditiveIdleClipSets;

	atArray<fwMvClipSetId>				m_FirstPersonRoadRageClipSets;

	sCellphoneClipsets					m_CellphoneClipsets;

	PAR_SIMPLE_PARSABLE
};

#endif // INC_VEHICLE_LAYOUT_H