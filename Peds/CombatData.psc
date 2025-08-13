<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">

  <enumdef type="CCombatData::Movement"> 
    <enumval name="CM_Stationary"/>
    <enumval name="CM_Defensive"/>
    <enumval name="CM_WillAdvance"/>
    <enumval name="CM_WillRetreat"/>
  </enumdef> 

  <enumdef type="CCombatData::Ability">
    <enumval name="CA_Poor"/>
    <enumval name="CA_Average"/>
    <enumval name="CA_Professional"/>
    <enumval name="CA_NumTypes"/>
  </enumdef>

  <enumdef type="CCombatData::Range">
    <enumval name="CR_Near"/>
    <enumval name="CR_Medium"/>
    <enumval name="CR_Far"/>
    <enumval name="CR_VeryFar"/>
    <enumval name="CR_NumRanges"/>
  </enumdef>

  <enumdef type="CCombatData::TargetLossResponse">
    <enumval name="TLR_ExitTask"/>
    <enumval name="TLR_NeverLoseTarget"/>
    <enumval name="TLR_SearchForTarget"/>
  </enumdef>
 
  <enumdef type="CCombatData::TargetInjuredReaction">
    <enumval name="TIR_TreatAsDead" description="Ped will ignore as a target and continue as if the ped is dead"/>
    <enumval name="TIR_TreatAsAlive" description="Ped will see the target as a threat and continue combat as normal"/>
    <enumval name="TIR_Execute" description="Ped will approach the ped and shoot at close range"/>
    <enumval name="TIR_Count"/>
  </enumdef>

  <enumdef type="CCombatData::BehaviourFlags" generate="bitset">
    <enumval name="BF_CanUseCover"/>
    <enumval name="BF_CanUseVehicles"/>
    <enumval name="BF_CanDoDrivebys"/>
    <enumval name="BF_CanLeaveVehicle"/>
    <enumval name="BF_CanUseDynamicStrafeDecisions"/>
    <enumval name="BF_AlwaysFight"/>
    <enumval name="BF_FleeWhilstInVehicle"/>
    <enumval name="BF_JustFollowInVehicle"/>
    <enumval name="BF_Unused_3"/>
    <enumval name="BF_WillScanForDeadPeds"/>
    <enumval name="BF_Unused_1"/>
    <enumval name="BF_JustSeekCover"/>
    <enumval name="BF_BlindFireWhenInCover"/>
    <enumval name="BF_Aggressive"/>
    <enumval name="BF_CanInvestigate"/>
    <enumval name="BF_HasRadio"/>
    <enumval name="BF_Unused_2"/>
    <enumval name="BF_AlwaysFlee"/>
    <enumval name="BF_ForceInjuredOnGround"/>
    <enumval name="BF_DisableInjuredOnGround"/>
    <enumval name="BF_CanTauntInVehicle"/>
    <enumval name="BF_CanChaseTargetOnFoot"/>
	<enumval name="BF_WillDragInjuredPedsToSafety"/>
    <enumval name="BF_RequiresLosToShoot"/>
    <enumval name="BF_UseProximityFiringRate"/>
    <enumval name="BF_DisableSecondaryTarget"/>
    <enumval name="BF_DisableEntryReactions"/>
    <enumval name="BF_PerfectAccuracy"/>
    <enumval name="BF_CanUseFrustratedAdvance"/>
    <enumval name="BF_MoveToLocationBeforeCoverSearch"/>
    <enumval name="BF_CanShootWithoutLOS"/>
    <enumval name="BF_MaintainMinDistanceToTarget" description="Ped will try to keep their minimum distance to their target (for cover use only right now)"/>
    <enumval name="BF_IgnoreHatedPedsInFastMovingVehicles"/>
    <enumval name="BF_UseProximityAccuracy"/>
    <enumval name="BF_CanUsePeekingVariations"/>
    <enumval name="BF_DisablePinnedDown"/>
    <enumval name="BF_DisablePinDownOthers"/>
    <enumval name="BF_ClearAreaSetDefensiveIfDefensiveAreaReached"/>
    <enumval name="BF_DisableBulletReactions"/>
    <enumval name="BF_CanBust"/>
    <enumval name="BF_IgnoredByOtherPedsWhenWanted"/>
    <enumval name="BF_CanCommandeerVehicles"/>
    <enumval name="BF_CanFlank"/>
	<enumval name="BF_SwitchToAdvanceIfCantFindCover"/>
    <enumval name="BF_SwitchToDefensiveIfInCover"/>
    <enumval name="BF_ClearPrimaryDefensiveAreaWhenReached"/>
	<enumval name="BF_CanFightArmedPedsWhenNotArmed"/>
    <enumval name="BF_EnableTacticalPointsWhenDefensive"/>
    <enumval name="BF_DisableCoverArcAdjustments"/>
	<enumval name="BF_UseEnemyAccuracyScaling"/>
	<enumval name="BF_CanCharge"/>
    <enumval name="BF_ClearAreaSetAdvanceIfDefensiveAreaReached"/>
    <enumval name="BF_UseVehicleAttack"/>
    <enumval name="BF_UseVehicleAttackIfVehicleHasMountedGuns"/>
    <enumval name="BF_AlwaysEquipBestWeapon"/>
    <enumval name="BF_CanSeeUnderwaterPeds"/>
    <enumval name="BF_DisableAimAtAITargetsInHelis"/>
	<enumval name="BF_DisableSeekDueToLineOfSight"/>
    <enumval name="BF_DisableFleeFromCombat"/>
    <enumval name="BF_DisableTargetChangesDuringVehiclePursuit"/>
	<enumval name="BF_CanThrowSmokeGrenade"/>
    <enumval name="BF_NonMissionPedsFleeFromThisPedUnlessArmed"/>
    <enumval name="BF_ClearAreaSetDefensiveIfDefensiveCannotBeReached"/>
    <enumval name="BF_FleesFromInvincibleOpponents" />
    <enumval name="BF_DisableBlockFromPursueDuringVehicleChase" />
    <enumval name="BF_DisableSpinOutDuringVehicleChase" />
    <enumval name="BF_DisableCruiseInFrontDuringBlockDuringVehicleChase" />
    <enumval name="BF_CanIgnoreBlockedLosWeighting" />
	<enumval name="BF_DisableReactToBuddyShot" />
    <enumval name="BF_PreferNavmeshDuringVehicleChase" />
    <enumval name="BF_AllowedToAvoidOffroadDuringVehicleChase" />
	<enumval name="BF_PermitChargeBeyondDefensiveArea" />
	<enumval name="BF_UseRocketsAgainstVehiclesOnly" />
    <enumval name="BF_DisableTacticalPointsWithoutClearLos" />
    <enumval name="BF_DisablePullAlongsideDuringVehicleChase" />
    <enumval name="BF_DisableShoutTargetPosition" />
    <enumval name="BF_SetDisableShoutTargetPositionOnCombatStart" />
    <enumval name="BF_DisableRespondedToThreatBroadcast" />
    <enumval name="BF_DisableAllRandomsFlee" />
    <enumval name="BF_WillGenerateDeadPedSeenScriptEvents" />
    <enumval name="BF_UseMaxSenseRangeWhenReceivingEvents" />
    <enumval name="BF_RestrictInVehicleAimingToCurrentSide" />
	<enumval name="BF_UseDefaultBlockedLosPositionAndDirection" />
    <enumval name="BF_RequiresLosToAim" />
    <enumval name="BF_CruiseAndBlockInVehicle"/>
    <enumval name="BF_PreferAirCombatWhenInAircraft"/>
    <enumval name="BF_AllowDogFighting"/>
    <enumval name="BF_PreferNonAircraftTargets"/>
    <enumval name="BF_PreferKnownTargetsWhenCombatClosestTarget"/>
    <enumval name="BF_ForceCheckAttackAngleForMountedGuns"/>
    <enumval name="BF_BlockFireForVehiclePassengerMountedGuns"/>
    <enumval name="MAX_COMBAT_FLAGS"/>
  </enumdef>

</ParserSchema>
