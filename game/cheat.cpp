//
// Title	:	Cheat.cpp
// Author	:	John_Gurney
// Started	:	25/8/04
//

// File header
#include "cheat.h"

// Rage Headers
#include "physics/Archetype.h"
#include "phcore/phMath.h"
#include "phbound/BoundComposite.h"
#include "rmcore/Drawable.h"
#include "system/bootmgr.h"
#include "string/unicode.h"
#include "diag/channel.h"

// Framework Headers
#include "fwmaths/Angle.h"

// Game Headers

#include "animation/AnimDefines.h"
#include "scene/portals/portal.h"
#include "control/gamelogic.h"
#include "control/replay/replayinternal.h"
#include "core/game.h"
#include "event/EventNetwork.h"
#include "game/Clock.h"
#include "game/ModelIndices.h"
#include "game/Weather.h"
#include "game/localisation.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Network/Live/NetworkTelemetry.h"
#include "network/NetworkInterface.h"
#include "physics/Breakable.h"
#include "objects/Object.h"
#include "peds/ped_channel.h"
#include "peds/PedFactory.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "physics/GtaArchetype.h"
#include "physics/GtaInst.h"
#include "physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/scene.h"
#include "scene/world/gameWorld.h"
#include "Stats/StatsInterface.h"
#include "streaming/streaming.h"
#include "system/ControlMgr.h"
#include "system/FileMgr.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Text/TextFile.h"
#include "vehicles/Automobile.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/bike.h"
#include "text/messages.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "weapons/weapon.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/PauseMenu.h"
#include "control/replay/replay.h"

RAGE_DEFINE_CHANNEL(cheat)
#define cheatDisplayf(fmt,...) RAGE_DISPLAYF(cheat,fmt,##__VA_ARGS__)


bool	CCheat::m_aCheatsActive[NUM_CHEATS];
u32	CCheat::m_LastTimeCheatUsed[NUM_CHEATS];
bool 	CCheat::m_bHasPlayerCheated = false;
char	CCheat::m_CheatString[CHEATMEM];
u32		CCheat::sm_aHashesOfPreviousKeyPresses[CHEATMEM];

u32	CCheat::m_MoneyCheated;

#if RSG_PC
bool CCheat::m_bCheckPcKeyboard = false;
u32  CCheat::m_uPcCheatCodeHash = 0u;
#endif // RSG_PC

//u32	CCheat::m_SlownessEffect = 0;

#if !__FINAL
bool	CCheat::m_bAllowDevelopmentCheats;
bool	CCheat::m_bInNetworkGame;
#endif

#define MIN_CHEAT_LENGTH	6

// the functions which are called for each cheat
CheatFunction CCheat::m_aCheatFunctions[NUM_CHEATS] =
{
	&PoorWeaponCheat,
	&AdvancedWeaponCheat,
	&HealthCheat,
	&WantedLevelUpCheat,
	&WantedLevelDownCheat,
	&MoneyUpCheat,
	&ChangeWeatherCheat,
	&AnnihilatorCheat,
	&Nrg900Cheat,
	&FbiCheat,
	&JetmaxCheat,
	&CometCheat,
	&TurismoCheat,
	&CognoscentiCheat,
	&SuperGTCheat,
	&SanchezCheat,
//	&SlownessEffectCheat,
};

// Y Yellow
// X Blue
// A Green
// B Red
// U up
// D down
// L left
// R right
// 1 left shoulder 1
// 2 left shoulder 2
// 3 right shoulder 1
// 4 right shoulder 2

PARAM(nocheats, "Turn off cheats like H for Health, V for Invincibility");

void CCheat::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
        m_MoneyCheated = 0;
		/* Done at compile time now.
		for(s32 i = 0; i < NUM_CHEATS; ++i)
		{
			m_aCheatHashKeys[i] = atStringHash(m_aCheatStrings[i]);
			sm_aCheatStringLengths[i] = ustrlen(m_aCheatStrings[i]);
		}
		*/
    }
    else if(initMode == INIT_SESSION)
    {
	    for (s32 i = 0; i < NUM_CHEATS; i++)
	    {
		    m_LastTimeCheatUsed[i] = 0;
	    }

#if !__FINAL
	    m_bAllowDevelopmentCheats = true;
		if (PARAM_nocheats.Get())
		{
			m_bAllowDevelopmentCheats = false;
		}

	    m_bInNetworkGame = false;
#endif
    }
}

bool CCheat::HasCheatActivated(u32 hashOfCheatString, u32 lengthOfCheatString)
{
	if (Verifyf((lengthOfCheatString >= MIN_CHEAT_LENGTH) && (lengthOfCheatString < CHEATMEM), "CCheat::HasCheatActivated - lengthOfCheatString is %u. It should be >= %u and < %u", lengthOfCheatString, MIN_CHEAT_LENGTH, CHEATMEM))
	{
		if(hashOfCheatString == sm_aHashesOfPreviousKeyPresses[lengthOfCheatString])
		{
			ClearCheatString();
			return true;
		}
	}

	return false;
}

bool CCheat::HasCheatActivated(const char* cheatCode)
{
	if (cheatCode)
	{
		u32 cheatHash = atStringHash(cheatCode);
		u32 stringLength = ustrlen(cheatCode);

		return HasCheatActivated(cheatHash, stringLength);
	}

	return false;
}


#if RSG_PC
bool CCheat::HasPcCheatActivated(u32 hashOfCheatString)
{
	if(m_uPcCheatCodeHash == hashOfCheatString)
	{
		m_uPcCheatCodeHash = 0u;
		return true;
	}

	return false;
}
#endif // RSG_PC


void CCheat::ClearArrayOfHashesOfPreviousKeyPresses()
{
	for (u32 loop = 0; loop < CHEATMEM; loop++)
	{
		sm_aHashesOfPreviousKeyPresses[loop] = 0;
	}
}

void CCheat::ClearCheatString()
{
	for (u32 loop = 0; loop < CHEATMEM; loop++)
	{
		m_CheatString[loop] = 0;
	}
	ClearArrayOfHashesOfPreviousKeyPresses();
}

void CCheat::FillArrayOfHashesOfPreviousKeyPresses()
{
	ClearArrayOfHashesOfPreviousKeyPresses();

	const u32 maximumNumberOfCharacters = (CHEATMEM-1);

//	New button presses are added to the end of m_CheatString so count the number of valid characters after the initial zeros
	u32 lengthOfCheatString = maximumNumberOfCharacters;
	u32 j = 0;
	while ( (j < maximumNumberOfCharacters) && (m_CheatString[j] == 0) )
	{
		if (Verifyf(lengthOfCheatString > 0, "CCheat::FillArrayOfHashesOfPreviousKeyPresses - lengthOfCheatString is 0. Graeme Williamson has made a mistake"))
		{
			lengthOfCheatString--;
		}
		j++;
	}


	if (lengthOfCheatString >= MIN_CHEAT_LENGTH)
	{
		u32 startingIndex = 0;
		if (Verifyf(lengthOfCheatString <= maximumNumberOfCharacters, "CCheat::FillArrayOfHashesOfPreviousKeyPresses - lengthOfCheatString is %u. It's larger than %u. Graeme Williamson has made a mistake.", lengthOfCheatString, maximumNumberOfCharacters))
		{
			startingIndex = maximumNumberOfCharacters - lengthOfCheatString;
		}
		const char* testStr = &m_CheatString[startingIndex];

		for (u32 i = lengthOfCheatString; i >= MIN_CHEAT_LENGTH; --i)
		{
			sm_aHashesOfPreviousKeyPresses[i] = atStringHash(testStr);
			++testStr;
		}
	}
}

// Purpose		:	Activates cheats
// Parameters	:	None
// Returns		:	Nothing
void CCheat::DoCheats()
{

    // cheats re-enabled in network game by default for the time being (request by Les)
//#if !__FINAL
//	if (!m_bInNetworkGame)
//	{
//		if (NetworkInterface::IsGameInProgress())
//		{
//#if !__DEV
//			m_bAllowDevelopmentCheats = false;
//#endif
//			m_bInNetworkGame = true;
//		}
//	}
//#endif

	if (!NetworkInterface::IsGameInProgress() REPLAY_ONLY(&& !CReplayMgr::IsReplayInControlOfWorld()))
	{
#if RSG_PC
		CControl& playerControl = CControlMgr::GetMainPlayerControl();
		if(m_bCheckPcKeyboard == false && playerControl.WasKeyboardMouseLastKnownSource() && playerControl.GetEnterCheatCode().IsPressed())
		{
			USES_CONVERSION;

			ioVirtualKeyboard::Params params;

			params.m_InitialValue = 0;
			params.m_KeyboardType = ioVirtualKeyboard::kTextType_DEFAULT;
			params.m_Description = UTF8_TO_WIDE(TheText.Get("PC_CHEAT_DESCRIPTION"));
			params.m_Title = UTF8_TO_WIDE(TheText.Get("PC_CHEAT_TITLE"));
			params.m_MaxLength = CHEATMEM;
			params.m_PlayerIndex = 0;

			CControlMgr::ShowVirtualKeyboard(params);

			m_bCheckPcKeyboard = true;
		}
		else if(m_bCheckPcKeyboard)
		{
			ioVirtualKeyboard::eKBState state = CControlMgr::UpdateVirtualKeyboard();
			if(state == ioVirtualKeyboard::kKBState_SUCCESS)
			{
				m_uPcCheatCodeHash = atStringHash(CControlMgr::GetVirtualKeyboardResult());
				m_bCheckPcKeyboard = false;
			}
			else if(state != ioVirtualKeyboard::kKBState_PENDING)
			{
				m_bCheckPcKeyboard = false;
			}
		}
#endif // RSG_PC


	// This is where to disable the cheats.
		ioValue::ReadOptions options = ioValue::NO_DEAD_ZONE;
		options.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);

		if (CControlMgr::GetMainFrontendControl().GetFrontendY().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('Y');
		if (CControlMgr::GetMainFrontendControl().GetFrontendX().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('X');

		// If PS3's X/O are swapped then FrontendAccept will be on 'O' and cancel will be on 'X'.
#if __PPU || RSG_ORBIS
		if(CPauseMenu::GetMenuPreference(PREF_ACCEPT_IS_CROSS) == false)
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendCancel().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('A');
			if (CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('B');
		}
		else
#endif // __PPU
		{
			if (CControlMgr::GetMainFrontendControl().GetFrontendAccept().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('A');
			if (CControlMgr::GetMainFrontendControl().GetFrontendCancel().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('B');
		}
		
		if (CControlMgr::GetMainFrontendControl().GetFrontendUp().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('U');
		if (CControlMgr::GetMainFrontendControl().GetFrontendDown().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('D');
		if (CControlMgr::GetMainFrontendControl().GetFrontendLeft().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('L');
		if (CControlMgr::GetMainFrontendControl().GetFrontendRight().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('R');

		if (CControlMgr::GetMainFrontendControl().GetFrontendLB().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('1');
		if (CControlMgr::GetMainFrontendControl().GetFrontendLT().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('2');
		if (CControlMgr::GetMainFrontendControl().GetFrontendRB().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('3');
		if (CControlMgr::GetMainFrontendControl().GetFrontendRT().IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options)) AddToCheatString('4');
		
		FillArrayOfHashesOfPreviousKeyPresses();
	}

	// Stick a few cheats on the keyboard to help when debugging
#if !__FINAL
	if (m_bAllowDevelopmentCheats)
	{
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_H, KEYBOARD_MODE_DEBUG, "health cheat"))
		{
			HealthCheat();
			cheatDisplayf("H - Health restored!");
		}
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_DEBUG, "weapon cheat") 
			REPLAY_ONLY( || CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_REPLAY, "weapon cheat")))
		{
			WeaponCheat1();
			cheatDisplayf("W - Weapons!");
		}
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_V, KEYBOARD_MODE_DEBUG, "invincibility cheat"))
		{
			CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth = false;
			CPlayerInfo::ms_bDebugPlayerInvincible = !CPlayerInfo::ms_bDebugPlayerInvincible;
			if (CPlayerInfo::ms_bDebugPlayerInvincible)
			{
				char *pString = TheText.Get("HELP_INVIN");
				CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
				cheatDisplayf("V - Invincible activated!");
			}
			else
			{
				char *pString = TheText.Get("HELP_NOINVIN");
				CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
				cheatDisplayf("V - Invincible deactivated!");
			}

			if (NetworkInterface::IsGameInProgress())
				GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_INVINCIBLE));
		}
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_V, KEYBOARD_MODE_DEBUG_ALT, "invincibility cheat restore health"))
		{
			CPlayerInfo::ms_bDebugPlayerInvincible = false;
			CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth = !CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth;
			if (CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth)
			{
				char *pString = TheText.Get("HELP_INVIN_R");
				CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
				cheatDisplayf("Alt-V - Invincible + restore health activated!");
			}
			else
			{
				char *pString = TheText.Get("HELP_NOINVIN_R");
				CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
				cheatDisplayf("Alt-V - Invincible + restore health deactivated!");
			}

			if (NetworkInterface::IsGameInProgress())
				GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_INVINCIBLE));
		}
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_V, KEYBOARD_MODE_DEBUG_SHIFT_ALT, "invincibility cheat restore armor with health"))
		{
			CPlayerInfo::ms_bDebugPlayerInvincibleRestoreArmorWithHealth = !CPlayerInfo::ms_bDebugPlayerInvincibleRestoreArmorWithHealth;
			
			char *pString = CPlayerInfo::ms_bDebugPlayerInvincibleRestoreArmorWithHealth ? TheText.Get("HELP_INV_A_ON") : TheText.Get("HELP_INV_A_OFF");
			CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
			cheatDisplayf("Shift-Alt-V - Invincible + Restore Health switched modes!");
		}
		if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_W, KEYBOARD_MODE_DEBUG_CTRL, "Infinite Ammo"))
		{
			
			static int AmmoLimit = 0;
			AmmoLimit = (AmmoLimit + 1) % (2 + __BANK);
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if (pPlayer)
			{
				if (pPlayer && pPlayer->GetInventory())
				{
					const bool shouldUseInfiniteAmmo = (AmmoLimit == 1);
					pPlayer->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(shouldUseInfiniteAmmo);
#if __BANK
					const bool shouldUseInfiniteClips = (AmmoLimit == 2);
					pPlayer->GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(shouldUseInfiniteClips);
#endif
				}

				//Display confirmation debug text (if the HUD is enabled.)
				static const char* s_Strings[3] = {"HELP_AMMO", "HELP_AMMO_UNL", "HELP_AMMO_UNLCL"};
				char *pString = TheText.Get(s_Strings[AmmoLimit]);
				CGameStreamMgr::GetGameStream()->PostTicker( pString, false );

				cheatDisplayf("Ctrl-W - Activated: %s", TheText.Get(s_Strings[AmmoLimit]) );
			}
		}
#if __BANK && 0	// Disable this since script uses S for skipping missions so can accidently force a slow zone
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG, "slowness cheat"))
		{
			CVehicle::ms_bForceSlowZone = !CVehicle::ms_bForceSlowZone;
		}
#endif
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F3, KEYBOARD_MODE_DEBUG, "decrease wanted level"))
		{
			CCheat::WantedLevelDownCheat();
		}
		if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F4, KEYBOARD_MODE_DEBUG, "increase wanted level"))
		{
			CCheat::WantedLevelUpCheat();
		}
	}
#endif //!__FINAL

}

// Name			:	ResetCheats
// Purpose		:	Resets cheats 
// Parameters	:	None
// Returns		:	Nothing

void CCheat::ResetCheats()
{
	for (s32 i=0; i<NUM_CHEATS; i++)
		m_aCheatsActive[i] = false;
		
	ClearCheatString();
		
	m_bHasPlayerCheated = false;

}

// Name			:	AddToCheatString
// Purpose		:	Adds an extra char to the cheat string and check whether any cheats should be activated
// Parameters	:	None
// Returns		:	Nothing

void CCheat::AddToCheatString(char NewChar)
{
	for (u32 i = 0; i < CHEATMEM-2; i++)
	{
		m_CheatString[i] = m_CheatString[i+1];
		//m_CheatString[i+1] = m_CheatString[i];
	}
	
	m_CheatString[CHEATMEM-2] = NewChar;
	m_CheatString[CHEATMEM-1] = 0;	// Make sure the string is terminated properly.
}

// Name			:	GenerateCheatHashKeys
// Purpose		:	
// Parameters	:	None
// Returns		:	Nothing

#if __DEV && 0
void CCheat::GenerateCheatHashKeys()
{
	if(sysBootManager::IsBootedFromDisc())
		return;

	FileHandle fileId = CFileMgr::OpenFileForWriting("CheatHashKeys.dat");
	Assertf( CFileMgr::IsValidFileHandle(fileId), "Could not open CheatHashKeys.dat" );

	s32 i,j,len;
	u32 hashKey;
	char str[CHEATMEM+1];
	
	for (i=0; i<NUM_CHEATS; i++)
	{
		len = istrlen(m_aCheatStrings[i]);
		
		if (len == 0)
			hashKey = 0; // these are script cheats that are not activated using the hashkeys
		else
		{ 
			Assert(len >= MIN_CHEAT_LENGTH);
		
			// reverse the string
			for (j=0; j<len; j++)
				str[len-1-j] = m_aCheatStrings[i][j];
			
			str[len] = 0;
		
			hashKey = atStringHash(str);
		}
		
		sprintf(str, "%uU,\r\n", hashKey);
		
		CFileMgr::Write(fileId, str, istrlen(str));				
	}

	CFileMgr::CloseFile(fileId);
}
#endif // __DEV && 0

// Name			:	DoDebugCheats
// Purpose		:	
// Parameters	:	None
// Returns		:	Nothing

bool CCheat::CheatHappenedRecently(s32 cheatId, u32 time)
{
	Assert(cheatId >= 0 && cheatId < NUM_CHEATS);
	if (m_LastTimeCheatUsed[cheatId] == 0 ||
		fwTimer::GetTimeInMilliseconds() > m_LastTimeCheatUsed[cheatId] + time)
	{
		return false;
	}
	return true;
}

//#pragma mark -------------------------- GAMEPLAY CHEATS --------------------------

NOTFINAL_ONLY(static int WEAPON_CHEAT_LOAD_OUT_INDEX = 0);

bool SetWeaponCheatLoadOut()
{
#if !__FINAL
	// The root hash of the cheat load outs
	static u32 uPartialCheatLoadOutHash = atPartialStringHash("LOADOUT_CHEAT_");

	// Create a hash key with the current cheat index
	char indexBuff[2];
	sprintf(indexBuff, "%d", WEAPON_CHEAT_LOAD_OUT_INDEX);
	u32 uCheatLoadOutHash = atStringHash(indexBuff, uPartialCheatLoadOutHash);

	// Create a hash key with the current level name
	char levelBuff[64];
	sprintf(levelBuff, "%d_%s", WEAPON_CHEAT_LOAD_OUT_INDEX, CScene::GetLevelsData().GetFriendlyName(CGameLogic::GetCurrentLevelIndex()));
	u32 uCheatLoadOutLevelHash = atStringHash(levelBuff, uPartialCheatLoadOutHash);

	CPed * pPlayerPed = FindPlayerPed();

	// Don't allow weapon cheat whilst parachuting, since it'll remove all the gadgets
	// I don't want to remove all gadgets except the parachute since it'll mean more code changes
	// for a simple debug assert. CC
	if (pPlayerPed)
	{
		if (pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE))
		{
			return false;
		}
	}

	if(pPlayerPed && ((CPedInventoryLoadOutManager::HasLoadOut(uCheatLoadOutLevelHash) && CPedInventoryLoadOutManager::SetLoadOut(pPlayerPed, uCheatLoadOutLevelHash)) || (CPedInventoryLoadOutManager::HasLoadOut(uCheatLoadOutHash) && CPedInventoryLoadOutManager::SetLoadOut(pPlayerPed, uCheatLoadOutHash))))
	{
		// Increment the index, so we use the next load out next time
		WEAPON_CHEAT_LOAD_OUT_INDEX = Clamp(WEAPON_CHEAT_LOAD_OUT_INDEX + 1, 0, 99);

		// Success
		return true;
	}
	else
	{
		// No load out for index
		return false;
	}
#else // !__FINAL
	// Not built in __FINAL builds
	return false;
#endif // !__FINAL
}

void CCheat::WeaponCheat1()
{
	IncrementTimesCheatedStat();

	if(!SetWeaponCheatLoadOut())
	{
		// Reset the index
		NOTFINAL_ONLY(WEAPON_CHEAT_LOAD_OUT_INDEX = 0);

		// Attempt to set the 0 load out, as the last attempt failed
		SetWeaponCheatLoadOut();
	}

	CNetworkTelemetry::CheatApplied("W");

	if (NetworkInterface::IsGameInProgress())
	{
		GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_WEAPONS));

// 		CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();
// 
// 		// Always give the player a loudhailer if they are a cop. Les request.
// 		if (pLocalPlayer && pLocalPlayer->GetPlayerPed() && pLocalPlayer->GetTeam() == eTEAM_COP)
// 			pLocalPlayer->GetPlayerPed()->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_LOUDHAILER, 0);
	}
}

void CCheat::PoorWeaponCheat()
{
	WeaponCheat1();
};
 

void CCheat::AdvancedWeaponCheat()
{
	WeaponCheat1();
};

#if !__FINAL

void CCheat::MoneyArmourHealthCheat()
{
	cheatDisplayf("Money/Health/Armour Cheat!");
	IncrementTimesCheatedStat();

	if(NetworkInterface::IsInFreeMode())
	{
		//Done script side?
	}
	else
	{
		StatsInterface::IncrementStat(STAT_TOTAL_CASH.GetStatId(), 250000);
	}


	pedAssertf(FindPlayerPed(), "No PlayerPed!");
	FindPlayerPed()->SetArmour( FindPlayerPed()->GetPlayerInfo()->MaxArmour );
	HealthCheat();
	m_MoneyCheated += 250000;
}
#endif

void CCheat::HealthCheat()
{
	// Disable the health check when injured, it doens't correctly revive the player
	if( FindPlayerPed() == NULL || FindPlayerPed()->IsInjured() )
	{
		return;
	}
	cheatDisplayf("HEALTH UP");
	
	IncrementTimesCheatedStat();

	FindPlayerPed()->SetHealth( FindPlayerPed()->GetPlayerInfo()->MaxHealth );
	FindPlayerPed()->SetArmour( FindPlayerPed()->GetPlayerInfo()->MaxArmour );
	FindPlayerPed()->ClearDamageAndScars();

	// Fix car
	if (FindPlayerVehicle())
	{
		if(!FindPlayerVehicle()->IsNetworkClone())
		{
			if(FindPlayerVehicle()->GetStatus() != STATUS_WRECKED)
			{	
				FindPlayerVehicle()->SetHealth(1000.0f);
				FindPlayerVehicle()->Fix();
			}
		}
	}
	CNetworkTelemetry::CheatApplied("H");

	m_LastTimeCheatUsed[HEALTH_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(HEALTH_CHEAT);

	if (NetworkInterface::IsGameInProgress())
		GetEventScriptNetworkGroup()->Add(CEventNetworkCheatTriggered(CEventNetworkCheatTriggered::CHEAT_HEALTH));
};


void CCheat::WantedLevelUpCheat()
{
	cheatDisplayf("F4 - WANTED_LEVEL_UP_CHEAT");

	IncrementTimesCheatedStat();

	CWanted * pPlayerWanted = FindPlayerPed()->GetPlayerWanted();
	pPlayerWanted->CheatWantedLevel(MIN(pPlayerWanted->GetWantedLevel() + 1, WANTED_LEVEL_LAST - 1));

	m_LastTimeCheatUsed[WANTED_LEVEL_UP_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(WANTED_LEVEL_UP_CHEAT);
}

void CCheat::WantedLevelDownCheat()
{
	cheatDisplayf("F3 - WANTED_LEVEL_DOWN_CHEAT");

	m_LastTimeCheatUsed[WANTED_LEVEL_DOWN_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(WANTED_LEVEL_DOWN_CHEAT);

	IncrementTimesCheatedStat();

	CWanted * pPlayerWanted = FindPlayerPed()->GetPlayerWanted();
	pPlayerWanted->CheatWantedLevel( MAX(pPlayerWanted->GetWantedLevel() - 1, WANTED_CLEAN) );
}

void CCheat::MoneyUpCheat()
{
/* Obbe: removed. This cheat caused too many problems
	IncrementTimesCheatedStat();

	FindPlayerPed()->GetPlayerInfo()->Score += 5000;
	m_MoneyCheated += 5000;

	m_LastTimeCheatUsed[MONEY_UP_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(MONEY_UP_CHEAT);
*/
}


void CCheat::ChangeWeatherCheat()
{
	cheatDisplayf("Weather Changed!");
	IncrementTimesCheatedStat();

	g_weather.IncrementType();

	m_LastTimeCheatUsed[CHANGE_WEATHER_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(CHANGE_WEATHER_CHEAT);
}


CVehicle* CCheat::CreateVehicleAdminIntf( const char* vehicleName)
{
	return VehicleCheat(vehicleName, false);
}



//
// name:		VehicleCheat
// description:	Generic vehicle cheat to be used for all vehicle generation cheats
CVehicle* CCheat::VehicleCheat(const char* pName, bool countAsCheat)
{
	fwModelId modelId = CModelInfo::GetModelIdFromName(pName);
	if(!modelId.IsValid())
		return NULL;

	cheatDisplayf("Vehicle cheated %s", pName);
	if(countAsCheat)
		IncrementTimesCheatedStat();

	CVehicle *pNewVehicle = NULL;

	// Remove the previous vehicles that were created by cheat.
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVeh;
	s32 i = (s32) VehiclePool->GetSize();
	while(i--)
	{
		pVeh = VehiclePool->GetSlot(i);
		if(pVeh && pVeh->m_nVehicleFlags.bCreatedUsingCheat && pVeh->PopTypeIsRandomNonPermanent() &&
			pVeh->GetSeatManager()->CountPedsInSeats() == 0)
		{
			CGameWorld::Remove(pVeh);
			CVehicleFactory::GetFactory()->Destroy(pVeh);
		}
	}




	if (CVehicle::GetPool()->GetNoOfFreeSpaces() < 5)
		return NULL;
		
	// Stream vehicle
	
	u32 flags = CModelInfo::GetAssetStreamFlags(modelId);
	CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE);
	CStreaming::LoadAllRequestedObjects();
	
	// when vehicle has loaded then create on nearest car node
	if(CModelInfo::HaveAssetsLoaded(modelId))
	{
		// if previously vehicle was deletable, then set it to be deletable
		if(!(flags & STRFLAG_DONTDELETE))
		{
			CModelInfo::SetAssetsAreDeletable(modelId);
		}

		const Vector3 vecPlayerRight(VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetA()));
		const Vector3 vecPlayerForward(VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetB()));

		Matrix34 CreationMatrix;
		CreationMatrix.Identity();
		CreationMatrix.d = CGameWorld::FindLocalPlayerCoors() + vecPlayerForward * 10.0f;
		CreationMatrix.d.z += 2.0f;

		pNewVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_OTHER, POPTYPE_RANDOM_AMBIENT, &CreationMatrix);

		if (pNewVehicle)
		{
			pNewVehicle->m_nVehicleFlags.bCreatedUsingCheat = true;
			pNewVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = true;

			for (s32 tries = 0; tries < 4; tries++)
			{
				CreationMatrix.Identity();
				switch (tries)
				{
					case 0:
						CreationMatrix.d = CGameWorld::FindLocalPlayerCoors() + vecPlayerForward * 10.0f;
						break;
					case 1:
						CreationMatrix.d = CGameWorld::FindLocalPlayerCoors() + vecPlayerRight * 10.0f;
						break;
					case 2:
						CreationMatrix.d = CGameWorld::FindLocalPlayerCoors() - vecPlayerRight * 10.0f;
						break;
					case 3:
						CreationMatrix.d = CGameWorld::FindLocalPlayerCoors() - vecPlayerForward * 10.0f;
						break;
				}
				CreationMatrix.d.z += 2.0f;
				pNewVehicle->SetMatrix(CreationMatrix);

				pNewVehicle->PlaceOnRoadAdjust();
				
				CBaseModelInfo* pModel = pNewVehicle->GetBaseModelInfo();
				Assert(pModel->GetFragType());
			
				int nTestTypeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;

				int	nTestTypeFlagsCars = ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE;


				Vector3 start, end;
				pNewVehicle->TransformIntoWorldSpace(start, Vector3(0.0f, pModel->GetBoundingBoxMax().y, 0.0f));
				pNewVehicle->TransformIntoWorldSpace(end, Vector3(0.0f, pModel->GetBoundingBoxMin().y, 0.0f));
				Vector3 start2 = start + Vector3(0.0f, 0.0f, 1.0f);
				Vector3 end2 = end + Vector3(0.0f, 0.0f, 1.0f);
	
				Vector3 startCars, endCars;
				pNewVehicle->TransformIntoWorldSpace(startCars, Vector3(0.0f, pModel->GetBoundingBoxMax().y+2.0f, 0.0f));
				pNewVehicle->TransformIntoWorldSpace(endCars, Vector3(0.0f, pModel->GetBoundingBoxMin().y-2.0f, 0.0f));

				Vector3 startCars2, endCars2;
				pNewVehicle->TransformIntoWorldSpace(startCars2, Vector3(2.0f, 0.0f, 0.0f));
				pNewVehicle->TransformIntoWorldSpace(endCars2, Vector3(-2.0f, 0.0f, 0.0f));

				Vector3 startCars3, endCars3;
				pNewVehicle->TransformIntoWorldSpace(startCars3, Vector3(0.0f, 0.0f, 2.0f));
				pNewVehicle->TransformIntoWorldSpace(endCars3, Vector3(0.0f, 0.0f, -2.0f));

				WorldProbe::CShapeTestProbeDesc probeDesc1;
				probeDesc1.SetStartAndEnd(start, end);
				probeDesc1.SetExcludeEntity(pNewVehicle);
				probeDesc1.SetIncludeFlags(nTestTypeFlags);
				probeDesc1.SetContext(WorldProbe::LOS_Unspecified);

				WorldProbe::CShapeTestProbeDesc probeDesc2;
				probeDesc2.SetStartAndEnd(start2, end2);
				probeDesc2.SetIncludeFlags(nTestTypeFlags);
				probeDesc2.SetContext(WorldProbe::LOS_Unspecified);

				WorldProbe::CShapeTestProbeDesc probeDesc3;
				probeDesc3.SetStartAndEnd(startCars, endCars);
				probeDesc3.SetExcludeEntity(pNewVehicle);
				probeDesc3.SetIncludeFlags(nTestTypeFlagsCars);
				probeDesc3.SetContext(WorldProbe::LOS_Unspecified);

				WorldProbe::CShapeTestProbeDesc probeDesc4;
				probeDesc4.SetStartAndEnd(startCars2, endCars2);
				probeDesc4.SetExcludeEntity(pNewVehicle);
				probeDesc4.SetIncludeFlags(nTestTypeFlagsCars);
				probeDesc4.SetContext(WorldProbe::LOS_Unspecified);

				WorldProbe::CShapeTestProbeDesc probeDesc5;
				probeDesc5.SetStartAndEnd(startCars3, endCars3);
				probeDesc5.SetExcludeEntity(pNewVehicle);
				probeDesc5.SetIncludeFlags(nTestTypeFlagsCars);
				probeDesc5.SetContext(WorldProbe::LOS_Unspecified);

				if (tries >= 3 || 
					(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc1) && 
					 !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc2) &&
					 !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc3) && 
					 !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc4) && 
					 !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc5)) )
				{
					break;		// We will use this one.
				}
			}

			CScriptEntities::ClearSpaceForMissionEntity(pNewVehicle);
			CGameWorld::Add(pNewVehicle, CGameWorld::OUTSIDE );
			pNewVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
		}
	}
	return pNewVehicle;
}




void CCheat::AnnihilatorCheat()
{
	if (!CPortal::IsInteriorScene())		// Helis are too big to be spawned inside interiors.
	{
		CVehicle *pVeh = CGameWorld::FindLocalPlayer()->GetVehiclePedInside();
		if (!pVeh || pVeh->GetVehicleType() != VEHICLE_TYPE_TRAIN)	
		{
			VehicleCheat("annihilator");

			m_LastTimeCheatUsed[ANNIHILATOR_CHEAT] = fwTimer::GetTimeInMilliseconds();
			SetCheatActivatedMessage(ANNIHILATOR_CHEAT);
		}
	}
}

void CCheat::Nrg900Cheat()
{
	VehicleCheat("nrg900"); 
	m_LastTimeCheatUsed[NRG900_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(NRG900_CHEAT);
}

void CCheat::FbiCheat()
{
	VehicleCheat("fbi"); 
	m_LastTimeCheatUsed[FBI_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(FBI_CHEAT);
}

void CCheat::JetmaxCheat()
{
	if (!CPortal::IsInteriorScene())		// Jetmaxes are too big to be spawned inside interiors.
	{
		CVehicle *pVeh = CGameWorld::FindLocalPlayer()->GetVehiclePedInside();
		if (!pVeh || pVeh->GetVehicleType() != VEHICLE_TYPE_TRAIN)	
		{
			VehicleCheat("jetmax");
			m_LastTimeCheatUsed[JETMAX_CHEAT] = fwTimer::GetTimeInMilliseconds();
			SetCheatActivatedMessage(JETMAX_CHEAT);
		}
	}
}

void CCheat::CometCheat()
{
	VehicleCheat("comet");
	m_LastTimeCheatUsed[COMET_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(COMET_CHEAT);
}


void CCheat::TurismoCheat()
{
	VehicleCheat("turismo");
	m_LastTimeCheatUsed[TURISMO_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(TURISMO_CHEAT);
}

void CCheat::CognoscentiCheat()
{
	VehicleCheat("cognoscenti");
	m_LastTimeCheatUsed[COGNOSCENTI_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(COGNOSCENTI_CHEAT);
}


void CCheat::SuperGTCheat()
{
	VehicleCheat("supergt");
	m_LastTimeCheatUsed[SUPERGT_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(SUPERGT_CHEAT);
}

void CCheat::SanchezCheat()
{
	VehicleCheat("sanchez");
	m_LastTimeCheatUsed[SANCHEZ_CHEAT] = fwTimer::GetTimeInMilliseconds();
	SetCheatActivatedMessage(SANCHEZ_CHEAT);
}

/*
void CCheat::SlownessEffectCheat()
{
	static char *aStrings[4] = { "Slowness Effect: 0 (drag*=2.0)", "Slowness Effect: 1 (drag*=1.5)", "Slowness Effect: 2 (drag*=1.25)", "Slowness Effect: 3 (drag*=1.0)" };
	
	char	uString[64];

	m_SlownessEffect = (m_SlownessEffect + 1)%4;

	s32 i = 0;
	while (aStrings[m_SlownessEffect][i] != 0)
	{
		uString[i] = (char)aStrings[m_SlownessEffect][i];
		uString[i+1] = 0;
		i++;
	}

	CMessages::HelpMessage.SetMessageWithNumber(uString);
}
*/

//////////////////////////////////////////////////////////////////////////////
// ScriptActivatedCheat
// The script can activate cheats (The player can activate them with the phone)
//////////////////////////////////////////////////////////////////////////////

void CCheat::ScriptActivatedCheat(s32 cheatNum)
{
	char cheatText[50];
	Assert(cheatNum >= 0 && cheatNum < NUM_CHEATS);

	ActivateCheat(cheatNum);

	sprintf(cheatText, "%d", cheatNum);

	cheatDisplayf("Script cheated - %s", cheatText);

	CNetworkTelemetry::CheatApplied(cheatText);
	CCheat::SetCheatActivatedMessage(cheatNum);
}

//////////////////////////////////////////////////////////////////////////////
// IncrementTimesCheatedStat
//
//////////////////////////////////////////////////////////////////////////////

void CCheat::IncrementTimesCheatedStat()
{
	CPed* playerPed = FindPlayerPed();
	CPedModelInfo* mi = playerPed ? playerPed->GetPedModelInfo() : NULL;
	if (!mi)
		return;

	static u32 lastFrameCheatDone = 0;

	//This is needed because the script is calling 2 cheats for one phone number. The stat should only increase once.
	if (fwTimer::GetSystemFrameCount() != lastFrameCheatDone)
	{
		StatId stat = StatsInterface::GetStatsModelHashId("TIMES_CHEATED");
		StatsInterface::IncrementStat(stat, 1.0f);
		lastFrameCheatDone = fwTimer::GetSystemFrameCount();
	}
}

void CCheat::SetCheatActivatedMessage(s32 cheatNum)
{
	Assert(cheatNum >= 0 && cheatNum < NUM_CHEATS);

	char cCheatText[50];
	sprintf(cCheatText, "CHEAT%d", cheatNum);

	char* pString = TheText.Get(cCheatText); // cheat activated
	CGameStreamMgr::GetGameStream()->PostTicker( pString, false );
}

//////////////////////////////////////////////////////////////////////////////
// InitWidgets
//
//////////////////////////////////////////////////////////////////////////////
#if __BANK
void CCheat::InitWidgets()
{
	bkBank *bank = CGameLogic::GetGameLogicBank();

	bank->PushGroup("Cheats");
		bank->AddToggle("Allow Development Cheats", &m_bAllowDevelopmentCheats);
#if __DEV && 0
		bank->AddButton("Output file cheat hashkeys", &GenerateCheatHashKeys);
#endif
	bank->PopGroup();
}

#endif // __BANK



