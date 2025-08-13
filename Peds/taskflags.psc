<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">

<enumdef type="CTaskFlags::Flags" generate="bitset">
	<enumval name="ConsiderAsInWaterInLowLodPhysics"/>
	<enumval name="DisableCover"/>
	<enumval name="DisableCower"/>
	<enumval name="DisableHandsUp"/>
	<enumval name="DisableVehicleUsage"/>
	<enumval name="DisableNonStandardDamageTypes"/>
	<enumval name="DisableUseScenariosWithNoModelSet"/>
	<enumval name="DisableWallHitAnimation"/>
	<enumval name="DisableReactAndFleeAnimation"/>
	<enumval name="DisablePotentialBlastResponse"/>
	<enumval name="DisablePotentialToBeWalkedIntoResponse"/>
	<enumval name="PreferFleeOnPavements"/>
	<enumval name="DisableMeleeIntroAnimation"/>
	<enumval name="PlayNudgeAnimationForBumps"/>
	<enumval name="DontInfluenceWantedLevel"/>
	<enumval name="DisableBraceForImpact"/>
	<enumval name="CanScreamDuringFlee"/>
	<enumval name="IgnorePavementCheckWhenLeavingScenarios"/>
	<enumval name="DiesInstantlyToFire"/>
	<enumval name="UseAmbientScaling"/>
	<enumval name="CanBeShoved"/>
	<enumval name="DisableEvasiveStep"/>
	<enumval name="FleeFromCombatWhenTargetIsInVehicle"/>
	<enumval name="CanBeTalkedTo"/>
	<enumval name="AlwaysSprintWhenFleeingInThreatResponse"/>
	<enumval name="UseAquaticRoamMode"/>
	<enumval name="ApplyExtraHeadingChangesInFishLocomotion"/>
	<enumval name="AlignPitchToWavesInFishLocomotion"/>
	<enumval name="UseSimplePitchingInFishLocomotion"/>
	<enumval name="ProbeForCollisionsInScenarioFlee"/>
	<enumval name="SwimStraightInSwimmingWander"/>
	<enumval name="UseSimpleWander"/>
	<enumval name="BlockIdleTurnsInSmartFleeWhileWaitingOnPath"/>
	<enumval name="ForceSlowChaseInAnimalMelee"/>
	<enumval name="ExpandAvoidanceRadius"/>
	<enumval name="DependentAmbientFriend"/>
	<enumval name="UseLooseCrowdAroundMetrics"/>
	<enumval name="UseLooseHeadingAdjustmentsInMelee"/>
	<enumval name="ForceNoTurningInFishLocomotion"/>
	<enumval name="UseLongerBlendsInFishLocomotion"/>
	<enumval name="ForceSlowSwimWhenUnderPlayerControl"/>
	<enumval name="CanShove"/>
	<enumval name="BlockPlayerFishPitchingWhenSlow"/>
	<enumval name="UseSlowPlayerFishPitchAcceleration"/>
	<enumval name="IgnoreDelaysOnGunshotEvents"/>
	<enumval name="DiesInstantlyToMelee"/>
</enumdef>
	
</ParserSchema>
