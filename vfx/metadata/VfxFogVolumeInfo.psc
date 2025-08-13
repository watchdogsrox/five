<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="eFogVolumeLightingType">
    <enumval name="FOGVOLUME_LIGHTING_FOGHDR" value="0"/>
    <enumval name="FOGVOLUME_LIGHTING_DIRECTIONAL" />
    <enumval name="FOGVOLUME_LIGHTING_NONE" />
  </enumdef>

  <structdef type="CVfxFogVolumeInfo">
    <float      name="m_fadeDistStart" />
    <float      name="m_fadeDistEnd" />
    <Vector3    name="m_position" />
    <Vector3    name="m_rotation" />
    <Vector3    name="m_scale" />
    <u8         name="m_colR" />
    <u8         name="m_colG" />
    <u8         name="m_colB" />
    <u8         name="m_colA" />
    <float      name="m_hdrMult" />
    <float      name="m_range" />
    <float      name="m_density" />
    <float      name="m_falloff" />
    <u64        name="m_interiorHash"/>
    <bool       name="m_isUnderwater" init="false"/>    
    <enum       name="m_lightingType" type="eFogVolumeLightingType" init="FOGVOLUME_LIGHTING_NONE"/>
  </structdef>
  
  <structdef type="CVfxFogVolumeInfoMgr">
	<array name="vfxFogVolumeInfos" type="atArray">
	  <pointer type="CVfxFogVolumeInfo" policy="owner"/>
	</array>
  </structdef>

</ParserSchema>
