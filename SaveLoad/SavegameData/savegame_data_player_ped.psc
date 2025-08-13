<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CPlayerPedSaveStructure::CPlayerDecorationStruct" preserveNames="true">
    <Vector4 name="m_UVCoords"/>
    <Vector2 name="m_Scale"/>
    <u32 name="m_TxdHash"/>
    <u32 name="m_TxtHash"/>
    <u8 name="m_Type"/>
    <u8 name="m_Zone"/>
    <u8 name="m_Alpha"/>
    <u8 name="m_FixedFrame"/>
    <float name="m_Age"/>
    <u8 name="m_FlipUVFlags"/>
    <u32 name="m_SourceNameHash"/>
  </structdef>

  <structdef type="CPlayerPedSaveStructure::SPedCompPropConvData" preserveNames="true">
    <u32 name="m_hash"/>
    <u32 name="m_data"/>
  </structdef>

  <structdef type="CPlayerPedSaveStructure" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <u32 name="m_GamerIdHigh"/>
    <u32 name="m_GamerIdLow"/>
    <u16 name="m_CarDensityForCurrentZone"/>
    <s32 name="m_CollectablesPickedUp"/>
    <s32 name="m_TotalNumCollectables"/>
    <bool name="m_DoesNotGetTired"/>
    <bool name="m_FastReload"/>
    <bool name="m_FireProof"/>
    <u16 name="m_MaxHealth"/>
    <u16 name="m_MaxArmour"/>
    <bool name="m_bCanDoDriveBy"/>
    <bool name="m_bCanBeHassledByGangs"/>
    <u16 name="m_nLastBustMessageNumber"/>

    <u32 name="m_ModelHashKey"/>
    <Vector3 name="m_Position"/>
    <float name="m_fHealth"/>
    <float name="m_nArmour"/>
    <u32 name="m_nCurrentWeaponSlot"/>

    <bool name="m_bPlayerHasHelmet"/>
    <s32 name="m_nStoredHatPropIdx"/>
    <s32 name="m_nStoredHatTexIdx"/>

    <array name="m_CurrentProps" type="atFixedArray" size="6">
      <struct type="CPlayerPedSaveStructure::SPedCompPropConvData"/>
    </array>

    <map name="m_ComponentVariations" type="atBinaryMap" key="atHashString">
      <struct type="CPlayerPedSaveStructure::SPedCompPropConvData"/>
    </map>

    <map name="m_TextureVariations" type="atBinaryMap" key="atHashString">
      <u8/>
    </map>

    <array name="m_DecorationList" type="atArray">
      <struct type="CPlayerPedSaveStructure::CPlayerDecorationStruct"/>
    </array>

  </structdef>



  <structdef type="CPlayerPedSaveStructure_Migration" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <u32 name="m_GamerIdHigh"/>
    <u32 name="m_GamerIdLow"/>
    <u16 name="m_CarDensityForCurrentZone"/>
    <s32 name="m_CollectablesPickedUp"/>
    <s32 name="m_TotalNumCollectables"/>
    <bool name="m_DoesNotGetTired"/>
    <bool name="m_FastReload"/>
    <bool name="m_FireProof"/>
    <u16 name="m_MaxHealth"/>
    <u16 name="m_MaxArmour"/>
    <bool name="m_bCanDoDriveBy"/>
    <bool name="m_bCanBeHassledByGangs"/>
    <u16 name="m_nLastBustMessageNumber"/>

    <u32 name="m_ModelHashKey"/>
    <Vector3 name="m_Position"/>
    <float name="m_fHealth"/>
    <float name="m_nArmour"/>
    <u32 name="m_nCurrentWeaponSlot"/>

    <bool name="m_bPlayerHasHelmet"/>
    <s32 name="m_nStoredHatPropIdx"/>
    <s32 name="m_nStoredHatTexIdx"/>

    <array name="m_CurrentProps" type="atFixedArray" size="6">
      <struct type="CPlayerPedSaveStructure::SPedCompPropConvData"/>
    </array>

    <map name="m_ComponentVariations" type="atBinaryMap" key="u32">
      <struct type="CPlayerPedSaveStructure::SPedCompPropConvData"/>
    </map>

    <map name="m_TextureVariations" type="atBinaryMap" key="u32">
      <u8/>
    </map>

    <array name="m_DecorationList" type="atArray">
      <struct type="CPlayerPedSaveStructure::CPlayerDecorationStruct"/>
    </array>

  </structdef>

</ParserSchema>
