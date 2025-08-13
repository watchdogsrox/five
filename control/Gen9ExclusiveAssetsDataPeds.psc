<?xml version="1.0"?> 
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema" generate="class">

  <hinsert>
    #include "Peds/rendering/PedVariationDS.h"
  </hinsert>

  <structdef type="LocalDrawableIndicesForOnePedComponent">
    <enum name="m_ComponentType" type="ePedVarComp"/>
    <array name="m_LocalDrawableIndices" type="atArray">
      <u32/>
    </array>
  </structdef>

  <structdef type="PedComponentsForOneDLCPack">
    <string name="m_dlcName" type="atHashString"/>
    <array name="m_ComponentArray" type="atArray">
      <struct type="LocalDrawableIndicesForOnePedComponent"/>
    </array>
  </structdef>

  <structdef type="PedModelData">
    <string name="m_PedModelName" type="atHashString"/>
    <array name="m_DLCData" type="atArray">
      <struct type="PedComponentsForOneDLCPack"/>
    </array>
  </structdef>

  <structdef type="Gen9ExclusiveAssetsDataPeds">
    <array name="m_PedData" type="atArray">
      <struct type="PedModelData"/>
    </array>
  </structdef>

</ParserSchema>
