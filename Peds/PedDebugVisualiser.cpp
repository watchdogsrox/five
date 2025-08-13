#include "Peds/PedDebugVisualiser.h"

#if PEDDEBUGVISUALISER // bank-only

// game includes
#include "ai/debug/types/AnimationDebugInfo.h"
#include "ai/debug/types/AttachmentsDebugInfo.h"
#include "ai/debug/types/CombatDebugInfo.h"
#include "ai/debug/types/DamageDebugInfo.h"
#include "ai/debug/types/PedDebugInfo.h"
#include "ai/debug/types/MovementDebugInfo.h"
#include "ai/debug/types/RelationshipDebugInfo.h"
#include "ai/debug/types/ScriptHistoryDebugInfo.h"
#include "ai/debug/types/TaskDebugInfo.h"
#include "ai/debug/types/VehicleDebugInfo.h"
#include "ai/debug/types/WantedDebugInfo.h"
#include "ai/debug/types/EventsDebugInfo.h"
#include "ai/debug/types/TargetingDebugInfo.h"
#include "Animation/AnimBones.h" 
#include "animation/debug/AnimViewer.h"
#include "animation/move.h"
#include "fwanimation/animmanager.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/Viewport.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Control/GameLogic.h"
#include "crmotiontree/nodeanimation.h"
#include "Debug/DebugGlobals.h"
#include "Debug/DebugScene.h"
#include "Event/Decision/EventDecisionMaker.h"
#include "Event/EventDamage.h"
#include "Event/EventGroup.h"
#include "Event/EventNames.h"
#include "Event/Events.h"
#include "Event/EventMovement.h"
#include "Event/EventReactions.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "Event/System/EventData.h"
#include "game/Dispatch/DispatchHelpers.h"
#include "game/Dispatch/DispatchServices.h"
#include "game/Dispatch/Incidents.h"
#include "game/Dispatch/IncidentManager.h"
#include "Ik/IkManager.h"
#include "Ik/IkRequest.h"
#include "Ik/solvers/ArmIkSolver.h"
#include "Ik/solvers/BodyLookSolverProxy.h"
#include "Ik/solvers/BodyLookSolver.h"
#include "Ik/solvers/HeadSolver.h"
#include "Ik/solvers/LegSolverProxy.h"
#include "Ik/solvers/LegSolver.h"
#include "Ik/solvers/QuadLegSolverProxy.h"
#include "Ik/solvers/QuadLegSolver.h"
#include "ik/solvers/QuadrupedReactSolver.h"
#include "Ik/solvers/RootSlopeFixupSolver.h"
#include "Ik/solvers/TorsoReactSolver.h"
#include "Ik/solvers/TorsoVehicleSolverProxy.h"
#include "Ik/solvers/TorsoVehicleSolver.h"
#include "Ik/solvers/TorsoSolver.h"
#include "Modelinfo/BaseModelInfo.h"
#include "network/General/NetworkVehicleModelDoorLockTable.h"
#include "network/objects/entities/netobjvehicle.h"
#include "network/objects/entities/netobjphysical.h"
#include "Objects/Object.h"
#include "PedGroup/Formation.h"
#include "PedGroup/PedGroup.h"
#include "Peds/CombatData.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedEventScanner.h"
#include "Peds/PedFactory.h"
#include "Peds/PedFlagsMeta.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedIntelligence/PedAILodManager.h"
#include "Peds/PedIKSettings.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/PedPopulation.h"
#include "Peds/PedTaskRecord.h"
#include "Peds/rendering/pedVariationPack.h"
#include "peds/rendering/PedVariationDebug.h"
#include "Peds/Ped.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Peds/Pedmoveblend/PedMoveBlendOnFootPM.h"
#include "Peds/Pedmoveblend/PedMoveBlendOnSkis.h"
#include "Peds/PlayerInfo.h"
#include "Peds/HealthConfig.h"
#include "Peds/VehicleCombatAvoidanceArea.h"
#include "Peds/WildlifeManager.h"
#include "Physics/gtaInst.h"
#include "Physics/Physics.h"
#include "Physics/RagdollConstraints.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/River.h"
#include "Scene/2deffect.h"
#include "Scene/Entity.h"
#include "scene/EntityIterator.h"
#include "Script/commands_ped.h"
#include "Script/commands_player.h"
#include "Script/commands_task.h"
#include "Script/script_hud.h"
#include "Script/script_cars_and_peds.h"
#include "System/controlMgr.h"
#include "System/system.h"
#include "Task/Animation/TaskMoVEScripted.h"
#include "Task/Animation/TaskReachArm.h"
#include "Task/Animation/TaskScriptedAnimation.h"
#include "Task/Combat/CombatDirector.h"
#include "Task/Combat/CombatMeleeDebug.h"
#include "Task/Combat/CombatOrders.h"
#include "Task/Combat/CombatSituation.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/Cover/TaskStayInCover.h"
#include "Task/Combat/SubTasks/TaskDraggingToSafety.h"
#include "task/Combat/SubTasks/TaskShellShocked.h"
#include "task/Combat/SubTasks/TaskShootAtTarget.h"
#include "Task/Combat/SubTasks/TaskVariedAimPose.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Combat/TaskSharkAttack.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/TaskToHurtTransit.h"
#include "Task/Combat/TaskSearch.h"
#include "Task/Combat/TaskWrithe.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Debug/TaskDebug.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskChat.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Default/TaskFlyingWander.h"
#include "Task/Default/TaskSwimmingWander.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Default/TaskWander.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/Phone/TaskCallPolice.h"
#include "Task/Service/Swat/TaskSwat.h"
#include "Task/Combat/TaskReact.h"
#include "Task/motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Movement/Climbing/ClimbDebug.h"
#include "task/Movement/Climbing/DropDownDetector.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"
#include "Task/Movement/Climbing/TaskVault.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskFollowWaypointRecording.h"
#include "Task/Movement/TaskMoveWithinAttackWindow.h"
#include "Task/Movement/TaskFlyToPoint.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskGenericMoveToPoint.h"
#include "Task/Movement/TaskGoToPointAiming.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Physics/TaskAnimatedAttach.h"
#include "Task/Physics/AttachTasks/TaskNMGenericAttach.h"
#include "Task/Physics/NmDebug.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMBindPose.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMBuoyancy.h"
#include "Task/Physics/TaskNMDrunk.h"
#include "Task/Physics/TaskNMElectrocute.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Physics/TaskNMFallDown.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Task/Physics/TaskNMInjuredOnGround.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "Task/Physics/TaskNMPrototype.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMRiverRapids.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/TaskRageRagdoll.h"
#include "task/Response/TaskAgitated.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Response/TaskShockingEvents.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/Types/TaskChatScenario.h"
#include "Task/Scenario/ScenarioDebug.h"
#include "Task/System/MotionTaskData.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTree.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Movement/Climbing/TaskRappel.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Movement/TaskJetpack.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/System/TaskClassInfo.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCarCollisionResponse.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Vehicle/VehicleHullAI.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskAimGunScripted.h"
#include "Task/Weapons/TaskWeapon.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/WeaponTarget.h"
#include "Vehicles/VehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "Weapons/Explosion.h"
#include "weapons/TargetSequence.h"
#include "Task/Movement/TaskParachute.h"
#include "text/messages.h"
#include "Text/text.h"
#include "Text/TextConversion.h"
#include "vehicleAi/task/TaskVehicleGoTo.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/task/TaskVehiclePlaneChase.h"
#include "vehicleAi/task/TaskVehicleGoToPlane.h"
#include "vehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "vehicles/Metadata/VehicleEntryPointInfo.h"
#include "vehicles/Metadata/VehicleDebug.h"
#include "task/Default/ArrestHelpers.h"
#include "task/Default/TaskArrest.h"
#include "task/Default/TaskCuffed.h"
#include "Task/Combat/Subtasks/TaskReactToBuddyShot.h"
#include "Task/Response/TaskReactToImminentExplosion.h"
#include "Task/Response/TaskReactToExplosion.h"
#include "Task/motion/Locomotion/TaskMotionTennis.h"
#include "Weapons/AirDefence.h"

// rage includes
#include "art/messageparams.h"
#include "audioengine/engine.h"
#include "audiosoundtypes/sounddefs.h"
#include "crclip/clip.h"
#include "crclip/clipanimation.h"
#include "crmotiontree/nodeanimation.h"
#include "crmotiontree/nodeblendn.h "
#include "crmotiontree/nodeblend.h"
#include "crmotiontree/nodeclip.h"
#include "crmotiontree/nodefilter.h"
#include "crmotiontree/nodeexpression.h"
#include "fwanimation/directorcomponentfacialrig.h"
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwanimation/animhelpers.h"
#include "fwdecorator/decoratorExtension.h"
#include "fwscript/scriptobjinfo.h"
#include "fwscript/scriptinterface.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "fragmentnm/manager.h"
#include "vector/colors.h"
#include "vector/vector3.h"
#include "math/angmath.h"
#include "grcore/font.h"

AI_OPTIMISATIONS();

float g_fTaskMoveBlendRatio = MOVEBLENDRATIO_RUN;

//Mike tester: Task Set and Defend Position
bool	g_bIsPatrolling			= false;
float	g_fProximity			= 0.5f;
float	g_fTimer				= -1.0f;
int		g_iTaskGetupState		= 0;
int		g_iTaskGetupSpeed		= 0;

// Tez test
static char g_vCurrentOffsetPosition[256];
float g_fTargetRadius					= 0.1f;

bool g_bNavMeshUseTargetHeading = false;
bool g_bNavMeshStopExactly = false;
float g_fNavMeshTargetHeading = 0.0f;
bool g_bNavMeshDontAdjustTargetPosition = false;
float g_fNavMeshMaxDistanceToAdjustStartPosition = TRequestPathStruct::ms_fDefaultMaxDistanceToAdjustPathEndPoints;
bool g_bNavMeshAllowToNavigateUpSteepPolygons = false;

u32 CPedDebugVisualiserMenu::ms_FlushLogTime = 0;

bool CPedDebugVisualiserMenu::ms_bUseNewLocomotionTask = true;
char CPedDebugVisualiserMenu::ms_onFootClipSetText[60] = "";

bool CPedDebugVisualiserMenu::ms_BlipPedsInRelGroup = true;
char CPedDebugVisualiserMenu::ms_addRemoveSetRelGroupText[60] = "";
char CPedDebugVisualiserMenu::ms_RelGroup1Text[60] = "";
char CPedDebugVisualiserMenu::ms_RelGroup2Text[60] = "";

// Event History options
bool CPedDebugVisualiserMenu::ms_bShowRemovedEvents = true;
bool CPedDebugVisualiserMenu::ms_bHideFocusedPedsEventHistoryText = false;
bool CPedDebugVisualiserMenu::ms_bShowFocusedPedsEventPositions = true;
bool CPedDebugVisualiserMenu::ms_bShowFocusedPedsEventPositionsStateInfo = false;

bkBank*		CPedDebugVisualiserMenu::ms_pBank = NULL;
bkButton*	CPedDebugVisualiserMenu::ms_pCreateButton = NULL;

CDebugClipSelector	CPedDebugVisualiserMenu::m_alternateAnimSelector(true, false, false);
float				CPedDebugVisualiserMenu::m_alternateAnimBlendDuration;
bool				CPedDebugVisualiserMenu::m_alternateAnimLooped;
bool				CPedDebugVisualiserMenu::m_actionModeRun = true;
Matrix34			CPedDebugVisualiserMenu::m_StartMat(Matrix34::IdentityType);
bool				CPedDebugVisualiserMenu::m_bUseStartPos = false;


s32			CPedDebugVisualiserMenu::m_Mission = MISSION_NONE;
RegdEnt		CPedDebugVisualiserMenu::m_TargetEnt;
Vector3		CPedDebugVisualiserMenu::m_TargetPos = VEC3_ZERO;
Vector3		CPedDebugVisualiserMenu::m_goToTarget = Vector3(0.0f,2000.0f,1000.0f);
s32			CPedDebugVisualiserMenu::m_DrivingFlags = 3;
float		CPedDebugVisualiserMenu::m_StraightLineDistance = 10.0f;
float		CPedDebugVisualiserMenu::m_TargetReachedDistance = 10.0f;
float		CPedDebugVisualiserMenu::m_CruiseSpeed = 10.0f;
float		CPedDebugVisualiserMenu::m_fHeliOrientation = 0.0f;
s32			CPedDebugVisualiserMenu::m_iFlightHeight = 20;
s32			CPedDebugVisualiserMenu::m_iMinHeightAboveTerrain = 20;
bool		CPedDebugVisualiserMenu::m_abDrivingFlags[];
bool		CPedDebugVisualiserMenu::ms_bTestPlaneAttackPlayer = true;
bool		CPedDebugVisualiserMenu::ms_bTestPlanePlayerControl = false;
s32			CPedDebugVisualiserMenu::ms_iTestPlaneNumToSpawn = 3;
s32			CPedDebugVisualiserMenu::ms_iTestPlaneType = 0;

#if __DEV
PARAM(disableNewLocomotionTask, "Disable new locomotion task");
#endif // __DEV

PARAM(TestHarnessStartcoords,	"[AI] Test harness start coords");
PARAM(TestHarnessTargetcoords,	"[AI] Test harness Target coords");
PARAM(TestHarnessTask,			"[AI] Test harness task index");
PARAM(TestHarnessMissionIndex,	"[AI] Test harness mission index");
PARAM(TestHarnessCruiseSpeed,	"[AI] Test harness Cruise speed");

PARAM(focusTargetDebugMode,		"[AI] Render focus ped debug in screen space at side of screen");

PARAM(onlydisplayforfocusped,   "[AI] only display for focus ped");
PARAM(focusPedDisplayIn2D,		"[AI] Display focus ped in 2d");
PARAM(visualiseplayer,			"[AI] only display for player and visualise in vehicles");

//ped debug tools
Vector3 MousePosThisFrame = VEC3_ZERO;
Vector3 LastFrameWorldPos = VEC3_ZERO;
Vector3 ThisFrameWorldPos = VEC3_ZERO;
int LastFrameMouseX = 0;
int LastFrameMouseY = 0;

extern const char* parser_CVehicleEnterExitFlags__Flags_Strings[];

namespace ART
{
	extern NmRsEngine* gRockstarARTInstance;
}


static const char * g_pNavigateToCursorModes[] =
{
	"CTaskMoveGoToPoint",
	"CTaskMoveGoToPointAndStandStill",
	"CTaskMoveFollowNavMesh",
	"CTaskMoveFollowNavMesh - flee from target",
	"CTaskGoToPointAnyMeans",
	"CTaskGoToPointAiming",
	"RepositionMove",
	0
};

int g_iNavigateToCursorMode = 2;
int NAVIGATE_TO_CURSOR_NUM_MODES = 7;


void ShapeTestError_PS3();
void TestDodgyRunStart();


atString CClipDebugIterator::GetClipDebugInfo(const crClip* pClip, float Time, float weight, bool& addedContent)
{
	const char *clipDictionaryName = NULL;
	atString addedContentStr;
	addedContent = false;

	if (pClip->GetDictionary())
	{
		/* Iterate through the clip dictionaries in the clip dictionary store */
		const crClipDictionary *clipDictionary = NULL;
		int clipDictionaryIndex = 0, clipDictionaryCount = g_ClipDictionaryStore.GetSize();
		for(; clipDictionaryIndex < clipDictionaryCount && clipDictionaryName==NULL; clipDictionaryIndex ++)
		{
			if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
			{
				clipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));
				if(clipDictionary == pClip->GetDictionary())
				{
					clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictionaryIndex));
#if __BANK
					//Displayf("Dict(%s) - Path(%s)", clipDictionaryName, g_ClipDictionarySource[clipDictionaryIndex].c_str());
					if (g_ClipDictionarySource[clipDictionaryIndex].GetLength() > 0)
					{
						addedContentStr = g_ClipDictionarySource[clipDictionaryIndex];
						addedContent = true;
					}
#endif
				}
			}
		}
	}

	char outText[256] = "";

	sprintf(outText, "W:%.3f", weight );
	//const crClip* pClip = pClipNode->GetClipPlayer().GetClip();
	Assert(pClip);
	atString clipName(pClip->GetName());
	fwAnimHelpers::GetDebugClipName(*pClip, clipName);

	atString userName;
	const crProperties *pProperties = pClip->GetProperties();
	if(pProperties)
	{
		static u32 uUserKey = ATSTRINGHASH("SourceUserName", 0x69497CB4);
		const crProperty *pProperty = pProperties->FindProperty(uUserKey);
		if(pProperty)
		{
			const crPropertyAttribute *pPropertyAttribute = pProperty->GetAttribute(uUserKey);
			if(pPropertyAttribute && pPropertyAttribute->GetType() == crPropertyAttribute::kTypeInt)
			{
				const crPropertyAttributeInt *pPropertyAttributeInt = static_cast< const crPropertyAttributeInt * >(pPropertyAttribute);

				int iUserHash = pPropertyAttributeInt->GetInt();
				u32 uUserHash = *reinterpret_cast< u32 * >(&iUserHash);
				const char *szUser = atHashString(uUserHash).TryGetCStr();
				if(szUser)
				{
					userName = szUser;
				}
				else
				{
					userName = atVarString("%u", uUserHash);
				}
			}
		}
	}

	sprintf(outText, "%s, %s/%s, T:%.3f", outText, clipDictionaryName ? clipDictionaryName : "UNKNOWN", clipName.c_str(), Time);

	if (pClip->GetDuration()>0.0f)
	{
		sprintf(outText, "%s, P:%.3f", outText, Time / pClip->GetDuration());
	}
	sprintf(outText, "%s, %s", outText, userName.c_str() ? userName.c_str() : "No User");

	if (addedContent)
		sprintf(outText, "%s, %s", outText, addedContentStr.c_str());

	atString outAtString(outText);

	return outAtString;
}

void CClipDebugIterator::Visit(crmtNode& node)
{
	crmtIterator::Visit(node);

	if (node.GetNodeType()==crmtNode::kNodeClip)
	{

		crmtNodeClip* pClipNode = static_cast<crmtNodeClip*>(&node);
		if (pClipNode
		 && pClipNode->GetClipPlayer().HasClip())
		{
			//get the weight from the parent node (if it exists)
			float weight = CalcVisibleWeight(node);
			//if (weight > 0.0f)
			{
				const crClip* clip = pClipNode->GetClipPlayer().GetClip();
				bool addedContent = false;
				/* To display the clip dictionary name we need to find the clip dictionary index in the clip dictionary store */

				atString outText = GetClipDebugInfo(clip, pClipNode->GetClipPlayer().GetTime(), weight, addedContent);
				Color32 colour = addedContent ? CRGBA(180,192,96,255) : CRGBA(255,192,96,255);

				grcDebugDraw::Text( m_worldCoords, colour, 0, m_noOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), outText);
				m_noOfTexts++;
			}
		}
	}
}

float CClipDebugIterator::CalcVisibleWeight(crmtNode& node)
{
	crmtNode * pNode = &node;

	float weight = 1.0f;

	crmtNode* pParent = pNode->GetParent();

	while (pParent)
	{
		float outWeight = 1.0f;

		switch(pParent->GetNodeType())
		{
		case crmtNode::kNodeBlend:
			{
				crmtNodePairWeighted* pPair = static_cast<crmtNodePairWeighted*>(pParent);

				if (pNode==pPair->GetFirstChild())
				{
					outWeight = 1.0f - pPair->GetWeight();
				}
				else
				{
					outWeight = pPair->GetWeight();
				}
			}
			break;
		case crmtNode::kNodeBlendN:
			{
				crmtNodeN* pN = static_cast<crmtNodeN*>(pParent);
				u32 index = pN->GetChildIndex(*pNode);
				outWeight = pN->GetWeight(index);
			}
			break;
		default:
			outWeight = 1.0f;
			break;
		}

		weight*=outWeight;

		pNode = pParent;
		pParent = pNode->GetParent();
	}

	return weight;
}

char PersonalityTypeTexts[32][18] =
{
	"Player",
	"Cop",
	"Medic",
	"Fireman",
	"Gang 1",
	"Gang 2",
	"Gang 3",
	"Gang 4",
	"Gang 5",
	"Gang 6",
	"Gang 7",
	"Street Guy",
	"Suit Guy",
	"Sensible Guy",
	"Geek Guy",
	"Old Guy",
	"Tough Guy",
	"Street Girl",
	"Suit Girl",
	"Sensible Girl",
	"Geek Girl",
	"Old Girl",
	"Tough Girl",
	"Tramp",
	"Tourist",
	"Prostitute",
	"Criminal",
	"Busker",
	"Taxi Driver",
	"Psycho",
	"Steward",
	"Sports Fan"
};

//#define VISUALISE_RANGE				30.0f
//#define VISUALISE_RANGE_SQR			(30.0f * 30.0f)
#define DEFAULT_VISUALISE_RANGE				30.0f


CPedDebugVisualiser::ePedDebugVisMode CPedDebugVisualiser::eDisplayDebugInfo = CPedDebugVisualiser::eOff;
CPedDebugVisualiser::ePedDebugNMDebugMode CPedDebugVisualiser::eDebugNMMode = CPedDebugVisualiser::eNMOff;
bool CPedDebugVisualiser::ms_bDetailedDamageDisplay = false;
float CPedDebugVisualiser::ms_fDefaultVisualiseRange = DEFAULT_VISUALISE_RANGE;
float CPedDebugVisualiser::ms_fVisualiseRange = ms_fDefaultVisualiseRange;

Vector3 g_vDebugVisOrigin;
typedef CEntity * _ListType;
int CompareEntityFunc(const _ListType * ppEntity1, const _ListType * ppEntity2)
{
	const CEntity * pEntity1 = *ppEntity1;
	const CEntity * pEntity2 = *ppEntity2;
	float fDistSrq1 = MagSquared(pEntity1->GetTransform().GetPosition() - RCC_VEC3V(g_vDebugVisOrigin)).Getf();
	float fDistSrq2 = MagSquared(pEntity2->GetTransform().GetPosition() - RCC_VEC3V(g_vDebugVisOrigin)).Getf();
	int iCmp = (fDistSrq1 > fDistSrq2) ? -1 : 1;
	return iCmp;
}

void CPedDebugVisualiser::VisualisePedsNearPlayer()
{
	CPed * pPlayerPed = FindPlayerPed();

	// Set origin to be the debug cam, or the player..
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	const Vector3 origin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());

	g_vDebugVisOrigin = origin;

	atArray<CEntity*> entityList;

	// Chance here to override visialise range per-mode
	switch(GetDebugDisplay())
	{
		case eNavigation:
			ms_fVisualiseRange = 100.0f;
			break;
		default:
			break;
	}
	Vec3V vIteratorPos = VECTOR3_TO_VEC3V(origin);
	CEntityIterator entityIterator( IteratePeds|IterateVehicles|IterateObjects, pPlayerPed, &vIteratorPos, ms_fVisualiseRange*ms_fVisualiseRange );
	CEntity* pEntity = entityIterator.GetNext();
	while(pEntity)
	{
		entityList.PushAndGrow(pEntity);
		pEntity = entityIterator.GetNext();
	}

	entityList.QSort(0, -1, CompareEntityFunc);

	const bool isDisplayingOnlyFocus = CPedDebugVisualiserMenu::IsDisplayingOnlyForFocus();
	const bool isDisplayingOnlyPlayers = CPedDebugVisualiserMenu::IsDisplayingOnlyForPlayers();
	int e;
	for(e=0; e<entityList.size(); e++)
	{
		pEntity = entityList[e];
		const bool entityIsInFocusGroup = CDebugScene::FocusEntities_IsInGroup(pEntity);
		if( !isDisplayingOnlyFocus || entityIsInFocusGroup )
		{
			switch( pEntity->GetType() )
			{
			case ENTITY_TYPE_PED:
				if(!isDisplayingOnlyPlayers || ((CPed*)pEntity)->IsAPlayerPed())
				{
					VisualisePed(*(CPed*)pEntity);
				}
				break;
			case ENTITY_TYPE_VEHICLE:
				if(!isDisplayingOnlyPlayers || ((CVehicle*)pEntity)->ContainsPlayer())
				{
					VisualiseVehicle(*(CVehicle*)pEntity);
				}
				break;
			case ENTITY_TYPE_OBJECT:
				if(!isDisplayingOnlyPlayers)
				{
					VisualiseObject(*(CObject*)pEntity);
				}
				break;
			}
		}
	}
}

#include "camera/viewports/viewportManager.h"

void CPedDebugVisualiser::VisualisePed(const CPed& ped) const
{
	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsInReusePool))
	{
		return;
	}
	if(ped.GetMyMount())
	{
		if(CPedDebugVisualiserMenu::ms_menuFlags.m_bDontDisplayForRiders)
		{
			return;
		}
	}
	if(ped.GetSeatManager() && ped.GetSeatManager()->GetDriver())
	{
		if(CPedDebugVisualiserMenu::ms_menuFlags.m_bDontDisplayForMounts)
		{
			return;
		}
	}
	if(ped.IsDead() && CPedDebugVisualiserMenu::ms_menuFlags.m_bDontDisplayForDeadPeds)
	{
		return;
	}

	bool bDrawAxis = true;
	bool bDrawHeading = true;

	if(CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualisePedsInVehicles || (!(ped.GetMyVehicle() && ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))) || GetDebugDisplay()==eFacialDebugging || GetDebugDisplay()==eVisibilityDebugging)
	{
		switch(GetDebugDisplay())
		{
		case eTasksNoText:
			{
				VisualiseTasks(ped);
				break;
			}
		case eVehicleEntryDebug:
			{
				VisualiseVehicleEntryDebug(ped);
				break;
			}
		case eTasksFullDebugging:
			{
				VisualiseTasks(ped);
				VisualiseText(ped);
				break;
			}
		case eCombatDebugging:
			{
				VisualiseCombatText(ped);
				break;
			}
		case eSimpleCombat:
			{
				VisualiseSimpleCombatText(ped);
				bDrawAxis = false;
				bDrawHeading = false;
				break;
			}
		case eRelationshipDebugging:
			{
				VisualiseRelationshipText(ped);
				break;
			}
		case ePedGroupDebugging:
			{
				VisualisePedGroupText(ped);
				break;
			}
		case eMovementDebugging:
			{
				VisualiseMovement(ped);
				break;
			}
		case eRagdollDebugging:
			{
				VisualiseRagdoll(ped);
				break;
			}
		case eRagdollTypeDebugging:
			{
				VisualiseRagdollType(ped);
				bDrawAxis = false;
				bDrawHeading = false;
				break;
			}
		case eAnimationDebugging:
			{
				VisualiseAnimation(ped);
				break;
			}
		case eFacialDebugging:
			{
				VisualiseFacial(ped);
				break;
			}
		case eVisibilityDebugging:
			{
				VisualiseVisibility(ped);
				break;
			}
		case eGestureDebugging:
			{
				VisualiseGesture(ped);
				break;
			}
		case eIKDebugging:
			{
				VisualiseIK(ped);
				break;
			}
		case eQueriableStateDebugging:
			{
				VisualiseQueriableState(ped);
				break;
			}
		case ePedAmbientAnimations:
			{
				VisualiseAmbientAnimations(ped);
				break;
			}
		case eModelInformation:
			{
				VisualiseModelInformation(ped);
				bDrawHeading = false;
				break;
			}
		case ePedNames:
			{
				VisualisePedNames(ped);
				bDrawHeading = false;
				break;
			}
		case ePedLookAboutScoring:
			{
				VisualiseLookAbout(ped);
				break;
			}
		case eScanners:
			{
				VisualiseScanners(ped);
				bDrawAxis = false;
				break;
			}
		case eAvoidance:
			{
				VisualiseAvoidance(ped);
				bDrawAxis = false;
				bDrawHeading = false;
				break;
			}
		case eNavigation:
			{
				VisualiseNavigation(ped);
				break;
			}
		case eScriptHistory:
			{
				VisualiseScriptHistory(ped);
				break;
			}
		case eAudioContexts:
			{
				VisualiseAudioContexts(ped);
				break;
			}
		case eTaskHistory:
			{
				VisualiseTaskHistory(ped);
				break;
			}
		case eCurrentEvents:
			{
				VisualiseCurrentEvents(ped);
				break;
			}
		case eEventHistory:
			{
				VisualiseEventHistory(ped);
				break;
			}
		case eMissionStatus:
			{
				VisualiseMissionStatus(ped);
				break;
			}
		case eStealth:
			{
				VisualiseStealth(ped);
				break;
			}
		case eAssistedMovement:
			{
				VisualiseAssistedMovement(ped);
				break;
			}
		case eAttachment:
			{
				VisualiseAttachments(ped);
				break;
			}
		case eLOD:
			{
				VisualiseLOD(ped);
				break;
			}
		case eMotivation:
			{
				VisualiseMotivation(ped);
				break;
			}
		case eWanted:
			{
				VisualiseWanted(ped);
				break;
			}
		case eOrder:
			{
				VisualiseOrder(ped);
				break;
			}
		case ePersonalities:
			{
				VisualisePersonalities(ped);
				break;
			}
		case ePedFlags:
			{
				VisualisePedFlags(ped);
				break;
			}
		case eDamage:
			{
				VisualiseDamage(ped);
				break;
			}
		case eCombatDirector:
			{
				VisualiseCombatDirector(ped);
				break;
			}
		case eTargeting:
			{
				VisualiseTargeting(ped);
				break;
			}
		case eFocusAiPedTargeting:
			{
				VisualiseFocusAiPedTargeting(ped);
				break;
			}
		case ePopulationInfo:
			{
				VisualisePopulationInfo(ped);
				break;
			}
		case eArrestInfo:
			{
				VisualiseArrestInfo(ped);
				break;
			}
		case eVehLockInfo:
			{
				VisualiseVehicleLockInfo(ped);
				break;
			}
		case ePlayerCoverSearchInfo:
			{
				if (ped.IsLocalPlayer())
				{
					if (CCoverDebug::ms_Tunables.m_EnableNewCoverDebugRendering)
					{
						bool bChangedContext = false;
						const s32 iNumSubContexts = CCoverDebug::ms_Tunables.m_DefaultCoverDebugContext.m_SubContexts.GetCount();
						if (iNumSubContexts > 0)
						{
							if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG, "Decrement Cover Context"))
							{
								if (CCoverDebug::ms_Tunables.m_CurrentSelectedContext > -1)
								{
									--CCoverDebug::ms_Tunables.m_CurrentSelectedContext;
								}
								else
								{
									CCoverDebug::ms_Tunables.m_CurrentSelectedContext = iNumSubContexts-1;
								}
								bChangedContext = true;
							}
							if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG, "Increment Cover Context"))
							{
								if (CCoverDebug::ms_Tunables.m_CurrentSelectedContext < iNumSubContexts-1)
								{
									++CCoverDebug::ms_Tunables.m_CurrentSelectedContext;
								}
								else
								{
									CCoverDebug::ms_Tunables.m_CurrentSelectedContext = -1;
								}
								bChangedContext = true;
							}
							if (bChangedContext)
							{
								Color32 txtCol(255,255,0,255);
								static char text[256];
								if (CCoverDebug::ms_Tunables.m_CurrentSelectedContext == -1)
								{
									sprintf(text, "Cover Debug Context Changed To : DEFAULT");
								}
								else
								{
									if (CCoverDebug::ms_Tunables.m_DefaultCoverDebugContext.m_SubContexts[CCoverDebug::ms_Tunables.m_CurrentSelectedContext].m_Name == CCoverDebug::STATIC_COVER_POINTS)
									{
										CCoverDebug::ms_Tunables.m_RenderCoverPoints = true;
									}
									else
									{
										CCoverDebug::ms_Tunables.m_RenderCoverPoints = false;
									}
									sprintf(text, "Cover Debug Context Changed To : %s", CCoverDebug::ms_Tunables.m_DefaultCoverDebugContext.m_SubContexts[CCoverDebug::ms_Tunables.m_CurrentSelectedContext].m_Name.GetCStr());
								}

								CMessages::AddMessage(text, -1,
									1000, true, false, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false);
							}
						}
					}

					VisualisePlayerCoverSearchInfo(ped);

					if (CCoverDebug::ms_Tunables.m_EnableNewCoverDebugRendering)
					{
						return;
					}
				}
				break;
			}
		case ePlayerInfo:
			{
				VisualisePlayerInfo(ped);
				break;
			}
		case eParachuteInfo:
			{
				VisualiseParachuteInfo(ped);
				break;
			}
		case eMelee:
			{
				VisualiseMelee(ped);
				break;
			}
		default:
			break;
		}
	}

	VisualiseFOV(ped);

	if(bDrawHeading)
	{
		VisualisePosition(ped);
	}

	if(bDrawAxis && CPedDebugVisualiserMenu::ms_nRagdollDebugDrawBone > -2)
	{
		Matrix34 matDraw;
		if(CPedDebugVisualiserMenu::ms_nRagdollDebugDrawBone==-1)
			matDraw = RCC_MATRIX34(ped.GetCurrentPhysicsInst()->GetMatrix());
		else
			ped.GetGlobalMtx(CPedDebugVisualiserMenu::ms_nRagdollDebugDrawBone, matDraw);

		grcDebugDraw::Line(matDraw.d, matDraw.d + matDraw.a, Color32(1.0f,0.0f,0.0f));
		grcDebugDraw::Line(matDraw.d, matDraw.d + matDraw.b, Color32(0.0f,1.0f,0.0f));
		grcDebugDraw::Line(matDraw.d, matDraw.d + matDraw.c, Color32(0.0f,0.0f,1.0f));
	}

	// Displays the relationships by drawing lines between the ped and all other related peds
	if( CPedDebugVisualiserMenu::IsVisualisingRelationships() )
	{
		VisualiseRelationships(ped);
	}

	if(CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayPedBravery)
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if( pPlayerPed )
		{
			char debugText[10];
			sprintf(debugText, "%d", ped.GetPedModelInfo()->GetPersonalitySettings().GetBraveryFlags());
			grcDebugDraw::Text( VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + Vector3(0.0f, 0.0f, 1.0f), Color_blue, debugText);
		}
	}

	bool isDead = ped.IsDead();
	if( !isDead || !CPedDebugVisualiserMenu::DisableVisualisingDefensiveAreasForDeadPeds() )
	{
		if( CPedDebugVisualiserMenu::IsVisualisingDefensiveAreas() &&
			ped.GetPedIntelligence()->GetDefensiveArea()->IsActive() )
		{
			const bool bEditing = CPedDebugVisualiserMenu::GetFocusPed()==&ped;

			if(!CPedDebugVisualiserMenu::IsDebugDefensiveAreaDraggingActivated() || !bEditing ||
				( fwTimer::GetSystemFrameCount() & 2) || (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT))
			{
				// debug check defensive area
				if( !ped.GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()), true, isDead, bEditing) )
				{
					Vector3 vPoint;
					float fUnused;
					ped.GetPedIntelligence()->GetDefensiveArea()->GetCentreAndMaxRadius(vPoint, fUnused);
					// Draw a line between the ped and the centre point of the area
					grcDebugDraw::Line( VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()), vPoint, Color32( 1.0f, 0.0f, 0.0f ) );
				}
			}
		}

		if( CPedDebugVisualiserMenu::IsVisualisingSecondaryDefensiveAreas() )
		{
			CDefensiveArea* pSecondaryDefensiveArea = ped.GetPedIntelligence()->GetDefensiveAreaManager()->GetSecondaryDefensiveArea(false);
			if( pSecondaryDefensiveArea && pSecondaryDefensiveArea->IsActive() )
			{
				// debug check defensive area
				Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
				if( !pSecondaryDefensiveArea->IsPointInDefensiveArea(vPedPosition, true, isDead, false) )
				{
					Vector3 vPoint;
					float fUnused;
					pSecondaryDefensiveArea->GetCentreAndMaxRadius(vPoint, fUnused);
					// Draw a line between the ped and the centre point of the area
					grcDebugDraw::Line( VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()), vPoint, Color32( 1.0f, 0.0f, 0.0f ) );
				}
			}
		}
	}

	if( CPedDebugVisualiserMenu::GetFocusPed() == &ped && !isDead &&
		CPedDebugVisualiserMenu::IsVisualisingAttackWindowForFocusPed() &&
		ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
	{
		CEntity* pTargetEntity = ped.GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT);
		if(pTargetEntity && pTargetEntity->GetIsTypePed())
		{
			float fWindowMin = 0.0f;
			float fWindowMax = 0.0f;
			CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(&ped, fWindowMin, fWindowMax, false);
			grcDebugDraw::Sphere(pTargetEntity->GetTransform().GetPosition(), fWindowMin, Color_OrangeRed, false);
			grcDebugDraw::Sphere(pTargetEntity->GetTransform().GetPosition(), fWindowMax, Color_OrangeRed, false);
			float fCoverMax = ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatAttackWindowDistanceForCover);
			if(fCoverMax > 0.0f)
			{
				grcDebugDraw::Sphere(pTargetEntity->GetTransform().GetPosition(), fCoverMax, Color_OrangeRed, false);
			}
		}
	}

	if(CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayPedGroups)
	{
		CPedGroup* pPedsGroup = ped.GetPedsGroup();
		{
			if( pPedsGroup )
			{
				CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();

				if( &ped == pLeader )
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition())+ZAXIS, 0.3f, Color_green );
				}
				else
				{
					const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
					grcDebugDraw::Sphere(vecPedPosition+ZAXIS, 0.15f, Color_blue );
					if( pLeader )
					{
						grcDebugDraw::Line(vecPedPosition+ZAXIS, VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition())+ZAXIS, Color_blue, Color_green );
					}
				}
			}
		}
	}
}


void CPedDebugVisualiser::VisualiseVehicle(const CVehicle& vehicle) const
{
	switch(GetDebugDisplay())
	{
	case eAnimationDebugging:
		VisualiseAnimation(vehicle);
		break;
	case ePedLookAboutScoring:
		VisualiseLookAbout(vehicle);
		break;
	case ePedNames:
		VisualiseEntityName(vehicle,vehicle.GetDebugName());
		break;
	default:
		break;
	}
}

void CPedDebugVisualiser::VisualiseObject(const CObject& object) const
{
	if (GetDebugDisplay() == eAnimationDebugging)
	{
		VisualiseAnimation(object);
	}
	if( CPedDebugVisualiser::eDisplayDebugInfo == ePedLookAboutScoring )
	{
		VisualiseLookAbout(object);
	}
}


void CPedDebugVisualiser::VisualisePlayer()
{
	CPed * pPlayerPed = FindPlayerPed();

	if (CPedDebugVisualiserMenu::IsFocusPedDisplayIn2D() && pPlayerPed != CDebugScene::FocusEntities_Get(0))
	{
		return;
	}

	// Targeting debug
	pPlayerPed->GetPlayerInfo()->GetTargeting().Debug();

	if( CPedDebugVisualiserMenu::IsDisplayingForPlayer() )
	{
		VisualisePed(*pPlayerPed);
	}

	VisualiseNearbyVehicles(*pPlayerPed);

	if(CPedDebugVisualiserMenu::ms_menuFlags.m_bVisualiseNearbyLadders)
	{
		CTaskGoToAndClimbLadder::DrawNearbyLadder2dEffects(pPlayerPed);
	}
}

void CPedDebugVisualiser::DisplayPedDamageRecords(const bool bDisplay, const bool bDetailed)
{
	eDisplayDebugInfo = CPedDebugVisualiser::eOff;
	CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayDamageRecords = bDisplay;
	SetDetailedDamageDisplay(bDetailed);
}

void CPedDebugVisualiser::VisualiseNearbyVehicles(const CPed& ped) const
{
	const CVehicle* pVehicle=ped.GetPedIntelligence()->GetClosestVehicleInRange();
	if(pVehicle)
	{
		VisualiseOneVehicle(ped,*pVehicle);
	}
}

void CPedDebugVisualiser::VisualiseOneVehicle(const CPed& ped, const CVehicle& vehicle) const
{
	const float fPedZ = ped.GetTransform().GetPosition().GetZf();
	CEntityBoundAI bound((CVehicle&)vehicle, fPedZ, ped.GetCapsuleInfo()->GetHalfWidth());
	Vector3 corners[4];
	bound.GetCorners(corners);
	int i;
	Vector3 v0=corners[3];
	for(i=0;i<4;i++)
	{
		Vector3 v1=corners[i];
		grcDebugDraw::Line(v0,v1,Color32(0x00,0x00,0xff,0xff));
		v0=v1;
	}

	CVehicleComponentVisualiser::Process();

	if(CVehicleComponentVisualiser::m_bDrawConvexHull)
	{
		float fPedMinZ = -FLT_MAX;
		float fPedMaxZ = FLT_MAX;

		if(!CVehicleComponentVisualiser::m_bDebugIgnorePedZ)
		{
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
			fPedMinZ = vPedPosition.z - ped.GetCapsuleInfo()->GetGroundToRootOffset();
			fPedMaxZ = vPedPosition.z + ped.GetCapsuleInfo()->GetMaxSolidHeight();
		}

		CVehicleHullAI vehicleHull(const_cast<CVehicle*>(&vehicle));
		vehicleHull.Init(ped.GetCapsuleInfo()->GetHalfWidth(), NULL, fPedMinZ, fPedMaxZ);

		const int iNumHullVerts = vehicleHull.GetNumVerts();
		if(iNumHullVerts)
		{
			char tmpTxt[16];
			Vector2 vLast2d = vehicleHull.GetVert(iNumHullVerts-1);
			Vector3 vLast(vLast2d.x, vLast2d.y, fPedZ);
			for(int v=0; v<iNumHullVerts; v++)
			{
				Vector2 vVert2d = vehicleHull.GetVert(v);
				Vector3 vVert(vVert2d.x, vVert2d.y, fPedZ);

				grcDebugDraw::Line(vLast, vVert, Color32(0xff,0x00,0xff,0xff));
				sprintf(tmpTxt, "(%i)", v);
				grcDebugDraw::Text(vVert, Color_turquoise1, tmpTxt);

				vLast = vVert;
			}

			grcDebugDraw::Cross(Vec3V(vehicleHull.GetCentre().x, vehicleHull.GetCentre().y, fPedZ), 0.125f, Color32(0xff,0x00,0xff,0xff) );
		}

		// Now do a few tests on CVehicleHullAI
		Vector2 vCentre = vehicleHull.GetCentre();
		Assert(vehicleHull.LiesInside(vCentre));

		ASSERT_ONLY(Vector2 vMinusX = vCentre - Vector2(10.0f, 0.0f);)
		ASSERT_ONLY(Vector2 vPlusX = vCentre + Vector2(10.0f, 0.0f);)
		Assert(vehicleHull.Intersects(vMinusX, vPlusX));

		Vector2 vNearCentre = vCentre + Vector2(0.1f, 0.1f);
		Assert(vehicleHull.MoveOutside(vNearCentre));

		vNearCentre = vCentre + Vector2(0.1f, 0.1f);
		Assert(vehicleHull.MoveOutside(vNearCentre, Vector2(1.0f, 0.0f)));
	}
}


//-------------------------------------------------------------------------
// Displays the relationships by drawing lines between the focus peds and all other related peds
//-------------------------------------------------------------------------
void CPedDebugVisualiser::VisualiseRelationships(const CPed& ped) const
{
	// Loop through all nearby peds and draw lines to those that it has relationships with
	CPed::Pool *pool = CPed::GetPool();
	CPed* pOtherPed = NULL;
	s32 iPoolSize=pool->GetSize();

	const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	while(iPoolSize--)
	{
		pOtherPed = pool->GetSlot(iPoolSize);
		if(pOtherPed && pOtherPed != &ped)
		{
			Color32 iColRed(0xff,0,0,0xff);
			Color32 iColGreen(0,0xff,0,0xff);

			if( ped.GetPedIntelligence()->IsFriendlyWith( *pOtherPed ) )
			{
				grcDebugDraw::Line(vecPedPosition, VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition()), iColGreen);
			}
			else if( ped.GetPedIntelligence()->IsThreatenedBy( *pOtherPed ) )
			{
				grcDebugDraw::Line(vecPedPosition, VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition()), iColRed);
			}
		}
	}
}

void CPedDebugVisualiser::VisualiseAll(void)
{
	ms_fVisualiseRange = ms_fDefaultVisualiseRange;

	DebugJacking();

	if (CCoverDebug::ms_Tunables.m_RenderOtherDebug || GetDebugDisplay() != ePlayerCoverSearchInfo)
	{
		CPedAccuracy::ms_debugDraw.Render();
		CTask::ms_debugDraw.Render();
		CVehicleDebug::RenderDebug();
		CClimbLadderDebug::Draw();
		CClimbLadderDebug::Debug();
		CEventDebug::Debug();
		CCoverDebug::Debug();
		CVehicleCombatAvoidanceArea::Debug();
	}

	Vector3 vPos0 = Vector3::ZeroType;
	TUNE_GROUP_FLOAT(OFFSET_DEBUG, headingDegs, 0.0f, 0.0f, 360.0f, 1.0f);
	static Vector3 s_vOffset(0.0f,0.0f,0.0f);
	if (CPhysics::GetMeasuringToolPos(0, vPos0))
	{
		const float fHeadingRads = DtoR * headingDegs;
		Vector3 vRotatedOffset = s_vOffset;
		vRotatedOffset.x = s_vOffset.x * cosf(fHeadingRads) - s_vOffset.y * sinf(fHeadingRads);
		vRotatedOffset.y = s_vOffset.y * cosf(fHeadingRads) + s_vOffset.x * sinf(fHeadingRads);
		grcDebugDraw::Sphere(vPos0 + vRotatedOffset, 0.025f, Color_red);
		grcDebugDraw::Line(vPos0, vPos0 + vRotatedOffset, Color_red);
	}

	VisualisePedsNearPlayer();
	VisualisePlayer();

	switch (GetDebugDisplay())
	{
	case eRagdollDebugging:
	case eRagdollTypeDebugging:
		{
			// Print number of ragdolls available in the corner of the screen
			char ragdollStateString[64];

			static s32 s1 = 5;
			static s32 s2 = 7;
			static s32 yOffset = 1;
			bool bSave = grcDebugDraw::GetDisplayDebugText();
			int numMedLodRagdolls = 0;
			int numLowLodRagdolls = 0;
			phArticulatedCollider::GetNumberOfMedAndLodLODBodies(numMedLodRagdolls, numLowLodRagdolls);

			int numActiveAgents = FRAGNMASSETMGR->GetAgentCapacity(0) - FRAGNMASSETMGR->GetAgentCount(0);
			grcDebugDraw::SetDisplayDebugText(TRUE);

			s32 yPosition = s2+(yOffset);

			sprintf(ragdollStateString, "Active NM Agents: %d", numActiveAgents);
			grcDebugDraw::PrintToScreenCoors(ragdollStateString, s1, yPosition);
			yPosition += yOffset;
			sprintf(ragdollStateString, "Active Rage Ragdolls : %d", FRAGNMASSETMGR->GetNumActiveNonNMRagdolls());
			grcDebugDraw::PrintToScreenCoors(ragdollStateString, s1, yPosition);
			yPosition += yOffset;
			sprintf(ragdollStateString, "Active Med LOD Ragdolls: %d", numMedLodRagdolls);
			grcDebugDraw::PrintToScreenCoors(ragdollStateString, s1, yPosition);
			yPosition += yOffset;
			sprintf(ragdollStateString, "Active Low LOD Ragdolls: %d", numLowLodRagdolls);
			grcDebugDraw::PrintToScreenCoors(ragdollStateString, s1, yPosition);
			yPosition += yOffset;

			bool bPoolingEnabled = NetworkInterface::IsGameInProgress() ? CTaskNMBehaviour::sm_Tunables.m_EnableRagdollPoolingMp : CTaskNMBehaviour::sm_Tunables.m_EnableRagdollPooling;
			sprintf(ragdollStateString, "Ragdoll pooling is %s", bPoolingEnabled ? "ON" : "OFF");
			grcDebugDraw::PrintToScreenCoors(ragdollStateString, s1, yPosition);
			yPosition += yOffset;

			for (s32 i=CTaskNMBehaviour::kRagdollPoolNmGameplay; i < CTaskNMBehaviour::kNumRagdollPools; i++)
			{
				CTaskNMBehaviour::eRagdollPool pool = (CTaskNMBehaviour::eRagdollPool)i;
				CTaskNMBehaviour::CRagdollPool& ragdollPool = CTaskNMBehaviour::GetRagdollPool(pool);
				sprintf(ragdollStateString, "%s pool, num ragdolls: %d/%d", CTaskNMBehaviour::GetRagdollPoolName(pool), ragdollPool.GetRagdollCount(), ragdollPool.GetMaxRagdolls());
				grcDebugDraw::PrintToScreenCoors(ragdollStateString, s1, yPosition);
				yPosition+=yOffset;
			}

			grcDebugDraw::SetDisplayDebugText(bSave);
		}
		break;
	default:
		{
		}
		break;
	}

	if(CPedDebugVisualiserMenu::ms_bRagdollContinuousTestCode)
		CPedDebugVisualiserMenu::RagdollContinuousDebugCode();
}


bool GetCoorsAndAlphaForDebugText(const CEntity &entity, Vector3 &vecReturnWorldCoors, u8 &ReturnAlpha)
{
	const Vector3 vecEntityPosition = VEC3V_TO_VECTOR3(entity.GetTransform().GetPosition());
	Vector3 vDiff = vecEntityPosition - camInterface::GetPos();

	float fDist = vDiff.Mag();
	if(fDist >= CPedDebugVisualiser::ms_fVisualiseRange && CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eVehicleEntryDebug)
		return false;

	vecReturnWorldCoors = vecEntityPosition + Vector3(0,0,1.0f);

#if FPS_MODE_SUPPORTED
	TUNE_GROUP_FLOAT(DEBUG_TEXT, DEFAULT_SIDE_OFFSET, 1.5f, 0.0f, 4.0f, 0.01f);
	TUNE_GROUP_FLOAT(DEBUG_TEXT, SCOPED_SIDE_OFFSET, 0.5f, 0.0f, 4.0f, 0.01f);
	TUNE_GROUP_FLOAT(DEBUG_TEXT, DEFAULT_UP_OFFSET, 0.5f, 0.0f, 4.0f, 0.01f);
	TUNE_GROUP_FLOAT(DEBUG_TEXT, SCOPED_UP_OFFSET, 0.3f, 0.0f, 4.0f, 0.01f);
	const bool bScoped = (entity.GetIsTypePed() && static_cast<const CPed&>(entity).GetMotionData()->GetIsFPSScope());
	const float fSideOffset = bScoped ? SCOPED_SIDE_OFFSET : DEFAULT_SIDE_OFFSET;
	const float fUpOffset = bScoped ? SCOPED_UP_OFFSET : DEFAULT_UP_OFFSET;
	if(camInterface::GetDominantRenderedCamera() == (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() &&
		&entity == ((camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera())->GetAttachParent())
	{
		Matrix34 camMatrix = ((camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera())->GetFrame().GetWorldMatrix();
		vecReturnWorldCoors = camMatrix.d + camMatrix.b * 3.0f + camMatrix.c * fUpOffset - camMatrix.a * fSideOffset;
	}
	else if (camInterface::GetDominantRenderedCamera() == (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera() &&
		entity.GetIsTypePed() && static_cast<const CPed&>(entity).IsInFirstPersonVehicleCamera())
	{
		Matrix34 camMatrix = ((camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera())->GetFrame().GetWorldMatrix();
		vecReturnWorldCoors = camMatrix.d + camMatrix.b * 3.0f + camMatrix.c * fUpOffset - camMatrix.a * fSideOffset;
	}
#endif // FPS_MODE_SUPPORTED

	float fScale = 1.0f - (fDist / CPedDebugVisualiser::ms_fVisualiseRange);
	ReturnAlpha = CPedDebugVisualiserMenu::IsFocusPedDisplayIn2D() ? 255 : (u8)Clamp((int)(255.0f * fScale), 0, 255);
	return true;
}

EXTERN_PARSER_ENUM(BehaviourFlags);
extern const char* parser_CCombatData__BehaviourFlags_Strings[];

bool CPedDebugVisualiser::VisualiseCombatText(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	CCombatTextDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, 255));
	tempDebugInfo.Print();
	return true;
}

bool CPedDebugVisualiser::VisualiseSimpleCombatText(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	int iNoOfTexts=0;
	const int DebugTextLength = 128;
	char debugText[DebugTextLength];

	// General combat activity status:
	// InCover - using cover
	// Charging - charging
	// MovingToCover - moving to cover
	// MovintToTacticalPoint - moving to a tactical point
	// InOpen - default combat case
	const CQueriableInterface* pQueriableInterface = ped.GetPedIntelligence()->GetQueriableInterface();
	if( pQueriableInterface )
	{
		CTaskInfo* pCombatTaskInfo = pQueriableInterface->FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_COMBAT);
		if( pCombatTaskInfo )
		{
			// First check if the ped is in cover (multiple states can do this)
			CTaskInfo* pInCoverTaskInfo = pQueriableInterface->FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_IN_COVER);
			if( pInCoverTaskInfo )
			{
				formatf( debugText, "InCover");
			}
			else // not in cover
			{
				int iCombatState = pCombatTaskInfo->GetState();
				if( iCombatState == CTaskCombat::State_ChargeTarget )
				{
					formatf( debugText, "Charging");
				}
				else if( iCombatState == CTaskCombat::State_MoveToCover )
				{
					formatf( debugText, "MovingToCover");
				}
				else if( iCombatState == CTaskCombat::State_MoveToTacticalPoint )
				{
					formatf( debugText, "MovingToTacticalPoint");
				}
				else // default combat case
				{
					formatf( debugText, "InOpen");
				}
			}
			grcDebugDraw::Text( WorldCoors, CRGBA(64,255,64,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
		else // not in combat
		{
			return false;
		}
	}

	// Combat movement status:
	// DefensiveArea - if ped has a defensive area OR
	// Stationary, Defensive, WillAdvance, WillRetreat - according to combat movement
	// NOTE: Also append a (C) if allowed to charge
	const bool bMayCharge = ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanCharge);
	if( ped.GetPedIntelligence()->GetDefensiveArea() && ped.GetPedIntelligence()->GetDefensiveArea()->IsActive() )
	{
		if( bMayCharge )
		{
			formatf( debugText, "DefensiveArea(C)");
		}
		else
		{
			formatf( debugText, "DefensiveArea");
		}
	}
	else if( ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_Stationary )
	{
		if( bMayCharge )
		{
			formatf( debugText, "Stationary(C)");
		}
		else
		{
			formatf( debugText, "Stationary");
		}
	}
	else if( ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_Defensive )
	{
		if( bMayCharge )
		{
			formatf( debugText, "Defensive(C)");
		}
		else
		{
			formatf( debugText, "Defensive");
		}
	}
	else if( ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_WillRetreat )
	{
		if( bMayCharge )
		{
			formatf( debugText, "WillRetreat(C)");
		}
		else
		{
			formatf( debugText, "WillRetreat");
		}
	}
	else if( ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() == CCombatData::CM_WillAdvance )
	{
		if( bMayCharge )
		{
			formatf( debugText, "WillAdvance(C)");
		}
		else
		{
			formatf( debugText, "WillAdvance");
		}
	}
	else
	{
		formatf( debugText, "Unknown!");
	}
	grcDebugDraw::Text( WorldCoors, CRGBA(64,255,64,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	// LOS to target status:
	CPedTargetting* pPedTargetting = ped.GetPedIntelligence()->GetTargetting(false);
	if( pPedTargetting )
	{
		const CEntity* pCurrentTarget = pPedTargetting->GetCurrentTarget();
		if( pCurrentTarget )
		{
			const bool bForceImmediateUpdate = false;
			LosStatus currentStatus = pPedTargetting->GetLosStatus(pCurrentTarget, bForceImmediateUpdate);
			if( currentStatus == Los_clear )
			{
				formatf( debugText, "LOSToTarget");
				grcDebugDraw::Text( WorldCoors, CRGBA(0,255,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
			else
			{
				formatf( debugText, "NoLOSToTarget");
				grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
		}
	}

	// Health and Armor
	formatf( debugText, "Hlth(%.0f), Ar(%.0f)", ped.GetHealth(), ped.GetArmour());
	grcDebugDraw::Text( WorldCoors, CRGBA(0,0,255,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	// Endurance
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested))
	{
		formatf(debugText, "En(%.0f)", ped.GetEndurance());
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 180, 0, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}

	// Accuracy
	formatf( debugText, "Acc(%.0f)", 100.0f * ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponAccuracy) );
	grcDebugDraw::Text( WorldCoors, CRGBA(255, 223, 0, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	// Damage
	formatf( debugText, "Dmg(%.2f)", ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier) );
	grcDebugDraw::Text( WorldCoors, CRGBA(255, 223, 0, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	return (iNoOfTexts > 0);
}

bool CPedDebugVisualiser::VisualiseRelationshipText(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	CRelationshipDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
	return true;
}

bool CPedDebugVisualiser::VisualisePedGroupText(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlphaNotUsed = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlphaNotUsed))
	{
		return false;
	}

	int iNoOfTexts=0;

	// Relationship group

	s32 iPedGroupIndex = ped.GetPedGroupIndex();

	static const s32 MAX_STRING_LENGTH = 64;

	char szText[MAX_STRING_LENGTH];

	formatf(szText, "Ped Group Index : %i", iPedGroupIndex);
	grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), szText);

	CPedGroup* pGroup = ped.GetPedsGroup();
	if (pGroup)
	{
		//s32 iPedGroup = pGroup->GetGroupIndex();

		const CPed* pLeader = pGroup->GetGroupMembership()->GetLeader();

		if (pLeader)
		{
			const CBaseModelInfo* pModelInfo = pLeader->GetBaseModelInfo();
			if (pModelInfo)
			{
				formatf(szText, MAX_STRING_LENGTH, "Group Leader Model : %s", pModelInfo->GetModelName());
				grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), szText);
			}
		}

		s32 iNumMembers = pGroup->GetGroupMembership()->CountMembers();
		formatf(szText, MAX_STRING_LENGTH, "Number in group : %i", iNumMembers);
		grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), szText);

		if (pGroup->GetFormation())
		{
#if __DEV
			formatf(szText, MAX_STRING_LENGTH, "Formation : %s", pGroup->GetFormation()->GetFormationTypeName());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), szText);
#endif // __DEV
		}
	}

	return (iNoOfTexts > 0);
}



//// Display animation debug text
bool CPedDebugVisualiser::VisualiseAnimation(const CDynamicEntity& dynamicEntity) const
{
	Vector3 WorldCoors;
	u8 alpha = 255;
	if (!GetCoorsAndAlphaForDebugText(dynamicEntity, WorldCoors, alpha))
	{
		return false;
	}

	CAnimationDebugInfo tempDebugInfo(&dynamicEntity, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, alpha));
	tempDebugInfo.Print();
	return true;
}

//// Display animation debug text
bool CPedDebugVisualiser::VisualiseFacial(const CDynamicEntity& dynamicEntity) const
{
	Vector3 WorldCoors;
	int iNoOfTexts=0;
	u8 alpha = 255;
	char tmp[256];

	if (!GetCoorsAndAlphaForDebugText(dynamicEntity, WorldCoors, alpha))
	{
		return false;
	}

	if (alpha==0)
	{
		return false;
	}

	if (dynamicEntity.GetAnimDirector())
	{
		CClipDebugIterator it(WorldCoors, NULL);

		fwAnimDirectorComponentMotionTree* pComponentMotionTreeMidPhysics = dynamicEntity.GetAnimDirector()->GetComponentByPhase<fwAnimDirectorComponentMotionTree>(fwAnimDirectorComponent::kPhaseMidPhysics);
		if (pComponentMotionTreeMidPhysics)
		{
			pComponentMotionTreeMidPhysics->GetMotionTree().Iterate(it);
		}

		iNoOfTexts = it.GetNoOfLines();
	}

	if (dynamicEntity.GetIsTypePed())
	{
		const CPed& ped = static_cast<const CPed&>(dynamicEntity);
		// render the facial idle state

		// render the current viseme requests from audio
		sprintf(tmp, "Visemes %s: %s, %s", ped.GetVisemeAnimsBlocked() && ped.GetVisemeAnimsAudioBlocked() ? "blocked" : "allowed", ped.GetVisemeAnimsBlocked() ? "blocked by game" : "allowed by game", ped.GetVisemeAnimsAudioBlocked() ? "blocked by audio" : "allowed by audio");
		grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,alpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);

		if (ped.GetIsVisemePlaying())
		{
			const CPed::VisemeDebugInfo& info = ped.GetVisemeDebugInfo();
			atString clipName(info.m_clipName.GetCStr());
			atArray<atString> splitName;
			clipName.Split(splitName, "/");
			sprintf(tmp, "Viseme playing: %s", splitName[splitName.GetCount()-1].c_str());
			grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,alpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
			sprintf(tmp, "time: %.3f/%.3f predelay: %.3f/%.3f prev time: %.3f body additive blend: %.3f", ((float)info.m_time)/1000.0f, ped.GetVisemeDuration(), ((float)info.m_preDelayRemaining)/1000.0f, ((float)info.m_preDelay)/1000.0f, ped.GetPreviousVisemeTime(), ped.GetVisemeBodyAdditiveWeight());
			grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,alpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
		}
		else
		{
			grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,alpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "No viseme playing");
		}
		if (ped.GetIsWaitingForSpeechToPreload())
		{
			grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,alpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Waiting for speech to preload");
		}

		const audSpeechAudioEntity* pSpeechEntity = ped.GetSpeechAudioEntity();
		if (pSpeechEntity)
		{
			if (pSpeechEntity->IsAmbientSpeechPlaying() || pSpeechEntity->IsScriptedSpeechPlaying() || pSpeechEntity->IsPainPlaying())
			{
				sprintf(tmp, "Speech entity playing: %s %s %s", pSpeechEntity->IsAmbientSpeechPlaying() ? "ambient" : "",  pSpeechEntity->IsScriptedSpeechPlaying() ? "scripted" : "",  pSpeechEntity->IsPainPlaying() ? "pain" : "");
				grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,alpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
			}

		}
	}

	return true;
}


bool CPedDebugVisualiser::VisualiseGesture(const CPed &ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	int iNoOfTexts=0;
	char tmp[256];

	if (ped.IsPlayingAGestureAnim())
	{
		atString clip; clip = ped.GetGestureClip()->GetName();
		clip.Replace("pack:/", "");
		clip.Replace(".clip", "");
		sprintf(tmp, "Gesture:%s\\%s", ped.GetGestureClipSet().TryGetCStr(), clip.c_str());
		grcDebugDraw::Text( WorldCoors, CRGBA(0,255,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
	}
	else
	{
		if(ped.BlockGestures(BANK_ONLY(false)))
		{
			sprintf(tmp, "Gesture:%s", ped.GetGestureBlockReason());
			grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
		}
		else
		{
			sprintf(tmp, "Gesture:ALLOWED");
			grcDebugDraw::Text( WorldCoors, CRGBA(0,255,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
		}
	}

	return true;
}



//// Display visulation debug text
bool CPedDebugVisualiser::VisualiseVisibility(const CDynamicEntity& dynamicEntity) const
{
	Vector3 WorldCoors;
	int iNoOfTexts=0;
	u8 alpha = 255;
	char tmp[256];

	if (!GetCoorsAndAlphaForDebugText(dynamicEntity, WorldCoors, alpha))
	{
		return false;
	}

	if (alpha==0)
	{
		return false;
	}

	if (dynamicEntity.GetIsTypePed())
	{
		const CPed& ped = static_cast<const CPed&>(dynamicEntity);

		// Visible info
		char isVisibleInfo[128];
		ped.GetIsVisibleInfo( isVisibleInfo, 128 );
		sprintf(tmp, "IsVisible: %s", isVisibleInfo);
		grcDebugDraw::Text( WorldCoors, ped.IsBaseFlagSet(fwEntity::IS_VISIBLE)? Color_green : Color_red, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
		iNoOfTexts++;
	}

	return true;
}

// Display IK debug text
bool CPedDebugVisualiser::VisualiseIK(const CPed& BANK_ONLY(constPed)) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(constPed, WorldCoors, iAlpha))
	{
		return false;
	}

	CIkDebugInfo tempDebugInfo(&constPed, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
	return true;
}

#if ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG
void CPedDebugVisualiser::VisualiseStateHistoryForTask(const CTask& task, const Vector3& WorldCoors, s32 iAlpha, s32& iNoOfTexts, char* tmp) const
{
	if (aiTaskStateHistoryManager::GetInstance())
	{
		aiTaskStateHistory* pTaskStateHistory = aiTaskStateHistoryManager::GetInstance()->GetTaskStateHistoryForTask(&task);
		if (pTaskStateHistory)
		{
			Color32 col = Color_red; col.SetAlpha(iAlpha);
			sprintf(tmp, "Frame Changed | Previous States");
			grcDebugDraw::Text( WorldCoors, col, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
			iNoOfTexts++;

			s32 iCurrentState = pTaskStateHistory->GetFreeSlotStateIndex();
			if (iCurrentState == 0 || iCurrentState == 1)
			{
				iCurrentState = aiTaskStateHistory::MAX_TASK_STATES + (iCurrentState - 2);
			}
			else
			{
				iCurrentState -= 2;
			}

			const s32 iStartState = iCurrentState;

			while (pTaskStateHistory->GetStateAtIndex(iCurrentState) != (s32)aiTaskStateHistory::TASK_STATE_INVALID)
			{
				sprintf(tmp, "%u | %s", pTaskStateHistory->GetStateFrameCountAtIndex(iCurrentState), TASKCLASSINFOMGR.GetTaskStateName(task.GetTaskType(), pTaskStateHistory->GetStateAtIndex(iCurrentState)));

				if (iCurrentState == 0)
				{
					iCurrentState = aiTaskStateHistory::MAX_TASK_STATES - 1;
				}
				else
				{
					--iCurrentState;
				}

				if (iCurrentState == iStartState)
				{
					break;
				}

				// Only draw text if we haven't already displayed it for this state
				grcDebugDraw::Text( WorldCoors, col, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
				iNoOfTexts++;
			}
		}
	}
}
#endif // ENABLE_SIMPLE_TASK_STATE_HISTORY_DEBUG

void CPedDebugVisualiser::VisualiseText(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	if(iAlpha==0)
		return;

	CTasksFullDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
}

bool CPedDebugVisualiser::VisualiseModelInformation(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	int iNoOfTexts=0;

	Color32 colour = CRGBA(128,128,255,iAlpha);
	const char* debugText = ped.GetBaseModelInfo()->GetModelName();
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );

	return true;
}

bool CPedDebugVisualiser::VisualisePedNames(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	int iNoOfTexts=0;

	Color32 colour = CRGBA(128,128,255,iAlpha);
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ped.m_debugPedName);

	extern bool bShowMissingLowLOD;			// in ped.cpp
	if (bShowMissingLowLOD)
	{
		Color32 colour = CRGBA(255,255,255,iAlpha);
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "<No Low LOD>");
	}

	return true;
}

void CPedDebugVisualiser::VisualiseEntityName(const CEntity& entity, const char* strName) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(entity, WorldCoors, iAlpha))
	{
		return;
	}

	int iNoOfTexts=0;

	Color32 colour = CRGBA(128,128,255,iAlpha);
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), strName);
}

bool CPedDebugVisualiser::VisualiseRagdollType(const CPed& ped) const
{
	if (ped.GetUsingRagdoll())
	{
		Vector3 WorldCoors;
		u8 iAlpha = 255;
		if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
		{
			return false;
		}

		int iNoOfTexts=0;

		Color32 colour = CRGBA(255,192,128,iAlpha);
		//***************************************************************************
		//	Display the ped's ragdoll type....
		//***************************************************************************

		char ragdollStateString[64];
		colour = CRGBA(255,192,128,iAlpha);

		bool bUsingRageRagdoll = ped.GetRagdollInst() && ped.GetRagdollInst()->GetNMAgentID() == -1;
		bool bUsingNMRelax = !bUsingRageRagdoll && (ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_RELAX)!=NULL);

		if (CPedDebugVisualiserMenu::ms_bRenderRagdollTypeText)
		{
			sprintf(ragdollStateString, "%s(0x%p)", bUsingRageRagdoll ? "Rage" : bUsingNMRelax ? "NM Relax" : "NM", &ped);
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
		}

		if(CPedDebugVisualiserMenu::ms_bRenderRagdollTypeSpheres)
		{
			Color32 col;
			if(bUsingRageRagdoll)
			{
				col = Color_red;
			}
			else if(bUsingNMRelax)
			{
				col = Color_yellow;
			}
			else
			{
				col = Color_green;
			}

			grcDebugDraw::Sphere(WorldCoors, 0.3f, col);
		}

	}
	return true;
}

bool CPedDebugVisualiser::VisualiseRagdoll(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	int iNoOfTexts=0;

	Color32 colour = CRGBA(255,192,128,iAlpha);
	//***************************************************************************
	//	Display the ped's ragdoll state....
	//***************************************************************************

	char ragdollStateString[128];
	colour = CRGBA(255,192,128,iAlpha);

	formatf(ragdollStateString, "Ragdoll State(%s)", CPed::GetRagdollStateName(ped.GetRagdollState()));
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	formatf(ragdollStateString, "Ragdoll pool: current(%s) desired(%s)", CTaskNMBehaviour::GetRagdollPoolName(ped.GetCurrentRagdollPool()), CTaskNMBehaviour::GetRagdollPoolName(ped.GetDesiredRagdollPool()));
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	formatf(ragdollStateString, "Current physics inst: %s", ped.GetCurrentPhysicsInst()==ped.GetAnimatedInst() ? "ANIMATED" : "RAGDOLL");
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

#if __ASSERT
	formatf(ragdollStateString, "Last ragdoll controlling task: %s", TASKCLASSINFOMGR.GetTaskName(ped.GetLastRagdollControllingTaskType()));
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;
#endif //__ASSERT

	if(ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_SHOT))
	{
		CTaskNMShot *task = (CTaskNMShot*)ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_SHOT);
		if(task)
		{
			formatf(ragdollStateString, "Shot type: %s", task->GetShotTypeName());
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
		}
	}

	// How did the ped die (Animated? Ragdolled? Was snap to ground used? etc.)
	if (ped.IsDead())
	{
		CTaskDyingDead* pTaskDead = static_cast<CTaskDyingDead*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
		if( pTaskDead )
		{
			formatf(ragdollStateString, "TaskDyingDead State History:");
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;

			CTaskDyingDead::StateData stateElem;
			atQueue<CTaskDyingDead::StateData, STATE_HISTORY_BUFFER_SIZE> tempQueue;
			while (!pTaskDead->GetStateHistory().IsEmpty())
			{
				stateElem = pTaskDead->GetStateHistory().Pop();
				switch(stateElem.m_state)
				{
				case CTaskDyingDead::State_Start:
					formatf(ragdollStateString, " State_Start: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_StreamAssets:
					formatf(ragdollStateString, " State_StreamAssets: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DyingAnimated:
					formatf(ragdollStateString, " State_DyingAnimated: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DyingRagdoll:
					formatf(ragdollStateString, " State_DyingRagdoll: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_RagdollAborted:
					formatf(ragdollStateString, " State_RagdollAborted: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DeadAnimated:
					formatf(ragdollStateString, " State_DeadAnimated: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DeadRagdoll:
					formatf(ragdollStateString, " State_DeadRagdoll: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DeadRagdollFrame:
					formatf(ragdollStateString, " State_DeadRagdollFrame: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_FallOutOfVehicle:
					formatf(ragdollStateString, " State_FallOutOfVehicle: %u", stateElem.m_startTimeInStateMS);
					break;
				case CTaskDyingDead::State_DyingAnimatedFall:
					formatf(ragdollStateString, " State_DyingAnimatedFall: %u", stateElem.m_startTimeInStateMS);
					break;
				default:
					Assertf(0, "Unknown State");
					break;
				}
				grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
				iNoOfTexts++;
				tempQueue.Push(stateElem);
			}
			while (!tempQueue.IsEmpty())
			{
				pTaskDead->GetStateHistory().Push(tempQueue.Pop());
			}

			// Print the snap state
			switch(pTaskDead->GetSnapToGroundStage())
			{
			case CTaskDyingDead::kSnapNotBegun:
				formatf(ragdollStateString, " Snap state: kSnapNotBegun");
				break;
			case CTaskDyingDead::kSnapFailed:
				formatf(ragdollStateString, " Snap state: kSnapFailed");
				break;
			case CTaskDyingDead::kSnapPoseRequested:
				formatf(ragdollStateString, " Snap state: kSnapPoseRequested");
				break;
			case CTaskDyingDead::kSnapPoseReceived:
				formatf(ragdollStateString, " Snap state: kSnapPoseReceived");
				break;
			default:
				Assertf(0, "Unknown Snap State");
				break;
			}
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
		}
	}

	// ragdoll blocking flags
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact))
	{
		formatf(ragdollStateString, "Blocking activation from any ped impacts");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_DontActivateRagdollFromAnyPedImpactReset))
	{
		formatf(ragdollStateString, "Blocking activation from any ped impacts (reset flag)");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromVehicleImpact))
	{
		formatf(ragdollStateString, "Blocking activation from vehicle impacts");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromBulletImpact))
	{
		formatf(ragdollStateString, "Blocking activation from bullet impacts");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromRubberBullet))
	{
		formatf(ragdollStateString, "Blocking activation from rubber bullet impacts");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFire))
	{
		formatf(ragdollStateString, "Blocking activation from fire");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions))
	{
		formatf(ragdollStateString, "Blocking activation from explosions");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromElectrocution))
	{
		formatf(ragdollStateString, "Blocking activation from electrocution");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_BlockWeaponReactionsUnlessDead))
	{
		formatf(ragdollStateString, "Blocking weapon reactions until dead");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromImpactObject))
	{
		formatf(ragdollStateString, "Blocking activation from impact object");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromMelee))
	{
		formatf(ragdollStateString, "Blocking activation from melee");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromWaterJet))
	{
		formatf(ragdollStateString, "Blocking activation from water jets");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFalling))
	{
		formatf(ragdollStateString, "Blocking activation from falling");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromDrowning))
	{
		formatf(ragdollStateString, "Blocking activation from drowning");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}
	if (ped.GetPedConfigFlag(CPED_CONFIG_FLAG_AllowBlockDeadPedRagdollActivation))
	{
		formatf(ragdollStateString, "Allowing blocking of ragdoll activation when ped is dead");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}

	// Kinematic physics mode
	formatf(ragdollStateString, "CanUseKinematicPhysics : %s", ped.CanUseKinematicPhysics() ? "yes" : "no");
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	formatf(ragdollStateString, "IsUsingKinematicPhysics : %s", ped.IsUsingKinematicPhysics() ? "yes" : "no");
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	// Gravity switched off (aka can extract Z)
	formatf(ragdollStateString, "UseExtZVel : %s", ped.GetUseExtractedZ() ? "on" : "off");
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	// bUsesCollision/bIsFixed
	formatf(ragdollStateString, "Collision: %s", ped.IsCollisionEnabled() ? "on" : "off");
	grcDebugDraw::Text( WorldCoors, ped.IsCollisionEnabled()? colour : Color_red, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	Color32 fixedColour = colour;
	char szFixedState[128];
	if ( ped.IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION) )
	{
		fixedColour = Color_red;
		formatf(szFixedState, "Fixed waiting for collision!");
	}
	else if( ped.IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) )
	{
		fixedColour = Color_red;
		formatf(szFixedState, "Fixed by network!");
	}
	else if( ped.IsBaseFlagSet(fwEntity::IS_FIXED) )
	{
		fixedColour = Color_red;
		formatf(szFixedState, "Fixed by code/script!");
	}
	else
	{
		formatf(szFixedState, "Not fixed");
	}
	formatf(ragdollStateString, "Fixed: %s",  szFixedState);
	grcDebugDraw::Text( WorldCoors, fixedColour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	CTaskNMControl* pNMTask = smart_cast<CTaskNMControl*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
	if (pNMTask)
	{
		formatf(ragdollStateString, "Balance failed: %s", pNMTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE) ? "yes" : "no");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}

	formatf(ragdollStateString, "Load collision: %s", ped.ShouldLoadCollision() ? "yes" : "no");
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	formatf(ragdollStateString, "phInst->IsInLevel() : %s", ped.GetCurrentPhysicsInst()->IsInLevel() ? "true" : "false");
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	formatf(ragdollStateString, "phInst->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) : %s", ped.GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) ? "true" : "false");
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	Mat34V pedMat = ped.GetMatrix();
	formatf(ragdollStateString, "Matrix position : x - %.3f, y - %.3f, z - %.3f", VEC3V_ARGS(pedMat.GetCol3()));
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	const phInst* pInst = ped.GetInstForKinematicMode();
	if (pInst->HasLastMatrix())
	{
		const Vec3V lastPos = PHLEVEL->GetLastInstanceMatrix(pInst).GetCol3();
		formatf(ragdollStateString, "Last matrix position : x - %.3f, y - %.3f, z - %.3f", VEC3V_ARGS(lastPos));
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}

	pInst = ped.GetRagdollInst();
	if (pInst->HasLastMatrix())
	{
		const Vec3V lastPos = PHLEVEL->GetLastInstanceMatrix(pInst).GetCol3();
		formatf(ragdollStateString, "Ragdoll Last matrix position : x - %.3f, y - %.3f, z - %.3f", VEC3V_ARGS(lastPos));
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}

	if(ped.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL))
	{
		CTaskNMControl* pTask = smart_cast<CTaskNMControl*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
		if(pTask)
		{
			formatf(ragdollStateString, "Has balance failed? : %s", pTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE) ? "yes" : "no");
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
		}
	}

	if (ped.GetRagdollInst())
	{
		// Ragdoll LOD
		const char * ragDollLODNames[fragInst::PHYSICS_LOD_MAX] =
		{
			"HIGH",
			"MEDIUM",
			"LOW"
		};
		formatf(ragdollStateString, "Ragdoll LOD : %s", ragDollLODNames[ped.GetRagdollInst()->GetCurrentPhysicsLOD()]);
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;

		// NM Art asset ID
		formatf(ragdollStateString, "NM Art Asset ID : %s (%d)",
			ped.GetRagdollInst()->GetARTAssetID() == -1 ? "non-NM physics rig" :
			ped.GetRagdollInst()->GetARTAssetID() == 0 ? "ragdoll_type_male" :
			ped.GetRagdollInst()->GetARTAssetID() == 1 ? "ragdoll_type_female" :
			ped.GetRagdollInst()->GetARTAssetID() == 2 ? "ragdoll_type_male_large" :
			"unknown art asset", ped.GetRagdollInst()->GetARTAssetID());
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;

		if (ped.GetRagdollState()==RAGDOLL_STATE_PHYS)
		{
			// NM agent?
			formatf(ragdollStateString, "NM Agent / Rage Ragdoll : %s", ped.GetRagdollInst()->GetNMAgentID() != -1 ? "NM Agent" : "Rage Ragdoll");
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;

			// Time as a ragdoll
			u32 activationTime = ped.GetRagdollInst()->GetActivationStartTime();
			Assert(activationTime > 0 && (float)(fwTimer::GetTimeInMilliseconds() - activationTime) >= 0);
			formatf(ragdollStateString, "Time Spent As Ragdoll : %.2f", ((float)(fwTimer::GetTimeInMilliseconds() - activationTime)) / 1000.0f);
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
		}

		formatf(ragdollStateString, "Is Prone : %s", ped.IsProne() ? "TRUE" : "FALSE");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;

#if LAZY_RAGDOLL_BOUNDS_UPDATE
		// Are the ragdoll bounds up to date?
		formatf(ragdollStateString, "Ragdoll bounds up to date : %s", ped.AreRagdollBoundsUpToDate() ? "TRUE" : "FALSE");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;

		// Frames requested for ragdoll bounds update
		formatf(ragdollStateString, "Requested ragdoll bounds update frames : %u", ped.GetRagdollBoundsUpdateRequestedFrames());
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
#endif // LAZY_RAGDOLL_BOUNDS_UPDATE

		// Ragdoll collision mask
		fragInstNMGta* pRagdollInst = ped.GetRagdollInst();
		if (pRagdollInst)
		{
			// Ragdoll mask
			const bool bRAGDOLL_BUTTOCKS				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_BUTTOCKS)) ? true : false;
			const bool bRAGDOLL_THIGH_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_THIGH_LEFT)) ? true : false;
			const bool bRAGDOLL_SHIN_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SHIN_LEFT)) ? true : false;
			const bool bRAGDOLL_FOOT_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_FOOT_LEFT)) ? true : false;
			const bool bRAGDOLL_THIGH_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_THIGH_RIGHT)) ? true : false;
			const bool bRAGDOLL_SHIN_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SHIN_RIGHT)) ? true : false;
			const bool bRAGDOLL_FOOT_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_FOOT_RIGHT)) ? true : false;
			const bool bRAGDOLL_SPINE0					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE0)) ? true : false;
			const bool bRAGDOLL_SPINE1					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE1)) ? true : false;
			const bool bRAGDOLL_SPINE2					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE2)) ? true : false;
			const bool bRAGDOLL_SPINE3					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_SPINE3)) ? true : false;
			const bool bRAGDOLL_CLAVICLE_LEFT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_CLAVICLE_LEFT)) ? true : false;
			const bool bRAGDOLL_UPPER_ARM_LEFT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_UPPER_ARM_LEFT)) ? true : false;
			const bool bRAGDOLL_LOWER_ARM_LEFT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_LOWER_ARM_LEFT)) ? true : false;
			const bool bRAGDOLL_HAND_LEFT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_HAND_LEFT)) ? true : false;
			const bool bRAGDOLL_CLAVICLE_RIGHT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_CLAVICLE_RIGHT)) ? true : false;
			const bool bRAGDOLL_UPPER_ARM_RIGHT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_UPPER_ARM_RIGHT)) ? true : false;
			const bool bRAGDOLL_LOWER_ARM_RIGHT			= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_LOWER_ARM_RIGHT)) ? true : false;
			const bool bRAGDOLL_HAND_RIGHT				= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_HAND_RIGHT)) ? true : false;
			const bool bRAGDOLL_NECK					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_NECK)) ? true : false;
			const bool bRAGDOLL_HEAD					= (pRagdollInst->GetDisableCollisionMask() & BIT(RAGDOLL_HEAD)) ? true : false;

			const bool bAtLeastOneFlagSet				= ( bRAGDOLL_BUTTOCKS || bRAGDOLL_THIGH_LEFT || bRAGDOLL_SHIN_LEFT || bRAGDOLL_FOOT_LEFT || bRAGDOLL_THIGH_RIGHT || bRAGDOLL_SHIN_RIGHT || bRAGDOLL_FOOT_RIGHT ||
															bRAGDOLL_SPINE0 || bRAGDOLL_SPINE1 || bRAGDOLL_SPINE2 || bRAGDOLL_SPINE3 || bRAGDOLL_CLAVICLE_LEFT || bRAGDOLL_UPPER_ARM_LEFT || bRAGDOLL_LOWER_ARM_LEFT ||
															bRAGDOLL_HAND_LEFT || bRAGDOLL_CLAVICLE_RIGHT || bRAGDOLL_UPPER_ARM_RIGHT || bRAGDOLL_LOWER_ARM_RIGHT || bRAGDOLL_HAND_RIGHT || bRAGDOLL_NECK || bRAGDOLL_HEAD) ? true : false;
			formatf(ragdollStateString, "---------- Ragdoll Disabled Collision Mask ----------");
			grcDebugDraw::Text( WorldCoors, bAtLeastOneFlagSet ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
			formatf(ragdollStateString, "%s | %s | %s | %s",
				bRAGDOLL_BUTTOCKS			? "RAGDOLL_BUTTOCKS" : "NULL",
				bRAGDOLL_THIGH_LEFT			? "RAGDOLL_THIGH_LEFT" : "NULL",
				bRAGDOLL_SHIN_LEFT			? "RAGDOLL_SHIN_LEFT" : "NULL",
				bRAGDOLL_FOOT_LEFT			? "RAGDOLL_FOOT_LEFT" : "NULL");
			grcDebugDraw::Text( WorldCoors, bAtLeastOneFlagSet ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;

			formatf(ragdollStateString, "%s | %s | %s | %s",
				bRAGDOLL_THIGH_RIGHT		? "RAGDOLL_THIGH_RIGHT" : "NULL",
				bRAGDOLL_SHIN_RIGHT			? "RAGDOLL_SHIN_RIGHT" : "NULL",
				bRAGDOLL_FOOT_RIGHT			? "RAGDOLL_FOOT_RIGHT" : "NULL",
				bRAGDOLL_SPINE0				? "RAGDOLL_SPINE0" : "NULL");
			grcDebugDraw::Text( WorldCoors, bAtLeastOneFlagSet ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;

			formatf(ragdollStateString, "%s | %s | %s | %s",
				bRAGDOLL_SPINE1				? "RAGDOLL_SPINE1" : "NULL",
				bRAGDOLL_SPINE2				? "RAGDOLL_SPINE2" : "NULL",
				bRAGDOLL_SPINE3				? "RAGDOLL_SPINE3" : "NULL",
				bRAGDOLL_CLAVICLE_LEFT		? "RAGDOLL_CLAVICLE_LEFT" : "NULL");
			grcDebugDraw::Text( WorldCoors, bAtLeastOneFlagSet ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;

			formatf(ragdollStateString, "%s | %s | %s | %s",
				bRAGDOLL_UPPER_ARM_LEFT		? "RAGDOLL_UPPER_ARM_LEFT" : "NULL",
				bRAGDOLL_LOWER_ARM_LEFT		? "RAGDOLL_LOWER_ARM_LEFT" : "NULL",
				bRAGDOLL_HAND_LEFT			? "RAGDOLL_HAND_LEFT" : "NULL",
				bRAGDOLL_CLAVICLE_RIGHT		? "RAGDOLL_CLAVICLE_RIGHT" : "NULL");
			grcDebugDraw::Text( WorldCoors, bAtLeastOneFlagSet ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;

			formatf(ragdollStateString, "%s | %s | %s | %s | %s",
				bRAGDOLL_UPPER_ARM_RIGHT	? "RAGDOLL_UPPER_ARM_RIGHT" : "NULL",
				bRAGDOLL_LOWER_ARM_RIGHT	? "RAGDOLL_LOWER_ARM_RIGHT" : "NULL",
				bRAGDOLL_HAND_RIGHT			? "RAGDOLL_HAND_RIGHT" : "NULL",
				bRAGDOLL_NECK				? "RAGDOLL_NECK" : "NULL",
				bRAGDOLL_HEAD				? "RAGDOLL_HEAD" : "NULL");
			grcDebugDraw::Text( WorldCoors, bAtLeastOneFlagSet ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
			formatf(ragdollStateString, "---------------------------------------------------");
			grcDebugDraw::Text( WorldCoors, bAtLeastOneFlagSet ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
		}
	}

	if (ped.GetActivateRagdollOnCollision())
	{
		formatf(ragdollStateString, "Activate ragdoll on collision enabled - event:%s", ped.GetActivateRagdollOnCollisionEvent()? ped.GetActivateRagdollOnCollisionEvent()->GetName() : "none");
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
		formatf(ragdollStateString, "Allowed penetration: %.3f, Allowed slope: %.3f, Allowed parts: %d", ped.GetRagdollOnCollisionAllowedPenetration(), ped.GetRagdollOnCollisionAllowedSlope(), ped.GetRagdollOnCollisionAllowedPartsMask());
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
		formatf(ragdollStateString, "Ignore Physical: (%s) - 0x%p", ped.GetRagdollOnCollisionIgnorePhysical() ? ped.GetRagdollOnCollisionIgnorePhysical()->GetModelName() : "NULL", ped.GetRagdollOnCollisionIgnorePhysical());
		grcDebugDraw::Text( WorldCoors, ped.GetRagdollOnCollisionIgnorePhysical() ? Color_red : colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}

	if (CTaskNMBehaviour::m_pTuningSetHistoryPed==&ped)
	{
		for (s32 i=CTaskNMBehaviour::m_TuningSetHistory.GetCount()-1; i>-1; i--)
		{
			formatf(ragdollStateString, "%s: %.3f", CTaskNMBehaviour::m_TuningSetHistory[i].id.GetCStr(), ((float)(fwTimer::GetTimeInMilliseconds() - CTaskNMBehaviour::m_TuningSetHistory[i].time))/1000.0f);
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
			iNoOfTexts++;
		}
	}

	if(CPedDebugVisualiserMenu::GetFocusPed()==&ped)
	{
		VisualiseTasks(ped);
	}

	// Does this entity already have a scene update extension?
	const fwSceneUpdateExtension *extension = ped.GetExtension<fwSceneUpdateExtension>();

	if (extension)
	{
		formatf(ragdollStateString, "Scene update flags: %d", extension->m_sceneUpdateFlags);
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
		iNoOfTexts++;
	}

	formatf(ragdollStateString, "Accumulated sideswipe magnitude: %.4f", ped.GetAccumulatedSideSwipeImpulseMag());
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	formatf(ragdollStateString, "Time first under another ragdoll: %d", ped.GetTimeOfFirstBeingUnderAnotherRagdoll());
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), ragdollStateString);
	iNoOfTexts++;

	return true;
}

bool CPedDebugVisualiser::VisualiseMovement(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	CMovementTextDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
	tempDebugInfo.Visualise();

	if(CPedDebugVisualiserMenu::GetFocusPed()==&ped)
	{
		VisualiseTasks(ped);
	}
	return true;
}

bool CPedDebugVisualiser::VisualiseQueriableState(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	int iNoOfTexts=0;

	//***************************************************************************
	//	Display the task heirarchy for this ped
	//***************************************************************************
	iAlpha -= 60;
	char tmp[256];
	Color32 colour = CRGBA(64,255,64,iAlpha);
	while( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskType(iNoOfTexts) != -1 )
	{
		if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskPriority(iNoOfTexts) <= PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP )
		{
			colour = CRGBA(255,255,255,iAlpha);
			sprintf( tmp, "T:");
		}
		else if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskPriority(iNoOfTexts) == PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP )
		{
			colour = CRGBA(255,255,64,iAlpha);
			sprintf( tmp, "ER:");
		}
		else if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskPriority(iNoOfTexts) == PED_TASK_PRIORITY_PRIMARY )
		{
			colour = CRGBA(64,64,255,iAlpha);
			sprintf( tmp, "SC:");
		}
		else if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskPriority(iNoOfTexts) == PED_TASK_PRIORITY_DEFAULT )
		{
			colour = CRGBA(64,255,64,iAlpha);
			sprintf( tmp, "D:");
		}

		strcat(tmp, TASKCLASSINFOMGR.GetTaskName( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskType(iNoOfTexts) ) );

		if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskInfoType(iNoOfTexts) == CTaskInfo::INFO_TYPE_NONE )
		{
		}
		else if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskInfoType(iNoOfTexts) == CTaskInfo::INFO_TYPE_SEQUENCE )
		{
			strcat( tmp, "(SEQ)" );
		}
		else if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskInfoType(iNoOfTexts) == CTaskInfo::INFO_TYPE_TARGET )
		{
			strcat( tmp, "(TAR)" );
		}
		else if( ped.GetPedIntelligence()->GetQueriableInterface()->GetDebugTaskInfoType(iNoOfTexts) == CTaskInfo::INFO_TYPE_CAR_MISSION )
		{
			strcat( tmp, "(CAR)" );
		}

		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
		iNoOfTexts++;
	}

	if( ped.IsPlayer() )
	{
		sprintf( tmp, "WL %d", ped.GetPlayerWanted()->GetWantedLevel() );
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), tmp);
		iNoOfTexts++;
	}

	if( iNoOfTexts == 0 )
	{
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "No Queriable state");
		iNoOfTexts++;
	}

	return (iNoOfTexts > 0);
}


bool CPedDebugVisualiser::VisualiseAmbientAnimations(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return false;
	}

	int iNoOfTexts=0;

	//***************************************************************************
	//	Display the task heirarchy for this ped
	//***************************************************************************

	Color32 colour = CRGBA(64,64,255,iAlpha);
	iAlpha -= 60;


	if( ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS) )
	{
#if __DEV
		char debugText[128];

		CTaskAmbientClips* pTask = (CTaskAmbientClips*) ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS);

		if ( pTask )
		{
			const CConditionalAnimsGroup * pConditionalAnimsGroup = pTask->GetConditionalAnimsGroup();

			if(Verifyf(pConditionalAnimsGroup,"No conditional anim group for task?"))
			{
				if ( pConditionalAnimsGroup->GetHash() != 0 )
				{
					formatf( debugText, pConditionalAnimsGroup->GetName());
				}
				grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
				iNoOfTexts++;

				grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
				iNoOfTexts++;

				// Idle animation
				colour = CRGBA(255,255,255,iAlpha);
				if( pTask->GetClipHelper() )
				{
					formatf( debugText, "Idle(%s, %s)", pTask->GetClipHelper()->GetClipDictionaryMetadata().GetCStr(), pTask->GetClipHelper()->GetClipName() );
				}
				else
				{
					formatf( debugText, "No idle anim" );
				}
				grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
				iNoOfTexts++;

				colour = CRGBA(64,255,64,iAlpha);

				for( s32 i = 0; i < pConditionalAnimsGroup->GetNumAnims(); i++ )
				{
					const CConditionalAnims * pAnims = pConditionalAnimsGroup->GetAnims(i);
					s32 iFirstFailureCondition;
					//float fPriority = 0;
						//CCachedState cachedState;
					CScenarioCondition::sScenarioConditionData conditionData;
					conditionData.pPed = &ped;
					const bool bGroupValid = pAnims->CheckConditions(conditionData, &iFirstFailureCondition );
					if( !bGroupValid )
					{
						colour = CRGBA(255,64,64,iAlpha);
					}

					const CConditionalAnims::ConditionalClipSetArray * pBaseAnimsArray = pAnims->GetClipSetArray(CConditionalAnims::AT_BASE);
					if ( pBaseAnimsArray )
					{
						for ( u32 i=0; i<pBaseAnimsArray->size(); i++ )
						{
							const CConditionalClipSet * pClipSet = pBaseAnimsArray->GetElements()[i];
							if (pClipSet && pClipSet->GetClipSetHash() != 0)
							{
								formatf( debugText, "%s Base Anim",pClipSet->GetClipSetName());
								grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
								iNoOfTexts++;
							}
						}
					}
					const CConditionalAnims::ConditionalClipSetArray * pIntroAnimsArray = pAnims->GetClipSetArray(CConditionalAnims::AT_ENTER);
					if ( pIntroAnimsArray )
					{
						for ( u32 i=0; i<pIntroAnimsArray->size(); i++ )
						{
							const CConditionalClipSet * pClipSet = pIntroAnimsArray->GetElements()[i];
							if (pClipSet && pClipSet->GetClipSetHash() != 0)
							{
								formatf( debugText, "%s Intro Anim",pClipSet->GetClipSetName());
								grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
								iNoOfTexts++;
							}
						}
					}
					const CConditionalAnims::ConditionalClipSetArray * pOutroAnimsArray = pAnims->GetClipSetArray(CConditionalAnims::AT_EXIT);
					if ( pOutroAnimsArray )
					{
						for ( u32 i=0; i<pOutroAnimsArray->size(); i++ )
						{
							const CConditionalClipSet * pClipSet = pOutroAnimsArray->GetElements()[i];
							if (pClipSet && pClipSet->GetClipSetHash() != 0)
							{
								formatf( debugText, "%s Outro Anim",pClipSet->GetClipSetName());
								grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
								iNoOfTexts++;
							}
						}
					}
					const CConditionalAnims::ConditionalClipSetArray * pVariationAnimsArray = pAnims->GetClipSetArray(CConditionalAnims::AT_VARIATION);
					if ( pVariationAnimsArray )
					{
						for ( u32 i=0; i<pVariationAnimsArray->size(); i++ )
						{
							const CConditionalClipSet * pClipSet = pVariationAnimsArray->GetElements()[i];
							if (pClipSet && pClipSet->GetClipSetHash() != 0)
							{
								formatf( debugText, "%s Variation Anim",pClipSet->GetClipSetName());
								grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
								iNoOfTexts++;
							}
						}
					}
				}
			}
		}
#endif // __DEV
	}


	if( iNoOfTexts == 1 )
	{
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "No available ambients");
		iNoOfTexts++;
	}

	return (iNoOfTexts > 0);
}

// Visualise the player ped's scoring, unless there is a focus ped then visualise that ped's scoring.
bool CPedDebugVisualiser::VisualiseLookAbout(const CEntity& entity) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(entity, WorldCoors, iAlpha))
	{
		return false;
	}

	char debugText[20];
	int iNoOfTexts=0;

	Color32 colour = CRGBA(64,64,255,iAlpha);

	CPed* pPed = CPedDebugVisualiserMenu::GetFocusPed();
	float fScore = 0.0f;

	if (pPed && !pPed->IsAPlayerPed())
	{
		fScore = CAmbientLookAt::ScoreInterestingLookAt(pPed, &entity);
	}
	else
	{
		fScore = CAmbientLookAt::ScoreInterestingLookAtPlayer(FindPlayerPed(), &entity);
	}


	formatf( debugText, "Score %f", fScore );
	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	return (iNoOfTexts > 0);
}

void CPedDebugVisualiser::VisualiseScanners(const CPed & ped) const
{
	// If there is no focus ped then render every peds' event scanners
	// Otherwise render only the focus ped's
	if(!CPedDebugVisualiserMenu::GetFocusPed() || CPedDebugVisualiserMenu::GetFocusPed()==&ped)
	{
		CPedIntelligence * pPedAi = ped.GetPedIntelligence();

		// Render nearest entity lists
		pPedAi->GetPedScanner()->DebugRender(Color_red);
		pPedAi->GetVehicleScanner()->DebugRender(Color_blue);
		pPedAi->GetObjectScanner()->DebugRender(Color_green);
	}
}

void CPedDebugVisualiser::VisualiseAvoidance(const CPed & ped) const
{
	if(ped.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
		return;

	CPedIntelligence * pPedAi = ped.GetPedIntelligence();

	CTask * pMoveTask = pPedAi->GetActiveSimplestMovementTask();
	if(!pMoveTask ||
		(pMoveTask->GetTaskType() != CTaskTypes::TASK_MOVE_GO_TO_POINT &&
		pMoveTask->GetTaskType() != CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE))
		return;

	CTaskMoveGoToPoint * pGoToTask = (CTaskMoveGoToPoint*)pMoveTask;
	if(!pGoToTask->m_pSeparationDebug)
		return;

	const float fArrowSize = 0.25f;
	const float fImpulseScale = 2.0f;

	const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	int n;
	for(n=0; n<pGoToTask->m_pSeparationDebug->m_iNumNearbyPeds; n++)
	{
		Vector3 vVec(pGoToTask->m_pSeparationDebug->m_vImpulses[n].x, pGoToTask->m_pSeparationDebug->m_vImpulses[n].y, 0.0f);
		grcDebugDraw::Line(vecPedPosition, vecPedPosition + (vVec*fImpulseScale), Color_red);
	}
	for(n=0; n<pGoToTask->m_pSeparationDebug->m_iNumNearbyObjects; n++)
	{
		Vector3 vVec(pGoToTask->m_pSeparationDebug->m_vObjImpulses[n].x, pGoToTask->m_pSeparationDebug->m_vObjImpulses[n].y, 0.0f);
		grcDebugDraw::Line(vecPedPosition, vecPedPosition + (vVec*fImpulseScale), Color_purple);
	}

	if(pGoToTask->m_pSeparationDebug->m_vToTarget.Mag2())
		grcDebugDraw::Line(vecPedPosition, vecPedPosition + pGoToTask->m_pSeparationDebug->m_vToTarget, Color_green );

	if(pGoToTask->m_pSeparationDebug->m_vCurrentTarget.Mag2())
	{
		Vector3 vToSeparationTarget = pGoToTask->m_pSeparationDebug->m_vCurrentTarget - vecPedPosition;
		vToSeparationTarget.Normalize();

		Color32 col = (ped.GetNavMeshTracker().GetWasAvoidanceTestClearLeft() || ped.GetNavMeshTracker().GetWasAvoidanceTestClearRight()) ? Color_blue : Color_red;
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vecPedPosition), VECTOR3_TO_VEC3V(vecPedPosition+vToSeparationTarget), fArrowSize, col);
	}
	if(pGoToTask->m_pSeparationDebug->m_vPullToRoute.Mag2())
	{
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vecPedPosition), VECTOR3_TO_VEC3V(vecPedPosition + pGoToTask->m_pSeparationDebug->m_vPullToRoute), fArrowSize, Color_NavajoWhite );
	}
}

void CPedDebugVisualiser::VisualiseNavigation(const CPed & ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	CPedIntelligence * pPedAi = ped.GetPedIntelligence();
	const CPedNavCapabilityInfo & navCaps = pPedAi->GetNavCapabilities();

	char txt[256];
	Color32 col = Color_yellow;
	s32 iFlag = 1;
	s32 iY = -10 * grcDebugDraw::GetScreenSpaceTextHeight();
	bool bQuit = false;

	while(!bQuit)
	{
		const char * pFlagName = NULL;
		switch(iFlag)
		{
			case CPedNavCapabilityInfo::FLAG_MAY_CLIMB:
				pFlagName = "FLAG_MAY_CLIMB";
				break;
			case CPedNavCapabilityInfo::FLAG_MAY_DROP:
				pFlagName = "FLAG_MAY_DROP";
				break;
			case CPedNavCapabilityInfo::FLAG_MAY_JUMP:
				pFlagName = "FLAG_MAY_JUMP";
				break;
			case CPedNavCapabilityInfo::FLAG_MAY_ENTER_WATER:
				pFlagName = "FLAG_MAY_ENTER_WATER";
				break;
			case CPedNavCapabilityInfo::FLAG_MAY_USE_LADDERS:
				pFlagName = "FLAG_MAY_USE_LADDERS";
				break;
			case CPedNavCapabilityInfo::FLAG_MAY_FLY:
				pFlagName = "FLAG_MAY_FLY";
				break;
			case CPedNavCapabilityInfo::FLAG_AVOID_OBJECTS:
				pFlagName = "FLAG_AVOID_OBJECTS";
				break;
			case CPedNavCapabilityInfo::FLAG_USE_MASS_THRESHOLD:
				pFlagName = "FLAG_USE_MASS_THRESHOLD";
				break;
			case CPedNavCapabilityInfo::FLAG_PREFER_TO_AVOID_WATER:
				pFlagName = "FLAG_PREFER_TO_AVOID_WATER";
				break;
			case CPedNavCapabilityInfo::FLAG_AVOID_FIRE:
				pFlagName = "FLAG_AVOID_FIRE";
				break;
			case CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE:
				pFlagName = "FLAG_PREFER_NEAR_WATER_SURFACE";
				break;
			case CPedNavCapabilityInfo::FLAG_SEARCH_FOR_PATHS_ABOVE_PED:
				pFlagName = "FLAG_SEARCH_FOR_PATHS_ABOVE_PED";
				break;
			case CPedNavCapabilityInfo::FLAG_NEVER_STOP_NAVMESH_TASK_EARLY_IN_FOLLOW_LEADER:
				pFlagName = "FLAG_NEVER_STOP_NAVMESH_TASK_EARLY_IN_FOLLOW_LEADER";
				break;
			default:
				bQuit = true;
				break;
		}

		if(pFlagName)
		{
			sprintf(txt, "%s : %s", pFlagName, navCaps.IsFlagSet( static_cast<CPedNavCapabilityInfo::NavCapabilityFlags>(iFlag)) ? "true":"false");
			grcDebugDraw::Text(WorldCoors, col, 0, iY, txt);
			iY += grcDebugDraw::GetScreenSpaceTextHeight();
			iFlag = (iFlag << 1);
		}
	}

	iY += grcDebugDraw::GetScreenSpaceTextHeight();

	// Find CTaskNavBase derived movement task to debug
	CTask * pTask = pPedAi->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
	if( pTask == NULL)
		pTask = pPedAi->FindMovementTaskByType(CTaskTypes::TASK_MOVE_WANDER);

	if( pTask == NULL)
	{
		// I've hijacked this mode to also display CTaskFollowWaypointRecording in some more detail
		pTask = pPedAi->FindTaskActiveByType(CTaskTypes::TASK_FOLLOW_WAYPOINT_RECORDING);
		if(pTask)
		{
			CTaskFollowWaypointRecording * pWptTask = (CTaskFollowWaypointRecording*)pTask;
			pWptTask->DebugVisualiserDisplay(&ped, WorldCoors, iY);
			return;
		}
	}

	if(pTask == NULL)
		return;

	CTaskNavBase * pNavMeshTask = (CTaskNavBase*)pTask;

	//*********************************************************************************************************

	// Color32 colour = CRGBA(64,64,255,iAlpha);
	iAlpha -= 60;

	//*********************************************************************************************************

	pNavMeshTask->DebugVisualiserDisplay(&ped, WorldCoors, iY);

	//*********************************************************************************************************
}

void CPedDebugVisualiser::VisualiseScriptHistory(const CPed& DEV_ONLY(ped)) const
{
#if __DEV//def CAM_DEBUG

	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	CScriptHistoryDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
#endif // __DEV
}

void CPedDebugVisualiser::VisualiseTaskHistory(const CPed& DEV_ONLY(ped)) const
{
#if __DEV//def CAM_DEBUG
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	int iNoOfTexts=0;

	Color32 colour = Color_green;
	iAlpha -= 60;

	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "Task history" );
	iNoOfTexts++;

	for( s32 i = 0; i < CTaskTree::MAX_TASK_HISTORY; i++ )
	{
		if( ped.GetPedIntelligence()->GetTaskManager()->GetHistory(PED_TASK_TREE_PRIMARY, i) != CTaskTypes::TASK_NONE )
		{
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), TASKCLASSINFOMGR.GetTaskName(ped.GetPedIntelligence()->GetTaskManager()->GetHistory(PED_TASK_TREE_PRIMARY, i)));
			iNoOfTexts++;
		}
	}
#endif // __DEV
}

void CPedDebugVisualiser::VisualiseCurrentEvents(const CPed& DEV_ONLY(ped)) const
{
#if __DEV//def CAM_DEBUG
	Vector3 WorldCoors;
	u8 iAlphaNotUsed = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlphaNotUsed))
	{
		return;
	}

	CEventsDebugInfo tempDebugInfo(&ped, CEventsDebugInfo::LF_CURRENT_EVENTS | CEventsDebugInfo::LF_DECISION_MAKER, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, 255));
	tempDebugInfo.Print();

#endif // __DEV
}

#if DEBUG_EVENT_HISTORY

namespace // anonymous namespace (DEBUG_EVENT_HISTORY internals)
{
std::pair<Vec3V, u32>& RetrieveEventHistoryPositionInfo(atArray<std::pair<Vec3V, u32> >& aPositionInfos, Vec3V_In vPosition)
{
	std::pair<Vec3V, u32>* pPositionInfo = NULL;

	const u32 uNumPositions = aPositionInfos.GetCount();
	for (u32 uPositionIdx = 0; uPositionIdx < uNumPositions; ++uPositionIdx)
	{
		std::pair<Vec3V, u32>& rPositionNumEvents = aPositionInfos[uPositionIdx];

		static const ScalarV scSAME_POSITION_MAX_DISTANCE_SQUARED(0.1f);
		if (IsLessThanAll(DistSquared(rPositionNumEvents.first, vPosition), scSAME_POSITION_MAX_DISTANCE_SQUARED))
		{
			pPositionInfo = &rPositionNumEvents;
			++pPositionInfo->second;
			break;
		}
	}

	if (!pPositionInfo)
	{
		pPositionInfo = &aPositionInfos.Append();
		pPositionInfo->first = vPosition;
		pPositionInfo->second = 0u;
	}

	return *pPositionInfo;
}

void DrawEventHistoryEntryAtPosition(const CPed& rPed, fwFlags32 uLogFlags, atArray<std::pair<Vec3V, u32> >& aPositionInfos, const CPedIntelligence::SEventHistoryEntry& rEventHEntry)
{
	std::pair<Vec3V, u32>& rPositionInfo = RetrieveEventHistoryPositionInfo(aPositionInfos, rEventHEntry.vPosition);

	static const float fEVENT_SPHERE_RADIUS = 0.2f;
	static const float fEVENT_SAME_POSITION_OFFSET = 0.025f;
	Vec3V vEventSourcePos = rEventHEntry.vPosition - Vec3V(V_Z_AXIS_WZERO) * ScalarV(rPositionInfo.second * fEVENT_SAME_POSITION_OFFSET);

	Color32 stateColor = CEventsDebugInfo::GetEventStateColor(rEventHEntry.GetState());
	grcDebugDraw::Line(rPed.GetTransform().GetPosition(), vEventSourcePos, stateColor);

	grcDebugDraw::Sphere(vEventSourcePos, fEVENT_SPHERE_RADIUS, stateColor, false);

	static const u32 uBUFFER_SIZE = 256;
	char szBuffer[uBUFFER_SIZE];

	if (CPedDebugVisualiserMenu::ms_bShowFocusedPedsEventPositionsStateInfo)
	{
		const CPedIntelligence* pPedIntelligence = rPed.GetPedIntelligence();

		grcDebugDraw::Text(rEventHEntry.vPosition + Vec3V(V_Z_AXIS_WZERO) * ScalarV(fEVENT_SPHERE_RADIUS), stateColor, 0, rPositionInfo.second*grcDebugDraw::GetScreenSpaceTextHeight(), CEventsDebugInfo::GetEventHistoryEntryDescription(*pPedIntelligence, rEventHEntry).c_str());
		++rPositionInfo.second;
		grcDebugDraw::Text(rEventHEntry.vPosition + Vec3V(V_Z_AXIS_WZERO) * ScalarV(fEVENT_SPHERE_RADIUS), stateColor, 0, rPositionInfo.second*grcDebugDraw::GetScreenSpaceTextHeight(), CEventsDebugInfo::GetEventHistoryEntryStateDescription(rEventHEntry, uLogFlags).c_str());
	}
	else
	{
		snprintf(szBuffer, uBUFFER_SIZE, "%d, %s", rEventHEntry.uCreatedTS, rEventHEntry.sEventDescription.c_str());
		grcDebugDraw::Text(rEventHEntry.vPosition + Vec3V(V_Z_AXIS_WZERO) * ScalarV(fEVENT_SPHERE_RADIUS), stateColor, 0, rPositionInfo.second*grcDebugDraw::GetScreenSpaceTextHeight(), szBuffer);
	}
}

} // anonymous namespace (DEBUG_EVENT_HISTORY internals)

#endif // DEBUG_EVENT_HISTORY

void CPedDebugVisualiser::VisualiseEventHistory(const CPed& DEBUG_EVENT_HISTORY_ONLY(ped)) const
{
#if DEBUG_EVENT_HISTORY
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	const bool bIsEntityFocused = CDebugScene::FocusEntities_IsInGroup(&ped);
	if(bIsEntityFocused)
	{
		iAlpha = 255;
	}

	if(iAlpha==0)
		return;
	
	u32 uLogFlags = CEventsDebugInfo::LF_PED_ID | CEventsDebugInfo::LF_EVENTS_HISTORY | CEventsDebugInfo::LF_EVENTS_HISTORY_TASKS_TRANSITIONS;
	if (CPedDebugVisualiserMenu::ms_bShowRemovedEvents)
	{
		uLogFlags |= CEventsDebugInfo::LF_EVENTS_HISTORY_SHOW_REMOVED_EVENTS;
	}

	if (!bIsEntityFocused || !CPedDebugVisualiserMenu::ms_bHideFocusedPedsEventHistoryText)
	{
		CEventsDebugInfo tempDebugInfo(&ped, uLogFlags, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
		tempDebugInfo.Print();
	}
	else
	{
		CEventsDebugInfo tempDebugInfo(&ped, CEventsDebugInfo::LF_PED_ID, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
		tempDebugInfo.Print();
	}

	// Only render the extra information for the focus entities, otherwise we'd have too much info
	if(bIsEntityFocused && CPedDebugVisualiserMenu::ms_bShowFocusedPedsEventPositions)
	{
		CPedIntelligence* pPedIntelligence = ped.GetPedIntelligence();
		if (pPedIntelligence)
		{
			atArray<std::pair<Vec3V, u32> > aPositionInfos(0, CPedIntelligence::uEVENT_HISTORY_MAX_NUM_EVENTS);

			const u32 uNumEvents = pPedIntelligence->GetEventHistoryCount();
			for (u32 uEventIdx = 0; uEventIdx < uNumEvents; ++uEventIdx)
			{
				const CPedIntelligence::SEventHistoryEntry& rEventHEntry = pPedIntelligence->GetEventHistoryEntryAt(uEventIdx);
				if (CPedDebugVisualiserMenu::ms_bShowRemovedEvents || rEventHEntry.GetState() != CPedIntelligence::SEventHistoryEntry::SState::REMOVED)
				{
					DrawEventHistoryEntryAtPosition(ped, uLogFlags, aPositionInfos, rEventHEntry);
				}
			}
		}
	}

#endif // DEBUG_EVENT_HISTORY
}

void CPedDebugVisualiser::VisualiseAudioContexts(const CPed& DEV_ONLY(ped)) const
{
#if __DEV//def CAM_DEBUG
	if( ped.GetSpeechAudioEntity() == NULL )
	{
		return;
	}
	Vector3 WorldCoors;
	u8 iAlphaNotUsed = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlphaNotUsed))
	{
		return;
	}

	int iNoOfTexts=0;

	Color32 colour = Color_green;

	grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "Audio contexts");
	iNoOfTexts++;

	colour = Color_blue;
	// Temp disabled while I change how voice allocation works
	//const_cast<CPed*>(&ped)->GetSpeechAudioEntity()->ChooseFullVoiceIfUndecided();
	s16 iContexts = const_cast<CPed*>(&ped)->GetSpeechAudioEntity()->GetPossibleConversationTopics();
	// Cycle through the various conversation types, and see if we have valid lines of dialogue for that topic.
	const char* conversationLines[] =
	{
		"CONV_TOPIC_SMOKER",
		"CONV_TOPIC_BUSINESS",
		"CONV_TOPIC_BUM",
		"CONV_TOPIC_CONSTRUCTION",
		"CONV_TOPIC_PIMP",
		"CONV_TOPIC_HOOKER",
		"MOBILE_CHAT",
		"TWO_WAY_PHONE_CHAT",
		"CONV_TOPIC_GANG"
	};
	const u32 conversationTopics[] =
	{
		AUD_CONVERSATION_TOPIC_SMOKER,
		AUD_CONVERSATION_TOPIC_BUSINESS,
		AUD_CONVERSATION_TOPIC_BUM,
		AUD_CONVERSATION_TOPIC_CONSTRUCTION,
		AUD_CONVERSATION_TOPIC_PIMP,
		AUD_CONVERSATION_TOPIC_HOOKER,
		AUD_CONVERSATION_TOPIC_MOBILE,
		AUD_CONVERSATION_TOPIC_TWO_WAY,
		AUD_CONVERSATION_TOPIC_GANG
	};

	for (u32 i=0; i<(sizeof(conversationTopics)/sizeof(u32)); i++)
	{
		if( iContexts & conversationTopics[i] )
		{
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), conversationLines[i]);
			iNoOfTexts++;
		}
	}

#endif // __DEV
}

void CPedDebugVisualiser::VisualiseTextViaMenu(const CPed& ped) const
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

	int iNoOfTexts=0;

	Vector3 ScreenCoors, In;

	const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	In.x = vecPedPosition.x;
	In.y = vecPedPosition.y;
	In.z = vecPedPosition.z + 2.0f;

	Color32 colour = Color_green;

	if (m_textDisplayFlags.m_bDisplayAnimList)
	{

	}

	if (m_textDisplayFlags.m_bDisplayTasks)
	{
		CTask* pActiveTask=ped.GetPedIntelligence()->GetTaskActive();
		if(pActiveTask)
		{
			CTask* pTaskToPrint=pActiveTask;
			while(pTaskToPrint)
			{
				grcDebugDraw::Text( In, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), pTaskToPrint->GetName());
				iNoOfTexts++;
				pTaskToPrint=pTaskToPrint->GetSubTask();
			}
		}
		else
		{
			grcDebugDraw::Text( In, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "None");
			iNoOfTexts++;
		}
	}
}

void CPedDebugVisualiser::VisualiseBoundingVolumes(CPed& ped) const
{
	int i;

	Color32 iColRed(0xff,0,0,0xff);
	Color32 iColGreen(0,0xff,0,0xff);

	Vector3 corners[4];
	Vector3 vCentre;

	CEntityBoundAI bound(ped, ped.GetTransform().GetPosition().GetZf(), ped.GetCapsuleInfo()->GetHalfWidth());
	bound.GetCorners(corners);
	bound.GetCentre(vCentre);

	for(i=0;i<4;i++)
	{
		const Vector3 v1=vCentre;
		const Vector3 v2=corners[i];
		grcDebugDraw::Line(v1,v2,iColGreen);
	}

	Vector3 v1=corners[3];
	for(i=0;i<4;i++)
	{
		const Vector3& v2=corners[i];
		grcDebugDraw::Line(v1,v2,iColRed);
		v1=v2;
	}
}

void CPedDebugVisualiser::VisualiseHitSidesToPlayer(CPed& ped) const
{
	const CPed * pPlayerPed = FindPlayerPed();
	int iSide;
	Vector3 dirs[4];
	float dists[4];

	Color32 iColBlue(0,0,0xff,0xff);

	if(ped.GetMyVehicle() && ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		CEntityBoundAI bound(*ped.GetMyVehicle(), ped.GetMyVehicle()->GetTransform().GetPosition().GetZf(), ped.GetCapsuleInfo()->GetHalfWidth());
		iSide = bound.ComputeHitSideByPosition(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()));
		bound.GetPlanes(dirs, dists);

		//		iSide=CPedGeometryAnalyser::ComputeEntityHitSide(*pPlayerPed,*ped.m_pMyVehicle);
		//		CPedGeometryAnalyser::ComputeEntityDirs(*ped.m_pMyVehicle,dirs);
	}
	else
	{
		CEntityBoundAI bound(ped, ped.GetTransform().GetPosition().GetZf(), ped.GetCapsuleInfo()->GetHalfWidth());
		iSide = bound.ComputeHitSideByPosition(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()));
		bound.GetPlanes(dirs, dists);

		//		iSide=CPedGeometryAnalyser::ComputeEntityHitSide(*pPlayerPed, ped);
		//		CPedGeometryAnalyser::ComputeEntityDirs(ped,dirs);
	}

	const Vector3 v1= VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	const Vector3& vDir=dirs[iSide];
	Vector3 v2 = v1 + vDir;

	grcDebugDraw::Line(v1,v2,iColBlue);
}


void CPedDebugVisualiser::VisualiseTasks(const CPed& ped) const
{
	if (CCombatDebug::ms_bRenderPreferredCoverPoints)
	{
		CCombatDebug::RenderPreferredCoverDebugForPed(ped);
	}

    const CTask* pTaskActive=ped.GetPedIntelligence()->GetTaskActive();
    if(pTaskActive && pTaskActive->GetIsFlagSet(aiTaskFlags::HasBegun))
    {
	    pTaskActive->Debug();

		const CTask* pTaskSecondary=ped.GetPedIntelligence()->GetTaskSecondary(0);
		if(pTaskSecondary && pTaskSecondary->GetIsFlagSet(aiTaskFlags::HasBegun))
		{
			pTaskSecondary->Debug();
		}
    }

    const CTask* pTaskMoveActive=ped.GetPedIntelligence()->GetActiveMovementTask();
    //Assertf(pTaskMoveActive, "Ped *must* have a movement task - even if it's MoveDoNothing.");
    if(pTaskMoveActive && pTaskMoveActive->GetIsFlagSet(aiTaskFlags::HasBegun))
    {
	    pTaskMoveActive->Debug();
    }

    const CTask* pTaskMotionActive=(CTask*)ped.GetPedIntelligence()->GetTaskManager()->GetActiveTask(PED_TASK_TREE_MOTION);
	if(pTaskMotionActive && pTaskMotionActive->GetIsFlagSet(aiTaskFlags::HasBegun))
	{
		pTaskMotionActive->Debug();
	}
}

void CPedDebugVisualiser::VisualiseVehicleEntryDebug(const CPed& ped) const
{
	u8 iAlpha = 255;
	Vector3 WorldCoors;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	if (ped.IsLocalPlayer())
	{
		ped.GetPlayerInfo()->GetVehicleClipRequestHelper().Debug();
	}

	int iNoOfTexts=0;
	// Color32 colour = CRGBA(0,255,0,iAlpha);

	char debugText[128];

	CTaskEnterVehicle* pEnterVehicleTask = static_cast<CTaskEnterVehicle*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
	CTaskExitVehicle* pExitVehicleTask = static_cast<CTaskExitVehicle*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));

	CTaskVehicleFSM* pVehicleTask = pEnterVehicleTask ? static_cast<CTaskVehicleFSM*>(pEnterVehicleTask) : static_cast<CTaskVehicleFSM*>(pExitVehicleTask);
	if (pVehicleTask)
	{
		sprintf(debugText, "========================"); grcDebugDraw::Text( WorldCoors, CRGBA(0,0,255,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		sprintf(debugText, "ENTER/EXIT VEHICLE FLAGS"); grcDebugDraw::Text( WorldCoors, CRGBA(0,0,255,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		VehicleEnterExitFlags vehicleFlags = pVehicleTask->GetRunningFlags();
		for (s32 i=0; i<vehicleFlags.BitSet().GetNumBits(); i++)
		{
			if (pVehicleTask->IsFlagSet((CVehicleEnterExitFlags::Flags)i))
			{
				sprintf(debugText, "%s", parser_CVehicleEnterExitFlags__Flags_Strings[i]);
				grcDebugDraw::Text( WorldCoors, CRGBA(0,0,255,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
		}
		sprintf(debugText, "Target Seat Index : %i", pVehicleTask->GetTargetSeat());
		grcDebugDraw::Text( WorldCoors, CRGBA(0,0,255,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		sprintf(debugText, "Target Entry Point Index : %i", pVehicleTask->GetTargetEntryPoint());
		grcDebugDraw::Text( WorldCoors, CRGBA(0,0,255,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		sprintf(debugText, "Seat Request Type : %s", CSeatAccessor::GetSeatRequestTypeString(pVehicleTask->GetSeatRequestType())); 
		grcDebugDraw::Text( WorldCoors, CRGBA(0,0,255,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		if (pEnterVehicleTask)
		{
			const bool bWillWarpAfterTime = pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::WarpAfterTime);
			sprintf(debugText, "Will Warp After Time ? %s, Time Before Warp : %.2f", bWillWarpAfterTime ? "TRUE" : "FALSE", pEnterVehicleTask->GetTimeBeforeWarp());
			if (pEnterVehicleTask->GetTimeBeforeWarp() > 0.0f && bWillWarpAfterTime)
			{
				grcDebugDraw::Text( WorldCoors, CRGBA(0,255,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
			else
			{
				grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}

			CTaskEnterVehicleAlign* pEnterVehicleAlignTask = static_cast<CTaskEnterVehicleAlign*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE_ALIGN));
			if(pEnterVehicleAlignTask)
			{
				if(pEnterVehicleAlignTask->GetState()==CTaskEnterVehicleAlign::State_StreamAssets)
				{
					const bool bEnterOnVehicle = pEnterVehicleTask->IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle);

					fwMvClipSetId commonClipsetId = pEnterVehicleAlignTask->GetRequestedCommonClipSetId(bEnterOnVehicle);
					fwMvClipSetId entryClipsetId = pEnterVehicleAlignTask->GetRequestedEntryClipSetId();

					bool commonInPriorityStreamer = CPrioritizedClipSetRequestManager::GetInstance().HasClipSetBeenRequested(CPrioritizedClipSetRequestManager::RC_Vehicle, commonClipsetId);
					bool entryInPriorityStreamer = CPrioritizedClipSetRequestManager::GetInstance().HasClipSetBeenRequested(CPrioritizedClipSetRequestManager::RC_Vehicle, entryClipsetId);

					const CPrioritizedClipSetRequest* pCommonRequest = CPrioritizedClipSetStreamer::GetInstance().FindRequest(commonClipsetId);
					const CPrioritizedClipSetRequest* pEntryRequest = CPrioritizedClipSetStreamer::GetInstance().FindRequest(entryClipsetId);
					eStreamingPriority nCommonPriority = pCommonRequest ? pCommonRequest->GetStreamingPriority() : SP_Invalid;
					eStreamingPriority nEntryPriority = pEntryRequest ? pEntryRequest->GetStreamingPriority() : SP_Invalid;

					sprintf(debugText, "StreamAssets - enter clip set: %s (in priority streamer=%s, has req=%s, pri=%d)",  entryClipsetId.GetCStr(), entryInPriorityStreamer ? "true" : "false", pEntryRequest ? "true" : "false", nEntryPriority);
					grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
					sprintf(debugText, "StreamAssets - common clip set: %s (in priority streamer=%s, has req=%s, pri=%d)",  commonClipsetId.GetCStr(), commonInPriorityStreamer ? "true" : "false", pCommonRequest ? "true" : "false", nCommonPriority);
					grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

					sprintf(debugText, "Will Warp After Time ? %s, Time Before Warp : %.2f", bWillWarpAfterTime ? "TRUE" : "FALSE", pEnterVehicleTask->GetTimeBeforeWarp());
				}
			}

			if(pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_StreamAnimsToAlign)
			{
				fwMvClipSetId commonClipsetId = pEnterVehicleTask->GetRequestedCommonClipSetId();
				bool commonInPriorityStreamer = CPrioritizedClipSetRequestManager::GetInstance().HasClipSetBeenRequested(CPrioritizedClipSetRequestManager::RC_Vehicle, commonClipsetId);
				const CPrioritizedClipSetRequest* pCommonRequest = CPrioritizedClipSetStreamer::GetInstance().FindRequest(commonClipsetId);
				eStreamingPriority nCommonPriority = pCommonRequest ? pCommonRequest->GetStreamingPriority() : SP_Invalid;

				sprintf(debugText, "StreamAssets - common clip set: %s (in priority streamer=%s, has req=%s, pri=%d)",  commonClipsetId.GetCStr(), commonInPriorityStreamer ? "true" : "false", nCommonPriority ? "true" : "false", nCommonPriority);
				grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}

			if(pEnterVehicleTask->GetState() == CTaskEnterVehicle::State_StreamAnimsToOpenDoor)
			{
				fwMvClipSetId entryClipsetId = pEnterVehicleTask->GetRequestedEntryClipSetIdForOpenDoor();
				bool entryInPriorityStreamer = CPrioritizedClipSetRequestManager::GetInstance().HasClipSetBeenRequested(CPrioritizedClipSetRequestManager::RC_Vehicle, entryClipsetId);
				const CPrioritizedClipSetRequest* pEntryRequest = CPrioritizedClipSetStreamer::GetInstance().FindRequest(entryClipsetId);
				eStreamingPriority nEntryPriority = pEntryRequest ? pEntryRequest->GetStreamingPriority() : SP_Invalid;

				sprintf(debugText, "StreamAssets - entry clip set: %s (in priority streamer=%s, has req=%s, pri=%d)",  entryClipsetId.GetCStr(), entryInPriorityStreamer ? "true" : "false", pEntryRequest ? "true" : "false", nEntryPriority);
				grcDebugDraw::Text( WorldCoors, CRGBA(255,0,0,iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}
		}
	}
}

void CPedDebugVisualiser::VisualiseMotivation(const CPed& ped)const
{
	u32 iNoOfTexts=0;

	Vector3 WorldCoors = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + Vector3(0,0,1.1f);

	char debugText[100];

	sprintf( debugText, "Ped Motivation:");
	grcDebugDraw::Text( WorldCoors, Color_orange, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	iNoOfTexts++;

	//sprintf( debugText, "Happy: %f Modify: %f", ped.GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::HAPPY_STATE),
	//	ped.GetPedIntelligence()->GetPedMotivation().GetMotivationRateModifier(CPedMotivation::HAPPY_STATE)  );
	//grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	//iNoOfTexts++;

	sprintf( debugText, "Fear: %f Modify: %f", ped.GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::FEAR_STATE),
		ped.GetPedIntelligence()->GetPedMotivation().GetMotivationRateModifier(CPedMotivation::FEAR_STATE) );
	grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	iNoOfTexts++;

	sprintf( debugText, "Anger: %f Modify: %f", ped.GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::ANGRY_STATE),
		ped.GetPedIntelligence()->GetPedMotivation().GetMotivationRateModifier(CPedMotivation::ANGRY_STATE) );
	grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	iNoOfTexts++;

	//sprintf( debugText, "Tired: %f Modify: %f", ped.GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::TIRED_STATE),
	//	ped.GetPedIntelligence()->GetPedMotivation().GetMotivationRateModifier(CPedMotivation::TIRED_STATE));
	//grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	//iNoOfTexts++;

	//sprintf( debugText, "Hunger: %f Modify: %f", ped.GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::HUNGRY_STATE),
	//	ped.GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::HUNGRY_STATE) );
	//grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	//iNoOfTexts++;
}

void CPedDebugVisualiser::VisualiseWanted(const CPed& ped)const
{
	if( ped.GetPlayerWanted() )
	{
		Vector3 WorldCoors = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + Vector3(0,0,1.1f);
		CWantedDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, 255));
		tempDebugInfo.Print();
		tempDebugInfo.Visualise();
	}
}

void CPedDebugVisualiser::VisualiseOrder(const CPed& ped)const
{
	//Ensure the order is valid.
	const COrder* pOrder = ped.GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return;
	}

	u32 iNoOfTexts=0;

	Vector3 WorldCoors = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + Vector3(0,0,1.1f);

	char debugText[100];

	if (NetworkInterface::IsGameInProgress())
	{
		formatf( debugText, "Order Index: %d", pOrder->GetOrderIndex());
		grcDebugDraw::Text( WorldCoors, Color_orange, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
		iNoOfTexts++;
	}


	formatf( debugText, "Dispatch type: %d", pOrder->GetDispatchType());
	grcDebugDraw::Text( WorldCoors, Color_orange, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	iNoOfTexts++;

	formatf( debugText, "Order Type: %d", pOrder->GetType());
	grcDebugDraw::Text( WorldCoors, Color_orange, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
	iNoOfTexts++;

	//Check the type.
	switch(pOrder->GetType())
	{
		case COrder::OT_Police:
		{
			//Grab the police order.
			const CPoliceOrder* pPoliceOrder = static_cast<const CPoliceOrder *>(pOrder);

			formatf( debugText, "Police Order Type: %d", pPoliceOrder->GetPoliceOrderType());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;

			break;
		}
		case COrder::OT_Swat:
		{
			//Grab the swat order.
			const CSwatOrder* pSwatOrder = static_cast<const CSwatOrder *>(pOrder);

			formatf( debugText, "Swat Order Type: %d", pSwatOrder->GetSwatOrderType());
			grcDebugDraw::Text( WorldCoors, Color_blue, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText );
			iNoOfTexts++;

			break;
		}
		default:
		{
			break;
		}
	}
}

void CPedDebugVisualiser::VisualisePersonalities(const CPed& ped)const
{
	u8 iAlpha = 255;
	Vector3 WorldCoors;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	int iNoOfTexts=0;
	Color32 colour = CRGBA(255,192,128,iAlpha);

	const CPedModelInfo* mi = ped.GetPedModelInfo();
	Assert(mi);
	const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();

	char debugText[128];
	sprintf(debugText, "Personality: %s", pd.GetPersonalityName());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Weapon Loadout: %s", pd.GetDefaultWeaponLoadoutName());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Attack strength: %.2f [%.2f, %.2f]", pd.GetAttackStrength(ped.GetRandomSeed()), pd.GetAttackStrengthMin(), pd.GetAttackStrengthMax());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Stamina Efficiency: %.2f", pd.GetStaminaEfficiency());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Motivation: %d [%d, %d]", pd.GetMotivation(ped.GetRandomSeed()), pd.GetMotivationMin(), pd.GetMotivationMax());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Age: %d", mi->GetAge());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Sexiness: %d (flags)", mi->GetSexinessFlags());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Bravery: (%s) %d (flags)", pd.GetBraveryName(), pd.GetBraveryFlags());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Agility: %d (flags)", pd.GetAgilityFlags());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

    sprintf(debugText, "Crime: %d (flags)", pd.GetCriminalityFlags());
    grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Driving ability: %d [%d, %d]", pd.GetDrivingAbility(ped.GetRandomSeed()), pd.GetDrivingAbilityMin(), pd.GetDrivingAbilityMax());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Driving aggressiveness: %d [%d, %d]", pd.GetDrivingAggressiveness(ped.GetRandomSeed()), pd.GetDrivingAggressivenessMin(), pd.GetDrivingAggressivenessMax());
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "%s", pd.GetIsMale() ? "Male" : "Female");
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	sprintf(debugText, "Drives car types:");
	if (pd.DrivesVehicleType(PED_DRIVES_POOR_CAR))
		safecat(debugText, " poor");
	if (pd.DrivesVehicleType(PED_DRIVES_AVERAGE_CAR))
		safecat(debugText, " average");
	if (pd.DrivesVehicleType(PED_DRIVES_RICH_CAR))
		safecat(debugText, " rich");
	if (pd.DrivesVehicleType(PED_DRIVES_BIG_CAR))
		safecat(debugText, " big");
	if (pd.DrivesVehicleType(PED_DRIVES_MOTORCYCLE))
		safecat(debugText, " motorcycles");
	if (pd.DrivesVehicleType(PED_DRIVES_BOAT))
		safecat(debugText, " boats");
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	Affluence a = pd.GetAffluence();
	sprintf(debugText, "Affluence:");
	switch(a) {
		case AFF_POOR:
			safecat(debugText, " poor");
			break;
		case AFF_AVERAGE:
			safecat(debugText, " average");
			break;
		case AFF_RICH:
			safecat(debugText, " rich");
			break;
		default:
			safecat(debugText, " NOT SET!");
			break;
	}
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	TechSavvy t = pd.GetTechSavvy();
	sprintf(debugText, "TechSavvy:");
	switch(t) {
		case TS_LOW:
			safecat(debugText, " low");
			break;
		case TS_HIGH:
			safecat(debugText, " high");
			break;
		default:
			safecat(debugText, " NOT SET!");
			break;
	}
	grcDebugDraw::Text(WorldCoors, colour, 0, (iNoOfTexts++) * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
}

extern const char* parser_ePedConfigFlags_Strings[];
extern const char* parser_ePedResetFlags_Strings[];

void CPedDebugVisualiser::VisualisePedFlags(const CPed& ped) const
{
	u8 iAlpha = 255;
	Vector3 WorldCoors;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	CPedFlagsDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
}


void CPedDebugVisualiser::VisualiseDamage(const CPed& ped, const bool bOnlyDamageRecords) const
{
	const Vector3 vecEntityPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 vDiff = vecEntityPosition - camInterface::GetPos();

	int nHitComponent = ped.GetWeaponDamageComponent();
	float fDist = vDiff.Mag();
	if(fDist >= ms_fVisualiseRange)
	{
		if( ped.GetWeaponDamageEntity() )
		{
			Vector3 vSourcePos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
			if (nHitComponent > 0)
			{
				eAnimBoneTag BoneTag = ped.GetBoneTagFromRagdollComponent(nHitComponent);
				ped.GetBonePosition( vSourcePos, BoneTag );
			}
			grcDebugDraw::Line( vSourcePos, VEC3V_TO_VECTOR3(ped.GetWeaponDamageEntity()->GetTransform().GetPosition()), Color_yellow, Color_red );
		}
		return;
	}

	Vector3 vTextPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + ZAXIS;
	CDamageDebugInfo tempDebugInfo(&ped, bOnlyDamageRecords, CAIDebugInfo::DebugInfoPrintParams(vTextPos, 255));
	tempDebugInfo.Print();
}


void CPedDebugVisualiser::VisualiseCombatDirector(const CPed& ped) const
{
	const Vector3 vecEntityPosition = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 vDiff = vecEntityPosition - camInterface::GetPos();

	float fDist = vDiff.Mag();
	if(fDist >= ms_fVisualiseRange)
	{
		return;
	}

	CCombatDirector* pCombatDirector = ped.GetPedIntelligence()->GetCombatDirector();
	if(!pCombatDirector)
	{
		return;
	}

	CCombatOrders* pCombatOrders = ped.GetPedIntelligence()->GetCombatOrders();
	if(!pCombatOrders)
	{
		return;
	}

	char debugText[1024];
	int iNoOfTexts=0;
	Vector3 vTextPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + ZAXIS;

	formatf(debugText, "Combat Director: %p", pCombatDirector);
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	if (CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayCombatDirectorAnimStreaming)
	{
		formatf(debugText, "  Num Peds With 1H Weapons: %u", pCombatDirector->GetNumPedsWith1HWeapons());
		grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		formatf(debugText, "  Num Peds With 2H Weapons: %u", pCombatDirector->GetNumPedsWith2HWeapons());
		grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverPeekingVariations1HInfo(false).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    Low 1H Clipset Requested: %s", pCombatDirector->GetCoverPeekingVariations1HInfo(false).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPeekingVariations1HInfo(false).m_uNumTimesUsed, pCombatDirector->GetCoverPeekingVariations1HInfo(false).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverPeekingVariations1HInfo(true).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    High 1H Clipset Requested: %s", pCombatDirector->GetCoverPeekingVariations1HInfo(true).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPeekingVariations1HInfo(true).m_uNumTimesUsed, pCombatDirector->GetCoverPeekingVariations1HInfo(true).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverPeekingVariations2HInfo(false).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    Low 2H Clipset Requested: %s", pCombatDirector->GetCoverPeekingVariations2HInfo(false).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPeekingVariations2HInfo(false).m_uNumTimesUsed, pCombatDirector->GetCoverPeekingVariations2HInfo(false).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverPeekingVariations2HInfo(true).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    High 2H Clipset Requested: %s", pCombatDirector->GetCoverPeekingVariations2HInfo(true).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPeekingVariations2HInfo(true).m_uNumTimesUsed, pCombatDirector->GetCoverPeekingVariations2HInfo(true).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		//////////////////////////////////////////////////////////////////////////

		for (s32 i=0; i<pCombatDirector->GetCoverPinnedVariations1HInfo(false).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    Low 1H Clipset Requested: %s", pCombatDirector->GetCoverPinnedVariations1HInfo(false).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPinnedVariations1HInfo(false).m_uNumTimesUsed, pCombatDirector->GetCoverPinnedVariations1HInfo(false).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverPinnedVariations1HInfo(true).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    High 1H Clipset Requested: %s", pCombatDirector->GetCoverPinnedVariations1HInfo(true).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPinnedVariations1HInfo(true).m_uNumTimesUsed, pCombatDirector->GetCoverPinnedVariations1HInfo(true).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverPinnedVariations2HInfo(false).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    Low 2H Clipset Requested: %s", pCombatDirector->GetCoverPinnedVariations2HInfo(false).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPinnedVariations2HInfo(false).m_uNumTimesUsed, pCombatDirector->GetCoverPinnedVariations2HInfo(false).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverPinnedVariations2HInfo(true).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    High 2H Clipset Requested: %s", pCombatDirector->GetCoverPinnedVariations2HInfo(true).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverPinnedVariations2HInfo(true).m_uNumTimesUsed, pCombatDirector->GetCoverPinnedVariations2HInfo(true).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		//////////////////////////////////////////////////////////////////////////

		for (s32 i=0; i<pCombatDirector->GetCoverIdleVariations1HInfo(false).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    Low 1H Clipset Requested: %s", pCombatDirector->GetCoverIdleVariations1HInfo(false).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverIdleVariations1HInfo(false).m_uNumTimesUsed, pCombatDirector->GetCoverIdleVariations1HInfo(false).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverIdleVariations1HInfo(true).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    High 1H Clipset Requested: %s", pCombatDirector->GetCoverIdleVariations1HInfo(true).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverIdleVariations1HInfo(true).m_uNumTimesUsed, pCombatDirector->GetCoverIdleVariations1HInfo(true).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverIdleVariations2HInfo(false).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    Low 2H Clipset Requested: %s", pCombatDirector->GetCoverIdleVariations2HInfo(false).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverIdleVariations2HInfo(false).m_uNumTimesUsed, pCombatDirector->GetCoverIdleVariations2HInfo(false).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		for (s32 i=0; i<pCombatDirector->GetCoverIdleVariations2HInfo(true).m_aVariationClipsetRequestHelpers.GetCount(); ++i)
		{
			formatf(debugText, "    High 2H Clipset Requested: %s", pCombatDirector->GetCoverIdleVariations2HInfo(true).m_aVariationClipsetRequestHelpers[i].GetClipSetId().GetCStr());
			grcDebugDraw::Text( vTextPos, Color_yellow, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		formatf(debugText, "    Times Used: %u / Max: %u", pCombatDirector->GetCoverIdleVariations2HInfo(true).m_uNumTimesUsed, pCombatDirector->GetCoverIdleVariations2HInfo(true).m_uMaxNumTimesUsed);
		grcDebugDraw::Text( vTextPos, Color_orange, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}

	formatf(debugText, "  Situation: %s", pCombatDirector->GetSituation()->GetName());
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "  Action: %.2f", pCombatDirector->GetAction());
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Combat Orders: %p", pCombatOrders);
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "  Modifiers");
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "    Shoot Rate: %.2f", pCombatOrders->GetShootRateModifier());
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "    Time Between Bursts: %.2f", pCombatOrders->GetTimeBetweenBurstsModifier());
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "  Has Attack Range: %s", pCombatOrders->HasAttackRange() ? "Yes" : "No");
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	if(pCombatOrders->HasAttackRange())
	{
		formatf(debugText, "    Attack Range: %s",
			(pCombatOrders->GetAttackRange() == CCombatData::CR_Near)		?	"Near" :
			(pCombatOrders->GetAttackRange() == CCombatData::CR_Medium)		?	"Medium" :
			(pCombatOrders->GetAttackRange() == CCombatData::CR_Far)		?	"Far" :
			(pCombatOrders->GetAttackRange() == CCombatData::CR_VeryFar)	?	"Very Far" :
																				"Unknown");
		grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}

	formatf(debugText, "  Active Orders");
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "    Cover Fire: %s", pCombatOrders->GetActiveFlags().IsFlagSet(CCombatOrders::AF_CoverFire) ? "Yes" : "No");
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "  Passive Orders");
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "    Block Cover: %s", pCombatOrders->GetPassiveFlags().IsFlagSet(CCombatOrders::PF_BlockCover) ? "Yes" : "No");
	grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
}

void CPedDebugVisualiser::VisualiseTargeting(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	CTargetingDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
	tempDebugInfo.Visualise();
}

void CPedDebugVisualiser::VisualiseFocusAiPedTargeting(const CPed& ped) const
{
	// Only render the extra information for the focus entity, otherwise we'd have too much info
	if(CDebugScene::FocusEntities_Get(0) != &ped)
	{
		return;
	}

	CPedTargetting* pPedTargetting = ped.GetPedIntelligence()->GetTargetting(false);
	if(!pPedTargetting)
	{
		return;
	}

	char debugText[1024];
	int iNoOfTexts=0;
	Vector3 vTextPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + ZAXIS;
	Vector3 WorldCoors;
	u8 iAlpha = 255;

	Vec3V vPedPosition = ped.GetTransform().GetPosition();
	Vector3 vLastSeenPosition = VEC3_ZERO;

	const int numActiveTargets = pPedTargetting->GetNumActiveTargets();
	for(int i = 0; i < numActiveTargets; i++)
	{
		const CEntity* pTarget = pPedTargetting->GetTargetAtIndex(i);
		CSingleTargetInfo* pTargetInfo = pTarget ? pPedTargetting->FindTarget(pTarget) : NULL;
		if(pTargetInfo)
		{
			Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();

			iNoOfTexts = 0;
			vTextPos = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition()) + ZAXIS;

			if(pTarget == pPedTargetting->GetCurrentTarget())
			{
				formatf(debugText, "-CURRENT TARGET-");
				grcDebugDraw::Text( vTextPos, Color_blue, 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			}

			switch(pTargetInfo->GetLosStatus())
			{
				case Los_blocked:
				{
					formatf(debugText, "Los Status: Blocked");
					grcDebugDraw::Line(vPedPosition, vTargetPosition, Color_red);
					break;
				}
				case Los_blockedByFriendly:
				{
					formatf(debugText, "Los Status: Blocked by friendly");
					grcDebugDraw::Line(vPedPosition, vTargetPosition, Color_orange);
					break;
				}
				case Los_clear:
				{
					formatf(debugText, "Los Status: Clear");
					grcDebugDraw::Line(vPedPosition, vTargetPosition, Color_green);
					break;
				}
				case Los_unchecked:
				{
					formatf(debugText, "Los Status: Unchecked");
					grcDebugDraw::Line(vPedPosition, vTargetPosition, Color_yellow);
					break;
				}
				default:
				{
					formatf(debugText, "Los Status: Unknown");
					break;
				}
			}

			if(!GetCoorsAndAlphaForDebugText(*pTarget, WorldCoors, iAlpha))
			{
				continue;
			}

			grcDebugDraw::Text( vTextPos, CRGBA(85, 200, 255, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			formatf(debugText, "Are target whereabouts known: %s", pTargetInfo->AreWhereaboutsKnown() ? "Yes" : "No");
			grcDebugDraw::Text( vTextPos, CRGBA(85, 200, 255, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			Vector3 vLastSeenPosition = pTargetInfo->GetLastPositionSeen();
			formatf(debugText, "Last seen position: %.2f, %.2f, %.2f", vLastSeenPosition.x, vLastSeenPosition.y, vLastSeenPosition.z);
			grcDebugDraw::Text( vTextPos, CRGBA(85, 200, 255, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			formatf(debugText, "Last seen timer: %.2f", pTargetInfo->GetLastSeenTimer());
			grcDebugDraw::Text( vTextPos, CRGBA(85, 200, 255, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			formatf(debugText, "Targeting score: %.2f", pTargetInfo->GetTotalScore());
			grcDebugDraw::Text( vTextPos, CRGBA(85, 200, 255, iAlpha), 0, iNoOfTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
	}
}

// Draw the ped's population type.
void CPedDebugVisualiser::VisualisePopulationInfo(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	char debugText[1024];
	int iNoOfTexts = 0;
	const char* cPopInfoString;

	// Get a nice string representation of the ped's pop info.
	switch(ped.PopTypeGet())
	{
	case POPTYPE_MISSION:
		{
			cPopInfoString = "Mission";
			break;
		}
	case POPTYPE_RANDOM_AMBIENT:
		{
			cPopInfoString = "Ambient";
			break;
		}
	case POPTYPE_RANDOM_SCENARIO:
		{
			cPopInfoString = "Scenario";
			break;
		}
	case POPTYPE_PERMANENT:
		{
			cPopInfoString = "Player / Player Groups";
			break;
		}
	default:
		{
			cPopInfoString = "Unknown type";
			break;
		}
	}

	formatf(debugText, "Population type: %s", cPopInfoString);

	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
}

void CPedDebugVisualiser::VisualiseArrestInfo(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	char debugText[1024];
	int iNoOfTexts = 0;

	// Endurance
	formatf(debugText, "Endurance: %.0f / %.0f", ped.GetEndurance(), ped.GetMaxEndurance());
	grcDebugDraw::Text(WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	if(ped.GetCustodian())
	{
		const char *pName = ped.GetCustodian() && ped.GetCustodian()->GetNetworkObject() ? ped.GetCustodian()->GetNetworkObject()->GetLogName() : "No Name";
		formatf(debugText, "Custodian: %s", pName);
	}
	else
	{
		formatf(debugText, "Custodian: NULL");
	}

	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "In Custody: %s", ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody) ? "true" : "false" );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Arrested: %s", ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) ? "true" : "false" );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Can Be Arrested: %s", ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested) ? "true" : "false" );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Can Perform Arrest: %s", ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformArrest) ? "true" : "false" );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Can Uncuff: %s", ped.GetPedConfigFlag(CPED_CONFIG_FLAG_CanPerformUncuff) ? "true" : "false" );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

#if ENABLE_TASKS_ARREST_CUFFED
	//! Arrest Task.
	CTaskArrest* pTaskArrest = static_cast<CTaskArrest*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST));
	if (pTaskArrest)
	{
		formatf(debugText, "Arrest Phase: %f", pTaskArrest->GetPhase());
		grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}
	//! Cuffed Task
	CTaskCuffed* pTaskCuffed = static_cast<CTaskCuffed*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CUFFED));
	if (pTaskCuffed)
	{
		formatf(debugText, "Cuffed Phase: %f", pTaskCuffed->GetPhase());
		grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}
	//! In Custody Task.
	if(ped.GetCustodian())
	{
		CTaskArrest* pTaskArrest = static_cast<CTaskArrest*>(ped.GetCustodian()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST));
		if(pTaskArrest && pTaskArrest->IsTakingCustody())
		{
			formatf(debugText, "In Custody Phase: %f", pTaskArrest->GetPhase());
			grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
	}
#endif // ENABLE_TASKS_ARREST_CUFFED
}

void CPedDebugVisualiser::VisualiseParachuteInfo(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	char debugText[1024];
	int iNoOfTexts = 0;

	//Ensure the ped's inventory is valid.
	const CPedInventory* pPedInventory = ped.GetInventory();
	if(!pPedInventory)
	{
		return;
	}

	formatf(debugText, "Has Parachute?: %s", pPedInventory->GetWeapon(GADGETTYPE_PARACHUTE) ? "true" : "false" );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Has Reserve?: %s", ped.GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute) ? "true" : "false" );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	//! PACK.
	formatf(debugText, "Current Parachute Pack Tint: %d", CTaskParachute::GetTintIndexForParachutePack(ped) );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Parachute Pack Model ID: %s (%d)", CTaskParachute::GetModelForParachutePack(&ped).TryGetCStr() ? CTaskParachute::GetModelForParachutePack(&ped).TryGetCStr() : "none", CTaskParachute::GetModelForParachutePack(&ped).GetHash()  );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	const CTaskParachute::PedVariation* pVariation = CTaskParachute::FindParachutePackVariationForPed(ped);
	if(pVariation)
	{
		formatf(debugText, "Parachute Pack Variation. Comp: %d, Draw: %d, AltDraw: %d, Tex: %d", (int)pVariation->m_Component, pVariation->m_DrawableId, pVariation->m_DrawableAltId, pVariation->m_TexId );
		grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}

	if(ped.GetPlayerInfo())
	{
		if(ped.GetPlayerInfo()->GetPedParachuteVariationComponentOverride() != PV_COMP_INVALID)
		{
			formatf(debugText, "Script Parachute Pack Variation. Comp: %d, Draw: %d, AltDraw: %d, Tex: %d", (int)ped.GetPlayerInfo()->GetPedParachuteVariationComponentOverride(),
				ped.GetPlayerInfo()->GetPedParachuteVariationDrawableOverride(), 
				ped.GetPlayerInfo()->GetPedParachuteVariationAltDrawableOverride(), 
				ped.GetPlayerInfo()->GetPedParachuteVariationTexIdOverride() );
			grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}

		if(ped.GetPlayerInfo()->GetPreviousVariationComponent() != PV_COMP_INVALID)
		{
			u32				PreviousDrawableId;
			u32				PreviousDrawableAltId;
			u32				PreviousTexId;
			u32				PreviousVariationPaletteId;
			ped.GetPlayerInfo()->GetPreviousVariationInfo(PreviousDrawableId, PreviousDrawableAltId, PreviousTexId, PreviousVariationPaletteId);

			formatf(debugText, "Previous Ped Variation. Comp: %d, Draw: %d, AltDraw: %d, Tex: %d", (int)ped.GetPlayerInfo()->GetPreviousVariationComponent(),
				PreviousDrawableId, 
				PreviousDrawableAltId, 
				PreviousTexId );
			grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
		}
	}

	formatf(debugText, "Parachute Pack Variation Active: %s", CTaskParachute::IsParachutePackVariationActiveForPed(ped) ? "true" : "false"  );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	//! PARACHUTE.
	formatf(debugText, "Current Parachute Tint: %d", CTaskParachute::GetTintIndexForParachute(ped) );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	formatf(debugText, "Parachute Model ID: %s (%d)", CTaskParachute::GetModelForParachute(&ped).TryGetCStr() ? CTaskParachute::GetModelForParachute(&ped).TryGetCStr() : "none", CTaskParachute::GetModelForParachute(&ped).GetHash()  );
	grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

	//! RAW - ped intelligence info overrides.
	if(ped.GetPedIntelligence())
	{
		formatf(debugText, "Raw Parachute Pack Tint Override: %d", ped.GetPedIntelligence()->GetTintIndexForParachutePack() );
		grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		formatf(debugText, "Raw Parachute Tint Override: %d", ped.GetPedIntelligence()->GetTintIndexForParachute() );
		grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

		formatf(debugText, "Raw Reserve Parachute Tint Override: %d", ped.GetPedIntelligence()->GetTintIndexForReserveParachute() );
		grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
	}
}

void CPedDebugVisualiser::VisualiseMelee(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	char debugText[1024];
	int iNoOfTexts = 0;

	CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));
	if(pFocusPed == &ped)
	{
		CCombatMeleeGroup* pFocusPedMeleeGroup = CCombatManager::GetMeleeCombatManager()->FindMeleeGroup(ped);
		if(pFocusPedMeleeGroup)
		{
			formatf(debugText, "Num opponents: %d", pFocusPedMeleeGroup->GetNumOpponents());
			grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			formatf(debugText, "Is melee group full: %s", pFocusPedMeleeGroup->IsMeleeGroupFull() ? "Yes" : "No");
			grcDebugDraw::Text( WorldCoors, CRGBA(0, 0, 255, iAlpha), 0, iNoOfTexts++ * grcDebugDraw::GetScreenSpaceTextHeight(), debugText);

			for(u32 i = 0; i < pFocusPedMeleeGroup->GetNumOpponents(); i++)
			{
				const CPed* pOpponent = pFocusPedMeleeGroup->GetOpponentAtIndex(i);
				if(pOpponent)
				{
					grcDebugDraw::Line(ped.GetTransform().GetPosition(), pOpponent->GetTransform().GetPosition(), Color_purple);
				}
			}
		}
	}
}

void CPedDebugVisualiser::VisualiseVehicleLockInfo(const CPed& ped) const
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	const CVehicle* pNearestVehicle = CVehicleLockDebugInfo::GetNearestVehicleToVisualise(&ped);
	if (pNearestVehicle)
	{
		CVehicleLockDebugInfo tempDebugInfo(&ped, pNearestVehicle, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
		tempDebugInfo.Print();
		tempDebugInfo.Visualise();
	}
}

void CPedDebugVisualiser::VisualisePlayerCoverSearchInfo(const CPed& ped) const
{
	if (ped.IsLocalPlayer())
	{
		Vector3 WorldCoors;
		u8 iAlpha = 255;
		if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
		{
			return;
		}

		if (CCoverDebug::ms_Tunables.m_EnableNewCoverDebugRendering)
		{
			CCoverDebug::Debug();
		}

		s32 iActiveIndex = CPlayerInfo::ms_DynamicCoverHelper.GetActiveVehicleCoverBoundIndex();
		TUNE_GROUP_BOOL(COVER_BOUND_TUNE, RENDER_ACTIVE_INDEX, false);
		if (RENDER_ACTIVE_INDEX)
		{
			char szDebugText[128];
			formatf(szDebugText, "Active Cover Bound Index : %i", iActiveIndex);
			CCoverDebug::AddDebugText(ped.GetTransform().GetPosition(), Color_green, szDebugText, -1, 0, CCoverDebug::DEFAULT, true);
		}
		else
		{
			const bool bCoverEntryEnabled = !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_COVER);
			if (!bCoverEntryEnabled)
			{
				char szDebugText[128];
				formatf(szDebugText, "COVER INPUT IS DISABLED");
				CCoverDebug::AddDebugText(ped.GetTransform().GetPosition(), Color_red, szDebugText, -1, 0, CCoverDebug::DEFAULT, true);
			}
		}

		TUNE_GROUP_BOOL(COVER_BOUND_TUNE, RENDER_ACTIVE_BOUNDS, true);
		const CEntityScannerIterator entityList = ped.GetPedIntelligence()->GetNearbyVehicles();
		for(const CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext())
		{
			const CVehicle& rVeh = *static_cast<const CVehicle*>(pEntity);
			if (rVeh.GetVehicleCoverBoundOffsetInfo())
			{
				const atArray<CVehicleCoverBoundInfo>& rCoverBoundInfoArray = rVeh.GetVehicleCoverBoundOffsetInfo()->GetCoverBoundInfoArray();
				const s32 iNumElements = rCoverBoundInfoArray.GetCount();
				if (iNumElements > 0)
				{
					for (s32 i=0; i<iNumElements; ++i)
					{
						bool bIsActiveBound = false;
						if (RENDER_ACTIVE_BOUNDS && iActiveIndex  > -1 && iActiveIndex < iNumElements)
						{
							bIsActiveBound = CVehicleCoverBoundInfo::IsBoundActive(rCoverBoundInfoArray[iActiveIndex], rCoverBoundInfoArray[i]);
						}
						CVehicleCoverBoundInfo::RenderCoverBound(rVeh, rCoverBoundInfoArray, i, bIsActiveBound, RENDER_ACTIVE_BOUNDS && i == iActiveIndex);
					}
				}
			}
		}

		TUNE_GROUP_BOOL(COVER_BOUND_TUNE, RENDER_NOTHING_ELSE, true);
		if (RENDER_NOTHING_ELSE)
		{
			return;
		}

		CCoverDebug::ms_searchDebugDraw.Render();
	}
}

void CPedDebugVisualiser::VisualisePlayerInfo(const CPed& ped) const
{
	if(ped.IsLocalPlayer())
	{
		s32 iNoOfTexts = 0;
		Vector3 vTextPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + ZAXIS;
		char debugText[128];

		formatf(debugText, "Auto give parachute when enter plane: %s", ped.GetPlayerInfo()->GetAutoGiveParachuteWhenEnterPlane() ? "yes" : "no");
		grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText);
		iNoOfTexts++;

		formatf(debugText, "Auto give scuba gear when exit vehicle: %s", ped.GetPlayerInfo()->GetAutoGiveScubaGearWhenExitVehicle() ? "yes" : "no");
		grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText);
		iNoOfTexts++;

		formatf(debugText, "Disable agitation: %s", ped.GetPlayerInfo()->GetDisableAgitation() ? "yes" : "no");
		grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText);
		iNoOfTexts++;

		formatf(debugText, "Recharge Script Multiplier: %.2f", ped.GetPlayerInfo()->GetHealthRecharge().GetRechargeScriptMultiplier());
		grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText);
		iNoOfTexts++;

	}
}

void CPedDebugVisualiser::VisualiseStealth(const CPed& ped) const
{
	s32 iNoOfTexts = 0;
	if(ped.GetIsStanding())
	{
		phMaterialMgr::Id groundId = ped.GetPackedGroundMaterialId();
		const phMaterial& groundMat = PGTAMATERIALMGR->GetMaterial(groundId);
		const char* groundName = groundMat.GetName();

		// Get stealth value here:
		// e.g.
		float fMatNoisiness = PGTAMATERIALMGR->GetMtlNoisiness(groundId);
		Vector3 vTextPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()) + ZAXIS;

		// Ground noisiness
		char debugText[50];
		sprintf(debugText,"Ground material: %s", groundName);
		grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText);
		iNoOfTexts++;

		sprintf(debugText,"Ground noisiness %.2f", fMatNoisiness);
		grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),debugText);
		++iNoOfTexts;

		if (ped.GetPlayerInfo())
		{
			char playerLoudness[50];
			sprintf(playerLoudness, "Player loudness: %.2f", ped.GetPlayerInfo()->GetStealthNoise());
			grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),playerLoudness);
			iNoOfTexts++;

			sprintf(playerLoudness, "Player stealth multiplier: %.2f", ped.GetPlayerInfo()->GetStealthMultiplier());
			grcDebugDraw::Text(vTextPos,Color_blue,0,iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(),playerLoudness);
			iNoOfTexts++;
		}
	}
}

void CPedDebugVisualiser::VisualiseAssistedMovement(const CPed& ped) const
{
	VisualiseTasks(ped);

	if(ped.IsPlayer())
	{
		CTaskMovePlayer * pMovePlayer = (CTaskMovePlayer*) ped.GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_PLAYER);
		if(pMovePlayer)
		{
			CPlayerAssistedMovementControl * pAss = pMovePlayer->GetAssistedMovementControl();
			pAss->Debug(&ped);

			if(pAss->GetIsUsingRoute())
			{
				Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());

				Color32 iCol = Color_orange;
				iCol.SetAlpha(192);

				static char dbgTxt[128];
				int iTextHeight = grcDebugDraw::GetScreenSpaceTextHeight();
				int iTextOffsetY = iTextHeight * -10;

				sprintf(dbgTxt, "DesiredHeading : %.2f, DesiredPitch : %.2f", ped.GetDesiredHeading(), ped.GetDesiredPitch());
				grcDebugDraw::Text(vPedPos, iCol, 0, iTextOffsetY, dbgTxt);
				iTextOffsetY += iTextHeight;

				sprintf(dbgTxt, "CurrentHeading : %.2f, CurrentPitch : %.2f", ped.GetCurrentHeading(), ped.GetCurrentPitch());
				grcDebugDraw::Text(vPedPos, iCol, 0, iTextOffsetY, dbgTxt);
				iTextOffsetY += iTextHeight;

				sprintf(dbgTxt, "LocalCurvature : %.2f", pAss->GetLocalRouteCurvature());
				grcDebugDraw::Text(vPedPos, iCol, 0, iTextOffsetY, dbgTxt);
				iTextOffsetY += iTextHeight;
			}
		}
	}

	CAssistedMovementRouteStore::Debug();
}

void CPedDebugVisualiser::VisualiseAttachments(const CPed& ped) const
{
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if (pEnt && pEnt->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
		static s32 iEntryPointIndex = 0;

		grcDebugDraw::AddDebugOutput(Color_yellow, "Roll : %.4f", pVehicle->GetTransform().GetRoll());
		grcDebugDraw::AddDebugOutput(Color_yellow, "Heading : %.4f", pVehicle->GetTransform().GetHeading());
		grcDebugDraw::AddDebugOutput(Color_yellow, "Pitch : %.4f", pVehicle->GetTransform().GetPitch());

		const CEntryExitPoint* pEntryExitPoint = pVehicle->GetEntryExitPoint(iEntryPointIndex);

		Vector3 vGetInOffset(Vector3::ZeroType);

		float fHeadingRotation = 0.0f;
		pVehicle->GetVehicleGetInOffset(vGetInOffset, fHeadingRotation, iEntryPointIndex);
		grcDebugDraw::AddDebugOutput(Color_green, "Get In Heading Rotation : %.4f",fHeadingRotation);

		Matrix34 mCarMat(Matrix34::IdentityType);
		pVehicle->GetMatrixCopy(mCarMat);

		Matrix34 mSeatMat(Matrix34::IdentityType);
		int iBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(pEntryExitPoint->GetSeat(SA_directAccessSeat));
		mSeatMat = pVehicle->GetObjectMtx(iBoneIndex);

		if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
		{
			// Add on additional offset to bring the height up even if the bike is on its side.
			const float fUpZ = pVehicle->GetTransform().GetC().GetZf();
			float fAdditionalZ = MAX(1.0f - ABS(fUpZ),0.0f) * mSeatMat.d.z;
			Vector3 vAdditionalZ(0.0f, 0.0f, fAdditionalZ);

			const float fRoll = pVehicle->GetTransform().GetRoll();
			vAdditionalZ.RotateY(fRoll);

			// Orientate the seat matrix as if the bike is upright
			mSeatMat.RotateY(fRoll);

			mSeatMat.d += vAdditionalZ;
		}

		Matrix34 mWorldSeatMat(Matrix34::IdentityType);
		mWorldSeatMat.Dot(mSeatMat, mCarMat);

		grcDebugDraw::Axis(mWorldSeatMat, 0.2f);
		grcDebugDraw::Text(mWorldSeatMat.d, Color_green, "Seat matrix");

		mSeatMat.Transform3x3(vGetInOffset);

		if (ABS(mSeatMat.c.z) > ZUPFOROBJECTTOBEUPRIGHT)
		{
			mSeatMat.RotateZ(fHeadingRotation);
		}
		else if( ABS(mSeatMat.b.z) > ZUPFOROBJECTTOBEUPRIGHT)
		{
			mSeatMat.RotateY(fHeadingRotation);
		}
		else if( ABS(mSeatMat.a.z) > ZUPFOROBJECTTOBEUPRIGHT)
		{
			mSeatMat.RotateX(fHeadingRotation);
		}

		mSeatMat.d += vGetInOffset;

		Matrix34 mWorldAttachMat(Matrix34::IdentityType);
		mWorldAttachMat.Dot(mSeatMat, mCarMat);
		//mWorldAttachMat.d.z += fGlobalAdditionalZ;

		grcDebugDraw::Line(mWorldSeatMat.d, mWorldAttachMat.d, Color_orange);

		grcDebugDraw::Axis(mWorldAttachMat, 0.2f, true);
		grcDebugDraw::Text(mWorldAttachMat.d, Color_green, "Get in start matrix");
	}

	if (ped.GetAttachParent())
	{
		const Vector3 vAttachParentPos = VEC3V_TO_VECTOR3(ped.GetAttachParent()->GetTransform().GetPosition());
		grcDebugDraw::Sphere(vAttachParentPos, 0.05f, Color_orange);
		grcDebugDraw::Line(vAttachParentPos,vAttachParentPos+ped.GetAttachOffset(), Color_orange, Color_red);
		grcDebugDraw::Sphere(vAttachParentPos+ped.GetAttachOffset(), 0.05f, Color_red);
	}

	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	CAttachmentsDebugInfo tempDebugInfo(&ped, CAIDebugInfo::DebugInfoPrintParams(WorldCoors, iAlpha));
	tempDebugInfo.Print();
}

void CPedDebugVisualiser::VisualiseLOD(const CPed& ped) const
{
	ped.GetPedAiLod().DebugRender();
}

void CPedDebugVisualiser::VisualiseFOV(const CPed& BANK_ONLY(ped)) const
{
#if __BANK
	ped.GetPedIntelligence()->GetPedPerception().DrawVisualField();
#endif // __BANK
}

void CPedDebugVisualiser::VisualisePosition(const CPed& ped) const
{
	static const Color32 iColRed(0xff,0,0,0xff);
	static const Color32 iColDarkRed(0x7f,0,0,0xff);
	static const Color32 iColGreen(0,0xff,0,0xff);
	static const Color32 iColDarkGreen(0,0x7f,0,0xff);
	static const Color32 iColBlue(0,0,0xff,0xff);
	static const Color32 iColDarkBlue(0,0,0x7f,0xff);

	static const Color32 iColWhite(0xff,0xff,0xff,0xff);
	static const Color32 iColGrey(0x7f,0x7f,0x7f,0xff);
	static const Color32 iColYellow(0xff,0xff,0,0xff);
	static const Color32 iColDarkYellow(0x7f,0x7f,0,0xff);

	Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());

	//***********************************************
	// Show the ped's current & desired heading
	//***********************************************

	Matrix34 headingMat, pitchMat;

	float fCurrentHeading = ped.GetCurrentHeading();
	float fCurrentPitch = ped.GetCurrentPitch();

	pitchMat.MakeRotateX(fCurrentPitch);
	headingMat.MakeRotateZ(fCurrentHeading);
	headingMat.DotFromLeft(pitchMat);
	Vector3 vCurrentHeading = headingMat.b;

	float fDesiredHeading = ped.GetDesiredHeading();
	float fDesiredPitch = ped.GetDesiredPitch();

	pitchMat.MakeRotateX(fDesiredPitch);
	headingMat.MakeRotateZ(fDesiredHeading);
	headingMat.DotFromLeft(pitchMat);
	Vector3 vDesiredHeading = headingMat.b;

	grcDebugDraw::Line(vPedPos, vPedPos + vCurrentHeading, iColGrey, iColWhite);
	grcDebugDraw::Line(vPedPos, vPedPos + vDesiredHeading, iColDarkYellow, iColYellow);

	//***********************************************
	//	Show the ped's matrix
	//***********************************************

	static const float fAxisLength = 0.5f;
	Matrix34 pedMat;
	ped.GetMatrixCopy(pedMat);

	grcDebugDraw::Line(vPedPos, vPedPos + pedMat.a*fAxisLength, iColDarkRed, iColRed);
	grcDebugDraw::Line(vPedPos, vPedPos + pedMat.b*fAxisLength, iColDarkGreen, iColGreen);
	grcDebugDraw::Line(vPedPos, vPedPos + pedMat.c*fAxisLength, iColDarkBlue, iColBlue);

	// Render the facing direction
	if(ped.GetBoneIndexFromBoneTag(BONETAG_FACING_DIR) != -1)
	{
		float fFacingDirectionHeading = ped.GetFacingDirectionHeading();
		Matrix34 m;
		m.MakeRotateZ(fFacingDirectionHeading);
		grcDebugDraw::Line(vPedPos, vPedPos + (m.b * 0.5f), Color_black);
	}
}


CPedDebugVisualiser::CTextDisplayFlags	CPedDebugVisualiser::m_textDisplayFlags;

CPedDebugVisualiser::CTextDisplayFlags::CTextDisplayFlags() :
m_bDisplayAnimList(false),
m_bDisplayTasks(false),
m_bDisplayPersonality(false)
{

}

void CPedDebugVisualiser::VisualiseMissionStatus( const CPed& ped ) const
{
	Vector3 WorldCoors;
	u8 iAlphaNotUsed = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlphaNotUsed))
	{
		return;
	}

	int iNoOfTexts=0;

	Color32 colour = Color_green;
	if( ped.IsNetworkClone() )
	{
		grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "Clone char");
		iNoOfTexts++;
	}
	else
		if( ped.PopTypeIsRandom() )
		{
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "Random char");
			iNoOfTexts++;
		}
		else if( ped.IsAPlayerPed() )
		{
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), "Player char");
			iNoOfTexts++;
		}
		else if( ped.PopTypeIsMission() )
		{
			char debugText[50];
			colour = Color_blue;
			sprintf( debugText, "Mission char(unknown)" );
#if __DEV
			strStreamingObjectName pMissionName;
			if (ped.GetThreadThatCreatedMe())
			{
				sprintf( debugText, "Mission char(%s)", const_cast<GtaThread*>(ped.GetThreadThatCreatedMe())->GetScriptName());
			}
#endif // __DEV
			grcDebugDraw::Text( WorldCoors, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
			iNoOfTexts++;
		}
}

void AdjustMovementTurnMultiplier()
{
}

// ------------------------------------------------------------ //

// -------------------------------------- debug menu control -- //

CPedDebugVisualiserMenu::CMenuFlags	CPedDebugVisualiserMenu::ms_menuFlags;
int	CPedDebugVisualiserMenu::ms_nRagdollDebugDrawBone = -2;
float CPedDebugVisualiserMenu::ms_fRagdollApplyForceFactor = 0.8f;

float CPedDebugVisualiserMenu::ms_fNMInteractiveShooterShotForce = 30.0f;
float CPedDebugVisualiserMenu::ms_fNMInteractiveGrabLeanAmount = 2.5f;

bool CPedDebugVisualiserMenu::ms_bUseHandCuffs = false;
bool CPedDebugVisualiserMenu::ms_bUseAnkleCuffs = false;
bool CPedDebugVisualiserMenu::ms_bFreezeRelativeWristPositions = true;
bool CPedDebugVisualiserMenu::ms_bAnimPoseArms = true;
float CPedDebugVisualiserMenu::ms_bHandCuffLength = 0.06f;
bool CPedDebugVisualiserMenu::ms_bUseLeftArmIK = false;
bool CPedDebugVisualiserMenu::ms_bUseRightArmIK = false;
Vector3 CPedDebugVisualiserMenu::ms_vTargetLeftArmIK = VEC3_ZERO;
Vector3 CPedDebugVisualiserMenu::ms_vTargetRightArmIK = VEC3_ZERO;
float CPedDebugVisualiserMenu::ms_fArmIKBlendInTime = PEDIK_ARMS_FADEIN_TIME;
float CPedDebugVisualiserMenu::ms_fArmIKBlendOutTime = PEDIK_ARMS_FADEOUT_TIME;
float CPedDebugVisualiserMenu::ms_fArmIKBlendInRange = 0.0f;
float CPedDebugVisualiserMenu::ms_fArmIKBlendOutRange = 0.0f;
bool CPedDebugVisualiserMenu::ms_bUseBoneAndOffset = false;
bool CPedDebugVisualiserMenu::ms_bTargetInWorldSpace = false;
bool CPedDebugVisualiserMenu::ms_bTargetWRTHand = false;
bool CPedDebugVisualiserMenu::ms_bTargetWRTPointHelper = false;
bool CPedDebugVisualiserMenu::ms_bTargetWRTIKHelper = false;
bool CPedDebugVisualiserMenu::ms_bUseAnimAllowTags = false;
bool CPedDebugVisualiserMenu::ms_bUseAnimBlockTags = false;

bool CPedDebugVisualiserMenu::ms_bHeadIKEnable;
Vector3 CPedDebugVisualiserMenu::ms_vHeadIKTarget = VEC3_ZERO;
bool CPedDebugVisualiserMenu::ms_bHeadIKUseWorldSpace = false;
float CPedDebugVisualiserMenu::ms_fHeadIKBlendInTime = PEDIK_ARMS_FADEIN_TIME;
float CPedDebugVisualiserMenu::ms_fHeadIKBlendOutTime = PEDIK_ARMS_FADEOUT_TIME;
float CPedDebugVisualiserMenu::ms_fHeadIKTime = 10.0f;

bool CPedDebugVisualiserMenu::ms_bSlowTurnRate = false;
bool CPedDebugVisualiserMenu::ms_bFastTurnRate = false;
bool CPedDebugVisualiserMenu::ms_bWideYawLimit = false;
bool CPedDebugVisualiserMenu::ms_bWidestYawLimit = false;
bool CPedDebugVisualiserMenu::ms_bWidePitchLimit = false;
bool CPedDebugVisualiserMenu::ms_bWidestPitchLimit = false;
bool CPedDebugVisualiserMenu::ms_bNarrowYawLimit = false;
bool CPedDebugVisualiserMenu::ms_bNarrowestYawLimit = false;
bool CPedDebugVisualiserMenu::ms_bNarrowPitchLimit = false;
bool CPedDebugVisualiserMenu::ms_bNarrowestPitchLimit = false;
bool CPedDebugVisualiserMenu::ms_bWhileNotInFOV = false;
bool CPedDebugVisualiserMenu::ms_bUseEyesOnly = false;

Vector3 CPedDebugVisualiserMenu::ms_vLookIKTarget = VEC3_ZERO;
bool CPedDebugVisualiserMenu::ms_bLookIKEnable = false;
bool CPedDebugVisualiserMenu::ms_bLookIKUseWorldSpace = false;
bool CPedDebugVisualiserMenu::ms_bLookIKUseCamPos = false;
bool CPedDebugVisualiserMenu::ms_bLookIkUsePlayerPos = false;
bool CPedDebugVisualiserMenu::ms_bLookIKUpdateTarget = false;
bool CPedDebugVisualiserMenu::ms_bLookIkUseTags = false;
bool CPedDebugVisualiserMenu::ms_bLookIkUseTagsFromAnimViewer = false;
bool CPedDebugVisualiserMenu::ms_bLookIkUseAnimAllowTags = false;
bool CPedDebugVisualiserMenu::ms_bLookIkUseAnimBlockTags = false;
u8 CPedDebugVisualiserMenu::ms_uRefDirTorso = 0;
u8 CPedDebugVisualiserMenu::ms_uRefDirNeck = 0;
u8 CPedDebugVisualiserMenu::ms_uRefDirHead = 0;
u8 CPedDebugVisualiserMenu::ms_uRefDirEye = 0;
u8 CPedDebugVisualiserMenu::ms_uRotLimTorso[2] = { 0, 0 };
u8 CPedDebugVisualiserMenu::ms_uRotLimNeck[2] = { 0, 0 };
u8 CPedDebugVisualiserMenu::ms_uRotLimHead[2] = { 3, 3 };
u8 CPedDebugVisualiserMenu::ms_uTurnRate = 1;
u8 CPedDebugVisualiserMenu::ms_auBlendRate[2] = { 2, 2 };
u8 CPedDebugVisualiserMenu::ms_uHeadAttitude = 3;
u8 CPedDebugVisualiserMenu::ms_auArmComp[2] = { 0, 0 };

CPedDebugVisualiser	CPedDebugVisualiserMenu::m_debugVis;

bkCombo * CPedDebugVisualiserMenu::m_pRelationshipsCombo = NULL;
bkCombo * CPedDebugVisualiserMenu::m_pTasksCombo = NULL;
bkCombo * CPedDebugVisualiserMenu::m_pEventsCombo = NULL;
bkCombo * CPedDebugVisualiserMenu::m_pFormationCombo = NULL;

s32 CPedDebugVisualiserMenu::m_iRelationshipsComboIndex = 0;
atArray<s32>	CPedDebugVisualiserMenu::m_ScenarioComboTypes;
s32 CPedDebugVisualiserMenu::m_iScenarioComboIndex = 0;
atArray<const char*> CPedDebugVisualiserMenu::m_scenarioNames;
atArray<CPedDebugVisualiserMenu::eFocusPedTasks>	CPedDebugVisualiserMenu::m_TaskComboTypes;
s32 CPedDebugVisualiserMenu::m_iTaskComboIndex = 0;
atArray<CPedDebugVisualiserMenu::eFocusPedEvents>	CPedDebugVisualiserMenu::m_EventComboTypes;
s32 CPedDebugVisualiserMenu::m_iEventComboIndex = 0;
atArray<u32> CPedDebugVisualiserMenu::m_FormationComboIDs;
s32 CPedDebugVisualiserMenu::m_iFormationComboIndex = 0;
float CPedDebugVisualiserMenu::m_fFormationSpacing = 4.75f;
float CPedDebugVisualiserMenu::m_fFormationAccelDist = 9.5f;
float CPedDebugVisualiserMenu::m_fFormationDecelDist = 4.25f;
u32 CPedDebugVisualiserMenu::m_iFormationSpacingScriptId = (u32)THREAD_INVALID;
bool CPedDebugVisualiserMenu::ms_bDebugSearchPositions = false;
bool CPedDebugVisualiserMenu::ms_bRagdollContinuousTestCode = false;
bool CPedDebugVisualiserMenu::ms_bRenderRagdollTypeText = true;
bool CPedDebugVisualiserMenu::ms_bRenderRagdollTypeSpheres = true;
#if __DEV
bkToggle * CPedDebugVisualiserMenu::m_pToggleAssertIfFocusPedFailsToFindNavmeshRoute = NULL;
bool CPedDebugVisualiserMenu::m_bAssertIfFocusPedFailsToFindNavmeshRoute = false;
#endif // __DEV
bool CPedDebugVisualiserMenu::ms_bRouteRoundCarTest = false;
ePedConfigFlagsBitSet CPedDebugVisualiserMenu::ms_DebugPedConfigFlagsBitSet;
ePedResetFlagsBitSet CPedDebugVisualiserMenu::ms_DebugPedPostPhysicsResetFlagsBitSet;
bool CPedDebugVisualiserMenu::ms_bDebugPostPhysicsResetFlagActive = false;

TNavMeshRouteDebugToggles CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles;

CPedDebugVisualiserMenu::CMenuFlags::CMenuFlags() :
m_bDisplayAddresses(false),
m_bDisplayTasks(false),
m_bDisplayTaskHierarchy(false),
m_bDisplayEvents(false),
m_bDisplayHitSides(false),
m_bDisplayBoundingVolumes(false),
m_bDisplayGroupInfo(false),
m_bDisplayAnimatedCol(false),
m_bDisplayCombatDirectorAnimStreaming(false),
m_bDisplayPrimaryTaskStateHistory(false),
m_bDisplayMovementTaskStateHistory(false),
m_bDisplayMotionTaskStateHistory(false),
m_bDisplaySecondaryTaskStateHistory(false),
m_bVisualisePedsInVehicles(false),
m_bOnlyDisplayForFocus(false),
m_bFocusPedDisplayIn2D(false),
m_bOnlyDisplayForPlayers(false),
m_bDontDisplayForMounts(false),
m_bDontDisplayForDeadPeds(false),
m_bDontDisplayForPlayer(false),
m_bDontDisplayForRiders(false),
m_bDisplayDebugShooting(false),
m_bVisualiseRelationships(false),
m_bVisualiseDefensiveAreas(false),
m_bDisableVisualiseDefensiveAreasForDeadPeds(false),
m_bVisualiseSecondaryDefensiveAreas(false),
m_bVisualiseAttackWindowForFocusPed(false),
m_bVisualiseNearbyLadders(false),
m_bDisplayPedBravery(false),
m_bDisplayCoverSearch(false),
m_bDisplayCoverLineTests(false),
m_bDisplayPedGroups(false),
m_bDisableEvasiveDives(false),
m_bDisableFlinchReactions(false),
m_bLogPedSpeech(false),
m_bDisplayDebugAccuracy(false),
m_bDisplayScriptTaskHistoryCodeTasks(false),
m_bDisplayPedSpatialArray(false),
m_bDisplayVehicleSpatialArray(false),
m_bLogAIEvents(false),
m_bDisplayDamageRecords(false),
m_bRenderInitialSpawnPoint(false)
{}




void
CPedDebugVisualiserMenu::ToggleDisplayAddresses(void)
{
	ms_menuFlags.m_bDisplayAddresses = !ms_menuFlags.m_bDisplayAddresses;
}
void
CPedDebugVisualiserMenu::ToggleVisualisePedsInVehicles(void)
{
	ms_menuFlags.m_bVisualisePedsInVehicles = !ms_menuFlags.m_bVisualisePedsInVehicles;
}
void
CPedDebugVisualiserMenu::ToggleDisplayGroupInfo(void)
{
	ms_menuFlags.m_bDisplayGroupInfo = !ms_menuFlags.m_bDisplayGroupInfo;
}
void
CPedDebugVisualiserMenu::ToggleDisplayAnims(void)
{
	m_debugVis.m_textDisplayFlags.m_bDisplayAnimList = !m_debugVis.m_textDisplayFlags.m_bDisplayAnimList;
}
void
CPedDebugVisualiserMenu::ToggleDisplayTaskHeirarchies(void)
{
	ms_menuFlags.m_bDisplayTasks = !ms_menuFlags.m_bDisplayTasks;
}
void
CPedDebugVisualiserMenu::ToggleDisplayEvents(void)
{
	ms_menuFlags.m_bDisplayEvents = !ms_menuFlags.m_bDisplayEvents;
}
void
CPedDebugVisualiserMenu::ToggleDisplayPersonality(void)
{
	m_debugVis.m_textDisplayFlags.m_bDisplayPersonality = !m_debugVis.m_textDisplayFlags.m_bDisplayPersonality;
}
void
CPedDebugVisualiserMenu::ToggleDisplayAnimatedCol(void)
{
	ms_menuFlags.m_bDisplayAnimatedCol = !ms_menuFlags.m_bDisplayAnimatedCol;
}
void
CPedDebugVisualiserMenu::ToggleDisplayCoverPoints(void)
{
	CCoverDebug::ms_Tunables.m_RenderCoverPoints = !CCoverDebug::ms_Tunables.m_RenderCoverPoints;
}

void
CPedDebugVisualiserMenu::TogglePlayerInvincibility(void)
{
	CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth = false; // Disable other invincibility mode

	if (CPlayerInfo::ms_bDebugPlayerInvincible)
	{
		char *pString = TheText.Get("HELP_INVIN");
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, pString);
	}
	else
	{
		char *pString = TheText.Get("HELP_NOINVIN");
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, pString);
	}
}

void
CPedDebugVisualiserMenu::TogglePlayerInvincibilityRestoreHealth(void)
{
	CPlayerInfo::ms_bDebugPlayerInvincible = false; // Disable other invincibility mode

	if (CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth)
	{
		char *pString = TheText.Get("HELP_INVIN");
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, pString);
	}
	else
	{
		char *pString = TheText.Get("HELP_NOINVIN");
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, pString);
	}
}

static const char * pPedDebuggingText[] =
{
	"Off",					// eOff
	"TasksFull",			// eTasksFullDebugging
	"TasksNoText",			// eTasksNoText
	"VehicleEntry",			// eVehicleEntryDebug
	"PedNames",				// ePedNames
	"ModelInformation",		// eModelInformation
	"Animation",			// eAnimationDebugging
	"Facial",				// eFacialDebugging
	"Visibility",			// eVisibilityDebugging
	"Gesture",				// eGestureDebugging
	"IK",					// eIKDebugging
	"Combat",				// eCombatDebugging
	"Relationships",		// eRelationshipDebugging
	"Groups",				// ePedGroupDebugging
	"Movement",				// eMovementDebugging
	"Ragdoll",				// eRagdollDebugging
	"RagdollType",			// eRagdollTypeDebugging
	"QueriableState",		// eQueriableStateDebugging
	"AmbientAnimations",	// ePedAmbientAnimations
	"PedDragging",			// ePedDragging
	"LookAboutScoring",		// ePedLookAboutScoring
	"Scanners",				// eScanners
	"Avoidance",			// eAvoidance
	"ScriptHistory",		// eScriptHistory
	"AudioContexts",		// eAudioContexts
	"eTaskHistory",			// eTaskHistory
	"CurrentEvents",		// eCurrentEvents
	"EventHistory",			// eEventHistory
	"MissionStatus",		// eMissionStatus
	"Navigation",			// eNavigation
	"Stealth",				// eStealth
	"AssistedMovement",		// eAssistedMovement
	"Attachments",			// eAttachments
	"LOD",					// eLOD
	"Motivation",			// eMotivation
	"Wanted",				// eWanted
	"Order",				// eOrder
	"Personalities",		// ePersonalities
	"Ped Flags",			// ePedFlags
	"Damage",				// eDamage
	"Combat Director",		// eCombatDirector
	"Targeting",			// eTargeting
	"Focus AI Ped Targeting",	// eFocusAiPedTargeting
	"Population Info",		// ePopulationInfo
	"Arrest Info",			// eArrestInfo
	"Vehicle Lock Info",	// eVehLockInfo
	"Player Cover Search Info",	// ePlayerCoverSearchInfo
	"Player Info",			// ePlayerInfo
	"Parachute Info",			// eParachuteInfo
	"Melee",				// eMelee

	"Combat (Simple)", // eSimpleCombat

	0
};

CompileTimeAssert(sizeof(pPedDebuggingText) / sizeof(const char*) == (CPedDebugVisualiser::eNumModes + 1));

static const char * pNMDebuggingText[] =
{
	"Off",
	"Interactive Shoot",
	"Interactive Grab",
	0
};
static const char * pRagdollComponentsText[] =
{
	// Remember to check that this is in sync with the enums in PedDefines.h!
	"Buttocks",
	"Thigh_Left",
	"Shin_Left",
	"Foot_Left",
	"Thigh_Right",
	"Shin_Right",
	"Foot_Right",
	"Spine0",
	"Spine1",
	"Spine2",
	"Spine3",
	"Clavicle_Left",
	"Upper_Arm_Left",
	"Lower_Arm_Left",
	"Hand_Left",
	"Clavicle_Right",
	"Upper_Arm_Right",
	"Lower_Arm_Right",
	"Hand_Right",
	"Neck",
	"Head"
};

static bool g_bForceAllCops	= false;

bool CPedDebugVisualiserMenu::GetForceAllCops(void)
{
	return g_bForceAllCops;
}
void CPedDebugVisualiserMenu::SetForceAllCops(bool bForce)
{
	g_bForceAllCops = bForce;
}

//-------------------------------------------------------------------------
// Returns true if ped dragging is enabled
//-------------------------------------------------------------------------
bool CPedDebugVisualiserMenu::IsDebugDefensiveAreaDraggingActivated(void)
{
	return CCombatDebug::ms_bDefensiveAreaDragging;
}

bool CPedDebugVisualiserMenu::IsDebugDefensiveAreaSecondary()
{
	return CCombatDebug::ms_bUseSecondaryDefensiveArea;
}

//-------------------------------------------------------------------------
// Updates ped dragging
//-------------------------------------------------------------------------
void CPedDebugVisualiserMenu::UpdateDefensiveAreaDragging(void)
{
	static bool bDragging = false;
	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT )
	{
		bDragging = true;
	}
	else if( !(ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT) )
	{
		Vector3	vPos;
		if( bDragging &&
			CDebugScene::FocusEntities_Get(0) &&
			CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));

			CDefensiveArea* pDefensiveArea = !IsDebugDefensiveAreaSecondary() ?
				pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetPrimaryDefensiveArea() :
				pFocusPed->GetPedIntelligence()->GetDefensiveAreaManager()->GetSecondaryDefensiveArea(true);
			if( pDefensiveArea->IsActive() &&
				CDebugScene::GetWorldPositionUnderMouse(vPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE))
			{
				Vector3 vVec1, vVec2;
				Vector3 vCentre;
				float fWidth, fMaxRadiusSq;
				const CEntity* pAttachedEntity = pDefensiveArea->GetAttachedEntity();
				pDefensiveArea->GetCentreAndMaxRadius(vCentre,fMaxRadiusSq);

				Vector3 vPreviousDirection = pDefensiveArea->GetDefaultCoverFromPosition();

				pDefensiveArea->Get(vVec1, vVec2, fWidth);
				Vector3 vMovingDistance = vPos - vCentre;
				vVec1 += vMovingDistance;
				vVec2 += vMovingDistance;
				if( pAttachedEntity )
				{
					const Vector3 vecAttachedEntityPos = VEC3V_TO_VECTOR3(pAttachedEntity->GetTransform().GetPosition());
					vVec1 -= vecAttachedEntityPos;
					vVec2 -= vecAttachedEntityPos;
				}
				if( pDefensiveArea->GetType() == CDefensiveArea::AT_AngledArea )
				{
					pDefensiveArea->Set(vVec1, vVec2, fWidth, pAttachedEntity);
				}
				else if( pDefensiveArea->GetType() == CDefensiveArea::AT_Sphere )
				{
					pDefensiveArea->SetAsSphere(vVec1, fWidth, pAttachedEntity);
				}
				pDefensiveArea->SetDefaultCoverFromPosition(vPreviousDirection);
			}
		}
	}
	else
	{
		if( bDragging )
		{

		}
		bDragging = false;
	}
}

void OnRecalcAIBoundForObject()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
		if(pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);

			if(!pFocusPed) return;

			const CObject * pObj = pFocusPed->GetPedIntelligence()->GetClosestObjectInRange();
			if(pObj)
			{
				CEntityBoundAI boundAI(*pObj, pFocusPed->GetTransform().GetPosition().GetZf(), 0.35f, true);
			}
		}
	}
}
extern void UpdatePos1(void);
extern void UpdatePos2(void);
#if __DEV
void FindEntityUnderPlayer()
{
	CPed * pPlayer = FindPlayerPed();
	const Vector3 vStart = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	const Vector3 vEnd = vStart - Vector3(0.0f,0.0f,5.0f);


	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetExcludeEntity(pPlayer);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		phInst* pInst = probeResult[0].GetHitInst();
		if(pInst)
		{
			CEntity * pEntity = (CEntity*)pInst->GetUserData();
			if(pEntity->IsArchetypeSet())
			{
				CBaseModelInfo * pModelInfo = pEntity->GetBaseModelInfo();
				const char * pModelName = pModelInfo->GetModelName();
				if(pModelName)
				{
					printf("ModelName : %s\n", pModelName);
				}
			}
		}
	}
}

static CEntity* gpFocusEntity=NULL;
void CPedDebugVisualiserMenu::OnAssertIfFocusPedFailsToFindNavmeshRoute()
{
	gpFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(gpFocusEntity && gpFocusEntity->GetIsTypePed())
	{
		((CPed *)gpFocusEntity)->GetPedIntelligence()->m_bAssertIfRouteNotFound = m_bAssertIfFocusPedFailsToFindNavmeshRoute;
	}
}

#endif // __DEV

bool g_nmRequestAnimPose = false;
bool g_AllowLegInterpenetration = false;
bkGroup* g_NmBlendOutSetGoup = NULL;

void CPedDebugVisualiserMenu::ToggleAllowLegInterpenetration(void)
{
	FRAGNMASSETMGR->SetAllowLegInterpenetration(g_AllowLegInterpenetration);
}

void CPedDebugVisualiserMenu::PrintRagdollUsageData(void)
{
	if (fragInstNMGta::ms_bLogUsedRagdolls)
	{
		Printf("\n\n");

		if (CTheScripts::GetPlayerIsOnAMission())
		{
			// Display max ragdolls used for the level
			Displayf("[RAGDOLL WATERMARK] A maximum of %d NM AGENTS and %d RAGE RAGDOLLS have been used individually during [%s].  %d fallback animations were used.",
				fragInstNMGta::ms_MaxNMAgentsUsedCurrentLevel, fragInstNMGta::ms_MaxRageRagdollsUsedCurrentLevel, CDebug::GetCurrentMissionName(), fragInstNMGta::ms_NumFallbackAnimsUsedCurrentLevel);
			Displayf("[RAGDOLL WATERMARK] At peak usage this mission, a total of %d ragdolls were used simultaneously.  %d of those were NM agents and %d were rage ragdolls.",
				fragInstNMGta::ms_MaxTotalRagdollsUsedCurrentLevel, fragInstNMGta::ms_MaxNMAgentsUsedInComboCurrentLevel, fragInstNMGta::ms_MaxRageRagdollsUsedInComboCurrentLevel);
		}

		// Display max ragdolls used globally
		Displayf("[RAGDOLL WATERMARK] A maximum of %d NM AGENTS and %d RAGE RAGDOLLS were used individually (GLOBAL).  %d fallback animations were used.",
			fragInstNMGta::ms_MaxNMAgentsUsedGlobally, fragInstNMGta::ms_MaxRageRagdollsUsedGlobally, fragInstNMGta::ms_NumFallbackAnimsUsedGlobally);
		Displayf("[RAGDOLL WATERMARK] At peak usage GLOBALLY, a total of %d ragdolls were used simultaneously.  %d of those were NM agents and %d were rage ragdolls.",
			fragInstNMGta::ms_MaxTotalRagdollsUsedGlobally, fragInstNMGta::ms_MaxNMAgentsUsedInComboGlobally, fragInstNMGta::ms_MaxRageRagdollsUsedInComboGlobally);
		Printf("\n\n");
	}
}

void CPedDebugVisualiserMenu::SpewRagdollTaskInfo(void)
{
#if __ASSERT
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
		pFocusPed->SpewRagdollTaskInfo();
	}
#endif  //__ASSERT
}

void CPedDebugVisualiserMenu::ToggleNaturalMotionAnimPose(void)
{
	if(g_nmRequestAnimPose)
		CTaskNMControl::m_DebugFlags |= CTaskNMControl::DO_CLIP_POSE;
	else
		CTaskNMControl::m_DebugFlags &= ~CTaskNMControl::DO_CLIP_POSE;
}

void CPedDebugVisualiserMenu::ToggleNaturalMotionHandCuffs(void)
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
		if(!pFocusPed) return;

		pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsHandCuffed, ms_bUseHandCuffs );
		pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming, ms_bUseHandCuffs );

		Assert(pFocusPed->GetRagdollConstraintData());
		if (pFocusPed->GetRagdollConstraintData())
			pFocusPed->GetRagdollConstraintData()->EnableHandCuffs(pFocusPed, ms_bUseHandCuffs);
	}
}

void CPedDebugVisualiserMenu::ToggleNaturalMotionAnkleCuffs(void)
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEntity);
		if(!pFocusPed) return;

		pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsAnkleCuffed, ms_bUseAnkleCuffs );
		pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DiesInstantlyWhenSwimming, ms_bUseAnkleCuffs );

		Assert(pFocusPed->GetRagdollConstraintData());
		if (pFocusPed->GetRagdollConstraintData())
			pFocusPed->GetRagdollConstraintData()->EnableBoundAnkles(pFocusPed, ms_bUseAnkleCuffs);
	}
}
bool g_nmShowNmControlFlags = false;
void CPedDebugVisualiserMenu::ToggleNaturalMotionFlagDebug(void)
{
	CTaskNMControl::m_bDisplayFlags = g_nmShowNmControlFlags;
}

bool g_nmAPIDump = false;
void CPedDebugVisualiserMenu::ToggleNaturalMotionAPILog(void)
{
	FRAGNMASSETMGR->EnableAPILogging(g_nmAPIDump);
}

bool g_nmDistributedTasksEnabled = true;
void CPedDebugVisualiserMenu::ToggleDistributedTasksEnabled(void)
{
	g_nmDistributedTasksEnabled = !FRAGNMASSETMGR->AreDistributedTasksEnabled();
	FRAGNMASSETMGR->SetDistributedTasksEnabled(g_nmDistributedTasksEnabled);
}

void CPedDebugVisualiserMenu::ReloadTaskParameters(void)
{
	CTaskNMBehaviour::LoadTaskParameters();
}

void CPedDebugVisualiserMenu::ReloadBlendFromNmSets(void)
{
	CNmBlendOutSetManager::Shutdown();
	CNmBlendOutSetManager::Load();

	UpdateBlendFromNmSetsWidgets();
}

void CPedDebugVisualiserMenu::UpdateBlendFromNmSetsWidgets(void)
{
	// update the editor widgets
	if (g_NmBlendOutSetGoup && ms_pBank)
	{
		while(g_NmBlendOutSetGoup->GetChild())
		{
			g_NmBlendOutSetGoup->GetChild()->Destroy();
		}

		bool bSetGroup = false;
		if (ms_pBank->GetCurrentGroup() == ms_pBank)
		{
			ms_pBank->SetCurrentGroup(*g_NmBlendOutSetGoup);
			bSetGroup = true;
		}

		if (Verifyf(ms_pBank->GetCurrentGroup() == g_NmBlendOutSetGoup, "CPedDebugVisualiserMenu::UpdateBlendFromNmSetsWidgets: Invalid group set"))
		{
			g_NmBlendOutSetGoup->AddButton("Load nm blend out sets", ReloadBlendFromNmSets, "");
			g_NmBlendOutSetGoup->AddButton("Save nm blend out sets", CNmBlendOutSetManager::Save, "");
			PARSER.AddWidgets(*ms_pBank, &g_NmBlendOutPoseManager);

			if (bSetGroup)
			{
				ms_pBank->UnSetCurrentGroup(*g_NmBlendOutSetGoup);
			}
		}
	}
}

void CPedDebugVisualiserMenu::ReloadWeaponTargetSequences(void)
{
#if USE_TARGET_SEQUENCES
	CTargetSequenceManager::ShutdownMetadata(SHUTDOWN_SESSION);
	CTargetSequenceManager::InitMetadata(INIT_SESSION);
#endif // USE_TARGET_SEQUENCES
}

void CPedDebugVisualiserMenu::SendNMFallOverInstructionToFocusEntity(void)
{
	CPed* pPed = GetFocusPed();
	if (pPed)
	{
		CTaskNMBehaviour* pTask = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);

		if (pTask)
		{
			pTask->ForceFallOver();
		}
	}
}

void CPedDebugVisualiserMenu::CreateJetTest()
{
	CPedPopulation::RemoveAllPedsHardNotPlayer();
	CVehiclePopulation::RemoveAllVehs();

	CVehicleIntelligence::m_bDisplayCarAiTaskInfo = true;
	CVehicleIntelligence::m_eFilterVehicleDisplayByType = 2;
	CVehicleIntelligence::ms_fDisplayVehicleAIRange = 500.0f;

	CPed::CreateBank();

	for(u32 pedPicId = 0; pedPicId < CPedVariationDebug::GetNumPedNames(); pedPicId++)
	{
		if ( strstr(CPedVariationDebug::pedNames[pedPicId],"S_M_Y_Cop_01") )
		{
			CPedVariationDebug::SetCrowdPedSelection(0,pedPicId);
			CPedVariationDebug::SetCrowdPedSelection(1,pedPicId);
			CPedVariationDebug::SetCrowdPedSelection(2,pedPicId);
			CPedVariationDebug::SetCrowdPedSelection(3,pedPicId);
			break;
		}
	}

	u32 pedVehId = 0;
	for(pedVehId = 0; pedVehId < CVehicleFactory::vehicleNames.GetCount(); pedVehId++)
	{
		if ( !strcmp(CVehicleFactory::vehicleNames[pedVehId],"LAZER") )
		{
			CVehicleFactory::currVehicleNameSelection = pedVehId;
			break;
		}
	}

	if( pedVehId == (u32)CVehicleFactory::vehicleNames.GetCount())
		return;

	CVehicleFactory::CarNumChangeCB();

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	CVehicle* pPlayerVehicle = NULL;

	if( pLocalPlayer)
	{
		if(!pLocalPlayer->GetIsInVehicle())
		{
			CVehicleFactory::CreateCar();
			pPlayerVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();
		}
		else
		{
			pPlayerVehicle = pLocalPlayer->GetMyVehicle();
		}

		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByBullets = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByFlames = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByCollisions = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByMelee = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByAnything = true;

		pLocalPlayer->GetPedIntelligence()->FlushImmediately(true);
		pLocalPlayer->SetPedInVehicle(pPlayerVehicle, 0, CPed::PVF_Warp);
		pPlayerVehicle->SwitchEngineOn(true);

		Vector3 vStart(0.0f, -2000.0f, 1000.0f);
		pPlayerVehicle->Teleport(vStart);
		Vector3 vVelocity(0.0f, 50.0f, 0.0f);
		pPlayerVehicle->SetVelocity(vVelocity);

		if( !ms_bTestPlanePlayerControl)
		{
			sVehicleMissionParams params;
			params.SetTargetPosition(m_goToTarget);
			params.m_fTargetArriveDist = 10.0f;
			params.m_fCruiseSpeed = 1000.0f;
			aiTask *pVehicleTask = rage_new CTaskVehicleGoToPlane(params);
			CTask* pTaskToGiveToFocusPed = rage_new CTaskControlVehicle(pPlayerVehicle, pVehicleTask);

			CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGiveToFocusPed,false,E_PRIORITY_GIVE_PED_TASK);
			pLocalPlayer->GetPedIntelligence()->AddEvent(event);
		}
	}

	switch (ms_iTestPlaneType)
	{
	case 0:  //chase/attack player
		{
			for( u32 index = 0; index < ms_iTestPlaneNumToSpawn; ++index)
			{
				CVehicleFactory::CreateCar();
				CVehicle* pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();

				Vector3 vStart(((ms_iTestPlaneNumToSpawn-1) * -100.0f / 2.0f) + (index * 100.0f), -2200.0f, 1000.0f);
				pFocusVehicle->Teleport(vStart);
				pFocusVehicle->SwitchEngineOn(true);
				Vector3 vVelocity(0.0f, 50.0f, 0.0f);
				pFocusVehicle->SetVelocity(vVelocity);

				CPedVariationDebug::populateCarCB(0,1);
				pFocusVehicle->GetDriver()->SetPedInVehicle(pFocusVehicle, 0, CPed::PVF_Warp);

				if(pFocusVehicle->GetDriver())
				{
					CPed* pFocusPed = pFocusVehicle->GetDriver();
					if( pFocusPed )
					{
						sVehicleMissionParams params;
						params.SetTargetPosition(VEC3V_TO_VECTOR3(pPlayerVehicle->GetTransform().GetPosition()));
						params.SetTargetEntity(pPlayerVehicle);
						params.m_fTargetArriveDist = 10.0f;
						params.m_fCruiseSpeed = 1000.0f;

						CAITarget rTarget;
						rTarget.SetEntity(pPlayerVehicle);

						CTask* pTaskToGiveToFocusPed = NULL;

						if( ms_bTestPlaneAttackPlayer )
						{
							CTaskThreatResponse* pTaskThreatResponse = rage_new CTaskThreatResponse( pLocalPlayer );
							pTaskThreatResponse->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
							pTaskToGiveToFocusPed = pTaskThreatResponse;
						}
						else
						{
							aiTask *pVehicleTask = rage_new CTaskVehiclePlaneChase(params, rTarget);
							pTaskToGiveToFocusPed = rage_new CTaskControlVehicle(pFocusPed->GetMyVehicle(), pVehicleTask);
						}

						CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGiveToFocusPed,false,E_PRIORITY_GIVE_PED_TASK);
						pFocusPed->GetPedIntelligence()->AddEvent(event);
					}
				}
			}
		}
		break;
	case 1: // on collision course
		{
			CVehicleFactory::CreateCar();
			CVehicle* pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();

			Vector3 vStart(0.0f, -1000.0f, 1000.0f);
			pFocusVehicle->Teleport(vStart);
			pFocusVehicle->SwitchEngineOn(true);
			Vector3 vVelocity(0.0f, -50.0f, 0.0f);
			pFocusVehicle->SetVelocity(vVelocity);
			float playerHeading = pPlayerVehicle->GetTransform().GetHeading();
 			pFocusVehicle->SetHeading(playerHeading + PI);

			CPedVariationDebug::populateCarCB(0,1);
			pFocusVehicle->GetDriver()->SetPedInVehicle(pFocusVehicle, 0, CPed::PVF_Warp);

			if(pFocusVehicle->GetDriver())
			{
				CPed* pFocusPed = pFocusVehicle->GetDriver();
				if( pFocusPed )
				{
					Vector3 vEnd(0.0f, -2000.0f, 1000.0f);
					sVehicleMissionParams params;
					params.SetTargetPosition(vEnd);
					params.m_fTargetArriveDist = 10.0f;
					params.m_fCruiseSpeed = 1000.0f;
					aiTask *pVehicleTask = rage_new CTaskVehicleGoToPlane(params);
					CTask* pTaskToGiveToFocusPed = rage_new CTaskControlVehicle(pFocusVehicle, pVehicleTask);

					CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGiveToFocusPed,false,E_PRIORITY_GIVE_PED_TASK);
					pFocusPed->GetPedIntelligence()->AddEvent(event);
				}
			}
		}
		break;
	case 2: // close to each other
		{
			for( u32 index = 0; index < ms_iTestPlaneNumToSpawn; ++index)
			{
				CVehicleFactory::CreateCar();
				CVehicle* pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();
				float xRandom = fwRandom::GetRandomNumberInRange(-5.0f, 5.0f);
				float yRandom = fwRandom::GetRandomNumberInRange(-5.0f, 5.0f);

				Vector3 vStart(0.0f + xRandom, -2000.0f + yRandom, 1000.0f + (index + 1) * 10.0f);
				pFocusVehicle->Teleport(vStart);
				pFocusVehicle->SwitchEngineOn(true);
				Vector3 vVelocity(0.0f, 50.0f, 0.0f);
				pFocusVehicle->SetVelocity(vVelocity);

				CPedVariationDebug::populateCarCB(0,1);
				pFocusVehicle->GetDriver()->SetPedInVehicle(pFocusVehicle, 0, CPed::PVF_Warp);

				if(pFocusVehicle->GetDriver())
				{
					CPed* pFocusPed = pFocusVehicle->GetDriver();
					if( pFocusPed )
					{
						sVehicleMissionParams params;
						params.SetTargetPosition(m_goToTarget);
						params.m_fTargetArriveDist = 10.0f;
						params.m_fCruiseSpeed = 1000.0f;
						aiTask *pVehicleTask = rage_new CTaskVehicleGoToPlane(params);
						CTask* pTaskToGiveToFocusPed = rage_new CTaskControlVehicle(pFocusVehicle, pVehicleTask);

						CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGiveToFocusPed,false,E_PRIORITY_GIVE_PED_TASK);
						pFocusPed->GetPedIntelligence()->AddEvent(event);
					}
				}
			}
		}
		break;
	default:
		break;
	}
}


void CPedDebugVisualiserMenu::CreateHeliTest()
{
	CPedPopulation::RemoveAllPedsHardNotPlayer();
	CVehiclePopulation::RemoveAllVehs();

	CVehicleIntelligence::m_bDisplayCarAiTaskInfo = true;
	CVehicleIntelligence::m_eFilterVehicleDisplayByType = 7;
	CVehicleIntelligence::ms_fDisplayVehicleAIRange = 500.0f;

	CPed::CreateBank();

	for(u32 pedPicId = 0; pedPicId < CPedVariationDebug::GetNumPedNames(); pedPicId++)
	{
		if ( strstr(CPedVariationDebug::pedNames[pedPicId],"S_M_Y_Cop_01") )
		{
			CPedVariationDebug::SetCrowdPedSelection(0,pedPicId);
			CPedVariationDebug::SetCrowdPedSelection(1,pedPicId);
			CPedVariationDebug::SetCrowdPedSelection(2,pedPicId);
			CPedVariationDebug::SetCrowdPedSelection(3,pedPicId);
			break;
		}
	}

	u32 pedVehId = 0;
	for(pedVehId = 0; pedVehId < CVehicleFactory::vehicleNames.GetCount(); pedVehId++)
	{
		if ( !strcmp(CVehicleFactory::vehicleNames[pedVehId],"BUZZARD") )
		{
			CVehicleFactory::currVehicleNameSelection = pedVehId;
			break;
		}
	}

	if( pedVehId == (u32)CVehicleFactory::vehicleNames.GetCount())
		return;

	CVehicleFactory::CarNumChangeCB();

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	CVehicle* pPlayerVehicle = NULL;

	if( pLocalPlayer)
	{
		if(!pLocalPlayer->GetIsInVehicle())
		{
			CVehicleFactory::CreateCar();
			pPlayerVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();
		}
		else
		{
			pPlayerVehicle = pLocalPlayer->GetMyVehicle();
		}

		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByBullets = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByFlames = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByCollisions = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByMelee = true;
		pPlayerVehicle->m_nPhysicalFlags.bNotDamagedByAnything = true;

		pLocalPlayer->GetPedIntelligence()->FlushImmediately(true);
		pLocalPlayer->SetPedInVehicle(pPlayerVehicle, 0, CPed::PVF_Warp);
		pPlayerVehicle->SwitchEngineOn(true);

		Vector3 vStart(0.0f, -150.0f, 500.0f);
		pPlayerVehicle->Teleport(vStart);
		Vector3 vVelocity(0.0f, 0.0f, 0.0f);
		pPlayerVehicle->SetVelocity(vVelocity);
	}

	for( u32 index = 0; index < 4; ++index)
	{
		Vector3 vStart;
		float heading = 0.0f;
		Vector3 vEnd;

		switch (ms_iTestPlaneType)
		{
		case 0: //spawns 4 hovering helis
			{
				if(index ==0)		vStart = Vector3(-50.0f, 0.0f, 500.0f);
				else if(index == 1)	vStart = Vector3(0.0f, -50.0f, 500.0f);
				else if(index == 2)	vStart = Vector3(-50.0f, -50.0f, 500.0f);
				else if(index == 3)	vStart = Vector3(0.0f, 0.0f, 500.0f);
				vEnd = vStart;
			}
			break;
		case 1: //four in a square heading to center
			{
				if(index ==0){			vStart = Vector3(-50.0f, 50.0f, 500.0f);	heading = 2.35f; }
				else if(index == 1)	{	vStart = Vector3(50.0f, -50.0f, 500.0f);	heading = 0.785f; }
				else if(index == 2)	{	vStart = Vector3(50.0f, 50.0f, 500.0f);		heading = 2.35f; }
				else if(index == 3)	{	vStart = Vector3(-50.0f, -50.0f, 500.0f);	heading = -0.785f; }
				vEnd = Vector3(0.0f,0.0f,500.0f);
			}
			break;
		case 2: //spawns 4 in a line going to same destination
			{
				if(index ==0)		vStart = Vector3(-100.0f, 0.0f, 500.0f);
				else if(index == 1)	vStart = Vector3(-50.0f, 0.0f, 500.0f);
				else if(index == 2)	vStart = Vector3(0.0f, 0.0f, 500.0f);
				else if(index == 3)	vStart = Vector3(50.0f, 0.0f, 500.0f);
				vEnd = m_goToTarget;
			}
			break;
		default:
			break;
		}

		CVehicleFactory::CreateCar();
		CVehicle* pFocusVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();

		pFocusVehicle->Teleport(vStart);
		pFocusVehicle->SwitchEngineOn(true);
		Vector3 vVelocity(0.0f, 0.0f, 0.0f);
		pFocusVehicle->SetVelocity(vVelocity);
		pFocusVehicle->SetHeading(heading);

		CPedVariationDebug::populateCarCB(0,1);
		pFocusVehicle->GetDriver()->SetPedInVehicle(pFocusVehicle, 0, CPed::PVF_Warp);

		if(pFocusVehicle->GetDriver())
		{
			CPed* pFocusPed = pFocusVehicle->GetDriver();
			if( pFocusPed )
			{
				sVehicleMissionParams params;
				params.SetTargetPosition(vEnd);
				params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved;
				params.m_fTargetArriveDist = 10.0f;
				params.m_fCruiseSpeed = 10.0f;

				CTask* pTaskToGiveToFocusPed = NULL;

				aiTask *pVehicleTask = rage_new CTaskVehicleGoToHelicopter(params);
				pTaskToGiveToFocusPed = rage_new CTaskControlVehicle(pFocusPed->GetMyVehicle(), pVehicleTask);

				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGiveToFocusPed,false,E_PRIORITY_GIVE_PED_TASK);
				pFocusPed->GetPedIntelligence()->AddEvent(event);
			}
		}
	}
}

void CPedDebugVisualiserMenu::CreateVehiclePolicePursuitCB(void)
{
	//Spawn car and police peds inside
	for(u32 pedVehId = 0; pedVehId < CVehicleFactory::vehicleNames.GetCount(); pedVehId++)
	{
		if ( strstr(CVehicleFactory::vehicleNames[pedVehId],"police") )
		{
			//Set car type
			CVehicleFactory::currVehicleNameSelection = pedVehId;

			CVehicleFactory::CarNumChangeCB();

			//Spawn car
			CVehicleFactory::CreateCar();

			//Set ped type
			for(u32 pedPicId = 0; pedPicId < CPedVariationDebug::GetNumPedNames(); pedPicId++)
			{
				if ( strstr(CPedVariationDebug::pedNames[pedPicId],"S_M_Y_Cop_01") )
				{
					CPedVariationDebug::SetCrowdPedSelection(0,pedPicId);
					CPedVariationDebug::SetCrowdPedSelection(1,pedPicId);
					CPedVariationDebug::SetCrowdPedSelection(2,pedPicId);
					CPedVariationDebug::SetCrowdPedSelection(3,pedPicId);
					break;
				}
			}

			bkBank* pBank = BANKMGR.FindBank("Peds");
			bkWidget* pWidget = BANKMGR.FindWidget("Peds/Ped Type");
			if(pBank && pWidget)
			{
				//Spawn peds in car
				CPedVariationDebug::populateCarCB(0,2);
			}
			else
			{
				Assertf(pBank && pWidget, "Please create the ped widget to allow cops to spawn in vehicle");
			}

			break;
		}
	}
}

void CPedDebugVisualiserMenu::CreatePolicePursuitCB(void)
{
	//Allow crimes to ignore crime flags
	CRandomEventManager::SetForceCrime();

	//Create a cop
	CPedVariationDebug::SetDistToCreateCrowdFromCamera(10.0f);
	CPedVariationDebug::SetCrowdSize(1);
	CPedVariationDebug::SetCreateExactGroupSize(true);
	CPedVariationDebug::SetWanderAfterCreate(true);
	CPedVariationDebug::SetCrowdUniformDistribution(1.0f);


	Vector3 srcPos = camInterface::GetPos();
	Vector3 destPos = srcPos;
	Vector3 viewVector = camInterface::GetFront();

	static dev_float DIST_IN_FRONT = 7.0f;
	destPos += viewVector*DIST_IN_FRONT;		// create at position DIST_IN_FRONT metres in front of current camera position

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(srcPos, destPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	probeDesc.SetExcludeEntity(CGameWorld::FindLocalPlayer());
	probeDesc.SetContext(WorldProbe::LOS_Unspecified);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		destPos = probeResults[0].GetHitPosition();
		static dev_float MOVE_BACK_DIST = 1.f;
		destPos -= viewVector * MOVE_BACK_DIST;
	}

	// try to avoid creating the ped on top of the player
	const Vector3 vLocalPlayerPosition = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
	if((vLocalPlayerPosition - destPos).XYMag2() < 0.8f*0.8f)
	{
		if((vLocalPlayerPosition - camInterface::GetPos()).Mag2() < square(3.0f))
			destPos -= viewVector;
	}

	if (destPos.z <= -100.0f)
	{
		destPos.z = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, destPos.x, destPos.y);
	}
	destPos.z += 1.0f;

	CPedVariationDebug::SetCrowdSpawnLocation(destPos);

	for(u32 pedPicId = 0; pedPicId < CPedVariationDebug::GetNumPedNames(); pedPicId++)
	{
		if ( strstr(CPedVariationDebug::pedNames[pedPicId],"S_M_Y_Cop_01") )
		{
			CPedVariationDebug::SetCrowdPedSelection(0,pedPicId);
			break;
		}
	}

	bkBank* pBank = BANKMGR.FindBank("Peds");
	bkWidget* pWidget = BANKMGR.FindWidget("Peds/Ped Type");
	if(pBank && pWidget)
	{
		createCrowdCB();
	}
	else
	{
		Assertf(pBank && pWidget, "Please create the ped widget to allow cops to spawn");
	}

}


#define MAX_TASK_NAMES 100

CTaskTypes::eTaskType iTaskTypesWithTestHarness[] =
{
	CTaskTypes::TASK_CLONED_FSM_TEST_HARNESS,
	CTaskTypes::TASK_COMBAT,
	CTaskTypes::TASK_PARACHUTE,
	CTaskTypes::TASK_STAND_GUARD_FSM,
	CTaskTypes::TASK_PATROL,
	CTaskTypes::TASK_STAY_IN_COVER,
	CTaskTypes::TASK_CROUCH_TOGGLE,
	CTaskTypes::TASK_SAY_AUDIO,
	CTaskTypes::TASK_SMART_FLEE,
	CTaskTypes::TASK_ESCAPE_BLAST,
	CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON,
	CTaskTypes::TASK_VEHICLE_PROJECTILE,
	CTaskTypes::TASK_VEHICLE_GUN,
	CTaskTypes::TASK_SMASH_CAR_WINDOW,
	CTaskTypes::TASK_AMBIENT_CLIPS,
	CTaskTypes::TASK_FALL_AND_GET_UP,
	CTaskTypes::TASK_SIT_DOWN,
	CTaskTypes::TASK_INVESTIGATE,
	CTaskTypes::TASK_NM_POSE,
	CTaskTypes::TASK_NM_SHOT,
	CTaskTypes::TASK_NM_HIGH_FALL,
	CTaskTypes::TASK_RAGE_RAGDOLL,
	CTaskTypes::TASK_MOTION_IN_AUTOMOBILE,
	CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT,
	CTaskTypes::TASK_COMBAT_FLANK,
	CTaskTypes::TASK_COMBAT_SEEK_COVER,
	CTaskTypes::TASK_COMBAT_CHARGE,
	CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA,
	CTaskTypes::TASK_SEPARATE,
	CTaskTypes::TASK_MOTION_TENNIS,
	CTaskTypes::TASK_MOVE_SCRIPTED,
	CTaskTypes::TASK_HANDS_UP,
	CTaskTypes::TASK_IN_COVER,
	CTaskTypes::TASK_AIM_GUN_ON_FOOT,
	CTaskTypes::TASK_AIM_GUN_SCRIPTED,
	CTaskTypes::TASK_MOVE_GO_TO_POINT,
	CTaskTypes::TASK_COWER,
	CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE,
	CTaskTypes::TASK_IN_VEHICLE_BASIC,
	CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY,
	CTaskTypes::TASK_COVER,
	CTaskTypes::TASK_CONTROL_VEHICLE,
	CTaskTypes::TASK_SYNCHRONIZED_SCENE,
	CTaskTypes::TASK_RELOAD_GUN,
	CTaskTypes::TASK_THREAT_RESPONSE,
	CTaskTypes::TASK_REVIVE,
    CTaskTypes::TASK_HELI_PASSENGER_RAPPEL,
	CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER,
	CTaskTypes::TASK_REACT_AND_FLEE,
	CTaskTypes::TASK_WRITHE,
	CTaskTypes::TASK_TO_HURT_TRANSIT,
	CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS,
	CTaskTypes::TASK_REACT_TO_BUDDY_SHOT,
	CTaskTypes::TASK_REACT_TO_EXPLOSION,
	CTaskTypes::TASK_REACT_TO_IMMINENT_EXPLOSION,
	CTaskTypes::TASK_LEAVE_ANY_CAR,
	CTaskTypes::TASK_JETPACK,
	// Terminating entry
	CTaskTypes::TASK_NONE
};


void CPedDebugVisualiserMenu::WarpToStartCB()
{
	if( CDebugScene::FocusEntities_Get(0) )
	{
		if( CDebugScene::FocusEntities_Get(0)->GetIsPhysical() )
		{
			static_cast<CPhysical*>(CDebugScene::FocusEntities_Get(0))->SetVelocity(Vector3(0.0f, 0.0f, 0.0f));
		}

		CDebugScene::FocusEntities_Get(0)->Teleport(m_StartMat.d, 0.0f, false, true, false, true, false, true);
		CDebugScene::FocusEntities_Get(0)->SetMatrix(m_StartMat, true, true, true);
	}
}

static s32 iSelectedTestHarness = 0;

void CPedDebugVisualiserMenu::PrintTestHarnessParamsCB()
{
	char szOutput[1024];
	if( m_bUseStartPos )
	{
		sprintf(szOutput, "-TestHarnessStartcoords=%.2f,%.2f,%.2f\n", m_StartMat.d.x, m_StartMat.d.y, m_StartMat.d.z);
		Printf("%s", szOutput);
	}
	sprintf(szOutput, "-TestHarnessTargetcoords=%.2f,%.2f,%.2f\n", m_TargetPos.x, m_TargetPos.y, m_TargetPos.z);
	Printf("%s", szOutput);
	sprintf(szOutput, "-TestHarnessTask=%d\n", iSelectedTestHarness);
	Printf("%s", szOutput);
	sprintf(szOutput, "-TestHarnessMissionIndex=%d\n", m_Mission);
	Printf("%s", szOutput);
	sprintf(szOutput, "-TestHarnessCruiseSpeed=%d\n", (s32) m_CruiseSpeed);
	Printf("%s", szOutput);
}


void CPedDebugVisualiserMenu::TestHarnessCB()
{
	if( CPedDebugVisualiserMenu::m_bUseStartPos )
	{
		WarpToStartCB();
	}

	CPed* pFocusPed = NULL;
	if( CDebugScene::FocusEntities_Get(0) &&
		CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
	{
		pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));
	}

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	int iEventPriority = E_PRIORITY_GIVE_PED_TASK;

	CTask* pTaskToGiveToFocusPed = NULL;
	switch(iTaskTypesWithTestHarness[iSelectedTestHarness])
	{
	case CTaskTypes::TASK_COMBAT:
		if( pLocalPlayer )
		{
			pTaskToGiveToFocusPed = rage_new CTaskCombat(pLocalPlayer);
		}
		break;

	case CTaskTypes::TASK_PARACHUTE:
		if(pFocusPed)
		{
			pTaskToGiveToFocusPed = rage_new CTaskParachute(CTaskParachute::PF_HasTeleported | CTaskParachute::PF_GiveParachute);

			Vector3 vWarpPos = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition() );
			vWarpPos.z += 400.0f;
			pFocusPed->Teleport(vWarpPos, pFocusPed->GetCurrentHeading() );

			iEventPriority = E_PRIORITY_IN_AIR + 1;
		}
		break;
	case CTaskTypes::TASK_STAND_GUARD_FSM:
		if( pFocusPed )
		{
			pTaskToGiveToFocusPed = rage_new CTaskStandGuardFSM(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), pFocusPed->GetTransform().GetHeading());
		}
		break;
	case CTaskTypes::TASK_PATROL:
		if( pFocusPed )
		{
			static float sfDebugSenseRange = 30.0f;
			static float sfRecogniseTargetDistance = 10.0f;
			if( pFocusPed )
			{
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetSeeingRange(sfDebugSenseRange);
				pFocusPed->GetPedIntelligence()->GetPedPerception().SetIdentificationRange(sfRecogniseTargetDistance);
			}
			CTaskPatrol::AlertStatus alertStatus = CPatrolRoutes::ms_bToggleGuardsAlert ? CTaskPatrol::AS_alert : CTaskPatrol::AS_casual;
			pTaskToGiveToFocusPed = rage_new CTaskPatrol(0, alertStatus);
		}
		break;

	case CTaskTypes::TASK_STAY_IN_COVER:
		if( pFocusPed )
		{
			if( !pFocusPed->GetPedIntelligence()->GetPrimaryDefensiveArea()->IsActive() )
			{
				pFocusPed->GetPedIntelligence()->GetPrimaryDefensiveArea()->SetAsSphere(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), 5.0f);
			}
			if( pFocusPed->GetPedIntelligence()->GetPrimaryDefensiveArea()->IsActive() )
			{
				pFocusPed->GetPedIntelligence()->GetPrimaryDefensiveArea()->SetDefaultCoverFromPosition(camInterface::GetFront());
				pTaskToGiveToFocusPed = rage_new CTaskStayInCover();
			}
		}
		break;

	case CTaskTypes::TASK_CROUCH_TOGGLE:
		if(pFocusPed)
		{
			pTaskToGiveToFocusPed = rage_new CTaskCrouchToggle();
		}
		break;

	case CTaskTypes::TASK_SAY_AUDIO:
		// Make a non-player ped speak to the player.
		if(pFocusPed)
		{
			CPed* pPlayerPed = FindPlayerPed();
			Assertf(pPlayerPed, "Pointer to Player ped is NULL!");
			// TODO RA: Change the audio file below to an actual resource.
			pTaskToGiveToFocusPed = rage_new CTaskSayAudio("APPREHEND_PLAYER_STATE", 0.5f, 1.0f, pPlayerPed);
		}
		break;

	case CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON:
		if(pFocusPed)
		{
			static CTaskVehicleMountedWeapon::eTaskMode testMode = CTaskVehicleMountedWeapon::Mode_Player;
			pTaskToGiveToFocusPed = rage_new CTaskVehicleMountedWeapon(testMode);
			static_cast<CTaskVehicleMountedWeapon*>(pTaskToGiveToFocusPed)->SetTarget(CGameWorld::FindLocalPlayer());
		}
		break;
	case CTaskTypes::TASK_SMART_FLEE:
		if (pFocusPed && !pFocusPed->IsPlayer() && pLocalPlayer)
		{
			pTaskToGiveToFocusPed = rage_new CTaskSmartFlee(CAITarget(pLocalPlayer));
		}
		break;
	case CTaskTypes::TASK_ESCAPE_BLAST:
		if (pFocusPed && !pFocusPed->IsPlayer() && pLocalPlayer)
		{
			if (pFocusPed->GetPedIntelligence()->GetClosestVehicleInRange())
			{
				pTaskToGiveToFocusPed = rage_new CTaskEscapeBlast(pFocusPed->GetPedIntelligence()->GetClosestVehicleInRange(), VEC3V_TO_VECTOR3(pFocusPed->GetPedIntelligence()->GetClosestVehicleInRange()->GetTransform().GetPosition()), 15.0f, false);
			}
		}
		break;
	case CTaskTypes::TASK_VEHICLE_PROJECTILE:
 		if(pFocusPed && pFocusPed->GetMyVehicle())
 		{
			static dev_bool bTestPlayer = true;
			if(bTestPlayer)
				pTaskToGiveToFocusPed = rage_new CTaskVehicleProjectile(CTaskVehicleProjectile::Mode_Player);
			else
				pTaskToGiveToFocusPed = rage_new CTaskVehicleProjectile(CTaskVehicleProjectile::Mode_Fire);
 		}
		break;
	case CTaskTypes::TASK_VEHICLE_GUN:
		if(pFocusPed && pFocusPed->GetMyVehicle())
		{
			static CTaskVehicleGun::eTaskMode eTestMode = pFocusPed->IsLocalPlayer() ? CTaskVehicleGun::Mode_Player : CTaskVehicleGun::Mode_Fire;
			static u32 uTestHash = ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE_DRIVEBY", 0xD31265F2);
			CAITarget target(CDebugScene::FocusEntities_Get(1));	// Point at the 2nd debug entity
			static dev_float fShootRateModifier = 1.0f;
			pTaskToGiveToFocusPed = rage_new CTaskVehicleGun(eTestMode,uTestHash,&target,fShootRateModifier);
		}
		break;
	case CTaskTypes::TASK_SMASH_CAR_WINDOW:
		if(pFocusPed && pFocusPed->GetMyVehicle())
		{
			static dev_bool bSmashWindscreen = false;
			pTaskToGiveToFocusPed = rage_new CTaskSmashCarWindow(bSmashWindscreen);
		}
		break;
	case CTaskTypes::TASK_AMBIENT_CLIPS:
		if(pFocusPed)
		{
			s32 type = CScenarioManager::GetScenarioTypeFromName("WORLD_HUMAN_SEAT_WALL");
			pTaskToGiveToFocusPed = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PlayBaseClip, CScenarioManager::GetConditionalAnimsGroupForScenario(type) );
		}
		break;
	case CTaskTypes::TASK_FALL_AND_GET_UP:
		if (pFocusPed)
		{
			pTaskToGiveToFocusPed = rage_new CTaskFallAndGetUp(1,5.0f);
		}
		break;
	case CTaskTypes::TASK_INVESTIGATE:
		if (pFocusPed && !pFocusPed->IsLocalPlayer())
		{
			pTaskToGiveToFocusPed = rage_new CTaskInvestigate(VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()));
		}
		break;
	case CTaskTypes::TASK_NM_POSE:
		if(pFocusPed)
		{
			CTaskNMPose* pPoseTask = rage_new CTaskNMPose(65535,65535,MOVE_PED_BASIC_LOCOMOTION,CLIP_MOVE_SPRINT);
			pTaskToGiveToFocusPed = rage_new CTaskNMControl(65535,65535,pPoseTask,CTaskNMControl::ALL_FLAGS_CLEAR);
		}
		break;
	case CTaskTypes::TASK_NM_SHOT:
		if(pFocusPed)
		{
			CTaskNMShot* pShotTask = rage_new CTaskNMShot(pFocusPed,65535,65535,NULL,WEAPONTYPE_PISTOL,0, VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetB()), -VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetB()));
			pTaskToGiveToFocusPed = rage_new CTaskNMControl(65535,65535,pShotTask,CTaskNMControl::ALL_FLAGS_CLEAR);
		}
		break;
	case CTaskTypes::TASK_NM_HIGH_FALL:
		if(pFocusPed)
		{
			CTaskNMHighFall* pHighFall = rage_new CTaskNMHighFall(65535);
			pTaskToGiveToFocusPed = rage_new CTaskNMControl(65535,65535,pHighFall,CTaskNMControl::ALL_FLAGS_CLEAR);
		}
		break;
	case CTaskTypes::TASK_RAGE_RAGDOLL:
		if(pFocusPed)
		{
			pTaskToGiveToFocusPed = rage_new CTaskRageRagdoll();
		}
		break;
	case CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT:
		if(pFocusPed && !pFocusPed->IsLocalPlayer() && pLocalPlayer)
		{
			pTaskToGiveToFocusPed = rage_new CTaskReactToGunAimedAt(pLocalPlayer);
		}
		break;
	case CTaskTypes::TASK_COMBAT_FLANK:
		if(pFocusPed && !pFocusPed->IsLocalPlayer() && pLocalPlayer)
		{
			pTaskToGiveToFocusPed = rage_new CTaskCombatFlank(pLocalPlayer);
		}
		break;
	case CTaskTypes::TASK_COMBAT_SEEK_COVER:
		if(pFocusPed && !pFocusPed->IsLocalPlayer() && pLocalPlayer)
		{
			pTaskToGiveToFocusPed = rage_new CTaskCombatSeekCover(pLocalPlayer, VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), true);
		}
		break;
	case CTaskTypes::TASK_COMBAT_CHARGE:
		if(pFocusPed && !pFocusPed->IsLocalPlayer() && pLocalPlayer)
		{
			pTaskToGiveToFocusPed = rage_new CTaskCombatChargeSubtask(pLocalPlayer);
		}
		break;
	case CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA:
		if(pFocusPed)
		{
			pTaskToGiveToFocusPed = rage_new CTaskCombatClosestTargetInArea(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), 10.0f, true);
		}
		break;
	case CTaskTypes::TASK_SEPARATE:
		if(pFocusPed && !pFocusPed->IsLocalPlayer() && pLocalPlayer)
		{
			const Vector3 vecLocalPlayerPos = VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition());
			CTaskCombatAdditionalTask* pAdditionalTask = rage_new CTaskCombatAdditionalTask( CTaskCombatAdditionalTask::RT_Default, pLocalPlayer, vecLocalPlayerPos);
			Vector3 pos = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());
			pTaskToGiveToFocusPed = rage_new CTaskSeparate(&pos, vecLocalPlayerPos, pLocalPlayer, pAdditionalTask);
		}
		break;

	case CTaskTypes::TASK_MOTION_TENNIS:
		if(pFocusPed)
		{
			CTaskMotionPed* pCurrentMotionTask = static_cast<CTaskMotionPed*>(pFocusPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_MOTION, CTaskTypes::TASK_MOTION_PED));
			if(pCurrentMotionTask)
			{
				pCurrentMotionTask->SetCleanupMotionTaskNetworkOnQuit(false);
			}

			CTaskMotionTennis* pTask = rage_new CTaskMotionTennis(!pFocusPed->IsMale());
			pFocusPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_MOTION, pTask, PED_TASK_MOTION_DEFAULT, true);
		}
		break;
	case CTaskTypes::TASK_MOVE_SCRIPTED:
		if(pFocusPed)
		{
			pTaskToGiveToFocusPed = rage_new CTaskMoVEScripted(CClipNetworkMoveInfo::ms_NetworkMgBenchPress, 0);
		}
		break;
	case CTaskTypes::TASK_HANDS_UP:
		if(pFocusPed)
		{
			pTaskToGiveToFocusPed = rage_new CTaskHandsUp(-1);
		}
		break;
	case CTaskTypes::TASK_IN_COVER:
		if(pFocusPed)
		{
			CCoverPoint* pCoverPoint = CCover::FindClosestCoverPoint(pFocusPed, VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()));
			if (pCoverPoint)
			{
				pCoverPoint->ReserveCoverPointForPed(pFocusPed);
				pFocusPed->ReleaseCoverPoint();
				pFocusPed->SetCoverPoint(pCoverPoint);
				//pTaskToGiveToFocusPed = rage_new CTaskUseCoverNew(CWeaponTarget(pLocalPlayer));
			}
		}
		break;
	case CTaskTypes::TASK_AIM_GUN_ON_FOOT:
		if(pFocusPed)
		{
			Vector3 vPosition1(Vector3::ZeroType);
			static float sfAimTime = 5.0f;

			if (CPhysics::GetMeasuringToolPos( 0, vPosition1 ))
			{
				pTaskToGiveToFocusPed = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(vPosition1), sfAimTime);
			}
		}
		break;
	case CTaskTypes::TASK_AIM_GUN_SCRIPTED:
		if(pLocalPlayer)
		{
			pFocusPed = pLocalPlayer;
			static bool sbGunTask = true;
			if (sbGunTask)
			{
				CTaskGun* pGunTask = rage_new CTaskGun(CWeaponController::WCT_Player, CTaskTypes::TASK_AIM_GUN_SCRIPTED, CWeaponTarget(Vector3(Vector3::ZeroType)));
				pGunTask->GetGunFlags().SetFlag(GF_DisableBlockingClip);
				pGunTask->GetGunFlags().SetFlag(GF_ForceAimState);
				pTaskToGiveToFocusPed = pGunTask;
			}
			else
				pTaskToGiveToFocusPed = rage_new CTaskAimGunScripted(CWeaponController::WCT_Player, 0, CWeaponTarget(Vector3(Vector3::ZeroType)));
		}
		break;
	case CTaskTypes::TASK_MOVE_GO_TO_POINT:
		{
			Vector3 vPosition1, vPosition2;
			if (CPhysics::GetMeasuringToolPos( 0, vPosition1 ) && CPhysics::GetMeasuringToolPos( 1, vPosition2 ))
			{
				if (!pFocusPed)
					pFocusPed = pLocalPlayer;

				pTaskToGiveToFocusPed = rage_new CTaskComplexControlMovement(rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, vPosition2));
			}
		}
		break;
	case CTaskTypes::TASK_COWER:
		{
			pFocusPed = pLocalPlayer;
			pTaskToGiveToFocusPed = rage_new CTaskCower(1000);
		}
		break;
	case CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE:
		{
			pFocusPed = pLocalPlayer;

			if (pFocusPed->GetMyVehicle())
			{
				// Actually shuffle over to the seat
				pTaskToGiveToFocusPed = rage_new CTaskInVehicleSeatShuffle(pFocusPed->GetMyVehicle(),NULL);
			}

		}
		break;
	case CTaskTypes::TASK_IN_VEHICLE_BASIC:
		{
			pFocusPed = pLocalPlayer;

			if (pFocusPed->GetMyVehicle())
			{
				// Actually shuffle over to the seat
				pTaskToGiveToFocusPed = rage_new CTaskInVehicleBasic(pFocusPed->GetMyVehicle());
			}
		}
		break;
	case CTaskTypes::TASK_COMPLEX_TURN_TO_FACE_ENTITY:
		{
			if (CVehicleDebug::ms_pLastCreatedPed)
			{
				pFocusPed = CVehicleDebug::ms_pLastCreatedPed;
			}

			if (pFocusPed)
			{
				Vector3 vPosition1(Vector3::ZeroType);
				if (CPhysics::GetMeasuringToolPos( 0, vPosition1 ))
				{
					CTaskSequenceList* pTaskList = rage_new CTaskSequenceList();
					TUNE_GROUP_BOOL(HEADING_BUG, JUST_TURN, false);
					if (!JUST_TURN)
					{
						TUNE_GROUP_BOOL(HEADING_BUG, RUN, false);
						float moveBlendRatio = MOVEBLENDRATIO_WALK;
						if (RUN)
							moveBlendRatio = MOVEBLENDRATIO_RUN;
						TUNE_GROUP_FLOAT(HEADING_BUG, TARGET_RADIUS, 1.0f, 0.0f, 5.0f, 0.01f);
						CTaskMoveFollowNavMesh* pTaskFollowNavMesh = rage_new CTaskMoveFollowNavMesh(moveBlendRatio,
							vPosition1,
							TARGET_RADIUS,
							CTaskMoveFollowNavMesh::ms_fSlowDownDistance,
							-1,
							true,
							false,
							NULL,
							TARGET_RADIUS);

						pTaskFollowNavMesh->SetIsScriptedRoute(true);
						pTaskList->AddTask(rage_new CTaskComplexControlMovement( pTaskFollowNavMesh, NULL, CTaskComplexControlMovement::TerminateOnMovementTask, -1.0f ));
					}
					pTaskList->AddTask(rage_new CTaskTurnToFaceEntityOrCoord(pLocalPlayer));

					pTaskToGiveToFocusPed = rage_new CTaskUseSequence(*pTaskList);
				}
			}
		}
		break;
	case CTaskTypes::TASK_COVER:
		{
			if (!pFocusPed)
				pFocusPed = pLocalPlayer;

			Vector3 vPedPosition = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());

			CCoverPoint* pCoverPoint = CCover::FindClosestCoverPoint(pFocusPed, vPedPosition);
			if (pCoverPoint)
			{
				Vector3 vCoverDirection = VEC3V_TO_VECTOR3(pCoverPoint->GetCoverDirectionVector());
				static dev_float COVER_THREAT_DIRECTION_MULTIPLIER = 3.0f;
				vCoverDirection *= COVER_THREAT_DIRECTION_MULTIPLIER;
				Vector3 vThreatPosition = vPedPosition + vCoverDirection;
#if DEBUG_DRAW
				CTask::ms_debugDraw.AddArrow(RCC_VEC3V(vPedPosition), RCC_VEC3V(vThreatPosition), 0.25f, Color_red, 1000);
#endif
				s32 iFlags = CTaskCover::CF_PutPedDirectlyIntoCover;
				TUNE_GROUP_BOOL(COVER_TUNE, FORCE_INITIAL_FACING_DIRECTION, false);
				if (FORCE_INITIAL_FACING_DIRECTION)
				{
					iFlags |= CTaskCover::CF_SpecifyInitialHeading;
					TUNE_GROUP_BOOL(COVER_TUNE, FACE_LEFT, true);
					if (FACE_LEFT)
					{
						iFlags |= CTaskCover::CF_FacingLeft;
					}
				}
				pTaskToGiveToFocusPed = rage_new CTaskCover(CAITarget(vThreatPosition), iFlags);
				CTaskCover* pCoverTask = static_cast<CTaskCover*>(pTaskToGiveToFocusPed);


				TUNE_GROUP_FLOAT(COVER_TUNE, DEBUG_BLEND_IN_DURATION, INSTANT_BLEND_DURATION, INSTANT_BLEND_DURATION, 2.0f, 0.01f);
				pCoverTask->SetBlendInDuration(DEBUG_BLEND_IN_DURATION);
				pCoverTask->SetSearchFlags(CTaskCover::CF_FindClosestPointAtStart);
				pCoverTask->SetSearchPosition(vPedPosition);
			}
		}
		break;
	case CTaskTypes::TASK_CONTROL_VEHICLE:
		{
			CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
			if (CDebugScene::FocusEntities_Get(0))
			{
				if( pFocusEntity->GetIsTypeVehicle() )
				{
					CVehicle* pFocusVehicle = static_cast<CVehicle*>(pFocusEntity);
					if(pFocusVehicle->GetDriver())
					{
						pFocusPed = pFocusVehicle->GetDriver();
					}
				}
			}

			if( pFocusPed == NULL )
				pFocusPed = pLocalPlayer;

			if( pFocusPed && pFocusPed->GetMyVehicle() && pFocusPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
			{
				CPhysical* pTargetPhys = (m_TargetEnt && m_TargetEnt->GetIsPhysical()) ? static_cast<CPhysical*>(m_TargetEnt.Get()) : NULL;

				s32 iDrivingFlags = 0;
				for( s32 i = 0; i < 32; i++ )
				{
					if( m_abDrivingFlags[i] )
					{
						iDrivingFlags |= 1<<i;
					}
				}

				aiTask *pVehicleTask = NULL;
				CVehicle* pVehicle = pFocusPed->GetMyVehicle();
				if( pVehicle && pVehicle->InheritsFromHeli() )
				{
					pVehicleTask = CVehicleIntelligence::GetHeliTaskFromMissionIdentifier(pFocusPed->GetMyVehicle(), m_Mission, pTargetPhys, &m_TargetPos, m_TargetReachedDistance, m_CruiseSpeed, m_fHeliOrientation, m_iFlightHeight, m_iMinHeightAboveTerrain, -1.0f);
				}
				else if (pVehicle && pVehicle->InheritsFromPlane())
				{
					pVehicleTask = CVehicleIntelligence::GetPlaneTaskFromMissionIdentifier(pFocusPed->GetMyVehicle(), m_Mission, pTargetPhys, &m_TargetPos, m_TargetReachedDistance, m_CruiseSpeed, m_fHeliOrientation, m_iFlightHeight, m_iMinHeightAboveTerrain, false);
				}
				else if (pVehicle && (pVehicle->InheritsFromSubmarine() || (pVehicle->InheritsFromSubmarineCar() && pVehicle->IsInSubmarineMode())))
				{
					pVehicleTask = CVehicleIntelligence::GetSubTaskFromMissionIdentifier(pFocusPed->GetMyVehicle(), m_Mission, pTargetPhys, &m_TargetPos, m_TargetReachedDistance, m_CruiseSpeed, m_fHeliOrientation, m_iMinHeightAboveTerrain, -1.0f);
				}
				else
				{
					pVehicleTask = CVehicleIntelligence::GetTaskFromMissionIdentifier(pFocusPed->GetMyVehicle(), m_Mission, pTargetPhys, &m_TargetPos, iDrivingFlags, m_TargetReachedDistance, m_StraightLineDistance, m_CruiseSpeed, false);
				}

				pTaskToGiveToFocusPed = rage_new CTaskControlVehicle(pFocusPed->GetMyVehicle(), pVehicleTask);
			}
		}
		break;
	case CTaskTypes::TASK_SYNCHRONIZED_SCENE:
		{
			if( pFocusPed )
			{
				Vector3 pos(pFocusPed->GetPreviousPosition());
				Vector3 eulers(0.0f, 0.0f, 0.0f);
				eulers*=DtoR;
				Quaternion rot;
				rot.FromEulers(eulers);

				fwSyncedSceneId scene = fwAnimDirectorComponentSyncedScene::StartSynchronizedScene();

				if(fwAnimDirectorComponentSyncedScene::IsValidSceneId(scene))
				{
					fwAnimDirectorComponentSyncedScene::SetSyncedSceneOrigin(scene, pos, rot);

					pTaskToGiveToFocusPed = rage_new CTaskSynchronizedScene( scene, atPartialStringHash("high_l_pistol"), "cover_dive");
					Assert(pTaskToGiveToFocusPed);
				}
			}
		}
		break;
	case CTaskTypes::TASK_RELOAD_GUN:
		{
			if( pFocusPed && pFocusPed->GetWeaponManager() && pFocusPed->GetWeaponManager()->GetEquippedWeapon() )
			{
				pTaskToGiveToFocusPed = rage_new CTaskReloadGun(CWeaponController::WCT_Reload);
				Assert(pTaskToGiveToFocusPed);
			}
		}
		break;
	case CTaskTypes::TASK_THREAT_RESPONSE:
		{
			if(pFocusPed && pLocalPlayer)
			{
				CTaskThreatResponse* pTaskThreatResponse = rage_new CTaskThreatResponse( pLocalPlayer );
				pTaskThreatResponse->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
				pTaskToGiveToFocusPed = pTaskThreatResponse;
				Assert(pTaskToGiveToFocusPed);
			}
		}
		break;
	case CTaskTypes::TASK_REVIVE:
		{
			if((pFocusPed && pLocalPlayer) && (pFocusPed != pLocalPlayer))
			{
				// Get the ambulance we need to have created already....
				CVehicle* ambulance = NULL;
				CVehicle::Pool* vehiclePool = CVehicle::GetPool();

				for(s32 i = 0; i < vehiclePool->GetSize(); i++)
				{
					CVehicle *veh = vehiclePool->GetSlot(i);
					if(veh)
					{
						if(veh->GetModelIndex() == MI_CAR_AMBULANCE)
						{
							ambulance = veh;
							break;
						}
					}
				}

				if(ambulance)
				{
					pTaskToGiveToFocusPed = rage_new CTaskRevive( pLocalPlayer, ambulance );
					Assert(pTaskToGiveToFocusPed);
				}
			}
		}
		break;
    case CTaskTypes::TASK_HELI_PASSENGER_RAPPEL:
        {
            if(pFocusPed &&
               pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) &&
               pFocusPed->GetMyVehicle() && pFocusPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_HELI)
            {
                pTaskToGiveToFocusPed = rage_new CTaskHeliPassengerRappel(10.0f);
                Assert(pTaskToGiveToFocusPed);
            }
        }
        break;
	case CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER:
		{
			if(pFocusPed)
			{
				Vector3 pos;
				int ladderIndex = -1;
				CEntity* pTargetLadder = CTaskGoToAndClimbLadder::ScanForLadderToClimb(*pFocusPed, ladderIndex, pos);

				if((pTargetLadder) && (ladderIndex != -1))
				{
					pTaskToGiveToFocusPed = rage_new CTaskGoToAndClimbLadder(pTargetLadder, ladderIndex, CTaskGoToAndClimbLadder::AutoClimbNormal);
					Assert(pTaskToGiveToFocusPed);
				}
			}
		}
		break;
	case CTaskTypes::TASK_REACT_AND_FLEE:
		{
			if((pFocusPed) && (pFocusPed != pLocalPlayer))
			{
				pTaskToGiveToFocusPed = rage_new CTaskReactAndFlee(CAITarget(pLocalPlayer), CLIP_SET_REACTION_GUNFIRE_RUNS, CLIP_SET_REACTION_GUNFIRE_RUNS);
				Assert(pTaskToGiveToFocusPed);
			}
		}
		break;

	case CTaskTypes::TASK_WRITHE:
		if (pFocusPed)
		{
			CEventWrithe event(CWeaponTarget(pLocalPlayer), false);
			if (!pFocusPed->GetPedIntelligence()->HasEventOfType(&event))
				pFocusPed->GetPedIntelligence()->AddEvent(event);
		}
		break;

	case CTaskTypes::TASK_TO_HURT_TRANSIT:
		{
			if (pFocusPed)
			{
				CEventGivePedTask event(PED_TASK_PRIORITY_PHYSICAL_RESPONSE, rage_new CTaskToHurtTransit(pLocalPlayer), false);
				pFocusPed->GetPedIntelligence()->AddEvent(event);
			}
		}
		break;

	case CTaskTypes::TASK_GO_TO_POINT_ANY_MEANS:
		if((pFocusPed) && (pFocusPed != pLocalPlayer))
		{
			Vector3 vPosition1;
			if (CPhysics::GetMeasuringToolPos( 0, vPosition1 ))
			{
				if (pFocusPed->IsNetworkClone())
				{
					CTask* pTask=rage_new CTaskGoToPointAnyMeans(MOVEBLENDRATIO_RUN,vPosition1);
					CScriptPeds::GivePedScriptedTask( CTheScripts::GetGUIDFromEntity(*pFocusPed), pTask, SCRIPT_TASK_GO_TO_COORD_ANY_MEANS, "TASK_GO_TO_COORD_ANY_MEANS");
				}
				else
				{
					pTaskToGiveToFocusPed = rage_new CTaskGoToPointAnyMeans(MOVEBLENDRATIO_RUN, vPosition1);
					Assert(pTaskToGiveToFocusPed);
				}
			}
		}
		break;
	case CTaskTypes::TASK_REACT_TO_BUDDY_SHOT:
		{
			if((pFocusPed) && (pFocusPed != pLocalPlayer))
			{
				pTaskToGiveToFocusPed = rage_new CTaskReactToBuddyShot(CAITarget(pLocalPlayer), CAITarget(pLocalPlayer));
				Assert(pTaskToGiveToFocusPed);
			}
		}
		break;
	case CTaskTypes::TASK_REACT_TO_EXPLOSION:
		{
			if((pLocalPlayer) &&  (pFocusPed) && (pFocusPed != pLocalPlayer))
			{
				Vec3V localPos = pLocalPlayer->GetTransform().GetPosition();
				pTaskToGiveToFocusPed = rage_new CTaskReactToExplosion(localPos, pLocalPlayer, 3.0f);
			}
		}
		break;
	case CTaskTypes::TASK_REACT_TO_IMMINENT_EXPLOSION:
		{
			if((pLocalPlayer) &&  (pFocusPed) && (pFocusPed != pLocalPlayer))
			{
				pTaskToGiveToFocusPed = rage_new CTaskReactToImminentExplosion(CAITarget(pLocalPlayer), 3.0f, 0);
				Assert(pTaskToGiveToFocusPed);
			}
		}
		break;
	case CTaskTypes::TASK_LEAVE_ANY_CAR:
		{
			if(pFocusPed)
			{
				VehicleEnterExitFlags vehicleFlags;
				if (!vehicleFlags.BitSet().IsSet(CVehicleEnterExitFlags::DontDefaultWarpIfDoorBlocked))
				{
					vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
				}
				pTaskToGiveToFocusPed=rage_new CTaskLeaveAnyCar(0, vehicleFlags);
				Assert(pTaskToGiveToFocusPed);
			}
		}
		break;
	case CTaskTypes::TASK_JETPACK:
		if(pFocusPed)
		{
			//Ensure the inventory is valid.
			CPedInventory* pPedInventory = pFocusPed->GetInventory();
			if(pPedInventory)
			{
				//! Give ped a jetpack.
				pPedInventory->AddWeapon(GADGETTYPE_JETPACK);
			}
		}
		break;
	default:
		break;
	}

	if( pTaskToGiveToFocusPed )
	{
		if( pFocusPed )
		{
			CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGiveToFocusPed,false,iEventPriority);
			pFocusPed->GetPedIntelligence()->AddEvent(event);
		}
		else
		{
			delete pTaskToGiveToFocusPed;
			pTaskToGiveToFocusPed = NULL;
		}
	}
}

// Add all the widgets
void CPedDebugVisualiserMenu::Initialise()
{
#if __DEV
	if(PARAM_disableNewLocomotionTask.Get())
	{
		ms_bUseNewLocomotionTask = false;
	}
#endif // __DEV

	int taskIndex;
	if(PARAM_TestHarnessTask.Get(taskIndex))
	{
		iSelectedTestHarness = taskIndex;
	}
	int missionIndex;
	if(PARAM_TestHarnessMissionIndex.Get(missionIndex))
	{
		m_Mission = missionIndex;
	}
	int cruiseSpeed;
	if(PARAM_TestHarnessCruiseSpeed.Get(cruiseSpeed))
	{
		m_CruiseSpeed = (float)cruiseSpeed;
	}

	float coords[3];
	if(PARAM_TestHarnessStartcoords.GetArray(coords, 3))
	{
		m_StartMat.d.x = coords[0];
		m_StartMat.d.y = coords[1];
		m_StartMat.d.z = coords[2];
		m_bUseStartPos = true;
	}
	if(PARAM_TestHarnessTargetcoords.GetArray(coords, 3))
	{
		m_TargetPos.x = coords[0];
		m_TargetPos.y = coords[1];
		m_TargetPos.z = coords[2];
	}
}

void CPedDebugVisualiserMenu::InitBank()
{
	if(ms_pBank)
	{
		ShutdownBank();
	}

	if (PARAM_focusTargetDebugMode.Get())
	{
		ms_menuFlags.m_bOnlyDisplayForFocus = true;
		ms_menuFlags.m_bFocusPedDisplayIn2D = true;
	}

	if (PARAM_onlydisplayforfocusped.Get())
		ms_menuFlags.m_bOnlyDisplayForFocus = true;

	if (PARAM_focusPedDisplayIn2D.Get())
		ms_menuFlags.m_bFocusPedDisplayIn2D = true;

	if(PARAM_visualiseplayer.Get())
	{
		ms_menuFlags.m_bOnlyDisplayForPlayers = true;
		ms_menuFlags.m_bVisualisePedsInVehicles = true;
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("A.I.", 0, 0, false);
	if(weaponVerifyf(ms_pBank, "Failed to create A.I. bank"))
	{
		ms_pCreateButton = ms_pBank->AddButton("Create A.I. widgets", &CPedDebugVisualiserMenu::CreateBank);
		CCoverDebug::InitWidgets();
	}
}

EXTERN_PARSER_ENUM(ePedConfigFlags);
EXTERN_PARSER_ENUM(ePedResetFlags);

static const char * spRelationshipText[] =
{
	"ACQUAINTANCE_TYPE_PED_RESPECT",
	"ACQUAINTANCE_TYPE_PED_LIKE",
	"ACQUAINTANCE_TYPE_PED_IGNORE",
	"ACQUAINTANCE_TYPE_PED_DISLIKE",
	"ACQUAINTANCE_TYPE_PED_WANTED",
	"ACQUAINTANCE_TYPE_PED_HATE",
	"ACQUAINTANCE_TYPE_PED_DEAD"
};

void CPedDebugVisualiserMenu::CreateBank()
{
	aiAssertf(ms_pBank, "A.I. bank needs to be created first");
	if(ms_pCreateButton)
	{
		ms_pCreateButton->Destroy();
		ms_pCreateButton = NULL;
	}
	else
	{
		//bank must already be setup as the create button doesn't exist so just return.
		return;
	}

	bkBank& bank = *ms_pBank;

	bank.AddCombo("Ped debugging", (int*)&CPedDebugVisualiser::eDisplayDebugInfo, CPedDebugVisualiser::eNumModes, pPedDebuggingText, NullCB );

	{
		bank.PushGroup("Focus Entity Ped Selection Controls", false);
		bank.AddButton("Set Focus Entity to Player", ChangeFocusToPlayer, "Sets the focused entity to be the players ped");
		bank.AddButton("Set Focus Entity to Closest Ped Or Dummy Ped to the Player", ChangeFocusToClosestToPlayer,  "Sets the focused ped to one closest to the players ped");
		bank.AddButton(">> Change Focus Entity to Ped >>", ChangeFocusForwards, "Cycles the focused entity forwards through all currently existing peds");
		bank.AddButton("<< Change Focus Entity to Ped <<", ChangeFocusBack, "Cycles the focused entity back through all currently existing peds");
#if __DEV
		bank.AddToggle("Break On ProcessControl Of Focus Entity",				&CDebugScene::ms_bBreakOnProcessControlOfFocusEntity,				NullCB, "Causes the debugger to break in the ::ProcessControl() function of the focus entity");
		bank.AddToggle("Break On ProcessIntelligence Of Focus Entity",			&CDebugScene::ms_bBreakOnProcessIntelligenceOfFocusEntity,			NullCB, "Causes the debugger to break in the ::ProcessIntelligence() function of the focus entity");
		bank.AddToggle("Break On ProcessPhysics Of Focus Entity",				&CDebugScene::ms_bBreakOnProcessPhysicsOfFocusEntity,				NullCB, "Causes the debugger to break in the ::ProcessPhysics() function of the focus entity");
		bank.AddToggle("Break On PreRender Of Focus Entity",					&CDebugScene::ms_bBreakOnPreRenderOfFocusEntity,					NullCB, "Causes the debugger to break in the ::PreRender() function of the focus entity");
		bank.AddToggle("Break On UpdateAnim Of Focus Entity",					&CDebugScene::ms_bBreakOnUpdateAnimOfFocusEntity,					NullCB, "Causes the debugger to break in the ::UpdateAnim() function of the focus entity");
		bank.AddToggle("Break On UpdateAnimAfterCameraUpdate Of Focus Entity",	&CDebugScene::ms_bBreakOnUpdateAnimAfterCameraUpdateOfFocusEntity,	NullCB, "Causes the debugger to break in the ::UpdateAnimAfterCameraUpdate() function of the focus entity");
		bank.AddToggle("Break On Render Of Focus Entity",						&CDebugScene::ms_bBreakOnRenderOfFocusEntity,						NullCB, "Causes the debugger to break in the ::Render() function of the focus entity");
		bank.AddToggle("Break On AddToDrawList Of Focus Entity",				&CDebugScene::ms_bBreakOnAddToDrawListOfFocusEntity,				NullCB, "Causes the debugger to break in the ::AddToDrawList() function of the focus entity");
		bank.AddToggle("Break On Destroy Of Focus Entity",						&CDebugScene::ms_bBreakOnDestroyOfFocusEntity,						NullCB, "Causes the debugger to break in the Destroy (~) function of the focus entity");
		bank.AddToggle("Break On CalcDesiredVelocity Of Focus Entity",			&CDebugScene::ms_bBreakOnCalcDesiredVelocityOfFocusEntity,			NullCB, "Causes the debugger to break in the ::CalcDesiredVelocity() function of the focus entity");
		bank.AddToggle("Stop ProcessControl Of All Entities",					&CDebugScene::ms_bStopProcessCtrlAllEntities,						NullCB, "Stops processing all entities");
		bank.AddToggle("Stop ProcessControl Of Entities Except Focus0",			&CDebugScene::ms_bStopProcessCtrlAllExceptFocus0Entity,				NullCB, "Stops processing all entities except the focus entity");
		bank.AddToggle("Stop ProcessControl Of All Entities Of Focus0 Type",	&CDebugScene::ms_bStopProcessCtrlAllEntitiesOfFocus0Type,				NullCB, "Stops processing all entities of focus type");
		bank.AddToggle("Stop ProcessControl Of Entities Of Focus0 Type Except Focus0",	&CDebugScene::ms_bStopProcessCtrlAllOfFocus0TypeExceptFocus0,	NullCB, "Stops processing all entities of focus type except the focus entity");
#endif // __DEV
		bank.PopGroup();
	}

	{
		bank.PushGroup("Ped Config Flags", false);

		// Add a toggle for each bit
		for(int i = 0; i < PARSER_ENUM(ePedConfigFlags).m_NumEnums; i++)
		{
			const char* name = parser_ePedConfigFlags_Strings[i];
			if (name)
			{
				u32 bitsPerBlock = sizeof(u32) * 8;
				int block = i / bitsPerBlock;
				int bitInBlock = i - (block * bitsPerBlock);
				bank.AddToggle(name, (reinterpret_cast<u32*>(&ms_DebugPedConfigFlagsBitSet.BitSet()) + block), (u32)(1 << bitInBlock));
			}
		}

		bank.AddButton("Set Selected Config Flags On Focus Ped", SetSelectedConfigFlagsOnFocusPed);
		bank.AddButton("Clear Selected Config Flags On Focus Ped", ClearSelectedConfigFlagsOnFocusPed);
		bank.PopGroup();

		bank.PushGroup("Ped Reset Flags", false);

		// Add a toggle for each bit
		for(int i = 0; i < PARSER_ENUM(ePedResetFlags).m_NumEnums; i++)
		{
			const char* name = parser_ePedResetFlags_Strings[i];
			if (name)
			{
				u32 bitsPerBlock = sizeof(u32) * 8;
				int block = i / bitsPerBlock;
				int bitInBlock = i - (block * bitsPerBlock);
				bank.AddToggle(name, (reinterpret_cast<u32*>(&ms_DebugPedPostPhysicsResetFlagsBitSet.BitSet()) + block), (u32)(1 << bitInBlock));
			}
		}

		bank.AddButton("Set Selected Post Physics Reset Flags On Focus Ped", SetSelectedPostPhysicsResetFlagsOnFocusPed);
		bank.AddButton("Clear Selected PostPhysics Reset Flags On Focus Ped", ClearSelectedPostPhysicsResetFlagsOnFocusPed);
		bank.PopGroup();

		bank.PushGroup("Ped Relationship Groups", false);
			bank.AddButton("Spew All Relationship Groups To TTY", SpewRelationShipGroupsToTTY);
			bank.AddText("Add/Remove/Set Group", &ms_addRemoveSetRelGroupText[0], 60, false);
 			bank.AddButton("Add Relationship Group", AddRelationShipGroup);
 			bank.AddButton("Remove Relationship Group", RemoveRelationShipGroup);
			bank.AddButton("Set Relationship Group", SetRelationShipGroup);
			bank.AddToggle("Blips On/Off", &ms_BlipPedsInRelGroup);
			bank.AddButton("Set Blips For Peds In Relationship Group ", SetBlipPedsInRelationshipGroup);
			bank.AddText("Relationship Group 1", &ms_RelGroup1Text[0], 60, false);
			bank.AddText("Relationship Group 2", &ms_RelGroup2Text[0], 60, false);

			m_pTasksCombo = bank.AddCombo(
				"RELATIONSHIP TYPE",
				&m_iRelationshipsComboIndex,
				MAX_NUM_ACQUAINTANCE_TYPES,
				spRelationshipText,
				NullCB,	//OnSelectTaskCombo,
				"Selects a relationship type for setting between groups"
				);

 			bank.AddButton("Set Relationship Between Group 1 And Group 2", SetRelationshipBetweenGroups);
			bank.AddButton("Get Relationship Between Group 1 And Group 2", GetRelationshipBetweenGroups);
			bank.AddButton("Toggle Friendly Fire", ToggleFriendlyFire);
		bank.PopGroup();
	}

	{
		bank.PushGroup("Display", false);
		bank.AddSlider("Default visualise range", &CPedDebugVisualiser::ms_fDefaultVisualiseRange, 0.0f, 1000.0f, 0.1f, NullCB, "Default range to visualise ped debugging");
		bank.AddToggle("Only display for focus ped", &ms_menuFlags.m_bOnlyDisplayForFocus, NullCB, "Debug info is only displayed above the focus ped");
		bank.AddToggle("Render focus ped display in 2D", &ms_menuFlags.m_bFocusPedDisplayIn2D, NullCB, "Debug info is rendered at side of screen");
		bank.AddToggle("Only display for player peds", &ms_menuFlags.m_bOnlyDisplayForPlayers, NullCB, "Debug info is only displayed above player peds");
		bank.AddToggle("Don't display for player", &ms_menuFlags.m_bDontDisplayForPlayer, NullCB, "Debug info isnt displayed above the player");
		bank.AddToggle("Don't display for riders", &ms_menuFlags.m_bDontDisplayForRiders, NullCB, "Debug info isn't displayed for (horse) riders");
		bank.AddToggle("Don't display for mounts", &ms_menuFlags.m_bDontDisplayForMounts, NullCB, "Debug info isn't displayed for peds (horses) that have a rider");
		bank.AddToggle("Don't display for dead peds", &ms_menuFlags.m_bDontDisplayForDeadPeds, NullCB, "Debug info isn't displayed for dead peds");
		bank.AddToggle("Display Ped Addresses", &ms_menuFlags.m_bDisplayAddresses, NullCB, "Displays/hides the hex address of all peds");
		bank.AddToggle("Visualise Peds In Vehicles", &ms_menuFlags.m_bVisualisePedsInVehicles, NullCB, "Toggles the visualising of peds in vehicles");
		bank.AddToggle("Display Combat Director Anim Streaming", &ms_menuFlags.m_bDisplayCombatDirectorAnimStreaming, NullCB, "Toggles whether the combat director anim streaming is shown");
		bank.AddToggle("Display Primary Task State History", &ms_menuFlags.m_bDisplayPrimaryTaskStateHistory, NullCB, "Toggles whether Primary state history is shown");
		bank.AddToggle("Display Movement Task State History", &ms_menuFlags.m_bDisplayMovementTaskStateHistory, NullCB, "Toggles whether Movement state history is shown");
		bank.AddToggle("Display Motion Task State History", &ms_menuFlags.m_bDisplayMotionTaskStateHistory, NullCB, "Toggles whether Motion state history is shown");
		bank.AddToggle("Display Secondary Task State History", &ms_menuFlags.m_bDisplaySecondaryTaskStateHistory, NullCB, "Toggles whether Secondary state history is shown");
		bank.AddToggle("Display Group Info", &ms_menuFlags.m_bDisplayGroupInfo, NullCB, "Toggles whether information about ped groups is displayed");
		bank.AddToggle("Display Animated Col", &ms_menuFlags.m_bDisplayAnimatedCol, NullCB, "Toggles whether animated collision is displayed");
		bank.AddToggle("Display Cover Points", &CCoverDebug::ms_Tunables.m_RenderCoverPoints, NullCB, "Toggles whether cover points are displayed");
		bank.AddToggle("Display Cover Point addresses", &CCoverDebug::ms_Tunables.m_RenderCoverPointAddresses, NullCB, "Toggles whether cover point arcs are displayed");
		bank.AddToggle("Display Cover Point types", &CCoverDebug::ms_Tunables.m_RenderCoverPointTypes, NullCB, "Toggles whether cover point types are displayed");
		bank.AddToggle("Display Cover Point arcs", &CCoverDebug::ms_Tunables.m_RenderCoverPointArcs, NullCB, "Toggles whether cover point addresses are displayed");
		bank.AddToggle("Display Cover Point height rulers", &CCoverDebug::ms_Tunables.m_RenderCoverPointHeightRulers, NullCB, "Toggles whether cover point height rulers are displayed");
		bank.AddToggle("Display Cover Point usage", &CCoverDebug::ms_Tunables.m_RenderCoverPointUsage, NullCB, "Toggles whether cover point usage text is displayed");
		bank.AddToggle("Display Cover Point low corner", &CCoverDebug::ms_Tunables.m_RenderCoverPointLowCorners, NullCB, "Toggles whether cover point low corners marks are displayed");
		bank.AddToggle("Display debug shooting", &ms_menuFlags.m_bDisplayDebugShooting, NullCB, "Displays red debug lines to represent shots!");
		bank.AddToggle("Display debug accuracy", &ms_menuFlags.m_bDisplayDebugAccuracy, NullCB, "Displays a cone to represent bullet spread");
		bank.AddToggle("Display Ped relationships", &ms_menuFlags.m_bVisualiseRelationships, NullCB, "Displays relationships as lines, green for friendly, red for hostile");
		bank.AddToggle("Display defensive areas", &ms_menuFlags.m_bVisualiseDefensiveAreas, NullCB, "Displays defensive areas, with a line from each ped to their area.");
		bank.AddToggle("Disable display defensive areas for dead Peds", &ms_menuFlags.m_bDisableVisualiseDefensiveAreasForDeadPeds, NullCB, "Disables displays defensive areas for dead peds.");
		bank.AddToggle("Display secondary defensive areas", &ms_menuFlags.m_bVisualiseSecondaryDefensiveAreas, NullCB, "Displays secondary defensive areas.");
		bank.AddToggle("Display attack window for focus ped", &ms_menuFlags.m_bVisualiseAttackWindowForFocusPed, NullCB, "Displays attack window for focus ped only, one sphere for the min and one for the max.");
		bank.AddToggle("Display nearby ladders", &ms_menuFlags.m_bVisualiseNearbyLadders, NullCB, "Displays ladders with 2dfx in proximity to the player.");
		bank.AddToggle("Display Ped bravery", &ms_menuFlags.m_bDisplayPedBravery, NullCB, "Display Ped bravery.");
		bank.AddToggle("Display Cover search", &ms_menuFlags.m_bDisplayCoverSearch, NullCB, "Display Cover search.");
		bank.AddToggle("Display Cover line tests", &ms_menuFlags.m_bDisplayCoverLineTests, NullCB, "Displays cover line tests that were spamming TasksFull.");
		bank.AddToggle("Display Ped groups", &ms_menuFlags.m_bDisplayPedGroups, NullCB, "Display ped groups.");
		bank.AddToggle("Display ambient streaming use", &CAmbientClipStreamingManager::DISPLAY_AMBIENT_STREAMING_USE, NullCB, ".");
		bank.AddToggle("Display FSM state transitions for focus ped", &aiTaskTree::ms_bSpewStateTransToTTY);
#if __DEV
		bank.AddToggle("Display FOV perception tests", &CPedPerception::ms_bDrawVisualField, NullCB, ".");
		bank.AddToggle("Display script task history code tasks", &ms_menuFlags.m_bDisplayScriptTaskHistoryCodeTasks, NullCB, ".");
		bank.AddToggle("Display Ped Spatial Array", &ms_menuFlags.m_bDisplayPedSpatialArray);
		bank.AddToggle("Display Vehicle Spatial Array", &ms_menuFlags.m_bDisplayVehicleSpatialArray);
#endif // __DEV
		bank.AddToggle("Log AI events", &ms_menuFlags.m_bLogAIEvents, NullCB, ".");

		bank.PopGroup();
	}

	{
		bank.PushGroup("Animation", false);
		bank.PopGroup();
	}

	{
		bank.PushGroup("IK", false);

			bank.PushGroup("Head IK", false);

				bank.AddToggle("Enable",		 &CPedDebugVisualiserMenu::ms_bHeadIKEnable);
				bank.AddVector("Target",		 &ms_vHeadIKTarget, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f );
				bank.AddToggle("Toggle World/Model Space", &CPedDebugVisualiserMenu::ms_bHeadIKUseWorldSpace);

				bank.AddSlider("Blend in time",  &CPedDebugVisualiserMenu::ms_fHeadIKBlendInTime, 0.01f, 10.0f, 0.01f);
				bank.AddSlider("Blend out time", &CPedDebugVisualiserMenu::ms_fHeadIKBlendOutTime, 0.01f, 10.0f, 0.01f);
				bank.AddSlider("Time",			 &CPedDebugVisualiserMenu::ms_fHeadIKTime, -1.0f, 10.0f, 0.01f);

				bank.AddToggle("ms_bSlowTurnRate", &CPedDebugVisualiserMenu::ms_bSlowTurnRate);
				bank.AddToggle("ms_bFastTurnRate", &CPedDebugVisualiserMenu::ms_bFastTurnRate);
				bank.AddToggle("ms_bNarrowestYawLimit", &CPedDebugVisualiserMenu::ms_bNarrowestYawLimit);
				bank.AddToggle("ms_bNarrowestPitchLimit", &CPedDebugVisualiserMenu::ms_bNarrowestPitchLimit);
				bank.AddToggle("ms_bNarrowYawLimit", &CPedDebugVisualiserMenu::ms_bNarrowYawLimit);
				bank.AddToggle("ms_bNarrowPitchLimit", &CPedDebugVisualiserMenu::ms_bNarrowPitchLimit);
				bank.AddToggle("ms_bWideYawLimit", &CPedDebugVisualiserMenu::ms_bWideYawLimit);
				bank.AddToggle("ms_bWidePitchLimit", &CPedDebugVisualiserMenu::ms_bWidePitchLimit);
				bank.AddToggle("ms_bWidestYawLimit", &CPedDebugVisualiserMenu::ms_bWidestYawLimit);
				bank.AddToggle("ms_bWidestPitchLimit", &CPedDebugVisualiserMenu::ms_bWidestPitchLimit);
				bank.AddToggle("ms_bWhileNotInFOV", &CPedDebugVisualiserMenu::ms_bWhileNotInFOV);
				bank.AddToggle("ms_bUseEyesOnly", &CPedDebugVisualiserMenu::ms_bUseEyesOnly);
			bank.PopGroup(); // "Head IK"

			bank.PushGroup("Arm IK", false);
				bank.AddSlider("Blend in time", &ms_fArmIKBlendInTime, 0.01f, 10.0f, 0.01f);
				bank.AddSlider("Blend out time", &ms_fArmIKBlendOutTime, 0.01f, 10.0f, 0.01f);
				bank.AddSlider("Blend in range", &ms_fArmIKBlendInRange, 0.0f, 10.0f, 0.01f);
				bank.AddSlider("Blend out range", &ms_fArmIKBlendOutRange, 0.0f, 10.0f, 0.01f);
				bank.AddToggle("Enable left arm", &CPedDebugVisualiserMenu::ms_bUseLeftArmIK);
				bank.AddVector("Left arm target", &ms_vTargetLeftArmIK, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f );
				bank.AddToggle("Enable right arm", &CPedDebugVisualiserMenu::ms_bUseRightArmIK);
				bank.AddVector("Right arm target", &ms_vTargetRightArmIK, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f );
				bank.AddToggle("Use bone and offset", &CPedDebugVisualiserMenu::ms_bUseBoneAndOffset);
				bank.AddToggle("Target in world space", &CPedDebugVisualiserMenu::ms_bTargetInWorldSpace);
				bank.AddToggle("Target w.r.t. hand", &CPedDebugVisualiserMenu::ms_bTargetWRTHand);
				bank.AddToggle("Target w.r.t. point helper", &CPedDebugVisualiserMenu::ms_bTargetWRTPointHelper);
				bank.AddToggle("Target w.r.t. ik manager", &CPedDebugVisualiserMenu::ms_bTargetWRTIKHelper);
				bank.AddToggle("Use allow tags from anim", &CPedDebugVisualiserMenu::ms_bUseAnimAllowTags);
				bank.AddToggle("Use block tags from anim", &CPedDebugVisualiserMenu::ms_bUseAnimBlockTags);
			bank.PopGroup();

			bank.PushGroup("Look IK", false);
				const char* aszTurnRate[] = { "SLOW", "NORMAL", "FAST" };
				const char* aszRefDir[] = { "LOOK TARGET", "FACING", "MOVER", "HEAD" };
				const char* aszRotLim[] = { "OFF", "NARROWEST", "NARROW", "WIDE", "WIDEST" };
				const char* aszHeadAtt[] = { "OFF", "LOW", "MED", "FULL" };
				const char* aszArmComp[] = { "OFF", "LOW", "MED", "FULL", "IK" };
				const char* aszBlendRate[] = { "SLOWEST", "SLOW", "NORMAL", "FAST", "FASTEST" };

				bank.AddToggle("Debug Target", &CBodyLookIkSolver::ms_bRenderTargetPosition, NullCB, "");
				bank.AddSeparator();
				bank.AddToggle("Enable", &ms_bLookIKEnable);
				bank.AddVector("Target", &ms_vLookIKTarget, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 0.01f);
				bank.AddToggle("World Space (Model Space)", &ms_bLookIKUseWorldSpace);
				bank.AddToggle("Use Camera Position", &ms_bLookIKUseCamPos);
				bank.AddToggle("Use Player Position", &ms_bLookIkUsePlayerPos);
				bank.AddToggle("Sync Target To Camera Position", &ms_bLookIKUpdateTarget);
				bank.AddToggle("Use Tags From MoVE Network", &ms_bLookIkUseTags);
				bank.AddToggle("Use Allow Tags From MoVE Network", &ms_bLookIkUseAnimAllowTags);
				bank.AddToggle("Use Block Tags From MoVE Network", &ms_bLookIkUseAnimBlockTags);
				bank.AddToggle("Use Tags From Animation/Preview Anims", &ms_bLookIkUseTagsFromAnimViewer);
				bank.AddCombo("Turn Rate", &ms_uTurnRate, 3, aszTurnRate);
				bank.AddCombo("Blend In", &ms_auBlendRate[0], 5, aszBlendRate);
				bank.AddCombo("Blend Out", &ms_auBlendRate[1], 5, aszBlendRate);
				bank.AddCombo("Eye Reference Direction", &ms_uRefDirEye, 4, aszRefDir);
				bank.AddCombo("Head Reference Direction", &ms_uRefDirHead, 4, aszRefDir);
				bank.AddCombo("Neck Reference Direction", &ms_uRefDirNeck, 4, aszRefDir);
				bank.AddCombo("Torso Reference Direction", &ms_uRefDirTorso, 4, aszRefDir);
				bank.AddCombo("Head Yaw", &ms_uRotLimHead[0], 5, aszRotLim);
				bank.AddCombo("Head Pitch", &ms_uRotLimHead[1], 5, aszRotLim);
				bank.AddCombo("Head Attitude", &ms_uHeadAttitude, 4, aszHeadAtt);
				bank.AddCombo("Neck Yaw", &ms_uRotLimNeck[0], 5, aszRotLim);
				bank.AddCombo("Neck Pitch", &ms_uRotLimNeck[1], 5, aszRotLim);
				bank.AddCombo("Torso Yaw", &ms_uRotLimTorso[0], 5, aszRotLim);
				bank.AddCombo("Torso Pitch", &ms_uRotLimTorso[1], 5, aszRotLim);
				bank.AddCombo("Left Arm Compensation", &ms_auArmComp[0], 5, aszArmComp);
				bank.AddCombo("Right Arm Compensation", &ms_auArmComp[1], 5, aszArmComp);
			bank.PopGroup();

		bank.PopGroup(); // "IK"
	}

	{
		bank.PushGroup("Tasks", false);

		eFocusPedTasks iTaskTypes[] =
		{
			eFollowEntityOffset,
			eStandStill,
			eFacePlayer,
			eAddToPlayerGroup,
			eRemoveFromPlayerGroup,
			eWander,
			eCarDriveWander,
			eUnalerted,
			eFlyingWander,
			eSwimmingWander,
			eGoToPointToCameraPos,
			eFlyToPointToCameraPos,
			eFollowNavMeshToCameraPos,
			eGenericMoveToCameraPos,
			eFollowNavMeshToCameraPosWithSlide,
			eFollowNavMeshAndSlideInSequence,
			eSlideToCoordToCameraPos,
			eSetMoveTaskTargetToCameraPos,
			eSetDesiredHeadingToFaceCamera,
			eWalkAlongsideClosestPed,
			eTaskDie,
			eSeekCover,
			eUseMobilePhone,
			eHandsUp,
			eEvasiveStep,
			eAvoidanceTest,
			eGetOffBoat,
			eDefendCurrentPosition,
			eEnterNearestCar,
			eShuffleBetweenSeats,
			eExitNearestCar,
			eClearCharTasks,
			eFlushCharTasks,
			eDriveToLocator,
			eGoDirectlyIntoCover,
			eMobileChat,
			eMobileChat2Way,
			eShootAtPoint,
			eGoToPointShooting,
			eGoToPointShootingPlayer,
			eUseNearestScenario,
			eUseNearestScenarioWarp,
			eFallAndGetup,
			eClimbUp,
			eClimbVault,
			eReactToCarCollision,
			eNavMeshAndTurnToFacePedInSequence,
			eNMRelax,
			eNMPose,
#if __DEV
			eNMBindPose,
#endif	//	__DEV
			eNMBrace,
			eNMBuoyancy,
			eNMShot,
			eNMMeleeHit,
			eNMHighFall,
			eNMBalance,
			eNMBalanceGrab,
#if ENABLE_DRUNK
			eNMDrunk,
#endif // ENABLE_DRUNK
			//eNMStumble,
			eNMExplosion,
			eNMOnFire,
			eNMFlinch,
			eNMRollUp,
			eNMFallOverWall,
			eNMRiverRapids,

			eTreatAccident,
			eDropOverEdge,

			eDragInjuredToSafety,
			eVariedAimPose,

			eCombat,

			eFollowNavmeshToMeasuringToolStopExactly,
			eCowerCrouched,
			eCowerStanding,
			eShellShocked,
			eReachArm,
			eMoveFaceTarget,

			eShockingBackAway,
			eShockingHurryAway,
			eShockingReact,

			eSharkAttack,
			eSharkCircleForever,

			eCallPolice,
			eBeastJump,

			eNoTask
		};
		static const char * pTaskText[] =
		{
			"Follow entity offset",
			"Stand Still",
			"Face Player",
			"Add To Player's Group",
			"Remove From Player's Group",
			"Wander",
			"Car Drive Wander",
			"Unalerted",
			"Flying Wander",
			"Swimming Wander",
			"GotoPoint To Camera Pos",
			"Fly to Point To Camera Pos",
			"eFollowNavMeshToCameraPos",
			"Generic Move to Camera Position",
			"eFollowNavMeshToCameraPosWithSlide",
			"eFollowNavMeshAndSlideInSequence",
			"eSlideToCoordToCameraPos",
			"eSetMoveTaskTargetToCameraPos",
			"eSetDesiredHeadingToFaceCamera",
			"eWalkAlongsideClosestPed",
			//
			"Die",
			"Seek cover",
			"Use Mobile Phone",
			"Hands Up",
			"Evasive Step",
			"Avoidance Test",
			"Get Off Boat",
			"Defend Position",
			"Enter Nearest Car",
			"Shuffle Between Seats",
			"Exit Nearest Car",
			"Clear Char Tasks",
			"Flush Char Tasks",
			"Drive to locator",
			"GoDirectlyIntoCover",
			"MobileChat",
			"MobileChat2Way",
			"ShootAtPoint",
			"GoToPointShooting",
			"GoToPointShootingPlayer",
			"UseNearestScenario",
			"UseNearestScenarioWarp",
			"Fall and Getup",
			"Climb Up",
			"Climb Vault",
			"React To Car Collision",
			"NavMeshAndTurnToFacePedInSequence",
			"NM Relax",
			"NM Pose",
#if __DEV
			"NM BindPose",
#endif	//	__DEV
			"NM Brace",
			"NM Buoyancy",
			"NM Shot",
			"NM MeleeHit",
			"NM HighFall",
			"NM Balance",
			"NM Bal_Grab",
#if ENABLE_DRUNK
			"NM Drunk",
#endif // ENABLE_DRUNK
			//"NM Stumble",
			"NM Explosion",
			"NM OnFire",
			"NM Flinch",
			"NM RollUp",
			"NM FallOverWall",
			"NM RiverRapids",
			"TreatAccident",
			"DropOverEdge",
			"Drag Injured To Safety",
			"Varied Aim Pose",
			"Combat",
			"Follow navmesh stop exactly (measuring tool)",
			"Cower (crouched anims)",
			"Cower (standing anims)",
			"Shell Shocked",
			"Reach arm",
			"Move face target (player)",

			"Shocking back away (explosion player)",
			"Shocking hurry away (explosion player)",
			"Shocking react (explosion player)",

			"Shark attack (player)",
			"Shark circle forever (player)",

			"Call police (player)",

			"Beast jump",

			0
		};

		u32 iTaskCount = 0;
		while(iTaskTypes[iTaskCount] != eNoTask)
		{
			m_TaskComboTypes.PushAndGrow(iTaskTypes[iTaskCount]);
			Assert(pTaskText[iTaskCount]);
			iTaskCount++;
		}

		m_pTasksCombo = bank.AddCombo(
			"Focus Ped Task :",
			&m_iTaskComboIndex,
			iTaskCount,
			pTaskText,
			NullCB,	//OnSelectTaskCombo,
			"Selects a task to the focus ped"
			);

		bank.AddText("Relative Offset To Target Entity To Maintain x y z", g_vCurrentOffsetPosition, sizeof(g_vCurrentOffsetPosition), false, &MoveFollowEntityOffset);
		bank.AddSlider("TargetRadius", &g_fTargetRadius, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("MoveBlendRatio", &g_fTaskMoveBlendRatio, 0.0f, 3.0f, 0.1f);

		bank.PushGroup("Tasks - Additional Vars", false);
		//Defend Position Tests
		bank.AddToggle("Defend Pos Patrols",&g_bIsPatrolling,NullCB,"Whether or not Guard AI Patrols.");
		bank.AddSlider("Defend Pos Proximity",&g_fProximity,0.5f,100.0f,0.5f,NullCB,"What is the range of any patrol.");
		bank.AddSlider("Defend Pos Task Timer",&g_fTimer,-1.0f,1000.0f,1.0f,NullCB,"Task Timer.");

		//Getup Tests
		static const char * pGetupStates[] = { "GETUP_FRONT", "GETUP_RIGHT", "GETUP_LEFT", "GETUP_BACK" };
		m_pTasksCombo = bank.AddCombo(
			"GetupState :",
			&g_iTaskGetupState,
			4,
			pGetupStates,
			NullCB,
			"GetupState for task"
			);
		static const char * pGetupSpeeds[] = { "GETUP_FAST", "GETUP_NORMAL", "GETUP_SLOW" };
		m_pTasksCombo = bank.AddCombo(
			"GetupSpeed :",
			&g_iTaskGetupSpeed,
			3,
			pGetupSpeeds,
			NullCB,
			"GetupSpeed for task"
			);
		bank.PopGroup();

		bank.AddButton("Give Task", OnSelectTaskCombo, "Gives selected task to focus ped");

		bank.AddButton("Drop Ped At Camera Position", DropPedAtCameraPos, "Drops the focus ped at the camera position");

		bank.PopGroup();

		bank.PushGroup("Task test harness");
			bank.AddButton("Print Test harness params", PrintTestHarnessParamsCB, "Prints out params that can be used in the command line");
			bank.AddButton("Kill selected ped", CPedDebugVisualiserMenu::KillFocusPed, "Sets the focus peds health to zero");
			bank.PushGroup("Start pos");
				bank.AddToggle( "Warp to start position when given task", &CPedDebugVisualiserMenu::m_bUseStartPos );
				bank.AddSlider("X", &m_StartMat.d.x,-9999.0f,9999.0f,0.5f);
				bank.AddSlider("Y", &m_StartMat.d.y,-9999.0f,9999.0f,0.5f);
				bank.AddSlider("Z", &m_StartMat.d.z,-9999.0f,9999.0f,0.5f);
				bank.AddButton( "Set start pos from measuring tool", SetStartPosToMeasuringTool, "Set target pos from measuring tool");
				bank.AddButton( "Set start pos from debug cam", SetStartPosToDebugCam, "Set target pos from debug cam");
				bank.AddButton( "Set start pos from Focus Entity", SetStartPosToFocusPos, "Set target pos from measuring tool");
				bank.AddButton("Warp to start position", WarpToStartCB, "Warps");
			bank.PopGroup();
			static const char* szTaskHarnessNames[100];
			s32 iTaskHarnessCount = 0;
			for( s32 i = 0; iTaskTypesWithTestHarness[i] != CTaskTypes::TASK_NONE; i++ )
			{
				szTaskHarnessNames[i] = TASKCLASSINFOMGR.GetTaskName(iTaskTypesWithTestHarness[i]);
				++iTaskHarnessCount;
			}
			m_pTasksCombo = bank.AddCombo(
				"Task:",
				&iSelectedTestHarness,
				iTaskHarnessCount,
				szTaskHarnessNames,
				NullCB,
				"Task harness selection"
				);
			static const char * pDrivingNames[] =
			{
				"None",
				"DMode_StopForCars",
				"DMode_StopForCars_Strict",
				"DMode_AvoidCars",
				"DMode_AvoidCars_Reckless",
				"DMode_PloughThrough",
				"DMode_StopForCars_IgnoreLights",
				"DMode_AvoidCars_ObeyLights",
				"DMode_AvoidCars_StopForPeds_ObeyLights"
			};
			bank.AddButton("Give Task", TestHarnessCB, "Gives selected task to focus ped");
			bank.PushGroup("Vehicle control params");
				bank.AddToggle( "Turn on measuring tool", &CPhysics::ms_bDebugMeasuringTool );
				bank.AddCombo( "Mission:", &m_Mission, MISSION_LAST, CVehicleIntelligence::MissionStrings, NullCB, "Mission selection" );
				bank.AddButton( "Set target entity", SetVehicleMissionTargetEntity, "Select the entity to be focus entity");
				bank.AddButton( "Clear target entity", ClearVehicleMissionTargetEntity, "Select the entity to be focus entity");
				bank.AddButton( "Set target pos from measuring tool", SetVehicleMissionTargetPosToMeasuringTool, "Set target pos from measuring tool");
				bank.AddButton( "Set target pos from debug cam", SetVehicleMissionTargetPosToDebugCam, "Set target pos from debug cam");
				bank.AddButton( "Set target pos to target entity pos", SetVehicleMissionTargetPosToFocusPos, "Select the entity to be focus entity");
				bank.AddButton( "Set target pos to random road pos", SetVehicleMissionTargetPosToRandomRoadPos, "");
				bank.AddSlider("Cruise speed", &m_CruiseSpeed,0.0f,100.0f,0.1f);
				bank.AddSlider("Straight line distance", &m_StraightLineDistance,0.0f,999.0f,0.1f);
				bank.AddSlider("Target reached distance", &m_TargetReachedDistance,0.0f,999.0f,0.1f);
				bank.AddSlider("Flight orientation", &m_fHeliOrientation,-1.0f,TWO_PI,0.001f);
				bank.AddSlider("Flight Height", &m_iFlightHeight,0,999,1);
				bank.AddSlider("Min height above terrain", &m_iMinHeightAboveTerrain,0,999,1);
				bank.PushGroup("TargetPos");
					bank.AddSlider("X", &m_TargetPos.x,-9999.0f,9999.0f,0.5f);
					bank.AddSlider("Y", &m_TargetPos.y,-9999.0f,9999.0f,0.5f);
					bank.AddSlider("Z", &m_TargetPos.z,-9999.0f,9999.0f,0.5f);
				bank.PopGroup();
				bank.AddCombo( "Driving Flags:", &m_DrivingFlags, 8, pDrivingNames, SetVehicleDrivingFlags, "DrivingFlags selection" );
				s32 iIndex = 0;
				for( u32 iFlag = DF_StopForCars; ; iFlag = iFlag << 1)
				{
					char widgetName[256];
					m_abDrivingFlags[iIndex] = false;
					sprintf(widgetName, "Flag: %s", CVehicleIntelligence::GetDrivingFlagName((s32)iFlag));
					bank.AddToggle(widgetName, &m_abDrivingFlags[iIndex++]);

					if(iFlag == DF_LastFlag)
					{
						break;
					}
				}

				SetVehicleDrivingFlags();

				bank.PushGroup("AI Aircraft Testing", false);
					bank.AddSlider("Test Type", &ms_iTestPlaneType,0,2,1);
					bank.AddToggle( "Attack(on)/chase(off)", &ms_bTestPlaneAttackPlayer );
					bank.AddToggle( "Allow player control", &ms_bTestPlanePlayerControl );
					bank.AddSlider( "AI to Spawn", &ms_iTestPlaneNumToSpawn,1,10,1);
					bank.PushGroup("TargetPos");
						bank.AddSlider("X", &m_goToTarget.x,-9999.0f,9999.0f,0.5f);
						bank.AddSlider("Y", &m_goToTarget.y,-9999.0f,9999.0f,0.5f);
						bank.AddSlider("Z", &m_goToTarget.z,-9999.0f,9999.0f,0.5f);
					bank.PopGroup();
					bank.AddButton("Create Jet Test", CreateJetTest, "Spawns some jets that interact with each other");
					bank.AddButton("Create Heli Test", CreateHeliTest, "Spawns some heli that interact with each other");
				bank.PopGroup();

			bank.PopGroup();
		bank.PopGroup();

#if 0 // CS - WILL MOVE TO DATA
		bank.AddSlider("Chance of blank firing",	&CTaskFireGun::CHANCE_OF_FIRING_BLANKS,0.0f,1.0f,0.05f);
#endif // 0
	}

	{
		bank.PushGroup("Task Data", false);
		CTaskDataInfoManager::AddWidgets(bank);
		bank.PopGroup();
	}

	{
		bank.PushGroup("Events", false);

			static eFocusPedEvents iEventTypes[] =
			{
				eCarUndriveable,
				eClimbNavMeshOnRoute,
				eGunshot,
				ePotentialGetRunOver,
				eRanOverPed,
				eScriptCommand,
				eShockingCarCrash,
				eShockingDrivingOnPavement,
				eShockingDeadBody,
				eShockingRunningPed,
				eShockingGunShotFired,
				eShockingWeaponThreat,
				eShockingCarStolen,
				eShockingInjuredPed,
				eShockingGunfight1,
				eShockingGunfight2,
				eShockingSeenPedKilled,
				eNoEvent
			};
			static const char * pEventText[] =
			{
				"Car undriveable",
				"Climb nav mesh on route",
				"Gunshot (player)",
				"Potential Get Run Over (player in vehicle only)",
				"Ran Over Ped (player)",
				"Script Command",
				"Shocking Car Crash (player)",
				"Shocking Driving on Pavement (player)",
				"Shocking Dead Body (player)",
				"Shocking Running Ped (player)",
				"Shocking Gun Shot Fired (player)",
				"Shocking Weapon Threat (player)",
				"Shocking Car Stolen (player in vehicle only)",
				"Shocking injured ped (player)",
				"Shocking gunfight (source of player)",
				"Shocking gunfight (no source ped)",
				"Shocking seen ped killed (source of player)",
				0
			};

			u32 iEventCount = 0;
			while(iEventTypes[iEventCount] != eNoEvent)
			{
				m_EventComboTypes.PushAndGrow(iEventTypes[iEventCount]);
				Assert(pEventText[iEventCount]);
				iEventCount++;
			}

			m_pEventsCombo = bank.AddCombo(
				"Focus Ped Event :",
				&m_iEventComboIndex,
				iEventCount,
				pEventText,
				NullCB,
				"Selects an event for the focus ped"
				);

			bank.AddButton("Give Event", OnSelectEventCombo, "Gives selected event to focus ped");
			bank.PushGroup("Event History", false);
				bank.AddToggle("Show removed events", &CPedDebugVisualiserMenu::ms_bShowRemovedEvents);
				bank.AddToggle("Show focused peds events positions", &CPedDebugVisualiserMenu::ms_bShowFocusedPedsEventPositions);
				bank.AddToggle("Show focused peds events positions state info", &CPedDebugVisualiserMenu::ms_bShowFocusedPedsEventPositionsStateInfo);
				bank.AddToggle("Hide focused peds events history text", &CPedDebugVisualiserMenu::ms_bHideFocusedPedsEventHistoryText);
			bank.PopGroup();


		bank.PopGroup();
	}

	{
		bank.PushGroup("Targetting", false);
		bank.AddToggle( "DebugTargetting",					&CPedTargetting::DebugTargetting );
		bank.AddToggle( "DebugTargettingLos",				&CPedTargetting::DebugTargettingLos );
		bank.AddToggle( "TargettingLos - do async",			&CPedGeometryAnalyser::ms_bProcessCanPedTargetPedAsynchronously);
		bank.AddToggle( "DebugAcquaintanceScanners",		&CPedTargetting::DebugAcquaintanceScanner );
		bank.AddToggle( "AcquaintanceScanners - do async",	&CPedAcquaintanceScanner::ms_bDoPedAcquaintanceScannerLosAsync);

		bank.AddSlider("BASIC_THREAT_WEIGHTING",			&CPedTargetting::BASIC_THREAT_WEIGHTING,0.0f,2.0f,0.05f);
		bank.AddSlider("WEAPON_PISTOL_WEIGHTING",			&CPedTargetting::WEAPON_PISTOL_WEIGHTING,0.0f,5.0f,0.05f);
		bank.AddSlider("WEAPON_2HANDED_WEIGHTING",			&CPedTargetting::WEAPON_2HANDED_WEIGHTING,0.0f,5.0f,0.05f);
		bank.AddSlider("DIRECT_THREAT_WEIGHTING",			&CPedTargetting::DIRECT_THREAT_WEIGHTING,0.0f,5.0f,0.05f);
		bank.AddSlider("PLAYER_THREAT_WEIGHTING",			&CPedTargetting::PLAYER_THREAT_WEIGHTING,0.0f,5.0f,0.05f);
		bank.AddSlider("DEFEND_PLAYER_WEIGHTING",			&CPedTargetting::DEFEND_PLAYER_WEIGHTING,0.0f,5.0f,0.05f);
		bank.AddSlider("FRIENDLY_TARGETTING_REDUCTION",		&CPedTargetting::FRIENDLY_TARGETTING_REDUCTION,0.0f,5.0f,0.05f);
		bank.AddSlider("SCRIPT_THREAT_WEIGHTING",			&CPedTargetting::SCRIPT_THREAT_WEIGHTING,0.0f,5.0f,0.05f);
		bank.AddButton("Re-load target sequences from metadata", ReloadWeaponTargetSequences, "");
		bank.AddSlider("DECOY_PED_SCORING_MULTIPLIER",		&CPedTargetting::DECOY_PED_SCORING_MULTIPLIER, 0.0f, 1000.0f, 0.05f);
		bank.PopGroup();
	}

	{
		bank.PushGroup("Path Following", false);
#if __DEV
			m_pToggleAssertIfFocusPedFailsToFindNavmeshRoute = bank.AddToggle("ASSERT if this ped fails to find a navmesh route", &m_bAssertIfFocusPedFailsToFindNavmeshRoute, OnAssertIfFocusPedFailsToFindNavmeshRoute);
#endif
			bank.AddToggle("Log path requests", &CTaskNavBase::ms_bDebugRequests);
			bank.AddSlider("ms_fGotoTargetLookAheadDist", &CTaskNavBase::ms_fGotoTargetLookAheadDist, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("ms_fCorneringLookAheadTime", &CTaskNavBase::ms_fCorneringLookAheadTime, 0.0f, 10.0f, 0.1f);
			bank.AddSeparator();
			bank.AddSlider("ms_fMinCorneringMBR",&CTaskMoveGoToPoint::ms_fMinCorneringMBR,0.0f,3.0f,0.01f);
			bank.AddSlider("ms_fAccelMBR",&CTaskMoveGoToPoint::ms_fAccelMBR,0.0f,100.0f,0.01f);
			bank.AddSeparator();
			bank.AddToggle("Scan for obstructions", &CTaskNavBase::ms_bScanForObstructions);
			bank.AddSlider("Scan Freq - Walk (ms)", &CTaskNavBase::ms_iScanForObstructionsFreq_Walk, 0, 10000, 1);
			bank.AddSlider("Scan Freq - Run (ms)", &CTaskNavBase::ms_iScanForObstructionsFreq_Run, 0, 10000, 1);
			bank.AddSlider("Scan Freq - Sprint (ms)", &CTaskNavBase::ms_iScanForObstructionsFreq_Sprint, 0, 10000, 1);
			bank.AddSlider("Default distance ahead for SetNewTarget() repath", &CTaskNavBase::ms_fDefaultDistanceAheadForSetNewTarget, 0.0f, 10.0f, 0.25f);
			bank.AddToggle("Force allow ClimbLadder as subtask", &CTaskComplexControlMovement::ms_bForceAllowClimbLadderAsSubtask);

		bank.PopGroup();
	}

	{
		bank.PushGroup("Avoidance", false);

			bank.AddToggle("Perform local avoidance", &CTaskMoveGoToPoint::ms_bPerformLocalAvoidance);
			bank.AddSlider("Navmesh avoidance probe distance min", &CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeDistanceMin, 0.0f, 5.0f, 0.1f);
			bank.AddSlider("Navmesh avoidance probe distance max", &CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeDistanceMax, 0.1f, 5.0f, 0.1f);
			bank.AddSlider("Navmesh avoidance probe angle", &CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeAngle, 0.1f, 90.0f, 0.1f);
			bank.AddToggle("Allow slowing", &CTaskMoveGoToPoint::ms_bAllowStoppingForOthers);
			bank.AddToggle("Peds respond to object collisions", &CPedIntelligence::ms_bPedsRespondToObjectCollisionEvents);
			bank.AddToggle("Enable Steering around peds", &CTaskMoveGoToPoint::ms_bEnableSteeringAroundPeds);
			bank.AddToggle("Enable Steering around objects", &CTaskMoveGoToPoint::ms_bEnableSteeringAroundObjects);
			bank.AddToggle("Enable Steering around vehicles", &CTaskMoveGoToPoint::ms_bEnableSteeringAroundVehicles);
			bank.AddToggle("Enable Steering around peds that are behind me", &CTaskMoveGoToPoint::ms_bEnableSteeringAroundPedsBehindMe);

			bank.AddSlider("Angle relative to current forward avoidance weight", &CTaskMoveGoToPoint::ms_AngleRelToForwardAvoidanceWeight, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Angle relative to desired avoidance weight", &CTaskMoveGoToPoint::ms_AngleRelToDesiredAvoidanceWeight, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Time of collision avoidance weight", &CTaskMoveGoToPoint::ms_TimeOfCollisionAvoidanceWeight, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Distance from path avoidance weight", &CTaskMoveGoToPoint::ms_DistanceFromPathAvoidanceWeight, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Max avoidance time value", &CTaskMoveGoToPoint::ms_MaxAvoidanceTValue, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Collision radius extra padding", &CTaskMoveGoToPoint::ms_CollisionRadiusExtra, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Collision radius extra padding TIGHT", &CTaskMoveGoToPoint::ms_CollisionRadiusExtraTight, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Tangent radius extra padding", &CTaskMoveGoToPoint::ms_TangentRadiusExtra, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Tangent radius extra padding TIGHT", &CTaskMoveGoToPoint::ms_TangentRadiusExtraTight, -1.0f, 1.0f, 0.1f);
			bank.AddSlider("Max distance from path", &CTaskMoveGoToPoint::ms_MaxDistanceFromPath, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Failed to compute tangent max angle", &CTaskMoveGoToPoint::ms_NoTangentAvoidCollisionAngleMax, 0.0f, 180.0f, 1.0f);
			bank.AddSlider("Failed to compute tangent min angle", &CTaskMoveGoToPoint::ms_NoTangentAvoidCollisionAngleMin, 0.0f, 180.0f, 1.0f);
			bank.AddSlider("Max avoidance angle rel to desired", &CTaskMoveGoToPoint::ms_MaxAngleAvoidanceRelToDesired, 0.0f, 180.0f, 1.0f);
			bank.AddSlider("Max avoidance angle rel to target when fleeing", &CTaskMoveGoToPoint::ms_MaxRelToTargetAngleFlee, 0.0f, 180.0f, 1.0f);

			bank.AddSlider("Collision time to enable slow down", &CTaskMoveGoToPoint::ms_fMaxCollisionTimeToEnableSlowdown, 0.0f, 10.0f, 0.25f);
			bank.AddSlider("Collision speed walk", &CTaskMoveGoToPoint::ms_fCollisionMinSpeedWalk, 0.0f, 10.0f, 0.25f);
			bank.AddSlider("Collision speed stop", &CTaskMoveGoToPoint::ms_fCollisionMinSpeedStop, 0.0f, 10.0f, 0.25f);

		bank.PopGroup();
	}

	{
		bank.PushGroup("Ragdoll", false);
		bank.AddToggle("Render ragdoll type text", &ms_bRenderRagdollTypeText);
		bank.AddToggle("Render ragdoll type spheres", &ms_bRenderRagdollTypeSpheres);
		bank.AddToggle("Toggle AllowRagdolls", &CPedPopulation::ms_bAllowRagdolls);
		bank.AddButton("Switch to NM test",		SwitchPedToNMBalanceCB);
		bank.AddButton("Switch to Animated",	SwitchPedToAnimatedCB);
		bank.AddButton("Switch to Driven Ragdoll", SwitchPedToDrivenRagdollCB);
		bank.AddButton("Force Animated",		ForcePedToAnimatedCB);
		bank.AddButton("Switch to Static Frame", SwitchToStaticFrameCB);
		bank.AddButton("Clear NM Behaviours",	ClearAllBehavioursCB);
		bank.AddButton("Send NM Relax",			SetRelaxMessageCB);
		bank.AddButton("Unlock Ragdoll",		UnlockPedRagdollCB);
		bank.AddButton("Fix Ped Physics",		FixPedPhysicsCB);
		bank.AddButton("Test Code",				TriggerRagdollTestCodeCB);
		bank.AddToggle("Log Forces",			&fragInstNMGta::ms_bLogRagdollForces);
		bank.AddToggle("Run Continuous Test Code", &ms_bRagdollContinuousTestCode);
		bank.AddButton("Drop Weapon",			DropWeaponCB);
		bank.AddSlider("Draw Bone Matrix",		&ms_nRagdollDebugDrawBone, -2, 80, 1, NullCB, "Draw Bone Matrix (-1 for phInst Matrix)");
		bank.AddSlider("Test Apply Force Factor", &ms_fRagdollApplyForceFactor, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Activation Anim Vel Clamp", &CPhysics::ms_fRagdollActivationAnimVelClamp, 0.0f, 1000.0f, 0.1f);
		bank.AddSlider("Activation Anim AngVel Clamp", &CPhysics::ms_fRagdollActivationAnimAngVelClamp, 0.0f, 1000.0f, 0.1f);
		//bank.AddSlider("Bullet Force Apply Time", &CBulletForce::ms_fPedBulletApplyTime, 0.01f, 10.0f, 0.01f);

		bank.PushGroup("Physical Gun Models", false);
#if 0 // CS
		bank.AddToggle("Enable Two-Handed Constraints",	&CPedWeaponMgr::ms_bEnable2ndHandedConstraint,	NullCB, "If active, peds with 2-handed weapons will constrain both hands to the physically simulated gun");
		bank.AddSlider("Gun-Hand Constraint Strength",	&CPedWeaponMgr::ms_fPhysicalGun2ndHandConstraintStrength, -1.0f, 300.0f, 0.1f, NullCB, "breaking strength of hand-gun constraint");
#endif // 0
		bank.PopGroup();
		bank.PopGroup();
	}

	{

		CRandomEventManager::GetInstance().AddWidgets(bank);
	}

	{
		bank.PushGroup("NaturalMotion", false);

		g_AllowLegInterpenetration = FRAGNMASSETMGR->GetAllowLegInterpenetration();
		bank.AddToggle("Allow leg interpenetration", &g_AllowLegInterpenetration, ToggleAllowLegInterpenetration);
		bank.AddButton("Print ragdoll task info for selected ped", SpewRagdollTaskInfo);
		bank.AddButton("Print ragdoll usage data", PrintRagdollUsageData);
		bank.AddToggle("Enable animPose behaviour for all behaviours?", &g_nmRequestAnimPose, ToggleNaturalMotionAnimPose);
		bank.AddToggle("Show NM control task flags", &g_nmShowNmControlFlags, ToggleNaturalMotionFlagDebug);
		bank.AddToggle("Dump NM API Calls To Log",	&g_nmAPIDump, ToggleNaturalMotionAPILog);
		g_nmDistributedTasksEnabled = FRAGNMASSETMGR->AreDistributedTasksEnabled();
		bank.AddToggle("Distributed tasks",	&g_nmDistributedTasksEnabled, ToggleDistributedTasksEnabled);
		bank.AddToggle("Use specific parameter sets",	&CTaskNMBehaviour::ms_bUseParameterSets);
		bank.AddButton("Re-load task parameters", ReloadTaskParameters, "");

		g_NmBlendOutSetGoup = bank.PushGroup("NM Blend out sets", false);
		UpdateBlendFromNmSetsWidgets();
		bank.PopGroup();

		{
			bank.AddCombo("NM Debugging", (int*)&CPedDebugVisualiser::eDebugNMMode, CPedDebugVisualiser::eNumNMDebugModes, pNMDebuggingText, NullCB );
			bank.AddSlider("Interactive Shoot - Shot Force",		&CPedDebugVisualiserMenu::ms_fNMInteractiveShooterShotForce,		0.0f, 250.0f, 1.0f, NullCB, "");
			bank.AddSlider("Interactive Grab - LeanInDir Amount",	&CPedDebugVisualiserMenu::ms_fNMInteractiveGrabLeanAmount,		0.0f, 3.0f, 0.1f, NullCB, "");
		}
		bank.AddToggle("Visualise NM transforms for focus ped in-game", &CNmDebug::ms_bDrawTransforms, NullCB);
		bank.AddToggle("Send transforms in world coords (animPose test)", &CClipPoseHelper::ms_bUseWorldCoords, NullCB);
		bank.AddToggle("Freeze anim on first frame (animPose test)", &CClipPoseHelper::ms_bLockClipFirstFrame, NullCB);
		bank.AddToggle("Enable bumped by ped reactions on NM blend out", &CTaskGetUp::ms_bEnableBumpedReactionClips);
#if DEBUG_DRAW && __DEV
		bank.AddToggle("Show safe position checks on NM blend out", &CTaskGetUp::ms_bShowSafePositionChecks);
#endif // DEBUG_DRAW && __DEV
		bank.AddButton("Send fall over instruction to focus entity", SendNMFallOverInstructionToFocusEntity);
		bank.PushGroup("Teeter", false);
		bank.AddToggle("Enable teeter behaviour for all relevant behaviours?", &CTaskNMControl::ms_bTeeterEnabled);
		bank.AddToggle("Display edge detection results for teeter", &CNmDebug::ms_bDrawTeeterEdgeDetection);
		bank.AddSlider("Angle of direction vector for edge test", &CNmDebug::ms_fEdgeTestAngle, 0.0f, 2.0f*PI, 0.01f, NullCB);
		bank.PopGroup(); // "Teeter"

		bank.PushGroup("Attached test", false);
		bank.AddToggle("Enable debug draw for NM attach tasks", &CTaskAnimatedAttach::m_bEnableDebugDraw);
		bank.AddSlider("animPose: muscle stiffness of masked joints", &CClipPoseHelper::m_sfAPmuscleStiffness, -1.0f, 10.0f, 0.1f, NullCB, "");
		bank.AddSlider("animPose: stiffness of masked joints", &CClipPoseHelper::m_sfAPstiffness, 2.0f, 16.0f, 0.1f, NullCB, "");
		bank.AddSlider("animPose: damping of masked joints", &CClipPoseHelper::m_sfAPdamping, 0.0f, 2.0f, 0.1f, NullCB, "");
		bank.AddSlider("Inverse mass multiplier for attachment (0=vehicle has infinite mass, 1=vehicle has normal mass)", &CTaskAnimatedAttach::ms_fInvMassMult, 0.0f, 1.0f, 0.01f, NullCB, "");
		bank.AddSlider("sf_muscleStiffnessLeftArm", &CTaskNMGenericAttach::sf_muscleStiffnessLeftArm, -1.0f, 10.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_muscleStiffnessRightArm", &CTaskNMGenericAttach::sf_muscleStiffnessRightArm, -1.0f, 10.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_muscleStiffnessSpine", &CTaskNMGenericAttach::sf_muscleStiffnessSpine, -1.0f, 10.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_muscleStiffnessLeftLeg", &CTaskNMGenericAttach::sf_muscleStiffnessLeftLeg, -1.0f, 10.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_muscleStiffnessRightLeg", &CTaskNMGenericAttach::sf_muscleStiffnessRightLeg, -1.0f, 10.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_stiffnessLeftArm", &CTaskNMGenericAttach::sf_stiffnessLeftArm, -1.0f, 16.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_stiffnessRightArm", &CTaskNMGenericAttach::sf_stiffnessRightArm, -1.0f, 16.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_stiffnessSpine", &CTaskNMGenericAttach::sf_stiffnessSpine, -1.0f, 16.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_stiffnessLeftLeg", &CTaskNMGenericAttach::sf_stiffnessLeftLeg, -1.0f, 16.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_stiffnessRightLeg", &CTaskNMGenericAttach::sf_stiffnessRightLeg, -1.0f, 16.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_dampingLeftArm", &CTaskNMGenericAttach::sf_dampingLeftArm, 0.0f, 2.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_dampingRightArm", &CTaskNMGenericAttach::sf_dampingRightArm, 0.0f, 2.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_dampingSpine", &CTaskNMGenericAttach::sf_dampingSpine, 0.0f, 2.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_dampingLeftLeg", &CTaskNMGenericAttach::sf_dampingLeftLeg, 0.0f, 2.0f, 0.1f, NullCB, "");
		bank.AddSlider("sf_dampingRightLeg", &CTaskNMGenericAttach::sf_dampingRightLeg, 0.0f, 2.0f, 0.1f, NullCB, "");
		bank.PopGroup(); // "Attached test"

		// Tunable parameters for CNmDebug:
		bank.PushGroup("NM feedback history", false);
		bank.AddToggle("Display message history", &CNmDebug::ms_bDrawFeedbackHistory);
		bank.AddToggle("Only display messages for focus ped", &CNmDebug::ms_bFbMsgOnlyShowFocusPed);
		bank.AddToggle("Show \"success\" messages", &CNmDebug::ms_bFbMsgShowSuccess);
		bank.AddToggle("Show \"failure\" messages", &CNmDebug::ms_bFbMsgShowFailure);
		bank.AddToggle("Show \"event\" messages", &CNmDebug::ms_bFbMsgShowEvent);
		bank.AddToggle("Show \"start\" messages", &CNmDebug::ms_bFbMsgShowStart);
		bank.AddToggle("Show \"finish\" messages", &CNmDebug::ms_bFbMsgShowFinish);
		bank.AddSlider("Message window screen X", &CNmDebug::ms_fListHeaderX, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("Message window screen Y", &CNmDebug::ms_fListHeaderY, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("Message entry height", &CNmDebug::ms_fListElementHeight, 0.0f, 1.0f, 0.001f);
		bank.AddSlider("Ticks before starting colour fade", &CNmDebug::ms_nColourFadeStartTick, 0, 1000, 1);
		bank.AddSlider("Ticks to end colour fade", &CNmDebug::ms_nColourFadeEndTick, 0, 1000, 1);
		bank.AddSlider("Colour to fade to (grey-scale)", &CNmDebug::ms_nEndFadeColour, 0, 255, 1);
		bank.PopGroup(); // "NM feedback history"

		bank.PushGroup("Ragdoll component visualisation", false);
		bank.AddToggle("Show selected component matrix (choose below)", &CNmDebug::ms_bDrawComponentMatrices);
		bank.AddCombo("Impact cone ragdoll component", (int*)&CNmDebug::ms_nSelectedRagdollComponent, RAGDOLL_NUM_COMPONENTS, pRagdollComponentsText, NullCB);
		bank.PopGroup(); // "Ragdoll component visualisation"

		// Tunable parameters and other NM task specific stuff:

		//bank.PushGroup("TaskNMStumble", false);
		//bank.AddToggle("Enable debug draw", &CNmDebug::ms_bDrawStumbleEnvironmentDetection);
		//bank.PopGroup(); // "TaskNMStumble"

		bank.PushGroup("TaskNMBuoyancy", false);
		bank.AddToggle("Enable debug draw", &CNmDebug::ms_bDrawBuoyancyEnvironmentDetection);
		bank.PopGroup(); // "TaskNMBuoyancye"

		bank.PushGroup("Cuffs", false);
		bank.AddToggle("Toggle HandCuffs", &CPedDebugVisualiserMenu::ms_bUseHandCuffs, ToggleNaturalMotionHandCuffs);
		bank.AddToggle("Toggle AnkleCuffs", &CPedDebugVisualiserMenu::ms_bUseAnkleCuffs, ToggleNaturalMotionAnkleCuffs);
		bank.AddToggle("Toggle Fixed Relative Hand Positions", &CPedDebugVisualiserMenu::ms_bFreezeRelativeWristPositions);
		bank.AddToggle("Toggle Anim Pose Arms", &CPedDebugVisualiserMenu::ms_bAnimPoseArms);
		bank.AddSlider("Handcuff Constraint Length", &CPedDebugVisualiserMenu::ms_bHandCuffLength, 0.0f, 0.2f, 0.005f);
		bank.PopGroup(); // "Cuffs"

		bank.PopGroup(); // "NaturalMotion"
	}


	{

		/////////////////////////
		// PedGroups
		/////////////////////////
		bank.PushGroup("Ped Groups", false);
		m_pFormationCombo = bank.AddCombo(
			"Focus Formation :",
			&m_iFormationComboIndex,
			CPedFormationTypes::NUM_FORMATION_TYPES,
			NULL,
			OnSelectFormationCombo,
			"Assigns a formation to the focus ped's ped-group"
			);
#if __DEV
		for(int f=0; f<CPedFormationTypes::NUM_FORMATION_TYPES; f++)
		{
			const char * pFormationName = CPedFormation::GetFormationTypeName(f);
			m_pFormationCombo->SetString(f, pFormationName);
		}
#endif

		bank.AddSlider("Formation Spacing", &CPedDebugVisualiserMenu::m_fFormationSpacing, 0.0f, 50.0f, 0.5f);
		bank.AddSlider("Accel Dist", &CPedDebugVisualiserMenu::m_fFormationAccelDist, 0.0f, 50.0f, 0.5f);
		bank.AddSlider("Decel Dist", &CPedDebugVisualiserMenu::m_fFormationDecelDist, 0.0f, 50.0f, 0.5f);
		bank.AddSlider("Fake Script ID", &CPedDebugVisualiserMenu::m_iFormationSpacingScriptId, 0, 0xffffffff, 1);

		bank.AddButton("Set formation spacing", ChangeFormationSpacing);
#if __DEV
		bank.AddToggle("Debug Formations", &CPedFormation::ms_bDebugPedFormations);
#endif


		/////////////////////////

		bank.PopGroup();

		bank.PushGroup("Player", false);
		bank.AddToggle("Toggle invincibility",			&CPlayerInfo::ms_bDebugPlayerInvincible, TogglePlayerInvincibility, NULL);
		bank.AddToggle("Toggle invincibility with restore health",	&CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth, TogglePlayerInvincibilityRestoreHealth, NULL);
		bank.AddToggle("Invincibility restore health should restore armor",	&CPlayerInfo::ms_bDebugPlayerInvincibleRestoreArmorWithHealth);
		bank.AddToggle("Toggle invisibility",			&CPlayerInfo::ms_bDebugPlayerInvisible);
		bank.AddToggle("Toggle infinite stamina",		&CPlayerInfo::ms_bDebugPlayerInfiniteStamina);
		bank.AddToggle("No sprinting in interiors as default",	&CTaskMovePlayer::ms_bDefaultNoSprintingInInteriors);
		bank.AddToggle("Allow strafing when unarmed",	&CTaskPlayerOnFoot::ms_bAllowStrafingWhenUnarmed);
		bank.AddToggle("Sticky run button",				&CTaskMovePlayer::ms_bStickyRunButton);
		bank.AddToggle("Use GTAIV MP controls by default",	&CTaskMovePlayer::ms_bUseMultiPlayerControlInSinglePlayer);
		bank.AddToggle("Allow ladder climbing",			&CTaskPlayerOnFoot::ms_bAllowLadderClimbing);

		bank.AddToggle("Enable Duck And Cover",			&CTaskPlayerOnFoot::ms_bEnableDuckAndCover);
		bank.AddToggle("Enable Slope Scramble",			&CTaskPlayerOnFoot::ms_bEnableSlopeScramble);

		bank.AddSlider("Quick Switch Weapon PickUp Time", &CTaskPlayerOnFoot::ms_iQuickSwitchWeaponPickUpTime, 0, 10000, 100);

		bank.AddSlider("Max snow depth ratio for jumping",  &CTaskPlayerOnFoot::ms_fMaxSnowDepthRatioForJump, 0.0f, 1.0f, 0.05f);
		bank.AddSlider("Get in vehicle distance",		&CPlayerInfo::SCANNEARBYVEHICLES, 0.0f, 20.0f, 0.5f);
		bank.AddToggle("Render vehicle searches",		&CTaskPlayerOnFoot::ms_bRenderPlayerVehicleSearch);

		bank.AddSlider("Slide into cover cam transition dist",	&CPedIntelligence::DISTANCE_TO_START_COVER_CAM_WHEN_SLIDING , 0.0f, 20.0f, 0.1f);


		bank.AddButton("Increase Wanted Level", IncreaseWantedLevel, "Increases the wanted level for the local player");
		bank.AddButton("Decrease Wanted Level", DecreaseWantedLevel, "Decreases the wanted level for the local player");
		bank.AddButton("Wanted Level Always Zero", WantedLevelAlwaysZero, "Locks the wanted level for the local player to zero");
        bank.AddButton("Switch player to focus ped", SwitchPlayerToFocusPed, "Changes the player to the selected ped");
		bank.AddButton("Trigger world extents protection.", TriggerWorldExtentsProtection, "Performs special logic for dealing with the player wandering out of bounds.");
		bank.AddButton("Spawn a shark to the camera position.", TriggerSharkSpawnToCamera, "Force a shark to be dispatched to the camera position.");
		bank.AddButton("Trigger arrest", TriggerArrest, "Triggers an arrest on the player");

#if __PLAYER_ASSISTED_MOVEMENT
		bank.PushGroup("Assisted Movement");
		bank.AddToggle("Enable \'assisted movement\'", &CPlayerAssistedMovementControl::ms_bAssistedMovementEnabled);
		bank.AddToggle("Enable scanning for routes", &CPlayerAssistedMovementControl::ms_bRouteScanningEnabled);
		bank.AddToggle("Load all routes", &CPlayerAssistedMovementControl::ms_bLoadAllRoutes, CAssistedMovementRouteStore::RescanNow);

		bank.PushGroup("Settings");
		bank.AddToggle("Draw all routes", &CPlayerAssistedMovementControl::ms_bDrawRouteStore);

		DEV_ONLY( bank.AddToggle("Debug capsule LOS hits", &CPlayerAssistedMovementControl::ms_bDebugOutCapsuleHits); );

		bank.PopGroup();

		bank.PushGroup("Auto-Generated in Doors");
		bank.AddToggle("Enable routes in doorways", &CAssistedMovementRouteStore::ms_bAutoGenerateRoutesInDoorways);
		bank.AddSlider("Distance out from door", &CAssistedMovementRouteStore::ms_fDistOutFromDoor, 0.125f, 5.0f, 0.125f);
		bank.PopGroup();

		bank.PushGroup("Script Route Editing");
		bank.AddButton("Add waypoint at player's pos", &CPlayerAssistedMovementControl::AddPointAtPlayerPos);
		bank.AddButton("Add waypoint at camera pos", &CPlayerAssistedMovementControl::AddPointAtCameraPos);
		bank.AddButton("Clear all waypoints", &CPlayerAssistedMovementControl::ClearPoints);
		//bank.AddSlider("Modify route width", &CAssistedMovementRouteStore::ms_RouteStore[CAssistedMovementRouteStore::MAX_ROUTES-1].m_fPathWidth, 0.0f, 5.0f,0.1f);
		bank.AddButton("Dump route script snippet", &CPlayerAssistedMovementControl::DumpScript);
		bank.PopGroup();

		bank.PopGroup(); // "Assisted Movement"
#endif // __PLAYER_ASSISTED_MOVEMENT
		bank.PushGroup("Health recharge");
			bank.AddToggle("Enable health recharge", &CPlayerHealthRecharge::ms_bActivated);
			bank.AddSlider("Time after damage", &CPlayerHealthRecharge::ms_fTimeSinceDamageToStartRecharding, 0.0f, 100.0f, 0.125f);
			bank.AddSlider("Time after damage (Crouched)", &CPlayerHealthRecharge::ms_fTimeSinceDamageToStartRechardingCrouchedOrCover, 0.0f, 100.0f, 0.125f);
			bank.AddSlider("Recharge rate (ps)", &CPlayerHealthRecharge::ms_fRechargeSpeed, 0.0f, 100.0f, 0.125f);
			bank.AddSlider("Recharge rate (Crouched) (ps)", &CPlayerHealthRecharge::ms_fRechargeSpeedWhileCrouchedOrCover, 0.0f, 100.0f, 0.125f);
		bank.PopGroup();

		bank.PushGroup("Num Enemies In Combat");
		bank.AddToggle("Display num in combat", &CPlayerInfo::GetDisplayNumEnemiesInCombat());
		bank.AddToggle("Display num shooting in combat", &CPlayerInfo::GetDisplayNumEnemiesShootingInCombat());
		bank.PopGroup();

		bank.PushGroup("Cover Tracking");
		bank.AddToggle("Display cover status tracking", &CPlayerInfo::GetDisplayCoverTracking());
		bank.PopGroup();

		bank.PushGroup("Combat Loitering");
		bank.AddToggle("Debug with no enemies", &CPlayerInfo::GetDebugCombatLoitering());
		bank.AddToggle("Display combat loitering", &CPlayerInfo::GetDisplayCombatLoitering());
		bank.PopGroup();

		bank.PushGroup("Candidate Charge Positions");
		bank.AddToggle("Debug with no enemies", &CPlayerInfo::GetDebugCandidateChargeGoalPositions());
		bank.AddToggle("Debug rendering", &CPlayerInfo::GetDebugDrawCandidateChargePositions());
		bank.PopGroup();

		bank.PushGroup("Enemy Accuracy Scale Logging");
		bank.AddButton("Begin Log and Test", &CTaskCombat::EnemyAccuracyLogButtonCB);
		bank.AddSeparator("EASL_Separator_0");
		bank.AddToggle("Enable logging", &CTaskCombat::ms_EnemyAccuracyLog.GetEnabled());
		bank.AddSlider("Logging interval time (millis)", &CTaskCombat::ms_EnemyAccuracyLog.GetMeasureOutputIntervalMS(), 0, 60000, 100);
		bank.AddToggle("Enable log summary display", &CTaskCombat::ms_EnemyAccuracyLog.GetRenderDebug());
		bank.AddSlider("Display history time (millis)", &CTaskCombat::ms_EnemyAccuracyLog.GetRenderHistoryTimeMS(), 0, 60000, 100);
		bank.PopGroup();

		bank.PopGroup(); // "Player"

		bank.PushGroup("Player targeting", false);

		bank.AddButton("Add Targetable Entity", &CPlayerPedTargeting::DebugAddTargetableEntity);
		bank.AddButton("Clear Targetable Entities", &CPlayerPedTargeting::DebugClearTargetableEntities);

		CPedTargetEvaluator::AddWidgets();
		bank.AddToggle("Test analogue lockon-freeaim targeting",&CTaskPlayerOnFoot::ms_bAnalogueLockonAimControl);
		CPlayerInfo::AddAimWidgets(bank);

		bank.PopGroup();

		bank.PushGroup("Cops", false);
		bank.AddToggle("Toggle all cops",			&g_bForceAllCops);
		bank.AddToggle("Display cop searching",		&CPedDebugVisualiserMenu::ms_bDebugSearchPositions);
		bank.PopGroup();

		bank.PushGroup("New car task", false);
			bank.AddToggle("Display door and seat use",		&CComponentReservation::DISPLAY_COMPONENT_USE);
			bank.AddSeparator();
			bank.AddGroup("Go To Vehicle Door", false);
				bank.AddToggle("Use tighter turn settings", &CTaskMoveGoToVehicleDoor::ms_bUseTighterTurnSettings);
		bank.PopGroup();

		bank.PushGroup("Investigation Behaviour", false);
		bank.AddToggle("Toggle render investigation position",			&CTaskInvestigate::ms_bToggleRenderInvestigationPosition);
		bank.PopGroup();

		CExpensiveProcessDistributer::AddWidgets(bank);

		bank.PushGroup("Conversations", false);

		bank.AddSlider("TIME_BETWEEN_STATEMENTS_MIN", &CTaskChatScenario::TIME_BETWEEN_STATEMENTS_MIN, 0.0f, 100.0f, 1.0f, NullCB);
		bank.AddSlider("TIME_BETWEEN_STATEMENTS_MAX", &CTaskChatScenario::TIME_BETWEEN_STATEMENTS_MAX, 0.0f, 100.0f, 1.0f, NullCB);
		bank.AddSlider("TIME_BETWEEN_RESPONSE_MIN", &CTaskChatScenario::TIME_BETWEEN_RESPONSE_MIN, 0.0f, 100.0f, 1.0f, NullCB);
		bank.AddSlider("TIME_BETWEEN_RESPONSE_MAX", &CTaskChatScenario::TIME_BETWEEN_RESPONSE_MAX, 0.0f, 100.0f, 1.0f, NullCB);

		bank.PopGroup();

		bank.PushGroup("PedGeometryAnalyser", false);
		bank.AddToggle("Display AI Los stats", &CPedGeometryAnalyser::ms_bDisplayAILosInfo);
		bank.AddToggle("Display CanPedTargetPed cache",		&CPedGeometryAnalyser::ms_bDebugCanPedTargetPed);
		bank.AddToggle("Process CanPedTargetPed async", &CPedGeometryAnalyser::ms_bProcessCanPedTargetPedAsynchronously);
		bank.AddToggle("Process PedAcquaintance scanning async", &CPedAcquaintanceScanner::ms_bDoPedAcquaintanceScannerLosAsync);
		bank.AddSlider("CanPedTargetPed pos delta (sqr)", &CPedGeometryAnalyser::ms_fCanTargetCacheMaxPosChangeSqr, 0.0f, 64.0f, 0.1f);
		bank.PopGroup();

		bank.PushGroup("Shocking Events", false);

		bank.AddToggle("Display shocking events list",			&CShockingEventsManager::RENDER_AS_LIST);
		bank.AddToggle("Display shocking events vectormap",		&CShockingEventsManager::RENDER_VECTOR_MAP);
		bank.AddToggle("Display shocking events in 3d World",	&CShockingEventsManager::RENDER_ON_3D_SCREEN);
		bank.AddToggle("Render Event Effect Area Only",			&CShockingEventsManager::RENDER_EVENT_EFFECT_AREA);
		bank.AddToggle("Render Level Interesting",				&CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_INTERESTING);
		bank.AddToggle("Render Level AffectsOthers",			&CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_AFFECTSOTHERS);
		bank.AddToggle("Render Level PotentiallyDangerous",		&CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_POTENTIALLYDANGEROUS);
		bank.AddToggle("Render Level SeriousDanger",			&CShockingEventsManager::RENDER_SHOCK_EVENT_LEVEL_SERIOUSDANGER);
		bank.AddButton("Clear all events",						CShockingEventsManager::ClearAllEventsCB);
		bank.AddButton("Remove all random peds",				CPedPopulation::RemoveAllRandomPeds);
		bank.PopGroup();

		// Patrol group debug
		bank.PushGroup("Patrol", false);
		bank.AddToggle("Toggle render patrol routes", &CPatrolRoutes::ms_bRenderRoutes );
		bank.AddToggle("Toggle debug spawned task alertness", &CPatrolRoutes::ms_bToggleGuardsAlert);
		bank.AddToggle("Toggle render ped look at point", &CPatrolRoutes::ms_bTogglePatrolLookAt);
		bank.AddToggle("Toggle Player Stealth Level", &CPedStealth::ms_bRenderDebugStealthLevel);
		bank.AddButton("Stop threat response", &CPedDebugVisualiserMenu::StopThreatResponse);
		bank.AddSlider("Alertness reset time",&CTaskPatrol::ms_fAlertnessResetTime, 0.0f, 180.0f,1.0f);
		bank.PopGroup();

		// Setup climbing widgets
		CClimbDebug::SetupWidgets(bank);

		// Setup climbing widgets
		CDropDownDetector::SetupWidgets(bank);

		// Setup arrest widgets
		CArrestDebug::SetupWidgets(bank);

		// Set up 'N' key navigation widgets
		bank.PushGroup("Navigate Focus Ped To Cursor (Key 'N')", false);

			bank.AddCombo("Navigation Mode:", &g_iNavigateToCursorMode, NAVIGATE_TO_CURSOR_NUM_MODES, g_pNavigateToCursorModes, NullCB);
			bank.AddSlider("MoveBlendRatio", &g_fTaskMoveBlendRatio, 0.0f, 3.0f, 0.1f);
			bank.AddSlider("TargetRadius", &g_fTargetRadius, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance", &CTaskMoveGoToPointAndStandStill::ms_fSlowDownDistance, 0.0f, 10.0f, 0.1f);
			bank.AddToggle("Stop exactly", &g_bNavMeshStopExactly);
			bank.AddToggle("Use target heading", &g_bNavMeshUseTargetHeading);
			bank.AddSlider("Target heading", &g_fNavMeshTargetHeading, -PI, PI, 0.1f);
			bank.AddSlider("Max adjustment to start position", &g_fNavMeshMaxDistanceToAdjustStartPosition, 0.0f, TRequestPathStruct::ms_fDefaultMaxDistanceToAdjustPathEndPoints, 0.25f);
			bank.AddToggle("Don't adjust target position", &g_bNavMeshDontAdjustTargetPosition);
			bank.AddToggle("Allowt to navigate up steep polygons", &g_bNavMeshAllowToNavigateUpSteepPolygons);

			bank.AddSeparator();
			bank.AddToggle("bUseFreeSpaceRefVal", &CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_bUseFreeSpaceRefVal);
			bank.AddToggle("bFavourFreeSpaceGreaterThanRefVal", &CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_bFavourFreeSpaceGreaterThanRefVal);
			bank.AddSlider("iFreeSpaceRefVal", &CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_iFreeSpaceRefVal, 0, MAX_FREE_SPACE_AROUND_POLY_VALUE, 1);
			bank.AddToggle("bUseDirectionalCover (dbg camera pos)", &CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_bUseDirectionalCover);
			bank.AddToggle("bGoToPointAnyMeans", &CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_bGoToPointAnyMeans);

		bank.PopGroup();


		bank.PushGroup("Misc", false);
#if __CATCH_EVENT_LEAK
			bank.AddToggle("url:bugstar:1371503 - help catch EVENT_SWITCH_2_NM_TASK leak", &CEventSwitch2NM::ms_bCatchEventLeak);
#endif
#if __LOG_MOTIONPED_RESTART_REASONS
			bank.AddToggle("Log MotionPed Restart Reasons", &CTaskMotionPed::ms_LogRestartReasons);
#endif
			bank.AddToggle("Allow warping on routes", &CPedIntelligence::ms_bAllowTeleportingWhenRoutesTimeOut);
			bank.AddToggle("Prevent wandering peds crossing roads", &CTaskWander::ms_bPreventCrossingRoads);
			bank.AddToggle("Promote wandering peds crossing roads", &CTaskWander::ms_bPromoteCrossingRoads);
			bank.AddToggle("Enable chain crossing", &CTaskWander::ms_bEnableChainCrossing);
			bank.AddToggle("Enable pavement arrival check", &CTaskWander::ms_bEnablePavementArrivalCheck);
			bank.AddToggle("Enable fail wander crossing", &CTaskWander::ms_bEnableWanderFailCrossing);
			bank.AddToggle("Enable no pavement crossing", &CTaskWander::ms_bEnableArriveNoPavementCrossing);

			bank.AddToggle("Switch on route-round-car test", &CPedDebugVisualiserMenu::ms_bRouteRoundCarTest);
			bank.AddButton("ShapeTestError_PS3", ShapeTestError_PS3);
			bank.AddButton("Test Dodgy Run-Start", TestDodgyRunStart);

			bank.AddToggle("Debug CEventStaticCountReachedMax climb reponse", &CEventStaticCountReachedMax::ms_bDebugClimbAttempts);

	#if __DEV
			bank.AddButton("FindEntityUnderPlayer", FindEntityUnderPlayer);
	#endif
			bank.AddButton("Recalc AI bound for closest obj", OnRecalcAIBoundForObject);

			bank.AddToggle("Disable evasive dives", &ms_menuFlags.m_bDisableEvasiveDives );
			bank.AddToggle("Disable flinch reactions", &ms_menuFlags.m_bDisableFlinchReactions );
			bank.AddToggle("Log ped speech", &ms_menuFlags.m_bLogPedSpeech );

			bank.AddButton("Print out task sizes", CTaskClassInfoManager::PrintTaskNamesAndSizes);
			bank.AddButton("Print out large task sizes", CTaskClassInfoManager::PrintOnlyLargeTaskNamesAndSizes);
			bank.AddButton("Print out event sizes", CEventNames::PrintEventNamesAndSizes);

			bank.AddToggle("DisableTalkingThePlayerDown", &CTaskPlayerOnFoot::ms_bDisableTalkingThePlayerDown);

		bank.PopGroup();

		CTaskParachute::InitWidgets();

		camCinematicMountedCamera::AddWidgets();

#if ENABLE_JETPACK
		CTaskJetpack::InitWidgets();
#endif
		CMoveFollowEntityOffsetDebug::InitWidgets();
		CClimbLadderDebug::InitWidgets();
		CCombatDebug::InitWidgets();
		CCombatMeleeDebug::InitWidgets();

#if __DEV
		CTaskAgitated::InitWidgets();
#endif

		CFleeDebug::InitWidgets();
		CPedAILodManager::AddDebugWidgets();
		CEventDebug::InitWidgets();
		CTaskAimGun::InitWidgets();
		CTaskMoVEScripted::InitWidgets();
		CTaskMotionTennis::InitWidgets();
		CVehicleComponentVisualiser::InitWidgets();
		CTaskScriptedAnimation::InitWidgets();

		CPedPerception::InitWidgets();

		CAirDefenceManager::InitWidgets();

		CMotionTaskDataManager::InitWidgets();
		CPedNavCapabilityInfoManager::AddWidgets(bank);

		bank.PushGroup("Motion", false);
		{
			bank.AddToggle("Use New Locomotion", &ms_bUseNewLocomotionTask);
			bank.AddText("Apply On Foot clip set", &ms_onFootClipSetText[0], 60, false, ApplyOnFootClipSetToSelectedPed );

			bank.PushGroup("Alternate walk anim");
			{
				m_alternateAnimSelector.AddWidgets(ms_pBank);
				bank.AddSlider("blend duration", &m_alternateAnimBlendDuration, 0.0f, 10.0f, 0.0001f);
				bank.AddToggle("Loop", &m_alternateAnimLooped);
				bank.AddButton("Start alternate walk anim", StartAlternateWalkAnimSelectedPed);
				bank.AddButton("End alternate walk anim", EndAlternateWalkAnimSelectedPed);
			}
			bank.PopGroup();
		}
		bank.PopGroup();

		bank.PushGroup("ActionMode", false);

		bank.AddButton("Toggle Action Mode on/off", ToggleActionMode);
		bank.AddButton("Toggle Movement mode override on/off", ToggleMovementModeOverride);
		bank.AddToggle("Action Mode Run On/Off", &m_actionModeRun);

		CPedIntelligence::AddWidgets(bank);

		bank.PopGroup();
	}
}

void CPedDebugVisualiserMenu::ApplyOnFootClipSetToSelectedPed()
{
	CPed* pPed = GetFocusPed();
	if (pPed)
	{
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		//verify that this is a valid clip set
		fwClipSet* pSet = fwClipSetManager::GetClipSet(fwMvClipSetId(ms_onFootClipSetText));

		if (pSet && pTask)
		{
			pSet->StreamIn_DEPRECATED();
			pTask->SetOnFootClipSet(fwMvClipSetId(ms_onFootClipSetText));
		}
	}
}

void CPedDebugVisualiserMenu::StartAlternateWalkAnimSelectedPed()
{
	CPed* pPed = GetFocusPed();
	if (pPed)
	{
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
		{
			CTaskMotionPed* pTaskPed = static_cast<CTaskMotionPed*>(pTask);

			atHashWithStringNotFinal dictHash(m_alternateAnimSelector.GetSelectedClipDictionary());
			atHashWithStringNotFinal animHash(m_alternateAnimSelector.GetSelectedClipName());

			pTaskPed->SetAlternateClip(CTaskMotionBase::ACT_Walk, CTaskMotionBase::sAlternateClip(dictHash, animHash, m_alternateAnimBlendDuration), m_alternateAnimLooped);
		}
	}
}

void CPedDebugVisualiserMenu::ToggleActionMode()
{
	CPed* pPed = GetFocusPed() ? GetFocusPed() : CGameWorld::FindLocalPlayer();
	if (pPed)
	{
#if __BANK
		if(pPed->IsActionModeReasonActive(CPed::AME_Debug))
		{
			pPed->SetUsingActionMode(false, CPed::AME_Debug, -1, m_actionModeRun);
		}
		else
		{
			pPed->SetUsingActionMode(true, CPed::AME_Debug);
		}
#endif
	}
}

void CPedDebugVisualiserMenu::ToggleMovementModeOverride()
{
	CPed* pPed = GetFocusPed() ? GetFocusPed() : CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		if(pPed->IsMovementModeEnabled())
		{
			pPed->EnableMovementMode(false);
		}
		else
		{
			pPed->EnableMovementMode(true);
		}
	}
}

void CPedDebugVisualiserMenu::EndAlternateWalkAnimSelectedPed()
{
	CPed* pPed = GetFocusPed();
	if (pPed)
	{
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();

		if (pTask->GetTaskType()==CTaskTypes::TASK_MOTION_PED)
		{
			CTaskMotionPed* pTaskPed = static_cast<CTaskMotionPed*>(pTask);

			pTaskPed->StopAlternateClip(CTaskMotionBase::ACT_Walk, m_alternateAnimBlendDuration);
		}
	}
}

void CPedDebugVisualiserMenu::ShutdownBank()
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
	ms_pCreateButton = NULL;
}

void CPedDebugVisualiserMenu::StopThreatResponse()
{
	CPed::Pool* pPedPool = CPed::GetPool();
	aiAssert(pPedPool);
	CPed* pPed = NULL;
	for (s32 i = 0; i < pPedPool->GetSize(); ++i)
	{
		pPed = pPedPool->GetSlot(i);
		if (pPed)
		{
			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE);
			bool bClearTasks = false;
			if (pTask)
			{
				bClearTasks = true;
			}
			pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GO_TO_POINT_AIMING);
			if (pTask)
			{
				bClearTasks = true;
			}
			pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE);
			if (pTask)
			{
				bClearTasks = true;
			}
			pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_TO_DEAD_PED);
			if (pTask)
			{
				bClearTasks = true;
			}
			pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WATCH_INVESTIGATION);
			if (pTask)
			{
				bClearTasks = true;
			}
			pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PATROL);
			if (pTask)
			{
				pPed->GetPedIntelligence()->SetAlertnessState(CPedIntelligence::AS_NotAlert);
			}
			if (bClearTasks)
			{
				pPed->GetPedIntelligence()->SetAlertnessState(CPedIntelligence::AS_NotAlert);
				pPed->GetPedIntelligence()->ClearTaskEventResponse();
				pPed->GetPedIntelligence()->FlushEvents();
			}
		}
	}
}

void CPedDebugVisualiserMenu::Shutdown()
{
}



void CPedDebugVisualiserMenu::Process()
{
	if(ms_FlushLogTime <= fwTimer::GetTimeInMilliseconds())
	{
		CAILogManager::GetLog().Flush();

		static dev_u32 FLUSH_LOG_FREQUENCY = 1000;
		ms_FlushLogTime = fwTimer::GetTimeInMilliseconds() + FLUSH_LOG_FREQUENCY;
	}

	// Reset peds combat, investigation and reaction tasks
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_0, KEYBOARD_MODE_DEBUG))
	{
		StopThreatResponse();
	}

	int iMode = (int)CPedDebugVisualiser::eDisplayDebugInfo;
	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_A, KEYBOARD_MODE_DEBUG, "AI display increment"))
	{
		iMode++;
		if(iMode==CPedDebugVisualiser::eNumModes)
			iMode=CPedDebugVisualiser::eOff;
	}
	else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_A, KEYBOARD_MODE_DEBUG_SHIFT, "AI display decrement"))
	{
		iMode--;
		if(iMode<CPedDebugVisualiser::eOff)
			iMode=CPedDebugVisualiser::eNumModes-1;
	}
	else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_A, KEYBOARD_MODE_DEBUG_CTRL, "Toggle AI display off"))
	{
		iMode = 0;
	}
	else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_A, KEYBOARD_MODE_DEBUG_CNTRL_SHIFT_ALT, "Damage Display"))
	{
		if(iMode == CPedDebugVisualiser::eDamage)
		{
			if(!CPedDebugVisualiser::GetDetailedDamageDisplay())
			{
				CPedDebugVisualiser::SetDetailedDamageDisplay(true);
			}
			else
			{
				iMode = 0;
			}
		}
		else
		{
			iMode = CPedDebugVisualiser::eDamage;
			CPedDebugVisualiser::SetDetailedDamageDisplay(false);
		}
	}

	if(iMode != CPedDebugVisualiser::eDisplayDebugInfo)
	{
		ms_menuFlags.m_bDisplayDamageRecords = false;
		CPedDebugVisualiser::eDisplayDebugInfo = (CPedDebugVisualiser::ePedDebugVisMode)iMode;
		Color32 txtCol(255,255,0,255);
		static char text[256];
		sprintf(text, "AI Display : %s", pPedDebuggingText[(int)CPedDebugVisualiser::eDisplayDebugInfo]);
		CMessages::AddMessage(text, -1,
			1000, true, false, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false);
	}

	if(iMode == CPedDebugVisualiser::eTasksFullDebugging)
	{
		CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

		if (!pEntity || !pEntity->GetIsTypePed())
		{
			pEntity = FindPlayerPed();
		}

		if (pEntity)
		{
			CAILogManager::PrintDebugInfosForPedToLog(*static_cast<CPed*>(pEntity));
		}
	}

	if(ms_bRouteRoundCarTest)
	{
		ProcessRouteRoundCarTest();
	}

	if(CPedDebugVisualiserMenu::ms_bHeadIKEnable)
	{
		ProcessHeadIK();
	}

	if(CPedDebugVisualiserMenu::ms_bLookIKEnable)
	{
		ProcessLookIK();
	}

	if(CPedDebugVisualiserMenu::ms_bUseLeftArmIK || CPedDebugVisualiserMenu::ms_bUseRightArmIK)
	{
		ProcessArmIK();
	}

	CVehicleDebug::ProcessPersonalVehicle();
}

void CPedDebugVisualiserMenu::Render()
{
	TUNE_GROUP_FLOAT(AI_DEBUG_SCALE, fDebugTextScale, 1.0f, 0.5f, 5.0f, 0.01f);

	//! DMKH. On Next Gen platforms, debug text is too small to read clearly. So we use 720 as a base, and scale in proportion to that based on our new screen height. E.g. 1080
	//! is 50% more. Need to scale text and spacing to get alignment all matching as well.
	float fBaseHeightRatio = grcFont::GetCurrent().GetHeight() / 720.0f;

	float fNewHeightRatio = (float)grcFont::GetCurrent().GetHeight() / (float)GRCDEVICE.GetHeight();

	float fFontScale = 1.0f / (fNewHeightRatio/fBaseHeightRatio);

	float fOldScaleX = grcDebugDraw::GetDrawLabelScaleX();
	float fOldScaleY = grcDebugDraw::GetDrawLabelScaleY();

	grcDebugDraw::SetDrawLabelScaleX(fFontScale * fDebugTextScale);
	grcDebugDraw::SetDrawLabelScaleY(fFontScale * fDebugTextScale);

	s32 iOldScreenHeight = grcDebugDraw::GetScreenSpaceTextHeight();

	float fNewSpaceTextHeight = ceil((float)iOldScreenHeight * fFontScale * fDebugTextScale);
	grcDebugDraw::SetScreenSpaceTextHeight( (s32)fNewSpaceTextHeight );

	if(CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayDamageRecords)
	{
		DisplayDamageRecords();
	}

	if (CPedDebugVisualiserMenu::ms_menuFlags.m_bRenderInitialSpawnPoint)
	{
		RenderInitialSpawnPointOfSelectedPed();
	}

#if __DEV
	if(ms_menuFlags.m_bDisplayPedSpatialArray)
	{
		CPed::GetSpatialArray().DebugDraw();
	}
	if(ms_menuFlags.m_bDisplayVehicleSpatialArray)
	{
		CVehicle::GetSpatialArray().DebugDraw();
	}
#endif	// __DEV

	if( !CPedDebugVisualiser::GetDebugDisplay() && !CPedPerception::ms_bDrawVisualField &&
		!CPedDebugVisualiserMenu::IsVisualisingDefensiveAreas() && !CPedDebugVisualiserMenu::IsVisualisingSecondaryDefensiveAreas() )
	{

	}
	else
	{

		Color32 iColRed(0xff,0,0,0xff);
		Color32 iColGreen(0,0xff,0,0xff);
		Color32 iColBlue(0,0,0xff,0xff);
		Color32 iColWhite(0xff,0xff,0xff,0xff);

		if (ms_menuFlags.m_bDisplayAddresses)
			DisplayNearbyPedIDs();

		for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
		{
			CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
			if(pEnt)
			{
				Vector3 vecPosition = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition());
				vecPosition.z += 1.0f;
				grcDebugDraw::Line(vecPosition - Vector3(0.0f,0.0f,0.4f), vecPosition + Vector3(0.0f,0.0f,0.4f), iColRed);
				grcDebugDraw::Line(vecPosition - Vector3(0.0f,0.4f,0.0f), vecPosition + Vector3(0.0f,0.4f,0.0f), iColGreen);
				grcDebugDraw::Line(vecPosition - Vector3(0.4f,0.0f,0.0f), vecPosition + Vector3(0.4f,0.0f,0.0f), iColBlue);
			}
		}

		CPedDebugVisualiser debugVisualiser;
		debugVisualiser.VisualiseAll();
	}

	//! Reset debug draw scaling.
	grcDebugDraw::SetDrawLabelScaleX(fOldScaleX);
	grcDebugDraw::SetDrawLabelScaleY(fOldScaleY);

	grcDebugDraw::SetScreenSpaceTextHeight( iOldScreenHeight );
}

void CPedDebugVisualiserMenu::OnChangeFocusPedCB(CEntity * pEntity)
{
	if(pEntity && !pEntity->GetIsTypePed())
		pEntity = NULL;

	//------------------------------------------------------------

#if __DEV
	CPed * pPed = (CPed*)pEntity;

	m_bAssertIfFocusPedFailsToFindNavmeshRoute = false;

	if(pPed)
	{
		m_bAssertIfFocusPedFailsToFindNavmeshRoute = pPed->GetPedIntelligence()->m_bAssertIfRouteNotFound;
	}
#endif // __DEV
}

void CPedDebugVisualiserMenu::DisplayPedInfo(const CPed& ped)
{
	if (ms_menuFlags.m_bDisplayTasks)
		m_debugVis.VisualiseTasks(ped);

	CPedVariationDebug::Visualise(ped);
}

void CPedDebugVisualiserMenu::DisplayGroupInfo()
{
	// get the group that the player is in
	CPed* pPlayer = FindPlayerPed();
	int iGroupID = -1;
	if (CPedGroups::IsGroupLeader(pPlayer))
	{
		// get existing group
		for (int i = 0; i < CPedGroups::MAX_NUM_GROUPS; i++)
		{
			if (CPedGroups::ms_groups[i].GetGroupMembership()->IsLeader(pPlayer))
			{
				iGroupID = i;
				break;
			}
		}
	}

	Color32 iColWhite(0xff,0xff,0xff,0xff);

	// go through each ped in the group and display their destination point for formations.
	CPedGroup& group = CPedGroups::ms_groups[iGroupID];
	Vector3 vSeekPos;
	CPed* pPed;
	for (int i = 0; i < CPedGroupMembership::MAX_NUM_MEMBERS; i++)
	{
		pPed = group.GetGroupMembership()->GetMember(i);
		if (pPed && pPed != pPlayer)
		{
			DisplayPedInfo(*pPed);

			const CPedFormation * pFormation = group.GetFormation();
			Assert(pFormation);

			const CPedPositioning & pedPositioning = pFormation->GetPedPositioning(i);
			Vector3 vPos = pedPositioning.GetPosition();
			grcDebugDraw::Sphere(vPos, 0.1f, iColWhite);

			/*
			const Vector2& vOffset(CTaskComplexFollowLeaderInFormation::ms_offsets.GetOffSet(i));
			CEntitySeekPosCalculatorXYOffset seekPosCalculator(Vector3(vOffset.x,vOffset.y,0));
			seekPosCalculator.ComputeEntitySeekPos(*pPed, *pPlayer, vSeekPos);
			grcDebugDraw::Sphere(vSeekPos.x, vSeekPos.y, vSeekPos.z, 0.1f, iColWhite);
			*/
		}
	}
}

void CPedDebugVisualiserMenu::ChangeFocusToPlayer()
{
	CPed * pPlayerPed = FindPlayerPed();
	if( pPlayerPed )
	{
		CDebugScene::FocusEntities_Clear();
		CDebugScene::FocusEntities_Set(pPlayerPed, 0);
	}
}

void CPedDebugVisualiserMenu::SetSelectedConfigFlagsOnFocusPed()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (!pEntity || !pEntity->GetIsTypePed())
	{
		pEntity = FindPlayerPed();
	}

	if (pEntity)
	{
		for(int i = 0; i < PARSER_ENUM(ePedConfigFlags).m_NumEnums; i++)
		{
			if (ms_DebugPedConfigFlagsBitSet.BitSet().IsSet(i))
			{
				static_cast<CPed*>(pEntity)->SetPedConfigFlag((ePedConfigFlags)i, true);
			}
		}
	}
}

void CPedDebugVisualiserMenu::ClearSelectedConfigFlagsOnFocusPed()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (!pEntity || !pEntity->GetIsTypePed())
	{
		pEntity = FindPlayerPed();
	}

	if (pEntity)
	{
		for(int i = 0; i < PARSER_ENUM(ePedConfigFlags).m_NumEnums; i++)
		{
			if (ms_DebugPedConfigFlagsBitSet.BitSet().IsSet(i))
			{
				static_cast<CPed*>(pEntity)->SetPedConfigFlag((ePedConfigFlags)i, false);
			}
		}
	}
}

void CPedDebugVisualiserMenu::SetSelectedPostPhysicsResetFlagsOnFocusPed()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (!pEntity || !pEntity->GetIsTypePed())
	{
		pEntity = FindPlayerPed();
	}

	if (pEntity)
	{
		for(int i = 0; i < PARSER_ENUM(ePedResetFlags).m_NumEnums; i++)
		{
			if (ms_DebugPedPostPhysicsResetFlagsBitSet.BitSet().IsSet(i))
			{
				static_cast<CPed*>(pEntity)->SetPedResetFlag((ePedResetFlags)i, true);
			}
		}
		ms_bDebugPostPhysicsResetFlagActive = true;
	}
}

void CPedDebugVisualiserMenu::ClearSelectedPostPhysicsResetFlagsOnFocusPed()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);

	if (!pEntity || !pEntity->GetIsTypePed())
	{
		pEntity = FindPlayerPed();
	}

	if (pEntity)
	{
		ms_bDebugPostPhysicsResetFlagActive = false;
	}
}

void CPedDebugVisualiserMenu::SpewRelationShipGroupsToTTY()
{
	aiDisplayf("RELATIONSHIP GROUP SPEW");
	s32 iNumRelationShipGroups = 0;

	s32 iPoolSize = CRelationshipGroup::GetPool()->GetSize();
	for( s32 i = 0; i < iPoolSize; i++ )
	{
		CRelationshipGroup* pGroup = CRelationshipGroup::GetPool()->GetSlot(i);
		if (pGroup)
		{
			aiDisplayf("%s", pGroup->GetName().GetCStr());
			++iNumRelationShipGroups;
		}
	}
	aiDisplayf("Found %i relationship groups (EXCLUDES STATIC GROUPS)", iNumRelationShipGroups);
}

void CPedDebugVisualiserMenu::AddRelationShipGroup()
{
	atHashString hashString(ms_addRemoveSetRelGroupText);
	CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(hashString);
	if (!pGroup)
	{
		pGroup = CRelationshipManager::AddRelationshipGroup(hashString, RT_mission);
	}
	else
	{
		aiAssertf(0, "Relationship group %s already exists", hashString.GetCStr());
	}
}

void CPedDebugVisualiserMenu::RemoveRelationShipGroup()
{
	atHashString hashString(ms_addRemoveSetRelGroupText);
	CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(hashString);
	if (pGroup)
	{
		CRelationshipManager::RemoveRelationshipGroup(pGroup);
	}
	else
	{
		aiAssertf(0, "Relationship group %s doesn't exists", hashString.GetCStr());
	}
}

void CPedDebugVisualiserMenu::SetRelationShipGroup()
{
	CEntity* pEntity = CDebugScene::FocusEntities_Get(0);
	if (pEntity && pEntity->GetIsTypePed())
	{
		atHashString hashString(ms_addRemoveSetRelGroupText);
		CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(hashString);
		if (pGroup)
		{
			CPed* pPed = static_cast<CPed*>(pEntity);
			pPed->GetPedIntelligence()->SetRelationshipGroup(pGroup);

			if (pGroup->GetShouldBlipPeds())
			{
				ped_commands::SetRelationshipGroupBlipForPed(pPed, true);
			}
			else
			{
				ped_commands::SetRelationshipGroupBlipForPed(pPed, false);
			}
		}
		else
		{
			aiAssertf(0, "Relationship group %s doesn't exists", hashString.GetCStr());
		}
	}
	else
	{
		aiAssertf(0, "No focus ped set");
	}
}

void CPedDebugVisualiserMenu::SetBlipPedsInRelationshipGroup()
{
	atHashString hashString(ms_addRemoveSetRelGroupText);
	CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(hashString);
	if (pGroup)
	{
		pGroup->SetShouldBlipPeds(ms_BlipPedsInRelGroup);
		u32 IndexOfGroupThatHasJustBeenModified = pGroup->GetIndex();

		// Loop through entire ped pool - fwPool<CPed>::GetSlot() already ignores cached peds
		CPed::Pool* pPedPool = CPed::GetPool();
		const s32 pedPoolSize = pPedPool->GetSize();
		for(s32 i=0; i < pedPoolSize; i++)
		{
			CPed *pPedToCheck = pPedPool->GetSlot(i);
			if (pPedToCheck && !pPedToCheck->IsDead() && !pPedToCheck->IsInjured() && pPedToCheck->GetPedIntelligence())
			{
				const CRelationshipGroup* pRelGroupForThisPed = pPedToCheck->GetPedIntelligence()->GetRelationshipGroup();
				if (pRelGroupForThisPed->GetIndex() == IndexOfGroupThatHasJustBeenModified)
				{
					ped_commands::SetRelationshipGroupBlipForPed(pPedToCheck, ms_BlipPedsInRelGroup);
				}
			}
		}
	}
	else
	{
		aiAssertf(0, "Relationship group %s doesn't exists", hashString.GetCStr());
	}
}

void CPedDebugVisualiserMenu::SetRelationshipBetweenGroups()
{
	atHashString hashString1(ms_RelGroup1Text);
	atHashString hashString2(ms_RelGroup2Text);

	CRelationshipGroup* pRelGroup1 = CRelationshipManager::FindRelationshipGroup(hashString1);
	if (!pRelGroup1)
	{
		aiAssertf(0, "Relationship group %s doesn't exist", hashString1.GetCStr());
	}
	else
	{
		CRelationshipGroup* pRelGroup2 = CRelationshipManager::FindRelationshipGroup(hashString2);
		if (!pRelGroup2)
		{
			aiAssertf(0, "Relationship group %s doesn't exist", hashString2.GetCStr());
		}
		else
		{
			pRelGroup1->SetRelationship((eRelationType)m_iRelationshipsComboIndex, pRelGroup2);
		}
	}
}

void CPedDebugVisualiserMenu::GetRelationshipBetweenGroups()
{
	atHashString hashString1(ms_RelGroup1Text);
	atHashString hashString2(ms_RelGroup2Text);

	CRelationshipGroup* pRelGroup1 = CRelationshipManager::FindRelationshipGroup(hashString1);
	if (!pRelGroup1)
	{
		aiAssertf(0, "Relationship group %s doesn't exist", hashString1.GetCStr());
	}
	else
	{
		CRelationshipGroup* pRelGroup2 = CRelationshipManager::FindRelationshipGroup(hashString2);
		if (!pRelGroup2)
		{
			aiAssertf(0, "Relationship group %s doesn't exist", hashString2.GetCStr());
		}
		else
		{
			s32 iRelationType = (s32)pRelGroup1->GetRelationship(pRelGroup2->GetIndex());
			if (iRelationType >= 0 && iRelationType < MAX_NUM_ACQUAINTANCE_TYPES)
			{
				aiDisplayf("Relationship between group %s and group %s is %s", hashString1.GetCStr(), hashString2.GetCStr(), spRelationshipText[iRelationType]);
			}
			else if (iRelationType == ACQUAINTANCE_TYPE_INVALID)
			{
				aiDisplayf("Relationship between group %s and group %s is ACQUAINTANCE_TYPE_INVALID", hashString1.GetCStr(), hashString2.GetCStr());
			}
		}
	}
}

void CPedDebugVisualiserMenu::ToggleFriendlyFire()
{
	CNetwork::SetFriendlyFireAllowed(!CNetwork::IsFriendlyFireAllowed());
}

void CPedDebugVisualiserMenu::ChangeFocusToClosestToPlayer()
{
	CPed * pPlayerPed = FindPlayerPed();
	if( pPlayerPed )
	{
		// Find the ped closest to the player.
		CEntity* pClosestPedOrDummyPedToPlayer = NULL;
		{
			Vec3V playerPos = pPlayerPed->GetTransform().GetPosition();
			float closestDistanceToPlayer = 10000000.0f;

			// Go through the ped pool and check the distance from each
			// one to the player.
			CPed::Pool* pPedPool = CPed::GetPool();
			Assert(pPedPool);
			s32 k0 = pPedPool->GetSize();
			CPed* pPed = NULL;
			while(k0--)
			{
				pPed = pPedPool->GetSlot(k0);
				if(pPed && (pPed != pPlayerPed))
				{
					const float distance = Mag(pPed->GetTransform().GetPosition() - playerPos).Getf();
					if( distance < closestDistanceToPlayer )
					{
						closestDistanceToPlayer = distance;
						pClosestPedOrDummyPedToPlayer = pPed;
					}
				}
			}
		}

		if(pClosestPedOrDummyPedToPlayer)
		{
			CDebugScene::FocusEntities_Clear();
			CDebugScene::FocusEntities_Set(pClosestPedOrDummyPedToPlayer, 0);
		}
	}
}


void CPedDebugVisualiserMenu::ChangeFocusForwards()
{
	ChangeFocus(1);
}
void CPedDebugVisualiserMenu::ChangeFocusBack()
{
	ChangeFocus(-1);
}

// note: use GetIsTypePed() to test for pedness of entity
void CPedDebugVisualiserMenu::ChangeFocus(int iDir)
{
	CPed * pPlayerPed = FindPlayerPed();
	CPed::Pool * pPool = CPed::GetPool();

	s32 iStartIndex = 0;

	if(CDebugScene::FocusEntities_Get(0))
	{
		for(int i=0; i<pPool->GetSize(); i++)
		{
			CPed * pPed = pPool->GetSlot(i);

			if(CDebugScene::FocusEntities_Get(0) && pPed == CDebugScene::FocusEntities_Get(0))
			{
				iStartIndex = i;
			}
		}
	}

	s32 iIndex = iStartIndex;

	CPed * pPed = NULL;

	int iSize = pPool->GetSize()+1;
	while(iSize)
	{
		iIndex+=iDir;
		if(iIndex >= pPool->GetSize())
			iIndex = 0;
		if(iIndex < 0)
			iIndex = pPool->GetSize()-1;

		pPed = pPool->GetSlot(iIndex);
		if(pPed != pPlayerPed)
		{
			pPed = pPool->GetSlot(iIndex);
		}

		if(pPed)
			break;

		iSize--;
	}

	// No peds in pool, except for player ped ?
	if(!pPed)
		return;

	CDebugScene::FocusEntities_Clear();
	CDebugScene::FocusEntities_Set(pPed, 0);
}


// returns the next ped to focus on depending on the search direction
// and the ped that is currently in 'focus'
CPed* CPedDebugVisualiserMenu::FindPed(const CPed* UNUSED_PARAM(pIgnorePed), const Vector3& UNUSED_PARAM(vCurrentDirection), eSearchType UNUSED_PARAM(search))
{
	return NULL;
}

//void CPedDebugVisualiserMenu::CompareInDirection(const CEntity& ped, Real fCurrentHeading, eSearchDirection searchDirection,

// the ID is just the pointer at the mo'... think this should suffice?
void CPedDebugVisualiserMenu::DisplayNearbyPedIDs()
{
	CPed::Pool * pPool = CPed::GetPool();
	CPed* pPed;
	s32 k = pPool->GetSize();

	// Set origin to be the debug cam, or the player..
#if __DEV
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 origin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#else
	Vector3 origin = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#endif

	while(k--)
	{
		pPed = pPool->GetSlot(k);
		if(pPed && pPed != CDebugScene::FocusEntities_Get(0))
		{
			Vector3 vec = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - origin;
			if( ( CPedDebugVisualiserMenu::IsDisplayingOnlyForFocus() && CDebugScene::FocusEntities_IsInGroup(pPed) ) ||
				( !CPedDebugVisualiserMenu::IsDisplayingOnlyForFocus() && vec.Mag2() < CPedDebugVisualiser::ms_fVisualiseRange*CPedDebugVisualiser::ms_fVisualiseRange ) )
			{
				DisplayPedID(*pPed);
			}
		}
	}

	// display focus ped last so we can always read the text
	if( CDebugScene::FocusEntities_Get(0) &&
		CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
	{
		CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));

		DisplayPedID(*pFocusPed);
	}
}

void CPedDebugVisualiserMenu::DisplayPedID(const CPed& ped)
{
	Vector3 WorldCoors;
	u8 iAlpha = 255;
	if (!GetCoorsAndAlphaForDebugText(ped, WorldCoors, iAlpha))
	{
		return;
	}

	int iNoOfTexts = 0;

	Vector3 vWorldPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	char debugText[20];

	sprintf(debugText, "%p", &ped);

	Color32 colour = CRGBA(255,255,255,iAlpha);

	grcDebugDraw::Text( vWorldPos, colour, 0, iNoOfTexts*grcDebugDraw::GetScreenSpaceTextHeight(), debugText);
}

void CPedDebugVisualiserMenu::DisplayDamageRecords()
{
	// Set origin to be the debug cam, or the player..
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	const Vector3 origin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());

	g_vDebugVisOrigin = origin;

	atArray<CEntity*> entityList;

	Vec3V vIteratorPos = VECTOR3_TO_VEC3V(origin);
	CEntityIterator entityIterator( IteratePeds|IterateVehicles|IterateObjects, NULL, &vIteratorPos, CPedDebugVisualiser::ms_fVisualiseRange*CPedDebugVisualiser::ms_fVisualiseRange );
	CEntity* pEntity = entityIterator.GetNext();
	while(pEntity)
	{
		entityList.PushAndGrow(pEntity);
		pEntity = entityIterator.GetNext();
	}

	entityList.QSort(0, -1, CompareEntityFunc);

	CPedDebugVisualiser debugVisualiser;

	int e;
	for(e=0; e<entityList.size(); e++)
	{
		pEntity = entityList[e];
		switch(pEntity->GetType())
		{
		case ENTITY_TYPE_PED:
			debugVisualiser.VisualiseDamage(*(CPed*)pEntity, true);
			break;
		}
	}
}

void CPedDebugVisualiserMenu::RenderInitialSpawnPointOfSelectedPed(void)
{
	// get the selected ped and draw
	CPed * pFocus = CPedDebugVisualiserMenu::GetFocusPed();
	if(pFocus)
	{
		Vec3V spawnPos = pFocus->m_vecInitialSpawnPosition;
		Vec3V currPedPos = pFocus->GetTransform().GetPosition();
		grcDebugDraw::Sphere(spawnPos, 0.25f, Color_red);
		grcDebugDraw::Line(currPedPos, spawnPos, Color_aquamarine3);
	}
}

void CPedDebugVisualiserMenu::ProcessHeadIK()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			s32 flags = 0;
			flags |= ms_bSlowTurnRate ? LF_SLOW_TURN_RATE: 0;
			flags |= ms_bFastTurnRate ? LF_FAST_TURN_RATE: 0;
			flags |= ms_bWideYawLimit ? LF_WIDE_YAW_LIMIT: 0;
			flags |= ms_bWidestYawLimit ? LF_WIDEST_YAW_LIMIT: 0;
			flags |= ms_bWidePitchLimit ? LF_WIDE_PITCH_LIMIT: 0;
			flags |= ms_bWidestPitchLimit ? LF_WIDEST_PITCH_LIMIT: 0;
			flags |= ms_bNarrowYawLimit ? LF_NARROW_YAW_LIMIT: 0;
			flags |= ms_bNarrowestYawLimit ? LF_NARROWEST_YAW_LIMIT: 0;
			flags |= ms_bNarrowPitchLimit ? LF_NARROW_PITCH_LIMIT: 0;
			flags |= ms_bNarrowestPitchLimit ? LF_NARROWEST_PITCH_LIMIT: 0;
			flags |= ms_bWhileNotInFOV ? LF_WHILE_NOT_IN_FOV: 0;
			flags |= ms_bUseEyesOnly ? LF_USE_EYES_ONLY: 0;

			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			Vector3 targetPoint;
			if (ms_bHeadIKUseWorldSpace)
			{
				targetPoint.Set(CPedDebugVisualiserMenu::ms_vHeadIKTarget);
				pFocusPed->GetIkManager().LookAt(0, NULL, ms_fHeadIKTime < 0 ? -1 :(s32)(ms_fHeadIKTime*1000.0f), BONETAG_INVALID, &targetPoint, flags, (s32)(ms_fHeadIKBlendInTime*1000.0f), (s32)(ms_fHeadIKBlendOutTime*1000.0f));
			}
			else
			{
				targetPoint.Set(pEnt->TransformIntoWorldSpace(CPedDebugVisualiserMenu::ms_vHeadIKTarget));
				pFocusPed->GetIkManager().LookAt(0, NULL, ms_fHeadIKTime < 0 ? -1 :(s32)(ms_fHeadIKTime*1000.0f), BONETAG_INVALID, &targetPoint, flags, (s32)(ms_fHeadIKBlendInTime*1000.0f), (s32)(ms_fHeadIKBlendOutTime*1000.0f));
			}
		}
	}
}

void CPedDebugVisualiserMenu::ProcessLookIK()
{
	for (int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);

		if (pEnt && pEnt->GetIsTypePed())
		{
			u32 uFlags = LOOKIK_BLOCK_ANIM_TAGS;

			if (ms_bLookIkUseTags)
			{
				uFlags |= LOOKIK_USE_ANIM_TAGS;
				uFlags &= ~LOOKIK_BLOCK_ANIM_TAGS;
			}

			if (ms_bLookIkUseAnimAllowTags)
			{
				uFlags |= LOOKIK_USE_ANIM_ALLOW_TAGS;
			}

			if (ms_bLookIkUseAnimBlockTags)
			{
				uFlags |= LOOKIK_USE_ANIM_BLOCK_TAGS;
			}

			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			Vector3 vTargetPoint(ms_vLookIKTarget);

			if (!ms_bLookIKUseWorldSpace)
			{
				vTargetPoint.Set(pEnt->TransformIntoWorldSpace(ms_vLookIKTarget));
			}

			if (ms_bLookIKUseCamPos)
			{
				vTargetPoint = camInterface::GetPos();
			}

			if (ms_bLookIkUsePlayerPos)
			{
				CPed* pPlayer = FindPlayerPed();

				if (pPlayer && (pPlayer != pFocusPed))
				{
					rage::crSkeleton* pSkeleton = pPlayer->GetSkeleton();

					if (pSkeleton)
					{
						Mat34V mtxHeadGlobal;
						pSkeleton->GetGlobalMtx(CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton->GetSkeletonData(), BONETAG_HEAD), mtxHeadGlobal);
						vTargetPoint = VEC3V_TO_VECTOR3(mtxHeadGlobal.GetCol3());
					}
				}
			}

			if (ms_bLookIKUpdateTarget)
			{
				ms_vLookIKTarget = vTargetPoint;

				if (!ms_bLookIKUseWorldSpace)
				{
					Matrix34 mtxTransform = MAT34V_TO_MATRIX34(pFocusPed->GetTransform().GetMatrix());
					mtxTransform.UnTransform(ms_vLookIKTarget);
				}
			}

			bool bActive = true;

			if (ms_bLookIkUseTagsFromAnimViewer)
			{
				bActive = false;

				const crClip* pClip = CAnimViewer::GetSelectedClip();

				if (pClip)
				{
					float fPhase = CAnimViewer::GetPhase();

					const crTags* pTags = pClip->GetTags();

					if (pTags)
					{
						crTagIterator it(*pTags, CClipEventTags::LookIk);

						while (*it)
						{
							const crTag* pTag = *it;

							if ((fPhase >= pTag->GetStart()) && (fPhase <= pTag->GetEnd()))
							{
								const CClipEventTags::CLookIKEventTag& rProperty = static_cast<const CClipEventTags::CLookIKEventTag&>(pTag->GetProperty());

								ms_uRefDirTorso = rProperty.GetTorsoReferenceDirection();
								ms_uRefDirNeck = rProperty.GetNeckReferenceDirection();
								ms_uRefDirHead = rProperty.GetHeadReferenceDirection();
								ms_uRefDirEye = rProperty.GetEyeReferenceDirection();

								ms_uTurnRate = rProperty.GetTurnSpeed();
								ms_auBlendRate[0] = rProperty.GetBlendInSpeed();
								ms_auBlendRate[1] = rProperty.GetBlendOutSpeed();
								ms_uHeadAttitude = rProperty.GetHeadAttitude();

								ms_uRotLimTorso[LOOKIK_ROT_ANGLE_YAW] = rProperty.GetTorsoYawDeltaLimit();
								ms_uRotLimTorso[LOOKIK_ROT_ANGLE_PITCH] = rProperty.GetTorsoPitchDeltaLimit();
								ms_uRotLimNeck[LOOKIK_ROT_ANGLE_YAW] = rProperty.GetNeckYawDeltaLimit();
								ms_uRotLimNeck[LOOKIK_ROT_ANGLE_PITCH] = rProperty.GetNeckPitchDeltaLimit();
								ms_uRotLimHead[LOOKIK_ROT_ANGLE_YAW] = rProperty.GetHeadYawDeltaLimit();
								ms_uRotLimHead[LOOKIK_ROT_ANGLE_PITCH] = rProperty.GetHeadPitchDeltaLimit();

								ms_auArmComp[0] = rProperty.GetLeftArmCompensation();
								ms_auArmComp[1] = rProperty.GetRightArmCompensation();

								bActive = true;
								break;
							}

							++it;
						}
					}
				}
			}

			if (bActive)
			{
				CIkRequestBodyLook oIkRequest(NULL, BONETAG_INVALID, VECTOR3_TO_VEC3V(vTargetPoint), uFlags, CIkManager::IK_LOOKAT_VERY_HIGH, 0);

				oIkRequest.SetRefDirTorso((LookIkReferenceDirection)ms_uRefDirTorso);
				oIkRequest.SetRefDirNeck((LookIkReferenceDirection)ms_uRefDirNeck);
				oIkRequest.SetRefDirHead((LookIkReferenceDirection)ms_uRefDirHead);
				oIkRequest.SetRefDirEye((LookIkReferenceDirection)ms_uRefDirEye);
				oIkRequest.SetTurnRate((LookIkTurnRate)ms_uTurnRate);
				oIkRequest.SetBlendInRate((LookIkBlendRate)ms_auBlendRate[0]);
				oIkRequest.SetBlendOutRate((LookIkBlendRate)ms_auBlendRate[1]);
				oIkRequest.SetHeadAttitude((LookIkHeadAttitude)ms_uHeadAttitude);

				for (int rotAngleIndex = 0; rotAngleIndex < LOOKIK_ROT_ANGLE_NUM; ++rotAngleIndex)
				{
					oIkRequest.SetRotLimTorso((LookIkRotationAngle)rotAngleIndex, (LookIkRotationLimit)ms_uRotLimTorso[rotAngleIndex]);
					oIkRequest.SetRotLimNeck((LookIkRotationAngle)rotAngleIndex, (LookIkRotationLimit)ms_uRotLimNeck[rotAngleIndex]);
					oIkRequest.SetRotLimHead((LookIkRotationAngle)rotAngleIndex, (LookIkRotationLimit)ms_uRotLimHead[rotAngleIndex]);
				}

				for (int armIndex = 0; armIndex < LOOKIK_ARM_NUM; ++armIndex)
				{
					oIkRequest.SetArmCompensation((LookIkArm)armIndex, (LookIkArmCompensation)ms_auArmComp[armIndex]);
				}

				pFocusPed->GetIkManager().Request(oIkRequest);
			}
		}
	}
}

void CPedDebugVisualiserMenu::ProcessArmIK()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			s32 controlflags = 0;
			if ( ms_bTargetWRTHand )
			{
				controlflags = AIK_TARGET_WRT_HANDBONE;
			}
			else if (ms_bTargetWRTPointHelper )
			{
				controlflags = AIK_TARGET_WRT_POINTHELPER;

			}
			else if (ms_bTargetWRTIKHelper)
			{
				controlflags = AIK_TARGET_WRT_IKHELPER;
			}

			if (ms_bUseAnimAllowTags)
			{
				controlflags |= AIK_USE_ANIM_ALLOW_TAGS;
			}

			if (ms_bUseAnimBlockTags)
			{
				controlflags |= AIK_USE_ANIM_BLOCK_TAGS;
			}

			if (ms_bUseLeftArmIK)
			{
				if (ms_bUseBoneAndOffset)
				{
					pFocusPed->GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::LEFT_ARM, pFocusPed, pFocusPed->GetBoneIndexFromBoneTag(BONETAG_L_PH_HAND), CPedDebugVisualiserMenu::ms_vTargetLeftArmIK, controlflags, ms_fArmIKBlendInTime, ms_fArmIKBlendOutTime);
				}
				else
				{
					Vector3 targetPoint = ms_bTargetInWorldSpace ? CPedDebugVisualiserMenu::ms_vTargetLeftArmIK : pEnt->TransformIntoWorldSpace(CPedDebugVisualiserMenu::ms_vTargetLeftArmIK);
					pFocusPed->GetIkManager().SetAbsoluteArmIKTarget(crIKSolverArms::LEFT_ARM, targetPoint, controlflags, ms_fArmIKBlendInTime, ms_fArmIKBlendOutTime, ms_fArmIKBlendInRange, ms_fArmIKBlendOutRange);
				}
			}

			if (ms_bUseRightArmIK)
			{
				if (ms_bUseBoneAndOffset)
				{
					pFocusPed->GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::RIGHT_ARM, pFocusPed, pFocusPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND), CPedDebugVisualiserMenu::ms_vTargetRightArmIK, controlflags, ms_fArmIKBlendInTime, ms_fArmIKBlendOutTime);
				}
				else
				{
					Vector3 targetPoint = ms_bTargetInWorldSpace ? CPedDebugVisualiserMenu::ms_vTargetRightArmIK : pEnt->TransformIntoWorldSpace(CPedDebugVisualiserMenu::ms_vTargetRightArmIK);
					pFocusPed->GetIkManager().SetAbsoluteArmIKTarget(crIKSolverArms::RIGHT_ARM, targetPoint, controlflags, ms_fArmIKBlendInTime, ms_fArmIKBlendOutTime, ms_fArmIKBlendInRange, ms_fArmIKBlendOutRange);
				}
			}
		}
	}
}

void CPedDebugVisualiserMenu::IKHeadTest1()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			// Test 1 look at point in world space
			CPed * pPlayer = FindPlayerPed();
			Vector3 lookAtPoint(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()));
			static s32 blendInTime1 = 1000;
			static s32 blendOutTime1 = 1000;
			static s32 holdTime1 = 10000;
			pFocusPed->GetIkManager().LookAt(0, NULL, holdTime1, BONETAG_INVALID, &lookAtPoint, 0, blendInTime1, blendOutTime1);
		}
	}
}

void CPedDebugVisualiserMenu::IKHeadTest2()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			// Test 2 look at entity
			CPed * pPlayer = FindPlayerPed();
			static s32 holdTime2 = 10000;
			pFocusPed->GetIkManager().LookAt(0, pPlayer, holdTime2, BONETAG_INVALID, NULL);
			//pPlayer->GetAnimBlender()->GetFirstAnimPlayer()->UnsetFlag(APF_ISPLAYING);
		}
	}
}

void CPedDebugVisualiserMenu::IKHeadTest3()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			// Test 3 look at entity and offset vector
			// the offset vector is w.r.t. character space (facing down the +ve y axis)
			CPed * pPlayer = FindPlayerPed();
			Vector3 offsetVec(0.0f, 0.0f, 2.0f); // 2 meters above the player position
			static s32 holdTime3 = 5000;
			pFocusPed->GetIkManager().LookAt(0, pPlayer, holdTime3, BONETAG_INVALID, &offsetVec);
		}
	}
}

void CPedDebugVisualiserMenu::IKHeadTest4()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			// Test 4 look at entity and offset bone
			CPed * pPlayer = FindPlayerPed();
			//static s32 holdTime4 = 5000;
			static s32 boneTag4 = BONETAG_R_FOOT;
			pFocusPed->GetIkManager().LookAt(0, pPlayer, 5000, (eAnimBoneTag)boneTag4, NULL);
		}
	}
}

void CPedDebugVisualiserMenu::IKHeadTest5()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			// Test 5 look at entity and offset bone and offset vector
			// the offset vector is w.r.t bone space
			// In the case of the players left foot (facing down the -ve y axis)
			CPed * pPlayer = FindPlayerPed();
			static s32 holdTime5 = 5000;
			static Vector3 offsetVec(0.0f, -0.2f, 0.0f);  // 0.2 meters in front of the players left foot
			static s32 boneTag5 = BONETAG_L_FOOT;
			pFocusPed->GetIkManager().LookAt(0, pPlayer, holdTime5, (eAnimBoneTag)boneTag5, &offsetVec);

			/*
			// Check AbortLookAt
			CPed * pPlayer = FindPlayerPed();
			m_pFocus->GetIkManager().AbortLookAt(250);
			*/
		}
	}
}

void CPedDebugVisualiserMenu::IKHeadTest6()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			CPed * pPlayer = FindPlayerPed();
			if (pPlayer)
				pFocusPed->SetPrimaryLookAt(pPlayer);
		}
	}
}

void CPedDebugVisualiserMenu::IKHeadTest7()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			CPed * pPlayer = FindPlayerPed();
			if (pPlayer)
				pFocusPed->SetSecondaryLookAt(pPlayer);
		}
	}
}


void CPedDebugVisualiserMenu::IKArmTest1()
{
	if( CDebugScene::FocusEntities_Get(0) &&
		CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
	{
		//CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));

		// Test 1 point arm at point in world space
		CPed * pPlayer = FindPlayerPed();
		Vector3 pointArmPoint(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()));
		pointArmPoint.x += 1.0f;
		//pFocusPed->GetIkManager().PointArm("Test 1", CIkManager::IK_BODYPART_R_ARM, NULL, -1, &pointArmPoint, 0.25f, 500, 100.0f, false, 1000, 4);
	}
}

void CPedDebugVisualiserMenu::IKArmTest2()
{
	if( CDebugScene::FocusEntities_Get(0) &&
		CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
	{
		//CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));

		// Test 2 Abort point arm
		//pFocusPed->GetIkManager().AbortPointArm(CIkManager::IK_BODYPART_L_ARM, 1000);
	}
}
void CPedDebugVisualiserMenu::IKArmTest3()
{
	if( CDebugScene::FocusEntities_Get(0) &&
		CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
	{
		//CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));

		// Test 3 point arm at point in world space for N seconds
		CPed * pPlayer = FindPlayerPed();
		Vector3 pointArmPoint(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()));
		pointArmPoint.y += 1.0f;
		//pFocusPed->GetIkManager().PointArm("Test 3", CIkManager::IK_BODYPART_R_ARM, NULL, -1, &pointArmPoint, 0.25f, 500, 100.0f, false, 1000, 4);
	}
}

void CPedDebugVisualiserMenu::IKArmTest4()
{
}
void CPedDebugVisualiserMenu::IKArmTest5()
{
}


void CPedDebugVisualiserMenu::IKLegTest1()
{
	// Test 1 point leg at point in world space
	//CPed * pPlayer = FindPlayerPed();
	//Vector3 pointArmPoint(pPlayer->GetPosition());
	//m_pFocus->GetIkManager().PointLeg("Test 1", CIkManager::IK_BODYPART_L_LEG, NULL, -1, &pointArmPoint, 0.25f, 1000, 100.0f);
}
void CPedDebugVisualiserMenu::IKLegTest2()
{
	// Test 2 Abort point leg
	//m_pFocus->GetIkManager().AbortPointLeg(CIkManager::IK_BODYPART_L_LEG, 1000);
}
void CPedDebugVisualiserMenu::IKLegTest3()
{
	// Point right leg at point in world space for N seconds
	//CPed * pPlayer = FindPlayerPed();
	//Vector3 pointLegPoint(pPlayer->GetPosition());
	//m_pFocus->GetIkManager().PointLeg("Test 3", CIkManager::IK_BODYPART_R_LEG, NULL, -1, &pointLegPoint, 0.25f, 500, 100.0f, false, 1000);
}
void CPedDebugVisualiserMenu::IKLegTest4()
{
}
void CPedDebugVisualiserMenu::IKLegTest5()
{
}

void CPedDebugVisualiserMenu::AddTaskStandStill()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			pFocusPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(-1), PED_TASK_PRIORITY_PRIMARY);
		}
	}
}

void CPedDebugVisualiserMenu::AddOrRemovePedFromGroup(CPed * BANK_ONLY(pLeaderPed), CPed * BANK_ONLY(pFollower), bool BANK_ONLY(bAdd))
{
	Assert(pLeaderPed && pFollower);

	// Firstly, remove "pFollower" ped from any group he might be in.
	CPedGroup* pFollowerGroup = pFollower->GetPedsGroup();

	if(pFollowerGroup)
	{
		pFollowerGroup->GetGroupMembership()->RemoveMember(pFollower);
		pFollowerGroup->Process();
		pFollower->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(-1), PED_TASK_PRIORITY_PRIMARY);
		//pFollower->GetPedIntelligence()->SetPedDecisionMakerType(CDecisionMakerTypes::DECISION_MAKER_PED_RANDOM1);

		// If this group is empty apart from its leader, then remove the "pLeaderPed".
		if(pFollowerGroup->GetGroupMembership()->CountMembersExcludingLeader() == 0)
		{
			pFollowerGroup->GetGroupMembership()->Flush();
		}

		pFollowerGroup->Process();
	}

	// If we only want to remove this ped, then return
	if(!bAdd)
	{
		return;
	}

	// See if the "pLeaderPed" is already a leader of any group
	int iGroupID = -1;
	if(!CPedGroups::IsGroupLeader(pLeaderPed))
	{
		// No, so create a new group
		iGroupID = CPedGroups::AddGroup(POPTYPE_RANDOM_AMBIENT);
		//		Assertf(iGroupID >= 0, "%s:No free groups left", "CPedDebugVisualiserMenu");
		//		if(iGroupID < 0) return;
		//
		//			CPedGroups::ms_groups[iGroupID].GetGroupIntelligence()->SetDefaultTaskAllocatorType(CPedGroupDefaultTaskAllocatorTypes::DEFAULT_TASK_ALLOCATOR_FOLLOW_ANY_MEANS);

		// set "pLeaderPed" as group leader
		CPedGroups::ms_groups[iGroupID].GetGroupMembership()->SetLeader(pLeaderPed);
		CPedGroups::ms_groups[iGroupID].Process();
	}
	else
	{
		// Yes, so get the existing group.  NB : We should have a function for this IFF peds can only belong to one group at a time.
		for (int i = 0; i < CPedGroups::MAX_NUM_GROUPS; i++)
		{
			if(CPedGroups::ms_groups[i].GetGroupMembership()->IsLeader(pLeaderPed))
			{
				iGroupID = i;
				break;
			}
		}
	}

	Assert(iGroupID >= 0);

	// If this ped is not already in the group, then add
	if(pFollower && !CPedGroups::ms_groups[iGroupID].GetGroupMembership()->IsFollower(pFollower))
	{
		// add the "pFollower" to our group as a member
		pFollower->PopTypeSet(POPTYPE_MISSION);
		pFollower->SetDefaultDecisionMaker();
		pFollower->SetCharParamsBasedOnManagerType();
		pFollower->GetPedIntelligence()->SetDefaultRelationshipGroup();
		CPedGroups::ms_groups[iGroupID].GetGroupMembership()->AddFollower(pFollower);
		CPedGroups::ms_groups[iGroupID].Process();
		CPedGroups::ms_groups[iGroupID].Process();
	}
}

void CPedDebugVisualiserMenu::AddTaskGotoPlayer()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			pFocusPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskDoNothing(-1), PED_TASK_PRIORITY_PRIMARY);
		}
	}
}

void CPedDebugVisualiserMenu::MoveFollowEntityOffset()
{
	Vector3 vecPos(VEC3_ZERO);

	// Try to parse the line.
	int nItems = 0;
	{
		//	Make a copy of this string as strtok will destroy it.
		Assertf( (strlen(g_vCurrentOffsetPosition) < 256), "Offset position line is too long to copy.");
		char offsetPositionLine[256];
		strncpy(offsetPositionLine, g_vCurrentOffsetPosition, 256);

		// Get the locations to store the float values into.
		float* apVals[3] = {&vecPos.x, &vecPos.y, &vecPos.z };

		// Parse the line.
		char* pToken = NULL;
		const char* seps = " ,\t";
		pToken = strtok(offsetPositionLine, seps);
		while(pToken)
		{
			// Try to read the token as a float.
			int success = sscanf(pToken, "%f", apVals[nItems]);
			Assertf(success, "Unrecognized token '%s' in offset position line.",pToken);
			if(success)
			{
				++nItems;
				Assertf((nItems < 4), "Too many tokens in offset position line." );
			}
			pToken = strtok(NULL, seps);
		}
	}

	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( !pEnt || !pEnt->GetIsTypePed() )
		{
			continue;
		}

		CPed* pFocusPed = static_cast<CPed*>(pEnt);


		if (pFocusPed)
		{
			// Follow the player
			if (!pFocusPed->IsAPlayerPed())
			{
				CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
				//Vector3 vOffset(1.0f,0.0f,0.0f);
				CTask* pMoveTask = rage_new CTaskMoveFollowEntityOffset(pPlayerPed,g_fTaskMoveBlendRatio,0.0f,vecPos,-1);
				CTask* pTaskToAdd = rage_new CTaskComplexControlMovement(pMoveTask);
				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToAdd);
				pFocusPed->GetPedIntelligence()->AddEvent(event);
			}
		}
	}
}

void
CPedDebugVisualiserMenu::AttackPlayersCar(void)
{
}

void
CPedDebugVisualiserMenu::DropPedAtCameraPos(void)
{
#if __DEV//def CAM_DEBUG
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			camDebugDirector& debugDirector = camInterface::GetDebugDirector();
			if(debugDirector.IsFreeCamActive())
			{
				const Vector3& pos = debugDirector.GetFreeCamFrame().GetPosition();
				pFocusPed->SetPosition(pos);
			}
		}
	}
#endif // __DEV
}

void TestDodgyRunStart()
{
	//static float fHeading = PI*0.5f;
	//static Vector3 vPedPos(0.0f, 74.5f, 7.5f);
	//static Vector3 vTargetOffset(20.0f, 0.0f, 0.0);

	static float fHeading = 0.0f;
	static Vector3 vPedPos(-70.0f, 34.0f, 7.5f);
	static Vector3 vTargetOffset(0.0f, -30.0f, 0.0);

	static float fMBR = MOVEBLENDRATIO_RUN;

	CPed * pPed = FindPlayerPed();
	if(pPed)
	{
		pPed->StopAllMotion(true);
		pPed->Teleport(vPedPos, fHeading);
		Vector3 vTarget = vPedPos + vTargetOffset;

		CTaskMoveGoToPointAndStandStill * pGoto = rage_new CTaskMoveGoToPointAndStandStill(fMBR, vTarget);
		pPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskComplexControlMovement(pGoto), PED_TASK_PRIORITY_PRIMARY);
	}
}



bool g_bOverrideScenarioWithThisCall = false;

void CPedDebugVisualiserMenu::ForceSpawnInArea()
{
	Vector3 v1, v2;
	if( !CPhysics::GetMeasuringToolPos( 0, v1 ) ||
		!CPhysics::GetMeasuringToolPos( 1, v2 ) )
	{
		v1 = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
		v2 = v1;
		v2.x += 80.0f;
	}
	g_bOverrideScenarioWithThisCall = true;
	const bool allowDeepInteriors = true;
	CPedPopulation::GenerateScenarioPedsInSpecificArea(v1, allowDeepInteriors, (v1-v2).Mag(), 100);
	g_bOverrideScenarioWithThisCall = false;
}

void
CPedDebugVisualiserMenu::IncreaseWantedLevel()
{
	CPed * pPlayerPed = FindPlayerPed();
	Assert(pPlayerPed);

	CWanted * pWanted = pPlayerPed->GetPlayerWanted();
	Assert(pWanted);

	s32 iWantedLevel = pWanted->GetWantedLevel();

	if(iWantedLevel < pWanted->nMaximumWantedLevel)
		iWantedLevel++;

	pWanted->CheatWantedLevel(iWantedLevel);
	//pWanted->SetWantedLevel(iWantedLevel);

	printf("New Player Wanted Level : %i\n", iWantedLevel);
}


void
CPedDebugVisualiserMenu::DecreaseWantedLevel()
{
	CPed * pPlayerPed = FindPlayerPed();
	Assert(pPlayerPed);

	CWanted * pWanted = pPlayerPed->GetPlayerWanted();
	Assert(pWanted);

	s32 iWantedLevel = pWanted->GetWantedLevel();

	if(iWantedLevel > 0)
		iWantedLevel--;

	pWanted->CheatWantedLevel(iWantedLevel);
	//pWanted->SetWantedLevel(iWantedLevel);

	printf("New Player Wanted Level : %i\n", iWantedLevel);
}

void CPedDebugVisualiserMenu::TriggerArrest()
{
	CPed * pPlayerPed = FindPlayerPed();
	Assert(pPlayerPed);
	pPlayerPed->SetArrestState(ArrestState_Arrested);
}


void
CPedDebugVisualiserMenu::WantedLevelAlwaysZero()
{
	CPed * pPlayerPed = FindPlayerPed();
	Assert(pPlayerPed);

	CWanted * pWanted = pPlayerPed->GetPlayerWanted();
	Assert(pWanted);

	pWanted->CheatWantedLevel(0);

	pWanted->m_fMultiplier = 0.0f;

	printf("New Player Wanted Level : 0\n");
}

void
CPedDebugVisualiserMenu::SwitchPlayerToFocusPed()
{
    CPed * pPlayerPed = FindPlayerPed();
    Assert(pPlayerPed);

    CPed* pFocusPed = GetFocusPed();
    if( pFocusPed && !pFocusPed->IsAPlayerPed() )
    {
        CGameWorld::ChangePlayerPed(*pPlayerPed, *pFocusPed);
    }
}

void CPedDebugVisualiserMenu::TriggerWorldExtentsProtection()
{
	CPlayerInfo::ms_bFakeWorldExtentsTresspassing = true;
}

void CPedDebugVisualiserMenu::TriggerSharkSpawnToCamera()
{
	CWildlifeManager::GetInstance().DispatchASharkNearPlayer(false, true);
}

void
CPedDebugVisualiserMenu::KillFocusPed(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			pFocusPed->SetHealth(0.0f);
		}
	}
}

CPed* CPedDebugVisualiserMenu::GetFocusPed(void)
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

void
CPedDebugVisualiserMenu::ReviveFocusPed(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			const CHealthConfigInfo* pHealthInfo = pFocusPed->GetPedHealthInfo();
			Assert( pHealthInfo );

			if(pFocusPed->IsDead())
			{
				//Bring the ped back to life.
				pFocusPed->SetHealth( pHealthInfo->GetDefaultHealth() );
				pFocusPed->EnableCollision();
				pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KnockedUpIntoAir, false );
				pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth, false );
				pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown, false );
				pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout, false );
				pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStandardMelee, false );
				pFocusPed->SetDeathState(DeathState_Alive);
				pFocusPed->SetArrestState(ArrestState_None);
				pFocusPed->ClearDeathInfo();
				pFocusPed->m_nPhysicalFlags.bRenderScorched = false;

				CTask* pTask=rage_new CTaskGetUpAndStandStill();
				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTask);
				CPedIntelligence * pPedAI = pFocusPed->GetPedIntelligence();
				pPedAI->AddEvent(event);
			}
			else if( pFocusPed->IsInjured() )
			{
				pFocusPed->SetHealth( pHealthInfo->GetDefaultHealth() );
			}
		}
	}
}

void
CPedDebugVisualiserMenu::ChangeFormationSpacing()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			CPedGroup * pGroup = pFocusPed->GetPedsGroup();
			if(pGroup)
			{
				pGroup->GetFormation()->SetSpacing(CPedDebugVisualiserMenu::m_fFormationSpacing, CPedDebugVisualiserMenu::m_fFormationDecelDist, CPedDebugVisualiserMenu::m_fFormationAccelDist, (scrThreadId)CPedDebugVisualiserMenu::m_iFormationSpacingScriptId);
			}
		}
	}
}


void CPedDebugVisualiserMenu::MakePedFollowNavMeshToCoord(CPed * pPed, const Vector3 & vPos)
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	const Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	switch(g_iNavigateToCursorMode)
	{
		case 0:
		{
			CTaskMoveGoToPoint * pTaskGoto = rage_new CTaskMoveGoToPoint(g_fTaskMoveBlendRatio, vPos);
			CTaskComplexControlMovement * pControlMovement = rage_new CTaskComplexControlMovement(pTaskGoto);
			pPed->GetPedIntelligence()->AddTaskAtPriority(pControlMovement, PED_TASK_PRIORITY_PRIMARY);

			break;
		}
		case 1:
		{
			CTaskMoveGoToPointAndStandStill * pTaskGotoStandStill;

			if(g_bNavMeshUseTargetHeading)
				pTaskGotoStandStill = rage_new CTaskMoveGoToPointAndStandStill(g_fTaskMoveBlendRatio, vPos, 0.f, 5.f, false, g_bNavMeshStopExactly, g_fNavMeshTargetHeading);
			else
				pTaskGotoStandStill = rage_new CTaskMoveGoToPointAndStandStill(g_fTaskMoveBlendRatio, vPos, 0.f, 5.f, false, g_bNavMeshStopExactly);


			CTaskComplexControlMovement * pControlMovement = rage_new CTaskComplexControlMovement(pTaskGotoStandStill);
			pPed->GetPedIntelligence()->AddTaskAtPriority(pControlMovement, PED_TASK_PRIORITY_PRIMARY);

			grcDebugDraw::Sphere(vPos, 0.3f, Color32(0xff, 0x00, 0x00, 0xff), false, 1024);

			break;
		}
		case 2:
		case 3:
		{
			// If in a vehicle, then use prototype in-vehicle navigation
			if( g_iNavigateToCursorMode==2 && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				Assert(pPed->GetMyVehicle());
				sVehicleMissionParams params;
				const float fStraightLineDistance = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST;
				CTaskVehicleMissionBase* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, fStraightLineDistance);
				params.m_iDrivingFlags.SetFlag(DF_PreferNavmeshRoute);
				CTaskControlVehicle* pDriveTask = rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pVehicleTask);
				pPed->GetPedIntelligence()->AddTaskAtPriority(pDriveTask, PED_TASK_PRIORITY_PRIMARY);
			}
			else
			{
				CTaskMoveFollowNavMesh * pExistingTask = NULL;
				CTaskComplexControlMovement * pCtrlMoveTask = (CTaskComplexControlMovement*) pPed->GetPedIntelligence()->FindTaskPrimaryByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
				if(pCtrlMoveTask)
				{
					if(pCtrlMoveTask->GetRunningMovementTask(pPed) && pCtrlMoveTask->GetRunningMovementTask(pPed)->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
					{
						pExistingTask = (CTaskMoveFollowNavMesh*) pCtrlMoveTask->GetRunningMovementTask(pPed);
					}
				}

				if(pExistingTask)
				{
					pExistingTask->SetTarget(pPed, RCC_VEC3V(vPos));
				}
				else
				{
					pPed->GetPedIntelligence()->GetNavCapabilities().SetFlag(CPedNavCapabilityInfo::FLAG_MAY_CLIMB);
					pPed->GetPedIntelligence()->GetNavCapabilities().SetFlag(CPedNavCapabilityInfo::FLAG_MAY_DROP);

					CNavParams params;
					params.m_vTarget = vPos;
					params.m_fMoveBlendRatio = g_fTaskMoveBlendRatio;
					params.m_bFleeFromTarget = (g_iNavigateToCursorMode==3);
					params.m_fFleeSafeDistance = 30.0f;
					params.m_fTargetRadius = g_fTargetRadius;

					CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(params);

					if(g_bNavMeshUseTargetHeading)
						pNavMeshTask->SetTargetStopHeading(g_fNavMeshTargetHeading);
					if(g_bNavMeshStopExactly)
						pNavMeshTask->SetStopExactly(true);

					pNavMeshTask->SetMaxDistanceMayAdjustPathStartPosition(g_fNavMeshMaxDistanceToAdjustStartPosition);

					if(g_bNavMeshDontAdjustTargetPosition)
						pNavMeshTask->SetDontAdjustTargetPosition(true);
					if(g_bNavMeshAllowToNavigateUpSteepPolygons)
						pNavMeshTask->SetAllowToNavigateUpSteepPolygons(true);

					if(params.m_bFleeFromTarget)
					{
						bool bPedStartedInWater = pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning );
						pNavMeshTask->SetFleeNeverEndInWater( !bPedStartedInWater );
					}

					if(pPed->GetPrimaryMotionTask())
					{
						Vector3 vMoveDir;
						pPed->GetPrimaryMotionTask()->GetMoveVector(vMoveDir);
						if(vMoveDir.Mag() > MBR_WALK_BOUNDARY)
							pNavMeshTask->SetKeepMovingWhilstWaitingForPath(true);
					}

					// Set flags, etc
					pNavMeshTask->SetUseLargerSearchExtents(true);

					static dev_bool bGoFarAsPoss = false;
					pNavMeshTask->SetGoAsFarAsPossibleIfNavMeshNotLoaded(bGoFarAsPoss);

					pNavMeshTask->SetUseFreeSpaceReferenceValue(
						CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_bUseFreeSpaceRefVal,
						CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_iFreeSpaceRefVal,
						CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_bFavourFreeSpaceGreaterThanRefVal);

					if(CPedDebugVisualiserMenu::ms_NavmeshRouteDebugToggles.m_bUseDirectionalCover)
					{
						Vector3 vThreatOrigin(vOrigin.x, vOrigin.y, vOrigin.z); // NB: Z component is ignored
						pNavMeshTask->SetUseDirectionalCover(true, &vThreatOrigin);
					}

					CTaskComplexControlMovement * pControlMovement = rage_new CTaskComplexControlMovement(pNavMeshTask);
					pPed->GetPedIntelligence()->AddTaskAtPriority(pControlMovement, PED_TASK_PRIORITY_PRIMARY);
				}
			}
			break;
		}
		case 4:
		{
			CTaskGoToPointAnyMeans * pAnyMeans = rage_new CTaskGoToPointAnyMeans(g_fTaskMoveBlendRatio, vPos);
			pPed->GetPedIntelligence()->AddTaskAtPriority(pAnyMeans, PED_TASK_PRIORITY_PRIMARY);
			break;
		}
		case 5:
		{
			// If debug camera is active, use this as our aim-at origin
			Vector3 vAimAtPos;
			camDebugDirector& debugDirector = camInterface::GetDebugDirector();
			const bool isUsingDebugCam = debugDirector.IsFreeCamActive();
			if(isUsingDebugCam)
			{
				const camFrame& freeCamFrame = debugDirector.GetFreeCamFrame();
				vAimAtPos = freeCamFrame.GetPosition();
			}
			// Otherwise use the target position
			else
			{
				vAimAtPos = vPos + ZAXIS;
			}

			static bool bSkipIdleTransition = false;
			CTaskGoToPointAiming* pTask = rage_new CTaskGoToPointAiming(CAITarget(vPos), CAITarget(vAimAtPos), g_fTaskMoveBlendRatio, bSkipIdleTransition);
			pTask->SetTargetRadius(g_fTargetRadius);
			pTask->SetUseNavmesh(true);
			//pTask->SetScriptNavFlags(iNavFlags);

//			if(bShoot)
//			{
//				pTask->SetFiringPattern(FIRING_PATTERN_FULL_AUTO);
//			}

			pPed->GetPedIntelligence()->AddTaskAtPriority(pTask, PED_TASK_PRIORITY_PRIMARY);

			break;
		}
		case 6:
		{
			pPed->GetMotionData()->SetRepositionMoveTarget(true, &vPos);
			break;
		}
	}
}


void
CPedDebugVisualiserMenu::JamesDebugTest(void)
{
#if 0
	CPed * pPlayerPed = FindPlayerPed();
	Vector3 vPlayerPos = pPlayerPed->GetPosition();
	Matrix34 tempMat;

	static int iTestType = 0;

	switch(iTestType)
	{
		//****************************************************
		//	Set up two peds, heading straight at each other
		//****************************************************
	case 0:
		{
			// Grab the 2 closest peds to the player
			CPed * pPed1 = (CPed*)pPlayerPed->GetPedIntelligence()->GetNearbyPeds()[0];
			CPed * pPed2 = (CPed*)pPlayerPed->GetPedIntelligence()->GetNearbyPeds()[1];

			if(pPed1 && pPed2)
			{
				Vector3 vPed1ToPed2 = pPed2->GetPosition() - pPed1->GetPosition();
				float fSeparation = vPed1ToPed2.Mag();
				float fExtraDist = 0.0f;
				vPed1ToPed2.Normalize();

				Vector3 vPed1Target = pPed1->GetPosition() + (vPed1ToPed2 * (fSeparation + fExtraDist));
				Vector3 vPed2Target = pPed2->GetPosition() - (vPed1ToPed2 * (fSeparation + fExtraDist));

				CTaskMoveGoToPoint * pMoveTask1 = rage_new CTaskMoveGoToPoint(g_fTaskMoveBlendRatio, vPed1Target);
				pPed1->GetPedIntelligence()->AddTaskAtPriority( rage_new CTaskComplexControlMovement(pMoveTask1), PED_TASK_PRIORITY_PRIMARY );

				CTaskMoveGoToPoint * pMoveTask2 = rage_new CTaskMoveGoToPoint(g_fTaskMoveBlendRatio, vPed2Target);
				pPed2->GetPedIntelligence()->AddTaskAtPriority( rage_new CTaskComplexControlMovement(pMoveTask2), PED_TASK_PRIORITY_PRIMARY );
			}

			break;
		}

		//****************************************************
		//	Set up two peds, next to each other and heading
		//	for the same point
		//****************************************************
	case 1:
		{
			CPed::Pool * pPedPool = CPed::GetPool();
			if(pPedPool->GetNoOfFreeSpaces() < 2)
				return;

			Vector3 vPedPositions[2];
			vPedPositions[0] = vPlayerPos - Vector3(10.0f, -1.0f, 0);
			vPedPositions[1] = vPlayerPos - Vector3(10.0f, 1.0f, 0);

			Vector3 vPedHeadings[2] = { Vector3(1.0f, 0, 0), Vector3(1.0f, 0, 0) };
			Vector3 vPedTarget = vPlayerPos + Vector3(10.0f, 0, 0);

			for(int p=0; p<2; p++)
			{
				int pedId = 0;

				//get the modelInfo associated with this ped ID
				CStaticStore<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
				CPedModelInfo& pedModelInfo = pedModelInfoStore[pedId];
				u32 PedModelIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();

				//get the name of the model used by this model info and display it
				Displayf("This ped uses the model: %s\n",pedModelInfo.GetModelName());

				// ensure that the model is loaded and ready for drawing for this ped
				if(!CModelInfo::HaveAssetsLoaded(PedModelIndex))
				{
					CModelInfo::RequestAssets(PedModelIndex, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
					CStreaming::LoadAllRequestedObjects(true);
				}

				vPedPositions[p].z = CWorldProbe::FindGroundZForCoord(vPedPositions[p].x, vPedPositions[p].y);
				float fHeading = fwAngle::GetRadianAngleBetweenPoints(0,0,vPedHeadings[p].x,vPedHeadings[p].y);
				fHeading = fwAngle::LimitRadianAngle(( DtoR * fHeading));

				tempMat.Identity();
				tempMat.RotateZ(fHeading);
				tempMat.d = vPedPositions[p];

				// create the debug ped which will have it's textures played with
				const CControlledByInfo localAiControl(false, false);
				CPed * pPedVarDebugPed = CPedFactory::GetFactory()->Create(localAiControl, PedModelIndex, &tempMat);
				Assertf(pPedVarDebugPed, "CPedDebugVisualiserMenu::SetupAvoidanceTest : CreatePed() - Couldn't create a the ped variation debug ped");

				CTaskMoveGoToPoint * pMoveTask = rage_new CTaskMoveGoToPoint(PEDMOVE_WALK, vPedTarget);
				CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement( pMoveTask );
				pPedVarDebugPed->GetPedIntelligence()->AddTaskDefault(pTask);

				CGameWorld::Add(pPedVarDebugPed, CGameWorld::OUTSIDE );

				pPedVarDebugPed->SetCurrentHeading(fHeading);
				pPedVarDebugPed->SetDesiredHeading(fHeading);
			}

			break;
		}

		//****************************************************
		//	Set up a load of random peds
		//****************************************************
	case 2:
		{
			int iNumPeds = 10;
			float fRandomRange = 5.0f;

			// NB : sometimes we can get better deadlock between colliding peds if this is 'false'
			// and they all start out walking in the same direction..
			for(int p=0; p<iNumPeds; p++)
			{
				CPed::Pool * pPedPool = CPed::GetPool();
				if(!pPedPool->GetNoOfFreeSpaces())
					break;

				int pedId = 0;

				//get the modelInfo associated with this ped ID
				CStaticStore<CPedModelInfo>& pedModelInfoStore = CModelInfo::GetPedModelInfoStore();
				CPedModelInfo& pedModelInfo = pedModelInfoStore[pedId];
				u32 PedModelIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();

				//get the name of the model used by this model info and display it
				Displayf("This ped uses the model: %s\n",pedModelInfo.GetModelName());

				// ensure that the model is loaded and ready for drawing for this ped
				if(!CModelInfo::HaveAssetsLoaded(PedModelIndex))
				{
					CModelInfo::RequestAssets(PedModelIndex, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
					CStreaming::LoadAllRequestedObjects(true);
				}


				// generate a location to create the ped from the camera position & orientation
				Vector3 vPedPos(
					vPlayerPos.x + fwRandom::GetRandomNumberInRange(-fRandomRange, fRandomRange),
					vPlayerPos.y + fwRandom::GetRandomNumberInRange(-fRandomRange, fRandomRange),
					vPlayerPos.z + 10.0f
					);
				vPedPos.z = CWorldProbe::FindGroundZForCoord(vPedPos.x, vPedPos.y);
				float fRandHeading = fwRandom::GetRandomNumberInRange(0.0f, 360.0f);
				fRandHeading = fwAngle::LimitRadianAngle(( DtoR * fRandHeading));

				tempMat.Identity();
				tempMat.RotateZ(fRandHeading);
				tempMat.d = vPedPos;

				// create the debug ped which will have it's textures played with
				const CControlledByInfo localAiControl(false, false);
				CPed * pPedVarDebugPed = CPedFactory::GetFactory()->Create(localAiControl, PedModelIndex, &tempMat);
				Assertf(pPedVarDebugPed, "CPedDebugVisualiserMenu::SetupAvoidanceTest : CreatePed() - Couldn't create a the ped variation debug ped");

				//pPedVarDebugPed->GetPedIntelligence()->AddTaskDefault(new CTaskSimpleStandStill(999999,true));
				u8 iWanderDir = (u8)fwRandom::GetRandomNumberInRange(0, 8);
				CTaskComplexWander * pWanderTask = rage_new CTaskComplexWander(PEDMOVE_WALK, iWanderDir);
				pPedVarDebugPed->GetPedIntelligence()->AddTaskDefault(pWanderTask);

				CGameWorld::Add(pPedVarDebugPed, CGameWorld::OUTSIDE );

				pPedVarDebugPed->SetCurrentHeading(fRandHeading);
				pPedVarDebugPed->SetDesiredHeading(fRandHeading);
			}
		}
	}
#endif // 0
}

void
CPedDebugVisualiserMenu::OnSelectTaskCombo(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( !pEnt || !pEnt->GetIsTypePed() )
		{
			continue;
		}

		CPed* pFocusPed = static_cast<CPed*>(pEnt);

		if(!pFocusPed || m_iTaskComboIndex >= m_TaskComboTypes.GetCount())
		{
			continue;
		}

		CPed * pPlayer = FindPlayerPed();
		Vector3 vCameraPos;
		float fCameraHeading = 0.0f;

#if __DEV//def CAM_DEBUG
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		if(debugDirector.IsFreeCamActive())
		{
			const camFrame& freeCamFrame = debugDirector.GetFreeCamFrame();
			vCameraPos = freeCamFrame.GetPosition();
			fCameraHeading = freeCamFrame.ComputeHeading();
		}
		else
		{
			vCameraPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
			fCameraHeading = pPlayer->GetTransform().GetHeading();
		}
#else
		vCameraPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
#endif

		pFocusPed->GetPedIntelligence()->ClearTasks(true, true);

		eFocusPedTasks focusPedTask = m_TaskComboTypes[m_iTaskComboIndex];
		CTask* pTaskToAdd = NULL;

		switch(focusPedTask)
		{
		case eFollowEntityOffset:
			{
				if (pFocusPed)
				{
					float fTargetRadius = 0.0f;
					// Follow the player
					if (!pFocusPed->IsAPlayerPed())
					{
						CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
						Vector3 vOffset(1.0f,0.0f,0.0f);
						CTask* pMoveTask = rage_new CTaskMoveFollowEntityOffset(pPlayerPed,g_fTaskMoveBlendRatio,fTargetRadius,vOffset,-1);
						pTaskToAdd = rage_new CTaskComplexControlMovement(pMoveTask);
					}

					// Follow the first vehicle
					/*CVehicle* pNearestVehicle=pFocusPed->GetPedIntelligence()->GetClosestVehicleInRange();
					Vector3 vOffset(3.0f,0.0f,0.0f);
					CTask* pMoveTask = rage_new CTaskMoveFollowEntityOffset(pNearestVehicle,g_fTaskMoveBlendRatio,fTargetRadius,vOffset);
					pTaskToAdd = rage_new CTaskComplexControlMovement(pMoveTask);*/
				}
				break;
			}
		case eStandStill:
			{
				pTaskToAdd = rage_new CTaskDoNothing(-1);
				break;
			}
		case eFacePlayer:
			{
				pTaskToAdd = rage_new CTaskTurnToFaceEntityOrCoord(pPlayer);
				break;
			}
		case eAddToPlayerGroup:
			{
				AddOrRemovePedFromGroup(pPlayer, pFocusPed, true);
				break;
			}
		case eRemoveFromPlayerGroup:
			{
				AddOrRemovePedFromGroup(pPlayer, pFocusPed, false);
				break;
			}
		case eWander:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				CTaskWander* pTaskWander = rage_new CTaskWander(MOVEBLENDRATIO_WALK, pFocusPed->GetCurrentHeading());
				pTaskWander->SetMoveBlendRatio(g_fTaskMoveBlendRatio);
				pFocusPed->GetPedIntelligence()->AddTaskDefault(pTaskWander);
				break;
			}
		case eCarDriveWander:
			{
				CVehicle* pVehicle = pFocusPed->GetMyVehicle();
				if (!pVehicle)
				{
					pVehicle = pFocusPed->GetPedIntelligence()->GetClosestVehicleInRange();
				}

				if (pVehicle)
				{
					pFocusPed->GetPedIntelligence()->ClearTasks();
					CTaskCarDriveWander* pTaskCarDriveWander = rage_new CTaskCarDriveWander(pVehicle);
					pFocusPed->GetPedIntelligence()->AddTaskDefault(pTaskCarDriveWander);
				}
				break;
			}
		case eUnalerted:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				CTaskUnalerted* pTask = rage_new CTaskUnalerted;
				pFocusPed->GetPedIntelligence()->AddTaskDefault(pTask);
				break;
			}
		case eFlyingWander:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				pFocusPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskFlyingWander);
				break;
			}
		case eSwimmingWander:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				pFocusPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskSwimmingWander);
			}
		case eGoToPointToCameraPos:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				float fTargetRadius = 0.1f;

				CTaskMoveGoToPointAndStandStill * pGoToTask = rage_new CTaskMoveGoToPointAndStandStill(
					g_fTaskMoveBlendRatio,
					vCameraPos,
					fTargetRadius,
					0.0f,
					false, false,
					fCameraHeading
					);

				pTaskToAdd = rage_new CTaskComplexControlMovement(pGoToTask);
				break;
			}
		case eFlyToPointToCameraPos:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				float fTargetRadius = 0.1f;

				CTaskFlyToPoint * pGoToTask = rage_new CTaskFlyToPoint(
					g_fTaskMoveBlendRatio,
					vCameraPos,
					fTargetRadius
					);

				pTaskToAdd = rage_new CTaskComplexControlMovement(pGoToTask);
				break;
			}
		case eFollowNavmeshToMeasuringToolStopExactly:
			{
				Vector3 vPosition;
				if( CPhysics::GetMeasuringToolPos(0, vPosition) )
				{
					CTaskMoveFollowNavMesh* pTaskFollowNavMesh = rage_new CTaskMoveFollowNavMesh(
						g_fTaskMoveBlendRatio,
						vPosition);
					pTaskFollowNavMesh->SetStopExactly(true);
					Vector3 vLookPosition;
					if (CPhysics::GetMeasuringToolPos(1, vLookPosition))
					{
						// set the target stop heading to point at the
						// look position
						pTaskFollowNavMesh->SetTargetStopHeading(fwAngle::GetRadianAngleBetweenPoints(vLookPosition.x, vLookPosition.y, vPosition.x, vPosition.y));
					}
					pTaskToAdd = rage_new CTaskComplexControlMovement(pTaskFollowNavMesh);
				}
				break;
			}
		case eFollowNavMeshToCameraPos:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(g_fTaskMoveBlendRatio, vCameraPos);
				pTaskToAdd = rage_new CTaskComplexControlMovement(pNavMeshTask);
				break;
			}
		case eGenericMoveToCameraPos:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				CTaskGenericMoveToPoint * genMoveTask = rage_new CTaskGenericMoveToPoint(g_fTaskMoveBlendRatio, vCameraPos);
				pTaskToAdd = rage_new CTaskComplexControlMovement(genMoveTask);
				break;
			}
		case eFollowNavMeshToCameraPosWithSlide:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();

				CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(g_fTaskMoveBlendRatio, vCameraPos);
				pNavMeshTask->SetSlideToCoordAtEnd(true, fCameraHeading);
				pTaskToAdd = rage_new CTaskComplexControlMovement(pNavMeshTask);

				break;
			}
		case eFollowNavMeshAndSlideInSequence:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();

				CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(g_fTaskMoveBlendRatio, vCameraPos);
				CTaskComplexControlMovement * pCtrlMove = rage_new CTaskComplexControlMovement(pNavTask);
				CTaskSlideToCoord * pSlideTask = rage_new CTaskSlideToCoord(vCameraPos, fCameraHeading, 1.0f);

				CTaskSequenceList * pSeqTask = rage_new CTaskSequenceList();
				pSeqTask->AddTask(pCtrlMove);
				pSeqTask->AddTask(pSlideTask);
				pTaskToAdd = rage_new CTaskUseSequence(*pSeqTask);

				CTaskDoNothing * pStillTask = rage_new CTaskDoNothing(10000);
				pFocusPed->GetPedIntelligence()->AddTaskDefault(pStillTask);

				// Make this guys a mission char or they might become a dummy
				pFocusPed->PopTypeSet(POPTYPE_TOOL);
				pFocusPed->SetDefaultDecisionMaker();
				pFocusPed->SetCharParamsBasedOnManagerType();
				pFocusPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

				break;
			}
		case eSlideToCoordToCameraPos:
			{
				pFocusPed->GetPedIntelligence()->ClearTasks();
				static float fSpeed = 1.0f;
				pTaskToAdd = rage_new CTaskSlideToCoord(vCameraPos, fCameraHeading, fSpeed);
				break;
			}
		case eSetMoveTaskTargetToCameraPos:
			{
				CTask * pMoveTask = pFocusPed->GetPedIntelligence()->GetGeneralMovementTask();
				if(pMoveTask && pMoveTask->IsMoveTask())
				{
					CTaskMoveInterface * pMoveInterface = (CTaskMoveInterface*)pMoveTask;
					pMoveInterface->SetTarget(pFocusPed, VECTOR3_TO_VEC3V(vCameraPos));
				}
				break;
			}
		case eSetDesiredHeadingToFaceCamera:
			{
				pFocusPed->SetDesiredHeading( vCameraPos );
				break;
			}
		case eWalkAlongsideClosestPed:
			{
				CPed * pClosestPed = (CPed*)pFocusPed->GetPedIntelligence()->GetClosestPedInRange();
				if(pClosestPed)
				{
					CTaskMoveBeInFormation * pFormationTask = rage_new CTaskMoveBeInFormation(pClosestPed);
					pTaskToAdd = rage_new CTaskComplexControlMovement(pFormationTask);
				}

				break;
			}
		case eTaskDie:
			{
				CTask* pTaskDie=rage_new CTaskDyingDead(NULL, 0, 0);
				CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTaskDie);
				pFocusPed->GetPedIntelligence()->AddEvent(event);
				break;
			}
		case eSeekCover:
			{
				break;
			}
		case eUseMobilePhone:
			{
				pTaskToAdd = rage_new CTaskComplexUseMobilePhone();
				break;
			}
		case eHandsUp:
			{
				pTaskToAdd = rage_new CTaskHandsUp(20000);
				break;
			}
		case eDefendCurrentPosition:
			{
				Vector3 vPosition		= VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());
				float fHeading			= pFocusPed->GetTransform().GetHeading();
				//float fProximity		= 0.0f;

				//fHeading = ( RtoD * fHeading);

				pTaskToAdd = rage_new CTaskStandGuard(vPosition,fHeading,g_fProximity,g_bIsPatrolling?GM_PatrolProximity:GM_Stand, g_fTimer);
				break;
			}

		case eEvasiveStep:
			{
				pTaskToAdd = rage_new CTaskComplexEvasiveStep(pPlayer->GetMyVehicle(), true);
				break;
			}
		case eAvoidanceTest:
			{
				JamesDebugTest();
				break;
			}
		case eGetOffBoat:
			{
				pTaskToAdd = rage_new CTaskComplexGetOffBoat();
				break;
			}
		case eEnterNearestCar:
			{
				if( !pFocusPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
				{
					CVehicle* pNearestVehicle=pFocusPed->GetPedIntelligence()->GetClosestVehicleInRange();
					if( pNearestVehicle )
					{
						pTaskToAdd = rage_new CTaskEnterVehicle(pNearestVehicle, SR_Prefer, 1 );
					}
				}
			}
			break;
		case eShuffleBetweenSeats:
			if( pFocusPed->GetMyVehicle() )
			{
				pTaskToAdd=rage_new CTaskInVehicleSeatShuffle( pFocusPed->GetMyVehicle(), NULL );
			}
			break;
		case eExitNearestCar:
			if( pFocusPed->GetMyVehicle() )
			{
				VehicleEnterExitFlags vehicleFlags;
				pTaskToAdd = rage_new CTaskExitVehicle(pFocusPed->GetMyVehicle(), vehicleFlags );
			}
			break;
		case eClearCharTasks:
			pFocusPed->GetPedIntelligence()->ClearTasks(true, true);
			break;
		case eFlushCharTasks:
			pFocusPed->GetPedIntelligence()->FlushImmediately(true);
			break;
		case eDriveToLocator:
			if( pFocusPed->GetMyVehicle() )
			{
				Vector3 vPosition;
				if( CPhysics::GetMeasuringToolPos(0, vPosition) )
				{
					sVehicleMissionParams params;
					params.SetTargetPosition(vPosition);
					aiTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
					pTaskToAdd = rage_new CTaskControlVehicle(pFocusPed->GetMyVehicle(), pTask);
				}
			}
			break;
		case eGoDirectlyIntoCover:
			break;
		case eMobileChat:
			{
				//pTaskToAdd = rage_new CTaskMobileChatScenario(CScenarioManager::Wander_MobileConversation);
			}
			break;

		case eMobileChat2Way:
			{
				//pTaskToAdd = rage_new CTaskMobileChatScenario(CScenarioManager::Wander_2WayConversation);
			}
			break;

		case eShootAtPoint:
			{
				Vector3 vPos1;
				if( CPhysics::GetMeasuringToolPos(0, vPos1) )
				{
					pTaskToAdd = rage_new CTaskGun(CWeaponController::WCT_Fire, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(vPos1), 60.0f);
				}
			}
			break;

		case eGoToPointShooting:
			{
				Vector3 vPos1, vPos2;
				if( CPhysics::GetMeasuringToolPos(0, vPos1) && CPhysics::GetMeasuringToolPos(1, vPos2) )
				{
					pTaskToAdd = rage_new CTaskGoToPointAiming( CAITarget(vPos1), CAITarget(vPos2), g_fTaskMoveBlendRatio );
					static_cast<CTaskGoToPointAiming*>(pTaskToAdd)->SetFiringPattern(FIRING_PATTERN_BURST_FIRE);
				}
			}
			break;

		case eGoToPointShootingPlayer:
			{
				Vector3 vPos1, vPos2;
				if( CPhysics::GetMeasuringToolPos(0, vPos1) && CPhysics::GetMeasuringToolPos(1, vPos2) )
				{
					pTaskToAdd = rage_new CTaskGoToPointAiming( CAITarget(vPos1), CAITarget(CGameWorld::FindLocalPlayer()), g_fTaskMoveBlendRatio );
					static_cast<CTaskGoToPointAiming*>(pTaskToAdd)->SetFiringPattern(FIRING_PATTERN_BURST_FIRE);
				}
			}
			break;
		case eUseNearestScenario:
		case eUseNearestScenarioWarp:
			{
				Vector3 vPos1 = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());
				CPhysics::GetMeasuringToolPos(0, vPos1);
				scrVector vec;
				vec.x = vPos1.x;
				vec.y = vPos1.y;
				vec.z = vPos1.z;

				pTaskToAdd = task_commands::CommonUseNearestScenarioToPos( pFocusPed, vec, 2.0f,focusPedTask == eUseNearestScenarioWarp, true, 0 );
			}
			break;
		case eFallAndGetup:
			{
				pFocusPed->SetIsStanding(false);
				static dev_float fDownTime = 0.5f;
				pTaskToAdd = rage_new CTaskFallAndGetUp(CEntityBoundAI::REAR, fDownTime);
			}
			break;

		case eClimbUp:
			{
				//pTaskToAdd = rage_new CTaskClimb(NULL);
			}
			break;

		case eClimbVault:
			{
				Mat34V m(pFocusPed->GetMatrix());
				CClimbHandHoldDetected handHold;
				if(pFocusPed->GetPedIntelligence()->GetClimbDetector().SearchForHandHold(RCC_MATRIX34(m), handHold))
				{
					pTaskToAdd = rage_new CTaskVault(handHold);
				}
			}
			break;

		case eReactToCarCollision:
			{
				CPed * pPlayerPed = CGameWorld::FindLocalPlayer();

				if(pFocusPed->GetMyVehicle() && pPlayerPed && pPlayerPed->GetMyVehicle())
				{
					static float DAMAGE_DONE = 10.0f;

					pTaskToAdd = rage_new CTaskCarReactToVehicleCollision(pFocusPed->GetMyVehicle(), pPlayerPed->GetMyVehicle(), DAMAGE_DONE, VEC3V_TO_VECTOR3(pFocusPed->GetMyVehicle()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pFocusPed->GetMyVehicle()->GetTransform().GetForward()));
				}
			}
			break;

		case eNavMeshAndTurnToFacePedInSequence:
			{
				CPed * pPlayer = CGameWorld::FindLocalPlayer();
				if(pPlayer && pPlayer != pFocusPed)
				{
					CTaskSequenceList * pTaskSeq = rage_new CTaskSequenceList();

					CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(g_fTaskMoveBlendRatio, VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) + Vector3(6.0f, 0.0f, 0.0f));
					pTaskSeq->AddTask( rage_new CTaskComplexControlMovement(pNavTask) );
					CTaskTurnToFaceEntityOrCoord * pTurnTask = rage_new CTaskTurnToFaceEntityOrCoord(pPlayer);
					pTaskSeq->AddTask(pTurnTask);

					pTaskToAdd = rage_new CTaskUseSequence(*pTaskSeq);
				}
			}
			break;
		case eNMRelax:
			{
				CTask* pTaskNM = rage_new CTaskNMRelax(3000, 6000);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMPose:
			{
				//CTask* pTaskNM = rage_new CTaskNMPose(10000, 10000, CLIP_SET_JUMP_PED, CLIP_JUMP_LAND_SQUAT, 8.0f, 2.5f, CTaskNMPose::FULL_BODY, true);
				//SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
#if __DEV
		case eNMBindPose:
			{
				CTask* pTaskNM = rage_new CTaskNMBindPose(true);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
#endif	//	__DEV
		case eNMBrace:
			{
				// find nearest car to brace against?

				CTask* pTaskNM = rage_new CTaskNMBrace(2000, 10000, NULL);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMShot:
		case eNMMeleeHit:
			{
				Matrix34 rootMatrix;
				pFocusPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT);
				Vector3 vecForward = rootMatrix.a;
				vecForward.z = 0.0f;
				vecForward.NormalizeFast();

				Assert(pFocusPed->GetRagdollInst());
				phBoundComposite *bounds = pFocusPed->GetRagdollInst()->GetTypePhysics()->GetCompositeBounds();
				Assert(bounds);
				int nComponent = fwRandom::GetRandomNumberInRange(1, bounds->GetNumBounds()-1);
				if(focusPedTask==eNMMeleeHit)
					nComponent = 10;
				static int TEST_SHOT_HIT_COMPONENT = -1;
				if(TEST_SHOT_HIT_COMPONENT >= 0)
					nComponent = TEST_SHOT_HIT_COMPONENT;

				Matrix34 matComponent;
				pFocusPed->GetBoneMatrixFromRagdollComponent(matComponent, nComponent);
				Vector3 vecHitPos = matComponent.d;
				Vector3 vecHitNormal = vecForward;
				static float TEST_SHOT_HIT_FORCE_MULTIPLIER = -30.0f;
				Vector3 vecHitForce = TEST_SHOT_HIT_FORCE_MULTIPLIER * vecForward;

				WorldProbe::CShapeTestFixedResults<> probeResult;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&probeResult);
				probeDesc.SetStartAndEnd(vecHitPos+2.0f*vecHitNormal, vecHitPos-2.0f*vecHitNormal);
				probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
				probeDesc.SetIncludeEntity(pFocusPed);
				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
					vecHitPos = probeResult[0].GetHitPosition();
					vecHitNormal = probeResult[0].GetHitNormal();
				}

				u32 weaponHash = WEAPONTYPE_PISTOL;
				if(focusPedTask==eNMMeleeHit)
					weaponHash = pFocusPed->GetDefaultUnarmedWeaponHash();;

				CEntity* pEntityResponsible = NULL;
				if(pFocusPed!=CGameWorld::FindLocalPlayer() && Mag(pFocusPed->GetTransform().GetPosition() - CGameWorld::FindLocalPlayer()->GetTransform().GetPosition()).Getf() < 30.0f)
					pEntityResponsible = CGameWorld::FindLocalPlayer();

				CTask* pTaskNM = rage_new CTaskNMShot(pFocusPed, 1000, 10000, pEntityResponsible, weaponHash, nComponent, vecHitPos, vecHitForce, vecHitNormal);
				SwitchPedToNMBehaviour(pTaskNM);

				// ** NM will take care of applying the bullet impulse to NM agents **
				if (pFocusPed->GetRagdollInst()->m_AgentId == -1)
				{
					static bool sbUseBulletForce = true;
					if(sbUseBulletForce)
					{
						pFocusPed->GetRagdollInst()->ApplyBulletForce(pFocusPed, TEST_SHOT_HIT_FORCE_MULTIPLIER, vecForward, vecHitPos, nComponent, WEAPONTYPE_PISTOL);
					}
					else
					{
						// transform magnitude of impulse to equalize the effect over the body
						pFocusPed->GetRagdollInst()->ScaleImpulseByComponentMass(vecHitForce, nComponent, ms_fRagdollApplyForceFactor);
						pFocusPed->ApplyImpulse(vecHitForce, vecHitPos - VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), nComponent);
					}
				}
				grcDebugDraw::Line(vecHitPos, vecHitPos - 2.0f * vecForward, Color32(1.0f,0.0f,0.0f));
			}
			break;

		case eNMHighFall:
			{
				CTask* pTaskNM = rage_new CTaskNMHighFall(3000);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMBalance:
			{
				CTask* pTaskNM = rage_new CTaskNMBalance(3000, 6000, NULL, 0);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMBalanceGrab:
			{
				Vector3 vecLean(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetB()));
				vecLean.z = 0.0f;
				vecLean.NormalizeFast();

				CTask* pTaskNM = NULL;
				if(pFocusPed->IsPlayer())
					pTaskNM = rage_new CTaskNMBalance(3000, 6000, NULL, CGrabHelper::TARGET_AUTO_WHEN_STANDING);
				else
					pTaskNM = rage_new CTaskNMBalance(3000, 20000, NULL, CGrabHelper::TARGET_AUTO_WHEN_STANDING, &vecLean, 0.3f);

				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
#if ENABLE_DRUNK
		case eNMDrunk:
			{
				CTask* pTaskNM = rage_new CTaskNMDrunk(100000, 100000);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
#endif // ENABLE_DRUNK
		//case eNMStumble:
		//	{
		//		CTask* pTaskNM = rage_new CTaskNMStumble(3000, 30000);
		//		SwitchPedToNMBehaviour(pTaskNM);
		//	}
		//	break;
		case eNMBuoyancy:
			{
				CTask* pTaskNM = rage_new CTaskNMBuoyancy(3000, 30000);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMExplosion:
			{
				CTask* pTaskNM = rage_new CTaskNMExplosion(3000, 6000, VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()));
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMOnFire:
			{
				CTask* pTaskNM = rage_new CTaskNMOnFire(1000, 30000);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMFlinch:
			{
				// flinch target is either:
				// + the camera position, extended back along vector from player->cam
				// + the player's car, if focus is on a ped and the player has a car

				Vector3 dirToFocusEntFromCam(camInterface::GetPos());
				dirToFocusEntFromCam.Subtract(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()));

				Vector3 flinchTarget(camInterface::GetPos());
				flinchTarget.AddScaled(dirToFocusEntFromCam, 16.0f);

				CTask* pTaskNM = rage_new CTaskNMFlinch(3000, 6000, flinchTarget,
					((pPlayer != pFocusPed)?pPlayer->GetMyVehicle():NULL));

				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;
		case eNMRollUp:
			{
				ART::MessageParams msgRelax;
				msgRelax.addBool(NMSTR_PARAM(NM_START), true);
				pFocusPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ROLLUP_MSG), &msgRelax);
			}
			break;

		case eNMFallOverWall:
			{
				CTask* pTaskNM = rage_new CTaskNMFallDown(2000, 10000, CTaskNMFallDown::TYPE_OVER_WALL, VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetB()), pFocusPed->GetTransform().GetPosition().GetZf() - 3.0f);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;

		case eNMRiverRapids:
			{
				CTask* pTaskNM = rage_new CTaskNMRiverRapids(3000, 30000);
				SwitchPedToNMBehaviour(pTaskNM);
			}
			break;

		case eTreatAccident:
			{
				CPed * pPlayerPed = FindPlayerPed();

				const CEntityScannerIterator entityList = pPlayerPed->GetPedIntelligence()->GetNearbyPeds();
				for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
				{
					Assert(pEnt->GetType()==ENTITY_TYPE_PED);
					CPed* pClosePed=(CPed*)pEnt;
					if (pClosePed->IsInjured())
					{
						break;
					}
				}
			}
			break;
		case eDropOverEdge:
		{
#if 0
			CTaskDropOverEdge * pDropTask = rage_new CTaskDropOverEdge(pFocusPed->GetPosition(), pFocusPed->GetTransform().GetHeading());
			pTaskToAdd = pDropTask;
#endif
			break;
		}
		case eDragInjuredToSafety:
		{
			const CEntityScannerIterator entityList = pFocusPed->GetPedIntelligence()->GetNearbyPeds();
			for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
			{
				Assert(pEnt->GetType()==ENTITY_TYPE_PED);
				CPed* pClosePed=(CPed*)pEnt;
				if (pClosePed->IsInjured())
				{
					pTaskToAdd = rage_new CTaskDraggingToSafety(pClosePed, FindPlayerPed(),
						CTaskDraggingToSafety::CF_CoverNotRequired | CTaskDraggingToSafety::CF_DisableTargetTooClose);
					break;
				}
			}

			break;
		}
		case eVariedAimPose:
		{
			CAITarget target(FindPlayerPed());
			CTaskShootAtTarget* pShootTask = rage_new CTaskShootAtTarget(target, 0);

			pTaskToAdd = rage_new CTaskVariedAimPose(pShootTask);
			break;
		}
		case eCombat:
		{
			CTaskThreatResponse* pThreatTask = rage_new CTaskThreatResponse( FindPlayerPed() );
			Assert(pThreatTask);
			pThreatTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
			pTaskToAdd = pThreatTask;
			break;
		}
		case eCowerCrouched:
			{
				pFocusPed->GetPedIntelligence()->GetFleeBehaviour().SetStationaryReactHash(atHashWithStringNotFinal("CODE_HUMAN_COWER",0xD58CB510));
				CTaskCower* pCowerTask = rage_new CTaskCower(-1);
				pTaskToAdd = pCowerTask;
				break;
			}
		case eCowerStanding:
			{
				pFocusPed->GetPedIntelligence()->GetFleeBehaviour().SetStationaryReactHash(atHashWithStringNotFinal("CODE_HUMAN_STAND_COWER",0x161530CA));
				CTaskCower* pCowerTask = rage_new CTaskCower(-1);
				pTaskToAdd = pCowerTask;
				break;
			}
		case eShellShocked:
		{
			//Generate the position.
			Vec3V vPosition = pFocusPed->GetTransform().GetPosition();
			float fX = fwRandom::GetRandomNumberInRange(-1.0f, 1.0f);
			float fY = fwRandom::GetRandomNumberInRange(-1.0f, 1.0f);
			vPosition = Add(vPosition, Vec3V(fX, fY, 0.0f));

			//Generate the duration.
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			CTaskShellShocked::Duration nDuration = fRandom < 0.5f ? CTaskShellShocked::Duration_Short : CTaskShellShocked::Duration_Long;

			//Create the task.
			pTaskToAdd = rage_new CTaskShellShocked(vPosition, nDuration);
			break;
		}
		case eReachArm:
			{
				static Vec3V offsetVector(0.0f, 0.25f, 0.0f);
				static s32 taskTime = 20000;
				static float blendIn = 0.5f;
				static float blendOut = 0.4f;

				CTaskReachArm* pReachTask = rage_new CTaskReachArm(crIKSolverArms::LEFT_ARM, pFocusPed, BONETAG_SPINE2, offsetVector, AIK_TARGET_WRT_POINTHELPER, taskTime, blendIn, blendOut);
				fwMvClipSetId clipSet("move_injured_generic",0xBE27F702);
				fwMvClipId clip("run_turn_l2",0xDE3E7A4D);
				fwMvFilterId filter("BONEMASK_ARMONLY_L",0x67B52A94);
				pReachTask->SetClip(clipSet, clip, filter);
				pFocusPed->GetPedIntelligence()->AddTaskSecondary(pReachTask,PED_TASK_SECONDARY_PARTIAL_ANIM);
				break;
			}
		case eMoveFaceTarget:
			{
				if (Verifyf(pPlayer, "Could not find player ped!"))
				{
					CAITarget target(pPlayer);
					CTaskMoveFaceTarget* pTaskTarget = rage_new CTaskMoveFaceTarget(target);
					pTaskToAdd = rage_new CTaskComplexControlMovement(pTaskTarget);
				}
				break;
			}
		case eShockingBackAway:
			{
				if (Verifyf(pPlayer, "Could not find player ped!"))
				{
					CEventShockingExplosion eventShocking(pPlayer->GetTransform().GetPosition());
					pTaskToAdd = rage_new CTaskShockingEventBackAway(&eventShocking, CTaskShockingEventBackAway::GetDefaultBackAwayPositionForPed(*pFocusPed));
				}
				break;
			}
		case eShockingHurryAway:
			{
				if (Verifyf(pPlayer, "Could not find player ped!"))
				{
					CEventShockingExplosion eventShocking(pPlayer->GetTransform().GetPosition());
					pTaskToAdd = rage_new CTaskShockingEventHurryAway(&eventShocking);
				}
				break;
			}
		case eShockingReact:
			{
				if (Verifyf(pPlayer, "Could not find player ped!"))
				{
					CEventShockingExplosion eventShocking(pPlayer->GetTransform().GetPosition());
					pTaskToAdd = rage_new CTaskShockingEventReact(&eventShocking);
				}
				break;
			}
		case eSharkAttack:
			{
				if (Verifyf(pPlayer, "Could not find player ped!"))
				{
					pTaskToAdd = rage_new CTaskSharkAttack(pPlayer);
				}
				break;
			}
		case eSharkCircleForever:
			{
				if (Verifyf(pPlayer, "Could not find player ped!"))
				{
					CAITarget target;
					target.SetEntity(pPlayer);
					pTaskToAdd = rage_new CTaskComplexControlMovement(rage_new CTaskSharkCircle(g_fTaskMoveBlendRatio, target, 10.0f, CTaskSharkAttack::GetTunables().m_CirclingAngularSpeed));
				}
				break;
			}
		case eCallPolice:
			{
				if (Verifyf(pPlayer, "Could not find player ped!"))
				{
					pTaskToAdd = rage_new CTaskCallPolice(pPlayer);
				}
				break;
			}
		case eBeastJump:
			{
				s32 nFlags = JF_DisableVault | JF_SuperJump | JF_BeastJump;

				TUNE_GROUP_BOOL(PED_JUMPING, bAISuperJumpUseFullForce, false);
				if (bAISuperJumpUseFullForce)
					nFlags |= JF_AIUseFullForceBeastJump;

				pTaskToAdd=rage_new CTaskJumpVault(nFlags);
				break;
			}
		default:
			{
				continue;
			}
		}
		if( pTaskToAdd )
		{
			CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToAdd);
			pFocusPed->GetPedIntelligence()->AddEvent(event);
		}
	}
}


void
CPedDebugVisualiserMenu::OnSelectEventCombo(void)
{
	//Iterate over the focus entities.
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		//Ensure the entity is valid.
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(!pEnt || !pEnt->GetIsTypePed())
		{
			continue;
		}

		//Ensure the focus ped and event index are valid.
		CPed* pFocusPed = static_cast<CPed *>(pEnt);
		if(!pFocusPed || m_iEventComboIndex >= m_EventComboTypes.GetCount())
		{
			continue;
		}

		//Find the player.
		CPed* pPlayer = FindPlayerPed();

		//Keep track of the event to add.
		CEvent* pEventToAdd = NULL;

		//Generate the event to add.
		eFocusPedEvents focusPedEvent = m_EventComboTypes[m_iEventComboIndex];
		switch(focusPedEvent)
		{
			case eCarUndriveable:
			{
				pEventToAdd = rage_new CEventCarUndriveable(pFocusPed->GetVehiclePedInside());
				break;
			}
			case eClimbNavMeshOnRoute:
			{
				pEventToAdd = rage_new CEventClimbNavMeshOnRoute(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), pFocusPed->GetCurrentHeading(),
					VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), MOVEBLENDRATIO_WALK, 0, NULL, 5000,
					VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()));
				break;
			}
			case eGunshot:
			{
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventGunShot(pPlayer, VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()),
						VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), false, 0);
				}
				break;
			}
			case ePotentialGetRunOver:
			{
				if (pPlayer && pPlayer->GetVehiclePedInside())
				{
					pEventToAdd = rage_new CEventPotentialGetRunOver(pPlayer->GetVehiclePedInside());
				}
				break;
			}
			case eRanOverPed:
			{
				if (pPlayer && pFocusPed->GetIsDrivingVehicle())
				{
					pEventToAdd = rage_new CEventRanOverPed(pFocusPed->GetVehiclePedInside(), pPlayer, 0.0f);
				}
				break;
			}
			case eScriptCommand:
			{
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventScriptCommand(PED_TASK_PRIORITY_PRIMARY, rage_new CTaskSmartFlee(CAITarget(pPlayer)));
				}
				break;
			}
			case eShockingCarCrash:
			{
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventShockingCarCrash(*pPlayer, pPlayer);
				}
				break;
			}
			case eShockingBicycleCrash:
			{
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventShockingBicycleCrash(*pPlayer, pPlayer);
				}
				break;
			}
			case eShockingDrivingOnPavement:
			{
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventShockingDrivingOnPavement(*pPlayer);
				}
				break;
			}
			case eShockingDeadBody:
			{
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventShockingDeadBody(*pPlayer);
				}
				break;
			}
			case eShockingRunningPed:
			{
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventShockingRunningPed(*pPlayer);
				}
				break;
			}
			case eShockingGunShotFired:
			{
				//Ensure the player is valid.
				if(pPlayer)
				{
					//Generate the event to add.
					pEventToAdd = rage_new CEventShockingGunshotFired(*pPlayer);
				}

				break;
			}
			case eShockingWeaponThreat:
			{
				//Ensure the player is valid.
				if(pPlayer)
				{
					//Generate the event to add.
					pEventToAdd = rage_new CEventShockingWeaponThreat(*pPlayer, pFocusPed);
				}

				break;
			}
			case eShockingCarStolen:
			{
				//Ensure the player is valid.
				if (pPlayer)
				{
					if (pPlayer->GetVehiclePedInside())
					{
						pEventToAdd = rage_new CEventShockingSeenCarStolen(*pPlayer, pPlayer->GetVehiclePedInside());
					}
					else
					{
						Warningf("Player ped must be in a vehicle to use this widget.");
					}
				}

				break;
			}
			case eShockingInjuredPed:
			{
				//Ensure the player is valid.
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventShockingInjuredPed(*pPlayer, pPlayer);
				}
				break;
			}
			case eShockingGunfight1:
			{
				// Gunfight with a source of the player.
				if (pPlayer)
				{
					CEventShockingGunshotFired gunshotEvent(*pPlayer);
					pEventToAdd = rage_new CEventShockingGunFight();
					gunshotEvent.InitClone(*static_cast<CEventShocking*>(pEventToAdd), false);
				}
				break;
			}
			case eShockingGunfight2:
			{
				// Gunfight with no source.
				pEventToAdd = rage_new CEventShockingGunFight();
				break;
			}
			case eShockingSeenPedKilled:
			{
				// Seen ped killed with a source of the player.
				if (pPlayer)
				{
					pEventToAdd = rage_new CEventShockingSeenPedKilled(*pPlayer, pPlayer);
				}
				break;
			}
			default:
			{
				break;
			}
		}

		//Check if there is an event to add.
		if(pEventToAdd)
		{
			//Give the focus ped the event.
			pFocusPed->GetPedIntelligence()->AddEvent(*pEventToAdd);

			//Free the event.
			delete pEventToAdd;
		}
	}
}




void
CPedDebugVisualiserMenu::OnSelectFormationCombo()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			CPedGroup * pGroup = pFocusPed->GetPedsGroup();
			if(!pGroup)
				return;

			Assert(pGroup->GetFormation());
			Assert(m_iFormationComboIndex >= 0 && m_iFormationComboIndex < CPedFormationTypes::NUM_FORMATION_TYPES);
			pGroup->SetFormation(m_iFormationComboIndex);
		}
	}
}



void CPedDebugVisualiserMenu::SwitchPedToNMBehaviour(CTask* pTaskNM)
{
#if __DEV
	if(!CPedPopulation::ms_bAllowRagdolls)
		return;
#endif

	if( CDebugScene::FocusEntities_Get(0) && CDebugScene::FocusEntities_Get(0)->GetIsTypePed() )
	{
		CPed* pFocusPed = static_cast<CPed*>(CDebugScene::FocusEntities_Get(0));

		if( nmVerifyf(CTaskNMBehaviour::CanUseRagdoll(pFocusPed, RAGDOLL_TRIGGER_DEBUG), "CPedDebugVisualiserMenu::SwitchPedToNMBehaviour: Not allowed to ragdoll") )
		{
			if(pFocusPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				pFocusPed->SetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle, false );
				pFocusPed->UpdateSpatialArrayTypeFlags();

				pFocusPed->EnableCollision();



				if(pTaskNM==NULL)
					pTaskNM = rage_new CTaskNMBalance(1000, 10000, NULL, 0);
				CEventSwitch2NM event(10000, pTaskNM);
				pTaskNM = NULL;

				pFocusPed->SwitchToRagdoll(event);

				pFocusPed->ApplyImpulse(300.0f*VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetC()) - 50.0f*VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetA()), Vector3(0.0f,0.0f,0.0f));
				pFocusPed->SetNoCollision(pFocusPed->GetMyVehicle(), NO_COLLISION_RESET_WHEN_NO_IMPACTS);
			}
			else
			{


				if(pTaskNM==NULL)
					pTaskNM = rage_new CTaskNMBalance(1000, 10000, NULL, 0);
				CEventSwitch2NM event(10000, pTaskNM);
				pTaskNM = NULL;

				pFocusPed->SwitchToRagdoll(event);
			}
		}
	}

	if(pTaskNM)
		delete pTaskNM;
}


void CPedDebugVisualiserMenu::SwitchPedToAnimatedCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
				CTask *pActiveTask = pFocusPed->GetPedIntelligence()->GetTaskActiveSimplest();
				CTaskNMBehaviour* pTaskNM = pActiveTask->IsNMBehaviourTask() ? (CTaskNMBehaviour *) pActiveTask : NULL;
				if(pTaskNM)
				{
					pTaskNM->ForceTimeout();
				}
			}
			else if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_ANIM_DRIVEN)
			{
				pFocusPed->SwitchToAnimated();
			}
		}
	}
}

void CPedDebugVisualiserMenu::SwitchPedToDrivenRagdollCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_ANIM)
			{
				pFocusPed->SwitchToAnimDrivenRagdoll();
			}
		}
	}
}



void CPedDebugVisualiserMenu::ForcePedToAnimatedCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
				Vector3 vecPos = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());
				vecPos.z += 0.8f;
				pFocusPed->SetPosition(vecPos);

				float fNewHeading = pFocusPed->GetTransform().GetHeading();
				pFocusPed->SetDesiredHeading(fNewHeading);
				pFocusPed->SetHeading(fNewHeading);

				pFocusPed->SwitchToAnimated(true, true);
			}
		}
	}
}

void CPedDebugVisualiserMenu::SwitchToStaticFrameCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
				// Switch the ped AI state back to animation, and get the capsule back to the correct orientation
				pFocusPed->SwitchToAnimated(true, true, true, false, false, true, true);
			}
		}
	}
}

void CPedDebugVisualiserMenu::ClearAllBehavioursCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
				if(pFocusPed->GetRagdollInst()->GetNMAgentID()!=-1)
				{
					// stop all currently running behaviours before we start new ones
					ART::MessageParams msg;
					pFocusPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STOP_ALL_MSG), &msg);
				}
			}
		}
	}
}

void CPedDebugVisualiserMenu::SetRelaxMessageCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
				if(pFocusPed->GetRagdollInst()->GetNMAgentID()!=-1)
				{
					ART::MessageParams msg;
					msg.addBool(NMSTR_PARAM(NM_START), true);
					msg.addFloat(NMSTR_PARAM(NM_RELAX_RELAXATION), 97.0);
					pFocusPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_RELAX_MSG), &msg);
				}
			}
		}
	}
}


void CPedDebugVisualiserMenu::UnlockPedRagdollCB(void)
{
#if __DEV
	if(!CPedPopulation::ms_bAllowRagdolls)
		return;
#endif
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->GetRagdollState()==RAGDOLL_STATE_ANIM_LOCKED)
			{
				pFocusPed->SetRagdollState(RAGDOLL_STATE_ANIM);
			}
		}
	}
}


//namespace misc_commands{void CommandFireSingleBullet(float StartX, float StartY, float StartZ, float EndX, float EndY, float EndZ, int DamageCaused);};
//namespace ped_commands{void CommandSwitchToRagdoll(int PedIndex, int MinTime, int MaxTime, bool AddStumbleBehaviour);};

void CPedDebugVisualiserMenu::TriggerRagdollTestCodeCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			//CPed* pFocusPed = static_cast<CPed*>(pEnt);

			/*		// testing deactivating the ragdoll and then immediately reactivating it.
			if(m_pFocus && m_pFocus->GetRagdollState()==RAGDOLL_STATE_PHYS)
			{
			Vector3 vecPos = m_pFocus->GetPosition();
			vecPos.z += 0.8f;
			m_pFocus->SetPosition(vecPos, true);

			float fNewHeading = m_pFocus->GetTransform().GetHeading();
			m_pFocus->SetCurrentHeading(fNewHeading);
			m_pFocus->SetDesiredHeading(fNewHeading);
			m_pFocus->CEntity::SetHeading(fNewHeading);

			m_pFocus->SwitchToAnimated();
			m_pFocus->SwitchToRagdoll();
			}
			*/
			/*
			if(pPedVarDebugPed->m_pAttachToEntity)
			{
			pPedVarDebugPed->DettachPedFromEntity();
			}
			else
			{
			CGameWorld::CFindData results;
			CGameWorld::ForAllEntitiesIntersecting(spdSphere(10.0f, pPedVarDebugPed->GetPosition()), CGameWorld::FindEntitiesCB, &results,
			ENTITY_TYPE_MASK_VEHICLE, SEARCH_LOCATION_EXTERIORS);
			if(results.GetNumEntities() > 0)
			{
			pPedVarDebugPed->AttachPedToEntity(results.GetEntity(0), Vector3(2.0f, 2.0f, 1.0f), 0, 0.0f);
			}
			}
			*/
			/*
			int nPedPoolIndex = CTheScripts::GetGUIDFromEntity(m_pFocus);
			ped_commands::CommandSwitchToRagdoll(nPedPoolIndex, 2000, 10000, true);
			*/
		}
	}

	//Vector3 vecStart = camInterface::GetPos();
	//Vector3 vecEnd = vecStart + 50.0f*camInterface::GetFront();
	//misc_commands::CommandFireSingleBullet(vecStart.x, vecStart.y, vecStart.z, vecEnd.x, vecEnd.y, vecEnd.z, 10);
}

void CPedDebugVisualiserMenu::RagdollContinuousDebugCode(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			static Vector3 vecApplyForceToFocusPed(-1500.0f, 0.0f, -1000.0f);

			if(pFocusPed->GetRagdollInst()->GetNMAgentID()!=-1)
			{
				static bool sbTriggerShotBehaviour = false;
				if(sbTriggerShotBehaviour)
				{

				}
				else
				{
					static bool sbSendStopMsg = false;
					if(sbSendStopMsg)
					{
						ART::MessageParams msgStopAll;
						pFocusPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STOP_ALL_MSG), &msgStopAll);
					}
					static bool sbSendRelaxMsg = false;
					if(sbSendRelaxMsg)
					{
						ART::MessageParams msgRelax;
						msgRelax.addBool(NMSTR_PARAM(NM_START), true);
						msgRelax.addFloat(NMSTR_PARAM(NM_RELAX_RELAXATION), 97.0);
						pFocusPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_RELAX_MSG), &msgRelax);
					}
				}
			}
			else if(vecApplyForceToFocusPed.x != 0.0f)
			{
				pFocusPed->ApplyForceCg(vecApplyForceToFocusPed);
			}
		}
	}
}

void CPedDebugVisualiserMenu::FixPedPhysicsCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			if(pFocusPed->IsBaseFlagSet(fwEntity::IS_FIXED))
				pFocusPed->SetFixedPhysics(false);
			else
				pFocusPed->SetFixedPhysics(true);
		}
	}
}

void CPedDebugVisualiserMenu::DropWeaponCB(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			//			Vector3 velocity = pFocusPed ->GetVelocity();
			//			Vector3 rotationalvelocity = pFocusPed ->GetTurnSpeed();
			//			rotationalvelocity += pFocusPed ->GetA() * -0.1f;
			//			pFocusPed ->m_PedWeapons.DropWeapon(&velocity, &rotationalvelocity);

			CPedWeaponManager *pPedWeaponMgr = pFocusPed->GetWeaponManager();
			if(pPedWeaponMgr)
				pPedWeaponMgr->DropWeapon(pPedWeaponMgr->GetEquippedWeaponHash(), true);
		}
	}
}


//-------------------------------------------------------------------------
// Returns true if ped dragging is enabled
//-------------------------------------------------------------------------
bool CPedDebugVisualiserMenu::IsDebugPedDraggingActivated(void)
{
	return (CPedDebugVisualiser::eDisplayDebugInfo == CPedDebugVisualiser::ePedDragging);
}

static RegdPed g_pMovingPed(NULL);

//-------------------------------------------------------------------------
// Updates ped dragging
//-------------------------------------------------------------------------
void CPedDebugVisualiserMenu::UpdatePedDragging(void)
{
	Vector3 MousePointNear, MousePointFar;


	if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		if( !g_pMovingPed )
		{
			CEntity* pEntity = CDebugScene::GetEntityUnderMouse();
			if(pEntity && pEntity->GetIsTypePed())
			{
				g_pMovingPed = (CPed*)pEntity;
				if(g_pMovingPed)
				{
					if(!CDebugScene::GetWorldPositionUnderMouse(LastFrameWorldPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE))
					{
						return ;
					}
				}
			}
		}
	}
	else if( !(ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT) )
	{
		Vector3	UpdateMousePos, vPedPosition, vFinalScreenPosition, FinalPedPosition, vLastFrameScreenPos, vThisFrameScreenPos;
		//grab the position of where mouse
		if(g_pMovingPed && CDebugScene::GetWorldPositionUnderMouse(ThisFrameWorldPos,  ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE))
		{
			//check that the mouse has moved this frame
			int ThisFrameMouseX = ioMouse::GetX();
			int ThisFrameMouseY = ioMouse::GetY();

			float fCurrentHeading = g_pMovingPed->GetTransform().GetHeading();

			if (ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT)
			{
				if(ioMapper::DebugKeyDown(KEY_LSHIFT))
				{
					fCurrentHeading += 0.1f;
				}
				else
				{
					fCurrentHeading -= 0.1f;
				}
				g_pMovingPed->SetHeading(fCurrentHeading);
				g_pMovingPed->SetDesiredHeading( fCurrentHeading );

			}

				CDebugScene::GetScreenPosFromWorldPoint(ThisFrameWorldPos, vThisFrameScreenPos.x, vThisFrameScreenPos.y);				//convert our second point to screen
				CDebugScene::GetScreenPosFromWorldPoint(LastFrameWorldPos, vLastFrameScreenPos.x, vLastFrameScreenPos.y);			//convert our first point to screen


				CDebugScene::GetScreenPosFromWorldPoint(VEC3V_TO_VECTOR3(g_pMovingPed->GetTransform().GetPosition())-Vector3(0.0f, 0.0f, 1.0f), vPedPosition.x, vPedPosition.y);	//convert our the ped point on screen, get his feet pos for correct line probe test

				vFinalScreenPosition = vPedPosition + (vThisFrameScreenPos - vLastFrameScreenPos );										//convert final position in screen space

				int tempfinalx = rage::round (vFinalScreenPosition.x);
				int tempfinaly = rage::round (vFinalScreenPosition.y);

				vFinalScreenPosition.y = float (tempfinaly);
				vFinalScreenPosition.x = float (tempfinalx);

				if (CDebugScene::GetWorldPositionFromScreenPos(vFinalScreenPosition.x, vFinalScreenPosition.y, FinalPedPosition, ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE))
				{
					//the mouse position has moved so update the ped
					if (!(ThisFrameMouseX == LastFrameMouseX && ThisFrameMouseY == LastFrameMouseY))
					{
						g_pMovingPed->Teleport(FinalPedPosition+Vector3(0.0f, 0.0f, 1.0f), fCurrentHeading, true);
					}
					 LastFrameWorldPos = ThisFrameWorldPos;
					 LastFrameMouseX = ThisFrameMouseX;
					 LastFrameMouseY = ThisFrameMouseY;
				}
		}
	}
	else
	{
		if( g_pMovingPed )
		{
			g_pMovingPed = NULL;
		}
	}

}


//-------------------------------------------------------------------------
// Debugging tool that lets you shoot peds with the mouse
// and switch to NM Shot behaviour
//-------------------------------------------------------------------------

bool CPedDebugVisualiserMenu::IsNMInteractiveShooterActivated(void)
{
	return (CPedDebugVisualiser::eDebugNMMode == CPedDebugVisualiser::eNMInteractiveShooter);
}

void CPedDebugVisualiserMenu::UpdateNMInteractiveShooter(void)
{
	CEntity* pEntity = CDebugScene::GetEntityUnderMouse();

	if(pEntity && pEntity->GetIsTypePed())
	{
		CPed* pShotPed = (CPed*)pEntity;

		Vector3 vMouseNear, vMouseFar;
		CDebugScene::GetMousePointing(vMouseNear, vMouseFar);

		Vector3 vecHitPos;
		Vector3 vecHitNormal;
		u16 nComponent;

		WorldProbe::CShapeTestFixedResults<> probeResult;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(vMouseNear, vMouseFar);
		probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		probeDesc.SetIncludeEntity(pShotPed);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			vecHitPos = probeResult[0].GetHitPosition();
			vecHitNormal = probeResult[0].GetHitNormal();
			nComponent = probeResult[0].GetHitComponent();

			grcDebugDraw::Line(vecHitPos, vecHitPos + vecHitNormal, Color32(1.0f,0.0f,0.0f), Color32(1.0f,1.0f,0.0f));

			if (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT)
			{
				Vector3 vecHitForce = -CPedDebugVisualiserMenu::ms_fNMInteractiveShooterShotForce * vecHitNormal;

				CEntity* pSavedFocus = CDebugScene::FocusEntities_Get(0);
				{
					CDebugScene::FocusEntities_Set(pShotPed, 0);
					CTask* pTaskNM = rage_new CTaskNMShot(pShotPed, 1000, 10000, CGameWorld::FindLocalPlayer(), WEAPONTYPE_PISTOL, nComponent, vecHitPos, vecHitForce, vecHitNormal);
					SwitchPedToNMBehaviour(pTaskNM);
				}
				CDebugScene::FocusEntities_Set(pSavedFocus, 0);

				// ** NM will take care of applying the bullet impulse to NM agents **
				if (pShotPed->GetRagdollInst()->m_AgentId == -1)
				{
					// transform magnitude of impulse to equalize the effect over the body
					pShotPed->GetRagdollInst()->ScaleImpulseByComponentMass(vecHitForce, nComponent, ms_fRagdollApplyForceFactor);
					pShotPed->ApplyImpulse(vecHitForce, vecHitPos - VEC3V_TO_VECTOR3(pShotPed->GetTransform().GetPosition()), nComponent);
				}
			}
		}
	}
}

void CPedDebugVisualiserMenu::ProcessRouteRoundCarTest()
{
	static bool bActive = false;
	Assert(!bActive);
	if(!bActive)
	{
		bActive = true;

		CPed * pPlayer = FindPlayerPed();
		CVehicle * pClosestVehicle = NULL;
		float fClosestDistSqr = FLT_MAX;

		const CEntityScannerIterator entityList = pPlayer->GetPedIntelligence()->GetNearbyVehicles();
		for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
		{
			CVehicle * pVehicle = (CVehicle*)pEnt;

			const float f2 = MagSquared(pVehicle->GetTransform().GetPosition()-pPlayer->GetTransform().GetPosition()).Getf();
			if(f2 < fClosestDistSqr)
			{
				fClosestDistSqr = f2;
				pClosestVehicle = pVehicle;
			}
		}

		if(pClosestVehicle)
		{
			Vector3 vWorldHitPos;
			if(CDebugScene::GetWorldPositionUnderMouse(vWorldHitPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER))
			{
				vWorldHitPos.z += 1.0f;

				CPointRoute route;
				CRouteToDoorCalculator routeCalc(pPlayer, VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), vWorldHitPos, pClosestVehicle, &route);
				if(routeCalc.CalculateRouteToDoor())
				{
					for(int p=1; p<route.GetSize(); p++)
					{
						grcDebugDraw::Line(route.Get(p-1), route.Get(p), Color_VioletRed, Color_VioletRed);
					}
				}
			}
		}

		bActive = false;
	}
}

void ShapeTestError_PS3()
{
#if __ASSERT
	static Vector3 vStart(-29.0f, -26.0f, 13.0f);
	static Vector3 vEnd(-36.0f, -26.0f, 13.0f);

	CPhysics::g_vClickedPos[0] = vStart;
	CPhysics::g_vClickedNormal[0] = ZAXIS;
	CPhysics::g_pPointers[0] = NULL;
	CPhysics::g_bHasPosition[0] = true;
	CPhysics::g_vClickedPos[1] = vEnd;
	CPhysics::g_vClickedNormal[1] = ZAXIS;
	CPhysics::g_pPointers[1] = NULL;
	CPhysics::g_bHasPosition[1] = true;
	CPhysics::ms_bDebugMeasuringTool = true;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	bool bDirectLosClear = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

	WorldProbe::CShapeTestFixedResults<> probeResults;
	WorldProbe::CShapeTestProbeDesc probe;
	probe.SetResultsStructure(&probeResults);
	probe.SetStartAndEnd(vStart,vEnd);
	probe.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probe.SetOptions(0);
	probe.SetIsDirected(true);
	WorldProbe::GetShapeTestManager()->SubmitTest(probe, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

	if(probeResults.GetResultsStatus() != WorldProbe::SUBMISSION_FAILED)
	{
		while(!probeResults.GetResultsReady()) { sysIpcYield(0); }

		bool bAsyncLosClear = probeResults.GetNumHits() == 0u;

		Assert(bDirectLosClear == bAsyncLosClear);
	}
#endif // __ASSERT
}

//-------------------------------------------------------------------------
// Debugging tool that lets you choose a point in the world for the focus
// ped to wander over to and try and grab at
//-------------------------------------------------------------------------

struct NMGrabberState
{
	Vector3	m_vecPos1,
		m_vecPos2,
		m_vecNorm1,
		m_vecNorm2,
		m_vecPos3,
		m_vecPos4;

	enum
	{
		ePickOne,
		ePickTwo,
		ePickThree,
		ePickFour
	}	m_eState;

	CEntity*	m_pEntity;

	NMGrabberState() : m_eState(ePickOne), m_pEntity(NULL) {}
};
static NMGrabberState	g_grabberState;

bool CPedDebugVisualiserMenu::IsNMInteractiveGrabberActivated(void)
{
	return (CPedDebugVisualiser::eDebugNMMode == CPedDebugVisualiser::eNMInteractiveGrabber);
}


void CPedDebugVisualiserMenu::UpdateNMInteractiveGrabber(void)
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);

			bool useSurfaceGrab = (ioMapper::DebugKeyDown(KEY_CONTROL)) != 0;	// default off
			bool useLineGrab	= (ioMapper::DebugKeyDown(KEY_SHIFT) == 0);	// default on

			const float sphereMarkerSz = 0.05f;

			const Color32	cPrimary(0.5f,1.0f,useSurfaceGrab?0.5f:0.0f),
				cSecondary(1.0f,0.5f,useSurfaceGrab?0.5f:0.0f);

			s32 flags = (ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);


			Vector3 leanViz(g_grabberState.m_vecPos1);
			if (g_grabberState.m_eState >= NMGrabberState::ePickTwo)
			{
				leanViz.Add(g_grabberState.m_vecPos2);

				if (g_grabberState.m_eState >= NMGrabberState::ePickThree)
					leanViz.Add(g_grabberState.m_vecPos3);
				if (g_grabberState.m_eState >= NMGrabberState::ePickFour)
					leanViz.Add(g_grabberState.m_vecPos4);

				leanViz.Scale(1.0f / (float)(g_grabberState.m_eState + 1) );
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()), leanViz, Color32(0.0f, 1.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f));
			}


			if (g_grabberState.m_eState == NMGrabberState::ePickOne)
			{
				if(CDebugScene::GetWorldPositionUnderMouse(g_grabberState.m_vecPos1, flags, &g_grabberState.m_vecNorm1))
				{
					grcDebugDraw::Sphere(g_grabberState.m_vecPos1, sphereMarkerSz, cPrimary, true);

					g_grabberState.m_pEntity = CDebugScene::GetEntityUnderMouse();

					if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
					{
						g_grabberState.m_eState = NMGrabberState::ePickTwo;
					}
				}
			}
			else if (g_grabberState.m_eState == NMGrabberState::ePickTwo)
			{
				if (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_MIDDLE)
				{
					g_grabberState.m_eState = NMGrabberState::ePickOne;
					return;
				}

				grcDebugDraw::Sphere(g_grabberState.m_vecPos1, sphereMarkerSz, cSecondary, true);

				if(CDebugScene::GetWorldPositionUnderMouse(g_grabberState.m_vecPos2, flags, &g_grabberState.m_vecNorm2))
				{
					grcDebugDraw::Sphere(g_grabberState.m_vecPos2, sphereMarkerSz, cPrimary, true);

					if (useLineGrab)
					{
						Vector3 line1(g_grabberState.m_vecPos1),
							line2(g_grabberState.m_vecPos2);

						line1.AddScaled(g_grabberState.m_vecNorm1, 0.075f);
						line2.AddScaled(g_grabberState.m_vecNorm2, 0.075f);
						grcDebugDraw::Line(line1, line2, cSecondary, cPrimary);
					}

					if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
					{
						if (useSurfaceGrab)
						{
							g_grabberState.m_eState = NMGrabberState::ePickThree;
							return;
						}
						else
							g_grabberState.m_eState = NMGrabberState::ePickOne;

						// lean direction is XY from focus ped position -> center of picked line
						Vector3 leanDir = (g_grabberState.m_vecPos1 + g_grabberState.m_vecPos2);
						leanDir.Scale(0.5f);
						leanDir.Subtract(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()));
						leanDir.z = 0.0f;
						leanDir.NormalizeFast();

						// if we dragged across the same entity, use it and shift the positions
						// into it's local-space... otherwise null it out and use world-space positions
						CEntity* pEntUnderMouse = CDebugScene::GetEntityUnderMouse();
						if (g_grabberState.m_pEntity != pEntUnderMouse)
							g_grabberState.m_pEntity = NULL;
						else
						{
							const fwTransform& t = g_grabberState.m_pEntity->GetTransform();
							g_grabberState.m_vecPos1 = VEC3V_TO_VECTOR3(t.UnTransform(VECTOR3_TO_VEC3V(g_grabberState.m_vecPos1)));
							g_grabberState.m_vecNorm1 = VEC3V_TO_VECTOR3(t.UnTransform3x3(VECTOR3_TO_VEC3V(g_grabberState.m_vecNorm1)));
							g_grabberState.m_vecPos2 = VEC3V_TO_VECTOR3(t.UnTransform(VECTOR3_TO_VEC3V(g_grabberState.m_vecPos2)));
							g_grabberState.m_vecNorm2 = VEC3V_TO_VECTOR3(t.UnTransform3x3(VECTOR3_TO_VEC3V(g_grabberState.m_vecNorm2)));
						}

						CTaskNMBalance* pTaskNM = rage_new CTaskNMBalance(3000, 20000, NULL, CGrabHelper::TARGET_SPECIFIC, &leanDir, CPedDebugVisualiserMenu::ms_fNMInteractiveGrabLeanAmount);
						if (pTaskNM->GetGrabHelper())
						{
							pTaskNM->GetGrabHelper()->SetGrabTarget(
								g_grabberState.m_pEntity,
								&g_grabberState.m_vecPos1,
								&g_grabberState.m_vecNorm1/*,
														  &g_grabberState.m_vecPos2,
														  &g_grabberState.m_vecNorm2*/);
						}

						if (useLineGrab && pTaskNM->GetGrabHelper())
							pTaskNM->GetGrabHelper()->SetFlag(CGrabHelper::TARGET_LINE_GRAB);

						SwitchPedToNMBehaviour(pTaskNM);
					}
				}
			}
			else if (g_grabberState.m_eState == NMGrabberState::ePickThree)
			{
				if (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_MIDDLE)
				{
					g_grabberState.m_eState = NMGrabberState::ePickOne;
					return;
				}

				grcDebugDraw::Sphere(g_grabberState.m_vecPos1, sphereMarkerSz, cSecondary, true);
				grcDebugDraw::Sphere(g_grabberState.m_vecPos2, sphereMarkerSz, cSecondary, true);

				if(CDebugScene::GetWorldPositionUnderMouse(g_grabberState.m_vecPos3, flags))
				{
					grcDebugDraw::Sphere(g_grabberState.m_vecPos3, sphereMarkerSz, cPrimary, true);

					Vector3 poly1(g_grabberState.m_vecPos1),
						poly2(g_grabberState.m_vecPos2),
						poly3(g_grabberState.m_vecPos3);

					poly1.AddScaled(g_grabberState.m_vecNorm1, 0.075f);
					poly2.AddScaled(g_grabberState.m_vecNorm2, 0.075f);
					poly3.AddScaled(g_grabberState.m_vecNorm2, 0.075f);

					grcDebugDraw::Line(poly1, poly2, cSecondary);
					grcDebugDraw::Line(poly2, poly3, cSecondary, cPrimary);
					grcDebugDraw::Line(poly3, poly1, cPrimary, cSecondary);


					if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
					{
						CEntity* pEntUnderMouse = CDebugScene::GetEntityUnderMouse();
						if (g_grabberState.m_pEntity != pEntUnderMouse)
							g_grabberState.m_pEntity = NULL;

						g_grabberState.m_eState = NMGrabberState::ePickFour;
						return;
					}
				}
			}
			else if (g_grabberState.m_eState == NMGrabberState::ePickFour)
			{
				if (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_MIDDLE)
				{
					g_grabberState.m_eState = NMGrabberState::ePickOne;
					return;
				}

				grcDebugDraw::Sphere(g_grabberState.m_vecPos1, sphereMarkerSz, cSecondary, true);
				grcDebugDraw::Sphere(g_grabberState.m_vecPos2, sphereMarkerSz, cSecondary, true);
				grcDebugDraw::Sphere(g_grabberState.m_vecPos3, sphereMarkerSz, cSecondary, true);

				if(CDebugScene::GetWorldPositionUnderMouse(g_grabberState.m_vecPos4, flags))
				{
					grcDebugDraw::Sphere(g_grabberState.m_vecPos4, sphereMarkerSz, cPrimary, true);

					Vector3 poly1(g_grabberState.m_vecPos1),
						poly2(g_grabberState.m_vecPos2),
						poly3(g_grabberState.m_vecPos3),
						poly4(g_grabberState.m_vecPos4);

					poly1.AddScaled(g_grabberState.m_vecNorm1, 0.075f);
					poly2.AddScaled(g_grabberState.m_vecNorm2, 0.075f);
					poly3.AddScaled(g_grabberState.m_vecNorm2, 0.075f);
					poly4.AddScaled(g_grabberState.m_vecNorm1, 0.075f);

					grcDebugDraw::Line(poly1, poly2, cSecondary);
					grcDebugDraw::Line(poly2, poly3, cSecondary);
					grcDebugDraw::Line(poly3, poly4, cSecondary, cPrimary);
					grcDebugDraw::Line(poly4, poly1, cPrimary, cSecondary);

					if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
					{
						g_grabberState.m_eState = NMGrabberState::ePickOne;

						CEntity* pEntUnderMouse = CDebugScene::GetEntityUnderMouse();
						if (g_grabberState.m_pEntity != pEntUnderMouse)
							g_grabberState.m_pEntity = NULL;

						// lean direction is XY from focus ped position -> center of picked poly
						Vector3 leanDir(g_grabberState.m_vecPos1);
						leanDir.Add(g_grabberState.m_vecPos2);
						leanDir.Add(g_grabberState.m_vecPos3);
						leanDir.Add(g_grabberState.m_vecPos4);
						leanDir.Scale(0.25f);
						leanDir.Subtract(VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition()));
						leanDir.z = 0.0f;
						leanDir.NormalizeFast();

						// if we dragged across the same entity, use it and shift the positions
						// into it's local-space... otherwise null it out and use world-space positions
						if (g_grabberState.m_pEntity != NULL)
						{
							const fwTransform& t = g_grabberState.m_pEntity->GetTransform();
							g_grabberState.m_vecPos1 = VEC3V_TO_VECTOR3(t.UnTransform(VECTOR3_TO_VEC3V(g_grabberState.m_vecPos1)));
							g_grabberState.m_vecNorm1 = VEC3V_TO_VECTOR3(t.UnTransform3x3(VECTOR3_TO_VEC3V(g_grabberState.m_vecNorm1)));
							g_grabberState.m_vecPos2 = VEC3V_TO_VECTOR3(t.UnTransform(VECTOR3_TO_VEC3V(g_grabberState.m_vecPos2)));
							g_grabberState.m_vecNorm2 = VEC3V_TO_VECTOR3(t.UnTransform3x3(VECTOR3_TO_VEC3V(g_grabberState.m_vecNorm2)));


							g_grabberState.m_vecPos3 = VEC3V_TO_VECTOR3(t.UnTransform(VECTOR3_TO_VEC3V(g_grabberState.m_vecPos3)));
							g_grabberState.m_vecPos4 = VEC3V_TO_VECTOR3(t.UnTransform(VECTOR3_TO_VEC3V(g_grabberState.m_vecPos4)));
						}

						CTaskNMBalance* pTaskNM = rage_new CTaskNMBalance(3000, 20000, NULL, CGrabHelper::TARGET_SPECIFIC, &leanDir, CPedDebugVisualiserMenu::ms_fNMInteractiveGrabLeanAmount);
						if (pTaskNM->GetGrabHelper())
						{
							pTaskNM->GetGrabHelper()->SetGrabTarget(
								g_grabberState.m_pEntity,
								&g_grabberState.m_vecPos1,
								&g_grabberState.m_vecNorm1,
								&g_grabberState.m_vecPos2,
								&g_grabberState.m_vecNorm2,
								&g_grabberState.m_vecPos3,
								&g_grabberState.m_vecPos4);

							pTaskNM->GetGrabHelper()->SetFlag(CGrabHelper::TARGET_SURFACE_GRAB);
						}

						SwitchPedToNMBehaviour(pTaskNM);
					}
				}
			}
		}
	}
}

void CPedDebugVisualiserMenu::SetVehicleMissionTargetEntity( void )
{
	m_TargetEnt = CDebugScene::FocusEntities_Get(0);
}

void CPedDebugVisualiserMenu::ClearVehicleMissionTargetEntity( void )
{
	m_TargetEnt = NULL;
}

void CPedDebugVisualiserMenu::SetStartPosToMeasuringTool( void )
{
	m_StartMat.Identity();
	CPhysics::GetMeasuringToolPos(0, m_StartMat.d);
}

void CPedDebugVisualiserMenu::SetStartPosToDebugCam( void )
{
	m_StartMat = camInterface::GetMat();
}

void CPedDebugVisualiserMenu::SetStartPosToFocusPos( void )
{
	if( CDebugScene::FocusEntities_Get(0) )
	{
		m_StartMat = MAT34V_TO_MATRIX34(CDebugScene::FocusEntities_Get(0)->GetTransform().GetMatrix());
	}
}

void CPedDebugVisualiserMenu::SetVehicleMissionTargetPosToMeasuringTool( void )
{
	CPhysics::GetMeasuringToolPos(0, m_TargetPos);
}

void CPedDebugVisualiserMenu::SetVehicleMissionTargetPosToDebugCam( void )
{
	m_TargetPos = camInterface::GetPos();
}

void CPedDebugVisualiserMenu::SetVehicleMissionTargetPosToFocusPos( void )
{
	if( CDebugScene::FocusEntities_Get(0) )
	{
		m_TargetPos = VEC3V_TO_VECTOR3(CDebugScene::FocusEntities_Get(0)->GetTransform().GetPosition());
	}
}

void CPedDebugVisualiserMenu::SetVehicleMissionTargetPosToRandomRoadPos( void )
{
	s32 iTries = 1000;
	while( iTries > 0 )
	{
		--iTries;
		m_TargetPos.x = fwRandom::GetRandomNumberInRange(-5000.0f, 5000.0f);
		m_TargetPos.y = fwRandom::GetRandomNumberInRange(-5000.0f, 5000.0f);
		m_TargetPos.z = 0.0f;

		CNodeAddress NearestNode = ThePaths.FindNodeClosestToCoors(m_TargetPos, 99999.9f); //vDestination);
		if (!NearestNode.IsEmpty())
		{
			Vector3 nearestCoors;
			ThePaths.FindNodePointer(NearestNode)->GetCoors(nearestCoors);
			m_TargetPos = nearestCoors;
			return;
		}
	}
}

void CPedDebugVisualiserMenu::SetVehicleDrivingFlags( void )
{
	s32 iDrivingTypes[] =
	{
		0,
		DMode_StopForCars,
		DMode_StopForCars_Strict,
		DMode_AvoidCars,
		DMode_AvoidCars_Reckless,
		DMode_PloughThrough,
		DMode_StopForCars_IgnoreLights,
		DMode_AvoidCars_ObeyLights,
		DMode_AvoidCars_StopForPeds_ObeyLights
	};

	for( s32 i = 0; i < 32; i++ )
	{
		if( iDrivingTypes[m_DrivingFlags] & (1<<i) )
		{
			m_abDrivingFlags[i] = true;
		}
		else
		{
			m_abDrivingFlags[i] = false;
		}
	}
}

void CPedDebugVisualiser::DebugJacking()
{
	TUNE_GROUP_BOOL(VEHICLE_DEBUG, DEBUG_JACKING, false);

	if (DEBUG_JACKING)
	{
		CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
		if(pFocusEnt && pFocusEnt->GetIsTypePed())
		{
			CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
			if (pFocusPed)
			{
				grcDebugDraw::AddDebugOutput(Color_yellow, "Focus ped index %i", CPed::GetPool()->GetIndex(pFocusPed));

				CPed* pJackingPed = NULL;

				bool bPedBeingJacked = false;
				if (pFocusPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
				{
					CTaskInfo* pTaskInfo = pFocusPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_EXIT_VEHICLE, PED_TASK_PRIORITY_MAX);
					aiAssert(dynamic_cast<CClonedExitVehicleInfo*>(pTaskInfo));
					CClonedExitVehicleInfo* pVehInfo = static_cast<CClonedExitVehicleInfo*>(pTaskInfo);
					if (pVehInfo && pVehInfo->GetFlags().BitSet().IsSet(CVehicleEnterExitFlags::BeJacked))
					{
						bPedBeingJacked = true;
						pJackingPed = pVehInfo->GetPedsJacker();
					}
				}

				if (bPedBeingJacked)
				{
					grcDebugDraw::AddDebugOutput(Color_red, "Focus ped is being jacked");
					if (pJackingPed)
					{
						grcDebugDraw::AddDebugOutput(Color_red, "Focus ped is being jacked by ped with index %i", CPed::GetPool()->GetIndex(pJackingPed));
					}
				}
				else
					grcDebugDraw::AddDebugOutput(Color_green, "Focus ped is NOT being jacked");

				CPed* pJackedPed = NULL;
				bool bPedIsJacking = false;
				if (pFocusPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
				{
					CTaskInfo* pTaskInfo = pFocusPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX);
					aiAssert(dynamic_cast<CClonedEnterVehicleInfo*>(pTaskInfo));
					CClonedEnterVehicleInfo* pVehInfo = static_cast<CClonedEnterVehicleInfo*>(pTaskInfo);
					if (pVehInfo && pVehInfo->GetFlags().BitSet().IsSet(CVehicleEnterExitFlags::HasJackedAPed))
					{
						bPedIsJacking = true;
						pJackedPed = pVehInfo->GetJackedPed();
					}
				}

				if (bPedIsJacking)
				{
					grcDebugDraw::AddDebugOutput(Color_red, "Focus ped is jacking a ped");
					if (pJackedPed)
					{
						grcDebugDraw::AddDebugOutput(Color_red, "Focus ped is jacking a ped with index %i", CPed::GetPool()->GetIndex(pJackedPed));
					}
				}
				else
					grcDebugDraw::AddDebugOutput(Color_green, "Focus ped is NOT jacking");
			}
		}
	}
}

#endif // PEDDEBUGVISUALISER
