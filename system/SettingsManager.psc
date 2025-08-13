<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <autoregister allInFile="true"/>

  <enumdef type="eSettingsManagerConfig" preserveNames="true">
    <enumval name="SMC_AUTO"/>
    <enumval name="SMC_DEMO"/>
    <enumval name="SMC_FIXED"/>
    <enumval name="SMC_SAFE"/>
    <enumval name="SMC_MEDIUM"/>
    <enumval name="SMC_UNKNOWN"/>
  </enumdef>

    <structdef type="CSettings">
	</structdef>

  <enumdef type="CSettings::eSettingsLevel" preserveNames="true">
    <enumval name="Low"/>
    <enumval name="Medium"/>
    <enumval name="High"/>
    <enumval name="Ultra"/>
    <enumval name="Custom"/>
    <enumval name="eSettingsCount"/>
  </enumdef>
	
	<structdef type="CGraphicsSettings" base="CSettings" preserveNames="true">
    <int   name="m_Tessellation" />
    <float name="m_LodScale" />
    <float name="m_PedLodBias" />
    <float name="m_VehicleLodBias" />
    <int   name="m_ShadowQuality" />
    <int   name="m_ReflectionQuality" />
    <int   name="m_ReflectionMSAA" />

    <int  name="m_SSAO" />
    <int   name="m_AnisotropicFiltering" />
    <int   name="m_MSAA" />
    <int   name="m_MSAAFragments" />
    <int   name="m_MSAAQuality" />
    <int   name="m_SamplingMode" />

    <int   name="m_TextureQuality" />
    <int   name="m_ParticleQuality" />
    <int   name="m_WaterQuality" />
    <int   name="m_GrassQuality" />

    <int   name="m_ShaderQuality" />

    <int   name="m_Shadow_SoftShadows" />
    <bool  name="m_UltraShadows_Enabled" />
    <bool  name="m_Shadow_ParticleShadows" />
    <float name="m_Shadow_Distance" />
    <bool  name="m_Shadow_LongShadows" />
    <float name="m_Shadow_SplitZStart" />
    <float name="m_Shadow_SplitZEnd" />
    <float name="m_Shadow_aircraftExpWeight" />
    <bool  name="m_Shadow_DisableScreenSizeCheck" />
    <bool  name="m_Reflection_MipBlur" />
    <bool  name="m_FXAA_Enabled" />
    <bool  name="m_TXAA_Enabled" />
    <bool  name="m_Lighting_FogVolumes" />
    <bool  name="m_Shader_SSA" />
    <int   name="m_DX_Version" />
    <float name="m_CityDensity" />
    <float name="m_PedVarietyMultiplier" />
    <float name="m_VehicleVarietyMultiplier"/>
    <int   name="m_PostFX" />
    <bool  name="m_DoF" />
    <bool  name="m_HdStreamingInFlight"/>
    <float name="m_MaxLodScale"/>
    <float name="m_MotionBlurStrength"/>
  </structdef>
	
	<structdef type="CSystemSettings" base="CSettings" preserveNames="true">
    <int name="m_numBytesPerReplayBlock" />
    <int name="m_numReplayBlocks" />
    <int name="m_maxSizeOfStreamingReplay" />
    <int name="m_maxFileStoreSize" />
	</structdef>
	
	<structdef type="CAudioSettings" base="CSettings" preserveNames="true">
		<bool name="m_Audio3d" />
	</structdef>

  <structdef type="CVideoSettings" base="CSettings" preserveNames="true">
    <int	name="m_AdapterIndex" />
    <int	name="m_OutputIndex" />
    <int	name="m_ScreenWidth" />
    <int	name="m_ScreenHeight" />
    <int	name="m_RefreshRate" />
	<int	name="m_Windowed" />
	<int	name="m_VSync" />
	<int	name="m_Stereo" />
	<float	name="m_Convergence" />
	<float	name="m_Separation" />
	<int	name="m_PauseOnFocusLoss" />
	<int	name="m_AspectRatio" />
  </structdef>

  <structdef type="Settings" preserveNames="true">
    <int name="m_version" />
    <enum name="m_configSource" type="eSettingsManagerConfig" />
    <struct name="m_graphics" type="CGraphicsSettings" />
    <struct name="m_system" type="CSystemSettings" />
    <struct name="m_audio" type="CAudioSettings" />
    <struct name="m_video" type="CVideoSettings" />
    <string name="m_VideoCardDescription" type="atString"/>
  </structdef>
  
  <structdef type="CSettingsManager" preserveNames="true">
    <struct type="Settings" name="m_settings" />    
  </structdef>
  
</ParserSchema>
