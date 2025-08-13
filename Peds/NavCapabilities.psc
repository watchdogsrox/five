<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="CPedNavCapabilityInfo::NavCapabilityFlags">
	<enumval name="MayClimb"/>
	<enumval name="MayDrop"/>
	<enumval name="MayJump"/>
	<enumval name="MayEnterWater"/>
	<enumval name="MayUseLadders"/>
	<enumval name="MayFly"/>
	<enumval name="AvoidObjects"/>
	<enumval name="UseMassThreshold"/>
	<enumval name="PreferToAvoidWater"/>
	<enumval name="AvoidFire"/>
	<enumval name="PreferNearWaterSurface"/>
	<enumval name="SearchForPathsAbovePed"/>
	<enumval name="NeverStopNavmeshTaskEarlyInFollowLeader"/>
</enumdef>

<structdef type="CPedNavCapabilityInfo">
	<string name="m_Name" type="atHashString"/>
	<bitset name="m_BitFlags" type="fixed32" values="CPedNavCapabilityInfo::NavCapabilityFlags"/>
	<float name="m_MaxClimbHeight" init="0" min="0" max="10000"/>
	<float name="m_MaxDropHeight" init="0" min="0" max="10000"/>
	<float name="m_IgnoreObjectMass" init="0" min="0" max="10000"/>
	<float name="m_fClimbCostModifier" init="1.0" min="0" max="10"/>
	<float name="m_GotoTargetLookAheadDistance" min="0.0f" max="20.0f" step="0.01f" init="1.5f"/>
	<float name="m_GotoTargetInitialLookAheadDistance" min="0.0f" max="20.0f" step="0.01f" init="0.25f"/>
	<float name="m_GotoTargetLookAheadDistanceIncrement" min="0.0f" max="20.0f" step="0.01f" init="4.0f"/>
	<float name="m_DefaultMoveFollowLeaderRunDistance" min="0.0f" max="30.0f" step="0.1f" init="6.0f"/>
	<float name="m_DefaultMoveFollowLeaderSprintDistance" min="0.0f" max="30.0f" step="0.1f" init="9.0f"/>
	<float name="m_DesiredMbrLowerBoundInMoveFollowLeader" min="0.0f" max="3.0f" step="0.1f" init="1.0f"/>
	<float name="m_FollowLeaderSeekEntityWalkSpeedRange" min="0.0f" max="30.0f" step="0.1f" init="10.0f"/>
	<float name="m_LooseFormationTargetRadiusOverride" min="-1.0f" max="30.0f" step="0.1f" init="-1.0f"/>
	<float name="m_FollowLeaderResetTargetDistanceSquaredOverride" min="-1.0f" max="10.0f" init="-1.0f"/>
	<float name="m_FollowLeaderSeekEntityRunSpeedRange" min="0.0f" max="30.0f" step="0.1f" init="20.0f"/>
	<float name="m_DefaultMaxSlopeNavigable" min="0.0f" max="3.14f" step="0.01f" init="0.0f"/>
	<float name="m_StillLeaderSpacingEpsilon" min="0.0f" max="10.0f" step="0.01f" init="0.0f"/>
	<float name="m_MoveFollowLeaderHeadingTolerance" min="0.0f" max="90.0f" step="0.01f" init="0.0f"/>
	<float name="m_NonGroupMinDistToAdjustSpeed" min="0.0f" max="100.0f" step="0.01f" init="3.0f"/>
	<float name="m_NonGroupMaxDistToAdjustSpeed" min="0.0f" max="100.0f" step="0.01f" init="16.0f"/>
	<bool name="m_CanSprintInFollowLeaderSeekEntity" init="false"/>
	<bool name="m_UseAdaptiveMbrInMoveFollowLeader" init="false"/>
	<bool name="m_AlwaysKeepMovingWhilstFollowingLeaderPath" init="false"/>
	<bool name="m_IgnoreMovingTowardsLeaderTargetRadiusExpansion" init="false" />
</structdef>

<structdef type="CPedNavCapabilityInfoManager">
	<array name="m_aPedNavCapabilities" type="atArray">
		<struct type="CPedNavCapabilityInfo"/>
	</array>
</structdef>

</ParserSchema>
