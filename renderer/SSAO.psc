<?xml version="1.0"?>

<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="psoChecks">

  <structdef type="MR_SSAO_Parameters" layoutHint="novptr" simple="true">
    <Vector2 name="m_Dim"/>
    <float name="m_FallOffRadius"/>
    <float name="m_KernelSizePercentage"/>
    <int name="m_MaxKernel_Lod0"/>
    <int name="m_MaxKernel_Lod1"/>
    <int name="m_LodToApply"/>
    <int name="m_BottomLod"/>
    <int name="m_NormalWeightPower"/>
    <float name="m_DepthWeightPower"/>
    <float name="m_Strength"/>
    <int name="m_KernelType"/>
  </structdef>


  <structdef type="MR_SSAO_ScenePreset" layoutHint="novptr" simple="true">
    <array name="m_Resolutions" type="member" size="3">
      <struct type="MR_SSAO_Parameters" />
    </array>
  </structdef>

  <structdef type="MR_SSAO_AllScenePresets" layoutHint="novptr" simple="true">
    <array name="m_SceneType" type="member" size="2">
      <struct type="MR_SSAO_ScenePreset" />
    </array>
  </structdef>


  <structdef type="QS_SSAO_Parameters" layoutHint="novptr" simple="true">
    <Vector2 name="m_Dim"/>
    <int name="m_BlurPasses"/>
  </structdef>

  <structdef type="QS_SSAO_ScenePreset" layoutHint="novptr" simple="true">
    <array name="m_Resolutions" type="member" size="3">
      <struct type="QS_SSAO_Parameters" />
    </array>
  </structdef>


  <structdef type="HDAO_Parameters" layoutHint="novptr" simple="true">
    <string name="m_Quality" type="atString"/>
    <bool name="m_Alternative"/>
    <string name="m_BlurType" type="atString"/>
    <float name="m_Strength" />
    <float name="m_RejectRadius" />
    <float name="m_AcceptRadius" />
    <float name="m_FadeoutDist" />
    <float name="m_NormalScale" />
    <float name="m_WorldSpace" />
    <float name="m_TargetScale" />
    <float name="m_RadiusScale" />
    <float name="m_BaseWeight" />
  </structdef>

  <structdef type="HDAO_ScenePreset" layoutHint="novptr" simple="true">
    <array name="m_QualityLevels" type="member" size="3">
      <struct type="HDAO_Parameters" />
    </array>
  </structdef>
  

  <structdef type="HDAO2_Parameters" layoutHint="novptr" simple="true">
    <string name="m_Quality" type="atString"/>
    <bool name="m_UseFilter"/>
    <float name="m_Strength" />
    <float name="m_RejectRadius" />
    <float name="m_AcceptRadius" />
    <float name="m_FadeoutDist" />
    <float name="m_NormalScale" />
  </structdef>

  <structdef type="HDAO2_ScenePreset" layoutHint="novptr" simple="true">
    <array name="m_QualityLevels" type="member" size="3">
      <struct type="HDAO2_Parameters" />
    </array>
  </structdef>


  <structdef type="CPQSMix_Parameters" layoutHint="novptr" simple="true">
    <float name="m_CPRelativeStrength"/>
    <float name="m_QSRelativeStrength"/>
    <float name="m_QSFadeInStart"/>
    <float name="m_QSFadeInEnd"/>
    <float name="m_QSRadius"/>
    <float name="m_CPRadius"/>
    <float name="m_CPNormalOffset"/>
  </structdef>

  <structdef type="HBAO_Parameters" layoutHint="novptr" simple="true">
    <float name="m_HBAORelativeStrength"/>
    <float name="m_CPRelativeStrength"/>
    <float name="m_HBAORadius0"/>
    <float name="m_HBAORadius1"/>
    <float name="m_HBAOBlendDistance"/>
    <float name="m_CPRadius"/>
    <float name="m_MaxPixels"/>
    <float name="m_CutoffPixels"/>
    <float name="m_FoliageStrength"/>
    <float name="m_HBAOFalloffExponent"/>
    <float name="m_CPStrengthClose"/>
    <float name="m_CPBlendDistanceMin"/>
    <float name="m_CPBlendDistanceMax"/>
  </structdef>

</ParserSchema>
