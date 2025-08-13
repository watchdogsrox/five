<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">
		
	<structdef type="CPathZone">
		<Vector2 name="vMin"/>
		<Vector2 name="vMax"/>
		<s32 name="ZoneKey"/>
	</structdef>
	
	<structdef type="CPathZoneMapping">
		<Vector3 name="vDestination"/>
		<s32 name="SrcKey"/>
		<s32 name="DestKey"/>
    <bool name="LoadEntireExtents"/>
	</structdef>
	
	<structdef type="CPathZoneArray">
		<array name="m_Entries" type="atFixedArray" size="4">
			<struct type="CPathZone"/>
		</array>
	</structdef>
	
	<structdef type="CPathZoneMappingArray">
		<array name="m_Entries" type="atFixedArray" size="12">
			<struct type="CPathZoneMapping"/>
		</array>
	</structdef>
	
	<structdef type="CPathZoneData">
		<struct type="CPathZoneArray" name="m_PathZones"/>
		<struct type="CPathZoneMappingArray" name="m_PathZoneMappings"/>
	</structdef>
</ParserSchema>