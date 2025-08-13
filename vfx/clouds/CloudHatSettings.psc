<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef name="CloudHatSettings" type="rage::CloudHatSettings" onPostLoad="PostLoad" onPreSave="PreSave">
	<struct name="m_CloudList" type="rage::CloudListController"/>
  <struct name="m_CloudColor" type="rage::ptxKeyframe"/>
  <struct name="m_CloudLightColor" type="rage::ptxKeyframe"/>
  <struct name="m_CloudAmbientColor" type="rage::ptxKeyframe"/>
	<struct name="m_CloudSkyColor" type="rage::ptxKeyframe"/>
	<struct name="m_CloudBounceColor" type="rage::ptxKeyframe"/>
	<struct name="m_CloudEastColor" type="rage::ptxKeyframe"/>
	<struct name="m_CloudWestColor" type="rage::ptxKeyframe"/>
	<struct name="m_CloudScaleFillColors" type="rage::ptxKeyframe"/>
	<struct name="m_CloudDensityShift_Scale_ScatteringConst_Scale" type="rage::ptxKeyframe"/>
	<struct name="m_CloudPiercingLightPower_Strength_NormalStrength_Thickness" type="rage::ptxKeyframe"/>
	<struct name="m_CloudScaleDiffuseFillAmbient_WrapAmount" type="rage::ptxKeyframe"/>
</structdef>

</ParserSchema>