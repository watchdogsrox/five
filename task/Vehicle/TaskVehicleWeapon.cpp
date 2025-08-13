#include "Task/Vehicle/TaskVehicleWeapon.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/ThirdPersonPedAimCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/scripted/ScriptDirector.h"
#include "control/replay/Effects/ParticleVehicleFxPacket.h"
#include "Debug/DebugScene.h"
#include "frontend/PauseMenu.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PlayerInfo.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "system/control.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/TaskWeapon.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Trailer.h"
#include "vfx/systems/VfxVehicle.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Weapons/Weapon.h"
#include "weapons/info/weaponinfo.h"
#include "AI/Ambient/ConditionalAnimManager.h"
#include "VehicleAi/task/TaskVehicleAttack.h"

#if USE_MOUNTED_GUN_PM
#include "animation/PMDictionaryStore.h"
#endif //USE_MOUNTED_GUN_PM

AI_OPTIMISATIONS();
AI_VEHICLE_OPTIMISATIONS();

#if __BANK
int CTaskVehicleMountedWeapon::iTurretTuneDebugTimeToLive = 1;
#endif

//////////////////////////////////////////////////////////////////////////
// Class CTaskVehicleMountedWeapon
//////////////////////////////////////////////////////////////////////////

CTaskVehicleMountedWeapon::CTaskVehicleMountedWeapon(eTaskMode mode, const CAITarget* target, float fFireTolerance)
: m_mode(mode), m_fFireTolerance(fFireTolerance)
, m_targetPosition(Vector3(0.0f,0.0f,0.0f))
, m_targetPositionCam(VEC3_ZERO)
, m_bFiredAtLeastOnce(false)
, m_bFiredThisFrame(false)
, m_DimRecticle(true)
, m_pPotentialTarget(NULL)
, m_fPotentialTargetTimer(0.0f)
, m_fTargetingResetTimer(0.0f)
, m_uManualSwitchTargetTime(0)
, m_bInWater(false)
, m_bCloneCanFire(false)
, m_bCloneFiring(false)
, m_bForceShotInWeaponDirection(false)
, m_fTimeLookingBehind(-1.0f)
, m_fChargedLaunchTimer(0.0f)
, m_bWasReadyToFire(true)
, m_bFireWeaponLastFrame(false)
{
#if USE_MOUNTED_GUN_PM
	m_pmHelper.SetAssociatedTask(this);
#endif //USE_MOUNTED_GUN_PM
	if(target)
	{
		m_target = CAITarget(*target);
	}

	m_scopeModifier = 5.0f;

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON);
}

CTaskVehicleMountedWeapon::~CTaskVehicleMountedWeapon()
{
}

bool CTaskVehicleMountedWeapon::ProcessPostCamera()
{
	CPed& rPed = *GetPed();
	if (!rPed.IsNetworkClone())
	{
		ProcessVehicleWeaponControl(rPed);
	}

	return true;
}

bool CTaskVehicleMountedWeapon::ProcessMoveSignals()
{
	CPed& rPed = *GetPed();

	rPed.SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);

	if (rPed.IsNetworkClone())
	{
		ProcessVehicleWeaponControl(rPed);
	}

	return true;
}

bool CTaskVehicleMountedWeapon::ProcessPostPreRenderAfterAttachments()
{
#if __BANK
	TUNE_GROUP_BOOL(TURRET_TUNE, DISABLE_EXTRA_UPDATE_CAMERA_RELATIVE_MATRIX_AFTER_ATTACHMENT, false);
	if (DISABLE_EXTRA_UPDATE_CAMERA_RELATIVE_MATRIX_AFTER_ATTACHMENT)
		return false;
#endif // __BANK

	// We need to do this when in first person camera to update the camera-ped relative matrices used for rendering, after the ped has moved due to the seat attachment
	CPed& rPed = *GetPed();
	rPed.UpdateFpsCameraRelativeMatrix();
	return true;
}

#if !__FINAL
void CTaskVehicleMountedWeapon::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Sphere(m_targetPosition, 0.25f, Color_OrangeRed);

	const CPed* pPed = GetPed();
	const CVehicle *pVehicle = pPed->GetMyVehicle();
	if (pVehicle && pVehicle->GetVehicleWeaponMgr() && pVehicle->IsTrailerSmall2())
	{
		const CVehicleWeapon *pVehWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		if (pVehWeapon)
		{
			const CVehicleWeaponBattery* pVehWeaponBattery = (CVehicleWeaponBattery*)pVehWeapon;
			{
				if (pVehWeaponBattery && pVehWeaponBattery->GetWeaponInfo() && pVehWeaponBattery->GetWeaponInfo()->GetFireType() == FIRE_TYPE_PROJECTILE)
				{
					char szText[128];
					for (int i = 0; i < pVehWeaponBattery->GetNumWeaponsInBattery(); i++)
					{
						eHierarchyId currentMissileFlapMeshId = (eHierarchyId) ((int)FIRST_MISSILE_FLAP + i);
						int iMissileFlapBoneIndex = pVehicle->GetBoneIndex(currentMissileFlapMeshId);
						if (iMissileFlapBoneIndex == -1)
							continue;

						Matrix34 flapMat;
						pVehicle->GetGlobalMtx(iMissileFlapBoneIndex, flapMat);

						switch (pVehWeaponBattery->GetMissileFlapState(i))
						{
							case CVehicleWeaponBattery::MFS_CLOSED: formatf(szText, "MFS_CLOSED"); break;
							case CVehicleWeaponBattery::MFS_CLOSING: formatf(szText, "MFS_CLOSING"); break;
							case CVehicleWeaponBattery::MFS_OPEN: formatf(szText, "MFS_OPEN"); break;
							case CVehicleWeaponBattery::MFS_OPENING: formatf(szText, "MFS_OPENING"); break;
							default: formatf(szText, "UNKNOWN"); break;
						}
						grcDebugDraw::Text(flapMat.d, Color_blue, 0, 0, szText);
					}

					if (pVehWeaponBattery->GetMissileBatteryState() == CVehicleWeaponBattery::MBS_RELOADING)
					{
						formatf(szText, "MBS_RELOADING - C(%i), L(%i), TTN(%i)", pVehWeaponBattery->GetFiredWeaponCounter(), pVehWeaponBattery->GetLastFiredWeapon(), pVehWeaponBattery->GetTimeToNextLaunch());
						grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), Color_red, 0, 0, szText);
					}
					else
					{
						formatf(szText, "MBS_IDLE - C(%i), L(%i), TTN(%i)", pVehWeaponBattery->GetFiredWeaponCounter(), pVehWeaponBattery->GetLastFiredWeapon(), pVehWeaponBattery->GetTimeToNextLaunch());
						grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), Color_green, 0, 0, szText);
					}
				}
			}
		}
	}

#endif // DEBUG_DRAW
}

const char * CTaskVehicleMountedWeapon::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_StreamingClips : return "State_StreamingAnims";
		case State_Idle :			return "State_Idle";
		case State_Aiming :			return "State_Aiming";
		case State_Firing :			return "State_Firing";
		case State_Finish :			return "State_Finish";
		default: taskAssertf(0, "Missing State Name in CTaskVehicleMountedWeapon");
	}
	return "State_Invalid";
}
#endif // !__FINAL

CTask::FSM_Return CTaskVehicleMountedWeapon::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_StreamingClips)		
		FSM_OnUpdate
		return StreamingClips_OnUpdate(pPed);
	FSM_State(State_Idle)
		FSM_OnEnter
		Idle_OnEnter(pPed);
	FSM_OnUpdate
		return Idle_OnUpdate(pPed);
	FSM_State(State_Aiming)
		FSM_OnEnter
		Aiming_OnEnter(pPed);
	FSM_OnUpdate
		return Aiming_OnUpdate(pPed);
	FSM_State(State_Firing)
		FSM_OnEnter
		Firing_OnEnter(pPed);
	FSM_OnUpdate
		return Firing_OnUpdate(pPed);
	FSM_End
}

#if USE_MOUNTED_GUN_PM
char* CTaskVehicleMountedWeapon::ms_szPmDictName = "turreted_gun";
char* CTaskVehicleMountedWeapon::ms_szPmDataName = "turreted_gun";
s32 CTaskVehicleMountedWeapon::ms_siPmDictIndex = -1;
#endif //USE_MOUNTED_GUN_PM

CTask::FSM_Return CTaskVehicleMountedWeapon::StreamingClips_OnUpdate(CPed* pPed)
{
	Assert(pPed);

// 	// Stream the mounted weapon clips
// 	bool bClipsStreamed = m_clipSetRequestHelper.Request(CLIP_SET_VEH_WEAPON);
// 
// #if USE_MOUNTED_GUN_PM
// 	// Stream the mounted weapon PM
// 	ms_siPmDictIndex = CPMDictionaryStore::FindSlot(ms_szPmDictName);
// 	taskAssertf(ms_siPmDictIndex != -1, "(turreted_gun) mounted weapon PM missing from pm image!");
// 	bool bPmStreamed = m_pmRequestHelper.RequestPm(ms_siPmDictIndex);
// #else
// 	bool bPmStreamed = true;
// #endif //USE_MOUNTED_GUN_PM

	// If both clips and the PM are streamed, move to the idle state
	if(!pPed->IsNetworkClone() /*&& bClipsStreamed && bPmStreamed*/ )
	{
		// Need to update the target position before the cam update for the queriable state to have a valid target position
		TUNE_GROUP_BOOL(TURRET_TUNE, UPDATE_TARGET_POSITION, true);
		if (UPDATE_TARGET_POSITION)
		{
			Vector3 vTargetPosition;
			const CEntity* pTarget = NULL;
			if(!GetTargetInfo(pPed,vTargetPosition,pTarget))
			{
				// Just point straight forward from the ped
				vTargetPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() + pPed->GetTransform().GetB());
			}

			Vector3 vTargetPositionCam;
			GetTargetInfo(pPed,vTargetPositionCam,pTarget,true);

			m_targetPosition = vTargetPosition;
			m_targetPositionCam = vTargetPositionCam;
			AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "[VehicleMountedWeapon] - Ped %s setting initial target position (%.2f,%.2f,%.2f), camera position (%.2f,%.2f,%.2f)\n", AILogging::GetDynamicEntityNameSafe(pPed), m_targetPosition.x, m_targetPosition.y, m_targetPosition.z, m_targetPositionCam.x,m_targetPositionCam.y,m_targetPositionCam.z);
		}

		SetState(State_Idle);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleMountedWeapon::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pPed->GetNetworkObject() && GetState() > State_StreamingClips)
	{
		NetworkInterface::ForceSynchronisation(pPed);
	}

	if(pPed && pPed->IsNetworkClone())
	{
		m_bForceShotInWeaponDirection = false;

		// clone peds may not have cloned their vehicle on this machine when
		// the task starts, in this case the task will wait in the start state
		// until this has occurred. The peds vehicle can be removed from this machine
		// before the ped too, and we need to quit this task (without asserting) in this case
		if (!pPed->GetMyVehicle())
		{
			return FSM_Continue;
		}
		// Also wait until clone ped has a valid seat index
		if (pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed)==-1)
		{
			if (GetState()==State_StreamingClips || GetState()==State_Idle)
			{
				return FSM_Continue;
			}
		}

		//Now we are in a vehicle refresh cloned ped weapon status
		if(pPed->GetWeaponManager())
		{
			int iSelectedWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();

			CNetObjPed *netObjPed = static_cast<CNetObjPed *>(pPed->GetNetworkObject());

			if(netObjPed && iSelectedWeapon < 0)
			{  //no weapon selected yet
				u32 weapon;
				u8   weaponObjectTintIndex;
				bool weaponObjectExists;
				bool weaponObjectVisible;
				bool weaponObjectFlashLightOn;
				bool weaponObjectHasAmmo;
				bool weaponObjectAttachLeft;

				netObjPed->GetWeaponData( weapon, weaponObjectExists, weaponObjectVisible, weaponObjectTintIndex, weaponObjectFlashLightOn, weaponObjectHasAmmo, weaponObjectAttachLeft);
				netObjPed->SetWeaponData( weapon, weaponObjectExists, weaponObjectVisible, weaponObjectTintIndex, weaponObjectFlashLightOn, weaponObjectHasAmmo, weaponObjectAttachLeft);
			}
		}
	}

	if(pPed && !pPed->IsNetworkClone())
	{
		if(!CTaskVehicleMountedWeapon::IsTaskValid(pPed))
		{
			return FSM_Quit;
		}

		ProcessLockOn(pPed);
	}

	// When the submarine car changes water state, switch to next valid weapon if required
	if (pPed && pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromSubmarineCar())
	{
		const CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedVehicleWeapon() : NULL;
		if (pVehicleWeapon)
		{
			bool bIsInWater = pVehicleWeapon->IsUnderwater(pPed->GetMyVehicle());
			if (m_bInWater != bIsInWater)
			{
				CPedWeaponSelector* pSelector = pPed->GetWeaponManager()->GetWeaponSelector();
				if (!pSelector->GetIsWeaponValid(pPed, pVehicleWeapon->GetWeaponInfo(), true, true))
				{
					pSelector->SelectWeapon(pPed, SET_NEXT);
				}

				m_bInWater = bIsInWater;
			}
		}
	}

	// Moved from PostCam for clone ped so target is updated for use in main fsm update
	if (pPed->IsNetworkClone())
	{
		ProcessVehicleWeaponControl(*pPed);
	}

	return FSM_Continue;
}

void CTaskVehicleMountedWeapon::Idle_OnEnter(CPed* pPed)
{
	if(pPed->IsNetworkClone() && (!pPed->GetMyVehicle()/* || !m_clipSetRequestHelper.Request(CLIP_SET_VEH_WEAPON)*/))
	{//Clone could take longer to get a valid vehicle while remote has got this far, so wait
		return;
	}

	// HAX: don't play clip if you are the driver
	// awaiting refactor of vehicle clips associated with seats
/*	if(pPed->GetMyVehicle()->GetDriver() != pPed)
	{
#if USE_MOUNTED_GUN_PM
		// Start the turret PM if its not already active.
		if( m_pmHelper.GetClipHelper() == NULL )
			m_pmHelper.StartPm(pPed, ms_szPmDictName, ms_szPmDataName, APF_ISBLENDAUTOREMOVE, BONEMASK_ALL, AP_MEDIUM, NORMAL_BLEND_IN_DELTA );
#else
		// Start playing idle aiming clip
		CTaskRunClip* pClipTask = rage_new CTaskRunClip(CLIP_SET_VEH_WEAPON, CLIP_VEH_USE_TURRET, 10.0f, -10.0f, -1, 0);
		SetNewTask(pClipTask);
#endif //USE_MOUNTED_GUN_PM
	}
	else */if(pPed->IsAPlayerPed())	// Enable look IK if the driver is a player
	{
		// Use "EMPTY" ambient context. We don't want any ambient anims to be played. Only lookAts.
		CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(0, CONDITIONALANIMSMGR.GetConditionalAnimsGroup(CTaskVehicleFSM::emptyAmbientContext.GetHash()));
		SetNewTask( pAmbientTask );
	}
}

CTask::FSM_Return CTaskVehicleMountedWeapon::Idle_OnUpdate(CPed* pPed)
{
	Assert(pPed);
	if (!pPed)
		return FSM_Continue;

	// check to see if we should turn on search mode
	// default on in helis with search lights
	CVehicle *pVehicle = pPed->GetMyVehicle();
	if(pVehicle && m_mode == Mode_Idle && pVehicle->InheritsFromHeli())
	{
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		if(pWeaponMgr)
		{
			// Cycle through the weapons and validate them, plus check for the searchlight
			for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if(pWeapon && pWeapon->GetType() == VGT_SEARCHLIGHT)
				{
					// Once we find the searchlight we want to break out of the loop
					CSearchLight* pSearchLight = static_cast<CSearchLight*>(pWeapon);
					if ( pSearchLight && pSearchLight->GetLightOn() )
					{
						m_mode = Mode_Search;
					}
					break;
				}
			}
		}
	}

	RequestProcessMoveSignalCalls();

	if(!pPed->IsNetworkClone())
	{
		const bool bIsHeldTurret = pVehicle && pVehicle->GetSeatAnimationInfo(pPed) && pVehicle->GetSeatAnimationInfo(pPed)->IsTurretSeat();
		const bool bWantsToFire = WantsToFire(pPed);
		if( bWantsToFire && CanFireAtTarget(pPed) )
		{		
			SetState(State_Firing);		
		}
		else if( (bWantsToFire || WantsToAim(pPed)) && bIsHeldTurret )
		{		
			SetState(State_Aiming);		
		}
	}	

	return FSM_Continue;
}

void CTaskVehicleMountedWeapon::Firing_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	/*if(pPed->IsNetworkClone() && !pPed->GetMyVehicle())
	{ //Clone could take longer to get a valid vehicle while remote has got this far, so wait
		return;
	}*/

// 	// HAX: don't play clip if you are the driver
// 	// awaiting refactor of vehicle clips associated with seats
// 	if(pPed->GetMyVehicle()->GetDriver() != pPed)
// 	{
// #if USE_MOUNTED_GUN_PM
// 		// Start the turret PM if its not already active.
// 		if( m_pmHelper.GetClipHelper() == NULL )
// 			m_pmHelper.StartPm(pPed, ms_szPmDictName, ms_szPmDataName, APF_ISBLENDAUTOREMOVE, BONEMASK_ALL, AP_MEDIUM, NORMAL_BLEND_IN_DELTA );
// #else
// 		CTaskRunClip* pClipTask = rage_new CTaskRunClip(CLIP_SET_VEH_WEAPON, CLIP_VEH_USE_TURRET, 10.0f, -10.0f, -1, 0);
// 		SetNewTask(pClipTask);
// #endif //USE_MOUNTED_GUN_PM
// 	}

	// Reset the charged launch timer
	m_fChargedLaunchTimer = 0.0f;
}

CTask::FSM_Return CTaskVehicleMountedWeapon::Firing_OnUpdateClone(CPed* pPed)
{
	Assert(pPed && pPed->IsNetworkClone());

	if(!pPed->GetMyVehicle())
	{   //Clone could take longer to get a valid vehicle while remote has got this far, so wait
		return FSM_Continue;
	}

	CVehicleWeapon* pEquippedVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
	if(pEquippedVehicleWeapon)
	{
		// Can get away with just asserting on these variables because ProcessPreFSM handles the case when they are NULL
		aiFatalAssertf(pPed->GetMyVehicle(),"Unexpected NULL vehicle in CTaskUseVehicleWeapon");
		CVehicleWeaponMgr* pVehWeaponMgr = pPed->GetMyVehicle()->GetVehicleWeaponMgr();
		aiFatalAssertf(pVehWeaponMgr,"CTaskUseVehicleWeapon: This ped's vehicle has no weapon manager");

		// Get all weapons and turrets associated with our seat
		atArray<CVehicleWeapon*> weaponArray;
		atArray<CTurret*> turretArray;
		pVehWeaponMgr->GetWeaponsAndTurretsForSeat(pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed),weaponArray,turretArray);

		if (pEquippedVehicleWeapon->IsWaterCannon() && m_bCloneFiring)
		{
			CVehicleWaterCannon* pWaterCannon = (CVehicleWaterCannon*)pEquippedVehicleWeapon;

			Matrix34 matTurret;
			turretArray[0]->GetTurretMatrixWorld(matTurret,pPed->GetMyVehicle());

			pWaterCannon->FireWaterCannonFromMatrixPositionAndAlignment(pPed->GetMyVehicle(), matTurret);
		}
		else if (m_bCloneCanFire && m_bCloneFiring)
		{
			Vector3 pos(Vector3::ZeroType);
			int iSelectedWeaponIndex = pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();
			if (m_bForceShotInWeaponDirection && iSelectedWeaponIndex >= 0 && iSelectedWeaponIndex < weaponArray.size() && weaponArray[iSelectedWeaponIndex])
			{
				weaponArray[iSelectedWeaponIndex]->GetForceShotInWeaponDirectionEnd(pPed->GetMyVehicle(),pos);
				weaponDebugf3("CTaskVehicleMountedWeapon::Firing_OnUpdateClone forced shot in weapon direction - position[%f %f %f]",pos.x,pos.y,pos.z);
			}
			else
			{
				m_target.GetPosition(pos);
				if (GetHeldTurretIfShouldSyncLocalDirection())
				{
					Vector3 vTargetPosition;
					const CEntity* pTarget = NULL;
					if(!GetTargetInfo(pPed,vTargetPosition,pTarget))
					{
						vTargetPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() + pPed->GetTransform().GetB());
					}
					pos = vTargetPosition;
				}
				weaponDebugf3("CTaskVehicleMountedWeapon::Firing_OnUpdateClone mode[%d] entity[%p] position[%f %f %f]",m_target.GetMode(),m_target.GetEntity(),pos.x,pos.y,pos.z);
			}
			pEquippedVehicleWeapon->Fire(pPed,pPed->GetMyVehicle(),pos,m_target.GetEntity()); //fire the remote weapon - otherwise it will not actuate on the remote (no animations) - no animations of firing for tank / no firing of machine guns on buzzard - lavalley	
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleMountedWeapon::Firing_OnUpdate(CPed* pPed)
{
	weaponDebugf3("CTaskVehicleMountWeapon::Firing_OnUpdate");

	RequestProcessMoveSignalCalls();

	if( !WantsToFire(pPed) || !CanFireAtTarget(pPed) )
	{		
		if(WantsToAim(pPed))
		{
			SetState(State_Aiming);	
		}
		else
		{		
			weaponDebugf3("CTaskVehicleMountWeapon::Firing_OnUpdate--!WantsToFire-->State_Idle");
			SetState(State_Idle);
		}

		return FSM_Continue;
	}

	ProcessIgnoreLookBehindForLocalPlayer(pPed);
	return FSM_Continue;
}

void CTaskVehicleMountedWeapon::Aiming_OnEnter(CPed* UNUSED_PARAM(pPed))
{
}

void CTaskVehicleMountedWeapon::CheckForWeaponSpin(CPed* pPed)
{
	// Spin up if required while aiming
	const CVehicle* pVehicle = pPed->GetMyVehicle();
	if (pVehicle) 
	{
		const CVehicleWeaponMgr* pVehWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		const s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

		if (pVehWeaponMgr && pVehicle->IsSeatIndexValid(iSeatIndex))
		{
			CFixedVehicleWeapon* pVehicleWeapon = pVehWeaponMgr->GetFirstFixedWeaponForSeat(iSeatIndex);
			if (pVehicleWeapon && pVehicleWeapon->GetWeapon())
			{
				pVehicleWeapon->SpinUp(pPed);
			}
		}
	}

}

CTask::FSM_Return CTaskVehicleMountedWeapon::Aiming_OnUpdateClone(CPed* pPed)
{	
	if (pPed)
	{
		CheckForWeaponSpin(pPed);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleMountedWeapon::Aiming_OnUpdate(CPed* pPed)
{
	CheckForWeaponSpin(pPed);
	RequestProcessMoveSignalCalls();

	const CVehicle* pVehicle = pPed->GetMyVehicle();
	const bool bIsHeldTurret = pVehicle && pVehicle->GetSeatAnimationInfo(pPed) && pVehicle->GetSeatAnimationInfo(pPed)->IsTurretSeat();
	const bool bWantsToFire = WantsToFire(pPed);
	if(bWantsToFire && CanFireAtTarget(pPed))
	{		
		SetState(State_Firing);		
	} 
	else if(!WantsToAim(pPed) && (!bIsHeldTurret || !bWantsToFire))
	{
		SetState(State_Idle);
	}

	ProcessIgnoreLookBehindForLocalPlayer(pPed);
	return FSM_Continue;
}

CSearchLight* HelperGetVehicleSearchlight(CPed& in_Ped)
{
	// Here is where we check for a spotlight, which would mean we should switch to aim on a valid target later on.
	CVehicle *pVehicle = in_Ped.GetMyVehicle();
	if ( pVehicle )
	{
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		if(pWeaponMgr)
		{
			for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if(pWeapon && pWeapon->GetType() == VGT_SEARCHLIGHT)
				{
					return static_cast<CSearchLight*>(pWeapon);
				}
			}
		}
	}

	return NULL;
}

void CTaskVehicleMountedWeapon::ProcessIgnoreLookBehindForLocalPlayer(CPed* pPed)
{
	if (pPed->IsLocalPlayer() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))		
	{
		camThirdPersonPedAimCamera* thirdPersonPedAimCamera = static_cast<camThirdPersonPedAimCamera*>(camInterface::GetGameplayDirector().GetThirdPersonAimCamera());
		camControlHelper* controlHelper = thirdPersonPedAimCamera ? thirdPersonPedAimCamera->GetControlHelper() : NULL;
		if (controlHelper)
		{
			controlHelper->IgnoreLookBehindInputThisUpdate();
		}
	}
}

void CTaskVehicleMountedWeapon::ControlVehicleWeapons(CPed* pPed, bool bFireWeapons)
{
#if !__NO_OUTPUT
	if ((Channel_weapon.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_weapon.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{
		if (bFireWeapons)
			weaponDebugf3("CTaskVehicleMountWeapon::ControlVehicleWeapons--bFireWeapons[1]");
	}
#endif

	aiFatalAssertf(pPed,"Unexpected NULL ped in CTaskVehicleMountedWeapon");

	if ( m_mode == Mode_Search)
	{
		CSearchLight* pSearchLight = HelperGetVehicleSearchlight(*pPed);
		if ( pSearchLight )
		{
			static float s_RadiansPerSecond = 30.0f;
			float deltaRadians = s_RadiansPerSecond * fwTimer::GetTimeStep();

			float SweepTheta = pSearchLight->GetSweepTheta() + deltaRadians;
			while(SweepTheta > 360.0f )
			{
				SweepTheta -= 360.0f;
			}
			pSearchLight->SetSweepTheta(SweepTheta);
		}
	}

	Vector3 vTargetPosition;
	Vector3 vSyncPosition;
	const CEntity* pTarget = NULL;
	if(!GetTargetInfo(pPed,vTargetPosition,pTarget,true))
	{
		// We don't have a valid target position anymore
		bFireWeapons = false;

		// Just point straight forward from the ped
		vTargetPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() + pPed->GetTransform().GetB());
		vSyncPosition = vTargetPosition;
	}
	else
	{
		GetTargetInfo(pPed,vSyncPosition,pTarget);
	}

	// B*2254696: Valkyrie Passenger Turret: Don't rotate the turret if script have blocked us from firing (ie not in the heli gun cam), just aim straight ahead.
	CVehicleWeapon* pEquippedVehicleWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedVehicleWeapon() : NULL;
	const CWeaponInfo* pWeaponInfo = pEquippedVehicleWeapon ? pEquippedVehicleWeapon->GetWeaponInfo() : NULL;
	if (pPed->IsLocalPlayer() && pPed->GetControlFromPlayer() && pPed->GetControlFromPlayer()->IsInputDisabled(INPUT_ATTACK) 
		&& pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("VEHICLE_WEAPON_NOSE_TURRET_VALKYRIE", 0x4170e491) && pPed->GetVehiclePedInside())
	{
		static dev_float fDist = 20.0f;
		Vector3 vDir = VEC3V_TO_VECTOR3(pPed->GetVehiclePedInside()->GetTransform().GetB());
		vDir.Scale(fDist);
		vTargetPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vDir;
		vSyncPosition = vTargetPosition;
	}

	if (!pPed->IsNetworkClone() || !GetHeldTurretIfShouldSyncLocalDirection())
	{
#if __BANK
		if (vSyncPosition.IsClose(VEC3_ZERO, SMALL_FLOAT))
		{
			AI_LOG_WITH_ARGS("Ped %s is trying to set a target position close to the origin. vehicle - %s [%s]", AILogging::GetDynamicEntityNameSafe(pPed), pPed->GetMyVehicle() ? pPed->GetMyVehicle()->GetModelName() : "NULL", pPed->GetMyVehicle() ? AILogging::GetDynamicEntityNameSafe(pPed->GetMyVehicle()) : "");
		}

		if (vTargetPosition.IsClose(VEC3_ZERO, SMALL_FLOAT))
		{
			AI_LOG_WITH_ARGS("Ped %s is trying to set a camera target position close to the origin. mode - %i, vehicle - %s [%s]", AILogging::GetDynamicEntityNameSafe(pPed), m_mode, pPed->GetMyVehicle() ? pPed->GetMyVehicle()->GetModelName() : "NULL", pPed->GetMyVehicle() ? AILogging::GetDynamicEntityNameSafe(pPed->GetMyVehicle()) : "");
		}
#endif // __BANK
		m_targetPosition = vSyncPosition;
		m_targetPositionCam = vTargetPosition;
	}

	// Can get away with just asserting on these variables because ProcessPreFSM handles the case when they are NULL
	CVehicle* pVehicle = pPed->GetMyVehicle();
	aiFatalAssertf(pVehicle,"Unexpected NULL vehicle in CTaskUseVehicleWeapon");
	CVehicleWeaponMgr* pVehWeaponMgr = pVehicle->GetVehicleWeaponMgr();
	aiFatalAssertf(pVehWeaponMgr,"CTaskUseVehicleWeapon: This ped's vehicle has no weapon manager");

	// Get all weapons and turrets associated with our seat
	atArray<CVehicleWeapon*> weaponArray;
	atArray<CTurret*> turretArray;
	pVehWeaponMgr->GetWeaponsAndTurretsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed),weaponArray,turretArray);

	const bool bIsHeldTurret = pVehicle->GetSeatAnimationInfo(pPed) && pVehicle->GetSeatAnimationInfo(pPed)->IsTurretSeat();

	// See how close we are to target

	int numTurrets = turretArray.size(); 
	bool bAiMpPed = NetworkInterface::IsGameInProgress() && !pPed->IsAPlayerPed();
	float fSmallestAngleToTarget = bAiMpPed ? 0.f : ( numTurrets==0 ? 0.f : PI);
	bool bIsWithinFiringAngularThreshold = true;
	Matrix34 matTurret;
	for(int iTurretIndex = 0; iTurretIndex < numTurrets; iTurretIndex++)
	{
		//skip processing if the turretArray element is NULL
		CTurret* pTurret = turretArray[iTurretIndex];
		if (!pTurret)
			continue;

		//skip processing if this turret is not our equipped weapon
		s32 sTurretWeaponIndex = pVehWeaponMgr->GetWeaponIndexForTurret(*pTurret);
		if (pEquippedVehicleWeapon && pVehWeaponMgr && sTurretWeaponIndex != -1 && pVehWeaponMgr->GetIndexOfVehicleWeapon(pEquippedVehicleWeapon) != sTurretWeaponIndex)
			continue;

		bool bIsBombushka = MI_PLANE_BOMBUSHKA.IsValid() && MI_PLANE_BOMBUSHKA == pVehicle->GetModelIndex();
		bool bIsVolatol = MI_PLANE_VOLATOL.IsValid() && MI_PLANE_VOLATOL == pVehicle->GetModelIndex();
		if (bIsBombushka || bIsVolatol)
		{
			CVehicleWeapon* pVehicleWeapon = sTurretWeaponIndex != -1 ? pVehWeaponMgr->GetVehicleWeapon(sTurretWeaponIndex) : NULL;
			if (pVehicleWeapon)
			{
				int iAmmo = pVehicle->GetRestrictedAmmoCount(pVehicleWeapon->m_handlingIndex);
				if (iAmmo == 0)
				{
					continue;
				}
			}
		}

		if (bAiMpPed)
			fSmallestAngleToTarget = PI;

		bool bIsFixedTampa3Turret = MI_CAR_TAMPA3.IsValid() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetModelIndex() == MI_CAR_TAMPA3 && pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("VEHICLE_WEAPON_TAMPA_FIXEDMINIGUN",0xDAC57AAD);

		//skip turret processing on the AVENGER and TERBYTE, script should always drive the turrets on these weapons
		bool bSkipTurretProcessing = (MI_PLANE_AVENGER.IsValid() && pVehicle->GetModelIndex() == MI_PLANE_AVENGER) ||
									 (MI_CAR_TERBYTE.IsValid() && pVehicle->GetModelIndex() == MI_CAR_TERBYTE);
		if (!bSkipTurretProcessing)
		{
			if (!bIsFixedTampa3Turret)
				pTurret->AimAtWorldPos(vTargetPosition,pPed->GetMyVehicle(), m_mode==Mode_Player, m_mode==Mode_Camera || (!bIsHeldTurret && pVehicle->InheritsFromHeli()));

			if( turretArray[ iTurretIndex ]->IsPhysicalTurret() )
			{
				CTurretPhysical *pPhyscialTurret = (CTurretPhysical *)turretArray[ iTurretIndex ];
				pPhyscialTurret->SetPedHasControl( true );
			}
		}

		// Calc angle to target
		// If it is too big we wont fire
		float fAngleToTarget;
		pTurret->GetTurretMatrixWorld(matTurret,pPed->GetMyVehicle());
		Vector3 vToTarget = vTargetPosition - matTurret.d;

		fAngleToTarget = matTurret.b.Angle(vToTarget);
		if(fAngleToTarget < fSmallestAngleToTarget)
			fSmallestAngleToTarget = fAngleToTarget;

		TUNE_GROUP_FLOAT(TURRET_TUNE, fAngularThresholdDefaultTurret, 14.9f, 0.0f, 90.0f, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, fAngularThresholdHeldTurret, 30.0f, 0.0f, 90.0f, 0.01f);

		TUNE_GROUP_FLOAT(TURRET_TUNE, fAngularThresholdDuneTurret, 11.5f, 0.0f, 90.0f, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, fAngularThresholdChernoTurret, 7.5f, 0.0f, 90.0f, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, fAngularThresholdBombushkaTurret, 30.0f, 0.0f, 90.0f, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, fAngularThresholdKhanjaliTopTurret, 30.0f, 0.0f, 90.0f, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, fAngularThresholdCerberusTurret, 90.0f, 0.0f, 90.0f, 0.01f); // No limits!

		float fAngularThreshold = fAngularThresholdDefaultTurret * DtoR;
		u32 uVehModelIndex = pPed->GetMyVehicle() ? pPed->GetMyVehicle()->GetModelIndex() : 0;

		if (MI_TANK_KHANJALI.IsValid() && uVehModelIndex == MI_TANK_KHANJALI && pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed) > 0)
		{
			fAngularThreshold = fAngularThresholdKhanjaliTopTurret * DtoR;
		}
		else if (MI_CAR_CHERNOBOG.IsValid() && uVehModelIndex == MI_CAR_CHERNOBOG)
		{
			fAngularThreshold = fAngularThresholdChernoTurret * DtoR;
		}
		else if (MI_CAR_DUNE3.IsValid() && uVehModelIndex == MI_CAR_DUNE3)
		{
			fAngularThreshold = fAngularThresholdDuneTurret * DtoR;
		}
		else if ((MI_PLANE_BOMBUSHKA.IsValid() && uVehModelIndex == MI_PLANE_BOMBUSHKA) || (MI_PLANE_VOLATOL.IsValid() && uVehModelIndex == MI_PLANE_VOLATOL))
		{
			fAngularThreshold = fAngularThresholdBombushkaTurret * DtoR;
		}
		else if ((MI_TRUCK_CERBERUS.IsValid() && uVehModelIndex == MI_TRUCK_CERBERUS) ||
				 (MI_TRUCK_CERBERUS2.IsValid() && uVehModelIndex == MI_TRUCK_CERBERUS2) || 
				 (MI_TRUCK_CERBERUS3.IsValid() && uVehModelIndex == MI_TRUCK_CERBERUS3) )
		{
			fAngularThreshold = fAngularThresholdCerberusTurret * DtoR;
		}
		else if (bIsHeldTurret)
		{
			fAngularThreshold = fAngularThresholdHeldTurret * DtoR;
		}

		bIsWithinFiringAngularThreshold &= (bIsFixedTampa3Turret || pTurret->IsWithinFiringAngularThreshold( fAngularThreshold ));

		if (bIsHeldTurret && pPed->IsLocalPlayer())
		{
			TUNE_GROUP_BOOL(TURRET_TUNE, DO_CAM_CHECK_FOR_RETICLE, true);
			if (DO_CAM_CHECK_FOR_RETICLE)
			{
				const camThirdPersonPedAimCamera* thirdPersonPedAimCamera = static_cast<const camThirdPersonPedAimCamera*>(camInterface::GetGameplayDirector().GetThirdPersonAimCamera());
				if (thirdPersonPedAimCamera)
				{
					Vector3 vCamFront = thirdPersonPedAimCamera->GetFrame().GetFront();
					if(camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
					{
						vCamFront = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera()->GetFrame().GetFront();
					}

					const float fDot = vCamFront.Dot(matTurret.b);
					if (fDot < 0.0f)
					{
						bIsWithinFiringAngularThreshold = false;
					}
				}
			}

			const CControl* pControl = pPed->GetControlFromPlayer();
			if (!IsAimingOrFiring() && pControl && pControl->GetVehicleLookBehind().IsDown())
			{
				if (m_fTimeLookingBehind < 0.0f)
				{
					m_fTimeLookingBehind = 0.0f;
				}
				m_fTimeLookingBehind += fwTimer::GetTimeStep();
			}
			else
			{
				m_fTimeLookingBehind = -1.0f;
			}	
		}

		
		if (pEquippedVehicleWeapon && pWeaponInfo && pWeaponInfo->GetShouldPerformFrontClearTest())
		{
			if (pVehicle->InheritsFromTrailer())
			{
				CTrailer* pTrailer = static_cast<CTrailer*>(pVehicle);
				const CVehicle* pAttachedToVehicle = pTrailer->GetAttachedToParent();
				if (!pAttachedToVehicle)
				{
					CVehicleWeaponBattery* pVehWeaponBattery = (CVehicleWeaponBattery*)pEquippedVehicleWeapon;
					if (pVehWeaponBattery)
					{
						// Renable all turrets if we're detached
						const u32 uNumWeapons = pVehWeaponBattery->GetNumWeaponsInBattery();

						for (u32 i=0; i< uNumWeapons; i++)
						{
							CFixedVehicleWeapon* pFixedVehWeapon = (CFixedVehicleWeapon*)pVehWeaponBattery->GetVehicleWeapon(i);
							if (pFixedVehWeapon)
							{
								pFixedVehWeapon->SetEnabled(NULL, true);
							}
						}
					}
				}
				else
				{
					TUNE_GROUP_BOOL(TURRET_TUNE, bRenderFrontClearShapetest, false);
					const camThirdPersonPedAimCamera* thirdPersonPedAimCamera = static_cast<const camThirdPersonPedAimCamera*>(camInterface::GetGameplayDirector().GetThirdPersonAimCamera());
					if (thirdPersonPedAimCamera)
					{
						Vector3 vCamFront = thirdPersonPedAimCamera->GetFrame().GetFront();
						Vector3 vCamPosition = thirdPersonPedAimCamera->GetFrame().GetPosition();
						if(camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
						{
							vCamFront = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera()->GetFrame().GetFront();
							vCamPosition = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera()->GetFrame().GetPosition();
						}

						Vector3 vStart = vCamPosition;
						// Assumes reticle is in the centre of the screen and 20m is sufficient
						Vector3 vEnd = vStart + vCamFront * 20.0f;

						WorldProbe::CShapeTestProbeDesc probeDesc;
						WorldProbe::CShapeTestSingleResult probeResults;
						probeDesc.SetResultsStructure(&probeResults);
						probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
						probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);	
						probeDesc.SetStartAndEnd(vStart, vEnd);

						// Only look for collision with attached vehicle for trailer turrets
						probeDesc.SetIncludeEntity(pAttachedToVehicle);
						probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);

						if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
						{
							bIsWithinFiringAngularThreshold = false;
						}	
#if __BANK
						if (bRenderFrontClearShapetest)
						{
							grcDebugDraw::Line(VECTOR3_TO_VEC3V(vStart),VECTOR3_TO_VEC3V(vEnd), bIsWithinFiringAngularThreshold ? Color_green : Color_red, -1);
						}
#endif
					}

					// Skip all the weapons in the battery if the reticle test fails, we don't want to fire, so no point doing extra work
					CVehicleWeaponBattery* pVehWeaponBattery = (CVehicleWeaponBattery*)pEquippedVehicleWeapon;
					if (pVehWeaponBattery && bIsWithinFiringAngularThreshold)
					{
						bool aHits[MAX_NUM_BATTERY_WEAPONS];
						const u32 uNumWeapons = pVehWeaponBattery->GetNumWeaponsInBattery();

						for (u32 i=0; i< uNumWeapons; i++)
						{
							CFixedVehicleWeapon* pFixedVehWeapon = (CFixedVehicleWeapon*)pVehWeaponBattery->GetVehicleWeapon(i);
							if (pFixedVehWeapon)
							{
								eHierarchyId eBoneId = pFixedVehWeapon->GetWeaponBone();
								s32 nWeaponBoneIndex = pVehicle->GetBoneIndex(eBoneId);
								if (nWeaponBoneIndex >= 0)
								{
									Matrix34 testMat;
									pVehicle->GetGlobalMtx(nWeaponBoneIndex, testMat);

									float fStartFwdOffset = pWeaponInfo->GetFrontClearForwardOffset();
									float fStartVertOffset = pWeaponInfo->GetFrontClearVerticalOffset();
									float fHorizontalOffset = pWeaponInfo->GetFrontClearHorizontalOffset();
									float fCapsuleLength = pWeaponInfo->GetFrontClearCapsuleLength();
									float fCapsuleRadius = pWeaponInfo->GetFrontClearCapsuleRadius();
									Vector3 vGunFwd = testMat.b;
									Vector3 vGunUp = testMat.c;
									Vector3 vGunRight = testMat.a;
									vGunFwd.NormalizeFast();
									vGunUp.NormalizeFast();
									vGunRight.NormalizeFast();

									Vector3 vStart = testMat.d + vGunFwd * fStartFwdOffset;
									vStart = vStart + vGunUp * fStartVertOffset;
									vStart = vStart + vGunRight * fHorizontalOffset;
									Vector3 vEnd = vStart + vGunFwd * fCapsuleLength;

									WorldProbe::CShapeTestCapsuleDesc probeDesc;
									WorldProbe::CShapeTestSingleResult probeResults;
									probeDesc.SetResultsStructure(&probeResults);
									probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
									probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
									probeDesc.SetIsDirected(false);
									probeDesc.SetCapsule(vStart, vEnd, fCapsuleRadius);

									// Only look for collision with attached vehicle for trailer turrets
									probeDesc.SetIncludeEntity(pAttachedToVehicle);
									probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);

#if __BANK
									Vector3 vHitPos;
#endif

									if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
									{
#if __BANK
										vHitPos = probeResults[0].GetHitPosition();
#endif

										aHits[i] = true;
									}
									else
									{
										aHits[i] = false;
									}

#if __BANK
									TUNE_GROUP_FLOAT(TURRET_TUNE, fFrontClearHitPosSphereRadius, 0.1f, 0.0f, 100.0f, 0.1f);
									if (bRenderFrontClearShapetest)
									{
										static u32 uTimeToLive = 1;
										grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vStart),VECTOR3_TO_VEC3V(vEnd),fCapsuleRadius, aHits[i] ? Color_red : Color_green, false, uTimeToLive);

										if (aHits[i])
										{
											grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vHitPos),fFrontClearHitPosSphereRadius,Color_orange,true,uTimeToLive);
									}
									}
#endif

									pFixedVehWeapon->SetEnabled(pVehicle, !aHits[i]);
								}
							}
						}

						bool bAllWeaponsBlocked = true;
						for (u32 i=0; i< uNumWeapons; i++)
						{
							if (!aHits[i])
							{
								bAllWeaponsBlocked = false;
							}
						}

						if (bAllWeaponsBlocked)
						{
							bIsWithinFiringAngularThreshold = false;
						}
					}
				}
			}
		}

#if USE_MOUNTED_GUN_PM
		// Update the turret firing PM phase (heading) and parmeter (pitch)
		CClipPlayer* pPmPlayer = m_pmHelper.GetClipHelper();
		if( pPmPlayer )
		{
			// PITCH ------------------------------------------------------------------
			{
				// PM parameter 0 controls the pitch in radians
				pPmPlayer->SetParameter(0, pTurret->GetAimPitch());
			}

			// HEADING ----------------------------------------------------------------
			{
				// The phase controls the heading, so firstly stop the clip from updating itself
				pPmPlayer->UnsetFlag(APF_ISPLAYING);

				// Work out the phase relative to the overall clip range
				static dev_float PM_AnimationRange = HALF_PI;
				// Calculate the phase relative to the clip range
				float fDesiredPhase = pTurret->GetAimHeading()/PM_AnimationRange;
				// Limit the phase to between 0 and 1
				fDesiredPhase = 1.0f-rage::Clamp(fDesiredPhase+0.5f, 0.0f, 1.0f);
				pPmPlayer->SetPhase(fDesiredPhase);
			}
			// ------------------------------------------------------------------------
		}
#endif //USE_MOUNTED_GUN_PM
	}

	m_DimRecticle = (0 < numTurrets) ? (!bIsWithinFiringAngularThreshold) : false;

    if(pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
    {
        CHeli *pHeli = static_cast<CHeli*>(pVehicle);
        pHeli->SetHoverModeDesiredTarget(vTargetPosition);
    }

	//Ensure the camera system is ready for this ped to fire. Firing is blocked if this ped is the local player and we're interpolating into a vehicle
	//aim camera for their vehicle, as we want the aiming reticule to settle before firing begins.
	camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
	bool shouldBlockFiringForCamera = pPed->IsLocalPlayer() && gameplayDirector.IsAiming(pPed->GetMyVehicle()) && gameplayDirector.IsCameraInterpolating();

	if (bIsHeldTurret && m_DimRecticle)
	{
		bFireWeapons = false;
	}

	aiFatalAssertf(pPed->GetWeaponManager(), "GetWeaponManager NULL cannot deref and GetEquippedVehicleWeaponIndex fail");
	int iSelectedWeaponIndex = pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();
	if(shouldBlockFiringForCamera || (fSmallestAngleToTarget > m_fFireTolerance) || iSelectedWeaponIndex < 0) //possible to have no weapon selected if script forces it out of hand
	{
		bFireWeapons = false;
	}

#if ENABLE_MOTION_IN_TURRET_TASK
	if(pVehicle->GetSeatAnimationInfo(pPed) && bIsHeldTurret && bFireWeapons)
	{
		const CTaskMotionInTurret* pMotionInTurretTask = static_cast<const CTaskMotionInTurret*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_TURRET));
		bFireWeapons = pMotionInTurretTask ? pMotionInTurretTask->GetCanFire() : false;
	}
#endif // ENABLE_MOTION_IN_TURRET_TASK

	if(m_mode == Mode_Player)
	{
		CVehicleWeapon* pEquippedVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		const CWeaponInfo* pWeaponInfo = pEquippedVehicleWeapon ? pEquippedVehicleWeapon->GetWeaponInfo() : NULL;

		if(pWeaponInfo && pWeaponInfo->IsAssistedAimVehicleWeapon() && !pTarget)
		{
			Vector3 vWeaponOverrideDirection(Vector3::ZeroType);
			if(EvaluateTerrainForFireDirection(vWeaponOverrideDirection))
			{
				aiFatalAssertf(iSelectedWeaponIndex >= 0 && iSelectedWeaponIndex < weaponArray.size() && weaponArray[iSelectedWeaponIndex], "Selected Vehicle Weapon Index [%d] is out of range 1. Vehicle[%s]", iSelectedWeaponIndex, pVehicle->GetDebugName());
				weaponArray[iSelectedWeaponIndex]->SetOverrideWeaponDirection(vWeaponOverrideDirection);
			}
		}
	}

	// B*1910978: Non-turret weapons: Check if angle to target is valid. If angle is too big, don't fire.
	// Just done for non-turret weapons and planes/AI peds so we don't break anything else.
	bool bFireAngleTooLarge = false;
	bool bValidVehicleForAngleCheck = (numTurrets == 0 && pVehicle->InheritsFromPlane()) || pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_ForceCheckAttackAngleForMountedGuns);
	if (bValidVehicleForAngleCheck && !pPed->IsPlayer())
	{
		CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();

		CVehicleWeapon* pEquippedVehicleWeapon = pWeaponManager->GetEquippedVehicleWeapon();
		if (pEquippedVehicleWeapon && pEquippedVehicleWeapon->GetWeaponInfo() && pVehicle->GetBaseModelInfo())
		{
			Matrix34 matWeapon;
			s32 nWeaponBoneIndex = pEquippedVehicleWeapon->GetWeaponInfo()->GetGunFeedBone().GetBoneIndex(pVehicle->GetBaseModelInfo());
			pVehicle->GetGlobalMtx(nWeaponBoneIndex, matWeapon);

			if (NetworkInterface::IsGameInProgress())
			{
				CFixedVehicleWeapon* pFixedVehWeapon = nullptr;
				if (pEquippedVehicleWeapon->GetType() == VGT_FIXED_VEHICLE_WEAPON)
				{
					pFixedVehWeapon = (CFixedVehicleWeapon*)pEquippedVehicleWeapon;
				}
				else if (pEquippedVehicleWeapon->GetType() == VGT_VEHICLE_WEAPON_BATTERY)
				{
					CVehicleWeaponBattery* pVehWeaponBattery = (CVehicleWeaponBattery*)pEquippedVehicleWeapon;
					if (pVehWeaponBattery)
					{
						//Grab the first fixed weapon in the battery
						pFixedVehWeapon = (CFixedVehicleWeapon*)pVehWeaponBattery->GetVehicleWeapon(0);
					}
				}

				if (pFixedVehWeapon)
				{
					eHierarchyId eBoneId = pFixedVehWeapon->GetWeaponBone();

					CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();
					if (pVehicleModelInfo->GetBoneIndex(eBoneId) != VEH_INVALID_ID)
					{
						pVehicle->GetGlobalMtx(pVehicleModelInfo->GetBoneIndex(eBoneId), matWeapon);
					}
				}
			}

			float fAngleToTarget;
			Vector3 vToTarget = vTargetPosition - matWeapon.d;
			fAngleToTarget = matWeapon.b.Angle(vToTarget);

			float sfAngularThreshold = pEquippedVehicleWeapon->GetWeaponInfo()->GetVehicleAttackAngle() * DtoR;
			if (fAngleToTarget > sfAngularThreshold)
			{
				bFireAngleTooLarge = true;
			}
		}
	}

	//MP - ensure that the m_targetPosition is updated properly - otherwise the remote will shoot outside of bounds -- fix url:bugstar:2098116 -- Remote player's bullets from side Valkyrie gun are out of sync.
	// Changed this so we alter the only the bullet position for the clone, as changing the synced target position causes stuttering B*2107522
	if (NetworkInterface::IsGameInProgress() && pPed->IsNetworkClone() && !bIsWithinFiringAngularThreshold)
	{
		m_bForceShotInWeaponDirection = true;
	}

	//check if the vehicle is able to fire weapons
	bFireWeapons &= CTaskVehicleAttack::IsAbleToFireWeapons(*pVehicle, pPed);

	// Allow the player to hold fire to charge a launches shot, and fire on release
	if (pWeaponInfo && pWeaponInfo->GetIsProjectile() && pWeaponInfo->GetIsVehicleChargedLaunch())
	{
		const CAmmoProjectileInfo* pAmmoInfo = static_cast<const CAmmoProjectileInfo*>(pWeaponInfo->GetAmmoInfo());
		if (pAmmoInfo && pAmmoInfo->GetChargedLaunchTime() > 0.0f)
		{
			if (bFireWeapons) // Fire State
			{
				// Charging, so increase timer and block fire
				m_fChargedLaunchTimer += fwTimer::GetTimeStep();
				bFireWeapons = false;
			}
			else if (m_fChargedLaunchTimer > 0.0f) // Idle / Aiming State
			{
				// We may have left the fire state because our camera-to-turret diff is too great, cancel the timer and don't fire
				if (!bIsWithinFiringAngularThreshold)
				{
					m_fChargedLaunchTimer = 0.0f;
					bFireWeapons = false;
				}
				else
				{
					// Was previously charging in Fire state, so calculate launch speed, fire and reset timer						
					m_fChargedLaunchTimer = Clamp(m_fChargedLaunchTimer, 0.0f, pAmmoInfo->GetChargedLaunchTime());
					float fLaunchSpeedMult = Lerp(m_fChargedLaunchTimer / pAmmoInfo->GetChargedLaunchTime(), 1.0f, pAmmoInfo->GetChargedLaunchSpeedMult());
					aiFatalAssertf(iSelectedWeaponIndex >= 0 && iSelectedWeaponIndex < weaponArray.size() && weaponArray[iSelectedWeaponIndex], "Selected Vehicle Weapon Index [%d] is out of range 2. Vehicle[%s]", iSelectedWeaponIndex, pVehicle->GetDebugName());
					weaponArray[iSelectedWeaponIndex]->SetOverrideLaunchSpeed(pAmmoInfo->GetLaunchSpeed() * fLaunchSpeedMult);

					m_fChargedLaunchTimer = 0.0f;
					bFireWeapons = true;	

					// TODO: Get this 'firing' properly (weapon animations, muzzle fx, not actually firing) on clone machines, which comes from Firing_OnUpdateClone
				}
			}

			// Process audio / vfx for charged shot
			if (m_fChargedLaunchTimer > 0.0f)
			{
				float fChargePhase = Clamp(m_fChargedLaunchTimer / pAmmoInfo->GetChargedLaunchTime(), 0.0f, 1.0f);

				s32 boneIndex = -1;
				if (weaponArray[iSelectedWeaponIndex]->GetType() == VGT_FIXED_VEHICLE_WEAPON)
				{
					eHierarchyId weaponBone = static_cast<CFixedVehicleWeapon*>(weaponArray[iSelectedWeaponIndex])->GetWeaponBone();
					boneIndex = pVehicle->GetBoneIndex(weaponBone);
					if (boneIndex>-1)
					{
						g_vfxVehicle.UpdatePtFxVehicleWeaponCharge(pVehicle, boneIndex, fChargePhase);
						pVehicle->GetVehicleAudioEntity()->RequestWeaponChargeSfx(boneIndex, fChargePhase);

#if GTA_REPLAY
						if(CReplayMgr::ShouldRecord())
						{
							CReplayMgr::RecordFx<CPacketVehicleWeaponCharge>(CPacketVehicleWeaponCharge(weaponBone, fChargePhase), pVehicle);
						}
#endif
					}
				}
			}
		}
	}

	CFiringPattern& firingPattern = pPed->GetPedIntelligence()->GetFiringPattern();
	const bool readyToFire = firingPattern.ReadyToFire();

    if (pPed->IsNetworkClone())
    {
        m_bCloneCanFire = (bFireWeapons && !bFireAngleTooLarge) ? true : false;
    }
    else
    {
        if (readyToFire)
        {
            if (!m_bWasReadyToFire)
            {
                g_WeaponAudioEntity.TriggerWeaponReloadConfirmation(matTurret.d, pVehicle);
            }
        }
        else
        {
            if (bFireWeapons && !m_bFireWeaponLastFrame)
            {
                g_WeaponAudioEntity.TriggerNotReadyToFireSound(matTurret.d, pVehicle);
            }
        }
    }

	m_bWasReadyToFire = readyToFire;
	m_bFireWeaponLastFrame = bFireWeapons;

	if(bFireWeapons && !bFireAngleTooLarge)
	{	
		const bool wantsToFire = firingPattern.WantsToFire();
		if( readyToFire && wantsToFire )
		{
			//Force disable timeslicing on the vehicle if we're firing a volumetric weapon. This needs to be called every frame so looped vfx can process correctly.
			if (pVehicle && pWeaponInfo && pWeaponInfo->GetFireType() == FIRE_TYPE_VOLUMETRIC_PARTICLE)
			{
				pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing = false;
				pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing = false;
				pVehicle->m_nVehicleFlags.bLodForceUpdateThisTimeslice = true;
			}

			//Only ai need this, not player
			if(m_mode == Mode_Player)
			{
				vTargetPosition.Zero();
			}

			// Converting from fatal assert to verify for now until I can figure out what script are screwing up - B*3958863/4004542/5748872
			if(iSelectedWeaponIndex >= 0 && iSelectedWeaponIndex < weaponArray.GetCount())
			{
				if (weaponArray[iSelectedWeaponIndex])
				{
					weaponDebugf3("CTaskVehicleMountedWeapon::ControlVehicleWeapons-->initiate weaponArray[i] Fire");
					weaponArray[iSelectedWeaponIndex]->Fire(pPed, pPed->GetMyVehicle(), vTargetPosition, pTarget, !bIsWithinFiringAngularThreshold);
					m_bFiredAtLeastOnce = true;
					m_bFiredThisFrame = true;
				}
				else
				{
					aiAssertf(0, "ControlVehicleWeapons - Selected weapon index [%d] on %s contains null CVehicleWeapon pointer", iSelectedWeaponIndex, pVehicle->GetDebugName());
					for (int i = 0; i < weaponArray.GetCount(); i++)
					{
						aiDisplayf("ControlVehicleWeapons - Weapon Index %i - 0x%p - %s", i, weaponArray[i], weaponArray[i] ? weaponArray[i]->GetName() : "NULL");
					}
				}
			}
			else
			{
				aiAssertf(0, "ControlVehicleWeapons - Selected weapon index [%d] on %s is out of range (weaponArray size = %d)", iSelectedWeaponIndex, pVehicle->GetDebugName(), weaponArray.GetCount());
				for (int i = 0; i < weaponArray.GetCount(); i++)
				{
					aiDisplayf("ControlVehicleWeapons - Weapon Index %i - 0x%p - %s", i, weaponArray[i], weaponArray[i] ? weaponArray[i]->GetName() : "NULL");
				}		
			}
		}
	}
}

bool CTaskVehicleMountedWeapon::EvaluateTerrainForFireDirection(Vector3 &vFireDirection)
{
	CPed *pPed = GetPed();
	CVehicle* pVehicle = pPed->GetMyVehicle(); 

	switch(m_mode)
	{
	case Mode_Player:
		{
			if(pPed->IsLocalPlayer())
			{
				//! only do if vehicle is on ground (or front wheels are at least).
				bool bWheelsOnGround = false;
				if( (pVehicle->GetWheel(0) && pVehicle->GetWheel(0)->GetIsTouching()) || (pVehicle->GetWheel(1) && pVehicle->GetWheel(1)->GetIsTouching()) )
				{
					bWheelsOnGround=true;
				}

				if(bWheelsOnGround && !pVehicle->GetIsInWater())
				{
					CVehicle* pVehicle = pPed->GetMyVehicle();

					Vector3	vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

					TUNE_GROUP_BOOL(SPY_CAR_TUNE, bRenderTerrainAnalysis, false);
					TUNE_GROUP_INT(SPY_CAR_TUNE, iNumVerticalProbes, 3, 0, 10, 1);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fHorTestStartZOffset, 0.2f, 0.0f, 100.0f, 0.01f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fVerticalProbeZOffset, 3.0f, 0.0f, 100.0f, 0.01f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fDistanceBetweenVerticalProbes, 1.75f, 0.0f, 100.0f, 0.1f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fVerticalProbeLength, 15.0f, 0.0f, 100.0f, 0.1f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fSteepSlopeRejectDot, 0.5f, 0.0f, 100.0f, 0.1f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fHorTerrainProbeLength, 25.0f, 0.0f, 100.0f, 0.1f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fHorTestNormalRejectDot, 0.3f, 0.0f, 100.0f, 0.1f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fFireDirectionStartZOffset, 0.05f, 0.0f, 100.0f, 0.1f);
					TUNE_GROUP_FLOAT(SPY_CAR_TUNE, fFixedGroundOffset, 0.225f, 0.0f, 100.0f, 0.1f);

					spdAABB vehBoundBox;
					vehBoundBox = pVehicle->GetBoundBox(vehBoundBox);
					float fToFrontDist = VEC3V_TO_VECTOR3(vehBoundBox.GetExtent()).x;

					Vector3	vTestDir = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());

					Vector3	vVehicleFrontPos = vVehiclePosition;
					vVehicleFrontPos = vVehicleFrontPos + (vTestDir * (fToFrontDist));

					Vector3 vHorTestStart = vVehicleFrontPos;
					vHorTestStart.z += fHorTestStartZOffset;

					Vector3 vHorTestEnd = vHorTestStart + (vTestDir * (fHorTerrainProbeLength));

					Vector3 vAimPos;
					vAimPos.Zero();
					bool bDoVerticalTests = true;
					bool bValidAimPos = false;

					WorldProbe::CShapeTestHitPoint hitPoint;
					bool bHit = DoTerrainProbe(pVehicle, vHorTestStart, vHorTestEnd, hitPoint, true);

					float fHorProbeLength = fHorTerrainProbeLength;
					Vector3 vTerrainProbeStart;
					if(bHit)
					{
						BANK_ONLY(if(bRenderTerrainAnalysis) { grcDebugDraw::Line(vHorTestStart, hitPoint.GetHitPosition(), Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 1 ); } )

						Vector3 vToHit = hitPoint.GetHitPosition() - vHorTestStart;
						fHorProbeLength = vToHit.Mag();

						if(hitPoint.GetHitEntity() && hitPoint.GetHitEntity()->GetIsTypeVehicle())
						{
							vAimPos = hitPoint.GetHitPosition();
							bValidAimPos = true;
							bDoVerticalTests = false;
						}
						else
						{
							vTerrainProbeStart = hitPoint.GetHitPosition();

							//! If we hit a vertical wall, don't proceed.
							if(hitPoint.GetHitNormal().z <= fHorTestNormalRejectDot)
							{
								return false;
							}
						}
					}
					else
					{
						BANK_ONLY(if(bRenderTerrainAnalysis) { grcDebugDraw::Line(vHorTestStart, vHorTestEnd, Color32(255, 255, 0, 255), Color32(255, 0, 0, 255), 1 ); } )
						vTerrainProbeStart = vHorTestEnd;
					}

					if(bDoVerticalTests)
					{
						Vector3 vPrevHit;
						u32 nNumHits = 0;
						vTerrainProbeStart.z += fVerticalProbeZOffset;

						for(int i = 0; i < iNumVerticalProbes; ++i)
						{
							Vector3 vStart = vTerrainProbeStart + ((float)i * vTestDir * fDistanceBetweenVerticalProbes);
							Vector3 vEnd = vStart;
							vEnd.z -= fVerticalProbeLength;
							bool bHit = DoTerrainProbe(pVehicle, vStart, vEnd, hitPoint, false);
							if(bHit)
							{
								BANK_ONLY(if(bRenderTerrainAnalysis) { grcDebugDraw::Line(vStart, vEnd, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), 1 ); } )

								//! Reject if we hit a steep slope.
								if(nNumHits > 0)
								{
									Vector3 vToPrevHit = hitPoint.GetHitPosition() - vPrevHit;
									vToPrevHit.NormalizeSafe();
									bool bSteepSlope = (abs(vToPrevHit.z) > fSteepSlopeRejectDot);
									
									if(bSteepSlope)
									{
										break;
									}
								}

								vPrevHit = hitPoint.GetHitPosition();
								
								Vector3 vHitPositionToStore = hitPoint.GetHitPosition();
								vHitPositionToStore.z += fFixedGroundOffset;
								vAimPos += vHitPositionToStore;
								nNumHits++;
							}
							else
							{
								//! if we hit a hole, stop.
								BANK_ONLY(if(bRenderTerrainAnalysis) { grcDebugDraw::Line(vStart, vEnd, Color32(255, 255, 0, 255), Color32(255, 0, 0, 255), 1 ); } )
								break;
							}
						}

						if(nNumHits > 1)
						{
							vAimPos = vAimPos / (float)nNumHits;
							bValidAimPos = true;
						}
					}

					if(bValidAimPos)
					{
						Vector3 vFireStart = vVehicleFrontPos;
						vFireStart.z -= fFireDirectionStartZOffset;

						vFireDirection = vAimPos - vFireStart;
						vFireDirection.Normalize();

						Vector3	vFireDirStart = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
						vFireDirStart = vFireDirStart + (vTestDir * (fToFrontDist));

						Vector3 vFireDirEnd = vFireDirStart + (vFireDirection * fHorTerrainProbeLength);

						//! test that we haven't just maneuvered fire direction into ground.
						bool bAcceptNewDirection = true;
						if(bDoVerticalTests)
						{
							bool bHit = DoTerrainProbe(pVehicle, vFireStart, vFireDirEnd, hitPoint, false);
							if(bHit)
							{
								Vector3 vToHit = hitPoint.GetHitPosition() - vHorTestStart;
								float fFireDirectionLength = vToHit.Mag();
								if(fFireDirectionLength < fHorProbeLength)
								{
									bAcceptNewDirection = false;
								}
							}
						}

						if(bAcceptNewDirection)
						{
							BANK_ONLY(if(bRenderTerrainAnalysis) { grcDebugDraw::Line(vFireDirStart, vFireDirEnd, Color32(0, 0, 255, 255), Color32(0, 0, 255, 255), 1 ); } )
							return true;
						}
					}
				}
			}
		}
	default:
		break;
	}

	return false;
}

bool CTaskVehicleMountedWeapon::DoTerrainProbe(const CVehicle* pVehicle, Vector3 &start, Vector3 &end, WorldProbe::CShapeTestHitPoint &hitPoint, bool bIncludeVehicles)
{
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestResults probeResults(hitPoint);
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(start, end);
	probeDesc.SetExcludeEntity(pVehicle);

	u32 nFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;
	if(bIncludeVehicles)
	{
		nFlags |= ArchetypeFlags::GTA_MAP_TYPE_VEHICLE;
	}

	probeDesc.SetIncludeFlags(nFlags);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		return true;
	}
	return false;
}

void CTaskVehicleMountedWeapon::ComputeSearchTarget( Vector3& o_Target, CPed& in_Ped )
{
	CVehicle* pVehicle = in_Ped.GetMyVehicle();
	aiFatalAssertf(pVehicle,"Unexpected NULL vehicle in CTaskUseVehicleWeapon");

	CSearchLight* pSearchLight = HelperGetVehicleSearchlight(in_Ped);
	float SweepTheta = 0.0f;
	if ( pSearchLight )
	{
		SweepTheta = pSearchLight->GetSweepTheta();
		if ( SweepTheta > 90.0f && SweepTheta <= 270.0f )
		{
			SweepTheta = 180.0f - SweepTheta;
		}
		else if ( SweepTheta > 270.0f )
		{
			SweepTheta = SweepTheta - 360.0f;
		}
	}

	Vector3 vPosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	if(pSearchLight->GetSearchLightTarget() && pSearchLight)
	{	
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fRadiansPerSecondX, 0.35f, 0.0f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fRadiansPerSecondY, 0.6f, 0.0f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fRadiansPerSecondZ, 0.025f, 0.0f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fScaleExpandUpper, 2.25f, 0.0f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fScaleExpandLow, 0.25f, 0.0f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fScaleOllisationX, 0.05f, 0.01f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fScaleOllisationY, 0.09f, 0.01f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fScaleOllisationZ, 0.125f, 0.01f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fTargetOffsetX, 5.0f, 0.01f, 50.0f, 0.01f);
		TUNE_GROUP_FLOAT (SEARCH_LIGHT, fTargetOffsetY, 7.5f, 0.01f, 50.0f, 0.01f);

		//Increment the timers
		Vector3 fSearchLightExtendTimer = pSearchLight->GetSearchLightExtendTimer();
		fSearchLightExtendTimer.x += fRadiansPerSecondX * fwTimer::GetTimeStep();
		fSearchLightExtendTimer.y += fRadiansPerSecondY * fwTimer::GetTimeStep();
		fSearchLightExtendTimer.z += fRadiansPerSecondZ * fwTimer::GetTimeStep();
		pSearchLight->SetSearchLightExtendTimer(fSearchLightExtendTimer);

		//Get the target which is currently only ever the player
		Vector3 vTargetPosition = VEC3V_TO_VECTOR3(pSearchLight->GetSearchLightTarget()->GetTransform().GetPosition());	
	
		//Random offset to the target position
		vTargetPosition.x += sin(fSearchLightExtendTimer.x) * fTargetOffsetX;
		vTargetPosition.y += sin(fSearchLightExtendTimer.y) * fTargetOffsetY;

		//Calculate the Z rotation
		float fSinAngleY = sin(fSearchLightExtendTimer.y) / fScaleOllisationY;
		Vector3 offset(0.0f, fSinAngleY, 0.0f);

		//scale the rotation and rotate.
		offset.RotateZ(fSearchLightExtendTimer.z / fScaleOllisationZ);

		//Scale the offset so it focus of the target briefly 
		float fScaler = Lerp(Abs(sin(fSearchLightExtendTimer.x)),fScaleExpandLow,fScaleExpandUpper);
		offset *= fScaler;

		//Add the offset to the vTargetPosition
		vTargetPosition += offset;
		o_Target = vTargetPosition;
	}
	else
	{
		Vector3 vForward = Vector3(0.0f, 1.0f, 0.0f);
		vForward.RotateX(-QUARTER_PI);
		vForward.RotateZ(pVehicle->GetTransform().GetHeading() + SweepTheta * DtoR);
		o_Target = vPosition + vForward * 1000.0f;
	}	
}

REGISTER_TUNE_GROUP_BOOL( USE_LOCAL_TARGET_POS, true )

bool CTaskVehicleMountedWeapon::GetTargetInfo(CPed* pPed, Vector3& vTargetPosOut,const CEntity*& pTargetOut, bool bDriveTurretTarget) const
{
	if(pPed->IsNetworkClone())
	{
		// Use the m_target
		pTargetOut = m_target.GetEntity();

		if(pTargetOut)
		{
			return m_target.GetIsValid() && m_target.GetPosition(vTargetPosOut);
		}
		else
		{
			INSTANTIATE_TUNE_GROUP_BOOL( TURRET_TUNE, USE_LOCAL_TARGET_POS )
			const CTurret* pHeldTurret = GetHeldTurretIfShouldSyncLocalDirection();
			if (pHeldTurret && USE_LOCAL_TARGET_POS)
			{
				vTargetPosOut = pHeldTurret->ComputeWorldPosFromLocalDir(bDriveTurretTarget ? m_targetPositionCam : m_targetPosition, pPed->GetMyVehicle());
			}
			else
			{
				vTargetPosOut = bDriveTurretTarget ? m_targetPositionCam : m_targetPosition;
			}
		}

#if DEBUG_DRAW
		TUNE_GROUP_BOOL(TURRET_TUNE, SYNCING_bDrawPositionReceivedByVehOwner, false);
		TUNE_GROUP_BOOL(TURRET_TUNE, SYNCING_bDrawPositionReceivedByVehOwnerCamera, false);
		static float fReticuleSphereRadius = 0.5f;
		static bool bSolidReticule = false;
		if (SYNCING_bDrawPositionReceivedByVehOwner && !bDriveTurretTarget)
		{
			grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vTargetPosOut),fReticuleSphereRadius,Color_blue,bSolidReticule,iTurretTuneDebugTimeToLive);
		}
		if (SYNCING_bDrawPositionReceivedByVehOwnerCamera && bDriveTurretTarget)
		{
			grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(vTargetPosOut),fReticuleSphereRadius,Color_CadetBlue,bSolidReticule,iTurretTuneDebugTimeToLive);
		}
#endif

		return true;
	}

	switch(m_mode)
	{
	case Mode_Player:
		{
			if(pPed->IsLocalPlayer())
			{
				// How far we look along the camera / weapon vector to generate the target position. A small value only really works accurately for small weapons with a camera close to the vehicle.
				float fLookAheadDistance = 30.0f;
				const CVehicleWeapon* pVehicleWeapon = GetEquippedVehicleWeapon(pPed);
				if (pVehicleWeapon && pVehicleWeapon->GetWeaponInfo() && pVehicleWeapon->GetWeaponInfo()->GetUseWeaponRangeForTargetProbe())
				{
					fLookAheadDistance = pVehicleWeapon->GetWeaponInfo()->GetRange();
				}

				pTargetOut = pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
				if( pTargetOut )
				{
					// For vehicle weapons behaving like on-foot homing (TRAILERSMALL2), we want a valid target entity returned, but the target position (for turret facing, etc.) to always be camera forward
					const CVehicleWeapon* pVehicleWeapon = GetEquippedVehicleWeapon(pPed);
					if (pVehicleWeapon && pVehicleWeapon->GetWeaponInfo() && pVehicleWeapon->GetWeaponInfo()->GetIsOnFootHoming())
					{
						vTargetPosOut = camInterface::GetPos() + (camInterface::GetFront() * fLookAheadDistance);
					}
					else
					{
						vTargetPosOut = VEC3V_TO_VECTOR3(pTargetOut->GetTransform().GetPosition());
					}
				}
				else
				{
					if(camInterface::GetScriptDirector().IsRendering() )
					{
						if (NetworkInterface::IsGameInProgress() && 
							camInterface::GetScriptDirector().GetFrame().GetPosition().IsClose(g_InvalidPosition,0.5f))
						{
							AI_LOG_WITH_ARGS("[VehicleMountedWeapon] - Ped %s can't get target due to script director rendering at origin\n", AILogging::GetDynamicEntityNameSafe(pPed));
							return false;
						}

						vTargetPosOut = camInterface::GetScriptDirector().GetFrame().GetPosition() + (camInterface::GetScriptDirector().GetFrame().GetFront() * 30.0f);
					}
					else
					{
						camBaseCamera* pVehCam = (camBaseCamera*)camInterface::GetGameplayDirector().GetFollowVehicleCamera();
						Vector3 vBasePos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

						Matrix34 weaponMat;
						bool bSyncingActualReticlePosition = false;
						CVehicle* pVehicle = pPed->GetMyVehicle();
						TUNE_GROUP_BOOL(TURRET_TUNE, bSyncAccurateReticulePosition, true);
						if (bSyncAccurateReticulePosition && !bDriveTurretTarget && NetworkInterface::IsGameInProgress() && pVehicle && pVehicle->GetVehicleWeaponMgr())
						{
							s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
							CTurret* pTurret = pVehicle->GetVehicleWeaponMgr()->GetFirstTurretForSeat(iSeatIndex);
							if (pTurret)
							{
								CFixedVehicleWeapon* pFixedWeapon = pVehicle->GetVehicleWeaponMgr()->GetFirstFixedWeaponForSeat(iSeatIndex);
								if (pFixedWeapon)
								{
									if (pFixedWeapon->GetReticlePosition(vTargetPosOut))
									{
										bSyncingActualReticlePosition = true;

										s32 nWeaponBoneIndex = pVehicle->GetBoneIndex(pFixedWeapon->GetWeaponBone());
										if(nWeaponBoneIndex >= 0)
										{
											pVehicle->GetGlobalMtx(nWeaponBoneIndex, weaponMat);

											vBasePos = weaponMat.d;
											Vector3 vDir = vTargetPosOut - vBasePos;
											vDir.NormalizeFast();
											vTargetPosOut = vBasePos + vDir * fLookAheadDistance;
										}

										AI_LOG_WITH_ARGS("[%s] - Ped %s got reticle position <%.2f,%.2f,%.2f>\n", GetTaskName(), AILogging::GetDynamicEntityNameSafe(pPed), VEC3V_ARGS(VECTOR3_TO_VEC3V(vTargetPosOut)));
									}
								}
							}
						}
						
#if ENABLE_MOTION_IN_TURRET_TASK
						const CTaskMotionInTurret* pMotionInTurretTask = static_cast<const CTaskMotionInTurret*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_TURRET));
#endif

						const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
						bool bShouldUseThirdPersonAimCamera = pDrivebyInfo && pDrivebyInfo->GetUseAimCameraInThisSeat();

						if (!bSyncingActualReticlePosition)
						{
#if ENABLE_MOTION_IN_TURRET_TASK
							const camControlHelper* controlHelper = NULL;
							if (pMotionInTurretTask || bShouldUseThirdPersonAimCamera)
							{
								pVehCam = (camBaseCamera*)camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
								controlHelper = pVehCam ? static_cast<const camThirdPersonPedAimCamera*>(pVehCam)->GetControlHelper() : NULL;
								if(camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
								{
									camCinematicMountedCamera* pTurretCamera = camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
									pVehCam = (camBaseCamera*)camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
									controlHelper = pTurretCamera->GetControlHelper();
								}
							}
#else
							const camThirdPersonPedAimCamera* thirdPersonPedAimCamera = static_cast<const camThirdPersonPedAimCamera*>(pVehCam);
							const camControlHelper* controlHelper = thirdPersonPedAimCamera ? thirdPersonPedAimCamera->GetControlHelper() : NULL;
#endif // ENABLE_MOTION_IN_TURRET_TASK

							if (pVehCam)
								vBasePos = pVehCam->GetFrame().GetPosition();

							const bool isLookingBehind = controlHelper && controlHelper->IsLookingBehind();
							const bool isAimingOrFiring = GetState() == State_Aiming || GetState() == State_Firing;
						
							// B*3650068: fix for cinematic idle cam rotating turrets (that aren't using CTaskMotionInTurret; ie APC turret etc)
							if (!pVehCam && camInterface::GetCinematicDirector().IsRendering())
							{
								if (camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera())
								{
									pVehCam = (camBaseCamera*)camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();
								}
								else
								{
									pVehCam = (camBaseCamera*)camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
								}
							}

							const Vector3 vCamFront = pVehCam ? ((isLookingBehind && !isAimingOrFiring) ? pVehCam->GetBaseFront() : pVehCam->GetFrame().GetFront()) : camInterface::GetFront();
							const Vector3 vTargetOffset = vCamFront * fLookAheadDistance;
							vTargetPosOut = vBasePos + vTargetOffset;
						}

#if ENABLE_MOTION_IN_TURRET_TASK	
						// Lerp between initial turret heading direction and target when adjusting
						float fMoveRatio = 0.0f;
						if (pMotionInTurretTask && !pMotionInTurretTask->GetIsSeated() && pMotionInTurretTask->GetState() == CTaskMotionInTurret::State_InitialAdjust)
						{
							const float fInitialTurretHeading = pMotionInTurretTask->GetInitialTurretHeading();
							Vector3 vInitialFront(0.0f, 1.0f, 0.0f);
							vInitialFront.RotateZ(fInitialTurretHeading);
							const Vector3 vInitialTarget = vBasePos + vInitialFront * fLookAheadDistance;
							const Vector3 vDesiredTarget = vTargetPosOut;
							fMoveRatio = pMotionInTurretTask->GetAdjustStepMoveRatio();
							vTargetPosOut = Lerp(fMoveRatio, vInitialTarget, vDesiredTarget);
						}
#endif // ENABLE_MOTION_IN_TURRET_TASK

						//NOTE: We use the (unshaken) frame of a first-person mounted camera, if it is flagged to affect aiming.
						//Ideally this should be handled more generically, via the camera interface, but we need to be careful at this point.
						const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
						if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
						{
							const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
							const bool shouldAffectAiming					= pMountedCamera->ShouldAffectAiming();
							if(shouldAffectAiming)
							{
								const camFrame& mountedCameraFrame = pMountedCamera->GetFrameNoPostEffects();
								vTargetPosOut = mountedCameraFrame.GetPosition() + (mountedCameraFrame.GetFront() * fLookAheadDistance);
							}
						}

#if DEBUG_DRAW
						TUNE_GROUP_BOOL(TURRET_TUNE, RENDER_TARGET, false);
						if (RENDER_TARGET)
							grcDebugDraw::Sphere(vTargetPosOut, 0.25f MOTION_IN_TURRET_TASK_ONLY(* (1.0f - fMoveRatio)) + 0.01f MOTION_IN_TURRET_TASK_ONLY(* fMoveRatio), pVehCam ? Color_blue : Color_green);
#endif // DEBUG_DRAW
					}
				}

				Assert(rage::FPIsFinite(vTargetPosOut.x));
				Assert(rage::FPIsFinite(vTargetPosOut.y));
				Assert(rage::FPIsFinite(vTargetPosOut.z));

				return true;
			}
			else
			{
				// Non local player with task in player mode
				return false;
			}			
		}
	case Mode_Aim:
	case Mode_Fire:
		{
			// Use the m_target
			pTargetOut = m_target.GetEntity();
			return m_target.GetIsValid() && m_target.GetPosition(vTargetPosOut);
		}
	case Mode_Idle:
		{
			// Point straight forward
			pTargetOut = NULL;
			vTargetPosOut = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(pPed->GetTransform().GetB())* 50.0f);
			return true;
		}
	case Mode_Camera:
		{
			pTargetOut = NULL;
			vTargetPosOut = camInterface::GetPos() + (camInterface::GetFront() * 30.0f);
			return true;
		}
	case Mode_Search:
		{
			ComputeSearchTarget(vTargetPosOut, *pPed);
			return true;
		}

	default:
		taskAssertf(false,"Unhandled mode in CTaskVehicleMountedWeapon");
		return false;
	}
}

int CTaskVehicleMountedWeapon::GetVehicleTargetingFlags(CVehicle *pVehicle, const CPed *pPed, const CVehicleWeapon* pEquippedVehicleWeapon, CEntity* pCurrentTarget)
{
	const CPlayerPedTargeting &targeting = pPed->GetPlayerInfo()->GetTargeting();

	int iTargetingFlags = 0;
	if( pVehicle->GetIsAircraft() || (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_AIRCRAFT_STYLE_WEAPON_TARGETING) && pEquippedVehicleWeapon->GetWeaponInfo()->GetIsHoming()))
		iTargetingFlags = CPedTargetEvaluator::AIRCRAFT_WEAPON_TARGET_FLAGS;
	else
		iTargetingFlags = CPedTargetEvaluator::VEHICLE_WEAPON_TARGET_FLAGS;

	if (pVehicle->GetHomingCanLockOnToObjects())
	{
		iTargetingFlags |= CPedTargetEvaluator::TS_CHECK_OBJECTS;
	}

	const CWeaponInfo* pWeaponInfo = pEquippedVehicleWeapon->GetWeaponInfo();
	if( pWeaponInfo )
	{
		if( pWeaponInfo->GetIsHoming() && targeting.GetVehicleHomingEnabled())
		{
			iTargetingFlags |= CPedTargetEvaluator::TS_HOMING_MISSLE_CHECK;
		}

		if( pWeaponInfo->GetAllowDriverLockOnToAmbientPeds())
		{
			iTargetingFlags |= CPedTargetEvaluator::TS_ALLOW_DRIVER_LOCKON_TO_AMBIENT_PEDS;
		}

		if (pWeaponInfo->GetIsOnFootHoming()) // Vehicle weapon that behaves like on-foot homing (TRAILERSMALL2)
		{
			iTargetingFlags &= ~CPedTargetEvaluator::TS_CHECK_PEDS;
			iTargetingFlags &= ~CPedTargetEvaluator::TS_IGNORE_VEHICLES_IN_LOS;
		}
		
		if (pWeaponInfo->GetUseCameraHeadingForHomingTargetCheck())
		{
			iTargetingFlags &= ~CPedTargetEvaluator::TS_PED_HEADING;
			iTargetingFlags |= CPedTargetEvaluator::TS_CAMERA_HEADING;
		}
	}

	// Do we currently have a target and allow weapon select?
	if( pCurrentTarget || pEquippedVehicleWeapon->IsWaterCannon() )
	{
		// static dev_float s_fControl = 1.0f;
		const CControl* pControl = pPed->GetControlFromPlayer();

		if(pVehicle->GetIsAircraft())
		{
			if( pControl && pControl->GetVehicleFlySelectLeftTarget().IsPressed() )
			{
				iTargetingFlags |= CPedTargetEvaluator::TS_CYCLE_LEFT;
			}
			if( pControl && pControl->GetVehicleFlySelectRightTarget().IsPressed() )
			{
				iTargetingFlags |= CPedTargetEvaluator::TS_CYCLE_RIGHT;
			}
		}
		else
		{
			if (CPlayerInfo::IsSelectingLeftTarget(false))
			{
				iTargetingFlags |= CPedTargetEvaluator::TS_CYCLE_LEFT;
			}
			if (CPlayerInfo::IsSelectingRightTarget(false))
			{
				iTargetingFlags |= CPedTargetEvaluator::TS_CYCLE_RIGHT;
			}
		}
	}

	return iTargetingFlags;
}

CEntity* CTaskVehicleMountedWeapon::FindTarget(CVehicle *pVehicle, 
											   const CPed *pPed, 
											   const CVehicleWeapon* pEquippedVehicleWeapon, 
											   CEntity* BANK_ONLY(pCurrentTarget), 
											   CEntity* pCurrentTargetPreSwitch, 
											   int iTargetingFlags, 
											   float &fWeaponRange, 
											   float &fHeadingLimit, 
											   float &fPitchLimitMin, 
											   float &fPitchLimitMax)
{
	const CPlayerPedTargeting &targeting = pPed->GetPlayerInfo()->GetTargeting();

	// Find range of current weapon
	fHeadingLimit = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetAngularLimitVehicleWeapon;
	fPitchLimitMin = 0.0f;
	fPitchLimitMax = 0.0f;
	fWeaponRange = CVehicleWaterCannon::FIRETRUCK_CANNON_RANGE;
	float fRejectionWeaponRange = fWeaponRange;

	const CWeaponInfo* pWeaponInfo = pEquippedVehicleWeapon->GetWeaponInfo();
	if( pWeaponInfo )
	{
		const CAimingInfo* pAimingInfo = pWeaponInfo->GetAimingInfo();
		if( pAimingInfo )
		{
			fHeadingLimit = pAimingInfo->GetHeadingLimit();
			fPitchLimitMin =pAimingInfo->GetSweepPitchMin();
			fPitchLimitMax =pAimingInfo->GetSweepPitchMax();
		}

		fWeaponRange = pWeaponInfo->GetRange();
		fRejectionWeaponRange = fWeaponRange;

		if( pWeaponInfo->GetIsHoming() && targeting.GetVehicleHomingEnabled())
		{
			//! Increase rejection range for aircraft. This will double lock on range as we allow lock aircraft to aircraft
			//! lock-ons within this range too.
			if(pVehicle->GetIsAircraft())
			{
				fRejectionWeaponRange*=CPedTargetEvaluator::ms_Tunables.m_AircraftToAircraftRejectionModifier;
			}
		}
	}

#if __BANK
	if(CPedTargetEvaluator::DEBUG_VEHICLE_LOCKON_TARGET_SCRORING_ENABLED)
	{
#if DEBUG_DRAW
		CTask::ms_debugDraw.ClearAll();
#endif
		int testTargetingFlags = iTargetingFlags;
		CEntity *pTestTarget = NULL;

		switch(CPedTargetEvaluator::DEBUG_TARGETSCORING_DIRECTION)
		{
		case(CPedTargetEvaluator::eDSD_Left):
			testTargetingFlags |= CPedTargetEvaluator::TS_CYCLE_LEFT;
			pTestTarget = pCurrentTarget;
			break;
		case(CPedTargetEvaluator::eDSD_Right):
			testTargetingFlags |= CPedTargetEvaluator::TS_CYCLE_RIGHT;
			pTestTarget = pCurrentTarget;
			break;
		default:
			break;
		}

		CPedTargetEvaluator::EnableDebugScoring(true);
		CPedTargetEvaluator::FindTarget( pPed, testTargetingFlags, fWeaponRange, fRejectionWeaponRange, pTestTarget, NULL, fHeadingLimit, fPitchLimitMin, fPitchLimitMax );
		CPedTargetEvaluator::EnableDebugScoring(false);
	}
#endif

	CEntity* pFoundTarget = CPedTargetEvaluator::FindTarget( pPed, iTargetingFlags, fWeaponRange, fRejectionWeaponRange, pCurrentTargetPreSwitch, NULL, fHeadingLimit, fPitchLimitMin, fPitchLimitMax );
	return pFoundTarget;
}

void CTaskVehicleMountedWeapon::ProcessLockOn(CPed* pPed)
{
	switch(m_mode)
	{
	case Mode_Player:
		{
			if(pPed->IsLocalPlayer())
			{
#if __BANK
				TUNE_GROUP_BOOL(SPY_CAR_TUNE, bSpyCarTerrainTest, false);
				if(bSpyCarTerrainTest)
				{
					Vector3 vTemp;
					EvaluateTerrainForFireDirection(vTemp);
				}
#endif

				CVehicle* pVehicle = pPed->GetMyVehicle(); 
				weaponAssert(pPed->GetWeaponManager());
				CVehicleWeapon* pEquippedVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
				taskAssertf( pEquippedVehicleWeapon, "No weaponinfo inside mounted gun task");

				CPlayerPedTargeting &targeting = pPed->GetPlayerInfo()->GetTargeting();
				if(IsLockonEnabled(pVehicle, pEquippedVehicleWeapon) && IsLockonPressedForVehicle(pPed, pVehicle, pEquippedVehicleWeapon))
				{
					CEntity* pCurrentTarget = targeting.GetLockOnTarget();

					int iTargetingFlags = GetVehicleTargetingFlags(pVehicle, pPed, pEquippedVehicleWeapon, pCurrentTarget);

					CEntity *pCurrentTargetPreSwitch = NULL;
					bool bManualTargetSwitch = false;
					if(iTargetingFlags & (CPedTargetEvaluator::TS_CYCLE_LEFT | CPedTargetEvaluator::TS_CYCLE_RIGHT ))
					{
						pCurrentTargetPreSwitch = pCurrentTarget;
						bManualTargetSwitch = true; 
					}

					float fWeaponRange = 0.0f;
					float fHeadingLimit = 0.0f;
					float fPitchLimitMin = 0.0f;
					float fPitchLimitMax = 0.0f;
					CEntity* pFoundTarget = FindTarget(pVehicle, pPed, pEquippedVehicleWeapon, pCurrentTarget, pCurrentTargetPreSwitch, iTargetingFlags, fWeaponRange, fHeadingLimit, fPitchLimitMin, fPitchLimitMax);
					
					//! If we tried to find a left/right target and failed, keep current target.
					if(pCurrentTargetPreSwitch && !pFoundTarget)
					{
						pFoundTarget = pCurrentTargetPreSwitch;
					}

					if(pFoundTarget)
					{
						// While we have a target, increment the potential target timer
						if( m_pPotentialTarget )
						{
							m_fPotentialTargetTimer += GetTimeStep();
						}

						TUNE_GROUP_FLOAT(VEHICLE_LOCK_TUNE, MIN_TIME_TO_BREAK_LOCK, 0.5f, 0.0f, 2.0f, 0.01f);
						m_fTargetingResetTimer = MIN_TIME_TO_BREAK_LOCK;

						// If we had no previous target, just set it
						if( !pCurrentTarget )
						{
							targeting.ResetTimeAimingAtTarget();
							targeting.SetLockOnTarget( pFoundTarget );
						}
						// Does our potential target differ from our current?
						else if( pCurrentTarget != pFoundTarget )
						{
							const CWeaponInfo* pWeaponInfo = pEquippedVehicleWeapon->GetWeaponInfo();

							// Check to see the current target is valid and alive
							Vector3 vHeading = VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() );
							float fHeadingDifference = 0.0f;
							bool bOutsideMinHeading = false;
							bool bInInclusionRangeFailedHeading = false;
							int iIsValidFlags = iTargetingFlags;
							iIsValidFlags |= CPedTargetEvaluator::TS_CYCLE_LEFT;
							iIsValidFlags |= CPedTargetEvaluator::TS_CYCLE_RIGHT;
							bool bInOnFootHomingDeadzone = false;
							bool bIsValid = CPedTargetEvaluator::IsTargetValid( pPed->GetTransform().GetPosition(), 
								pCurrentTarget->GetTransform().GetPosition(), 
								iIsValidFlags, 
								vHeading, 
								fWeaponRange, 
								fHeadingDifference, 
								bOutsideMinHeading, 
								bInInclusionRangeFailedHeading, 
								bInOnFootHomingDeadzone,
								fHeadingLimit, 
								fPitchLimitMin, 
								fPitchLimitMax );

							bool bIsAlive = pCurrentTarget->GetIsPhysical() && ((CPhysical*)pCurrentTarget)->GetHealth() > 0;

							// Homing specific logic 
							if( bIsValid && bIsAlive && pWeaponInfo && pWeaponInfo->GetIsHoming() && targeting.GetVehicleHomingEnabled())
							{
								// Selected this manually.
								if( bManualTargetSwitch )
								{
									targeting.ResetTimeAimingAtTarget();
									targeting.SetLockOnTarget( pFoundTarget );

									//! Selected target manually. Allow a longer cooldown between auto switching targets.
									m_uManualSwitchTargetTime = fwTimer::GetTimeInMilliseconds();
								}

								// Do we have a new priority target to be?
								if( pFoundTarget != m_pPotentialTarget )
								{
									// Cache off the new potential target and reset the change target timer
									m_pPotentialTarget = pFoundTarget;
									m_fPotentialTargetTimer = 0.0f;
								}

								// prevent switching targets every frame
								const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
								if( pAmmoInfo && pAmmoInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId() )
								{
									const CAmmoRocketInfo* pRocketInfo = static_cast<const CAmmoRocketInfo*>(pAmmoInfo);
									Assert( pRocketInfo );

									// Calculate the speed ratio based on the default speed
									float fMaxSpeed = rage::square( DEFAULT_MAX_SPEED );
									float fSpeedRatio = rage::Min( pVehicle->GetVelocity().Mag2(), fMaxSpeed ) / fMaxSpeed;

									// Determine the time to switch targets based on the vehicle speed
									float fTimeToSwitchBetweenTargets = Lerp( fSpeedRatio, pRocketInfo->GetTimeBeforeSwitchTargetMax(), pRocketInfo->GetTimeBeforeSwitchTargetMin() );
									
									static dev_u32 s_uManualSwitchCooldown = 5000;

									if( fTimeToSwitchBetweenTargets < m_fPotentialTargetTimer && 
										((m_uManualSwitchTargetTime == 0) || ((fwTimer::GetTimeInMilliseconds() - s_uManualSwitchCooldown) > m_uManualSwitchTargetTime) ) )
									{
										targeting.ResetTimeAimingAtTarget();
										targeting.SetLockOnTarget( pFoundTarget );
										m_uManualSwitchTargetTime = 0;
									}
								}
							}
							// Otherwise just set the new target
							else
							{
								targeting.ResetTimeAimingAtTarget();
								targeting.SetLockOnTarget( pFoundTarget );
							}
						}
					}
					// Clear the target if we don't have one
					else
					{
						m_pPotentialTarget = NULL;
						m_fPotentialTargetTimer = 0.0f;

						m_fTargetingResetTimer -= fwTimer::GetTimeStep();
						if(m_fTargetingResetTimer <= 0.0f)
						{
							targeting.ResetTimeAimingAtTarget();
							targeting.ClearLockOnTarget();
						}
					}
#if __DEV
					// Debug draw the target for now
					static dev_bool bDebugDrawLockon = false;
					if(bDebugDrawLockon)
					{
						if(pCurrentTarget)
						{
							grcDebugDraw::BoxOriented(RCC_VEC3V(pCurrentTarget->GetBoundingBoxMin()),VECTOR3_TO_VEC3V(pCurrentTarget->GetBoundingBoxMax()), pCurrentTarget->GetMatrix(),Color_white);
						}
					}					
#endif
				}
				// Clear the target if we don't have one
				else if( targeting.GetLockOnTarget() != NULL )
				{
					m_pPotentialTarget = NULL;
					m_fPotentialTargetTimer = 0.0f;
					targeting.ResetTimeAimingAtTarget();
					targeting.ClearLockOnTarget();
				}
			}
		}
		break;
	default:
		// Do nothing
		break;
	}
}

CVehicleWeapon* CTaskVehicleMountedWeapon::GetEquippedVehicleWeapon(const CPed* pPed) const
{
	if (!pPed)
		return NULL;

	// Get the equipped vehicle weapon
	aiAssertf(pPed->GetMyVehicle(),"Unexpected NULL vehicle in CTaskUseVehicleWeapon");
	if (!pPed->GetMyVehicle())
		return NULL;

	CVehicleWeaponMgr* pVehWeaponMgr = pPed->GetMyVehicle()->GetVehicleWeaponMgr();
	aiAssertf(pVehWeaponMgr,"CTaskUseVehicleWeapon: This ped's vehicle has no weapon manager");
	if (!pVehWeaponMgr)
		return NULL;

	if (!pPed->GetMyVehicle()->GetSeatManager())
		return NULL;

	atArray<CVehicleWeapon*> weaponArray;
	pVehWeaponMgr->GetWeaponsForSeat(pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed),weaponArray);

	weaponAssert(pPed->GetWeaponManager());
	if (!pPed->GetWeaponManager())
		return NULL;

	int iSelectedWeaponIndex = pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();

	//Only return here with an index that is valid for the weaponArray - otherwise fallback to default and return NULL.
	if (iSelectedWeaponIndex >= 0 && iSelectedWeaponIndex < weaponArray.size())
		return weaponArray[iSelectedWeaponIndex];

	return NULL;
}

bool CTaskVehicleMountedWeapon::WantsToFire(CPed* pPed) const
{
	s32 pVehStatus = pPed->GetMyVehicle() ? pPed->GetMyVehicle()->GetStatus() : -1;
	if (pVehStatus == STATUS_WRECKED)
	{
		return false;
	}

	bool bRet = false;
	switch(m_mode)
	{
	case Mode_Player:
		{
			if(pPed->IsLocalPlayer())
			{
				if(taskVerifyf(pPed->GetPlayerInfo(),"No player info for player"))
				{
					if (ShouldPreventAimingOrFiringDueToLookBehind())
					{
						return false;
					}

					if(pPed->GetPlayerInfo()->IsFiring())
					{
						CVehicleWeapon* pWeapon = GetEquippedVehicleWeapon(pPed);
						if (!pWeapon || !pWeapon->GetWeaponInfo() || !pWeapon->GetWeaponInfo()->GetOnlyFireOneShotPerTriggerPress() || !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasFiring))
						{
							bRet = true;
						}
					}
				}
			}
		}
		break;
	case Mode_Idle:
	case Mode_Aim:
	case Mode_Camera:
	case Mode_Search:
		bRet = false;
		break;
	case Mode_Fire:
		{
			CVehicleWeapon* pWeapon = GetEquippedVehicleWeapon(pPed);
			if (!pWeapon->GetWeaponInfo() || !pWeapon->GetWeaponInfo()->GetOnlyFireOneShotPerTriggerPress() || !m_bFiredAtLeastOnce)
			{
				bRet = true;
			}
		}
		break;
	default:
		{
			taskAssertf(false,"Unhandled mode in WantsToFire");
			bRet = false;
		}
		break;

	}

	return bRet;
}

bool CTaskVehicleMountedWeapon::WantsToAim(CPed* pPed) const
{
	if (m_mode != Mode_Player)
	{
		return false;
	}

	if (ShouldPreventAimingOrFiringDueToLookBehind())
	{
		return false;
	}

	bool bConsiderFiringAsAiming = true;
	
	if (pPed->GetIsInVehicle())
	{
		const s32 iSeatIndex = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
		bConsiderFiringAsAiming = !pPed->GetMyVehicle()->IsTurretSeat(iSeatIndex);
	}

	return pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsAiming(bConsiderFiringAsAiming);	
}

bool CTaskVehicleMountedWeapon::ShouldPreventAimingOrFiringDueToLookBehind() const
{
	TUNE_GROUP_BOOL(TURRET_TUNE, DISABLE_PREVENT_AIM_OR_FIRE, false);
	if (DISABLE_PREVENT_AIM_OR_FIRE)
		return false;

	TUNE_GROUP_FLOAT(TURRET_TUNE, MIN_TIME_AFTER_LOOK_BEHIND_TO_ALLOW_AIM_OR_FIRE, 0.15f, 0.0f, 2.0f, 0.01f);
	if (m_fTimeLookingBehind >= 0.0f && m_fTimeLookingBehind < MIN_TIME_AFTER_LOOK_BEHIND_TO_ALLOW_AIM_OR_FIRE)
	{
		return true;
	}
	return false;
}

const CTurret* CTaskVehicleMountedWeapon::GetHeldTurretIfShouldSyncLocalDirection() const
{
	const CPed* pPed = GetPed();
	const CVehicle* pVehicle = pPed->GetMyVehicle();
	if (pVehicle && pVehicle->GetSeatAnimationInfo(pPed) && pVehicle->GetSeatAnimationInfo(pPed)->IsTurretSeat() && pVehicle->InheritsFromHeli())
	{
		const CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		const CSeatManager* pSeatMgr = pVehicle->GetSeatManager();
		if (pWeaponMgr && pSeatMgr)
		{
			return pWeaponMgr->GetFirstTurretForSeat(pSeatMgr->GetPedsSeatIndex(pPed));

		}
	}
	return NULL;
}

bool CTaskVehicleMountedWeapon::IsTaskValid(CPed* pPed)
{
	if(!pPed)
	{
		return false;
	}

	// Verify our vehicle and the vehicle's weapon manager
	if(!pPed->GetMyVehicle())
	{
		return false;
	}

	// If we don't have weapons at this seat index then this task isn't valid
	// GetSeatHasWeaponsOrTurrets does a validity check on the vehicle weapon manager as well, returning false if it doesn't exist
	s32 iPedsSeatIndex = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
	const bool bSeatHasWeapons = pPed->GetMyVehicle()->GetSeatHasWeaponsOrTurrets(iPedsSeatIndex);
	if(!bSeatHasWeapons)
	{
		return false;
	}

	// Verify the selected weapon in valid
	weaponAssert(pPed->GetWeaponManager());
	int iSelectedWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();
	if(iSelectedWeapon < 0 || iSelectedWeapon >= pPed->GetMyVehicle()->GetVehicleWeaponMgr()->GetNumWeaponsForSeat(iPedsSeatIndex))
	{
		return false;
	}

	//Don't use the mounted weapon if it's a searchlight on the boat, we still want to be able to do drivebys.
	if(pPed->IsPlayer() && pPed->GetWeaponManager() && pPed->GetMyVehicle()->GetVehicleWeaponMgr() )
	{
		CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		if(pVehicleWeapon && pVehicleWeapon->GetType() == VGT_SEARCHLIGHT &&
			(pPed->GetMyVehicle()->GetVehicleWeaponMgr()->GetNumWeaponsForSeat(pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed)) == 1 || 
			!pPed->GetPlayerInfo()->GetCanUseSearchLight()))
		{
			return false;
		}
	}

	// If the weapon manager has been destroyed we shouldn't control the weapons for turret seats (force an exit instead)
#if ENABLE_MOTION_IN_TURRET_TASK
	if(pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
	{
		if(!CTaskMotionInTurret::HasValidWeapon(*pPed, *pPed->GetMyVehicle()))
		{
			return false;
		}
	}
#endif // ENABLE_MOTION_IN_TURRET_TASK
	return true;
}

bool CTaskVehicleMountedWeapon::IsLockonEnabled(CVehicle* pVehicle, CVehicleWeapon* pEquippedVehicleWeapon)
{
	taskFatalAssertf(pVehicle,"Unexpected NULL pointer");

	// Check if vehicle weapon is disabled
	if (!pEquippedVehicleWeapon->GetIsEnabled())
	{
		return false;
	}
	// Check if vehicle weapon is out of ammo (using handling index, not weapon manager index...)
	else if (pVehicle->GetRestrictedAmmoCount(pEquippedVehicleWeapon->m_handlingIndex) == 0)
	{
		return false;
	}
	
	return true;
}

bool CTaskVehicleMountedWeapon::IsLockonPressedForVehicle(CPed* pPlayerPed, CVehicle* pVehicle, CVehicleWeapon* pEquippedVehicleWeapon)
{
	taskFatalAssertf(pPlayerPed,"Unexpected NULL pointer");
	taskFatalAssertf(pVehicle,"Unexpected NULL pointer");

	// Aircraft currently need separate lock-on controls compared to cars
	const CWeaponInfo* pWeaponInfo = pEquippedVehicleWeapon->GetWeaponInfo();
	const bool bIsVehicleAircraftOrSubmarineCar = pPlayerPed->GetMyVehicle() && (pVehicle->GetIsAircraft() || pVehicle->InheritsFromSubmarineCar());
	bool bVehicleHomingEnabled = pWeaponInfo && pWeaponInfo->GetIsHoming() && pPlayerPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingEnabled();
	bool bLockonIsDown = false;

	if(pWeaponInfo && pWeaponInfo->GetLockOnRequiresAim())
	{
		bLockonIsDown = bVehicleHomingEnabled && CPlayerInfo::IsHardAiming();
	}
	else if(bIsVehicleAircraftOrSubmarineCar || pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_AIRCRAFT_STYLE_WEAPON_TARGETING))
	{
		bLockonIsDown = bVehicleHomingEnabled;
	}
	else if( !(pWeaponInfo && ( pVehicle->GetHoverMode() || pWeaponInfo->GetIsStaticReticulePosition() ) ) )
	{
		// Hacky fix (we've removed this code in RDR2). If this comes up again, probably best to add a new weapon flag.
		if (pVehicle->GetModelIndex() != MI_CAR_DUNE3)
		{
			CControl* pControl = pPlayerPed->GetControlFromPlayer();
			bLockonIsDown = pControl ? pControl->GetVehicleSelectNextWeapon().IsDown() : false;
		}
	}
	return bLockonIsDown;
}

//////////////////////////////////////////////////////////////////////////
// Handle CTaskVehicleMountedWeapon clone functions
//////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskVehicleMountedWeapon::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

#if __ASSERT
	if (!m_target.GetEntity() && m_targetPosition.IsClose(VEC3_ZERO, SMALL_FLOAT))
	{
		GetPed()->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
		taskAssertf(0, "Ped %s trying to sync an invalid target position, entity pointer ? %s, mode = %i, vehicle = %s [%s]", AILogging::GetDynamicEntityNameSafe(GetPed()), m_target.GetEntity() ? "TRUE" : "FALSE", m_mode, GetPed()->GetMyVehicle() ?  GetPed()->GetMyVehicle()->GetModelName() : "NULL", GetPed()->GetMyVehicle() ? AILogging::GetDynamicEntityNameSafe(GetPed()->GetMyVehicle()) : "");
	}

	if (m_targetPositionCam.IsClose(VEC3_ZERO, SMALL_FLOAT))
	{
		GetPed()->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
		taskAssertf(0, "Ped %s trying to sync an invalid camera target position, entity pointer ? %s, mode = %i, vehicle = %s [%s]", AILogging::GetDynamicEntityNameSafe(GetPed()), m_target.GetEntity() ? "TRUE" : "FALSE", m_mode, GetPed()->GetMyVehicle() ?  GetPed()->GetMyVehicle()->GetModelName() : "NULL", GetPed()->GetMyVehicle() ? AILogging::GetDynamicEntityNameSafe(GetPed()->GetMyVehicle()) : "");
	}
#endif // __ASSERT

#if DEBUG_DRAW
	TUNE_GROUP_BOOL( TURRET_TUNE, SYNCING_bDrawPositionSentByTurretController, false);
	TUNE_GROUP_BOOL( TURRET_TUNE, SYNCING_bDrawPositionSentByTurretControllerCamera, false);
	static float fReticuleSphereRadius = 0.5f;
	static bool bSolidReticule = false;
	if (SYNCING_bDrawPositionSentByTurretController)
	{
		grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_targetPosition),fReticuleSphereRadius,Color_yellow,bSolidReticule,iTurretTuneDebugTimeToLive);
	}
	if (SYNCING_bDrawPositionSentByTurretControllerCamera)
	{
		grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_targetPositionCam),fReticuleSphereRadius,Color_orange,bSolidReticule,iTurretTuneDebugTimeToLive);
	}
#endif

	Vector3 vTargetPos = m_targetPosition;
	Vector3 vTargetPosCam = m_targetPositionCam;
	INSTANTIATE_TUNE_GROUP_BOOL( TURRET_TUNE, USE_LOCAL_TARGET_POS )	
	const CTurret* pHeldTurret = GetHeldTurretIfShouldSyncLocalDirection();
	if (pHeldTurret && USE_LOCAL_TARGET_POS)
	{
		vTargetPos = pHeldTurret->ComputeLocalDirFromWorldPos(m_targetPosition, GetPed()->GetMyVehicle());
		vTargetPosCam = pHeldTurret->ComputeLocalDirFromWorldPos(m_targetPositionCam, GetPed()->GetMyVehicle());
	}

	CFiringPattern& firingPattern = GetPed()->GetPedIntelligence()->GetFiringPattern();
	bool bFiring = firingPattern.ReadyToFire() && firingPattern.WantsToFire();

	// only for watercannon we use the m_bFiredThisFrame instead of firing pattern
	const CVehicleWeapon* pEquippedVehicleWeapon = GetPed()->GetWeaponManager()->GetEquippedVehicleWeapon();
	if(pEquippedVehicleWeapon && pEquippedVehicleWeapon->IsWaterCannon())
	{
		bFiring = m_bFiredThisFrame;
	}

	pInfo = rage_new CClonedControlTaskVehicleMountedWeaponInfo(m_mode, m_target.GetEntity(), vTargetPos, vTargetPosCam, GetState(), m_fFireTolerance, bFiring);

	return pInfo;
}

void CTaskVehicleMountedWeapon::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_VEHICLE_MOUNTED_WEAPON);

	CClonedControlTaskVehicleMountedWeaponInfo *vehicleMountedWeaponInfo = static_cast<CClonedControlTaskVehicleMountedWeaponInfo*>(pTaskInfo);

	m_mode		= vehicleMountedWeaponInfo->GetMode();
	m_fFireTolerance = vehicleMountedWeaponInfo->GetFireTolerance();
	m_bCloneFiring = vehicleMountedWeaponInfo->GetFiringWeapon();

	if(vehicleMountedWeaponInfo->GetTargetEntity())
	{
		m_target.SetEntity(vehicleMountedWeaponInfo->GetTargetEntity());
	}
	else
	{
		m_target.SetPosition(vehicleMountedWeaponInfo->GetTargetPosition());
		m_targetPosition = vehicleMountedWeaponInfo->GetTargetPosition();
		m_targetPositionCam = vehicleMountedWeaponInfo->GetTargetCameraPosition();
	}

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskVehicleMountedWeapon::CleanUp()
{
	CTaskGun::ClearToggleAim(GetPed());
}

// Is the specified state handled by the clone FSM
bool  CTaskVehicleMountedWeapon::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
	case State_Finish:
		return true;

	default:
		return false;
	}
}

CTask::FSM_Return CTaskVehicleMountedWeapon::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != GetState())
			{
				SetState(GetStateFromNetwork());
			}
		}
	}

	FSM_Begin
		FSM_State(State_StreamingClips)		
			FSM_OnUpdate
				return StreamingClips_OnUpdate(pPed);
		FSM_State(State_Idle)
			FSM_OnEnter
				Idle_OnEnter(pPed);
			FSM_OnUpdate
				return Idle_OnUpdate(pPed);
		FSM_State(State_Aiming)
			FSM_OnEnter			
				Aiming_OnEnter(pPed);
			FSM_OnUpdate
				return Aiming_OnUpdateClone(pPed);
		FSM_State(State_Firing)
			FSM_OnEnter
				Firing_OnEnter(pPed);
			FSM_OnUpdate
				return Firing_OnUpdateClone(pPed);
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskFSMClone* CTaskVehicleMountedWeapon::CreateTaskForClonePed(CPed *UNUSED_PARAM(pPed))
{
	CTaskVehicleMountedWeapon * pTaskVehicleMountedWeapon = rage_new CTaskVehicleMountedWeapon(m_mode,&m_target, m_fFireTolerance);

	pTaskVehicleMountedWeapon->m_targetPosition = m_targetPosition;
	pTaskVehicleMountedWeapon->m_targetPositionCam = m_targetPositionCam;

	return pTaskVehicleMountedWeapon;
}

CTaskFSMClone* CTaskVehicleMountedWeapon::CreateTaskForLocalPed(CPed *pPed)
{
	return CreateTaskForClonePed(pPed);
}

void CTaskVehicleMountedWeapon::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

//////////////////////////////////////////////////////////////////////////
// CClonedControlTaskVehicleMountedWeaponInfo
//////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CClonedControlTaskVehicleMountedWeaponInfo::CClonedControlTaskVehicleMountedWeaponInfo(CTaskVehicleMountedWeapon::eTaskMode mode, const CEntity *targetEntity, const Vector3 &targetPosition, const Vector3 &targetPositionCam, const u32 vehicleMountedWeaponState, const float fFireTolerance, bool firingWeapon ) :
m_mode(mode),
m_targetEntity(const_cast<CEntity*>(targetEntity)),
m_targetPosition(targetPosition),
m_targetPositionCam(targetPositionCam),
m_fFireTolerance(fFireTolerance),
m_firingWeapon(firingWeapon)
{
	SetStatusFromMainTaskState(vehicleMountedWeaponState);
}

CClonedControlTaskVehicleMountedWeaponInfo::CClonedControlTaskVehicleMountedWeaponInfo() :
m_mode(CTaskVehicleMountedWeapon::Mode_Player),
m_targetPosition(Vector3(0.0f,0.0f,0.0f)),
m_targetPositionCam(Vector3(0.0f,0.0f,0.0f)),
m_fFireTolerance(0.0f)
{
	m_targetEntity.Invalidate();
}


//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CClonedControlTaskVehicleMountedWeaponInfo::~CClonedControlTaskVehicleMountedWeaponInfo()
{
}

CTaskFSMClone * CClonedControlTaskVehicleMountedWeaponInfo::CreateCloneFSMTask()
{
	CTaskVehicleMountedWeapon* pVehicleMountedWeaponTask = NULL;

	pVehicleMountedWeaponTask = rage_new CTaskVehicleMountedWeapon(m_mode);

	if(GetTargetEntity())
	{
		pVehicleMountedWeaponTask->m_target.SetEntity(GetTargetEntity());
	}
	else
	{
		pVehicleMountedWeaponTask->m_target.SetPosition(GetTargetPosition());
		pVehicleMountedWeaponTask->m_targetPosition = GetTargetPosition();
		pVehicleMountedWeaponTask->m_targetPositionCam = GetTargetCameraPosition();
	}

	return pVehicleMountedWeaponTask;
}

CTask* CClonedControlTaskVehicleMountedWeaponInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}







//////////////////////////////////////////////////////////////////////////
// CClonedVehicleGunInfo
//////////////////////////////////////////////////////////////////////////

CClonedVehicleGunInfo::CClonedVehicleGunInfo(CTaskVehicleGun::eTaskMode mode, const CEntity *targetEntity, const Vector3 &targetPosition, const u32 taskState) :
m_mode(mode),
m_targetEntity(const_cast<CEntity*>(targetEntity)),
m_targetPosition(targetPosition)
{
    CSerialisedFSMTaskInfo::SetStatusFromMainTaskState(taskState);
}

CTaskFSMClone *CClonedVehicleGunInfo::CreateCloneFSMTask()
{
    return rage_new CTaskVehicleGun(m_mode);
}

//////////////////////////////////////////////////////////////////////////
// Class CTaskVehicleGun

CTaskVehicleGun::CTaskVehicleGun(eTaskMode mode, u32 uFiringPatternHash, const CAITarget* pTarget , float fShootRateModifier, float fAbortRange)
: m_mode(mode)
, m_uFiringPatternHash(uFiringPatternHash)
, m_fAbortRange(fAbortRange)
, m_bWeaponInLeftHand(false)
, m_firingDelayTimer(0.0f,0.0f)
, m_bSmashedWindow(false)
, m_bRolledDownWindow(false)
, m_bShouldDestroyWeaponObject(false)
, m_bAimComputed(false)
, m_fMinYaw(0.0f)
, m_fMaxYaw(0.0f)
, m_bNeedToDriveDoorOpen(false)
, m_bUsingDriveByOverrideAnims(false)
, m_bRestarting(false)
, m_bInFirstPersonVehicleCamera(false)
, m_bSkipDrivebyIntroOnRestart(false)
, m_bInstantlyBlendFPSDrivebyAnims(false)
{
	if(pTarget)
	{
		m_target = CAITarget(*pTarget);
	}

	m_fShootRateModifierInv = 1.0f;
	if(fShootRateModifier > 0.0f)
		m_fShootRateModifierInv = 1.0f / fShootRateModifier;

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GUN);
}

CTaskVehicleGun::~CTaskVehicleGun()
{
}

CTask::FSM_Return CTaskVehicleGun::ProcessPreFSM()
{
	CPed* pPed = GetPed(); 

	// If we have no vehicle then epic fail
	if(!pPed->GetMyVehicle() && !pPed->GetMyMount() && !pPed->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		return FSM_Quit;
	}

	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ) && !pPed->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		return FSM_Quit;
	}

	if(!pPed->GetWeaponManager())
	{
		return FSM_Quit;
	}

	// Refresh our clip info stuff each frame
	if(!CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(pPed))
	{
		return FSM_Quit;
	}

	// Make sure weapon can be used for driveby
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();	
	if(!pWeaponInfo || !pWeaponInfo->CanBeUsedAsDriveBy(pPed) || pWeaponInfo->GetIsThrownWeapon())
	{
		return FSM_Quit;
	}

	// Check range of target
	if(m_fAbortRange > 0.0f && m_target.GetEntity())
	{
		Vector3 vSeperation = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - m_target.GetEntity()->GetTransform().GetPosition());
		if(vSeperation.Mag2() > m_fAbortRange*m_fAbortRange)
		{
			RequestTermination();
			return FSM_Continue;
		}
	}
	
	// B*2334179 Fix the door control fighting between CTaskCloseVehicleDoorFromInside and this task
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsAiming, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsDoingDriveby, true );
	
	// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
	m_vAimStart.Set(Vector3::ZeroType);
	m_vAimEnd.Set(Vector3::ZeroType);
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();		
	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	const Matrix34* pmWeapon;
	if (pWeaponObject)
		pmWeapon = &RCC_MATRIX34(pWeaponObject->GetMatrixRef());
	else
		pmWeapon = &RCC_MATRIX34(pPed->GetMatrixRef());
	m_bAimComputed=true;
	if (pWeapon && pPed->IsLocalPlayer() && !pPed->GetPlayerInfo()->AreControlsDisabled() &&
		( pPed->GetWeaponManager()->GetIsArmedGunOrCanBeFiredLikeGun() || pPed->GetWeaponManager()->GetEquippedVehicleWeapon() ) && 
		pWeapon->CalcFireVecFromAimCamera(NULL, *pmWeapon, m_vAimStart, m_vAimEnd))		
	{
		taskAssert(!m_vAimStart.IsEqual(VEC3_ZERO) && !m_vAimEnd.IsEqual(VEC3_ZERO));	

		WorldProbe::CShapeTestHitPoint probeIsect;
		WorldProbe::CShapeTestResults probeResults(probeIsect);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(m_vAimStart, m_vAimEnd);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_WEAPON_TYPES);
		probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TYPES);
		probeDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		probeDesc.SetExcludeEntity(pPed, 0);
		probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{				
			m_vAimEnd = probeResults[0].GetHitPosition();
		} 		
		m_vAimStart = pPed->GetBonePositionCached(BONETAG_NECK);			
	} 
	else
	{
		m_bAimComputed = false;
		if (pWeapon) 
		{
			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_GUN)
			{			
				if (GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)
				{
					if (NetworkInterface::IsGameInProgress() && pPed->IsPlayer() && pPed->IsNetworkClone())
					{
						Vector3 vTarget(Vector3::ZeroType);
						Vector3 vCurrentAim(pPed->GetWeaponManager()->GetWeaponAimPosition());
						GetTargetPosition(pPed,vTarget);

						CWeaponTarget weaponTarget(vTarget);
						static_cast<CTaskAimGunVehicleDriveBy*>(GetSubTask()->GetSubTask())->SetTarget(weaponTarget);
						
						const float IN_CAR_AIM_LERP_SPEED = 0.125f;

						pPed->GetWeaponManager()->SetWeaponAimPosition(Lerp(IN_CAR_AIM_LERP_SPEED, vCurrentAim, vTarget));
					}

					if (static_cast<CTaskAimGunVehicleDriveBy*>(GetSubTask()->GetSubTask())->CalculateFiringVector(pPed, m_vAimStart, m_vAimEnd))				
						m_bAimComputed = true;
				}
			}
		}
	}

	// Keep timer in sync with current weapon
	float fMinTime = pWeaponInfo->GetTimeBetweenShots();
	if(m_uFiringPatternHash == 0)
	{
		fMinTime *= m_fShootRateModifierInv;
	}

	m_firingDelayTimer.SetTime(fMinTime);
	m_firingDelayTimer.Tick(GetTimeStep());

	if (m_bNeedToDriveDoorOpen)
	{
		CCarDoor* pDoor = GetDoorForPed(pPed);
		if (pDoor)
		{
			if (GetClipHelper()) 
			{
				float fPhase = GetClipHelper()->GetPhase();
				const crClip* pClip = GetClipHelper()->GetClip();
				static const float STARTOPENDOOR = 0.244f;
				static const float ENDOPENDOOR = 0.529f;	
				float fStartPhase, fEndPhase;
				bool bStartTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fStartPhase);
				bool bEndTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fEndPhase);

				if (!bStartTagFound)
					fStartPhase = STARTOPENDOOR;

				if (!bEndTagFound)
					fEndPhase = ENDOPENDOOR;

				if (fPhase >= fEndPhase)
				{
					pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
				}
				else if (fPhase >= fStartPhase)
				{
					float fRatio = rage::Min(1.0f, (fPhase - fStartPhase) / (fEndPhase - fStartPhase));
					fRatio = rage::Max(pDoor->GetDoorRatio(), fRatio);
					pDoor->SetTargetDoorOpenRatio(fRatio, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
				}

			}
			else
			{
				pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
			}

		}
	}

    // peds doing driveby's must be synced in the network game, but peds in cars are not
    // synchronised by default, in order to save bandwidth as they don't do much
    if(NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
    {
        NetworkInterface::ForceSynchronisation(pPed);
    }

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleGun::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_Init)
		FSM_OnUpdate
				return Init_OnUpdate();

	FSM_State(State_StreamingClips)
		FSM_OnUpdate
				return StreamingClips_OnUpdate();

	FSM_State(State_SmashWindow)
		FSM_OnEnter
				SmashWindow_OnEnter();
		FSM_OnUpdate
				SmashWindow_OnUpdate();

	FSM_State(State_OpeningDoor)
		FSM_OnUpdate
			return OpeningDoor_OnUpdate();		
	
	FSM_State(State_WaitingToFire)
		FSM_OnUpdate
				return WaitingToFire_OnUpdate();

	FSM_State(State_Gun)
		FSM_OnEnter
			Gun_OnEnter();
		FSM_OnUpdate
			return Gun_OnUpdate();


	FSM_State(State_Finish)
		FSM_OnUpdate
				return FSM_Quit;

FSM_End
}

void CTaskVehicleGun::CleanUp()
{
	StopClip();
	if (m_bNeedToDriveDoorOpen)
	{
		CPed* pPed = GetPed();
		CCarDoor* pDoor = GetDoorForPed(pPed);
		if (pDoor)
		{
			if (!pDoor->GetFlag(CCarDoor::AXIS_SLIDE_X) && !pDoor->GetFlag(CCarDoor::AXIS_SLIDE_Y) && !pDoor->GetFlag(CCarDoor::AXIS_SLIDE_Z))
			{
				pDoor->SetSwingingFree(GetPed()->GetMyVehicle());
			}
		}
	}

	CTaskGun::ClearToggleAim(GetPed());
}

CTask::FSM_Return CTaskVehicleGun::Init_OnUpdate()
{
	CPed* pPed = GetPed();
	pPed->GetPedIntelligence()->GetFiringPattern().SetFiringPattern(m_uFiringPatternHash,true);

	if (m_fShootRateModifierInv != 1.0f)
	{ // don't touch it unless script actually intends to override.  They should be using SET_PED_SHOOT_RATE for this anyway...
		pPed->GetPedIntelligence()->GetFiringPattern().SetTimeTillNextShotModifier(m_fShootRateModifierInv);	
		pPed->GetPedIntelligence()->GetFiringPattern().SetTimeTillNextBurstModifier(m_fShootRateModifierInv);	
	}

	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	taskAssert(pDrivebyInfo);
	m_bNeedToDriveDoorOpen = pDrivebyInfo->GetNeedToOpenDoors();

	SelectClipSet(pPed);

	m_fMinYaw = pDrivebyInfo->GetMinRestrictedAimSweepHeadingAngleDegs(pPed) * DtoR;
	m_fMaxYaw = pDrivebyInfo->GetMaxRestrictedAimSweepHeadingAngleDegs(pPed) * DtoR;

	Vector2 vMinMax;
	if (CTaskVehicleGun::GetRestrictedAimSweepHeadingAngleRads(*pPed, vMinMax))
	{
		m_fMinYaw = vMinMax.x;
		m_fMaxYaw = vMinMax.y;
	}

	const CVehicleSeatAnimInfo* pSeatClipInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
	const CSeatOverrideAnimInfo* pOverrideInfo = CTaskMotionInVehicle::GetSeatOverrideAnimInfoFromPed(*pPed);
	m_bShouldDestroyWeaponObject = pOverrideInfo ? !pOverrideInfo->GetWeaponVisibleAttachedToRightHand() :
		(pSeatClipInfo ? !pSeatClipInfo->GetWeaponRemainsVisible() : false);

	if (pSeatClipInfo && pDrivebyInfo)
	{
		m_bWeaponInLeftHand = pSeatClipInfo->GetWeaponAttachedToLeftHand();
		if (pDrivebyInfo->GetWeaponAttachedToLeftHand1HOnly())
		{
			CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
			const CWeaponInfo* pWeaponInfo = pWeaponMgr ? pWeaponMgr->GetEquippedWeaponInfo() : NULL;
			m_bWeaponInLeftHand= pWeaponInfo ? !pWeaponInfo->GetIsGun2Handed() : false;
		}
		else
		{
			m_bWeaponInLeftHand = m_bWeaponInLeftHand FPS_MODE_SUPPORTED_ONLY(|| ((pPed->IsInFirstPersonVehicleCamera() || pPed->IsFirstPersonShooterModeEnabledForPlayer(false)) && pDrivebyInfo->GetLeftHandedFirstPersonAnims()));
		}
	}
	else
	{
		m_bWeaponInLeftHand = false;
	}

	//Play open door clip if available
	if (m_bNeedToDriveDoorOpen)
	{
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (pVehicle->IsSeatIndexValid(iSeat))
			{
				// We may not have streamed in the in vehicle anims yet, so we must wait before playing
				if (!CTaskVehicleFSM::RequestClipSetFromSeat(pVehicle, iSeat))
				{
					return FSM_Continue;
				}

				s32 iDirectEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);
				if (iDirectEntryIndex > -1)
				{
					CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iDirectEntryIndex);
					if (pDoor && !pDoor->GetIsFullyOpen(0.05f) && !pDoor->GetFlag(CCarDoor::IS_BROKEN_OFF))
					{
						s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
						if (taskVerifyf(iSeat != -1, "Invalid seat index"))
						{
							const CVehicleSeatAnimInfo* pSeatClipInfo = pVehicle->GetSeatAnimationInfo(iSeat);
							if (pSeatClipInfo && (pSeatClipInfo->GetSeatClipSetId().GetHash() != 0))
							{
								static const fwMvClipId OPEN_DOOR_CLIP("d_open_in",0x444B1303);
								StartClipBySetAndClip(pPed,pSeatClipInfo->GetSeatClipSetId(), OPEN_DOOR_CLIP,SLOW_BLEND_IN_DELTA);
								SetState(State_OpeningDoor);
								return FSM_Continue;
							}
						}
					}
				}
			}
		}
	}

	// Only attempt to stream in clip anims if they exist
	if( m_iClipSet != CLIP_SET_ID_INVALID )
		SetState(State_StreamingClips);
	else
		SetState(State_WaitingToFire);

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleGun::StreamingClips_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::TerminationRequested))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (m_ClipSetRequestHelper.Request(m_iClipSet))
	{
		SetState(State_WaitingToFire);
	}
	return FSM_Continue;
}

void CTaskVehicleGun::OpeningDoor_OnEnterClone()
{
	const CPed* pPed = GetPed();
	CVehicle* pVeh = GetPed()->GetMyVehicle();
	if (pVeh)
	{
		const s32 iDirectEntryPoint = pVeh->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(pVeh->GetSeatManager()->GetPedsSeatIndex(pPed), pVeh);
		CTaskVehicleFSM::SetRemotePedUsingDoorsTrue(*pPed, pVeh, iDirectEntryPoint);
	}
}

CTask::FSM_Return CTaskVehicleGun::OpeningDoor_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::TerminationRequested))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (!GetClipHelper()) //anim done
	{
		if (m_ClipSetRequestHelper.Request(m_iClipSet))
		{
			SetState(State_WaitingToFire);
		} 
		else 
		{
			SetState(State_WaitingToFire);
		}
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskVehicleGun::OpeningDoor_OnExitClone()
{
	const CPed* pPed = GetPed();
	CVehicle* pVeh = GetPed()->GetMyVehicle();
	if (pVeh)
	{
		const s32 iDirectEntryPoint = pVeh->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(pVeh->GetSeatManager()->GetPedsSeatIndex(pPed), pVeh);
		CTaskVehicleFSM::SetRemotePedUsingDoorsFalse(*pPed, pVeh, iDirectEntryPoint);
	}
}

void CTaskVehicleGun::SmashWindow_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetMyVehicle();
	weaponAssert(pPed->GetWeaponManager());
	bool smashWindowAnimFound = true;

	// Check to see if a smash window animation exists, some clipsets are missing them
	if (pVehicle)
	{
		const CVehicleSeatAnimInfo* pVehicleSeatInfo = pVehicle->GetSeatAnimationInfo(pPed);
		s32 iDictIndex = -1;
		u32 iAnimHash = 0;
		if (pVehicleSeatInfo)
		{
			bool bFirstPerson = pPed->IsInFirstPersonVehicleCamera();
			smashWindowAnimFound = pVehicleSeatInfo->FindAnim(CVehicleSeatAnimInfo::SMASH_WINDOW, iDictIndex, iAnimHash, bFirstPerson);
		}
	}

	// Don't try to smash the window if the door is more than half open
	bool bIsVehicleDoorOpen = false;
	if (pVehicle)
	{
		CCarDoor* pDoor = GetDoorForPed(pPed);
		if (pDoor && pDoor->GetDoorRatio() > 0.5f)
		{
			bIsVehicleDoorOpen = true;
		}	
	}

	// Don't try to smash the window of a convertible while the roof is being raised or lowered, just roll it down
	bool bIsConvertibleAnimPlaying = pVehicle && pVehicle->DoesVehicleHaveAConvertibleRoofAnimation() && pVehicle->GetConvertibleRoofProgress() > 0.05f && pVehicle->GetConvertibleRoofProgress() < 1.0f;

	if (pVehicle && (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_BULLETPROOF_GLASS) || pPed->GetWeaponManager()->GetIsArmedMelee() || !smashWindowAnimFound || bIsConvertibleAnimPlaying)) //melee now pops windows B* 529241
	{
		// Find window
		s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (pVehicle->IsSeatIndexValid(iSeat))
		{
			int iEntryPointIndex = -1;
			iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);

			// Did we find a door?
			if(iEntryPointIndex != -1)
			{
				const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPointIndex);
				Assertf(pEntryPointInfo, "Vehicle is missing entry point");

				eHierarchyId window = pEntryPointInfo->GetWindowId();
				pVehicle->RolldownWindow(window);

				// Need to cache whether we rolled or smashed so we can pass correct value to drive task later
				m_bRolledDownWindow = true;
			}
		}
	}
	else if (bIsVehicleDoorOpen)
	{
		m_bRolledDownWindow = true; // So we don't signal this window as smashed
	}
	else
	{
		SetNewTask(rage_new CTaskSmashCarWindow(false));
		m_bRolledDownWindow = false;
	}
}

CTask::FSM_Return CTaskVehicleGun::SmashWindow_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		m_bSmashedWindow = !m_bRolledDownWindow;
		CPed* pPed = GetPed();

		if(pPed->IsLocalPlayer())
		{
			CTaskPlayerDrive* pDriveTask = static_cast<CTaskPlayerDrive*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_DRIVE));
			if (pDriveTask)
			{
				pDriveTask->SetSmashedWindow(m_bSmashedWindow);
			}
		}

		SetState(State_WaitingToFire);
	}

	// Set the reset flag to let the hud know we want to dim the reticule
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_DimTargetReticule, true );

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleGun::WaitingToFire_OnUpdate()
{
	CPed* pPed = GetPed();

    if(pPed->IsNetworkClone() && GetStateFromNetwork() == State_Finish)
    {
        SetState(State_Finish);
		return FSM_Continue;
    }

	bool bWantsToSwapWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() == -1 && 
		!pPed->GetWeaponManager()->GetIsArmedMelee() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetUsesAmmo() && pPed->GetInventory()->GetWeaponAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo()) == 0 ? true : false;
	if(bWantsToSwapWeapon)
	{
		Assert(pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() == -1);
		// Attempt to switch to the best available weapon
		pPed->GetWeaponManager()->EquipBestWeapon();		
	}

	Vector3 vTarget(Vector3::ZeroType);
	if (m_bAimComputed)
		vTarget = m_vAimEnd;
	else if(!GetTargetPosition(pPed, vTarget))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

#if __BANK
	m_vTargetPos = vTarget;
	taskAssertf(IsFiniteAll(RCC_VEC3V(m_vTargetPos)), "m_vTargetPos wasn't valid in CTaskVehicleGun::WaitingToFire_OnUpdate()");
#endif

	if(GetIsFlagSet(aiTaskFlags::TerminationRequested))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if (pWeaponInfo && pWeaponInfo->GetIsThrownWeapon()) //switch tasks
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(!m_bSmashedWindow)
	{
		if (NeedsToSmashWindowFromTargetPos(pPed,vTarget))
		{
			SetState(State_SmashWindow);
			return FSM_Continue;
		}
		else if (ClearOutPartialSmashedWindow(pPed, vTarget))
		{
			// We cleared out a partially smashed window
			m_bSmashedWindow = true;
		}
	}
	Vector3 vAimFromPosition = m_bAimComputed ? m_vAimStart : VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());	

	// Work out the yaw and pitch angles to the target
	float fDesiredYaw = CTaskAimGun::CalculateDesiredYaw(pPed, vAimFromPosition, vTarget);
	//float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(vAimFromPosition, vTarget);

	// See if we can fire
	static dev_float s_fFiringRangeBuffer = 10.0f*DtoR;

	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	bool bBlockOutsideRange = pDrivebyInfo && pPed->IsPlayer() && pDrivebyInfo->GetUseBlockingAnimsOutsideAimRange();

	bool bFiringConditionsOK = CheckFiringConditions(pPed, VECTOR3_TO_VEC3V(vTarget), fDesiredYaw, m_fMinYaw+s_fFiringRangeBuffer, m_fMaxYaw-s_fFiringRangeBuffer, bBlockOutsideRange);

	if(pPed->IsNetworkClone())
	{
		if (GetStateFromNetwork() != State_Gun)
		{
			bFiringConditionsOK = false;
		}
	}

	if (bFiringConditionsOK && !pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged)) //not in the same frame of a weapon change
	{
		ProcessWeaponObject(pPed);
		const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
		if(pWeaponInfo && pWeaponInfo->CanBeUsedAsDriveBy(pPed) && (pWeaponInfo->GetIsVehicleWeapon() || pPed->GetWeaponManager()->GetIsArmedMelee() || pPed->GetWeaponManager()->GetEquippedWeaponObject()))
		{
			if (pPed->GetWeaponManager()->GetSelectedWeaponHash() == pPed->GetWeaponManager()->GetEquippedWeaponHash())
			{
				const CVehicleDriveByAnimInfo* pClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash());
				if (pClipInfo && (m_bUsingDriveByOverrideAnims || pClipInfo->GetClipSet() == m_iClipSet || pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) == m_iClipSet || pClipInfo->GetRestrictedClipSet() == m_iClipSet))
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet( m_iClipSet );
					if( m_iClipSet.GetHash() == 0 || ( pClipSet && pClipSet->IsStreamedIn_DEPRECATED() ) )
					{
						AI_LOG_IF_PLAYER(pPed, "CTaskVehicleGun::WaitingToFire_OnUpdate - Setting State_Gun for ped [%s] as m_iClipSet.GetHash() is: %s or Clipset:%s->IsStreamedIn_DEPRECATED() is: %s  \n", AILogging::GetDynamicEntityNameSafe(pPed), BOOL_TO_STRING(m_iClipSet.GetHash()), pClipSet ? pClipSet->GetClipDictionaryName().TryGetCStr() : "null", BOOL_TO_STRING(pClipSet ? pClipSet->IsStreamedIn_DEPRECATED() : false));
						SetState(State_Gun);	
						return FSM_Continue;
					}
					else
					{
						m_ClipSetRequestHelper.Request( m_iClipSet );
						return FSM_Continue;
					}
				}

				SetState(State_Init);
				return FSM_Continue;
			}
		}
	}
	else
	{
		if (!m_bShouldDestroyWeaponObject)
		{
			CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if (pWeapon && pWeapon->GetState() == CWeapon::STATE_RELOADING)
			{
				pWeapon->DoReload(true);
			}
		}

		pPed->SetPedResetFlag( CPED_RESET_FLAG_DimTargetReticule, true );
	}


	return FSM_Continue;
}


bool CTaskVehicleGun::ShouldRestartStateDueToCameraSwitch()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED

	bool bInFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera(true) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));

	bool bCameraChanged = (bInFirstPersonVehicleCamera != m_bInFirstPersonVehicleCamera);
	bool bCamNotifiedChange = pPed->GetPlayerInfo() && pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);

	// Check if we need to restart due to needing/not needing restricted anims due to a passenger camera switch.
	bool bNeedsRestartDueToRestrictedAnims = false; 
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();	
	const CVehicleDriveByAnimInfo* pClipInfo = pWeaponInfo ? CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash()) : NULL;
	if (pClipInfo && pClipInfo->GetRestrictedClipSet() != CLIP_SET_ID_INVALID)
	{
		if ((CTaskAimGunVehicleDriveBy::CanUseRestrictedDrivebyAnims() && CTaskAimGunVehicleDriveBy::ShouldUseRestrictedDrivebyAnimsOrBlockingIK(pPed, true) && m_iClipSet != pClipInfo->GetRestrictedClipSet())
			|| ((!CTaskAimGunVehicleDriveBy::CanUseRestrictedDrivebyAnims() || !CTaskAimGunVehicleDriveBy::ShouldUseRestrictedDrivebyAnimsOrBlockingIK(pPed, true)) && m_iClipSet == pClipInfo->GetRestrictedClipSet()))
		{
			m_bSkipDrivebyIntroOnRestart = true;
			bNeedsRestartDueToRestrictedAnims = true;
		}
	}

	bool bNeedsRestartDueToAnimCameraMismatch = false;
	fwMvClipSetId firstPersonClipset = pClipInfo ? pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) : CLIP_SET_ID_INVALID;
	if (bInFirstPersonVehicleCamera && firstPersonClipset != CLIP_SET_ID_INVALID && m_iClipSet != firstPersonClipset)
	{
		bNeedsRestartDueToAnimCameraMismatch = true;
	}

	if (!m_bRestarting && ( bCameraChanged || bCamNotifiedChange || bNeedsRestartDueToRestrictedAnims || bNeedsRestartDueToAnimCameraMismatch) )
	{	
		return true;
	}
#endif // FPS_MODE_SUPPORTED
	return false;
}

void CTaskVehicleGun::SelectClipSet(const CPed *pPed)
{
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();	
	const CVehicleDriveByAnimInfo* pClipInfo = pWeaponInfo ? CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash()) : NULL;
	taskAssert(pClipInfo);

	if (CTaskAimGunVehicleDriveBy::CanUseRestrictedDrivebyAnims() && CTaskAimGunVehicleDriveBy::ShouldUseRestrictedDrivebyAnimsOrBlockingIK(pPed, true) && pClipInfo->GetRestrictedClipSet() != CLIP_SET_ID_INVALID)
	{
		m_iClipSet = pClipInfo->GetRestrictedClipSet();
	}
	else if( (pPed->IsInFirstPersonVehicleCamera() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false))) && (pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) != CLIP_SET_ID_INVALID))
	{
		m_iClipSet = pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash());
	}
	else if (pPed->GetMotionData()->GetOverrideDriveByClipSet() != CLIP_SET_ID_INVALID)
	{
		m_iClipSet = pPed->GetMotionData()->GetOverrideDriveByClipSet();
		m_bUsingDriveByOverrideAnims = true;
	}
	else
	{
		m_iClipSet = pClipInfo->GetClipSet();
	}
}

void CTaskVehicleGun::Gun_OnEnter()
{
	u32 uFiringPatternHash = FIRING_PATTERN_FULL_AUTO;

	CTaskGun* pGunTask = NULL;
	
	CPed* pPed = GetPed();

	if (pPed->IsLocalPlayer())
	{
		if (pPed->GetPlayerInfo()->AreControlsDisabled())
		{
			//@@: location CTASKVEHICLEGUN_GUN_ONENTER
			pGunTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, m_target.GetEntity() ? CWeaponTarget(m_target.GetEntity()) : m_target);
			pGunTask->SetFiringPatternHash(m_uFiringPatternHash);
		}
		else 
		{
			pGunTask = rage_new CTaskGun(CWeaponController::WCT_Player, CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, m_target.GetEntity() ? CWeaponTarget(m_target.GetEntity()) : m_target);
			pGunTask->SetFiringPatternHash(uFiringPatternHash);
		}
	}
	else
	{
		// Looks like we're completely deleting the offset information here with the new CWeaponTarget.
		// Temporary work-around for jetpack AI:
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
		{
			pGunTask = rage_new CTaskGun(CWeaponController::WCT_Fire, CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, m_target);
			pGunTask->SetFiringPatternHash(m_uFiringPatternHash);
		}
		else
		{
			pGunTask = rage_new CTaskGun(CWeaponController::WCT_Fire, CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, m_target.GetEntity() ? CWeaponTarget(m_target.GetEntity()) : m_target);
			pGunTask->SetFiringPatternHash(m_uFiringPatternHash);
		}
	}

	pGunTask->GetGunFlags().SetFlag(GF_DontUpdateHeading);
	pGunTask->GetGunFlags().SetFlag(GF_FireAtIkLimitsWhenReached);
	pGunTask->GetGunFlags().SetFlag(GF_PreventAutoWeaponSwitching);
	pGunTask->GetGunFlags().SetFlag(GF_AllowFiringWhileBlending);
	pGunTask->GetGunFlags().SetFlag(GF_DisableTorsoIk);
	pGunTask->GetGunFlags().SetFlag(GF_DisableBlockingClip);

	pGunTask->SetShouldSkipDrivebyIntro(m_bSkipDrivebyIntroOnRestart);
	m_bSkipDrivebyIntroOnRestart = false;

	pGunTask->SetShouldInstantlyBlendDrivebyAnims(m_bInstantlyBlendFPSDrivebyAnims);;
	m_bInstantlyBlendFPSDrivebyAnims = false;

	bool bShouldBlockWeaponFire = pPed && pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockWeaponFire);

	// Re-evaluate the weapon hand when the gun task restarts (first-person and third-person can use different hands)
	if (m_bRestarting)
	{
		const CVehicleSeatAnimInfo* pSeatClipInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
		const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
		if (pSeatClipInfo && pDrivebyInfo)
		{
			m_bWeaponInLeftHand = pSeatClipInfo->GetWeaponAttachedToLeftHand();
			if (pDrivebyInfo->GetWeaponAttachedToLeftHand1HOnly())
			{
				CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
				const CWeaponInfo* pWeaponInfo = pWeaponMgr ? pWeaponMgr->GetEquippedWeaponInfo() : NULL;
				m_bWeaponInLeftHand = pWeaponInfo ? !pWeaponInfo->GetIsGun2Handed() : false;
			}
			else
			{
				m_bWeaponInLeftHand = m_bWeaponInLeftHand FPS_MODE_SUPPORTED_ONLY(|| ((pPed->IsInFirstPersonVehicleCamera() || pPed->IsFirstPersonShooterModeEnabledForPlayer(false)) && pDrivebyInfo->GetLeftHandedFirstPersonAnims()));
			}
		}
		else
		{
			m_bWeaponInLeftHand = false;
		}

		ProcessWeaponObject(pPed);
	}

	if (m_mode!=Mode_Player && !bShouldBlockWeaponFire) 
		pGunTask->GetGunFlags().SetFlag(GF_FireAtLeastOnce);

	if (!GetPed()->IsPlayer())
	{
		pGunTask->GetGunFlags().SetFlag(GF_ForceAimState);
	}
    else
    {
        pGunTask->GetGunFlags().SetFlag(GF_UpdateTarget);
    }

	pGunTask->SetOverrideClipSetId(m_iClipSet);
	SetNewTask(pGunTask);

	m_bInFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera(true) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));
}

CTask::FSM_Return CTaskVehicleGun::Gun_OnUpdate()
{
	CPed* pPed = GetPed();

	if(ShouldRestartStateDueToCameraSwitch())
	{
		SelectClipSet(pPed);
		m_bRestarting = true;
		// We're transitioning into the FPS camera, so instantly blend in the FPS anims
		if (!m_bInFirstPersonVehicleCamera)
		{
			m_bInstantlyBlendFPSDrivebyAnims = true;
		}
		SetState(State_Init);
		return FSM_Continue;
	}
	
	m_bInFirstPersonVehicleCamera = pPed->IsInFirstPersonVehicleCamera(true) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false));

	m_bRestarting = false;

    if(pPed->IsNetworkClone() && GetStateFromNetwork() == State_Finish)
    {
        SetState(State_Finish);
		return FSM_Continue;
    }

	Vector3 vTarget(Vector3::ZeroType);
	if (m_bAimComputed)
		vTarget = m_vAimEnd;
	else if(!GetTargetPosition(pPed, vTarget))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

#if __BANK
	m_vTargetPos = vTarget;
	taskAssertf(IsFiniteAll(RCC_VEC3V(m_vTargetPos)), "m_vTargetPos wasn't valid in CTaskVehicleGun::Gun_OnUpdate()");
#endif

	Vector3 vAimFromPosition = m_bAimComputed ? m_vAimStart : VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());	

	// Work out the yaw and pitch angles to the target
	float fDesiredYaw = CTaskAimGun::CalculateDesiredYaw(pPed, vAimFromPosition, vTarget);	

	// Do we need to clear out partially broken windows?
	if (!m_bSmashedWindow && ClearOutPartialSmashedWindow(pPed, vTarget))
	{
		m_bSmashedWindow = true;
	}


	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	bool bBlockOutsideRange = pDrivebyInfo && pPed->IsPlayer() && pDrivebyInfo->GetUseBlockingAnimsOutsideAimRange();
	bool bAllowSingleSideOvershoot = pDrivebyInfo && pPed->IsPlayer() && pDrivebyInfo->GetAllowSingleSideOvershoot();

	// See if we can fire
	bool bFiringConditionsOK = CheckFiringConditions(pPed, VECTOR3_TO_VEC3V(vTarget), fDesiredYaw, m_fMinYaw, m_fMaxYaw, bBlockOutsideRange);
	bool bOutsideRange = CheckIfOutsideRange(fDesiredYaw, m_fMinYaw, m_fMaxYaw, bAllowSingleSideOvershoot);

	if(bFiringConditionsOK && !m_bSmashedWindow && NeedsToSmashWindowFromTargetPos(pPed, vTarget))
	{		
		bFiringConditionsOK = false;
	}

	if(bBlockOutsideRange)
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_GUN)
		{
			if (GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)
			{
				bool bBlock = bOutsideRange && static_cast<CTaskGun*>(GetSubTask())->GetState() == CTaskGun::State_Aim;
				static_cast<CTaskAimGunVehicleDriveBy*>(GetSubTask()->GetSubTask())->SetBlockOutsideAimingRange(bBlock);
			}  
		}
	}

	// Check ammo. No need to check ammo for vehicle weapons since they should have infinite ammo.
	weaponAssert(pPed->GetInventory());	
	if (!bFiringConditionsOK || GetIsFlagSet(aiTaskFlags::TerminationRequested))
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_GUN)
		{
			if (static_cast<CTaskGun*>(GetSubTask())->GetState() == CTaskGun::State_Aim)
			{
 				if (GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)
 				{
 					GetSubTask()->GetSubTask()->RequestTermination();
 				}  
			}
			else
			{

				SetState(State_WaitingToFire);
				return FSM_Continue;
			}
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (pPed->IsNetworkClone())
		{
			SetState(State_Init);
		}
		else
		{
			SetState(State_WaitingToFire);
		}
	}

	return FSM_Continue;
}

bool CTaskVehicleGun::CheckIfOutsideRange(float fDesiredYaw, float fMinYaw, float fMaxYaw, bool bAllowSingleSideOvershoot)
{
	bool bAngleOutsideRange = false;
	static bool sbEnableRangeCheck = true;
	if (sbEnableRangeCheck) 
	{
		// Allow an overshoot on a single side for certain flagged driveby infos
		if (bAllowSingleSideOvershoot)
		{
			if (fMinYaw < -PI)
			{
				float fOvershootDiff = fMinYaw + PI;
				if (fDesiredYaw < PI && fDesiredYaw > PI + fOvershootDiff)
				{
					return false;
				}
			}
			else if (fMaxYaw > PI)
			{
				float fOvershootDiff = fMaxYaw - PI;
				if (fDesiredYaw > -PI && fDesiredYaw < -PI + fOvershootDiff)
				{
					return false;
				}
			}
		}

		if (fDesiredYaw < fMinYaw || fDesiredYaw >fMaxYaw)
		{
			bAngleOutsideRange = true;
		}
	}

	return bAngleOutsideRange;
}

bool CTaskVehicleGun::CheckFiringConditions(const CPed *pPed, Vec3V_In vTargetPosition, float fDesiredYaw, float fMinYaw, float fMaxYaw, bool bBlockOutsideRange)
{
	// network clone firing is dictated by the owning machine
	if (pPed->IsNetworkClone())
	{
		return true;
	}

	if( !CheckVehicleFiringConditions( pPed, vTargetPosition ) )
	{
		return false;
	}

	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	bool bAllowSingleSideOvershoot = pDrivebyInfo && pPed->IsPlayer() && pDrivebyInfo->GetAllowSingleSideOvershoot();
	bool bAngleOutsideRange = CheckIfOutsideRange(fDesiredYaw, fMinYaw, fMaxYaw, bAllowSingleSideOvershoot);

	// Stop firing if outside the angle ranges
	if (bAngleOutsideRange && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim) && !bBlockOutsideRange)
	{
		return false;
	}

	if( pPed->GetIsArrested() )
	{
		return false;
	}

	// No longer terminate task for reloading, just keep gun out per B* 550797  [6/22/2012 musson]
// 	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
// 	const CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
// 	if( !pVehicleWeapon && pWeapon && pWeapon->GetState() == CWeapon::STATE_RELOADING  && pPed->GetMyMount()==NULL ) //mounted peds can play reload anims
// 		return false;

	return true;
}

bool CTaskVehicleGun::GetTargetPosition(const CPed* pPed, Vector3& vTargetPosWorldOut)
{
	switch(m_mode)
	{
	case Mode_Player:
		{
			if(pPed->IsLocalPlayer())
			{
				const CEntity* pTarget = pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
				if( pTarget )
				{
					vTargetPosWorldOut = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
				}
				else
				{
					weaponAssert(pPed->GetWeaponManager());
					const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
					if(pWeapon && pWeaponObject)
					{
						Vector3 vecStart;
						Vector3 vecEnd;
						const Matrix34& weaponMatrix = RCC_MATRIX34(pWeaponObject->GetMatrixRef());

						//NOTE: The aim camera should be attached to this ped's vehicle.
						CVehicle *pVehicle = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) ? pPed->GetMyVehicle() : NULL;
						pWeapon->CalcFireVecFromAimCamera(pVehicle, weaponMatrix, vecStart, vecEnd);
						vTargetPosWorldOut = vecEnd;
					}
					else
					{
						const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
						vTargetPosWorldOut = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (aimFrame.GetFront() * 30.0f);
					}
				}

				Assert(rage::FPIsFinite(vTargetPosWorldOut.x));
				Assert(rage::FPIsFinite(vTargetPosWorldOut.y));
				Assert(rage::FPIsFinite(vTargetPosWorldOut.z));

                // store the target position in the ped target to simplify syncing of this data over the network
                m_target.SetPosition(vTargetPosWorldOut);

				return true;
			}
            else if(taskVerifyf(pPed->IsPlayer(), "Non player ped %s in player mode!", pPed->GetDebugName()))
            {
                bool gotPosition = m_target.GetIsValid() && m_target.GetPosition(vTargetPosWorldOut);

                Assert(rage::FPIsFinite(vTargetPosWorldOut.x));
				Assert(rage::FPIsFinite(vTargetPosWorldOut.y));
				Assert(rage::FPIsFinite(vTargetPosWorldOut.z));

				return gotPosition;
            }
			else
			{
				// Non local player with task in player mode
				return false;
			}			
		}
	case Mode_Aim:
	case Mode_Fire:
		{
			// Use the m_target
			return m_target.GetIsValid() && m_target.GetPosition(vTargetPosWorldOut);
		}
	default:
		taskAssertf(false,"Unhandled mode in CTaskVehicleGun");
		return false;
	}
}

bool CTaskVehicleGun::NeedsToSmashWindowFromTargetPos(const CPed* pPed, const Vector3 &vPosition, bool bHeadingCheckOnly)
{
#if __BANK
	if (CTaskAimGunVehicleDriveBy::ms_bDisableWindowSmash)
	{
		return false;
	}
#endif //__BANK

	// Don't smash the window to taunt, that would be mad, poofing them instead, B* 529241
// 	if (pPed->GetWeaponManager()->GetIsArmedMelee())
// 	{
// 		return false;
// 	}

	// vehicle weapons do not need to smash windows
	if( pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() != -1 )
	{
		return false;
	}

	// By default we need to smash windows
	// But some driver clips have windows where we DONT need to smash the windows

	// First see if we have a window
	s32 iDictIndex = -1;
	u32 iClipHash = 0;
	eHierarchyId iHierarchyId;

	//! Don't smash or roll down window in 1st person view when unarmed.
	if(pPed->IsInFirstPersonVehicleCamera() && pPed->GetWeaponManager()->GetIsArmedMelee())
	{
		return false;
	}

	CVehicle* pVehicle = pPed->GetMyVehicle();
	bool bAttemptingRolldown = pVehicle && (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_BULLETPROOF_GLASS) || pPed->GetWeaponManager()->GetIsArmedMelee()); 
	if(!CTaskSmashCarWindow::GetWindowToSmash(pPed,false,iClipHash,iDictIndex,iHierarchyId, bAttemptingRolldown) && !bHeadingCheckOnly)
	{
		// No window to smash
		return false;
	}

	if(pPed->IsLocalPlayer())
	{
		CTaskPlayerDrive* pDriveTask = static_cast<CTaskPlayerDrive*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_DRIVE));
		if (pDriveTask && pDriveTask->GetSmashedWindow() && !bHeadingCheckOnly)
		{
			// Already smashed window
			return false;
		}
	}

	// Disable window smashing for Zhaba drivebys, doors are nowhere near the seats
	if (MI_CAR_ZHABA.IsValid() && pVehicle && pVehicle->GetModelIndex() == MI_CAR_ZHABA)
	{
		return false;
	}

	if (pVehicle)
	{
		CCarDoor* pDoor = GetDoorForPed(pPed);
		if (pDoor && pDoor->GetDoorRatio() > 0.5f)
		{
			// Can't reach window
			return false;
		}	
	}

	// Calculate the desired clip phase
	//if( clipInfo.m_iFlags & DBF_DriverLimitsToSmashWindows )
	if (vPosition.IsZero())
	{
		return true;
	}
	else
	{
		float fHeading = 0.0f;
		float fPitch = 0.0f;
		CalculateYawAndPitchToTarget(pPed, vPosition,fHeading,fPitch);

		const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
		if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			//! Clamp animation if camera is looking a particular way past threshold.
			const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
			if(pMountedCamera)
			{
				bool bLookingLeft = pMountedCamera->IsLookingLeft();

				if(bLookingLeft && fHeading > HALF_PI)
				{
					fHeading = -PI;
				}
				else if(!bLookingLeft && fHeading < -HALF_PI)
				{
					fHeading = PI;
				}
			}
		}

		const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
		taskAssert(pDrivebyInfo);

		float fMinSmashWindowAngle;
		float fMaxSmashWindowAngle;
		if(pPed->IsInFirstPersonVehicleCamera())
		{
			fMinSmashWindowAngle = pDrivebyInfo->GetMinSmashWindowAngleFirstPersonDegs() * DtoR;
			fMaxSmashWindowAngle = pDrivebyInfo->GetMaxSmashWindowAngleFirstPersonDegs() * DtoR;
		}
		else
		{
			fMinSmashWindowAngle = pDrivebyInfo->GetMinSmashWindowAngleDegs() * DtoR;
			fMaxSmashWindowAngle = pDrivebyInfo->GetMaxSmashWindowAngleDegs() * DtoR;
		}

		if (fHeading >= fMinSmashWindowAngle && fHeading <= fMaxSmashWindowAngle)
		{
			return true;
		}
	} 	
	return false;
}

// If the window is partially broken, then add some force to smash the rest out
bool CTaskVehicleGun::ClearOutPartialSmashedWindow(const CPed* pPed, const Vector3 &vPosition)
{
	if (NeedsToSmashWindowFromTargetPos(pPed, vPosition, true))
	{
		// Find the entry point
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (pVehicle->IsSeatIndexValid(iSeat))
			{
				int iEntryPointIndex = -1;
				iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);

				// Find the window
				if(iEntryPointIndex != -1)
				{
					const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPointIndex);
					Assertf(pEntryPointInfo, "Vehicle is missing entry point");

					eHierarchyId windowId = pEntryPointInfo->GetWindowId();
					if (windowId != VEH_INVALID_ID)
					{	
						const s32 windowBoneIndex = pVehicle->GetBoneIndex(windowId);
						if (windowBoneIndex>=0)
						{
							const s32 windowComponentId = pVehicle->GetFragInst()->GetComponentFromBoneIndex(windowBoneIndex);
							if (windowComponentId>-1)
							{
								if (pVehicle->IsBrokenFlagSet(windowComponentId))
								{
									pVehicle->SmashWindow(windowId, VEHICLEGLASSFORCE_WEAPON_IMPACT, true);	
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CTaskVehicleGun::GetRestrictedAimSweepHeadingAngleRads(const CPed& rPed, Vector2& vMinMax)
{
	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&rPed);
	if (!pDrivebyInfo)
		return false;

	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo(rPed);
	if (!pWeaponInfo)
		return false;

	const CVehicleDriveByAnimInfo* pDrivebyClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(&rPed, pWeaponInfo->GetHash());
	if (!pDrivebyClipInfo)
		return false;

	vMinMax.x = pDrivebyInfo->GetMinRestrictedAimSweepHeadingAngleDegs(&rPed) * DtoR;
	vMinMax.y = pDrivebyInfo->GetMaxRestrictedAimSweepHeadingAngleDegs(&rPed) * DtoR;

	if (!rPed.IsAPlayerPed())
	{
		const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
		const CVehicle* pVehicle = rPed.GetMyVehicle();
		if (pVehicle && pLocalPlayer && pLocalPlayer->IsInFirstPersonVehicleCamera() && pVehicle->IsSeatIndexValid(pVehicle->GetSeatManager()->GetPedsSeatIndex(pLocalPlayer)))
		{
			const s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
			if (pVehicle->IsSeatIndexValid(iSeatIndex))
			{
				const CFirstPersonDriveByLookAroundData* pFPSLookAroundData = pVehicle->GetVehicleModelInfo()->GetFirstPersonLookAroundData(iSeatIndex);
				if (pFPSLookAroundData)
				{
					vMinMax.x = DtoR * pFPSLookAroundData->m_HeadingLimits.x;
					vMinMax.y = DtoR * pFPSLookAroundData->m_HeadingLimits.y;
				}
			}
		}
	}
	return true;
}

const CWeaponInfo* CTaskVehicleGun::GetPedWeaponInfo(const CPed& rPed)
{
	weaponAssert(rPed.GetWeaponManager());
	u32 uWeaponHash = rPed.GetWeaponManager()->GetEquippedWeaponHash();
	if( uWeaponHash == 0 )
	{
		const CVehicleWeapon* pWeapon = rPed.GetWeaponManager()->GetEquippedVehicleWeapon();
		if( pWeapon )
		{
			uWeaponHash = pWeapon->GetHash();
		}
	}

	return CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
}

void CTaskVehicleGun::ProcessWeaponObject(CPed* pPed)
{
	weaponAssert(pPed->GetWeaponManager());

	if( pPed->GetWeaponManager()->GetEquippedVehicleWeapon() )
		return;

	CPedEquippedWeapon::eAttachPoint attachPoint = pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint();
	CPedEquippedWeapon::eAttachPoint desiredAttachPoint = m_bWeaponInLeftHand ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand;

	if (m_bRestarting && desiredAttachPoint != attachPoint)
	{
		// As we've only restarted the gun state, re-attach same weapon to new attach point
		pPed->GetWeaponManager()->AttachEquippedWeapon(desiredAttachPoint);
	}

	if(pPed->GetWeaponManager()->GetRequiresWeaponSwitch() || ((desiredAttachPoint != attachPoint) && !pPed->GetEquippedWeaponInfo()->GetIsUnarmed()) )
	{
		// Wait for weapon to stream in before we try and create it
		// Streaming is managed by weapon manager.
		// This function is called every frame that we need a weapon object
		// so should get to create it eventually
		if(pPed->GetWeaponManager()->GetEquippedWeaponHash() != 0 && pPed->GetEquippedWeaponInfo() && pPed->GetInventory()->GetIsStreamedIn(pPed->GetWeaponManager()->GetEquippedWeaponHash()))
		{
			pPed->GetWeaponManager()->CreateEquippedWeaponObject(desiredAttachPoint);
			CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if( pWeapon )
				pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
		}
	}
}

CCarDoor* CTaskVehicleGun::GetDoorForPed(const CPed* pPed)
{
	if (!pPed)
		return NULL;

	CVehicle* pVehicle = pPed->GetMyVehicle();
	if (!pVehicle)
		return NULL;

	s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (!pVehicle->IsSeatIndexValid(iSeat))
		return NULL;

	s32 iDirectEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);
	if (!pVehicle->IsEntryIndexValid(iDirectEntryIndex))
		return NULL;

	return CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iDirectEntryIndex);
}

void CTaskVehicleGun::OnCloneTaskNoLongerRunningOnOwner()
{
    SetStateFromNetwork(State_Finish);
}

bool CTaskVehicleMountedWeapon::CanFireAtTarget(CPed* pPed)
{
	CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if( pEquippedWeapon && pPed->IsLocalPlayer() )
	{
		CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());					
		const CEntity* pEntity = targeting.GetFreeAimTargetRagdoll() ? targeting.GetFreeAimTargetRagdoll() : targeting.GetLockOnTarget();
		if( pEntity &&
			pEquippedWeapon->GetWeaponInfo() && 
			!pEquippedWeapon->GetWeaponInfo()->GetIsNonViolent() && 
			!pPed->IsAllowedToDamageEntity(pEquippedWeapon->GetWeaponInfo(), pEntity) )
		{
			return false;
		}

		if (ShouldDimReticule())
		{
			return false;
		}

		if( pPed->GetMyVehicle() &&
			pPed->GetMyVehicle()->GetDriver() == pPed &&
			pPed->GetMyVehicle()->IsShuntModifierActive() )
		{
			return false;
		}
	}

	// Check if ped is within an air defence zone.
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
	{
		return false;
	}

	return true;
}

void CTaskVehicleMountedWeapon::ProcessVehicleWeaponControl(CPed& rPed)
{
	m_bFiredThisFrame = false;

	if(rPed.GetMyVehicle() && rPed.GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(&rPed)!=-1 && rPed.GetMyVehicle()->GetVehicleWeaponMgr())
	{
		switch(GetState())
		{
		case State_Idle:
			ControlVehicleWeapons(&rPed,false);
			break;
		case State_Aiming:
			ControlVehicleWeapons(&rPed,false);
			break;
		case State_Firing:
			ControlVehicleWeapons(&rPed,true);
			break;
		default:
			break;
		}
	}
}

CTaskInfo *CTaskVehicleGun::CreateQueriableState() const
{
    Vector3 targetPosition(VEC3_ZERO);
	if (m_target.GetIsValid())
		m_target.GetPosition(targetPosition);

	return rage_new CClonedVehicleGunInfo(m_mode, m_target.GetEntity(), targetPosition, GetState());
}

void CTaskVehicleGun::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
    Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_VEHICLE_GUN);

    CClonedVehicleGunInfo *vehicleGunInfo = static_cast<CClonedVehicleGunInfo *>(pTaskInfo);

    m_mode                 = vehicleGunInfo->GetMode();

    if(vehicleGunInfo->GetTargetEntity())
    {
        m_target.SetEntity(vehicleGunInfo->GetTargetEntity());
    }
    else
    {
        m_target.SetPosition(vehicleGunInfo->GetTargetPosition());
    }

    // Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskVehicleGun::FSM_Return CTaskVehicleGun::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_Init)
		FSM_OnUpdate
				return Init_OnUpdate();

	FSM_State(State_StreamingClips)
		FSM_OnUpdate
				return StreamingClips_OnUpdate();

	FSM_State(State_SmashWindow)
		FSM_OnEnter
				SmashWindow_OnEnter();
		FSM_OnUpdate
				SmashWindow_OnUpdate();

	FSM_State(State_OpeningDoor)
		FSM_OnEnter
			OpeningDoor_OnEnterClone();
		FSM_OnUpdate
			return OpeningDoor_OnUpdate();		
		FSM_OnExit
			OpeningDoor_OnExitClone();
	
	FSM_State(State_WaitingToFire)
		FSM_OnUpdate
				return WaitingToFire_OnUpdate();

		FSM_State(State_Gun)
		FSM_OnEnter
				Gun_OnEnter();
		FSM_OnUpdate
				return Gun_OnUpdate();

		FSM_State(State_Finish)
		FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

bool CTaskVehicleGun::IsInScope(const CPed* pPed)
{
    bool inScope = CTaskFSMClone::IsInScope(pPed);

	if((!pPed->GetIsInVehicle()) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
    {
        return false;
    }

    weaponAssert(pPed->GetWeaponManager());
    if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHash() == 0)
    {
        return false;
    }

    // weapon updates may be received after the task update starting the task, so we have
    // to wait to prevent the task being aborted in ProcessPreFSM()
	if(!CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(pPed))
	{
		inScope = false;
	}

	// Make sure weapon can be used for driveby
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash());
	weaponAssert(pWeaponInfo);
	if(!pWeaponInfo->CanBeUsedAsDriveBy(pPed) || pWeaponInfo->GetIsThrownWeapon())
	{
		inScope = false;
	}

    return inScope; 
}

float CTaskVehicleGun::GetScopeDistance() const
{
	return static_cast<float>(CTaskGun::CLONE_TASK_SCOPE_DISTANCE);
}

const CWeaponInfo* CTaskVehicleGun::GetPedWeaponInfo() const
{
	return CTaskVehicleGun::GetPedWeaponInfo(*GetPed());
}

#if !__FINAL
void CTaskVehicleGun::Debug() const
{
#if __BANK
	const CPed* pPed = GetPed();

	if (CTaskAimGunVehicleDriveBy::ms_bRenderVehicleGunDebug && IsFiniteAll(RCC_VEC3V(m_vTargetPos)))
	{	
		// Might want to add an aiming offset on here because the clips
		// do not usually point relative to the mover position
		Vector3 vAimFromPosition = m_bAimComputed ? m_vAimStart : VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());	
		if (IsFiniteAll(RCC_VEC3V(vAimFromPosition)))
		{

			// Work out the yaw and pitch angles to the target
			float fDesiredYaw = CTaskAimGun::CalculateDesiredYaw(pPed, vAimFromPosition, m_vTargetPos);
			float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vAimFromPosition, m_vTargetPos);

			grcDebugDraw::AddDebugOutput(Color_green, "CTaskVehicleGun :: Desired Yaw : %.2f, Desired Pitch : %.2f", fDesiredYaw, fDesiredPitch);
			grcDebugDraw::AddDebugOutput(Color_green,"CTaskVehicleGun :: Yaw Min : %.2f, Yaw Max : %.2f", m_fMinYaw, m_fMaxYaw);

			if (CTaskAimGun::ms_bRenderYawDebug)
			{
				// Draw the yaw sweep arc
				CTaskAimGun::RenderYawDebug(pPed, VECTOR3_TO_VEC3V(vAimFromPosition), fDesiredYaw, m_fMinYaw, m_fMaxYaw);
			}

			if (CTaskAimGun::ms_bRenderPitchDebug)
			{
				// Draw the pitch sweep arc
				CTaskAimGun::RenderPitchDebug(pPed, VECTOR3_TO_VEC3V(vAimFromPosition), fDesiredYaw, fDesiredPitch, -QUARTER_PI, QUARTER_PI);
			}
		}
	}

	if (GetSubTask())
	{
		GetSubTask()->Debug();
	}


#endif
}

const char * CTaskVehicleGun::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Init:			return "State_Init";
		case State_StreamingClips:	return "State_StreamingAnims";
		case State_SmashWindow:		return "State_SmashWindow";
		case State_OpeningDoor :    return "State_OpeningDoor";
		case State_WaitingToFire:	return "State_WaitingToFire";
		case State_Gun:				return "State_Gun";
		case State_Finish :			return "State_Finish";
		default: taskAssertf(0, "Missing State Name");
	}

	return "State_Invalid";

}
#endif

bool CTaskVehicleGun::CheckVehicleFiringConditions( const CPed* pPed, Vec3V_In vTargetPosition )
{
	if (pPed->GetMyMount())
		return true; //TODO, need aim camera?

	if (pPed->GetIsParachuting())
		return true; //TODO, need aim camera?

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
		return true; //TODO, need aim camera?

	if (!pPed->GetMyVehicle())	// Should be checked already?
		return false;

	const bool bCantFireWhenLookingDown = pPed->GetMyVehicle()->GetVehicleType() != VEHICLE_TYPE_HELI &&
										  pPed->GetMyVehicle()->GetVehicleType() != VEHICLE_TYPE_PLANE;

	// If the vehicle is leaning over, stop firing
	if(!pPed->IsInFirstPersonVehicleCamera() && pPed->GetMyVehicle()->GetTransform().GetC().GetZf() < 0.707f && bCantFireWhenLookingDown && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim) )
	{
		return false;
	}

	if( pPed->IsLocalPlayer() )
	{
		WeaponControllerState weaponControllerState = CWeaponControllerManager::GetController(CWeaponController::WCT_Player)->GetState(pPed, false);
		if (weaponControllerState == WCS_None)
		{
			taskDisplayf("Frame %i, Local player failed firing conditions because weapon controller state was none", fwTimer::GetFrameCount());
			return false;
		}

		const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
		const bool isAnAimCameraActive = gameplayDirector.IsAiming(pPed->GetMyVehicle());
		if(isAnAimCameraActive)
		{
			// If the camera is pointing down, cant fire.
			const camFrame& aimCameraFrame = camInterface::GetPlayerControlCamAimFrame();
			static dev_float sf_AirDownFireLimit =-0.9f; // B* 1011855 [1/9/2013 musson]
			if(bCantFireWhenLookingDown && aimCameraFrame.GetFront().z < sf_AirDownFireLimit )
			{
				return false;
			}
		}
	}
	else
	{
		const CVehicleWeapon* pEquippedVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		const CWeaponInfo* pWeaponInfo = pEquippedVehicleWeapon ? pEquippedVehicleWeapon->GetWeaponInfo() : NULL;
		if( pWeaponInfo && (pWeaponInfo->GetShouldEnforceAimingRestrictions() || !pPed->IsAPlayerPed()) )
		{
			// Find range of current weapon
			float fHeadingAngularLimit = CPedTargetEvaluator::ms_Tunables.m_DefaultTargetAngularLimitVehicleWeapon;
			float fPitchLimitMin = 0.0f;
			float fPitchLimitMax = 0.0f;

			const CAimingInfo* pAimingInfo = pWeaponInfo->GetAimingInfo();
			if( pAimingInfo )
			{
				fHeadingAngularLimit = pAimingInfo->GetHeadingLimit();
				fPitchLimitMin =pAimingInfo->GetSweepPitchMin();
				fPitchLimitMax =pAimingInfo->GetSweepPitchMax();
			}

			Vector3 vHeading = VEC3V_TO_VECTOR3( pPed->GetTransform().GetB() );
			float fHeadingDifference = 0.0f;
			bool bOutsideMinHeading = false;
			bool bInInclusionRangeFailedHeading = false;
			bool bOnFootHomingTargetInDeadzone = false;
			return CPedTargetEvaluator::IsTargetValid( pPed->GetTransform().GetPosition(), vTargetPosition, 0, vHeading, pWeaponInfo->GetRange(), fHeadingDifference, bOutsideMinHeading, bInInclusionRangeFailedHeading, bOnFootHomingTargetInDeadzone, fHeadingAngularLimit, fPitchLimitMin, fPitchLimitMax );
		}
	}

	return true;
}

void CTaskVehicleGun::CalculateYawAndPitchToTarget(const CPed * pPed, const Vector3& vTarget, float &fYaw, float &fPitch)
{
	Vector3 vToTarget = vTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// Transform direction into peds local space to make it relative to the ped
	vToTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vToTarget)));
	fPitch = rage::Atan2f(vToTarget.z, vToTarget.XYMag());
	vToTarget.z = 0.0f;
	vToTarget.Normalize();
	fYaw = rage::Atan2f(vToTarget.x, vToTarget.y);
	fYaw = fwAngle::LimitRadianAngle(fYaw);
}

//////////////////////////////////////////////////////////////////////////
// Class CTaskVehicleProjectile
fwMvClipId CTaskVehicleProjectile::ms_ThrowGrenadeClipId("throw",0xCDEC4431);

CTaskVehicleProjectile::CTaskVehicleProjectile(eTaskMode mode)
: m_taskMode(mode)
, m_bWantsToHold(true)
, m_fHoldPhase(0.0f)
, m_bSmashedWindow(false)
, m_bThrownGrenade(false)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PROJECTILE);
}

#if !__FINAL
const char * CTaskVehicleProjectile::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Init :			return "State_Init";
		case State_StreamingClips : return "State_StreamingAnims";
		case State_WaitingToFire :	return "State_WaitingToFire";
		case State_SmashWindow :	return "State_SmashWindow";
		case State_Intro :			return "State_Intro";
		case State_Hold :			return "State_Hold";
		case State_Throw :			return "State_Throw";
		case State_Finish :			return "State_Finish";
		default: taskAssertf(0, "Missing State Name in CTaskVehicleProjectile");
	}

	return "State_Invalid";
}
#endif

CTask::FSM_Return CTaskVehicleProjectile::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Check for fire being released
	eTaskInput eDesiredInput = GetInput(pPed);

	if(pPed && !pPed->IsNetworkClone())
	{
		if(eDesiredInput == Input_Hold)
		{
			m_bWantsToHold = true;
			m_bWantsToFire = false;
		}
		else if (eDesiredInput == Input_Fire)
		{
			m_bWantsToHold = false;
			m_bWantsToFire = true;
		} 
		else
		{
			m_bWantsToHold = m_bWantsToFire = false;
		}
	}

	// Ped not in vehicle
	if (!pPed->GetMyVehicle())
	{
		return FSM_Quit;
	}

	// Refresh our clip info stuff each frame
	weaponAssert(pPed->GetWeaponManager());
	if (!CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(pPed))
	{
		return FSM_Quit;
	}

	// Block weapon switching while this task is firing
	pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );

	return FSM_Continue;
}

void CTaskVehicleProjectile::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTaskInfo *CTaskVehicleProjectile::CreateQueriableState() const
{
	return rage_new CClonedVehicleProjectileInfo(GetState(),m_bWantsToHold);
}

void CTaskVehicleProjectile::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_VEHICLE_PROJECTILE);

	CClonedVehicleProjectileInfo *vehicleProjectileInfo = static_cast<CClonedVehicleProjectileInfo *>(pTaskInfo);

	m_bWantsToHold = vehicleProjectileInfo->GetWantsToHold();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskVehicleGun::FSM_Return CTaskVehicleProjectile::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iState != State_StreamingClips && iState != State_Finish && iEvent == OnUpdate)
	{
		s32 currentState = GetState();
		s32 networkState = GetStateFromNetwork();
		if ((networkState != -1) && (networkState > State_Init) && (currentState != networkState))
		{
			if ((currentState > State_Intro) && (currentState != State_Finish)) //allow these states to proceed uninterrupted on remotes - otherwise the animation will not happen properly.
			{
				if ((currentState == State_Hold) && (networkState == State_Intro))
				{
					//disregard as this can lead into a loop between hold->intro->hold on remotes...
				}
				else
				{
					SetState(networkState); //allow the state to be set from the network state
				}
			}
		}
	}

	return UpdateFSM(iState,iEvent);
}

bool CTaskVehicleProjectile::IsInScope(const CPed* pPed)
{
	bool inScope = CTaskFSMClone::IsInScope(pPed);

	if((!pPed->GetIsInVehicle()) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		return false;
	}

	weaponAssert(pPed->GetWeaponManager());
	if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHash() == 0)
	{
		return false;
	}

	// weapon updates may be received after the task update starting the task, so we have
	// to wait to prevent the task being aborted in ProcessPreFSM()
	if(!CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(pPed))
	{
		inScope = false;
	}

	// Make sure weapon can be used for driveby
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash());
	weaponAssert(pWeaponInfo);
	if(!pWeaponInfo->CanBeUsedAsDriveBy(pPed))
	{
		inScope = false;
	}

	return inScope; 
}

CTaskVehicleProjectile::FSM_Return CTaskVehicleProjectile::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Init)
		FSM_OnUpdate
			return Init_OnUpdate(pPed);
	FSM_State(State_StreamingClips)
		FSM_OnUpdate
			return StreamingClips_OnUpdate();
	FSM_State(State_WaitingToFire)
		FSM_OnUpdate
			return WaitingToFire_OnUpdate();
	FSM_State(State_SmashWindow)
		FSM_OnEnter
			SmashWindow_OnEnter(pPed);
		FSM_OnUpdate
			SmashWindow_OnUpdate(pPed);
	FSM_State(State_Intro)
		FSM_OnEnter
			Intro_OnEnter(pPed);
		FSM_OnUpdate
			return Intro_OnUpdate(pPed);
	FSM_State(State_Hold)
		FSM_OnEnter
			Hold_OnEnter(pPed);
		FSM_OnUpdate
			return Hold_OnUpdate(pPed);
	FSM_State(State_Throw)
		FSM_OnEnter
			Throw_OnEnter(pPed);
		FSM_OnUpdate
			return Throw_OnUpdate(pPed);
	FSM_State(State_Finish)
		FSM_OnUpdate
			return Finish_OnUpdate(pPed);
FSM_End
}

void CTaskVehicleProjectile::CleanUp()
{
	weaponAssert(GetPed()->GetWeaponManager());
	GetPed()->GetWeaponManager()->DestroyEquippedWeaponObject();
	if(GetClipHelper())
		GetClipHelper()->StopClip();
}

CTask::FSM_Return CTaskVehicleProjectile::Init_OnUpdate(CPed* pPed)
{
	weaponAssert(pPed->GetWeaponManager());
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();	
	const CVehicleDriveByAnimInfo* pClipInfo = pWeaponInfo ? CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash()) : NULL;
	taskAssert(pClipInfo);

	if( (pPed->IsInFirstPersonVehicleCamera() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false))) && (pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) != CLIP_SET_ID_INVALID))
	{
		m_ClipSet = pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash());
	}
	else
	{
		m_ClipSet = pClipInfo->GetClipSet();
	}

	if (m_ClipSet == CLIP_SET_ID_INVALID)
	{
		SetState(State_Finish);
	}
	else
	{
		SetState(State_StreamingClips);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleProjectile::StreamingClips_OnUpdate()
{
	if (m_ClipSetRequestHelper.Request(m_ClipSet))
	{		
		CPed* pPed = GetPed();
		if (pPed->IsNetworkClone())
		{
			if (GetStateFromNetwork() == State_Finish)
			{
				SetState(State_Finish);
				return FSM_Continue;
			}

			const crClip* pClip = fwClipSetManager::GetClip(m_ClipSet, ms_ThrowGrenadeClipId);
			if (!pClip)
			{
				fwMvClipSetId driveByClipSetID;
				const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();	
				const CVehicleDriveByAnimInfo* pClipInfo = pWeaponInfo ? CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash()) : NULL;
				taskAssert(pClipInfo);

				if( (pPed->IsInFirstPersonVehicleCamera() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false))) && (pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) != CLIP_SET_ID_INVALID))
				{
					driveByClipSetID = pClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash());
				}
				else
				{
					driveByClipSetID = pClipInfo->GetClipSet();
				}

				if (m_ClipSet != driveByClipSetID)
				{
					// Clipset invalid, wait for the weapon to sync
					SetState(State_Init);
					return FSM_Continue;
				}
				else
				{
					return FSM_Continue;
				}
			}
		}
		SetState(State_WaitingToFire);
	}

	return FSM_Continue;
}	

CTask::FSM_Return CTaskVehicleProjectile::WaitingToFire_OnUpdate()
{
	CPed* pPed = GetPed();
	Vector3 vTarget(Vector3::ZeroType);
	
	if(!GetTargetPosition(pPed, vTarget))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(!m_bSmashedWindow)
	{
		if (CTaskVehicleGun::NeedsToSmashWindowFromTargetPos(pPed,vTarget))
		{
			SetState(State_SmashWindow);
			return FSM_Continue;
		}
		else if (CTaskVehicleGun::ClearOutPartialSmashedWindow(pPed, vTarget))
		{
			// We cleared out a partially smashed window
			m_bSmashedWindow = true;
		}
	}

	SetState(State_Intro);
	return FSM_Continue;
}

void CTaskVehicleProjectile::SmashWindow_OnEnter(CPed* pPed)
{
	CVehicle* pVehicle = pPed->GetMyVehicle();
	bool smashWindowAnimFound = true;

	// Check to see if a smash window animation exists, some clipsets are missing them
	if (pVehicle)
	{
		const CVehicleSeatAnimInfo* pVehicleSeatInfo = pVehicle->GetSeatAnimationInfo(pPed);
		s32 iDictIndex = -1;
		u32 iAnimHash = 0;
		if (pVehicleSeatInfo)
		{
			bool bFirstPerson = pPed->IsInFirstPersonVehicleCamera();
			smashWindowAnimFound = pVehicleSeatInfo->FindAnim(CVehicleSeatAnimInfo::SMASH_WINDOW, iDictIndex, iAnimHash, bFirstPerson);
		}
	}

	// Don't try to smash the window if the door is more than half open
	bool bIsVehicleDoorOpen = false;
	if (pVehicle)
	{
		CCarDoor* pDoor = CTaskVehicleGun::GetDoorForPed(pPed);
		if (pDoor && pDoor->GetDoorRatio() > 0.5f)
		{
			bIsVehicleDoorOpen = true;
		}	
	}

	// Don't try to smash the window of a convertible while the roof is being raised or lowered, just roll it down
	bool bIsConvertibleAnimPlaying = pVehicle && pVehicle->DoesVehicleHaveAConvertibleRoofAnimation() && pVehicle->GetConvertibleRoofProgress() > 0.05f && pVehicle->GetConvertibleRoofProgress() < 1.0f;

	if (pVehicle && (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_BULLETPROOF_GLASS) || !smashWindowAnimFound || bIsConvertibleAnimPlaying)) 
	{
		// Find window
		s32 iSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if (pVehicle->IsSeatIndexValid(iSeat))
		{
			int iEntryPointIndex = -1;
			iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeat, pVehicle);

			// Did we find a door?
			if(iEntryPointIndex != -1)
			{
				const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPointIndex);
				Assertf(pEntryPointInfo, "Vehicle is missing entry point");

				eHierarchyId window = pEntryPointInfo->GetWindowId();
				pVehicle->RolldownWindow(window);
			}
		}
	}
	else if (bIsVehicleDoorOpen)
	{
		// Don't create the smash window subtask
	}
	else
	{
		SetNewTask(rage_new CTaskSmashCarWindow(false));
	}

	m_bSmashedWindow = true;
}

CTask::FSM_Return CTaskVehicleProjectile::SmashWindow_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		SetState(State_Intro);
	}

	return FSM_Continue;
}

void CTaskVehicleProjectile::Intro_OnEnter(CPed* pPed)
{
	// start playing the throw clip	
	StartClipBySetAndClip(pPed, m_ClipSet, ms_ThrowGrenadeClipId, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, AP_HIGH, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
}

CTask::FSM_Return CTaskVehicleProjectile::Intro_OnUpdate(CPed* pPed)
{
	// Need to detect the loop 1 enter flag
	// This signifies the end of the intro clip
	CClipHelper* pClipPlayer = GetClipHelper();

	if(pClipPlayer)
	{
		// When the create object event occurs then spawn the grenade in the ped's hand		
		if( pClipPlayer->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Create, true) || pClipPlayer->GetPhase() > 0.9f)
		{
			weaponAssert(pPed->GetWeaponManager());
			pPed->GetWeaponManager()->CreateEquippedWeaponObject();			
			m_fHoldPhase = pClipPlayer->GetPhase();
			SetState(State_Hold);
		}
	}
	else
	{
		// No clip player
		// This is weird
		// Abort
		aiAssertf(false, "No clip player!  This is weird.  Quitting CTaskVehicleProjectile");
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskVehicleProjectile::Hold_OnEnter(CPed* pPed)
{
	weaponAssert(pPed->GetWeaponManager());
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if (pWeapon)
		pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	

	// Make sure the correct clip is playing 
	CClipHelper* pClipPlayer = GetClipHelper();
	bool bForceStartClip = false;
	if(pClipPlayer)
	{
		pClipPlayer->SetRate(0);

		// Make sure the clip player is playing the correct clip, and that its at the right phase
	if(pClipPlayer->GetClipSet() != m_ClipSet) 
		{
			bForceStartClip = true;
		}
		else
		{
			if (m_bWantsToHold) 
			{ 
				//until we get real priming anims?				
				pClipPlayer->SetPhase(m_fHoldPhase);
			}
		}
		
	}
	else
	{
		bForceStartClip = true;
	}

	// Start playing the intro clip and force the phase to the start of the 1st loop
	if(bForceStartClip)
	{
	if (StartClipBySetAndClip(pPed, m_ClipSet, ms_ThrowGrenadeClipId, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, AP_HIGH, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish))	
		{	
			pClipPlayer = GetClipHelper();
			if(aiVerifyf(pClipPlayer,"StartClip returned true but clip player is NULL!"))
			{
				pClipPlayer->SetPhase(m_fHoldPhase);
			}
		}
	}
}

CTask::FSM_Return CTaskVehicleProjectile::Hold_OnUpdate(CPed* pPed)
{
	if (pPed && !pPed->IsNetworkClone())
	{
		if(!m_bWantsToHold && !m_bWantsToFire)
		{		
			SetState(State_Finish);	
		}
		else if (m_bWantsToFire)
		{		
			SetState(State_Throw);	
		}
	}

	weaponAssert(pPed->GetWeaponManager());
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	const float fFuseTime = pWeapon ? pWeapon->GetWeaponInfo()->GetProjectileFuseTime(true) : 0.0f;	
	if ( fFuseTime > 0.0f && pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()) <= 0 ) 
	{
		//BOOM!
		m_bThrownGrenade = ThrowProjectileImmediately(pPed);		
		SetState(State_Finish);			
	}

	const CControl* pControl = pPed->GetControlFromPlayer();
	// See if player wants to change weapons
	if(pControl && (pControl->GetVehicleSelectNextWeapon().IsPressed() || pControl->GetVehicleSelectPrevWeapon().IsPressed()) && pWeapon)
	{		
		pWeapon->CancelCookTimer();

		if(pControl->GetVehicleSelectNextWeapon().IsPressed())
		{
			pPed->GetWeaponManager()->GetWeaponSelector()->SelectWeapon(pPed, SET_NEXT);
		}
		else
		{
			pPed->GetWeaponManager()->GetWeaponSelector()->SelectWeapon(pPed, SET_PREVIOUS);
		}

		SetState(State_Finish);	
	}

	// Else we continue looping the hold clip
	CClipHelper* pClipPlayer = GetClipHelper();
	if (pClipPlayer && m_bWantsToHold) 
	{ 
		//until we get real priming anims		
		pClipPlayer->SetPhase(m_fHoldPhase);
	}
	
	return FSM_Continue;
}

void CTaskVehicleProjectile::Throw_OnEnter(CPed* pPed)
{
	// Start playing the throw clip

		// Make sure the correct clip is playing
		CClipHelper* pClipPlayer = GetClipHelper();
		bool bForceStartClip = false;
		if(pClipPlayer)
		{
			pClipPlayer->SetRate(1.0f);

			// Make sure the clip player is playing the correct clip, and that its at the right phase
			if(pClipPlayer->GetClipSet() != m_ClipSet) 
			{
				bForceStartClip = true;
			}
		}
		else
		{
			bForceStartClip = true;
		}

		// Start playing the intro clip and force the phase to the start of the 1st loop
		if(bForceStartClip)
		{
			if (StartClipBySetAndClip(pPed, m_ClipSet, ms_ThrowGrenadeClipId, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, AP_HIGH, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish))		
			{	
				pClipPlayer = GetClipHelper();
				aiAssertf(pClipPlayer, "StartClip returned true but clip player is NULL!");
			}
		}
	}

CTask::FSM_Return CTaskVehicleProjectile::Throw_OnUpdate(CPed* pPed)
{
	// When the fire clip event happens, throw the projectile
	const CClipHelper* pClipPlayer = GetClipHelper();
	if(!pClipPlayer || GetIsFlagSet(aiTaskFlags::AnimFinished))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}
	
	if(!m_bThrownGrenade && (pClipPlayer->GetLastPhase() < pClipPlayer->GetPhase()) && pClipPlayer->IsEvent(CClipEventTags::Fire))
	{
		// Time to throw the projectile
		m_bThrownGrenade = ThrowProjectileImmediately(pPed);
	}

	return FSM_Continue;
}

bool CTaskVehicleProjectile::GetTargetPosition(const CPed* pPed, Vector3& vTargetPosWorldOut)
{
	Vector3 vTrajectory;
	s32 seat = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);

	int iSeatBoneIndex = pPed->GetMyVehicle()->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(seat);
	Assert(iSeatBoneIndex > -1);

	const crBoneData* pBoneData = pPed->GetMyVehicle()->GetSkeletonData().GetBoneData(iSeatBoneIndex);
	Assert(pBoneData);
	bool bIsLeft = pBoneData->GetDefaultTranslation().GetXf() < 0.0f || pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats() <= 1;

	weaponAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon && pWeapon->GetWeaponInfo()->GetIsThrownWeapon())
	{
		int nBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
		Assert(nBoneIndex != BONETAG_INVALID);

		Matrix34 weaponMat;
		pPed->GetGlobalMtx(nBoneIndex, weaponMat);

		float fProjectileThrownFromVehicleVelocity = 5.0f;

		const CAmmoInfo* pAmmoInfo = pWeapon->GetWeaponInfo()->GetAmmoInfo();
		if(pAmmoInfo && pAmmoInfo->GetIsClassId(CAmmoThrownInfo::GetStaticClassId()))
		{
			const CAmmoThrownInfo* pAmmoProjectileInfo = static_cast<const CAmmoThrownInfo*>(pAmmoInfo);
			fProjectileThrownFromVehicleVelocity = pAmmoProjectileInfo->GetThrownForceFromVehicle();			
		}

		if( bIsLeft)
		{
			vTrajectory = Vector3(-fProjectileThrownFromVehicleVelocity, 0.0f, 0.0f);
		}
		else
		{
			vTrajectory = Vector3(fProjectileThrownFromVehicleVelocity, 0.0f, 0.0f);
		}

		vTrajectory.RotateZ(pPed->GetMyVehicle()->GetTransform().GetHeading());
		vTrajectory += pPed->GetMyVehicle()->GetVelocity();
		vTargetPosWorldOut = weaponMat.d + vTrajectory;		
	}
	else
	{
		if( bIsLeft)
		{
			vTrajectory = Vector3(-30.0f, 0.0f, 0.0f);
		}
		else
		{
			vTrajectory = Vector3(30.0f, 0.0f, 0.0f);
		}
		vTrajectory.RotateZ(pPed->GetMyVehicle()->GetTransform().GetHeading());

		vTargetPosWorldOut = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vTrajectory;		
	}

	return true;
}

bool CTaskVehicleProjectile::ThrowProjectileImmediately(CPed* pPed)
{
	bool bRet = false;

	weaponAssert(pPed->GetWeaponManager());
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon && pWeapon->GetWeaponInfo()->GetIsThrownWeapon())
	{
		Vector3 vEnd;
		GetTargetPosition(pPed, vEnd);

		int nBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
		Assert(nBoneIndex != BONETAG_INVALID);
		Matrix34 weaponMat;
		pPed->GetGlobalMtx(nBoneIndex, weaponMat);

		float fOverrideLifeTime = -1.0f;
		const CAmmoInfo* pAmmoInfo = pWeapon->GetWeaponInfo()->GetAmmoInfo();
		if(pAmmoInfo && pAmmoInfo->GetIsClassId(CAmmoThrownInfo::GetStaticClassId()))
		{
			const CAmmoThrownInfo* pAmmoProjectileInfo = static_cast<const CAmmoThrownInfo*>(pAmmoInfo);			
			fOverrideLifeTime = pAmmoProjectileInfo->GetFromVehicleLifeTime();
		}

		if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
			fOverrideLifeTime = Max(0.0f, pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()));

		// Throw the projectile
		CWeapon::sFireParams params(pPed, weaponMat, &weaponMat.d, &vEnd);
		params.fOverrideLifeTime = fOverrideLifeTime;
		if(pWeapon->Fire(params))
		{
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
			bRet = true;
		}

		// Check ammo
		s32 iAmmo = pPed->GetInventory()->GetWeaponAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo());
		if( iAmmo == 0 )
		{
			pPed->GetWeaponManager()->EquipBestWeapon();
		}
	}
	return bRet;
}

CTaskVehicleProjectile::eTaskInput CTaskVehicleProjectile::GetInput(CPed* pPed)
{
	eTaskInput input = Input_None;
	switch(m_taskMode)
	{
	case Mode_Player:
		{
			if(pPed->IsLocalPlayer())
			{
				CControl* pControl = pPed->GetControlFromPlayer();
				if(aiVerifyf(pControl,"NULL CControl from a player CPed"))
				{
					bool bVehicleAttackReleased = pControl->IsVehicleAttackInputReleased();
					bool bVehicleAimDown = pControl->GetVehicleAim().IsDown();
					if(bVehicleAttackReleased)
					{
						input = Input_Fire;
					}
					else if(bVehicleAimDown)
					{
						input = Input_Hold;
					}	

					// Passenger can use standard ped attack buttons
					if(taskVerifyf(pPed->GetMyVehicle(),"Unexpected NULL vehicle pointer"))
					{
						if(pPed->GetMyVehicle()->ContainsPed(pPed) && !pPed->GetMyVehicle()->IsDriver(pPed) && input != Input_Fire)
						{
							if(pControl->GetPedTargetIsDown() || pControl->GetPedAttack().IsDown() || pControl->GetPedAttack2().IsDown())
							{
								input = Input_Hold;
							}
							if(pControl->GetPedTargetIsReleased() || pControl->GetPedAttack().IsReleased() || pControl->GetPedAttack2().IsReleased())
							{
								input = Input_Fire;
							}
						}
					}					
				}
				break;
			}			
		}
	case Mode_Fire:
		{
			return Input_Fire;
		}
	default:
		taskAssertf(false,"Unhandled mode in CTaskVehicleProjectile::GetInput");
	}

	return input;
}

//////////////////////////////////////////////////////////////////////////
// CClonedVehicleProjectileInfo
//////////////////////////////////////////////////////////////////////////
CTaskFSMClone * CClonedVehicleProjectileInfo::CreateCloneFSMTask()
{	
	return rage_new CTaskVehicleProjectile(CTaskVehicleProjectile::Mode_Player);
}
