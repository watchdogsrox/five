// Rage headers
#include "math/angmath.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"
#include "fwmaths/Vector.h"
 
// Game headers
#include "Animation/MovePed.h"
#include "camera/CamInterface.h"
#include "camera/helpers/Frame.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Movement/Climbing/TaskGoToAndClimbLadder.h"
#include "Task/Movement/Climbing/TaskLadderClimb.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskNavBase.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Animation/TaskAnims.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/Ped.h"
#include "Physics/gtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "fwscene/search/SearchVolumes.h"
#include "debug/DebugScene.h"
#include "Vfx/misc/LODLightManager.h" // hashing func in GenerateHash() 
#include "physics/WorldProbe/shapetestbounddesc.h"
#include "Physics/WaterTestHelper.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//float	CTaskClimbLadder::ms_fLadderMoveScale								= 1.00391531f;

/* static const char* climbModeNames[] = 
{
	"EUndefined",
	"EGetOnAtBottom",
	"EGetOffAtBottom",
	"EOnLadder",
	"EGetOnAtTop",
	"EGetOffAtTop",
	"EMaxClimbMode"
}; */

#if __BANK

bool CClimbLadderDebug::ms_bRenderDebug	= false;

#if DEBUG_DRAW
#define MAX_LADDER_DRAWABLES 50
CDebugDrawStore CClimbLadderDebug::ms_debugDraw(MAX_LADDER_DRAWABLES);
#endif // DEBUG_DRAW

void CClimbLadderDebug::DrawLadderInfo(CEntity* pLadderEntity, int iLadderIndex)
{
	if (pLadderEntity)
	{
		// Go through the effects one by one
		CBaseModelInfo* pBaseModel = pLadderEntity->GetBaseModelInfo();

		C2dEffect* pEffect = pBaseModel->Get2dEffect(iLadderIndex);

		if (pEffect->GetType() == ET_LADDER)
		{
			CLadderInfo* ldr = pEffect->GetLadder();

			Vector3 vPosAtTop, vPosAtBottom, vLadderNormal;
			ldr->vTop.Get(vPosAtTop);
			ldr->vBottom.Get(vPosAtBottom);
			ldr->vNormal.Get(vLadderNormal);

			TUNE_BOOL(USE_OVERRIDDEN_NORMAL, false);
			if (USE_OVERRIDDEN_NORMAL)
			{
				vLadderNormal = Vector3(0.0f,-1.0f,0.0f);
			}

			char szDebugText[128];
			formatf(szDebugText, "Top : (%.2f,%.2f,%.2f)", vPosAtTop.x, vPosAtTop.y, vPosAtTop.z);
			grcDebugDraw::AddDebugOutput(Color_green, szDebugText);
			formatf(szDebugText, "Bottom : (%.2f,%.2f,%.2f)", vPosAtBottom.x, vPosAtBottom.y, vPosAtBottom.z);
			grcDebugDraw::AddDebugOutput(Color_green, szDebugText);
			formatf(szDebugText, "Normal : (%.2f,%.2f,%.2f)", vLadderNormal.x, vLadderNormal.y, vLadderNormal.z);
			grcDebugDraw::AddDebugOutput(Color_green, szDebugText);

			const Matrix34 mat = MAT34V_TO_MATRIX34(pLadderEntity->GetMatrix());

			// Transform positions into worldspace and render
			mat.Transform(vPosAtTop);
			mat.Transform(vPosAtBottom);

			grcDebugDraw::Sphere(vPosAtTop, 0.05f, Color_blue);
			grcDebugDraw::Sphere(vPosAtBottom, 0.05f, Color_green);

			// Calculate the ladder normal in world space
			mat.Transform3x3(vLadderNormal);

			Matrix34 BottomNormalMatrix(M34_IDENTITY); 
			Vector3 vUp (0.0f, 0.0f, 1.0f); 
			BottomNormalMatrix.d = vPosAtBottom; 
	
			camFrame::ComputeWorldMatrixFromFront(vLadderNormal, BottomNormalMatrix); 

			Matrix34 TopNormalMatrix(M34_IDENTITY); 
			
			TopNormalMatrix.d = vPosAtTop; 

			camFrame::ComputeWorldMatrixFromFront(vLadderNormal, TopNormalMatrix); 

			grcDebugDraw::Axis(BottomNormalMatrix, 1.0f, true);
			
			grcDebugDraw::Axis(TopNormalMatrix, 1.0f, true);

			//grcDebugDraw::Arrow(RCC_VEC3V(vPosAtTop), VECTOR3_TO_VEC3V(vPosAtTop + vLadderNormal), 0.05f, Color_blue);

			//grcDebugDraw::Arrow(RCC_VEC3V(vPosAtTop), VECTOR3_TO_VEC3V(vPosAtTop + vLadderNormal), 0.05f, Color_blue);
			//grcDebugDraw::Arrow(RCC_VEC3V(vPosAtBottom), VECTOR3_TO_VEC3V(vPosAtBottom + vLadderNormal), 0.05f, Color_green);

			// The get on heading is 180 degrees from the ladder normal
			float fGetOnLadderHeading = fwAngle::GetRadianAngleBetweenPoints(0.0f, 0.0f, vLadderNormal.x, vLadderNormal.y);

			formatf(szDebugText, "Heading : %.2f", fGetOnLadderHeading);
			grcDebugDraw::AddDebugOutput(Color_green, szDebugText);

			Vector3 vHeading(0.0f,1.0f,0.0f);
			vHeading.RotateZ(fGetOnLadderHeading);
		//	grcDebugDraw::Arrow(RCC_VEC3V(vPosAtBottom), VECTOR3_TO_VEC3V(vPosAtBottom + vHeading), 0.1f, Color_red);

			// Do a line test from the calculated get off point to work out the actual ladder top
// 			Vector3 vGetOffClipDisplacement = fwAnimHelpers::GetMoverTrackTranslationDiff(CLIP_SET_CLIMB_PED, CLIP_LADDER_GET_OFF_TOP, 0.f, 1.f); 

			Matrix34 mat2;
			mat2.MakeRotateZ(fGetOnLadderHeading);
// 			mat2.Transform3x3(vGetOffClipDisplacement);
// 			TUNE_BOOL(ALTER_Z_DISPLACEMENT_DEBUG, false);
// 			if (ALTER_Z_DISPLACEMENT_DEBUG)
// 			{
// 				TUNE_FLOAT(EXTRA_Z_DISPLACEMENT_DEBUG, 0.0f, -5.0f, 5.0f, 0.01f);
// 				vGetOffClipDisplacement.z += EXTRA_Z_DISPLACEMENT_DEBUG;
// 			}

			Vector3 vGetOffStartPos = vPosAtTop;
			TUNE_FLOAT(PED_MOVER_DISTANCE_FROM_LADDER_Y, 0.5f, 0.0f, 1.0f, 0.01f);
			vGetOffStartPos += vLadderNormal*PED_MOVER_DISTANCE_FROM_LADDER_Y;
			TUNE_FLOAT(PED_MOVER_DISTANCE_FROM_LADDER_Z, 0.5f, 0.0f, 1.0f, 0.01f);
			vGetOffStartPos.z -= PED_MOVER_DISTANCE_FROM_LADDER_Z;

			//grcDebugDraw::Sphere(vGetOffStartPos, 0.05f, Color_orange);
			Vector3 vGetOffEndPos = vGetOffStartPos/* + vGetOffClipDisplacement*/;
			//grcDebugDraw::Arrow(RCC_VEC3V(vGetOffStartPos), RCC_VEC3V(vGetOffEndPos), 0.05f, Color_yellow);
			//grcDebugDraw::Sphere(RCC_VEC3V(vGetOffEndPos), 0.05f, Color_red);
			
			Vec3V vX(0.5f,0.0f,0.0f);
			Vec3V vY(0.0f,0.5f,0.0f);

			static dev_float s_fAuthoredRungSeparation = 0.25;	// Calculated rung separation
		
			// Get the approximate number of rungs in this ladder
			float fLadderHeight = VECTOR3_TO_VEC3V(vPosAtTop).GetZf() - VECTOR3_TO_VEC3V(vPosAtBottom).GetZf();
			float fNumRungs = fLadderHeight / s_fAuthoredRungSeparation;
		
			// Round the number of rungs to the nearest whole number
			float fNumRungsCeil  = rage::Floorf(fNumRungs + 1.0f);
			float fNumRungsFloor = rage::Floorf(fNumRungs);
		
			// Use the number fNumRungs is closest to as the value 
			if (Abs(fNumRungsCeil - fNumRungs) < Abs(fNumRungsFloor - fNumRungs))
			{
				fNumRungs = fNumRungsCeil;
			}
			else
			{
				fNumRungs = fNumRungsFloor;
			}
		
			for (s32 i = 0; i < fNumRungs; ++i)
			{
				Vec3V vNewPos = VECTOR3_TO_VEC3V(vPosAtBottom);
				vNewPos.SetZ(vNewPos.GetZ() + ScalarVFromF32(i * s_fAuthoredRungSeparation));
				ms_debugDraw.AddLine(vNewPos-vX,vNewPos+vX,Color_LightSteelBlue1,5000);
				ms_debugDraw.AddLine(vNewPos-vY,vNewPos+vY,Color_LightSteelBlue1,5000);
				//grcDebugDraw::Line(vNewPos-vX,vNewPos+vX,Color_LightSteelBlue1);
				//grcDebugDraw::Line(vNewPos-vY,vNewPos+vY,Color_LightSteelBlue1);
			}
		}
	}
}

void CClimbLadderDebug::Debug()
{
	if (ms_bRenderDebug)
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

		s32 iLadderIndex;
		Vector3 vTarget; 
		CTaskGoToAndClimbLadder::ScanForLadderToClimb(*pFocusPed, iLadderIndex,vTarget);
	}
}

void CClimbLadderDebug::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{
		pBank->PushGroup("Climb Ladder");
			pBank->AddToggle("Render Debug",&ms_bRenderDebug);
			pBank->AddToggle("Use Easy Ladder Conditions In MP", &CTaskGoToAndClimbLadder::ms_bUseEasyLadderConditionsInMP);
			//pBank->PushGroup("Climb Ladder Task");
			//	//pBank->AddSlider("ClimbingClipRate",&CTaskClimbLadder::ms_fClimbingClipRate,0.0f,5.0f,0.1f);
			//	//pBank->AddSlider("MinDistForHardLanding",&CTaskClimbLadder::ms_fMinDistForHardLanding,0.0f,10.0f,0.1f);
			//	//pBank->AddSlider("HeightBeforeTopToFinish",&CTaskClimbLadder::ms_fHeightBeforeTopToFinish,0.0f,5.0f,0.01f);
			//	//pBank->AddSlider("HeightBeforeBottomToFinish",&CTaskClimbLadder::ms_fHeightBeforeBottomToFinish,0.0f,5.0f,0.01f);
			//pBank->PopGroup();

			//pBank->PushGroup("Go To And Climb Ladder Task");
			//	pBank->AddButton("Visualise nearby ladders",VisualiseNearbyLaddersCB);
			//	pBank->AddSlider("MaxSlideToLadderTime",&CTaskGoToAndClimbLadder::ms_iMaxSlideToLadderTime,0,5000,100);
			//	pBank->AddSlider("MaxTimeToWaitForNavMeshRoute",&CTaskGoToAndClimbLadder::ms_fMaxTimeToWaitForNavMeshRoute,0.0f,5.0f,0.1f);
			//	pBank->AddSlider("GettingOnOffsetAtBase",&CTaskGoToAndClimbLadder::ms_fGettingOnOffsetAtBase,0.0f,1.0f,0.01f);
			//	pBank->AddSlider("GettingOnOffsetAtTop",&CTaskGoToAndClimbLadder::ms_fGettingOnOffsetAtTop,0.0f,1.0f,0.01f);
			//	pBank->AddSlider("OffsetFrom2dFxToCapsule",&CTaskGoToAndClimbLadder::ms_fOffsetFrom2dFxToCapsule,0.0f,1.0f,0.01f);
			//	pBank->AddSlider("HeightBeforeBottomToFinish",&CTaskGoToAndClimbLadder::ms_fSlideOffset,0.0f,1.0f,0.01f);
			//pBank->PopGroup();
		pBank->PopGroup();
	}
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
// CTaskGoToAndClimbLadder
//////////////////////////////////////////////////////////////////////////

bank_s32	CTaskGoToAndClimbLadder::ms_iMaxSlideToLadderTime					= 1000;
bank_float	CTaskGoToAndClimbLadder::ms_fMaxTimeToWaitForNavMeshRoute			= 1.0f;
float		CTaskGoToAndClimbLadder::ms_fGettingOnOffsetAtBase					= 0.5f;
float		CTaskGoToAndClimbLadder::ms_fGettingOnOffsetAtTop					= 0.5f;
float		CTaskGoToAndClimbLadder::ms_fSlideOffset							= 0.1f;
float		CTaskGoToAndClimbLadder::ms_fOffsetFrom2dFxToCapsule				= 0.42f;
float		CTaskGoToAndClimbLadder::ms_fClimbingClipOffsetFromCentreOfLadder	= 0.5f;
float		CTaskGoToAndClimbLadder::ms_fGoToPointDist							= 0.2f;
bank_bool	CTaskGoToAndClimbLadder::ms_bUseEasyLadderConditionsInMP			= true;
atArray<RegdEnt> CTaskGoToAndClimbLadder::ms_Ladders; 

CTaskGoToAndClimbLadder::CTaskGoToAndClimbLadder(eAutoClimbMode autoClimb) :
m_eAutoClimbMode(autoClimb),
m_bGotOnAtBottom(false),
m_vPositionOfTop(0.0f, 0.0f, 0.0f),
m_vPositionOfBottom(0.0f, 0.0f, 0.0f),
m_vPositionToGetOn(0.0f, 0.0f, 0.0f),
m_fLadderHeading(0.0f),
m_eClimbMode(EUndefined),
m_eAIAction(InputState_Nothing),
m_bCanGetOffAtTop(false),
m_bCheckedNavMeshRoute(false),
m_bTopOfLadderIsTooHigh(false),
m_pLadder(NULL),
m_LadderIndex(-1),
m_getToLadderTimer(ms_fMaxTimeToWaitForNavMeshRoute),
m_bIsEasyEntry(false)
{
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER);
}

CTaskGoToAndClimbLadder::CTaskGoToAndClimbLadder(CEntity* pLadder, s32 ladderIndex, eAutoClimbMode autoClimb/*=DontAutoClimb*/, InputState aiAction/*=InputState_Nothing*/, bool bIsEasyEntry/* = false*/) :
m_eAutoClimbMode(autoClimb),
m_bGotOnAtBottom(false),
m_vPositionOfTop(0.0f,0.0f,0.0f),
m_vPositionOfBottom(0.0f,0.0f,0.0f),
m_vPositionToGetOn(0.0f,0.0f,0.0f),
m_fLadderHeading(0.0f),
m_eClimbMode(EUndefined),
m_eAIAction(aiAction),
m_bCanGetOffAtTop(false),
m_bCheckedNavMeshRoute(false),
m_bTopOfLadderIsTooHigh(false),
m_pLadder(pLadder),
m_LadderIndex(ladderIndex),
m_getToLadderTimer(ms_fMaxTimeToWaitForNavMeshRoute),
m_bIsEasyEntry(bIsEasyEntry)
{
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER);
}

CTaskGoToAndClimbLadder::~CTaskGoToAndClimbLadder()
{}

bool CTaskGoToAndClimbLadder::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	// Don't abort for entering water - continue climbing/descending
	if(pEvent && ((CEvent*)pEvent)->GetEventType()==EVENT_IN_WATER)
	{
		return false;
	}

	if(!GetSubTask())
		return true;

	return GetSubTask()->MakeAbortable(iPriority, pEvent);
}

bool CTaskGoToAndClimbLadder::IsClosestLadder(CEntity* pEntity, const CPed& ped, s32& ladderIndex, Vec3V& getOnPosition, ScalarV& closestDistance, bool returnNearestLadder /*= false*/)
{
	// First check if the entity is way off
	ScalarV maxXYEntityFromPed(25000.f);
	Mat34V mat = pEntity->GetMatrix();
	Vec3V pedPosition = ped.GetTransform().GetPosition();
	if(IsGreaterThanAll(MagXYSquared(pedPosition - mat.GetCol3()), maxXYEntityFromPed))
		return false;

	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	taskFatalAssertf(pModelInfo, "Invalid model info for ladder");
	taskAssertf(pModelInfo->GetIsLadder(), "%s:Is not a ladder.", pModelInfo->GetModelName());
	taskAssertf(pModelInfo->GetNum2dEffectsNoLights(), "%s:Ladder doesn't have any 2d Effects defined.", pModelInfo->GetModelName());
	GET_2D_EFFECTS_NOLIGHTS(pModelInfo);
	if(iNumEffects == 0)
	{
		return false; 
	}

	Vec3V pedFront = ped.GetTransform().GetB();
	ScalarV groundToRootOffset = ScalarVFromF32(ped.GetCapsuleInfo()->GetGroundToRootOffset());
	bool isNetworkClone = ped.IsNetworkClone() || returnNearestLadder;
	bool bFoundLadder = false; 

	for (s32 i = 0; i < iNumEffects; ++i)
	{
		const C2dEffect* pEffect = (*pa2dEffects)[i];
		if(pEffect->GetType() != ET_LADDER)
			continue;

		const CLadderInfo* pLadderInfo = static_cast<const CLadderInfo*>(pEffect);

		// Get top and bottom of ladder in local space
		Vec3V posAtTop = Transform(mat, Vec3V(Vec::V4LoadUnaligned(&pLadderInfo->vTop.x)));
		Vec3V posAtBottom = Transform(mat, Vec3V(Vec::V4LoadUnaligned(&pLadderInfo->vBottom.x)));

		// Some hack to disable weird ladders placed in the world
		{
			const Vec3V posToIgnore(2818.34f, 1473.76f, 31.58f);
			const Vec3V diff = posToIgnore - posAtTop;
			const ScalarV dist2 = MagSquared(diff);
			const ScalarV maxDist2(1.f);
			if(IsGreaterThanAll(maxDist2, dist2))	// If we are within X units we just skip this ladder
				continue;
		}

#if __BANK
		if(CClimbLadderDebug::ms_bRenderDebug)
		{
			CClimbLadderDebug::DrawLadderInfo(pEntity, i); 
		}
#endif // __BANK

		// TOP==0, BOTTOM==1
		Vec3V posToConsider = posAtTop;
		for (s32 ladder_end = 0; ladder_end < 2; ++ladder_end, posToConsider = posAtBottom)
		{
			Vec3V diff = posToConsider - pedPosition;
	
			ScalarV ZOffset = diff.GetZ() + groundToRootOffset; // get offset from peds feet rather than middle
				
			Vec3V XYDiff = And(Vec3V(V_MASKXY), diff);
				
			ScalarV XYDist = MagSquared(XYDiff);

			if(!isNetworkClone)
			{
				ScalarV maxXYDistFromPed(16.0f);	// Long distance for run get ons
				ScalarV maxZDistFromFeet(2.5f);	

				if(IsGreaterThanAll(XYDist, maxXYDistFromPed) || IsGreaterThanAll(Abs(ZOffset), maxZDistFromFeet))
					continue;

				ScalarV offsetDist = XYDist + Scale(ZOffset, ZOffset);
						
				Vec3V dir = Normalize(XYDiff);

				ScalarV dot = Dot(pedFront, dir);

				offsetDist *= ScalarV(V_ONE) - dot; // scale the score by how much we are pointing towards the ladder in case we are surrounded by a huge selection of ladders, low score is best though hence 1-dot

				if(IsGreaterThanAll(offsetDist, closestDistance))
					continue;

				bool bIsLOS = false;

				Vec3V dirToLadder = posAtBottom - pedPosition;

				// Work out whether we're closer to the top or bottom of the ladder
				ScalarV positionZ = pedPosition.GetZ() - ScalarV(V_ONE);
				ScalarV diffToTop = Abs(positionZ - posAtTop.GetZ());
				ScalarV diffToBottom = Abs(positionZ - posAtBottom.GetZ());

				if (ladder_end == 0 && IsLessThanAll(diffToTop, diffToBottom)) // We are mounting on top
				{
					if (IsLessThanAll(MagXYSquared(dirToLadder), ScalarVFromF32(6.25f)))
					{
						Vec3V ladderNormal = Transform3x3(mat, Vec3V(Vec::V4LoadUnaligned(&pLadderInfo->vNormal.x)));
					//	Vec3V posAtTopOffsetShort = posAtTop + ladderNormal * ScalarVFromF32(0.05f);
						Vec3V posAtTopOffset = posAtTop + ladderNormal * ScalarVFromF32(0.5f);

						WorldProbe::CShapeTestCapsuleDesc capsuleTest;
						capsuleTest.SetCapsule(VEC3V_TO_VECTOR3(posAtTopOffset + Vec3V(V_Z_AXIS_WZERO)), VEC3V_TO_VECTOR3(posAtTopOffset - Vec3V(V_Z_AXIS_WZERO) * Vec3VFromF32(2.5f)), 0.225f);
#if __BANK
						if(CClimbLadderDebug::ms_bRenderDebug)
						{
							grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(capsuleTest.GetStart()), VECTOR3_TO_VEC3V(capsuleTest.GetEnd()), 0.225f, Color_yellow, true, 1);
						}
#endif

						capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
						capsuleTest.SetIsDirected(false);
						capsuleTest.SetContext(WorldProbe::LOS_GeneralAI);
						const CEntity* ppExcludeEnts[2] = { &ped, pEntity };
						capsuleTest.SetExcludeEntities(ppExcludeEnts, 2);
						capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
						capsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_COLLISION);

						bIsLOS = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);

						// Also check if there is a wall in the way
						if (bIsLOS)
						{
							WorldProbe::CShapeTestCapsuleDesc capsuleTest;
							capsuleTest.SetCapsule(VEC3V_TO_VECTOR3(pedPosition), VEC3V_TO_VECTOR3(posAtTopOffset + Vec3V(V_Z_AXIS_WZERO)), 0.1f);
							
#if __BANK
							if(CClimbLadderDebug::ms_bRenderDebug)
							{
								grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(capsuleTest.GetStart()), VECTOR3_TO_VEC3V(capsuleTest.GetEnd()), 0.15f, Color_red, true, 1);
							}
#endif

							capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
							capsuleTest.SetIsDirected(false);
							capsuleTest.SetContext(WorldProbe::LOS_GeneralAI);
							const CEntity* ppExcludeEnts[1] = { &ped };
							capsuleTest.SetExcludeEntities(ppExcludeEnts, 1);
							capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_MAP_TYPE_MOVER);
							capsuleTest.SetOptions(WorldProbe::LOS_IGNORE_NO_COLLISION);

							bIsLOS = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
						}
					}
					else
					{
						bIsLOS = true;
					}
				}
				else if (ladder_end == 1 && IsGreaterThanOrEqualAll(diffToTop, diffToBottom))	// We are mounting on bottom
				{
					ScalarV XYDistToLadder = MagXYSquared(dirToLadder);

					static dev_float sMaxDist = 12.25f;
					static dev_float sMinDist = 0.3f;

					if (IsLessThanAll(XYDistToLadder, ScalarVFromF32(sMaxDist)) && IsGreaterThanAll(XYDistToLadder, ScalarVFromF32(sMinDist)))
					{
						Vec3V XYDirToLadder = And(Vec3V(V_MASKXY), dirToLadder);

						WorldProbe::CShapeTestProbeDesc probeDesc;
						probeDesc.SetStartAndEnd(RCC_VECTOR3(pedPosition), VEC3V_TO_VECTOR3(pedPosition + XYDirToLadder * Vec3VFromF32(0.8f))); // Cut of last 20% to "exclude" the ladder
						
#if __BANK
						if(CClimbLadderDebug::ms_bRenderDebug)
						{
							grcDebugDraw::Line(pedPosition, pedPosition + XYDirToLadder * Vec3VFromF32(0.8f), Color_blue);
						}
#endif
						
						const CEntity* ppExcludeEnts[2] = { &ped, pEntity };
						probeDesc.SetExcludeEntities(ppExcludeEnts, 2);
						probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
						probeDesc.SetOptions(WorldProbe::LOS_IGNORE_NO_COLLISION);

						bIsLOS = !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
					}
					else
					{
						bIsLOS = true;
					}
				}
							
				if(bIsLOS)
				{
					getOnPosition = posToConsider; 
					closestDistance = offsetDist;
					ladderIndex = i;
					bFoundLadder = true; 
				}
			}
			else
			{
				// just find the closest ladder to the ped's position for network clones
				ScalarV offsetDist = XYDist + Abs(ZOffset);

				if(IsLessThanAll(offsetDist, closestDistance))
				{
					getOnPosition = posToConsider; 
					closestDistance =  offsetDist;
					ladderIndex = i;
					bFoundLadder = true; 
				}
			}
		}
	}

	return bFoundLadder;
}

bool CTaskGoToAndClimbLadder::IsLadderTopBlockedMP(CEntity* pEntity, const CPed& ped, s32 ladderIndex)
{
	if (!pEntity)
	{
		return false;
	}

	Vec3V pedPosition = ped.GetTransform().GetPosition();

	// Some hack to fix B* 1692253
	{
		const Vec3V posToIgnore(-120.8f, -976.0f, 175.6f);
		const Vec3V diff = pedPosition - posToIgnore;
		const ScalarV dist2 = MagSquared(diff);
		const ScalarV maxDist2(25.f);
		if(dist2.Getf() < maxDist2.Getf()) //fixed this because previous check was returning true incorrectly - this is more explicit and accurate and works. still a hack but a hack that is tested now. (lavalley)
			return false;
	}


	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	taskFatalAssertf(pModelInfo, "Invalid model info for ladder");
	taskAssertf(pModelInfo->GetIsLadder(), "%s:Is not a ladder.", pModelInfo->GetModelName());
	taskAssertf(pModelInfo->GetNum2dEffectsNoLights(), "%s:Ladder doesn't have any 2d Effects defined.", pModelInfo->GetModelName());


	C2dEffect* pEffect = pModelInfo->Get2dEffect(ladderIndex);
	if(!pEffect || pEffect->GetType() != ET_LADDER)	
	{
		return false;
	}

	CLadderInfo* pLadderInfo = pEffect->GetLadder();
	if(pLadderInfo)
	{
		Mat34V mat = pEntity->GetMatrix();

		// Get top and bottom of ladder in local space
		Vec3V posAtTop = Transform(mat, Vec3V(Vec::V4LoadUnaligned(&pLadderInfo->vTop.x)));
		Vec3V posAtBottom = Transform(mat, Vec3V(Vec::V4LoadUnaligned(&pLadderInfo->vBottom.x)));

		// Work out whether we're closer to the top or bottom of the ladder
		ScalarV positionZ = pedPosition.GetZ() - ScalarV(V_ONE);
		ScalarV diffToTop = Abs(positionZ - posAtTop.GetZ());
		ScalarV diffToBottom = Abs(positionZ - posAtBottom.GetZ());

		if (IsLessThanAll(diffToTop, diffToBottom)) // We are mounting on top
		{
			Vec3V ladderNormal = Transform3x3(mat, Vec3V(Vec::V4LoadUnaligned(&pLadderInfo->vNormal.x)));
			Vec3V posAtTopOffset = posAtTop + ladderNormal * ScalarVFromF32(0.5f);

			WorldProbe::CShapeTestCapsuleDesc capsuleTest;
			static const float scfDistanceTopToBottom = 2.5f; //provide a const value here that is used to show the distance top to bottom for the capsule, increase from 1.5 to 2.0. (lavalley)
			static const float scfCapsuleRadius = 0.225f;
			capsuleTest.SetCapsule(VEC3V_TO_VECTOR3(posAtTopOffset + Vec3V(V_Z_AXIS_WZERO)), VEC3V_TO_VECTOR3(posAtTopOffset - Vec3V(V_Z_AXIS_WZERO) * Vec3VFromF32(scfDistanceTopToBottom)), scfCapsuleRadius);
			//grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(capsuleTest.GetStart()), VECTOR3_TO_VEC3V(capsuleTest.GetEnd()), 0.225f, Color_yellow, false, 500);

			capsuleTest.SetTypeFlags(TYPE_FLAGS_ALL);
			capsuleTest.SetIsDirected(false);
			capsuleTest.SetContext(WorldProbe::ENetwork);
			const CEntity* ppExcludeEnts[2] = { &ped, pEntity };
			capsuleTest.SetExcludeEntities(ppExcludeEnts, 2);
			capsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_PED_TYPE);

			return WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		}
	}

	return false;
}

void CTaskGoToAndClimbLadder::CalcShouldGetOnForwardsOrBackwards(CPed * pPed)
{
	// Vector from the ped to the ladder
	Vector3 vDiff = m_vPositionOfTop - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vDiff.z = 0.0f; //position of top and bottom are the same if you ignore z
	vDiff.Normalize();

	// Do a test in the XY plane to see if we're facing the ladder
	float fDot = DotProduct(VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()), vDiff);

	// If I'm greater than 113o from the ladder and getting on at the top, we're getting on backwards
	if(fDot <- 0.4f && m_eClimbMode == EGetOnAtTop)
	{
		taskAssert(m_eClimbMode==EGetOnAtTop);
		m_bGetOnBackwards = true;
	}
	else
	{
		taskAssert(m_eClimbMode==EGetOnAtBottom || m_eClimbMode==EGetOnAtTop);
		m_bGetOnBackwards=false;
	}
}

void CTaskGoToAndClimbLadder::EnsurePedIsInPositionForLadder(CPed * pPed)
{
	m_bTopOfLadderIsTooHigh = false;
	const float fPedOriginalZ = pPed->GetTransform().GetPosition().GetZf();
	const float fZDiff = m_vPositionToGetOn.z - (fPedOriginalZ - 1.0f);
	if(m_eClimbMode==EGetOnAtTop && fZDiff > 0.5f)
	{
		// It looks like this is another fucked ladder.  Ladders need to have their top position flush with
		// the floor level, but many have been placed incorrectly & not tested.  Here is a messy workaround
		// which disables gravity for this ped & sets their position flush with the 3dfx, so that the
		// get-on clip can then play to get them on at the correct height for this ladder..  Gravity is
		// disabled during the get-on clip in this case (otherwise it remains on as usual).
		m_bTopOfLadderIsTooHigh = true;
		//Assertf(false, "ART: The top of the ladder at (%.1f,%.1f,%.1f) is too high above floor level.\nThe top of a ladder needs to be flush with the floor level where the ped is to get on.", m_vPositionToGetOn.x, m_vPositionToGetOn.y, m_vPositionToGetOn.z);
	}

	Vector3 vPos = (m_eClimbMode==EGetOnAtTop) ? m_vSlideTargetAtTop : m_vPositionToGetOn;

	if(!m_bTopOfLadderIsTooHigh)
	{
		vPos.z = fPedOriginalZ;
	}
	else
	{
		vPos.z = m_vPositionToGetOn.z + 1.0f;
		pPed->SetUseExtractedZ(true);
	}

	//pPed->SetPosition(vPos, true, true, true);
}

bool CTaskGoToAndClimbLadder::CalcLadderInfo(CEntity &pLadder, s32 effectIndex,Vector3 &vBasePos,Vector3 &vTopPos,Vector3 &vNormal,bool &bCanGetOffAtTopOfThisLadderSection)
{
	CBaseModelInfo* pBaseModel = pLadder.GetBaseModelInfo();
	taskAssertf(pBaseModel, "CalcLadderInfo: ladder has no base modecalcladderinfol info was expecing one"); 
	if(pBaseModel && pBaseModel->GetIsLadder())
	{
		taskAssertf(effectIndex < pBaseModel->GetNum2dEffectsNoLights(), "CalcLadderInfo: The effect index passed in is out of range for %s model", pBaseModel->GetModelName());

		C2dEffect* pEffect = pBaseModel->Get2dEffect(effectIndex);
		if(!pEffect || pEffect->GetType() != ET_LADDER)	
		{
			return false;
		}

		CLadderInfo* pLadderInfo = pEffect->GetLadder();

		if(pLadderInfo)
		{
			// Get positions in local space
			pLadderInfo->vTop.Get(vTopPos);
			pLadderInfo->vBottom.Get(vBasePos);
			pLadderInfo->vNormal.Get(vNormal);

			TUNE_GROUP_BOOL(LADDER_DEBUG, LADDER_USE_OVERRIDEN_NORM, false);
			if (LADDER_USE_OVERRIDEN_NORM)
			{
				static Vector3 vOverridenNormal(0.0f, -1.0f, 0.0f);
				vNormal = vOverridenNormal;
			}

			// Transform to world space
			const Matrix34  mat = MAT34V_TO_MATRIX34(pLadder.GetMatrix());
			mat.Transform(vTopPos);
			mat.Transform(vBasePos);
			mat.Transform3x3(vNormal);

			bCanGetOffAtTopOfThisLadderSection = pLadderInfo->bCanGetOffAtTop;

			return true;
		}
	}

	return false;
}

void CTaskGoToAndClimbLadder::AddLadder(CEntity* pEntity)
{
	if (ms_Ladders.GetCapacity() < 256)
		ms_Ladders.Reserve(256);

	ms_Ladders.PushAndGrow(RegdEnt(pEntity)); 
	Assertf(ms_Ladders[ms_Ladders.GetCount() - 1], " Added invalid ladder, this is fatal!");
	Assertf(ms_Ladders.GetCount()<256, "Have more than x ladders attached to buildings consider looking at a better way to tag building ladders"); 
}

void CTaskGoToAndClimbLadder::RemoveLadder(CEntity* pEntity)
{
	for(int i=0; i < ms_Ladders.GetCount(); i++)
	{
		if(ms_Ladders[i] == pEntity)
		{
			ms_Ladders.DeleteFast(i);
			break; 
		}
	}
}

/* static */ u32 CTaskGoToAndClimbLadder::GenerateHash(Vector3 const& pos, const u32 modelnameHash)
{
	u32 components[4];

	// using dm or cm accuracy will create different hashes on remotes from the local - just use the meter based xyz truncated as the ladder hash basis here.
	// also the xyz isn't unique - need additional variable to make the hash unique otherwise we end up with hash mismatched on remote == bad
	//
	// so use the base (m) xyz and the hash from the model name to come up with a unique hash that works across the network

	components[0] = u32(pos.x);
	components[1] = u32(pos.y);
	components[2] = u32(pos.z);
	components[3] = modelnameHash;

	return CLODLightManager::hash(	components, 4, 0 );
}

CEntity* CTaskGoToAndClimbLadder::FindLadderUsingPosHash(const CPed & ped, u32 const hash)
{
	Vector3 pedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()); 
	CEntity* pLadder = NULL;
	float distFromPed = 0.0f;

	if (ped.IsNetworkClone())
	{
		pedPos = NetworkInterface::GetLastPosReceivedOverNetwork(&ped);
	}

	if(0 != hash)
	{
		CEntityScannerIterator entityList = ped.GetPedIntelligence()->GetNearbyObjects();
		
		for(CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
		{
			CBaseModelInfo * pModelInf = pEnt->GetBaseModelInfo();
			taskAssertf(pModelInf, "Wtf someone removed the CBaseModelInfo!?");

			if(pModelInf && pModelInf->GetIsLadder())
			{
				Vector3 ladderPos = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition());
				u32 generatedHash = GenerateHash(ladderPos,pModelInf->GetModelNameHash());
				if(generatedHash == hash)
				{
					Vector3 diff = ladderPos - pedPos;
					float dist = diff.Mag2();

					if (!pLadder)
					{
						pLadder = pEnt;
						distFromPed = dist;
					}
					else
					{
						taskWarningf("Multiple ladders found with same pos hash %d. Choosing closest one to ped %s", hash, ped.GetNetworkObject() ? ped.GetNetworkObject()->GetLogName() : "");
						
						if (dist < distFromPed)
						{
							pLadder = pEnt;
							distFromPed = dist;
						}
					}
				}
			}
		}

		if (pLadder)
		{
			return pLadder;
		}

		int nLadders = ms_Ladders.GetCount();
		for(int i =0; i < nLadders; i++)
		{
			if (taskVerifyf(ms_Ladders[i], "Wtf someone removed my ladder!?") &&
				taskVerifyf(ms_Ladders[i]->GetBaseModelInfo(), "Wtf someone removed my ladder base model info!?"))
			{
				Vector3 ladderPos = VEC3V_TO_VECTOR3(ms_Ladders[i]->GetTransform().GetPosition());
				u32 generatedHash = GenerateHash(ladderPos,ms_Ladders[i]->GetBaseModelInfo()->GetModelNameHash());
				if(generatedHash == hash)
				{
					Vector3 diff = ladderPos - pedPos;
					float dist = diff.Mag2();

					if (!pLadder)
					{
						pLadder = ms_Ladders[i];
						distFromPed = dist;
					}
					else
					{
						taskWarningf("Multiple ladders found with same pos hash %d. Choosing closest one to ped %s", hash, ped.GetNetworkObject() ? ped.GetNetworkObject()->GetLogName() : "");

						if (dist < distFromPed)
						{
							pLadder = ms_Ladders[i];
							distFromPed = dist;
						}
					}
				}
			}
		}
	}

	return pLadder;
}

CEntity* CTaskGoToAndClimbLadder::ScanForLadderToClimb(const CPed & ped,s32 &ladderIndex, Vector3& vGetOnPostion, bool ignoreMinXYDistLimit /*= false*/)
{
	ScalarV closestLadderDist(V_FLT_MAX);
	
	CEntity* pClosestEntity = NULL; 

	CEntityScannerIterator entityList = ped.GetPedIntelligence()->GetNearbyObjects();

#if __BANK
	TUNE_GROUP_BOOL( LADDER_DEBUG, bLogLadderScanner, false );

	if(bLogLadderScanner)
	{
		Displayf( "START OF - CTaskGoToAndClimbLadder::ScanForLadderToClimb" );
		Displayf( "ped.GetPedIntelligence()->GetNearbyObjects() found %i entities", entityList.GetCount() );
	}
#endif
	
	//Search for ladder objects that are around the ped.
	for(CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		CBaseModelInfo * pModelInf = pEnt->GetBaseModelInfo();
		Assertf(pModelInf, "Wtf someone removed the CBaseModelInfo!?");

		if(pModelInf->GetIsLadder())
		{
			if(IsClosestLadder(pEnt, ped, ladderIndex, RC_VEC3V(vGetOnPostion), closestLadderDist, ignoreMinXYDistLimit))
			{
				pClosestEntity = pEnt; 
				Assertf(!(pClosestEntity->GetIplIndex() == 0xdddd && pClosestEntity->GetAudioBin() == 0xdd), "Ladder is deleted in ScanForLadderToClimb() - entityList!");
			}
		}
	}

	//Search for ladders that are attached directly to buildings, these will not be detected in the object search.
	//This list is generated when a building is added to the world.
	int nLadders = ms_Ladders.GetCount();
#if __BANK
	if(bLogLadderScanner)
	{
		Displayf( "ms_Ladders.GetCount() found %i ladders attached to static buildings", ms_Ladders.GetCount() );
	}
#endif
	for(int i =0; i < nLadders; i++)
	{
		Assertf(ms_Ladders[i], "Wtf someone removed my ladder!?");
		if (IsClosestLadder(ms_Ladders[i], ped, ladderIndex, RC_VEC3V(vGetOnPostion), closestLadderDist, ignoreMinXYDistLimit))
		{
			pClosestEntity = ms_Ladders[i]; 
			Assertf(!(pClosestEntity->GetIplIndex() == 0xdddd && pClosestEntity->GetAudioBin() == 0xdd), "Ladder is deleted in ScanForLadderToClimb() - ms_Ladders!");
		}
	}
	
#if __BANK
	if(bLogLadderScanner)
	{
		if(pClosestEntity != NULL)
		{
			Vector3 vLadderPos = VEC3V_TO_VECTOR3( pClosestEntity->GetMatrix().d() );
			Displayf( "Result: The closest ladder that was found (%p), has the following position: (%f, %f, %f)", pClosestEntity, vLadderPos.x, vLadderPos.y, vLadderPos.z );
		}
		else
		{
			Displayf( "Result: No ladders were found or no ladders were close enough" );
		}
		Displayf("END OF - CTaskGoToAndClimbLadder::ScanForLadderToClimb");
	}
#endif

	return pClosestEntity; 
}


bool CTaskGoToAndClimbLadder::FindTopAndBottomOfAllLadderSections(CEntity * pLadder,s32 ladderIndex, Vector3 & vPositionOfTop, Vector3 & vPositionOfBottom, Vector3 & vNormal, bool & bCanGetOffAtTopOfLadder)
{
	if(!pLadder)
	{
		return false; 
	}

	CBaseModelInfo* pBaseModel = pLadder->GetBaseModelInfo();
	
	if(!pBaseModel)
	{
		return false; 
	}
	
	taskAssert(pBaseModel->GetIsLadder());
	bool oneLadderIsFine;
	bool bCalcLadderInfoOk=false;

	if (ladderIndex>-1)
	{
		bCalcLadderInfoOk = CalcLadderInfo(*pLadder, ladderIndex, vPositionOfBottom, vPositionOfTop, vNormal, bCanGetOffAtTopOfLadder ); //top and bottom of one 2d_effect
		oneLadderIsFine=true;
	}
	else
	{
		oneLadderIsFine = false;

		GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
		//just use the first ladder section thats valid
		int e;
		for(e=0; e<iNumEffects; e++)
		{
			C2dEffect* pEffect = (*pa2dEffects)[e];
			if(pEffect->GetType() == ET_LADDER)
			{
				ladderIndex = e;
				bCalcLadderInfoOk = CalcLadderInfo(*pLadder, ladderIndex, vPositionOfBottom, vPositionOfTop, vNormal, bCanGetOffAtTopOfLadder ); //top and bottom of one 2d_effect
				oneLadderIsFine=true;
				break;
			}
		}
	}

	if(!bCalcLadderInfoOk)
	{
		return false;
	}

	if(FindTopAndBottomOfSingleLadderSectionFrom2dFx(pLadder, ladderIndex, vPositionOfTop, vPositionOfBottom, vNormal, bCanGetOffAtTopOfLadder))
	{
		oneLadderIsFine=true;
	}

	return oneLadderIsFine;
}

bool CTaskGoToAndClimbLadder::FindTopAndBottomOfSingleLadderSectionFrom2dFx(CEntity * pLadder,s32 ladderIndex, Vector3 & vOutTop, Vector3 & vOutBottom,Vector3 &vOutNormal, bool & bCanGetOffAtTopOfThisLadderSection)
{
	if(!pLadder)
	{
		return false; 
	}
	
	bool bFoundTopAndBottom = false;
	// Go through the effects one by one
	CBaseModelInfo* pBaseModel = pLadder->GetBaseModelInfo();
	Assert(pBaseModel->GetIsLadder());

	if(!pBaseModel)
	{
		return false; 
	}

	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for(s32 c=0; c<iNumEffects; c++)
	{
		if(c==ladderIndex)
			continue;
		C2dEffect* pEffect = (*pa2dEffects)[c];
		
		if(pEffect && pEffect->GetType() == ET_LADDER)
		{
			Vector3 vTop, vBottom,vNormal;
			bool	blocalCanGetOffAtTopOfThisLadderSection=false;
			CalcLadderInfo(*pLadder,c,vBottom,vTop,vNormal,blocalCanGetOffAtTopOfThisLadderSection);

			// Check that the artwork has been marked-up properly.
			// Handle it by swapping the top & bottom positions
			Assertf(vTop.z >= vBottom.z, "It looks like this ladder has its Ladder 2dFx set up the wrong way round.");

			if(ladderIndex==-1||( fabsf(vTop.x-vOutTop.x)<0.3f && fabsf(vTop.y-vOutTop.y)<0.3f && DotProduct(vNormal,vOutNormal)>0.9f)) //check ladder part is close by and normals are similar
			{
				// Don't accept breaks between the ladders
				if (fabsf(vTop.z - vOutBottom.z) < 0.5f || fabsf(vBottom.z - vOutTop.z) < 0.5f)
				{
					if(vTop.z > vOutTop.z)
					{
						vOutTop = vTop;
						bCanGetOffAtTopOfThisLadderSection = blocalCanGetOffAtTopOfThisLadderSection;

					}
					if(vBottom.z < vOutBottom.z)
					{
						vOutBottom = vBottom;
					}
					bFoundTopAndBottom = true;


					if(ladderIndex==-1)
					{
						ladderIndex=c;
						vOutNormal=vNormal;
					}
				}
			}
		}
	}
	return bFoundTopAndBottom;
}

bool CTaskGoToAndClimbLadder::GetNormalOfLadderFrom2dFx(CEntity * pLadder, Vector3 & vOutNormal, s32 EffectIndex )
{
	// Go through the effects one by one
	if(pLadder)
	{
		CBaseModelInfo* pBaseModel = pLadder->GetBaseModelInfo();
		if(pBaseModel)
		{
			C2dEffect* pEffect = pBaseModel->Get2dEffect(EffectIndex);

			if(pEffect && pEffect->GetType() == ET_LADDER)
			{
				CLadderInfo* ldr = pEffect->GetLadder();
				if(ldr)
				{
					Vector3 vNormal;
					ldr->vNormal.Get(vNormal);
					const Matrix34 mat = MAT34V_TO_MATRIX34(pLadder->GetMatrix());
					mat.Transform3x3(vNormal);
					vOutNormal = vNormal;
					return true;
				}
			}
		}
	}
	return false;
}

bool CTaskGoToAndClimbLadder::IsGettingOnBottom(CEntity * pLadder, const CPed& ped, s32 EffectIndex, Vector3* pvBottomPos)
{
	if(!pLadder)
	{
		return false; 
	}
	
	CBaseModelInfo* pBaseModel = pLadder->GetBaseModelInfo();

	const s32 iNumEffects = pBaseModel->GetNum2dEffectsNoLights();
	
	if(!pBaseModel)
	{
		return false; 
	}

	taskAssertf(iNumEffects, "The model has no 2d effects may result in strange get on behaviour"); 

	if (iNumEffects > 0)
	{
		C2dEffect* pEffect = pBaseModel->Get2dEffect(EffectIndex);
		
		if(pEffect && pEffect->GetType() == ET_LADDER)
		{
			CLadderInfo* pLadderInfo = pEffect->GetLadder();
			
			if(pLadderInfo)
			{
				// Get top and bottom of ladder in local space
				Vec3V vPosAtTop, vPosAtBottom;
				pLadderInfo->vTop.Get(RC_VECTOR3(vPosAtTop));
				pLadderInfo->vBottom.Get(RC_VECTOR3(vPosAtBottom));

				const Mat34V mat = pLadder->GetMatrix();
				vPosAtTop = Transform(mat, vPosAtTop);
				vPosAtBottom = Transform(mat, vPosAtBottom);

				if (pvBottomPos)
					*pvBottomPos = RC_VECTOR3(vPosAtBottom);

				
				if (ped.GetIsSwimming())
					return true;

				Vec3V vDistTop = vPosAtTop - ped.GetTransform().GetPosition();
				Vec3V vDistBot = vPosAtBottom - ped.GetTransform().GetPosition();


				if(NetworkInterface::IsGameInProgress())
				{
					//url:bugstar:6554278 Check if this is a yatch ladder we are trying to climb on. This will allow us to climb on from half way up the ladder
					if( pBaseModel->GetModelNameHash() == ATSTRINGHASH("apa_mp_apa_yacht_win", 0xBCDAC9E7))
					{
						if(ped.GetTransform().GetPosition().GetZ().Getf() < vPosAtTop.GetZ().Getf())
						{
							return true;
						}
					}
				}

				if(VEC3V_TO_VECTOR3(vDistTop).Mag2() < VEC3V_TO_VECTOR3(vDistBot).Mag2())
				{
					return false; 
				}
			}
			else
			{
				return false; 
			}
		}
		else
		{
			return false;
		}
	}
	return true; 
}


bool CTaskGoToAndClimbLadder::IsPedOrientatedCorrectlyToLadderToGetOn(CEntity* pLadder, const CPed& ped, s32 EffectIndex, bool bIdealMountWalkDist, bool& bIsEasyEntry)
{
	if (pLadder)
	{
		Vector3 vLadderNormal, vTop, vBottom;		
		if (ped.IsLocalPlayer())
		{
			bool bCanGetOffAtTopOfThisLadderSection;
			if (CalcLadderInfo(*pLadder, EffectIndex, vBottom, vTop, vLadderNormal, bCanGetOffAtTopOfThisLadderSection))
			{	
				float fPedDesiredHeading = ped.GetDesiredHeading();

#if FPS_MODE_SUPPORTED
				bool bFPSModeEnabled = ped.IsFirstPersonShooterModeEnabledForPlayer(false);
				if(bFPSModeEnabled)
				{
					const CControl *pControl = ped.GetControlFromPlayer();
					if(pControl)
					{
						Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);

						float fMag = vStickInput.Mag();
						if(fMag > 0.01f)
						{
							float fCamOrient = camInterface::GetPlayerControlCamHeading(ped);
							float fStickAngle = rage::Atan2f(-vStickInput.x, vStickInput.y);	
							fStickAngle = fStickAngle + fCamOrient;
							fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
							fPedDesiredHeading = fStickAngle;
						}
					}
				}
#endif

				// Don't allow to climb ladders completely submerged in water
				float fWaterZ = 0.0f;
				if (CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vTop, &fWaterZ))
				{
					if (fWaterZ >= vTop.GetZ())
						return false;
				}
			
				Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
				Vector3 vToLadder =	vBottom - vPedPos;
				float fXYDist2ToLadder = vToLadder.XYMag2();
				vToLadder.z = 0.0f; 
				vToLadder.NormalizeSafe(); 

				if (IsGettingOnBottom(pLadder, ped, EffectIndex))
				{
					const float fNormalToladderDot = vLadderNormal.Dot(vToLadder);
					if (fNormalToladderDot > 0.f)	// Don't mount from behind unless we are close!
					{
						if (fXYDist2ToLadder > 1.f)
						{
							return false;
						}

						if (!CTaskClimbLadder::LoadMetaDataCanMountBehind(pLadder, EffectIndex))
						{
							return false;
						}
					}

					TUNE_GROUP_FLOAT(LADDER_DEBUG, DesiredAngleDeltaDif, 2.0f, 0.0f, 3.14f, 0.1f);
		//			TUNE_GROUP_FLOAT(LADDER_DEBUG, DesiredToLadderDot, 0.866f, 0.0f, 1.0f, 0.1f);
					TUNE_GROUP_FLOAT(LADDER_DEBUG, VelocityToLadderDot, 0.2f, 0.0f, 1.0f, 0.1f);
					TUNE_GROUP_FLOAT(LADDER_DEBUG, VelocityPlaneSizeThreshold, 0.0f, 0.0f, 2.0f, 0.1f);
					TUNE_GROUP_FLOAT(LADDER_DEBUG, LadderPlaneHalfWidth, 0.775f, 0.0f, 1.0f, 0.1f);
					TUNE_GROUP_FLOAT(LADDER_DEBUG, LadderPlaneHalfWidthSmall, 0.575f, 0.0f, 1.0f, 0.1f);

					const float CurrentDesiredDelta = fwAngle::LimitRadianAngle(fPedDesiredHeading - ped.GetCurrentHeading());
					if (abs(CurrentDesiredDelta) < (DesiredAngleDeltaDif))
					{
						Vector3 vDesiredHeading = Vector3(-sin(fPedDesiredHeading), cos(fPedDesiredHeading), 0.f);
					//	float fDesiredToLadderDot = DotProduct(vToLadder, vDesiredHeading);

						const bool bIngorePlaneCalc = bIdealMountWalkDist && (fNormalToladderDot < 0.15f && fNormalToladderDot > -0.25f);

						// See where on the ladder plane we hit
						const float fPlaneDirProj = (vBottom - vPedPos).Dot(vLadderNormal);
						float fD = fPlaneDirProj / vDesiredHeading.Dot(vLadderNormal);
						if (fD <= 0.f && !bIngorePlaneCalc)	// Behind plane
							return false;

						const Vector3 vIntersectPointDesired = fD * vDesiredHeading + vPedPos;

						Vector3 vPedVel = ped.GetDesiredVelocity();
						vPedVel.z = 0.f;
						if (vPedVel.Mag2() < 0.001f)
							return false;

						vPedVel.Normalize();

						// See where on the ladder plane we hit
						fD = fPlaneDirProj / vPedVel.Dot(vLadderNormal);
						if (fD <= 0.f && !bIngorePlaneCalc)	// Behind plane
							return false;

						const Vector3 vIntersectPointVelocity = fD * vPedVel + vPedPos;

						float fLadderPlaneHalfWidth = (ped.GetVelocity().XYMag2() > rage::square(VelocityPlaneSizeThreshold) ? LadderPlaneHalfWidth : LadderPlaneHalfWidthSmall);
						
						//// Debug shiete
						//{
						//	Vector3 vLadderSide = vIntersectPointDesired - vBottom;
						//	vLadderSide.z = 0.f;
						//	vLadderSide.Normalize();

						//	grcDebugDraw::Sphere(vIntersectPointDesired, 0.15f, Color_red, true, 60);
						//	grcDebugDraw::Sphere(vIntersectPointVelocity, 0.15f, Color_blue, true, 60);
						//	grcDebugDraw::Line(vLadderSide * fLadderPlaneHalfWidth + vBottom, vLadderSide * fLadderPlaneHalfWidth + vTop, Color_yellow, 60);
						//	grcDebugDraw::Line(-vLadderSide * fLadderPlaneHalfWidth + vBottom, -vLadderSide * fLadderPlaneHalfWidth + vTop, Color_yellow, 60);
						//}

						const float fVelToLadderDot = DotProduct(vToLadder, vPedVel);
						if (((vIntersectPointVelocity - vBottom).XYMag2() < rage::square(fLadderPlaneHalfWidth) ||	// We either hit the plane
							(vIntersectPointDesired - vBottom).XYMag2() < rage::square(fLadderPlaneHalfWidth) ||
							bIngorePlaneCalc) &&																	// Or we are mounting from the very sides
							fVelToLadderDot > VelocityToLadderDot)													// Must move towards the ladder somewhat too
						{
							return true;
						}
					}
				}
				else
				{
					// We are too far above the ladder
					if (vPedPos.z - 1.5f > vTop.z)
						return false;

					// 
					const float fNormalToladderDot = vLadderNormal.Dot(vToLadder);
					if (fNormalToladderDot < -0.15f)	// Don't mount from behind
					{
						return false;
					}

					static dev_float HEADING_DOT = 0.4f;
					static dev_float HEADING_SLACK_DOT = 0.2f;
					static dev_float HEADING_SLACK_CLOSE_DOT = 0.8f;

					// We need to cache the easy entry value to ensure the conditions match when starting the ladder task
					const bool bUseEasyLadderConditions = bIsEasyEntry || (!ped.GetPedResetFlag(CPED_RESET_FLAG_DisableEasyLadderConditions) && NetworkInterface::IsGameInProgress() && ped.IsLocalPlayer() && ms_bUseEasyLadderConditionsInMP && (ped.IsUsingActionMode() || ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun)));
					if (bUseEasyLadderConditions)
					{
						bIsEasyEntry = true;
					}
					float fHeadingDotTolerance = bUseEasyLadderConditions ? HEADING_SLACK_DOT : HEADING_DOT;

					TUNE_GROUP_BOOL(LADDER_DEBUG, USE_STRICTER_CONDS_WHEN_CLOSE, true);
					TUNE_GROUP_FLOAT(LADDER_DEBUG, CloseDist, 0.5f, 0.0f, 1.0f, 0.01f);
					if (USE_STRICTER_CONDS_WHEN_CLOSE && bUseEasyLadderConditions && fXYDist2ToLadder < square(CloseDist))
					{
						fHeadingDotTolerance = HEADING_SLACK_CLOSE_DOT;
					}

					// We must kinda move towards the ladder somewhat
					Vector3 pedVel = ped.GetVelocity();
					pedVel.z = 0.f;
					pedVel.Normalize();

					if (!bIdealMountWalkDist && ped.IsPlayer() && ped.GetMotionData())
					{	
						if (!ped.GetMotionData()->GetIsLessThanRun())
						{
							static dev_float HEADING_DOT_RUN = 0.95f;	//
							float fVelToLadderDot = DotProduct(vToLadder, pedVel);
							if (fVelToLadderDot < HEADING_DOT_RUN)
							{
								ASSERT_ONLY(aiDebugf2("Frame : %i, Ped %s FAILING IsPedOrientatedCorrectlyToLadderToGetOn condition RUN fVelToLadderDot = %.2f", fwTimer::GetFrameCount(), ped.GetDebugName(), fVelToLadderDot);)
								return false;
							}
						}
					}

					float fVelToLadderDot = DotProduct(vToLadder, pedVel);
					if (!bUseEasyLadderConditions && fVelToLadderDot < fHeadingDotTolerance)
					{
						ASSERT_ONLY(aiDebugf2("Frame : %i, Ped %s FAILING IsPedOrientatedCorrectlyToLadderToGetOn condition fVelToLadderDot = %.2f", fwTimer::GetFrameCount(), ped.GetDebugName(), fVelToLadderDot);)
						return false;
					}

					//check the desired heading first
					float fDesiredHeading = fPedDesiredHeading;
					aiDisplayf("fDesiredHeading = %.2f", fDesiredHeading);
					if (bUseEasyLadderConditions)
					{
						TUNE_GROUP_BOOL(LADDER_DEBUG, USE_STICK_INPUT, true);
						TUNE_GROUP_BOOL(LADDER_DEBUG, USE_VELOCITY, true);
						Vector2 vStickInput;
						const CTaskPlayerOnFoot* pPlayerTask = static_cast<CTaskPlayerOnFoot*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
						if (USE_STICK_INPUT && pPlayerTask && pPlayerTask->GetLastStickInputIfValid(vStickInput))
						{
							const float fCamHeading = camInterface::GetPlayerControlCamHeading(ped);
							aiDebugf2("fCamHeading = %.2f", fCamHeading);
							const float fStickHeading = rage::Atan2f(-vStickInput.x, vStickInput.y);
							aiDebugf2("fStickHeading = %.2f", fStickHeading);
							const float fHeadingDiff = fwAngle::LimitRadianAngle(fCamHeading - fStickHeading);
							aiDebugf2("fHeadingDiff = %.2f", fStickHeading);
							fDesiredHeading = fwAngle::LimitRadianAngle(fCamHeading - fHeadingDiff);
							aiDebugf2("USING STICK AS DESIRED fDesiredHeading = %.2f", fDesiredHeading);
						}
						else if (USE_VELOCITY)
						{
							// When strafing the desired heading gets reset to the current, so we need to use the
							// peds velocity (or possible cached desired heading if available)
							fDesiredHeading = rage::Atan2f(-pedVel.x, pedVel.y);
							aiDebugf2("USING VEL AS DESIRED fDesiredHeading = %.2f", fDesiredHeading);
						}
					}

					Matrix34 mat = MAT34V_TO_MATRIX34 (ped.GetTransform().GetMatrix()); 
					mat.RotateLocalZ(fDesiredHeading - ped.GetCurrentHeading());  
					float fLdrDotHdg = DotProduct(vLadderNormal,mat.b);
#if __ASSERT
					const float fLadderHeading = rage::Atan2f(-vLadderNormal.x, vLadderNormal.y);
					aiDebugf2("LADDER HEADING = %.2f", fLadderHeading);
					aiDebugf2("fLdrDotHdg = %.2f", fLdrDotHdg);
#endif // __ASSERT
					if(fLdrDotHdg > fHeadingDotTolerance)
					{
						return true;
					}

					const float fLdrNormalDotPedForward = DotProduct(vLadderNormal,VEC3V_TO_VECTOR3(ped.GetTransform().GetForward()));
					aiDebugf2("fLdrNormalDotPedForward = %.2f", fLdrNormalDotPedForward);
					if(fLdrNormalDotPedForward > fHeadingDotTolerance)
					{
						return true;
					}

					if (bUseEasyLadderConditions)
					{
						Vector3 vForwardComp(0.0f,1.0f,0.0f);
						vForwardComp.RotateZ(fDesiredHeading);
						aiDebugf2("fLdrNormalDotPedForward2 = %.2f", fLdrNormalDotPedForward);
						if(fLdrNormalDotPedForward > fHeadingDotTolerance)
						{
							return true;
						}
					}
				}
			}
		}
	}

	ASSERT_ONLY(aiDisplayf("Frame : %i, Ped %s FAILING IsPedOrientatedCorrectlyToLadderToGetOn condition default", fwTimer::GetFrameCount(), ped.GetDebugName());)
	return false; 
}

bool CTaskGoToAndClimbLadder::CalculatePositionsForThisLadder(const CPed* pPed)
{
	taskAssert(m_eClimbMode == EGetOnAtBottom || m_eClimbMode == EGetOnAtTop);
	
	if (!m_pLadder)
		return false;

	// Go through the effects one by one
	CBaseModelInfo* pBaseModel = m_pLadder->GetBaseModelInfo();

	s32 c = m_LadderIndex;
	C2dEffect* pEffect = pBaseModel->Get2dEffect(c);
	if(pEffect->GetType() == ET_LADDER)
	{
		CLadderInfo* ldr = pEffect->GetLadder();

		Vector3 vPosAtTop, vPosAtBottom;
		ldr->vTop.Get(vPosAtTop);
		ldr->vBottom.Get(vPosAtBottom);
		ldr->vNormal.Get(m_vLadderNormal);

		TUNE_GROUP_BOOL(LADDER_DEBUG, LADDER_USE_OVERRIDEN_NORM_2, false);
		if (LADDER_USE_OVERRIDEN_NORM_2)
		{
			static Vector3 vOverridenNormal(0.0f, -1.0f, 0.0f);
			m_vLadderNormal = vOverridenNormal;
		}

		const Matrix34  mat = MAT34V_TO_MATRIX34(m_pLadder->GetMatrix());
		mat.Transform(vPosAtTop);
		mat.Transform(vPosAtBottom);

		// Only set the XY positions.. The z value will already have been set in FindTopAndBottomOfLadder().
		m_vPositionOfTop.x = vPosAtTop.x;
		m_vPositionOfTop.y = vPosAtTop.y;
		m_vPositionOfBottom.x = vPosAtBottom.x;
		m_vPositionOfBottom.y = vPosAtBottom.y;

		// Calculate the ladder normal in world space
		mat.Transform3x3(m_vLadderNormal);
		m_vLadderNormal.z = 0.0f;

		m_fLadderHeading = fwAngle::GetRadianAngleBetweenPoints(0.0f, 0.0f, m_vLadderNormal.x, m_vLadderNormal.y);
		
		// Remember that local players can be under AI control as well
		if( pPed->IsLocalPlayer() && pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame() )
		{
			if (!IsPedOrientatedCorrectlyToLadderToGetOn(m_pLadder, *pPed, m_LadderIndex, true, m_bIsEasyEntry))	// Should check distance to ladder
			{
				return false; 
			}
		}

		TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnAIBotScalar, 0.50f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, GetOnAITopScalar, 0.50f, -1.0f, 1.0f, 0.1f);
		if (m_eClimbMode == EGetOnAtBottom)
		{
			m_vPositionToGetOn = m_vPositionOfBottom + (m_vLadderNormal * GetOnAIBotScalar  );
		}
		else
		{
			m_vPositionToGetOn = m_vPositionOfTop - (m_vLadderNormal * GetOnAITopScalar  );
		}

		return true;
	}

	return false;
}

Vector3 CTaskGoToAndClimbLadder::CalculatePositionToGetOnLadder()
{
	Vector3 vStartingPosition;

	// Manually figure out the offset from the starting position, to where the "get on at bottom"
	// clip has left the ped.
	Vector3 vLadderNormal(-rage::Sinf(m_fLadderHeading),rage::Cosf(m_fLadderHeading),0.0f);

	if (m_eClimbMode== EGetOnAtTop)
	{
		vStartingPosition = m_vPositionOfTop;
		vStartingPosition -= (vLadderNormal * ms_fOffsetFrom2dFxToCapsule);
		vStartingPosition.z -= 1.0f;
		dev_float	VERTICAL_OFFSET_FOR_TOP_RUNG=-0.08f;
		vStartingPosition.z += VERTICAL_OFFSET_FOR_TOP_RUNG;
	}
	else
	{
		vStartingPosition = m_vPositionOfBottom;
		vStartingPosition -= (vLadderNormal * ms_fOffsetFrom2dFxToCapsule);
		vStartingPosition.z += 1.0f;
	}

	//grcDebugDraw::Sphere(vStartingPosition, 0.05f, Color_green, true, 5000);

	return vStartingPosition;
}

bool CTaskGoToAndClimbLadder::IsClimbingUp()
{
	// Maybe just check the climb ladder tasks state?
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CLIMB_LADDER)
	{
		CTaskClimbLadder* pClimbTask = static_cast<CTaskClimbLadder*>(GetSubTask());
		return pClimbTask->IsClimbingUp();
	}

	return false;
}

bool CTaskGoToAndClimbLadder::IsClimbingDown()
{
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CLIMB_LADDER)
	{
		CTaskClimbLadder* pClimbTask = static_cast<CTaskClimbLadder *>(GetSubTask());
		return pClimbTask->IsClimbingDown();
	}

	return false;
}

bool CTaskGoToAndClimbLadder::IsSlidingDown()
{
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CLIMB_LADDER)
	{
		CTaskClimbLadder* pClimbTask = static_cast<CTaskClimbLadder*>(GetSubTask());
		return pClimbTask->IsSlidingDown();
	}
	return false;
}

void CTaskGoToAndClimbLadder::CleanUp()
{
	CPed *pPed = GetPed();

	// Make sure task can't quit without this being reset
	pPed->SetUseExtractedZ(false);
	// Explicitly reset this flag - there may be another task created this same frame which will look at this.
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsClimbing, false );
}

#if !__FINAL
void CTaskGoToAndClimbLadder::Debug() const
{
#if __BANK

	if (CClimbLadderDebug::ms_bRenderDebug)
	{
#if DEBUG_DRAW
		static const Vector3 vX(0.5f,0.0f,0.0f);
		static const Vector3 vY(0.0f,0.5f,0.0f);

		// Draw the position used for getting onto the ladder
		grcDebugDraw::Line(m_vPositionToGetOn - vX, m_vPositionToGetOn + vX, Color_red);
		grcDebugDraw::Line(m_vPositionToGetOn - vY, m_vPositionToGetOn + vY, Color_red);

		// Draw a cross at the top
		grcDebugDraw::Line(m_vPositionOfTop - vX, m_vPositionOfTop + vX, Color_yellow);
		grcDebugDraw::Line(m_vPositionOfTop - vY, m_vPositionOfTop + vY, Color_yellow);

		// Draw a cross at the bottom
		grcDebugDraw::Line(m_vPositionOfBottom - vX, m_vPositionOfBottom + vX, Color_purple);
		grcDebugDraw::Line(m_vPositionOfBottom - vY, m_vPositionOfBottom + vY, Color_purple);

		// Draw the normal of the climbing surface (same as ladder heading)
		Vector3 vMid = (m_vPositionOfTop + m_vPositionOfBottom) * 0.5f;
		Vector3 vNormal(-rage::Sinf(m_fLadderHeading), rage::Cosf(m_fLadderHeading), 0.0f);
		grcDebugDraw::Line(vMid, vMid + vNormal, Color_yellow, Color_purple);

		DrawNearbyLadder2dEffects(GetPed(), m_pLadder);
#endif // DEBUG_DRAW
	}

		if(GetSubTask())
		{
			GetSubTask()->Debug();
		}

#endif // __BANK
}

#if DEBUG_DRAW
void CTaskGoToAndClimbLadder::DrawNearbyLadder2dEffects(const CPed * pPed, const CEntity * UNUSED_PARAM(pLadderToIgnore))
{
#if __BANK
	if (aiVerifyf(pPed, "No ped round which to draw effects"))
	{
		
		bool bOldDebug = CClimbLadderDebug::ms_bRenderDebug;
		CClimbLadderDebug::ms_bRenderDebug = true; 

		s32 iLadderIndex;
		Vector3 vTarget; 
		CTaskGoToAndClimbLadder::ScanForLadderToClimb(*pPed, iLadderIndex,vTarget);

		CClimbLadderDebug::ms_bRenderDebug = bOldDebug; 
	}
#endif
}
#endif	// DEBUG_DRAW
#endif    

bool CTaskGoToAndClimbLadder::HandlePlayerInput(CPed* pPed)
{
	CControl * pControl = NULL;
	pControl = pPed->GetControlFromPlayer();
	if(pControl && !CControlMgr::IsDisabledControl(pControl))
	{
		// Has player changed their mind about getting onto the ladder ?
		// If the player is approaching the ladder, then holding the stick in the opposite
		// direction breaks-out of the task (similar to when entering a vehicle)
		if(m_eClimbMode != EOnLadder)
		{
			if(pControl->GetPedEnter().IsPressed())
			{	
				SetState(State_Finish);
				return true;
			}
			else
			{
				const Vector2 vecStickInput( pControl->GetPedWalkLeftRight().GetNorm(), pControl->GetPedWalkUpDown().GetNorm() );
				const float fInputMag = vecStickInput.Mag();
				if(fInputMag > 0.75f)
				{
					const float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPed);
					float fStickAngle = rage::Atan2f(-vecStickInput.x, vecStickInput.y);
					fStickAngle = fStickAngle + fCamOrient;
					fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
					const Vector3 vecDesiredDirection(-rage::Sinf(fStickAngle), rage::Cosf(fStickAngle), 0.0f);

					const float fDot = DotProduct(m_vLadderNormal, vecDesiredDirection);
					if((m_eClimbMode==EGetOnAtBottom && fDot > 0.707f) || (m_eClimbMode==EGetOnAtTop && fDot < -0.707f))
					{
						SetState(State_Finish);
						return true;
					}
				}
			}
		}
		// If the player is on the ladder, then pressing triangle gets them to quit
		if (m_eClimbMode == EOnLadder)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePlayerJumping, true);

			//taskAssert(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CLIMB_LADDER);
			//CTaskClimbLadder* pClimbLadderTask = static_cast<CTaskClimbLadder*>(GetSubTask());
			//taskAssert(pClimbLadderTask);
			//s32 iState = pClimbLadderTask->GetState();
			//// Don't quit if we're playing a get on/off clip
			//if (iState == CTaskClimbLadder::State_MountingAtTop || iState == CTaskClimbLadder::State_MountingAtBottom
			//	|| iState == CTaskClimbLadder::State_MountingAtTopRun || iState == CTaskClimbLadder::State_MountingAtBottomRun
			//	|| iState == CTaskClimbLadder::State_DismountAtTop || iState == CTaskClimbLadder::State_DismountAtBottom)

			//{
			//	return false;
			//}
			//else if ((pControl->GetPedEnter().IsDown() && !pControl->GetPedEnter().WasDown()) ||
			//		 (pControl->GetPedJump().IsDown() && !pControl->GetPedJump().WasDown()))
			//{
			//	SetState(State_Finish);
			//	return true;
			//}
		}
	}
	return false;
}

bool CTaskGoToAndClimbLadder::ShouldAbortIfStuck()
{
	// Handle the static-counter reaching its max : we are probably walking into something & looking stupid.
	if(GetPed()->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit
		&& GetTimeInState() > 0.5f)
	{
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		GetPed()->GetPedIntelligence()->AddEvent(staticEvent);
		return true;
	}
	return false; 
}

void CTaskGoToAndClimbLadder::OnAIActionFailed(CPed* pPed)
{
	CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*) pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
	if (pCtrlMove)
	{
		pCtrlMove->OnClimbingLadderFailed();
	}
}

CTask::FSM_Return CTaskGoToAndClimbLadder::ProcessPreFSM()
{
	if (GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskGoToAndClimbLadder::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_MoveToLadderViaNavMesh)
			FSM_OnEnter
			MoveToLadderViaNavMesh_OnEnter(pPed);
		FSM_OnUpdate
			return MoveToLadderViaNavMesh_OnUpdate(pPed);		

		FSM_State(State_MoveToLadderViaGoToPoint)
			FSM_OnEnter
			MoveToLadderViaGoToPoint_OnEnter(pPed);
		FSM_OnUpdate
			return MoveToLadderViaGoToPoint_OnUpdate(pPed);	

		FSM_State(State_SlideToLadder)
			FSM_OnEnter
				SlideToLadder_OnEnter(pPed);
			FSM_OnUpdate
				return SlideToLadder_OnUpdate(pPed);	

		FSM_State(State_ClimbingLadder)
			FSM_OnEnter
				ClimbingLadder_OnEnter(pPed);
			FSM_OnUpdate
				return ClimbingLadder_OnUpdate(pPed);
				
	/*	FSM_State(State_AchieveHeading)
			FSM_OnEnter
			AchieveHeading_OnEnter(pPed);
		FSM_OnUpdate
			return AchieveHeading_OnUpdate(pPed);	*/

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskGoToAndClimbLadder::Start_OnUpdate(CPed* pPed)
{
	// If no ladder, quit
	if (!m_pLadder || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableLadderClimbing))
	{	
		SetState(State_Finish);
		return FSM_Continue;
	}
	else
	{
		Vector3 vNormal;
		if (!FindTopAndBottomOfAllLadderSections(m_pLadder, m_LadderIndex, m_vPositionOfTop, m_vPositionOfBottom, vNormal, m_bCanGetOffAtTop))
		{		
			// Failed to find top & bottom, quit
			SetState(State_Finish);
			return FSM_Continue;
		}

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// Work out whether we're closer to the top or bottom of the ladder
		const float fDiffToTop = Abs(vPedPosition.z-1.0f - m_vPositionOfTop.z);
		const float fDiffToBottom = Abs(vPedPosition.z-1.0f - m_vPositionOfBottom.z);

		if (fDiffToTop < fDiffToBottom)
		{		
			m_eClimbMode = EGetOnAtTop;
			m_bGotOnAtBottom = false;
		}
		else
		{
			m_eClimbMode = EGetOnAtBottom;
			m_bGotOnAtBottom = true;
		}

		bool bGotPositions = CalculatePositionsForThisLadder(pPed);
		if (bGotPositions)
		{
			// Figure out whether we should get on backwards or forwards?
			CalcShouldGetOnForwardsOrBackwards(pPed);
			if (m_bGetOnBackwards)
			{				
				float fNewHeading = fwAngle::LimitRadianAngle(m_fLadderHeading + PI); 
				pPed->SetDesiredHeading(fNewHeading);
			}

			SetState(State_ClimbingLadder);
		}
		else
		{
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

void CTaskGoToAndClimbLadder::MoveToLadderViaNavMesh_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	m_getToLadderTimer.Reset();
	const float fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	const Vector3 vNavPos = m_vPositionToGetOn + Vector3(0.0f,0.0f,1.5f);	// Add a little in case the top 2dfx is lower than expected, meaning we might pick up navmesh underneath
	CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(fMoveBlendRatio, vNavPos, ms_fGoToPointDist);
	pNavTask->SetDontUseLadders(true);
	SetNewTask(rage_new CTaskComplexControlMovement(pNavTask));
}

CTask::FSM_Return CTaskGoToAndClimbLadder::MoveToLadderViaNavMesh_OnUpdate(CPed* pPed)
{
	if(ShouldAbortIfStuck()) 
	{
		SetState(State_Finish); 
		return FSM_Continue;
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		SetState(State_SlideToLadder);
		return FSM_Continue;
	}

	int iTaskType = (GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) ? ((CTaskComplexControlMovement*)GetSubTask())->GetMoveTaskType() : GetSubTask()->GetTaskType();

	// Handle the case where the navmesh route task has failed to find a route.
	// Try to navigate using gotopoint, otherwise quit out.
	if(iTaskType==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
		CTask * pCurrMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
		if(pCurrMoveTask)
		{
			taskAssert(pCurrMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
			CTaskMoveFollowNavMesh * pNavTask = (CTaskMoveFollowNavMesh*)pCurrMoveTask;

			if(pNavTask->GetNavMeshRouteResult()==NAVMESHROUTE_ROUTE_NOT_FOUND && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
			{
				SetState(State_MoveToLadderViaGoToPoint);
				return FSM_Continue;
			}
			else if (m_getToLadderTimer.Tick())
			{
				m_getToLadderTimer.Reset();
				if (pNavTask->GetNavMeshRouteResult()==	NAVMESHROUTE_ROUTE_NOT_YET_TRIED)
				{	
					SetState(State_MoveToLadderViaGoToPoint);
					return FSM_Continue;
				}
			}

			// Fix for a nasty bug.  Sometimes ladders have been placed up the side of large fixed objects, which
			// have no navmesh on them.  What happens if a ped tries to climb down here, is their navmesh route
			// starts at the closest piece of navmesh - which may be several meters away.  As a fix, examine the
			// first point in the navmesh route - and if it's significantly far away from the ped's position then
			// change to a CTaskMoveGoToPointAndStandStill.
			if(!m_bCheckedNavMeshRoute && pNavTask->IsFollowingNavMeshRoute() && pNavTask->GetRoute() && pNavTask->GetRoute()->GetSize()>0)
			{
				const Vector3 & vFirstPoint = pNavTask->GetRoute()->GetPosition(0);
				const Vector3 vDiff = vFirstPoint - m_vPositionToGetOn;
				if(vDiff.Mag2() > 9.0f && GetSubTask()->MakeAbortable( ABORT_PRIORITY_URGENT, NULL))
				{
					m_bCheckedNavMeshRoute = true;
								
					SetState(State_MoveToLadderViaGoToPoint);
					return FSM_Continue;
				}
			}
		}
	}

	// Handle orientating the ped correctly by strafing & setting desired heading
//	pPed->SetIsStrafing(true);
	Vector3 vLadderNormal(-rage::Sinf(m_fLadderHeading), rage::Cosf(m_fLadderHeading), 0.0f);
	//Vector3 vFacingTarget = (m_eClimbMode == EGetOnAtBottom) ?
		// m_vPositionToGetOn + vLadderNormal : m_vPositionToGetOn - vLadderNormal;

	return FSM_Continue;
}

void CTaskGoToAndClimbLadder::MoveToLadderViaGoToPoint_OnEnter(CPed* pPed)
{
	if(m_bGetOnBackwards)
	{
		pPed->SetDesiredHeading(fwAngle::LimitRadianAngle(m_fLadderHeading + PI));
	}

	const float fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	SetNewTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveGoToPointAndStandStill(fMoveBlendRatio, m_vPositionToGetOn, ms_fGoToPointDist) ));
}

CTask::FSM_Return CTaskGoToAndClimbLadder::MoveToLadderViaGoToPoint_OnUpdate(CPed* UNUSED_PARAM(pPed))
{	
	if(ShouldAbortIfStuck()) 
	{
		SetState(State_Finish); 
		return FSM_Continue;
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{	
		SetState(State_SlideToLadder);
	}
	return FSM_Continue;
}

void CTaskGoToAndClimbLadder::SlideToLadder_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	static const float fSpeed = 1000.0f;

	m_fTargetHeading = (m_eClimbMode==EGetOnAtBottom||m_bGetOnBackwards) ? m_fLadderHeading : fwAngle::LimitRadianAngle(m_fLadderHeading + PI);

	Vector3 vSlideTarget = m_vPositionToGetOn;
	if(m_eClimbMode==EGetOnAtTop)
	{
		// This is to make sure that the get-on clip positions us out of collision with
		// the thing object/map which we are climbing down.  The clip is played with a task
		// which automatically repositions the ped at the end..
		vSlideTarget += m_vLadderNormal * ms_fSlideOffset;	
		m_vSlideTargetAtTop = vSlideTarget;
	}

	CTaskSlideToCoord * pTask = rage_new CTaskSlideToCoord(vSlideTarget, m_fTargetHeading, fSpeed);
	pTask->SetHeadingIncrement(PI*2.0f);
	pTask->SetPosAccuracy(0.01f);
	pTask->SetMaxTime(ms_iMaxSlideToLadderTime);
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGoToAndClimbLadder::SlideToLadder_OnUpdate(CPed* pPed)
{
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsNearLaddder, true); // To Allow AI peds properly slide to ladders

	if(ShouldAbortIfStuck()) 
	{	
		SetState(State_Finish); 
		return FSM_Continue;
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_SLIDE_TO_COORD))
	{	
		pPed->SetHeading(m_fTargetHeading);
		pPed->SetDesiredHeading(m_fTargetHeading);
		Vector3 vGetOn = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); 
		if(m_eClimbMode==EGetOnAtTop)
		{
			vGetOn.x = m_vSlideTargetAtTop.x; 
			vGetOn.y = m_vSlideTargetAtTop.y; 
		}
		else
		{
			vGetOn.x = m_vPositionToGetOn.x; 
			vGetOn.y = m_vPositionToGetOn.y; 
		}

		pPed->SetPosition(vGetOn); 
		
		SetState(State_ClimbingLadder);
		
	}
	return FSM_Continue;
}

void CTaskGoToAndClimbLadder::ClimbingLadder_OnEnter(CPed* pPed)
{
	if (pPed->IsNetworkClone() && !m_pLadder)
	{
		// If we are a clone we might not actually have a ladder, do a last final attempt to grab one before ignoring it all together
		// If we fail this we should just hope that the player doesn't reach the climbing ped or it might look funky
		// If that happens and we need to fix it, we should do this procedure in ClimbingLadder_OnUpdate also
		// and start the ladder task once we get a ladder and just "break" into it without enter anims
		Vector3 vTarget;
		m_pLadder = CTaskGoToAndClimbLadder::ScanForLadderToClimb(*pPed,m_LadderIndex,vTarget);

		if (!m_pLadder)
			return;

		GenerateLadderStartUpData(pPed);
	}

	taskAssert(m_eClimbMode == EGetOnAtBottom || m_eClimbMode == EGetOnAtTop);

	// If I'm not a player, calculate the initial AI action....
	const bool bPlayerInControl = pPed->IsPlayer() && pPed->GetMotionData()->GetPlayerHadControlOfPedLastFrame();
	if(!pPed->IsNetworkClone() && !bPlayerInControl)
	{
		CalculateAIAction();
	}

	// calculate the position to get onto the ladder
	Vec3V vStartingPosition = VECTOR3_TO_VEC3V(CalculatePositionToGetOnLadder());

#if DEBUG_DRAW
	ms_debugDraw.AddSphere(vStartingPosition, 0.2f, Color_red, 5000);
#endif // DEBUG_DRAW

	//pPed->GetBonePosition(vStartingPosition, BONETAG_ROOT);

	Vector3 vToLadder = VEC3V_TO_VECTOR3(vStartingPosition) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	CTaskClimbLadder::ClimbingState GetOnMoveState = CTaskClimbLadder::State_MountingAtBottom;

	const float MinRunDistSqr = square(1.75f);
	Vector2 vCurrMoveRatio;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMoveRatio);
	if (vToLadder.XYMag2() > MinRunDistSqr && vCurrMoveRatio.y > 1.5f)
	{
		if (m_eClimbMode == EGetOnAtTop)
			GetOnMoveState = CTaskClimbLadder::State_MountingAtTopRun;
		else if (pPed->GetIsSwimming())
			GetOnMoveState = CTaskClimbLadder::State_MountingAtBottom;
		else
			GetOnMoveState = CTaskClimbLadder::State_MountingAtBottomRun;
	}
	else if (m_eClimbMode == EGetOnAtTop)
	{
		GetOnMoveState = CTaskClimbLadder::State_MountingAtTop;
	}

	SetNewTask(rage_new CTaskClimbLadder(m_pLadder, m_LadderIndex, m_vPositionOfTop, m_vPositionOfBottom, RCC_VECTOR3(vStartingPosition), m_fLadderHeading, m_bCanGetOffAtTop, GetOnMoveState, m_bGetOnBackwards, (CTaskClimbLadder::eStandardLadder), m_eAIAction));

	m_eClimbMode = EOnLadder;
}

CTask::FSM_Return CTaskGoToAndClimbLadder::ClimbingLadder_OnUpdate(CPed* pPed)
{
	if(pPed->IsPlayer() && pPed->GetMotionData()->GetPlayerHasControlOfPedThisFrame() && !pPed->IsNetworkClone())
	{
		// Sets state to finish if true;
		if (HandlePlayerInput(pPed))
		{
			return FSM_Continue;
		}
	}
/*
	// Not required unless we're changing our minds while climbing....
	if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_CLIMB_LADDER && m_eAutoClimbMode!=DontAutoClimb)
	{
		CTaskClimbLadder* pClimbLadderTask = static_cast<CTaskClimbLadder*>(GetSubTask());

		if(m_bGotOnAtBottom)
		{
			CTaskClimbLadder::InputState eAction = CTaskClimbLadder::InputState_WantsToClimbUp;
			if (m_eAutoClimbMode>=AutoClimbFast)
			{
				eAction = CTaskClimbLadder::InputState_WantsToClimbUpFast;
			}
			pClimbLadderTask->SetInputStateFromAI(eAction);
		}
		else
		{
			CTaskClimbLadder::InputState eAction = CTaskClimbLadder::InputState_WantsToClimbDown;
			if (m_eAutoClimbMode==AutoClimbFast)
			{
				eAction = CTaskClimbLadder::InputState_WantsToClimbDownFast;
			}
			else if (m_eAutoClimbMode==AutoClimbExtraFast)
			{
				eAction = CTaskClimbLadder::InputState_WantsToSlideDown;
			}
			pClimbLadderTask->SetInputStateFromAI(eAction);
		}
	}
*/
	CTask* pSubTask = GetSubTask();
	CTaskClimbLadder* pClimbLadderTask = (pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_CLIMB_LADDER) ? static_cast<CTaskClimbLadder*>(pSubTask) : NULL;

	if (GetIsSubtaskFinished(CTaskTypes::TASK_CLIMB_LADDER))
	{
		// Check if the AI Action failed
		if (pClimbLadderTask && pClimbLadderTask->GetAIClimbState() != m_eAIAction)
		{
			OnAIActionFailed(pPed);
		}

		SetState(State_Finish);
	}
	else
	{
		// If we have an AI ped blocked by the player, change direction and move fast
		if (!pPed->IsPlayer())
		{
			CEntity* pEntityBlockingMovement = pClimbLadderTask->GetEntityBlockingMovement();

			const CPed* pPedBlockingMovement = pEntityBlockingMovement && pEntityBlockingMovement->GetIsTypePed() ? SafeCast(const CPed, pEntityBlockingMovement) : NULL;
			if (pPedBlockingMovement && pPedBlockingMovement->IsPlayer())
			{
				InputState climbState = pClimbLadderTask->GetAIClimbState();
				if ((climbState == InputState_WantsToClimbUp) || (climbState == InputState_WantsToClimbUpFast))
				{
					pClimbLadderTask->SetInputStateFromAI(CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast);
				}
				else if ((climbState == InputState_WantsToClimbDown) || (climbState == InputState_WantsToClimbDownFast))
				{
					pClimbLadderTask->SetInputStateFromAI(CTaskGoToAndClimbLadder::InputState_WantsToClimbUpFast);
				}
			}
		}
	}

	return FSM_Continue;
}

#if !__FINAL
const char * CTaskGoToAndClimbLadder::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start					: return "State_Start";
		case State_MoveToLadderViaNavMesh	: return "State_MoveToLadderViaNavMesh";
		case State_MoveToLadderViaGoToPoint : return "State_MoveToLadderViaGoToPoint";
		case State_SlideToLadder			: return "State_SlideToLadder";
		case State_ClimbingLadder			: return "State_ClimbingLadder";
		case State_Finish					: return "State_Finish";
		default : taskAssertf(0,"Unhandled state name");
	}

	return "State_Invalid";
}
#endif // !__FINAL

bool CTaskGoToAndClimbLadder::GenerateLadderStartUpData(CPed* pPed)
{
	if(!m_pLadder)
	{
		return false;
	}

	Assert(m_LadderIndex != -1);

	bool dataGenerated = true;
	Vector3 vNormal;
	dataGenerated &= FindTopAndBottomOfAllLadderSections(m_pLadder, m_LadderIndex, m_vPositionOfTop, m_vPositionOfBottom, vNormal, m_bCanGetOffAtTop);
	
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Work out whether we're closer to the top or bottom of the ladder
	const float fDiffToTop = Abs(vPedPosition.z-1.0f - m_vPositionOfTop.z);
	const float fDiffToBottom = Abs(vPedPosition.z-1.0f - m_vPositionOfBottom.z);

	if (fDiffToTop < fDiffToBottom)
	{
		m_eClimbMode = EGetOnAtTop;
		m_bGotOnAtBottom = false;
	}
	else
	{
		m_eClimbMode = EGetOnAtBottom;
		m_bGotOnAtBottom = true;
	}

	dataGenerated &= CalculatePositionsForThisLadder(pPed);

	CalcShouldGetOnForwardsOrBackwards(pPed);

	if (m_bGetOnBackwards)
	{				
		float fNewHeading = fwAngle::LimitRadianAngle(m_fLadderHeading + PI); 
		
		pPed->SetDesiredHeading(fNewHeading);		
	}

	return dataGenerated;
}

void CTaskGoToAndClimbLadder::CalculateAIAction( )
{
	// if we've not set this (i.e. it's not been passed into the cstr to force this action)....
	if(m_eAIAction == InputState_Nothing)
	{
		if(m_bGotOnAtBottom)
		{
			m_eAIAction = InputState_WantsToClimbUp;
			if (m_eAutoClimbMode>=AutoClimbFast)
			{
				m_eAIAction = InputState_WantsToClimbUpFast;
			}
		}
		else
		{
			m_eAIAction = InputState_WantsToClimbDown;
			if (m_eAutoClimbMode==AutoClimbFast)
			{
				m_eAIAction = InputState_WantsToClimbDownFast;
			}
			else if (m_eAutoClimbMode==AutoClimbExtraFast)
			{
				m_eAIAction = InputState_WantsToSlideDown;
			}
		}
	}
	else
	{
		Assert(m_eAIAction >= InputState_Nothing);
		Assert(m_eAIAction <= InputState_Max);
	}
}

//const int CTaskClimbLadderFully::ms_nMaxBlockedLadders = 64;
CTaskClimbLadderFully::SLadderBlock CTaskClimbLadderFully::ms_pBlockedLadders[ms_nMaxBlockedLadders];
int CTaskClimbLadderFully::ms_nBlockedLadders = 0;

//////////////////////////////////////////////////////////////////////////
// CTaskClimbLadderFully
//////////////////////////////////////////////////////////////////////////

CTaskClimbLadderFully::CTaskClimbLadderFully(const float fMoveBlendRatio) 
: m_fMoveBlendRatio(fMoveBlendRatio)
, m_iLadderIndex(-1)
, m_pLadder(NULL)
, m_BlockLadderFlag(0)
{
	SetInternalTaskType(CTaskTypes::TASK_CLIMB_LADDER_FULLY);
}

CTaskClimbLadderFully::~CTaskClimbLadderFully()
{
	ReleaseLadder(m_pLadder, m_iLadderIndex);
}

#if !__FINAL
void CTaskClimbLadderFully::Debug() const
{
#if __BANK
	if (CClimbLadderDebug::ms_bRenderDebug)
	{
		if(GetSubTask())
		{
			GetSubTask()->Debug();
		}
	}
#endif // __BANK
}
#endif    

CTask::FSM_Return CTaskClimbLadderFully::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

	FSM_State(State_WaitToUseLadder)
		FSM_OnUpdate
			return WaitToUseLadder_OnUpdate(pPed);

	FSM_State(State_GoToAndClimbLadder)
		FSM_OnEnter
			GoToAndClimbLadder_OnEnter(pPed);
		FSM_OnUpdate
			return GoToAndClimbLadder_OnUpdate(pPed);

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskClimbLadderFully::Start_OnUpdate(CPed* pPed)
{
	Vector3 vTarget; 

	m_pLadder = CTaskGoToAndClimbLadder::ScanForLadderToClimb(*pPed,m_iLadderIndex,vTarget); 

	if(m_pLadder)
	{
		Assertf(!(m_pLadder->GetIplIndex() == 0xdddd && m_pLadder->GetAudioBin() == 0xdd), "Ladder is deleted in Start_OnUpdate()");

		if (CTaskGoToAndClimbLadder::IsGettingOnBottom(m_pLadder, *pPed, m_iLadderIndex))
			m_BlockLadderFlag = BF_CLIMBING_UP;
		else
			m_BlockLadderFlag = BF_CLIMBING_DOWN;

		SetState(State_WaitToUseLadder);
	}
	else
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskClimbLadderFully::WaitToUseLadder_OnUpdate(CPed* pPed)
{
	if(NetworkInterface::IsGameInProgress())
	{
		// Is there a ped preventing us from getting on the bottom or top...
		if(IsLadderBaseOrTopBlockedByPedMP(m_pLadder, m_iLadderIndex, m_BlockLadderFlag))
			return FSM_Continue;

		if(CTaskGoToAndClimbLadder::IsGettingOnBottom(m_pLadder, *pPed, m_iLadderIndex))
		{
			// Is there a ped or player standing in front of the base of the ladder that would prevent us getting on...
			if(IsLadderPhysicallyBlockedAtBaseMP(pPed, m_pLadder, m_iLadderIndex))
				return FSM_Continue;
		}

		// if there a ped climbing in the opposite direction we would like to climb.
		if(IsLadderNPCBlockedByPedClimbingInOppositeDirectionMP(pPed, m_pLadder, m_iLadderIndex, m_BlockLadderFlag))
			return FSM_Continue;
	}
	else
	{
		if (IsLadderBlocked(m_pLadder, m_iLadderIndex, fwTimer::GetTimeInMilliseconds(), m_BlockLadderFlag))
			return FSM_Continue;
	}

	SetState(State_GoToAndClimbLadder);
	return FSM_Continue;
}

void CTaskClimbLadderFully::GoToAndClimbLadder_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskGoToAndClimbLadder::eAutoClimbMode eClimb = CTaskGoToAndClimbLadder::AutoClimbNormal;
	if (CPedMotionData::GetIsRunning(m_fMoveBlendRatio))
		eClimb = CTaskGoToAndClimbLadder::AutoClimbFast;
	else if(CPedMotionData::GetIsSprinting(m_fMoveBlendRatio))
		eClimb = CTaskGoToAndClimbLadder::AutoClimbExtraFast;

	BlockLadder(m_pLadder, m_iLadderIndex, fwTimer::GetTimeInMilliseconds(), m_BlockLadderFlag);
	SetNewTask(rage_new CTaskGoToAndClimbLadder(m_pLadder,m_iLadderIndex,eClimb ));
}

CTask::FSM_Return CTaskClimbLadderFully::GoToAndClimbLadder_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_GO_TO_AND_CLIMB_LADDER))
	{
		ReleaseLadder(m_pLadder, m_iLadderIndex);
		m_pLadder = NULL; // To prevent us releasing someone elses ladder in the destructor
		SetState(State_Finish);
	}
	return FSM_Continue;
}


void CTaskClimbLadderFully::BlockLadder(CEntity* pLadder, s32 iLadder, u32 TimeStamp, u32 Flags)
{
	if (ms_nBlockedLadders >= ms_nMaxBlockedLadders)
		return;

	// We can't block ladders this way in MP or we might end up with blocked ladders that "cannot be unblocked"
	// Need a better solution for MP or just accept it there
	// It is a fairly rare bug that peds end up inside each other on a ladder anyway
	if (NetworkInterface::IsGameInProgress())
		return;

	if (!pLadder)
		return;

	if (IsLadderBlocked(pLadder, iLadder, TimeStamp, Flags))
		return;

	ms_pBlockedLadders[ms_nBlockedLadders].m_pLadder = pLadder;
	ms_pBlockedLadders[ms_nBlockedLadders].m_iLadder = iLadder;
	ms_pBlockedLadders[ms_nBlockedLadders].m_TimeStamp = TimeStamp;
	ms_pBlockedLadders[ms_nBlockedLadders++].m_Flags = Flags;
}

void CTaskClimbLadderFully::ReleaseLadder(CEntity* pLadder, s32 iLadder)
{
	if (ms_nBlockedLadders > 0)
	{
		int iBlockedIndex = GetOldestBlockedLadderIndex(pLadder, iLadder);
		if (iBlockedIndex >= 0)
			ms_pBlockedLadders[iBlockedIndex] = ms_pBlockedLadders[--ms_nBlockedLadders];

	}
}

int CTaskClimbLadderFully::GetNewestBlockedLadderIndex(CEntity* pLadder, s32 iLadder)
{
	u32 OldestTimeStamp = 0;
	int iSelectedLadder = -1;
	for (int i = 0; i < ms_nBlockedLadders; ++i)
	{
		if (ms_pBlockedLadders[i].m_pLadder == pLadder && ms_pBlockedLadders[i].m_iLadder == iLadder)
		{
			if (OldestTimeStamp < ms_pBlockedLadders[i].m_TimeStamp)
			{
				OldestTimeStamp = ms_pBlockedLadders[i].m_TimeStamp;
				iSelectedLadder = i;
			}
		}
	}

	return iSelectedLadder;
}

int CTaskClimbLadderFully::GetOldestBlockedLadderIndex(CEntity* pLadder, s32 iLadder)
{
	u32 OldestTimeStamp = UINT_MAX;
	int iSelectedLadder = -1;
	for (int i = 0; i < ms_nBlockedLadders; ++i)
	{
		if (ms_pBlockedLadders[i].m_pLadder == pLadder && ms_pBlockedLadders[i].m_iLadder == iLadder)
		{
			if (OldestTimeStamp > ms_pBlockedLadders[i].m_TimeStamp)
			{
				OldestTimeStamp = ms_pBlockedLadders[i].m_TimeStamp;
				iSelectedLadder = i;
			}
		}
	}

	return iSelectedLadder;
}

bool CTaskClimbLadderFully::IsLadderBlocked(CEntity* pLadder, s32 iLadder, u32 TimeStamp, u32 Flags)
{
	static bank_u32 LadderTimeout = 2250;	// So other peds can start climbing before the first is finished
	int iNewestBlockedLadder = GetNewestBlockedLadderIndex(pLadder, iLadder);
	if (iNewestBlockedLadder >= 0)
	{
		if (TimeStamp - ms_pBlockedLadders[iNewestBlockedLadder].m_TimeStamp < LadderTimeout)
			return true;

		// Eg. if we want to be climbing up but someone else is also climbing up, this will match and allow climbing!
		if (!(ms_pBlockedLadders[iNewestBlockedLadder].m_Flags & Flags))
			return true;
	}

	return false;
}

//-----------------------------------------------------------------
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//-----------------------------------------------------------------

// Is there a ped standing around the base of the ladder extremely close but not on it?
// This can cause problems if another ped gets on the ladder as it will pin the first 
// or push it through the wall behind the ladder and out the world.
/* static */ bool CTaskClimbLadderFully::IsLadderPhysicallyBlockedAtBaseMP(CPed const* pPed, CEntity* pLadder, s32 iLadder)
{
	if(!NetworkInterface::IsGameInProgress())
		return false;

	if(!pLadder)
		return false;

	if(!pPed)
		return false;

	// Create a box around the base of the ladder 
	Vector3 top(VEC3_ZERO);
	Vector3 bottom(VEC3_ZERO);
	Vector3 normal(VEC3_ZERO);

	bool canGetOffAtTop = false;
	if(CTaskGoToAndClimbLadder::FindTopAndBottomOfAllLadderSections(pLadder, iLadder, top, bottom, normal, canGetOffAtTop))
	{
		TUNE_GROUP_FLOAT(LADDER_DEBUG, LadderBoundingBoxHalfWidth, 0.045f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, LadderBoundingBoxHalfDepth, 0.1f, 0.0f, 1.0f, 0.1f);
		TUNE_GROUP_FLOAT(LADDER_DEBUG, LadderBoundingBoxHalfHeight,1.0f, 0.0f, 4.0f, 0.1f);	// Could just use ped height?
		
		Matrix34 rotMatrix(M34_IDENTITY); 
		camFrame::ComputeWorldMatrixFromFrontAndUp(normal, ZAXIS, rotMatrix);		
		rotMatrix.d = bottom;
		
		Vector3 delta(LadderBoundingBoxHalfWidth, LadderBoundingBoxHalfDepth, 0.0f);

		Vector3 min = -delta;
		Vector3 max = delta + Vector3(0.0f, 0.0f, LadderBoundingBoxHalfHeight * 2.0f);

		Vector3 boundingBoxSize = max - min;
		CREATE_SIMPLE_BOUND_ON_STACK(phBoundBox, boundingBox, boundingBoxSize);

		WorldProbe::CShapeTestBoundDesc boundingBoxDesc;
		boundingBoxDesc.SetBound(&boundingBox);
		boundingBoxDesc.SetTransform(&rotMatrix);
		boundingBoxDesc.SetIncludeFlags(ArchetypeFlags::GTA_PED_TYPE);
		boundingBoxDesc.SetContext(WorldProbe::ENetwork);	
		boundingBoxDesc.AddExludeInstance(pPed->GetCurrentPhysicsInst());

		if(WorldProbe::GetShapeTestManager()->SubmitTest(boundingBoxDesc))
		{
			//grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(min), VECTOR3_TO_VEC3V(max), MATRIX34_TO_MAT34V(rotMatrix), Color_red, false);
			return true;
		}
		//else
		//{
			//grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(min), VECTOR3_TO_VEC3V(max), MATRIX34_TO_MAT34V(rotMatrix), Color_magenta, false);
		//}
	}

	return false;
}

/* static */ bool CTaskClimbLadderFully::IsLadderNPCBlockedByPedClimbingInOppositeDirectionMP(CPed const* pPed, CEntity* pLadder, s32 iLadder, u32 const flags)
{
	// local peds only...
	if(!pPed || pPed->IsNetworkClone())
	{
		return false;
	}

	// NPC's only - players do whatever they want...
	if(pPed->IsPlayer())
	{
		return false;
	}

	Vector3 pedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Go through all the nearyby peds and see if any or climbing the same ladder / part of ladder in the opposite direction to our desired direction...
	CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		CPed* pNearbyPed = static_cast<CPed*>(pEnt);
		if( (pNearbyPed) && ( pNearbyPed != pPed ) )
		{
			Vector3 nearPlayerPos = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());	

			// if the ped within a 30m cylinder of us at any height (could be at the bottom of a very tall ladder)
			if((pedPos - nearPlayerPos).XYMag2() < (10.0f * 10.0f))
			{
				CTaskClimbLadder* nearbyLadderTask = static_cast<CTaskClimbLadder*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CLIMB_LADDER));
				if(nearbyLadderTask)
				{
					if((nearbyLadderTask->GetLadder() == pLadder) && (nearbyLadderTask->GetEffectIndex() == iLadder))
					{
						// NPC won't get on ladder if player is on it
						if(pNearbyPed->IsPlayer())
						{
							return true;
						}
						else
						{
							// NPC won't get on ladder if another NPC is climbing in opposite direction...
							CTaskGoToAndClimbLadder::InputState climbState = nearbyLadderTask->GetAIClimbState();

							if(flags == BF_CLIMBING_UP)
							{
								if((climbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDown)			|| 
									(climbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbDownFast)	|| 
										(climbState == CTaskGoToAndClimbLadder::InputState_WantsToLetGo)		||
											(climbState == CTaskGoToAndClimbLadder::InputState_WantsToSlideDown))
								{
									return true;
								}
							}
							else if(flags == BF_CLIMBING_DOWN)
							{
								if((climbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUp) || (climbState == CTaskGoToAndClimbLadder::InputState_WantsToClimbUpFast))
								{
									return true;
								}
							}
							else
							{
								Assert(false); // huh?
							}
						}
					}
				}
			}
		}
	}

	return false;
}

/* static */ bool CTaskClimbLadderFully::BlockLadderMP(CEntity* pLadder, s32 iLadder, u32 Flags, s32& index)
{
	if (!pLadder)
		return false;

	if (IsLadderBaseOrTopBlockedByPedMP(pLadder, iLadder, Flags))
		return false;	

	for(int i = 0; i < ms_nMaxBlockedLadders; ++i)
	{
		if(ms_pBlockedLadders[i].m_pLadder.Get() == NULL)
		{
			index = i;

			ms_pBlockedLadders[i].m_TimeStamp	= 0;
			ms_pBlockedLadders[i].m_pLadder	= pLadder;
			ms_pBlockedLadders[i].m_iLadder	= iLadder;
			ms_pBlockedLadders[i].m_Flags	= Flags;
			return true;
		}
	}

	return false;
}

/* static */ bool CTaskClimbLadderFully::ReleaseLadderByIndexMP(s32 index)
{
	if((index >= 0) && (index < ms_nMaxBlockedLadders))
	{	
		ms_pBlockedLadders[index].Reset();
		return true;
	}

	return false;
}

/* static */ int CTaskClimbLadderFully::GetBlockedLadderIndexMP(CEntity* pLadder, s32 iLadder, u32 Flags)
{
	for(int i = 0; i < ms_nMaxBlockedLadders; ++i)
	{
		if(ms_pBlockedLadders[i].m_pLadder.Get())
		{
			if((ms_pBlockedLadders[i].m_Flags == Flags) && (ms_pBlockedLadders[i].m_iLadder == iLadder) && (ms_pBlockedLadders[i].m_pLadder == pLadder))
			{
				return i;
			}
		}
	}

	return -1;
}

/* static */ bool CTaskClimbLadderFully::IsLadderBaseOrTopBlockedByPedMP(CEntity* pLadder, s32 iLadder, u32 Flags)
{
	if(pLadder)
	{
		return GetBlockedLadderIndexMP(pLadder, iLadder, Flags) != -1;
	}

	return false;
}

#if !__FINAL && DEBUG_DRAW

/* static */ void CTaskClimbLadderFully::DebugRenderBlockedInfo(void)
{
	char buffer[1024] = "\0";
	char temp[100] = "\0";

	strcat(buffer, "//-------------------------------------\n");
	
	sprintf(temp, "%s\n", __FUNCTION__);
	strcat(buffer, temp);

	for(int i = 0; i < ms_nMaxBlockedLadders; ++i)
	{
		if(ms_pBlockedLadders[i].m_pLadder.Get())
		{
			sprintf(temp,  "%d : Ladder %p : iLadder %d : TimeStamp %d : m_Flags BF_CLIMBING_UP %d, BF_CLIMBING_DOWN %d\n", 
			i, 
			ms_pBlockedLadders[i].m_pLadder.Get(), 
			ms_pBlockedLadders[i].m_iLadder, 
			ms_pBlockedLadders[i].m_TimeStamp, 
			ms_pBlockedLadders[i].m_Flags & BF_CLIMBING_UP, 
			ms_pBlockedLadders[i].m_Flags & BF_CLIMBING_DOWN);
			strcat(buffer, temp);
		}
	}

	sprintf(temp, "//-------------------------------------\n");
	strcat(buffer, temp);
	
	Vector3 pos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
	pos += Vector3(0.0f, 0.0f, 2.0f);
	grcDebugDraw::Text(pos, Color_white, buffer, false);
}

#endif /* !__FINAL && DEBUG_DRAW */

#if !__FINAL
const char * CTaskClimbLadderFully::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start				: return "State_Start";
		case State_WaitToUseLadder		: return "State_WaitToUseLadder";
		case State_GoToAndClimbLadder	: return "State_GoToAndClimbLadder";
		case State_Finish				: return "State_Finish";
		default : taskAssertf(0,"Unhandled state name");
	}

	return "State_Invalid";
}
#endif // !__FINAL
