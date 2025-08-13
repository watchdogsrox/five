<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="MemoryProfileModuleStat">
    <s32 name="m_VirtualMemory"/>
    <s32 name="m_PhysicalMemory"/>
  </structdef>

  <structdef type="MemoryFootprint">
    <map name="m_ModuleMemoryStatsMap" type="atBinaryMap" key="atHashString">
      <struct type="MemoryProfileModuleStat"/>
    </map>
  </structdef>

  <structdef type="MemoryProfileLocation">
		<string name="m_Name" type="atString"/>
		<Vec3V name="m_PlayerPos"/>
		<Mat34V name="m_CameraMtx"/>
    <struct name="m_MemoryFootprint" type="MemoryFootprint" />
	</structdef>

	<structdef type="MemoryProfileLocationList">
		<array name="m_Locations" type="atArray">
			<struct type="MemoryProfileLocation"/>
		</array>
	</structdef>

</ParserSchema>
