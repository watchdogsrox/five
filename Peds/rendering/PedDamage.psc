<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="DamageBlitRotType">
	<enumval name="kNoRotation"/>
	<enumval name="kRandomRotation"/>
	<enumval name="kGravityRotation"/>	
	<enumval name="kAutoDetectRotation"/>	 
</enumdef>

<enumdef type="DamageDecalCoverageType">
  <enumval name="kSkinOnly"/>
  <enumval name="kClothOnly"/>
</enumdef>
  
<enumdef type="ePedDamageZones">
  <enumval name="kDamageZoneTorso"/>
  <enumval name="kDamageZoneHead"/>
  <enumval name="kDamageZoneLeftArm"/>
  <enumval name="kDamageZoneRightArm"/>
  <enumval name="kDamageZoneLeftLeg"/>
  <enumval name="kDamageZoneRightLeg"/>
</enumdef>

<enumdef type="PedTypes">
  <enumval name="HUMAN"/>
  <enumval name="DOG"/>
  <enumval name="DEER"/>
  <enumval name="BOAR"/>
  <enumval name="COYOTE"/>
  <enumval name="MTLION"/>
  <enumval name="PIG"/>
  <enumval name="CHIMP"/>
  <enumval name="COW"/>
  <enumval name="ROTTWEILER"/>
  <enumval name="ELK"/>
  <enumval name="RETRIEVER"/>
  <enumval name="RAT"/>
  <enumval name="BIRD"/>
  <enumval name="FISH"/>
  <enumval name="SMALL_MAMMAL"/>
</enumdef>

 <enumdef type="ePushType">
   <enumval name="kPushRadialU"/>
   <enumval name="kPushSine"/>
   <enumval name="kPushRadialUDress"/>
 </enumdef>

  
  <structdef type="CPedDamageCylinderInfo">
  <Vector3 name="baseOffset" init="0.0f, 0.0f, 0.0f" min="-5.0f" max="5.0f" description="Offset from base in local space"/>
  <Vector3 name="topOffset" init="0.0f, 0.0f, 0.0f" min="-5.0f" max="5.0f" description="Offset from top in local space"/>
  <float name="rotation" init="0.0f" min="-360.0f" max="360.0f" description="Rotation fixup for this cylinder"/>
  <float name="push" init="0.0f" min="-1.0f" max="1.0f" description="push from edge"/>
  <float name="lengthScale" init="1.0f" min="0.0f" max="5.0f" description="scaling the length of the cylinder"/>
	<float name="radius" init="0.3f" min="0.0f" max="5.0f" description="radius of the cylinder"/>
  <enum name="bone1" type="eAnimBoneTag" init="BONETAG_ROOT" description="Base joint #1 (averaged with #2)"/>
  <enum name="bone2" type="eAnimBoneTag" init="BONETAG_ROOT" description="Base joint #2 (averaged with #1)"/>
  <enum name="topBone" type="eAnimBoneTag" init="BONETAG_ROOT" description="Top Joint for other end of the cylinder"/>

  <array name="pushAreas" type="atArray" description="Array of push areas for each type of ped/animal">
    <struct type="CPedDamagePushArea" addGroupWidget="false"/>
  </array>
</structdef> 

<structdef type="CPedDamageCylinderInfoSet">
  <enum name="type" type="PedTypes" init="HUMAN" description="The type of animal"/>
  <struct name="torsoInfo" type="CPedDamageCylinderInfo" description="The torso cylinder"/>
  <struct name="headInfo" type="CPedDamageCylinderInfo" description="The head cylinder"/>
  <struct name="leftArmInfo" type="CPedDamageCylinderInfo" description="The left arm cylinder"/>
  <struct name="rightArmInfo" type="CPedDamageCylinderInfo" description="The right arm cylinder"/>
  <struct name="leftLegInfo" type="CPedDamageCylinderInfo" description="The left leg cylinder"/>
  <struct name="rightLegInfo" type="CPedDamageCylinderInfo" description="The right leg cylinder"/>
</structdef>

 <structdef type="CPedDamagePushArea">
   <enum name="m_Type" type="ePushType" init="kPushRadialU" description="Push type"/>
   <Vector2 name="m_Center" init="0.0f, 0.0f" min="-1.0f" max="1.0f" description="Center of the push area"/>
   <Vector2 name="m_RadiusOrAmpDir" init="0.0f, 1.0f" min="-1.0f" max="1.0f" description="Radius in the U and V, or the sine amplitude and Direction of the V push" />
 </structdef>


<structdef type="CPedDamageTexture">
	<string name="m_TextureName" type="atHashString" init="None" ui_key="true" description="Name of the texture in the specified texture dictionary"/>
	<string name="m_TxdName" type="atFinalHashString" init="None" description="Name of texture dictionary for this texture"/>
	<float name="m_TexGridRows" init="1" min="1" max="32" step="1.0f" description="How many rows of subframes are in the texture grid"/>
	<float name="m_TexGridCols" init="1" min="1" max="32" step="1.0f" description="How many rows of subframes are in the texture grid"/>
	<float name="m_FrameMin" init="0" min="-1" max="32" step="1.0f" description="min frame to use for animation or random selection"/>
	<float name="m_FrameMax" init="0" min="-1" max="32" step="1.0f" description="max frame to use for animation or random selection"/>
	<float name="m_AnimationFrameCount" init="0" min="-1" max="32" step="1.0f" description="The number of frames in the animation (if less than m_FrameMax-m_FrameMin, there are multiple animation sequences)"/>
	<float name="m_AnimationFPS" init="0.0f" min="0" max="30" description="The number of frames per second for animated textures. (0 = no animation)"/>
	<bool name="m_AllowUFlip" init="false" description="can we randomnly flip the texture in the U direction"/>
	<bool name="m_AllowVFlip" init="false" description="can we randomnly flip the texture in the V direction"/>
	<Vector2 name="m_CenterOffset" init="0.0f,0.0f" min="-1" max="1" description="Offset from center of texture used for rotation and scaling(+/-1 range)"/>
</structdef>

<structdef type="CPedBloodDamageInfo" onPreAddWidgets="PreAddWidgets" onPostAddWidgets="PostAddWidgets">
	<string name="m_Name" type="atHashString" ui_key="true" hideWidgets="true" description="The name of the blood damage type"/>
	<struct name="m_SoakTexture" type="CPedDamageTexture" description="The texture info for soak part of this damage type"/>
	<struct name="m_SoakTextureGravity" type="CPedDamageTexture" description="The texture info for soak down part of this damage when usign kGravityRotation (or kAutomaticRotation and ped is statnding)"/>
	<struct name="m_WoundTexture" type="CPedDamageTexture" description="The texture info for wound part of this damagetype"/>
	<struct name="m_SplatterTexture" type="CPedDamageTexture" description="The texture info for splatter part of this damagetype"/>
	
	<enum name="m_RotationType" type="DamageBlitRotType" init="kNoRotation" description="Which type of rotation to use when blitting this texture (kNoRotation, kRandomRotation, or kGravityRotation)"/>
	<bool name="m_SyncSoakWithWound" init="false" description="should the soak and splatter animation sqeuence or frame be matched to the random frame used for the wound"/>

	<float name="m_WoundMinSize" init="1.0f" min="0.0f" max="100.0f" description="Initial size (0..100) of blood wound when the damage is added (wound is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_WoundMaxSize" init="2.0f" min="0.0f" max="100.0f" description="Final size (0..100) of blood wound when it's done growing(wound is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_WoundIntensityWet" init="1.0f" min="0.0f" max="1.0f" description="The intensity of the wound when first created (wound is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_WoundIntensityDry" init="0.75f" min="0.0f" max="1.0f" description="The intensity of the wound when fully dry (wound is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_WoundDryingTime" init="10.0f" min="0.0f" max="100.0f" description="How long the blood wound takes to dry"/>

	<float name="m_SplatterMinSize" init="0.0f" min="0.0f" max="100.0f" description="Initial size (0..100)of blood splatter when the damage is added (spatter is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_SplatterMaxSize" init="0.0f" min="0.0f" max="100.0f" description="Final size( 0..100) of blood splatter when it's done growing(splatter is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_SplatterIntensityWet" init="0.1f" min="0.0f" max="1.0f" description="The intensity of the splatter when first created (wound is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_SplatterIntensityDry" init="0.08f" min="0.0f" max="1.0f" description="The intensity of the splatter when fully dry (wound is blit Red*BloodColor*SplatIntensity, with blue as alpha)"/>
	<float name="m_SplatterDryingTime" init="10.0f" min="0.0f" max="100.0f" description="How long the blood splatter takes to dry"/>

  <float name="m_SoakStartSize" init="0.0f" min="0.0f" max="100.0f" description="Initial size (0..100) of blood soak when the damage is added (soak is blit Green*SoakColor*SoakIntensity)"/>
  <float name="m_SoakEndSize" init="0.0f" min="0.0f" max="100.0f" description="Final size (0..100) of blood soak when it's done growing (soak is blit Green*SoakColor*SoakIntensity)"/>
  <float name="m_SoakStartSizeGravity" init="0.0f" min="0.0f" max="100.0f" description="Initial size (0..100) of blood soak if using the gravity rotation damage (soak is blit Green*SoakColor*SoakIntensity)"/>
  <float name="m_SoakEndSizeGravity" init="0.0f" min="0.0f" max="100.0f" description="Final size (0..100) of blood soak if it's using the gravity rotation damage (soak is blit Green*SoakColor*SoakIntensity)"/>
  <float name="m_SoakScaleTime" init="5.0f" min="0.0f" max="1000.0f" description="Time it takes to grow from initial to final soak size (soak is blit Green*SoakColor*SoakIntensity)"/>
	<float name="m_SoakIntensityWet" init="0.25" min="0" max="1" description="The intensity of the soaked area when first created (soak is blit Green*SoakColor*SoakIntensity)"/>
	<float name="m_SoakIntensityDry" init="0.55f" min="0.0f" max="1.0f" description="The intensity of the soaked area when fully dry (soak is blit Green*SoakColor*SoakIntensity)"/>
	<float name="m_SoakDryingTime" init="5.0f" min="0.0f" max="100.0f" description="How long it takes the soaked area to dry (soak is blit Green*SoakColor*SoakIntensity)"/>

	<float name="m_BloodFadeStartTime" init="400.0f" min="0" max="10000.0f" description="The time (in seconds) after blood is created, when the blood wound starts to fade out"/>
	<float name="m_BloodFadeTime" init="10.0f" min="0.0f" max="1000.0f" description="How long does blood wound take to fade out, once it starts"/>
  <string name="m_ScarName" type="atHashString" description="The name of the scar that will appear at the location of the blood (None = no scar will appear)"/>
	<float name="m_ScarStartTime" init="180.0" min="-1.0f" max="10000.0f" description="When (in seconds) the scar should apear. -1 means no auto wound to scar conversion"/>
</structdef>

<structdef type="CPedDamageDecalInfo" onPreAddWidgets="PreAddWidgets" onPostAddWidgets="PostAddWidgets">
  <string name="m_Name" type="atHashString" init="None" ui_key="true" hideWidgets="true" description="Name of the damage decal"/>
  <struct name="m_Texture" type="CPedDamageTexture" description="Damage texture info for this damage decal"/>
  <struct name="m_BumpTexture" type="CPedDamageTexture" description="Bumpmap texture info for this damage decal"/>
  <float name="m_MinSize" init="50.0f" min="0.0f" max="100.0f" description="The min scale of the damage decal when placed"/>
  <float name="m_MaxSize" init="50.0f" min="0.0f" max="100.0f" description="The max scale of the damage decal when placed"/>
  <float name="m_MinStartAlpha" init="1.0f" min="0.0f" max="1.0f" description="The min starting fade alpha for the damage decal when placed, unless overridden by code/script"/>
  <float name="m_MaxStartAlpha" init="1.0f" min="0.0f" max="1.0f" description="The max starting fade alpha for the damage decal when placed, unless overridden by code/script"/>
  <float name="m_FadeInTime" init="300.0f" min="0.0f" max="1020.0f" description="The time it takes to fade in"/>
  <float name="m_FadeOutTime" init="300.0f" min="0.0f" max="1020.0f" description="The time it takes to fade out"/>
  <float name="m_FadeOutStartTime" init="300.0f" min="-1.0f" max="1020.0f" description="The Time before the damage decal starts to fadeout (-1 disables fading)"/>
  <enum name="m_CoverageType" type="DamageDecalCoverageType" init="kSkinOnly" description="Is the damage decal skin only or cloth only (can't be both)"/>
</structdef>

<structdef type="CPedDamagePackEntry">
	<string name="m_DamageName" type="atHashString" init="None" ui_key="true" hideWidgets="false" onWidgetChanged="DamagePackUpdated" description="Name of the damage damage to apply (could be blood or decoration)"/>
	<enum name="m_Zone" type="ePedDamageZones" init="kDamageZoneTorso" onWidgetChanged="DamagePackUpdated" description="Which Zone to apply this pack entry to"/>
	<Vector2 name="m_uvPos" init="0.0f, 0.0f" min="0.0f" max="1.0f" onWidgetChanged="DamagePackUpdated" description="UV Location for this damage pack entry"/>
	<float name="m_Rotation" init="0.0f" min="0.0f" max="360.0f" onWidgetChanged="DamagePackUpdated" description="Rotation for this damage pack entry"/>
	<float name="m_Scale" init="1.0f" min="0.0f" max="1.0f" onWidgetChanged="DamagePackUpdated" description="Scale for this damage pack entry"/>
	<int name="m_ForcedFrame" init="-1" min="-1" max="16" onWidgetChanged="DamagePackUpdated" description="forced framenumber for the entry (or -1 for random)"/>
</structdef>

<structdef type="CPedDamagePack">
	<string name="m_Name" type="atHashString" init="None" ui_key="true" hideWidgets="false" description="Name of the damage pack"/>
	<array name="m_Entries" type="atArray" description="Array of blood/damage effects for this damage pack">
		<struct type="CPedDamagePackEntry" addGroupWidget="false"/>
	</array>
</structdef>
 
<structdef type="CPedDamageData" onPostLoad="OnPostLoad">
	<Vector3 name="m_BloodColor" init="0.4f,0.0f,0.0f" min="0.0f" max="1.0f" description="The color of the blood wound and splatter (scaled by SplatIntensity and the blit texture's Red with Blue used as an alpha blend). "/>
	<Vector3 name="m_SoakColor" init="0.5f,0.25f,0.0f" min="0.0f" max="1.0f" description="The color of the blood soak (scaled by SoakIntensity and the blit texture's Green)"/>
	
	<float name="m_BloodSpecExponent" init="1.0f" min="0.0f" max="500.0f" description="Blood Specular Exponent value"/>
	<float name="m_BloodSpecIntensity" init="0.125f" min="0.0f" max="1.0f" description="Blood Specular Intensity value"/>
	<float name="m_BloodFresnel" init="0.95f" min="0.0f" max="0.95f" description="Blood Fresnel term"/>
	<float name="m_BloodReflection" init="0.6f" min="0.0f" max="1.0f" description="Blood envmap reflection adjustment value"/>
	<float name="m_WoundBumpAdjust" init="0.4f" min="0.01f" max="4.0f" description="Bump scale adjustment (based on blit textures's Red*Blue"/>
	
	<array name="m_BloodData" type="atArray" description="Array of blood damage effects">
		<pointer type="CPedBloodDamageInfo" policy="owner" addGroupWidget="false"/>
	</array>
	<array name="m_DamageDecalData" type="atArray" description="Array of scar effects">
		<pointer type="CPedDamageDecalInfo" policy="owner" addGroupWidget="false"/>
	</array>
	<array name="m_DamagePacks" type="atArray" description="Array of damage packs">
		<pointer type="CPedDamagePack" policy="owner" addGroupWidget="false"/>
	</array>
  <array name="m_CylinderInfoSets" type="atArray" description="Array of cylinder sets for every type of animal">
    <pointer type="CPedDamageCylinderInfoSet" policy="owner" addGroupWidget="false"/>
  </array>

	<float name="m_HiResTargetDistanceCutoff" init="2.0f" min="0.0f" max="100.0f" description="The distance of the ped from the camera at which we switch from high resolution damage rendertargets to the medium res"/>
	<float name="m_MedResTargetDistanceCutoff" init="5.0f" min="0.0f" max="100.0f" description="The distance of the ped from the camera at which we switch from medium resolution damage rendertargets to the low res"/>
	<float name="m_LowResTargetDistanceCutoff" init="30.0f" min="0.0f" max="100.0f" description="The distance of the ped from the camera at which we stop generating blood for the ped"/>
	<float name="m_CutsceneLODDistanceScale" init="1.0f" min="1.0f" max="4.0f" description="The Multiplier for medium and Hires LOD to use when in cutscenes"/>
	<int name="m_MaxMedResTargetsPerFrame" init="7" min="0" max="kMaxMedResBloodRenderTargets" description="The max number of medium res blood render targets we allow (even if more are close enough)"/>
	<int name="m_MaxTotalTargetsPerFrame" init="16" min="0" max="kMaxLowResBloodRenderTargets" description="The max number of low res blood render targets we allow (even if more are close enough)"/>
	<int name="m_NumWoundsToScarsOnDeathSP" init="1" min="0" max="8" description="The number of wounds that should auto convert to scars on death for singleplayer play"/>
	<int name="m_NumWoundsToScarsOnDeathMP" init="1" min="0" max="8" description="The number of wounds that should auto convert to scars on death for multiplayer play"/>
	<int name="m_MaxPlayerBloodWoundsSP" init="7" min="-1" max="CPedDamageSetBase::kMaxBloodDamageBlits" description="The maximum number of blood damage blits for the player ped in single player games (-1 is no limit)"/>
	<int name="m_MaxPlayerBloodWoundsMP" init="-1" min="-1" max="CPedDamageSetBase::kMaxBloodDamageBlits" description="The maximum number of blood damage blits for the player ped in multiplayer player games (-1 is no limit)"/>
	<Vector4 name="m_TattooTintAdjust" init=".35f,.35f,.35f,1.0f" min="0.0f" max="1.0f" description="RGB is the tattoo tint multiply values, and A is the intesity alpha adust factor."/>
</structdef>

</ParserSchema>