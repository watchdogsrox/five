<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema">

  <structdef type="CSaveGarages::CSavedVehicleVariationInstance" preserveNames="true">
    <u8 name="m_kitIdx"/>
    <u8 name="m_color1"/>
    <u8 name="m_color2"/>
    <u8 name="m_color3"/>
    <u8 name="m_color4"/>
    <u8 name="m_color5" init="0"/>
    <u8 name="m_color6" init="0"/>

    <u8 name="m_smokeColR"/>
    <u8 name="m_smokeColG"/>
    <u8 name="m_smokeColB"/>

    <u8 name="m_neonColR" init="0"/>
    <u8 name="m_neonColG" init="0"/>
    <u8 name="m_neonColB" init="0"/>
    <u8 name="m_neonFlags" init="0"/>

    <map name="m_mods" type="atBinaryMap" key="atHashString">
      <u8/>
    </map>

    <u8 name="m_windowTint"/>
    <u8 name="m_wheelType" init="0"/>

    <array name="m_modVariation" type="member" size="2">
      <bool />
    </array>

    <u16 name="m_kitID_U16" init="65535"/>
  </structdef>

  <const name="CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX" value="8"/>

  <structdef type="CSaveGarages::CSavedVehicle" preserveNames="true">
    <struct name="m_variation" type="CSaveGarages::CSavedVehicleVariationInstance"/>
    <float name="m_CoorX"/>
    <float name="m_CoorY"/>
    <float name="m_CoorZ"/>
    <u32 name="m_FlagsLocal"/>
    <u32 name="m_ModelHashKey"/>
    <u32 name="m_nDisableExtras"/>
    <s32 name="m_LiveryId"/>
    <s32 name="m_Livery2Id" init="-1"/>
    <s16 name="m_HornSoundIndex"/>
    <s16 name="m_AudioEngineHealth"/>
    <s16 name="m_AudioBodyHealth"/>
    <s8 name="m_iFrontX"/>
    <s8 name="m_iFrontY"/>
    <s8 name="m_iFrontZ"/>
    <bool name="m_bUsed"/>
    <bool name="m_bInInterior"/>
    <bool name="m_bNotDamagedByBullets"/>
    <bool name="m_bNotDamagedByFlames"/>
    <bool name="m_bIgnoresExplosions"/>
    <bool name="m_bNotDamagedByCollisions"/>
    <bool name="m_bNotDamagedByMelee"/>
    <bool name="m_bTyresDontBurst" init="false"/>
    <s8 name="m_LicensePlateTexIndex" init="-1"/>

    <array name="m_LicencePlateText" type="member" size="CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX">
      <u8 />
    </array>

  </structdef>

  <structdef type="CSaveGarages::CSavedVehiclesInSafeHouse" preserveNames="true">
    <u32 name="m_NameHashOfGarage"/>
    <array name="m_VehiclesSavedInThisSafeHouse" type="atArray">
      <struct type="CSaveGarages::CSavedVehicle"/>
    </array>
  </structdef>

  <structdef type="CSaveGarages::CSaveGarage" preserveNames="true">
    <u32 name="m_NameHash"/>
    <u8 name="m_Type"/>
    <bool name="m_bLeaveCameraAlone"/>
    <bool name="m_bSavingVehilclesEnabled" init="true"/>
  </structdef>

  <structdef type="CSaveGarages" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <bool name="m_RespraysAreFree"/>
    <bool name="m_NoResprays"/>

    <array name="m_SafeHouses" type="atArray">
      <struct type="CSaveGarages::CSavedVehiclesInSafeHouse"/>
    </array>

    <array name="m_SavedGarages" type="atArray">
      <struct type="CSaveGarages::CSaveGarage"/>
    </array>
  </structdef>



  <structdef type="CSavedVehicleVariationInstance_Migration" preserveNames="true">
    <u8 name="m_kitIdx"/>
    <u8 name="m_color1"/>
    <u8 name="m_color2"/>
    <u8 name="m_color3"/>
    <u8 name="m_color4"/>
    <u8 name="m_color5" init="0"/>
    <u8 name="m_color6" init="0"/>

    <u8 name="m_smokeColR"/>
    <u8 name="m_smokeColG"/>
    <u8 name="m_smokeColB"/>

    <u8 name="m_neonColR" init="0"/>
    <u8 name="m_neonColG" init="0"/>
    <u8 name="m_neonColB" init="0"/>
    <u8 name="m_neonFlags" init="0"/>

    <map name="m_mods" type="atBinaryMap" key="u32">
      <u8/>
    </map>

    <u8 name="m_windowTint"/>
    <u8 name="m_wheelType" init="0"/>

    <array name="m_modVariation" type="member" size="2">
      <bool />
    </array>

    <u16 name="m_kitID_U16" init="65535"/>
  </structdef>

  <structdef type="CSavedVehicle_Migration" preserveNames="true">
    <struct name="m_variation" type="CSavedVehicleVariationInstance_Migration"/>
    <float name="m_CoorX"/>
    <float name="m_CoorY"/>
    <float name="m_CoorZ"/>
    <u32 name="m_FlagsLocal"/>
    <u32 name="m_ModelHashKey"/>
    <u32 name="m_nDisableExtras"/>
    <s32 name="m_LiveryId"/>
    <s32 name="m_Livery2Id" init="-1"/>
    <s16 name="m_HornSoundIndex"/>
    <s16 name="m_AudioEngineHealth"/>
    <s16 name="m_AudioBodyHealth"/>
    <s8 name="m_iFrontX"/>
    <s8 name="m_iFrontY"/>
    <s8 name="m_iFrontZ"/>
    <bool name="m_bUsed"/>
    <bool name="m_bInInterior"/>
    <bool name="m_bNotDamagedByBullets"/>
    <bool name="m_bNotDamagedByFlames"/>
    <bool name="m_bIgnoresExplosions"/>
    <bool name="m_bNotDamagedByCollisions"/>
    <bool name="m_bNotDamagedByMelee"/>
    <bool name="m_bTyresDontBurst" init="false"/>
    <s8 name="m_LicensePlateTexIndex" init="-1"/>

    <array name="m_LicencePlateText" type="member" size="CCustomShaderEffectVehicle::LICENCE_PLATE_LETTERS_MAX">
      <u8 />
    </array>

  </structdef>

  <structdef type="CSavedVehiclesInSafeHouse_Migration" preserveNames="true">
    <u32 name="m_NameHashOfGarage"/>
    <array name="m_VehiclesSavedInThisSafeHouse" type="atArray">
      <struct type="CSavedVehicle_Migration"/>
    </array>
  </structdef>

  <structdef type="CSaveGarage_Migration" preserveNames="true">
    <u32 name="m_NameHash"/>
    <u8 name="m_Type"/>
    <bool name="m_bLeaveCameraAlone"/>
    <bool name="m_bSavingVehilclesEnabled" init="true"/>
  </structdef>

  <structdef type="CSaveGarages_Migration" onPostLoad="PostLoad" onPreSave="PreSave" preserveNames="true">
    <bool name="m_RespraysAreFree"/>
    <bool name="m_NoResprays"/>

    <array name="m_SafeHouses" type="atArray">
      <struct type="CSavedVehiclesInSafeHouse_Migration"/>
    </array>

    <array name="m_SavedGarages" type="atArray">
      <struct type="CSaveGarage_Migration"/>
    </array>
  </structdef>


</ParserSchema>
