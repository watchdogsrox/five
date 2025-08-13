// File header
#include "game/Riots.h"

// Game headers
#include "Peds/ped.h"
#include "weapons/Weapon.h"

// Parser headers
#include "game/Riots_parser.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CRiots
////////////////////////////////////////////////////////////////////////////////

CRiots* CRiots::sm_Instance = NULL;

CRiots::Tunables CRiots::sm_Tunables;
IMPLEMENT_TUNABLES(CRiots, "CRiots", 0x8005b3e6, "Riots Tuning", "Game Logic")

CRiots::CRiots()
: m_bEnabled(false)
{

}

CRiots::~CRiots()
{

}

void CRiots::UpdateInventory(CPed& rPed)
{
	//Ensure riots are enabled.
	if(!m_bEnabled)
	{
		return;
	}

	//Ensure the inventory is valid.
	CPedInventory* pInventory = rPed.GetInventory();
	if(!pInventory)
	{
		return;
	}

	//Give the ped a gun.
	static const atHashWithStringNotFinal s_hPistol("WEAPON_PISTOL",0x1B06D571);
	static const atHashWithStringNotFinal s_hMicroSmg("WEAPON_MICROSMG",0x13532244);
	u32 uHash = fwRandom::GetRandomTrueFalse() ? s_hPistol.GetHash() : s_hMicroSmg.GetHash();
	pInventory->AddWeaponAndAmmo(uHash, CWeapon::INFINITE_AMMO);
}

void CRiots::Init(unsigned UNUSED_PARAM(initMode))
{
	//Create the instance.
	Assert(!sm_Instance);
	sm_Instance = rage_new CRiots;
}

void CRiots::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Free the instance.
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}
