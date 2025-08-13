// Title	:	PedType.cpp
// Author	:	Gordon Speirs
// Started	:	17/09/99

// C headers
#include <stdio.h>
#include <string.h>

// Rage headers
#include "System\Timer.h"
#include "system/memory.h"

// Framework headers
//#include "fwmaths\Maths.h"

// Game headers
#include "Debug\Debug.h"
#include "Peds\PedType.h"
#include "System\FileMgr.h"

CPedType* CPedType::ms_apPedTypes;
dev_float CPedType::ms_fMaxHeadingChange = ( DtoR * 360.0f*2.5f);

const char *aPedTypeNames[PEDTYPE_LAST_PEDTYPE] =
{
	"PLAYER_0",
	"PLAYER_1",
	"NETWORK_PLAYER",
	"PLAYER_2",
	"CIVMALE",
	"CIVFEMALE",
	"COP",
	"GANG_ALBANIAN",
	"GANG_BIKER_1",
	"GANG_BIKER_2",
	"GANG_ITALIAN",
	"GANG_RUSSIAN",
	"GANG_RUSSIAN_2",
	"GANG_IRISH",
	"GANG_JAMAICAN",
	"GANG_AFRICAN_AMERICAN",
	"GANG_KOREAN",
	"GANG_CHINESE_JAPANESE",
	"GANG_PUERTO_RICAN",
	"DEALER",
	"MEDIC",
	"FIREMAN",
	"CRIMINAL",
	"BUM",
	"PROSTITUTE",
	"SPECIAL",
	"MISSION",
	"SWAT",
	"ANIMAL",
	"ARMY",
};

// Name			:	IsPlayerModel
// Purpose		:	Takes a string and returns whether this is one of the reserved player names.
// Parameters	:	pString - string to check
// Returns		:	if it is a player model or not.
bool CPedType::IsPlayerModel(const atHashString& hash)
{
	static const atHashValue player0("PLAYER_0");
	static const atHashValue player1("PLAYER_1");
	static const atHashValue player2("PLAYER_2");

	if(hash == player0 || hash == player1 || hash == player2)
	{
		return true;
	}

	return false;
}

// Name			:	FindPedType
// Purpose		:	Takes a string and returns the corresponding ped type
// Parameters	:	pString - string to check
// Returns		:	Ped type
ePedType CPedType::FindPedType(const atHashString& hash)
{
	for(int i=0; i<PEDTYPE_LAST_PEDTYPE; i++)
	{
		if(hash == atHashValue(aPedTypeNames[i]))
		{
			return ePedType(i);
		}
	}

#if !__FINAL	
	Errorf("Unknown ped type %s, Pedtype.cpp", hash.GetCStr());
#endif		
	return PEDTYPE_LAST_PEDTYPE;
}

void CPedType::GetPedTypeNameFromId(s32 id, char* retString)
{
	Assert( id >= 0 && id < PEDTYPE_LAST_PEDTYPE );
	sprintf(retString, "%s", aPedTypeNames[id]);
}

const char* CPedType::GetPedTypeNameFromId(const ePedType pedType)
{
    return AssertVerify(pedType >= 0 && pedType < PEDTYPE_LAST_PEDTYPE)
            ? aPedTypeNames[pedType]
            : NULL;
}
