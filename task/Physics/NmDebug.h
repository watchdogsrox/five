// Class to collect useful debug stuff to do with Natural Motion tasks outside of those classes.
// RenderDebug() will be called from CDebugScene::Render_MainThread() to allow fwDebugDraw commands
// to be persistent between actual instances of an NM behaviour task.

#ifndef INC_NMDEBUG_H_
#define INC_NMDEBUG_H_

#include "animation/AnimBones.h"
#include "vector/Matrix34.h"

#define NUM_FEEDBACK_HISTORY_ELEMENTS 30

class CNmDebug
{
public:
	static void RenderDebug();
	static void SetComponentTMsFromSkeleton(const crSkeleton& skeleton);
	static void AddBehaviourFeedbackMessage(const char* zMessage);

private:
	static void RenderShotImpactCones();
	static void RenderIncomingTransforms();
	static void RenderFeedbackHistory();
	static void RenderRagdollComponentMatrices();
	static void RenderEdgeDetectionResults();
	static void RenderMoreEfficientEdgeDetectionResults();

	// Internal helper routines:
	static void StoreFocusPedAddress();

public:
	static bool ms_bDrawTransforms;
	static bool ms_bDrawFeedbackHistory;
	static bool ms_bDrawTeeterEdgeDetection;
	//static bool ms_bDrawStumbleEnvironmentDetection;
	static bool ms_bDrawBuoyancyEnvironmentDetection;
	static CPed* ms_pFocusPed;

private:
	static int m_nNumBounds;
	static Matrix34 ms_currentMatrices[RAGDOLL_NUM_COMPONENTS];

	// Data to implement an on-screen list of feedback messages from NM.
	struct SFeedbackHistory
	{
		char ms_aMessages[NUM_FEEDBACK_HISTORY_ELEMENTS][100];
		u32 ms_nAgeOfMessage[NUM_FEEDBACK_HISTORY_ELEMENTS];
		int ms_nCount;
		int ms_nOldestIndex; // Index of the oldest element in the history.
		int ms_nNewestIndex; // Index of the next empty space in the history.

		SFeedbackHistory() : ms_nCount(0), ms_nOldestIndex(0), ms_nNewestIndex(NUM_FEEDBACK_HISTORY_ELEMENTS-1) {}
	};
	static SFeedbackHistory ms_feedbackMessageHistory;

	// Variables which can be tweaked from RAG:
public:
	// Feedback history:
	static bool ms_bFbMsgOnlyShowFocusPed;
	static bool ms_bFbMsgShowSuccess;
	static bool ms_bFbMsgShowFailure;
	static bool ms_bFbMsgShowEvent;
	static bool ms_bFbMsgShowStart;
	static bool ms_bFbMsgShowFinish;
	static float ms_fListHeaderX;
	static float ms_fListHeaderY;
	static float ms_fListElementHeight;
	static u32 ms_nColourFadeStartTick;
	static u32 ms_nColourFadeEndTick;
	static u32 ms_nEndFadeColour;
	// Drawing component matrices:
	static bool ms_bDrawComponentMatrices;
	static int ms_nSelectedRagdollComponent;
	// TEMP: For teeter debug.
	static float ms_fEdgeTestAngle;
};

#endif // !INC_NMDEBUG_H_
