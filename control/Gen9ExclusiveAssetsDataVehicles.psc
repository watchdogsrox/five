<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">

  <hinsert>
    #include "modelinfo/VehicleModelInfoEnums.h"  // For eVehicleModType
  </hinsert>

  <structdef type="VehicleModVisible">
    <enum name="m_modType" type="eVehicleModType"/>
    <array name="m_modelNames" type="atArray">
      <string type="atHashString"/>
    </array>
  </structdef>

  <structdef type="VehicleModStat">
    <enum name="m_modType" type="eVehicleModType"/>
    <array name="m_indices" type="atArray">
      <u32/>
    </array>
  </structdef>

  <structdef type="VehicleModKitData">
    <string name="m_kitName" type="atHashString"/>
    <array name="m_visibleMods" type="atArray">
      <struct type="VehicleModVisible"/>
    </array>
    <array name="m_statMods" type="atArray">
      <struct type="VehicleModStat"/>
    </array>
  </structdef>

  <structdef type="Gen9ExclusiveAssetsDataVehicles">
    <array name="m_VehicleData" type="atArray">
      <struct type="VehicleModKitData"/>
    </array>
  </structdef>

</ParserSchema>
