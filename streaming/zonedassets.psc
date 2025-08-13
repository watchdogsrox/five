<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
generate="class">

  <hinsert>
#include    "spatialdata/aabb.h"
  </hinsert>
  
  <structdef type="CZonedAssets">
    <string name="m_Name" type="atHashString"/>
    <struct name="m_Extents" type="rage::spdAABB"/>
    <array name="m_Models" type="atArray">
      <string type="atHashString"/>
    </array>
	<int name="m_Version" init="0"/>
  </structdef>

</ParserSchema>
