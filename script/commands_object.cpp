
// Rage headers
#include "script/wrapper.h"
#include "physics/gtaInst.h"
#include "physics/sleep.h"
#include "fragment/instance.h"
#include "fragment/cache.h"
#include "fragment/cachemanager.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/animmanager.h"
#include "fwmaths/Angle.h"
#include "fwmaths/vector.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwscene/world/WorldLimits.h"
#include "phbound/boundcomposite.h"

// Framework headers
#include "fwdecorator/decoratorExtension.h"

// Game headers
#include "animation/AnimDefines.h"
#include "animation/MoveObject.h"
#include "audio/emitteraudioentity.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/viewports/Viewport.h"
#include "control/replay/replay.h"
#include "control/replay/Entity/ObjectPacket.h"
#include "frontend/MiniMap.h"
#include "game/ModelIndices.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "network/Network.h"
#include "network/NetworkInterface.h"
#include "network/Objects/Entities/NetObjDoor.h"
#include "network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Live/NetworkTelemetry.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkUtil.h"
#include "objects/Door.h"
#include "objects/DummyObject.h"
#include "objects/ObjectPopulation.h"
#include "pathserver/PathServer.h"
#include "peds/Ped.h"
#include "pickups/Data/PickupIds.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/Pickup.h"
#include "pickups/PickupPlacement.h"
#include "pickups/PickupManager.h"
#include "physics/WorldProbe/worldprobe.h"
#include "physics/physics.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/Lights/LightEntity.h"
#include "scene/AnimatedBuilding.h"
#include "scene/entities/compEntity.h"
#include "scene/world/gameWorld.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/script.h"
#include "script/script_areas.h"
#include "script/script_cars_and_peds.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "shaders/CustomShaderEffectProp.h"
#include "security/plugins/scripthook.h"
#include "streaming/streaming.h"
#include "task/Combat/Cover/Cover.h"
#include "vehicles/vehicle.h"
#include "commands_interiors.h"
#include "commands_object.h"
#include "commands_misc.h"
#include "control/garages.h"
#include "fwscene/stores/staticboundsstore.h"
#include "Pickups/PickupRewards.h"
#include "weapons/Weapon.h"
#include "weapons/components/WeaponComponentFlashLight.h"

SCRIPT_OPTIMISATIONS ()
NETWORK_OPTIMISATIONS ()

#if __BANK
	#include "fwdebug/picker.h"
	extern bool sm_breakOnSelectedDoorSetState;
	extern bool sm_logStateChangesForSelectedDoor;
	extern int sm_debugDooorSystemEnumHash;
	extern char sm_debugDooorSystemEnumString[CDoor::DEBUG_DOOR_STRING_LENGTH];
#define OutputScriptState(__doorEnumHash, __pString, ...) do { if (sm_logStateChangesForSelectedDoor && sm_debugDooorSystemEnumHash == __doorEnumHash)    \
 														  { Displayf("Door Hash %s, %d", sm_debugDooorSystemEnumString, __doorEnumHash);				  \
 														  Displayf(__pString, __VA_ARGS__);															      \
 														  Displayf("Called by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());} } while(0)
#else
	#define OutputScriptState(__doorEnumHash, __pString, ...)
#endif

#if __BANK
PARAM(netDebugCreatePortablePickup, "Output will echo the passed parameters, result, and frame times of CreatePortablePickup");
#endif
namespace object_commands
{


bool ObjectInAreaCheck( bool Do3dCheck, int iObjectID, const Vector3& vAreaFrom, const Vector3& vAreaTo, bool bHighlightArea )
{
	const CObject *pObj;

	float TargetX1, TargetY1, TargetZ1;
	float TargetX2, TargetY2, TargetZ2;
	float temp_float;

	bool bResult = false;

	pObj = CTheScripts::GetEntityToQueryFromGUID<CObject>(iObjectID);
	if (!pObj)
	{
		return bResult;
	}
	TargetX1 = vAreaFrom.x;
	TargetY1 = vAreaFrom.y;

	if (Do3dCheck)
	{
		TargetZ1 = vAreaFrom.z;
		TargetX2 = vAreaTo.x;
		TargetY2 = vAreaTo.y;
		TargetZ2 = vAreaTo.z;

		if (TargetZ1 > TargetZ2)
		{
			temp_float = TargetZ1;
			TargetZ1 = TargetZ2;
			TargetZ2 = temp_float;
		}
	}
	else
	{
		TargetZ1 = INVALID_MINIMUM_WORLD_Z;
		TargetX2 = vAreaTo.x;
		TargetY2 = vAreaTo.y;
		TargetZ2 = TargetZ1;
	}

	if (TargetX1 > TargetX2)
	{
		temp_float = TargetX1;
		TargetX1 = TargetX2;
		TargetX2 = temp_float;
	}

	if (TargetY1 > TargetY2)
	{
		temp_float = TargetY1;
		TargetY1 = TargetY2;
		TargetY2 = temp_float;
	}

	const Vector3 ObjectPos = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());

//	Check that the object is within the specified area
	if (Do3dCheck)
	{
		if ((ObjectPos.x >= TargetX1) && (ObjectPos.x <= TargetX2)
			&& (ObjectPos.y >= TargetY1) && (ObjectPos.y <= TargetY2)
			&& (ObjectPos.z >= TargetZ1) && (ObjectPos.z <= TargetZ2))
		{
			bResult = TRUE;
		}
	}
	else
	{
		if ((ObjectPos.x >= TargetX1) && (ObjectPos.x <= TargetX2)
			&& (ObjectPos.y >= TargetY1) && (ObjectPos.y <= TargetY2))
		{
			bResult = TRUE;
		}
	}

	if (bHighlightArea)
	{
		CScriptAreas::NewHighlightImportantArea(TargetX1, TargetY1, TargetZ1, TargetX2, TargetY2, TargetZ2, Do3dCheck, CALCULATION_OBJECT_IN_AREA);
	}

	if (CScriptDebug::GetDbgFlag())
	{
		if (Do3dCheck)
		{
			CScriptDebug::DrawDebugCube(TargetX1, TargetY1, TargetZ1, TargetX2, TargetY2, TargetZ2);
		}
		else
		{
			CScriptDebug::DrawDebugSquare(TargetX1, TargetY1, TargetX2, TargetY2);
		}
	}
	return( bResult );
}

bool PointInAngledAreaCheck(bool Do3dCheck, const Vector3& vObjectPos, const Vector3& vCoords1, const Vector3& vCoords2, float DistanceFrom1To4, bool HighlightArea)
{
	// Tests if the given point is contained within a non-axis aligned rectangle which is defined by the midpoints of two parallel edges
	// and the length of those sides:
	//
	// <--AreaWidth-->
	//  srcVecCoors1
	// -------x-------
	// |             |
	// |             |
	// -------x-------
	//  srcVecCoors2

	float TargetX1, TargetY1, TargetZ1;
	float TargetX2, TargetY2, TargetZ2;
	float TargetX3, TargetY3;	//	, TargetZ3;
	float TargetX4, TargetY4;	//	, TargetZ4;

	float temp_float;

	bool bResult = false;

	float RadiansBetweenFirstTwoPoints;
	float RadiansBetweenPoints1and4;
	float TestDistance;

	Vector2 vec1To2, vec1To4, vec1ToObject;

	float DistanceFrom1To2, DistanceFrom1To4Test;

	TargetX1 = vCoords1.x;
	TargetY1 = vCoords1.y;

	if (Do3dCheck)
	{
		TargetZ1 = vCoords1.z;
		TargetX2 = vCoords2.x;
		TargetY2 = vCoords2.y;
		TargetZ2 = vCoords2.z;

		if (TargetZ1 > TargetZ2)
		{
			temp_float = TargetZ1;
			TargetZ1 = TargetZ2;
			TargetZ2 = temp_float;
		}
	}
	else
	{
		TargetZ1 = 0.0f;
		TargetX2 = vCoords2.x;
		TargetY2 = vCoords2.y;
		TargetZ2 = 0.0f;
	}

	RadiansBetweenFirstTwoPoints = fwAngle::GetRadianAngleBetweenPoints(TargetX1, TargetY1, TargetX2, TargetY2);
	RadiansBetweenPoints1and4 = RadiansBetweenFirstTwoPoints + (PI / 2);

	while (RadiansBetweenPoints1and4 < 0.0f)
	{
		RadiansBetweenPoints1and4 += (2 * PI);
	}

	while (RadiansBetweenPoints1and4 > (2 * PI))
	{
		RadiansBetweenPoints1and4 -= (2 * PI);
	}

	float fSinResult = rage::Sinf(RadiansBetweenPoints1and4);
	float fCosResult = rage::Cosf(RadiansBetweenPoints1and4);
	TargetX3 = (0.5f*DistanceFrom1To4 * fSinResult) + TargetX2;
	TargetY3 = (0.5f*DistanceFrom1To4 * -fCosResult) + TargetY2;

	TargetX4 = (0.5f*DistanceFrom1To4 * fSinResult) + TargetX1;
	TargetY4 = (0.5f*DistanceFrom1To4 * -fCosResult) + TargetY1;

	// Modify Target1 and Target2 to be at the opposite corners of the rectangle from 3 and 4; they are currently defining
	// the midpoints of two sides.
	TargetX1 = TargetX1 - (0.5f*DistanceFrom1To4 * fSinResult);
	TargetY1 = TargetY1 - (0.5f*DistanceFrom1To4 * -fCosResult);
	TargetX2 = TargetX2 - (0.5f*DistanceFrom1To4 * fSinResult);
	TargetY2 = TargetY2 - (0.5f*DistanceFrom1To4 * -fCosResult);

	vec1To2 = Vector2((TargetX2 - TargetX1), (TargetY2 - TargetY1));
	vec1To4 = Vector2((TargetX4 - TargetX1), (TargetY4 - TargetY1));

	DistanceFrom1To2 = vec1To2.Mag();
	DistanceFrom1To4Test = vec1To4.Mag();


	bResult = FALSE;

	vec1ToObject = Vector2((vObjectPos.x - TargetX1), (vObjectPos.y - TargetY1));

	vec1To2.Normalize();
	TestDistance = vec1ToObject.Dot(vec1To2);

	//	Check that the object is within the specified area
	if ((TestDistance >= 0.0f)
		&& (TestDistance <= DistanceFrom1To2))
	{
		vec1To4.Normalize();
		TestDistance = vec1ToObject.Dot(vec1To4);

		if ((TestDistance >= 0.0f)
			&& (TestDistance <= DistanceFrom1To4Test))
		{
			if (Do3dCheck)
			{
				if ((vObjectPos.z >= TargetZ1) && (vObjectPos.z <= TargetZ2))
				{
					bResult = TRUE;
				}
			}
			else
			{
				bResult = TRUE;
			}
		}
	}

	if (HighlightArea)
	{
		if (Do3dCheck)
		{
			//			float CentreZ = (TargetZ1 + TargetZ2) / 2.0f;

			CScriptAreas::HighlightImportantAngledArea( (u32)(CTheScripts::GetCurrentGtaScriptThread()->GetIDForThisLineInThisThread()),
				TargetX1, TargetY1,
				TargetX2, TargetY2,
				TargetX3, TargetY3,
				TargetX4, TargetY4,
				TargetZ1);
		}
		else
		{
			CScriptAreas::HighlightImportantAngledArea( (u32)(CTheScripts::GetCurrentGtaScriptThread()->GetIDForThisLineInThisThread()),
				TargetX1, TargetY1,
				TargetX2, TargetY2,
				TargetX3, TargetY3,
				TargetX4, TargetY4,
				INVALID_MINIMUM_WORLD_Z);
		}
	}


	if (CScriptDebug::GetDbgFlag())
	{
		if (Do3dCheck)
		{
			CScriptDebug::DrawDebugAngledCube(TargetX1, TargetY1, TargetZ1, TargetX2, TargetY2, TargetZ2, TargetX3, TargetY3, TargetX4, TargetY4);
		}
		else
		{
			CScriptDebug::DrawDebugAngledSquare(TargetX1, TargetY1, TargetX2, TargetY2, TargetX3, TargetY3, TargetX4, TargetY4);
		}
	}
	return(bResult);
}

void CommandSetActivateObjectPhysicsAsSoonAsItIsUnfrozen(int ObjectIndex, bool bActivatePhysicsWhenUnfrozen)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{
		if (bActivatePhysicsWhenUnfrozen)
		{
			pObj->m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = true;
		}
		else
		{
			pObj->m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = false;
		}
	}
}

// bCalledByScriptCommand will be false if this function is called when loading from memory card
// PoolIndex is only used when loading from the memory card. It will be -1 in all other cases
void ObjectCreationFunction(int ObjectModelHashKey, float NewX, float NewY, float NewZ, bool bWithZOffset, bool bCalledByScriptCommand, s32 PoolIndex, int& iNewObjectIndex, bool bRegisterAsNetworkObject, bool bScriptHostObject, bool bForceToBeObject)
{
	iNewObjectIndex = 0;
	fwModelId ObjectModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ObjectModelHashKey, &ObjectModelId);		//	ignores return value

	// non-networked objects should always be treated as client objects
	if (!bRegisterAsNetworkObject) 
		bScriptHostObject = false;

	if (!SCRIPT_VERIFY (ObjectModelId.IsValid(), "ObjectCreationFunction - this is not a valid model index"))
	{
		return;
	}
	if (!SCRIPT_VERIFY ((rage::Abs(NewX)< WORLDLIMITS_XMAX) &&(rage::Abs(NewY)< WORLDLIMITS_YMAX) && (rage::Abs(NewZ)< WORLDLIMITS_ZMAX), "ObjectCreationFunction - Trying to create an object outside the world bounds"))
	{
		return;
	}
	if (!SCRIPT_VERIFY(!CNetwork::IsGameInProgress() || !bRegisterAsNetworkObject || CTheScripts::GetCurrentGtaScriptHandlerNetwork() || bCalledByScriptCommand, "ObjectCreationFunction - Non-networked scripts that are safe to run during the network game cannot create networked entities"))
	{
		return;
	}

	CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(ObjectModelId);

	if (!SCRIPT_VERIFY (pModel, "ObjectCreationFunction - Couldn't find Model Info"))
	{
		return;
	}
	if (!SCRIPT_VERIFY_TWO_STRINGS (pModel->GetIsTypeObject(), "%s:ObjectCreationFunction - Model isn't type object", pModel->GetModelName()))
	{
		return;
	}
	if (bScriptHostObject && (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
	{
		if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "ObjectCreationFunction - Non-host machines cannot create host objects"))
		{
			return;
		}
	}
	if (!SCRIPT_VERIFY (!(pModel->GetUsesDoorPhysics() && !bForceToBeObject && NetworkInterface::IsGameInProgress()), "ObjectCreationFunction - Cannot create doors in MP, it is currently not supported. Set ForceToBeObject to TRUE to force this door to be an object and prevent this assert"))
	{
		return;
	}

	if (!SCRIPT_VERIFY (!(CObjectPopulation::IsBlacklistedForMP(pModel->GetModelNameHash()) && NetworkInterface::IsGameInProgress()), "ObjectCreationFunction - Cannot create blacklisted objects in MP, it is currently not permitted."))
	{
		return;
	}

#if __DEV
		printf("creating object %s, by script %s \n", pModel->GetModelName(), CTheScripts::GetCurrentScriptNameAndProgramCounter()); 
#endif

	// don't network objects created during a cutscene
	if (!NetworkInterface::AreCutsceneEntitiesNetworked() && CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetInMPCutscene())
	{
		if (scriptVerifyf(NetworkInterface::IsInMPCutscene(), "Script cutscene flag still set when not in a cutscene"))
		{
			bRegisterAsNetworkObject = false;
		}
		else
		{
			CTheScripts::GetCurrentGtaScriptHandlerNetwork()->SetInMPCutscene(false);
		}
	}

	//Create the input.
	CObjectPopulation::CreateObjectInput input(ObjectModelId, ENTITY_OWNEDBY_SCRIPT, false);
	input.m_bInitPhys = true;
	input.m_bClone = bRegisterAsNetworkObject;
	input.m_bForceClone = bRegisterAsNetworkObject;
	input.m_iPoolIndex = (PoolIndex >= 0) ? PoolIndex : -1;
	input.m_bForceObjectType = bForceToBeObject;

	CObject* pObject = CObjectPopulation::CreateObject(input);

	if (!SCRIPT_VERIFY (pObject, "ObjectCreationFunction - Couldn't create a new object"))
	{
		return;
	}

	REPLAY_ONLY(CReplayMgr::RecordObject(pObject));

	pObject->m_nPhysicalFlags.bNotToBeNetworked = !bRegisterAsNetworkObject;

	if (NetworkInterface::IsGameInProgress() && bRegisterAsNetworkObject)
	{
		scriptAssertf(pObject->GetNetworkObject(), "ObjectCreationFunction - Ran out of network objects for this object");
	}

	if (NewZ <= INVALID_MINIMUM_WORLD_Z)
	{
		NewZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, NewX, NewY);
	}

	if (bWithZOffset)
	{
		NewZ += pObject->GetDistanceFromCentreOfMassToBaseOfModel();
	}

	pObject->SetPosition(Vector3(NewX, NewY, NewZ));
	pObject->SetHeading(0.0f);

	// Add Object to world after its position has been set
	CGameWorld::Add(pObject, CGameWorld::OUTSIDE );	
	pObject->GetPortalTracker()->RequestRescanNextUpdate();
	pObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition()));

	pObject->CreateDrawable();

	CScriptEntities::ClearSpaceForMissionEntity(pObject);
	iNewObjectIndex =  CTheScripts::GetGUIDFromEntity(*pObject);

	if (bCalledByScriptCommand && CTheScripts::GetCurrentGtaScriptThread())
	{
		pObject->GetLodData().SetResetDisabled(true);
		CTheScripts::RegisterEntity(pObject, bScriptHostObject, bRegisterAsNetworkObject);
	}

#if GTA_REPLAY
	// HACK for TV Static Object (B*2240576)
	if( pModel && pModel->GetModelNameHash() == ATSTRINGHASH("Prop_TT_screenstatic",0xe2e039bc) )
	{
		CReplayMgr::RecordObject(pObject);
	}
	// HACK
#endif	//GTA_REPLAY

	// B*7009579: HACK for Ammu-Nation gun selection wall going bright during the day:
	if(pObject->GetModelNameHash() == ATSTRINGHASH("ch_prop_board_wpnwall_02a", 0x0A3C70FA))
		pObject->SetNaturalAmbientScale(0);
}

int CommandCreateObject(int ObjectModelHashKey, const scrVector & scrVecNewCoors, bool bRegisterAsNetworkObject, bool bScriptHostObject, bool bForceToBeObject)
{
	int iNewObjectIndex;
	
	Vector3 vNewCoors = Vector3 (scrVecNewCoors);

	scriptAssertf( (rage::Abs(vNewCoors.x) > 0.05f) || (rage::Abs(vNewCoors.y) > 0.05f) || vNewCoors.z > 100.0f, "CREATE_OBJECT. Don't create objects at (0,0). Create them at their final destination instead" );

	ObjectCreationFunction(ObjectModelHashKey, vNewCoors.x, vNewCoors.y, vNewCoors.z, true, true, -1, iNewObjectIndex, bRegisterAsNetworkObject, bScriptHostObject, bForceToBeObject);

	return iNewObjectIndex;
}

int CommandCreateObjectNoOffset(int ObjectModelHashKey, const scrVector & scrVecNewCoors, bool bRegisterAsNetworkObject, bool bScriptHostObject, bool bForceToBeObject )
{
	int iNewObjectIndex;
	
	Vector3 vNewCoors = Vector3 (scrVecNewCoors);

	scriptAssertf( (rage::Abs(vNewCoors.x) > 0.05f) || (rage::Abs(vNewCoors.y) > 0.05f) || vNewCoors.z > 100.0f, "CREATE_OBJECT_NO_OFFSET. Don't create objects at (0,0). Create them at their final destination instead" );
	SCRIPT_CHECK_CALLING_FUNCTION
	ObjectCreationFunction(ObjectModelHashKey, vNewCoors.x, vNewCoors.y, vNewCoors.z, false, true, -1, iNewObjectIndex, bRegisterAsNetworkObject, bScriptHostObject, bForceToBeObject);

	return iNewObjectIndex;
}

void DeleteScriptObject(CObject *pObject)
{
	if (scriptVerifyf(pObject, "DeleteScriptObject - object doesn't exist"))
	{
		CScriptEntityExtension* pExtension = pObject->GetExtension<CScriptEntityExtension>();

//	Maybe it's okay to allow these objects to be deleted now. As soon as they are grabbed from the world, all ties are broken.
//	I no longer keep a pointer to the related dummy object.
		if (SCRIPT_VERIFY(pExtension, "DeleteScriptObject - The object is not a script entity") &&
			(SCRIPT_VERIFY(pExtension->GetScriptObjectWasGrabbedFromTheWorld() == false, "DeleteScriptObject - can't delete an object that was grabbed from the world")) )
		{
			CObjectPopulation::DestroyObject(pObject);
		}
	}
}

void CommandDeleteObject(int &ObjectIndex)
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex, CTheScripts::GUID_ASSERT_FLAG_NOT_CLONE);

	if (pObject && !NetworkUtils::IsNetworkCloneOrMigrating(pObject))
	{
		CScriptEntityExtension* pExtension = pObject->GetExtension<CScriptEntityExtension>();

		if (SCRIPT_VERIFY(pExtension, "DELETE_OBJECT - The object is not a script entity") &&
			SCRIPT_VERIFY(pExtension->GetScriptHandler()==CTheScripts::GetCurrentGtaScriptHandler(), "DELETE_OBJECT - The object was not created by this script"))
		{
			DeleteScriptObject(pObject);
		}
	}

	ObjectIndex = 0;
}

bool CommandPlaceObjectOnGroundProperly(int ObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	if(pObject)
	{
		bool ret = pObject->PlaceOnGroundProperly();
		if(!ret)
		{
			pObject->SetIsFixedWaitingForCollision(true);
		}

		return ret;
	}
	return false;
}

bool CommandPlaceObjectOnGroundOrObjectProperly(int ObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	if(pObject)
	{
		bool ret = pObject->PlaceOnGroundProperly( 10.0f, true, 0.0f, false, false, NULL, true );
		if(!ret)
		{
			pObject->SetIsFixedWaitingForCollision(true);
		}

		return ret;
	}
	return false;
}

scrVector CommandGetSafePickupCoords(const scrVector & scrVecInCoors, float minDist, float maxDist )
{
	Vector3 srcPos( scrVecInCoors );
	bool bGroundLoaded = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(VECTOR3_TO_VEC3V(srcPos), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
	if(bGroundLoaded)
	{
		CPickupManager::CPickupPositionInfo pos( srcPos );
		if ( !scriptVerifyf( minDist < maxDist, "GET_SAFE_PICKUP_COORDS - swapping min and max values as wrong way round [%f - %f]",
			minDist, maxDist ) )
		{
			float tmp(minDist);
			minDist = maxDist;
			maxDist = tmp;
		}
		pos.m_MinDist = minDist;
		pos.m_MaxDist = maxDist;
		CPickupManager::CalculateDroppedPickupPosition(pos, true);

		return pos.m_Pos;
	}

	aiWarningf("Ground collision has not streamed in.");
	return srcPos;
}

void CommandAddExtendedPickupProbeArea(const scrVector& position, float radius)
{
	if(NetworkInterface::IsGameInProgress())
	{
#if !__FINAL 
		scrThread::PrePrintStackTrace(); 
		scriptDebugf1("%s : ADD_EXTENDED_PICKUP_PROBE_AREA called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif 

		Vector3 areaPosition(position);
#if ENABLE_NETWORK_LOGGING
		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "ADD_EXTENDED_PICKUP_PROBE_AREA", "x:%.f, y:%.f, z:%.f", areaPosition.GetX(), areaPosition.GetY(), areaPosition.GetZ());
		log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		log.WriteDataValue("Script", "%.f", radius);
#endif // ENABLE_NETWORK_LOGGING
		
		if(CPickup::AddExtendedProbeArea(areaPosition, radius))
		{
#if ENABLE_NETWORK_LOGGING
			log.WriteDataValue("Result", "%s", "Successful");
#endif // ENABLE_NETWORK_LOGGING
		}
		else
		{
#if ENABLE_NETWORK_LOGGING
			log.WriteDataValue("Result", "%s", "Failed");
#endif // ENABLE_NETWORK_LOGGING
		}
	}
}

void CommandClearExtendedPickupProbeAreas()
{
#if !__FINAL 
	scrThread::PrePrintStackTrace(); 
	scriptDebugf1("%s : CLEAR_EXTENDED_PICKUP_PROBE_AREAS called", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif 

#if ENABLE_NETWORK_LOGGING
	if(NetworkInterface::IsGameInProgress())
	{
		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "CLEAR_EXTENDED_PICKUP_PROBE_AREAS", "");
		log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
#endif
	CPickup::ClearExtendedProbeAreas();
}

// temp function to convert the old pickup types to the new hashes. Remove once the scripts are all using the hashes.
int ConvertOldPickupTypeToHash(int PickupType)
{
	if (PickupType >=0 && PickupType < NUM_PICKUP_TYPES)
	{
		switch (PickupType)
		{
		case PICKUP_HEALTH_STANDARD_TYPE:
			return PICKUP_HEALTH_STANDARD;
		case PICKUP_ARMOUR_STANDARD_TYPE:
			return PICKUP_ARMOUR_STANDARD;
		case PICKUP_MONEY_VARIABLE_TYPE:
			return PICKUP_MONEY_VARIABLE;
		case PICKUP_MONEY_CASE_TYPE:
			return PICKUP_MONEY_CASE;
		case PICKUP_MONEY_WALLET_TYPE:
			return PICKUP_MONEY_WALLET;
		case PICKUP_MONEY_PURSE_TYPE:
			return PICKUP_MONEY_PURSE;
		case PICKUP_MONEY_DEP_BAG_TYPE:
			return PICKUP_MONEY_DEP_BAG;
		case PICKUP_MONEY_MED_BAG_TYPE:
			return PICKUP_MONEY_MED_BAG;
		case PICKUP_MONEY_PAPER_BAG_TYPE:
			return PICKUP_MONEY_PAPER_BAG;
		case PICKUP_MONEY_SECURITY_CASE_TYPE:
			return PICKUP_MONEY_SECURITY_CASE;
		case PICKUP_CUSTOM_SCRIPT_TYPE:
			return PICKUP_CUSTOM_SCRIPT;
		case PICKUP_HANDCUFF_KEY_TYPE:
			return PICKUP_HANDCUFF_KEY;
		case PICKUP_CAMERA_TYPE:
			return PICKUP_CAMERA;
		case PICKUP_PORTABLE_PACKAGE_TYPE:
			return PICKUP_PORTABLE_PACKAGE;

		// Weapons
		case PICKUP_WEAPON_PISTOL_TYPE:
			return PICKUP_WEAPON_PISTOL;
		case PICKUP_WEAPON_COMBATPISTOL_TYPE:
			return PICKUP_WEAPON_COMBATPISTOL;
		case PICKUP_WEAPON_PISTOL50_TYPE:
			return PICKUP_WEAPON_PISTOL50;
		case PICKUP_WEAPON_APPISTOL_TYPE:
			return PICKUP_WEAPON_APPISTOL;
		case PICKUP_WEAPON_MICROSMG_TYPE:
			return PICKUP_WEAPON_MICROSMG;
		case PICKUP_WEAPON_SMG_TYPE:
			return PICKUP_WEAPON_SMG;
		case PICKUP_WEAPON_ASSAULTSMG_TYPE:
			return PICKUP_WEAPON_ASSAULTSMG;
		case PICKUP_WEAPON_ASSAULTRIFLE_TYPE:
			return PICKUP_WEAPON_ASSAULTRIFLE;
		case PICKUP_WEAPON_CARBINERIFLE_TYPE:
			return PICKUP_WEAPON_CARBINERIFLE;
		case PICKUP_WEAPON_HEAVYRIFLE_TYPE:
			return PICKUP_WEAPON_HEAVYRIFLE;
		case PICKUP_WEAPON_ADVANCEDRIFLE_TYPE:
			return PICKUP_WEAPON_ADVANCEDRIFLE;
		case PICKUP_WEAPON_MG_TYPE:
			return PICKUP_WEAPON_MG;
		case PICKUP_WEAPON_COMBATMG_TYPE:
			return PICKUP_WEAPON_COMBATMG;
		case PICKUP_WEAPON_ASSAULTMG_TYPE:
			return PICKUP_WEAPON_ASSAULTMG;
		case PICKUP_WEAPON_PUMPSHOTGUN_TYPE:
			return PICKUP_WEAPON_PUMPSHOTGUN;
		case PICKUP_WEAPON_SAWNOFFSHOTGUN_TYPE:
			return PICKUP_WEAPON_SAWNOFFSHOTGUN;
		case PICKUP_WEAPON_BULLPUPSHOTGUN_TYPE:
			return PICKUP_WEAPON_BULLPUPSHOTGUN;
		case PICKUP_WEAPON_ASSAULTSHOTGUN_TYPE:
			return PICKUP_WEAPON_ASSAULTSHOTGUN;
		case PICKUP_WEAPON_SNIPERRIFLE_TYPE:
			return PICKUP_WEAPON_SNIPERRIFLE;
		case PICKUP_WEAPON_ASSAULTSNIPER_TYPE:
			return PICKUP_WEAPON_ASSAULTSNIPER;
		case PICKUP_WEAPON_HEAVYSNIPER_TYPE:
			return PICKUP_WEAPON_HEAVYSNIPER;
		case PICKUP_WEAPON_GRENADELAUNCHER_TYPE:
			return PICKUP_WEAPON_GRENADELAUNCHER;
		case PICKUP_WEAPON_RPG_TYPE:
			return PICKUP_WEAPON_RPG;
		case PICKUP_WEAPON_MINIGUN_TYPE:
			return PICKUP_WEAPON_MINIGUN;
		case PICKUP_WEAPON_GRENADE_TYPE:
			return PICKUP_WEAPON_GRENADE;
		case PICKUP_WEAPON_SMOKEGRENADE_TYPE:
			return PICKUP_WEAPON_SMOKEGRENADE;
		case PICKUP_WEAPON_STICKYBOMB_TYPE:
			return PICKUP_WEAPON_STICKYBOMB;
		case PICKUP_WEAPON_MOLOTOV_TYPE:
			return PICKUP_WEAPON_MOLOTOV;
		case PICKUP_WEAPON_STUNGUN_TYPE:
			return PICKUP_WEAPON_STUNGUN;
		case PICKUP_WEAPON_RUBBERGUN_TYPE:
			return PICKUP_WEAPON_RUBBERGUN;
		case PICKUP_WEAPON_PROGRAMMABLEAR_TYPE:
			return PICKUP_WEAPON_PROGRAMMABLEAR;
		case PICKUP_WEAPON_FIREEXTINGUISHER_TYPE:
			return PICKUP_WEAPON_FIREEXTINGUISHER;
		case PICKUP_WEAPON_PETROLCAN_TYPE:
			return PICKUP_WEAPON_PETROLCAN;
		case PICKUP_WEAPON_LOUDHAILER_TYPE:
			return PICKUP_WEAPON_LOUDHAILER;
#if LASSO_WEAPON_EXISTS
		case PICKUP_WEAPON_LASSO_TYPE:
			return PICKUP_WEAPON_LASSO;
#endif
		case PICKUP_WEAPON_KNIFE_TYPE:
			return PICKUP_WEAPON_KNIFE;
		case PICKUP_WEAPON_NIGHTSTICK_TYPE:
			return PICKUP_WEAPON_NIGHTSTICK;
		case PICKUP_WEAPON_HAMMER_TYPE:
			return PICKUP_WEAPON_HAMMER;
		case PICKUP_WEAPON_BAT_TYPE:
			return PICKUP_WEAPON_BAT;
		case PICKUP_WEAPON_GOLFCLUB_TYPE:
			return PICKUP_WEAPON_GOLFCLUB;

		// vehicle pickups
		case PICKUP_VEHICLE_HEALTH_STANDARD_TYPE:
			return PICKUP_VEHICLE_HEALTH_STANDARD;
		case PICKUP_VEHICLE_ARMOUR_STANDARD_TYPE:
			return PICKUP_VEHICLE_ARMOUR_STANDARD;
		case PICKUP_VEHICLE_WEAPON_PISTOL_TYPE:
			return PICKUP_VEHICLE_WEAPON_PISTOL;
		case PICKUP_VEHICLE_WEAPON_COMBATPISTOL_TYPE:
			return PICKUP_VEHICLE_WEAPON_COMBATPISTOL;
		case PICKUP_VEHICLE_WEAPON_PISTOL50_TYPE:
			return PICKUP_VEHICLE_WEAPON_PISTOL50;
		case PICKUP_VEHICLE_WEAPON_APPISTOL_TYPE:
			return PICKUP_VEHICLE_WEAPON_APPISTOL;
		case PICKUP_VEHICLE_WEAPON_MICROSMG_TYPE:
			return PICKUP_VEHICLE_WEAPON_MICROSMG;
		case PICKUP_VEHICLE_WEAPON_SMG_TYPE:
			return PICKUP_VEHICLE_WEAPON_SMG;
		case PICKUP_VEHICLE_WEAPON_SAWNOFF_TYPE:
			return PICKUP_VEHICLE_WEAPON_SAWNOFF;
		case PICKUP_VEHICLE_WEAPON_ASSAULTSMG_TYPE:
			return PICKUP_VEHICLE_WEAPON_ASSAULTSMG;
		case PICKUP_VEHICLE_WEAPON_GRENADE_TYPE:
			return PICKUP_VEHICLE_WEAPON_GRENADE;
		case PICKUP_VEHICLE_WEAPON_SMOKEGRENADE_TYPE:
			return PICKUP_VEHICLE_WEAPON_SMOKEGRENADE;
		case PICKUP_VEHICLE_WEAPON_STICKYBOMB_TYPE:
			return PICKUP_VEHICLE_WEAPON_STICKYBOMB;
		case PICKUP_VEHICLE_WEAPON_MOLOTOV_TYPE:
			return PICKUP_VEHICLE_WEAPON_MOLOTOV;

		case PICKUP_AMMO_BULLET_MP_TYPE:
			return PICKUP_AMMO_BULLET_MP;
		case PICKUP_AMMO_MISSILE_MP_TYPE:
			return PICKUP_AMMO_MISSILE_MP;

		case PICKUP_PORTABLE_CRATE_UNFIXED_TYPE:
			return PICKUP_PORTABLE_CRATE_UNFIXED;
		case PICKUP_VEHICLE_CUSTOM_SCRIPT_TYPE:
			return PICKUP_VEHICLE_CUSTOM_SCRIPT;
		case PICKUP_SUBMARINE_TYPE:
			return PICKUP_SUBMARINE;

		case PICKUP_WEAPON_CROWBAR_TYPE:
			return PICKUP_WEAPON_CROWBAR;
		case PICKUP_HEALTH_SNACK_TYPE:
			return PICKUP_HEALTH_SNACK;

		case PICKUP_PARACHUTE_TYPE:
			return PICKUP_PARACHUTE;
		default:
			Assertf(0, "Unknown pickup type %d", PickupType);
		}
	}
	return PickupType;
}

int  CommandCreatePickup( int PickupHash, const scrVector & scrVecNewCoors, int PickupFlags, int amount, bool bScriptHostObject, int modelHashKey )
{
	PickupHash = ConvertOldPickupTypeToHash(PickupHash);

	Vector3 pickupPosition = Vector3(scrVecNewCoors);
	Vector3 pickupOrientation = Vector3(1.0f, 1.0f, 1.0f); 

#if __BANK
	if (PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_DEBUG_CREATED)
	{
		Assertf(0, "Script pickup being created at %f, %f, %f as a debug pickup - bad flags set", pickupPosition.x, pickupPosition.y, pickupPosition.z);
		PickupFlags &= ~CPickupPlacement::PLACEMENT_CREATION_FLAG_DEBUG_CREATED;
	}
#endif

	bool bMapPickup = PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_MAP;

	// map pickups are only needed in MP as the flag dictates how the pickup is networked 
	if (!scriptVerifyf(!bMapPickup || NetworkInterface::IsGameInProgress(), "CREATE_PICKUP - PLACEMENT_FLAG_MAP set on a pickup in single player. This is only used in MP. Ignoring."))
	{
		PickupFlags &= ~CPickupPlacement::PLACEMENT_CREATION_FLAG_MAP;
		bMapPickup = false;
	}

	// ignore the blip flags, they are not used anymore
	scriptAssertf(PickupFlags < (1<<CPickupPlacement::PLACEMENT_FLAG_NUM_CREATION_FLAGS), "CREATE_PICKUP - invalid flags"); 

	const CPickupData* pPickupData = CPickupDataManager::GetPickupData((u32)PickupHash);

	if (scriptVerifyf(pPickupData, "CREATE_PICKUP - invalid pickup type"))
	{
#if !__NO_OUTPUT
		const CBaseModelInfo* pModelInfo = NULL;
#endif
		u32 customModelIndex = fwModelId::MI_INVALID;

		if (modelHashKey != 0 && CTheScripts::Frack2())
		{
			fwModelId modelId;
#if !__NO_OUTPUT
			pModelInfo = 
#endif
				CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);

			if (scriptVerifyf(modelId.IsValid(), "%s: CREATE_PICKUP - the custom model is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
				scriptVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "%s: CREATE_PICKUP - the custom model is not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				customModelIndex = modelId.GetModelIndex();
			}
		}

		if (bScriptHostObject && !bMapPickup && (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
		{
			if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "CREATE_PICKUP - Non-host machines cannot create host pickups"))
			{
				return NULL_IN_SCRIPTING_LANGUAGE;
			}
		}

		CPickupPlacement* pPlacement = CPickupManager::RegisterPickupPlacement((u32)PickupHash, pickupPosition, pickupOrientation, (PlacementFlags)PickupFlags, NULL, customModelIndex);
		
		if (pPlacement)
		{
#if !__NO_OUTPUT
			scriptDebugf1("%s: CREATE_PICKUP 0x%p, PickupHash %d, overridden model %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPlacement, PickupHash, pModelInfo ?  pModelInfo->GetModelName() : "null");
#endif

			if (CTheScripts::GetCurrentGtaScriptHandler()->RegisterNewScriptObject(static_cast<scriptHandlerObject&>(*pPlacement), bScriptHostObject, !bMapPickup))
			{
				if (amount >= 0)
				{
					pPlacement->SetAmount(static_cast<u16>(amount));
				}

				if (scriptVerify(pPlacement->GetScriptInfo()))
				{
					return pPlacement->GetScriptInfo()->GetObjectId();
				}
			}
			else
			{
				CPickupManager::RemovePickupPlacement(pPlacement);
			}
		}
	}

	return NULL_IN_SCRIPTING_LANGUAGE;
}

void CommandSetCustomPickupWeaponHash(int weaponHash, int pickupPlacementIndex)
{
	CPickupPlacement* pPickupPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(pickupPlacementIndex));

	if (pPickupPlacement)
	{
		pPickupPlacement->SetWeaponHash(weaponHash);
	}
}

void CommandForcePickupRotateFaceUp()
{
	CPickupManager::SetForceRotatePickupFaceUp();
}

int  CommandCreatePickupRotate( int PickupHash, const scrVector & scrVecCoors, const scrVector & scrVecOrientation, int PickupFlags, int amount, s32 RotOrder, bool bScriptHostObject, int modelHashKey )
{
	PickupHash = ConvertOldPickupTypeToHash(PickupHash);

	scriptAssertf(PickupFlags < (1<<CPickupPlacement::PLACEMENT_FLAG_NUM_CREATION_FLAGS), "CREATE_PICKUP_ROTATE - invalid flags"); 

	const CPickupData* pPickupData = CPickupDataManager::GetPickupData((u32)PickupHash);

	if (scriptVerifyf(pPickupData, "CREATE_PICKUP_ROTATE - invalid pickup type"))
	{
		Vector3 pickupPosition = Vector3(scrVecCoors);
		Vector3 pickupOrientation = Vector3(scrVecOrientation);
		pickupOrientation *= DtoR;	//	Convert the Orientation vector to Radians

#if __BANK
		if (PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_DEBUG_CREATED)
		{
			Assertf(0, "Script pickup being created at %f, %f, %f as a debug pickup - bad flags set", pickupPosition.x, pickupPosition.y, pickupPosition.z);
			PickupFlags &= ~CPickupPlacement::PLACEMENT_CREATION_FLAG_DEBUG_CREATED;
		}
#endif
		bool bMapPickup = PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_MAP;

		// map pickups are only needed in MP as the flag dictates how the pickup is networked 
		if (!scriptVerifyf(!bMapPickup || NetworkInterface::IsGameInProgress(), "CREATE_PICKUP_ROTATE - PLACEMENT_FLAG_MAP set on a pickup in single player. This is only used in MP. Ignoring."))
		{
			PickupFlags &= ~CPickupPlacement::PLACEMENT_CREATION_FLAG_MAP;
			bMapPickup = false;
		}

		// Convert the order of Euler rotations from whatever was passed in to XYZ so that the rest of the pickup code can work the same as it always did
		Quaternion quatRotation;
		CScriptEulers::QuaternionFromEulers(quatRotation, pickupOrientation, static_cast<EulerAngleOrder>(RotOrder));
		pickupOrientation = CScriptEulers::QuaternionToEulers(quatRotation, EULER_XYZ);

		u32 customModelIndex = fwModelId::MI_INVALID;

#if !__NO_OUTPUT
		const CBaseModelInfo* pModelInfo = NULL;
#endif
		if (CTheScripts::Frack() && modelHashKey != 0)
		{
			fwModelId modelId;

#if !__NO_OUTPUT
			pModelInfo = 
#endif
				CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);

			if (scriptVerifyf(modelId.IsValid(), "%s: CREATE_PICKUP_ROTATE - the custom model is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
				scriptVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "%s: CREATE_PICKUP_ROTATE - the custom model is not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				customModelIndex = modelId.GetModelIndex();
			}
		}

		if (bScriptHostObject && !bMapPickup && (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
		{
			if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "CREATE_PICKUP_ROTATE - Non-host machines cannot create host pickups"))
			{
				return NULL_IN_SCRIPTING_LANGUAGE;
			}
		}

		CPickupPlacement* pPlacement = CPickupManager::RegisterPickupPlacement((u32)PickupHash, pickupPosition, pickupOrientation, (PlacementFlags)PickupFlags, NULL, customModelIndex);

		if (pPlacement && CTheScripts::Frack2())
		{
#if !__NO_OUTPUT
			scriptDebugf1("%s: CREATE_PICKUP_ROTATE 0x%p, PickupHash %d, overridden model %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPlacement, PickupHash, pModelInfo ?  pModelInfo->GetModelName() : "null");
#endif

			if (CTheScripts::GetCurrentGtaScriptHandler()->RegisterNewScriptObject(static_cast<scriptHandlerObject&>(*pPlacement), bScriptHostObject, !bMapPickup))
			{
				if (amount >= 0)
				{
					pPlacement->SetAmount(static_cast<u16>(amount));
				}

				if (scriptVerify(pPlacement->GetScriptInfo()))
				{
					return pPlacement->GetScriptInfo()->GetObjectId();
				}
			}
			else
			{
				CPickupManager::RemovePickupPlacement(pPlacement);
			}
		}
	}

	return NULL_IN_SCRIPTING_LANGUAGE;
}

void CommandBlockPlayersForPickup(int /*pickupID*/, int /*playerFlags*/) 
{
	/*
	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		scriptHandlerObject* pObject = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(pickupID);

		if (scriptVerifyf(pObject, "BLOCK_PLAYERS_FOR_PICKUP - Pickup does not exist"))
		{
			if (scriptVerifyf(static_cast<CGameScriptHandlerObject*>(pObject)->GetType() == SCRIPT_HANDLER_OBJECT_TYPE_PICKUP, "BLOCK_PLAYERS_FOR_PICKUP - This is not a pickup id"))
			{
				CPickupPlacement* pPlacement = SafeCast(CPickupPlacement, pObject);
			
				if(scriptVerifyf(pPlacement->GetIsMapPlacement(), "BLOCK_PLAYERS_FOR_PICKUP - This is currently only supported for map pickups (using PLACEMENT_FLAG_MAP)"))
				{
					scriptAssertf(0, "BLOCK_PLAYERS_FOR_PICKUP is deprecated");
					//pPlacement->SetPlayersAvailability(playerFlags);
				}
			}
		}
	}
	*/
	scriptAssertf(0, "BLOCK_PLAYERS_FOR_PICKUP is deprecated");
}

void CommandBlockPlayersForAmbientPickup(int pickupIndex, int playerFlag)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(pickupIndex);

	if(pPickup)
	{
		if(pPickup->GetNetworkObject())
		{
#if __BANK
			if(NetworkUtils::IsNetworkCloneOrMigrating(pPickup))
			{
				scriptAssertf(false, "This function can only be called on players who own the pickup. Pickup index:%d", pickupIndex);
				return;
			}
#endif
			static_cast<CNetObjPickup*>(pPickup->GetNetworkObject())->SetBlockedPlayersFromCollecting(playerFlag);
		}
		else
		{
			scriptAssertf(false, "Can't block players of pickup that has no network object. Pickup index:%d", pickupIndex);
		}
	}
	else
	{
		scriptAssertf(false, "Can't block players of pickup that doesn't exist. Pickup index:%d", pickupIndex);
	}
}

int CreateAmbientPickup( int PickupHash, const scrVector & scrVecNewCoors, int PickupFlags, int amount, int modelHashKey, bool bCreateAsScriptObject, bool bScriptHostObject, bool bNetworked )
{
	PickupHash = ConvertOldPickupTypeToHash(PickupHash);

	int iPickupIndex = NULL_IN_SCRIPTING_LANGUAGE;

	scriptAssertf(PickupFlags < (1<<CPickupPlacement::PLACEMENT_FLAG_NUM_CREATION_FLAGS), "CREATE_AMBIENT_PICKUP - invalid flags"); 

	const CPickupData* pPickupData = CPickupDataManager::GetPickupData((u32)PickupHash);

	if (bCreateAsScriptObject && bScriptHostObject && (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()))
	{
		if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "CREATE_AMBIENT_PICKUP - Non-host machines cannot create host pickups"))
		{
			return iPickupIndex;
		}
	}

	if (scriptVerifyf(pPickupData, "CREATE_AMBIENT_PICKUP - invalid pickup type"))
	{
		strLocalIndex customModelIndex = strLocalIndex(fwModelId::MI_INVALID);

		if (modelHashKey != 0)
		{
			fwModelId modelId;
			CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);

			if (scriptVerifyf(modelId.IsValid(), "%s: CREATE_AMBIENT_PICKUP - the custom model is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
				scriptVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "%s: CREATE_AMBIENT_PICKUP - the custom model is not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				customModelIndex = modelId.GetModelIndex();
			}
		}

		Matrix34 placementM;
		placementM.Identity();
		placementM.d = Vector3(scrVecNewCoors);

		CPickup* pPickup = CPickupManager::CreatePickup((u32)PickupHash, placementM, NULL, bNetworked, customModelIndex.Get(), true, bCreateAsScriptObject);

		if (pPickup)
		{
			// Should we attempt to snap this to the ground?
			if( (PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_SNAP_TO_GROUND) != 0 )
			{
				bool bOrientToGround = (PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_ORIENT_TO_GROUND) !=0;
				bool bUpright = (PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_UPRIGHT) !=0;

				pPickup->SetPlaceOnGround(bOrientToGround,bUpright);
			}

			if (amount >= 0)
			{
				pPickup->SetAmount(amount);
			}

			if (bCreateAsScriptObject)
			{
				CTheScripts::RegisterEntity(pPickup, bScriptHostObject, bNetworked);
			}

			pPickup->SetDontFadeOut();

			if( (PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_PLAYER_GIFT) != 0 )
			{
				pPickup->SetPlayerGift();
			}
			
			if( (PickupFlags & CPickupPlacement::PLACEMENT_CREATION_FLAG_ON_OBJECT) != 0 ) 
			{
				pPickup->SetLyingOnFixedObject();
			}

			iPickupIndex = CTheScripts::GetGUIDFromEntity(*pPickup);
		}
	}

	return iPickupIndex;
}

int CommandCreateAmbientPickup( int pickupHash, const scrVector & scrVecNewCoors, int PickupFlags, int amount, int modelHashKey, bool bCreateAsScriptObject, bool bScriptHostObject)
{
	bool networked = true;

	if (NetworkInterface::IsGameInProgress())
	{
		//@@: range COMMANDCREATEAMBIENTPICKUP {
		// we want to send telemetry to detect cheater who spawn cash
		if(CPickupManager::PickupHashIsMoneyType(pickupHash))
		{
			networked = CPickupManager::IsNetworkedMoneyPickupAllowed();
			u64 id = 0;
			rlCreateUUID(&id);

			//Should we append cash created metric locally.
			bool appendCashCreated = true;
			//@@: location COMMANDCREATEAMBIENTPICKUP_CASH_CREATED
			const unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
			if (numRemotePhysicalPlayers > 0)
			{
				//Random my ass. ;)
				const int index = fwRandom::GetRandomNumberInRange(0, (int)numRemotePhysicalPlayers);

				const netPlayer* const* remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();
				const netPlayer* pPlayer = remotePhysicalPlayers[index];
				if (pPlayer)
				{
					//Don't send locally since a peer is already sending it for us.
					appendCashCreated = false;

					// Send an event to the other peers so they also send the metric
					CReportCashSpawnEvent::Trigger(id, amount, pickupHash, pPlayer->GetActivePlayerIndex());
				}
			}

			//We didn't manage to send the request to any peer, so try locally.
			if (appendCashCreated)
			{
				CNetworkTelemetry::AppendCashCreated(id,  NetworkInterface::GetLocalGamerHandle(), amount, pickupHash);
			}
		}
		//@@: } COMMANDCREATEAMBIENTPICKUP
	}

	return CreateAmbientPickup( pickupHash, scrVecNewCoors, PickupFlags, amount, modelHashKey, bCreateAsScriptObject, bScriptHostObject, networked);

}

int CommandCreateNonNetworkedAmbientPickup( int PickupHash, const scrVector & scrVecNewCoors, int PickupFlags, int amount, int modelHashKey, bool bCreateAsScriptObject, bool bScriptHostObject)
{
	return CreateAmbientPickup( PickupHash, scrVecNewCoors, PickupFlags, amount, modelHashKey, bCreateAsScriptObject, bScriptHostObject, false );
}

int CreatePortablePickup( int PickupHash, const scrVector & scrVecNewCoors, bool SnapToGround, int modelHashKey, bool bNetworked )
{	
	PickupHash = ConvertOldPickupTypeToHash(PickupHash);

	int iPickupIndex = NULL_IN_SCRIPTING_LANGUAGE;

	const CPickupData* pPickupData = CPickupDataManager::GetPickupData((u32)PickupHash);

	if (scriptVerifyf(pPickupData, "CREATE_PORTABLE_PICKUP - invalid pickup type") &&
		scriptVerifyf(pPickupData->GetInvisibleWhenCarried(),  "CREATE_PORTABLE_PICKUP - this pickup type is not portable"))
	{
		if (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent())
		{
			if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "CREATE_PORTABLE_PICKUP - Non-host machines cannot create host pickups"))
			{
#if __BANK
				if(NetworkInterface::IsGameInProgress() && PARAM_netDebugCreatePortablePickup.Get())
				{
					Displayf("CreatePortablePickup SUCCESS iPickupIndex %d. PickupHash 0x%x, scrVecNewCoors %.2f, %.2f, %.2f, SnapToGround %s, modelHashKey 0x%x, bNetworked %s. Frame: %d",
						iPickupIndex,PickupHash,scrVecNewCoors.x,scrVecNewCoors.y,scrVecNewCoors.z,  SnapToGround?"TRUE":"FALSE",  modelHashKey,  bNetworked?"TRUE":"FALSE",fwTimer::GetFrameCount() );
				}
#endif

				return iPickupIndex;
			}
		}

		if (scriptVerifyf(pPickupData->GetAttachmentBone() != BONETAG_INVALID, "CREATE_PORTABLE_PICKUP - this pickup type is not setup as a portable pickup in pickups.meta"))
		{
			strLocalIndex customModelIndex = strLocalIndex(fwModelId::MI_INVALID);

			if (modelHashKey != 0)
			{
				fwModelId modelId;
				CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);

				if (scriptVerifyf(modelId.IsValid(), "%s: CREATE_PORTABLE_PICKUP - the custom model is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
					scriptVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "%s: CREATE_PORTABLE_PICKUP - the custom model is not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					customModelIndex = modelId.GetModelIndex();
				}
			}

			Matrix34 placementM;
			placementM.Identity();
			placementM.d = Vector3(scrVecNewCoors);

			CPickup* pPickup = CPickupManager::CreatePickup((u32)PickupHash, placementM, NULL, bNetworked, customModelIndex.Get(), true, true);

			if (pPickup)
			{
				pPickup->SetPortable();
				pPickup->GetLodData().SetResetDisabled(true);
				CTheScripts::RegisterEntity(pPickup, bNetworked, bNetworked);

				if (SnapToGround)
				{			
					pPickup->SetPlaceOnGround(true, true);
				}

				iPickupIndex = CTheScripts::GetGUIDFromEntity(*pPickup);
			}
		}
	}

#if __BANK
	if(NetworkInterface::IsGameInProgress()  && PARAM_netDebugCreatePortablePickup.Get())
	{
		Displayf("CreatePortablePickup return iPickupIndex %d. PickupHash 0x%x, scrVecNewCoors %.2f, %.2f, %.2f, SnapToGround %s, modelHashKey 0x%x, bNetworked %s. Frame: %d",
			iPickupIndex,PickupHash,scrVecNewCoors.x,scrVecNewCoors.y,scrVecNewCoors.z,  SnapToGround?"TRUE":"FALSE",  modelHashKey,  bNetworked?"TRUE":"FALSE",fwTimer::GetFrameCount() );
	}
#endif
	return iPickupIndex;
}

int CommandCreatePortablePickup( int PickupHash, const scrVector & scrVecNewCoors, bool SnapToGround, int modelHashKey )
{
	return CreatePortablePickup( PickupHash, scrVecNewCoors, SnapToGround, modelHashKey, true );
}

int CommandCreateNonNetworkedPortablePickup( int PickupHash, const scrVector & scrVecNewCoors, bool SnapToGround, int modelHashKey )
{
	return CreatePortablePickup( PickupHash, scrVecNewCoors, SnapToGround, modelHashKey, false );
}

void CommandAttachPortablePickupToPed(int pickupID, int pedID)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(pickupID);
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedID);

	if (pPickup && pPed)
	{
		// avoid asserts if this command has been called twice and the pickup is still trying to attach itself to the ped
		if (pPickup->GetPendingCarrier() == pPed )
		{
			return;
		}

		if (scriptVerifyf(pPickup->IsFlagSet(CPickup::PF_Portable), "ATTACH_PORTABLE_PICKUP_TO_PED - the pickup is not portable") && 
			scriptVerifyf(!pPickup->GetIsAttached(), "ATTACH_PORTABLE_PICKUP_TO_PED - the pickup is already attached") &&
			scriptVerifyf(!pPed->GetPlayerInfo() || !pPed->GetPlayerInfo()->PortablePickupPending, "ATTACH_PORTABLE_PICKUP_TO_PED - there is another pickup pending attachement to this ped"))
		{
			pPickup->AttachPortablePickupToPed(pPed, "ATTACH_PORTABLE_PICKUP_TO_PED script command");
		}
	}
}

void CommandDetachPortablePickupFromPed(int pickupID)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(pickupID);

	if (pPickup)
	{
		if (scriptVerifyf(pPickup->IsFlagSet(CPickup::PF_Portable), "DETACH_PORTABLE_PICKUP_FROM_PED - the pickup is not portable") && 
			scriptVerifyf(pPickup->GetIsAttached(), "DETACH_PORTABLE_PICKUP_FROM_PED - the pickup is not attached") &&
			scriptVerifyf(static_cast<CEntity*>(pPickup->GetAttachParent())->GetIsTypePed(), "DETACH_PORTABLE_PICKUP_FROM_PED - the pickup is not attached to a ped"))
		{
			pPickup->DetachPortablePickupFromPed("DETACH_PORTABLE_PICKUP_FROM_PED");
		}
	}
}

void CommandForcePortablePickupLastAccessiblePositionSetting(int pickupID)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(pickupID);
	if (pPickup)
	{
#if ENABLE_NETWORK_LOGGING
		if (NetworkInterface::IsGameInProgress())
		{
			netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
			NetworkLogUtils::WriteLogEvent(log, "PORTABLE_PICKUP_FORCE_LAST_ACCESSIBLE_POSITION", "%s", pPickup->GetLogName());
			log.WriteDataValue("Position", "x:%.2f, y:%.2f, z:%.2f", pPickup->GetTransform().GetPosition().GetXf(), pPickup->GetTransform().GetPosition().GetYf(), pPickup->GetTransform().GetPosition().GetZf());
			log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif
		pPickup->ForceSetLastAccessibleLocation();
	}
}

void CommandHidePortablePickupWhenDetached(int pickupID, bool bHide)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(pickupID);

	if (pPickup)
	{
		if (scriptVerifyf(pPickup->IsFlagSet(CPickup::PF_Portable), "HIDE_PORTABLE_PICKUP_WHEN_DETACHED - the pickup is not portable")) 
		{
			pPickup->SetHideWhenDetached(bHide);
		}
	}
}

void CommandSetMaxNumPortablePickupsCarriedByPlayer(int modelHashKey, int max)
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();

	if (scriptVerifyf(pPlayerPed, "SET_MAX_NUM_PORTABLE_PICKUPS_CARRIED_BY_PLAYER - local player does not exist") &&
		scriptVerifyf(max >= -1, "SET_MAX_NUM_PORTABLE_PICKUPS_CARRIED_BY_PLAYER - max is invalid (%d)", max) &&
		AssertVerify(pPlayerPed->GetPlayerInfo()))
	{
		if (modelHashKey != 0)
		{
			fwModelId modelId;
			CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);

			if (scriptVerifyf(modelId.IsValid(), "%s: SET_MAX_NUM_PORTABLE_PICKUPS_CARRIED_BY_PLAYER - the model is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				pPlayerPed->GetPlayerInfo()->SetMaxPortablePickupsOfTypePermitted(modelId.GetModelIndex(), max);
			}
		}
		else 
		{
			pPlayerPed->GetPlayerInfo()->m_maxPortablePickupsCarried = (s8)max;
		}
	}
}

void CommandSetLocalPlayerCanCollectPortablePickups(bool bCanCollect)
{
	CPed * pPlayerPed = CGameWorld::FindLocalPlayer();

	if (scriptVerifyf(pPlayerPed, "SET_LOCAL_PLAYER_CAN_COLLECT_PORTABLE_PICKUPS - local player does not exist") &&
		AssertVerify(pPlayerPed->GetPlayerInfo()))
	{
		if (bCanCollect)
		{
			if (pPlayerPed->GetPlayerInfo()->m_maxPortablePickupsCarried == 0)
			{
				pPlayerPed->GetPlayerInfo()->m_maxPortablePickupsCarried = -1;
			}
		}
		else
		{
			pPlayerPed->GetPlayerInfo()->m_maxPortablePickupsCarried = 0;
		}

#if ENABLE_NETWORK_LOGGING
		if (NetworkInterface::IsGameInProgress())
		{
			netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
			NetworkLogUtils::WriteLogEvent(log, "SET_LOCAL_PLAYER_CAN_COLLECT_PORTABLE_PICKUPS", "%s", bCanCollect ? "true" : "false");
			log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif
	}
}

bool CommandIsPickupRewardTypeSuppressed( s32 nPickupFlag )
{
	return CPickupManager::IsSuppressionFlagSet( nPickupFlag );
}

void CommandSuppressPickupRewardType( s32 nPickupFlag, bool bClearPreviousFlags )
{
	if( bClearPreviousFlags )
		CPickupManager::ClearSuppressionFlags();

	CPickupManager::SetSuppressionFlag( nPickupFlag );

#if ENABLE_NETWORK_LOGGING
	if (NetworkInterface::IsGameInProgress())
	{
		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "SUPPRESS_PICKUP_REWARD", "%d", nPickupFlag);
		log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptName());
	}
#endif
}

void CommandClearAllPickupRewardTypeSuppression( void )
{
	CPickupManager::ClearSuppressionFlags();

#if ENABLE_NETWORK_LOGGING
	if (NetworkInterface::IsGameInProgress())
	{
		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "CLEAR_ALL_PICKUP_REWARD_SUPPRESSION", "");
		log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptName());
	}
#endif
}

void CommandClearPickupRewardTypeSuppression( s32 nPickupFlag )
{
#if ENABLE_NETWORK_LOGGING
	if (NetworkInterface::IsGameInProgress())
	{
		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "CLEAR_PICKUP_REWARD_SUPPRESSION", "%d", nPickupFlag);
		log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptName());
	}
#endif

	return CPickupManager::ClearSuppressionFlag( nPickupFlag );
}

void CommandRenderFakePickup(const scrVector & pos, int glowType)
{
	CPickupManager::RenderScriptedGlow(pos,glowType);
}

void CommandSetPickupObjectCollectableInVehicle(int pickupId)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(pickupId);

	if (pPickup)
	{
		scriptAssertf(!pPickup->GetPlacement(), "SET_PICKUP_OBJECT_COLLECTABLE_IN_VEHICLE - the pickup object has been spawned by a pickup - use SET_PICKUP_COLLECTABLE_ON_MOUNT instead to avoid losing the this setting");
		scriptAssertf(pPickup->GetPickupData()->GetCollectableOnFoot(), "SET_PICKUP_OBJECT_COLLECTABLE_IN_VEHICLE - this can only be called on pickups collectable on foot");

		pPickup->SetAllowCollectionInVehicle();
	}
}

void CommandSetPickupTrackDamageEvents(int pickupId, bool set)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(pickupId);

	if (pPickup)
	{
#if ENABLE_NETWORK_LOGGING
		netLoggingInterface *log = &NetworkInterface::GetObjectManager().GetLog();
		if (log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "SET_PICKUP_TRACK_DAMAGE_EVENTS", pPickup->GetLogName());
			log->WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			log->WriteDataValue("Enable", "%s", set ? "TRUE" : "FALSE");
		}
#endif // ENABLE_NETWORK_LOGGING
		pPickup->SetForceWaitForFullySyncBeforeDestruction(set);
	}
}

scrVector CommandGetPickupCoords( int iPickupID )
{
	Vector3 coordVec(0.0,0.0,-100.0);

	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));

		if (SCRIPT_VERIFY(pPlacement,"GET_PICKUP_COORDS - Pickup does not exist (or has been collected)" ))
		{
			coordVec = pPlacement->GetPickupPosition();
		}
	}

	return coordVec;
}

void CommandSuppressPickupSoundForPickup( int iPickupID, bool suppress ) 
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(iPickupID);

	if(pPickup)
	{
		pPickup->SetSuppressPickupAudio(suppress);		
	}
}

void CommandRemoveAllPickupsOfType(int iPickupHash)
{
	iPickupHash = ConvertOldPickupTypeToHash(iPickupHash);
	CPickupManager::RemoveAllPickupsOfType((u32)iPickupHash);
}

int CommandGetNumberOfPickupsOfType(int iPickupHash)
{
	iPickupHash = ConvertOldPickupTypeToHash(iPickupHash);
	return CPickupManager::CountPickupsOfType((u32)iPickupHash);
}

bool CommandHasPickupBeenCollected( int PickupID )
{
	CGameScriptObjInfo objInfo(CGameScriptId(*CTheScripts::GetCurrentGtaScriptThread()), PickupID, 0);

	return CPickupManager::HasPickupBeenCollected(objInfo);
}

void CommandRemovePickup( int PickupID )
{
	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		scriptHandlerObject* pObject = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID);

		if (scriptVerifyf(pObject, "REMOVE_PICKUP - Pickup does not exist"))
		{
			if (scriptVerifyf(static_cast<CGameScriptHandlerObject*>(pObject)->GetType() == SCRIPT_HANDLER_OBJECT_TYPE_PICKUP, "REMOVE_PICKUP - This is not a pickup id"))
			{
				CPickupPlacement* pPlacement = SafeCast(CPickupPlacement, pObject);

				bool pickupIsLocal = !pPlacement->GetNetworkObject() || pPlacement->GetIsMapPlacement() || (!pPlacement->IsNetworkClone() && !pPlacement->GetNetworkObject()->IsPendingOwnerChange());

				if (SCRIPT_VERIFY(pickupIsLocal, "REMOVE_PICKUP - Cannot remove a pickup controlled by another machine"))
				{
					CPickupManager::RemovePickupPlacement(pPlacement);
				}
			}
		}
	}
}

void CommandCreateMoneyPickups( const scrVector & scrVecCoors, int amount, int numPickups, int modelHashKey)
{
	Vector3 moneyPosition = Vector3(scrVecCoors);

	SCRIPT_ASSERT(numPickups<=8, "CREATE_MONEY_PICKUPS - Can't create more than 8 money pickups" );

	strLocalIndex customModelIndex = strLocalIndex(fwModelId::MI_INVALID);

	if (modelHashKey != 0 && CTheScripts::Frack())
	{
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHashKey, &modelId);

		if (scriptVerifyf(modelId.IsValid(), "%s: CREATE_MONEY_PICKUPS - the custom model is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(CModelInfo::HaveAssetsLoaded(modelId), "%s: CREATE_MONEY_PICKUPS - the custom model is not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			customModelIndex = modelId.GetModelIndex();
		}
	}

	CPickupManager::CreateSomeMoney(moneyPosition, amount, numPickups, customModelIndex.Get());
}

void CommandSetPlayersDropMoneyInNetworkGame(bool /*bValue*/)
{
	SCRIPT_ASSERT(false,"SET_PLAYERS_DROP_MONEY_IN_NETWORK_GAME - Needs reimplementation. Speak to John G or Phil" );
	//CPickups::m_bPlayersDropMoneyInNetworkGame = bValue;
}

bool CommandRotateObject( int iObjectID, float fTargetRotation, float fRotationStep, bool bStopForCollisionFlag )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectID);
	bool bResult = false;
		
	if (pObj)
	{
		float fHeading;
		float RotationDifference1, RotationDifference2;
		bool ObjectHasCollided;
		Matrix34 temp_mat;
		Vector3 BBoxVMin, BBoxVMax;
		Vector3 Point1, Point2, Point3, Point4;

		fHeading = ( RtoD * pObj->GetTransform().GetHeading());
		if (fHeading < 0.0f)
		{
			fHeading += 360.0f;
		}
		scriptAssertf(fRotationStep > 0.0f, "%s:ROTATE_OBJECT - Rotation Step must be greater than 0.0", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (fHeading == fTargetRotation)
		{
			bResult = TRUE;
		}
		else
		{
			RotationDifference1 = fTargetRotation - fHeading;
			RotationDifference2 = fHeading - fTargetRotation;
			if (RotationDifference1 < 0.0f)
			{
				RotationDifference1 += 360.0f;
			}
			if (RotationDifference2 < 0.0f)
			{
				RotationDifference2 += 360.0f;
			}
			if (RotationDifference1 < RotationDifference2)
				{
				if (RotationDifference1 < fRotationStep)
				{
					fHeading = fTargetRotation;
				}
				else
				{
					fHeading += fRotationStep;
				}
			}
			else
				{
				if (RotationDifference2 < fRotationStep)
				{
					fHeading = fTargetRotation;
				}
				else
				{
					fHeading -= fRotationStep;
				}
			}
			ObjectHasCollided = FALSE;
			if (bStopForCollisionFlag)
			{
				Vector3 TempCoors = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());
				temp_mat.Identity();
				temp_mat.RotateLocalZ(( DtoR * fHeading));
				temp_mat.Translate(TempCoors);
				BBoxVMin = pObj->GetBaseModelInfo()->GetBoundingBoxMin();
				BBoxVMax = pObj->GetBaseModelInfo()->GetBoundingBoxMax();
				Point1 = Vector3(BBoxVMin.x, BBoxVMin.y, BBoxVMin.z);
				Point2 = Vector3(BBoxVMax.x, BBoxVMin.y, BBoxVMax.z);
				Point3 = Vector3(BBoxVMax.x, BBoxVMax.y, BBoxVMin.z);
				Point4 = Vector3(BBoxVMin.x, BBoxVMax.y, BBoxVMax.z);
				Point1 = temp_mat * Point1;
				Point2 = temp_mat * Point2;
				Point3 = temp_mat * Point3;
				Point4 = temp_mat * Point4;
			}
			if (ObjectHasCollided)
			{
				bResult = TRUE;
			}
			else
			{
				pObj->SetHeading(( DtoR * fHeading));
				if (fHeading == fTargetRotation)
				{
					bResult = TRUE;
				}
			}
		}
	}
	return(bResult);
}

bool CommandSlideObject( int iObjectID, const scrVector & scrVecDestCoors, const scrVector & scrVecIncrement, bool bStopOnCollision )
{
	Vector3 VecDestCoors = Vector3 (scrVecDestCoors);
	Vector3 VecIncrement = Vector3 (scrVecIncrement);
	
	bool bResult = false;
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectID);

	if (pObj)
	{
		float XDiff, YDiff, ZDiff;
		bool ObjectHasCollided;
		Matrix34 temp_mat;
		Vector3 BBoxVMin, BBoxVMax;
		Vector3 Point1, Point2, Point3, Point4;

		Vector3 TempCoors = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition());

		//if ((TempCoors.x == VecIncrement.x) && (TempCoors.y == VecIncrement.y) && (TempCoors.z == VecIncrement.z))
		if (TempCoors == VecDestCoors)
		{
			bResult = TRUE;
		}
		else
		{
			XDiff = TempCoors.x - VecDestCoors.x;
			YDiff = TempCoors.y - VecDestCoors.y;
			ZDiff = TempCoors.z - VecDestCoors.z;
			scriptAssertf(VecIncrement.x >= 0.0f, "%s:SLIDE_OBJECT - X Increment must be positive", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptAssertf(VecIncrement.y >= 0.0f, "%s:SLIDE_OBJECT - Y Increment must be positive", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptAssertf(VecIncrement.z >= 0.0f, "%s:SLIDE_OBJECT - Z Increment must be positive", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			if (XDiff >= 0.0f)
			{
				if (XDiff > VecIncrement.x)
				{
					TempCoors.x -= VecIncrement.x;
				}
				else
				{
					TempCoors.x = VecDestCoors.x;
				}
			}
			else
			{
				XDiff = -XDiff;
				if (XDiff > VecIncrement.x)
				{
					TempCoors.x += VecIncrement.x;
				}
				else
				{
					TempCoors.x = VecDestCoors.x;
				}
			}
			if (YDiff >= 0.0f)
			{
				if (YDiff > VecIncrement.y)
				{
					TempCoors.y -= VecIncrement.y;
				}
				else
				{
					TempCoors.y = VecDestCoors.y;
				}
			}
			else
			{
				YDiff = -YDiff;
				if (YDiff > VecIncrement.y)
				{
					TempCoors.y += VecIncrement.y;
				}
				else
				{
					TempCoors.y = VecDestCoors.y;
				}
			}
			if (ZDiff >= 0.0f)
			{
				if (ZDiff > VecIncrement.z)
				{
					TempCoors.z -= VecIncrement.z;
				}
				else
				{
					TempCoors.z = VecDestCoors.z;
				}
			}
			else
			{
				ZDiff = -ZDiff;
				if (ZDiff > VecIncrement.z)
				{
					TempCoors.z += VecIncrement.z;
				}
				else
				{
					TempCoors.z = VecDestCoors.z;
				}
			}
			ObjectHasCollided = FALSE;
			if (bStopOnCollision)
			{
				temp_mat = MAT34V_TO_MATRIX34(pObj->GetMatrix());
				temp_mat.d.SetX(TempCoors.x);
				temp_mat.d.SetY(TempCoors.y);
				temp_mat.d.SetZ(TempCoors.z);
				BBoxVMin = pObj->GetBaseModelInfo()->GetBoundingBoxMin();
				BBoxVMax = pObj->GetBaseModelInfo()->GetBoundingBoxMax();
				Point1 = Vector3(BBoxVMin.x, BBoxVMin.y, BBoxVMin.z);
				Point2 = Vector3(BBoxVMax.x, BBoxVMin.y, BBoxVMax.z);
				Point3 = Vector3(BBoxVMax.x, BBoxVMax.y, BBoxVMin.z);
				Point4 = Vector3(BBoxVMin.x, BBoxVMax.y, BBoxVMax.z);
				Point1 = temp_mat * Point1;
				Point2 = temp_mat * Point2;
				Point3 = temp_mat * Point3;
				Point4 = temp_mat * Point4;

			}
			if (ObjectHasCollided)
			{
				bResult = TRUE;
			}
			else
			{
				pObj->Teleport(TempCoors);
				if ((TempCoors.x == VecDestCoors.x) && (TempCoors.y == VecDestCoors.y) && (TempCoors.z == VecDestCoors.z))
				{
					bResult = TRUE;
				}
				else
				{
					bResult = FALSE;
				}
			}
		}
	}

	return(bResult);
}

void CommandSetObjectTargettable( int iObjectID, bool bTargettable )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectID);

	if (pObj)
	{
		CPed * pPlayerPed = CGameWorld::FindLocalPlayer();
		if (bTargettable)
		{
			pObj->SetObjectTargettable(true);
		}
		else
		{
			pObj->SetObjectTargettable(false);
			CPlayerPedTargeting & targeting = pPlayerPed->GetPlayerInfo()->GetTargeting();
			if (pObj == targeting.GetLockOnTarget() )
			{
				targeting.SetLockOnTarget(NULL);
			}
		}
	}
}

void CommandSetObjectForceVehiclesToAvoid(int iObjectID, bool bAvoid )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectID);

	if (pObj)
	{
		pObj->SetForceVehiclesToAvoid(bAvoid);
	}
}

s32 CommandGetClosestObjectOfType(const scrVector & scrVecCentreCoors, float Radius, int ObjectModelHashKey, bool bRegisterAsScriptObject, bool bScriptHostObject, bool bRegisterAsNetworkObject)
{
	s32 ReturnObjectIndex = NULL_IN_SCRIPTING_LANGUAGE;

	Vector3 VecNewCoors = Vector3(scrVecCentreCoors);

	// non-networked objects should always be treated as client objects
	if (bRegisterAsScriptObject && bScriptHostObject)
	{
		if (!bRegisterAsNetworkObject) 
		{
			scriptAssertf(0, "GET_CLOSEST_OBJECT_OF_TYPE - Script host objects must be networked. Forcing this to true");
			bRegisterAsNetworkObject = true;
		}

		if (CTheScripts::GetCurrentGtaScriptHandlerNetwork() && CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent())
		{
			if (!SCRIPT_VERIFY (CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal(), "GET_CLOSEST_OBJECT_OF_TYPE - Non-host machines cannot create host objects"))
			{
				return ReturnObjectIndex;
			}
		}
	}

	// B*1982595 & 2111681 & 2116196: Tell the closest object lookup to only consider objects with the specific "ownedby" status that we are looking for (See switch statement below).
	// This prevents the command from picking up frag objects and the like and allows it to find an actually useful object if one exists in the search range.
	s32 nOwnedByFlags = 0xFFFFFFFF;
	if(bRegisterAsScriptObject)
	{
		nOwnedByFlags = ENTITY_OWNEDBY_MASK_RANDOM | ENTITY_OWNEDBY_MASK_TEMP | ENTITY_OWNEDBY_MASK_SCRIPT;
	}

	CEntity *pClosestEntity = CObject::GetPointerToClosestMapObjectForScript(VecNewCoors.x, VecNewCoors.y, VecNewCoors.z, Radius, ObjectModelHashKey, ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT, nOwnedByFlags);
	if (pClosestEntity)
	{
	    CObject *pClosestObject = NULL;

	    if (pClosestEntity->GetIsTypeDummyObject())
	    {
		    CDummyObject *pDummyObject = static_cast<CDummyObject*>(pClosestEntity);
		    pClosestObject = CObjectPopulation::ConvertToRealObject(pDummyObject);

		    scriptAssertf(pClosestObject, "%s:GET_CLOSEST_OBJECT_OF_TYPE - Failed to convert dummy object %s to a real object", 
				CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName());
	    }
	    else if (pClosestEntity->GetIsTypeObject())
	    {
		    pClosestObject = static_cast<CObject*>(pClosestEntity);

			if (pClosestObject->IsADoor() && (bRegisterAsScriptObject||bRegisterAsNetworkObject) && NetworkInterface::IsGameInProgress())
			{
				scriptAssertf(0, "%s:GET_CLOSEST_OBJECT_OF_TYPE - you can't call this on a door (%s) if you want to register as a script object or network it. Use the correct door commands.", 
					CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName());
				return ReturnObjectIndex;
			}
	    }
	    else
	    {
		    scriptAssertf(0, "%s:GET_CLOSEST_OBJECT_OF_TYPE - closest entity %s is not an object or a dummy object", 
				CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName());
	    }

	    if (pClosestObject)
	    {
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive() == false)
			{
				CReplayMgr::RecordMapObject(pClosestObject);
			}
#endif
			if (bRegisterAsScriptObject && pClosestObject->IsNetworkClone())
			{
				scriptAssertf(0, "%s:GET_CLOSEST_OBJECT_OF_TYPE - can't register as a script object - the entity %s is not controlled by this machine (net id: %s)", 
					CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName(), pClosestObject->GetNetworkObject()->GetLogName());
				bRegisterAsScriptObject = false;
			}

			if (bRegisterAsScriptObject)
			{
				// if this is a network game and we don't have an existing network object, register
				if (bRegisterAsNetworkObject && NetworkInterface::IsGameInProgress() && !pClosestObject->GetNetworkObject())
                {
					NetworkInterface::RegisterObject(pClosestObject, 0, CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true);
                    
                    if(pClosestObject && pClosestObject->GetNetworkObject())
                    {
                        CNetObjObject *pNetObjObject = SafeCast(CNetObjObject, pClosestObject->GetNetworkObject());
                        pNetObjObject->SetScriptGrabParameters(VecNewCoors, Radius);
                    }
                }

				CDummyObject *pRelatedDummy = NULL;
				fwInteriorLocation RelatedDummyLocation;
				s32 IplIndexOfObject = 0;

				switch (pClosestObject->GetOwnedBy())
				{
				case ENTITY_OWNEDBY_RANDOM :
					{
						pRelatedDummy = pClosestObject->GetRelatedDummy();
						RelatedDummyLocation = pClosestObject->GetRelatedDummyLocation();
						IplIndexOfObject = pClosestObject->GetIplIndex();

						CTheScripts::RegisterEntity(pClosestObject, bScriptHostObject, bRegisterAsNetworkObject);
						scriptAssertf(pClosestObject->GetRelatedDummy() == NULL, "%s:GET_CLOSEST_OBJECT_OF_TYPE - expected CObject::SetupMissionState() to have called ClearRelatedDummy() for %s", 
							CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName());

						pClosestObject->SetIplIndex(0);
					}
					break;

				case ENTITY_OWNEDBY_TEMP :
					//	If an object has been grabbed then marked as no longer needed, it will be a Temp object
					//	Until Changelist 2412021, it would have been ENTITY_OWNEDBY_RANDOM
					scriptAssertf(0, "%s:GET_CLOSEST_OBJECT_OF_TYPE - GetOwnedBy() returned ENTITY_OWNEDBY_TEMP for the entity with model %s so it's not safe to return to script. Maybe this entity had already been grabbed once from the world and later set as no longer needed", 
						CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName());
					return NULL_IN_SCRIPTING_LANGUAGE;
	//					break;

				case ENTITY_OWNEDBY_SCRIPT :
					{	//	If this is already a mission object
						const GtaThread* pCreationThread = pClosestObject->GetThreadThatCreatedMe();
						if (scriptVerifyf(pCreationThread, "%s:GET_CLOSEST_OBJECT_OF_TYPE - creation thread not found (object's model is %s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName()) )
						{
							if (pCreationThread==CTheScripts::GetCurrentGtaScriptThread())
							{	//	If the mission object was grabbed/created by this script, return its ID here
								//	Don't run all the model swap code
								scriptAssertf(0, "%s:GET_CLOSEST_OBJECT_OF_TYPE - entity with model %s is already a mission object owned by this script", 
											  CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName());

								return CTheScripts::GetGUIDFromEntity(*pClosestObject);
							}
							else
							{	//	If the mission object was grabbed/created by another script then assert and return NULL
								scriptAssertf(0, "%s:GET_CLOSEST_OBJECT_OF_TYPE - entity with model %s created by %s, trying to mark as mission entity by %s", 
									CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName(), const_cast<GtaThread*>(pCreationThread)->GetScriptName(), CTheScripts::GetCurrentScriptName());
							}
						}

						return NULL_IN_SCRIPTING_LANGUAGE;
					}
					break;

				default :
					scriptAssertf(0, "%s:GET_CLOSEST_OBJECT_OF_TYPE - Unexpected GetOwnedBy() value %d for entity with model %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestObject->GetOwnedBy(), pClosestEntity->GetModelName());
					return NULL_IN_SCRIPTING_LANGUAGE;
	//				    break;
				}

				CScriptEntityExtension* pExtension = pClosestObject->GetExtension<CScriptEntityExtension>();

				if (scriptVerifyf(pExtension, "%s:GET_CLOSEST_OBJECT_OF_TYPE - failed to get the script extension for the closest object with model %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestEntity->GetModelName()))
				{
					pExtension->SetScriptObjectWasGrabbedFromTheWorld(true);

					ReturnObjectIndex = CTheScripts::GetGUIDFromEntity(*pClosestObject);

					spdSphere searchVolume((Vec3V) VecNewCoors, ScalarV(Radius));
					CTheScripts::GetHiddenObjects().AddToHiddenObjectMap(ReturnObjectIndex, searchVolume, ObjectModelHashKey, 
						pRelatedDummy, RelatedDummyLocation, IplIndexOfObject);
				}
			}
			else
			{
				ReturnObjectIndex = CTheScripts::GetGUIDFromEntity(*pClosestObject);
			}
	    }
	}

	return ReturnObjectIndex;
}

bool CommandHasObjectBeenBroken( int iObjectID, bool networked )
{
	const CObject *pObj = CTheScripts::GetEntityToQueryFromGUID<CObject>(iObjectID);

	if (pObj)
	{
		// If we want to fetch the networked value instead of the local one
		if (networked)
		{
			if(pObj->GetNetworkObject() && pObj->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_OBJECT)
			{
				CNetObjObject *netObjObject = SafeCast(CNetObjObject, pObj->GetNetworkObject());

				if(netObjObject)
				{
					return netObjObject->IsObjectDamaged();
				}
			}
		}
		else
		{
			if (pObj->IsObjectDamaged())
			{
				return true;
			}
		}		
	}
	return false;
}

const u32 SearchFlag_Exterior = 0x01;
const u32 SearchFlag_Interior = 0x02;

bool CommandHasClosestObjectOfTypeBeenBroken(const scrVector & scrVecCoors, float Radius, int ObjectModelHashKey, s32 SearchFlags)
{
	Vector3 VecCoors = Vector3(scrVecCoors);

	bool bResult = false;

	float NewX = VecCoors.x;
	float NewY = VecCoors.y;
	float NewZ = VecCoors.z;

	if (NewZ <= INVALID_MINIMUM_WORLD_Z)
	{
		NewZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, NewX, NewY);
	}

	fwModelId ObjectModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ObjectModelHashKey, &ObjectModelId);		//	ignores return value
	if (SCRIPT_VERIFY(ObjectModelId.IsValid(), "HAS_CLOSEST_OBJECT_OF_TYPE_BEEN_BROKEN - this is not a valid model index"))
	{
		ClosestObjectDataStruct ClosestObjectData;
		ClosestObjectData.fClosestDistanceSquared = Radius * Radius * 4.0f;	//	set this to larger than the scan range (remember it's squared distance)
		ClosestObjectData.pClosestObject = NULL;

		ClosestObjectData.CoordToCalculateDistanceFrom = Vector3(NewX, NewY, NewZ);

		ClosestObjectData.ModelIndexToCheck = ObjectModelId.GetModelIndex();
		ClosestObjectData.bCheckModelIndex = true;
		ClosestObjectData.bCheckStealableFlag = false;

		ClosestObjectData.nOwnedByMask = 0xFFFFFFFF;

		s32 SearchLocFlags = SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS;
		if (scriptVerifyf( (SearchFlags & (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS) ) != 0, "HAS_CLOSEST_OBJECT_OF_TYPE_BEEN_BROKEN - no SEARCH_LOCATION_FLAGS have been set"))
		{
			if ((SearchFlags & SearchFlag_Exterior) == 0)
			{
				SearchLocFlags = SEARCH_LOCATION_INTERIORS;	//	Search interiors only
			}

			if ((SearchFlags & SearchFlag_Interior) == 0)
			{
				SearchLocFlags = SEARCH_LOCATION_EXTERIORS;	//	Search exteriors only
			}
		}

		fwIsSphereIntersecting testSphere(spdSphere(RCC_VEC3V(ClosestObjectData.CoordToCalculateDistanceFrom), ScalarV(Radius)));
		CGameWorld::ForAllEntitiesIntersecting(&testSphere, CTheScripts::GetClosestObjectCB, (void*) &ClosestObjectData, 
			ENTITY_TYPE_MASK_OBJECT, SearchLocFlags,
			SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_SCRIPT);	//	|ENTITY_TYPE_MASK_DUMMY_OBJECT

		if (ClosestObjectData.pClosestObject && ClosestObjectData.pClosestObject->GetIsTypeObject())
		{
			CObject *pObj = (CObject *)ClosestObjectData.pClosestObject;
			if (pObj->IsObjectDamaged())
			{
				bResult = true;
			}
		}
	}
	return bResult;
}

bool CommandHasClosestObjectOfTypeBeenCompletelyDestroyed(const scrVector & scrVecCoors, float Radius, int ObjectModelHashKey, s32 SearchFlags)
{
	Vector3 VecCoors = Vector3(scrVecCoors);

	bool bResult = false;

	float NewX = VecCoors.x;
	float NewY = VecCoors.y;
	float NewZ = VecCoors.z;

	if(NewZ <= INVALID_MINIMUM_WORLD_Z)
	{
		NewZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, NewX, NewY);
	}

	fwModelId ObjectModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ObjectModelHashKey, &ObjectModelId);		//	ignores return value
	if(SCRIPT_VERIFY(ObjectModelId.IsValid(), "HAS_CLOSEST_OBJECT_OF_TYPE_BEEN_COMPLETELY_DESTROYED - this is not a valid model index"))
	{
		ClosestObjectDataStruct ClosestObjectData;
		ClosestObjectData.fClosestDistanceSquared = Radius * Radius * 4.0f;	//	set this to larger than the scan range (remember it's squared distance)
		ClosestObjectData.pClosestObject = NULL;

		ClosestObjectData.CoordToCalculateDistanceFrom = Vector3(NewX, NewY, NewZ);

		ClosestObjectData.ModelIndexToCheck = ObjectModelId.GetModelIndex();
		ClosestObjectData.bCheckModelIndex = true;
		ClosestObjectData.bCheckStealableFlag = false;

		ClosestObjectData.nOwnedByMask = 0xFFFFFFFF;

		s32 SearchLocFlags = SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS;
		if(scriptVerifyf( (SearchFlags & (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS) ) != 0, "HAS_CLOSEST_OBJECT_OF_TYPE_BEEN_COMPLETELY_DESTROYED - no SEARCH_LOCATION_FLAGS have been set"))
		{
			if ((SearchFlags & SearchFlag_Exterior) == 0)
			{
				SearchLocFlags = SEARCH_LOCATION_INTERIORS;	//	Search interiors only
			}

			if ((SearchFlags & SearchFlag_Interior) == 0)
			{
				SearchLocFlags = SEARCH_LOCATION_EXTERIORS;	//	Search exteriors only
			}
		}

		fwIsSphereIntersecting testSphere(spdSphere(RCC_VEC3V(ClosestObjectData.CoordToCalculateDistanceFrom), ScalarV(Radius)));
		CGameWorld::ForAllEntitiesIntersecting(&testSphere, CTheScripts::GetClosestObjectCB, (void*) &ClosestObjectData, 
			ENTITY_TYPE_MASK_OBJECT, SearchLocFlags,
			SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_SCRIPT);	//	|ENTITY_TYPE_MASK_DUMMY_OBJECT

		if(ClosestObjectData.pClosestObject && ClosestObjectData.pClosestObject->GetIsTypeObject())
		{
			CObject* pObj = static_cast<CObject*>(ClosestObjectData.pClosestObject);
			Assert(pObj);
			fragInst* pFragInst = pObj->GetFragInst();
			if(SCRIPT_VERIFY(pFragInst, "HAS_CLOSEST_OBJECT_OF_TYPE_BEEN_COMPLETELY_DESTROYED - Doesn't make sense to call this function on a non fraggable object."))
			{
				if(pFragInst->GetCached() && !pFragInst->GetCacheEntry()->IsFurtherBreakable())
					bResult = true;
			}
		}
	}
	return bResult;
}

bool CommandGetHasObjectBeenCompletelyDestroyed( int iObjectID )
{
	const CObject *pObj = CTheScripts::GetEntityToQueryFromGUID<CObject>( iObjectID );
	bool bResult = false;

	if( pObj )
	{
		fragInst* pFragInst = pObj->GetFragInst();

		if( SCRIPT_VERIFY( pFragInst, "GET_HAS_OBJECT_BEEN_COMPLETELY_DESTROYED - Doesn't make sense to call this function on a non fraggable object." ) )
		{
			if( pFragInst->GetCached() && !pFragInst->GetCacheEntry()->IsFurtherBreakable() )
			{
				bResult = true;
			}
		}
	}
	return bResult;
}

scrVector CommandGetOffsetFromCoordAndHeadingInWorldCoords( const scrVector & scrVecCoors, float fHeadingDegs, const scrVector & scrOffset )
{
	const float fHeadingRads = DtoR * fHeadingDegs;
	Vector3 vResult = scrVecCoors;
	Vector3 vRotatedOffset = scrOffset;
	vRotatedOffset.x = scrOffset.x * cosf(fHeadingRads) - scrOffset.y * sinf(fHeadingRads);
	vRotatedOffset.y = scrOffset.y * cosf(fHeadingRads) + scrOffset.x * sinf(fHeadingRads);
	vResult += vRotatedOffset;
	return vResult;
}


void CommandObjectFragmentBreakChild( int iObjectID, int iComponent, bool bDisappear )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectID);

	if (pObj)
	{
		if (pObj && pObj->m_nFlags.bIsFrag && pObj->GetCurrentPhysicsInst())
		{
			fragInst* pFragInst = (fragInst*)pObj->GetCurrentPhysicsInst();

			if (iComponent > -1 && !pFragInst->GetChildBroken(iComponent))
			{
				if (bDisappear)
				{
					pFragInst->DeleteAbove(iComponent);
				}
				else
				{
					Assertf(!pFragInst->IsBreakingDisabled(), "This command will have no effect with breaking disabled. Call SET_DISABLE_BREAKING first");
					if(pFragInst->GetTypePhysics()->GetChild(iComponent)->GetOwnerGroupPointerIndex() == 0)
					{
						pFragInst->BreakOffRoot();
					}
					else
					{
						pFragInst->BreakOffAbove(iComponent);
					}
				}
			}
		}
	}
}


void CommandObjectFragmentDamageChild( int iObjectID, int iComponent, float newHealth )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectID);

	if (pObj && pObj->m_nFlags.bIsFrag && pObj->GetCurrentPhysicsInst())
	{
		fragInst* pFragInst = (fragInst*)pObj->GetCurrentPhysicsInst();
		fragPhysicsLOD* pFragLOD = pFragInst->GetTypePhysics();

		if (Verifyf(iComponent > -1 && iComponent < pFragLOD->GetNumChildren(), "Component %d out of range [0, %d) for prop %s", iComponent, pFragLOD->GetNumChildren(), pFragInst->GetType()->GetTuneName()))
		{
			int iGroupIndex = pFragLOD->GetChild(iComponent)->GetOwnerGroupPointerIndex();
			if (!pFragInst->GetCached() || pFragInst->GetCacheEntry()->IsGroupUnbroken(iGroupIndex))
			{
				pFragInst->ReduceGroupDamageHealth(iGroupIndex, newHealth, pFragInst->GetCenterOfMass(), Vec3V(V_ZERO));
			}
		}
	}
}


void CommandObjectFragmentFix( int iObjectID )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectID);

	if (pObj) 
	{
		if (pObj && pObj->m_nFlags.bIsFrag && pObj->GetCurrentPhysicsInst())
		{
			fragInst* pFragInst = (fragInst*)pObj->GetCurrentPhysicsInst();
			if (pFragInst)
			{
				fragCacheEntry *pCacheEntry = pFragInst->GetCacheEntry();
				if (pCacheEntry)
				{
					FRAGCACHEMGR->ReleaseCacheEntry(pCacheEntry);
				}
				pFragInst->PutIntoCache();
			}
		}
	}
}


void CommandFreezePositionOfClosestObjectOfType(const scrVector & scrVecCoors, float Radius, int ObjectModelHashKey, bool bFreezeFlag)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);

	CEntity *pClosestObj = CObject::GetPointerToClosestMapObjectForScript(VecCoors.x, VecCoors.y, VecCoors.z, Radius, ObjectModelHashKey);
	if (pClosestObj)
	{
		if (SCRIPT_VERIFY(!NetworkUtils::IsNetworkClone(pClosestObj), "FREEZE_POSITION_OF_CLOSEST_OBJECT_OF_TYPE - Cannot call this command on an entity owned by another machine!"))
		{       
			if (SCRIPT_VERIFY(!CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL)->GetUsesDoorPhysics(), "FREEZE_POSITION_OF_CLOSEST_OBJECT_OF_TYPE - not allowed on a door"))
			{       
				if (SCRIPT_VERIFY(pClosestObj->GetIsTypeObject(), "FREEZE_POSITION_OF_CLOSEST_OBJECT_OF_TYPE - closest object is not a proper object"))
				{       
					((CObject *)pClosestObj)->SetFixedPhysics(bFreezeFlag);
				}
			}
		}
	}
}


void CommandTrackObjectVisibility(int ObjectID)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if (pObject)
	{
		pObject->SetUseOcclusionQuery(true);
	}
}


bool CommandIsObjectVisible(int ObjectID)
{
	bool result = false;
	
	const CObject* pObject = CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectID);

	if (pObject)
	{
		scriptAssertf(pObject->GetUseOcclusionQuery(), "%s:IS_OBJECT_VISIBLE - Object visibility isn't tracked", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		fwDrawData* drawData = pObject->GetDrawHandlerPtr();
		if( drawData )
		{
			u32 queryId = drawData->GetOcclusionQueryId();
			
			if( queryId )
			{
				int pixelCount = OcclusionQueries::OQGetQueryResult(queryId);
				result = pixelCount > 10; // 10 pixels should be enough for everybody.
			}
		}
	}
	
	return result;
}


void CommandSetObjectIsSpecialGolfBall(int ObjectID, bool IsGolfBall)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectID);
	if (pObject)
	{
		pObject->m_nDEflags.bIsGolfBall = IsGolfBall;
	}
}

void CommandSetTakesDamageFromCollidingWithBuildings(int ObjectID, bool bTakesDamage)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectID);
	if (pObject)
	{
		pObject->SetTakesDamageFromCollisionsWithBuildings(bTakesDamage);
	}
}

void CommandAllowDamageEventsForNonNetworkedObjects(bool allow)
{
#if ENABLE_NETWORK_LOGGING
	if(NetworkInterface::IsGameInProgress())
	{
		netLoggingInterface *log = &NetworkInterface::GetObjectManager().GetLog();
		if (log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "ALLOW_DAMAGE_EVENTS_FOR_NON_NETWORKED_OBJECTS", "%s", allow ? "TRUE" : "FALSE");
		}
	}
#endif // ENABLE_NETWORK_LOGGING
	CObject::ms_bAllowDamageEventsForNonNetworkedObjects = allow;
}

void CommandSetCutscenesWeaponFlashlightOnThisFrame(int ObjectID, bool bForceOn)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectID);
	if (SCRIPT_VERIFY(pObject, "FORCE_WEAPON_FLASHLIGHT_ON_THIS_FRAME_FOR_CUTSCENE - Could not find entity from ID"))
	{
		if (SCRIPT_VERIFY(pObject->GetWeapon(), "FORCE_WEAPON_FLASHLIGHT_ON_THIS_FRAME_FOR_CUTSCENE - Object is not a weapon"))
		{
			if (pObject->GetWeapon()->GetFlashLightComponent())
			{
				pObject->GetWeapon()->GetFlashLightComponent()->SetForceFlashLightOn(bForceOn);
			}
		}
	}
}

bool CommandGetCoordsAndRotationOfClosestObjectOfType(const scrVector & scrVecCoords, float fRadius, int ObjectModelHashKey, Vector3& vOutCoords, Vector3& vOutRot, s32 RotOrder)
{
	Vector3 VecCoords = Vector3(scrVecCoords);
	
	vOutCoords = VEC3_ZERO;
	vOutRot = VEC3_ZERO;

	CEntity *pClosestObj = CObject::GetPointerToClosestMapObjectForScript(VecCoords.x, VecCoords.y, VecCoords.z, fRadius, ObjectModelHashKey);
	if (pClosestObj)
	{
		if (pClosestObj->GetIsTypeObject())
		{	
            //	Only return true if the object is a proper object - not a dummy or building
			vOutCoords = VEC3V_TO_VECTOR3(pClosestObj->GetTransform().GetPosition());
			
			Matrix34 m = MAT34V_TO_MATRIX34(pClosestObj->GetMatrix());
			vOutRot = CScriptEulers::MatrixToEulers(m, static_cast<EulerAngleOrder>(RotOrder));
			vOutRot *= RtoD;

			return true;
		}
	}

	return false;
}

void ClosestDoorAssertHelper(CDoor::ChangeDoorStateResult result, int ASSERT_ONLY(ObjectModelHashKey), const char * ASSERT_ONLY(functionName))
{
	// is it a lockable door
	if(result == CDoor::CHANGE_DOOR_STATE_NOT_DOOR)
	{
		scriptAssertf(0, "%s is not a door object\n", CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL)->GetModelName());
	}
	else if(result == CDoor::CHANGE_DOOR_STATE_INVALID_OBJECT_TYPE)
	{
		SCRIPT_ASSERT_TWO_STRINGS(0, "%s - closest object must be a dummy or proper object", functionName);
	}
	else if(result == CDoor::CHANGE_DOOR_STATE_IS_BUILDING)
	{
		SCRIPT_ASSERT_TWO_STRINGS(0, "%s - closest object with the given model is a building", functionName);
	}
	else if(result == CDoor::CHANGE_DOOR_STATE_NO_OBJECT)
	{
		SCRIPT_ASSERT_TWO_STRINGS(0, "%s - Couldn't find an object with the given model", functionName);
	}
}

void CommandSetStateOfClosestDoorOfType(int ObjectModelHashKey, const scrVector & scrVecPos, bool bLockState, float fOpenRatio, bool bRemoveSpring)
{
	if(SCRIPT_VERIFY( CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL), "SET_STATE_OF_CLOSEST_DOOR_OF_TYPE - this is not a valid model index"),
		SCRIPT_VERIFY( !NetworkInterface::IsGameInProgress(), "SET_STATE_OF_CLOSEST_DOOR_OF_TYPE - not supported in MP"))
	{
		// Vector3 doorPosition = Vector3(scrVecPos);

		if (bRemoveSpring && bLockState)
		{
			bRemoveSpring = false;
			SCRIPT_ASSERT(0, "SET_STATE_OF_CLOSEST_DOOR_OF_TYPE - Cannot remove door spring at same time as locking a door");
		}

		CDoor::ChangeDoorStateResult result = CDoor::ChangeDoorStateForScript(ObjectModelHashKey, Vector3(scrVecPos), NULL, false, bLockState, fOpenRatio, bRemoveSpring);

		if (result != CDoor::CHANGE_DOOR_STATE_SUCCESS)
		{
			#if __DEV
				if (result == CDoor::CHANGE_DOOR_STATE_NO_OBJECT)
				{
					const char *pModelName = CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL)->GetModelName();
					if (!pModelName)
					{
						pModelName = "FAILED LOOKING UP MODEL NAME";
					}

					float radius = 6.0f;
					CEntity *pClosestObj = CObject::GetPointerToClosestMapObjectForScript(scrVecPos.x, scrVecPos.y, scrVecPos.z, radius, ObjectModelHashKey);
					if (pClosestObj)
					{
						Vector3 position = VEC3V_TO_VECTOR3(pClosestObj->GetTransform().GetPosition());
						scriptAssertf(0, "%s: SET_STATE_OF_CLOSEST_DOOR_OF_TYPE - Couldn't find an object %s at position %f, %f, %f\n" 
										 "A subsequent search for object %s found it at position %f, %f, %f", 										 
										CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
										pModelName,
										scrVecPos.x,
										scrVecPos.y,
										scrVecPos.z,
										pModelName, 
										position.x, 
										position.y, 
										position.z);
						return;
					}

					scriptAssertf(false, "%s: SET_STATE_OF_CLOSEST_DOOR_OF_TYPE - Couldn't find an object %s at position %f, %f, %f\n", 
									CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
									pModelName,
									scrVecPos.x,
									scrVecPos.y,
									scrVecPos.z);
					return;
				}
			#endif
			ClosestDoorAssertHelper(result, ObjectModelHashKey, "SET_STATE_OF_CLOSEST_DOOR_OF_TYPE");
		}
	}
}

void CommandGetStateOfClosestDoorOfType(int ObjectModelHashKey, const scrVector & scrVecPos, int &bReturnLockState, float &fReturnOpenRatio)
{
	if(SCRIPT_VERIFY( CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL), "GET_STATE_OF_CLOSEST_DOOR_OF_TYPE - this is not a valid model index"))
	{
		bReturnLockState = false;
		fReturnOpenRatio = 0.0f;

		// is it a lockable door
		CBaseModelInfo *modelInfo = CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL);
		if( modelInfo->GetUsesDoorPhysics() )
		{
			Vector3 vecTestPos = Vector3(scrVecPos);		
			vecTestPos.x = rage::Floorf(vecTestPos.x + 0.5f);
			vecTestPos.y = rage::Floorf(vecTestPos.y + 0.5f);
			vecTestPos.z = rage::Floorf(vecTestPos.z + 0.5f);

			// Determine the radius to search within, matching SET_STATE_OF_CLOSEST_DOOR_OF_TYPE.
			// If it's a sliding door that moves as it's being opened, the radius may have to be
			// larger than if it's a rotating one.
			const float radius = CDoor::GetScriptSearchRadius(*modelInfo);

			CEntity *pClosestObj = CDoor::FindClosestDoor(modelInfo, vecTestPos, radius);
			
			if (!pClosestObj)
			{
				pClosestObj = CObject::GetPointerToClosestMapObjectForScript(vecTestPos.x, vecTestPos.y, vecTestPos.z, radius, ObjectModelHashKey);
				// MAGDEMO 
				// Removed this assert for the magdemo as its really just a warning that I had made an assert.
				// Assertf(!pClosestObj, "CDoor::FindClosestDoor failed to find the door but GetPointerToClosestMapObjectForScript did add bug for David Ely");
			}

			if (pClosestObj)
			{
				bReturnLockState = pClosestObj->GetIsAnyFixedFlagSet();

                if (SCRIPT_VERIFY(pClosestObj->GetIsTypeObject() || pClosestObj->GetIsTypeDummyObject(), "GET_STATE_OF_CLOSEST_DOOR_OF_TYPE - closest object must be a dummy or proper object"))
				{   
					if(pClosestObj->GetIsTypeObject() && static_cast<CObject*>(pClosestObj)->IsADoor())
						fReturnOpenRatio = static_cast<CDoor*>(pClosestObj)->GetDoorOpenRatio();
				}
			}
		}
		else
		{
			scriptAssertf(false, "%s is not a door object\n", CModelInfo::GetBaseModelInfoFromHashKey(ObjectModelHashKey, NULL)->GetModelName());
		}
	}
}

void CommandSetLockedUnstreamedInDoorOfType(int ObjectModelHashKey, const scrVector & scrVecPos, bool bLockState, float fAutomaticRate, float fAutomaticDist, float fOpenRatio)
{
	if (SCRIPT_VERIFY( !NetworkInterface::IsGameInProgress(), "SET_LOCKED_UNSTREAMED_IN_DOOR_OF_TYPE - not supported in MP"))
	{
		Vector3 doorPosition = Vector3(scrVecPos);

		bool bRemoveSpring = false;

		bool bSmoothLock = bLockState;
		if(fabsf(fOpenRatio) > SMALL_FLOAT)
		{
			// In this case, the door is not actually requested to close. Make sure
			// to not activate the smooth close-and-lock feature, which could conflict
			// with the request to be open.
			bSmoothLock = false;
		}

		CDoor::ChangeDoorStateResult result = CDoor::ChangeDoorStateForScript(ObjectModelHashKey, Vector3(scrVecPos), NULL, true, bLockState, fOpenRatio, bRemoveSpring, bSmoothLock, fAutomaticRate, fAutomaticDist, false);

		if(result != CDoor::CHANGE_DOOR_STATE_SUCCESS)
		{
			//Object not streamed in on host so just set mapped global locked object status
			CDoorSystem::SetLocked(doorPosition, bLockState, fOpenRatio, bRemoveSpring, ObjectModelHashKey, fAutomaticRate, fAutomaticDist, NULL,  false);
		}
	}
}

void CommandPlayObjectAutoStartAnim(int ObjectIndex)
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObject)
	{
		pObject->PlayAutoStartAnim();
	}
}

void CommandAddDoorToSystem(int doorEnumHash, int hashKey, const scrVector & position, bool useOldOverrides, bool network, bool permanent)
{
	// atFinalizeHash(u32 key) ... key += (key << 15), thus every valid has must be greater than 1 << 15.  This assert exists to catch scripters passing enums instead of hashes
	scriptAssertf(u32(doorEnumHash) > u32(1 << 15), "%s: ADD_DOOR_TO_SYSTEM passed an invalid hash %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), doorEnumHash);

	CDoorSystemData* pDoorSystemData = CDoorSystem::GetDoorData(doorEnumHash);

	Vector3 doorPos = Vector3(position.x, position.y, position.z);

#if __DEV
	if (!pDoorSystemData)
	{
		CDoorSystemData *pExistingDoorData = CDoorSystem::FindDoorSystemData(doorPos, hashKey, 0.1f);
		if (pExistingDoorData)
		{
			atNonFinalHashString lookUpHash(hashKey);
			scriptAssertf(0, "%s: ADD_DOOR_TO_SYSTEM - a door already exists for model %s at this position %f, %f, %f. It was added by %s", 
							CTheScripts::GetCurrentScriptNameAndProgramCounter(),
							lookUpHash.GetCStr(), 
							pExistingDoorData->GetPosition().x, 
							pExistingDoorData->GetPosition().y, 
							pExistingDoorData->GetPosition().z, 
							pExistingDoorData->m_pScriptName);
		}
	}
#endif
	if (pDoorSystemData)
	{
		if (!doorPos.IsClose(pDoorSystemData->GetPosition(), 0.1f))
		{
			scriptAssertf(0, "%s: ADD_DOOR_TO_SYSTEM - a door already exists with this enum hash but has a different position %f, %f, %f. It was added by %s",
							CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
							pDoorSystemData->GetPosition().x, 
							pDoorSystemData->GetPosition().y, 
							pDoorSystemData->GetPosition().z,
							pDoorSystemData->m_pScriptName);
		}
		return;
	}

	if (NetworkInterface::IsGameInProgress() && network)
	{
		if (permanent)
		{
			scriptAssertf(0, "%s: ADD_DOOR_TO_SYSTEM - door is flagged as networked and permanent, permanent doors cannot be networked as they are not attached to the script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			network = false;
		}

		if (!CTheScripts::GetCurrentGtaScriptHandlerNetwork() || !CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent())
		{
			scriptAssertf(0, "%s: ADD_DOOR_TO_SYSTEM - a non-networked script cannot add doors during MP", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return;
		}

		if (CNetObjDoor::GetPool()->GetNoOfFreeSpaces() == 0)
		{
			scriptAssertf(0, "%s: ADD_DOOR_TO_SYSTEM - ran out of networked doors (max: %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CNetObjDoor::GetPool()->GetSize());
			return;
		}

		if (!CTheScripts::GetCurrentGtaScriptHandlerNetwork()->GetNetworkComponent()->IsHostLocal())
		{
			scriptAssertf(0, "%s: ADD_DOOR_TO_SYSTEM - a non-script host cannot add networked doors to the system", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return;
		}

		if (!CNetworkObjectPopulationMgr::CanRegisterObject(NET_OBJ_TYPE_DOOR, true))
		{
			scriptAssertf(0, "%s: ADD_DOOR_TO_SYSTEM - no free ids or ran out of networked doors (max allowed: %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), MAX_NUM_NETOBJDOORS);
			return;
		}
	}

	pDoorSystemData = CDoorSystem::AddDoor(doorEnumHash, hashKey, doorPos);
	if (pDoorSystemData)
	{
		pDoorSystemData->SetUseOldOverrides(useOldOverrides);

		if (permanent)
		{
			pDoorSystemData->SetPermanentState();
		}

		if (NetworkInterface::IsGameInProgress() && !permanent)
		{
			if (!network)
			{
				pDoorSystemData->SetPersistsWithoutNetworkObject();
			}

			// only register with the script handler in MP, not sure what this would break in SP! The SP scripts may not be expecting the doors to be 
			// cleaned up when the script ends
			if (!pDoorSystemData->GetScriptHandler())
			{
				CTheScripts::GetCurrentGtaScriptHandler()->RegisterNewScriptObject(static_cast<scriptHandlerObject&>(*pDoorSystemData), true, network);
			}

			NetworkInterface::RegisterScriptDoor(*pDoorSystemData, network);	
		}
		
		OutputScriptState(doorEnumHash, "Command - %s", "ADD_DOOR_TO_SYSTEM");
	}
}

void CommandRemoveDoorFromSystem(int doorEnumHash, bool bLock = false)
{
	CDoorSystemData* pDoorSystemData = CDoorSystem::GetDoorData(doorEnumHash);

	scriptAssertf(pDoorSystemData, "%s: REMOVE_DOOR_FROM_SYSTEM - the door has not been added to the door system", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if (pDoorSystemData)
	{
		if (pDoorSystemData->GetScriptHandler() && pDoorSystemData->GetScriptHandler()->GetScriptId() != CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId())
		{
			scriptAssertf(pDoorSystemData, "%s: REMOVE_DOOR_FROM_SYSTEM - the door is controlled by another script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return;
		}

		if (pDoorSystemData->GetNetworkObject())
		{
			if (pDoorSystemData->GetNetworkObject()->IsClone() || pDoorSystemData->GetNetworkObject()->IsPendingOwnerChange())
			{
				scriptAssertf(pDoorSystemData, "%s: REMOVE_DOOR_FROM_SYSTEM - the door is not locally controlled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				return;
			}

			NetworkInterface::UnregisterScriptDoor(*pDoorSystemData);	
		}
		else 
		{
			if (pDoorSystemData->GetScriptHandler())
			{
				pDoorSystemData->GetScriptHandler()->UnregisterScriptObject(static_cast<scriptHandlerObject&>(*pDoorSystemData));
			}
		}

		if (CDoor* pDoor = pDoorSystemData->GetDoor())
		{
			pDoor->SetDoorControlFlag(CDoor::DOOR_CONTROL_FLAGS_LOCK_WHEN_REMOVED, bLock);
		}

		pDoorSystemData->SetFlagForRemoval();
		OutputScriptState(doorEnumHash, "Command - %s", "REMOVE_DOOR_FROM_SYSTEM");

	}
}

void CommandDoorSystemSetState(int doorEnumHash, int doorState, bool network, bool flushState)
{
#if __BANK
	if (CDoor::sm_IngnoreScriptSetDoorStateCommands)
	{
		return;
	}
#endif
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

#if __BANK
	if (sm_debugDooorSystemEnumHash == doorEnumHash || (pDoor && pDoor->GetDoor() && pDoor->GetDoor() == g_PickerManager.GetSelectedEntity()))
	{
		if (sm_breakOnSelectedDoorSetState)
		{
			__debugbreak();
		}
	}

#endif
	OutputScriptState(doorEnumHash, "Command - %s, state %s, network %s, flushState %s", "DOOR_SYSTEM_SET_DOOR_STATE", CDoorSystemData::GetDoorStateName((DoorScriptState)doorState), network ? "true" : "false", flushState ? "true" : "false");

	SCRIPT_ASSERT(pDoor, "Door hash supplied failed find a door");

	if (pDoor)
	{
		if (NetworkInterface::IsGameInProgress())
		{
			netObject* pNetObj = pDoor->GetNetworkObject();
			if (pNetObj)
			{
				if (!scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s : DOOR_SYSTEM_SET_DOOR_STATE - the door is controlled by another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					return;
				}

				network = true;
			}
			else if (network && pDoor->GetPersistsWithoutNetworkObject())
			{
				// don't start networking if the state of the door is not changing
				if (pDoor->GetPendingState() != DOORSTATE_INVALID)
				{
					if (pDoor->GetPendingState() == doorState)
					{
						network = false;
					}
				}
				else if (pDoor->GetState() == doorState)
				{
					network = false;
				}
			}

			// start networking the door if it has changed from its initial state
			if (!pNetObj && network)
			{
				if (CNetObjDoor::GetPool()->GetNoOfFreeSpaces() == 0)
				{
					scriptAssertf(0, "%s: DOOR_SYSTEM_SET_DOOR_STATE - ran out of networked doors (max: %d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CNetObjDoor::GetPool()->GetSize());
					network = false;
				}

				if (pDoor->GetPermanentState())
				{
					scriptAssertf(0, "%s: DOOR_SYSTEM_SET_DOOR_STATE - Permanent doors cannot be networked", CTheScripts::GetCurrentScriptNameAndProgramCounter());
					network = false;
				}

				if (network)
				{
					// we have to trigger a network event here, to request that the door is networked
					CRequestDoorEvent::Trigger(*pDoor, doorState);
					return;
				}
			}
		}

		gnetDebug3("DOOR_SYSTEM_SET_DOOR_STATE - %d setting pending state to: %d from: %s", doorEnumHash, doorState, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		pDoor->SetPendingState((DoorScriptState)doorState);

		if ((int)pDoor->GetState() != doorState || pDoor->GetStateNeedsToBeFlushed() || flushState)
		{
			CDoorSystem::SetPendingStateOnDoor(*pDoor);
		}		
	}
}

int CommandDoorSystemGetState(int doorEnumHash)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "DOOR_SYSTEM_GET_DOOR_STATE: Door hash supplied failed find a door");

	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_DOOR_STATE");

	if (pDoor)
	{
		return (int)pDoor->GetState();
	}

	return -1;
}

int CommandDoorSystemGetPendingState(int doorEnumHash)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "DOOR_SYSTEM_GET_DOOR_PENDING_STATE: Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_DOOR_PENDING_STATE");

	if (pDoor)
	{
		return (int)pDoor->GetPendingState();
	}

	return -1;
}

void CommandDoorSystemSetAutomaticRate(int doorEnumHash, float automaticRate, bool network, bool flushState)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s, automaticRate %.2f, network %s, flushstate %s", "DOOR_SYSTEM_SET_AUTOMATIC_RATE", automaticRate, network ? "true" : "false", flushState ? "true" : "false");

	if (pDoor)
	{
		netObject* pNetObj = pDoor->GetNetworkObject();

		if (pNetObj)
		{
			if (!scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s : DOOR_SYSTEM_SET_AUTOMATIC_RATE - the door is controlled by another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return;
			}
		}

		pDoor->SetAutomaticRate(automaticRate);

		if (flushState)
		{
			CommandDoorSystemSetState(doorEnumHash, pDoor->GetState(), network, flushState);
		}
	}

}

void CommandDoorSystemSetAutomaticDistance(int doorEnumHash, float automaticDistance, bool network, bool flushState)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s, automaticDistance %.2f, network %s, flushstate %s", "DOOR_SYSTEM_SET_AUTOMATIC_DISTANCE", automaticDistance, network ? "true" : "false", flushState ? "true" : "false");

	if (pDoor)
	{
		netObject* pNetObj = pDoor->GetNetworkObject();

		if (pNetObj)
		{
			if (!scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s : DOOR_SYSTEM_SET_AUTOMATIC_DISTANCE - the door is controlled by another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return;
			}
		}

		pDoor->SetAutomaticDistance(automaticDistance);

		if (flushState)
		{
			CommandDoorSystemSetState(doorEnumHash, pDoor->GetState(), network, flushState);
		}
	}
}

void CommandDoorSystemSetOpenRatio(int doorEnumHash, float openRatio, bool network, bool flushState)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s, openRatio %.2f, network %s, flushstate %s", "DOOR_SYSTEM_SET_OPEN_RATIO", openRatio, network ? "true" : "false", flushState ? "true" : "false");

	if (pDoor)
	{
		netObject* pNetObj = pDoor->GetNetworkObject();

		if (pNetObj)
		{
			if (!scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s : DOOR_SYSTEM_SET_OPEN_RATIO - the door is controlled by another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return;
			}
		}

		pDoor->SetTargetRatio(openRatio);

		if (flushState)
		{
			CommandDoorSystemSetState(doorEnumHash, pDoor->GetState(), network, flushState);
		}
	}

}


float CommandDoorSystemGetAutomaticRate(int doorEnumHash)
{
	CDoorSystemData *pDoorData = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoorData, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_AUTOMATIC_RATE");

	if (pDoorData)
	{
		CDoor *pDoor = pDoorData->GetDoor();
		if (pDoor)
		{
			return pDoor->GetAutomaticRate();
		}
		else
		{
			return pDoorData->GetAutomaticRate();
		}
	}
	else
	{
		return 0.0f;
	}
}

float CommandDoorSystemGetAutomaticDistance(int doorEnumHash)
{
	CDoorSystemData *pDoorData = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoorData, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_SET_AUTOMATIC_DISTANCE");

	if (pDoorData)
	{
		CDoor *pDoor = pDoorData->GetDoor();
		if (pDoor)
		{
			return pDoor->GetAutomaticDist();
		}
		else
		{
			return pDoorData->GetAutomaticDistance();
		}
	}
	else
	{
		return 0.0f;
	}
}

float CommandDoorSystemGetOpenRatio(int doorEnumHash)
{
	CDoorSystemData *pDoorData = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoorData, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_OPEN_RATIO");

	if (pDoorData)
	{
		CDoor *pDoor = pDoorData->GetDoor();
		if (pDoor)
		{
			return pDoor->GetDoorOpenRatio();
		}
		else
		{
			return pDoorData->GetTargetRatio();
		}
	}
	else
	{
		return 0.0f;
	}
}


void CommandDoorSystemSetUnsprung(int doorEnumHash, bool unsprung, bool network, bool flushState)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s, unsprung %s, network %s, flushstate %s", "DOOR_SYSTEM_SET_UNSPRUNG", unsprung ? "true" : "false", network ? "true" : "false", flushState ? "true" : "false");

	if (pDoor)
	{
		netObject* pNetObj = pDoor->GetNetworkObject();

		if (pNetObj)
		{
			if (!scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s : DOOR_SYSTEM_SET_UNSPRUNG - the door is controlled by another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return;
			}
		}

		pDoor->SetUnsprung(unsprung);

		if (flushState)
		{
			CommandDoorSystemSetState(doorEnumHash, pDoor->GetState(), network, flushState);
		}
	}

}

bool CommandDoorSystemGetUnsprung(int doorEnumHash)
{
	CDoorSystemData *pData = CDoorSystem::GetDoorData(doorEnumHash);
	SCRIPT_ASSERT(pData, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_UNSPRUNG");
	if (pData)
	{
		return pData->GetUnsprung();

	}
	return false;
}

void CommandDoorSystemSetHoldOpen(int doorEnumHash, bool holdOpen)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s, holdOpen %s", "DOOR_SYSTEM_SET_HOLD_OPEN", holdOpen ? "true" : "false");

	if (pDoor)
	{
		netObject* pNetObj = pDoor->GetNetworkObject();

		if (pNetObj)
		{
			if (!scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s : DOOR_SYSTEM_SET_HOLD_OPEN - the door is controlled by another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return;
			}
		}

		pDoor->SetHoldOpen(holdOpen);
	}

}

bool CommandDoorSystemGetHoldOpen(int doorEnumHash)
{
	CDoorSystemData *pData = CDoorSystem::GetDoorData(doorEnumHash);
	SCRIPT_ASSERT(pData, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_HOLD_OPEN");
	if (pData)
	{
		return pData->GetHoldOpen();
	}
	return false;
}


bool CommandIsDoorRegisteredWithSystem(int doorEnumHash)
{
	OutputScriptState(doorEnumHash, "Command - %s", "IS_DOOR_REGISTERED_WITH_SYSTEM");
	bool bRegistered = CDoorSystem::GetDoorData(doorEnumHash) != NULL;

	return bRegistered;
}

bool CommandIsDoorClosed(int doorEnumHash)
{
	CDoorSystemData *pDoorData = CDoorSystem::GetDoorData(doorEnumHash);
	OutputScriptState(doorEnumHash, "Command - %s", "IS_DOOR_CLOSED");

	bool bClosed = true;

	if (scriptVerifyf(pDoorData, "%s: IS_DOOR_CLOSED - door does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CDoor* pDoor = pDoorData->GetDoor();

		if (pDoor)
		{
			bClosed = pDoor->IsDoorShut();
		}
		else if (pDoorData->GetNetworkObject() && pDoorData->GetNetworkObject()->IsClone())
		{
			bClosed = static_cast<CNetObjDoor*>(pDoorData->GetNetworkObject())->IsClosed();
		}
		else if (pDoorData->GetPendingState() != DOORSTATE_INVALID)
		{
			bClosed = pDoorData->GetPendingState() != DOORSTATE_FORCE_OPEN_THIS_FRAME;
		}
		else 
		{
			bClosed = pDoorData->GetState() != DOORSTATE_FORCE_OPEN_THIS_FRAME;
		}
	}

	return bClosed;
}

bool CommandIsDoorFullyOpen(int doorEnumHash)
{
	CDoorSystemData *pDoorData = CDoorSystem::GetDoorData(doorEnumHash);
	OutputScriptState(doorEnumHash, "Command - %s", "IS_DOOR_FULLY_OPEN");
	bool bFullyOpen = true;

	if (scriptVerifyf(pDoorData, "%s: IS_DOOR_FULLY_OPEN - door does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		if (pDoorData->GetDoor())
		{
			bFullyOpen = (pDoorData->GetDoor()->GetDoorOpenRatio() > 0.99f);
		}
		else if (pDoorData->GetNetworkObject() && pDoorData->GetNetworkObject()->IsClone())
		{
			bFullyOpen = static_cast<CNetObjDoor*>(pDoorData->GetNetworkObject())->IsFullyOpen();
		}
		else if (pDoorData->GetPendingState() != DOORSTATE_INVALID)
		{
			bFullyOpen = pDoorData->GetPendingState() == DOORSTATE_FORCE_OPEN_THIS_FRAME;
		}
		else 
		{
			bFullyOpen = pDoorData->GetState() == DOORSTATE_FORCE_OPEN_THIS_FRAME;
		}
	}

	return bFullyOpen;
}

void CommandOpenAllBarriersForRace(bool snapOpen)
{
#if __BANK
	if (sm_debugDooorSystemEnumHash != -1)
	{
		Displayf("Command - OPEN_ALL_BARRIERS_FOR_RACE %s", snapOpen ? "true" : "false");
	}
#endif
	CDoor::OpenAllBarriersForRace(snapOpen);
}

void CommandCloseAllBarriersOpenedForRace()
{
#if __BANK
	if (sm_debugDooorSystemEnumHash != -1)
	{
		Displayf("Command - CLOSE_ALL_BARRIERS_FOR_RACE");
	}
#endif
	CDoor::CloseAllBarriersOpenedForRace();
}

void CommandDoorSystemSetOpenForRaces(int doorEnumHash, bool open)
{
	CDoorSystemData *pDoor = CDoorSystem::GetDoorData(doorEnumHash);

	SCRIPT_ASSERT(pDoor, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s, open %s", "DOOR_SYSTEM_SET_DOOR_OPEN_FOR_RACES", open ? "true" : "false");

	if (pDoor)
	{
		netObject* pNetObj = pDoor->GetNetworkObject();

		if (pNetObj)
		{
			if (!scriptVerifyf(!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange(), "%s : DOOR_SYSTEM_SET_DOOR_OPEN_FOR_RACES - the door is controlled by another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				return;
			}
		}

		pDoor->SetOpenForRaces(open);

		if (pDoor->GetDoor() && CDoor::AreDoorsOpenForRacing())
		{
			if (open)
			{
				pDoor->GetDoor()->OpenDoor();
			}
			else
			{
				pDoor->GetDoor()->CloseDoor();
			}
		}
	}

}

bool CommandDoorSystemGetOpenForRaces(int doorEnumHash)
{
	CDoorSystemData *pData = CDoorSystem::GetDoorData(doorEnumHash);
	SCRIPT_ASSERT(pData, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_DOOR_OPEN_FOR_RACES");
	if (pData)
	{
		return pData->GetOpenForRaces();
	}

	return false;
}

void CommandDoorSetAlwaysPush( int doorEnumHash, bool push )
{
	CDoorSystemData *pData = CDoorSystem::GetDoorData( doorEnumHash );
	SCRIPT_ASSERT( pData, "Door hash supplied failed find a door" );
	OutputScriptState( doorEnumHash, "Command - %s", "DOOR_SET_ALWAYS_PUSH" );

	if( pData && pData->GetDoor() )
	{   
		pData->GetDoor()->SetAlwaysPush( push );
	}
}

void CommandDoorSetAlwaysPushVehicles(int ObjectID , bool pushVehicles)
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectID);
	if (pObject && pObject->IsADoor())
	{   
		static_cast<CDoor *>(pObject)->SetAlwaysPushVehicles(pushVehicles);
	}
}

bool CommandDoorSystemGetIsPhysicsLoaded(int doorEnumHash)
{
	CDoorSystemData *pData = CDoorSystem::GetDoorData(doorEnumHash);
	SCRIPT_ASSERT(pData, "Door hash supplied failed find a door");
	OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_GET_IS_PHYSICS_LOADED");
	if (pData)
	{
		return pData->GetDoor() != NULL;
	}

	return false;
}

bool CommandDoorSystemFindExistingDoor(const scrVector & scrVecPos, int ObjectModelHashKey, int &doorEnumHash)
{
	CDoorSystemData *pData = CDoorSystem::FindDoorSystemData(scrVecPos, ObjectModelHashKey, 0.5f);
	if (pData)
	{
		doorEnumHash = pData->GetEnumHash();
		OutputScriptState(doorEnumHash, "Command - %s", "DOOR_SYSTEM_FIND_EXISTING_DOOR");
		return true;
	}

	return false;
}


bool CommandDoesObjectOfTypeExistAtCoords(const scrVector & scrVecPos, float Radius, int ObjectModelHashKey, bool checkPhysicsExists)
{
	Vector3 VecCoors = Vector3 (scrVecPos);
	
	bool bObjectExists = false;
	CEntity *pClosestObj = CObject::GetPointerToClosestMapObjectForScript(VecCoors.x, VecCoors.y, VecCoors.z, Radius, ObjectModelHashKey);
	if (pClosestObj)
	{
		if (pClosestObj->GetIsTypeObject())
		{	//	Only return true if the object is a proper object - not a dummy or building
			if (checkPhysicsExists)
			{				
				bObjectExists = pClosestObj->GetCurrentPhysicsInst() != 0;
			}
			else
			{
				bObjectExists = true;
			}
		}
	}
	return bObjectExists;
}

bool CommandIsPointInAngledArea(const scrVector & scrVecPoint, const scrVector & scrVecCoors1, const scrVector & scrVecCoors2, float DistanceP1toP4, bool HighlightArea, bool bCheck3d)
{
	Vector3 VecPoint = Vector3(scrVecPoint);
	Vector3 VecCoors1 = Vector3(scrVecCoors1);
	Vector3 VecCoors2 = Vector3(scrVecCoors2);

	if(!bCheck3d)
	{
		VecCoors1.z = INVALID_MINIMUM_WORLD_Z;
		VecCoors2.z = VecCoors1.z;
	}

	return PointInAngledAreaCheck(bCheck3d, VecPoint, VecCoors1, VecCoors2, DistanceP1toP4, HighlightArea);
}


void CommandSetObjectAsStealable(int ObjectID, bool IsStealable )
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectID);
	if (pObject)
	{   
		pObject->m_nObjectFlags.bIsStealable = IsStealable;
		pObject->m_nObjectFlags.bCanBePickedUp = IsStealable;
	}
}

bool CommandHasObjectBeenUprooted( int ObjectID )
{
	bool bResult = false;

	const CObject *pObject = CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectID);
	if (pObject)
	{   
		if(pObject)
		{
			bResult = bool(pObject->m_nObjectFlags.bHasBeenUprooted);
		}
	}
	return (bResult);
}

bool CommandDoesPickupExist( int PickupID )
{
	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		scriptHandlerObject* pObject = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID);

		if (pObject && static_cast<CGameScriptHandlerObject*>(pObject)->GetType() == SCRIPT_HANDLER_OBJECT_TYPE_PICKUP)
		{
			return true;
		}
	}

	return false;
}

bool CommandDoesPickupObjectExist( int PickupID )
{
	bool bObjExists = false;

	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		scriptHandlerObject* pObject = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID);

		if (pObject)
		{
			if (scriptVerifyf(static_cast<CGameScriptHandlerObject*>(pObject)->GetType() == SCRIPT_HANDLER_OBJECT_TYPE_PICKUP, "DOES_PICKUP_OBJECT_EXIST - This is not a pickup id"))
			{
				CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID));

				if (SCRIPT_VERIFY(pPlacement,"DOES_PICKUP_OBJECT_EXIST - Pickup does not exist" ))		
				{
					bObjExists = !pPlacement->GetIsCollected() && !pPlacement->GetHasPickupBeenDestroyed();

					if (pPlacement->GetHiddenWhenUncollectable() && !pPlacement->CanCollectScript())
					{
						bObjExists = false;
					}
				}
			}
		}
	}

	return bObjExists;
}

int CommandGetPickupObject( int PickupID )
{
	int objectIndex = -1;

	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		scriptHandlerObject* pObject = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID);

		if (pObject)
		{
			if (scriptVerifyf(static_cast<CGameScriptHandlerObject*>(pObject)->GetType() == SCRIPT_HANDLER_OBJECT_TYPE_PICKUP, "GET_PICKUP_OBJECT - This is not a pickup id"))
			{
				CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID));

				if (SCRIPT_VERIFY(pPlacement,"GET_PICKUP_OBJECT - Pickup does not exist" ))		
				{
					if (pPlacement->GetPickup())
					{
						objectIndex = CTheScripts::GetGUIDFromEntity(*pPlacement->GetPickup());
					}
				}
			}
		}
	}

	return objectIndex;
}

bool CommandIsObjectAPickup(int ObjectIndex)
{
	bool bIsPickup = false;

	const CObject *pObj = CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectIndex);

	bIsPickup = pObj && pObj->m_nObjectFlags.bIsPickUp;

	return bIsPickup;
}

bool CommandIsObjectAPortablePickup(int ObjectIndex)
{
	bool bIsPortablePickup = false;

	const CObject *pObj = CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectIndex);

	if (pObj && pObj->m_nObjectFlags.bIsPickUp)
	{
		const CPickup* pPickup = SafeCast(const CPickup, pObj);

		bIsPortablePickup = pPickup->IsFlagSet(CPickup::PF_Portable);
	}

	return bIsPortablePickup;
}

bool CommandDoesPickupOfTypeExistInArea( int PickupHash, const scrVector & scrVecCoors, float radius)
{
	PickupHash = ConvertOldPickupTypeToHash(PickupHash);

	bool bPickupExists = false;

	Vector3 VecCoords = Vector3 (scrVecCoors);

	radius *= radius;

	s32 i = CPickup::GetPool()->GetSize();

	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && pPickup->GetPickupHash() == (u32)PickupHash)
		{
			Vector3 diff = VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()) - VecCoords;

			if (diff.Mag2() <= radius)
			{
				bPickupExists = true;
				break;
			}
		}
	}

	return bPickupExists;
}


void CommandSetNearestBuildingAnimRate(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float Rate, int RateFlags)
{
	Vector3 VecCentre = Vector3(scrVecCentreCoors);

	CEntity *pEntity = CObject::GetPointerToClosestMapObjectForScript(VecCentre.x, VecCentre.y, VecCentre.z, Radius, ModelHashKey, ENTITY_TYPE_MASK_ANIMATED_BUILDING|ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT);
	if (pEntity)
	{
		if (SCRIPT_VERIFY(pEntity->GetIsTypeAnimatedBuilding(), "SET_NEAREST_BUILDING_ANIM_RATE - Closest entity is not a building!"))
		{
			CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);
			CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();
			CEntity *pLod = static_cast< CEntity * >(pAnimatedBuilding->GetRootLod());
			if(SCRIPT_VERIFY(pLod, "SET_NEAREST_BUILDING_ANIM_RATE - Could not get root lod from animated building!"))
			{
				/* Clip */
				CBaseModelInfo *pBaseModelInfo = pAnimatedBuilding->GetBaseModelInfo();
				if(SCRIPT_VERIFY(pBaseModelInfo, "SET_NEAREST_BUILDING_ANIM_RATE - Could not get base model info!"))
				{
					strLocalIndex clipDictionaryIndex = strLocalIndex(pBaseModelInfo->GetClipDictionaryIndex());
					if(SCRIPT_VERIFY(g_ClipDictionaryStore.IsValidSlot(clipDictionaryIndex), "SET_NEAREST_BUILDING_ANIM_RATE - Could not get clip dictionary index from base model info!"))
					{
						const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex.Get(), pBaseModelInfo->GetHashKey());
						if(SCRIPT_VERIFY(pClip, "SET_NEAREST_BUILDING_ANIM_RATE - Could not get clip!"))
						{
							/* Rate */
							float rate = Rate;
							if((RateFlags & AB_RATE_RANDOM) != 0)
							{
								const CClipEventTags::CRandomizeRateProperty *pRateProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeRateProperty >(pClip, CClipEventTags::RandomizeRate);
								if(SCRIPT_VERIFY(pRateProp, "SET_NEAREST_BUILDING_ANIM_RATE - Could not get rate property from clip!"))
								{
									rate = pRateProp->PickRandomRate((u32)(size_t)pLod);
								}
							}
							moveAnimatedBuilding.SetRate(rate);
						}
					}
				}
			}
		}
	}
	else
	{
		Displayf("Failed to find building\n");
	}
}

void CommandGetNearestBuildingAnimRate(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float &ReturnRate)
{
	Vector3 VecCentre = Vector3(scrVecCentreCoors);

	CEntity *pEntity = CObject::GetPointerToClosestMapObjectForScript(VecCentre.x, VecCentre.y, VecCentre.z, Radius, ModelHashKey, ENTITY_TYPE_MASK_ANIMATED_BUILDING|ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT);
	if (pEntity)
	{
		if (SCRIPT_VERIFY(pEntity->GetIsTypeAnimatedBuilding(), "PLAY_NEAREST_BUILDING_ANIM - Closest entity is not a building"))
		{
			CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);
			CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();

			/* Rate */
			ReturnRate = moveAnimatedBuilding.GetRate();
		}
	}
	else
	{
		Displayf("Failed to find building\n");
	}
}

void CommandSetNearestBuildingAnimPhase(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float Phase, int PhaseFlags)
{
	Vector3 VecCentre = Vector3(scrVecCentreCoors);

	CEntity *pEntity = CObject::GetPointerToClosestMapObjectForScript(VecCentre.x, VecCentre.y, VecCentre.z, Radius, ModelHashKey, ENTITY_TYPE_MASK_ANIMATED_BUILDING|ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT);
	if (pEntity)
	{
		if (SCRIPT_VERIFY(pEntity->GetIsTypeAnimatedBuilding(), "SET_NEAREST_BUILDING_ANIM_PHASE - Closest entity is not a building!"))
		{
			CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);
			CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();
			CEntity *pLod = static_cast< CEntity * >(pAnimatedBuilding->GetRootLod());
			if(SCRIPT_VERIFY(pLod, "SET_NEAREST_BUILDING_ANIM_PHASE - Could not get root lod from animated building!"))
			{
				/* Clip */
				CBaseModelInfo *pBaseModelInfo = pAnimatedBuilding->GetBaseModelInfo();
				if(SCRIPT_VERIFY(pBaseModelInfo, "SET_NEAREST_BUILDING_ANIM_PHASE - Could not get base model info!"))
				{
					strLocalIndex clipDictionaryIndex = strLocalIndex(pBaseModelInfo->GetClipDictionaryIndex());
					if(SCRIPT_VERIFY(g_ClipDictionaryStore.IsValidSlot(clipDictionaryIndex), "SET_NEAREST_BUILDING_ANIM_PHASE - Could not get clip dictionary index from base model info!"))
					{
						const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex.Get(), pBaseModelInfo->GetHashKey());
						if(SCRIPT_VERIFY(pClip, "SET_NEAREST_BUILDING_ANIM_PHASE - Could not get clip!"))
						{
							/* Phase */
							float phase = Phase;
							if((PhaseFlags & AB_PHASE_RANDOM) != 0)
							{
								const CClipEventTags::CRandomizeStartPhaseProperty *pPhaseProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeStartPhaseProperty >(pClip, CClipEventTags::RandomizeStartPhase);
								if(SCRIPT_VERIFY(pPhaseProp, "SET_NEAREST_BUILDING_ANIM_PHASE - Could not get phase property from clip!"))
								{
									phase = pPhaseProp->PickRandomStartPhase((u32)(size_t)pLod);
								}
							}
							phase = phase - Floorf(phase);
							moveAnimatedBuilding.SetPhase(phase);
						}
					}
				}
			}
		}
	}
	else
	{
		Displayf("Failed to find building\n");
	}
}

void CommandGetNearestBuildingAnimPhase(const scrVector & scrVecCentreCoors, float Radius, s32 ModelHashKey, float &ReturnPhase)
{
	Vector3 VecCentre = Vector3(scrVecCentreCoors);

	CEntity *pEntity = CObject::GetPointerToClosestMapObjectForScript(VecCentre.x, VecCentre.y, VecCentre.z, Radius, ModelHashKey, ENTITY_TYPE_MASK_ANIMATED_BUILDING|ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT);
	if (pEntity)
	{
		if (SCRIPT_VERIFY(pEntity->GetIsTypeAnimatedBuilding(), "PLAY_NEAREST_BUILDING_ANIM - Closest entity is not a building"))
		{
			CAnimatedBuilding *pAnimatedBuilding = static_cast< CAnimatedBuilding * >(pEntity);
			CMoveAnimatedBuilding &moveAnimatedBuilding = pAnimatedBuilding->GetMoveAnimatedBuilding();

			/* Phase */
			ReturnPhase = moveAnimatedBuilding.GetPhase();
		}
	}
	else
	{
		Displayf("Failed to find building\n");
	}
}

void CommandSetObjectAllowLowLodBuoyancy(int ObjectIndex, bool bAllow)
{
	CObject* pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObj)
	{
		pObj->m_Buoyancy.m_buoyancyFlags.bScriptLowLodOverride = bAllow ? 1 : 0;
	}
}

void CommandSetObjectPhysicsParams(int ObjectIndex, float fMass, float fGravityFactor, const scrVector & vecTranslationDamping, const scrVector & vecRotationDamping, float fCollisionMargin, float fMaxAngularSpeed, float fBuoyancyFactor)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{	
		if(SCRIPT_VERIFY(pObj->GetCurrentPhysicsInst(), "SET_OBJECT_PHYSICS_PARAMS - Object doesn't have physics streamed in yet"))
		{
	
			CBaseModelInfo *pModelInfo = pObj->GetBaseModelInfo();
			
			// check if object still has original physics archetype
			if(pObj->GetCurrentPhysicsInst()->GetArchetype() == pModelInfo->GetPhysics())
			{
				// if so then we need to clone it so that we can modify values
				phArchetypeDamp* pNewArchetype = static_cast<phArchetypeDamp*>(pObj->GetCurrentPhysicsInst()->GetArchetype()->Clone());
				// pass new archetype to the objects physinst -> should have 1 ref only
				pObj->GetCurrentPhysicsInst()->SetArchetype(pNewArchetype);
			}

			phArchetypeDamp* pGtaArchetype = static_cast<phArchetypeDamp*>(pObj->GetCurrentPhysicsInst()->GetArchetype());

			// option to set mass
			if(fMass > -1.0f)
			{
				if (SCRIPT_VERIFY(fMass>0.0f, "SET_OBJECT_PHYSICS_PARAMS - Mass must be greater than zero (or -1.0f)"))
				{
					pObj->SetMass(fMass);
				}
			}
			else
			{
				Assert(pModelInfo->GetPhysics());
				// Reset mass to normal
				if(pModelInfo->GetPhysics())
				{
					pGtaArchetype->SetMass(pModelInfo->GetPhysics()->GetMass());
				}
				else
				{
					pGtaArchetype->SetMass(1.0f);
				}
			}

			// option to set gravity factor
			if(fGravityFactor > -1.0f)
			{
				pGtaArchetype->SetGravityFactor(fGravityFactor);
			}

			// Option to set buoyancy factor.
			if(fBuoyancyFactor > -1.0f)
			{
				pGtaArchetype->SetBuoyancyFactor(fBuoyancyFactor);
			}

			if(vecTranslationDamping.x > -1.0f)
				pGtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_C, Vector3(vecTranslationDamping.x, vecTranslationDamping.x, vecTranslationDamping.x));
			if(vecTranslationDamping.y > -1.0f)
				pGtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(vecTranslationDamping.y, vecTranslationDamping.y, vecTranslationDamping.y));
			if(vecTranslationDamping.z > -1.0f)
				pGtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(vecTranslationDamping.z, vecTranslationDamping.z, vecTranslationDamping.z));

			if(vecRotationDamping.x > -1.0f)
				pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(vecRotationDamping.x, vecRotationDamping.x, vecRotationDamping.x));
			if(vecRotationDamping.y > -1.0f)
				pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(vecRotationDamping.y, vecRotationDamping.y, vecRotationDamping.y));
			if(vecRotationDamping.z > -1.0f)
				pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(vecRotationDamping.z, vecRotationDamping.z, vecRotationDamping.z));

			if(fCollisionMargin > 0.0f && fCollisionMargin < 0.1f)
			{
				// when we cloned the archetype, the bound will have been cloned as well
				// so we can do what we like to it, so long as it's not a composite - only want to set margin on geometry bounds for now anyway
				if(pGtaArchetype->GetBound()->GetType()==phBound::GEOMETRY)
				{
					phBoundGeometry* pBoundGeom = static_cast<phBoundGeometry*>(pGtaArchetype->GetBound());
					pBoundGeom->SetMarginAndShrink(fCollisionMargin);
				}
				else
					scriptAssertf(false, "%s:SET_OBJECT_PHYSICS_PARAMS - Collision margin can only be set on geometry bounds", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}

			if(fMaxAngularSpeed > 1.0f)
			{
				pGtaArchetype->SetMaxAngSpeed(fMaxAngularSpeed);
			}

            if(pObj->GetNetworkObject() && pObj->GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_OBJECT)
            {
                CNetObjObject *netObjObject = SafeCast(CNetObjObject, pObj->GetNetworkObject());
                netObjObject->SetUsingScriptedPhysicsParams(true);

                scriptAssertf(fMass                <= -1.0f &&
                              fGravityFactor       <= -1.0f &&
                              vecRotationDamping.x <= -1.0f &&
                              vecRotationDamping.y <= -1.0f &&
                              vecRotationDamping.z <= -1.0f &&
                              fCollisionMargin     <= -1.0f && 
                              fMaxAngularSpeed     <= -1.0f && 
                              fBuoyancyFactor      <= -1.0f, "%s:SET_OBJECT_PHYSICS_PARAMS - Currently only setting translational damping is synced in network games!", CTheScripts::GetCurrentScriptNameAndProgramCounter());
            }
		}
	}
}

float GetObjFragmentDamagedHealth(const CObject *pObj, bool bHealthPercentageByMass)
{
	float fReturnHealth = 1.0f;
	if(pObj && pObj->m_nFlags.bIsFrag && pObj->GetCurrentPhysicsInst())
	{
		fragInst* pFragInst = (fragInst*)pObj->GetCurrentPhysicsInst();
		if(pFragInst->GetCached())
		{
			if(bHealthPercentageByMass)
			{
				fReturnHealth = pFragInst->GetArchetype()->GetMass() / pFragInst->GetTypePhysics()->GetTotalUndamagedMass();
			}
			else
			{
				int nNumGroups = pFragInst->GetTypePhysics()->GetNumChildGroups();
				int nNumGroupsAlive = 0;
				for(int i=0; i<nNumGroups; i++)
				{
					if(pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(i))
						nNumGroupsAlive++;
				}

				if(nNumGroups > 0)
					fReturnHealth = float(nNumGroupsAlive) / float(nNumGroups);
			}
		}
	}

	return fReturnHealth;
}

float CommandGetObjectFragmentDamageHealth(int ObjectIndex, bool bHealthPercentageByMass)
{
	float fFragmentHealth = 1.0f;
	const CObject *pObj = CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectIndex);

	if(pObj)
	{
		fFragmentHealth = GetObjFragmentDamagedHealth(pObj, bHealthPercentageByMass);
	}

	return fFragmentHealth;
}

bool HasObjFragmentRootBeenDamaged(CObject *pObj)
{
	if (pObj)
	{
		fragInst* pFragInst = pObj->GetFragInst();
		if(pFragInst && pFragInst->GetCached())
		{
			if(pFragInst->GetTypePhysics()->GetAllChildren()[0]->GetDamagedEntity()
			&& pFragInst->GetCacheEntry()->GetHierInst()->groupInsts[0].IsDamaged())
			{
				return true;
			}
		}
	}

	return false;
}


bool HasObjBeenDamagedByPed(const CObject *pObj, int PedIndex)
{
	bool bReturnValue = false;

	const CPed *pPed = NULL;
	if (PedIndex != NULL_IN_SCRIPTING_LANGUAGE)
	{
		pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if(!pPed)
		{
			return false;
		}
	}
    else
    {
	    if (pObj && pObj->HasBeenDamagedByAnyPed())
	    {
		    return true;
	    }
    }

    if(pObj && pObj->GetNetworkObject())
    {
        bReturnValue = NetworkInterface::HasBeenDamagedBy(pObj->GetNetworkObject(), pPed);
    }
    else
    {
	    if (pObj && pPed)
	    {
			if (pObj->HasBeenDamagedByEntity(const_cast<CPed*>(pPed)))
			{
				bReturnValue = true;
			}
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				scriptAssertf(pPed->GetMyVehicle(), "%s:HasObjBeenDamagedByPed - Character is not in a vehicle", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				if (pObj->HasBeenDamagedByEntity(pPed->GetMyVehicle()))
				{
					bReturnValue = true;
				}
			}
	    }
    }

	return bReturnValue;
}


void CommandSetArrowAboveBlippedPickups(bool bOn)
{
	eBLIP_DISPLAY_TYPE blipDisplayType;

	if (bOn)
		blipDisplayType = BLIP_DISPLAY_BOTH;
	else
		blipDisplayType = BLIP_DISPLAY_BLIPONLY;

	CMiniMapBlip *pSimpleBlip = CMiniMap::GetBlipFromActualId(CMiniMap::GetActualSimpleBlipId());

	if (pSimpleBlip)
	{
		CMiniMap::SetBlipDisplayValue(pSimpleBlip, blipDisplayType);
	}
}

bool CommandIsAnyObjectNearPoint(const scrVector & scrVecPoint, float radius, bool bConsiderScriptCreatedObjectsOnly)
{
	Vector3 vPoint =  Vector3 (scrVecPoint);
	float radiusSqr = radius*radius;
	ScalarV vRadiusSqr = ScalarVFromF32(radiusSqr);

	for(s32 i=0; i<CObject::GetPool()->GetSize(); i++)
	{
		CObject *pObject = CObject::GetPool()->GetSlot(i);

		if (pObject && (!bConsiderScriptCreatedObjectsOnly || pObject->IsAScriptEntity()))
		{
//			Vector3 v = pObject->GetPosition() - vPoint;
			Vec3V v = pObject->GetTransform().GetPosition() - RCC_VEC3V(vPoint);

//			if (v.Mag2() <= radiusSqr)
			if (IsLessThanOrEqualAll(MagSquared(v), vRadiusSqr))
			{
				return true;
			}
		}
	}

	return false;
}

bool CommandIsObjectNearPoint(int modelHash, const scrVector & scrVecPoint, float radius)
{
	if(SCRIPT_VERIFY( CModelInfo::GetBaseModelInfoFromHashKey(modelHash, NULL), "IS_OBJECT_NEAR_POINT - this is not a valid model hash"))
	{
		Vector3 vPoint =  Vector3 (scrVecPoint);
		float radiusSqr = radius*radius;
		ScalarV vRadiusSqr = ScalarVFromF32(radiusSqr);
		for(s32 i=0; i<CObject::GetPool()->GetSize(); i++)
		{
			CObject *pObject = CObject::GetPool()->GetSlot(i);
			if (pObject)
			{
				CBaseModelInfo *pModelInfo = pObject->GetBaseModelInfo();
				if( pModelInfo && pModelInfo->GetModelNameHash() == (u32)modelHash )
				{
					Vec3V v = pObject->GetTransform().GetPosition() - RCC_VEC3V(vPoint);
					if (IsLessThanOrEqualAll(MagSquared(v), vRadiusSqr))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void CommandRequestObjectHighDetailModel(int iObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectIndex); 

	if (pObject)
	{
		Displayf("Requesting high detail object (this doesn't do anything - see a coder)"); 
	}
}

void CommandRemoveObjectHighDetailModel(int iObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(iObjectIndex); 

	if (pObject)
	{
		Displayf("Removing high detail object (this doesn't do anything - see a coder)"); 
	}
}

/*
enum eCompEntityState {
CE_STATE_INVALID		=	-1,
CE_STATE_INIT			=	0,
CE_STATE_SYNC_STARTING  =   1,
CE_STATE_STARTING		=	2,
CE_STATE_START			=	3,
CE_STATE_PRIMING		=	4,
CE_STATE_PRIMED			=	5,
CE_STATE_START_ANIM		=	6,
CE_STATE_ANIMATING		=	7,
CE_STATE_SYNC_ENDING	=	8,
CE_STATE_ENDING			=	9,
CE_STATE_END			=	10,
CE_STATE_RESET			=	11,
CE_STATE_PAUSED			=	12,
CE_STATE_RESUME			=	13,
};
*/

void CommandSetCompositeEntityState(int iCompEntityIndex, int targetState)
{
	CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(iCompEntityIndex); 

	if (SCRIPT_VERIFY (pCompEntity, "SetCompositeEntityState - Object doesn't exist"))
	{
		pCompEntity->SetModifiedBy(CTheScripts::GetCurrentScriptName());
		switch(targetState)
		{
			case CE_STATE_STARTING:
				pCompEntity->SetToStarting();
				break;
			case CE_STATE_PRIMING:
				pCompEntity->SetToPriming();
				break;
			case CE_STATE_START_ANIM:
				if (SCRIPT_VERIFY ((pCompEntity->GetState() == CE_STATE_PRIMED), "SetCompositeEntityState - can't start anim if not primed")){
					pCompEntity->TriggerAnim();
				}
				break;
			case CE_STATE_ENDING:
				pCompEntity->SetToEnding();
				break;
			case CE_STATE_RESET:
				pCompEntity->Reset();
				break;
			case CE_STATE_PAUSED:
				pCompEntity->PauseAnimAt(0.0f);		// pause immediately
				break;
			case CE_STATE_RESUME:
				pCompEntity->ResumeAnim();
				break;
			default:
				scriptAssertf(false, "[script] State (%d) passed to object (%s) is not handled.", targetState, pCompEntity->GetModelName());
				break;
		}
	}
}
void CommandPauseCompositeEntityAtPhase(int iCompEntityIndex, float targetPhase)
{
	CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(iCompEntityIndex); 

	if (SCRIPT_VERIFY (pCompEntity, "PauseCompositeEntityAtPhase - Object doesn't exist"))
	{
		if (SCRIPT_VERIFY(targetPhase > 0.0f && targetPhase < 1.0f, "PauseCompositeEntityAtPhase - invalid phase")){
			pCompEntity->PauseAnimAt(targetPhase);
		}
	}
}
float CommandGetCompEntityAnimationPhase(int iCompEntityIndex)
{
	CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(iCompEntityIndex); 

	if (SCRIPT_VERIFY (pCompEntity, "GET_RAYFIRE_MAP_OBJECT_PHASE - Object doesn't exist"))
	{
		if(pCompEntity->GetState() == CE_STATE_ANIMATING)
		{
			CObject* pAnimatedObj = pCompEntity->GetPrimaryAnimatedObject();
			if(pAnimatedObj)
			{
				if(pAnimatedObj->GetAnimDirector())
				{
					CMoveObject & move = pAnimatedObj->GetMoveObject();
					return move.GetClipHelper().GetPhase();
				}
			}
		}
	}
	return 0.0f;
}

s32 CommandGetCompositeEntityState(int iCompEntityIndex)
{
	CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(iCompEntityIndex); 

	if (SCRIPT_VERIFY (pCompEntity, "GetCompositeEntityState - Object doesn't exist"))
	{
		return(pCompEntity->GetState());
	}

	return(-1);
}

s32 CommandGetCompositeEntityAt(const scrVector & scrVecCoors, float Radius, const char* pRayFireName )
{
	u32 ModelHashKey = atStringHash(pRayFireName); 
	u32 poolSize = CCompEntity::GetPool()->GetSize();

	Vector3 vCoors = Vector3(scrVecCoors);
	ScalarV vRadiusSquared = ScalarVFromF32(Radius*Radius);

	for(u32 i=0; i< poolSize; i++){
		CCompEntity* pCompEntity = CCompEntity::GetPool()->GetSlot(i); 

		if (pCompEntity){
			CBaseModelInfo* pModelInfo = pCompEntity->GetBaseModelInfo();
			if (pModelInfo && (pModelInfo->GetHashKey() == (u32)ModelHashKey)){
//				Vector3 delta = (pCompEntity->GetPosition()) - (Vector3)scrVecCoors;
//				if (delta.Mag2() < (Radius*Radius)){
				if (IsLessThanAll(DistSquared(pCompEntity->GetTransform().GetPosition(), RCC_VEC3V(vCoors)), vRadiusSquared))
				{
					s32 returnCompEntityIndex = CCompEntity::GetPool()->GetIndex(pCompEntity);
					scriptAssertf(returnCompEntityIndex >= 0, "invalid index returned for %s",pCompEntity->GetModelName());
					return(returnCompEntityIndex);
				}
			}
		}
	}

	return(NULL_IN_SCRIPTING_LANGUAGE);
}

//need a hash version to be called by the cutscene code
s32 CommandGetCompositeEntityAtInternal(const scrVector & scrVecCoors, float Radius, const atHashString& RayFireHash )
{
	u32 ModelHashKey = RayFireHash.GetHash(); 
	u32 poolSize = CCompEntity::GetPool()->GetSize();

	Vector3 vCoors = Vector3(scrVecCoors);
	ScalarV vRadiusSquared = ScalarVFromF32(Radius*Radius);

	for(u32 i=0; i< poolSize; i++){
		CCompEntity* pCompEntity = CCompEntity::GetPool()->GetSlot(i); 

		if (pCompEntity){
			CBaseModelInfo* pModelInfo = pCompEntity->GetBaseModelInfo();
			if (pModelInfo && (pModelInfo->GetHashKey() == (u32)ModelHashKey)){
				//				Vector3 delta = (pCompEntity->GetPosition()) - (Vector3)scrVecCoors;
				//				if (delta.Mag2() < (Radius*Radius)){
				if (IsLessThanAll(DistSquared(pCompEntity->GetTransform().GetPosition(), RCC_VEC3V(vCoors)), vRadiusSquared))
				{
					s32 returnCompEntityIndex = CCompEntity::GetPool()->GetIndex(pCompEntity);
					scriptAssertf(returnCompEntityIndex >= 0, "invalid index returned for %s",pCompEntity->GetModelName());
					return(returnCompEntityIndex);
				}
			}
		}
	}

	return(NULL_IN_SCRIPTING_LANGUAGE);
}

bool CommandDoesRayFireMapObjectExist(int iCompEntityIndex)
{
	if (scriptVerifyf(iCompEntityIndex >= 0, "DOES_RAYFIRE_MAP_OBJECT_EXIST - called with invalid argument"))
	{
		CCompEntity* pCompEntity = CCompEntity::GetPool()->GetAt(iCompEntityIndex); 

		if(pCompEntity)
		{
			return true; 
		}
	}

	return false; 
}

void CommandSetPickupRegenerationTime( int iPickupID, int regenTime )
{
	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));

		if (SCRIPT_VERIFY(pPlacement,"SET_PICKUP_REGENERATION_TIME - Pickup does not exist" ))
		{
			if (SCRIPT_VERIFY(!pPlacement->GetIsCollected(), "SET_PICKUP_REGENERATION_TIME - Pickup has already been collected" ) &&
				SCRIPT_VERIFY(!pPlacement->GetNetworkObject() || !pPlacement->GetNetworkObject()->HasBeenCloned(), "SET_PICKUP_REGENERATION_TIME - Can't set this once the pickup has been created on other machines. You must set this immediately after the pickup is created"))
			{
				pPlacement->SetRegenerationTime((u32)regenTime);
			}
		}
	}
}

void CommandForcePickupRegenerate(int iPickupID)
{
	if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));
		if (scriptVerifyf(pPlacement, "Placement with id: %d can not be found", iPickupID)
		&& !pPlacement->IsNetworkClone())
		{
#if ENABLE_NETWORK_LOGGING
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "FORCE_PICKUP_REGENERATE", "%s", pPlacement->GetNetworkObject()? pPlacement->GetNetworkObject()->GetLogName():"N/A");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // ENABLE_NETWORK_LOGGING
			CPickupManager::ForcePlacementFromRegenerationListToRegenerate(*pPlacement);
		}
	}
}

void CommandSetPlayerPermittedToCollectPickupsOfType(int PlayerIndex, int iPickupHash, bool bAllow)
{
	iPickupHash = ConvertOldPickupTypeToHash(iPickupHash);

	CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(PlayerIndex);

	if (pPlayer)
	{
#if ENABLE_NETWORK_LOGGING
		if (NetworkInterface::IsGameInProgress())
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SET_PLAYER_PERMITTED_TO_COLLECT_PICKUPS_OF_TYPE", "");
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Player", "%d", PlayerIndex);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Type", "0x%x", iPickupHash);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Allow", "%s", bAllow ? "True" : "False");
		}
#endif
		if (bAllow)
		{
			CPickupManager::AllowCollectionForPlayer((u32)iPickupHash, pPlayer->GetPhysicalPlayerIndex());

			Assertf(!CPickupManager::IsCollectionProhibited((u32)iPickupHash, pPlayer->GetPhysicalPlayerIndex()), "SET_PLAYER_PERMITTED_TO_COLLECT_PICKUPS_OF_TYPE failed to allow collection");

#if ENABLE_NETWORK_LOGGING
			if (NetworkInterface::IsGameInProgress() && CPickupManager::IsCollectionProhibited((u32)iPickupHash, pPlayer->GetPhysicalPlayerIndex()))
			{
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Fail", "Collection still prohibited");
			}
#endif
		}
		else
		{
			CPickupManager::ProhibitCollectionForPlayer((u32)iPickupHash, pPlayer->GetPhysicalPlayerIndex());
		}
	}
}

void CommandSetLocalPlayerPermittedToCollectPickupsWithModel(int modelHashKey, bool bAllow)
{
	if (bAllow)
	{
		CPickupManager::RemoveProhibitedPickupModelCollection(modelHashKey, CTheScripts::GetCurrentScriptName());
	}
	else
	{
		CPickupManager::ProhibitPickupModelCollectionForLocalPlayer(modelHashKey, CTheScripts::GetCurrentScriptName());
	}
}

void CommandAllowAllPlayersToCollectPickupsOfType(int iPickupHash)
{
	iPickupHash = ConvertOldPickupTypeToHash(iPickupHash);
	CPickupManager::RemoveProhibitedCollection((u32)iPickupHash);
}

void CommandSetTeamPickup(int iPickupID, int team)
{
	CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));

	if (SCRIPT_VERIFY(pPlacement,"SET_TEAM_PICKUP - Pickup does not exist" ) &&
		SCRIPT_VERIFY(team >=0 && team < MAX_NUM_TEAMS, "SET_TEAM_PICKUP - Invalid team"))
	{
		pPlacement->AddTeamPermit(static_cast<u8>(team));
	}
}

void CommandSetTeamPickupObject(int PickupIndex, int team, bool bSet)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup && SCRIPT_VERIFY(team >=0 && team < MAX_NUM_TEAMS, "SET_TEAM_PICKUP_OBJECT - Invalid team"))
	{
		if (bSet)
		{
			pPickup->AddTeamPermit(static_cast<u8>(team));
		}
		else
		{
			pPickup->RemoveTeamPermit(static_cast<u8>(team));
		}
	}
}

void CommandSetPickupGlowForSameTeam(int iPickupID)
{
	CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));
	if (SCRIPT_VERIFY(pPlacement,"SET_PICKUP_GLOW_IN_SAME_TEAM - Pickup does not exist" ))
	{
		pPlacement->SetGlowWhenInSameTeam();
	}
}

void CommandSetPickupObjectGlowForSameTeam(int iPickupID)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(iPickupID);
	if (pPickup)
	{
		pPickup->SetGlowWhenInSameTeam();
	}
}

void CommandSetObjectGlowForSameTeam(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (SCRIPT_VERIFY(pObj, "SET_OBJECT_GLOW_IN_SAME_TEAM - Object does not exist"))
	{
		if(pObj->IsPickup())
		{
			CPickup* pickup = static_cast<CPickup*>(pObj);
		
			if(SCRIPT_VERIFY(pickup, "SET_OBJECT_GLOW_IN_SAME_TEAM - Pickup does not exist"))
			{
				pickup->SetGlowWhenInSameTeam();
			}
		}
	}
}

void CommandSetPickupObjectGlowWhenUncollectable(int PickupIndex, bool bSet)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup)
	{
		pPickup->SetGlowOnProhibitCollection(bSet);
	}
}

void CommandSetPickupGlowOffset(int iPickupID, float offset)
{
	CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));

	if (SCRIPT_VERIFY(pPlacement,"SET_PICKUP_GLOW_OFFSET - Pickup does not exist" ))
	{
		pPlacement->SetPickupGlowOffset(offset);
	}
}

void CommandSetPickupObjectGlowOffset(int PickupIndex, float offset, bool bSet)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup)
	{
		pPickup->SetGlowOffset(offset);
		pPickup->SetOffsetGlow(bSet);
	}
}

void CommandSetPickupObjectArrowMarker(int PickupIndex, bool bSet)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup)
	{
		pPickup->SetArrowMarker(bSet);
	}
}

void CommandAllowPickupObjectArrowMarkerWhenUncollectable(int PickupIndex, bool bSet)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup)
	{
		pPickup->SetAllowArrowMarkerWhenUncollectable(bSet);
	}
}

int CommandGetDefaultAmmoForWeaponPickup(int iPickupHash)
{
	const CPickupData* pPickupData = CPickupDataManager::GetPickupData(iPickupHash);

	if (SCRIPT_VERIFY(pPickupData,"GET_DEFAULT_AMMO_FOR_WEAPON_PICKUP - Unrecognised pickup type" ))
	{
		for (u32 i=0; i<pPickupData->GetNumRewards(); i++)
		{
			if (pPickupData->GetReward(i)->GetType() == PICKUP_REWARD_TYPE_AMMO)
			{
				return SafeCast(const CPickupRewardAmmo, pPickupData->GetReward(i))->GetAmount();
			}
		}

		scriptAssertf(0, "GET_DEFAULT_AMMO_FOR_WEAPON_PICKUP - This pickup type is not a weapon pickup type");
	}

	return 0;
}

void CommandSetVehicleWeaponPickupsSharedByPassengers(bool bSet)
{
	CPickup::SetVehicleWeaponsSharedAmongstPassengers(bSet);
}

void CommandSetPickupGenerationRangeMultiplier(float multiplier)
{
	CPickupPlacement::SetGenerationRangeMultiplier(multiplier);

	if (multiplier != 1.0f && CTheScripts::GetCurrentGtaScriptHandler()->GetNumScriptResourcesOfType(CGameScriptResource::SCRIPT_RESOURCE_PICKUP_GENERATION_MULTIPLIER) == 0)
	{
		CScriptResource_PickupGenerationMultiplier pickupMultiplierResource;
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(pickupMultiplierResource);
	}
}

float CommandGetPickupGenerationRangeMultiplier()
{
	return CPickupPlacement::GetGenerationRangeMultiplier();
}

void CommandSetOnlyAllowAmmoCollectionWhenLow(bool bSet)
{
#if ENABLE_NETWORK_LOGGING
	if (NetworkInterface::IsGameInProgress() && bSet != CPickupManager::GetOnlyAllowAmmoCollectionWhenLowOnAmmo())
	{
		char str[10];
		formatf(str, "%s", bSet ? "true" : "false");
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "SET_ONLY_ALLOW_AMMO_COLLECTION_WHEN_LOW", str);
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptName());
	}
#endif

	CPickupManager::SetOnlyAllowAmmoCollectionWhenLowOnAmmo(bSet);
}

void CommandSetPickupUncollectable(int iPickupID, bool bSet)
{
	CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));

	if (SCRIPT_VERIFY(pPlacement,"SET_PICKUP_UNCOLLECTABLE - Pickup does not exist" ) &&
		SCRIPT_VERIFY(pPlacement->GetIsMapPlacement(), "SET_PICKUP_UNCOLLECTABLE - Only currently supported for map pickups (PLACEMENT_FLAG_MAP set)"))
	{
		pPlacement->SetUncollectable(bSet);
	}
}

void CommandSetPickupTransparentWhenUncollectable(int iPickupID, bool bSet)
{
	CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));

	if (SCRIPT_VERIFY(pPlacement,"SET_PICKUP_TRANSPARENT_WHEN_UNCOLLECTABLE - Pickup does not exist" ) &&
		SCRIPT_VERIFY(pPlacement->GetIsMapPlacement(), "SET_PICKUP_TRANSPARENT_WHEN_UNCOLLECTABLE - Only currently supported for map pickups (PLACEMENT_FLAG_MAP set)"))
	{
		pPlacement->SetTransparentWhenUncollectable(bSet);
	}
}

void CommandSetPickupHiddenWhenUncollectable(int iPickupID, bool bSet)
{
	CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(iPickupID));

	if (SCRIPT_VERIFY(pPlacement,"SET_PICKUP_HIDDEN_WHEN_UNCOLLECTABLE - Pickup does not exist" ) &&
		SCRIPT_VERIFY(pPlacement->GetIsMapPlacement(), "SET_PICKUP_HIDDEN_WHEN_UNCOLLECTABLE - Only currently supported for map pickups (PLACEMENT_FLAG_MAP set)"))
	{
		pPlacement->SetHiddenWhenUncollectable(bSet);
	}
}

void CommandSetPickupObjectTransparentWhenUncollectable(int PickupIndex, bool bSet)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup)
	{
		pPickup->SetTransparentWhenUncollectable(bSet);
	}
}

void CommandSetPickupObjectAlphaWhenTransparent(int alpha)
{
	if (SCRIPT_VERIFY(alpha>=0 && alpha<=255,"SET_PICKUP_OBJECT_ALPHA_WHEN_TRANSPARENT - Invalid alpha" ))
	{
		CPickup::PICKUP_ALPHA_WHEN_TRANSPARENT = (u32)alpha;
	}
}

void CommandSetPortablePickupPersist(int PickupIndex, bool toEnable)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup)
	{
		pPickup->SetPortablePickupPersist(toEnable);
	}
}

void CommandAllowPortablePickupToMigrateToNonParticipants(int PickupIndex, bool allow)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup && 
		SCRIPT_VERIFY(pPickup->GetNetworkObject(), "ALLOW_PORTABLE_PICKUP_TO_MIGRATE_TO_NON_PARTICIPANTS - the pickup is not networked") && 
		SCRIPT_VERIFY(pPickup->IsFlagSet(CPickup::PF_Portable), "ALLOW_PORTABLE_PICKUP_TO_MIGRATE_TO_NON_PARTICIPANTS - the pickup is not portable"))
	{
		pPickup->GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION, !allow);
	}
}

void CommandForceActivatePhysicsOnUnfixedPickup(int PickupIndex, bool force)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup && 
		SCRIPT_VERIFY(pPickup->IsFlagSet(CPickup::PF_Portable), "FORCE_PORTABLE_PICKUP_TO_NOT_FIX - the pickup is not portable"))
	{
		pPickup->SetForceActivatePhysicsOnUnfixedPickup(force);
	}
}

void CommandForceActivatePhysicsOnDroppedPickupNearSubmarine(int PickupIndex, bool force)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (pPickup && 
		SCRIPT_VERIFY(pPickup->IsFlagSet(CPickup::PF_Portable), "FORCE_ACTIVATE_PHYSICS_ON_DROPPED_PICKUP_NEAR_SUBMARINE - the pickup is not portable"))
	{
#if ENABLE_NETWORK_LOGGING
		if(NetworkInterface::IsGameInProgress() && (pPickup->GetForceActivatePhysicsOnDropNearSubmarine() != force))
		{
			netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
			NetworkLogUtils::WriteLogEvent(log, "FORCE_ACTIVATE_PHYSICS_ON_DROPPED_PICKUP_NEAR_SUBMARINE", "%s", pPickup->GetLogName());
			log.WriteDataValue("Force", "%s", force?"TRUE":"FALSE");
			log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif // ENABLE_NETWORK_LOGGING

		pPickup->SetForceActivatePhysicsOnUnfixedPickup(force);
	}
}

void CommandAllowPickupByNonParticipant(int PickupIndex, bool allow)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);

	if (SCRIPT_VERIFY(pPickup, "Pickup doesn't exists"))
	{
		pPickup->SetAllowNonScriptParticipantCollect(allow);
	}
}

bool CommandIsPortablePickupCustomColliderReady(int PickupIndex)
{
	const CPickup* pickup = CTheScripts::GetEntityToQueryFromGUID<CPickup>(PickupIndex);
	if(scriptVerifyf(pickup->IsFlagSet((CPickup::PF_Portable)), "IS_PORTABLE_PICKUP_CUSTOM_COLLIDER_READY called on non portable pickup from: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		return pickup->IsCustomColliderReady();
	}
	return true;
}

void CommandPreventCollectionOfPortablePickup(int PickupIndex, bool prevent, bool localOnly)
{
	CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex, localOnly ? CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES : CTheScripts::GUID_ASSERT_FLAGS_ALL);

#if ENABLE_NETWORK_LOGGING
	if(NetworkInterface::IsGameInProgress() && (!pPickup || pPickup->GetProhibitCollection(localOnly) != prevent))
	{
		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "PREVENT_COLLECTION_OF_PORTABLE_PICKUP", "%s", pPickup ? pPickup->GetLogName():"None");
		log.WriteDataValue("Script", "%s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		log.WriteDataValue("Prevent", "%s", prevent?"TRUE":"FALSE");
		log.WriteDataValue("localOnly", "%s", localOnly?"TRUE":"FALSE");
	}
#endif // ENABLE_NETWORK_LOGGING

	if (pPickup)
	{
		if (localOnly)
		{
			pPickup->SetProhibitLocalCollection(prevent);	
		}
		else
		{
			pPickup->SetProhibitCollection(prevent);
		}
	}
}

void CommandSetEntityFlag_SUPPRESS_SHADOW( int iEntityID, bool bFlag )
{
	CEntity *pEntity = const_cast<CEntity*>(CTheScripts::GetEntityToQueryFromGUID<CEntity>(iEntityID));

	if (pEntity)
	{
		fwFlags16& visibilityMask = pEntity->GetRenderPhaseVisibilityMask();

		if (bFlag) { visibilityMask.ClearFlag( VIS_PHASE_MASK_SHADOWS ); }
		else       { visibilityMask.SetFlag( VIS_PHASE_MASK_SHADOWS ); }
	}
}

void CommandSetEntityFlag_RENDER_SMALL_SHADOW( int iEntityID, bool bFlag )
{
	CEntity *pEntity = const_cast<CEntity*>(CTheScripts::GetEntityToQueryFromGUID<CEntity>(iEntityID));

	if (pEntity)
	{
		if (bFlag) { pEntity->SetBaseFlag  (fwEntity::RENDER_SMALL_SHADOW); }
		else       { pEntity->ClearBaseFlag(fwEntity::RENDER_SMALL_SHADOW); }
	}
}

bool CommandIsGarageEmpty(int garageHash, bool onAllMachines, int boxIndex)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);

	if (onAllMachines)
	{
		return CGarages::IsGarageEmptyOnAllMachines(index, boxIndex);
	}
	else
	{
		return CGarages::IsGarageEmpty(index, boxIndex);
	}
}

// This uses the vehicles bounding volume in the event the player is in a vehicle.
bool CommandIsPlayerEntirelyInsideGarage(int garageHash, int playerIndex, float margin, int boxIndex) 
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (SCRIPT_VERIFY(pPlayer, "Failed to get player, bad player index"))
	{
		return CGarages::IsPlayerEntirelyInsideGarage(index, pPlayer, margin, boxIndex);
	}

	return false;
}

// This uses the vehicles bounding volume in the event the player is in a vehicle.
bool CommandIsPlayerEntirelyOutsideGarage(int garageHash, int playerIndex, float margin, int boxIndex)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);
	if (SCRIPT_VERIFY(pPlayer, "Failed to get player, bad player index"))
	{
		return CGarages::IsPlayerEntirelyOutsideGarage(index, pPlayer, margin, boxIndex);
	}

	return false;
}

// This uses the vehicles bounding volume in the event the player is in a vehicle.
bool CommandIsPlayerPartiallyInsideGarage(int garageHash, int playerIndex, int boxIndex)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	CPed * pPlayer = CTheScripts::FindLocalPlayerPed(playerIndex);

	if (SCRIPT_VERIFY(pPlayer, "Failed to get player, bad player index"))
	{
		return CGarages::IsPlayerPartiallyInsideGarage(index, pPlayer, boxIndex);
	}

	return false;
}

bool CommandAreEntitiesEntirelyInsideGarage(int garageHash, bool peds, bool vehs, bool objs, int boxIndex)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	return CGarages::AreEntitiesEntirelyInsideGarage(index, peds, vehs, objs, 0.f, boxIndex);
}

bool CommandIsAnyEntityEntirelyInsideGarage(int garageHash, bool peds, bool vehs, bool objs, int boxIndex)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	return CGarages::IsAnyEntityEntirelyInsideGarage(index, peds, vehs, objs, 0.f, boxIndex);
}


bool CommandIsObjectEntirelyInsideGarage(int garageHash, int entityIndex, float margin, int boxIndex) 
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
	if (SCRIPT_VERIFY(pEntity, "Failed to get entity, bad entity index "))
	{
		return CGarages::IsObjectEntirelyInsideGarage(index, pEntity, margin, boxIndex);
	}
	return false;
}

bool CommandIsObjectEntirelyOutsideGarage(int garageHash, int entityIndex, float margin, int boxIndex)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
	if (SCRIPT_VERIFY(pEntity, "Failed to get entity, bad entity index "))
	{
		return CGarages::IsObjectEntirelyOutsideGarage(index, pEntity, margin, boxIndex);
	}
	return false;
}

bool CommandIsObjectPartiallyInsideGarage(int garageHash, int entityIndex, int boxIndex)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	const CEntity *pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);
	
	if (SCRIPT_VERIFY(pEntity, "Failed to get entity, bad entity index "))
	{
		return CGarages::IsObjectPartiallyInsideGarage(index, pEntity, boxIndex);
	}

	return false;
}

void CommandClearGarage(int garageHash, bool bBroadcast)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	atArray<CEntity*> objectArray;
	CGarages::FindAllObjectsInGarage(index, objectArray, true, true, true);

	CGarage *pGarage = &CGarages::aGarages[index];

	// Just use use box 0
	CGarage::Box &box = pGarage->m_boxes[0];
	float z = (box.CeilingZ - box.BaseZ) * 0.5f + box.BaseZ;

	Vector2 delta1(box.Delta1X, box.Delta1Y);
	Vector2 delta2(box.Delta2X, box.Delta2Y);
	Vector2 base(box.BaseX, box.BaseY);

	Vector2 pos2 = base + ((delta1 + delta2) * 0.5f);
	Vector3 position(pos2.x, pos2.y, z);

	for (int i = 0; i < objectArray.GetCount(); ++i)
	{
		if (objectArray[i]->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
		{
			continue;
		}
		if (objectArray[i]->GetIsTypePed())
		{
			CPed *pPlayer = static_cast<CPed*>(objectArray[i]);
			if (pPlayer->IsControlledByLocalOrNetworkPlayer())
			{
				continue;
			}
		}
		if (objectArray[i]->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(objectArray[i]);
			if (pVehicle->ContainsPlayer())
			{
				continue;
			}
		}

		objectArray[i]->SetPosition(position);
	}
	
	float halfMag1 = delta1.Mag() * 0.5f;
	float halfMag2 = delta2.Mag() * 0.5f;
	float radius = Min(halfMag1, halfMag2);

	scrVector srcVec(position);
	misc_commands::CommandClearArea(srcVec, radius, true, true, true, bBroadcast); 
}

void CommandClearObjectsFromGarage(int garageHash, bool vehicles, bool peds, bool objects, bool bBroadcast)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	atArray<CEntity*> objectArray;
	CGarages::FindAllObjectsInGarage(index, objectArray, peds, vehicles, objects);

	CGarage *pGarage = &CGarages::aGarages[index];
 
	// Just use use box 0
	CGarage::Box &box = pGarage->m_boxes[0];
	float z = (box.CeilingZ - box.BaseZ) * 0.5f + box.BaseZ;

	Vector2 delta1(box.Delta1X, box.Delta1Y);
	Vector2 delta2(box.Delta2X, box.Delta2Y);
	Vector2 base(box.BaseX, box.BaseY);

	Vector2 pos2 = base + ((delta1 + delta2) * 0.5f);
	Vector3 position(pos2.x, pos2.y, z);

	for (int i = 0; i < objectArray.GetCount(); ++i)
	{
		CEntity *pEntity = objectArray[i];
		if (pEntity->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
		{
			continue;
		}

		if (pEntity->GetIsTypePed())
		{
			CPed *pPlayer = static_cast<CPed*>(pEntity);
			if (pPlayer->IsControlledByLocalOrNetworkPlayer())
			{
				continue;
			}
		}

		if (pEntity->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pEntity);
			if (pVehicle->ContainsPlayer())
			{
				continue;
			}
		}

		if ((vehicles && pEntity->GetIsTypeVehicle()) ||
			(peds     && pEntity->GetIsTypePed())     ||
			(objects  && pEntity->GetIsTypeObject()))
		{
			pEntity->SetPosition(position);
		}
	}

	float halfMag1 = delta1.Mag() * 0.5f;
	float halfMag2 = delta2.Mag() * 0.5f;
	float radius = Min(halfMag1, halfMag2);

	scrVector srcVec(position);
	if (vehicles)
	{
		misc_commands::CommandClearAreaOfVehicles(srcVec, radius, true, false, false, false, bBroadcast, false, false); 
	}

	if (peds)
	{
		misc_commands::CommandClearAreaOfPeds(srcVec, radius, bBroadcast); 
	}

	if (objects)
	{
		s32 flags = 0;
		if (bBroadcast)
		{
			flags |= CLEAROBJ_FLAG_BROADCAST;
		}
		misc_commands::CommandClearAreaOfObjects(srcVec, radius, flags);
	}
}

void CommandCloseSafeHouseGarages()
{
	CGarages::CloseSafeHouseGarages();
}

void CommandEnableSavingInGarage(int garageHash, bool enable)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	if (SCRIPT_VERIFY(index >= 0 && index < CGarages::GetNumGarages(), "Garage hash returned an invalid index"))
	{
		CGarage *pGarage = &CGarages::aGarages[index];
		pGarage->Flags.bSavingVehiclesEnabled  = enable;
	}
}

bool CommandIsSavingEnableForGarage(int garageHash)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	if (SCRIPT_VERIFY(index >= 0 && index < CGarages::GetNumGarages(), "Garage hash returned an invalid index"))
	{
		CGarage *pGarage = &CGarages::aGarages[index];
		return pGarage->Flags.bSavingVehiclesEnabled;		
	}
	return false;
}

void CommandDisableTidyingUp(int garageHash, bool disable)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	if (SCRIPT_VERIFY(index >= 0 && index < CGarages::GetNumGarages(), "DISABLE_TIDYING_UP_IN_GARAGE - Garage hash returned an invalid index"))
	{
		CGarage *pGarage = &CGarages::aGarages[index];
		pGarage->Flags.bDisableTidyingUp  = disable;
	}
}

bool CommandIsTidyingUpDisabledForGarage(int garageHash)
{
	s32 index = CGarages::FindGarageIndexFromNameHash(garageHash);
	if (SCRIPT_VERIFY(index >= 0 && index < CGarages::GetNumGarages(), "IS_TIDYING_UP_DISABLED_FOR_GARAGE - Garage hash returned an invalid index"))
	{
		CGarage *pGarage = &CGarages::aGarages[index];
		return pGarage->Flags.bDisableTidyingUp;		
	}
	return false;
}

int CommandGetWeaponTypeFromPickupType(int pickupHash)
{
	pickupHash = ConvertOldPickupTypeToHash(pickupHash);
	const CPickupData* pPickupData = CPickupDataManager::GetPickupData((u32)pickupHash);
	if(SCRIPT_VERIFY(pPickupData, "GET_WEAPON_TYPE_FROM_PICKUP_TYPE: pickup metadata does not exist for this pickup type."))
	{
		u32 numRewards = pPickupData->GetNumRewards();
		for (u32 i=0; i<numRewards; i++)
		{
			const CPickupRewardData* pReward = pPickupData->GetReward(i);
			if(pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
			{
				return ((CPickupRewardWeapon*)pReward)->GetWeaponHash();
			}
		}
	}

	return 0;
}

int CommandGetPickupTypeFromWeaponHash(int weaponHash)
{
	const CWeaponInfo* weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
	if(weaponInfo)
	{
		return weaponInfo->GetPickupHash();
	}

	return 0;
}
//
//
//
//
bool CommandIsPickupWeaponObjectValid(int PickupIndex)
{
	const CPickup* pPickup = CTheScripts::GetEntityToModifyFromGUID<CPickup>(PickupIndex);
	if (SCRIPT_VERIFY(pPickup,"GET_PICKUP_OBJECT - Pickup does not exist"))		
	{
		if (pPickup->GetWeapon())
		{
			return true;
		}
	}
	return false;
}

int CommandGetObjectTintIndex(int ObjectIndex)
{
	const CObject *pObj = CTheScripts::GetEntityToQueryFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{
		return pObj->GetTintIndex();
	}
	return 0;
}

void CommandSetObjectTintIndex(int ObjectIndex, int TintIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{
		pObj->SetTintIndex(TintIndex);

		CCustomShaderEffectTint *pTintCSE = pObj->GetCseTintFromObject();
		if(pTintCSE)
		{
			pTintCSE->SelectTintPalette((u8)TintIndex, pObj);
		} 

#if GTA_REPLAY
		if(NetworkInterface::IsGameInProgress() && pObj->GetModelNameHash() == 0x4fcad2e0/*"apa_mp_apa_yacht"*/ && pObj->GetRelatedDummy())
		{
			CReplayMgr::RecordFx(CPacketSetDummyObjectTint(pObj->GetMatrix().GetCol3(), pObj->GetModelNameHash(), TintIndex, pObj->GetRelatedDummy()));
		}
		else if(NetworkInterface::IsGameInProgress())
		{
			CReplayMgr::RecordFx(CPacketSetObjectTint(TintIndex, pObj), pObj);
		}
#endif // GTA_REPLAY
	}
}

//
//
//
//
int CommandGetTintIndexClosestBuildingOfType(const scrVector & scrVecCentreCoors, float Radius, int ObjectModelHashKey)
{
	Vector3 VecNewCoors = Vector3(scrVecCentreCoors);

	s32 nOwnedByFlags = 0xFFFFFFFF;

	CEntity *pClosestEntity = CObject::GetPointerToClosestMapObjectForScript(VecNewCoors.x, VecNewCoors.y, VecNewCoors.z, Radius, ObjectModelHashKey, ENTITY_TYPE_MASK_BUILDING, nOwnedByFlags);
	if (pClosestEntity)
	{
	    CBuilding *pClosestBuilding = NULL;

		if(pClosestEntity->GetIsTypeBuilding())
		{
			pClosestBuilding = static_cast<CBuilding*>(pClosestEntity);
		}
		else
		{
		    scriptAssertf(false, "%s:GET_TINT_INDEX_CLOSEST_BUILDING_OF_TYPE - grabbed object %s is not a building!", 
				CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestBuilding->GetModelName());
			return -1;
		}


	    if (pClosestBuilding)
	    {
			return pClosestBuilding->GetTintIndex();
	    }
	}

	return -1;
}

static CCustomShaderEffectTint* GetCseTintFromEntity(CEntity *pEntity)
{
	if( pEntity->GetDrawHandlerPtr() )
	{
		fwCustomShaderEffect *fwCSE = pEntity->GetDrawHandler().GetShaderEffect();
		if(fwCSE && fwCSE->GetType()==CSE_TINT)
		{
			return static_cast<CCustomShaderEffectTint*>(fwCSE);
		}
	}
	return NULL;
}

//
//
//
//
bool CommandSetTintIndexClosestBuildingOfType(const scrVector & scrVecCentreCoors, float Radius, int ObjectModelHashKey, int TintIndex)
{
	Vector3 VecNewCoors = Vector3(scrVecCentreCoors);

	s32 nOwnedByFlags = 0xFFFFFFFF;

	CEntity *pClosestEntity = CObject::GetPointerToClosestMapObjectForScript(VecNewCoors.x, VecNewCoors.y, VecNewCoors.z, Radius, ObjectModelHashKey, ENTITY_TYPE_MASK_BUILDING, nOwnedByFlags);
	if (pClosestEntity)
	{
	    CBuilding *pClosestBuilding = NULL;

		if(pClosestEntity->GetIsTypeBuilding())
		{
			pClosestBuilding = static_cast<CBuilding*>(pClosestEntity);
		}
		else
		{
		    scriptAssertf(false, "%s:SET_TINT_INDEX_CLOSEST_BUILDING_OF_TYPE - grabbed object %s is not a building!", 
				CTheScripts::GetCurrentScriptNameAndProgramCounter(), pClosestBuilding->GetModelName());
			return false;
		}


	    if (pClosestBuilding)
	    {
			pClosestBuilding->SetTintIndex(TintIndex);

			CCustomShaderEffectTint *pTintCSE = GetCseTintFromEntity(pClosestBuilding);
			if(pTintCSE)
			{
				pTintCSE->SelectTintPalette((u8)TintIndex, pClosestBuilding);
			} 

			
			#if GTA_REPLAY
					if(NetworkInterface::IsGameInProgress())
					{
						CReplayMgr::RecordFx(CPacketSetBuildingTint(VECTOR3_TO_VEC3V(VecNewCoors), Radius, ObjectModelHashKey, TintIndex, pClosestBuilding));
					}
			#endif // GTA_REPLAY
			
			return true;
	    }
	}

	return false;
}

//
//
//
//
static CCustomShaderEffectProp* GetCsePropFromObject(CObject *pObj)
{
	fwCustomShaderEffect *fwCSE = pObj->GetDrawHandler().GetShaderEffect();
	if(fwCSE && fwCSE->GetType()==CSE_PROP)
	{
		return static_cast<CCustomShaderEffectProp*>(fwCSE);
	}

	return NULL;
}

int CommandGetPropTintIndex(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{
		CCustomShaderEffectProp *pPropCSE = GetCsePropFromObject(pObj);
		if(pPropCSE)
		{
			return pPropCSE->GetMaxTintPalette();
		}
	}
	return 0;
}

void CommandSetPropTintIndex(int ObjectIndex, int TintIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{
		CCustomShaderEffectProp *pPropCSE = GetCsePropFromObject(pObj);
		if(pPropCSE)
		{
			return pPropCSE->SelectTintPalette((u8)TintIndex, pObj);
		}
	}
}



//
//
//
//
bool CommandSetPropLightColor(int ObjectIndex, bool enable, int red, int green, int blue)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	bool bSuccess = false;

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();
	Color32 previousColour(255, 255, 255, 255);

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if(pLightEntity && (pLightEntity->GetParent() == pObj))
			{
				bSuccess = true;

				pLightEntity->OverrideLightColor(enable, (u8)red, (u8)green, (u8)blue);
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

#if GTA_REPLAY
	if(NetworkInterface::IsGameInProgress() && bSuccess)
	{
		Color32* prevColour = nullptr;
		CPacketOverrideObjectLightColour::GetColourOverride(pObj->GetReplayID(), prevColour);

	 	Color32 newColour((u8)red, (u8)green, (u8)blue, enable ? 255 : 0);
	 	if(prevColour == nullptr || *prevColour != newColour)
	 	{
			if(prevColour)
				previousColour = *prevColour;

	 		CReplayMgr::RecordFx(CPacketOverrideObjectLightColour(previousColour, newColour), pObj);

			CPacketOverrideObjectLightColour::StoreColourOverride(pObj->GetReplayID(), newColour);

			replayDebugf3("Record colour override %p %d 0x%08X, %d, %d, %d, %d (%d)", pObj, ObjectIndex, pObj->GetReplayID().ToInt(), red, green, blue, enable ? 255 : 0, CPacketOverrideObjectLightColour::GetColourOverrideCount());
	 	}
	}
#endif // GTA_REPLAY

	return bSuccess;
}

//
//
//
//
bool CommandIsPropLightColorOverriden(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if (pLightEntity && (pLightEntity->GetParent() == pObj))
			{
				// status of first child only:
				return pLightEntity->IsLightColorOverriden();
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

	return false;
}

//
//
//
//
int CommandGetPropLightColorRed(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if (pLightEntity && (pLightEntity->GetParent() == pObj))
			{	// status of first child only:
				Color32 col = pLightEntity->GetOverridenLightColor();
				return pLightEntity->IsLightColorOverriden() ? col.GetRed() : -1;
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

	return -1;
}

int CommandGetPropLightColorGreen(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if(pLightEntity && (pLightEntity->GetParent() == pObj))
			{	// status of first child only:
				Color32 col = pLightEntity->GetOverridenLightColor();
				return pLightEntity->IsLightColorOverriden() ? col.GetGreen() : -1;
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

	return -1;
}

int CommandGetPropLightColorBlue(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if(pLightEntity && (pLightEntity->GetParent() == pObj))
			{	// status of first child only:
				Color32 col = pLightEntity->GetOverridenLightColor();
				return pLightEntity->IsLightColorOverriden() ? col.GetBlue() : -1;
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

	return -1;
}

int CommandGetPropLightOrigColorRed(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if (pLightEntity && (pLightEntity->GetParent() == pObj))
			{	// status of first child only:
				CLightAttr* attr = pLightEntity->GetLight();
				if (attr)
				{
					return attr->m_colour.red;
				}
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

	return -1;
}

int CommandGetPropLightOrigColorGreen(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if (pLightEntity && (pLightEntity->GetParent() == pObj))
			{	// status of first child only:
				CLightAttr* attr = pLightEntity->GetLight();
				if (attr)
				{
					return attr->m_colour.green;
				}
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

	return -1;
}

int CommandGetPropLightOrigColorBlue(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	const s32 maxNumLightEntities = (s32)CLightEntity::GetPool()->GetSize();

	for(s32 i=0; i<maxNumLightEntities; i++)
	{
		if(CLightEntity::GetPool()->GetIsUsed(i))
		{
			CLightEntity* pLightEntity = CLightEntity::GetPool()->GetSlot(i);
			if (pLightEntity && (pLightEntity->GetParent() == pObj))
			{	// status of first child only:
				CLightAttr* attr = pLightEntity->GetLight();
				if (attr)
				{
					return attr->m_colour.blue;
				}
			}
		}
	}//for(s32 i=0; i<maxNumLightEntities; i++)...

	return -1;
}



void CommandSetForceObjectThisFrame(const scrVector & vPos, float fRadius)
{
	scriptAssertf(fRadius<=80.0f, "%s:SET_FORCE_OBJECT_THIS_FRAME - radius %.2f is too big (50m max)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), fRadius);

	const float fClampedRadius = Clamp(fRadius, 1.0f, 80.0f);
	spdSphere forceObjSphere((Vec3V) vPos, ScalarV(fClampedRadius) );

	CObjectPopulation::SetScriptForceConvertVolumeThisFrame(forceObjSphere);
}

void CommandOnlyCleanUpObjectWhenOutOfRange(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{
		pObj->m_nObjectFlags.bOnlyCleanUpWhenOutOfRange = true;
	}
}

int CommandDoesObjectExistWithDecorator(const char *label)
{
    CObject::Pool *pool = CObject::GetPool();

    if(pool)
    {
        atHashWithStringBank decKey(label);

        s32 i = (s32) pool->GetSize();

	    while(i--)
	    {
		    CObject *pObject = pool->GetSlot(i);

            if(pObject)
            {
                if(fwDecoratorExtension::ExistsOn(*pObject, decKey))
                {
                    return CTheScripts::GetGUIDFromEntity(const_cast<CObject&>(*pObject));
                }
            }
        }
    }

    return NULL_IN_SCRIPTING_LANGUAGE;
}

void CommandSetIsVisibleInMirrors(int ObjectIndex, bool bEnable)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if (pObj)
	{
		pObj->ChangeVisibilityMask(VIS_PHASE_MASK_MIRROR_REFLECTION, bEnable, false);
	}
}

void CommandSetObjectSpeedBoostAmount( int ObjectIndex, int BoostAmount )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID< CObject >( ObjectIndex );
	if( pObj )
	{
		if( scriptVerifyf( BoostAmount > 0 && BoostAmount < 255, "%s:SET_OBJECT_SPEED_BOOST_AMOUNT - setting boost amount to an invalid value %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), BoostAmount ) )
		{
			pObj->SetSpeedBoost( (u8)BoostAmount );
		}
	}
}


void CommandSetObjectSpeedBoostDuration( int ObjectIndex, float BoostDuration )
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID< CObject >( ObjectIndex );
	if( pObj )
	{
		if( scriptVerifyf( BoostDuration > 0.0f && BoostDuration < 11.0f, "%s:SET_OBJECT_SPEED_BOOST_DURATION - setting boost duration to an invalid value %f", CTheScripts::GetCurrentScriptNameAndProgramCounter(), BoostDuration ) )
		{
			pObj->SetSpeedBoostDuration( (u8)( BoostDuration * 25.0f ) );
		}
	}
}

void CommandSetDisableCarCollisionsWithCarParachutes( bool disableCollisions )
{
	CObject::ms_bDisableCarCollisionsWithCarParachute = disableCollisions;
}


void CommandSetProjectilesShouldExplodeWhenHitting( int ObjectIndex, bool Explode )
{
    CObject *pObj = CTheScripts::GetEntityToModifyFromGUID< CObject >( ObjectIndex );
    if( pObj )
    {
        pObj->SetProjectilesShouldExplodeOnImpact( Explode );
    }
}

void CommandSetDriveArticulatedJoint( int iEntityID, bool driveOpen, int jointIndex )
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( SCRIPT_VERIFY( pObject, "SET_ARTICULATED_JOINT_TARGET_ANGLE - Entity is not an object" ) )
	{
		//if( SCRIPT_VERIFY( ( driveOpen && !pObject->IsDriveToMaxAngleEnabled() ) || ( !driveOpen ), "SET_ARTICULATED_JOINT_TARGET_ANGLE - Driving joint that is still being driven" ) )
		if( ( driveOpen && !pObject->IsDriveToMaxAngleEnabled() ) || !driveOpen )
		{
			pObject->SetDriveToTarget( driveOpen, jointIndex );
		}
	}
}

void CommandSetDriveArticulatedJointWithInflictor( int iEntityID, bool driveOpen, int jointIndex, int iInflictorID )
{
	CommandSetDriveArticulatedJoint( iEntityID, driveOpen, jointIndex );
	CEntity* pInflictor = CTheScripts::GetEntityToModifyFromGUID<CEntity>( iInflictorID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( pObject )
	{
		pObject->SetDamageInflictor( pInflictor );
	}
}

void CommandSetObjectIsAPressurePlate( int iEntityID, bool isPressurePlate )
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( SCRIPT_VERIFY( pObject, "SET_OBJECT_IS_A_PRESSURE_PLATE - Entity is not an object" ) )
	{
		pObject->SetObjectIsAPressurePlate( isPressurePlate );
	}
}

bool CommandGetIsPressurePlatePressed( int iEntityID )
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( SCRIPT_VERIFY( pObject, "GET_IS_PRESSURE_PLATE_PRESSED - Entity is not an object" ) )
	{
		return pObject->IsPressurePlatePressed();
	}

	return false;
}


bool CommandGetIsArticulatedJointAtMinAngle( int iEntityID, int jointIndex )
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( SCRIPT_VERIFY( pObject, "GET_IS_ARTICULATED_JOINT_AT_MIN_ANGLE - Entity is not an object" ) )
	{
		return pObject->IsJointAtMinAngle( jointIndex );
	}

	return false;
}

bool CommandGetIsArticulatedJointAtMaxAngle(int iEntityID, int jointIndex)
{
    CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

    if (SCRIPT_VERIFY(pObject, "GET_IS_ARTICULATED_JOINT_AT_MAX_ANGLE - Entity is not an object"))
    {
        return pObject->IsJointAtMaxAngle(jointIndex);
    }

    return false;
}

void CommandSetWeaponImpactsApplyGreaterForce( int iEntityID, bool weaponImpactsApplyGreaterForce )
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( pObject )
	{
		pObject->SetWeaponImpactsApplyGreaterForce( weaponImpactsApplyGreaterForce );
	}
}

void CommandSetObjectIsArticulated( int iEntityID, bool isArticulated )
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( SCRIPT_VERIFY( pObject, "SET_IS_OBJECT_ARTICULATED - Entity is not an object" ) )
	{
		pObject->SetObjectIsArticulated( isArticulated );
	}
}

void CommandSetObjectIsBall( int iEntityID, bool isBall )
{
	CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

	if( SCRIPT_VERIFY( pObject, "SET_IS_OBJECT_BALL - Entity is not an object" ) )
	{
		pObject->SetObjectIsBall( isBall );
	}
}

void CommandSetObjectPopsTyres( int iEntityID, bool popsTyres )
{
    CObject *pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>( iEntityID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES );

    if( SCRIPT_VERIFY( pObject, "SET_OBJECT_POPS_TYRES - Entity is not an object" ) )
    {
        pObject->m_nObjectFlags.bPopTires = popsTyres;
    }
}


void CommandSetObjectAlbedoFlag(int ObjectIndex, bool enable)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		pObject->SetIsAlbedoAlpha(enable);
	}
}

bool CommandGetObjectAlbedoFlag(int ObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		return pObject->GetIsAlbedoAlpha();
	}

	return false;
}

void CommandEnableGlobalAlbedoObjectsSupport(bool enable)
{
	CObjectPopulation::EnableCObjectsAlbedoSupport(enable);
}



void SetupScriptCommands()
{
	SCR_REGISTER_SECURE(CREATE_OBJECT,0x0e536d72ab30f4c8,											CommandCreateObject							);		
	SCR_REGISTER_SECURE(CREATE_OBJECT_NO_OFFSET,0x0a7322c6d0e1a985,									CommandCreateObjectNoOffset					);
	SCR_REGISTER_SECURE(DELETE_OBJECT,0x4bda5afd88c085eb,											CommandDeleteObject							);
	SCR_REGISTER_SECURE(PLACE_OBJECT_ON_GROUND_PROPERLY,0x82af23f360c69cae,							CommandPlaceObjectOnGroundProperly			);
	SCR_REGISTER_SECURE(PLACE_OBJECT_ON_GROUND_OR_OBJECT_PROPERLY,0xe6a4c4b2e8de36d3,				CommandPlaceObjectOnGroundOrObjectProperly	);
	SCR_REGISTER_SECURE(ROTATE_OBJECT,0x3f35e3308bda6748,											CommandRotateObject							);		
	SCR_REGISTER_SECURE(SLIDE_OBJECT,0x4a84ec87916ef892,											CommandSlideObject							);		
	SCR_REGISTER_SECURE(SET_OBJECT_TARGETTABLE,0x6486d097e5c691f3,									CommandSetObjectTargettable					);
	SCR_REGISTER_SECURE(SET_OBJECT_FORCE_VEHICLES_TO_AVOID,0x0321bbb2a2b3b1ed,						CommandSetObjectForceVehiclesToAvoid		);	
	SCR_REGISTER_SECURE(GET_CLOSEST_OBJECT_OF_TYPE,0x87a467676bdf8c35,								CommandGetClosestObjectOfType				);
	SCR_REGISTER_SECURE(HAS_OBJECT_BEEN_BROKEN,0xadebc1d44fe55f78,									CommandHasObjectBeenBroken					);		
	SCR_REGISTER_SECURE(HAS_CLOSEST_OBJECT_OF_TYPE_BEEN_BROKEN,0x438657bc68985918,					CommandHasClosestObjectOfTypeBeenBroken		);
	SCR_REGISTER_SECURE(HAS_CLOSEST_OBJECT_OF_TYPE_BEEN_COMPLETELY_DESTROYED,0xfa14247da5d930c6,	CommandHasClosestObjectOfTypeBeenCompletelyDestroyed);
	SCR_REGISTER_SECURE(GET_HAS_OBJECT_BEEN_COMPLETELY_DESTROYED,0x1bf1f6d9dc221b91,				CommandGetHasObjectBeenCompletelyDestroyed);
	SCR_REGISTER_SECURE(GET_OFFSET_FROM_COORD_AND_HEADING_IN_WORLD_COORDS,0x6db7fbd602c51670,		CommandGetOffsetFromCoordAndHeadingInWorldCoords);	
	SCR_REGISTER_UNUSED(FREEZE_POSITION_OF_CLOSEST_OBJECT_OF_TYPE,0xbc22d08e20303367,				CommandFreezePositionOfClosestObjectOfType	);
	SCR_REGISTER_SECURE(GET_COORDS_AND_ROTATION_OF_CLOSEST_OBJECT_OF_TYPE,0x5fe5dad7c292ebe5,		CommandGetCoordsAndRotationOfClosestObjectOfType);
	SCR_REGISTER_SECURE(SET_STATE_OF_CLOSEST_DOOR_OF_TYPE,0x798e4bba7a3c56ae,						CommandSetStateOfClosestDoorOfType			);
	SCR_REGISTER_SECURE(GET_STATE_OF_CLOSEST_DOOR_OF_TYPE,0xd25fb7e1030e5e2d,						CommandGetStateOfClosestDoorOfType			);
	SCR_REGISTER_SECURE(SET_LOCKED_UNSTREAMED_IN_DOOR_OF_TYPE,0x82ef7f252a48dc41,					CommandSetLockedUnstreamedInDoorOfType		);
	SCR_REGISTER_SECURE(PLAY_OBJECT_AUTO_START_ANIM,0xc225c3d9cc954480,								CommandPlayObjectAutoStartAnim);

	SCR_REGISTER_SECURE(ADD_DOOR_TO_SYSTEM,0x5dd86ac785d8e188,										CommandAddDoorToSystem						);
	SCR_REGISTER_SECURE(REMOVE_DOOR_FROM_SYSTEM,0xa5bd780899e2d5f0,									CommandRemoveDoorFromSystem					);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_SET_DOOR_STATE,0x7e15597aa5f53f9a,								CommandDoorSystemSetState                   );
	SCR_REGISTER_SECURE(DOOR_SYSTEM_GET_DOOR_STATE,0xa570d693c4a2b421,								CommandDoorSystemGetState                   );
	SCR_REGISTER_SECURE(DOOR_SYSTEM_GET_DOOR_PENDING_STATE,0xdc62e683fb337ec3,						CommandDoorSystemGetPendingState            );

	SCR_REGISTER_SECURE(DOOR_SYSTEM_SET_AUTOMATIC_RATE,0x1f0b6d1966e55ddb,							CommandDoorSystemSetAutomaticRate			);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_SET_AUTOMATIC_DISTANCE,0x32dbceb580393767,						CommandDoorSystemSetAutomaticDistance		);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_SET_OPEN_RATIO,0x1b3e1454bd86fce6,								CommandDoorSystemSetOpenRatio			    );

	SCR_REGISTER_UNUSED(DOOR_SYSTEM_GET_AUTOMATIC_RATE,0xb3eef85759c0825d,							CommandDoorSystemGetAutomaticRate			);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_GET_AUTOMATIC_DISTANCE,0x478b4c9fd32a6ef9,						CommandDoorSystemGetAutomaticDistance		);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_GET_OPEN_RATIO,0xc4336736b30c9df5,								CommandDoorSystemGetOpenRatio			    );

	SCR_REGISTER_UNUSED(DOOR_SYSTEM_GET_IS_SPRING_REMOVED,0xc1a5bd435907950f,						CommandDoorSystemGetUnsprung				);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_SET_SPRING_REMOVED,0xf9d6c7efa350cf1d,							CommandDoorSystemSetUnsprung				);

	SCR_REGISTER_UNUSED(DOOR_SYSTEM_GET_IS_HOLDING_OPEN,0x0b3c3f3cb7039f8f,							CommandDoorSystemGetHoldOpen				);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_SET_HOLD_OPEN,0x240efbc99404843a,								CommandDoorSystemSetHoldOpen				);

	SCR_REGISTER_SECURE(DOOR_SYSTEM_SET_DOOR_OPEN_FOR_RACES,0xeeff17c042a8cb2c,						CommandDoorSystemSetOpenForRaces			);
	SCR_REGISTER_UNUSED(DOOR_SYSTEM_GET_DOOR_OPEN_FOR_RACES,0xc00251d702757ea3,						CommandDoorSystemGetOpenForRaces			);
	
	SCR_REGISTER_UNUSED(SET_DOOR_OBJECT_ALWAYS_PUSH_VEHICLES,0xac05f02664b3fb07,					CommandDoorSetAlwaysPushVehicles			);
		
	SCR_REGISTER_SECURE(IS_DOOR_REGISTERED_WITH_SYSTEM,0xec32121f2e3875c8,							CommandIsDoorRegisteredWithSystem			);
	SCR_REGISTER_SECURE(IS_DOOR_CLOSED,0xbe1365fc512b1f58,											CommandIsDoorClosed							);
	SCR_REGISTER_UNUSED(IS_DOOR_FULLY_OPEN,0xdc06ae18038d670f,										CommandIsDoorFullyOpen						);

	SCR_REGISTER_SECURE(OPEN_ALL_BARRIERS_FOR_RACE,0x25dc8c12248c3966,								CommandOpenAllBarriersForRace				);
	SCR_REGISTER_SECURE(CLOSE_ALL_BARRIERS_FOR_RACE,0x711d6d89f98ade64,								CommandCloseAllBarriersOpenedForRace		);

	SCR_REGISTER_UNUSED(DOOR_SET_ALWAYS_PUSH,0x480500aa9e8b631f,									CommandDoorSetAlwaysPush					);

	SCR_REGISTER_SECURE(DOOR_SYSTEM_GET_IS_PHYSICS_LOADED,0x163a0b16532bf618,						CommandDoorSystemGetIsPhysicsLoaded			);
	SCR_REGISTER_SECURE(DOOR_SYSTEM_FIND_EXISTING_DOOR,0x733969cd24ac2dd3,							CommandDoorSystemFindExistingDoor			);

	SCR_REGISTER_SECURE(IS_GARAGE_EMPTY,0x627ec3ebc804e164,											CommandIsGarageEmpty						);
	SCR_REGISTER_SECURE(IS_PLAYER_ENTIRELY_INSIDE_GARAGE,0x3bbe1aefbffe5646,						CommandIsPlayerEntirelyInsideGarage			);
	SCR_REGISTER_UNUSED(IS_PLAYER_ENTIRELY_OUTSIDE_GARAGE,0xa5e25f9b71a87908,						CommandIsPlayerEntirelyOutsideGarage		);
	SCR_REGISTER_SECURE(IS_PLAYER_PARTIALLY_INSIDE_GARAGE,0xf2706324c8ce4795,						CommandIsPlayerPartiallyInsideGarage		);
	SCR_REGISTER_SECURE(ARE_ENTITIES_ENTIRELY_INSIDE_GARAGE,0x8f65a904fbec6771,						CommandAreEntitiesEntirelyInsideGarage		);
	SCR_REGISTER_SECURE(IS_ANY_ENTITY_ENTIRELY_INSIDE_GARAGE,0x57550d67d90afdd6,					CommandIsAnyEntityEntirelyInsideGarage		);

	SCR_REGISTER_SECURE(IS_OBJECT_ENTIRELY_INSIDE_GARAGE,0x26b6fbd29e6b5df6,						CommandIsObjectEntirelyInsideGarage			);
	SCR_REGISTER_UNUSED(IS_OBJECT_ENTIRELY_OUTSIDE_GARAGE,0xd8b483e51cad8cfc,						CommandIsObjectEntirelyOutsideGarage		);
	SCR_REGISTER_SECURE(IS_OBJECT_PARTIALLY_INSIDE_GARAGE,0xe251d6fac6328abf,						CommandIsObjectPartiallyInsideGarage		);

	SCR_REGISTER_SECURE(CLEAR_GARAGE,0xd3fd4677db79e1d1,											CommandClearGarage							);
	SCR_REGISTER_SECURE(CLEAR_OBJECTS_INSIDE_GARAGE,0x22676f5a35047ca3,								CommandClearObjectsFromGarage				);

	SCR_REGISTER_SECURE(DISABLE_TIDYING_UP_IN_GARAGE,0xeaec749a161c79e4,							CommandDisableTidyingUp						);
	SCR_REGISTER_UNUSED(IS_TIDYING_UP_DISABLED_FOR_GARAGE,0xeb1f9fd79915129b,						CommandIsTidyingUpDisabledForGarage			);
	SCR_REGISTER_UNUSED(IS_SAVING_ENABLED_FOR_GARAGE,0xeb21b94a7160cc61,							CommandIsSavingEnableForGarage				);
	SCR_REGISTER_SECURE(ENABLE_SAVING_IN_GARAGE,0x334505b911993b1e,									CommandEnableSavingInGarage					);
	SCR_REGISTER_SECURE(CLOSE_SAFEHOUSE_GARAGES,0xb2443d8f4dba9077,									CommandCloseSafeHouseGarages				);

	SCR_REGISTER_SECURE(DOES_OBJECT_OF_TYPE_EXIST_AT_COORDS,0x574e2a23f0f76e35,						CommandDoesObjectOfTypeExistAtCoords		);
	SCR_REGISTER_SECURE(IS_POINT_IN_ANGLED_AREA,0x5f8653e60ed2288e,									CommandIsPointInAngledArea					);
	SCR_REGISTER_UNUSED(SET_OBJECT_AS_STEALABLE,0xcd45c0d75d9bf726,									CommandSetObjectAsStealable					);		
	SCR_REGISTER_UNUSED(HAS_OBJECT_BEEN_UPROOTED,0x65bc987a38ca0272,								CommandHasObjectBeenUprooted				);		
	SCR_REGISTER_UNUSED(SET_NEAREST_BUILDING_ANIM_RATE,0xe5f39e67f337bf33,							CommandSetNearestBuildingAnimRate);
	SCR_REGISTER_UNUSED(GET_NEAREST_BUILDING_ANIM_RATE,0xdd6b1ab4b4acae7e,							CommandGetNearestBuildingAnimRate);
	SCR_REGISTER_UNUSED(SET_NEAREST_BUILDING_ANIM_PHASE,0x17818d14b29dc55a,							CommandSetNearestBuildingAnimPhase);
	SCR_REGISTER_UNUSED(GET_NEAREST_BUILDING_ANIM_PHASE,0x371743df03ed7aee,							CommandGetNearestBuildingAnimPhase);
	SCR_REGISTER_SECURE(SET_OBJECT_ALLOW_LOW_LOD_BUOYANCY,0x225f199ba06b3d82,						CommandSetObjectAllowLowLodBuoyancy);
	SCR_REGISTER_SECURE(SET_OBJECT_PHYSICS_PARAMS,0xcc2383c749571bd1,								CommandSetObjectPhysicsParams);
	SCR_REGISTER_SECURE(GET_OBJECT_FRAGMENT_DAMAGE_HEALTH,0x13ac33b32a4f4e11,						CommandGetObjectFragmentDamageHealth);
	SCR_REGISTER_SECURE(SET_ACTIVATE_OBJECT_PHYSICS_AS_SOON_AS_IT_IS_UNFROZEN,0xe987efb3b3fab187,	CommandSetActivateObjectPhysicsAsSoonAsItIsUnfrozen);
	SCR_REGISTER_UNUSED(SET_ARROW_ABOVE_BLIPPED_PICKUPS, 0xede49b32,								CommandSetArrowAboveBlippedPickups);
	SCR_REGISTER_SECURE(IS_ANY_OBJECT_NEAR_POINT,0x2d3cddf3fe35fca6,								CommandIsAnyObjectNearPoint); 
	SCR_REGISTER_SECURE(IS_OBJECT_NEAR_POINT,0x6870f49b9937b9d1,									CommandIsObjectNearPoint); 
	SCR_REGISTER_UNUSED(REQUEST_OBJECT_HIGH_DETAIL_MODEL,0x3b03a53d753d3bf9,						CommandRequestObjectHighDetailModel);
	SCR_REGISTER_SECURE(REMOVE_OBJECT_HIGH_DETAIL_MODEL,0x5f012c14ed0c0c36,							CommandRemoveObjectHighDetailModel);

	SCR_REGISTER_SECURE(BREAK_OBJECT_FRAGMENT_CHILD,0xb35d04a52bd51f16,								CommandObjectFragmentBreakChild);	
	SCR_REGISTER_SECURE(DAMAGE_OBJECT_FRAGMENT_CHILD,0xae6555cd897a035d,							CommandObjectFragmentDamageChild);	
	SCR_REGISTER_SECURE(FIX_OBJECT_FRAGMENT,0xf4da4bbc6876e774,										CommandObjectFragmentFix);

	SCR_REGISTER_SECURE(TRACK_OBJECT_VISIBILITY,0x6e740265db1f6ea1,									CommandTrackObjectVisibility);
	SCR_REGISTER_SECURE(IS_OBJECT_VISIBLE,0x3e04de565f9095b8,										CommandIsObjectVisible);

	SCR_REGISTER_SECURE(SET_OBJECT_IS_SPECIAL_GOLFBALL,0x04a0ccec1611db8a,							CommandSetObjectIsSpecialGolfBall);
	SCR_REGISTER_SECURE(SET_OBJECT_TAKES_DAMAGE_FROM_COLLIDING_WITH_BUILDINGS,0x2c08bd44740dbbfe,   CommandSetTakesDamageFromCollidingWithBuildings);
	SCR_REGISTER_SECURE(ALLOW_DAMAGE_EVENTS_FOR_NON_NETWORKED_OBJECTS,0xd9b751e2b8db8914,		    CommandAllowDamageEventsForNonNetworkedObjects);

	SCR_REGISTER_SECURE(SET_CUTSCENES_WEAPON_FLASHLIGHT_ON_THIS_FRAME,0x0e4a297378aba0e1,			CommandSetCutscenesWeaponFlashlightOnThisFrame);

// Rayfire commands
	SCR_REGISTER_SECURE(GET_RAYFIRE_MAP_OBJECT,0x6b3ee0ca145e8488,									CommandGetCompositeEntityAt);
	SCR_REGISTER_SECURE(SET_STATE_OF_RAYFIRE_MAP_OBJECT,0x288f018bfd3d51ed,							CommandSetCompositeEntityState);
	SCR_REGISTER_SECURE(GET_STATE_OF_RAYFIRE_MAP_OBJECT,0xd17b3afa40998041,							CommandGetCompositeEntityState);
	SCR_REGISTER_UNUSED(PAUSE_RAYFIRE_MAP_OBJECT_AT_PHASE,0x29874e719d2e59e7,						CommandPauseCompositeEntityAtPhase);
	SCR_REGISTER_SECURE(DOES_RAYFIRE_MAP_OBJECT_EXIST,0xf0254dadd9d91fb8,							CommandDoesRayFireMapObjectExist); 
	SCR_REGISTER_SECURE(GET_RAYFIRE_MAP_OBJECT_ANIM_PHASE,0xb0431da36e2aa134,						CommandGetCompEntityAnimationPhase);
// pickup commands
	SCR_REGISTER_SECURE(CREATE_PICKUP,0x1cd347d2bd906560,											CommandCreatePickup							  );	
	SCR_REGISTER_SECURE(CREATE_PICKUP_ROTATE,0x1ee09f99fef19daf,									CommandCreatePickupRotate					  );
	SCR_REGISTER_SECURE(FORCE_PICKUP_ROTATE_FACE_UP,0x520df2d45632ce77,								CommandForcePickupRotateFaceUp				  );
	SCR_REGISTER_SECURE(SET_CUSTOM_PICKUP_WEAPON_HASH,0x764344c3b3372eda,							CommandSetCustomPickupWeaponHash			  );
	SCR_REGISTER_SECURE(CREATE_AMBIENT_PICKUP,0xb765858a7a410073,									CommandCreateAmbientPickup					  );	
	SCR_REGISTER_SECURE(CREATE_NON_NETWORKED_AMBIENT_PICKUP,0xc9b8424fe1c703cf,						CommandCreateNonNetworkedAmbientPickup		  );
	SCR_REGISTER_UNUSED(BLOCK_PLAYERS_FOR_PICKUP,0x895af49b57426c0f,								CommandBlockPlayersForPickup				  );
	SCR_REGISTER_SECURE(BLOCK_PLAYERS_FOR_AMBIENT_PICKUP,0x03f97d082c4210a5,						CommandBlockPlayersForAmbientPickup			  );
	SCR_REGISTER_SECURE(CREATE_PORTABLE_PICKUP,0x9431d28bfc30340b,									CommandCreatePortablePickup					  );	
	SCR_REGISTER_SECURE(CREATE_NON_NETWORKED_PORTABLE_PICKUP,0xa22803867109bfb0,					CommandCreateNonNetworkedPortablePickup		  );
	SCR_REGISTER_SECURE(ATTACH_PORTABLE_PICKUP_TO_PED,0xa4dc05dfb08a8957,							CommandAttachPortablePickupToPed			  );	
	SCR_REGISTER_SECURE(DETACH_PORTABLE_PICKUP_FROM_PED,0x1c9488ec3f23b199,							CommandDetachPortablePickupFromPed			  );
	SCR_REGISTER_SECURE(FORCE_PORTABLE_PICKUP_LAST_ACCESSIBLE_POSITION_SETTING, 0x5CE2E45A5CE2E45A, CommandForcePortablePickupLastAccessiblePositionSetting);
	SCR_REGISTER_SECURE(HIDE_PORTABLE_PICKUP_WHEN_DETACHED,0xd649c4fd90f64861,						CommandHidePortablePickupWhenDetached		  );	
	SCR_REGISTER_SECURE(SET_MAX_NUM_PORTABLE_PICKUPS_CARRIED_BY_PLAYER,0x9af1d0c5f270c910,			CommandSetMaxNumPortablePickupsCarriedByPlayer);	
	SCR_REGISTER_SECURE(SET_LOCAL_PLAYER_CAN_COLLECT_PORTABLE_PICKUPS,0x91fbfe715b78280c,			CommandSetLocalPlayerCanCollectPortablePickups);	
	SCR_REGISTER_SECURE(GET_SAFE_PICKUP_COORDS,0x7f65c587e17def5d,									CommandGetSafePickupCoords					  );
	SCR_REGISTER_SECURE(ADD_EXTENDED_PICKUP_PROBE_AREA,0x28ee006afec511d9,							CommandAddExtendedPickupProbeArea			  );
	SCR_REGISTER_SECURE(CLEAR_EXTENDED_PICKUP_PROBE_AREAS,0x7a888ab940b876e8,						CommandClearExtendedPickupProbeAreas		  );
	SCR_REGISTER_SECURE(GET_PICKUP_COORDS,0x70453f7df40780a5,										CommandGetPickupCoords						  );		
	SCR_REGISTER_SECURE(SUPPRESS_PICKUP_SOUND_FOR_PICKUP,0xb409e96217e8aa92,						CommandSuppressPickupSoundForPickup			  );
	SCR_REGISTER_SECURE(REMOVE_ALL_PICKUPS_OF_TYPE,0xf5341a080595c3f3,								CommandRemoveAllPickupsOfType				  );
	SCR_REGISTER_UNUSED(GET_NUMBER_OF_PICKUPS_OF_TYPE,0xb54b23d6eafb5fcf,							CommandGetNumberOfPickupsOfType				  );
	SCR_REGISTER_SECURE(HAS_PICKUP_BEEN_COLLECTED,0x803592456ae579b3,								CommandHasPickupBeenCollected				  );		
	SCR_REGISTER_SECURE(REMOVE_PICKUP,0x451f33099166532a,											CommandRemovePickup							  );		
	SCR_REGISTER_SECURE(CREATE_MONEY_PICKUPS,0xae315b17ac08c2a9,									CommandCreateMoneyPickups					  );
	SCR_REGISTER_UNUSED(SET_PLAYERS_DROP_MONEY_IN_NETWORK_GAME, 0x806204b,							CommandSetPlayersDropMoneyInNetworkGame		  );
	SCR_REGISTER_SECURE(DOES_PICKUP_EXIST,0x2791155fb0769fe5,										CommandDoesPickupExist						  );		
	SCR_REGISTER_SECURE(DOES_PICKUP_OBJECT_EXIST,0x5f0e7de3bfa7690c,								CommandDoesPickupObjectExist				  );	
	SCR_REGISTER_SECURE(GET_PICKUP_OBJECT,0x7af1def5f183c1a6,										CommandGetPickupObject						  );	
	SCR_REGISTER_SECURE(IS_OBJECT_A_PICKUP,0xccebb0c0198fecdb,										CommandIsObjectAPickup						  );		
	SCR_REGISTER_SECURE(IS_OBJECT_A_PORTABLE_PICKUP,0x9f781807386ac867,								CommandIsObjectAPortablePickup				  );				
	SCR_REGISTER_SECURE(DOES_PICKUP_OF_TYPE_EXIST_IN_AREA,0xe4e0f69f436c8ff8,						CommandDoesPickupOfTypeExistInArea			  );		
	SCR_REGISTER_SECURE(SET_PICKUP_REGENERATION_TIME,0x69df462e3146f76e,							CommandSetPickupRegenerationTime			  );	
	SCR_REGISTER_SECURE(FORCE_PICKUP_REGENERATE,0x482dcf58b0c559a4,									CommandForcePickupRegenerate				  );
	SCR_REGISTER_SECURE(SET_PLAYER_PERMITTED_TO_COLLECT_PICKUPS_OF_TYPE,0xcbd9f5754fd640bf,			CommandSetPlayerPermittedToCollectPickupsOfType);	
	SCR_REGISTER_SECURE(SET_LOCAL_PLAYER_PERMITTED_TO_COLLECT_PICKUPS_WITH_MODEL,0xd985cd4e4d28a723, CommandSetLocalPlayerPermittedToCollectPickupsWithModel);
	SCR_REGISTER_SECURE(ALLOW_ALL_PLAYERS_TO_COLLECT_PICKUPS_OF_TYPE,0xc4f1303f19df6af2,			CommandAllowAllPlayersToCollectPickupsOfType  );	
	SCR_REGISTER_UNUSED(SET_TEAM_PICKUP,0x0c480c11c7e0c37a,											CommandSetTeamPickup						  );
	SCR_REGISTER_SECURE(SET_TEAM_PICKUP_OBJECT,0x0548bdefcaa1caa7,									CommandSetTeamPickupObject					  );	
	SCR_REGISTER_SECURE(PREVENT_COLLECTION_OF_PORTABLE_PICKUP,0x8175b06ed630a82a,					CommandPreventCollectionOfPortablePickup	  );	
	SCR_REGISTER_SECURE(SET_PICKUP_OBJECT_GLOW_WHEN_UNCOLLECTABLE,0x0e1b3b43a16940eb,				CommandSetPickupObjectGlowWhenUncollectable	  );
	SCR_REGISTER_SECURE(SET_PICKUP_GLOW_OFFSET,0x6f57da45338c5293,									CommandSetPickupGlowOffset					  );
	SCR_REGISTER_SECURE(SET_PICKUP_OBJECT_GLOW_OFFSET,0x1092ed0cc7e5a2f5,							CommandSetPickupObjectGlowOffset			  );
	SCR_REGISTER_UNUSED(SET_PICKUP_GLOW_IN_SAME_TEAM,0x269098405297dfe9,							CommandSetPickupGlowForSameTeam				  );
	SCR_REGISTER_UNUSED(SET_PICKUP_OBJECT_GLOW_IN_SAME_TEAM,0x1cedd008a678bd02,						CommandSetPickupObjectGlowForSameTeam		  );
	SCR_REGISTER_SECURE(SET_OBJECT_GLOW_IN_SAME_TEAM,0xc10f6115159d2b04,							CommandSetObjectGlowForSameTeam			      );
	SCR_REGISTER_SECURE(SET_PICKUP_OBJECT_ARROW_MARKER,0xfd3eb69ea4ce479b,							CommandSetPickupObjectArrowMarker			  );
	SCR_REGISTER_SECURE(ALLOW_PICKUP_ARROW_MARKER_WHEN_UNCOLLECTABLE,0xd9b42564a7d9daa8,			CommandAllowPickupObjectArrowMarkerWhenUncollectable);
	SCR_REGISTER_SECURE(GET_DEFAULT_AMMO_FOR_WEAPON_PICKUP,0x675e742c60ddd638,						CommandGetDefaultAmmoForWeaponPickup		  );	
	SCR_REGISTER_UNUSED(SET_VEHICLE_WEAPON_PICKUPS_SHARED_BY_PASSENGERS,0x41923cbe924491d9,			CommandSetVehicleWeaponPickupsSharedByPassengers);	
	SCR_REGISTER_SECURE(SET_PICKUP_GENERATION_RANGE_MULTIPLIER,0x296fb914127b2d58,					CommandSetPickupGenerationRangeMultiplier	  );	
	SCR_REGISTER_SECURE(GET_PICKUP_GENERATION_RANGE_MULTIPLIER,0xea9410ebed10ae7c,					CommandGetPickupGenerationRangeMultiplier	  );	
	SCR_REGISTER_SECURE(SET_ONLY_ALLOW_AMMO_COLLECTION_WHEN_LOW,0x79c07e0ee199af10,					CommandSetOnlyAllowAmmoCollectionWhenLow	  );	
	SCR_REGISTER_SECURE(SET_PICKUP_UNCOLLECTABLE,0xac74e3ca074d8dbd,								CommandSetPickupUncollectable				  );	
	SCR_REGISTER_SECURE(SET_PICKUP_TRANSPARENT_WHEN_UNCOLLECTABLE,0xdd5baaae5696f80d,				CommandSetPickupTransparentWhenUncollectable  );	
	SCR_REGISTER_SECURE(SET_PICKUP_HIDDEN_WHEN_UNCOLLECTABLE,0xa46cb4ea8156117b,					CommandSetPickupHiddenWhenUncollectable		  );	
	SCR_REGISTER_SECURE(SET_PICKUP_OBJECT_TRANSPARENT_WHEN_UNCOLLECTABLE,0x678cc86c4633cc3f,		CommandSetPickupObjectTransparentWhenUncollectable);	
	SCR_REGISTER_SECURE(SET_PICKUP_OBJECT_ALPHA_WHEN_TRANSPARENT,0x6155572d912b46d6,				CommandSetPickupObjectAlphaWhenTransparent	  );	
	SCR_REGISTER_SECURE(SET_PORTABLE_PICKUP_PERSIST,0x43da905252ee4ca1,								CommandSetPortablePickupPersist				  );	
	SCR_REGISTER_SECURE(ALLOW_PORTABLE_PICKUP_TO_MIGRATE_TO_NON_PARTICIPANTS,0x41db71dce7894fab,	CommandAllowPortablePickupToMigrateToNonParticipants);	
	SCR_REGISTER_SECURE(FORCE_ACTIVATE_PHYSICS_ON_UNFIXED_PICKUP,0x189ab741d56f49b9,				CommandForceActivatePhysicsOnUnfixedPickup    );	// Are you sure this is what you want? Not FORCE_ACTIVATE_PHYSICS_ON_DROPPED_PICKUP_NEAR_SUBMARINE?
	SCR_REGISTER_UNUSED(FORCE_ACTIVATE_PHYSICS_ON_DROPPED_PICKUP_NEAR_SUBMARINE,0xf46083d431705b1c,CommandForceActivatePhysicsOnDroppedPickupNearSubmarine);
	SCR_REGISTER_SECURE(ALLOW_PICKUP_BY_NONE_PARTICIPANT,0xc6e2d6b7217f48ac,						CommandAllowPickupByNonParticipant		      );

	SCR_REGISTER_UNUSED(IS_PORTABLE_PICKUP_CUSTOM_COLLIDER_READY,0x0a303c7e33325097,				CommandIsPortablePickupCustomColliderReady	  );
	SCR_REGISTER_UNUSED(IS_PICKUP_REWARD_TYPE_SUPPRESSED,0xba9733cfcaafa2dc,						CommandIsPickupRewardTypeSuppressed			  );
	SCR_REGISTER_SECURE(SUPPRESS_PICKUP_REWARD_TYPE,0xdef665962974b74c,								CommandSuppressPickupRewardType				  );
	SCR_REGISTER_SECURE(CLEAR_ALL_PICKUP_REWARD_TYPE_SUPPRESSION,0x402458806232a271,				CommandClearAllPickupRewardTypeSuppression	  );
	SCR_REGISTER_SECURE(CLEAR_PICKUP_REWARD_TYPE_SUPPRESSION,0x261c0495ce1149be,					CommandClearPickupRewardTypeSuppression		  );
																																				  
	SCR_REGISTER_SECURE(RENDER_FAKE_PICKUP_GLOW,0x8c6bd716fdd7dfb2,									CommandRenderFakePickup						  );

	SCR_REGISTER_SECURE(SET_PICKUP_OBJECT_COLLECTABLE_IN_VEHICLE,0x07106eb9ee13466d,				CommandSetPickupObjectCollectableInVehicle	  );
	SCR_REGISTER_SECURE(SET_PICKUP_TRACK_DAMAGE_EVENTS,0xfd2d2d41983bb472,							CommandSetPickupTrackDamageEvents			  );

// entity flag commands
	SCR_REGISTER_SECURE(SET_ENTITY_FLAG_SUPPRESS_SHADOW,0xb43a82419553a1e8,							CommandSetEntityFlag_SUPPRESS_SHADOW		  );
	SCR_REGISTER_SECURE(SET_ENTITY_FLAG_RENDER_SMALL_SHADOW,0x01a9c9579974823f,						CommandSetEntityFlag_RENDER_SMALL_SHADOW	  );

	SCR_REGISTER_SECURE(GET_WEAPON_TYPE_FROM_PICKUP_TYPE,0xc6b776ed315b2193,						CommandGetWeaponTypeFromPickupType			  );		  
	SCR_REGISTER_SECURE(GET_PICKUP_TYPE_FROM_WEAPON_HASH,0xfc53f42ae5bd9601,						CommandGetPickupTypeFromWeaponHash			  );		  
	SCR_REGISTER_SECURE(IS_PICKUP_WEAPON_OBJECT_VALID,0xa64dbd522133c8f1,							CommandIsPickupWeaponObjectValid			  );		  

	SCR_REGISTER_SECURE(GET_OBJECT_TINT_INDEX,0x873082e7f40db44c,									CommandGetObjectTintIndex					  );					  
	SCR_REGISTER_SECURE(SET_OBJECT_TINT_INDEX,0x71b42d724f2db164,									CommandSetObjectTintIndex					  );					  
	SCR_REGISTER_UNUSED(GET_TINT_INDEX_CLOSEST_BUILDING_OF_TYPE,0x860f1ceecefadac0,					CommandGetTintIndexClosestBuildingOfType	  );
	SCR_REGISTER_SECURE(SET_TINT_INDEX_CLOSEST_BUILDING_OF_TYPE,0xa56ad7cec1ae4c83,					CommandSetTintIndexClosestBuildingOfType	  );
	SCR_REGISTER_UNUSED(GET_PROP_TINT_INDEX,0xd2647cb2bf6f11c0,										CommandGetPropTintIndex						  );
	SCR_REGISTER_SECURE(SET_PROP_TINT_INDEX,0x5352da2e8f114fa9,										CommandSetPropTintIndex						  );

	SCR_REGISTER_SECURE(SET_PROP_LIGHT_COLOR,0x0aeba112bfb0eeb0,							CommandSetPropLightColor);
	SCR_REGISTER_SECURE(IS_PROP_LIGHT_OVERRIDEN,0x384da7361ab033f6,							CommandIsPropLightColorOverriden);
	SCR_REGISTER_UNUSED(GET_PROP_LIGHT_COLOR_RED,0x0cf5873c6c5099f2,							CommandGetPropLightColorRed);
	SCR_REGISTER_UNUSED(GET_PROP_LIGHT_COLOR_GREEN,0x11311794f44f4271,							CommandGetPropLightColorGreen);
	SCR_REGISTER_UNUSED(GET_PROP_LIGHT_COLOR_BLUE,0x7543963afdb844f4,							CommandGetPropLightColorBlue);
	SCR_REGISTER_UNUSED(GET_PROP_LIGHT_ORIG_COLOR_RED,0x54a58e7404dec833,							CommandGetPropLightOrigColorRed);
	SCR_REGISTER_UNUSED(GET_PROP_LIGHT_ORIG_COLOR_GREEN,0x7ab3d3e47cf58566,							CommandGetPropLightOrigColorGreen);
	SCR_REGISTER_UNUSED(GET_PROP_LIGHT_ORIG_COLOR_BLUE,0xb4f12b45661b34b6,							CommandGetPropLightOrigColorBlue);


	SCR_REGISTER_SECURE(SET_OBJECT_IS_VISIBLE_IN_MIRRORS,0x9bceec68518a0848,						CommandSetIsVisibleInMirrors				  );

	SCR_REGISTER_SECURE(SET_OBJECT_SPEED_BOOST_AMOUNT,0xe67d071d390bd9d9,							CommandSetObjectSpeedBoostAmount			  );
	SCR_REGISTER_SECURE(SET_OBJECT_SPEED_BOOST_DURATION,0x9b7ee172fae35099,							CommandSetObjectSpeedBoostDuration			  );

	// temporary
	SCR_REGISTER_SECURE(CONVERT_OLD_PICKUP_TYPE_TO_NEW,0x9b788b7cb1c92c62,							ConvertOldPickupTypeToHash);

	SCR_REGISTER_SECURE(SET_FORCE_OBJECT_THIS_FRAME,0x8e21c2b8d76bd2c7,								CommandSetForceObjectThisFrame);

	SCR_REGISTER_SECURE(ONLY_CLEAN_UP_OBJECT_WHEN_OUT_OF_RANGE,0x13a78fce42e3b132,					CommandOnlyCleanUpObjectWhenOutOfRange);

    SCR_REGISTER_UNUSED(DOES_OBJECT_EXIST_WITH_DECORATOR,0xc637d97e669fafac,                        CommandDoesObjectExistWithDecorator);

	SCR_REGISTER_SECURE(SET_DISABLE_COLLISIONS_BETWEEN_CARS_AND_CAR_PARACHUTE,0xe4998dcce92adeb9,	CommandSetDisableCarCollisionsWithCarParachutes);

    SCR_REGISTER_SECURE(SET_PROJECTILES_SHOULD_EXPLODE_ON_CONTACT,0x73c56b3110244a3b,				CommandSetProjectilesShouldExplodeWhenHitting);

	SCR_REGISTER_SECURE( SET_DRIVE_ARTICULATED_JOINT,0xfe483aa70caa8182,				            CommandSetDriveArticulatedJoint );
	SCR_REGISTER_SECURE( SET_DRIVE_ARTICULATED_JOINT_WITH_INFLICTOR,0xcb795d4409baec14,			CommandSetDriveArticulatedJointWithInflictor );
	SCR_REGISTER_SECURE( SET_OBJECT_IS_A_PRESSURE_PLATE,0xe04446da0adbbeab,			            CommandSetObjectIsAPressurePlate );
	SCR_REGISTER_UNUSED( GET_IS_PRESSURE_PLATE_PRESSED,0x5b2c00195589b9f9,				            CommandGetIsPressurePlatePressed );
	SCR_REGISTER_SECURE( SET_WEAPON_IMPACTS_APPLY_GREATER_FORCE,0xd278f4541b205f55,                CommandSetWeaponImpactsApplyGreaterForce );
	SCR_REGISTER_SECURE( GET_IS_ARTICULATED_JOINT_AT_MIN_ANGLE,0xe90697e735a0e9fe,                 CommandGetIsArticulatedJointAtMinAngle );
    SCR_REGISTER_SECURE(GET_IS_ARTICULATED_JOINT_AT_MAX_ANGLE,0xec2ec59776098a83,                  CommandGetIsArticulatedJointAtMaxAngle);
	SCR_REGISTER_SECURE( SET_IS_OBJECT_ARTICULATED,0x447fadeab88b0db1,                             CommandSetObjectIsArticulated );
	SCR_REGISTER_SECURE( SET_IS_OBJECT_BALL,0x70c27579df64816f, CommandSetObjectIsBall );
    SCR_REGISTER_UNUSED( SET_OBJECT_POPS_TYRES,0x46ed143d558e9590, CommandSetObjectPopsTyres );

	SCR_REGISTER_UNUSED( SET_OBJECT_ALBEDO_FLAG,0x0d7c43a3a5850b32,								CommandSetObjectAlbedoFlag);
	SCR_REGISTER_UNUSED( GET_OBJECT_ALBEDO_FLAG,0xcfc676c358a840ff,								CommandGetObjectAlbedoFlag);
	SCR_REGISTER_UNUSED( ENABLE_GLOBAL_ALBEDO_OBJECTS_SUPPORT,0xe8c6a8e6a6e0bd3b,				CommandEnableGlobalAlbedoObjectsSupport);
	
}
}	//	end of namespace object_commands
