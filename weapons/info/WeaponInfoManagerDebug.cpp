//
// weapons/weaponinfomanagerdebug.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#if __BANK // Only in __BANK builds

// Class header
#include "Weapons/Info/WeaponInfoManager.h"

// Rage headers
#include "bank/bank.h"
#include "parser/manager.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Weapons/Info/AmmoInfo.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/Inventory/PedInventoryLoadOut.h"
#include "Weapons/WeaponDebug.h"
#include "Scene/EntityIterator.h"
#include "Scene/World/GameWorld.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

#undef	WEAPON_STATS
#define WEAPON_STATS(X)	X(WeaponName), \
						X(WeaponGroup), \
						X(DamageHealth), \
						X(DamageEndurance), \
						X(DamageNumPellets), \
						X(DamageModifierHeadAI), \
						X(DamageModifierHeadPlayer), \
						X(DamageModifierLimbSP), \
						X(DamageModifierLimbMP), \
						X(DamageModifierArmor), \
						X(DamageModifierVehicle), \
						X(AccuracySpread), \
						X(AccuracyPelletSpread), \
						X(AccuracyErrorTime), \
						X(AccuracyRecovery), \
						X(AccuracyRecoil), \
						X(RangeLockOn), \
						X(RangeMaximum), \
						X(RangeFallOffStart), \
						X(RangeFallOffEnd), \
						X(RangeFallOffMod), \
						X(ReloadAnimClipSet), \
						X(ReloadAnimDict), \
						X(ReloadAnimLength), \
						X(ReloadAnimRate), \
						X(ReloadAnimIgnoreRateMod), \
						X(ReloadAnimLooped), \
						X(AmmoDefaultClipSize), \
						X(AmmoLargeClipSize), \
						X(AmmoDrumClipSize), \
						X(HudDamage), \
						X(HudFireRate), \
						X(HudCapacity), \
						X(HudAccuracy), \
						X(HudRange)

#define DEFINE_WEAPON_STATS_ENUM(S) S
#define DEFINE_WEAPON_STATS_STRING(S) #S

enum WeaponStatFields
{
	WEAPON_STATS(DEFINE_WEAPON_STATS_ENUM),
	NUM_WEAPON_STATS
};

////////////////////////////////////////////////////////////////////////////////

atString CWeaponInfoManager::GetWidgetGroupName()
{
	return atString("Weapon Info");
}

void CWeaponInfoManager::AddWidgetButton(bkBank& bank)
{
	bank.PushGroup(GetWidgetGroupName().c_str());

	atString metadataWidgetCreateLabel("");
	metadataWidgetCreateLabel += "Create ";
	metadataWidgetCreateLabel += GetWidgetGroupName();
	metadataWidgetCreateLabel += " widgets";

	bank.AddButton(metadataWidgetCreateLabel.c_str(), datCallback(CFA1(CWeaponInfoManager::AddWidgets), &bank));

	bank.PopGroup();
}

void CWeaponInfoManager::AddWidgets(bkBank& bank)
{
	// Destroy stub group before creating new one
	if (bkWidget* pWidgetGroup = bank.FindChild(GetWidgetGroupName().c_str()))
	{
		pWidgetGroup->Destroy();
	}

	bank.PushGroup(GetWidgetGroupName().c_str());

	bank.AddButton("Dump Weapon Stats to CSV in common:/", datCallback(CFA(DumpWeaponStats)));

	bank.AddSeparator();

	for (int x=0; x<ms_Instance.m_WeaponInfoBlobs.GetCount(); x++)
	{
		ms_Instance.m_WeaponInfoBlobs[x].AddWidgets(bank);
	}

	bank.PopGroup();
}

void CWeaponInfoBlob::AddWidgets(bkBank& bank)
{
	bank.PushGroup(m_Name.c_str());

	bank.AddTitle(m_Filename.c_str());

	// Tuning
	// AddTuningWidgets(bank); // PB - 11/06/19 - We don't really use this, disabled for now to save debug heap memory

	// Add info widgets
	AddInfoWidgets(bank, IT_Weapon);
	AddInfoWidgets(bank, IT_VehicleWeapon);
	AddInfoWidgets(bank, IT_Ammo);

	// Add gun info widgets
	AddAimingInfoWidgets(bank);

	// Shaders
	AddGraphicsWidgets(bank);

	// Vehicle components
	bank.PushGroup("Vehicle Component Infos");
	for(s32 i = 0; i < m_VehicleWeaponInfos.GetCount(); i++)
	{
		bank.PushGroup(m_VehicleWeaponInfos[i]->GetName());
		PARSER.AddWidgets(bank, m_VehicleWeaponInfos[i]);
		bank.PopGroup();
	}
	bank.PopGroup(); // "Vehicle Component Infos"

	// Upper body expression data
	bank.PushGroup("Upper Body Expression Fixup");
	for(s32 i = 0; i < m_UpperBodyFixupExpressionData.GetCount(); i++)
	{
		m_UpperBodyFixupExpressionData[i].AddWidgets(bank);
	}
	bank.PopGroup(); // "Upper Body Expression Fixup"

	bank.AddButton("Save", datCallback(MFA(CWeaponInfoBlob::Save), this));

	bank.PopGroup(); // "m_Name.c_str()"
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponInfoManager::GetRandomOneHandedWeaponHash()
{
	atArray<u32> weaponHashes;

	const sWeaponInfoList::InfoList& weaponList = ms_Instance.m_Infos[CWeaponInfoBlob::IT_Weapon].Infos;
	for(s32 i = 0; i < weaponList.GetCount(); i++)
	{
		if(weaponVerifyf(weaponList[i]->GetIsClassId(CWeaponInfo::GetStaticClassId()), "Info [%s] is not of type weapon, but is in CWeaponInfoBlob::IT_Weapon list", weaponList[i]->GetName()))
		{
			const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(weaponList[i]);
			if(pWeaponInfo->GetIsGun1Handed())
			{
				weaponHashes.PushAndGrow(pWeaponInfo->GetHash());
			}
		}
	}

	if(weaponHashes.GetCount() > 0)
	{
		return weaponHashes[fwRandom::GetRandomNumberInRange(0, weaponHashes.GetCount())];
	}
	else
	{
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponInfoManager::GetRandomTwoHandedWeaponHash()
{
	atArray<u32> weaponHashes;

	const sWeaponInfoList::InfoList& weaponList = ms_Instance.m_Infos[CWeaponInfoBlob::IT_Weapon].Infos;
	for(s32 i = 0; i < weaponList.GetCount(); i++)
	{
		if(weaponVerifyf(weaponList[i]->GetIsClassId(CWeaponInfo::GetStaticClassId()), "Info [%s] is not of type weapon, but is in CWeaponInfoBlob::IT_Weapon list", weaponList[i]->GetName()))
		{
			const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(weaponList[i]);
			if(pWeaponInfo->GetIsGun2Handed() && pWeaponInfo->GetIsAutomatic() && !pWeaponInfo->GetIsProjectile())
			{
				weaponHashes.PushAndGrow(pWeaponInfo->GetHash());
			}
		}
	}

	if(weaponHashes.GetCount() > 0)
	{
		return weaponHashes[fwRandom::GetRandomNumberInRange(0, weaponHashes.GetCount())];
	}
	else
	{
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponInfoManager::GetRandomProjectileWeaponHash()
{
	atArray<u32> weaponHashes;

	const sWeaponInfoList::InfoList& weaponList = ms_Instance.m_Infos[CWeaponInfoBlob::IT_Weapon].Infos;
	for(s32 i = 0; i < weaponList.GetCount(); i++)
	{
		if(weaponVerifyf(weaponList[i]->GetIsClassId(CWeaponInfo::GetStaticClassId()), "Info [%s] is not of type weapon, but is in CWeaponInfoBlob::IT_Weapon list", weaponList[i]->GetName()))
		{
			const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(weaponList[i]);
			if(pWeaponInfo->GetIsProjectile() && !pWeaponInfo->GetIsThrownWeapon())
			{
				weaponHashes.PushAndGrow(pWeaponInfo->GetHash());
			}
		}
	}

	if(weaponHashes.GetCount() > 0)
	{
		return weaponHashes[fwRandom::GetRandomNumberInRange(0, weaponHashes.GetCount())];
	}
	else
	{
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponInfoManager::GetRandomThrownWeaponHash()
{
	atArray<u32> weaponHashes;

	const sWeaponInfoList::InfoList& weaponList = ms_Instance.m_Infos[CWeaponInfoBlob::IT_Weapon].Infos;
	for(s32 i = 0; i < weaponList.GetCount(); i++)
	{
		if(weaponVerifyf(weaponList[i]->GetIsClassId(CWeaponInfo::GetStaticClassId()), "Info [%s] is not of type weapon, but is in CWeaponInfoBlob::IT_Weapon list", weaponList[i]->GetName()))
		{
			const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(weaponList[i]);
			if(pWeaponInfo->GetIsProjectile() && pWeaponInfo->GetIsThrownWeapon())
			{
				weaponHashes.PushAndGrow(pWeaponInfo->GetHash());
			}
		}
	}

	if(weaponHashes.GetCount() > 0)
	{
		return weaponHashes[fwRandom::GetRandomNumberInRange(0, weaponHashes.GetCount())];
	}
	else
	{
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

CWeaponInfoManager::StringList& CWeaponInfoManager::GetNames(CWeaponInfoBlob::InfoType type)
{
	return ms_Instance.m_InfoNames[type];
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoBlob::AddTuningWidgets(bkBank& bank)
{
	bank.PushGroup("Weapon Damage Tuning");

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(pLocalPlayer)
	{
		if(pLocalPlayer->GetPlayerInfo())
		{
			pLocalPlayer->GetPlayerInfo()->AddWeaponTuningWidgets(bank);
		}
	}

	for(s32 i = 0; i < m_Infos[CWeaponInfoBlob::IT_Weapon].Infos.GetCount(); i++)
	{
		if(weaponVerifyf(m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i]->GetIsClass<CWeaponInfo>(), "Info [%p] is not a CWeaponInfo [%s]", m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i], m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i]->GetClassId().TryGetCStr()))
		{
			CWeaponInfo* pWeaponInfo = static_cast<CWeaponInfo*>(m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i]);
			if(pWeaponInfo->GetIsGun())
			{
				bank.PushGroup(pWeaponInfo->GetName());
				pWeaponInfo->AddTuningWidgets(bank);
				bank.AddButton("Save", datCallback(MFA(CWeaponInfoBlob::Save), this));
				bank.PopGroup();
			}
		}
	}

	bank.AddSeparator("sep");
	bank.PushGroup("Non Guns");
	for(s32 i = 0; i < m_Infos[CWeaponInfoBlob::IT_Weapon].Infos.GetCount(); i++)
	{
		if(weaponVerifyf(m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i]->GetIsClass<CWeaponInfo>(), "Info [%p] is not a CWeaponInfo [%s]", m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i], m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i]->GetClassId().TryGetCStr()))
		{
			CWeaponInfo* pWeaponInfo = static_cast<CWeaponInfo*>(m_Infos[CWeaponInfoBlob::IT_Weapon].Infos[i]);
			if(!pWeaponInfo->GetIsGun())
			{
				bank.PushGroup(pWeaponInfo->GetName());
				pWeaponInfo->AddTuningWidgets(bank);
				bank.AddButton("Save", datCallback(MFA(CWeaponInfoBlob::Save), this));
				bank.PopGroup();
			}
		}
	}
	bank.PopGroup(); // "Non Guns"

	bank.PopGroup(); // "Weapon Damage Tuning"
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoBlob::AddGraphicsWidgets(bkBank& bank)
{
	bank.PushGroup("Weapon Shader Tuning");

	bank.AddToggle("Force Shader Update", CWeaponInfoManager::GetForceShaderUpdatePtr());

	for(s32 i = 0; i < m_TintSpecValues.GetCount(); i++)
	{
		bank.PushGroup(m_TintSpecValues[i].GetName());
		for(s32 j = 0; j < m_TintSpecValues[i].GetCount(); j++)
		{
			char buff[8];
			sprintf(buff, "%d", j);
			bank.PushGroup(buff);
			PARSER.AddWidgets(bank, m_TintSpecValues[i].GetSpecValuesForTint(j));
			bank.PopGroup();
		}
		bank.PopGroup();
	}

	bank.PopGroup(); // "Weapon Shader Tuning"
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoBlob::AddInfoWidgets(bkBank& bank, CWeaponInfoBlob::InfoType type)
{
	bank.PushGroup(CWeaponInfoManager::GetInfoTypeName(type));
	for(s32 i = 0; i < m_Infos[type].Infos.GetCount(); i++)
	{
		bank.PushGroup(m_Infos[type].Infos[i]->GetName());
		PARSER.AddWidgets(bank, m_Infos[type].Infos[i]);
		bank.PopGroup();
	}
	bank.PopGroup();
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoBlob::AddAimingInfoWidgets(bkBank& bank)
{
	bank.PushGroup("Aiming Infos");
	for(s32 i = 0; i < m_AimingInfos.GetCount(); i++)
	{
		bank.PushGroup(m_AimingInfos[i]->GetName());
		PARSER.AddWidgets(bank, m_AimingInfos[i]);
		bank.PopGroup();
	}
	bank.PopGroup();
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::SaveExamples()
{
	// For testing purposes - saves out examples of our data so we can see the text format
	static bank_bool SAVE_EXAMPLES = false;
	if(SAVE_EXAMPLES)
	{
		CWeaponInfoBlob &testBlob = ms_Instance.m_WeaponInfoBlobs.Grow();

		CWeaponInfoBlob::sWeaponSlotList pWeaponSlotList;
		CWeaponInfoBlob::SlotEntry slotEntry;
		slotEntry.m_OrderNumber = 10;
		slotEntry.m_Entry = atHashWithStringNotFinal("Hello",0xC8FD181B);
		pWeaponSlotList.WeaponSlots.PushAndGrow(slotEntry);
		for(s32 i = 0; i < CWeaponInfoBlob::WS_Max; i++)
		{
			ms_Instance.m_SlotNavigateOrder[i] = pWeaponSlotList;
		}
		ms_Instance.m_Infos[CWeaponInfoBlob::IT_Weapon].Infos.PushAndGrow(rage_new CWeaponInfo);
		ms_Instance.m_Infos[CWeaponInfoBlob::IT_Ammo].Infos.PushAndGrow(rage_new CAmmoInfo);
		ms_Instance.m_Infos[CWeaponInfoBlob::IT_Ammo].Infos.PushAndGrow(rage_new CAmmoProjectileInfo);
		ms_Instance.m_Infos[CWeaponInfoBlob::IT_Ammo].Infos.PushAndGrow(rage_new CAmmoRocketInfo);
		ms_Instance.m_Infos[CWeaponInfoBlob::IT_Ammo].Infos.PushAndGrow(rage_new CAmmoThrownInfo);
		testBlob.GetVehicleWeaponInfos().PushAndGrow(rage_new CVehicleWeaponInfo);

		// Only do this once per save
		SAVE_EXAMPLES = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::RemoveAllWeaponInfoReferences()
{
	CEntityIterator iterator( IteratePeds);
	CEntity* pEntity = iterator.GetNext();
	while(pEntity)
	{
		if(static_cast<CPed*>(pEntity)->GetInventory())
		{
			static_cast<CPed*>(pEntity)->GetInventory()->RemoveAll();
		}
		pEntity = iterator.GetNext();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::GiveAllPedsDefaultItems()
{
	CEntityIterator iterator( IteratePeds );
	CEntity* pEntity = iterator.GetNext();
	while(pEntity)
	{
		CPedInventoryLoadOutManager::SetDefaultLoadOut(static_cast<CPed*>(pEntity));
		pEntity = iterator.GetNext();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfoManager::DumpWeaponStats()
{
	CFileMgr::SetDir("common:/");
	const char* fileName = "WeaponStats.csv";
	FileHandle fileHandle = CFileMgr::OpenFileForWriting(fileName);
	CFileMgr::SetDir("");

	if(weaponVerifyf(CFileMgr::IsValidFileHandle(fileHandle), "Can't open file %s", fileName))
	{	
		// Reuse the same animation helper for every weapon
		fwClipSetRequestHelper statsClipSetRequestHelper;

		// Generate and print header
		static const char* WeaponStatNames[] = { WEAPON_STATS(DEFINE_WEAPON_STATS_STRING) };
		atString weaponStatString;
		for(s32 i = 0; i < NUM_WEAPON_STATS; i++)
		{
			weaponStatString += WeaponStatNames[i];
			if(i != (NUM_WEAPON_STATS - 1))
			{
				weaponStatString += ",";
			}
		}
		weaponStatString += '\n';
		CFileMgr::Write(fileHandle, weaponStatString.c_str(), weaponStatString.GetLength());
		
		// Print stats for each weapon
		const sWeaponInfoList::InfoList& weaponList = ms_Instance.m_Infos[CWeaponInfoBlob::IT_Weapon].Infos;
		for(s32 i = 0; i < weaponList.GetCount(); i++)
		{
			const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(weaponList[i]);
			if (pWeaponInfo && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET)
			{
				DumpWeaponStats(pWeaponInfo, fileHandle, statsClipSetRequestHelper);
			}
		}

		// Save file
		CFileMgr::CloseFile(fileHandle);
	}
}

////////////////////////////////////////////////////////////////////////////////

#define STORE_WEAPON_STATS_STRING(_Enum, _Format, _Var1) sprintf(buff, _Format, _Var1); statStrings[_Enum] = buff;
//#define STORE_WEAPON_STATS_STRING_2(_Enum, _Format, _Var1, _Var2) sprintf(buff, _Format, _Var1, _Var2); statStrings[_Enum] = buff;

void CWeaponInfoManager::DumpWeaponStats(const CWeaponInfo* pWeaponInfo, FileHandle fileHandle, fwClipSetRequestHelper& rAnimReqHelper)
{
	//const CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	char buff[256];
	atString statStrings[NUM_WEAPON_STATS];

	STORE_WEAPON_STATS_STRING(WeaponName, "%s", pWeaponInfo->GetName());
	STORE_WEAPON_STATS_STRING(WeaponGroup, "%s", pWeaponInfo->GetGroupName());

	//
	// Damage
	//

	STORE_WEAPON_STATS_STRING(DamageHealth, "%f", pWeaponInfo->GetDamage() / pWeaponInfo->GetWeaponDamageModifier()); // Undo script modifier
	STORE_WEAPON_STATS_STRING(DamageEndurance, "%f", pWeaponInfo->GetEnduranceDamage());
	STORE_WEAPON_STATS_STRING(DamageNumPellets, "%u", pWeaponInfo->GetBulletsInBatch());

	//
	// Damage Modifiers
	//

	STORE_WEAPON_STATS_STRING(DamageModifierHeadAI, "%f", pWeaponInfo->GetHeadShotDamageModifierAI());
	STORE_WEAPON_STATS_STRING(DamageModifierHeadPlayer, "%f", pWeaponInfo->GetHeadShotDamageModifierPlayer() / pWeaponInfo->GetHeadShotDamageModifierPlayerModifier()); // Undo script modifier
	STORE_WEAPON_STATS_STRING(DamageModifierLimbSP, "%f", pWeaponInfo->GetHitLimbsDamageModifier());
	STORE_WEAPON_STATS_STRING(DamageModifierLimbMP, "%f", pWeaponInfo->GetNetworkHitLimbsDamageModifier());
	STORE_WEAPON_STATS_STRING(DamageModifierArmor, "%f", pWeaponInfo->GetLightlyArmouredDamageModifier());
	STORE_WEAPON_STATS_STRING(DamageModifierVehicle, "%f", pWeaponInfo->GetVehicleDamageModifier());

	//
	// Accuracy
	//

	STORE_WEAPON_STATS_STRING(AccuracySpread, "%f", pWeaponInfo->GetAccuracySpread());
	STORE_WEAPON_STATS_STRING(AccuracyPelletSpread, "%f", pWeaponInfo->GetBatchSpread());
	STORE_WEAPON_STATS_STRING(AccuracyErrorTime, "%f", pWeaponInfo->GetRecoilErrorTime());
	STORE_WEAPON_STATS_STRING(AccuracyRecovery, "%f", pWeaponInfo->GetRecoilRecoveryRate());
	STORE_WEAPON_STATS_STRING(AccuracyRecoil, "%f", pWeaponInfo->GetRecoilShakeAmplitude());

	//
	// Range
	//

	STORE_WEAPON_STATS_STRING(RangeLockOn, "%f", pWeaponInfo->GetLockOnRange());
	STORE_WEAPON_STATS_STRING(RangeMaximum, "%f", pWeaponInfo->GetRange());
	STORE_WEAPON_STATS_STRING(RangeFallOffStart, "%f", pWeaponInfo->GetDamageFallOffRangeMin() / pWeaponInfo->GetDamageFallOffRangeModifier()); // Undo script modifier
	STORE_WEAPON_STATS_STRING(RangeFallOffEnd, "%f", pWeaponInfo->GetDamageFallOffRangeMax() / pWeaponInfo->GetDamageFallOffRangeModifier()); // Undo script modifier
	STORE_WEAPON_STATS_STRING(RangeFallOffMod, "%f", pWeaponInfo->GetDamageFallOffModifier());

	//
	// Firing Rate (TBD)
	//

	//
	// Reloads
	//

	atHashString reloadClipDictName;
	float fReloadClipDuration = 0.0f;

	// Extra data from animation itself
	fwMvClipSetId reloadClipSetId = pWeaponInfo->GetWeaponClipSetStreamedId();
	if (reloadClipSetId.IsNotNull())
	{
		rAnimReqHelper.Request(reloadClipSetId, true);
		if (rAnimReqHelper.IsLoaded())
		{
			fwClipSet* pReloadClipSet = rAnimReqHelper.GetClipSet();
			if (pReloadClipSet)
			{
				reloadClipDictName = pReloadClipSet->GetClipDictionaryName();

				static const fwMvClipId reloadClipId("reload_aim",0x1D34D8F0);
				crClip* pReloadClip = pReloadClipSet->GetClip(reloadClipId);
				if (pReloadClip)
				{
					fReloadClipDuration = pReloadClip->GetDuration();
				}
			}
		}
		rAnimReqHelper.Release();
	}

	STORE_WEAPON_STATS_STRING(ReloadAnimClipSet, "%s", reloadClipSetId.IsNotNull() ? reloadClipSetId.GetCStr() : "n/a");
	STORE_WEAPON_STATS_STRING(ReloadAnimDict, "%s", reloadClipDictName.IsNotNull() ? reloadClipDictName.GetCStr() : "n/a");
	STORE_WEAPON_STATS_STRING(ReloadAnimLength, "%f", fReloadClipDuration);

	// Rest of the info is from metadata
	STORE_WEAPON_STATS_STRING(ReloadAnimRate, "%f", pWeaponInfo->GetAnimReloadRate());
	STORE_WEAPON_STATS_STRING(ReloadAnimIgnoreRateMod, "%s", pWeaponInfo->GetIgnoreAnimReloadRateModifiers() ? "true" : "false");
	STORE_WEAPON_STATS_STRING(ReloadAnimLooped, "%s", pWeaponInfo->GetUseLoopedReloadAnim() ? "true" : "false");

	//
	// Capacity
	//

	s32 iDefaultClipSize = -1;
	s32 iLargeClipSize = -1;
	s32 iDrumClipSize = -1;
	atArray<s32> iOtherClipSizes;

	// Loop through all potential clip components
	const CWeaponInfo::AttachPoints& attachPoints = pWeaponInfo->GetAttachPoints();
	for(int i = 0; i < attachPoints.GetCount(); i++)
	{
		const CWeaponComponentPoint::Components& components = attachPoints[i].GetComponents();
		for(int j = 0; j < components.GetCount(); j++)
		{
			const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(components[j].m_Name);
			if (pComponentInfo && pComponentInfo->GetClassId() == WCT_Clip)
			{
				const CWeaponComponentClipInfo *pComponentClipInfo = static_cast<const CWeaponComponentClipInfo*>(pComponentInfo);
				if (pComponentClipInfo)
				{
					s32 iComponentClipSize = pComponentClipInfo->GetClipSize();

					if (components[j].m_Default)
					{
						iDefaultClipSize = iComponentClipSize;
					}
					else
					{
						iOtherClipSizes.PushAndGrow(iComponentClipSize);
					}
				}
			}
		}
	}

	// Large clip will be next number higher than default size
	if (iDefaultClipSize != -1)
	{
		for (int i = 0; i < iOtherClipSizes.GetCount(); i++)
		{
			if (iLargeClipSize == -1 && iOtherClipSizes[i] > iDefaultClipSize)
			{
				iLargeClipSize = iOtherClipSizes[i];
			}
			else if (iOtherClipSizes[i] > iDefaultClipSize && iOtherClipSizes[i] < iLargeClipSize)
			{
				iLargeClipSize = iOtherClipSizes[i];
			}
		}
	}

	// Drum clip will be the biggest after that
	if (iLargeClipSize != -1)
	{
		for (int i = 0; i < iOtherClipSizes.GetCount(); i++)
		{
			if (iOtherClipSizes[i] > iLargeClipSize)
			{
				iDrumClipSize = iOtherClipSizes[i];
			}
		}
	}

	// If there's no clip component marked as default, use the weaponinfo clip size
	if (iDefaultClipSize == -1)
	{
		iDefaultClipSize = pWeaponInfo->GetClipSize();
	}

	STORE_WEAPON_STATS_STRING(AmmoDefaultClipSize, "%i", iDefaultClipSize);
	STORE_WEAPON_STATS_STRING(AmmoLargeClipSize, "%i", iLargeClipSize);
	STORE_WEAPON_STATS_STRING(AmmoDrumClipSize, "%i", iDrumClipSize);

	//
	// HUD Stats
	//

	STORE_WEAPON_STATS_STRING(HudDamage, "%i", pWeaponInfo->GetHudDamage());
	STORE_WEAPON_STATS_STRING(HudFireRate, "%i", pWeaponInfo->GetHudSpeed());
	STORE_WEAPON_STATS_STRING(HudCapacity, "%i", pWeaponInfo->GetHudCapacity());
	STORE_WEAPON_STATS_STRING(HudAccuracy, "%i", pWeaponInfo->GetHudAccuracy());
	STORE_WEAPON_STATS_STRING(HudRange, "%i", pWeaponInfo->GetHudRange());

	// Generate and print stats
	atString weaponStatString;
	for(s32 i = 0; i < NUM_WEAPON_STATS; i++)
	{
		weaponStatString += statStrings[i];
		if(i != (NUM_WEAPON_STATS - 1))
			weaponStatString += ",";
	}
	weaponStatString += '\n';
	CFileMgr::Write(fileHandle, weaponStatString.c_str(), weaponStatString.GetLength());
}

#endif // __BANK
