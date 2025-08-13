<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
							
  <structdef type="CSlownessZone">
    <struct name="m_bBox" type="rage::spdAABB"/>
  </structdef>
  
  <structdef type="CSlownessZoneManager">
	<array name="m_aSlownessZone" type="atArray">
		<struct type="CSlownessZone"/> 
	</array> 
  </structdef>
</ParserSchema>

