<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="CFlareFX">
	<float name="fDistFromLight"/>
	<float name="fSize"/>
	<float name="fWidthRotate" init="0.1"/>
 	<float name="fScrollSpeed" init="0.15"/>
	<float name="fDistanceInnerOffset" init="0.0"/>
	<float name="fIntensityScale"/>
	<float name="fIntensityFade" init="0.0"/>
	<float name="m_fAnimorphicScaleFactorU"/>
	<float name="m_fAnimorphicScaleFactorV"/>
  <bool name="m_bAnimorphicBehavesLikeArtefact" init="false"/>
  <bool name="m_bAlignRotationToSun" init="false"/>
  <Color32 name="color"/>
 	<float name="m_fGradientMultiplier" init="1.0"/>
 	<u8 name="nTexture"/>
	<u8 name="nSubGroup"/>
  <float name="m_fPositionOffsetU"/>
  <float name="m_fTextureColorDesaturate"/>
</structdef>

<structdef type="CFlareTypeFX">
	<array name="arrFlareFX" type="atArray">
		<struct type="CFlareFX"/>
	</array>
</structdef>

<structdef type="CLensFlareSettings">
	<array name="m_arrFlareTypeFX" type="member" size="1">
		<struct type="CFlareTypeFX"/>
	</array>
	<float name="m_fSunVisibilityFactor" init="1.0"/>
	<int name="m_iSunVisibilityAlphaClip" init="10"/>
	<float name="m_fSunFogFactor" init="1.0"/>
	<float name="m_fChromaticTexUSize" init="0.25"/>
	<float name="m_fExposureScaleFactor" init="1.0"/>
	<float name="m_fExposureIntensityFactor" init="1.0"/>
	<float name="m_fExposureRotationFactor" init="1.0"/>
	<float name="m_fChromaticFadeFactor" init="1.0"/>
	<float name="m_fArtefactDistanceFadeFactor" init="1.0"/>
	<float name="m_fArtefactRotationFactor" init="1.0"/>
	<float name="m_fArtefactScaleFactor" init="1.0"/>
	<float name="m_fArtefactGradientFactor" init="0.0"/>
	<float name="m_fCoronaDistanceAdditionalSize" init="1.0"/>
  <float name="m_fMinExposureScale" init="0.0"/>
  <float name="m_fMaxExposureScale" init="20.0"/>
  <float name="m_fMinExposureIntensity" init="0.0"/>
  <float name="m_fMaxExposureIntensity" init="20.0"/>
</structdef>

</ParserSchema>
