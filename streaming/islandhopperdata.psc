<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

	<structdef type="CIslandHopperData" simple="true">
    <string name="Name" type="atHashString"/>
    <string name="Cullbox" type="atString"/>
    <string name="HeightMap" type="atString"/>
    <string name="LodLights" type="atString"/>
    <array name="IPLsToEnable" type="atArray">
      <string type="atHashString"/>
    </array>
    <array name="IMAPsToPreempt" type="atArray">
      <string type="atHashString"/>
    </array>
	</structdef>
  
</ParserSchema>