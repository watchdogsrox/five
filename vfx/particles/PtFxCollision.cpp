///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxCollision.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	05 Mar 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "PtFxCollision.h"

// rage
#include "physics/iterator.h"
#include "phbound/boundbvh.h"
#include "rmptfx/ptxeffectrule.h"
#include "vector/geometry.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"

// game
#include "Peds/Ped.h"
#include "Physics/GtaArchetype.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/Entity.h"
#include "Scene/World/GameWorld.h"
#include "Vehicles/Planes.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  PARAMS
///////////////////////////////////////////////////////////////////////////////

XPARAM(mapMoverOnlyInMultiplayer);


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define PTFXHELPER_UNDERWATER_CULL_PLANE_OFFSET		(-0.25f)


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxColnEntity
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void				CPtFxColnBase::Init				()
{
	m_isActive = false;
	m_registeredThisFrame = false;
	m_needsProcessed = false;
	m_onlyProcessBVH = true;
	m_vPos = Vec3V(V_ZERO);
	m_colnType = PTFX_COLN_TYPE_NONE;
	m_probeDist = 0.0f;
	m_range = 0.0f;
	m_colnSet.Init(MAX_COLN_POLYS_BOUND);
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void				CPtFxColnBase::Update					()
{
	if (m_isActive)
	{
		if (m_registeredThisFrame)
		{
			if (m_needsProcessed)
			{
				Process();
				m_needsProcessed = false;
			}

			// make inactive so we know if we get registered next frame
			m_registeredThisFrame = false;
		}
		else
		{
			m_isActive = false;
			m_needsProcessed = false;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void				CPtFxColnBase::RenderDebug				()
{	
	if (m_isActive)
	{
		for (s32 i=0; i<m_colnSet.GetNumColnPolys(); i++)
		{
			ptxCollisionPoly& colnPoly = m_colnSet.GetColnPoly(i);

			grcDebugDraw::Poly(colnPoly.GetVertex(0), colnPoly.GetVertex(1), colnPoly.GetVertex(2), m_renderCol);

			if (colnPoly.GetNumVerts()==4)
			{
				grcDebugDraw::Poly(colnPoly.GetVertex(0), colnPoly.GetVertex(2), colnPoly.GetVertex(3), m_renderCol);
			}
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  Process
///////////////////////////////////////////////////////////////////////////////

void				CPtFxColnBound::Process			()
{
	m_colnSet.Clear();

	if (m_colnType==PTFX_COLN_TYPE_GROUND_BOUND)
	{
		// ground bound
		CPtFxCollision::ProcessGroundBounds(&m_colnSet, m_vPos, m_probeDist, m_range, m_onlyProcessBVH, true);
	}
	else if (m_colnType==PTFX_COLN_TYPE_GROUND_BOUNDS)
	{
		// ground bounds
		CPtFxCollision::ProcessGroundBounds(&m_colnSet, m_vPos, m_probeDist, m_range, m_onlyProcessBVH, false);
	}
	else if (m_colnType==PTFX_COLN_TYPE_NEARBY_BOUND)
	{
		// nearby bounds
		CPtFxCollision::ProcessNearbyBounds(&m_colnSet, m_vPos, m_range, m_onlyProcessBVH);
	}
	else if (m_colnType==PTFX_COLN_TYPE_VEHICLE_PLANE)
	{
		// vehicle upwards
		CPtFxCollision::ProcessVehiclePlane(&m_colnSet, m_vPos, m_probeDist);
	}
	else
	{
		ptfxAssertf(0, "trying to process an invalid collision bound");
	}

#if __BANK
	s32 colR = g_DrawRand.GetRanged(0, 255);
	s32 colG = g_DrawRand.GetRanged(0, 255);
	s32 colB = g_DrawRand.GetRanged(0, 255);

	m_renderCol.Set(colR, colG, colB, 255);
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  Process
///////////////////////////////////////////////////////////////////////////////

void				CPtFxColnPlane::Process			()
{
	m_colnSet.Clear();

	if (m_colnType==PTFX_COLN_TYPE_GROUND_PLANE)
	{
		// ground plane
		CPtFxCollision::ProcessGroundPlane(&m_colnSet, m_vPos, m_probeDist, m_range);
	}
	else if (m_colnType==PTFX_COLN_TYPE_UNDERWATER_PLANE)
	{
		// water plane
		CPtFxCollision::ProcessUnderWaterPlane(&m_colnSet, m_vPos, m_range);
	}	
	else
	{
		ptfxAssertf(0, "trying to process an invalid collision plane");
	}

#if __BANK
	s32 colR = g_DrawRand.GetRanged(0, 255);
	s32 colG = g_DrawRand.GetRanged(0, 255);
	s32 colB = g_DrawRand.GetRanged(0, 255);

	m_renderCol.Set(colR, colG, colB, 255);
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxCollision
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::Init					()
{	
	for (s32 i=0; i<MAX_COLN_PLANES; i++)
	{
		m_colnPlanes[i].Init();
	}

	for (s32 i=0; i<MAX_COLN_BOUNDS; i++)
	{
		m_colnBounds[i].Init();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::Shutdown				()
{	
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::Update					()
{	
	for (s32 i=0; i<MAX_COLN_PLANES; i++)
	{
		m_colnPlanes[i].Update();
	}

	for (s32 i=0; i<MAX_COLN_BOUNDS; i++)
	{
		m_colnBounds[i].Update();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void				CPtFxCollision::RenderDebug				()
{	
	for (s32 i=0; i<MAX_COLN_PLANES; i++)
	{
		m_colnPlanes[i].RenderDebug();
	}

	for (s32 i=0; i<MAX_COLN_BOUNDS; i++)
	{
		m_colnBounds[i].RenderDebug();
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  Register
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::Register				(ptxEffectInst* pFxInst)
{	
	ptxEffectRule* pEffectRule = pFxInst->GetEffectRule();
	if (pEffectRule)
	{
		// get the collision information
		PtFxColnType_e colnType = static_cast<PtFxColnType_e>(pEffectRule->GetColnType());
		float colnRange = pEffectRule->GetColnRange();

		if (colnType==PTFX_COLN_TYPE_NONE || colnRange==0.0f)
		{
			return;
		}

		float probeDist = pEffectRule->GetColnProbeDist();
		bool useEntity = pEffectRule->GetShareEntityColn() != 0;
		bool onlyProcessBVH = pEffectRule->GetOnlyUseBVHColn() != 0;

		CEntity* pEntity = NULL;
		if (useEntity)
		{
			pEntity = static_cast<CEntity*>(ptfxAttach::GetAttachEntity(pFxInst));
		}

		// get the position to do the collision checks from
		Vec3V vColnPos;
		if (pEntity)
		{
			vColnPos = pEntity->GetTransform().GetPosition();
		}
		else
		{
			vColnPos = pFxInst->GetCurrPos();
		}

		if (colnType==PTFX_COLN_TYPE_GROUND_PLANE || colnType==PTFX_COLN_TYPE_UNDERWATER_PLANE)
		{
			RegisterColnPlane(pFxInst, vColnPos, colnType, probeDist, colnRange);
		}
		else
		{
			RegisterColnBound(pFxInst, vColnPos, colnType, probeDist, colnRange, onlyProcessBVH);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterUserPlane
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::RegisterUserPlane		(ptxEffectInst* pFxInst, Vec3V_In vPos, Vec3V_In vNormal, float range, u8 mtlId)
{
	// make sure the current collision set is cleared
	pFxInst->ClearCollisionSet();

	ptxCollisionSet colnSet;

	ProcessPlane(&colnSet, vPos, vNormal, range, mtlId);

	if (colnSet.GetNumColnPolys()>0)
	{
		pFxInst->CopyCollisionSet(&colnSet);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterColnPlane
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::RegisterColnPlane		(ptxEffectInst* pFxInst, Vec3V_In vColnPos, PtFxColnType_e colnType, float probeDist, float colnRange)
{	
	// make sure the current collision set is cleared
	pFxInst->ClearCollisionSet();

	// try to find an existing collision entity
	CPtFxColnPlane* pColnPlane = FindColnPlane(vColnPos, colnType);
	if (pColnPlane==NULL)
	{
		// none exists get the next free one instead
		for (s32 i=0; i<MAX_COLN_PLANES; i++)
		{
			if (m_colnPlanes[i].m_isActive==false)
			{
				// use this one - initialise it
				pColnPlane = &m_colnPlanes[i];
				pColnPlane->m_isActive = true;
				pColnPlane->m_registeredThisFrame = true;
				pColnPlane->m_needsProcessed = true;
				pColnPlane->m_onlyProcessBVH = true;
				pColnPlane->m_vPos = vColnPos;
				pColnPlane->m_colnType = colnType;
				pColnPlane->m_probeDist = probeDist;
				pColnPlane->m_range = colnRange;

				// process the collision
				pColnPlane->Process();
				pColnPlane->m_needsProcessed = false;

				break;
			}
		}
	}

	if (pColnPlane)
	{
		pColnPlane->m_registeredThisFrame = true;

		if (colnRange>pColnPlane->m_range)
		{
			pColnPlane->m_range = colnRange;
			pColnPlane->m_needsProcessed = true;
		}

		if (probeDist>pColnPlane->m_probeDist)
		{
			pColnPlane->m_probeDist = probeDist;
			pColnPlane->m_needsProcessed = true;
		}

		if (pColnPlane->m_colnSet.GetNumColnPolys())
		{
			pFxInst->SetCollisionSet(&pColnPlane->m_colnSet);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterColnBound
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::RegisterColnBound		(ptxEffectInst* pFxInst, Vec3V_In vColnPos, PtFxColnType_e colnType, float probeDist, float colnRange, bool onlyProcessBVH)
{
	// make sure the current collision set is cleared
	pFxInst->ClearCollisionSet();

	// try to find an existing collision entity
	CPtFxColnBound* pColnBound = FindColnBound(vColnPos, colnType, onlyProcessBVH);
	if (pColnBound==NULL)
	{
		// none exists get the next free one instead
		for (s32 i=0; i<MAX_COLN_BOUNDS; i++)
		{
			if (m_colnBounds[i].m_isActive==false)
			{
				// use this one - initialise it
				pColnBound = &m_colnBounds[i];
				pColnBound->m_isActive = true;
				pColnBound->m_registeredThisFrame = true;
				pColnBound->m_needsProcessed = true;
				pColnBound->m_onlyProcessBVH = onlyProcessBVH;
				pColnBound->m_vPos = vColnPos;
				pColnBound->m_colnType = colnType;
				pColnBound->m_probeDist = probeDist;
				pColnBound->m_range = colnRange;

				// process the collision
				pColnBound->Process();
				pColnBound->m_needsProcessed = false;

				break;
			}
		}
	}

	if (pColnBound)
	{
		pColnBound->m_registeredThisFrame = true;

		// update the range or probe if they are longer
		if (colnRange>pColnBound->m_range)
		{
			pColnBound->m_range = colnRange;
			pColnBound->m_needsProcessed = true;
		}

		if (probeDist>pColnBound->m_probeDist)
		{
			pColnBound->m_probeDist = probeDist;
			pColnBound->m_needsProcessed = true;
		}

		// add the collision polys to the effect instance
		if (colnType==PTFX_COLN_TYPE_VEHICLE_PLANE)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("collision", 0x86618E11), 0.0f);
		}

		if (pColnBound->m_colnSet.GetNumColnPolys()>0)
		{
			pFxInst->SetCollisionSet(&pColnBound->m_colnSet);

			if (colnType==PTFX_COLN_TYPE_VEHICLE_PLANE)
			{
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("collision", 0x86618E11), 1.0f);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  FindColnPlane
///////////////////////////////////////////////////////////////////////////////

CPtFxColnPlane*	CPtFxCollision::FindColnPlane			(Vec3V_In vPos, PtFxColnType_e colnType)
{	
	// search for an existing collision entity on this entity
	for (s32 i=0; i<MAX_COLN_PLANES; i++)
	{
		if (m_colnPlanes[i].m_isActive &&
			m_colnPlanes[i].m_colnType==colnType)
		{
			float distSqr = DistSquared(vPos, m_colnPlanes[i].m_vPos).Getf();
			float distThresh = m_colnPlanes[i].m_range*0.5f;
			if (distSqr<distThresh*distThresh)
			{
				// vehicle checks want to re-probe every frame
				if (colnType==PTFX_COLN_TYPE_VEHICLE_PLANE && !m_colnBounds[i].m_registeredThisFrame)
				{
					continue;
				}
				else
				{
					return &m_colnPlanes[i];
				}
			}
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  FindColnBound
///////////////////////////////////////////////////////////////////////////////

CPtFxColnBound*	CPtFxCollision::FindColnBound			(Vec3V_In vPos, PtFxColnType_e colnType, bool onlyProcessBVH)
{	
	// search for an existing collision entity on this entity
	for (s32 i=0; i<MAX_COLN_BOUNDS; i++)
	{
		// check the type is correct
		if (m_colnBounds[i].m_isActive &&
			m_colnBounds[i].m_colnType==colnType)
		{
			// check that it is in range
			float distSqr = DistSquared(vPos, m_colnBounds[i].m_vPos).Getf();
			float distThresh = m_colnBounds[i].m_range*0.5f;
			if (distSqr<distThresh*distThresh)
			{
				// non-bvh checks want to re-probe every frame
				if (!onlyProcessBVH && !m_colnBounds[i].m_registeredThisFrame)
				{
					continue;
				}
				else
				{
					return &m_colnBounds[i];
				}
			}
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessPlane
///////////////////////////////////////////////////////////////////////////////

void			CPtFxCollision::ProcessPlane				(ptxCollisionSet* pColnSet, Vec3V_In vPos, Vec3V_In vNormal, float range, u8 mtlId)
{	
	Mat34V fxMat;
	CVfxHelper::CreateMatFromVecZ(fxMat, vPos, vNormal);

	ScalarV vRange = ScalarVFromF32(range);
	Vec3V vA = fxMat.GetCol0()*vRange;
	Vec3V vB = fxMat.GetCol1()*vRange;

	Vec3V vVerts[4];
	vVerts[0] = vPos - vA + vB;
	vVerts[1] = vPos - vA - vB;
	vVerts[2] = vPos + vA - vB;
	vVerts[3] = vPos + vA + vB;

	pColnSet->AddQuad(&vVerts[0], vNormal, mtlId);
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessUnderWaterPlane
///////////////////////////////////////////////////////////////////////////////

void			CPtFxCollision::ProcessUnderWaterPlane		(ptxCollisionSet* pColnSet, Vec3V_In vPos, float range)
{	
	Vec3V vNormal = -Vec3V(V_Z_AXIS_WZERO);

	float waterZ;
	CVfxHelper::GetWaterZ(vPos, waterZ);

	Vec3V vWaterPos = vPos;
	vWaterPos.SetZf(waterZ + PTFXHELPER_UNDERWATER_CULL_PLANE_OFFSET);

	ProcessPlane(pColnSet, vWaterPos, vNormal, range, (u8)0);
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessGroundPlane
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::ProcessGroundPlane		(ptxCollisionSet* pColnSet, Vec3V_In vPos, float probeDist, float range)
{
	// scan for geometry to interact with
	Vec3V vStartPos = vPos;
	vStartPos.SetZ(vPos.GetZ()+ScalarV(0.05f));
	Vec3V vEndPos(vStartPos.GetXY(), vPos.GetZ()-ScalarV(probeDist));
	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
	//probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	probeDesc.SetExcludeEntity(NULL);
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		phMaterialMgr::Id packedMtlId = probeResults[0].GetHitMaterialId();
		phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(packedMtlId);
		ProcessPlane(pColnSet, RCC_VEC3V(probeResults[0].GetHitPosition()), RCC_VEC3V(probeResults[0].GetHitNormal()), range, (u8)mtlId);
	}

#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbes())
	{
		grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 1.0f, 0.0f, 1.0f), g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
		grcDebugDraw::Cross(RCC_VEC3V(probeResults[0].GetHitPosition()), 0.25f, Color32(0.0f, 1.0f, 0.0f, 1.0f), g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessGroundBounds
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::ProcessGroundBounds		(ptxCollisionSet* pColnSet, Vec3V_In vPos, float probeDist, float range, bool onlyProcessBVH, bool useColnEntityOnly)
{
	// Scan for geometry to interact with.
	Vec3V vStartPos = vPos;
	vStartPos.SetZ(vPos.GetZ()+ScalarV(0.05f));
	Vec3V vEndPos(vStartPos.GetXY(), vPos.GetZ()-ScalarV(probeDist));
	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
	probeDesc.SetResultsStructure(&probeResults);
	//probeDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);
	//probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	probeDesc.SetExcludeEntity(NULL);
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		if (useColnEntityOnly)
		{
			AddInstBoundsToColnSet(pColnSet, MAX_COLN_POLYS_BOUND, probeResults[0].GetHitInst(), RCC_VEC3V(probeResults[0].GetHitPosition()), range, onlyProcessBVH);
		}
		else
		{
			ProcessNearbyBounds(pColnSet, RCC_VEC3V(probeResults[0].GetHitPosition()), range, onlyProcessBVH);
		}
	}

#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbes())
	{
		grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 1.0f, 0.0f, 1.0f), g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
		grcDebugDraw::Cross(RCC_VEC3V(probeResults[0].GetHitPosition()), 0.25f, Color32(0.0f, 1.0f, 0.0f, 1.0f), g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessNearbyBounds
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::ProcessNearbyBounds		(ptxCollisionSet* pColnSet, Vec3V_In vPos, float range, bool onlyProcessBVH)
{
	// cull the level with a sphere test to get considered insts to process
#if ENABLE_PHYSICS_LOCK
	phIterator it(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else
	phIterator it;
#endif
	it.InitCull_Sphere(RCC_VECTOR3(vPos), range);

	//Create an array to hold poly collision info on the stack
	it.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
	it.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	u16 curObjectLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(it);
	while (curObjectLevelIndex != phInst::INVALID_INDEX)
	{
		phInst& culledInst = *CPhysics::GetLevel()->GetInstance(curObjectLevelIndex);

		AddInstBoundsToColnSet(pColnSet, MAX_COLN_POLYS_BOUND, &culledInst, vPos, range, onlyProcessBVH);

		curObjectLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(it);
	}

#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbes())
	{
		grcDebugDraw::Sphere(vPos, range, Color32(0.0f, 0.0f, 1.0f, 1.0f), false, g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVehiclePlane
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::ProcessVehiclePlane	(ptxCollisionSet* pColnSet, Vec3V_In vPos, float probeDist)
{	
	Vec3V vForward(V_Z_AXIS_WZERO);

	// scan upwards for vehicle to interact with
	Vec3V vStartPos = vPos - (vForward*ScalarVFromF32(0.05f));
	Vec3V vEndPos = vStartPos + (vForward*ScalarVFromF32(probeDist));

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeDesc.SetIsDirected(false);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		AddVehBoundsToColnSet(pColnSet, probeResults[0].GetHitInst());
	}

#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbes())
	{
		grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 1.0f, 0.0f, 1.0f), g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
		grcDebugDraw::Cross(RCC_VEC3V(probeResults[0].GetHitPosition()), 0.25f, Color32(0.0f, 1.0f, 0.0f, 1.0f), g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  AddVehBoundsToFxInst
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::AddVehBoundsToColnSet	(ptxCollisionSet* pColnSet, phInst* pInst)
{	
	ptfxAssertf(pInst, "called with NULL effect instance");

	CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
	if (pEntity && pEntity->GetIsTypeVehicle())
	{
		// get this entity's bounding box
		Vec3V_ConstRef bbMin = RCC_VEC3V(pEntity->GetBoundingBoxMin());
		Vec3V_ConstRef bbMax = RCC_VEC3V(pEntity->GetBoundingBoxMax());

		// add the bound to the effect instance
		Vec3V vVerts[4];
		vVerts[0] = Vec3V(bbMin.GetXY(), ScalarV(V_ZERO));
		vVerts[1] = Vec3V(bbMin.GetX(), bbMax.GetY(), ScalarV(V_ZERO));
		vVerts[2] = Vec3V(bbMax.GetXY(), ScalarV(V_ZERO));
		vVerts[3] = Vec3V(bbMax.GetX(), bbMin.GetY(), ScalarV(V_ZERO));

		for (s32 i=0; i<4; i++)
		{
			vVerts[i] = VECTOR3_TO_VEC3V(pEntity->TransformIntoWorldSpace(RCC_VECTOR3(vVerts[i])));
		}

		pColnSet->AddQuad(&vVerts[0], -(pEntity->GetTransform().GetC()), 0);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  AddInstBoundsToColnSet
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::AddInstBoundsToColnSet	(ptxCollisionSet* pColnSet, s32 maxPolys, phInst* pInst, Vec3V_In vPos, float range, bool onlyProcessBVH)
{	
	if (ptfxVerifyf(pInst, "called with NULL effect instance"))
	{
		if (ptfxVerifyf(pInst->GetArchetype(), "effect instance doesn't have a valid archetype"))
		{
			if (ptfxVerifyf(pInst->GetArchetype()->GetBound(), "effect instance doesn't have a valid bound"))
			{
				phBound* pBound = pInst->GetArchetype()->GetBound();
				if (pBound->GetType()==phBound::COMPOSITE)
				{
					const CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
					if (pEntity)
					{
						if (pEntity->GetIsTypeVehicle() && !onlyProcessBVH)
						{
							const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);

							int boneIndices[3] = {-1, -1, -1};
							
							// use the low lod chassis bone if one exists - otherwise just use the chassis bone
							boneIndices[0] = pVehicle->GetBoneIndex(VEH_CHASSIS_LOWLOD);
							if (boneIndices[0]==-1)
							{
								boneIndices[0] = pVehicle->GetBoneIndex(VEH_CHASSIS);
							}

							// for planes also add the wings
							if (pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
							{
								const CPlane* pPlane = static_cast<const CPlane*>(pVehicle);

								boneIndices[1] = pPlane->GetBoneIndex(PLANE_WING_L);
								boneIndices[2] = pPlane->GetBoneIndex(PLANE_WING_R);
							}

							// loop through any valid bone indices adding the collision
							for (int j=0; j<3; j++)
							{
								// check that we have a valid bone
								if (boneIndices[j]!=-1)
								{
									// get the group index of this bone
									int groupIndex = pVehicle->GetVehicleFragInst()->GetGroupFromBoneIndex(boneIndices[j]);
									if (groupIndex > -1)
									{
										phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pBound);

										// go through the children in this group
										const fragPhysicsLOD* physicsLOD = pVehicle->GetVehicleFragInst()->GetTypePhysics();
										fragTypeGroup* pGroup = physicsLOD->GetGroup(groupIndex);
										int firstChildIndex = pGroup->GetChildFragmentIndex();
										for (int i=0; i<pGroup->GetNumChildren(); i++)
										{
											int childIndex = firstChildIndex+i;

											Mat34V newBoundWldMtx;
											Transform(newBoundWldMtx, pInst->GetMatrix(), pBoundComp->GetCurrentMatrix(childIndex));
											AddBoundsToColnSet(pColnSet, maxPolys, pInst, pBoundComp->GetBound(childIndex), newBoundWldMtx, vPos, range, onlyProcessBVH);
										}
									}
								}
							}
						}
						else
						{
							u32 typeFlags = 0;
							u32 levelIndex = pInst->GetLevelIndex();
							if (levelIndex==phInst::INVALID_INDEX)
							{
								typeFlags = pInst->GetArchetype()->GetTypeFlags();
							}
							else
							{
								typeFlags = CPhysics::GetLevel()->GetInstanceTypeFlags(levelIndex);
							}

							phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pBound);
							for (int i=0; i<pBoundComp->GetNumBounds(); i++)
							{
								if (pBoundComp->GetTypeAndIncludeFlags())
								{
									typeFlags = pBoundComp->GetTypeFlags(i);
								}

								if ((typeFlags & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) ||
									(typeFlags & ArchetypeFlags::GTA_OBJECT_TYPE))
								{
									Mat34V newBoundWldMtx;
									Transform(newBoundWldMtx, pInst->GetMatrix(), pBoundComp->GetCurrentMatrix(i));
									AddBoundsToColnSet(pColnSet, maxPolys, pInst, pBoundComp->GetBound(i), newBoundWldMtx, vPos, range, onlyProcessBVH);
								}
							}
						}
					}
				}
				else if (pBound->GetType()==phBound::BVH)
				{
					AddBoundsToColnSet(pColnSet, maxPolys, pInst, pBound, pInst->GetMatrix(), vPos, range, onlyProcessBVH);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  AddBoundsToColnSet
///////////////////////////////////////////////////////////////////////////////

void				CPtFxCollision::AddBoundsToColnSet	(ptxCollisionSet* pColnSet, s32 maxPolys, phInst* pInst, phBound* pBound, Mat34V_In boundMtx, Vec3V_In vPos, float range, bool onlyProcessBVH)
{	
	// make sure this gets referenced
	(void)pInst;

	// get how much space we have left in the collision set
	s32 numSpareColnPolys = maxPolys-pColnSet->GetNumColnPolys();
	if (numSpareColnPolys==0)
	{
		// return early if no space available
		return;
	}

	if (pBound)
	{
		if (pBound->GetType() == phBound::GEOMETRY)
		{
			if (!onlyProcessBVH)
			{
				// go through this bound's polys
				const phBoundPolyhedron* pBoundPoly = static_cast<const phBoundPolyhedron*>(pBound);
				
				s32 numPolysToAdd = Min(pBoundPoly->GetNumPolygons(), numSpareColnPolys);
				for (int i=0; i<numPolysToAdd; i++)
				{
					const phPolygon& currPoly = pBoundPoly->GetPolygon(i);

					phMaterialIndex boundMtlId = pBoundPoly->GetPolygonMaterialIndex(ptrdiff_t_to_int(&currPoly - pBoundPoly->GetPolygonPointer()));
					phMaterialMgr::Id packedMtlId = pBoundPoly->GetMaterialId(boundMtlId);
					phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(packedMtlId);

					Vec3V vVertsLcl[3];
					Vec3V vVertsWld[3];
					for (s32 j=0; j<3; j++)
					{
						vVertsLcl[j] = pBoundPoly->GetVertex(currPoly.GetVertexIndex(j));
						vVertsWld[j] = Transform(boundMtx, vVertsLcl[j]);
					}

					// calc the normal of this poly
					Vec3V vNormalLcl = Cross(vVertsLcl[1]-vVertsLcl[0], vVertsLcl[2]-vVertsLcl[0]);
					Vec3V vNormal1Wld = Transform3x3(boundMtx, vNormalLcl);
					Vec3V vNormal1WldNorm = Normalize(vNormal1Wld);

					if (geomSpheres::TestSphereToTriangle(Vec4V(vPos, ScalarV(range)), vVertsWld, vNormal1WldNorm))
					{
						pColnSet->AddTri(&vVertsWld[0], vNormal1WldNorm, (u8)mtlId);
					}
				}
			}
		}
		else if (pBound->GetType()==phBound::BOX)
		{
			if (!onlyProcessBVH)
			{
				const phBoundBox* pBoundBox = static_cast<const phBoundBox*>(pBound);

				Vec3V vBoxMax = pBoundBox->GetBoundingBoxMax();
				Vec3V vBoxMin = pBoundBox->GetBoundingBoxMin();
				Vec3V vCentrePos = Vec3V(V_ZERO);//pBoundBox->GetCentroidOffset();

				ScalarV vMinX = SplatX(vBoxMin);
				ScalarV vMinY = SplatY(vBoxMin);
				ScalarV vMinZ = SplatZ(vBoxMin);
				ScalarV vMaxX = SplatX(vBoxMax);
				ScalarV vMaxY = SplatY(vBoxMax);
				ScalarV vMaxZ = SplatZ(vBoxMax);

				Vec3V vRight   = Vec3V(V_X_AXIS_WZERO);
				Vec3V vForward = Vec3V(V_Y_AXIS_WZERO);
				Vec3V vUp      = Vec3V(V_Z_AXIS_WZERO);

				Vec3V vBoxVerts[8];
				vBoxVerts[0] = vCentrePos + vRight*vMinX + vForward*vMinY + vUp*vMinZ;
				vBoxVerts[1] = vCentrePos + vRight*vMaxX + vForward*vMinY + vUp*vMinZ;
				vBoxVerts[2] = vCentrePos + vRight*vMaxX + vForward*vMaxY + vUp*vMinZ;
				vBoxVerts[3] = vCentrePos + vRight*vMinX + vForward*vMaxY + vUp*vMinZ;
				vBoxVerts[4] = vCentrePos + vRight*vMinX + vForward*vMinY + vUp*vMaxZ;
				vBoxVerts[5] = vCentrePos + vRight*vMaxX + vForward*vMinY + vUp*vMaxZ;
				vBoxVerts[6] = vCentrePos + vRight*vMaxX + vForward*vMaxY + vUp*vMaxZ;
				vBoxVerts[7] = vCentrePos + vRight*vMinX + vForward*vMaxY + vUp*vMaxZ;

				static const int boxTris[12][3] = {{0, 2, 1},
				{0, 3, 2},
				{1, 6, 5},
				{1, 2, 6},
				{4, 3, 0},
				{4, 7, 3},
				{4, 1, 5},
				{4, 0, 1},
				{3, 6, 2},
				{3, 7, 6},
				{7, 5, 6},
				{7, 4, 5}};

				ScalarV vehDmgScale(V_ZERO);

				s32 numPolysToAdd = Min(12, numSpareColnPolys);
				for (int i=0; i<numPolysToAdd; i++)
				{
					const phMaterialMgr::Id packedMtlId = pBoundBox->GetMaterialId(0);
					phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(packedMtlId);

					Vec3V vVertsLcl[3];
					Vec3V vVertsWld[3];
					for (s32 j=0; j<3; j++)
					{
						vVertsLcl[j] = vBoxVerts[boxTris[i][j]];
						vVertsWld[j] = Transform(boundMtx, vVertsLcl[j]);
					}

					// calc the normal of this poly
					Vec3V vNormalLcl = Cross(vVertsLcl[1]-vVertsLcl[0], vVertsLcl[2]-vVertsLcl[0]);
					Vec3V vNormal1Wld = Transform3x3(boundMtx, vNormalLcl);
					Vec3V vNormal1WldNorm = Normalize(vNormal1Wld);
				
					if (geomSpheres::TestSphereToTriangle(Vec4V(vPos, ScalarV(range)), vVertsWld, vNormal1WldNorm))
					{
						pColnSet->AddTri(&vVertsWld[0], vNormal1WldNorm, (u8)mtlId);
					}
				}
			}
		}
		else if (pBound->GetType()==phBound::BVH)
		{
			phBoundBVH* pBoundBVH = static_cast<phBoundBVH*>(pBound);

			Vec3V vCullPos = vPos;
			vCullPos = UnTransformOrtho(boundMtx, vCullPos);

			phBoundCuller culler;
			const int maxCulledPolys = 1024;
			phPolygon::Index culledPolys[maxCulledPolys];
			culler.SetArrays(culledPolys, maxCulledPolys);
			pBoundBVH->CullSpherePolys(culler,vCullPos,ScalarVFromF32(range));

#if __BANK
			if (g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbes())
			{
				grcDebugDraw::Sphere(vCullPos, range, Color32(1.0f, 0.0f, 0.0f, 1.0f), false, g_ptFxManager.GetGameDebugInterface().GetRenderDebugCollisionProbesFrames());
			}
#endif

			int numActivePolygons = culler.GetNumCulledPolygons();

			// go through this object's active polys (non culled)
			for (s32 i=numActivePolygons-1; i>=0; i--)
			{
				phPolygon::Index polyIndex = culler.GetCulledPolygonIndex(i);
				const phPrimitive& bvhPrimitive = pBoundBVH->GetPrimitive(polyIndex);

				phMaterialIndex boundMtlId = pBoundBVH->GetPolygonMaterialIndex(polyIndex);
				phMaterialMgr::Id packedMtlId = pBoundBVH->GetMaterialId(boundMtlId);
				phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(packedMtlId);

				if (bvhPrimitive.GetType() == PRIM_TYPE_POLYGON)
				{
					const phPolygon& currPoly = bvhPrimitive.GetPolygon();
					Vec3V vVerts[3];
					for (s32 j=0; j<3; j++)
					{
						Vec3V vCoords = pBoundBVH->GetVertex(currPoly.GetVertexIndex(j));
						vVerts[j] = Transform(boundMtx, vCoords);
					}

					Vec3V vNormal = pBoundBVH->GetPolygonUnitNormal(polyIndex);
					vNormal = Transform3x3(boundMtx, vNormal);

					if (pColnSet->GetNumColnPolys()<maxPolys)
					{
						pColnSet->AddTri(&vVerts[0], vNormal, (u8)mtlId);
					}
				}
				else if (bvhPrimitive.GetType() == PRIM_TYPE_BOX)
				{
					const phPrimBox& bvhBoxPrim = bvhPrimitive.GetBox();

					//			0______5
					//		   /	  /|
					//		  /		 / |
					//		6/_____3/  |
					//		|		|  |
					//		|	4	|  1
					//		|		| /
					//		|		|/
					//		2-------7 

					// store vertex info (transforming into world space)
					Vec3V vVertsWld[8];
					vVertsWld[0] = pBoundBVH->GetVertex(bvhBoxPrim.GetVertexIndex(0));
					vVertsWld[1] = pBoundBVH->GetVertex(bvhBoxPrim.GetVertexIndex(1));
					vVertsWld[2] = pBoundBVH->GetVertex(bvhBoxPrim.GetVertexIndex(2));
					vVertsWld[3] = pBoundBVH->GetVertex(bvhBoxPrim.GetVertexIndex(3));

					// transform into world space
					vVertsWld[0] = Transform(boundMtx, vVertsWld[0]);
					vVertsWld[1] = Transform(boundMtx, vVertsWld[1]);
					vVertsWld[2] = Transform(boundMtx, vVertsWld[2]);
					vVertsWld[3] = Transform(boundMtx, vVertsWld[3]);

					Vec3V vDirRight = ScalarV(V_HALF) * (vVertsWld[1] + vVertsWld[3] - vVertsWld[0] - vVertsWld[2]);
					Vec3V vDirUp = ScalarV(V_HALF) * (vVertsWld[0] + vVertsWld[3] - vVertsWld[1] - vVertsWld[2]);
					// Vec3V vCenter = ScalarV(V_QUARTER) * (vVertsWld[0] + vVertsWld[1] + vVertsWld[2] + vVertsWld[3]);

					// calculate the other vertices
					vVertsWld[4] = vVertsWld[1] - vDirRight;
					vVertsWld[5] = vVertsWld[0] + vDirRight;
					vVertsWld[6] = vVertsWld[2] + vDirUp;
					vVertsWld[7] = vVertsWld[2] + vDirRight;

					// add the box quads
					Vec3V vNormal;
					Vec3V vVerts[4];
					if (pColnSet->GetNumColnPolys()<maxPolys)
					{
						vVerts[0] = vVertsWld[2];
						vVerts[1] = vVertsWld[4];
						vVerts[2] = vVertsWld[1];
						vVerts[3] = vVertsWld[7];
						vNormal = Cross(vVerts[2]-vVerts[1], vVerts[0]-vVerts[1]);
						vNormal = Normalize(vNormal);

						pColnSet->AddQuad(&vVerts[0], vNormal, (u8)mtlId);
					}

					if (pColnSet->GetNumColnPolys()<maxPolys)
					{
						vVerts[0] = vVertsWld[4];
						vVerts[1] = vVertsWld[0];
						vVerts[2] = vVertsWld[5];
						vVerts[3] = vVertsWld[1];
						vNormal = Cross(vVerts[2]-vVerts[1], vVerts[0]-vVerts[1]);
						vNormal = Normalize(vNormal);

						pColnSet->AddQuad(&vVerts[0], vNormal, (u8)mtlId);
					}

					if (pColnSet->GetNumColnPolys()<maxPolys)
					{
						vVerts[0] = vVertsWld[0];
						vVerts[1] = vVertsWld[6];
						vVerts[2] = vVertsWld[3];
						vVerts[3] = vVertsWld[5];
						vNormal = Cross(vVerts[2]-vVerts[1], vVerts[0]-vVerts[1]);
						vNormal = Normalize(vNormal);

						pColnSet->AddQuad(&vVerts[0], vNormal, (u8)mtlId);
					}

					if (pColnSet->GetNumColnPolys()<maxPolys)
					{
						vVerts[0] = vVertsWld[6];
						vVerts[1] = vVertsWld[2];
						vVerts[2] = vVertsWld[7];
						vVerts[3] = vVertsWld[3];
						vNormal = Cross(vVerts[2]-vVerts[1], vVerts[0]-vVerts[1]);
						vNormal = Normalize(vNormal);

						pColnSet->AddQuad(&vVerts[0], vNormal, (u8)mtlId);
					}

					if (pColnSet->GetNumColnPolys()<maxPolys)
					{
						vVerts[0] = vVertsWld[4];
						vVerts[1] = vVertsWld[2];
						vVerts[2] = vVertsWld[6];
						vVerts[3] = vVertsWld[0];
						vNormal = Cross(vVerts[2]-vVerts[1], vVerts[0]-vVerts[1]);
						vNormal = Normalize(vNormal);

						pColnSet->AddQuad(&vVerts[0], vNormal, (u8)mtlId);
					}

					if (pColnSet->GetNumColnPolys()<maxPolys)
					{
						vVerts[0] = vVertsWld[5];
						vVerts[1] = vVertsWld[3];
						vVerts[2] = vVertsWld[7];
						vVerts[3] = vVertsWld[1];
						vNormal = Cross(vVerts[2]-vVerts[1], vVerts[0]-vVerts[1]);
						vNormal = Normalize(vNormal);

						pColnSet->AddQuad(&vVerts[0], vNormal, (u8)mtlId);
					}
				}
				else if (bvhPrimitive.GetType() == PRIM_TYPE_SPHERE)
				{
					ptfxDebugf2("CPtFxCollision::AddBoundsToColnSet - %s has sphere primitive - particles will not collide", pInst->GetArchetype()->GetFilename());
				}
				else if (bvhPrimitive.GetType() == PRIM_TYPE_CAPSULE)
				{
					ptfxDebugf2("CPtFxCollision::AddBoundsToColnSet - %s has capsule primitive - particles will not collide", pInst->GetArchetype()->GetFilename());
				}
				else if (bvhPrimitive.GetType() == PRIM_TYPE_CYLINDER)
				{
					ptfxDebugf2("CPtFxCollision::AddBoundsToColnSet - %s has cylinder primitive - particles will not collide", pInst->GetArchetype()->GetFilename());
				}
				else
				{
					ptfxDebugf2("CPtFxCollision::AddBoundsToColnSet - %s has unknown primitive (%d) - particles will not collide", pInst->GetArchetype()->GetFilename(), bvhPrimitive.GetType());
				}
			}
		}
	}
}

