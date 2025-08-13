// Title	:	Cheat.h
// Author	:	John Gurney
// Started	:	25/8/04

#ifndef _CHEAT_H_
#define _CHEAT_H_

#if __WIN32PC
#define CHEATMEM		30	// maximum number of characters we remember for cheats
#else
#define CHEATMEM		20	// maximum number of characters we remember for cheats
#endif

typedef void (*CheatFunction)();

class CVehicle;

class CCheat
{
public:

	enum eCheat
	{
		POOR_WEAPON_CHEAT = 0,
		ADVANCED_WEAPON_CHEAT,
		HEALTH_CHEAT,
		WANTED_LEVEL_UP_CHEAT,
		WANTED_LEVEL_DOWN_CHEAT,
		MONEY_UP_CHEAT,
		CHANGE_WEATHER_CHEAT,
		ANNIHILATOR_CHEAT,
		NRG900_CHEAT,
		FBI_CHEAT,
		JETMAX_CHEAT,
		COMET_CHEAT,
		TURISMO_CHEAT,
		COGNOSCENTI_CHEAT,
		SUPERGT_CHEAT,
		SANCHEZ_CHEAT,
//		SLOWNESS_TEST_CHEAT,
		NUM_CHEATS
	};

	
	// ** HOW TO ADD A NEW CHEAT **
	// 1) add a new enum above for the new cheat, and updated the enum of cheats ids in commands_misc.sch
	// 2) if it requires a function, create one and add it to m_aCheatFunctions, otherwise just add a NULL in the right place
	// 3) add a corresponding entry to m_aCheatNameIds, which is the number of the cheat id held in american.txt which is displayed when the cheat is activated
	// 4) add the pad/keyboard string to activate the cheat into m_aCheatStrings
	// 5) run the game in release / debug, and activate widget 'Game Logic/Cheat/Output file cheat hashkeys  this will generate the CHEATHASHKEYS.DAT file
	// 6) copy the contents of this file and paste into m_aCheatHashKeys
	// 7) build and run the game again and the cheat should work
	
	// ** HOW TO TEST IF A CHEAT IS ACTIVE ** 
	// call IsCheatActive() with one of the enums above
	
	static void Init(unsigned initMode);

#if __BANK
	static void InitWidgets();
#endif

	static void DoCheats();

	static void ResetCheats();

	// gameplay cheat functions
	static void HealthCheat();
	static void WeaponCheat1();

	static bool CheatHappenedRecently(s32 cheatId, u32 time);

	static void ScriptActivatedCheat(s32 cheatNum);
	static bool HasCheatActivated(const char* cheatCode);
	static bool HasCheatActivated(u32 hashOfCheatString, u32 lengthOfCheatString);

#if RSG_PC
	static bool HasPcCheatActivated(u32 hashOfCheatString);
#endif // RSG_PC

public:
	static bool m_bHasPlayerCheated;
	static u32	m_MoneyCheated;

	static CVehicle* CreateVehicleAdminIntf(const char* vehicleName);

private:

	static void ClearCheatString();
	static void AddToCheatString(char NewChar);

	static void ClearArrayOfHashesOfPreviousKeyPresses();

	static void FillArrayOfHashesOfPreviousKeyPresses();

	static void ActivateCheat(u32 ch)
	{
		if (m_aCheatFunctions[ch])
			m_aCheatFunctions[ch]();
		else
			m_aCheatsActive[ch] = !m_aCheatsActive[ch];
	}
	static bool IsCheatActive(u32 ch) { return m_aCheatsActive[ch]; }
	static void ClearCheat(u32 ch) { m_aCheatsActive[ch] = false; }


	// used by other cheat functions:
	static CVehicle* VehicleCheat(const char* pName, bool isCheat = true);

	// gameplay cheat functions
	static void PoorWeaponCheat();
	static void AdvancedWeaponCheat();
	static void WantedLevelUpCheat();
	static void WantedLevelDownCheat();
	static void MoneyUpCheat();
	static void ChangeWeatherCheat();
	static void AnnihilatorCheat();
	static void Nrg900Cheat();
	static void FbiCheat();
	static void JetmaxCheat();
	static void CometCheat();
	static void TurismoCheat();
	static void CognoscentiCheat();
	static void SuperGTCheat();
	static void SanchezCheat();
	static void SlownessEffectCheat();

	static void IncrementTimesCheatedStat();

	// debug cheats
#if !__FINAL
	static void MoneyArmourHealthCheat();
	static void FunnyCheat();
#endif


	static void SetCheatActivatedMessage(s32 cheatNum);


	static char				m_CheatString[CHEATMEM];
	static u32			sm_aHashesOfPreviousKeyPresses[CHEATMEM];

	static bool			m_aCheatsActive[NUM_CHEATS];
	static u32			m_LastTimeCheatUsed[NUM_CHEATS];
	static CheatFunction 	m_aCheatFunctions[NUM_CHEATS];


//	static u32			m_SlownessEffect;


#if !__FINAL
	static bool				m_bAllowDevelopmentCheats;
	static bool				m_bInNetworkGame;
#endif

#if RSG_PC
	static bool				m_bCheckPcKeyboard;
	static u32				m_uPcCheatCodeHash;
#endif // RSG_PC
};


#endif

