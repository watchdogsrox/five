<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CMapZoneArea" >
    <array name="m_ZoneAreaHull" type="atArray">
      <Vector3/>
    </array>
  </structdef>    

  <structdef type="CMapZone" >
    <string name="m_Name" type="atString"/>
    <array name="m_ZoneAreas" type="atArray">
      <struct type="CMapZoneArea"/>
    </array>
    <struct name = "m_BoundBox" type = "spdAABB"/>   
  </structdef>

  <structdef type="CMapZonesContainer" >
    <array name="m_Zones" type="atArray">
      <struct type="CMapZone"/>
    </array>
  </structdef>

</ParserSchema>