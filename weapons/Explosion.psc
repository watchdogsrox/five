<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CExplosionTagData">
    <string name="name" type="atString" ui_key="true"/>
    <float name="damageAtCentre" init="0" min="0" max="10000"/>
    <float name="damageAtEdge" init="0" min="0" max="10000"/>
    <float name="networkPlayerModifier" init="0" min="0" max="2.0"/>
    <float name="networkPedModifier" init="0" min="0" max="1.00"/>
    <float name="endRadius" init="0" min="0" max="1000"/>
    <float name="initSpeed" init="0" min="0" max="1000"/>
    <float name="decayFactor" init="0" min="-1000" max="1000"/>
    <float name="forceFactor" init="0" min="0" max="1000"/>
    <float name="fRagdollForceModifier" init="0" min="0" max="1000"/>
    <float name="fSelfForceModifier" init="1" min="0" max="1000"/>
    <float name="directedWidth" init="0" min="0" max="1000"/>
    <float name="directedLifeTime" init="0" min="0" max="1000"/>
    <string name="camShakeName" type="atHashString"/>
    <float name="camShake" init="0" min="0" max="1000"/>
    <float name="camShakeRollOffScaling" init="1.0f" min="0.0f" max="10.0f"/>
    <float name="shockingEventVisualRangeOverride" init="-1.0f"/>
    <float name="shockingEventAudioRangeOverride" init="-1.0f"/>
    <bool name="minorExplosion" init="1"/>
    <string name="vfxTagHashName" type="atHashString"/>
    <float name="fragDamage" init="0" min="0" max="10000"/>
    <bool name="bAppliesContinuousDamage" init="false"/>
    <bool name="bPostProcessCollisionsWithNoForce" init="false"/>
    <bool name="bDamageVehicles" init="true"/>
    <bool name="bDamageObjects" init="true"/>
    <bool name="bOnlyAffectsLivePeds" init="false"/>
    <bool name="bIgnoreExplodingEntity" init="false"/>
    <bool name="bAlwaysAllowFriendlyFire" init="false"/>
    <bool name="bNoOcclusion" init="false"/>
	<bool name="explodeAttachEntityWhenFinished" init="false"/>
	<bool name="bCanSetPedOnFire" init="false"/>
	<bool name="bCanSetPlayerOnFire" init="false"/>
    <bool name="bSuppressCrime" init="false"/>
    <bool name="bUseDistanceDamageCalc" init="false"/>
    <bool name="bPreventWaterExplosionVFX" init="false"/>
    <bool name="bIgnoreRatioCheckForFire" init="false"/>
    <bool name="bAllowUnderwaterExplosion" init="false"/>
    <bool name="bForceVehicleExplosion" init="false"/>
    <float name="midRadius" init="-1.0" min="-1.0" max="10000"/>
    <float name="damageAtMid" init="-1.0" min="-1.0" max="10000"/>
    <bool name="bApplyVehicleEMP" init="false"/>
    <bool name="bApplyVehicleSlick" init="false"/>
    <bool name="bApplyVehicleSlowdown" init="false"/>
    <bool name="bApplyVehicleTyrePop" init="false"/>
    <bool name="bApplyVehicleTyrePopToAllTyres" init="false"/>
    <bool name="bForcePetrolTankDamage" init="false"/>
    <bool name="bIsSmokeGrenade" init="false"/>
    <bool name="bForceNonTimedVfx" init="false"/>
    <bool name="bDealsEnduranceDamage" init="false" description="Any damage will be dealt to endurance and not health (while in game modes that support endurance damage)"/>
    <bool name="bApplyFlashGrenadeEffect" init="false"/>
    <bool name="bEnforceDamagedEntityList" init="false" description="Skips the check that ignores the damage entity list on the frame that a ped ragdoll is activating, sometimes causing damage to apply twice"/>
    <bool name="bAlwaysCollideVehicleOccupants" init="false" description="Always add all vehicle occupants to the collision intersection list, not just specific situations like hanging and open seats"/>
  </structdef>

  <structdef type="CExplosionInfoManager">
    <array name="m_aExplosionTagData" type="atArray">
      <struct type="CExplosionTagData"/>
    </array>
  </structdef>
  
  
</ParserSchema>
