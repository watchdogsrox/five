<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <enumdef type="netCatalogBaseItem::StorageType">
    <enumval name="INT"/>
    <enumval name="INT64"/>
    <enumval name="BITFIELD"/>
  </enumdef>

  <structdef type="netCatalogBaseItem" constructable="false" cond="__BANK">
    <int name="m_keyhash" />
    <string name="m_category" type="atHashString" />
    <int name="m_price" />

    <string name="m_name" type="atString"/>
    <string name="m_textTag" type="atHashString" />
    <array name="m_openitems" type="atArray" >
      <string type="atHashString" />
    </array>
    <array name="m_deleteitems" type="atArray" >
      <string type="atHashString" />
    </array>
  </structdef>

  <structdef type="netCatalogBaseItem" constructable="false" cond="!__BANK">
    <int name="m_keyhash" />
    <string name="m_category" type="atHashString" />
    <int name="m_price" />
  </structdef>

 <structdef type="netCatalogPackedStatInventoryItem" base="netCatalogBaseItem" cond="!__FINAL" onPostLoad="PostLoad">
    <int name="m_statEnumValue" init="0"/>
    <bool name="m_isPlayerStat" init="false" />
    <string name="m_statName" type="atString"/>
    <u8 name="m_bitShift" init="0" />
    <u8 name="m_bitSize" init="0"/>
  </structdef>
  <structdef type="netCatalogPackedStatInventoryItem" base="netCatalogBaseItem" cond="__FINAL">
    <int name="m_statEnumValue" init="0"/>
  </structdef>

  <structdef type="netCatalogServiceItem" base="netCatalogBaseItem">
  </structdef>
  
  <structdef type="netCatalogServiceWithThresholdItem" base="netCatalogBaseItem">
  </structdef>

  <structdef type="netCatalogServiceLimitedItem" base="netCatalogBaseItem">
  </structdef>

  <structdef type="netCatalogInventoryItem" base="netCatalogBaseItem">
    <string name="m_statName" type="atString" />
    <bool name="m_isPlayerStat" init="false" />
    </structdef>

  <structdef type="netCatalogOnlyItem" base="netCatalogBaseItem">
  </structdef>

  <structdef type="netCatalogOnlyItemWithStat" base="netCatalogBaseItem">
    <int name="m_statValue" init="0"/>
  </structdef>

  <structdef type="CacheCatalogParsableInfo">
    <array name="m_Items1" type="atArray" >
      <pointer type="netCatalogBaseItem" policy="owner"/>
    </array>
    <array name="m_Items2" type="atArray" >
      <pointer type="netCatalogBaseItem" policy="owner"/>
    </array>
    <int name="m_Version" init="0"/>
    <u32 name="m_CRC" init="0"/>
    <int name="m_LatestServerVersion" init="0"/>
  </structdef>
 
  <structdef type="netCatalogGeneralItem" base="netCatalogBaseItem">
    <string name="m_statName" type="atString" />
    <enum name="m_storageType" type="netCatalogBaseItem::StorageType" size="8" />
    <u8 name="m_bitShift" init="0"/>
    <u8 name="m_bitSize" init="0" />
    <bool name="m_isPlayerStat" init="false" />
  </structdef>

  <structdef type="netCatalog">
    <map name="m_catalog" type="atMap" key="atHashString" >
      <pointer type="netCatalogBaseItem" policy="owner"/>
    </map>
  </structdef>

</ParserSchema>
