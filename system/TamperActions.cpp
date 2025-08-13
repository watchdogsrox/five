#include "TamperActions.h"
//Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/msgbox.h"
#include "security/obfuscatedtypes.h"

//Framework headers
#include "fwdecorator/decoratorExtension.h"
#include "fwdecorator/decoratorInterface.h"

//Game headers
#include "game/weather.h"
#include "script/commands_player.h"
#include "Stats/StatsInterface.h"
#include "script/commands_fire.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "Network/General/NetworkAssetVerifier.h"
#include "vfx/systems/VfxWheel.h"
#include "vfx/systems/VfxPed.h"

#include "script/thread.h"

#if TAMPERACTIONS_ENABLED

#if RSG_PC || RSG_DURANGO
	#define NOINLINE __declspec(noinline)
#endif

// Assert instead of triggering the tamper actions? Overridden if you use the RAG widgets
#define ASSERT_TAMPER_ACTION !__FINAL

// Fake assert so we get an assert-like message box in bank release
#if __ASSERT
	#define AssertMsgBox(msg) Assertf(false, msg)
#else
	#define AssertMsgBox(msg) do { static bool ignored = false; \
		if(!ignored) { \
			int ret = MessageBoxA(NULL, msg, "Tamper Action Triggered", MB_ABORTRETRYIGNORE | MB_ICONERROR); \
			if(ret == IDABORT) __debugbreak(); \
			else if(ret == IDIGNORE) ignored = true; \
		} \
	} while(0)
#endif

// Macro inserted at the start of every tamper action:
// * Do an assert to let QA know it fired
// * If we're in prologue, bail out and do nothing
#define INVOKED_TAMPER_ACTION do { \
		if(ASSERT_TAMPER_ACTION) { AssertMsgBox(__FUNCTION__ " called"); } \
	} while(0)

enum TamperActionsEnum
{
	TAMPER_SixStarWantedLevel =			0x00000004,
	TAMPER_SpinningMiniMap =			0x00000020,
	TAMPER_PhoneDisabled_Online =		0x00000040,
	TAMPER_PhoneDisabled_Michael =		0x00000080,
	TAMPER_PhoneDisabled_Franklin =		0x00000200,
	TAMPER_PhoneDisabled_Trevor =		0x00001000,
	TAMPER_PhoneDisabled_All = TAMPER_PhoneDisabled_Online | TAMPER_PhoneDisabled_Michael | TAMPER_PhoneDisabled_Franklin | TAMPER_PhoneDisabled_Trevor,
	TAMPER_CopsInvincible =				0x00004000,
	TAMPER_DMTEffect =					0x00008000,
	TAMPER_Snow =						0x01000000,
	TAMPER_LeakPetrol =					0x02000000,
	TAMPER_KnockMask =					0xfcff2d1b,
	TAMPER_SaveGameFudge =				0x1a4e7d5f, // "Picked out of thin air" random number XOR'd with data for save game
};

// Currently switched on tamper actions
static sysObfuscated<u32, true> s_tamperActions;

// Currently switched on tamper actions, cached, non-mutating copy for thread safety
static sysObfuscated<u32, false> s_tamperActionsThreadSafe;

// Currently detected, but not necessarily enabled tamper actions - stored in the save game
static sysObfuscated<u32, true> s_deferredTamperActions;

//-------------------------------------------------------------------------------------------------

static __forceinline void UpdatePhoneDisabledDecorator(CPed& ped)
{
#if TAMPERACTIONS_DISABLEPHONE
	bool val = (sysObfuscatedTypes::obfRand() & 1) != 0;
	if((s_tamperActions.Get() & TAMPER_PhoneDisabled_Michael) && ped.GetModelNameHash() == atHashString("Player_Zero", 0xd7114c9))
		fwDecoratorExtension::Set(ped, atHashWithStringBank("Synched",0x495d1288), val);
	else if((s_tamperActions.Get() & TAMPER_PhoneDisabled_Online) && NetworkInterface::IsAnySessionActive())
		fwDecoratorExtension::Set(ped, atHashWithStringBank("Synched",0x495d1288), val);
	else if((s_tamperActions.Get() & TAMPER_PhoneDisabled_Franklin) && ped.GetModelNameHash() == atHashString("Player_One", 0x9b22dbaf))
		fwDecoratorExtension::Set(ped, atHashWithStringBank("Synched",0x495d1288), val);
	else if((s_tamperActions.Get() & TAMPER_PhoneDisabled_Trevor) && ped.GetModelNameHash() == atHashString("Player_Two", 0x9b810fa2))
		fwDecoratorExtension::Set(ped, atHashWithStringBank("Synched",0x495d1288), val);
	else
		fwDecoratorExtension::RemoveFrom(ped, atHashWithStringBank("Synched",0x495d1288));
#else
	(void)ped;
#endif
}

//-------------------------------------------------------------------------------------------------
#define TAMPERACTION_ID 0x8AC40FED 
NOINLINE void TamperAction_ClobberCode(int param)
{
	INVOKED_TAMPER_ACTION;
	if(param!=0)
	{
		//@@: location TAMPERACTION_CLOBBERCODE_CLOBBER_MATCHMAKING
		CNetworkTelemetry::AppendTamperMetric(param);
	}
}


NOINLINE void TamperAction_ReactGameServerAndCrash(int param)
{
	INVOKED_TAMPER_ACTION;
	RageSecPluginGameReactionObject obj(
		REACT_GAMESERVER, 
		TAMPERACTION_ID, 
		fwRandom::GetRandomNumber(),
		fwRandom::GetRandomNumber(),
		param);
	rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
	RageSecInduceStreamerCrash();
}

NOINLINE void TamperAction_ReactGpuCrash(int param)
{
	INVOKED_TAMPER_ACTION;
	GBuffer::IncrementAttached(param);
}

NOINLINE void TamperAction_GameserverReport(int param)
{
	INVOKED_TAMPER_ACTION;
	RageSecPluginGameReactionObject obj(
		REACT_GAMESERVER, 
		TAMPERACTION_ID, 
		fwRandom::GetRandomNumber(),
		fwRandom::GetRandomNumber(),
		param);
	rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
}

NOINLINE void TamperAction_SendTelemetry(int param)
{
	INVOKED_TAMPER_ACTION;

	CNetworkTelemetry::AppendTamperMetric(param);
}

NOINLINE void TamperAction_FrackBlendStates(int)
{
#if TAMPERACTIONS_DMT
	INVOKED_TAMPER_ACTION;

	GRCDEVICE.SetKillSwitch(grcDevice::s_KillSwitchMask);
#endif
}

NOINLINE void TamperAction_ReportPlayer(int param)
{
	INVOKED_TAMPER_ACTION;

	if(NetworkInterface::IsGameInProgress())
	{
		CReportMyselfEvent::Trigger(NetworkRemoteCheaterDetector::CODE_TAMPERING, param);
	}
	
}

NOINLINE void TamperAction_SetSixStarWantedLevel(int)
{
#if TAMPERACTIONS_SIXSTARWANTEDLEVEL
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_SixStarWantedLevel);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return;

	// Set 5 star wanted level; rendering of 6 star handled by code in NewHud.cpp, based on TamperActions::IsSixStarWantedLevel()
	const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	pPlayer->GetPlayerWanted()->SetWantedLevel(vPlayerPosition, WANTED_LEVEL5, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_CHEAT);
#endif
}

NOINLINE void TamperAction_SpinMiniMap(int)
{
#if TAMPERACTIONS_ROTATEMINIMAP
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_SpinningMiniMap);
#endif
}


NOINLINE void TamperAction_DisablePhone_Online(int)
{
#if TAMPERACTIONS_DISABLEPHONE
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Online);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return;

	UpdatePhoneDisabledDecorator(*pPlayer);
#endif
}

// Disable bringing up the phone if the local player is playing as Michael, and the game progress is > 50%
NOINLINE void TamperAction_DisablePhone_Michael(int)
{
#if TAMPERACTIONS_DISABLEPHONE
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Michael);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return;

	UpdatePhoneDisabledDecorator(*pPlayer);
#endif
}

NOINLINE void TamperAction_DisablePhone_Franklin(int)
{
#if TAMPERACTIONS_DISABLEPHONE
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Franklin);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return;

	UpdatePhoneDisabledDecorator(*pPlayer);
#endif
}

NOINLINE void TamperAction_DisablePhone_Trevor(int)
{
#if TAMPERACTIONS_DISABLEPHONE
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Trevor);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return;

	UpdatePhoneDisabledDecorator(*pPlayer);
#endif
}

NOINLINE void TamperAction_CopsInvincible(int)
{
#if TAMPERACTIONS_INVINCIBLECOP
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_CopsInvincible);
#endif
}

NOINLINE void TamperAction_DMT(int)
{
#if TAMPERACTIONS_DMT					
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_DMTEffect);
#endif
}

NOINLINE void TamperAction_Snow(int)
{
#if TAMPERACTIONS_SNOW
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_Snow);
#endif
}

NOINLINE void TamperAction_LeakPetrol(int)
{
#if TAMPERACTIONS_LEAKPETROL
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_LeakPetrol);
#endif
}

//-------------------------------------------------------------------------------------------------

NOINLINE void TamperActionSet_Cops(int param)
{
#if TAMPERACTIONS_INVINCIBLECOP && TAMPERACTIONS_SIXSTARWANTEDLEVEL && TAMPERACTIONS_DMT
	if(NetworkInterface::IsAnySessionActive())
	{
		TamperAction_SetSixStarWantedLevel(param);
		TamperAction_CopsInvincible(param);
		TamperAction_DMT(param);
	}
#else
	(void)param;
#endif
}

NOINLINE void TamperActionSet_Cars(int param)
{
#if TAMPERACTIONS_ROTATEMINIMAP && TAMPERACTIONS_LEAKPETROL
	TamperAction_LeakPetrol(param);
	TamperAction_SpinMiniMap(param);
#else
	(void)param;
#endif
}

NOINLINE void TamperAction_SetVideoClipModifiedContent(int param)
{
#if TAMPERACTIONS_SET_VIDEOCLIP_MOD
	CReplayMgr::SetWasModifiedContent();
#endif
	(void)param;
}


NOINLINE void TamperActionSet_PhoneNow(int)
{
#if TAMPERACTIONS_DISABLEPHONE
	INVOKED_TAMPER_ACTION;

	s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_All);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return;

	UpdatePhoneDisabledDecorator(*pPlayer);
#endif
}

NOINLINE void TamperActionSet_PhoneLater(int)
{
#if TAMPERACTIONS_DISABLEPHONE
	INVOKED_TAMPER_ACTION;

	s_deferredTamperActions.Set(s_deferredTamperActions.Get() | TAMPER_PhoneDisabled_All);

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return;

	UpdatePhoneDisabledDecorator(*pPlayer);
#endif
}

//-------------------------------------------------------------------------------------------------

#if __BANK
static bool s_frackBlendStates = false;
static bool s_sixStarWantedLevel = false;
static bool s_phoneDisabledOnline = false;
static bool s_phoneDisabledMichael = false;
static bool s_phoneDisabledFranklin = false;
static bool s_phoneDisabledTrevor = false;
static bool s_rotateMiniMap = false;
static bool s_copsInvincible = false;
static bool s_dmtEffect = false;
static bool s_snowing = false;
static bool s_leakPetrol = false;

static void Bank_SendTelemetryPlayerToPlayer()
{
	TamperAction_ReportPlayer(0xDEAD);
}
static void Bank_SendTelemetry()
{
	TamperAction_SendTelemetry(42);
}

static void Bank_FrackBlendStates()
{
	s_frackBlendStates = true;
	TamperAction_FrackBlendStates(0);
}

static void Bank_SpinMiniMap()
{
	if(s_rotateMiniMap)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_SpinningMiniMap);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_SpinningMiniMap));
}

static void Bank_SixStarWantedLevel()
{
	if(s_sixStarWantedLevel)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_SixStarWantedLevel);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_SixStarWantedLevel));

	if(s_sixStarWantedLevel)
	{
		TamperAction_SetSixStarWantedLevel(0);
	}
}

static void Bank_DisablePhone_Online()
{
	if(s_phoneDisabledOnline)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Online);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_PhoneDisabled_Online));

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
		UpdatePhoneDisabledDecorator(*pPlayer);
}

static void Bank_DisablePhone_Michael()
{
	if(s_phoneDisabledMichael)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Michael);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_PhoneDisabled_Michael));

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
		UpdatePhoneDisabledDecorator(*pPlayer);
}

static void Bank_DisablePhone_Franklin()
{
	if(s_phoneDisabledFranklin)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Franklin);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_PhoneDisabled_Franklin));

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
		UpdatePhoneDisabledDecorator(*pPlayer);
}

static void Bank_DisablePhone_Trevor()
{
	if(s_phoneDisabledTrevor)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_PhoneDisabled_Trevor);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_PhoneDisabled_Trevor));

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
		UpdatePhoneDisabledDecorator(*pPlayer);
}

static void Bank_CopsInvincible()
{
	if(s_copsInvincible)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_CopsInvincible);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_CopsInvincible));
}

static void Bank_DMTEffect()
{
	if(s_dmtEffect)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_DMTEffect);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_DMTEffect));

	if(!s_dmtEffect)
	{
		// Disable effect
		ANIMPOSTFXMGR.Stop(atHashString("DrugsDrivingIn", 0x8b05df27), AnimPostFXManager::kDefault);
	}
}

static void Bank_Snowing()
{
	if(s_snowing)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_Snow);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_Snow));

	if(!s_snowing)
	{
		// Disable snow
		g_weather.StopOverriding();
		g_weather.SetNextTypeIndex(0);
		g_vfxPed.SetUseSnowFootVfxWhenUnsheltered(false);
		g_vfxWheel.SetUseSnowWheelVfxWhenUnsheltered(false);
	}
}

static void Bank_LeakPetrol()
{
	if(s_leakPetrol)
		s_tamperActions.Set(s_tamperActions.Get() | TAMPER_LeakPetrol);
	else
		s_tamperActions.Set(s_tamperActions.Get() & (~TAMPER_LeakPetrol));
}

static void Bank_SetCops()
{
	s_sixStarWantedLevel = true;
	s_copsInvincible = true;
	s_dmtEffect = true;
	TamperActionSet_Cops(0);
}

static void Bank_SetCars()
{
	s_leakPetrol = true;
	s_rotateMiniMap = true;
	TamperActionSet_Cars(0);
}

static void Bank_SetPhoneNow()
{
	s_phoneDisabledOnline = true;
	s_phoneDisabledMichael = true;
	s_phoneDisabledFranklin = true;
	s_phoneDisabledTrevor = true;
	TamperActionSet_PhoneNow(0);
}

static void Bank_SetPhoneLater()
{
	TamperActionSet_PhoneLater(0);
}

static void Bank_CallCommandGetPlayerIndex()
{
	player_commands::CommandGetPlayerIndex();
}

typedef scrThread::State (*GtaThreadFunctionCall)(GtaThread *);
static volatile GtaThreadFunctionCall oldThreadCall;
static scrThread::State ReRouteCode(GtaThread *thread)
{
	return oldThreadCall(thread);
}
static void Bank_BlowUpVehicleFromLocalPlayer()
{
	// Get the local player
	CPed* localPlayerPed = CGameWorld::FindLocalPlayer();
	// Get Our position
	const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(localPlayerPed->GetTransform().GetPosition());
	// Blow up
	fire_commands::CommandAddOwnedExplosion(CTheScripts::GetGUIDFromEntity(*localPlayerPed), vPlayerPosition, EXP_TAG_GRENADELAUNCHER, 1.0, false, false, false);

}

static void Bank_BlowUpVehicleFromRemotePlayer()
{
	// Get the local player
	CPed* localPlayerPed = CGameWorld::FindLocalPlayer();
	// Get Our position
	const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(localPlayerPed->GetTransform().GetPosition());
	
	// Get the number of players
	unsigned                 numPhysicalPlayers		= netInterface::GetNumRemotePhysicalPlayers();
	// Get a list of those players
	const netPlayer * const *remotePhysicalPlayers	= netInterface::GetRemotePhysicalPlayers();

	for(unsigned index = 0; index < numPhysicalPlayers; index++)
	{
		const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

		// Check that the remote player is legit
		if(remotePlayer)
		{
			// Get my ped object
			CPed const* remotePlayerPed = remotePlayer->GetPlayerPed();
			// Cast away the const, like a cowboy
			CPed *remotePlayerPedNonConst = (CPed*)remotePlayerPed;
			// Trigger the explosion for testing
			fire_commands::CommandAddOwnedExplosion(CTheScripts::GetGUIDFromEntity(*remotePlayerPedNonConst), vPlayerPosition, EXP_TAG_GRENADELAUNCHER, 1.0, false, false, false);
			// Breaking out of the condition
			index = numPhysicalPlayers;
		}
	}
}
static void Bank_ClobberMainPersistentRunFunction()
{
	atArray<scrThread*> &threads = scrThread::GetScriptThreads();
	for (int i=0; i<threads.GetCount(); i++)
	{
		scrThread *t = threads[i];
		if (t)
		{
			// Target Main Persistent
			if(t->GetProgram() == 0x5700179C)
			{
				// Score the vtable that I want to modify. 
				DWORD64* vtablePtr = (DWORD64*)((DWORD64*)threads[i])[0];
				unsigned char *bytes = rage_new unsigned char[32];
				memset(bytes, 0, 32);
				// Wholesale copy
				memcpy(bytes, vtablePtr, 32);
				// Convert to DWORD64*
				DWORD64 * bytesPtr = (DWORD64*)bytes;
				// Increment to RUn
				bytesPtr+=2;
				// Store it off
				oldThreadCall = (GtaThreadFunctionCall)(*bytesPtr);
				// Write 
				*bytesPtr = (DWORD64)&ReRouteCode;
				memcpy((void*)t, &bytes, 8);
			}
			
		}
	}


}

void TamperActions::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Tamper Actions");

	bank.AddToggle("Fracked Blend States", &s_frackBlendStates, rage::datCallback(), NULL, NULL, true);
	bank.AddToggle("6 Star Wanted Level", &s_sixStarWantedLevel, Bank_SixStarWantedLevel);
    bank.AddToggle("Spin MiniMap", &s_rotateMiniMap, Bank_SpinMiniMap);
	bank.AddToggle("Phone Disabled (Online)", &s_phoneDisabledOnline, Bank_DisablePhone_Online);
	bank.AddToggle("Phone Disabled (Michael)", &s_phoneDisabledMichael, Bank_DisablePhone_Michael);
	bank.AddToggle("Phone Disabled (Franklin)", &s_phoneDisabledFranklin, Bank_DisablePhone_Franklin);
	bank.AddToggle("Phone Disabled (Trevor)", &s_phoneDisabledTrevor, Bank_DisablePhone_Trevor);
	bank.AddToggle("Cops Invincible", &s_copsInvincible, Bank_CopsInvincible);
	bank.AddToggle("DMT / Michael Hallucination Effect", &s_dmtEffect, Bank_DMTEffect);
	bank.AddToggle("Snowing", &s_snowing, Bank_Snowing);
	bank.AddToggle("Player Cars Leak Petrol Then Explode", &s_leakPetrol, Bank_LeakPetrol);
	bank.AddSeparator();
	bank.AddButton("Enable Fracked Blend States", Bank_FrackBlendStates);
	bank.AddButton("Send Telemetry", Bank_SendTelemetry);
	bank.AddButton("Send Telemetry (Player to Player)", Bank_SendTelemetryPlayerToPlayer);
	bank.AddSeparator();
	bank.AddButton("Cops+DMT Tamper Set", Bank_SetCops);
	bank.AddButton("Cars+Map Tamper Set", Bank_SetCars);
	bank.AddButton("Disable Phones Now Tamper Set", Bank_SetPhoneNow);
	bank.AddButton("Disable Phones Deferred Tamper Set", Bank_SetPhoneLater);
	bank.AddSeparator();
	bank.AddButton("CommandGetPlayerIndex", Bank_CallCommandGetPlayerIndex);
	bank.AddButton("Clobber MAIN_PERSISTENT Run Function", Bank_ClobberMainPersistentRunFunction);
	bank.AddButton("Blow up (From Remote Player)", Bank_BlowUpVehicleFromRemotePlayer);
	bank.AddButton("Blow up (From Local Player)", Bank_BlowUpVehicleFromLocalPlayer);
}
#endif

void TamperActions::Init(unsigned initMode)
{
	// Prevent dead-stripping of the tamper action functions
	if(initMode == 0xffffffff)
	{
		//@@: range TAMPERACTIONS_INIT_NOP {
		TamperAction_ReactGpuCrash(0);
		TamperAction_SendTelemetry(0);
		TamperAction_FrackBlendStates(0);
		TamperAction_ReportPlayer(0);
		TamperAction_SetSixStarWantedLevel(0);
		TamperAction_SpinMiniMap(0);
		TamperAction_DisablePhone_Michael(0);
		TamperAction_DisablePhone_Online(0);
		TamperAction_DisablePhone_Franklin(0);
		TamperAction_DisablePhone_Trevor(0);
		TamperAction_CopsInvincible(0);
		TamperAction_DMT(0);
		TamperAction_Snow(0);
		TamperAction_LeakPetrol(0);

		TamperActionSet_Cops(0);
		TamperActionSet_Cars(0);
		TamperActionSet_PhoneNow(0);
		TamperActionSet_PhoneLater(0);

		TamperAction_SetVideoClipModifiedContent(0);
		TamperAction_ClobberCode(0);
		TamperAction_GameserverReport(0);
		TamperAction_ReactGameServerAndCrash(0);
		//@@: } TAMPERACTIONS_INIT_NOP
	}
	else if(initMode == INIT_CORE)
	{
        gp_DecoratorInterface->Register(atHashWithStringBank("Synched",0x495d1288), fwDecorator::TYPE_BOOL, "");
	}
}

static bool ApplyTamperActionsFromDependency(const sysDependency& dep)
{
	// Apply any deferred tamper actions
	if(s_deferredTamperActions.Get() & (~TAMPER_KnockMask))
	{
		// Get game progress (0.0f..100.0f)
		float totalGameProgress = 0.0f;
		StatId statTotalProgressMade(atStringHash("TOTAL_PROGRESS_MADE"));
		if (StatsInterface::IsKeyValid(statTotalProgressMade) && StatsInterface::GetStatIsNotNull(statTotalProgressMade))
		{
			totalGameProgress = StatsInterface::GetFloatStat(statTotalProgressMade);
		}

		// Deferred phone disabled?
#if TAMPERACTIONS_DISABLEPHONE
		if(s_deferredTamperActions.Get() & TAMPER_PhoneDisabled_All)
		{
			// System time in ms until the phone becomes disabled, or zero if not triggered
			static sysObfuscated<u32, true> phoneDisabledTimeout(0);
			if(phoneDisabledTimeout.Get() == 0)
			{
				u32 phoneDisabledT = (sysObfuscatedTypes::obfRand() % 600) + 600; // 600..1200 seconds
				phoneDisabledTimeout.Set(sysTimer::GetSystemMsTime() + phoneDisabledT*1000);
			}

			// Disable phone after 4% game completion, or rand(10..20) mins. Whichever comes first.
			if(sysTimer::GetSystemMsTime() > phoneDisabledTimeout.Get() || totalGameProgress > 4.0f)
			{
				// Move to active actions
				u32 actions = s_deferredTamperActions.Get() & TAMPER_PhoneDisabled_All;
				s_tamperActions.Set(s_tamperActions.Get() | actions);
			}
		}
#endif
	}

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return true;

#if TAMPERACTIONS_SIXSTARWANTEDLEVEL
	if(s_tamperActions.Get() & TAMPER_SixStarWantedLevel)
	{
		// Set 5 star wanted level; rendering of 6 star handled by code in NewHud.cpp, based on TamperActions::IsSixStarWantedLevel()
		const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		pPlayer->GetPlayerWanted()->SetWantedLevel(vPlayerPosition, WANTED_LEVEL5, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_CHEAT);
	}
#endif

#if TAMPERACTIONS_DISABLEPHONE
	if(s_tamperActions.Get() & (TAMPER_PhoneDisabled_Online | TAMPER_PhoneDisabled_Michael | TAMPER_PhoneDisabled_Franklin | TAMPER_PhoneDisabled_Trevor))
	{
		UpdatePhoneDisabledDecorator(*pPlayer);
	}
#endif

#if TAMPERACTIONS_DMT
	if((s_tamperActions.Get() & TAMPER_DMTEffect) && !ANIMPOSTFXMGR.IsRunning(atHashString("DrugsDrivingIn", 0x8b05df27)))
	{
		ANIMPOSTFXMGR.Start(atHashString("DrugsDrivingIn", 0x8b05df27), 0, false, false, false, 0, AnimPostFXManager::kDefault);
	}
#endif

#if TAMPERACTIONS_SNOW
	if((s_tamperActions.Get() & TAMPER_Snow) && !g_weather.GetIsOverriding())
	{
		atHashString snowWeather("XMAS", 0xaac9c895);
		s32 typeIndex = g_weather.GetTypeIndex(snowWeather.GetHash());
		if(typeIndex >= 0 && typeIndex < g_weather.GetNumTypes())
		{
			// Set snow as next weather type if not already set
			if(g_weather.GetNextTypeIndex() != (u32)typeIndex)
			{
				g_weather.SetNextTypeIndex(typeIndex);
				g_vfxPed.SetUseSnowFootVfxWhenUnsheltered(true);
				g_vfxWheel.SetUseSnowWheelVfxWhenUnsheltered(true);
			}
			
			// "Lock" snow weather once set
			else if(g_weather.GetSnow() > 0.1f)
			{
				g_weather.OverrideType(typeIndex);
			}
		}
	}
#endif

	sysIpcSema* pSem = (sysIpcSema*)dep.m_Params[0].m_AsPtr;
	sysIpcSignalSema(*pSem);
	return true;
}

void TamperActions::Update()
{
	// Knock the tamper actions
	//@@: location TAMPERACTIONS_UPDATE_ENTRY
	u32 newRand = sysObfuscatedTypes::obfRand() & TAMPER_KnockMask;
	//@@: range TAMPERACTIONS_UPDATE_BODY {
	s_tamperActions.Set((s_tamperActions.Get() & (~TAMPER_KnockMask)) | newRand);
	// Re-apply any triggered tamper actions
	if((s_tamperActions.Get() | s_deferredTamperActions.Get()) & (~TAMPER_KnockMask))
	{
		static u32 nextWait = 1;
		--nextWait;
		if(nextWait == 0)
		{
			sysDependency dep;
			dep.Init(ApplyTamperActionsFromDependency, 0, sysDepFlag::INPUT0);
			sysIpcSema sem = sysIpcCreateSema(0);
			dep.m_Priority = sysDependency::kPriorityCritical;
			dep.m_Params[0].m_AsPtr = &sem;
			dep.m_DataSizes[0] = sizeof(sysIpcSema);
			sysDependencyScheduler::Insert(&dep);
			sysIpcWaitSema(sem);
			sysIpcDeleteSema(sem);

			// Re-apply after a random number of frames between 30 and 150 (1-5 sec @ 30Hz, 0.5-2.5 sec @ 60Hz)
			nextWait = (sysObfuscatedTypes::obfRand() % 120) + 30;
		}
	}
	//@@: } TAMPERACTIONS_UPDATE_BODY
}

void TamperActions::PerformSafeModeOperations()
{
	s_tamperActionsThreadSafe.Set(s_tamperActions.Get());
}

void TamperActions::OnPlayerSwitched(CPed&, CPed& newPed)
{
	if(!CMiniMap::GetInPrologue() && s_tamperActions.Get() & (TAMPER_PhoneDisabled_Online | TAMPER_PhoneDisabled_Michael | TAMPER_PhoneDisabled_Franklin | TAMPER_PhoneDisabled_Trevor))
	{
		UpdatePhoneDisabledDecorator(newPed);
	}
}

bool TamperActions::IsSixStarWantedLevel()
{
	// NB: Not the thread safe version, it's 1 frame late
	Assert(sysThreadType::IsUpdateThread());
	return (!CMiniMap::GetInPrologue() && s_tamperActions.Get() & TAMPER_SixStarWantedLevel) != 0;
}

bool TamperActions::ShouldRotateMiniMap()
{
	return (!CMiniMap::GetInPrologue() && s_tamperActionsThreadSafe.Get() & TAMPER_SpinningMiniMap) != 0;
}

bool TamperActions::IsPhoneDisabled(int)
{
	// It's "safer" to check if the local player has the decorator on them, so we don't go out
	// of sync with script. Plus it makes the online / Michael distinction easier :)
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
		return false;

	return !CMiniMap::GetInPrologue() && fwDecoratorExtension::ExistsOn(*pPlayer, atHashWithStringBank("Synched",0x495d1288));
}

bool TamperActions::AreCopsInvincible()
{
	return (!CMiniMap::GetInPrologue() && s_tamperActionsThreadSafe.Get() & TAMPER_CopsInvincible) != 0;
}

bool TamperActions::IsSnowing()
{
	return (s_tamperActionsThreadSafe.Get() & TAMPER_Snow) != 0;
}

bool TamperActions::DoVehiclesLeakPetrol()
{
	return (!CMiniMap::GetInPrologue() && s_tamperActionsThreadSafe.Get() & TAMPER_LeakPetrol) != 0;
}

bool TamperActions::IsInvincibleCop(CPed* pPed)
{
	if(!CPedType::IsLawEnforcementType(pPed->GetPedType()))
	{
		return false;
	}

	return AreCopsInvincible();
}

#endif // TAMPERACTIONS_ENABLED

//-------------------------------------------------------------------------------------------------

void TamperActions::SetTriggeredActionsFromSaveGame(u32)
{
	// Actions no longer stored in save game
}

u32 TamperActions::GetTriggeredActions()
{
	// Actions no longer stored in save game
	return 0xcafebeef;
}

//-------------------------------------------------------------------------------------------------
