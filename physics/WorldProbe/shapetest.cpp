// WorldProbe headers:
#include "physics/WorldProbe/shapetest.h"
#include "physics/WorldProbe/debug.h"

// RAGE headers:
#include "fwdebug/debugdraw.h"

// Game headers:
#include "physics/physics.h"
#include "vehicles/vehicle.h"

PHYSICS_OPTIMISATIONS()

#if WORLD_PROBE_DEBUG
void WorldProbe::CShapeTest::DrawLine(rage::Color32 colour, const char* debugText, phShapeProbe* probeShapePointer)
{
	phShapeTest<phShapeProbe>& probeTester = *reinterpret_cast< phShapeTest<phShapeProbe>* >( &m_ShapeTest );
	phShapeProbe& probeShape = probeShapePointer ? *probeShapePointer : probeTester.GetShape();

	const phSegmentV& worldSegment = probeShape.GetWorldSegment();

	if(IsDirected())
		grcDebugDraw::Arrow(worldSegment.GetStart(), worldSegment.GetEnd(), WorldProbe::CShapeTestManager::ms_fHitPointRadius, colour);
	else
		grcDebugDraw::Line(worldSegment.GetStart(), worldSegment.GetEnd(), colour, colour);

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, worldSegment.GetStart(), Color_black, 1, 1);
}

void WorldProbe::CShapeTest::DrawCapsule(rage::Color32 colour, const char* debugText, phShapeCapsule* capsuleShapePointer)
{
	const phShapeCapsule& capsuleShape = capsuleShapePointer ? *capsuleShapePointer : (IsDirected() ? 
		reinterpret_cast<const phShapeTest<phShapeSweptSphere>* >( &m_ShapeTest )->GetShape() :
		reinterpret_cast<const phShapeTest<phShapeCapsule>* >( &m_ShapeTest )->GetShape());

	const phSegmentV& worldSegment = capsuleShape.GetWorldSegment();

	grcDebugDraw::Capsule(worldSegment.GetStart(), worldSegment.GetEnd(), capsuleShape.GetRadius().Getf(), colour, false);

	if(IsDirected())
		grcDebugDraw::Arrow(worldSegment.GetStart(), worldSegment.GetEnd(), WorldProbe::CShapeTestManager::ms_fHitPointRadius, colour);

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, worldSegment.GetStart(), Color_black, 0, 1);
}

void WorldProbe::CShapeTest::DrawSphere(rage::Color32 colour, const char* debugText)
{
	phShapeTest<phShapeSphere>& sphereTester = *reinterpret_cast< phShapeTest<phShapeSphere>* >( &m_ShapeTest );

	grcDebugDraw::Sphere(RCC_VEC3V(sphereTester.GetShape().GetWorldCenter()), sphereTester.GetShape().GetRadius(), colour, false);

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, RCC_VEC3V(sphereTester.GetShape().GetWorldCenter()), Color_black, 0, 1);
}

void WorldProbe::CShapeTest::DrawBound(rage::Color32 colour, const char* debugText)
{
	phShapeTest<phShapeObject>& objectTester = *reinterpret_cast< phShapeTest<phShapeObject>* >( &m_ShapeTest );

	const phBound* bound = objectTester.GetShape().GetBound();

	if( bound )
	{
		grcDebugDraw::BoxOriented(bound->GetBoundingBoxMin(), bound->GetBoundingBoxMax(),
			RCC_MAT34V(objectTester.GetShape().GetWorldTransform()), colour, false);
	}

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, RCC_VEC3V(objectTester.GetShape().GetWorldTransform().d), Color_black, 0, 1);
}

void WorldProbe::CShapeTest::DrawBoundingBox(rage::Color32 colour, const char* debugText)
{
	CBoundingBoxShapeTestData& boundingBoxTesterData = *reinterpret_cast< CBoundingBoxShapeTestData* >( &m_ShapeTest );

	const phBound* bound = boundingBoxTesterData.m_pBound;

	if( bound )
	{
		if( IsBoundingBoxTestAHit() )
		{
			colour.SetAlpha(128); 
		}

		grcDebugDraw::BoxOriented(bound->GetBoundingBoxMin(), bound->GetBoundingBoxMax(),
			RCC_MAT34V(boundingBoxTesterData.m_TransformMatrix), colour, IsBoundingBoxTestAHit());
	}

	if(debugText)
		CShapeTestManager::DebugRenderTextLineWorldSpace(debugText, RCC_VEC3V(boundingBoxTesterData.m_TransformMatrix.d), Color_black, 0, 1);
}

void WorldProbe::CShapeTest::DrawBatch(rage::Color32 colour, const char* debugText)
{
	phShapeTest<phShapeBatch>& batchTester = *reinterpret_cast< phShapeTest<phShapeBatch>* >( &m_ShapeTest );

	for(int i = 0 ; i < batchTester.GetShape().GetNumProbes() ; i++)
	{
		DrawLine( colour, debugText, &batchTester.GetShape().GetProbe(i) );
	}

	for(int i = 0 ; i < batchTester.GetShape().GetNumEdges() ; i++)
	{
		DrawLine( colour, debugText, &batchTester.GetShape().GetEdge(i) );
	}

	for(int i = 0 ; i < batchTester.GetShape().GetNumCapsules() ; i++)
	{
		DrawCapsule( colour, debugText, &batchTester.GetShape().GetCapsule(i) );
	}

	for(int i = 0 ; i < batchTester.GetShape().GetNumSweptSpheres() ; i++)
	{
		DrawCapsule( colour, debugText, &batchTester.GetShape().GetSweptSphere(i) );
	}
}

void WorldProbe::CShapeTest::ConstructPausedRenderDataLine(WorldProbe::CShapeTestDebugRenderData& outRenderData, phShapeProbe const * probeShapePointer) const
{
	phShapeTest<phShapeProbe> const & probeTester = *reinterpret_cast< phShapeTest<phShapeProbe> const * >( &m_ShapeTest );
	phShapeProbe const & probeShape = probeShapePointer ? *probeShapePointer : probeTester.GetShape();

	const phSegmentV& worldSegment = probeShape.GetWorldSegment();

	outRenderData.m_StartPos = worldSegment.GetStart();
	outRenderData.m_EndPos = worldSegment.GetEnd();
	outRenderData.m_bDirected = IsDirected();
}

void WorldProbe::CShapeTest::ConstructPausedRenderDataCapsule(WorldProbe::CShapeTestDebugRenderData& outRenderData) const
{
	const phShapeCapsule& capsuleShape = IsDirected() ? reinterpret_cast<const phShapeTest<phShapeSweptSphere>* >( &m_ShapeTest )->GetShape() :
		reinterpret_cast<const phShapeTest<phShapeCapsule>* >( &m_ShapeTest )->GetShape();

	const phSegmentV& worldSegment = capsuleShape.GetWorldSegment();

	outRenderData.m_StartPos = worldSegment.GetStart();
	outRenderData.m_EndPos = worldSegment.GetEnd();
	outRenderData.m_Dims.SetX( capsuleShape.GetRadius() );
	outRenderData.m_bDirected = IsDirected();
}

void WorldProbe::CShapeTest::ConstructPausedRenderDataSphere(WorldProbe::CShapeTestDebugRenderData& outRenderData) const
{
	phShapeTest<phShapeSphere> const& sphereTester = *reinterpret_cast< phShapeTest<phShapeSphere> const* >( &m_ShapeTest );

	outRenderData.m_StartPos = RCC_VEC3V( sphereTester.GetShape().GetWorldCenter() );
	outRenderData.m_Dims.SetX( sphereTester.GetShape().GetRadius() );
}

void WorldProbe::CShapeTest::ConstructPausedRenderDataBound(WorldProbe::CShapeTestDebugRenderData& outRenderData) const
{
	phShapeTest<phShapeObject> const& objectTester = *reinterpret_cast< phShapeTest<phShapeObject> const * >( &m_ShapeTest );

	const phBound* bound = objectTester.GetShape().GetBound();

	outRenderData.m_bBoundExists = (bound != NULL);
	outRenderData.m_Transform = RCC_MAT34V(objectTester.GetShape().GetWorldTransform());

	if( bound )
	{
		outRenderData.m_StartPos = bound->GetBoundingBoxMin();
		outRenderData.m_EndPos = bound->GetBoundingBoxMax();
	}	
}

void WorldProbe::CShapeTest::ConstructPausedRenderDataBoundingBox(WorldProbe::CShapeTestDebugRenderData& outRenderData) const
{
	CBoundingBoxShapeTestData const & boundingBoxTesterData = *reinterpret_cast< CBoundingBoxShapeTestData const * >( &m_ShapeTest );

	const phBound* bound = boundingBoxTesterData.m_pBound;

	outRenderData.m_bBoundExists = (bound != NULL);
	outRenderData.m_Transform = RCC_MAT34V(boundingBoxTesterData.m_TransformMatrix);
	outRenderData.m_bHit = IsBoundingBoxTestAHit();

	if( bound )
	{
		outRenderData.m_StartPos = bound->GetBoundingBoxMin();
		outRenderData.m_EndPos = bound->GetBoundingBoxMax();
	}
}

void WorldProbe::CShapeTest::ConstructPausedRenderDataBatch(WorldProbe::CShapeTestDebugRenderData outRenderData[]) const
{
	phShapeTest<phShapeBatch> const & batchTester = *reinterpret_cast< phShapeTest<phShapeBatch> const * >( &m_ShapeTest );

	for(int i = 0 ; i < batchTester.GetShape().GetNumProbes() ; i++)
	{
		outRenderData[i].m_ShapeTestType = DIRECTED_LINE_TEST;
		ConstructPausedRenderDataLine( outRenderData[i], &batchTester.GetShape().GetProbe(i) );
	}

	for(int i = 0 ; i < batchTester.GetShape().GetNumEdges(); i++)
	{
		outRenderData[i].m_ShapeTestType = UNDIRECTED_LINE_TEST;
		ConstructPausedRenderDataLine( outRenderData[i], &batchTester.GetShape().GetEdge(i) );
	}	
}
#endif // WORLD_PROBE_DEBUG

int WorldProbe::CShapeTest::PerformLineAgainstVehicleTyresTest(phShapeProbe& shape, const phInst* const * pInstExceptions, int nNumInstExceptions, const phShapeTestCullResults* pCullResults)
{
	if(AssertVerify(pCullResults))
	{
		Vector3 start = VEC3V_TO_VECTOR3(shape.GetWorldSegment().GetStart());
		Vector3 end = VEC3V_TO_VECTOR3(shape.GetWorldSegment().GetEnd());
		for(phShapeTestCullResults::CulledInstanceIndex culledInstanceIndex = 0; culledInstanceIndex < pCullResults->GetNumInstances(); ++culledInstanceIndex)
		{
			pCullResults->SetCurrentInstanceIndex(culledInstanceIndex);
			const phInst* pInstance = pCullResults->GetCurrentInstance();
			AssertVerify(pInstance && pInstance->IsInLevel());
			if(CPhysics::GetLevel()->GetInstanceTypeFlag(pInstance->GetLevelIndex(),ArchetypeFlags::GTA_WHEEL_TEST))
			{
				// Don't test vehicles the user excluded. We could probably have the rage shapetest not add these in the first place
				bool bSkip = false;
				if(pInstExceptions)
				{
					for(int nInstEx=0; nInstEx<nNumInstExceptions; nInstEx++)
					{
						if(pInstExceptions[nInstEx]==pInstance)
						{
							bSkip = true;
							break;
						}
					}
				}
				if(bSkip)
					continue;

				const CEntity* pEntity = CPhysics::GetEntityFromInst(pInstance);
				// Do non-vehicles set ArchetypeFlags::GTA_WHEEL_TEST? I hope not.
				if(AssertVerify(pEntity && pEntity->GetIsTypeVehicle()))
				{
					const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
					// first test against bound sphere of vehicle
					float fBoundRadius = pVehicle->GetBoundRadius();
					if( !pVehicle->GetAreWheelsJustProbes() &&
						geomSpheres::TestSphereToSeg(VECTOR3_TO_VEC3V(pVehicle->GetBoundCentre()), ScalarVFromF32(fBoundRadius), VECTOR3_TO_VEC3V(start), VECTOR3_TO_VEC3V(end)) )
					{
						const CWheel * const * pWheels = pVehicle->GetWheels();
						for(int nWheel=0; nWheel < pVehicle->GetNumWheels(); nWheel++)
						{
							const CWheel & wheel = *pWheels[nWheel];
							if(wheel.GetFragChild() != -1)
							{
								Vector3 vecWheelPos;
								float fWheelRadius = wheel.GetWheelPosAndRadius(vecWheelPos);
								if( geomSpheres::TestSphereToSeg(VECTOR3_TO_VEC3V(vecWheelPos), ScalarVFromF32(fWheelRadius), VECTOR3_TO_VEC3V(start), VECTOR3_TO_VEC3V(end)) )
								{
									Matrix34 matWheel;
									Vector3 vecWheelBoxHalfWidth;
									wheel.GetWheelMatrixAndBBox(matWheel, vecWheelBoxHalfWidth);

									Vector3 vecLocalStart = start;
									matWheel.UnTransform(vecLocalStart);
									Vector3 vecLocalDelta = end - start;
									matWheel.UnTransform3x3(vecLocalDelta);

									ScalarV fT1V;
									Vector3 vecNormal1;
									int nIndex1;
									if(geomBoxes::TestSegmentToBox(
										RC_VEC3V(vecLocalStart), 
										RC_VEC3V(vecLocalDelta), 
										RC_VEC3V(vecWheelBoxHalfWidth), 
										&fT1V, &RC_VEC3V(vecNormal1), &nIndex1, NULL, NULL, NULL))
									{
										if(shape.GetNumHits() < shape.GetMaxNumIntersections())
										{
											float fT1 = fT1V.Getf();
											matWheel.Transform3x3(vecNormal1);
											vecNormal1.Normalize();
											Vector3 vecIntersectionPos = start + fT1 * (end - start);

											phMaterialMgr::Id nMaterialId = PGTAMATERIALMGR->g_idRubber;
											// see if we think we hit the wheel itself rather than the rubber
											if((vecIntersectionPos - matWheel.d).Mag2() < wheel.GetWheelRimRadius() * wheel.GetWheelRimRadius())
												nMaterialId = PGTAMATERIALMGR->g_idCarMetal;

											PGTAMATERIALMGR->PackPolyFlag(nMaterialId, POLYFLAG_VEHICLE_WHEEL);
											phInst* pVehicleInst = pVehicle->GetCurrentPhysicsInst();
											const u16 nLevelIndex = pVehicleInst->GetLevelIndex();
											const u16 nGenerationID = CPhysics::GetLevel()->GetGenerationID(nLevelIndex);
											physicsAssert(pVehicleInst);
											phIntersection newHit;
											newHit.Set(nLevelIndex, nGenerationID,
												RCC_VEC3V(vecIntersectionPos), RCC_VEC3V(vecNormal1),
												fT1, 0.0f, (u16)nWheel, 0, nMaterialId);
											shape.AppendIntersection(newHit);
										}
										else if(shape.IsBooleanTest())
										{
											return 1;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return shape.GetNumHits();
}

int WorldProbe::CShapeTest::GetNumHits(int shapeIndex) const
{
	switch(GetShapeTestType())
	{
	case DIRECTED_LINE_TEST:
		return reinterpret_cast<const phShapeTest<phShapeProbe>* >(&m_ShapeTest)->GetShape().GetNumHits();

	case UNDIRECTED_LINE_TEST:
		return reinterpret_cast<const phShapeTest<phShapeEdge>* >(&m_ShapeTest)->GetShape().GetNumHits();

	case CAPSULE_TEST:
		return reinterpret_cast<const phShapeTest<phShapeCapsule>* >(&m_ShapeTest)->GetShape().GetNumHits();

	case SWEPT_SPHERE_TEST:
		return reinterpret_cast<const phShapeTest<phShapeSweptSphere>* >(&m_ShapeTest)->GetShape().GetNumHits();

	case SPHERE_TEST:
		return reinterpret_cast<const phShapeTest<phShapeSphere>* >(&m_ShapeTest)->GetShape().GetNumHits();

	case BOUND_TEST:
		return reinterpret_cast<const phShapeTest<phShapeObject>* >(&m_ShapeTest)->GetShape().GetNumHits();

	case BOUNDING_BOX_TEST:
		return (reinterpret_cast<const CBoundingBoxShapeTestData* >(&m_ShapeTest)->m_pResults) ? reinterpret_cast<const CBoundingBoxShapeTestData* >(&m_ShapeTest)->m_pResults->m_nNumHits : 0;

	case BATCHED_TESTS:
	{
		const phShapeBatch& batchShape = reinterpret_cast<const phShapeTest<phShapeBatch>* >(&m_ShapeTest)->GetShape();
		if(batchShape.GetNumProbes() > 0)
		{
			return batchShape.GetProbe(shapeIndex).GetNumHits();
		}
		else if(batchShape.GetNumEdges() > 0)
		{
			return batchShape.GetEdge(shapeIndex).GetNumHits();
		}
		else
		{
			Assert(batchShape.GetNumSweptSpheres() > 0);
			return batchShape.GetSweptSphere(shapeIndex).GetNumHits();
		}
	}

	case INVALID_TEST_TYPE:
	default:
		physicsAssertf(false, "Non-RAGE shape test descriptor type");
		return 0;
	}	
}

template <typename ShapeType> void SetShapeTestCullResultsHelper(phShapeTest<ShapeType>* shapeTest, phShapeTestCullResults* pShapeTestCullResults, phShapeTestCull::State cullState)
{
	shapeTest->SetCullResults(pShapeTestCullResults);
	shapeTest->SetCullState(pShapeTestCullResults ? cullState : phShapeTestCull::DEFAULT);
}

void WorldProbe::CShapeTest::SetShapeTestCullResults(phShapeTestCullResults* pShapeTestCullResults, phShapeTestCull::State cullState)
{
	switch(GetShapeTestType())
	{
	case DIRECTED_LINE_TEST:
		SetShapeTestCullResultsHelper(reinterpret_cast<phShapeTest<phShapeProbe>* >(&m_ShapeTest),pShapeTestCullResults,cullState); break;

	case UNDIRECTED_LINE_TEST:
		SetShapeTestCullResultsHelper(reinterpret_cast<phShapeTest<phShapeEdge>* >(&m_ShapeTest),pShapeTestCullResults,cullState); break;

	case CAPSULE_TEST:
		SetShapeTestCullResultsHelper(reinterpret_cast<phShapeTest<phShapeCapsule>* >(&m_ShapeTest),pShapeTestCullResults,cullState); break;

	case SWEPT_SPHERE_TEST:
		SetShapeTestCullResultsHelper(reinterpret_cast<phShapeTest<phShapeSweptSphere>* >(&m_ShapeTest),pShapeTestCullResults,cullState); break;

	case SPHERE_TEST:
		SetShapeTestCullResultsHelper(reinterpret_cast<phShapeTest<phShapeSphere>* >(&m_ShapeTest),pShapeTestCullResults,cullState); break;

	case BOUND_TEST:
		SetShapeTestCullResultsHelper(reinterpret_cast<phShapeTest<phShapeObject>* >(&m_ShapeTest),pShapeTestCullResults,cullState); break;

	case BATCHED_TESTS:
		SetShapeTestCullResultsHelper(reinterpret_cast<phShapeTest<phShapeBatch>* >(&m_ShapeTest),pShapeTestCullResults,cullState); break;

	case INVALID_TEST_TYPE:
	default:
		physicsAssertf(false, "Non-RAGE shape test descriptor type");
	}
}

bool WorldProbe::CShapeTest::IsBoundingBoxTestAHit() const
{
	physicsAssert(GetShapeTestType() == BOUNDING_BOX_TEST);
	CBoundingBoxShapeTestData const & boundingBoxTesterData = *reinterpret_cast< CBoundingBoxShapeTestData const* >( &m_ShapeTest );
	return (boundingBoxTesterData.m_nNumHits > 0);
}

bool WorldProbe::CShapeTest::IsDirected() const
{
	switch(m_eShapeTestType)
	{
	case UNDIRECTED_LINE_TEST:
	case CAPSULE_TEST:
		return false;

	case DIRECTED_LINE_TEST:
	case SWEPT_SPHERE_TEST:
	case BATCHED_TESTS:
		return true;

	default:
		Assert(false);
		return false;
	}
}

WorldProbe::CShapeTest::CShapeTest(const WorldProbe::CShapeTestProbeDesc& shapeTestDesc)
: m_eShapeTestType(shapeTestDesc.GetTestType())
, m_eShapeTestDomain(shapeTestDesc.m_eShapeTestDomain)
, m_bSortResults((shapeTestDesc.GetOptions()&LOS_NO_RESULT_SORTING) == 0)
, m_bIncludeTyres((shapeTestDesc.GetOptions()&LOS_TEST_VEH_TYRES) != 0)
{
	physicsAssert(shapeTestDesc.GetTestType() == WorldProbe::DIRECTED_LINE_TEST || shapeTestDesc.GetTestType() == WorldProbe::UNDIRECTED_LINE_TEST);

	physicsAssert(rage::FPIsFinite(shapeTestDesc.m_segProbe.A.x) && rage::FPIsFinite(shapeTestDesc.m_segProbe.A.y) && rage::FPIsFinite(shapeTestDesc.m_segProbe.A.z));
	physicsAssert(rage::FPIsFinite(shapeTestDesc.m_segProbe.B.x) && rage::FPIsFinite(shapeTestDesc.m_segProbe.B.y) && rage::FPIsFinite(shapeTestDesc.m_segProbe.B.z));

	int nNumIsectsForTest = shapeTestDesc.m_nNumIntersections;
	CShapeTestHitPoint* pIsectsForTest = nNumIsectsForTest ? &shapeTestDesc.m_pIsects[shapeTestDesc.m_nFirstFreeIntersectionIndex] : NULL;
	int Options = shapeTestDesc.GetOptions(); 

	if(shapeTestDesc.GetIsDirected())
	{
		phShapeTest<phShapeProbe>& probeTester = *(new(&m_ShapeTest) phShapeTest<phShapeProbe>());
		
		probeTester.InitProbe(shapeTestDesc.m_segProbe,pIsectsForTest,nNumIsectsForTest);

		probeTester.SetIncludeFlags(shapeTestDesc.m_nIncludeFlags);
		probeTester.SetTypeFlags(shapeTestDesc.m_nTypeFlags);
		probeTester.SetStateIncludeFlags(shapeTestDesc.m_nStateIncludeFlags);
		probeTester.SetTypeExcludeFlags(shapeTestDesc.m_nExcludeTypeFlags);
		probeTester.GetShape().SetIgnoreMaterialFlags(ConvertLosOptionsToIgnoreMaterialFlags(Options));
		probeTester.SetTreatPolyhedralBoundsAsPrimitives(shapeTestDesc.GetTreatPolyhedralBoundsAsPrimitives()); 

		if(m_eShapeTestDomain == TEST_IN_LEVEL)
		{
			probeTester.SetExcludeInstanceList(shapeTestDesc.m_aExcludeInstList.GetElements(), shapeTestDesc.m_aExcludeInstList.GetCount());
		}
		else //(m_eShapeTestDomain == TEST_AGAINST_INDIVIDUAL_OBJECTS)
		{
			physicsAssertf(shapeTestDesc.m_aExcludeInstList.GetCount()==0,
				"List of instances to exclude from this shape test is not empty. This will have no effect in this test mode.");
		}
	}
	else
	{
		phShapeTest<phShapeEdge>& edgeTester = *(new(&m_ShapeTest) phShapeTest<phShapeEdge>());

		edgeTester.InitEdge(shapeTestDesc.m_segProbe,pIsectsForTest,nNumIsectsForTest);

		edgeTester.SetIncludeFlags(shapeTestDesc.m_nIncludeFlags);
		edgeTester.SetTypeFlags(shapeTestDesc.m_nTypeFlags);
		edgeTester.SetStateIncludeFlags(shapeTestDesc.m_nStateIncludeFlags);
		edgeTester.SetTypeExcludeFlags(shapeTestDesc.m_nExcludeTypeFlags);
		edgeTester.GetShape().SetIgnoreMaterialFlags(ConvertLosOptionsToIgnoreMaterialFlags(Options));
		edgeTester.SetTreatPolyhedralBoundsAsPrimitives(shapeTestDesc.GetTreatPolyhedralBoundsAsPrimitives()); 

		if(m_eShapeTestDomain == TEST_IN_LEVEL)
		{
			edgeTester.SetExcludeInstanceList(shapeTestDesc.m_aExcludeInstList.GetElements(), shapeTestDesc.m_aExcludeInstList.GetCount());
		}
		else //(m_eShapeTestDomain == TEST_AGAINST_INDIVIDUAL_OBJECTS)
		{
			physicsAssertf(shapeTestDesc.m_aExcludeInstList.GetCount()==0,
				"List of instances to exclude from this shape test is not empty. This will have no effect in this test mode.");
		}
	}
}

WorldProbe::CShapeTest::CShapeTest(const WorldProbe::CShapeTestBatchDesc& shapeTestDesc)
: m_eShapeTestType(WorldProbe::BATCHED_TESTS)
, m_eShapeTestDomain(shapeTestDesc.m_eShapeTestDomain)
, m_bSortResults((shapeTestDesc.GetOptions()&LOS_NO_RESULT_SORTING) == 0)
, m_bIncludeTyres((shapeTestDesc.GetOptions() & LOS_TEST_VEH_TYRES) != 0)
{
	phShapeTest<phShapeBatch>& batchTester = *(new(&m_ShapeTest) phShapeTest<phShapeBatch>());

	// Specify the cull volume if set in the descriptor.
#if WORLD_PROBE_DEBUG
	if(WorldProbe::CShapeTestManager::ms_bUseCustomCullVolume)
	{
#endif //WORLD_PROBE_DEBUG
		if(shapeTestDesc.m_pCustomCullVolumeDesc)
		{
			const CCullVolumeCapsuleDesc* pCapsuleCullDesc = NULL;
			const CCullVolumeBoxDesc* pBoxCullDesc = NULL;
			switch(shapeTestDesc.m_pCustomCullVolumeDesc->GetTestType())
			{
			case CULL_VOLUME_CAPSULE:
				pCapsuleCullDesc = static_cast<const CCullVolumeCapsuleDesc*>(shapeTestDesc.m_pCustomCullVolumeDesc);
				m_CullShape.InitCull_Capsule(Vec3V(pCapsuleCullDesc->m_vStartPos), Vec3V(pCapsuleCullDesc->m_vAxis),
					ScalarV(pCapsuleCullDesc->m_fLength), ScalarV(pCapsuleCullDesc->m_fRadius));
				batchTester.GetShape().SetUserProvidedCullShape(m_CullShape);
#if WORLD_PROBE_DEBUG
				if(CShapeTestManager::ms_bDrawCullVolume)
				{
					grcDebugDraw::Capsule(Vec3V(pCapsuleCullDesc->m_vStartPos),
						Vec3V(pCapsuleCullDesc->m_vStartPos)+Vec3V(pCapsuleCullDesc->m_vAxis)*ScalarV(pCapsuleCullDesc->m_fLength),
						pCapsuleCullDesc->m_fRadius, Color_red, false, 100);
				}
#endif // WORLD_PROBE_DEBUG
				break;

			case CULL_VOLUME_SPHERE:
				physicsAssertf(false, "CULL_VOLUME_SPHERE not implemented yet!");
				break;

			case CULL_VOLUME_BOX:
				pBoxCullDesc = static_cast<const CCullVolumeBoxDesc*>(shapeTestDesc.m_pCustomCullVolumeDesc);
				m_CullShape.InitCull_Box(MATRIX34_TO_MAT34V(pBoxCullDesc->m_matBoxAxes), Vec3V(pBoxCullDesc->m_vBoxHalfWidths));
				batchTester.GetShape().SetUserProvidedCullShape(m_CullShape);
#if WORLD_PROBE_DEBUG
				if(CShapeTestManager::ms_bDrawCullVolume)
				{
					Matrix34 matBoxAxes = pBoxCullDesc->m_matBoxAxes;
					Vector3 vBoxHalfWidths = pBoxCullDesc->m_vBoxHalfWidths;
					grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(-vBoxHalfWidths), VECTOR3_TO_VEC3V(vBoxHalfWidths), MATRIX34_TO_MAT34V(matBoxAxes), Color_red, false, 100);
				}
#endif // WORLD_PROBE_DEBUG
				break;

			case INVALID_CULL_VOLUME_TYPE: // Intentional fall-through.
			default:
				physicsAssertf(false, "Invalid cull volume type: %d.", shapeTestDesc.m_pCustomCullVolumeDesc->GetTestType());
				break;
			}
		}
#if WORLD_PROBE_DEBUG
	}
#endif //WORLD_PROBE_DEBUG

	const u32 nNumLineTests=shapeTestDesc.m_nNumProbes;
	const u32 nNumCapsuleTests=shapeTestDesc.m_nNumCapsules;
	Assertf(!(nNumLineTests > 0 && nNumCapsuleTests > 0), "Heterogeneous batches are not supported.");

	if(nNumLineTests > 0)
	{
		// All probe tests must be directed or undirected (cannot mix types).
		bool bDirected = shapeTestDesc.m_pProbeDescriptors[0].GetIsDirected();

		// Dynamic allocations each batch test : (
		if(bDirected)
		{
			// Allocate space for the probes.
			batchTester.GetShape().AllocateProbes(nNumLineTests);
		}
		else
		{
			batchTester.GetShape().AllocateEdges(nNumLineTests);
		}

		// Add the individual probes to the RAGE phShapeTest<phShapeBatch> object.
		for(u32 probeIndex=0; probeIndex < nNumLineTests; ++probeIndex)
		{
			physicsAssertf(!shapeTestDesc.GetResultsStructure(), "WARNING: you must set results structures on each test in a batch but not the container");

			// Cache some useful stuff from the descriptor.
			const CShapeTestProbeDesc& refProbeDesc = shapeTestDesc.m_pProbeDescriptors[probeIndex];
			const CShapeTestResults& refResults = *refProbeDesc.m_pResults;
			const phSegment& refSegment = refProbeDesc.m_segProbe;

			physicsAssert(rage::FPIsFinite(refSegment.A.x) && rage::FPIsFinite(refSegment.A.y) && rage::FPIsFinite(refSegment.A.z));
			physicsAssert(rage::FPIsFinite(refSegment.B.x) && rage::FPIsFinite(refSegment.B.y) && rage::FPIsFinite(refSegment.B.z));

			// If we are going to use options for this test, use the static intersection array to store the maximum amount of hit information
			// so that we can sort or cull it later. Otherwise, just write directly into the results structure's intersection array.
			CShapeTestHitPoint* pIsectsForTest = &refResults.m_pResults[refProbeDesc.m_nFirstFreeIntersectionIndex];
			int nNumIsectsForTest = refProbeDesc.m_nNumIntersections;

			// Is this a line test or a directed probe test?
			// This will add a shapetest 

			if (bDirected)
			{
				batchTester.InitProbe(refSegment,pIsectsForTest,nNumIsectsForTest);
			}
			else
			{
				batchTester.InitEdge(refSegment,pIsectsForTest,nNumIsectsForTest);
			}
		}
	}
	else if(nNumCapsuleTests > 0)
	{
		// All capsule tests must be directed or undirected (cannot mix types).
		bool bDirected = shapeTestDesc.m_pCapsuleDescriptors[0].GetIsDirected();

		if (bDirected)
		{
			// Allocate space for the swept spheres.
			batchTester.GetShape().AllocateSweptSpheres(nNumCapsuleTests);
		}
		else
		{
			// Allocate space for the capsules.
			batchTester.GetShape().AllocateCapsules(nNumCapsuleTests);
		}

		// Add the individual capsules to the RAGE phShapeTest<phShapeBatch> object.
		for(u32 capsuleIndex=0; capsuleIndex < nNumCapsuleTests; ++capsuleIndex)
		{
			physicsAssertf(!shapeTestDesc.GetResultsStructure(), "WARNING: you must set results structures on each test in a batch but not the container");

			// Cache some useful stuff from the descriptor.
			const CShapeTestCapsuleDesc& refCapsuleDesc = shapeTestDesc.m_pCapsuleDescriptors[capsuleIndex];
			const CShapeTestResults& refResults = *refCapsuleDesc.m_pResults;
			const phSegment& refSegment = refCapsuleDesc.m_segProbe;

			physicsAssert(rage::FPIsFinite(refSegment.A.x) && rage::FPIsFinite(refSegment.A.y) && rage::FPIsFinite(refSegment.A.z));
			physicsAssert(rage::FPIsFinite(refSegment.B.x) && rage::FPIsFinite(refSegment.B.y) && rage::FPIsFinite(refSegment.B.z));

			// If we are going to use options for this test, use the static intersection array to store the maximum amount of hit information
			// so that we can sort or cull it later. Otherwise, just write directly into the results structure's intersection array.
			CShapeTestHitPoint* pIsectsForTest = &refResults.m_pResults[refCapsuleDesc.m_nFirstFreeIntersectionIndex];
			int nNumIsectsForTest = refCapsuleDesc.m_nNumIntersections;

			// This will add a shapetest 
			if (bDirected)
			{
				batchTester.InitSweptSphere(refSegment, refCapsuleDesc.m_fRadius, pIsectsForTest, nNumIsectsForTest);
			}
			else
			{
				batchTester.InitCapsule(refSegment,refCapsuleDesc.m_fRadius,pIsectsForTest,nNumIsectsForTest);
			}
		}
	}

	int Options = shapeTestDesc.GetOptions(); 
	batchTester.GetShape().SetIgnoreMaterialFlags(ConvertLosOptionsToIgnoreMaterialFlags(Options));
	batchTester.SetTreatPolyhedralBoundsAsPrimitives(shapeTestDesc.GetTreatPolyhedralBoundsAsPrimitives()); 
	batchTester.SetIncludeFlags(shapeTestDesc.m_nIncludeFlags);
	batchTester.SetTypeFlags(shapeTestDesc.m_nTypeFlags);
	batchTester.SetStateIncludeFlags(shapeTestDesc.m_nStateIncludeFlags);
	batchTester.SetTypeExcludeFlags(shapeTestDesc.m_nExcludeTypeFlags);

	if(m_eShapeTestDomain == TEST_IN_LEVEL)
	{
		batchTester.SetExcludeInstanceList(shapeTestDesc.m_aExcludeInstList.GetElements(), shapeTestDesc.m_aExcludeInstList.GetCount());
	}
	else //(m_eShapeTestDomain == TEST_AGAINST_INDIVIDUAL_OBJECTS)
	{
		physicsAssertf(shapeTestDesc.m_aExcludeInstList.GetCount()==0,
			"List of instances to exclude from this shape test is not empty. This will have no effect in this test mode.");
		physicsAssertf(false, "There is no gain from using a batch test in TEST_AGAINST_INDIVIDUAL_OBJECTS mode. Use CShapeTestProbeDesc.");
	}
}

WorldProbe::CShapeTest::CShapeTest(const WorldProbe::CShapeTestSphereDesc& shapeTestDesc)
: m_eShapeTestType(WorldProbe::SPHERE_TEST)
, m_eShapeTestDomain(shapeTestDesc.m_eShapeTestDomain)
, m_bSortResults((shapeTestDesc.GetOptions()&LOS_NO_RESULT_SORTING) == 0)
, m_bIncludeTyres(false)
{
	physicsAssert(rage::FPIsFinite(shapeTestDesc.m_vCentre.x) && rage::FPIsFinite(shapeTestDesc.m_vCentre.y) && rage::FPIsFinite(shapeTestDesc.m_vCentre.z));
	physicsAssertf(rage::FPIsFinite(shapeTestDesc.m_fRadius) && shapeTestDesc.m_fRadius>0.0f, "radius = %7.3f.", shapeTestDesc.m_fRadius);

	int nNumIsectsForTest = shapeTestDesc.m_nNumIntersections;
	CShapeTestHitPoint* pIsectsForTest = nNumIsectsForTest ? &shapeTestDesc.m_pIsects[shapeTestDesc.m_nFirstFreeIntersectionIndex] : NULL;

	phShapeTest<phShapeSphere>& sphereTester = *(new(&m_ShapeTest) phShapeTest<phShapeSphere>());
	
	int Options = shapeTestDesc.GetOptions(); 
	sphereTester.InitSphere(shapeTestDesc.m_vCentre,shapeTestDesc.m_fRadius,pIsectsForTest,nNumIsectsForTest);
	sphereTester.GetShape().SetIgnoreMaterialFlags(ConvertLosOptionsToIgnoreMaterialFlags(Options));
	sphereTester.SetTreatPolyhedralBoundsAsPrimitives(shapeTestDesc.GetTreatPolyhedralBoundsAsPrimitives()); 
	sphereTester.SetIncludeFlags(shapeTestDesc.m_nIncludeFlags);
	sphereTester.SetTypeFlags(shapeTestDesc.m_nTypeFlags);
	sphereTester.SetStateIncludeFlags(shapeTestDesc.m_nStateIncludeFlags);
	sphereTester.SetTypeExcludeFlags(shapeTestDesc.m_nExcludeTypeFlags);
	sphereTester.GetShape().SetTestBackFacingPolygons(shapeTestDesc.GetTestBackFacingPolygons());

	// Do we want to exclude more than a single phInst from the collision test?
	switch(shapeTestDesc.m_eShapeTestDomain)
	{

	case TEST_IN_LEVEL:
		sphereTester.SetExcludeInstanceList(shapeTestDesc.m_aExcludeInstList.GetElements(), shapeTestDesc.m_aExcludeInstList.GetCount());
		break;

	case TEST_AGAINST_INDIVIDUAL_OBJECTS:
		physicsAssertf(shapeTestDesc.m_aIncludeInstList.GetCount()>0, "List of instances to test against is empty in TEST_AGAINST_INDIVIDUAL_OBJECTS mode.");
		break;

	default:
		physicsAssertf(false, "Unrecognised domain setting for shape test (%d).", m_eShapeTestDomain);
		break;

	}
}

WorldProbe::CShapeTest::CShapeTest(const WorldProbe::CShapeTestCapsuleDesc& shapeTestDesc)
: m_eShapeTestType(shapeTestDesc.m_eShapeTestType)
, m_eShapeTestDomain(shapeTestDesc.m_eShapeTestDomain)
, m_bSortResults((shapeTestDesc.GetOptions()&LOS_NO_RESULT_SORTING) == 0)
, m_bIncludeTyres(false)
{
	physicsAssert(rage::FPIsFinite(shapeTestDesc.m_segProbe.A.x) && rage::FPIsFinite(shapeTestDesc.m_segProbe.A.y) && rage::FPIsFinite(shapeTestDesc.m_segProbe.A.z));
	physicsAssert(rage::FPIsFinite(shapeTestDesc.m_segProbe.B.x) && rage::FPIsFinite(shapeTestDesc.m_segProbe.B.y) && rage::FPIsFinite(shapeTestDesc.m_segProbe.B.z));
	physicsAssertf(rage::FPIsFinite(shapeTestDesc.m_fRadius) && shapeTestDesc.m_fRadius>0.0f, "radius = %7.3f.", shapeTestDesc.m_fRadius);

	int nNumIsectsForTest = shapeTestDesc.m_nNumIntersections;
	CShapeTestHitPoint* pIsectsForTest = nNumIsectsForTest ? &shapeTestDesc.m_pIsects[shapeTestDesc.m_nFirstFreeIntersectionIndex] : NULL;

	phSegment segment;
	segment.Set(shapeTestDesc.m_segProbe.A,shapeTestDesc.m_segProbe.B);

	if(shapeTestDesc.GetIsDirected())
	{
		phShapeTest<phShapeSweptSphere>& sweptSphereTester = *(new(&m_ShapeTest) phShapeTest<phShapeSweptSphere>());

		sweptSphereTester.InitSweptSphere(segment,shapeTestDesc.m_fRadius,pIsectsForTest,nNumIsectsForTest);
		sweptSphereTester.GetShape().SetTestInitialSphere(shapeTestDesc.GetDoInitialSphereCheck());
				
		int Options = shapeTestDesc.GetOptions(); 
		sweptSphereTester.GetShape().SetIgnoreMaterialFlags(ConvertLosOptionsToIgnoreMaterialFlags(Options));
		sweptSphereTester.SetTreatPolyhedralBoundsAsPrimitives(shapeTestDesc.GetTreatPolyhedralBoundsAsPrimitives()); 
		sweptSphereTester.SetIncludeFlags(shapeTestDesc.m_nIncludeFlags);
		sweptSphereTester.SetTypeFlags(shapeTestDesc.m_nTypeFlags);
		sweptSphereTester.SetStateIncludeFlags(shapeTestDesc.m_nStateIncludeFlags);
		sweptSphereTester.SetTypeExcludeFlags(shapeTestDesc.m_nExcludeTypeFlags);

		switch(shapeTestDesc.m_eShapeTestDomain)
		{
		case TEST_IN_LEVEL:
			sweptSphereTester.SetExcludeInstanceList(shapeTestDesc.m_aExcludeInstList.GetElements(), shapeTestDesc.m_aExcludeInstList.GetCount());
			break;

		case TEST_AGAINST_INDIVIDUAL_OBJECTS:
			physicsAssertf(shapeTestDesc.m_aExcludeInstList.GetCount()==0,
				"List of instances to exclude from this shape test is not empty. This will have no effect in this test mode.");
			break;

		default:
			physicsAssertf(false, "Unrecognised domain setting for shape test (%d).", m_eShapeTestDomain);
			break;
		}
	}
	else
	{
		phShapeTest<phShapeCapsule>& capsuleTester = *(new(&m_ShapeTest) phShapeTest<phShapeCapsule>());

		capsuleTester.InitCapsule(segment,shapeTestDesc.m_fRadius,pIsectsForTest,nNumIsectsForTest);
		capsuleTester.GetShape().SetIgnoreMaterialFlags(ConvertLosOptionsToIgnoreMaterialFlags(shapeTestDesc.GetOptions()));
		capsuleTester.SetIncludeFlags(shapeTestDesc.m_nIncludeFlags);
		capsuleTester.SetTypeFlags(shapeTestDesc.m_nTypeFlags);
		capsuleTester.SetStateIncludeFlags(shapeTestDesc.m_nStateIncludeFlags);
		capsuleTester.SetTypeExcludeFlags(shapeTestDesc.m_nExcludeTypeFlags);

		switch(shapeTestDesc.m_eShapeTestDomain)
		{
		case TEST_IN_LEVEL:
			capsuleTester.SetExcludeInstanceList(shapeTestDesc.m_aExcludeInstList.GetElements(), shapeTestDesc.m_aExcludeInstList.GetCount());
			break;

		case TEST_AGAINST_INDIVIDUAL_OBJECTS:
			physicsAssertf(shapeTestDesc.m_aExcludeInstList.GetCount()==0,
				"List of instances to exclude from this shape test is not empty. This will have no effect in this test mode.");
			break;

		default:
			physicsAssertf(false, "Unrecognised domain setting for shape test (%d).", m_eShapeTestDomain);
			break;
		}
	}
}

WorldProbe::CShapeTest::CShapeTest(const WorldProbe::CShapeTestBoundDesc& shapeTestDesc)
: m_eShapeTestType(WorldProbe::BOUND_TEST)
, m_eShapeTestDomain(shapeTestDesc.m_eShapeTestDomain)
, m_bSortResults((shapeTestDesc.GetOptions()&LOS_NO_RESULT_SORTING) == 0)
, m_bIncludeTyres(false)
{
	int nNumIsectsForTest = shapeTestDesc.m_nNumIntersections;
	CShapeTestHitPoint* pIsectsForTest = nNumIsectsForTest ? &shapeTestDesc.m_pIsects[shapeTestDesc.m_nFirstFreeIntersectionIndex] : NULL;

	phShapeTest<phShapeObject>& objectTester = *(new(&m_ShapeTest) phShapeTest<phShapeObject>());

	// Return no collision if we have been given a NULL phBound. This could happen if the bound is set from
	// a CEntity which doesn't have a phInst.
	if(shapeTestDesc.m_pBound == NULL)
	{
		// Initialize the base shapetest so we can look at that data safely. Maybe NULL bounds should be taken care of at a higher level?
		objectTester.GetShape().Init(pIsectsForTest,nNumIsectsForTest);
		objectTester.GetShape().SetBound(NULL);
		objectTester.GetShape().SetWorldTransform(*shapeTestDesc.m_pTransformMatrix);
		return;
	}

	objectTester.InitObject(*shapeTestDesc.m_pBound,*shapeTestDesc.m_pTransformMatrix,pIsectsForTest,nNumIsectsForTest);
	int Options = shapeTestDesc.GetOptions(); 
	objectTester.GetShape().SetIgnoreMaterialFlags(ConvertLosOptionsToIgnoreMaterialFlags(Options));
	ASSERT_ONLY(bool isBvhTest = (shapeTestDesc.m_pBound->GetType() == phBound::BVH) || (shapeTestDesc.m_pBound->GetType() == phBound::COMPOSITE && static_cast<const phBoundComposite*>(shapeTestDesc.m_pBound)->GetContainsBVH());)
	Assertf(!isBvhTest || shapeTestDesc.GetTreatPolyhedralBoundsAsPrimitives(),"Trying to perform BVH bound shapetests without treating polyhedral bounds as primitives isn't allowed.");
	objectTester.SetTreatPolyhedralBoundsAsPrimitives(shapeTestDesc.GetTreatPolyhedralBoundsAsPrimitives()); 
	objectTester.SetIncludeFlags(shapeTestDesc.m_nIncludeFlags);
	objectTester.SetTypeFlags(shapeTestDesc.m_nTypeFlags);
	objectTester.SetStateIncludeFlags(shapeTestDesc.m_nStateIncludeFlags);
	objectTester.SetTypeExcludeFlags(shapeTestDesc.m_nExcludeTypeFlags);

	// Do we want to exclude more than a single phInst from the collision test?
	switch(shapeTestDesc.m_eShapeTestDomain)
	{
	case TEST_IN_LEVEL:
		objectTester.SetExcludeInstanceList(shapeTestDesc.m_aExcludeInstList.GetElements(), shapeTestDesc.m_aExcludeInstList.GetCount());
		break;

	case TEST_AGAINST_INDIVIDUAL_OBJECTS:
		physicsAssertf(shapeTestDesc.m_aExcludeInstList.GetCount()==0,
			"List of instances to exclude from this shape test is not empty. This will have no effect in this test mode.");
		break;

	default:
		physicsAssertf(false, "Unrecognised domain setting for shape test (%d).", m_eShapeTestDomain);
		break;
	}
}

WorldProbe::CShapeTest::CShapeTest(const WorldProbe::CShapeTestBoundingBoxDesc& shapeTestDesc)
: m_eShapeTestType(WorldProbe::BOUNDING_BOX_TEST)
, m_eShapeTestDomain(shapeTestDesc.m_eShapeTestDomain)
, m_bSortResults((shapeTestDesc.GetOptions()&LOS_NO_RESULT_SORTING) == 0)
, m_bIncludeTyres(false)
{
	CBoundingBoxShapeTestData& boundingBoxTestData = *new(&m_ShapeTest) CBoundingBoxShapeTestData();

	physicsAssertf( (shapeTestDesc.m_nIncludeFlags & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) == 0 && (shapeTestDesc.m_nIncludeFlags & ArchetypeFlags::GTA_MAP_TYPE_MOVER) == 0, 
		"No point doing a bounding box test vs bounding box test against the world because its bounding box envelopes almost everything");

	boundingBoxTestData.m_pBound = shapeTestDesc.m_pBound;
	boundingBoxTestData.m_TransformMatrix = *shapeTestDesc.m_pTransformMatrix;
	boundingBoxTestData.m_nNumHits = 0;

	//Copy the data that we need to do the test
	if(physicsVerifyf(shapeTestDesc.m_pBound, "The descriptor for this test has a NULL phBound."))
	{
		boundingBoxTestData.m_pResults = shapeTestDesc.m_pResults;
		boundingBoxTestData.m_aExcludeInstList = shapeTestDesc.m_aExcludeInstList;
		boundingBoxTestData.m_nIncludeFlags  = shapeTestDesc.m_nIncludeFlags;
		boundingBoxTestData.m_nTypeFlags = shapeTestDesc.m_nTypeFlags;
		boundingBoxTestData.m_nExcludeTypeFlags = shapeTestDesc.m_nExcludeTypeFlags;
		boundingBoxTestData.m_nStateIncludeFlags = shapeTestDesc.m_nStateIncludeFlags;
		boundingBoxTestData.m_vExtendBBoxMin = shapeTestDesc.m_vExtendBBoxMin;
		boundingBoxTestData.m_vExtendBBoxMax = shapeTestDesc.m_vExtendBBoxMax;
	}
}

WorldProbe::CShapeTest::~CShapeTest()
{
	switch(m_eShapeTestType)
	{
	case DIRECTED_LINE_TEST:
		reinterpret_cast< phShapeTest<phShapeProbe>* >(&m_ShapeTest)->~phShapeTest<phShapeProbe>();
		break;

	case UNDIRECTED_LINE_TEST:
		reinterpret_cast< phShapeTest<phShapeEdge>* >(&m_ShapeTest)->~phShapeTest<phShapeEdge>();
		break;

	case CAPSULE_TEST:
		reinterpret_cast< phShapeTest<phShapeCapsule>* >(&m_ShapeTest)->~phShapeTest<phShapeCapsule>();
		break;

	case SWEPT_SPHERE_TEST:
		reinterpret_cast< phShapeTest<phShapeSweptSphere>* >(&m_ShapeTest)->~phShapeTest<phShapeSweptSphere>();
		break;

	case POINT_TEST:
		physicsAssertf(m_eShapeTestType != POINT_TEST, "Destructing a POINT_TEST, but we don't support them");
		break;

	case SPHERE_TEST:
		reinterpret_cast< phShapeTest<phShapeSphere>* >(&m_ShapeTest)->~phShapeTest<phShapeSphere>();
		break;

	case BOUND_TEST:
		reinterpret_cast< phShapeTest<phShapeObject>* >(&m_ShapeTest)->~phShapeTest<phShapeObject>();
		break;

	case BOUNDING_BOX_TEST:
		reinterpret_cast< CBoundingBoxShapeTestData* >(&m_ShapeTest)->~CBoundingBoxShapeTestData();
		break;

	case BATCHED_TESTS:
		reinterpret_cast< phShapeTest<phShapeBatch>* >(&m_ShapeTest)->~phShapeTest<phShapeBatch>();
		break;

	case INVALID_TEST_TYPE:
	default:
		physicsAssertf(false, "Unknown shape test type: %d.", m_eShapeTestType);
	}
}


int WorldProbe::CShapeTest::PerformDirectedLineTest()
{
	phShapeTest<phShapeProbe>& probeTester = *reinterpret_cast< phShapeTest<phShapeProbe>* >( &m_ShapeTest );

	int nNumProbeResults = probeTester.TestInLevel();

	if( m_bIncludeTyres && !(probeTester.GetShape().IsBooleanTest() && nNumProbeResults > 0))
	{
		const phInst* const * pExcludeInstanceList;
		int numExcludeEntities;
		probeTester.GetExcludeInstanceList(pExcludeInstanceList, numExcludeEntities);		
		nNumProbeResults = PerformLineAgainstVehicleTyresTest(probeTester.GetShape(), pExcludeInstanceList, numExcludeEntities, probeTester.GetCullResults());
	}

	return nNumProbeResults;
}

int WorldProbe::CShapeTest::PerformUndirectedLineTest()
{
	phShapeTest<phShapeEdge>& edgeTester = *reinterpret_cast< phShapeTest<phShapeEdge>* >( &m_ShapeTest );
	int nNumEdgeResults = edgeTester.TestInLevel();

	if( m_bIncludeTyres && !(edgeTester.GetShape().IsBooleanTest() && nNumEdgeResults > 0))
	{
		const phInst* const * pExcludeInstanceList;
		int numExcludeEntities;
		edgeTester.GetExcludeInstanceList(pExcludeInstanceList, numExcludeEntities);
		nNumEdgeResults = PerformLineAgainstVehicleTyresTest(edgeTester.GetShape(), pExcludeInstanceList, numExcludeEntities, edgeTester.GetCullResults());
	}

	return nNumEdgeResults;
}

int WorldProbe::CShapeTest::PerformBatchTest()
{
	phShapeTest<phShapeBatch>& batchTester = *reinterpret_cast< phShapeTest<phShapeBatch>* >( &m_ShapeTest );
	int nTotalNumProbeResults = batchTester.TestInLevel();

	Assert(!batchTester.GetShape().IsBooleanTest());

	if(m_bIncludeTyres)
	{
		for(int i = 0; i < batchTester.GetShape().GetNumProbes(); ++i)
		{
			phShapeProbe& probe = batchTester.GetShape().GetProbe(i);

			int nNumIsectsForTest = probe.GetMaxNumIntersections();

			// Number of intersections already used in the array for this probe.
			int nNumIsectOffset = probe.GetNumHits();

			const phInst* const * pExcludeInstanceList;
			int numExcludeEntities;
			batchTester.GetExcludeInstanceList(pExcludeInstanceList, numExcludeEntities);	

			if(nNumIsectOffset < nNumIsectsForTest)
			{
				int nTotalNumHitsThisProbe = PerformLineAgainstVehicleTyresTest(probe, pExcludeInstanceList, numExcludeEntities, batchTester.GetCullResults());
				// Subtract off the number of hits already recorded from the RAGE batch test to find the
				// number of hits specifically due to vehicle tyres.
				int nNumVehicleTyreHits = nTotalNumHitsThisProbe - nNumIsectOffset;
				nTotalNumProbeResults += nNumVehicleTyreHits;
			}
		}
		for(int i = 0; i < batchTester.GetShape().GetNumEdges(); ++i)
		{
			phShapeEdge& edge = batchTester.GetShape().GetEdge(i);
			int nNumIsectsForTest = edge.GetMaxNumIntersections();

			// Number of intersections already used in the array for this probe.
			int nNumIsectOffset = edge.GetNumHits();

			const phInst* const * pExcludeInstanceList;
			int numExcludeEntities;
			batchTester.GetExcludeInstanceList(pExcludeInstanceList, numExcludeEntities);	

			if(nNumIsectOffset < nNumIsectsForTest)
			{
				int nTotalNumHitsThisProbe = PerformLineAgainstVehicleTyresTest(edge, pExcludeInstanceList, numExcludeEntities, batchTester.GetCullResults());
				// Subtract off the number of hits already recorded from the RAGE batch test to find the
				// number of hits specifically due to vehicle tyres.
				int nNumVehicleTyreHits = nTotalNumHitsThisProbe - nNumIsectOffset;
				nTotalNumProbeResults += nNumVehicleTyreHits;
			}
		}
	}

	return nTotalNumProbeResults;
}

int WorldProbe::CShapeTest::PerformSphereTest()
{
	return reinterpret_cast< phShapeTest<phShapeSphere>* >( &m_ShapeTest )->TestInLevel();
}

int WorldProbe::CShapeTest::PerformCapsuleTest()
{
	return reinterpret_cast< phShapeTest<phShapeCapsule>* >( &m_ShapeTest )->TestInLevel();
}

int WorldProbe::CShapeTest::PerformSweptSphereTest()
{
	return reinterpret_cast< phShapeTest<phShapeSweptSphere>* >( &m_ShapeTest )->TestInLevel();
}

int WorldProbe::CShapeTest::PerformBoundTest()
{
	phShapeTest<phShapeObject>& objectTester = *reinterpret_cast< phShapeTest<phShapeObject>* >( &m_ShapeTest );

	if(objectTester.GetShape().GetBound() != NULL)
	{
#if __ASSERT
		const phBound* pBound = objectTester.GetShape().GetBound();
		u32 bvhTypes = (ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_BVH_TYPE);
		if(phBound::IsTypeComposite(pBound->GetType()))
		{
			const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);
			if(pBoundComposite->GetTypeAndIncludeFlags() != NULL)
			{
				for(int componentIndex = 0; componentIndex < pBoundComposite->GetNumBounds(); ++componentIndex)
				{
					if(const phBound* pComponentBound = pBoundComposite->GetBound(componentIndex))
					{
						u32 componentIncludeFlags = pBoundComposite->GetIncludeFlags(componentIndex);
						Assertf(pComponentBound->IsConvex() || (componentIncludeFlags & bvhTypes) == 0, "Trying to perform a bound test with a concave bound that will likely hit other concave bounds.");
					}
				}
			}
		}
		else
		{
			Assertf(pBound->IsConvex() || (objectTester.GetIncludeFlags() & bvhTypes) == 0, "Trying to perform a bound test with a concave bound that will likely hit other concave bounds.");
		}
#endif // __ASSERT
		
		return objectTester.TestInLevel();
	}
	else
	{
		return 0;
	}
}

int WorldProbe::CShapeTest::PerformBoundingBoxTest()
{
	Assertf(m_eShapeTestDomain != TEST_AGAINST_INDIVIDUAL_OBJECTS, "Bounding box tests aren't supported against individual objects.");
	CBoundingBoxShapeTestData& boundingBoxTesterData = *reinterpret_cast< CBoundingBoxShapeTestData* >( &m_ShapeTest );

	if( boundingBoxTesterData.m_pBound == NULL )
	{
		return 0;
	}

	// Compute or obtain the properties of the bounding box.
	const float fRadius = boundingBoxTesterData.m_pBound->GetRadiusAroundCentroid();
	Vector3 vCentre = VEC3V_TO_VECTOR3(boundingBoxTesterData.m_pBound->GetWorldCentroid(RCC_MAT34V(boundingBoxTesterData.m_TransformMatrix)));

	// Initialise the RAGE bound cull iterator.
#if ENABLE_PHYSICS_LOCK
	phIterator iterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
	phIterator iterator;
#endif	// ENABLE_PHYSICS_LOCK
	iterator.InitCull_Sphere(vCentre, fRadius);
	iterator.SetIncludeFlags(boundingBoxTesterData.m_nIncludeFlags);
	iterator.SetTypeFlags(boundingBoxTesterData.m_nTypeFlags);
	iterator.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
	iterator.SetTypeExcludeFlags(boundingBoxTesterData.m_nExcludeTypeFlags);

	Mat34V& transformMatrix = RC_MAT34V(boundingBoxTesterData.m_TransformMatrix);
	Vec3V vBoundMin = Transform(transformMatrix, boundingBoxTesterData.m_pBound->GetBoundingBoxMin());
	Vec3V vBoundMax = Transform(transformMatrix, boundingBoxTesterData.m_pBound->GetBoundingBoxMax());

	// Loop over all the objects within the cull sphere using the iterator.
	u16 culledLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(iterator);
	CShapeTestResults* pResults = boundingBoxTesterData.m_pResults;
	int iNumFound = 0;
	bool bFillResults = (pResults && pResults->m_nNumResults > 0);
	const int iMaxToFind = (bFillResults) ? pResults->m_nNumResults : 1;
	while(culledLevelIndex != phInst::INVALID_INDEX && iNumFound < iMaxToFind)
	{
		phInst* pCulledInstance = CPhysics::GetLevel()->GetInstance(culledLevelIndex);
		physicsAssert(pCulledInstance);

		// Search through the list of excluded instances to see whether we should test
		// for collision with this culled object.
		bool bThisObjectExcluded = false;
		for(int i = 0; i < boundingBoxTesterData.m_aExcludeInstList.GetCount(); ++i)
		{
			if(pCulledInstance == boundingBoxTesterData.m_aExcludeInstList[i])
			{
				bThisObjectExcluded = true;
				break;
			}
		}

		if(!bThisObjectExcluded)
		{
			physicsAssert(pCulledInstance->GetArchetype());
			const phBound* pCulledBound = pCulledInstance->GetArchetype()->GetBound();
			physicsAssert(pCulledBound);

			Vector3 halfWidth1(ORIGIN), boxCenter1;
			boundingBoxTesterData.m_pBound->GetBoundingBoxHalfWidthAndCenter(RC_VEC3V(halfWidth1),RC_VEC3V(boxCenter1));

			// Option to expand the bounding box check.
			physicsAssert(boundingBoxTesterData.m_vExtendBBoxMin.x <= 0.0f && boundingBoxTesterData.m_vExtendBBoxMin.y <= 0.0f && boundingBoxTesterData.m_vExtendBBoxMin.z <= 0.0f);
			halfWidth1.Subtract(boundingBoxTesterData.m_vExtendBBoxMin * VEC3_HALF);
			boxCenter1.Add(boundingBoxTesterData.m_vExtendBBoxMin * VEC3_HALF);

			halfWidth1.Add(boundingBoxTesterData.m_vExtendBBoxMax * VEC3_HALF);
			boxCenter1.Add(boundingBoxTesterData.m_vExtendBBoxMax * VEC3_HALF);

			Vector3 halfWidth2(ORIGIN), boxCenter2;
			pCulledBound->GetBoundingBoxHalfWidthAndCenter(RC_VEC3V(halfWidth2),RC_VEC3V(boxCenter2));

			// Move and expand the matrix for the composite bound part's bounding box to include both this and the previous frame.
			//geomBoxes::ExpandOBBFromMotion(*pMatrix,*pLastMatrix,halfWidth1,boxCenter1);

			// Get the world matrix for the composite bound part's bounding box.
			Matrix34 partWorldCenter1;
			partWorldCenter1.Set3x3(boundingBoxTesterData.m_TransformMatrix);
			boundingBoxTesterData.m_TransformMatrix.Transform(boxCenter1, partWorldCenter1.d);

			// Move and expand the matrix for the other object's bounding box to include both this and the previous frame.
			Mat34V_ConstRef currentMtx = pCulledInstance->GetMatrix();
			Mat34V_ConstRef lastMtx = PHSIM->GetLastInstanceMatrix(pCulledInstance);
			geomBoxes::ExpandOBBFromMotion(currentMtx, lastMtx, RC_VEC3V(halfWidth2), RC_VEC3V(boxCenter2));

			// Get the world matrix for the other object's bounding box.
			Matrix34 partWorldCenter2;
			partWorldCenter2.Set3x3(RCC_MATRIX34(pCulledInstance->GetMatrix()));
			RCC_MATRIX34(pCulledInstance->GetMatrix()).Transform(boxCenter2, partWorldCenter2.d);

#if __PPU || __XENON
			halfWidth1.And(VEC3_ANDW);
			halfWidth2.And(VEC3_ANDW);
#elif VECTORIZED_PADDING
			halfWidth1.w = 0.0f;
			halfWidth2.w = 0.0f;
#endif

			// Compute the matrix that transforms from the composite bound part's coordinate system to the other object's coordinate system.
			Matrix34 matrix;
			matrix.DotTranspose(partWorldCenter1, partWorldCenter2);

			// Perform the test to see if the bounding boxes overlap.
			if(geomBoxes::TestBoxToBoxOBBFaces(halfWidth1, halfWidth2, matrix))
			{
				if (bFillResults)
				{
					physicsAssert(boundingBoxTesterData.m_pResults && boundingBoxTesterData.m_pResults->m_nNumResults > iNumFound);
					Vec3V vHitPosition = Clamp(pCulledInstance->GetPosition(), vBoundMin, vBoundMax);
					boundingBoxTesterData.m_pResults->m_pResults[iNumFound].SetHitLevelIndex(culledLevelIndex);
					boundingBoxTesterData.m_pResults->m_pResults[iNumFound].SetHitGenerationId(CPhysics::GetLevel()->GetGenerationID(culledLevelIndex));
					boundingBoxTesterData.m_pResults->m_pResults[iNumFound].SetPosition(vHitPosition);
				}
				iNumFound++; 
			}
		}
		culledLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(iterator);
	}
	boundingBoxTesterData.m_nNumHits = iNumFound;
	return iNumFound;
}
