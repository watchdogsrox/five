<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">


  <structdef type="CVfxRegionGpuPtFxInfo">
    <bool name="m_gpuPtFxEnabled" init="false"/>
    <string name="m_gpuPtFxName" type="atHashString"/>
    <float name="m_gpuPtFxSunThresh" init="0.0f"/>
    <float name="m_gpuPtFxWindThresh" init="0.0f"/>
    <float name="m_gpuPtFxTempThresh" init="-10.0f"/>
    <s32 name="m_gpuPtFxTimeMin" init="0"/>
    <s32 name="m_gpuPtFxTimeMax" init="24"/>
    <float name="m_gpuPtFxInterpRate" init="1.0f"/>
    <float name="m_gpuPtFxMaxLevelNoWind" init="1.0f"/>
    <float name="m_gpuPtFxMaxLevelFullWind" init="1.0f"/>
  </structdef>

  <structdef type="CVfxRegionInfo">
    <bool name="m_windDebrisPtFxEnabled" init="false"/>
    <string name="m_windDebrisPtFxName" type="atHashString"/>
    <array name="m_vfxRegionGpuPtFxDropInfos" type="atArray">
      <struct type="CVfxRegionGpuPtFxInfo"/>
    </array>
    <array name="m_vfxRegionGpuPtFxMistInfos" type="atArray">
      <struct type="CVfxRegionGpuPtFxInfo"/>
    </array>
    <array name="m_vfxRegionGpuPtFxGroundInfos" type="atArray">
      <struct type="CVfxRegionGpuPtFxInfo"/>
    </array>
  </structdef>

  <structdef type="CVfxRegionInfoMgr">
    <map name="m_vfxRegionInfos" type="atBinaryMap" key="atHashString">
      <pointer type="CVfxRegionInfo" policy="owner"/>
    </map>
  </structdef>

</ParserSchema>
