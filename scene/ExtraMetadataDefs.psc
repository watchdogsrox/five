<?xml version="1.0"?>
<ParserSchema xmlns="http://www.rockstargames.com/RageParserSchema"
 generate="class">

  <hinsert>
#include "peds/rendering/PedProps.h"
#include "modelinfo/VehicleModelInfoEnums.h"
#include "peds/rendering/PedVariationDS.h"
#include "task/Physics/TaskNMSimple.h"
#include "task/Physics/TaskNMBrace.h"
  </hinsert>
	
	<structdef type="NMTuningSetEntry">
		<struct name="m_tuningSet" type="CNmTuningSet"/>
		<string name="m_key" type="atHashString"/>
	</structdef>
	<structdef type="NMTaskSimpleTunableEntry">
		<struct name="m_taskSimpleEntry" type="CTaskNMSimple::Tunables::Tuning"/>
		<string name="m_key" type="atHashString"/>
	</structdef>
	<structdef type="NMExtraTunables">
		<array name="m_weaponFlinchTunables" type="atArray">
			<struct type="NMTuningSetEntry"/>
		</array>
		<array name="m_actionFlinchTunables" type="atArray">
			<struct type="NMTuningSetEntry"/>
		</array>
		<array name="m_weaponShotTunables" type="atArray">
			<struct type="NMTuningSetEntry"/>
		</array>
		<array name="m_grabHelperTunables" type="atArray">
			<struct type="NMTuningSetEntry"/>
		</array>
		<array name="m_jumpRollTunables" type="atArray">
			<struct type="NMTuningSetEntry"/>
		</array>
		<array name="m_taskSimpleTunables" type="atArray">
			<struct type="NMTaskSimpleTunableEntry" />
		</array>
		<array name="m_vehicleTypeTunables" type="atArray">
			<struct type="CTaskNMBrace::Tunables::VehicleTypeOverrides"/>
		</array>
	</structdef>

  <!-- BASE SHOP ITEM -->
  <structdef type="BaseShopItem" constructable="false">
    <string name="m_lockHash" type="atHashString"/>
    <u16 name="m_cost" init="50"/>
    <string name="m_textLabel" type="atString" init ="INVALID"/>
  </structdef>
  <!-- BASE SHOP ITEM -->
  
  <!-- CLOTHING SHOP ITEMS -->
  <enumdef type="eLocateEnum">
    <enumval name="LOC_ANY" value="-1"/>
  </enumdef>
  
  <enumdef type="eShopEnum">
    <enumval name="CLO_SHOP_LOW" value="0"/>
    <enumval name="CLO_SHOP_MID"/>
    <enumval name="CLO_SHOP_HIGH"/>
    <enumval name="CLO_SHOP_GUN_LARGE"/>
    <enumval name="CLO_SHOP_GUN_SMALL"/>
    <enumval name="CLO_SHOP_AMB"/>
    <enumval name="CLO_SHOP_CASINO"/>
    <enumval name="CLO_SHOP_CAR_MEET"/>
    <enumval name="CLO_SHOP_ARMORY_FIXER"/>
    <enumval name="CLO_SHOP_MUSIC_STUDIO"/>
    <enumval name="CLO_SHOP_NONE"/>
  </enumdef>

  <enumdef type="eScrCharacters">
    <enumval name="SCR_CHAR_MICHAEL" value="0"/>
    <enumval name="SCR_CHAR_FRANKLIN"/>
    <enumval name="SCR_CHAR_TREVOR"/>
    <enumval name="SCR_CHAR_MULTIPLAYER"/>
    <enumval name="SCR_CHAR_MULTIPLAYER_F"/>
    <enumval name="SCR_CHAR_ANY"/>
  </enumdef>

  <enumdef type="eShopPedApparel">
    <enumval name="SHOP_PED_COMPONENT" value="0"/>
    <enumval name="SHOP_PED_PROP"/>
    <enumval name="SHOP_PED_OUTFIT"/>
  </enumdef>

  <!-- General tags about a given piece of apparel -->
  <structdef type="RestrictionTags">
    <string name="m_tagNameHash" type="atHashString" init="0"/>
  </structdef>

  <structdef type="ComponentDescription">
    <string name="m_nameHash" type="atHashString" init="0"/>
    <s32 name="m_enumValue" init="-1"/>
    <enum name="m_eCompType" type="ePedVarComp" init="PV_COMP_HEAD"/>
  </structdef>

  <structdef type="PropDescription">
    <string name="m_nameHash" type="atHashString" init="0"/>
    <s32 name="m_enumValue" init="-1"/>
    <enum name="m_eAnchorPoint" type="eAnchorPoints" init="ANCHOR_NONE"/>
  </structdef>

  <structdef type="BaseShopPedApparel" base="BaseShopItem">
    <string name="m_uniqueNameHash" type="atHashString"/>
    <enum name="m_eShopEnum" type="eShopEnum" init="CLO_SHOP_LOW"/>
    <array name="m_restrictionTags" type="atArray">
      <struct type="RestrictionTags"/>
    </array>
    <array name="m_forcedComponents" type="atArray">
      <struct type="ComponentDescription"/>
    </array>
    <array name="m_forcedProps" type="atArray">
      <struct type="PropDescription"/>
    </array>    
    <array name="m_variantComponents" type="atArray">
      <struct type="ComponentDescription"/>
    </array>
    <array name="m_variantProps" type="atArray">
      <struct type="PropDescription"/>
    </array>
    <s16 name="m_locate" init="0"/>
  </structdef>

  <structdef type="ShopPedComponent" base="BaseShopPedApparel">
    <u32 name="m_drawableIndex" init="0" hideWidgets="true"/>
    <u8 name="m_localDrawableIndex" init="0"/>
    <u8 name="m_textureIndex" init="0"/>
    <bool name="m_isInOutfit" init="false"/>
    <enum name="m_eCompType" type="ePedVarComp" init="PV_COMP_HEAD"/>
  </structdef>

  <structdef type="ShopPedProp" base="BaseShopPedApparel">
    <u8 name="m_propIndex" init="0" hideWidgets="true"/>
    <u8 name="m_localPropIndex" init="0"/>
    <u8 name="m_textureIndex" init="0"/>
    <bool name="m_isInOutfit" init="false"/>
    <enum name="m_eAnchorPoint" type="eAnchorPoints" init="ANCHOR_NONE"/>
  </structdef>

  <structdef type="ShopPedOutfit" base="BaseShopPedApparel">
	  <array name="m_includedPedComponents" type="atArray">
		  <struct type="ComponentDescription"/>
	  </array>
	  <array name="m_includedPedProps" type="atArray">
		  <struct type="PropDescription"/>
	  </array>
  </structdef>

  <structdef type="ShopPedApparel">
    <string name="m_pedName" type="ConstString"/>
    <string name="m_dlcName" type="ConstString"/>
    <string name="m_fullDlcName" type="atHashString"/> <!-- This is actually the CPedVariationInfo meta data name -->
    <string name="m_creatureMetaData" type="atHashString"/>
    <string name="m_nameHash" type="atHashString"/>
    <enum name="m_eCharacter" type="eScrCharacters" init="SCR_CHAR_ANY"/>
    <array name="m_pedComponents" type="atArray">
      <struct type="ShopPedComponent"/>
    </array>
    <array name="m_pedProps" type="atArray">
      <struct type="ShopPedProp"/>
    </array>
    <array name="m_pedOutfits" type="atArray">
      <struct type="ShopPedOutfit"/>
    </array>
  </structdef>
  <!-- CLOTHING SHOP ITEMS -->

  <!-- TATTOO ITEMS -->
  <enumdef type="eTattooFaction">
    <enumval name="TATTOO_SP_MICHAEL" value="0"/>
    <enumval name="TATTOO_SP_FRANKLIN" />
    <enumval name="TATTOO_SP_TREVOR" />
    <enumval name="TATTOO_MP_FM"/>
    <enumval name="TATTOO_MP_FM_F"/>
  </enumdef>

  <enumdef type="eTattooFacing">
    <enumval name="TATTOO_FRONT" value="0"/>
    <enumval name="TATTOO_BACK" />
    <enumval name="TATTOO_LEFT" />
    <enumval name="TATTOO_RIGHT"/>
    <enumval name="TATTOO_FRONT_LEFT"/>
    <enumval name="TATTOO_BACK_LEFT"/>
    <enumval name="TATTOO_FRONT_RIGHT"/>
    <enumval name="TATTOO_BACK_RIGHT"/>
  </enumdef>

  <structdef type="TattooShopItem" base="BaseShopItem">
    <u16 name="m_id" />
    <string name="m_collection" type="atHashString" />
    <string name="m_preset" type="atHashString" />
    <string name="m_updateGroup" type="atHashString" />
    <enum name="m_eFacing" type="eTattooFacing" init="TATTOO_FRONT" description="camera facing of the item"/>
    <enum name="m_eFaction" type="eTattooFaction" />
  </structdef>

  <structdef type="TattooShopItemArray">
    <array name="TattooShopItems" type="atArray">
      <struct type="TattooShopItem"/>
    </array>
    <string name="m_nameHash" type="atHashString"/>
  </structdef>
  <!-- TATTOO ITEMS -->


  <!-- WEAPON ITEMS -->
	<structdef type="ShopWeaponComponent" base ="BaseShopItem">
		<string name="m_componentName" type="atHashString" />
		<string name="m_componentDesc" type="atString" init ="INVALID"/>
		<u16 name="m_id"/>
	</structdef>
	<structdef type="WeaponShopItem" base="BaseShopItem">
		<s32 name="m_ammoCost" init="50"/>
		<string name="m_nameHash" type="atHashString" />
		<string name="m_weaponDesc" type="atString" init ="INVALID"/>
		<string name="m_weaponTT" type="atString" init ="INVALID"/>
		<string name="m_weaponUppercase" type="atString" init ="INVALID"/>
		<array name ="m_weaponComponents" type="atArray">
			<struct type="ShopWeaponComponent"/>
		</array>
    <bool name="m_availableInSP" init="false"/>
	</structdef>


	<structdef type="WeaponShopItemArray">
		<array name="m_weaponShopItems" type="atArray">
			<struct type="WeaponShopItem"/>
		</array>
		<string name="m_nameHash" type="atHashString" init="NONE"/>
	</structdef>
  <!-- WEAPON ITEMS -->

  <!-- VEHICLES -->
  <enumdef type="eVehicleFlags">
    <enumval name="VF_DISABLE_GARAGE"/>
  </enumdef>

  <structdef type="ShopVehicleMod">
		<string name="m_nameHash" type="atHashString" />
		<string name="m_lockHash" type="atHashString"/>
    	<u16 name="m_cost" init="50"/>
	</structdef>
	<structdef type="ShopVehicleData" base="BaseShopItem">
		<string name="m_modelNameHash" type="atHashString" />
		<array name ="m_vehicleMods" type="atArray">
			<struct type="ShopVehicleMod"/>
		</array>
    <bitset name="m_vehicleFlags" numBits="32" type="fixed32" values="eVehicleFlags"/>
	</structdef>

	<structdef type="ShopVehicleDataArray">
		<array name="m_Vehicles" type="atArray">
			<struct type="ShopVehicleData"/>
		</array>
		<array name="m_VehicleMods" type="atArray">
			<struct type="ShopVehicleMod"/>
		</array>
		<string name="m_nameHash" type="atHashString" init="NONE"/>
	</structdef>

  <!-- VEHICLES -->

</ParserSchema>
