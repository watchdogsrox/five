#include "script\handlers\GameScriptResources.h"

//game includes
#include "ai/BlockingBounds.h"
#include "camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "camera/scripted/ScriptDirector.h"
#include "cutscene/CutSceneManagerNew.h"
#include "control/record.h"
#include "event/decision/EventDecisionMakerManager.h"
#include "frontend/MiniMap.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormStore.h"
#include "game/Dispatch/DispatchHelpers.h"
#include "game/Dispatch/DispatchManager.h"
#include "game/Dispatch/DispatchServices.h"
#include "modelinfo/MloModelInfo.h"
#include "pedgroup/PedGroup.h"
#include "peds/ped.h"
#include "peds/pedpopulation.h"
#include "peds/PopCycle.h"
#include "peds/Relationships.h"
#include "Peds/PedScriptResource.h"
#include "Peds/VehicleCombatAvoidanceArea.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/PickupPlacement.h"
#include "scene/volume/VolumeManager.h"
#include "script/script_debug.h"
#include "script/script_helper.h"
#include "script/script_hud.h"
#include "script/script_itemsets.h"
#include "script/streamedscripts.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "task/General/TaskBasic.h"
#include "task/Default/Patrol/PatrolRoutes.h"
#include "vehicleAi/roadspeedzone.h"
#include "vfx/misc/Checkpoints.h"
#include "vfx/misc/Fire.h"
#include "Vfx/Misc/MovieManager.h"
#include "Vfx/Misc/MovieMeshManager.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxscript.h"
#include "vfx/systems/vfxscript.h"
#include "weapons/weaponscriptresource.h"
#include "vehicles/metadata/vehiclescriptresource.h"

// Framework headers
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwanimation/anim_channel.h"
#include "fwscript/scriptguid.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "vfx/channel.h"

//rage includes
#include "fwanimation/animmanager.h"
#include "fwpheffects/ropemanager.h"
#include "fwscript/scriptguid.h"
#include "grcore/debugdraw.h"
#include "string/stringhash.h"
#include "vectormath/legacyconvert.h"

SCRIPT_OPTIMISATIONS()

FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CGameScriptResource, CONFIGURED_FROM_FILE, 0.47f, atHashString("CGameScriptResource",0x8cb13b20), CGameScriptResource::MAX_SIZE_OF_SCRIPT_RESOURCE);

CompileTimeAssert(CScriptResource_SequenceTask::SEQUENCE_RESOURCE_ID_SHIFT + CScriptResource_SequenceTask::SIZEOF_SEQUENCE_ID < (sizeof(ScriptResourceId)<<3));


#if __REPORT_TOO_MANY_RESOURCES
s32 CGameScriptResource::ms_MaximumExpectedNumberOfResources[SCRIPT_RESOURCE_MAX];
s32 CGameScriptResource::ms_CurrentNumberOfUsedResources[SCRIPT_RESOURCE_MAX];
s32 CGameScriptResource::ms_MaximumNumberOfResourcesSoFar[SCRIPT_RESOURCE_MAX];
#endif	//	__REPORT_TOO_MANY_RESOURCES

///////////////////////////////////////////
// CGameScriptResource
///////////////////////////////////////////

CGameScriptResource::CGameScriptResource(ScriptResourceType type, ScriptResourceRef reference) : scriptResource(type, reference)
{
#if __REPORT_TOO_MANY_RESOURCES
	IncrementNumberOfUsedResources();
#endif	//	__REPORT_TOO_MANY_RESOURCES
}

CGameScriptResource::~CGameScriptResource()
{
#if __REPORT_TOO_MANY_RESOURCES
	DecrementNumberOfUsedResources();
#endif	//	__REPORT_TOO_MANY_RESOURCES
}


#if __BANK
void CGameScriptResource::DisplayDebugInfo(const char* scriptName) const
{
	if (scriptName)
	{
		grcDebugDraw::AddDebugOutput("%s: %s %d", scriptName, GetResourceName(), m_Reference);
	}
	else
	{
		grcDebugDraw::AddDebugOutput("%s %d", GetResourceName(), m_Reference);
	}
}

void CGameScriptResource::SpewDebugInfo(const char* scriptName) const
{
	if (scriptName)
	{
		scriptDisplayf("%s: %s %d\n", scriptName, GetResourceName(), GetReference());
	}
	else
	{
		scriptDisplayf("%s %d\n", GetResourceName(), GetReference());
	}
}
#endif	//	__BANK


#if __SCRIPT_MEM_CALC
void CGameScriptResource::CalculateMemoryUsage(const char* UNUSED_PARAM(scriptName), atArray<strIndex>& UNUSED_PARAM(streamingIndices), const strIndex* UNUSED_PARAM(ignoreList), s32 UNUSED_PARAM(numIgnores), 
							 u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
							 bool UNUSED_PARAM(bDisplayScriptDetails), u32 UNUSED_PARAM(skipFlag)) const
{
	NonStreamingVirtualForThisResource = 0;
	NonStreamingPhysicalForThisResource = 0;
}
#endif	//	__SCRIPT_MEM_CALC



#if __REPORT_TOO_MANY_RESOURCES

#if __BANK
void CGameScriptResource::DisplayResourceUsage(bool bOutputToScreen)
{
	s32 usedTotal = 0;
	s32 maxSoFarTotal = 0;
	s32 maxExpectedTotal = 0;
	for (s32 resourceType = 0; resourceType < SCRIPT_RESOURCE_MAX; resourceType++)
	{
		usedTotal += ms_CurrentNumberOfUsedResources[resourceType];
		maxSoFarTotal += ms_MaximumNumberOfResourcesSoFar[resourceType];
		maxExpectedTotal += ms_MaximumExpectedNumberOfResources[resourceType];

		if (bOutputToScreen)
		{
			grcDebugDraw::AddDebugOutput("%s Current=%d MaxSoFar=%d MaxExpected=%d", GetName(resourceType), ms_CurrentNumberOfUsedResources[resourceType], ms_MaximumNumberOfResourcesSoFar[resourceType], ms_MaximumExpectedNumberOfResources[resourceType]);
		}
		else
		{
			scriptDisplayf("%s Current=%d MaxSoFar=%d MaxExpected=%d", GetName(resourceType), ms_CurrentNumberOfUsedResources[resourceType], ms_MaximumNumberOfResourcesSoFar[resourceType], ms_MaximumExpectedNumberOfResources[resourceType]);
		}
	}

	if (bOutputToScreen)
	{
		grcDebugDraw::AddDebugOutput("Total Current=%d MaxSoFar=%d MaxExpected=%d", usedTotal, maxSoFarTotal, maxExpectedTotal);
	}
	else
	{
		scriptDisplayf("Total Current=%d MaxSoFar=%d MaxExpected=%d", usedTotal, maxSoFarTotal, maxExpectedTotal);
	}
}
#endif	//	__BANK

void CGameScriptResource::IncrementNumberOfUsedResources()
{
	ms_CurrentNumberOfUsedResources[m_Type]++;

	if (ms_CurrentNumberOfUsedResources[m_Type] > ms_MaximumNumberOfResourcesSoFar[m_Type])
	{
		scriptDebugf1("CGameScriptResource::IncrementNumberOfUsedResources - New maximum for resource type %s = %d", GetName(m_Type), ms_CurrentNumberOfUsedResources[m_Type]);
		ms_MaximumNumberOfResourcesSoFar[m_Type] = ms_CurrentNumberOfUsedResources[m_Type];
	}

	if (ms_CurrentNumberOfUsedResources[m_Type] > ms_MaximumExpectedNumberOfResources[m_Type])
	{
		scriptDisplayf("CGameScriptResource::IncrementNumberOfUsedResources - Exceeded expected maximum for resource type %s = %d", GetName(m_Type), ms_CurrentNumberOfUsedResources[m_Type]);
#if __BANK
		static u32 s_uLastSpewTime[SCRIPT_RESOURCE_MAX] = {0};
		if(s_uLastSpewTime[m_Type] < fwTimer::GetTimeInMilliseconds())
		{
			CTheScripts::GetScriptHandlerMgr().SpewResourceInfoForType(m_Type);
			s_uLastSpewTime[m_Type] = fwTimer::GetTimeInMilliseconds() + 5000;
		}
#endif	//	__BANK
		scriptAssertf(0, "CGameScriptResource::IncrementNumberOfUsedResources - number of instances of resource type %s is now %d. The expected maximum number (read from gameconfig.xml) is %d - Graeme. Search the console log for the last line that says \"Exceeded expected maximum for resource type %s = \" In gameconfig.xml, update that entry. Also in gameconfig.xml, increment the size of the CGameScriptResource pool by the same amount.", GetName(m_Type), ms_CurrentNumberOfUsedResources[m_Type], ms_MaximumExpectedNumberOfResources[m_Type], GetName(m_Type));
	}
}

void CGameScriptResource::DecrementNumberOfUsedResources()
{
	if (scriptVerifyf(ms_CurrentNumberOfUsedResources[m_Type] > 0, "CGameScriptResource::DecrementNumberOfUsedResources - can't decrement ms_CurrentNumberOfUsedResources. It's already %d", ms_CurrentNumberOfUsedResources[m_Type]))
	{
		ms_CurrentNumberOfUsedResources[m_Type]--;
	}
}
#endif	//	__REPORT_TOO_MANY_RESOURCES


const char* CGameScriptResource::GetName(const int resourceType)
{
	switch (resourceType)
	{
	case SCRIPT_RESOURCE_PTFX:                return "PTFX";
	case SCRIPT_RESOURCE_PTFX_ASSET:          return "PTFX_ASSET";
	case SCRIPT_RESOURCE_FIRE:                return "FIRE";
	case SCRIPT_RESOURCE_PEDGROUP:            return "PEDGROUP";
	case SCRIPT_RESOURCE_SEQUENCE_TASK:       return "SEQUENCE_TASK";
	case SCRIPT_RESOURCE_DECISION_MAKER:      return "DECISION_MAKER";
	case SCRIPT_RESOURCE_CHECKPOINT:          return "CHECKPOINT";
	case SCRIPT_RESOURCE_TEXTURE_DICTIONARY:  return "TEXTURE_DICTIONARY";
	case SCRIPT_RESOURCE_DRAWABLE_DICTIONARY: return "DRAWABLE_DICTIONARY";
	case SCRIPT_RESOURCE_CLOTH_DICTIONARY:	  return "CLOTH_DICTIONARY";
	case SCRIPT_RESOURCE_FRAG_DICTIONARY:	  return "FRAG_DICTIONARY";
	case SCRIPT_RESOURCE_DRAWABLE:			  return "DRAWABLE";
	case SCRIPT_RESOURCE_COVERPOINT:          return "COVERPOINT";
	case SCRIPT_RESOURCE_ANIMATION:           return "ANIMATION";
	case SCRIPT_RESOURCE_MODEL:               return "MODEL";
	case SCRIPT_RESOURCE_RADAR_BLIP:          return "RADAR_BLIP";
	case SCRIPT_RESOURCE_ROPE:				  return "ROPE";
	case SCRIPT_RESOURCE_CAMERA:              return "CAMERA";
	case SCRIPT_RESOURCE_PATROL_ROUTE:        return "PATROL_ROUTE";
	case SCRIPT_RESOURCE_MLO:                 return "MLO";
	case SCRIPT_RESOURCE_RELATIONSHIP_GROUP:  return "RELATIONSHIP_GROUP";
	case SCRIPT_RESOURCE_SCALEFORM_MOVIE:	  return "SCALEFORM_MOVIE";
	case SCRIPT_RESOURCE_STREAMED_SCRIPT:	  return "STREAMED_SCRIPT";
	case SCRIPT_RESOURCE_ITEMSET:			  return "ITEM_SET";
	case SCRIPT_RESOURCE_VOLUME:              return "VOLUME";
	case SCRIPT_RESOURCE_SPEED_ZONE:		  return "SPEED_ZONE";
	case SCRIPT_RESOURCE_WEAPON_ASSET:        return "WEAPON_ASSET";
	case SCRIPT_RESOURCE_VEHICLE_ASSET :	  return "VEHICLE_ASSET";
	case SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE: return "POPSCHEDULE_OVERRIDE";
	case SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL : return "POPSCHEDULE_OVERRIDE_VEHICLE_MODEL";
	case SCRIPT_RESOURCE_SCENARIO_BLOCKING_AREA: return "SCENARIO_BLOCKING_AREA";
	case SCRIPT_RESOURCE_BINK_MOVIE:		  return "BINK_MOVIE";
	case SCRIPT_RESOURCE_MOVIE_MESH_SET:	  return "MOVIE_MESH_SET";
	case SCRIPT_RESOURCE_SET_REL_GROUP_DONT_AFFECT_WANTED_LEVEL : return "SET_REL_GROUP_DONT_AFFECT_WANTED_LEVEL";
	case SCRIPT_RESOURCE_VEHICLE_COMBAT_AVOIDANCE_AREA : return "VEHICLE_COMBAT_AVOIDANCE_AREA";
	case SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS : return "DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS";
	case SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER : return "DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER";
	case SCRIPT_RESOURCE_SYNCED_SCENE : return "SYNCED_SCENE";
	case SCRIPT_RESOURCE_VEHICLE_RECORDING : return "VEHICLE_RECORDING";
	case SCRIPT_RESOURCE_MOVEMENT_MODE_ASSET : return "MOVEMENT_MODE_ASSET";
	case SCRIPT_RESOURCE_CUT_SCENE : return "CUT_SCENE";
	case SCRIPT_RESOURCE_CUT_FILE	: return "CUT_FILE";
	case SCRIPT_RESOURCE_CLIP_SET:			return "CLIP_SET";
	case SCRIPT_RESOURCE_GHOST_SETTINGS:	return "GHOST_SETTINGS";
	case SCRIPT_RESOURCE_PICKUP_GENERATION_MULTIPLIER:	return "PICKUP_GENERATION_MULTIPLIER";
	}

	return "UNKNOWN";
}


void CGameScriptResource::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
#if __REPORT_TOO_MANY_RESOURCES
		scriptDisplayf("CGameScriptResource::Init - sizeof(CGameScriptResource) = %" SIZETFMT "d", sizeof(CGameScriptResource));
		s32 totalResources = 0;
		for (s32 resourceType = 0; resourceType < SCRIPT_RESOURCE_MAX; resourceType++)
		{
			strStreamingEngine::GetLoader().CallKeepAliveCallbackIfNecessary();
			ms_CurrentNumberOfUsedResources[resourceType] = 0;
			ms_MaximumExpectedNumberOfResources[resourceType] = CGameConfig::Get().GetConfigMaxNumberOfScriptResource(GetName(resourceType));
			scriptDisplayf("CGameScriptResource::Init - max number of instances of %s has been set as %d by gameconfig.xml", GetName(resourceType), ms_MaximumExpectedNumberOfResources[resourceType]);
			scriptAssertf(ms_MaximumExpectedNumberOfResources[resourceType] > 0, "CGameScriptResource::Init - max number of instances of %s is %d. Check that it has been set up in gameconfig.xml", GetName(resourceType), ms_MaximumExpectedNumberOfResources[resourceType]);
			totalResources += ms_MaximumExpectedNumberOfResources[resourceType];

			ms_MaximumNumberOfResourcesSoFar[resourceType] = 0;	//	ms_MaximumExpectedNumberOfResources[resourceType];
		}

		scriptDisplayf("CGameScriptResource::Init - sum of all resource type subtotals = %d. CGameScriptResource pool has size %d", totalResources, GetPool()->GetSize());

		//scriptAssertf(totalResources == GetPool()->GetSize(), "CGameScriptResource::Init - sum of all resource type subtotals %d doesn't match the size of the CGameScriptResource pool %d. Check gameconfig.xml", totalResources, GetPool()->GetSize());
#endif	//	__REPORT_TOO_MANY_RESOURCES
	}
}

void CGameScriptResource::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
#if __REPORT_TOO_MANY_RESOURCES
		for (s32 resourceType = 0; resourceType < SCRIPT_RESOURCE_MAX; resourceType++)
		{
			if (ms_CurrentNumberOfUsedResources[resourceType] > 0)
			{
				scriptDisplayf("CGameScriptResource::Shutdown - expected number of used resources for %s to be 0 when shutting down the session but it's %d", GetName(resourceType), ms_CurrentNumberOfUsedResources[resourceType]);
				scriptAssertf(0, "CGameScriptResource::Shutdown - expected number of used resources for %s to be 0 when shutting down the session but it's %d", GetName(resourceType), ms_CurrentNumberOfUsedResources[resourceType]);
				ms_CurrentNumberOfUsedResources[resourceType] = 0;
			}
		}
#endif	//	__REPORT_TOO_MANY_RESOURCES
	}
}



///////////////////////////////////////////
// StreamingResource
///////////////////////////////////////////
CScriptStreamingResource::CScriptStreamingResource(ScriptResourceType resourceType, strStreamingModule& streamingModule, u32 index, u32 streamingFlags)
: CGameScriptResource(resourceType, INVALID_STREAMING_REF)
{
	if (SCRIPT_VERIFY(index != -1, "Invalid streaming resource index"))
	{
		if (SCRIPT_VERIFY( CStreaming::IsObjectInImage(strLocalIndex(index), streamingModule.GetStreamingModuleId()), "Resource is not in the streaming module"))
		{
			s32 flags = STRFLAG_MISSION_REQUIRED|streamingFlags;
			CTheScripts::PrioritizeRequestsFromMissionScript(flags);
			streamingModule.StreamingRequest(strLocalIndex(index), flags);

			m_Reference = index;
		}
	}
}

void CScriptStreamingResource::Destroy()
{
	CStreaming::SetMissionDoesntRequireObject(strLocalIndex(m_Reference), GetStreamingModule().GetStreamingModuleId());
}

strIndex CScriptStreamingResource::GetStreamingIndex() const
{
	if (scriptVerify(m_Reference != GetInvalidReference()))
	{
		return GetStreamingModule().GetStreamingIndex(strLocalIndex(m_Reference));
	}

	return strIndex(strIndex::INVALID_INDEX);
}


#if __SCRIPT_MEM_CALC
void CScriptStreamingResource::CalculateMemoryUsage(const char* SCRIPT_MEM_DISPLAY_ONLY(scriptName), atArray<strIndex>& streamingIndices, 
							const strIndex* SCRIPT_MEM_DISPLAY_ONLY(ignoreList), s32 SCRIPT_MEM_DISPLAY_ONLY(numIgnores), 
							u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
							bool SCRIPT_MEM_DISPLAY_ONLY(bDisplayScriptDetails), u32 skipFlag) const
{
	NonStreamingVirtualForThisResource = 0;
	NonStreamingPhysicalForThisResource = 0;

	strIndex streamingIndex = GetStreamingIndex();

	if(streamingIndex != strIndex(strIndex::INVALID_INDEX) && !(strStreamingEngine::GetInfo().GetObjectFlags(streamingIndex) & skipFlag))
	{
		streamingIndices.PushAndGrow(streamingIndex);

#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			u32 virtualSize = 0;
			u32 physicalSize = 0;
			strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndex, virtualSize, physicalSize, ignoreList, numIgnores, true);

			grcDebugDraw::AddDebugOutput("%s %s %s %dK %dK", 
				scriptName, 
				GetStreamingModule().GetName(strLocalIndex(m_Reference)),
				GetResourceName(),
				virtualSize>>10, physicalSize>>10);
		}
#endif	//	__SCRIPT_MEM_DISPLAY
	}
}
#endif	//	__SCRIPT_MEM_CALC


#if __DEV
void CScriptStreamingResource::DisplayDebugInfo(const char* scriptName) const
{
	grcDebugDraw::AddDebugOutput("%s: %s %d (%s, %s)", scriptName, GetResourceName(), m_Reference, GetStreamingModule().GetModuleName(), GetStreamingModule().GetName(strLocalIndex(m_Reference)));
}

void CScriptStreamingResource::SpewDebugInfo(const char* scriptName) const
{
	Displayf("%s: %s %d (%s, %s)\n", scriptName, GetResourceName(), GetReference(), GetStreamingModule().GetModuleName(), GetStreamingModule().GetName(strLocalIndex(m_Reference)));
}
#endif	//	__DEV


///////////////////////////////////////////
// SCRIPT_RESOURCE_PTFX
///////////////////////////////////////////
CScriptResource_PTFX::CScriptResource_PTFX(const char* pFxName, atHashWithStringNotFinal usePtFxAssetHashName, CEntity* pParentEntity, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	// don't allow attachment to parent entities without a drawable
	if (!pParentEntity || pParentEntity->GetDrawable())
	{
		const char* pScriptName = CTheScripts::GetCurrentScriptName();
		atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);
		if (usePtFxAssetHashName!=0)
		{
			ptfxAssetName = usePtFxAssetHashName;
		}

		if (ptfxVerifyf(ptfxAssetName.GetHash(), "script is trying to play a particle effect but no particle asset is set up"))
		{
			m_Reference = ptfxScript::Start(atHashWithStringNotFinal(pFxName), ptfxAssetName, pParentEntity, boneTag, RC_VEC3V(const_cast<Vector3&>(fxPos)), RC_VEC3V(const_cast<Vector3&>(fxRot)), scale, invertAxes);
		}
	}
	else
	{
		ptfxAssertf(0, "failed to start scripted effect (%s) due to it being attached to an entity (%s) without a drawable", pFxName, pParentEntity->GetDebugName());
	}
}

void CScriptResource_PTFX::Destroy()
{
	ptfxScript::Destroy(m_Reference);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_PTFX_ASSET
///////////////////////////////////////////
CScriptResource_PTFX_Asset::CScriptResource_PTFX_Asset(u32 index, u32 streamingFlags)
: CScriptStreamingResource(RESOURCE_TYPE, ptfxManager::GetAssetStore(), index, streamingFlags)
{
}

strStreamingModule& CScriptResource_PTFX_Asset::GetStreamingModule() const
{
	return ptfxManager::GetAssetStore();
}


///////////////////////////////////////////
// SCRIPT_RESOURCE_FIRE
///////////////////////////////////////////
CScriptResource_Fire::CScriptResource_Fire(const Vector3& VecPos, int numGenerations, bool isPetrolFire)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = g_fireMan.StartScriptFire(RCC_VEC3V(VecPos), NULL, 0.0f, 1.0f, numGenerations, isPetrolFire);
}

void CScriptResource_Fire::Destroy()
{
	g_fireMan.RemoveScriptFire(m_Reference);
}

void CScriptResource_Fire::Detach()
{
	g_fireMan.ClearScriptFireFlag(m_Reference);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_VOLUME
///////////////////////////////////////////
#if VOLUME_SUPPORT
CScriptResource_Volume::CScriptResource_Volume(CVolume* pVolume)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
    if (pVolume)
    {
        m_Reference = fwScriptGuid::CreateGuid((*pVolume));
    }
}

void CScriptResource_Volume::Destroy()
{
    CVolume *pVolume = fwScriptGuid::FromGuid<CVolume>(m_Reference);
    if (pVolume)
    {
        g_VolumeManager.DeleteVolume(pVolume);
    }
}
#endif // VOLUME_SUPPORT


///////////////////////////////////////////
// SCRIPT_RESOURCE_PEDGROUP
///////////////////////////////////////////
CScriptResource_PedGroup::CScriptResource_PedGroup(CGameScriptHandler* pScriptHandler)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = CPedGroups::AddGroup(POPTYPE_MISSION, pScriptHandler);

	SCRIPT_ASSERT(m_Reference>=0, "No free groups left");
}

CScriptResource_PedGroup::CScriptResource_PedGroup(u32 existingGroupSlot)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = existingGroupSlot;
}

void CScriptResource_PedGroup::Destroy()
{
	if (CPedGroups::ms_groups[m_Reference].PopTypeGet() != POPTYPE_PERMANENT)
	{
		CPedGroups::RemoveGroupFromScript(m_Reference);
	}
	else
	{
		// reset the formation, in case it was adjusted
		if (CPedGroups::ms_groups[m_Reference].IsLocallyControlled())
		{
			CPedFormation * pFormation = CPedGroup::CreateDefaultFormation();
			CPedGroups::ms_groups[m_Reference].SetFormation(pFormation);
		}
	}
}

bool CScriptResource_PedGroup::LeaveForOtherScripts() const
{
	// permanent groups (eg player groups) can have their formation modified and so this must be left until there are no scripts
	// referencing the group. Then the formation will be reset to the default formation.
	return (CPedGroups::ms_groups[m_Reference].PopTypeGet() == POPTYPE_PERMANENT);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_SEQUENCE_TASK
///////////////////////////////////////////
CScriptResource_SequenceTask::CScriptResource_SequenceTask()
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	if (SCRIPT_VERIFY(-1==CTaskSequences::ms_iActiveSequence, "There is already a sequence open"))
	{
		m_Reference = CTaskSequences::GetAvailableSlot(true);

		if (SCRIPT_VERIFY(m_Reference>=0, "No free sequences left."))
		{
			CTaskSequences::ms_bIsOpened[m_Reference]=true;
			CTaskSequences::ms_TaskSequenceLists[m_Reference].Flush();
			CTaskSequences::ms_TaskSequenceLists[m_Reference].Register();
			CTaskSequences::ms_iActiveSequence=m_Reference;
		}
	}
}

void CScriptResource_SequenceTask::Destroy()
{
	if(SCRIPT_VERIFY(!CTaskSequences::ms_bIsOpened[m_Reference], "Sequence task is still open"))
	{
		CTaskSequences::ms_TaskSequenceLists[m_Reference].DeRegister();
	}
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_DECISION_MAKER
///////////////////////////////////////////
CScriptResource_DecisionMaker::CScriptResource_DecisionMaker(s32 iSourceDecisionMakerId, const char* pNewName)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	CEventDecisionMaker* pCopy = CEventDecisionMakerManager::CloneDecisionMaker(iSourceDecisionMakerId, pNewName);

	if(SCRIPT_VERIFY_TWO_STRINGS(pCopy, "Failed to create new decision maker [%s]", pNewName))
	{	
		m_Reference = pCopy->GetId();
	}
}

void CScriptResource_DecisionMaker::Destroy()
{
	CEventDecisionMakerManager::DeleteDecisionMaker(m_Reference);
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_CHECKPOINT
///////////////////////////////////////////
CScriptResource_Checkpoint::CScriptResource_Checkpoint(int CheckpointType, const Vector3& vecPosition, const Vector3& vecPointAt, float fSize, int colR, int colG, int colB, int colA, int num)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = g_checkpoints.Add((CheckpointType_e)CheckpointType, RCC_VEC3V(vecPosition), RCC_VEC3V(vecPointAt), fSize, 
		static_cast<u8>(colR), static_cast<u8>(colG), static_cast<u8>(colB), static_cast<u8>(colA), static_cast<u8>(num));
}

void CScriptResource_Checkpoint::Destroy()
{
	g_checkpoints.Delete(m_Reference);
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_TEXTURE_DICTIONARY
///////////////////////////////////////////
CScriptResource_TextureDictionary::CScriptResource_TextureDictionary(strLocalIndex index, u32 streamingFlags)
: CScriptStreamingResource(RESOURCE_TYPE, *g_TxdStore.GetStreamingModule(), index.Get(), streamingFlags)
{
}

strStreamingModule& CScriptResource_TextureDictionary::GetStreamingModule() const
{
	return *g_TxdStore.GetStreamingModule();
}

void CScriptResource_TextureDictionary::Destroy()
{
	// We need to add a reference for this texture dictionary to prevent it from being streamed out if it is still being rendered.
	if (g_TxdStore.HasObjectLoaded(strLocalIndex(m_Reference))){
		gDrawListMgr->AddTxdReference(m_Reference);
	}

	CScriptStreamingResource::Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_DRAWABLE_DICTIONARY
///////////////////////////////////////////
CScriptResource_DrawableDictionary::CScriptResource_DrawableDictionary(strLocalIndex index, u32 streamingFlags)
: CScriptStreamingResource(RESOURCE_TYPE, *g_DwdStore.GetStreamingModule(), index.Get(), streamingFlags)
{
}

strStreamingModule& CScriptResource_DrawableDictionary::GetStreamingModule() const
{
	return *g_DwdStore.GetStreamingModule();
}

void CScriptResource_DrawableDictionary::Destroy()
{
	// We need to add a reference for this drawable dictionary to prevent it from being streamed out if it is still being rendered.
	if (g_DwdStore.HasObjectLoaded(strLocalIndex(m_Reference))){
		gDrawListMgr->AddDwdReference(m_Reference);
	}

	CScriptStreamingResource::Destroy();
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_CLOTH_DICTIONARY
///////////////////////////////////////////
CScriptResource_ClothDictionary::CScriptResource_ClothDictionary(strLocalIndex index, u32 streamingFlags)
: CScriptStreamingResource(RESOURCE_TYPE, *g_ClothStore.GetStreamingModule(), index.Get(), streamingFlags)
{
}

strStreamingModule& CScriptResource_ClothDictionary::GetStreamingModule() const
{
	return *g_ClothStore.GetStreamingModule();
}

void CScriptResource_ClothDictionary::Destroy()
{
 	// We need to add a reference for this cloth dictionary to prevent it from being streamed out if it is still being rendered.
 	if (g_ClothStore.HasObjectLoaded(strLocalIndex(m_Reference)))
 	{
		// TODO: Do something ?... but cloth already has proper ref counting 
		// ... it is not about cloth store at all but modelinfo ref count
 	}

	CScriptStreamingResource::Destroy();
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_FRAG_DICTIONARY
///////////////////////////////////////////
CScriptResource_FragDictionary::CScriptResource_FragDictionary(strLocalIndex index, u32 streamingFlags)
	: CScriptStreamingResource(RESOURCE_TYPE, *g_FragmentStore.GetStreamingModule(), index.Get(), streamingFlags)
{
}

strStreamingModule& CScriptResource_FragDictionary::GetStreamingModule() const
{
	return *g_FragmentStore.GetStreamingModule();
}

void CScriptResource_FragDictionary::Destroy()
{
	// We need to add a reference for this fragment dictionary to prevent it from being streamed out if it is still being rendered.
	if (g_FragmentStore.HasObjectLoaded(strLocalIndex(m_Reference)))
		gDrawListMgr->AddFragReference(m_Reference);

	CScriptStreamingResource::Destroy();
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_DRAWABLE
///////////////////////////////////////////
CScriptResource_Drawable::CScriptResource_Drawable(strLocalIndex index, u32 streamingFlags)
	: CScriptStreamingResource(RESOURCE_TYPE, *g_DrawableStore.GetStreamingModule(), index.Get(), streamingFlags)
{
}

strStreamingModule& CScriptResource_Drawable::GetStreamingModule() const
{
	return *g_DrawableStore.GetStreamingModule();
}

void CScriptResource_Drawable::Destroy()
{
	// We need to add a reference for this drawable to prevent it from being streamed out if it is still being rendered.
	if (g_DrawableStore.HasObjectLoaded(strLocalIndex(m_Reference)))
		gDrawListMgr->AddDrawableReference(m_Reference);

	CScriptStreamingResource::Destroy();
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_COVERPOINT
///////////////////////////////////////////
CScriptResource_Coverpoint::CScriptResource_Coverpoint(const Vector3& vCoors, float fDirection, s32 iUsage, s32 iHeight, s32 iArc, bool bIsPriority)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	// Only LOW and TOOHIGH are valid until we receive more animations.
	int iIdentifier = CCover::AddCoverPoint(CCover::CP_SciptedPoint,
											CCoverPoint::COVTYPE_SCRIPTED,
											NULL,
											const_cast<Vector3*>(&vCoors),
											static_cast<CCoverPoint::eCoverHeight>(iHeight),
											static_cast<CCoverPoint::eCoverUsage>(iUsage),
											static_cast<u8>(( DtoR * fDirection) * (COVER_POINT_DIR_RANGE / (2.0f * PI))),
											static_cast<CCoverPoint::eCoverArc>(iArc),
											bIsPriority);

	if (SCRIPT_VERIFY( iIdentifier != INVALID_COVER_POINT_INDEX, "Failed to add scripted cover point!" ))
	{
		CScriptedCoverPoint* pScriptedCoverPoint = rage_new CScriptedCoverPoint(iIdentifier);
		if (aiVerifyf(pScriptedCoverPoint, "Failed to create scripted coverpoint"))
		{
			// If added successfully, add it to the mission list of entities
			m_Reference = CTheScripts::GetGUIDFromPreferredCoverPoint(*pScriptedCoverPoint);
		}
	}
}

void CScriptResource_Coverpoint::Destroy()
{
	const CScriptedCoverPoint* pScriptedCoverPoint = CTheScripts::GetScriptedCoverPointToModifyFromGUID(m_Reference);
	if (pScriptedCoverPoint && pScriptedCoverPoint->GetScriptIndex() != INVALID_COVER_POINT_INDEX)
	{
		CCoverPoint* pCoverPoint = CCover::FindCoverPointWithIndex(pScriptedCoverPoint->GetScriptIndex());
		if(pCoverPoint && pCoverPoint->IsOccupied())
		{
			pCoverPoint->SetFlag(CCoverPoint::COVFLAGS_WaitingForCleanup);
		}
		else
		{
			CCover::RemoveCoverPointWithIndex(pScriptedCoverPoint->GetScriptIndex());
		}

		delete pScriptedCoverPoint;
	}
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_ANIMATION
///////////////////////////////////////////
CScriptResource_Animation::CScriptResource_Animation(u32 index, u32 streamingFlags)
: CScriptStreamingResource(RESOURCE_TYPE, *fwAnimManager::GetStreamingModule(), index, streamingFlags)
{
}

strStreamingModule& CScriptResource_Animation::GetStreamingModule() const
{
	return *fwAnimManager::GetStreamingModule();
}

void CScriptResource_Animation::Destroy()
{
	animDebugf3("CScriptResource_Animation::Destroy() - %s - Anim Dict Slot Index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), m_Reference);

	CScriptStreamingResource::Destroy();
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_CLIPSET
///////////////////////////////////////////
CScriptResource_ClipSet::CScriptResource_ClipSet(fwMvClipSetId clipSetId)
	: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = clipSetId;
}

void CScriptResource_ClipSet::Destroy()
{
#if __ASSERT
	fwMvClipSetId clipSetid(m_Reference);
	animDebugf3("CScriptResource_ClipSet::Destroy() - %s - ClipSet %s (ClipSetId: %u)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), clipSetid.TryGetCStr(), clipSetid.GetHash());
#endif // __ASSERT

	fwClipSetManager::Script_RemoveClipSet((fwMvClipSetId)m_Reference);
	m_Reference = INVALID_REFERENCE;
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_VEHICLE_RECORDING
///////////////////////////////////////////
CScriptResource_VehicleRecording::CScriptResource_VehicleRecording(u32 index, u32 streamingFlags)
	: CScriptStreamingResource(RESOURCE_TYPE, *CVehicleRecordingMgr::GetStreamingModule(), index, streamingFlags)
{
}

strStreamingModule& CScriptResource_VehicleRecording::GetStreamingModule() const
{
	return *CVehicleRecordingMgr::GetStreamingModule();
}


///////////////////////////////////////////
// SCRIPT_RESOURCE_CUT_SCENE
///////////////////////////////////////////

CScriptResource_CutScene::CScriptResource_CutScene(u32 cutSceneNameHash)
	: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	if(Verifyf(cutSceneNameHash != INVALID_REFERENCE, "cutSceneNameHash is invalid!"))
	{
		m_Reference = cutSceneNameHash;
	}
}

void CScriptResource_CutScene::Destroy()
{
	if(Verifyf(m_Reference != INVALID_REFERENCE, "cutSceneNameHash is invalid!"))
	{
		CutSceneManager::GetInstance()->ReleaseScriptResource(m_Reference);
	}
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_CUTFILE_DEF
///////////////////////////////////////////
CScriptResource_CutFile::CScriptResource_CutFile(s32 index, u32 streamingFlags)
	: CScriptStreamingResource(RESOURCE_TYPE, g_CutSceneStore, index, streamingFlags)
{
}

strStreamingModule& CScriptResource_CutFile::GetStreamingModule() const
{
	return g_CutSceneStore;
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_SYNCED_SCENE
///////////////////////////////////////////

CompileTimeAssert(sizeof(scrThreadId) == sizeof(u32));

void DestroySceneCallback(fwSyncedSceneId sceneId, void *pParam)
{
	if(sceneId != INVALID_SYNCED_SCENE_ID && pParam)
	{
		scrThreadId threadId = static_cast< scrThreadId >(reinterpret_cast< size_t >(pParam));
		GtaThread *pThread = static_cast< GtaThread * >(scrThread::GetThread(threadId));
		if(pThread)
		{
			CGameScriptHandler *pHandler = pThread->m_Handler;
			if(pHandler)
			{
				pHandler->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_SYNCED_SCENE, sceneId);
			}
		}
	}
}

CScriptResource_SyncedScene::CScriptResource_SyncedScene(fwSyncedSceneId sceneId)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	if (SCRIPT_VERIFY(fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneId), "Invalid synced scene"))
	{
		GtaThread *pThread = CTheScripts::GetCurrentGtaScriptThread();
		if(pThread)
		{
			scrThreadId threadId = pThread->GetThreadId();
			void *pParam = reinterpret_cast< void * >(static_cast< u32 >(threadId));
			fwAnimDirectorComponentSyncedScene::SetDestroySceneCallback(sceneId, DestroySceneCallback, pParam);
		}

		m_Reference = sceneId;
	}
}

void CScriptResource_SyncedScene::Destroy()
{
	fwSyncedSceneId sceneId = static_cast< fwSyncedSceneId >(m_Reference);

	if (fwAnimDirectorComponentSyncedScene::IsValidSceneId(sceneId))
	{
		if(fwAnimDirectorComponentSyncedScene::GetSyncedSceneRefCount(sceneId) == 0)
		{
			// Remove synced scenes that have never had anything added to them
			fwAnimDirectorComponentSyncedScene::AddSyncedSceneRef(sceneId);
			fwAnimDirectorComponentSyncedScene::DecSyncedSceneRef(sceneId);
		}
	}

	m_Reference = INVALID_REFERENCE;
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_MODEL
///////////////////////////////////////////
CScriptResource_Model::CScriptResource_Model(u32 modelHashkey, u32 streamingFlags)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(modelHashkey, &modelId);	

	if (SCRIPT_VERIFY(pModelInfo, "Invalid model"))
	{
		if (pModelInfo->GetModelType() == MI_TYPE_MLO)
		{
			s32 flags = STRFLAG_MISSION_REQUIRED|STRFLAG_FORCE_LOAD|streamingFlags;
			CTheScripts::PrioritizeRequestsFromMissionScript(flags);
			static_cast<CMloModelInfo*>(pModelInfo)->RequestObjectsInRoom("limbo", flags);			// "limbo" is always the name of room 0
		} 
		else
		{
			s32 flags = STRFLAG_MISSION_REQUIRED|streamingFlags;
			CTheScripts::PrioritizeRequestsFromMissionScript(flags);

			strLocalIndex localIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId);
			GetStreamingModule().StreamingRequest(localIndex, flags);

			//if this is a vehicle, also request its driver
			if (pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
			{
				gPopStreaming.RequestVehicleDriver(modelId.GetModelIndex());
			}
		}

//		The call to fwArchetypeManager::AddArchetypeToTypeFileRef(pModelInfo) has been moved to CommandRequestModel in commands_streaming.cpp
		m_Reference = modelHashkey;
	}
}

void CScriptResource_Model::Destroy()
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_Reference, &modelId);

	if(SCRIPT_VERIFY( pModelInfo, "Model does not exist or is invalid"))
	{
		if (pModelInfo->GetModelType() == MI_TYPE_MLO)
		{
			static_cast<CMloModelInfo*>(pModelInfo)->SetMissionDoesntRequireRoom("limbo");			// "limbo" is always the name of room 0
		}
		else
		{
			strLocalIndex localIndex = CModelInfo::LookupLocalIndex(modelId);
			CStreaming::SetMissionDoesntRequireObject(localIndex, GetStreamingModule().GetStreamingModuleId());
		}

		fwArchetypeManager::ScheduleRemoveArchetypeToTypeFileRef(pModelInfo);
	}
};

strIndex CScriptResource_Model::GetStreamingIndex() const
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_Reference, &modelId);

	if(SCRIPT_VERIFY( pModelInfo, "Model does not exist or is invalid"))
	{
		if (pModelInfo->GetModelType() != MI_TYPE_MLO)
		{
			strLocalIndex localIndex = CModelInfo::LookupLocalIndex(modelId);

			return GetStreamingModule().GetStreamingIndex(localIndex);
		}
	}

	return strIndex(strIndex::INVALID_INDEX);
}

strStreamingModule& CScriptResource_Model::GetStreamingModule() const
{
	return *CModelInfo::GetStreamingModule();
}

#if __DEV
void CScriptResource_Model::DisplayDebugInfo(const char* scriptName) const
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_Reference, &modelId);

	if(SCRIPT_VERIFY( pModelInfo, "Model does not exist or is invalid"))
	{
		if (pModelInfo->GetModelType() != MI_TYPE_MLO)
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			if (pLocalPlayer && pLocalPlayer->GetModelIndex() == modelId.GetModelIndex())
			{
				return;
			}

			strLocalIndex localIndex = strLocalIndex(CModelInfo::LookupLocalIndex(modelId));

			grcDebugDraw::AddDebugOutput("%s: Requested Model %d (%s, %s)", scriptName, localIndex, GetStreamingModule().GetModuleName(), GetStreamingModule().GetName(localIndex));
		}
	}
}

void CScriptResource_Model::SpewDebugInfo(const char* scriptName) const
{
	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_Reference, &modelId);

	if(SCRIPT_VERIFY( pModelInfo, "Model does not exist or is invalid"))
	{
		if (pModelInfo->GetModelType() != MI_TYPE_MLO)
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			if (pLocalPlayer && pLocalPlayer->GetModelIndex() == modelId.GetModelIndex())
			{
				return;
			}

			strLocalIndex localIndex = strLocalIndex(CModelInfo::LookupLocalIndex(modelId));

			Displayf("%s: Requested Model %d (%s, %s)\n", scriptName, localIndex.Get(), GetStreamingModule().GetModuleName(), GetStreamingModule().GetName(localIndex));
		}
	}
}
#endif	//	__DEV


#if __SCRIPT_MEM_CALC
void CScriptResource_Model::CalculateMemoryUsage(const char* SCRIPT_MEM_DISPLAY_ONLY(scriptName), atArray<strIndex>& streamingIndices, 
							const strIndex* SCRIPT_MEM_DISPLAY_ONLY(ignoreList), s32 SCRIPT_MEM_DISPLAY_ONLY(numIgnores), 
							u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
							bool SCRIPT_MEM_DISPLAY_ONLY(bDisplayScriptDetails), u32 skipFlag) const
{
	NonStreamingVirtualForThisResource = 0;
	NonStreamingPhysicalForThisResource = 0;

	fwModelId modelId;
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(m_Reference, &modelId);

	if(SCRIPT_VERIFY(pModelInfo, "CScriptResource_Model::CalculateMemoryUsage - Model does not exist or is invalid"))
	{
		if (pModelInfo->GetModelType() != MI_TYPE_MLO)
		{
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			if (pLocalPlayer && pLocalPlayer->GetModelIndex() == modelId.GetModelIndex())
			{
				return;
			}

			strLocalIndex localIndex = strLocalIndex(CModelInfo::LookupLocalIndex(modelId));
			strIndex streamingIndex = GetStreamingModule().GetStreamingIndex(localIndex);

			if(streamingIndex != strIndex(strIndex::INVALID_INDEX) && 
				!(strStreamingEngine::GetInfo().GetObjectFlags(streamingIndex) & skipFlag))
			{
				streamingIndices.PushAndGrow(streamingIndex);

#if __SCRIPT_MEM_DISPLAY
				if (bDisplayScriptDetails)
				{
					u32 virtualSize = 0;
					u32 physicalSize = 0;

					strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndex, virtualSize, physicalSize, ignoreList, numIgnores, true);

					grcDebugDraw::AddDebugOutput("%s %s Requested Model %dK %dK", 
						scriptName, 
						GetStreamingModule().GetName(localIndex),
						virtualSize>>10, physicalSize>>10);	
				}
#endif	//	__SCRIPT_MEM_DISPLAY
			}
		}
	}
}
#endif	//	__SCRIPT_MEM_CALC


///////////////////////////////////////////
// SCRIPT_RESOURCE_RADAR_BLIP
///////////////////////////////////////////
CScriptResource_RadarBlip::CScriptResource_RadarBlip(u32 iBlipType, const CEntityPoolIndexForBlip& poolIndex, u32 iDisplayFlag, const char *pScriptName, float scale)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	s32 ref = CMiniMap::CreateBlip(true, (eBLIP_TYPE)iBlipType, poolIndex, (eBLIP_DISPLAY_TYPE)iDisplayFlag, pScriptName);

	if (CMiniMap::IsBlipIdInUse(ref))
	{
		if (scale > 0.0f)
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, ref, scale);
		}

		m_Reference = ref;
	}
}

CScriptResource_RadarBlip::CScriptResource_RadarBlip(u32 iBlipType, const Vector3& vPosition, u32 iDisplayFlag, const char *pScriptName, float scale)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	s32 ref = CMiniMap::CreateBlip(true, (eBLIP_TYPE)iBlipType, vPosition, (eBLIP_DISPLAY_TYPE)iDisplayFlag, pScriptName);

	if (CMiniMap::IsBlipIdInUse(ref))
	{
		if (scale > 0.0f)
		{
			CMiniMap::SetBlipParameter(BLIP_CHANGE_SCALE, ref, scale);
		}

		m_Reference = ref;
	}
}

CScriptResource_RadarBlip::CScriptResource_RadarBlip(s32 blipIndex)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = blipIndex;
}

void CScriptResource_RadarBlip::Destroy()
{
	CMiniMap::SetBlipParameter(BLIP_CHANGE_DELETE, m_Reference);
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_ROPE
///////////////////////////////////////////
CScriptResource_Rope::CScriptResource_Rope(Vec3V_In pos, Vec3V_In rot, float initLength, float minLength, float maxLength, float lengthChangeRate, int ropeType, bool ppuOnly, bool /*collisionOn*/, bool lockFromFront, float timeMultiplier, bool breakable, bool pinned )
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "CScriptResource_Rope::CScriptResource_Rope(1) - Rope Manager doesn't exist or is damaged!") )
	{
		initLength = Selectf(initLength, initLength, maxLength );
	
		ropeInstance* pRope = pRopeManager->AddRope(pos, rot, initLength, minLength, maxLength, lengthChangeRate, ropeType, -1, ppuOnly, lockFromFront, timeMultiplier, breakable, pinned);
		if( SCRIPT_VERIFY(pRope, "CScriptResource_Rope::CScriptResource_Rope - Rope creation failed!") )
		{
// TODO: disabled by svetli
// ropes will be marked GTA_ENVCLOTH_OBJECT_TYPE by default
// 			if( collisionOn)
 				pRope->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );

			m_Reference = pRope->SetNewUniqueId();
		}
	}
}

CScriptResource_Rope::CScriptResource_Rope(ropeInstance* pRope)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "CScriptResource_Rope::CScriptResource_Rope(2) - Rope Manager doesn't exist or is damaged!") )
	{
		if( SCRIPT_VERIFY(pRope, "CScriptResource_Rope::CScriptResource_Rope - Rope doesn't exist!") )
		{
			m_Reference = pRope->SetNewUniqueId();
		}
	}
}

void CScriptResource_Rope::Destroy()
{
	ropeManager* pRopeManager = CPhysics::GetRopeManager();
	if( SCRIPT_VERIFY(pRopeManager, "CScriptResource_Rope::Destroy - Rope Manager doesn't exist or is damaged!") )
	{
		const int numDeletedRopes = pRopeManager->RemoveRope( m_Reference );
		if( !numDeletedRopes )
		{
			clothDebugf1("CScriptResource_Rope::Destroy - Rope with ID: %d, doesn't exist or is damaged ... most likely is deleted already!", m_Reference );
		}		
	}
}


///////////////////////////////////////////
// SCRIPT_RESOURCE_CAMERA
///////////////////////////////////////////
CScriptResource_Camera::CScriptResource_Camera(const char* CameraName, bool bActivate)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	int NewCameraIndex = camInterface::GetScriptDirector().CreateCamera(CameraName);

	camBaseCamera* pCamera = camBaseCamera::GetCameraAtPoolIndex(NewCameraIndex);
	if(pCamera)
	{
		m_Reference = NewCameraIndex;

		if(bActivate)
		{
			camInterface::GetScriptDirector().ActivateCamera(NewCameraIndex);	
		}
	}
}

CScriptResource_Camera::CScriptResource_Camera(u32 CameraHash, bool bActivate)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	int NewCameraIndex = camInterface::GetScriptDirector().CreateCamera(CameraHash);

	camBaseCamera* pCamera = camBaseCamera::GetCameraAtPoolIndex(NewCameraIndex);
	if(pCamera)
	{
		m_Reference = NewCameraIndex;

		if(bActivate)
		{
			camInterface::GetScriptDirector().ActivateCamera(NewCameraIndex);	
		}
	}
}

void CScriptResource_Camera::Destroy()
{
	camInterface::GetScriptDirector().DestroyCamera(m_Reference);
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_PATROL_ROUTE
///////////////////////////////////////////
CScriptResource_PatrolRoute::CScriptResource_PatrolRoute(u32 patrolRouteHash, fwIplPatrolNode* aPatrolNodes, s32* aOptionalNodeIds, 
														 s32 iNumNodes, fwIplPatrolLink* aPatrolLinks, s32 iNumLinks, s32 iNumGuardsPerRoute)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	if (SCRIPT_VERIFY(!CPatrolRoutes::GetNamedPatrolRoute(patrolRouteHash), "Route name already exists"))	
	{
		CPatrolRoutes::StorePatrolRoutes(aPatrolNodes, aOptionalNodeIds, iNumNodes, aPatrolLinks, iNumLinks, iNumGuardsPerRoute);
		m_Reference = patrolRouteHash;
	}
}

void CScriptResource_PatrolRoute::Destroy()
{
	CPatrolRoutes::DeleteNamedPatrolRoute(m_Reference);
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_MLO
///////////////////////////////////////////
CScriptResource_MLO::CScriptResource_MLO(s32 InteriorProxyIndex)
: CGameScriptResourceWithCachedSize(RESOURCE_TYPE, INVALID_REFERENCE)
{
 	CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);

	if (SCRIPT_VERIFY(pIntProxy, "Interior proxy doesn't exist"))
	{
		pIntProxy->SetRequestedState( CInteriorProxy::RM_SCRIPT, CInteriorProxy::PS_FULL_WITH_COLLISIONS);

		m_Reference = InteriorProxyIndex;
	}
}

void CScriptResource_MLO::Destroy()
{
	CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetAt(m_Reference);
	Assert(pIntProxy);

	if (SCRIPT_VERIFY(pIntProxy, "Interior proxy doesn't exist"))
	{
		pIntProxy->SetRequestedState( CInteriorProxy::RM_SCRIPT, CInteriorProxy::PS_NONE);
	}
};


#if __SCRIPT_MEM_CALC

void CGameScriptResourceWithCachedSize::CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
								  u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
								  bool bDisplayScriptDetails, u32 skipFlag) const
{
	if( IsCached() )
	{
		NonStreamingVirtualForThisResource = m_CachedNonStreamingVirtualForThisResource;
		NonStreamingPhysicalForThisResource	= m_CachedNonStreamingPhysicalForThisResource;
	}
	else
	{
		if (CalculateMemoryUsageForCache(	scriptName, streamingIndices, ignoreList, numIgnores, 
			NonStreamingVirtualForThisResource, NonStreamingPhysicalForThisResource,
			bDisplayScriptDetails, skipFlag))
		{
			m_isCached = true;
			m_CachedNonStreamingVirtualForThisResource = NonStreamingVirtualForThisResource;
			m_CachedNonStreamingPhysicalForThisResource	= NonStreamingPhysicalForThisResource;
		}
	}


	DisplayMemoryUsage( NonStreamingVirtualForThisResource, NonStreamingPhysicalForThisResource, scriptName, bDisplayScriptDetails);
}


bool CScriptResource_MLO::CalculateMemoryUsageForCache(const char* UNUSED_PARAM(scriptName), atArray<strIndex>& UNUSED_PARAM(streamingIndices), const strIndex* UNUSED_PARAM(ignoreList), s32 UNUSED_PARAM(numIgnores), 
								  u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
								  bool UNUSED_PARAM(bDisplayScriptDetails), u32 UNUSED_PARAM(skipFlag)) const
{
	NonStreamingVirtualForThisResource = 0;
	NonStreamingPhysicalForThisResource = 0;

	CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetAt(m_Reference);
	Assert(pIntProxy);
	CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();

	if (pIntInst)
	{
		if (pIntInst->m_bInstanceDataGenerated)
		{
			u32 modelIdxCount = 0;

			atArray<u32> modelIdxArray;

			modelIdxArray.Reset();
			modelIdxArray.Reserve(256);
			pIntInst->AddInteriorContents(-1, false, true, modelIdxArray); //get portals
			pIntInst->AddInteriorContents(-1, true, false, modelIdxArray);	//	all rooms

			s32 totalPhysicalMemory = 0;
			s32 totalVirtualMemory = 0;
			s32 drawableMemory = 0;
			s32 drawableVirtualMemory = 0;
			s32 textureMemory = 0;
			s32 textureVirtualMemory = 0;
			s32 boundsMemory = 0;
			s32 boundsVirtualMemory = 0;
			CStreamingDebug::GetMemoryStatsByAllocation(modelIdxArray, totalPhysicalMemory, totalVirtualMemory, drawableMemory, drawableVirtualMemory, textureMemory, textureVirtualMemory, boundsMemory, boundsVirtualMemory);
			NonStreamingPhysicalForThisResource = totalPhysicalMemory;
			NonStreamingVirtualForThisResource = totalVirtualMemory;
			modelIdxCount = modelIdxArray.GetCount();

			m_CachedModelIdxArrayCount = modelIdxCount;
			return true;

		}
	} 
	return false;
}

void CScriptResource_MLO::DisplayMemoryUsage(u32 NonStreamingVirtualForThisResource, u32 NonStreamingPhysicalForThisResource, const char *scriptName, bool bDisplayScriptDetails) const
{
	CInteriorProxy* pIntProxy = CInteriorProxy::GetPool()->GetAt(m_Reference);
	Assert(pIntProxy);
	CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();

	if (pIntInst)
	{

#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			// display memory use for this interior
			grcDebugDraw::AddDebugOutput("%s Pinned Interior %s (%d entities) : Virtual %dK Physical %dK", 
				scriptName, pIntInst->GetModelName(), m_CachedModelIdxArrayCount, 
				NonStreamingVirtualForThisResource>>10, NonStreamingPhysicalForThisResource>>10);
		}
#endif	//	__SCRIPT_MEM_DISPLAY

	}
	else
	{

#if __SCRIPT_MEM_DISPLAY
		grcDebugDraw::AddDebugOutput("%s script pinned %s : Proxy has no interior", scriptName, pIntProxy->GetModelName() );
#endif	//	__SCRIPT_MEM_DISPLAY

	}
}

#endif	//	__SCRIPT_MEM_CALC

///////////////////////////////////////////
// SCRIPT_RESOURCE_RELATIONSHIP_GROUP
///////////////////////////////////////////
CScriptResource_RelGroup::CScriptResource_RelGroup(const char* szGroupName)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	atHashString hashString(szGroupName);
	CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup(hashString);
	if(!pGroup)
	{
		pGroup = CRelationshipManager::AddRelationshipGroup(hashString, RT_mission);
	}
	else if(scriptVerifyf(pGroup->GetCanBeCleanedUp(), "%s: Trying to recreate a rel group that isn't marked for cleanup(%s)",
						CTheScripts::GetCurrentScriptNameAndProgramCounter(), szGroupName)) 
	{
		// "Reset" the relationship group to make it as if we just created it
		pGroup->SetCanBeCleanedUp(false);
		pGroup->ClearAllRelationships();
		pGroup->SetRelationship(ACQUAINTANCE_TYPE_PED_LIKE, pGroup);
	}

	if (SCRIPT_VERIFY(pGroup, "Failed to add relationship group"))
	{
		m_Reference = pGroup->GetName().GetHash();
	}
}

void CScriptResource_RelGroup::Destroy()
{
	CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup((u32)m_Reference);

	if (scriptVerifyf(pGroup, "%s: Trying to remove a relationship group (%d) that could not be found", CTheScripts::GetCurrentScriptNameAndProgramCounter(), (u32)m_Reference))
	{
		if (SCRIPT_VERIFY_TWO_STRINGS(pGroup->GetType() == RT_mission, "(%s) Cant remove non-mission created relationship groups!", pGroup->GetName().GetCStr()))
		{
			if(!pGroup->IsReferenced())
			{
				CRelationshipManager::RemoveRelationshipGroup(pGroup);
			}
			else
			{
				pGroup->SetCanBeCleanedUp(true);
			}
		}
	}
};



///////////////////////////////////////////
// SCRIPT_RESOURCE_SCALEFORM_MOVIE
///////////////////////////////////////////
CScriptResource_ScaleformMovie::CScriptResource_ScaleformMovie(s32 iNewId, const char *pFilename)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	s32 iRef = CScriptHud::SetupScaleformMovie(iNewId, pFilename);

	m_Reference = iRef;
}

void CScriptResource_ScaleformMovie::Destroy()
{
	sfDebugf3("ScriptResource_ScaleformMovie::Destroy is about to check if movie %s (%d) (SF %d) is in a state where it can be removed", CScriptHud::ScriptScaleformMovie[m_Reference-1].cFilename, m_Reference, CScriptHud::ScriptScaleformMovie[m_Reference-1].iId);

	if (CScaleformMgr::CanMovieBeRemovedByScript(CScriptHud::ScriptScaleformMovie[m_Reference-1].iId))
	{
		sfDebugf3("ScriptResource_ScaleformMovie::Destroy has set movie %s (%d) (SF %d) as no longer needed", CScriptHud::ScriptScaleformMovie[m_Reference-1].cFilename, m_Reference, CScriptHud::ScriptScaleformMovie[m_Reference-1].iId);

		CScriptHud::RemoveScaleformMovie(m_Reference-1);
	}
};

#if __SCRIPT_MEM_CALC
strLocalIndex CScriptResource_ScaleformMovie::GetLocalStreamingIndex() const
{
	u32 movId = CScriptHud::GetScaleformMovieID(m_Reference);
	return CScaleformMgr::GetMovieStreamingIndex(movId);	
}

strIndex CScriptResource_ScaleformMovie::GetStreamingIndex() const
{
	return g_ScaleformStore.GetStreamingIndex(GetLocalStreamingIndex());	
}

void CScriptResource_ScaleformMovie::CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& UNUSED_PARAM(streamingIndices), const strIndex* ignoreList, s32 numIgnores, 
								  u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
								  bool bDisplayScriptDetails, u32 skipFlag) const
{
	if (GetLocalStreamingIndex().Get() == strLocalIndex::INVALID_INDEX)
		return;

	strIndex streamingIndex = GetStreamingIndex();
	NonStreamingVirtualForThisResource = 0;
	NonStreamingPhysicalForThisResource = 0;

	if(streamingIndex != strIndex(strIndex::INVALID_INDEX) && !(strStreamingEngine::GetInfo().GetObjectFlags(streamingIndex) & skipFlag))
	{
#if __SCRIPT_MEM_DISPLAY
		if (bDisplayScriptDetails)
		{
			u32 virtualSize = 0;
			u32 physicalSize = 0;
			strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndex, virtualSize, physicalSize, ignoreList, numIgnores, true);
			grcDebugDraw::AddDebugOutput("%s %s %s (%dK %dK)", 
				scriptName, 
				g_ScaleformStore.GetName(GetLocalStreamingIndex()),
				GetResourceName(),
				virtualSize>>10, physicalSize>>10);
		}
#endif	//	__SCRIPT_MEM_DISPLAY
	}

}
#endif	//	__SCRIPT_MEM_CALC


///////////////////////////////////////////
// SCRIPT_RESOURCE_STREAMED_SCRIPT
///////////////////////////////////////////
CScriptResource_StreamedScript::CScriptResource_StreamedScript(s32 StreamedScriptIndex)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	if(StreamedScriptIndex != INVALID_REFERENCE)
	{
		g_StreamedScripts.StreamingRequest(strLocalIndex(StreamedScriptIndex), STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);
		m_Reference = StreamedScriptIndex;
	}
}

void CScriptResource_StreamedScript::Destroy()
{
	if (m_Reference != INVALID_REFERENCE)
	{
		g_StreamedScripts.ClearRequiredFlag(m_Reference, STRFLAG_MISSION_REQUIRED);
	}
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_ITEMSET
///////////////////////////////////////////
CScriptResource_ItemSet::CScriptResource_ItemSet(bool autoClean)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	CItemSet* pObjSet = rage_new CItemSet(autoClean); 
	if(SCRIPT_VERIFY(pObjSet, "CREATE_ITEMSET - Couldn't create a new object set"))
	{
		m_Reference = fwScriptGuid::CreateGuid(*pObjSet);
	}
}

void CScriptResource_ItemSet::Destroy()
{
	CItemSet* pItemSet = fwScriptGuid::FromGuid<CItemSet>(m_Reference);
	if(SCRIPT_VERIFY(pItemSet, "Failed to find set, was expected to exist."))
	{
		delete pItemSet;
	}
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_SPEED_ZONE
///////////////////////////////////////////
CScriptResource_SpeedZone::CScriptResource_SpeedZone(Vector3& vSphereCenter, float fSphereRadius, float fMaxSpeed, bool bAllowAffectMissionVehs)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = CRoadSpeedZoneManager::GetInstance().AddZone(vSphereCenter, fSphereRadius, fMaxSpeed, bAllowAffectMissionVehs);
}

void CScriptResource_SpeedZone::Destroy()
{
	CRoadSpeedZoneManager::GetInstance().RemoveZone(m_Reference);
}


///////////////////////////////////////////
// SCRIPT_RESOURCE_SCENARIO_SUPPRESSION_ZONE
///////////////////////////////////////////
CScriptResource_ScenarioBlockingArea::CScriptResource_ScenarioBlockingArea(Vector3& vMin, Vector3 &vMax, bool bCancelActive, bool bBlocksPeds, bool bBlocksVehicles)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	const char* debugUserName = NULL;
#if __BANK
	debugUserName = CTheScripts::GetCurrentScriptName();
#endif	// __BANK

	m_Reference = CScenarioBlockingAreas::AddScenarioBlockingArea(vMin, vMax, bCancelActive, bBlocksPeds, bBlocksVehicles, CScenarioBlockingAreas::kUserTypeScript, debugUserName);
}

void CScriptResource_ScenarioBlockingArea::Destroy()
{
	CScenarioBlockingAreas::RemoveScenarioBlockingArea(m_Reference);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_WEAPON_ASSET
///////////////////////////////////////////
CScriptResource_Weapon_Asset::CScriptResource_Weapon_Asset(const s32 iWeaponHash, s32 iRequestFlags, s32 iExtraComponentFlags)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = CWeaponScriptResourceManager::RegisterResource(iWeaponHash, iRequestFlags, iExtraComponentFlags);
}

void CScriptResource_Weapon_Asset::Destroy()
{
	CWeaponScriptResourceManager::UnregisterResource(m_Reference);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_VEHICLE_ASSET
///////////////////////////////////////////
CScriptResource_Vehicle_Asset::CScriptResource_Vehicle_Asset(const s32 iVehicleModelHash, s32 iRequestFlags)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = CVehicleScriptResourceManager::RegisterResource(iVehicleModelHash, iRequestFlags);
}

void CScriptResource_Vehicle_Asset::Destroy()
{
	CVehicleScriptResourceManager::UnregisterResource(m_Reference);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE
///////////////////////////////////////////
CScriptResource_PopScheduleOverride::CScriptResource_PopScheduleOverride(const s32 schedule, u32 popGroup, int percentage)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	CPopSchedule& popSchedule = CPopCycle::GetPopSchedules().GetSchedule(schedule);
	popSchedule.SetOverridePedGroup(popGroup, percentage);
	
	m_Reference = schedule;
}

void CScriptResource_PopScheduleOverride::Destroy()
{
	CPopCycle::GetPopSchedules().GetSchedule(m_Reference).SetOverridePedGroup(0, 0);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE
///////////////////////////////////////////
CScriptResource_PopScheduleVehicleModelOverride::CScriptResource_PopScheduleVehicleModelOverride(const s32 schedule, u32 modelIndex)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	CPopSchedule& popSchedule = CPopCycle::GetPopSchedules().GetSchedule(schedule);
	popSchedule.SetOverrideVehicleModel(modelIndex);
	
	m_Reference = schedule;
}

void CScriptResource_PopScheduleVehicleModelOverride::Destroy()
{
	CPopCycle::GetPopSchedules().GetSchedule(m_Reference).SetOverrideVehicleModel(fwModelId::MI_INVALID);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_BINK_MOVIE
///////////////////////////////////////////
CScriptResource_BinkMovie::CScriptResource_BinkMovie(const char *pMovieName)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = g_movieMgr.PreloadMovieFromScript((char*) pMovieName);
}

void CScriptResource_BinkMovie::Destroy()
{
	u32 handle = g_movieMgr.GetHandleFromScriptSlot(m_Reference);
	g_movieMgr.Release(handle);
}

#if __SCRIPT_MEM_CALC
void CScriptResource_BinkMovie::CalculateMemoryUsage(const char* SCRIPT_MEM_DISPLAY_ONLY(scriptName), atArray<strIndex>& UNUSED_PARAM(streamingIndices), const strIndex* UNUSED_PARAM(ignoreList), s32 UNUSED_PARAM(numIgnores), 
								  u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
								  bool SCRIPT_MEM_DISPLAY_ONLY(bDisplayScriptDetails), u32 UNUSED_PARAM(skipFlag)) const
{
	u32 handle = g_movieMgr.GetHandleFromScriptSlot(m_Reference);
	NonStreamingVirtualForThisResource = g_movieMgr.GetMemoryUsage(handle);
	NonStreamingPhysicalForThisResource = 0;

#if __SCRIPT_MEM_DISPLAY
	if (bDisplayScriptDetails)
	{
		grcDebugDraw::AddDebugOutput("%s %s %dK %dK", 
			scriptName, 
	//		GetStreamingModule().GetName(m_Reference),
			GetResourceName(),
			NonStreamingVirtualForThisResource>>10, NonStreamingPhysicalForThisResource>>10);
	}
#endif	//	__SCRIPT_MEM_DISPLAY
}
#endif	//	__SCRIPT_MEM_CALC


///////////////////////////////////////////
// SCRIPT_RESOURCE_MOVIE_MESH_SET
///////////////////////////////////////////
CScriptResource_MovieMeshSet::CScriptResource_MovieMeshSet(const char* pSetName)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	MovieMeshSetHandle handle = g_movieMeshMgr.LoadSet(pSetName);
	m_Reference = g_movieMeshMgr.GetScriptIdFromHandle(handle);
}

void CScriptResource_MovieMeshSet::Destroy()
{
 	MovieMeshSetHandle handle = g_movieMeshMgr.GetHandleFromScriptId(m_Reference);
 	g_movieMeshMgr.ReleaseSet(handle);
}

#if __SCRIPT_MEM_CALC
void CScriptResource_MovieMeshSet::CalculateMemoryUsage(const char* UNUSED_PARAM(scriptName), atArray<strIndex>& UNUSED_PARAM(streamingIndices), const strIndex* UNUSED_PARAM(ignoreList), s32 UNUSED_PARAM(numIgnores), 
								  u32& NonStreamingVirtualForThisResource, u32& NonStreamingPhysicalForThisResource,
								  bool UNUSED_PARAM(bDisplayScriptDetails), u32 UNUSED_PARAM(skipFlag)) const
{
	NonStreamingVirtualForThisResource = 0;
	NonStreamingPhysicalForThisResource = 0;

	MovieMeshSetHandle handle = g_movieMeshMgr.GetHandleFromScriptId(m_Reference);

	g_movieMeshMgr.CountMemoryUsageForMovies(handle);
	g_movieMeshMgr.CountMemoryUsageForObjects(handle);
	g_movieMeshMgr.CountMemoryUsageForRenderTargets(handle);
}
#endif	//	__SCRIPT_MEM_CALC


///////////////////////////////////////////
// SCRIPT_RESOURCE_SET_REL_GROUP_DONT_AFFECT_WANTED_LEVEL
///////////////////////////////////////////
CScriptResource_SetRelGroupDontAffectWantedLevel::CScriptResource_SetRelGroupDontAffectWantedLevel(int iRelGroup)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	// Find the relationship group and set it to not affects the wanted level
	CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup((u32)iRelGroup);
	if (SCRIPT_VERIFY(pGroup, "Failed to find relationship group!"))
	{
		pGroup->SetAffectsWantedLevel(false);
		m_Reference = iRelGroup;
	}
}

void CScriptResource_SetRelGroupDontAffectWantedLevel::Destroy()
{
	// We only attempt to change the influence back to true if we successfully found the rel group on creation
	if(m_Reference != INVALID_REFERENCE)
	{
		CRelationshipGroup* pGroup = CRelationshipManager::FindRelationshipGroup((u32)m_Reference);
		if (pGroup)
		{
			pGroup->SetAffectsWantedLevel(true);
		}
	}
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_VEHICLE_COMBAT_AVOIDANCE_AREA
///////////////////////////////////////////
CScriptResource_VehicleCombatAvoidanceArea::CScriptResource_VehicleCombatAvoidanceArea(Vec3V_In vStart, Vec3V_In vEnd, float fWidth)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	//Add the area.
	s32 iIndex = CVehicleCombatAvoidanceArea::Add(vStart, vEnd, fWidth);
	if(SCRIPT_VERIFY(iIndex != NULL_IN_SCRIPTING_LANGUAGE, "Failed to add vehicle combat avoidance area."))
	{
		//Assign the reference.
		m_Reference = iIndex;
	}
}

CScriptResource_VehicleCombatAvoidanceArea::CScriptResource_VehicleCombatAvoidanceArea(Vec3V_In vCenter, float fRadius)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	//Add the area.
	s32 iIndex = CVehicleCombatAvoidanceArea::AddSphere(vCenter, fRadius);
	if(SCRIPT_VERIFY(iIndex != NULL_IN_SCRIPTING_LANGUAGE, "Failed to add vehicle combat avoidance area."))
	{
		//Assign the reference.
		m_Reference = iIndex;
	}
}

void CScriptResource_VehicleCombatAvoidanceArea::Destroy()
{
	//Ensure the reference is valid.
	if(m_Reference != INVALID_REFERENCE)
	{
		//Remove the area.
		CVehicleCombatAvoidanceArea::Remove(m_Reference);
	}
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS
///////////////////////////////////////////
CScriptResource_DispatchTimeBetweenSpawnAttempts::CScriptResource_DispatchTimeBetweenSpawnAttempts(int iType, float fTimeBetweenSpawnAttempts)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	//Ensure the dispatch type is valid.
	if(SCRIPT_VERIFY(iType > DT_Invalid && iType < DT_Max, "Dispatch type is invalid."))
	{
		//Assign the reference.
		m_Reference = iType;
		
		//Grab the type.
		DispatchType nType = (DispatchType)iType;
		
		//Set the time between spawn attempts.
		CDispatchService* pDispatchService = CDispatchManager::GetInstance().GetDispatch(nType);
		if(SCRIPT_VERIFY(pDispatchService, "Dispatch service is invalid."))
		{
			pDispatchService->SetTimeBetweenSpawnAttemptsOverride(fTimeBetweenSpawnAttempts);
		}
	}
}

void CScriptResource_DispatchTimeBetweenSpawnAttempts::Destroy()
{
	//Ensure the reference is valid.
	if(m_Reference != INVALID_REFERENCE)
	{
		//Grab the type.
		DispatchType nType = (DispatchType)m_Reference;
		
		//Reset the time between spawn attempts.
		CDispatchService* pDispatchService = CDispatchManager::GetInstance().GetDispatch(nType);
		if(SCRIPT_VERIFY(pDispatchService, "Dispatch service is invalid."))
		{
			pDispatchService->ResetTimeBetweenSpawnAttemptsOverride();
		}
	}
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER
///////////////////////////////////////////
CScriptResource_DispatchTimeBetweenSpawnAttemptsMultiplier::CScriptResource_DispatchTimeBetweenSpawnAttemptsMultiplier(int iType, float fTimeBetweenSpawnAttemptsMultiplier)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	//Ensure the dispatch type is valid.
	if(SCRIPT_VERIFY(iType > DT_Invalid && iType < DT_Max, "Dispatch type is invalid."))
	{
		//Assign the reference.
		m_Reference = iType;

		//Grab the type.
		DispatchType nType = (DispatchType)iType;

		//Set the time between spawn attempts.
		CDispatchService* pDispatchService = CDispatchManager::GetInstance().GetDispatch(nType);
		if(SCRIPT_VERIFY(pDispatchService, "Dispatch service is invalid."))
		{
			pDispatchService->SetTimeBetweenSpawnAttemptsMultiplierOverride(fTimeBetweenSpawnAttemptsMultiplier);
		}
	}
}

void CScriptResource_DispatchTimeBetweenSpawnAttemptsMultiplier::Destroy()
{
	//Ensure the reference is valid.
	if(m_Reference != INVALID_REFERENCE)
	{
		//Grab the type.
		DispatchType nType = (DispatchType)m_Reference;

		//Reset the time between spawn attempts.
		CDispatchService* pDispatchService = CDispatchManager::GetInstance().GetDispatch(nType);
		if(SCRIPT_VERIFY(pDispatchService, "Dispatch service is invalid."))
		{
			pDispatchService->ResetTimeBetweenSpawnAttemptsMultiplierOverride();
		}
	}
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_ACTION_MODE_ASSET
///////////////////////////////////////////
CScriptResource_MovementMode_Asset::CScriptResource_MovementMode_Asset(const s32 iMovementModeHash, const CPedModelInfo::PersonalityMovementModes::MovementModes Type)
: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
	m_Reference = CMovementModeScriptResourceManager::RegisterResource(iMovementModeHash, Type);
}

void CScriptResource_MovementMode_Asset::Destroy()
{
	CMovementModeScriptResourceManager::UnregisterResource(m_Reference);
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_GHOST_SETTINGS
///////////////////////////////////////////
CScriptResource_GhostSettings::CScriptResource_GhostSettings()
	: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
}

void CScriptResource_GhostSettings::Destroy()
{
	CNetObjPhysical::SetInvertGhosting(false);
	CNetObjPhysical::ResetGhostAlpha();
}

///////////////////////////////////////////
// SCRIPT_RESOURCE_PICKUP_GENERATION_MULTIPLIER
///////////////////////////////////////////
CScriptResource_PickupGenerationMultiplier::CScriptResource_PickupGenerationMultiplier()
	: CGameScriptResource(RESOURCE_TYPE, INVALID_REFERENCE)
{
}

void CScriptResource_PickupGenerationMultiplier::Destroy()
{
	CPickupPlacement::SetGenerationRangeMultiplier(1.0f);	
}
// eof
