<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="psoChecks">	
				

  <structdef type="CBlockDesc" simple="true">
    <u32 name="m_version"/>
    <u32 name="m_flags"/>
    <string name="m_name" type="atString"/>
    <string name="m_exportedBy" type="atString"/>
    <string name="m_owner" type="atString"/>
    <string name="m_time" type="atString"/>
  </structdef>
  
  <!-- Map data describing archetypes, entities and additional game data for GTA V -->
	<structdef type="CMapData" base="::rage::fwMapData" >

    <array name="m_timeCycleModifiers" type="atArray">
      <struct type="CTimeCycleModifier" />
    </array>
    
		<array name="m_carGenerators" type="atArray">
			<struct type="CCarGen" />
		</array>

    <struct name="m_LODLightsSOA" type="CLODLight"/>
    <struct name="m_DistantLODLightsSOA" type="CDistantLODLight"/>
    <struct name="m_block" type="CBlockDesc"/>
		
	</structdef>
		
</ParserSchema>
	
