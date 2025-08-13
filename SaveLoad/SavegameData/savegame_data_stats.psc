<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CBaseStatsSaveStructure::CIntStatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <int name="m_Data"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CFloatStatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <float name="m_Data"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CBoolStatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <bool name="m_Data"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CStringStatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <string name="m_Data" type="atString" />
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CUInt8StatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <u8 name="m_Data"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CUInt16StatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <u16 name="m_Data"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CUInt32StatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <u32 name="m_Data"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CUInt64StatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <u32 name="m_DataHigh"/>
    <u32 name="m_DataLow"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure::CInt64StatStruct" preserveNames="true">
    <int name="m_NameHash"/>
    <u32 name="m_DataHigh"/>
    <u32 name="m_DataLow"/>
  </structdef>

  <structdef type="CBaseStatsSaveStructure" preserveNames="true">
    <array name="m_IntData" type="atArray">
      <struct type="CBaseStatsSaveStructure::CIntStatStruct"/>
    </array>
    <array name="m_FloatData" type="atArray">
      <struct type="CBaseStatsSaveStructure::CFloatStatStruct"/>
    </array>
    <array name="m_BoolData" type="atArray">
      <struct type="CBaseStatsSaveStructure::CBoolStatStruct"/>
    </array>
    <array name="m_StringData" type="atArray">
      <struct type="CBaseStatsSaveStructure::CStringStatStruct"/>
    </array>
    <array name="m_UInt8Data" type="atArray">
      <struct type="CBaseStatsSaveStructure::CUInt8StatStruct"/>
    </array>
    <array name="m_UInt16Data" type="atArray">
      <struct type="CBaseStatsSaveStructure::CUInt16StatStruct"/>
    </array>
    <array name="m_UInt32Data" type="atArray">
      <struct type="CBaseStatsSaveStructure::CUInt32StatStruct"/>
    </array>
    <array name="m_UInt64Data" type="atArray">
      <struct type="CBaseStatsSaveStructure::CUInt64StatStruct"/>
    </array>
    <array name="m_Int64Data" type="atArray">
      <struct type="CBaseStatsSaveStructure::CInt64StatStruct"/>
    </array>
  </structdef>

  <structdef type="CStatsVehicleUsage" preserveNames="true">
    <u32     name="m_VehicleId"/>
    <u32     name="m_CharacterId"/>
    <u32     name="m_TimeDriven"/>
    <float   name="m_DistDriven"/>
    <float   name="m_LastDistDriven"/>
    <u32     name="m_NumDriven"/>
    <u32     name="m_NumStolen"/>
    <u32     name="m_NumSpins"/>
    <u32     name="m_NumFlips"/>
    <u32     name="m_NumPlaneLandings"/>
    <u32     name="m_NumWheelies"/>
    <u32     name="m_NumAirLaunches"/>
    <u32     name="m_NumAirLaunchesOver5s"/>
    <u32     name="m_NumAirLaunchesOver5m"/>
    <u32     name="m_NumAirLaunchesOver40m"/>
    <u32     name="m_NumLargeAccidents"/>
    <u32     name="m_TimeSpentInVehicle"/>
    <float   name="m_HighestSpeed"/>
    <u32     name="m_LongestWheelieTime"/>
    <float   name="m_LongestWheelieDist"/>
    <float   name="m_HighestJumpDistance"/>
    <u32     name="m_LastTimeSpentInVehicle"/>
    <u32     name="m_NumPedsRundown"/>
    <bool    name="m_Online"/>
  </structdef>

  <structdef type="CStatsSaveStructure" base="CBaseStatsSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <array name="m_SpVehiclesDriven" type="atArray">
      <u32/>
    </array>
    <map name="m_PedsKilledOfThisType" type="atBinaryMap" key="atHashString">
      <int/>
    </map>
    <map name="m_aStationPlayTime" type="atBinaryMap" key="atHashString">
      <float/>
    </map>
    <u32 name="m_currCountAsFacebookDriven"/>
    <array name="m_vehicleRecords" type="atFixedArray" size="20">
      <struct type="CStatsVehicleUsage"/>
    </array>
  </structdef>

  <structdef type="CMultiplayerStatsSaveStructure" base="CBaseStatsSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <array name="m_MpVehiclesDriven" type="atArray">
      <u32/>
    </array>
  </structdef>



  <structdef type="CStatsSaveStructure_Migration" base="CBaseStatsSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <array name="m_SpVehiclesDriven" type="atArray">
      <u32/>
    </array>
    <map name="m_PedsKilledOfThisType" type="atBinaryMap" key="u32">
      <int/>
    </map>
    <map name="m_aStationPlayTime" type="atBinaryMap" key="u32">
      <float/>
    </map>
    <u32 name="m_currCountAsFacebookDriven"/>
    <array name="m_vehicleRecords" type="atFixedArray" size="20">
      <struct type="CStatsVehicleUsage"/>
    </array>
  </structdef>

</ParserSchema>
