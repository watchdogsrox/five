//
// Vehicles/Metadata/VehicleSeatInfo.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_VEHICLE_SEAT_INFO_H
#define INC_VEHICLE_SEAT_INFO_H

////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "atl/hashstring.h"
#include "atl/string.h"
#include "fwanimation/animdefines.h"
#include "fwtl/pool.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

// Game headers
#include "Task/Physics/TaskAnimatedAttach.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Groups of weapon hashes which are assigned to CDrivebyAnimInfo's to 
//			determine whether a paticular weapontype can be used in the driveby seat
////////////////////////////////////////////////////////////////////////////////

class CDrivebyWeaponGroup
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleDriveByAnimInfo info pointer
	static const char* GetNameFromInfo(const CDrivebyWeaponGroup* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleDriveByAnimInfo info pointer from a name
	static const CDrivebyWeaponGroup* GetInfoFromName(const char* name);	

	atHashWithStringNotFinal GetName() const { return m_Name; }

	const atArray<atHashWithStringNotFinal>& GetWeaponGroupNames() const { return m_WeaponGroupNames; }
	const atArray<atHashWithStringNotFinal>& GetWeaponTypeNames() const { return m_WeaponTypeNames; }

private:

	////////////////////////////////////////////////////////////////////////////////

	atHashWithStringNotFinal			m_Name;
	atArray<atHashWithStringNotFinal>	m_WeaponGroupNames;
	atArray<atHashWithStringNotFinal>	m_WeaponTypeNames;

	PAR_SIMPLE_PARSABLE

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Class which contains driveby anim info for a particular 
//			seat and armament type
////////////////////////////////////////////////////////////////////////////////

class CVehicleDriveByAnimInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleDriveByAnimInfo info pointer
	static const char* GetNameFromInfo(const CVehicleDriveByAnimInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleDriveByAnimInfo info pointer from a name
	static const CVehicleDriveByAnimInfo* GetInfoFromName(const char* name);

	////////////////////////////////////////////////////////////////////////////////

	struct sAltClips
	{
		u32 GetNameHash(){return m_ClipSet.GetHash();}
		atArray<atHashWithStringNotFinal> m_Weapons;
		fwMvClipId m_ClipSet;

		PAR_SIMPLE_PARSABLE;
	};

	enum eDrivebyNetworkType
	{
		STD_CAR_DRIVEBY = 0,
		BIKE_DRIVEBY,
		TRAILER_DRIVEBY,
		REAR_HELI_DRIVEBY,
		THROWN_GRENADE_DRIVEBY,
		MOUNTED_DRIVEBY,
		MOUNTED_THROW
	};

	// PURPOSE : Get the name of this driveby anim info
	atHashWithStringNotFinal	GetName() const { return m_Name; }

	// PURPOSE : Get the weapon group	
	const CDrivebyWeaponGroup*	GetWeaponGroup() const { return m_WeaponGroup; }

	// PURPOSE : Get the clipset
	fwMvClipSetId				GetClipSet() const { return fwMvClipSetId(m_DriveByClipSet); }

	// PURPOSE: Get the restricted clipset (used for FPS drivebys)
	fwMvClipSetId				GetRestrictedClipSet() const { return fwMvClipSetId(m_RestrictedDriveByClipSet); }

	fwMvClipSetId				GetVehicleMeleeClipSet() const { return fwMvClipSetId(m_VehicleMeleeClipSet); }

	fwMvClipSetId				GetFirstPersonVehicleMeleeClipSet() const { return fwMvClipSetId(m_FirstPersonVehicleMeleeClipSet); }

	// PURPOSE : Get the first person clipset (if alternative requested and not found, return standard)
	fwMvClipSetId				GetFirstPersonClipSet(u32 uWeaponHash = 0) const;

	// PURPOSE : Get the network enum
	eDrivebyNetworkType			GetNetwork() const { return m_Network; }

	// PURPOSE : Does this driveby anim info work for the weapon slot
	bool						ContainsWeaponGroup(u32 uHash) const;

	// PURPOSE : Does this driveby anim info work for the weapon type
	bool						ContainsWeaponType(u32 uHash) const;

	// PURPOSE : Should we use override sweeps for this weapon, or global values for this seat?
	bool						GetUseOverrideAngles() const { return m_UseOverrideAngles; }

	// PURPOSE : Animation authored sweep values for this weapon group only (override main ones defined in CDriveByInfo)
	float						GetOverrideMinAimAngle() const { return m_OverrideMinAimAngle; }						
	float						GetOverrideMaxAimAngle() const { return m_OverrideMaxAimAngle; }

	// PURPOSE : Code restricted sweep values for this weapon group only (override main ones defined in CDriveByInfo)
	float						GetOverrideMinRestrictedAimAngle() const { return m_OverrideMinRestrictedAimAngle; }
	float						GetOverrideMaxRestrictedAimAngle() const { return m_OverrideMaxRestrictedAimAngle; }

	// PURPOSE : Should we use override sweeps for this seat in first person?
	bool						GetOverrideAnglesInThirdPersonOnly() const { return m_OverrideAnglesInThirdPersonOnly; }

private:

	////////////////////////////////////////////////////////////////////////////////

	atHashWithStringNotFinal	m_Name;							// Name of this CVehicleDriveByAnimInfo 	
	atHashWithStringNotFinal	m_DriveByClipSet;				// Clipset for this driveby anim info
	atHashWithStringNotFinal	m_FirstPersonDriveByClipSet;	// First person clipset for this driveby anim info
	atArray<sAltClips>			m_AltFirstPersonDriveByClipSets; // Alt first person clipsets
	atHashWithStringNotFinal	m_RestrictedDriveByClipSet;		// Restricted clipset for this driveby anim info (used for passenger peds for FPS drivebys); essentially replaces m_DriveByClipSet.
	atHashWithStringNotFinal	m_VehicleMeleeClipSet;
	atHashWithStringNotFinal	m_FirstPersonVehicleMeleeClipSet;
	eDrivebyNetworkType			m_Network;						// MoVE network to use
	const CDrivebyWeaponGroup*	m_WeaponGroup;					// The weapon types that can use these anims and network

	// These aim sweep values override the main ones defined in CDriveByInfo for this animation only
	bool						m_UseOverrideAngles;
	bool						m_OverrideAnglesInThirdPersonOnly;
	float						m_OverrideMinAimAngle;
	float						m_OverrideMaxAimAngle;
	float						m_OverrideMinRestrictedAimAngle;
	float						m_OverrideMaxRestrictedAimAngle;

	PAR_SIMPLE_PARSABLE

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Class which contains driveby anim info for a particular 
//			seat and armament type
////////////////////////////////////////////////////////////////////////////////

class CVehicleDriveByInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleDriveByInfo info pointer
	static const char* GetNameFromInfo(const CVehicleDriveByInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleDriveByInfo info pointer from a name
	static const CVehicleDriveByInfo* GetInfoFromName(const char* name);

	////////////////////////////////////////////////////////////////////////////////

	enum eDriveByFlags
	{
		AdjustHeight						= BIT0,
		DampenRecoil						= BIT1,
		AllowDamageToVehicle				= BIT2,
		AllowDamageToVehicleOccupants		= BIT3,
		UsePedAsCamEntity					= BIT4,
		NeedToOpenDoors						= BIT5,
		UseThreeAnimIntroOutro				= BIT6,
		UseMountedProjectileTask			= BIT7,
		UseThreeAnimThrow					= BIT8,
		UseSpineAdditive					= BIT9,
		LeftHandedProjctiles				= BIT10,
		LeftHandedFirstPersonAnims			= BIT11,
		LeftHandedUnarmedFirstPersonAnims	= BIT12,
		WeaponAttachedToLeftHand1HOnly		= BIT13,
		UseBlockingAnimsOutsideAimRange		= BIT14,
		AllowSingleSideOvershoot			= BIT15,
		UseAimCameraInThisSeat				= BIT16,
	};

public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE : Get the name of this driveby info
	atHashWithStringNotFinal GetName() const			{ return m_Name; }

	// PURPOSE : Anim authored min sweep values needed to determine the normalised anim phase parameter passed into the move network
	float GetMinAimSweepHeadingAngleDegs(const CPed*) const;				
	float GetMaxAimSweepHeadingAngleDegs(const CPed*) const;

	// PURPOSE : Anim authored min sweep values needed to determine the normalised anim phase parameter passed into the move network
	float GetFirstPersonMinAimSweepHeadingAngleDegs() const			{ return m_FirstPersonMinAimSweepHeadingAngleDegs; }						
	float GetFirstPersonMaxAimSweepHeadingAngleDegs() const			{ return m_FirstPersonMaxAimSweepHeadingAngleDegs; }
	float GetFirstPersonUnarmedMinAimSweepHeadingAngleDegs() const	{ return m_FirstPersonUnarmedMinAimSweepHeadingAngleDegs; }						
	float GetFirstPersonUnarmedMaxAimSweepHeadingAngleDegs() const	{ return m_FirstPersonUnarmedMaxAimSweepHeadingAngleDegs; }

	// PURPOSE : Code restricted values (so we can reuse a std driveby clipset with a van, but restrict the sweep values)
	float GetMinRestrictedAimSweepHeadingAngleDegs(const CPed*) const;
	float GetMaxRestrictedAimSweepHeadingAngleDegs(const CPed*) const;

	// PURPOSE : Min/Max smash window angles
	float GetMinSmashWindowAngleDegs() const		{ return m_MinSmashWindowAngleDegs; }
	float GetMaxSmashWindowAngleDegs() const		{ return m_MaxSmashWindowAngleDegs; }

	// PURPOSE : Min/Max smash window angles
	float GetMinSmashWindowAngleFirstPersonDegs() const		{ return m_MinSmashWindowAngleFirstPersonDegs; }
	float GetMaxSmashWindowAngleFirstPersonDegs() const		{ return m_MaxSmashWindowAngleFirstPersonDegs; }

	float GetMaxSpeedParam() const							{ return m_MaxSpeedParam; }
	float GetMaxLongitudinalLeanBlendWeightDelta() const	{ return m_MaxLongitudinalLeanBlendWeightDelta; }
	float GetMaxLateralLeanBlendWeightDelta() const			{ return m_MaxLateralLeanBlendWeightDelta; }
	float GetApproachSpeedToWithinMaxBlendDelta() const		{ return m_ApproachSpeedToWithinMaxBlendDelta; }

	// PURPOSE : Animation info for this driveby
	const atArray<CVehicleDriveByAnimInfo*>& GetDriveByAnimInfos() const { return m_DriveByAnimInfos; }

	// PURPOSE : Custom camera for this driveby
	atHashWithStringNotFinal GetDriveByCamera() const	{ return m_DriveByCamera; }
	atHashWithStringNotFinal GetPovDriveByCamera() const	{ return m_PovDriveByCamera; }

	bool CanUseWeaponGroup(u32 uGroupHash) const;
	bool CanUseWeaponType(u32 uTypeHash) const;
	
	u32	 GetAllFlags() const								{ return m_DriveByFlags.GetAllFlags(); }
	bool GetAdjustHeight() const							{ return m_DriveByFlags.IsFlagSet(AdjustHeight); }
	bool GetDampenRecoil() const							{ return m_DriveByFlags.IsFlagSet(DampenRecoil); }
	bool GetAllowDamageToVehicle() const					{ return m_DriveByFlags.IsFlagSet(AllowDamageToVehicle); }
	bool GetAllowDamageToVehicleOccupants() const			{ return m_DriveByFlags.IsFlagSet(AllowDamageToVehicleOccupants); }
	bool GetUsePedAsCamEntity() const						{ return m_DriveByFlags.IsFlagSet(UsePedAsCamEntity); }
	bool GetNeedToOpenDoors() const							{ return m_DriveByFlags.IsFlagSet(NeedToOpenDoors); }
	bool GetUseThreeAnimIntroOutro() const					{ return m_DriveByFlags.IsFlagSet(UseThreeAnimIntroOutro); }
	bool GetUseMountedProjectileTask() const				{ return m_DriveByFlags.IsFlagSet(UseMountedProjectileTask); }	
	bool GetUseThreeAnimThrow() const						{ return m_DriveByFlags.IsFlagSet(UseThreeAnimThrow); }	
	bool GetUseSpineAdditive() const						{ return m_DriveByFlags.IsFlagSet(UseSpineAdditive); }
	bool GetLeftHandedProjctiles() const					{ return m_DriveByFlags.IsFlagSet(LeftHandedProjctiles);}
	bool GetLeftHandedFirstPersonAnims() const				{ return m_DriveByFlags.IsFlagSet(LeftHandedFirstPersonAnims);}
	bool GetLeftHandedUnarmedFirstPersonAnims() const		{ return m_DriveByFlags.IsFlagSet(LeftHandedUnarmedFirstPersonAnims);}
	bool GetWeaponAttachedToLeftHand1HOnly() const			{ return m_DriveByFlags.IsFlagSet(WeaponAttachedToLeftHand1HOnly);}
	bool GetUseBlockingAnimsOutsideAimRange() const			{ return m_DriveByFlags.IsFlagSet(UseBlockingAnimsOutsideAimRange);}
	bool GetAllowSingleSideOvershoot() const				{ return m_DriveByFlags.IsFlagSet(AllowSingleSideOvershoot);}
	bool GetUseAimCameraInThisSeat() const					{ return m_DriveByFlags.IsFlagSet(UseAimCameraInThisSeat);}

	float GetSpineAdditiveBlendInDelay() const			{ return m_SpineAdditiveBlendInDelay; }
	float GetSpineAdditiveBlendInDurationStill() const	{ return m_SpineAdditiveBlendInDurationStill; }
	float GetSpineAdditiveBlendInDuration() const		{ return m_SpineAdditiveBlendInDuration; }
	float GetSpineAdditiveBlendOutDelay() const			{ return m_SpineAdditiveBlendOutDelay; }
	float GetSpineAdditiveBlendOutDuration() const		{ return m_SpineAdditiveBlendOutDuration; }

	float GetMinUnarmedDrivebyYawIfWindowRolledUp() const	{ return m_MinUnarmedDrivebyYawIfWindowRolledUp; }
	float GetMaxUnarmedDrivebyYawIfWindowRolledUp() const	{ return m_MaxUnarmedDrivebyYawIfWindowRolledUp; }

private:

	////////////////////////////////////////////////////////////////////////////////

	atHashWithStringNotFinal			m_Name;
	float								m_MinAimSweepHeadingAngleDegs;						// The minimum heading the anim allows us to driveby
	float								m_MaxAimSweepHeadingAngleDegs;						// The maximum heading the anim allows us to driveby
	float								m_FirstPersonMinAimSweepHeadingAngleDegs;			// The minimum heading the anim allows us to driveby
	float								m_FirstPersonMaxAimSweepHeadingAngleDegs;			// The maximum heading the anim allows us to driveby
	float								m_FirstPersonUnarmedMinAimSweepHeadingAngleDegs;	// The minimum heading the anim allows us to driveby
	float								m_FirstPersonUnarmedMaxAimSweepHeadingAngleDegs;	// The maximum heading the anim allows us to driveby	
	float								m_MinAimSweepPitchAngleDegs;						// The minimum pitch the anim allows us to driveby
	float								m_MaxAimSweepPitchAngleDegs;						// The maximum pitch the anim allows us to driveby
	float								m_MinRestrictedAimSweepHeadingAngleDegs;			// The minimum heading the data allows us to driveby
	float								m_MaxRestrictedAimSweepHeadingAngleDegs;			// The maximum heading the data allows us to driveby
	float								m_MinSmashWindowAngleDegs;							// The minimum heading the data allows us to smash window
	float								m_MaxSmashWindowAngleDegs;							// The maximum heading the data allows us to smash window
	float								m_MinSmashWindowAngleFirstPersonDegs;				// The minimum heading the data allows us to smash window 
	float								m_MaxSmashWindowAngleFirstPersonDegs;				// The maximum heading the data allows us to smash window
	float								m_MaxSpeedParam;		
	float								m_MaxLongitudinalLeanBlendWeightDelta;				// Max blend weight from centre during driveby
	float								m_MaxLateralLeanBlendWeightDelta;					// Max blend weight from centre during driveby
	float								m_ApproachSpeedToWithinMaxBlendDelta;				// How fast we drive to the max blend weight delta
	float								m_SpineAdditiveBlendInDelay;
	float								m_SpineAdditiveBlendInDurationStill;
	float								m_SpineAdditiveBlendInDuration;
	float								m_SpineAdditiveBlendOutDelay;
	float								m_SpineAdditiveBlendOutDuration;
	float								m_MinUnarmedDrivebyYawIfWindowRolledUp;
	float								m_MaxUnarmedDrivebyYawIfWindowRolledUp;

	atArray<CVehicleDriveByAnimInfo*>	m_DriveByAnimInfos;							// Array of driveby anim info pointers
	atHashWithStringNotFinal			m_DriveByCamera;
	atHashWithStringNotFinal			m_PovDriveByCamera;
	fwFlags32							m_DriveByFlags;

	PAR_SIMPLE_PARSABLE

	////////////////////////////////////////////////////////////////////////////////
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Class which contains info for a particular seat
////////////////////////////////////////////////////////////////////////////////

class CVehicleSeatInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleSeatInfo info pointer
	static const char* GetNameFromInfo(const CVehicleSeatInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleSeatInfo info pointer from a name
	static const CVehicleSeatInfo* GetInfoFromName(const char* name);

	////////////////////////////////////////////////////////////////////////////////

	enum eDefaultCarTask
	{
		TASK_DRIVE_WANDER = 0,
		TASK_HANGING_ON_VEHICLE,
		NUM_DEFAULT_CAR_TASKS
	};

	////////////////////////////////////////////////////////////////////////////////

	enum eSeatFlags
	{
		IsFrontSeat							= BIT0,		// Flags front seat
		IsIdleSeat							= BIT1,		// Flags rear (idle) seat, used for WORLD_VEHICLE_BOAT_IDLE and other things
		NoIndirectExit						= BIT2,		// Don't consider shuffle links when exiting
		PedInSeatTargetable					= BIT3,		// Ped in seat processed by target evaluator code
		KeepOnHeadProp						= BIT4,		// Don't auto remove head prop during SetPedInVehicle
		DontDetachOnWorldCollision			= BIT5,		// Don't allow collision to knock peds off standing seats
		DisableFinerAlignTolerance			= BIT6,		// Force a more accurate align before entering (per-seat version of VehicleLayoutInfo flag)
		IsExtraShuffleSeat					= BIT7,		// Flags a seat that can be manually shuffled to using a button input
		DisableWhenTrailerAttached			= BIT8,		// Flags a seat we can't enter if a trailer is attached to the vehicle
		UseSweepsForTurret					= BIT9,		// If a turret seat, should use 'sweep_' animations (player faces single direction) instead of 'sit_aim_' (player rotates)
		UseSweepLoopsForTurret				= BIT10,	// If a turret seat, will use looped sweep animations
		GivePilotHelmetForSeat				= BIT11,	// Will give pilot helmet for peds in this seat even if there's no weapons
		FakeSeat							= BIT12,	// Fake seat, used by script, not accessible via gameplay
		CameraSkipWaitingForTurretAlignment = BIT13,	// Camera will blend in immediately, not waiting for turret heading to initially adjust
		DisallowHeadProp					= BIT14,	// Prevents head props such as hats/helmets from being visible whilst in this seat
		PreventJackInWater					= BIT15		// Prevents jacking the seat when the vehicle is in water
	};

public:

	CVehicleSeatInfo() {}
	~CVehicleSeatInfo() {}

	//one time ptr assignment optimization
	void LoadLinkPtrs();

	// Get the name of this seat info
	atHashWithStringNotFinal GetName() const { return m_Name; }
	const char* GetBoneName() const { return m_SeatBoneName.c_str(); }

	const CVehicleSeatInfo*	GetShuffleLink() const { return m_ShuffleLinkPtr; }	
	const CVehicleSeatInfo*	GetShuffleLink2() const { return m_ShuffleLink2Ptr; }	
	const CVehicleSeatInfo* GetRearLink() const { return m_RearSeatLinkPtr; }	

	eDefaultCarTask GetDefaultCarTask() const { return m_DefaultCarTask; }

	bool GetIsFrontSeat() const { return m_SeatFlags.IsFlagSet(IsFrontSeat); }
	bool GetIsIdleSeat() const { return m_SeatFlags.IsFlagSet(IsIdleSeat); }
	bool GetNoIndirectExit() const { return m_SeatFlags.IsFlagSet(NoIndirectExit); }
	bool GetPedInSeatTargetable() const { return m_SeatFlags.IsFlagSet(PedInSeatTargetable); }
	bool GetKeepOnHeadProp() const { return m_SeatFlags.IsFlagSet(KeepOnHeadProp); }
	bool GetDontDetachOnWorldCollision() const { return m_SeatFlags.IsFlagSet(DontDetachOnWorldCollision); }
	bool GetDisableFinerAlignTolerance() const { return m_SeatFlags.IsFlagSet(DisableFinerAlignTolerance); }
	bool GetIsExtraShuffleSeat() const { return m_SeatFlags.IsFlagSet(IsExtraShuffleSeat); }
	bool GetDisableWhenTrailerAttached() const { return m_SeatFlags.IsFlagSet(DisableWhenTrailerAttached); }
	bool GetUseSweepsForTurret() const { return m_SeatFlags.IsFlagSet(UseSweepsForTurret); }
	bool GetUseSweepLoopsForTurret() const { return m_SeatFlags.IsFlagSet(UseSweepLoopsForTurret); }
	bool GetGivePilotHelmetForSeat() const { return m_SeatFlags.IsFlagSet(GivePilotHelmetForSeat); }
	bool GetIsFakeSeat() const { return m_SeatFlags.IsFlagSet(FakeSeat); }
	bool GetCameraSkipWaitingForTurretAlignment() const { return m_SeatFlags.IsFlagSet(CameraSkipWaitingForTurretAlignment); }
	bool GetPreventJackInWater() const { return m_SeatFlags.IsFlagSet(PreventJackInWater); }

	float GetHairScale() const { return m_HairScale; }

	const bool ShouldRemoveHeadProp(CPed& rPed) const;

private:

	// Not allowed to copy construct or assign
	CVehicleSeatInfo(const CVehicleSeatInfo& other);
	CVehicleSeatInfo& operator=(const CVehicleSeatInfo& other);


private:

	atHashWithStringNotFinal	m_Name;
	atString					m_SeatBoneName;
	const CVehicleSeatInfo*     m_ShuffleLinkPtr;
	atHashWithStringNotFinal    m_ShuffleLink;
	const CVehicleSeatInfo*     m_RearSeatLinkPtr;
	atHashWithStringNotFinal    m_RearSeatLink;
	eDefaultCarTask				m_DefaultCarTask;
	fwFlags32					m_SeatFlags;
	float						m_HairScale;
	atHashWithStringNotFinal	m_ShuffleLink2;
	const CVehicleSeatInfo*     m_ShuffleLink2Ptr;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Class which contains animation info for a particular seat
////////////////////////////////////////////////////////////////////////////////

class CVehicleSeatAnimInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CVehicleSeatAnimInfo info pointer
	static const char* GetNameFromInfo(const CVehicleSeatAnimInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CVehicleSeatInfo info pointer from a name
	static const CVehicleSeatAnimInfo* GetInfoFromName(const char* name);

	////////////////////////////////////////////////////////////////////////////////

	static fwMvClipId ms_DeadClipId;

	// Temp: these are within the move network now and can probably be removed with a bit of work
	// This is a list of all the per seat animation types that code needs to worry about
	enum eSeatAnimType
	{
		DEAD = 0,
		HELMET_ON,
		HOTWIRE,
		LOOK_BEHIND,
		SHUFFLE,
		SHUFFLE_PUSH_OUT,
		SHUNT,
		SHUNT_ALT,
		SIT,
		SMASH_WINDOW,
		SMASH_WINDSCREEN,
		START,
		THROUGH_WINDSCREEN,
		THROW_GRENADE
	};

	enum eInVehicleMoveNetwork
	{
		VEHICLE_DEFAULT = 0,
		VEHICLE_BICYCLE,
		VEHICLE_TURRET_SEATED,
		VEHICLE_TURRET_STANDING,
		NUM_INVEHICLEMOVENETWORKS,
	};

	// Seat Anim Flags
	enum eSeatAnimFlags
	{
		CanDetachViaRagdoll					= BIT0,		// Allows ragdoll on death and other cases
		WeaponAttachedToLeftHand			= BIT1,		// Attaches weapon to left hand for driveby instead of default right hand
		WeaponRemainsVisible				= BIT2,		// Weapon remains visible in hand, even when idle (hanging seat)
		AttachLeftHand						= BIT3,		// Flagging limb attached to vehicle for NM
		AttachLeftFoot						= BIT4,		// Flagging limb attached to vehicle for NM
		AttachRightHand						= BIT5,		// Flagging limb attached to vehicle for NM
		AttachRightFoot						= BIT6,		// Flagging limb attached to vehicle for NM
		CannotBeJacked						= BIT7,		// Disables jacking of this seat
		UseStandardInVehicleAnims			= BIT8,		// Use standard vehicle animations in move network
		UseBikeInVehicleAnims				= BIT9,		// Use bicycle/bike animations in move network
		UseJetSkiInVehicleAnims				= BIT10,	// Use jetski animations in move network
		HasPanicAnims						= BIT11,	// Use panic animations defined in external clipset rather than looking in base dictionary
		UseCloseDoorBlendAnims				= BIT12,	// Blend between two animations for close door, depending on door open ratio
		SupportsInAirState					= BIT13,	// Allows high/low sit animations when vehicle gets air
		UseBasicAnims						= BIT14,	// Use sit animation for seat (and not much else)
		PreventShuffleJack					= BIT15,	// Disables shuffle jacking of this seat
		RagdollWhenVehicleUpsideDown		= BIT16,	// Ragdolls when upside down (also requires CanDetachViaRagdoll)
		FallsOutWhenDead					= BIT17,	// Falls out either using ragdoll or dead_fall_out animation
		CanExitEarlyInCombat				= BIT18,	// Allow cops to exit early if they're wanting to aim with weapon
		UseBoatInVehicleAnims				= BIT19,	// Use boat animations in move network
		CanWarpToDriverSeatIfNoDriver		= BIT20,	// Allow warp to the driver seat if driver/pilot has bailed and we don't have indirect access
		NoIK								= BIT21,	// No hand IK to weapon's grip during 2H driveby
		DisableAbnormalExits				= BIT22,	// Stops flee, electrocution, and other events that could cause exit task
		SimulateBumpiness					= BIT23,	// Add randomness to lean params when going at speed
		IsLowerPrioritySeatInSP				= BIT24,	// Lower priority when evaluating seat / entry access
		KeepCollisionOnWhenInVehicle		= BIT25,	// Allows ped to be knocked off from hanging seats
		UseDirectEntryOnlyWhenEntering		= BIT26,	// Only direct entry, but may use shuffle links for exit
		UseTorsoLeanIK						= BIT27,	// Attempt to keep player's torso upright using IK
		UseRestrictedTorsoLeanIK			= BIT28,	// Attempt to keep player's torso upright using IK
		WarpIntoSeatIfStoodOnIt				= BIT29,	// Warp if standing directly on top and press enter
		RagdollAtExtremeVehicleOrientation	= BIT30,	// Ragdolls when at steep angle (also requires CanDetachViaRagdoll)
		NoShunts							= BIT31		// Disable collision shunt animations
	};

public:	

	// Get the name of this seat anim info
	atHashWithStringNotFinal	GetName() const { return m_Name; }

	// Temp Tez: hard coded anim dictionary index/anim hash
	bool FindAnim(eSeatAnimType nAnimType, s32& iDictIndex, u32& iAnimHash, bool bFirstPerson) const;
	static bool FindAnimInClipSet(const fwMvClipSetId &clipSetIdToSearch, u32 iDesiredAnimHash, s32& iDictIndex, u32& iAnimHash);
 
	// Getters for data
	const CVehicleDriveByInfo*		GetDriveByInfo() const { return m_DriveByInfo; } 
	fwMvClipSetId					GetSeatClipSetId() const 
	{ 
		if (vehicleVerifyf(m_InsideClipSetMap, "Couldn't find clipset map for seat anim info %s", m_Name.GetCStr()))
		{
			return m_InsideClipSetMap->GetMaps()[0].m_ClipSetId; 
		}
		return CLIP_SET_ID_INVALID;
	}
	fwMvClipSetId					GetFirstPersonSeatClipSetId() const 
	{ 
		if (vehicleVerifyf(m_InsideClipSetMap, "Couldn't find clipset map for seat anim info %s", m_Name.GetCStr()))
		{
			return m_InsideClipSetMap->GetMaps().GetCount() > 1 ? m_InsideClipSetMap->GetMaps()[1].m_ClipSetId : m_InsideClipSetMap->GetMaps()[0].m_ClipSetId; 
		}
		return CLIP_SET_ID_INVALID;
	}
	fwMvClipSetId					GetPanicClipSet() const { return fwMvClipSetId(m_PanicClipSet.GetHash()); }
	fwMvClipSetId					GetAgitatedClipSet() const { return fwMvClipSetId(m_AgitatedClipSet.GetHash()); }
	fwMvClipSetId					GetDuckedClipSet() const { return fwMvClipSetId(m_DuckedClipSet.GetHash()); }
	fwMvClipSetId					GetFemaleClipSet() const { return fwMvClipSetId(m_FemaleClipSet.GetHash()); }
	fwMvClipSetId					GetLowriderLeanClipSet() const { return fwMvClipSetId(m_LowriderLeanClipSet.GetHash()); }
	fwMvClipSetId					GetAltLowriderLeanClipSet() const { return fwMvClipSetId(m_AltLowriderLeanClipSet.GetHash()); }
#if CNC_MODE_ENABLED
	fwMvClipSetId					GetStunnedClipSet() const { return fwMvClipSetId(m_StunnedClipSet.GetHash()); }
#endif
	fwMvClipId						GetLowLODIdleAnim() const { return m_LowLODIdleAnim; }
	atHashWithStringNotFinal		GetSeatAmbientContext() const { return m_SeatAmbientContext; }
	eInVehicleMoveNetwork			GetInVehicleMoveNetwork() const { return m_InVehicleMoveNetwork; }

	bool  IsTurretSeat() const { return m_InVehicleMoveNetwork == VEHICLE_TURRET_SEATED || m_InVehicleMoveNetwork == VEHICLE_TURRET_STANDING; }
	bool  IsStandingTurretSeat() const { return m_InVehicleMoveNetwork == VEHICLE_TURRET_STANDING; }
	bool  GetCanExitEarlyInCombat() const	{ return m_SeatAnimFlags.IsFlagSet(CanExitEarlyInCombat); }
	bool  GetCanDetachViaRagdoll() const			{ return m_SeatAnimFlags.IsFlagSet(CanDetachViaRagdoll); }
	bool  GetWeaponRemainsVisible() const	{ return m_SeatAnimFlags.IsFlagSet(WeaponRemainsVisible); }
	bool  GetWeaponAttachedToLeftHand() const	{ return m_SeatAnimFlags.IsFlagSet(WeaponAttachedToLeftHand); }
	CTaskAnimatedAttach::eNMConstraintFlags GetNMConstraintFlags() const
	{
		s32 iFlags = 0;
		if (GetAttachLeftHand()) iFlags |= CTaskAnimatedAttach::NMCF_LEFT_HAND;
		if (GetAttachLeftFoot()) iFlags |= CTaskAnimatedAttach::NMCF_LEFT_FOOT;
		if (GetAttachRightHand()) iFlags |= CTaskAnimatedAttach::NMCF_RIGHT_HAND;
		if (GetAttachRightFoot()) iFlags |= CTaskAnimatedAttach::NMCF_RIGHT_FOOT;
		return (CTaskAnimatedAttach::eNMConstraintFlags) iFlags;
	}
	bool GetCannotBeJacked() const { return m_SeatAnimFlags.IsFlagSet(CannotBeJacked); }
	bool GetUseStandardInVehicleAnims() const { return m_SeatAnimFlags.IsFlagSet(UseStandardInVehicleAnims); }
	bool GetUseBikeInVehicleAnims() const { return m_SeatAnimFlags.IsFlagSet(UseBikeInVehicleAnims); }
	bool GetUseJetSkiInVehicleAnims() const { return m_SeatAnimFlags.IsFlagSet(UseJetSkiInVehicleAnims); }
	bool GetUseBoatInVehicleAnims() const { return m_SeatAnimFlags.IsFlagSet(UseBoatInVehicleAnims); }
	bool GetHasPanicAnims() const { return m_SeatAnimFlags.IsFlagSet(HasPanicAnims); }
	bool GetUseCloseDoorBlendAnims() const { return m_SeatAnimFlags.IsFlagSet(UseCloseDoorBlendAnims); }
	bool GetSupportsInAirState() const { return m_SeatAnimFlags.IsFlagSet(SupportsInAirState); }
	bool GetUseBasicAnims() const { return m_SeatAnimFlags.IsFlagSet(UseBasicAnims); }
	bool GetPreventShuffleJack() const { return m_SeatAnimFlags.IsFlagSet(PreventShuffleJack); }
	bool GetRagdollWhenVehicleUpsideDown() const { return m_SeatAnimFlags.IsFlagSet(RagdollWhenVehicleUpsideDown); }
	bool GetFallsOutWhenDead() const { return m_SeatAnimFlags.IsFlagSet(FallsOutWhenDead); }
	bool GetCanWarpToDriverSeatIfNoDriver() const { return m_SeatAnimFlags.IsFlagSet(CanWarpToDriverSeatIfNoDriver); }
	bool GetNoIK() const { return m_SeatAnimFlags.IsFlagSet(NoIK); }
	bool GetDisableAbnormalExits() const { return m_SeatAnimFlags.IsFlagSet(DisableAbnormalExits); }
	bool GetSimulateBumpiness() const { return m_SeatAnimFlags.IsFlagSet(SimulateBumpiness); }
	bool GetIsLowerPrioritySeatInSP() const { return m_SeatAnimFlags.IsFlagSet(IsLowerPrioritySeatInSP); }
	bool GetKeepCollisionOnWhenInVehicle() const { return m_SeatAnimFlags.IsFlagSet(KeepCollisionOnWhenInVehicle); }
	bool GetUseDirectEntryOnlyWhenEntering() const { return m_SeatAnimFlags.IsFlagSet(UseDirectEntryOnlyWhenEntering); }
	bool GetUseTorsoLeanIK() const { return m_SeatAnimFlags.IsFlagSet(UseTorsoLeanIK); }
	bool GetUseRestrictedTorsoLeanIK() const { return m_SeatAnimFlags.IsFlagSet(UseRestrictedTorsoLeanIK); }
	bool GetWarpIntoSeatIfStoodOnIt() const { return m_SeatAnimFlags.IsFlagSet(WarpIntoSeatIfStoodOnIt); }
	bool GetRagdollAtExtremeVehicleOrientation() const { return m_SeatAnimFlags.IsFlagSet(RagdollAtExtremeVehicleOrientation); }
	bool GetNoShunts() const { return m_SeatAnimFlags.IsFlagSet(NoShunts); }

	float GetSteeringSmoothing() const { return m_SteeringSmoothing; }

	atHashWithStringNotFinal GetExitToAimInfoName() const { return m_ExitToAimInfoName; }

	fwMvClipSetId GetMaleGestureClipSetId() const { return m_MaleGestureClipSetId; }
	fwMvClipSetId GetFemaleGestureClipSetId() const { return m_FemaleGestureClipSetId; }

	float GetFPSMinSteeringRateOverride() const { return m_FPSMinSteeringRateOverride; }
	float GetFPSMaxSteeringRateOverride() const { return m_FPSMaxSteeringRateOverride; }

	Vector3 GetSeatCollisionBoundsOffset() const { return m_SeatCollisionBoundsOffset; }

private:

	bool  GetAttachLeftHand() const			{ return m_SeatAnimFlags.IsFlagSet(AttachLeftHand); }
	bool  GetAttachLeftFoot() const			{ return m_SeatAnimFlags.IsFlagSet(AttachLeftFoot); }
	bool  GetAttachRightHand() const		{ return m_SeatAnimFlags.IsFlagSet(AttachRightHand); }
	bool  GetAttachRightFoot() const		{ return m_SeatAnimFlags.IsFlagSet(AttachRightFoot); }

	atHashWithStringNotFinal	m_Name;
	CVehicleDriveByInfo*		m_DriveByInfo;
	CClipSetMap* 				m_InsideClipSetMap;
	atHashWithStringNotFinal 	m_PanicClipSet;
	atHashWithStringNotFinal 	m_AgitatedClipSet;
	atHashWithStringNotFinal	m_DuckedClipSet;
	atHashWithStringNotFinal	m_FemaleClipSet;
	atHashWithStringNotFinal	m_LowriderLeanClipSet;
	atHashWithStringNotFinal	m_AltLowriderLeanClipSet;
#if CNC_MODE_ENABLED
	atHashWithStringNotFinal	m_StunnedClipSet;
#endif
	fwMvClipId 					m_LowLODIdleAnim;
	atHashWithStringNotFinal 	m_SeatAmbientContext;
	eInVehicleMoveNetwork	 	m_InVehicleMoveNetwork;
	fwFlags32				 	m_SeatAnimFlags;
	float					 	m_SteeringSmoothing;
	atHashWithStringNotFinal	m_ExitToAimInfoName;
	fwMvClipSetId				m_MaleGestureClipSetId;
	fwMvClipSetId				m_FemaleGestureClipSetId;
	float					 	m_FPSMinSteeringRateOverride;
	float					 	m_FPSMaxSteeringRateOverride;
	Vector3						m_SeatCollisionBoundsOffset;

	PAR_SIMPLE_PARSABLE
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Class which contains animation info for a particular seat
////////////////////////////////////////////////////////////////////////////////

class CSeatOverrideAnimInfo
{
public:

	// Seat Anim Flags
	enum eSeatOverrideAnimFlags
	{
		WeaponVisibleAttachedToRightHand = BIT0,
		UseBasicAnims					 = BIT1
	};

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CSeatOverrideAnimInfo info pointer
	static const char* GetNameFromInfo(const CSeatOverrideAnimInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CSeatOverrideAnimInfo info pointer from a name
	static const CSeatOverrideAnimInfo* GetInfoFromName(const char* name);

	////////////////////////////////////////////////////////////////////////////////

	// Get the name of this seat overrid anim info
	atHashWithStringNotFinal	GetName() const { return m_Name; }

	// Getters for data
	fwMvClipSetId				GetSeatClipSet() const { return m_SeatOverrideClipSet; }
	bool						GetWeaponVisibleAttachedToRightHand() const { return m_SeatOverrideAnimFlags.IsFlagSet(WeaponVisibleAttachedToRightHand); }
	bool						GetUseBasicAnims() const { return m_SeatOverrideAnimFlags.IsFlagSet(UseBasicAnims); }

private:

	atHashWithStringNotFinal	m_Name;
	fwMvClipSetId 				m_SeatOverrideClipSet;
	fwFlags32					m_SeatOverrideAnimFlags;

	PAR_SIMPLE_PARSABLE
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Class which maps a seat override anim info for a particular seat anim info
////////////////////////////////////////////////////////////////////////////////

class CSeatOverrideInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// Getters for data
	const CVehicleSeatAnimInfo*		GetSeatAnimInfo() const { return m_SeatAnimInfo; } 
	const CSeatOverrideAnimInfo*	GetSeatOverrideAnimInfo() const { return m_SeatOverrideAnimInfo; } 

	////////////////////////////////////////////////////////////////////////////////

private:

	CVehicleSeatAnimInfo*		m_SeatAnimInfo;
	CSeatOverrideAnimInfo*		m_SeatOverrideAnimInfo;

	PAR_SIMPLE_PARSABLE
};

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Class which contains a set of seat override infos, 
// script set the hash of the name of one of these InVehicleOverrideInfos
// when the ped starts or restarts the invehicle network we check to see if 
// our seat anim info matches any of the seat override infos and use the
// clipset associated with that instead
////////////////////////////////////////////////////////////////////////////////

class CInVehicleOverrideInfo
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Used by the PARSER.  Gets a name from a CInVehicleOverrideInfo info pointer
	static const char* GetNameFromInfo(const CInVehicleOverrideInfo* pInfo);

	// PURPOSE: Used by the PARSER.  Gets a CInVehicleOverrideInfo info pointer from a name
	static const CInVehicleOverrideInfo* GetInfoFromName(const char* name);

	////////////////////////////////////////////////////////////////////////////////

	// Get the name of this seat override info
	atHashWithStringNotFinal	GetName() const { return m_Name; }

	const CSeatOverrideAnimInfo* FindSeatOverrideAnimInfoFromSeatAnimInfo(const CVehicleSeatAnimInfo* pSeatAnimInfo) const;

	// Getters for data
	const atArray<CSeatOverrideInfo*>& GetSeatOverrideInfos() const { return m_SeatOverrideInfos; }

private:

	atHashWithStringNotFinal	m_Name;
	atArray<CSeatOverrideInfo*>	m_SeatOverrideInfos;

	PAR_SIMPLE_PARSABLE
};

////////////////////////////////////////////////////////////////////////////////

#endif // INC_VEHICLE_SEAT_INFO_H
