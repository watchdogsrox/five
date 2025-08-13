<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

<structdef type="InteriorPosOrientInfo" autoregister="true">
	<string name="name" type="atString"/>
	<string name="archetypeName" type="atString"/>
	<Vector3 name="position"/>
	<float name="heading"/>
	<u32 name="mapsDataSlotIDX"/>
</structdef>

<structdef type="InteriorPosOrientContainer" autoregister="true">
	<array name="info" type="atArray">
		<struct type="InteriorPosOrientInfo"/>
	</array>
</structdef>

<structdef type="SInteriorOrderData" autoregister="true">
  <s32 name="m_startFrom"/>
  <u32 name="m_filePathHash"/>
  <array name="m_proxies" type="atArray">
    <string name="m_proxyName" type="atHashString"/>
  </array>
</structdef>

</ParserSchema>