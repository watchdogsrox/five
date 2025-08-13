// Filename   :	TaskNMStumble.cpp
// Description:	GTA-side of the NM Stumble behavior

#if 0 // This code is currently out of use

//---- Include Files ---------------------------------------------------------------------------------------------------------------------------------
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "phbound/boundcomposite.h"
#include "physics/shapetest.h"
#include "physics/iterator.h"

// Framework headers
#include "grcore/debugdraw.h"
//#include "fwmaths\Angle.h"

// Game headers
#include "Peds/ped.h"
#include "Task/Physics/TaskNMStumble.h"
#include "Task/Physics/NmDebug.h"
#include "Weapons/Components/WeaponComponentClip.h"
#include "Weapons/Components/WeaponComponentScope.h"
#include "Weapons/Components/WeaponComponentSuppressor.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMStumble //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMStumble::CTaskNMStumble(u32 nMinTime, u32 nMaxTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime),
m_FramesSinceLastProbe(0)
{
	Assertf(0, "This task is out of commision.  Let an NM programmer know so we can switch this task to the new NM tuning system and re-open it for business.");

	for (int i=0; i<NUM_GRAB_POINTS; i++)
	{
		m_Hit[i] = false;
		m_Fixed[i] = false;
		m_HitLevelIndex[i] = -1;
		m_HitGenID[i] = 0;
		m_GrabPoints[i] = Vec3V(V_ZERO);
		m_GrabNormals[i] = Vec3V(V_X_AXIS_WZERO);
	}
#if !__FINAL
	m_strTaskName = "NMStumble";
#endif // !__FINAL
	SetInternalTaskType(CTaskTypes::TASK_NM_STUMBLE);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMStumble::~CTaskNMStumble()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Send Initial Messages
	HandleStumble(pPed);
	HandleGrab(pPed);
	HandleStayUpright(pPed);
	HandleConfigureBalance(pPed);
	HandleLeanRandom(pPed);
	HandleSetCharacterHealth(pPed);

	ControlBehaviour(pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Update grab points as necessary
	HandleGrab(pPed);

	CTaskNMBehaviour::ControlBehaviour(pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::HandleStumble(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Stumble
	ART::MessageParams msgStumble;
	//GetNmParameterSets()->m_StumbleParameterSets.m_parameters[CNmParameterSets::STUMBLE_DEFAULT].LoadParameters(msgStumble, true);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STUMBLE_MSG), &msgStumble);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::HandleGrab(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Probe the environment for grab points/normals
	ProbeEnvironment(pPed);

	// Grab
	ART::MessageParams msgGrab;
	//GetNmParameterSets()->m_GrabParameterSets.m_parameters[CNmParameterSets::GRAB_FOR_STUMBLE].LoadParameters(msgGrab, true);
	msgGrab.addBool(NMSTR_PARAM(NM_GRAB_USE_LEFT), m_Hit[LeftHigh] && m_Hit[LeftMid]);
	msgGrab.addBool(NMSTR_PARAM(NM_GRAB_USE_RIGHT), m_Hit[RightHigh] && m_Hit[RightMid]);
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS1_VEC3), m_GrabPoints[2].GetXf(), m_GrabPoints[2].GetYf(), m_GrabPoints[2].GetZf());
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS2_VEC3), m_GrabPoints[0].GetXf(), m_GrabPoints[0].GetYf(), m_GrabPoints[0].GetZf());
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS3_VEC3), m_GrabPoints[3].GetXf(), m_GrabPoints[3].GetYf(), m_GrabPoints[3].GetZf());
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_POS4_VEC3), m_GrabPoints[1].GetXf(), m_GrabPoints[1].GetYf(), m_GrabPoints[1].GetZf());
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_NORM1_VEC3), m_GrabNormals[2].GetXf(), m_GrabNormals[2].GetYf(), m_GrabNormals[2].GetZf());
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_NORM2_VEC3), m_GrabNormals[0].GetXf(), m_GrabNormals[0].GetYf(), m_GrabNormals[0].GetZf());
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_NORM3_VEC3), m_GrabNormals[3].GetXf(), m_GrabNormals[3].GetYf(), m_GrabNormals[3].GetZf());
	msgGrab.addVector3(NMSTR_PARAM(NM_GRAB_NORM4_VEC3), m_GrabNormals[1].GetXf(), m_GrabNormals[1].GetYf(), m_GrabNormals[1].GetZf());
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_GRAB_MSG), &msgGrab);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::HandleStayUpright(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// StayUpright
	ART::MessageParams msgStayUpright;
	//GetNmParameterSets()->m_StayUprightParameterSets.m_parameters[CNmParameterSets::STAYUPRIGHT_FOR_STUMBLE].LoadParameters(msgStayUpright, true);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STAYUPRIGHT_MSG), &msgStayUpright);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::HandleConfigureBalance(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// ConfigureBalance
	ART::MessageParams msgConfigureBalance;
	//GetNmParameterSets()->m_ConfigureBalanceParameterSets.m_parameters[CNmParameterSets::CONFIGURE_BALANCE_FOR_STUMBLE].LoadParameters(msgConfigureBalance, true);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CONFIGURE_BALANCE_MSG), &msgConfigureBalance);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::HandleLeanRandom(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// LeanRandom
	ART::MessageParams msgLeanRandom;
	//GetNmParameterSets()->m_LeanRandomParameterSets.m_parameters[CNmParameterSets::LEAN_RANDOM_FOR_STUMBLE].LoadParameters(msgLeanRandom, true);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_LEAN_RANDOM_MSG), &msgLeanRandom);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMStumble::HandleSetCharacterHealth(CPed* /*pPed*/)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//ART::MessageParams msg;
	//GetNmParameterSets()->m_SetFallingReactionParameterSets.m_parameters[CNmParameterSets::SET_FALLING_REACTION_HEALTHY].LoadParameters(msg, true, true, pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMStumble::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	static float fallingSpeed = -4.0f;
	if (pPed->GetVelocity().z < fallingSpeed)
	{
		m_bHasFailed = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
	}

	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_VEL_CHECK|FLAG_QUIT_IN_WATER);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMStumble::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedNMStumbleInfo(); 
}


CTaskFSMClone *CClonedNMStumbleInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMStumble(4000, 30000);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMStumble::ProbeEnvironment(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	bool bNewDataObtained = false;

	// Get orientation data for the Ped
	phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
	Matrix34 pelvisMat;
	pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The pelvis
	Vec3V dirFacing = -(RCC_MAT34V(pelvisMat).GetCol2());  
	dirFacing.SetZ(ScalarV(V_ZERO));
	dirFacing = NormalizeSafe(dirFacing, Vec3V(V_X_AXIS_WZERO));
	Vec3V dirSide = RCC_MAT34V(pelvisMat).GetCol1();  
	dirSide.SetZ(ScalarV(V_ZERO));
	dirSide = NormalizeSafe(dirSide, Vec3V(V_Z_AXIS_WZERO));
	Matrix34 footMat = RCC_MATRIX34(bound->GetCurrentMatrix(3));
	footMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(3)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The right ankle
	Mat34V footMatV = RCC_MAT34V(footMat);
	float footHeight = footMatV.GetCol3().GetZf(); 
	const Vec3V pedPositionV = pPed->GetTransform().GetPosition();	//	RCC_VEC3V(pedPosition);
	Vector3 pedPosition = RCC_VECTOR3(pedPositionV);

	// capsule start and end locations
	static float highHeight = 1.25f;
	static float mediumHeight = 0.65f;
	static float fForward = 1.0f;
	static float fStartForward = 0.35f;
	float fSide = (highHeight-mediumHeight) / 2.0f;
	atRangeArray<Vec3V, NUM_GRAB_POINTS> capStartPositions;
	atRangeArray<Vec3V, NUM_GRAB_POINTS> capEndPositions;

	capStartPositions[LeftHigh] = AddScaled(AddScaled(pedPositionV, dirSide, ScalarV(fSide)), dirFacing, ScalarV(fStartForward));
	capStartPositions[LeftHigh].SetZf(footHeight + highHeight);
	capEndPositions[LeftHigh] = AddScaled(capStartPositions[LeftHigh], dirFacing, ScalarV(fForward));
	capStartPositions[RightHigh] = AddScaled(AddScaled(pedPositionV, dirSide, ScalarV(-fSide)), dirFacing, ScalarV(fStartForward));
	capStartPositions[RightHigh].SetZf(footHeight + highHeight);
	capEndPositions[RightHigh] = AddScaled(capStartPositions[RightHigh], dirFacing, ScalarV(fForward));

	capStartPositions[LeftMid] = AddScaled(AddScaled(pedPositionV, dirSide, ScalarV(fSide)), dirFacing, ScalarV(fStartForward));
	capStartPositions[LeftMid].SetZf(footHeight + mediumHeight);
	capEndPositions[LeftMid] = AddScaled(capStartPositions[LeftMid], dirFacing, ScalarV(fForward));
	capStartPositions[RightMid] = AddScaled(AddScaled(pedPositionV, dirSide, ScalarV(-fSide)), dirFacing, ScalarV(fStartForward));
	capStartPositions[RightMid].SetZf(footHeight + mediumHeight);
	capEndPositions[RightMid] = AddScaled(capStartPositions[RightMid], dirFacing, ScalarV(fForward));

	// Probe if we don't already have hit data
	for (int i=0; i<NUM_GRAB_POINTS; i++)
	{
		// The probe frequency for dynamic objects
		static int probeFrequency = 10;

		// Periodically re-probe dynamic insts
		if (m_Hit[i] && !m_Fixed[i] && ++m_FramesSinceLastProbe >= probeFrequency)
		{
			m_Hit[i] = false;  
			m_FramesSinceLastProbe = 0;
			if (PHLEVEL->IsLevelIndexGenerationIDCurrent(m_HitLevelIndex[i], m_HitGenID[i]))
			{
				const phInst *pHitInst = PHLEVEL->GetInstance(m_HitLevelIndex[i]);
				if (pHitInst)
				{
					phIntersection intersect;
					phShapeTest<phShapeSweptSphere> shapeTest;
					phSegment seg(RCC_VECTOR3(capStartPositions[i]), RCC_VECTOR3(capEndPositions[i]));
					shapeTest.InitSweptSphere(seg, fSide, &intersect);
					shapeTest.SetSaveCulledPolygons(false);
					phPolygon::Index culledPolyIndexList[DEFAULT_MAX_CULLED_POLYS];
					shapeTest.SetArrays(culledPolyIndexList, DEFAULT_MAX_CULLED_POLYS);
					if (shapeTest.TestOneObject(*pHitInst))
					{
						m_GrabPoints[i] = intersect.GetPosition();
						m_GrabNormals[i] = intersect.GetNormal();
						m_Hit[i] = true;
						m_Fixed[i] = PHLEVEL->IsFixed(m_HitLevelIndex[i]);
						m_FramesSinceLastProbe = 0;
					}
				}
			}
		}

		if (!m_Hit[i])
		{
			// Iterate the insts within a radius of the player's position
			phIntersection intersect;
			phShapeTest<phShapeSweptSphere> shapeTest;
			phSegment seg(RCC_VECTOR3(capStartPositions[i]), RCC_VECTOR3(capEndPositions[i]));
			shapeTest.InitSweptSphere(seg, fSide, &intersect);
			shapeTest.SetSaveCulledPolygons(false);
			phPolygon::Index culledPolyIndexList[DEFAULT_MAX_CULLED_POLYS];
			shapeTest.SetArrays(culledPolyIndexList, DEFAULT_MAX_CULLED_POLYS);

#if ENABLE_PHYSICS_LOCK
			phIterator Iterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
			phIterator Iterator;
#endif	// ENABLE_PHYSICS_LOCK
			Iterator.InitCull_Capsule(capStartPositions[i], dirFacing, ScalarV(fForward), ScalarV(fSide));
			for (u16 ObjectLevelIndex = Iterator.GetFirstLevelIndex(PHLEVEL);
				 !m_Hit[i] && ObjectLevelIndex != phInst::INVALID_INDEX;
				 ObjectLevelIndex = Iterator.GetNextLevelIndex(PHLEVEL))
			{
				const phInst *pHitInst = PHLEVEL->GetInstance(ObjectLevelIndex);
				if (IsValidHit(pHitInst, pPed) && shapeTest.TestOneObject(*pHitInst))
				{
					// Now do a detailed sphere test against that one inst
					//	int classType = pHitInst->GetClassType();
					//	if(classType == PH_INST_FRAG_PED)

					m_GrabPoints[i] = intersect.GetPosition();
					m_GrabNormals[i] = intersect.GetNormal();
					m_Hit[i] = true;
					m_Fixed[i] = PHLEVEL->IsFixed(ObjectLevelIndex);
					m_HitLevelIndex[i] = ObjectLevelIndex;
					m_HitGenID[i] = PHLEVEL->GetGenerationID(ObjectLevelIndex);
					m_FramesSinceLastProbe = 0;
				}
			}
		}
	}

#if DEBUG_DRAW
	if(CNmDebug::ms_bDrawStumbleEnvironmentDetection)
	{
		// The search area
		for (int i=0; i<NUM_GRAB_POINTS; i++)
			grcDebugDraw::Capsule(capStartPositions[i], capEndPositions[i], fSide, i%2 ? Color_red : Color_blue, false);

		// Facing direction
		Vec3V endLoc = AddScaled(pedPositionV, dirFacing, ScalarV(0.8f));
		grcDebugDraw::Line(pedPosition, RCC_VECTOR3(endLoc), Color_DarkOrchid);

		// Side direction
		endLoc = AddScaled(pedPositionV, dirSide, ScalarV(0.8f));
		grcDebugDraw::Line(pedPosition, RCC_VECTOR3(endLoc), Color_firebrick);

		for (int i=0; i<NUM_GRAB_POINTS; i++)
		{
			if (m_Hit[i])
			{
				const Color32 color = i%2 ? Color_red : Color_blue;

				// The grab point
				grcDebugDraw::Sphere(RCC_VECTOR3(m_GrabPoints[i]), 0.05f, color, true);

				// The grab normal
				Vec3V endLoc = AddScaled(m_GrabPoints[i],m_GrabNormals[i], ScalarV(0.15f));
				grcDebugDraw::Line(RCC_VECTOR3(m_GrabPoints[i]), RCC_VECTOR3(endLoc), color);
			}
		}
	}
#endif // DEBUG_DRAW

	return bNewDataObtained;
}

bool CTaskNMStumble::IsValidHit(const phInst *pHitInst, const CPed *pPed)
{
	if (!pHitInst)
		return false;

	// Ignore the actor's inst
	if (pHitInst == pPed->GetRagdollInst() || pHitInst == pPed->GetAnimatedInst())
		return false;

	// Also ignore a held weapon 
	if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon() &&
		pPed->GetWeaponManager()->GetEquippedWeapon()->GetEntity() &&
		pPed->GetWeaponManager()->GetEquippedWeapon()->GetEntity()->GetCurrentPhysicsInst())
	{
		if (pHitInst == pPed->GetWeaponManager()->GetEquippedWeapon()->GetEntity()->GetCurrentPhysicsInst())
			return false;

		// ...and the held weapon's components
		const CWeapon::Components& weaponComponents = pPed->GetWeaponManager()->GetEquippedWeapon()->GetComponents();
		for(s32 i = 0; i < weaponComponents.GetCount(); i++)
		{
			if (weaponComponents[i]->GetDrawable() && weaponComponents[i]->GetDrawable()->GetCurrentPhysicsInst())
			{
				if (pHitInst == weaponComponents[i]->GetDrawable()->GetCurrentPhysicsInst())
					return false;
			}
		}
	}

	return true;
}

#endif