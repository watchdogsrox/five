<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

    <structdef type="CWeaponComponentData">
        <string name="m_Name" type="atHashString" ui_key="true"/>
    </structdef>

    <structdef type="CWeaponComponentReloadData" base="CWeaponComponentData">
        <string name="m_PedIdleReloadClipId" type="atHashString"/>
        <string name="m_PedIdleReloadEmptyClipId" type="atHashString"/>
        <string name="m_PedAimReloadClipId" type="atHashString"/>
        <string name="m_PedAimReloadEmptyClipId" type="atHashString"/>
        <string name="m_PedLowLeftCoverReloadClipId" type="atHashString"/>
        <string name="m_PedLowRightCoverReloadClipId" type="atHashString"/>
        <string name="m_PedLowCoverReloadEmptyClipId" type="atHashString"/>
        <string name="m_WeaponIdleReloadClipId" type="atHashString"/>
        <string name="m_WeaponIdleReloadEmptyClipId" type="atHashString"/>
        <string name="m_WeaponAimReloadClipId" type="atHashString"/>
        <string name="m_WeaponAimReloadEmptyClipId" type="atHashString"/>
        <string name="m_WeaponLowLeftCoverReloadClipId" type="atHashString"/>
        <string name="m_WeaponLowRightCoverReloadClipId" type="atHashString"/>
        <string name="m_WeaponLowCoverReloadEmptyClipId" type="atHashString"/>
        <float  name="m_AnimRateModifier" init="1.0f" min="0.5f" max="1.5f"/>
    </structdef>

		<structdef type="CWeaponSwapData" base="CWeaponComponentData">
				<string name="m_PedHolsterClipId" type="atHashString"/>
				<string name="m_PedHolsterCrouchClipId" type="atHashString"/>
				<string name="m_PedHolsterCoverClipId" type="atHashString"/>
				<string name="m_PedHolsterDiscardClipId" type="atHashString"/>
				<string name="m_PedHolsterCrouchDiscardClipId" type="atHashString"/>
        <string name="m_PedHolsterWeaponClipId" type="atHashString"/>
				<string name="m_PedUnHolsterClipId" type="atHashString"/>
				<string name="m_PedUnHolsterCrouchClipId" type="atHashString"/>
				<string name="m_PedUnHolsterLeftCoverClipId" type="atHashString"/>
				<string name="m_PedUnHolsterRightCoverClipId" type="atHashString"/>
        <string name="m_PedUnHolsterWeaponClipId" type="atHashString"/>
        <float name="m_AnimPlaybackRate" init="-1.0f" min="-1.0f" max="5.0f"/>
		</structdef>

	<const name="CWeaponComponentReloadLoopedData::MaxLoopSections" value="3"/>
		
	<structdef type="CWeaponComponentReloadLoopedData" base="CWeaponComponentData">
		<array name="m_Sections" type="member" size="CWeaponComponentReloadLoopedData::MaxLoopSections">
			<struct type="CWeaponComponentReloadData"/>
		</array>
	</structdef>

    <structdef type="CWeaponComponentInfo">
        <string name="m_Name" type="atHashString" ui_key="true"/>
        <string name="m_Model" type="atHashString"/>
        <string name="m_LocName" type="atFinalHashString" init="WCT_INVALID"/>
		<string name="m_LocDesc" type="atFinalHashString" init="WCD_INVALID"/>
      <struct name="m_AttachBone" type="CWeaponBoneId"/>
      <pointer name="m_AccuracyModifier" type="CWeaponAccuracyModifier" policy="owner"/>
      <pointer name="m_DamageModifier" type="CWeaponDamageModifier" policy="owner"/>
			<pointer name="m_FallOffModifier" type="CWeaponFallOffModifier" policy="owner"/>
      <bool name="m_bShownOnWheel" init="true"/>
      <bool name="m_CreateObject" init="true"/>
      <bool name="m_ApplyWeaponTint" init="false"/>
      <s8 name="m_HudDamage"	  init="0"/>
      <s8 name="m_HudSpeed"	  init="0"/>
      <s8 name="m_HudCapacity"  init="0"/>
      <s8 name="m_HudAccuracy"  init="0"/>
      <s8 name="m_HudRange"     init="0"/>
    </structdef>

    <structdef type="CWeaponComponentClipInfo" base="CWeaponComponentInfo">
        <s32 name="m_ClipSize"/>
        <pointer name="m_ReloadData" type="CWeaponComponentData" policy="external_named" toString="CWeaponComponentManager::GetNameFromData" fromString="CWeaponComponentManager::GetDataFromName"/>
        <string name="m_AmmoInfo" type="atHashString"/>
        <string name="m_TracerFx" type="atHashString"/>
        <s32 name="m_TracerFxSpacing" init="-1"/>
        <s32 name="m_TracerFxForceLast" init="-1"/>
        <string name="m_ShellFx" type="atHashString"/>
        <string name="m_ShellFxFP" type="atHashString"/>
        <u32 name="m_BulletsInBatch" init="0"/>
    </structdef>

    <structdef type="CWeaponComponentFlashLightInfo" base="CWeaponComponentInfo">
        <float name="m_MainLightIntensity"/>
        <Color32 name="m_MainLightColor"/>
        <float name="m_MainLightRange"/>
        <float name="m_MainLightFalloffExponent"/>
        <float name="m_MainLightInnerAngle"/>
        <float name="m_MainLightOuterAngle"/>
        <float name="m_MainLightCoronaIntensity"/>
        <float name="m_MainLightCoronaSize"/>
        <float name="m_MainLightVolumeIntensity"/>
        <float name="m_MainLightVolumeSize"/>
        <float name="m_MainLightVolumeExponent"/>
        <Color32 name="m_MainLightVolumeOuterColor"/>
        <float name="m_MainLightShadowFadeDistance"/>
        <float name="m_MainLightSpecularFadeDistance"/>
        <float name="m_SecondaryLightIntensity"/>
        <Color32 name="m_SecondaryLightColor"/>
        <float name="m_SecondaryLightRange"/>
        <float name="m_SecondaryLightFalloffExponent"/>
        <float name="m_SecondaryLightInnerAngle"/>
        <float name="m_SecondaryLightOuterAngle"/>
        <float name="m_SecondaryLightVolumeIntensity"/>
        <float name="m_SecondaryLightVolumeSize"/>
        <float name="m_SecondaryLightVolumeExponent"/>
        <Color32 name="m_SecondaryLightVolumeOuterColor"/>
        <float name="m_SecondaryLightFadeDistance"/>
        <float name="m_fTargetDistalongAimCamera"/>
        <struct name="m_FlashLightBone" type="CWeaponBoneId"/>
        <struct name="m_FlashLightBoneBulbOn" type="CWeaponBoneId"/>
        <struct name="m_FlashLightBoneBulbOff" type="CWeaponBoneId"/>
      <bool name="m_ToggleWhenAiming" init="false" />
    </structdef>

    <structdef type="CWeaponComponentLaserSightInfo" base="CWeaponComponentInfo">
        <float name="m_CoronaSize"/>
        <float name="m_CoronaIntensity"/>
        <struct name="m_LaserSightBone" type="CWeaponBoneId"/>
    </structdef>

    <structdef type="CWeaponComponentProgrammableTargetingInfo" base="CWeaponComponentInfo">
    </structdef>

	<enumdef type="CWeaponComponentScopeInfo::SpecialScopeType">
		<enumval name="None" value="0"/>
		<enumval name="NightVision"/>
		<enumval name="ThermalVision"/>
	</enumdef>

	<structdef type="CWeaponComponentScopeInfo" base="CWeaponComponentInfo">
        <string name="m_CameraHash" type="atHashString"/>
        <float name="m_RecoilShakeAmplitude" init="1.0f" min="0.0f" max="10.0f"/>
        <float name="m_ExtraZoomFactorForAccurateMode" init="0.0f" min="0.0f" max="10.0f"/>
        <string name="m_ReticuleHash" type="atHashString"/>
			<enum name="m_SpecialScopeType" type="CWeaponComponentScopeInfo::SpecialScopeType"/>
    </structdef>

    <structdef type="CWeaponComponentSuppressorInfo" base="CWeaponComponentInfo">
        <struct name="m_MuzzleBone" type="CWeaponBoneId"/>
        <string name="m_FlashFx" type="atHashString"/>
        <bool name="m_ShouldSilence" init="true"/>
			<float name="m_RecoilShakeAmplitudeModifier" init="1.0f" min="0.1f" max="10.0f"/>
    </structdef>

  <structdef type="CWeaponComponentVariantModelInfo" base="CWeaponComponentInfo">
    <s8 name="m_TintIndexOverride"     init="0"/>
    <array name="m_ExtraComponents" type="atArray">
      <struct type="CWeaponComponentVariantModelInfo::sVariantModelExtraComponents"/>
    </array>
  </structdef>
  
  <structdef type="CWeaponComponentVariantModelInfo::sVariantModelExtraComponents">
    <string name="m_ComponentName" type="atHashString"/>
    <string name="m_ComponentModel" type="atHashString"/>
  </structdef>
  
	<const name="CWeaponComponentGroupInfo::MAX_INFOS" value="2"/>
	
    <structdef type="CWeaponComponentGroupInfo" base="CWeaponComponentInfo">
        <array name="m_Infos" type="atFixedArray" size="CWeaponComponentGroupInfo::MAX_INFOS">
            <pointer type="CWeaponComponentInfo" policy="external_named" toString="CWeaponComponentManager::GetNameFromInfo" fromString="CWeaponComponentManager::GetInfoFromName"/>
        </array>
    </structdef>

	<const name="CWeaponComponentInfo::MAX_STORAGE" value="425"/>
	
    <structdef type="CWeaponComponentInfoBlob">
        <array name="m_Data" type="atArray">
            <pointer type="CWeaponComponentData" policy="owner"/>
        </array>
        <array name="m_Infos" type="atFixedArray" size="CWeaponComponentInfo::MAX_STORAGE">
            <pointer type="CWeaponComponentInfo" policy="owner"/>
        </array>
        <string name="m_InfoBlobName" type="atString" />
    </structdef>

</ParserSchema>


