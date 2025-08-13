// Title	:	Entity.cpp
// Author	:	Richard Jobling
// Started	:	23/10/98

#define ENABLE_VEHICLE_CB		(1)
 
#include "scene/entity.h"

// Rage Headers
#include "audioengine/environmentgroup.h"
#include "cloth/charactercloth.h"
#include "grcore/debugdraw.h"
#include "grmodel/shadersorter.h"
#include "rmcore/lodgroup.h"
#include "file/diskcache.h"
#include "fragment/type.h"
#include "fragment/drawable.h"
#include "physics/inst.h"
#include "rmptfx/ptxmanager.h"
#include "string/stringutil.h"
#include "system/memory.h"
#include "system/nelem.h"

// Framework headers
#include "vfx/channel.h"
#include "vfx/decal/decalmanager.h"
#include "fwutil/idgen.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "fwscene/lod/LodAttach.h"
#include "fwdebug/picker.h"
#include "fwanimation/directorcomponentcreature.h"

// Game Headers
#include "animation/MoveObject.h"
#include "animation/MovePed.h"
#include "animation/debug/AnimViewer.h"
#include "audio/emitteraudioentity.h"
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "control/trafficlights.h"
#include "control/replay/replay.h"
#include "control/replay/ReplayHideManager.h"
#include "control/replay/Effects/ParticleEntityFxPacket.h"
#include "core/game.h"
#include "debug/debugscene.h"
#include "debug/DebugRecorder.h"
#include "debug/vectormap.h"
#include "game/weather.h"
#include "modelinfo/basemodelinfo.h"
#include "modelinfo/PedModelInfo.h"
#include "network/NetworkInterface.h"
#include "network/general/NetworkUtil.h"
#include "objects/dummyobject.h"
#include "objects/object.h"
#include "objects/procobjects.h"
#include "objects/Door.h"
#include "pathserver/pathserver.h"
#include "pathserver/ExportCollision.h"
#include "peds/AssistedMovementStore.h"
#include "peds/ped.h"
#include "peds/PlayerInfo.h"
#include "physics/breakable.h"
#include "physics/gtainst.h"
#include "physics/physics.h"
#include "renderer/clip_stat.h"
#include "renderer/DrawLists/drawlistmgr.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/renderer.h"
#include "renderer/lights/lights.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/PostScan.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h" // for WATER_REFLECTION_PRE_REFLECTED define ..
#include "renderer/renderthread.h"
#include "renderer/River.h"
#include "scene/animatedbuilding.h"
#include "scene/EntityBatch.h"
#include "scene/entities/compEntity.h"
#include "fwscene/stores/mapdatastore.h"
#include "shaders/CustomShaderEffectCable.h"
#include "shaders/CustomShaderEffectTint.h"
#include "scene/lod/LodScale.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/loader/mapdata_entities.h"
#include "Scene/portals/Portal.h"
#include "Scene/portals/PortalInst.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/gameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/world/WorldDebugHelper.h"
#include "scene/scene_channel.h"
#include "script/script.h"
#include "shaders/shaderlib.h"
#include "streaming/defragmentation.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "TimeCycle/TimeCycle.h"
#include "timeCycle/TimeCycleAsyncQueryMgr.h"
#include "vehicleAi/task/DeferredTasks.h"
#include "vehicles/vehicle.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Misc/Scrollbars.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxEntity.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/VfxHelper.h"
#include "vfx/vehicleglass/VehicleGlass.h" // for IsEntitySmashable
#include "vfx/vehicleglass/VehicleGlassComponent.h" // for IsEntitySmashable
#include "weapons/explosion.h"
#if __BANK
#include "Network/Objects/Entities/NetObjPlayer.h"
#endif
#if GTA_REPLAY
#include "control/replay/ReplayExtensions.h"
#endif

u32 CEntity::ms_DowntownIplIndex = 0;
u32 CEntity::ms_VinewoodIplIndex = 0;

AUTOID_IMPL(CIsVisibleExtension);

INSTANTIATE_RTTI_CLASS(CEntity,0xb1389614);

u32 CEntity::g_InstancePriority(3);
PARAM(instancePriority, "[game Entity] force the instance priority level");

PARAM(enableVisibilityFlagTracking, "Enable Visibility Flag Tracking");
PARAM(enableVisibilityDetailedSpew, "Enable Visibility Detailed Spew");
PARAM(enableVisibilityObsessiveSpew, "Enable Visibility Obsessive Spew");

atRangeArray<fwBasePool *, ENTITY_TYPE_TOTAL> CEntity::ms_EntityPools;

#if __BANK
bool CEntity::ms_cullProps = false;
bool CEntity::ms_renderOnlyProps = false;

struct NetworkVisFlagCallStack
{	
	enum { TRACE_DEPTH = 32};
	
	u16 m_objectId;
	CEntity *m_pEntity;
	size_t  m_trace[TRACE_DEPTH];
	eIsVisibleModule m_module;
	bool m_valid;
};
int g_networkPlayerObjectIdToTrace = -1;

enum {	VISFLAGCALLSTACK_COUNT = 24 };
static NetworkVisFlagCallStack gVisFlagCallstacks[VISFLAGCALLSTACK_COUNT];

void CEntity::ResetNetworkVisFlagCallStackData()
{
	if (PARAM_enableVisibilityFlagTracking.Get())
	{
		memset(gVisFlagCallstacks, 0,  sizeof(gVisFlagCallstacks));
	}
}

void CEntity::TraceNetworkVisFlagsRemovePlayer(u16 objectID)
{
	if (PARAM_enableVisibilityFlagTracking.Get())
	{
		for (int i = 0; i < VISFLAGCALLSTACK_COUNT; ++i)
		{
			if (gVisFlagCallstacks[i].m_valid && gVisFlagCallstacks[i].m_objectId == objectID)
			{
				memset(&gVisFlagCallstacks[i], 0, sizeof(NetworkVisFlagCallStack));
				break;
			}
		}
	}
}

static void TraceNetworkPlayerVisFlagsCallback()
{
	bool found = false;
	for (int i = 0; i < VISFLAGCALLSTACK_COUNT; ++i)
	{
		if (gVisFlagCallstacks[i].m_valid && gVisFlagCallstacks[i].m_objectId == g_networkPlayerObjectIdToTrace)
		{
			Displayf("Last call to SetIsVisibleForModule(false)");
			Displayf("CEntity pointer 0x%p, Module %d", gVisFlagCallstacks[i].m_pEntity, gVisFlagCallstacks[i].m_module);

			sysStack::PrintCapturedStackTrace(gVisFlagCallstacks[i].m_trace, NetworkVisFlagCallStack::TRACE_DEPTH, sysStack::DefaultDisplayLine);
			found = true;
			break;
		}
	}

	if	(!found)
	{
		Displayf("Network Player Id not found");
	}

}

void CEntity::AddInstancePriorityWidget()
{
	bkBank* pBankNetwork = BANKMGR.FindBank("Network");
	pBankNetwork->AddSlider("Network Player Index", &g_networkPlayerObjectIdToTrace, -1, 1000, 1);
	pBankNetwork->AddButton("Trace Network Players last call to SetIsVisibleForModule", datCallback(TraceNetworkPlayerVisFlagsCallback));
}

void CEntity::AddPriorityWidget(bkBank *pBank)
{
	pBank->AddToggle("Cull props", &ms_cullProps);
	pBank->AddToggle("Render only props", &ms_renderOnlyProps);
}
#endif //#if __BANK

#if !__NO_OUTPUT
static const char* s_moduleCaptions[ NUM_VISIBLE_MODULES ] =
{
	"DEBUG",
	"CAMERA",
	"SCRIPT",
	"CUTSCENE",
	"GAMEPLAY",
	"FRONTEND",
	"VFX",
	"NETWORK",
	"PHYSICS",
	"WORLD",
	"DUMMY_CONVERSION",
	"PLAYER",
	"PICKUP",
	"FIRST_PERSON",
	"VEHICLE_RESPOT",
	"RESPAWN",
	"REPLAY",
};
#endif

#if 1 
	PARAM(expandtolightbounds, "expand object bounds to attached lights bound");
#endif

//
//
// name:		CEntity::CEntity
// description:	constructor
CEntity::CEntity(const eEntityOwnedBy ownedBy) : fwEntity()
{
	m_ownedBy = ownedBy;

	fwEntity::ClearBaseFlag( fwEntity::IS_DYNAMIC );
	SetRetainedFlag(false);
	fwEntity::SetOwnerEntityContainer( NULL );

	m_type = ENTITY_TYPE_NOTHING;

	m_nFlags.bTimedEntityDelayedState = 0;

	m_nFlags.bRenderDamaged = false;

	m_nFlags.bUseAltFadeDistance = false;

	m_nFlags.bIsProcObject = false;
	m_nFlags.bIsCompEntityProcObject = false;
	m_nFlags.bIsFloatingProcObject = false;
#if __DEV
	m_nFlags.bIsEntityProcObject = false;
#endif 
	m_nFlags.bLightObject = false;

	m_nFlags.bIsFrag = false;
	m_nFlags.bInMloRoom = false;
	m_nFlags.bCreatedProcObjects = false;
	m_nFlags.bPossiblyTouchesWater = false;
	m_nFlags.bWillSpawnPeds = true;
	m_nFlags.bAlreadyInAudioList = 0;

	m_nFlags.bLightsIgnoreDayNightSetting = false;
	m_nFlags.bLightsCanCastStaticShadows = true;
	m_nFlags.bLightsCanCastDynamicShadows = true;

	m_nFlags.bHasExploded = false;
	m_nFlags.bIsOnFire = false;

	m_nFlags.bHasSpawnPoints = false;

	m_nFlags.bAddtoMotionBlurMask = false;

	m_nFlags.bAlwaysPreRender = false;
	m_nFlags.bNeverDummy = false;

	m_nFlags.bFoundInPvs = false;

	m_nFlags.bPreRenderedThisFrame = false;

	m_nFlags.bUpdatingThroughAnimQueue = false;

	m_nFlags.bUseOcclusionQuery = false;

	m_nFlags.bUseMaxDistanceForWaterReflection = false;

	m_nFlags.bCloseToCamera = false;

	m_iplIndex = 0;
	m_pDrawHandler = NULL;

	m_audioBin = 0xff;
	m_naturalAmbientScale = 255;
	m_artificialAmbientScale = 255;
	m_useAmbientScale = false;
	m_useDynamicAmbientScale = false;
	m_setupAmbientScaleFlags = true;
	m_calculateAmbientScales = true;

	m_tintIndex = 0;

#if STENCIL_VEHICLE_INTERIOR
	m_useVehicleInteriorMaterial = false;
#endif // STENCIL_VEHICLE_INTERIOR

	SetTrafficLightOverride(0x3);

	m_bIsInteriorStaticCollider = false;

	m_noWeaponDecals = false;

	m_baseDeferredType = DEFERRED_MATERIAL_DEFAULT;

	// clear the proxy flags
	m_phaseVisibilityMask.ClearFlag( (u16)~VIS_PHASE_MASK_ALL );

#if __BANK
	m_debugPriority = 0;
	m_debugIsFixed = 0;
#endif // __BANK	
}

CEntity::~CEntity()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	//////////////////////////////////////////////////////////////////////////
	// some entities are just placeholders for physics use - they are never added to the scene graph etc
	Assertf(
		CGameWorld::IsDeleteAllowed() || GetOwnedBy()==ENTITY_OWNEDBY_STATICBOUNDS || GetOwnedBy()==ENTITY_OWNEDBY_EXPLOSION,
		"Error: Entity destructor called whilst gbAllowDelete has been disabled"
	);	
	//////////////////////////////////////////////////////////////////////////

	Assert(!NetworkUtils::GetNetworkObjectFromEntity(this));

#if __DEV
	// check if a procedural object is being deleted when it shouldn't be
	if (m_nFlags.bIsProcObject)
	{
		bool bAllowedToDelete = g_procObjMan.IsAllowedToDelete() || CCompEntity::IsAllowedToDelete();
		Assertf(bAllowedToDelete, "Procedural Object (Material) %s(%d) being deleted by non procedural object code", GetBaseModelInfo()->GetModelName(), GetModelIndex());
	}
	if (m_nFlags.bIsEntityProcObject)
	{
		Assertf(g_procObjMan.IsAllowedToDelete(), "Procedural Object (Entity) being deleted by non procedural object code");
	}
#endif
#if __ASSERT
	if (GetIsDynamic())
	{
		CDynamicEntity *pDE = static_cast<CDynamicEntity*>(this);
		Assertf(pDE->m_nDEflags.bIsStraddlingPortal == 0 && pDE->GetPortalStraddlingContainerIndex() == 0xFF, "Entity is still straddling a portal");
	}
#endif //__ASSERT


	CGameWorld::Remove( this, false, true );
	Assert( !GetIsRetainedByInteriorProxy() && !GetOwnerEntityContainer() );		// don't delete things that are still in the world!

	// lodtree removal
	if (!GetLodData().IsOrphanHd())
	{
		fwLodAttach::RemoveEntityFromLodHierarchy(this);
	}

	DeleteDrawable();
	Assert(this);		// Make sure ResolveReferences didn't set the this pointer to NULL 
	ClearAllKnownRefs();
	Assert(this);		// Make sure ResolveReferences didn't set the this pointer to NULL. It looks like this is happening. (Obbe)
	DeleteInst(true);

	if (GetAlwaysPreRender())
	{
		gPostScan.RemoveFromAlwaysPreRenderList(this);
	}

	// keep SLOD3 / SLOD4 models resident until the entity is destroyed
	ReleaseImportantSlodAssets();

#if REGREF_VALIDATE_CDCDCDCD
	fwRefAwareBaseImpl_RemoveInterestingObject(this);
#endif
}

#if __ASSERT
static void AppendDontRenderInFlagsStrings(char* str, u32 dontRenderInFlags, u32 originalFlags, bool bIsArchetypeNoShadows)
{
	if (dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS)
	{
		strcat(str, "\n\t\"don't render in shadows\"");

		if (bIsArchetypeNoShadows)
			strcat(str, " (archetype flag)");
		if (originalFlags & fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS)
			strcat(str, " (instance flag)");
	}

	if (dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_REFLECTIONS)
		strcat(str, "\n\t\"don't render in reflections\" (paraboloid and mirror)");
	if (dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_WATER_REFLECTIONS)
		strcat(str, "\n\t\"don't render in water reflections\"");
	if (dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_MIRROR_REFLECTIONS)
		strcat(str, "\n\t\"don't render in mirror reflections\"");
}

static void AppendOnlyRenderInFlagsStrings(char* str, u32 onlyRenderInFlags, u32 originalFlags, bool bIsArchetypeShadowProxy)
{
	if (onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_SHADOWS)
	{
		strcat(str, "\n\t\"only render in shadows\"");

		if (bIsArchetypeShadowProxy)
			strcat(str, " (archetype flag)");
		if (originalFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_SHADOWS)
			strcat(str, " (instance flag)");
	}

	if (onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_REFLECTIONS)
		strcat(str, "\n\t\"only render in reflections\" (paraboloid and mirror)");
	if (onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_WATER_REFLECTIONS)
		strcat(str, "\n\t\"only render in water reflections\"");
	if (onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS)
		strcat(str, "\n\t\"only render in mirror reflections\"");
}
#endif // __ASSERT

void CEntity::InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex)
{
	fwEntity::InitEntityFromDefinition(definition, archetype, mapDataDefIndex);

	CEntityDef&			def = *smart_cast<CEntityDef*>(definition);
	CBaseModelInfo*		modelInfo = static_cast< CBaseModelInfo* >( archetype );

	const u32			loadFlags = def.m_flags;

	fwFlags16&			visibilityMask = GetRenderPhaseVisibilityMask();

	if(loadFlags & fwEntityDef::FLAG_DOESNOTTOUCHWATER)
		m_nFlags.bPossiblyTouchesWater = false;		// This guy will always be rendered as if above the water (even though its coordinates are below)

	if(loadFlags & fwEntityDef::FLAG_DOESNOTSPAWNPEDS)
		m_nFlags.bWillSpawnPeds = false;

	if(loadFlags & fwEntityDef::FLAG_DRAWABLELODUSEALTFADE)
		m_nFlags.bUseAltFadeDistance = true;

	u32 dontRenderInFlags = loadFlags & (fwEntityDef::FLAG_DONT_RENDER_IN_REFLECTIONS | fwEntityDef::FLAG_DONT_RENDER_IN_WATER_REFLECTIONS | fwEntityDef::FLAG_DONT_RENDER_IN_MIRROR_REFLECTIONS | fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS);
	u32 onlyRenderInFlags = loadFlags & (fwEntityDef::FLAG_ONLY_RENDER_IN_REFLECTIONS | fwEntityDef::FLAG_ONLY_RENDER_IN_WATER_REFLECTIONS | fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS | fwEntityDef::FLAG_ONLY_RENDER_IN_SHADOWS);

	// B*7385302: HACK: this model's load flags are set to not cast shadows. We can't change the assets at this point, so hasck it here.
	if(GetModelNameHash() == ATSTRINGHASH("prop_generator_03b", 0xFC96F411))
		dontRenderInFlags &= ~fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS;

	// set archetype flags
	if(modelInfo->GetDontCastShadows())
		dontRenderInFlags |= fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS;

	if(modelInfo->GetIsShadowProxy())
	{
		onlyRenderInFlags |= fwEntityDef::FLAG_ONLY_RENDER_IN_SHADOWS;
		visibilityMask.SetFlag( VIS_PHASE_MASK_SHADOW_PROXY );
	}

	if(dontRenderInFlags == 0 && onlyRenderInFlags == (fwEntityDef::FLAG_ONLY_RENDER_IN_WATER_REFLECTIONS | fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS))
	{
		// NOTE -- this is a hack for BS#1319522, so we can have an entity render only in water and
		// mirror reflections. a more general solution would allow for any combination, e.g. rendering in
		// water and paraboloid and shadow but not mirror reflections etc., but the logic would become
		// more complex. for next project we should probably change the way these flags are handled.
		visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_WATER_REFLECTION|VIS_PHASE_MASK_MIRROR_REFLECTION|VIS_PHASE_MASK_STREAMING)) );
	}
	else if(dontRenderInFlags == fwEntityDef::FLAG_DONT_RENDER_IN_REFLECTIONS && onlyRenderInFlags == fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS)
	{
		// NOTE -- this is a hack for BS#1236652, so we can have an entity not render in paraboloid
		// reflections but still continue to render in mirror reflections and gbuf/shadows/water.
		visibilityMask.ClearFlag( VIS_PHASE_MASK_PARABOLOID_REFLECTION );
	}
	else if(dontRenderInFlags == (fwEntityDef::FLAG_DONT_RENDER_IN_REFLECTIONS | fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS) && onlyRenderInFlags == fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS)
	{
		// NOTE -- this is a hack for BS#1366789, so we can have an entity not render in paraboloid
		// reflections or shadows but still continue to render in mirror reflections and gbuf/water.
		visibilityMask.ClearFlag( VIS_PHASE_MASK_PARABOLOID_REFLECTION|VIS_PHASE_MASK_SHADOWS );
	}
	else
	{
#if __ASSERT
		const bool bWarnAboutRedundantFlags = false; // we could turn this on if we want to fix these

		if(onlyRenderInFlags & (onlyRenderInFlags - 1))
		{
			char msg[512] = "";
			sprintf(msg, "%s has multiple \"only render in xxx\" flags (0x%08x)", definition->m_archetypeName.GetCStr(), onlyRenderInFlags);
			AppendOnlyRenderInFlagsStrings(msg, onlyRenderInFlags, loadFlags, modelInfo->GetIsShadowProxy());

			if(onlyRenderInFlags != (fwEntityDef::FLAG_ONLY_RENDER_IN_REFLECTIONS | fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS))
			{
				// inconsistent flags .. these need to be fixed
				Assertf(0, "%s", msg);
			}
			else if(bWarnAboutRedundantFlags)
			{
				// redundant flags ("only render in reflections" is redundant) .. not as bad but still kind of stupid
				Warningf("%s", msg);
			}
		}

		if(onlyRenderInFlags != 0 && dontRenderInFlags != 0)
		{
			const bool dontRenderInShadows = (dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS) != 0;
			const bool dontRenderInParaboloidReflection = (dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_REFLECTIONS) != 0;
			const bool dontRenderInMirrorReflection = (dontRenderInFlags & (fwEntityDef::FLAG_DONT_RENDER_IN_REFLECTIONS | fwEntityDef::FLAG_DONT_RENDER_IN_MIRROR_REFLECTIONS)) != 0;
			const bool dontRenderInWaterReflection = (dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_WATER_REFLECTIONS) != 0;

			const bool onlyRenderInShadows = (onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_SHADOWS) != 0;
			const bool onlyRenderInParaboloidReflection = (onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_REFLECTIONS) != 0;
			const bool onlyRenderInMirrorReflection = (onlyRenderInFlags & (fwEntityDef::FLAG_ONLY_RENDER_IN_REFLECTIONS | fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS)) != 0;
			const bool onlyRenderInWaterReflection = (onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_WATER_REFLECTIONS) != 0;

			char msg[512] = "";
			sprintf(msg, "%s has both \"only render in xxx\" flags (0x%08x) and \"don't render in xxx\" flags (0x%08x)", definition->m_archetypeName.GetCStr(), onlyRenderInFlags, dontRenderInFlags);
			AppendOnlyRenderInFlagsStrings(msg, onlyRenderInFlags, loadFlags, modelInfo->GetIsShadowProxy());
			AppendDontRenderInFlagsStrings(msg, dontRenderInFlags, loadFlags, modelInfo->GetDontCastShadows());

			if ((dontRenderInShadows && onlyRenderInShadows) ||
				(dontRenderInParaboloidReflection && onlyRenderInParaboloidReflection) ||
				(dontRenderInMirrorReflection && onlyRenderInMirrorReflection) ||
				(dontRenderInWaterReflection && onlyRenderInWaterReflection))
			{
				// inconsistent flags .. these need to be fixed
				Assertf(0, "%s", msg);
			}
			else if(bWarnAboutRedundantFlags)
			{
				// redundant flags ("don't render in xxx" is redundant) .. not as bad but still kind of stupid
				Warningf("%s", msg);
			}
		}
#endif // __ASSERT

		// set flags for shadows
		if(dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_SHADOWS)
			visibilityMask.ClearFlag( VIS_PHASE_MASK_SHADOWS );
		else if(onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_SHADOWS)
			visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_SHADOWS|VIS_PHASE_MASK_STREAMING)) );

		// set flags for paraboloid and mirror reflections
		if(dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_REFLECTIONS)
			visibilityMask.ClearFlag( VIS_PHASE_MASK_REFLECTIONS );
		else if(onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_REFLECTIONS)
			visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_REFLECTIONS|VIS_PHASE_MASK_STREAMING)) );

		// set flags for water reflection
		if(dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_WATER_REFLECTIONS)
			visibilityMask.ClearFlag( VIS_PHASE_MASK_WATER_REFLECTION );
		else if(onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_WATER_REFLECTIONS)
			visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_WATER_REFLECTION|VIS_PHASE_MASK_STREAMING)) );

		// set flags for mirror reflection
		if(dontRenderInFlags & fwEntityDef::FLAG_DONT_RENDER_IN_MIRROR_REFLECTIONS)
			visibilityMask.ClearFlag( VIS_PHASE_MASK_MIRROR_REFLECTION );
		else if(onlyRenderInFlags & fwEntityDef::FLAG_ONLY_RENDER_IN_MIRROR_REFLECTIONS)
			visibilityMask.ClearFlag( (u16)(~(VIS_PHASE_MASK_MIRROR_REFLECTION|VIS_PHASE_MASK_STREAMING)) );
	}

	// set shadow proxy flag
	if(visibilityMask.IsFlagSet( VIS_PHASE_MASK_SHADOWS ) && !visibilityMask.IsFlagSet( VIS_PHASE_MASK_GBUF ))
		visibilityMask.SetFlag( VIS_PHASE_MASK_SHADOW_PROXY );

	// set water reflection proxy flag
	if(visibilityMask.IsFlagSet( VIS_PHASE_MASK_WATER_REFLECTION ) && !visibilityMask.IsFlagSet( VIS_PHASE_MASK_GBUF ))
		visibilityMask.SetFlag( VIS_PHASE_MASK_WATER_REFLECTION_PROXY );

	// also set water reflection proxy flag if it's tagged as such on the archetype
	// this allows us to have a model with is a water reflection proxy but also has
	// a separate mesh which renders in gbuffer, shadows etc.
	if(modelInfo->GetHasDrawableProxyForWaterReflections())
	{
		visibilityMask.SetFlag( VIS_PHASE_MASK_WATER_REFLECTION_PROXY );

#if __ASSERT
		if(!visibilityMask.IsFlagSet( VIS_PHASE_MASK_WATER_REFLECTION ))
			Assertf(0, "%s is a water reflection proxy but does not render in water reflection", definition->m_archetypeName.GetCStr());
#endif // __ASSERT
	}

#if WATER_REFLECTION_PRE_REFLECTED
	if(modelInfo->GetHasPreReflectedWaterProxy())
	{
		visibilityMask.SetFlag( VIS_PHASE_MASK_WATER_REFLECTION_PROXY_PRE_REFLECTED );
	}

#if __ASSERT
	if(visibilityMask.IsFlagSet( VIS_PHASE_MASK_WATER_REFLECTION_PROXY_PRE_REFLECTED ))
	{
		if(!visibilityMask.IsFlagSet( VIS_PHASE_MASK_WATER_REFLECTION ))
			Assertf(0, "%s has a pre-reflected water proxy but is not visible in water reflection", definition->m_archetypeName.GetCStr());
		if(!visibilityMask.IsFlagSet( VIS_PHASE_MASK_WATER_REFLECTION_PROXY ))
			Warningf("%s has a pre-reflected water proxy but is not actually a water reflection proxy", definition->m_archetypeName.GetCStr());
	}
#endif // __ASSERT
#endif // WATER_REFLECTION_PRE_REFLECTED

	if(loadFlags & fwIplInstance::FLAG_LIGHTS_CAST_STATIC_SHADOWS)
		m_nFlags.bLightsCanCastStaticShadows = true;

	if(loadFlags & fwIplInstance::FLAG_LIGHTS_CAST_DYNAMIC_SHADOWS)
		m_nFlags.bLightsCanCastDynamicShadows = true;

	if(loadFlags & fwIplInstance::FLAG_LIGHTS_IGNORE_DAY_NIGHT_SETTINGS)
		m_nFlags.bLightsIgnoreDayNightSetting = true;

	SetTintIndex(def.m_tintValue);

	Assert(def.m_ambientOcclusionMultiplier < 256);
	Assert(def.m_artificialAmbientOcclusion < 256);

	SetNaturalAmbientScale(def.m_ambientOcclusionMultiplier);
	SetArtificialAmbientScale(def.m_artificialAmbientOcclusion);
	SetupAmbientScaleFlags();
	
	SetOwnedBy(ENTITY_OWNEDBY_IPL);
}

bool CEntity::GetIsAddedToWorld() const {
	return GetIsRetainedByInteriorProxy() || GetOwnerEntityContainer() != NULL;
}

bool CEntity::GetIsInExterior() const
{
	if ( GetIsRetainedByInteriorProxy() || GetOwnerEntityContainer() == NULL )
		return false;

	return ( GetOwnerEntityContainer()->GetOwnerSceneNode()->GetType() == SCENE_GRAPH_NODE_TYPE_EXTERIOR ||
			 GetOwnerEntityContainer()->GetOwnerSceneNode()->GetType() == SCENE_GRAPH_NODE_TYPE_STREAMED );
}

bool CEntity::GetShouldUseScreenDoor() const
{
	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	
	if(pModelInfo)
	{
		bool useScreenDoor = true;
		int type = GetType();
		switch(type)
		{
			//case ENTITY_TYPE_BUILDING:
			//case ENTITY_TYPE_ANIMATED_BUILDING:
			//case ENTITY_TYPE_VEHICLE:
			//case ENTITY_TYPE_PED:
			//case ENTITY_TYPE_COMPOSITE:
			//case ENTITY_TYPE_OBJECT:
			//case ENTITY_TYPE_DUMMY_OBJECT:
			//case ENTITY_TYPE_NOTINPOOLS:
			//default:
			//	useScreenDoor |= true;
			//	break;
			case ENTITY_TYPE_OBJECT:
				// Pickup are going through forward, so no screendoor for them
				useScreenDoor = !((CObject*)this)->IsPickup();
				break;
			case ENTITY_TYPE_PARTICLESYSTEM:
			case ENTITY_TYPE_LIGHT:
			case ENTITY_TYPE_PORTAL:
			case ENTITY_TYPE_MLO:
				useScreenDoor = false;
				break;
		}
	
		// Decals are only rendered in deferred, so those needs to be forced through screendoor.
		if( IsBaseFlagSet(fwEntity::HAS_DECAL) && IsBaseFlagSet(fwEntity::HAS_OPAQUE) && type != ENTITY_TYPE_OBJECT)
			useScreenDoor = true;

		return useScreenDoor;
	}
	
	return false;
}

bool CEntity::CanBeDeleted(void) const { return true; }
bool CEntity::RequiresProcessControl() const {return false;}
bool CEntity::ProcessControl() { Assertf(false, "Entity with modelindex %d and type %d doesn't have ProcessControl function", GetModelIndex(), GetType()); return false; }
bool CEntity::TestCollision(Matrix34* UNUSED_PARAM(pTestMat), const phInst** UNUSED_PARAM(ppExcludeList), u32 UNUSED_PARAM(numExclusions)) { Assert(false); return false;}
void CEntity::Teleport(const Vector3& UNUSED_PARAM(vecSetCoors), float UNUSED_PARAM(fSetHeading), bool UNUSED_PARAM(bCalledByPedTask), bool UNUSED_PARAM(bTriggerPortalRescan), bool UNUSED_PARAM(bCalledByPedTask2), bool UNUSED_PARAM(bWarp), bool UNUSED_PARAM(bKeepRagdoll), bool UNUSED_PARAM(bResetPlants)) { Assert(false); }
void CEntity::ProcessFrozen() {} // Called when the entity is frozen instead of process control
bool CEntity::SetupLighting() {return false;}		// in renderer.cpp
void CEntity::RemoveLighting(bool) {}	// in renderer.cpp
void CEntity::FlagToDestroyWhenNextProcessed(void) { Assertf(false, "CEntity::FlagToDestroyWhenNextProcessed"); }
void CEntity::GetLockOnTargetAimAtPos(Vector3& aimAtPos) const {aimAtPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());}
float CEntity::GetLockOnTargetableDistance() const { return -1.0f; }
void CEntity::SetLockOnTargetableDistance(float UNUSED_PARAM(fTargetableDist)) {}
void CEntity::UpdateNetwork() {}
void CEntity::ProcessControlNetwork() { Assert(false); }


audTracker *CEntity::GetPlaceableTracker()
{
	if (GetIsTypePed())
	{
		return ((CPed*)this)->GetPlaceableTracker();
	}
	else if (GetIsTypeVehicle())
	{
		return ((CVehicle*)this)->GetPlaceableTracker();
	}
	else if (GetIsTypeObject())
	{
		return ((CObject*)this)->GetPlaceableTracker();
	}
	return NULL;
}

const audTracker *CEntity::GetPlaceableTracker() const 
{
	if (GetIsTypePed())
	{
		return ((CPed*)this)->GetPlaceableTracker();
	}
	else if (GetIsTypeVehicle())
	{
		return ((CVehicle*)this)->GetPlaceableTracker();
	}
	else if (GetIsTypeObject())
	{
		return ((CObject*)this)->GetPlaceableTracker();
	}
	return NULL;
}

void CEntity::UpdateRenderFlags()
{
	fwEntity::UpdateRenderFlags();

#if HAS_RENDER_MODE_SHADOWS
	bool isVehiclePart = GetIsTypeVehicle();
	AssignRenderFlag(RenderFlag_USE_CUSTOMSHADOWS, isVehiclePart);
#endif // HAS_RENDER_MODE_SHADOWS
	AssignRenderFlag(RenderFlag_IS_DYNAMIC, GetIsDynamic());
	AssignRenderFlag(RenderFlag_IS_GROUND, GetIsTypeBuilding()); // hopefully this is separated from ground at some point
	AssignRenderFlag(RenderFlag_IS_BUILDING, GetIsTypeBuilding());
	AssignRenderFlag(RenderFlag_IS_PED, GetIsTypePed());

	if (GetIsTypeVehicle())
	{
		// vehicles handle the refs in SetModelId and dtor
		SetBaseDeferredType(DEFERRED_MATERIAL_VEHICLE);
	}
	else if (GetIsTypePed())
	{
		SetBaseDeferredType(DEFERRED_MATERIAL_PED);
	}
}


//
// name:		TellAudioAboutAudioEffectsAdd
// description:	Called by objects and buildings that get created to tell the audio code about the 2deffects
//				that are in fact audio effects that are attached to this entity

void CEntity::TellAudioAboutAudioEffectsAdd()
{
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect *pEffect = (*pa2dEffects)[i];

		if (pEffect->GetType() == ET_AUDIOEFFECT)
		{
			CAudioAttr* a = pEffect->GetAudio();
			Vector3 Pos;
			pEffect->GetPos(Pos);
			
			Quaternion	Q(a->qx, a->qy, a->qz, a->qw);
			g_EmitterAudioEntity.RegisterEntityEmitter(this, a->effectindex, Pos, Q);
		}
	}
#if __BANK
	if(audNorthAudioEngine::GetGtaEnvironment()->IsProcessinWorldAudioSectors())
	{
		if (pBaseModel->GetIsTree())
		{
			const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			audNorthAudioEngine::GetGtaEnvironment()->AddTreeToWorld(vThisPosition,this);
		}
		else if(GetType() == ENTITY_TYPE_BUILDING)
		{
			const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			audNorthAudioEngine::GetGtaEnvironment()->AddBuildingToWorld(vThisPosition,this);
		}
	}
#endif
}

#if (__BANK && __PS3) || DRAWABLE_STATS
//
// name:		GetDrawableStatsContext
// description:
u16 CEntity::GetDrawableStatsContext()
{
	if(m_lodData.IsSlod1())
		return DCC_SLOD1;
	if(m_lodData.IsSlod2())
		return DCC_SLOD2;
	if(m_lodData.IsSlod3())
		return DCC_SLOD3;
	if(m_lodData.IsSlod4())
		return DCC_SLOD4;
	if(GetIsTypePed())
		return DCC_PEDS;
	if(GetIsTypeVehicle())
		return DCC_VEHICLES;
	if(m_lodData.IsLod())
		return DCC_LOD;
	if(m_lodData.IsTree())
		return DCC_VEG;
	if(GetBaseModelInfo()->GetIsProp())
		return DCC_PROPS;
	
	return DCC_NO_CATEGORY;
}

#endif

//
// name:		TellAudioAboutAudioEffectsRemove
// description:	Called by objects and buildings that get created to tell the audio code about the 2deffects
//				that are in fact audio effects that are attached to this entity

void CEntity::TellAudioAboutAudioEffectsRemove()
{
	g_EmitterAudioEntity.UnregisterEmittersForEntity(this);
#if __BANK
	if(audNorthAudioEngine::GetGtaEnvironment()->IsProcessinWorldAudioSectors())
	{
		if(IsArchetypeSet())
		{
			CBaseModelInfo* pBaseModel = GetBaseModelInfo();

			if (pBaseModel->GetIsTree())
			{
				const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				audNorthAudioEngine::GetGtaEnvironment()->RemoveTreeFromWorld(vThisPosition);
			}
			else if(GetType() == ENTITY_TYPE_BUILDING)
			{
				const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				audNorthAudioEngine::GetGtaEnvironment()->RemoveBuildingFromWorld(vThisPosition);
			}
		}
	}
#endif
}


//
//
// name:		SetModelId
// description:	Set model model for entity
void CEntity::SetModelId(fwModelId modelId) 
{ 
	Assert(m_pArchetype == NULL);		// Can only call SetModelId once

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	Assert(pModelInfo);				

	//m_modelId = modelId;
	fwEntity::SetModelId(modelId);
	//SetArchetype(pModelInfo);

	// Only check for prerender effects if it has not already been set
	if(IsBaseFlagSet(fwEntity::HAS_PRERENDER) == false)
		AssignBaseFlag(fwEntity::HAS_PRERENDER, pModelInfo->GetHasPreRenderEffects());

	if (pModelInfo->GetHasSpawnPoints())
		m_nFlags.bHasSpawnPoints = true;

	if (pModelInfo->GetModelType() == MI_TYPE_TIME)
	{
		SetBaseFlag(fwEntity::IS_TIMED_ENTITY);
	}

	if (GetLodData().IsOrphanHd())
	{
		GetLodData().SetLodDistance((u16) pModelInfo->GetLodDistanceUnscaled());
	}

	if (pModelInfo->GetHasBoundInDrawable())
	{
		SetBaseFlag(fwEntity::HAS_PHYSICS_DICT);
	}

	m_lodData.SetIsTree(pModelInfo->GetIsTree());

	if (GetIsDynamic() && pModelInfo->GetOverridePhysicsBounds())
		static_cast< CDynamicEntity* >(this)->m_nDEflags.bOverridePhysicsBounds = true;
}

fwDrawData* CEntity::AllocateDrawHandler(rmcDrawable* pDrawable)
{
	CBaseModelInfo* pModel = GetBaseModelInfo();

	if (GetIsTypeDummyObject() && pModel->GetAttributes() == MODEL_ATTRIBUTE_IS_BENDABLE_PLANT)
	{
		if(pDrawable->HasInstancedGeometry())
		{
			return rage_new CEntityInstancedBendableDrawHandler(this, pDrawable);
		}
		return rage_new CEntityBendableDrawHandler(this, pDrawable);
	}
	else if (m_nFlags.bIsFrag)
	{
		if(pDrawable->HasInstancedGeometry())
		{
			return rage_new CEntityInstancedFragDrawHandler(this, pDrawable);
		}
		return rage_new CEntityFragDrawHandler(this, pDrawable);
	}
	else
	{
		if(pDrawable->HasInstancedGeometry())
		{
			return rage_new CEntityInstancedBasicDrawHandler(this, pDrawable);
		}
		return rage_new CEntityBasicDrawHandler(this, pDrawable);
	}
}

//
// CEntity::CreateDrawable(): Create the renderable object associated with this entity
//
bool CEntity::CreateDrawable()
{
#if NAVMESH_EXPORT
	const bool bExportingCollision = CNavMeshDataExporter::WillExportCollision();
#else
	const bool bExportingCollision = false;
#endif

	// MN - added so that objects set to invisible aren't instantly recreated
	// they will only be created when they are visible again
	if (GetIsVisible()==0)
		return false;

	if(m_pDrawHandler)
		return false;

	CBaseModelInfo* pModel = GetBaseModelInfo();
	rmcDrawable* pDrawable;

	Assertf(pModel, "No geometry");

	// Get drawable
	if(pModel->GetFragType())
	{
		pDrawable = pModel->GetFragType()->GetCommonDrawable();

		m_nFlags.bIsFrag = true;
		Assert(GetFragInst() || GetCurrentPhysicsInst()==NULL);
	}
	else
	{
		pDrawable = pModel->GetDrawable();
	}

	// If no drawable return
	if(pDrawable == NULL)
		return false;
	// Create GTA drawable and setup command buffers
	m_pDrawHandler = AllocateDrawHandler(pDrawable);

	if(m_pDrawHandler == NULL)
		return false;
	
	if (!GetIsTypeVehicle())
		GetLodData().SetResetAlpha(true);

	pModel->AddRef();

	// keep SLOD3 / SLOD4 models resident until the entity is destroyed (and timed emissive SLOD1)
	if ( !IsProtectedBaseFlagSet(fwEntity::HAS_SLOD_REF)
		&& (m_lodData.IsContainerLod() || (m_lodData.IsSlod1() && IsTimedEntity() && ImportantEmissive())) )
	{
		AssignProtectedBaseFlag(fwEntity::HAS_SLOD_REF, true);
		CModelInfo::SetAssetRequiredFlag(GetModelId(), STRFLAG_DONTDELETE);
	}

	u8 modelType = pModel->GetModelType();
	if (modelType == MI_TYPE_PED)
	{
		((CPedModelInfo *)pModel)->AddPedModelRef();
		SetBaseDeferredType(DEFERRED_MATERIAL_PED);
	}
	else if (modelType == MI_TYPE_VEHICLE)	
	{
		// vehicles handle the refs in SetModelId and dtor
		SetBaseDeferredType(DEFERRED_MATERIAL_VEHICLE);
	}
	else if (modelType == MI_TYPE_WEAPON)
	{
		SetBaseDeferredType(DEFERRED_MATERIAL_PED);
	}

	//if(pModel->GetDontCullSmallShadows())
	//{
	//	SetBaseFlag(fwEntity::RENDER_SMALL_SHADOW);
	//}

	if(pModel->GetDrawLast())
	{
		SetBaseFlag(fwEntity::DRAW_LAST);
	}

	// update entity bucket flags for lods:
	for(u32 lod=0; lod<LOD_COUNT; lod++)
	{
		u32 bucketMask=0;

		if(pModel->GetFragType())
			bucketMask=pModel->GetFragType()->GetShaderBucketMask(lod);
		else
			bucketMask=pDrawable->GetBucketMask(lod);

		if(bucketMask&(0x1<<CRenderer::RB_OPAQUE))
			SetBaseFlag(fwEntity::HAS_OPAQUE);

		if(bucketMask&(0x1<<CRenderer::RB_ALPHA))
			SetBaseFlag(fwEntity::HAS_ALPHA);

		if(bucketMask&(0x1<<CRenderer::RB_DECAL))
			SetBaseFlag(fwEntity::HAS_DECAL);

		if(bucketMask&(0x1<<CRenderer::RB_CUTOUT))
			SetBaseFlag(fwEntity::HAS_CUTOUT);

		if(bucketMask&(0x1<<CRenderer::RB_WATER))
		{
			SetBaseFlag(fwEntity::HAS_WATER);
			River::AddToRiverEntityList(this);
		}

		if(bucketMask&(0x1<<CRenderer::RB_DISPL_ALPHA))
			SetBaseFlag(fwEntity::HAS_DISPL_ALPHA);

		if(bucketMask&(0x1<<CRenderer::RB_CUTOUT))
			SetBaseFlag(fwEntity::HAS_CUTOUT);
	}

	static_cast<CEntityDrawHandler&>(GetDrawHandler()).ShaderEffectCreateInstance(pModel, this);
	// Only check for prerender effects if it has not already been set
	if(IsBaseFlagSet(fwEntity::HAS_PRERENDER) == false)
		AssignBaseFlag(fwEntity::HAS_PRERENDER, pModel->GetHasPreRenderEffects());

	AssignBaseFlag(fwEntity::USE_SCREENDOOR, GetShouldUseScreenDoor());

	SetupAmbientScaleFlags();
	CalculateAmbientScales();

	if (pModel->Has2dEffects() && m_nFlags.bLightObject == false)
	{
		if(!bExportingCollision) // avoid crash
		{
			m_nFlags.bLightObject = LightEntityMgr::AddAttachedLights(this);
		}
	}

	AssignBaseFlag(fwEntity::HAS_ALPHA_SHADOW, pModel->GetHasAlphaShadow());

	AssignRenderFlag(RenderFlag_IS_SKINNED, pDrawable->IsSkinned());

#if __BANK
	// entrypoint into a number of debug systems, including height maps and debug archetype proxy checks ..
	{
		CGameWorldHeightMap::PostCreateDrawableForEntity(this);

#if __PS3
		extern bool WaterBoundary_IsEnabled();
		extern void WaterBoundary_AddEntity(const CEntity* pEntity, float z);

		if (WaterBoundary_IsEnabled())
		{
			WaterBoundary_AddEntity(this, 0.0f);
		}
#endif // __PS3
	}
#endif // __BANK

#if RSG_PC
	//	Ugly hack to force alpha shadows on the cable
	if(pModel->GetCustomShaderEffectType() && pModel->GetCustomShaderEffectType()->GetType() == CSE_CABLE && CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == CSettings::Ultra && CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality >= CSettings::High)
	{
		//	needed flags to get them rendered in the alpha shadows
		AssignBaseFlag(fwEntity::HAS_ALPHA_SHADOW, true);
		AssignBaseFlag(fwEntity::HAS_OPAQUE, true);
	}
#endif

	//	url:bugstar:2615806 - Use the vehicle stencil material to avoid puddle
	if(pModel->GetCustomShaderEffectType() && pModel->GetCustomShaderEffectType()->GetType() == CSE_TINT && ((CCustomShaderEffectTintType*)pModel->GetCustomShaderEffectType())->GetForceVehicleStencil())
	{
		SetBaseDeferredType(DEFERRED_MATERIAL_VEHICLE);
	}

	if(pModel->GetModelNameHash() == ATSTRINGHASH("trailer_mirrorr", 0xC46336DB))
	{
		SetHidden(true);
	}

	return true;
}// end of CEntity::CreateDrawable()...

//
//
// CEntity::DeleteRwObject(): Delete object associated with this object
//
void CEntity::DeleteDrawable()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); // Don't even think about it.

	if(m_pDrawHandler)
	{
		Assert(GetBaseModelInfo());
		Assert(GetBaseModelInfo()->GetStreamSlot().Get() == (s32)GetModelIndex());

		DECALMGR.OnDelete(this);
		
		delete m_pDrawHandler;
		m_pDrawHandler = NULL;

		GetLodData().SetResetDisabled(true);
		CBaseModelInfo *pModel = GetBaseModelInfo();

		pModel->RemoveRef();

		if (pModel->GetModelType() == MI_TYPE_VEHICLE)
		{
			// vehicles handle the refs in SetModelId and dtor
		}
		else if (pModel->GetModelType() == MI_TYPE_PED)
		{
			((CPedModelInfo *)pModel)->RemovePedModelRef();
		}
		if(GetBaseFlags() & fwEntity::HAS_WATER)
			River::RemoveFromRiverEntityList(this);

	}//if(m_pDrawable)...

}// end of CEntity::DeleteDrawable()...


//
// name:		CEntity::GetBoundBox
// description:	Return the axix aligned bounding box for this entity;
FASTRETURNCHECK(const spdAABB &) CEntity::GetBoundBox(spdAABB& box) const
{
	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	Assert(pModelInfo != NULL);
	box = pModelInfo->GetBoundingBox();
	box.Transform(GetNonOrthoMatrix());

	return box;
}



//
// name:		CEntity::GetBoundCentre
// description:	Get bound centre in world space
Vector3 CEntity::GetBoundCentre() const
{
	return TransformIntoWorldSpace(GetBaseModelInfo()->GetBoundingSphereOffset());
}

//
// name:		CEntity::GetBoundCentre
// description:	Get bound centre in world space
void CEntity::GetBoundCentre(Vector3& centre) const
{
	TransformIntoWorldSpace(centre, GetBaseModelInfo()->GetBoundingSphereOffset());
}

//
// name:		GetBoundRadius
// description:	Return the radius of the bounding sphere
float CEntity::GetBoundRadius() const
{ 
	return GetBaseModelInfo()->GetBoundingSphereRadius();
}

float CEntity::GetBoundCentreAndRadius(Vector3& centre) const
{
	TransformIntoWorldSpace(centre, GetBaseModelInfo()->GetBoundingSphereOffset());
	return GetBaseModelInfo()->GetBoundingSphereRadius();
}

void CEntity::GetBoundSphere(spdSphere& sphere) const
{
	Vec4V modelSphere = GetBaseModelInfo()->GetBoundingSphereV4();
	Vec3V worldCentre;
	TransformIntoWorldSpace(RC_VECTOR3(worldCentre), VEC4V_TO_VECTOR3(modelSphere));

	sphere.Set(worldCentre, modelSphere.GetW());
}

//
// name:		CEntity::GetBoundingBoxMin
// description:	Get bounding box min
const Vector3& CEntity::GetBoundingBoxMin() const
{
	return GetBaseModelInfo()->GetBoundingBoxMin();
}

//
// name:		CEntity::GetBoundingBoxMax
// description:	Get bounding box max
const Vector3& CEntity::GetBoundingBoxMax() const
{
	return GetBaseModelInfo()->GetBoundingBoxMax();
}

//
// name:		StartAnimUpdate
// description:	Start the parallel animation update
void CEntity::StartAnimUpdate(float UNUSED_PARAM(fTimeStep))
{
}

//
// name:		UpdateVelocityAndAngularVelocity
// description:
void CEntity::UpdateVelocityAndAngularVelocity(float UNUSED_PARAM(fTimeStep))
{
}

void CEntity::SetupAmbientScaleFlags()
{
	if( m_setupAmbientScaleFlags == true )
	{
		CBaseModelInfo* pModel = GetBaseModelInfo();

		// Force dynamic ambient scale on for all weapons
		if (pModel->GetModelType() == MI_TYPE_WEAPON) 
		{
			SetUseAmbientScale(true);
			SetUseDynamicAmbientScale(true);
		}
		else 
		{
			m_useAmbientScale = pModel->GetUseAmbientScale();
			m_useDynamicAmbientScale = GetNaturalAmbientScale() == 255;
		}
		
		m_setupAmbientScaleFlags = false;
	}
}



void CEntity::CalculateAmbientScales()
{
	if (m_useAmbientScale)
	{
		if (m_useDynamicAmbientScale)
		{
			if(m_calculateAmbientScales)
				CalculateDynamicAmbientScales();
		}
	}
	else
	{
		SetNaturalAmbientScale(255);
		SetArtificialAmbientScale(255);
	}	
}

void CEntity::CalculateDynamicAmbientScales()
{
	fwInteriorLocation interiorLocation = GetInteriorLocation();

	const Vec2V vAmbientScale = g_timeCycle.CalcAmbientScale(GetTransform().GetPosition(), interiorLocation);
	Assertf(vAmbientScale.GetXf() >= 0.0f && vAmbientScale.GetXf() <= 1.0f,"Dynamic Natural Ambient is out of range (%f, should be between 0.0f and 1.0f)",vAmbientScale.GetXf());
	Assertf(vAmbientScale.GetYf() >= 0.0f && vAmbientScale.GetYf() <= 1.0f,"Dynamic Artificial Ambient is out of range (%f, should be between 0.0f and 1.0f)",vAmbientScale.GetYf());

	const Vec2V vScaledAmbientScale = vAmbientScale * ScalarV(255.0f);
	SetNaturalAmbientScale(u8(vScaledAmbientScale.GetXf()));	
	SetArtificialAmbientScale(u8(vScaledAmbientScale.GetYf()));
}

float CEntity::CalcLightCrossFadeRatio(CEntity *pEntityForDistance,float unscaledLodDist,bool treatAsLod)
{
	static const float fCullDist = 300.0f;	//??

	float lightFade=1.0f;
	float lodDistance;
	float farClipPlane;
	Vector3 camPos;
	
	CRenderPhase *pRenderPhase=g_SceneToGBufferPhaseNew;
	fwRenderPhaseScanner *scanner = pRenderPhase->GetScanner();
	farClipPlane=scanner->GetCameraFarClip();
	lodDistance = g_LodScale.GetGlobalScale();
	camPos = scanner->GetCameraPositionForFade();


	Vector3 distV = VEC3V_TO_VECTOR3(pEntityForDistance->GetTransform().GetPosition()) - camPos;		// get distance from camera
	float dist = distV.Mag();

//	float unscaledLodDist=this->GetUnscaledStoredLodDist();
	float scaledLodDistance=unscaledLodDist*lodDistance;
	if(!treatAsLod) //not from bigbuildinglist
	{
		if(dist > fCullDist && fCullDist < (scaledLodDistance) &&
			dist < (scaledLodDistance)+LODTYPES_FADE_DIST)
		{
			dist += (scaledLodDistance) - fCullDist;

		}
	}
	float fadeDist = LODTYPES_FADE_DIST;
	float lodDist = rage::Min((scaledLodDistance), farClipPlane + this->GetBoundRadius());

	if((dist + fadeDist - LODTYPES_FADE_DIST)<scaledLodDistance  )
	{
		//we are fully visible our lod wont draw therefore its crossfade is 0 and its alpha is 255
		lightFade=1.0f;
	}
	else if( (dist  - LODTYPES_FADE_DIST)<scaledLodDistance  )
	{
		//we are in the cross fade zone

		float ratio = (lodDist + LODTYPES_FADE_DIST - dist) / fadeDist;
		ratio = rage::Min(ratio, 1.0f);
		if (ratio > 0.5f)
		{
			// fade out LOD
			//float LODRatio = (1.0f - ratio) * 2.0f;		// LOD fades out from (0.5f to 1.0f)
			lightFade = (ratio-0.5f)*2.0f; //as the lod fades in we fade out
		}
		else
		{ //lod has now completely faded in, so HD's light should be off
			lightFade=0.0f;
		}
	}
	else
	{
		lightFade=0.0f; //we are faded out
	}
	return lightFade;
}

//
// name:		PreRender
// description:	Add prerender effects for this entity
ePrerenderStatus CEntity::PreRender(const bool UNUSED_PARAM(bIsVisibleInMainViewport))
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	CBaseModelInfo *pBaseModel = GetBaseModelInfo();

	if(!IsBaseFlagSet(fwEntity::HAS_PRERENDER))
		return PRERENDER_DONE;

	// don't update the gtaDrawable for dummy peds like this - they have to be handled as CPedVariations for shader update etc.
	if(m_pDrawHandler)
	{
		if (m_pDrawHandler->GetShaderEffect())
		{
			m_pDrawHandler->GetShaderEffect()->Update(this);
		}
	}

	// these light alpha updates can be handled by the lodtree system now
	// to avoid pointer chasing lod parents and expensive calls into
	// get alpha or get lod distance fns. revisit this. -iank

	const atArray<C2dEffect*>* pEffectArray = pBaseModel->Get2dEffectsNoLights();
	const atArray<CLightAttr>* pLightArray = pBaseModel->GetLightArray();

	if(pBaseModel->GetHasLoaded())
	{
		if((pEffectArray && pEffectArray->GetCount()) ||
			(pLightArray && pLightArray->GetCount()))
		{
			float lightFade=GetAlpha()/255.0f;

			//higher lod lights will all fade up when lower lod lights fade out
			{
				lightFade=1.0f;
				
				if (GetLodData().HasChildren()){
					if (lightFade==1.0f)
					{
						//try and work out what our child is doing with the crossfade then do the opposite :)
						// TODO: int-to-float conversion. Can we avoid that? Have GetLodDistance() return float maybe? /MAK
						lightFade=1.0f-CalcLightCrossFadeRatio(this, (float) GetLodDistance(), !GetLodData().IsHighDetail()); // calculate the LightFade using the distance to the lod 
					}
				} else { 			
					lightFade=GetAlpha()*(1.0f/255.0f);
				}
			}

			#if 0 //keep this for debugging
			if (pBaseModel->GetNum2dEffectLights()>0)// || pBaseModel->GetHasFxEntityLight() )//|| pModel->GetNum2dEffectType(ET_LIGHT_SHAFT)>0)
			{
				if ( this->m_nFlags.bInMloRoom )
				{
					this->m_nFlags.bInMloRoom=true; //prendering an interior object with lights

					if ( this->m_nFlags.bLightObject==false )
					{
						this->m_nFlags.bLightObject=false; //but didn't add light objects
					}
				}
				else
				{
					if ( this->m_nFlags.bLightObject==false )
					{
						this->m_nFlags.bLightObject=false; //prendering a non interior object with light that didn't add light objects
					}
				}
			}
			#endif

			Update2dEffects(lightFade, pEffectArray, pLightArray);
		}
	}

	if (CTrafficLights::IsMITrafficLight(pBaseModel))
	{
		CTrafficLights::RenderLights(this);
	}
	else if (CRailwayCrossingLights::IsRailwayCrossingLight(pBaseModel))
	{
		CRailwayCrossingLights::RenderLights(this);
	}
	else if (CRailwayBarrierLights::IsRailwayBarrierLight(this))
	{
		CRailwayBarrierLights::RenderLights(this);
	}

	return PRERENDER_DONE;
}// end of CEntity::PreRender()...

void CEntity::GetBounds(spdAABB &box, spdSphere &sphere) const
{
	Assertf(!GetIsDynamic() && !GetIsTypeLight(), "Don't use CEntity::GetBounds for dynamic or light entitys");

	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	Assert(pModelInfo != NULL);
	box = pModelInfo->GetBoundingBox();
	Mat34V mat = GetNonOrthoMatrix();
	box.Transform(mat);

	Vec4V modelSphere = GetBaseModelInfo()->GetBoundingSphereV4();
	Vec3V worldCentre = Transform(mat, modelSphere.GetXYZ());
	sphere.Set(worldCentre, modelSphere.GetW());
}

bool gTestBatch = false;

//
//
// name:		GetIsTouching
// description:	Returns if entity's bounding sphere is touching another
bool CEntity::GetIsTouching(const CEntity* pEntity) const
{
	Vector3 vecCentreA;
	Vector3 vecCentreB;
	
	GetBoundCentre(vecCentreA);
	pEntity->GetBoundCentre(vecCentreB);
	
	float radius = GetBoundRadius() + pEntity->GetBoundRadius();

	return ((vecCentreA - vecCentreB).Mag2() < radius*radius);
}

//
//
// name:		GetIsTouching
// description:	Returns if entity's bounding sphere is touching another bounding sphere
bool CEntity::GetIsTouching(const Vector3& centre, float radius) const
{
	Vector3 vecCentreB;

	GetBoundCentre(vecCentreB);
	
	radius += GetBoundRadius();
	vecCentreB -= centre;
	
	return (vecCentreB.Mag2() < radius*radius);
}

//
// name:		IsVisible
// descriptionL	Returns if entity is currently visible
bool CEntity::IsVisible() const
{
	if((m_pDrawHandler == NULL && !m_nFlags.bIsFrag) || GetIsVisible() == FALSE)
		return false;

	return GetIsOnScreen();
}

u32 CEntity::GetLodRandomSeed(CEntity* pLod)
{
	//use the location as the random seed in MP so that each position will be random but across the network it will be the same
	if (NetworkInterface::IsNetworkOpen() && pLod)
	{
		return (u32)(size_t) pLod->GetBoundCentre().Mag2();
	}

	// create a 32-bit seed from a 64-bit pointer
	static u32 rot = 5;
	size_t seed = (size_t)pLod;
	return u32((seed>>rot)|(seed<<(32-rot)));
}

bool CEntity::GetIsOnScreen(bool bIncludeNetworkPlayers) const
{
	float radius = 0.0f;
	Vector3 centre = VEC3_ZERO;

	if (GetIsDynamic() && RequiresProcessControl())
	{
		const CDynamicEntity* pDynEnt = static_cast<const CDynamicEntity*>(this);
		if(pDynEnt->GetIsVisibleInSomeViewportThisFrame())		// this cached result is only valid for entities that require process control!
		{
			return true;
		}
	}
	else
	{
		radius = GetBoundRadius();
		GetBoundCentre(centre);
	
		if (camInterface::IsSphereVisibleInGameViewport(centre, radius))
			return true;
	}

	if (bIncludeNetworkPlayers && NetworkInterface::IsGameInProgress())
	{
		if (radius == 0.0f)
		{
			radius = GetBoundRadius();
			GetBoundCentre(centre);
		}

		if (NetworkInterface::IsVisibleToAnyRemotePlayer(centre, radius, 100.0f))
			return true;
	}

	return false;
}

bool CEntity::GetIsVisibleInViewport(s32 viewportId)
{
	float radius = GetBoundRadius();
	Vector3 centre;
	GetBoundCentre(centre);

	bool visible=camInterface::IsSphereVisibleInViewport(viewportId, centre, radius);
	//enum { CLIP_PLANE_NEAR, CLIP_PLANE_LEFT, CLIP_PLANE_RIGHT, CLIP_PLANE_FAR, CLIP_PLANE_TOP, CLIP_PLANE_BOTTOM };

	return visible;

	/* DW - This is not the best way to do since, we require that the grcViewport to be set to current ( should only be required when rendering )
	Assert(grcViewport::GetCurrent());
	return grcViewport::GetCurrent()->IsSphereVisible(centre.x, centre.y, centre.z, radius);
	*/
}

CEntity* g_pDebugEntity = NULL;
//
// name:		AddToInterior
// description:	Add entity to interior room
void CEntity::AddToInterior(fwInteriorLocation interiorLocation){

	Assert(interiorLocation.IsValid());
	Assert(!GetIsInExterior());		// make sure not in exterior already

	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfAddToInteriorCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
	g_pDebugEntity = this;
	Assert(GetIsTypeObject() == false);				// objects have portal trackers and go on active object list for room

	CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation( interiorLocation );
	Assertf(pIntInst, "No valid interior found at this interior Idx");
	if (pIntInst){
		pIntInst->AppendObjectInstance(this, interiorLocation);
	} else {
		Assertf(false,"Can't add entity to interior - target interior Id is not valid");
	}

	Assert(m_nFlags.bInMloRoom == true);
}

//
// name:		Add
// description:	Add entity to world sectors
void CEntity::Add()
{
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfAddCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
	g_pDebugEntity = this;

	Assert(m_nFlags.bInMloRoom == false);

	fwRect rect = GetBoundRect();
	AddWithRect(rect);
}


//
// name:		Add
// description:	Add entity to world sectors
void CEntity::AddWithRect(const fwRect& unclippedRect)
{
	Assert(IsArchetypeSet());

	Assert(m_nFlags.bInMloRoom == false);

	gWorldMgr.AddToWorld(this, unclippedRect);
}



//
// name:		RemoveFromInterior
// description:	Remove entity from the interior entity it is in
void CEntity::RemoveFromInterior(void){

	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfRemoveFromInteriorCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
	Assert(m_nFlags.bInMloRoom == true);
	Assert(GetIsTypeObject() == false);				// objects have portal trackers and go on active object list for room

	if ( this->GetInteriorLocation().IsValid() )
	{
		CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation( this->GetInteriorLocation() );
		if (pIntInst){
			pIntInst->RemoveObjectInstance(this);
		}
	}

	Assert(m_nFlags.bInMloRoom == false);
}

//
// name:		Remove
// description:	Remove entity from world sectors
void CEntity::Remove()
{
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfRemoveCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	Assert(m_nFlags.bInMloRoom == false);

	if (GetIsDynamic())
	{
		CDynamicEntity *pDE = static_cast<CDynamicEntity*>(this);
		if (pDE->IsStraddlingPortal())
		{
			pDE->RemoveFromStraddlingPortalsContainer();
		}
	}
	gWorldMgr.RemoveFromWorld(this);
}

//
// name:		CEntity::UpdateWorldFromEntity
// description:	Quicker way of removing and adding an entity from the world
void CEntity::UpdateWorldFromEntity()
{
#if __ASSERT
	const Vec3V		position = GetTransform().GetPosition();
	const char*		debugName = this->GetIsPhysical() ? static_cast< CPhysical* >(this)->GetDebugName() : "none";
	const char*		networkName = this->GetIsPhysical() && static_cast< CPhysical* >( this )->GetNetworkObject() ? static_cast< CPhysical* >( this )->GetNetworkObject()->GetLogName() : "none";
	Assertf( g_WorldLimits_AABB.ContainsPoint( position ),
		"Position of entity %s (debug name %s log name %s) is outside world limits (at %f,%f,%f)", this->GetModelName(), debugName, networkName,
		position.GetXf(), position.GetYf(), position.GetZf() );
#endif

	if ( m_nFlags.bInMloRoom )
	{
		CInteriorInst* interior = CInteriorInst::GetInteriorForLocation( this->GetInteriorLocation() );

		Assert( this->GetInteriorLocation().IsValid() );
		Assert( this->GetIsRetainedByInteriorProxy() == false );
		Assert( interior );
		Assert( interior->CanReceiveObjects());

		interior->UpdateObjectInstance( this );
	}
	else
	{
		// only update in the exterior if already in the exterior! If in interior, then it will be updated separately.
		if (GetIsInExterior())
		{
			fwDynamicEntityComponent*	dynamicComponent = GetDynamicComponent();

			if ( dynamicComponent )
			{
				Assert( dynamicComponent->GetEntityDesc() );
				CGameWorld::UpdateEntityDesc( this, dynamicComponent->GetEntityDesc() );
			}
			else
				gWorldMgr.UpdateInWorld(this, this->GetBoundRect());
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
// NAME :     AssertEntityPointerValid
// FUNCTION : Will assert if the entity pointer is not valid or the entity is not in
//            the world.
//////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void AssertEntityPointerValid(CEntity *ASSERT_ONLY(pEntity) )
{
    Assert(IsEntityPointerValid(pEntity));
}

// Same as above but this version doesn't require the entity to
// be added to the world

void AssertEntityPointerValid_NotInWorld(CEntity *UNUSED_PARAM(pEntity))
{
/*
	Assert(pEntity);	// Null pointers are not valid

	switch(pEntity->GetType())
	{
		case ENTITY_TYPE_NOTHING:
		default:
			Assert(0);
			break;
		case ENTITY_TYPE_BUILDING:
			AssertBuildingPointerValid((CBuilding *)pEntity);
			break;
		case ENTITY_TYPE_VEHICLE:
			AssertVehiclePointerValid_NotInWorld((CVehicle *)pEntity);
			break;
		case ENTITY_TYPE_PED:
			AssertPedPointerValid_NotInWorld((CPed *)pEntity);
			break;
		case ENTITY_TYPE_OBJECT:
			AssertObjectPointerValid_NotInWorld((CObject *)pEntity);
			break;
		case ENTITY_TYPE_DUMMY:
			AssertDummyPointerValid_NotInWorld((CDummy *)pEntity);
			break;
		case ENTITY_TYPE_PORTAL:
			AssertObjectPointerValid_NotInWorld((CPortalInst *)pEntity);
			break;
		case ENTITY_TYPE_MLO:
			AssertObjectPointerValid_NotInWorld((CInteriorInst *)pEntity);
			break;
		case ENTITY_TYPE_NOTINPOOLS:
			break;
	}*/
}
#endif

bool IsEntityPointerValid(CEntity *pEntity)
{
	if(!pEntity)
	{
	    return false;
	}	
/*	
	switch(pEntity->GetType())
	{
		case ENTITY_TYPE_NOTHING:
		    return false;
		    break;
		case ENTITY_TYPE_BUILDING:
			return IsBuildingPointerValid((CBuilding *)pEntity);
			break;
		case ENTITY_TYPE_VEHICLE:
			return IsVehiclePointerValid((CVehicle *)pEntity);
			break;
		case ENTITY_TYPE_PED:
			return IsPedPointerValid((CPed *)pEntity);
			break;
		case ENTITY_TYPE_OBJECT:
			return IsObjectPointerValid((CObject *)pEntity);
			break;
		case ENTITY_TYPE_DUMMY:
		    return IsDummyPointerValid((CDummy *)pEntity);
			break;
		case ENTITY_TYPE_PORTAL:
			return IsObjectPointerValid((CPortalInst *)pEntity);
			break;
		case ENTITY_TYPE_MLO:
			return IsObjectPointerValid((CInteriorInst *)pEntity);
			break;
		case ENTITY_TYPE_NOTINPOOLS:
			return true;
			break;
		default:
		    return false;
		    break;
	}*/
	return true;
}


//
// Finds a square on the z=0 plane that represents (as accurately as possible)
// the projection of the bounding box.
// This is used by Gordon for his AI code.
//
#if 0
void CEntity::CalculateBBProjection(Vector3 *pPoint1, Vector3 *pPoint2,Vector3 *pPoint3, Vector3 *pPoint4)
{
	CBaseModelInfo *pModelInfo = GetBaseModelInfo();

	const fwTransform& t = GetTransform();
	float	LengthXProj = MagXY(t.GetA()).Getf();
	float	LengthYProj = MagXY(t.GetB()).Getf();
	float	LengthZProj = MagXY(t.GetC()).Getf();
	
	float	SizeBBX = rage::Max(pModelInfo->GetBoundingBoxMax().x, -pModelInfo->GetBoundingBoxMin().x);
	float	SizeBBY = rage::Max(pModelInfo->GetBoundingBoxMax().y, -pModelInfo->GetBoundingBoxMin().y);
	float	SizeBBZ = rage::Max(pModelInfo->GetBoundingBoxMax().z, -pModelInfo->GetBoundingBoxMin().z);
	
	float	TotalSizeX = LengthXProj * SizeBBX * 2.0f;
	float	TotalSizeY = LengthYProj * SizeBBY * 2.0f;
	float	TotalSizeZ = LengthZProj * SizeBBZ * 2.0f;

	float		OffsetLength1, OffsetLength2, OffsetLength;
	float		OffsetWidth1, OffsetWidth2, OffsetWidth;
	Vector3		MainVec, MainVecPerp;
	Vector3		HigherPoint, LowerPoint;

	if (TotalSizeX > TotalSizeY && TotalSizeX > TotalSizeZ)
	{		// x-axis is the main one
		MainVec = Vector3(t.GetTransform().GetA().GetXf(), t.GetTransform().GetA().GetYf(), 0.0f);

		HigherPoint = GetPosition() + SizeBBX * MainVec;
		LowerPoint = GetPosition() - SizeBBX * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBY * (MainVec.y * t.GetB().GetYf()   +   MainVec.x * t.GetB().GetXf());
		OffsetWidth1 =  SizeBBY * (MainVec.x * t.GetB().GetYf()  -   MainVec.y * GetB().GetXf());
		OffsetLength2 = SizeBBZ * (MainVec.y * t.GetC().GetYf()   +   MainVec.x * GetC().GetXf());
		OffsetWidth2 =  SizeBBZ * (MainVec.x * t.GetC().GetYf()   -   MainVec.y * GetC().GetXf());
	}
	else if (TotalSizeY > TotalSizeZ)
	{		// y-axis is the main one
		MainVec = Vector3(t.GetB().GetXf(), t.GetB().GetYf(), 0.0f);

		HigherPoint = GetPosition() + SizeBBY * MainVec;
		LowerPoint = GetPosition() - SizeBBY * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBX * (MainVec.y * t.GetA().GetYf()   +   MainVec.x * t.GetA().GetXf());
		OffsetWidth1 =  SizeBBX * (MainVec.x * t.GetA().GetYf()   -   MainVec.y * t.GetA().GetXf());
		OffsetLength2 = SizeBBZ * (MainVec.y * t.GetC().GetYf()   +   MainVec.x * t.GetC().GetXf());
		OffsetWidth2 =  SizeBBZ * (MainVec.x * t.GetC().GetYf()   -   MainVec.y * t.GetC().GetXf());
	}
	else
	{		// z-axis is the main one
		MainVec = Vector3(t.GetC().GetXf(), t.GetC().GetYf(), 0.0f);

		HigherPoint = GetPosition() + SizeBBZ * MainVec;
		LowerPoint = GetPosition() - SizeBBZ * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBX * (MainVec.y * t.GetA().GetYf()   +   MainVec.x * t.GetA().GetXf());
		OffsetWidth1 =  SizeBBX * (MainVec.x * t.GetA().GetYf()   -   MainVec.y * t.GetA().GetXf());
		OffsetLength2 = SizeBBY * (MainVec.y * t.GetB().GetYf()  +   MainVec.x * t.GetB().GetXf());
		OffsetWidth2 =  SizeBBY * (MainVec.x * t.GetB().GetYf()  -   MainVec.y * t.GetB().GetXf());
	}

	MainVecPerp = Vector3(MainVec.y, -MainVec.x, 0.0f);

	OffsetLength = ABS(OffsetLength1) + ABS(OffsetLength2);
	OffsetWidth = ABS(OffsetWidth1) + ABS(OffsetWidth2);

	*pPoint1 = HigherPoint + OffsetLength * MainVec - OffsetWidth * MainVecPerp;
	*pPoint2 = HigherPoint + OffsetLength * MainVec + OffsetWidth * MainVecPerp;
	*pPoint3 = LowerPoint - OffsetLength * MainVec + OffsetWidth * MainVecPerp;
	*pPoint4 = LowerPoint - OffsetLength * MainVec - OffsetWidth * MainVecPerp;

	pPoint1->z = pPoint2->z = pPoint3->z = pPoint4->z = GetPosition().z;
}
#else
void CEntity::CalculateBBProjection(Vector3 *pPoint1, Vector3 *pPoint2,Vector3 *pPoint3, Vector3 *pPoint4)
{
	CBaseModelInfo *pModelInfo = GetBaseModelInfo();

	Vector3 vecRight(VEC3V_TO_VECTOR3(GetTransform().GetA()));
	Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
	Vector3 vecUp(VEC3V_TO_VECTOR3(GetTransform().GetC()));
	float	LengthXProj = rage::Sqrtf(vecRight.x * vecRight.x + vecRight.y * vecRight.y);
	float	LengthYProj = rage::Sqrtf(vecForward.x * vecForward.x + vecForward.y * vecForward.y);
	float	LengthZProj = rage::Sqrtf(vecUp.x * vecUp.x + vecUp.y * vecUp.y);

	float	SizeBBX = rage::Max(pModelInfo->GetBoundingBoxMax().x, -pModelInfo->GetBoundingBoxMin().x);
	float	SizeBBY = rage::Max(pModelInfo->GetBoundingBoxMax().y, -pModelInfo->GetBoundingBoxMin().y);
	float	SizeBBZ = rage::Max(pModelInfo->GetBoundingBoxMax().z, -pModelInfo->GetBoundingBoxMin().z);

	float	TotalSizeX = LengthXProj * SizeBBX * 2.0f;
	float	TotalSizeY = LengthYProj * SizeBBY * 2.0f;
	float	TotalSizeZ = LengthZProj * SizeBBZ * 2.0f;

	float		OffsetLength1, OffsetLength2, OffsetLength;
	float		OffsetWidth1, OffsetWidth2, OffsetWidth;
	Vector3		MainVec, MainVecPerp;
	Vector3		HigherPoint, LowerPoint;

	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	if (TotalSizeX > TotalSizeY && TotalSizeX > TotalSizeZ)
	{		// x-axis is the main one
		MainVec = Vector3(vecRight.x, vecRight.y, 0.0f);

		HigherPoint = vThisPosition + SizeBBX * MainVec;
		LowerPoint = vThisPosition - SizeBBX * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBY * (MainVec.y * vecForward.y   +   MainVec.x * vecForward.x);
		OffsetWidth1 =  SizeBBY * (MainVec.x * vecForward.y   -   MainVec.y * vecForward.x);
		OffsetLength2 = SizeBBZ * (MainVec.y * vecUp.y   +   MainVec.x * vecUp.x);
		OffsetWidth2 =  SizeBBZ * (MainVec.x * vecUp.y   -   MainVec.y * vecUp.x);
	}
	else if (TotalSizeY > TotalSizeZ)
	{		// y-axis is the main one
		MainVec = Vector3(vecForward.x, vecForward.y, 0.0f);

		HigherPoint = vThisPosition + SizeBBY * MainVec;
		LowerPoint = vThisPosition - SizeBBY * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBX * (MainVec.y * vecRight.y   +   MainVec.x * vecRight.x);
		OffsetWidth1 =  SizeBBX * (MainVec.x * vecRight.y   -   MainVec.y * vecRight.x);
		OffsetLength2 = SizeBBZ * (MainVec.y * vecUp.y   +   MainVec.x * vecUp.x);
		OffsetWidth2 =  SizeBBZ * (MainVec.x * vecUp.y   -   MainVec.y * vecUp.x);
	}
	else
	{		// z-axis is the main one
		MainVec = Vector3(vecUp.x, vecUp.y, 0.0f);

		HigherPoint = vThisPosition + SizeBBZ * MainVec;
		LowerPoint = vThisPosition - SizeBBZ * MainVec;
		MainVec.Normalize();

		OffsetLength1 = SizeBBX * (MainVec.y * vecRight.y   +   MainVec.x * vecRight.x);
		OffsetWidth1 =  SizeBBX * (MainVec.x * vecRight.y   -   MainVec.y * vecRight.x);
		OffsetLength2 = SizeBBY * (MainVec.y * vecForward.y   +   MainVec.x * vecForward.x);
		OffsetWidth2 =  SizeBBY * (MainVec.x * vecForward.y   -   MainVec.y * vecForward.x);
	}

	MainVecPerp = Vector3(MainVec.y, -MainVec.x, 0.0f);

	OffsetLength = ABS(OffsetLength1) + ABS(OffsetLength2);
	OffsetWidth = ABS(OffsetWidth1) + ABS(OffsetWidth2);

	*pPoint1 = HigherPoint + OffsetLength * MainVec - OffsetWidth * MainVecPerp;
	*pPoint2 = HigherPoint + OffsetLength * MainVec + OffsetWidth * MainVecPerp;
	*pPoint3 = LowerPoint - OffsetLength * MainVec + OffsetWidth * MainVecPerp;
	*pPoint4 = LowerPoint - OffsetLength * MainVec - OffsetWidth * MainVecPerp;

	pPoint1->z = pPoint2->z = pPoint3->z = pPoint4->z = vThisPosition.z;
}
#endif


C2dEffect* CEntity::GetRandom2dEffect(s32 effectType, bool UNUSED_PARAM(mustBeFree) )
{
	#define MAX_CANDIDATES (32)
	s32 numCandidates = 0;
	C2dEffect* pCandidates[MAX_CANDIDATES];

	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if(effectType != ET_LIGHT)
	{
		GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
		for (s32 i=0; i < iNumEffects; i++)
		{
			C2dEffect* pEffect = (*pa2dEffects)[i];

			if (pEffect->GetType() == effectType)
			{
				//if (!mustBeFree || GetPedAttractorManager()->HasEmptySlot(pEffect, this))
				{
					if (numCandidates<MAX_CANDIDATES)
					{
						pCandidates[numCandidates] = pEffect;
						numCandidates++;
					}
				}
			}
		}
	}
	else
	{
		GET_2D_EFFECTS(pBaseModel);
		for (s32 j=0; j<numEffects; j++)
		{		
			C2dEffect* pEffect = a2dEffects[j];
		
			if (pEffect->GetType() == effectType)
			{
				//if (!mustBeFree || GetPedAttractorManager()->HasEmptySlot(pEffect, this))
				{
					if (numCandidates<MAX_CANDIDATES)
					{
						pCandidates[numCandidates] = pEffect;
						numCandidates++;
					}
				}
			}
		}
	}

	if (numCandidates)
	{
		return pCandidates[fwRandom::GetRandomNumberInRange(0, numCandidates)];
	}
	
	return NULL;
}

C2dEffect* CEntity::GetClosest2dEffect(s32 effectType, const Vector3& vPos)
{
	C2dEffect* pCandidate = NULL;
	float fBestDist = 0.0f;

	CBaseModelInfo* pBaseModel = GetBaseModelInfo();

	if(effectType != ET_LIGHT)
	{
		GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
		for (s32 i=0; i < iNumEffects; i++)
		{
			C2dEffect* pEffect = (*pa2dEffects)[i];
			if(pEffect->GetType() == effectType)
			{
				Vector3 vEffectPos = pEffect->GetWorldPosition(this);
				float fDist = vEffectPos.Dist2(vPos);
				if( !pCandidate || fDist < fBestDist )
				{
					pCandidate = pEffect;
					fBestDist = fDist;
				}
			}
		}
	}
	else
	{
		GET_2D_EFFECTS(pBaseModel);
		for (s32 j=0; j<numEffects; j++)
		{
			C2dEffect* pEffect = a2dEffects[j];

			if (pEffect->GetType() == effectType)
			{
				Vector3 vEffectPos = pEffect->GetWorldPosition(this);
				float fDist = vEffectPos.Dist2(vPos);
				if( !pCandidate || fDist < fBestDist )
				{
					pCandidate = pEffect;
					fBestDist = fDist;
				}
			}
		}
	}

	return pCandidate;
}

//void
//CEntity::SetReducedBBForPathfind(bool bOn)
//{
//	m_nFlags.bUsingReducedBBForPathfind = bOn;
//}

bool 
CEntity::IsReducedBBForPathfind() const
{
	TDynamicObjectIndex iObject = PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS;
	if (GetType() == ENTITY_TYPE_OBJECT)
		iObject = ((CObject*)this)->GetPathServerDynamicObjectIndex();
	else if (GetType() == ENTITY_TYPE_VEHICLE)
		iObject = ((CVehicle*)this)->GetPathServerDynamicObjectIndex();

	if (iObject < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS)
	{
		return CPathServer::m_PathServerThread.GetDynamicObject(iObject).m_bForceReducedBoundingBox;
	}

	return false;
//	return m_nFlags.bUsingReducedBBForPathfind;
}

bool
CEntity::IsInPathServer(void) const
{
	if(GetType() == ENTITY_TYPE_OBJECT)
	{
		return (((CObject*)this)->GetPathServerDynamicObjectIndex() < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);
	}
	else if(GetType() == ENTITY_TYPE_VEHICLE)
	{
		return (((CVehicle*)this)->GetPathServerDynamicObjectIndex() < PATHSERVER_MAX_NUM_DYNAMIC_OBJECTS);
	}
	return false;
}

bool
CEntity::WasUnableToAddToPathServer(void)
{
	if(GetType() == ENTITY_TYPE_OBJECT)
	{
		return (((CObject*)this)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
	}
	else if(GetType() == ENTITY_TYPE_VEHICLE)
	{
		return (((CVehicle*)this)->GetPathServerDynamicObjectIndex() == DYNAMIC_OBJECT_INDEX_UNABLE_TO_ADD);
	}
	return false;
}

// GTA_RAGE_PHYSICS
//
// name:		CEntity::SetPhysicsInst
// description:	
void CEntity::SetPhysicsInst(phInst * inst, bool bAddPhysRef)
{
	DR_ONLY(debugPlayback::CurrentInstChange(GetCurrentPhysicsInst(), inst));

	fwEntity::SetCurrentPhysicsInst(inst);

	// modelindex might not be setup yet (in the case of objects in the fragment cache)
	if(bAddPhysRef)
	{
		Assert(IsArchetypeSet());
		CBaseModelInfo* pModelInfo = GetBaseModelInfo();

		if(m_nFlags.bIsFrag)
		{
			pModelInfo->AddRef();
		}

#if __ASSERT
		if(pModelInfo->GetFragType())
		{
			Assert(inst->GetClassType() >= PH_INST_FRAG_GTA);
		}
#endif	//__ASSERT
	}
}


//
// name:		CPhysical::DeleteInst
// description:	Delete physics instance
void CEntity::DeleteInst(bool ASSERT_ONLY(bCalledFromDestructor))
{
	phInst *physicsInst = GetCurrentPhysicsInst();

	if(physicsInst)
	{
		u32 instClassType = physicsInst->GetClassType();
		Assert(instClassType != fragInst::PH_FRAG_INST);
		Assert(instClassType != PH_INST_FRAG_GTA);

		if (instClassType == PH_INST_FRAG_CACHE_OBJECT)
		{
			// these fragment instances were created inside the fragment manager
			// let the fragment manager take care of deleting them
			Assertf(bCalledFromDestructor==false, "Can't delete a fragment cache object by calling delete!");
			// AF: Should I set the physics instance to NULL
		}
		else if (instClassType >= PH_INST_FRAG_GTA)
		{
			fragInst* pFragInst = GetFragInst();
					
			const fragCacheEntry* cacheEntry = pFragInst->GetCacheEntry();
			const bool hasCloth = (cacheEntry && cacheEntry->GetHierInst() && (0 != cacheEntry->GetHierInst()->envCloth)) ? true: false;
			if( hasCloth )
			{
				PHLEVEL->SetInstanceTypeAndIncludeFlags( physicsInst->GetLevelIndex(), physicsInst->GetArchetype()->GetTypeFlags(), 0 );
				physicsInst->SetUserData(NULL);
			}

			physicsInst->SetInstFlag(phInstGta::FLAG_BEING_DELETED, true);

			// these fragment instances were created by the game code
			if(((fragInst *)physicsInst)->GetInserted() && CPhysics::GetFragManager())
			{
				// Set user data to null to prevent any callbacks
				if(physicsInst->GetInstFlag(phfwInst::FLAG_USERDATA_ENTITY))
				{
					physicsInst->SetUserData(NULL);
				}
				((fragInst*)physicsInst)->Remove();
			}
			else
			{
				Assert(0);
			}

			Assert(!physicsInst->IsInLevel());
			delete physicsInst;
		}
		else
		{
			Assert(!physicsInst->IsInLevel());
			delete physicsInst;
		}

		ClearCurrentPhysicsInst();

		// decrement physics reference count
		CBaseModelInfo* pModelInfo = GetBaseModelInfo();

		if( m_nFlags.bIsFrag )
		{
			pModelInfo->RemoveRef();
		}
		else if (pModelInfo->GetHasBoundInDrawable())
		{
			pModelInfo->RemoveRefFromDrawablePhysics();
		}
	}
}

//
// name:		CEntity::InitPhys
// description:	Initialise physics
int CEntity::InitPhys()
{
	if(IsArchetypeSet() && GetCurrentPhysicsInst()==NULL)
	{
		// Create the physics instance for this object
		CBaseModelInfo* pModelInfo = GetBaseModelInfo();

		if(pModelInfo->GetFragType())
		{
			m_nFlags.bIsFrag = true;
			const Matrix34 mat = MAT34V_TO_MATRIX34(GetMatrix());

			fragInstGta* pFragInst = rage_new fragInstGta(PH_INST_FRAG_BUILDING, pModelInfo->GetFragType(), mat);
			pFragInst->SetManualSkeletonUpdate(true);

			SetPhysicsInst(pFragInst, true);

			pFragInst->Insert(false);


		}
		else if (pModelInfo->GetHasBoundInDrawable())
		{
			if ( pModelInfo->CreatePhysicsArchetype() )
			{
				pModelInfo->AddRefToDrawablePhysics();

				phInst* pNewInst = rage_new phInstGta(PH_INST_BUILDING);
				const Matrix34 mat = MAT34V_TO_MATRIX34(GetMatrix());
				pNewInst->Init(*pModelInfo->GetPhysics(), mat);
				SetPhysicsInst(pNewInst, true);
			}
#if PHYSICS_REQUEST_LIST_ENABLED
			else
			{
				CPhysics::AddToPhysicsRequestList(this);
			}
#endif	//PHYSICS_REQUEST_LIST_ENABLED

		}
	}

	return CPhysical::INIT_OK;
}

//
// name:		CEntity::GetCollider
// description:	Return physics collider
phCollider* CEntity::GetCollider()
{
	return GetCurrentCollider();
}

//
// name:		CEntity::GetCollider
// description:	Return physics collider
const phCollider* CEntity::GetCollider() const
{
	return GetCurrentCollider();
}

bool CEntity::GetIsStatic() const
{
	return GetCurrentCollider() == NULL;
}

bool CEntity::GetIsFixedRoot() const
{
	return	GetFragInst() && 
			GetFragInst()->GetCacheEntry() && 
			GetFragInst()->GetCacheEntry()->GetHierInst()->body && 
			GetFragInst()->GetCacheEntry()->GetHierInst()->body->RootIsFixed();
}

//
// name:		CEntity::GetPhysArch
// description:	Return physics archetype
phArchetypeDamp* CEntity::GetPhysArch()
{
	if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->GetArchetype())
		return static_cast<phArchetypeDamp*>(GetCurrentPhysicsInst()->GetArchetype());

	return static_cast<phArchetypeDamp*>(GetBaseModelInfo()->GetPhysics());
}

//
// name:		CEntity::GetPhysArch
// description:	Return physics archetype
const phArchetypeDamp* CEntity::GetPhysArch() const
{
	if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->GetArchetype())
		return static_cast<phArchetypeDamp*>(GetCurrentPhysicsInst()->GetArchetype());

	return static_cast<phArchetypeDamp*>(GetBaseModelInfo()->GetPhysics());
}

// add and remove for physics world
//
// name:		CEntity::AddPhysics
// description:	Add physics instance to physics level
void CEntity::AddPhysics()
{
	phInst* pInst = GetCurrentPhysicsInst();
	if(pInst && pInst->GetArchetype() && !pInst->IsInLevel())
	{
		UpdatePhysicsFromEntity(true);

		Assert(GetMatrix().IsOrthonormal3x3( ScalarVFromF32(0.01f) ));
		CPhysics::GetSimulator()->AddFixedObject(pInst);

		// If we are re-adding the object to the physics level, configure the collision include
		// flags accordingly since they are not cached in the physics inst.
		physicsAssert(pInst->IsInLevel());
		IsCollisionEnabled() ? EnableCollision() : DisableCollision();
	}
}

//
// name:		CEntity::RemovePhysics
// description:	Remove physics instance from physics level
void CEntity::RemovePhysics()
{
	phInst *pPhysInst = GetCurrentPhysicsInst();

	if(pPhysInst)
	{
		if(pPhysInst->IsInLevel())
		{
			CPhysics::GetSimulator()->DeleteObject(pPhysInst->GetLevelIndex());
		}
	}
}

void CEntity::GainingFragCacheEntry(fragCacheEntry& entry)
{
	if(crSkeleton* pSkeleton = entry.GetHierInst()->skeleton)
	{
		Assert(pSkeleton == GetSkeleton());
		if(fwAnimDirector* pDirector = GetAnimDirector())
		{
			if(fwAnimDirectorComponentCreature* createrComponent = pDirector->GetComponent<fwAnimDirectorComponentCreature>())
			{
				createrComponent->ChangeSkeleton(*pSkeleton);
			}
		}
	}
}

void CEntity::LosingFragCacheEntry()
{
	if(fwAnimDirector* pDirector = GetAnimDirector())
	{
		pDirector->WaitForAnyActiveUpdateToComplete(false);
	}
}

void CEntity::EnableCollision(phInst* pInst, u32 nIncludeflags)
{
	// This flag can be read to query whether this entity has had its include flags configured so as not to
	// collide with other entities by game code / script. It will also be used to determine whether
	// the flags need to be set up later if the physics instance wasn't added to the level when this function
	// was first called.
	AssignProtectedBaseFlag(fwEntity::USES_COLLISION, true);

	// Just use the current physics inst if we haven't been given a particular instance.
	if(!pInst)
	{
		pInst = GetCurrentPhysicsInst();
	}

#if __DEV
    if( GetIsTypeVehicle() && !pInst )
    {
        sysStack::PrintStackTrace();
        Warningf( "EnableCollision will fail due to no pInst on vehicle 0x%p '%s'.", this, GetModelName() );
    }
#endif // __DEV

	// If there is an attachment then this call will override its initial collision flags.
	fwAttachmentEntityExtension* pAttachmentExtension = GetAttachmentExtension();
	if(pAttachmentExtension)
	{
		pAttachmentExtension->SetAttachFlag(ATTACH_FLAG_COL_ON, true);
	}

	if(pInst)
	{
		int nLevelIndex = pInst->GetLevelIndex();
		if(CPhysics::GetLevel()->IsInLevel(nLevelIndex))
		{
			u32 nOrigIncludeFlags = nIncludeflags;

			if(nOrigIncludeFlags == 0)
			{
				nOrigIncludeFlags = GetDefaultInstanceIncludeFlags(pInst);
			}

			CPhysics::GetLevel()->SetInstanceIncludeFlags(nLevelIndex, nOrigIncludeFlags);
		}
#if __DEV
        else
        {

            if( GetIsTypeVehicle() )
            {
                sysStack::PrintStackTrace();
                Warningf( "EnableCollision will fail due to vehicle not being in the level 0x%p '%s'.", this, GetModelName() );
            }
        }
#endif // __DEV
	}
}

void CEntity::DisableCollision(phInst* pInst, bool bCompletelyDisabled)
{
#if __DEV
	if(GetIsTypeVehicle() && GetCollider() != NULL)
	{
		sysStack::PrintStackTrace();
		Warningf("Disabling collision on active vehicle 0x%p '%s'. It will probably fall through the world.",this,GetModelName());
	}
#endif // __DEV

	// This flag can be read to query whether this entity has had its include flags configured so as not to
	// collide with other entities by game code / script. It will also be used to determine whether
	// the flags need to be set up later if the physics instance wasn't added to the level when this function
	// was first called.
	AssignProtectedBaseFlag(fwEntity::USES_COLLISION, false);

	// Just use the current physics inst if we haven't been given a particular instance.
	if(!pInst)
	{
		pInst = GetCurrentPhysicsInst();
	}

	// If there is an attachment then this call will override its initial collision flags.
	fwAttachmentEntityExtension* pAttachmentExtension = GetAttachmentExtension();
	if(pAttachmentExtension)
	{
		pAttachmentExtension->SetAttachFlag(ATTACH_FLAG_COL_ON, false);
	}

	if(pInst)
	{
		int nLevelIndex = pInst->GetLevelIndex();
		if(CPhysics::GetLevel()->IsInLevel(nLevelIndex))
		{
			u32 includeFlags = bCompletelyDisabled ? 0 : ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES;
			CPhysics::GetLevel()->SetInstanceIncludeFlags(nLevelIndex, includeFlags);
		}
	}
}

bool CEntity::GetIsCollisionCompletelyDisabled()
{
	bool isDisabled = false;

	phInst* currentInst = GetCurrentPhysicsInst();

	if(currentInst)
	{
		int nLevelIndex = currentInst->GetLevelIndex();
		if(CPhysics::GetLevel()->IsInLevel(nLevelIndex))
		{
			isDisabled = ( CPhysics::GetLevel()->GetInstanceIncludeFlags(nLevelIndex) == 0 );
		}
	}

	return isDisabled;
}

void CEntity::SetIsCollisionCompletelyDisabled()
{
	// Disable collision completely on the current physics inst
	DisableCollision(NULL, true);
}

//
// name:		CEntity::UpdatePhysicsFromEntity
// description:	
void CEntity::UpdatePhysicsFromEntity(bool UNUSED_PARAM(bWarp))
{
	if(GetCurrentPhysicsInst())
	{
		GetCurrentPhysicsInst()->SetMatrix(GetMatrix());
		if(GetCurrentPhysicsInst()->IsInLevel())
		{
			Assert(GetCollider()==NULL);
			CPhysics::GetLevel()->UpdateObjectLocation(GetCurrentPhysicsInst()->GetLevelIndex());
		}
	}
}

void CEntity::GeneratePhysExclusionList(const CEntity** ppExclusionListStorage, int& nStorageUsed, int nMaxStorageSize, u32 UNUSED_PARAM(includeFlags), u32 UNUSED_PARAM(typeFlags), const CEntity* pOptionalExclude) const
{
	if(pOptionalExclude && nStorageUsed<nMaxStorageSize)
	{
		ppExclusionListStorage[nStorageUsed++] = pOptionalExclude;
	}
}

void CEntity::GeneratePhysInstExclusionList(const phInst** ppExclusionListStorage, const CEntity** ppExclusionListSource, int nSourceSize)
{
	for(int i=0; i<nSourceSize; ++i)
	{
		ppExclusionListStorage[i] = ppExclusionListSource[i] ? ppExclusionListSource[i]->GetCurrentPhysicsInst() : NULL;
	}
}

u32 CEntity::GetDefaultInstanceIncludeFlags(phInst* pInst) const
{
	u32 nOrigIncludeFlags = 0;

	// Just use the current physics inst if we haven't been given a particular instance.
	if(!pInst)
	{
		pInst = GetCurrentPhysicsInst();
	}

	// Reset the archetype flags back to model info default.
	fwArchetype* pModelInfo = m_pArchetype;
	Assert(pModelInfo);
	fragInst* pFragInst = GetFragInst();		
	if(pInst == pFragInst)
	{
		// Deal with frag, get original include flags from type.
		Assert(pFragInst);
		Assert(pFragInst->GetTypePhysics());
		Assert(pFragInst->GetTypePhysics()->GetArchetype());
		nOrigIncludeFlags = pFragInst->GetTypePhysics()->GetArchetype()->GetIncludeFlags();
	}
	else if(Verifyf(pModelInfo && pModelInfo->GetPhysics(), "Model '%s' has no physics archetype", pModelInfo ? pModelInfo->GetModelName() : "NULL"))
	{
		nOrigIncludeFlags = pModelInfo->GetPhysics()->GetIncludeFlags();
	}

	return nOrigIncludeFlags;
}

void CEntity::SetMaxSpeed(float fMaxSpeed)
{
	CBaseModelInfo* pBaseModelInfo		= GetBaseModelInfo();
	phInst*			pCurrentPhysicsInst	= GetCurrentPhysicsInst();
	phArchetype*	pArchetype			= NULL;

	if( pBaseModelInfo && pCurrentPhysicsInst )
	{
		pArchetype = pCurrentPhysicsInst->GetArchetype();
	}

	if( pArchetype )
	{
		// Clone archetype before changing the max speed.
		if( pArchetype == pBaseModelInfo->GetPhysics())
		{						
			phArchetype *pNewArchetype = pArchetype->Clone();					
			pCurrentPhysicsInst->SetArchetype( pNewArchetype );
			pArchetype = pCurrentPhysicsInst->GetArchetype();
		}

		phArchetypeDamp* pPhysics = pBaseModelInfo->GetPhysics();

		if( pPhysics )
		{
			float fArchetypeMaxSpeed = pPhysics->GetMaxSpeed();

			if(fMaxSpeed == -1)
			{
				static_cast<phArchetypePhys*>( pArchetype )->SetMaxSpeed( fArchetypeMaxSpeed );
			}
			else
			{
				Assertf(fMaxSpeed <= fArchetypeMaxSpeed, "Setting entity max speed (%f) that is greater than archetype max speed (%f). Use a parameter of -1 to reset back to the default max speed.", fMaxSpeed, fArchetypeMaxSpeed);						
				static_cast<phArchetypePhys*>( pArchetype )->SetMaxSpeed( fMaxSpeed );

				// Propagate this change to the collider, if it exists.
				// Not doing this now would mean the entity would only respond to the max speed change when it next gets activated and gets given a new collider.
				if( GetCollider() )
				{
					GetCollider()->SetMaxSpeed( fMaxSpeed );
				}
			}
		}
	}
}

//
// name:		CEntity::SetNetworkVisibility
// description:	This function is required for cases where
//              an entities visibility status is different on the
//              local machine to what it should be on remote machines
void CEntity::SetNetworkVisibility(bool bVisible, const char* reason)
{
    netObject *pNetworkObject = NetworkUtils::GetNetworkObjectFromEntity(this);

    if(pNetworkObject)
    {
        CEntity *pEntity = pNetworkObject->GetEntity();

        Assert(pEntity);
        Assert(pEntity==this);

        if(pEntity)
        {
            CNetObjEntity *pNetObjEntity = static_cast<CNetObjEntity *>(pNetworkObject);

            pNetObjEntity->SetOverridingRemoteVisibility(true, bVisible, reason);
        }
    }
}

//
//
//
//
void CEntity::ChangeVisibilityMask(u16 flags, bool value, bool bProcessChildAttachments)
{
	GetRenderPhaseVisibilityMask().ChangeFlag(flags, value);
	RequestUpdateInWorld();

	if (bProcessChildAttachments)
	{
		CEntity* pAttachChild = static_cast<CEntity*>(GetChildAttachment());
		while (pAttachChild)
		{
			pAttachChild->ChangeVisibilityMask(flags, value, true);
			pAttachChild = static_cast<CEntity*>(pAttachChild->GetSiblingAttachment());
		}
	}
}

//
//
//
//
__forceinline void CEntity::Update2dEffect(const C2dEffect& effect, int index, float lightFade, bool hasFxEntityAmbient, bool hasFxEntityInWater)
{
	if (effect.GetType()==ET_PARTICLE)
	{
		if (hasFxEntityAmbient && effect.GetParticle()->m_fxType==PT_AMBIENT_FX)
		{
			// check if this ambient entity effect only plays on the damage model
			const CParticleAttr* pPtxAttr = effect.GetParticle();
			if (pPtxAttr->m_onlyOnDamageModel)
			{
				bool isDamageModel = false;
				fragInst* pFragInst = GetFragInst();
				if (pFragInst && pFragInst->GetCached())
				{
					// get the bone index from the bone tag
					const fragType* pFragType = static_cast<fragInstGta*>(GetFragInst())->GetType();
					ptfxAssertf(pFragType, "CEntity::Update2dEffects - couldn't get frag type from frag inst");

					int boneIndex = 0;
					if (pFragType->GetSkeletonData().ConvertBoneIdToIndex((u16)pPtxAttr->m_boneTag, boneIndex))
					{
						// get the component id from the bone index
						s32 componentId = pFragInst->GetComponentFromBoneIndex(boneIndex);
						if (componentId!=-1)
						{
							if (Verifyf(componentId>=0 && componentId<pFragInst->GetTypePhysics()->GetNumChildren(), "%s has converted a bone index into an invalid component id (%d - num compenents is %d)", GetModelName(), componentId, GetFragInst()->GetTypePhysics()->GetNumChildren()))
							{
								fragTypeChild** ppChildren = pFragInst->GetTypePhysics()->GetAllChildren();
								if (ppChildren[componentId])
								{
									// check if this component uses a damage model
									fragTypeChild* pChild = ppChildren[componentId];
									u8 groupIndex = pChild->GetOwnerGroupPointerIndex();
									if (pFragInst->GetCacheEntry()->GetHierInst()->groupInsts[groupIndex].IsDamaged())
									{
										isDamageModel = true;
									}
								}
							}
						}
					}
				}

				if (!isDamageModel)
				{
					return;
				}
			}

			g_vfxEntity.ProcessPtFxEntityAmbient(this, effect.GetParticle(), index);
		}
		else if (hasFxEntityInWater && effect.GetParticle()->m_fxType==PT_INWATER_FX)
		{
			g_vfxEntity.TriggerPtFxEntityInWater(this, effect.GetParticle());
		}
	}
	else if (effect.GetType()==ET_SCROLLBAR)
	{
		g_scrollbars.Register(this, *(effect.GetScrollBar()), lightFade);
	}
	else if (effect.GetType()==ET_WINDDISTURBANCE)
	{
		g_weather.ProcessEntityWindDisturbance(this, effect.GetWindDisturbance(), index);
	}

}
void CEntity::Update2dEffects(float lightFade, const atArray<C2dEffect*>* pEffectArray, const atArray<CLightAttr>* pLightArray)
{
	// only update the 2d effects if all dependencies are loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	const bool hasFxEntityAmbient = pBaseModel->GetHasFxEntityAmbient() != 0;
	const bool hasFxEntityInWater = pBaseModel->GetHasFxEntityInWater() != 0;

	int index = 0;
	if(pEffectArray)
	{
		int numEffects = pEffectArray->GetCount();
		for (s32 i=0; i<numEffects; i++)
		{
			C2dEffect* pEffect = (pEffectArray->GetElements())[i];
			Update2dEffect(*pEffect, index++, lightFade, hasFxEntityAmbient, hasFxEntityInWater);
		}
	}

	if(pLightArray)
	{
		int numEffects = pLightArray->GetCount();
		for (s32 i=0; i<numEffects; i++)
		{
			const C2dEffect* pEffect = static_cast<const C2dEffect*>(&((pLightArray->GetElements())[i]));
			Update2dEffect(*pEffect, index++, lightFade, hasFxEntityAmbient, hasFxEntityInWater);
		}
	}
}

//
//
// Render roadsign stuff (if present):
//
//
void CEntity::Render2dEffects()
{
/*	if(this->m_nFlags.bHasRoadsignText)
	{
		CBaseModelInfo *pBaseModel = GetBaseModelInfo();

		const s32 numEffects = pBaseModel->GetNum2dEffects();
		if(numEffects>0)
		{
			for(s32 i=0; i<numEffects; i++)
			{
				C2dEffect	*pEffect = pBaseModel->Get2dEffect(i);
				if(pEffect->m_type == ET_ROADSIGN)
				{
					CCustomRoadsignMgr::RenderRoadsignAtomic(pEffect->attr.rs.m_rsAtomic, TheCamera.GetPosition());				
					continue;
				}
			}
		}
	}//if(this->m_nFlags.bHasRoadsignText)...
*/
}// end of CEntity::Render2dEffects()...


///////////////////////////////////////////////////////////////////////////////
//  ProcessFxEntityCollision
///////////////////////////////////////////////////////////////////////////////

void CEntity::ProcessFxEntityCollision(const Vector3& pos, const Vector3& normal, const u32 componentId, float accumImpulse)
{
	// return if the model id isn't valid
	if (!IsArchetypeSet())
	{
		return;
	}

	// return if the model dependencies aren't all loaded - we don't want to be playing a particle effect that isn't loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if (pBaseModel->GetHasLoaded()==false)
	{
		return;
	}

	// return if this model doesn't have any collision vfx
	if (pBaseModel->GetHasFxEntityCollision()==false)
	{
		return;
	}

	// calc the bonetag to deal with
	bool isDamageModel = false;
	s32 collidedBoneTag = 0;
	fragInst* pFragInst = GetFragInst();
	if (pFragInst)
	{
		fragTypeChild** ppChildren = GetFragInst()->GetTypePhysics()->GetAllChildren();
		Assertf(componentId<GetFragInst()->GetTypePhysics()->GetNumChildren(), "%s - %d/%d", GetModelName(), componentId, GetFragInst()->GetTypePhysics()->GetNumChildren());
		if (ppChildren[componentId]==NULL)
		{
			return;
		}

		if (pFragInst->GetCached())
		{
			fragTypeChild* pChild = ppChildren[componentId];
			u8 groupIndex = pChild->GetOwnerGroupPointerIndex();
			if (pFragInst->GetCacheEntry()->GetHierInst()->groupInsts[groupIndex].IsDamaged())
			{
				isDamageModel = true;
			}
		}

		const fragType* pFragType = static_cast<fragInstGta*>(GetFragInst())->GetType();
		s32 boneIndex = pFragType->GetBoneIndexFromID(ppChildren[componentId]->GetBoneID());
		if (boneIndex==-1)
		{
			return;
		}
		// Note that it is impossible for GetBoneData to fail to return NULL since it is just an array lookup
		// Also, GetSkeletonData should never fail for a fragType
		collidedBoneTag = pFragType->GetSkeletonData().GetBoneData(boneIndex)->GetBoneId();
	}

	
	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];

		if (pEffect->GetType()==ET_PARTICLE)
		{
			CParticleAttr* pPtxAttr = pEffect->GetParticle();

			if (!isDamageModel || !pPtxAttr->m_ignoreDamageModel)
			{
				if (pPtxAttr->m_fxType==PT_COLLISION_FX && (pPtxAttr->m_boneTag==collidedBoneTag || pPtxAttr->m_boneTag==-1))
				{
					g_vfxEntity.TriggerPtFxEntityCollision(this, pPtxAttr, RCC_VEC3V(pos), RCC_VEC3V(normal), collidedBoneTag, accumImpulse);
				}
			}
		}
		else if (pEffect->GetType()==ET_DECAL)
		{
			CDecalAttr* pDecalAttr = pEffect->GetDecal();

			if (!isDamageModel || !pDecalAttr->m_ignoreDamageModel)
			{
				if (pDecalAttr->m_decalType==DT_COLLISION_FX && (pDecalAttr->m_boneTag==collidedBoneTag || pDecalAttr->m_boneTag==-1))
				{
					g_vfxEntity.TriggerDecalEntityCollision(this, pDecalAttr, RCC_VEC3V(pos), RCC_VEC3V(normal), collidedBoneTag, accumImpulse);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessFxEntityShot
///////////////////////////////////////////////////////////////////////////////

void CEntity::ProcessFxEntityShot(const Vector3& pos, const Vector3& normal, const u32 componentId, const CWeaponInfo* pWeaponInfo, CEntity* pFiringEntity)
{
	// return if the model id isn't valid
	if (!IsArchetypeSet())
	{
		return;
	}

	// return if the model dependencies aren't all loaded - we don't want to be playing a particle effect that isn't loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if (pBaseModel->GetHasLoaded()==false)
	{
		return;
	}

	// return if this model doesn't have any shot vfx
	if (pBaseModel->GetHasFxEntityShot()==false)
	{
		return;
	}

	// calc the bonetag to deal with
	bool isDamageModel = false;
	s32 shotBoneTag = 0;
	fragInst* pFragInst = GetFragInst();
	if (pFragInst)
	{
		fragTypeChild** ppChildren = GetFragInst()->GetTypePhysics()->GetAllChildren();
		Assert(componentId<GetFragInst()->GetTypePhysics()->GetNumChildren());
		if (ppChildren[componentId]==NULL)
		{
			return;
		}

		if (pFragInst->GetCached())
		{
			fragTypeChild* pChild = ppChildren[componentId];
			u8 groupIndex = pChild->GetOwnerGroupPointerIndex();
			if (pFragInst->GetCacheEntry()->GetHierInst()->groupInsts[groupIndex].IsDamaged())
			{
				isDamageModel = true;
			}
		}

		const fragType* pFragType = static_cast<fragInstGta*>(GetFragInst())->GetType();
		s32 boneIndex = pFragType->GetBoneIndexFromID(ppChildren[componentId]->GetBoneID());
		if (boneIndex==-1)
		{
			return;
		}
		// Note that it is impossible for GetBoneData to fail to return NULL since it is just an array lookup
		// Also, GetSkeletonData should never fail for a fragType
		shotBoneTag = pFragType->GetSkeletonData().GetBoneData(boneIndex)->GetBoneId();
	}

	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];
		Vector3 effectPos;
		pEffect->GetPos(effectPos);

		if (pEffect->GetType()==ET_PARTICLE)
		{
			CParticleAttr* pPtxAttr = pEffect->GetParticle();
			
			if (!isDamageModel || !pPtxAttr->m_ignoreDamageModel)
			{
				if (pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_BULLET_RUBBER || pPtxAttr->m_allowRubberBulletShotFx)
				{
					if (pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_ELECTRIC || pPtxAttr->m_allowElectricBulletShotFx)
					{
						if(pPtxAttr->m_boneTag==shotBoneTag || pPtxAttr->m_boneTag==-1)
						{
							if (pPtxAttr->m_fxType==PT_SHOT_FX)
							{
								g_vfxEntity.TriggerPtFxEntityShot(this, pPtxAttr, RCC_VEC3V(pos), RCC_VEC3V(normal), shotBoneTag);
							}
						}
					}
				}
			}
		}
		else if (pEffect->GetType()==ET_DECAL)
		{
			CDecalAttr* pDecalAttr = pEffect->GetDecal();

			if (!isDamageModel || !pDecalAttr->m_ignoreDamageModel)
			{
				if (pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_BULLET_RUBBER || pDecalAttr->m_allowRubberBulletShotFx)
				{
					if (pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_ELECTRIC || pDecalAttr->m_allowElectricBulletShotFx)
					{
						if (pDecalAttr->m_boneTag==shotBoneTag || pDecalAttr->m_boneTag==-1)
						{
							if (pDecalAttr->m_decalType==DT_SHOT_FX)
							{
								g_vfxEntity.TriggerDecalEntityShot(this, pDecalAttr, RCC_VEC3V(pos), RCC_VEC3V(normal), shotBoneTag);
							}
						}
					}
				}
			}
		}
		else if (pEffect->GetType()==ET_EXPLOSION) 
		{
			CExplosionAttr* pExpAttr = pEffect->GetExplosion();
			
			// clone gunfire is not allowed to create explosions
			if (NetworkUtils::IsNetworkClone(pFiringEntity))
			{
				return;
			}

			if (!isDamageModel || !pExpAttr->m_ignoreDamageModel)
			{
				if (pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_BULLET_RUBBER || pExpAttr->m_allowRubberBulletShotFx)
				{
					if (pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_ELECTRIC || pExpAttr->m_allowElectricBulletShotFx)
					{
						// Don't spawn explosions when shooting bulletproof objects. VFX without side effects like decals are expected though.
						if(!GetIsPhysical() || !static_cast<const CPhysical*>(this)->m_nPhysicalFlags.bNotDamagedByBullets)
						{
							if ((pExpAttr->m_boneTag==shotBoneTag || pExpAttr->m_boneTag==-1))
							{
								if (pExpAttr->m_tagHashName.GetHash()!=0)
								{
									eExplosionTag expTag = eExplosionTag_Parse(pExpAttr->m_tagHashName);
									vfxAssertf(expTag!=EXP_TAG_DONTCARE, "invalid explosion tag on entity [%s]", GetDebugName());

									if (pExpAttr->m_explosionType==XT_SHOT_PT_FX)
									{
										if (!m_nFlags.bHasExploded || CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
										{
											CDummyObject *relatedDummyForNetwork = 0;
											bool bNetworkExplosion = true;

											if (!CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
											{
												SetHasExploded(&bNetworkExplosion);
											}

											if(GetIsTypeObject())
											{
												CObject *pObject = static_cast<CObject *>(this);
												relatedDummyForNetwork = pObject->GetRelatedDummy();

												if(relatedDummyForNetwork == 0 && pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject())
												{
													relatedDummyForNetwork = static_cast<CObject *>(pObject->GetFragParent())->GetRelatedDummy();
												}
											}

											CExplosionManager::CExplosionArgs explosionArgs(expTag, pos);
											explosionArgs.m_vDirection = normal;
											explosionArgs.m_pAttachEntity = this;
											explosionArgs.m_attachBoneTag = shotBoneTag;
											explosionArgs.m_pRelatedDummyForNetwork = relatedDummyForNetwork;
											explosionArgs.m_pExplodingEntity = this;
											explosionArgs.m_pEntExplosionOwner = pFiringEntity;
											explosionArgs.m_bIsLocalOnly = !bNetworkExplosion;
											CExplosionManager::AddExplosion(explosionArgs);
										}
									}
									else if (pExpAttr->m_explosionType==XT_SHOT_GEN_FX)
									{
										if (pExpAttr->m_tagHashName.GetHash()!=0)
										{
											eExplosionTag expTag = eExplosionTag_Parse(pExpAttr->m_tagHashName);
											vfxAssertf(expTag!=EXP_TAG_DONTCARE, "invalid explosion tag on entity [%s]", GetDebugName());

											if (!m_nFlags.bHasExploded || CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
											{
												// get the matrix that the 2d effect is offset from 
												Matrix34 boneMtx;
												CVfxHelper::GetMatrixFromBoneTag(RC_MAT34V(boneMtx), this, (eAnimBoneTag)shotBoneTag);

												// return if we don't have a valid matrix
												if (boneMtx.a.IsZero())
												{
													continue;
												}

												// get the 2d effect offset matrix (from parent bone)
												Matrix34 expMat;
												expMat.FromQuaternion(Quaternion(pExpAttr->qX, pExpAttr->qY, pExpAttr->qZ, pExpAttr->qW));
												expMat.d = effectPos;

												// transform this into world space
												expMat.Dot(boneMtx);

												CDummyObject *relatedDummyForNetwork = 0;
												bool bNetworkExplosion = true;

												if (!CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
												{
													SetHasExploded(&bNetworkExplosion);
												}

												if(GetIsTypeObject())
												{
													CObject *pObject = static_cast<CObject *>(this);
													relatedDummyForNetwork = pObject->GetRelatedDummy();

													if(relatedDummyForNetwork == 0 && pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject())
													{
														relatedDummyForNetwork = static_cast<CObject *>(pObject->GetFragParent())->GetRelatedDummy();
													}
												}

												CExplosionManager::CExplosionArgs explosionArgs(expTag, expMat.d);
												explosionArgs.m_vDirection = expMat.c;
												explosionArgs.m_pAttachEntity = this;
												explosionArgs.m_attachBoneTag = shotBoneTag;
												explosionArgs.m_pRelatedDummyForNetwork = relatedDummyForNetwork;
												explosionArgs.m_pExplodingEntity = this;
												explosionArgs.m_pEntExplosionOwner = pFiringEntity;
												explosionArgs.m_bIsLocalOnly = !bNetworkExplosion;
												CExplosionManager::AddExplosion(explosionArgs);
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
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessFxFragmentBreak
///////////////////////////////////////////////////////////////////////////////

void CEntity::ProcessFxFragmentBreak(s32 brokenBoneTag, bool isChildEntity, s32 parentBoneTag)
{		
	// return if the model id isn't valid
	if (!IsArchetypeSet())
	{
		return;
	}

	// return if the model dependencies aren't all loaded - we don't want to be playing a particle effect that isn't loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if (pBaseModel->GetHasLoaded()==false)
	{
		return;
	}

	// If this was a light, break its associated LODLight
	CLODLightManager::RegisterBrokenLightsForEntity(this);

	// return if this model doesn't have any break vfx
	if (pBaseModel->GetHasFxEntityBreak()==false)
	{
		return;
	}

	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];
		Vector3 effectPos;
		pEffect->GetPos(effectPos);

		if (pEffect->GetType()==ET_PARTICLE)
		{
			CParticleAttr* pPtxAttr = pEffect->GetParticle();
			if (pPtxAttr->m_playOnParent!=isChildEntity)
			{
				if (pPtxAttr->m_fxType==PT_BREAK_FX && (pPtxAttr->m_boneTag==brokenBoneTag || pPtxAttr->m_boneTag==-1))
				{
					g_vfxEntity.TriggerPtFxFragmentBreak(this, pPtxAttr, brokenBoneTag, parentBoneTag);
				}
			}
		}
		else if (pEffect->GetType()==ET_DECAL)
		{
			CDecalAttr* pDecalAttr = pEffect->GetDecal();
			if (pDecalAttr->m_playOnParent!=isChildEntity)
			{
				if (pDecalAttr->m_decalType==DT_BREAK_FX && (pDecalAttr->m_boneTag==brokenBoneTag || pDecalAttr->m_boneTag==-1))
				{
					g_vfxEntity.TriggerDecalFragmentBreak(this, pDecalAttr, brokenBoneTag, parentBoneTag);
				}
			}
		}
		else if (pEffect->GetType()==ET_EXPLOSION)
		{
			CExplosionAttr* pExpAttr = pEffect->GetExplosion();
			if (pExpAttr->m_playOnParent!=isChildEntity)
			{
				if (pExpAttr->m_explosionType==XT_BREAK_FX && (pExpAttr->m_boneTag==brokenBoneTag || pExpAttr->m_boneTag==-1))
				{
					if (pExpAttr->m_tagHashName.GetHash()!=0)
					{
						eExplosionTag expTag = eExplosionTag_Parse(pExpAttr->m_tagHashName);
						vfxAssertf(expTag!=EXP_TAG_DONTCARE, "invalid explosion tag on entity [%s]", GetDebugName());

						if (!m_nFlags.bHasExploded || CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
						{
							CEntity* pInflictor = NULL;
							// Try to figure out what caused the explosion
							if(GetIsTypeObject() || GetIsTypeVehicle() || GetIsTypePed())
							{
								CPhysical* pPhysical = static_cast<CPhysical*>(this);

								const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - pPhysical->GetWeaponDamagedTime();
								const u32 TIME_INTERVAL = 5000;
								if(pPhysical && (timeSinceLastDamage<TIME_INTERVAL || pPhysical->IsNetworkClone()))	// Did it happen this frame or close enough?
								{
									pInflictor = pPhysical->GetWeaponDamageEntity();
								}
							}

							// get world position to play explosion at
							s32 attachBoneTag = brokenBoneTag;
							if (pExpAttr->m_playOnParent)
							{
								attachBoneTag = parentBoneTag;
							}

							Matrix34 boneMtx;
							CVfxHelper::GetMatrixFromBoneTag(RC_MAT34V(boneMtx), this, (eAnimBoneTag)attachBoneTag);

							// return if we don't have a valid matrix
							if (!boneMtx.a.IsZero())
							{	
								Vector3 expPos;
								boneMtx.Transform(effectPos, expPos);

								CDummyObject *relatedDummyForNetwork = 0;
								bool bNetworkExplosion = true;

								if (!CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
								{
									SetHasExploded(&bNetworkExplosion);

									// If we're a major explosion, break the whole object apart
									if (GetIsTypeObject())
									{
										if (fragInst* inst = GetFragInst())
										{
											// If the explosion is attached to a child group, it's the parent we want to blow up
											inst = inst->GetParent();

											// None of this will work if we don't have a cache entry
											if (!inst->GetCached())
											{
												inst->PutIntoCache();
											}

											if (fragCacheEntry* entry = inst->GetCacheEntry())
											{
												fragPhysicsLOD* physicsLod = inst->GetTypePhysics();
												int numGroups = physicsLod->GetNumChildGroups();

												// Don't break group zero (BreakOffAboveGroup will complain if you do...)
												for (int groupIndex = 1; groupIndex < numGroups; ++groupIndex)
												{
													// Only break off parts with non-infinite strength ...
													if (physicsLod->GetGroup(groupIndex)->GetStrength() >= 0.0f)
													{
														// ... that aren't already broken off
														if (entry->GetHierInst()->groupBroken->IsClear(groupIndex))
														{
															inst->BreakOffAboveGroup(groupIndex);
														}
													}
												}
											}
										}
									}
								}

								if(GetIsTypeObject())
								{
									CObject *pObject = static_cast<CObject *>(this);
									relatedDummyForNetwork = pObject->GetRelatedDummy();

									if(relatedDummyForNetwork == 0 && pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject())
									{
										relatedDummyForNetwork = static_cast<CObject *>(pObject->GetFragParent())->GetRelatedDummy();
									}
								}

								CExplosionManager::CExplosionArgs explosionArgs(expTag, expPos);
								explosionArgs.m_pEntExplosionOwner = pInflictor;
								explosionArgs.m_vDirection = VEC3V_TO_VECTOR3(GetTransform().GetC());
								explosionArgs.m_pAttachEntity = this;
								explosionArgs.m_attachBoneTag = attachBoneTag;
								explosionArgs.m_pRelatedDummyForNetwork = relatedDummyForNetwork;
								explosionArgs.m_pExplodingEntity = this;
								explosionArgs.m_bIsLocalOnly = !bNetworkExplosion;
								CExplosionManager::AddExplosion(explosionArgs);
							}
						}
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessFxFragmentDestroy
///////////////////////////////////////////////////////////////////////////////

void CEntity::ProcessFxFragmentDestroy(s32 destroyedBoneTag, Matrix34& fxMat)
{	
	// return if the model id isn't valid
	if (!IsArchetypeSet())
	{
		return;
	}

	// return if the model dependencies aren't all loaded - we don't want to be playing a particle effect that isn't loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if (pBaseModel->GetHasLoaded()==false)
	{
		return;
	}

	// If this was a light, break its associated LODLight
	CLODLightManager::RegisterBrokenLightsForEntity(this);

	// return if this model doesn't have any destroy vfx
	if (pBaseModel->GetHasFxEntityDestroy()==false)
	{
		return;
	}

	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];
		Vector3 effectPos;
		pEffect->GetPos(effectPos);

		if (pEffect->GetType()==ET_PARTICLE)
		{
			CParticleAttr* pPtxAttr = pEffect->GetParticle();

			if (pPtxAttr->m_fxType==PT_DESTROY_FX && (pPtxAttr->m_boneTag==destroyedBoneTag || pPtxAttr->m_boneTag==-1))
			{
				g_vfxEntity.TriggerPtFxFragmentDestroy(this, pPtxAttr, RCC_MAT34V(fxMat));
			}
		}
		else if (pEffect->GetType()==ET_DECAL)
		{
			CDecalAttr* pDecalAttr = pEffect->GetDecal();

			if (pDecalAttr->m_decalType==DT_DESTROY_FX && (pDecalAttr->m_boneTag==destroyedBoneTag || pDecalAttr->m_boneTag==-1))
			{
				g_vfxEntity.TriggerDecalFragmentDestroy(this, pDecalAttr, RCC_MAT34V(fxMat));
			}
		}
		else if (pEffect->GetType()==ET_EXPLOSION)
		{
			CExplosionAttr* pExpAttr = pEffect->GetExplosion();

			if (pExpAttr->m_explosionType==XT_DESTROY_FX && (pExpAttr->m_boneTag==destroyedBoneTag || pExpAttr->m_boneTag==-1))
			{
				if (pExpAttr->m_tagHashName.GetHash()!=0)
				{
					eExplosionTag expTag = eExplosionTag_Parse(pExpAttr->m_tagHashName);
					vfxAssertf(expTag!=EXP_TAG_DONTCARE, "invalid explosion tag on entity [%s]", GetDebugName());

					// check if this entity (or it's parents) have already exploded
					bool bIgnoreBecauseHasAlreadyExploded = m_nFlags.bHasExploded;
					if(CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
						bIgnoreBecauseHasAlreadyExploded = false;
					else if(GetIsTypeObject() && ((CObject*)this)->GetFragParent() && ((CObject*)this)->GetFragParent()->m_nFlags.bHasExploded)
						bIgnoreBecauseHasAlreadyExploded = true;

					if (!bIgnoreBecauseHasAlreadyExploded)
					{
						CEntity* pInflictor = NULL;
						// Try to figure out what caused the explosion
						if(GetIsTypeObject() || GetIsTypeVehicle() || GetIsTypePed())
						{
							CPhysical* pPhysical = static_cast<CPhysical*>(this);

							const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - pPhysical->GetWeaponDamagedTime();
							const u32 TIME_INTERVAL = 5000;
							if(pPhysical && (timeSinceLastDamage<TIME_INTERVAL || pPhysical->IsNetworkClone()))	// Did it happen this frame or close enough?
							{
								pInflictor = pPhysical->GetWeaponDamageEntity();
							}
						}

						Vector3 expPos;
						fxMat.Transform(effectPos, expPos);
						CDummyObject *relatedDummyForNetwork = 0;
						bool bNetworkExplosion = true;

						if (!CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
						{
							SetHasExploded(&bNetworkExplosion);
						}

						if(GetIsTypeObject())
						{
							CObject *pObject = static_cast<CObject *>(this);
							relatedDummyForNetwork = pObject->GetRelatedDummy();

							if(relatedDummyForNetwork == 0 && pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject())
							{
								relatedDummyForNetwork = static_cast<CObject *>(pObject->GetFragParent())->GetRelatedDummy();
							}
						}

						CExplosionManager::CExplosionArgs explosionArgs(expTag, expPos);
						explosionArgs.m_pEntExplosionOwner = pInflictor;
						explosionArgs.m_vDirection = fxMat.c;
						explosionArgs.m_pRelatedDummyForNetwork = relatedDummyForNetwork;
						explosionArgs.m_pExplodingEntity = this;
						explosionArgs.m_bIsLocalOnly = !bNetworkExplosion;
						CExplosionManager::AddExplosion(explosionArgs);
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessFxAnimRegistered
///////////////////////////////////////////////////////////////////////////////

void CEntity::ProcessFxAnimRegistered()
{
	// return if the model id isn't valid
	if (!IsArchetypeSet())
	{
		return;
	}

	// return if the model dependencies aren't all loaded - we don't want to be playing a particle effect that isn't loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if (pBaseModel->GetHasLoaded()==false)
	{
		return;
	}

	// return if this model doesn't have any anim vfx
	if (pBaseModel->GetHasFxEntityAnim()==false)
	{
		return;
	}

	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];

		if (pEffect->GetType()==ET_PARTICLE)
		{
			if (pEffect->GetParticle()->m_fxType==PT_ANIM_FX)
			{
				g_vfxEntity.UpdatePtFxEntityAnim(this, pEffect->GetParticle(), i);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessFxAnimTriggered
///////////////////////////////////////////////////////////////////////////////

void CEntity::ProcessFxAnimTriggered()
{
	// return if the model id isn't valid
	if (!IsArchetypeSet())
	{
		return;
	}

	// return if the model dependencies aren't all loaded - we don't want to be playing a particle effect that isn't loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if (pBaseModel->GetHasLoaded()==false)
	{
		return;
	}

	// return if this model doesn't have any anim vfx
	if (pBaseModel->GetHasFxEntityAnim()==false)
	{
		return;
	}

	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];

		if (pEffect->GetType()==ET_PARTICLE)
		{
			if (pEffect->GetParticle()->m_fxType==PT_ANIM_FX)
			{
				g_vfxEntity.TriggerPtFxEntityAnim(this, pEffect->GetParticle());
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TryToExplode
///////////////////////////////////////////////////////////////////////////////

bool CEntity::TryToExplode(CEntity* pCulprit, bool bNetworkExplosion)
{
	bool bExplosionSuccessful = false;

	// return if the model id isn't valid
	if (!IsArchetypeSet())
	{
		weaponDebugf1("CEntity::TryToExplode - Failed - No archetype set");
		return false;
	}

	// return if the model dependencies aren't all loaded - we don't want to be playing a particle effect that isn't loaded
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	if (pBaseModel->GetHasLoaded()==false)
	{
		weaponDebugf1("CEntity::TryToExplode - Failed - Base model info not loaded");
		return false;
	}

	// return if this model doesn't have any destroy vfx
	if (pBaseModel->GetHasFxEntityDestroy()==false)
	{
		weaponDebugf1("CEntity::TryToExplode - Failed - No destroy FX");
		return false;
	}

	GET_2D_EFFECTS_NOLIGHTS(pBaseModel);
	for (s32 i=0; i<iNumEffects; i++)
	{
		C2dEffect* pEffect = (*pa2dEffects)[i];

		if (pEffect->GetType()==ET_EXPLOSION)
		{
			CExplosionAttr* pExpAttr = pEffect->GetExplosion();

			if (pExpAttr->m_explosionType==XT_DESTROY_FX)
			{
				// make sure that the bone with the explosion attached to it is still attached
				bool doExplosion = true;
				if (pExpAttr->m_boneTag>=0)
				{
					// get the matrix that the 2d effect is offset from 
					Matrix34 boneMtx;
					CVfxHelper::GetMatrixFromBoneTag(RC_MAT34V(boneMtx), this, (eAnimBoneTag)pExpAttr->m_boneTag);

					// return if we don't have a valid matrix
					if (boneMtx.a.IsZero())
					{
						doExplosion = false;
					}
				}

				if (doExplosion)
				{
					if (pExpAttr->m_tagHashName.GetHash()!=0)
					{
						eExplosionTag expTag = eExplosionTag_Parse(pExpAttr->m_tagHashName);
						vfxAssertf(expTag!=EXP_TAG_DONTCARE, "invalid explosion tag on entity [%s]", GetDebugName());

						if (!m_nFlags.bHasExploded || CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
						{
							Vector3 effectPos;
							pEffect->GetPos(effectPos);

							// get the matrix that the 2d effect is offset from 
							Matrix34 boneMtx;
							s32 expBoneTag = pExpAttr->m_boneTag>-1 ? pExpAttr->m_boneTag : 0;
							CVfxHelper::GetMatrixFromBoneTag(RC_MAT34V(boneMtx), this, (eAnimBoneTag)expBoneTag);

							// return if we don't have a valid matrix
							if (boneMtx.a.IsZero())
							{
								continue;
							}

							// get the 2d effect offset matrix (from parent bone)
							Matrix34 expMat;
							expMat.FromQuaternion(Quaternion(pExpAttr->qX, pExpAttr->qY, pExpAttr->qZ, pExpAttr->qW));
							expMat.d = effectPos;

							// transform this into world space
							expMat.Dot(boneMtx);

							CDummyObject *relatedDummyForNetwork = 0;

							if (!CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).minorExplosion)
							{
								SetHasExploded(&bNetworkExplosion);
							}

							if(GetIsTypeObject())
							{
								CObject *pObject = static_cast<CObject *>(this);
								relatedDummyForNetwork = pObject->GetRelatedDummy();

								if(relatedDummyForNetwork == 0 && pObject->GetFragParent() && pObject->GetFragParent()->GetIsTypeObject())
								{
									relatedDummyForNetwork = static_cast<CObject *>(pObject->GetFragParent())->GetRelatedDummy();
								}
							}

							CExplosionManager::CExplosionArgs explosionArgs(expTag, expMat.d);
							explosionArgs.m_vDirection = expMat.c;
							explosionArgs.m_pAttachEntity = nullptr;
							explosionArgs.m_attachBoneTag = expBoneTag;
							explosionArgs.m_pRelatedDummyForNetwork = relatedDummyForNetwork;
							explosionArgs.m_pExplodingEntity = this;
							explosionArgs.m_pEntExplosionOwner = pCulprit;
							explosionArgs.m_bIsLocalOnly = !bNetworkExplosion;
							bExplosionSuccessful = CExplosionManager::AddExplosion(explosionArgs);

							// Switch to the damaged model
							if(fragInst* objectFragInst = GetFragInst())
							{
								if(objectFragInst->IsInLevel())
								{
									objectFragInst->DamageAllGroups();
								}
							}
#if __BANK
							if(!bExplosionSuccessful)
								weaponDebugf1("CExplosionManager::AddExplosion - Failed - CExplosionManager::AddExplosion failed");
#endif // __BANK
						}

						return bExplosionSuccessful;
					}
				}
			}
		}
	}

	if (m_nFlags.bHasExploded)
	{
		weaponDebugf1("CEntity::TryToExplode - Failed - Already exploded");
	}
	else
	{
		weaponDebugf1("CEntity::TryToExplode - Failed - No explosions triggered");
	}

	return bExplosionSuccessful;
}

void CEntity::ProcessCollisionVfx(VfxCollisionInfo_s& vfxColnInfo)
{
	// don't process potentially smashable glass
	if (PGTAMATERIALMGR->GetIsSmashableGlass(vfxColnInfo.mtlIdA) && IsEntitySmashable(this))
	{
		return;
	}

	// get the physics instance
	phInst* pInst = NULL;
	if (GetFragInst())
	{
		pInst = GetFragInst();
	}
	else
	{
		pInst = GetCurrentPhysicsInst();
	}

	// return early if the data is no longer valid
	if (pInst==NULL || pInst->IsInLevel()==false)
	{
		return;
	}

	if (g_vfxMaterial.ProcessCollisionVfx(vfxColnInfo))
	{
		// in range - check for collision vfx 2d effects
		ProcessFxEntityCollision(RCC_VECTOR3(vfxColnInfo.vWorldPosA), RCC_VECTOR3(vfxColnInfo.vWorldNormalA), vfxColnInfo.componentIdA, vfxColnInfo.vAccumImpulse.Getf());
	}
}





//
// name:		IsWithinArea
// description:	Return if placeable is within a 2d bounding box
bool CEntity::IsWithinArea(float x1, float y1, float x2, float y2) const
{
	float temp_float;
	Vector3 posn = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	if (x1 > x2)
	{	// x1 should be less than x2
		temp_float = x1;
		x1 = x2;
		x2 = temp_float;
	}

	if (y1 > y2)
	{	// y1 should be less than y2
		temp_float = y1;
		y1 = y2;
		y2 = temp_float;
	}

	if ((posn.x >= x1) && (posn.x <= x2)
		&& (posn.y >= y1) && (posn.y <= y2))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


//
// name:		IsWithinArea
// description:	Return if placeable is within a 3d bounding box
bool CEntity::IsWithinArea(float x1, float y1, float z1, float x2, float y2, float z2) const
{
	float temp_float;
	Vector3 posn = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	if (x1 > x2)
	{	// x1 should be less than x2
		temp_float = x1;
		x1 = x2;
		x2 = temp_float;
	}

	if (y1 > y2)
	{	// y1 should be less than y2
		temp_float = y1;
		y1 = y2;
		y2 = temp_float;
	}

	if (z1 > z2)
	{	// z1 should be less than z2
		temp_float = z1;
		z1 = z2;
		z2 = temp_float;
	}

	if ((posn.x >= x1) && (posn.x <= x2)
		&& (posn.y >= y1) && (posn.y <= y2)
		&& (posn.z >= z1) && (posn.z <= z2))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

audEnvironmentGroupInterface *CEntity::GetAudioEnvironmentGroup(bool create) const
{
	naAudioEntity* ent = GetAudioEntity();
	audEnvironmentGroupInterface * ret = NULL;
	if(ent)
	{
		ret = ent->GetEnvironmentGroup(create);
	}
	return ret;

}

naAudioEntity * CEntity::GetAudioEntity() const
{
	if (GetIsTypePed())
	{
		return ((CPed*)this)->GetPedAudioEntity();
	}
	else if (GetIsTypeVehicle())
	{
		return ((CVehicle*)this)->GetVehicleAudioEntity();
	}
	else if(GetIsTypeObject())
	{
		if(static_cast<const CObject*>(this)->IsADoor())
		{
			return (((CDoor*)(this))->GetDoorAudioEntity());
		}
	}

	return NULL;
}

void CEntity::SetAudioInteriorLocation(const fwInteriorLocation intLoc)
{
	naAudioEntity* audEnt = NULL;
	if (GetIsTypePed())
	{
		audEnt = ((CPed*)this)->GetPedAudioEntity();
	}
	else if (GetIsTypeVehicle())
	{
		audEnt = ((CVehicle*)this)->GetVehicleAudioEntity();
	}
	else if(GetIsTypeObject())
	{
		audEnt = ((CObject*)this)->GetObjectAudioEntity();
	}

	if(audEnt)
	{
		audEnt->SetCachedInteriorLocation(intLoc);
	}
}

fwInteriorLocation CEntity::GetAudioInteriorLocation() const
{
	fwInteriorLocation intLoc;	// Constructor inits to invalid
	naAudioEntity* audEnt = NULL;
	if (GetIsTypePed())
	{
		audEnt = ((CPed*)this)->GetPedAudioEntity();
	}
	else if (GetIsTypeVehicle())
	{
		audEnt = ((CVehicle*)this)->GetVehicleAudioEntity();
	}
	else if(GetIsTypeObject())
	{
		audEnt = ((CObject*)this)->GetObjectAudioEntity();
	}

	if(audEnt)
	{
		intLoc = audEnt->GetCachedInteriorLocation();
	}

	return intLoc;
}

void CEntity::InitPoolLookupArray()
{
	sysMemSet(&ms_EntityPools[0], 0, sizeof(ms_EntityPools));

	ms_EntityPools[ENTITY_TYPE_BUILDING] = 				CBuilding						::GetPool();
	ms_EntityPools[ENTITY_TYPE_ANIMATED_BUILDING] =		CAnimatedBuilding				::GetPool();
	ms_EntityPools[ENTITY_TYPE_PED] =					CPed							::GetPool();
	ms_EntityPools[ENTITY_TYPE_DUMMY_OBJECT] =			CDummyObject					::GetPool();
	ms_EntityPools[ENTITY_TYPE_PORTAL] =				CPortalInst						::GetPool();
	ms_EntityPools[ENTITY_TYPE_MLO] =					CInteriorInst					::GetPool();
	ms_EntityPools[ENTITY_TYPE_NOTINPOOLS] =			NULL;
	ms_EntityPools[ENTITY_TYPE_PARTICLESYSTEM] =		CPtFxSortedEntity				::GetPool();
	ms_EntityPools[ENTITY_TYPE_LIGHT] =					NULL;
	ms_EntityPools[ENTITY_TYPE_COMPOSITE] =				CCompEntity						::GetPool();
	ms_EntityPools[ENTITY_TYPE_INSTANCE_LIST] =			CEntityBatch					::GetPool();
	ms_EntityPools[ENTITY_TYPE_GRASS_INSTANCE_LIST] =	CGrassBatch						::GetPool();
	ms_EntityPools[ENTITY_TYPE_VEHICLEGLASSCOMPONENT] =	CVehicleGlassComponentEntity	::GetPool();

	// CVehicles and Objects do NOT use fwPool anymore!
	//
}

void CEntity::GetEntityModifiedTargetPosition(const CEntity& entity, const Vector3& vTargeterPosition, Vector3& vEntityTargeteePosition)
{
	const float fTargetableDist = entity.GetLockOnTargetableDistance();
	// Don't adjust target position if the entity hasn't been set for a shorter los check
	if (fTargetableDist > 0.0f)
	{
		Vector3 vToEntity = vEntityTargeteePosition - vTargeterPosition;
		float fNewMag = vToEntity.Mag() - fTargetableDist;
		// Don't adjust target position if the targeter is within the targetable adjustment range
		if (fNewMag > 0.0f)
		{
			vToEntity.Normalize();
			vToEntity.Scale(fNewMag);
			vEntityTargeteePosition = vTargeterPosition + vToEntity;
		}
		else // Just force the targettee position to be the same as the targeter pos (i.e. say we have a clear los anyway)
		{	
			vEntityTargeteePosition = vTargeterPosition;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessLightsForEntity
///////////////////////////////////////////////////////////////////////////////

#if __DEV
void	CEntity::DrawOnVectorMap(Color32 col)
{
	float x1, x2, y1, y2;

	float radius=GetBoundRadius();
	Vector3 pos=VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	x1 = x2 = pos.x;
	y1 = y2 = pos.y;
	x1 -= radius;
	y1 -= radius;
	x2 += radius;
	y2 += radius;

	CVectorMap::DrawLine(Vector3(x1,y1,0.0f),Vector3(x2,y1,0.0f),col,col);
	CVectorMap::DrawLine(Vector3(x2,y1,0.0f),Vector3(x2,y2,0.0f),col,col);
	CVectorMap::DrawLine(Vector3(x1,y2,0.0f),Vector3(x2,y2,0.0f),col,col);
	CVectorMap::DrawLine(Vector3(x1,y1,0.0f),Vector3(x1,y2,0.0f),col,col);
}
#endif

void CEntity::SetIsVisibleForAllModules()
{
	ASSERT_ONLY( CIsVisibleExtension*	ext = GetExtension<CIsVisibleExtension>(); )
	const bool							wasVisible = IsBaseFlagSet( fwEntity::IS_VISIBLE );

	Assertf( wasVisible == (ext ? ext->AreAllIsVisibleFlagsSet() : true),
		"CIsVisibleExtension state is not consistent with the visibility state of this entity" );

	DestroyExtension<CIsVisibleExtension>();
	if ( !wasVisible )
	{
		AssignBaseFlag( fwEntity::IS_VISIBLE, true );
		UpdateWorldFromEntity();
	}
}

void CEntity::SetIsVisibleForModule(const eIsVisibleModule module, bool bIsVisibleForModule, bool bProcessChildAttachments) 
{
	CIsVisibleExtension*	ext = GetExtension<CIsVisibleExtension>();
	const bool				wasVisible = IsBaseFlagSet( fwEntity::IS_VISIBLE );
	bool					isNowVisible;
	
	//--- Check extension consistency ---//

	Assertf( wasVisible == (ext ? ext->AreAllIsVisibleFlagsSet() : true),
		"CIsVisibleExtension state is not consistent with the visibility state of this entity" );

	//--- Compute whether the entity is now visible, based on the extension ---//
#if __BANK
	if (PARAM_enableVisibilityFlagTracking.Get() && GetIsPhysical())
	{
		netObject *pNetObj = static_cast<CPhysical*>(this)->GetNetworkObject();
		if (pNetObj && pNetObj->GetObjectType() == NET_OBJ_TYPE_PLAYER && !bIsVisibleForModule)
		{
			CNetObjPlayer *pNetPlayerObj = static_cast<CNetObjPlayer*>(pNetObj);
			int firstFreeIndex = -1;
			int foundIndex = -1;
			for (int i = 0; i < VISFLAGCALLSTACK_COUNT; ++i)
			{
				if (gVisFlagCallstacks[i].m_objectId == pNetPlayerObj->GetObjectID())
				{
					foundIndex = i;
					break;
				}

				if (firstFreeIndex == -1 && !gVisFlagCallstacks[i].m_valid)
				{
					firstFreeIndex = i;
				}
			}

			int index = foundIndex != -1 ? foundIndex : firstFreeIndex;
			if (index != -1)
			{
				gVisFlagCallstacks[index].m_valid = true;
				gVisFlagCallstacks[index].m_objectId = pNetPlayerObj->GetObjectID();
				gVisFlagCallstacks[index].m_pEntity = this;
				gVisFlagCallstacks[index].m_module = module;
				rage::sysStack::CaptureStackTrace(gVisFlagCallstacks[index].m_trace, NetworkVisFlagCallStack::TRACE_DEPTH);
			}
		}
	}

#endif
#if __ASSERT
	if (!bIsVisibleForModule && GetIsPhysical() && !IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
	{
		netObject* pNetObj = static_cast<CPhysical*>(this)->GetNetworkObject();

		// in a MP game only certain modules are allowed to set invisibility (this prevents entities getting stuck invisible after migration)
		if (pNetObj && pNetObj->GetObjectType() != NET_OBJ_TYPE_PLAYER && 
			module != SETISVISIBLE_MODULE_SCRIPT && 
			module != SETISVISIBLE_MODULE_CAMERA && 
			module != SETISVISIBLE_MODULE_PICKUP && 
			module != SETISVISIBLE_MODULE_NETWORK && 
			module != SETISVISIBLE_MODULE_VEHICLE_RESPOT && 
			module != SETISVISIBLE_MODULE_RESPAWN && 
			module != SETISVISIBLE_MODULE_WORLD &&
			module != SETISVISIBLE_MODULE_CUTSCENE)
		{
			// prevent this assert firing for entities attached to the local player
			CEntity* pAttachParent = static_cast<CEntity*>(GetAttachParent());
			netObject* pAttachParentObj = (pAttachParent && pAttachParent->GetIsPhysical()) ? static_cast<CPhysical*>(pAttachParent)->GetNetworkObject() : NULL;

			if (!(pAttachParentObj && pAttachParentObj->GetObjectType() == NET_OBJ_TYPE_PLAYER && !pAttachParentObj->IsClone()))
			{
				Assertf(0, "SetIsVisibleForModule - trying to set network object %s invisible illegally via module %d", pNetObj->GetLogName(), module);
			}
			return;
		}
	}
	if (module == SETISVISIBLE_MODULE_PICKUP)
	{
		Assertf(GetType() == ENTITY_TYPE_OBJECT, "The pickup module should only be setting Entities of type 'Object' to be visible/invisible");
	}
#endif // __ASSERT		

	if ( ext )
	{
		ext->SetIsVisibleFlag( module, bIsVisibleForModule );
		isNowVisible = ext->AreAllIsVisibleFlagsSet();
		if ( isNowVisible )
		{
			DestroyExtension<CIsVisibleExtension>();
			ext = NULL;
		}
	}
	else
	{
		if ( bIsVisibleForModule )
			isNowVisible = true;
		else
		{
			ext = rage_new CIsVisibleExtension;
			GetExtensionList().Add( *ext );
			ext->SetIsVisibleFlag( module, bIsVisibleForModule );
			isNowVisible = ext->AreAllIsVisibleFlagsSet();
		}
	}

#if __BANK
	//--- If someone's trying to change the local player's visibility, log it (only on file) ---//
	bool	printCallstack = (wasVisible != isNowVisible) && (Channel_scene_visibility.FileLevel >= DIAG_SEVERITY_DEBUG1) && (this == CGameWorld::FindLocalPlayer() || this == g_PickerManager.GetSelectedEntity());

	if(PARAM_enableVisibilityDetailedSpew.Get())
	{
		const char*		debugName = "none";
		if ( this->GetIsPhysical() )
			debugName = static_cast< CPhysical* >(this)->GetDebugName();

		visibilityDebugf1( "SetIsVisibleForModule called: entity %p (model %s, debugname %s), module %d, state %s", this, this->GetModelName(), debugName, module, bIsVisibleForModule ? "true" : "false" );
		printCallstack = ( (wasVisible != isNowVisible) || PARAM_enableVisibilityObsessiveSpew.Get() ) && (Channel_scene_visibility.FileLevel >= DIAG_SEVERITY_DEBUG1);
	}

	if ( printCallstack )
	{
		visibilityDebugf1( "Visibility state changed on entity %p (%s)(%s)", this, this->GetModelName(), NetworkUtils::GetNetworkObjectFromEntity(this) ? NetworkUtils::GetNetworkObjectFromEntity(this)->GetLogName() : "");
		visibilityDebugf1( "Requested state %s, module %d", bIsVisibleForModule ? "true" : "false", module );

		if ( ext )
			visibilityDebugf1( "CIsVisible extension is now present, flags: 0x%x", ext->GetAllIsVisibleFlags() );
		else
			visibilityDebugf1( "CIsVisible extension is now NOT present" );
		
		visibilityDebugf1( "Changing fwEntity::IS_VISIBLE state from %s to %s", wasVisible ? "true" : "false", isNowVisible ? "true" : "false" );
		visibilityDebugf1( "Callstack:" );
		visibilityDebugf1StackTrace();
		visibilityDebugf1( "End callstack" );
	}

#if !__NO_OUTPUT
	if(this->GetIsPhysical())
	{
		CPhysical* pPhys = static_cast<CPhysical*>(this); 

		if(pPhys->m_LogVisibiltyCalls)
		{
			const char*		debugName = "none";
			debugName = pPhys->GetDebugName();
			Displayf( "SetIsVisibleForModule called: entity %p (model %s, debugname %s), module %d, state %s, frame %d", this, this->GetModelName(), debugName, module, bIsVisibleForModule ? "true" : "false", fwTimer::GetFrameCount() );
			EntityDebugfWithCallStack(pPhys, "SetIsVisibleForModule"); 
		}
	}
#endif

#endif

	//--- Set the base flag that has the actual effect on rendering ---//
	
	if ( wasVisible != isNowVisible )
	{
		AssignBaseFlag( fwEntity::IS_VISIBLE, isNowVisible );
		UpdateWorldFromEntity(); 

		if (GetBaseModelInfo()->GetUsesDoorPhysics() && !IsRetainedFlagSet())
		{	
			fwBaseEntityContainer *pContainer = GetOwnerEntityContainer();
			if (pContainer)
			{
				fwSceneGraphNode *pNode  = pContainer->GetOwnerSceneNode();
				if (pNode->GetType() == SCENE_GRAPH_NODE_TYPE_PORTAL)
				{
					fwEntityContainer *pEntityContainer = static_cast<fwEntityContainer*>(pContainer);
					if (bIsVisibleForModule && pContainer->GetOwnerSceneNode()->IsEnabled())	
					{
						bool allExceptThisVisibleAndDoors = true;
						for (int i = 0; i < pEntityContainer->GetEntityCount(); ++i)
						{
							CEntity *pEntity = static_cast<CEntity*>(pEntityContainer->GetEntity(i));
							if (pEntity != this && (!pEntity->GetIsVisible() || !pEntity->GetBaseModelInfo()->GetUsesDoorPhysics()))
							{
								allExceptThisVisibleAndDoors = false;
								break;
							}
						}
						if (allExceptThisVisibleAndDoors)
						{
							pNode->Enable(false);
						}
					}
					else if (!bIsVisibleForModule && !pContainer->GetOwnerSceneNode()->IsEnabled())	
					{
						pNode->Enable(true);
					}
				}
			}
		}

	}

	//--- Process attachments ---//
	
	if (bProcessChildAttachments)
	{
		// Iterate through all attachments and make them visible/invisible too
		CEntity* pAttachChild = static_cast<CEntity*>(GetChildAttachment());
		while(pAttachChild)
		{
			pAttachChild->SetIsVisibleForModule(module, bIsVisibleForModule, bProcessChildAttachments);
			pAttachChild = static_cast<CEntity*>(pAttachChild->GetSiblingAttachment());
		}
	}
}

bool CEntity::GetIsVisibleForModule(const eIsVisibleModule module) const
{
	const CIsVisibleExtension*	ext = GetExtension<CIsVisibleExtension>();
	return ext ? ext->GetIsVisibleFlag( module ) : true;
}

bool CEntity::GetIsVisibleIgnoringModule(const eIsVisibleModule module) const
{
	const CIsVisibleExtension*	ext = GetExtension<CIsVisibleExtension>();
	return ext ? ext->AreAllIsVisibleFlagsSetIgnoringModule( module ) : true;
}

void CEntity::CopyIsVisibleFlagsFrom(CEntity* src, bool bProcessChildAttachments)
{
	CIsVisibleExtension*	srcExt = src->GetExtension<CIsVisibleExtension>();
	CIsVisibleExtension*	ext = GetExtension<CIsVisibleExtension>();
	const bool				wasVisible = GetIsVisible();
	bool					isNowVisible;

	Assertf( wasVisible == (ext ? ext->AreAllIsVisibleFlagsSet() : true),
		"CIsVisibleExtension state is not consistent with the visibility state of this entity" );

	if ( srcExt )
	{
		if ( !ext )
		{
			ext = rage_new CIsVisibleExtension;
			GetExtensionList().Add( *ext );
		}
		
		*ext = *srcExt;
		isNowVisible = ext->AreAllIsVisibleFlagsSet();
	}
	else
	{
		DestroyExtension<CIsVisibleExtension>();
		isNowVisible = true;
	}

	if ( wasVisible != isNowVisible )
	{
		AssignBaseFlag( fwEntity::IS_VISIBLE, isNowVisible );
		UpdateWorldFromEntity(); 
	}

	if (bProcessChildAttachments)
	{
		// Iterate through all attachments and make them visible/invisible too
		CEntity* pAttachChild = static_cast<CEntity*>(GetChildAttachment());
		while(pAttachChild)
		{
			pAttachChild->CopyIsVisibleFlagsFrom(src);
			pAttachChild = static_cast<CEntity*>(pAttachChild->GetSiblingAttachment());
		}
	}
}

const fwInteriorLocation CEntity::GetInteriorLocation() const
{
	Assert((sysThreadType::IsUpdateThread() || aiDeferredTasks::g_Running || CTimeCycleAsyncQueryMgr::IsRunning() REPLAY_ONLY(|| CReplayMgr::IsStoringEntities())) && !sysThreadType::IsRenderThread());

	if ( !GetIsRetainedByInteriorProxy() && GetOwnerEntityContainer() )
	{
		const fwBaseEntityContainer*	entityContainer = GetOwnerEntityContainer();
		const fwSceneGraphNode*			sceneNode = entityContainer->GetOwnerSceneNode();

		if ( sceneNode->IsTypeRoom() )
			return static_cast< const fwRoomSceneGraphNode* >( sceneNode )->GetInteriorLocation();
		else if ( sceneNode->IsTypePortal() )
			return static_cast< const fwPortalSceneGraphNode* >( sceneNode )->GetInteriorLocation();
		else
			Assertf( sceneNode->IsTypeExterior() || sceneNode->IsTypeStreamed(), "Scene node type is wrong while trying to get the interior location for the entity");
	}

	return fwInteriorLocation();
}

// adding/removing entity from audio mix groups: needs to be implemented at the entity level containing an audioEntity/occlusion group
// so this version should never be called
void CEntity::AddToMixGroup(s16 mixGroupIndex) 
{
	Assertf(GetAudioEnvironmentGroup(), "Entity is missing it's audio environment group");
	if(GetAudioEnvironmentGroup())
	{
		GetAudioEnvironmentGroup()->SetMixGroup(mixGroupIndex); 
	}
}
void CEntity::RemoveFromMixGroup() 
{ 
	Assertf(GetAudioEnvironmentGroup(), "Entity is missing it's audio environment group");
	if(GetAudioEnvironmentGroup())
	{
		DEV_ONLY(dmDebugf1("Removing %s reference from mixgroup %s in CEntity", GetDebugName(), g_AudioEngine.GetDynamicMixManager().GetMixGroupNameFromIndex(GetMixGroupIndex()));)
		GetAudioEnvironmentGroup()->UnsetMixGroup(); 
	}
}
s32 CEntity::GetMixGroupIndex() 
{
	Assertf(GetAudioEnvironmentGroup(), "Entity is missing it's audio environment group");
	if(GetAudioEnvironmentGroup())
	{			
		return GetAudioEnvironmentGroup()->GetMixGroupIndex();
	}
	return -1;
}

u32 CEntity::GetMainSceneUpdateFlag() const
{
	return CGameWorld::SU_UPDATE;
}

u32 CEntity::GetStartAnimSceneUpdateFlag() const
{
	return CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS;
}

#if !__SPU
fwMove *CEntity::CreateMoveObject()
{
	// TODO: We need a default handler in fwEntity.
	return rage_new CMoveObjectPooledObject((CDynamicEntity &) *this); 
}

const fwMvNetworkDefId &CEntity::GetAnimNetworkMoveInfo() const
{
	return CClipNetworkMoveInfo::ms_NetworkObject;
}

crFrameFilter *CEntity::GetLowLODFilter() const
{
	return g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(BONEMASK_LOD_LO));
}

#endif // !__SPU


const char* GetEntityTypeStr(int entityType)
{
	static const char* entityTypeStrings[] =
	{
		"NOTHING"				,
		"BUILDING"				,
		"ANIMATED_BUILDING"		,
		"VEHICLE"				,
		"PED"					,
		"OBJECT"				,
		"DUMMY_OBJECT"			,
		"PORTAL"				,
		"INTERIOR"				,
		"NOTINPOOLS"			,
		"PARTICLESYSTEM"		,
		"LIGHT_ENTITY"			,
		"COMPOSITE_ENTITY"		,
		"INSTANCE_LIST"			,
		"GRASS_INSTANCE_LIST"	,
		"VEHICLE_GLASS"			,
	};
	CompileTimeAssert(NELEM(entityTypeStrings) == ENTITY_TYPE_TOTAL);

	if (entityType >= 0 && entityType < ENTITY_TYPE_TOTAL)
	{
		return entityTypeStrings[entityType];
	}

	return "?";
}

void CEntity::SwapModel(u32 newModelIndex)
{
	if (Verifyf(GetIsTypeBuilding() || GetIsTypeObject() || GetIsTypeDummyObject(), "SwapModel called on unsupported entity type"))
	{
		// Remove old model
		DeleteDrawable();

		// remove the old physics model, new one will get added by CPhysics::ScanArea()
		if(GetCurrentPhysicsInst())
		{
			RemovePhysics();
			DeleteInst();
		}

		// Create new model
		//m_modelId.Invalidate();			//	hack to stop assert firing at top of SetModelId about "Can only call SetModelId once"
		m_pArchetype = NULL;				//	hack to stop assert firing at top of SetModelId about "Can only call SetModelId once"

		SetModelId(fwModelId(strLocalIndex(newModelIndex)));

		m_nFlags.bIsFrag = false;

		if (GetIsDynamic())
		{
			((CDynamicEntity*)this)->m_nDEflags.bIsBreakableGlass = false;
		}

		// Check for a physics dictionary on the swapped in model to make sure collision gets loaded
		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(newModelIndex)));

		if (pModelInfo->GetFragType())
			CreateDrawable();

		if ( !GetIsTypeDummyObject() &&
			(pModelInfo->GetHasBoundInDrawable() || pModelInfo->GetDrawableType()==fwArchetype::DT_FRAGMENT) )
		{
			InitPhys();
			AddPhysics();
		}

		GetLodData().SetResetDisabled(true);

		UpdateWorldFromEntity();
	}
}

void CEntity::SetHidden(bool bHidden)
{
	if (Verifyf(GetIsTypeBuilding() || GetIsTypeObject() || GetIsTypeDummyObject() || GetIsTypeAnimatedBuilding(), "HideModel called on unsupported entity type"))
	{
		GetLodData().SetResetDisabled(true);

		// visibility
		if ( GetIsTypeDummyObject() && !GetExtension<CIsVisibleExtension>() )
		{
			const bool				isVisible = GetIsVisible();
			CIsVisibleExtension*	ext = rage_new CIsVisibleExtension;
			GetExtensionList().Add( *ext );
			ext->SetIsVisibleFlag( SETISVISIBLE_MODULE_DUMMY_CONVERSION, isVisible );
		}

#if GTA_REPLAY
		if( CReplayMgr::IsRecording() && ReplayHideManager::IsNonReplayTrackedObjectType(this) )
		{
			ReplayHideManager::AddNewHide(this, !bHidden);
		}
#endif

		SetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD, !bHidden, true);

		// collision
		switch (GetType())
		{
		case ENTITY_TYPE_DUMMY_OBJECT:
			// dummy objects don't have physics
			break;
		case ENTITY_TYPE_BUILDING:
		case ENTITY_TYPE_ANIMATED_BUILDING:
			if (IsBaseFlagSet(HAS_PHYSICS_DICT))
			{
				if (bHidden)
				{
					SetBaseFlag(NO_INSTANCED_PHYS);
				}
				else
				{
					ClearBaseFlag(NO_INSTANCED_PHYS);
				}
			}
			// intentional fall-through
		case ENTITY_TYPE_OBJECT:
			{
				if (bHidden)
				{
					DisableCollision(GetCurrentPhysicsInst(), true);

					if(IsInPathServer())
					{
						CAssistedMovementRouteStore::MaybeRemoveRouteForDoor(this);
						CPathServerGta::MaybeRemoveDynamicObject(this);
					}
				}
				else
				{
					EnableCollision();

					if(!IsInPathServer())
					{
						if(CPathServerGta::MaybeAddDynamicObject(this))
							CAssistedMovementRouteStore::MaybeAddRouteForDoor(this);
					}
				}
			}
			break;
		default:
			break;
		}
	}
}

#if __BANK

bool CEntity::GetGuid(u32& guid)
{
	// get the corresponding map data slot index. objects should query their related dummy, if applicable
	strLocalIndex iplIndex = strLocalIndex(GetIplIndex());
	CEntity* pSearchEntity = this;

	if (GetIsTypeObject())
	{
		CObject* pObj = (CObject*) this;
		if (pObj->GetRelatedDummy())
		{
			pSearchEntity = pObj->GetRelatedDummy();

			if (iplIndex.Get() <= 0)
			{
				iplIndex = strLocalIndex(pObj->GetRelatedDummy()->GetIplIndex());
			}
		}
	}

	if (iplIndex.Get()>0)
	{
		fwMapDataContents* pMapContents = fwMapDataStore::GetStore().GetSafeFromIndex(iplIndex);
		if ( Verifyf(pMapContents, "Querying GUID for %s but it's IMAP is invalid", GetModelName()) )
		{
			return pMapContents->GetGuid(pSearchEntity, guid);
		}
	}
	return false;
}

#endif	//__BANK

#if __BANK
void CEntity::GetIsVisibleInfo(char* buf, const int maxLen) const
{
	const CIsVisibleExtension*	ext = this->GetExtension<CIsVisibleExtension>();
	if ( !ext )
	{
		Assert( this->IsBaseFlagSet( fwEntity::IS_VISIBLE ) == true );

		if (IsBaseFlagSet(IS_VISIBLE))
		{
			formatf( buf, maxLen, "true" );
		}
		else
		{
			formatf( buf, maxLen, "false" );
		}
	}
	else
	{
		Assert( this->IsBaseFlagSet( fwEntity::IS_VISIBLE ) == false );
		formatf( buf, maxLen, "false - module(s) " );

		bool first = true;
		for (int i = 0; i < NUM_VISIBLE_MODULES; ++i)
		{
			if ( !ext->GetIsVisibleFlag( static_cast<eIsVisibleModule>(i) ) )
			{
				if ( !first )
				{
					strcat( buf, "," );
				}
				strcat( buf, s_moduleCaptions[i] );
				first = false;
			}
		}
	}
}
#endif

#if !__NO_OUTPUT
void CEntity::ProcessFailmarkVisibility() const
{
	Displayf("   VISIBILITY:");

	const CIsVisibleExtension*	ext = this->GetExtension<CIsVisibleExtension>();
	if ( !ext )
	{
		Assert( this->IsBaseFlagSet( fwEntity::IS_VISIBLE ) == true );

		if (IsBaseFlagSet(IS_VISIBLE))
		{
			Displayf("     VISIBLE");
		}
		else
		{
			Displayf("     HIDDEN");
		}
	}
	else
	{
		for (int i = 0; i < NUM_VISIBLE_MODULES; ++i)
		{
			if ( !ext->GetIsVisibleFlag( static_cast<eIsVisibleModule>(i) ) )
			{
				Displayf("     !SETISVISIBLE_MODULE_%s",s_moduleCaptions[i]);
			}
		}
	}
}
#endif //!__NO_OUTPUT

#if !__NO_OUTPUT

void CEntity::CommonDebugStr(CEntity* pEnt, char * debugStr)
{
	if(pEnt)
	{
		sprintf(debugStr, "FC(%u) Entity(%p, %s), script: %s ", fwTimer::GetFrameCount(), pEnt, pEnt->GetModelName(), CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
}

#endif // !__NO_OUTPUT

void CEntity::SetHasExploded(bool* UNUSED_PARAM(bNetworkExplosion))
{
	// B*1891841 and 1927523: Only allow an entity so be set as exploded if it hasn't already been flagged.
	// This is to prevent issues where explosion events and exploded state synchronisation would occur in the wrong order and lead to either explosions being removed prematurely, or explosions not being removed correctly on clones.
	if(m_nFlags.bHasExploded)
		return;

	m_nFlags.bHasExploded = true;

	// update the frag parent exploded state also, this is required for the network game
	if(GetIsTypeObject())
	{
		CObject *pObject = static_cast<CObject *>(this);

		if(pObject->GetFragParent())
		{
			pObject->GetFragParent()->m_nFlags.bHasExploded = true;
		}
	}

	// Flag any existing explosions for removal, as we're going to be "destroyed". Ignore remotely triggered explosions as we need them to complete. They 
	// may have been triggered on this entity beforehand, due to the order of network packets.
	CExplosionManager::FlagsExplosionsForRemoval(this);

	// stop any particle effects currently playing on this entity
	g_ptFxManager.StopPtFxOnEntity(this);
}

void CEntity::SetAddedToDrawListThisFrame(u32 drawListType, bool wasAdded)
{
	if(drawListType == DL_RENDERPHASE_GBUF)
		m_bRenderedInGBufThisFrame = wasAdded;
}

bool CEntity::GetAddedToDrawListThisFrame(u32 UNUSED_PARAM(drawListType)) const
{
	return m_bRenderedInGBufThisFrame;	//For now, if we were added to the drawlist in the gbuffer renderphase, we'll assume we were in all the rest as well.
}


#if GTA_REPLAY
void	CEntity::SetCreationFrameRef(const FrameRef& frameRef)
{
	ReplayEntityExtension* pExt = ReplayEntityExtension::GetExtension(this);
	replayFatalAssertf(pExt, "No extension in entity!");
	pExt->m_CreationFrameRef = frameRef;
}

bool	CEntity::GetCreationFrameRef(FrameRef& frameRef) const
{ 
	const ReplayEntityExtension* pExt = ReplayEntityExtension::GetExtension(this);
	replayFatalAssertf(pExt, "No extension in entity!");
	frameRef = pExt->m_CreationFrameRef;
	return frameRef != FrameRef::INVALID_FRAME_REF;
}

bool	CEntity::GetUsedInReplay() const
{
	const ReplayEntityExtension* pExt = ReplayEntityExtension::GetExtension(this);
	if(pExt == NULL || pExt->m_CreationFrameRef == FrameRef::INVALID_FRAME_REF)
		return false;
	return true;
}
#endif

#if !__NO_OUTPUT
const char* CEntity::GetLogName() const
{
	if (!this)
	{
		return "NULL";
	}

	netObject *obj = NetworkUtils::GetNetworkObjectFromEntity(this);
	return obj ? obj->GetLogName() : GetModelName();
}
#endif // !__NO_OUTPUT
