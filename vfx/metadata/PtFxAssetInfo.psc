<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CPtFxAssetDependencyInfo">
	<string name="m_parentName" type="atHashString" ui_key="true"/>
	<string name="m_childName" type="atHashString"/>
  </structdef>
  
  <structdef type="CPtFxAssetInfoMgr">
	<array name="m_ptfxAssetDependencyInfos" type="atArray">
	  <pointer type="CPtFxAssetDependencyInfo" policy="owner"/>
	</array>
  </structdef>

</ParserSchema>
