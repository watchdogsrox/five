// FILE :    NavCapabilities.h
// PURPOSE : Load in navigation capabilities for different types of peds
// CREATED : 4-21-2011

#ifndef NAV_CAPABILITIES_H
#define NAV_CAPABILITIES_H

#include "atl/array.h"
#include "atl/hashstring.h"
#include "fwutil/flags.h" //fwFlags
#include "parser/macros.h"
#include "system/bit.h"

class CPedNavCapabilityInfo
{
public:
	CPedNavCapabilityInfo();
	void Init();

	u32 GetHash() { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name from the metadata manager
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	typedef enum 
	{
		FLAG_MAY_CLIMB =					BIT0,
		FLAG_MAY_DROP =						BIT1,
		FLAG_MAY_JUMP =						BIT2,
		FLAG_MAY_ENTER_WATER =				BIT3,
		FLAG_MAY_USE_LADDERS =				BIT4,
		FLAG_MAY_FLY =						BIT5,
		FLAG_AVOID_OBJECTS =				BIT6,
		FLAG_USE_MASS_THRESHOLD =			BIT7,
		FLAG_PREFER_TO_AVOID_WATER =		BIT8,
		FLAG_AVOID_FIRE	=					BIT9,
		FLAG_PREFER_NEAR_WATER_SURFACE	=	BIT10,
		FLAG_SEARCH_FOR_PATHS_ABOVE_PED =	BIT11,
		FLAG_NEVER_STOP_NAVMESH_TASK_EARLY_IN_FOLLOW_LEADER = BIT12

	} NavCapabilityFlags;

	void SetFlag(NavCapabilityFlags flag, bool bValue=true)
	{
		m_BitFlags.ChangeFlag(flag, bValue);
	}

	bool ClearFlag(NavCapabilityFlags flag)
	{
		return m_BitFlags.ClearFlag(flag);
	}

	bool IsFlagSet(NavCapabilityFlags flag) const
	{
		return m_BitFlags.IsFlagSet(flag);
	}

	fwFlags32 GetAllFlags() const
	{
		return m_BitFlags;
	}

	void SetAllFlags(u32 flags)
	{
		m_BitFlags = flags;
	}

	u64 BuildPathStyleFlags() const;

	void SetClimbCostModifier(float f) { m_fClimbCostModifier = f; }
	float GetClimbCostModifier() const { return m_fClimbCostModifier; }

	float GetGotoTargetLookAheadDistance() const			{ return m_GotoTargetLookAheadDistance; }
	float GetGotoTargetInitialLookAheadDistance() const		{ return m_GotoTargetInitialLookAheadDistance; }
	float GetGotoTargetLookAheadDistanceIncrement() const	{ return m_GotoTargetLookAheadDistanceIncrement; }

	float GetDefaultMoveFollowLeaderRunDistance() const				{ return m_DefaultMoveFollowLeaderRunDistance; }
	float GetDefaultMoveFollowLeaderSprintDistance() const			{ return m_DefaultMoveFollowLeaderSprintDistance; }
	float GetDesiredMbrLowerBoundInMoveFollowLeader() const			{ return m_DesiredMbrLowerBoundInMoveFollowLeader; }
	float GetFollowLeaderMoveSeekEntityWalkSpeedRange() const		{ return m_FollowLeaderSeekEntityWalkSpeedRange; }
	float GetFollowLeaderMoveSeekEntityRunSpeedRange() const		{ return m_FollowLeaderSeekEntityRunSpeedRange; }
	float GetLooseFormationTargetRadiusOverride() const				{ return m_LooseFormationTargetRadiusOverride; }
	float GetFollowLeaderResetTargetDistanceSquaredOverride() const	{ return m_FollowLeaderResetTargetDistanceSquaredOverride; }
	float GetDefaultMaxSlopeNavigable() const						{ return m_DefaultMaxSlopeNavigable; }
	float GetStillLeaderSpacingEpsilon() const						{ return m_StillLeaderSpacingEpsilon; }
	float GetMoveFollowLeaderHeadingTolerance() const				{ return m_MoveFollowLeaderHeadingTolerance; } // in degrees
	float GetNonGroupMinDistToAdjustSpeed() const					{ return m_NonGroupMinDistToAdjustSpeed; }
	float GetNonGroupMaxDistToAdjustSpeed() const					{ return m_NonGroupMaxDistToAdjustSpeed; }

	bool CanSprintInFollowLeaderSeekEntity() const					{ return m_CanSprintInFollowLeaderSeekEntity; }
	bool UseAdaptiveMbrAccelInFollowLeader() const					{ return m_UseAdaptiveMbrInMoveFollowLeader; }
	bool AlwaysKeepMovingWhilstFollowingLeaderPath() const			{ return m_AlwaysKeepMovingWhilstFollowingLeaderPath; }
	bool ShouldIgnoreMovingTowardsLeaderTargetRadiusExpansion() const { return m_IgnoreMovingTowardsLeaderTargetRadiusExpansion; }
	
private:
	atHashWithStringNotFinal m_Name;
	fwFlags32 m_BitFlags;
	float m_MaxClimbHeight;
	float m_MaxDropHeight;
	float m_IgnoreObjectMass;
	float m_fClimbCostModifier;
	float m_GotoTargetLookAheadDistance;
	float m_GotoTargetInitialLookAheadDistance;
	float m_GotoTargetLookAheadDistanceIncrement;
	float m_DefaultMoveFollowLeaderRunDistance;
	float m_DefaultMoveFollowLeaderSprintDistance;
	float m_DesiredMbrLowerBoundInMoveFollowLeader;
	float m_FollowLeaderSeekEntityWalkSpeedRange;
	float m_FollowLeaderSeekEntityRunSpeedRange;
	float m_LooseFormationTargetRadiusOverride;
	float m_FollowLeaderResetTargetDistanceSquaredOverride;
	float m_DefaultMaxSlopeNavigable;
	float m_StillLeaderSpacingEpsilon;
	float m_MoveFollowLeaderHeadingTolerance;
	float m_NonGroupMinDistToAdjustSpeed;
	float m_NonGroupMaxDistToAdjustSpeed;
	bool m_CanSprintInFollowLeaderSeekEntity;
	bool m_UseAdaptiveMbrInMoveFollowLeader;
	bool m_AlwaysKeepMovingWhilstFollowingLeaderPath;
	bool m_IgnoreMovingTowardsLeaderTargetRadiusExpansion;
	PAR_SIMPLE_PARSABLE;
};

class CPedNavCapabilityInfoManager
{
public:
	// Initialise
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Access the Capsule information
	static const CPedNavCapabilityInfo* const GetInfo(const u32 uNameHash)
	{
		if(uNameHash != 0)
		{
			for (s32 i = 0; i < m_NavCapabilityManagerInstance.m_aPedNavCapabilities.GetCount(); i++ )
			{
				if(m_NavCapabilityManagerInstance.m_aPedNavCapabilities[i].GetHash() == uNameHash)
				{
					return &m_NavCapabilityManagerInstance.m_aPedNavCapabilities[i];
				}
			}
		}
		return NULL;
	}

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load(const char* pFileName);

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

	atArray<CPedNavCapabilityInfo> m_aPedNavCapabilities;

	static CPedNavCapabilityInfoManager m_NavCapabilityManagerInstance;

	PAR_SIMPLE_PARSABLE;
};

#endif //NAV_CAPABILITIES_H
