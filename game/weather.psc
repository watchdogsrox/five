<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<enumdef type="eSystemType">
	<enumval name="SYSTEM_TYPE_DROP"/>
	<enumval name="SYSTEM_TYPE_MIST"/>
	<enumval name="SYSTEM_TYPE_GROUND"/>
</enumdef>

<enumdef type="eDriveType">
  <enumval name="DRIVE_TYPE_NONE" value="0"/>
  <enumval name="DRIVE_TYPE_RAIN" value="1"/>
  <enumval name="DRIVE_TYPE_SNOW" value="2"/>
  <enumval name="DRIVE_TYPE_UNDERWATER" value="3"/>
  <enumval name="DRIVE_TYPE_REGION" value="4"/>
</enumdef>

<structdef type="CWeatherGpuFxFromXmlFile">
	<string name="m_Name" type="atString"/>
	<enum name="m_SystemType" type="eSystemType"/>
	<string name="m_diffuseName" type="atString"/>
  <string name="m_distortionTexture" type="atString" init=""/>
	<string name="m_diffuseSplashName" type="atString"/>
	<enum name="m_driveType" type="eDriveType"/>
	<float name="m_windInfluence"/>
	<float name="m_gravity"/>
	<string name="m_emitterSettingsName" type="atString"/>
	<string name="m_renderSettingsName" type="atString"/>
</structdef>

<structdef type="CWeatherTypeFromXmlFile">
	<string name="m_Name" type="atString"/>
	<float name="m_Sun"/>
	<float name="m_Cloud"/>
	<float name="m_WindMin"/>
	<float name="m_WindMax"/>
	<float name="m_Rain"/>
	<float name="m_Snow"/>
  <float name="m_SnowMist"/>
	<float name="m_Fog"/>
	<bool name="m_Lightning"/>
	<bool name="m_Sandstorm"/>
	<float name="m_RippleBumpiness"/>
	<float name="m_RippleMinBumpiness"/>
	<float name="m_RippleMaxBumpiness"/>
	<float name="m_RippleBumpinessWindScale"/>
	<float name="m_RippleSpeed"/>
    <float name="m_RippleVelocityTransfer"/>
    <float name="m_RippleDisturb"/>
    <float name="m_OceanBumpiness"/>
    <float name="m_DeepOceanScale"/>
    <float name="m_OceanNoiseMinAmplitude"/>
    <float name="m_OceanWaveAmplitude"/>
    <float name="m_ShoreWaveAmplitude"/>
    <float name="m_OceanWaveWindScale"/>
    <float name="m_ShoreWaveWindScale"/>
    <float name="m_OceanWaveMinAmplitude"/>
    <float name="m_ShoreWaveMinAmplitude"/>
    <float name="m_OceanWaveMaxAmplitude"/>
    <float name="m_ShoreWaveMaxAmplitude"/>
    <float name="m_OceanFoamIntensity"/>
    <string name="m_DropSettingName" type="atString"/>
	<string name="m_MistSettingName" type="atString"/>
	<string name="m_GroundSettingName" type="atString"/>
	<string name="m_TimeCycleFilename" type="atString"/>
	<string name="m_CloudSettingsName" type="atHashString"/>
</structdef>
<structdef type="CWeatherTypeEntry">
  <string name="m_CycleName" type="atString"/>
  <u32 name="m_TimeMult" init="1"/>
</structdef>
<structdef type="CContentsOfWeatherXmlFile">
	<float name="m_VersionNumber"/>
	<array name="m_WeatherGpuFx" type="atArray">
		<struct type="CWeatherGpuFxFromXmlFile"/>
	</array>
	<array name="m_WeatherTypes" type="atArray">
		<struct type="CWeatherTypeFromXmlFile"/>
	</array>
	<array name="m_WeatherCycles" type="atArray">
		<struct type="CWeatherTypeEntry"/>
	</array>
</structdef>

</ParserSchema>
