#ifndef WEAPON_CONTROLLER_H
#define WEAPON_CONTROLLER_H

// Game headers
#include "fwtl/pool.h"

// Forward declarations
class CPed;

//////////////////////////////////////////////////////////////////////////
// WeaponControllerState
//////////////////////////////////////////////////////////////////////////

// These are the actions that can be performed by the weapon controller
enum WeaponControllerState
{
	WCS_None,
	WCS_Aim,
	WCS_Fire,
	WCS_FireHeld,
	WCS_Reload,
	WCS_CombatRoll,
};

//////////////////////////////////////////////////////////////////////////
// CWeaponController
//////////////////////////////////////////////////////////////////////////

class CWeaponController
{
public:

	enum WeaponControllerType
	{
		WCT_Player,
		WCT_Aim,
		WCT_Fire,
		WCT_FireHeld,
		WCT_Reload,
		WCT_None,

		WCT_Max,
	};

	CWeaponController() {}
	virtual ~CWeaponController() {}

	// This gets the controller state
	virtual WeaponControllerState GetState(const CPed* pPed, const bool bFireAtLeastOnce) const = 0;
};

//////////////////////////////////////////////////////////////////////////
// CWeaponControllerFixed
//////////////////////////////////////////////////////////////////////////

// Is constructed with a specific state for its lifetime
class CWeaponControllerFixed : public CWeaponController
{
public:

	// Construct a weapon controller with a fixed state
	CWeaponControllerFixed(WeaponControllerState state) : m_state(state) {}
	virtual ~CWeaponControllerFixed() {}

	// This gets the controller state
	virtual WeaponControllerState GetState(const CPed* UNUSED_PARAM(pPed), const bool UNUSED_PARAM(bFireAtLeastOnce)) const { return m_state; }

private:

	// Our state we are performing
	WeaponControllerState m_state;
};

//////////////////////////////////////////////////////////////////////////
// CWeaponControllerPlayer
//////////////////////////////////////////////////////////////////////////

// Takes player input and returns the appropriate state
class CWeaponControllerPlayer : public CWeaponController
{
public:

	CWeaponControllerPlayer() {}
	virtual ~CWeaponControllerPlayer() {}

	// This gets the controller state
	virtual WeaponControllerState GetState(const CPed* pPed, const bool bFireAtLeastOnce) const;
};

////////////////////////////////////////////////////////////////////////////////
// CWeaponControllerManager
////////////////////////////////////////////////////////////////////////////////

class CWeaponControllerManager
{
public:

	static void Init();
	static void Shutdown();

	// Get a controller
	static const CWeaponController* GetController(CWeaponController::WeaponControllerType type) { return ms_WeaponControllers[type]; }

private:

	// Storage for weapon controllers
	static CWeaponController* ms_WeaponControllers[CWeaponController::WCT_Max];
};

#endif // WEAPON_CONTROLLER_H
