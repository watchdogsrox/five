// Rage /framework headers
#include "script_helper.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/wrapper.h"
#include "phbound\boundbox.h"
#include "camera/viewports/ViewportManager.h"

// Game headers
#include "script\script_shapetests.h"
#include "commands_shapetest.h"


namespace shapetest_commands
{

//-----------------------------------------------------------------------------

//Returns script GUID for the shapetest, or 0 if initiation failed (for example if there was no room for queuing more shape tests)
ScriptShapeTestGuid CommandStartShapeTestLOSProbe(const scrVector & scrVecStartPos, const scrVector & scrVecEndPos, s32 LOSFlags, int nExcludeEntityIndex,
												  s32 nOptionFlags)
{
	CPhysical* pEntity = NULL;
	if(nExcludeEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
		pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(nExcludeEntityIndex));

	WorldProbe::CShapeTestProbeDesc probeParams;
	probeParams.SetStartAndEnd(Vector3(scrVecStartPos), Vector3(scrVecEndPos));
	if(pEntity)
		probeParams.SetExcludeEntity(pEntity);
	//probeParams.SetIsDirected(false);
	probeParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	probeParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	probeParams.SetContext(WorldProbe::EScript);
	probeParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	return CScriptShapeTestManager::CreateShapeTestRequest(probeParams, false /* Synchronous? */);
}

// Synchronous version of CommandStartShapeTestLOSProbe(). USE SPARINGLY!!
ScriptShapeTestGuid CommandStartExpensiveSynchronousShapeTestLOSProbe(const scrVector & scrVecStartPos, const scrVector & scrVecEndPos, s32 LOSFlags,
																	  int nExcludeEntityIndex, s32 nOptionFlags)
{
	CPhysical* pEntity = NULL;
	if(nExcludeEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
		pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(nExcludeEntityIndex));

	WorldProbe::CShapeTestProbeDesc probeParams;
	probeParams.SetStartAndEnd(Vector3(scrVecStartPos), Vector3(scrVecEndPos));
	if(pEntity)
		probeParams.SetExcludeEntity(pEntity);
	//probeParams.SetIsDirected(false);
	probeParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	probeParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	probeParams.SetContext(WorldProbe::EScript);
	probeParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	return CScriptShapeTestManager::CreateShapeTestRequest(probeParams, true /* Synchronous? */);
}

ScriptShapeTestGuid CommandStartShapeTestSphere(const scrVector & scrVecPos, float fRadius, s32 LOSFlags, int nExcludeEntityIndex, s32 nOptionFlags)
{
	CPhysical* pEntity = NULL;
	if(nExcludeEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
		pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(nExcludeEntityIndex));

	WorldProbe::CShapeTestSphereDesc sphereParams;
	sphereParams.SetSphere(Vector3(scrVecPos), fRadius);
	if(pEntity)
		sphereParams.SetExcludeEntity(pEntity);
	sphereParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	sphereParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	sphereParams.SetContext(WorldProbe::EScript);
	sphereParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	return CScriptShapeTestManager::CreateShapeTestRequest(sphereParams, false /* Synchronous? */);
}

ScriptShapeTestGuid CommandStartShapeTestBoundingBox(int EntityIndex, s32 LOSFlags, s32 nOptionFlags)
{
	const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
	if(pEntity == NULL) return 0;

	const phInst* inst = pEntity->GetCurrentPhysicsInst();
	if(inst == NULL) return 0;

	WorldProbe::CShapeTestBoundingBoxDesc boxParams;
	boxParams.SetBound(inst->GetArchetype()->GetBound());
	boxParams.SetTransform(&RCC_MATRIX34(inst->GetMatrix()));
	boxParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	boxParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	boxParams.SetExcludeEntity(pEntity);
	boxParams.SetContext(WorldProbe::EScript);
	boxParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	return CScriptShapeTestManager::CreateShapeTestRequest(boxParams, true /* Synchronous? */);
}

ScriptShapeTestGuid CommandStartShapeTestBound(int EntityIndex, s32 LOSFlags, s32 nOptionFlags)
{
	const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
	if(pEntity == NULL) return 0;

	const phInst* inst = pEntity->GetCurrentPhysicsInst();
	if(inst == NULL) return 0;

	WorldProbe::CShapeTestBoundDesc boundTestParams;
	boundTestParams.SetBound(inst->GetArchetype()->GetBound());
	boundTestParams.SetTransform(&RCC_MATRIX34(inst->GetMatrix()));
	boundTestParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	boundTestParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	boundTestParams.SetExcludeEntity(pEntity);
	boundTestParams.SetContext(WorldProbe::EScript);
	boundTestParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	return CScriptShapeTestManager::CreateShapeTestRequest(boundTestParams, true /* Synchronous? */);
}

ScriptShapeTestGuid CommandStartShapeTestBox(const scrVector & scrVecPos, const scrVector & scrVecDims, const scrVector & scrVecRotation, int RotOrder, s32 LOSFlags,
											 int nExcludeEntityIndex, s32 nOptionFlags)
{
	phBoundBox* pBound = rage_new phBoundBox(Vector3(scrVecDims));

	Vector3 vecRotation(scrVecRotation);

	Matrix34 trans;
	if(vecRotation.IsNonZero())
	{
		CScriptEulers::MatrixFromEulers(trans, DtoR * vecRotation, static_cast<EulerAngleOrder>(RotOrder));
	}
	else
	{
		trans.Identity();
	}

	// Translate the box to the desired world position.
	trans.d = Vector3(scrVecPos);

	CPhysical* pEntity = NULL;
	if(nExcludeEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
		pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(nExcludeEntityIndex));

	WorldProbe::CShapeTestBoundDesc boundTestParams;
	boundTestParams.SetBound(static_cast<phBound*>(pBound));
	if(pEntity)
		boundTestParams.SetExcludeEntity(pEntity);
	boundTestParams.SetTransform(&trans);
	boundTestParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	boundTestParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	boundTestParams.SetContext(WorldProbe::EScript);
	boundTestParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	ScriptShapeTestGuid guid = CScriptShapeTestManager::CreateShapeTestRequest(boundTestParams, true /* Synchronous? */);

	pBound->Release();

	return guid;
}

ScriptShapeTestGuid CommandStartShapeTestCapsule(const scrVector & scrVecPos, const scrVector & scrVecEndPos, float fRadius, s32 LOSFlags, int nExcludeEntityIndex,
												 s32 nOptionFlags)
{
	CPhysical* pEntity = NULL;
	if(nExcludeEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
		pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(nExcludeEntityIndex));

	WorldProbe::CShapeTestCapsuleDesc capsuleParams;
	capsuleParams.SetCapsule(Vector3(scrVecPos), Vector3(scrVecEndPos), fRadius);
	if(pEntity)
		capsuleParams.SetExcludeEntity(pEntity);
	capsuleParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	capsuleParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	capsuleParams.SetContext(WorldProbe::EScript);
	capsuleParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	return CScriptShapeTestManager::CreateShapeTestRequest(capsuleParams, true /* Synchronous? */);
}

ScriptShapeTestGuid CommandStartShapeTestSweptSphere(const scrVector & scrVecPos, const scrVector & scrVecEndPos, float fRadius, s32 LOSFlags, int nExcludeEntityIndex,
	s32 nOptionFlags)
{
	CPhysical* pEntity = NULL;
	if(nExcludeEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
		pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(nExcludeEntityIndex));

	WorldProbe::CShapeTestCapsuleDesc capsuleParams;
	capsuleParams.SetCapsule(Vector3(scrVecPos), Vector3(scrVecEndPos), fRadius);
	if(pEntity)
		capsuleParams.SetExcludeEntity(pEntity);
	capsuleParams.SetIsDirected(true);
	capsuleParams.SetDoInitialSphereCheck(true);
	capsuleParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	capsuleParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	capsuleParams.SetContext(WorldProbe::EScript);
	capsuleParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	return CScriptShapeTestManager::CreateShapeTestRequest(capsuleParams, true /* Synchronous? */);
}

ScriptShapeTestGuid CommandStartShapeTestMouseCursorLOSProbe(Vector3 &vOutProbeStartPos, Vector3 &vOutProbeEndPos, s32 LOSFlags, int nExcludeEntityIndex,
												  s32 nOptionFlags)
{
	// Grab the screen position of the mouse cursor and from that calculate the near and far world positions based on the camera projection.
	const float mouseScreenX = static_cast<float>(ioMouse::GetX());
	const float mouseScreenY = static_cast<float>(ioMouse::GetY());
	
	Vector3 vMouseNear;
	Vector3 vMouseFar;

	gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform(mouseScreenX, mouseScreenY, (Vec3V&)vMouseNear, (Vec3V&)vMouseFar);

	// Perform a probe test using the mouse near and far as the start and end positions.
	CPhysical* pEntity = NULL;
	if(nExcludeEntityIndex != NULL_IN_SCRIPTING_LANGUAGE)
		pEntity = const_cast<CPhysical*>(CTheScripts::GetEntityToQueryFromGUID<CPhysical>(nExcludeEntityIndex));

	WorldProbe::CShapeTestProbeDesc probeParams;
	probeParams.SetStartAndEnd(vMouseNear, vMouseFar);
	if(pEntity)
		probeParams.SetExcludeEntity(pEntity);
	//probeParams.SetIsDirected(false);
	probeParams.SetTypeFlags(ArchetypeFlags::GTA_SCRIPT_TEST);
	probeParams.SetIncludeFlags(GetPhysicsFlags(LOSFlags));
	probeParams.SetContext(WorldProbe::EScript);
	probeParams.SetOptions(GetShapeTestOptionFlags(nOptionFlags));

	vOutProbeStartPos = vMouseNear;
	vOutProbeEndPos = vMouseFar;

	return CScriptShapeTestManager::CreateShapeTestRequest(probeParams, false /* Synchronous? */);
}

// If this returns SHAPETEST_STATUS_RESULTS_READY it will also delete the CScriptShapeTestRequest.
SHAPETEST_STATUS CommandGetShapeTestResult(int shapeTestGuid, int& bHitSomething, Vector3& vPos, Vector3& vNormal, int& nEntityIndex)
{
	WorldProbe::CShapeTestHitPoint result;
	
	SHAPETEST_STATUS status = CScriptShapeTestManager::GetShapeTestResult(shapeTestGuid, result);

	if( status == SHAPETEST_STATUS_RESULTS_READY )
	{
		bHitSomething = (result.GetHitInst() != NULL);

		if(bHitSomething)
		{
			vPos = result.GetHitPosition();
			vNormal = result.GetHitNormal();

			CEntity* pHitEntity = result.GetHitEntity();
			nEntityIndex = pHitEntity ? CTheScripts::GetGUIDFromEntity(*pHitEntity) : 0;
		}
	}

	return status;
}

// If this returns SHAPETEST_STATUS_RESULTS_READY it will also delete the CScriptShapeTestRequest.
SHAPETEST_STATUS CommandGetShapeTestResultIncludingMaterial(int shapeTestGuid, int& bHitSomething, Vector3& vPos, Vector3& vNormal, int& material,
															int& nEntityIndex)
{
	WorldProbe::CShapeTestHitPoint result;

	SHAPETEST_STATUS status = CScriptShapeTestManager::GetShapeTestResult(shapeTestGuid, result);

	if( status == SHAPETEST_STATUS_RESULTS_READY )
	{
		bHitSomething = (result.GetHitInst() != NULL);

		if(bHitSomething)
		{
			vPos = result.GetHitPosition();
			vNormal = result.GetHitNormal();

			CEntity* pHitEntity = result.GetHitEntity();
			nEntityIndex = pHitEntity ? CTheScripts::GetGUIDFromEntity(*pHitEntity) : 0;

			char zMaterialName[128];
			PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(result.GetMaterialId()), zMaterialName, 128);
			material = rage::atStringHash(zMaterialName);
		}
	}

	return status;
}

void CommandReleaseScriptGuidFromEntity(int nEntityIndex)
{
	CEntity *pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(nEntityIndex, 0); // don't assert if no entity

	if (pEntity)
	{
		const CScriptEntityExtension* pExtension = pEntity->GetExtension<CScriptEntityExtension>();

		if (!pExtension)	//	Don't remove the script GUID from an entity that has a script extension
		{
			fwScriptGuid::DeleteGuid(*pEntity);
		}
	}
}

u32 GetPhysicsFlags(s32 LOSFlags)
{
	u32 Flags = 0;

	if ((LOSFlags & SCRIPT_INCLUDE_MOVER) == SCRIPT_INCLUDE_MOVER)
	{
		Flags |= ArchetypeFlags::GTA_ALL_MAP_TYPES;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_VEHICLE) == SCRIPT_INCLUDE_VEHICLE)
	{
		Flags |= ArchetypeFlags::GTA_VEHICLE_TYPE;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_PED) == SCRIPT_INCLUDE_PED)
	{
		Flags |= ArchetypeFlags::GTA_PED_TYPE;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_RAGDOLL) == SCRIPT_INCLUDE_RAGDOLL)
	{
		Flags |= ArchetypeFlags::GTA_RAGDOLL_TYPE;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_OBJECT) == SCRIPT_INCLUDE_OBJECT)
	{
		Flags |= ArchetypeFlags::GTA_OBJECT_TYPE;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_PICKUP) == SCRIPT_INCLUDE_PICKUP)
	{
		Flags |= ArchetypeFlags::GTA_PICKUP_TYPE;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_GLASS) == SCRIPT_INCLUDE_GLASS)
	{
		Flags |= ArchetypeFlags::GTA_GLASS_TYPE;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_RIVER) == SCRIPT_INCLUDE_RIVER)
	{
		Flags |= ArchetypeFlags::GTA_RIVER_TYPE;
	}

	if ((LOSFlags & SCRIPT_INCLUDE_FOLIAGE) == SCRIPT_INCLUDE_FOLIAGE)
	{
		Flags |= ArchetypeFlags::GTA_FOLIAGE_TYPE;
	}


	scriptAssertf(Flags,"%s:HAS_ENTITY_CLEAR_LOS_TO_ENTITY - No flags set", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	return Flags;
}

u32 GetShapeTestOptionFlags(s32 nOptionFlags)
{
	u32 nShapeTestOptions = 0;
	if(nOptionFlags&SCRIPT_SHAPETEST_OPTION_IGNORE_GLASS)
	{
		nShapeTestOptions |= WorldProbe::LOS_GO_THRU_GLASS;
	}
	if(nOptionFlags&SCRIPT_SHAPETEST_OPTION_IGNORE_SEE_THROUGH)
	{
		nShapeTestOptions |= WorldProbe::LOS_IGNORE_SEE_THRU;
	}
	if(nOptionFlags&SCRIPT_SHAPETEST_OPTION_IGNORE_NO_COLLISION)
	{
		nShapeTestOptions |= WorldProbe::LOS_IGNORE_NO_COLLISION;
	}

	return nShapeTestOptions;
}

void SetupScriptCommands()
{
	SCR_REGISTER_SECURE(START_SHAPE_TEST_LOS_PROBE,0x1e4c19dc2ad86a18, CommandStartShapeTestLOSProbe);
	SCR_REGISTER_SECURE(START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE,0x53dfe85fc69724bd, CommandStartExpensiveSynchronousShapeTestLOSProbe);
	//SCR_REGISTER_UNUSED(START_SHAPE_TEST_SPHERE, 0x6744489f, CommandStartShapeTestSphere); Since nobody is using them, async sphere tests are turned off
	SCR_REGISTER_SECURE(START_SHAPE_TEST_BOUNDING_BOX,0x1031cba8ec6122a0, CommandStartShapeTestBoundingBox);
	SCR_REGISTER_SECURE(START_SHAPE_TEST_BOX,0x3e6477b1f848780e, CommandStartShapeTestBox);
	SCR_REGISTER_SECURE(START_SHAPE_TEST_BOUND,0x4d2318768f84c92c, CommandStartShapeTestBound);
	SCR_REGISTER_SECURE(START_SHAPE_TEST_CAPSULE,0x9d2c476a7d8d9385, CommandStartShapeTestCapsule);
	SCR_REGISTER_SECURE(START_SHAPE_TEST_SWEPT_SPHERE,0x418f12b9602cb2c7, CommandStartShapeTestSweptSphere);
	SCR_REGISTER_SECURE(START_SHAPE_TEST_MOUSE_CURSOR_LOS_PROBE,0x34686dbb08c1704f, CommandStartShapeTestMouseCursorLOSProbe);
	SCR_REGISTER_SECURE(GET_SHAPE_TEST_RESULT,0xb2d581bd7446bbe9, CommandGetShapeTestResult);
	SCR_REGISTER_SECURE(GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL,0xc2c6d9d32d339eea, CommandGetShapeTestResultIncludingMaterial);

	SCR_REGISTER_SECURE(RELEASE_SCRIPT_GUID_FROM_ENTITY,0x67be6880a27eb293, CommandReleaseScriptGuidFromEntity);
}

//-----------------------------------------------------------------------------


}	// namespace shapetest_commands
