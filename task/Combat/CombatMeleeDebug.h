#ifndef INC_COMBAT_MELEE_DEBUG_H
#define INC_COMBAT_MELEE_DEBUG_H

#include "vector/vector3.h"
#include <vector>
#include "Debug/DebugDrawStore.h"

// Rage includes
#include "atl/string.h"

// Forward declarations
namespace rage { class bkBank; }

#if DEBUG_DRAW
class CDebugDrawStore;
#endif // DEBUG_DRAW

//////////////////////////////////////////////////////////////////////////
// CCombatMeleeDebug
//////////////////////////////////////////////////////////////////////////

#if __BANK

class CCombatMeleeDebug
{
public:

	// Setup the RAG widgets 
	static void InitWidgets();

	// Display the attack window debug
	static void DebugDrawAttackWindow(float fPhase);

	// Toggles for  various debug options
	static bool	sm_bDrawPreStealthKillDebug;

	static bool sm_bDrawAttackWindowDebug;
	static bool sm_bDoHomingAndSeparation;
	static bool sm_bDoCollisions;
	static bool sm_bSetDebugToMeleeOnStartOfMeleeTask;
	static bool sm_bDisplayQueueType;
	static bool	sm_bRelationshipDisplayDistanceXY;
	static bool	sm_bRelationshipDisplayHeightDiff;
	static bool	sm_bRelationshipDisplayGroundHeightDiff;
	static bool	sm_bRelationshipDisplayFacingAngleXY;
	static bool	sm_bRelationshipDisplayBoxAndClosestBoxPointDistXY;
	static bool	sm_bRelationshipDisplayBoxAndClosestBoxPointHeightDiff;
	static bool sm_bUseCollision;
	static bool	sm_bFocusPedBreakOnTaskCombatMeleeStateStart;
	static bool	sm_bFocusPedBreakOnTaskCombatMeleeProcessPreFSM;
	static bool	sm_bFocusPedBreakOnTaskCombatMeleeShouldAbort;
	static bool	sm_bFocusPedBreakOnTaskMeleeMovementStateRunning;
	static bool	sm_bFocusPedBreakOnTaskMeleeActionProcessPreFSM;
	static bool sm_bFocusPedBreakOnSelectedMeleeAction;
	static bool	sm_bActionToDebugShowNormalTest;
	static bool	sm_bActionToDebugShowFreeRoamTest;
	static bool	sm_bVisualisePlayer;
	static bool	sm_bVisualiseNPCs;
	static bool sm_bFocusPedBreakOnAIWantsMoveButCantFindOne;
	static bool sm_bControlSubtaskDebugDisplayForPlayer;
	static bool sm_bControlSubtaskDebugDisplayForNPCs;
	static bool sm_bDrawQueueVolumes;
	static bool sm_bDrawDoingSomethingFromIdle;
	static bool sm_bDrawHomingDesiredPosDir;
	static bool sm_bDrawTargetPos;
	static bool sm_bDrawClonePos;
	static bool sm_bAllowNpcInitiatedMoves;
	static bool sm_bAllowNpcInitiatedMovement;
	static bool sm_bAllowMeleeRagdollReactions;
	static bool sm_bBreakOnFocusPedActionTransition;
	static bool sm_bUseSetRateOnPairedClips;

	static char	sm_actionToDebug[256];
	static float sm_fSlideToXCoord;
	static float sm_fSlideToYCoord;

	static float sm_fForwardMultiplier;
	static float sm_fBrainDeadPedsHealth;

	// Variables for paired clip tests
	static s32 sm_iComboNum;
	static Vector3 sm_vPreviousCoords;

	static bool sm_bMeleeDisplayDebugEnabled;

#if DEBUG_DRAW
	static CDebugDrawStore sm_debugDraw;
	static u32 sm_nLastDebugDrawFrame;
#endif // DEBUG_DRAW

private:
	static void MakePlayerFaceFocusPedCB();
	static void MakePlayerSlideToCoordCB();
	static void SetCollisionOffFocusPedCB();
	static void PositionPedCB();
	static void DoCapsuleTestCB();
	static void DoLocalHitReactionCB();
	static void DoLocalKillMoveCB();
};

#endif // __BANK

#endif // INC_COMBAT_MELEE_DEBUG_H
