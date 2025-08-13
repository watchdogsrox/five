<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <const name="g_MaxRadioNameLength" value="255"/>

  <structdef type="CRadioStationSaveStructure::CRadioStationStruct" preserveNames="true">
    <float name="m_ListenTimer"/>
    <string name="m_RadioStationName" type="member" size="g_MaxRadioNameLength"/>
    <u8 name="m_IdentsHistoryWriteIndex"/>
    <array name="m_IdentsHistorySpace" type="atArray">
      <u32/>
    </array>
    <u8 name="m_MusicHistoryWriteIndex"/>
    <array name="m_MusicHistorySpace" type="atArray">
      <u32/>
    </array>
    <u8 name="m_DjSoloHistoryWriteIndex"/>
    <array name="m_DjSoloHistorySpace" type="atArray">
      <u32/>
    </array>
    <u8 name="m_DjSpeechHistoryWriteIndex"/>
    <array name="m_DjSpeechHistory" type="atArray">
      <u32/>
    </array>
  </structdef>

  <structdef type="CRadioStationSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <u8 name="m_NewsReportHistoryWriteIndex"/>
    <array name="m_NewsReportHistorySpace" type="atArray">
      <u32/>
    </array>
    <u8 name="m_WeatherReportHistoryWriteIndex"/>
    <array name="m_WeatherReportHistorySpace" type="atArray">
      <u32/>
    </array>
    <u8 name="m_GenericAdvertHistoryWriteIndex"/>
    <array name="m_GenericAdvertHistorySpace" type="atArray">
      <u32/>
    </array>
    <array name="m_NewsStoryState" type="atArray">
      <u8/>
    </array>

    <array name="m_RadioStation" type="atArray">
      <struct type="CRadioStationSaveStructure::CRadioStationStruct"/>
    </array>
  </structdef>

</ParserSchema>
