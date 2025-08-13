//
// filename:	commands_extrametadata.cpp
// description:
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/wrapper.h"

// Game headers
#include "scene/ExtraMetadataMgr.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "peds/rendering/PedVariationPack.h"
#include "peds/ped.h"
#include "weapons/components/WeaponComponentManager.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// Keep in sync with definitions in commands_extrametadata.sch

struct scrOutfitCompStruct
{
	scrValue nameHash;
	scrValue enumValue;
	scrValue eCompType;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrOutfitCompStruct);

struct scrOutfitPropStruct
{
	scrValue nameHash;
	scrValue enumValue;
	scrValue anchorPoint;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrOutfitPropStruct);

struct scrTattooShopItemValues
{
	scrValue m_lockHash;
	scrValue id;
	scrValue collection;
	scrValue preset;
	scrValue cost;
	scrValue facing;
	scrValue updateGroup;
	scrTextLabel31 label;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrTattooShopItemValues);

// Keep in sync with definitions in commands_extrametadata.sch
struct scrShopPedComponent
{
	scrValue m_lockHash;
	scrValue m_nameHash;
	scrValue m_locate;
	scrValue m_drawableIndex;
	scrValue m_textureIndex;
	scrValue m_cost;
	scrValue m_eCompType;
	scrValue m_eShopEnum;
	scrValue m_eCharacter;
	scrTextLabel31 m_textLabel;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrShopPedComponent);

// Keep in sync with definitions in commands_extrametadata.sch
struct scrShopPedProp
{
	scrValue m_lockHash;
	scrValue m_nameHash;
	scrValue m_locate;
	scrValue m_propIndex;
	scrValue m_textureIndex;
	scrValue m_cost;
	scrValue m_eAnchorPoint;
	scrValue m_eShopEnum;
	scrValue m_eCharacter;
	scrTextLabel31 m_textLabel;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrShopPedProp);

// Keep in sync with definitions in commands_extrametadata.sch
struct scrShopPedOutfit
{
	scrValue m_lockHash;
	scrValue m_nameHash;
	scrValue m_cost;
	scrValue m_numberOfProps;
	scrValue m_numberOfComponents;
	scrValue m_eShopEnum;
	scrValue m_eCharacter;
	scrTextLabel31 m_textLabel;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrShopPedOutfit);

// Keep in sync with definitions in commands_extrametadata.sch
struct scrShopVehicleData
{
	scrValue m_lockHash;
	scrValue m_nameHash;
	scrValue m_cost;
	scrTextLabel31 m_textLabel;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrShopVehicleData);

//Keep in sync with definitions in commands_extrametadata.sch
struct scrShopVehicleModData
{
	scrValue m_lockHash;
	scrValue m_nameHash;
	scrValue m_cost;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrShopVehicleModData);

// Keep in sync with definitions in commands_extrametadata.sch
struct scrShopWeaponData
{
	scrValue m_lockHash;
	scrValue m_nameHash;
	scrValue id;
	scrValue cost;
	scrValue ammoCost;
	scrValue ammoType;
	scrValue defaultClipSize;
	scrTextLabel31 label;
	scrTextLabel31 weaponDesc;
	scrTextLabel31 weaponTT;
	scrTextLabel31 weaponUppercase;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrShopWeaponData);

//Keep in sync with definitions in commands_extrametadata.sch
struct scrShopWeaponComponentData
{
	scrValue m_ModType;
	scrValue m_isDefault;
	scrValue m_lockHash;
	scrValue m_componentName;
	scrValue m_id;
	scrValue m_cost;
	scrTextLabel31 m_textLabel;
	scrTextLabel31 m_componentDesc;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrShopWeaponComponentData);


// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

namespace extrametadata_commands {

int CommandGetNumDLCVehicles()
{
	return EXTRAMETADATAMGR.GetNumDLCVehicles();
}

int CommandGetDLCVehicleModel(int index)
{
	return EXTRAMETADATAMGR.GetShopVehicleModelNameHash(index);
}

int CommandGetNumDLCVehicleMods(int vehicleIndex)
{
	return EXTRAMETADATAMGR.GetNumDLCVehicleMods(vehicleIndex);
}

bool CommandGetDLCVehicleData(int dlcIndex, scrShopVehicleData* out_Values)
{
	const ShopVehicleData* veh = EXTRAMETADATAMGR.GetShopVehicleData(dlcIndex);
	if(scriptVerifyf(veh, "GET_DLC_VEHICLE_DATA - %s mod is NULL for dlcIndex [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), dlcIndex))
	{
		out_Values->m_cost.Int = veh->m_cost;
		out_Values->m_lockHash.Int = veh->m_lockHash;
		out_Values->m_nameHash.Int = veh->m_modelNameHash;
		return true;
	}
	return false;
}

int CommandGetDLCVehicleFlags(int dlcIndex)
{
	const ShopVehicleData* veh = EXTRAMETADATAMGR.GetShopVehicleData(dlcIndex);
	if (scriptVerifyf(veh, "GET_DLC_VEHICLE_FLAGS - %s mod is NULL for dlcIndex [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), dlcIndex))
		return (int)veh->m_vehicleFlags.GetRawBlock(0);
	
	return 0;
}

bool CommandGetDLCVehicleModData(int dlcIndex, int modIndex, scrShopVehicleModData* out_Values)
{
	const ShopVehicleMod* vMod = EXTRAMETADATAMGR.GetShopVehicleModData(dlcIndex,modIndex);
	if(scriptVerifyf(vMod, "GET_DLC_VEHICLE_MOD_DATA - %s mod is NULL for dlcIndex [%d] modIndex [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), dlcIndex, modIndex))
	{
		out_Values->m_cost.Int = vMod->m_cost;
		out_Values->m_lockHash.Int = vMod->m_lockHash;
		out_Values->m_nameHash.Int = vMod->m_nameHash;
		return true;
	}
	return false;
}
bool CommandIsDlcVehicleMod(s32 identifier)
{
	const atArray<ShopVehicleMod*>& modArray = EXTRAMETADATAMGR.GetGenericVehicleModList();
	for(int i=0;i<modArray.GetCount();i++)
	{
		if(modArray[i]->m_nameHash.GetHash() == static_cast<u32>(identifier))
			return true;
	}
	return false;	
}

s32 CommandGetDLCVehicleModLockHash(s32 identifier)
{
	const atArray<ShopVehicleMod*>& modArray = EXTRAMETADATAMGR.GetGenericVehicleModList();
	for(int i=0;i<modArray.GetCount();i++)
	{
		if(modArray[i]->m_nameHash.GetHash() == static_cast<u32>(identifier))
			return modArray[i]->m_lockHash;
	}
	return -1;
}

int CommandGetNumTattooShopItems(int faction)
{
	scriptAssertf((u32)faction < (u32)eTattooFaction_NUM_ENUMS, "GET_NUM_TATTOO_SHOP_DLC_ITEMS - %s wrong faction [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), faction);
	return EXTRAMETADATAMGR.GetNumTattooShopItemsInFaction((eTattooFaction)faction);
}

bool CommandGetTattooShopItemData(int faction, int tattooIdx, scrTattooShopItemValues* out_Values)
{
	const TattooShopItem* pTattooItem = EXTRAMETADATAMGR.GetTattooShopItem((eTattooFaction)faction, tattooIdx);
	if(scriptVerifyf(pTattooItem, "GET_TATTOO_SHOP_DLC_ITEM_DATA - %s pTattooItem is NULL for faction [%d] idx [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), faction, tattooIdx))
	{
		out_Values->m_lockHash.Int = pTattooItem->m_lockHash;
		out_Values->id.Int = pTattooItem->m_id;
		out_Values->collection.Int = pTattooItem->m_collection;
		out_Values->preset.Int = pTattooItem->m_preset;
		out_Values->cost.Int = pTattooItem->m_cost;
		out_Values->facing.Int = (int)pTattooItem->m_eFacing;
		out_Values->updateGroup.Int = pTattooItem->m_updateGroup;
		safecpy(out_Values->label, pTattooItem->m_textLabel.c_str());	

		return true;
	}

	return false;
}

s32 CommandGetTattooShopItemIndex(s32 faction, s32 collectionHash, s32 presetHash)
{
	if (scriptVerifyf( (faction >= 0) && (faction < eTattooFaction_NUM_ENUMS), "GET_TATTOO_SHOP_DLC_ITEM_INDEX - %s - Faction index [%d] is out of range 0 to %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), faction, eTattooFaction_NUM_ENUMS))
	{
		return EXTRAMETADATAMGR.GetTattooShopItemIndex((eTattooFaction)faction, collectionHash, presetHash);
	}

	return -1;
}

void CommandInitShopPedComponent(scrShopPedComponent* itemInfo)
{
	itemInfo->m_nameHash.Int = 0;
	itemInfo->m_cost.Int = 0;
	itemInfo->m_drawableIndex.Int = 0;
	itemInfo->m_eCharacter.Int = 0;
	itemInfo->m_eCompType.Int = 0;
	itemInfo->m_eShopEnum.Int = 0;
	itemInfo->m_locate.Int = 0;
	itemInfo->m_textureIndex.Int = 0;
}

void CommandInitShopPedProp(scrShopPedProp* itemInfo)
{
	itemInfo->m_nameHash.Int = 0;
	itemInfo->m_cost.Int = 0;
	itemInfo->m_propIndex.Int = 0;
	itemInfo->m_eCharacter.Int = 0;
	itemInfo->m_eAnchorPoint.Int = 0;
	itemInfo->m_eShopEnum.Int = 0;
	itemInfo->m_locate.Int = 0;
	itemInfo->m_textureIndex.Int = 0;
}

void CopyShopPedComponentToScr(scrShopPedComponent* dest, ShopPedComponent* src, eScrCharacters character)
{
	if (src && dest)
	{
		dest->m_lockHash.Int = src->m_lockHash;
		dest->m_nameHash.Int = src->m_uniqueNameHash;
		dest->m_cost.Int = src->m_cost;
		dest->m_drawableIndex.Int = src->m_drawableIndex;
		dest->m_eCharacter.Int = character;
		dest->m_eCompType.Int = src->m_eCompType;
		dest->m_eShopEnum.Int = src->m_eShopEnum;
		dest->m_locate.Int = src->m_locate;
		dest->m_textureIndex.Int = src->m_textureIndex;
		safecpy(dest->m_textLabel, src->m_textLabel.c_str());
	}
}

void CopyShopPedPropToScr(scrShopPedProp* dest, ShopPedProp* src, eScrCharacters character)
{
	if (src && dest)
	{
		dest->m_lockHash.Int = src->m_lockHash;
		dest->m_nameHash.Int = src->m_uniqueNameHash;
		dest->m_cost.Int = src->m_cost;
		dest->m_propIndex.Int = src->m_propIndex;
		dest->m_eCharacter.Int = character;
		dest->m_eAnchorPoint.Int = src->m_eAnchorPoint;
		dest->m_eShopEnum.Int = src->m_eShopEnum;
		dest->m_locate.Int = src->m_locate;
		dest->m_textureIndex.Int = src->m_textureIndex;
		safecpy(dest->m_textLabel, src->m_textLabel.c_str());
	}
}

void CopyShopPedOutfitToScr(scrShopPedOutfit* dest, ShopPedOutfit* src, eScrCharacters character)
{
	if(src&&dest)
	{
		dest->m_lockHash.Int = src->m_lockHash;
		dest->m_cost.Int = src->m_cost;
		dest->m_nameHash.Int = src->m_uniqueNameHash;
		dest->m_numberOfComponents.Int = src->m_includedPedComponents.GetCount();
		dest->m_numberOfProps.Int = src->m_includedPedProps.GetCount();
		dest->m_eCharacter.Int = character;
		dest->m_eShopEnum.Int = src->m_eShopEnum;
		safecpy(dest->m_textLabel, src->m_textLabel.c_str());
	}
}

// TU function so we don't change SP scripts...
s32 CommandSetupShopPedApparelQueryTU(s32 character, s32 shop, s32 locate, s32 apparelType, s32 anchorPoint, s32 componentType)
{
	if (apparelType == SHOP_PED_COMPONENT)
		return EXTRAMETADATAMGR.SetupShopPedComponentQuery((eScrCharacters)character, (eShopEnum)shop, locate, (ePedVarComp)componentType);
	else if (apparelType == SHOP_PED_PROP)
		return EXTRAMETADATAMGR.SetupShopPedPropQuery((eScrCharacters)character, (eShopEnum)shop, locate, (eAnchorPoints)anchorPoint);
	else if (apparelType == SHOP_PED_OUTFIT)
		return EXTRAMETADATAMGR.SetupShopPedOutfitQuery((eScrCharacters)character, (eShopEnum)shop, locate);

	return 0;
}

// Non TU function so we don't change SP scripts...
s32 CommandSetupShopPedApparelQuery(s32 character, s32 shop, s32 locate, s32 apparelType)
{
	if (apparelType == SHOP_PED_COMPONENT)
		return EXTRAMETADATAMGR.SetupShopPedComponentQuery((eScrCharacters)character, (eShopEnum)shop, locate, PV_COMP_INVALID);
	else if (apparelType == SHOP_PED_PROP)
		return EXTRAMETADATAMGR.SetupShopPedPropQuery((eScrCharacters)character, (eShopEnum)shop, locate, ANCHOR_NONE);
	else if (apparelType == SHOP_PED_OUTFIT)
		return EXTRAMETADATAMGR.SetupShopPedOutfitQuery((eScrCharacters)character, (eShopEnum)shop, locate);

	return 0;
}

s32 CommandSetupShopPedOutfitQuery(s32 character, s32 shop)
{
	return EXTRAMETADATAMGR.SetupShopPedOutfitQuery((eScrCharacters)character, (eShopEnum)shop, LOC_ANY);
}

void CommandGetShopPedQueryComponent(s32 index, scrShopPedComponent* itemInfo)
{
	if (ShopPedComponent* queryComp = EXTRAMETADATAMGR.GetShopPedQueryComponent(index))
		CopyShopPedComponentToScr(itemInfo, queryComp, EXTRAMETADATAMGR.GetShopPedApparelCharacter(queryComp->m_uniqueNameHash));
}

s32 CommandGetShopPedQueryComponentIndex(s32 nameHash)
{
	return EXTRAMETADATAMGR.GetShopPedQueryComponentIndex(nameHash);
}

void CommandGetShopPedComponent(s32 hashName, scrShopPedComponent* itemInfo)
{
	if (ShopPedComponent* queryComp = EXTRAMETADATAMGR.GetShopPedComponent((u32)hashName))
		CopyShopPedComponentToScr(itemInfo, queryComp, EXTRAMETADATAMGR.GetShopPedApparelCharacter(queryComp->m_uniqueNameHash));
}

void CommandGetShopPedQueryProp(s32 index, scrShopPedProp* itemInfo)
{
	if (ShopPedProp* queryProp = EXTRAMETADATAMGR.GetShopPedQueryProp(index))
		CopyShopPedPropToScr(itemInfo, queryProp, EXTRAMETADATAMGR.GetShopPedApparelCharacter(queryProp->m_uniqueNameHash));
}

s32 CommandGetShopPedQueryPropIndex(s32 nameHash)
{
	return EXTRAMETADATAMGR.GetShopPedQueryPropIndex(nameHash);
}

void CommandGetShopPedProp(s32 hashName, scrShopPedProp* itemInfo)
{
	if (ShopPedProp* queryProp = EXTRAMETADATAMGR.GetShopPedProp((u32)hashName))
		CopyShopPedPropToScr(itemInfo, queryProp, EXTRAMETADATAMGR.GetShopPedApparelCharacter(queryProp->m_uniqueNameHash));
}

void CommandGetShopPedQueryOutfit(s32 index, scrShopPedOutfit* itemInfo)
{
	if (ShopPedOutfit* queryOutfit = EXTRAMETADATAMGR.GetShopPedQueryOutfit(index))
		CopyShopPedOutfitToScr(itemInfo, queryOutfit, EXTRAMETADATAMGR.GetShopPedApparelCharacter(queryOutfit->m_uniqueNameHash));
}

void CommandGetShopPedOutfit(s32 hashName, scrShopPedOutfit* itemInfo)
{
	if (ShopPedOutfit* queryOutfit = EXTRAMETADATAMGR.GetShopPedOutfit((u32)hashName))
		CopyShopPedOutfitToScr(itemInfo, queryOutfit, EXTRAMETADATAMGR.GetShopPedApparelCharacter(queryOutfit->m_uniqueNameHash));
}

int CommandGetShopPedOutfitLocate(s32 hashName)
{
	if (ShopPedOutfit* queryOutfit = EXTRAMETADATAMGR.GetShopPedOutfit((u32)hashName))
		return static_cast<int>(queryOutfit->m_locate);
	return -1;
}

int CommandGetShopPedOutfitComponent(int nameHash, int componentIndex)
{
	ShopPedOutfit* outfit = EXTRAMETADATAMGR.GetShopPedOutfit(nameHash);
	if(scriptVerifyf(outfit,"%s : Outfit with the name hash %d not found",CTheScripts::GetCurrentScriptNameAndProgramCounter(),nameHash))
	{
		atArray<ComponentDescription>& componentArray = outfit->m_includedPedComponents;
			if(	scriptVerifyf(componentIndex>=0&&componentIndex<componentArray.GetCount(),
				"%s : Component index out of bounds: Input: '%d', Number of components: '%d'", 
				CTheScripts::GetCurrentScriptNameAndProgramCounter(),componentIndex, componentArray.GetCount()))
			{
				return componentArray[componentIndex].m_nameHash;
			}
	}
	return -1;
}

int CommandGetShopPedOutfitProp(int nameHash, int propIndex)
{
	ShopPedOutfit* outfit = EXTRAMETADATAMGR.GetShopPedOutfit(nameHash);
	if(scriptVerifyf(outfit,"%s : Outfit with the name hash %d not found",CTheScripts::GetCurrentScriptNameAndProgramCounter(),nameHash))
	{
		atArray<PropDescription>& propArray = outfit->m_includedPedProps;
			if(	scriptVerifyf(propIndex>=0&&propIndex<propArray.GetCount(),
				"%s : Component index out of bounds: Input: '%d', Number of components: '%d'", 
				CTheScripts::GetCurrentScriptNameAndProgramCounter(),propIndex, propArray.GetCount()))
			{
				return propArray[propIndex].m_nameHash;
			}
	}
	return -1;
}

bool CommandGetShopPedOutfitComponentVariant(int nameHash, int componentIndex, scrOutfitCompStruct* itemInfo)
{
	ShopPedOutfit* outfit = EXTRAMETADATAMGR.GetShopPedOutfit(nameHash);
	if(scriptVerifyf(outfit,"%s : Outfit with the name hash %d not found",CTheScripts::GetCurrentScriptNameAndProgramCounter(),nameHash))
	{
		atArray<ComponentDescription>& componentArray = outfit->m_includedPedComponents;
		if(	scriptVerifyf(componentIndex>=0&&componentIndex<componentArray.GetCount(),
			"%s : Component index out of bounds: Input: '%d', Number of components: '%d'", 
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),componentIndex, componentArray.GetCount()))
		{
			itemInfo->nameHash.Int = componentArray[componentIndex].m_nameHash;
			itemInfo->enumValue.Int = componentArray[componentIndex].m_enumValue;
			itemInfo->eCompType.Int = (int)componentArray[componentIndex].m_eCompType;
			return true;
		}
	}
	return false;
}

bool CommandGetShopPedOutfitPropVariant(int nameHash, int propIndex, scrOutfitPropStruct* itemInfo)
{
	ShopPedOutfit* outfit = EXTRAMETADATAMGR.GetShopPedOutfit(nameHash);
	if(scriptVerifyf(outfit,"%s : Outfit with the name hash %d not found",CTheScripts::GetCurrentScriptNameAndProgramCounter(),nameHash))
	{
		atArray<PropDescription>& propArray = outfit->m_includedPedProps;
		if(	scriptVerifyf(propIndex>=0&&propIndex<propArray.GetCount(),
			"%s : Component index out of bounds: Input: '%d', Number of props: '%d'", 
			CTheScripts::GetCurrentScriptNameAndProgramCounter(),propIndex, propArray.GetCount()))
		{
			itemInfo->nameHash.Int = propArray[propIndex].m_nameHash;
			itemInfo->enumValue.Int = propArray[propIndex].m_enumValue;
			itemInfo->anchorPoint.Int = (int)propArray[propIndex].m_eAnchorPoint;
			return true;
		}
	}
	return false;
}

int CommandGetHashNameForComponent(s32 pedIndex, s32 componentType, s32 drawableIndex, s32 textureIndex)
{
	return EXTRAMETADATAMGR.GetHashNameForComponent(pedIndex, (ePedVarComp)componentType, (u32)drawableIndex, (u32)textureIndex);
}

int CommandGetHashNameForProp(s32 pedIndex, s32 anchorPoint, s32 propIndex, s32 textureIndex)
{
	return EXTRAMETADATAMGR.GetHashNameForProp(pedIndex, (eAnchorPoints)anchorPoint, (u32)propIndex, (u32)textureIndex);
}

int CommandGetShopPedApparelVariantPropsCount(s32 nameHash)
{
	return EXTRAMETADATAMGR.GetShopPedApparelVariantPropsCount((u32)nameHash);
}

int CommandGetShopPedApparelVariantComponentsCount(s32 nameHash)
{
	return EXTRAMETADATAMGR.GetShopPedApparelVariantComponentsCount((u32)nameHash);
}

int CommandGetShopPedApparelForcedComponentsCount(s32 nameHash)
{
	return EXTRAMETADATAMGR.GetShopPedApparelForcedComponentsCount((u32)nameHash);
}

int CommandGetShopPedApparelForcedPropsCount(s32 nameHash)
{
	return EXTRAMETADATAMGR.GetShopPedApparelForcedPropsCount((u32)nameHash);
}

void CommandGetForcedComponent(s32 nameHash, s32 forcedComponentIndex, s32& forcedComponentNameHash, s32& forcedComponentEnumValue, s32& forcedComponentType)
{
	EXTRAMETADATAMGR.GetForcedComponent((u32)nameHash, (u32)forcedComponentIndex, forcedComponentNameHash, forcedComponentEnumValue, forcedComponentType);
}

void CommandGetForcedProp(s32 nameHash, s32 forcedPropIndex, s32& forcedPropNameHash, s32& forcedPropEnumValue, s32& forcedPropAnchor)
{
	EXTRAMETADATAMGR.GetForcedProp((u32)nameHash, (u32)forcedPropIndex, forcedPropNameHash, forcedPropEnumValue, forcedPropAnchor);
}

void CommandGetVariantComponent(s32 nameHash, s32 variantComponentIndex, s32& variantComponentNameHash, s32& variantComponentEnumValue, s32& variantComponentType)
{
	EXTRAMETADATAMGR.GetVariantComponent((u32)nameHash, (u32)variantComponentIndex, variantComponentNameHash, variantComponentEnumValue, variantComponentType);
}

void CommandGetVariantProp(s32 nameHash, s32 variantPropIndex, s32& variantPropNameHash, s32& variantPropEnumValue, s32& variantPropAnchor)
{
	EXTRAMETADATAMGR.GetVariantProp((u32)nameHash, (u32)variantPropIndex, variantPropNameHash, variantPropEnumValue, variantPropAnchor);
}

bool CommandDoesShopPedApparelHaveRestrictionTag(int nameHash, int tagHash, s32 /*apparelType*/)
{
	return EXTRAMETADATAMGR.DoesShopPedApparelHaveRestrictionTag((u32)nameHash, (u32)tagHash);
}

bool CommandDoesCurrentPedComponentHaveRestrictionTag(int pedIndex, int componentId, int tagHash)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if (pPed)
	{
		u32 drawableIndex = CPedVariationPack::GetCompVar(pPed, (ePedVarComp)componentId);
		u32 textureIndex = CPedVariationPack::GetTexVar(pPed, (ePedVarComp)componentId);
		u32 componentHash = EXTRAMETADATAMGR.GetHashNameForComponent(pedIndex, (ePedVarComp)componentId, drawableIndex, textureIndex);
		return EXTRAMETADATAMGR.DoesShopPedApparelHaveRestrictionTag(componentHash, (u32)tagHash);
	}
	return false;
}

bool CommandDoesCurrentPedPropHaveRestrictionTag(int pedIndex, int position, int tagHash)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex);
	if (pPed)
	{
		u32 propIndex = CPedPropsMgr::GetPedPropIdx(pPed, (eAnchorPoints)position);
		u32 textureIndex = CPedPropsMgr::GetPedPropTexIdx(pPed, (eAnchorPoints)position);
		u32 componentHash = EXTRAMETADATAMGR.GetHashNameForProp(pedIndex, (eAnchorPoints)position, propIndex, textureIndex);
		return EXTRAMETADATAMGR.DoesShopPedApparelHaveRestrictionTag(componentHash, (u32)tagHash);
	}
	return false;
}

#if __EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
void CommandSetBitShopPedApparel(int nameHash, int bitFlag, s32 /*apparelType*/)
{
	EXTRAMETADATAMGR.SetBitShopPedApparel(nameHash, bitFlag);
}

bool CommandIsBitSetShopPedApparel(int nameHash, int bitFlag, s32 /*apparelType*/)
{
	return EXTRAMETADATAMGR.IsBitSetShopPedApparel(nameHash, bitFlag);
}

void CommandClearBitShopPedApparel(int nameHash, int bitFlag, s32 /*apparelType*/)
{
	EXTRAMETADATAMGR.ClearBitShopPedApparel(nameHash, bitFlag);
}
#else	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS
void CommandSetBitShopPedApparel(int UNUSED_PARAM(nameHash), int UNUSED_PARAM(bitFlag), s32 /*apparelType*/)
{
	scriptAssertf(0, "%s : SET_BIT_SHOP_PED_APPAREL has been deprecated", CTheScripts::GetCurrentScriptNameAndProgramCounter());
}

bool CommandIsBitSetShopPedApparel(int UNUSED_PARAM(nameHash), int UNUSED_PARAM(bitFlag), s32 /*apparelType*/)
{
	scriptAssertf(0, "%s : IS_BIT_SET_SHOP_PED_APPAREL has been deprecated", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	return false;
}

void CommandClearBitShopPedApparel(int UNUSED_PARAM(nameHash), int UNUSED_PARAM(bitFlag), s32 /*apparelType*/)
{
	scriptAssertf(0, "%s : CLEAR_BIT_SHOP_PED_APPAREL has been deprecated", CTheScripts::GetCurrentScriptNameAndProgramCounter());
}
#endif	//	__EXTRA_METADATA_CONTAINS_SCRIPT_SAVE_DATA_FLAGS

#if !RSG_ORBIS
#pragma endregion

#pragma region WeaponShop
#endif


int CommandGetNumDLCWeapons()
{
    return EXTRAMETADATAMGR.GetNumDLCWeapons();
}

int CommandGetNumDLCWeaponsSP()
{
	return EXTRAMETADATAMGR.GetNumDLCWeaponsSP();
}

int CommandGetNumDLCWeaponComponents(int index)
{
	return EXTRAMETADATAMGR.GetNumDLCWeaponComponents(index);
}

int CommandGetNumDLCWeaponComponentsSP(int index)
{
	return EXTRAMETADATAMGR.GetNumDLCWeaponComponentsSP(index);
}

bool CommandGetDLCWeaponData(int index,  scrShopWeaponData* out_Values)
{
    const WeaponShopItem* pWeaponItem = EXTRAMETADATAMGR.GetDLCWeaponShopItem(index);
    if (scriptVerifyf(pWeaponItem, "GET_WEAPON_SHOP_DLC_ITEM_DATA - pWeaponItem is NULL for idx [%d]", index))
    {			
		out_Values->m_lockHash.Int		= pWeaponItem->m_lockHash;
		out_Values->m_nameHash.Int		= pWeaponItem->m_nameHash;
        out_Values->id.Int				= pWeaponItem->m_nameHash;
        out_Values->cost.Int			= pWeaponItem->m_cost;
		out_Values->ammoCost.Int		= pWeaponItem->m_ammoCost;
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeaponItem->m_nameHash);	
		if(scriptVerifyf(pWeaponInfo,"WeaponInfo is NULL for weapon : %s",pWeaponItem->m_nameHash.TryGetCStr()))
		{
			out_Values->ammoType.Int		= pWeaponInfo->GetAmmoInfo()? pWeaponInfo->GetAmmoInfo()->GetHash() : 0;
			out_Values->defaultClipSize.Int = pWeaponInfo ? pWeaponInfo->GetClipSize() : 0;
		}
		safecpy(out_Values->label, pWeaponItem->m_textLabel.c_str());
		safecpy(out_Values->weaponDesc, pWeaponItem->m_weaponDesc.c_str());
		safecpy(out_Values->weaponTT, pWeaponItem->m_weaponTT.c_str());	
		safecpy(out_Values->weaponUppercase, pWeaponItem->m_weaponUppercase.c_str());	

        return true;
    }

    return false;
}

bool CommandGetDLCWeaponDataSP(int index, scrShopWeaponData* out_Values)
{
	 s32 uIndexOfSPWeapon = EXTRAMETADATAMGR.GetIndexForSPWeapons(index);

	if (uIndexOfSPWeapon >= 0)
	{
		return CommandGetDLCWeaponData(uIndexOfSPWeapon, out_Values);
	}

	return false;
}

bool CommandGetDLCWeaponComponentData(int weaponDlcIdx, int compIdx, scrShopWeaponComponentData* out_Values)
{
	const ShopWeaponComponent* component = EXTRAMETADATAMGR.GetShopWeaponComponentData(weaponDlcIdx,compIdx);
	const WeaponShopItem* weapon = EXTRAMETADATAMGR.GetDLCWeaponShopItem(weaponDlcIdx);
	if(scriptVerifyf(component,"GET_SHOP_DLC_WEAPON_COMPONENT_DATA - %s component is NULL for dlcIdx [%d] compIdx [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), weaponDlcIdx, compIdx))
	{
		const CWeaponComponentInfo* compInfo =  CWeaponComponentManager::GetInfo<CWeaponComponentInfo>(component->m_componentName);
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weapon->m_nameHash);
		const CWeaponComponentPoint* compPoint = NULL;
		if(pWeaponInfo&&compInfo)
		{
			compPoint = pWeaponInfo->GetAttachPoint(compInfo);
		}
		out_Values->m_componentName.Int = component->m_componentName;
		out_Values->m_cost.Int			= component->m_cost;
		out_Values->m_id.Int			= component->m_id;
		out_Values->m_lockHash.Int		= component->m_lockHash;
		out_Values->m_ModType.Int		= compPoint? compPoint->GetAttachBoneHash(): 0;
		out_Values->m_isDefault.Int		= pWeaponInfo ? pWeaponInfo->GetIsWeaponComponentDefault(component->m_componentName):false;
		safecpy(out_Values->m_textLabel, component->m_textLabel.c_str());
		safecpy(out_Values->m_componentDesc, component->m_componentDesc.c_str());
		return true;
	}
	return false;
}

bool CommandGetDLCWeaponComponentDataSP(int weaponDlcIdx, int compIdx, scrShopWeaponComponentData* out_Values)
{
	s32 uIndexOfSPWeapon = EXTRAMETADATAMGR.GetIndexForSPWeapons(weaponDlcIdx);

	if (uIndexOfSPWeapon >= 0)
	{
		return CommandGetDLCWeaponComponentData(weaponDlcIdx, compIdx, out_Values);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
///	Content changesets
//////////////////////////////////////////////////////////////////////////
void CommandExecuteContentChangeSet(s32 contentHash, s32 changeSetGroup, s32 changeSetName)
{
	EXTRACONTENT.ExecuteContentChangeSet(contentHash, changeSetGroup, changeSetName);
}

void CommandRevertContentChangeSet(s32 contentHash, s32 changeSetGroup, s32 changeSetName)
{
	EXTRACONTENT.RevertContentChangeSet(contentHash, changeSetGroup, changeSetName);
}

void CommandExecuteContentChangeSetGroup(s32 contentHash, s32 changeSetGroup)
{
	EXTRACONTENT.ExecuteContentChangeSetGroup(contentHash, changeSetGroup);
}

void CommandRevertContentChangeSetGroup(s32 contentHash, s32 changeSetGroup)
{
	EXTRACONTENT.RevertContentChangeSetGroup(contentHash, changeSetGroup);
}

void CommandExecuteContentChangeSetGroupForAll(s32 changeSetGroup)
{
	EXTRACONTENT.ExecuteContentChangeSetGroupForAll(changeSetGroup);
}

void CommandRevertContentChangeSetGroupForAll(s32 changeSetGroup)
{
	EXTRACONTENT.RevertContentChangeSetGroupForAll(changeSetGroup);
}
//////////////////////////////////////////////////////////////////////////

#if !RSG_ORBIS
#pragma endregion
#endif

bool CommandHaveShopContentRightsChanged(int type)
{
	return EXTRAMETADATAMGR.GetShopContentRightsChanged((eContentRightsTypes)type);
}

void CommandResetShopContentRightsChanged(int type)
{
	EXTRAMETADATAMGR.ResetShopContentRightsChanged((eContentRightsTypes)type);
}

bool CommandIsContentItemLocked(int lockHash)
{
	return EXTRACONTENT.IsContentItemLocked((u32)lockHash);		
}

//
// name:		SetupScriptCommands
// description:	Setup extra metadata script commands
void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(HAVE_SHOP_CONTENT_RIGHTS_CHANGED,0x1dfb7045135146e5, CommandHaveShopContentRightsChanged);
	SCR_REGISTER_UNUSED(RESET_SHOP_CONTENT_RIGHTS_CHANGED,0xff08664f971c8226, CommandResetShopContentRightsChanged);

	SCR_REGISTER_SECURE(GET_NUM_TATTOO_SHOP_DLC_ITEMS,0x883e6f2863a2c982, CommandGetNumTattooShopItems);
	SCR_REGISTER_SECURE(GET_TATTOO_SHOP_DLC_ITEM_DATA,0x7a1b82e97e3bd5be, CommandGetTattooShopItemData);
	SCR_REGISTER_SECURE(GET_TATTOO_SHOP_DLC_ITEM_INDEX,0x35f01d0bcdf83b24, CommandGetTattooShopItemIndex);

	SCR_REGISTER_SECURE(INIT_SHOP_PED_COMPONENT,0xb5abdb2fa64e87f2, CommandInitShopPedComponent);
	SCR_REGISTER_SECURE(INIT_SHOP_PED_PROP,0x1cf199d23d4dfae4, CommandInitShopPedProp);
	SCR_REGISTER_SECURE(SETUP_SHOP_PED_APPAREL_QUERY,0xd58129e68c118090, CommandSetupShopPedApparelQuery);
	SCR_REGISTER_SECURE(SETUP_SHOP_PED_APPAREL_QUERY_TU,0xa188d1127a77c942, CommandSetupShopPedApparelQueryTU);
	SCR_REGISTER_SECURE(GET_SHOP_PED_QUERY_COMPONENT,0xea030ac498b5f181, CommandGetShopPedQueryComponent);
	SCR_REGISTER_SECURE(GET_SHOP_PED_QUERY_COMPONENT_INDEX,0x8f3f503bada161ed, CommandGetShopPedQueryComponentIndex);
	SCR_REGISTER_SECURE(GET_SHOP_PED_COMPONENT,0x37e8462bc2a63393, CommandGetShopPedComponent);
	SCR_REGISTER_SECURE(GET_SHOP_PED_QUERY_PROP,0xc99ce91134872039, CommandGetShopPedQueryProp);
	SCR_REGISTER_SECURE(GET_SHOP_PED_QUERY_PROP_INDEX,0x971016887dc14ced, CommandGetShopPedQueryPropIndex);
	SCR_REGISTER_SECURE(GET_SHOP_PED_PROP,0x46bb251c1291e1ba, CommandGetShopPedProp);

	SCR_REGISTER_SECURE(GET_HASH_NAME_FOR_COMPONENT,0x748ef68a43bbbc6c, CommandGetHashNameForComponent);
	SCR_REGISTER_SECURE(GET_HASH_NAME_FOR_PROP,0x7abbacadfb201c3a, CommandGetHashNameForProp);
	SCR_REGISTER_SECURE(GET_SHOP_PED_APPAREL_VARIANT_COMPONENT_COUNT,0x9dcc12e44dda0a36, CommandGetShopPedApparelVariantComponentsCount);
	SCR_REGISTER_SECURE(GET_SHOP_PED_APPAREL_VARIANT_PROP_COUNT,0xda897eb65abfc365, CommandGetShopPedApparelVariantPropsCount);
	SCR_REGISTER_SECURE(GET_VARIANT_COMPONENT,0x0225613f2ac9e3c8, CommandGetVariantComponent);
	SCR_REGISTER_SECURE(GET_VARIANT_PROP,0xead0175c012d1ca4, CommandGetVariantProp);
	SCR_REGISTER_SECURE(GET_SHOP_PED_APPAREL_FORCED_COMPONENT_COUNT,0x2c13d8eab528ce21, CommandGetShopPedApparelForcedComponentsCount);
	SCR_REGISTER_SECURE(GET_SHOP_PED_APPAREL_FORCED_PROP_COUNT,0x2abaebbc719ed4fe, CommandGetShopPedApparelForcedPropsCount);
	SCR_REGISTER_SECURE(GET_FORCED_COMPONENT,0xd58956643d657992, CommandGetForcedComponent);
	SCR_REGISTER_SECURE(GET_FORCED_PROP,0xdd2207d8f05a54a3, CommandGetForcedProp);
	SCR_REGISTER_SECURE(DOES_SHOP_PED_APPAREL_HAVE_RESTRICTION_TAG,0x036d1ea7243f2ccc, CommandDoesShopPedApparelHaveRestrictionTag);

	SCR_REGISTER_SECURE(DOES_CURRENT_PED_COMPONENT_HAVE_RESTRICTION_TAG, 0x7796b21b76221bc5, CommandDoesCurrentPedComponentHaveRestrictionTag);
	SCR_REGISTER_SECURE(DOES_CURRENT_PED_PROP_HAVE_RESTRICTION_TAG, 0xd726bab4554da580, CommandDoesCurrentPedPropHaveRestrictionTag);

	SCR_REGISTER_UNUSED(SET_BIT_SHOP_PED_APPAREL,0x25ef2afe569ef782, CommandSetBitShopPedApparel);
	SCR_REGISTER_UNUSED(IS_BIT_SET_SHOP_PED_APPAREL,0x933dec9b53969ffe, CommandIsBitSetShopPedApparel);
	SCR_REGISTER_UNUSED(CLEAR_BIT_SHOP_PED_APPAREL,0x59bda04c2d87125d, CommandClearBitShopPedApparel);

	SCR_REGISTER_SECURE(SETUP_SHOP_PED_OUTFIT_QUERY,0x30264a1c9adaae62, CommandSetupShopPedOutfitQuery);
	SCR_REGISTER_SECURE(GET_SHOP_PED_QUERY_OUTFIT,0x43ffb630e515d7a1, CommandGetShopPedQueryOutfit);
	SCR_REGISTER_SECURE(GET_SHOP_PED_OUTFIT,0x24dfba133538eaec, CommandGetShopPedOutfit);
	SCR_REGISTER_UNUSED(GET_SHOP_PED_OUTFIT_PROP,0x7a3888c090e2a616, CommandGetShopPedOutfitProp);
	SCR_REGISTER_SECURE(GET_SHOP_PED_OUTFIT_LOCATE,0xb3ac49cb24f59ed4, CommandGetShopPedOutfitLocate);
	SCR_REGISTER_UNUSED(GET_SHOP_PED_OUTFIT_COMPONENT,0x7b77c31c6e50678f, CommandGetShopPedOutfitComponent);
	SCR_REGISTER_SECURE(GET_SHOP_PED_OUTFIT_PROP_VARIANT,0xe0a9b7f977ebd7af, CommandGetShopPedOutfitPropVariant);
	SCR_REGISTER_SECURE(GET_SHOP_PED_OUTFIT_COMPONENT_VARIANT,0xa5404cdf63badf48, CommandGetShopPedOutfitComponentVariant);

	SCR_REGISTER_SECURE(GET_NUM_DLC_VEHICLES,0x63913871ecc3707e, CommandGetNumDLCVehicles);
	SCR_REGISTER_SECURE(GET_DLC_VEHICLE_MODEL,0x6f4ef840847d6dab, CommandGetDLCVehicleModel);
	SCR_REGISTER_SECURE(GET_DLC_VEHICLE_DATA,0xcd58de0b13d04118, CommandGetDLCVehicleData);
	SCR_REGISTER_SECURE(GET_DLC_VEHICLE_FLAGS,0x05cc5402fda31300, CommandGetDLCVehicleFlags);

	SCR_REGISTER_UNUSED(GET_NUM_DLC_VEHICLE_MODS,0xd5edccf2f3a5b664, CommandGetNumDLCVehicleMods);
	SCR_REGISTER_UNUSED(GET_DLC_VEHICLE_MOD_DATA,0xced26b1d82b4f831, CommandGetDLCVehicleModData);

    SCR_REGISTER_SECURE(GET_NUM_DLC_WEAPONS,0x501053ebab36db66, CommandGetNumDLCWeapons);
	SCR_REGISTER_SECURE(GET_NUM_DLC_WEAPONS_SP,0xeff3ecb899fc93ac, CommandGetNumDLCWeaponsSP);
	SCR_REGISTER_SECURE(GET_DLC_WEAPON_DATA,0x4cd88d794e108bec, CommandGetDLCWeaponData);
	SCR_REGISTER_SECURE(GET_DLC_WEAPON_DATA_SP,0x75bacf95335672b8, CommandGetDLCWeaponDataSP);
    SCR_REGISTER_SECURE(GET_NUM_DLC_WEAPON_COMPONENTS,0x8c780bef2d6db238, CommandGetNumDLCWeaponComponents);
	SCR_REGISTER_SECURE(GET_NUM_DLC_WEAPON_COMPONENTS_SP,0xa67aea8bbdc78f33, CommandGetNumDLCWeaponComponentsSP);
	SCR_REGISTER_SECURE(GET_DLC_WEAPON_COMPONENT_DATA,0x45f755b731a742d2, CommandGetDLCWeaponComponentData);	
	SCR_REGISTER_SECURE(GET_DLC_WEAPON_COMPONENT_DATA_SP,0xd6677a8863dc6340, CommandGetDLCWeaponComponentDataSP);

	SCR_REGISTER_SECURE(IS_CONTENT_ITEM_LOCKED,0x1b5c8ee302fc0d1e, CommandIsContentItemLocked);
	SCR_REGISTER_SECURE(IS_DLC_VEHICLE_MOD,0xd624ba89d119abd9,CommandIsDlcVehicleMod);
	SCR_REGISTER_SECURE(GET_DLC_VEHICLE_MOD_LOCK_HASH,0x3d7b86f100512881, CommandGetDLCVehicleModLockHash);

	// --- DLC Scripts -----------------------------------------------------------------------------
	SCR_REGISTER_UNUSED(EXECUTE_CONTENT_CHANGESET,0x6dbfcee18f70ff9b,  CommandExecuteContentChangeSet);
	SCR_REGISTER_UNUSED(REVERT_CONTENT_CHANGESET,0x67f40fc4e8c80e77,  CommandRevertContentChangeSet);
	SCR_REGISTER_UNUSED(EXECUTE_CONTENT_CHANGESET_GROUP,0xb1d68580bdbc7484,  CommandExecuteContentChangeSetGroup);
	SCR_REGISTER_UNUSED(REVERT_CONTENT_CHANGESET_GROUP,0xc7c82b4c73e95908,  CommandRevertContentChangeSetGroup);
	SCR_REGISTER_SECURE(EXECUTE_CONTENT_CHANGESET_GROUP_FOR_ALL,0xec1d16396ec19654, CommandExecuteContentChangeSetGroupForAll);
	SCR_REGISTER_SECURE(REVERT_CONTENT_CHANGESET_GROUP_FOR_ALL,0x26908af0bdf2030d, CommandRevertContentChangeSetGroupForAll);
}

}; //namespace extrametadata_commands
