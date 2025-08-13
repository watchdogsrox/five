//
// name:		GameScriptResources.h
// description:	Classes managing the creation and destruction of various resources used by the scripts
// written by:	John Gurney
//
#ifndef GAME_SCRIPT_RESOURCES_H
#define GAME_SCRIPT_RESOURCES_H

// Rage headers
#include "vectormath/vec3v.h"

// framework includes
#include "entity/archetypeManager.h"
#include "fwanimation/animdefines.h"
#include "fwanimation/clipsets.h"
#include "fwscript/scripthandler.h"
#include "streaming/streamingdefs.h"

// Game includes
#include "frontend/MiniMapCommon.h"
#include "modelinfo/pedmodelinfo.h"
#include "scene/volume/volume.h"
#include "script/script_memory_tracking.h"

namespace rage
{
class strStreamingModule;
class fwIplPatrolNode;
class fwIplPatrolLink;
class ropeInstance;
};

class CInteriorInst;

// These are a bit nasty, but make the creation of new resources easier and less error prone!

// COMMON_GAME_RESOURCE_DEF
//		Stick this in the declaration of a standard resource
// PARAMS:
//		classType - the class name
//		resourceType - the resource type (one of the enums declared in CGameScriptResource)
//		resourceName - a string containing the name of the resource. Used for debugging.
//		invalidReference - the value of an invalid reference for the resource (usually -1 or 0)
#define COMMON_GAME_RESOURCE_DEF(className, resourceType, resourceName, invalidReference)\
	COMMON_RESOURCE_DEF(className, CGameScriptResource, resourceType, resourceName, invalidReference)\

// Same as COMMON_GAME_RESOURCE_DEF but will cache returned sizes on first call
#define COMMON_GAME_RESOURCE_CACHED_SIZE_DEF( className, resourceType, resourceName, invalidReference)\
	COMMON_RESOURCE_DEF(className, CGameScriptResourceWithCachedSize, resourceType, resourceName, invalidReference)

// COMMON_STREAMING_RESOURCE_DEF
//		Stick this in the declaration of a streaming resource (a resource derived from CScriptStreamingResource)
// PARAMS:
//		classType - the class name
//		resourceType - the resource type (one of the enums declared in CGameScriptResource)
//		resourceName - a string containing the name of the resource. Used for debugging.
#define COMMON_STREAMING_RESOURCE_DEF(className, resourceType, resourceName)\
	COMMON_RESOURCE_DEF(className, CScriptStreamingResource, resourceType, resourceName, INVALID_STREAMING_REF)\


#define __REPORT_TOO_MANY_RESOURCES		!__FINAL

///////////////////////////////////////////
// BASECLASS 
///////////////////////////////////////////
class CGameScriptResource :  public scriptResource
{
public:

	enum 
	{
		SCRIPT_RESOURCE_PTFX,
		SCRIPT_RESOURCE_PTFX_ASSET,
		SCRIPT_RESOURCE_FIRE,
		SCRIPT_RESOURCE_PEDGROUP,
		SCRIPT_RESOURCE_SEQUENCE_TASK,
		SCRIPT_RESOURCE_DECISION_MAKER,
		SCRIPT_RESOURCE_CHECKPOINT,
		SCRIPT_RESOURCE_TEXTURE_DICTIONARY,
		SCRIPT_RESOURCE_DRAWABLE_DICTIONARY,
		SCRIPT_RESOURCE_CLOTH_DICTIONARY,
		SCRIPT_RESOURCE_FRAG_DICTIONARY,
		SCRIPT_RESOURCE_DRAWABLE,
		SCRIPT_RESOURCE_COVERPOINT,
		SCRIPT_RESOURCE_ANIMATION,
		SCRIPT_RESOURCE_MODEL,
		SCRIPT_RESOURCE_RADAR_BLIP,
		SCRIPT_RESOURCE_ROPE,
		SCRIPT_RESOURCE_CAMERA,
		SCRIPT_RESOURCE_PATROL_ROUTE,
		SCRIPT_RESOURCE_MLO,
		SCRIPT_RESOURCE_RELATIONSHIP_GROUP,
		SCRIPT_RESOURCE_SCALEFORM_MOVIE,
		SCRIPT_RESOURCE_STREAMED_SCRIPT,
		SCRIPT_RESOURCE_ITEMSET,
        SCRIPT_RESOURCE_VOLUME,
		SCRIPT_RESOURCE_SPEED_ZONE,
		SCRIPT_RESOURCE_WEAPON_ASSET,
		SCRIPT_RESOURCE_VEHICLE_ASSET,
		SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE,
        SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL,
		SCRIPT_RESOURCE_SCENARIO_BLOCKING_AREA,
		SCRIPT_RESOURCE_BINK_MOVIE,
		SCRIPT_RESOURCE_MOVIE_MESH_SET,
		SCRIPT_RESOURCE_SET_REL_GROUP_DONT_AFFECT_WANTED_LEVEL,
		SCRIPT_RESOURCE_VEHICLE_COMBAT_AVOIDANCE_AREA,
		SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS,
		SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER,
		SCRIPT_RESOURCE_SYNCED_SCENE,
		SCRIPT_RESOURCE_VEHICLE_RECORDING,
		SCRIPT_RESOURCE_MOVEMENT_MODE_ASSET,
		SCRIPT_RESOURCE_CUT_SCENE,
		SCRIPT_RESOURCE_CUT_FILE,
		SCRIPT_RESOURCE_CLIP_SET,
		SCRIPT_RESOURCE_GHOST_SETTINGS,
		SCRIPT_RESOURCE_PICKUP_GENERATION_MULTIPLIER,
		SCRIPT_RESOURCE_MAX
	};

//	static unsigned const MAX_NUM_SCRIPT_RESOURCES		= 400;			//	Put back to 256 once there are new commands to handle all of the 1000 new static blips as a single resource.
																		//	We'll maybe have one resource for each script that creates 1 or more static blips

	static unsigned const MAX_SIZE_EXCESS				=
#if __SCRIPT_MEM_CALC
															// Cache: u32 virtual, u32 physical, bool dirty, + 1 extra - protected by CompileTimeAsserts.
#if RSG_CPU_X64
															24; // (3*8 with padding)
#else
															16;	// (4*4)
#endif
#else
														0;
#endif
	static unsigned const MAX_SIZE_OF_SCRIPT_RESOURCE	= sizeof(scriptResource) + MAX_SIZE_EXCESS;

#if __REPORT_TOO_MANY_RESOURCES
	static s32 ms_MaximumExpectedNumberOfResources[SCRIPT_RESOURCE_MAX];
	static s32 ms_CurrentNumberOfUsedResources[SCRIPT_RESOURCE_MAX];
	static s32 ms_MaximumNumberOfResourcesSoFar[SCRIPT_RESOURCE_MAX];

#if __BANK
	static void DisplayResourceUsage(bool bOutputToScreen);
#endif	//	__BANK

	void IncrementNumberOfUsedResources();
	void DecrementNumberOfUsedResources();
#endif	//	__REPORT_TOO_MANY_RESOURCES

public:

	CGameScriptResource(ScriptResourceType type, ScriptResourceRef reference);
	virtual ~CGameScriptResource();

	FW_REGISTER_CLASS_POOL(CGameScriptResource);

	// creation is done in the constructors.
	virtual bool Create() { return m_Reference != GetInvalidReference(); }

	virtual strIndex GetStreamingIndex() const { return strIndex(strIndex::INVALID_INDEX); }

	static const char* GetName(const int resourceType);

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

#if __SCRIPT_MEM_CALC
	virtual void CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag) const;
#endif	//	__SCRIPT_MEM_CALC

#if __BANK
	virtual void DisplayDebugInfo(const char* scriptName) const;
	virtual void SpewDebugInfo(const char* scriptName) const;
#endif
};

//////////////////////////////////////////////////////////////////////////////
// CGameScriptResourceWithCachedSize
//
// Alternative cached variation of base class CGameScriptResource - keeps CalculateMemoryUsage cheap.
// CalculateMemoryUsage continues to query CalculateMemoryUsageForCache until it returns true
// After that point, it just returns cached physical and virtual sizes.
// If you need extra info for DisplayMemoryUsage, store it at top-most class level from CalculateMemoryUsageForCache
//////////////////////////////////////////////////////////////////////////////

class CGameScriptResourceWithCachedSize : public CGameScriptResource
{
public:
	CGameScriptResourceWithCachedSize(ScriptResourceType type, ScriptResourceRef reference) : CGameScriptResource(type, reference)

#if __SCRIPT_MEM_CALC
		, m_isCached(false), 
		m_CachedNonStreamingPhysicalForThisResource(0),
		m_CachedNonStreamingVirtualForThisResource(0) 
#endif	//__SCRIPT_MEM_CALC
	{}

#if __SCRIPT_MEM_CALC

	virtual void CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag) const;

	// Return false if we can't cache yet.
	virtual bool CalculateMemoryUsageForCache(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag ) const = 0;

	virtual void DisplayMemoryUsage(u32 NonStreamingVirtualForThisResource, u32 NonStreamingPhysicalForThisResource, const char *scriptName, 
									bool bDisplayScriptDetails) const = 0;

private:

	inline bool IsCached() const
	{
		return m_isCached;
	}

	// Mutable because CalculateMemoryUsage should by rights be const and this is just an optimisation.
	mutable bool	m_isCached;
	mutable u32		m_CachedNonStreamingPhysicalForThisResource;
	mutable u32		m_CachedNonStreamingVirtualForThisResource;

#endif	//__SCRIPT_MEM_CALC

};

CompileTimeAssert( sizeof(CGameScriptResourceWithCachedSize) <= CGameScriptResource::MAX_SIZE_OF_SCRIPT_RESOURCE );

///////////////////////////////////////////
// StreamingResource
///////////////////////////////////////////
class CScriptStreamingResource : public CGameScriptResource
{
public:

	static const ScriptResourceRef	INVALID_STREAMING_REF	= -1;

public:

	CScriptStreamingResource(ScriptResourceType resourceType, strStreamingModule& streamingModule, u32 index, u32 streamingFlags); 

	CScriptStreamingResource(ScriptResourceType type, ScriptResourceRef reference) : CGameScriptResource(type, reference) {}

	virtual strStreamingModule& GetStreamingModule() const = 0;

	virtual void Destroy();
	virtual bool LeaveForOtherScripts() const { return true; }

	virtual strIndex GetStreamingIndex() const;

#if __SCRIPT_MEM_CALC
	virtual void CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag) const;
#endif	//	__SCRIPT_MEM_CALC

#if __DEV
	virtual void DisplayDebugInfo(const char* scriptName) const;
	virtual void SpewDebugInfo(const char* scriptName) const;
#endif
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_PTFX
///////////////////////////////////////////
class CScriptResource_PTFX : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_PTFX, SCRIPT_RESOURCE_PTFX, "Ptfx", 0);

	CScriptResource_PTFX(const char* pFxName, atHashWithStringNotFinal usePtFxAssetHashName, class CEntity* pParentEntity, int boneTag, const Vector3& fxPos, const Vector3& fxRot, float scale, u8 invertAxes); 
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_PTFX_ASSET
///////////////////////////////////////////
class CScriptResource_PTFX_Asset : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_PTFX_Asset, SCRIPT_RESOURCE_PTFX_ASSET, "Ptfx Asset");

	CScriptResource_PTFX_Asset(u32 index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_FIRE
///////////////////////////////////////////
class CScriptResource_Fire : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Fire, SCRIPT_RESOURCE_FIRE, "Fire", -1);

	CScriptResource_Fire(const Vector3& vecPos, int numGenerations, bool isPetrolFire);
	virtual void Destroy();
	virtual void Detach();
	virtual bool DetachOnCleanup() const { return true; }
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_VOLUME
///////////////////////////////////////////
#if VOLUME_SUPPORT
class CScriptResource_Volume : public CGameScriptResource
{
public:

    COMMON_GAME_RESOURCE_DEF(CScriptResource_Volume, SCRIPT_RESOURCE_VOLUME, "Volume", -1);

    CScriptResource_Volume(class CVolume* pVolume);
    virtual void Destroy();
};
#endif // VOLUME_SUPPORT

///////////////////////////////////////////
// SCRIPT_RESOURCE_PEDGROUP
///////////////////////////////////////////
class CScriptResource_PedGroup : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_PedGroup, SCRIPT_RESOURCE_PEDGROUP, "Ped Group", -1);

	CScriptResource_PedGroup(class CGameScriptHandler* pScriptHandler);
	CScriptResource_PedGroup(u32 existingGroupSlot);
	virtual void Destroy();

	virtual bool LeaveForOtherScripts() const;
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_SEQUENCE_TASK
///////////////////////////////////////////
class CScriptResource_SequenceTask : public CGameScriptResource
{
public:
	// sequences have their own id system, so that the ids match across the network when the order of sequences is defined in the same 
	// order by the script, irrespective of other resource allocations.

	static const unsigned SEQUENCE_RESOURCE_ID_SHIFT = 16;	
	static const unsigned SIZEOF_SEQUENCE_ID = 15; 
	static const unsigned MAX_NUM_SEQUENCES_PERMITTED = (1<<SIZEOF_SEQUENCE_ID)-1;

	static ScriptResourceId IsValidResourceId(ScriptResourceId resourceId) { return (resourceId >= (1<<SEQUENCE_RESOURCE_ID_SHIFT) && resourceId < (1<<(SEQUENCE_RESOURCE_ID_SHIFT+SIZEOF_SEQUENCE_ID))); }
	static ScriptResourceId GetResourceIdFromSequenceId(ScriptResourceId seqId) { return seqId<<SEQUENCE_RESOURCE_ID_SHIFT; }
	static ScriptResourceId GetSequenceIdFromResourceId(ScriptResourceId resourceId) { return resourceId>>SEQUENCE_RESOURCE_ID_SHIFT; }

public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_SequenceTask, SCRIPT_RESOURCE_SEQUENCE_TASK, "Sequence Task", -1);

	CScriptResource_SequenceTask();
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_DECISION_MAKER
///////////////////////////////////////////
class CScriptResource_DecisionMaker : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_DecisionMaker, SCRIPT_RESOURCE_DECISION_MAKER, "Decision Maker", 0);

	CScriptResource_DecisionMaker(s32 iSourceDecisionMakerId, const char* pNewName);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_CHECKPOINT
///////////////////////////////////////////
class CScriptResource_Checkpoint : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Checkpoint, SCRIPT_RESOURCE_CHECKPOINT, "Checkpoint", -1);

	CScriptResource_Checkpoint(int CheckpointType, const Vector3& vecPosition, const Vector3& vecPointAt, float fSize, int colR, int colG, int colB, int colA, int num);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_TEXTURE_DICTIONARY
///////////////////////////////////////////
class CScriptResource_TextureDictionary : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_TextureDictionary, SCRIPT_RESOURCE_TEXTURE_DICTIONARY, "Texture Dictionary");

	CScriptResource_TextureDictionary(strLocalIndex index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_DRAWABLE_DICTIONARY
///////////////////////////////////////////
class CScriptResource_DrawableDictionary : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_DrawableDictionary, SCRIPT_RESOURCE_DRAWABLE_DICTIONARY, "Drawable Dictionary");

	CScriptResource_DrawableDictionary(strLocalIndex index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_CLOTH_DICTIONARY
///////////////////////////////////////////
class CScriptResource_ClothDictionary : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_ClothDictionary, SCRIPT_RESOURCE_CLOTH_DICTIONARY, "Cloth Dictionary");

	CScriptResource_ClothDictionary(strLocalIndex index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_FRAG_DICTIONARY
///////////////////////////////////////////
class CScriptResource_FragDictionary : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_FragDictionary, SCRIPT_RESOURCE_FRAG_DICTIONARY, "Frag Dictionary");

	CScriptResource_FragDictionary(strLocalIndex index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_DRAWABLE
///////////////////////////////////////////
class CScriptResource_Drawable : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_Drawable, SCRIPT_RESOURCE_DRAWABLE, "Drawable");

	CScriptResource_Drawable(strLocalIndex index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_COVERPOINT
///////////////////////////////////////////
class CScriptResource_Coverpoint : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Coverpoint, SCRIPT_RESOURCE_COVERPOINT, "Coverpoint", -1);

	CScriptResource_Coverpoint(const Vector3& vCoors, float fDirection, s32 iUsage, s32 iHeight, s32 iArc, bool bIsPriority);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_ANIMATION
///////////////////////////////////////////
class CScriptResource_Animation : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_Animation, SCRIPT_RESOURCE_ANIMATION, "Animation");

	CScriptResource_Animation(u32 index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;
	
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_CLIPSET
///////////////////////////////////////////
class CScriptResource_ClipSet : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_ClipSet, SCRIPT_RESOURCE_CLIP_SET, "ClipSet", -1);

	CScriptResource_ClipSet(fwMvClipSetId clipSetId);

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_VEHICLE_RECORDING
///////////////////////////////////////////
class CScriptResource_VehicleRecording : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_VehicleRecording, SCRIPT_RESOURCE_VEHICLE_RECORDING, "Vehicle Recording");

	CScriptResource_VehicleRecording(u32 index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_CUT_SCENE
///////////////////////////////////////////
class CScriptResource_CutScene : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_CutScene, SCRIPT_RESOURCE_CUT_SCENE, "CutScene", 0);

	CScriptResource_CutScene(u32 cutSceneNameHash);

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_CUT_FILE
///////////////////////////////////////////
class CScriptResource_CutFile : public CScriptStreamingResource
{
public:

	COMMON_STREAMING_RESOURCE_DEF(CScriptResource_CutFile, SCRIPT_RESOURCE_CUT_FILE, "CutFile");

	CScriptResource_CutFile(s32 index, u32 streamingFlags);

	virtual strStreamingModule& GetStreamingModule() const;
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_SYNCED_SCENE
///////////////////////////////////////////
class CScriptResource_SyncedScene : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_SyncedScene, SCRIPT_RESOURCE_SYNCED_SCENE, "SyncedScene", -1);

	CScriptResource_SyncedScene(fwSyncedSceneId sceneId);

	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_MODEL
///////////////////////////////////////////
class CScriptResource_Model : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Model, SCRIPT_RESOURCE_MODEL, "Model", 0);

	CScriptResource_Model(u32 modelHashkey, u32 streamingFlags);

	virtual void Destroy();
	virtual bool LeaveForOtherScripts() const { return true; }

	virtual strIndex GetStreamingIndex() const;
	virtual strStreamingModule& GetStreamingModule() const;

#if __SCRIPT_MEM_CALC
	virtual void CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag) const;
#endif	//	__SCRIPT_MEM_CALC

#if __DEV
	virtual void DisplayDebugInfo(const char* scriptName) const;
	virtual void SpewDebugInfo(const char* scriptName) const;
#endif
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_RADAR_BLIP
///////////////////////////////////////////
class CScriptResource_RadarBlip : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_RadarBlip, SCRIPT_RESOURCE_RADAR_BLIP, "Radar Blip", 0);

	CScriptResource_RadarBlip(u32 iBlipType, const CEntityPoolIndexForBlip& poolIndex, u32 iDisplayFlag, const char *pScriptName, float scale = 0.0f );
	CScriptResource_RadarBlip(u32 iBlipType, const Vector3& vPosition, u32 iDisplayFlag, const char *pScriptName, float scale = 0.0f );
	CScriptResource_RadarBlip(s32 blipIndex);
	virtual void Destroy();
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_ROPE
///////////////////////////////////////////
class CScriptResource_Rope : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Rope, SCRIPT_RESOURCE_ROPE, "Rope", 0);

	CScriptResource_Rope(Vec3V_In pos, Vec3V_In rot, float initLength, float minLength, float maxLength, float lengthChangeRate, int ropeType, bool ppuOnly, bool collisionOn, bool lockFromFront, float timeMultiplier, bool breakable, bool pinned );
	CScriptResource_Rope(ropeInstance* pRope);
	virtual void Destroy();
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_CAMERA
///////////////////////////////////////////
class CScriptResource_Camera : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Camera, SCRIPT_RESOURCE_CAMERA, "Camera", -1);

	CScriptResource_Camera(const char* CameraName, bool bActivate);
	CScriptResource_Camera(u32 CameraName, bool bActivate); 
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_PATROL_ROUTE
///////////////////////////////////////////
class CScriptResource_PatrolRoute : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_PatrolRoute, SCRIPT_RESOURCE_PATROL_ROUTE, "Patrol Route", 0);

	CScriptResource_PatrolRoute(u32 patrolRouteHash, fwIplPatrolNode* aPatrolNodes, s32* aOptionalNodeIds, s32 iNumNodes, 
								fwIplPatrolLink* aPatrolLinks, s32 iNumLinks, s32 iNumGuardsPerRoute );
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_MLO
///////////////////////////////////////////

class CScriptResource_MLO : public CGameScriptResourceWithCachedSize
{
public:

	COMMON_GAME_RESOURCE_CACHED_SIZE_DEF(CScriptResource_MLO, SCRIPT_RESOURCE_MLO, "MLO", -1);

	CScriptResource_MLO(s32 InteriorProxyIndex);
	virtual void Destroy();

#if __SCRIPT_MEM_CALC
	virtual bool CalculateMemoryUsageForCache(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
			u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
			bool bDisplayScriptDetails, u32 skipFlag) const;

	virtual void DisplayMemoryUsage(u32 NonStreamingVirtualForThisResource, u32 NonStreamingPhysicalForThisResource, const char *scriptName, bool bDisplayScriptDetails) const;

	mutable u32		m_CachedModelIdxArrayCount;
#endif	//	__SCRIPT_MEM_CALC

};

CompileTimeAssert( sizeof(CScriptResource_MLO) <= CGameScriptResource::MAX_SIZE_OF_SCRIPT_RESOURCE );

///////////////////////////////////////////
// SCRIPT_RESOURCE_RELATIONSHIP_GROUP
///////////////////////////////////////////
class CScriptResource_RelGroup : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_RelGroup, SCRIPT_RESOURCE_RELATIONSHIP_GROUP, "Relationship Group", 0);

	CScriptResource_RelGroup(const char* szGroupName);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_SCALEFORM_MOVIE
///////////////////////////////////////////
class CScriptResource_ScaleformMovie : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_ScaleformMovie, SCRIPT_RESOURCE_SCALEFORM_MOVIE, "Scaleform Movie", 0);

	CScriptResource_ScaleformMovie(s32 iNewId, const char *pFilename);
	virtual void Destroy();
	virtual bool LeaveForOtherScripts() const { return true; }
#if __SCRIPT_MEM_CALC
	virtual strIndex GetStreamingIndex() const;
	virtual strLocalIndex GetLocalStreamingIndex() const;
	
	virtual void CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag) const;
#endif	//	__SCRIPT_MEM_CALC
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_STREAMED_SCRIPT
///////////////////////////////////////////
class CScriptResource_StreamedScript : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_StreamedScript, SCRIPT_RESOURCE_STREAMED_SCRIPT, "Streamed Script", -1);

	CScriptResource_StreamedScript(s32 StreamedScriptIndex);
	virtual void Destroy();
	virtual bool LeaveForOtherScripts() const { return true; }
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_ITEMSET
///////////////////////////////////////////
class CScriptResource_ItemSet : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_ItemSet, SCRIPT_RESOURCE_ITEMSET, "Item Set", 0);

	explicit CScriptResource_ItemSet(bool autoClean);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_SPEED_ZONE
///////////////////////////////////////////
class CScriptResource_SpeedZone : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_SpeedZone, SCRIPT_RESOURCE_SPEED_ZONE, "Speed Zone", -1);
	CScriptResource_SpeedZone(Vector3& vSphereCenter, float fSphereRadius, float fMaxSpeed, bool bAllowAffectMissionVehs);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_SCENARIO_BLOCKING_AREA
///////////////////////////////////////////
class CScriptResource_ScenarioBlockingArea : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_ScenarioBlockingArea, SCRIPT_RESOURCE_SCENARIO_BLOCKING_AREA, "Scenario Blocking Area", 0 /*CScenarioBlockingAreas::kScenarioBlockingAreaIdInvalid*/);
	CScriptResource_ScenarioBlockingArea(Vector3& vMin, Vector3& vMax, bool bCancelActive, bool bBlocksPeds, bool bBlocksVehicles);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_WEAPON_ASSET
///////////////////////////////////////////
class CScriptResource_Weapon_Asset : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Weapon_Asset, SCRIPT_RESOURCE_WEAPON_ASSET, "Weapon Asset", -1);
	CScriptResource_Weapon_Asset(const s32 iWeaponHash, s32 iRequestFlags, s32 iExtraComponentFlags);
	virtual void Destroy();
	virtual bool LeaveForOtherScripts() const { return true; }
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_VEHICLE_ASSET
///////////////////////////////////////////
class CScriptResource_Vehicle_Asset : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_Vehicle_Asset, SCRIPT_RESOURCE_VEHICLE_ASSET, "Vehicle Asset", -1);
	CScriptResource_Vehicle_Asset(const s32 iVehicleModelHash, s32 iRequestFlags);
	virtual void Destroy();
	virtual bool LeaveForOtherScripts() const { return true; }
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE
///////////////////////////////////////////
class CScriptResource_PopScheduleOverride : public CGameScriptResource
{
public:
	COMMON_GAME_RESOURCE_DEF(CScriptResource_PopScheduleOverride, SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE, "PopSchedule Override", -1);
	CScriptResource_PopScheduleOverride(const s32 schedule, u32 popGroup, int percentage);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL
///////////////////////////////////////////
class CScriptResource_PopScheduleVehicleModelOverride : public CGameScriptResource
{
public:
	COMMON_GAME_RESOURCE_DEF(CScriptResource_PopScheduleVehicleModelOverride, SCRIPT_RESOURCE_POPSCHEDULE_OVERRIDE_VEHICLE_MODEL, "PopSchedule Override Vehicle Model", fwModelId::MI_INVALID);
	CScriptResource_PopScheduleVehicleModelOverride(const s32 schedule, u32 modelIndex);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_BINK_MOVIE
///////////////////////////////////////////
class CScriptResource_BinkMovie : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_BinkMovie, SCRIPT_RESOURCE_BINK_MOVIE, "Bink Movie", 0);

	CScriptResource_BinkMovie(const char *pMovieName);

	virtual void Destroy();

	//	CMovie has its own m_refCount that must be decremented every time a movie is freed by RemoveScriptResource()
	//	so LeaveForOtherScripts() should return false
	virtual bool LeaveForOtherScripts() const { return false; }

#if __SCRIPT_MEM_CALC
	virtual void CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag) const;
#endif	//	__SCRIPT_MEM_CALC
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_MOVIE_MESH_SET
///////////////////////////////////////////
class CScriptResource_MovieMeshSet : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_MovieMeshSet, SCRIPT_RESOURCE_MOVIE_MESH_SET, "Movie Mesh Set", 0);

	CScriptResource_MovieMeshSet(const char* pSetName);

	virtual void Destroy();

//	After speaking to Luis, I've changed this to return false (as CScriptResource_BinkMovie does).
//	This is to ensure that CMovie::DecRefCount() is called every time.
//	I'm not sure how likely it is that two scripts will request the same MovieMeshSet at the same time so this
//	situation might never arise.
	virtual bool LeaveForOtherScripts() const { return false; }

#if __SCRIPT_MEM_CALC
	virtual void CalculateMemoryUsage(const char* scriptName, atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &NonStreamingVirtualForThisResource, u32 &NonStreamingPhysicalForThisResource,
		bool bDisplayScriptDetails, u32 skipFlag) const;
#endif	//	__SCRIPT_MEM_CALC
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_SET_REL_GROUP_AFFECTS_WANTED_LEVEL
///////////////////////////////////////////
class CScriptResource_SetRelGroupDontAffectWantedLevel : public CGameScriptResource
{
public:
	COMMON_GAME_RESOURCE_DEF(CScriptResource_SetRelGroupDontAffectWantedLevel, SCRIPT_RESOURCE_SET_REL_GROUP_DONT_AFFECT_WANTED_LEVEL, "Relationship group WL influence Override", -1);
	CScriptResource_SetRelGroupDontAffectWantedLevel(int iRelGroup);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_VEHICLE_COMBAT_AVOIDANCE_AREA
///////////////////////////////////////////
class CScriptResource_VehicleCombatAvoidanceArea : public CGameScriptResource
{
public:
	COMMON_GAME_RESOURCE_DEF(CScriptResource_VehicleCombatAvoidanceArea, SCRIPT_RESOURCE_VEHICLE_COMBAT_AVOIDANCE_AREA, "Vehicle Combat Avoidance Area", 0);
	CScriptResource_VehicleCombatAvoidanceArea(Vec3V_In vStart, Vec3V_In vEnd, float fWidth);
	CScriptResource_VehicleCombatAvoidanceArea(Vec3V_In vCenter, float fRadius);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS
///////////////////////////////////////////
class CScriptResource_DispatchTimeBetweenSpawnAttempts : public CGameScriptResource
{
public:
	COMMON_GAME_RESOURCE_DEF(CScriptResource_DispatchTimeBetweenSpawnAttempts, SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS, "Dispatch Time Between Spawn Attempts", -1);
	CScriptResource_DispatchTimeBetweenSpawnAttempts(int iType, float fTimeBetweenSpawnAttempts);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER
///////////////////////////////////////////
class CScriptResource_DispatchTimeBetweenSpawnAttemptsMultiplier : public CGameScriptResource
{
public:
	COMMON_GAME_RESOURCE_DEF(CScriptResource_DispatchTimeBetweenSpawnAttemptsMultiplier, SCRIPT_RESOURCE_DISPATCH_TIME_BETWEEN_SPAWN_ATTEMPTS_MULTIPLIER, "Dispatch Time Between Spawn Attempts Multiplier", -1);
	CScriptResource_DispatchTimeBetweenSpawnAttemptsMultiplier(int iType, float fTimeBetweenSpawnAttemptsMultiplier);
	virtual void Destroy();
};

///////////////////////////////////////////
// SCRIPT_RESOURCE_ACTION_MOED_ASSET
///////////////////////////////////////////
class CScriptResource_MovementMode_Asset : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_MovementMode_Asset, SCRIPT_RESOURCE_MOVEMENT_MODE_ASSET, "MovementMode Asset", -1);
	CScriptResource_MovementMode_Asset(const s32 iMovementModeHash, const CPedModelInfo::PersonalityMovementModes::MovementModes Type);
	virtual void Destroy();
	virtual bool LeaveForOtherScripts() const { return true; }
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_GHOST_SETTINGS
///////////////////////////////////////////
class CScriptResource_GhostSettings : public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_GhostSettings, SCRIPT_RESOURCE_GHOST_SETTINGS, "Ghost Settings", -1);
	CScriptResource_GhostSettings();
	virtual void Destroy();
};


///////////////////////////////////////////
// SCRIPT_RESOURCE_PICKUP_GENERATION_MULTIPLIER
///////////////////////////////////////////
class CScriptResource_PickupGenerationMultiplier: public CGameScriptResource
{
public:

	COMMON_GAME_RESOURCE_DEF(CScriptResource_PickupGenerationMultiplier, SCRIPT_RESOURCE_PICKUP_GENERATION_MULTIPLIER, "Pickup generation multiplier", -1);
	CScriptResource_PickupGenerationMultiplier();
	virtual void Destroy();
};

#endif // GAME_SCRIPT_RESOURCES_H
