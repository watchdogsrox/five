// 
// script/commands_itemsets.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "commands_itemsets.h"
#include "fwscript/scriptguid.h"
#include "script_helper.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/wrapper.h"

namespace itemsets_commands
{
	int CommandCreateItemSet(bool autoClean)
	{
		CScriptResource_ItemSet newSet(autoClean);
		return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(newSet);
	}

	void CommandDestroyItemSet(int objSetGuid)
	{
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_ITEMSET, objSetGuid);
	}

	bool CommandIsItemSetValid(int objSetGuid)
	{
		const CItemSet* pObjSet = CTheScripts::GetItemSetToQueryFromGUID(objSetGuid, "IS_ITEMSET_VALID", 0);
		return pObjSet != NULL;
	}

	bool CommandAddToItemSet(int objToAddGuid, int objSetGuid)
	{
		CItemSet* pObjSet = CTheScripts::GetItemSetToModifyFromGUID(objSetGuid, "ADD_TO_ITEMSET");
		if(!pObjSet)
		{
			return false;
		}

		/*const*/ fwExtensibleBase* pObjToAdd = CTheScripts::GetExtensibleBaseToModifyFromGUID(objToAddGuid, "ADD_TO_ITEMSET");
		if(!pObjToAdd)
		{
			return false;
		}

		return pObjSet->AddEntityToSet(*pObjToAdd);
	}

	void CommandRemoveFromItemSet(int objToRemoveGuid, int objSetGuid)
	{
		CItemSet* pObjSet = CTheScripts::GetItemSetToModifyFromGUID(objSetGuid, "REMOVE_FROM_ITEMSET");
		if(!pObjSet)
		{
			return;// false;
		}

		/*const*/ fwExtensibleBase* pObjToRemove = CTheScripts::GetExtensibleBaseToModifyFromGUID(objToRemoveGuid, "REMOVE_FROM_ITEMSET");
		if(!pObjToRemove)
		{
			return;// false;
		}

		scriptVerifyf(pObjSet->RemoveEntityFromSet(*pObjToRemove), "%s:Failed to remove entity from set.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	int CommandGetItemSetSize(int objSetGuid)
	{
		CItemSet* pObjSet = CTheScripts::GetItemSetToModifyFromGUID(objSetGuid, "GET_ITEMSET_SIZE");
		if(pObjSet)
		{
			return pObjSet->GetSetSize();
		}
		return 0;
	}

	int CommandGetIndexedItemInItemSet(int indexOfEntity, int objSetGuid)
	{
		const CItemSet* pObjSet = CTheScripts::GetItemSetToQueryFromGUID(objSetGuid, "GET_INDEXED_ITEM_IN_ITEMSET");
		if(pObjSet)
		{
			fwExtensibleBase* pObj = pObjSet->GetEntity(indexOfEntity);
			// Note: if not using the auto-clean option, or if using it and then deleting
			// something after the call to GET_ITEMSET_SIZE, it's possible for these pointers
			// to be NULL, which we don't consider an error here.
			if(pObj)
			{
				return CTheScripts::GetGUIDFromExtensibleBase(*pObj);
			}
		}
		return 0;
	}

	bool CommandIsInItemSet(int objToCheckGuid, int objSetGuid)
	{
		const CItemSet* pObjSet = CTheScripts::GetItemSetToQueryFromGUID(objSetGuid);
		if(pObjSet)
		{
			const fwExtensibleBase* pObjToCheck = CTheScripts::GetExtensibleBaseToQueryFromGUID(objToCheckGuid);
			if(pObjToCheck)
			{
				return pObjSet->IsEntityInSet(*pObjToCheck);
			}
		}
		return false;
	}

	void CommandCleanItemSet(int objSetGuid)
	{
		CItemSet* pObjSet = CTheScripts::GetItemSetToModifyFromGUID(objSetGuid);
		if(pObjSet)
		{
			pObjSet->Clean();
		}
	}

	bool CommandIsAnItemSet(int objGuid)
	{
		fwExtensibleBase* pObj = fwScriptGuid::GetBaseFromGuid(objGuid);

		// Note: calling fwScriptGuid::FromGuid() fails an assert if it's not the type
		// we're checking for, which is undesirable here. Maybe we should make this a different
		// template function in fwScriptGuid, though.
		return pObj && pObj->GetIsClassId(CItemSet::GetStaticClassId());
	}

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(CREATE_ITEMSET,0x5dba089fcc1e8441,					CommandCreateItemSet);
		SCR_REGISTER_SECURE(DESTROY_ITEMSET,0xf8787973a530cd9d,				CommandDestroyItemSet);
		SCR_REGISTER_SECURE(IS_ITEMSET_VALID,0x8cc4f96fbf3cfbe3,				CommandIsItemSetValid);

		SCR_REGISTER_SECURE(ADD_TO_ITEMSET,0x4ada1f27dc259a08,					CommandAddToItemSet);
		SCR_REGISTER_SECURE(REMOVE_FROM_ITEMSET,0x4e49d01105a580ba,			CommandRemoveFromItemSet);
		SCR_REGISTER_SECURE(GET_ITEMSET_SIZE,0x3f0236a455cd21fb,				CommandGetItemSetSize);
		SCR_REGISTER_SECURE(GET_INDEXED_ITEM_IN_ITEMSET,0x5d8dd9724918ffbd,	CommandGetIndexedItemInItemSet);
		SCR_REGISTER_SECURE(IS_IN_ITEMSET,0xab829f8e88b23382,					CommandIsInItemSet);
		SCR_REGISTER_SECURE(CLEAN_ITEMSET,0xbfa82d057e04ca9e,					CommandCleanItemSet);
		SCR_REGISTER_UNUSED(IS_AN_ITEMSET,0x3841a80ccc00b54f,					CommandIsAnItemSet);
	}

}	// namespace itemsets_commands

// End of file script/commands_itemsets.cpp
