<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CWeaponAnimations" onPostLoad="OnPostLoad">
    <string name="CoverMovementClipSetHash" type="atHashString"/>
    <string name="CoverMovementExtraClipSetHash" type="atHashString"/>
    <string name="CoverAlternateMovementClipSetHash" type="atHashString"/>
    <string name="CoverWeaponClipSetHash" type="atHashString"/>
    <string name="MotionClipSetHash" type="atHashString"/>
    <string name="MotionFilterHash" type="atHashString"/>
    <string name="MotionCrouchClipSetHash" type="atHashString"/>
    <string name="MotionStrafingClipSetHash" type="atHashString"/>
    <string name="MotionStrafingStealthClipSetHash" type="atHashString"/>
    <string name="MotionStrafingUpperBodyClipSetHash" type="atHashString"/>
    <string name="WeaponClipSetHash" type="atHashString"/>
    <string name="WeaponClipSetStreamedHash" type="atHashString"/>
    <string name="WeaponClipSetHashInjured" type="atHashString"/>
    <string name="WeaponClipSetHashStealth" type="atHashString"/>
    <string name="WeaponClipSetHashHiCover" type="atHashString"/>    
    <string name="AlternativeClipSetWhenBlocked" type="atHashString"/>
    <string name="ScopeWeaponClipSet" type="atHashString"/>
    <string name="AlternateAimingStandingClipSetHash" type="atHashString"/>
    <string name="AlternateAimingCrouchingClipSetHash" type="atHashString"/>
    <string name="FiringVariationsStandingClipSetHash" type="atHashString"/>
    <string name="FiringVariationsCrouchingClipSetHash" type="atHashString"/>
    <string name="AimTurnStandingClipSetHash" type="atHashString"/>
    <string name="AimTurnCrouchingClipSetHash" type="atHashString"/>
	<string name="MeleeBaseClipSetHash" type="atHashString"/>
    <string name="MeleeClipSetHash" type="atHashString"/>
    <string name="MeleeVariationClipSetHash" type="atHashString"/>
    <string name="MeleeTauntClipSetHash" type="atHashString"/>
    <string name="MeleeSupportTauntClipSetHash" type="atHashString"/>
	<string name="MeleeStealthClipSetHash" type="atHashString"/>
    <string name="ShellShockedClipSetHash" type="atHashString"/>
    <string name="JumpUpperbodyClipSetHash" type="atHashString"/>
    <string name="FallUpperbodyClipSetHash" type="atHashString"/>
    <string name="FromStrafeTransitionUpperBodyClipSetHash" type="atHashString"/>
    <string name="SwapWeaponFilterHash" type="atHashString"/>
    <string name="SwapWeaponInLowCoverFilterHash" type="atHashString"/>
    <float name="AnimFireRateModifier" init="1.0f"/>
    <float name="AnimBlindFireRateModifier" init="1.0f"/>
    <float name="AnimWantingToShootFireRateModifier" init="-1.0f"/>
	<bool name="UseFromStrafeUpperBodyAimNetwork" init="true"/>
	<bool name="AimingDownTheBarrel" init="true"/>
    <pointer name="WeaponSwapData" type="CWeaponComponentData" policy="external_named" toString="CWeaponComponentManager::GetNameFromData" fromString="CWeaponComponentManager::GetDataFromName"/>
    <string name="WeaponSwapClipSetHash" type="atHashString"/>
    <string name="AimGrenadeThrowNormalClipsetHash" type="atHashString"/>
    <string name="AimGrenadeThrowAlternateClipsetHash" type="atHashString"/>
	<string name="GestureBeckonOverrideClipSetHash" type="atHashString"/>
	<string name="GestureOverThereOverrideClipSetHash" type="atHashString"/>
	<string name="GestureHaltOverrideClipSetHash" type="atHashString"/>
	<string name="GestureGlancesOverrideClipSetHash" type="atHashString"/>
	<string name="CombatReactionOverrideClipSetHash" type="atHashString"/>
	<bool name="UseLeftHandIKAllowTags" init="false"/>
  <bool name="BlockLeftHandIKWhileAiming" init="false"/>
	<string name="FPSTransitionFromIdleHash" type="atHashString"/>
	<string name="FPSTransitionFromRNGHash" type="atHashString"/>
	<string name="FPSTransitionFromLTHash" type="atHashString"/>
	<string name="FPSTransitionFromScopeHash" type="atHashString"/>
    <string name="FPSTransitionFromUnholsterHash" type="atHashString"/>
	<string name="FPSTransitionFromStealthHash" type="atHashString"/>
	<string name="FPSTransitionToStealthHash" type="atHashString"/>	
	<string name="FPSTransitionToStealthFromUnholsterHash" type="atHashString"/>	
	<array name="FPSFidgetClipsetHashes" type="atArray">
		<string type="atHashString"/>
	</array>
	<string name="MovementOverrideClipSetHash" type="atHashString"/>
	<string name="WeaponClipSetHashForClone" type="atHashString"/>
    <string name="MotionClipSetHashForClone" type="atHashString"/>
  </structdef>

  <structdef type="CWeaponAnimationsSet">
    <string name="m_Fallback" type="atHashString"/>
    <string name="FPSStrafeClipSetHash" type="atHashString"/>
    <map name="m_WeaponAnimations" type="atBinaryMap" key="atHashString">
      <struct type="CWeaponAnimations" />
    </map>
  </structdef>

  <structdef type="CWeaponAnimationsSets">
    <map name="m_WeaponAnimationsSets" type="atBinaryMap" key="atHashString">
      <struct type="CWeaponAnimationsSet" />
    </map>
  </structdef>

</ParserSchema>
