#if __BANK

// File headers
#include "Vehicles/Metadata/VehicleDebug.h"

// Rage headers
#include "Bank/BkMgr.h"
#include "fwdecorator/decoratorExtension.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Game/ModelIndices.h"
#include "Debug/DebugScene.h"
#include "ModelInfo/PedModelInfo.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/Objects/Entities/NetObjVehicle.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedFactory.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Rendering/PedVariationPack.h"
#include "Physics/Physics.h"
#include "Scene/Entity.h"
#include "Scene/World/GameWorld.h"
#include "Script/commands_player.h"
#include "Streaming/streaming.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/Climbing/TaskRappel.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Vehicles/Door.h"
#include "Vehicles/vehicle_channel.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/VehicleDefines.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Weapons/Explosion.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

// Spawn A Ped
RegdPed		CVehicleDebug::ms_pLastCreatedPed(NULL);
RegdVeh		CVehicleDebug::ms_pPersonalVehicle(NULL);
RegdPed		CVehicleDebug::ms_pPersonalVehicleOwner(NULL);
bool		CVehicleDebug::ms_bUseSpecificSpawnLocation = true;
bool		CVehicleDebug::ms_bPutInPlayersGroup		= false;

int			CVehicleDebug::ms_iPreferredPassengerSeatIndex = 1;
bool		CVehicleDebug::ms_bUseSpawnOffset			= false;
float		CVehicleDebug::ms_fSpawnOffsetX				= 0.0f;
float		CVehicleDebug::ms_fSpawnOffsetY				= 0.0f;
u16			CVehicleDebug::ms_iPlayerJackRate			= 100;
atArray<s32> CVehicleDebug::ms_aDesiredStates;
bool		CVehicleDebug::ms_bAttachToBike				= false;
bool		CVehicleDebug::ms_bTestOnBike				= true;
bool		CVehicleDebug::ms_bUseDefaultAimExitStateList = true;
bool		CVehicleDebug::ms_bUseDefaultStateList		= false;
bool		CVehicleDebug::ms_bUseDefaultBikeStateList	= false;
bool		CVehicleDebug::ms_bUseDefaultPlaneStateList	= false;
bool		CVehicleDebug::ms_bDisableGroundAndOrientationFixup = false;
bool		CVehicleDebug::ms_bUseFastBikeAnims			= false;

// Vehicle Tasks
s32			CVehicleDebug::ms_iSelectedTaskState		= CTaskAttachedVehicleAnimDebug::State_OpenDoor;
s32			CVehicleDebug::ms_iSelectedTask				= 0;
s32			CVehicleDebug::ms_eSeatRequestType			= SR_Specific;
s32			CVehicleDebug::ms_iSeatRequested			= 0;
s32			CVehicleDebug::ms_iEntryPoint				= 0;
s32			CVehicleDebug::ms_iShuffleLink				= 0;

// Enter/Exit Flags
bool		CVehicleDebug::ms_bUseDirectEntryOnly		= false;
bool		CVehicleDebug::ms_bJumpOut					= false;
bool		CVehicleDebug::ms_bDelayForTime				= false;
bool		CVehicleDebug::ms_bJustPullPedOut			= false;
bool		CVehicleDebug::ms_bJackAnyOne				= false;
bool		CVehicleDebug::ms_bWarpAfterTime			= false;
bool		CVehicleDebug::ms_bWarpIfDoorIsBlocked		= false;
bool		CVehicleDebug::ms_bJustOpenDoor				= false;
bool		CVehicleDebug::ms_bDontCloseDoor			= false;
bool		CVehicleDebug::ms_bWarpToEntryPoint			= false;
bool		CVehicleDebug::ms_bWaitForEntryToBeClear	= false;
bool		CVehicleDebug::ms_bWarp						= false;

float		CVehicleDebug::ms_fDelayTime				= 0.0f;
float		CVehicleDebug::ms_fWarpTime					= 0.0f;
float		CVehicleDebug::ms_fMoveBlendRatio			= MOVEBLENDRATIO_RUN;

float		CVehicleDebug::ms_fAimBlendDirection		= 0.0f;

// Drive By Params
s32			CVehicleDebug::ms_iDriveByMode = CTaskVehicleGun::Mode_Aim;
CAITarget	CVehicleDebug::ms_PedTarget;

// Forced Vehicle Seat Usage
bool		CVehicleDebug::ms_ForceUseRearSeatsOnly = false;
bool		CVehicleDebug::ms_ForceUseFrontSeatsOnly = false;
bool		CVehicleDebug::ms_ForceForAnyVehicle = false;
s32			CVehicleDebug::ms_SelectedForcedVehicleUsageSlot = 0;
s32			CVehicleDebug::ms_iSeatIndex = -1;

// Debug rendering
bool	CVehicleDebug::ms_bRenderDebug							= false;
bool	CVehicleDebug::ms_renderEnterTurret						= false;
bool	CVehicleDebug::ms_bRenderExclusiveDriverDebug			= false;
bool	CVehicleDebug::ms_bRenderLastDriverDebug				= false;
bool	CVehicleDebug::ms_bRenderEntryPoints					= false;
bool	CVehicleDebug::ms_bRenderLayoutInfo						= false;
bool	CVehicleDebug::ms_bRenderEntryAnim						= false;
bool	CVehicleDebug::ms_bRenderExitAnim						= false;
bool	CVehicleDebug::ms_bRenderThroughWindscreenAnim			= false;
bool	CVehicleDebug::ms_bRenderPickUpAnim						= false;
bool	CVehicleDebug::ms_bRenderSeats							= false;
bool	CVehicleDebug::ms_bRenderEntrySeatLinks					= false;
bool	CVehicleDebug::ms_bRenderComponentReservationText		= false;
bool	CVehicleDebug::ms_bRenderComponentReservationUsage		= false;
bool	CVehicleDebug::ms_bRenderEntryCollisionCheck			= false;
bool	CVehicleDebug::ms_bRenderEntryPointEvaluationDebug		= false;
bool	CVehicleDebug::ms_bRenderAnimStreamingDebug				= false;
bool	CVehicleDebug::ms_bRenderAnimStreamingText				= false;
s32		CVehicleDebug::ms_iDebugVerticalOffset					= 10;
bool	CVehicleDebug::ms_bRenderEntryPointEvaluationTaskDebug	= false;
bool	CVehicleDebug::ms_bRenderDriveByTaskDebug				= false;
bool	CVehicleDebug::ms_bRenderDriveByYawLimitsForPed			= false;
bool	CVehicleDebug::ms_bOutputSeatRelativeOffsets			= true;
s32		CVehicleDebug::ms_iDebugEntryPoint						= -1;
char	CVehicleDebug::ms_szInVehicleContextOverride[128];
bool	CVehicleDebug::ms_bForcePlayerToUseSpecificSeat			= false;
CRidableEntryPointEvaluationDebugInfo CVehicleDebug::ms_EntryPointsDebug;
bool	CVehicleDebug::ms_bRenderInitialSpawnPoint				= false;

// Vehicle locks
s32		CVehicleDebug::ms_iVehicleLockState						= CARLOCK_NONE;
bool	CVehicleDebug::ms_bRepairOnDebugVehicleLock				= false;
float	CVehicleDebug::ms_fDesiredTurretHeading					= 0.0f;
float	CVehicleDebug::ms_fDesiredTurretPitch					= 0.0f;

// Anim Streaming
bool	CVehicleDebug::ms_bLoadAnimsWithVehicle					= true;

u32		CVehicleDebug::ms_ExclusiveDriverIndex					= 0;

// Bank Stuff
bkBank* CVehicleDebug::ms_pBank									= NULL;
bkCombo* CVehicleDebug::ms_pTasksCombo							= NULL;
bkButton* CVehicleDebug::ms_pCreateButton						= NULL;

PARAM(visualisepedsinvehicle, "Visualise peds in vehicles from startup");

#if __BANK
const char* VehicleLockStateStrings[] =
{
	"CARLOCK_NONE", 
	"CARLOCK_UNLOCKED", 
	"CARLOCK_LOCKED",
	"CARLOCK_LOCKOUT_PLAYER_ONLY",
	"CARLOCK_LOCKED_PLAYER_INSIDE",
	"CARLOCK_LOCKED_INITIALLY",		// for police cars and suchlike 
	"CARLOCK_FORCE_SHUT_DOORS",		// not locked", but force player to shut door after they getout
	"CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED",
	"CARLOCK_LOCKED_BUT_BOOT_UNLOCKED",
	"CARLOCK_LOCKED_NO_PASSENGERS",
	"CARLOCK_CANNOT_ENTER",
	"CARLOCK_PARTIALLY",
	"CARLOCK_LOCKED_EXCEPT_TEAM1",	// car lock states for only allowing certain teams to use a vehicle
	"CARLOCK_LOCKED_EXCEPT_TEAM2",
	"CARLOCK_LOCKED_EXCEPT_TEAM3",
	"CARLOCK_LOCKED_EXCEPT_TEAM4",
	"CARLOCK_LOCKED_EXCEPT_TEAM5",
	"CARLOCK_LOCKED_EXCEPT_TEAM6",
	"CARLOCK_LOCKED_EXCEPT_TEAM7",
	"CARLOCK_LOCKED_EXCEPT_TEAM8"
};

CompileTimeAssert(COUNTOF(VehicleLockStateStrings) == CARLOCK_NUM_STATES);
#endif

const char* SeatRequestTypeStrings[] = {
	"SR_Any",
	"SR_Specific",
	"SR_Prefer"
};

const char* DriveByModeStrings[] = {
	"Mode_Player",
	"Mode_Aim",
	"Mode_Fire"
};

void CVehicleDebug::InitBank()
{
	if(ms_pBank)
	{
		ShutdownBank();
	}

	CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualisePedsInVehicles |= PARAM_visualisepedsinvehicle.Get();

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("Vehicle Ped Tasks", 0, 0, false); 
	if(vehicleVerifyf(ms_pBank, "Failed to create vehicle ped tasks bank"))
	{
		ms_pCreateButton = ms_pBank->AddButton("Create vehicle ped tasks widgets", datCallback(CFA1(CVehicleDebug::AddWidgets), ms_pBank));
	}
}

void CVehicleDebug::AddWidgets(bkBank& bank)
{
	// destroy first widget which is the create button
	if(!ms_pCreateButton)
		return;
	ms_pCreateButton->Destroy();
	ms_pCreateButton = NULL;

	// General widgets
	InitWidgets(bank);

	// Vehicle info widgets
	CVehicleMetadataMgr::InitWidgets(bank);
}

void CVehicleDebug::ShutdownBank()
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
}

void CVehicleDebug::SpawnPedCB()
{
	u32 PedModelIndex = MI_PED_COP; 

	Vector3 vSpawnPos = GenerateSpawnLocation();

	// Ensure that the model is loaded and ready for drawing for this ped.
	fwModelId modelId((strLocalIndex(PedModelIndex)));
	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	// Create the ped.
	CreatePed(PedModelIndex, vSpawnPos);
	atFixedBitSet32 incs;
	atFixedBitSet32 excs;
	ms_pLastCreatedPed->SetVarRandom(ms_pLastCreatedPed->IsPlayer(), PV_FLAG_NONE, &incs, &excs, NULL, PV_RACE_UNIVERSAL);
	CPedVariationPack::CheckVariation(ms_pLastCreatedPed);
	CPedPropsMgr::SetPedPropsRandomly(ms_pLastCreatedPed, 0.4f, 0.25f, PV_FLAG_NONE, PV_FLAG_NONE, &incs, &excs);
	CPedVariationPack::CheckVariation(ms_pLastCreatedPed);

	if (ms_bPutInPlayersGroup)
	{
		ms_pLastCreatedPed->GetPedIntelligence()->SetRelationshipGroup(CRelationshipManager::s_pPlayerGroup);
	}

	ms_pLastCreatedPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_PRIMARY, rage_new CTaskDoNothing(-1), PED_TASK_PRIORITY_DEFAULT);
	ms_pLastCreatedPed->SetBlockingOfNonTemporaryEvents( true);
}

void CVehicleDebug::SpawnVehicleAndPedCB()
{
	CVehicleFactory::CreateCar();
	SpawnPedCB();
}

Vector3 CVehicleDebug::GenerateSpawnLocation()
{
	Vector3 vSpawnPos(Vector3::ZeroType);
	if (ms_bUseSpawnOffset)
	{
		if (CVehicleFactory::ms_pLastCreatedVehicle)
		{
			Vector3 vOffset(ms_fSpawnOffsetX, ms_fSpawnOffsetY, 0.0f);
			vOffset.RotateZ(CVehicleFactory::ms_pLastCreatedVehicle->GetTransform().GetHeading());
			vSpawnPos = VEC3V_TO_VECTOR3(CVehicleFactory::ms_pLastCreatedVehicle->GetTransform().GetPosition()) + vOffset;
		}
	}
	else if (ms_bUseSpecificSpawnLocation && CPhysics::GetMeasuringToolPos(0, vSpawnPos))
	{	
	}
	else
	{
		// Generate a tentative location to create the ped from the camera position & orientation
		Vector3 destPos = camInterface::GetPos();
		Vector3 viewVector = camInterface::GetFront();
		destPos.Add(viewVector*(3.0f));		// create at position in front of current camera position
		vSpawnPos.x = destPos.GetX() + fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		vSpawnPos.y = destPos.GetY() + fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		vSpawnPos.z = destPos.GetZ();
	}

	// Do a test probe against the world at the location to find the ground to set the ped on.
	Vector3 vTestFromPos(vSpawnPos.x, vSpawnPos.y, camInterface::GetPos().z + 1.0f);
	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vTestFromPos, Vector3(vTestFromPos.x, vTestFromPos.y, -1000.0f));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		// Set the actual position to create the ped at.
		vSpawnPos.z = probeResult[0].GetHitPosition().z;
		vSpawnPos.z += 1.0f;
	}

	return vSpawnPos;
}

void CVehicleDebug::SetFocusPedInSeatCB()
{
	CPed* pWarpPed = CGameWorld::FindLocalPlayer();

	pWarpPed->GetPedIntelligence()->FlushImmediately();
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pWarpPed = static_cast<CPed*>(pFocusEntity);
	}

	static s32 iFlags = 0;
	pWarpPed->SetPedInVehicle(CVehicleFactory::ms_pLastCreatedVehicle, CVehicleFactory::ms_iSeatToEnter, iFlags);
}

void CVehicleDebug::ToggleAICanUseExclusiveSeatsCB()
{
	CVehicle* pFocusVehicle = NULL;

	// Get the focus entities
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	CEntity* pFocusEntity2 = CDebugScene::FocusEntities_Get(1);

	// Select the vehicle from the first focus entity
	if (pFocusEntity1 && pFocusEntity1->GetIsTypeVehicle())
	{
		pFocusVehicle = static_cast<CVehicle*>(pFocusEntity1);
	}

	// Select the vehicle from the second focus entity
	if (!pFocusVehicle)
	{
		if (pFocusEntity2 && pFocusEntity2->GetIsTypeVehicle())
		{
			pFocusVehicle = static_cast<CVehicle*>(pFocusEntity2);
		}
	}

	if (!pFocusVehicle)
	{
		pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pFocusVehicle)
	{
		pFocusVehicle->m_nVehicleFlags.bAICanUseExclusiveSeats = !pFocusVehicle->m_nVehicleFlags.bAICanUseExclusiveSeats;
	}
}

void CVehicleDebug::SetFocusPedExclusiveDriverCB()
{
	CPed* pFocusPed = NULL;
	CVehicle* pFocusVehicle = NULL;

	// Get the focus entities
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	CEntity* pFocusEntity2 = CDebugScene::FocusEntities_Get(1);

	// Select the ped from the first focus entity
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity1);
	}

	// Select the ped from the second focus entity
	if (!pFocusPed)
	{
		if (pFocusEntity2 && pFocusEntity2->GetIsTypePed())
		{
			pFocusPed = static_cast<CPed*>(pFocusEntity2);
		}
	}

	// If unsuccessful try to use the last ped created through the widgets
	if (!pFocusPed)
	{
		pFocusPed = ms_pLastCreatedPed.Get();
	}

	// If still no success, use the player
	if (!pFocusPed)
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	// Select the vehicle from the first focus entity
	if (pFocusEntity1 && pFocusEntity1->GetIsTypeVehicle())
	{
		pFocusVehicle = static_cast<CVehicle*>(pFocusEntity1);
	}

	// Select the vehicle from the second focus entity
	if (!pFocusVehicle)
	{
		if (pFocusEntity2 && pFocusEntity2->GetIsTypeVehicle())
		{
			pFocusVehicle = static_cast<CVehicle*>(pFocusEntity2);
		}
	}

	if (!pFocusVehicle)
	{
		pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pFocusPed && pFocusVehicle)
	{
		if(NetworkUtils::IsNetworkCloneOrMigrating(pFocusVehicle))
		{
			ObjectId exclusiveDriverID = NETWORK_INVALID_OBJECT_ID;

			if(pFocusPed->GetNetworkObject())
			{
				exclusiveDriverID = pFocusPed->GetNetworkObject()->GetObjectID();
			}

			CScriptEntityStateChangeEvent::CSetVehicleExclusiveDriver parameters(exclusiveDriverID, ms_ExclusiveDriverIndex);
			CScriptEntityStateChangeEvent::Trigger(pFocusVehicle->GetNetworkObject(), parameters);
		}
		else
		{
			if(pFocusVehicle->GetExclusiveDriverPed(ms_ExclusiveDriverIndex) == pFocusPed)
			{
				pFocusVehicle->SetExclusiveDriverPed(NULL, ms_ExclusiveDriverIndex);
			}
			else
			{
				pFocusVehicle->SetExclusiveDriverPed(pFocusPed, ms_ExclusiveDriverIndex);
			}
		}
	}
}

void CVehicleDebug::CreatePed(u32 pedModelIndex, const Vector3& pos)
{
	Matrix34 TempMat;

	TempMat.Identity();
	TempMat.d = pos;

	// don't allow the creation of player ped type - it doesn't work!
	if (pedModelIndex == CGameWorld::FindLocalPlayer()->GetModelIndex()) 
	{
		return;
	}
	
	fwModelId pedModelId((strLocalIndex(pedModelIndex)));
	// ensure that the model is loaded and ready for drawing for this ped
	if (!CModelInfo::HaveAssetsLoaded(pedModelId))
	{
		CModelInfo::RequestAssets(pedModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

#if __ASSERT
	CPedModelInfo * pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	aiAssert(pPedModelInfo);
#endif

	// create the debug ped which will have it's textures played with
	const CControlledByInfo localAiControl(false, false);

	if (ms_pLastCreatedPed)
	{
		ms_pLastCreatedPed = NULL;
	}

	ms_pLastCreatedPed = CPedFactory::GetFactory()->CreatePed(localAiControl, pedModelId, &TempMat, false, true, true);

	if (aiVerifyf(ms_pLastCreatedPed, "CEnterVehicleDebug.cpp : CreatePed() - Couldn't create a the ped variation debug ped"))
	{
		// use the settings in the widget for this ped
		ms_pLastCreatedPed->SetVariation(static_cast<ePedVarComp>(0), 0, 0, 0, 0, 0, false);

		ms_pLastCreatedPed->PopTypeSet(POPTYPE_TOOL);
		ms_pLastCreatedPed->SetDefaultDecisionMaker();
		ms_pLastCreatedPed->SetCharParamsBasedOnManagerType();
		ms_pLastCreatedPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

		float fNewHeading = fwRandom::GetRandomNumberInRange(-PI, PI);

		Vector3 v0, v1;
		if (ms_bUseSpecificSpawnLocation && CPhysics::GetMeasuringToolPos(0, v0) && CPhysics::GetMeasuringToolPos(1, v1))
		{	
			Vector3 vDir = v1 - v0;
			vDir.Normalize();
			fNewHeading = rage::Atan2f(-vDir.x, vDir.y);
		}

		ms_pLastCreatedPed->SetHeading(fNewHeading);
		ms_pLastCreatedPed->SetDesiredHeading(fNewHeading);
		ms_pLastCreatedPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskDoNothing(-1));

		CGameWorld::Add(ms_pLastCreatedPed, CGameWorld::OUTSIDE );
	}
}

CTaskTypes::eTaskType iVehicleTestHarnessTaskTypes[] =
{
	CTaskTypes::TASK_ENTER_VEHICLE,
	CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE,
	CTaskTypes::TASK_EXIT_VEHICLE,
	CTaskTypes::TASK_VEHICLE_GUN,
	CTaskTypes::TASK_ATTACHED_VEHICLE_ANIM_DEBUG,
	CTaskTypes::TASK_HELI_PASSENGER_RAPPEL,
	CTaskTypes::TASK_NONE
};

//copied from TaskPlayer GetInVehicle_OnEnter to get some good debug
void CVehicleDebug::RenderEnterTurretSeats()
{
	const CVehicle* m_pTargetVehicle = GetFocusVehicle();
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	if(m_pTargetVehicle && pPlayerPed)
	{
		s32	iSeat = m_pTargetVehicle->GetDriverSeat();
		const bool bHasTurret = m_pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);

		bool bShouldPreferTurretSeat = false;
		int iBestTurretSeat = -1;

		const CPed* pDriver = CTaskVehicleFSM::GetPedInOrUsingSeat(*m_pTargetVehicle, iSeat);
		if( bHasTurret && (pDriver == NULL || pDriver->GetPedIntelligence()->IsFriendlyWith(*pPlayerPed)) )
		{
			int iBestSeatIndex = -1;
			CVehicleModelInfo* pVehicleModelInfo = m_pTargetVehicle->GetVehicleModelInfo();
			if(pVehicleModelInfo)
			{
				Vector3 fPedForward = pPlayerPed->GetVelocity();
				float fPedSpeed = fPedForward.XYMag();
				fPedForward /= fPedSpeed;
				Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
				Vector3 vPedHeading = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetForward());
				float fClosestDist = 10000.0f;
				const s32 iNumSeats = pVehicleModelInfo->GetModelSeatInfo()->GetNumSeats();
				char szText[64];
				for( s32 seatIndex = 0; seatIndex < iNumSeats; seatIndex++ )
				{				
					Vector3 vOpenDoorPos;
					Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
					CModelSeatInfo::CalculateEntrySituation(m_pTargetVehicle, pPlayerPed, vOpenDoorPos, qEntryOrientation, seatIndex);		
					float fDistanceToEntryPoint = vPedPosition.Dist2(vOpenDoorPos);
					float fModifiedDist = fDistanceToEntryPoint;

					grcDebugDraw::Sphere(vOpenDoorPos, 0.1f, Color_green);	
					if(fPedSpeed > 1.0f && m_pTargetVehicle->GetIsAircraft())
					{	
						Vector3 vOpenDoorPosFlat = vOpenDoorPos;
						vOpenDoorPosFlat.z = vPedPosition.z;
						Vector3 vPedToDoor = vOpenDoorPosFlat - vPedPosition;						
						vPedToDoor.Normalize();
						float fToDoorDot = fPedForward.Dot(vPedToDoor);

						float fDistToLine = geomDistances::DistanceLineToPoint(vPedPosition, vPedHeading * 20.0f, vOpenDoorPosFlat);
						if(fToDoorDot > 0.0f)
						{
							fModifiedDist += fDistToLine * ( fDistToLine > 0.25f ? 5.0f : 0.0f );
						}

						formatf(szText, "Seat %d: Distance %.2f, Dot %.2f, Offset %.2f, MOD %.2f", seatIndex, fDistanceToEntryPoint, fToDoorDot, fDistToLine, fModifiedDist);
						grcDebugDraw::Text(vOpenDoorPos + Vector3(0.0f,0.0f, 0.25f), Color_green, 0, 0, szText);
					}

					if(fModifiedDist < fClosestDist)
					{
						fClosestDist = fModifiedDist;
						iBestSeatIndex = seatIndex;
					}
				}

				if(fPedSpeed > 1.0f)
				{	
					grcDebugDraw::Line(vPedPosition, vPedPosition + vPedHeading * 20.0f, Color_green);	
				}

				if(iBestSeatIndex != -1)
				{
					//check we can actually enter this seat
					const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicleModelInfo->GetSeatAnimationInfo(iBestSeatIndex);
					if(pSeatAnimInfo && pSeatAnimInfo->IsTurretSeat())
					{
						const CPed* pTurretOccupier = m_pTargetVehicle->GetSeatManager()->GetPedInSeat(iBestSeatIndex);
						if(!pTurretOccupier || !CTaskEnterVehicle::ms_Tunables.m_OnlyJackDeadPedsInTurret || pTurretOccupier->ShouldBeDead() || pTurretOccupier->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE))
						{
							bShouldPreferTurretSeat = true;
							iBestTurretSeat = iBestSeatIndex;
						}
					}
				}
			}

			Vector3 vOpenDoorPos;
			Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
			CModelSeatInfo::CalculateEntrySituation(m_pTargetVehicle, pPlayerPed, vOpenDoorPos, qEntryOrientation, iBestTurretSeat == -1 ? 0 : iBestTurretSeat);		
			grcDebugDraw::Sphere(vOpenDoorPos, 0.15f, Color_red);	
		}
	}
}

void CVehicleDebug::SetFocusPedPreferredPassengerSeatIndexCB()
{
	CPed* pPed = GetFocusPed();
	if (pPed)
	{
		pPed->m_PedConfigFlags.SetPassengerIndexToUseInAGroup(ms_iPreferredPassengerSeatIndex);
	}
}

void CVehicleDebug::MakeFocusPedPopTypeScriptCB()
{
	CPed* pPed = GetFocusPed();
	if (pPed)
	{
		pPed->PopTypeSet(POPTYPE_MISSION);
	}
}

void CVehicleDebug::SetInVehicleContextOverrideCB()
{
	CPed* pPed = GetFocusPed();
	if (!pPed)
		return;

	const u32 OverrideHash = atStringHash(ms_szInVehicleContextOverride);
	const CInVehicleOverrideInfo* pOverrideInfo = CVehicleMetadataMgr::GetInVehicleOverrideInfo(OverrideHash);
	if (aiVerifyf(pOverrideInfo, "Couldn't find context for %s", ms_szInVehicleContextOverride))
	{
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
		if (pTask)
		{
			pTask->SetInVehicleContextHash(OverrideHash, true);
		}
	}
}

void CVehicleDebug::ClearInVehicleContextOverrideCB()
{
	CPed* pPed = GetFocusPed();
	if (!pPed)
		return;

	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	if (pTask)
	{
		pTask->SetInVehicleContextHash(0, true);
	}
}

void CVehicleDebug::GivePedSelectedTaskCB()
{
	CPed* pFocusPed = NULL;
	CVehicle* pFocusVehicle = NULL;

	// Get the focus entities
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	CEntity* pFocusEntity2 = CDebugScene::FocusEntities_Get(1);

	// Select the ped from the first focus entity
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity1);
	}

	// Select the ped from the second focus entity
	if (!pFocusPed)
	{
		if (pFocusEntity2 && pFocusEntity2->GetIsTypePed())
		{
			pFocusPed = static_cast<CPed*>(pFocusEntity2);
		}
	}

	// If unsuccessful try to use the last ped created through the widgets
	if (!pFocusPed)
	{
		pFocusPed = ms_pLastCreatedPed.Get();
	}

	// If still no success, use the player
	if (!pFocusPed)
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	// Select the vehicle from the first focus entity
	if (pFocusEntity1 && pFocusEntity1->GetIsTypeVehicle())
	{
		pFocusVehicle = static_cast<CVehicle*>(pFocusEntity1);
	}

	// Select the vehicle from the second focus entity
	if (!pFocusVehicle)
	{
		if (pFocusEntity2 && pFocusEntity2->GetIsTypeVehicle())
		{
			pFocusVehicle = static_cast<CVehicle*>(pFocusEntity2);
		}
	}

	if (!pFocusVehicle)
	{
		pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (pFocusPed && pFocusVehicle)
	{
		CTask* pTaskToGiveToFocusPed = NULL;

		switch(iVehicleTestHarnessTaskTypes[ms_iSelectedTask])
		{
			case CTaskTypes::TASK_ENTER_VEHICLE:
				{
					if (pFocusVehicle->IsSeatIndexValid(ms_iSeatRequested))
					{
						pTaskToGiveToFocusPed = rage_new CTaskEnterVehicle(pFocusVehicle, (SeatRequestType) ms_eSeatRequestType, ms_iSeatRequested, GetFlags(), ms_fWarpTime, ms_fMoveBlendRatio);
					}
				}
				break;
			case CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE:
				{
					if (pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
					{
						s32 iTargetSeatIndex = -1; // Use first shuffle link by default
						if (ms_iShuffleLink > 0)
						{
							const s32 iCurrentSeatIndex = pFocusVehicle->GetSeatManager()->GetPedsSeatIndex(pFocusPed);
							const s32 iCurrentEntryPointIndex = pFocusVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iCurrentSeatIndex, pFocusVehicle);
							iTargetSeatIndex = pFocusVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iCurrentEntryPointIndex, iCurrentSeatIndex, true);
						}

						pTaskToGiveToFocusPed = rage_new CTaskInVehicleSeatShuffle(pFocusVehicle, NULL, true, iTargetSeatIndex);
					}
				}
				break;
			case CTaskTypes::TASK_EXIT_VEHICLE:
				{
					pTaskToGiveToFocusPed = rage_new CTaskExitVehicle(pFocusPed->GetMyVehicle(), GetFlags(), ms_fDelayTime);
				}
				break;
			case CTaskTypes::TASK_VEHICLE_GUN:
				{
					if (pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
					{
						if (pFocusEntity2)
						{
							ms_PedTarget = pFocusEntity2;
							pTaskToGiveToFocusPed = rage_new CTaskVehicleGun(static_cast<CTaskVehicleGun::eTaskMode>(ms_iDriveByMode), FIRING_PATTERN_FULL_AUTO, &ms_PedTarget);
						}	
					}
				}
			case CTaskTypes::TASK_ATTACHED_VEHICLE_ANIM_DEBUG:
				{
					if (ms_bUseDefaultStateList)
					{
						ClearAllStatesFromArrayCB();
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_Align);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_OpenDoor);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetIn);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_CloseDoorFromInside);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_Idle);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetOut);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_CloseDoorFromOutside);
						DisplayStateList();
					}
					else if (ms_bUseDefaultBikeStateList)
					{
						ClearAllStatesFromArrayCB();
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_Align);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetIn);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_Idle);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetOut);
						DisplayStateList();
					}
					else if (ms_bUseDefaultAimExitStateList)
					{
						ClearAllStatesFromArrayCB();
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_OpenDoor);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetIn);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetOutToAim);
						DisplayStateList();
					}
					else if (ms_bUseDefaultPlaneStateList)
					{
						ClearAllStatesFromArrayCB();
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_ClimbUp);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_OpenDoor);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetIn);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_CloseDoorFromInside);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_Idle);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_GetOut);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_CloseDoorFromOutside);
						ms_aDesiredStates.PushAndGrow(CTaskAttachedVehicleAnimDebug::State_ClimbDown);
						DisplayStateList();
					}

					pTaskToGiveToFocusPed = rage_new CTaskAttachedVehicleAnimDebug(pFocusVehicle, ms_iSeatRequested, ms_iEntryPoint, ms_aDesiredStates);
				}
				break;
			case CTaskTypes::TASK_HELI_PASSENGER_RAPPEL:
			{
				const float fMinRappelHeight = 10.f;
				pTaskToGiveToFocusPed = rage_new CTaskHeliPassengerRappel(fMinRappelHeight);
			}
			break;
			default:
				taskAssertf(0, "Unhandled Task");
		}

		if (pTaskToGiveToFocusPed)
		{
			if (pFocusPed)
			{
				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTaskToGiveToFocusPed, false, E_PRIORITY_GIVE_PED_TASK);
				pFocusPed->GetPedIntelligence()->AddEvent(event);
			}
			else
			{
				delete pTaskToGiveToFocusPed;
				pTaskToGiveToFocusPed = NULL;
			}
		}
	}
}

void CVehicleDebug::AddStateToArrayCB()
{
	ms_aDesiredStates.PushAndGrow(ms_iSelectedTaskState);
	DisplayStateList();
}

void CVehicleDebug::ClearLastStateFromArrayCB()
{
	if (ms_aDesiredStates.GetCount() > 0)
	{
		ms_aDesiredStates.Pop();
	}
	DisplayStateList();
}

void CVehicleDebug::ClearAllStatesFromArrayCB()
{
	ms_aDesiredStates.clear();
	DisplayStateList();
}

void CVehicleDebug::DisplayStateList()
{
	Displayf("==================");
	Displayf("DESIRED STATE LIST");
	Displayf("==================");
	for (s32 i=0; i<ms_aDesiredStates.GetCount(); ++i)
	{
		Displayf("%s", CTaskAttachedVehicleAnimDebug::GetStaticStateName(ms_aDesiredStates[i]));
	}
}
VehicleEnterExitFlags CVehicleDebug::GetFlags()
{
	VehicleEnterExitFlags iFlags;

	if (ms_bUseDirectEntryOnly)  iFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
	if (ms_bJumpOut)			 iFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
	if (ms_bDelayForTime)		 iFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
	if (ms_bJustPullPedOut)		 iFlags.BitSet().Set(CVehicleEnterExitFlags::JustPullPedOut);
	if (ms_bJackAnyOne)			 iFlags.BitSet().Set(CVehicleEnterExitFlags::JackIfOccupied);
	if (ms_bWarp)				 iFlags.BitSet().Set(CVehicleEnterExitFlags::Warp);
	if (ms_bWarpAfterTime)		 iFlags.BitSet().Set(CVehicleEnterExitFlags::WarpAfterTime);
	if (ms_bWarpIfDoorIsBlocked) iFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
	if (ms_bJustOpenDoor)		 iFlags.BitSet().Set(CVehicleEnterExitFlags::JustOpenDoor);
	if (ms_bDontCloseDoor)		 iFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	if (ms_bWarpToEntryPoint)	 iFlags.BitSet().Set(CVehicleEnterExitFlags::WarpToEntryPosition);
	if (ms_bWaitForEntryToBeClear)	 iFlags.BitSet().Set(CVehicleEnterExitFlags::WaitForEntryToBeClearOfPeds);

	return iFlags;
}

void CVehicleDebug::CreateMule5()
{
	bool bCreateAsPersonalCar = false;

	if (bCreateAsPersonalCar)
	{
		bCreateAsMissionCar = true;
	}

	CVehicle *pVehicle = NULL;
	atHashString mule5("MULE5");
	fwModelId modelID;
	CModelInfo::GetBaseModelInfoFromHashKey(mule5.GetHash(), &modelID);

	if (modelID.IsValid())
	{

		Matrix34 tempMat;
		tempMat.Identity();

		float fRotation = camInterface::GetDebugDirector().IsFreeCamActive() && bUseDebugCamRotation ? camInterface::GetHeading() : carDropRotation;
		tempMat.RotateZ(fRotation);

		tempMat.d = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetPos() : VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());

		Vector3 vecCreateOffset = camInterface::GetFront();
		vecCreateOffset.z = 0.0f;
		vecCreateOffset.NormalizeSafe();

		pVehicle = CreateCarAtMatrix(modelID.GetModelIndex(), &tempMat, vecCreateOffset, false);
		if (pVehicle)
		{
			pVehicle->GetPortalTracker()->ScanUntilProbeTrue();

			//keep a record of the last created vehicle
			if (CVehicleFactory::ms_pLastCreatedVehicle)
				CVehicleFactory::ms_pLastLastCreatedVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
			CVehicleFactory::ms_pLastCreatedVehicle = pVehicle;
			CVehicleFactory::ms_bForceHd = pVehicle->IsForcedHd();
			CVehicleFactory::ms_fLodMult = pVehicle->GetLodMultiplier();
			CVehicleFactory::ms_clampedRenderLod = pVehicle->GetClampedRenderLod();

			if (bCreateAsPersonalCar)
			{
				CVehicleDebug::SetVehicleAsPersonalCB();
			}

			CVehicleFactory::VariationDebugColorChangeCB();

			if (pVehicle->IsModded())
			{
				CVehicleFactory::VariationDebugUpdateMods();
			}
		}
	}

	CPed* pPed = CGameWorld::FindLocalPlayer();

	if (pPed && pVehicle)
	{
		pPed->GetPedIntelligence()->FlushImmediately();

		static s32 iFlags = 0;
		pPed->SetPedInVehicle(pVehicle, CVehicleDebug::ms_iSeatRequested, iFlags);
		
		for (int i = 0; i < pVehicle->GetNumDoors(); i++)
		{
			CCarDoor * pDoor = pVehicle->GetDoor(i);

			if (pDoor && pDoor->GetIsIntact(pVehicle))
			{
				pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET | CCarDoor::DRIVEN_SPECIAL);
			}
		}

		ms_bForcePlayerToUseSpecificSeat = true;
		//Give weapons
		pPed->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, CWeapon::INFINITE_AMMO);
		pPed->GetWeaponManager()->EquipWeapon(WEAPONTYPE_PISTOL, -1, true);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim, true); 
	}
}

void CVehicleDebug::DeleteMule5()
{
	if (CVehicleFactory::ms_pLastCreatedVehicle)
	{
		g_ptFxManager.RemovePtFxFromEntity(CVehicleFactory::ms_pLastCreatedVehicle);

		CVehicleFactory::GetFactory()->Destroy(CVehicleFactory::ms_pLastCreatedVehicle);
		CVehicleFactory::ms_pLastCreatedVehicle = NULL;
		CVehicleFactory::ms_bForceHd = false;
		CVehicleFactory::ms_fLodMult = 1.f;
		CVehicleFactory::ms_clampedRenderLod = -1;
	}
}

void CVehicleDebug::InitWidgets(bkBank& bank)
{	
	// Debug rendering
	bank.AddSeparator();

		bank.PushGroup("Task Debug");

			// Spawn A Ped
			bank.AddToggle("Use specific spawn location", &ms_bUseSpecificSpawnLocation);
			bank.AddToggle("Turn on Measuring tool", &CPhysics::ms_bDebugMeasuringTool);
			bank.AddToggle("Put ped in players group", &ms_bPutInPlayersGroup);
			bank.AddButton("Spawn Ped", &SpawnPedCB);
			bank.AddButton("Spawn Vehicle And Ped", &SpawnVehicleAndPedCB);
			bank.AddToggle("Use Vehicle Offset For Spawn", &ms_bUseSpawnOffset);
			bank.AddSlider("X Offset", &ms_fSpawnOffsetX, -10.0f, 10.0f, 0.1f);
			bank.AddSlider("Y Offset", &ms_fSpawnOffsetY, -10.0f, 10.0f, 0.1f);

			// Enter/Exit Tasks
			bank.AddSlider("Seat Requested", &ms_iSeatRequested, 0, 16, 1);	
			bank.AddSlider("Entry Point", &ms_iEntryPoint, 0, 16, 1);	
			bank.AddSlider("Shuffle Link", &ms_iShuffleLink, 0, 1, 1);	
			bank.AddCombo("Seat Request Type", &ms_eSeatRequestType, Num_SeatRequestTypes, SeatRequestTypeStrings);

			static const u32 MAX_TASKS = 10;
			static const char* szTaskHarnessNames[MAX_TASKS];
			s32 iTaskHarnessCount = 0;
			for (s32 i = 0; iVehicleTestHarnessTaskTypes[i] != CTaskTypes::TASK_NONE; i++)
			{
				szTaskHarnessNames[i] = TASKCLASSINFOMGR.GetTaskName(iVehicleTestHarnessTaskTypes[i]);
				++iTaskHarnessCount;
			}

			ms_pTasksCombo = bank.AddCombo("Task:", &ms_iSelectedTask, iTaskHarnessCount, szTaskHarnessNames, NullCB, "Vehicle Task harness selection");
			bank.AddButton("Give Ped Vehicle Task", &GivePedSelectedTaskCB);

			static const u32 NUM_VEHICLE_DEBUG_STATES = (u32)CTaskAttachedVehicleAnimDebug::State_Finish + 1;
			static const char* szTaskStateNames[NUM_VEHICLE_DEBUG_STATES];
			for (s32 i = 0; i<NUM_VEHICLE_DEBUG_STATES; i++)
			{
				szTaskStateNames[i] = CTaskAttachedVehicleAnimDebug::GetStaticStateName(i);
			}
			ms_pTasksCombo = bank.AddCombo("TaskAttachedVehicleAnimDebugState:", &ms_iSelectedTaskState, NUM_VEHICLE_DEBUG_STATES, szTaskStateNames, NullCB, "Attach Vehicle Anim Task State");
			bank.AddButton("Add Task State To Array", &AddStateToArrayCB);
			bank.AddButton("Clear Last Task State To Array", &ClearLastStateFromArrayCB);
			bank.AddButton("Clear All Task States", &ClearAllStatesFromArrayCB);
			bank.AddToggle("Attach To Bike", &ms_bAttachToBike);
			bank.AddToggle("Test On Bikes", &ms_bTestOnBike);
			bank.AddToggle("Use Fast Bike Anims", &ms_bUseFastBikeAnims);
			bank.AddToggle("Use Default Exit To Aim State List", &ms_bUseDefaultAimExitStateList);
			bank.AddToggle("Use Default State List", &ms_bUseDefaultStateList);
			bank.AddToggle("Use Default Bike State List", &ms_bUseDefaultBikeStateList);
			bank.AddToggle("Use Default Plane State List", &ms_bUseDefaultPlaneStateList);
			bank.AddToggle("Disable Ground And Orientation Fixup", &ms_bDisableGroundAndOrientationFixup);
			bank.AddToggle("Force player to use seat index specified as target seat normally", &ms_bForcePlayerToUseSpecificSeat);
			bank.AddSlider("Preferred Passenger Seat Index For Groups", &ms_iPreferredPassengerSeatIndex, -1, 16, 1);
			bank.AddButton("Set Preferred Passenger Seat Index For Focus Ped", SetFocusPedPreferredPassengerSeatIndexCB);
			bank.AddButton("Make Focus Ped Poptype Mission", MakeFocusPedPopTypeScriptCB);

			bank.PushGroup("Task Parameters");

				bank.AddToggle("Use Direct Entry Only", &ms_bUseDirectEntryOnly);
				bank.AddToggle("Just Open Door", &ms_bJustOpenDoor);
				bank.AddToggle("Don't Close Door", &ms_bDontCloseDoor);
				bank.AddToggle("Jump Out", &ms_bJumpOut);
				bank.AddToggle("Just Pull Ped Out", &ms_bJustPullPedOut);
				bank.AddToggle("Jack Anyone", &ms_bJackAnyOne);
				bank.AddToggle("Warp", &ms_bWarp);
				bank.AddToggle("Wait For Entry To Be Clear", &ms_bWaitForEntryToBeClear);
				bank.AddToggle("Warp After Time", &ms_bWarpAfterTime);
				bank.AddToggle("Warp If Door Is Blocked", &ms_bWarpIfDoorIsBlocked);
				bank.AddSlider("Warp Time", &ms_fWarpTime, 0.0f, 100.0f, 0.1f);
				bank.AddToggle("Warp To Entry Point", &ms_bWarpToEntryPoint);
				bank.AddToggle("Delay Exit For Time", &ms_bDelayForTime);
				bank.AddSlider("Delay Time", &ms_fDelayTime, 0.0f, 100.0f, 0.1f);
				bank.AddSlider("Move Blend Ratio", &ms_fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT, 0.1f);
				bank.AddSlider("Aim Blend Direction", &ms_fAimBlendDirection, 0.0f, 1.0f, 0.01f);	
				bank.AddCombo("DriveBy Mode", &ms_iDriveByMode, CTaskVehicleGun::Num_FireModes, DriveByModeStrings);

				bank.AddSlider("Forced Seat Usage Selected Slot", &ms_SelectedForcedVehicleUsageSlot, 0, CSyncedPedVehicleEntryScriptConfig::MAX_VEHICLE_ENTRY_DATAS-1,1);
				bank.AddSlider("Forced Usage Seat Index", &ms_iSeatIndex, -1, 19, 1);
				bank.AddToggle("Force Use Rear Seats Only", &ms_ForceUseRearSeatsOnly);
				bank.AddToggle("Force Use Front Seats Only", &ms_ForceUseFrontSeatsOnly);
				bank.AddToggle("Force For Any Vehicle", &ms_ForceForAnyVehicle);
				bank.AddButton("Apply forced vehicle usage settings", SetForcedVehicleUsageCB);
				bank.AddButton("Clear forced vehicle usage settings", ClearForcedVehicleUsageCB);
				bank.AddButton("Validate Script Config", ValidateScriptConfigCB);
			bank.PopGroup();

			// Force Warp The Ped In
			bank.AddButton("Set Ped In Seat", &SetFocusPedInSeatCB);
			bank.AddButton("Roll Down All Windows", &RollDownAllWindowsCB);

			bank.AddButton("Override In Vehicle Context", &SetOverrideInVehicleContextCB);
			bank.AddButton("Reset In Vehicle Context", &ResetOverrideInVehicleContextCB);

			bank.AddButton("Set Ped Exclusive Driver", &SetFocusPedExclusiveDriverCB);
			bank.AddButton("Toggle AI can use exclusive seats", &ToggleAICanUseExclusiveSeatsCB);

			bank.AddSlider("Desired Turret Heading", &ms_fDesiredTurretHeading, -PI, PI, 0.01f);	
			bank.AddSlider("Desired Turret Pitch", &ms_fDesiredTurretPitch, -HALF_PI, HALF_PI, 0.01f);	
			bank.AddButton("Set Turret Desired heading And Pitch", &SetTurretDesiredHeadingAndPitchCB);
			bank.AddButton("Rotate for player", &RotateTurretToMatchPlayerCB);
			
		bank.PopGroup();

	bank.AddSeparator();

		bank.PushGroup("Debug Rendering");

			bank.AddToggle("Render Peds In Vehicles", &CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualisePedsInVehicles);
			bank.AddToggle("Render Debug", &ms_bRenderDebug);
			bank.AddToggle("Render Enter Turrets", &ms_renderEnterTurret);
			bank.AddToggle("Render Exclusive Driver", &ms_bRenderExclusiveDriverDebug);
			bank.AddToggle("Render Last Driver", &ms_bRenderLastDriverDebug);
			bank.AddSlider("Exclusive Driver Index", &ms_ExclusiveDriverIndex, 0, CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS-1, 1);
			bank.AddToggle("Render Seats", &ms_bRenderSeats);
			bank.AddToggle("Render Entry Points", &ms_bRenderEntryPoints);
			bank.AddToggle("Render Layout Info", &ms_bRenderLayoutInfo);
			bank.AddToggle("Render Through Windscreen Anim", &ms_bRenderThroughWindscreenAnim);
			bank.AddToggle("Render Entry Anim", &ms_bRenderEntryAnim);
			bank.AddToggle("Render PickUp Anim", &ms_bRenderPickUpAnim);
			bank.AddToggle("Render Exit Anim", &ms_bRenderExitAnim);
			bank.AddToggle("Render Entry-Seat Links", &ms_bRenderEntrySeatLinks);
			bank.AddToggle("Render Entry Collision Check", &ms_bRenderEntryCollisionCheck);
			bank.AddToggle("Render DriveBy Task Debug", &ms_bRenderDriveByTaskDebug);
			bank.AddToggle("Render DriveBy Limits For Focus Ped", &ms_bRenderDriveByYawLimitsForPed);
			bank.AddToggle("Render Entry Point Evaluation Task Debug", &ms_bRenderEntryPointEvaluationTaskDebug);
			bank.AddToggle("Render Entry Point Evaluation Debug", &ms_bRenderEntryPointEvaluationDebug);
			bank.AddToggle("Render Anim Streaming Debug", &ms_bRenderAnimStreamingDebug);
			bank.AddToggle("Render Anim Streaming Text", &ms_bRenderAnimStreamingText);
			bank.AddToggle("Render Component Reservation Text", &ms_bRenderComponentReservationText);
			bank.AddToggle("Render Component Reservation Usage", &ms_bRenderComponentReservationUsage);
			bank.AddToggle("Display Seat Relative Offset", &ms_bOutputSeatRelativeOffsets);
			bank.AddSlider("Debug Entry Point", &ms_iDebugEntryPoint, -1, 16, 1);	

		bank.PopGroup();
		
	bank.AddSeparator();

		bank.PushGroup("Other Vehicle Related Stuff");
			// Anim Streaming
			bank.AddToggle("Load Anims With Vehicle Model", &ms_bLoadAnimsWithVehicle);
		
			// Vehicle Locks
			bank.AddCombo("Vehicle Lock State", &ms_iVehicleLockState, CARLOCK_NUM_STATES, VehicleLockStateStrings);
			bank.AddButton("Set Vehicle Lock State", LockVehicleDoorsCB);
			bank.AddToggle("Repair Vehicle When Setting Debug Vehicle Lock State", &ms_bRepairOnDebugVehicleLock);
			bank.AddButton("Set Player May Only Enter Vehicle", SetMayOnlyEnterThisVehicleCB);
			bank.AddButton("Set Vehicle As Personal", SetVehicleAsPersonalCB);
			bank.AddSlider("Player Jack Rate", &ms_iPlayerJackRate, 0, 300, 1, SetPlayerJackRateCB);

			bank.AddButton("Set In Vehicle Context Override", SetInVehicleContextOverrideCB);
			bank.AddButton("Clear In Vehicle Context Override", ClearInVehicleContextOverrideCB);
			bank.AddText("In Vehicle Context Override", ms_szInVehicleContextOverride, sizeof(ms_szInVehicleContextOverride));

		bank.PopGroup();

	bank.AddSeparator();

		bank.PushGroup("Mule5 Driveby Setup");
			bank.AddButton("Create Mule5", CreateMule5);
			bank.AddButton("Delete Mule5", DeleteMule5);
			bank.AddSlider("Seat Index - 4 Rear Left Side, 5 Rear Right Side", &ms_iSeatRequested, 0, 16, 1);
			
		bank.PopGroup();

	bank.AddSeparator();	
}

CVehicle* CVehicleDebug::GetFocusVehicle(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypeVehicle() )
		{
			CVehicle* pFocusVeh = static_cast<CVehicle*>(pEnt);
			return pFocusVeh;
		}
	}
	return NULL;
}

CPed* CVehicleDebug::GetFocusPed()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			return pFocusPed;
		}
	}
	return NULL;
}

void CVehicleDebug::RenderDebug()
{
	if(ms_renderEnterTurret)
	{
		RenderEnterTurretSeats();
	}

	if (ms_bRenderDebug)
	{
		if (ms_bRenderDriveByYawLimitsForPed)
		{
			CPed* pFocusPed = NULL;

			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
			if (pFocusEntity && pFocusEntity->GetIsTypePed())
			{
				pFocusPed = static_cast<CPed*>(pFocusEntity);
			}

			if (!pFocusPed)
			{
				pFocusPed = CGameWorld::FindLocalPlayer();
			}

			if (pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
			{
				const CVehicleDriveByInfo* pDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pFocusPed);
				Vec3V vDrawCenter = pFocusPed->GetTransform().GetPosition();
				static float sfRadius = 5.0f;
				Color32 iColor = Color_blue;
				Vec3V vecRight(pFocusPed->GetTransform().GetA());
				Vec3V vecForward(pFocusPed->GetTransform().GetB());
				grcDebugDraw::Arc(vDrawCenter, sfRadius, iColor,  vecForward, vecRight, 0.0f, -fwAngle::LimitRadianAngle(pDriveByInfo->GetMinAimSweepHeadingAngleDegs(pFocusPed) * DtoR));
				grcDebugDraw::Arc(vDrawCenter, sfRadius, iColor, vecForward, vecRight, -fwAngle::LimitRadianAngle(pDriveByInfo->GetMaxAimSweepHeadingAngleDegs(pFocusPed)* DtoR), 0.0f);
			}
		}

		CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
		if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
		{
			pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
		}

		if (pVehicle)
		{
			s32 iNumEntryPoints = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumberEntryExitPoints();

			CBaseModelInfo* pModelInfo = pVehicle->GetBaseModelInfo();
			if(pModelInfo)	grcDebugDraw::AddDebugOutput(Color_green, "Name : %s", pModelInfo->GetModelName());
			grcDebugDraw::AddDebugOutput(Color_green, "Layout : %s", pVehicle->GetLayoutInfo()->GetName().GetCStr());
			grcDebugDraw::AddDebugOutput(Color_green, "Num Entrypoints : %u", iNumEntryPoints);
			grcDebugDraw::AddDebugOutput(Color_green, "Num Seats : %u", pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumSeats());
			grcDebugDraw::AddDebugOutput(Color_green, "Lock State : %s", VehicleLockStateStrings[pVehicle->GetCarDoorLocks()]);

			for (s32 iEntryPointIndex=0; iEntryPointIndex<iNumEntryPoints; iEntryPointIndex++)
			{
				if( (iEntryPointIndex == ms_iDebugEntryPoint) || (ms_iDebugEntryPoint == -1) )
				{
					const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(iEntryPointIndex);
					if (pEntryPoint)
					{
						CCarDoor* pDoor = pVehicle->GetDoorFromBoneIndex(pEntryPoint->GetDoorBoneIndex());
						if (pDoor)
						{
							grcDebugDraw::AddDebugOutput(Color_green, "Door (%u), Open Ratio : %.2f, Target Ratio : %.2f, Current Speed : %.2f",
								iEntryPointIndex, pDoor->GetDoorRatio(), pDoor->GetTargetDoorRatio(), pDoor->GetCurrentSpeed());

							grcDebugDraw::AddDebugOutput(Color_green, "%s : %i, %s : %i, %s : %i, %s : %i", 
								CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SHUT), (s32)pDoor->GetFlag(CCarDoor::DRIVEN_SHUT),
								CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_AUTORESET), (s32)pDoor->GetFlag(CCarDoor::DRIVEN_AUTORESET),
								CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_NORESET), (s32)pDoor->GetFlag(CCarDoor::DRIVEN_NORESET),
								CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_GAS_STRUT), (s32)pDoor->GetFlag(CCarDoor::DRIVEN_GAS_STRUT));

							grcDebugDraw::AddDebugOutput(Color_green, "%s : %i, %s : %i, %s : %i, %s : %i, %s : %i, %s : %i",
								CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SWINGING), (s32)pDoor->GetFlag(CCarDoor::DRIVEN_SWINGING),
								CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SMOOTH), (s32)pDoor->GetFlag(CCarDoor::DRIVEN_SMOOTH),
								CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SPECIAL), (s32)pDoor->GetFlag(CCarDoor::DRIVEN_SPECIAL),	
								CCarDoor::GetDoorFlagString(CCarDoor::PED_DRIVING_DOOR), (s32)pDoor->GetFlag(CCarDoor::PED_DRIVING_DOOR),
								CCarDoor::GetDoorFlagString(CCarDoor::PED_USING_DOOR_FOR_COVER), (s32)pDoor->GetFlag(CCarDoor::PED_USING_DOOR_FOR_COVER),	
								CCarDoor::GetDoorFlagString(CCarDoor::LOOSE_LATCHED_DOOR), (s32)pDoor->GetFlag(CCarDoor::LOOSE_LATCHED_DOOR));	
						}
					}

					s32 iNumTexts = 0;
					Color32 colour = Color_blue;
					char szText[64];

					if(ms_bRenderExclusiveDriverDebug)
					{
						for (u32 i=0;i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; i++)
						{
							formatf(szText, "Exclusive Driver[%i] %p", i, pVehicle->GetExclusiveDriverPed(i));
							grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
						}

						formatf(szText, "AI can use exclusive seats: %s", pVehicle->m_nVehicleFlags.bAICanUseExclusiveSeats ? "TRUE" : "FALSE" );
						grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
					}

					if(ms_bRenderLastDriverDebug)
					{
						formatf(szText, "Last Driver %s (%d) ", pVehicle->GetLastDriver() ? pVehicle->GetLastDriver()->GetDebugName() : "", pVehicle->m_LastTimeWeHadADriver);
						grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
					}

					RenderEntryPointCollisionCheck(pVehicle, iEntryPointIndex);

					RenderEntryPointExtraVehicleNodes(pVehicle, iEntryPointIndex);

					RenderEntryPoints(pVehicle, pVehicle->GetEntryExitPoint(iEntryPointIndex), iEntryPointIndex, iNumTexts);

					RenderSeats(pVehicle, iNumTexts);

					RenderDoorComponentReservation(pVehicle, pVehicle->GetEntryExitPoint(iEntryPointIndex), iEntryPointIndex, pVehicle->GetComponentReservationMgr()->GetDoorReservation(iEntryPointIndex), iNumTexts);

					RenderSeatComponentReservation(pVehicle, pVehicle->GetEntryExitPoint(iEntryPointIndex), iEntryPointIndex, pVehicle->GetComponentReservationMgr()->GetSeatReservation(iEntryPointIndex, SA_directAccessSeat),++iNumTexts);
				}
			}
		}
	}
}

void CVehicleDebug::RenderEntryPointCollisionCheck(CVehicle* pVehicle, s32 iEntryIndex)
{
	if (pVehicle && ms_bRenderEntryCollisionCheck)
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed)
		{
			CVehicle::IsEntryPointClearForPed(*pVehicle, *pPlayerPed, iEntryIndex);
		}
	}
}

void CVehicleDebug::RenderEntryPointExtraVehicleNodes(CVehicle* pVehicle, s32 iEntryIndex)
{
	if (pVehicle)
	{
		if (ms_bRenderEntryPoints)
		{
			Vector3 vNewTargetPosition(Vector3::ZeroType);
			Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);

			static float sfScale = 0.25f;

			Color32 color = Color_yellow;
			char szText[64];

			for(int i = CExtraVehiclePoint::MAX_GET_IN_POINTS-1; i >= 0; i--)
			{
				CExtraVehiclePoint::ePointType point = CExtraVehiclePoint::ePointType(CExtraVehiclePoint::GET_IN + i);

				if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, point))
				{	
					color = Color_orange;
					formatf(szText, "Get In Point (%i) %i", i, iEntryIndex);
					grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
					grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
					Vector3 vEulers;
					qNewTargetOrientation.ToEulers(vEulers, "xyz");
					Vector3 vDir(0.0f,1.0f,0.0f);
					vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
					vDir.Scale(sfScale);
					grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
				}
			}

			if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::GET_OUT))
			{	
				color = Color_red;
				formatf(szText, "Get Out Point %i", iEntryIndex);
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}

			if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::VAULT_HAND_HOLD))
			{	
				color = Color_purple;
				formatf(szText, "Vault Hand Hold Point %i", iEntryIndex);
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}

			if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::CLIMB_UP_FIXUP_POINT))
			{	
				color = Color_orange;
				formatf(szText, "Climb Up Fixup Point %i", iEntryIndex);
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}

			if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::ON_BOARD_JACK))
			{	
				color = Color_pink;
				formatf(szText, "On Board Jack Point %i", iEntryIndex);
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}
			
			if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::UPSIDE_DOWN_EXIT))
			{	
				color = Color_blue;
				formatf(szText, "Upside Down Exit Point %i, Vehicle Roll %.2f", iEntryIndex, pVehicle->GetTransform().GetRoll());
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}

			bool bRenderPickUp = true;

			TUNE_GROUP_BOOL(BIKE_TUNE, DISABLE_EXTRA_POINTS_RENDERING, true);
			if (DISABLE_EXTRA_POINTS_RENDERING)
			{
				const float fSide = pVehicle->GetTransform().GetA().GetZf();
				if (Abs(fSide) > CTaskEnterVehicle::ms_Tunables.m_MinMagForBikeToBeOnSide)
				{
					const CModelSeatInfo* pModelSeatinfo = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
					const CVehicleEntryPointInfo* pEntryInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointInfo(iEntryIndex); 
					taskFatalAssertf(pEntryInfo, "CalculateEntrySituation: Entity doesn't have valid entry point info");
					const bool bLeftSide = (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT) ? true : false;
					const bool bIsPickUp = (fSide < 0.0f) ? (bLeftSide ? false : true) : (bLeftSide ? true : false);

					if (!bIsPickUp)
					{
						bRenderPickUp = false;
					}
				}
			}

			if (bRenderPickUp && CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::PICK_UP_POINT))
			{	
				color = Color_orange;
				formatf(szText, "Pick Up Point %i", iEntryIndex);
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}


			bool bRenderPullUp = true;

			if (DISABLE_EXTRA_POINTS_RENDERING)
			{
				const float fSide = pVehicle->GetTransform().GetA().GetZf();
				if (Abs(fSide) > CTaskEnterVehicle::ms_Tunables.m_MinMagForBikeToBeOnSide)
				{
					const CModelSeatInfo* pModelSeatinfo = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
					const CVehicleEntryPointInfo* pEntryInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointInfo(iEntryIndex); 
					taskFatalAssertf(pEntryInfo, "CalculateEntrySituation: Entity doesn't have valid entry point info");
					const bool bLeftSide = (pEntryInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT) ? true : false;
					const bool bIsPickUp = (fSide < 0.0f) ? (bLeftSide ? false : true) : (bLeftSide ? true : false);

					if (bIsPickUp)
					{
						bRenderPullUp = false;
					}
				}
			}

			if (bRenderPullUp && CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::PULL_UP_POINT))
			{	
				color = Color_yellow;
				formatf(szText, "Pull Up Point Point %i", iEntryIndex);
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}

			if (CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*pVehicle, vNewTargetPosition, qNewTargetOrientation, iEntryIndex, CExtraVehiclePoint::QUICK_GET_ON_POINT))
			{	
				color = Color_red;
				formatf(szText, "Quick Get On Point Point %i", iEntryIndex);
				grcDebugDraw::Text(vNewTargetPosition, color, 0, 0, szText);
				grcDebugDraw::Sphere(vNewTargetPosition, 0.05f, color);	
				Vector3 vEulers;
				qNewTargetOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vNewTargetPosition),VECTOR3_TO_VEC3V(vNewTargetPosition + vDir), 0.025f, color);
			}
		}
	}
}

void CVehicleDebug::LockVehicleDoorsCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (pVehicle)
	{
		if (ms_bRepairOnDebugVehicleLock)
		{
			pVehicle->Fix();
		}

		pVehicle->SetCarDoorLocks(static_cast<eCarLockState>(ms_iVehicleLockState));
	}
}

void CVehicleDebug::SetMayOnlyEnterThisVehicleCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (pVehicle)
	{
		CPed* pPed = CGameWorld::FindLocalPlayer();
		pPed->GetPlayerInfo()->SetMayOnlyEnterThisVehicle(pVehicle);
	}
}

void CVehicleDebug::SetVehicleAsPersonalCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (pVehicle)
	{
		s32 iVehicleGuid = CTheScripts::GetGUIDFromEntity(*pVehicle);
		if (iVehicleGuid != NULL_IN_SCRIPTING_LANGUAGE)
		{
			fwExtensibleBase* pBase = fwScriptGuid::GetBaseFromGuid(iVehicleGuid);
			if (pBase)
			{
				CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
				if (pPlayerPed->GetNetworkObject() && pVehicle->GetNetworkObject())
				{
					CNetGamePlayer* pPlayer = pPlayerPed->GetNetworkObject()->GetPlayerOwner();
					PhysicalPlayerIndex iPlayerIdx = pPlayer->GetPhysicalPlayerIndex();
					if (iPlayerIdx != INVALID_PLAYER_INDEX)
					{
						const char* szPlayerName = player_commands::GetPlayerName(iPlayerIdx, "CVehicleDebug::SetVehicleAsPersonalCB");
						s32 iPlayerNameHash = (s32)atStringHash(szPlayerName);
						atHashWithStringBank decKey("Player_Vehicle");
						fwDecoratorExtension::Set((*pBase), decKey, iPlayerNameHash);
						pVehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer = true;
						static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->LockForAllPlayers(true);
						static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->LockForPlayer(iPlayerIdx, false);
						pVehicle->PopTypeSet(POPTYPE_MISSION);
						ms_pPersonalVehicle = pVehicle;
						ms_pPersonalVehicleOwner = pPlayerPed;
					}
				}
			}
		}
	}
}

void CVehicleDebug::ProcessPersonalVehicle()
{
	if (!ms_pPersonalVehicle || !ms_pPersonalVehicle->GetNetworkObject() || ms_pPersonalVehicle->IsNetworkClone())
		return;

	if (!ms_pPersonalVehicleOwner || !ms_pPersonalVehicleOwner->GetNetworkObject())
		return;

	if (!ms_pPersonalVehicleOwner->GetIsInVehicle() || ms_pPersonalVehicleOwner->GetMyVehicle() != ms_pPersonalVehicle)
	{
		static_cast<CNetObjVehicle*>(ms_pPersonalVehicle->GetNetworkObject())->LockForAllPlayers(true);
		CNetGamePlayer* pPlayer = ms_pPersonalVehicleOwner->GetNetworkObject()->GetPlayerOwner();
		PhysicalPlayerIndex iPlayerIdx = pPlayer->GetPhysicalPlayerIndex();
		static_cast<CNetObjVehicle*>(ms_pPersonalVehicle->GetNetworkObject())->LockForPlayer(iPlayerIdx, false);
		return;
	}

	TUNE_GROUP_BOOL(PERSONAL_VEHICLE_DEBUG, PREVENT_UNLOCKING_WHEN_INSIDE, false);
	if (!PREVENT_UNLOCKING_WHEN_INSIDE)
	{
		static_cast<CNetObjVehicle*>(ms_pPersonalVehicle->GetNetworkObject())->LockForAllPlayers(false);
	}
}

s32 CVehicleDebug::GetForcedUsageFlags()
{
	s32 iFlags = 0;

	if (ms_ForceUseFrontSeatsOnly)
	{
		Assertf(!ms_ForceUseRearSeatsOnly, "Either force front or rear seats, not both!");
		iFlags |= CSyncedPedVehicleEntryScriptConfigData::ForceUseFrontSeats;
	}
	else if (ms_ForceUseRearSeatsOnly)
	{
		Assertf(!ms_ForceUseFrontSeatsOnly, "Either force front or rear seats, not both!");
		iFlags |= CSyncedPedVehicleEntryScriptConfigData::ForceUseRearSeats;
	}

	if (ms_ForceForAnyVehicle)
	{
		iFlags |= CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle;
	}

	return iFlags;
}

void CVehicleDebug::SetForcedVehicleUsageCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	CPed* pPed = CVehicleDebug::GetFocusPed();
	if (!pPed)
	{
		pPed = CGameWorld::FindLocalPlayer();
	}

	s32 iFlags = GetForcedUsageFlags();
	if (pVehicle && iFlags == 0)
	{
		Assertf(0, "No flags have been set");
	}

	pPed->SetForcedSeatUsage(ms_SelectedForcedVehicleUsageSlot, iFlags, iFlags & CSyncedPedVehicleEntryScriptConfigData::ForceForAnyVehicle ? NULL : pVehicle, ms_iSeatIndex);
}

void CVehicleDebug::ClearForcedVehicleUsageCB()
{
	CPed* pPed = CVehicleDebug::GetFocusPed();
	if (!pPed)
	{
		pPed = CGameWorld::FindLocalPlayer();
	}
	pPed->ClearForcedSeatUsage();
}

void CVehicleDebug::ValidateScriptConfigCB()
{
	CPed* pPed = CVehicleDebug::GetFocusPed();
	if (!pPed)
	{
		pPed = CGameWorld::FindLocalPlayer();
	}

	const CSyncedPedVehicleEntryScriptConfig* pVehicleEntryConfig = pPed->GetVehicleEntryConfig();
	if (pVehicleEntryConfig)
	{
		if (!pVehicleEntryConfig->ValidateData())
		{
			pVehicleEntryConfig->SpewDataToTTY();
			aiAssertf(0, "Ped %s has been setup with conflicting data, it has been spewed to the tty", AILogging::GetDynamicEntityNameSafe(pPed));
		}
	}
}

void CVehicleDebug::RollDownAllWindowsCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (pVehicle)
	{
		s32 iNumEntryPoints = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumberEntryExitPoints();

		for (s32 iEntryPoint = 0; iEntryPoint < iNumEntryPoints; ++iEntryPoint)
		{
			if (pVehicle->IsEntryIndexValid(iEntryPoint))
			{
				const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(iEntryPoint);
				Assertf(pEntryPointInfo, "Vehicle is missing entry point");

				eHierarchyId window = pEntryPointInfo->GetWindowId();
				if (window != VEH_INVALID_ID)
				{
					pVehicle->RolldownWindow(window);
					Assert(pVehicle->HasComponent(window));
				}
			}
		}
	}
}

void CVehicleDebug::SetOverrideInVehicleContextCB()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pPed = static_cast<CPed*>(pFocusEntity);
	}

	if (pPed)
	{	
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (pTask)
		{
			pTask->SetInVehicleContextHash(ATSTRINGHASH("MISS_ARMENIAN3_FRANKLIN_TENSE", 0x97942602), true);
		}
	}
}

void CVehicleDebug::ResetOverrideInVehicleContextCB()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pPed = static_cast<CPed*>(pFocusEntity);
	}

	if (pPed)
	{	
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (pTask)
		{
			pTask->ResetInVehicleContextHash(true);
		}
	}
}

void CVehicleDebug::SetTurretDesiredHeadingAndPitchCB()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (!pPed)
		return;

	// Select the vehicle from the first focus entity
	CVehicle* pFocusVehicle = GetFocusVehicle();
	if (!pFocusVehicle)
	{
		pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (!pFocusVehicle)
		return;

	CVehicleWeaponMgr* pVehWeaponMgr = pFocusVehicle->GetVehicleWeaponMgr();
	if (!aiVerifyf(pVehWeaponMgr, "Vehicle doesn't have a vehicle weapon manager"))
		return;

	atArray<CVehicleWeapon*> weaponArray;
	atArray<CTurret*> turretArray;
	pVehWeaponMgr->GetWeaponsAndTurretsForSeat(2, weaponArray, turretArray);

	int numTurrets = turretArray.size(); 
	for(int iTurretIndex = 0; iTurretIndex < numTurrets; iTurretIndex++)
	{
		//skip processing if the turretArray element is NULL
		if (!turretArray[iTurretIndex])
			continue;

		Quaternion desiredAngle;
		desiredAngle.FromEulers(Vector3(ms_fDesiredTurretPitch, 0.f, ms_fDesiredTurretHeading));
		turretArray[iTurretIndex]->SetDesiredAngle(desiredAngle, pFocusVehicle);
	}
}

void CVehicleDebug::RotateTurretToMatchPlayerCB()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (!pPed)
		return;

	// Select the vehicle from the first focus entity
	CVehicle* pFocusVehicle = GetFocusVehicle();
	if (!pFocusVehicle)
	{
		pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (!pFocusVehicle)
		return;

	CVehicleWeaponMgr* pVehWeaponMgr = pFocusVehicle->GetVehicleWeaponMgr();
	if (!aiVerifyf(pVehWeaponMgr, "Vehicle doesn't have a vehicle weapon manager"))
		return;

	atArray<CVehicleWeapon*> weaponArray;
	atArray<CTurret*> turretArray;
	pVehWeaponMgr->GetWeaponsAndTurretsForSeat(2, weaponArray, turretArray);

	int numTurrets = turretArray.size(); 
	for(int iTurretIndex = 0; iTurretIndex < numTurrets; iTurretIndex++)
	{
		//skip processing if the turretArray element is NULL
		if (!turretArray[iTurretIndex])
			continue;

		Matrix34 matTurretWorldTest;
		turretArray[iTurretIndex]->GetTurretMatrixWorld(matTurretWorldTest,pFocusVehicle);
		Vector3 vTurretPos = matTurretWorldTest.d;
		Vector3 vToPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vTurretPos;
		vToPedPos.RotateZ(PI);
		Vector3 vAimPos = vTurretPos + vToPedPos;
		turretArray[iTurretIndex]->AimAtWorldPos(vAimPos, pFocusVehicle, false, false);
	}
}

void CVehicleDebug::SetPlayerJackRateCB()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed && pPed->GetPlayerInfo())
	{
		pPed->GetPlayerInfo()->JackSpeed = ms_iPlayerJackRate;
	}
}

void CVehicleDebug::RenderTestCapsuleAtPosition(const CPed* pPed, Vec3V_In vPos, Vec3V_In vUpVec, Color32 colour, u32 uTime, const CVehicle& rVeh, float fStartZOffset)
{
	const CBipedCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo()->GetBipedCapsuleInfo();
	if(pCapsuleInfo)
	{
		Vec3V vCapsuleBottom = vPos + vUpVec * ScalarVFromF32((pCapsuleInfo->GetHeadHeight() - pCapsuleInfo->GetRadius()) + fStartZOffset);
		Vec3V vCapsuleTop = vPos + vUpVec * ScalarVFromF32(pCapsuleInfo->GetCapsuleZOffset() + pCapsuleInfo->GetRadius());

		CTask::ms_debugDraw.AddSphere(vCapsuleBottom, 0.025f, Color_blue, uTime);
		CTask::ms_debugDraw.AddSphere(vCapsuleTop, 0.025f, Color_yellow, uTime);
		CTask::ms_debugDraw.AddCapsule(vCapsuleBottom, vCapsuleTop, pCapsuleInfo->GetRadius() * CTaskVehicleFSM::GetEntryRadiusMultiplier(rVeh), colour, uTime, 0, false);
	}
}

void CVehicleDebug::RenderEntryPoints(CVehicle* pVehicle, const CEntryExitPoint* pEntryPoint, s32 iEntryIndex, s32 iNumTexts)
{
	Color32 colour = Color_green;

	if (pVehicle && pEntryPoint)
	{
		if (ms_bRenderEntryPoints)
		{
			// Render text at the entry point
			Vector3 vEntryPos(Vector3::ZeroType);
			Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
			CModelSeatInfo::CalculateEntrySituation(pVehicle, NULL, vEntryPos, qEntryOrientation, iEntryIndex);

			char szText[64];
			formatf(szText, "Entry Point %i Vehicle Roll %.2f", iEntryIndex, pVehicle->GetTransform().GetRoll());
			grcDebugDraw::Text(vEntryPos, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
			grcDebugDraw::Sphere(vEntryPos, 0.05f, colour);

			if (ms_bRenderLayoutInfo)
			{
				sprintf(szText, "Info: %s", pVehicle->GetLayoutInfo()->GetEntryPointInfo(iEntryIndex)->GetName().GetCStr());
				grcDebugDraw::Text(vEntryPos, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
				sprintf(szText, "Anim: %s", pVehicle->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryIndex)->GetName().GetCStr());
				grcDebugDraw::Text(vEntryPos, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
			}

			if (ms_bRenderEntryAnim)
			{
				static float sfScale = 0.25f;
				Vector3 vEulers;
				qEntryOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(vEntryPos),VECTOR3_TO_VEC3V(vEntryPos + vDir), 0.025f, colour);
			}

			if (ms_bRenderExitAnim)
			{
				Vector3 vExitPos(Vector3::ZeroType);
				Quaternion qExitOrientation(0.0f,0.0f,0.0f,1.0f);
				CModelSeatInfo::CalculateExitSituation(pVehicle, vExitPos, qExitOrientation, iEntryIndex);
				static float sfScale = 0.25f;
				Vector3 vEulers;
				qExitOrientation.ToEulers(vEulers, "xyz");
				Vector3 vDir(0.0f,1.0f,0.0f);
				vDir.RotateZ(fwAngle::LimitRadianAngle(vEulers.z));
				vDir.Scale(sfScale);
				grcDebugDraw::Sphere(RCC_VEC3V(vExitPos), 0.05f, Color_orange);
				grcDebugDraw::Arrow(RCC_VEC3V(vExitPos),VECTOR3_TO_VEC3V(vExitPos + vDir), 0.025f, Color_orange);
			}

			s32 iNumAccessibleSeats = pEntryPoint->GetSeatAccessor().GetNumSeatsAccessible();

			for (s32 i=0; i<iNumAccessibleSeats; i++)
			{
				s32 iAccessibleSeatIndex = pEntryPoint->GetSeatAccessor().GetSeatByIndex(i);

				if (iAccessibleSeatIndex > -1)
				{
					s32 iBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iAccessibleSeatIndex);

					Matrix34 m_BoneMat;
					pVehicle->GetGlobalMtx(iBoneIndex, m_BoneMat);

					if (ms_bRenderEntrySeatLinks)
					{
						grcDebugDraw::Line(RCC_VEC3V(vEntryPos), RCC_VEC3V(m_BoneMat.d), colour, Color_blue);
					}
				}
			}
		}
	}
}

void CVehicleDebug::RenderSeats(CVehicle* pVehicle, s32 iNumTexts)
{
	Color32 colour = Color_blue;

	if (pVehicle && pVehicle->GetLayoutInfo())
	{
		if (ms_bRenderSeats)
		{		
			for (s32 i=0; i<pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumSeats(); i++)
			{
				s32 iBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(i);
				Matrix34 m_BoneMat;
				pVehicle->GetGlobalMtx(iBoneIndex, m_BoneMat);		

				if (ms_bRenderThroughWindscreenAnim)
				{
					Vector3 vExitPos(Vector3::ZeroType);
					if (CModelSeatInfo::CalculateThroughWindowPosition(*pVehicle, vExitPos, i))
					{
						grcDebugDraw::Line(m_BoneMat.d, vExitPos, Color_pink);
						grcDebugDraw::Sphere(vExitPos, 0.05f, Color_pink);
					}
				}

				char szText[64];
				formatf(szText, "Seat %i", i);
				grcDebugDraw::Text(m_BoneMat.d, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
				grcDebugDraw::Sphere(m_BoneMat.d, 0.05f, colour);
				static float sfScale = 0.25f;
				Vector3 vDir = m_BoneMat.b;
				vDir.Scale(sfScale);
				grcDebugDraw::Arrow(RCC_VEC3V(m_BoneMat.d), VECTOR3_TO_VEC3V(m_BoneMat.d + vDir), 0.1f, colour);

				if (pVehicle->IsTurretSeat(i))
				{
					const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(i);
					if (pSeatAnimInfo && pSeatAnimInfo->IsStandingTurretSeat())
					{
						CPed* pPed = CGameWorld::FindLocalPlayer();
						if (pPed)
						{
							CVehicleWeaponMgr* pVehWeaponMgr = pVehicle->GetVehicleWeaponMgr();
							if (pVehWeaponMgr)
							{
								atArray<CVehicleWeapon*> weaponArray;
								atArray<CTurret*> turretArray;
								pVehWeaponMgr->GetWeaponsAndTurretsForSeat(i, weaponArray, turretArray);

								int numTurrets = turretArray.size(); 
								for(int iTurretIndex = 0; iTurretIndex < numTurrets; iTurretIndex++)
								{
									//skip processing if the turretArray element is NULL
									if (!turretArray[iTurretIndex])
										continue;

									Matrix34 idealMtx = CTaskVehicleFSM::ComputeIdealSeatMatrixForPosition(*turretArray[iTurretIndex], *pVehicle, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), i);
									formatf(szText, "Predicted Seat %i", i);
									grcDebugDraw::Text(idealMtx.d, Color_cyan, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
									grcDebugDraw::Sphere(idealMtx.d, 0.05f, Color_cyan);
									static float sfScale = 0.25f;
									Vector3 vDir = idealMtx.b;
									vDir.Scale(sfScale);
									grcDebugDraw::Arrow(RCC_VEC3V(idealMtx.d), VECTOR3_TO_VEC3V(idealMtx.d + vDir), 0.1f, Color_cyan);
								}
							}
						}
					}
				}

				// Figure out which side the seat is on
				if(iBoneIndex > -1)
				{
					const crBoneData* pBoneData = pVehicle->GetSkeletonData().GetBoneData(iBoneIndex);
					Assert(pBoneData);
					bool bLeft = pBoneData->GetDefaultTranslation().GetXf() < 0.0f;

					if( bLeft )
					{
						grcDebugDraw::Text(m_BoneMat.d, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, "Left Side");
					}
					else
					{
						grcDebugDraw::Text(m_BoneMat.d, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, "Right Side");
					}
				}

				if (ms_bRenderLayoutInfo)
				{
					sprintf(szText, "Info: %s", pVehicle->GetLayoutInfo()->GetSeatInfo(i)->GetName().GetCStr());
					grcDebugDraw::Text(m_BoneMat.d, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
					sprintf(szText, "Anim: %s", pVehicle->GetLayoutInfo()->GetSeatAnimationInfo(i)->GetName().GetCStr());
					grcDebugDraw::Text(m_BoneMat.d, colour, 0, iNumTexts++ * ms_iDebugVerticalOffset, szText);
				}
			}
		}
	}
}

void CVehicleDebug::RenderDoorComponentReservation(CVehicle* pVehicle, const CEntryExitPoint* pEntryPoint, s32 iEntryIndex, CComponentReservation* pComponentReservation, s32 iNumTexts)
{
	Color32 colour = Color_orange;

	if (pVehicle && pEntryPoint && pComponentReservation)
	{
		if (ms_bRenderComponentReservationText)
		{
			// Render text at the entry point
			Vector3 vEntryPos(Vector3::ZeroType);
			if (pEntryPoint->GetEntryPointPosition(pVehicle, NULL, vEntryPos))
			{
				char szText[64];
				CPed* pPed = pComponentReservation->GetPedUsingComponent();
				if (pPed)
				{
#if __ASSERT
                    const char *pedName = pPed->GetDebugName();
#else
                    const char *pedName = pPed->GetPedModelInfo()->GetModelName();
#endif // __ASSERT

					formatf(szText, "Door %i Reservation: %s", iEntryIndex, pedName);
					grcDebugDraw::Text(vEntryPos, colour, 0, iNumTexts * ms_iDebugVerticalOffset, szText);
				}
				else
				{
					formatf(szText, "Door %i Reservation: None", iEntryIndex);
					grcDebugDraw::Text(vEntryPos, colour, 0, iNumTexts * ms_iDebugVerticalOffset, szText);
				}
			}
		}

		RenderComponentUsage(pVehicle, pComponentReservation, colour);
	}
}

void CVehicleDebug::RenderSeatComponentReservation(CVehicle* pVehicle, const CEntryExitPoint* pEntryPoint, s32 iEntryIndex, CComponentReservation* pComponentReservation, s32 iNumTexts)
{
	Color32 colour = Color_blue;

	if (pVehicle && pEntryPoint && pComponentReservation)
	{
		if (ms_bRenderComponentReservationText)
		{
			// Render text at the entry point
			Vector3 vEntryPos(Vector3::ZeroType);
			if (pEntryPoint->GetEntryPointPosition(pVehicle, NULL, vEntryPos))
			{
				char szText[64];
				CPed* pPed = pComponentReservation->GetPedUsingComponent();
				if (pPed)
				{
#if __ASSERT
                    const char *pedName = pPed->GetDebugName();
#else
                    const char *pedName = pPed->GetPedModelInfo()->GetModelName();
#endif // __ASSERT
					formatf(szText, "Seat %i Reservation: %s", iEntryIndex, pedName);
					grcDebugDraw::Text(vEntryPos, colour, 0, iNumTexts * ms_iDebugVerticalOffset, szText);
				}
				else
				{
					formatf(szText, "Seat %i Reservation: None", iEntryIndex);
					grcDebugDraw::Text(vEntryPos, colour, 0, iNumTexts * ms_iDebugVerticalOffset, szText);
				}
			}
		}

		RenderComponentUsage(pVehicle, pComponentReservation, colour);
	}
}

void CVehicleDebug::RenderComponentUsage(CVehicle* pVehicle, CComponentReservation* pComponentReservation, Color32 colour)
{
	if (ms_bRenderComponentReservationUsage)
	{
		// Render debug showing which ped is using the component
		Vector3 vComponentPos;
		s32 boneId = pComponentReservation->GetBoneIndex();
		if (boneId!=-1)
		{
			Matrix34 m_BoneMat;
			pVehicle->GetGlobalMtx(boneId, m_BoneMat);
			vComponentPos = m_BoneMat.d;

			// If a ped is using the component, change the render color to green to highlight it
			CPed* pPed = pComponentReservation->GetPedUsingComponent();
			if (pPed)
			{
				colour = Color_green;
				grcDebugDraw::Line(vComponentPos, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), colour);
			}

			grcDebugDraw::Sphere(vComponentPos, 0.05f, colour);
		}
	}
}


#endif // __BANK
