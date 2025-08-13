<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef name="VfxLightningKeyFrames" type="::rage::VfxLightningKeyFrames">
	<struct name="m_LightningMainIntensity" type="rage::ptxKeyframe"/>
	<struct name="m_LightningBranchIntensity" type="rage::ptxKeyframe"/>
</structdef>

<structdef name="VfxLightningDirectionalBurstSettings" type="::rage::VfxLightningDirectionalBurstSettings">
	<float name="m_BurstIntensityMin"/>	
	<float name="m_BurstIntensityMax"/>
	<float name="m_BurstDurationMin"/>
	<float name="m_BurstDurationMax"/>
	<int name="m_BurstSeqCount"/>
	<float name="m_RootOrbitXVariance"/>
	<float name="m_RootOrbitYVarianceMin"/>
	<float name="m_RootOrbitYVarianceMax"/>
	<float name="m_BurstSeqOrbitXVariance"/>
	<float name="m_BurstSeqOrbitYVariance"/>
</structdef>


<structdef name="VfxLightningCloudBurstCommonSettings" type="::rage::VfxLightningCloudBurstCommonSettings">
	<float name="m_BurstDurationMin"/>
	<float name="m_BurstDurationMax"/>
	<int name="m_BurstSeqCount"/>
	<float name="m_RootOrbitXVariance"/>
	<float name="m_RootOrbitYVarianceMin"/>
	<float name="m_RootOrbitYVarianceMax"/>
	<float name="m_LocalOrbitXVariance"/>
	<float name="m_LocalOrbitYVariance"/>
	<float name="m_BurstSeqOrbitXVariance"/>
	<float name="m_BurstSeqOrbitYVariance"/>
	<float name="m_DelayMin"/>	
	<float name="m_DelayMax"/>
	<float name="m_SizeMin"/>
	<float name="m_SizeMax"/>
</structdef>

<structdef name="VfxLightningCloudBurstSettings" type="::rage::VfxLightningCloudBurstSettings">
	<float name="m_BurstIntensityMin"/>	
	<float name="m_BurstIntensityMax"/>
	<float name="m_LightSourceExponentFalloff"/>
	<float name="m_LightDeltaPos"/>
	<float name="m_LightDistance"/>	
	<Color32 name="m_LightColor"/>
	<struct name="m_CloudBurstCommonSettings" type="rage::VfxLightningCloudBurstCommonSettings"/>
</structdef>

<structdef name="VfxLightningStrikeSplitPointSettings" type="::rage::VfxLightningStrikeSplitPointSettings">
	<float name="m_FractionMin"/>	
	<float name="m_FractionMax"/>
	<float name="m_DeviationDecay"/>
	<float name="m_DeviationRightVariance"/>
</structdef>

<structdef name="VfxLightningStrikeForkPointSettings" type="::rage::VfxLightningStrikeForkPointSettings">
	<float name="m_DeviationRightVariance"/>	
	<float name="m_DeviationForwardMin"/>
	<float name="m_DeviationForwardMax"/>
	<float name="m_LengthMin"/>
	<float name="m_LengthMax"/>
	<float name="m_LengthDecay"/>
</structdef>

<structdef name="VfxLightningStrikeVariationSettings" type="::rage::VfxLightningStrikeVariationSettings">
	<float name="m_DurationMin"/>
	<float name="m_DurationMax"/>
	<float name="m_AnimationTimeMin"/>
	<float name="m_AnimationTimeMax"/>
	<float name="m_EndPointOffsetXVariance"/>
	<u8 name="m_SubdivisionPatternMask"/>
	<struct name="m_ZigZagSplitPoint" type="rage::VfxLightningStrikeSplitPointSettings"/>
	<struct name="m_ForkZigZagSplitPoint" type="rage::VfxLightningStrikeSplitPointSettings"/>
	<struct name="m_ForkPoint" type="rage::VfxLightningStrikeForkPointSettings"/>
	<struct name="m_KeyFrameData" type="::rage::VfxLightningKeyFrames" noInit="true"/>	
</structdef>

<structdef name="VfxLightningStrikeSettings" type="::rage::VfxLightningStrikeSettings">
	<float name="m_IntensityMin"/>	
	<float name="m_IntensityMax"/>
	<float name="m_IntensityMinClamp"/>
	<float name="m_WidthMin"/>
	<float name="m_WidthMax"/>
	<float name="m_WidthMinClamp"/>
	<float name="m_IntensityFlickerMin"/>
	<float name="m_IntensityFlickerMax"/>
  <float name="m_IntensityFlickerFrequency"/>
	<float name="m_IntensityLevelDecayMin"/>
	<float name="m_IntensityLevelDecayMax"/>
	<float name="m_WidthLevelDecayMin"/>
	<float name="m_WidthLevelDecayMax"/>
  <float name="m_NoiseTexScale"/>
  <float name="m_NoiseAmplitude"/>
  <float name="m_NoiseAmpMinDistLerp"/>
  <float name="m_NoiseAmpMaxDistLerp"/>
  <u8 name="m_SubdivisionCount"/>
	<float name="m_OrbitDirXVariance"/>	
	<float name="m_OrbitDirYVarianceMin"/>
	<float name="m_OrbitDirYVarianceMax"/>
	<float name="m_MaxDistanceForExposureReset"/>
	<float name="m_AngularWidthFactor"/>
	<float name="m_MaxHeightForStrike"/>
	<float name="m_CoronaIntensityMult"/>
	<float name="m_BaseCoronaSize"/>
	<Color32 name="m_BaseColor"/>
	<Color32 name="m_FogColor"/>
	<float name="m_CloudLightIntensityMult"/>
	<float name="m_CloudLightDeltaPos"/>
	<float name="m_CloudLightRadius"/>
	<float name="m_CloudLightFallOffExponent"/>
	<float name="m_MaxDistanceForBurst"/>
	<float name="m_BurstThresholdIntensity"/>	
	<float name="m_LightningFadeWidth"/>	
	<float name="m_LightningFadeWidthFalloffExp"/>
	<array name="m_variations" type="atRangeArray" size="3">
		<struct type="::rage::VfxLightningStrikeVariationSettings" noInit="true"/>
	</array>	
	<struct name="m_CloudBurstCommonSettings" type="rage::VfxLightningCloudBurstCommonSettings"/>
</structdef>

<structdef name="VfxLightningTimeCycleSettings" type="::rage::VfxLightningTimeCycleSettings">
    <string name="m_tcLightningDirectionalBurst" type="atHashString"/>
    <string name="m_tcLightningCloudBurst" type="atHashString"/>
    <string name="m_tcLightningStrikeOnly" type="atHashString"/>
    <string name="m_tcLightningDirectionalBurstWithStrike" type="atHashString"/>
</structdef>

<structdef name="VfxLightningSettings" type="rage::VfxLightningSettings" onPostLoad="PostLoad">
	<int name="m_lightningOccurranceChance"/>	
	<float name="m_lightningShakeIntensity"/>
	<struct name="m_lightningTimeCycleMods" type="rage::VfxLightningTimeCycleSettings"/>
	<struct name="m_DirectionalBurstSettings" type="rage::VfxLightningDirectionalBurstSettings"/>
	<struct name="m_CloudBurstSettings" type="rage::VfxLightningCloudBurstSettings"/>
	<struct name="m_StrikeSettings" type="rage::VfxLightningStrikeSettings"/>
</structdef>

</ParserSchema>