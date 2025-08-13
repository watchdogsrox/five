<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef name="WaterQuad" type="CWaterQuadParsable" autoregister="true">
	<s16 name="minX"/>
	<s16 name="maxX"/>
	<s16 name="minY"/>
	<s16 name="maxY"/>
	<u8 name="m_Type" init="0"/>
	<bool name="m_IsInvisible" init="false"/>
	<bool name="m_HasLimitedDepth" init="false"/>
	<bool name="m_NoStencil" init="false"/>
	<float name="z" init="0.0"/>
	<u8 name="a1" init="26"/>
	<u8 name="a2" init="26"/>
	<u8 name="a3" init="26"/>
	<u8 name="a4" init="26"/>
</structdef>
<structdef name="CalmingQuad" type="CCalmingQuadParsable" autoregister="true">
	<s16 name="minX"/>
	<s16 name="maxX"/>
	<s16 name="minY"/>
	<s16 name="maxY"/>
	<float name="m_fDampening"/>
</structdef>
<structdef name="WaveQuad" type="CWaveQuadParsable" autoregister="true">
	<s16 name="minX"/>
	<s16 name="maxX"/>
	<s16 name="minY"/>
	<s16 name="maxY"/>
	<float name="m_Amplitude"/>
	<float name="m_XDirection"/>
	<float name="m_YDirection"/>
</structdef>
<structdef name="DeepWaveQuad" type="CDeepWaveQuadParsable" autoregister="true">
	<s16 name="minX"/>
	<s16 name="maxX"/>
	<s16 name="minY"/>
	<s16 name="maxY"/>
	<float name="m_Amplitude"/>
</structdef>
<structdef name="FogQuad" type="CFogQuadParsable" autoregister="true">
	<s16 name="minX"/>
	<s16 name="maxX"/>
	<s16 name="minY"/>
	<s16 name="maxY"/>
	<Color32 name="c0"/>
	<Color32 name="c1"/>
	<Color32 name="c2"/>
	<Color32 name="c3"/>
</structdef>
<structdef name="WaterData" type="CWaterData" autoregister="true">
	<array name="m_WaterQuads" type="atArray">
		<struct type="CWaterQuadParsable"/>
	</array>
	<array name="m_CalmingQuads" type="atArray">
		<struct type="CCalmingQuadParsable"/>
	</array>
	<array name="m_WaveQuads" type="atArray">
		<struct type="CWaveQuadParsable"/>
	</array>
</structdef>
<structdef name="SecondaryWaterTune" type="CSecondaryWaterTune" autoregister="true">
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
</structdef>
<structdef name="MainWaterTune" type="CMainWaterTune" autoregister="true">
  <Color32 name="m_WaterColor"/>
  <float name="m_RippleScale"/>
  <float name="m_OceanFoamScale"/>
  <float name="m_SpecularFalloff"/>
  <float name="m_FogPierceIntensity"/>
  <float name="m_RefractionBlend" init="0.5f"/>
  <float name="m_RefractionExponent" init="0.25f"/>
  <float name="m_WaterCycleDepth" init="100.0f"/>
  <float name="m_WaterCycleFade" init="25.0f"/>
  <float name="m_WaterLightningDepth" init="20.0f"/>
  <float name="m_WaterLightningFade" init="5.0f"/>
  <float name="m_DeepWaterModDepth" init="150.0f"/>
  <float name="m_DeepWaterModFade" init="25.0f"/>
  <float name="m_GodRaysLerpStart" init="150.0f"/>
  <float name="m_GodRaysLerpEnd" init="25.0f"/>
  <float name="m_DisturbFoamScale" init="0.1f"/>
  <Vector2 name="m_FogMin"/>
  <Vector2 name="m_FogMax"/>
</structdef>

</ParserSchema>