#if __BANK

// Game includes:
#include "peds/ped.h"
#include "Task/Physics/NmDebug.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMShot.h"
#include "debug/DebugScene.h"

// Framework includes: 
#include "ai/aichannel.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"

// RAGE includes:
#include "art/ARTRockstar.h"
#include "fragmentnm/manager.h"
#include "phbound/boundcapsule.h"
#include "physics/WorldProbe/worldprobe.h"


AI_OPTIMISATIONS()

// Initialise static member variables:
bool CNmDebug::ms_bDrawTransforms = false;
bool CNmDebug::ms_bDrawFeedbackHistory = false;
bool CNmDebug::ms_bDrawTeeterEdgeDetection = false;
//bool CNmDebug::ms_bDrawStumbleEnvironmentDetection = false;
bool CNmDebug::ms_bDrawBuoyancyEnvironmentDetection = false;
int CNmDebug::m_nNumBounds = -1;
Matrix34 CNmDebug::ms_currentMatrices[RAGDOLL_NUM_COMPONENTS];
CNmDebug::SFeedbackHistory CNmDebug::ms_feedbackMessageHistory;
CPed* CNmDebug::ms_pFocusPed = 0;
// RAG variables:
bool CNmDebug::ms_bFbMsgOnlyShowFocusPed = false;
bool CNmDebug::ms_bFbMsgShowSuccess = true;
bool CNmDebug::ms_bFbMsgShowFailure = true;
bool CNmDebug::ms_bFbMsgShowEvent = true;
bool CNmDebug::ms_bFbMsgShowStart = true;
bool CNmDebug::ms_bFbMsgShowFinish = true;
float CNmDebug::ms_fListHeaderX = 0.05f;
float CNmDebug::ms_fListHeaderY = 0.375f;
float CNmDebug::ms_fListElementHeight = 0.017f;
u32 CNmDebug::ms_nColourFadeStartTick = 0;
u32 CNmDebug::ms_nColourFadeEndTick = 40;
u32 CNmDebug::ms_nEndFadeColour = 100;
bool CNmDebug::ms_bDrawComponentMatrices = false;
int CNmDebug::ms_nSelectedRagdollComponent = 0;

float CNmDebug::ms_fEdgeTestAngle = 0.0f;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::RenderDebug()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	StoreFocusPedAddress();

	if(CTaskNMShot::sm_Tunables.m_bEnableDebugDraw)
	{
		RenderShotImpactCones();
	}

	if(ms_bDrawTransforms)
	{
		RenderIncomingTransforms();
	}

	if(ms_bDrawFeedbackHistory)
	{
		RenderFeedbackHistory();
	}

	if(ms_bDrawComponentMatrices)
	{
		RenderRagdollComponentMatrices();
	}

	if(ms_bDrawTeeterEdgeDetection)
	{
		static dev_bool bShowNewEdgeDetect = false;
		if(bShowNewEdgeDetect)
			RenderMoreEfficientEdgeDetectionResults();
		else
			RenderEdgeDetectionResults();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::StoreFocusPedAddress()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Null this pointer in case there is no focus ped.
	ms_pFocusPed = 0;

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	// Early out if no ped is selected.
	if(!pFocusEntity) return;
	if(!pFocusEntity->GetIsTypePed()) return;

	// We must have a selected ped by this stage:
	ms_pFocusPed = static_cast<CPed*>(pFocusEntity);
	taskAssertf(ms_pFocusPed, "Ped should be selected but pointer to ped is NULL.");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::RenderShotImpactCones()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Visualise the front and back impact cones on a selected ped. These cones define the impact angles which will trigger
	// a certain animPose reaction in the shot behaviour.

	if(!ms_pFocusPed)
		return;

	// Start by defining the start point and various axes for the cone.
	//Vector3 vConeOrigin = ms_pFocusPed->GetPosition();
	Matrix34 compMatrix;
	ms_pFocusPed->GetRagdollComponentMatrix(compMatrix, CTaskNMShot::sm_Tunables.m_eImpactConeRagdollComponent);
	Vector3 vConeOrigin = compMatrix.d;
	Vector3 vPedFrontNormal;
	Matrix34 m = MAT34V_TO_MATRIX34(ms_pFocusPed->GetMatrix());
	vPedFrontNormal.Scale(m.GetVector(2), -1.0f);
	Vector3 vPedSideAxis = m.GetVector(1);

	// Create an array of vectors which live on the surface of a cone around the normal coming out of
	// the front of the ped.
	WIN32_ONLY(const) static int nVectorsInCone = 20;
	Vector3 *vCone = Alloca(Vector3,nVectorsInCone);
	for(int i = 0; i < nVectorsInCone; ++i)
	{
		vCone[i] = Vector3(YAXIS);
		vCone[i].RotateAboutAxis(CTaskNMShot::sm_Tunables.m_fImpactConeAngleFront * PI/180.0f, 'x');
		vCone[i].RotateAboutAxis((float)i*(2*PI/nVectorsInCone), 'y');

		// TODO RA: Hack to correct for discrepancy between animated and ragdoll inst matrices. Remove when this
		// has been resolved.
		if(ms_pFocusPed->GetRagdollState() == RAGDOLL_STATE_PHYS)
		{
			vCone[i].RotateAboutAxis(-PI/2.0f, 'x');
		}

		// Rotate the cone to match the ped's orientation.
		vCone[i] = VEC3V_TO_VECTOR3(ms_pFocusPed->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vCone[i])));
	}

	for(int i = 0; i < nVectorsInCone; ++i)
	{
		// Draw the vectors.
		Vector3 vEndOfConeVector;
		vEndOfConeVector.Add(vConeOrigin, vCone[i]);
		grcDebugDraw::Line(vConeOrigin, vEndOfConeVector, Color_blue);

		// Draw a low-res "circle" at the end of the cone vectors.
		if(i > 0)
		{
			Vector3 vStart, vEnd;
			vStart.Add(vConeOrigin, vCone[i]);
			vEnd.Add(vConeOrigin, vCone[i-1]);
			grcDebugDraw::Line(vStart, vEnd, Color_blue);
		}
		else
		{
			Vector3 vStart, vEnd;
			vStart.Add(vConeOrigin, vCone[nVectorsInCone-1]);
			vEnd.Add(vConeOrigin, vCone[0]);
			grcDebugDraw::Line(vStart, vEnd, Color_blue);
		}
	}

	// Draw "front" normal for this component.
	Vector3 vEndOfLine;
	vEndOfLine.Add(vConeOrigin, vPedFrontNormal);
	grcDebugDraw::Line(vConeOrigin, vEndOfLine, Color_blue);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::RenderIncomingTransforms()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Show the position and orientation of the transformation matrices sent to NM.

	static bool bSolid = true;

	if(!ms_pFocusPed)
		return;

	// The bound geometries are defined by an archetype. Use the current world space matrices being sent
	// to NM to transform the bounds and draw them.
	aiAssertf(dynamic_cast<phBoundComposite*>(ms_pFocusPed->GetRagdollInst()->GetArchetype()->GetBound()),
		"Ragdoll physics archetype should have composite bound");
	phBoundComposite* pCompBound = static_cast<phBoundComposite*>(ms_pFocusPed->GetRagdollInst()->GetArchetype()->GetBound());

	// Draw the collision bounds:
	for(int i = 0; i < m_nNumBounds; ++i)
	{
		phBound* pBound = pCompBound->GetBound(i);
		phBoundCapsule* pBndCap = NULL;
		phBoundBox* pBndBox = NULL;
		Vec3V v1, v2, vCentroidOffset, vCentroidOffsetWorldSpace;
		ScalarV vCapsuleLength;
		Vec3V vHalfCapsuleLengthY;
		Mat34V centerMtx;
		switch(pBound->GetType())
		{
		case phBound::CAPSULE:
			pBndCap = static_cast<phBoundCapsule*>(pBound);
			vCentroidOffset = pBndCap->GetCentroidOffset();
			vCapsuleLength = pBndCap->GetLengthV();
			vCentroidOffsetWorldSpace = Transform(RCC_MAT34V(ms_currentMatrices[i]), vCentroidOffset);
			vHalfCapsuleLengthY = And(Vec3V(vCapsuleLength), Vec3V(V_MASKY));
			vHalfCapsuleLengthY *= ScalarV(V_HALF);

			v1 = vHalfCapsuleLengthY;
			v2 = -vHalfCapsuleLengthY;
			v1 = Add(v1, vCentroidOffset);
			v2 = Add(v2, vCentroidOffset);
			v1 = Transform(RCC_MAT34V(ms_currentMatrices[i]), v1);
			v2 = Transform(RCC_MAT34V(ms_currentMatrices[i]), v2);
			grcDebugDraw::Line(v1, v2, Color_purple);
			grcDebugDraw::Sphere(v1, pBndCap->GetRadius(), Color_purple, bSolid);
			grcDebugDraw::Sphere(v2, pBndCap->GetRadius(), Color_purple, bSolid);
			break;
		case phBound::BOX:
			pBndBox = static_cast<phBoundBox*>(pBound);
			v1 = pBndBox->GetBoundingBoxMin();
			v2 = pBndBox->GetBoundingBoxMin();	// TODO: Should that be Max() instead? /MAK
			centerMtx = RCC_MAT34V(ms_currentMatrices[i]);
			centerMtx.SetCol3(Transform(RCC_MAT34V(ms_currentMatrices[i]), pBndBox->GetCentroidOffset()));
			grcDebugDraw::BoxOriented(v1, v2, centerMtx, Color_purple, bSolid);
		default:
			grcDebugDraw::Sphere(ms_currentMatrices[i].d, 0.02f, Color_purple);
			break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::SetComponentTMsFromSkeleton(const crSkeleton& skeleton)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(!ms_pFocusPed)
		return;

	// Does this ped have a Natural Motion agent?
	int nAgentId = ms_pFocusPed->GetRagdollInst()->m_AgentId;

	if(ms_bDrawTransforms && nAgentId != -1)
	{
		aiAssert(ms_pFocusPed->GetRagdollInst()->GetType());

		int numChildren = ms_pFocusPed->GetRagdollInst()->GetTypePhysics()->GetNumChildren();
		aiAssert(numChildren <= RAGDOLL_NUM_COMPONENTS);
		m_nNumBounds = numChildren;
		Matrix34* currentMatrices = FRAGNMASSETMGR->GetWorldCurrentMatrices(nAgentId);

		// Go through each fragTypeChild/bound component ...
		for(int childIndex = 0; childIndex < numChildren; ++childIndex)
		{
			fragTypeChild* child = ms_pFocusPed->GetRagdollInst()->GetTypePhysics()->GetAllChildren()[childIndex];
			int boneIndex = ms_pFocusPed->GetRagdollInst()->GetType()->GetBoneIndexFromID(child->GetBoneID());
			Assert(boneIndex >= 0);

			const Matrix34* pattachment = ART::getComponentToBoneTransform(nAgentId, childIndex);
			Assertf(pattachment, "Failed to find attachment matrix from getComponentToBoneTransform");
			Matrix34 attachment;
			if (pattachment)
			{
				attachment = *pattachment;
				attachment.Inverse();
			}
			else
			{
				attachment.Identity();
			}

			Matrix34 boneMtx;
			skeleton.GetGlobalMtx(boneIndex, RC_MAT34V(boneMtx));
			currentMatrices[childIndex] = attachment;
			currentMatrices[childIndex].Dot(boneMtx);
			currentMatrices[childIndex].a.w = 0.0f;
			currentMatrices[childIndex].b.w = 0.0f;
			currentMatrices[childIndex].c.w = 0.0f;
			currentMatrices[childIndex].d.w = 1.0f;

			ms_currentMatrices[childIndex].Set(currentMatrices[childIndex]);
			/*
			// Store the necessary geometric components to draw the bounds later.
			if(type->GetCompositeBounds()->GetBound(childIndex)->GetType() == phBound::CAPSULE)
			{
			phBoundCapsule* pBound = static_cast<phBoundCapsule*>(type->GetCompositeBounds()->GetBound(childIndex));
			ms_vBoundStart[childIndex].Set(pBound->GetWorldPoint(0));
			ms_vBoundEnd[childIndex].Set(pBound->GetWorldPoint(1));
			ms_fBoundRadius[childIndex] = pBound->GetRadius();
			}
			else
			{
			ms_vBoundStart[childIndex].Set(currentMatrices[childIndex].d);
			ms_vBoundEnd[childIndex].Set(currentMatrices[childIndex].d);
			ms_fBoundRadius[childIndex] = 0.05f;
			}
			*/
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::AddBehaviourFeedbackMessage(const char* zMessage)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Add a new message to the history and, if the list is already at full capacity, remove the oldest message.

	if(ms_feedbackMessageHistory.ms_nCount < NUM_FEEDBACK_HISTORY_ELEMENTS)
	{
		strcpy(ms_feedbackMessageHistory.ms_aMessages[ms_feedbackMessageHistory.ms_nCount], zMessage);
		ms_feedbackMessageHistory.ms_nAgeOfMessage[ms_feedbackMessageHistory.ms_nCount] = 0;
		ms_feedbackMessageHistory.ms_nCount++;
	}
	else
	{
		// Remove the oldest message by overwriting with the new.
		strcpy(ms_feedbackMessageHistory.ms_aMessages[ms_feedbackMessageHistory.ms_nOldestIndex], zMessage);
		ms_feedbackMessageHistory.ms_nAgeOfMessage[ms_feedbackMessageHistory.ms_nOldestIndex] = 0;
		ms_feedbackMessageHistory.ms_nOldestIndex++;
		ms_feedbackMessageHistory.ms_nNewestIndex++;
		if(ms_feedbackMessageHistory.ms_nOldestIndex == NUM_FEEDBACK_HISTORY_ELEMENTS) ms_feedbackMessageHistory.ms_nOldestIndex = 0;
		if(ms_feedbackMessageHistory.ms_nNewestIndex == NUM_FEEDBACK_HISTORY_ELEMENTS) ms_feedbackMessageHistory.ms_nNewestIndex = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::RenderFeedbackHistory()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Display a history of feedback strings sent from the NM system.

	// Display the title message:
	char sFlagDebugString[] = "NM FEEDBACK MESSAGES:";

	grcDebugDraw::Text(Vector2(ms_fListHeaderX, ms_fListHeaderY), Color32(0xff, 0xff, 0xff, 0xff), sFlagDebugString, true);

	int nMessageIndex = ms_feedbackMessageHistory.ms_nOldestIndex;
	int nTextColour;
	for(int i = 0; i < ms_feedbackMessageHistory.ms_nCount; ++i)
	{
		// Determine grey-scale colour based on age of message in history and fade rate.
		if(ms_feedbackMessageHistory.ms_nAgeOfMessage[nMessageIndex] < ms_nColourFadeStartTick)
		{
			nTextColour = 0xff;
		}
		else if(ms_feedbackMessageHistory.ms_nAgeOfMessage[nMessageIndex] > ms_nColourFadeEndTick)
		{
			nTextColour = ms_nEndFadeColour;
		}
		else
		{
			u32 nTotalFadePeriod = ms_nColourFadeEndTick - ms_nColourFadeStartTick;
			u32 nFadeTime = ms_feedbackMessageHistory.ms_nAgeOfMessage[nMessageIndex] - ms_nColourFadeStartTick;
			float fFadeFactor = 1.0f - (float)nFadeTime/(float)nTotalFadePeriod;
			nTextColour = ms_nEndFadeColour + (u32)(fFadeFactor*(float)(0xff-ms_nEndFadeColour));
		}

		grcDebugDraw::Text(Vector2(ms_fListHeaderX, ms_fListHeaderY + (i+1)*ms_fListElementHeight), Color32(nTextColour, nTextColour, nTextColour, 0xff),
			ms_feedbackMessageHistory.ms_aMessages[nMessageIndex], false);
		nMessageIndex++;
		if(nMessageIndex == NUM_FEEDBACK_HISTORY_ELEMENTS) nMessageIndex = 0;

		// Update the age of this message.
		if(!fwTimer::IsGamePaused())
			ms_feedbackMessageHistory.ms_nAgeOfMessage[nMessageIndex]++;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::RenderRagdollComponentMatrices()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(!ms_pFocusPed)
		return;

	Matrix34 ragdollComponentMatrix;
	if(ms_pFocusPed->GetRagdollComponentMatrix(ragdollComponentMatrix, ms_nSelectedRagdollComponent))
	{
		grcDebugDraw::Axis(ragdollComponentMatrix, 0.5f, true);
	}
	else
	{
		taskAssertf(false, "GetRagdollComponentMatrix() returned \"false\".");
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::RenderEdgeDetectionResults()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(!ms_pFocusPed)
		return;

	bool bEdgeDefined = false;
	Vector3 vEdgeLeft, vEdgeRight, vEdgeMiddle;

	Vector3 vPedPos = VEC3V_TO_VECTOR3(ms_pFocusPed->GetTransform().GetPosition());
	// This is a normal vector in the direction the ped is facing (eventually to be the direction of motion).
	Vector3 vPedDirNormal(rage::Sinf(ms_fEdgeTestAngle), rage::Cosf(ms_fEdgeTestAngle), 0.0f);  //ms_pFocusPed->GetB();
	// Rotate the direction vector above to get the directions for the left and right extrema probes.
	/*
	static dev_float sfMaxProbeAngle = 30.0f * PI/180.0f;
	Vector3 vLeftProbeDir = vPedDirNormal; vLeftProbeDir.RotateZ(sfMaxProbeAngle)
	Vector3 vRightProbeDir = vPedDirNormal; vRightProbeDir.RotateZ(-sfMaxProbeAngle);
	*/

	const float fPelvisToGround = vPedPos.z - ms_pFocusPed->GetGroundPos().z;

	Vector3 vProbe0Start, vProbe0End;
	Vector3 vProbe1Start, vProbe1End;
	Vector3 vProbe2Start, vProbe2End;
	static dev_float sfHorProbeLength = 3.0f;
	static dev_float sfCriticalDropHeight = 2.0f; // Define the critical height difference which triggers a teeter reaction.

	// Define start and end points for horizontal probe at hip height:
	vProbe0Start = vPedPos;
	vProbe0End.Add(vProbe0Start, vPedDirNormal*sfHorProbeLength);

	// Define start and end points for horizontal probe at ankle height:
	vProbe1Start = vPedPos;
	static dev_float sfDistBetweenHipAndAnkleProbes = 1.0f;
	vProbe1Start.Subtract(Vector3(ZAXIS)*sfDistBetweenHipAndAnkleProbes);
	vProbe1End.Add(vProbe1Start, vPedDirNormal*sfHorProbeLength);

	// Define start and end points for vertical probe at end of horizontal probes.
	vProbe2Start = vProbe0End;
	vProbe2End = vProbe2Start; vProbe2End.z -= sfCriticalDropHeight;

	// Probe along the horizontal lines defined above looking for scenery.
	WorldProbe::CShapeTestProbeDesc probe0;
	WorldProbe::CShapeTestFixedResults<> probe0Results;
	probe0.SetStartAndEnd(vProbe0Start, vProbe0End);
	probe0.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probe0.SetResultsStructure(&probe0Results);
	WorldProbe::GetShapeTestManager()->SubmitTest(probe0, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
	//
	WorldProbe::CShapeTestProbeDesc probe1;
	WorldProbe::CShapeTestFixedResults<> probe1Results;
	probe1.SetStartAndEnd(vProbe1Start, vProbe1End);
	probe1.SetResultsStructure(&probe1Results);
	probe1.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	WorldProbe::GetShapeTestManager()->SubmitTest(probe1, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

	// Do a vertical test at the end of the horizontal tests to see if we are near a significant drop.
	if(!probe0Results[0].GetHitDetected()) // Is it safe to test vertically down from here?
	{
		WorldProbe::CShapeTestProbeDesc probe2;
		WorldProbe::CShapeTestFixedResults<> probe2Results;
		probe2.SetStartAndEnd(vProbe2Start, vProbe2End);
		probe2.SetResultsStructure(&probe2Results);
		probe2.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		WorldProbe::GetShapeTestManager()->SubmitTest(probe2, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

		// If we found a drop, fire rays back to detect the edge.
		if(!probe2Results[0].GetHitDetected())
		{
			Vector3 vProbeFindEdgeStart, vProbeFindEdgeEndL, vProbeFindEdgeEndR;
			//	// Should be along the same line as the previous horizontal probes but just under the
			//	// level of the ground where the ped is standing.
			vProbeFindEdgeStart = vProbe0End; vProbeFindEdgeStart.z = vPedPos.z - fPelvisToGround;
			vProbeFindEdgeEndL = vProbe0Start; vProbeFindEdgeEndL.z -= 1.0f;
			vProbeFindEdgeEndR = vProbe0Start; vProbeFindEdgeEndR.z -= 1.0f;

			static dev_float sfCapsuleRadius = 1.0f;

			// Use two line tests to define the end points along the edge which we will pass to NM.
			Vector3 vEdgeLeftProbeStart, vEdgeLeftProbeEnd;
			Vector3 vEdgeRightProbeStart, vEdgeRightProbeEnd;
			Vector3 vEdgeMiddleProbeStart, vEdgeMiddleProbeEnd;

			vEdgeLeftProbeStart = vProbeFindEdgeStart; vEdgeLeftProbeStart.z -= sfCapsuleRadius;
			vEdgeRightProbeStart = vProbeFindEdgeStart; vEdgeRightProbeStart.z -= sfCapsuleRadius;
			vEdgeMiddleProbeStart = vProbeFindEdgeStart; vEdgeMiddleProbeStart.z -= sfCapsuleRadius;

			Vector3 vOrthoTestLine = vPedDirNormal; vOrthoTestLine.RotateZ(PI/2.0f);
			vEdgeLeftProbeEnd.Add(vProbeFindEdgeEndL, vOrthoTestLine*sfCapsuleRadius);
			vEdgeRightProbeEnd.Add(vProbeFindEdgeEndR, vOrthoTestLine*-sfCapsuleRadius);
			vEdgeMiddleProbeEnd = ms_pFocusPed->GetGroundPos();

			// Define the roughly horizontal probes to define the edge, look for corners, etc.
			WorldProbe::CShapeTestProbeDesc probeEdgeLeft;
			WorldProbe::CShapeTestFixedResults<> probeEdgeLeftResults;
			probeEdgeLeft.SetStartAndEnd(vEdgeLeftProbeStart, vEdgeLeftProbeEnd);
			probeEdgeLeft.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeEdgeLeft.SetResultsStructure(&probeEdgeLeftResults);
			WorldProbe::GetShapeTestManager()->SubmitTest(probeEdgeLeft, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			//
			WorldProbe::CShapeTestProbeDesc probeEdgeRight;
			WorldProbe::CShapeTestFixedResults<> probeEdgeRightResults;
			probeEdgeRight.SetStartAndEnd(vEdgeRightProbeStart, vEdgeRightProbeEnd);
			probeEdgeRight.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeEdgeRight.SetResultsStructure(&probeEdgeRightResults);
			WorldProbe::GetShapeTestManager()->SubmitTest(probeEdgeRight, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			//
			WorldProbe::CShapeTestProbeDesc probeEdgeMiddle;
			WorldProbe::CShapeTestFixedResults<> probeEdgeMiddleResults;
			probeEdgeMiddle.SetStartAndEnd(vEdgeMiddleProbeStart, vEdgeMiddleProbeEnd);
			probeEdgeMiddle.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeEdgeMiddle.SetResultsStructure(&probeEdgeMiddleResults);
			WorldProbe::GetShapeTestManager()->SubmitTest(probeEdgeMiddle, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

			// Visualise probe and collision results.
			grcDebugDraw::Line(vEdgeLeftProbeStart, vEdgeLeftProbeEnd, probeEdgeLeftResults[0].GetHitDetected() ? Color_red : Color_green);
			grcDebugDraw::Line(vEdgeRightProbeStart, vEdgeRightProbeEnd, probeEdgeRightResults[0].GetHitDetected() ? Color_red : Color_blue);
			grcDebugDraw::Line(vEdgeMiddleProbeStart, vEdgeMiddleProbeEnd, probeEdgeMiddleResults[0].GetHitDetected() ? Color_red : Color_yellow);
			// And (barring a test for being at a corner) we should have the end-points of a line along the edge.
			//float fLeftHeading = 0.0f, fRightHeading = 0.0f;
			if(probeEdgeLeftResults[0].GetHitDetected())
			{
				vEdgeLeft = probeEdgeLeftResults[0].GetHitPosition(); vEdgeLeft.z = ms_pFocusPed->GetGroundPos().z;
				grcDebugDraw::Sphere(vEdgeLeft, 0.05f, Color_green, true);

				// Work out the heading of the line between this point and the intersection of the capsule test.
				/*Vector3 vLeftHeading = capsuleIsect.GetPosition();
				vLeftHeading.Subtract(vEdgeLeft);
				fLeftHeading = atan(vLeftHeading.y/vLeftHeading.x);
				if(fLeftHeading > PI) fLeftHeading -= PI;*/
			}
			if(probeEdgeRightResults[0].GetHitDetected())
			{
				vEdgeRight = probeEdgeRightResults[0].GetHitPosition(); vEdgeRight.z = ms_pFocusPed->GetGroundPos().z;
				grcDebugDraw::Sphere(vEdgeRight, 0.05f, Color_blue, true);

				// Work out the heading of the line between this point and the intersection of the capsule test.
				/*Vector3 vRightHeading = capsuleIsect.GetPosition();
				vRightHeading.Subtract(vEdgeRight);
				fRightHeading = atan(vRightHeading.y/vRightHeading.x);
				if(fRightHeading > PI) fRightHeading -= PI;*/
			}
			if(probeEdgeMiddleResults[0].GetHitDetected())
			{
				vEdgeMiddle = probeEdgeMiddleResults[0].GetHitPosition(); vEdgeMiddle.z = ms_pFocusPed->GetGroundPos().z;
				grcDebugDraw::Sphere(vEdgeMiddle, 0.05f, Color_red, true);
			}


			if( probeEdgeLeftResults[0].GetHitDetected() && probeEdgeRightResults[0].GetHitDetected())
			{
				bEdgeDefined = true;
			}
			else if( probeEdgeLeftResults[0].GetHitDetected())
			{
				vEdgeRight = vEdgeMiddle;
				bEdgeDefined = true;
			}
			else if(probeEdgeRightResults[0].GetHitDetected())
			{
				vEdgeLeft = vEdgeMiddle;
				bEdgeDefined = true;
			}
			else
			{
				bEdgeDefined = false;
			}
		}
		// Visualise probe and collision results.
		grcDebugDraw::Line(vProbe2Start, vProbe2End, probe2Results[0].GetHitDetected() ? Color_red : Color_yellow);


		// Visualise any collisions with scenery.
		grcDebugDraw::Line(vProbe0Start, vProbe0End, probe0Results[0].GetHitDetected() ? Color_red : Color_yellow);
		grcDebugDraw::Line(vProbe1Start, vProbe1End, probe1Results[0].GetHitDetected() ? Color_red : Color_yellow);
		if(probe0Results[0].GetHitDetected())
		{
			grcDebugDraw::Sphere(probe0Results[0].GetHitPosition(), 0.05f, Color_red, true);
		}
		if(probe1Results[0].GetHitDetected())
		{
			grcDebugDraw::Sphere(probe1Results[0].GetHitPosition(), 0.05f, Color_red, true);
		}

		// Visualise end result if edge detected.
		if(bEdgeDefined)
		{
			Vector3 v1(vEdgeLeft.x, vEdgeLeft.y, vEdgeLeft.z+0.2f);
			Vector3 v2(vEdgeRight.x, vEdgeRight.y, vEdgeRight.z+0.2f);
			grcDebugDraw::Line(v1,	v2, Color_white);
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CNmDebug::RenderMoreEfficientEdgeDetectionResults()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(!ms_pFocusPed)
		return;

	bool bEdgeDefined = false;

	static dev_float sfEdgeProbeSeparationAngle = 60.0f; // Angle in degrees between the left and right probes.
	static dev_float sfCriticalDepth = 1.5f; // Critical height difference from pelvis to ground to consider a ledge.
	static dev_float sfEdgeHorDistThreshold = 0.5f; // The horizontal distance from an edge at which the drop is first detected.
	const float fPelvisToGround = 0.5f;
	// Compute quantities derived from the above parameters.
	float fTheta = tan(sfEdgeHorDistThreshold/fPelvisToGround);
	float fProbeHorDist = sfCriticalDepth*atan(fTheta);

	// TEMP!!!!!
	static Vector3 s_vLeftEdgePoint(Vector3::ZeroType);
	static Vector3 s_vMiddleEdgePoint(Vector3::ZeroType);
	static Vector3 s_vRightEdgePoint(Vector3::ZeroType);
	static Vector3 s_vEdgeLineLeft(Vector3::ZeroType);
	static Vector3 s_vEdgeLineRight(Vector3::ZeroType);
	////////////

	Vector3 vPedPos = VEC3V_TO_VECTOR3(ms_pFocusPed->GetTransform().GetPosition());
	// This is a normal vector in the direction the ped is facing (eventually to be the direction of motion).
	//Vector3 vPedDirNormal(rage::Sinf(ms_fEdgeTestAngle), rage::Cosf(ms_fEdgeTestAngle), 0.0f);
	Vector3 vPedDirNormal = VEC3V_TO_VECTOR3(ms_pFocusPed->GetTransform().GetB());

	// Define the start and end points of the line tests to look for a large drop.
	Vector3 vProbeStart = vPedPos;
	//
	Vector3 vProbeEndMiddle;
	vProbeEndMiddle.Add(vPedDirNormal*fProbeHorDist, vProbeStart);
	vProbeEndMiddle.z -= sfCriticalDepth;
	//
	Vector3 vProbeEndLeft, vProbeLeftDirNormal;
	vProbeLeftDirNormal = vPedDirNormal;
	vProbeLeftDirNormal.RotateZ(0.5f*sfEdgeProbeSeparationAngle*PI/180.0f);
	vProbeEndLeft.Add(vProbeLeftDirNormal*fProbeHorDist, vProbeStart);
	vProbeEndLeft.z -= sfCriticalDepth;
	//
	Vector3 vProbeEndRight, vProbeRightDirNormal;
	vProbeRightDirNormal = vPedDirNormal;
	vProbeRightDirNormal.RotateZ(-0.5f*sfEdgeProbeSeparationAngle*PI/180.0f);
	vProbeEndRight.Add(vProbeRightDirNormal*fProbeHorDist, vProbeStart);
	vProbeEndRight.z -= sfCriticalDepth;
	//
	grcDebugDraw::Sphere(vProbeStart, 0.03f, Color_blue, true);
	grcDebugDraw::Sphere(vProbeEndLeft, 0.03f, Color_blue, true);
	grcDebugDraw::Sphere(vProbeEndRight, 0.03f, Color_blue, true);
	grcDebugDraw::Sphere(vProbeEndMiddle, 0.03f, Color_blue, true);
	grcDebugDraw::Line(vProbeStart, vProbeEndLeft, Color_yellow);
	grcDebugDraw::Line(vProbeStart, vProbeEndRight, Color_yellow);
	grcDebugDraw::Line(vProbeStart, vProbeEndMiddle, Color_yellow);

	// Probe along the horizontal lines defined above looking for scenery.
	WorldProbe::CShapeTestProbeDesc probeLeft;
	WorldProbe::CShapeTestFixedResults<> probeResultLeft;
	probeLeft.SetResultsStructure(&probeResultLeft);
	probeLeft.SetStartAndEnd(vProbeStart, vProbeEndLeft);
	probeLeft.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	//
	WorldProbe::CShapeTestProbeDesc probeRight;
	WorldProbe::CShapeTestFixedResults<> probeResultRight;
	probeRight.SetResultsStructure(&probeResultRight);
	probeRight.SetStartAndEnd(vProbeStart, vProbeEndRight);
	probeRight.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	//
	WorldProbe::CShapeTestProbeDesc probeMiddle;
	WorldProbe::CShapeTestFixedResults<> probeResultMiddle;
	probeMiddle.SetResultsStructure(&probeResultMiddle);
	probeMiddle.SetStartAndEnd(vProbeStart, vProbeEndMiddle);
	probeMiddle.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	//
	WorldProbe::GetShapeTestManager()->SubmitTest(probeLeft);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeRight);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeMiddle);

	static bool bLeftFoundDrop = false;
	static bool bRightFoundDrop = false;
	static bool bMiddleFoundDrop = false;
	static int nFirstDropDetection = -1;
	if(probeResultLeft[0].GetHitDetected())
	{
		grcDebugDraw::Sphere(probeResultLeft[0].GetHitPosition(), 0.05f, Color_red, true);
		s_vLeftEdgePoint = probeResultLeft[0].GetHitPosition();
		bLeftFoundDrop = false;
	}
	else
	{
		// "No hit" means we are near a drop.
		bLeftFoundDrop = true;
		// The last two probes to find the edge define it.
		if(!bRightFoundDrop && !bMiddleFoundDrop)
		{
			nFirstDropDetection = 0;
		}

		grcDebugDraw::Sphere(s_vLeftEdgePoint, 0.05f, Color_purple, true);
	}
	if(probeResultRight[0].GetHitDetected())
	{
		grcDebugDraw::Sphere(probeResultRight[0].GetHitPosition(), 0.05f, Color_red, true);
		s_vRightEdgePoint = probeResultRight[0].GetHitPosition();
		bRightFoundDrop = false;
	}
	else
	{
		bRightFoundDrop = true;
		if(!bLeftFoundDrop && !bMiddleFoundDrop)
		{
			nFirstDropDetection = 1;
		}

		grcDebugDraw::Sphere(s_vRightEdgePoint, 0.05f, Color_grey, true);
	}
	if(probeResultMiddle[0].GetHitDetected())
	{
		bMiddleFoundDrop = false;
		grcDebugDraw::Sphere(probeResultMiddle[0].GetHitPosition(), 0.05f, Color_red, true);
		s_vMiddleEdgePoint = probeResultMiddle[0].GetHitPosition();
	}
	else
	{
		bMiddleFoundDrop = true;
		if(!bLeftFoundDrop && !bRightFoundDrop)
		{
			nFirstDropDetection = 2;
		}

		grcDebugDraw::Sphere(s_vMiddleEdgePoint, 0.05f, Color_green, true);
	}
	
	if(nFirstDropDetection == 0 && bRightFoundDrop && bMiddleFoundDrop)
	{
		s_vEdgeLineLeft = s_vMiddleEdgePoint;
		s_vEdgeLineRight = s_vRightEdgePoint;
		bEdgeDefined = true;
	}
	else if(nFirstDropDetection == 1 && bLeftFoundDrop && bMiddleFoundDrop)
	{
		s_vEdgeLineLeft = s_vLeftEdgePoint;
		s_vEdgeLineRight = s_vMiddleEdgePoint;
		bEdgeDefined = true;
	}
	else if(nFirstDropDetection == 2 && bLeftFoundDrop && bRightFoundDrop)
	{
		s_vEdgeLineLeft = s_vLeftEdgePoint;
		s_vEdgeLineRight = s_vRightEdgePoint;
		bEdgeDefined = true;
	}
	else
	{
		bEdgeDefined = false;
	}

	if(bEdgeDefined)
	{
		grcDebugDraw::Line(s_vEdgeLineLeft, s_vEdgeLineRight, Color_white);
	}
}
#endif // __BANK