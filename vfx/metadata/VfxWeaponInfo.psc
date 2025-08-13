<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="VfxWeaponSpecialMtlPtFxInfo">
	<string name="m_shotPtFxTagName" type="atHashString"/>
	<float name="m_probability" init="1.0f"/>
	<float name="m_scale" init="1.0f"/>
	<Color32 name="m_colTint" />
  </structdef>

  <structdef type="VfxWeaponSpecialMtlExpInfo">
	<string name="m_shotPtExpTagName" type="atHashString"/>
	<float name="m_probability" init="1.0f"/>
	<float name="m_historyCheckDist" init="1.0f"/>
	<float name="m_historyCheckTime" init="1.0f"/>
  </structdef>

  <structdef type="CVfxWeaponInfoMgr">
	<map name="m_vfxWeaponSpecialMtlPtFxInfos" type="atBinaryMap" key="atHashString">
	  <pointer type="VfxWeaponSpecialMtlPtFxInfo" policy="owner"/>
	</map>
	<map name="m_vfxWeaponSpecialMtlExpInfos" type="atBinaryMap" key="atHashString">
	  <pointer type="VfxWeaponSpecialMtlExpInfo" policy="owner"/>
	</map>
  </structdef>

</ParserSchema>
