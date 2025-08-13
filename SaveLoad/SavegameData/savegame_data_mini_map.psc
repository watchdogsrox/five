<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CMiniMapSaveStructure::CPointOfInterestStruct" preserveNames="true">
    <bool name="m_bIsSet"/>
    <Vector2 name="m_vPos"/>
  </structdef>

  <structdef type="CMiniMapSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <array name="m_PointsOfInterestList" type="atArray">
      <struct type="CMiniMapSaveStructure::CPointOfInterestStruct"/>
    </array>
  </structdef>

</ParserSchema>
