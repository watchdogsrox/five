#ifndef TASK_AIM_GUN_FACTORY_H
#define TASK_AIM_GUN_FACTORY_H

// Framework headers
#include "fwutil/Flags.h"

// Game headers
#include "Task/System/TaskTypes.h"
#include "Task/Weapons/WeaponController.h"

// Forward declarations
class CGunIkInfo;
class CTaskAimGun;
class CWeaponTarget;
class CMoveNetworkHelper;

//////////////////////////////////////////////////////////////////////////
// CTaskAimGunFactory
//////////////////////////////////////////////////////////////////////////

class CTaskAimGunFactory
{
public:

	static CTaskAimGun* CreateTask(CTaskTypes::eTaskType type, const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const fwFlags32& iGFFlags, const CWeaponTarget& target, const CGunIkInfo& ikInfo, CMoveNetworkHelper* pMoveNetworkHelper = NULL, u32 uBlockFiringTime = 0);
};

#endif // TASK_AIM_GUN_FACTORY_H
