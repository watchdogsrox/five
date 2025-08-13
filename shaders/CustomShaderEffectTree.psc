<?xml version="1.0"?>

<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="psoChecks">

  <structdef type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_GLOBALS" layoutHint="novptr" simple="true">
    <float name="m_umLowWind"/>
    <float name="m_umHighWind"/>
    <float name="m_WindVariationRange"/>
    <array name="m_WindVariationScales" type="member" size="3">
      <float />
    </array>
    <array name="m_WindVariationBlendWaveFreq" type="member" size="2">
      <float />
    </array>
    <float name="m_WindSmoothChangeControlPointInterval"/>
    <float name="m_WindSpeedSoftClamp"/>
    <float name="m_WindSpeedSoftClampUnrestrictedProportion"/>
    <float name="m_UnderWaterTransitionRange"/>
    <float name="m_AlphaCardOnlyGlobalStiffness"/>
  </structdef>

  <structdef type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SFX_GLOBALS" layoutHint="novptr" simple="true">
    <Vector3 name="m_SfxWindEvalModulationFreq"/>
    <float name="m_SfxWindEvalModulationDisplacementAmp"/>
    <float name="m_SfxWindValueModulationProportion"/>
  </structdef>

  <structdef type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS::TRIANGLE_WAVE" layoutHint="novptr" simple="true">
    <Vector2 name="freqAndAmp"/>
  </structdef>
  
  <structdef type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS::BASIS_WAVE" layoutHint="novptr" simple="true">
    <struct type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS::TRIANGLE_WAVE" name="lowWind"/>
    <struct type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS::TRIANGLE_WAVE" name="highWind"/>
  </structdef>
 
  <structdef type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS" layoutHint="novptr" simple="true">
    <float name="m_PivotHeight"/>
    <float name="m_TrunkStiffnessAdjustLow"/>
    <float name="m_TrunkStiffnessAdjustHigh"/>
    <float name="m_PhaseStiffnessAdjustLow"/>
    <float name="m_PhaseStiffnessAdjustHigh"/>
    <array name="m_BasisWaves" type="member" size="3">
      <struct type="CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS::BASIS_WAVE" />
    </array>
  </structdef>

</ParserSchema>


