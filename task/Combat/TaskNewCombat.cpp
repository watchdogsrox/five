// FILE :    TaskCombat.cpp
// PURPOSE : Combat tasks to replace killped* tasks, greater dependancy on decision makers
//			 So that designers can tweak combat behaviour and proficiency based on character type
// AUTHOR :  Phil H
// CREATED : 18-08-2005

// C++ headers
#include <limits>

// Game headers
#include "AI/ambient/ConditionalAnimManager.h"
#include "animation/FacialData.h"
#include "animation/MovePed.h"
#include "Audio/policescanner.h"
#include "debug/DebugScene.h"
#include "Event/Events.h"
#include "Event/EventDamage.h"
#include "Event/EventMovement.h"
#include "Event/EventWeapon.h"
#include "Event/EventReactions.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "Game/Clock.h"
#include "Game/Dispatch/Orders.h"
#include "Game/GameSituation.h"
#include "PedGroup/formation.h"
#include "Peds/Ped.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "peds/QueriableInterface.h"
#include "script/script_itemsets.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/Cover/coverfilter.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "task/Combat/Subtasks/BoatChaseDirector.h"
#include "Task/Combat/Subtasks/TaskAdvance.h"
#include "Task/Combat/Subtasks/TaskCharge.h"
#include "Task/Combat/Subtasks/TaskDraggingToSafety.h"
#include "task/Combat/Subtasks/TaskReactToBuddyShot.h"
#include "Task/Combat/Subtasks/TaskShellShocked.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"
#include "Task/Combat/Subtasks/TaskTargetUnreachable.h"
#include "Task/Combat/Subtasks/TaskVariedAimPose.h"
#include "Task/Combat/Subtasks/TaskVehicleCombat.h"
#include "Task/Combat/Subtasks/VehicleChaseDirector.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskCombatMounted.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Combat/TaskSearch.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskCuffed.h"
#include "task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Movement/TaskMoveWithinAttackWindow.h"
#include "Task/Movement/TaskMoveWithinDefensiveArea.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Response/TaskFlee.h"
#include "task/Response/TaskReactToExplosion.h"
#include "task/Response/TaskReactToImminentExplosion.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "vfx/misc/Fire.h"

// rage
#include "crskeleton/skeleton.h"
#include "math/angmath.h"
#include "physics/shapetest.h"
#include "profile/profiler.h"

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

#if __BANK
#include "Camera/CamInterface.h"
#include "Peds/PedDebugVisualiser.h"

// For measuring task size
//#if !__64BIT
//CompileTimeAssert(sizeof(CTaskCombat) <= 416);
//#endif

namespace AICombatStats
{
	PF_PAGE(AICombat, "AI Combat");
	PF_GROUP(CoverUpdate);
	PF_GROUP(Functions);
	PF_GROUP(Tasks);
	PF_LINK(AICombat, CoverUpdate);
	PF_LINK(AICombat, Functions);
	PF_LINK(AICombat, Tasks);

	PF_TIMER(CTaskCombat, Tasks);
	PF_TIMER(CTaskHeliPassengerRappel, Tasks);
	PF_TIMER(CTaskVariedAimPose, Tasks);

	PF_TIMER(CalculateVantagePointForCoverPoint, Functions);
	PF_TIMER(ComputeHighCoverStepOutPosition, Functions);
	PF_TIMER(CTacticalAnalysis_Update, Functions);
	PF_TIMER(FindClosestCoverPoint, Functions);
	PF_TIMER(FindClosestCoverPointWithCB, Functions);
	PF_TIMER(FindCoverPointInArc, Functions);
	PF_TIMER(FindCoverPoints, Functions);
	PF_TIMER(FindCoverPointsWithinRange, Functions);
	PF_TIMER(UpdateGestureAnim, Functions);
	PF_TIMER(UpdateCombatDirectors, Functions);
	PF_TIMER(UpdateVehicleChaseDirectors, Functions);
	PF_TIMER(WantsToChargeTarget, Functions);
	PF_TIMER(WantsToThrowSmokeGrenade, Functions);

	PF_TIMER(CoverUpdateTotal, CoverUpdate);
	PF_TIMER_OFF(CoverUpdateArea, CoverUpdate);
	PF_TIMER_OFF(CoverUpdateMap, CoverUpdate);
	PF_TIMER_OFF(CoverUpdateMoveAndRemove, CoverUpdate);
	PF_TIMER_OFF(CoverUpdateObjects, CoverUpdate);
	PF_TIMER_OFF(CoverUpdateStatus, CoverUpdate);
	PF_TIMER_OFF(CoverUpdateVehicles, CoverUpdate);
}
using namespace AICombatStats;


bool	CCombatDebug::ms_bRenderCombatTargetInfo		= false;
bool	CCombatDebug::ms_bPlayGlanceAnims				= true;
float	CCombatDebug::ms_fSetHealth						= 200.0f;
float	CCombatDebug::ms_fIdentificationRange			= 20.0f;
float	CCombatDebug::ms_fSeeingRange					= 60.0f;
float	CCombatDebug::ms_fSeeingRangePeripheral			= 5.0f;
float	CCombatDebug::ms_fVisualFieldMinElevationAngle	= -75.0f;
float	CCombatDebug::ms_fVisualFieldMaxElevationAngle	= 60.0f;
float	CCombatDebug::ms_fVisualFieldMinAzimuthAngle	= -90.0f;
float	CCombatDebug::ms_fVisualFieldMaxAzimuthAngle	= 90.0f;
float	CCombatDebug::ms_fCenterOfGazeMaxAngle			= 60.0f;
float	CCombatDebug::ms_fHearingRange					= 60.0f;
char	CCombatDebug::ms_vDefensiveAreaPos1[256]		= "";
char	CCombatDebug::ms_vDefensiveAreaPos2[256]		= "";
float	CCombatDebug::ms_fDefensiveAreaWidth			= 5.0f;
bool	CCombatDebug::ms_bDefensiveAreaAttachToPlayer	= false;
bool	CCombatDebug::ms_bDefensiveAreaDragging			= false;
bool	CCombatDebug::ms_bGiveSphereDefensiveArea		= false;
bool	CCombatDebug::ms_bRenderPreferredCoverPoints	= false;
bool	CCombatDebug::ms_bUseSecondaryDefensiveArea		= false;

void CCombatDebug::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{	
		pBank->PushGroup("Combat", false);

			pBank->AddToggle("Render combat target info",&ms_bRenderCombatTargetInfo);
			pBank->AddToggle("Play glance animations", &ms_bPlayGlanceAnims);
			pBank->AddToggle("Add waypoints at slope changes in CTaskComplexSeparate", &CTaskSeparate::ms_bPreserveSlopeInfoInPath);
			pBank->AddToggle("Use hurt combat strafing", &CPed::ms_bUseHurtCombat);
			pBank->AddToggle("Activate ragdoll on collision in Writhe", &CPed::ms_bActivateRagdollOnCollision);
			pBank->AddToggle("Enable toggling between strafe and run", &CTaskCombat::ms_bEnableStrafeToRunBreakup);

			pBank->PushGroup("Modify Ped", false);
				pBank->AddButton("Get Focus ped to attack player",						TellFocusPedToAttackPlayerCB, "Tells the focus ped to attack the player!");
				pBank->AddSlider("Change focus peds health",							&ms_fSetHealth,0.0f,10000.0f,0.5f, PedHealthChangeCB );
				pBank->AddSlider("Change focus peds seeing range",						&ms_fSeeingRange,0.0f,1000.0f,0.5f, PedSeeingRangeChangeCB );
				pBank->AddSlider("Change focus peds seeing range peripheral",			&ms_fSeeingRangePeripheral,0.0f,1000.0f,0.5f, PedSeeingRangePeripheralChangeCB );
				pBank->AddSlider("Change focus peds visual field min elevation angle",	&ms_fVisualFieldMinElevationAngle,-180.0f,0.0f,0.5f, PedVisualFieldMinElevationAngleChangeCB );
				pBank->AddSlider("Change focus peds visual field max elevation angle",	&ms_fVisualFieldMaxElevationAngle,0.0f,180.0f,0.5f, PedVisualFieldMaxElevationAngleChangeCB );
				pBank->AddSlider("Change focus peds visual field min azimuth angle",	&ms_fVisualFieldMinAzimuthAngle,-180.0f,0.0f,0.5f, PedVisualFieldMinAzimuthAngleChangeCB );
				pBank->AddSlider("Change focus peds visual field max azimuth angle",	&ms_fVisualFieldMaxAzimuthAngle,0.0f,180.0f,0.5f, PedVisualFieldMaxAzimuthAngleChangeCB );
				pBank->AddSlider("Change focus peds center of gaze max angle",			&ms_fCenterOfGazeMaxAngle,0.0f,180.0f,0.5f, PedCenterOfGazeMaxAngleChangeCB );
				pBank->AddSlider("Change focus peds id range",							&ms_fIdentificationRange,0.0f,1000.0f,0.5f, PedIdRangeChangeCB );
				pBank->AddSlider("Change focus peds hearing range",						&ms_fHearingRange,0.0f,1000.0f,0.5f, PedHearingRangeChangeCB );
			pBank->PopGroup();

			pBank->PushGroup("Combat Behaviour Flags", false);
				pBank->AddButton("Set Use Cover",SetCanUseCoverFlagCB);
				pBank->AddButton("UnSet Use Cover",UnSetCanUseCoverFlagCB);
				pBank->AddButton("Set Use Vehicles",SetCanUseVehiclesFlagCB);
				pBank->AddButton("UnSet Use Vehicles",UnSetCanUseVehiclesFlagCB);
				pBank->AddButton("Set Can Do DriveBys",SetCanDoDriveBysFlagCB);
				pBank->AddButton("UnSet Can Do DriveBys",UnSetCanDoDriveBysFlagCB);
				pBank->AddButton("Set Can Leave Vehicle",SetCanLeaveVehicleCB);
				pBank->AddButton("UnSet Can Leave Vehicle",UnSetCanLeaveVehicleCB);
				pBank->AddButton("Set Can Investigate",SetCanInvestigateFlagCB);
				pBank->AddButton("UnSet Can Investigate",UnSetCanInvestigateFlagCB);
				pBank->AddButton("Set Flee Whilst In Vehicle",SetFleeWhilstInVehicleFlagCB);
				pBank->AddButton("UnSet Flee Whilst In Vehicle",UnSetFleeWhilstInVehicleFlagCB);
				pBank->AddButton("Set Just Follow In Vehicle",SetJustFollowInVehicleFlagCB);
				pBank->AddButton("UnSet Just Follow In Vehicle",UnSetJustFollowInVehicleFlagCB);
				pBank->AddButton("Set Just Seek Cover",SetJustSeekCoverFlagCB);
				pBank->AddButton("UnSet Just Seek Cover",UnSetJustSeekCoverFlagCB);
				pBank->AddButton("Set Will Scan For Dead Peds",SetWillScanForDeadPedsFlagCB);
				pBank->AddButton("UnSet Will Scan For Dead Peds",UnSetWillScanForDeadPedsFlagCB);
				pBank->AddButton("Set Just Seek Cover",SetJustSeekCoverFlagCB);
				pBank->AddButton("UnSet Just Seek Cover",UnSetJustSeekCoverFlagCB);
				pBank->AddButton("Set Blind Fire When In Cover",SetBlindFireWhenInCoverFlagCB);
				pBank->AddButton("UnSet Blind Fire When In Cover",UnSetBlindFireWhenInCoverFlagCB);
				pBank->AddButton("Set Aggressive",SetAggressiveFlagCB);
				pBank->AddButton("UnSet Aggressive",UnSetAggressiveFlagCB);
				pBank->AddButton("Set Has Radio",SetHasRadioFlagCB);
				pBank->AddButton("UnSet Has Radio",UnSetHasRadioFlagCB);
				pBank->AddButton("Set Can Use Vehicle Taunts",SetCanUseVehicleTauntsCB);
				pBank->AddButton("UnSet Can Use Vehicle Taunts",UnSetCanUseVehicleTauntsCB);
				pBank->AddButton("Set Will Drag Injured Peds To Safety",SetWillDragInjuredPedsToSafetyCB);
				pBank->AddButton("UnSet Will Drag Injured Peds To Safety",UnSetWillDragInjuredPedsToSafetyCB);
				pBank->AddButton("Set Maintain Minimum Distance To Target",SetMaintainMinDistanceToTargetCB);
				pBank->AddButton("UnSet Maintain Minimum Distance To Target",UnSetMaintainMinDistanceToTargetCB);
				pBank->AddButton("Set Can Use Peeking Variations", SetCanUsePeekingVariationsCB);
				pBank->AddButton("UnSet Can Use Peeking Variations",UnSetCanUsePeekingVariationsCB);
				pBank->AddButton("Set Enable Tactical Points When Defensive", SetEnableTacticalPointsWhenDefensiveCB);
				pBank->AddButton("UnSet Enable Tactical Points When Defensive",UnSetEnableTacticalPointsWhenDefensiveCB);
				pBank->AddButton("Set Disable Aim At AI Targets in Helicopters", SetDisableAimAtAITargetsInHelisCB);
				pBank->AddButton("UnSet Disable Aim At AI Targets in Helicopters", UnSetDisableAimAtAITargetsInHelisCB);
				pBank->AddButton("Set Can Ignore Blocked Los Weighting", SetCanIgnoreBlockedLosWeightingCB);
				pBank->AddButton("UnSet Can Ignore Blocked Los Weighting", UnSetCanIgnoreBlockedLosWeightingCB);
				pBank->AddButton("Set WillGenerateDeadPedSeenScriptEvents",SetWillGenerateDeadPedSeenScriptEventsFlagCB);
				pBank->AddButton("UnSet WillGenerateDeadPedSeenScriptEvents",UnSetWillGenerateDeadPedSeenScriptEventsFlagCB);
			pBank->PopGroup();

			pBank->PushGroup("Combat Movement Flags",false);
				pBank->AddButton("Set Stationary Flag", SetStationaryCB);
				pBank->AddButton("Set Defensive Flag",SetDefensiveCB);
				pBank->AddButton("Set Will Advance Flag",SetWillAdvanceCB);
				pBank->AddButton("Set Will Retreat",SetWillRetreatCB);
			pBank->PopGroup();

			pBank->PushGroup("Combat Range",false);
				pBank->AddButton("Near", SetAttackRangeNearCB);
				pBank->AddButton("Medium",SetAttackRangeMediumCB);
				pBank->AddButton("Far",SetAttackRangeFarCB);
			pBank->PopGroup();

			pBank->PushGroup("Target Loss Response",false);
				pBank->AddButton("Never Lose Target", SetNeverLoseTargetCB);
			pBank->PopGroup();

			pBank->PushGroup("Defensive areas", false);
				pBank->AddToggle("Use secondary defensive area", &ms_bUseSecondaryDefensiveArea, NullCB);
				pBank->AddToggle("Display", &CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualiseDefensiveAreas, NullCB, "Displays defensive areas, with a line from each ped to their area.");
				pBank->AddToggle("Display Secondary", &CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualiseSecondaryDefensiveAreas, NullCB, "Displays secondary defensive areas, with a line from each ped to their area.");
				pBank->PushGroup("Sphere", false);
					pBank->AddButton("Add ped sphere defensive area", CreateSphereDefensiveAreaForFocusPedCB, "Gives the specified defensive area to the focus ped!");
					pBank->AddSlider("Defensive area radius",	&ms_fDefensiveAreaWidth,0.0f,100.0f,0.5f, UpdateRadiusForFocusPedCB);
				pBank->PopGroup();
				pBank->PushGroup("Angled", false);
					pBank->AddText("Defensive area pos1", ms_vDefensiveAreaPos1, sizeof(ms_vDefensiveAreaPos1), false);
					pBank->AddButton("Get pos1 from camera", GetDefensivePos1FromCameraCoordsCB, "Fills in the position from the camera coords!");
					pBank->AddText("Defensive area pos2", ms_vDefensiveAreaPos2, sizeof(ms_vDefensiveAreaPos2), false);
					pBank->AddButton("Get pos2 from camera", GetDefensivePos2FromCameraCoordsCB, "Fills in the position from the camera coords!");
					pBank->AddSlider("Defensive area width",	&ms_fDefensiveAreaWidth,0.0f,100.0f,0.5f, UpdateRadiusForFocusPedCB);
					pBank->AddButton("Add defensive area to focus ped", CreateAngledDefensiveAreaForFocusPedCB, "Gives the specified defensive area to the focus ped!");
				pBank->PopGroup();
				pBank->AddToggle("Drag focus peds defensive area", &ms_bDefensiveAreaDragging, NullCB, "Allows dragging of focus peds defensive area.");
				pBank->AddButton("Print out focus peds defensive area info", PrintDefensiveAreaCB, "Prints out the vectors and widths that make up the peds defensive decision maker!");
				pBank->AddToggle("Attach to player", &ms_bDefensiveAreaAttachToPlayer, NullCB, "The defensive area created will be attached to the player.");
			pBank->PopGroup();

			pBank->PushGroup("Preferred cover", false);
				pBank->AddToggle("Render preferred cover", &ms_bRenderPreferredCoverPoints);
			pBank->PopGroup();

			CCombatManager::GetInstance().AddWidgets(*pBank);

		pBank->PopGroup();
	}
}

//-------------------------------------------------------------------------
// Causes the ped selected to attack the player
//-------------------------------------------------------------------------
void CCombatDebug::TellFocusPedToAttackPlayerCB()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			CPedIntelligence * pPedAI = pFocusPed->GetPedIntelligence();
			pPedAI->ClearTasks();
			aiTask* pCombatTask = rage_new CTaskThreatResponse(FindPlayerPed());
			pPedAI->AddTaskDefault(pCombatTask);
		}
	}
}

void CCombatDebug::PedHealthChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

	for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
		if(pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
			pFocusPed->SetHealth(ms_fSetHealth);
			ASSERT_ONLY(bFoundOne = true;)
		}
	}

	aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedIdRangeChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

	for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
		if(pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
			pFocusPed->GetPedIntelligence()->GetPedPerception().SetIdentificationRange(ms_fIdentificationRange);
			ASSERT_ONLY(bFoundOne = true;)
		}
	}

	aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedSeeingRangeChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

	for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
		if(pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
			pFocusPed->GetPedIntelligence()->GetPedPerception().SetSeeingRange(ms_fSeeingRange);
			ASSERT_ONLY(bFoundOne = true;)
		}
	}

	aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedSeeingRangePeripheralChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

		for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
			if(pFocusEntity && pFocusEntity->GetIsTypePed())
			{
				CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetSeeingRangePeripheral(ms_fSeeingRangePeripheral);
				ASSERT_ONLY(bFoundOne = true;)
			}
		}

		aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedVisualFieldMinElevationAngleChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

		for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
			if(pFocusEntity && pFocusEntity->GetIsTypePed())
			{
				CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMinElevationAngle(ms_fVisualFieldMinElevationAngle);
				ASSERT_ONLY(bFoundOne = true;)
			}
		}

		aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedVisualFieldMaxElevationAngleChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

		for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
			if(pFocusEntity && pFocusEntity->GetIsTypePed())
			{
				CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMaxElevationAngle(ms_fVisualFieldMaxElevationAngle);
				ASSERT_ONLY(bFoundOne = true;)
			}
		}

		aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedVisualFieldMinAzimuthAngleChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

		for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
			if(pFocusEntity && pFocusEntity->GetIsTypePed())
			{
				CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMinAzimuthAngle(ms_fVisualFieldMinAzimuthAngle);
				ASSERT_ONLY(bFoundOne = true;)
			}
		}

		aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedVisualFieldMaxAzimuthAngleChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

		for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
			if(pFocusEntity && pFocusEntity->GetIsTypePed())
			{
				CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetVisualFieldMaxAzimuthAngle(ms_fVisualFieldMaxAzimuthAngle);
				ASSERT_ONLY(bFoundOne = true;)
			}
		}

		aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedCenterOfGazeMaxAngleChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

		for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
			if(pFocusEntity && pFocusEntity->GetIsTypePed())
			{
				CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetCentreOfGazeMaxAngle(ms_fCenterOfGazeMaxAngle);
				ASSERT_ONLY(bFoundOne = true;)
			}
		}

		aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::PedHearingRangeChangeCB()
{
	ASSERT_ONLY(bool bFoundOne = false;)

	for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
		if(pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
			pFocusPed->GetPedIntelligence()->GetPedPerception().SetHearingRange(ms_fHearingRange);
			ASSERT_ONLY(bFoundOne = true;)
		}
	}

	aiAssertf(bFoundOne, "No Focus peds selected!");
}

void CCombatDebug::SetBehaviourFlag(s32 iFlag)
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if (pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
		pFocusPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag((CCombatData::BehaviourFlags)iFlag);
	}
}

void CCombatDebug::UnSetBehaviourFlag(s32 iFlag)
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if (pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
		pFocusPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag((CCombatData::BehaviourFlags)iFlag);
	}
}

void CCombatDebug::SetMovementFlag(CCombatData::Movement iFlag)
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if (pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
		pFocusPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(iFlag);
	}
}

void CCombatDebug::SetAttackRange(CCombatData::Range eRange)
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if (pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
		pFocusPed->GetPedIntelligence()->GetCombatBehaviour().SetAttackRange(eRange);
	}
}

void CCombatDebug::SetNeverLoseTargetCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if (pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
		pFocusPed->GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse(CCombatData::TLR_NeverLoseTarget);
	}
}

void CCombatDebug::GetDefensivePos1FromCameraCoordsCB()
{
	sprintf( ms_vDefensiveAreaPos1, "%2.2f %2.2f %2.2f", camInterface::GetPos().x, camInterface::GetPos().y, camInterface::GetPos().z );
}

void CCombatDebug::GetDefensivePos2FromCameraCoordsCB()
{
	sprintf( ms_vDefensiveAreaPos2, "%2.2f %2.2f %2.2f", camInterface::GetPos().x, camInterface::GetPos().y, camInterface::GetPos().z );
}

void CCombatDebug::CreateSphereDefensiveAreaForFocusPedCB()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			Vector3 vPos = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition());
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			// Attach to the player if the option is ticked, orientate if the player is in a vehicle
			CPed* pAttach = ms_bDefensiveAreaAttachToPlayer ? FindPlayerPed() : NULL;

			CDefensiveArea* pDefensiveArea = !ms_bUseSecondaryDefensiveArea ?
				pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea() :
				pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetSecondaryDefensiveArea(true);
			pDefensiveArea->SetAsSphere(vPos, ms_fDefensiveAreaWidth, pAttach);
		}
	}
}

void CCombatDebug::CreateAngledDefensiveAreaForFocusPedCB()
{
	Vector3 vPos1, vPos2;
	if(sscanf(ms_vDefensiveAreaPos1, "%f %f %f", &vPos1.x, &vPos1.y, &vPos1.z) &&
		sscanf(ms_vDefensiveAreaPos2, "%f %f %f", &vPos2.x, &vPos2.y, &vPos2.z))
	{
		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
			if( pEnt && pEnt->GetIsTypePed() )
			{
				CPed* pFocusPed = static_cast<CPed*>(pEnt);

				// Attach to the player if the option is ticked, orientate if the player is in a vehicle
				CPed* pAttach = ms_bDefensiveAreaAttachToPlayer ? FindPlayerPed() : NULL;
				bool bOrientateWithEntity = pAttach && pAttach->GetMyVehicle() && pAttach->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle );

				CDefensiveArea* pDefensiveArea = !ms_bUseSecondaryDefensiveArea ?
					pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea() :
					pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetSecondaryDefensiveArea(true);
				pDefensiveArea->Set(vPos1, vPos2, ms_fDefensiveAreaWidth, pAttach, bOrientateWithEntity );
			}
		}
	}
}

void CCombatDebug::UpdateRadiusForFocusPedCB()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			CDefensiveArea* pDefensiveArea = !ms_bUseSecondaryDefensiveArea ?
				pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea() :
				pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetSecondaryDefensiveArea(true);
			if( pDefensiveArea->IsActive() )
			{
				pDefensiveArea->SetWidth(ms_fDefensiveAreaWidth);
			}
		}
	}
}

void CCombatDebug::RenderPreferredCoverDebugForPed(const CPed& ped)
{
	s32 iPreferredCoverGuid = ped.GetPedIntelligence()->GetCombatBehaviour().GetPreferredCoverGuid();
	if (iPreferredCoverGuid >= 0)
	{
		CItemSet* pItemSet = CTheScripts::GetItemSetToModifyFromGUID(iPreferredCoverGuid);
		if (pItemSet)
		{
			for (s32 i=0; i<pItemSet->GetSetSize(); i++)
			{
				const fwExtensibleBase* pExtBase = pItemSet->GetEntity(i);

				if(aiVerifyf(pExtBase && pExtBase->GetIsClassId(CScriptedCoverPoint::GetStaticClassId()), "Not a preferred coverpoint"))
				{
					const CScriptedCoverPoint* pPreferredCoverPoint = static_cast<const CScriptedCoverPoint*>(pExtBase);
					if (pPreferredCoverPoint)
					{
						CCoverPoint* pCoverPoint = CCover::FindCoverPointWithIndex(pPreferredCoverPoint->GetScriptIndex());
						Vector3 vCoverPosition;
						pCoverPoint->GetCoverPointPosition(vCoverPosition);
						grcDebugDraw::Sphere(vCoverPosition, 0.025f, Color_red);
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
// Print out the focus ped's defensive area details
//-------------------------------------------------------------------------
void CCombatDebug::PrintDefensiveAreaCB()
{
	if( CDebugScene::FocusEntities_Get(0) &&
		CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
	{
		CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));
		if( pFocusPed && pFocusPed->GetPedIntelligence()->GetPrimaryDefensiveArea()->IsActive() )
		{
			Vector3 vVec1, vVec2;
			float fWidth;
			if( pFocusPed->GetPedIntelligence()->GetPrimaryDefensiveArea()->GetSetupVectors( vVec1, vVec2, fWidth ) )
			{
				Displayf( "\n\n----FOCUS PEDS DEFENSIVE AREA INFO -------------\n" );
				Displayf( "Vector1 ( %2.5f, %2.5f, %2.5f )\n", vVec1.x, vVec1.y, vVec1.z );
				Displayf( "Vector2 ( %2.5f, %2.5f, %2.5f )\n", vVec2.x, vVec2.y, vVec2.z );
				Displayf( "Width (%2.5f )\n", fWidth );
				Displayf( "---- END FOCUS PEDS DEFENSIVE AREA INFO -------------\n\n\n" );
				if( pFocusPed->GetPedIntelligence()->GetPrimaryDefensiveArea()->GetType() == CArea::AT_Sphere )
				{
					Displayf( "SET_PED_SPHERE_DEFENSIVE_AREA( <PED>, <<%2.5f, %2.5f, %2.5f>>, %2.5f )\n", vVec1.x, vVec1.y, vVec1.z, fWidth  );
				}
			}
		}
	}
}

#endif // __BANK

// Average time between periodic state searches
static const float AVERAGE_STATE_SEARCH_TIME = 2.0f;

static COnFootArmedTransitions s_OnFootArmedTransitions;
static COnFootArmedCoverOnlyTransitions s_OnFootArmedCoverOnlyTransitions;
static COnFootUnarmedTransitions s_OnFootUnarmedTransitions;
static CUnderwaterArmedTransitions s_UnderwaterArmedTransitions;

// Armed and Unarmed transition sets
aiTaskStateTransitionTable*	CTaskCombat::ms_combatTransitionTables[CCombatBehaviour::CT_NumTypes] = 
{
	&s_OnFootArmedTransitions,
	&s_OnFootArmedCoverOnlyTransitions,
	&s_OnFootUnarmedTransitions,
	&s_UnderwaterArmedTransitions
};

// Response ability scales
float CTaskCombat::ms_ReactionScale[CCombatData::CA_NumTypes] = { 5.0f, 2.5f, 1.0f };

static float COVER_MOVMENT_THRESHOLD = 0.2f;
static float COVER_MOVEMENT_MAX_TIME = 10.0f;
static float COVER_STATIONARY_TIME = 2.0f;

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskCombat, 0x29503dd8)
CTaskCombat::Tunables CTaskCombat::ms_Tunables;

// This is actually initialized in the CTaskCombat::InitClass but there is no default constructor so a number was needed
CTaskTimer CTaskCombat::ms_moveToCoverTimerGlobal(15.0f);
CTaskTimer CTaskCombat::ms_allowFrustrationTimerGlobal(15.0f);
float CTaskCombat::ms_fTimeUntilNextGestureGlobal = 0.0f;
u32 CTaskCombat::ms_iTimeLastAsyncProbeSubmitted = 0;
u32 CTaskCombat::ms_uTimeLastDragWasActive = 0;
u8 CTaskCombat::ms_uNumDriversEnteringVehiclesEarly = 0;
u8 CTaskCombat::ms_uNumPedsChasingOnFoot = 0;
bool CTaskCombat::ms_bGestureProbedThisFrame = false;
bool CTaskCombat::ms_bEnableStrafeToRunBreakup = true;
RegdPed CTaskCombat::ms_pFrustratedPed;

#if __BANK
// Debugging class to help tune enemy accuracy scaling
CTaskCombat::CEnemyAccuracyScalingLog CTaskCombat::ms_EnemyAccuracyLog;
#endif // __BANK

// Ambient animation clips
const fwMvClipSetId	CTaskCombat::ms_BeckonPistolClipSetId("combat_gestures_beckon_pistol",0xF218401C);
const fwMvClipSetId	CTaskCombat::ms_BeckonPistolGangClipSetId("combat_gestures_beckon_pistol_gang",0xAC4C4A32);
const fwMvClipSetId	CTaskCombat::ms_BeckonRifleClipSetId("combat_gestures_beckon_rifle",0x8BC9D259);
const fwMvClipSetId	CTaskCombat::ms_BeckonSmgGangClipSetId("combat_gestures_beckon_smg_gang",0x1A6AD309);
const fwMvClipSetId CTaskCombat::ms_GlancesPistolClipSetId("combat_gestures_glances_pistol",0x9D44477);
const fwMvClipSetId CTaskCombat::ms_GlancesPistolGangClipSetId("combat_gestures_glances_pistol_gang",0x3B757E8F);
const fwMvClipSetId CTaskCombat::ms_GlancesRifleClipSetId("combat_gestures_glances_rifle",0x6C489A23);
const fwMvClipSetId CTaskCombat::ms_GlancesSmgGangClipSetId("combat_gestures_glances_smg_gang",0x626D28A7);
const fwMvClipSetId	CTaskCombat::ms_HaltPistolClipSetId("combat_gestures_halt_pistol",0x635F89FD);
const fwMvClipSetId	CTaskCombat::ms_HaltPistolGangClipSetId("combat_gestures_halt_pistol_gang",0xEB32C6E9);
const fwMvClipSetId	CTaskCombat::ms_HaltRifleClipSetId("combat_gestures_halt_rifle",0x574AAD50);
const fwMvClipSetId	CTaskCombat::ms_HaltSmgGangClipSetId("combat_gestures_halt_smg_gang",0x3F2E58A7);
const fwMvClipSetId	CTaskCombat::ms_OverTherePistolClipSetId("combat_gestures_overthere_pistol",0xDD39C79A);
const fwMvClipSetId	CTaskCombat::ms_OverTherePistolGangClipSetId("combat_gestures_overthere_pistol_gang",0x55FEDB5C);
const fwMvClipSetId	CTaskCombat::ms_OverThereRifleClipSetId("combat_gestures_overthere_rifle",0xDDCE7A71);
const fwMvClipSetId	CTaskCombat::ms_OverThereSmgGangClipSetId("combat_gestures_overthere_smg_gang",0x65AB0234);
const fwMvClipSetId	CTaskCombat::ms_SwatGetsureClipSetId("swat_gestures",0x5ED0B752);

//////////////////////////////////////////////////////////////////////////
// CClonedCombatTaskInfo
//////////////////////////////////////////////////////////////////////////

CTask*	CClonedCombatTaskInfo::CreateLocalTask(fwEntity* BANK_ONLY(pEntity))
{
	u32 uFlags = !m_bIsWaitingAtRoadBlock ? 0 : CTaskCombat::ComF_IsWaitingAtRoadBlock;
	if (m_bPreventChangingTarget)
	{
		uFlags |= CTaskCombat::ComF_PreventChangingTarget;
	}

	CTaskCombat* pTask = rage_new CTaskCombat(m_target.GetPed(), 0.0f, uFlags);

#if __BANK
		AI_LOG_WITH_ARGS("CClonedCombatTaskInfo::CreateLocalTask() - ComF_PreventChangingTarget set, ped = %s, task = %p, m_bPreventChangingTarget = %s \n", pEntity ? pEntity->GetDebugName() : "Null", pTask, m_bPreventChangingTarget ? "True" : "False");
#endif

	return pTask;
}

CTaskFSMClone *CClonedCombatTaskInfo::CreateCloneFSMTask()
{
	u32 uFlags = !m_bIsWaitingAtRoadBlock ? 0 : CTaskCombat::ComF_IsWaitingAtRoadBlock;
	if (m_bPreventChangingTarget)
	{
		uFlags |= CTaskCombat::ComF_PreventChangingTarget;
	}

	CTaskCombat* pTask  = rage_new CTaskCombat(m_target.GetPed(), 0.0f, uFlags);

#if __BANK
	AI_LOG_WITH_ARGS("CClonedCombatTaskInfo::CreateCloneFSMTask() - ComF_PreventChangingTarget set, task = %p, m_bPreventChangingTarget = %s \n", pTask, m_bPreventChangingTarget ? "True" : "False");
#endif

    return pTask;
}

bool CTaskCombat::SpheresOfInfluenceBuilder( TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed )
{
	const CTaskCombat* pCombatTask = static_cast<const CTaskCombat*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if( pCombatTask && pCombatTask->GetPrimaryTarget() )
	{
		apSpheres[0].Init(VEC3V_TO_VECTOR3(pCombatTask->GetPrimaryTarget()->GetTransform().GetPosition()), CTaskCombat::ms_Tunables.m_TargetInfluenceSphereRadius, 1.0f*INFLUENCE_SPHERE_MULTIPLIER, 0.5f*INFLUENCE_SPHERE_MULTIPLIER);
		iNumSpheres = 1;
		return true;
	}
	return false;
}	

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CTaskCombat::CTaskCombat( const CPed* pPrimaryTarget, float UNUSED_PARAM(fTimeToQuit), const fwFlags32 uFlags)
: m_TransitionFlagDistributer(CExpensiveProcess::PPT_CombatTransitionDistributer)
, m_pPrimaryTarget(pPrimaryTarget)
, m_pSecondaryTarget(NULL)
, m_fLastCommunicationTimer(-1.0f)
, m_fLastCommunicationBlockingLosTimer(-1.0f)
, m_fCoverMovementTimer(COVER_MOVEMENT_MAX_TIME)
, m_fSecondaryTargetTimer(ms_Tunables.m_fTimeBetweenSecondaryTargetUsesMax)
, m_fLastCombatDirectorUpdateTimer(0.0f)
, m_fTimeWaitingForClearEntry(-1.0f)
, m_fTimeUntilNextAltCoverSearch(ms_Tunables.m_fTimeBetweenAltCoverSearches)
, m_fTimeSinceLastQuickGlance(0.0f)
, m_fLastQuickGlanceHeadingDelta(0.0f)
, m_pInjuredPedToHelp(NULL)
, m_pBuddyShot(NULL)
, m_pTacticalAnalysis(NULL)
, m_pCoverFinder(NULL)
, m_uFlags(uFlags | ComF_IsInDefensiveArea)
, m_DesiredAmbientClipSetId(CLIP_SET_ID_INVALID)
, m_DesiredAmbientClipId(CLIP_ID_INVALID)
, m_fInjuredTargetTimer(0.0f)
, m_fTimeSinceWeLastHadCover(0.0f)
, m_iDesiredState(-1)
, m_uTimeTargetVehicleLastMoved(0)
, m_uRunningFlags(0)
, m_uConfigFlagsForVehiclePursuit(0)
, m_uTimeOfLastChargeTargetCheck(0)
, m_uTimeWeLastCharged(0)
, m_uTimeOfLastSmokeThrowCheck(0)
, m_uTimeWeLastThrewSmokeGrenade(0)
, m_nReasonToHoldFire(RTHF_None)
, m_pVehicleToUseForChase(NULL)
, m_uCachedWeaponHash(0)
, m_uTimeToReactToExplosion(0)
, m_uLastFrameCheckedStateTransitions(0)
, m_buddyShotReactSubTaskLaunched(false)
, m_bPersueVehicleTargetOutsideDefensiveArea(false)
, m_fExplosionX(0.0f)
, m_fExplosionY(0.0f)
, m_fExplosionRadius(0.0f)
, m_fTimeChaseOnFootDelayed(0.0f)
, m_fTimeUntilForcedFireAllowed(ms_Tunables.m_TimeBeforeInitialForcedFire)
, m_uTimeOfLastMeleeAttack(0)
, m_fTimeJacking(0.0f)
, m_uTimeToStopHoldingFire(0)
, m_uTimeOfLastJackAttempt(0)
, m_uMigrationFlags(0)
, m_uMigrationCoverSetup(0)
, m_vInitialPosition(V_ZERO)
, m_fInitialHeading(0.0f)
{
	//Register the slots.
	m_TransitionFlagDistributer.RegisterSlot();

	// Init all of our timers
	m_fTimeUntilAllyProximityCheck = ms_Tunables.m_fTimeBetweenAllyProximityChecks * fwRandom::GetRandomNumberInRange(0.9f, 1.1f);
	m_fTimeUntilAmbientAnimAllowed = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenPassiveAnimsMin, ms_Tunables.m_fTimeBetweenPassiveAnimsMax);
	m_fQuickGlanceTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenQuickGlancesMin, ms_Tunables.m_fTimeBetweenQuickGlancesMax);
	m_fGestureTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenGestureAnimsMin, ms_Tunables.m_fTimeBetweenGestureAnimsMax);
	m_coverSearchTimer.Reset(0.0f, ms_Tunables.m_fTimeBetweenCoverSearchesMin);
	m_fInjuredTargetTimerModifier = fwRandom::GetRandomNumberInRange(-ms_Tunables.m_MaxInjuredTargetTimerVariation, ms_Tunables.m_MaxInjuredTargetTimerVariation);
	ResetRetreatTimer();
	ResetLostTargetTimer();
	ResetGoToAreaTimer();
	ResetJackingTimer();
	ResetMoveToCoverTimer();

	SetInternalTaskType(CTaskTypes::TASK_COMBAT);

#if __BANK
	LogTargetInformation();
#endif
}


//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CTaskCombat::~CTaskCombat()
{}

void CTaskCombat::CleanUp( )
{
	CPed* pPed = GetPed();
	
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UsingCoverPoint, false );

	if (!pPed->GetPedIntelligence()->ShouldIgnoreLowPriShockingEvents())
	{
		// Peds who have been in combat no longer care about minor shocking events, see B*1746585.
		pPed->GetPedIntelligence()->StartIgnoreLowPriShockingEvents();
	}
	
	// Remove ourselves to the combat task manager
	CCombatManager::GetCombatTaskManager()->RemovePed(pPed);

	//Release the combat director.
	ReleaseCombatDirector();

	//Release the tactical analysis.
	ReleaseTacticalAnalysis();

	// Release the equipped weapon anims
	CEquipWeaponHelper::ReleaseEquippedWeaponAnims( *pPed );
	m_uCachedWeaponHash = 0;
	
	//Clear holding fire.
	ClearHoldingFire();
	
	//Clear the firing at target state.
	ClearFiringAtTarget();

	//Clear action mode.
	SetUsingActionMode(false);
	
	// Clear the early vehicle entry
	ClearEarlyVehicleEntry();

	//Clear the frustrated ped, if necessary.
	if(CTaskCombat::ms_pFrustratedPed == pPed)
	{
		CTaskCombat::ms_pFrustratedPed = NULL;
	}
}

void CTaskCombat::DoAbort(const AbortPriority /*iPriority*/, const aiEvent* pEvent)
{
	if(pEvent && pEvent->GetEventType() == EVENT_STATIC_COUNT_REACHED_MAX)
	{
		m_uFlags.ChangeFlag(ComF_TaskAbortedForStaticMovement, true);
	}
}

#if !__FINAL

const char * CTaskCombat::GetStaticStateName( s32 iState )
{
	taskAssertf(iState >= State_Start && iState <= State_Finish,
		"iState fell outside expected valid range. iState value was %d", 
		iState);
	if ( iState == State_Invalid )
	{
		return "State_Invalid";
	}
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Resume",
		"State_Fire",
		"State_InCover",
		"State_MoveToCover",
		"State_Retreat",
		"State_Advance",
		"State_AdvanceFrustrated",
		"State_ChaseOnFoot",
		"State_ChargeTarget",
		"State_Flank",
		"State_MeleeAttack",
		"State_MoveWithinAttackWindow",
		"State_MoveWithinDefensiveArea",
		"State_PersueInVehicle",
		"State_WaitingForEntryPointToBeClear",
		"State_GoToEntryPoint",
		"State_PullTargetFromVehicle",
		"State_EnterVehicle",
		"State_Search",
		"State_DecideTargetLossResponse",
		"State_DragInjuredToSafety",
		"State_Mounted",
		"State_MeleeToArmed",
		"State_ReactToImminentExplosion",
		"State_ReactToExplosion",
		"State_ReactToBuddyShot",
		"State_GetOutOfWater",
		"State_WaitingForCoverExit",
		"State_AimIntro",
		"State_TargetUnreachable",
		"State_ArrestTarget",
		"State_MoveToTacticalPoint",
		"State_ThrowSmokeGrenade",
		"State_Victory",
		"State_ReturnToInitialPosition",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskCombat::UpdateGunTarget(bool bRestartCurrentState)
{
	// Keep the peds target up to date
	CTaskCombatAdditionalTask* pAdditionalCombatTask = static_cast<CTaskCombatAdditionalTask*>(FindSubTaskOfType(CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK));
	if(pAdditionalCombatTask)
	{
		if(pAdditionalCombatTask->GetTarget() != m_pPrimaryTarget)
		{
			if(bRestartCurrentState)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
			else
			{
				pAdditionalCombatTask->SetTargetPed(m_pPrimaryTarget);
			}
		}
	}
	else
	{
		CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
		if (pGunTask && pGunTask->GetTarget().GetEntity() != m_pPrimaryTarget)
		{
			if(bRestartCurrentState)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
			else
			{
				// Gun tasks take CAITarget/CWeaponTarget so create that then pass it in
				CAITarget newTarget(m_pPrimaryTarget);
				pGunTask->SetTarget(newTarget);
			}
		}
	}
}

void CTaskCombat::UpdateCommunication( CPed* pPed )
{	
	m_fLastCommunicationBlockingLosTimer -= GetTimeStep();
	if(m_fLastCommunicationBlockingLosTimer < 0.0f)
	{
		CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( true );
		if( pPedTargetting )
		{
			LosStatus losStatus = pPedTargetting->GetLosStatus( (const CEntity*) m_pPrimaryTarget, false );
			if( losStatus == Los_blockedByFriendly )
			{
				CSingleTargetInfo* pTargetInfo = pPedTargetting->FindTarget(m_pPrimaryTarget);
				if(pTargetInfo)
				{
					CPed* pFriendly = pTargetInfo->GetBlockedByFriendlyPed();
					if(pFriendly)
					{
						CEventShoutBlockingLos event(pPed, pFriendly);
						pFriendly->GetPedIntelligence()->AddEvent(event);
						
						m_fLastCommunicationBlockingLosTimer = ms_Tunables.m_fShoutBlockingLosInterval * fwRandom::GetRandomNumberInRange( 0.9f, 1.1f);
					}
				}
			}
		}
	}

	// Don't shout target until our start/intro states are done
	if(GetState() == State_AimIntro || GetState() == State_Start || GetState() == State_Victory)
	{
		return;
	}

	if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableShoutTargetPosition))
	{
		return;
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontShoutTargetPosition))
	{
		return;
	}

	m_fLastCommunicationTimer -= GetTimeStep();

	if (m_fLastCommunicationTimer < 0.0f)
	{
		//Reset the timer.
		m_fLastCommunicationTimer = ms_Tunables.m_fShoutTargetPositionInterval * fwRandom::GetRandomNumberInRange( 0.9f, 1.1f);

		//In certain cases, we don't want to shout the target position.
		static float s_fMinTimeSinceOurPositionWasLastShouted = 2.5f;

		if( m_pPrimaryTarget->PopTypeIsRandom() &&
			(!m_pPrimaryTarget->GetWeaponManager() || !m_pPrimaryTarget->GetWeaponManager()->GetIsArmed()) )
		{
			//Do not shout.
		}
		else if((m_pPrimaryTarget->GetPedIntelligence()->GetLastTimeOurPositionWasShouted() +
			(u32)(s_fMinTimeSinceOurPositionWasLastShouted * 1000.0f)) > fwTimer::GetTimeInMilliseconds())
		{
			//Do not shout.
		}
		else
		{
			CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( false );
			if( pPedTargetting )
			{
				LosStatus losStatus = pPedTargetting->GetLosStatus( (const CEntity*) m_pPrimaryTarget, false );
				if( losStatus != Los_blocked && losStatus != Los_unchecked && pPedTargetting->AreTargetsWhereaboutsKnown(NULL, m_pPrimaryTarget) )
				{
					//Set the last time our position was shouted.
					m_pPrimaryTarget->GetPedIntelligence()->SetLastTimeOurPositionWasShouted();

					//Create the input.
					CEvent::CommunicateEventInput input;
					input.m_bInformRespectedPedsOnly				= true;
					input.m_bInformRespectedPedsDueToSameVehicle	= true;
					input.m_bInformRespectedPedsDueToScenarios		= true;

					if(pPed->PopTypeIsMission())
					{
						input.m_fMaxRangeWithoutRadio = pPed->GetPedIntelligence()->GetMaxInformRespectedFriendDistance(); 
					}

					//Shout the target position.
					CEventShoutTargetPosition event(const_cast<CPed*>(pPed), const_cast<CPed*>(m_pPrimaryTarget.Get()));
					event.CEvent::CommunicateEvent(pPed, input);
				}
			}
		}
	}
}

CTask::FSM_Return CTaskCombat::ProcessPreFSM()
{
	PF_FUNC(CTaskCombat);

	CPed *pPed = GetPed(); //Get the ped ptr.

	// Update our current primary target as needed
	if( !UpdatePrimaryTarget(pPed) )
	{
		// Logs to try to catch B*2177569/B*2175380
#if __BANK
		CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. Primary target could not be updated.\n", 
			AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

		return FSM_Quit;
	}

	if (!m_pPrimaryTarget)
	{
		// Logs to try to catch B*2177569/B*2175380
#if __BANK
		CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. We do no longer have a primary target.\n", 
			AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

		return FSM_Quit;
	}

	if( pPed->GetVehiclePedInside() && GetParent() == NULL && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_AlwaysFlee) )
	{
		// Logs to try to catch B*2177569/B*2175380
#if __BANK
		CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. No threat response parent and is told to always flee.\n", 
			AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

		aiTask* pTaskToGive = rage_new CTaskThreatResponse(m_pPrimaryTarget);
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTaskToGive); 
		pPed->GetPedIntelligence()->AddEvent(event);

		return FSM_Quit;
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsInCombat, true);

	//Disable injured movement.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured, false);

	//Process weapon anim streaming.
	ProcessWeaponAnims();

	//Process road block audio.
	ProcessRoadBlockAudio();

	if(pPed->IsNetworkClone())
	{
		return ProcessPreFSM_Clone();
	}

	//Process the potential blast.
	ProcessPotentialBlast();
	
	//Process the aim pose.
	ProcessAimPose();
	
	//Process the facial idle clip.
	ProcessFacialIdleClip();

	//Process actions for when the defensive area is reached.
	ProcessActionsForDefensiveArea();

	//Process the movement type.
	ProcessMovementType();

	//Process the movement clip set.
	ProcessMovementClipSet();

	// Cache the ped intelligence
	CPedIntelligence* pPedIntell = pPed->GetPedIntelligence();

	// Cops stop chasing the player when wanted level is cleared 
	if( pPed->ShouldBehaveLikeLaw() && m_pPrimaryTarget->IsPlayer() )
	{
		CWanted* pPlayerWanted = m_pPrimaryTarget->GetPlayerWanted();
		if( pPlayerWanted )
		{
			if( pPlayerWanted->GetWantedLevel() == WANTED_CLEAN )
			{
				if( pPed->PopTypeIsRandom() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanAttackNonWantedPlayerAsLaw) && !pPlayerWanted->m_LawPedCanAttackNonWantedPlayer && !pPed->IsExitingVehicle() )
				{
					// The player target won't be removed from the target list anymore (due to not shutting down targeting on abort)
					// so in this case we actually need to reset the target index to make sure the cop doesn't consider the player a target
					CPedTargetting* pPedTargetting = pPedIntell->GetTargetting( false );
					if( pPedTargetting )
					{
						pPedTargetting->RemoveThreat(m_pPrimaryTarget);
					}

					// Logs to try to catch B*2177569/B*2175380
#if __BANK
					CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. The target %s no longer has a wanted level.\n", 
						AILogging::GetEntityDescription(GetPed()).c_str(), AILogging::GetEntityDescription(m_pPrimaryTarget).c_str());
#endif // __BANK

					return FSM_Quit;
				}
			}
		}
	}

	// Not ideal but we need to know if we aren't supposed to attack law when the player isn't wanted
	if( !pPedIntell->CanAttackPed(m_pPrimaryTarget) )
	{
		// Logs to try to catch B*2177569/B*2175380
#if __BANK
		CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. CanAttackPed returned false for the target %s.\n", 
			AILogging::GetEntityDescription(GetPed()).c_str(), AILogging::GetEntityDescription(m_pPrimaryTarget).c_str());
#endif // __BANK

		return FSM_Quit;
	}

	if(pPedIntell->IsFriendlyWith(*m_pPrimaryTarget))
	{
		// Logs to try to catch B*2177569/B*2175380
#if __BANK
		CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. Target %s is friendly with ped.\n", 
			AILogging::GetEntityDescription(GetPed()).c_str(), AILogging::GetEntityDescription(m_pPrimaryTarget).c_str());
#endif // __BANK

		return FSM_Quit;
	}

	// We've lost the target for a period of time, quit
	if (m_uFlags.IsFlagSet(ComF_TargetLost))
	{
		if (m_lostTargetTimer.Tick(GetTimeStep()))
		{
			CCombatBehaviour& combatBehaviour = pPedIntell->GetCombatBehaviour();
			if (combatBehaviour.IsFlagSet(CCombatData::BF_CanInvestigate))
			{
				CEventReactionInvestigateThreat reactEvent(CAITarget(m_pPrimaryTarget),0,CEventReactionInvestigateThreat::LOW_THREAT);
				pPedIntell->AddEvent(reactEvent);
			}

			// Logs to try to catch B*2177569/B*2175380
#if __BANK
			CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. Target %s lost.\n", 
				AILogging::GetEntityDescription(GetPed()).c_str(), AILogging::GetEntityDescription(m_pPrimaryTarget).c_str());
#endif // __BANK

			return FSM_Quit;
		}
	}
	else
	{
		ResetLostTargetTimer();
	}

	m_jackingTimer.Tick(GetTimeStep());

	m_goToAreaTimer.Tick(GetTimeStep());

	m_moveToCoverTimer.Tick(GetTimeStep());

	// If we can be frustrated then update our async probe for testing LOS to the targets lock on position
	if(CanBeFrustrated(pPed))
	{
		if(CTaskCombat::ms_pFrustratedPed != pPed)
		{
			UpdateAsyncProbe(pPed);
		}
	}
	else if(CTaskCombat::ms_pFrustratedPed == pPed)
	{
		// Otherwise if we can't get frustrated but we were planning on doing the advance reset the ped
		CTaskCombat::ms_pFrustratedPed = NULL;
	}

	float fCurrTimeStep = GetTimeStep();

	// Increase some timers
	m_fTimeSinceLastQuickGlance += fCurrTimeStep;

	// Decrement our quick glance and overall ambient anim timers
	m_fQuickGlanceTimer -= fCurrTimeStep;
	m_fGestureTimer -= fCurrTimeStep;
	m_fTimeUntilAmbientAnimAllowed -= fCurrTimeStep;
	m_fTimeUntilForcedFireAllowed -= fCurrTimeStep;

	if((GetState() == State_GoToEntryPoint) || (GetState() == State_PullTargetFromVehicle))
	{
		m_fTimeJacking += fCurrTimeStep;
	}
	else
	{
		m_fTimeJacking = 0.0f;
	}

	// Don't attack injured peds, consider them as no longer threats
	// If there were any non-injured targets, they would have been switched above.
	if( m_pPrimaryTarget->IsInjured() && !pPed->WillAttackInjuredPeds() )
	{
		m_fInjuredTargetTimer += fCurrTimeStep;

		// Get our time to invalidate injured targets and then multiply it by our small random number to get a slight variation
		float fTimeForInjuredTargetToBeValid = pPedIntell->GetCombatBehaviour().GetCombatFloat(kAttribFloatTimeToInvalidateInjuredTarget);
		fTimeForInjuredTargetToBeValid += m_fInjuredTargetTimerModifier;

		if( m_fInjuredTargetTimer >= fTimeForInjuredTargetToBeValid && CanUpdatePrimaryTarget() )
		{
			// We want peds to aim at the injured player ped indefinitely in SP if there are no other peds (unless they've been explicitly told not to)
			bool bIsTargetPlayerInSP = m_pPrimaryTarget->IsAPlayerPed() && !NetworkInterface::IsGameInProgress();
			if( !bIsTargetPlayerInSP || fTimeForInjuredTargetToBeValid <= 0.0f )
			{
				// If we're currently in cover or trying to exit cover, continue trying to exit
				if(GetState() == State_InCover || GetState() == State_WaitingForCoverExit)
				{
					m_uFlags.SetFlag(ComF_ExitCombatRequested);
				}
				else
				{
					CTaskMelee* pMeleeTask = GetState() == State_MeleeAttack ? static_cast<CTaskMelee*>(FindSubTaskOfType(CTaskTypes::TASK_MELEE)) : NULL;
					if(pMeleeTask && pMeleeTask->IsCurrentlyDoingAMove())
					{
						pMeleeTask->SetTaskFlag(CTaskMelee::MF_QuitTaskAfterNextAction);
					}
					else
					{
						// Clear the target.
						ChangeTarget(NULL);		

#if __BANK
						CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. Target %s is injured and we aren't allowed to attack injured peds.\n", 
							AILogging::GetEntityDescription(GetPed()).c_str(), AILogging::GetEntityDescription(m_pPrimaryTarget).c_str());
#endif // __BANK

						return FSM_Quit;
					}
				}
			}
		}
	}
	else
	{
		m_fInjuredTargetTimer = 0.0f;
	}

	// Check if the target's vehicle (if the target has one) is going too fast to allow them to be jacked
	CVehicle* pTargetVehicle = m_pPrimaryTarget->GetVehiclePedInside();
	if( pTargetVehicle && !IsVehicleMovingSlowerThanSpeed(*pTargetVehicle, ms_Tunables.m_MaxSpeedToStartJackingVehicle) )
	{
		m_uTimeTargetVehicleLastMoved = fwTimer::GetTimeInMilliseconds();
	}

	// Make sure this ped doesn't activate ragdoll from player impacts
	// NOTE: Melee handles this independently of combat
	if( !pPed->GetPedIntelligence() || !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE ) )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset, true );
	}

	// If this ped will use cover, set the reset flag to force them to load cover around them
	if (pPedIntell->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover) )
		pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForCover, true );

	// If part of this task has reserved a cover point make sure it is marked as in use.
	// Also update our cover finder
	UpdateCoverStatus(pPed, true);
	UpdateCoverFinder(pPed);

	// Make sure we keep any desired cover point
	pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepDesiredCoverPoint, true );

	// update communication to other friends in the area
	UpdateCommunication(pPed);

	// Update our information about being in our defensive area and if we should use an alternate area
	ProcessDefensiveArea();

	// Make this ped randomise their pathfinding points slightly, to reduce chances of multiple peds taking identical routes
	pPed->SetPedResetFlag( CPED_RESET_FLAG_RandomisePointsDuringNavigation, true );

	// If within a close distance of the target, fire much faster:
	if(pPedIntell->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseProximityFiringRate))
	{
		float fDistanceToTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist(VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
		if( fDistanceToTarget < ms_Tunables.m_fFireContinuouslyDistMax )
		{
			float fShootRateModifier = Clamp((fDistanceToTarget - ms_Tunables.m_fFireContinuouslyDistMin) / (ms_Tunables.m_fFireContinuouslyDistMax - ms_Tunables.m_fFireContinuouslyDistMin), SMALL_FLOAT, 1.0f);
			fShootRateModifier = 1.0f / fShootRateModifier;
			if( pPedIntell->GetCombatBehaviour().GetShootRateModifier() < fShootRateModifier )
			{
				pPedIntell->GetCombatBehaviour().SetShootRateModifierThisFrame(fShootRateModifier);
			}
		}
	}
	
	// If hurt, reduce shoot rate
	if(pPed->HasHurtStarted())
	{
		static dev_float HURT_FIRE_RATE_MOD = 0.33f;
		float fShootRateModifier = pPed->GetPedIntelligence()->GetCombatBehaviour().GetShootRateModifier() * HURT_FIRE_RATE_MOD;
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetShootRateModifierThisFrame(fShootRateModifier);
	}
	
	//Process the combat director.
	ProcessCombatDirector();
	
	//Process the combat orders.
	ProcessCombatOrders();
	
	//Process the explosion.
	ProcessExplosion();

	// If the ped is in a group, make sure they fall back to the follow task if the leader gets too far away
	CPedGroup* pMyGroup = pPed->GetPedsGroup();
	if( pMyGroup )
	{
		CPed* pLeader = pMyGroup->GetGroupMembership()->GetLeader();
		if( pLeader && pPed != pLeader && pLeader->IsAPlayerPed() )
		{
			bool inVehicle = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle);
			if( (!inVehicle && pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() &&
				!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) )
				|| (inVehicle && !pMyGroup->GetGroupMembership()->GetLeader()->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)) )
			{
				// Logs to try to catch B*2177569/B*2175380
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat to follow leader.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				return FSM_Quit;
			}

			if( pPed->GetIsInWater() )
			{
				// Logs to try to catch B*2177569/B*2175380
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat to follow leader.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				return FSM_Quit;
			}
		}
	}

	// if we require an order then make sure we have a valid one
	if(m_uFlags.IsFlagSet(ComF_RequiresOrder))
	{
		const COrder* pOrder = pPedIntell->GetOrder();
		if(!pOrder || !pOrder->GetIsValid())
		{
			// Get our targeting but don't wake it up. If we don't have one we should quit
			CPedTargetting* pTargeting = pPedIntell->GetTargetting(false);
			if(!pTargeting)
			{
				// Logs to try to catch B*2177569/B*2175380
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. The ped requires order and not PedTargetting was found.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				return FSM_Quit;
			}

			// Get our target info and check LOS status. If no target info or our LOS is blocked we should quit
			CSingleTargetInfo* pTargetInfo = pTargeting->FindTarget(m_pPrimaryTarget);
			if(!pTargetInfo || pTargetInfo->GetLosStatus() == Los_blocked)
			{
				// Logs to try to catch B*2177569/B*2175380
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::ProcessPreFSM, Ped %s, Aborting TaskCombat. The ped requires order and the PrimaryTarget %s was not found or has Los_blocked status on the Targetting.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str(), AILogging::GetEntityDescription(m_pPrimaryTarget).c_str());
#endif // __BANK

				return FSM_Quit;
			}
		}		
	}
	
	//Process whether we are holding fire.
	ProcessHoldFire();
	
	//Process whether we are firing at the target.
	ProcessFiringAtTarget();

	//Process the audio.
	ProcessAudio();

	//Prestream ai cover entry anims
	CTaskEnterCover::PrestreamAiCoverEntryAnims(*pPed);

	// Need to set this here as the (current) cover subtask might not run if one of it's parent tasks changes states
	CTaskCover* pCoverTask = static_cast<CTaskCover*>(FindSubTaskOfType(CTaskTypes::TASK_COVER));
	if (pCoverTask && pCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft, true);
	}

	//Note that we are in combat.
	CGameSituation::GetInstance().GetFlags().SetFlag(CGameSituation::Combat);

	//Look at the target.
	if(GetState() != State_ReturnToInitialPosition && GetState() != State_Search)
	{
		pPed->GetIkManager().LookAt(0, m_pPrimaryTarget, 2000, BONETAG_HEAD, NULL, LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskCombat::ProcessPreFSM_Clone()
{
	//Prestream ai cover entry anims
	CTaskEnterCover::PrestreamAiCoverEntryAnims(*GetPed());

	// update communication to other friends in the area
	UpdateCommunication(GetPed());

	return FSM_Continue;
}

#define GET_IN_VEHICLE_AT_TARGET_SPEED_SQ 25.0f
#define STOP_CAR_JACK_TARGET_SPEED_SQ 36.0f
#define CAR_JACK_DISTANCE_START 50.0f
#define CAR_JACK_DISTANCE_STOP 60.0f
#define ARMED_MELEE_ATTACK_DISTANCE_SQ 2.0f 
#define MAX_DIST_TO_SHOOT_WHILE_CHASING 25.0f
#define STOP_CHASING_DIST_TARGET_STILL 17.5f
#define STOP_CHASING_DIST_TARGET_MOVING 12.5f
#define STOP_CHASING_DIST_TARGET_NOT_SEEN 5.0f
#define START_CHASING_DIST 15.0f
#define MAX_DISTANCE_FOR_IN_COVER_STATE .25f

s64 CTaskCombat::GenerateTransitionConditionFlags( bool bStateEnded )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	s64 iConditionFlags = bStateEnded ? CF_StateEnded : 0;
	// Obtain information about the target from the targeting system
	CPedIntelligence* pPedIntelligence =  pPed->GetPedIntelligence();
	CPedTargetting* pTargetting = pPedIntelligence->GetTargetting(true);
	Assert(pTargetting);
	CSingleTargetInfo* pTargetInfo = pTargetting ? pTargetting->FindTarget(m_pPrimaryTarget) : NULL;
	if( pTargetInfo == NULL )
	{
		if (pTargetting)
		{
			pTargetting->RegisterThreat(m_pPrimaryTarget, true, true);
			pTargetInfo = pTargetting->FindTarget(m_pPrimaryTarget);
		}

		if(aiVerify(pTargetInfo))
		{
			// Make sure the target is considered being targeted
			pTargetInfo->SetIsBeingTargeted(true);
		}
	}

	// Only do this stuff if we have a target info
	if(pTargetInfo)
	{
		LosStatus losStatus = pTargetInfo->GetLosStatus();

		//Check if the state does not handle a blocked line of sight.
		if(!StateHandlesBlockedLineOfSight())
		{
			// Build Los conditions
			if( losStatus == Los_blocked )
			{
				iConditionFlags |= CF_LosBlocked;
				iConditionFlags |= CF_LosNotBlockedByFriendly;
			}
			else if( losStatus != Los_blockedByFriendly )
			{
				iConditionFlags |= CF_LosNotBlockedByFriendly;
				iConditionFlags |= CF_LosClear;
			}
			else
				iConditionFlags |= CF_LosBlocked;
		}
	}

	CCombatBehaviour& combatBehaviour = pPedIntelligence->GetCombatBehaviour();
	CCombatData::Movement combatMovement = combatBehaviour.GetCombatMovement();

	bool bIsDefensiveAreaActive = pPedIntelligence->GetDefensiveArea()->IsActive();

	// Will the ped move in combat
	if( combatMovement == CCombatData::CM_WillAdvance )
		iConditionFlags |= CF_WillAdvance;

	// include any combat setup on peds
	if (combatMovement != CCombatData::CM_Stationary)
	{
		if (combatBehaviour.IsFlagSet(CCombatData::BF_CanUseCover))
		{
			bool bWeaponUsableInCover = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->IsEquippedWeaponUsableInCover() : false;
			if (bWeaponUsableInCover)
			{
				iConditionFlags |= CF_WillUseCover;
			}		
		}
	}
	
	//Update the combat flags that rely on the state of nearby peds.
	UpdateFlagsForNearbyPeds(iConditionFlags);

	// Has the ped got a valid coverpoint?
	if( pPed->GetCoverPoint() )
	{
		UpdateCoverStatus(pPed, false);
		
		// See if the ped is considered in cover for state transitions
		if(CanPedBeConsideredInCover(pPed))
		{
			iConditionFlags |= CF_InCover;

			if((fwTimer::GetTimeInMilliseconds() - pPedIntelligence->GetLastPinnedDownTime()) > ms_Tunables.m_SafeTimeBeforeLeavingCover)
			{
				iConditionFlags |= CF_IsSafeToLeaveCover;
			}
		}

		// TODO: add conditions for when the state is ending (i.e. ignore the timers and just check for having desired cover)
		if( HasValidatedDesiredCover() &&
			((GetState() == State_InCover && m_moveToCoverTimer.IsFinished() && ms_moveToCoverTimerGlobal.IsFinished())) )
		{
			iConditionFlags |= CF_HasDesiredCover;
		}
	}
	else
	{
		if( HasValidatedDesiredCover() )
		{
			iConditionFlags |= CF_HasDesiredCover;
		}
	}

	// If we are not in cover then set the proper flag
	if( (iConditionFlags & CF_InCover) == 0 )
	{
		iConditionFlags |= CF_NotInCover;
	}

	// Check if we are the ped marked as the frustrated ped and set our frustrated advance flag
	if( CTaskCombat::ms_pFrustratedPed == pPed )
	{
		iConditionFlags |= CF_WillUseFrustratedAdvance;
	}

	// IS the ped in or outside of a vehicle
	if( pPed->GetVehiclePedInside() )
	{
		iConditionFlags |= CF_InVehicle;
	}
	else if(pPed->GetMyMount())
	{
		iConditionFlags |= CF_Mounted;
	}
	else
	{
		iConditionFlags |= CF_OnFoot;
	}

	// Do the cheapest check first for whether we should ignore attack windows
	bool bPreventUsingAttackWindows = false;
	if(combatMovement == CCombatData::CM_Defensive)
	{
		if(!combatBehaviour.IsFlagSet(CCombatData::BF_EnableTacticalPointsWhenDefensive))
		{
			bPreventUsingAttackWindows = true;
		}
	}
	else if(combatMovement != CCombatData::CM_WillAdvance)
	{
		bPreventUsingAttackWindows = true;
	}

	Vec3V vTargetPosition = m_pPrimaryTarget->GetTransform().GetPosition();
	bool bIsInCoverState = (GetState() == State_MoveToCover) || (GetState() == State_InCover);
	if (bIsDefensiveAreaActive)
	{
		Vector3 vCenter;
		float fDefensiveAreaRadiusSq = 0.0f;
		pPedIntelligence->GetDefensiveArea()->GetCentreAndMaxRadius(vCenter, fDefensiveAreaRadiusSq);

		if (pPedIntelligence->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(vTargetPosition)))
		{
			iConditionFlags |= CF_TargetWithinDefensiveArea;
		}

		if (!m_uFlags.IsFlagSet(ComF_IsInDefensiveArea))
		{
			bPreventUsingAttackWindows = true;
			iConditionFlags |= CF_OutsideDefensiveArea;
		}
		else if(!bPreventUsingAttackWindows)
		{
			if(fDefensiveAreaRadiusSq < square(ms_Tunables.m_MinDefensiveAreaRadiusForWillAdvance))
			{
				bPreventUsingAttackWindows = true;
			}
			else if((iConditionFlags & CF_TargetWithinDefensiveArea) == 0)
			{
				float fAtkWindowMin = 0.0f, fAtkWindowMax = 0.0f;
				CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(pPed, fAtkWindowMin, fAtkWindowMax, bIsInCoverState);
				const ScalarV scTargetToDefensiveAreaSq = DistSquared(VECTOR3_TO_VEC3V(vCenter), vTargetPosition);
				if(IsGreaterThanAll(scTargetToDefensiveAreaSq, LoadScalar32IntoScalarV(fAtkWindowMax * fAtkWindowMax)))
				{
					bPreventUsingAttackWindows = true;
				}
			}
		}
	}
	else
	{
		iConditionFlags |= CF_TargetWithinDefensiveArea;
	}

	// Check to see if the ped is within their attack window
	bool bIsCloserThanAttackWindow;
	if( !bPreventUsingAttackWindows &&
		CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow(pPed, m_pPrimaryTarget, pPed->GetTransform().GetPosition(), bIsInCoverState, bIsCloserThanAttackWindow) )
	{
		iConditionFlags |= CF_OutsideAttackWindow;

		if(combatMovement == CCombatData::CM_Defensive && bIsCloserThanAttackWindow)
		{
			iConditionFlags |= CF_WillUseDefensiveAttackWindow;
		}
	}
	else
	{
		iConditionFlags |= CF_InsideAttackWindow;
	}

	// Calculate the target location status
	if( pTargetting && pTargetting->AreTargetsWhereaboutsKnown(NULL, m_pPrimaryTarget) )
	{
		m_uFlags.ClearFlag(ComF_TargetLost);
		iConditionFlags |= CF_TargetLocationKnown;
	}
	else
	{
		m_uFlags.SetFlag(ComF_TargetLost);
		iConditionFlags |= CF_TargetLocationLost;
	}

	// See if we are allowed to jack/bust the target
	if(CanJackTarget())
	{
		iConditionFlags |= CF_CanJackTarget;
	}

	// Check if we can chase our target using a vehicle
	if(CanChaseInVehicle())
	{
		iConditionFlags |= CF_CanChaseInVehicle;
	}

	// Need the distance from our target for a couple checks
	ScalarV scDistToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), vTargetPosition);

	// Melee attack
	if( HelperCanPerformMeleeAttack() )
	{
		iConditionFlags |= CF_CanPerformMeleeAttack;
	}

	// Should we allow flank behavior?
	if( combatBehaviour.IsFlagSet(CCombatData::BF_CanFlank) )
	{
		// Don't do a flank if we're too close to the target
		ScalarV scMaxDistSq = ScalarVFromF32(square(CTaskCombatFlank::sm_Tunables.m_fInfluenceSphereRequestRadius));
		if(IsGreaterThanAll(scDistToTargetSq, scMaxDistSq))
		{
			iConditionFlags |= CF_CanFlank;
		}
	}

	// Should we allow chase on foot behavior?
	if( !bIsDefensiveAreaActive && ShouldChaseTarget(pPed, m_pPrimaryTarget) )
	{
		m_fTimeChaseOnFootDelayed += GetTimeStep();
		if(GetState() == State_ChaseOnFoot || m_fTimeChaseOnFootDelayed >= ms_Tunables.m_TimeToDelayChaseOnFoot)
		{
			iConditionFlags |= CF_CanChaseOnFoot;
		}
	}
	else
	{
		m_fTimeChaseOnFootDelayed = MAX(m_fTimeChaseOnFootDelayed - GetTimeStep(), 0.0f);
		iConditionFlags |= CF_DisallowChaseOnFoot;
	}
	
	//Check if cover is being blocked.
	if( (!pPed->PopTypeIsMission() && m_CombatOrders.GetPassiveFlags().IsFlagSet(CCombatOrders::PF_BlockCover)) ||
		m_pPrimaryTarget->IsDead() ||
		pPed->HasHurtStarted() ||
		(m_uFlags.IsFlagSet(ComF_MoveToCoverStopped) && GetState() == State_Fire && GetTimeInState() < ms_Tunables.m_FireTimeAfterStoppingMoveToCover) )
	{
		//Clear the cover flags.
		iConditionFlags &= ~(CF_WillUseCover|CF_HasDesiredCover);
	}

	//Check if we can react to an imminent explosion.
	if(CanReactToImminentExplosion())
	{
		iConditionFlags |= CF_CanReactToImminentExplosion;
	}
	
	//Check if we can react to an explosion.
	if(CanReactToExplosion())
	{
		iConditionFlags |= CF_CanReactToExplosion;
	}
	
	//Check if we can move within our defensive area.
	if(CanMoveWithinDefensiveArea())
	{
		iConditionFlags |= CF_CanMoveWithinDefensiveArea;
	}

	//Check if we can arrest the target.
	if(ShouldArrestTarget())
	{
		iConditionFlags |= CF_CanArrestTarget;
	}

	//Check if we can react to a buddy being shot.
	CPed* pBuddyShot = NULL;
	if(CanReactToBuddyShot(&pBuddyShot))
	{
		//Set the buddy shot.
		m_pBuddyShot = pBuddyShot;

		//Set the flag.
		iConditionFlags |= CF_CanReactToBuddyShot;
	}

	//Check if we can move to a tactical point.
	if(CanMoveToTacticalPoint())
	{
		iConditionFlags |= CF_CanMoveToTacticalPoint;
	}
	
	return iConditionFlags;
}

bool CTaskCombat::PeriodicallyCheckForStateTransitions(float fAverageTimeBetweenChecks)
{
	//Ensure we can change state.
	if(ShouldNeverChangeStateUnderAnyCircumstances())
	{
		return false;
	}

	// See if we should early out if the expensive process says we shouldn't process this frame
	if(!m_TransitionFlagDistributer.ShouldBeProcessedThisFrame())
	{
		// If our ped isn't on screen or they are but their target is not a player then we don't need to force an update
		if(!GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen) || !m_pPrimaryTarget->IsAPlayerPed())
		{
			return false;
		}

		// Check if we should update this frame by comparing if we're currently on an even frame to if we should update on even frames
		bool bCheckedLastFrame = ((m_uLastFrameCheckedStateTransitions + 1) == fwTimer::GetFrameCount());
		if(bCheckedLastFrame)
		{
			return false;
		}
	}

	//Set the last frame we checked the state transitions.
	m_uLastFrameCheckedStateTransitions = fwTimer::GetFrameCount();

	return aiTask::PeriodicallyCheckForStateTransitions(fAverageTimeBetweenChecks);
}

#if __BANK
// Begin a new logging test
void CTaskCombat::CEnemyAccuracyScalingLog::BeginLoggingTest()
{
	m_uLoggingTestStartTimeMS = fwTimer::GetTimeInMilliseconds();
	m_iLoggingTestHighestWL = -1;
	m_iLoggingTestHighestNumShooters = 0;
	if( !m_bEnabled )
	{
		m_bEnabled = true;
	}
}

// Main update method
void CTaskCombat::CEnemyAccuracyScalingLog::Update()
{
	// If logging is enabled
	if( m_bEnabled )
	{
		// Get current time
		const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

		// Get history count
		const int iHistCount = m_ShotAccuracyHistory.GetCount();

		// Check if we should log output
		if( m_bHasRecordsToLog && iHistCount > 0 && currentTimeMS > m_uNextOutputTimeMS )
		{
			// Schedule next log time
			m_uNextOutputTimeMS = currentTimeMS + m_uMeasureOutputIntervalMS;

			// Compute what time the history items were last logged
			u32 uLastOutputTimeMS = 0;
			if( currentTimeMS > m_uMeasureOutputIntervalMS )
			{
				uLastOutputTimeMS =  currentTimeMS - m_uMeasureOutputIntervalMS;
			}

			// Dump contents of the history to output
			Displayf("*** ENEMY ACCURACY SCALING HISTORY LOG BEGIN (Time: %d) ***", currentTimeMS);
			Displayf("(Time MS), (PlayerWL), (NumShooters), (AccReduction PCT), (FinalAccuracy PCT), (Min Enemies for scaling), (Accuracy reduction per enemy), (Accuracy floor)");
			for(int i=0; i < iHistCount; i++)
			{
				const CEnemyShotAccuracyMeasurement& LogMeasure = m_ShotAccuracyHistory[i];
				if( LogMeasure.m_uTimeOfShotMS > uLastOutputTimeMS )
				{
					Displayf("ACCURACYLOGITEM %d, %d, %d, %.4f, %.4f, %d, %.4f, %.4f", LogMeasure.m_uTimeOfShotMS, LogMeasure.m_iCurrentPlayerWL, LogMeasure.m_iNumEnemyShooters, LogMeasure.m_fAccuracyReduction, LogMeasure.m_fAccuracyFinal,
						ms_Tunables.m_EnemyAccuracyScaling.m_iMinNumEnemiesForScaling, ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionPerEnemy, ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionFloor);
				}
			}
			Displayf("*** ENEMY ACCURACY SCALING HISTORY LOG END ***");
			m_bHasRecordsToLog = false;
		}
	}

	// If we should render a summary
	if( m_bRenderDebug )
	{
		// Get current time
		const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

		// Get history count
		const int iHistCount = m_ShotAccuracyHistory.GetCount();

		// Sum relevant entries
		int iCounted = 0;
		float fSumNumShooters = 0.0f;
		float fSumAccReduction = 0.0f;
		float fSumAccFinal = 0.0f;
		for(int i=0; i < iHistCount; i++)
		{
			const CEnemyShotAccuracyMeasurement& LogMeasure = m_ShotAccuracyHistory[i];
			if( LogMeasure.m_uTimeOfShotMS + m_uRenderHistoryTimeMS  >= currentTimeMS )
			{
				iCounted++;
				fSumNumShooters += LogMeasure.m_iNumEnemyShooters;
				fSumAccReduction += LogMeasure.m_fAccuracyReduction;
				fSumAccFinal += LogMeasure.m_fAccuracyFinal;
			}
		}

		// Compute averages for display
		float fAvgNumShooters = 0;
		float fAvgAccReduction = 0;
		float fAvgAccFinal = 0;
		if(iCounted > 0)
		{
			fAvgNumShooters = fSumNumShooters / iCounted;
			fAvgAccReduction = fSumAccReduction / iCounted;
			fAvgAccFinal = fSumAccFinal / iCounted;
		}

		// Constant values for the display
		static Vec2V vDebugScreenPosInitial(50.0f,75.0f);
		static s32 iVerticalSpace = 10;

		// String for writing
		const int DEBUG_STRING_SIZE = 128;
		char debugString[DEBUG_STRING_SIZE];

		// Vars for writing multiple lines
		Vec2V vDebugScreenPos = vDebugScreenPosInitial;

		formatf(debugString, DEBUG_STRING_SIZE, "In past %.1f seconds, shooting at local player:", (m_uRenderHistoryTimeMS / 1000.0f));
		grcDebugDraw::Text(vDebugScreenPos, DD_ePCS_Pixels, Color_red, debugString);
		vDebugScreenPos.SetYf(vDebugScreenPos.GetYf() + iVerticalSpace);

		formatf(debugString, DEBUG_STRING_SIZE, "AVG NumShooters: %.2f", fAvgNumShooters);
		grcDebugDraw::Text(vDebugScreenPos, DD_ePCS_Pixels, Color_red, debugString);
		vDebugScreenPos.SetYf(vDebugScreenPos.GetYf() + iVerticalSpace);

		formatf(debugString, DEBUG_STRING_SIZE, "AVG AccReduction: %.2f", fAvgAccReduction);
		grcDebugDraw::Text(vDebugScreenPos, DD_ePCS_Pixels, Color_red, debugString);
		vDebugScreenPos.SetYf(vDebugScreenPos.GetYf() + iVerticalSpace);

		formatf(debugString, DEBUG_STRING_SIZE, "AVG AccFinal: %.2f", fAvgAccFinal);
		grcDebugDraw::Text(vDebugScreenPos, DD_ePCS_Pixels, Color_red, debugString);
	}
}

// Record shot fired occurrence
void CTaskCombat::CEnemyAccuracyScalingLog::RecordEnemyShotFired(int currentPlayerWL, int iNumEnemyShooters, float fNumEnemiesAccuracyReduction, float fShotAccuracyFinal)
{
	// Initialize the measurement to record
	CEnemyShotAccuracyMeasurement newMeasurement(fwTimer::GetTimeInMilliseconds(), currentPlayerWL, iNumEnemyShooters, fNumEnemiesAccuracyReduction, fShotAccuracyFinal);
	
	// If list isn't full yet, append to list
	if( m_ShotAccuracyHistory.GetCount() < m_ShotAccuracyHistory.GetMaxCount() )
	{
		m_ShotAccuracyHistory.Push(newMeasurement);
	}
	// list is full, start overwriting from the front and loop from now on
	else if( AssertVerify(m_iHistoryWriteIndex >= 0) && AssertVerify(m_iHistoryWriteIndex < m_ShotAccuracyHistory.GetMaxCount()) )
	{
		m_ShotAccuracyHistory[m_iHistoryWriteIndex] = newMeasurement;
		m_iHistoryWriteIndex++;
		if( m_iHistoryWriteIndex >= m_ShotAccuracyHistory.GetMaxCount() )
		{
			m_iHistoryWriteIndex = 0;
		}
	}

	// we have new info to log
	m_bHasRecordsToLog = true;

	// Keep track of highest wanted level achieved
	if( currentPlayerWL > m_iLoggingTestHighestWL )
	{
		m_iLoggingTestHighestWL = currentPlayerWL;
	}

	// Keep track of highest number of shooters
	if( iNumEnemyShooters > m_iLoggingTestHighestNumShooters )
	{
		m_iLoggingTestHighestNumShooters = iNumEnemyShooters;
	}
}

// Record local player wanted level cleared
void CTaskCombat::CEnemyAccuracyScalingLog::RecordWantedLevelCleared()
{
	// If logging is enabled
	if( m_bEnabled )
	{
		// Get current time
		const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

		// Compute elapsed test time
		const u32 elapsedTestTimeMS = currentTimeMS - m_uLoggingTestStartTimeMS;

		Displayf("*** ENEMY ACCURACY SCALING HISTORY LOG BEGIN (Time: %d) ***", currentTimeMS);
		Displayf("(Elapsed Test Time MS), (Highest Player WL), (Highest Num Shooters), (Min Enemies for scaling), (Accuracy reduction per enemy), (Accuracy floor)");
		Displayf("ACCURACYLOGPLAYERCLEAN %d, %d, %d, %d, %.4f, %.4f", elapsedTestTimeMS, m_iLoggingTestHighestWL, m_iLoggingTestHighestNumShooters,
			ms_Tunables.m_EnemyAccuracyScaling.m_iMinNumEnemiesForScaling, ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionPerEnemy, ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionFloor);
		Displayf("*** ENEMY ACCURACY SCALING HISTORY LOG END ***");
	}
	
}

// Record local player killed
void CTaskCombat::CEnemyAccuracyScalingLog::RecordLocalPlayerDeath()
{
	// If logging is enabled
	if( m_bEnabled )
	{
		// Get current time
		const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

		// Compute elapsed test time
		const u32 elapsedTestTimeMS = currentTimeMS - m_uLoggingTestStartTimeMS;

		Displayf("*** ENEMY ACCURACY SCALING HISTORY LOG BEGIN (Time: %d) ***", currentTimeMS);
		Displayf("(Elapsed Test Time MS), (Highest Player WL), (Highest Num Shooters), (Min Enemies for scaling), (Accuracy reduction per enemy), (Accuracy floor)");
		Displayf("ACCURACYLOGPLAYERDEATH %d, %d, %d, %d, %.4f, %.4f", elapsedTestTimeMS, m_iLoggingTestHighestWL, m_iLoggingTestHighestNumShooters,
			ms_Tunables.m_EnemyAccuracyScaling.m_iMinNumEnemiesForScaling, ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionPerEnemy, ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionFloor);
		Displayf("*** ENEMY ACCURACY SCALING HISTORY LOG END ***");
	}
}

// Callback for logging enemy accuracy:
// Gives local player full health, removes invincibility, and starts logging
void CTaskCombat::EnemyAccuracyLogButtonCB()
{
	CPed* pPlayerPed = FindPlayerPed();
	if( pPlayerPed )
	{
		pPlayerPed->SetHealth( pPlayerPed->GetPlayerInfo()->MaxHealth );
	}
	if( CPlayerInfo::ms_bDebugPlayerInvincible )
	{
		CPlayerInfo::ms_bDebugPlayerInvincible = false;
	}
	ms_EnemyAccuracyLog.BeginLoggingTest();
}
#endif // __BANK

aiTaskStateTransitionTable* CTaskCombat::GetTransitionSet()
{
	CCombatBehaviour::CombatType combatType = CCombatBehaviour::GetCombatType(GetPed());
	Assert( combatType >= CCombatBehaviour::CT_OnFootArmed && combatType < CCombatBehaviour::CT_NumTypes );
	return ms_combatTransitionTables[combatType];
}

float CTaskCombat::ScaleReactionTime( CPed* pPed, float UNUSED_PARAM(fOriginalTime) )
{
	CCombatData::Ability ability = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatAbility();
	Assert( ability >= CCombatData::CA_Poor && ability < CCombatData::CA_NumTypes );
	return ms_ReactionScale[ability];
}

CTaskInfo* CTaskCombat::CreateQueriableState() const
{
	return rage_new CClonedCombatTaskInfo(GetState(), m_pPrimaryTarget.Get(), m_pBuddyShot.Get(), m_fExplosionX, m_fExplosionY, m_fExplosionRadius, m_uFlags.IsFlagSet(ComF_IsWaitingAtRoadBlock), m_uFlags.IsFlagSet(ComF_PreventChangingTarget));
}

void CTaskCombat::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

void CTaskCombat::LeaveScope(const CPed* pPed)
{
	ASSERT_ONLY(SetCanChangeState(true);)
    SetTaskCombatState(State_Start);
	ASSERT_ONLY(SetCanChangeState(false);)

    CTaskFSMClone::LeaveScope(pPed);
}

void CTaskCombat::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CPed* pPed = GetEntity() ? GetPed() : NULL;

	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_COMBAT );
    CClonedCombatTaskInfo *combatInfo = static_cast<CClonedCombatTaskInfo*>(pTaskInfo);

	CPed* pNewTarget = static_cast<CPed*>(combatInfo->GetTarget());
	if(pNewTarget != m_pPrimaryTarget)
	{
		m_pPrimaryTarget = pNewTarget;
#if __BANK
		LogTargetInformation();
#endif
		// If we switched to a different target due to networking sync of the
		// task data, update the cached data in CQueriableInterface. This might
		// help fixing an assert about a mismatch in the cached data.
		if(pPed)
		{
			pPed->GetPedIntelligence()->GetQueriableInterface()->UpdateCachedInfo();
		}
	}

	m_pBuddyShot			= static_cast<CPed*>(combatInfo->GetBuddyShot());
	m_fExplosionX			= combatInfo->GetExplosionX();
	m_fExplosionY			= combatInfo->GetExplosionY();
	m_fExplosionRadius		= combatInfo->GetExplosionRadius();

	m_uFlags.ChangeFlag(ComF_IsWaitingAtRoadBlock, combatInfo->IsWaitingAtRoadBlock());
	bool bChangeTarget = combatInfo->GetPreventChangingTarget();
	m_uFlags.ChangeFlag(ComF_PreventChangingTarget, bChangeTarget);
#if __BANK
	if(pPed && !pPed->IsPlayer())
	{
		AI_LOG_WITH_ARGS("CTaskCombat::ReadQueriableState() - ComF_PreventChangingTarget set to %s, ped = %s \n", bChangeTarget ? "true" : "false", pPed->GetLogName());
	}
#endif

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

CTaskFSMClone* CTaskCombat::CreateTaskForClonePed(CPed* OUTPUT_ONLY(pPed))
{
	// NOTE[egarcia]: Check if should clean up the current target (UpdatePrimaryTarget will not be called for the clone task so if we leave a target could get hanging here).
	// This could happen if we are changing the ped ownership because his current owner player has just died.
	if (m_pPrimaryTarget && m_pPrimaryTarget->IsPlayer())
	{
		const CPlayerInfo* pPrimaryTargetPlayerInfo = m_pPrimaryTarget->GetPlayerInfo();
		if (taskVerifyf(pPrimaryTargetPlayerInfo, "Any player should have PlayerInfo!"))
		{
			const CPlayerInfo::State ePlayerInfoState = pPrimaryTargetPlayerInfo->GetPlayerState(); 
			if (ePlayerInfoState == CPlayerInfo::PLAYERSTATE_HASDIED || CPlayerInfo::PLAYERSTATE_RESPAWN)
			{
				ChangeTarget(NULL);
			}
		}
	}

	CTaskCombat* newTask = rage_new CTaskCombat( m_pPrimaryTarget );
	taskDebugf2("CTaskCombat::CreateTaskForClonePed frame[%u] pPed[%p][%s][%s] -- newTask[%p][%s]",fwTimer::GetFrameCount(),pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "",newTask,newTask->GetTaskName());
	newTask->SetTaskCombatState(GetState());

	//flag the cover task to know it's migrating so it keeps the anim task subnetwork in the ped tree..
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COVER)
	{
		(static_cast<CTaskCover*>(GetSubTask()))->SetIsMigrating(true);
	}

	return newTask;
}

CTaskFSMClone* CTaskCombat::CreateTaskForLocalPed(CPed* pPed)
{
	CTaskCombat* newTask = rage_new CTaskCombat( m_pPrimaryTarget );
	newTask->SetTaskCombatState(GetState());

	newTask->SetPreventChangingTarget(m_uFlags.IsFlagSet(ComF_PreventChangingTarget));

	//flag the cover task to know it's migrating so it keeps the anim task subnetwork in the ped tree..
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COVER)
	{
		(static_cast<CTaskCover*>(GetSubTask()))->SetIsMigrating(true);
	}

	if((GetState() == State_MoveToCover) || (GetState() == State_InCover))
	{
		Vector3 pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		CCoverPoint* coverPoint = CCover::FindClosestCoverPoint(pPed, pos);

		// No cover point, then we have to pop out....
		if(coverPoint)
		{
			if(GetState() == State_MoveToCover)
			{
				pPed->SetDesiredCoverPoint(coverPoint);
			}
			else
			{
				//-

				// Copy the config flags from the exiting task for initialisation with the replacement task...
				if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COVER)
				{
					CTaskCover* taskCover = static_cast<CTaskCover*>(GetSubTask());
					newTask->m_uMigrationCoverSetup = taskCover->GetAllCoverFlags();
				}

				// Set the flag so we go directly into cover and skip enter cover...
				newTask->m_uMigrationCoverSetup.SetFlag(CTaskCover::CF_PutPedDirectlyIntoCover);

				//-

				// Flag the new combat task to skip state change scan in ::InCover_OnUpdate to stop it immediately changing state...
				newTask->m_uMigrationFlags.SetFlag(MigrateF_PreventPeriodicStateChangeScan);

				//-

				// Set the ped cover point and reserve it so TaskInCover runs without running TaskCover first....
				pPed->SetCoverPoint(coverPoint);
				pPed->GetCoverPoint()->ReserveCoverPointForPed(pPed);
			}
		}
	}

	return newTask;
}

void CTaskCombat::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	if((State_Start == iState) || (State_Finish == iState))
	{
		return;
	}

	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_Flank:
			expectedTaskType = CTaskTypes::TASK_CLIMB_LADDER;
			break;
		case State_InCover:
		case State_MoveToCover:
		case State_Retreat:
		case State_MoveWithinAttackWindow:
		case State_MoveWithinDefensiveArea:
		case State_MoveToTacticalPoint:
		case State_Advance:
		case State_WaitingForCoverExit:
			{
				if(!GetSubTask() && !GetNewTask())
				{
					expectedTaskType = CTaskTypes::TASK_COVER;

					if(!CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType) && iState != State_InCover)
					{
						expectedTaskType = CTaskTypes::TASK_CLIMB_LADDER;

						if (!CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType))
						{
							expectedTaskType = CTaskTypes::TASK_GUN;

							if (CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType))
							{
								taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::UpdateClonedSubTasks -- invoke CreateSubTaskFromClone 3 -- success -- iState[%s] expectedTaskType[%d][%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(iState),expectedTaskType,TASKCLASSINFOMGR.GetTaskName(expectedTaskType));
							}
						}
						else
						{
							taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::UpdateClonedSubTasks -- invoke CreateSubTaskFromClone 2 -- success -- iState[%s] expectedTaskType[%d][%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(iState),expectedTaskType,TASKCLASSINFOMGR.GetTaskName(expectedTaskType));
						}
					}
					else
					{
						taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::UpdateClonedSubTasks -- invoke CreateSubTaskFromClone 1 -- success -- iState[%s] expectedTaskType[%d][%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(iState),expectedTaskType,TASKCLASSINFOMGR.GetTaskName(expectedTaskType));
					}
				}

				return;
			}
			break;
		case State_AdvanceFrustrated:
			expectedTaskType = CTaskTypes::TASK_COVER; 
			break;
		case State_PullTargetFromVehicle:
		case State_EnterVehicle:
			expectedTaskType = CTaskTypes::TASK_ENTER_VEHICLE;
			break;
		case State_MeleeAttack:
			expectedTaskType = CTaskTypes::TASK_MELEE;
			break;	
		case State_AimIntro:
			expectedTaskType = CTaskTypes::TASK_REACT_AIM_WEAPON;
			break;
		case State_PersueInVehicle:
			{
				if(!GetSubTask() && !GetNewTask())
				{
					expectedTaskType = CTaskTypes::TASK_VEHICLE_GUN;

					if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON))
					{
						expectedTaskType = CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON;
					}

					if(!CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType))
					{
						expectedTaskType = CTaskTypes::TASK_EXIT_VEHICLE;

						if (CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType))
						{
							taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::UpdateClonedSubTasks -- CreateSubTaskFromClone 4 -- success -- iState[%s] expectedTaskType[%d][%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(iState),expectedTaskType,TASKCLASSINFOMGR.GetTaskName(expectedTaskType));
						}
					}
					else
					{
						taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::UpdateClonedSubTasks -- invoke CreateSubTaskFromClone 3 -- success -- iState[%s] expectedTaskType[%d][%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(iState),expectedTaskType,TASKCLASSINFOMGR.GetTaskName(expectedTaskType));
					}
				}

				return;
			}
			break;
        case State_TargetUnreachable:
            {
                if(!GetSubTask() && !GetNewTask())
                {
                    expectedTaskType = CTaskTypes::TASK_WEAPON_BLOCKED;
                }
            }
            break;

		case State_Fire:
			{
				if (!GetSubTask() && !GetNewTask() && CanVaryAimPose(pPed))
				{
					expectedTaskType = CTaskTypes::TASK_VARIED_AIM_POSE;
				}
			}
			break;

			// we don't want to return as we have the gun task to try and create later....
		default:
			break;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		// if we've created a subtask here, just return....
		if(CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType))
		{
			taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::UpdateClonedSubTasks -- invoke CreateSubTaskFromClone 5 -- success -- iState[%s] expectedTaskType[%d][%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(iState),expectedTaskType,TASKCLASSINFOMGR.GetTaskName(expectedTaskType));
			return;
		}
	}

	//----------------

	// it's only these states that can fire a gun off in combat....
	if((iState == State_Fire) || (iState == State_Advance) || (iState == State_ChargeTarget)
		|| (iState == State_AdvanceFrustrated) || (iState == State_ChaseOnFoot) || (iState == State_MoveWithinAttackWindow)
			|| (iState == State_ArrestTarget) || (iState == State_TargetUnreachable))
	{
		// if i'm running a subtask it's up to it to shoot (otherwise something 
		// like cover subtask can get replaced with shooting when we want cover
		// subtask to shoot instead....
		if(!GetSubTask() && !GetNewTask())
		{
			// it's possible that the ped can be updated with the full combat heirarchy sent over the queirable interface
			// at once (i.e. [update X] TaskDoNothing [update X + 1] TaskThreatResponse, TaskCombat, TaskCover, TaskGun is sent over.
			// if this happens, when TaskCombat is ran it will try to create TaskGun here as it's part of the heirarchy.
			// We want this to wait until TaskCover has been cloned and for it to create TaskGun so we test against that here...
			CQueriableInterface* queriableInterface = pPed->GetPedIntelligence()->GetQueriableInterface();
			Assert(queriableInterface);

			// if we haven't found the subtask that takes over TaskGun launching in the queriable interface (may not have been ran yet)
			CTaskInfo* info = queriableInterface->GetTaskInfoForTaskType(CTaskTypes::TASK_COVER, PED_TASK_PRIORITY_MAX, false);
			
			if(!info)
			{
				expectedTaskType = CTaskTypes::TASK_GUN;
				if (CTaskFSMClone::CreateSubTaskFromClone(pPed, expectedTaskType))
				{
					taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::UpdateClonedSubTasks -- invoke CreateSubTaskFromClone 6 -- success -- iState[%s] expectedTaskType[%d][%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(iState),expectedTaskType,TASKCLASSINFOMGR.GetTaskName(expectedTaskType));
				}
			}
		}
	}
}

CTask::FSM_Return  CTaskCombat::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(State_Finish != iState)
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != -1)
			{
				if(GetStateFromNetwork() != GetState())
				{
					s32 stateFromNetwork = GetStateFromNetwork();

					if((GetState() == State_InCover && GetStateFromNetwork() == State_WaitingForCoverExit)
						|| (GetState() == State_MoveToCover && GetStateFromNetwork() == State_WaitingForCoverExit)
						|| (GetState() == State_WaitingForCoverExit && GetStateFromNetwork() == State_InCover))
					{
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					}

					if (GetState() == State_MoveToCover && GetStateFromNetwork() == State_WaitingForCoverExit)
					{
						// force an in-cover state if we have to transition from move -> cover exit, otherwise the cover subtask will get dumped
						stateFromNetwork = State_InCover;
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					}

					// hack bug fix to keep peds firing vehicle mounted weapons when they come back into scope
					if (GetState() == State_Start && stateFromNetwork == State_PersueInVehicle)
					{
						SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
					}

					SetTaskCombatState(stateFromNetwork);
					return FSM_Continue;
				}
			}

			Assert(GetState() == iState);
			UpdateClonedSubTasks(pPed, GetState());
		}	
	}

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter_Clone(pPed);

		FSM_State(State_Resume)
			FSM_OnEnter
				Resume_OnEnter_Clone(pPed);

		FSM_State(State_ReactToExplosion)
			FSM_OnEnter
				ReactToExplosion_OnEnter(pPed);

		FSM_State(State_ReactToBuddyShot)
			FSM_OnEnter
				ReactToBuddyShot_OnEnter_Clone();
			FSM_OnUpdate
				ReactToBuddyShot_OnUpdate_Clone();
			FSM_OnExit
				ReactToBuddyShot_OnExit_Clone();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

		// Task terminates here
		FSM_State(State_Advance)
		FSM_State(State_AdvanceFrustrated)
		FSM_State(State_Fire)
		FSM_State(State_InCover)
		FSM_State(State_MoveToCover)
		FSM_State(State_Retreat)
		FSM_State(State_Search)
		FSM_State(State_MeleeAttack)
		FSM_State(State_DragInjuredToSafety)
		FSM_State(State_Mounted)
		FSM_State(State_MeleeToArmed)
		FSM_State(State_ReactToImminentExplosion)
		FSM_State(State_EnterVehicle)
		FSM_State(State_PullTargetFromVehicle)
		FSM_State(State_MoveWithinDefensiveArea)
		FSM_State(State_MoveWithinAttackWindow)
		FSM_State(State_Flank)
 		FSM_State(State_GoToEntryPoint)
		FSM_State(State_ChaseOnFoot)
		FSM_State(State_ChargeTarget)
		FSM_State(State_WaitingForEntryPointToBeClear)
		FSM_State(State_DecideTargetLossResponse)
		FSM_State(State_PersueInVehicle)
		FSM_State(State_GetOutOfWater)
		FSM_State(State_WaitingForCoverExit)
		FSM_State(State_AimIntro)
		FSM_State(State_TargetUnreachable)
		FSM_State(State_ArrestTarget)
		FSM_State(State_MoveToTacticalPoint)
		FSM_State(State_ThrowSmokeGrenade)
		FSM_State(State_Victory)
		FSM_State(State_ReturnToInitialPosition)

	FSM_End
}

CTask::FSM_Return CTaskCombat::UpdateFSM( s32 iState, FSM_Event iEvent )
{
	PF_FUNC(CTaskCombat);

	CPed *pPed = GetPed(); //Get the pedptr.

	FSM_Begin
		// Initial start to the task
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		// Resume
		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		// Fire from a stationary position
		FSM_State(State_Fire)
			FSM_OnEnter
				Fire_OnEnter(pPed);
			FSM_OnUpdate
				return Fire_OnUpdate(pPed);
			FSM_OnExit
				Fire_OnExit();

		// Advance directly toward the target
		FSM_State(State_Advance)
			FSM_OnEnter
				Advance_OnEnter(pPed);
			FSM_OnUpdate
				return Advance_OnUpdate(pPed);

		// Advance directly toward the target under specific conditions (target is in cover, no LOS to torso)
		FSM_State(State_AdvanceFrustrated)
			FSM_OnEnter
				AdvanceFrustrated_OnEnter(pPed);
			FSM_OnUpdate
				return AdvanceFrustrated_OnUpdate(pPed);
			FSM_OnExit
				AdvanceFrustrated_OnExit();

		// Charge the target's position
		FSM_State(State_ChargeTarget)
			FSM_OnEnter
				ChargeTarget_OnEnter(pPed);
			FSM_OnUpdate
				return ChargeTarget_OnUpdate(pPed);
			FSM_OnExit
				ChargeTarget_OnExit(pPed);

		// Try to chase after the target
		FSM_State(State_ChaseOnFoot)
			FSM_OnEnter
				ChaseOnFoot_OnEnter(pPed);
			FSM_OnUpdate
				return ChaseOnFoot_OnUpdate(pPed);
			FSM_OnExit
				ChaseOnFoot_OnExit();

		// Try to move around the target and approach from a different direction
		FSM_State(State_Flank)
			FSM_OnEnter
				Flank_OnEnter(pPed);
			FSM_OnUpdate
				return Flank_OnUpdate(pPed);

			// Fire from cover
		FSM_State(State_InCover)
			FSM_OnEnter
				InCover_OnEnter(pPed);
			FSM_OnUpdate
				return InCover_OnUpdate(pPed);

		// Move to some cover we found
		FSM_State(State_MoveToCover)
			FSM_OnEnter
				MoveToCover_OnEnter(pPed);
			FSM_OnUpdate
				return MoveToCover_OnUpdate(pPed);

		// Retreat to seek cover
		FSM_State(State_Retreat)
			FSM_OnEnter
				Retreat_OnEnter(pPed);
			FSM_OnUpdate
				return Retreat_OnUpdate(pPed);

		// Seek cover nearby
		FSM_State(State_MeleeAttack)
			FSM_OnEnter
				MeleeAttack_OnEnter(pPed);
			FSM_OnUpdate
				return MeleeAttack_OnUpdate(pPed);
			FSM_OnExit
				MeleeAttack_OnExit(pPed);

		// Search for cover in the peds defined attack window
		FSM_State(State_MoveWithinAttackWindow)
			FSM_OnEnter
				MoveWithinAttackWindow_OnEnter(pPed);
			FSM_OnUpdate
				return MoveWithinAttackWindow_OnUpdate(pPed);
			FSM_OnExit
				MoveWithinAttackWindow_OnExit(pPed);

		// Move back inside a defensive area
		FSM_State(State_MoveWithinDefensiveArea)
			FSM_OnEnter
				MoveWithinDefensiveArea_OnEnter(pPed);
			FSM_OnUpdate
					return MoveWithinDefensiveArea_OnUpdate(pPed);
			FSM_OnExit
				MoveWithinDefensiveArea_OnExit(pPed);

		// Persue the car from inside a vehicle
		FSM_State(State_PersueInVehicle)
			FSM_OnEnter
				PersueInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return PersueInVehicle_OnUpdate(pPed);
			FSM_OnExit
				PersueInVehicle_OnExit(pPed);

		FSM_State(State_WaitingForEntryPointToBeClear)
			FSM_OnEnter
				WaitingForEntryPointToBeClear_OnEnter();
			FSM_OnUpdate
				return WaitingForEntryPointToBeClear_OnUpdate();

		FSM_State(State_GoToEntryPoint)
			FSM_OnEnter
				GoToEntryPoint_OnEnter();
			FSM_OnUpdate
				return GoToEntryPoint_OnUpdate();

		// PullTargetFromVehicle
		FSM_State(State_PullTargetFromVehicle)
			FSM_OnEnter
				PullTargetFromVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return PullTargetFromVehicle_OnUpdate(pPed);

		FSM_State(State_EnterVehicle)
			FSM_OnEnter
				EnterVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return EnterVehicle_OnUpdate(pPed);
			FSM_OnExit
				EnterVehicle_OnExit();

		// Search for a lost target
		FSM_State(State_Search)
			FSM_OnEnter
				Search_OnEnter(pPed);
			FSM_OnUpdate
				return Search_OnUpdate(pPed);

		FSM_State(State_DecideTargetLossResponse)
			FSM_OnUpdate
				return DecideTargetLossResponse_OnUpdate(pPed);
				
		FSM_State(State_DragInjuredToSafety)
			FSM_OnEnter
				DragInjuredToSafety_OnEnter();
			FSM_OnUpdate
				return DragInjuredToSafety_OnUpdate();

		FSM_State(State_Mounted)
			FSM_OnEnter
				Mounted_OnEnter();
			FSM_OnUpdate
				return Mounted_OnUpdate();
				
		FSM_State(State_MeleeToArmed)
			FSM_OnUpdate
				return MeleeToArmed_OnUpdate();
				
		FSM_State(State_ReactToImminentExplosion)
			FSM_OnEnter
				ReactToImminentExplosion_OnEnter();
			FSM_OnUpdate
				return ReactToImminentExplosion_OnUpdate();
				
		FSM_State(State_ReactToExplosion)
			FSM_OnEnter
				ReactToExplosion_OnEnter(pPed);
			FSM_OnUpdate
				return ReactToExplosion_OnUpdate();

		FSM_State(State_ReactToBuddyShot)
			FSM_OnEnter
				ReactToBuddyShot_OnEnter();
			FSM_OnUpdate
				return ReactToBuddyShot_OnUpdate();

		FSM_State(State_GetOutOfWater)
			FSM_OnEnter
				GetOutOfWater_OnEnter();
			FSM_OnUpdate
				return GetOutOfWater_OnUpdate();

		FSM_State(State_WaitingForCoverExit)
			FSM_OnUpdate
				return WaitingForCoverExit_OnUpdate();
				
		FSM_State(State_AimIntro)
			FSM_OnEnter
				AimIntro_OnEnter();
			FSM_OnUpdate
				return AimIntro_OnUpdate();
			FSM_OnExit
				AimIntro_OnExit();
				
		FSM_State(State_TargetUnreachable)
			FSM_OnEnter
				TargetUnreachable_OnEnter();
			FSM_OnUpdate
				return TargetUnreachable_OnUpdate();

		FSM_State(State_ArrestTarget)
			FSM_OnEnter
				ArrestTarget_OnEnter();
			FSM_OnUpdate
				return ArrestTarget_OnUpdate();
			FSM_OnExit
				ArrestTarget_OnExit();

		FSM_State(State_MoveToTacticalPoint)
			FSM_OnEnter
				MoveToTacticalPoint_OnEnter();
			FSM_OnUpdate
				return MoveToTacticalPoint_OnUpdate();

		FSM_State(State_ThrowSmokeGrenade)
			FSM_OnEnter
				ThrowSmokeGrenade_OnEnter();
			FSM_OnUpdate
				return ThrowSmokeGrenade_OnUpdate();
		
		FSM_State(State_Victory)
			FSM_OnEnter
				Victory_OnEnter();
			FSM_OnUpdate
				return Victory_OnUpdate();

		FSM_State(State_ReturnToInitialPosition)
			FSM_OnEnter
				ReturnToInitialPosition_OnEnter();
			FSM_OnUpdate
				return ReturnToInitialPosition_OnUpdate();

		// Task terminates here
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
		
	FSM_End
}

CTask::FSM_Return CTaskCombat::ProcessPostFSM()
{
	// Some things are frame dependent and since we only check transitions on specific frames we should only reset these things on those frames
	// otherwise the transition checks won't match properly with our timers
	bool bCheckedThisFrame = (m_uLastFrameCheckedStateTransitions == fwTimer::GetFrameCount());
	if(bCheckedThisFrame)
	{
		if(m_jackingTimer.IsFinished())
		{
			ResetJackingTimer();
		}
	}

	// Go ahead and reset this flag (as of now it only relates to the combat task)
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsBeingMeleeHomedByPlayer, false);

	return FSM_Continue;
}

void CTaskCombat::SetTaskCombatState(const s32 iState)
{
#if !__NO_OUTPUT
	if ((Channel_ai_task.FileLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_ai_task.TtyLevel >= DIAG_SEVERITY_DEBUG2))
	{
		s32 currentState = GetState();
		if (currentState != iState)
		{
			const CPed* pPed = GetEntity() ? GetPed() : NULL;
			if (pPed)
			{
				taskDebugf2("Frame : %d - %s ped : %s : 0x%p : %s -- CTaskCombat::SetTaskCombatState -- current GetState[%s] incoming iState[%s]",fwTimer::GetFrameCount(),pPed->IsNetworkClone() ? "Clone" : "Local", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : pPed->GetModelName(),pPed,pPed->GetModelName(),GetStateName(currentState),GetStateName(iState));
			}
		}
	}
#endif

	SetState(iState);
}

aiTask* CTaskCombat::Copy() const
{
	//Create the task.
	CTaskCombat* pTask = rage_new CTaskCombat(m_pPrimaryTarget, 0.0f, m_uFlags);
	pTask->SetConfigFlagsForVehiclePursuit(m_uConfigFlagsForVehiclePursuit);
	
	return pTask;
}

void CTaskCombat::Start_OnEnter_Clone( CPed* pPed )
{
	// Add ourselves to the combat task manager
	CCombatManager::GetCombatTaskManager()->AddPed(pPed);

	// Set initial position and heading
	m_vInitialPosition = pPed->GetTransform().GetPosition();
	m_fInitialHeading = pPed->GetCurrentHeading();
}

CTask::FSM_Return CTaskCombat::Start_OnUpdate( CPed* pPed )
{
	// Pick the transition set to be used
	aiTaskStateTransitionTable* pTransitionSet = GetTransitionSet();
	if(GetTransitionTableData()->GetTransitionSet() != pTransitionSet)
	{
		GetTransitionTableData()->SetTransitionSet(pTransitionSet);
	}

	// Keep track of whether we should arrest the target
	bool bShouldArrestTarget = false;

	// Note if you are entering combat because of getting dragged out of your car.
	if (GetPed()->GetPedIntelligence()->GetCurrentEventType() == EVENT_DRAGGED_OUT_CAR)
	{
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_WentIntoCombatAfterBeingJacked, true);
	}

	//Check if we have not started.
	const bool bStartCombat = !m_uFlags.IsFlagSet(ComF_HasStarted);
	if(bStartCombat)
	{
		//Say initial audio.
		SayInitialAudio();

		// Go ahead and reset this flag (need to make sure it's not stale in case it was set when we were not in combat)
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsBeingMeleeHomedByPlayer, false);

		// Add ourselves to the combat task manager
		CCombatManager::GetCombatTaskManager()->AddPed(pPed);

		//Request a combat director.
		RequestCombatDirector();

		//Request a tactical analysis.
		RequestTacticalAnalysis();

		//Check if we have not equipped a weapon.
		if(!m_uFlags.IsFlagSet(ComF_WeaponAlreadyEquipped))
		{
			//Equip a weapon for the threat.
			EquipWeaponForThreat();
		}

		// Should we arrest?
		bShouldArrestTarget = m_uFlags.IsFlagSet(ComF_ArrestTarget) && CanArrestTarget();

		// Used to vary the time it takes to exit combat due to injured/dead target
		m_fInjuredTargetTimerModifier = fwRandom::GetRandomNumberInRange(-ms_Tunables.m_MaxInjuredTargetTimerVariation, ms_Tunables.m_MaxInjuredTargetTimerVariation);

		// Get the target's wanted and try to trigger the arrive audio
		if(pPed->IsRegularCop() && m_pPrimaryTarget->GetIsOnFoot())
		{
			CWanted* pTargetWanted = m_pPrimaryTarget->GetPlayerWanted();
			if(pTargetWanted)
			{
				pTargetWanted->TriggerCopArrivalAudioFromPed(pPed);
			}
		}

		// For mp games specifically, use the following context at the beginning of combat
		if(NetworkInterface::IsGameInProgress())
		{
			pPed->NewSay("MP_SHOUT_THREATEN_PED");
		}

		//Update our order.
		UpdateOrder();

		// Set initial position and heading
		m_vInitialPosition = pPed->GetTransform().GetPosition();
		m_fInitialHeading = pPed->GetCurrentHeading();

		//Set the flag.
		m_uFlags.SetFlag(ComF_HasStarted);
	}

	// Set certain combat behaviour flags affecting communication - this must be done before UpdateCommunication() in ProcessPreFSM().
	if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_SetDisableShoutTargetPositionOnCombatStart))
	{
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableShoutTargetPosition);
	}

	//Set action mode.
	SetUsingActionMode(true);

	if (pPed->GetWeaponManager())
	{
		if (pPed->PopTypeIsMission())
		{
			const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();

			// TEMP FIX
			// if the ped is inside a fire truck, it's water cannon / vehicle mounted weapon means when it gets in, 
			// the mission ped has it's weapon unequipped so pWeaponInfo comes back NULL...
			if( !pWeaponInfo && pPed->GetIsInVehicle() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetModelIndex() == MI_CAR_FIRETRUCK )
			{
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::Start_OnUpdate, Ped %s, Aborting TaskCombat. Firetruck hack.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				return FSM_Quit;
			}

			if (!pWeaponInfo || !pWeaponInfo->GetIsGun())
			{
				pPed->GetWeaponManager()->EquipBestWeapon();

				// Equip best weapon resets the time until next burst so reset it so we can fire immediately
				GetPed()->GetPedIntelligence()->GetFiringPattern().ResetTimeUntilNextBurst();
			}
		}
	}
	else
	{
		weaponAssert(!m_uFlags.IsFlagSet(ComF_ForceFire));		// if we have no weapons then forcing firing is probably bad!
	}

	if (pPed->GetCoverPoint())
	{
		s32 iDesiredState;
		if(GetDesiredCoverStateToResume(iDesiredState))
		{
			if(iDesiredState == State_MoveToCover)
			{
				// Need to make sure the desired cover point is set and clear our existing cover point (due to the logic of the on enter of move to cover state).
				pPed->SetDesiredCoverPoint(pPed->GetCoverPoint());
				pPed->ReleaseCoverPoint();
			}

			SetTaskCombatState(iDesiredState);
			return FSM_Continue;
		}
	}
	
	CombatState eDesiredNextCombatState = m_uFlags.IsFlagSet(ComF_ForceFire) ? State_Fire : static_cast<CombatState>(FindNewStateTransition(State_Finish));
	const bool bShouldPlayAimIntro = bStartCombat && ShouldPlayAimIntro(eDesiredNextCombatState, bShouldArrestTarget);
	if(bShouldPlayAimIntro)
	{
		//Play an aim intro.
		SetTaskCombatState(State_AimIntro);
		return FSM_Continue;
	}
	else if(bShouldArrestTarget)
	{
		//Don't force arrest.
		m_uRunningFlags.SetFlag(RF_DontForceArrest);

		//Arrest the target.
		SetTaskCombatState(State_ArrestTarget);
		return FSM_Continue;
	}

	SetTaskCombatState(eDesiredNextCombatState);

	return FSM_Continue;
}

void CTaskCombat::Resume_OnEnter_Clone( CPed* pPed )
{
	// Add ourselves to the combat task manager
	CCombatManager::GetCombatTaskManager()->AddPed(pPed);
}

CTask::FSM_Return CTaskCombat::Resume_OnUpdate()
{
	// Go ahead and reset this flag (need to make sure it's not stale in case it was set when we were not in combat)
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsBeingMeleeHomedByPlayer, false);

	// Reset the time until the next burst so we can fire immediately
	GetPed()->GetPedIntelligence()->GetFiringPattern().ResetTimeUntilNextBurst();

	// Add ourselves to the combat task manager
	CCombatManager::GetCombatTaskManager()->AddPed(GetPed());

	//Request a tactical analysis.
	RequestTacticalAnalysis();

	//Set the state to resume to.
	bool bKeepSubTask = false;
	int iStateToResumeTo = ChooseStateToResumeTo(bKeepSubTask);
	SetTaskCombatState(iStateToResumeTo);

	//Check if we should keep the sub-task.
	if(bKeepSubTask)
	{
		//Keep the sub-task across the transition.
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}

	return FSM_Continue;
}

void CTaskCombat::Fire_OnEnter( CPed* pPed )
{
	CAITarget target(m_pPrimaryTarget);
	CTaskShootAtTarget* pNewTask = rage_new CTaskShootAtTarget(target, 0, -1.0f);
	
	//Check if the aim pose can be varied.
	if(CanVaryAimPose(pPed))
	{
		//Vary the aim pose.
		SetNewTask(rage_new CTaskVariedAimPose(pNewTask));
	}
	else
	{
		SetNewTask(pNewTask);
	}
}

CTask::FSM_Return CTaskCombat::Fire_OnUpdate( CPed* pPed )
{
	if(m_uFlags.IsFlagSet(ComF_TemporarilyForceFiringState))
	{
		ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
		ScalarV scMinDistSq = LoadScalar32IntoScalarV(square(ms_Tunables.m_MinForceFiringDistance));
		if(IsLessThanAll(scDistSq, scMinDistSq))
		{
			m_uFlags.ClearFlag(ComF_TemporarilyForceFiringState);
		}
	}

	//Check if the state can change.
	bool bIsPotentialStateChangeUrgent = IsPotentialStateChangeUrgent();
	bool bStateCanChange = (bIsPotentialStateChangeUrgent || StateCanChange());
	if(bStateCanChange)
	{
		// Don't allow state changes if we haven't been in the shoot state of varied aim pose for long enough
		// Unless the potential state change is urgent... otherwise, peds won't run from potential blasts.
		if(!bIsPotentialStateChangeUrgent)
		{
			const CTaskVariedAimPose* pTask = static_cast<const CTaskVariedAimPose *>(FindSubTaskOfType(CTaskTypes::TASK_VARIED_AIM_POSE));
			if( pTask && pTask->GetState() == CTaskVariedAimPose::State_Shoot && pTask->GetPreviousState() == CTaskVariedAimPose::State_PlayTransition &&
				pTask->GetTimeInState() < ms_Tunables.m_fMinTimeAfterAimPoseForStateChange )
			{
				bStateCanChange = false;
			}
		}
	}
	
	UpdateGunTarget(true);

	// Check for (and set) the move away from position for varied aim pose
	UpdateVariedAimPoseMoveAwayFromPosition(pPed);

	// Possibly play an ambient combat animation
	UpdateAmbientAnimPlayback(pPed);

	if (bStateCanChange && CheckForMovingWithinDefensiveArea(true))
	{
		return FSM_Continue;
	}
	
	if(bStateCanChange && CheckForTargetLossResponse())
	{
		return FSM_Continue;
	}

	if( bStateCanChange && CheckShouldMoveBackInsideAttackWindow(pPed) )
	{
		SetTaskCombatState(State_MoveWithinAttackWindow);
		return FSM_Continue;
	}

	if( bStateCanChange && CheckForChargeTarget() )
	{
		return FSM_Continue;
	}

	if( bStateCanChange && CheckForThrowSmokeGrenade() )
	{
		return FSM_Continue;
	}

	if( bStateCanChange && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover) && HasValidatedDesiredCover() )
	{
		SetTaskCombatState(State_MoveToCover);
		return FSM_Continue;
	}

	if( bStateCanChange && ShouldReturnToInitialPosition() )
	{
		if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_WillAdvance)
		{
			SetTaskCombatState(State_Advance);
		}
		else
		{
			SetTaskCombatState(State_ReturnToInitialPosition);
		}

		return FSM_Continue;
	}

	pPed->ReleaseCoverPoint();

	pPed->SetIsStrafing(true);
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if( PickNewStateTransition(State_Finish) )
		{
			return FSM_Continue;
		}
		else
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}
	}

	// Decide whether we can check the state transitions
	bool bCheckStateTransitions = true;

	CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
	if( pGunTask )
	{
		if( pGunTask->GetIsReloading() || pGunTask->GetIsSwitchingWeapon())
		{
			// Don't change state if we are in the middle of reloading or swapping
			bCheckStateTransitions = false;
		}
	}

	CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
	if (bStateCanChange && pIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover) && !m_uFlags.IsFlagSet(ComF_IsInDefensiveArea) && CanMoveWithinDefensiveArea())
	{
		SetTaskCombatState(State_MoveWithinDefensiveArea);
	}

	// Periodically check for a new state
	if( bStateCanChange && bCheckStateTransitions && PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME) ) )
	{
		return FSM_Continue;
	}

	//Activate time slicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskCombat::Fire_OnExit()
{
	m_uFlags.ClearFlag(ComF_TaskAbortedForStaticMovement);
	m_uFlags.ClearFlag(ComF_MoveToCoverStopped);
	m_uFlags.ClearFlag(ComF_TemporarilyForceFiringState);
	m_fTimeUntilForcedFireAllowed = ms_Tunables.m_TimeBetweenForcedFireStates * fwRandom::GetRandomNumberInRange(.9f, 1.1f);
}

void CTaskCombat::InCover_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	if(GetPreviousState() != State_WaitingForCoverExit)
	{
		// Reset our ped specific move to cover timer
		ResetMoveToCoverTimer();

		s32 coverFlags = CTaskCover::CF_SkipIdleCoverTransition;

		// if we've recently migrated and the previous cover task was set up, apply the same flags here...
		if(m_uMigrationCoverSetup != 0)
		{
			coverFlags |= m_uMigrationCoverSetup.GetAllFlags();
			m_uMigrationCoverSetup.ClearAllFlags();
		}

		// Need to stop any ambient clip that is playing
		StopClip(FAST_BLEND_OUT_DELTA);
		CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(m_pPrimaryTarget), coverFlags/*, forcedExit*/);
		pCoverTask->SetBlendInDuration(NORMAL_BLEND_DURATION);
		SetNewTask(pCoverTask);
	}
}

CTask::FSM_Return CTaskCombat::InCover_OnUpdate(CPed* pPed)
{
	// Mark our coverpoint as in use if we have one
	pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint, true );

	// Don't allow transitions if the ped is forced to stay in cover
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedToStayInCover))
	{
		return FSM_Continue;
	}

	// Grab our cover sub task and our in cover sub task
	CTaskCover* pCoverSubtask = static_cast<CTaskCover*>(FindSubTaskOfType(CTaskTypes::TASK_COVER));
	CTaskInCover* pInCoverSubtask = static_cast<CTaskInCover*>(FindSubTaskOfType(CTaskTypes::TASK_IN_COVER));

	s32 iNewState = -1;
	bool bSafeToChangeState = true;
	bool bCoverTaskStarting = false;
	bool bAimingFromCover = false;

	if (FindSubTaskOfType(CTaskTypes::TASK_ENTER_COVER))
	{
		// Don't change state when entering cover
		bSafeToChangeState = false;
	}
	else if (pInCoverSubtask)
	{
		s32 iCoverState = pInCoverSubtask->GetState();
		bAimingFromCover = iCoverState == CTaskInCover::State_Aim;
		if (!bAimingFromCover)
		{
			bSafeToChangeState = false;
		}
	}
	else
	{
		if(pPed->IsNetworkClone() && !pCoverSubtask && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COVER))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}
		if(pCoverSubtask && pCoverSubtask->GetState() == CTaskCover::State_Start)
		{
			bSafeToChangeState = false;
			bCoverTaskStarting = true;
		}
	}
	
	if (m_uFlags.IsFlagSet(ComF_ExitCombatRequested))
	{
		iNewState = State_Finish;
	}
	else if (CheckForTargetLossResponse())
	{
		iNewState = State_DecideTargetLossResponse;
	}
	else if (CheckForMovingWithinDefensiveArea(false))
	{
		iNewState = State_MoveWithinDefensiveArea;
	}
	else if (CheckShouldMoveBackInsideAttackWindow(pPed) )
	{
		iNewState = State_MoveWithinAttackWindow;
	}
	else if (bAimingFromCover && CheckIfAnotherPedIsAtMyCoverPoint())
	{
		iNewState = State_Fire;
	}
	else if(ShouldReturnToInitialPosition())
	{
		iNewState = State_ReturnToInitialPosition;
	}
	else if (pPed->GetCoverPoint() && !DoesCoverPointProvideCover(pPed->GetCoverPoint()))
	{
		if(CCombatManager::GetCombatTaskManager()->CountPedsInCombatWithTarget(*m_pPrimaryTarget, CCombatTaskManager::OF_ReturnAfterInitialValidPed))
		{
			pPed->NewSay("GET_HIM");
		}

		iNewState = State_Fire;
	}
	else if( CheckForChargeTarget(false) )
	{
		// go to fire state, then charge
		iNewState = State_Fire;
	}
	else if (GetIsSubtaskFinished(CTaskTypes::TASK_COVER))
	{
		pPed->NewSay("FUCK_THIS",0,true);

		// go to seek cover if we can use cover, otherwise fire state
		CCombatBehaviour& combatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();
		if (combatBehaviour.IsFlagSet(CCombatData::BF_CanUseCover) && CanUseDesiredCover())
		{
			iNewState = State_MoveToCover;
		}
		else
		{
			iNewState = State_Fire;
		}
	}
	else
	{
		// Keep the peds target up to date
		if (pInCoverSubtask)
		{
			pInCoverSubtask->UpdateTarget(m_pPrimaryTarget);
		}

		if (!pPed->IsLocalPlayer() && !pPed->IsNetworkClone())
		{
			if (!pPed->GetWeaponManager()->GetRequiresWeaponSwitch() && HelperGetEquippedWeaponInfo()
				&& HelperGetEquippedWeaponInfo()->GetIsMelee())
			{
				iNewState = State_MeleeAttack;
			}
		}

		// If the target gets too close, look for cover again
		if (pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_WillRetreat && (Dist(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition()).Getf() < ms_Tunables.m_fTargetTooCloseDistance))
		{
			iNewState = State_Retreat;
		}

		// if this is the first run through after migrating then don't check for a state transition as things haven't been set up yet (e.g line of sight check)...
		if(!NetworkInterface::IsGameInProgress() || !m_uMigrationFlags.IsFlagSet(MigrateF_PreventPeriodicStateChangeScan))
		{
			// Periodically check for a new state
			if(PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)))
			{
				iNewState = GetState();
			}
		}
	}

	// if a new state was requested
	if(iNewState >= 0)
	{
		// If we can change states or we don't have a cover task just go ahead and set the state right away
		if( bSafeToChangeState ||
			(!pInCoverSubtask && !bCoverTaskStarting) )
		{
			pPed->ReleaseCoverPoint();

			if(ms_Tunables.m_EnableForcedFireForTargetProximity && iNewState == State_Fire)
			{
				m_uFlags.SetFlag(ComF_TemporarilyForceFiringState);
			}

#if __ASSERT
			if (pPed->IsNetworkClone())
			{
				aiDebugf1("Frame : %i, Clone %s quitting out of in cover task due to combat state transition, previous state %s, state from network %s", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), GetStateName(GetPreviousState()), GetStateName(GetStateFromNetwork()));
			}
#endif // __ASSERT
			SetTaskCombatState(iNewState);
			return FSM_Continue;
		}
		else if(pInCoverSubtask)
		{
			// store off our desired state
			m_iDesiredState = iNewState;

			// Try to go to idle if we are finished with combat
			if(m_uFlags.IsFlagSet(ComF_ExitCombatRequested))
			{
				pCoverSubtask->SetForcedExit(pInCoverSubtask->GetState() == CTaskInCover::State_AimIntro ? CTaskCover::FE_Aim : CTaskCover::FE_Idle);
			}
			else
			{
				// Request an exit via aim intro and keep our current task then go to the waiting for cover exit state
				pInCoverSubtask->SetCoverFlag(CTaskCover::CF_AimIntroRequested);
			}

#if __ASSERT
			if (pPed->IsNetworkClone())
			{
				aiDebugf1("Frame : %i, Clone %s quitting out of in cover task due to combat state transition, previous state %s, state from network %s", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), GetStateName(GetPreviousState()), GetStateName(GetStateFromNetwork()));
			}
#endif // __ASSERT
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			SetTaskCombatState(State_WaitingForCoverExit);
			return FSM_Continue;
		}
	}

	// Check if the ped wants to throw smoke grenade
	const bool bIsPlayer = false;
	const bool bChangeState = false;
	if( pInCoverSubtask && pInCoverSubtask->CouldThrowProjectile(bIsPlayer) && CheckForThrowSmokeGrenade(bChangeState) )
	{
		// Tell the in cover subtask to throw a smoke grenade as soon as possible
		pInCoverSubtask->RequestThrowSmokeGrenade();
	}

	// We've now had a chance to run through this state once after migrating so can clear this flag....
	m_uMigrationFlags.ClearFlag(MigrateF_PreventPeriodicStateChangeScan);

	return FSM_Continue;
}

CTask::FSM_Return CTaskCombat::WaitingForCoverExit_OnUpdate()
{
	// Check if we have reached our max time for this state and start our can change state at true
	bool bHasReachedMaxTime = GetTimeInState() >= ms_Tunables.m_fMaxWaitForCoverExitTime;
	bool bCanChangeState = true;

	// check for our cover use task and make sure we have not reached our max time yet
	CTaskCover* pCoverSubtask = static_cast<CTaskCover*>(FindSubTaskOfType(CTaskTypes::TASK_COVER));
	CTaskInCover* pInCoverSubtask = static_cast<CTaskInCover*>(FindSubTaskOfType(CTaskTypes::TASK_IN_COVER));
	if(pInCoverSubtask)
	{
		s32 iCoverState = pInCoverSubtask->GetState();
		if(!bHasReachedMaxTime)
		{
			// if we are not in aim or aim directly then we can't change states yet
			if (iCoverState != CTaskInCover::State_Aim)
			{
				bCanChangeState = false;
			}
		}
		else if(iCoverState == CTaskInCover::State_AimIntro)
		{
			bCanChangeState = false;
		}
	}
	else if(pCoverSubtask && pCoverSubtask->GetState() == CTaskCover::State_ExitCover)
	{
		bCanChangeState = false;
	}
	else if (GetPed()->IsNetworkClone() && !pCoverSubtask && !pInCoverSubtask)
	{
		bCanChangeState = true;
	}

	if(bCanChangeState)
	{
		if(IsStateStillValid(m_iDesiredState))
		{
			CPed* pPed = GetPed();

			// Release our current cover point so that the next state doesn't think we have a cover point
			pPed->ReleaseCoverPoint();

			// go ahead and tell the ped to keep aiming this frame and change our state
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_AimWeaponReactionRunning, true);

			if(ms_Tunables.m_EnableForcedFireForTargetProximity && m_iDesiredState == State_Fire)
			{
				m_uFlags.SetFlag(ComF_TemporarilyForceFiringState);
			}

			SetTaskCombatState(m_iDesiredState);
		}
		else
		{
			// otherwise we should stay in cover, if we have a cover task clear the aim intro request and keep our subtask
			if(pInCoverSubtask)
			{
				pInCoverSubtask->ClearCoverFlag(CTaskCover::CF_AimIntroRequested);
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			}

			SetTaskCombatState(State_InCover);
		}
	}

	return FSM_Continue;
}

void CTaskCombat::AimIntro_OnEnter()
{
	//Create the config flags.
	fwFlags8 uConfigFlags = 0;
	if(m_uFlags.IsFlagSet(ComF_UseFlinchAimIntro))
	{
		uConfigFlags.SetFlag(CTaskReactAimWeapon::CF_Flinch);
	}
	else if(m_uFlags.IsFlagSet(ComF_UseSurprisedAimIntro))
	{
		uConfigFlags.SetFlag(CTaskReactAimWeapon::CF_Surprised);
	}
	else if(m_uFlags.IsFlagSet(ComF_UseSniperAimIntro))
	{
		uConfigFlags.SetFlag(CTaskReactAimWeapon::CF_Sniper);
	}
	
	//Create the task.
	CTask* pTask = rage_new CTaskReactAimWeapon(m_pPrimaryTarget, uConfigFlags);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::AimIntro_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Calculate the next state, using the previous state.
		s32 iNextState = State_Finish;
		switch(GetPreviousState())
		{
			case State_Start:
			{
				if(m_uFlags.IsFlagSet(ComF_ArrestTarget) && CanArrestTarget())
				{
					//Don't force arrest.
					m_uRunningFlags.SetFlag(RF_DontForceArrest);

					//Arrest the target.
					iNextState = State_ArrestTarget;
				}
				else
				{
					iNextState = State_Start;
				}

				break;
			}
			default:
			{
				iNextState = State_Fire;
				break;
			}
		}
		
		//Move to the next state.
		SetTaskCombatState(iNextState);
	}

	return FSM_Continue;
}

void CTaskCombat::AimIntro_OnExit()
{
	//Clear the flags.
	m_uFlags.ClearFlag(ComF_UseFlinchAimIntro);
	m_uFlags.ClearFlag(ComF_UseSurprisedAimIntro);
	m_uFlags.ClearFlag(ComF_UseSniperAimIntro);
}

void CTaskCombat::TargetUnreachable_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskTargetUnreachable(CAITarget(m_pPrimaryTarget), CTaskTargetUnreachable::CF_QuitIfLosClear|CTaskTargetUnreachable::CF_QuitIfRouteFound);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::TargetUnreachable_OnUpdate()
{
	// Check if we changed our target
	if(CheckForTargetChangedThisFrame(false))
	{
		return FSM_Continue;
	}

	// We could have lost the target
	if (CheckForTargetLossResponse())
	{
		return FSM_Continue;
	}

	// Should do the return to initial position state?
	if(ShouldReturnToInitialPosition())
	{
		SetTaskCombatState(State_ReturnToInitialPosition);
		return FSM_Continue;
	}

	//Wait for the task to finish.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Fire, true) )
		{
			return FSM_Continue;
		}
	}

	// Update the target of the subtask (protects against the rare cases where the target change doesn't quit)
	CTaskTargetUnreachable* pTaskTargetUnreachable = static_cast<CTaskTargetUnreachable*>(FindSubTaskOfType(CTaskTypes::TASK_TARGET_UNREACHABLE));
	if(pTaskTargetUnreachable)
	{
		pTaskTargetUnreachable->SetTarget(CAITarget(m_pPrimaryTarget));
	}

	//Periodically check for a new state.
	if(PeriodicallyCheckForStateTransitions(ScaleReactionTime(GetPed(), AVERAGE_STATE_SEARCH_TIME)))
	{
		return FSM_Continue;
	}

	return FSM_Continue;
}

bool CTaskCombat::IsStateStillValid(s32 iState)
{
	// check for invalid states
	if(iState < 0)
	{
		return false;
	}
	else if(iState == State_MoveToCover)
	{
		// move to cover needs a valid desired cover point and it needs to be able to accommodate this ped
		if(!CanUseDesiredCover())
		{
			return false;
		}
	}
	else if(iState == State_ReactToImminentExplosion)
	{
		//Check if we can still react to the imminent explosion.
		if(!CanReactToImminentExplosion())
		{
			return false;
		}
	}
	else if(iState == State_ChaseOnFoot)
	{
		if(!ShouldChaseTarget(GetPed(), m_pPrimaryTarget))
		{
			return false;
		}
	}

	return true;
}

void CTaskCombat::MoveToCover_OnEnter(CPed* pPed)
{
	if(!pPed->IsNetworkClone())
	{
		if( !pPed->GetDesiredCoverPoint() || !pPed->GetDesiredCoverPoint()->CanAccomodateAnotherPed() )
		{
			return;
		}

		weaponAssert(pPed->GetWeaponManager());

		pPed->SetCoverPoint(pPed->GetDesiredCoverPoint());
		pPed->GetCoverPoint()->ReserveCoverPointForPed(pPed);
		pPed->SetDesiredCoverPoint(NULL);

		pPed->SetIsCrouching(false);

		// Reset our global move to cover timer
		ms_moveToCoverTimerGlobal.Reset();
	}

	// Update if we are in our defensive area now that we are using a cover point
	if(!m_uFlags.IsFlagSet(ComF_IsInDefensiveArea) && pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() && IsInDefensiveArea())
	{
		m_uFlags.SetFlag(ComF_IsInDefensiveArea);
	}

	s32 iFlags = CTaskCover::CF_QuitAfterCoverEntry|CTaskCover::CF_RunSubtaskWhenMoving|CTaskCover::CF_RunSubtaskWhenStationary|
		CTaskCover::CF_MoveToPedsCover|CTaskCover::CF_NeverEnterWater|CTaskCover::CF_AllowClimbLadderSubtask;

	// If already moving we should keep moving
	Vector2 vCurrentMbr;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMbr);
	if(vCurrentMbr.IsNonZero())
	{
		iFlags |= CTaskCover::CF_KeepMovingWhilstWaitingForPath;
	}

	float fDesiredMBR = MOVEBLENDRATIO_RUN;
	bool bShouldStrafe = true;
	CTaskCombat::ChooseMovementAttributes(pPed, bShouldStrafe, fDesiredMBR);

	s32 iCombatRunningFlags = CTaskCombat::GenerateCombatRunningFlags(bShouldStrafe, fDesiredMBR);

	// The second argument specifies whether a LOS is needed to the target from any cover point
	CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(m_pPrimaryTarget));
	pCoverTask->SetSearchFlags(iFlags);
	pCoverTask->SetMoveBlendRatio(fDesiredMBR);
	pCoverTask->SetSubtaskToUseWhilstSearching(	rage_new CTaskCombatAdditionalTask( iCombatRunningFlags, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()) ) );
	SetNewTask(pCoverTask);
}

CTask::FSM_Return CTaskCombat::MoveToCover_OnUpdate(CPed* pPed)
{
	// Don't change state if we're entering cover!
	if (FindSubTaskOfType(CTaskTypes::TASK_ENTER_COVER))
	{
		return FSM_Continue;
	}

	// Hack for B*1572354, prevent player from auto firing rpg in combat if they're under local player control
	TUNE_GROUP_BOOL(PLAYER_SWITCH_HACK, USE_HACK_1, true);
	if (USE_HACK_1 && pPed->IsLocalPlayer() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponInfo() 
		&& pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsRpg())
	{
		CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
		if (pGunTask)
		{
			pGunTask->SetWeaponControllerType(CWeaponController::WCT_Aim);
		}
		return FSM_Continue;
	}

	CCoverPoint* pCoverPoint = pPed->GetCoverPoint();
	if (pCoverPoint)
	{
		// Always transition to in cover state if we made it as this state deals with safely transitioning out
		if(pPed->GetIsInCover())
		{
			Vector3 vCoverCoords(Vector3::ZeroType);
			pCoverPoint->GetCoverPointPosition(vCoverCoords);
			Vector3 vPedToCover = vCoverCoords - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			
			if( vPedToCover.XYMag2() < ms_Tunables.m_MaxDistToCoverXY * ms_Tunables.m_MaxDistToCoverXY &&
				Abs(vPedToCover.z) < ms_Tunables.m_MaxDistToCoverZ )
			{
				SetTaskCombatState(State_InCover);
				return FSM_Continue;
			}
		}

		// We should stop running to cover if the cover point becomes dangerous, or is close to the player
		if(pCoverPoint->IsDangerous() || pCoverPoint->IsCloseToPlayer(*pPed))
		{
			// Find the new state from the table
			if( PickNewStateTransition(State_Fire, true) )
			{
				pPed->ReleaseCoverPoint();
				return FSM_Continue;
			}
		}
	}

	if (!pPed->GetWeaponManager()->GetRequiresWeaponSwitch() && HelperGetEquippedWeaponInfo())
	{
		if (HelperGetEquippedWeaponInfo()->GetIsMelee())
		{
			SetTaskCombatState(State_MeleeAttack);
			return FSM_Continue;
		}
	}

	// Check for some misc state changes that aren't handled by the transition table
	if(CheckForPossibleStateChanges(false))
	{
		return FSM_Continue;
	}

	UpdateGunTarget(false);

	// Possibly play an ambient combat animation
	UpdateAmbientAnimPlayback(pPed);

	// Mark our cover point as in use if we have one
	pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint, true );
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UsingCoverPoint, true );

	if( !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
		else
		{
			// Must have found the same state
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}

	CPedIntelligence* pIntelligence = pPed->GetPedIntelligence();
	if (pIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover) && !m_uFlags.IsFlagSet(ComF_IsInDefensiveArea) && m_goToAreaTimer.IsFinished() && CanMoveWithinDefensiveArea())
	{
		SetTaskCombatState(State_MoveWithinDefensiveArea);
	}

	// Periodically check for a new state. Most transitions from move to cover aren't allowed during the move and are protected by the state ended flag.
	// But some, like chasing targets in vehicle or on foot, reacting to explosions, etc should be allowed during the movement
	if( PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)) )
	{
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskCombat::Retreat_OnEnter(CPed* pPed)
{
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vToTarget = vPedPosition - VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
	vToTarget.RotateZ(fwRandom::GetRandomNumberInRange(-PI*0.25f,PI*0.25f));
	vToTarget.Scale(2.0f);
	SetNewTask(rage_new CTaskCombatSeekCover(m_pPrimaryTarget, vPedPosition+vToTarget, true));
	ResetRetreatTimer();
}

CTask::FSM_Return CTaskCombat::Retreat_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (m_retreatTimer.Tick(GetTimeStep()) || GetIsSubtaskFinished(CTaskTypes::TASK_COMBAT_SEEK_COVER))
	{
		m_coverSearchTimer.ForceFinish();
		SetTaskCombatState(State_Fire);
	}
	return FSM_Continue;
}

void CTaskCombat::MeleeAttack_OnEnter(CPed* pPed)
{
	Assert( pPed->GetWeaponManager() );
	const CWeaponInfo* pWeaponInfo = HelperGetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return;
	}

	if( pWeaponInfo->GetIsMelee() )
	{
		u32 uMeleeFlags = CTaskMelee::MF_ShouldBeInMeleeMode | CTaskMelee::MF_HasLockOnTarget | CTaskMelee::MF_AllowStrafeMode;
		if( m_uFlags.IsFlagSet( ComF_MeleeAnimPhaseSync ) )
		{
			uMeleeFlags |= CTaskMelee::MF_AnimSynced;
			uMeleeFlags |= CTaskMelee::MF_HasPerformedInitialPursuit;
		}
		else if( m_uFlags.IsFlagSet( ComF_MeleeImmediateThreat ) )
		{
			uMeleeFlags |= CTaskMelee::MF_HasPerformedInitialPursuit;
		}
		else
		{
			uMeleeFlags |= CTaskMelee::MF_PerformIntroAnim;
		}

		m_uFlags.ClearFlag( ComF_MeleeAnimPhaseSync );

		SetNewTask( rage_new CTaskMelee( NULL, const_cast<CPed*>(m_pPrimaryTarget.Get()), uMeleeFlags, CSimpleImpulseTest::ImpulseNone ) );
	}
	else
	{
		aiTask* pArmedMeleeRequestTask = CTaskMelee::CheckForAndGetMeleeAmbientMove( pPed, const_cast<CPed*>( m_pPrimaryTarget.Get() ), true, false, pWeaponInfo->GetAllowMeleeIntroAnim(), true );
		if( pArmedMeleeRequestTask )
			SetNewTask( pArmedMeleeRequestTask );
	}
	
	// We want to be able to check for transitions on our first frame of melee
	GetTransitionTableData()->SetLastStateSearchTimer(SMALL_FLOAT);

	// We want to be able to jack immediately if we are going into combat, so force our timer to end
	m_jackingTimer.ForceFinish();
}

CTask::FSM_Return CTaskCombat::MeleeAttack_OnUpdate( CPed* pPed )
{
	// Check if the melee task has changed our target
	CTaskMelee* pMeleeTask = static_cast<CTaskMelee*>(FindSubTaskOfType(CTaskTypes::TASK_MELEE));
	if(pMeleeTask && !pMeleeTask->IsCurrentlyDoingAMove(false))
	{
		CEntity* pMeleeTarget = pMeleeTask->GetTargetEntity();
		if(pMeleeTarget && pMeleeTarget != m_pPrimaryTarget.Get())
		{
			if( pMeleeTask->GetQueueType() == CTaskMelee::QT_InactiveObserver ||
				pMeleeTask->GetQueueType() == CTaskMelee::QT_SupportCombatant ||
				(pMeleeTarget->GetIsTypePed() && static_cast<CPed*>(pMeleeTarget)->IsInjured()) )
			{
				pMeleeTask->SetTargetEntity(const_cast<CPed*>(m_pPrimaryTarget.Get()));
			}
		}

		if(ShouldConsiderVictoriousInMelee())
		{
			SetTaskCombatState(State_Victory);
			return FSM_Continue;
		}

		if (ShouldDrawWeapon())
		{
			SetTaskCombatState(State_MeleeToArmed);
			return FSM_Continue;
		}
	}

	if (CheckForMovingWithinDefensiveArea(true))
	{
		return FSM_Continue;
	}

	if( GetIsSubtaskFinished(CTaskTypes::TASK_MELEE) )
	{
		if (!pPed->GetCurrentMotionTask()->IsOnFoot())
		{
			// Just stay in this state, Temporary until we know how we're going to handle space, underwater and ski melee
			return FSM_Continue;
		}

		PickNewStateTransition(State_Start, true );
		return FSM_Continue;
	}

	// Periodically check for a new state
	if( PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)) )
	{
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskCombat::MeleeAttack_OnExit( CPed* /*pPed*/ )
{
	const CWeaponInfo* pWeaponInfo = HelperGetEquippedWeaponInfo();
	if( pWeaponInfo && !pWeaponInfo->GetIsMelee() )
	{
		// Cache off the last time a non melee attack happened
		m_uTimeOfLastMeleeAttack = fwTimer::GetTimeInMilliseconds();
	}

	// Make sure to skip over any initial pursuit logic
	m_uFlags.SetFlag( ComF_MeleeImmediateThreat );
}

void CTaskCombat::Advance_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_ADVANCE))
	{
		return;
	}

	//Generate the config flags.
	fwFlags8 uConfigFlags = 0;
	if(m_uFlags.IsFlagSet(ComF_UseFrustratedAdvance))
	{
		uConfigFlags.SetFlag(CTaskAdvance::CF_IsFrustrated);
	}

	//Create the task.
	CTaskAdvance* pTask = rage_new CTaskAdvance(CAITarget(m_pPrimaryTarget), uConfigFlags);

	//Set the sub-task to use during movement.
	CTask* pSubTaskToUseDuringMovement = rage_new CTaskCombatAdditionalTask(
		CTaskCombatAdditionalTask::RT_Default | CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked | CTaskCombatAdditionalTask::RT_AllowAimFireToggle |
		CTaskCombatAdditionalTask::RT_AllowAimingOrFiringIfLosBlocked | CTaskCombatAdditionalTask::RT_MakeDynamicAimFireDecisions,
		m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
	pTask->SetSubTaskToUseDuringMovement(pSubTaskToUseDuringMovement);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::Advance_OnUpdate( CPed* pPed )
{
	// Check for some misc state changes that aren't handled by the transition table
	if(CheckForPossibleStateChanges(false))
	{
		return FSM_Continue;
	}

	// We need to return to the cover state first so we can do a proper cover exit
	if(ShouldReturnToInitialPosition() && CanPedBeConsideredInCover(pPed))
	{
		SetTaskCombatState(State_InCover);
		return FSM_Continue;
	}

	// Possibly play an ambient combat animation
	UpdateAmbientAnimPlayback(pPed);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
	}

	//Check if we can change state.
	CTaskAdvance* pTask = static_cast<CTaskAdvance *>(GetSubTask());
	bool bCanChangeState = (!pTask || !pTask->IsMovingToPoint() || IsPotentialStateChangeUrgent());

	// Periodically check for a new state
	if( bCanChangeState && PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)) )
	{
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskCombat::AdvanceFrustrated_OnEnter( CPed* pPed )
{
	//Use the frustrated advance.
	m_uFlags.SetFlag(ComF_UseFrustratedAdvance);

	pPed->NewSay("MOVE_IN_PERSONAL");

	Advance_OnEnter(pPed);
}

CTask::FSM_Return CTaskCombat::AdvanceFrustrated_OnUpdate( CPed* pPed )
{
	// Make sure to constantly let the targeting know that we want to ignore the cover position of our targets for LOS checks
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IgnoreTargetsCoverForLOS, true);

	if(CTaskCombat::ms_pFrustratedPed != pPed)
	{
		SetTaskCombatState(State_Start);
		return FSM_Continue;
	}

	// The rest of the behavior is currently the same as normal advance so go ahead and run that update
	return Advance_OnUpdate(pPed);
}

void CTaskCombat::AdvanceFrustrated_OnExit()
{
	m_uFlags.ClearFlag(ComF_UseFrustratedAdvance);

	ms_allowFrustrationTimerGlobal.Reset();
	CTaskCombat::ms_pFrustratedPed = NULL;
}

void CTaskCombat::ChargeTarget_OnEnter( CPed* pPed )
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_CHARGE))
	{
		return;
	}

	// If scripts requested this charge
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ShouldChargeNow ) )
	{
		// clear the flag now
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ShouldChargeNow, false);
	}

	// Check if the target is gone
	if( !m_pPrimaryTarget || !m_pPrimaryTarget.Get() ) 
	{ 
		return; // no more can be done, we'll exit this state immediately in the OnUpdate
	}

	// Get the target's player info
	// NOTE: we should have player info unless the charge was requested directly by scripts.
	//       In the scripts case the pPlayerInfo may be NULL
	CPlayerInfo* pPlayerInfo = m_pPrimaryTarget.Get()->GetPlayerInfo();

	// Initialize the charge goal at the target's position
	Vec3V vChargeGoalPos = m_pPrimaryTarget->GetTransform().GetPosition();

	// Check for a charge goal override, if any
	const Vec3V vChargeGoalOverride = pPed->GetPedIntelligence()->GetChargeGoalPositionOverride();
	bool bUsingOverride = false;
	if( !IsZeroAll(vChargeGoalOverride) )
	{
		// using override
		bUsingOverride = true;
		vChargeGoalPos = vChargeGoalOverride;

		// zero the override now that it has been used
		pPed->GetPedIntelligence()->SetChargeGoalPositionOverride(Vec3V(V_ZERO));
	}

	// Unless using override
	if( !bUsingOverride && pPlayerInfo )
	{
		// fetch a validated charge goal position if possible
		pPlayerInfo->GetValidatedChargeGoalPosition(pPed->GetTransform().GetPosition(), vChargeGoalPos);
	}

	// report this charging is underway
	if( pPlayerInfo )
	{
		pPlayerInfo->ReportEnemyStartedCharge();
	}

	// Record the last time we charged, which is right now
	m_uTimeWeLastCharged = fwTimer::GetTimeInMilliseconds();

	//Create the task.
	CTaskCharge* pChargeTask = rage_new CTaskCharge(vChargeGoalPos, m_pPrimaryTarget->GetTransform().GetPosition(), ms_Tunables.m_ChargeTuning.m_fChargeGoalCompletionRadius);

	//Create the sub-task to use during movement.
	CTask* pSubTaskToUseDuringMovement = rage_new CTaskCombatAdditionalTask(
		CTaskCombatAdditionalTask::RT_Default | CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked | CTaskCombatAdditionalTask::RT_AllowAimFireToggle |
		CTaskCombatAdditionalTask::RT_AllowAimingOrFiringIfLosBlocked | CTaskCombatAdditionalTask::RT_MakeDynamicAimFireDecisions,
		m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));

	//Set the sub-task to use during movement.
	pChargeTask->SetSubTaskToUseDuringMovement(pSubTaskToUseDuringMovement);

	pPed->NewSay("GENERIC_WAR_CRY");

	// Release desired coverpoint, if any
	pPed->ReleaseDesiredCoverPoint();

	SetNewTask(pChargeTask);
}

CTask::FSM_Return CTaskCombat::ChargeTarget_OnUpdate( CPed* pPed )
{
	// Check if the subtask finished
	if( GetIsSubtaskFinished(CTaskTypes::TASK_CHARGE) )
	{
		// force firing for a time
		m_uFlags.SetFlag(ComF_TemporarilyForceFiringState);
		// go to the firing state
		SetTaskCombatState(State_Fire);
		return FSM_Continue;
	}

	// Check for state changes
	const bool bRestartStateOnTargetChange = false;
	if( CheckForPossibleStateChanges(bRestartStateOnTargetChange) )
	{
		return FSM_Continue;
	}

	// Check for state specific stopping conditions
	if(CheckForStopChargingTarget())
	{
		return FSM_Continue;
	}

	// Make sure to constantly let the targeting know that we want to ignore the cover position of our targets for LOS checks
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IgnoreTargetsCoverForLOS, true);

	return FSM_Continue;
}

void CTaskCombat::ChargeTarget_OnExit( CPed* UNUSED_PARAM(pPed) )
{

}

void CTaskCombat::ChaseOnFoot_OnEnter( CPed* pPed )
{
	// Increase the number of peds chasing on foot
	ms_uNumPedsChasingOnFoot++;

	// Get the last seen position of our target and supply it to our follow nav mesh task
	Vector3 vTargetPos(Vector3::ZeroType);
	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(true);
	if( pPedTargetting && pPedTargetting->AreTargetsWhereaboutsKnown(&vTargetPos, m_pPrimaryTarget) &&
		vTargetPos.IsNonZero() )
	{
		if(fwRandom::GetRandomTrueFalse())
		{
			pPed->NewSay("FOOT_CHASE");
		}
		else
		{
			Vector3 pedMovement(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()));
			audDirection dirHeading = audScannerManager::CalculateAudioDirection(pedMovement);

			if(dirHeading == AUD_DIR_NORTH)
			{
				pPed->NewSay("FOOT_CHASE_HEADING_NORTH");
			}
			else if(dirHeading == AUD_DIR_EAST)
			{
				pPed->NewSay("FOOT_CHASE_HEADING_EAST");
			}
			else if(dirHeading == AUD_DIR_SOUTH)
			{
				pPed->NewSay("FOOT_CHASE_HEADING_SOUTH");
			}
			else if(dirHeading == AUD_DIR_WEST)
			{
				pPed->NewSay("FOOT_CHASE_HEADING_WEST");
			}
			else
			{
				pPed->NewSay("FOOT_CHASE");
			}
		}

		m_uTimeChaseOnFootSpeedSet = fwTimer::GetTimeInMilliseconds();
		float fDesiredMoveBlendRatio = m_pPrimaryTarget->GetMotionData()->GetIsSprinting() ? MOVEBLENDRATIO_SPRINT : MOVEBLENDRATIO_RUN;
		CTaskMoveFollowNavMesh* pMovementTask = rage_new CTaskMoveFollowNavMesh(fDesiredMoveBlendRatio, vTargetPos, 10.0f);
		pMovementTask->SetNeverEnterWater(true);

		//Allow peds to fire if chasing a vehicle on foot.
		bool bCanFire = m_pPrimaryTarget->GetIsInVehicle();
		s32 iCombatFlags = bCanFire ? CTaskCombatAdditionalTask::RT_Default : CTaskCombatAdditionalTask::RT_DefaultJustClips;
		iCombatFlags |= CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked;

		// Create a new control movement task using the follow nav mesh and the additional combat task
		SetNewTask(rage_new CTaskComplexControlMovement( pMovementTask, rage_new CTaskCombatAdditionalTask(iCombatFlags, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()) ) ) );
	}
}

CTask::FSM_Return CTaskCombat::ChaseOnFoot_OnUpdate( CPed* pPed )
{
	// If the target changed we should go back to the start state
	if(CheckForTargetChangedThisFrame(false))
	{
		return FSM_Continue;
	}
	
	//Check for target loss.
	if (CheckForTargetLossResponse())
	{
		return FSM_Continue;
	}

	// This makes sure we are within our defensive area, switching our state if we are outside of it
	if (CheckForMovingWithinDefensiveArea(true))
	{
		return FSM_Continue;
	}

	// If our subtask is finished or we are failing our navigation
	CTask* pSubTask = GetSubTask();
	if(!pSubTask || GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) || CheckForNavFailure(pPed))
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Start, true) )
		{
			return FSM_Continue;
		}
	}
	else if(pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskCombatAdditionalTask* pCombatTask = static_cast<CTaskCombatAdditionalTask*>(FindSubTaskOfType(CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK));
		bool bShouldFire = false;

		// Get the last seen position of the target
		Vector3 vTargetPos(Vector3::ZeroType);
		bool bHasTargetInfo = false;
		CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(true);
		if( pPedTargetting && pPedTargetting->AreTargetsWhereaboutsKnown(&vTargetPos, m_pPrimaryTarget, NULL, &bHasTargetInfo) &&
			bHasTargetInfo )
		{
			// Don't bother doing any of this if the additional combat task doesn't exist or if we are too far
			Vec3V vToTarget = VECTOR3_TO_VEC3V(vTargetPos) - pPed->GetTransform().GetPosition();
			ScalarV scDistSq = MagSquared(vToTarget);
			if(pCombatTask && IsLessThanOrEqualAll(scDistSq, ScalarVFromF32(square(MAX_DIST_TO_SHOOT_WHILE_CHASING))))
			{
				// if the target is not running then we should be firing
				if( m_pPrimaryTarget->GetMotionData()->GetIsLessThanRun())
				{
					bShouldFire = true;
				}
				else
				{
					// if the target is running we need to make sure they are running away from us, otherwise we should fire
					ScalarV scVelocityDot = Dot(VECTOR3_TO_VEC3V(m_pPrimaryTarget->GetVelocity()), vToTarget);
					if( IsLessThanOrEqualAll(scVelocityDot, ScalarV(V_ZERO)) )
					{
						bShouldFire = true;
					}
				}
			}

			// Like IV this is to offset where we are moving to so that peds doing the chase don't clump together, then set the new target pos
			CTaskComplexControlMovement* pComplexTask = static_cast<CTaskComplexControlMovement*>(pSubTask);
			vTargetPos += VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetA()) * (-0.5f + (float)( pPed->GetRandomSeed()/(float)RAND_MAX_16 )) * 7.0f;
			pComplexTask->SetTarget(pPed, vTargetPos);

			float fDesiredMoveBlendRatio = m_pPrimaryTarget->GetMotionData()->GetIsSprinting() ? MOVEBLENDRATIO_SPRINT : MOVEBLENDRATIO_RUN;
			if(pComplexTask->GetMoveBlendRatio() != fDesiredMoveBlendRatio && (fwTimer::GetTimeInMilliseconds() - m_uTimeChaseOnFootSpeedSet > ms_Tunables.m_MinTimeToChangeChaseOnFootSpeed))
			{
				// If target is running faster than us then play the foot chase losing line
				if(fDesiredMoveBlendRatio > pComplexTask->GetMoveBlendRatio() && pPed->IsLawEnforcementPed())
				{
					CPed* pRespondingLawPed = NULL;
					CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
					for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
					{
						CPed* pNearbyPed = static_cast<CPed*>(pEnt);
						if(pNearbyPed->IsLawEnforcementPed())
						{
							pRespondingLawPed = pNearbyPed;
							break;
						}
					}

					pPed->NewSay("FOOT_CHASE_LOSING", 0, false, false, -1, pRespondingLawPed, ATSTRINGHASH("FOOT_CHASE_RESPONSE", 0xA856B369));
				}

				m_uTimeChaseOnFootSpeedSet = fwTimer::GetTimeInMilliseconds();
				pComplexTask->SetMoveBlendRatio(pPed, fDesiredMoveBlendRatio);
			}
		}
		
		// if we have the combat task set the flags based on if we should be firing firing or not
		if(pCombatTask)
		{
			s32 iCombatFlags = bShouldFire ? CTaskCombatAdditionalTask::RT_Default : CTaskCombatAdditionalTask::RT_DefaultJustClips;
			iCombatFlags |= CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked;
			pCombatTask->SetFlags(iCombatFlags);
		}
	}

	// Periodically check for a new state
	if( PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)) )
	{
		return FSM_Continue;
	}

	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskCombat::ChaseOnFoot_OnExit()
{
	if(ms_uNumPedsChasingOnFoot > 0)
	{
		ms_uNumPedsChasingOnFoot--;
	}
	else
	{
		aiWarningf("Num peds chasing is <= 0 already, something went wrong.");
	}

	m_fTimeChaseOnFootDelayed = 0.0f;
}

void CTaskCombat::Flank_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskCombatFlank(m_pPrimaryTarget));
}

CTask::FSM_Return CTaskCombat::Flank_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (CheckForTargetLossResponse())
	{
		return FSM_Continue;
	}
	
	if (CheckForMovingWithinDefensiveArea(true))
	{
		return FSM_Continue;
	}

	if( GetIsSubtaskFinished(CTaskTypes::TASK_COMBAT_FLANK) )
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

void CTaskCombat::MoveWithinAttackWindow_OnEnter(CPed* pPed)
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_WITHIN_ATTACK_WINDOW)
	{
		return;
	}

	if(pPed->GetIsCrouching() && (!GetSubTask() || GetSubTask()->GetTaskType() != CTaskTypes::TASK_VARIED_AIM_POSE))
		m_uFlags.SetFlag(ComF_WasCrouched);
	pPed->SetIsCrouching(false);

	CTaskMoveWithinAttackWindow* pNewTask = rage_new CTaskMoveWithinAttackWindow(m_pPrimaryTarget);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskCombat::MoveWithinAttackWindow_OnUpdate(CPed* pPed)
{
	if (pPed->HasHurtStarted())
	{
		pPed->SetIsStrafing(true);
	}

	// Check for some misc state changes that aren't handled by the transition table
	if(CheckForPossibleStateChanges(true))
	{
		return FSM_Continue;
	}

	UpdateGunTarget(false);

	// Possibly play an ambient combat animation
	UpdateAmbientAnimPlayback(pPed);

	if( GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_WITHIN_ATTACK_WINDOW) )
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
	}
	// Periodically check for a new state
	if( PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)) )
	{
		return FSM_Continue;
	}

	//Activate timeslicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskCombat::MoveWithinAttackWindow_OnExit(CPed* pPed)
{
	pPed->SetIsCrouching(m_uFlags.IsFlagSet(ComF_WasCrouched));
	m_uFlags.ClearFlag(ComF_WasCrouched);

	if(GetState() != State_InCover && !GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		pPed->ReleaseCoverPoint();
	}
}

//-------------------------------------------------------------------------
// finds the nearest point in the peds defensive area to head too
//-------------------------------------------------------------------------
float CTaskCombat::GetClosestDefensiveAreaPointToNavigateTo( CPed* pPed, Vector3& vPoint, CDefensiveArea* pDefensiveAreaToUse )
{
	CDefensiveArea* pDefensiveArea = pDefensiveAreaToUse ? pDefensiveAreaToUse : pPed->GetPedIntelligence()->GetDefensiveArea();
	Assert( pDefensiveArea && pDefensiveArea->IsActive() );

	// Get the 2 vector definition of the defensive area
	Vector3 vVec1, vVec2, vCenter;
	float fWidthX, fWidthY;
	pDefensiveArea->Get( vVec1, vVec2, fWidthY );
	float fSmallestRadius = fWidthY;
	if( pDefensiveArea->GetType() == CDefensiveArea::AT_AngledArea )
	{
		vCenter = vVec1 + vVec2;
		vCenter.Scale(0.5f);

		// Just set the position to the center if we should be using the center position
		if( pDefensiveArea->GetUseCenterAsGoToPosition() )
		{
			vPoint = vCenter;
		}
		else
		{		
			fWidthX = (vVec1 - vVec2).XYMag();

			fSmallestRadius = Min(fWidthX, fWidthY);
			// Get the 2 direction vectors
			Vector3 vDirX, vDirY;
			vDirX = vVec2 - vVec1;
			vDirX.z = 0.0f;
			vDirX.Normalize();
			vDirY.Cross( vDirX, Vector3( 0.0f, 0.0f, 1.0f ) );
			vDirY.z = 0.0f;
			vDirY.Normalize();

			// Work out the direction toward the ped
			Vector3 vDirToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vCenter;
			vDirToPed.RotateZ(fwRandom::GetRandomNumberInRange(-ms_Tunables.m_MaxMoveToDefensiveAreaAngleVariation, ms_Tunables.m_MaxMoveToDefensiveAreaAngleVariation));
			vDirToPed.z = 0.0f;
			vDirToPed.Normalize();

			// Project the estimated position out towards the edge of the area
			float fSafetyProportion = fwRandom::GetRandomNumberInRange(ms_Tunables.m_SafetyProportionInDefensiveAreaMin, ms_Tunables.m_SafetyProportionInDefensiveAreaMax);
			vPoint = vDirToPed.Dot(vDirX) * vDirX * fSafetyProportion * fWidthX * 0.5f;
			vPoint += vDirToPed.Dot(vDirY) * vDirY * fSafetyProportion * fWidthY * 0.5f;
			vPoint += vCenter;
		}
	}
	else if( pDefensiveArea->GetType() == CDefensiveArea::AT_Sphere )
	{
		// Just set the position to the center if we should be using the center position
		if( pDefensiveArea->GetUseCenterAsGoToPosition() )
		{
			vPoint = vVec1;
		}
		else
		{
			// Work out the direction toward the ped
			vCenter = vVec1;
			Vector3 vDirToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vCenter;
			vDirToPed.RotateZ(fwRandom::GetRandomNumberInRange(-ms_Tunables.m_MaxMoveToDefensiveAreaAngleVariation, ms_Tunables.m_MaxMoveToDefensiveAreaAngleVariation));
			vDirToPed.z = 0.0f;
			vDirToPed.Normalize();

			float fSafetyProportion = fwRandom::GetRandomNumberInRange(ms_Tunables.m_SafetyProportionInDefensiveAreaMin, ms_Tunables.m_SafetyProportionInDefensiveAreaMax);
			vDirToPed.Scale(fSafetyProportion * fWidthY);
			vPoint = vDirToPed + vCenter;
		}
	}

	return fSmallestRadius;
}

void CTaskCombat::MoveWithinDefensiveArea_OnEnter(CPed* pPed)
{
	ResetGoToAreaTimer();

	if(pPed->GetIsCrouching() && (!GetSubTask() || GetSubTask()->GetTaskType() != CTaskTypes::TASK_VARIED_AIM_POSE))
		m_uFlags.SetFlag(ComF_WasCrouched);

	pPed->SetIsCrouching(false);

	CTaskMoveWithinDefensiveArea* pNewTask = rage_new CTaskMoveWithinDefensiveArea(m_pPrimaryTarget);
	pNewTask->GetConfigFlags().SetFlag(CTaskMoveWithinDefensiveArea::CF_DontSearchForCover);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskCombat::MoveWithinDefensiveArea_OnUpdate(CPed* pPed)
{
	// Check for some misc state changes that aren't handled by the transition table
	if(CheckForPossibleStateChanges(true))
	{
		return FSM_Continue;
	}

	UpdateGunTarget(false);

	// Possibly play an ambient combat animation
	UpdateAmbientAnimPlayback(pPed);

	if( GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_WITHIN_DEFENSIVE_AREA))
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
	}

	// Periodically check for a new state
	if(PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)))
	{
		return FSM_Continue;
	}

	//Activate timeslicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskCombat::MoveWithinDefensiveArea_OnExit(CPed* pPed)
{
	pPed->SetIsCrouching(m_uFlags.IsFlagSet(ComF_WasCrouched));
	m_uFlags.ClearFlag(ComF_WasCrouched);

	if(GetState() != State_InCover)
	{
		pPed->ReleaseCoverPoint();
	}

	m_coverSearchTimer.ForceFinish();
}

void CTaskCombat::PersueInVehicle_OnEnter( CPed* pPed )
{
	taskAssertf(pPed->GetVehiclePedInside(), "Task started when not in a vehicle!"); 

	CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea();
	if (m_bPersueVehicleTargetOutsideDefensiveArea && pDefensiveArea && pDefensiveArea->IsActive())
	{
		Vector3 vCentre;
		pDefensiveArea->GetCentre(vCentre);

		aiTask *pVehicleTask = CVehicleIntelligence::GetGotoTaskForVehicle(pPed->GetVehiclePedInside(), NULL, &vCentre, 0, 2.0f, 0.0f, 20.f);
		SetNewTask(rage_new CTaskControlVehicle(pPed->GetVehiclePedInside(), pVehicleTask));
	}
	else
	{
		CAITarget target(m_pPrimaryTarget);
		SetNewTask( rage_new CTaskVehiclePersuit(target, m_uConfigFlagsForVehiclePursuit) );
	}
}

CTask::FSM_Return CTaskCombat::PersueInVehicle_OnUpdate( CPed* pPed )
{
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StayInDefensiveAreaWhenInVehicle))
	{
		CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea();
		if( !m_bPersueVehicleTargetOutsideDefensiveArea && pDefensiveArea && pDefensiveArea->IsActive() && m_pPrimaryTarget)
		{
			Vector3 vCentre;
			float fRadiusSq;
			pDefensiveArea->GetCentreAndMaxRadius(vCentre,fRadiusSq);

			fRadiusSq *= 1.1f; // Increase the max distance by 10% to avoid oscillation

			float fDistSq = vCentre.Dist2(VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
			if (fDistSq > fRadiusSq)
			{
				m_bPersueVehicleTargetOutsideDefensiveArea = true;
			}
		}

		if (m_bPersueVehicleTargetOutsideDefensiveArea)
		{
			if (GetIsSubtaskFinished(CTaskTypes::TASK_CONTROL_VEHICLE))
			{
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::PersueInVehicle_OnUpdate, Ped %s, Aborting TaskCombat. Target outside defensive area when in vehicle.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				return FSM_Quit;
			}

			bool bControlVehicleSubtaskRunning = GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE;
			if (GetTimeInState() > (fwTimer::GetTimeStep() * 2.0f) && !bControlVehicleSubtaskRunning)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}

			if (!GetSubTask() && GetTimeInState() > 10.f)
			{
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::PersueInVehicle_OnUpdate, Ped %s, Aborting TaskCombat. Target outside defensive area when in vehicle, (sub task timeout).\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				return FSM_Quit;
			}

			return FSM_Continue;
		}
	}

	//Check for target loss.
	if(CheckForTargetLossResponse())
	{
		return FSM_Continue;
	}
	
	if((!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_PERSUIT)) && (GetTimeInState() >= 0.25f))
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Start, true) )
		{
			return FSM_Continue;
		}
		else
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}

	if (pPed->GetIsInVehicle() && pPed->GetMyVehicle()->GetSeatAnimationInfo(pPed)->IsTurretSeat())
	{
		float fMaxAttackRange = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxVehicleTurretFiringRange);
		if (fMaxAttackRange > 0.0f)
		{
			ScalarV svDistSq = DistSquared(m_pPrimaryTarget->GetTransform().GetPosition(),pPed->GetTransform().GetPosition());
			ScalarV svMaxDistSq =  ScalarVFromF32(fMaxAttackRange*fMaxAttackRange);
			if (IsGreaterThanAll(svDistSq,svMaxDistSq))
			{
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::PersueInVehicle_OnUpdate, Ped %s, Aborting TaskCombat. Outside turrent attack range.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				SetTaskCombatState(State_Finish);
				return FSM_Continue;
			}
		}
	}

	// Keep any vehicle combat tasks target up to date.
	CTaskVehiclePersuit* pVehiclePersuit = static_cast<CTaskVehiclePersuit*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_PERSUIT));
	if( pVehiclePersuit && pVehiclePersuit->GetTarget() != m_pPrimaryTarget && m_pPrimaryTarget != NULL)
	{
		CAITarget target(m_pPrimaryTarget);
		pVehiclePersuit->SetTarget(target);
	}

	return FSM_Continue;
}

void CTaskCombat::PersueInVehicle_OnExit( CPed* pPed )
{
	//Clear the flags that only apply once.
	m_uConfigFlagsForVehiclePursuit.ClearFlag(CTaskVehiclePersuit::CF_ExitImmediately);

	// Force our burst to be ready to fire
	pPed->GetPedIntelligence()->GetFiringPattern().ResetTimeUntilNextBurst();

	// Reset our ambient anim allowed time so we don't play an anim immediately after exiting which would end up preventing our firing
	m_fTimeUntilAmbientAnimAllowed = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenGesturesMinGlobal, ms_Tunables.m_fTimeBetweenGesturesMaxGlobal);
}

static dev_float DISTANCE_TO_RESTART_SEEK_TASK = 8.0f;

void CTaskCombat::WaitingForEntryPointToBeClear_OnEnter()
{
	CPed* pPed = GetPed();

	pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));

	if (GetPreviousState() != State_WaitingForEntryPointToBeClear)
	{
		m_fTimeWaitingForClearEntry = 0.0f;
		m_uTimeOfLastJackAttempt = fwTimer::GetTimeInMilliseconds();
	}

	if (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist2(VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition())) > rage::square(DISTANCE_TO_RESTART_SEEK_TASK))
	{
		static dev_float DISTANCE_TO_GET_WITHIN_RANGE = 3.5f;
		TTaskMoveSeekEntityStandard * pSeekEntityTask = rage_new TTaskMoveSeekEntityStandard(
			m_pPrimaryTarget,
			-1,
			TTaskMoveSeekEntityStandard::ms_iPeriod,
			DISTANCE_TO_GET_WITHIN_RANGE
			);

		// Try to get within range
		SetNewTask(rage_new CTaskComplexControlMovement(pSeekEntityTask));
	}
	else
	{
		const CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
		if( pWeaponMgr )
		{
			const CWeaponInfo* pWeaponInfo = pWeaponMgr->GetEquippedWeaponInfo();
			if( pWeaponInfo && pWeaponInfo->GetIsMelee() )
			{
				u32 uMeleeFlags = CTaskMelee::MF_ShouldBeInMeleeMode | CTaskMelee::MF_HasLockOnTarget | CTaskMelee::MF_ForceInactiveTauntMode;
				SetNewTask( rage_new CTaskMelee( NULL, const_cast<CPed*>(m_pPrimaryTarget.Get()), uMeleeFlags ) );
			}
			else if( pWeaponMgr->GetIsArmedGun() )
			{
				s32 iCombatRunningFlags = CTaskCombatAdditionalTask::RT_Default|CTaskCombatAdditionalTask::RT_AllowAimFireToggle;
				SetNewTask(rage_new CTaskCombatAdditionalTask(iCombatRunningFlags, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition())));
			}
		}
	}

	// Used to test for open vehicle entry point
	m_jackingTimer.ForceFinish();
}

CTask::FSM_Return CTaskCombat::WaitingForEntryPointToBeClear_OnUpdate()
{
	//static dev_float TIME_BETWEEN_CLEAR_ENTRY_CHECKS = 1.0f;
	static dev_float MAX_TIME_TO_WAIT_FOR_CLEAR_ENTRY = 45.0f;

	m_fTimeWaitingForClearEntry += GetTimeStep();

	CPed* pPed = GetPed();

	if( m_fTimeWaitingForClearEntry > MAX_TIME_TO_WAIT_FOR_CLEAR_ENTRY )
	{
		if( pPed->GetPedIntelligence()->GetCombatBehaviour().GetTargetLossResponse() == CCombatData::TLR_ExitTask )
		{
#if __BANK
			CAILogManager::GetLog().Log("CTaskCombat::WaitingForEntryPointToBeClear_OnUpdate, Ped %s, Aborting TaskCombat.\n", 
				AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

			SetTaskCombatState( State_Finish );
			return FSM_Continue;
		}
	}

	// Check to see if we are close enough to restart the state
	CTask* pSubtask = GetSubTask();
	if( !pSubtask || ( pSubtask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT && VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition()).Dist2( VEC3V_TO_VECTOR3( m_pPrimaryTarget->GetTransform().GetPosition() ) ) < rage::square( DISTANCE_TO_RESTART_SEEK_TASK ) ) )
	{
		// Set the state so the previous state is set
		SetTaskCombatState( State_WaitingForEntryPointToBeClear );
		SetFlag( aiTaskFlags::RestartCurrentState );
		return FSM_Continue;
	}

	if( !m_pPrimaryTarget->GetIsInVehicle() )
	{
		SetTaskCombatState( State_Start );
		return FSM_Continue;
	}

	if( CheckForTargetLossResponse() )
		return FSM_Continue;

	if(CheckForGiveUpOnWaitingForEntryPointToBeClear())
	{
		SetTaskCombatState(State_Victory);
		return FSM_Continue;
	}

	// Ignore the time in state check if this is our first time 
	m_jackingTimer.Tick(GetTimeStep());
	if( m_jackingTimer.IsFinished() )
	{
		ResetJackingTimer();

		CVehicle* pTargetsVehicle = m_pPrimaryTarget->GetVehiclePedInside();
		if( taskVerifyf( pTargetsVehicle, "Expected target to be in a vehicle" ) )
		{
			if(pTargetsVehicle->IsUpsideDown() || pTargetsVehicle->IsOnItsSide())
			{
				SetTaskCombatState( State_Start );
				return FSM_Continue;
			}

			s32 iTargetEntryPoint = GetTargetsEntryPoint( *pTargetsVehicle );
			if( pTargetsVehicle->IsEntryPointIndexValid(iTargetEntryPoint) )
			{
				if( !pPed->GetTaskData().GetIsFlagSet( CTaskFlags::DisableVehicleUsage ) && CVehicle::IsEntryPointClearForPed( *pTargetsVehicle, *pPed, iTargetEntryPoint) )
				{
					SetTaskCombatState( State_GoToEntryPoint );
					return FSM_Continue;
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskCombat::GoToEntryPoint_OnEnter()
{
	CVehicle* pVehicle = m_pPrimaryTarget->GetVehiclePedInside();
	if (pVehicle)
	{
		s32 iTargetEntryPoint = GetTargetsEntryPoint(*pVehicle);
		if (pVehicle->IsEntryPointIndexValid(iTargetEntryPoint))
		{
			CPed* pPed = GetPed();

			CTaskMoveGoToVehicleDoor * pMoveTask = rage_new CTaskMoveGoToVehicleDoor(pVehicle, MOVEBLENDRATIO_RUN, true, iTargetEntryPoint,
				CTaskGoToCarDoorAndStandStill::ms_fTargetRadius, CTaskGoToCarDoorAndStandStill::ms_fSlowDownDistance,
				CTaskGoToCarDoorAndStandStill::ms_fMaxSeekDistance,	CTaskGoToCarDoorAndStandStill::ms_iMaxSeekTime,
				true);

			CTask* pCombatTask = NULL;
			if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetIsArmed())
			{
				s32 iCombatRunningFlags = CTaskCombatAdditionalTask::RT_Default|CTaskCombatAdditionalTask::RT_AllowAimFireToggle;
				pCombatTask = rage_new CTaskCombatAdditionalTask(iCombatRunningFlags, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
			}

			CTaskComplexControlMovement * pCtrlMove = rage_new CTaskComplexControlMovement(pMoveTask, pCombatTask);
			SetNewTask(pCtrlMove);
		}
	}
}

CTask::FSM_Return CTaskCombat::GoToEntryPoint_OnUpdate()
{
	// If our target changed we need to restart our task
	if(CheckForTargetChangedThisFrame(false))
	{
		return FSM_Continue;
	}

	if(CheckForTargetLossResponse())
	{
		return FSM_Continue;
	}

	if(CheckForGiveUpOnGoToEntryPoint())
	{
		SetTaskCombatState(State_Victory);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	// Find the vehicle we are in or are entering
	CVehicle* pVehicle = m_pPrimaryTarget->GetVehiclePedInside();
	if (!m_pPrimaryTarget->IsExitingVehicle() && pVehicle && !pVehicle->IsUpsideDown() && !pVehicle->IsOnItsSide())
	{
		bool bEntryPointValid = false;
		s32 iTargetsEntryPoint = GetTargetsEntryPoint(*pVehicle);
		if (pVehicle->IsEntryPointIndexValid(iTargetsEntryPoint))
		{
			bEntryPointValid = true;
		}

		bool bIsCloseEnough = false;
		if (bEntryPointValid)
		{
			// Get the entry position
			Vector3 vEntryPosition;
			Quaternion qEntryOrientation;
			CModelSeatInfo::CalculateEntrySituation(pVehicle, pPed, vEntryPosition, qEntryOrientation, iTargetsEntryPoint);
			Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			if ((vPedPosition-vEntryPosition).Mag2() < square(ms_Tunables.m_TargetJackRadius))
			{
				bIsCloseEnough = true;
			}
		}

		bool bIsSubTaskFinished = !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished);
		if (bIsCloseEnough || bIsSubTaskFinished)
		{
			if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
			{
				SetTaskCombatState(State_Start);
				return FSM_Continue;
			}

			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceMeleeStrafingAnims, true);
			pPed->SetIsStrafing(true);
			pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));

			if (bEntryPointValid)
			{
				s32 iTargetSeat = pVehicle->GetEntryExitPoint(iTargetsEntryPoint)->GetSeat(SA_directAccessSeat);
				CPed* pPedInSeat = pVehicle->GetSeatManager()->GetPedInSeat(iTargetSeat);
				if (pPedInSeat)
				{
					// If the ped in the seat is our target then try to pull from vehicle, otherwise restart combat because something went wrong
					if(pPedInSeat == m_pPrimaryTarget)
					{
						SetTaskCombatState(State_PullTargetFromVehicle);
					}
					else
					{
						SetTaskCombatState(State_Start);
					}

					return FSM_Continue;
				}
			}
			
			// At this point if our subtask is finished it means we probably won't be getting to the door properly
			if(bIsSubTaskFinished)
			{
				SetState(State_Start);
				return FSM_Continue;
			}
		}
		//Check if the vehicle is not moving slow enough to continue jacking.
		else if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetIsArmed() &&
				!IsVehicleMovingSlowerThanSpeed(*pVehicle, ms_Tunables.m_MaxSpeedToContinueJackingVehicle))
		{
			m_uTimeTargetVehicleLastMoved = fwTimer::GetTimeInMilliseconds();
			SetTaskCombatState(State_Start);
		}
		// We don't want to do this every frame but it's not something that needs a timer as it doesn't really need to be tunable
		else if(fwTimer::GetSystemFrameCount() % 4 == 0)
		{
			// Check if the entry point is still clear, if it isn't then we should be waiting for it to be clear
			if( pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableVehicleUsage) ||
				!pVehicle->IsEntryPointIndexValid(iTargetsEntryPoint) ||
				!CVehicle::IsEntryPointClearForPed(*pVehicle, *pPed, iTargetsEntryPoint) )
			{
				SetTaskCombatState(State_WaitingForEntryPointToBeClear);
				return FSM_Continue;
			}
		}
	}
	else
	{
		SetTaskCombatState(State_Start);
	}

	return FSM_Continue;
}

void CTaskCombat::PullTargetFromVehicle_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CVehicle* pVehicle = m_pPrimaryTarget->GetVehiclePedInside();
	if (pVehicle)
	{
		s32 iTargetEntryPoint = GetTargetsEntryPoint(*pVehicle);
		if (pVehicle->IsEntryPointIndexValid(iTargetEntryPoint))
		{
			const CEntryExitPoint* pEntryPoint = pVehicle->GetEntryExitPoint(iTargetEntryPoint);
			if (pEntryPoint)
			{
				s32 iSeat = pEntryPoint->GetSeat(SA_directAccessSeat);

				VehicleEnterExitFlags vehicleFlags;
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::CombatEntry);

				bool bJustPullPedOut = !m_uFlags.IsFlagSet(ComF_QuitIfDriverJacked) || (iSeat != Seat_driver);
				if(bJustPullPedOut)
				{
					vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JustPullPedOut);
				}

				SetNewTask(rage_new CTaskEnterVehicle(pVehicle, SR_Specific, iSeat, vehicleFlags));
			}
		}

		if (pVehicle == GetPed()->GetMyVehicle())
		{
			GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_WentIntoCombatAfterBeingJacked, true);
		}
	}
}

CTask::FSM_Return CTaskCombat::PullTargetFromVehicle_OnUpdate( CPed* pPed )
{
	//Ensure the sub-task is valid.
	if(!GetSubTask())
	{
		SetTaskCombatState(State_Start);
		return FSM_Continue;
	}

	//Check if the jack is successful.
	if(!m_pPrimaryTarget->GetIsInVehicle() || m_pPrimaryTarget->IsBeingJackedFromVehicle())
	{
		HoldFireFor(ms_Tunables.m_TimeToHoldFireAfterJack);
	}

	// If our sub task is finished then we should probably go back to start state
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we are driving the vehicle.
		if(GetPed()->GetIsDrivingVehicle())
		{
			//Check if we want to quit if the driver is jacked.
			if(m_uFlags.IsFlagSet(ComF_QuitIfDriverJacked))
			{
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::PullTargetFromVehicle_OnUpdate, Ped %s, Aborting TaskCombat - ComF_QuitIfDriverJacked.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

				SetTaskCombatState(State_Finish);
				return FSM_Continue;
			}
			else
			{
				//We jacked the vehicle back, but the jack was not interruptible, so we couldn't just pull them out.
				//Flee, if it's an option, because getting back out here looks bad.
				bool bCanFlee = (GetPed()->PopTypeIsRandom() && !GetPed()->IsLawEnforcementPed());
				if(bCanFlee)
				{
					//Check if our parent is threat response.
					if(GetParent() && (GetParent()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE))
					{
						//Flee in the vehicle after jack.
						CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(GetParent());
						pTaskThreatResponse->FleeInVehicleAfterJack();

#if __BANK
						CAILogManager::GetLog().Log("CTaskCombat::PullTargetFromVehicle_OnUpdate, Ped %s, Aborting TaskCombat - bCanFlee.\n", 
							AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

						SetTaskCombatState(State_Finish);
						return FSM_Continue;
					}
				}
			}
		}

		SetTaskCombatState(State_Start);
		return FSM_Continue;
	}

	//Get the enter vehicle task.
	taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE);
	const CTaskEnterVehicle* pTaskEnterVehicle = static_cast<CTaskEnterVehicle *>(GetSubTask());

	//Check if we are just pulling the ped out.
	bool bJustPullPedOut = pTaskEnterVehicle->IsFlagSet(CVehicleEnterExitFlags::JustPullPedOut);

	// If ped gets out of car or CTaskEnterVehicle finishes, restart and decide what to do
	if(!m_pPrimaryTarget->GetIsInVehicle() && bJustPullPedOut)
	{
		// Don't want to just abort if we're in the middle of something
		if (!pPed->GetIsAttached() || !pTaskEnterVehicle || pTaskEnterVehicle->GetState() == CTaskEnterVehicle::State_OpenDoor || pTaskEnterVehicle->GetState() == CTaskEnterVehicle::State_Align)
		{
			CTaskExitVehicleSeat* pExitVehicleTask = static_cast<CTaskExitVehicleSeat* >(m_pPrimaryTarget->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
			if(!pExitVehicleTask || !pExitVehicleTask->IsBeingArrested())
			{
				SetTaskCombatState( State_Start );
				return FSM_Continue;
			}
		}
	}

	if(CheckForTargetLossResponse())
	{
		return FSM_Continue;
	}

	const float fDistanceToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition()).Getf();

	// if target is on a train or if we are further away from the target than we should be, try to fire at the opponent
	CVehicle* pTargetsVehicle = m_pPrimaryTarget->GetMyVehicle();
	if(pTargetsVehicle)
	{
		if(pTargetsVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN || fDistanceToTargetSq > rage::square(CAR_JACK_DISTANCE_STOP))
		{
			const CWeaponInfo* pBestWeapon = pPed->GetWeaponManager()->GetBestWeaponInfo();
			if( pBestWeapon && pBestWeapon->GetIsGun() )
			{
				pPed->GetWeaponManager()->EquipBestWeapon( true );
				SetTaskCombatState( State_Fire );
				return FSM_Continue;
			}
		}
		//Check if the vehicle is not moving slow enough to continue jacking.
		else if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetIsArmed() &&
				!IsVehicleMovingSlowerThanSpeed(*pTargetsVehicle, ms_Tunables.m_MaxSpeedToContinueJackingVehicle))
		{
			m_uTimeTargetVehicleLastMoved = fwTimer::GetTimeInMilliseconds();
			SetTaskCombatState(State_Start);
		}
	}

	return FSM_Continue;
}


void CTaskCombat::EnterVehicle_OnEnter(CPed* pPed)
{
	if(m_pVehicleToUseForChase)
	{
		// If our target is only entering their vehicle then this must be an early vehicle entry
		if(m_pPrimaryTarget->GetIsEnteringVehicle())
		{
			m_uFlags.SetFlag(ComF_DoingEarlyVehicleEntry);
			ms_uNumDriversEnteringVehiclesEarly++;
		}

		// Figure out our desired seat index and request type
		bool bUsingMyVehicle = (m_pVehicleToUseForChase == pPed->GetMyVehicle());
		s32 seatIndex = bUsingMyVehicle ? m_pVehicleToUseForChase->GetSeatManager()->GetLastPedsSeatIndex(pPed) : m_pVehicleToUseForChase->GetDriverSeat();
		if(seatIndex != m_pVehicleToUseForChase->GetDriverSeat())
		{
			CPed* pDriver = m_pVehicleToUseForChase->GetDriver();
			if(!pDriver)
			{
				pDriver = m_pVehicleToUseForChase->GetSeatManager()->GetLastPedInSeat(m_pVehicleToUseForChase->GetDriverSeat());
			}

			if(!pDriver || pDriver->IsInjured())
			{
				bool bDriverSeatAvailable = true;
				const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
				for( const CEntity* pEnt = entityList.GetFirst(); pEnt && bDriverSeatAvailable; pEnt = entityList.GetNext() )
				{
					const CPed* pNearbyPed = static_cast<const CPed*>(pEnt);
					
					CTaskEnterVehicle* pEnterTask = static_cast<CTaskEnterVehicle*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
					if (pEnterTask && pEnterTask->GetVehicle() == m_pVehicleToUseForChase && pEnterTask->GetTargetSeat() == m_pVehicleToUseForChase->GetDriverSeat())
					{
						bDriverSeatAvailable = false;
					}
				}

				if(bDriverSeatAvailable)
				{
					seatIndex = m_pVehicleToUseForChase->GetDriverSeat();
				}
			}
		}

		SeatRequestType requestType = (seatIndex >= 0) ? SR_Specific : SR_Any;

		SetNewTask(rage_new CTaskEnterVehicle(m_pVehicleToUseForChase.Get(), requestType, seatIndex));
	}
}

CTask::FSM_Return CTaskCombat::EnterVehicle_OnUpdate(CPed* pPed)
{
	// If we lost our desired vehicle somehow or our target is no longer in a vehicle then go ahead and restart combat
	if( !m_pVehicleToUseForChase ||
		(!m_pPrimaryTarget->GetIsInVehicle() && !m_pPrimaryTarget->GetIsOnMount()) )
	{
		SetTaskCombatState(State_Start);
		return FSM_Continue;
	}

	// First get our driver (or the last ped in the driver seat)
	CPed* pDriver = m_pVehicleToUseForChase->GetDriver();
	if(!pDriver)
	{
		pDriver = m_pVehicleToUseForChase->GetSeatManager()->GetLastPedInSeat(m_pVehicleToUseForChase->GetDriverSeat());
	}

	// Check if our driver is valid (different cases for if our vehicle we will use is our existing vehicle or if we are commandeering
	bool isTaskValid = false;
	if(m_pVehicleToUseForChase == pPed->GetMyVehicle())
	{
		isTaskValid = !pDriver || (!pDriver->IsDead() && pPed->GetPedIntelligence()->IsFriendlyWith(*pDriver));
	}
	else if(IsVehicleMovingSlowerThanSpeed(*m_pVehicleToUseForChase, ms_Tunables.m_MaxSpeedToContinueJackingVehicle))
	{
		isTaskValid = !pDriver || !pPed->GetPedIntelligence()->IsFriendlyWith(*pDriver);
	}

	if(!isTaskValid || !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go ahead and quit out if we made it into a vehicle
		if(pPed->GetVehiclePedInside())
		{
			SetTaskCombatState(State_PersueInVehicle);
		}
		else
		{
			SetTaskCombatState(State_Start);
		}
	}
	//Check if we should warp if possible.
	else if(ShouldWarpIntoVehicleIfPossible())
	{
		//Check if the sub-task is valid.
		CTask* pSubTask = GetSubTask();
		if(pSubTask)
		{
			//Warp if possible.
			taskAssert(pSubTask->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE);
			CTaskEnterVehicle* pTaskEnterVehicle = static_cast<CTaskEnterVehicle *>(pSubTask);
			pTaskEnterVehicle->WarpIfPossible();
		}
	}

	return FSM_Continue;
}

void CTaskCombat::EnterVehicle_OnExit()
{
	ClearEarlyVehicleEntry();
}

void CTaskCombat::Search_OnEnter( CPed* pPed )
{
	//Create the params.
	CTaskSearch::Params params;
	CTaskSearch::LoadDefaultParamsForPedAndTarget(*pPed, *m_pPrimaryTarget, params);
	
	//Create the task.
	CTask* pTask = rage_new CTaskSearch(params);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::Search_OnUpdate( CPed* pPed )
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_SEARCH) )
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
	}
	// Periodically check for a new state
	if( PeriodicallyCheckForStateTransitions(ScaleReactionTime(pPed, AVERAGE_STATE_SEARCH_TIME)) )
	{
		return FSM_Continue;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskCombat::DecideTargetLossResponse_OnUpdate( CPed* pPed )
{
	//Say the audio.
	if(!m_pPrimaryTarget->ShouldBeDead())
	{
		GetPed()->NewSay("SUSPECT_LOST");
	}

	CCombatBehaviour combatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();
	switch (combatBehaviour.GetTargetLossResponse())
	{
		case CCombatData::TLR_ExitTask:
		{
#if __BANK
			CAILogManager::GetLog().Log("CTaskCombat::DecideTargetLossResponse_OnUpdate, Ped %s, Aborting TaskCombat - TLR_ExitTask.\n", 
				AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

			SetTaskCombatState(State_Finish);
			return FSM_Continue;
		}
		case CCombatData::TLR_SearchForTarget:
		{
			//Check if we should finish this task to search for the target.
			if(ShouldFinishTaskToSearchForTarget())
			{
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::PullTargetFromVehicle_OnUpdate, Ped %s, Aborting TaskCombat - TLR_SearchForTarget.\n", 
					AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK
				//Finish the task.
				SetTaskCombatState(State_Finish);
				return FSM_Continue;
			}
			else
			{
				SetTaskCombatState(State_Search);
				return FSM_Continue;
			}
		}
		case CCombatData::TLR_NeverLoseTarget:
			{
#if __BANK
				CAILogManager::GetLog().Log("CTaskCombat::DecideTargetLossResponse_OnUpdate: %s, TLR_NeverLoseTarget is set, but the target was lost\n", 
					AILogging::GetEntityDescription(pPed).c_str());
#endif // __BANK
				SetTaskCombatState(State_Search);
				return FSM_Continue;
			}
		default:
			taskAssertf(0, "Unhandled target loss response: %i", (s32)combatBehaviour.GetTargetLossResponse());
	}
	return FSM_Continue;
}

void CTaskCombat::DragInjuredToSafety_OnEnter()
{
	//Ensure the injured ped is valid.
	if(!m_pInjuredPedToHelp)
	{
		return;
	}
	
	//Ensure the dragging task is allowed.
	if(!AllowDraggingTask())
	{
		return;
	}
	
	//Start the drag task.
	CTaskDraggingToSafety* pTask = rage_new CTaskDraggingToSafety(m_pInjuredPedToHelp, m_pPrimaryTarget, CTaskDraggingToSafety::CF_Interrupt);
	SetNewTask(pTask);
	
	//Clear the injured ped.
	m_pInjuredPedToHelp = NULL;
}

CTask::FSM_Return CTaskCombat::DragInjuredToSafety_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
	}
	else
	{
		//Set the time that the last drag was active.
		ms_uTimeLastDragWasActive = fwTimer::GetTimeInMilliseconds();
	}
	
	return FSM_Continue;
}

void CTaskCombat::Mounted_OnEnter()
{
	SetNewTask(rage_new CTaskCombatMounted(m_pPrimaryTarget));
}

CTask::FSM_Return CTaskCombat::Mounted_OnUpdate()
{
	if(GetIsSubtaskFinished(CTaskTypes::TASK_COMBAT_MOUNTED))
	{
		// Find the new state from the table
		if(PickNewStateTransition(State_Start, true))
		{
			return FSM_Continue;
		}
		else
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}

	// Keep the target in the subtask up to date, in case it changed (is this possible?).
	CTask* pSubTask = GetSubTask();
	if(pSubTask && Verifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_COMBAT_MOUNTED, "Expected CTaskCombatMounted subtask, got %d", pSubTask->GetTaskType()))
	{
		CTaskCombatMounted* pTaskCombatMounted = static_cast<CTaskCombatMounted*>(pSubTask);
		pTaskCombatMounted->SetTarget(m_pPrimaryTarget);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCombat::MeleeToArmed_OnUpdate()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the weapon manager is valid.
	if(pPed->GetWeaponManager())
	{
		//Equip the best weapon.
		pPed->GetWeaponManager()->EquipBestWeapon();
	}
	
	//Reset the transition table.
	aiTaskStateTransitionTable* pTransitionSet = GetTransitionSet();
	if(GetTransitionTableData()->GetTransitionSet() != pTransitionSet)
	{
		GetTransitionTableData()->SetTransitionSet(pTransitionSet);
	}
	
	//Set the state.
	SetTaskCombatState(State_Start);
	
	return FSM_Continue;
}

void CTaskCombat::ReactToImminentExplosion_OnEnter()
{
	//Ensure the potential blast is valid.
	if(!m_PotentialBlast.IsValid())
	{
		return;
	}

	//Stop any ambient clips.
	StopClip(FAST_BLEND_OUT_DELTA);

	//Say the audio.
	if(!(CNetwork::IsGameInProgress() && GetPed()->GetIsInVehicle()))
	{
		if(fwRandom::GetRandomTrueFalse())
		{
			GetPed()->NewSay("EXPLOSION_IMMINENT");
		}
		else
		{
			GetPed()->NewSay("DUCK");
		}
	}

	//Get the explosion target.
	CAITarget explosionTarget;
	m_PotentialBlast.GetTarget(*GetPed(), explosionTarget);

	//Create the task.
	CTaskReactToImminentExplosion* pTask = rage_new CTaskReactToImminentExplosion(explosionTarget, m_PotentialBlast.m_fRadius, m_PotentialBlast.m_uTimeOfExplosion);

	//Set the aim target.
	CAITarget aimTarget;
	GetAimTarget(aimTarget);
	pTask->SetAimTarget(aimTarget);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::ReactToImminentExplosion_OnUpdate()
{
	//Check if the sub-task is invalid, or finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Pick a new state.
		PickNewStateTransition(State_Finish, true);
	}
	//Periodically check for state transitions.
	else if(PeriodicallyCheckForStateTransitions(ScaleReactionTime(GetPed(), AVERAGE_STATE_SEARCH_TIME)))
	{
		//Nothing to do, state has been changed.
	}
	
	return FSM_Continue;
}

void CTaskCombat::ReactToExplosion_OnEnter(CPed const* pPed)
{	
	if(pPed && !pPed->IsNetworkClone())
	{
		// Force finish the cover search timer, we want to hopefully come out of the reaction with a cover point ready
		m_coverSearchTimer.ForceFinish();
	}

	//Stop any ambient clips.
	StopClip(FAST_BLEND_OUT_DELTA);
	
	//Create the task.
	float fExplosionZ = GetPed()->GetTransform().GetPosition().GetZf();
	CTaskReactToExplosion* pTask = rage_new CTaskReactToExplosion(Vec3V(m_fExplosionX, m_fExplosionY, fExplosionZ), NULL, m_fExplosionRadius,
		CTaskReactToExplosion::CF_DisableThreatResponse);
	pTask->SetConfigFlagsForShellShocked(CTaskShellShocked::CF_Interrupt | CTaskShellShocked::CF_DisableMakeFatigued);

	if (GetPed()->IsNetworkClone())
	{
		pTask->SetRunningLocally(true);
	}

	//Set the aim target.
	CAITarget aimTarget;
	GetAimTarget(aimTarget);
	pTask->SetAimTarget(aimTarget);

	//Set the duration for shell-shocked.
	pTask->SetDurationOverrideForShellShocked(CTaskShellShocked::Duration_Short);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::ReactToExplosion_OnUpdate()
{
	//Wait for the task to finish.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we have a gun.
		if(GetPed()->GetWeaponManager() &&
			GetPed()->GetWeaponManager()->GetIsArmedGun())
		{
			//Check if we should blend into an aim.
			if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim))
			{
				SetTaskCombatState(State_Fire);
			}
			else
			{
				SetTaskCombatState(State_AimIntro);
			}
		}
		else
		{
			SetTaskCombatState(State_Start);
		}
	}
	
	return FSM_Continue;
}

void CTaskCombat::ReactToBuddyShot_OnEnter()
{
	if(m_pBuddyShot)
	{
		//Get the aim target.
		CAITarget aimTarget;
		GetAimTarget(aimTarget);

		//Create the task.
		taskAssert(CTaskReactToBuddyShot::IsValid(*GetPed(), CAITarget(m_pBuddyShot), aimTarget));
		CTask* pTask = rage_new CTaskReactToBuddyShot(CAITarget(m_pBuddyShot), aimTarget);

		//Start the task.
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskCombat::ReactToBuddyShot_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsSubtaskFinished(CTaskTypes::TASK_REACT_TO_BUDDY_SHOT))
	{
		//Pick a new state.
		PickNewStateTransition(State_Finish, true);
	}

	return FSM_Continue;
}

void CTaskCombat::ReactToBuddyShot_OnEnter_Clone()
{
	m_buddyShotReactSubTaskLaunched	= false;
}

CTask::FSM_Return CTaskCombat::ReactToBuddyShot_OnUpdate_Clone()
{
	CPed* ped = GetPed();

	if(!ped || !ped->IsNetworkClone() || !ped->GetWeaponManager())
	{
		return FSM_Continue;
	}

	if(!m_pBuddyShot)
	{
		return FSM_Continue;
	}

	fwMvClipSetId clipSet = CTaskReactToBuddyShot::ChooseClipSet(*ped);

	if(clipSet == CLIP_SET_ID_INVALID)
	{
		return FSM_Continue;
	}

	m_ClipSetRequestHelperForBuddyShot.RequestClipSet(clipSet);
	if(!m_ClipSetRequestHelperForBuddyShot.IsClipSetStreamedIn())
	{
		return FSM_Continue;	
	}

	CTask* task = ped->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_REACT_TO_BUDDY_SHOT);

	if((!task) && (!m_buddyShotReactSubTaskLaunched))
	{ 
		CAITarget aimTarget;
		GetAimTarget(aimTarget);

		CTask* pTask = rage_new CTaskReactToBuddyShot(CAITarget(m_pBuddyShot), aimTarget);
		SetNewTask(pTask);

		m_buddyShotReactSubTaskLaunched = true;
	}

	return FSM_Continue;
}

void CTaskCombat::ReactToBuddyShot_OnExit_Clone()
{
	m_ClipSetRequestHelperForBuddyShot.ReleaseClipSet();
	m_buddyShotReactSubTaskLaunched = false;
}

void CTaskCombat::GetOutOfWater_OnEnter()
{
	static const float fGetOutOfWaterSearchDist = 50.0f;
	CTask* pTask = rage_new CTaskGetOutOfWater(fGetOutOfWaterSearchDist, MOVEBLENDRATIO_RUN);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::GetOutOfWater_OnUpdate()
{
	//Wait for the task to finish.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Pick a new state.
		PickNewStateTransition(State_Finish, true);
	}

	return FSM_Continue;
}

void CTaskCombat::ArrestTarget_OnEnter()
{
	//Check if we should force arrest.
	bool bDontForceArrest = m_uRunningFlags.IsFlagSet(RF_DontForceArrest);
	bool bForceArrest = !bDontForceArrest;

	SetNewTask(rage_new CTaskArrestPed(m_pPrimaryTarget, bForceArrest, true));
}

CTask::FSM_Return CTaskCombat::ArrestTarget_OnUpdate()
{
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetTaskCombatState(State_Start);
	}

	return FSM_Continue;
}

void CTaskCombat::ArrestTarget_OnExit()
{
	//Clear the 'arrest' flag.
	m_uFlags.ClearFlag(ComF_ArrestTarget);

	//Clear the 'don't force arrest' flag.
	m_uRunningFlags.ClearFlag(RF_DontForceArrest);

	//Set the time an arrest was last attempted if this is a player
	if(m_pPrimaryTarget)
	{
		CPlayerInfo* pPlayerInfo = m_pPrimaryTarget->GetPlayerInfo();
		if(pPlayerInfo)
		{
			pPlayerInfo->SetLastTimeArrestAttempted();
		}
	}
}

void CTaskCombat::MoveToTacticalPoint_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT))
	{
		return;
	}

	//Create the task.
	CTaskMoveToTacticalPoint* pTask = rage_new CTaskMoveToTacticalPoint(CAITarget(m_pPrimaryTarget));

	//Check if we have a clear line of sight to our target, and only a few peds have clear line of sight.
	static dev_s32 s_iMinPedsToNotCareAboutLos = 4;
	if(HasClearLineOfSightToTarget() && (CountPedsWithClearLineOfSightToTarget() < s_iMinPedsToNotCareAboutLos))
	{
		//Disable points without a clear line of sight.
		pTask->GetConfigFlags().SetFlag(CTaskMoveToTacticalPoint::CF_DisablePointsWithoutClearLos);
	}

	//Set the sub-task to use during movement.
	CTask* pSubTaskToUseDuringMovement = rage_new CTaskCombatAdditionalTask(
		CTaskCombatAdditionalTask::RT_Default | CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked | CTaskCombatAdditionalTask::RT_AllowAimFireToggle |
		CTaskCombatAdditionalTask::RT_AllowAimingOrFiringIfLosBlocked | CTaskCombatAdditionalTask::RT_MakeDynamicAimFireDecisions,
		m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
	pTask->SetSubTaskToUseDuringMovement(pSubTaskToUseDuringMovement);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::MoveToTacticalPoint_OnUpdate()
{
	// Check for some misc state changes that aren't handled by the transition table
	if(CheckForPossibleStateChanges(false))
	{
		return FSM_Continue;
	}

	// Possibly play an ambient combat animation
	UpdateAmbientAnimPlayback(GetPed());

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Find the new state from the table
		if( PickNewStateTransition(State_Finish, true) )
		{
			return FSM_Continue;
		}
	}

	//Check if we can change state.
	CTaskMoveToTacticalPoint* pTask = static_cast<CTaskMoveToTacticalPoint *>(GetSubTask());
	bool bCanChangeState = (!pTask || !pTask->IsMovingToPoint() || IsPotentialStateChangeUrgent());

	// Periodically check for a new state
	if( bCanChangeState && PeriodicallyCheckForStateTransitions(ScaleReactionTime(GetPed(), AVERAGE_STATE_SEARCH_TIME)) )
	{
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskCombat::ThrowSmokeGrenade_OnEnter()
{
	CPed* pPed = GetPed();

	// Get the target's player info, which should exist if we are in this state!
	CPlayerInfo* pPlayerInfo = m_pPrimaryTarget.Get()->GetPlayerInfo();
	if( !pPlayerInfo )
	{
		return; // no more can be done, we'll exit this state immediately in the OnUpdate
	}
	
	// give the ped a smoke grenade to throw
	if( pPed->GetInventory() )
	{
		if( !pPed->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_SMOKEGRENADE, 1) )
		{
			return; // no more can be done, we'll exit this state immediately in the OnUpdate
		}
	}

	// Mark tracking variables
	ReportSmokeGrenadeThrown();

	// equip the smoke grenade
	Assert(pPed->GetWeaponManager());
	const s32 iVehicleIndex = -1; // not a vehicle weapon
	const bool bCreateWeaponWhenLoaded = false;
	pPed->GetWeaponManager()->EquipWeapon(WEAPONTYPE_SMOKEGRENADE, iVehicleIndex, bCreateWeaponWhenLoaded);
	
	// Setup the projectile throw subtask
	CWeaponTarget throwTarget(m_pPrimaryTarget);
	CTaskAimAndThrowProjectile* pThrowTask = rage_new CTaskAimAndThrowProjectile(throwTarget);
	SetNewTask(pThrowTask);
}

CTask::FSM_Return CTaskCombat::ThrowSmokeGrenade_OnUpdate()
{
	if( !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		// TODO: Is it important to RemoveWeaponAndAmmo after this?
		// Currently the ped is left with smoke grenades in their inventory with zero ammo
		// If we want to remove, we will have to use a flag to only remove if ped didn't have it prior to this state

		// switch back to primary weapon
		EquipWeaponForThreat();

		// go to fire state
		SetTaskCombatState(State_Fire);
	}

	return FSM_Continue;
}

void CTaskCombat::Victory_OnEnter()
{
	//Say some audio.
	GetPed()->NewSay("WON_DISPUTE");

	//Note that we were victorious over the target.
	GetPed()->GetPedIntelligence()->AchievedCombatVictoryOver(m_pPrimaryTarget);

	//Create the move task.
	CTask* pMoveTask = rage_new CTaskMoveFaceTarget(CAITarget(m_pPrimaryTarget));

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombat::Victory_OnUpdate()
{
	//Check if we have finished talking.
	if((GetTimeInState() > 1.0f) &&
		(!GetPed()->GetSpeechAudioEntity() || !GetPed()->GetSpeechAudioEntity()->IsAmbientSpeechPlaying()))
	{
		//Say some audio.
		GetPed()->NewSay("GENERIC_WHATEVER");

#if __BANK
		CAILogManager::GetLog().Log("CTaskCombat::Victory_OnUpdate, Ped %s, Aborting TaskCombat.\n", 
			AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

		SetTaskCombatState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskCombat::ReturnToInitialPosition_OnEnter()
{
	if(IsCloseAll(m_vInitialPosition, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)))
	{
		return;
	}

	CTaskMoveGoToPointStandStillAchieveHeading* pMoveTask = rage_new CTaskMoveGoToPointStandStillAchieveHeading(MOVEBLENDRATIO_RUN, VEC3V_TO_VECTOR3(m_vInitialPosition), m_fInitialHeading,
																		CTaskMoveGoToPointAndStandStill::ms_fTargetRadius, CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance,
																		CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac, CTaskMoveAchieveHeading::ms_fHeadingTolerance, -LARGE_FLOAT);

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask, rage_new CTaskWeaponBlocked(CWeaponController::WCT_Aim)));
}

CTask::FSM_Return CTaskCombat::ReturnToInitialPosition_OnUpdate()
{
	// If for some reason our task ended then it failed to be created
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetTaskCombatState(State_Start);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();
	CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
	if(pPedTargeting && pPedTargeting->GetLosStatus(m_pPrimaryTarget, false) == Los_clear)
	{
		SetTaskCombatState(State_Start);
		return FSM_Continue;
	}

	// Once we get there we should switch to the gun task and force blocked
	CTaskComplexControlMovement* pControlMovementSubtask = static_cast<CTaskComplexControlMovement*>(FindSubTaskOfType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
	if( pControlMovementSubtask && pControlMovementSubtask->GetMainSubTaskType() != CTaskTypes::TASK_GUN &&
		pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_ACHIEVE_HEADING) &&
		CTaskMoveAchieveHeading::IsHeadingAchieved(pPed, m_fInitialHeading, CTaskMoveAchieveHeading::ms_fHeadingTolerance) )
	{
		Vec3V vOffset(-rage::Sinf(m_fInitialHeading) * 10.0f, rage::Cosf(m_fInitialHeading) * 10.0f, 0.0f);
		Vector3 vTargetPosition = VEC3V_TO_VECTOR3(m_vInitialPosition + vOffset);

		CTaskGun* pGunTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(vTargetPosition));
		pGunTask->GetGunFlags().SetFlag(GF_ForceBlockedClips);

		pControlMovementSubtask->SetNewMainSubtask(pGunTask);
	}

	return FSM_Continue;
}

bool CTaskCombat::CheckForMovingWithinDefensiveArea(bool bSetState) 
{
	// already moving into defensive area
	if( GetState() == State_MoveWithinDefensiveArea )
	{
		// no state change needed
		return false;
	}

	// script marked this ped for charging beyond defensive area
	if( GetState() == State_ChargeTarget && GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PermitChargeBeyondDefensiveArea) )
	{
		// no state change needed
		return false;
	}

	// Check for active defensive area and ped being outside of it
	if (GetPed()->GetPedIntelligence()->GetDefensiveArea()->IsActive() && !m_uFlags.IsFlagSet(ComF_IsInDefensiveArea) && CanMoveWithinDefensiveArea())
	{
		// Don't let us interrupt the cover intro task
		if( FindSubTaskOfType(CTaskTypes::TASK_ENTER_COVER) )
		{
			return false;
		}

		if(bSetState)
		{
			SetTaskCombatState(State_MoveWithinDefensiveArea);
		}

		return true;
	}
	return false;
}

bool CTaskCombat::CheckForTargetChangedThisFrame(bool bRestartStateOnTargetChange)
{
	if(m_uFlags.IsFlagSet(ComF_TargetChangedThisFrame))
	{
		m_uFlags.ClearFlag(ComF_TargetChangedThisFrame);

		CTaskCover* pCoverTask = static_cast<CTaskCover *>(FindSubTaskOfType(CTaskTypes::TASK_COVER));
		if(!pCoverTask ||
			(pCoverTask->GetState() <= CTaskCover::State_MoveToSearchLocation && !DoesCoverPointProvideCover(GetPed()->GetCoverPoint())))
		{
			if(bRestartStateOnTargetChange)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
			else
			{
				SetTaskCombatState(State_Start);
			}

			return true;
		}
	}

	return false;
}

bool CTaskCombat::CheckForStopMovingToCover()
{
	CPed* pPed = GetPed();
	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		return false;
	}

	if(pPed->PopTypeIsRandom() && pPed->GetCoverPoint()) 
	{
		// We have to be moving to a cover point
		CTaskCover* pCoverTask = static_cast<CTaskCover*>(FindSubTaskOfType(CTaskTypes::TASK_COVER));
		if(!pCoverTask || pCoverTask->GetState() != CTaskCover::State_MoveToCover || pCoverTask->GetTimeInState() < ms_Tunables.m_MinMovingToCoverTimeToStop)
		{
			return false;
		}

		// Must be firing/aiming
		CTaskCombatAdditionalTask* pAdditionalCombatTask = static_cast<CTaskCombatAdditionalTask *>(pCoverTask->FindSubTaskOfType(CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK));
		if(!pAdditionalCombatTask || pAdditionalCombatTask->GetState() != CTaskCombatAdditionalTask::State_AimingOrFiring)
		{
			return false;
		}

		// We must have LOS to our target
		CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
		CPedTargetting* pPedTargeting = pPedIntelligence->GetTargetting(false);
		if(!pPedTargeting || pPedTargeting->GetLosStatus(m_pPrimaryTarget, false) != Los_clear)
		{
			return false;
		}

		// If we have a defensive area we really don't want to stop outside of it
		CDefensiveArea* pDefensiveArea = pPedIntelligence->GetDefensiveArea();
		if(pDefensiveArea && pDefensiveArea->IsActive())
		{
			if(!m_uFlags.IsFlagSet(ComF_IsInDefensiveArea))
			{
				return false;
			}
		}

		// I we are set to advance and are outside the attack window, we probably don't want to stop yet
		if(CTaskMoveWithinAttackWindow::IsOutsideAttackWindow(pPed, m_pPrimaryTarget, false))
		{
			return false;
		}

		// Lastly, let's make sure that the cover point is far enough away from us
		Vector3 vCoverPosition;
		if(pPed->GetCoverPoint()->GetCoverPointPosition(vCoverPosition))
		{
			ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vCoverPosition));
			ScalarV scMinDistSq = ScalarVFromF32(ms_Tunables.m_MinDistanceToCoverToStop);
			scMinDistSq = scMinDistSq * scMinDistSq;
			if(IsGreaterThanAll(scDistSq, scMinDistSq))
			{
				// Store out current cover point into our desired, hopefully we can return to it
				pPed->SetDesiredCoverPoint(pPed->GetCoverPoint());
				pPed->ReleaseCoverPoint();

				m_uFlags.SetFlag(ComF_MoveToCoverStopped);
				SetTaskCombatState(State_Fire);
				return true;
			}
		}
	}

	return false;
}

bool CTaskCombat::CheckForForceFireState()
{
	if(!ms_Tunables.m_EnableForcedFireForTargetProximity)
	{
		return false;
	}

	if(m_fTimeUntilForcedFireAllowed > 0.0f)
	{
		return false;
	}

	if(GetTimeInState() < ms_Tunables.m_MinTimeInStateForForcedFire)
	{
		return false;
	}

	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	ScalarV scMaxDistSq = LoadScalar32IntoScalarV(square(ms_Tunables.m_MaxForceFiringDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	ScalarV scMinDistSq = LoadScalar32IntoScalarV(square(ms_Tunables.m_MinForceFiringDistance));
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	m_uFlags.SetFlag(ComF_TemporarilyForceFiringState);
	SetTaskCombatState(State_Fire);

	return true;
}

bool CTaskCombat::CheckForChargeTarget(bool bChangeState)
{
	// Check if feature is disabled
	if( !ms_Tunables.m_ChargeTuning.m_bChargeTargetEnabled || ms_Tunables.m_ChargeTuning.m_uMaxNumActiveChargers <= 0 )
	{
		return false;
	}

	// If we are already charging target
	if( GetState() == State_ChargeTarget )
	{
		// no more work to be done here
		return false;
	}

	// If we should charge the target
	const CPed* pPed = GetPed();
	if( ShouldChargeTarget(pPed, m_pPrimaryTarget) )
	{
		if( bChangeState )
		{
			// Go to charge target state
			SetTaskCombatState(State_ChargeTarget);
		}

		// report state change
		return true;
	}

	PF_START(WantsToChargeTarget);
	bool bWantsToChargeTarget = WantsToChargeTarget(pPed, m_pPrimaryTarget);
	PF_STOP(WantsToChargeTarget);

	// If we would like to charge the target
	if( bWantsToChargeTarget )
	{
		// tell the target's player info
		CPlayerInfo* pPlayerInfo = m_pPrimaryTarget->GetPlayerInfo();
		if( AssertVerify(pPlayerInfo) ) // this is checked for as part of WantsToChargeTarget
		{
			// this will enter us into consideration to charge,
			// and if we win we will return true for ShouldChargeTarget shortly.
			pPlayerInfo->RegisterCandidateCharger(pPed);
			// NOTE: It is alright to keep registering while we want to charge, as the selection
			// process will only be open for a short time before moving on to the even shorter acceptance period.
			// Once the acceptance period ends, either by short timeout or winner charging, we will
			// no longer want to charge target due to min time or other checks. (dmorales)
		}
	}

	// by default no state change
	return false;
}

bool CTaskCombat::CheckForStopChargingTarget()
{
	// If we are charging target
	if( AssertVerify(GetState() == State_ChargeTarget) )
	{
		if( m_pPrimaryTarget && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CHARGE )
		{
			// Get a handle to the charge subtask
			const CTaskCharge* pChargeSubTask = static_cast<const CTaskCharge*>(GetSubTask());

			// Check if target is using cover
			bool bTargetUsingCover = m_pPrimaryTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER)	|| m_pPrimaryTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOTION_IN_COVER);
			
			// Use tunable distance according to target being in or out of cover
			float targetMovementDist = ms_Tunables.m_ChargeTuning.m_fCancelTargetOutOfCoverMovedDist;
			if( bTargetUsingCover )
			{
				targetMovementDist = ms_Tunables.m_ChargeTuning.m_fCancelTargetInCoverMovedDist;
			}

			// Check if target has moved too far away from where they were when the charging started
			const Vec3V currentTargetPos = m_pPrimaryTarget->GetTransform().GetPosition();
			const Vec3V initialTargetPos = pChargeSubTask->GetTargetInitialPosition();
			const ScalarV distTargetMovedSq = DistSquared(currentTargetPos, initialTargetPos);
			const ScalarV distTargetMovedSq_MAX = ScalarVFromF32(rage::square(targetMovementDist));
			if( IsGreaterThanAll(distTargetMovedSq, distTargetMovedSq_MAX) )
			{
				// go back to firing state
				SetTaskCombatState(State_Fire);

				// report state changed
				return true;
			}

			// If the target is out of cover
			// Check if we have gotten close enough to the target to force firing
			const CPed* pPed = GetPed();
			if( pPed )
			{
				float fStopProximity = 2.0f; // @TODO: charge tunable
				if( !bTargetUsingCover )
				{
					fStopProximity = ms_Tunables.m_MaxForceFiringDistance;
				}
				const Vec3V myPos = pPed->GetTransform().GetPosition();
				const ScalarV distToTargetSq = DistSquared(currentTargetPos, myPos);
				const ScalarV distToTargetSq_MIN = ScalarVFromF32(rage::square(fStopProximity));
				if( IsLessThanAll(distToTargetSq, distToTargetSq_MIN) )
				{
					// Check that we have clear line of sight to primary target
					const bool bWakeIfInactive = false;
					CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(bWakeIfInactive);
					if( pPedTargetting )
					{
						CSingleTargetInfo* pTargetInfo = pPedTargetting->FindTarget(m_pPrimaryTarget);
						if(pTargetInfo && pTargetInfo->GetLosStatus() == Los_clear)
						{
							// force firing for a time
							m_uFlags.SetFlag(ComF_TemporarilyForceFiringState);

							// go to the firing state
							SetTaskCombatState(State_Fire);

							// report state changed
							return true;
						}
					}
				}
			}
		}
	}

	// no state change
	return false;
}

bool CTaskCombat::WantsToChargeTarget(const CPed* pPed, const CPed* pTargetPed)
{
	// If no target
	if( !pTargetPed )
	{
		return false;
	}

	// Ped must be able to charge
	if( !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanCharge) )
	{
		return false;
	}

	// Ped shouldn't charge if using wounded movement
	if( pPed->HasHurtStarted() )
	{
		return false;
	}

	// Ped must not be in a vehicle, or mounted, or swimming
	if( pPed->GetVehiclePedInside() || pPed->GetMyMount() || pPed->GetIsSwimming() )
	{
		// cannot charge
		return false;
	}

	// Ped must know where the target is to charge them
	if( m_uFlags.IsFlagSet(ComF_TargetLost) )
	{
		return false;
	}

	// If not enough time has passed since this ped started the combat task
	const float fTimeRunningSeconds = GetTimeRunning();
	if( fTimeRunningSeconds < ms_Tunables.m_ChargeTuning.m_fMinTimeInCombatSeconds )
	{
		return false;
	}

	// If not enough time has passed since this check was last performed
	const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();
	if( m_uTimeOfLastChargeTargetCheck > 0 && currentTimeMS < m_uTimeOfLastChargeTargetCheck + ms_Tunables.m_ChargeTuning.m_uCheckForChargeTargetPeriodMS )
	{
		return false;
	}
	else // proceeding with remaining checks
	{
		m_uTimeOfLastChargeTargetCheck = currentTimeMS;
	}

	// If not enough time has passed since this specific Ped last charged
	if( m_uTimeWeLastCharged > 0 && currentTimeMS < m_uTimeWeLastCharged + ms_Tunables.m_ChargeTuning.m_uMinTimeForSamePedToChargeAgainMS )
	{
		return false;
	}

	// Target must be player controlled
	if( !pTargetPed->IsControlledByLocalOrNetworkPlayer() )
	{
		return false;
	}

	// Target must have player info
	CPlayerInfo* pPlayerInfo = pTargetPed->GetPlayerInfo();
	if( !pPlayerInfo )
	{
		return false;
	}

	// If the time since the last charger at this target is too recent
	if( pPlayerInfo->HaveNumEnemiesChargedWithinQueryPeriod(ms_Tunables.m_ChargeTuning.m_uMinTimeBetweenChargesAtSameTargetMS, 1) )
	{
		return false;
	}

	// If the number of recent chargers limit has been reached for this target
	if( pPlayerInfo->HaveNumEnemiesChargedWithinQueryPeriod(ms_Tunables.m_ChargeTuning.m_uConsiderRecentChargeAsActiveTimeoutMS, ms_Tunables.m_ChargeTuning.m_uMaxNumActiveChargers) )
	{
		return false;
	}

	// Ped must not charge if the target is outside their defensive area
	if( pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() )
	{
		if( !pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition())) )
		{
			return false;
		}
	}

	// Compute the distance to the target squared
	const ScalarV distToTargetSquared = DistSquared(pPed->GetTransform().GetPosition(), pTargetPed->GetTransform().GetPosition());

	// If we are too close to the target to start charging
	const ScalarV distToTargetSquaredMIN = ScalarVFromF32( rage::square(ms_Tunables.m_ChargeTuning.m_fMinDistanceToTarget) );
	if( !IsGreaterThanAll(distToTargetSquared, distToTargetSquaredMIN) )
	{
		return false;
	}

	// If we are too far from the target to start charging
	const ScalarV distToTargetSquaredMAX = ScalarVFromF32( rage::square(ms_Tunables.m_ChargeTuning.m_fMaxDistanceToTarget) );
	if( !IsLessThanAll(distToTargetSquared, distToTargetSquaredMAX) )
	{
		return false;
	}

	// Compute a required time the target has to have been hiding in cover based on distance:
	const float fOuterSeconds = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatTriggerChargeTime_Far);
	const float fInnerSeconds = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatTriggerChargeTime_Near);
	const float fOuterDistance = ms_Tunables.m_ChargeTuning.m_fDistToHidingTarget_Outer;
	const float fInnerDistance = ms_Tunables.m_ChargeTuning.m_fDistToHidingTarget_Inner;
	// Initialize hiding time for outer distance
	float fMinHidingTimeSeconds = fOuterSeconds;
	// Check if target is inside outer range
	const ScalarV outerDistToTargetSquared = ScalarVFromF32(rage::square(fOuterDistance));
	if( IsLessThanAll(distToTargetSquared,outerDistToTargetSquared) )
	{
		// Check if target is inside inner range
		const ScalarV innerDistToTargetSquared = ScalarVFromF32(rage::square(fInnerDistance));
		if( IsLessThanAll(distToTargetSquared, innerDistToTargetSquared) )
		{
			fMinHidingTimeSeconds = fInnerSeconds;
		}
		else // otherwise scale the distance within ranges
		{
			const float fDistanceToTarget = Sqrt(distToTargetSquared).Getf();
			fMinHidingTimeSeconds = fInnerSeconds + ((fOuterSeconds - fInnerSeconds)/(fOuterDistance - fInnerDistance)) * (fDistanceToTarget - fInnerDistance);
		}
	}
	// Target must have been hiding in cover long enough
	if( pPlayerInfo->GetTimeElapsedHidingInCoverSeconds() < fMinHidingTimeSeconds )
	{
		return false;
	}

	// Get the list of the peds near this ped
	// NOTE: This list is sorted by distance already, so the first enemy is closest
	bool bCheckedClosestEnemy = false;
	CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		CPed* pNearbyPed = static_cast<CPed*>(pEnt);
		// If ped is not self and not target
		if( pNearbyPed && pNearbyPed != pPed && pNearbyPed != pTargetPed )
		{
			// Check if an enemy living ped
			if( !pNearbyPed->GetIsDeadOrDying() && pPed->GetPedIntelligence()->IsThreatenedBy(*pNearbyPed) )
			{
				// If we haven't already checked the closest enemy
				if( !bCheckedClosestEnemy )
				{
					// do this once
					bCheckedClosestEnemy = true;

					// Compute the distance squared to the closest enemy
					const ScalarV distSqToClosestNonTargetEnemy = DistSquared(pPed->GetTransform().GetPosition(), pNearbyPed->GetTransform().GetPosition());

					// If the closest enemy ally is too close, do not charge
					const ScalarV distSqToClosestNonTargetEnemyMIN = ScalarVFromF32( rage::square(ms_Tunables.m_ChargeTuning.m_fMinDistToNonTargetEnemy) );
					if( !IsGreaterThanAll(distSqToClosestNonTargetEnemy, distSqToClosestNonTargetEnemyMIN) )
					{
						return false;
					}
				}

				// Compute the distance squared from this enemy to the target
				const ScalarV distSqFromEnemyToTarget = DistSquared(pNearbyPed->GetTransform().GetPosition(), pTargetPed->GetTransform().GetPosition());

				// If the target is too close to another enemy, do not charge
				const ScalarV distSqFromEnemyToTargetMIN = ScalarVFromF32( rage::square(ms_Tunables.m_ChargeTuning.m_fMinDistBetweenTargetAndOtherEnemies) );
				if( !IsGreaterThanAll(distSqFromEnemyToTarget, distSqFromEnemyToTargetMIN) )
				{
					return false;
				}
			}
		}
	}

	// all checks passed, we want to charge
	return true;
}

bool CTaskCombat::ShouldChargeTarget( const CPed* pPed, const CPed* pTarget )
{
	// If no target
	if( !pTarget )
	{
		return false;
	}

	// If scripts have marked us for charging
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ShouldChargeNow ) )
	{
		// Check if we just switched targets, need to let that settle first
		if( !m_uFlags.IsFlagSet(ComF_TargetChangedThisFrame) )
		{
			// we should charge target
			return true;
		}
	}

	// Get the target's player info, and check if we are the best charger candidate ped
	CPlayerInfo* pPlayerInfo = pTarget->GetPlayerInfo();
	if( pPlayerInfo && pPlayerInfo->IsBestChargerCandidate(pPed) )
	{
		// we should charge target
		return true;
	}

	// by default do not charge target
	return false;
}

///////////////////////////////////////////////////////////////

bool CTaskCombat::CheckForThrowSmokeGrenade(bool bChangeState)
{
	// Check if feature is disabled
	if( !ms_Tunables.m_ThrowSmokeGrenadeTuning.m_bThrowSmokeGrenadeEnabled || ms_Tunables.m_ThrowSmokeGrenadeTuning.m_uMaxNumActiveThrowers <= 0 )
	{
		return false;
	}

	// Check if the player is on a mission
	if( CTheScripts::GetPlayerIsOnAMission() )
	{
		return false;
	}

	// If we should throw smoke grenade
	const CPed* pPed = GetPed();
	if( ShouldThrowSmokeGrenade(pPed, m_pPrimaryTarget) )
	{
		if( bChangeState )
		{
			// Go to charge target state
			SetTaskCombatState(State_ThrowSmokeGrenade);
		}

		// report state change
		return true;
	}

	PF_START(WantsToThrowSmokeGrenade);
	bool bWantsToThrowSmokeGrenade = WantsToThrowSmokeGrenade(pPed, m_pPrimaryTarget);
	PF_STOP(WantsToThrowSmokeGrenade);

	// If we would like to throw smoke at the target
	if( bWantsToThrowSmokeGrenade )
	{
		// tell the target's player info
		CPlayerInfo* pPlayerInfo = m_pPrimaryTarget->GetPlayerInfo();
		if( AssertVerify(pPlayerInfo) ) // this is checked for as part of WantsToThrowSmokeGrenade
		{
			// this will enter us into consideration to throw,
			// and if we win we will return true for ShouldThrowSmokeGrenade shortly.
			pPlayerInfo->RegisterCandidateSmokeThrower(pPed);
			// NOTE: It is alright to keep registering while we want to throw, as the selection
			// process will only be open for a short time before moving on to the even shorter acceptance period.
			// Once the acceptance period ends, either by short timeout or winner throwing, we will
			// no longer want to throw at target due to min time or other checks. (dmorales)
		}
	}

	// by default no state change
	return false;
}

bool CTaskCombat::WantsToThrowSmokeGrenade(const CPed* pPed, const CPed* pTargetPed)
{
	// If no target
	if( !pTargetPed )
	{
		return false;
	}

	// If in a state that shouldn't be interrupted for smoke throw
	int iCurrentState = GetState();
	if( iCurrentState == State_ChargeTarget )
	{
		return false;
	}

	// Ped must be able to throw smoke grenades
	if( !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanThrowSmokeGrenade) )
	{
		return false;
	}

	// Ped can't be in hurt mode
	if( pPed->HasHurtStarted() )
	{
		return false;
	}

	// Ped must not be in a vehicle, or mounted, or swimming
	if( pPed->GetVehiclePedInside() || pPed->GetMyMount() || pPed->GetIsSwimming() )
	{
		return false;
	}

	// Ped must know where the target is
	if( m_uFlags.IsFlagSet(ComF_TargetLost) )
	{
		return false;
	}

	// If not enough time has passed since this check was last performed
	const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();
	if( m_uTimeOfLastSmokeThrowCheck > 0 && currentTimeMS < m_uTimeOfLastSmokeThrowCheck + ms_Tunables.m_ThrowSmokeGrenadeTuning.m_uCheckForSmokeThrowPeriodMS )
	{
		return false;
	}
	else // proceeding with remaining checks
	{
		m_uTimeOfLastSmokeThrowCheck = currentTimeMS;
	}

	// If not enough time has passed since this specific Ped last threw smoke
	if( m_uTimeWeLastThrewSmokeGrenade > 0 && currentTimeMS < m_uTimeWeLastThrewSmokeGrenade + ms_Tunables.m_ThrowSmokeGrenadeTuning.m_uMinTimeForSamePedToThrowAgainMS )
	{
		// don't throw
		return false;
	}

	// Target must be player controlled
	if( !pTargetPed->IsControlledByLocalOrNetworkPlayer() )
	{
		return false;
	}

	// Target must have player info
	CPlayerInfo* pPlayerInfo = pTargetPed->GetPlayerInfo();
	if( !pPlayerInfo )
	{
		return false;
	}

	// If the time since the last throw at this target is too recent
	if( pPlayerInfo->HaveNumEnemiesThrownSmokeWithinQueryPeriod(ms_Tunables.m_ThrowSmokeGrenadeTuning.m_uMinTimeBetweenThrowsAtSameTargetMS, 1) )
	{
		return false;
	}

	// If the number of recent throws limit has been reached for this target
	if( pPlayerInfo->HaveNumEnemiesThrownSmokeWithinQueryPeriod(ms_Tunables.m_ThrowSmokeGrenadeTuning.m_uConsiderRecentThrowAsActiveTimeoutMS, ms_Tunables.m_ThrowSmokeGrenadeTuning.m_uMaxNumActiveThrowers) )
	{
		return false;
	}

	// Target must have been loitering long enough
	if( pPlayerInfo->GetTimeElapsedLoiteringInCombatSeconds() < ms_Tunables.m_ThrowSmokeGrenadeTuning.m_fMinLoiteringTimeSeconds )
	{
		return false;
	}

	// Must have clear line of sight to target
	const bool bWakeIfInactive = false;
	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(bWakeIfInactive);
	if( pPedTargetting )
	{
		CSingleTargetInfo* pTargetInfo = pPedTargetting->FindTarget(pTargetPed);
		if(pTargetInfo && pTargetInfo->GetLosStatus() != Los_clear)
		{
			return false;
		}
	}

	// Compute the distance to the target squared
	const ScalarV distToTargetSquared = DistSquared(pPed->GetTransform().GetPosition(), pTargetPed->GetTransform().GetPosition());

	// If we are too close to the target
	const ScalarV distToTargetSquaredMIN = ScalarVFromF32( rage::square(ms_Tunables.m_ThrowSmokeGrenadeTuning.m_fMinDistanceToTarget) );
	if( !IsGreaterThanAll(distToTargetSquared, distToTargetSquaredMIN) )
	{
		return false;
	}

	// If we are too far from the target
	const ScalarV distToTargetSquaredMAX = ScalarVFromF32( rage::square(ms_Tunables.m_ThrowSmokeGrenadeTuning.m_fMaxDistanceToTarget) );
	if( !IsLessThanAll(distToTargetSquared, distToTargetSquaredMAX) )
	{
		return false;
	}

	// If we are not in cover
	if( iCurrentState != State_InCover )
	{
		// Check that the target ped is in front of this ped
		ScalarV dotPedToTarget = Dot( pPed->GetTransform().GetForward(), (pTargetPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition()) );
		if( IsLessThanAll(dotPedToTarget, LoadScalar32IntoScalarV(ms_Tunables.m_ThrowSmokeGrenadeTuning.m_fDotMinThrowerToTarget)) )
		{
			return false;
		}
	}

	// all checks passed, we want to throw a smoke grenade
	return true;
}

bool CTaskCombat::ShouldThrowSmokeGrenade(const CPed* pPed, const CPed* pTargetPed)
{
	// If no target
	if( !pTargetPed )
	{
		return false;
	}

	// If scripts have marked us for throwing smoke grenade manually
	if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ShouldThrowSmokeNow ) )
	{
		// we should throw
		return true;
	}

	// Get the target's player info, and check if we are the best thrower candidate ped
	CPlayerInfo* pPlayerInfo = pTargetPed->GetPlayerInfo();
	if( pPlayerInfo && pPlayerInfo->IsBestSmokeThrowerCandidate(pPed) )
	{
		// we should throw
		return true;
	}

	// by default do not throw
	return false;
}

const CWeaponInfo* CTaskCombat::HelperGetEquippedWeaponInfo() const
{
	const CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon)
	{
		return pWeapon->GetWeaponInfo();
	}
	else
	{
		const CWeaponInfo* pEquippedWeaponInfo = GetPed()->GetWeaponManager()->GetEquippedWeaponInfo();
		if(pEquippedWeaponInfo && pEquippedWeaponInfo->GetIsMelee())
		{
			return pEquippedWeaponInfo;
		}
	}
	
	return NULL;
}

bool CTaskCombat::HelperCanPerformMeleeAttack() const
{
	const CPed* pPed = GetPed();
	taskAssert(pPed);

	// cannot melee attack if inside a vehicle
	if( pPed->GetVehiclePedInside() )
	{
		return false;
	}

	// cannot melee attack if using wounded movement
	if( pPed->HasHurtStarted() )
	{
		return false;
	}

	// cannot melee attack if target is null, or dead, or dead by melee
	if( !m_pPrimaryTarget || m_pPrimaryTarget->IsDead() || m_pPrimaryTarget->IsDeadByMelee() )
	{
		return false;
	}

	if(!pPed->GetWeaponManager())
	{
		return false;
	}

	// If ped has a valid weapon manager
	const CWeaponInfo* pWeaponInfo = HelperGetEquippedWeaponInfo();
	if( !pWeaponInfo )
	{
		return false;
	}

	if( pWeaponInfo->GetIsMelee() )
	{
		// ped has a melee weapon, can melee
		return true;
	}

	// Not a fan of hard coding this but the fire extinguisher doesn't have any melee attacks and we can't do the expensive action checks here
	if( pWeaponInfo->GetHash() == WEAPONTYPE_FIREEXTINGUISHER )
	{
		return false;
	}

	// target is too far away to strike with non-melee weapon
	ScalarV scDistToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	ScalarV scMaxNonMeleeWeaponDistSq = ScalarVFromF32( ARMED_MELEE_ATTACK_DISTANCE_SQ );
	if( IsGreaterThanAll( scDistToTargetSq, scMaxNonMeleeWeaponDistSq) )
	{
		return false;
	}

	// We only care about this when the ped is stationary
	if( !ms_Tunables.m_AllowMovingArmedMeleeAttack )
	{		
		float fTargetMoveBlendRatioSq = pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2();
		if( fTargetMoveBlendRatioSq > rage::square( MBR_WALK_BOUNDARY ) )
		{
			return false;
		}
	}

	// Do not allow a melee attack unless we have waited X amount of time
	if( m_uTimeOfLastMeleeAttack == 0 || ( ( fwTimer::GetTimeInMilliseconds() - m_uTimeOfLastMeleeAttack ) > ms_Tunables.m_TimeBetweenArmedMeleeAttemptsInMs ) )
	{
		return true;
	}

	// cannot melee attack by default
	return false;
}

bool CTaskCombat::CheckForPossibleStateChanges(bool bResetStateOnTargetChange)
{
	//Ensure we can change state.
	if(ShouldNeverChangeStateUnderAnyCircumstances())
	{
		return false;
	}

	// If we are being melee homed by the player we need to stop so the player can hit us
	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsBeingMeleeHomedByPlayer))
	{
		SetTaskCombatState(State_Fire);
		return true;
	}

	// If the target changed we should go back to the start state
	if(CheckForTargetChangedThisFrame(bResetStateOnTargetChange))
	{
		return true;
	}

	if(CheckForTargetLossResponse())
	{
		return true;
	}
	
	if(CheckForForceFireState())
	{
		return true;
	}

	if(CheckForMovingWithinDefensiveArea(true))
	{
		return true;
	}

	if(CheckForStopMovingToCover())
	{
		return true;
	}

	if(CheckForChargeTarget())
	{
		return true;
	}

	if(CheckForThrowSmokeGrenade())
	{
		return true;
	}

	return false;
}

s32 CTaskCombat::GetTargetsEntryPoint(CVehicle& vehicle) const
{
	aiAssertf(m_pPrimaryTarget->GetVehiclePedInside() == &vehicle, "Expected ped to be inside vehicle %s", vehicle.GetModelName());

	s32 iPedsSeatIndex = vehicle.GetSeatManager()->GetPedsSeatIndex(m_pPrimaryTarget);
	if (taskVerifyf(iPedsSeatIndex >=0, "Ped seat index %d wasn't valid for vehicle %s", iPedsSeatIndex, vehicle.GetModelName()))
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);

		return CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(GetPed(), &vehicle, SR_Specific, iPedsSeatIndex, false, vehicleFlags);
	}

	return -1;
}

void CTaskCombat::ResetCoverSearchTimer()
{
	m_coverSearchTimer.Reset(ms_Tunables.m_fTimeBetweenCoverSearchesMin, ms_Tunables.m_fTimeBetweenCoverSearchesMax);
}

void CTaskCombat::ResetRetreatTimer()
{
	m_retreatTimer.Reset(ms_Tunables.m_fRetreatTime);
}

void CTaskCombat::ResetLostTargetTimer()
{
	m_lostTargetTimer.Reset(ms_Tunables.m_fLostTargetTime);
}

void CTaskCombat::ResetGoToAreaTimer()
{
	m_goToAreaTimer.Reset(ms_Tunables.m_fGoToDefAreaTimeOut);
}

void CTaskCombat::ResetJackingTimer()
{
	m_jackingTimer.Reset(ms_Tunables.m_fTimeBetweenJackingAttempts);
}

void CTaskCombat::ResetMoveToCoverTimer()
{
	m_moveToCoverTimer.Reset(ms_Tunables.m_fMinAttemptMoveToCoverDelay, ms_Tunables.m_fMaxAttemptMoveToCoverDelay);
}

bool CTaskCombat::IsPlayingAimPoseTransition() const
{
	//Ensure we are playing an aim pose transition.
	if(!m_uFlags.IsFlagSet(ComF_IsPlayingAimPoseTransition))
	{
		return false;
	}
	
	return true;
}

bool CTaskCombat::JustStartedPlayingAimPoseTransition() const
{
	//Ensure we were not playing an aim pose transition last frame.
	if(m_uFlags.IsFlagSet(ComF_WasPlayingAimPoseTransitionLastFrame))
	{
		return false;
	}
	
	//Ensure we are playing an aim pose transition this frame.
	if(!IsPlayingAimPoseTransition())
	{
		return false;
	}
	
	return true;
}

void CTaskCombat::ReportSmokeGrenadeThrown()
{
	// Get the target's player info
	CPlayerInfo* pPlayerInfo = m_pPrimaryTarget.Get()->GetPlayerInfo();
	if( pPlayerInfo )
	{
		// Report this throw is starting
		pPlayerInfo->ReportEnemyStartedSmokeThrow();
	}

	// Mark the time
	m_uTimeWeLastThrewSmokeGrenade = fwTimer::GetTimeInMilliseconds();

	// If scripts requested this throw, clear the flag now
	CPed* pPed = GetPed();
	if( pPed && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_ShouldThrowSmokeNow ) )
	{
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_ShouldThrowSmokeNow, false);
	}
}

bool CTaskCombat::IsValidForMotionTask( CTaskMotionBase& task ) const
{
	bool isValid;

	if (task.IsOnFoot() || task.IsInVehicle())
		isValid = true;
	else
		isValid = false;

	return isValid;
}

void CTaskCombat::UpdateClass()
{
	ms_bGestureProbedThisFrame = false;
	ms_moveToCoverTimerGlobal.Tick();
	ms_allowFrustrationTimerGlobal.Tick();
	ms_fTimeUntilNextGestureGlobal -= fwTimer::GetTimeStep();
#if __BANK
	ms_EnemyAccuracyLog.Update();
#endif // __BANK

	//Update the combat directors.
	PF_START(UpdateCombatDirectors);
	CCombatDirector::UpdateCombatDirectors();
	PF_STOP(UpdateCombatDirectors);
	
	//Update the vehicle chase directors.
	PF_START(UpdateVehicleChaseDirectors);
	CVehicleChaseDirector::UpdateVehicleChaseDirectors();
	PF_STOP(UpdateVehicleChaseDirectors);

	//Update the boat chase directors.
	CBoatChaseDirector::UpdateAll();

	//Update the tactical analysis.
	PF_START(CTacticalAnalysis_Update);
	CTacticalAnalysis::Update();
	PF_STOP(CTacticalAnalysis_Update);
}

bool CTaskCombat::UpdatePrimaryTarget( CPed* pPed )
{
	// Cache the ped intelligence
	CPedIntelligence* pPedIntell = pPed->GetPedIntelligence();

	// Make sure the targeting system is activated.
	CPedTargetting* pPedTargetting = pPedIntell->GetTargetting( true );
	Assert(pPedTargetting);

	// Make sure the targeting system is activated, scan for a new target
	m_uFlags.ClearFlag(ComF_TargetChangedThisFrame);

	if(!m_uFlags.IsFlagSet(ComF_PreventChangingTarget))
	{
		m_fSecondaryTargetTimer -= GetTimeStep();
		if(m_fSecondaryTargetTimer <= 0.0f)
		{			
			// if our timer runs out and we aren't currently using the secondary target then we should try to
			if( m_pPrimaryTarget && 
				!m_uFlags.IsFlagSet(ComF_UsingSecondaryTarget) &&
				!pPedIntell->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableSecondaryTarget) && !m_pPrimaryTarget->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDecoyPed))
			{
				Vec3V vMyPosition = pPed->GetTransform().GetPosition();

				bool bCanChooseNewTarget = false;
				s32 iState = GetState();
				if(iState == State_Fire || iState == State_Mounted)
				{
					// TODO add tunable for 15.0f distance
					ScalarV scDistToPrimaryTargetSq = DistSquared(m_pPrimaryTarget->GetTransform().GetPosition(), vMyPosition);
					ScalarV scMinDistanceSq = ScalarVFromF32(rage::square(ms_Tunables.m_fMinDistanceFromPrimaryTarget));
					if(IsGreaterThanOrEqualAll(scDistToPrimaryTargetSq, scMinDistanceSq))
					{
						bCanChooseNewTarget = true;
					}
				}

				// if we have a secondary target then use it and set the timer for how long we should use it
				m_pSecondaryTarget = bCanChooseNewTarget ? (pPedTargetting ? pPedTargetting->GetSecondBestTarget() : NULL) : NULL;
				if(m_pSecondaryTarget)
				{
					CSingleTargetInfo* pTargetInfo = pPedTargetting ? pPedTargetting->FindTarget(m_pSecondaryTarget) : NULL;
					if(pTargetInfo && pTargetInfo->GetLosStatus() == Los_clear)
					{
						Vec3V vPedToPrimaryTarget = m_pPrimaryTarget->GetTransform().GetPosition() - vMyPosition;
						Vec3V vPedToSecondaryTarget = m_pSecondaryTarget->GetTransform().GetPosition() - vMyPosition;
						ScalarV scAngleBetweenTargets = rage::Angle(vPedToPrimaryTarget, vPedToSecondaryTarget);
						if(IsLessThanOrEqualAll(scAngleBetweenTargets, ScalarVFromF32(ms_Tunables.m_fMaxAngleBetweenTargets)))
						{
							m_uFlags.SetFlag(ComF_UsingSecondaryTarget);
							m_fSecondaryTargetTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeToUseSecondaryTargetMin, ms_Tunables.m_fTimeToUseSecondaryTargetMax);
						}
					}
				}
			}
			else
			{
				// if we were using our secondary target then we should stop for now
				m_uFlags.ClearFlag(ComF_UsingSecondaryTarget);
			}

			// if we either stopped using the secondary target or failed to find out then we need to reset our timer
			if(!m_uFlags.IsFlagSet(ComF_UsingSecondaryTarget))
			{
				m_pSecondaryTarget = NULL;
				m_fSecondaryTargetTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenSecondaryTargetUsesMin, ms_Tunables.m_fTimeBetweenSecondaryTargetUsesMax);
			}
		}
		else if(m_uFlags.IsFlagSet(ComF_UsingSecondaryTarget))
		{
			// if already using a secondary target make sure it still exists, if not then make sure we are set to not use it
			if(!m_pSecondaryTarget || pPedIntell->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableSecondaryTarget))
			{
				m_pSecondaryTarget = NULL;
				m_uFlags.ClearFlag(ComF_UsingSecondaryTarget);
				m_fSecondaryTargetTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenSecondaryTargetUsesMin, ms_Tunables.m_fTimeBetweenSecondaryTargetUsesMax);
			}
		}
	}
	// Set the target entity in TaskMeleeas the target ped to prevent changing target while migrating - url:bugstar:7176130
	else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontChangeTargetFromMelee) && pPed->IsControlledByLocalPlayer())
	{
		if (CTaskMelee* meleeTask = static_cast<CTaskMelee*>(FindSubTaskOfType(CTaskTypes::TASK_MELEE)))
		{
			if (m_pPrimaryTarget && meleeTask->GetTargetEntity() && meleeTask->GetTargetEntity() != m_pPrimaryTarget.Get())
			{
				CPed* pTargetPed = const_cast<CPed*>(m_pPrimaryTarget.Get());
				meleeTask->SetTargetEntity(static_cast<CEntity*>(pTargetPed));

				AI_LOG_WITH_ARGS("CTaskCombat::UpdatePrimaryTarget - Setting TaskMelee target entity to %s, ped = %s, task = %p \n", m_pPrimaryTarget->GetLogName(), pPed->GetLogName(), this);
			}
		}
	}

	// Finally check which target to use and if it's different than our current target change it and mark the target changed this frame
	CPed* pBestTarget = m_pSecondaryTarget ? m_pSecondaryTarget.Get() : (pPedTargetting ? pPedTargetting->GetBestTarget() : NULL);
	if( pBestTarget )
	{
		if( pBestTarget != m_pPrimaryTarget && !m_uFlags.IsFlagSet(ComF_PreventChangingTarget) &&
			(!m_pPrimaryTarget || CanUpdatePrimaryTarget()) )
		{
			ChangeTarget(pBestTarget);
		}
	}
	else if( NetworkInterface::IsGameInProgress() && m_pPrimaryTarget && m_pPrimaryTarget->IsAPlayerPed() )
	{
		if( m_pPrimaryTarget->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_RESPAWN ||
			(m_pPrimaryTarget->IsNetworkClone() && m_pPrimaryTarget->GetPlayerInfo()->GetPlayerState() == CPlayerInfo::PLAYERSTATE_HASDIED) ||
			(m_pPrimaryTarget->GetPlayerInfo()->GetHasRespawnedWithinTime(ms_Tunables.m_MaxTimeToRejectRespawnedTarget) && (GetTimeRunning() * 1000.0f) > ms_Tunables.m_MaxTimeToRejectRespawnedTarget) )
		{		
			ChangeTarget(NULL);
			return false;
		}
	}

	// Tell the target that it is being targeted this frame
	CSingleTargetInfo* pTargetInfo = pPedTargetting ? pPedTargetting->FindTarget(m_pPrimaryTarget) : NULL;
	if(pTargetInfo)
	{
		pTargetInfo->SetIsBeingTargeted(true);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CTaskCombat::UpdateAmbientAnimPlayback( CPed* pPed )
{
	// If the move network expired then we should stop the clip
	CClipHelper* pClipHelper = GetClipHelper();
	if(pClipHelper)
	{
		if(pClipHelper->GetBlendHelper().HasNetworkExpired() || fwTimer::GetTimeInMilliseconds() > m_uTimeToForceStopAmbientAnim)
		{
			StopClip(FAST_BLEND_OUT_DELTA);
		}
	}

	//Ensure we can play an ambient anim.
	if(!CanPlayAmbientAnim())
	{
		return;
	}

	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

	// First check for any CTaskGun so we can see if it's doing a bullet reaction
	if(!pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN) || 
		pPedIntelligence->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_GUN) != CTaskGun::State_Aim ||
		GetIsPlayingAmbientClip())
	{
		return;
	}

	// First check for playing a quick glance then if we fail that we can try the others
	if(pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		// If we haven't chosen an anim yet then try playing an anim
		if(m_DesiredAmbientClipSetId == CLIP_SET_ID_INVALID)
		{
			// Make sure we're far enough from our target, if we aren't then we can't play the anims
			ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
			ScalarV scMinDistSq = ScalarVFromF32(ms_Tunables.m_fAmbientAnimsMinDistToTargetSq);
			if(IsLessThanAll(scDistSq, scMinDistSq))
			{
				return;
			}

			// Make sure we're close enough to our target
			ScalarV scMaxDistSq = ScalarVFromF32(ms_Tunables.m_fAmbientAnimsMaxDistToTargetSq);
			if(IsGreaterThanAll(scDistSq, scMaxDistSq))
			{
				return;
			}

			// Get our weapon info and return if we don't have one
			const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
			const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

			if(!pWeaponInfo || !pWeaponInfo->GetIsGun())
			{
				return;
			}

			if(!UpdateGestureAnim(pPed))
			{
				UpdateQuickGlanceAnim(pPed);
			}
		}

		// This will start the ambient anims that were chosen but don't use the move network
		RequestAndStartAmbientAnim(pPed);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CTaskCombat::UpdateQuickGlanceAnim( CPed* pPed )
{
#if __BANK
	if(!CCombatDebug::ms_bPlayGlanceAnims)
	{
		return false;
	}
#endif

	if(pPed->HasHurtStarted())
	{
		return false;
	}

	//Check if we just started playing an aim pose transition.
	if(JustStartedPlayingAimPoseTransition())
	{
		//Ensure we are not playing an ambient clip.
		if(GetIsPlayingAmbientClip())
		{
			return false;
		}
	}
	else
	{
		// make sure we can play this specific animation
		if(m_fQuickGlanceTimer > 0.0f || m_fTimeUntilAmbientAnimAllowed > 0.0f)
		{
			return false;
		}
	}

	// reset the quick glance timer, even if we fail the random we still want to have to wait again for this
	m_fQuickGlanceTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenQuickGlancesMin, ms_Tunables.m_fTimeBetweenQuickGlancesMax);

	// Randomly check now if we should play a glance anim.
	if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > .55f)
	{
		return false;
	}
	
	//Ensure the clip set is valid.
	fwMvClipSetId clipSetId = ChooseClipSetForQuickGlance(*pPed);
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	// Have a chance at playing animations
	CPed* pAllyForGlance = NULL;

	CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
	if(pCombatDirector && pCombatDirector->GetNumPeds() > 0)
	{
		ePedType pedsType = pPed->GetPedType();

		// Cycle through the peds, but use the shuffler
		int iNumPeds = pCombatDirector->GetNumPeds();
		SemiShuffledSequence shuffler(iNumPeds);
		for(int i = 0; i < iNumPeds; i++)
		{
			// Make sure this ped exists and that they are of the same ped type as we are (cops only gesture to cops, swat only gesture to swat)
			CPed* pOtherPed = pCombatDirector->GetPed(shuffler.GetElement(i));

			if(pOtherPed && pOtherPed != pPed && pOtherPed->GetPedType() == pedsType && 
				pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
			{
				// make this the ped we'll look at with glance anims (break if we only needed an ally for glance)
				pAllyForGlance = pOtherPed;
				break;
			}
		}

		// if we have an ally we chose to look at then use the ally for direction of the quick glance
		if(pAllyForGlance)
		{
			// Get information about ourselves
			Vec3V_ConstRef vCurPos = pPed->GetTransform().GetPosition();
			float fHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());

			//Generate the heading from the ped to the ally ped.
			Vec3V vAllyPos = pAllyForGlance->GetTransform().GetPosition();
			float fHeadingToOtherPed = fwAngle::LimitRadianAngleSafe(fwAngle::GetRadianAngleBetweenPoints(vAllyPos.GetXf(), vAllyPos.GetYf(), vCurPos.GetXf(), vCurPos.GetYf()));
			float fHeadingDelta = SubtractAngleShorter(fHeading, fHeadingToOtherPed);
			
			//Check if the heading is in generally the same direction.
			float fHeadingDifference = Abs(SubtractAngleShorter(fHeadingDelta, m_fLastQuickGlanceHeadingDelta));
			float fMaxHeadingDifference = ms_Tunables.m_MaxHeadingDifferenceForQuickGlanceInSameDirection;
			if(fHeadingDifference < fMaxHeadingDifference)
			{
				//Ensure the time since the last quick glance has exceeded the threshold.
				float fMinTimeBetweenQuickGlancesInSameDirection = ms_Tunables.m_MinTimeBetweenQuickGlancesInSameDirection;
				if(m_fTimeSinceLastQuickGlance < fMinTimeBetweenQuickGlancesInSameDirection)
				{
					return false;
				}
			}
			
			//Set the data for the quick glance.
			m_fTimeSinceLastQuickGlance = 0.0f;
			m_fLastQuickGlanceHeadingDelta = fHeadingDelta;
			
			//Clamp the heading delta.
			fHeadingDelta = Clamp(fHeadingDelta / PI, -1.0f, 1.0f);

			// figure out which animation to use based on the direction to the other ped
			m_DesiredAmbientClipId = GetAmbientClipFromHeadingDelta(fHeadingDelta);
			m_DesiredAmbientClipSetId = clipSetId;

			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskCombat::UpdateGestureAnim( CPed* pPed )
{
	PF_FUNC(UpdateGestureAnim);

	// Don't check if another ped has already done gesture probes this frame
	if(ms_bGestureProbedThisFrame)
	{
		return false;
	}

	if(pPed->HasHurtStarted())
	{
		return false;
	}

	//Ensure we haven't been aimed at for a while.
	if(pPed->GetPedIntelligence()->GetTimeSinceLastAimedAtByPlayer() < ms_Tunables.m_fTimeSinceLastAimedAtForGesture)
	{
		return false;
	}
	
	//Check if we just started playing an aim pose transition.
	if(JustStartedPlayingAimPoseTransition())
	{
		//Ensure we are not playing an ambient clip.
		if(GetIsPlayingAmbientClip())
		{
			return false;
		}
	}

	// Quit out if it isn't time to play an anim yet
	if(m_fGestureTimer > 0.0f || m_fTimeUntilAmbientAnimAllowed > 0.0f || ms_fTimeUntilNextGestureGlobal > 0.0f)
	{
		return false;
	}

	CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
	if(pCombatDirector && pCombatDirector->GetNumPeds() > 0)
	{
		// Even though the last aimed at said we were no longer aimed at, let's still check the local player's target
		CPlayerInfo* pMainPlayerInfo = CGameWorld::GetMainPlayerInfo();
		if(pMainPlayerInfo && pMainPlayerInfo->GetTargeting().GetTarget() == pPed)
		{
			pPed->GetPedIntelligence()->SetTimeLastAimedAtByPlayer(fwTimer::GetTimeInMilliseconds());
			return false;
		}

		// Get information about ourselves
		Vec3V_ConstRef vCurPos = pPed->GetTransform().GetPosition();
		ePedType pedsType = pPed->GetPedType();

		CPed* pAllyToBeckon = NULL;
		CPed* pAllyBehindMe = NULL;

		// Figure out if we are moving forwards
		bool bIsPedMovingForward = false;
		if(pPed->GetVelocity().Mag2() > 1.0f)
		{
			Vec3V vPedVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
			ScalarV scDot = Dot(pPed->GetTransform().GetForward(), Normalize(vPedVelocity));
			bIsPedMovingForward = (IsGreaterThanAll(scDot, ScalarVFromF32(0.85f)) != 0);
		}

		// load the desired distance to any target into a scalar
		ScalarV scTargetDistSq = ScalarVFromF32(rage::square(2.5f));

		// Cycle through the peds, but use the shuffler
		int iNumPeds = pCombatDirector->GetNumPeds();
		SemiShuffledSequence shuffler(iNumPeds);
		for(int i = 0; i < iNumPeds; i++)
		{
			// Make sure this ped exists and that they are of the same ped type as we are (cops only gesture to cops, swat only gesture to swat)
			CPed* pOtherPed = pCombatDirector->GetPed(shuffler.GetElement(i));

			if(pOtherPed && pOtherPed != pPed)
			{
				if( pOtherPed->GetPedType() != pedsType ||
					!pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) )
				{
					continue;
				}

				CVehicle* pOtherVehicle = pOtherPed->GetVehiclePedInside();
				if(pOtherVehicle && pOtherVehicle->InheritsFromHeli())
				{
					continue;
				}

				// Check the direction to the other ped (they need to be behind us)
				Vec3V vOtherPedPos = pOtherPed->GetTransform().GetPosition();
				Vec3V vToOtherPed = vOtherPedPos - vCurPos;

				// Check if the target is far enough away
				ScalarV scDistSq = MagSquared(vToOtherPed);
				if(IsGreaterThanAll(scDistSq, scTargetDistSq))
				{
					// Check if the ped is exiting their vehicle
					if(pOtherPed->IsExitingVehicle())
					{
						pAllyToBeckon = pOtherPed;
					}

					// Don't probe more than one ally a frame
					if(ms_bGestureProbedThisFrame)
					{
						continue;
					}

					ScalarV scDot = Dot(pPed->GetTransform().GetB(), vToOtherPed);
					if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
					{
						ms_bGestureProbedThisFrame = true;

						if(CPedGeometryAnalyser::CanPedTargetPedAsync(*pPed, *pOtherPed) == ECanTarget)
						{
							// Set this ped as the ally behind us
							pAllyBehindMe = pOtherPed;

							// Don't bother checking any more peds for needing to "beckon" if we've already found one
							if(!pAllyToBeckon)
							{
								// Check if the ped is moving towards us
								scDot = Dot(VECTOR3_TO_VEC3V(pOtherPed->GetVelocity()), vToOtherPed);
								if(pOtherPed->GetVelocity().Mag2() > 1.0f && IsLessThanAll(scDot, ScalarV(V_ZERO)))
								{
									pAllyToBeckon = pOtherPed;
								}
							}
						}
					}
				}
				else
				{
					// Reset the gesture timer in fail cases so we don't continually run probes towards allies every frame
					m_fGestureTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenFailedGestureMin, ms_Tunables.m_fTimeBetweenFailedGestureMax);

					// We have an ally that is too close, don't play any gesture
					return false;
				}
			}
		}
		
		//Check if the ped can do beckon or over there.
		bool bCanPedDoBeckonOrOverThere = ((pedsType == PEDTYPE_SWAT) || pPed->IsGangPed());

		// If swat have a ped moving towards us play the come forward anim
		GestureType nGestureType = GT_None;
		if(bCanPedDoBeckonOrOverThere && (pAllyToBeckon || (pAllyBehindMe && bIsPedMovingForward)))
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				nGestureType = GT_Beckon;
			}
			else
			{
				nGestureType = GT_OverThere;
			}
		}
		// otherwise if we do not have a ped moving towards us and do have an ally behind us play a halt anim
		else if(!pAllyToBeckon && pAllyBehindMe)
		{
			nGestureType = GT_Halt;
		}
		
		m_DesiredAmbientClipSetId = ChooseClipSetForGesture(*pPed, nGestureType);
		if(m_DesiredAmbientClipSetId != CLIP_SET_ID_INVALID)
		{
			//Generate the heading from the ped to the ally ped
			float fHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
			Vec3V vAllyPos = pAllyToBeckon ? pAllyToBeckon->GetTransform().GetPosition() : pAllyBehindMe->GetTransform().GetPosition();
			float fHeadingToOtherPed = fwAngle::LimitRadianAngleSafe(fwAngle::GetRadianAngleBetweenPoints(vAllyPos.GetXf(), vAllyPos.GetYf(), vCurPos.GetXf(), vCurPos.GetYf()));
			float fHeadingDelta = SubtractAngleShorter(fHeading, fHeadingToOtherPed);
			fHeadingDelta = Clamp(fHeadingDelta / PI, -1.0f, 1.0f);

			// figure out which animation to use based on the direction to the other ped
			m_DesiredAmbientClipId = GetAmbientClipFromHeadingDelta(fHeadingDelta);

			// swat have a chance of playing a different animation at this angle
			if(pedsType == PEDTYPE_SWAT && fHeadingDelta < -0.75f && fwRandom::GetRandomTrueFalse())
			{
				m_DesiredAmbientClipSetId = ms_SwatGetsureClipSetId;
				m_DesiredAmbientClipId = fwMvClipId("freeze",0xBD42AA91);
			}
			
			ms_fTimeUntilNextGestureGlobal = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenGesturesMinGlobal, ms_Tunables.m_fTimeBetweenGesturesMaxGlobal);
			m_fGestureTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenGestureAnimsMin, ms_Tunables.m_fTimeBetweenGestureAnimsMax);
			return true;
		}
	}

	// Reset the gesture timer in fail cases so we don't continually run probes towards allies every frame
	m_fGestureTimer = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenFailedGestureMin, ms_Tunables.m_fTimeBetweenFailedGestureMax);

	return false;
}

fwMvClipId CTaskCombat::GetAmbientClipFromHeadingDelta(float fHeadingDelta)
{
	// figure out which animation to use based on the direction to the other ped
	if(fHeadingDelta < -0.75f)
	{
		return fwMvClipId("-180",0x523E57AD);
	}
	else if(fHeadingDelta < -0.25f)
	{
		return fwMvClipId("-90",0x7C913D9E);
	}
	else if(fHeadingDelta < 0.25f)
	{
		return fwMvClipId("0",0x6E3C5C6B);
	}
	else if(fHeadingDelta < 0.75)
	{
		return fwMvClipId("90",0xA4220602);
	}
	else
	{
		return fwMvClipId("180",0xC03C9089);
	}
}

bool CTaskCombat::RequestAndStartAmbientAnim( CPed* pPed )
{
	//Keep track of whether we should reset the animation.
	bool bResetAnimation = false;

	//Keep track of whether we played an animation.
	bool bPlayedAnimation = false;

	//Check if we have an animation.
	if(m_DesiredAmbientClipSetId != CLIP_SET_ID_INVALID && m_DesiredAmbientClipId != CLIP_ID_INVALID)
	{
		//Request the clip set.
		m_AmbientClipSetRequestHelper.RequestClipSet(m_DesiredAmbientClipSetId);

		//Check if the clip set should be streamed out.
		if(m_AmbientClipSetRequestHelper.ShouldClipSetBeStreamedOut())
		{
			//Reset the animation.
			bResetAnimation = true;
		}
		//Check if the clip set is streamed in.
		else if(m_AmbientClipSetRequestHelper.ShouldClipSetBeStreamedIn() && m_AmbientClipSetRequestHelper.IsClipSetStreamedIn())
		{
			//Start the animation.
			if(StartClipBySetAndClip(pPed, m_DesiredAmbientClipSetId, m_DesiredAmbientClipId, APF_ISFINISHAUTOREMOVE|APF_UPPERBODYONLY, (u32)BONEMASK_UPPERONLY, AP_MEDIUM, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish))
			{
				//Note that we played an animation.
				bPlayedAnimation = true;
			}

			//Reset the animation.
			bResetAnimation = true;
		}
	}
	else
	{
		//Release the clip set.
		m_AmbientClipSetRequestHelper.ReleaseClipSet();
	}

	//Check if we played the animation.
	if(bPlayedAnimation)
	{
		if(GetClipHelper())
		{
			m_uTimeToForceStopAmbientAnim = fwTimer::GetTimeInMilliseconds() + (u32)((GetClipHelper()->GetDuration() + ms_Tunables.m_AmbientAnimLengthBuffer) * 1000.0f);
		}

		//Check the gesture type.
		switch(GetGestureTypeForClipSet(m_DesiredAmbientClipSetId, pPed))
		{
			case GT_Beckon:
			{
				GetPed()->NewSay("COME_OVER_HERE");
				break;
			}
			case GT_OverThere:
			{
				float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
				if(fRandom <= 0.33f)
				{
					GetPed()->NewSay("MOVE_IN");
				}
				else if(fRandom <= 0.66f)
				{
					GetPed()->NewSay("MOVE_IN_PERSONAL");
				}
				else
				{
					GetPed()->NewSay("GET_HIM");
				}

				break;
			}
			case GT_Halt:
			{
				GetPed()->NewSay("WAIT");
				break;
			}
			default:
			{
				break;
			}
		}
	}

	//Check if we should reset the animation.
	if(bResetAnimation)
	{
		//Randomize the time.
		m_fTimeUntilAmbientAnimAllowed = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fTimeBetweenPassiveAnimsMin, ms_Tunables.m_fTimeBetweenPassiveAnimsMax);

		//Clear the clip set and clip ids.
		m_DesiredAmbientClipSetId	= CLIP_SET_ID_INVALID;
		m_DesiredAmbientClipId		= CLIP_ID_INVALID;
	}

	return bPlayedAnimation;
}

//////////////////////////////////////////////////////////////////////////

void CTaskCombat::ChooseMovementAttributes(CPed* pPed, bool& bShouldStrafe, float& fMoveBlendRatio)
{
	// If we don't get a ped then set our params to some sensible defaults
	if(!pPed)
	{
		bShouldStrafe = true;
		fMoveBlendRatio = MOVEBLENDRATIO_RUN;
		return;
	}

	if(pPed->HasHurtStarted())
	{
		bShouldStrafe = true;
		fMoveBlendRatio = MOVEBLENDRATIO_WALK;		
		return;
	}

	// Check if we should strafe
	bShouldStrafe = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatStrafeWhenMovingChance);

	// If we can't strafe then make sure we are at least running, otherwise check if we should walk
	if(!bShouldStrafe)
	{
		fMoveBlendRatio = MAX(fMoveBlendRatio, MOVEBLENDRATIO_RUN);
	}
	else if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWalkWhenStrafingChance))
	{
		fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	}
}

s32 CTaskCombat::GenerateCombatRunningFlags(bool bShouldStrafe, float fDesiredMbr)
{
	s32 iCombatRunningFlags = bShouldStrafe ? CTaskCombatAdditionalTask::RT_Default : CTaskCombatAdditionalTask::RT_DefaultJustClips;
	if(fDesiredMbr >= MOVEBLENDRATIO_RUN)
	{
		iCombatRunningFlags |= CTaskCombatAdditionalTask::RT_AllowAimFireToggle;
	}

	return iCombatRunningFlags;
}

bool CTaskCombat::ShouldReactBeforeAimIntroForEvent(const CPed* pPed, const CEvent& rEvent, bool& bUseFlinch, bool& bUseSurprised, bool& bUseSniper)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Clear the flags.
	bUseFlinch		= false;
	bUseSurprised	= false;
	bUseSniper		= false;

	//Init the min distance that we are allowed to use the sniper reaction
	static dev_float s_fMinDistanceToUseSniper = 30.0f;

	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_GUN_AIMED_AT:
		case EVENT_RESPONDED_TO_THREAT:
		case EVENT_SHOUT_TARGET_POSITION:
		{
			//Check if we already have a weapon equipped.  Surprised animations don't work well unless we are unarmed.
			if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon() &&
				!pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsUnarmed())
			{
				bUseFlinch = true;
			}
			else
			{
				bUseSurprised = true;
			}

			return true;
		}
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_SHOCKING_GUN_FIGHT:
		case EVENT_SHOCKING_GUNSHOT_FIRED:
		{
			//Check if the source is far away.
			ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), rEvent.GetEntityOrSourcePosition());
			ScalarV scMinDistToUseSniperSq = ScalarVFromF32(square(s_fMinDistanceToUseSniper));
			if(IsGreaterThanAll(scDistSq, scMinDistToUseSniperSq))
			{
				bUseSniper = true;
			}
			else
			{
				bUseFlinch = true;
			}

			return true;
		}
		case EVENT_SHOCKING_SEEN_PED_KILLED:
		{
			// Check if we heard a gunshot withing 
			static dev_u32 s_iTreatAsShotFiredTimeMs = 500;
			u32 iDiffGunshot = fwTimer::GetTimeInMilliseconds() - pPed->GetPedIntelligence()->GetLastHeardGunFireTime();
			if (iDiffGunshot < s_iTreatAsShotFiredTimeMs)
			{
				//Check if the source is far away.
				ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), rEvent.GetEntityOrSourcePosition());
				ScalarV scMinDistToUseSniperSq = ScalarVFromF32(square(s_fMinDistanceToUseSniper));
				if(IsGreaterThanAll(scDistSq, scMinDistToUseSniperSq))
				{
					bUseSniper = true;
				}
				else
				{
					bUseFlinch = true;
				}

				return true;
			}
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskCombat::ShouldArrestTargetForEvent(const CPed* pPed, const CPed* pTargetPed, const CEvent& rEvent)
{
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	if(!pTargetPed || !pPed)
	{
		return false;
	}

	if(!pPed->IsCopPed())
	{
		return false;
	}

	CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
	if(pMyVehicle && pMyVehicle->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		return false;
	}

	CVehicle* pTargetVehicle = pTargetPed->GetVehiclePedInside();
	if(pTargetVehicle && (pTargetVehicle->GetLayoutInfo()->GetDisableJackingAndBusting() || pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DISABLE_BUSTING)))
	{
		return false;
	}

	eWantedLevel nWantedLevel = pTargetPed->GetPlayerWanted() ? pTargetPed->GetPlayerWanted()->GetWantedLevel() : WANTED_CLEAN;
	switch(rEvent.GetEventType())
	{
		case EVENT_SHOUT_TARGET_POSITION:
		case EVENT_RESPONDED_TO_THREAT:
		case EVENT_ACQUAINTANCE_PED_WANTED:
		case EVENT_MELEE_ACTION:
		case EVENT_SHOCKING_CAR_ALARM:
		{
			if(nWantedLevel > WANTED_LEVEL1)
			{
				return false;
			}

			break;
		}
		case EVENT_DAMAGE:
		{
			if(nWantedLevel > WANTED_LEVEL1)
			{
				return false;
			}

			if(static_cast<const CEventDamage &>(rEvent).GetWeaponUsedHash() != WEAPONTYPE_FALL.GetHash())
			{
				return false;
			}

			break;
		}
		default:
		{
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskCombat::IsAnImmediateMeleeThreat(const CPed* pPed, const CPed* pTargetPed, const CEvent& rEvent)
{
	if(!pTargetPed || !pPed)
	{
		return false;
	}

	if(pPed->GetIsInVehicle() || pTargetPed->GetIsInVehicle())
	{
		return false;
	}

	switch(rEvent.GetEventType())
	{
		case EVENT_MELEE_ACTION:
		case EVENT_DAMAGE:
		case EVENT_GUN_AIMED_AT:
		{
			return true;
		}
		default:
			break;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskCombat::ShouldMaintainMinDistanceToTarget(const CPed& rPed)
{
	//Check if the ped has the flag set.
	if(rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_MaintainMinDistanceToTarget))
	{
		return true;
	}

	//Check if the ped is defensive, AND does not have a defensive area.
	if((rPed.GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_Defensive) &&
		!rPed.GetPedIntelligence()->GetDefensiveArea()->IsActive())
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskCombat::UpdateCoverStatus( CPed* pPed, bool bIncrementTimers )
{
	if( pPed->GetCoverPoint() )
	{
		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UsingCoverPoint, true );
		if( pPed->GetCoverPoint()->IsCoverPointMoving(COVER_MOVMENT_THRESHOLD) )
		{
			m_fCoverMovementTimer = 0.0f;
		}
		else if(bIncrementTimers)
		{
			m_fCoverMovementTimer += GetTimeStep();
			m_fCoverMovementTimer = MIN( m_fCoverMovementTimer, COVER_MOVEMENT_MAX_TIME );
		}

		m_fTimeSinceWeLastHadCover = 0.0f;
	}
	else
	{
		m_fCoverMovementTimer = COVER_MOVEMENT_MAX_TIME;

		if(bIncrementTimers)
		{
			m_fTimeSinceWeLastHadCover += GetTimeStep();
		}
	}
}

void CTaskCombat::UpdateCoverFinder( CPed* pPed )
{
	// Get the combat movement and behavior
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	CCombatData::Movement combatMovement = pPedIntelligence->GetCombatBehaviour().GetCombatMovement();
	CCombatBehaviour& combatBehaviour = pPedIntelligence->GetCombatBehaviour();
	
	// Only do cover searches if we aren't set to be stationary, can use cover and are not in a vehicle or on a mount
	if(combatMovement != CCombatData::CM_Stationary && combatBehaviour.IsFlagSet(CCombatData::BF_CanUseCover) && pPed->GetIsOnFoot())
	{
		// Acquire our cover finder
		if(!m_pCoverFinder)
		{
			m_pCoverFinder = pPed->GetPedIntelligence()->GetCoverFinder();
		}

			// make sure the cover finder isn't already active
		if(m_pCoverFinder && !m_pCoverFinder->IsActive())
		{
			// Check whether we have cover or not and update the proper timer
			if(pPed->GetCoverPoint())
			{
				m_fTimeUntilNextAltCoverSearch -= GetTimeStep();
			}
			else
			{
				m_coverSearchTimer.Tick(GetTimeStep());
			}

			// Get our combat task state
			s32 iCombatState = GetState();

			// Make sure enough time has elapsed and we aren't in a state that doesn't allow us to look for cover
			if((m_fTimeUntilNextAltCoverSearch <= 0.0f || m_coverSearchTimer.IsFinished()) && 
				iCombatState != State_MoveWithinAttackWindow && iCombatState != State_MoveToCover)
			{
				Vector3 vCoverSearchStartPos(0.0f, 0.0f, 0.0f);
				float fWindowMin = 0.0f;
				float fWindowMax = 0.0f;
				s32 iFlags = 0;
				
				CDefensiveArea* pDefensiveAreaForCoverSearch = pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch();
				if(pDefensiveAreaForCoverSearch && pDefensiveAreaForCoverSearch->IsActive())
				{
					pDefensiveAreaForCoverSearch->GetCentre(vCoverSearchStartPos);
					pDefensiveAreaForCoverSearch->GetMaxRadius(fWindowMax);
				}
				else if( combatBehaviour.GetCombatMovement() == CCombatData::CM_WillAdvance && iCombatState != State_TargetUnreachable )
				{
					// Setup our search params as well as setting the cover start search pos and the search mode
					CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(pPed, fWindowMin, fWindowMax, true);
					vCoverSearchStartPos = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
					iFlags |= CTaskCover::CF_SearchInAnArcAwayFromTarget;
					iFlags |= CTaskCover::CF_RequiresLosToTarget;
				}
				else
				{
					vCoverSearchStartPos =  VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					fWindowMax = 15.0f;
				}

				if( iCombatState == State_TargetUnreachable )
				{
					iFlags = CTaskCover::CF_FindClosestPointAtStart;
				}

				// The only flag we care about right now is the require los to target flag
				CPedTargetting* pPedTargetting = pPedIntelligence->GetTargetting(true);
				if (pPedTargetting)
				{
					m_pCoverFinder->SetupSearchParams(pPedTargetting->GetBestTarget(), VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()), vCoverSearchStartPos, iFlags, fWindowMin, fWindowMax,
						CCover::CS_MUST_PROVIDE_COVER, CCoverFinderFSM::COVER_SEARCH_BY_POS, INVALID_COVER_POINT_INDEX, CCoverPointFilterTaskCombat::Construct);
					m_pCoverFinder->StartSearch(pPed);
				}

				ResetCoverSearchTimer();
				m_fTimeUntilNextAltCoverSearch = ms_Tunables.m_fTimeBetweenAltCoverSearches * fwRandom::GetRandomNumberInRange(0.9f, 1.1f);
			}
		}
	}
}

bool CTaskCombat::DoesCoverPointProvideCover(CCoverPoint* pCoverPoint)
{
	if(!pCoverPoint)
	{
		return false;
	}

	Vec3V vTargetPosition = m_pPrimaryTarget->GetTransform().GetPosition();
	Vector3 vCoverPosition;
	pCoverPoint->GetCoverPointPosition(vCoverPosition);

	CPed* pPed = GetPed();
	if(ShouldMaintainMinDistanceToTarget(*pPed))
	{
		// load the desired distance to any target into a scalar
		ScalarV scTargetDistSq = ScalarVFromF32(rage::square(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMinimumDistanceToTarget)));

		// Check if the target is far enough away
		ScalarV scDistSq = DistSquared(vTargetPosition, VECTOR3_TO_VEC3V(vCoverPosition));
		if(IsLessThanAll(scDistSq, scTargetDistSq))
		{
			return false;
		}
	}

	return CCover::DoesCoverPointProvideCoverFromTargets(pPed, VEC3V_TO_VECTOR3(vTargetPosition), pCoverPoint, pCoverPoint->GetArcToUse(*pPed, m_pPrimaryTarget), 
														 vCoverPosition, m_pPrimaryTarget);
}

bool CTaskCombat::CanUseDesiredCover() const
{
	CCoverPoint* pDesiredCover = GetPed()->GetDesiredCoverPoint();

	return pDesiredCover && pDesiredCover->CanAccomodateAnotherPed() && !pDesiredCover->IsDangerous() && !pDesiredCover->IsCloseToPlayer(*GetPed());
}

bool CTaskCombat::HasValidatedDesiredCover() const
{
	return (m_pCoverFinder && !m_pCoverFinder->IsActive() && CanUseDesiredCover() );
}

bool CTaskCombat::CanPedBeConsideredInCover( const CPed* pPed ) const
{
	if(pPed->GetCoverPoint())
	{
		const bool bStationary = m_fCoverMovementTimer > COVER_STATIONARY_TIME;

		if(FindSubTaskOfType(CTaskTypes::TASK_IN_COVER) || pPed->GetIsInCover())
		{
			if(bStationary && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseCover))
			{
				Vector3 vCoverCoords(Vector3::ZeroType);
				pPed->GetCoverPoint()->GetCoverPointPosition(vCoverCoords);
				Vector3 vPedToCover = vCoverCoords - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

				if( vPedToCover.XYMag2() < ms_Tunables.m_MaxDistToCoverXY * ms_Tunables.m_MaxDistToCoverXY &&
					Abs(vPedToCover.z) < ms_Tunables.m_MaxDistToCoverZ )
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

bool CTaskCombat::ShouldChaseTarget( CPed* pPed, const CPed* pTarget)
{
	// Don't chase if hurt
	if(pPed->HasHurtStarted())
	{
		return false;
	}

	// Must be able to advance in order to chase
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() != CCombatData::CM_WillAdvance)
	{
		return false;
	}

	// Ensure the target is not swimming
	if(pTarget->GetIsSwimming())
	{
		return false;
	}
	
	// Target must not be in a vehicle
	if(pTarget->GetVehiclePedInside())
	{
		return false;
	}

	// If our combat behavior doesn't allow us chasing then return false
	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanChaseTargetOnFoot))
	{
		return false;
	}

	// Lots of checking to do here, check if our target was seen and get the last seen position
	Vector3 vTarget(Vector3::ZeroType);
	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(true);
	bool bTargetSeen = pPedTargetting && pPedTargetting->AreTargetsWhereaboutsKnown(&vTarget, pTarget);
	if(vTarget.IsZero())
	{
		return false;
	}

	// Get the to target vector, get the distance to our target, check if the target is running a gun task
	Vec3V vMyPosition = pPed->GetTransform().GetPosition();
	Vec3V vToTarget = VECTOR3_TO_VEC3V(vTarget) - vMyPosition;
	ScalarV scDistToTargetSq = MagSquared(vToTarget);
	bool bTargetRunningGunTask = pTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN);

	// We do some different logic if we are not yet chasing
	if(GetState() != State_ChaseOnFoot)
	{
		// First, if we aren't already chasing then we need to make sure there isn't enough people chasing already
		if(ms_uNumPedsChasingOnFoot >= ms_Tunables.m_MaxNumPedsChasingOnFoot)
		{
			return false;
		}

		// OK, to start chasing our target has to be at least running, be running and facing AWAY from us, be far enough away 
		// And the target should not be running a gun task
		if( !bTargetRunningGunTask && !pTarget->GetMotionData()->GetIsLessThanRun() )
		{
			ScalarV scStartChasingDistSq = ScalarVFromF32(rage::square(START_CHASING_DIST));
			if(IsLessThanOrEqualAll(scDistToTargetSq, scStartChasingDistSq))
			{
				return false;
			}

			ScalarV scTargetHeadingDot = Dot(pTarget->GetTransform().GetB(), vToTarget);
			ScalarV scMinDot = ScalarVFromF32(.1f);
			if(IsLessThanOrEqualAll(scTargetHeadingDot, scMinDot))
			{
				return false;
			}

			ScalarV scTargetVelocityDot = Dot(VECTOR3_TO_VEC3V(pTarget->GetVelocity()), vToTarget);
			if(IsLessThanOrEqualAll(scTargetVelocityDot, scMinDot))
			{
				return false;
			}

			ScalarV scMaxDistSq = LoadScalar32IntoScalarV(3.5f);
			scMaxDistSq = (scMaxDistSq * scMaxDistSq);

			CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
			for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
			{
				CPed* pNearbyPed = static_cast<CPed*>(pEntity);
				if(pNearbyPed)
				{
					ScalarV scDistSq = DistSquared(vMyPosition, pNearbyPed->GetTransform().GetPosition());
					if(IsLessThanAll(scDistSq, scMaxDistSq))
					{
						if( pPed->GetPedIntelligence()->IsFriendlyWith(*pNearbyPed)&&
							pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true) &&
							pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_COMBAT) == State_ChaseOnFoot )
						{
							return false;
						}

						continue;
					}

					break;
				}
			}

			return true;
		}
	}
	// If we are chasing and our target hasn't stopped
	else if(!pTarget->GetMotionData()->GetIsStill())
	{
		// We are chasing and we can see the target, target is not running a gun task, and we are far enough away
		// OR We are chasing and target not seen and are far enough away then we are allowed to continue chasing
		if(bTargetSeen && !bTargetRunningGunTask)
		{
			ScalarV scStopChasingDistSq = ScalarVFromF32(rage::square(STOP_CHASING_DIST_TARGET_MOVING));
			if(IsGreaterThanAll(scDistToTargetSq, scStopChasingDistSq))
			{
				return true;
			}
		}
		else if(!bTargetSeen)
		{
			ScalarV scStopChasingDistSq = ScalarVFromF32(rage::square(STOP_CHASING_DIST_TARGET_NOT_SEEN));
			if(IsGreaterThanAll(scDistToTargetSq, scStopChasingDistSq))
			{
				return true;
			}
		}
	}

	return false;
}

bool CTaskCombat::ShouldDrawWeapon() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}
	
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}
	
	//Ensure the ped is not armed with a gun.
	if(pWeaponManager->GetIsArmedGun())
	{
		return false;
	}
	
	//Ensure the best weapon info is valid.
	const CWeaponInfo* pInfo = pWeaponManager->GetBestWeaponInfo();
	if(!pInfo)
	{
		return false;
	}
	
	//Ensure the best weapon is a gun.
	if(!pInfo->GetIsGun())
	{
		return false;
	}
	
	//At this point, we know the ped has a gun but does not have it armed.
	//There are a few conditions that should cause them to draw:
	//	1) Their target draws a gun.
	//	2) They are losing a fight badly.
	//	3) One of their buddies draws a gun.
	
	//Check if the target is valid.
	if(m_pPrimaryTarget)
	{
		//Check if the weapon manager is valid.
		if(m_pPrimaryTarget->GetWeaponManager())
		{
			//Check if the target is armed with a gun.
			if(m_pPrimaryTarget->GetWeaponManager()->GetIsArmedGun())
			{
				return true;
			}
		}
	}
	
	//Check if the ped is willing to draw a weapon if losing.
	if(m_uFlags.IsFlagSet(ComF_DrawWeaponWhenLosing))
	{
		//Ensure the target is valid.
		if(m_pPrimaryTarget)
		{
			//Normalize the health.
			float fCurrent = pPed->GetHealth() / pPed->GetMaxHealth();
			float fTarget = m_pPrimaryTarget->GetHealth() / m_pPrimaryTarget->GetMaxHealth();

			//Check if the ped is losing by a significant margin.
			if(fCurrent <= (fTarget * 0.75f))
			{
				return true;
			}
		}
	}

	//Check if a buddy has drawn a gun.
	const CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
	if(pCombatDirector && (pCombatDirector->GetNumPedsWithGuns() > 0))
	{
		return true;
	}
	
	return false;
}

bool CTaskCombat::CheckForNavFailure( CPed* pPed )
{
	CTask* pSubTask = GetSubTask();

	// Check if we are running a nav mesh task that can't find a route.
	if( pSubTask &&	pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&
		((CTaskComplexControlMovement*)pSubTask)->GetBackupCopyOfMovementSubtask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
	{
		CTaskMoveFollowNavMesh* pNavmeshTask = (CTaskMoveFollowNavMesh*)((CTaskComplexControlMovement*)pSubTask)->GetRunningMovementTask(pPed);
		if( pNavmeshTask && pNavmeshTask->IsUnableToFindRoute() )
		{
			return true;
		}
	}

	return false;
}

bool CTaskCombat::CheckForTargetLossResponse()
{
	// Calculate the target location status
	CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting();
	if(pPedTargetting && !pPedTargetting->AreTargetsWhereaboutsKnown(NULL, m_pPrimaryTarget) )
	{
		SetTaskCombatState(State_DecideTargetLossResponse);
		return true;
	}

	return false;
}

bool CTaskCombat::DoesPedHaveGun(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the ped is armed with a gun.
	if(!pWeaponManager->GetIsArmedGun())
	{
		return false;
	}

	return true;
}

bool CTaskCombat::DoesPedNeedHelp(CPed* pPed) const
{
	//Ensure the ped is injured.
	if(!pPed->IsInjured())
	{
		return false;
	}
	
	//Ensure the ped is not being helped.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsBeingDraggedToSafety))
	{
		return false;
	}
	
	//Ensure the ped has not been helped.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasBeenDraggedToSafety))
	{
		return false;
	}

	//Ensure the ped can use a ragdoll.
	if(!CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DRAGGED_TO_SAFETY))
	{
		return false;
	}
	
	return true;
}

bool CTaskCombat::DoesTargetHaveGun() const
{
	//Ensure the target is valid.
	if(!m_pPrimaryTarget)
	{
		return false;
	}

	//Ensure the target has a gun.
	if(!DoesPedHaveGun(*m_pPrimaryTarget))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::CanHelpAnyPed() const
{
	//Ensure the ped is not injured.
	if(GetPed()->IsInjured())
	{
		return false;
	}

	// And not in hunched or hurt mode
	if(GetPed()->HasHurtStarted())
	{
		return false;
	}
	
	//Ensure the ped will help other injured peds.
	CCombatBehaviour& combatBehaviour = GetPed()->GetPedIntelligence()->GetCombatBehaviour();
	if(!combatBehaviour.IsFlagSet(CCombatData::BF_WillDragInjuredPedsToSafety))
	{
		return false;
	}
	
	//Ensure the ped is not currently helping another ped.
	if(GetState() == State_DragInjuredToSafety)
	{
		return false;
	}
	
	return true;
}

bool CTaskCombat::CanHelpSpecificPed(CPed* pPed, const CPed* pTarget) const
{
	//Ensure the peds are friendly.
	if(!GetPed()->GetPedIntelligence()->IsFriendlyWith(*pPed))
	{
		return false;
	}
	
	//Swat peds will only help fellow swat peds.
	if(GetPed()->GetPedType() == PEDTYPE_SWAT && pPed->GetPedType() != PEDTYPE_SWAT)
	{
		return false;
	}

	//Check if we aren't fire resistant, and the ped is on fire.
	if(!GetPed()->IsFireResistant() && g_fireMan.IsEntityOnFire(pPed))
	{
		return false;
	}
	
	//Grab the helper position.
	Vec3V vHelperPosition = GetPed()->GetTransform().GetPosition();
	
	//Grab the injured position.
	Vec3V vInjuredPosition = pPed->GetTransform().GetPosition();
	
	//Calculate the distance from the helper to the injured.
	ScalarV scHelperToInjuredDistSq = DistSquared(vHelperPosition, vInjuredPosition);
	
	//Ensure the distance has not exceeded the threshold.
	ScalarV scMaxDistanceToHelpPedSq = ScalarVFromF32(square(ms_Tunables.m_MaxDistanceFromPedToHelpPed));
	if(IsGreaterThanAll(scHelperToInjuredDistSq, scMaxDistanceToHelpPedSq))
	{
		return false;
	}
	
	//Check if the target is valid.
	if(pTarget)
	{
		//Grab the target position.
		Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
		
		//Calculate the distance from the helper to the target.
		ScalarV scHelperToTargetDistSq = DistSquared(vHelperPosition, vTargetPosition);
		
		//Ensure we are closer to the injured ped than the target.
		if(IsGreaterThanAll(scHelperToInjuredDistSq, scHelperToTargetDistSq))
		{
			return false;
		}
		
		//Calculate the direction from the helper to the target.
		Vec3V vHelperToTarget = Subtract(vTargetPosition, vHelperPosition);
		Vec3V vHelperToTargetDirection = NormalizeFastSafe(vHelperToTarget, Vec3V(V_ZERO));
		
		//Calculate the direction from the helper to the injured ped.
		Vec3V vHelperToInjured = Subtract(vInjuredPosition, vHelperPosition);
		Vec3V vHelperToInjuredDirection = NormalizeFastSafe(vHelperToInjured, Vec3V(V_ZERO));
		
		//Ensure the helper won't have to move towards the target.
		ScalarV scDot = Dot(vHelperToTargetDirection, vHelperToInjuredDirection);
		ScalarV scMaxDot = ScalarVFromF32(ms_Tunables.m_MaxDotToTargetToHelpPed);
		if(IsGreaterThanAll(scDot, scMaxDot))
		{
			return false;
		}
	}
	
	return true;
}

void CTaskCombat::UpdateVariedAimPoseMoveAwayFromPosition( CPed* pPed )
{
	m_fTimeUntilAllyProximityCheck -= GetTimeStep();
	if(m_fTimeUntilAllyProximityCheck <= 0.0f)
	{
		CTask* pSubTask = GetSubTask();
		if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_VARIED_AIM_POSE)
		{
			// Loop through nearby peds and check if any allies are too close, then move away from the closest
			CPed* pAllyToMoveAwayFrom = NULL;
			ScalarV scMaxDistSq = ScalarVFromF32(square(ms_Tunables.m_fMaxDstanceToMoveAwayFromAlly));

			CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
			for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
			{
				CPed* pNearbyPed = static_cast<CPed*>(pEntity);
				if(pNearbyPed && pPed->GetPedIntelligence()->IsFriendlyWith(*pNearbyPed))
				{
					ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), pNearbyPed->GetTransform().GetPosition());
					if(IsLessThanAll(scDistSq, scMaxDistSq))
					{
						scMaxDistSq = scDistSq;
						pAllyToMoveAwayFrom = pNearbyPed;
					}
				}
			}

			Vec3V vMoveAwayFromPosition = pAllyToMoveAwayFrom ? pAllyToMoveAwayFrom->GetTransform().GetPosition() : Vec3V(V_ZERO);
			static_cast<CTaskVariedAimPose*>(pSubTask)->MoveAwayFromPosition(vMoveAwayFromPosition);
		}

		m_fTimeUntilAllyProximityCheck = ms_Tunables.m_fTimeBetweenAllyProximityChecks * fwRandom::GetRandomNumberInRange(0.9f, 1.1f);
	}
}

void CTaskCombat::OnCallForCover(const CEventCallForCover& rEvent)
{
	//Respond to the cover request only if the ped is in cover.
	if(GetState() == State_InCover)
	{
		//Grab the use cover task.
		CTaskInCover* pTask = static_cast<CTaskInCover *>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
		if(pTask)
		{
			//Handle the cover request.
			pTask->OnCallForCover(rEvent);
		}
	}
	else if(GetState() == State_Fire)
	{
		GetPed()->NewSay("COVER_YOU");
	}
}

void CTaskCombat::OnExplosion(const CEventExplosion& rEvent)
{
	//Ensure the explosion is invalid.
	if(m_uFlags.IsFlagSet(ComF_ExplosionValid))
	{
		return;
	}
	
	//Set the explosion position.
	Vec3V vExplosionPosition = rEvent.GetPosition();
	m_fExplosionX = vExplosionPosition.GetXf();
	m_fExplosionY = vExplosionPosition.GetYf();

	//Set the explosion radius.
	m_fExplosionRadius = rEvent.GetRadius();

	//Set the reaction time.
	float fTimeBeforeReactToExplosion = fwRandom::GetRandomNumberInRange(
		ms_Tunables.m_fMinTimeBeforeReactToExplosion, ms_Tunables.m_fMaxTimeBeforeReactToExplosion);
	m_uTimeToReactToExplosion = fwTimer::GetTimeInMilliseconds() + (u32)(fTimeBeforeReactToExplosion * 1000.0f);
	
	//Note that the explosion is valid.
	m_uFlags.SetFlag(ComF_ExplosionValid);
}

void CTaskCombat::OnGunAimedAt(const CEventGunAimedAt& rEvent)
{
	//Force firing at target.
	ForceFiringAtTarget();
	
	//Check if the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check if the sub-task is a varied aim pose.
		if(pSubTask->GetTaskType() == CTaskTypes::TASK_VARIED_AIM_POSE)
		{
			//Send the event to the sub-task.
			CTaskVariedAimPose* pTask = static_cast<CTaskVariedAimPose *>(pSubTask);
			pTask->OnGunAimedAt(rEvent);
		}
		//Check if the sub-task is cover.
		else if(pSubTask->GetTaskType() == CTaskTypes::TASK_COVER)
		{
			//Send the event to the sub-task.
			CTaskCover* pTask = static_cast<CTaskCover *>(pSubTask);
			pTask->OnGunAimedAt(rEvent);
		}
	}
}

void CTaskCombat::OnGunShot(const CEventGunShot& rEvent)
{
	//Note that a gun shot occurred.
	OnGunShot();

	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Ensure the combat director is valid.
	CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
	if(!pCombatDirector)
	{
		return;
	}
	
	//Notify the combat director of the gun shot.
	pCombatDirector->OnGunShot(rEvent);
}

void CTaskCombat::OnGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent)
{
	//Note that a gun shot occurred.
	OnGunShot();

	//Force firing at target.
	ForceFiringAtTarget();
	
	//Check if the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check if the sub-task is a varied aim pose.
		if(pSubTask->GetTaskType() == CTaskTypes::TASK_VARIED_AIM_POSE)
		{
			//Send the event to the sub-task.
			CTaskVariedAimPose* pTask = static_cast<CTaskVariedAimPose *>(pSubTask);
			pTask->OnGunShotBulletImpact(rEvent);
		}
	}
	
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	//Ensure the combat director is valid.
	CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
	if(!pCombatDirector)
	{
		return;
	}

	//Notify the combat director of the event.
	pCombatDirector->OnGunShotBulletImpact(rEvent);
}

void CTaskCombat::OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent)
{
	//Note that a gun shot occurred.
	OnGunShot();

	//Force firing at target.
	ForceFiringAtTarget();
	
	//Check if the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check if the sub-task is a varied aim pose.
		if(pSubTask->GetTaskType() == CTaskTypes::TASK_VARIED_AIM_POSE)
		{
			//Send the event to the sub-task.
			CTaskVariedAimPose* pTask = static_cast<CTaskVariedAimPose *>(pSubTask);
			pTask->OnGunShotWhizzedBy(rEvent);
		}
	}

	// Let the gun task know about the event (if we have one)
	CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
	if (pGunTask)
	{
		pGunTask->OnGunShotWhizzedBy(rEvent);
	}


	CTaskCombatAdditionalTask* pAdditionalCombatTask = static_cast<CTaskCombatAdditionalTask *>(FindSubTaskOfType(CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK));
	if(pAdditionalCombatTask)
	{
		pAdditionalCombatTask->OnGunShotWhizzedBy(rEvent);
	}
	
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	//Ensure the combat director is valid.
	CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
	if(!pCombatDirector)
	{
		return;
	}

	//Notify the combat director of the event.
	pCombatDirector->OnGunShotWhizzedBy(rEvent);
}

void CTaskCombat::OnPotentialBlast(const CEventPotentialBlast& rEvent)
{
	//Ensure the blast is not smoke.
	if(rEvent.GetIsSmoke())
	{
		return;
	}

	//Ignore the potential blast if we don't know the time of explosion.
	u32 uTimeOfExplosion = rEvent.GetTimeOfExplosion();
	if(uTimeOfExplosion == 0)
	{
		return;
	}

	//Ensure the potential blast is either invalid, or will come after the new one.
	if(m_PotentialBlast.IsValid() && (m_PotentialBlast.m_uTimeOfExplosion <= uTimeOfExplosion))
	{
		return;
	}

	//Set the potential blast.
	m_PotentialBlast.Set(rEvent.GetTarget(), rEvent.GetRadius(), uTimeOfExplosion);
}

void CTaskCombat::OnProvidingCover(const CEventProvidingCover& rEvent)
{
	//Check the state.
	if(GetState() == State_DragInjuredToSafety)
	{
		//Grab the drag task.
		CTaskDraggingToSafety* pTask = static_cast<CTaskDraggingToSafety *>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DRAGGING_TO_SAFETY));
		if(pTask)
		{
			//Notify the task of the event.
			pTask->OnProvidingCover(rEvent);
		}
	}
}

void CTaskCombat::OnShoutBlockingLos(const CEventShoutBlockingLos& rEvent)
{
	//Check if the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check if the sub-task is a varied aim pose.
		if(pSubTask->GetTaskType() == CTaskTypes::TASK_VARIED_AIM_POSE)
		{
			//Send the event to the sub-task.
			CTaskVariedAimPose* pTask = static_cast<CTaskVariedAimPose *>(pSubTask);
			pTask->OnShoutBlockingLos(rEvent);
		}
	}
}

bool CTaskCombat::GetIsPlayingAmbientClip() const
{
	if(GetClipHelper())
	{
		return true;
	}

	return false;
}

// Look at the event and decide who a recipient should fight.
const CPed* CTaskCombat::GetCombatTargetFromEvent(const CEvent& rEvent)
{
	switch(rEvent.GetEventType())
	{
		case EVENT_SHOCKING_CAR_CRASH:
		case EVENT_SHOCKING_MAD_DRIVER:
		case EVENT_SHOCKING_MAD_DRIVER_EXTREME:
		case EVENT_SHOCKING_SEEN_PED_KILLED:
			return rEvent.GetSourcePed();
		default:
			return rEvent.GetTargetPed();
	}
}

bool CTaskCombat::CheckShouldMoveBackInsideAttackWindow( CPed * pPed )
{
	if( !pPed->GetIsInVehicle() && pPed->HasHurtStarted() )
	{
		static dev_float MinDistanceSqr = 2.0f * 2.0f;
		ScalarV scToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
		ScalarV scMinDistSq = LoadScalar32IntoScalarV(MinDistanceSqr);
		if(IsLessThanAll(scToTargetSq, scMinDistSq))
		{
			return true;
		}
	}

	return false;
}

bool CTaskCombat::CheckIfAnotherPedIsAtMyCoverPoint()
{
	const CPed& rPed = *GetPed();
	if (rPed.GetCoverPoint())
	{
		Vector3 vMyCoverPosition;
		if (rPed.GetCoverPoint()->GetCoverPointPosition(vMyCoverPosition))
		{
			vMyCoverPosition.z += 1.0f;

			const CEntityScannerIterator it = rPed.GetPedIntelligence()->GetNearbyPeds();
			for (const CEntity* pEntity = it.GetFirst(); pEntity; pEntity = it.GetNext())
			{
				if (pEntity->GetIsTypePed() && pEntity != &rPed)
				{
					const CPed& rOtherPed = *static_cast<const CPed*>(pEntity);
					const Vector3 vOtherPedPosition = VEC3V_TO_VECTOR3(rOtherPed.GetTransform().GetPosition());
					const Vector3 vDiffBetweenMyCoverAndOtherPed = vMyCoverPosition - vOtherPedPosition;
					TUNE_GROUP_FLOAT(COVER_TUNE, MIN_DIST_BETWEEN_OTHER_PED_AND_MY_COVER_TO_INVALIDATE_COVER, 1.0f, 0.0f, 2.0f, 0.01f);
					if (vDiffBetweenMyCoverAndOtherPed.Mag2() < rage::square(MIN_DIST_BETWEEN_OTHER_PED_AND_MY_COVER_TO_INVALIDATE_COVER))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

// Test if we can get frustrated
bool CTaskCombat::CanBeFrustrated( CPed* pPed )
{
	//Ensure the ped can advance.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() != CCombatData::CM_WillAdvance)
	{
		return false;
	}

	if(CTaskCombat::ms_pFrustratedPed != pPed)
	{
		// Early exit if we have set somebody as frustrated (nobody should be running or checking probes)
		if(CTaskCombat::ms_pFrustratedPed)
		{
			return false;
		}

		// If our timer hasn't finished don't let anyone submit a probe
		if(!ms_allowFrustrationTimerGlobal.IsFinished() && !m_asyncProbeHelper.IsProbeActive())
		{
			return false;
		}
	}

	// Don't need to check some things if we are already in the frustrated state
	bool bIsDoingFrustratedAdvance		=  (GetState() == State_AdvanceFrustrated);
	bool bWillBeDoingFrustratedAdvance	= ((GetState() == State_WaitingForCoverExit) && (m_iDesiredState == State_AdvanceFrustrated));
	if(!bIsDoingFrustratedAdvance && !bWillBeDoingFrustratedAdvance)
	{
		// Don't exit any state too quickly
		if(GetTimeInState() <= AVERAGE_STATE_SEARCH_TIME)
		{
			return false;
		}

		// Only cops are allowed to get frustrated
		if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseFrustratedAdvance))
		{
			return false;
		}
	}
		
	// Make sure our target is in cover
	if(!m_pPrimaryTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
	{
		return false;
	}

	return true;
}

void CTaskCombat::UpdateAsyncProbe( CPed* pPed )
{
	// Check our results if the probe is already active
	if(m_asyncProbeHelper.IsProbeActive())
	{
		ProbeStatus probeStatus;
		if(m_asyncProbeHelper.GetProbeResults(probeStatus))
		{
			// Tell the task it can be frustrated and reset our global timer that allows peds to be frustrated
			if(probeStatus == PS_Blocked)
			{
				CTaskCombat::ms_pFrustratedPed = pPed;
				ms_allowFrustrationTimerGlobal.Reset();
			}

			m_asyncProbeHelper.ResetProbe();
		}
	}
	else if(fwTimer::GetTimeInMilliseconds() - ms_iTimeLastAsyncProbeSubmitted > 2000 && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < .1f)
	{
		Vector3 vTargetPos;
		m_pPrimaryTarget->GetLockOnTargetAimAtPos(vTargetPos);
		WorldProbe::CShapeTestProbeDesc probeData;
		probeData.SetStartAndEnd(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),vTargetPos);
		probeData.SetContext(WorldProbe::ELosCombatAI);
		probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
		probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU); 
		m_asyncProbeHelper.ResetProbe();
		m_asyncProbeHelper.StartTestLineOfSight(probeData);

		ms_iTimeLastAsyncProbeSubmitted = fwTimer::GetTimeInMilliseconds();
	}
}

void CTaskCombat::ActivateTimeslicing()
{
	TUNE_GROUP_BOOL(COMBAT, ACTIVATE_TIMESLICING, true);
	if(ACTIVATE_TIMESLICING)
	{
		GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}
}

bool CTaskCombat::CanArrestTarget() const
{
	//Ensure we are not in MP.
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Ensure the target is a player.
	if(!m_pPrimaryTarget->IsPlayer())
	{
		return false;
	}

	// Make sure the target is not on fire
	if(m_pPrimaryTarget->m_nFlags.bIsOnFire)
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the flag is set.
	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanBust))
	{
		return false;
	}

	// Check if police should be backing off
	CWanted* pPlayerWanted = m_pPrimaryTarget->GetPlayerWanted();
	if(pPlayerWanted && pPlayerWanted->PoliceBackOff())
	{
		return false;
	}

	// B* 1995719 - not if the player is an animal
	if (m_pPrimaryTarget->GetPedType() == PEDTYPE_ANIMAL)
	{
		return false;
	}

	// Not for the sasquatch.
	const CPedModelInfo* pModelInfo = m_pPrimaryTarget->GetPedModelInfo();
	if (pModelInfo && pModelInfo->GetModelNameHash() == MI_PED_ORLEANS.GetName().GetHash())
	{
		return false;
	}

	return true;
}

bool CTaskCombat::ShouldArrestTarget() const
{
	// We don't want to try and arrest too frequently
	if(!m_jackingTimer.IsFinished())
	{
		return false;
	}
	
	// Check to see if an arrest was not attempted too recently
	CPlayerInfo* pPlayerInfo = m_pPrimaryTarget->GetPlayerInfo();
	if(pPlayerInfo && pPlayerInfo->GetLastTimeArrestAttempted() + ms_Tunables.m_TimeBetweenPlayerArrestAttempts > fwTimer::GetTimeInMilliseconds())
	{
		return false;
	}

	// Some simple checks we want to do elsewhere so we put them in a separate helper
	if(!CanArrestTarget())
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is on foot.
	if(!pPed->GetIsOnFoot())
	{
		return false;
	}

	//Make sure we are the closest ped capable of busting the target
	if(pPed != CNearestPedInCombatHelper::Find(*m_pPrimaryTarget, CNearestPedInCombatHelper::OF_MustBeLawEnforcement|CNearestPedInCombatHelper::OF_MustBeOnFoot))
	{
		return false;
	}

	//Ensure no one else is attempting an arrest.
	if(pPlayerInfo && pPlayerInfo->GetNumAiAttempingArrestOnPlayer() > 0)
	{
		return false;
	}

	//Check if the target is in a vehicle.
	if(m_pPrimaryTarget->GetIsInVehicle())
	{
		if(!IsVehicleSafeForJackOrArrest(m_pPrimaryTarget->GetVehiclePedInside()))
		{
			return false;
		}

		u32 uTimeSinceTargetVehicleMoved;
		if(GetTimeSinceTargetVehicleMoved(uTimeSinceTargetVehicleMoved) && uTimeSinceTargetVehicleMoved < ms_Tunables.m_WaitTimeForJackingSlowedVehicle)
		{
			return false;
		}
		
		// Make sure we pass the arrest in vehicle tests
		if(!CTaskArrestPed::CanArrestPedInVehicle(pPed, m_pPrimaryTarget, true, false, false))
		{
			return false;
		}

		return true;
	}

	//Check if we are holding fire due to a lack of hostility.
	if(m_nReasonToHoldFire == RTHF_LackOfHostility)
	{
		return true;
	}

	return false;
}

bool CTaskCombat::CanJackTarget() const
{
	// Certain checks only happen if we aren't coming from a melee state
	if( GetState() != State_MeleeAttack )
	{
		if( !m_jackingTimer.IsFinished() )
		{
			return false;
		}
	}
	else
	{
		u32 uMinTimeBetweenJackAttemptsFromMelee = m_pPrimaryTarget->IsNetworkClone() ?  ms_Tunables.m_MinTimeBetweenMeleeJackAttemptsOnNetworkClone : ms_Tunables.m_MinTimeBetweenMeleeJackAttempts;
		if((fwTimer::GetTimeInMilliseconds() - m_uTimeOfLastJackAttempt) < uMinTimeBetweenJackAttemptsFromMelee)
		{
			return false;
		}

		const CTask* pSubTask = GetSubTask();
		if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE )
		{
			const CTaskMelee* pMeleeTask = static_cast<const CTaskMelee*>(pSubTask);
			if( pMeleeTask->GetQueueType() <= CTaskMelee::QT_InactiveObserver )
			{
				return false;
			}
		}
	}

	// target must not be currently entering or exiting a vehicle
	if( m_pPrimaryTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) ||
		m_pPrimaryTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) )
	{
		return false;
	}

	// Make sure the target is not on fire
	if(m_pPrimaryTarget->m_nFlags.bIsOnFire)
	{
		return false;
	}

	// Target much be inside a vehicle that isn't a train
	CVehicle* pVehicle = m_pPrimaryTarget->GetVehiclePedInside();
	if(!IsVehicleSafeForJackOrArrest(pVehicle))
	{
		return false;
	}
	
	if (pVehicle)
	{
		const s32 iTargetsSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(m_pPrimaryTarget);
		if (pVehicle->IsSeatIndexValid(iTargetsSeatIndex))
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromSeatIndex(pVehicle, iTargetsSeatIndex);
			if (pSeatAnimInfo && pSeatAnimInfo->GetCannotBeJacked())
			{
				return false;
			}
		}
	}

	// If this is a player ped then we don't want to attempt a jack/bust if they are already being arrested
	CPlayerInfo* pPlayerInfo = m_pPrimaryTarget->GetPlayerInfo();
	if(pPlayerInfo && pPlayerInfo->GetNumAiAttempingArrestOnPlayer() > 0)
	{
		return false;
	}

	const CPed* pPed = GetPed();
	

	// If a ped is marked as can bust then they shouldn't be jacking a player target
	if( pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanBust) &&
		m_pPrimaryTarget->IsAPlayerPed() )
	{
		return false;
	}

	if( m_pPrimaryTarget->GetPedConfigFlag(CPED_CONFIG_FLAG_DontDragMeOutCar) )
	{
		return false;
	}

	CWanted* pPlayerWanted = m_pPrimaryTarget->IsAPlayerPed() ? m_pPrimaryTarget->GetPlayerWanted() : NULL;
	if(pPlayerWanted && pPlayerWanted->m_EverybodyBackOff)
	{
		return false;
	}

	// get our equipped weapon info
	Assert( pPed->GetWeaponManager() );
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();

	//Ensure the vehicle is moving slow enough to start jacking.
	if( pWeaponInfo && pWeaponInfo->GetIsArmed())
	{
		u32 uTimeSinceTargetVehicleMoved;
		if(GetTimeSinceTargetVehicleMoved(uTimeSinceTargetVehicleMoved) && uTimeSinceTargetVehicleMoved < ms_Tunables.m_WaitTimeForJackingSlowedVehicle)
		{
			return false;
		}
	}

	// target must be close enough
	const float fDistanceToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition()).Getf();
	if( fDistanceToTargetSq > rage::square(CAR_JACK_DISTANCE_START) )
	{
		return false;
	}

	// Scan through nearby peds and make sure none of them are already attempting to bust or jack the target ped
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed * pNearbyPed = static_cast<const CPed*>(pEnt);
		if( pNearbyPed && pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
		{
			// if this ped is in combat we should check if they are in an approach vehicle state that would imply they are going to pull the target from the vehicle
			s32 iState = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_COMBAT);
			if( (iState == State_PullTargetFromVehicle || iState == State_GoToEntryPoint || iState == State_WaitingForEntryPointToBeClear ) &&
				pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT) == m_pPrimaryTarget.Get() )
			{
				return false;
			}
		}
	}

	return true;
}

bool CTaskCombat::CanChaseInVehicle()
{
	// No use in doing all these checks if we are already trying to enter a vehicle
	if(GetState() == State_EnterVehicle)
	{
		return false;
	}

	// We should clear this any time we are checking if we are going to do an early entry. We'll set it later in here if we choose an early entry
	ClearEarlyVehicleEntry();

	// Make sure we have a target who is in a vehicle or on a mount
	const CVehicle* pVehicleTargetInside = m_pPrimaryTarget ? m_pPrimaryTarget->GetVehiclePedInside() : NULL;
	const CPed* pMountTargetOn = m_pPrimaryTarget ? m_pPrimaryTarget->GetMountPedOn() : NULL;
	if(!pVehicleTargetInside && !pMountTargetOn)
	{
		return false;
	}

	const CPed* pPed = GetPed();
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();

	// Make sure we do not have a defensive area
	if(pPedIntelligence->GetDefensiveArea()->IsActive())
	{
		return false;
	}

	// Must be allowed to advance
	if(pPedIntelligence->GetCombatBehaviour().GetCombatMovement() != CCombatData::CM_WillAdvance)
	{
		return false;
	}

	// Must be allowed to use vehicles
	if(!pPedIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles))
	{
		return false;
	}

	m_pVehicleToUseForChase = NULL;

	// Check if we should be allowed to use our vehicle (based on it existing and being drivable)
	CVehicle* pMyVehicle = pPed->GetMyVehicle();
	bool bIsMyVehicleBike = pMyVehicle && pMyVehicle->InheritsFromBike();
	if( pMyVehicle && !pMyVehicle->IsOnFire() && (bIsMyVehicleBike || !pMyVehicle->IsOnItsSide()) &&
		!pMyVehicle->IsUpsideDown() && pMyVehicle->m_nVehicleFlags.bIsThisADriveableCar && pMyVehicle->IsInEnterableState(bIsMyVehicleBike) )
	{
		// Make sure we are close enough to our vehicle
		ScalarV scDistToMyVehicleSq = DistSquared(pPed->GetTransform().GetPosition(), pMyVehicle->GetTransform().GetPosition());
		ScalarV scMaxDistToMyVehicleSq = ScalarVFromF32(rage::square(ms_Tunables.m_MaxDistanceToMyVehicleToChase));
		if(IsLessThanAll(scDistToMyVehicleSq, scMaxDistToMyVehicleSq))
		{
			m_pVehicleToUseForChase = pMyVehicle;
		}
	}

	// If our vehicle is good enough to use
	if(m_pVehicleToUseForChase)
	{
		if(pVehicleTargetInside && CTargetInWaterHelper::IsInWater(*pVehicleTargetInside))
		{
			return false;
		}

		// Check who our driver is
		CPed* pDriver = m_pVehicleToUseForChase->GetDriver();
		if(!pDriver)
		{
			pDriver = m_pVehicleToUseForChase->GetSeatManager()->GetLastPedInSeat(m_pVehicleToUseForChase->GetDriverSeat());
		}

		// if we are the driver then we will make the decision on whether or not to enter the vehicle
		if(pPed == pDriver)
		{
			//Check if the speed has exceeded the threshold.
			float fSpeedSq = pVehicleTargetInside ? pVehicleTargetInside->GetVelocity().Mag2() : pMountTargetOn->GetVelocity().Mag2();
			if(fSpeedSq < GET_IN_VEHICLE_AT_TARGET_SPEED_SQ)
			{
				// Some attacking peds should enter their vehicle if the target is simply entering the vehicle
				if( ms_uNumDriversEnteringVehiclesEarly >= ms_Tunables.m_NumEarlyVehicleEntryDriversAllowed ||
					!m_pPrimaryTarget->GetIsEnteringVehicle() )
				{
					return false;
				}

				// Make sure we are far enough away from the target
				ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
				ScalarV scMinDistSq = ScalarVFromF32(rage::square(ms_Tunables.m_MinDistanceToEnterVehicleIfTargetEntersVehicle));

				if(IsLessThanAll(scDistSq, scMinDistSq))
				{
					return false;
				}
			}

			return true;
		}
		else
		{
			// Check if we have a driver
			if(pDriver)
			{
				// If our driver is dead or we are not friendly with them then we shouldn't try to enter the vehicle
				if(pDriver->IsInjured() || !pPedIntelligence->IsFriendlyWith(*pDriver))
				{
					return false;
				}

				// If the driver isn't already inside our vehicle
				if(pDriver->GetVehiclePedInside() != m_pVehicleToUseForChase)
				{
					// If the driver is not entering our vehicle
					CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
					if(!pEnterVehicleTask || pEnterVehicleTask->GetVehicle() != m_pVehicleToUseForChase)
					{
						return false;
					}
				}
				else
				{
					CTaskVehiclePersuit* pPursueTask = static_cast<CTaskVehiclePersuit*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_PERSUIT));
					if( (pPursueTask && pPursueTask->GetState() == CTaskVehiclePersuit::State_waitForVehicleExit) ||
						pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE) )
					{
						return false;
					}
				}
			}

			return true;
		}
	}
	else
	{
		// At this point we need to check if we should commandeer a vehicle, so quit out if we are not allowed to
		if(!pPedIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanCommandeerVehicles))
		{
			return false;
		}
		
		// We shouldn't commandeer vehicles if the target's vehicle is something like a heli/plane
		if(pVehicleTargetInside && (pVehicleTargetInside->InheritsFromHeli() || pVehicleTargetInside->InheritsFromPlane()))
		{
			return false;
		}

		// For now we don't do early entry when having to commandeer a vehicle
		float fSpeedSq = pVehicleTargetInside ? pVehicleTargetInside->GetVelocity().Mag2() : pMountTargetOn->GetVelocity().Mag2();
		if(fSpeedSq < GET_IN_VEHICLE_AT_TARGET_SPEED_SQ)
		{
			return false;
		}

		// Need the targeting system so we can check if the driver is one of our possible targets
		CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( false );
		ScalarV scMaxDistSq = ScalarVFromF32(rage::square(ms_Tunables.m_MaxDistanceToVehicleForCommandeer));

		CEntityScannerIterator nearbyVehicles = pPedIntelligence->GetNearbyVehicles();
		for( CEntity* pEnt = nearbyVehicles.GetFirst(); pEnt; pEnt = nearbyVehicles.GetNext())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
			if(!pVehicle || pVehicle->PopTypeIsMission() || pVehicle == pMyVehicle)
			{
				continue;
			}

			bool bVehicleTypeIsValid = pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || (pVehicle->InheritsFromBike() && !pPed->ShouldBehaveLikeLaw());
			if(!bVehicleTypeIsValid)
			{
				continue;
			}

			// Make sure we are close enough to the vehicle
			ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), pVehicle->GetTransform().GetPosition());
			if(IsGreaterThanAll(scDistSq, scMaxDistSq))
			{
				break;
			}

			// Make sure the vehicle isn't going too fast
			if(!IsVehicleMovingSlowerThanSpeed(*pVehicle, ms_Tunables.m_MaxSpeedToStartJackingVehicle))
			{
				continue;
			}

			CPed* pDriver = pVehicle->GetDriver();
			if(pDriver)
			{
				// If the driver is a player, or we are friendly with the driver or the driver is one of our targets then we shouldn't choose this vehicle
				if( pDriver->IsAPlayerPed() ||
					pPedIntelligence->IsFriendlyWith(*pDriver) ||
					(pPedTargetting && pPedTargetting->FindTarget(pDriver)) )
				{
					continue;
				}
			}

			m_pVehicleToUseForChase = pVehicle;

			// Force this vehicle to be unlocked for law peds so it doesn't look like they are breaking into it
			if(pPed->IsLawEnforcementPed())
			{
				m_pVehicleToUseForChase->SetCarDoorLocks(CARLOCK_UNLOCKED);
			}

			return true;
		}
	}

	return false;
}

bool CTaskCombat::CanClearPrimaryDefensiveArea() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the flag is set.
	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_ClearPrimaryDefensiveAreaWhenReached))
	{
		return false;
	}

	//Ensure the defensive area is active.
	if(!pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive())
	{
		return false;
	}

	//Ensure we are not moving within our defensive area.
	//Clearing our defensive area at this point causes the task to quit, which is undesirable.
	if(GetState() == State_MoveWithinDefensiveArea)
	{
		return false;
	}

	//Ensure we are physically in the defensive area.
	//We don't want to check if we are generally in our defensive area, because
	//this will check our current cover point as well, which is undesirable.
	if(!IsPhysicallyInDefensiveArea())
	{
		return false;
	}
	
	return true;
}

bool CTaskCombat::CanMoveToTacticalPoint() const
{
	//Ensure we are firing.
	if(GetState() != State_Fire)
	{
		return false;
	}

	//Ensure we have been firing for a bit of time.
	static dev_float s_fMinTime = 2.0f;
	if(GetTimeInState() < s_fMinTime)
	{
		return false;
	}

	//Iterate over the nearby peds.
	CEntityScannerIterator entityList = GetPed()->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Ensure the ped is alive.
		const CPed* pOtherPed = static_cast<CPed *>(pEntity);
		if(pOtherPed->IsInjured())
		{
			continue;
		}

		//Ensure the ped is friendly.
		if(!GetPed()->GetPedIntelligence()->IsFriendlyWithByAnyMeans(*pOtherPed))
		{
			continue;
		}

		//Ensure the ped is close.
		ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pOtherPed->GetTransform().GetPosition());
		static dev_float s_fMaxDistance = 1.25f;
		ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			break;
		}

		return true;
	}

	return false;
}

bool CTaskCombat::CanMoveWithinDefensiveArea() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is not stationary.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_Stationary)
	{
		return false;
	}
	
	return true;
}

bool CTaskCombat::CanOpenCombat() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the flag is set.
	if( !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_ClearAreaSetDefensiveIfDefensiveAreaReached) &&
		!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_ClearAreaSetAdvanceIfDefensiveAreaReached) )
	{
		return false;
	}

	//Ensure the defensive area is active.
	if(!pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive())
	{
		return false;
	}

	//Ensure we are not moving within our defensive area.
	//Clearing our defensive area at this point causes the task to quit, which is undesirable.
	if(GetState() == State_MoveWithinDefensiveArea)
	{
		return false;
	}

	//Ensure we are physically in the defensive area.
	//We don't want to check if we are generally in our defensive area, because
	//this will check our current cover point as well, which is undesirable.
	if(!IsPhysicallyInDefensiveArea())
	{
		return false;
	}

	return true;
}

bool CTaskCombat::CanPlayAmbientAnim() const
{
	//Ensure we will not need to react to an imminent explosion soon.
	if(WillNeedToReactToImminentExplosionSoon())
	{
		return false;
	}

	return true;
}

bool CTaskCombat::CanReactToBuddyShot() const
{
	CPed* pBuddyShot = NULL;
	return CanReactToBuddyShot(&pBuddyShot);
}

bool CTaskCombat::CanReactToBuddyShot(CPed** ppBuddyShot) const
{
	//Clear the buddy shot.
	*ppBuddyShot = NULL;

	//Ensure buddy shot is enabled.
	if(!ms_Tunables.m_BuddyShot.m_Enabled)
	{
		return false;
	}

	//Check if scripts have disabled this behaviour on this ped
	if( GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableReactToBuddyShot) )
	{
		return false;
	}

	//Ensure the ped is not hurt.
	if(GetPed()->HasHurtStarted())
	{
		return false;
	}

	//Keep track of the ped to react to.
	CPed* pPedToReactTo = NULL;

	//Just check a few peds.
	static const int s_iMaxPedsToCheck = 4;
	int iNumPedsChecked = 0;

	//Iterate over the nearby peds.
	CEntityScannerIterator entityList = GetPed()->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
	{
		//Check if the ped has been recently shot.
		CPed* pNearbyPed = static_cast<CPed*>(pEntity);
		if( (!pNearbyPed->IsAPlayerPed() || pNearbyPed->IsDead()) &&
			GetPed()->GetPedIntelligence()->IsFriendlyWith(*pNearbyPed) &&
			(pNearbyPed->GetPedIntelligence()->GetLastTimeShot() + (u32)(ms_Tunables.m_BuddyShot.m_MaxTimeSinceShot * 1000.0f) >= fwTimer::GetTimeInMilliseconds()) )
		{
			//Set the ped to react to.
			pPedToReactTo = pNearbyPed;
			break;
		}

		++iNumPedsChecked;
		if(iNumPedsChecked >= s_iMaxPedsToCheck)
		{
			return false;
		}
	}

	//Ensure the ped to react to is valid.
	if(!pPedToReactTo)
	{
		return false;
	}

	//Ensure the reaction time is enforced.
	float fTimeBeforeReact = GetPed()->GetRandomNumberInRangeFromSeed(ms_Tunables.m_BuddyShot.m_MinTimeBeforeReact,
		ms_Tunables.m_BuddyShot.m_MaxTimeBeforeReact);
	if(pPedToReactTo->GetPedIntelligence()->GetLastTimeShot() + (u32)(fTimeBeforeReact * 1000.0f) > fwTimer::GetTimeInMilliseconds())
	{
		return false;
	}

	//Ensure the ped is within the distance.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pPedToReactTo->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(ms_Tunables.m_BuddyShot.m_MaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	//Get the aim target.
	CAITarget aimTarget;
	GetAimTarget(aimTarget);

	//Ensure the task is valid.
	if(!CTaskReactToBuddyShot::IsValid(*GetPed(), CAITarget(pPedToReactTo), aimTarget))
	{
		return false;
	}

	//Ensure the ped has not been reacted to.
	if(CTaskReactToBuddyShot::HasBeenReactedTo(*GetPed(), *pPedToReactTo))
	{
		return false;
	}

	//Set the buddy shot.
	*ppBuddyShot = pPedToReactTo;

	return true;
}

bool CTaskCombat::CanReactToExplosion() const
{
	//Ensure the explosion is valid.
	if(!m_uFlags.IsFlagSet(ComF_ExplosionValid))
	{
		return false;
	}
	
	//Ensure the time to react has been exceeded.
	if(fwTimer::GetTimeInMilliseconds() < m_uTimeToReactToExplosion)
	{
		return false;
	}

	//Get the aim target.
	CAITarget aimTarget;
	GetAimTarget(aimTarget);

	//Ensure we can react to the explosion.
	float fExplosionZ = GetPed()->GetTransform().GetPosition().GetZf();
	if(!CTaskReactToExplosion::IsValid(*GetPed(), Vec3V(m_fExplosionX, m_fExplosionY, fExplosionZ), NULL, m_fExplosionRadius,
		CTaskReactToExplosion::CF_DisableThreatResponse, aimTarget))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::CanReactToImminentExplosion() const
{
	// animation needs to be streamed in on MP but the delay might make things look odd....
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Ensure the potential blast is valid.
	if(!m_PotentialBlast.IsValid())
	{
		return false;
	}

	//Calculate the time before the react.
	static const u32 s_uMinTimeBeforeExplosionToReact = 800;
	static const u32 s_uMaxTimeBeforeExplosionToReact = 1600;
	u32 uTimeBeforeExplosionToReact = (u32)GetPed()->GetRandomNumberInRangeFromSeed(
		(float)s_uMinTimeBeforeExplosionToReact, (float)s_uMaxTimeBeforeExplosionToReact);

	//Increase the time before the react if we are in cover, to give us time to exit.
	bool bIsInCover = (GetState() == State_InCover) || (GetState() == State_WaitingForCoverExit);
	if(bIsInCover)
	{
		static dev_u32 s_uExtraTime = 750;
		uTimeBeforeExplosionToReact += s_uExtraTime;
	}

	//Ensure the time is valid.
	if((fwTimer::GetTimeInMilliseconds() + uTimeBeforeExplosionToReact) < m_PotentialBlast.m_uTimeOfExplosion)
	{
		return false;
	}

	//Get the explosion target.
	CAITarget explosionTarget;
	m_PotentialBlast.GetTarget(*GetPed(), explosionTarget);

	//Get the aim target.
	CAITarget aimTarget;
	GetAimTarget(aimTarget);

	//Ensure the task is valid.
	bool bIgnoreCover = bIsInCover;
	if(!CTaskReactToImminentExplosion::IsValid(*GetPed(), explosionTarget, m_PotentialBlast.m_fRadius, aimTarget, bIgnoreCover))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::CanUpdatePrimaryTarget() const
{
	//Ensure we are not playing an aim pose transition.
	if(IsPlayingAimPoseTransition())
	{
		return false;
	}

	//Ensure we have not disabled target changes during vehicle pursuit.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableTargetChangesDuringVehiclePursuit) && (GetState() == State_PersueInVehicle))
	{
		return false;
	}

	return true;
}

void CTaskCombat::ChangeTarget(const CPed* pTarget)
{
	//Ensure the target is changing.
	if(pTarget == m_pPrimaryTarget)
	{
		return;
	}

	// Check if we already have a primary target, and a non-null candidate, and are in the charging state
	if( m_pPrimaryTarget && pTarget && GetState() == State_ChargeTarget )
	{
		// Get handle to ped
		const CPed* pPed = GetPed();

		// Compute distance from ped to primary target
		const ScalarV distToPrimarySq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());

		// Compute distance from ped to proposed new target
		const ScalarV distToCandidateSq = DistSquared(pPed->GetTransform().GetPosition(), pTarget->GetTransform().GetPosition());

		// If the primary target is closer, do not switch
		if( IsLessThanAll(distToPrimarySq, distToCandidateSq) )
		{
			return;
		}
	}
	
	//Clear the firing at target state.
	ClearFiringAtTarget();
	
	//Assign the target.
	m_pPrimaryTarget = pTarget;
#if __BANK
	LogTargetInformation();
#endif
	//Note that the target changed.
	m_uFlags.SetFlag(ComF_TargetChangedThisFrame);

	//Release the tactical analysis.
	ReleaseTacticalAnalysis();

	//Request a tactical analysis.
	RequestTacticalAnalysis();

	// Reset the time the target's vehicle last moved (since we don't technically know this about the new target)
	m_uTimeTargetVehicleLastMoved = 0;
}

bool CTaskCombat::CheckForGiveUpOnGoToEntryPoint() const
{
	//Ensure we are random.
	if(!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure we are not law-enforcement.
	if(GetPed()->IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure we are not a security guard, and the target is wanted.
	if(GetPed()->IsSecurityPed() && IsTargetWanted())
	{
		return false;
	}

	//Ensure we don't have a gun.
	if(DoesPedHaveGun(*GetPed()))
	{
		return false;
	}

	//Ensure the distance has exceeded the threshold.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	static dev_float s_fMinDistance = 30.0f;
	ScalarV scMinDistSq = ScalarVFromF32(square(s_fMinDistance));
	if(IsGreaterThanAll(scDistSq, scMinDistSq))
	{
		return true;
	}
	
	//Check if the time has exceeded the threshold.
	if(m_fTimeJacking > 30.0f)
	{
		return true;
	}

	return false;
}

bool CTaskCombat::CheckForGiveUpOnWaitingForEntryPointToBeClear() const
{
	//Ensure we are random.
	if(!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure we are not law-enforcement.
	if(GetPed()->IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure we are not a security guard, and the target is wanted.
	if(GetPed()->IsSecurityPed() && IsTargetWanted())
	{
		return false;
	}

	//Ensure we don't have a gun.
	if(DoesPedHaveGun(*GetPed()))
	{
		return false;
	}

	//Ensure the time has exceeded the threshold.
	if(GetTimeInState() < 5.0f)
	{
		return false;
	}

	return true;
}

int CTaskCombat::CountPedsWithClearLineOfSightToTarget() const
{
	return CCombatManager::GetCombatTaskManager()->CountPedsWithClearLosToTarget(*m_pPrimaryTarget);
}

fwMvClipSetId CTaskCombat::ChooseClipSetForFranticRuns(bool& bAreDefaultRunsFrantic) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Set whether the default runs are frantic.
	bAreDefaultRunsFrantic = (pPed->IsGangPed());

	//Check if the ped is male & law-enforcement.
	if(pPed->IsMale() && pPed->IsLawEnforcementPed())
	{
		static fwMvClipSetId s_ClipSetId("move_m@law@frantic@runs",0xCF15847E);
		return s_ClipSetId;
	}

	return CLIP_SET_ID_INVALID;
}

fwMvClipSetId CTaskCombat::ChooseClipSetForGesture(const CPed& rPed, GestureType nGestureType) const
{
	//Ensure the gesture type is valid.
	if(nGestureType == GT_None)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return CLIP_SET_ID_INVALID;
	}
	
	//Ensure the equipped weapon info is valid.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	fwMvClipSetId overrideGestureClipSet(CLIP_SET_ID_INVALID);
	switch(nGestureType)
	{
	case GT_Beckon:
		{
			overrideGestureClipSet = pWeaponInfo->GetGestureBeckonOverride(rPed);
			break;
		}
	case GT_OverThere:
		{
			overrideGestureClipSet = pWeaponInfo->GetGestureOverThereOverride(rPed);
			break;
		}
	case GT_Halt:
		{
			overrideGestureClipSet = pWeaponInfo->GetGestureHaltOverride(rPed);
			break;
		}
	default:
		{
			break;
		}
	}

	if(overrideGestureClipSet != CLIP_SET_ID_INVALID)
	{
		return overrideGestureClipSet;
	}

	//Check if the ped is a gang ped.
	if(rPed.IsGangPed())
	{
		//Check if the weapon is a pistol.
		if(pWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL)
		{
			//Check the gesture type.
			switch(nGestureType)
			{
				case GT_Beckon:
				{
					return ms_BeckonPistolGangClipSetId;
				}
				case GT_OverThere:
				{
					return ms_OverTherePistolGangClipSetId;
				}
				case GT_Halt:
				{
					return ms_HaltPistolGangClipSetId;
				}
				default:
				{
					break;
				}
			}
		}
		//Check if the weapon is an smg.
		else if(pWeaponInfo->GetGroup() == WEAPONGROUP_SMG)
		{
			//Check the gesture type.
			switch(nGestureType)
			{
				case GT_Beckon:
				{
					return ms_BeckonSmgGangClipSetId;
				}
				case GT_OverThere:
				{
					return ms_OverThereSmgGangClipSetId;
				}
				case GT_Halt:
				{
					return ms_HaltSmgGangClipSetId;
				}
				default:
				{
					break;
				}
			}
		}
	}
	
	//Check if the weapon is a pistol.
	if(pWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL)
	{
		//Check the gesture type.
		switch(nGestureType)
		{
			case GT_Beckon:
			{
				return ms_BeckonPistolClipSetId;
			}
			case GT_OverThere:
			{
				return ms_OverTherePistolClipSetId;
			}
			case GT_Halt:
			{
				return ms_HaltPistolClipSetId;
			}
			default:
			{
				break;
			}
		}
	}
	//Check if the weapon is a rifle.
	else if(pWeaponInfo->GetGroup() == WEAPONGROUP_RIFLE)
	{
		//Check the gesture type.
		switch(nGestureType)
		{
			case GT_Beckon:
			{
				return ms_BeckonRifleClipSetId;
			}
			case GT_OverThere:
			{
				return ms_OverThereRifleClipSetId;
			}
			case GT_Halt:
			{
				return ms_HaltRifleClipSetId;
			}
			default:
			{
				break;
			}
		}
	}
	
	return CLIP_SET_ID_INVALID;
}

fwMvClipSetId CTaskCombat::ChooseClipSetForQuickGlance(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return CLIP_SET_ID_INVALID;
	}

	//Ensure the equipped weapon info is valid.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	//get override if weapon specifies.
	fwMvClipSetId overrideGestureClipSet = pWeaponInfo->GetGestureGlancesOverride(rPed);
	if(overrideGestureClipSet != CLIP_SET_ID_INVALID)
	{
		return overrideGestureClipSet;
	}
	
	//Check if the ped is a gang ped.
	if(rPed.IsGangPed())
	{
		//Check if the weapon is a pistol.
		if(pWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL)
		{
			return ms_GlancesPistolGangClipSetId;
		}
		//Check if the weapon is an smg.
		else if(pWeaponInfo->GetGroup() == WEAPONGROUP_SMG)
		{
			return ms_GlancesSmgGangClipSetId;
		}
	}
	
	//Check if the weapon is a pistol.
	if(pWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL)
	{
		return ms_GlancesPistolClipSetId;
	}
	//Check if the weapon is a rifle.
	else if(pWeaponInfo->GetGroup() == WEAPONGROUP_RIFLE)
	{
		return ms_GlancesRifleClipSetId;
	}
	
	return CLIP_SET_ID_INVALID;
}

bool CTaskCombat::GetDesiredCoverStateToResume(s32 &iDesiredState)
{
	CPed* pPed = GetPed();
	CCoverPoint* pCoverPoint = pPed->GetCoverPoint();
	if (pCoverPoint)
	{
		// Make sure the ped is close enough to be using the cover point
		Vector3 vCoverCoords(Vector3::ZeroType);
		pCoverPoint->GetCoverPointPosition(vCoverCoords);

		// If we are coming from already being in cover then try to stay in cover (otherwise check if we are close enough to at least move to it)
		if(pPed->GetIsInCover())
		{
			Vec3V vToCover = VECTOR3_TO_VEC3V(vCoverCoords) - pPed->GetTransform().GetPosition();
			vToCover.SetZ(V_ZERO);
			if(IsLessThanAll(MagSquared(vToCover), ScalarVFromF32(square(MAX_DISTANCE_FOR_IN_COVER_STATE))))
			{
				iDesiredState = State_InCover;
				return true;
			}
		}

		// Make sure the cover position is in our defensive area (if we have one) or within our attack window
		CDefensiveArea* pDefensiveArea = pPed->GetPedIntelligence()->GetDefensiveArea();
		if(pDefensiveArea->IsActive())
		{
			if(!pDefensiveArea->IsPointInDefensiveArea(vCoverCoords))
			{
				iDesiredState = State_Start;
				return false;
			}
		}
		else if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_WillAdvance &&
			CTaskMoveWithinAttackWindow::IsPositionOutsideAttackWindow(pPed, m_pPrimaryTarget, VECTOR3_TO_VEC3V(vCoverCoords), true))
		{
			iDesiredState = State_Start;
			return false;
		}

		iDesiredState = State_MoveToCover;
		return true;
	}

	return false;
}

int CTaskCombat::ChooseStateToResumeTo(bool& bKeepSubTask)
{
	//Keep the sub-task.
	bKeepSubTask = true;

	// Force going back to fire if we aborted due to static movement
	if(m_uFlags.IsFlagSet(ComF_TaskAbortedForStaticMovement))
	{
		// Only do this if we actually got a valid weapon, fix for B* 1126663
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(GetPed());
		if (pWeaponInfo && pWeaponInfo->GetIsGunOrCanBeFiredLikeGun())
		{
			return State_Fire;
		}

		m_uFlags.ClearFlag(ComF_TaskAbortedForStaticMovement);
	}

	//Check if the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check the task type.
		switch(pSubTask->GetTaskType())
		{
			case CTaskTypes::TASK_ADVANCE:
			{
				return State_Advance;
			}
			case CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT:
			{
				return State_MoveToTacticalPoint;
			}
			case CTaskTypes::TASK_CHARGE:
			{
				return State_ChargeTarget;
			}
			default:
			{
				break;
			}
		}
	}

	//Check the previous state.
	int iPreviousState = GetPreviousState();
	switch(iPreviousState)
	{
		case State_MoveWithinAttackWindow:
		case State_ChaseOnFoot:
		{
			return iPreviousState;
		}
		case State_MoveToCover:
		case State_InCover:
		{
			CPed* pPed = GetPed();

			s32 iDesiredState = State_Start;
			if(GetDesiredCoverStateToResume(iDesiredState))
			{
				if(iDesiredState == State_MoveToCover)
				{
					// Need to make sure the desired cover point is set and clear our existing cover point (due to the logic of the on enter of move to cover state).
					pPed->SetDesiredCoverPoint(pPed->GetCoverPoint());
					pPed->ReleaseCoverPoint();
				}
			}
			else
			{
				pPed->ReleaseCoverPoint();
			}

			return iDesiredState;
		}
		default:
		{
			break;
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_Start;
}

void CTaskCombat::ClearFiringAtTarget()
{
	//Ensure we are firing at the target.
	if(!m_uFlags.IsFlagSet(ComF_FiringAtTarget))
	{
		return;
	}
	
	//Note that we are no longer firing at the target.
	m_uFlags.ClearFlag(ComF_FiringAtTarget);
	
	//Ensure the target is valid.
	if(!m_pPrimaryTarget)
	{
		return;
	}
	
	//Decrease the number of peds firing at the target.
	m_pPrimaryTarget->GetPedIntelligence()->DecreaseNumPedsFiringAt();
}

void CTaskCombat::ClearHoldingFire()
{
	//Ensure we are holding fire.
	if(!IsHoldingFire())
	{
		return;
	}

	//Clear the reason.
	ReasonToHoldFire nReasonWeWereHoldingFire = m_nReasonToHoldFire;
	m_nReasonToHoldFire = RTHF_None;

	//Note that we are no longer holding fire.
	OnNoLongerHoldingFire(nReasonWeWereHoldingFire);
}

void CTaskCombat::EquipWeaponForThreat()
{
	//Ensure the target is valid.
	const CPed* pTarget = m_pPrimaryTarget;
	if(!pTarget)
	{
		return;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	if (pPed->GetIsDrivingVehicle() && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockWeaponSwitching) || pPed->GetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponEquipping)))
	{
		return;
	}
		
	//Create the input.
	CEquipWeaponHelper::EquipWeaponForTargetInput input;
	input.m_EquipBestWeaponInput.m_bProcessWeaponInstructions = true;
	input.m_EquipBestMeleeWeaponInput.m_bProcessWeaponInstructions = true;

	//Equip a weapon for the threat.
	CEquipWeaponHelper::EquipWeaponForTarget(*pPed, *pTarget, input);

	// Total hack to make sure if we are switching to unarmed and have an ambient prop in our hand that it happens
	if(CEquipWeaponHelper::ShouldPedSwapWeapon(*pPed))
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if( pWeaponInfo && pWeaponInfo->GetIsUnarmed() &&
			pWeaponObject && pWeaponObject->m_nObjectFlags.bAmbientProp )
		{
			pPed->GetWeaponManager()->CreateEquippedWeaponObject();
		}
	}

	// Equip weapon resets the time until next burst so reset it so we can fire immediately
	pPed->GetPedIntelligence()->GetFiringPattern().ResetTimeUntilNextBurst();
}

void CTaskCombat::ForceFiringAtTarget()
{
	//Clear holding fire.
	ClearHoldingFire();
		
	//Set the firing at target.
	SetFiringAtTarget();
}

void CTaskCombat::GetAimTarget(CAITarget& rTarget) const
{
	//Ensure the gun task is running.
	const CTaskGun* pTask = static_cast<CTaskGun *>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
	if(!pTask)
	{
		return;
	}

	//Set the target.
	rTarget = pTask->GetTarget();
}

CTaskCombat::GestureType CTaskCombat::GetGestureTypeForClipSet(fwMvClipSetId clipSetId, CPed* pPed) const
{
	fwMvClipSetId overrideBeckonGestureClipSet(CLIP_SET_ID_INVALID);
	fwMvClipSetId overrideOverThereGestureClipSet(CLIP_SET_ID_INVALID);
	fwMvClipSetId overrideHaltGestureClipSet(CLIP_SET_ID_INVALID);

	if(pPed)
	{
		const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
		if(pWeaponManager)
		{
			const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
			if(pWeaponInfo)
			{
				overrideBeckonGestureClipSet = pWeaponInfo->GetGestureBeckonOverride(*pPed);
				overrideOverThereGestureClipSet = pWeaponInfo->GetGestureOverThereOverride(*pPed);
				overrideHaltGestureClipSet = pWeaponInfo->GetGestureHaltOverride(*pPed);
			}
		}
	}

	if(clipSetId != CLIP_SET_ID_INVALID)
	{

		//Check the clip set.
		if((clipSetId == ms_BeckonPistolClipSetId) || (clipSetId == ms_BeckonPistolGangClipSetId) ||
			(clipSetId == ms_BeckonRifleClipSetId) || (clipSetId == ms_BeckonSmgGangClipSetId) || (clipSetId == overrideBeckonGestureClipSet) )
		{
			return GT_Beckon;
		}
		else if((clipSetId == ms_OverTherePistolClipSetId) || (clipSetId == ms_OverTherePistolGangClipSetId) ||
			(clipSetId == ms_OverThereRifleClipSetId) || (clipSetId == ms_OverThereSmgGangClipSetId) || (clipSetId == overrideOverThereGestureClipSet))
		{
			return GT_OverThere;
		}
		else if((clipSetId == ms_HaltPistolClipSetId) || (clipSetId == ms_HaltPistolGangClipSetId) ||
			(clipSetId == ms_HaltRifleClipSetId) || (clipSetId == ms_HaltSmgGangClipSetId) || (clipSetId == overrideHaltGestureClipSet))
		{
			return GT_Halt;
		}
	}
	
	return GT_None;
}

bool CTaskCombat::HasClearLineOfSightToTarget() const
{
	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(false);
	if(!pTargeting)
	{
		return false;
	}

	//Ensure the line of sight is clear.
	if(pTargeting->GetLosStatus(m_pPrimaryTarget, false) != Los_clear)
	{
		return false;
	}

	return true;
}

void CTaskCombat::HoldFireFor(float fTime)
{
	u32 uTimeToStop = (fwTimer::GetTimeInMilliseconds() + (u32)(fTime * 1000.0f));
	m_uTimeToStopHoldingFire = Max(m_uTimeToStopHoldingFire, uTimeToStop);
}

bool CTaskCombat::IsVehicleSafeForJackOrArrest(const CVehicle* pTargetVehicle) const
{
	if(!pTargetVehicle)
	{
		return false;
	}

	if(pTargetVehicle->GetLayoutInfo()->GetDisableJackingAndBusting() )
	{
		return false;
	}

	if(pTargetVehicle->IsUpsideDown() || pTargetVehicle->IsOnItsSide())
	{
		return false;
	}

	// Don't attempt if the target vehicle is on fire or the target ped is on fire
	if( pTargetVehicle->m_nFlags.bIsOnFire || pTargetVehicle->GetVehicleDamage()->GetPetrolTankOnFire() || g_fireMan.IsEntityOnFire(pTargetVehicle) )
	{
		return false;
	}

	// Helis have to be landed
	if(pTargetVehicle->InheritsFromHeli())
	{
		const CHeli* pHeli = static_cast<const CHeli *>(pTargetVehicle);
		if(!pHeli->GetHeliIntelligence()->IsLanded())
		{
			return false;
		}
	}

	return true;
}

bool CTaskCombat::IsArresting() const
{
	//Check the state.
	switch(GetState())
	{
		case State_GoToEntryPoint:
		{
			//Ensure we can bust.
			if(!GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanBust))
			{
				break;
			}

			return true;
		}
		case State_ArrestTarget:
		{
			return true;
		}
		default:
		{
			break;
		}
	}

	return false;
}

bool CTaskCombat::IsCoverPointInDefensiveArea(const CCoverPoint& rCoverPoint) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Assert that the defensive area is active.
	taskAssert(pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive());

	//Ensure the cover point position is valid.
	Vector3 vCoverPointPosition;
	if(!rCoverPoint.GetCoverPointPosition(vCoverPointPosition))
	{
		return false;
	}

	//Ensure the cover point position is in the defensive area.
	if(!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(vCoverPointPosition))
	{
		return false;
	}
	
	return true;
}

bool CTaskCombat::IsEventTypeDirectlyThreatening(int iEventType) const
{
	//Check the event type.
	switch(iEventType)
	{
		case EVENT_DAMAGE:
		case EVENT_GUN_AIMED_AT:
		case EVENT_MELEE_ACTION:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskCombat::IsInDefensiveArea() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the ped is using a cover point.
	CCoverPoint* pCoverPoint = pPed->GetCoverPoint();
	if(pCoverPoint)
	{
		//Check if the cover point is in the defensive area.
		if(IsCoverPointInDefensiveArea(*pCoverPoint))
		{
			return true;
		}
	}
	else
	{
		//Check if the ped is physically in the defensive area.
		if(IsPhysicallyInDefensiveArea())
		{
			return true;
		}
	}

	return false;
}

bool CTaskCombat::IsNearbyFriendlyNotInCover() const
{
	//Calculate the max distance.
	static dev_float s_fMaxDistance = 15.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));

	//Iterate over the nearby peds.
	int iNumPedsChecked = 0;
	static dev_s32 s_iMaxPedsToCheck = 4;
	CEntityScannerIterator entityList = GetPed()->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEnt = entityList.GetFirst(); (pEnt != NULL) && (iNumPedsChecked < s_iMaxPedsToCheck); pEnt = entityList.GetNext())
	{
		//Increase the number of peds checked.
		++iNumPedsChecked;

		//Ensure the ped is not injured.
		CPed* pOtherPed = static_cast<CPed *>(pEnt);
		if(pOtherPed->IsInjured())
		{
			continue;
		}

		//Ensure the ped is not in cover.
		if(pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER, true))
		{
			continue;
		}

		//Ensure the peds are friendly.
		if(!GetPed()->GetPedIntelligence()->IsFriendlyWithByAnyMeans(*pOtherPed))
		{
			continue;
		}

		//Ensure the distance is valid.
		ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pOtherPed->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			break;
		}

		return true;
	}

	return false;
}

bool CTaskCombat::IsOnFoot() const
{
	return GetPed()->GetIsOnFoot();
}

bool CTaskCombat::IsOnScreen() const
{
	return (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen));
}

bool CTaskCombat::IsPhysicallyInDefensiveArea() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Assert that the defensive area is active.
	taskAssert(pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive());

	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Check if the ped position is in the defensive area.
	if(pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(vPedPosition)))
	{
		return true;
	}

	//Some defensive areas are very small, and on the ground.
	//For these, we need to take the ground to root offset into account.

	//Calculate the new ped position (Z).
	ScalarV scGroundToRootOffset = ScalarVFromF32(pPed->GetCapsuleInfo()->GetGroundToRootOffset());
	ScalarV scPedPositionZ = Subtract(vPedPosition.GetZ(), scGroundToRootOffset);

	//Calculate the adjusted ped position.
	Vec3V vAdjustedPedPosition = vPedPosition;
	vAdjustedPedPosition.SetZ(scPedPositionZ);

	//Check if the adjusted ped position is in the defensive area.
	if(pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(vAdjustedPedPosition)))
	{
		return true;
	}

	return false;
}

bool CTaskCombat::IsPotentialStateChangeUrgent() const
{
	return (CanReactToBuddyShot() || CanReactToImminentExplosion() || CanReactToExplosion() || m_uFlags.IsFlagSet(ComF_TargetLost));
}

bool CTaskCombat::IsTargetOnFoot() const
{
	return (m_pPrimaryTarget && m_pPrimaryTarget->GetIsOnFoot());
}

bool CTaskCombat::IsTargetWanted() const
{
	return (m_pPrimaryTarget && m_pPrimaryTarget->IsPlayer() &&
		(m_pPrimaryTarget->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN));
}

bool CTaskCombat::IsWithinDistanceOfTarget(float fMaxDistance) const
{
	//Ensure the target is valid.
	if(!m_pPrimaryTarget)
	{
		return false;
	}

	//Ensure the distance is valid.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::HasTargetFired(float queryTimeSeconds) const
{
	//Ensure the target is valid.
	if( m_pPrimaryTarget )
	{
		// Compare stored time to query time
		// NOTE: stored time elapsed since firing is capped (see CPed::ProcessControl_ResetVariables and m_fTimeSinceLastShotFired)
		const float fTimeSinceLastShotFired = m_pPrimaryTarget->GetTimeSinceLastShotFired();
		if( fTimeSinceLastShotFired <= queryTimeSeconds )
		{
			return true;
		}	
	}

	// by default report target has not fired within query time
	return false;
}

bool CTaskCombat::IsVehicleMovingSlowerThanSpeed(const CVehicle& rVehicle, float fSpeed)
{
	//Assert that the speed is valid.
	taskAssert(fSpeed >= 0.0f);

	//Grab the vehicle speed.
	if(rVehicle.IsCachedAiDataValid())
	{
		// If valid then used the cached data
		float fVehicleSpeed = rVehicle.GetAiXYSpeed();
		return (fVehicleSpeed < fSpeed);
	}
	else
	{
		// If the cached data is invalid then get the velocity directly
		Vec3V vehVelocity = VECTOR3_TO_VEC3V(rVehicle.GetVelocity());
		ScalarV fMaxSpeed = ScalarVFromF32(square(fSpeed));
		return IsLessThanAll(MagSquared(vehVelocity.GetXY()), fMaxSpeed) != 0;
	}
}

bool CTaskCombat::GetTimeSinceTargetVehicleMoved(u32& uTimeSinceTargetVehicleMoved) const
{
	if (m_uTimeTargetVehicleLastMoved != 0)
	{
		u32 currTime = fwTimer::GetTimeInMilliseconds();
		if (currTime < m_uTimeTargetVehicleLastMoved)
		{
			uTimeSinceTargetVehicleMoved = ((MAX_UINT32 - m_uTimeTargetVehicleLastMoved) + currTime);
		}
		else
		{
			uTimeSinceTargetVehicleMoved = (currTime - m_uTimeTargetVehicleLastMoved);
		}

		return true;
	}

	return false;
}

void CTaskCombat::ClearEarlyVehicleEntry()
{
	if(m_uFlags.IsFlagSet(ComF_DoingEarlyVehicleEntry))
	{
		if(Verifyf(ms_uNumDriversEnteringVehiclesEarly > 0, "Number of drivers entering vehicles early became mismatched, is already 0"))
		{
			ms_uNumDriversEnteringVehiclesEarly--;
		}

		m_uFlags.ClearFlag(ComF_DoingEarlyVehicleEntry);
	}
}

void CTaskCombat::ClearPrimaryDefensiveArea()
{
	//Assert that we can clear the primary defensive area.
	taskAssert(CanClearPrimaryDefensiveArea());

	//Grab the ped.
	CPed* pPed = GetPed();

	//Clear the flag.
	pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_ClearPrimaryDefensiveAreaWhenReached);

	//Clear the primary defensive area.
	pPed->GetPedIntelligence()->GetDefensiveAreaManager()->ClearPrimaryDefensiveArea();
}

void CTaskCombat::OnGunShot()
{
	//Check if we are in melee.
	if(GetState() == State_MeleeAttack)
	{
		//Equip our best weapon.
		CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
		if(pWeaponManager)
		{
			pWeaponManager->EquipBestWeapon();
		}
	}
}

void CTaskCombat::OnHoldingFire(ReasonToHoldFire nReasonToHoldFire)
{
	//Check if we are holding fire due to limit combatants.
	if(nReasonToHoldFire == RTHF_LimitCombatants)
	{
		//Set the combat movement to defensive.
		GetPed()->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CCombatData::CM_Defensive);
	}
}

void CTaskCombat::OnNoLongerHoldingFire(ReasonToHoldFire nReasonWeWereHoldingFire)
{
	//Check if we were holding fire due to limit combatants.
	if(nReasonWeWereHoldingFire == RTHF_LimitCombatants)
	{
		//Reset the combat movement.
		ResetCombatMovement();
	}
}

void CTaskCombat::OpenCombat()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Reset the defensive area.
	pPed->GetPedIntelligence()->GetDefensiveArea()->Reset();

	//Set the combat movement to defensive.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_ClearAreaSetDefensiveIfDefensiveAreaReached))
	{
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CCombatData::CM_Defensive);
	}
	else
	{
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CCombatData::CM_WillAdvance);
	}

	//Clear the flag.
	pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_ClearAreaSetDefensiveIfDefensiveAreaReached);
	pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_ClearAreaSetAdvanceIfDefensiveAreaReached);
}

void CTaskCombat::PlayRoadBlockAudio()
{
	//Ensure the primary target is valid.
	if(!taskVerifyf(m_pPrimaryTarget, "The primary target is invalid."))
	{
		return;
	}

	//Ensure the primary target is in a vehicle.
	if(!taskVerifyf(m_pPrimaryTarget->GetIsInVehicle(), "The primary target is not in a vehicle."))
	{
		return;
	}

	const CVehicle* pVehicle = m_pPrimaryTarget->GetVehiclePedInside();
	taskAssert(pVehicle);

	//Make sure no one has triggered this line recently.
	static u32 s_uTimeLastTriggered = 0;
	if((s_uTimeLastTriggered == 0) ||
		(CTimeHelpers::GetTimeSince(s_uTimeLastTriggered) > 5000))
	{
		//Set the time last triggered.
		s_uTimeLastTriggered = fwTimer::GetTimeInMilliseconds();

		//Check if the vehicle is a car.
		if(pVehicle->InheritsFromAutomobile())
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				GetPed()->NewSay("STOP_VEHICLE_CAR_MEGAPHONE");
			}
			else
			{
				GetPed()->NewSay("STOP_VEHICLE_CAR_WARNING_MEGAPHONE");
			}
		}
		else
		{
			GetPed()->NewSay("STOP_VEHICLE_GENERIC_MEGAPHONE");
		}
	}

	//Note that we have played road block audio.
	m_uRunningFlags.SetFlag(RF_HasPlayedRoadBlockAudio);
}

void CTaskCombat::ProcessDefensiveArea()
{
	CPed* pPed = GetPed();
	CPedIntelligence* pPedIntell = pPed->GetPedIntelligence();

	// Update whether we're in the defensive area assigned to us if we have one
	if( !pPedIntell->GetDefensiveArea()->IsActive() )
	{
		return;
	}

	//Clear the flag.
	m_uFlags.ClearFlag(ComF_IsInDefensiveArea);

	//Check if we are in the defensive area.
	if(!IsInDefensiveArea())
	{
		return;
	}

	//Set the flag.
	m_uFlags.SetFlag(ComF_IsInDefensiveArea);

	CDefensiveAreaManager* pDefensiveAreaMgr = pPedIntell->GetDefensiveAreaManager();
	CDefensiveArea* pAlternateDefensiveArea = pDefensiveAreaMgr->GetAlternateDefensiveArea();

	// Quit out now if we don't have an alternate defensive area or we're looking for cover in our current defensive area
	if( !pAlternateDefensiveArea || !pAlternateDefensiveArea->IsActive() ||
		(m_pCoverFinder && pDefensiveAreaMgr->GetCurrentDefensiveArea() == pDefensiveAreaMgr->GetDefensiveAreaForCoverSearch()) )
	{
		return;
	}

	// Check if the target is far enough away to not be considered an immediate threat to us in our defensive area
	ScalarV scDistSq = DistSquared(m_pPrimaryTarget->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
	ScalarV scTargetDistSq = ScalarVFromF32(rage::square(pPedIntell->GetCombatBehaviour().GetCombatFloat(kAttribFloatMinimumDistanceToTarget)));
	if(IsGreaterThanAll(scDistSq, scTargetDistSq))
	{
		return;
	}

	// Need to make sure the target isn't too close to the alternate defensive area point we would move to
	Vector3 vNewDefensiveAreaPoint;
	CTaskCombat::GetClosestDefensiveAreaPointToNavigateTo(pPed, vNewDefensiveAreaPoint, pAlternateDefensiveArea);
	scDistSq = DistSquared(m_pPrimaryTarget->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vNewDefensiveAreaPoint));
	scTargetDistSq = ScalarVFromF32(rage::square(pPedIntell->GetCombatBehaviour().GetCombatFloat(kAttribFloatMinimumDistanceToTarget) + 0.5f));
	if(IsLessThanAll(scDistSq, scTargetDistSq))
	{
		return;
	}

	// We've passed all the checks for threat which means we should use our alternate area
	pDefensiveAreaMgr->SetCurrentDefensiveArea(pAlternateDefensiveArea);

	// Since we've changed defensive areas we clear the flag again and recheck if we our in it (chances are we are not)
	m_uFlags.ClearFlag(ComF_IsInDefensiveArea);

	//Check if we are in the defensive area.
	if(IsInDefensiveArea())
	{
		//Set the flag.
		m_uFlags.SetFlag(ComF_IsInDefensiveArea);
	}
}

void CTaskCombat::ProcessRoadBlockAudio()
{
	//Check if we should play road block audio.
	if(ShouldPlayRoadBlockAudio())
	{
		//Play the road block audio.
		PlayRoadBlockAudio();
	}
}

void CTaskCombat::ProcessActionsForDefensiveArea()
{
	//Check if we can open combat.
	if(CanOpenCombat())
	{
		OpenCombat();
	}

	//Check if we can clear our primary defensive area.
	if(CanClearPrimaryDefensiveArea())
	{
		ClearPrimaryDefensiveArea();
	}
}

void CTaskCombat::ProcessAimPose()
{
	//Keep track of the aiming state.
	bool bWasPlayingAimPoseTransitionLastFrame = m_uFlags.IsFlagSet(ComF_IsPlayingAimPoseTransition);
	bool bIsPlayingAimPoseTransition = false;
	
	//Check if the aiming task is valid.
	const CTaskMotionAiming* pMotionAimingTask = static_cast<CTaskMotionAiming *>(GetPed()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
	if(pMotionAimingTask)
	{
		//Check if we are playing an aim pose transition.
		bIsPlayingAimPoseTransition = pMotionAimingTask->IsPlayingTransition();
	}
	
	//Update the flags.
	m_uFlags.ChangeFlag(ComF_WasPlayingAimPoseTransitionLastFrame, bWasPlayingAimPoseTransitionLastFrame);
	m_uFlags.ChangeFlag(ComF_IsPlayingAimPoseTransition, bIsPlayingAimPoseTransition);

	//Check if the varied aim pose task is valid.
	CTaskVariedAimPose* pVariedAimPoseTask = static_cast<CTaskVariedAimPose *>(FindSubTaskOfType(CTaskTypes::TASK_VARIED_AIM_POSE));
	if(pVariedAimPoseTask)
	{
		//Check if we will need to react to an imminent explosion soon.
		bool bWillNeedToReactToImminentExplosionSoon = WillNeedToReactToImminentExplosionSoon();

		//Generate the flags.
		bool bDisableNewPosesFromDefaultStandingPose = bWillNeedToReactToImminentExplosionSoon;
		bool bDisableTransitionsExceptToDefaultStandingPose = bWillNeedToReactToImminentExplosionSoon;
		bool bDisableTransitionsFromDefaultStandingPose = bWillNeedToReactToImminentExplosionSoon;

		//Set the flags.
		pVariedAimPoseTask->GetConfigFlags().ChangeFlag(CTaskVariedAimPose::CF_DisableNewPosesFromDefaultStandingPose,
			bDisableNewPosesFromDefaultStandingPose);
		pVariedAimPoseTask->GetConfigFlags().ChangeFlag(CTaskVariedAimPose::CF_DisableTransitionsExceptToDefaultStandingPose,
			bDisableTransitionsExceptToDefaultStandingPose);
		pVariedAimPoseTask->GetConfigFlags().ChangeFlag(CTaskVariedAimPose::CF_DisableTransitionsFromDefaultStandingPose,
			bDisableTransitionsFromDefaultStandingPose);
	}
}

void CTaskCombat::ProcessAudio()
{
	CPed* pPed = GetPed();

	//Ensure the audio should be processed this frame.
	if(!pPed->GetPedIntelligence()->GetAudioDistributer().ShouldBeProcessedThisFrame())
	{
		return;
	}

	//Ensure the flag is not set.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableCombatAudio))
	{
		return;
	}

	//Ensure we are not in a vehicle -- if so, defer to that task's audio.
	if(pPed->GetIsInVehicle())
	{
		return;
	}

	//Say the cop sees weapon audio line as soon as possible
	if(pPed->IsRegularCop() && m_pPrimaryTarget->GetPlayerWanted())
	{
		if(m_pPrimaryTarget->GetPlayerWanted()->TriggerCopSeesWeaponAudio(pPed, false))
		{
			return;
		}
	}

	//Ensure some time has passed before saying any lines.
	static dev_float s_fMinTime = 2.0f;
	if(GetTimeRunning() < s_fMinTime)
	{
		return;
	}

	//Check if we are in cover.
	if(GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER, true))
	{
		//Defer to the cover audio half the time.
		if(fwRandom::GetRandomTrueFalse())
		{
			return;
		}
	}

	bool bIsLosClearForAudio = true;
	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( false );
	if( pPedTargetting )
	{
		LosStatus losStatus = pPedTargetting->GetLosStatus(m_pPrimaryTarget, false);
		bIsLosClearForAudio = (losStatus == Los_clear || losStatus == Los_blockedByFriendly);
	}

	//These are arranged in prioritized order.
	if(m_pPrimaryTarget->GetPedType() == PEDTYPE_ANIMAL)
	{
		if(fwRandom::GetRandomTrueFalse())
		{
			pPed->NewSay("GENERIC_INSULT_MED");
		}
		else
		{
			pPed->NewSay("GENERIC_INSULT_HIGH");
		}
	}
	else if(m_pPrimaryTarget->IsInjured() && bIsLosClearForAudio &&
		(!pPed->IsLawEnforcementPed() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen)))
	{
		pPed->NewSay("SUSPECT_KILLED");
	}
	else if(IsArresting() && IsOnScreen() && IsWithinDistanceOfTarget(15.0f) && bIsLosClearForAudio)
	{
		if(pPed->NewSay(m_pPrimaryTarget->GetIsOnFoot() || fwRandom::GetRandomTrueFalse() ? "GET_HIM" : "SURROUNDED") && !m_pPrimaryTarget->GetIsArrested())
		{
			const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if(pWeaponInfo && pWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL)
			{
				CTaskMotionAiming* pTaskMotionAiming = static_cast<CTaskMotionAiming *>(
					pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
				if(pTaskMotionAiming)
				{
					static fwMvClipSetId s_ClipSetId("combat@aim_variations@arrest",0xAFFEFA1B);
					pTaskMotionAiming->PlayCustomAimingClip(s_ClipSetId);
				}
			}
		}
	}
	else if(pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetCurrentEvent() && bIsLosClearForAudio &&
		pPed->GetPedIntelligence()->GetCurrentEvent()->GetEventType() == EVENT_DRAGGED_OUT_CAR)
	{
		pPed->NewSay("JACKED_FIGHT");
	}
	else if(pPed->IsRegularCop() && (m_nReasonToHoldFire == RTHF_LackOfHostility) && bIsLosClearForAudio)
	{
		pPed->NewSay(m_pPrimaryTarget->GetIsOnFoot() || fwRandom::GetRandomTrueFalse() ? "COP_TALKDOWN" : "SURROUNDED");
	}
	else if(GetState() == State_Fire)
	{
		pPed->NewSay(fwRandom::GetRandomTrueFalse() ? "COMBAT_TAUNT" : "SHOOT");
	}
	else if(fwRandom::GetRandomTrueFalse() && IsOnFoot() && IsTargetOnFoot() &&
		DoesTargetHaveGun() && HasTargetFired(2.0f) && IsNearbyFriendlyNotInCover())
	{
		pPed->NewSay("TAKE_COVER");
	}
	else
	{
		pPed->NewSay("GENERIC_WAR_CRY");
	}
}

void CTaskCombat::ProcessCombatDirector()
{
	//Update the combat director every once in a while.
	m_fLastCombatDirectorUpdateTimer += GetTimeStep();
	if(m_fLastCombatDirectorUpdateTimer < ms_Tunables.m_fTimeBetweenCombatDirectorUpdates)
	{
		return;
	}
	
	//Clear the timer.
	m_fLastCombatDirectorUpdateTimer = 0.0f;
	
	//Refresh the combat director.
	RefreshCombatDirector();
}

void CTaskCombat::ProcessCombatOrders()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Don't process combat orders for mission peds
	if(pPed->PopTypeIsMission())
	{
		return;
	}

	//Get the shoot rate modifier.
	float fModifier = m_CombatOrders.GetShootRateModifier();
	if(fModifier != 1.0f)
	{
		//Apply the shoot rate modifier.
		float fShootRateModifier = pPed->GetPedIntelligence()->GetCombatBehaviour().GetShootRateModifier() * fModifier;
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetShootRateModifierThisFrame(fShootRateModifier);
	}
	
	//Check if the cover fire flag is set.
	if(m_CombatOrders.GetActiveFlags().IsFlagSet(CCombatOrders::AF_CoverFire))
	{
		//Clear the cover fire flag.
		m_CombatOrders.GetActiveFlags().ClearFlag(CCombatOrders::AF_CoverFire);
		
		//We can only provide cover fire when we are in cover.
		if(GetState() == State_InCover)
		{
			//Grab the cover task.
			CTaskInCover* pTask = static_cast<CTaskInCover *>(FindSubTaskOfType(CTaskTypes::TASK_IN_COVER));
			if(pTask)
			{
				//Set the cover fire flag.
				pTask->SetCoverFlag(CTaskCover::CF_CoverFire);
			}
		}
	}
}

void CTaskCombat::ProcessExplosion()
{
	//Ensure the explosion is valid.
	if(!m_uFlags.IsFlagSet(ComF_ExplosionValid))
	{
		return;
	}

	//Check if the time to react has been exceeded.
	if(fwTimer::GetTimeInMilliseconds() >= m_uTimeToReactToExplosion)
	{
		//Check if the time has exceeded the window.
		static u32 s_uMaxTimeToReactToExplosion = 250;
		if(fwTimer::GetTimeInMilliseconds() > (m_uTimeToReactToExplosion + s_uMaxTimeToReactToExplosion))
		{
			m_uFlags.ClearFlag(ComF_ExplosionValid);
		}
	}
}

void CTaskCombat::ProcessFacialIdleClip()
{
	// Reload task handles facial
	if(!GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsReloading))
	{
		//Ensure the facial data is valid.
		CFacialDataComponent* pFacialData = GetPed()->GetFacialData();
		if(!pFacialData)
		{
			return;
		}
		
		//Request the angry facial idle clip.
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Angry);
	}
}

void CTaskCombat::ProcessFiringAtTarget()
{
	//Check if we are holding fire.
	if(IsHoldingFire())
	{
		//Clear the firing at target state.
		ClearFiringAtTarget();
	}
	else
	{
		//Set the firing at target state.
		SetFiringAtTarget();
	}
}

void CTaskCombat::ProcessHoldFire()
{
	//Check if we should hold fire.
	ReasonToHoldFire nReasonToHoldFire = ShouldHoldFire();
	if(nReasonToHoldFire != RTHF_None)
	{
		//Set holding fire.
		SetHoldingFire(nReasonToHoldFire);
	}
	else
	{
		//Clear holding fire.
		ClearHoldingFire();
	}
}

void CTaskCombat::ProcessMovementClipSet()
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Choose a clip set for the frantic runs.
	bool bAreDefaultRunsFrantic = false;
	fwMvClipSetId clipSetIdForFranticRuns = ChooseClipSetForFranticRuns(bAreDefaultRunsFrantic);
	bool bHasClipSetForFranticRuns = (clipSetIdForFranticRuns != CLIP_SET_ID_INVALID);

	//Check the state.
	bool bCanUseFranticRuns		= (bAreDefaultRunsFrantic || bHasClipSetForFranticRuns);
	bool bShouldUseFranticRuns	= bCanUseFranticRuns && ShouldUseFranticRuns();
	bool bIsUsingFranticRuns	= m_uFlags.IsFlagSet(ComF_IsUsingFranticRuns);

	//Process the streaming.
	if(bShouldUseFranticRuns && bHasClipSetForFranticRuns)
	{
		//Request the clip set.
		m_FranticRunsClipSetRequestHelper.RequestClipSet(clipSetIdForFranticRuns);
	}
	else
	{
		//Release the clip set.
		m_FranticRunsClipSetRequestHelper.ReleaseClipSet();
	}

	//If the clip set wants to be streamed out, we should not be using frantic runs.
	if(m_FranticRunsClipSetRequestHelper.IsActive() && m_FranticRunsClipSetRequestHelper.ShouldClipSetBeStreamedOut())
	{
		bShouldUseFranticRuns = false;
	}

	//Check if we want to switch to frantic runs.
	if(!bIsUsingFranticRuns && bShouldUseFranticRuns)
	{
		//Check if we can switch to frantic runs.
		bool bCanSwitchToFranticRuns = (!bHasClipSetForFranticRuns ||
			(m_FranticRunsClipSetRequestHelper.IsActive() && m_FranticRunsClipSetRequestHelper.ShouldClipSetBeStreamedIn() && m_FranticRunsClipSetRequestHelper.IsClipSetStreamedIn()));
		if(bCanSwitchToFranticRuns)
		{
			//Clear action mode.
			SetUsingActionMode(false);

			//Check if we have a clip set for the frantic runs.
			if(bHasClipSetForFranticRuns)
			{
				//Set the on-foot clip set.
				pPed->GetPrimaryMotionTask()->SetOnFootClipSet(clipSetIdForFranticRuns);
			}

			//Note that we are using frantic runs.
			m_uFlags.SetFlag(ComF_IsUsingFranticRuns);
		}
	}
	//Check if we want to stop using frantic runs.
	else if(bIsUsingFranticRuns && !bShouldUseFranticRuns)
	{
		//Check if we have a clip set for the frantic runs.
		if(bHasClipSetForFranticRuns)
		{
			//Reset the on-foot clip set.
			pPed->GetPrimaryMotionTask()->ResetOnFootClipSet(true);
		}

		//Set action mode.
		SetUsingActionMode(true);

		//Note that we are no longer using frantic runs.
		m_uFlags.ClearFlag(ComF_IsUsingFranticRuns);
	}
}

void CTaskCombat::ProcessPotentialBlast()
{
	//Check if the potential blast is valid.
	if(m_PotentialBlast.IsValid())
	{
		//Check if we have exceeded the time of the blast.
		if(fwTimer::GetTimeInMilliseconds() > m_PotentialBlast.m_uTimeOfExplosion)
		{
			//Reset the potential blast.
			m_PotentialBlast.Reset();
		}
		else
		{
			//Check if we aren't pinned down very much.
			if(GetPed()->GetPedIntelligence()->GetAmountPinnedDown() < 0.75f)
			{
				//Check if the potential blast is a projectile that is close to exploding.
				if((fwTimer::GetTimeInMilliseconds() + 1500) > m_PotentialBlast.m_uTimeOfExplosion)
				{
					//Say the audio.
					if(!(CNetwork::IsGameInProgress() && GetPed()->GetIsInVehicle()))
					{
						if(fwRandom::GetRandomTrueFalse())
						{
							GetPed()->NewSay("EXPLOSION_IMMINENT");
						}
						else
						{
							GetPed()->NewSay("DUCK");
						}
					}

					//Check if we are in cover.
					if(GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
					{
						//Get the target.
						CAITarget target;
						m_PotentialBlast.GetTarget(*GetPed(), target);

						//Get the target position.
						Vec3V vTargetPosition;
						target.GetPosition(RC_VECTOR3(vTargetPosition));

						//Check if we are close.
						static dev_float s_fMaxDistance = 15.0f;
						ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), vTargetPosition);
						ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
						if(IsLessThanAll(scDistSq, scMaxDistSq))
						{
							//Set our pinned down value.
							bool bForce = !GetPed()->GetPedResetFlag(CPED_RESET_FLAG_DisablePotentialBlastReactions);
							GetPed()->GetPedIntelligence()->SetAmountPinnedDown(100.0f, bForce);
						}
					}
				}
			}
		}
	}
}

void CTaskCombat::ProcessWeaponAnims()
{
	CPed* pPed = GetPed();

	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	CPedEquippedWeapon* pEquippedWeapon = pWeaponManager->GetPedEquippedWeapon();
	if(!pEquippedWeapon)
	{
		return;
	}

	// a clone ped spawned from a scenario point can still be carrying 
	// an 'OBJECT' (cup of coffee / bottle of grog) when it starts combat 
	// as it's inventory can get synced after the task tree. 
	// If we can't find the object we're carrying in the inventory, 
	// don't try to stream non existant anims in...
	// B*1286702 - do this for all peds in a network game
	//if(pPed->IsNetworkClone())
	if(NetworkInterface::IsGameInProgress())
	{
		if(pPed->GetInventory())
		{
			if(pEquippedWeapon->GetWeaponHash() != 0)
			{
				CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon( pEquippedWeapon->GetWeaponHash() );
				if(!pWeaponItem)
				{
					return;			
				}
			}
		}
	}

	if(pEquippedWeapon->GetWeaponHash() != m_uCachedWeaponHash)
	{
		//Release (just to make sure).
		pEquippedWeapon->ReleaseWeaponAnims();

		//Request the weapon anims
		pEquippedWeapon->RequestWeaponAnims();

		m_uCachedWeaponHash = pEquippedWeapon->GetWeaponHash();
	}
	// We have to continually request for the weapon anims otherwise they will never stay in memory
	else if( pEquippedWeapon->HaveWeaponAnimsBeenRequested() )
		pEquippedWeapon->AreWeaponAnimsStreamedIn();

}

void CTaskCombat::ProcessMovementType()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if we should use stationary combat.
	bool bShouldUseStationaryCombat = (pPed->PopTypeIsRandom() && pPed->IsLawEnforcementPed() && pPed->GetGroundPhysical() &&
		pPed->GetGroundPhysical()->GetIsTypeVehicle() && static_cast<CVehicle *>(pPed->GetGroundPhysical())->InheritsFromBoat() &&
		(pPed->GetGroundPhysical()->m_Buoyancy.GetStatus() != NOT_IN_WATER));
	if(bShouldUseStationaryCombat)
	{
		//Check if we are not using stationary combat.
		if(!m_uRunningFlags.IsFlagSet(RF_UsingStationaryCombat))
		{
			//Switch to stationary.
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CCombatData::CM_Stationary);

			//Set the flag.
			m_uRunningFlags.SetFlag(RF_UsingStationaryCombat);
		}

		return;
	}
	//Check if we are using stationary combat.
	else if(m_uRunningFlags.IsFlagSet(RF_UsingStationaryCombat))
	{
		//Set the default combat movement.
		CCombatBehaviour::SetDefaultCombatMovement(*pPed);

		//Clear the flag.
		m_uRunningFlags.ClearFlag(RF_UsingStationaryCombat);
	}

	//Check if we want to switch to advance if we can't find cover.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_SwitchToAdvanceIfCantFindCover) &&
		(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() != CCombatData::CM_WillAdvance))
	{
		//Check if we can't find cover (we have been cover-less for X seconds).
		static float s_fMinTimeSinceWeLastHadCover = 10.0f;
		if(m_fTimeSinceWeLastHadCover >= s_fMinTimeSinceWeLastHadCover)
		{
			//Switch to advance.
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CCombatData::CM_WillAdvance);
			return;
		}
	}

	//Check if we want to switch to defensive if cover has been found.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_SwitchToDefensiveIfInCover) &&
		(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() != CCombatData::CM_Defensive))
	{
		//Check if we are in cover.
		if(GetState() == State_InCover)
		{
			//Switch to defensive.
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(CCombatData::CM_Defensive);
			return;
		}
	}
}

void CTaskCombat::RefreshCombatDirector()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	//Refresh the combat director.
	pPed->GetPedIntelligence()->RefreshCombatDirector();
}

void CTaskCombat::ReleaseCombatDirector()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	//Release the combat director.
	pPed->GetPedIntelligence()->ReleaseCombatDirector();
}

void CTaskCombat::ReleaseTacticalAnalysis()
{
	//Ensure the tactical analysis is valid.
	if(!m_pTacticalAnalysis)
	{
		return;
	}

	//Release the tactical analysis.
	m_pTacticalAnalysis->Release();

	//Clear the tactical analysis.
	m_pTacticalAnalysis = NULL;
}

void CTaskCombat::RequestCombatDirector()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Request the combat director.
	pPed->GetPedIntelligence()->RequestCombatDirector();
}

void CTaskCombat::RequestTacticalAnalysis()
{
	//Release the tactical analysis.
	ReleaseTacticalAnalysis();

	//Ensure the target is valid.
	if(!m_pPrimaryTarget)
	{
		return;
	}

	//Request the tactical analysis.
	m_pTacticalAnalysis = CTacticalAnalysis::Request(*m_pPrimaryTarget);
}

void CTaskCombat::ResetCombatMovement()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped model info is valid.
	const CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
	if(!taskVerifyf(pPedModelInfo, "Ped model info is invalid."))
	{
		return;
	}

	//Ensure the combat info is valid.
	u32 uCombatInfoHash = pPedModelInfo->GetCombatInfoHash();
	const CCombatInfo* pCombatInfo = CCombatInfoMgr::GetCombatInfoByHash(uCombatInfoHash);
	if(!taskVerifyf(pCombatInfo, "Combat info is invalid."))
	{
		return;
	}

	//Reset the combat movement.
	pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatMovement(pCombatInfo->GetCombatMovement());
}

void CTaskCombat::SayInitialAudio()
{
	//Check if the ped is random, the target is a player, is in a vehicle, and is close.
	if(GetPed()->PopTypeIsRandom() && m_pPrimaryTarget && m_pPrimaryTarget->IsPlayer() &&
		m_pPrimaryTarget->GetIsInVehicle() && IsWithinDistanceOfTarget(20.0f))
	{
		GetPed()->NewSay("GENERIC_INSULT_HIGH", 0, false, false, -1,
			const_cast<CPed *>(m_pPrimaryTarget.Get()), ATSTRINGHASH("GENERIC_CURSE_MED", 0x27813be2));
	}
}

void CTaskCombat::SetFiringAtTarget()
{
	//Ensure we are not firing at the target.
	if(m_uFlags.IsFlagSet(ComF_FiringAtTarget))
	{
		return;
	}
	
	//Ensure the target is valid.
	if(!m_pPrimaryTarget)
	{
		return;
	}
	
	//Note that we are firing at the target.
	m_uFlags.SetFlag(ComF_FiringAtTarget);
	
	//Increase the number of peds firing at the target.
	m_pPrimaryTarget->GetPedIntelligence()->IncreaseNumPedsFiringAt();
}

void CTaskCombat::SetHoldingFire(ReasonToHoldFire nReasonToHoldFire)
{
	//Ensure the reason to hold fire is changing.
	if(nReasonToHoldFire == m_nReasonToHoldFire)
	{
		return;
	}

	//Clear the holding fire.
	ClearHoldingFire();

	//Ensure the reason is valid.
	if(nReasonToHoldFire == RTHF_None)
	{
		return;
	}

	//Set the reason to hold fire.
	m_nReasonToHoldFire = nReasonToHoldFire;

	//Note that we are holding fire.
	OnHoldingFire(m_nReasonToHoldFire);
}

void CTaskCombat::SetUsingActionMode(bool bUsingActionMode)
{
	//Set the action mode.
	GetPed()->SetUsingActionMode(bUsingActionMode, CPed::AME_Combat);
}

bool CTaskCombat::ShouldConsiderVictoriousInMelee() const
{
	//Ensure we are random.
	if(!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure we are not law-enforcement.
	if(GetPed()->IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure we are not a security guard, and the target is wanted.
	if(GetPed()->IsSecurityPed() && IsTargetWanted())
	{
		return false;
	}

	//Check if the melee group is valid.
	CCombatMeleeGroup* pGroup = CCombatManager::GetMeleeCombatManager()->FindMeleeGroup(*m_pPrimaryTarget);
	if(pGroup)
	{
		//Check if the target is moving away from the active combatant.
		u32 uTimeTargetStartedMovingAwayFromActiveCombatant = pGroup->GetTimeTargetStartedMovingAwayFromActiveCombatant();
		if(uTimeTargetStartedMovingAwayFromActiveCombatant != 0)
		{
			//Check if if the target has been running away for a while.
			static dev_u32 s_uMinTime = 5000;
			static dev_u32 s_uMaxTime = 7500;
			u32 uTime = (u32)GetPed()->GetRandomNumberInRangeFromSeed((float)s_uMinTime, (float)s_uMaxTime);
			if(CTimeHelpers::GetTimeSince(uTimeTargetStartedMovingAwayFromActiveCombatant) > uTime)
			{
				return true;
			}
		}

		// Always return true for targets that are riding a train
		const CEntity* pTargetEntity = pGroup->GetTarget();
		if( pTargetEntity && pTargetEntity->GetIsTypePed() && static_cast<const CPed*>(pTargetEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_RidingTrain ) )
		{
			return true;
		}
	}

	//Check if the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_MELEE))
	{
		//Check if the target has been unreachable for a while.
		static dev_float s_fMinTime = 10.0f;
		if((pSubTask->GetState() == CTaskMelee::State_WaitForTarget || pSubTask->GetState() == CTaskMelee::State_InactiveObserverIdle) && 
		   (pSubTask->GetTimeInState() >= s_fMinTime))
		{
			return true;
		}
	}

	return false;
}

bool CTaskCombat::ShouldFinishTaskToSearchForTarget() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the order is valid.
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return false;
	}
	
	//Ensure the order type is valid.
	switch(pOrder->GetType())
	{
		case COrder::OT_Police:
		case COrder::OT_Swat:
		{
			//Police and swat orders have custom search behaviors.
			break;
		}
		default:
		{
			return false;
		}
	}
	
	return true;
}

CTaskCombat::ReasonToHoldFire CTaskCombat::ShouldHoldFire() const
{
	//Check if the target is valid.
	if(m_pPrimaryTarget)
	{
		//Check if we should hold fire due to the target being injured.
		if(ShouldHoldFireDueToTargetInjured())
		{
			return RTHF_TargetInjured;
		}

		//Check if we should hold fire due to the target being arrested.
		if(ShouldHoldFireDueToTargetArrested())
		{
			return RTHF_TargetArrested;
		}

		//Check if we should hold fire due to MP.
		if(ShouldHoldFireDueToMP())
		{
			return RTHF_MP;
		}

		//Check if we should hold fire due to limited combatants.
		TUNE_GROUP_BOOL(COMBAT, bCanHoldFireDueToLimitCombatants, false);
		if(bCanHoldFireDueToLimitCombatants && ShouldHoldFireDueToLimitCombatants())
		{
			return RTHF_LimitCombatants;
		}

		//Check if we should hold fire due to lack of hostility.
		if(ShouldHoldFireDueToLackOfHostility())
		{
			return RTHF_LackOfHostility;
		}

		//Check if we should hold fire due to an unarmed target.
		if(ShouldHoldFireDueToUnarmedTarget())
		{
			return RTHF_UnarmedTarget;
		}

		// Check if we are close to the target vehicle we are approaching
		if(ShouldHoldFireDueToApproachingTargetVehicle())
		{
			return RTHF_ApproachingTargetVehicle;
		}

		//Check if we set a hold fire time.
		if(m_uTimeToStopHoldingFire > fwTimer::GetTimeInMilliseconds())
		{
			return RTHF_ForcedHoldFireTimer;
		}

		// Check if there is any reason at the beginning of the task to delay firing
		if(ShouldHoldFireAtTaskInitialization())
		{
			return RTHF_InitialTaskDelay;
		}
	}

	return RTHF_None;
}

bool CTaskCombat::ShouldHoldFireDueToLackOfHostility() const
{
	//This is disabled in MP (due to cops not arresting).
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not a mission ped.
	if(pPed->PopTypeIsMission())
	{
		return false;
	}

	//Ensure the ped is law enforcement.
	if(!pPed->IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure the target is a player.
	if(!m_pPrimaryTarget->IsPlayer())
	{
		return false;
	}

	//Ensure the target is not in water.
	if(m_pPrimaryTarget->GetIsInWater())
	{
		return false;
	}

	//Ensure the wanted is valid.
	const CWanted* pWanted = m_pPrimaryTarget->GetPlayerWanted();
	if(!pWanted)
	{
		return false;
	}

	//Ensure the target is wanted.
	eWantedLevel nWantedLevel = pWanted->GetWantedLevel();
	if(nWantedLevel <= WANTED_CLEAN)
	{
		return false;
	}

	//Determine the minimum time since the last hostile action.
	Tunables::LackOfHostility::WantedLevel* pWantedLevel = NULL;
	switch(nWantedLevel)
	{
		case WANTED_LEVEL1:
		{
			pWantedLevel = &ms_Tunables.m_LackOfHostility.m_WantedLevel1;
			break;
		}
		case WANTED_LEVEL2:
		{
			pWantedLevel = &ms_Tunables.m_LackOfHostility.m_WantedLevel2;
			break;
		}
		case WANTED_LEVEL3:
		{
			pWantedLevel = &ms_Tunables.m_LackOfHostility.m_WantedLevel3;
			break;
		}
		case WANTED_LEVEL4:
		{
			pWantedLevel = &ms_Tunables.m_LackOfHostility.m_WantedLevel4;
			break;
		}
		case WANTED_LEVEL5:
		{
			pWantedLevel = &ms_Tunables.m_LackOfHostility.m_WantedLevel5;
			break;
		}
		default:
		{
			return false;
		}
	}

	//Ensure the wanted level is enabled.
	if(!pWantedLevel->m_Enabled)
	{
		return false;
	}

	if(nWantedLevel > WANTED_LEVEL1)
	{
		//Ensure the minimum time has been exceeded.
		float fTimeSinceLastHostileAction = pWanted->m_fTimeSinceLastHostileAction;
		if(fTimeSinceLastHostileAction < pWantedLevel->m_MinTimeSinceLastHostileAction)
		{
			return false;
		}

		//Ensure the target is not running.
		if(!m_pPrimaryTarget->GetMotionData()->GetIsLessThanRun())
		{
			return false;
		}

		//Check if the target is in a vehicle.
		const CVehicle* pVehicle = m_pPrimaryTarget->GetVehiclePedInside();
		if(pVehicle)
		{
			//Ensure the vehicle is not moving quickly.
			float fSpeedSqr = pVehicle->GetVelocity().XYMag2();
			if(fSpeedSqr > ms_Tunables.m_LackOfHostility.m_MaxSpeedForVehicle * ms_Tunables.m_LackOfHostility.m_MaxSpeedForVehicle)
			{
				return false;
			}
		}
	}

	return true;
}

bool CTaskCombat::ShouldHoldFireDueToLimitCombatants() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	// Don't hold fire for melee combatants
	if(GetState() == State_MeleeAttack)
	{
		return false;
	}

	//Ensure the ped is not a mission ped.
	if(pPed->PopTypeIsMission())
	{
		return false;
	}

	//Ensure we do not have an order
	COrder* pPedOrder = pPed->GetPedIntelligence()->GetOrder();
	if(pPedOrder && pPedOrder->GetIsValid())
	{
		return false;
	}

	//Ensure we are not firing at the target.  Once we start firing, we are not going to stop.
	if(m_uFlags.IsFlagSet(ComF_FiringAtTarget))
	{
		return false;
	}	

	//Ensure the ped wants to limit combatants.
	if(!pPed->CheckBraveryFlags(BF_LIMIT_COMBATANTS))
	{
		return false;
	}

	//Ensure this is not a network game.
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Ensure the current event type is not directly threatening.
	int iEventType = pPed->GetPedIntelligence()->GetCurrentEventType();
	if(IsEventTypeDirectlyThreatening(iEventType))
	{
		return false;
	}

	//Ensure the number of combatants has exceeded the limit.
	static const int s_iMaxCombatants = 5;
	if(m_pPrimaryTarget->GetPedIntelligence()->GetNumPedsFiringAt() < s_iMaxCombatants)
	{
		return false;
	}

	//Ensure the ped has not been damaged by the target.
	if(pPed->HasBeenDamagedByEntity(const_cast<CPed*>(m_pPrimaryTarget.Get())))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::ShouldHoldFireDueToMP() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	// Special reasons why we'd stop shooting in a network game if we are a COP/SWAT
	if(pPed->IsLawEnforcementPed() && NetworkInterface::IsGameInProgress())
	{
		CQueriableInterface* pTargetQueriableInterface = m_pPrimaryTarget->GetPedIntelligence()->GetQueriableInterface();

		// Check if our target is running the nm shot task
		if (pTargetQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_SHOT))
		{
			// Check the weapon hash from the task
			const CClonedNMShotInfo* pTaskInfo =  static_cast<CClonedNMShotInfo*>(m_pPrimaryTarget->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_NM_SHOT, PED_TASK_PRIORITY_MAX, false));
			if (pTaskInfo && pTaskInfo->GetWeaponHash() != 0)
			{
				// Get the weapon info from the hash and see if it is rubber of electric bullet type
				const CWeaponInfo* pTaskWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pTaskInfo->GetWeaponHash());						
				if (pTaskWeaponInfo && (pTaskWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER || pTaskWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC))
				{
					return true;
				}
			}
		}

		// Check if the target is in custody or is being cuffed
		if( pTargetQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_CUSTODY) ||
			pTargetQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_CUFFED) ||
			m_pPrimaryTarget->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody) )
		{
			return true;
		}
	}

	return false;
}

bool CTaskCombat::ShouldHoldFireDueToTargetArrested() const
{
	//Check if the target is arrested.
	if(m_pPrimaryTarget->GetIsArrested())
	{
		return true;
	}

	//Check if the target is in the process of being arrested (don't need to check the busted task as that sets everybody back off)
	CTaskExitVehicleSeat* pExitVehicleTask = static_cast<CTaskExitVehicleSeat* >(m_pPrimaryTarget->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
	if(pExitVehicleTask && pExitVehicleTask->IsBeingArrested())
	{
		return true;
	}

	CCombatOrders* pCombatOrders = GetPed()->GetPedIntelligence()->GetCombatOrders();
	if(pCombatOrders && pCombatOrders->GetPassiveFlags().IsFlagSet(CCombatOrders::PF_HoldFire))
	{
		return true;
	}

	return false;
}

bool CTaskCombat::ShouldHoldFireDueToTargetInjured() const
{
	//Ensure the target is injured.
	if(!m_pPrimaryTarget->IsInjured())
	{
		return false;
	}

	//Ensure we will not attack injured peds.
	if(GetPed()->WillAttackInjuredPeds())
	{
		return false;
	}

	return true;
}

bool CTaskCombat::ShouldHoldFireDueToUnarmedTarget() const
{
	//Ensure the target is unarmed.
	if(m_pPrimaryTarget->GetWeaponManager() && m_pPrimaryTarget->GetWeaponManager()->GetIsArmed())
	{
		return false;
	}

	//Ensure the time spent in combat has not exceeded the threshold.
	static float s_fMaxTime = 2.0f;
	if(GetTimeRunning() >= s_fMaxTime)
	{
		return false;
	}

	return true;
}

bool CTaskCombat::ShouldHoldFireDueToApproachingTargetVehicle() const
{
	if(GetState() != State_GoToEntryPoint && GetState() != State_ArrestTarget)
	{
		return false;
	}

	ScalarV scDistSq = DistSquared(m_pPrimaryTarget->GetTransform().GetPosition(), GetPed()->GetTransform().GetPosition());
	ScalarV scMinDistSq = ScalarVFromF32(ms_Tunables.m_ApproachingTargetVehicleHoldFireDistance);
	scMinDistSq = (scMinDistSq * scMinDistSq);
	if(IsGreaterThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::ShouldHoldFireAtTaskInitialization() const
{
	if(GetTimeRunning() > ms_Tunables.m_MaxTimeToHoldFireAtTaskInitialization)
	{
		return false;
	}

	const CPed* pPed = GetPed();
	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}

	if(pPed->ShouldBehaveLikeLaw())
	{
		return false;
	}

	if(pPed->IsGangPed() && !pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return true;
	}
	
	const CEvent* pCurrentEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
	if(pCurrentEvent && pCurrentEvent->GetEventType() == EVENT_VEHICLE_DAMAGE_WEAPON)
	{
		return true;
	}

	return false;
}

bool CTaskCombat::ShouldNeverChangeStateUnderAnyCircumstances() const
{
	//Check if we are climbing.
	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
	{
		return true;
	}

	//Check if we have a ladder task.
	if(GetPed()->GetPedIntelligence()->GetTaskClimbLadder())
	{
		return true;
	}

	return false;
}

bool CTaskCombat::ShouldPlayAimIntro(CombatState eDesiredNextCombatState, bool bWantToArrestTarget) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure we are not doing an instant blend to aim.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim) || pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimFromScript))
	{
		return false;
	}

	// Force disabled
	if( m_uFlags.IsFlagSet( ComF_DisableAimIntro ) )
	{
		return false;
	}

	//Ensure the ped does not have a cover point.
	if(pPed->GetCoverPoint() != NULL)
	{
		return false;
	}

	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the equipped weapon info is valid.
	const CWeaponInfo* pEquippedWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pEquippedWeaponInfo)
	{
		return false;
	}

	//Ensure the weapon is a gun.
	if(!pEquippedWeaponInfo->GetIsGun())
	{
		return false;
	}

	//Ensure the weapon has an appropriate animation
	if (pEquippedWeaponInfo->GetHash() == WEAPONTYPE_MINIGUN)
	{
		return false;
	}

	//Ensure the clip set is valid.
	fwMvClipSetId clipSetId = pEquippedWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	//Ensure the ped is not in a vehicle.
	if(pPed->GetIsInVehicle())
	{
		return false;
	}

	//Ensure the ped is not entering a vehicle.
	if(pPed->GetIsEnteringVehicle())
	{
		return false;
	}

	//Ensure the ped is not crouching.
	if(pPed->GetIsCrouching())
	{
		return false;
	}

	//Check if the motion task is valid.
	const CTaskMotionPed* pMotionTask = static_cast<CTaskMotionPed *>(pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_MOTION, CTaskTypes::TASK_MOTION_PED));
	if(pMotionTask)
	{
		//Ensure the motion task is not aiming.
		if(pMotionTask->IsAiming())
		{
			return false;
		}
	}

	const ScalarV scTargetDistSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());

	// Check min distance if want to arrest target
	ScalarV scMinDistSq = LoadScalar32IntoScalarV(ms_Tunables.m_MinDistanceForAimIntro);
	scMinDistSq = (scMinDistSq * scMinDistSq);
	if( bWantToArrestTarget && IsLessThanAll(scTargetDistSq, scMinDistSq) )
	{
		return false;
	}

	// Check min absolute distance
	if (IsLessThanOrEqualAll(scTargetDistSq, ScalarV(V_ONE)))
	{
		return false;
	}

	//If the ped is already moving, we skip the aim intro if he has not LOS and the next state is a movement one.
	//NOTE[egarcia]: We do not want to interrupt the movement without a good reason.
	Vector2 vCurrentMbr;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMbr);
	if(!vCurrentMbr.IsZero())
	{
		//NOTE[egarcia]: At the moment, we are allowing aim intro to be skipped just for "move within attack window" state that
		//usually corresponds with the case in which we are going into combat from a far away.
		const bool bGoingToMovementState = (eDesiredNextCombatState == State_MoveWithinAttackWindow);
		if (bGoingToMovementState && IsAimToTargetBlocked(m_pPrimaryTarget))
		{
			return false;
		}
	}

	return true;
}

// PURPOSE:	If we don't have recent or current LOS and we are far enough, we consider the aim direction is blocked.
bool CTaskCombat::IsAimToTargetBlocked(const CPed* pTargetPed) const
{
	const CPed* pPed = GetPed();
	taskAssert(pPed);

	if (!pTargetPed)
	{
		return false;
	}

	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( false );
	if (!pPedTargetting)
	{
		return false;
	}

	const int iTargetIndex = pPedTargetting->FindTargetIndex(pTargetPed);
	if (iTargetIndex < 0)
	{
		return false;
	}

	const CSingleTargetInfo* pTargetInfo = pPedTargetting->GetTargetInfoAtIndex(iTargetIndex);
	if (!taskVerifyf(pTargetInfo, "CTaskCombat::IsAimToTargetBlocked, pTargetInfo==NULL for valid iTargetIndex [%d]", iTargetIndex))
	{
		return false;
	}

	// If we don't have recent or current LOS and we are far enough, we consider the aim direction is blocked.
	const bool bLOSBlockedForLongEnough = !pTargetInfo->GetHasEverHadClearLos() || pTargetInfo->GetTimeLosBlocked() >= ms_Tunables.m_MinBlockedLOSTimeToBlockAimIntro;
	if (bLOSBlockedForLongEnough && (pPedTargetting->GetLosStatusAtIndex(iTargetIndex, true) == Los_blocked))
	{
		const ScalarV scTargetDistSqr = DistSquared(pPed->GetTransform().GetPosition(), pTargetPed->GetTransform().GetPosition());
		return IsGreaterThanAll(scTargetDistSqr, rage::square(ScalarV(ms_Tunables.m_MinDistanceToBlockAimIntro))) != 0;
	}

	return false;
}

bool CTaskCombat::ShouldPlayRoadBlockAudio() const
{
	//Ensure we are waiting at a road block.
	if(!m_uFlags.IsFlagSet(ComF_IsWaitingAtRoadBlock))
	{
		return false;
	}

	//Ensure we have not played road block audio.
	if(m_uRunningFlags.IsFlagSet(RF_HasPlayedRoadBlockAudio))
	{
		return false;
	}

	//Ensure the primary target is valid.
	if(!m_pPrimaryTarget)
	{
		return false;
	}

	//Ensure the primary target is in a vehicle.
	if(!m_pPrimaryTarget->GetIsInVehicle())
	{
		return false;
	}

	//Ensure the primary target is moving towards us.
	Vec3V vVelocity = VECTOR3_TO_VEC3V(m_pPrimaryTarget->GetVehiclePedInside()->GetVelocity());
	Vec3V vTargetToUs = Subtract(GetPed()->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	ScalarV scDot = Dot(vVelocity, vTargetToUs);
	if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}

	//Ensure the primary target is close enough.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	static dev_float s_fMaxDistance = 30.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::ShouldUseFranticRuns() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure we are not strafing.
	if(pPed->GetMotionData()->GetIsStrafing())
	{
		return false;
	}

	//Ensure we are running.
	if(!pPed->GetMotionData()->GetIsRunning())
	{
		return false;
	}

	//Check if we are using frantic runs.
	if(m_uFlags.IsFlagSet(ComF_IsUsingFranticRuns))
	{
		return true;
	}

	//Check if we are moving.
	const CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(
		pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask && pTask->HasTarget())
	{
		//Generate vectors from the ped to the targets.
		Vec3V vPedToMoveTarget		= Subtract(VECTOR3_TO_VEC3V(pTask->GetTarget()),			pPed->GetTransform().GetPosition());
		Vec3V vPedToCombatTarget	= Subtract(m_pPrimaryTarget->GetTransform().GetPosition(),	pPed->GetTransform().GetPosition());

		//Normalize the vectors.
		Vec3V vPedToMoveTargetDirection		= NormalizeFastSafe(vPedToMoveTarget,	Vec3V(V_ZERO));
		Vec3V vPedToCombatTargetDirection	= NormalizeFastSafe(vPedToCombatTarget,	Vec3V(V_ZERO));

		//Check if we are not moving towards the (combat) target.
		ScalarV scDot = Dot(vPedToMoveTargetDirection, vPedToCombatTargetDirection);
		ScalarV scMaxDot = ScalarV(0.707f);
		if(IsLessThanAll(scDot, scMaxDot))
		{
			return true;
		}
	}

	return false;
}

bool CTaskCombat::ShouldWarpIntoVehicleIfPossible() const
{
	//Ensure the ped is random.
	if(!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is law-enforcement.
	if(!GetPed()->IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure the ped is not visible.
	if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		return false;
	}

	//Ensure the target is a player.
	if(!m_pPrimaryTarget->IsPlayer())
	{
		return false;
	}

	//Ensure the target is wanted.
	if(m_pPrimaryTarget->GetPlayerWanted()->GetWantedLevel() <= WANTED_CLEAN)
	{
		return false;
	}

	//Ensure the state is valid.
	if(GetState() != State_EnterVehicle)
	{
		return false;
	}

	//Ensure the vehicle is valid.
	if(!m_pVehicleToUseForChase)
	{
		return false;
	}

	//Ensure the vehicle is not on screen.
	if(m_pVehicleToUseForChase->GetIsOnScreen(true))
	{
		return false;
	}

	//Ensure the distance is valid.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition());
	static dev_float s_fMinDistance = 15.0f;
	ScalarV scMinDistSq = ScalarVFromF32(square(s_fMinDistance));
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskCombat::ShouldReturnToInitialPosition() const
{
	const CPed* pPed = GetPed();
	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseDefaultBlockedLosPositionAndDirection))
	{
		return false;
	}

	// make sure we are trying to return to a valid position
	if(IsCloseAll(m_vInitialPosition, Vec3V(V_ZERO), ScalarV(SMALL_FLOAT)))
	{
		return false;
	}

	const CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
	if(!pPedTargeting)
	{
		return false;
	}

	const CSingleTargetInfo* pTargetInfo = pPedTargeting->FindTarget(m_pPrimaryTarget);
	if(!pTargetInfo)
	{
		return false;
	}

	if(pTargetInfo->GetTimeLosBlocked() < ms_Tunables.m_fTimeLosBlockedForReturnToInitialPosition)
	{
		return false;
	}

	return true;
}

bool CTaskCombat::StateCanChange() const
{
	//Ensure we are not reacting to an explosion.
	int nState = GetState();
	if( nState == State_ReactToExplosion ||
		nState == State_GetOutOfWater )
	{
		return false;
	}

	if(nState == State_Fire)
	{
		if(m_uFlags.IsFlagSet(ComF_TemporarilyForceFiringState))
		{
			if(GetTimeInState() < GetPed()->GetRandomNumberInRangeFromSeed(ms_Tunables.m_MinForceFiringStateTime, ms_Tunables.m_MaxForceFiringStateTime))
			{
				return false;
			}
		}

		if( GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsBeingMeleeHomedByPlayer) ||
			(m_uFlags.IsFlagSet(ComF_TaskAbortedForStaticMovement) && GetTimeInState() < ms_Tunables.m_FireTimeAfterStaticMovementAbort) )
		{
			return false;
		}

		if( GetPreviousState() == State_ChaseOnFoot && GetTimeInState() <= (ms_Tunables.m_FireTimeAfterChaseOnFoot * GetPed()->GetRandomNumberInRangeFromSeed(.8f, 1.2f)) )
		{
			return false;
		}
	}
	
	// First check for any CTaskGun so we can see if it's doing a bullet reaction
	if(GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GUN) && 
		GetPed()->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_GUN) == CTaskGun::State_BulletReaction)
	{
		return false;
	}

	// Check if we are running an animation in this task
	if(GetClipHelper())
	{
		return false;
	}
	
	//Check if we are playing an aim pose transition.
	if(IsPlayingAimPoseTransition())
	{
		return false;
	}
	
	return true;
}

bool CTaskCombat::StateHandlesBlockedLineOfSight() const
{
	//Get the sub-task.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check if the sub-task is varied aim pose.
		if(pSubTask->GetTaskType() == CTaskTypes::TASK_VARIED_AIM_POSE)
		{
			//Check if the varied aim pose has not failed to fix the line of sight.
			const CTaskVariedAimPose* pTask = static_cast<const CTaskVariedAimPose *>(pSubTask);
			if(!pTask->FailedToFixLineOfSight())
			{
				return true;
			}
		}
	}
	
	return false;
}

void CTaskCombat::UpdateFlagsForNearbyPeds(s64& iConditionFlags)
{
	//Clear the injured ped to help.
	m_pInjuredPedToHelp = NULL;

	//Enforce restrictions on dragging injured peds.
	if(!AllowDraggingTask())
	{
		return;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped can help.
	if(!CanHelpAnyPed())
	{
		return;
	}

	//Check if any nearby peds need help.
	static const int s_iMaxPedsToCheck = 4;
	int iNumPedsChecked = 0;
	CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		//Help the other ped, if possible.
		CPed* pOtherPed = static_cast<CPed *>(pEnt);
		if(DoesPedNeedHelp(pOtherPed) && CanHelpSpecificPed(pOtherPed, m_pPrimaryTarget))
		{
			//Set the injured ped to help.
			m_pInjuredPedToHelp = pOtherPed;

			//Set the help flag.
			iConditionFlags |= CF_CanDragInjuredPedToSafety;

			break;
		}

		//Enforce the max peds to check.
		++iNumPedsChecked;
		if(iNumPedsChecked >= s_iMaxPedsToCheck)
		{
			break;
		}
	}
}

void CTaskCombat::UpdateOrder()
{
	//Ensure the order is valid.
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return;
	}

	//Check the type.
	switch(pOrder->GetType())
	{
		case COrder::OT_Police:
		{
			static_cast<CPoliceOrder *>(pOrder)->SetPoliceOrderType(CPoliceOrder::PO_WANTED_COMBAT);
			break;
		}
		case COrder::OT_Swat:
		{
			static_cast<CSwatOrder *>(pOrder)->SetSwatOrderType(CSwatOrder::SO_WANTED_COMBAT);
			break;
		}
		default:
		{
			break;
		}
	}
}

bool CTaskCombat::WillNeedToReactToImminentExplosionSoon() const
{
	//Ensure the potential blast is active.
	if(!m_PotentialBlast.IsValid())
	{
		return false;
	}

	//Ensure the potential blast is coming soon.
	static u32 s_uLookAhead = 5000;
	if((fwTimer::GetTimeInMilliseconds() + s_uLookAhead) < m_PotentialBlast.m_uTimeOfExplosion)
	{
		return false;
	}

	return true;
}

bool CTaskCombat::AllowDraggingTask()
{
	//Ensure a network game is not in progress.
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}
	
	//Check if a drag has been active.
	if(ms_uTimeLastDragWasActive > 0)
	{
		//Ensure the min time between drags has elapsed.
		if((ms_uTimeLastDragWasActive + (u32)(ms_Tunables.m_fTimeBetweenDragsMin * 1000.0f)) > fwTimer::GetTimeInMilliseconds())
		{
			return false;
		}
	}
	
	return true;
}

bool CTaskCombat::CanVaryAimPose(CPed* pPed)
{
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_Stationary)
	{
		return false;
	}

	return !pPed->HasHurtStarted();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCombat::InitClass()
{
	ms_moveToCoverTimerGlobal.SetTime(ms_Tunables.m_fMinAttemptMoveToCoverDelayGlobal, ms_Tunables.m_fMaxAttemptMoveToCoverDelayGlobal);
	ms_allowFrustrationTimerGlobal.SetTime(ms_Tunables.m_fMinTimeBetweenFrustratedPeds, ms_Tunables.m_fMaxTimeBetweenFrustratedPeds);

	InitTransitionTables();
}

void CTaskCombat::InitTransitionTables()
{
	for (s32 i=0; i<CCombatBehaviour::CT_NumTypes; i++)
	{
		ms_combatTransitionTables[i]->Clear();
		ms_combatTransitionTables[i]->Init();
	}
}

////////////////////////////////////////////////////////////////////////////////

COnFootUnarmedTransitions::COnFootUnarmedTransitions()
{

}

////////////////////////////////////////////////////////////////////////////////

void COnFootUnarmedTransitions::Init()
{
	// Transitions from start
	AddTransition(CTaskCombat::State_Start,				CTaskCombat::State_MeleeAttack,				1.0f,	CF_OnFoot|CF_TargetWithinDefensiveArea|CF_CanPerformMeleeAttack);
	AddTransition(CTaskCombat::State_Start,				CTaskCombat::State_PersueInVehicle,			1.0f,	CF_InVehicle					);
	AddTransition(CTaskCombat::State_Start,				CTaskCombat::State_Mounted,					1.0f,	CF_Mounted						);
	AddTransition(CTaskCombat::State_Start,				CTaskCombat::State_MoveWithinDefensiveArea,	1.0f,	CF_OnFoot|CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea		);

	// Transitions from the melee task
	AddTransition(CTaskCombat::State_MeleeAttack,		CTaskCombat::State_WaitingForEntryPointToBeClear,		1.0f,	CF_CanJackTarget|CF_WillAdvance|CF_TargetWithinDefensiveArea	);
	AddTransition(CTaskCombat::State_MeleeAttack,		CTaskCombat::State_MoveWithinDefensiveArea,	1.0f,	CF_OnFoot|CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea		);
	AddTransition(CTaskCombat::State_MeleeAttack,		CTaskCombat::State_ReactToExplosion,		1.0f,	CF_CanReactToExplosion		);
	AddTransition(CTaskCombat::State_MeleeAttack,		CTaskCombat::State_Search,					1.0f,	CF_TargetLocationLost			);

	// Finish moving back inside the defensive area
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_Start, 1.0f, CF_StateEnded	);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_ReactToExplosion,	1.0f,	CF_CanReactToExplosion	);

	// Transitions from advancing back into melee when Los is re-established
	AddTransition(CTaskCombat::State_Advance,			CTaskCombat::State_MeleeAttack,				1.0f,	CF_LosClear|CF_CanPerformMeleeAttack);
	AddTransition(CTaskCombat::State_Advance,			CTaskCombat::State_Search,					1.0f,	CF_TargetLocationLost			);
	AddTransition(CTaskCombat::State_Search,			CTaskCombat::State_MeleeAttack,				1.0f,	CF_TargetLocationKnown|CF_CanPerformMeleeAttack);

	// Straight to firing after exiting a vehicle
	AddTransition(CTaskCombat::State_PersueInVehicle,	CTaskCombat::State_MeleeAttack,				1.0f,	CF_OnFoot|CF_StateEnded|CF_CanPerformMeleeAttack);

	// Transition from mounted
	AddTransition(CTaskCombat::State_Mounted,			CTaskCombat::State_Start,					1.0f,	CF_OnFoot|CF_StateEnded			);
	
	// Transition out after reaching shore
	AddTransition(CTaskCombat::State_GetOutOfWater,	CTaskCombat::State_Start,	1.0f,	CF_StateEnded);
}

////////////////////////////////////////////////////////////////////////////////

COnFootArmedTransitions::COnFootArmedTransitions()
{

}

////////////////////////////////////////////////////////////////////////////////

void COnFootArmedTransitions::Init()
{
	// Initial transitions - straight into firing if on foot, or pursue in vehicle if in one.
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_MoveToCover,				9.0f, CF_OnFoot|CF_WillUseCover|CF_HasDesiredCover	);	// CC: Changed these (9.0, 1.0 from 0.9, 0.1) values to 
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_Fire,					1.0f, CF_OnFoot|CF_LosClear							);	// make sure we transition from start state
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_PersueInVehicle,			1.0f, CF_InVehicle							);
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_Mounted,					1.0f, CF_Mounted							);
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_WaitingForEntryPointToBeClear,		2.0f, CF_OnFoot|CF_CanJackTarget|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_ArrestTarget,			1.0f, CF_CanArrestTarget);
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_MoveWithinDefensiveArea,	1.0f, CF_OnFoot|CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea				);
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_MoveWithinAttackWindow,	1.0f, CF_OnFoot|CF_OutsideAttackWindow|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_DragInjuredToSafety,		0.2f, CF_CanDragInjuredPedToSafety			);
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_MeleeAttack,				100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);
	AddFallback	 (CTaskCombat::State_Start,		CTaskCombat::State_Fire);

	// Transitions out of firing due to state changes
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_ChaseOnFoot,				1.0f, CF_CanChaseOnFoot|CF_TargetLocationKnown	 );	
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_Advance,					1.0f, CF_LosBlocked|CF_LosNotBlockedByFriendly|CF_WillAdvance);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_AdvanceFrustrated,		1.0f, CF_OnFoot|CF_WillUseFrustratedAdvance|CF_WillAdvance		);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle					);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_DecideTargetLossResponse,1.0f, CF_TargetLocationLost|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MoveWithinDefensiveArea,	1.0f, CF_OnFoot|CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea	);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MoveWithinAttackWindow,	1.0f, CF_OnFoot|CF_OutsideAttackWindow|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MoveWithinAttackWindow,	1.0f, CF_OnFoot|CF_WillUseDefensiveAttackWindow	);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_DragInjuredToSafety,		0.2f, CF_CanDragInjuredPedToSafety			);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_ReactToImminentExplosion,9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MeleeAttack,				100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_ReactToExplosion,		1.0f, CF_CanReactToExplosion				);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_ArrestTarget,			1.0f, CF_CanArrestTarget);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_ReactToBuddyShot,		1.0f, CF_CanReactToBuddyShot);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MoveToTacticalPoint,		1.0f, CF_CanMoveToTacticalPoint);

	// Occasional transitions
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_Flank,					0.3f, CF_LosBlocked|CF_LosNotBlockedByFriendly|CF_WillAdvance|CF_TargetLocationKnown|CF_CanFlank	);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MoveToCover,				1.0f, CF_InsideAttackWindow|CF_WillUseCover|CF_NotInCover|CF_HasDesiredCover	);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_WaitingForEntryPointToBeClear,		1.0f, CF_CanJackTarget|CF_WillAdvance		);

	// Transitions out of seeking cover on task completion
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_ChaseOnFoot,				1.0f, CF_CanChaseOnFoot|CF_TargetLocationKnown	 );	
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle				);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_InCover,					1.0f, CF_InCover|CF_StateEnded			);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_Fire,					1.0f, CF_NotInCover|CF_StateEnded		);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_MoveWithinDefensiveArea,	1.0f, CF_NotInCover|CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea	);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_DragInjuredToSafety,		0.2f, CF_CanDragInjuredPedToSafety		);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_ReactToImminentExplosion,9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_ReactToExplosion,		1.0f, CF_CanReactToExplosion			);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_ReactToBuddyShot,		1.0f, CF_CanReactToBuddyShot);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_WaitingForEntryPointToBeClear,	1.0f, CF_CanJackTarget|CF_WillAdvance		);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_ArrestTarget,			1.0f, CF_CanArrestTarget);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_MeleeAttack,				100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);

	// Transitions out of cover due to state changes
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_ChaseOnFoot,				1.0f, CF_CanChaseOnFoot|CF_TargetLocationKnown		);	
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_Advance,					1.0f, CF_LosBlocked|CF_LosNotBlockedByFriendly|CF_WillAdvance);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_AdvanceFrustrated,		1.0f, CF_OnFoot|CF_WillUseFrustratedAdvance|CF_WillAdvance);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle							);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveWithinDefensiveArea,	1.0f, CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea						);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveToCover,				0.9f, CF_InsideAttackWindow|CF_StateEnded|CF_WillUseCover|CF_NotInCover|CF_HasDesiredCover	);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveToCover,				0.9f, CF_InsideAttackWindow|CF_WillUseCover|CF_HasDesiredCover|CF_IsSafeToLeaveCover	);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_Fire,					0.1f, CF_StateEnded									);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveWithinAttackWindow,	1.0f, CF_OnFoot|CF_OutsideAttackWindow|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveWithinAttackWindow,	1.0f, CF_OnFoot|CF_WillUseDefensiveAttackWindow	);
	// Occasional transitions
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_Flank,					0.3f, CF_LosBlocked|CF_LosNotBlockedByFriendly|CF_WillAdvance|CF_TargetLocationKnown|CF_CanFlank	);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_WaitingForEntryPointToBeClear,		1.0f, CF_CanJackTarget|CF_WillAdvance		);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_DragInjuredToSafety,		0.2f, CF_CanDragInjuredPedToSafety|CF_IsSafeToLeaveCover);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_ReactToImminentExplosion,		9.0f, CF_OnFoot|CF_CanReactToImminentExplosion			);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_ReactToExplosion,		1.0f, CF_CanReactToExplosion				);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_ArrestTarget,			1.0f, CF_CanArrestTarget);

	// Finish advancing or flanking
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle				);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_Fire,					1.0f, CF_NotInCover|CF_LosClear			);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_InCover,					1.0f, CF_InCover|CF_LosClear			);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_DecideTargetLossResponse,1.0f, CF_TargetLocationLost				);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_TargetUnreachable,		1.0f, CF_StateEnded						);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_WaitingForEntryPointToBeClear,		1.0f, CF_CanJackTarget|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_ReactToImminentExplosion,9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_ReactToExplosion,		1.0f, CF_CanReactToExplosion			);
	AddTransition(CTaskCombat::State_Advance,	CTaskCombat::State_MeleeAttack,				100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);

	AddTransition(CTaskCombat::State_Flank,		CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle				);
	AddTransition(CTaskCombat::State_Flank,		CTaskCombat::State_Fire,					1.0f, CF_StateEnded|CF_LosClear			);
	AddTransition(CTaskCombat::State_Flank,		CTaskCombat::State_Advance,					1.0f, CF_StateEnded|CF_LosBlocked	);
	AddTransition(CTaskCombat::State_Flank,		CTaskCombat::State_WaitingForEntryPointToBeClear,		1.0f, CF_CanJackTarget|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_Flank,		CTaskCombat::State_ReactToImminentExplosion,9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_Flank,		CTaskCombat::State_ReactToExplosion,		1.0f, CF_CanReactToExplosion			);
	AddTransition(CTaskCombat::State_Flank,		CTaskCombat::State_MeleeAttack,				100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);

	// Finish being frustrated
	AddTransition(CTaskCombat::State_AdvanceFrustrated,	CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle				);
	AddTransition(CTaskCombat::State_AdvanceFrustrated,	CTaskCombat::State_Fire,					1.0f, CF_LosClear						);
	AddTransition(CTaskCombat::State_AdvanceFrustrated,	CTaskCombat::State_TargetUnreachable,		1.0f, CF_StateEnded						);
	AddTransition(CTaskCombat::State_AdvanceFrustrated,	CTaskCombat::State_WaitingForEntryPointToBeClear,	1.0f, CF_CanJackTarget|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_AdvanceFrustrated,	CTaskCombat::State_ReactToImminentExplosion,9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_AdvanceFrustrated,	CTaskCombat::State_ReactToExplosion,		1.0f, CF_CanReactToExplosion			);

	// Finish moving back inside the desired attack window
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_Advance,			1.0f, CF_LosBlocked|CF_LosNotBlockedByFriendly|CF_WillAdvance);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_Flank,			0.3f, CF_LosBlocked|CF_LosNotBlockedByFriendly|CF_WillAdvance|CF_TargetLocationKnown|CF_CanFlank	);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_EnterVehicle,	1.0f, CF_CanChaseInVehicle				);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_InCover,			1.0f, CF_InCover|CF_StateEnded			);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_Fire,			1.0f, CF_NotInCover|CF_StateEnded	);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_ReactToImminentExplosion,	9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_ReactToExplosion,	1.0f, CF_CanReactToExplosion	);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_ReactToBuddyShot,	1.0f, CF_CanReactToBuddyShot);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_ChaseOnFoot,		1.0f, CF_CanChaseOnFoot|CF_TargetLocationKnown		);	
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_WaitingForEntryPointToBeClear,	1.0f, CF_CanJackTarget|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_ArrestTarget,	1.0f, CF_CanArrestTarget);
	AddTransition(CTaskCombat::State_MoveWithinAttackWindow,	CTaskCombat::State_MeleeAttack,		100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);

	// Finish moving back inside the defensive area
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_EnterVehicle, 1.0f, CF_CanChaseInVehicle					);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_InCover,		 1.0f, CF_InCover|CF_StateEnded				);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_Fire,		 1.0f, CF_NotInCover|CF_StateEnded			);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_ReactToImminentExplosion,9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_ReactToExplosion,	1.0f, CF_CanReactToExplosion		);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_MoveToCover,	 1.0f, CF_InsideAttackWindow|CF_WillUseCover|CF_NotInCover|CF_HasDesiredCover	);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_MeleeAttack,	100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);

	// Straight to firing after exiting a vehicle
	AddTransition(CTaskCombat::State_PersueInVehicle,	CTaskCombat::State_Fire,				1.0f, CF_OnFoot|CF_StateEnded				);

	AddTransition(CTaskCombat::State_Search,	CTaskCombat::State_Fire,			1.0f, CF_OnFoot|CF_TargetLocationKnown);
	AddTransition(CTaskCombat::State_Search,	CTaskCombat::State_PersueInVehicle,	1.0f, CF_InVehicle|CF_TargetLocationKnown);
	AddTransition(CTaskCombat::State_Search,	CTaskCombat::State_ReactToImminentExplosion,9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_Search,	CTaskCombat::State_ReactToExplosion,	1.0f, CF_CanReactToExplosion);
	AddTransition(CTaskCombat::State_Search,	CTaskCombat::State_Finish,			1.0f, CF_TargetLocationLost|CF_StateEnded);

	//AddTransition(CTaskCombat::State_ChaseOnFoot,	CTaskCombat::State_MoveToCover,					1.0f, CF_InsideAttackWindow|CF_TargetLocationKnown|CF_DisallowChaseOnFoot|CF_WillUseCover|CF_OnFoot|CF_HasDesiredCover	);
	AddTransition(CTaskCombat::State_ChaseOnFoot,	CTaskCombat::State_Fire,						0.7f, CF_TargetLocationKnown|CF_DisallowChaseOnFoot|CF_OnFoot	);
	AddTransition(CTaskCombat::State_ChaseOnFoot,	CTaskCombat::State_Advance,						1.0f, CF_LosBlocked|CF_LosNotBlockedByFriendly|CF_WillAdvance|CF_DisallowChaseOnFoot);
	AddTransition(CTaskCombat::State_ChaseOnFoot,	CTaskCombat::State_EnterVehicle,				1.0f, CF_TargetLocationKnown|CF_CanChaseInVehicle				);
	AddTransition(CTaskCombat::State_ChaseOnFoot,	CTaskCombat::State_DecideTargetLossResponse,	1.0f, CF_TargetLocationLost);
	AddTransition(CTaskCombat::State_ChaseOnFoot,	CTaskCombat::State_ReactToImminentExplosion,	9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_ChaseOnFoot,	CTaskCombat::State_ReactToExplosion,			1.0f, CF_CanReactToExplosion);

	// Transition from mounted
	AddTransition(CTaskCombat::State_Mounted,		CTaskCombat::State_Start,				1.0f, CF_OnFoot|CF_StateEnded									);
	
	// Transition from reacting to an imminent explosion
	AddTransition(CTaskCombat::State_ReactToImminentExplosion,	CTaskCombat::State_InCover,				1.0f, CF_InCover|CF_StateEnded				);
	AddTransition(CTaskCombat::State_ReactToImminentExplosion,	CTaskCombat::State_Fire,				1.0f, CF_NotInCover|CF_StateEnded			);
	AddTransition(CTaskCombat::State_ReactToImminentExplosion,	CTaskCombat::State_ReactToExplosion,	1.0f, CF_CanReactToExplosion		);
	
	// Transition out of dragging an injured ped to safety.
	AddTransition(CTaskCombat::State_DragInjuredToSafety,	CTaskCombat::State_Fire,	1.0f,	CF_StateEnded);

	// Transition out of reacting to buddy shot.
	AddTransition(CTaskCombat::State_ReactToBuddyShot,	CTaskCombat::State_Fire,	1.0f,	CF_StateEnded);

	// Transition out after reaching shore
	AddTransition(CTaskCombat::State_GetOutOfWater,		CTaskCombat::State_Start,	1.0f,	CF_StateEnded);

	// Transitions from the end of target unreachable go to Fire or Advance based on LOS status
	AddTransition(CTaskCombat::State_TargetUnreachable,	CTaskCombat::State_DecideTargetLossResponse,	1.0f, CF_TargetLocationLost);
	AddTransition(CTaskCombat::State_TargetUnreachable,	CTaskCombat::State_Advance,						1.0f, CF_StateEnded|CF_LosBlocked|CF_LosNotBlockedByFriendly);

	// Transitions from move to tactical point
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_EnterVehicle,					1.0f, CF_CanChaseInVehicle				);
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_Fire,							1.0f, CF_NotInCover|CF_StateEnded		);
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_InCover,							1.0f, CF_InCover|CF_StateEnded			);
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_DecideTargetLossResponse,		1.0f, CF_TargetLocationLost				);
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_WaitingForEntryPointToBeClear,	1.0f, CF_CanJackTarget|CF_WillAdvance	);
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_ReactToImminentExplosion,		9.0f, CF_OnFoot|CF_CanReactToImminentExplosion);
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_ReactToExplosion,				1.0f, CF_CanReactToExplosion			);
	AddTransition(CTaskCombat::State_MoveToTacticalPoint,	CTaskCombat::State_MeleeAttack,						100.0f, CF_OnFoot|CF_CanPerformMeleeAttack);
}

////////////////////////////////////////////////////////////////////////////////

// Armed combat transitions where the BF_JustSeekCover has been set
COnFootArmedCoverOnlyTransitions::COnFootArmedCoverOnlyTransitions()
{
};

////////////////////////////////////////////////////////////////////////////////

void COnFootArmedCoverOnlyTransitions::Init()
{
	// Initital transitions - straight into firing if on foot, or persue in vehicle if in one.
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_MoveToCover,				1.0f, CF_OnFoot|CF_HasDesiredCover				);	
	AddTransition(CTaskCombat::State_Start,		CTaskCombat::State_Fire,					1.0f, CF_OnFoot								);	// make sure we transition from start state

	// Transitions out of seeking cover on task completion
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_ChaseOnFoot,			1.0f, CF_CanChaseOnFoot|CF_TargetLocationKnown	);	
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_EnterVehicle,		1.0f, CF_CanChaseInVehicle						);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_InCover,				1.0f, CF_InCover|CF_StateEnded					);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_Fire,				1.0f, CF_NotInCover|CF_StateEnded				);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_MoveToCover,			1.0f, CF_NotInCover|CF_StateEnded|CF_HasDesiredCover	);
	AddTransition(CTaskCombat::State_MoveToCover,	CTaskCombat::State_DragInjuredToSafety,	0.2f, CF_CanDragInjuredPedToSafety				);

	// Transitions out of cover due to state changes
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_ChaseOnFoot,				1.0f, CF_CanChaseOnFoot|CF_TargetLocationKnown	);	
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle			);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveWithinDefensiveArea,	1.0f, CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea		);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveToCover,				1.0f, CF_StateEnded|CF_NotInCover|CF_HasDesiredCover	);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_MoveToCover,				1.0f, CF_HasDesiredCover|CF_IsSafeToLeaveCover	);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_Fire,					0.1f, CF_StateEnded					);
	AddTransition(CTaskCombat::State_InCover,	CTaskCombat::State_DragInjuredToSafety,		0.2f, CF_CanDragInjuredPedToSafety|CF_IsSafeToLeaveCover);

	// Transitions out of firing due to state changes
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_ChaseOnFoot,				1.0f, CF_CanChaseOnFoot|CF_TargetLocationKnown	);	
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_EnterVehicle,			1.0f, CF_CanChaseInVehicle		);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MoveWithinDefensiveArea,	1.0f, CF_OutsideDefensiveArea|CF_CanMoveWithinDefensiveArea	);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_MoveToCover,				1.0f, CF_NotInCover|CF_HasDesiredCover			);
	AddTransition(CTaskCombat::State_Fire,		CTaskCombat::State_DragInjuredToSafety,		0.2f, CF_CanDragInjuredPedToSafety	);

	// Finish moving back inside the defensive area
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_EnterVehicle, 1.0f, CF_CanChaseInVehicle	);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_MoveToCover,	 1.0f, CF_HasDesiredCover	);
	AddTransition(CTaskCombat::State_MoveWithinDefensiveArea,	CTaskCombat::State_Fire,		 1.0f, CF_NotInCover|CF_StateEnded	);
	
	// Transition out of dragging an injured ped to safety.
	AddTransition(CTaskCombat::State_DragInjuredToSafety,	CTaskCombat::State_Fire,	1.0f,	CF_StateEnded);

	// Transition out after reaching shore
	AddTransition(CTaskCombat::State_GetOutOfWater,		CTaskCombat::State_Start,	1.0f,	CF_StateEnded);
}

////////////////////////////////////////////////////////////////////////////////

CUnderwaterArmedTransitions::CUnderwaterArmedTransitions()
{

}

////////////////////////////////////////////////////////////////////////////////

void CUnderwaterArmedTransitions::Init()
{
	// Transitions from start
	AddTransition( CTaskCombat::State_Start,	CTaskCombat::State_GetOutOfWater, 1.0f, 0 );

	// Transition out after reaching shore
	AddTransition( CTaskCombat::State_GetOutOfWater,	CTaskCombat::State_Start, 1.0f, 0 );
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskCombat::Debug() const
{
#if DEBUG_DRAW
	const CPed *pPed = GetPed();
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	static bool sbDrawDebug = false;

	if (m_pPrimaryTarget)
	{
		Color32 targetLineColor = Color_red;

		if (CCombatDebug::ms_bRenderCombatTargetInfo)
		{
			int iNumTexts = 0;
			Vec3V vTargetPos = m_pPrimaryTarget->GetTransform().GetPosition();

			static char szBuffer[128];

			formatf(szBuffer,"Target name: %s (0x%p)", AILogging::GetDynamicEntityNameSafe(m_pPrimaryTarget), m_pPrimaryTarget.Get());
			grcDebugDraw::Text(vPedPosition, Color_green, 0, 0, szBuffer);
			iNumTexts++;

			formatf(szBuffer,"Target health: %.02f", m_pPrimaryTarget->GetHealth());
			grcDebugDraw::Text(vPedPosition, Color_green, 0, iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), szBuffer);
			iNumTexts++;

			formatf(szBuffer,"Target position (xyz): %.02f, %.02f, %.02f", vTargetPos.GetXf(), vTargetPos.GetYf(), vTargetPos.GetZf());
			grcDebugDraw::Text(vPedPosition, Color_green, 0, iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), szBuffer);
			iNumTexts++;
			
			CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( false );
			if( pPedTargetting )
			{
				CSingleTargetInfo* pTargetInfo = pPedTargetting->FindTarget(m_pPrimaryTarget);
				if(pTargetInfo)
				{
					LosStatus losStatus = pTargetInfo->GetLosStatus();
					if(losStatus == Los_blockedByFriendly || losStatus == Los_blocked)
					{
						grcDebugDraw::Text(vPedPosition, Color_green, 0, iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), "LOS to target: Blocked");
						targetLineColor = Color_red4;
					}
					else if (losStatus == Los_clear)
					{
						grcDebugDraw::Text(vPedPosition, Color_green, 0, iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), "LOS to target: Clear");
					}
					else
					{
						grcDebugDraw::Text(vPedPosition, Color_green, 0, iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), "LOS to target: Unchecked");
						targetLineColor = Color_orange;
					}

					iNumTexts++;
				}
				else
				{
					grcDebugDraw::Text(vPedPosition, Color_green, 0, iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), "Target not in targeting list!");
					iNumTexts++;
				}
			}
			else
			{
				grcDebugDraw::Text(vPedPosition, Color_green, 0, iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), "Targeting not active!");
				iNumTexts++;
			}

			grcDebugDraw::Text(vPedPosition, Color_green, 0,iNumTexts * grcDebugDraw::GetScreenSpaceTextHeight(), m_uFlags.IsFlagSet(ComF_TargetLost) ? "Target lost" : "Target Location Known");
			iNumTexts++;
		}
			
		grcDebugDraw::Line(pPed->GetTransform().GetPosition(), m_pPrimaryTarget->GetTransform().GetPosition(), targetLineColor);
	}

	if (sbDrawDebug)
	{
		Vector3 vRetreatPosition(vPedPosition);
		const CTaskCombatSeekCover* pSeekCoverTask = static_cast<const CTaskCombatSeekCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT_SEEK_COVER));
		if(pSeekCoverTask)
		{
			vRetreatPosition = pSeekCoverTask->GetCoverSearchStartPos();
		}

		grcDebugDraw::Sphere(vPedPosition,ms_Tunables.m_fTargetTooCloseDistance,Color_red,false);
		grcDebugDraw::Line(vPedPosition,vRetreatPosition,Color_blue,Color_blue);

		if (pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustSeekCover))
		{
			grcDebugDraw::Sphere(vPedPosition,ms_Tunables.m_fTargetTooCloseDistance,Color_red,false);
			grcDebugDraw::Line(vPedPosition,vRetreatPosition,Color_blue,Color_blue);
		}

		if (m_uFlags.IsFlagSet(ComF_TargetLost))
		{
			// time lost for
			static char szBuffer[128];
			sprintf(szBuffer,"Target lost :%f",m_lostTargetTimer.GetCurrentTime());
			grcDebugDraw::Text(vPedPosition,Color_green,0,0,szBuffer);
		}
		else
		{
			grcDebugDraw::Text(vPedPosition,Color_red,0,0,"Target Location Known");
			if (m_uFlags.IsFlagSet(ComF_TargetLost))
			{
				// time lost for
				static char szBuffer[128];
				sprintf(szBuffer,"Target lost :%f",m_lostTargetTimer.GetCurrentTime());
				grcDebugDraw::Text(vPedPosition,Color_green,0,0,szBuffer);
			}
			else
			{
				grcDebugDraw::Text(vPedPosition,Color_red,0,0,"Target Location Known");
			}
		}
	}

	if (GetSubTask())
	{
		GetSubTask()->Debug();
	}
	
	if (m_pCoverFinder && m_pCoverFinder->IsActive())
	{
		m_pCoverFinder->Debug(0);
	}
#endif // DEBUG_DRAW
}

#endif // !__FINAL

#if __BANK
void CTaskCombat::LogTargetInformation()
{
	bool bPrintStack = false;
	if(GetEntity())
	{
		if(CPed* pPed = GetPed())
		{
			if(!pPed->IsPlayer())
			{
				AI_LOG_WITH_ARGS("Primary target updated, target = %s, ped = %s, task = %p, ComF_PreventChangingTarget = %s \n", m_pPrimaryTarget ? m_pPrimaryTarget->GetLogName() : "Null", pPed->GetLogName(), this, m_uFlags.IsFlagSet(ComF_PreventChangingTarget) ? "TRUE" : "FALSE");
				bPrintStack = true;
				if(CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(false))
				{
					for (int i = 0; i > pPedTargetting->GetNumActiveTargets(); i++)
					{
						const CEntity* pTarget = pPedTargetting->GetTargetAtIndex(i);
						if(CSingleTargetInfo* pTargetInfo = pTarget ? pPedTargetting->FindTarget(pTarget) : NULL)
						{
							AI_LOG_WITH_ARGS("Target = %s, has score = %f \n",  pTarget->GetLogName(), pTargetInfo->GetTotalScore());
						}
					}
				}
			}
		}
	}
	else
	{
		AI_LOG_WITH_ARGS("Primary target updated, target = %s, task = %p, ComF_PreventChangingTarget = %s \n", m_pPrimaryTarget ? m_pPrimaryTarget->GetLogName() : "Null", this, m_uFlags.IsFlagSet(ComF_PreventChangingTarget) ? "TRUE" : "FALSE");
		bPrintStack = true;
	}

	if(bPrintStack && m_uFlags.IsFlagSet(ComF_PreventChangingTarget))
	{
		AI_LOG("See console log for callstack \n");
		aiDisplayf("Frame %i, CTaskCombat::LogTargetInformation, task = %p - callstack \n", fwTimer::GetFrameCount(), this);
		sysStack::PrintStackTrace();
	}
}
#endif