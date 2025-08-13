// File header
#include "Task/Combat/CombatMeleeDebug.h"

// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Peds/Ped.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/rendering/PedVariationDebug.h"
#include "Scene/World/GameWorld.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Weapons/Weapon.h"
#include "event/eventDamage.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CCombatMeleeDebug
//////////////////////////////////////////////////////////////////////////

#if __BANK
bool	CCombatMeleeDebug::sm_bSetDebugToMeleeOnStartOfMeleeTask					= false;
bool	CCombatMeleeDebug::sm_bDisplayQueueType										= false;
bool	CCombatMeleeDebug::sm_bUseCollision											= true;
float	CCombatMeleeDebug::sm_fSlideToXCoord										= 0.0f;
float	CCombatMeleeDebug::sm_fSlideToYCoord										= 0.0f;
s32		CCombatMeleeDebug::sm_iComboNum												= 0;
Vector3 CCombatMeleeDebug::sm_vPreviousCoords = Vector3(0.0f,0.0f,0.0f);
bool	CCombatMeleeDebug::sm_bUseSetRateOnPairedClips								= true;
float	CCombatMeleeDebug::sm_fForwardMultiplier									= 1.0f;
bool	CCombatMeleeDebug::sm_bDrawPreStealthKillDebug								= false;
float	CCombatMeleeDebug::sm_fBrainDeadPedsHealth									= 200.0f; // TODO: DATA DRIVE

bool	CCombatMeleeDebug::sm_bMeleeDisplayDebugEnabled								= false;

#if DEBUG_DRAW
#define MAX_TASK_DRAWABLES 10
CDebugDrawStore CCombatMeleeDebug::sm_debugDraw(MAX_TASK_DRAWABLES);
u32 CCombatMeleeDebug::sm_nLastDebugDrawFrame = 0;
#endif // DEBUG_DRAW

bool	CCombatMeleeDebug::sm_bDrawAttackWindowDebug								= true;
bool	CCombatMeleeDebug::sm_bDoHomingAndSeparation								= true;
bool	CCombatMeleeDebug::sm_bDoCollisions											= true;
bool	CCombatMeleeDebug::sm_bRelationshipDisplayDistanceXY						= false;
bool	CCombatMeleeDebug::sm_bRelationshipDisplayHeightDiff						= false;
bool	CCombatMeleeDebug::sm_bRelationshipDisplayGroundHeightDiff					= false;
bool	CCombatMeleeDebug::sm_bRelationshipDisplayFacingAngleXY						= false;
bool	CCombatMeleeDebug::sm_bRelationshipDisplayBoxAndClosestBoxPointDistXY		= false;
bool	CCombatMeleeDebug::sm_bRelationshipDisplayBoxAndClosestBoxPointHeightDiff	= false;
bool	CCombatMeleeDebug::sm_bFocusPedBreakOnTaskCombatMeleeStateStart				= false;
bool	CCombatMeleeDebug::sm_bFocusPedBreakOnTaskCombatMeleeProcessPreFSM			= false;
bool	CCombatMeleeDebug::sm_bFocusPedBreakOnTaskCombatMeleeShouldAbort			= false;
bool	CCombatMeleeDebug::sm_bFocusPedBreakOnTaskMeleeMovementStateRunning			= false;
bool	CCombatMeleeDebug::sm_bFocusPedBreakOnTaskMeleeActionProcessPreFSM			= false;
bool	CCombatMeleeDebug::sm_bFocusPedBreakOnSelectedMeleeAction					= false;
char	CCombatMeleeDebug::sm_actionToDebug[256];
bool	CCombatMeleeDebug::sm_bActionToDebugShowNormalTest							= true;
bool	CCombatMeleeDebug::sm_bActionToDebugShowFreeRoamTest						= false;
bool	CCombatMeleeDebug::sm_bVisualisePlayer										= true;
bool	CCombatMeleeDebug::sm_bVisualiseNPCs										= false;
bool	CCombatMeleeDebug::sm_bBreakOnFocusPedActionTransition						= false;
bool	CCombatMeleeDebug::sm_bFocusPedBreakOnAIWantsMoveButCantFindOne				= false;
bool	CCombatMeleeDebug::sm_bControlSubtaskDebugDisplayForPlayer					= true;
bool	CCombatMeleeDebug::sm_bControlSubtaskDebugDisplayForNPCs					= false;
bool	CCombatMeleeDebug::sm_bDrawQueueVolumes										= false;
bool	CCombatMeleeDebug::sm_bDrawDoingSomethingFromIdle							= false;
bool	CCombatMeleeDebug::sm_bDrawHomingDesiredPosDir								= false;
bool	CCombatMeleeDebug::sm_bDrawTargetPos										= false;
bool	CCombatMeleeDebug::sm_bDrawClonePos											= false;
bool	CCombatMeleeDebug::sm_bAllowNpcInitiatedMoves								= true;
bool	CCombatMeleeDebug::sm_bAllowNpcInitiatedMovement							= true;
bool	CCombatMeleeDebug::sm_bAllowMeleeRagdollReactions							= true;

void CCombatMeleeDebug::DebugDrawAttackWindow(float fPhase)
{
	static Vector2 vAttackWindowMeterPos(0.8f,0.15f);
	float fScale = 0.2f;
	float fEndWidth = 0.01f;

	grcDebugDraw::Meter(vAttackWindowMeterPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Attack Window");
	grcDebugDraw::MeterValue(vAttackWindowMeterPos, Vector2(0.0f,1.0f), fScale, fPhase, fEndWidth, Color32(255,0,0),true);	
}

void CCombatMeleeDebug::MakePlayerFaceFocusPedCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
		CTaskTurnToFaceEntityOrCoord* pTaskToGive = rage_new CTaskTurnToFaceEntityOrCoord(pFocusEnt);
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGive,false,E_PRIORITY_MAX);
		pPlayerPed->GetPedIntelligence()->AddEvent(event);
	}
}

void CCombatMeleeDebug::MakePlayerSlideToCoordCB()
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
	Vector3 vSlideToPos = Vector3(sm_vPreviousCoords.x+sm_fSlideToXCoord,sm_vPreviousCoords.y+sm_fSlideToYCoord,0.0f);
	CTaskSlideToCoord* pTaskToGive = rage_new CTaskSlideToCoord(vSlideToPos,pPlayerPed->GetTransform().GetHeading(),10.0f);
	CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGive,false,E_PRIORITY_MAX);
	pPlayerPed->GetPedIntelligence()->AddEvent(event);
}

void CCombatMeleeDebug::PositionPedCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		Vector3 vForward = VEC3V_TO_VECTOR3(pFocusEnt->GetTransform().GetB());
		vForward.Normalize();
		const Vector3 vMoveToPosition = VEC3V_TO_VECTOR3(pFocusEnt->GetTransform().GetPosition()) + vForward*sm_fForwardMultiplier;
		CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
		CTaskSlideToCoord* pTaskToGive = rage_new CTaskSlideToCoord(vMoveToPosition, pPlayerPed->GetTransform().GetHeading(),10.0f);
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGive,false,E_PRIORITY_MAX);
		pPlayerPed->GetPedIntelligence()->AddEvent(event);
	}
}

void CCombatMeleeDebug::SetCollisionOffFocusPedCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
		//pFocusPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		if (pFocusPed->IsCollisionEnabled())
			pFocusPed->DisableCollision();
		else
			pFocusPed->EnableCollision();
	}
}

void CCombatMeleeDebug::DoCapsuleTestCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);
		
 		static dev_float s_fTestRadius = 1.0f;

		const u32 iTestFlagsBuildings = (ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);// | ArchetypeFlags::GTA_PED_TYPE);
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		const Vec3V vStartPos = pFocusPed->GetTransform().GetPosition();
		const Vec3V vEndPos = vStartPos + pFocusPed->GetTransform().GetB()* ScalarV(V_THREE);
		capsuleDesc.SetCapsule(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos), s_fTestRadius);
		//CEntity* pFocusEnt1 = CDebugScene::FocusEntities_Get(1);
		capsuleDesc.SetExcludeEntity(pFocusPed);
		capsuleDesc.SetIncludeFlags(iTestFlagsBuildings);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);
		const bool bClear = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
		
		if (bClear)
		{
			sm_debugDraw.AddSphere(vStartPos,s_fTestRadius,Color_green,5000,0,false);
			sm_debugDraw.AddLine(vStartPos,vEndPos,Color_green,5000);
			sm_debugDraw.AddSphere(vEndPos,s_fTestRadius,Color_green,5000,0,false);
		}
		else
		{
			sm_debugDraw.AddSphere(vStartPos,s_fTestRadius,Color_red,5000,0,false);
			sm_debugDraw.AddLine(vStartPos,vEndPos,Color_red,5000);
			sm_debugDraw.AddSphere(vEndPos,s_fTestRadius,Color_red,5000,0,false);
		}
	}
}

void CCombatMeleeDebug::DoLocalHitReactionCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pFocusPed = static_cast<CPed*>(pFocusEnt);

		fwFlags32 flags( CPedDamageCalculator::DF_MeleeDamage );
		flags.SetFlag( CPedDamageCalculator::DF_ForceMeleeDamage );

		u32 uWeaponHash = pFocusPed->GetWeaponManager() ? pFocusPed->GetWeaponManager()->GetEquippedWeaponHash() : pFocusPed->GetDefaultUnarmedWeaponHash();
		CWeapon tempWeapon( uWeaponHash );

		static dev_u32 nResultID = 2661951225;
		
		CWeaponDamage::sMeleeInfo info;
		info.uParentMeleeActionResultID = nResultID;
		info.uNetworkActionID = 0;
		info.uForcedReactionDefinitionID = 0;
		
		CWeaponDamage::GeneratePedDamageEvent(CGameWorld::FindLocalPlayer(), 
			pFocusPed, 
			uWeaponHash, 
			20.0f, 
			VEC3V_TO_VECTOR3( pFocusPed->GetTransform().GetPosition() ),
			NULL,
			flags,
			&tempWeapon,
			-1, 
			0.0f,
			&info);
	}
}

void CCombatMeleeDebug::DoLocalKillMoveCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		CPed* pTargetPed = static_cast<CPed*>(pFocusEnt);

		static dev_u32 nActionResultID = 11769692;

		u32 nIndexFound = 0;
		const CActionResult* pReactionResult = NULL;
		const CActionResult* pActionResult = ACTIONMGR.FindActionResult( nIndexFound, nActionResultID );

		if(pActionResult)
		{
			CActionFlags::ActionTypeBitSetData typeToLookFor;
			typeToLookFor.Set( CActionFlags::AT_HIT_REACTION, true );

			const CActionDefinition* pForcedReactionDefinition = ACTIONMGR.SelectSuitableAction(
				typeToLookFor,						// The type we want to search in.
				NULL,								// No Impulse combo
				false,								// Test whether or not it is an entry action.
				true,								// does not matter.
				pTargetPed,							// The hit ped
				pActionResult->GetID(),				// What result is invoking.
				pActionResult->GetPriority(),		// Priority
				pTargetPed,							// the acting ped
				false,								// We don't have a lock on them.
				false,								// Should we only allow impulse initiated moves.
				pTargetPed->IsLocalPlayer(),		// If the target is a player then test the controls.
				CActionManager::ShouldDebugPed( pTargetPed ) );

			if(pForcedReactionDefinition)
			{
				pReactionResult = pForcedReactionDefinition->GetActionResult(pTargetPed);
			}
		}

		// Check if we found an appropriate reaction, if not then do nothing.
		if( pReactionResult && pActionResult )
		{
			s32 iMeleeFlags = CTaskMelee::MF_IsDoingReaction | CTaskMelee::MF_ForcedReaction;

			//! Try to stop ped moving from here on in.
			pTargetPed->SetVelocity( VEC3_ZERO );
			pTargetPed->StopAllMotion();
			pTargetPed->SetDesiredVelocity( VEC3_ZERO );
			pTargetPed->SetDesiredHeading( pTargetPed->GetCurrentHeading() );

			// Force update the clip to play immediately - saves a frame delay
			pTargetPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );

			// let the target ped know they are dying via stealth
			if( pActionResult->GetIsAStealthKill() )
			{
				pTargetPed->SetRagdollOnCollisionIgnorePhysical( pTargetPed );

				if(!pTargetPed->IsNetworkClone())
				{
					pTargetPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth, true );
				}
			}
			else if( pActionResult->GetIsATakedown() )
			{
				// Takedowns can be unsuccessful
				const CDamageAndReaction* pDamageAndReaction = pReactionResult->GetDamageAndReaction();
				if( pDamageAndReaction && pDamageAndReaction->GetSelfDamageAmount() > 0 )
				{
					pTargetPed->SetRagdollOnCollisionIgnorePhysical( pTargetPed );
					if(!pTargetPed->IsNetworkClone())
					{
						pTargetPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown, true );
					}
				}
			}

			CTaskMelee* pReactionTask = rage_new CTaskMelee( pReactionResult, pTargetPed, iMeleeFlags );
			if( pReactionTask )
			{
				Assert( pTargetPed->GetPedIntelligence() );
				//! kick off a local task, this keeps animations in sync throughout.
				if( pTargetPed->IsNetworkClone() )
					pTargetPed->GetPedIntelligence()->AddLocalCloneTask(pReactionTask, PED_TASK_PRIORITY_PHYSICAL_RESPONSE);
				else
				{
					CEventGivePedTask forcedReactionEvent( PED_TASK_PRIORITY_PHYSICAL_RESPONSE, pReactionTask, false, E_PRIORITY_DAMAGE_PAIRED_MOVE );
					pTargetPed->GetPedIntelligence()->AddEvent( forcedReactionEvent );
				}
			}
		}
	}
}


void CCombatMeleeDebug::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");

	pBank->PushGroup("Melee Combat", false);
		pBank->PushGroup("Debug Tools", false);
			pBank->AddButton("Spawn One Melee Opponent", &CPedVariationDebug::CreateOneMeleeOpponentCB);
			pBank->AddButton("Spawn One Brain Dead Melee Opponent", &CPedVariationDebug::CreateOneBrainDeadMeleeOpponentCB);
			pBank->AddSlider("Brain Dead Peds Health", &sm_fBrainDeadPedsHealth, 0.0f, 10000.0f, 1.0f);

			pBank->AddToggle("Enable Melee Network Display Debug", &sm_bMeleeDisplayDebugEnabled);

			pBank->AddToggle("Toggle pre stealth kill debug text",&sm_bDrawPreStealthKillDebug);
#if __DEV
			pBank->AddToggle("Toggle melee debug on melee start",&sm_bSetDebugToMeleeOnStartOfMeleeTask);
				pBank->PushGroup("Interrelation Test Debug");
					pBank->AddToggle("Display Target Flat Distance", &sm_bRelationshipDisplayDistanceXY);
					pBank->AddToggle("Display Target Height Diff", &sm_bRelationshipDisplayHeightDiff);
					pBank->AddToggle("Display Target Ground Height Diff", &sm_bRelationshipDisplayGroundHeightDiff);
					pBank->AddToggle("Display Self To Target Facing Angle", &sm_bRelationshipDisplayFacingAngleXY);
					pBank->AddToggle("Display Target BBox + Nearest pt distxy", &sm_bRelationshipDisplayBoxAndClosestBoxPointDistXY);
					pBank->AddToggle("Display Target BBox + Nearest pt heightDiff", &sm_bRelationshipDisplayBoxAndClosestBoxPointHeightDiff);
					pBank->AddToggle("Display Queue Types", &sm_bDisplayQueueType);
				pBank->PopGroup();
				pBank->AddToggle("Use set rate on paired clip", &sm_bUseSetRateOnPairedClips);
				pBank->AddToggle("Break on focus ped transition", &sm_bBreakOnFocusPedActionTransition);
				pBank->AddSlider("Forward multiplier",&sm_fForwardMultiplier,0.0f,2.0f,0.05f);
			pBank->AddButton("Do Capsule Test On Focus Ped", DoCapsuleTestCB);
			pBank->AddButton("Make player face focus ped", MakePlayerFaceFocusPedCB, "Makes player face focus ped");
			pBank->AddButton("Make player slide to relative coord focus ped", MakePlayerSlideToCoordCB, "Makes player slide to coord");
			pBank->AddButton("Position player ped", PositionPedCB, "Makes player slide to coord");
			pBank->AddButton("Do local hit reaction", DoLocalHitReactionCB, "Makes ped do a local hit reaction");
			pBank->AddButton("Do local kill reaction", DoLocalKillMoveCB, "Makes ped do a local kill reaction");
			pBank->AddToggle("Use collision on focus",&sm_bUseCollision);
			pBank->AddSlider("sm_fSlideToXCoord",&sm_fSlideToXCoord,-10.0f,10.0f,0.01f);
			pBank->AddSlider("sm_fSlideToYCoord",&sm_fSlideToYCoord,-10.0f,10.0f,0.01f);
			pBank->AddButton("Set collision off focus ped",SetCollisionOffFocusPedCB,"Set collision off focus ped");
			pBank->AddToggle("Process Homing and separation",&sm_bDoHomingAndSeparation);
			pBank->AddToggle("Process Melee Collisions",&sm_bDoCollisions);
			pBank->AddText("Action To Debug", sm_actionToDebug, sizeof(sm_actionToDebug), false, NullCB);
			pBank->AddToggle("Show Debugged Action Normal Test", &sm_bActionToDebugShowNormalTest);
			pBank->AddToggle("Show Debugged Action Free Roam Test", &sm_bActionToDebugShowFreeRoamTest);
			pBank->AddToggle("Visualise Debugged Action For Player", &sm_bVisualisePlayer);
			pBank->AddToggle("Visualise Debugged Action For NPCs", &sm_bVisualiseNPCs);

			pBank->AddToggle("Focus Ped Break On TaskCombatMelee::StateStart", &sm_bFocusPedBreakOnTaskCombatMeleeStateStart);
			pBank->AddToggle("Focus Ped Break On TaskCombatMelee::ProcessPreFSM", &sm_bFocusPedBreakOnTaskCombatMeleeProcessPreFSM);
			pBank->AddToggle("Focus Ped Break On TaskCombatMelee::ShouldAbort", &sm_bFocusPedBreakOnTaskCombatMeleeShouldAbort);
			pBank->AddToggle("Focus Ped Break On TaskMeleeMovement::StateRunning", &sm_bFocusPedBreakOnTaskMeleeMovementStateRunning);
			pBank->AddToggle("Focus Ped Break On TaskMeleeAction::ProcessPreFSM", &sm_bFocusPedBreakOnTaskMeleeActionProcessPreFSM);
			
			pBank->AddToggle("Focus Ped Break On Selected Melee Action", &sm_bFocusPedBreakOnSelectedMeleeAction);
			pBank->AddToggle("Focus Ped Break On AI Wants Move But Cant Find One", &sm_bFocusPedBreakOnAIWantsMoveButCantFindOne);
			pBank->AddToggle("Control Subtask Debug Display For Player", &sm_bControlSubtaskDebugDisplayForPlayer);
			pBank->AddToggle("Control Subtask Debug Display For NPCs", &sm_bControlSubtaskDebugDisplayForNPCs);
			pBank->AddToggle("Debug draw queue volumes", &sm_bDrawQueueVolumes, NullCB, "Whether or not to draw queue volumes for the melee combat system.");
			pBank->AddToggle("Debug draw DoingSomethingFromIdle", &sm_bDrawDoingSomethingFromIdle, NullCB, "Whether or not to draw a marker when the ped is doing a melee move from idle.");
			pBank->AddToggle("Debug draw homing desired pos and dir", &sm_bDrawHomingDesiredPosDir, NullCB, "Whether or not to draw debug homing information for the melee combat system.");
			pBank->AddToggle("Debug draw target entity pos", &sm_bDrawTargetPos, NullCB, "Whether or not to draw target information for the melee combat system.");
			pBank->AddToggle("Debug draw clone entity pos", &sm_bDrawClonePos, NullCB, "Whether or not to draw clone information for the melee combat system.");
			pBank->AddToggle("Allow Npc initiated moves", &sm_bAllowNpcInitiatedMoves, NullCB, "Whether or not to to allow the NPCs to initiate moves.");	
			pBank->AddToggle("Allow Npc initiated movement", &sm_bAllowNpcInitiatedMovement, NullCB, "Whether or not to to allow the NPCs to move about.");	
			pBank->AddToggle("Allow Melee Ragdoll Reactions", &sm_bAllowMeleeRagdollReactions, NullCB, "Whether or not to to allow moves that cause ragdoll physics reactions.");	
#endif // __DEV
		pBank->PopGroup();	// "Debug Tools"	
	
		pBank->PushGroup("Global tuning", false);
			pBank->AddSlider("Block Timer Delay Enter", &CTaskMelee::sm_fBlockTimerDelayEnter, 0.0f,100.0f,0.1f, NullCB, "The amount of time(secs) player has to hold the button before it begins the block anim.");
			pBank->AddSlider("Block Timer Delay Exit", &CTaskMelee::sm_fBlockTimerDelayExit, 0.0f,100.0f,0.1f, NullCB, "The amount of time(secs) player has to have the button released before it reverses the block anim.");
			pBank->AddSlider("Maximium Number of Concurrent Combatants", &CTaskMelee::sm_nMaxNumActiveCombatants, 0, 5, 1);
			pBank->AddSlider("Maximium Number of Active Support Combatants", &CTaskMelee::sm_nMaxNumActiveSupportCombatants, 0, 5, 1);
			pBank->AddSlider("Time to remain in CTaskMelee if hit(ms)", &CTaskMelee::sm_nTimeInTaskAfterHitReaction, 0, 10000, 100, NullCB, "Amount of time the CTaskMelee will continue to run after hit by attacking ped. Can be interrupted by user input or targeting.");
			pBank->AddSlider("Time to remain in CTaskMelee after standard attack(ms)", &CTaskMelee::sm_nTimeInTaskAfterStardardAttack, 0, 10000, 100, NullCB, "Amount of time the CTaskMelee will continue to run after performing a standard attack. Can be interrupted by user input or targeting.");
		pBank->PopGroup(); // "Global tuning"

		pBank->PushGroup("Player tuning", false);
			pBank->AddSlider("Minimum strafing distance", &CTaskMelee::sm_fMeleeStrafingMinDistance, 0.0f,100.0f,0.1f);
			pBank->AddSlider("Moving strafing trigger dist (when behind target)", &CTaskMelee::sm_fMovingTargetStrafeTriggerRadius, 0.0f,100.0f,0.1f);
			pBank->AddSlider("Moving strafing breakout dist (when behind target)", &CTaskMelee::sm_fMovingTargetStrafeBreakoutRadius, 0.0f,100.0f,0.1f);
			pBank->AddSlider("Forced strafing radius", &CTaskMelee::sm_fForcedStrafeRadius, 0.0f,100.0f,0.1f);
			pBank->AddSlider("Player Clip Playback Speed Multiplier", &CTaskMelee::sm_fPlayerClipPlaybackSpeedMultiplier, 0.0f,100.0f,0.1f);
			pBank->AddSlider("Time between Taunts Min(ms)", &CTaskMelee::sm_nPlayerTauntFrequencyMinInMs, 0, 50000, 100);
			pBank->AddSlider("Time between Taunts Max(ms)", &CTaskMelee::sm_nPlayerTauntFrequencyMaxInMs, 0, 50000, 100);
			pBank->AddSlider("Impulse Interrupt Delay(ms)", &CTaskMelee::sm_nPlayerImpulseInterruptDelayInMs, 0, 50000, 100);
		pBank->PopGroup(); // "Player tuning"	
	
		pBank->PushGroup("AI tuning", false);
			pBank->PushGroup("AI Taunts", false);
				pBank->AddSlider("Hit delay timer Timer(ms)", &CTaskMelee::sm_nAiTauntHitDelayTimerInMs, 0, 50000, 100);
			pBank->PopGroup(); // "AI Taunts"

			pBank->PushGroup("AI Defense", false);
				pBank->AddSlider("Consecutive Hit Buff Increase", &CTaskMelee::sm_fAiBlockConsecutiveHitBuffIncrease, 0.0f, 1.0f, 0.05f);
				pBank->AddSlider("Action Buff Decrease", &CTaskMelee::sm_fAiBlockActionBuffDecrease, 0.0f, 1.0f, 0.05f);
			pBank->PopGroup(); // "AI Blocks"

			pBank->PushGroup("AI Attack", false);
				pBank->AddSlider("Consecutive hit frequency increase min(ms)", &CTaskMelee::sm_fAiAttackConsecutiveHitBuffIncreaseMin, 0, 10000, 10);
				pBank->AddSlider("Consecutive hit frequency increase max(ms)", &CTaskMelee::sm_fAiAttackConsecutiveHitBuffIncreaseMax, 0, 10000, 10);
			pBank->PopGroup(); // "AI Attack"

			pBank->PushGroup("AI Target", false);
				pBank->AddSlider("Separation Desired Self To Target Range", &CTaskMelee::sm_fAiTargetSeparationDesiredSelfToTargetRange, 0.0f, 10.0f, 0.05f);
				pBank->AddSlider("Separation Min Diff Range For Impetus", &CTaskMelee::sm_fAiTargetSeparationPrefdDiffMinForImpetus, 0.0f, 10.0f, 0.1f);
				pBank->AddSlider("Separation Max Diff Range For Impetus", &CTaskMelee::sm_fAiTargetSeparationPrefdDiffMaxForImpetus, 0.0f, 10.0f, 0.1f);
				pBank->AddSlider("Separation Forward Impetus Power Factor", &CTaskMelee::sm_fAiTargetSeparationForwardImpetusPowerFactor, 0.0f, 5.0f, 0.25f);
				pBank->AddSlider("Separation Backward Impetus Power Factor", &CTaskMelee::sm_fAiTargetSeparationBackwardImpetusPowerFactor, 0.0f, 5.0f, 0.25f);
				pBank->AddSlider("Separation Target Approach Desire Size", &CTaskMelee::sm_fAiTargetSeparationMinTargetApproachDesireSize, 0.0f, 1.0f, 0.05f);
			pBank->PopGroup(); // "AI Target"

			pBank->PushGroup("AI Avoid", false);
				pBank->AddSlider("Separation Desired Self To Avoid Range", &CTaskMelee::sm_fAiAvoidSeparationDesiredSelfToTargetRange, 0.0f, 10.0f, 0.05f);
				pBank->AddSlider("Min Time between selecting Avoid Targets", &CTaskMelee::sm_nAiAvoidEntityDelayMinInMs, 0, 10000, 10);
				pBank->AddSlider("Max Time between selecting Avoid Targets", &CTaskMelee::sm_nAiAvoidEntityDelayMaxInMs, 0, 10000, 10);
				pBank->AddSlider("Separation Min Diff Range For Impetus", &CTaskMelee::sm_fAiAvoidSeparationPrefdDiffMinForImpetus, 0.0f, 10.0f, 0.1f);
				pBank->AddSlider("Separation Max Diff Range For Impetus", &CTaskMelee::sm_fAiAvoidSeparationPrefdDiffMaxForImpetus, 0.0f, 10.0f, 0.1f);
				pBank->AddSlider("Separation Forward Impetus Power Factor", &CTaskMelee::sm_fAiAvoidSeparationForwardImpetusPowerFactor, 0.0f, 5.0f, 0.25f);
				pBank->AddSlider("Separation Backward Impetus Power Factor", &CTaskMelee::sm_fAiAvoidSeparationBackwardImpetusPowerFactor, 0.0f, 5.0f, 0.25f);
				pBank->AddSlider("Separation Avoid Approach Desire Size", &CTaskMelee::sm_fAiAvoidSeparationMinTargetApproachDesireSize, 0.0f, 1.0f, 0.05f);
				pBank->AddSlider("Separation Circling Strength", &CTaskMelee::sm_fAiAvoidSeparationCirlclingStrength, 0.0f, 1.0f, 0.05f);
			pBank->PopGroup(); // "AI Avoid"

			pBank->PushGroup("AI Seek", false);
				pBank->AddSlider("Scan Time Min", &CTaskMelee::sm_nAiSeekModeScanTimeMin, 0, 10000, 10);
				pBank->AddSlider("Scan Time Max", &CTaskMelee::sm_nAiSeekModeScanTimeMax, 0, 10000, 10);
				pBank->AddSlider("Target Height", &CTaskMelee::sm_fAiSeekModeTargetHeight, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Initial Pursuit Breakout Distance", &CTaskMelee::sm_fAiSeekModeInitialPursuitBreakoutDist, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Chase Trigger Offset Distance", &CTaskMelee::sm_fAiSeekModeChaseTriggerOffset, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Flee Trigger Offset Distance", &CTaskMelee::sm_fAiSeekModeFleeTriggerOffset, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Active Combatant Radius", &CTaskMelee::sm_fAiSeekModeActiveCombatantRadius, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Support Combatant Radius Min", &CTaskMelee::sm_fAiSeekModeSupportCombatantRadiusMin, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Support Combatant Radius Max", &CTaskMelee::sm_fAiSeekModeSupportCombatantRadiusMax, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Inactive Combatant Radius Min", &CTaskMelee::sm_fAiSeekModeInactiveCombatantRadiusMin, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Inactive Combatant Radius Max", &CTaskMelee::sm_fAiSeekModeInactiveCombatantRadiusMax, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Retreat Distance Min", &CTaskMelee::sm_fAiSeekModeRetreatDistanceMin, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Retreat Distance Max", &CTaskMelee::sm_fAiSeekModeRetreatDistanceMax, 0.0f, 100.0f, 0.05f);
			pBank->PopGroup(); // "AI Seek"

			pBank->PushGroup("AI Fight Queue", false);
				pBank->AddSlider("Preferred Min Diff Range for Impetus", &CTaskMelee::sm_fAiQueuePrefdPosDiffMinForImpetus, 0.0f, 100.0f, 0.05f);
				pBank->AddSlider("Preferred Max Diff Range for Impetus", &CTaskMelee::sm_fAiQueuePrefdPosDiffMaxForImpetus, 0.0f, 100.0f, 0.05f);
			pBank->PopGroup(); // "AI Fight Queue"
		pBank->PopGroup(); // "AI tuning"
	pBank->PopGroup(); // "Melee Combat"
}

#endif // __BANK
