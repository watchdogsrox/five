
#include "savegame_data_player_ped.h"
#include "savegame_data_player_ped_parser.h"


// Game headers
#include "Peds/Ped.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PedIntelligence.h"
#include "Peds/rendering/PedVariationPack.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_channel.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "task/Motion/TaskMotionBase.h"



void CPlayerPedSaveStructure::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CPlayerPedSaveStructure::PreSave"))
	{
		return;
	}

	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed == NULL)
	{
		savegameAssertf(0, "CPlayerPedSaveStructure::PreSave - player pointer is NULL");
		return;
	}

	CPlayerInfo *pPlayerInfo = pPlayerPed->GetPlayerInfo();
	if (pPlayerInfo == NULL)
	{
		savegameAssertf(0, "CPlayerPedSaveStructure::PreSave - failed to get CPlayerInfo from player ped");
		return;
	}

	u64 gamerId			= pPlayerInfo->m_GamerInfo.GetGamerId().GetId();

	m_GamerIdHigh = gamerId >> 32;
	m_GamerIdLow = (u32) gamerId;

	m_CarDensityForCurrentZone	= pPlayerInfo->CarDensityForCurrentZone;	
	m_CollectablesPickedUp		= pPlayerInfo->CollectablesPickedUp;	
	m_TotalNumCollectables		= pPlayerInfo->TotalNumCollectables;
	m_DoesNotGetTired			= false;	//	pPlayerInfo->bDoesNotGetTired;
	m_FastReload				= false;	//	pPlayerInfo->bFastReload;
	m_FireProof					= false;	//	pPlayerInfo->bFireProof;
	m_MaxHealth					= pPlayerInfo->MaxHealth;
	m_MaxArmour					= pPlayerInfo->MaxArmour;
	m_bCanDoDriveBy				= true;		//	pPlayerInfo->bCanDoDriveBy;
	m_bCanBeHassledByGangs		= false;	//	pPlayerInfo->bCanBeHassledByGangs;
	m_nLastBustMessageNumber	= pPlayerInfo->m_nLastBustMessageNumber;
	m_ModelHashKey				= pPlayerPed->GetBaseModelInfo()->GetHashKey();

	m_Position 		 		= VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
	m_fHealth 		 		= pPlayerPed->GetHealth();
	m_nArmour 		 		= pPlayerPed->m_nArmour;
	weaponAssert(pPlayerPed->GetWeaponManager());
	m_nCurrentWeaponSlot 	= pPlayerPed->GetWeaponManager()->GetEquippedWeapon() ? pPlayerPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetSlot() : 0;

	int loop = 0;
#if 0 // CS
	for (loop = 0; loop < CWeaponInfoManager::MAX_SLOTS; loop++)
	{
		m_nWeaponHashes[loop] = pPlayerPed->GetInventory().GetItemHashInSlot(loop);
		m_nAmmoTotals[loop] = (u16)pPlayerPed->GetWeaponMgr()->GetAmmoInSlot(loop);	//! is stored as u16 internally
	}
#endif // 0

	if (pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet ))
	{
		m_bPlayerHasHelmet = true;
	}
	else
	{
		m_bPlayerHasHelmet = false;
	}

	m_nStoredHatPropIdx = pPlayerPed->GetHelmetComponent()->GetStoredHatPropIndex();
	m_nStoredHatTexIdx = pPlayerPed->GetHelmetComponent()->GetStoredHatTexIndex();

	CPackedPedProps packedProps = CPedPropsMgr::GetPedPropsPacked(pPlayerPed);

	m_CurrentProps.Reset();

	for (int i = 0; i < MAX_PROPS_PER_PED; i++)
	{
		SPedCompPropConvData saveData;

		packedProps.GetPackedPedPropData(i, saveData.m_hash, saveData.m_data);

		m_CurrentProps.Push(saveData);
	}


	atHashString HashStringOfPedVarComp;
	m_ComponentVariations.Reset();
	m_TextureVariations.Reset();
	for (loop = 0; loop < PV_MAX_COMP; loop++)
	{
		if (savegameVerifyf(CSaveGameMappingEnumsToHashStrings::GetHashString_PedVarComp(loop, HashStringOfPedVarComp), "CPlayerPedSaveStructure::PreSave - failed to find a hash string for ped var comp %d", loop))
		{
			SPedCompPropConvData compConvData;
			u32 compIdx = CPedVariationPack::GetCompVar(pPlayerPed, static_cast<ePedVarComp>(loop));

			m_TextureVariations.Insert(HashStringOfPedVarComp, CPedVariationPack::GetTexVar(pPlayerPed, static_cast<ePedVarComp>(loop)));

			CPedVariationPack::GetLocalCompData(pPlayerPed, (ePedVarComp)loop, compIdx, compConvData.m_hash, compConvData.m_data);

			m_ComponentVariations.Insert(HashStringOfPedVarComp, compConvData);
		}
	}
	m_ComponentVariations.FinishInsertion();
	m_TextureVariations.FinishInsertion();

	// get all the ped decorations (tattoos/scars)
	//	m_DecorationList.Resize(CPedDamageSet::kMaxDecorationBlits);
	//	for (int i=0;i<CPedDamageSet::kMaxDecorationBlits; i++)
	//		memset(&m_DecorationList[i],0,sizeof (CPlayerDecorationStruct));

	m_DecorationList.Reset();

	CPedDamageSet * pDamageSet = PEDDAMAGEMANAGER.GetPlayerDamageSet();

	if (pDamageSet)
	{
		atVector<CPedDamageBlitDecoration> &decorationList = pDamageSet->GetDecorationList();

		savegameDisplayf("CPlayerPedSaveStructure::PreSave - decoration list size = %d", decorationList.GetCount());

		for (int i=0; i<decorationList.GetCount(); i++) 
		{
			if ( decorationList[i].GetZone()>=0 && !decorationList[i].IsDone())
			{
				CPlayerDecorationStruct DecorationToAdd;
				DecorationToAdd.m_UVCoords = decorationList[i].GetUVCoords();
				DecorationToAdd.m_Scale	   = decorationList[i].GetScale();
				DecorationToAdd.m_TxdHash  = decorationList[i].GetTxdHash();
				DecorationToAdd.m_TxtHash  = decorationList[i].GetTxtHash();	
				DecorationToAdd.m_Type	   = (u8)decorationList[i].GetType();
				DecorationToAdd.m_Zone     = (u8)decorationList[i].GetZone();
				DecorationToAdd.m_Alpha    = (u8)(decorationList[i].GetAlphaIntensity()*255);
				DecorationToAdd.m_Age	   = TIME.GetElapsedTime() - decorationList[i].GetBirthTime();
				DecorationToAdd.m_FlipUVFlags= decorationList[i].GetUVFlipFlags();
				DecorationToAdd.m_FixedFrame = (u8)(decorationList[i].GetFixedFrame());
				DecorationToAdd.m_SourceNameHash = decorationList[i].GetSourceNameHash();

				m_DecorationList.PushAndGrow(DecorationToAdd);
			}
		}

		savegameDisplayf("CPlayerPedSaveStructure::PreSave - %d decorations have been saved", m_DecorationList.GetCount());
	}
}

void CPlayerPedSaveStructure::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CPlayerPedSaveStructure::PostLoad"))
	{
		return;
	}

	const CModelInfoConfig& cfgModel = CGameConfig::Get().GetConfigModelInfo();
	CGameWorld::CreatePlayer(m_Position, atHashString(cfgModel.m_defaultPlayerName), 200.0f);

	// Copied from CommandChangePlayerModel
	// we want to do 'things' with the player. Better flush the draw list...
	//	gRenderThreadInterface.Flush();

	fwModelId playerModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_ModelHashKey, & playerModelId);
	savegameAssertf(playerModelId.IsValid(), "CPlayerPedSaveStructure::PostLoad - this is not a valid model index");

	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	if(savegameVerifyf(pPlayerPed, "CPlayerPedSaveStructure::PostLoad - cant find local player"))
	{
		// Update the player with the specified model
		//	Maybe I should skip all this if the player is already using playermodelindex
		// Does "player" model need requested?

		CModelInfo::RequestAssets(playerModelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);

		CPed *pNewPlayerPed = CGameWorld::ChangePedModel(*pPlayerPed, playerModelId);

		CModelInfo::SetAssetsAreDeletable(playerModelId);

		pNewPlayerPed->GetPortalTracker()->RequestRescanNextUpdate();
		pNewPlayerPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pNewPlayerPed->GetTransform().GetPosition()));

		//	I'm not sure if I should be copying this line from CommandChangePlayerModel
		pNewPlayerPed->GetPedIntelligence()->AddTaskDefault(pNewPlayerPed->GetCurrentMotionTask()->CreatePlayerControlTask());
	}

	pPlayerPed = CGameWorld::FindLocalPlayer();

	if (pPlayerPed == NULL)
	{
		savegameAssertf(0, "CPlayerPedSaveStructure::PostLoad - player pointer is NULL");
		return;
	}

	CPlayerInfo *pPlayerInfo = pPlayerPed->GetPlayerInfo();
	if (pPlayerInfo == NULL)
	{
		savegameAssertf(0, "CPlayerPedSaveStructure::PostLoad - failed to get CPlayerInfo from player ped");
		return;
	}

#if 0 // GSW
	if (bNetworkLoad)
	{	//	Loading from a storage device will call GetCreator but the network load needs 
		//	to check the player is still the same using the m
		u64 gamerId = static_cast<u64>(m_GamerIdHigh) << 32;
		gamerId += static_cast<u64>(m_GamerIdLow);

		if (pPlayerInfo->m_GamerInfo.GetGamerId() != gamerId)
		{
			return false;
		}
	}
#endif // 0

	pPlayerInfo->CarDensityForCurrentZone 	= m_CarDensityForCurrentZone;	
	pPlayerInfo->CollectablesPickedUp		= m_CollectablesPickedUp; 
	pPlayerInfo->TotalNumCollectables		= m_TotalNumCollectables; 
	//pPlayerInfo->bDoesNotGetTired			= m_DoesNotGetTired;
	//pPlayerInfo->bFastReload				= m_FastReload;
	//pPlayerInfo->bFireProof				= m_FireProof;
	pPlayerInfo->MaxHealth					= m_MaxHealth;
	pPlayerInfo->MaxArmour					= m_MaxArmour;
	//pPlayerInfo->bCanDoDriveBy			= m_bCanDoDriveBy;
	//pPlayerInfo->bCanBeHassledByGangs		= m_bCanBeHassledByGangs;
	pPlayerInfo->m_nLastBustMessageNumber	= m_nLastBustMessageNumber;

	//	pPlayerPed->SetPosition(m_Position);
	pPlayerPed->SetHealth(m_fHealth);
	pPlayerPed->m_nArmour	= m_nArmour;

	//	Clear the player's special slot - any object that he was carrying when the save happened won't have been saved
	//	so don't even try to set the weapon slot for it
	//	if (m_nCurrentWeaponSlot == WEAPONSLOT_SPECIAL)
	//	{
	//		m_nCurrentWeaponSlot = WEAPONSLOT_UNARMED;
	//	}

	m_nCurrentWeaponSlot = 0;	//	Just set this to 0 for now

	//	Is this wrong now?
	//	m_nWeaponHashes[WEAPONSLOT_SPECIAL] = WEAPONTYPE_UNARMED;
	//	m_nAmmoTotals[WEAPONSLOT_SPECIAL] = 0;

	//	for(loop = 0; loop < CWeaponInfoManager::MAX_SLOTS; loop++)
	//	{
	//		if(m_nWeaponHashes[loop] != 0)
	//		{
	//			pPlayerPed->GetInventory().AddItemAndAmmo(m_nWeaponHashes[loop], m_nAmmoTotals[loop]);
	//		}
	//	}

	if (m_bPlayerHasHelmet)
	{
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet, true );
	}
	else
	{
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet, false );
	}

	if (pPlayerPed->GetHelmetComponent())
		pPlayerPed->GetHelmetComponent()->SetStoredHatIndices(m_nStoredHatPropIdx, m_nStoredHatTexIdx);

	weaponAssert(pPlayerPed->GetWeaponManager() && pPlayerPed->GetInventory());
	// let the weapon manager request the models
	pPlayerPed->GetInventory()->Process();
	// force them to load up
	CStreaming::LoadAllRequestedObjects();
	// let the weapon manager reference the models now they're loaded
	pPlayerPed->GetInventory()->Process();

	// set the ped's current weapon, and give him the corresponding weapon object
	//  	pPlayerPed->GetWeaponManager().SelectWeaponSlot(m_nCurrentWeaponSlot, true);
	pPlayerPed->GetWeaponManager()->CreateEquippedWeaponObject();

	if (savegameVerifyf(m_CurrentProps.GetCount() == MAX_PROPS_PER_PED, "CPlayerPedSaveStructure::PostLoad - the m_CurrentProps array doesn't have MAX_PROPS_PER_PED elements. Maybe this is an old savegame"))
	{
		CPackedPedProps packedProps;

		for (int i = 0; i < MAX_PROPS_PER_PED; i++)
			packedProps.SetPackedPedPropData(i, m_CurrentProps[i].m_hash, m_CurrentProps[i].m_data);

		CPedPropsMgr::SetPedPropsPacked(pPlayerPed, packedProps);
	}

	atBinaryMap<SPedCompPropConvData, atHashString>::Iterator CompVarIterator = m_ComponentVariations.Begin();
	atBinaryMap<u8, atHashString>::Iterator TexVarIterator = m_TextureVariations.Begin();
	while ( (CompVarIterator != m_ComponentVariations.End()) && (TexVarIterator != m_TextureVariations.End()) )
	{
		const atHashString HashStringOfPedVarComp = CompVarIterator.GetKey();
		savegameAssertf(HashStringOfPedVarComp.GetHash() == TexVarIterator.GetKey().GetHash(), "CPlayerPedSaveStructure::PostLoad - expected hash string to match for corresponding array entries for comp (%s) and texture (%s) variations", HashStringOfPedVarComp.GetCStr(), TexVarIterator.GetKey().GetCStr());
		s32 PedVarCompIndex = CSaveGameMappingEnumsToHashStrings::FindIndexFromHashString_PedVarComp(HashStringOfPedVarComp);
		if (savegameVerifyf(PedVarCompIndex >= 0, "CPlayerPedSaveStructure::PostLoad - couldn't find ped var comp index for hash string %s", HashStringOfPedVarComp.GetCStr()))
		{
			SPedCompPropConvData convData = *CompVarIterator;
			u32 compIdx = CPedVariationPack::GetGlobalCompData(pPlayerPed, (ePedVarComp)PedVarCompIndex, convData.m_hash, convData.m_data);

			pPlayerPed->SetVariation(static_cast<ePedVarComp>(PedVarCompIndex), compIdx, 0, *TexVarIterator, 0, 0, false);
		}

		++CompVarIterator;
		++TexVarIterator;
	}

	//	The following was cut from CPed::LoadPlayerPed
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle, false );
	pPlayerPed->m_pMyVehicle = NULL;

	if (pPlayerPed->GetHelmetComponent())
		pPlayerPed->GetHelmetComponent()->DisableHelmet();

	//	Possible fix for Bug 120916 - I still need to test this
	if (pPlayerPed->GetHealth() < (pPlayerPed->GetInjuredHealthThreshold() +5.0f) )
	{
		savegameDisplayf("CPed::LoadPlayerPed - player health was %f so setting to (PED_INJURED_HEALTH_THRESHOLD+5)\n", pPlayerPed->GetHealth());
		pPlayerPed->SetHealth( (pPlayerPed->GetInjuredHealthThreshold() +5.0f) );
	}

	// clear and previous decorations or blood
	pPlayerPed->ReleaseDamageSet();

	// restore the ped decorations (tattoos/scars)
	//	for (int i=0;i<CPedDamageSet::kMaxDecorationBlits; i++)
	for (int i=0;i<m_DecorationList.GetCount(); i++)
	{
		if (m_DecorationList[i].m_TxtHash!=0) 
		{
			const Vector4 & uvCoords = m_DecorationList[i].m_UVCoords;

			// calc rotation and scale from uvcoords NOTE: it needs to match the encoding in CPedDamageSet::AddDecorationBlit()
			float rotation = 0.0f;

			Vector2 dir(uvCoords.z,uvCoords.w);
			float mag = dir.Mag();
			if (mag>0.001)
			{
				dir /= mag;
				rotation = atan2(dir.y,dir.x)*RtoD;
			}

			CPedDamageDecalInfo * sourceInfo = NULL;
			float fadeOutStartTime = -1.0f;
			float fadeOutTime = 0.0f;
			if (m_DecorationList[i].m_SourceNameHash !=0)
			{
				sourceInfo = PEDDAMAGEMANAGER.GetDamageData().GetDamageDecalInfoByHash(m_DecorationList[i].m_SourceNameHash);
				if(sourceInfo)
				{
					fadeOutStartTime = sourceInfo->GetFadeOutStartTime();
					fadeOutTime = sourceInfo->GetFadeOutTime();
				}
			}

			atHashString nullHash;
			PEDDAMAGEMANAGER.AddPedDecoration(  pPlayerPed, nullHash, nullHash,
				(ePedDamageZones)m_DecorationList[i].m_Zone,
				Vector2(uvCoords.x,uvCoords.y),
				rotation,
				m_DecorationList[i].m_Scale,
				false,
				strStreamingObjectName(m_DecorationList[i].m_TxdHash),
				strStreamingObjectName(m_DecorationList[i].m_TxtHash),
				(m_DecorationList[i].m_Type==kDamageTypeSkinBumpDecoration || m_DecorationList[i].m_Type==kDamageTypeClothBumpDecoration),
				m_DecorationList[i].m_Alpha/255.0f,
				(m_DecorationList[i].m_Type==kDamageTypeClothDecoration ||  m_DecorationList[i].m_Type==kDamageTypeClothBumpDecoration),
				-1,
				sourceInfo,
				0.0f,
				fadeOutStartTime,
				fadeOutTime,
				m_DecorationList[i].m_Age,
				m_DecorationList[i].m_FixedFrame,
				m_DecorationList[i].m_FlipUVFlags
				);
		}
	}
}

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
void CPlayerPedSaveStructure::Initialize(CPlayerPedSaveStructure_Migration &copyFrom)
{
	// Copy all the members that don't need to change
	m_GamerIdHigh = copyFrom.m_GamerIdHigh;
	m_GamerIdLow = copyFrom.m_GamerIdLow;
	m_CarDensityForCurrentZone = copyFrom.m_CarDensityForCurrentZone;
	m_CollectablesPickedUp = copyFrom.m_CollectablesPickedUp;
	m_TotalNumCollectables = copyFrom.m_TotalNumCollectables;
	m_DoesNotGetTired = copyFrom.m_DoesNotGetTired;
	m_FastReload = copyFrom.m_FastReload;
	m_FireProof = copyFrom.m_FireProof;
	m_MaxHealth = copyFrom.m_MaxHealth;
	m_MaxArmour = copyFrom.m_MaxArmour;
	m_bCanDoDriveBy = copyFrom.m_bCanDoDriveBy;
	m_bCanBeHassledByGangs = copyFrom.m_bCanBeHassledByGangs;
	m_nLastBustMessageNumber = copyFrom.m_nLastBustMessageNumber;

	m_ModelHashKey = copyFrom.m_ModelHashKey;
	m_Position = copyFrom.m_Position;
	m_fHealth = copyFrom.m_fHealth;
	m_nArmour = copyFrom.m_nArmour;
	m_nCurrentWeaponSlot = copyFrom.m_nCurrentWeaponSlot;
	//	u32	m_nWeaponHashes[CWeaponInfoManager::MAX_SLOTS];
	//	u32	m_nAmmoTotals[MAX_AMMOS];

	m_bPlayerHasHelmet = copyFrom.m_bPlayerHasHelmet;
	m_nStoredHatPropIdx = copyFrom.m_nStoredHatPropIdx;
	m_nStoredHatTexIdx = copyFrom.m_nStoredHatTexIdx;

	m_CurrentProps.Reset();
	m_CurrentProps = copyFrom.m_CurrentProps;
	// 	for (int i = 0; i < MAX_PROPS_PER_PED; i++)
	// 	{
	// 		m_CurrentProps[i] = copyFrom.m_CurrentProps[i];
	// 	}

	const s32 numberOfDecorationsToCopy = copyFrom.m_DecorationList.GetCount();
	m_DecorationList.Reset();
	m_DecorationList.Reserve(numberOfDecorationsToCopy);
	for (s32 loop = 0; loop < numberOfDecorationsToCopy; loop++)
	{
		CPlayerDecorationStruct &newEntry = m_DecorationList.Append();
		newEntry = copyFrom.m_DecorationList[loop];
	}

	// Now copy the contents of the two Binary Maps
	// The source binary maps use u32 for the key.
	// The binary maps in this class use atHashString for the key.
	// Hopefully the game already knows what the strings are for these atHashStrings.
	m_ComponentVariations.Reset();
	m_ComponentVariations.Reserve(copyFrom.m_ComponentVariations.GetCount());

	atBinaryMap<SPedCompPropConvData, u32>::Iterator componentIterator = copyFrom.m_ComponentVariations.Begin();
	while (componentIterator != copyFrom.m_ComponentVariations.End())
	{
		u32 keyAsU32 = componentIterator.GetKey();
		atHashString keyAsHashString(keyAsU32);
		CPlayerPedSaveStructure::SPedCompPropConvData valueToInsert = *componentIterator;
		m_ComponentVariations.Insert(keyAsHashString, valueToInsert);

		componentIterator++;
	}
	m_ComponentVariations.FinishInsertion();


	m_TextureVariations.Reset();
	m_TextureVariations.Reserve(copyFrom.m_TextureVariations.GetCount());

	atBinaryMap<u8, u32>::Iterator textureIterator = copyFrom.m_TextureVariations.Begin();
	while (textureIterator != copyFrom.m_TextureVariations.End())
	{
		u32 keyAsU32 = textureIterator.GetKey();
		atHashString keyAsHashString(keyAsU32);
		u8 valueToInsert = *textureIterator;
		m_TextureVariations.Insert(keyAsHashString, valueToInsert);

		textureIterator++;
	}
	m_TextureVariations.FinishInsertion();
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


//	#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
void CPlayerPedSaveStructure_Migration::Initialize(CPlayerPedSaveStructure &copyFrom)
{
// Copy all the members that don't need to change
	m_GamerIdHigh = copyFrom.m_GamerIdHigh;
	m_GamerIdLow = copyFrom.m_GamerIdLow;
	m_CarDensityForCurrentZone = copyFrom.m_CarDensityForCurrentZone;
	m_CollectablesPickedUp = copyFrom.m_CollectablesPickedUp;
	m_TotalNumCollectables = copyFrom.m_TotalNumCollectables;
	m_DoesNotGetTired = copyFrom.m_DoesNotGetTired;
	m_FastReload = copyFrom.m_FastReload;
	m_FireProof = copyFrom.m_FireProof;
	m_MaxHealth = copyFrom.m_MaxHealth;
	m_MaxArmour = copyFrom.m_MaxArmour;
	m_bCanDoDriveBy = copyFrom.m_bCanDoDriveBy;
	m_bCanBeHassledByGangs = copyFrom.m_bCanBeHassledByGangs;
	m_nLastBustMessageNumber = copyFrom.m_nLastBustMessageNumber;

	m_ModelHashKey = copyFrom.m_ModelHashKey;
	m_Position = copyFrom.m_Position;
	m_fHealth = copyFrom.m_fHealth;
	m_nArmour = copyFrom.m_nArmour;
	m_nCurrentWeaponSlot = copyFrom.m_nCurrentWeaponSlot;
	//	u32	m_nWeaponHashes[CWeaponInfoManager::MAX_SLOTS];
	//	u32	m_nAmmoTotals[MAX_AMMOS];

	m_bPlayerHasHelmet = copyFrom.m_bPlayerHasHelmet;
	m_nStoredHatPropIdx = copyFrom.m_nStoredHatPropIdx;
	m_nStoredHatTexIdx = copyFrom.m_nStoredHatTexIdx;

	m_CurrentProps.Reset();
	m_CurrentProps = copyFrom.m_CurrentProps;
// 	for (int i = 0; i < MAX_PROPS_PER_PED; i++)
// 	{
// 		m_CurrentProps[i] = copyFrom.m_CurrentProps[i];
// 	}

	const s32 numberOfDecorationsToCopy = copyFrom.m_DecorationList.GetCount();
	m_DecorationList.Reset();
	m_DecorationList.Reserve(numberOfDecorationsToCopy);
	for (s32 loop = 0; loop < numberOfDecorationsToCopy; loop++)
	{
		CPlayerPedSaveStructure::CPlayerDecorationStruct &newEntry = m_DecorationList.Append();
		newEntry = copyFrom.m_DecorationList[loop];
	}

	// Now copy the contents of the two Binary Maps
	// The original binary maps used atHashString for the key.
	// The string for an atHashString is stripped out of Final executables.
	// The binary maps in this class use u32 for the key so that they can be written to an XML file by a Final executable.
	m_ComponentVariations.Reset();
	m_ComponentVariations.Reserve(copyFrom.m_ComponentVariations.GetCount());

	atBinaryMap<CPlayerPedSaveStructure::SPedCompPropConvData, atHashString>::Iterator componentIterator = copyFrom.m_ComponentVariations.Begin();
	while (componentIterator != copyFrom.m_ComponentVariations.End())
	{
		u32 keyAsU32 = componentIterator.GetKey().GetHash();
		CPlayerPedSaveStructure::SPedCompPropConvData valueToInsert = *componentIterator;
		m_ComponentVariations.Insert(keyAsU32, valueToInsert);

		componentIterator++;
	}
	m_ComponentVariations.FinishInsertion();


	m_TextureVariations.Reset();
	m_TextureVariations.Reserve(copyFrom.m_TextureVariations.GetCount());

	atBinaryMap<u8, atHashString>::Iterator textureIterator = copyFrom.m_TextureVariations.Begin();
	while (textureIterator != copyFrom.m_TextureVariations.End())
	{
		u32 keyAsU32 = textureIterator.GetKey().GetHash();
		u8 valueToInsert = *textureIterator;
		m_TextureVariations.Insert(keyAsU32, valueToInsert);

		textureIterator++;
	}
	m_TextureVariations.FinishInsertion();
}


void CPlayerPedSaveStructure_Migration::PreSave()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CPlayerPedSaveStructure_Migration::PreSave"))
	{
		return;
	}
}

void CPlayerPedSaveStructure_Migration::PostLoad()
{
	if (!CGenericGameStorage::AllowPreSaveAndPostLoad("CPlayerPedSaveStructure_Migration::PostLoad"))
	{
		return;
	}
}
//	#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

