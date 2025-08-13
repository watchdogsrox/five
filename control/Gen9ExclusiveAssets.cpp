
#include "Gen9ExclusiveAssets.h"

// Rage headers
#include "parser/manager.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "modelinfo/VehicleModelInfoMods.h"
#include "Peds/Ped.h"
#include "Peds/rendering/PedVariationPack.h"


// Static variables
Gen9ExclusiveAssetsDataPeds CGen9ExclusiveAssets::sm_Gen9ExclusiveAssetsDataPeds;
Gen9ExclusiveAssetsDataVehicles CGen9ExclusiveAssets::sm_Gen9ExclusiveAssetsDataVehicles;



void CGen9ExclusiveAssets::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		sm_Gen9ExclusiveAssetsDataPeds.m_PedData.Reset();
		sm_Gen9ExclusiveAssetsDataVehicles.m_VehicleData.Reset();
	}
}


void CGen9ExclusiveAssets::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		sm_Gen9ExclusiveAssetsDataPeds.m_PedData.Reset();
		sm_Gen9ExclusiveAssetsDataVehicles.m_VehicleData.Reset();
	}
}


void CGen9ExclusiveAssets::LoadGen9ExclusiveAssetsPedsData(const char* pFilename)
{
	PARSER.LoadObject(pFilename, "meta", sm_Gen9ExclusiveAssetsDataPeds);
}


void CGen9ExclusiveAssets::LoadGen9ExclusiveAssetsVehiclesData(const char* pFilename)
{
	PARSER.LoadObject(pFilename, "meta", sm_Gen9ExclusiveAssetsDataVehicles);
}


bool CGen9ExclusiveAssets::IsGen9ExclusivePedDrawable(const CPed* pPed, ePedVarComp component, u32 globalDrawableIdx)
{
	if (Verifyf(pPed, "CGen9ExclusiveAssets::IsGen9ExclusivePedDrawable - pPed is NULL"))
	{
		if (Verifyf(pPed->GetBaseModelInfo(), "CGen9ExclusiveAssets::IsGen9ExclusivePedDrawable - pPed->GetBaseModelInfo() returned NULL"))
		{
			const u32 modelNameHash = pPed->GetBaseModelInfo()->GetModelNameHash();

			const s32 numberOfPedModels = sm_Gen9ExclusiveAssetsDataPeds.m_PedData.GetCount();
			s32 pedModelIndex = 0;
			bool bFoundMatch = false;
			while ( (pedModelIndex < numberOfPedModels) && !bFoundMatch)
			{
				if (sm_Gen9ExclusiveAssetsDataPeds.m_PedData[pedModelIndex].m_PedModelName.GetHash() == modelNameHash)
				{
					bFoundMatch = true;
				}
				else
				{
					pedModelIndex++;
				}
			}

			if (bFoundMatch)
			{
				u32 dlcHash = 0;
				u32 localDrawableIndex = 0;
				CPedVariationPack::GetLocalCompData(pPed, component, globalDrawableIdx, dlcHash, localDrawableIndex);

				if (dlcHash > 0)
				{
					const atArray<PedComponentsForOneDLCPack>& dlcData = sm_Gen9ExclusiveAssetsDataPeds.m_PedData[pedModelIndex].m_DLCData;
					const s32 numberOfDlcPacks = dlcData.GetCount();
					for (s32 dlcLoop = 0; dlcLoop < numberOfDlcPacks; dlcLoop++)
					{
						if (dlcData[dlcLoop].m_dlcName.GetHash() == dlcHash)
						{
							const atArray<LocalDrawableIndicesForOnePedComponent>& componentArray = dlcData[dlcLoop].m_ComponentArray;
							const s32 numberOfComponents = componentArray.GetCount();

							if (numberOfComponents == 0)
							{
								// Special case - if gen9_exclusive_assets_peds.meta has been set up to have an empty ComponentArray for this DLC 
								//	then treat all its components as Gen9 Exclusive.
								return true;
							}
							else
							{
								for (s32 componentLoop = 0; componentLoop < numberOfComponents; componentLoop++)
								{
									if (componentArray[componentLoop].m_ComponentType == component)
									{
										const atArray<u32>& drawableIndicesArray = componentArray[componentLoop].m_LocalDrawableIndices;
										const s32 numberOfDrawableIndices = drawableIndicesArray.GetCount();
										for (s32 drawableLoop = 0; drawableLoop < numberOfDrawableIndices; drawableLoop++)
										{
											if (drawableIndicesArray[drawableLoop] == localDrawableIndex)
											{
												return true;
											}
										}
									}
								}
							}
						}
					}
				} // if (dlcHash > 0)
			} // bFoundMatch for ped model name hash
		} // if (Verifyf(pPed->GetBaseModelInfo()
	} // if (Verifyf(pPed

	return false;
}

bool CGen9ExclusiveAssets::IsGen9ExclusiveVehicleMod(const CVehicleKit* pVehicleModKit, eVehicleModType modSlot, u8 modIndex)
{
	if (modIndex == INVALID_MOD)
	{
		return false;
	}

	if (Verifyf(pVehicleModKit, "CGen9ExclusiveAssets::IsGen9ExclusiveVehicleMod - pVehicleModKit is NULL"))
	{
		bool bIsVisibleMod = true;
		if (modSlot < VMT_RENDERABLE)
		{
			if (modIndex < pVehicleModKit->GetVisibleMods().GetCount())
			{
				if (pVehicleModKit->GetVisibleMods()[modIndex].GetType() == modSlot)
				{
					bIsVisibleMod = true;
				}
				else
				{
					Assertf(0, "CGen9ExclusiveAssets::IsGen9ExclusiveVehicleMod - VisibleMods[%u] has type %d. Expected it to be %d", 
						modIndex, (s32)pVehicleModKit->GetVisibleMods()[modIndex].GetType(), (s32)modSlot);
					return false;
				}
			}
			else
			{
				Assertf(0, "CGen9ExclusiveAssets::IsGen9ExclusiveVehicleMod - modSlot=%d modIndex=%u GetVisibleMods().GetCount()=%d", (s32)modSlot, modIndex, pVehicleModKit->GetVisibleMods().GetCount());
				return false;
			}
		}
		else if (modSlot < VMT_TOGGLE_MODS)
		{
			if (modIndex < pVehicleModKit->GetStatMods().GetCount())
			{
				if (pVehicleModKit->GetStatMods()[modIndex].GetType() == modSlot)
				{
					bIsVisibleMod = false;
				}
				else
				{
					Assertf(0, "CGen9ExclusiveAssets::IsGen9ExclusiveVehicleMod - StatMods[%u] has type %d. Expected it to be %d", 
						modIndex, (s32)pVehicleModKit->GetStatMods()[modIndex].GetType(), (s32)modSlot);
					return false;
				}
			}
			else
			{
				Assertf(0, "CGen9ExclusiveAssets::IsGen9ExclusiveVehicleMod - modSlot=%d modIndex=%u GetStatMods().GetCount()=%d", (s32)modSlot, modIndex, pVehicleModKit->GetStatMods().GetCount());
				return false;
			}
		}
		else
		{
			return false;
		}

		const u32 kitNameHash = pVehicleModKit->GetNameHash();

		const s32 numberOfVehicleKits = sm_Gen9ExclusiveAssetsDataVehicles.m_VehicleData.GetCount();
		s32 kitIndex = 0;
		bool bFoundMatch = false;
		while ( (kitIndex < numberOfVehicleKits) && !bFoundMatch)
		{
			if (sm_Gen9ExclusiveAssetsDataVehicles.m_VehicleData[kitIndex].m_kitName.GetHash() == kitNameHash)
			{ // The Gen9ExclusiveAssetsData contains an entry for this kit
				bFoundMatch = true;
			}
			else
			{
				kitIndex++;
			}
		}

		if (bFoundMatch)
		{
			const VehicleModKitData& modKitData = sm_Gen9ExclusiveAssetsDataVehicles.m_VehicleData[kitIndex];

			if (bIsVisibleMod)
			{
				const atHashString &modNameHashString = pVehicleModKit->GetVisibleMods()[modIndex].GetNameHashString();

				const s32 numOfVisibleMods = modKitData.m_visibleMods.GetCount();
				s32 visModIndex = 0;
				bFoundMatch = false;
				while ( (visModIndex < numOfVisibleMods) && !bFoundMatch)
				{
					if (modKitData.m_visibleMods[visModIndex].m_modType == modSlot)
					{ // The Gen9ExclusiveAssetsData for the kit contains an entry for this modSlot
						bFoundMatch = true;
					}
					else
					{
						visModIndex++;
					}
				}

				if (bFoundMatch)
				{
					const atArray<atHashWithStringNotFinal>& modelNameArray = modKitData.m_visibleMods[visModIndex].m_modelNames;
					const s32 numOfModelNames = modelNameArray.GetCount();
					for (s32 modelNameLoop = 0; modelNameLoop < numOfModelNames; modelNameLoop++)
					{
						if (modelNameArray[modelNameLoop].GetHash() == modNameHashString.GetHash())
						{ // The Gen9ExclusiveAssetsData for the kit and modSlot contains an entry for the modelName we're checking so return true
							return true;
						}
					}
				}
			}
			else
			{
				// I think modIndex is a "global" index so I need to convert it back to a "local" index for the type of mod specified by modSlot
				// Count the number of entries of modSlot type that appear before modIndex in the array.
				u32 localIndex = 0;
				for (u8 i = 0; i < modIndex; i++)
				{
					if (pVehicleModKit->GetStatMods()[i].GetType() == modSlot)
					{
						localIndex++;
					}
				}

				const s32 numOfStatMods = modKitData.m_statMods.GetCount();
				s32 statModIndex = 0;
				bFoundMatch = false;
				while ( (statModIndex < numOfStatMods) && !bFoundMatch)
				{
					if (modKitData.m_statMods[statModIndex].m_modType == modSlot)
					{ // The Gen9ExclusiveAssetsData for the kit contains an entry for this modSlot
						bFoundMatch = true;
					}
					else
					{
						statModIndex++;
					}
				}

				if (bFoundMatch)
				{
					const atArray<u32>& indicesArray = modKitData.m_statMods[statModIndex].m_indices;
					const s32 numOfIndices = indicesArray.GetCount();
					for (s32 indicesLoop = 0; indicesLoop < numOfIndices; indicesLoop++)
					{
						if (indicesArray[indicesLoop] == localIndex)
						{ // The Gen9ExclusiveAssetsData for the kit and modSlot contains an entry for the local index we're checking so return true
							return true;
						}
					}
				}
			}
		} // bFoundMatch for kit name hash
	} // if pVehicleModKit

	return false;
}


#if RSG_ORBIS || RSG_DURANGO || RSG_PC
bool CGen9ExclusiveAssets::CanCreatePedDrawable(const CPed *pPed, ePedVarComp component, u32 globalDrawableIdx, bool ASSERT_ONLY(bAssert) )
{
	if (IsGen9ExclusivePedDrawable(pPed, component, globalDrawableIdx))
	{
		Assertf(!bAssert, "CGen9ExclusiveAssets::CanCreatePedDrawable - returning false for component=%d globalDrawableIdx=%u", (s32)component, globalDrawableIdx);
		return false;
	}

	return true;
}

bool CGen9ExclusiveAssets::CanCreateVehicleMod(const CVehicleKit* pVehicleModKit, eVehicleModType modSlot, u8 modIndex, bool ASSERT_ONLY(bAssert) )
{
	if (IsGen9ExclusiveVehicleMod(pVehicleModKit, modSlot, modIndex))
	{
#if __ASSERT
		if (bAssert)
		{
			const char *pKitName = "<unknown>";
			u32 kitNameHash = 0;
			if (pVehicleModKit)
			{
				if (pVehicleModKit->GetNameHashString().TryGetCStr())
				{
					pKitName = pVehicleModKit->GetNameHashString().TryGetCStr();
				}
				kitNameHash = pVehicleModKit->GetNameHashString().GetHash();
			}

			if (modSlot < VMT_RENDERABLE)
			{
				const char *pModSlotName = "<unknown>";
				if (pVehicleModKit && pVehicleModKit->GetModSlotName(modSlot))
				{
					pModSlotName = pVehicleModKit->GetModSlotName(modSlot);
				}

				const atHashString &modNameHashString = pVehicleModKit->GetVisibleMods()[modIndex].GetNameHashString();
				const char *pModName = "";
				if (modNameHashString.TryGetCStr())
				{
					pModName = modNameHashString.GetCStr();
				}

				Assertf(0, "CGen9ExclusiveAssets::CanCreateVehicleMod - returning false for visible mod - kit=%s(%x) modSlot=%s(%d) modName=%s modIndex=%u", 
					pKitName, kitNameHash, 
					pModSlotName, (s32)modSlot, 
					pModName, modIndex);
			}
			else if (modSlot < VMT_TOGGLE_MODS)
			{
				u32 localModIndex = 0;
				for (u8 i = 0; i < modIndex; i++)
				{
					if (pVehicleModKit->GetStatMods()[i].GetType() == modSlot)
					{
						localModIndex++;
					}
				}

				Assertf(0, "CGen9ExclusiveAssets::CanCreateVehicleMod - returning false for stat mod - kit=%s(%x) modSlot=%d localModIndex = %u globalModIndex=%u", 
					pKitName, kitNameHash, 
					(s32)modSlot,
					localModIndex, modIndex);
			}
		}
#endif // __ASSERT

		return false;
	}

	return true;
}

#else // RSG_ORBIS || RSG_DURANGO || RSG_PC

bool CGen9ExclusiveAssets::CanCreatePedDrawable(const CPed* UNUSED_PARAM(pPed), ePedVarComp UNUSED_PARAM(component), u32 UNUSED_PARAM(drawableIdx), bool UNUSED_PARAM(bAssert) )
{
	// Always return true on Gen9 platforms
	return true;
}

bool CGen9ExclusiveAssets::CanCreateVehicleMod(const CVehicleKit* UNUSED_PARAM(pVehicleModKit), eVehicleModType UNUSED_PARAM(modSlot), u8 UNUSED_PARAM(modIndex), bool UNUSED_PARAM(bAssert) )
{
	// Always return true on Gen9 platforms
	return true;
}

#endif // RSG_ORBIS || RSG_DURANGO || RSG_PC

