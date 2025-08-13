// File header
#include "Task/Scenario/ScenarioDebug.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Response/TaskFlee.h"

#if SCENARIO_DEBUG

// Framework headers
#include "fwdebug/vectormap.h"
#include "fwsys/metadatastore.h"

// Game headers
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/Ambient/ConditionalAnimManager.h"
#include "ai/BlockingBounds.h"
#include "bank/button.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "debug/Debug.h"
#include "Event/Events.h"
#include "game/Clock.h"
#include "Network/Network.h"
#include "Peds/ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedplacement.h"
#include "Peds/pedpopulation.h"
#include "Peds/PopZones.h"
#include "physics/physics.h"
#include "scene/FocusEntity.h"
#include "scene/world/GameWorld.h"
#include "script/script_brains.h"
#include "script/script_cars_and_peds.h"
#include "streaming/streaming.h"
#include "task/General/TaskBasic.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Default/TaskWander.h"
#include "task/Scenario/info/ScenarioActionCondition.h"
#include "Task/Scenario/info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioChainingTests.h"
#include "Task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/ScenarioPointRegion.h"
#include "Task/Scenario/ScenarioVehicleManager.h"
#include "Task/Scenario/Types/TaskParkedVehicleScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "task/Response/TaskFlee.h"
#include "weapons/WeaponChannel.h"
#include "modelinfo/ModelInfo_Factories.h"

AI_OPTIMISATIONS()

// Static initialisation
bkBank* CScenarioDebug::ms_pBank = NULL;
bkButton* CScenarioDebug::ms_pCreateEditorButton = NULL;
bkButton* CScenarioDebug::ms_pCreateConditionalAnimsButton = NULL;
bkButton* CScenarioDebug::ms_pCreateDebugButton = NULL;
bkButton* CScenarioDebug::ms_pCreateModelSetsButton = NULL;
bkButton* CScenarioDebug::ms_pCreateOtherButton = NULL;
bkButton* CScenarioDebug::ms_pCreateScenarioInfoButton = NULL;

atArray<const char*> CScenarioDebug::ms_szScenarioNames;
s32 CScenarioDebug::ms_iScenarioComboIndex = 0;
s32 CScenarioDebug::ms_iAvailabilityComboIndex = 0;
atArray<CScenarioDebug::ConditionalAnimData> CScenarioDebug::ms_ConditionalAnims;
atArray<const char*> CScenarioDebug::ms_ConditionalAnimNames;
s32 CScenarioDebug::ms_iAmbientContextComboIndex = 0;
atQueue<CScenarioDebug::ScenarioPointMemory, 32> CScenarioDebug::ms_WarpToNearestAlreadyVisited;
int CScenarioDebug::ms_NextScenarioPropIndex = 0;
int CScenarioDebug::ms_WarpToNearestScenarioType = -1;
RegdObj CScenarioDebug::ms_DebugScenarioProp;
bool CScenarioDebug::ms_UseFixedPhysicsForProps = false;
bool CScenarioDebug::ms_UseScenarioCreateProp = true;
bool CScenarioDebug::ms_UseScenarioFindExisting = true;
bool CScenarioDebug::ms_UseScenarioWarp = true;
bool CScenarioDebug::ms_StopTargetPedImmediate = true;
bool CScenarioDebug::ms_bPlayBaseClip = true;
bool CScenarioDebug::ms_bInstantlyBlendInBaseClip = true;
bool CScenarioDebug::ms_bPlayIdleClips = true;
bool CScenarioDebug::ms_bPickPermanentProp = true;
bool CScenarioDebug::ms_bForcePropCreation = true;
bool CScenarioDebug::ms_bOverrideInitialIdleTime = false;
CScenarioEditor CScenarioDebug::ms_Editor;
bank_float CScenarioDebug::ms_fRenderDistance = 100.0f;
bank_bool  CScenarioDebug::ms_bRenderScenarioPoints = false;
bank_bool  CScenarioDebug::ms_bRenderStationaryReactionsPointsOnly = false;
bank_bool  CScenarioDebug::ms_bRenderOnlyChainedPoints = false;
bank_bool  CScenarioDebug::ms_bRenderChainPointUsage = false;
bank_bool  CScenarioDebug::ms_bRenderScenarioPointsDisabled = false;
bank_bool  CScenarioDebug::ms_bRenderScenarioPointsOnlyInPopZone = false;
bank_bool  CScenarioDebug::ms_bDisableScenariosWithNoClipData = false;
bank_bool  CScenarioDebug::ms_bRenderScenarioPointRegionBounds = false;
bank_bool  CScenarioDebug::ms_bRenderScenarioPointsOnlyInTimePeriod = false;
bank_bool  CScenarioDebug::ms_bRenderScenarioPointsOnlyInTimePeriodCurrent = false;
bank_u32   CScenarioDebug::ms_iRenderScenarioPointsOnlyInTimePeriodStart = 0;
bank_u32   CScenarioDebug::ms_iRenderScenarioPointsOnlyInTimePeriodEnd = 24;
bank_float CScenarioDebug::ms_fPointHandleLength = 0.5f;
bank_float CScenarioDebug::ms_fPointHandleRadius = 0.05f;
bank_float CScenarioDebug::ms_fPointDrawRadius = 0.1f;
bank_bool  CScenarioDebug::ms_bRenderIntoVectorMap = false;
bank_bool  CScenarioDebug::ms_bRenderSpawnHistory = false;
bank_bool  CScenarioDebug::ms_bRenderSpawnHistoryText = false;
bank_bool  CScenarioDebug::ms_bRenderSpawnHistoryTextPed = true;
bank_bool  CScenarioDebug::ms_bRenderSpawnHistoryTextVeh = true;
bank_bool CScenarioDebug::ms_bDisplayScenarioModelSetsOnVectorMap = false;
bank_bool CScenarioDebug::ms_bAnimDebugDisableSPRender = false; //set in CScenarioAnimDebugHelper
bank_bool CScenarioDebug::ms_bRenderScenarioSpawnCheckCache = false;
bank_bool CScenarioDebug::ms_bRenderPointText = true;
bank_bool CScenarioDebug::ms_bRenderPointRadius = true;
bank_bool CScenarioDebug::ms_bRenderPointAddress = false;
bank_bool CScenarioDebug::ms_bRenderClusterInfo = false;
bank_bool CScenarioDebug::ms_bRenderClusterReport = false;
bank_bool CScenarioDebug::ms_bRenderNonRegionSpatialArray = false;
bank_bool CScenarioDebug::ms_bRenderSummaryInfo = false;
bank_bool CScenarioDebug::ms_bRenderPopularTypesInSummary = false;
bank_bool CScenarioDebug::ms_bRenderSelectedTypeOnly = false;
bank_bool CScenarioDebug::ms_bRestrictVehicleAttractionToSelectedPoint = false;
bank_bool CScenarioDebug::ms_bRenderSelectedScenarioGroup = false;

bkButton* CScenarioDebug::ms_pCreateScenarioReactsButton = NULL;
bkButton* CScenarioDebug::ms_pTriggerReactsButton = NULL;
bkButton* CScenarioDebug::ms_pTriggerAllReactsButton = NULL;
bkButton* CScenarioDebug::ms_pSpawnScenarioButton = NULL;
bkButton* CScenarioDebug::ms_pCleanUpScenarioButton = NULL;
bkButton* CScenarioDebug::ms_pSpawnPropButton = NULL;
bkCombo* CScenarioDebug::ms_scenarioGroupCombo = NULL;
atArray<const char*> CScenarioDebug::ms_szScenarioTypeNames;
atArray<const char*> CScenarioDebug::ms_szPropSetNames;
atArray<const char*> CScenarioDebug::ms_szPedModelNames;
atArray<const char*> CScenarioDebug::ms_szReactionTypeNames;
atArray<CPedModelInfo*> CScenarioDebug::ms_PedModelInfoArray;
RegdPed CScenarioDebug::ms_pScenarioPed;
RegdObj CScenarioDebug::ms_pScenarioProp;
RegdPed CScenarioDebug::ms_pReactionPed;
s32* CScenarioDebug::ms_pedSortArray = NULL;
int CScenarioDebug::ms_iScenarioTypeIndex = 0;
s32 CScenarioDebug::ms_iScenarioGroupIndex = 0;
int CScenarioDebug::ms_iPropSetIndex = 0;
int CScenarioDebug::ms_iPedModelIndex = 0;
int CScenarioDebug::ms_iReactPedModelIndex = 0;
int CScenarioDebug::ms_iReactionTypeIndex = 0;
float CScenarioDebug::ms_fReactionHeading = 90.0f;
float CScenarioDebug::ms_fReactionDistance = 2.0f;
float CScenarioDebug::ms_fDistanceFromPlayer = 10.0f;
float CScenarioDebug::ms_fTimeToLeave = -1.0f;
float CScenarioDebug::ms_fTimeBetweenReactions = 10.0f;
bool CScenarioDebug::ms_bSpawnPedToUsePropScenario = true;
bool CScenarioDebug::ms_bTriggerOnFocusPed = false;
bool CScenarioDebug::ms_bPlayIntro = true;
bool CScenarioDebug::ms_bDontSpawnReactionPed = false;

static bool ms_bTriggerAllReacts = false;
static bool ms_bRetryTriggerReaction = false;
static float ms_fTimeBetweenReactsCountdown = 0.0f;

static const char* s_szAvaliablityNames[] = {"Both", "Sp Only", "Mp Only"};

void CScenarioDebug::InitBank()
{
	if(ms_pBank)
	{
		ShutdownBank();
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("Scenarios", 0, 0, false); 
	if(weaponVerifyf(ms_pBank, "Failed to create Scenarios. bank"))
	{
		ms_pCreateEditorButton = ms_pBank->AddButton("Create Scenario Editor widgets", &CScenarioDebug::CreateEditorBank);
		ms_pCreateDebugButton = ms_pBank->AddButton("Create Scenario Debug widgets", &CScenarioDebug::CreateDebugBank);
		ms_pCreateConditionalAnimsButton = ms_pBank->AddButton("Create Conditional Anims widgets", &CScenarioDebug::CreateConditionalAnimsBank);
		ms_pCreateModelSetsButton = ms_pBank->AddButton("Create Ambient Model Sets widgets", &CScenarioDebug::CreateModelSetsBank);
		ms_pCreateScenarioInfoButton = ms_pBank->AddButton("Create Scenarios Info widgets", &CScenarioDebug::CreateScenarioInfoBank);
		ms_pCreateScenarioReactsButton = ms_pBank->AddButton("Create Scenario Reacts Debug widgets", &CScenarioDebug::CreateScenarioReactsDebugBank);
		ms_pCreateOtherButton = ms_pBank->AddButton("Create Other Scenario widgets", &CScenarioDebug::CreateOtherBank);
	}
}

int CScenarioDebug::CompareModelInfos(const void* pVoidA, const void* pVoidB)
{
	const s32* pA = (const s32*)pVoidA;
	const s32* pB = (const s32*)pVoidB;

	return stricmp(CScenarioDebug::ms_PedModelInfoArray[*pA]->GetModelName(), CScenarioDebug::ms_PedModelInfoArray[*pB]->GetModelName());
}

void CScenarioDebug::SpawnProp()
{
	//check if the prop is valid
	if (ms_iPropSetIndex == 0)
	{
		return;
	}

	if (ms_pScenarioProp)
	{
		CObjectPopulation::DestroyObject(ms_pScenarioProp);
		ms_pScenarioProp = NULL;
	}

	u32 nModelIndex = CModelInfo::GetModelIdFromName(ms_szPropSetNames[ms_iPropSetIndex]).GetModelIndex();
	fwModelId modelId((strLocalIndex(nModelIndex)));
	if (CModelInfo::IsValidModelInfo(nModelIndex))
	{
		//load the selected object
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
		if(pModelInfo == NULL)
		{
			Displayf("CScenarioDebug::SpawnProp - Invalid model: %d", nModelIndex);
			return;
		}
		else
		{
			if (!pModelInfo->GetIsTypeObject())
			{
				Displayf("This is not a prop, bad naming convention. No object created.");
				return;
			}
			else
			{
				ms_pScenarioProp = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
			}

			if (ms_pScenarioProp)
			{
				Matrix34 tempMat;
				tempMat.Identity();
				
				// generate position
				Vector3 vForward = VEC3_ZERO;
				Vector3 vPosition = VEC3_ZERO;
				float fHeading = 0.0f;

				CEntity *pEnt = CGameWorld::FindLocalPlayer();
				if (pEnt && pEnt->GetIsTypePed())
				{
					vForward = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetB());
					vPosition = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition());
					fHeading = (RtoD * pEnt->GetTransform().GetHeading()) + 180.0f;

					if (fHeading < 0.0f)
					{
						fHeading += 360.0f;
					}
					if (fHeading > 360.0f)
					{
						fHeading -= 360.0f;
					}
				}
				vForward *=	(ms_fDistanceFromPlayer + 2.0f); // 1.1f is to offset its location from the spawned prop's position
				vPosition += vForward;
				vPosition.z -= 1.0f;	// adjust prop location slightly

				float fSetHeading = (DtoR * fHeading);
				fSetHeading = fwAngle::LimitRadianAngleSafe(fSetHeading);
				tempMat.RotateFullZ(fSetHeading);
				tempMat.d += vPosition;

				if (!ms_pScenarioProp->GetTransform().IsMatrix())
				{
					#if ENABLE_MATRIX_MEMBER
					Mat34V trans = MATRIX34_TO_MAT34V(tempMat);
					ms_pScenarioProp->SetTransform(trans);					
					ms_pScenarioProp->SetTransformScale(1.0f, 1.0f);		
					#else
					fwMatrixTransform* trans = rage_new fwMatrixTransform(MATRIX34_TO_MAT34V(tempMat));
					ms_pScenarioProp->SetTransform(trans);
					#endif
				}
				ms_pScenarioProp->SetMatrix(tempMat);

				if (fragInst* pInst = ms_pScenarioProp->GetFragInst())
				{
					pInst->SetResetMatrix(tempMat);
				}
				CGameWorld::Add(ms_pScenarioProp, CGameWorld::OUTSIDE );

				CObject* pDynamicEnt = static_cast<CObject*>(ms_pScenarioProp.Get()); 
				pDynamicEnt->GetPortalTracker()->RequestRescanNextUpdate();
				pDynamicEnt->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(ms_pScenarioProp->GetTransform().GetPosition()));

				pDynamicEnt->SetupMissionState();
			}
		}
	}
}

void CScenarioDebug::SpawnScenario()
{
	// spawn ped and prop (if any)
	SpawnScenarioPed();
	SpawnProp();

	if (!ms_pScenarioPed)
	{
		return;
	}

	s32 scenarioType = CScenarioManager::GetScenarioTypeFromName(ms_szScenarioTypeNames[ms_iScenarioTypeIndex]);
	if (scenarioType != Scenario_Invalid)
	{
		int flags = 0;
 		if (!ms_bSpawnPedToUsePropScenario || ms_pScenarioProp == NULL)
 		{
 			flags |= CTaskUseScenario::SF_StartInCurrentPosition;
 		}
		else
		{
			flags |= CTaskUseScenario::SF_GotoPositionTaskSet;
		}
		if (!ms_bPlayIntro)
		{
			flags &= ~CTaskUseScenario::SF_StartInCurrentPosition;
			flags |= CTaskUseScenario::SF_SkipEnterClip;
			flags |= CTaskUseScenario::SF_Warp;
		}
		if (ms_fTimeToLeave < 0.0f)
		{
			flags |= CTaskUseScenario::SF_IdleForever;
		}

		CScenarioDebugClosestFilterScenarioType filter(scenarioType);
		filter.m_IgnoreInUse = true;
		CScenarioPoint* sp = GetClosestScenarioPoint(ms_pScenarioPed->GetTransform().GetPosition(), filter);
		if(sp)
		{
			// Are we close to a point?
			Vector3 vecDistance = VEC3V_TO_VECTOR3(sp->GetPosition() - ms_pScenarioPed->GetTransform().GetPosition());
			if (vecDistance.Mag2() > rage::square(ms_fDistanceFromPlayer))
			{
				flags |= CTaskUseScenario::SF_SkipEnterClip;
			}

			CTaskUseScenario* pUseScenarioTask = rage_new CTaskUseScenario(scenarioType, sp, flags);
			if (ms_fTimeToLeave > 0.0f)
			{
				pUseScenarioTask->SetTimeToLeave(ms_fTimeToLeave / 1000.0f);
			}
			pUseScenarioTask->CreateProp();

			CTaskUnalerted* pUnalertedTask = rage_new CTaskUnalerted(pUseScenarioTask, sp, scenarioType);
			ms_pScenarioPed->GetPedIntelligence()->AddTaskDefault(pUnalertedTask);
			return;
		}

		CTaskUseScenario* pUseScenarioTask = rage_new CTaskUseScenario(scenarioType, flags);
		if (ms_fTimeToLeave > 0.0f)
		{
			pUseScenarioTask->SetTimeToLeave(ms_fTimeToLeave / 1000.0f);
		}
		pUseScenarioTask->CreateProp();

		ms_pScenarioPed->GetPedIntelligence()->AddTaskAtPriority(pUseScenarioTask, PED_TASK_PRIORITY_PRIMARY);
	}
}

void CScenarioDebug::CleanUpScenarioReactInfo()
{
	// remove prop, scenario ped, and scenario react ped if these exist
	if (ms_pScenarioPed)
	{
		CPedFactory::GetFactory()->DestroyPed(ms_pScenarioPed);
		ms_pScenarioPed = NULL;
	}
	if (ms_pReactionPed)
	{
		CPedFactory::GetFactory()->DestroyPed(ms_pReactionPed);
		ms_pReactionPed = NULL;
	}
	if (ms_pScenarioProp)
	{
		CObjectPopulation::DestroyObject(ms_pScenarioProp);
		ms_pScenarioProp = NULL;
	}
}

void CScenarioDebug::SpawnReactionPed(const Vector3& vSpawnPosition)
{
	if (ms_pReactionPed)
	{
		CPedFactory::GetFactory()->DestroyPed(ms_pReactionPed);
		ms_pReactionPed = NULL;
	}

	if (!ms_bDontSpawnReactionPed)
	{
		ms_pReactionPed = SpawnPed(vSpawnPosition, 0.0f, false);
	}
}

void CScenarioDebug::SpawnScenarioPed()
{
	if (ms_pScenarioPed)
	{
		CPedFactory::GetFactory()->DestroyPed(ms_pScenarioPed);
		ms_pScenarioPed = NULL;
	}

	float fHeading = 0.0f;

	Vector3 vForward = VEC3_ZERO;
	Vector3 vSpawnPosition = VEC3_ZERO;
	CEntity *pEnt = CGameWorld::FindLocalPlayer();
	if (pEnt && pEnt->GetIsTypePed())
	{
		vForward = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetB());
		vSpawnPosition = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition());
		fHeading = (RtoD * pEnt->GetTransform().GetHeading()) + 180.0f;

		if (fHeading < 0.0f)
		{
			fHeading += 360.0f;
		}
		if (fHeading > 360.0f)
		{
			fHeading -= 360.0f;
		}
	}
	vForward *=	ms_fDistanceFromPlayer;
	vSpawnPosition += vForward;

	ms_pScenarioPed = SpawnPed(vSpawnPosition, fHeading);
}

CPed* CScenarioDebug::SpawnPed(const Vector3& vSpawnPosition, float fHeading, bool bScenarioPed)
{
	CPed *pPed = NULL;
	Vector3 TempCoors = Vector3(vSpawnPosition);
	
	// If coordinates are valid, and either there's no networking enabled, or we have a valid handler...
	if (ABS(TempCoors.x) < WORLDLIMITS_XMAX && ABS(TempCoors.y) < WORLDLIMITS_YMAX &&
		(!CNetwork::IsNetworkOpen() || CTheScripts::GetCurrentGtaScriptHandlerNetwork()))
	{
		u32 nModelIndex = bScenarioPed 
			? CModelInfo::GetModelIdFromName(ms_szPedModelNames[ms_iPedModelIndex]).GetModelIndex()
			: CModelInfo::GetModelIdFromName(ms_szPedModelNames[ms_iReactPedModelIndex]).GetModelIndex();
		fwModelId modelId((strLocalIndex(nModelIndex)));
		if (CModelInfo::IsValidModelInfo(nModelIndex))
		{
			//load the selected ped
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
			if(pModelInfo == NULL)
			{
				Displayf("CScenarioDebug::SpawnPed - Invalid model: %d", nModelIndex);
				return NULL;
			}
			else
			{
				if (pModelInfo->GetModelType() == MI_TYPE_PED)
				{
					if (!CModelInfo::HaveAssetsLoaded(modelId))
					{
						CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD);
						CStreaming::LoadAllRequestedObjects(true);
					}
					if (!CModelInfo::HaveAssetsLoaded(modelId))
					{
						return NULL;
					}

					if (pModelInfo->GetDrawable() || pModelInfo->GetFragType())
					{
						// want to pass matrix into PedFactory
						if (TempCoors.z <= INVALID_MINIMUM_WORLD_Z)
						{
							TempCoors.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, TempCoors.x, TempCoors.y);
						}

						float fSetHeading = (DtoR * fHeading);
						fSetHeading = fwAngle::LimitRadianAngleSafe(fSetHeading);

						Matrix34 TempMat;
						TempMat.Identity();
						TempMat.RotateFullZ(fSetHeading);
						TempMat.d = TempCoors;
					
						// use pedfactory to create the ped:
						const CControlledByInfo localAiControl(false, false);
						pPed = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, &TempMat, true, true, true);

						if (pPed)
						{
							pPed->SetVarRandom(PV_FLAG_NONE, PV_FLAG_NONE, NULL, NULL, NULL, PV_RACE_UNIVERSAL);
							pPed->SetDesiredHeading( fSetHeading );
							pPed->SetHeading( fSetHeading );
							pPed->SetLodDistance(CPed::ms_uMissionPedLodDistance);

							CScriptEntities::ClearSpaceForMissionEntity(pPed);
							pPed->ActivatePhysics();

							CGameWorld::Add(pPed, CGameWorld::OUTSIDE );

							pPed->GetPortalTracker()->RequestRescanNextUpdate();
							pPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
						}
					}
				}
			}
		}
	}
	return pPed;
}

void CScenarioDebug::TriggerAllReacts()
{
	ms_iReactionTypeIndex = 0;
	ms_bSpawnPedToUsePropScenario = true;
	ms_bTriggerAllReacts = true;
	ms_bPlayIntro = true;
	ms_fTimeBetweenReactsCountdown = 0.0f;
}

void CScenarioDebug::TriggerReacts()
{
	CPed* pPed = ms_pScenarioPed;

	if (ms_bTriggerOnFocusPed)
	{
		pPed = CPedDebugVisualiserMenu::GetFocusPed();
		if (!pPed)
		{
			pPed = ms_pScenarioPed; // maybe this was checked by accident?
		}
	}

	if (pPed)
	{
		// early out
		CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
		if (!pTask)
		{
			ms_bRetryTriggerReaction = true;	// try one more time
			return;
		}

		Vector3 direction = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
		Vector3 eventPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		float fSin = sinf(ms_fReactionHeading);
		float fCos = cosf(ms_fReactionHeading);

		Vector3 eventDirection = Vector3(direction.GetX() * fCos - direction.GetY() * fSin, direction.GetX() * fSin + direction.GetY() * fCos, direction.GetZ());

		eventDirection *=	ms_fReactionDistance;
		eventPosition += eventDirection;

		// always spawn the ped now for visual clarity
		SpawnReactionPed(eventPosition);

		// if we have the ped, use it, otherwise use the eventPosition
		CAITarget target;
		if (ms_pReactionPed)
		{
			target = CAITarget(ms_pReactionPed);
		}
		else
		{
			target = CAITarget(eventPosition);
		}

		// preset the position
		CEventScenarioForceAction e;
		Vector3 position;
		target.GetPosition(position);
		e.SetSourcePos(position);

		switch (ms_iReactionTypeIndex)
		{
		case 0: // PanicExitToFlee
			{
				e.SetForceActionType(eScenarioActionFlee);
				e.SetTask(rage_new CTaskSmartFlee(target));
			}
			break;
		case 1: // PanicExitToCombat
			{
				e.SetForceActionType(eScenarioActionCombatExit);
				e.SetTask(rage_new CTaskThreatResponse(ms_pReactionPed));
			}
			break;
		case 2: // TriggerAggroThenExit
			{
				e.SetForceActionType(eScenarioActionThreatResponseExit);
				e.SetTask(rage_new CTaskThreatResponse(ms_pReactionPed));
			}
			break;
		case 3: // TriggerShockAnimation
			{
				e.SetForceActionType(eScenarioActionShockReaction);
				e.SetTask(rage_new CTaskThreatResponse(ms_pReactionPed));
			}
			break;
		case 4: // TriggerHeadLook
			{
				e.SetForceActionType(eScenarioActionHeadTrack);
			}
			break;
		default:
			; // do nothing
		}

		// finally, add the event
		pPed->GetPedIntelligence()->AddEvent(e);
	}
}

void CScenarioDebug::CreateScenarioReactsDebugBank()
{
	aiAssertf(ms_pBank, "Scenario bank needs to be created first");
	aiAssertf(ms_pCreateScenarioReactsButton, "Scenario bank needs to be created first");

	// remove the button. don't want it clickable again
	ms_pCreateScenarioReactsButton->Destroy();
	ms_pCreateScenarioReactsButton = NULL;
	
	// grab the bank
	bkBank* pBank = ms_pBank;

	// clean up the data
	ms_szScenarioTypeNames.Reset();
	ms_szPropSetNames.Reset();
	ms_szPedModelNames.Reset();

	// get the data for the scenario type combo box
	const int typeCount = SCENARIOINFOMGR.GetScenarioCount(true);
	ms_szScenarioTypeNames.Reserve(typeCount);
	for (s32 i = 0; i < typeCount; ++i)
	{
		ms_szScenarioTypeNames.Grow() = SCENARIOINFOMGR.GetNameForScenario(i);
	}

	// get the data for the scenario propset to use combo box
	ms_szPropSetNames.PushAndGrow("INVALID");

	//look through the permanent archetypes
	const u32 maxModelInfos = CModelInfo::GetMaxModelInfos();
	for (u32 i = CModelInfo::GetStartModelInfos(); i < maxModelInfos; ++i)
	{
		CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(i)));

		if (modelInfo && modelInfo->GetModelName())
		{
			atString searchText("prop_");
			atString modelName(modelInfo->GetModelName());
			searchText.Lowercase(); 
			modelName.Lowercase(); 
			if (modelName.IndexOf(searchText) >= 0)
			{
				if (modelInfo->GetModelType() != MI_TYPE_PED && modelInfo->GetModelType() != MI_TYPE_VEHICLE)
				{
					if (modelInfo->GetModelName())
					{
						// Add the object name to the array of model names
						ms_szPropSetNames.PushAndGrow(modelInfo->GetModelName());
					}
				}
			}
		}
	}

	if (ms_szPropSetNames.GetCount() > 1)
	{
		// Sort the model names alphabetically
		std::sort(&ms_szPropSetNames[1], &ms_szPropSetNames[1] + ms_szPropSetNames.GetCount() - 1, AlphabeticalSort());
	}

	const int propSetCount = ms_szPropSetNames.size();

	// get the data for the scenario ped and scenario reaction ped models to use combo box
	// num ped models
	fwArchetypeDynamicFactory<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
	pedModelInfoStore.GatherPtrs(ms_PedModelInfoArray);
	const int pedModelCount = ms_PedModelInfoArray.GetCount();
	ms_szPedModelNames.Reserve(pedModelCount);

	// create the sort array
	if (ms_pedSortArray)
	{
		delete[] ms_pedSortArray;	// shouldn't happen, but may if multiple button click events happen...
	}
	ms_pedSortArray = rage_new s32[pedModelCount];
	for(u32 i = 0; i < pedModelCount; ++i)
	{
		ms_pedSortArray[i] = i;
	}

	//get the modelInfo associated with this ped ID
	qsort(ms_pedSortArray, pedModelCount, sizeof(s32), CompareModelInfos);
	for(u32 i = 0; i < pedModelCount; ++i)
	{
		ms_szPedModelNames.PushAndGrow(ms_PedModelInfoArray[ms_pedSortArray[i]]->GetModelName());
	}

	// grab the data for the reaction type to use
	ms_szReactionTypeNames.PushAndGrow("PanicExitToFlee");
	ms_szReactionTypeNames.PushAndGrow("PanicExitToCombat");
	ms_szReactionTypeNames.PushAndGrow("TriggerAggroThenExit");
	ms_szReactionTypeNames.PushAndGrow("TriggerShockAnimation");
	ms_szReactionTypeNames.PushAndGrow("TriggerHeadLook");
	const int reactionTypeCount = ms_szReactionTypeNames.size();

	// add the widgets
	pBank->PushGroup("Scenario Reacts Debug", false);
	{
		pBank->PushGroup("Base Scenario Info", false);
		{
			pBank->AddCombo("Scenario Type",				&CScenarioDebug::ms_iScenarioTypeIndex,		typeCount,			&CScenarioDebug::ms_szScenarioTypeNames[0]);
			pBank->AddCombo("Scenario Ped Model",			&CScenarioDebug::ms_iPedModelIndex,			pedModelCount,		&CScenarioDebug::ms_szPedModelNames[0]);
			pBank->AddSlider("Distance From Player",		&CScenarioDebug::ms_fDistanceFromPlayer, 0.0f, 100.0f, 1.0f);
			pBank->AddSlider("Time To Leave Scenario (ms)",	&CScenarioDebug::ms_fTimeToLeave, -1.0f, 10000.0f, 1.0f);
			pBank->AddToggle("Play Intro", &CScenarioDebug::ms_bPlayIntro);
			ms_pSpawnScenarioButton = ms_pBank->AddButton("Spawn Scenario", &CScenarioDebug::SpawnScenario);

			pBank->AddSeparator();

			ms_pCleanUpScenarioButton = ms_pBank->AddButton("Clean up", &CScenarioDebug::CleanUpScenarioReactInfo);
		}
		pBank->PopGroup();

		pBank->AddSeparator();

		pBank->PushGroup("Scenario Prop Info", false);
		{
			pBank->AddCombo("Scenario PropSet Model",			&CScenarioDebug::ms_iPropSetIndex,			propSetCount,		&CScenarioDebug::ms_szPropSetNames[0]);
			pBank->AddToggle("Have Scenario Ped Spawn And Use Prop Scenario", &CScenarioDebug::ms_bSpawnPedToUsePropScenario);
			ms_pSpawnPropButton = ms_pBank->AddButton("Spawn Prop", &CScenarioDebug::SpawnProp);
		}
		pBank->PopGroup();

		pBank->AddSeparator();

		pBank->PushGroup("Force Scenario Reactions", false);
		{
			pBank->AddCombo("Scenario Reaction Type",	&CScenarioDebug::ms_iReactionTypeIndex,		reactionTypeCount,	&CScenarioDebug::ms_szReactionTypeNames[0]);
			pBank->AddCombo("Scenario React Ped Model", &CScenarioDebug::ms_iReactPedModelIndex,	pedModelCount,		&CScenarioDebug::ms_szPedModelNames[0]);
			pBank->AddToggle("Trigger Event on Focus Ped", &CScenarioDebug::ms_bTriggerOnFocusPed);
			pBank->AddToggle("Dont Spawn Reaction Ped", &CScenarioDebug::ms_bDontSpawnReactionPed);
			ms_pTriggerReactsButton = ms_pBank->AddButton("Trigger Scenario Reaction", &CScenarioDebug::TriggerReacts);
			pBank->AddSlider("Relative Heading From Scenario Ped to Cause Reaction", &CScenarioDebug::ms_fReactionHeading, -180.0f, 180.0f, 1.0f);
			pBank->AddSlider("Relative Distance From Scenario Ped to Cause Reaction", &CScenarioDebug::ms_fReactionDistance, 0.0f, 100.0f, 1.0f);
			pBank->AddSlider("Time between Reactions (sec)", &CScenarioDebug::ms_fTimeBetweenReactions, 0.0f, 100.0f, 1.0f);
			ms_pTriggerAllReactsButton = ms_pBank->AddButton("Trigger All Scenario Reactions", &CScenarioDebug::TriggerAllReacts);
		}
		pBank->PopGroup();
	}
	pBank->PopGroup();
}

void CScenarioDebug::CreateEditorBank()
{
	aiAssertf(ms_pBank, "Scenario bank needs to be created first");
	aiAssertf(ms_pCreateEditorButton, "Scenario bank needs to be created first");

	ms_pCreateEditorButton->Destroy();
	ms_pCreateEditorButton = NULL;

	// FF: In case the user didn't run with -scenarioedit, we need to flush the loaded files,
	// set the NOT_IN_PLACE_LOADABLE flag, and load them back again. Otherwise, we would get crashes
	// when trying to manipulate the runtime structures (acceleration grid, etc).
	parStructure* pointRegStruct = CScenarioPointRegion::parser_GetStaticStructure();
	if (pointRegStruct)
	{
		if(!pointRegStruct->GetFlags().IsSet(parStructure::NOT_IN_PLACE_LOADABLE))
		{
			aiDisplayf("Scenario editor may become enabled, reloading scenario point files to set NOT_IN_PLACE_LOADABLE.");
			SCENARIOPOINTMGR.BankLoad();
			ms_Editor.PostLoadData();

			g_fwMetaDataStore.SetAllowLoadInResourceMem(false);
			pointRegStruct->GetFlags().Set(parStructure::NOT_IN_PLACE_LOADABLE);
		}
	}


	bkBank* pBank = ms_pBank;
	ms_Editor.AddWidgets(pBank);
}

void CScenarioDebug::CreateConditionalAnimsBank()
{
	aiAssertf(ms_pBank, "Scenario bank needs to be created first");
	aiAssertf(ms_pCreateConditionalAnimsButton, "Scenario bank needs to be created first");

	ms_pCreateConditionalAnimsButton->Destroy();
	ms_pCreateConditionalAnimsButton = NULL;
	bkBank* pBank = ms_pBank;

	CONDITIONALANIMSMGR.AddWidgets(*pBank);
}

void CScenarioDebug::CreateDebugBank()
{
	aiAssertf(ms_pBank, "Scenario bank needs to be created first");
	aiAssertf(ms_pCreateDebugButton, "Scenario bank needs to be created first");

	ms_pCreateDebugButton->Destroy();
	ms_pCreateDebugButton = NULL;
	bkBank* pBank = ms_pBank;

	ms_Editor.RebuildScenarioGroupNameList();

	pBank->PushGroup("Debug", false);

	pBank->AddToggle("Render Selected Type Only", &ms_bRenderSelectedTypeOnly);
	pBank->AddToggle("Render Summary", &ms_bRenderSummaryInfo);
	pBank->AddToggle("Render Type Popularity in Summary", &ms_bRenderPopularTypesInSummary);
	pBank->AddToggle("Render Scenario Points", &ms_bRenderScenarioPoints);
	pBank->AddToggle("Render Point Text", &ms_bRenderPointText);
	pBank->AddToggle("Render Point Radius", &ms_bRenderPointRadius);
	pBank->AddToggle("Render Point Address", &ms_bRenderPointAddress);
	pBank->AddToggle("Render stationary reactions points only", &ms_bRenderStationaryReactionsPointsOnly);
	pBank->AddToggle("Render Chained Points ONLY", &ms_bRenderOnlyChainedPoints);
	pBank->AddToggle("Render Chained Points USAGE", &ms_bRenderChainPointUsage);
	pBank->AddToggle("Render Cluster Info", &ms_bRenderClusterInfo);
	//pBank->AddToggle("Render Cluster Report", &ms_bRenderClusterReport);
	pBank->AddSlider("Render Distance", &ms_fRenderDistance, 1.0f, 10000.0f, 10.0f);
	pBank->AddToggle("Render Region Bounds", &ms_bRenderScenarioPointRegionBounds);
	pBank->AddToggle("Render In Vector Map", &ms_bRenderIntoVectorMap);
	pBank->AddToggle("Render Disabled (Group/Mode) Scenario Points", &ms_bRenderScenarioPointsDisabled);
	pBank->AddToggle("Render Spawn History Links", &ms_bRenderSpawnHistory);
	pBank->AddToggle("Render Spawn History Text", &ms_bRenderSpawnHistoryText);
	pBank->AddToggle("Render Spawn History Text Ped", &ms_bRenderSpawnHistoryTextPed);
	pBank->AddToggle("Render Spawn History Text Vehicle", &ms_bRenderSpawnHistoryTextVeh);
	pBank->AddButton("Clear Spawn History", datCallback(CFA(CScenarioDebug::ClearSpawnHistoryCB)));
	pBank->AddToggle("Render Non Region Point Spatial Array", &ms_bRenderNonRegionSpatialArray);
	pBank->AddCombo("Restrict By Availability", &ms_iAvailabilityComboIndex, 3, &s_szAvaliablityNames[0]);
	pBank->AddToggle("Restrict Render to Current PopZone", &ms_bRenderScenarioPointsOnlyInPopZone);
	pBank->AddToggle("Render Scenario Spawn Check Cache", &ms_bRenderScenarioSpawnCheckCache);
	pBank->AddSeparator();
	pBank->AddToggle("Restrict Render to Time Period", &ms_bRenderScenarioPointsOnlyInTimePeriod);
	pBank->AddSlider("Start Time", &ms_iRenderScenarioPointsOnlyInTimePeriodStart, 0, 24, 1);
	pBank->AddSlider("End Time", &ms_iRenderScenarioPointsOnlyInTimePeriodEnd, 0, 24, 1);
	pBank->AddToggle("Use Current Time", &ms_bRenderScenarioPointsOnlyInTimePeriodCurrent);
	pBank->AddSeparator();
	pBank->AddToggle("Disable Incomplete Scenarios", &ms_bDisableScenariosWithNoClipData);
	pBank->AddToggle("Prevent Base Anim Streaming", &CTaskAmbientClips::sm_DebugPreventBaseAnimStreaming);

	const int count = SCENARIOINFOMGR.GetScenarioCount(true);

	ms_szScenarioNames.Reset();
	ms_szScenarioNames.Reserve(count);

	for (s32 i = 0; i < count; ++i)
	{
		ms_szScenarioNames.Grow() = SCENARIOINFOMGR.GetNameForScenario(i);
	}

	if(count)
	{
		pBank->AddCombo("Scenarios", &ms_iScenarioComboIndex, count, &ms_szScenarioNames[0]);
	}

	pBank->AddToggle("Render Selected Scenario Group", &ms_bRenderSelectedScenarioGroup);
	ms_scenarioGroupCombo = pBank->AddCombo("SelectedGroup", &ms_iScenarioGroupIndex, ms_Editor.m_GroupNames.GetCount(), &ms_Editor.m_GroupNames[0]);
	//B*134849, 242822
	pBank->AddButton("Warp to (Next) Nearest", WarpToNextNearestScenarioCB);
	pBank->AddButton("Warp to Previous", WarpToPreviousScenarioCB);

	pBank->AddSeparator();
	pBank->AddButton("Set Target Ped to Use Selected Scenario", CreateAndActivateScenarioOnSelectedPedCB, "Find or create a scenario of the requested type, and make the focus ped use it, either warping or navigating to it.");
	pBank->AddToggle("Create Prop Scenario is Attached to", &ms_UseScenarioCreateProp, NullCB, "If the selected scenario type is attached to a prop, create a matching prop first.");
	pBank->AddToggle("Use Fixed Physics for Props", &ms_UseFixedPhysicsForProps, NullCB, "");
	pBank->AddToggle("Try to Find Existing Scenario", &ms_UseScenarioFindExisting, NullCB, "Look for existing scenarios of the matching type before creating a new one.");
	pBank->AddToggle("Warp to Scenario", &ms_UseScenarioWarp, NullCB, "When using the selected scenario, warp straight into it rather than navigating.");
	pBank->AddSeparator();
	pBank->AddButton("Stop Target Ped Using Scenario", DeActivateScenarioOnSelectedPedCB, "Stop the focus ped from using a scenario, either immediately or by playing exit animations");
	pBank->AddToggle("Stop Immediately", &ms_StopTargetPedImmediate, NullCB, "When stopping the ped from using a scenario, stop immediately without playing exit animations.");
	pBank->AddSeparator();
	pBank->AddButton("Destroy Prop Created for Attached Scenario", DestroyScenarioProp, "If \"Set Target Ped to Use Selected Scenario\" has created a prop, destroy it.");
	pBank->AddSeparator();
	pBank->AddButton("Load Prop Offsets from Clips", LoadPropOffsetsCB);


	pBank->AddButton("Force spawn in area", ForceSpawnInAreaCB, "Define the area using the measuring");
	pBank->AddToggle("Display scenario blocking areas.", &CScenarioBlockingAreas::ms_bRenderScenarioBlockingAreas);	
	//pBank->AddColor("Scenario blocking areas debug draw color", &CScenarioBlockingAreas::ms_RenderScenarioBlockingAreasColor);

	pBank->AddButton("Remove all random peds", CPedPopulation::RemoveAllRandomPeds);
	pBank->AddToggle("Only allow selected scenario to attract vehicles.", &ms_bRestrictVehicleAttractionToSelectedPoint);

	pBank->PushGroup("Vehicle Scenarios", false);
	CScenarioManager::GetVehicleScenarioManager().AddWidgets(*pBank);
	pBank->PopGroup();

	pBank->PushGroup("Chain Tests", false);
	CScenarioManager::GetChainTests().AddWidgets(*pBank);
	pBank->PopGroup();

	pBank->PushGroup("Clusters", false);
	SCENARIOCLUSTERING.AddStaticWidgets(*pBank);
	pBank->PopGroup();

	pBank->PopGroup();	// "Debug"
}

void CScenarioDebug::CreateModelSetsBank()
{
	aiAssertf(ms_pBank, "Scenario bank needs to be created first");
	aiAssertf(ms_pCreateModelSetsButton, "Scenario bank needs to be created first");

	ms_pCreateModelSetsButton->Destroy();
	ms_pCreateModelSetsButton = NULL;
	bkBank* pBank = ms_pBank;

	CAmbientModelSetManager::GetInstance().AddWidgets(*pBank);
}

void CScenarioDebug::CreateOtherBank()
{
	aiAssertf(ms_pBank, "Scenario bank needs to be created first");
	aiAssertf(ms_pCreateOtherButton, "Scenario bank needs to be created first");

	ms_pCreateOtherButton->Destroy();
	ms_pCreateOtherButton = NULL;
	bkBank* pBank = ms_pBank;

	SCENARIOPOINTMGR.AddWidgets(pBank);

	pBank->PushGroup("Ambient clips", false);
	pBank->AddButton("Give focus ped random prop", GiveRandomProp, "Gives the focus ped a random prop");
	pBank->AddButton("Reload Ambient.dat", ReloadAmbientData);
	pBank->AddToggle("Block ambient clips", &CTaskAmbientClips::BLOCK_ALL_AMBIENT_IDLES);	

	// Build an array of the names of conditional animations, for the combo box,
	// with a parallel array to keep track of the entries the names refer to.
	ms_ConditionalAnims.Reset();
	ms_ConditionalAnimNames.Reset();
	const int numGroups = CONDITIONALANIMSMGR.GetNumConditionalAnimsGroups();
	for(int i = 0; i < numGroups; i++)
	{
		const CConditionalAnimsGroup& grp = CONDITIONALANIMSMGR.GetConditionalAnimsGroupByIndex(i);
		const int numAnimsInGroup = grp.GetNumAnims();
		for(int j = 0; j < numAnimsInGroup; j++)
		{
			const CConditionalAnims* pAnims = grp.GetAnims(j);
			if(pAnims)
			{
				ConditionalAnimData& d = ms_ConditionalAnims.Grow();
				d.m_Group = &grp;
				d.m_AnimIndexWithinGroup = j;
				ms_ConditionalAnimNames.Grow() = pAnims->GetName();
			}
		}
	}

	if(ms_ConditionalAnimNames.GetCount() > 0)
	{
		pBank->AddCombo("Ambient Contexts", &ms_iAmbientContextComboIndex, ms_ConditionalAnimNames.GetCount(), &ms_ConditionalAnimNames[0]);
	}
	pBank->AddToggle("Play Base Clip", &ms_bPlayBaseClip);
	pBank->AddToggle("Instantly Blend Base Clip", &ms_bInstantlyBlendInBaseClip);
	pBank->AddToggle("Play Idle Clips", &ms_bPlayIdleClips);
	pBank->AddToggle("Pick Permanent Prop", &ms_bPickPermanentProp);
	pBank->AddToggle("Force Prop Creation", &ms_bForcePropCreation);
	pBank->AddToggle("Override Initial Idle Time", &ms_bOverrideInitialIdleTime);
	pBank->AddButton("Give Focus Ped Ambient Task", GivePedAmbientTaskCB);
	pBank->PopGroup();

	pBank->PushGroup("Ambient Scenario Peds", false);
	pBank->AddToggle("Toggle render routes", &CPatrolRoutes::ms_bRenderRoutes );
	pBank->AddToggle("Toggle render 2D Effects", &CPatrolRoutes::ms_bRender2DEffects);
	//	pBank->AddSlider("Max dist difference before trying to using nav mesh path", &CTaskControlScenario::ms_fDistToleranceBeforeTryingNavMesh,	1.0f, 10.0f, 1.0f);
	//	pBank->AddSlider("Max time to wait before checking dist difference", &CTaskControlScenario::ms_fTimeToleranceBeforeTryingNavMesh, 2.0f, 10.0f, 1.0f);
	pBank->AddSlider("Sq Dist Spawn Ped Away From Another ped", &CPedPopulation::ms_fCloseSpawnPointDistControlSq, 100.0f, 1000.0f, 10.0f);
	pBank->AddSlider("Sq Random offset Dist Spawn Ped Away From Another ped", &CPedPopulation::ms_fCloseSpawnPointDistControlOffsetSq, 100.0f, 1000.0f, 10.0f);
	pBank->PopGroup();
}

void CScenarioDebug::CreateScenarioInfoBank()
{
	aiAssertf(ms_pBank, "Scenario bank needs to be created first");
	aiAssertf(ms_pCreateScenarioInfoButton, "Scenario bank needs to be created first");

	ms_pCreateScenarioInfoButton->Destroy();
	ms_pCreateScenarioInfoButton = NULL;
	bkBank* pBank = ms_pBank;

	SCENARIOINFOMGR.AddWidgets(*pBank);
}

void CScenarioDebug::ShutdownBank()
{
	if (ms_pedSortArray)
	{
		delete[] ms_pedSortArray;
		ms_pedSortArray = NULL;
	}

	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
	ms_pCreateEditorButton = NULL;
	ms_pCreateDebugButton = NULL;
	ms_pCreateOtherButton = NULL;
	ms_pCreateConditionalAnimsButton = NULL;
	ms_pCreateModelSetsButton = NULL;
	ms_pCreateScenarioInfoButton = NULL;
	ms_pCreateScenarioReactsButton = NULL;
	ms_pTriggerReactsButton = NULL;
	ms_pTriggerAllReactsButton = NULL;
	ms_pSpawnScenarioButton = NULL;
	ms_pCleanUpScenarioButton = NULL;
	ms_pSpawnPropButton = NULL;
}

void CScenarioDebug::DebugUpdate()
{
	if (ms_bTriggerAllReacts)
	{
		if (ms_bRetryTriggerReaction)
		{
			TriggerReacts();
			ms_bRetryTriggerReaction = false;
		}
		ms_fTimeBetweenReactsCountdown -= fwTimer::GetTimeStep();
		if (ms_fTimeBetweenReactsCountdown < 0.0f)
		{
			ms_fTimeBetweenReactsCountdown += ms_fTimeBetweenReactions;
			SpawnScenario();
			TriggerReacts();
			++ms_iReactionTypeIndex;

			if (ms_iReactionTypeIndex == ms_szReactionTypeNames.size())
			{
				ms_iReactionTypeIndex = 0;
				ms_bTriggerAllReacts = false;
				ms_fTimeBetweenReactsCountdown = ms_fTimeBetweenReactions;
			}
		}
	}
}

void CScenarioDebug::DebugRender()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG_CTRL) && !CDebug::IsDebugRecorderDisplayEnabled())
	{
		//Toggle scenario point display ... 
		ms_bRenderScenarioPoints = !ms_bRenderScenarioPoints;
	}

	CScenarioManager::GetVehicleScenarioManager().DebugRender();

	CScenarioManager::GetChainTests().DebugDraw();

	if (ms_bRenderClusterInfo)
	{
		SCENARIOCLUSTERING.DebugRender();
	}

	if (ms_bDisplayScenarioModelSetsOnVectorMap)
	{
		RenderModelSetsOnVectorMap();
	}

#if !__FINAL
	if(ms_bRenderScenarioSpawnCheckCache)
	{
		CScenarioManager::GetSpawnCheckCache().DebugDraw();
	}
#endif	// !__FINAL

#if __DEV
	if (ms_bRenderNonRegionSpatialArray)
	{
		const CSpatialArray* sparray = SCENARIOPOINTMGR.GetNonRegionSpatialArray();
		if (sparray)
		{
			sparray->DebugDraw();
		}
	}
#endif

	if (ms_bRenderSummaryInfo)
	{
		SCENARIOPOINTMGR.RenderPointSummary();
	}

	bool editing = CScenarioDebug::ms_Editor.IsEditing();
	if (!ms_bRenderScenarioPointRegionBounds &&
		!ms_bRenderScenarioPoints &&
		!editing &&
		!ms_bRenderChainPointUsage &&
		!ms_bRenderSpawnHistory &&
		!ms_bRenderSpawnHistoryText &&
		!ms_bRenderClusterReport
		)
	{
		return;//not drawing anything.
	}

#if DR_ENABLED
	int mousex = ioMouse::GetX();
	int mouseY = ioMouse::GetY();

	debugPlayback::OnScreenOutput screenSpew(10.0f);
	screenSpew.m_fMouseX = (float)mousex;
	screenSpew.m_fMouseY = (float)mouseY;
	screenSpew.m_Color.Set(200,200,200,255);
	screenSpew.m_HighlightColor.Set(255,0,0,255);
	screenSpew.m_fXPosition = 50.0f;
	screenSpew.m_fYPosition = 50.0f;
#endif

	if (ms_bRenderSpawnHistory)
	{
		CScenarioManager::sm_SpawnHistory.DebugRender();
	}

	if (ms_bRenderSpawnHistoryText)
	{
#if DR_ENABLED
		CScenarioManager::sm_SpawnHistory.DebugRenderToScreen(screenSpew);
#endif
	}

	if (ms_bRenderClusterReport)
	{
#if DR_ENABLED
		SCENARIOCLUSTERING.DebugReport(screenSpew);
#endif
	}

	CPopZone* pZone = NULL;
	if(ms_bRenderScenarioPointsOnlyInPopZone)
	{
		Vector3 pPos = camInterface::GetPos();
		pZone = CPopZones::FindSmallestForPosition(&pPos, ZONECAT_ANY, ZONETYPE_ANY);
	}

#if DR_ENABLED
	if (ms_bRenderChainPointUsage)
	{
		//Draw the titles ... 
		screenSpew.AddLine("Scenario Point Chain User Info - %d/%d", CScenarioPointChainUseInfo::_ms_pPool->GetNoOfUsedSpaces(), CScenarioPointChainUseInfo::_ms_pPool->GetSize());
		char buffer[RAGE_MAX_PATH];
		formatf(buffer, RAGE_MAX_PATH, "%-32s%8s%8s%10s%24s%10s%15s%10s%6s%11s%5s", "Region", "Chain", "Node", "User Add", "Ped", "Ped Add", "Veh", "Veh Add", "Dummy", "Ref Count", "Hist");
		screenSpew.AddLine(buffer);
	}
#endif

	//Region Points
	const int rcount = SCENARIOPOINTMGR.GetNumRegions();
	for (int r = 0; r < rcount; r++)
	{
		const CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(r);
		//draw the region bounds
		if(ms_bRenderScenarioPointRegionBounds || (editing && ms_Editor.IsEditingRegion(r)))
		{
			Color32 color = Color_RoyalBlue1;
			if (region)
			{
				//Streamed in ... 
				color = Color_green4;
			}
			const spdAABB& aabb = SCENARIOPOINTMGR.GetRegionAABB(r);
			grcDebugDraw::BoxAxisAligned(aabb.GetMin(), aabb.GetMax(), color, false);

			if (ms_bRenderIntoVectorMap)
			{
				fwVectorMap::Draw3DBoundingBoxOnVMap(aabb, color);
			}
		}

		if (region)
		{
			const CScenarioPointRegionEditInterface bankRegion(*region);

			// we can now debug draw everything else.
			if (ms_bRenderScenarioPoints || editing)
			{
				//Points
				const int pcount = bankRegion.GetNumPoints();
				for (int p = 0; p < pcount; p++)
				{
					RenderPoint(bankRegion.GetPoint(p), r, editing, pZone, NULL, p);
				}

				//ClusterPoints
				const int ccount = bankRegion.GetNumClusters();
				for (int c = 0; c < ccount; c++)
				{
					const int cpcount = bankRegion.GetNumClusteredPoints(c);
					for (int cp = 0; cp < cpcount; cp++)
					{
						RenderPoint(bankRegion.GetClusteredPoint(c, cp), r, editing, pZone, NULL, cp);
					}
				}			

				// Draw the active entity override points in this region.
				// Note: this does not draw override points in regions that are not streamed in,
				// but I'm leaning towards removing those when the region gets streamed out anyway.
				const int numOverrides = region->GetNumEntityOverrides();
				for(int i = 0; i < numOverrides; i++)
				{
					const CScenarioEntityOverride& override = bankRegion.GetEntityOverride(i);
					if(override.m_EntityCurrentlyAttachedTo)
					{
						for(int j = 0; j < override.m_ScenarioPointsInUse.GetCount(); j++)
						{
							const CScenarioPoint* pt = override.m_ScenarioPointsInUse[j];
							if(pt)
							{
								RenderPoint(*pt, r, editing, pZone, &override, j);
							}
						}

						if(!override.m_ScenarioPointsInUse.GetCount())
						{
							Assert(override.m_SpecificallyPreventArtPoints);
							CEntity* pEntity = override.m_EntityCurrentlyAttachedTo;
							if(pEntity)
							{

								Color32 color(150,222,209,255);
								if(editing)
								{
									if(!ms_Editor.IsRegionSelected(r))
									{
										color.Set(100,100,100, 192);	// Not in the current region.
									}
									else if(ms_Editor.IsEntitySelected(pEntity))
									{
										color.Set(255,16,16, 192);		// Selected entity.
									}
								}

								const Vec3V posV = pEntity->GetTransform().Transform(Average(VECTOR3_TO_VEC3V(pEntity->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(pEntity->GetBoundingBoxMax())));
								char text[128];
								int numTexts = 0;

								formatf(text, "Spawning disabled");
								grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);

								formatf(text, "Region %s", GetRegionDisplayName(r));
								grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);

								formatf(text, "Attached to %s.", pEntity->GetModelName());
								grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);

								if(override.m_EntityMayNotAlwaysExist)
								{
									formatf(text, "May not always exist.");
									grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);
								}
							}
						}
					}
					else
					{

						const Vec3V camPosV = VECTOR3_TO_VEC3V(camInterface::GetPos());
						const ScalarV distSqV = DistSquared(camPosV, override.m_EntityPosition);
						if(IsLessThanAll(distSqV, ScalarV(ms_fRenderDistance*ms_fRenderDistance)))
						{
							char text[128];
							int numTexts = 0;

							Color32 color = Color_grey40;
#if SCENARIO_VERIFY_ENTITY_OVERRIDES
							if (ms_Editor.GetSelectedOverrideWithNoPropIndex() == i)
							{
								color = Color_brown;
							}
							else if(override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusBroken)
							{
								if(override.m_EntityMayNotAlwaysExist)
								{
									color = Color_orange;
								}
								else
								{
									color = Color_red;
								}
							}
							else if(override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusBrokenProp)
							{
								color = Color_orange;
							}
							else if(override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusWorking)
							{
								color = Color_DarkGreen;
							}
							else if(override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusStruggling)
							{
								color = Color_yellow;
							}
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

							const Vec3V posV = override.m_EntityPosition;

							if(override.m_EntityMayNotAlwaysExist)
							{
								formatf(text, "May not always exist.");
								grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);
							}

							formatf(text, "Override not bound to prop: %s", override.m_EntityType.GetCStr());
							grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);

							formatf(text, "Region %s", GetRegionDisplayName(r));
							grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);

							if(override.m_ScenarioPoints.GetCount())
							{
								formatf(text, "Attached scenarios:");
								grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);

								for(int i = 0; i < override.m_ScenarioPoints.GetCount(); i++)
								{
									formatf(text, "  %s", override.m_ScenarioPoints[i].m_spawnType.GetCStr());
									grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);
								}
							}
							else
							{
								if(override.m_SpecificallyPreventArtPoints)
								{
									formatf(text, "No scenarios, just for disabling spawning");
								}
								else
								{
									formatf(text, "Possibly invalid, no scenarios found");
								}
								grcDebugDraw::Text(posV, color, 0, numTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);
							}
						}
					}
				}

				//Chaining Graph
				const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();
				const int numEdges = graph.GetNumEdges();
				for(int e = 0; e < numEdges; e++)
				{
					RenderChainEdge(graph, graph.GetEdge(e), editing, pZone);
				}
			}

			//Render the Chaining stats 
			if (ms_bRenderChainPointUsage)
			{
#if DR_ENABLED
				const CScenarioChainingGraph& graph = bankRegion.GetChainingGraph();

				const char* rname = SCENARIOPOINTMGR.GetRegionName(r);
				int length = istrlen(rname);
				int noff = 0;
				for (int c = 0; c < length; c++)
				{
					if (rname[c] == '/' || rname[c] == '\\')
					{
						noff = c+1;
					}
				}
				const char* name = &rname[noff];
				graph.RenderUsage(name, screenSpew);
#endif
			}
		}
	}

#if DR_ENABLED
	if(ms_bRenderChainPointUsage)
	{
		for(int u=0; u < CScenarioPointChainUseInfo::_ms_pPool->GetSize(); u++)
		{
			CScenarioPointChainUseInfo* pUser = CScenarioPointChainUseInfo::_ms_pPool->GetSlot(u);
			if(pUser && !pUser->IsIndexInfoValid())
			{
				CScenarioChain::RenderUsageSingle("-none-", *pUser, screenSpew);
			}
		}
	}
#endif	// DR_ENABLED

	//Entity Points
	if (ms_bRenderScenarioPoints || editing)
	{
		int eCount = SCENARIOPOINTMGR.GetNumEntityPoints();
		int p;
		for (p = 0; p < eCount; ++p)
		{
			const CScenarioPoint& pt = SCENARIOPOINTMGR.GetEntityPoint(p);
			if(!pt.IsEntityOverridePoint())
			{
				RenderPoint(pt, -1, editing, pZone);
			}
		}
		int wCount = SCENARIOPOINTMGR.GetNumWorldPoints();
		for (p = 0; p < wCount; ++p) 
		{
			RenderPoint(SCENARIOPOINTMGR.GetWorldPoint(p), -1, editing, pZone);
		}
	}

	//Editor specific rendering 
	if (editing)
	{
		ms_Editor.Render();
	}
}

//Random colors to use for debug display.
u32 s_ColorArray[] = {
	0xffA57069
	,0xff441906
	,0xff5E1580
	,0xff6E3A50
	,0xffA129AB
	,0xffB50256
	,0xffCABD3B
	,0xff48A5E3
	,0xff5518F6
	,0xff998B22
	,0xff6E770D
	,0xffED79AA
	,0xff5E3F41
	,0xff296326
	,0xffABF272
	,0xff5E0D51
	,0xffA4E1E9
	,0xffC38F2C
	,0xff146788
	,0xffD34D6B
	,0xffFBA080
	,0xff124042
	,0xff905108
	,0xffE1D495
	,0xffD6814F
	,0xffD3A733
	,0xff756C6A
	,0xffFFB41C
	,0xffDFDA46
	,0xff01D921
	,0xff805278
	,0xff6FECC4
	,0xff7B4205
	,0xffED5C89
	,0xff28A01D
	,0xff19D6C5
	,0xffF25EF1
	,0xff26C69D
	,0xffC8A142
	,0xffB7B57C
	,0xff396003
	,0xff6F5CD5
	,0xffD0BA74
	,0xff74D262
	,0xff519071
	,0xffDAB4BB
	,0xff71BCD8
	,0xff1B9D60
	,0xff2E4F0A
	,0xff26CB73
	,0xffD4D998
	,0xff40AFD8
	,0xff8F32CA
	,0xff2E87E9
	,0xff200EC7
	,0xff2ACF35
	,0xff885282
	,0xffA11E3D
	,0xff96C1DB
	,0xffA0CAF9
	,0xffA11E3D
	,0xff48F9B8
	,0xffC69D5C
	,0xffBE28F0
	,0xff4A0050
	,0xff981C62
	,0xff4AA346
	,0xffC0D5E2
	,0xff290420
	,0xff56B661
	,0xff6097C0
	,0xff91B45F
	,0xffB1086E
	,0xffB88BDA
	,0xffC20886
	,0xff9F6363
	,0xff304F59
	,0xffF0AF91
	,0xffE49607
	,0xff0C9042
	,0xff7E1E18
	,0xff25874E
	,0xff13EC27
	,0xffD0C77C
	,0xffE7888D
	,0xffA0BCED
	,0xff412D86
	,0xffBA996C
	,0xff14DDB4
	,0xff9AC234
	,0xff0D5B94
	,0xff9E5BBE
	,0xff168B0E
	,0xff7125FE
	,0xff9896BC
	,0xffBB9073
	,0xff4019BD
	,0xff8D3093
	,0xffF15164
	,0xff00D968
	,0xff61301B
	,0xff36B507
	,0xff38568E
	,0xffA70F97
	,0xff3A703E
	,0xff89FC91
	,0xff7D7338
	,0xffB600D8
	,0xffF70F69
	,0xff7D7338
	,0xffC21CFB
	,0xffE796AC
	,0xffE63E60
	,0xff65775D
	,0xff0C64A0
	,0xffC73D33
	,0xff81A185
	,0xffE6B142
	,0xff1B892D
	,0xff63AD85
	,0xffE4C220
	,0xff5A20CA
	,0xff5B01D5
	,0xff3F0B83
	,0xff9F73DF
	,0xff8C35E5
	,0xff54074D
	,0xff1A91B6
};

void CScenarioDebug::RenderModelSetsOnVectorMap()
{
	//Find the points within the radius
	Vector3 pPos = camInterface::GetPos();
	CScenarioPoint* areaPoints[CScenarioPointManager::kMaxPtsInArea];
	const s32 foundCount = SCENARIOPOINTMGR.FindPointsInArea(RCC_VEC3V(pPos), ScalarV(100.0f), areaPoints, NELEM(areaPoints));

	char text[128];
 	float lines = 0;
 	const float offset = 0.015f;
	short textPrinted[20];
	for (int i = 0; i < NELEM(textPrinted); i++)
	{
		textPrinted[i] = -1;
	}
	const float spawnradius = 5.0f; //read from somewhere ... 
	int totalPedSets = CAmbientModelSetManager::GetInstance().GetNumModelSets(CAmbientModelSetManager::kPedModelSets);
	for (int i = 0; i < foundCount; i++)
	{
		const CScenarioPoint& point = *(areaPoints[i]);

		const CEntity* pEntity = point.GetEntity();
		if((pEntity && (!pEntity->m_nFlags.bWillSpawnPeds && !point.IsEntityOverridePoint()))
				|| point.IsFlagSet(CScenarioPointFlags::NoSpawn))
		{
			continue;
		}
		if(!CScenarioManager::CheckScenarioPointMapData(point))
		{
			continue;
		}

		const int virtualType = point.GetScenarioTypeVirtualOrReal();
		const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(virtualType);

		for(int j = 0; j < numRealTypes; j++)
		{
			if(!CScenarioManager::CheckScenarioPointEnabled(point, j))
			{
				continue;
			}
			if(!CScenarioManager::CheckScenarioPointTimesAndProbability(point, false, false, j))
			{
				continue;
			}

			const int realType = SCENARIOINFOMGR.GetRealScenarioType(virtualType, j);
			const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(realType);
			if(pScenarioInfo && pScenarioInfo->GetSpawnProbability() <= 0.0f)
			{
				continue;
			}

			//Modelset
			bool drawSet = false;
			CAmbientModelSetManager::ModelSetType setType = CAmbientModelSetManager::kPedModelSets;
			if(CScenarioManager::IsVehicleScenarioType(point.GetScenarioTypeVirtualOrReal()))
			{
				setType = CAmbientModelSetManager::kVehicleModelSets;
			}
			unsigned modelSetId = point.GetModelSetIndex();
			if(modelSetId != CScenarioPointManager::kNoCustomModelSet)
			{
				drawSet = true;
			}
			else
			{
				if (pScenarioInfo && pScenarioInfo->GetModelSet() && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::ForceModel))
				{
					u32 modelSetHash = pScenarioInfo->GetModelSet()->GetHash();
					modelSetHash = CScenarioManager::LegacyDefaultModelSetConvert(modelSetHash);
					if(modelSetHash !=0 && modelSetHash!=CScenarioManager::MODEL_SET_USE_POPULATION)
					{
						// Even if this is a vehicle scenario, CScenarioInfo::GetModelSet() is still a ped model set.
						setType = CAmbientModelSetManager::kPedModelSets;

						int index = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(setType, modelSetHash);
						if (aiVerifyf(index != -1, "Unknown %s modelset hash %d 0x%x", (setType == CAmbientModelSetManager::kPedModelSets) ? "Ped" : "Vehicle", modelSetHash, modelSetHash))
						{
							Assign(modelSetId, (unsigned)index);
							drawSet = true;
						}
					}
				}
			}

			if(drawSet)
			{
				//figure out color.
				const int colorCnt = NELEM(s_ColorArray);
				const int colorOff = (setType == CAmbientModelSetManager::kVehicleModelSets) ? totalPedSets : 0;
				const int colorIdx = (modelSetId + colorOff) % colorCnt;
				const Color32 color = Color32(s_ColorArray[colorIdx]);

				//Draw
				fwVectorMap::DrawCircle(VEC3V_TO_VECTOR3(point.GetPosition()), spawnradius, color, false);

				//figure out if we drew text before
				bool drawText = true;
				int cur = 0;
				const int maxPrint = NELEM(textPrinted);
				while(cur < maxPrint && textPrinted[cur] != -1)
				{
					if (textPrinted[cur] == (int)(modelSetId + colorOff))
					{
						drawText = false;
						break;
					}
					cur++;
				}

				//draw it if we can.
				if (drawText && aiVerifyf(cur < maxPrint, "CScenarioDebug::RenderModelSetsOnVectorMap Not enough elements in printed text history please add more."))
				{
					formatf(text, "%s", CAmbientModelSetManager::GetInstance().GetModelSet(setType, modelSetId).GetName());
					grcDebugDraw::Text(Vec2V(0.75f, 0.12f+(lines*offset)), color, text, false, 1.0f, 1.0f);
					lines += 1.0f;
					Assign(textPrinted[cur], (int)(modelSetId + colorOff));
				}
			}
		}
	}
}

void CScenarioDebug::RenderPoint(const CScenarioPoint& point, int regionIndex, bool editorActive, CPopZone* pZone, const CScenarioEntityOverride* pEntityOverride, int index)
{
	if (CanRenderPoint(point, pZone) && !ms_bAnimDebugDisableSPRender)
	{
		Color32 color = Color_grey;
		Vec3V vPos = point.GetPosition();

		//////////////////////////////////////////////////////////////////////////
		// Text display
		char text[128];
		point.FindDebugColourAndText(&color, text, sizeof(text));
		if (!point.IsEditable() && point.IsFromRsc())
		{
			// Override the color for none editable points
			color = Color_blue;
		}
		if (ms_bRenderPointText)
		{
			s32 iNumTexts = 0;
			grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);

			if (point.HasTimeOverride())
			{	
				formatf( text, "Time (Overridden: %i -> %i)", point.GetTimeStartOverride(), point.GetTimeEndOverride() );
				Color32 col = Color_red;
				if (CClock::IsTimeInRange(point.GetTimeStartOverride(), point.GetTimeEndOverride()))
				{
					col = Color_green;
				}
				grcDebugDraw::Text(vPos, col, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			}
			else
			{
				formatf( text, "Time (Default for scenario)");
				grcDebugDraw::Text(vPos, Color_blue, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			}

			if(point.IsAvailability(CScenarioPoint::kOnlySp))
			{
				Color32 col = CNetwork::IsGameInProgress() ? Color_red : Color_green;
				grcDebugDraw::Text(vPos, col, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Only in SP");
			}
			else if(point.IsAvailability(CScenarioPoint::kOnlyMp))
			{
				Color32 col = CNetwork::IsGameInProgress() ? Color_green : Color_red;
				grcDebugDraw::Text(vPos, col, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Only in MP");
			}

			if(point.HasTimeTillPedLeaves())
			{
				float time = point.GetTimeTillPedLeaves();
				if(time < 0.0f)
				{
					formatf(text, "Never leave.");
				}
				else
				{
					formatf(text, "Leave after %.0f s", time);
				}
				grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			}

			if(point.HasProbabilityOverride())
			{
				if(point.GetProbabilityOverrideAsIntPercentage() < 100)
				{
					Color32 col = CScenarioManager::CheckScenarioProbability(&point, -1 /* shouldn't be used */) ? Color_green : Color_red;
					formatf(text, "Probability %.0f %% (from point).", 100.0f*point.GetProbabilityOverride());
					grcDebugDraw::Text(vPos, col, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
				}
			}
			else
			{
				const int scenarioType = point.GetScenarioTypeVirtualOrReal();
				if(!SCENARIOINFOMGR.IsVirtualIndex(scenarioType))
				{
					CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);
					if(pInfo && pInfo->GetSpawnProbability() < 0.999f && pInfo->GetSpawnProbability() > 0.001f)
					{
						Color32 col = CScenarioManager::CheckScenarioProbability(&point, scenarioType) ? Color_green : Color_red;
						formatf(text, "Probability %.0f %% (from type).", 100.0f*pInfo->GetSpawnProbability());
						grcDebugDraw::Text(vPos, col, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
					}
				}
			}

			if (point.GetScenarioGroup() != CScenarioPointManager::kNoGroup)
			{
				const CScenarioPointGroup& grp = SCENARIOPOINTMGR.GetGroupByIndex(point.GetScenarioGroup()-1);
				formatf(text, "Group %s", grp.GetName());
				grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			}

			//////////////////////////////////////////////////////////////////////////
			{
				//model set info ... 
				CAmbientModelSetManager::ModelSetType setType = CAmbientModelSetManager::kPedModelSets;
				const char* type = "Ped";
				if(CScenarioManager::IsVehicleScenarioType(point.GetScenarioTypeVirtualOrReal()))
				{
					setType = CAmbientModelSetManager::kVehicleModelSets;
					type = "Vehicle";
				}

				if (point.GetModelSetIndex() == CScenarioPointManager::kNoCustomModelSet)
				{
					formatf(text, "%s ModelSet UsePopulation", type);
				}
				else
				{
					formatf(text, "%s ModelSet %s", type, CAmbientModelSetManager::GetInstance().GetModelSet(setType, point.GetModelSetIndex()).GetName());
				}
				grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			}

			float fRadius = point.GetRadius();
			if (fRadius > 0.0f)
			{
				formatf(text, "Radius:  %2f", fRadius);
				grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			}

			if (point.IsFlagSet(CScenarioPointFlags::StationaryReactions))
			{
				formatf(text, "Stationary reactions enabled.");
				grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			}
			

			if (regionIndex != -1)
			{
				// Since peds can now only start using chains at a node with no incoming links, it's handy
				// to have some visual indication of which ones those are.
				if(point.IsChained())
				{
					const CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(regionIndex);
					Assert(region);//any point with a regionIndex must have a streamed in region.

					if(!region->IsChainedWithIncomingEdges(point))
					{
						grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Chain entry point");
					}
				}

				//Display the name of the region this point is in
				const char* name = GetRegionDisplayName(regionIndex);

				char interiorInfo[256];
				interiorInfo[0] = '\0';
				const unsigned int interiorId = point.GetInterior();
				if(interiorId != CScenarioPointManager::kNoInterior)
				{
					const char* interiorName = SCENARIOPOINTMGR.GetInteriorName(interiorId - 1);
					formatf(interiorInfo, " (in %s)", interiorName);
				}

				formatf(text, "Region %s%s", name, interiorInfo);
				grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);

				formatf(text, "Source %s", GetRegionSource(regionIndex));
				grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);

				if(point.GetRequiredIMap() != CScenarioPointManager::kNoRequiredIMap)
				{
					u32 rimHash = SCENARIOPOINTMGR.GetRequiredIMapHash(point.GetRequiredIMap());
					strLocalIndex rimSlot = strLocalIndex(SCENARIOPOINTMGR.GetRequiredIMapSlotId(point.GetRequiredIMap()));
					const char* status = "not found";
					bool loaded = false;
					if(rimSlot.Get() != CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound)
					{
						fwMapDataDef* def = fwMapDataStore::GetStore().GetSlot(rimSlot);
						if(def)
						{
							if(def->IsLoaded())
							{
								status = "loaded";
								loaded = true;
							}
							else
							{
								status = "not loaded";
							}
						}
					}

					Color32 col = loaded ? Color_green : Color_red;

					atHashString str(rimHash);
					formatf(text, "Requires IMAP %s (%s)", str.GetCStr(), status);
					grcDebugDraw::Text(vPos, col, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
				}

				if(point.IsEntityOverridePoint())
				{
					CEntity* pEntity = point.GetEntity();
					if(Verifyf(pEntity, "Expected entity if m_bEntityOverridePoint is set."))
					{
						formatf(text, "Attached to %s", pEntity->GetModelName());
						grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);

						if(pEntityOverride && pEntityOverride->m_EntityMayNotAlwaysExist)
						{
							formatf(text, "Prop may not always exist.");
							grcDebugDraw::Text(vPos, color, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text, false);
						}
					}
				}
			}

			// obstructed text
			Color32 col = point.IsObstructed() ? Color_red : Color_green;
			formatf(text, "Obstructed: %s PointIndex %d", point.IsObstructed() ? "true" : "false", index);
			grcDebugDraw::Text(vPos, col, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), text);
			if (point.IsObstructed())
			{
				grcDebugDraw::Sphere(vPos, 0.5f, col, true);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		//Point
		const Vec3V vPos2 = vPos + Vec3V(0.0f,0.0f,ms_fPointDrawRadius*2.0f);
		const Vec3V vHandleOffset = point.GetDirection() * ScalarV(ms_fPointHandleLength);

		bool selected = false;
		if (editorActive && ms_Editor.IsPointSelected(point))
		{
			color.Set(255,16,16, 192);	// selected point is red
			selected = true;
		}
		else if (editorActive && regionIndex != -1 && !ms_Editor.IsRegionSelected(regionIndex))
		{
			color.Set(100,100,100, 192);	// non selected region points are grey
		}

		if(point.IsEntityOverridePoint())
		{
			CEntity* pEntity = point.GetEntity();
			if(pEntity)
			{
				Color32 colorDimmed = color;
				colorDimmed.SetAlpha(colorDimmed.GetAlpha()/2);

				const Vec3V entityPosV = pEntity->GetTransform().Transform(Average(VECTOR3_TO_VEC3V(pEntity->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(pEntity->GetBoundingBoxMax())));
				grcDebugDraw::Line(point.GetPosition(), entityPosV, colorDimmed);
			}
		}

		if (point.IsFromRsc())
		{
			grcDebugDraw::Cone(vPos, vPos2, ms_fPointDrawRadius, color, true);
		}
		else
		{
			grcDebugDraw::Sphere(vPos, ms_fPointDrawRadius, color);
		}
		grcDebugDraw::Line(vPos, vPos + vHandleOffset, color);

		// Render associated area of this point.
		if (ms_bRenderPointRadius)
		{
			float fRadius = point.GetRadius();
			if (fRadius > 0.0f)
			{
				grcDebugDraw::Circle(vPos, fRadius, color, Vec3V(V_X_AXIS_WZERO), Vec3V(V_Y_AXIS_WZERO));
			}
		}
		
		if (ms_bRenderIntoVectorMap)
		{
			fwVectorMap::DrawCircle(VEC3V_TO_VECTOR3(vPos), ms_fPointDrawRadius, color, false);
			
// 			if (!point.IsFlagSet(CScenarioPointFlags::NoSpawn))
// 			{
// 				Color32 color = Color_yellow4; 
// 				if (point.IsInCluster())
// 				{
// 					color = Color_orange;
// 					if (point.GetUses())
// 					{
// 						color = Color_blue4;
// 					}
// 				}
// 				else if (point.GetUses())
// 				{
// 					color = Color_green2;
// 				}
// 
// 				color.SetAlpha(128);
// 				static float sphereSize = 50.0f;
// 				fwVectorMap::DrawCircle(VEC3V_TO_VECTOR3(vPos), sphereSize, color);
// 			}
		}

		if (editorActive && selected && point.IsEditable())
		{
			if (ms_Editor.IsPointHandleHovered(point))
			{
				color.Set(255,255,16, 192);	// hovered point handle is yellow
			}
			//draw the handle for changing the direction
			grcDebugDraw::Sphere(vPos + vHandleOffset, ms_fPointHandleRadius, color, true);
		}
	}
}

bool CScenarioDebug::IsTimeInRange(u32 currHour, u32 startHour, u32 endHour)
{
	if (startHour <= endHour)
	{
		if (currHour>=startHour && currHour<endHour)
		{
			return true;
		}
		else if (startHour == endHour && currHour == startHour)
		{
			return true;
		}
	}
	else if (currHour>=startHour || currHour<endHour)
	{
			return true;
	}

	return false;
}

bool CScenarioDebug::CanRenderPoint(const CScenarioPoint& point, CPopZone* pZone)
{
	if (ms_bRenderOnlyChainedPoints && !point.IsChained())
	{
		return false;
	}

	if (ms_bRenderStationaryReactionsPointsOnly && !point.IsFlagSet(CScenarioPointFlags::StationaryReactions))
	{
		return false;
	}

	if (!CanRenderPointType(point.GetScenarioTypeVirtualOrReal()))
	{
		return false;
	}

	// Check if all of the real scenario types are disabled.
	const int numReal = SCENARIOINFOMGR.GetNumRealScenarioTypes(point.GetScenarioTypeVirtualOrReal());
	bool enabled = false;
	for(int i = 0; i < numReal; i++)
	{
		if(CScenarioManager::CheckScenarioPointEnabled(point, i))
		{
			enabled = true;
			break;
		}
	}
	const bool disabled = !enabled;
	if(disabled && !ms_bRenderScenarioPointsDisabled)
	{
		return false;
	}
	
	const Vec3V v3Pos = point.GetPosition();

	//Radius
	const Vec3V vCameraPosition = VECTOR3_TO_VEC3V( camInterface::GetPos() );
	const float distance2 = DistSquared( vCameraPosition, v3Pos).Getf();
	if (distance2 > ms_fRenderDistance*ms_fRenderDistance)
	{
		return false;
	}

	//Frustum -- using the handle length because that is the longest part of the point
	if (!camInterface::IsSphereVisibleInGameViewport(VEC3V_TO_VECTOR3(v3Pos), ms_fPointHandleLength))
	{
		return false;
	}

	if (ms_bRenderSelectedScenarioGroup && (point.GetScenarioGroup() != (u32)ms_iScenarioGroupIndex))
	{
		return false;
	}

	//PopZone
	Vector3 vPos = VEC3V_TO_VECTOR3(v3Pos);
	bool passPopZone = (!pZone || (pZone && CPopZones::DoesPointLiesWithinPopZone( &vPos, pZone ) ));

	//Time period
	bool passTimePeriod = true;
	if (ms_bRenderScenarioPointsOnlyInTimePeriod)
	{
		// if the point has no time restriction then it is always valid.
		if (point.HasTimeOverride())
		{
			passTimePeriod = false;

			// If the current time is within the restricted time
			if (ms_bRenderScenarioPointsOnlyInTimePeriodCurrent)
			{
				if (IsTimeInRange(CClock::GetHour(), (u32)point.GetTimeStartOverride(), (u32)point.GetTimeEndOverride()))
					passTimePeriod = true;
			}
			else
			{
				for (int hour = 0; hour < 24; hour++)
				{
					//If the time is in the points time
					if (IsTimeInRange(hour, (u32)point.GetTimeStartOverride(), (u32)point.GetTimeEndOverride()))
					{
						//If the time is in the restricted time ... 
						if (IsTimeInRange(hour, ms_iRenderScenarioPointsOnlyInTimePeriodStart, ms_iRenderScenarioPointsOnlyInTimePeriodEnd))
						{
							passTimePeriod = true;
							break;
						}
					}
				}
			}
		}
	}

    bool passAvaliability = true;
	if	( 
			ms_iAvailabilityComboIndex != CScenarioPoint::kBoth &&
			ms_iAvailabilityComboIndex != point.GetAvailability()
		)
	{
		passAvaliability = false;
	}

	return (passPopZone && passTimePeriod && passAvaliability);
}

bool CScenarioDebug::CanRenderPointType(unsigned pointTypeVirtualOrReal)
{
	// Check if the tickbox is enabled.
	if (!ms_bRenderSelectedTypeOnly)
	{
		return true;
	}

	// Check if the point type is the same as the currently selected type in the editor.
	if (CScenarioDebug::ms_Editor.GetSelectedSpawnTypeIndex() == (int)pointTypeVirtualOrReal)
	{
		return true;
	}

	return false;
}

void CScenarioDebug::RenderChainEdge(const CScenarioChainingGraph& graph, const CScenarioChainingEdge& edge, bool editorActive, CPopZone* pZone)
{
	const CScenarioPoint* pointFrom = graph.GetChainingNodeScenarioPoint(edge.GetNodeIndexFrom());
	const CScenarioPoint* pointTo = graph.GetChainingNodeScenarioPoint(edge.GetNodeIndexTo());
	if (pointFrom && pointTo) //both valid
	{
		if (!CanRenderPoint(*pointFrom, pZone) && !CanRenderPoint(*pointTo, pZone))
		{
			//Find the closest point on the edge to the camera
			Vec3V vCameraPos	= VECTOR3_TO_VEC3V(camInterface::GetPos());
			Vec3V vClosestPoint = geomPoints::FindClosestPointSegToPoint(pointFrom->GetPosition(), pointTo->GetPosition() - pointFrom->GetPosition(), vCameraPos);
			const float distance2 = DistSquared(vCameraPos, vClosestPoint).Getf();
			if (distance2 > ms_fRenderDistance*ms_fRenderDistance)
			{
				return;
			}
		}
	}

	//if one of the points is null then draw anyway 
	const CScenarioChainingNode& nodeFrom = graph.GetNode(edge.GetNodeIndexFrom());
	const CScenarioChainingNode& nodeTo = graph.GetNode(edge.GetNodeIndexTo());

	bool incomplete = false;
	Vec3V pos1V;
	if(pointFrom)
	{
		pos1V = pointFrom->GetPosition();
	}
	else
	{
		pos1V = nodeFrom.m_Position;
		incomplete = true;
	}

	Vec3V pos2V;
	if(pointTo)
	{
		pos2V = pointTo->GetPosition();
	}
	else
	{
		pos2V = nodeTo.m_Position;
		incomplete = true;
	}

	Color32 color = CScenarioChainingEdge::GetNavModeColor(edge.GetNavMode());
	if(incomplete)
	{
		color.Set(128, 128, 128, 255);
	}

	if (editorActive)
	{
		//check to see if the point is selected/hovered on/other?
		if (ms_Editor.IsEdgeSelected(edge))
		{
			color.Set(255,16,16, 192);	// selected edge is red
		}
// 		else if (ms_Editor.IsEdgeHovered(edge))
// 		{
// 			color.Set(255,255,16, 192);	// hovered edge is yellow
// 		}
	}

	grcDebugDraw::Arrow(pos1V, pos2V, 0.3f, color);

	const CScenarioChainingEdge::eAction action = edge.GetAction();
	if(action != CScenarioChainingEdge::kActionDefault && action < CScenarioChainingEdge::kNumActions)
	{
		grcDebugDraw::Text(Average(pos1V, pos2V), color, CScenarioChainingEdge::sm_ActionNames[(int)action], false);
	}

	if (ms_bRenderIntoVectorMap)
	{
		fwVectorMap::DrawLine(VEC3V_TO_VECTOR3(pos1V), VEC3V_TO_VECTOR3(pos2V), color, color);
	}
}


const char* CScenarioDebug::GetRegionDisplayName(int regionIndex)
{
	const char* name = SCENARIOPOINTMGR.GetRegionName(regionIndex);
	int length = istrlen(name);
	int offset = 0;
	for (int c = 0; c < length; c++)
	{
		if (name[c] == '/' || name[c] == '\\')
		{
			offset = c+1;
		}
	}
	return name + offset;
}

const char* CScenarioDebug::GetRegionSource(int regionIndex)
{
	const char* name = SCENARIOPOINTMGR.GetRegionSource(regionIndex);
	return name;
}

CPed* CScenarioDebug::GetFocusPed()
{
	CEntity *pEnt = CGameWorld::FindLocalPlayer();
	if(!pEnt || !pEnt->GetIsTypePed())
	{
		return NULL;
	}
	return static_cast<CPed*>(pEnt);
}


void CScenarioDebug::WarpToNextNearestScenarioCB()
{
	CPed* pFocusPed = GetFocusPed();
	if(!pFocusPed)
	{
		return;
	}
	const Vec3V playerPosV = pFocusPed->GetTransform().GetPosition();

	// Clear the memory if the type changed.
	const int scenarioType = ms_iScenarioComboIndex;
	if(scenarioType != ms_WarpToNearestScenarioType)
	{
		ms_WarpToNearestScenarioType = scenarioType;
		ms_WarpToNearestAlreadyVisited.Reset();
	}

	// Prepare to loop until success, which normally would just take one iteration.
	int found = 0;
	int iter = 0;
	while(1)
	{
		iter++;
		if(iter > ms_WarpToNearestAlreadyVisited.GetSize())
		{
			// This is mostly a safety mechanism that's not expected to really happen, to make sure
			// we can't end up in an infinite loop. Probably possible to hit this now if you put more scenario
			// points in one single place than we have entries for in ms_WarpToNearestAlreadyVisited.
			break;
		}

		// Look for a scenario of the right type, which we haven't warped to recently.
		CScenarioDebugClosestFilterNotVisited filterVisited(scenarioType);
		CScenarioPoint* pFoundPoint = GetClosestScenarioPoint(playerPosV, filterVisited);

		if(pFoundPoint)
		{
			// If the memory is full, drop the oldest entry.
			if(ms_WarpToNearestAlreadyVisited.IsFull())
			{
				ms_WarpToNearestAlreadyVisited.Drop();
			}

			// Remember that we have warped here.
			ScenarioPointMemory m;
			m.m_pScenarioPoint = pFoundPoint;
			ms_WarpToNearestAlreadyVisited.Push(m);

			// Try to warp to the new point.
			if(WarpTo(*pFocusPed, *pFoundPoint))
			{
				return;
			}

			// In this case, we may already be in the right spot. Since we've added this point to the memory
			// now, just by doing another iteration we have a good chance of finding a different point.
			found++;
		}
		else
		{
			// If we couldn't find any scenario which hasn't already been visited, we drop the oldest
			// entry in the memory and try again, or break if the memory is empty.
			if(ms_WarpToNearestAlreadyVisited.IsEmpty())
			{
				break;
			}
			ms_WarpToNearestAlreadyVisited.Drop();
		}
	}

	// Whenever we didn't teleport successfully, give some written feedback to the user.
	if(!found)
	{
		Displayf("No scenario points of type %s found.", ms_szScenarioNames[ms_iScenarioComboIndex]);
	}
	else
	{
		Displayf("No other scenario points of type %s found.", ms_szScenarioNames[ms_iScenarioComboIndex]);
	}
}


void CScenarioDebug::WarpToPreviousScenarioCB()
{
	CPed* pFocusPed = GetFocusPed();
	if(!pFocusPed)
	{
		return;
	}
	const Vec3V playerPosV = pFocusPed->GetTransform().GetPosition();

	// Clear the memory if the type changed.
	const int scenarioType = ms_iScenarioComboIndex;
	if(scenarioType != ms_WarpToNearestScenarioType)
	{
		ms_WarpToNearestScenarioType = scenarioType;
		ms_WarpToNearestAlreadyVisited.Reset();
	}

	// Start looking back in the memory of previously visited scenarios.
	while(!ms_WarpToNearestAlreadyVisited.IsEmpty())
	{
		// atQueue is normally FIFO, so we pop from the "end" to get the most recently
		// visited one.
		const ScenarioPointMemory& m = ms_WarpToNearestAlreadyVisited.PopEnd();

		// While we already have the pointers we need, we can't be sure that the scenario
		// and entity are still valid. By using GetClosestScenarioPoint(), we know that
		// what we are about to access will be valid (in the unlikely very worst case,
		// we could possibly find a different object if the same locations in memory got reused
		// for other objects).
		if (!m.m_pScenarioPoint)
			continue;

		CScenarioDebugClosestFilterSpecific filter(scenarioType, m.m_pScenarioPoint);
		CScenarioPoint* pFoundPoint = GetClosestScenarioPoint(playerPosV, filter);

		// Try to warp if this point existed.
		if(pFoundPoint)
		{
			if(WarpTo(*pFocusPed, *pFoundPoint))
			{
				// Success!
				return;
			}
		}
	}

	// Queue empty, make sure to give some feedback.
	Displayf("No previous scenario of type %s is stored.", ms_szScenarioNames[ms_iScenarioComboIndex]);
}


bool CScenarioDebug::WarpTo(CPed& focusPed, const CScenarioPoint& scenarioPt)
{
	Vector3 pointPos = VEC3V_TO_VECTOR3(scenarioPt.GetPosition());
	// Add on a little bit to spawn in front of the point
	Vector3 vDir = VEC3V_TO_VECTOR3(scenarioPt.GetDirection()) * 2.0f;	// 2 meters away
	pointPos += vDir;
	// Need to get this to ground level
	if(!CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE,&pointPos, NULL, NULL, NULL, 1.0f, 99.5f, ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE, NULL, false))
	{
		// If this fails, just assume the point is on ground level and add the Root offset
		pointPos.z += PED_HUMAN_GROUNDTOROOTOFFSET;
	}

	Vec3V pointPosV = VECTOR3_TO_VEC3V(pointPos);
	Vec3V focusPedPosV = focusPed.GetTransform().GetPosition();
	ScalarV distThresholdSqV(0.2f);
	ScalarV distSqV = DistSquared(pointPosV, focusPedPosV);
	if(IsLessThanAll(distSqV, distThresholdSqV))
	{
		return false;
	}

	focusPed.SetPosition(pointPos);
	return true;
}


void CScenarioDebug::CreateAndActivateScenarioOnSelectedPedCB()
{
	// Get the ped to use, and its position.
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if(!pEnt)
	{
		pEnt = CGameWorld::FindLocalPlayer();
	}
	if(!pEnt || !pEnt->GetIsTypePed())
	{
		return;
	}
	CPed* pFocusPed = static_cast<CPed*>(pEnt);
	const Vec3V pedPosV = pFocusPed->GetTransform().GetPosition();

	CScenarioPoint* pPropScenarioPoint = NULL;

	// Get the scenario type. This is the same as the index in the combobox now, but that wouldn't
	// necessarily be the case if we wanted to sort the entries or something.
	const int scenarioTypeToCreate = ms_iScenarioComboIndex;

	CVehicle* pVehicleForScenario = NULL;
	bool usesPoint = true;

	bool needVehicle = false;
	eVehicleScenarioType vehScenarioType = VSCENARIO_TYPE_UNKNOWN;
	const CScenarioInfo* pScenarioInfo = NULL;
	if(!SCENARIOINFOMGR.IsVirtualIndex(scenarioTypeToCreate))
	{
		pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioTypeToCreate);
		if(pScenarioInfo->GetIsClass<CScenarioParkedVehicleInfo>())
		{
			needVehicle = true;
			usesPoint = false;
			vehScenarioType = static_cast<const CScenarioParkedVehicleInfo*>(pScenarioInfo)->GetVehicleScenarioType();
			Displayf("Looking for a nearby vehicle for scenario %s:", pScenarioInfo->GetName());
		}
	}

	if(needVehicle)
	{
		bool listedAny = false;
		ScalarV closestDistSqV(square(30.0f));
		CEntityScannerIterator nearbyVehicles = pFocusPed->GetPedIntelligence()->GetNearbyVehicles();
		for( CEntity* pEnt = nearbyVehicles.GetFirst(); pEnt; pEnt = nearbyVehicles.GetNext())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
			if(!pVehicle)
			{
				continue;
			}

			listedAny = true;

			const Vec3V vehiclePosV = pVehicle->GetTransform().GetPosition();
			Printf("  %s at (%.0f, %.0f, %.0f): ", pVehicle->GetVehicleModelInfo()->GetModelName(),
					vehiclePosV.GetXf(), vehiclePosV.GetYf(), vehiclePosV.GetZf());

			if(pVehicle->GetVelocity().Mag2() >= square(0.1f))
			{
				Printf("moving, can't be used\n");
				continue;
			}

			if(vehScenarioType == VSCENARIO_TYPE_BROKEN_DOWN_VEHICLE)
			{
				if(!CTaskParkedVehicleScenario::CanVehicleBeUsedForBrokenDownScenario(*pVehicle))
				{
					Printf("not allowed for broken down vehicle scenario\n");
					continue;
				}
			}

			const ScalarV distSqV = DistSquared(pedPosV, vehiclePosV);
			if(IsGreaterThanAll(distSqV, closestDistSqV))
			{
				Printf("too far away\n");
				continue;
			}

			Printf("can be used\n");
			closestDistSqV = distSqV;
			pVehicleForScenario = pVehicle;
		}

		if(!listedAny)
		{
			Displayf("no vehicles nearby");
		}
	}

	// If desired, look for an existing scenario.
	if(usesPoint && ms_UseScenarioFindExisting)
	{
		CScenarioDebugClosestFilterScenarioType filter(scenarioTypeToCreate);
		filter.m_IgnoreInUse = true;
		CScenarioPoint* sp = GetClosestScenarioPoint(pedPosV, filter);
		if(sp)
		{
			Vec3V ptPosV = sp->GetPosition();
			ScalarV distSqThresholdV(square(8.0f));
			if(IsLessThanAll(DistSquared(pedPosV, ptPosV), distSqThresholdV))
			{
				pPropScenarioPoint = sp;
				Displayf("Found existing %s scenario.", ms_szScenarioNames[ms_iScenarioComboIndex]);
			}
		}
	}

	// If we don't have a scenario to use yet, consider creating a prop with the desired
	// scenario attached to it, if possible.
	if(usesPoint && ms_UseScenarioCreateProp && !pPropScenarioPoint)
	{
		// Destroy any existing prop.
		DestroyScenarioProp();

		// Set up an array of prop types we can choose from.
		static const int kMaxPropTypesUsingScenario = 128;
		fwModelId propsToChooseFrom[kMaxPropTypesUsingScenario];
		int numPropsToChooseFrom = 0;

		// Loop over all models and look for prop types with the right scenario attached.
		const int maxModelInfos = CModelInfo::GetMaxModelInfos();
		for(int i = 0; i < maxModelInfos; i++)
		{
			// Check if this is an object with scenarios attached.
			fwModelId modelId(strLocalIndex((u32)i));
			CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(modelId);
			if(!modelInfo || !modelInfo->GetIsTypeObject() || !modelInfo->GetHasSpawnPoints())
			{
				continue;
			}

			// Loop over the scenarios to see if any are of the right type.
			GET_2D_EFFECTS_NOLIGHTS(modelInfo);
			for(int j = 0; j < iNumEffects; j++)
			{
				Assert(pa2dEffects);
				C2dEffect* pEffect = (*pa2dEffects)[j];
				if(!pEffect)
				{
					continue;
				}
				if(pEffect->GetType() != ET_SPAWN_POINT)
				{
					continue;
				}
				const CSpawnPoint* spawnPt = pEffect->GetSpawnPoint();
				if(!spawnPt)
				{
					continue;
				}

				const int scenarioType = spawnPt->iType;
				if(scenarioType == scenarioTypeToCreate && numPropsToChooseFrom < kMaxPropTypesUsingScenario)
				{
					// Found one, add to the array.
					propsToChooseFrom[numPropsToChooseFrom++] = modelId;
					break;
				}
			}
		}

		// List all prop types that use the scenario, could be useful.
		if(numPropsToChooseFrom > 0)
		{
			Displayf("Scenario is attached to the following props:");
			for(int i = 0; i < numPropsToChooseFrom; i++)
			{
				CBaseModelInfo* pOtherModelInfo = CModelInfo::GetBaseModelInfo(propsToChooseFrom[i]);
				if(!pOtherModelInfo)
				{
					continue;
				}
				Displayf("  %s", pOtherModelInfo->GetModelName());
			}
		}
		else
		{
			Displayf("Scenario is not attached to any props.");
		}

		// Check if we found anything.
		if(numPropsToChooseFrom > 0)
		{
			// Cycle through the props in order.
			int chosen = ms_NextScenarioPropIndex;
			if(chosen >= numPropsToChooseFrom)
			{
				chosen = 0;
			}
			ms_NextScenarioPropIndex = chosen + 1;

			fwModelId modelId = propsToChooseFrom[chosen];
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
			if(Verifyf(pModelInfo, "Expected model info."))
			{
				// Note: most of this object creation code was adapted from DisplayObject()
				// in 'Debug.cpp'.

				// Set up a matrix with a random rotation around the Z axis.
				Matrix34 testMat;
				testMat.Identity();
				testMat.MakeRotateZ(g_ReplayRand.GetFloat()*2*PI);

				// Compute the matrix to spawn at, in front of the debug camera or near the player.
				Vector3 vecCreateOffset = camInterface::GetFront();
				vecCreateOffset.z = 0.0f;
				vecCreateOffset *= 2.0f + pModelInfo->GetBoundingSphereRadius();
				camDebugDirector& debugDirector = camInterface::GetDebugDirector();
				if(debugDirector.IsFreeCamActive())
				{
					testMat.d = camInterface::GetPos();
					testMat.d += vecCreateOffset;
				}
				else
				{
					if(ms_UseScenarioWarp)
					{
						// If warping and not using the debug camera, we don't add the camera's
						// forward vector, because it's annoying if we keep translating around the world.
						testMat.d = RCC_VECTOR3(pedPosV);
					}
					else
					{
						testMat.d = CGameWorld::FindLocalPlayerCoors();
						testMat.d += vecCreateOffset;
					}
				}

				// Stream in the object (note: not sure if we are supposed to unrequest the object somehow).
				if(!CModelInfo::HaveAssetsLoaded(modelId))
				{
					CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
					CStreaming::LoadAllRequestedObjects(true);
				}

				// Create the object.
				CObject* pObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
				if(pObject)
				{
					// Create a transform and add to the world.
					if (!pObject->GetTransform().IsMatrix())
					{
						//should never come here if ENABLE_MATRIX_MEMBER is set
						#if ENABLE_MATRIX_MEMBER
						Mat34V trans(V_IDENTITY);						
						ms_pScenarioProp->SetTransform(trans);					
						ms_pScenarioProp->SetTransformScale(1.0f, 1.0f);		
						#else
						fwTransform *trans = rage_new fwMatrixTransform();						
						pObject->SetTransform(trans);
						#endif
					}
					pObject->SetMatrix(testMat, true);
					if(fragInst* pInst = pObject->GetFragInst())
					{
						pInst->SetResetMatrix(testMat);
					}
					CGameWorld::Add(pObject, CGameWorld::OUTSIDE );

					// Snap to the ground.
					pObject->PlaceOnGroundProperly(10.0f, false);

					if(ms_UseFixedPhysicsForProps)
					{
						pObject->SetFixedPhysics(true);
					}

					// Next, we'll look for scenarios on the prop, as there may be more than one.
					static const int kMaxPtsPerProp = 32;
					CScenarioPoint* ptsOnProp[kMaxPtsPerProp];
					int numPtsOnProp = SCENARIOPOINTMGR.FindPointsOfTypeForEntity(scenarioTypeToCreate, pObject, ptsOnProp, NELEM(ptsOnProp));

					// Pick one at random.
					if(Verifyf(numPtsOnProp > 0, "Expected scenario points on prop."))
					{
						const int ptIndex = g_ReplayRand.GetRanged(0, numPtsOnProp - 1);
						pPropScenarioPoint = ptsOnProp[ptIndex];
					}

					Displayf("Created prop %s for %s scenario.", pModelInfo->GetModelName(), ms_szScenarioNames[ms_iScenarioComboIndex]);

					ms_DebugScenarioProp = pObject;
				}
			}
		}
	}

	bool bUsePointForTask = true;

	// If we haven't created or found a scenario already, create a temporary object now.
	CScenarioPoint tempPoint;
	if(usesPoint && !pPropScenarioPoint)
	{
		if(!taskVerifyf(CScenarioPoint::GetPool()->GetNoOfFreeSpaces() > 0, "Out of CScenarioPoint objects."))
		{
			return;
		}

		Vec3V scenarioPosV = pedPosV;

		// If warping, we use the ped's current position. If not, we find a point
		// in front of the camera or near the player.
		if(!ms_UseScenarioWarp)
		{
			Vec3V vecCreateOffsetV = RCC_VEC3V(camInterface::GetFront());
			vecCreateOffsetV.SetZ(ScalarV(V_ZERO));
			vecCreateOffsetV = Scale(vecCreateOffsetV, ScalarV(V_TWO));

			camDebugDirector& debugDirector = camInterface::GetDebugDirector();
			if(debugDirector.IsFreeCamActive())
			{
				scenarioPosV = RCC_VEC3V(camInterface::GetPos());
				scenarioPosV += vecCreateOffsetV;
			}
			else
			{
				scenarioPosV = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
				scenarioPosV += vecCreateOffsetV;
			}
		}

		// Need to get this to ground level.
		CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE,&RC_VECTOR3(scenarioPosV), NULL, NULL, NULL, 0.5f, 99.5f, ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE, NULL, true);

		// Make use of this temporary object on the stack. It will never be inserted in the world
		// as a scenario point, it's just used to set up the ped's task/position/heading.
		pPropScenarioPoint = &tempPoint;

		Assert((unsigned)scenarioTypeToCreate < MAX_SCENARIO_TYPES);
		pPropScenarioPoint->SetScenarioType((unsigned)scenarioTypeToCreate);

		Vector3	forward = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetB());
		pPropScenarioPoint->SetDirection(rage::Atan2f(-forward.x, forward.y));
		pPropScenarioPoint->SetPosition(scenarioPosV);
		pPropScenarioPoint->SetTimeStartOverride(0);
		pPropScenarioPoint->SetTimeEndOverride(24);
		pPropScenarioPoint->SetModelSetIndex(CScenarioPointManager::kNoCustomModelSet);
		pPropScenarioPoint->SetInteriorState(CScenarioPoint::IS_Outside);
		pPropScenarioPoint->SetScenarioGroup(CScenarioPointManager::kNoGroup);
		pPropScenarioPoint->SetAvailability(CScenarioPoint::kBoth);
		pPropScenarioPoint->SetEditable(false);

		bUsePointForTask = false;

		Displayf("Created temporary %s scenario.", ms_szScenarioNames[ms_iScenarioComboIndex]);
	}
	Assert(!usesPoint || pPropScenarioPoint);


	bool bStartInCurrentPos = !ms_UseScenarioWarp;

	if(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioParkedVehicleInfo>())
	{
		if(pVehicleForScenario)
		{
			CTask* pScenarioTask = pScenarioTask = rage_new CTaskParkedVehicleScenario(scenarioTypeToCreate, pVehicleForScenario);
			pFocusPed->GetPedIntelligence()->AddTaskDefault(pScenarioTask);
		}
	}
	else
	{

		int scenarioTypeToCreateReal = CScenarioManager::ChooseRealScenario(scenarioTypeToCreate, pFocusPed);

		pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioTypeToCreateReal);

		float headingOut = 0.0f;
		Vector3 posOut;
		CTask* pScenarioTask = CScenarioManager::SetupScenarioAndTask( scenarioTypeToCreateReal, posOut, headingOut, *pPropScenarioPoint, 0, bStartInCurrentPos, bUsePointForTask, true );
		aiTask* pControlTask = CScenarioManager::SetupScenarioControlTask(*pFocusPed, *pScenarioInfo, pScenarioTask, pPropScenarioPoint, scenarioTypeToCreateReal);
		pFocusPed->GetPedIntelligence()->AddTaskDefault(pControlTask);
	}
}

void CScenarioDebug::DeActivateScenarioOnSelectedPedCB()
{
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if (!pEnt)
	{
		pEnt = CGameWorld::FindLocalPlayer();
	}

	if( pEnt && pEnt->GetIsTypePed() )
	{
		CPed* pFocusPed = static_cast<CPed*>(pEnt);

		CTask* pScenarioTask = pFocusPed->GetPedIntelligence()->GetTaskActive();
		if(pScenarioTask)
		{
			CTask::AbortPriority pri = ms_StopTargetPedImmediate ? CTask::ABORT_PRIORITY_IMMEDIATE : CTask::ABORT_PRIORITY_URGENT;
			pScenarioTask->MakeAbortable(pri, 0);
		}
	}
}

void CScenarioDebug::DestroyScenarioProp()
{
	if(!ms_DebugScenarioProp)
	{
		return;
	}

	CObject* prop = ms_DebugScenarioProp;
	CGameWorld::Remove(prop);
	CObjectPopulation::DestroyObject(prop);
	ms_DebugScenarioProp = NULL;
}

void CScenarioDebug::LoadPropOffsetsCB()
{
	s32 iNumScenarios = SCENARIOINFOMGR.GetScenarioCount(false);

	for (s32 i=0; i<iNumScenarios; i++)
	{
		CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(i);
		if (taskVerifyf(pScenarioInfo, "NULL scenario info"))
		{
			if (pScenarioInfo->GetSpawnPropOffsetDictHash() != 0 && pScenarioInfo->GetSpawnPropOffsetClipHash() !=0)
			{
				strLocalIndex iDictIndex = fwAnimManager::FindSlotFromHashKey(pScenarioInfo->GetSpawnPropOffsetDictHash());
				if (taskVerifyf(iDictIndex != -1, "Couldn't find clip dictionary %s", pScenarioInfo->GetSpawnPropIntroDictName()))
				{
					// Stream in the clip dictionary
					CStreaming::RequestObject(iDictIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
					CStreaming::LoadAllRequestedObjects();
					const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex.Get(), pScenarioInfo->GetSpawnPropOffsetClipHash());
					if (taskVerifyf(pClip, "Couldn't find clip %s in dictionary %s", pScenarioInfo->GetSpawnPropIntroClipName(), pScenarioInfo->GetSpawnPropIntroDictName()))
					{
						Vector3 vTransOffset(Vector3::ZeroType);
						Quaternion qRotationalOffset(0.0f, 0.0f, 0.0f, 1.0f);
						if (fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, vTransOffset, qRotationalOffset))
						{
							pScenarioInfo->SetSpawnPropOffset(vTransOffset);
							pScenarioInfo->SetSpawnPropRotationalOffset(qRotationalOffset);
						}
					}
					CStreaming::RemoveObject(iDictIndex, fwAnimManager::GetStreamingModuleId());
				}
			}
		}
	}
}

void CScenarioDebug::ForceSpawnInAreaCB()
{
	Vector3 v1, v2;
	if( !CPhysics::GetMeasuringToolPos( 0, v1 ) ||
		!CPhysics::GetMeasuringToolPos( 1, v2 ) )
	{
		v1 = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
		v2 = v1;
		v2.x += 80.0f;
	}
	const bool allowDeepInteriors = true;
	CPedPopulation::GenerateScenarioPedsInSpecificArea(v1, allowDeepInteriors, (v1-v2).Mag(), 100);
}

void CScenarioDebug::ClearSpawnHistoryCB()
{
	CScenarioManager::GetSpawnHistory().ClearHistory();
	CScenarioManager::GetVehicleScenarioManager().ClearAttractionTimers();
}

void CScenarioDebug::ReloadAmbientData()
{
//	CAmbientAnimationManager::UnloadData();
//	CAmbientAnimationManager::LoadData();
}

void CScenarioDebug::GiveRandomProp()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			u32 uiPropHash;
			int animIndex = -1;
			const CConditionalAnimsGroup* pGrp = CONDITIONALANIMSMGR.GetConditionalAnimsGroup("WANDER");
			CScenarioCondition::sScenarioConditionData conditionData;
			conditionData.pPed = pFocusPed;
			if( pGrp && CAmbientAnimationManager::RandomlyChoosePropForSpawningPed( conditionData, pGrp, uiPropHash, animIndex, -1) )
			{
				const CConditionalAnims* pAnims = pGrp->GetAnims(animIndex);
				strStreamingObjectName prop;
				prop.SetHash( uiPropHash );
				CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
				if (pTask)
				{
						pTask->GivePedProp( prop, pAnims->GetCreatePropInLeftHand(), pAnims->ShouldDestroyPropInsteadOfDropping() );
				}
			}
		}
	}
}

void CScenarioDebug::GivePedAmbientTaskCB()
{
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);

	if (!pEnt)
	{
		pEnt = CGameWorld::FindLocalPlayer();
	}

	if (pEnt && pEnt->GetIsTypePed() && ms_ConditionalAnimNames.GetCount() > 0)
	{
		CPed* pFocusPed = static_cast<CPed*>(pEnt);
		const ConditionalAnimData& d = ms_ConditionalAnims[ms_iAmbientContextComboIndex];

		CTaskComplexControlMovement* pControlTask = static_cast<CTaskComplexControlMovement*>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
		if (pControlTask)
		{
			u32 flags = GetAmbientFlags();
			pControlTask->SetNewMainSubtask(rage_new CTaskAmbientClips(flags, d.m_Group, d.m_AnimIndexWithinGroup));
		}
		else
		{
			// They weren't running a complex control movement task, so at least give them wander with the given animations.
			CTaskWander* pTaskWander = rage_new CTaskWander(MOVEBLENDRATIO_WALK, pFocusPed->GetCurrentHeading(), d.m_Group, d.m_AnimIndexWithinGroup);
			
			// Possibly create a prop.
			if (ms_bPickPermanentProp)
			{
				pTaskWander->CreateProp();
			}

			// Decide if we should ignore probability when creating the prop.
			if (ms_bForcePropCreation)
			{
				pTaskWander->ForceCreateProp();
			}

			// Give the focus ped the wander task.
			pFocusPed->GetPedIntelligence()->AddEvent(CEventGivePedTask(PED_TASK_PRIORITY_PRIMARY, pTaskWander));
		}
	}
}

u32 CScenarioDebug::GetAmbientFlags()
{
	u32 iFlags = 0;
	if (ms_bPlayBaseClip)				iFlags |= CTaskAmbientClips::Flag_PlayBaseClip;
	if (ms_bInstantlyBlendInBaseClip)	iFlags |= CTaskAmbientClips::Flag_PlayIdleClips;
	if (ms_bPlayIdleClips)				iFlags |= CTaskAmbientClips::Flag_PlayIdleClips;
	if (ms_bPickPermanentProp)			iFlags |= CTaskAmbientClips::Flag_PickPermanentProp;
	if (ms_bForcePropCreation)			iFlags |= CTaskAmbientClips::Flag_ForcePropInPickPermanent;
	if (ms_bOverrideInitialIdleTime)	iFlags |= CTaskAmbientClips::Flag_OverrideInitialIdleTime;
	return iFlags;
}

//-----------------------------------------------------------------------------
// Scenario filters

CScenarioDebug::CScenarioDebugClosestFilterScenarioInUse::CScenarioDebugClosestFilterScenarioInUse()
 : m_IgnoreInUse(false)
{
}

bool CScenarioDebug::CScenarioDebugClosestFilterScenarioInUse::Match(const CScenarioPoint& scenarioPt) const
{
	if (m_IgnoreInUse && CPedPopulation::IsEffectInUse(scenarioPt))
	{
		return false;
	}
	return true;
}

CScenarioDebug::CScenarioDebugClosestFilterScenarioType::CScenarioDebugClosestFilterScenarioType(int scenarioType)
		: CScenarioDebugClosestFilterScenarioInUse()
		, m_ScenarioType(scenarioType)
{
}

bool CScenarioDebug::CScenarioDebugClosestFilterScenarioType::Match(const CScenarioPoint& scenarioPt) const
{
	if(!CScenarioDebugClosestFilterScenarioInUse::Match(scenarioPt))
	{
		return false;
	}

	return scenarioPt.GetScenarioTypeVirtualOrReal() == m_ScenarioType;
}


CScenarioDebug::CScenarioDebugClosestFilterNotVisited::CScenarioDebugClosestFilterNotVisited(int scenarioType)
		: CScenarioDebugClosestFilterScenarioType(scenarioType)
{
}


bool CScenarioDebug::CScenarioDebugClosestFilterNotVisited::Match(const CScenarioPoint& scenarioPt) const
{
	if(!CScenarioDebugClosestFilterScenarioType::Match(scenarioPt))
	{
		return false;
	}

	const int numVisited = CScenarioDebug::ms_WarpToNearestAlreadyVisited.GetCount();
	for(int j = 0; j < numVisited; j++)
	{
		if(CScenarioDebug::ms_WarpToNearestAlreadyVisited[j].m_pScenarioPoint == &scenarioPt)
		{
			return false;
		}
	}

	return true;
}


CScenarioDebug::CScenarioDebugClosestFilterSpecific::CScenarioDebugClosestFilterSpecific(int scenarioType, CScenarioPoint* pScenarioPoint)
		: CScenarioDebugClosestFilterScenarioType(scenarioType)
		, m_pScenarioPoint(pScenarioPoint)
{
}


bool CScenarioDebug::CScenarioDebugClosestFilterSpecific::Match(const CScenarioPoint& scenarioPt) const
{
	if(!CScenarioDebugClosestFilterScenarioType::Match(scenarioPt))
	{
		return false;
	}
	if(&scenarioPt != m_pScenarioPoint)
	{
		return false;
	}
	return true;
}


CScenarioPoint* CScenarioDebug::GetClosestScenarioPoint(Vec3V_In fromPos, const CScenarioDebugClosestFilter& filter)
{
	const Vec3V fromPosV = fromPos;
	float fFoundPointDistanceSq = FLT_MAX;
	CScenarioPoint* pFoundPoint = NULL;

	//Check the regions ... first ... 
	int activeRegionCnt = SCENARIOPOINTMGR.GetNumActiveRegions();
	for (int r = 0; r < activeRegionCnt; r++)
	{
		CScenarioPointRegion& region = SCENARIOPOINTMGR.GetActiveRegion(r);
		CScenarioPointRegionEditInterface bankRegion(region);
		pFoundPoint = bankRegion.FindClosestPoint(fromPosV, filter, pFoundPoint, &fFoundPointDistanceSq);
	}

	//Check the entity points ... 
	s32 count = SCENARIOPOINTMGR.GetNumEntityPoints();
	for(s32 i = 0; i < count; i++)
	{
		CScenarioPoint& spoint = SCENARIOPOINTMGR.GetEntityPoint(i);
		
		const Vec3V posV = spoint.GetPosition();
		const float distanceSq = DistSquared(posV, fromPosV).Getf();
		if(distanceSq < fFoundPointDistanceSq)
		{
			if(filter.Match(spoint))
			{
				fFoundPointDistanceSq = distanceSq;
				pFoundPoint = &spoint;
			}
		}
	}
	
	count = SCENARIOPOINTMGR.GetNumWorldPoints();
	for(s32 i = 0; i < count; i++)
	{
		CScenarioPoint& spoint = SCENARIOPOINTMGR.GetWorldPoint(i);

		const Vec3V posV = spoint.GetPosition();
		const float distanceSq = DistSquared(posV, fromPosV).Getf();
		if(distanceSq < fFoundPointDistanceSq)
		{
			if(filter.Match(spoint))
			{
				fFoundPointDistanceSq = distanceSq;
				pFoundPoint = &spoint;
			}
		}
	}
	return pFoundPoint;
}

void CScenarioDebug::UpdateScenarioGroupCombo()
{
	if (!ms_scenarioGroupCombo)
	{
		return;
	}

	ms_scenarioGroupCombo->UpdateCombo("SelectedGroup", &ms_iScenarioGroupIndex, ms_Editor.m_GroupNames.GetCount(), &ms_Editor.m_GroupNames[0]);
}

//-----------------------------------------------------------------------------

#endif // SCENARIO_DEBUG
