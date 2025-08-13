// FILE :    PedCapsule.cpp
// PURPOSE : Load in capsule information for varying types of peds
// CREATED : 10-11-2010

// class header
#include "PedCapsule.h"

// Framework headers

// Game headers
#include "modelinfo/modelinfo_channel.h"
#include "physics/gtaMaterialManager.h"
#include "modelinfo/PedModelInfo.h"
#include "physics/gtaArchetype.h"

// rage headers
#include "fwmaths/angle.h"
#include "phbound/boundcapsule.h"
#include "phbound/boundcomposite.h"
#include "phbound/boundcurvedgeom.h"
#include "phbound/boundcylinder.h"
#if USE_SPHERES
	#include "phbound/boundsphere.h"
#else
	#include "phbound/boundbox.h"
#endif
#include "bank/bank.h"
#include "parser/manager.h"

// Parser headers
#include "PedCapsule_parser.h"

AI_OPTIMISATIONS()


//////////////////////////////////////////////////////////////////////////
// CPedCapsuleInfoManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CPedCapsuleInfoManager CPedCapsuleInfoManager::m_CapsuleManagerInstance;


void CPedCapsuleInfoManager::Init(const char* pFileName)
{
	Load(pFileName);
}


void CPedCapsuleInfoManager::Shutdown()
{
	// Delete
	Reset();
}


void CPedCapsuleInfoManager::Load(const char* pFileName)
{
	if(Verifyf(!m_CapsuleManagerInstance.m_aPedCapsule.GetCount(), "Capsule Info has already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		PARSER.LoadObject(pFileName, "xml", m_CapsuleManagerInstance);
	}
}

void CPedCapsuleInfoManager::Append(const char* pFileName)
{
	CPedCapsuleInfoManager tempInst;
	PARSER.LoadObject(pFileName, "xml", tempInst);

	for(int i = 0; i < tempInst.m_aPedCapsule.GetCount(); i++)
	{
		m_aPedCapsule.PushAndGrow(tempInst.m_aPedCapsule[i]); 
	}
}

void CPedCapsuleInfoManager::Reset()
{
	for (int i=0 ; i<m_CapsuleManagerInstance.m_aPedCapsule.GetCount() ; i++)
	{
		delete m_CapsuleManagerInstance.m_aPedCapsule[i];
	}
	m_CapsuleManagerInstance.m_aPedCapsule.Reset();
}

#if USE_GEOMETRY_CURVED
phBound* CBipedCapsuleInfo::InitPhysCreateCurvedGeomentryBound(float fRadius, float fHeight, float fMargin, phBoundCurvedGeometry* pCurvedBound)
{
	int numVerts = 4;
	int numMaterials = 1;
	int numPolys = 0;
	int numCurvedEdges = 4;
	int numCurvedFaces = 0;
	
	// Create and initialize a curved geometry bound if we weren't given one
	if(pCurvedBound == NULL)
	{
		pCurvedBound = rage_new phBoundCurvedGeometry;	
		pCurvedBound->Init(numVerts,numMaterials,numPolys,numCurvedEdges,numCurvedFaces);
	}
	else
	{
		modelinfoAssertf(pCurvedBound->GetNumVertices() == numVerts, "Invalid bound given");
		modelinfoAssertf(pCurvedBound->GetNumMaterials() == numMaterials,"Invalid bound given");
		modelinfoAssertf(pCurvedBound->GetNumPolygons() == numPolys,"Invalid bound given");
		modelinfoAssertf(pCurvedBound->GetNumCurvedEdges() == numCurvedEdges,"Invalid bound given");
		modelinfoAssertf(pCurvedBound->GetNumCurvedFaces() == numCurvedFaces,"Invalid bound given");
	}

	Vector3 bbMin(-fRadius - fMargin, -fRadius - fMargin, -0.5f*fHeight - fMargin);
	Vector3 bbMax( fRadius + fMargin,  fRadius + fMargin,  0.5f*fHeight + fMargin);

#if COMPRESSED_VERTEX_METHOD > 0
	// If the verts are compressed, the quantization needs to be initialized before we can set verts
	pCurvedBound->InitQuantization(RCC_VEC3V(bbMin), RCC_VEC3V(bbMax));
#else
	pCurvedBound->SetBoundingBox(RCC_VEC3V(bbMin), RCC_VEC3V(bbMax));
#endif

	// Set four vertices.
	pCurvedBound->SetMargin(fMargin);
	pCurvedBound->SetVertex(0,Vec3V( fRadius, 0.0f, -0.5f*fHeight));
	pCurvedBound->SetVertex(1,Vec3V(-fRadius, 0.0f, -0.5f*fHeight));
	pCurvedBound->SetVertex(2,Vec3V( fRadius, 0.0f,  0.5f*fHeight));
	pCurvedBound->SetVertex(3,Vec3V(-fRadius, 0.0f,  0.5f*fHeight));

	// Set two half-circular curved edges for the bottom.
	pCurvedBound->SetCurvedEdge(0, 1, 0, fRadius, -ZAXIS,  YAXIS);
	pCurvedBound->SetCurvedEdge(1, 0, 1, fRadius, -ZAXIS, -YAXIS);

	// Set two half-circular curved edges for the top.
	pCurvedBound->SetCurvedEdge(2, 2, 3, fRadius, ZAXIS, -YAXIS);
	pCurvedBound->SetCurvedEdge(3, 3, 2, fRadius, ZAXIS,  YAXIS);

	//pCurvedBound->CalculateGeomExtents();
	pCurvedBound->SetCentroidOffset(Vec3V(V_ZERO));
	pCurvedBound->SetCGOffset(Vec3V(V_ZERO));
	pCurvedBound->SetRadiusAroundCentroid(Dist(pCurvedBound->GetBoundingBoxMax(), pCurvedBound->GetCentroidOffset()));

	//pCurvedBound->CalculateGeomExtents();
	pCurvedBound->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);

	return pCurvedBound;
}
#endif // USE_GEOMETRY_CURVED

phBoundComposite* CBipedCapsuleInfo::GeneratePedBound(CPedModelInfo &/*rPedModelInfo*/, phBoundComposite* BANK_ONLY(pCompositeBound)) const
{
	// create a bound consisting of a capsule
	// (needs to be a composite because capsules are orientated around local y-axis)	
	int nPedBounds = 1;
	bool bPedInternalMotion = false;

	Assertf((GetHeadHeight()-GetCapsuleZOffset()) > 2.0f*GetRadius(),"Invalid ped bound dimensions for '%s'. The difference between the top and bottom of the capsule cannot be less than double the radius Head %f Cap Offset %f Radius %f.",GetName(), GetHeadHeight(), GetCapsuleZOffset(), GetRadius());

	const float fMainCapsuleHeight = GetHeadHeight();
#if PED_USE_CAPSULE_PROBES
	nPedBounds++;
	bPedInternalMotion = true;
#endif
#if PED_USE_EXTRA_BOUND
	nPedBounds++;
#endif
#if PLAYER_USE_LOWER_LEG_BOUND
	if(GetUseLowerLegBound())
		nPedBounds++;
#endif
#if PLAYER_USE_FPS_HEAD_BOUND
	if(GetUseFPSHeadBlocker())
		nPedBounds++;
#endif
#if PED_CURVEDGEOM_FOR_MAIN_CAPSULE
	// We need an extra capsule bound for shapetests to work if a curved geom bound is used
	nPedBounds++;
#endif

#if __BANK
	phBoundComposite* pComposite;
	if (pCompositeBound)
	{
		if (pCompositeBound->GetNumBounds() != nPedBounds)
		{
			Errorf("Tuning requires change of bound count - not supported at runtime. Please save file out and reload the game to see!");
			return NULL;
		}

		//We've been handed a bound to rebuild. Delete it's current contents
		pCompositeBound->Shutdown();
		pComposite = pCompositeBound;
	}
	else
	{
		pComposite = rage_new phBoundComposite;
	}
#else
	phBoundComposite* pComposite = rage_new phBoundComposite;
#endif

	pComposite->Init(nPedBounds, bPedInternalMotion);

#if PED_CURVEDGEOM_FOR_MAIN_CAPSULE
	static dev_float sfPedKneeHeight = GetCapsuleZOffset();
	float fCurvedGeomRadius = GetRadius() - PED_CURVEDGEOM_FOR_MAIN_MARGIN;
	float fCurvedGeomLength = fMainCapsuleHeight - sfPedKneeHeight - 2.0f*PED_CURVEDGEOM_FOR_MAIN_MARGIN;

	phBound* pMainCurvedGeomBound = InitPhysCreateCurvedGeomentryBound(fCurvedGeomRadius, fCurvedGeomLength, PED_CURVEDGEOM_FOR_MAIN_MARGIN, NULL);
	pComposite->SetBound(BOUND_MAIN_CAPSULE, pMainCurvedGeomBound);
	// remember to release AFTER ownership has been passed to the composite
	pMainCurvedGeomBound->Release();

	Matrix34 capsuleMat;
	capsuleMat.Identity();
	capsuleMat.d.z = 0.5f * (GetCapsuleZOffset() + fMainCapsuleHeight);
	pComposite->SetCurrentMatrix(BOUND_MAIN_CAPSULE, capsuleMat);
	pComposite->SetLastMatrix(BOUND_MAIN_CAPSULE, capsuleMat);

	// Keep the old style capsule bound so shape tests still work, since most tests don't work against curved geometry bounds
#if PED_USE_SHAPETEST_CAPSULE > 0
	phBoundCapsule *pShapetestCapsule = rage_new phBoundCapsule();
	pShapetestCapsule->SetCapsuleSize(GetRadius(), fMainCapsuleHeight - GetCapsuleZOffset() - 2.0f*GetRadius());
	pShapetestCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(PED_USE_SHAPETEST_CAPSULE, pShapetestCapsule);
	pShapetestCapsule->Release();

	capsuleMat.Identity();
	capsuleMat.d.z = 0.5f * (fMainCapsuleHeight + GetCapsuleZOffset());
	capsuleMat.RotateX(HALF_PI);
	pComposite->SetCurrentMatrix(PED_USE_SHAPETEST_CAPSULE, capsuleMat);
	pComposite->SetLastMatrix(PED_USE_SHAPETEST_CAPSULE, capsuleMat);

	// We don't want this capsule to collide with stuff except shapetests
	pComposite->AllocateTypeAndIncludeFlags();
	pComposite->SetIncludeFlags(PED_USE_SHAPETEST_CAPSULE,ArchetypeFlags::GTA_ALL_SHAPETEST_TYPES);
	pComposite->SetTypeFlags(PED_USE_SHAPETEST_CAPSULE,ArchetypeFlags::GTA_PED_TYPE);
#endif // PED_USE_SHAPETEST_CAPSULE

#else
	// need to setup capsule AFTER we've added it into the composite bound because the phBoundComposite::Init() function does a memset(0) on all structures!
	phBoundCapsule *pMainCapsule = rage_new phBoundCapsule();

	pMainCapsule->SetCapsuleSize(GetRadius(), fMainCapsuleHeight - GetCapsuleZOffset() - 2.0f*GetRadius());
	pMainCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(BOUND_MAIN_CAPSULE, pMainCapsule);
	pMainCapsule->Release();

	// The old backface culler doesn't work well with our PreComputeImpacts side shaving code. The only reason
	//   the new backface culler isn't used by default is vehicles heavily depend on the old one. 
	pMainCapsule->EnableUseNewBackFaceCull();

	Matrix34 capsuleMat;
	capsuleMat.Identity();
	capsuleMat.d.z = 0.5f * (fMainCapsuleHeight + GetCapsuleZOffset());
	capsuleMat.RotateX(HALF_PI);
	pComposite->SetCurrentMatrix(BOUND_MAIN_CAPSULE, RCC_MAT34V(capsuleMat));
	pComposite->SetLastMatrix(BOUND_MAIN_CAPSULE, RCC_MAT34V(capsuleMat));

	if (!pComposite->GetTypeAndIncludeFlags())
	{
		pComposite->AllocateTypeAndIncludeFlags();
	}

	// The main capsule shouldn't collide with stair slopes as they are allowed to have exposed edges which gives unrealistic contact normals
	pComposite->SetIncludeFlags(BOUND_MAIN_CAPSULE, pComposite->GetIncludeFlags(BOUND_MAIN_CAPSULE) & ~ArchetypeFlags::GTA_STAIR_SLOPE_TYPE);
#endif

	u8 nPedBoundIndex = 1;

#if PED_USE_CAPSULE_PROBES
	const float fPedProbeTop = GetKneeHeight() + 2.0f * GetProbeRadius();
	const float fPedProbeBottom = (-GetGroundToRootOffset()) - GetGroundOffsetExtend();


	// second capsule to replace the ped probes
	phBoundCapsule *pStandCapsule = rage_new phBoundCapsule();
	float fStandCapsuleLength = fPedProbeTop - fPedProbeBottom - 2.0f*GetProbeRadius();
	pStandCapsule->SetCapsuleSize(GetProbeRadius(), fStandCapsuleLength);
	pStandCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(BOUND_CAPSULE_PROBE, pStandCapsule);
	pStandCapsule->Release();

	capsuleMat.Identity();
	capsuleMat.d.z =  0.5f * (fPedProbeTop + fPedProbeBottom);
	capsuleMat.d.y += GetProbeYOffset();
	capsuleMat.RotateX(HALF_PI);
	pComposite->SetCurrentMatrix(BOUND_CAPSULE_PROBE, RCC_MAT34V(capsuleMat));
	capsuleMat.d.z = 0.5f * (fPedProbeTop + fPedProbeBottom) + (GetHeadHeight() - fPedProbeTop);;
	pComposite->SetLastMatrix(BOUND_CAPSULE_PROBE, RCC_MAT34V(capsuleMat));
	nPedBoundIndex++;
#endif //PED_USE_CAPSULE_PROBES

#if PED_USE_EXTRA_BOUND
	phBoundCapsule* pExtraBound = rage_new phBoundCapsule();

	// Initialize the bound using the same size and transform as the main capsule as we don't want it affecting
	// the overall extents of the composite bound
	pExtraBound->SetCapsuleSize(GetRadius(), fMainCapsuleHeight - GetCapsuleZOffset() - 2.0f*GetRadius());
	pExtraBound->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(nPedBoundIndex, pExtraBound);
	pExtraBound->Release();

	capsuleMat.Identity();
	capsuleMat.d.z = 0.5f * (fMainCapsuleHeight + GetCapsuleZOffset());
	capsuleMat.RotateX(HALF_PI);
	pComposite->SetCurrentMatrix(nPedBoundIndex, RCC_MAT34V(capsuleMat));
	pComposite->SetLastMatrix(nPedBoundIndex, RCC_MAT34V(capsuleMat));

	if (!pComposite->GetTypeAndIncludeFlags())
	{
		pComposite->AllocateTypeAndIncludeFlags();
	}

	//! Initially disabled. We enable it as needed.
	pComposite->SetIncludeFlags(nPedBoundIndex, 0);
	pComposite->SetTypeFlags(nPedBoundIndex, ArchetypeFlags::GTA_PED_TYPE);

	nPedBoundIndex++;
#endif //PED_USE_EXTRA_BOUND

#if PLAYER_USE_LOWER_LEG_BOUND
	if(GetUseLowerLegBound())
	{
#if !PLAYER_USE_CAPSULE_LEG_BOUND
		CompileTimeAssert(USE_GEOMETRY_CURVED);
		float fRadius = GetRadius() - PLAYER_LEGS_MARGIN;
		float fHeight = PLAYER_LEGS_TOP - PLAYER_LEGS_BOTTOM - 2.0f * PLAYER_LEGS_MARGIN;
		phBound* pLegsBound = InitPhysCreateCurvedGeomentryBound(fRadius, fHeight, PLAYER_LEGS_MARGIN, NULL);
#else // !PLAYER_USE_CAPSULE_LEG_BOUND
		// Create a cylinder bound that is equivalent to the curved geometry bound.
		float fRadius = GetRadius();
		float fHeight = PLAYER_LEGS_TOP - PLAYER_LEGS_BOTTOM;
		phBoundCylinder * pLegsBound = rage_new phBoundCylinder;
		pLegsBound->SetMargin(PLAYER_LEGS_MARGIN);
		pLegsBound->SetCylinderRadiusAndHalfHeight(ScalarV(fRadius), ScalarV(.5f * fHeight));
#endif // !PLAYER_USE_CAPSULE_LEG_BOUND

		pComposite->SetBound(nPedBoundIndex, pLegsBound);
		// remember to release AFTER ownership has been passed to the composite
		pLegsBound->Release();

#if !PLAYER_USE_CAPSULE_LEG_BOUND
		capsuleMat.Identity();
#else // !PLAYER_USE_CAPSULE_LEG_BOUND
		// The cylinder axis is along the y direction.
		// The player up axis is along the z direction, y forward, x right.
		// Set the cylinder orientation so that its axis is directed along the player's up axis.
		capsuleMat.Set(Vector3(1,0,0),Vector3(0,0,1),Vector3(0,-1,0),Vector3(0,0,0));
#endif // !PLAYER_USE_CAPSULE_LEG_BOUND

		capsuleMat.d.z = 0.5f * (PLAYER_LEGS_TOP + PLAYER_LEGS_BOTTOM);
		pComposite->SetCurrentMatrix(nPedBoundIndex, RCC_MAT34V(capsuleMat));
		pComposite->SetLastMatrix(nPedBoundIndex, RCC_MAT34V(capsuleMat));
		nPedBoundIndex++;
	}
#endif //PLAYER_USE_LOWER_LEG_BOUND

#if PLAYER_USE_FPS_HEAD_BOUND
	if (GetUseFPSHeadBlocker())
	{
		// Create a sphere to cover the peds head when in FPS mode to prevent camera clipping with obstacles
		phBoundSphere* pFPSHeadBound = rage_new phBoundSphere();

		pFPSHeadBound->SetSphereRadius(GetRadiusFPSHeadBlocker());
		pFPSHeadBound->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
		pComposite->SetBound(nPedBoundIndex, pFPSHeadBound);
		pFPSHeadBound->Release();

		capsuleMat.Identity();
		capsuleMat.d.z = GetHeadHeight();
		capsuleMat.RotateX(HALF_PI);
		pComposite->SetCurrentMatrix(nPedBoundIndex, RCC_MAT34V(capsuleMat));
		pComposite->SetLastMatrix(nPedBoundIndex, RCC_MAT34V(capsuleMat));

		if (!pComposite->GetTypeAndIncludeFlags())
		{
			pComposite->AllocateTypeAndIncludeFlags();
		}

		//! Initially disabled. We enable it as needed.
		pComposite->SetIncludeFlags(nPedBoundIndex, 0);
		pComposite->SetTypeFlags(nPedBoundIndex, ArchetypeFlags::GTA_PED_TYPE);

		nPedBoundIndex++;
	}
#endif	//PLAYER_USE_FPS_HEAD_BOUND

	pComposite->CalculateCompositeExtents();
	pComposite->UpdateBvh(true);

	return pComposite;
}

phBoundComposite* CFishCapsuleInfo::GeneratePedBound(CPedModelInfo &/*rPedModelInfo*/, phBoundComposite* BANK_ONLY(pCompositeBound)) const
{
#if __BANK
	phBoundComposite* pComposite;
	if (pCompositeBound)
	{
		//We've been handed a bound to rebuild. Delete it's current contents
		pCompositeBound->Shutdown();
		pComposite = pCompositeBound;
	}
	else
	{
		pComposite = rage_new phBoundComposite;
	}
#else
	phBoundComposite* pComposite = rage_new phBoundComposite;
#endif

	pComposite->Init(1, true);

	// need to setup capsule AFTER we've added it into the composite bound because the phBoundComposite::Init() function does a memset(0) on all structures!
	phBoundCapsule *pMainCapsule = rage_new phBoundCapsule();

	pMainCapsule->SetCapsuleSize(m_Radius, m_Length);
	pMainCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(BOUND_MAIN_CAPSULE, pMainCapsule);
	pMainCapsule->Release();

	Matrix34 capsuleMat;
	capsuleMat.Identity();
	capsuleMat.d.y = m_YOffset;
	capsuleMat.d.z = m_ZOffset;
	pComposite->SetCurrentMatrix(BOUND_MAIN_CAPSULE, RCC_MAT34V(capsuleMat));
	pComposite->SetLastMatrix(BOUND_MAIN_CAPSULE, RCC_MAT34V(capsuleMat));

	//Finish building the composite
	pComposite->CalculateCompositeExtents();
	pComposite->UpdateBvh(true);
	return pComposite;
}

void CQuadrupedCapsuleInfo::OnPostLoad()
{
	m_MinSolidHeight = GetGroundToRootOffset() - m_Radius + m_ZOffset;
	if (m_Blocker)
	{
		m_MinSolidHeight -= (m_BlockerLength * Abs(cosf(m_BlockerXRotation))) + m_BlockerRadius;
	}
}

phBoundComposite* CQuadrupedCapsuleInfo::GeneratePedBound(CPedModelInfo &rPedModelInfo, phBoundComposite* BANK_ONLY(pCompositeBound)) const
{
	// create a bound consisting of a capsule
	// (needs to be a composite because capsules are orientated around local y-axis)	
	int nPedBounds = 3;	//Main plus swept spheres for legs
	if (m_Blocker)
	{
		nPedBounds += 1;
	}
	if (m_PropBlocker)
	{
		nPedBounds += 1;
	}
	if (m_NeckBound)
	{
		nPedBounds += 1;
	}

#if __BANK
	phBoundComposite* pComposite;
	if (pCompositeBound)
	{
		if (pCompositeBound->GetNumBounds() != nPedBounds)
		{
			Errorf("Tuning requires change of bound count - not supported at runtime. Please save file out and reload the game to see!");
			return NULL;
		}

		//We've been handed a bound to rebuild. Delete it's current contents
		pCompositeBound->Shutdown();
		pComposite = pCompositeBound;
	}
	else
	{
		pComposite = rage_new phBoundComposite;
	}
#else
	phBoundComposite* pComposite = rage_new phBoundComposite;
#endif

	pComposite->Init(nPedBounds, true);

	// need to setup capsule AFTER we've added it into the composite bound because the phBoundComposite::Init() function does a memset(0) on all structures!
	phBoundCapsule *pMainCapsule = rage_new phBoundCapsule();

	pMainCapsule->SetCapsuleSize(m_Radius, m_Length);
	pMainCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(BOUND_MAIN_CAPSULE, pMainCapsule);
	pMainCapsule->Release();

	Matrix34 capsuleMat;
	capsuleMat.Identity();
	capsuleMat.d.y = m_YOffset;
	capsuleMat.d.z = m_ZOffset;
	pComposite->SetCurrentMatrix(BOUND_MAIN_CAPSULE, RCC_MAT34V(capsuleMat));
	pComposite->SetLastMatrix(BOUND_MAIN_CAPSULE, RCC_MAT34V(capsuleMat));

	if (!pComposite->GetTypeAndIncludeFlags())
	{
		pComposite->AllocateTypeAndIncludeFlags();
	}

	// The main capsule shouldn't collide with stair slopes as they are allowed to have exposed edges which gives unrealistic contact normals
	pComposite->SetIncludeFlags(BOUND_MAIN_CAPSULE, pComposite->GetIncludeFlags(BOUND_MAIN_CAPSULE) & ~ArchetypeFlags::GTA_STAIR_SLOPE_TYPE);

	//Swept spheres (in composite bound)
	// second capsule to replace the ped probes
#if USE_SPHERES

	Assertf(GetProbeRadius() < m_Radius, "Quadruped %s probe radius is larger or equal in size to the main capsule. This might cause unusual behaviour in some cases.", rPedModelInfo.GetModelName());

	phBoundSphere *pStandCapsule = rage_new phBoundSphere();
	pStandCapsule->SetSphereRadius(GetProbeRadius());
#else
	const float kfBoxSize = GetProbeRadius()*2.0f;
	const Vector3 vHorseScale(kfBoxSize,kfBoxSize*2.0f,kfBoxSize);
	phBoundBox *pStandCapsule = rage_new phBoundBox(vHorseScale);
#endif
	pStandCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(BOUND_CAPSULE_PROBE, pStandCapsule);
	pStandCapsule->Release();

	Matrix34 scratchMtx;
	scratchMtx.Identity();
	scratchMtx.d.z = -m_ProbeDepthFromRoot;
	scratchMtx.d.y = GetProbeYOffset();
	scratchMtx.RotateX(HALF_PI);								//TODO: Remove?
	pComposite->SetCurrentMatrix(BOUND_CAPSULE_PROBE, RCC_MAT34V(scratchMtx));

	float fLastMatZ = capsuleMat.d.z - m_Radius;
	rPedModelInfo.SetQuadrupedProbeDepth( fLastMatZ - scratchMtx.d.z );
	//scratchMtx.d = capsuleMat.d;
	scratchMtx.d.z = fLastMatZ;
	rPedModelInfo.SetQuadrupedProbeTopPosition(BOUND_CAPSULE_PROBE, scratchMtx.d);
	pComposite->SetLastMatrix(BOUND_CAPSULE_PROBE, RCC_MAT34V(scratchMtx));

#if USE_SPHERES
	phBoundSphere *pRearStandCapsule = rage_new phBoundSphere();
	pRearStandCapsule->SetSphereRadius(GetProbeRadius());
#else
	phBoundBox *pRearStandCapsule = rage_new phBoundBox(vHorseScale);
#endif

	pRearStandCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
	pComposite->SetBound(BOUND_CAPSULE_PROBE_2, pRearStandCapsule);
	pRearStandCapsule->Release();

	scratchMtx.d.y -= GetProbeYOffset()*2.0f; //Make tunable?
	scratchMtx.d.z = -m_ProbeDepthFromRoot;
	pComposite->SetCurrentMatrix(BOUND_CAPSULE_PROBE_2, RCC_MAT34V(scratchMtx));
	scratchMtx.d.z = fLastMatZ;
	rPedModelInfo.SetQuadrupedProbeTopPosition(BOUND_CAPSULE_PROBE_2, scratchMtx.d);
	pComposite->SetLastMatrix(BOUND_CAPSULE_PROBE_2, RCC_MAT34V(scratchMtx));

	//Don't stand on peds / animals. It looks silly
	if (!pComposite->GetTypeAndIncludeFlags())
	{
		pComposite->AllocateTypeAndIncludeFlags();
	}
	u32 probeFlags = pComposite->GetIncludeFlags(1) & ~(ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_HORSE_TYPE);

	pComposite->SetIncludeFlags(BOUND_CAPSULE_PROBE, probeFlags);
	pComposite->SetIncludeFlags(BOUND_CAPSULE_PROBE_2, probeFlags);

	//Blocker to prevent rising up over things
	if (m_Blocker)
	{
		scratchMtx.Identity();
		scratchMtx.RotateX(m_BlockerXRotation);

		scratchMtx.d.y = m_BlockerYOffset;
		scratchMtx.d.z = m_BlockerZOffset;
		phBoundCapsule *pBlockCapsule = rage_new phBoundCapsule();
		pBlockCapsule->SetCapsuleSize(m_BlockerRadius, m_BlockerLength);
		pBlockCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
		pComposite->SetBound(m_BlockerSlot, pBlockCapsule);
		pComposite->SetCurrentMatrix(m_BlockerSlot, RCC_MAT34V(scratchMtx));
		pComposite->SetLastMatrix(m_BlockerSlot, RCC_MAT34V(scratchMtx));
		pBlockCapsule->Release();
	}

	if (m_PropBlocker)
	{
		if (!pComposite->GetTypeAndIncludeFlags())
		{
			pComposite->AllocateTypeAndIncludeFlags();
		}

		scratchMtx.Identity();
		scratchMtx.RotateX(HALF_PI);
		scratchMtx.d = Vector3(0, m_PropBlockerY, m_PropBlockerZ);
		phBoundCapsule *pBlockCapsule = rage_new phBoundCapsule();
		pBlockCapsule->SetCapsuleSize(m_PropBlockerRadius, m_PropBlockerLength);
		pBlockCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
		u32 uIncludeFlags = ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_PED_TYPE
			| ArchetypeFlags::GTA_HORSE_TYPE | ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
		pComposite->SetIncludeFlags(m_PropBlockerSlot, uIncludeFlags);
		pComposite->SetBound(m_PropBlockerSlot, pBlockCapsule);
		pComposite->SetCurrentMatrix(m_PropBlockerSlot, RCC_MAT34V(scratchMtx));
		pComposite->SetLastMatrix(m_PropBlockerSlot, RCC_MAT34V(scratchMtx));
		pBlockCapsule->Release();
	}

	//Neck bound, for reigns and preventing head penetration
	if (m_NeckBound)
	{
		scratchMtx.Identity();
		scratchMtx.RotateX(m_NeckBoundRotation);

		scratchMtx.d = Vector3(0, m_NeckBoundFwdOffset, m_NeckBoundHeightOffset);
		phBoundCapsule *pNeckBoundCapsule = rage_new phBoundCapsule();
		pNeckBoundCapsule->SetCapsuleSize(m_NeckBoundRadius, m_NeckBoundLength);
		pNeckBoundCapsule->SetMaterial(PGTAMATERIALMGR->g_idPhysPedCapsule);
		pComposite->SetBound(m_NeckBoundSlot, pNeckBoundCapsule);
		pComposite->SetCurrentMatrix(m_NeckBoundSlot, RCC_MAT34V(scratchMtx));
		pComposite->SetLastMatrix(m_NeckBoundSlot, RCC_MAT34V(scratchMtx));
		pNeckBoundCapsule->Release();
	}

	pComposite->CalculateCompositeExtents();
	pComposite->UpdateBvh(true);

	return pComposite;
}

#if __BANK
void CPedCapsuleInfoManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Ped Capsules");

	bank.AddButton("Save", Save);
	bank.AddButton("Refresh", Refresh);

	bank.PushGroup("Capsules");
	for(s32 i = 0; i < m_CapsuleManagerInstance.m_aPedCapsule.GetCount(); i++)
	{
		bank.PushGroup(m_CapsuleManagerInstance.m_aPedCapsule[i]->GetName());
		PARSER.AddWidgets(bank, m_CapsuleManagerInstance.m_aPedCapsule[i]);
		bank.PopGroup(); 
	}
	bank.PopGroup();
	bank.PopGroup(); 
}


void CPedCapsuleInfoManager::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/pedbounds", "xml", &m_CapsuleManagerInstance, parManager::XML), "Failed to save ped bounds/capsules");
}

void CPedCapsuleInfoManager::Refresh()
{
	CPedModelInfo::PedDataChangedCB();
}

#endif // __BANK
