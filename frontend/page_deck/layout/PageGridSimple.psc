<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="CPageGridSimple::eType">
    <enumval name="GRID_1x1" value="0" />
    <enumval name="GRID_1x1_ALIGNED" />
    <enumval name="GRID_3x1_INTERLACED_ALIGNED" />
    <enumval name="GRID_3x5" />
    <enumval name="GRID_3x6" />
    <enumval name="GRID_3x7" />
    <enumval name="GRID_3x8" />
    <enumval name="GRID_3x13" />
    <enumval name="GRID_7x7_INTERLACED" />
    <enumval name="GRID_9x1" />     
  </enumdef>
  
  <structdef type="CPageGridSimple" base="CPageGrid">
    <string name="m_defaultSymbol" parName="DefaultSymbol" type="ConstString" />
    <enum name="m_configuration" parName="Config" type="CPageGridSimple::eType" init="0" />
  </structdef>

</ParserSchema>
