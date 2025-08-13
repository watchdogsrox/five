<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
							generate="class">

  <enumdef type="ePackFileMetaDataAssetType" >
    <enumval name="AT_TXD"/>
    <enumval name="AT_DRB"/>
    <enumval name="AT_DWD"/>
    <enumval name="AT_FRG"/>
  </enumdef>

  <enumdef type="ePackFileMetaDataImapGroupType">
    <enumval name="TIME_DEPENDENT"/>
    <enumval name="WEATHER_DEPENDENT"/>
  </enumdef>
  
  <structdef type="CHDTxdAssetBinding" simple="true">
    <enum name="m_assetType" type="ePackFileMetaDataAssetType" />
    <string name="m_targetAsset" type="member" size="64"/>
    <string name="m_HDTxd" type="member" size="64"/>
  </structdef>

  <structdef type="CMapDataGroup" simple="true">
    <string name="m_Name" type="atHashString"/>
    <array name="m_Bounds" type="atArray">
      <string type="atHashString"/>
    </array>
    <bitset name="m_Flags" type="fixed32" values="ePackFileMetaDataImapGroupType"/>
    <array name="m_WeatherTypes" type="atArray">
      <string type="atHashString"/>
    </array>
    <u32 name="m_HoursOnOff"/>
  </structdef>

  <structdef type="CImapDependency" simple="true">
    <string name="m_imapName" type="atHashString" />
    <string name="m_itypName" type="atHashString" />
    <string name="m_packFileName" type="atHashString" />
  </structdef>

  <enumdef type="manifestFlags" generate="bitset">
    <enumval name="INTERIOR_DATA"/>
  </enumdef>
  
  <structdef type="CImapDependencies" simple="true">
    <string name="m_imapName" type="atHashString" />
    <bitset name="m_manifestFlags" type="fixed" numBits="32" values="manifestFlags"/>
    <array name="m_itypDepArray" type="atArray">
      <string name="m_itypDepName" type="atHashString" />
    </array>  
  </structdef>

  <structdef type="CItypDependencies" simple="true">
    <string name="m_itypName" type="atHashString" />
    <bitset name="m_manifestFlags" type="fixed" numBits="32" values="manifestFlags"/>
    <array name="m_itypDepArray" type="atArray">
      <string name="m_itypDepName" type="atHashString" />
    </array>
  </structdef>

  <structdef type="CInteriorBoundsFiles" simple="true">
    <string name="m_Name" type="atHashString" />
    <array name="m_Bounds" type="atArray">
      <string type="atHashString" />
    </array>
  </structdef>
  
  <structdef type="CPackFileMetaData" simple="true">
    <array name="m_MapDataGroups" type="atArray">
      <struct type="CMapDataGroup"/>
    </array>
    <array name="m_HDTxdBindingArray" type="atArray">
      <struct type="CHDTxdAssetBinding"/>
    </array>
    <array name="m_imapDependencies" type="atArray">
      <struct type="CImapDependency"/>
    </array>
    <array name="m_imapDependencies_2" type="atArray">
      <struct type="CImapDependencies"/>
    </array>
    <array name="m_itypDependencies_2" type="atArray">
      <struct type="CItypDependencies"/>
    </array>
    <array name="m_Interiors" type="atArray">
      <struct type="CInteriorBoundsFiles"/>
    </array>
  </structdef>


  
</ParserSchema>