#ifndef INC_SCENARIO_INFO_H
#define INC_SCENARIO_INFO_H
 
// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "fwanimation/animdefines.h"
#include "fwtl/pool.h"
#include "fwutil/rtti.h"
#include "parser/macros.h"
#include "vector/quaternion.h"
#include "vector/vector3.h"

// Game headers
#include "AI/Ambient/ConditionalAnims.h"
#include "Task/Scenario/Info/ScenarioInfoFlags.h"
#include "Task/Scenario/Info/ScenarioTypes.h"

// Forward declarations
class CAmbientModelSet;
class CAmbientPedModelSet;
class CScenarioCondition;
class CScenarioConditionWorld;
class CVehicle;


////////////////////////////////////////////////////////////////////////////////
// CScenarioInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioInfo
{
	DECLARE_RTTI_BASE_CLASS(CScenarioInfo);
public:
	FW_REGISTER_CLASS_POOL(CScenarioInfo);

	// Construction
	CScenarioInfo();

	// Destruction
	virtual ~CScenarioInfo()
	{
	}

	// Scenario hash
	u32 GetHash() const { return m_Name.GetHash(); }

	// Prop hash
	u32 GetPropHash() const;

	bool IsEnabled(){ return m_Enabled; }
	void SetIsEnabled(bool val){ m_Enabled = val; }
	const CConditionalAnimsGroup& GetConditionalAnimsGroup() const { return m_ConditionalAnimsGroup; }
	const CConditionalAnimsGroup * GetClipData() const { return &m_ConditionalAnimsGroup; }

#if !__FINAL
	// Scenario name
	const char* GetName() const { return m_Name.GetCStr(); }
	// Prop name
	const char* GetPropName() const { return m_PropName.GetCStr();}
#endif // !__FINAL

	// Get the model set
	const CAmbientPedModelSet* GetModelSet() const { return m_Models; }

	// Get the blocked model set
	const CAmbientPedModelSet* GetBlockedModelSet() const { return m_BlockedModels; }

	//
	// Creation data
	//

	// Get the probability the scenario will spawn
	f32 GetSpawnProbability() const { return m_SpawnProbability; }

	// Set the probability the scenario will spawn
	void SetSpawnProbability(float f) { m_SpawnProbability = f; }

	// Get the minimum time between spawning scenarios of this type (in milliseconds)
	u32 GetSpawnInterval() const { return m_SpawnInterval; }

	// The range at which a previously spawned object will be "remembered" to not allow spawning of another object
	//	until the spawned object is out of range (default 50 meters)
	f32 GetSpawnHistoryRange() const { return m_SpawnHistoryRange; }

	// Get the time this scenario last spawned (in milliseconds)
	u32 GetLastSpawnTime() const { return m_LastSpawnTime; }

	// Set the time the scenario last spawned (in milliseconds)
	void SetLastSpawnTime(const u32 uLastSpawnTime) { m_LastSpawnTime = uLastSpawnTime; }

	// Get the max number of active scenarios of this type within the specified range
	u32 GetMaxNoInRange() const { return m_MaxNoInRange; }

	// Get the range
	f32 GetRange() const { return m_Range; }

	// Get the spawn prop offset anim dictionary hash
	u32	GetSpawnPropOffsetDictHash() const { return m_SpawnPropIntroDict.GetHash(); }

	// Get the spawn prop offset anim hash
	u32	GetSpawnPropOffsetClipHash() const { return m_SpawnPropIntroAnim.GetHash(); }

#if !__FINAL
	// Get the name of the intro dictionary
	const char* GetSpawnPropIntroDictName() const { return m_SpawnPropIntroDict.GetCStr(); }

	// Get the name of the intro anim
	const char* GetSpawnPropIntroClipName() const { return m_SpawnPropIntroAnim.GetCStr(); }

	bool HasAnimationData() const { return m_ConditionalAnimsGroup.HasAnimationData(); }

	bool HasProp() const;

#endif // !__FINAL

	// Get the spawn prop offset
	const Vector3& GetSpawnPropOffset() const { return m_SpawnPropOffset; }

	// Set the spawn prop offset
	void SetSpawnPropOffset(const Vector3& vOffset) { m_SpawnPropOffset = vOffset; }

	// Get the rotational prop spawn offset
	const Quaternion& GetSpawnPropRotationalOffset() const { return m_SpawnPropRotation; }

	// Set the rotational prop spawn offset
	void SetSpawnPropRotationalOffset(const Quaternion& qOffset) { m_SpawnPropRotation = qOffset; }

	//
	// Scenario data
	//

	// Get the time till the ped will stop what they are doing
	f32 GetTimeTillPedLeaves() const { return m_TimeTilPedLeaves; }

	// Get the chance of running
	f32 GetChanceOfRunningTowards() const { return m_ChanceOfRunningTowards; }

	// Get the end of life timeout in ms
	u32 GetPropEndOfLifeTimeoutMS() const { return m_PropEndOfLifeTimeoutMS; }

	//
	// Condition querying
	//

	// Check a flag
	bool GetIsFlagSet(CScenarioInfoFlags::Flags f) const { return m_Flags.BitSet().IsSet(f); }

	// Get all flags, useful for checking multiple flags in a single operation
	const CScenarioInfoFlags::FlagsBitSet& GetFlags() const
	{	return m_Flags;	}

	// Get the condition
	const CScenarioConditionWorld* GetCondition() const { return m_Condition; }

	// Get the stationary react hash
	atHashWithStringNotFinal GetStationaryReactHash() const { return m_StationaryReactHash; }

	// Get the camera name hash
	atHashWithStringNotFinal GetCameraNameHash() const { return m_CameraNameHash; }

	// Get enter anim blend duration
	float GetIntroBlendInDuration() const { return m_IntroBlendInDuration; }

	// Get exit anim blend duration
	float GetOutroBlendInDuration() const { return m_OutroBlendInDuration; }

	// Get exit anim blend out duration
	float GetOutroBlendOutDuration() const { return m_OutroBlendOutDuration; }

	// Get the duration for the blend out of the ambient clips subtask on immediate events.
	float GetImmediateExitBlendOutDuration() const { return m_ImmediateExitBlendOutDuration; }

	// Get the duration for the blend in for the panic exit clip in seconds.
	float GetPanicExitBlendInDuration() const { return m_PanicExitBlendInDuration; }

	// Get the capsule offset while using.
	Vector3 GetCapsuleOffset() const { return m_PedCapusleOffset; }

	// Return a value this scenario can replace the user's ped capsule radius with.  -1 is interpreted as no override.
	float GetCapsuleRadiusOverride() const { return m_PedCapsuleRadiusOverride; }

	// Height threshold for special logic dealing with small falls during the exit.
	float GetReassessGroundExitThreshold() const { return m_ReassessGroundExitThreshold; }

	// Height threshold for special logic dealing with large falls during the exit.
	float GetFallExitThreshold() const { return m_FallExitThreshold; }

	// If nonzero, this replaces the anim-derived end position for the exit probe helper checks.
	float GetProbeHelperFinalZOverride() const { return m_ExitProbeZOverride; }

	// If nonzero, this replaces the capsule radius used during the exit probe helper checks.
	float GetProbeHelperPedCapsuleRadiusOverride() const { return m_ExitProbeCapsuleRadiusOverride; }

	// If positive, this replaces the default distance to adjust path request start positions when exiting.
	float GetMaxDistanceMayAdjustPathSearchOnExit() const { return m_MaxDistanceMayAdjustPathSearchOnExit; }

	// Look-At importance level. Makes this scenario more or less likely to be looked at.
	eLookAtImportance GetLookAtImportance() const { return m_eLookAtImportance; }

private:

	//
	// Members
	//
	// Is scenario enabled
	bool		m_Enabled;
	// The hash of the name
	atHashWithStringNotFinal m_Name;

	// Prop that this scenario should be associated with
	atHashWithStringNotFinal m_PropName;

	// Models to use if specified
	CAmbientPedModelSet* m_Models;

	// Don't use the models specified in this list
	CAmbientPedModelSet* m_BlockedModels;

	//
	// Creation data
	//

	// Chance of spawning
	f32 m_SpawnProbability;
	
	// Minimum time between spawning scenarios of this type
	u32 m_SpawnInterval;

	// The range at which a previously spawned object will be "remembered" to not allow spawning of another object
	//	until the spawned object is out of range (default 50 meters)
	f32 m_SpawnHistoryRange;

	// The time this scenario last spawned
	u32 m_LastSpawnTime;
	
	// Max number of active scenarios of this type within the specified range
	u32 m_MaxNoInRange;

	// Time in seconds before dropped prop disappears
	u32 m_PropEndOfLifeTimeoutMS;

	// Range
	f32 m_Range;

	// Name of dictionary to extract prop offset from 
	atHashWithStringNotFinal m_SpawnPropIntroDict;
	
	// Name of anim to extract prop offset from 
	atHashWithStringNotFinal m_SpawnPropIntroAnim;

	// StationaryReact hash string
	atHashWithStringNotFinal m_StationaryReactHash;

	//
	// Scenario data
	//

	// The time before the ped quits the scenario task
	f32 m_TimeTilPedLeaves;

	// Chance of running to waypoints - scenario specific.
	f32 m_ChanceOfRunningTowards;

	// Offset from prop entity to spawn the ped (this is generated offline from the aboves animation clip offsets)
	Vector3 m_SpawnPropOffset;

	// Rotational prop offset
	Quaternion m_SpawnPropRotation;

	//
	// Conditions
	//

	// Flags
	CScenarioInfoFlags::FlagsBitSet m_Flags;

	// Conditions
	CScenarioConditionWorld* m_Condition;

	//
	// Animations
	//

	// Animations
	CConditionalAnimsGroup m_ConditionalAnimsGroup;

	// Cameras
	atHashWithStringNotFinal m_CameraNameHash;

	// Blend durations

	float m_IntroBlendInDuration;

	float m_OutroBlendInDuration;

	float m_OutroBlendOutDuration;

	float m_ImmediateExitBlendOutDuration;

	float m_PanicExitBlendInDuration;

	// Exit fall tolerances

	// Dealing with small falls.
	float	m_ReassessGroundExitThreshold;

	// Dealing with larger falls.
	float	m_FallExitThreshold;

	// Where to force the final position for the probe helper in the Z.
	float	m_ExitProbeZOverride;

	// What to force the ped capsule override to when performing a check.
	float	m_ExitProbeCapsuleRadiusOverride;

	// If positive, this replaces the default distance to adjust path request start positions when exiting.
	float	m_MaxDistanceMayAdjustPathSearchOnExit;

	// Look-At importance level.
	eLookAtImportance m_eLookAtImportance;

	// Bounds offsets

	Vector3 m_PedCapusleOffset;

	// Bound radius override
	float	m_PedCapsuleRadiusOverride;
	
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioPlayAnimsInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioPlayAnimsInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioPlayAnimsInfo, CScenarioInfo);
public:

	CScenarioPlayAnimsInfo();

	// Get the seated offset
	const Vector3& GetSpawnPointAdditionalOffset() const { return m_SeatedOffset; }

	u32 GetWanderScenarioToUseAfterHash() const
	{	return m_WanderScenarioToUseAfter.GetHash();	}

#if !__FINAL
	const char* GetWanderScenarioToUseAfter() const
	{	return m_WanderScenarioToUseAfter.GetCStr();	}
#endif

private:

	//
	// Members
	//

	// Seated offset
	Vector3 m_SeatedOffset;

	// PURPOSE:	Optional name of a wander scenario to implicitly use when done with this scenario.
	atHashWithStringNotFinal	m_WanderScenarioToUseAfter;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioWanderingInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioWanderingInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioWanderingInfo, CScenarioInfo);
public:
	CScenarioWanderingInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioWanderingInRadiusInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioWanderingInRadiusInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioWanderingInRadiusInfo, CScenarioInfo);
public:

	CScenarioWanderingInRadiusInfo();

	f32 GetWanderRadius() const { return m_WanderRadius; }

private:

	// The radius at which we want to wander in
	f32 m_WanderRadius;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioGroupInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioGroupInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioGroupInfo, CScenarioInfo);
public:
	CScenarioGroupInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioCoupleInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioCoupleInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioCoupleInfo, CScenarioInfo);
public:

	CScenarioCoupleInfo();

	// Get the move follow offset
	const Vector3& GetMoveFollowOffset() const { return m_MoveFollowOffset; }

	// Should we use head look at?
	bool GetUseHeadLookAt() const { return m_UseHeadLookAt; }

private:

	//
	// Members
	//

	// Follow offset
	Vector3 m_MoveFollowOffset;

	// Flags
	bool m_UseHeadLookAt;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioControlAmbientInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioControlAmbientInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioControlAmbientInfo, CScenarioInfo);
public:
	CScenarioControlAmbientInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioSkiingInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioSkiingInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioSkiingInfo, CScenarioInfo);
public:
	CScenarioSkiingInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioSkiLiftInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioSkiLiftInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioSkiLiftInfo, CScenarioInfo);
public:
	CScenarioSkiLiftInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioMoveBetweenInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioMoveBetweenInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioMoveBetweenInfo, CScenarioInfo);
public:

	CScenarioMoveBetweenInfo();

	// Get the time between moving
	float GetTimeBetweenMoving() const { return m_TimeBetweenMoving; }

private:

	//
	// Members
	//

	// Get the time between moving
	float m_TimeBetweenMoving;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioSniperInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioSniperInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioSniperInfo, CScenarioInfo);
public:
	CScenarioSniperInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioJoggingInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioJoggingInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioJoggingInfo, CScenarioInfo);
public:
	CScenarioJoggingInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioParkedVehicleInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioParkedVehicleInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioParkedVehicleInfo, CScenarioInfo);
public:
	CScenarioParkedVehicleInfo();

	eVehicleScenarioType GetVehicleScenarioType() const { return m_eVehicleScenarioType; }

private:

	// Scenario Type
	eVehicleScenarioType m_eVehicleScenarioType;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioVehicleInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioVehicleInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioVehicleInfo, CScenarioInfo);
public:

	CScenarioVehicleInfo();

	// PURPOSE:	Get the vehicle model set.
	const CAmbientModelSet* GetModelSet() const { return m_VehicleModelSet; }

	// PURPOSE:	Get the trailer model set.
	const CAmbientModelSet* GetTrailerModelSet() const { return m_VehicleTrailerModelSet; }

	float GetProbabilityForDriver() const { return m_ProbabilityForDriver; }
	float GetProbabilityForPassengers() const { return m_ProbabilityForPassengers; }
	float GetProbabilityForTrailer() const { return m_ProbabilityForTrailer; }
	unsigned int GetMaxNumPassengers() const { return m_MaxNumPassengers; }
	unsigned int GetMinNumPassengers() const { return m_MinNumPassengers; }

	bool AlwaysNeedsDriver() const { return m_ProbabilityForDriver > 0.999f; }
	bool AlwaysNeedsPassengers() const { return m_ProbabilityForPassengers > 0.999f; }

	u32 GetScenarioLayoutForPedsHash() const { return m_ScenarioLayoutForPeds.GetHash(); }
	eVehicleScenarioPedLayoutOrigin GetScenarioLayoutOrigin() const { return m_ScenarioLayoutOrigin; }

#if !__FINAL
	const char* GetScenarioLayoutForPeds() const { return m_ScenarioLayoutForPeds.GetCStr(); }
#endif	// !__FINAL

protected:
	// PURPOSE:	Name of vehicle scenario layout to use for spawning peds around the vehicle.
	atHashWithStringNotFinal m_ScenarioLayoutForPeds;

	// PURPOSE:	Specifies what coordinates in m_ScenarioLayoutForPeds are relative to.
	eVehicleScenarioPedLayoutOrigin m_ScenarioLayoutOrigin;

	// PURPOSE:	Models to use if specified.
	CAmbientModelSet* m_VehicleModelSet;

	// PURPOSE:	Models to use for trailers, if specified.
	CAmbientModelSet* m_VehicleTrailerModelSet;

	// PURPOSE:	Probability that a driver should spawn with the vehicle.
	float	m_ProbabilityForDriver;

	// PURPOSE:	Probability that passengers should spawn with the vehicle.
	float	m_ProbabilityForPassengers;

	// PURPOSE:	Probability for a trailer to spawn, if one is specified.
	float	m_ProbabilityForTrailer;

	// PURPOSE:	If spawning with passengers, this is the maximum number allowed.
	u8		m_MaxNumPassengers;

	// PURPOSE:	If spawning with passengers, this is the minimum number we try to create.
	u8		m_MinNumPassengers;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioVehicleInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioVehicleParkInfo : public CScenarioVehicleInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioVehicleParkInfo, CScenarioVehicleInfo);
public:

	CScenarioVehicleParkInfo();

	u8 GetParkType() const { return m_ParkType; }

protected:

	// PURPOSE: Should be the type of parking we want to do... (default is CTaskVehicleParkNew::Park_Perpendicular_NoseIn)
	u8 /*CTaskVehicleParkNew::ParkType*/ m_ParkType;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioFleeInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioFleeInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioFleeInfo, CScenarioInfo);
public:

	CScenarioFleeInfo();

	// Get the radius at which we are safe from the flee source when at this scenario
	float GetSafeRadius() const { return m_SafeRadius; }

private:

	// The radius at which we are safe from the flee source when at this scenario
	float m_SafeRadius;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioLookAtInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioLookAtInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioLookAtInfo, CScenarioInfo);
public:

	CScenarioLookAtInfo();

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CScenarioDeadPedInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioDeadPedInfo : public CScenarioInfo
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioDeadPedInfo, CScenarioInfo);
public:

	CScenarioDeadPedInfo();

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// DefaultScenarioInfo
////////////////////////////////////////////////////////////////////////////////

enum DefaultScenarioType
{
	DS_Invalid = -1,
	DS_StreetSweeper = 0,
	DS_StealVehicle
};

class DefaultScenarioInfo
{
public:
	DefaultScenarioInfo(DefaultScenarioType defaultScenario, CVehicle* pVehicleToSteal)
		: m_defaultScenario(defaultScenario)
		, m_pVehicleToSteal(pVehicleToSteal)
	{
	}
	DefaultScenarioInfo()
		: m_defaultScenario(DS_Invalid)
		, m_pVehicleToSteal(NULL){}
	~DefaultScenarioInfo() {}

	DefaultScenarioType	GetType() const { return m_defaultScenario; }
	CVehicle*			GetVehicleToSteal() const { return m_pVehicleToSteal; }

protected:
	DefaultScenarioType	m_defaultScenario;
	RegdVeh				m_pVehicleToSteal;
};

#endif // INC_SCENARIO_INFO_H
