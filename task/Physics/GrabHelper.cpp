// Filename   :	GrabHelper.cpp
// Description:	Helper class for natural motion tasks which need grab behaviour.

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "Peds/Ped.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "System/ControlMgr.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/GrabHelper.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CGrabHelper, CONFIGURED_FROM_FILE, atHashString("CGrabHelper",0x1116f36b));

// Tunable parameters. ///////////////////////////////////////////////////
CGrabHelper::Tunables CGrabHelper::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CGrabHelper, 0x1116f36b);
//////////////////////////////////////////////////////////////////////////

atHashString CGrabHelper::TUNING_SET_DEFAULT("Default",0xE4DF46D5);

bank_float TEST_FOR_GRAB_VEHICLE_VELOCITY = 12.0f;

Vector3	CGrabHelper::NM_grab_up(0,0,1.0f);

#if __DEV
bool CGrabHelper::ms_bDisplayDebug = false;
#endif

CGrabHelper::CGrabHelper(const CGrabHelper* copyFrom)
{
	// Start by using head look
	m_nFlags = 0;
	m_nStartGrabTime = 0;

	m_pGrabEnt = NULL;
	m_iBoundIndex = 0;

	m_vecGrab1.Zero();
	m_vecNorm1.Zero();
	m_vecGrab2.Zero();
	m_vecNorm2.Zero();
	m_vecGrab3.Zero();
	m_vecGrab4.Zero();

	m_bHasAborted = false;
	m_bHasFailed = false;
	m_bHasSucceeded = false;

	m_fAbortGrabAttemptTimer = 0.0f;
	m_fAbortGrabTimer = 0.0f;

	m_InitialTuningSet = TUNING_SET_DEFAULT;
	m_BalanceFailedTuningSet.SetHash(0);

	if(copyFrom)
	{
		m_nFlags = copyFrom->m_nFlags;
		m_nStartGrabTime = copyFrom->m_nStartGrabTime;

		m_pGrabEnt = copyFrom->m_pGrabEnt;

		m_iBoundIndex = copyFrom->m_iBoundIndex;

		m_vecGrab1 = copyFrom->m_vecGrab1;
		m_vecNorm1 = copyFrom->m_vecNorm1;

		// Catch invalid normals. Ideally we would like to trap bad normals with a stricter tolerance, but rage doesn't guarantee very accurate normals it seems.
		Assertf((m_vecNorm1.Mag2()+0.1f >= 0.0f) && (m_vecNorm1.Mag2()-0.1f <= 1.0f), "[CGrabHelper::CGrabHelper copy constructor] invalid grab normal (m_vecNorm1) (%f, %f, %f) = %f", m_vecNorm1.x, m_vecNorm1.y, m_vecNorm1.z, m_vecNorm1.Mag());

		m_vecGrab2 = copyFrom->m_vecGrab2;
		m_vecNorm2 = copyFrom->m_vecNorm2;

		// Catch invalid normals. Ideally we would like to trap bad normals with a stricter tolerance, but rage doesn't guarantee very accurate normals it seems.
		Assertf((m_vecNorm2.Mag2()+0.1f >= 0.0f) && (m_vecNorm2.Mag2()-0.1f <= 1.0f), "[CGrabHelper::CGrabHelper copy constructor] invalid grab normal (m_vecNorm2) (%f, %f, %f) = %f", m_vecNorm2.x, m_vecNorm2.y, m_vecNorm2.z, m_vecNorm2.Mag());

		m_vecGrab3 = copyFrom->m_vecGrab3;
		m_vecGrab4 = copyFrom->m_vecGrab4;

		m_bHasAborted = copyFrom->m_bHasAborted;
		m_bHasFailed = copyFrom->m_bHasFailed;
		m_bHasSucceeded = copyFrom->m_bHasSucceeded;

		m_fAbortGrabAttemptTimer = copyFrom->m_fAbortGrabAttemptTimer;
		m_fAbortGrabTimer = copyFrom->m_fAbortGrabTimer;
 
		m_InitialTuningSet = copyFrom->m_InitialTuningSet;
		m_BalanceFailedTuningSet = copyFrom->m_BalanceFailedTuningSet;
	}
}

CGrabHelper::~CGrabHelper()
{
}

// explicitly set the grab target (this will set target scan flags to TARGET_SPECIFIC)
void CGrabHelper::SetGrabTarget(CEntity* pGrabEnt, Vector3* pGrab1, Vector3* pNormal1, Vector3* pGrab2, Vector3* pNormal2, Vector3* pGrab3, Vector3* pGrab4)
{
	if(GetFlag(GRAB_SUCCEEDED_LHAND) || GetFlag(GRAB_SUCCEEDED_RHAND))
	{
		// oh dear - what to do now - tell the grab to release?
	}

	m_pGrabEnt = pGrabEnt;

	if(pGrab1)
		m_vecGrab1 = *pGrab1;
	if(pNormal1)
		m_vecNorm1 = *pNormal1;

	// Catch invalid normals. Ideally we would like to trap bad normals with a stricter tolerance, but rage doesn't guarantee very accurate normals it seems.
	Assertf((m_vecNorm1.Mag()+0.1f >= 0.0f) && (m_vecNorm1.Mag()-0.1f <= 1.0f), "[CGrabHelper::SetGrabTarget] invalid grab normal (m_vecNorm1) (%f, %f, %f) = %f", m_vecNorm1.x, m_vecNorm1.y, m_vecNorm1.z, m_vecNorm1.Mag());

	if(pGrab2)
		m_vecGrab2 = *pGrab2;
	if(pNormal2)
		m_vecNorm2 = *pNormal2;

	// Catch invalid normals. Ideally we would like to trap bad normals with a stricter tolerance, but rage doesn't guarantee very accurate normals it seems.
	Assertf((m_vecNorm2.Mag()+0.1f >= 0.0f) && (m_vecNorm2.Mag()-0.1f <= 1.0f), "[CGrabHelper::SetGrabTarget] invalid grab normal (m_vecNorm2) (%f, %f, %f) = %f", m_vecNorm2.x, m_vecNorm2.y, m_vecNorm2.z, m_vecNorm2.Mag());

	if(pGrab3)
		m_vecGrab3 = *pGrab3;
	if(pGrab4)
		m_vecGrab4 = *pGrab4;

	SetFlag(TARGET_SPECIFIC);
}

void CGrabHelper::SetGrabTarget_BoundIndex(int iIndex)
{
	m_iBoundIndex = iIndex;
}

void CGrabHelper::Abort(CPed* pPed)
{
	((void)pPed);

	// actually there's not much I can do here that's useful
	m_bHasAborted = true;
}

void CGrabHelper::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_GRAB_L_FB) || CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_GRAB_R_FB))
	{
		SetFlag(GRAB_FAILED);
	}

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		CNmTuningSet* pSet = sm_Tunables.m_Sets.Get(m_BalanceFailedTuningSet);
		if (pSet)
		{
			pSet->Post(*pPed, NULL);
		}
	}

}
void CGrabHelper::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	((void)pPed);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_GRAB_L_FB))
		SetFlag(GRAB_SUCCEEDED_LHAND);
	else if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_GRAB_R_FB))
		SetFlag(GRAB_SUCCEEDED_RHAND);
}
void CGrabHelper::BehaviourFinish(CPed* UNUSED_PARAM(pPed), ARTFeedbackInterfaceGta* UNUSED_PARAM(pFeedbackInterface))
{
}

// these should be called within the same functions of the owner task
void CGrabHelper::StartBehaviour(CPed* pPed)
{
	// if a grab has been specified then start grabbing for it now (might want to put a range on this
	if(m_vecGrab1.IsNonZero() || m_vecGrab2.IsNonZero())
	{
		ART::MessageParams msgGrab;

		// only set start flag if we're not already trying to grab
		if(!GetFlag(GRAB_TRYING))
		{
			msgGrab.addBool(NMSTR_PARAM(NM_START), true);

			m_nStartGrabTime = fwTimer::GetTimeInMilliseconds();
			SetFlag(GRAB_TRYING);
		}

		if(m_vecGrab1.IsNonZero())
		{
			msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS1_VEC3), m_vecGrab1.x, m_vecGrab1.y, m_vecGrab1.z);
		} 
		if(m_vecNorm1.IsNonZero())
		{
			// Catch invalid normals. Ideally we would like to trap bad normals with a stricter tolerance, but rage doesn't guarantee very accurate normals it seems.
			m_vecNorm1.Normalize();
			Assertf((m_vecNorm1.Mag2()+0.1f >= 0.0f) && (m_vecNorm1.Mag2()-0.1f <= 1.0f), "[CGrabHelper::StartBehaviour()] invalid grab normal (m_vecNorm1) (%f, %f, %f) = %f", m_vecNorm1.x, m_vecNorm1.y, m_vecNorm1.z, m_vecNorm1.Mag());

			msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_NORM1_VEC3), m_vecNorm1.x, m_vecNorm1.y, m_vecNorm1.z);
		}
		if(m_vecGrab2.IsNonZero())
		{
			msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS2_VEC3), m_vecGrab2.x, m_vecGrab2.y, m_vecGrab2.z);
		}
		if(m_vecNorm2.IsNonZero())
		{
			// Catch invalid normals. Ideally we would like to trap bad normals with a stricter tolerance, but rage doesn't guarantee very accurate normals it seems.
			m_vecNorm2.Normalize();
			Assertf((m_vecNorm2.Mag2()+0.1f >= 0.0f) && (m_vecNorm2.Mag2()-0.1f <= 1.0f), "[CGrabHelper::StartBehaviour()] invalid grab normal (m_vecNorm2) (%f, %f, %f) = %f", m_vecNorm2.x, m_vecNorm2.y, m_vecNorm2.z, m_vecNorm2.Mag());

			msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_NORM2_VEC3), m_vecNorm2.x, m_vecNorm2.y, m_vecNorm2.z);
		}
		if(m_vecGrab3.IsNonZero())
		{
			msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS3_VEC3), m_vecGrab3.x, m_vecGrab3.y, m_vecGrab3.z);
		}
		if(m_vecGrab4.IsNonZero())
		{
			msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS4_VEC3), m_vecGrab4.x, m_vecGrab4.y, m_vecGrab4.z);
		}

		if(m_pGrabEnt) {
			msgGrab.addInt(NMSTR_PARAM(NM_GRAB_INSTANCE_INDEX), m_pGrabEnt->GetCurrentPhysicsInst()->GetLevelIndex());
			msgGrab.addInt(NMSTR_PARAM(NM_GRAB_INSTANCE_PART_INDEX), m_iBoundIndex);
		}

		msgGrab.addBool(NMSTR_PARAM(NM_GRAB_USE_LEFT), m_vecGrab2.IsNonZero());
		msgGrab.addBool(NMSTR_PARAM(NM_GRAB_USE_RIGHT), m_vecGrab1.IsNonZero());

		// two-point, line or surface grab?
		if (GetFlag(TARGET_LINE_GRAB))
			msgGrab.addBool(NMSTR_PARAM(NM_GRAB_A_LINE), true);
		else if(GetFlag(TARGET_SURFACE_GRAB))
			msgGrab.addBool(NMSTR_PARAM(NM_GRAB_SURFACE), true);

		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_GRAB_MSG), &msgGrab);

		CNmTuningSet* pSet = sm_Tunables.m_Sets.Get(m_InitialTuningSet);
		if (pSet)
		{
			pSet->Post(*pPed, NULL);
		}
	}
}

void CGrabHelper::ControlBehaviour(CPed* pPed)
{
	const bool bSuccesfullyGrabbed = GetFlag(GRAB_SUCCEEDED_LHAND|GRAB_SUCCEEDED_RHAND);
	// Stop grabbing if the abort flag is set
	bool bAbortAnyRunningGrab = GetFlag(TARGET_ABORT_GRABBING);

	if(bSuccesfullyGrabbed && m_fAbortGrabTimer > 0.0f)
	{
		m_fAbortGrabTimer -= fwTimer::GetTimeStep();
		if(m_fAbortGrabTimer <= 0.0f)
		{
			bAbortAnyRunningGrab = true;
		}
	}
	// Check for the player hitting the "stop grabbing" button
	if( !bAbortAnyRunningGrab && pPed->IsLocalPlayer() )
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		bAbortAnyRunningGrab = pControl && pControl->GetPedEnter().IsPressed();
	}

	if( !bAbortAnyRunningGrab && m_pGrabEnt && m_pGrabEnt->GetIsTypeVehicle() )
	{
		Vector3 vRelVel = static_cast<CVehicle*>(m_pGrabEnt.Get())->GetRelativeVelocity(*pPed);
		// ignore fast moving vehicles
		bAbortAnyRunningGrab = (vRelVel.Mag2() > square(TEST_FOR_GRAB_VEHICLE_VELOCITY));
	}

	if( bAbortAnyRunningGrab &&bSuccesfullyGrabbed )
	{
		ART::MessageParams msgGrab;
		msgGrab.addBool(NMSTR_PARAM(NM_START), false);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_GRAB_MSG), &msgGrab);
		SetFlags(0);
	}
	// If the grab attempt should be aborted after time, count down
	if( !bSuccesfullyGrabbed && m_fAbortGrabAttemptTimer > 0.0f )
	{
		m_fAbortGrabAttemptTimer -= fwTimer::GetTimeStep();
		if( m_fAbortGrabAttemptTimer <= 0.0f )
		{
			ART::MessageParams msgGrab;
			msgGrab.addBool(NMSTR_PARAM(NM_START), false);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_GRAB_MSG), &msgGrab);
			SetFlags(0);
		}
	}
}

static float sfGrabStandingMinSpeed = 0.3f;
static float sfGrabFallingMinSpeed = 0.5f;
bool CGrabHelper::FinishConditions(CPed* pPed)
{
	Matrix34 matRoot;
	pPed->GetBoneMatrix(matRoot, BONETAG_ROOT);
	Vector3 vecRootSpeed = pPed->GetLocalSpeed(Vector3(0.0f,0.0f,0.0f), false, 0);

	Matrix34 matChest;
	pPed->GetBoneMatrix(matChest, BONETAG_SPINE3);
	Vector3 vecChestSpeed = pPed->GetLocalSpeed(Vector3(0.0f,0.0f,0.0f), false, 10);

	bool bScanForGrab = false;
	if(GetFlag(TARGET_AUTO_WHEN_STANDING) && GetFlag(BALANCE_STATUS_ACTIVE))
	{
		if((vecRootSpeed + vecChestSpeed).Mag2() > sfGrabStandingMinSpeed*sfGrabStandingMinSpeed)
			bScanForGrab = true;
	}
	if(GetFlag(TARGET_AUTO_WHEN_FALLING) && GetFlag(BALANCE_STATUS_FAILED|BALANCE_STATUS_FALLING))
	{
		if((vecRootSpeed + vecChestSpeed).Mag2() > sfGrabFallingMinSpeed*sfGrabFallingMinSpeed)
			bScanForGrab = true;
	}

	if(bScanForGrab && !GetFlag(TARGET_SPECIFIC))
	{
		static float TEST_FOR_GRAB_RADIUS = 0.6f;
		static float TEST_FOR_GRAB_FWD_OFFSET = 0.3f;
		static float TEST_FOR_GRAB_Z_OFFSET = 0.0f;

		Vector3 vecTestPos = matChest.d + Vector3(0.0f, 0.0f, TEST_FOR_GRAB_Z_OFFSET);
		vecTestPos += TEST_FOR_GRAB_FWD_OFFSET * matChest.b;

		int nIncludeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

#if __DEV
		if(ms_bDisplayDebug)
		{
			grcDebugDraw::Line(vecTestPos - 0.25f*matChest.a, vecTestPos + 0.25f*matChest.a, Color32(1.0f,0.0f,0.0f,1.0f));
			grcDebugDraw::Line(vecTestPos - 0.25f*matChest.b, vecTestPos + 0.25f*matChest.b, Color32(0.0f,1.0f,0.0f,1.0f));
			grcDebugDraw::Line(vecTestPos - 0.25f*matChest.c, vecTestPos + 0.25f*matChest.c, Color32(0.0f,0.0f,1.0f,1.0f));
		}
#endif

		phIntersection testIntersection;
		if(CPhysics::GetLevel()->TestSphere(vecTestPos, TEST_FOR_GRAB_RADIUS, &testIntersection, pPed->GetRagdollInst(), nIncludeFlags))
		{
			Vector3 vecGrabPos = RCC_VECTOR3(testIntersection.GetPosition());
			Vector3 vecGrabNorm = RCC_VECTOR3(testIntersection.GetNormal());

			CEntity* pGrabEnt = CPhysics::GetEntityFromInst(testIntersection.GetInstance());

			if(pGrabEnt->GetIsPhysical())
			{
				vecGrabPos = VEC3V_TO_VECTOR3(pGrabEnt->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vecGrabPos)));
				vecGrabNorm = VEC3V_TO_VECTOR3(pGrabEnt->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecGrabNorm)));

				if(pGrabEnt->GetIsTypeObject())
				{
					// ignore doors
					if(((CObject*)pGrabEnt)->IsADoor())
						pGrabEnt = NULL;
				}
				else if(pGrabEnt->GetIsTypeVehicle())
				{
					Vector3 vRelVel = static_cast<CVehicle*>(pGrabEnt)->GetRelativeVelocity(*pPed);
					// ignore fast moving vehicles
					if(vRelVel.Mag2() > square(TEST_FOR_GRAB_VEHICLE_VELOCITY))
						pGrabEnt = NULL;
				}
			}
			else
				pGrabEnt = NULL;

			// Catch invalid normals. Ideally we would like to trap bad normals with a stricter tolerance, but rage doesn't guarantee very accurate normals it seems.
			Assertf(vecGrabNorm.Mag() <= 1.1, "Grab vector normal too large: testIntersection.m_Component=%d, testIntersection.m_Normal=(%f,%f,%f) (mag=%f), testIntersection.m_PhysInst=0x%p, testIntersection.m_Position=(%f,%f,%f)", testIntersection.GetComponent(), VEC3V_ARGS(testIntersection.GetNormal()), Mag(testIntersection.GetNormal()).Getf(), testIntersection.GetInstance(), VEC3V_ARGS(testIntersection.GetPosition()));

			if(!pGrabEnt || (pGrabEnt->IsCollisionEnabled() && !((CPhysical*)pGrabEnt)->GetIsAttached()))
			{
				m_pGrabEnt = pGrabEnt;

				if(IsGreaterThanAll(Dot(pPed->GetTransform().GetA(), testIntersection.GetPosition() - pPed->GetTransform().GetPosition()), ScalarV(V_ZERO)))
				{
					// left
					m_vecGrab1 = vecGrabPos;
					m_vecNorm1 = vecGrabNorm;
					m_vecGrab2.Zero();
					m_vecNorm2.Zero();
				}
				else
				{
					// right
					m_vecGrab1.Zero();
					m_vecNorm1.Zero();
					m_vecGrab2 = vecGrabPos;
					m_vecNorm2 = vecGrabNorm;
				}

				StartBehaviour(pPed);
			}
		}
	}

	return false;
}
#if !__FINAL
void CGrabHelper::DrawDebug( void ) const
{
#if DEBUG_DRAW
	Color32 color = m_bHasSucceeded ? Color_green : Color_red;
	Vector3 vDebugSphere;
	Vector3 vDebugNormal;
	if(m_vecGrab1.IsNonZero())
	{
		vDebugSphere = m_vecGrab1;
		if( m_pGrabEnt )
		{				
			fragInst* pFragInst = m_pGrabEnt->GetFragInst();
			if (m_pGrabEnt->GetCurrentPhysicsInst() && pFragInst)
			{
				if (m_iBoundIndex>0)
				{
					phBoundComposite *carBound = pFragInst->GetCacheEntry()->GetBound();
					Matrix34 boundMat;
					boundMat.Dot(RCC_MATRIX34(carBound->GetCurrentMatrix(m_iBoundIndex)), RCC_MATRIX34(pFragInst->GetMatrix()));
					boundMat.Transform(vDebugSphere);
				}
				else
				{
					RCC_MATRIX34(pFragInst->GetMatrix()).Transform(vDebugSphere);
				}
			}		
		}
		grcDebugDraw::Sphere(vDebugSphere, 0.1f, color);
		if(m_vecNorm1.IsNonZero())
		{
			vDebugNormal = m_vecNorm1;
			if( m_pGrabEnt )
				vDebugNormal = VEC3V_TO_VECTOR3(m_pGrabEnt->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDebugNormal)));
			grcDebugDraw::Line(vDebugSphere, vDebugSphere + (vDebugNormal*0.5f), color);
		}
	}
	if(m_vecGrab2.IsNonZero())
	{
		vDebugSphere = m_vecGrab2;
		if( m_pGrabEnt )
			vDebugSphere = m_pGrabEnt->TransformIntoWorldSpace(vDebugSphere);
		grcDebugDraw::Sphere(vDebugSphere, 0.1f, color);
		if(m_vecNorm2.IsNonZero())
		{
			vDebugNormal = m_vecNorm2;
			if( m_pGrabEnt )
				vDebugNormal = VEC3V_TO_VECTOR3(m_pGrabEnt->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDebugNormal)));
			grcDebugDraw::Line(vDebugSphere, vDebugSphere + (vDebugNormal*0.5f), color);
		}
	}

	if(m_vecGrab3.IsNonZero())
	{
		vDebugSphere = m_vecGrab3;
		if( m_pGrabEnt )
			vDebugSphere = m_pGrabEnt->TransformIntoWorldSpace(vDebugSphere);
		grcDebugDraw::Sphere(vDebugSphere, 0.1f, color);
	}
	if(m_vecGrab4.IsNonZero())
	{
		vDebugSphere = m_vecGrab4;
		if( m_pGrabEnt )
			vDebugSphere = m_pGrabEnt->TransformIntoWorldSpace(vDebugSphere);
		grcDebugDraw::Sphere(vDebugSphere, 0.1f, color);
	}
#endif // DEBUG_DRAW
}
#endif // !__FINAL
