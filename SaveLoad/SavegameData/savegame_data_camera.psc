<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CCameraSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <map name="m_ContextViewModeMap" type="atBinaryMap" key="atHashValue">
      <string type="atHashValue"/>
    </map>
  </structdef>

  <structdef type="CCameraSaveStructure_Migration" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <map name="m_ContextViewModeMap" type="atBinaryMap" key="u32">
      <u32/>
    </map>
  </structdef>

</ParserSchema>
