//
// StatsInterface.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//


// --- Include Files ------------------------------------------------------------

//Rage Header
#include <time.h>
#include "rline/profilestats/rlprofilestats.h"
#include "rline/rl.h"
#include "diag/output.h"
#include "system/param.h"
#include "rline/savemigration/rlsavemigration.h "

//Framework Header
#include "fwscene/world/WorldLimits.h"
#include "fwnet/netobject.h"

// Game headers
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/stats_channel.h"
#include "Stats/StatsUtils.h"
#include "Stats/StatsSavesMgr.h"
#include "game/ModelIndices.h"
#include "network/Live/livemanager.h"
#include "network/NetworkInterface.h"
#include "network/Objects/Entities/NetObjGame.h"
#include "network/Cloud/Tunables.h"
#include "scene/DynamicEntity.h"
#include "scene/world/GameWorld.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "peds/Relationships.h"
#include "script/commands_stats.h"
#include "weapons/WeaponEnums.h"
#include "Modelinfo/PedModelInfo.h"
#include "Network/Objects/Synchronisation/SyncNodes/PlayerSyncNodes.h"
#include "weapons/info/WeaponInfo.h"
#include "weapons/WeaponTypes.h"

#if __ASSERT
#include "Text/TextFile.h"
#include "system/stack.h"
#include "scene/scene.h"
#include "control/gamelogic.h"
#include "script/thread.h"
#include "Stats/StatsTypes.h"
#endif // __ASSERT

FRONTEND_STATS_OPTIMISATIONS()
SAVEGAME_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

namespace StatsInterface
{
	PARAM(mpSavegameUseSc, "[stat_savemgr] Use Social Club savegames for multiplayer.");
	PARAM(netForceBadSport, "[stat_savemgr] Force local player to be a bad sport.");
	PARAM(forceCESP, "[stat_savemgr] Simulate that the local player previously purchased Criminal Enterprise Starter Pack");
	PARAM(forceNoCESP, "[stat_savemgr] Simulate that the local player did not previously purchase Criminal Enterprise Starter Pack");

	//Default values for badsport thresholds
	static const unsigned BADSPORT_THRESHOLD = 50;
	static const unsigned BADSPORT_THRESHOLD_NOTCHEATER = 45;

	//Represents the current character prefix that depends on the current player model.
	static u32  s_StatsPlayerModelPrefix = 0;
	static bool s_StatsPlayerModelValid = true;
	static bool s_StatsPlayerModelMultiplayer = true;

	//Total number of damage scars 
	static const int MAX_NUM_DAMAGE_SCARS = 24;
	CompileTimeAssert(CPedDamageSetBase::kMaxDecorationBlits == MAX_NUM_DAMAGE_SCARS);
	static const float SCAR_OFFSET_PACKING_MODIFIER = 10000.f; //use a multiplier here because it is stored in a position, and it is really an offset. Fix for losing precision (lavalley)

	//Setup the multiplayer savegame cloud service.
	static rlCloudOnlineService s_CloudSavegameService = RL_CLOUD_ONLINE_SERVICE_NATIVE;

	//Cache weapon hash Id.
	static StatId  s_WeaponHashIdHealdTime;
	static StatId  s_WeaponHashIdDbHealdTime;
	static StatId  s_WeaponHashIdShots;
	static StatId  s_WeaponHashIdHits;
	static StatId  s_WeaponHashIdHitsCar;
	static StatId  s_WeaponHashIdEnemyKills;
	static StatId  s_WeaponHashIdKills;
	static StatId  s_WeaponHashIdDeaths;
	static StatId  s_WeaponHashIdHeadShots;
	static u32     s_CurrWeaponHashUsed      = 0;
	static u32     s_CurrWeaponStatIndexUsed = 0;

	//Cache well known stats iterators - Freemode BAD-SPORT
	static CStatsDataMgr::StatsMap::Iterator s_StatIterIsBadSport;
	static CStatsDataMgr::StatsMap::Iterator s_StatIterOverallBadSport;

#if __ASSERT

struct StatAssertHistory
{
private:
	atArray < StatId > m_aKeys;

public:
	StatAssertHistory()
	{
		Reset();
	}

	void Reset()
	{
		m_aKeys.Reset();
	}

	//Return the number of invalid stats already recorded.
	unsigned  GetCount() const { return m_aKeys.GetCount(); }

	//Return TRUE if the error should be printed
	bool PrintError(const StatId& keyHash)
	{
		bool ret = true;

		for (int i=0; i<m_aKeys.GetCount(); i++)
		{
			if (m_aKeys[i].GetHash() == keyHash.GetHash())
			{
				ret = false;
				break;
			}
		}

		if (ret)
		{
			m_aKeys.PushAndGrow(keyHash);
		}

		return ret;
	}
};

	static StatAssertHistory s_AssertHistory;
#endif // __ASSERT

	void ValidateKey(const StatId& ASSERT_ONLY(keyHash), const char* ASSERT_ONLY(msg))
	{
#if __ASSERT
		if(CStatsUtils::CanUpdateStats() && !CStatsMgr::GetStatsDataMgr().IsKeyValid(keyHash) && NetworkInterface::GetActiveGamerInfo() && s_AssertHistory.PrintError(keyHash))
		{
			int modelIdx = -1;
			const char* modelName = NULL;
			if (CGameWorld::FindLocalPlayerSafe() && CGameWorld::FindLocalPlayer()->GetPedModelInfo() )
			{
				modelName = CGameWorld::FindLocalPlayer()->GetPedModelInfo()->GetModelName();
				modelIdx = CGameWorld::FindLocalPlayer()->GetModelIndex();
			}

			statErrorf("Invalid Key Stat hash=\"%d\", name=\"%s\", msg=\"%s\"", keyHash.GetHash(), keyHash.GetName(), msg);
			statErrorf(".......... Model Index = \"%d\"", modelIdx);
			statErrorf("........... Model Name = \"%s\"", modelName ? modelName : "Null");
			statErrorf("........ IsNetworkOpen = \"%s\"", NetworkInterface::IsNetworkOpen() ? "Yes" : "No");
			statErrorf("........... Stat Count = \"%u\"", s_AssertHistory.GetCount());
			sysStack::PrintStackTrace();
		}
#endif // __ASSERT
	}

	bool IsKeyValid(const StatId& keyHash)
	{
		return CStatsMgr::GetStatsDataMgr().IsKeyValid(keyHash);
	}

	eCharacterIndex GetCharacterIndex()
	{
		eCharacterIndex charIndex = CHAR_INVALID;

		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer)
		{
			u32 modelNameHash = pPlayer->GetPedModelInfo()->GetModelNameHash();

			if( modelNameHash == MI_PLAYERPED_PLAYER_ZERO.GetName() )
			{
				charIndex = CHAR_MICHEAL; // Micheal
			}
			else if( modelNameHash == MI_PLAYERPED_PLAYER_ONE.GetName() )
			{
				charIndex = CHAR_FRANKLIN; // Franklin
			}
			else if( modelNameHash == MI_PLAYERPED_PLAYER_TWO.GetName() )
			{
				charIndex = CHAR_TREVOR; // Trevor
			}

			if (NetworkInterface::IsNetworkOpen())
			{
				//We must be using a Multiplayer Model
				Assertf(StatsInterface::IsKeyValid("MPPLY_LAST_MP_CHAR"), "Stat MPPLY_LAST_MP_CHAR has changed name - fix this with new name");
				const int mpSlot = StatsInterface::GetIntStat(STAT_MPPLY_LAST_MP_CHAR);

				Assert(mpSlot<5);

				charIndex = static_cast<eCharacterIndex>(CHAR_MP_START+mpSlot);
			}
		}

		return charIndex;
	}

	int GetCharacterSlot()
	{
		const eCharacterIndex charIndex = GetCharacterIndex();

		if (NetworkInterface::IsNetworkOpen())
		{
			return (charIndex-CHAR_MP_START);
		}

		return charIndex;
	}

	bool ValidCharacterSlot(const int slot)
	{
		if (NetworkInterface::IsNetworkOpen())
		{
			return (0 <= slot && slot < MAX_NUM_MP_CHARS);
		}

		return (0 <= slot && slot < MAX_NUM_SP_CHARS);
	}

	const char* GetKeyName(const StatId& keyHash)
	{
		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatDataPtr* stat = statsDataMgr.GetStatPtr(keyHash);
		if (stat)
		{
#if __BANK && !__NO_OUTPUT
			return stat->GetKeyName();
#elif !__FINAL
			static char s_statName[MAX_STAT_LABEL_SIZE];
			snprintf(s_statName, MAX_STAT_LABEL_SIZE, "Hash=0x%08x", (u32)stat->GetKey());
			return s_statName;
#endif // __BANK && !__NO_OUTPUT
		}

		return "UNKNOWN_STAT_ID";
	}

	bool IsKeyValid(const int keyHash)
	{
		return CStatsMgr::GetStatsDataMgr().IsKeyValid(STAT_ID(keyHash));
	}

	bool IsKeyValid(const char* str)
	{
		return CStatsMgr::GetStatsDataMgr().IsKeyValid(STAT_ID(str));
	}

	bool GetStatIsNotNull(const StatId& keyHash)
	{
		return CStatsMgr::GetStatsDataMgr().GetStatIsNotNull(keyHash);
	}

	StatType  GetStatType(const StatId& keyHash)
	{
		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetType();
		}

		return STAT_TYPE_NONE;
	}

	rlProfileStatsType  GetExpectedProfileStatType(const StatId& keyHash)
	{
		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetExpectedProfileStatType();
		}

		return RL_PROFILESTATS_TYPE_INVALID;
	}


	const char*   GetStatTypeLabel(const StatId& keyHash)
	{
		return CStatsMgr::GetStatsDataMgr().GetStatTypeLabel(keyHash);
	}

	const sStatDescription* GetStatDesc(const StatId& keyHash)
	{
		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return &stat->GetDesc();
		}

		return NULL;
	}

	bool  GetIsBaseType(const StatId& keyHash, const StatType type)
	{
		return CStatsMgr::GetStatsDataMgr().GetIsBaseType(keyHash, type);
	}

	void  SignedOut()
	{
		CStatsMgr::SignedOut();
	}

	void  SignedIn()
	{
		CStatsMgr::SignedIn();
	}

	bool CanAccessStat(const StatId& keyHash)
	{
		if(!CStatsMgr::GetStatsDataMgr().IsStatsDataValid())
			return false;

		const sStatDescription* statDesc = GetStatDesc(keyHash);
		if(!statDesc)
			return false;

		if(!NetworkInterface::IsLocalPlayerOnline())
			return false;

		if(StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT))
			return false;

		if(StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			return false;

		if(statDesc->GetServerAuthoritative())
		{
			CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
			if(!CLiveManager::GetProfileStatsMgr().Synchronized(CProfileStats::PS_SYNC_MP))
				return false;

			int errorCode = 0;
			if(profileStatsMgr.SynchronizedFailed(CProfileStats::PS_SYNC_MP, errorCode))
				return false;
		}

		if(statDesc->GetIsCharacterStat())
		{
			const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot() + 1;
			if(StatsInterface::CloudFileLoadPending(selectedSlot))
				return false;

			if(StatsInterface::CloudFileLoadFailed(selectedSlot))
				return false;
		}

		return true;
	}


	bool  GetStatData(const StatId& keyHash, void* pData, const unsigned SizeofData)
	{
		ValidateKey(keyHash, "SetStatData");
		return CStatsMgr::GetStatsDataMgr().GetStatData(keyHash, pData, SizeofData);
	}

	bool  SetStatData(const StatId& keyHash, void* pData, const unsigned SizeofData, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return false;
		ValidateKey(keyHash, "SetStatData");
		return CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, pData, SizeofData, flags);
	}

	void  SetStatData(const StatId& keyHash, int data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");
		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT) || StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT64) 
					|| (data >= 0 && (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT8)  
										|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT16) 
										|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT32) 
										|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64)
										|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PACKED)
										|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PROFILE_SETTING))), "Invalid Stat key=\"%s\" hash=\"%d\" value=\"%d\"", keyHash.GetName(), keyHash.GetHash(), data);

		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT) || StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PROFILE_SETTING))
		{
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT64))
		{
			s64 datatemp = (s64)data;
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &datatemp, sizeof(s64), flags);
		}
		else if (data >= 0)
		{
			if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT8))
			{
				u8 datatemp = (u8)data;
				CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &datatemp, sizeof(u8), flags);
			}
			else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT16))
			{
				u16 datatemp = (u16)data;
				CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &datatemp, sizeof(u16), flags);
			}
			else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT32))
			{
				u32 datatemp = (u32)data;
				CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &datatemp, sizeof(u32), flags);
			}
			else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64) || StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PACKED))
			{
				u64 datatemp = (u64)data;
				CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &datatemp, sizeof(u64), flags);
			}
		}
	}

	void  SetStatData(const StatId& keyHash, float data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");
		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT), "Invalid Float stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT))
		{
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
		}
	}

	void  SetStatData(const StatId& keyHash, char* data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");
		statAssert(data);
		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_TEXTLABEL) 
					|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_STRING)
					|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_USERID)
					, "Invalid String stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());

		if (data)
		{
			if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_TEXTLABEL))
			{
				const char* text = static_cast<const char*>(data);
				int stringKeyHash = atStringHash(text);
				CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &stringKeyHash, sizeof(stringKeyHash), flags);
				statAssertf(TheText.DoesTextLabelExist(text), "Label \"%s\" for stat key=\"%s\" hash=\"%d\" doesnt exist.", text, keyHash.GetName(), keyHash.GetHash());
			}
			else if(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_STRING) || StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_USERID))
			{
				CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, data, ustrlen(data), flags);
			}
		}
	}

	void  SetStatData(const StatId& keyHash, bool data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");
		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_BOOLEAN), "Invalid Bool stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_BOOLEAN))
		{
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
		}
	}

	void  SetStatData(const StatId& keyHash, u8 data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");
		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT8), "Invalid u8 stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT8))
		{
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
		}
	}

	void  SetStatData(const StatId& keyHash, u16 data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");
		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT16), "Invalid u16 stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT16))
		{
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
		}
	}

	void  SetStatData(const StatId& keyHash, u32 data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");
		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT32), "Invalid u32 stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT32))
		{
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
		}
	}

	void  SetStatData(const StatId& keyHash, u64 data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");

		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64) 
			|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_DATE) 
			|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_POS)
			|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PACKED)
			|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_USERID), "Invalid u64 stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());

		CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
	}

	void  SetStatData(const StatId& keyHash, s64 data, const u32 flags)
	{
		if (!CStatsUtils::CanUpdateStats())
			return;
		ValidateKey(keyHash, "SetStatData");

		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT64), "Invalid s64 stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());

		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT64))
		{
			CStatsMgr::GetStatsDataMgr().SetStatData(keyHash, &data, sizeof(data), flags);
		}
	}

	void  SetSynched(const StatId& keyHash)
	{
		ValidateKey(keyHash, "SetStatData");
		CStatsMgr::GetStatsDataMgr().SetStatSynched(keyHash);
	}

	void  SetAllStatsToSynched(const int savegameSlot, const bool onlinestat, const bool includeServerAuthoritative)
	{
		CStatsMgr::GetStatsDataMgr().SetAllStatsToSynched(savegameSlot, onlinestat, includeServerAuthoritative);
	}

	void  SetAllStatsToSynchedByGroup(const eProfileStatsGroups group, const bool onlinestat, const bool includeServerAuthoritative)
	{
		CStatsMgr::GetStatsDataMgr().SetAllStatsToSynchedByGroup(group, onlinestat, includeServerAuthoritative);
	}

	bool  CloudFileLoadPending(const int file)
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().IsLoadPending(file);
	}

	void SetNoRetryOnFail(bool value)
	{
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().SetNoRetryOnFail(value);
	}

	bool  GetBlockSaveRequests()
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().GetBlockSaveRequests();
	}

	bool  CloudFileLoadFailed(const int file)
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().CloudLoadFailed(file);
	}

	bool  LoadPending(const int slot)
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().IsLoadInProgress(slot);
	}

	bool  SavePending()
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().IsSaveInProgress();
	}

	bool CloudSaveFailed(const u32 slotNumber)
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().CloudSaveFailed(slotNumber);
	}

	bool PendingSaveRequests()
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().PendingSaveRequests();
	}

	void ClearSaveFailed(const int file)
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearSaveFailed(file);
	}

	bool  Load(const eStatsLoadType type, const int slotNumber)
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().Load(type, slotNumber);
	}

	void   TryMultiplayerSave( const eSaveTypes savetype, u32 uSaveDelay )
	{
		if (!NetworkInterface::IsLocalPlayerOnline())
			return;

		if (!NetworkInterface::IsGameInProgress())
			return;

		const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot( ) + 1;
		if (StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT))
			return;
		if (StatsInterface::CloudFileLoadPending(selectedSlot))
			return;

		if (StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			return;
		if (StatsInterface::CloudFileLoadFailed(selectedSlot))
			return;

		StatsInterface::Save(STATS_SAVE_CLOUD, STAT_MP_CATEGORY_DEFAULT, savetype, uSaveDelay);
	}

	bool  Save(const eStatsSaveType type, const u32 slotNumber, const eSaveTypes savetype, const u32 uSaveDelay, const int reason)
	{
		return CStatsMgr::GetStatsDataMgr().GetSavesMgr().Save(type, slotNumber, savetype, uSaveDelay, reason);
	}

	int  GetIntStat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetIntStat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetInt();
		}

		return 0;
	}

	float  GetFloatStat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetFloatStat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetFloat();
		}

		return 0.0f;
	}

	bool  GetBooleanStat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetBooleanStat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetBoolean();
		}

		return false;
	}

	u8  GetUInt8Stat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetUInt8Stat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetUInt8();
		}

		return 0;
	}

	u16  GetUInt16Stat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetUInt16Stat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetUInt16();
		}

		return 0;
	}

	u32  GetUInt32Stat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetUInt32Stat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetUInt32();
		}

		return 0;
	}

	u64  GetUInt64Stat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetUInt64Stat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetUInt64();
		}

		return 0;
	}

	s64  GetInt64Stat(const StatId& keyHash)
	{
		ValidateKey(keyHash, "GetInt64Stat");

		const CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		const sStatData* stat = statsDataMgr.GetStat(keyHash);
		if (stat)
		{
			return stat->GetInt64();
		}

		return 0;
	}

	void  IncrementStat(const StatId& keyHash, const float fAmount, const u32 flags)
	{
		statAssertf(CStatsMgr::GetStatsDataMgr().IsKeyValid(keyHash), "Invalid Stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		CStatsMgr::IncrementStat(keyHash, fAmount, flags);
	}

	void  DecrementStat(const StatId& keyHash, const float fAmount, const u32 flags)
	{
		statAssertf(CStatsMgr::GetStatsDataMgr().IsKeyValid(keyHash), "Invalid Stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		CStatsMgr::DecrementStat(keyHash, fAmount, flags);
	}

	void  SetGreater(const StatId& keyHash, const float fAmount, const u32 flags)
	{
		statAssertf(CStatsMgr::GetStatsDataMgr().IsKeyValid(keyHash), "Invalid Stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		CStatsMgr::SetGreater(keyHash, fAmount, flags);
	}

	void  SetLesser(const StatId& keyHash, const float fAmount, const u32 flags)
	{
		statAssertf(CStatsMgr::GetStatsDataMgr().IsKeyValid(keyHash), "Invalid Stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash());
		CStatsMgr::SetLesser(keyHash, fAmount, flags);
	}

	bool  SyncProfileStats(const bool force, const bool multiplayer)
	{
		return CLiveManager::GetProfileStatsMgr().Synchronize(force, multiplayer);
	}

	bool  CanSyncProfileStats()
	{
		return CLiveManager::GetProfileStatsMgr().CanSynchronize();
	}

	void  UnPackDate(const u64 date, int& year, int& month, int& day, int& hour, int& min, int& sec, int& mill)
	{
		year = month = day = hour = min = sec = mill = 0;

		year  = (int)(date >> (52) & 0xFFF); // 12 bits (<=4095)
		month = (int)(date >> (48) &   0xF); //  4 bits (<=15)
		day   = (int)(date >> (43) &  0x1F); //  5 bits (<=31)
		hour  = (int)(date >> (38) &  0x1F); //  5 bits (<=31)
		min   = (int)(date >> (32) &  0x3F); //  6 bits (<=63)
		sec   = (int)(date >> (26) &  0x3F); //  6 bits (<=63)
		mill  = (int)(date >>  (0) & 0x3FF); // 10 bits (<=1023)
	}

	void  GetBSTDate(int& year, int& month, int& day, int& hour, int& min, int& sec)
	{
		// year - years since 1900
		// mon  - months since January	 (0-11)
		// day  - day of the month	     (1-31)
		// hour - hours since midnight	 (0-23)
		// min  - minutes after the hour   (0-59)
		// sec  - seconds after the minute (0-61)

		year = month = day = hour = min = sec = 0;

		time_t date = (time_t)(rlGetPosixTime() + 3600);

		struct tm* ptm;
		ptm = gmtime(&date);

		if (ptm)
		{
			year  = ptm->tm_year+1900;
			month = ptm->tm_mon+1;
			day   = ptm->tm_mday;
			hour  = ptm->tm_hour;
			min   = ptm->tm_min;
			sec   = ptm->tm_sec;
		}
	}

	void  GetPosixDate(int& year, int& month, int& day, int& hour, int& min, int& sec)
	{
		// year - years since 1900
		// mon  - months since January	 (0-11)
		// day  - day of the month	     (1-31)
		// hour - hours since midnight	 (0-23)
		// min  - minutes after the hour   (0-59)
		// sec  - seconds after the minute (0-61)

		year = month = day = hour = min = sec = 0;

		time_t date = (time_t)(rlGetPosixTime());

		struct tm* ptm;
		ptm = gmtime(&date);

		if (ptm)
		{
			year  = ptm->tm_year+1900;
			month = ptm->tm_mon+1;
			day   = ptm->tm_mday;
			hour  = ptm->tm_hour;
			min   = ptm->tm_min;
			sec   = ptm->tm_sec;
		}

		//statDebugf2("Posix Time = %" I64FMT "d, %d:%d:%d, %d-%d-%d"
		//				,date
		//				,hour
		//				,min
		//				,sec
		//				,day
		//				,month
		//				,year);
	}

	u64  PackDate(const u64 year, const u64 month, const u64 day, const u64 hour, const u64 min, const u64 sec, const u64 mill)
	{
			statAssert(year  <= 0xFFF);
			statAssert(month <=   0xF);
			statAssert(day   <=  0x1F);
			statAssert(hour  <=  0x1F);
			statAssert(min   <=  0x3F);
			statAssert(sec   <=  0x3F);
			statAssert(mill  <= 0x3FF);

		return (year << 52 | month << 48 | day << 43 | hour << 38 | min << 32 | sec << 26 | mill);
	}

	#define DECIMAL_PRECISION (10) // 1 decimal

	void  UnPackPos(const u64 pos, float& x, float& y, float& z)
	{
		x = y = z = 0.0f;
		float decimalX = 0;
		float decimalY = 0;
		float decimalZ = 0;

		decimalX = (float) (pos >> 58 & 0x3F);   // 6 bits
		       x = (float) (pos >> 44 & 0x3FFF); // 14 bits
		decimalY = (float) (pos >> 38 & 0x3F);   // 6 bits
		       y = (float) (pos >> 24 & 0x3FFF); // 14 bits
		decimalZ = (float) (pos >> 18 & 0x3F);   // 6 bits
		       z = (float) (pos >>  4 & 0x3FFF); // 14 bits

		int sign = 0;
		sign = (int)(pos >> (0) & 0xF); // 4 bits

		x += ((float)1/DECIMAL_PRECISION)*(float)decimalX;
		y += ((float)1/DECIMAL_PRECISION)*(float)decimalY;
		z += ((float)1/DECIMAL_PRECISION)*(float)decimalZ;

		if (sign&BIT(0)) x *= -1.0f; //First  bit set
		if (sign&BIT(1)) y *= -1.0f; //Second bit set
		if (sign&BIT(2)) z *= -1.0f; //Third  bit set

		statAssert(x <= WORLDLIMITS_XMAX);
		statAssert(y <= WORLDLIMITS_YMAX);
		statAssert(z <= WORLDLIMITS_ZMAX);

		statAssert(x >= WORLDLIMITS_XMIN);
		statAssert(y >= WORLDLIMITS_YMIN);
		statAssert(z >= WORLDLIMITS_ZMIN);
	}

	u64  PackPos(float x, float y, float z)
	{
		statAssert(x <= WORLDLIMITS_XMAX);
		statAssert(y <= WORLDLIMITS_YMAX);
		statAssert(z <= WORLDLIMITS_ZMAX);

		statAssert(x >= WORLDLIMITS_XMIN);
		statAssert(y >= WORLDLIMITS_YMIN);
		statAssert(z >= WORLDLIMITS_ZMIN);

		//Setup sign bits
		int sign = 0;
		if (0.0f > x) sign |= BIT(0); //First  bit set
		if (0.0f > y) sign |= BIT(1); //Second bit set
		if (0.0f > z) sign |= BIT(2); //Third  bit set

		//Make number signed
		if (0.0f > x) x *= -1.0f; //First  bit set
		if (0.0f > y) y *= -1.0f; //Second bit set
		if (0.0f > z) z *= -1.0f; //Third  bit set

		u64 integerPartX = static_cast<u64> (x);
		u64 decimalPartX = ((u64)(x*DECIMAL_PRECISION)%DECIMAL_PRECISION);
		u64 integerPartY = static_cast<u64> (y);
		u64 decimalPartY = ((u64)(y*DECIMAL_PRECISION)%DECIMAL_PRECISION);
		u64 integerPartZ = static_cast<u64> (z);
		u64 decimalPartZ = ((u64)(z*DECIMAL_PRECISION)%DECIMAL_PRECISION);

		return (decimalPartX << 58 | integerPartX << 44 | decimalPartY << 38 | integerPartY << 24 | decimalPartZ << 18 | integerPartZ << 4 | sign);
	}

	bool  IsGangMember(const CPed* ped)
	{
		statAssert(NetworkInterface::IsGameInProgress());

		bool isGangMember = false;

		if (statVerify(ped) && ped->GetNetworkObject())
		{
			if (ped->IsPlayer() && ped->GetNetworkObject()->GetPlayerOwner())
			{
				switch (ped->GetNetworkObject()->GetPlayerOwner()->GetTeam())
				{
				case MP_TEAM_CRIM_GANG_1: 
				case MP_TEAM_CRIM_GANG_2: 
				case MP_TEAM_CRIM_GANG_3: 
				case MP_TEAM_CRIM_GANG_4: 
					isGangMember = true; 
					break;

				case MP_TEAM_COP: 
				default:
					break;
				}
			}
			else if (GetPedOfType(ped, MP_GROUP_GANG_VAGOS) || GetPedOfType(ped, MP_GROUP_GANG_LOST))
			{
				isGangMember = true;
			}
		}

		return isGangMember;
	}

	bool  GetPedOfType(const CPed* ped, const u32 type)
	{
		statAssert(NetworkInterface::IsGameInProgress());
		statAssert(type == MP_GROUP_COP || type == MP_GROUP_GANG_VAGOS || type == MP_GROUP_GANG_LOST);

		bool result = false;

		if (statVerify(ped) && ped->GetNetworkObject())
		{
			CPedModelInfo* mi = ped->GetPedModelInfo();

			if (statVerify(mi))
			{
				result = (mi->GetRelationshipGroupHash() == type);
			}
		}

		return result;
	}

	bool  IsSameTeam(const CPed* pedA, const CPed* pedB)
	{
		statAssert(NetworkInterface::IsGameInProgress());

		bool isSameTeam = false;

		if (statVerify(pedA) && statVerify(pedB) && pedA->GetNetworkObject() && pedB->GetNetworkObject())
		{
			if (pedA->IsPlayer() && pedB->IsPlayer() && pedA->GetNetworkObject()->GetPlayerOwner() && pedB->GetNetworkObject()->GetPlayerOwner())
			{
				isSameTeam = ( pedA->GetNetworkObject()->GetPlayerOwner()->GetTeam() == pedB->GetNetworkObject()->GetPlayerOwner()->GetTeam() );
			}
			else if(pedA->IsPlayer() && pedA->GetNetworkObject()->GetPlayerOwner())
			{
				if (IsPlayerCop(pedA) && StatsInterface::GetPedOfType(pedB, StatsInterface::MP_GROUP_COP))
				{
					isSameTeam = true;
				}
				else if (IsPlayerVagos(pedA) && StatsInterface::GetPedOfType(pedB, StatsInterface::MP_GROUP_GANG_VAGOS))
				{
					isSameTeam = true;
				}
				else if (IsPlayerLost(pedA) && StatsInterface::GetPedOfType(pedB, StatsInterface::MP_GROUP_GANG_LOST))
				{
					isSameTeam = true;
				}
			}
			else if(pedB->IsPlayer() && pedB->GetNetworkObject()->GetPlayerOwner())
			{
				if (IsPlayerCop(pedB) && StatsInterface::GetPedOfType(pedA, StatsInterface::MP_GROUP_COP))
				{
					isSameTeam = true;
				}
				else if (IsPlayerVagos(pedB) && StatsInterface::GetPedOfType(pedA, StatsInterface::MP_GROUP_GANG_VAGOS))
				{
					isSameTeam = true;
				}
				else if (IsPlayerLost(pedB) && StatsInterface::GetPedOfType(pedA, StatsInterface::MP_GROUP_GANG_LOST))
				{
					isSameTeam = true;
				}
			}
			else
			{
				CPedModelInfo* miA = pedA->GetPedModelInfo();
				CPedModelInfo* miB = pedB->GetPedModelInfo();

				if (statVerify(miA) && statVerify(miB))
				{
					isSameTeam = (miA->GetRelationshipGroupHash() == miB->GetRelationshipGroupHash());
				}
			}
		}

		return isSameTeam;
	}

	bool  IsPlayerCop(const CPed* player)
	{
		statAssert(NetworkInterface::IsGameInProgress());

		bool iscop = false;

		if (statVerify(player && player->IsPlayer() && player->GetNetworkObject() && player->GetNetworkObject()->GetPlayerOwner()))
		{
			iscop = (player->GetNetworkObject()->GetPlayerOwner()->GetTeam() == MP_TEAM_COP);
		}
	
		return iscop;
	}

	bool  IsPlayerVagos(const CPed* player)
	{
		statAssert(NetworkInterface::IsGameInProgress());

		bool isvagos = false;

		if (statVerify(player && player->IsPlayer() && player->GetNetworkObject() && player->GetNetworkObject()->GetPlayerOwner()))
		{
			isvagos = (player->GetNetworkObject()->GetPlayerOwner()->GetTeam() == MP_TEAM_CRIM_GANG_1);
		}

		return isvagos;
	}

	bool  IsPlayerLost(const CPed* player)
	{
		statAssert(NetworkInterface::IsGameInProgress());

		bool islost = false;

		if (statVerify(player && player->IsPlayer() && player->GetNetworkObject() && player->GetNetworkObject()->GetPlayerOwner()))
		{
			islost = (player->GetNetworkObject()->GetPlayerOwner()->GetTeam() == MP_TEAM_CRIM_GANG_2);
		}

		return islost;
	}

	bool  FindStatIterator(const StatId& keyHash, CStatsDataMgr::StatsMap::Iterator& statIter  ASSERT_ONLY(, const char* debugText))
	{
		CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
		statsDataMgr.StatIterFind(keyHash, statIter);

		statAssertf(statIter.IsValid(), "Invalid stat %s - iterator is invalid, keyHash=%d", debugText, keyHash.GetHash());
		if (!statIter.IsValid())
			return false;

		statAssertf(statIter.GetKey().IsValid(), "Invalid stat %s - key is not valid, keyHash=%d", debugText, keyHash.GetHash());
		if (!statIter.GetKey().IsValid())
			return false;

		statAssertf(statIter.GetKey().GetHash() == keyHash.GetHash(), "Invalid stat %s - iterator key=%d != keyHash=%d", debugText, statIter.GetKey().GetHash(), keyHash.GetHash());
		if (statIter.GetKey().GetHash() != keyHash.GetHash())
			return false;

		return true;
	}

#if KEYBOARD_MOUSE_SUPPORT
	void UpdateUsingKeyboard(bool usingKeyboard, const s32 lastFrameDeviceIndex, const s32 deviceIndex)
	{
		if(!NetworkInterface::IsGameInProgress())
			return;

		if(StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT))
			return;

		if(StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			return;

		static const StatId s_statId(ATSTRINGHASH("MPPLY_USING_KEYBOARD", 0xC88D7CFA));
		static CStatsDataMgr::StatsMap::Iterator s_StatIter;

		if (!s_StatIter.IsValid() || s_StatIter.GetKey().GetHash() != s_statId.GetHash())
		{
			StatsInterface::FindStatIterator(s_statId.GetHash(), s_StatIter  ASSERT_ONLY(, "MPPLY_USING_KEYBOARD"));
		}

		if (s_StatIter.IsValid() && s_StatIter.GetKey().GetHash() == s_statId.GetHash())
		{
			const sStatData* statdata = *s_StatIter;
			if (statdata)
			{
				if (lastFrameDeviceIndex != deviceIndex || statdata->GetBoolean() != usingKeyboard)
				{
					CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();
					statsDataMgr.SetStatIterData(s_StatIter, &usingKeyboard, statdata->GetSizeOfData());
				}
			}
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	void  RegisterParachuteDeployed(CPed* ped)
	{
		if(!ped)
			return;
		if(ped->IsNetworkClone())
			return;
		CStatsMgr::RegisterParachuteDeployed(ped);
	}

	void RegisterPrepareParachuteDeploy(CPed* ped)
	{
		if(!ped)
			return;
		if(ped->IsNetworkClone())
			return;
		CStatsMgr::RegisterPreOpenParachute(ped);
	}

	void RegisterStartSkydiving(CPed* pPed)
	{
		if(!pPed)
			return;
		if(pPed->IsNetworkClone())
			return;
		CStatsMgr::RegisterStartSkydiving(pPed);
	}

	void RegisterDestroyParachute(const CPed* pPed)
	{
		if(!pPed)
			return;
		if(pPed->IsNetworkClone())
			return;
		CStatsMgr::RegisterDestroyParachute(pPed);
	}


	void FallenThroughTheMap(CPed* pPed)
	{
		if(!pPed)
			return;
		if(pPed->IsNetworkClone())
			return;
		CStatsMgr::FallenThroughTheMap(pPed);
	}

	bool  IsBadSport(bool fromScript OUTPUT_ONLY(, const bool displayOutput))
	{
#if !__FINAL
		int nBadSport = 0;
		if(PARAM_netForceBadSport.Get(nBadSport))
			return nBadSport > 0 ? true : false; 
#endif

		u32 posixTimeStart = 0;
		if(CLiveManager::GetSCGamerDataMgr().BadSportOverrideStart(posixTimeStart))
		{
			u32 posixTimeEnd = 0;
			if (statVerify(CLiveManager::GetSCGamerDataMgr().BadSportOverrideEnd(posixTimeEnd)))
			{
				const u64 currentPosixTime = rlGetPosixTime();
				statDebugf1("IsBadSport:: BadSportOverrideStart=%d | BadSportOverrideEnd=%d | PosixTime=%" I64FMT "d", posixTimeStart, posixTimeEnd, currentPosixTime);
				if (posixTimeStart <= (u64)currentPosixTime && currentPosixTime <= (u64)posixTimeEnd)
				{
					statDebugf1("IsBadSport:: override using GetSCGamerDataMgr().BadSportOverrideStart | GetSCGamerDataMgr().BadSportOverrideEnd");
					if(CLiveManager::GetSCGamerDataMgr().HasBadSportOverride())
					{
						return false;
					}
				}
			}
		}

        // check gamer data setting (only positive return value is effective)
        if(CLiveManager::GetSCGamerDataMgr().IsBadSport())
		{
			statDebugf1("IsBadSport:: override using GetSCGamerDataMgr().IsBadSport()");
            return true;
		}

		const bool statsLoaded = !StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT);

		if(!statsLoaded && !fromScript)
		{
			statDebugf1("IsBadSport:: Stats not loaded. Returning false.");
			return false;
		}

		bool isBadSport = false;

#if __ASSERT
		bool isTestLevel = (CScene::GetLevelsData().GetLevelIndexFromFriendlyName("gta5") != CGameLogic::GetCurrentLevelIndex());
		statAssertf(statsLoaded || isTestLevel, "statsLoaded=%s, isTestLevel=%s", statsLoaded ? "true" : "false", isTestLevel ? "true" : "false");
#endif // __ASSERT

		if (statsLoaded)
		{
			CStatsDataMgr& statsDataMgr = CStatsMgr::GetStatsDataMgr();

			if (!s_StatIterIsBadSport.IsValid() || s_StatIterIsBadSport.GetKey().GetHash() != STAT_MPPLY_IS_BADSPORT.GetHash())
			{
				if (!FindStatIterator(STAT_MPPLY_IS_BADSPORT.GetHash(), s_StatIterIsBadSport  ASSERT_ONLY(, "STAT_MPPLY_IS_BADSPORT")))
					return isBadSport;
			}
			statAssert(s_StatIterIsBadSport.GetKey().GetHash() == STAT_MPPLY_IS_BADSPORT.GetHash());

			const sStatData* statDataIsBadSport = *s_StatIterIsBadSport;
			statAssert(statDataIsBadSport);
			if (!statDataIsBadSport)
				return isBadSport;

			if (!s_StatIterOverallBadSport.IsValid() || s_StatIterOverallBadSport.GetKey().GetHash() != STAT_MPPLY_OVERALL_BADSPORT.GetHash())
			{
				if (!FindStatIterator(STAT_MPPLY_OVERALL_BADSPORT.GetHash(), s_StatIterOverallBadSport  ASSERT_ONLY(, "STAT_MPPLY_OVERALL_BADSPORT")))
					return isBadSport;
			}
			statAssert(s_StatIterOverallBadSport.GetKey().GetHash() == STAT_MPPLY_OVERALL_BADSPORT.GetHash());

			const sStatData* statDataOverallBadSport = *s_StatIterOverallBadSport;
			statAssert(statDataOverallBadSport);
			if (!statDataOverallBadSport)
				return isBadSport;

			isBadSport = statDataIsBadSport->GetBoolean();
			if (isBadSport)
			{
				int notBadSportTreshold = BADSPORT_THRESHOLD_NOTCHEATER;
				Tunables::GetInstance().Access(MP_GLOBAL_HASH, ATSTRINGHASH("BADSPORT_THRESHOLD_NOTCHEATER", 0x727f6777), notBadSportTreshold);

				if (statDataOverallBadSport->GetInt() <= notBadSportTreshold)
				{
					isBadSport = false;
				}
			}
			else
			{
				int badsportTreshold = BADSPORT_THRESHOLD;
				Tunables::GetInstance().Access(MP_GLOBAL_HASH, ATSTRINGHASH("BADSPORT_THRESHOLD", 0x4c507c3d), badsportTreshold);

				isBadSport = (statDataOverallBadSport->GetInt() >= badsportTreshold);
			}

			if (isBadSport != statDataIsBadSport->GetBoolean())
			{
				statsDataMgr.SetStatIterData(s_StatIterIsBadSport, &isBadSport, statDataIsBadSport->GetSizeOfData());
			}

#if !__NO_OUTPUT
			if (displayOutput)
			{
				statDebugf3("Current bad sport value is < %d > - isBadSport=<%s>", statDataOverallBadSport->GetInt(), isBadSport ? "True" : "False");
			}
#endif
		}

		return isBadSport;
	}

	int  GetPedModelPrefix(CPed* playerPed)
	{
		int prefix = -1;

		CPedModelInfo* mi = playerPed ? playerPed->GetPedModelInfo() : 0;
		if(mi)
		{
			u32 modelNameHash = mi->GetModelNameHash();
			if( modelNameHash == MI_PLAYERPED_PLAYER_ZERO.GetName() )
			{
				prefix = 0; // Micheal
			}
			else if( modelNameHash == MI_PLAYERPED_PLAYER_ONE.GetName() )
			{
				prefix = 1; // Franklin
			}
			else if( modelNameHash == MI_PLAYERPED_PLAYER_TWO.GetName() )
			{
				prefix = 2; // Trevor
			}
			//We must be using a Multiplayer Model
			else if(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			{
				const int mpSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
				Assert(mpSlot < MAX_NUM_MP_CHARS);
				prefix = mpSlot+CHAR_MP_START;
			}
			else
			{
				statWarningf("Assuming a multiplayer model %s - but stats are not loaded.", mi->GetModelName());
			}
		}

		return prefix;
	}

	void  SetStatsModelPrefix(CPed* playerPed)
	{
		CPedModelInfo* mi = playerPed && playerPed->IsLocalPlayer() ? playerPed->GetPedModelInfo() : 0;
		if(mi)
		{
			const bool wasModelValid = s_StatsPlayerModelValid;

			s_StatsPlayerModelValid = false;

			int prefix = StatsInterface::GetPedModelPrefix(playerPed);

			if (-1 < prefix && statVerify(prefix < MAX_STATS_CHAR_INDEXES))
			{
				const bool spCharChange = (wasModelValid && prefix != (int)s_StatsPlayerModelPrefix && prefix < CHAR_MP_START && s_StatsPlayerModelPrefix < CHAR_MP_START);

				s_StatsPlayerModelValid  = true;
				s_StatsPlayerModelPrefix = (u32)prefix;

				s_StatsPlayerModelMultiplayer = (s_StatsPlayerModelPrefix >= CHAR_MP_START);

				if (spCharChange)
				{
					StatsInterface::IncrementStat(StatId("SP_TOTAL_SWITCHES"), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
					StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("TOTAL_SWITCHES"), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
				}

				const CWeaponInfo* wi = playerPed->GetEquippedWeaponInfo();
				if (wi)
				{
					if(playerPed->GetIsInVehicle() && wi->GetIsVehicleWeapon())
					{
						const CVehicle* vehicle = playerPed->GetVehiclePedInside();

						if (statVerify(vehicle) && statVerify(vehicle->GetVehicleModelInfo()))
						{
#if __ASSERT
							const char* vehicleName = vehicle->GetVehicleModelInfo()->GetGameName();
							if (AssertVerify(vehicleName)) statAssertf(vehicleName[0] != '\0', "Vehicle name \"%s\" is empty for Vehicle Model Info \"%s:%d\"", vehicle->GetVehicleModelInfo()->GetVehicleMakeName(), vehicle->GetVehicleModelInfo()->GetModelName(), vehicle->GetVehicleModelInfo()->GetModelNameHash());
#endif //__ASSERT

							UpdateLocalPlayerWeaponInfoStatId(wi, vehicle->GetVehicleModelInfo()->GetGameName());
						}
					}
					else
					{
						UpdateLocalPlayerWeaponInfoStatId(wi, NULL);
					}
				}
			}

			statDebugf1("SetStatsModelPrefix: s_StatsPlayerModelValid=%s, perfix=%d", s_StatsPlayerModelValid?"true":"false", s_StatsPlayerModelPrefix);
		}
	}

	u32 GetStatsModelPrefix()
	{
		return s_StatsPlayerModelPrefix;
	}

	StatId GetStatsModelHashId(const char *stat)
	{
		char statString[MAX_STAT_LABEL_SIZE];
		statString[0] = '\0';

		if (GetStatsModelPrefix())
		{
			safecatf(statString,MAX_STAT_LABEL_SIZE,"%s_%s",s_StatsModelPrefixes[GetStatsModelPrefix()],stat);
		}
		else
		{
			safecatf(statString,MAX_STAT_LABEL_SIZE,"%s_%s",s_StatsModelPrefixes[0],stat);
		}

		//Create a statid, that way we ensure that whatever is generating the hash in there will match this one.
		return STAT_ID(statString);
	}

	bool   GetStatsPlayerModelValid()
	{
		return s_StatsPlayerModelValid;
	}

	bool  GetStatsPlayerModelIsMultiplayer()
	{
		return (s_StatsPlayerModelValid && s_StatsPlayerModelMultiplayer);
	}

	bool  GetStatsPlayerModelIsSingleplayer()
	{
		return (s_StatsPlayerModelValid && !s_StatsPlayerModelMultiplayer);
	}

	bool SetMaskedInt(const StatId& keyHash, int data, int shift, int numberOfBits, const bool ASSERT_ONLY(/*coderAssert*/), const u32 flags)
	{
		ValidateKey(keyHash, "SetMaskedInt");

		bool ret = false;

		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64) 
					|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PACKED) 
					|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT), "SetMaskedInt - Invalid Stat=\"%d\" type.", keyHash.GetHash());

		//STAT_SET_MASKED_INT(statHash, statValue, 8, 8) would set the second byte in a stat
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64) || StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PACKED))
		{
			u64 currValue = 0;
			if (statVerify(CStatsMgr::GetStatsDataMgr().GetStatData(keyHash, &currValue, sizeof(currValue))))
			{
				statAssertf(numberOfBits <= 32, "StatSetMaskedInt - Stat %s Invalid number of bits %d, max value is 32", StatsInterface::GetKeyName(keyHash), numberOfBits);
				statAssertf(shift <= 63, "StatSetMaskedInt - Stat %s Invalid offset %d, max value is 63", StatsInterface::GetKeyName(keyHash), shift);
				statAssertf(numberOfBits+shift <= 64, "StatSetMaskedInt - Stat %s Invalid offset %d and numberOfBits %d, max value of both combined is 64", StatsInterface::GetKeyName(keyHash), shift, numberOfBits);

				u64 mask = (1 << numberOfBits) - 1;
				statAssertf(data <= mask, "StatSetMaskedInt - Stat %s Invalid data %d, max value is %" I64FMT "d", StatsInterface::GetKeyName(keyHash), data, mask);

				u64 oldValue  = currValue;
				u64 dataInU64 = ((u64)data) << shift; //Data shifted to the position it should be
				u64 maskInU64 = (mask) << shift; //Shifted to the position were we want the bits changed

				oldValue = oldValue & ~maskInU64;     //Old value with 0 on the bits we want to change

				u64 newValue = oldValue | dataInU64;

				ret = StatsInterface::SetStatData(keyHash, &newValue, sizeof(newValue), flags);
			}
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT))
		{
			int currValue = 0;
			if (statVerify(CStatsMgr::GetStatsDataMgr().GetStatData(keyHash, &currValue, sizeof(currValue))))
			{
				statAssertf(numberOfBits <= 32, "StatSetMaskedInt - Stat %s Invalid number of bits %d, max value is 32", StatsInterface::GetKeyName(keyHash), numberOfBits);
				statAssertf(shift <= 31, "StatSetMaskedInt - Stat %s Invalid offset %d, max value is 31", StatsInterface::GetKeyName(keyHash), shift);
				statAssertf(numberOfBits+shift <= 32, "StatSetMaskedInt - Stat %s Invalid offset %d and numberOfBits %d, max value of both combined is 32", StatsInterface::GetKeyName(keyHash), shift, numberOfBits);

				u32 mask = (1 << numberOfBits) - 1;

				u32 oldValue  = (u32)currValue;
				u32 dataInInt = ((u32)data) << shift; //Data shifted to the position it should be
				u32 maskInInt = (mask) << shift; //Shifted to the position were we want the bits changed

				oldValue = oldValue & ~maskInInt; //Old value with 0 on the bits we want to change

				int newValue = (int)(oldValue | dataInInt);
				ret = StatsInterface::SetStatData(keyHash, &newValue, sizeof(newValue), flags);
			}
		}

		return ret;
	}

	bool GetMaskedInt(const int keyHash, int& data, int shift, int numberOfBits, int playerindex)
	{
		ValidateKey(keyHash, "GetMaskedInt");

		bool ret = false;

		statAssertf(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64) 
					|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PACKED) 
					|| StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT), "GetMaskedInt - Invalid Stat=\"%d\" type.", keyHash);

		// STAT_GET_MASKED_INT(statHash, statValue, 8, 8) would get the second byte in a stat
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64) || StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_PACKED))
		{
			const CNetGamePlayer* player = (NetworkInterface::IsGameInProgress() && -1 < playerindex) ? CTheScripts::FindNetworkPlayer(playerindex) : NULL;

			u64 statValue = 0;

			if (!player || (player && player->IsLocal()))
			{
				ret = CStatsMgr::GetStatsDataMgr().GetStatData(keyHash, &statValue, sizeof(statValue));
			}
			else if (statVerify(player && !player->IsLocal()))
			{
				ret = stats_commands::CommandGetRemotePlayerProfileStatValue(keyHash, player, &statValue, "STAT_GET_MASKED_INT");
			}

			if (statVerify(ret))
			{
				statAssertf(numberOfBits <= 32, "GetMaskedInt - Stat %s Invalid number of bits %d, max value is 32", StatsInterface::GetKeyName(keyHash), numberOfBits);
				statAssertf(shift <= 63, "GetMaskedInt - Stat %s Invalid offset %d, max value is 63", StatsInterface::GetKeyName(keyHash), shift);
				statAssertf(numberOfBits+shift <= 64, "GetMaskedInt - Stat %s Invalid offset %d and numberOfBits %d, max value of both combined is 64", StatsInterface::GetKeyName(keyHash), shift, numberOfBits);

				u64 mask        = (1 << numberOfBits) - 1;
				u64 maskShifted = (mask << shift);
				data = (int)((statValue & maskShifted) >> shift);
			}
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT))
		{
			const CNetGamePlayer* player = (NetworkInterface::IsGameInProgress() && -1 < playerindex) ? CTheScripts::FindNetworkPlayer(playerindex) : NULL;

			u32 statValue = 0;

			if (!player || (player && player->IsLocal()))
			{
				ret = CStatsMgr::GetStatsDataMgr().GetStatData(keyHash, &statValue, sizeof(int));
			}
			else if (statVerify(player && !player->IsLocal()))
			{
				ret = stats_commands::CommandGetRemotePlayerProfileStatValue(keyHash, player, &statValue, "STAT_GET_MASKED_INT");
			}

			if (statVerify(ret))
			{
				statAssertf(numberOfBits <= 32, "GetMaskedInt - Stat %s Invalid number of bits %d, max value is 32", StatsInterface::GetKeyName(keyHash), numberOfBits);
				statAssertf(shift <= 31, "GetMaskedInt - Stat %s Invalid offset %d, max value is 31", StatsInterface::GetKeyName(keyHash), shift);
				statAssertf(numberOfBits+shift <= 32, "GetMaskedInt - Stat %s Invalid offset %d and numberOfBits %d, max value of both combined is 32", StatsInterface::GetKeyName(keyHash), shift, numberOfBits);

				u32 mask        = (1 << numberOfBits) - 1;
				u32 maskShifted = (mask << shift);
				data = (int)((statValue & maskShifted) >> shift);
			}
		}

		return ret;
	}

	int  GetCurrentMultiplayerCharaterSlot()
	{
		if (!statVerify(NetworkInterface::IsLocalPlayerOnline()))
			return 0;

		statAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT), "Pending load of slot 0 multiplayer savegame.");
		statAssertf(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT),  "Load Failed of slot 0 multiplayer savegame.");

		if (!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
		{
			static CStatsDataMgr::StatsMap::Iterator s_statIter;

			if (!s_statIter.IsValid() || s_statIter.GetKey().GetHash() != STAT_MPPLY_LAST_MP_CHAR.GetHash())
			{
				if (!FindStatIterator(STAT_MPPLY_LAST_MP_CHAR.GetHash(), s_statIter  ASSERT_ONLY(, "MPPLY_LAST_MP_CHAR")))
					return 0;
			}
			statAssert(s_statIter.GetKey().GetHash() == STAT_MPPLY_LAST_MP_CHAR.GetHash());

			const sStatData* statData = *s_statIter;
			statAssert(statData);

			if (statData)
				return statData->GetInt();
		}

		return 0;
	}

	bool IsValidCharacterSlot(const int characterSlot)
	{
		return (characterSlot >= 0) && (characterSlot < MAX_NUM_MP_CHARS);
	}

	u64 GetBossGoonUUID(const int characterSlot)
	{
		const bool areStatsAvailable = !StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT);
		if(!statVerifyf(areStatsAvailable, "GetBossGoonUUID :: Stats are not loaded."))
			return 0;

		if(!statVerifyf(IsValidCharacterSlot(characterSlot), "GetBossGoonUUID :: Invalid characterSlot: %d", characterSlot))
			return 0;

		char statIdString[64];
		formatf(statIdString, "MP%d_BOSS_GOON_UUID", characterSlot);

		const StatId statId(statIdString);
		return StatsInterface::GetUInt64Stat(statId);
	}

	void SavePedScarData(CPed* ped)
	{
		if (!NetworkInterface::IsGameInProgress())
			return;

		if (!NetworkInterface::IsInFreeMode())
			return;

		if (!statVerify(ped))
			return;

		if (!ped->IsLocalPlayer())
			return;

		const int character = StatsInterface::GetCurrentMultiplayerCharaterSlot();
		Assert(character < MAX_NUM_MP_CHARS);

		const int perfix = GetPedModelPrefix(ped);
		if (-1==perfix || (character+CHAR_MP_START)!=perfix)
			return;

		const atArray<CPedScarNetworkData> *scarData = 0;
		PEDDAMAGEMANAGER.GetScarDataForLocalPed(ped, scarData);

		int numScars = 0;
		if(scarData && scarData->GetCount() > 0)
		{
			char statScarName[MAX_STAT_LABEL_SIZE];

			statDebugf3("Save Player Scar Data: ped=<%s>, character=<%d>", ped->GetModelName(), character);

			int scarsCount = scarData->GetCount()-1;
			statAssertf(scarsCount < MAX_NUM_DAMAGE_SCARS, "Number of scars %d is bigger that the allowed number in savegame %d", scarData->GetCount(), MAX_NUM_DAMAGE_SCARS);
			if (scarsCount >= MAX_NUM_DAMAGE_SCARS)
			{
				scarsCount = MAX_NUM_DAMAGE_SCARS-1;
			}

			for(int index=0; index <= scarsCount && numScars<MAX_NUM_DAMAGE_SCARS; index++)
			{
				const CPedScarNetworkData &scarDataFrom = (*scarData)[index];

				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_ZONE_", numScars);
				StatId stat = STAT_ID(statScarName);
				if (statVerifyf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName))
				{
					statDebugf3("..... Scar %d: zone=<%d>,scarNameHash=<%d>,scale=<%f>,rotation=<%f>,uvPos=<%f,%f>,forceFrame=<%d>,age=<%f>"
						,numScars
						,(int)scarDataFrom.zone
						,(int)scarDataFrom.scarNameHash
						,scarDataFrom.scale
						,scarDataFrom.rotation
						,scarDataFrom.uvPos.x, scarDataFrom.uvPos.y
						,scarDataFrom.forceFrame
						,scarDataFrom.age);

					int zonevalue = scarDataFrom.zone;
					StatsInterface::SetStatData(stat, &zonevalue, sizeof(int));

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_HASH_VALUE_", numScars);
					stat = STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					int hashvalue = (int)scarDataFrom.scarNameHash.GetHash();
					StatsInterface::SetStatData(stat, &hashvalue, sizeof(int));

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_SCALE_", numScars);
					stat = STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					float scalevalue = scarDataFrom.scale;
					StatsInterface::SetStatData(stat, &scalevalue, sizeof(float));

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_ROTATION_", numScars);
					stat = STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					float rotationvalue = scarDataFrom.rotation;
					StatsInterface::SetStatData(stat, &rotationvalue, sizeof(float));

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_UVPOS_", numScars);
					stat = STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					u64 posvalue = StatsInterface::PackPos(scarDataFrom.uvPos.x * SCAR_OFFSET_PACKING_MODIFIER, scarDataFrom.uvPos.y * SCAR_OFFSET_PACKING_MODIFIER, 0.0f);
					StatsInterface::SetStatData(stat, &posvalue, sizeof(u64));

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_FORCEFRAME_", numScars);
					stat = STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					int forceframevalue = scarDataFrom.forceFrame;
					StatsInterface::SetStatData(stat, &forceframevalue, sizeof(int));

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_AGE_", numScars);
					stat = STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					float agevalue = scarDataFrom.age;
					StatsInterface::SetStatData(stat, &agevalue, sizeof(float));
				}

				numScars++;
			}

			statAssertf(numScars<=MAX_NUM_DAMAGE_SCARS, "Max number of scars is %d, current is %d", MAX_NUM_DAMAGE_SCARS, numScars);
			StatsInterface::SetStatData(STAT_DAMAGE_SCAR_NUMBER.GetHash(character+CHAR_MP_START), numScars);
		}
	}

	void ClearPedScarData( )
	{
		statDebugf1(" ****** CLEARING ALL STATS FOR SCARS ****** ");

		char statScarName[MAX_STAT_LABEL_SIZE];

		for (int character=0;character<MAX_NUM_MP_CHARS;character++)
		{
			StatsInterface::SetStatData(STAT_DAMAGE_SCAR_NUMBER.GetHash(character+CHAR_MP_START), 0, STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);

			int   ivalue = 0;
			float fvalue = 0.0f;
			s64 i64value = 0;

			for (int numScars=0;numScars<MAX_NUM_DAMAGE_SCARS; numScars++)
			{
				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_ZONE_", numScars);
				StatId stat = STAT_ID(statScarName);
				statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
				StatsInterface::SetStatData(stat, &ivalue, sizeof(int), STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);

				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_HASH_VALUE_", numScars);
				stat = STAT_ID(statScarName);
				statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
				StatsInterface::SetStatData(stat, &ivalue, sizeof(int), STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);

				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_SCALE_", numScars);
				stat = STAT_ID(statScarName);
				statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
				StatsInterface::SetStatData(stat, &fvalue, sizeof(float), STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);

				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_ROTATION_", numScars);
				stat = STAT_ID(statScarName);
				statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
				StatsInterface::SetStatData(stat, &fvalue, sizeof(float), STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);

				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_UVPOS_", numScars);
				stat = STAT_ID(statScarName);
				statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
				StatsInterface::SetStatData(stat, &i64value, sizeof(u64), STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);

				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_FORCEFRAME_", numScars);
				stat = STAT_ID(statScarName);
				statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
				StatsInterface::SetStatData(stat, &ivalue, sizeof(int), STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);

				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_AGE_", numScars);
				stat = STAT_ID(statScarName);
				statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
				StatsInterface::SetStatData(stat, &fvalue, sizeof(float), STATUPDATEFLAG_LOAD_FROM_MP_SAVEGAME);
			}
		}
	}

	void ApplyPedScarData(CPed* ped, const u32 character)
	{
		//Flag that this is the first boot.
		static bool s_gameboot = true;
		if (s_gameboot)
		{
			s_gameboot = false;
			ClearPedScarData( );
			return;
		}

		if (!NetworkInterface::IsInFreeMode())
			return;

		if (!statVerify(ped))
			return;

		//Only apply scar data for the local player - because the stats data for other remote players isn't present.
		CPed* pLocalPlayer = CPedFactory::GetFactory() ? CPedFactory::GetFactory()->GetLocalPlayer() : NULL;
		bool bSameAsLocalPlayer = (ped == pLocalPlayer);
		if (!bSameAsLocalPlayer)
			return;

		if (statVerify(character < MAX_NUM_MP_CHARS))
		{
			const int numScars = StatsInterface::GetIntStat(STAT_DAMAGE_SCAR_NUMBER.GetHash(character+CHAR_MP_START));
			statAssertf(numScars<=MAX_NUM_DAMAGE_SCARS, "Max number of scars is %d, current is %d", MAX_NUM_DAMAGE_SCARS, numScars);

			statDebugf3("Apply Player Scar Data: ped=<%s>, numScars=<%d>, character=<%d>", ped->GetModelName(), numScars, character);

			//Clear all currently resident scars so that actual stats based scars are emplaced and take precedence, otherwise multiple calls to AddPedScar and ApplyPedScarData can emplace n-times the scars - so 1 scar can become 3 or 4... (lavalley)
			PEDDAMAGEMANAGER.ClearAllPlayerScars(ped);

			char statScarName[MAX_STAT_LABEL_SIZE];

			for(int index=0; index<numScars && index<MAX_NUM_DAMAGE_SCARS; index++)
			{
				formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_ZONE_", index);
				StatId stat = HASH_STAT_ID(statScarName);

				const bool isKeyValid = StatsInterface::IsKeyValid(stat);
				statAssertf(isKeyValid, "Stat %s is not valid", statScarName);

				if (isKeyValid)
				{
					const int zone = StatsInterface::GetIntStat(stat);

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_HASH_VALUE_", index);
					stat = HASH_STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					const int scarNameHash = StatsInterface::GetIntStat(stat);

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_SCALE_", index);
					stat = HASH_STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					const float scale = StatsInterface::GetFloatStat(stat);

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_ROTATION_", index);
					stat = HASH_STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					const float rotation = StatsInterface::GetFloatStat(stat);

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_UVPOS_", index);
					stat = HASH_STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					float x,y,z;
					StatsInterface::UnPackPos(StatsInterface::GetUInt64Stat(stat), x, y, z);
					x /= SCAR_OFFSET_PACKING_MODIFIER;
					y /= SCAR_OFFSET_PACKING_MODIFIER;
					Vector2 uvPos(x, y);

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_FORCEFRAME_", index);
					stat = HASH_STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					const int forceFrame = StatsInterface::GetIntStat(stat);

					formatf(statScarName, "%s%d_%s%d", "MP", character, "DAMAGE_SCAR_AGE_", index);
					stat = HASH_STAT_ID(statScarName);
					statAssertf(StatsInterface::IsKeyValid(stat), "Invalid ped scar stat=%s", statScarName);
					const float age = StatsInterface::GetFloatStat(stat);

					statDebugf3("..... Scar %d: zone=<%d>,uvPos=<%f,%f>,rotation=<%f>,scale=<%f>,scarNameHash=<%d>,forceFrame=<%d>,age=<%f>",index,zone,uvPos.x,uvPos.y,rotation,scale,scarNameHash,forceFrame,age);

					PEDDAMAGEMANAGER.AddPedScar(ped, (ePedDamageZones)zone, uvPos, rotation, scale, scarNameHash, true, forceFrame, false, age);
				}
			}
		}
	}

	//Setup the multiplayer savegame cloud service.
	rlCloudOnlineService   GetCloudSavegameService() 
	{
		statAssert(NetworkInterface::IsCloudAvailable());

		if (!NetworkInterface::HasValidRockstarId())
		{
			return RL_CLOUD_ONLINE_SERVICE_NATIVE;
		}

#if __BANK
		if(s_CloudSavegameService != RL_CLOUD_ONLINE_SERVICE_SC && PARAM_mpSavegameUseSc.Get())
		{
			s_CloudSavegameService = RL_CLOUD_ONLINE_SERVICE_SC;
		}
#endif // __BANK

		return s_CloudSavegameService;
	}

	void   SetCloudSavegameService(const rlCloudOnlineService service) 
	{
#if __BANK
		if (PARAM_mpSavegameUseSc.Get() && service == RL_CLOUD_ONLINE_SERVICE_NATIVE)
		{
			PARAM_mpSavegameUseSc.Set("0");
		}
#endif // __BANK

		s_CloudSavegameService = service;
	}

	bool   CanUseEncrytionInMpSave()
	{
//	AF: These save games always need to be encrypted
//		return (GetCloudSavegameService() == RL_CLOUD_ONLINE_SERVICE_NATIVE);
		return true;
	}

	void    UpdateLocalPlayerWeaponInfoStatId(const CWeaponInfo* wi, const char* vehicleName)
	{
		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			return;
		}
		if (!statVerify(wi))
			return;

		if (!statVerify(StatsInterface::GetStatsPlayerModelValid()))
			return;

		s_CurrWeaponHashUsed      = wi->GetHash();
		s_CurrWeaponStatIndexUsed = GetStatsModelPrefix();

		char statString[MAX_STAT_LABEL_SIZE];
		statString[0] = '\0';

		if (vehicleName)
		{
			statAssertf(vehicleName[0] != '\0', "Vehicle name is empty, vehicle stats will be broken.");

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "HELDTIME");
			s_WeaponHashIdHealdTime = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "DB_HELDTIME");
			s_WeaponHashIdDbHealdTime = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "SHOTS");
			s_WeaponHashIdShots = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "HITS");
			s_WeaponHashIdHits = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "HITS_CAR");
			s_WeaponHashIdHitsCar = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "ENEMY_KILLS");
			s_WeaponHashIdEnemyKills = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "KILLS");
			s_WeaponHashIdKills = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "DEATHS");
			s_WeaponHashIdDeaths = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicleName, wi->GetStatName(), "HEADSHOTS");
			s_WeaponHashIdHeadShots = StatId(statString);
		}
		else
		{
			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "HELDTIME");
			s_WeaponHashIdHealdTime = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "DB_HELDTIME");
			s_WeaponHashIdDbHealdTime = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "SHOTS");
			s_WeaponHashIdShots = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "HITS");
			s_WeaponHashIdHits = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "HITS_CAR");
			s_WeaponHashIdHitsCar = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "ENEMY_KILLS");
			s_WeaponHashIdEnemyKills = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "KILLS");
			s_WeaponHashIdKills = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "DEATHS");
			s_WeaponHashIdDeaths = StatId(statString);

			snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), "HEADSHOTS");
			s_WeaponHashIdHeadShots = StatId(statString);
		}
	}

	static StatId InvalidStatWeaponType("INVALID_STAT_WEAPON_TYPE");

	StatId  GetWeaponInfoHashId(const char* typeName, const CWeaponInfo* wi, const CPed* ped)
	{
		statAssert(typeName);
		statAssert(ped);
		statAssert(wi);

		if (typeName && wi && ped)
		{
			if(ped->GetIsInVehicle() && wi->GetIsVehicleWeapon())
			{
				const CVehicle* vehicle = ped->GetVehiclePedInside();

				if (statVerify(vehicle) && statVerify(vehicle->GetVehicleModelInfo()))
				{
					char statString[MAX_STAT_LABEL_SIZE];
					statString[0] = '\0';
					snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], vehicle->GetVehicleModelInfo()->GetGameName(), wi->GetStatName(), typeName);
					return StatId(statString);
				}
			}
			else
			{
				char statString[MAX_STAT_LABEL_SIZE];
				statString[0] = '\0';
				snprintf(statString, MAX_STAT_LABEL_SIZE, "%s_%s_%s", s_StatsModelPrefixes[GetStatsModelPrefix()], wi->GetStatName(), typeName);
				return StatId(statString);
			}
		}

		return InvalidStatWeaponType;
	}

	StatId  GetLocalPlayerWeaponInfoStatId(const eWeaponStatsTypes type, const CWeaponInfo* wi, const CPed* playerPed)
	{
		statAssert(wi);
		statAssert(playerPed && playerPed->IsLocalPlayer());

		if (!wi || !(playerPed && playerPed->IsLocalPlayer()))
			return InvalidStatWeaponType;

		if (wi->GetHash() != s_CurrWeaponHashUsed || s_CurrWeaponStatIndexUsed != GetStatsModelPrefix())
		{
			if(playerPed->GetIsInVehicle() && wi->GetIsVehicleWeapon())
			{
				const CVehicle* vehicle = playerPed->GetVehiclePedInside();

				if (statVerify(vehicle) && statVerify(vehicle->GetVehicleModelInfo()))
				{
#if __ASSERT
					const char* vehicleName = vehicle->GetVehicleModelInfo()->GetGameName();
					if (AssertVerify(vehicleName)) statAssertf(vehicleName[0] != '\0', "Vehicle name \"%s\" is empty for Vehicle Model Info \"%s:%d\"", vehicle->GetVehicleModelInfo()->GetVehicleMakeName(), vehicle->GetVehicleModelInfo()->GetModelName(), vehicle->GetVehicleModelInfo()->GetModelNameHash());
#endif //__ASSERT

					//B*3127632: If the weapon hash is a tank rocket, always attribute to RHINO (because script are doing weird things with changing vehicle models)
					if (wi->GetHash() == WEAPONTYPE_TANK)
					{
						UpdateLocalPlayerWeaponInfoStatId(wi, "RHINO");
					}
					else
					{
						UpdateLocalPlayerWeaponInfoStatId(wi, vehicle->GetVehicleModelInfo()->GetGameName());
					}
				}
			}
			else
			{
				//url:bugstar:6454734 - the periscope sub missile can be fired from the driving seat or a standalone periscope, so the player isn't a driver.
				if (wi->GetHash() == WEAPONTYPE_VEHICLE_WEAPON_SUB_MISSILE_HOMING)
				{
					UpdateLocalPlayerWeaponInfoStatId(wi, "KOSATKA");
				}
				else
				{
					UpdateLocalPlayerWeaponInfoStatId(wi, NULL);
				}
			}
		}

		switch (type)
		{
		case WEAPON_STAT_HELDTIME:     return s_WeaponHashIdHealdTime;
		case WEAPON_STAT_DB_HELDTIME:  return s_WeaponHashIdDbHealdTime;
		case WEAPON_STAT_SHOTS:        return s_WeaponHashIdShots;
		case WEAPON_STAT_HITS:         return s_WeaponHashIdHits;
		case WEAPON_STAT_HITS_CAR:     return s_WeaponHashIdHitsCar;
		case WEAPON_STAT_ENEMY_KILLS:  return s_WeaponHashIdEnemyKills;
		case WEAPON_STAT_KILLS:        return s_WeaponHashIdKills;
		case WEAPON_STAT_DEATHS:       return s_WeaponHashIdDeaths;
		case WEAPON_STAT_HEADSHOTS:    return s_WeaponHashIdHeadShots;
		}

		statAssertf(0, "Invalid weapon stat type=\"%d\"", type);

		return InvalidStatWeaponType;
	}

	void UpdateProfileSetting(const int id)
	{
		char statString[MAX_STAT_LABEL_SIZE];
		statString[0] = '\0';
		snprintf(statString, MAX_STAT_LABEL_SIZE, "_PROFILE_SETTING_%d", id);

		StatId stat(statString);
		if (StatsInterface::IsKeyValid(stat))
		{
			int fakevalue = 0;
			StatsInterface::SetStatData(stat, fakevalue);
		}
	}

	u32 GetVehicleLeaderboardWriteInterval( )
	{
		static const int SP_LB_WRITE_INTERVAL = 60*60*1000;
		static const int MP_LB_WRITE_INTERVAL = 30*60*1000;

		if (NetworkInterface::IsGameInProgress())
			return Tunables::GetInstance().TryAccess(MP_GLOBAL_HASH, ATSTRINGHASH("MP_VEHICLE_LB_WRITE_INTERVAL", 0xc0ef7953), SP_LB_WRITE_INTERVAL);
		else
			return Tunables::GetInstance().TryAccess(BASE_GLOBALS_HASH, ATSTRINGHASH("SP_VEHICLE_LB_WRITE_INTERVAL", 0xa246e188), MP_LB_WRITE_INTERVAL);
	}

	bool  GetVehicleLeaderboardWriteOnSavegame()
	{
		bool write = false;

		if (NetworkInterface::IsGameInProgress())
			Tunables::GetInstance().Access(MP_GLOBAL_HASH, ATSTRINGHASH("MP_VEHICLE_LB_WRITE_ON_SAVE", 0xb52bc385), write);
		else
			Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("SP_VEHICLE_LB_WRITE_ON_SAVE", 0x207af5a2), write);

		return write;
	}

	bool  GetVehicleLeaderboardKillWrite()
	{
		return true;
		//bool killWrite = false;

		//if (NetworkInterface::IsGameInProgress())
		//	Tunables::GetInstance().Access(MP_GLOBAL_HASH, ATSTRINGHASH("MP_VEHICLE_LB_NO_WRITE", 0x6d3007c5), killWrite);
		//else
		//	Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("SP_VEHICLE_LB_NO_WRITE", 0x5e6e8974), killWrite);

		//return killWrite;
	}

	void  TryVehicleLeaderboardWriteOnMatchEnd( )
	{
		bool write = false;

		if (NetworkInterface::IsGameInProgress())
			Tunables::GetInstance().Access(MP_GLOBAL_HASH, ATSTRINGHASH("MP_VEHICLE_LB_WRITE_ON_MATCHLBWRITE", 0xba0649d3), write);

		if (write)
			CStatsMgr::CheckWriteVehicleRecords( true );
	}

	bool  TryVehicleLeaderboardWriteOnBoot( )
	{
		bool done = false;

		if (NetworkInterface::IsLocalPlayerOnline() && NetworkInterface::IsCloudAvailable() && NetworkInterface::IsOnlineRos() && NetworkInterface::HasValidRosCredentials())
		{
			done = true;

			bool write = false;
			Tunables::GetInstance().Access(BASE_GLOBALS_HASH, ATSTRINGHASH("SP_VEHICLE_LB_WRITE_ON_BOOT", 0x133bb31d), write);
			if (write)
				CStatsMgr::CheckWriteVehicleRecords( true );
		}

		return done;
	}

	void CheckForProfileAwardStat(sStatDataPtr* ppStat)
	{
		statAssert(ppStat && ppStat->GetKey().IsValid());

		StatId statId;
		// Check for cash given to the player from scAdmin
		if( ppStat->GetKey() == HASH_STAT_ID("SCADMIN_SP0_CASH") ||
			ppStat->GetKey() == HASH_STAT_ID("SCADMIN_SP1_CASH") ||
			ppStat->GetKey() == HASH_STAT_ID("SCADMIN_SP2_CASH") )
		{
			statId = ppStat->GetKey();
		}
		if(statId.IsValid())
		{
			u32 cashToAdd = StatsInterface::GetIntStat(statId);
			if(cashToAdd>0)
			{
				statDebugf1("Adding money sent from the server to the player (%s) = %d $", ppStat->GetKeyName(), cashToAdd);

				int character=-1;
				if(statId == HASH_STAT_ID("SCADMIN_SP0_CASH"))  character = CHAR_MICHEAL;
				if(statId == HASH_STAT_ID("SCADMIN_SP1_CASH"))  character = CHAR_FRANKLIN;
				if(statId == HASH_STAT_ID("SCADMIN_SP2_CASH"))  character = CHAR_TREVOR;

				if(statVerify(character!=-1))
				{
					// Send an event to Script so they can add the cash and display a message
					sNetworkScAdminReceivedCash eventData;
					eventData.m_characterIndex.Int = character;
					eventData.m_cashAmount.Int  = cashToAdd;
					GetEventScriptNetworkGroup()->Add(CEventNetworkScAdminReceivedCash(&eventData));
					SetStatData(statId, 0);   // Set the value to 0
				}
			}
		}
	}

	bool HasLocalPlayerPurchasedCESP()
	{
#if RSG_BANK
		if (PARAM_forceCESP.Get())
		{
			return true;
		}
		else if (PARAM_forceNoCESP.Get())
		{
			return false;
		}
		else
#endif // RSG_BANK
		{
			/*const*/ CStatsDataMgr& dataMgr = CStatsMgr::GetStatsDataMgr();
			if (dataMgr.GetAllOnlineStatsAreSynched())
			{
				// Retrieve player CESP purchase status
				return StatsInterface::GetBooleanStat(STAT_MPPLY_SCADMIN_CESP);
			}
#if IS_GEN9_PLATFORM
			else
			{
				return NETWORK_LANDING_PAGE_STATS_MGR.HasCesp();
			}
#else
			return false;
#endif // IS_GEN9_PLATFORM
		}
	}
}

// EOF
