// Title	:	PedType.h
// Author	:	Gordon Speirs
// Started	:	17/09/99

#ifndef INC_PEDTYPE_H_
#define INC_PEDTYPE_H_

#include "atl/hashstring.h"

// if modify ePedType must also update aPedTypeNames array in pedtype.cpp
enum ePedType
{
	PEDTYPE_INVALID=-1,
	PEDTYPE_PLAYER_0=0,
	PEDTYPE_PLAYER_1,
	PEDTYPE_NETWORK_PLAYER,
	PEDTYPE_PLAYER_2,
	PEDTYPE_CIVMALE,
	PEDTYPE_CIVFEMALE,
	PEDTYPE_COP,
	PEDTYPE_GANG_ALBANIAN,		//8bits
	PEDTYPE_GANG_BIKER_1,
	PEDTYPE_GANG_BIKER_2,
	PEDTYPE_GANG_ITALIAN,
	PEDTYPE_GANG_RUSSIAN,
	PEDTYPE_GANG_RUSSIAN_2,
	PEDTYPE_GANG_IRISH,
	PEDTYPE_GANG_JAMAICAN,
	PEDTYPE_GANG_AFRICAN_AMERICAN,		//16bits
	PEDTYPE_GANG_KOREAN,
	PEDTYPE_GANG_CHINESE_JAPANESE,		// new
	PEDTYPE_GANG_PUERTO_RICAN,			// new
	PEDTYPE_DEALER,
	PEDTYPE_MEDIC,
	PEDTYPE_FIRE,
	PEDTYPE_CRIMINAL,
	PEDTYPE_BUM,
	PEDTYPE_PROSTITUTE,
	PEDTYPE_SPECIAL,
	PEDTYPE_MISSION,
	PEDTYPE_SWAT,
	PEDTYPE_ANIMAL,
	PEDTYPE_ARMY,
	PEDTYPE_LAST_PEDTYPE
};
CompileTimeAssert(PEDTYPE_LAST_PEDTYPE < BIT(6));

//#define PEDTYPE_PLAYER_GANG		PEDTYPE_GANG2	//	(Gang2 is Groves which is the Player's gang)
//#define PEDTYPE_GANG_WAR_1		PEDTYPE_GANG1	//	Gang that player can engage in gang war
//#define PEDTYPE_GANG_WAR_2		PEDTYPE_GANG3	//	Other gang that player can engage in gang war
//#define PEDTYPE_FRIENDLY_GANG	PEDTYPE_GANG8	//	Don't attack gang8

/*
// INSTEAD OF THESE FLAGS JUST USE 2^PedType enum
// should be more reliable so long as PedType doesn't go past 32!
//
// flags for ped avoidance/threats
#define PED_FLAG_PLAYER1		(0x000000001)
#define PED_FLAG_PLAYER2		(0x000000002)
#define PED_FLAG_PLAYER_NETWORK	(0x000000004)
#define PED_FLAG_PLAYER_NOTUSED	(0x000000008)
#define PED_FLAG_CIVMALE		(0x000000010)
#define PED_FLAG_CIVFEMALE		(0x000000020)
#define PED_FLAG_COP			(0x000000040)
#define PED_FLAG_GANG1			(0x000000080)	//8bits
#define PED_FLAG_GANG2			(0x000000100)
#define PED_FLAG_GANG3			(0x000000200)
#define PED_FLAG_GANG4			(0x000000400)
#define PED_FLAG_GANG5			(0x000000800)
#define PED_FLAG_GANG6			(0x000001000)
#define PED_FLAG_GANG7			(0x000002000)
#define PED_FLAG_GANG8			(0x000004000)
#define PED_FLAG_GANG9			(0x000008000)	//16bits
#define PED_FLAG_GANG10			(0x000010000)
#define PED_FLAG_DEALER			(0x000020000)
#define PED_FLAG_MEDIC			(0x000040000)
#define PED_FLAG_FIREMAN		(0x000080000)
#define PED_FLAG_CRIMINAL		(0x000100000)
#define PED_FLAG_BUM			(0x000200000)
#define PED_FLAG_PROSTITUTE		(0x000400000)
#define PED_FLAG_SPECIAL		(0x000800000)	//24bits
#define PED_FLAG_MISSION1		(0x001000000)
#define PED_FLAG_MISSION2		(0x002000000)
#define PED_FLAG_MISSION3		(0x004000000)
#define PED_FLAG_MISSION4		(0x008000000)
#define PED_FLAG_MISSION5		(0x010000000)
#define PED_FLAG_MISSION6		(0x020000000)
#define PED_FLAG_MISSION7		(0x040000000)
#define PED_FLAG_MISSION8		(0x080000000)	//32bits
//
#define PED_FLAG_MAX			(0x100000000)
*/

class CPedType
{
public:

	static dev_float ms_fMaxHeadingChange;

private:

	static CPedType* ms_apPedTypes;

public:

	static bool	IsPlayerModel(const atHashString& hash);
	static ePedType FindPedType(const atHashString& hash);
			
	static bool IsSinglePlayerType(const s32 PedTypeIndex) {return ((PEDTYPE_PLAYER_0==PedTypeIndex) || (PEDTYPE_PLAYER_1==PedTypeIndex) || (PEDTYPE_PLAYER_2==PedTypeIndex)); }
	static bool IsMPPlayerType(const s32 PedTypeIndex) {return (PEDTYPE_NETWORK_PLAYER==PedTypeIndex); }
	static bool IsFemaleType(const s32 PedTypeIndex) {return ((PEDTYPE_PROSTITUTE==PedTypeIndex)||(PEDTYPE_CIVFEMALE==PedTypeIndex));}
	static bool IsCivilianType(const s32 PedTypeIndex) {return ((PedTypeIndex == PEDTYPE_CIVMALE) || (PedTypeIndex == PEDTYPE_CIVFEMALE));}
	static bool IsEmergencyType(const s32 PedTypeIndex) {return ((PedTypeIndex == PEDTYPE_MEDIC) || (PedTypeIndex == PEDTYPE_FIRE));}
	static bool IsCopType(const s32 PedTypeIndex) {return (PedTypeIndex == PEDTYPE_COP);}
	static bool IsSwatType(const s32 PedTypeIndex) {return (PedTypeIndex == PEDTYPE_SWAT);}
	static bool IsLawEnforcementType(const s32 PedTypeIndex) {return ((PedTypeIndex == PEDTYPE_SWAT) || (PedTypeIndex == PEDTYPE_COP) || (PedTypeIndex == PEDTYPE_ARMY));}
	static bool IsCriminalType(const s32 PedTypeIndex) {return ((PedTypeIndex == PEDTYPE_DEALER) || (PedTypeIndex == PEDTYPE_CRIMINAL) || (PedTypeIndex == PEDTYPE_PROSTITUTE));}
	static bool IsGangType(const s32 PedTypeIndex) {return ((PedTypeIndex >= PEDTYPE_GANG_ALBANIAN) && (PedTypeIndex <= PEDTYPE_GANG_PUERTO_RICAN));}
	static bool IsAnimalType(const s32 PedTypeIndex) {return (PedTypeIndex == PEDTYPE_ANIMAL);}
	static bool IsArmyType(const s32 PedTypeIndex) {return (PedTypeIndex == PEDTYPE_ARMY);}
	static bool IsMedicType(const s32 PedTypeIndex) {return (PedTypeIndex == PEDTYPE_MEDIC);}

	static void GetPedTypeNameFromId(s32 id, char* retString);
	static const char* GetPedTypeNameFromId(const ePedType pedType);
#if !__FINAL
//	static void DumpPedTypeAcquaintanceInfo();
#endif
};



#define STAT_FIGHT_PUNCHONLY	0x001	// ped can only punch (weak)
#define STAT_FIGHT_KNEEHEAD	 	0x002	// ped can fight dirty at close range
#define STAT_FIGHT_CANKICK		0x004	// ped can kick (normal kicks)
#define STAT_FIGHT_CANROUNDHSE	0x008	// ped can do roundhouse kick
#define STAT_EVADE_NODIVE		0x010	// ped can dive away from cars or bullets
#define STAT_ONEHIT_KNOCKDOWN	0x020	// ped gets knocked down with a single hit
#define STAT_FIGHT_SHOPBAGS     0x040   // ped can only lash out with shopping bags
#define	STAT_SCARED_OF_GUNS		0x080	// ped doesn't like guns (panic if targeted!)

//#define MAX_HEADING_CHANGE	(11.0f)		// This used to be a PedStat but was 11.0f for every single ped.

#endif
