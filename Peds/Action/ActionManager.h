/////////////////////////////////////////////////////////////////////////////////
// Title	:	ActionManager.h
//
// These classes provide support for the "action manager".
// The "action manager" is a system used to simplify selecting and
// performing sequential "actions" in a lifelike and believable manner.
//
// An "action" is a set of movement, animation, and statistic changes that a
// ped might do in response to player/AI input or in response to collisions.
//
// This is used mainly for melee combat and is heavily data driven.
//
// Many of these classes describe the conditions under which a given action
// is suitable and the resulting movement, animation and statistic changes
// from performing that action.
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_ACTIONMANAGER_H
#define INC_ACTIONMANAGER_H

// Rage headers
#include "basetypes.h"
#include "vector/vector2.h"
#include "atl/array.h"
#include "scene/Entity.h"
#include "peds/peddefines.h"
#include "system/control.h"
#include "animation/AnimBones.h"
#include "crskeleton/skeleton.h"
#include "renderer\HierarchyIds.h"
#include "vehicles\vehicle.h"
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"
#include "system/FileMgr.h"

// Parser files
#include "Peds/Action/ActionFlags.h"

class CImpulseCombo;
class CTaskMeleeActionResult;
class CWeapon;

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This is used for speeding up repetitive action suitability tests.
/////////////////////////////////////////////////////////////////////////////////
class CActionTestCache
{
public:
	CActionTestCache();
	~CActionTestCache() {}

	float						m_fRandomProbability;
	float						m_fHeightDiff;
	float						m_fGroundHeightDiff;
	float						m_fViewerGroundHeight;
	float						m_fDistanceDiff;
	float						m_fAngularDiff;
	EstimatedPose				m_eEstimatePose;
	bool						m_bHaveHeightDiff : 1;
	bool						m_bHaveViewerGroundHeight : 1;
	bool						m_bHaveGroundHeightDiff : 1;
	bool						m_bHaveDistance : 1;
	bool						m_bHaveAngleDiff : 1;
	bool						m_bHavePedLosTest : 1;
	bool						m_bHavePedLosTestSimple : 1;
	bool						m_bPedLosTestClear : 1;
	bool						m_bPedLosTestClearSimple : 1;
	bool						m_bHaveFOVTest : 1;
	bool						m_bFOVClear : 1;
	bool						m_bHaveSurfaceCollisionResults : 1;
	bool						m_bFoundBreakableSurface : 1;
	bool						m_bFoundVerticalSurface : 1;
	bool						m_bFoundHorizontalSurface : 1;
	bool						m_bHaveFriendlyTargetTestResults : 1;
	bool						m_bFriendlyIsInDesiredDirection : 1;
	bool						m_bHaveAiTargetTestResults : 1;
	bool						m_bAiIsInDesiredDirection : 1;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This is used for testing if an ped is in an area relative to
// another entity.
/////////////////////////////////////////////////////////////////////////////////
class CRelativeRange
{
public:
	CRelativeRange();
	~CRelativeRange() {}

	// Test whether or not the viewed position is in the ranges specified.
	bool		IsViewerLocalPositionInRange		(const CPed* pViewerPed,
													 const float fRootOffsetZ,
													 const CEntity*	pViewedEntity,
													 CActionTestCache* pReusableResults,
													 const float fPredictedTargetTime,
													 const bool bDisplayDebug) const;

	void		PostLoad							(void);
	void		Reset								(void);
	void		Clear								(void) {}

	bool		GetCheckHeightRange					(void) const	{ return m_RelativeRangeAttrs.IsSet( CActionFlags::RRA_CHECK_HEIGHT_RANGE ); }
	bool		GetCheckGroundHeightRange			(void) const	{ return m_RelativeRangeAttrs.IsSet( CActionFlags::RRA_CHECK_GROUND_RANGE ); }
	bool		GetCheckDistanceRange				(void) const	{ return m_RelativeRangeAttrs.IsSet( CActionFlags::RRA_CHECK_DISTANCE_RANGE ); }
	bool		GetCheckPlanarAngularRange			(void) const	{ return m_RelativeRangeAttrs.IsSet( CActionFlags::RRA_CHECK_ANGULAR_RANGE ); }
	bool		ShouldAllowPedGetupGroundOverride	(void) const	{ return m_RelativeRangeAttrs.IsSet( CActionFlags::RRA_ALLOW_PED_GETUP_GROUND_OVERRIDE ); }
	bool		ShouldAllowPedDeadGroundOverride	(void) const	{ return m_RelativeRangeAttrs.IsSet( CActionFlags::RRA_ALLOW_PED_DEAD_GROUND_OVERRIDE ); }
	
	Vector2		GetGroundHeightRange				(void) const	{ return m_GroundHeightRange; }
	Vector2		GetDistanceRange					(void) const	{ return m_DistanceRange; }
	Vector2		GetPlanarAngularRange				(void) const	{ return m_PlanarAngularRange; }
	float		GetHeightDifference					(void) const	{ return m_HeightDifference; }
	
protected:
	bool		IsGroundHeightInRange				(const float viewerToViewedGroundHeightDiff) const;
	bool		IsDistanceInRange					(const float viewerToViewedDistanceXY) const;
	bool		IsAngleInRange						(const float viewerToViewedAngleDiffRads) const;
	bool		IsHeightInRange						(const float viewerToViewedHeightDiff) const;

private:
	Vector2		m_GroundHeightRange;
	Vector2		m_DistanceRange;
	Vector2		m_PlanarAngularRange;
	float		m_HeightDifference;
	CActionFlags::RelativeRangeAttrsBitSetData m_RelativeRangeAttrs;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This is used to make an ped move through space to a desired
// position.
/////////////////////////////////////////////////////////////////////////////////
class CHoming
{	
public:
	CHoming() {}
	~CHoming() {}

	void			Apply						(CPed* pHomingPed,
												 const CEntity* pTargetEntity,
												 Vec3V_InOut rvHomingPosition, 
												 Vec3V_InOut rvHomingDirection, 
												 Vec3V_In rvMoverDisplacement,
												 int& rnBoneCacheIdx,
												 const crClip* pClip,
												 const float fCurrentPhase,
												 bool bPerformDistanceHoming, 
												 bool bDidPerformDistanceHoming,
												 bool bPerformHeadingHoming,
												 bool bHasStruckSomething,
												 bool bUpdateDesiredHeading,
												 const float fTimeStep,
												 const int nTimeSlice,
												 Vec3V_In vCachedPedPosition);

	void			CalculateHomingWorldOffset	(CPed* pHomingPed,
												 Vec3V_In vHomingPosition,
												 Vec3V_In vHomingDirection,
												 Vec3V_In vMoverDisplacment,
												 Vec3V_InOut rvDesiredWorldPosition, 
												 float& rfDesiredHeading,
												 const bool bIgnoreXOffset = false, 
												 const bool bIgnoreAnimDisplacement = false);

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }
	
	Vector2			GetTargetOffset				(void) const	{ return m_TargetOffset; }	
	float			GetActivationDistance		(void) const	{ return m_ActivationDistance; }
	float			GetMaxSpeed					(void) const	{ return m_MaxSpeed; }
	float			GetTargetHeading			(void) const	{ return m_TargetHeading; }
	
	CActionFlags::TargetType			GetTargetType	(void) const	{ return m_TargetType; }
	CActionFlags::HomingAttrsBitSetData	GetHomingAttrs	(void) const	{ return m_HomingAttrs; }

	bool			GetCheckTargetDistance		(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_CHECK_TARGET_DISTANCE ); }
	bool			GetCheckTargetHeading		(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_CHECK_TARGET_HEADING ); }
	bool			ShouldUseDisplacementMagnitude(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_USE_DISPLACEMENT_MAGNITUDE ); }
	bool			ShouldUseCachedPosition		(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_USE_CACHED_POSITION ); }
	bool			ShouldMaintainIdealDistance	(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_MAINTAIN_IDEAL_DISTANCE ); }
	bool			ShouldUseActivationDistance	(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_USE_ACTIVATION_DISTANCE ); }
	bool			ShouldUseLowRootZOffset		(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_USE_LOW_ROOT_Z_OFFSET ); }
	bool			ShouldIgnoreSpeedMatchingCheck(void) const	{ return m_HomingAttrs.IsSet( CActionFlags::HA_IGNORE_SPEED_MATCHING_CHECK ); }

	bool			IsSurfaceProbe				(void) const	{ return m_TargetType == CActionFlags::TT_SURFACE_PROBE; }
	
#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV 

protected:
	void			ApplyWorldPosAndHeadingHoming	(CPed*				pPed,
													 Vec3V_In			vWorldPosDesired,
													 const float		fWorldHeadingDesired,
													 float				fCriticalFrameTimeRemaining, 
													 float				fHeadingUpdateTimeRemaining,
													 Vec3V_In			vAnimMoverDisplacement,
													 const float		fMoverRotationDelta,
													 const bool			bPerformDistanceHoming,
													 const bool			bDidPerformDistanceHoming,
													 const bool			bPerformHeadingHoming, 
													 const float		fTimeStep );
private:
	Vector2						m_TargetOffset;
	float						m_ActivationDistance;
	float						m_MaxSpeed;
	float						m_TargetHeading;
	atHashString				m_Name;
	CActionFlags::TargetType	m_TargetType;
	CActionFlags::HomingAttrsBitSetData	m_HomingAttrs;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Struct that holds the rumble information
/////////////////////////////////////////////////////////////////////////////////
class CActionRumble
{
public:
	CActionRumble();
	~CActionRumble() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }


#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

	u32				GetDuration					(void) const	{ return m_Duration; }
	float			GetIntensity				(void) const	{ return m_Intensity; }

private:
	atHashString				m_Name;
	u32							m_Duration;
	float						m_Intensity;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This handles the action suitability test on the relative
// attributes of the two entities (interrelation).
/////////////////////////////////////////////////////////////////////////////////
class CInterrelationTest
{
public:
	CInterrelationTest();
	~CInterrelationTest() {}

	bool			DoTest						(const CPed* pViewerPed,
												 const CEntity* pViewedEntity,
												 CActionTestCache* pReusableResults,
												 const bool bDisplayDebug) const;

	void			PostLoad					(void);
	void			Reset						(void);
	void			Clear						(void);
	
	float			GetPredictedTargetTime		(void) const	{ return m_PredictedTargetTime; }
	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }
	const CRelativeRange& GetRelativeRange		(void) const	{ return m_RelativeRange; }
	float			GetRootOffsetZ				(void) const	{ return m_RootOffsetZ; }

#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

private:
	float						m_PredictedTargetTime;
	CRelativeRange				m_RelativeRange;
	atHashString				m_Name;
	float						m_RootOffsetZ;
	
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	To package up the damage and reaction for a given action result.
// This should also help make the action results easier to read in Excel.
/////////////////////////////////////////////////////////////////////////////////
class CDamageAndReaction
{
public:
	CDamageAndReaction();
	~CDamageAndReaction() {}

	void			PostLoad					(void);
	void			Reset						(void);
	void			Clear						(void) {}
	
	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }

	atHashString	GetActionRumbleNameHashString(void) const	{ return m_ActionRumble; }
#if !__FINAL
	const char*		GetActionRumbleName			(void) const	{ return m_ActionRumble.GetCStr(); }
#endif	//	!__FINAL
	u32				GetActionRumbleID			(void) const	{ return m_ActionRumble.GetHash(); }
	CActionRumble*	GetActionRumble				(void) const	{ return m_pRumble; }
	void			SetActionRumble				(CActionRumble* pActionRumble)	{ m_pRumble = pActionRumble; }

	float			GetSelfDamageAmount			(void) const	{ return m_SelfDamageAmount; }
	float			GetOnHitDamageAmountMin		(void) const	{ return m_OnHitDamageAmountMin; }
	float			GetOnHitDamageAmountMax		(void) const	{ return m_OnHitDamageAmountMax; }

	eAnimBoneTag	GetForcedSelfDamageBoneTag	(void) const	{ return m_ForcedSelfDamageBoneTag; }

	bool			ShouldForceImmediateReaction		(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_FORCE_IMMEDIATE_REACTION ); }
	bool			ShouldDropWeapon					(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_DROP_WEAPON ); }
	bool			ShouldDisarmOpponentWeapon			(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_DISARM_OPPONENT_WEAPON); }
	bool			ShouldKnockOffProps					(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_KNOCK_OFF_PROPS ); }
	bool			ShouldStopDistanceHoming			(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_STOP_DISTANCE_HOMING ); }
	bool			ShouldStopTargetDistanceHoming		(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_STOP_TARGET_DISTANCE_HOMING ); }
	bool			ShouldAddSmallSpecialAbilityCharge	(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_ADD_SMALL_ABILITY_CHARGE ); }
	bool			ShouldDisableInjuredGroundBehvaior	(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_DISABLE_INJURED_GROUND_BEHAVIOR ); }
	bool			ShouldIgnoreArmor					(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_IGNORE_ARMOR ); }
	bool			ShouldIgnoreStatModifiers			(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_IGNORE_STAT_MODIFIERS ); }
	bool			ShouldUseDefaultUnarmedWeapon		(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_USE_DEFAULT_UNARMED_WEAPON ); }
	bool			ShouldCauseFatalHeadShot			(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_FATAL_HEAD_SHOT ); }
	bool			ShouldCauseFatalShot				(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_FATAL_SHOT ); }
	bool			ShouldMoveLowerArmAndHandHitsToClavicle	(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_MOVE_LOWER_ARM_AND_HAND_HITS_TO_CLAVICLE ); }
	bool			ShouldMoveUpperArmAndClavicleHitsToHead	(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_MOVE_UPPER_ARM_AND_CLAVICLE_HITS_TO_HEAD ); }
	bool			ShouldInvokeFacialPainAnimation		(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_INVOKE_FACIAL_PAIN_ANIMATION ); }
	bool			ShouldPreserveStealthMode			(void) const	{ return m_DamageReactionAttrs.IsSet( CActionFlags::DRA_PRESERVE_STEALTH_MODE ); }
	bool			ShouldDamagePeds					(void) const	{ return !m_DamageReactionAttrs.IsSet( CActionFlags::DRA_DONT_DAMAGE_PEDS ); }
	
#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

private:
	CActionRumble*													m_pRumble;

	atHashString													m_Name;
	atHashString													m_ActionRumble;
	float															m_OnHitDamageAmountMin;
	float															m_OnHitDamageAmountMax;
	float															m_SelfDamageAmount;
	eAnimBoneTag													m_ForcedSelfDamageBoneTag;
	CActionFlags::DamageReactionAttrsBitSetData						m_DamageReactionAttrs;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Individual strike bone info used in a set defined in CStrikeBoneSet.
/////////////////////////////////////////////////////////////////////////////////
class CStrikeBone
{
public:
	CStrikeBone();
	~CStrikeBone() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	float			GetRadius					(void) const	{ return m_Radius; }
	eAnimBoneTag	GetStrikeBoneTag			(void) const	{ return m_StrikeBoneTag; }

#if __DEV
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif

private:
	float				m_Radius;
	eAnimBoneTag		m_StrikeBoneTag;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	To package up the strike bone info for a given action result.
/////////////////////////////////////////////////////////////////////////////////
class CStrikeBoneSet
{
public:
	CStrikeBoneSet();
	~CStrikeBoneSet() {}

	void			PostLoad					(void);
	void			Reset						(void);
	void			Clear						(void);

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }

	// Strike bone queries.
	u32				GetNumStrikeBones			(void) const	{ return m_aStrikeBones.GetCount(); }
	eAnimBoneTag	GetStrikeBoneTag			(const u32 nStrikeBoneNum) const;
	float			GetStrikeBoneRadius			(const u32 nStrikeBoneNum) const;
	float			GetStrikeBoneStart			(const u32 nStrikeBoneNum) const;
	float			GetStrikeBoneEnd			(const u32 nStrikeBoneNum) const;

#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

private:
	atHashString			m_Name;
	atArray<CStrikeBone*>	m_aStrikeBones;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	To package up the vfx data that will be used on the action animations
/////////////////////////////////////////////////////////////////////////////////
class CActionVfx
{
public:
	CActionVfx();
	~CActionVfx() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }

	atHashString	GetVfxNameHashString		(void) const	{ return m_VfxName; }
#if !__FINAL
	const char*		GetVfxName					(void) const	{ return m_VfxName.GetCStr(); }
#endif	//	!__FINAL
	u32				GetVfxID					(void) const	{ return m_VfxName.GetHash(); }

	atHashString	GetDamagePackHashString		(void) const	{ return m_dmgPackName; }
#if !__FINAL
	const char*		GetDamagePackName			(void) const	{ return m_dmgPackName.GetCStr(); }
#endif	//	!__FINAL
	u32				GetDamagePackID				(void) const	{ return m_dmgPackName.GetHash(); }

	Vector3			GetOffset					(void) const	{ return m_Offset; }
	Vector3			GetRotation					(void) const	{ return m_Rotation; }
	eAnimBoneTag	GetBoneTag					(void) const	{ return m_BoneTag; }
	float			GetScale					(void) const	{ return m_Scale; }

#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

private:
	atHashString			m_Name;
	atHashString			m_VfxName;
	atHashString			m_dmgPackName; 
	Vector3					m_Offset;
	Vector3					m_Rotation;
	eAnimBoneTag			m_BoneTag;
	float					m_Scale;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	To package up the facial animation info for a given action.
/////////////////////////////////////////////////////////////////////////////////
class CActionFacialAnimSet
{
public:
	CActionFacialAnimSet();
	~CActionFacialAnimSet() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }

	// Strike bone queries.
	u32				GetNumFacialAnims			(void) const	{ return m_aFacialAnimClipIds.GetCount(); }
	fwMvClipId		GetFacialAnimClipId			(const u32 nFacialAnimIdx) const { Assert( nFacialAnimIdx < GetNumFacialAnims() ); return fwMvClipId( m_aFacialAnimClipIds[ nFacialAnimIdx ] ); }

#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

private:
	atHashString			m_Name;
	atArray<atHashString>	m_aFacialAnimClipIds;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	To package up action branches so we can distinguish between player 
//			and AI branches
/////////////////////////////////////////////////////////////////////////////////
class CActionBranch
{
public:
	CActionBranch();
	~CActionBranch() {}

	void			Reset						(void);
	void			Clear						(void) {}

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }

	CActionFlags::PlayerAiFilterTypeBitSetData GetFilterType (void) const { return m_Filter; }

#if __DEV
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif

private:
	atHashString								m_Name;
	CActionFlags::PlayerAiFilterTypeBitSetData	m_Filter;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Collection of CActionBranch items
/////////////////////////////////////////////////////////////////////////////////
class CActionBranchSet
{
public:
	CActionBranchSet();
	~CActionBranchSet() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void);

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }


#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

	// CActionBranch queries.
	u32				GetNumBranches				(void) const 	{ return m_aActionBranches.GetCount(); }
#if !__FINAL
	const char*		GetBranchActionName			(const u32 nBranchNum) const;
#endif	//	!__FINAL
	u32				GetBranchActionID			(const u32 nBranchNum) const;
	CActionFlags::PlayerAiFilterTypeBitSetData GetBranchFilterType( const u32 nBranchNum ) const;

private:
	atHashString				m_Name;
	atArray<CActionBranch*>		m_aActionBranches;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Animation information along with other useful attributes
/////////////////////////////////////////////////////////////////////////////////
class CActionResult : public fwRefAwareBase
{
public:
	CActionResult();
	~CActionResult() {}

	void			PostLoad					(void);
	void			Reset						(void);
	void			Clear						(void) {}

	atHashString	GetNameHashString			(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }

	fwMvClipSetId	GetClipSet					(void) const	{ return fwMvClipSetId(m_ClipSet.GetHash()); }
#if !__FINAL
	const char*		GetClipSetName				(void) const	{ return m_ClipSet.GetCStr(); }
#endif	//	!__FINAL
	u32				GetClipSetID				(void) const	{ return m_ClipSet.GetHash(); }

	fwMvClipSetId	GetFirstPersonClipSet		(void) const	{ return fwMvClipSetId(m_FirstPersonClipSet.GetHash()); }
#if !__FINAL
	const char*		GetFirstPersonClipSetName	(void) const	{ return m_FirstPersonClipSet.GetCStr(); }
#endif	//	!__FINAL
	u32				GetFirstPersonClipSetID		(void) const	{ return m_FirstPersonClipSet.GetHash(); }

	fwMvClipId		GetClip						(void) const	{ return fwMvClipId(m_Anim.GetHash()); }
	atHashString	GetAnimNameHashString		(void) const	{ return m_Anim; }
#if !__FINAL
	const char*		GetAnimName					(void) const	{ return m_Anim.GetCStr(); }
#endif	//	!__FINAL
	u32				GetAnimID					(void) const	{ return m_Anim.GetHash(); }
		
	float			GetAnimBlendInRate			(void) const	{ return m_AnimBlendInRate; }
	float			GetAnimBlendOutRate			(void) const	{ return m_AnimBlendOutRate; }

	const atArray<atHashString>&	GetLeftCameraAnims	(void) const	{ return m_CameraAnimLeft; }
	const atArray<atHashString>&	GetRightCameraAnims	(void) const 	{ return m_CameraAnimRight; }

	atHashString	GetActionBranchSetNameHashString	(void) const	{ return m_ActionBranchSet; }
#if !__FINAL
	const char*		GetActionBranchSetName		(void) const	{ return m_ActionBranchSet.GetCStr(); }
#endif	//	!__FINAL
	u32				GetActionBranchSetID		(void) const	{ return m_ActionBranchSet.GetHash(); }
	CActionBranchSet* GetActionBranchSet		(void) const	{ return m_pActionBranchSet; }
	void			SetActionBranchSet			(CActionBranchSet* pActionBranchSet) { m_pActionBranchSet = pActionBranchSet; }

	atHashString	GetHomingNameHashString		(void) const	{ return m_Homing; }
#if !__FINAL
	const char*		GetHomingName				(void) const	{ return m_Homing.GetCStr(); }
#endif	//	!__FINAL
	u32				GetHomingID					(void) const	{ return m_Homing.GetHash(); }
	CHoming*		GetHoming					(void) const	{ return m_pHoming; }
	void			SetHoming					(CHoming* pHoming)	{ m_pHoming = pHoming; }

	atHashString	GetDamageAndReactionNameString	(void) const	{ return m_DamageAndReaction; }
#if !__FINAL
	const char*		GetDamageAndReactionName	(void) const	{ return m_DamageAndReaction.GetCStr(); }
#endif	//	!__FINAL
	u32				GetDamageAndReactionID		(void) const	{ return m_DamageAndReaction.GetHash(); }
	const CDamageAndReaction* GetDamageAndReaction	(void) const 	{ return m_pDamageAndReaction; }
	void			SetDamageAndReaction		(CDamageAndReaction* pDamageAndReaction) { m_pDamageAndReaction= pDamageAndReaction; }
	
	atHashString	GetSoundHashString			(void) const	{ return m_Sound; }
#if !__FINAL
	const char*		GetSoundName				(void) const	{ return m_Sound.GetCStr(); }
#endif	//	!__FINAL
	u32				GetSoundID					(void) const	{ return m_Sound.GetHash(); }	

	atHashString	GetStrikeBoneSetNameString	(void) const	{ return m_StrikeBoneSet; }
#if !__FINAL
	const char*		GetStrikeBoneSetName		(void) const	{ return m_StrikeBoneSet.GetCStr(); }
#endif	//	!__FINAL
	u32				GetStrikeBoneSetID			(void) const	{ return m_StrikeBoneSet.GetHash(); }
	const CStrikeBoneSet* GetStrikeBoneSet		(void) const	{ return m_pStrikeBoneSet; }
	void			SetStrikeBoneSet			(CStrikeBoneSet* pStrikeBoneSet) { m_pStrikeBoneSet = pStrikeBoneSet; }
	int				GetPriority					(void) const	{ return m_Priority; }

	atHashString	GetNmReactionSet			(void) const	{ return m_NmReactionSet; }

	CActionFlags::CameraTarget	GetCameraTarget	(void) const	{ return m_CameraTarget; }					// for first person camera
	bool			IsApplyRelativeTargetPitch	(void) const	{ return m_ApplyRelativeTargetPitch; }		// for first person camera

	CActionFlags::ResultAttrsBitSetData	GetResultAttrs(void) const	{ return m_ResultAttrs; }

	bool			GetIsAnIntro				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_INTRO ); }
	bool			GetIsATaunt					(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_TAUNT ); }
	bool			GetPlayerIsUpperbodyOnly	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_PLAYER_IS_UPPERBODY_ONLY ); }
	bool			GetAIIsUpperbodyOnly		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_AI_IS_UPPERBODY_ONLY ); }
	bool			GetIsABlock					(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_BLOCK ); }
	bool			GetIsAStealthKill			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_STEALTH_KILL ); }
	bool			GetIsAKnockout				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_KNOCKOUT ); }
	bool			GetIsATakedown				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_TAKEDOWN ); }
	bool			GetIsAHitReaction			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_HIT_REACTION ); }
	bool			GetIsADazedHitReaction		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_DAZED_HIT_REACTION ); }
	bool			GetIsAStandardAttack		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_STANDARD_ATTACK ); }
	bool			GetIsACounterAttack			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_COUNTER_ATTACK ); }
	bool			GetIsARecoil				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_RECOIL); }
	bool			GetShouldResetCombo			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_RESET_COMBO ); }
	bool			AllowAimInterrupt			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_AIM_INTERRUPT ); }
	bool			GetShouldForceDamage		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_FORCE_DAMAGE ); }
	bool			AllowAdditiveAnims			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_ADDITIVE_ANIMS ); }
	bool			GetIsLethal					(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_LETHAL ); }
	bool			ShouldUseLegIk				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_USE_LEG_IK ); }
	bool			GetIsValidForRecoil			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_VALID_FOR_RECOIL ); }
	bool			GetIsNonMeleeHitReaction	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_NON_MELEE_HIT_REACTION ); }
	bool			GetShouldAllowStrafing		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_STRAFING ); }
	bool			GetShouldResetMinTimeInMelee(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_RESET_MIN_TIME_IN_MELEE ); }
	bool			AllowNoTargetInterrupt		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_NO_TARGET_INTERRUPT ); }
	bool			ShouldQuitTaskAfterAction	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_QUIT_TASK_AFTER_ACTION ); }
	bool			ActivateRagdollOnCollision	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ACTIVATE_RAGDOLL_ON_COLLISION ); }
	bool			AllowInitialActionCombo		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_INITIAL_ACTION_COMBO ); }
	bool			AllowMeleeTargetEvaluation	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_MELEE_TARGET_EVALUATION ); }
	bool			ShouldProcessPostHitResultsOnContactFrame (void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_PROCESS_POST_HIT_RESULTS_ON_CONTACT_FRAME ); }
	bool			GetRequiresAmmo				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_REQUIRES_AMMO ); }
	bool			ShouldGiveThreatResponseAfterAction	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_GIVE_THREAT_RESPONSE_AFTER_ACTION ); }
	bool			AllowDazedReaction			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_DAZED_REACTION ); }
	bool			ShouldIgnoreHitDotThreshold	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_IGNORE_HIT_DOT_THRESHOLD ); }
	bool			ShouldInterruptWhenOutOfWater (void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_INTERRUPT_WHEN_OUT_OF_WATER ); }
	bool			ShouldAllowScriptTaskAbort	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_SCRIPT_TASK_ABORT ); }	
	bool			TagSyncBlendOut				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_TAG_SYNC_BLEND_OUT ); }	
	bool			UpdateMoveBlendRatio		(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_UPDATE_MOVEBLENDRATIO ); }	
	bool			EndsInIdlePose				(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ENDS_IN_IDLE_POSE ); }	
	bool			ShouldDisablePedToPedRagdoll(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_DISABLE_PED_TO_PED_RAGDOLL ); }
	bool			ShouldAllowNoTargetBranches	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_NO_TARGET_BRANCHES ); }
	bool			ShouldUseKinematicPhysics	(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_USE_KINEMATIC_PHYSICS ); }
	bool			EndsInAimingPose			(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ENDS_IN_AIMING_POSE ); }	
	bool			ShouldUseCachedDesiredHeading (void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_USE_CACHED_DESIRED_HEADING ); }	
	bool			ShouldAddOrientToSpineOffset(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ADD_ORIENT_TO_SPINE_Z_OFFSET ); }
	bool			GetShouldAllowPlayerToEarlyOut(void) const	{ return m_ResultAttrs.IsSet( CActionFlags::RA_ALLOW_PLAYER_EARLY_OUT ); }
	bool			GetIsShoulderBarge(void) const				{ return m_ResultAttrs.IsSet( CActionFlags::RA_IS_SHOULDER_BARGE ); }

	bool			IsOffensiveMove() const { return GetIsAStandardAttack() || GetIsATakedown() || GetIsAKnockout() || GetIsAStealthKill() || GetIsACounterAttack(); }

#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

private:
	CHoming*					m_pHoming;
	CActionBranchSet*			m_pActionBranchSet;
	CStrikeBoneSet*				m_pStrikeBoneSet;
	CDamageAndReaction*			m_pDamageAndReaction;
	
	atHashString				m_Name;
	atHashString				m_ClipSet;
	atHashString				m_FirstPersonClipSet;
	atHashString				m_Anim;
	atArray<atHashString>		m_CameraAnimLeft;
	atArray<atHashString>		m_CameraAnimRight;
	atHashString				m_Homing;
	atHashString				m_DamageAndReaction;
	atHashString				m_StrikeBoneSet;
	atHashString				m_Sound;
	atHashString				m_ActionBranchSet;
	atHashString				m_NmReactionSet;
	CActionFlags::CameraTarget	m_CameraTarget;
	bool						m_ApplyRelativeTargetPitch;
	float						m_AnimBlendInRate;
	float						m_AnimBlendOutRate;
	int							m_Priority;
	CActionFlags::ResultAttrsBitSetData	m_ResultAttrs;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This handles the action suitability test for handling
// environmental and special case tests...
/////////////////////////////////////////////////////////////////////////////////
class CActionCondSpecial
{
public:
	CActionCondSpecial();
	~CActionCondSpecial() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	bool			DoTest						(const CPed* pViewerPed,
												 const CEntity* pTargetEntity,
												 const CActionResult* pActionResult,
												 CActionTestCache* pReusableResults,
												 const bool bDisplayDebug) const;

	atHashString	GetSpecialTestHashString	(void) const	{ return m_SpecialTest; }
#if !__FINAL
	const char*		GetSpecialTestName			(void) const	{ return m_SpecialTest.GetCStr(); }
#endif	//	!__FINAL
	u32				GetSpecialTestID			(void) const	{ return m_SpecialTest.GetHash(); }

	bool			GetInvertSense				(void) const	{ return m_InvertSense;}

	CActionFlags::PlayerAiFilterTypeBitSetData GetFilter (void) const	{ return m_Filter; }

#if __DEV
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif

private:
	atHashString								m_SpecialTest;
	CActionFlags::PlayerAiFilterTypeBitSetData	m_Filter;
	bool										m_InvertSense;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This handles the action suitability test for handling weapon melee
/////////////////////////////////////////////////////////////////////////////////
class CWeaponActionResult
{
public:
	CWeaponActionResult();
	~CWeaponActionResult() {}

	void			PostLoad			(void);
	void			Reset				(void);
	void			Clear				(void) {}

	CActionFlags::RequiredWeaponTypeBitSetData GetWeaponType	(void) const	{ return m_WeaponType; }

	atHashString			GetActionResultHashString	(void) const	{ return m_ActionResult; }
#if !__FINAL
	const char*				GetActionResultName			(void) const	{ return m_ActionResult.GetCStr(); }
#endif	//	!__FINAL
	u32						GetActionResultID			(void) const	{ return m_ActionResult.GetHash(); }
	const CActionResult*	GetActionResult				(void) const	{ return m_pActionResult; }
	void					SetActionResult				(const CActionResult* pActionResult) { m_pActionResult = pActionResult; }

#if __DEV
	void			PrintMemoryUsage	(size_t& rTotalSize);
#endif

private:
	RegdConstActionResult						m_pActionResult;

	// Parcodegen  variables
	CActionFlags::RequiredWeaponTypeBitSetData	m_WeaponType;
	atHashString								m_ActionResult;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This is a list version of CWeaponActionResult. Allows me to define
//			multiple actions without having to make duplicate CActionDefinition 
//			data.
/////////////////////////////////////////////////////////////////////////////////
class CWeaponActionResultList
{
public:
	CWeaponActionResultList();
	~CWeaponActionResultList() {}

	void					PostLoad					(void);
	void					Reset						(void);
	void					Clear						(void) {}

	CActionFlags::RequiredWeaponTypeBitSetData GetWeaponType	(void) const	{ return m_WeaponType; }
	int						GetNumActionResults			(void) const { return m_aActionResults.GetCount(); }

	atHashString			GetActionResultHashString	(int nActionResultIdx) const;
#if !__FINAL
	const char*				GetActionResultName			(int nActionResultIdx) const;
#endif	//	!__FINAL
	u32						GetActionResultID			(int nActionResultIdx) const;
	const CActionResult*	GetActionResult				(int nActionResultIdx) const;
	void					SetActionResult				(int nActionResultIdx, const CActionResult* pActionResult);

#if __DEV
	void					PrintMemoryUsage			(size_t& rTotalSize);
#endif

private:
	atArray<RegdConstActionResult>				m_apActionResults;

	// Parcodegen  variables
	CActionFlags::RequiredWeaponTypeBitSetData	m_WeaponType;
	atArray<atHashString>						m_aActionResults;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This is used for testing if an ped matches all the requirements in 
// regards to anothers current actions.
/////////////////////////////////////////////////////////////////////////////////
class CActionCondPedOther
{
public:
	CActionCondPedOther();
	~CActionCondPedOther() {}

	bool				DoTest								(const CPed* pViewerPed,
															 const CPed* pViewedPed,
															 const CActionResult* pViewerResult,
															 const u32 nForcingResultId,
															 CActionTestCache* pReusableResults,
															 const bool bDisplayDebug) const;

	bool				ShouldAllowBrawlingStyle			(u32 nBrawlingStyleHash) const;

	void				PostLoad							(void);
	void				Reset								(void);
	void				Clear								(void);

	u32								GetNumSpecialConditions		(void) const { return m_aCondSpecials.GetCount(); }
	const CActionCondSpecial*		GetSpecialCondition			(const u32 nIdx) const;

	u32								GetNumWeaponActionResultLists(void) const { return m_aWeaponActionResultList.GetCount(); }
	const CWeaponActionResultList*	GetWeaponActionResultList	(const CPed* pViewedPed) const;
	const CWeaponActionResultList*	GetWeaponActionResultList	(const int nIdx) const;

	u32								GetNumBrawlingStyles		(void) const { return m_aBrawlingStyles.GetCount(); }
#if !__FINAL
	const char*						GetBrawlingStyleName		(const u32 nIdx) const;
#endif	//	!__FINAL
	u32								GetBrawlingStyleID			(const u32 nIdx) const;

#if !__FINAL
	const char*						GetInterrelationTestName	(void) const		{ return m_InterrelationTest.GetCStr(); }
#endif	//	!__FINAL
	u32								GetInterrelationTestID		(void) const		{ return m_InterrelationTest.GetHash(); }
	const CInterrelationTest*		GetInterrelationTest		(void) const		{ return m_pInterrelationTest; }
	void							SetInterrelationTest		(CInterrelationTest* pInterrelationTest) { m_pInterrelationTest = pInterrelationTest; }

	CActionFlags::MovementSpeedBitSetData		GetMovementSpeed		(void) const	{ return m_MovementSpeed; }
	CActionFlags::PlayerAiFilterTypeBitSetData	GetSelfFilter	(void) const			{ return m_SelfFilter; }
	
	bool										GetCheckAnimTime(void) const	{ return m_CheckAnimTime; }	

#if __DEV
	void				PrintMemoryUsage					(size_t& rTotalSize);
#endif

private:
	CInterrelationTest*							m_pInterrelationTest;
	atArray<CActionCondSpecial*>				m_aCondSpecials;
	atArray<CWeaponActionResultList*>			m_aWeaponActionResultList;
	atArray<atHashString>						m_aBrawlingStyles;
	atHashString								m_InterrelationTest;
	CActionFlags::PlayerAiFilterTypeBitSetData	m_SelfFilter; 
	CActionFlags::MovementSpeedBitSetData		m_MovementSpeed;
	bool										m_CheckAnimTime;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This handles the action suitability data about how to test the
// controller.  i.e. did they press X and then hold it down within the last
// 10 frames?
/////////////////////////////////////////////////////////////////////////////////
class CSimpleImpulseTest
{
public:
	enum Impulse
	{
		ImpulseNone,
		ImpulseAttackLight,
		ImpulseAttackHeavy,	
		ImpulseAttackAlternate,	
		ImpulseFire,	
		ImpulseBlock,
		ImpulseSpecialAbility,
		ImpulseSpecialAbilitySecondary,
		ImpulseAnalogLeftRight,
		ImpulseAnalogUpDown,
		ImpulseAnalogFromCenter
	};

	enum ImpulseState
	{
		ImpulseStateNone,
		ImpulseStateDown,
		ImpulseStateHeldDown,
		ImpulseStateHeldUp,
		ImpulseStatePressedAndHeld,
		ImpulseStateReleasedAndHeld,
		ImpulseStatePressed,
		ImpulseStatePressedNoHistory,
		ImpulseStateReleased,
		ImpulseStateCombo1,
		ImpulseStateCombo2,
		ImpulseStateCombo3,
		ImpulseStateComboAny
	};

	CSimpleImpulseTest();
	~CSimpleImpulseTest() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	bool			DoTest						(const CControl* pController,
												 const CImpulseCombo* pImpulseCombo,
												 CActionTestCache* pReusableResults,
												 const bool bDisplayDebug) const;

	static bool		GetIOValuesForImpulse		(const CControl* pController, 
												 Impulse impulse, 
												 ioValue const* &ioVal0, 
												 ioValue const* &ioVal1);

	Impulse			GetImpulse					(void) const	{ return m_Impulse; }
	ImpulseState	GetImpulseState				(void) const	{ return m_State; }
	Vector2			GetAnalogRange				(void) const	{ return m_AnalogRange; }
	float			GetDuration					(void) const	{ return m_Duration; }

#if __DEV
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif

private:
	Impulse				m_Impulse;
	ImpulseState		m_State;
	Vector2				m_AnalogRange;
	float				m_Duration;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Storage class for the actual impulse (button press)
/////////////////////////////////////////////////////////////////////////////////
class CImpulse
{
public:
	CImpulse();
	CImpulse(CSimpleImpulseTest::Impulse impulse, bool bExecuted = false);
	~CImpulse() {}

	void			PostLoad					(void) {}
	void			Reset						(void);
	void			Clear						(void) {}

	CSimpleImpulseTest::Impulse GetImpulse		(void) const		{ return m_Impulse; }
	u32				GetRecordedTime				(void) const		{ return m_RecordedTime; };
	bool			HasExecuted					(void) const		{ return m_bExecuted; }
	
	void			SetImpulse					(CSimpleImpulseTest::Impulse impulse) { m_Impulse = impulse; }
	void			SetRecordedTime				(u32 newTime) { m_RecordedTime = newTime; }
	void			SetHasExecuted				(bool bExecuted)	{ m_bExecuted = bExecuted; }

#if __BANK
	static const char*		GetImpulseName(s32 impulse);
#endif // __BANK

private:
	CSimpleImpulseTest::Impulse m_Impulse;
	u32							m_RecordedTime;
	bool						m_bExecuted;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This stores the current combo input state, e.g. which impulses have
//			been generated and in which order.
/////////////////////////////////////////////////////////////////////////////////
class CImpulseCombo
{
public:
	static const s32 MAX_NUM_IMPULSES = 3;

	CImpulseCombo();
	CImpulseCombo(CSimpleImpulseTest::Impulse initialImpulse);
	~CImpulseCombo() {}

	bool operator==( const CImpulseCombo& rhs ) const;

	void			PostLoad					(void);
	void			Reset						(void);
	void			Clear						(void);

	// Resets the combo info and records the first impulse
	void			RecordFirstImpulse			(CSimpleImpulseTest::Impulse impulse);

	// Scans the controller for new impulses
	bool			ScanForImpulses				(const CControl* controller);	

	// loop through current impulses to find if one is still pending
	bool			HasPendingImpulse			(void) const;

	// Find when the most recent recorded impulse is
	u32				GetLatestRecordedImpulseTime(void) const;

	// CImpulse array queries.
	s32				GetNumImpulses				(void) const 	{ return m_aImpulses.GetCount(); }
	CSimpleImpulseTest::Impulse GetImpulseAtIndex(s32 iComboIndex) const;
	CImpulse*		GetImpulsePtrAtIndex		(s32 iComboIndex);
	const CImpulse*	GetImpulsePtrAtIndex		(s32 iComboIndex) const;

#if __DEV
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif

private:
	bool RecordImpulse(CSimpleImpulseTest::Impulse impulse, bool bMarkExecuted = false);
	atFixedArray<CImpulse, MAX_NUM_IMPULSES>	m_aImpulses;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Used in a list to determine if a stealth kill is possible
/////////////////////////////////////////////////////////////////////////////////
class CStealthKillTest
{
public:
	CStealthKillTest();
	~CStealthKillTest() {}

	void			PostLoad						(void) {}
	void			Reset							(void);
	void			Clear							(void) {}

	atHashString	GetHashString					(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName							(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID							(void) const	{ return m_Name.GetHash(); }

	atHashString	GetStealthKillActionHashString	(void) const	{ return m_StealthKillAction; }
#if !__FINAL
	const char*		GetStealthKillActionName		(void) const	{ return m_StealthKillAction.GetCStr(); }
#endif	//	!__FINAL
	u32				GetStealthKillActionID			(void) const	{ return m_StealthKillAction.GetHash(); }

	CActionFlags::ActionTypeBitSetData	GetActionType (void) const	{ return m_ActionType; }

#if __DEV
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif

private:
	atHashString							m_Name;
	atHashString							m_StealthKillAction;
	CActionFlags::ActionTypeBitSetData		m_ActionType;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	Used in a list to determine if a stealth kill is possible
/////////////////////////////////////////////////////////////////////////////////
class CImpulseTest
{
public:
	CImpulseTest();
	~CImpulseTest() {}

	void			PostLoad					(void);
	void			Reset						(void);
	void			Clear						(void);

	bool			DoTest						(const CControl* controller,
												 const CImpulseCombo* pImpulseCombo,
												 CActionTestCache* reusableResults,
												 const bool bDisplayDebug) const;

	atHashString	GetHashString				(void) const	{ return m_Name; }
#if !__FINAL
	const char*		GetName						(void) const	{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32				GetID						(void) const	{ return m_Name.GetHash(); }
	CActionFlags::ImpulseOp		GetImpulseOp	(void) const	{ return m_ImpulseOp; }

	// Impulse queries.
	u32				GetNumImpulses				(void) const	{ return m_aImpulses.GetCount(); }
	const CSimpleImpulseTest* GetImpulseByIndex	(const u32 impulseNum) const;

#if __DEV
	bool			IsValid						(void) const	{ return true; }
	void			PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

private:
	atHashString					m_Name;
	CActionFlags::ImpulseOp			m_ImpulseOp;
	atArray<CSimpleImpulseTest*>	m_aImpulses;
	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// Actions
//
// Actions are possible things to do.  When requested they are tested to see
// if invoking them is appropriate.  If so they are invoked.
// 
// Usually the action manager will get a generic request for an action from
// idle.  In this case usually multiple different actions will be able to be
// invoked.  In this case priority values and bias values are used to select
// only one of them to be invoked.
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This is a container that holds the tests needed for a a
// actionResult to be suitable, which actionResult to trigger, what else to
// do when it is triggered, and information about when to choose this action
// over other valid actions.
/////////////////////////////////////////////////////////////////////////////////
class CActionDefinition : public fwRefAwareBase
{
public:
	CActionDefinition();
	~CActionDefinition() {}
	
	void			PostLoad								(void);
	void			Reset									(void);
	void			Clear									(void);

	bool			ShouldAllowActionType					(const CActionFlags::ActionTypeBitSetData& typeToLookFor) const;
	bool			ShouldAllowBrawlingStyle				(u32 nBrawlingStyleHash) const;

	bool			IsSuitable								(const CActionFlags::ActionTypeBitSetData& typeToLookFor,
															 const CImpulseCombo* pImpulseCombo,
															 const bool bUseAvailabilityFromIdleTest,
															 const bool bDesiredAvailabilityFromIdle,
															 const CPed* pActingPed,
															 const u32 nForcingResultId,
															 const s32 nPriority,
															 const CEntity* pTargetEntity,
															 const bool bHasLockOnTargetEntity,
															 const bool	bImpulseInitiatedMovesOnly,
															 const bool	bUseImpulseTest,// For AI
															 const CControl* pController,
															 CActionTestCache* pReusableResults,
															 const bool bDisplayDebug ) const;

#if !__FINAL
	const char*					GetName						(void) const		{ return m_Name.GetCStr(); }
#endif	//	!__FINAL
	u32							GetID						(void) const		{ return m_Name.GetHash(); }

	CActionFlags::ActionTypeBitSetData	GetActionType		(void) const		{ return m_ActionType; }

#if !__FINAL
	const char*					GetImpulseTestName			(void) const		{ return m_ImpulseTest.GetCStr(); }
#endif	//	!__FINAL
	u32							GetImpulseTestID			(void) const		{ return m_ImpulseTest.GetHash(); }
	const CImpulseTest*			GetImpulseTest				(void) const		{ return m_pImpulseTest; }
	void						SetImpulseTest				(CImpulseTest* pImpulseTest) { m_pImpulseTest = pImpulseTest; }

#if !__FINAL
	const char*					GetInterrelationTestName	(void) const		{ return m_InterrelationTest.GetCStr(); }
#endif	//	!__FINAL
	u32							GetInterrelationTestID		(void) const		{ return m_InterrelationTest.GetHash(); }
	const CInterrelationTest*	GetInterrelationTest		(void) const		{ return m_pInterrelationTest; }
	void						SetInterrelationTest		(CInterrelationTest* pInterrelationTest) { m_pInterrelationTest = pInterrelationTest; }

	u32							GetNumSpecialConditions		(void) const		{ return m_aCondSpecials.GetCount(); }
	const CActionCondSpecial*	GetSpecialCondition			(const u32 nIdx) const;

	const CActionResult*		GetActionResult				(const CPed* pPed) const;
	u32							GetNumWeaponActionResults	(void) const		{ return m_aWeaponActionResults.GetCount(); }
	const CWeaponActionResult*	GetWeaponActionResult		(const u32 nIdx) const;

	u32							GetNumBrawlingStyles		(void) const { return m_aBrawlingStyles.GetCount(); }
#if !__FINAL
	const char*					GetBrawlingStyleName		(const u32 nIdx) const;
#endif	//	!__FINAL
	u32							GetBrawlingStyleID			(const u32 nIdx) const;

	
	const CActionCondPedOther*	GetCondPedOther				(void) const		{ return &m_CondPedOther; }
	u32							GetPriority					(void) const		{ return m_Priority; }
	float						GetBias						(void) const		{ return (float)m_SelectionRouletteBias; }
	u32							GetMaxImpulseDelay			(void) const		{ return m_MaxImpulseTestDelay; }
	
	CActionFlags::PlayerAiFilterTypeBitSetData	GetSelfFilter			(void) const	{ return m_SelfFilter; }
	CActionFlags::PlayerAiFilterTypeBitSetData	GetTargetFilter			(void) const	{ return m_TargetFilter; }
	CActionFlags::TargetEntityTypeBitSetData	GetTargetEntity			(void) const	{ return m_TargetEntity; }
	CActionFlags::MovementSpeedBitSetData		GetMovementSpeed		(void) const	{ return m_MovementSpeed; }
	CActionFlags::DesiredDirectionBitSetData	GetDesiredDirection		(void) const	{ return m_DesiredDirection; }
	CActionFlags::DefinitionAttrsBitSetData		GetDefinitionAttrs		(void) const	{ return m_DefinitionAttrs; }

	bool						GetIsPlayerEntryAction				(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_PLAYER_ENTRY_ACTION ); }
	bool						GetIsAiEntryAction					(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_AI_ENTRY_ACTION ); }
	bool						GetRequireTarget					(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_REQUIRE_TARGET ); }
	bool						GetRequireNoTarget					(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_REQUIRE_NO_TARGET ); }
	bool						GetRequireTargetLockOn				(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_REQUIRE_TARGET_LOCK_ON ); }
	bool						GetRequireNoTargetLockOn			(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_REQUIRE_NO_TARGET_LOCK_ON ); }
	bool						PreferRagdollResult					(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_PREFER_RAGDOLL_RESULT ); }
	bool						PreferBodyIkResult					(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_PREFER_BODY_IK_RESULT ); }
	bool						AllowInWater						(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_ALLOW_IN_WATER ); }
	bool						IsDisabledInFirstPerson				(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_DISABLE_IN_FIRST_PERSON ); }
	bool						IsDisabledIfTargetInFirstPerson		(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_DISABLE_IF_TARGET_IN_FIRST_PERSON ); }
	
	bool						GetIsEnabled				(void) const	{ return m_DefinitionAttrs.IsSet( CActionFlags::DA_IS_ENABLED ); }
	void						SetIsEnabled				(bool bEnabled)	{ bEnabled ? m_DefinitionAttrs.Set( CActionFlags::DA_IS_ENABLED ) : m_DefinitionAttrs.Clear( CActionFlags::DA_IS_ENABLED ); }
	bool						GetIsDebug					(void) const	{ return m_Debug; }

#if __DEV
	bool						IsValid						(void) const		{ return true; }
	void						PrintMemoryUsage			(size_t& rTotalSize);
#endif // __DEV

protected:
	bool						IsActionsClipSetAllowedForPed (const CPed* pActingPed) const;
	
private:
	CImpulseTest*								m_pImpulseTest;
	CInterrelationTest*							m_pInterrelationTest;

	// Parcodegen  variables
	atHashString								m_Name;
	atHashString								m_ImpulseTest;
	atHashString								m_InterrelationTest;
	
	atArray<CActionCondSpecial*>				m_aCondSpecials;
	atArray<CWeaponActionResult*>				m_aWeaponActionResults;
	atArray<atHashString>						m_aBrawlingStyles;
	CActionCondPedOther							m_CondPedOther;
	CActionFlags::ActionTypeBitSetData			m_ActionType;
	CActionFlags::PlayerAiFilterTypeBitSetData	m_SelfFilter; 
	CActionFlags::PlayerAiFilterTypeBitSetData	m_TargetFilter;
	CActionFlags::TargetEntityTypeBitSetData	m_TargetEntity;
	CActionFlags::MovementSpeedBitSetData		m_MovementSpeed;
	CActionFlags::DesiredDirectionBitSetData	m_DesiredDirection;
	CActionFlags::DefinitionAttrsBitSetData		m_DefinitionAttrs;
	int											m_Priority;
	int											m_SelectionRouletteBias;
	u32											m_MaxImpulseTestDelay;
	bool										m_Debug;
	PAR_SIMPLE_PARSABLE;
};

#define MAX_SUITABLE_DEFINITIONS    (10) 
#define ACTION_HISTORY_LENGTH		(12)

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This structure is used to help determine the most suitable of actions
/////////////////////////////////////////////////////////////////////////////////
struct sSuitableActionInfo
{
	void Reset(void)
	{
		fBiasInfluence = 1.0f;
		fBias = 1.0f;
		nSelectionIdx = 0;
		nHistoryIdx = ACTION_HISTORY_LENGTH;
		nConsecutiveCount = 1;
	}

	float	fBiasInfluence;
	float	fBias;
	u8		nSelectionIdx;
	u8		nHistoryIdx;
	u8		nConsecutiveCount;
};

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	This is the main store for all possible actions that a ped might
// take. It stores actions that can be accessed from idle, combat idle, and
// from other actions.
//
// It also handles loading of this data from a CSV file in such a way that
// it can be dynamically reloaded at run time (for easier tuning).
/////////////////////////////////////////////////////////////////////////////////
class CActionManager
{
	/////////////////////////////////////////////////////////////////////////////////
	// Static members
public:
	static dev_float		ms_fMBRWalkBoundary;
	static dev_float		ms_fLowSurfaceRootOffsetZ;
	static dev_float		ms_fHighSurfaceRootOffsetZ;

	static int				CompareActionPriorities				(const CActionDefinition* const* pA1, const CActionDefinition* const* pA2);
	static s32				CompareActionHistory				(const sSuitableActionInfo* infoA, const sSuitableActionInfo* infoB);
	static Vec3V_Out		GetClosestPointOnBox				(const phInst* pInst, Vec3V_In v3TestPos, bool& bContainsPoint);
	static bool				ArePedsFriendlyWithEachOther		(const CPed* pPedInflictor, const CPed* pPedTarget);
	static bool				PlayerAiFilterTypeDoesCatchPed		(const CActionFlags::PlayerAiFilterTypeBitSetData filterTypeBitSet, const CPed* pPed);
	static bool				TargetEntityTypeDoesCatchEntity		(const CActionFlags::TargetEntityTypeBitSetData targetEntityTypeBitSet, const CEntity* pEntity);
	static bool				TargetTypeDoesCatchEntity			(const CActionFlags::TargetType targetType, const CEntity* pEntity);
	static bool				TargetTypeGetPosDirFromEntity		(Vec3V_InOut rv3Position,
																 Vec3V_InOut rv3Direction,
																 const CActionFlags::TargetType	targetType,
																 const CEntity* pEntity,
																 const CPed* pViewerPed,
																 Vec3V_In rViewerPos,
																 float fTimeToReachEntity,
																 int& nBoneIdxCache,
																 float fMaxProbeDistance = 3.5f,
																 float fProbeRootOffsetZ = 0.0f,
																 bool* pbFoundBreakableSurface = NULL );
	static void				GetMatrixFromBoneTag				(Mat34V_InOut boneMtx, 
																 const CEntity* pEntity, 
																 eAnimBoneTag eBoneTag);
	static Vec3V_Out		CalculatePredictedTargetPosition	(const CPed* pPed, const CEntity* pTargetEntity, float fTimeToReachEntity, eAnimBoneTag eBoneTag);
	static bool				WithinMovementSpeed					(const CPed* pPed, const CActionFlags::MovementSpeedBitSetData movementSpeed);
	static bool				WithinDesiredDirection				(const CPed* pPed, const CActionFlags::DesiredDirectionBitSetData desiredDirection);

	static bool				ShouldAllowWeaponType				(const CPed* pPed, CActionFlags::RequiredWeaponTypeBitSetData weaponType);

	static void				RecordAction						(u32 uActionToRecord);

	static bool				ShouldDebugPed						(const CPed* pPed);

	static float			GetEntityGroundHeight				(const CEntity* pEntity, 
																 Vec3V_In vProbePosition, 
																 float fProbeZOffset);

	static float			CalculateDesiredHeading				(const CPed* pPed);

	static bool				IsPedTypeWithinAngularThreshold		(const CPed* pTestPed,
																 Vec3V_In vTestOffset,
																 const CEntity* pTargetEntity,
																 const float fMinDistThresholdSq,
																 const float fMaxDistThresholdSq, 
																 const float fMinDotThreshold,
																 const float fMaxDotThreshold, 
																 const float fLookAheadTime = 0.5f,
																 bool bFriendlyCheck = true,
																 bool bOnlyCheckAi = false );

	void					EnableAction						(u32 uActionToEnable, bool bEnable);
	bool					IsActionEnabled						(u32 uActionId);

#if __DEV
	static const char*		TypeGetStringFromPoseType			(const EstimatedPose ePose);
#endif // __DEV

#if __BANK
	static void				InitWidgets							(void);
	static void				CreateActionManagerWidgetsCB		(void);
	void					CreateActionManagerWidgets			(void);
	static void				ShutdownWidgets						(void);

	// Pointer to the main RAG bank
	static bkBank*			ms_pBank;
	static bkButton*		ms_pCreateButton;
#endif // __BANK

	/////////////////////////////////////////////////////////////////////////////////
	// Instance members
public:
	CActionManager();
	~CActionManager();

	void						OverrideActionDefinitions				(const char* fileName);
	void						RevertActionDefinitions					(void);

	static void					Init									(unsigned initMode);
	static void					Shutdown								(unsigned shutdownMode);
	void						Init									(void);
	void						Shutdown								(void);
	void						PostLoad								(void);
	void						LoadOrReloadActionTableData				(void);
	void						BuildMeleeClipSetData					(void);
	void						FreeMeleeAnimationDefinitions			(void);

#if __DEV
	void						VerifyMeleeAnimUsage					(void) const;
	bool						IsValid									(void) const;
	void						PrintMemoryUsage						(void);
#endif // __DEV

	const CImpulseTest*			FindImpulseTest							(u32& rnIndexOut, const u32 nDualImpulseTestId) const;
	const CInterrelationTest*	FindInterrelationTest					(u32& rnIndexOut, const u32 nInterrelationTestId) const;
	const CHoming*				FindHoming								(u32& rnIndexOut, const u32 nHomingId) const;
	const CDamageAndReaction*	FindDamageAndReaction					(u32& rnIndexOut, const u32 nDamageAndReactionId) const;
	const CStrikeBoneSet*		FindStrikeBoneSet						(u32& rnIndexOut, const u32 nStrikeBoneSetId) const;
	const CActionRumble*		FindActionRumble						(u32& rnIndexOut, const u32 nRumbleId) const;
	const CActionVfx*			FindActionVfx							(u32& rnIndexOut, const u32 nVfxId) const;
	const CActionFacialAnimSet*	FindActionFacialAnimSet					(u32& rnIndexOut, const u32 nFacialAnimSetId) const;

	const CActionDefinition*	GetActionDefinition						(u32 nIndexIn) const { return &m_aActionDefinitions[nIndexIn]; }
	const CActionDefinition*	FindActionDefinition					(u32& rnIndexOut, const u32 actionDefinitionId) const;
	const CActionBranchSet*		FindActionBranchSet						(u32& rnIndexOut, const u32 actionBranchSetId) const;
	const CActionResult*		FindActionResult						(u32& rnIndexOut, const u32 actionResultId) const;
	const CActionResult*		FindActionResultFromActionId			(CPed* pPed, const u32 actionId) const;

	const CActionDefinition*	SelectSuitableStealthKillAction			(const CPed* pPed,
																		 const CEntity* pTargetEntity,
																		 const bool bHasLockOnTarget,
																		 const CImpulseCombo* pImpulseCombo,
																		 const CActionFlags::ActionTypeBitSetData& typeToLookFor) const;

	const CActionDefinition*	SelectSuitableAction					(const CActionFlags::ActionTypeBitSetData& typeToLookFor,
																		 const CImpulseCombo* pImpulseCombo,
																		 const bool bUseAvailabilityFromIdleTest,
																		 const bool	bDesiredAvailabilityFromIdle,
																		 const CPed* pActingPed,
																		 const u32 nForcingResultId,
																		 const s32 nPriority,
																		 const CEntity* pTargetEntity,
																		 const bool bHasLockOnTargetEntity,
																		 const bool bImpulseInitiatedMovesOnly,
																		 const bool	bUseImpulseTest,
																		 const bool bDisplayDebug) const;

	bool						IsActionSuitable						(const u32 nActionId,
																		 const CActionFlags::ActionTypeBitSetData& typeToLookFor,
																		 const CImpulseCombo* pImpulseCombo,
																		 const bool bUseAvailabilityFromIdleTest,
																		 const bool bDesiredAvailabilityFromIdle,
																		 const CPed* pActingPed,
																		 const u32 nForcingResultId,
																		 const s32 nPriority,
																		 const CEntity* pTargetEntity,
																		 const bool bHasLockOnTargetEntity,
																		 const bool	bImpulseInitiatedMovesOnly,
																		 const bool	bUseImpulseTest,
																		 const bool bDisplayDebug) const;

	const CActionDefinition*	SelectActionFromSet						(atFixedArray<const CActionDefinition*,MAX_SUITABLE_DEFINITIONS>* paSuitableActionSet,
																		 const CPed* pActingPed,
																		 const bool bDisplayDebug) const;


protected:

	static void					ReloadActionTable				(void);
	static void					VerifyActionTableAnimationUsage	(void);
	void						ClearActionTableData			(void);

private:

#if __BANK
	static bool							ms_DebugFocusPed;
#endif

	atArray<CActionDefinition>			m_aActionDefinitions;
	atArray<CActionResult>				m_aActionResults;	
	atArray<CImpulseTest>				m_aImpulseTests;
	atArray<CInterrelationTest>			m_aInterrelationTests;
	atArray<CHoming>					m_aHomings;
	atArray<CDamageAndReaction>			m_aDamageAndReactions;
	atArray<CStrikeBoneSet>				m_aStrikeBoneSets;
	atArray<CActionBranchSet>			m_aActionBranchSets;
	atArray<CStealthKillTest>			m_aStealthKillTests;
	atArray<CActionRumble>				m_aActionRumbles;
	atArray<CActionVfx>					m_aActionVfx;
	atArray<CActionFacialAnimSet>		m_aActionFacialAnimSets;

	static atQueue<u32, ACTION_HISTORY_LENGTH> ms_qMeleeActionHistory;
	static atFixedArray<sSuitableActionInfo, MAX_SUITABLE_DEFINITIONS>  ms_aSuitableActionInfo;

	bool								m_bLoadActionTableDataOnInit;
};

typedef atSingleton<CActionManager> CActionManagerSingleton;
#define ACTIONMGR CActionManagerSingleton::InstanceRef()
#define INIT_ACTIONMGR									\
	do {												\
	if(!CActionManagerSingleton::IsInstantiated()) {	\
	CActionManagerSingleton::Instantiate();				\
	}													\
	} while(0)											\
	//END
#define SHUTDOWN_ACTIONMGR CActionManagerSingleton::Destroy()

#endif // INC_ACTIONMANAGER_H
