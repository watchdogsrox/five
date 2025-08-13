#ifndef WEAPON_DEBUG_H
#define WEAPON_DEBUG_H

// Rage includes
#include "Bank/Bank.h"

// Game headers
#include "Debug/DebugDrawStore.h"

#if __BANK // Only included in __BANK builds

class CAmmoRocketInfo;
class CPed;

//////////////////////////////////////////////////////////////////////////
// CWeaponDebug
//////////////////////////////////////////////////////////////////////////

class CWeaponDebug
{
public:

#if DEBUG_DRAW
	static CDebugDrawStore ms_debugStore;
#endif // DEBUG_DRAW

	static void InitBank();
	static void AddWidgets(bkBank& bank);
	static void ShutdownBank(); 

	// Debug rendering
	static void RenderDebug();

	//
	// Accessors
	//

	// Get the focus ped
	static CPed* GetFocusPed();

	// Get should render debug bullets
	static bool GetRenderDebugBullets() { return ms_bRenderDebugBullets; }

	// Get should we use ragdoll for stungun and rubbergun
	static bool GetUseRagdollForStungunAndRubbergun() { return ms_bUseRagdollForStungunAndRubbergun; }

private:

	// Init the general weapons widgets
	static void InitWidgets(bkBank& bank);

	// Update the infinite ammo
	static void UpdateInfiniteAmmo();

	// Update the focus ped statics
	static void UpdateFocusPedText();

	// Highlight the focus ped
	static void HighlightFocusPed();

	// Update the hashed strings
	static void UpdateHasherStrings();

	//
	static void PrintPoolUsage();

	static void ClearAllProjectiles();

	//
	// Members
	//

	// Pointer to the main RAG bank
	static bkBank* ms_pBank;

	// Are we debug rendering?
	static bool ms_bRenderDebug;
	static bool ms_bRenderDebugBullets;

	// Infinite ammo?
	static bool ms_bUseInfiniteAmmo;

	// Use ragdoll for stungun and rubbergun
	static bool ms_bUseRagdollForStungunAndRubbergun;

	//
	// Focus ped members
	//

	// Our max string length
	static const s32 MAX_STRING_LENGTH = 64;

	// Focus ped address
	static char ms_focusPedAddress[MAX_STRING_LENGTH];

	// Focus ped pool index
	static char ms_focusPedPoolIndex[MAX_STRING_LENGTH];

	// Focus ped model name
	static char ms_focusPedModelName[MAX_STRING_LENGTH];

	//
	// Helper debug
	//

	// The string we are currently hashing
	static char ms_hashString[MAX_STRING_LENGTH];

	// The unsigned result
	static char ms_hashStringResultUnsigned[MAX_STRING_LENGTH];

	// The signed result
	static char ms_hashStringResultSigned[MAX_STRING_LENGTH];
};

#endif // __BANK

#endif // WEAPON_DEBUG_H
