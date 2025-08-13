<?xml version="1.0"?>
<ParserSchema xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="schemas/parsermetadata.xsd">

<structdef type="hrsSpeedControl::Tune" cond="ENABLE_HORSE">
<float name="m_TopSpeed" min="0" max="20" step="0.01f"/>
<bool name="m_bInstantaneousAccel"/>
<float name="m_ReverseSpeedScale" init="1" min="0" max="10" step="0.01f"/>
<float name="m_WalkSpeedScale" init="1" min="0" max="10" step="0.01f"/>
<float name="m_TrotSpeedScale" init="1" min="0" max="10" step="0.01f"/>
<float name="m_RunSpeedScale" init="1" min="0" max="10" step="0.01f"/>
<float name="m_GallopSpeedScale" init="1" min="0" max="10" step="0.01f"/>
<float name="m_WalkMinSpeedRateScale" init="0.8" min="0" max="10" step="0.01f"/>
<float name="m_TrotMinSpeedRateScale" init="0.8" min="0" max="10" step="0.01f"/>
<float name="m_RunMinSpeedRateScale" init="0.8" min="0" max="10" step="0.01f"/>
<float name="m_GallopMinSpeedRateScale" init="0.8" min="0" max="10" step="0.01f"/>
<float name="m_WalkMaxSpeedRateScale" init="1.2" min="0" max="10" step="0.01f"/>
<float name="m_TrotMaxSpeedRateScale" init="1.2" min="0" max="10" step="0.01f"/>
<float name="m_RunMaxSpeedRateScale" init="1.2" min="0" max="10" step="0.01f"/>
<float name="m_GallopMaxSpeedRateScale" init="1.2" min="0" max="10" step="0.01f"/>
<float name="m_WalkTurnRate" init="45" min="0" max="180" step="0.1f"/>
<float name="m_TrotTurnRate" init="110" min="0" max="180" step="0.1f"/>
<float name="m_RunTurnRate" init="100" min="0" max="180" step="0.1f"/>
<float name="m_GallopTurnRate" init="80" min="0" max="180" step="0.1f"/>
<float name="m_fCollisionDecelRate" min="0" max="100" step="0.01f"/>
<float name="m_fCollisionDecelRateFixed" min="0" max="100" step="0.01f"/>
<float name="m_fDecayDelay" init="2" min="0" max="100" step="0.01f"/>
<float name="m_AimSpeedFactor" init=".75" min="0" max="1" step="0.01"/>
<float name="m_fSpeedGainFactorDelay" init="0.1f" min="0" max="100" step="0.01f"/>
<float name="m_fMaxSlidingSpeed" init="0.5f" min="0" max="1" step="0.01f"/>
<float name="m_fMaintainSpeedDelay" init=".25" min="0" max="5" step="0.01"/>
<float name="m_fMaxMaintainSpeed" init=".75" min="0" max="1" step="0.01"/>
<float name="m_fMinMaintainSpeed" init=".4" min="0" max="1" step="0.01"/>
<float name="m_fMtlWaterMult_Human" init="1" min="0" max="1" step="0.01f"/>
<float name="m_fMtlWaterMult_AI" init="1" min="0" max="1" step="0.01f"/>
<float name="m_fMtlWaterApproachRate" init="1" min="0" max="100" step="0.1f"/>
<float name="m_fMtlOffPathMult_Human" init="1" min="0" max="1" step="0.01f"/>
<float name="m_fMtlOffPathMult_AI" init="1" min="0" max="1" step="0.01f"/>
<float name="m_fMtlOffPathApproachRate" init="1" min="0" max="100" step="0.1f"/>
<float name="m_fMtlRecoveryApproachRate" init="1" min="0" max="100" step="0.1f"/>
<struct name="m_KFByFreshness" type="rage::ptxKeyframe"/>
<struct name="m_KFBySpeed" type="rage::ptxKeyframe"/>
<struct name="m_KFByStickMag" type="rage::ptxKeyframe"/>
<struct name="m_KFByStickY" type="rage::ptxKeyframe"/>
</structdef>

</ParserSchema>