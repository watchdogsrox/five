<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="eRagdollType">
	<enumval name="RD_MALE"/>
	<enumval name="RD_FEMALE"/>
	<enumval name="RD_MALE_LARGE"/>
	<enumval name="RD_CUSTOM"/>
</enumdef>

<structdef type="CBaseCapsuleInfo" constructable="false">
	<string name="m_Name" type="atHashString" ui_key="true"/>
	<float name="m_Mass" init="0" min="0" max="1000"/>
  <float name="m_GroundInstanceDamage" init="1.0" min="0" max="1000"/>
  <float name="m_GroundToRootOffset" init="0" min="0" max="2"/>
	<float name="m_ProbeRadius" init="0.1f" min="0" max="2"/>
	<float name="m_ProbeYOffset" init="0.0f" min="-5" max="5"/>	
	<float name="m_MaxNormalZForFlatten" init="-1.0f" min="-1" max="1"/>
	<float name="m_MinNormalZForFlatten" init="1.0f" min="-1" max="1"/>
	<float name="m_MaxNormalZForFlatten_Movable" init="0.5f" min="-1" max="1"/>
  <float name="m_MinHeightForFlatten" init="0.0f" min="0.0f" max="2.0f"/>
  <float name="m_MinHeightForFlattenInteriorOffset" init="-0.05f" min="-2.0f" max="2.0f"/>
  <enum name="m_RagdollType" type="eRagdollType" init="RD_MALE"/>
	<bool name="m_FlattenCollisionNormals" init="false"/>
	<bool name="m_UsesRagdollReactionIfShoved" init="false"/>
	<bool name="m_DiesOnEnteringRagdoll" init="false"/>
  <bool name="m_UseInactiveRagdollCollision" init="false"/>
</structdef>

<structdef type="CBipedCapsuleInfo" base="CBaseCapsuleInfo">
	<float name="m_Radius" init="0.35f" min="0.01" max="2"/>
	<float name="m_RadiusRunning" init="0.45f" min="0.0f" max="2.0f"/>
	<float name="m_RadiusCrouched" init="0.4f" min="0.0f" max="2.0f"/>
	<float name="m_RadiusInCover" init="0.2f" min="0.0f" max="2.0f"/>
	<float name="m_RadiusCrouchedinCover" init="0.4f" min="0.0f" max="2.0f"/>
	<float name="m_RadiusMobilePhone" init="0.54f" min="0.0f" max="2.0f"/>
  <float name="m_RadiusFPSHeadBlocker" init="0.125f" min="0.0f" max="2.0f"/>
  <float name="m_RadiusWeaponBlocked" init="0.50f" min="0.0f" max="2.0f"/>
  <float name="m_RadiusGrowSpeed" init="0.3f" min="0.0f" max="10.0f"/>
	<float name="m_HeadHeight" init="0" min="0" max="2"/>
	<float name="m_CapsuleZOffset" init="0" min="-2" max="2"/>
	<float name="m_KneeHeight" init="0" min="-2" max="2"/>
	<float name="m_HeadHeightCrouched" init="0" min="0" max="2"/>
	<float name="m_CrouchedCapsuleOffsetZ" init="0" min="-2" max="2"/>
	<float name="m_GroundOffsetExtend" init="0" min="0" max="2"/>
  <float name="m_BoatBlockerHeadHeight" init="0" min="0" max="2"/>
  <float name="m_BoatBlockerZOffset" init="0" min="-2" max="2"/>
  <bool name="m_UseLowerLegBound" init="false"/>
  <bool name="m_UseBoatBlocker" init="false"/>
  <bool name="m_UseFPSHeadBlocker" init="false"/>
</structdef>

<structdef type="CQuadrupedCapsuleInfo" base="CBaseCapsuleInfo" onPostLoad="OnPostLoad">
	<float name="m_Radius" init="0.2f" min="0.01" max="5"/>
	<float name="m_Length" init="1.0f" min="0.01" max="5"/>
	<float name="m_ZOffset" init="0.0f" min="-2" max="2"/>
	<float name="m_YOffset" init="0.0f" min="-2" max="2"/>
	<float name="m_ProbeDepthFromRoot" init="0.0f" min="-4" max="4"/>
	<float name="m_BlockerRadius" init="0.2f" min="0.01" max="5"/>
	<float name="m_BlockerLength" init="0.3f" min="0" max="5"/>	
	<float name="m_BlockerYOffset" init="0.0f" min="-5" max="5"/>
	<float name="m_BlockerZOffset" init="-0.15f" min="-5" max="5"/>
	<float name="m_BlockerXRotation" init="0.0f" min="-3.14" max="3.14"/>
	<float name="m_PropBlockerY" init="0.0f" min="-5" max="5"/>
	<float name="m_PropBlockerZ" init="0.0f" min="-5" max="5"/>
	<float name="m_PropBlockerRadius" init="0.2f" min="0.01" max="5"/>
	<float name="m_PropBlockerLength" init="0.0f" min="0" max="5"/>
	<float name="m_NeckBoundRadius" init="0.14f" min="0.01" max="5"/>
	<float name="m_NeckBoundLength" init="0.7f" min="0" max="5"/>	
	<float name="m_NeckBoundRotation" init="0.3854f" min="-3.14" max="3.14"/>
	<float name="m_NeckBoundHeightOffset" init="0.65f" min="0" max="5"/>	
	<float name="m_NeckBoundFwdOffset" init="1.05f" min="0" max="5"/>		
	<bool name="m_PropBlocker" init="false"/>
	<s8 name="m_PropBlockerSlot" init="4" min="3" max="5"/>
	<bool name="m_Blocker" init="false"/>
	<s8 name="m_BlockerSlot" init="3" min="3" max="5"/>
	<bool name="m_NeckBound" init="false"/>
	<s8 name="m_NeckBoundSlot" init="5" min="3" max="5"/>
	<bool name="m_UseHorseMapCollision" init="false"/>
</structdef>

<structdef type="CBirdCapsuleInfo" base="CBipedCapsuleInfo">
</structdef>

<structdef type="CFishCapsuleInfo" base="CBaseCapsuleInfo">
  <float name="m_Radius" init="0.1" min="0" max="10"/>
  <float name="m_Length" init="0.25" min="0" max="50"/>
  <float name="m_ZOffset" init="0" min="-5" max="5"/>
  <float name="m_YOffset" init="0" min="-5" max="5"/>
</structdef>

<structdef type="CPedCapsuleInfoManager">
	<array name="m_aPedCapsule" type="atArray">
		<pointer type="CBaseCapsuleInfo" policy="owner"/>
	</array>
</structdef>

</ParserSchema>
