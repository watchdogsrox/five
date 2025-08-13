<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CPosixTimeStampForMultiplayerSaves" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <u32 name="m_PosixHigh"/>
    <u32 name="m_PosixLow"/>
  </structdef>

</ParserSchema>
