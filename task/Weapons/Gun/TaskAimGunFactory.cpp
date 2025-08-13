// Class header
#include "Task/Weapons/Gun/TaskAimGunFactory.h"

// Game headers
#include "Scene/Entity.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/Gun/TaskAimGunScripted.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskAimGunFactory
//////////////////////////////////////////////////////////////////////////

CTaskAimGun* CTaskAimGunFactory::CreateTask(CTaskTypes::eTaskType type, const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const fwFlags32& iGFFlags, const CWeaponTarget& target, const CGunIkInfo& ikInfo, CMoveNetworkHelper* pMoveNetworkHelper, u32 uBlockFiringTime)
{
	CTaskAimGun* pTask = NULL;

	switch(type)
	{
	case CTaskTypes::TASK_AIM_GUN_ON_FOOT:
		{
			pTask = rage_new CTaskAimGunOnFoot(weaponControllerType, iFlags, iGFFlags, target, ikInfo, uBlockFiringTime);
		}
		break;

	case CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY:
		{
			pTask = rage_new CTaskAimGunVehicleDriveBy(weaponControllerType, iFlags, target);
		}
		break;

	case CTaskTypes::TASK_AIM_GUN_SCRIPTED:
		{
			pTask = rage_new CTaskAimGunScripted(weaponControllerType, iFlags, target);
		}
		break;

	case CTaskTypes::TASK_AIM_GUN_BLIND_FIRE:
		{
			taskFatalAssertf(pMoveNetworkHelper, "NULL network helper");
			pTask = rage_new CTaskAimGunBlindFire(weaponControllerType, iFlags, target, *pMoveNetworkHelper, ikInfo);
		}
		break;

	default:
		{
			Assertf(0, "Task type [%d] is not a valid aim gun task\n", type);
			return NULL;
		}
	}

	Assertf(pTask, "No aim gun task created\n");
	return pTask;
}
