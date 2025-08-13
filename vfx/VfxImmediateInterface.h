#ifndef _VFXIMMEDIATEINTERFACE_H_
#define _VFXIMMEDIATEINTERFACE_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY
#include "systems/VfxWater.h"

class CVfxImmediateInterface
{
public:

	static void		TriggerPtFxSplashHeliBlade(CEntity* pParent, Vec3V_In vPos)																														{ g_vfxWater.TriggerPtFxSplashHeliBlade(pParent, vPos);	}

	// ped splash ptfx
	static void		TriggerPtFxSplashPedIn		(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* pBoneWaterInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, s32 boneIndex, Vec3V_In vBoneDir, float boneSpeed, bool isPlayerVfx)		{	g_vfxWater.TriggerPtFxSplashPedIn(pPed, pVfxPedInfo, pSkeletonBoneInfo, pBoneWaterInfo, pVfxWaterSampleData, boneIndex, vBoneDir, boneSpeed, isPlayerVfx);	}
	static void		TriggerPtFxSplashPedOut		(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, s32 boneIndex, Vec3V_In vBoneDir, float boneSpeed)																													{	g_vfxWater.TriggerPtFxSplashPedOut(pPed, pVfxPedInfo, pSkeletonBoneInfo, boneIndex, vBoneDir, boneSpeed);	}
	static void		UpdatePtFxSplashPedWade		(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* pBoneWaterInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vBoneDir, float boneSpeed, s32 ptFxOffset)	{	g_vfxWater.UpdatePtFxSplashPedWade(pPed, pVfxPedInfo, pSkeletonBoneInfo, pBoneWaterInfo, pVfxWaterSampleData, vRiverVel, vBoneDir, boneSpeed, ptFxOffset);	}
	static void		UpdatePtFxSplashPedTrail	(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* pBoneWaterInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vBoneDir, float boneSpeed, s32 ptFxOffset)	{	g_vfxWater.UpdatePtFxSplashPedTrail(pPed, pVfxPedInfo, pSkeletonBoneInfo, pBoneWaterInfo, pVfxWaterSampleData, vRiverVel, vBoneDir, boneSpeed, ptFxOffset);	}

	// vehicle splash ptfx
	static void		TriggerPtFxSplashVehicleIn	(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float downwardSpeed)	{	g_vfxWater.TriggerPtFxSplashVehicleIn(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vLateralDir, lateralSpeed, downwardSpeed);	}
	static void		TriggerPtFxSplashVehicleOut	(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float upwardSpeed)		{	g_vfxWater.TriggerPtFxSplashVehicleOut(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vLateralDir, lateralSpeed, upwardSpeed);		}
	static void		UpdatePtFxSplashVehicleWade	(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset)			{	g_vfxWater.UpdatePtFxSplashVehicleWade(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vRiverVel, vLateralVel, ptFxOffset);			}
	static void		UpdatePtFxSplashVehicleTrail(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset)			{	g_vfxWater.UpdatePtFxSplashVehicleTrail(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vRiverVel, vLateralVel, ptFxOffset);		}

	// object splash ptfx
	static void		TriggerPtFxSplashObjectIn	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float downwardSpeed)			{	g_vfxWater.TriggerPtFxSplashObjectIn(pObject, pVfxWaterSampleData, vLateralDir, lateralSpeed, downwardSpeed);	}
	static void		TriggerPtFxSplashObjectOut	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float upwardSpeed)			{	g_vfxWater.TriggerPtFxSplashObjectOut(pObject, pVfxWaterSampleData, vLateralDir, lateralSpeed, upwardSpeed);	}
	static void		UpdatePtFxSplashObjectWade	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset)				{	g_vfxWater.UpdatePtFxSplashObjectWade(pObject, pVfxWaterSampleData, vRiverVel, vLateralVel, ptFxOffset);		}
	static void		UpdatePtFxSplashObjectTrail	(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset)				{	g_vfxWater.UpdatePtFxSplashObjectTrail(pObject, pVfxWaterSampleData, vRiverVel, vLateralVel, ptFxOffset);		}

	// boat ptfx
	static void		UpdatePtFxBoatBow			(CVehicle* pBoat, const CVfxVehicleInfo* pVfxVehicleInfo, s32 registerOffset, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vDir, bool goingForward)	{ g_vfxWater.UpdatePtFxBoatBow(pBoat, pVfxVehicleInfo, registerOffset, pVfxWaterSampleData, vDir, goingForward);	}
	static void		UpdatePtFxBoatWash			(CVehicle* pBoat, const CVfxVehicleInfo* pVfxVehicleInfo, s32 registerOffset, const VfxWaterSampleData_s* pVfxWaterSampleData)										{ g_vfxWater.UpdatePtFxBoatWash(pBoat, pVfxVehicleInfo, registerOffset, pVfxWaterSampleData);	}
	static void		TriggerPtFxBoatEntry		(CVehicle* pBoat, const CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos)																							{ g_vfxWater.TriggerPtFxBoatEntry(pBoat, pVfxVehicleInfo, vPos);	}
	static void		TriggerPtFxBoatExit		(CVehicle* pBoat, const CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos)																								{ g_vfxWater.TriggerPtFxBoatExit(pBoat, pVfxVehicleInfo, vPos);	}
};

#endif // GTA_REPLAY

#endif // _VFXIMMEDIATEINTERFACE_H_
