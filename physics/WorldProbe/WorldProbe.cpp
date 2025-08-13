// This file contains some helper functions declared directly in the WorldProbe namespace. The eventual
// plan would be to ship this stuff into more appropriate files.


// Rage headers
#include "grcore/debugdraw.h"
#include "physics/collision.h"
#include "physics/inst.h"
#include "physics/intersection.h"
#include "phbound/boundbvh.h"
#include "phbound/boundbox.h"
#include "phbound/boundcomposite.h"
#include "phbound/boundGeomSecondSurface.h"
#include "system/task.h"
#include "spatialdata/sphere.h"
#include "fwsys/gameskeleton.h"
#include "entity\transform.h"

// Framework headers

// Game headers
#include "debug/debugscene.h"
#include "peds/ped.h"
#include "modelinfo/PedModelInfo.h"
#include "physics/gtaArchetype.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/iterator.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "physics/levelnew.h"
#include "scene/entity.h"
#include "scene/physical.h"
#include "vehicles/Automobile.h"
#include "vehicles/Boat.h"
#include "vehicles/vehicle.h"
#include "vehicles/wheel.h"
#include "scene/world/GameWorld.h"

PHYSICS_OPTIMISATIONS()

PF_PAGE(GTA_WorldProbe, "Gta WorldProbe");
PF_GROUP(Probes);
PF_LINK(GTA_WorldProbe, Probes);
PF_TIMER(Total_Probes, Probes);
PF_TIMER(TestCapsuleVsEntity, Probes);
PF_TIMER(GeneralAI_Probes, Probes);
PF_TIMER(CombatAI_Probes, Probes);
PF_TIMER(SeekCoverAI_Probes, Probes);
PF_TIMER(SeperateAI_Probes, Probes);
PF_TIMER(FindCoverAI_Probes, Probes);
PF_TIMER(Camera_Probes, Probes);
PF_TIMER(Audio_Probes, Probes);
PF_TIMER(Weapon_Probes, Probes);
PF_TIMER(Unspecified_Probes, Probes);


#if RECORD_SHAPE_TEST_TASK_MANAGER_INTERACTION
	#include "atl\array.h"
	#define MAX_SHAPE_TEST_TASK_HANDLES_RECORDED (50) //Only really needs to be MAX_ASYNCHRONOUS_PROCESSING_SHAPE_TESTS, but this will count leakage
	atFixedArray<sysTaskHandle, MAX_SHAPE_TEST_TASK_HANDLES_RECORDED> g_DebugRecordTaskManagerInteractionShapeTestHandles;
	u32 g_DebugRecordTaskManagerInteractionShapeTestHandlesHighWaterMark = 0u;
	static unsigned int gDispatchThreadLoopCounter = 0u;
#endif


phMaterialMgrGta::Id WorldProbe::ConvertLosOptionsToIgnoreMaterialFlags(int losOptions)
{
	phMaterialMgrGta::Id ignoreMaterialFlags = 0;
	if(losOptions&LOS_IGNORE_SHOOT_THRU_FX)
	{
		ignoreMaterialFlags |= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_SHOOT_THROUGH_FX);
	}

	if(losOptions&(LOS_IGNORE_SHOOT_THRU | LOS_IGNORE_POLY_SHOOT_THRU | LOS_GO_THRU_GLASS))
	{
		ignoreMaterialFlags |= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_SHOOT_THROUGH);
	}

	if(losOptions&LOS_IGNORE_SEE_THRU)
	{
		ignoreMaterialFlags |= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_SEE_THROUGH);
	}

	if(losOptions&LOS_IGNORE_NO_CAM_COLLISION)
	{
		ignoreMaterialFlags |= (PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION) | PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING));
	}

	if(losOptions&LOS_IGNORE_NOT_COVER)
	{
		ignoreMaterialFlags |= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NOT_COVER);
	}

	return ignoreMaterialFlags;
}

#if __BANK
const char* WorldProbe::GetShapeTypeString(WorldProbe::eShapeTestType eTestType)
{
	switch(eTestType)
	{
	case DIRECTED_LINE_TEST:
		return "Directed Line Test";
	case UNDIRECTED_LINE_TEST:
		return "Undirected Line Test";
	case CAPSULE_TEST:
		return "Capsule Test";
	case SWEPT_SPHERE_TEST:
		return "Swept Sphere Test";
	case POINT_TEST:
		return "Point Test";
	case SPHERE_TEST:
		return "Sphere Test";
	case BOUND_TEST:
		return "Bound Test";
	case BOUNDING_BOX_TEST:
		return "Bounding Box Test";
	case BATCHED_TESTS:
		return "Batched Test";
	default:
	case INVALID_TEST_TYPE:
		return "Invalid Test";
	}
}

const char* WorldProbe::GetContextString(ELosContext eContext)
{
	switch(eContext)
	{
	case LOS_GeneralAI:
		return "General AI";
	case LOS_CombatAI:
		return "Combat AI";
	case LOS_Camera:
		return "Camera";
	case LOS_Audio:
		return "Audio";
	case LOS_Unspecified:
		return "Unspecified";
	case LOS_SeekCoverAI:
		return "Seek Cover AI";
	case LOS_SeparateAI:
		return "Separate AI";
	case LOS_FindCoverAI:
		return "Find cover AI";
	case LOS_Weapon:
		return "Weapon AI";
	case EPedTargetting:
		return "Ped targeting";
	case EPedAcquaintances:
		return "Ped acquaintances";
	case ELosCombatAI:
		return "LoS combat AI";
	case EPlayerCover:
		return "Player cover";
	case EAudioOcclusion:
		return "Audio occlusion";
	case EAudioLocalEnvironment:
		return "Audio local environment";
	case EMovementAI:
		return "Movement AI";
	case EScript:
		return "Script";
	case EHud:
		return "HUD";
	case EMelee:
		return "Melee";
	case ENetwork:
		return "Network";
	case ENotSpecified:
		return "Not specified (async)";
	case ERiverBoundQuery:
		return "River bound query";			
	case EVehicle:
		return "Vehicle";
	default:
		return "Invalid context";
	}
}
#endif // __BANK

// Statics:
#if WORLD_PROBE_DEBUG
void WorldProbe::StartProbeTimer(const WorldProbe::ELosContext eContext)
{
	PF_START(Total_Probes);
	switch(eContext)
	{
	case WorldProbe::LOS_GeneralAI:
		PF_START(GeneralAI_Probes);
		break;
	case WorldProbe::LOS_CombatAI:
		PF_START(CombatAI_Probes);
		break;
	case WorldProbe::LOS_SeekCoverAI:
		PF_START(SeekCoverAI_Probes);
		break;
	case WorldProbe::LOS_SeparateAI:
		PF_START(SeperateAI_Probes);
		break;
	case WorldProbe::LOS_FindCoverAI:
		PF_START(FindCoverAI_Probes);
		break;
	case WorldProbe::LOS_Camera:
		PF_START(Camera_Probes);
		break;
	case WorldProbe::LOS_Audio:
		PF_START(Audio_Probes);
		break;
	case WorldProbe::LOS_Weapon:
		PF_START(Weapon_Probes);
		break;
	case WorldProbe::LOS_Unspecified:
	default:
		PF_START(Unspecified_Probes);
		break;
	}
}
void WorldProbe::StopProbeTimer(const WorldProbe::ELosContext eContext)
{
	PF_STOP(Total_Probes);
	switch(eContext)
	{
	case WorldProbe::LOS_GeneralAI:
		PF_STOP(GeneralAI_Probes);
		break;
	case WorldProbe::LOS_CombatAI:
		PF_STOP(CombatAI_Probes);
		break;
	case WorldProbe::LOS_SeekCoverAI:
		PF_STOP(SeekCoverAI_Probes);
		break;
	case WorldProbe::LOS_SeparateAI:
		PF_STOP(SeperateAI_Probes);
		break;
	case WorldProbe::LOS_FindCoverAI:
		PF_STOP(FindCoverAI_Probes);
		break;
	case WorldProbe::LOS_Camera:
		PF_STOP(Camera_Probes);
		break;
	case WorldProbe::LOS_Audio:
		PF_STOP(Audio_Probes);
		break;
	case WorldProbe::LOS_Weapon:
		PF_STOP(Weapon_Probes);
		break;
	case WorldProbe::LOS_Unspecified:
	default:
		PF_STOP(Unspecified_Probes);
		break;
	}
}
#endif // WORLD_PROBE_DEBUG





// Name			:	FindGroundZForCoord
// Purpose		:	Finds the highest bit of road for a given X and Y
// Parameters	:	X, Y
// Returns		:	Highest Z
#if WORLD_PROBE_DEBUG
float WorldProbe::DebugFindGroundZForCoord(const char *strCodeFile, u32 nCodeLine, const float UNUSED_PARAM(fSecondSurfaceInterp), const float x, const float y, const ELosContext eContext, WorldProbe::CShapeTestResults* pShapeTestResults)
#else
float WorldProbe::FindGroundZForCoord(const float UNUSED_PARAM(fSecondSurfaceInterp), const float x, const float y, const ELosContext eContext, WorldProbe::CShapeTestResults* pShapeTestResults)
#endif
{
	Vector3 TestCoors;
	bool ReturnValue;

	ReturnValue = FALSE;

	TestCoors.x = x;
	TestCoors.y = y;
	TestCoors.z = 1200.0f;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(TestCoors, Vector3(TestCoors.x, TestCoors.y, -1200.0f));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probeDesc.SetContext(eContext);

	float fHeight=0;
	if(pShapeTestResults)
	{
		probeDesc.SetResultsStructure(pShapeTestResults);
#if WORLD_PROBE_DEBUG
		ReturnValue = WorldProbe::GetShapeTestManager()->DebugSubmitTest(strCodeFile, nCodeLine, probeDesc);
#else
		ReturnValue = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
#endif
		if(ReturnValue)
		{
			fHeight=(*pShapeTestResults)[0].GetHitPosition().z;
		}
	}
	else
	{
		WorldProbe::CShapeTestHitPoint probeIsect;
		WorldProbe::CShapeTestResults probeResult(probeIsect);
		probeDesc.SetResultsStructure(&probeResult);
#if WORLD_PROBE_DEBUG
		ReturnValue = WorldProbe::GetShapeTestManager()->DebugSubmitTest(strCodeFile, nCodeLine, probeDesc);
#else
		ReturnValue = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
#endif
		if(ReturnValue)
		{
			fHeight=probeResult[0].GetHitPosition().z;
		}
	}

	if (!ReturnValue)
	{
		//		printf("THERE IS NO MAP AT THE FOLLOWING COORS:%f %f. (FindGroundZForCoord)\n",
		//						TestCoors.x, TestCoors.y);
		return (20.0f);
	}

	return fHeight;
}

// Name			:	FindGroundZFor3DCoord
// Purpose		:	Finds the highest bit of road for a given X, Y and Z
// Parameters	:	X, Y, Z
// Returns		:	Highest Z below the given Z
#if WORLD_PROBE_DEBUG
float WorldProbe::DebugFindGroundZFor3DCoord(const char *strCodeFile, u32 nCodeLine, 
											  const float fSecondSurfaceInterp, const float x, const float y, const float z, bool *pBool, Vector3* pNormal, phInst **ppInst, const ELosContext eContext)
#else
float WorldProbe::FindGroundZFor3DCoord(const float fSecondSurfaceInterp, const float x, const float y, const float z, bool *pBool,
										 Vector3* pNormal, phInst **ppInst, const ELosContext eContext)
#endif
{
	Vector3 vTestCoords;
	vTestCoords.x = x;
	vTestCoords.y = y;
	vTestCoords.z = z;

#if WORLD_PROBE_DEBUG
	return DebugFindGroundZFor3DCoord(strCodeFile, nCodeLine, fSecondSurfaceInterp, vTestCoords, pBool, pNormal, ppInst, eContext);
#else
	return FindGroundZFor3DCoord(fSecondSurfaceInterp, vTestCoords, pBool, pNormal, ppInst, eContext);
#endif
}

// Name			:	FindGroundZFor3DCoord
// Purpose		:	Finds the highest bit of road for a given X, Y and Z
// Parameters	:	X, Y, Z
// Returns		:	Highest Z below the given Z
#if WORLD_PROBE_DEBUG
float WorldProbe::DebugFindGroundZFor3DCoord(const char *strCodeFile, u32 nCodeLine, 
											  const float UNUSED_PARAM(fSecondSurfaceInterp), const Vector3 &vTestCoords, bool *pBool, Vector3* pNormal, 
											  phInst **ppInst, const ELosContext eContext)
#else
float WorldProbe::FindGroundZFor3DCoord(const float UNUSED_PARAM(fSecondSurfaceInterp), const Vector3& vTestCoords, bool *pBool, Vector3* pNormal,
										 phInst **ppInst, const ELosContext eContext)
#endif	// WORLD_PROBE_DEBUG
{
	bool ReturnValue;

	ReturnValue = FALSE;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestHitPoint probeIsect;
	WorldProbe::CShapeTestResults probeResult(probeIsect);
	probeDesc.SetStartAndEnd(vTestCoords, Vector3(vTestCoords.x, vTestCoords.y, -1000.0f));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probeDesc.SetContext(eContext);

#if WORLD_PROBE_DEBUG
		ReturnValue = WorldProbe::GetShapeTestManager()->DebugSubmitTest(strCodeFile, nCodeLine, probeDesc);
#else
		ReturnValue = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
#endif

	if (!ReturnValue)
	{
		if(pBool) *pBool = false;
		if(ppInst) *ppInst = NULL;
		if(pNormal) *pNormal = Vector3(0.0f,0.0f,0.0f);
		return (0.0f);
	}

	if(pBool) *pBool = true;
	if(ppInst) *ppInst = probeResult[0].GetHitInst();
	if(pNormal) *pNormal = probeResult[0].GetHitNormal();

	return(probeResult[0].GetHitPosition().z);
}



#if ENABLE_ASYNC_STRESS_TEST
void WorldProbe::DebugAsyncStressTestOnce()
{
	u32 numToInsert = CShapeTestManager::ms_uStressTestProbeCount;
	u32 numInserted = 0u;
	bool bSubmissionFailed = false;

	for (int i = 0 ; i < MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS && numInserted < numToInsert ; i++)
	{
		if(CShapeTestManager::ms_DebugShapeTestResults[i].GetWaitingOnResults())
		{
			continue;
		}

		CShapeTestManager::ms_DebugShapeTestResults[i].Reset();

		// Generate some random start and end positions for the probes.
		CPed * pPed = FindPlayerPed();
		if(!pPed)
			return;
		Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vStartPos;
		Vector3 vEndPos;
		const float fMaxDist = 100.0f;
		do
		{
			vStartPos = Vector3(
				fwRandom::GetRandomNumberInRange(-1.0f, 1.0f),
				fwRandom::GetRandomNumberInRange(-1.0f, 1.0f),
				fwRandom::GetRandomNumberInRange(-1.0f, 1.0f)
				);
		} while (Abs(vStartPos.Mag2())<0.01f);

		do
		{
			vEndPos = Vector3(
				fwRandom::GetRandomNumberInRange(-1.0f, 1.0f),
				fwRandom::GetRandomNumberInRange(-1.0f, 1.0f),
				fwRandom::GetRandomNumberInRange(-1.0f, 1.0f)
				);
		} while (Abs(vEndPos.Mag2())<0.01f);

		vStartPos.Normalize();
		vEndPos.Normalize();
		vStartPos *= fMaxDist;
		vEndPos *= fMaxDist;
		vStartPos += vPos;
		vEndPos += vPos;

		WorldProbe::CShapeTestProbeDesc probe;
		probe.SetResultsStructure(&CShapeTestManager::ms_DebugShapeTestResults[i]);
		probe.SetStartAndEnd(vStartPos, vEndPos);
		probe.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_WEAPON);
		probe.SetOptions(0);
		probe.SetIsDirected(true);
		WorldProbe::GetShapeTestManager()->SubmitTest(probe, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

		if(CShapeTestManager::ms_DebugShapeTestResults[i].GetResultsStatus() == WorldProbe::SUBMISSION_FAILED)
		{
			bSubmissionFailed = true;
			break;
		}
		else
		{
			numInserted++;
		}
	}

	if(numInserted < numToInsert)
	{
		physicsWarningf("DebugAsyncStressTestOnce could only submit %u of %u requested tests. bSubmissionFailed=%d", numInserted, numToInsert,(int)bSubmissionFailed);
	}

	if(CShapeTestManager::ms_bAbortAllTestsInReverseOrderImmediatelyAfterStarting)
	{
		for (int i = MAX_ASYNCHRONOUS_REQUESTED_SHAPE_TESTS-1 ; i >= 0 ; i--)
		{
			CShapeTestManager::ms_DebugShapeTestResults[i].Reset();
		}
	}
}

#endif //ENABLE_ASYNC_STRESS_TEST
