// ========================================
// vfx/vehicleglass/VehicleGlassManager.cpp
// (c) 2012-2014 RockstarNorth
// ========================================

#include "vfx/vehicleglass/VehicleGlass.h"

// rage
#include "grcore/allocscope.h"
#include "physics/gtaInst.h"
#include "phbound/support.h"
#include "string/stringutil.h"
#include "system/xtl.h"

// framework
#include "fwdebug/picker.h"
#include "fwmaths/vectorutil.h"
#include "fwtl/customvertexpushbuffer.h"
#include "fwutil/xmacro.h"
#include "vfx/vfxwidget.h"
#include "vfx/vehicleglass/vehicleglassconstructor.h"
#include "vfx/vehicleglass/vehicleglasswindow.h"

// game
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/Replay.h"
#include "control/replay/Effects/ProjectedTexturePacket.h"
#include "Core/game.h"
#include "crskeleton/skeleton.h"
#include "debug/DebugGlobals.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "physics/gtaMaterialDebug.h"
#include "physics/physics.h"
#include "renderer/Lights/lights.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "scene/world/GameWorld.h"
#include "vehicles/vehicleDamage.h"
#include "Vehicles/VehicleFactory.h"
#include "vfx/Particles/PtFxCollision.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "vfx/vehicleglass/VehicleGlassComponent.h"
#include "vfx/vehicleglass/VehicleGlassSmashTest.h"
#include "vfx/vfx_channel.h"
#include "vfx/channel.h"
#include "vehicleAI/Task/TaskVehicleAnimation.h"
#include "TimeCycle/TimeCycle.h"

VEHICLE_GLASS_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

PARAM(vgasserts,""); // suppress certain asserts unless we specifically want them
PARAM(vgsmoketest_nodetach,"");

PF_PAGE(VehicleGlassManager, "Vehicle Glass Manager");
PF_GROUP(UpdatingCost);
PF_LINK(VehicleGlassManager, UpdatingCost);
PF_TIMER(UpdateSafe, UpdatingCost);
PF_TIMER(ProcessExplosion, UpdatingCost);
PF_TIMER(ProcessCollison, UpdatingCost);
PF_GROUP(RenderingCost);
PF_LINK(VehicleGlassManager, RenderingCost);
PF_TIMER(RenderComponents, RenderingCost);
PF_TIMER(LightsInArea, RenderingCost);

#define VEHICLEGLASS_SORTED_CAM_DIR_OFFSET (ScalarVConstant<FLOAT_TO_INT(0.8f)>())
///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
CVehicleGlassManager g_vehicleGlassMan;

dev_float VEHICLEGLASSPOLY_GRAVITY_SCALE               = 0.6f; // we're not in a vacuum
dev_float VEHICLEGLASSPOLY_ROTATION_SPEED              = 7.0f;

dev_float VEHICLEGLASSPOLY_VEL_MULT_COLN               = 10.0f;
dev_float VEHICLEGLASSPOLY_VEL_MULT_EXPL               = 1.0f;
dev_float VEHICLEGLASSPOLY_VEL_RAND_COLN               = 0.5f;
dev_float VEHICLEGLASSPOLY_VEL_RAND_EXPL               = 0.5f;
dev_float VEHICLEGLASSPOLY_VEL_DAMP                    = 0.5f; // Dampening value for the 

dev_float VEHICLEGLASSGROUP_RADIUS_MULT                = 0.25f;
dev_float VEHICLEGLASSGROUP_EXPOSED_SMASH_RADIUS       = 0.35f;
dev_float VEHICLEGLASSGROUP_SMASH_RADIUS               = 1000.0f;
dev_float VEHICLEGLASSFORCE_NOT_USED                   = -1.0f;
dev_float VEHICLEGLASSFORCE_WEAPON_IMPACT              = 0.25f;
dev_float VEHICLEGLASSFORCE_SIREN_SMASH                = 0.10f;
dev_float VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW          = 0.20f;
dev_float VEHICLEGLASSFORCE_WINDOW_DEFORM              = 0.10f;
dev_float VEHICLEGLASSFORCE_NO_TEXTURE_THRESH          = 0.25f;

dev_float VEHICLEGLASSTHRESH_PHYSICAL_COLN_MIN_IMPULSE = 1000.0f;
dev_float VEHICLEGLASSTHRESH_PHYSICAL_COLN_MAX_IMPULSE = 30000.0f;

dev_float VEHICLEGLASSTHRESH_VEH_SPEED_MIN             = 0.0f;
dev_float VEHICLEGLASSTHRESH_VEH_SPEED_MAX             = 1.0f;

dev_float VEHICLEGLASSTESSELLATIONSCALE_FORCE_SMASH    = 5.0f; // larger values tessellate into larger pieces
dev_float VEHICLEGLASSTESSELLATIONSCALE_EXPLOSION      = 8.0f;
#if __BANK

ScalarV g_VehicleGlassSortedCamDirOffset			   = VEHICLEGLASS_SORTED_CAM_DIR_OFFSET;
#endif

grcRasterizerStateHandle   CVehicleGlassManager::ms_rasterizerStateHandle[3] = { grcStateBlock::RS_Invalid, grcStateBlock::RS_Invalid, grcStateBlock::RS_Invalid };
grcDepthStencilStateHandle CVehicleGlassManager::ms_depthStencilStateHandle  = grcStateBlock::DSS_Invalid;
grcBlendStateHandle        CVehicleGlassManager::ms_blendStateHandle         = grcStateBlock::BS_Invalid;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVehicleGlassManager
///////////////////////////////////////////////////////////////////////////////
bool IsEntitySmashable(const CEntity* pEntity)
{
	if (pEntity->GetIsTypeVehicle())
	{
		return true;
	}

	if (pEntity->GetIsTypeObject() && static_cast<const CObject*>(pEntity)->m_nObjectFlags.bVehiclePart)
	{
		return true;
	}

	return false;
}

static phMaterialMgr::Id GetSmashableGlassMaterialId(const CPhysical* pPhysical, int componentId)
{
	phMaterialMgr::Id  materialId = phMaterialMgr::MATERIAL_NOT_FOUND;
	const fragInst*    pFragInst  = pPhysical->GetFragInst();
	const phArchetype* pArchetype = pFragInst ? pFragInst->GetArchetype() : NULL;

	if (AssertVerify(pArchetype))
	{
		const phBound* pBoundChild = CVehicleGlassManager::GetVehicleGlassWindowBound(pPhysical, componentId);

		if (pBoundChild)
		{
			if (pBoundChild->GetType() == phBound::GEOMETRY ||
				pBoundChild->GetType() == phBound::BVH) // e.g. sundeck uses BVH
			{
				const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(0));

				if (PGTAMATERIALMGR->GetIsSmashableGlass(mtlId)) // VFXGROUP_CAR_GLASS
				{
					materialId = mtlId;
#if __BANK
					if (g_vehicleGlassMan.GetIsAllGlassWeak())
					{
						materialId = PGTAMATERIALMGR->g_idCarGlassWeak;
					}
#endif // __BANK
				}
#if 0
				else if (mtlId == PGTAMATERIALMGR->g_idCarVoid) // blown up window?
				{
					const fragInst* pFragInst = pPhysical->GetFragInst();

					if (AssertVerify(pFragInst))
					{
						const fragType* pFragType = pFragInst->GetType();
						if (pFragType)
						{
							const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pFragType)->m_vehicleWindowData;

							if (pWindowData && fwVehicleGlassWindowData::GetWindow(pWindowData, componentId))
							{
								materialId = PGTAMATERIALMGR->g_idCarGlassWeak;
							}
						}
					}
				}
#endif
			}
		}
	}

	return materialId;
}

void CVehicleGlassManager::Reset(bool bClearFlags)
{
	for (int i = 0; i < m_numCollisionInfos; i++) { m_collisionInfos[i].regdEnt = NULL; }
	for (int i = 0; i < m_numExplosionInfos; i++) { m_explosionInfos[i].regdEnt = NULL; }

	m_numCollisionInfos = 0;
	m_numExplosionInfos = 0;

	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; )
	{
		CVehicleGlassComponent* pNextComponent = m_componentList.GetNext(pComponent);

		if (bClearFlags)
		{
			CPhysical* pPhysical = pComponent->m_regdPhy.Get();

			if (pPhysical)
			{
				pPhysical->ResetBrokenAndHiddenFlags();
				NetworkInterface::OnEntitySmashed(pPhysical, pComponent->m_componentId, false);
			}
		}

		pComponent->ShutdownComponent();
		pComponent = pNextComponent;
	}
	CleanupComponentEntities();
}

#if __PS3 && __DEV
PARAM(vgxv, "extra vertices for vehicle glass system");
PARAM(vgxi, "extra indices for vehicle glass system");
#endif // __PS3 && __DEV

class grcVehicleGlassInterface
{
public:
	typedef int  (*GetComponentTriangleCountFuncType)(const void* p);
	typedef void (*GetComponentTrianglePointFuncType)(const void* p, Vec3V* out_points);

	static void SetCallbacks(GetComponentTriangleCountFuncType GetComponentTriangleCount, GetComponentTrianglePointFuncType GetComponentTrianglePoint)
	{
		sm_GetComponentTriangleCountFunc = GetComponentTriangleCount;
		sm_GetComponentTrianglePointFunc = GetComponentTrianglePoint;
	}

#if __BANK
	void DebugDrawComponent(const void* p)
	{
		if (sm_GetComponentTriangleCountFunc &&
			sm_GetComponentTrianglePointFunc)
		{
			const int numTriangles = sm_GetComponentTriangleCountFunc(p);

			if (numTriangles > 0)
			{
				Vec3V* points = (Vec3V*)alloca(numTriangles*3*sizeof(Vec3V));

				sm_GetComponentTrianglePointFunc(p, points);

				for (int i = 0; i < numTriangles; i++)
				{
					const Vec3V p0 = *(points++);
					const Vec3V p1 = *(points++);
					const Vec3V p2 = *(points++);

					grcDebugDraw::Poly(p0, p1, p2, Color32(255,0,0,255), true, false);
				}
			}
		}
	}
#endif // __BANK

private:
	static GetComponentTriangleCountFuncType sm_GetComponentTriangleCountFunc;
	static GetComponentTrianglePointFuncType sm_GetComponentTrianglePointFunc;
};

grcVehicleGlassInterface::GetComponentTriangleCountFuncType grcVehicleGlassInterface::sm_GetComponentTriangleCountFunc = NULL;
grcVehicleGlassInterface::GetComponentTrianglePointFuncType grcVehicleGlassInterface::sm_GetComponentTrianglePointFunc = NULL;

void CVehicleGlassManager::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
#if VEHICLE_GLASS_SMASH_TEST
		g_vehicleGlassSmashTest.Init();
#endif // VEHICLE_GLASS_SMASH_TEST

#if __BANK
		m_RenderVehicleGlassAlphaBucket			   = true;
		m_RenderDebugCentroid					   = false;
		m_numTriangles                             = 0;
		m_numComponents                            = 0;
		m_totalVBMemory                            = 0;
		m_maxVBMemory                              = 0;
		m_testMPMemCap                             = false;
		m_smashWindowHierarchyId                   = VEH_WINDSCREEN;
		m_smashWindowForce                         = 1.0f;
		m_smashWindowDetachAll                     = false;
		m_smashWindowShowBoneMatrix                = false;
		m_smashWindowUpdateTessellation            = false;
		m_smashWindowUpdateTessellationAll         = false;
		m_smashWindowUpdateRandomSeed              = 1;
		m_smashWindowCurrentSmashSphere            = Vec4V(V_ZERO);
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		m_verbose                                  = false;
		m_verboseHit                               = false;
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		m_showInfo                                 = false;
		m_maxActiveComponents                      = 0;
		m_maxAttachedTriangles                     = 0;
		m_maxDetachedTriangles                     = 0;
		m_maxAttachedTrianglesPerComponent         = 0;
		m_maxDetachedTrianglesPerComponent         = 0;
#if !VEHICLE_GLASS_COMPRESSED_VERTICES
		m_testNormalQuantisationBits               = 32;
#endif // !VEHICLE_GLASS_COMPRESSED_VERTICES
		m_useHDDrawable                            = false;
#if USE_EDGE
		m_bInvalideateGeomCache                    = false;				
		m_bReuseExtractedGeom                      = true;
		m_bExtractGeomEarly                        = true;					
#endif // USE_EDGE
		m_forceVisible                             = false;
		m_useStaticVB                              = true;
		m_sort                                     = true;
		m_sortReverse                              = false;
		m_disableVehicleGlass                      = false;
		m_disableRender                            = false;
		m_disableFrontFaceRender                   = false;
		m_disableBackFaceRender                    = false;
		m_disablePosUpdate                         = false;
		m_disableSpinAxis                          = false;
		m_disableCanDetachTest                     = false;
		m_disableDetaching                         = false;
		m_ignoreRemoveAllGlass                     = false;
		m_detachAllOnExplosion                     = !PARAM_vgsmoketest_nodetach.Get(); // TODO -- investigate this, is this really necessary?
#if VEHICLE_GLASS_RANDOM_DETACH
		m_randomDetachEnabled                      = false;
		m_randomDetachThreshold                    = 0.1f;
#endif // VEHICLE_GLASS_RANDOM_DETACH
		m_tessellationEnabled                      = true;
		m_tessellationSphereTest                   = true;
		m_tessellationNormalise                    = false;
		m_tessellationScaleMin                     = 1.0f;
		m_tessellationScaleMax                     = 1.0f;
		m_tessellationSpreadMin                    = 1.0f;
		m_tessellationSpreadMax                    = 1.0f;
		m_vehicleGlassBreakRange                   = VEHICLE_GLASS_BREAK_RANGE;
		m_vehicleGlassDetachRange                  = VEHICLE_GLASS_DETACH_RANGE;
#if VEHICLE_GLASS_DEV
		m_distanceFieldSource                      = VGW_SOURCE_VEHICLE_MODEL_WINDOW_DATA;
		m_distanceFieldUncompressed                = false;
		m_distanceFieldOutputFiles                 = false;
#endif // VEHICLE_GLASS_DEV
		m_distanceFieldNumVerts                    = VEHICLE_GLASS_DEFAULT_DISTANCE_FIELD_NUM_VERTS_TO_DETACH;
		m_distanceFieldThresholdMin                = VEHICLE_GLASS_DEFAULT_DISTANCE_THRESHOLD_TO_DETACH_MIN;
#if VEHICLE_GLASS_CLUMP_NOISE
		m_distanceFieldThresholdMax                = VEHICLE_GLASS_DEFAULT_DISTANCE_THRESHOLD_TO_DETACH_MAX;
		m_distanceFieldThresholdPhase              = 0.0f;
		m_distanceFieldThresholdPeriod             = 6.0f;
		m_clumpNoiseEnabled                        = true;
		m_clumpNoiseSize                           = 80;
		m_clumpNoiseBuckets                        = 8;
		m_clumpNoiseBucketIsolate                  = -1;
		m_clumpNoiseGlobalOffset                   = 0;
		m_clumpNoiseDebugDrawGraphs                = false;
		m_clumpNoiseDebugDrawAngle                 = false;
		m_clumpSpread                              = 1.0f; // 0=no spread, 1=full spread within bucket
		m_clumpExponentBias                        = 1.0f;
		m_clumpRadiusMin                           = 0.2f;
		m_clumpRadiusMax                           = 0.5f;
		m_clumpHeightMin                           = 0.1f;
		m_clumpHeightMax                           = 1.2f;
#endif // VEHICLE_GLASS_CLUMP_NOISE
		m_sphereTestDetachNumVerts                 = CVehicleGlassComponent::VEHICLE_GLASS_TEST_DEFAULT;
		m_skipSphereTestOnDetach                   = VEHICLE_GLASS_SKIP_SPHERE_ON_DETACH_DEFAULT;
		m_skipDistanceFieldTestOnExtract           = true;
		m_crackTextureAmount                       = VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_AMOUNT;
		m_crackTextureScale                        = VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_SCALE;
		m_crackTextureScaleFP                      = VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_SCALE_FP;
		m_crackTextureBumpAmount                   = VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMP_AMOUNT;
		m_crackTextureBumpiness                    = VEHICLE_GLASS_DEFAULT_CRACK_TEXTURE_BUMPINESS;
		m_crackTint                                = VEHICLE_GLASS_DEFAULT_CRACK_TINT;
		m_fallingTintScale                         = VEHICLE_GLASS_DEFAULT_FALLING_TINT_SCALE;
		m_allowFindGeometryWithoutWindow           = false;
		m_impactEffectParticle                     = true;
		m_groundEffectAudio                        = true;
		m_groundEffectParticle                     = true;
		m_allGlassIsWeak                           = false;
		m_onlyApplyDecals                          = false;
		m_neverApplyDecals                         = false;
		m_deferHits                                = true;
#if !USE_GPU_VEHICLEDEFORMATION
		m_applyVehicleDamageOnUpdate               = true;
		m_forceVehicleDamageUpdate                 = false;
#endif // !USE_GPU_VEHICLEDEFORMATION
		m_smashWindowsWhenConvertibleRoofAnimPlays = true;
		m_renderDebug                              = false;
		m_renderDebugErrorsOnly                    = false;
		m_renderDebugTriangleNormals               = false;
		m_renderDebugVertNormals                   = false;
		m_renderDebugExposedVert                   = false;
		m_renderDebugSmashSphere                   = false;
		m_renderDebugExplosionSmashSphereHistory   = false;
		m_renderDebugOffsetTriangle                = false; // enable this to reduce zfighting
		m_renderDebugOffsetSpread                  = 0.0f;
		m_renderDebugFieldOpacity                  = 0.0f;
		m_renderDebugFieldOffset                   = 0.1f;
		m_renderDebugFieldColours                  = true;
		m_renderDebugTriCount                      = false;
		m_renderGroundPlane                        = false;
		m_timeScale                                = 1.0f;
#endif // __BANK

		// init render state blocks
		grcRasterizerStateDesc rasterizerStateDesc;
#if __BANK
		// We only use those states as reference in BANK builds
		ms_rasterizerStateHandle[1] = grcStateBlock::CreateRasterizerState(rasterizerStateDesc);
		rasterizerStateDesc.CullMode = grcRSV::CULL_FRONT;
		ms_rasterizerStateHandle[2] = grcStateBlock::CreateRasterizerState(rasterizerStateDesc);
#endif // __BANK
		rasterizerStateDesc.CullMode = grcRSV::CULL_NONE;
		ms_rasterizerStateHandle[0] = grcStateBlock::CreateRasterizerState(rasterizerStateDesc);

		grcDepthStencilStateDesc depthStencilDesc;
		depthStencilDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		ms_depthStencilStateHandle = grcStateBlock::CreateDepthStencilState(depthStencilDesc);

		grcBlendStateDesc blendStateDesc;
		blendStateDesc.BlendRTDesc[0].BlendEnable    = true;
		blendStateDesc.BlendRTDesc[0].DestBlend      = grcRSV::BLEND_INVSRCALPHA;
		blendStateDesc.BlendRTDesc[0].SrcBlend       = grcRSV::BLEND_SRCALPHA;
		blendStateDesc.BlendRTDesc[0].BlendOp        = grcRSV::BLENDOP_ADD;
		blendStateDesc.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		blendStateDesc.BlendRTDesc[0].SrcBlendAlpha  = grcRSV::BLEND_SRCALPHA;
		blendStateDesc.BlendRTDesc[0].BlendOpAlpha   = grcRSV::BLENDOP_ADD;
		ms_blendStateHandle = grcStateBlock::CreateBlendState(blendStateDesc);

		m_numCollisionInfos = 0;
		m_collisionInfos    = rage_new VfxCollInfo_s[VEHICLE_GLASS_MAX_COLLISIONS];
		m_numExplosionInfos = 0;
		m_explosionInfos    = rage_new VfxExpInfo_s [VEHICLE_GLASS_MAX_EXPLOSIONS];
#if __DEV
		m_collisionSmashFlags = rage_new u8[(VEHICLE_GLASS_MAX_COLLISIONS + 7)/8];
		m_explosionSmashFlags = rage_new u8[(VEHICLE_GLASS_MAX_EXPLOSIONS + 7)/8];
#endif // __DEV

		m_trianglePool = rage_new vfxPoolT<CVehicleGlassTriangle,VEHICLE_GLASS_MAX_TRIANGLES>;
		m_trianglePool->Init();

		m_componentPool = rage_new vfxPoolT<CVehicleGlassComponent,VEHICLE_GLASS_MAX_COMPONENTS>;
		m_componentPool->Init();
		m_componentEntities.Reserve(VEHICLE_GLASS_MAX_COMPONENTS);

		m_componentVBPool = rage_new vfxPoolT<CVehicleGlassVB,VEHICLE_GLASS_MAX_COMPONENT_VB>;
		m_componentVBPool->Init();

#if __PS3
		int extraVertices = 0;
		int extraIndices = 0;
#if __DEV
		PARAM_vgxv.Get(extraVertices);
		PARAM_vgxi.Get(extraIndices);
#endif // __DEV
		m_edgeVertexDataBufferSize = (VEHICLE_GLASS_CACHE_SIZE * VEHICLE_GLASS_MAX_VERTS + extraVertices)*sizeof(Vec4V)*4; // 4 possible streams: pos, normal, UVs, colors
		m_edgeVertexDataBuffer = rage_aligned_new(16) u8[m_edgeVertexDataBufferSize];
		m_edgeIndexDataBufferSize = (VEHICLE_GLASS_CACHE_SIZE * VEHICLE_GLASS_MAX_INDICES + extraIndices)*sizeof(u16);
		m_edgeIndexDataBuffer = rage_aligned_new(16) u16[m_edgeIndexDataBufferSize/sizeof(u16)];

		for(int i = 0; i < VEHICLE_GLASS_CACHE_SIZE; i++)
		{
			m_edgeDataCache[i].lastFrameUsed = 0;
			m_edgeDataCache[i].vertexDataBuffer = (void*)((u8*)m_edgeVertexDataBuffer + i * (m_edgeVertexDataBufferSize / VEHICLE_GLASS_CACHE_SIZE));
			Assert16(m_edgeDataCache[i].vertexDataBuffer);
			m_edgeDataCache[i].indexDataBuffer = (u16*)((u8*)m_edgeIndexDataBuffer + i * (m_edgeIndexDataBufferSize / VEHICLE_GLASS_CACHE_SIZE));
			Assert16(m_edgeDataCache[i].indexDataBuffer);
			m_edgeDataCache[i].m_pJobHandle = NULL;
		}
#endif // __PS3

#if VEHICLE_GLASS_CLUMP_NOISE
		m_clumpNoiseData = NULL;
#endif // VEHICLE_GLASS_CLUMP_NOISE

		// callback for simple triangle access (if we need some external system to be able to access this)
		{
			class GetComponentTriangleCount { public: static int func(const void* p)
			{
				return reinterpret_cast<const CVehicleGlassComponent*>(p)->GetNumTriangles(true);
			}};

			class GetComponentTrianglePoint { public: static void func(const void* p, Vec3V* out_points)
			{
				const CVehicleGlassComponent* pComponent = reinterpret_cast<const CVehicleGlassComponent*>(p);

				if (pComponent->m_pAttachedVB)
				{
					for (int iCurVert = 0; iCurVert < pComponent->m_pAttachedVB->GetVertexCount(); iCurVert++)
					{
						*(out_points++) = Transform(pComponent->m_matrix, pComponent->m_pAttachedVB->GetDamagedVertexP(iCurVert));
					}
				}

			}};

			grcVehicleGlassInterface::SetCallbacks(GetComponentTriangleCount::func, GetComponentTrianglePoint::func);
		}

#if RSG_PC
		CVehicleGlassComponent::CreateStaticVertexBuffers();
#endif // RSG_PC

#if VEHICLE_GLASS_CONSTRUCTOR
#if __PS3
		// setup edge buffers
		{
			fwVehicleGlassConstructorInterface::SetEdgeBuffers(
				m_edgeVertexDataBufferSize,
				m_edgeVertexDataBuffer,
				m_edgeIndexDataBufferSize,
				m_edgeIndexDataBuffer
			);
		}
#endif // __PS3

		// setup material flags
		{
			for (int mtlId = 0; mtlId < PGTAMATERIALMGR->GetNumMaterials(); mtlId++)
			{
				if (PGTAMATERIALMGR->GetIsSmashableGlass((phMaterialMgr::Id)mtlId)) // VFXGROUP_CAR_GLASS
				{
					char mtlIdName[64] = "";
					PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, sizeof(mtlIdName));
					fwVehicleGlassConstructorInterface::SetCarGlassMaterialFlag(mtlId, mtlIdName);
				}
			}
		}
#endif // VEHICLE_GLASS_CONSTRUCTOR
	}
}

void CVehicleGlassManager::Shutdown(unsigned WIN32PC_ONLY(shutdownMode))
{
#if RSG_PC
	if (shutdownMode == SHUTDOWN_CORE)
	{
		CVehicleGlassComponent::DestroyStaticVertexBuffers();
	}
#endif // RSG_PC
}

void CVehicleGlassManager::PrepareVehicleRpfSwitch()
{
	Reset(true);
	CVehicleGlassComponent::StaticShutdown();
}

void CVehicleGlassManager::CompleteVehicleRpfSwitch()
{
	CVehicleGlassComponent::StaticInit();
}

void CVehicleGlassManager::UpdateSafe(float deltaTime)
{
	PF_START(UpdateSafe);

	audSmashableGlassEvent::FrameReset();

#if VEHICLE_GLASS_SMASH_TEST
	g_vehicleGlassSmashTest.UpdateSafe();
#endif // VEHICLE_GLASS_SMASH_TEST

#if __BANK

#if __PS3
	if(m_bInvalideateGeomCache)
	{
		InvalidateCache();
		m_bInvalideateGeomCache = false;
	}
#endif // __PS3

	if (!m_deferHits || m_smashWindowUpdateTessellation)
	{
		for (int i = 0; i < m_numCollisionInfos; i++) { ProcessCollision(m_collisionInfos[i], DEV_SWITCH((m_collisionSmashFlags[i/8] & BIT(i%8)), 0) != 0); }
		for (int i = 0; i < m_numExplosionInfos; i++) { ProcessExplosion(m_explosionInfos[i], DEV_SWITCH((m_explosionSmashFlags[i/8] & BIT(i%8)), 0) != 0); }

		m_numCollisionInfos = 0;
		m_numExplosionInfos = 0;
	}
	else
#endif // __BANK
	{
		int numDeferredCollisionInfos = 0;
		int numDeferredExplosionInfos = 0;

		int hitsLeftThisFrame = VEHICLE_GLASS_MAX_VIS_HITS;

		// Try to defer the hits for windows that are not visible
		for (int i = 0; i < m_numCollisionInfos; i++)
		{
			const CPhysical* pPhysical = static_cast<CPhysical*>(m_collisionInfos[i].regdEnt.Get());

			if (pPhysical && pPhysical->GetIsVisibleInSomeViewportThisFrame() && hitsLeftThisFrame > 0)
			{
				hitsLeftThisFrame--;
				ProcessCollision(m_collisionInfos[i], DEV_SWITCH((m_collisionSmashFlags[i/8] & BIT(i%8)), 0) != 0);
			}
			else
			{
				m_collisionInfos[numDeferredCollisionInfos++] = m_collisionInfos[i];
			}
		}

		for (int i = 0; i < m_numExplosionInfos; i++)
		{
			const CPhysical* pPhysical = static_cast<CPhysical*>(m_explosionInfos[i].regdEnt.Get());

			if (pPhysical && pPhysical->GetIsVisibleInSomeViewportThisFrame() && hitsLeftThisFrame > 0)
			{
				hitsLeftThisFrame--;
				ProcessExplosion(m_explosionInfos[i], DEV_SWITCH((m_explosionSmashFlags[i/8] & BIT(i%8)), 0) != 0);
			}
			else
			{
				m_explosionInfos[numDeferredExplosionInfos++] = m_explosionInfos[i];
			}
		}

		// Process some of the invisible hits if we still have room this frame
		if (hitsLeftThisFrame > 0)
		{
			hitsLeftThisFrame = Min<int>(hitsLeftThisFrame, VEHICLE_GLASS_MAX_NON_VIS_HITS);

			while (numDeferredCollisionInfos > 0)
			{
				int i = --numDeferredCollisionInfos;
				ProcessCollision(m_collisionInfos[i], DEV_SWITCH((m_collisionSmashFlags[i/8] & BIT(i%8)), 0) != 0);

				if (--hitsLeftThisFrame <= 0)
				{
					break;
				}
			}

			if (hitsLeftThisFrame > 0)
			{
				while (numDeferredExplosionInfos > 0)
				{
					int i = --numDeferredExplosionInfos;
					ProcessExplosion(m_explosionInfos[i], DEV_SWITCH((m_explosionSmashFlags[i/8] & BIT(i%8)), 0) != 0);

					if (--hitsLeftThisFrame <= 0)
					{
						break;
					}
				}
			}
		}

		m_numCollisionInfos = numDeferredCollisionInfos;
		m_numExplosionInfos = numDeferredExplosionInfos;
	}

	if (m_componentPool->GetNumItems()<4)
	{
		FreeUpComponentPool();
	}

#if __BANK
	int numAttachedTriangles = 0;
	int numDetachedTriangles = 0;
#endif // __BANK

	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; )
	{
		CVehicleGlassComponent* pNextComponent = m_componentList.GetNext(pComponent);

		if (pComponent->m_removeComponent)
		{
			pComponent->ShutdownComponent();
		}
		else
		{
#if __BANK
			numAttachedTriangles += pComponent->GetNumTriangles(true);
			numDetachedTriangles += pComponent->GetNumTriangles(false) - pComponent->GetNumTriangles(true);
			m_maxAttachedTrianglesPerComponent = Max<int>(pComponent->GetNumTriangles(true), m_maxAttachedTrianglesPerComponent);
			m_maxDetachedTrianglesPerComponent = Max<int>(pComponent->GetNumTriangles(false) - pComponent->GetNumTriangles(true), m_maxDetachedTrianglesPerComponent);
#endif // __BANK

			if (!pComponent->UpdateComponent(deltaTime))
			{
				pComponent->ShutdownComponent();
			}
		}

		pComponent = pNextComponent;
	}

	CVehicleGlassVB::Update();

#if __PS3
	// Update the cache frame counters
	for(int i = 0; i < VEHICLE_GLASS_CACHE_SIZE; i++)
	{
		m_edgeDataCache[i].lastFrameUsed++;

		// If we fetched a cache entry but never used it we still have to call finalize to free the handle
		if(m_edgeDataCache[i].m_pJobHandle)
		{
			m_edgeDataCache[i].pGeomEdge->FinalizeGetVertexAndIndexAsync(m_edgeDataCache[i].m_pJobHandle);
			m_edgeDataCache[i].m_pJobHandle = NULL;
		}
	}
#endif // __PS3

#if __BANK
	if (m_showInfo)
	{
		m_maxActiveComponents = Max<int>(m_componentList.GetNumItems(), m_maxActiveComponents);
		m_maxAttachedTriangles = Max<int>(numAttachedTriangles, m_maxAttachedTriangles);
		m_maxDetachedTriangles = Max<int>(numDetachedTriangles, m_maxDetachedTriangles);

		grcDebugDraw::AddDebugOutput("vehicle glass active comps = %d, max = %d", m_componentList.GetNumItems(), m_maxActiveComponents);
		grcDebugDraw::AddDebugOutput("vehicle glass attached tris = %d, max = %d, max per component = %d", numAttachedTriangles, m_maxAttachedTriangles, m_maxAttachedTrianglesPerComponent);
		grcDebugDraw::AddDebugOutput("vehicle glass detached tris = %d, max = %d, max per component = %d", numDetachedTriangles, m_maxDetachedTriangles, m_maxDetachedTrianglesPerComponent);
	}
#endif // __BANK

#if __ASSERT
	CheckGroups();
#endif // __ASSERT

	CleanupComponentEntities();
	AddToRenderList(CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetEntityListIndex());
	PF_STOP(UpdateSafe);
}

void CVehicleGlassManager::UpdateSafeWhilePaused()
{
#if __BANK
	if (m_smashWindowUpdateTessellation)
	{
		UpdateSafe(0.0f);
		return;
	}
#endif // __BANK

	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	{
		const CPhysical* pPhysical = pComponent->m_regdPhy.Get();

		if (pPhysical && pPhysical->GetIsVisibleInSomeViewportThisFrame())
		{
			pComponent->SetVisible();

#if GTA_REPLAY
			//we need to update the component even if the replay is also pause otherwise
			//the glass will not move if you jump between markers
			if(CReplayMgr::IsReplayInControlOfWorld() && CReplayMgr::IsPlaybackPaused())
			{
				pComponent->UpdateComponent(0.0f);
			}
#endif
		}
	}

	CleanupComponentEntities();
	AddToRenderList(CRenderPhaseDrawSceneInterface::GetRenderPhase()->GetEntityListIndex());
}

void CVehicleGlassManager::UpdateBrokenPart(CEntity* pOldEntity, CEntity* pNewEntity, int componentId)
{
	Assert(pNewEntity->GetIsTypeObject() || pNewEntity->GetIsTypeVehicle());

	const fragInst* pFragInst = pOldEntity->GetFragInst();
	const fragType* pFragType = pFragInst ? pFragInst->GetType() : NULL;

	if (AssertVerify(pFragType))
	{
		for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; )
		{
			CVehicleGlassComponent* pNextComponent = m_componentList.GetNext(pComponent);

			if (pComponent->m_regdPhy.Get() == pOldEntity)
			{
				if (pComponent->m_componentId == componentId)
				{
					if (pNewEntity->GetIsTypeObject() ||
						pNewEntity->GetIsTypeVehicle())
					{
						CPhysical* pOldPhysical = static_cast<CPhysical*>(pOldEntity);
						CPhysical* pNewPhysical = static_cast<CPhysical*>(pNewEntity);

						if (pOldPhysical->IsBrokenFlagSet(componentId)) { pNewPhysical->SetBrokenFlag(componentId); }
						if (pOldPhysical->IsHiddenFlagSet(componentId)) { pNewPhysical->SetHiddenFlag(componentId); }

						pComponent->m_regdPhy = pNewPhysical;
					}
					else // just kill it, we don't support glass attached to non-physical entities
					{
						pComponent->ShutdownComponent();
					}

					break;
				}
			}

			pComponent = pNextComponent;
		}
	}
}

void CVehicleGlassManager::CleanupComponentEntities()
{
#if __DEV
	// Yes, I know. But I promise we don't do any world::Add/Remove
	CGameWorld::AllowDelete(true);
#endif
	// clean up any effects that are now finished and inactive - instead of waiting until they are recycled
	for (int j=0; j<m_componentEntities.GetCount(); j++)
	{	
		s32 refCount = m_componentEntities[j]->GetRef();
		if( refCount < 1 )
		{
			delete m_componentEntities[j];
			m_componentEntities.DeleteFast(j);
			j--; // Due to the way the array delete stuff, we need to ensure we stay on the same line...
		}		
	}

#if __DEV
	// Yes, I know. But I promise we don't do any world::Add/Remove
	CGameWorld::AllowDelete(false);
#endif

}


template <typename T> class SortPtr
{
public:
	inline void Set(T* pObject = NULL, float sortKey = 0.0f)
	{
		m_pObject = pObject;
		m_sortKey = sortKey;
	}

	inline void QSort(int count)
	{
		class CompareFunc { public: static s32 func(const SortPtr* a, const SortPtr* b)
		{
			if      (a->m_sortKey > b->m_sortKey) return +1;
			else if (a->m_sortKey < b->m_sortKey) return -1;
			return 0;
		}};

		qsort(this, count, sizeof(*this), (int(*)(const void*, const void*))CompareFunc::func);
	}

	T*    m_pObject;
	float m_sortKey;
};


void CVehicleGlassManager::AddToRenderList(s32 entityListIndex)
{
	extern bool g_render_lock;
	if (g_render_lock)
	{
		return;
	}

#if __BANK
	if(!m_RenderVehicleGlassAlphaBucket)
	{
		return;
	}
	if (m_disableRender)
	{
		return;
	}

#endif // __BANK


	// Make sure there is something to render
	if (m_componentList.GetNumItems() <= 0)
	{
		return;
	}

	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	vfxAssertf(IsFiniteAll(vCamPos), "invalid camPos %.3f, %.3f, %.3f", VEC3V_ARGS(vCamPos));

	for (int i=0; i<m_componentEntities.GetCount(); i++)
	{
		CVehicleGlassComponent* pComponent = m_componentEntities[i]->GetVehicleGlassComponent();
		if(pComponent && pComponent->CanRender())
		{
			Vec3V origCentroid = Transform(pComponent->m_matrix, pComponent->m_centroid);
			Vec3V centroid = origCentroid;
			{
				CPhysical* pPhysical = const_cast<CPhysical*>(pComponent->GetPhysical());
				if (pPhysical!=(void*)(0xffffffff) && pPhysical!=(void*)(0x00000000))
				{
					CEntity* pEntity = (CEntity*)pPhysical;
					if (pEntity->GetIsTypeVehicle())
					{
						CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
						const fwTransform& transform = pEntity->GetTransform();

						const Vec3V vVehPos = transform.GetPosition();

						vfxAssertf(IsFiniteAll(vVehPos), "invalid vehPos %.3f, %.3f, %.3f", VEC3V_ARGS(vVehPos));


						// To sort the vehicle glass with the alpha on the vehicle, 
						// we work out which "side" of the vehicle's extent the glass is the closest to (either A or B)
						// and then get the one on sides facing the camera to move forward, and push the back one further based on the test on that side.
						//															A
						//			-----------------------------------------------------------------------------------------------------
						//			|												^													|
						//			|												|													|
						//			|												| normalA (tangent)									|
						//			|												|													|
						//			|			normalB (direction)					|													|
						//			|<-----------------------------------------------													|
						//		B	|																									| B
						//			|																									|
						//			|																									|
						//			|																									|
						//			|																									|
						//			-----------------------------------------------------------------------------------------------------
						//															A
						//
						//					 <------------------------------------	Vehicle is facing


						spdAABB box;
						box = pVehicle->GetLocalSpaceBoundBox(box);
						const Vec3V extent = box.GetExtent();
						const ScalarV extentALength = extent.GetX();
						const ScalarV extentBLength = extent.GetY();

						Vec3V vehToGlassDir = centroid - vVehPos;
						Vec3V camToCentroidDir = centroid - vCamPos;				
						
						Vec3V camToVehDir = vVehPos - vCamPos;
						const Vec3V normalA = transform.GetA();
						const Vec3V normalB = transform.GetB();
						const Vec3V extentA = extentALength * normalA;
						const Vec3V extentB = extentBLength * normalB;

						const ScalarV dotA = Dot(vehToGlassDir,extentA);
						const ScalarV dotB = Dot(vehToGlassDir,extentB);

						//Figure out which side of the car is the camera on
						//If glass is in Left or right the vehicle, use Left/Right planes for the orient test
						//We un-transform and un scale the vector to get the right side of the vehicle no matter the scale/orientation of the vehicle/camera
						const Vec3V vehToGlassDirUnTransformed = transform.UnTransform3x3(vehToGlassDir) / extent;
						const BoolV useA = IsGreaterThan(Abs(Dot(vehToGlassDirUnTransformed, Vec3V(V_X_AXIS_WONE))), Abs(Dot(vehToGlassDirUnTransformed, Vec3V(V_Y_AXIS_WONE))));
						const Vec3V normalClosest = SelectFT(useA, dotB * normalB, dotA * normalA);
						const BoolV isClosestSide = IsLessThanOrEqual(Dot(normalClosest,camToVehDir),ScalarV(V_ZERO));

						//Find out the closest side to camera
						const Vec3V camToVehDirUnTransformed = transform.UnTransform3x3(camToVehDir) / extent;
						const BoolV useACam = IsGreaterThan(Abs(Dot(camToVehDirUnTransformed, Vec3V(V_X_AXIS_WONE))), Abs(Dot(camToVehDirUnTransformed, Vec3V(V_Y_AXIS_WONE))));

						//Glass and Camera are NOT on the same side, push it away from the camera
						ScalarV dirOffset = ScalarV(V_ZERO);						
						if(!(IsEqualIntAll(isClosestSide, BoolV(V_TRUE)) && IsEqualIntAll(useA, useACam)))
						{
							dirOffset = BANK_SWITCH(g_VehicleGlassSortedCamDirOffset, VEHICLEGLASS_SORTED_CAM_DIR_OFFSET);
						}
						
						const Vec3V finalOffset = Normalize(camToCentroidDir) * dirOffset;

						//applying offset
						centroid += finalOffset;	

#if __BANK
						if(m_RenderDebugCentroid)
						{
							//Debug only the selected entity
							CEntity *pSelectedEntity = static_cast< CEntity * >(g_PickerManager.GetSelectedEntity());
							if(pEntity && pEntity->GetIsTypeVehicle())
							{
								CVehicle* pSelectedVehicle = static_cast<CVehicle*>(pSelectedEntity);
								if(pSelectedVehicle == pVehicle )
								{
									grcDebugDraw::Arrow(origCentroid, centroid, 0.2f, Color_red);
									spdAABB bboxCar;
									bboxCar = pVehicle->GetLocalSpaceBoundBox(bboxCar);
									grcDebugDraw::BoxOriented(bboxCar.GetMin(), bboxCar.GetMax(), transform.GetMatrix(), Color_blue, false);	
									grcDebugDraw::Sphere(centroid, 0.05f, Color_red, true);								
								}

							}

						}
#endif
					}
				}
			}

			const Vec3V vDiff = vCamPos - centroid;
			const float dist = rage::Max(Mag(vDiff).Getf(), 0.0f);

			//Doing a deferred add because this is called in UpdateSafe which happens in safe execution
			CGtaRenderListGroup::DeferredAddEntity(entityListIndex, m_componentEntities[i], 0, dist, RPASS_ALPHA, 0);
		}
	}

}
void CVehicleGlassManager::RenderManual(CVehicleGlassComponent* pComponent)
{
	if(pComponent == NULL)
	{
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	Lights::ResetLightsUsed();

	grcLightState::SetEnabled(true);
	grcViewport::SetCurrentWorldIdentity();

	CShaderLib::SetGlobalEmissiveScale(g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_PTFX_EMISSIVE_INTENSITY_MULT), true);

	grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
	grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
	grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;
	u8 ActiveStencilRef_Backup = grcStateBlock::ActiveStencilRef;
	u32 ActiveBlendFactors_Backup = grcStateBlock::ActiveBlendFactors;
	u64 ActiveSampleMask_Backup = grcStateBlock::ActiveSampleMask;
	
	grcStateBlock::SetBlendState(ms_blendStateHandle);
	grcStateBlock::SetDepthStencilState(ms_depthStencilStateHandle);
	grcStateBlock::SetRasterizerState(ms_rasterizerStateHandle[0]);
	CShaderLib::SetGlobalInInterior(false);
	CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);

	//const CPhysical* pPhysical = m_vehGlassComponent->GetPhysical();

	// Setup the local lights if the physical has changed
	// We need to do this based on the entity so the selected lights won't change when the glass breaks
	//if (pCurrPhysical != pPhysical)
	{
		pComponent->SetCommonEntityStates();
		//pCurrPhysical = pPhysical;
	}
	pComponent->RenderComponent();

	Lights::ResetLightsUsed();
	grcStateBlock::SetDepthStencilState(DSS_Backup,ActiveStencilRef_Backup);
	grcStateBlock::SetRasterizerState(RS_Backup);
	grcStateBlock::SetBlendState(BS_Backup,ActiveBlendFactors_Backup,ActiveSampleMask_Backup);
	

	CShaderLib::SetGlobalEmissiveScale(1.0f);
}

void CVehicleGlassManager::Render()
{
#if __BANK
	if(m_RenderVehicleGlassAlphaBucket)
	{
		return;
	}
	if (m_disableRender)
	{
		return;
	}

	// Make sure there is something to render
	if (m_componentList.GetNumItems() <= 0)
	{
		return;
	}

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	CRenderPhaseReflection::SetReflectionMap();

	Lights::ResetLightsUsed();
	Lights::BeginLightweightTechniques();

	grcLightState::SetEnabled(true);
	grcViewport::SetCurrentWorldIdentity();

	const grcRasterizerStateHandle   RS_prev = grcStateBlock::RS_Active;
	const grcDepthStencilStateHandle DS_prev = grcStateBlock::DSS_Active;
	const grcBlendStateHandle        BS_prev = grcStateBlock::BS_Active;

	grcStateBlock::SetBlendState(ms_blendStateHandle);
	grcStateBlock::SetDepthStencilState(ms_depthStencilStateHandle);

	CShaderLib::SetGlobalInInterior(false);
	CShaderLib::SetGlobalDeferredMaterial(DEFERRED_MATERIAL_DEFAULT);

#if __BANK
	if (!m_sort)
	{
		PF_START(RenderComponents);

		const CPhysical* pCurrPhysical = NULL;

		for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
		{
			if (pComponent->CanRender())
			{
				const CPhysical* pPhysical = pComponent->GetPhysical();

				// Setup the local lights if the physical has changed
				// We need to do this based on the entity so the selected lights won't change when the glass breaks
				if (pCurrPhysical != pPhysical)
				{
					pComponent->SetCommonEntityStates();
					pCurrPhysical = pPhysical;
				}

				pComponent->RenderComponent();
			}
		}

		PF_STOP(RenderComponents);
	}
	else
#endif // __BANK
	{
		SortPtr<CVehicleGlassComponent> objs[VEHICLE_GLASS_MAX_COMPONENTS];
		int objCount = 0;

		const Mat34V viewToWorld = gVpMan.GetRenderGameGrcViewport()->GetCameraMtx();
		const Vec3V camDir = -viewToWorld.GetCol2();
		const Vec3V camPos = +viewToWorld.GetCol3();

		for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
		{
			if (pComponent->CanRender()) // Don't bother to do the sort if the component is not going to render
			{
				const Vec3V centroid = Transform(pComponent->m_matrix, pComponent->m_centroid);

				objs[objCount++].Set(pComponent, Dot(camDir, camPos - centroid).Getf());
			}
		}

#if __BANK
		if (m_sortReverse)
		{
			for (int i = 0; i < objCount; i++)
			{
				objs[i].m_sortKey *= -1.0f;
			}
		}
#endif // __BANK

		objs[0].QSort(objCount);

		PF_START(RenderComponents);

		grcStateBlock::SetRasterizerState(ms_rasterizerStateHandle[0]);

		const CPhysical* pCurrPhysical = NULL;

		for (int i = 0; i < objCount; i++)
		{
			const CPhysical* pPhysical = objs[i].m_pObject->GetPhysical();

			// Setup the local lights if the physical has changed
			// We need to do this based on the entity so the selected lights won't change when the glass breaks
			if (pCurrPhysical != pPhysical)
			{
				objs[i].m_pObject->SetCommonEntityStates();
				pCurrPhysical = pPhysical;
			}

			objs[i].m_pObject->RenderComponent();
		}

		PF_STOP(RenderComponents);
	}

	Lights::EndLightweightTechniques();
	Lights::ResetLightsUsed();

	grcStateBlock::SetStates(RS_prev, DS_prev, BS_prev); // restore previous stateblocks

#endif // __BANK
}

void CVehicleGlassManager::SmashCollision(CEntity* pEntity, int componentId, float force, Vec3V_In vNormal, bool bDetachAll)
{
	vfxAssertf(force > 0.0f && force <= 1.0f, "force is out of range (%.2f)", force);

	if (pEntity && IsEntitySmashable(pEntity))
	{
		VfxCollInfo_s info;

		info.regdEnt       = pEntity;
		info.vPositionWld  = Vec3V(V_FLT_MAX); // force smash
		info.vNormalWld    = vNormal;
		info.vDirectionWld = Vec3V(V_Z_AXIS_WZERO);
		info.materialId    = PGTAMATERIALMGR->g_idCarGlassWeak;
		info.componentId   = componentId;
		info.weaponGroup   = WEAPON_EFFECT_GROUP_PUNCH_KICK; // TODO: pass is correct weapon group?
		info.force         = bDetachAll ? VEHICLE_GLASS_FORCE_SMASH_DETACH_ALL : force;
		info.isBloody      = false;
		info.isWater       = false;
		info.isExitFx      = false;
		info.noPtFx		   = false;
		info.noPedDamage   = false;
		info.noDecal	   = false; 
		info.isSnowball	   = false;
		info.isFMJAmmo	   = false;

		if (!bDetachAll) // we can't just treat the material as "car glass weak" all the time ..
		{
			const phMaterialMgr::Id materialId = GetSmashableGlassMaterialId(static_cast<const CPhysical*>(pEntity), componentId);

			if (materialId != phMaterialMgr::MATERIAL_NOT_FOUND)
			{
				info.materialId = materialId;
			}
			else
			{
				return;
			}
		}

		StoreCollision(info, true);
	}
}

void CVehicleGlassManager::SmashExplosion(CEntity* pEntity, Vec3V_In vPos, float radius, float force)
{
	vfxAssertf(force > 0.0f, "force is out of range (%.2f)", force);

	if (pEntity && IsEntitySmashable(pEntity))
	{
		VfxExpInfo_s info;

		info.regdEnt             = pEntity;
		info.vPositionWld        = vPos;
		info.radius              = radius;
		info.force               = force;
		info.flags               = VfxExpInfo_s::EXP_FORCE_DETACH;
		StoreExplosion(info, true);
	}
}

void CVehicleGlassManager::StoreCollision(const VfxCollInfo_s& info, bool DEV_ONLY(bSmash), int DEV_ONLY(smashTestModelIndex))
{
	if (info.weaponGroup == WEAPON_EFFECT_GROUP_STUNGUN)
	{
		return; // stun guns don't smash vehicle glass (BS#787570)
	}

#if __DEV
	if (smashTestModelIndex != -1)
	{
#if VEHICLE_GLASS_SMASH_TEST
		g_vehicleGlassSmashTest.StoreCollision(info, smashTestModelIndex);
#endif // VEHICLE_GLASS_SMASH_TEST
		return;
	}
#endif // __DEV

#if __BANK
	if (m_disableVehicleGlass)
	{
		return;
	}
#endif // __BANK

	if (m_numCollisionInfos < VEHICLE_GLASS_MAX_COLLISIONS)
	{
#if __DEV
		if (bSmash)
		{
			m_collisionSmashFlags[m_numCollisionInfos/8] |= BIT(m_numCollisionInfos%8);
		}
		else
		{
			m_collisionSmashFlags[m_numCollisionInfos/8] &= ~BIT(m_numCollisionInfos%8);
		}
#endif // __DEV

		bool bShouldAdd = true;

		for (int i = 0; i < m_numCollisionInfos; i++)
		{
			// Avoid adding multiple collisions to the same component that have the same affect
			if (info.regdEnt.Get() == m_collisionInfos[i].regdEnt.Get() && info.componentId == m_collisionInfos[i].componentId)
			{
				if (IsEqualAll(m_collisionInfos[i].vPositionWld, Vec3V(V_FLT_MAX)))
				{
					bShouldAdd = false; // The hit we have is going to destroy all glass - no need for another one
					break;
				}
				else if (IsEqualAll(info.vPositionWld, Vec3V(V_FLT_MAX)))
				{
					// The new hit is going to destroy all glass - remove any other hit we stored
					m_collisionInfos[i] = info;

					for (int j = i + 1; j < m_numCollisionInfos;)
					{
						if (info.regdEnt.Get() == m_collisionInfos[j].regdEnt.Get())
						{
							// Remove the hit by moving all other hits one slot towards the start of the array
							for (int k = j + 1; k < m_numCollisionInfos; k++)
							{
								m_collisionInfos[k - 1] = m_collisionInfos[k];
							}

							m_numCollisionInfos--;
						}
						else
						{
							j++; // Advance to the next slot
						}
					}
					break;
				}
			}
		}

		if (bShouldAdd)
		{
#if GTA_REPLAY
			if (CReplayMgr::ShouldRecord() && info.regdEnt.Get()->GetIsPhysical())
			{
				CPhysical* pPhysical = static_cast<CPhysical*>(info.regdEnt.Get());
				bool firstGlassHit = pPhysical->IsBrokenFlagSet(info.componentId) == false;
				if(pPhysical->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pPhysical);

					//work out if this was the first hit on the glass pane, if it's currently not 
					//broken or if the vehicle has exploded (which causes all the glass to break)
					firstGlassHit = pVehicle->IsBrokenFlagSet(info.componentId) == false || pVehicle->m_nVehicleFlags.bBlownUp;
				}

				CReplayMgr::RecordFx<CPacketPTexWeaponShot>(
					CPacketPTexWeaponShot(	info,
					false,
					false,
					true,
					firstGlassHit,
					0,
					info.isSnowball),
					info.regdEnt.Get());
			}
#endif // GTA_REPLAY

			m_collisionInfos[m_numCollisionInfos++] = info;

// 			// try to track down where url:bugstar:1823575 is originating from
// #if __ASSERT
// 			CPhysical*    pPhysical = static_cast<CPhysical*>(info.regdEnt.Get());
// 			const phInst* pPhysInst = pPhysical ? pPhysical->GetFragInst() : NULL;
// 
// 			if (pPhysical && IsEntitySmashable(pPhysical) && pPhysical->GetDrawable() && pPhysInst)
// 			{
// 				int componentIdStart = 0;
// 				int componentIdEnd   = 1;
// 
// 				if (info.componentId == -1)
// 				{
// 					const phBound* pBound = pPhysInst->GetArchetype()->GetBound();
// 
// 					if (pBound->GetType() == phBound::COMPOSITE)
// 					{
// 						componentIdEnd = static_cast<const phBoundComposite*>(pBound)->GetNumBounds();
// 					}
// 				}
// 				else
// 				{
// 					componentIdStart = info.componentId;
// 					componentIdEnd   = info.componentId + 1;
// 				}
// 
// 				for (int componentId = componentIdStart; componentId < componentIdEnd; componentId++)
// 				{
// 					if (CanComponentBreak(pPhysical, componentId))
// 					{
// 						CVehicleGlassComponent* pComponent = NULL;
// 						if (pPhysical->IsBrokenFlagSet(componentId))
// 						{
// 							pComponent = FindComponent(pPhysical, componentId);
// 
// 							if (pComponent)
// 							{
// 								const phMaterialMgr::Id mtlIdComponent = PGTAMATERIALMGR->UnpackMtlId(pComponent->m_materialId);
// 								const phMaterialMgr::Id mtlIdInfo = PGTAMATERIALMGR->UnpackMtlId(info.materialId);
// 
// 								if (mtlIdComponent!=mtlIdInfo)
// 								{	
// 									const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName();
// 
// 									char mtlIdName1[128] = "";
// 									PGTAMATERIALMGR->GetMaterialName(mtlIdInfo, mtlIdName1, sizeof(mtlIdName1));
// 
// 									char mtlIdName2[128] = "";
// 									PGTAMATERIALMGR->GetMaterialName(mtlIdComponent, mtlIdName2, sizeof(mtlIdName2));
// 
// 									vfxAssertf(0, "Extra debug info for url:bugstar:1823575 - %s: collision info material = %s, but material for componentId=%d, boneIndex=%d, bone='%s' is \"%s\"", pPhysical->GetModelName(), mtlIdName1, pComponent->m_componentId, pComponent->m_boneIndex, boneName, mtlIdName2);
// 								}
// 							}
// 						}
// 					}
// 				}
//			}
//#endif
		}
	}
	else if (PARAM_vgasserts.Get())
	{
		vfxAssertf(0, "vehicle glass: too many collisions");
	}
}

void CVehicleGlassManager::StoreExplosion(const VfxExpInfo_s& info, bool DEV_ONLY(bSmash))
{
#if __BANK
	if (m_disableVehicleGlass)
	{
		return;
	}
#endif // __BANK

	if (m_numExplosionInfos < VEHICLE_GLASS_MAX_EXPLOSIONS)
	{
#if __DEV
		if (bSmash)
		{
			m_explosionSmashFlags[m_numExplosionInfos/8] |= BIT(m_numExplosionInfos%8);
		}
		else
		{
			m_explosionSmashFlags[m_numExplosionInfos/8] &= ~BIT(m_numExplosionInfos%8);
		}
#endif // __DEV

		bool bShouldAdd = true;

		for (int i = 0; i < m_numExplosionInfos; i++)
		{
			// Avoid adding multiple explosions that have the same affect
			if (info.regdEnt.Get() == m_explosionInfos[i].regdEnt.Get())
			{
				const float fWorldDist = Mag(info.vPositionWld - m_explosionInfos[i].vPositionWld).Getf();

				if (fWorldDist + info.radius >= m_explosionInfos[i].radius)
				{
					// New explosion encapsulates the old one - replace
					m_explosionInfos[i] = info;
					bShouldAdd = false;
					break;
				}
				else if (fWorldDist + m_explosionInfos[i].radius >= info.radius)
				{
					bShouldAdd = false; // New explosion encapsulated by old one - no need to add
					break;
				}
			}
		}

		if (bShouldAdd)
		{
			m_explosionInfos[m_numExplosionInfos++] = info;
		}
	}
	else if (PARAM_vgasserts.Get())
	{
		vfxAssertf(0, "vehicle glass: too many explosions");
	}
}

CVehicleGlassComponent* CVehicleGlassManager::ProcessCollision(VfxCollInfo_s& info, bool bSmash)
{
	PF_START(ProcessCollison);
	
	CPhysical*    pPhysical = static_cast<CPhysical*>(info.regdEnt.Get());
	const phInst* pPhysInst = pPhysical ? pPhysical->GetFragInst() : NULL;

	if (pPhysical == NULL || !IsEntitySmashable(pPhysical) || pPhysical->GetDrawable() == NULL || pPhysInst == NULL)
	{
		info.regdEnt = NULL;
		return NULL;
	}

	if (pPhysical->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pPhysical);
		if (pVehicle && pVehicle->m_bDontProcessVehicleGlass)
		{
			return nullptr;
		}
	}

	int componentIdStart = 0;
	int componentIdEnd   = 1;

	if (info.componentId == -1)
	{
		const phBound* pBound = pPhysInst->GetArchetype()->GetBound();

		if (pBound->GetType() == phBound::COMPOSITE)
		{
			componentIdEnd = static_cast<const phBoundComposite*>(pBound)->GetNumBounds();
		}
	}
	else
	{
		componentIdStart = info.componentId;
		componentIdEnd   = info.componentId + 1;
	}

	bool bProcessDetached = true;

	if (info.vPositionWld.GetXf() != FLT_MAX)
	{
		const float distSqrToCam = CVfxHelper::GetDistSqrToCamera(info.vPositionWld);

		// Skip processing explosions for entities that are too far
		if (distSqrToCam >= BANK_SWITCH(m_vehicleGlassBreakRange*m_vehicleGlassBreakRange, VEHICLE_GLASS_BREAK_RANGE_SQR))
		{
			// Search for the window components
			for (int componentId = componentIdStart; componentId < componentIdEnd; componentId++)
			{
				if (CanComponentBreak(pPhysical, componentId))
				{
					if (pPhysical->IsBrokenFlagSet(componentId))
					{
						// Mark existing components for delete
						CVehicleGlassComponent* pComponent = FindComponent(pPhysical, componentId);

						if (pComponent)
						{
							pComponent->m_removeComponent = true;
						}
					}
					else if (IsComponentSmashableGlass(pPhysical, componentId))
					{
						// Set hidden flag on windows to hide the non-broken glass
						pPhysical->SetHiddenFlag(componentId);
					}
				}
			}

			info.regdEnt = NULL;
			return NULL;
		}

		// No need to process detached glass if its too far to see
		bProcessDetached = (distSqrToCam <= BANK_SWITCH(m_vehicleGlassDetachRange*m_vehicleGlassDetachRange, VEHICLE_GLASS_DETACH_RANGE_SQR));
	}

	CVehicleGlassComponent* pFirstNewComponentCreated = NULL;

	for (int componentId = componentIdStart; componentId < componentIdEnd; componentId++)
	{
		if (CanComponentBreak(pPhysical, componentId))
		{
			CVehicleGlassComponent* pComponent = NULL;

			if (pPhysical->IsBrokenFlagSet(componentId))
			{
				pComponent = FindComponent(pPhysical, componentId);
			}
			else
			{
				pComponent = CreateComponent(pPhysical, componentId);

				if (pFirstNewComponentCreated == NULL && pComponent)
				{
					pFirstNewComponentCreated = pComponent;
				}
			}

			if (pComponent)
			{
				if (pComponent->ProcessComponentCollision(info, bSmash, bProcessDetached))
				{
					// ok
				}
				else if (pComponent->m_regdPhy.Get() == NULL)
				{
					// does this actually happen?
					vfxAssertf(0, "CVehicleGlassManager::ProcessCollision: pComponent->m_regdPhy is NULL (%s)", pPhysical->GetModelName());

					pComponent->ShutdownComponent();
					pPhysical->ResetBrokenAndHiddenFlags();
					NetworkInterface::OnEntitySmashed(pPhysical, componentId, false);
				}
			}
		}
	}

	info.regdEnt = NULL;

	PF_STOP(ProcessCollison);

	return pFirstNewComponentCreated;
}

CVehicleGlassComponent* CVehicleGlassManager::ProcessExplosion(VfxExpInfo_s& info, bool bSmash)
{
	PF_START(ProcessExplosion);

	CPhysical*    pPhysical = static_cast<CPhysical*>(info.regdEnt.Get());
	const phInst* pPhysInst = pPhysical ? pPhysical->GetFragInst() : NULL;

	if (pPhysical == NULL || !IsEntitySmashable(pPhysical) || pPhysical->GetDrawable() == NULL || pPhysInst == NULL)
	{
		info.regdEnt = NULL;
		return NULL;
	}

	if (pPhysical->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pPhysical);
		if (pVehicle)
		{
			if (pVehicle->m_bDontProcessVehicleGlass)
			{
				return nullptr;
			}

			if ((MI_CAR_STROMBERG.IsValid() && pPhysical->GetModelIndex()==MI_CAR_STROMBERG) || (MI_CAR_TOREADOR.IsValid() && pPhysical->GetModelIndex()==MI_CAR_TOREADOR))
			{
				if (pVehicle->GetStatus()!=STATUS_WRECKED)
				{
					return nullptr;
				}
			}
		}
	}

	int componentIdStart = 0;
	int componentIdEnd   = 1;

	if (1)//info.componentId == -1)
	{
		const phBound* pBound = pPhysInst->GetArchetype()->GetBound();

		if (pBound->GetType() == phBound::COMPOSITE)
		{
			componentIdEnd = static_cast<const phBoundComposite*>(pBound)->GetNumBounds();
		}
	}
	else
	{
		//componentIdStart = info.componentId;
		//componentIdEnd   = info.componentId + 1;
	}

	bool bProcessDetached = true;

	if (info.flags & VfxExpInfo_s::EXP_REMOVE_DETATCHED)
	{
		bProcessDetached = false;
	}
	else if (info.vPositionWld.GetXf() != FLT_MAX)
	{
		const float distSqrToCam = CVfxHelper::GetDistSqrToCamera(info.vPositionWld);

		// Skip processing explosions for entities that are too far
		if (distSqrToCam >= BANK_SWITCH(m_vehicleGlassBreakRange*m_vehicleGlassBreakRange, VEHICLE_GLASS_BREAK_RANGE_SQR))
		{
			// Search for the window components
			for (int componentId = componentIdStart; componentId < componentIdEnd; componentId++)
			{
				if (CanComponentBreak(pPhysical, componentId))
				{
					if (pPhysical->IsBrokenFlagSet(componentId))
					{
						// Mark existing components for delete
						CVehicleGlassComponent* pComponent = FindComponent(pPhysical, componentId);

						if (pComponent)
						{
							pComponent->m_removeComponent = true;
						}
					}
					else if (IsComponentSmashableGlass(pPhysical, componentId))
					{
						// Set hidden flag on windows to hide the non-broken glass
						pPhysical->SetHiddenFlag(componentId);
					}
				}
			}

			info.regdEnt = NULL;
			return NULL;
		}

		// No need to process detached glass if its too far to see
		bProcessDetached = (distSqrToCam <= BANK_SWITCH(m_vehicleGlassDetachRange*m_vehicleGlassDetachRange, VEHICLE_GLASS_DETACH_RANGE_SQR));
	}

	CVehicleGlassComponent* pFirstNewComponentCreated = NULL;

	for (int componentId = componentIdStart; componentId < componentIdEnd; componentId++)
	{
		if (CanComponentBreak(pPhysical, componentId))
		{
			bool bShouldProcess = false;

			if (pPhysical->GetIsTypeVehicle())
			{
				// For vehicles we want to make sure the window is actually inside the explosion radius
				const phBound* pWindowBound = GetVehicleGlassWindowBound(pPhysical, componentId);

				if (pWindowBound)
				{
					const fragInst* pFragInst = pPhysical->GetFragInst();
					const fragType* pFragType = pFragInst->GetType();
					if (pFragInst && pFragType)
					{
						fragPhysicsLOD* pFragPhysicsLOD = pFragInst->GetTypePhysics();
						if (pFragPhysicsLOD)
						{
							const fragTypeChild* pFragChild = pFragPhysicsLOD->GetChild(componentId);
							const int boneIndex = pFragType->GetBoneIndexFromID(pFragChild->GetBoneID());

							Mat34V toWorldMat;
							pPhysical->GetGlobalMtx(boneIndex, GTA_ONLY(RC_MATRIX34)(toWorldMat));

							if (pWindowBound && geomBoxes::TestSphereToBox(info.vPositionWld, ScalarV(info.radius), pWindowBound->GetBoundingBoxMin(), pWindowBound->GetBoundingBoxMax(), RCC_MATRIX34(toWorldMat)))
							{
								bShouldProcess = true;
							}
						}
					}
				}
			}
			else
			{
				// Don't bother with detached parts for now
				bShouldProcess = true;
			}

			// Make sure the glass intersects with the explosion sphere
			if (bShouldProcess)
			{
				CVehicleGlassComponent* pComponent = NULL;

				if (pPhysical->IsBrokenFlagSet(componentId))
				{
					pComponent = FindComponent(pPhysical, componentId);
				}
				else
				{
					pComponent = CreateComponent(pPhysical, componentId);

					if (pFirstNewComponentCreated == NULL && pComponent)
					{
						pFirstNewComponentCreated = pComponent;
					}
				}

				if (pComponent)
				{
					if (pComponent->ProcessComponentExplosion(info, bSmash, bProcessDetached))
					{
						// ok
					}
					else if (pComponent->m_regdPhy.Get() == NULL)
					{
						// does this actually happen?
						vfxAssertf(0, "CVehicleGlassManager::ProcessExplosion: pComponent->m_regdPhy is NULL (%s)", pPhysical->GetModelName());

						pComponent->ShutdownComponent();
						pPhysical->ResetBrokenAndHiddenFlags();
						NetworkInterface::OnEntitySmashed(pPhysical, componentId, false);
					}
				}
			}
		}
	}

	info.regdEnt = NULL;

	PF_STOP(ProcessExplosion);

	return pFirstNewComponentCreated;
}

void CVehicleGlassManager::RemoveComponent(const CPhysical* pPhysical, int componentId)
{
	if (componentId != -1)
	{
#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (g_vehicleGlassMan.m_verbose)
		{
			// let's report when this happens, are we sure we don't want to force-smash instead?
			Displayf("%s: removing vehicle glass componentId=%d", pPhysical->GetModelName(), componentId);
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

		if (!pPhysical->IsBrokenFlagSet(componentId))
		{
			Assert(!pPhysical->IsHiddenFlagSet(componentId));
			return; // not broken, nothing to remove
		}
	}

	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	{
		if (pPhysical == pComponent->m_regdPhy.Get())
		{
			if (componentId == -1 || componentId == pComponent->m_componentId)
			{
				pComponent->m_removeComponent = true; // will get removed on next Update
			}
		}
	}
}

void CVehicleGlassManager::FreeUpComponentPool()
{
	const Vec3V camPos = GTA_ONLY(RCC_VEC3V)(camInterface::GetPos());
	CVehicleGlassComponent* pFurthestComponent = NULL;
	ScalarV furthestComponentDistSqr(V_NEGONE);

	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	{
		const Vec3V centroid = Transform(pComponent->m_matrix, pComponent->m_centroid);
		const ScalarV distSqr = MagSquared(centroid - camPos);

		if (IsLessThanAll(furthestComponentDistSqr, distSqr))
		{
			furthestComponentDistSqr = distSqr;
			pFurthestComponent = pComponent;
		}
	}

	if (pFurthestComponent)
	{
		pFurthestComponent->m_removeComponent = true;
	}
}

void CVehicleGlassManager::RemoveFurthestComponent(const CPhysical* pPhysicalToIgnore, const CVehicleGlassComponent* pComponentToIgnore)
{
	/*
	// ====================================================================================
	TODO -- (BS#870201) This is a first-pass solution to the problem of running out of
	triangles. Basically we find the furthest component which is not attached to the same
	entity that is requesting the removal and just delete it, freeing up whatever triangles
	it's using.
	
	A better solution might take these factors into account as well:
		- how long ago the component was created (longer ago = more likely to delete)
		- how much of the window has been smashed (more smashed = more likely to delete)
		- whether the component is behind the camera (behind = more likely to delete)

	In addition to this, instead of simply removing the component we could force-smash it
	(tessellating if necessary) as if it has been smashed with a larger smash sphere.

	Alternatively, if the component has only a few triangles removed (and does not have
	any deformation applied), we could simply fix the component (and remove it) instead
	of deleting the component .. so that the window appears unbroken.
	// ====================================================================================
	*/

	const Vec3V camPos = GTA_ONLY(RCC_VEC3V)(camInterface::GetPos());
	CVehicleGlassComponent* pFurthestComponent = NULL;
	ScalarV furthestComponentDistSqr(V_NEGONE);

	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	{
		const CPhysical* pPhysical = pComponent->m_regdPhy.Get();

		if (pComponent->m_triangleListDetached.GetNumItems() > 0) // Skip components that don't have detached triangles
		{
			if (pPhysical && pPhysical != pPhysicalToIgnore && pComponent != pComponentToIgnore)
			{
				const ScalarV distSqr = MagSquared(pComponent->m_centroid - camPos);

				if (IsLessThanAll(furthestComponentDistSqr, distSqr))
				{
					furthestComponentDistSqr = distSqr;
					pFurthestComponent = pComponent;
				}
			}
		}
	}

	if (pFurthestComponent)
	{
		CPhysical* pPhysical = pFurthestComponent->m_regdPhy.Get();

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		const int numTrianglesInPoolPrev = m_trianglePool->GetNumItems();
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

		// When shutting down a component we have to make sure it is set to hidden as we are not going to render it anymore
		if (pPhysical && pFurthestComponent->m_componentId != -1)
		{
			pPhysical->SetHiddenFlag(pFurthestComponent->m_componentId);
		}

		pFurthestComponent->ClearDetached();

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		if (m_verbose)
		{
			const int numTrianglesFreed = m_trianglePool->GetNumItems() - numTrianglesInPoolPrev;
			const char* boneName = pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pFurthestComponent->m_boneIndex)->GetName();

			Displayf("%s: removing vehicle glass '%s' (dist = %f) .. freed %d triangles", pPhysical->GetModelName(), boneName, sqrtf(furthestComponentDistSqr.Getf()), numTrianglesFreed);
		}
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
	}
}

void CVehicleGlassManager::RemoveAllGlass(CVehicle* pVehicle)
{
	BANK_ONLY( if(m_ignoreRemoveAllGlass) return; )

	// Remove all existing components
	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	{
		if (pVehicle == pComponent->m_regdPhy.Get())
		{
			pComponent->m_removeComponent = true; // will get removed on next Update
		}
	}

	// Go over all the vehicle components and make sure all glass is removed
	const fragInst* pFragInst = pVehicle->GetFragInst();
	const phBound* pBound = pFragInst->GetArchetype()->GetBound();
	int componentIdEnd = 1;

	if (pBound->GetType() == phBound::COMPOSITE)
	{
		componentIdEnd = static_cast<const phBoundComposite*>(pBound)->GetNumBounds();
	}

	for (int componentId = 0; componentId < componentIdEnd; componentId++)
	{
		if (IsComponentSmashableGlass(pVehicle, componentId))
		{
			pVehicle->SetBrokenFlag(componentId);
			pVehicle->SetHiddenFlag(componentId);
		}
	}
}

void CVehicleGlassManager::ApplyVehicleDamage(const CVehicle* pVehicle)
{
#if __BANK || !USE_GPU_VEHICLEDEFORMATION
	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	{
		if (pVehicle == pComponent->m_regdPhy.Get())
		{
			pComponent->m_damageDirty = true;
		}
	}
#else
	(void)pVehicle;
#endif // __BANK || !USE_GPU_VEHICLEDEFORMATION
}

void CVehicleGlassManager::ConvertibleRoofAnimStart(CVehicle* pVehicle)
{
	if (pVehicle->AreAnyHiddenFlagsSet() BANK_ONLY(&& m_smashWindowsWhenConvertibleRoofAnimPlays))
	{
		for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
		{
			if (pComponent->m_regdPhy.Get() == pVehicle &&
				pComponent->m_smashed)
			{
				if (pComponent->m_hierarchyId == VEH_WINDOW_LF || // only side windows will smash ..
					pComponent->m_hierarchyId == VEH_WINDOW_RF ||
					pComponent->m_hierarchyId == VEH_WINDOW_LR ||
					pComponent->m_hierarchyId == VEH_WINDOW_RR)
				{
					static const float force = 0.00001f; // Use a very small force because we want the broken shards to just drop down
					pVehicle->SmashWindow(pComponent->m_hierarchyId, force, true, false);
				}
			}
		}
	}
}

__COMMENT(static) void CVehicleGlassManager::PreRender2(CPhysical* pPhysical)
{
	const fragInst* pFragInst = pPhysical->GetFragInst();

	if (AssertVerify(pFragInst))
	{
		const phBound* pBound = pFragInst->GetArchetype()->GetBound();
		const int componentIdStart = 0;
		const int componentIdEnd = (pBound->GetType() == phBound::COMPOSITE) ? static_cast<const phBoundComposite*>(pBound)->GetNumBounds() : 1;

		for (int componentId = componentIdStart; componentId < componentIdEnd; componentId++)
		{
			if (pPhysical->IsHiddenFlagSet(componentId))
			{
				const fragTypeChild* pFragChild = pFragInst->GetTypePhysics() ? pFragInst->GetTypePhysics()->GetChild(componentId) : NULL;
				if (pFragChild)
				{
					const int boneIndex = pFragInst->GetType() ? pFragInst->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID()) : -1;

					if (boneIndex != -1)
					{
						Mat34V mat;
						pPhysical->GetGlobalMtx(boneIndex, GTA_ONLY(RC_MATRIX34)(mat));
						mat.SetCol0(Vec3V(V_ZERO));
						mat.SetCol1(Vec3V(V_ZERO));
						mat.SetCol2(Vec3V(V_ZERO));
						pPhysical->SetGlobalMtx(boneIndex, GTA_ONLY(RCC_MATRIX34)(mat));
					}
				}
			}
		}
	}
}


__COMMENT(static) bool CVehicleGlassManager::IsComponentSmashableGlass(const CPhysical* pPhysical, int componentId)
{
	return GetSmashableGlassMaterialId(pPhysical, componentId) != phMaterialMgr::MATERIAL_NOT_FOUND;
}

__COMMENT(static) bool CVehicleGlassManager::CanComponentBreak(const CPhysical* pPhysical, int componentId)
{
	if (pPhysical->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);
		const fragInst* pFragInst = pPhysical->GetFragInst();
		if (pFragInst==NULL)
		{
			return false;
		}

		const fragType* pFragType = pFragInst->GetType();
		if (pFragType==NULL)
		{
			return false;
		}

		vfxAssertf(pFragType && pFragType->IsModelSkinned(), "%s: non-skinned frag??", pPhysical->GetModelName());

		fragPhysicsLOD* pFragPhysicsLOD = pFragInst->GetTypePhysics();
		if (pFragPhysicsLOD==NULL)
		{
			return false;
		}

		const fragTypeChild* pFragChild = pFragPhysicsLOD->GetChild(componentId);
		const int boneIndex = pFragType->GetBoneIndexFromID(pFragChild->GetBoneID());

		if (boneIndex >= 0)
		{
			// NOTE: We're not going to hit non-standard windows here (VEH_MISC_A etc.) but that's
			// ok because convertible roof animation won't use them and neither will IsWindowDown.
			for (int id = VEH_FIRST_WINDOW; id <= VEH_LAST_WINDOW; id++)
			{
				const int iWindowBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)id);

				if (iWindowBoneIndex == boneIndex)
				{
					if (id == VEH_WINDSCREEN_R && pVehicle->DoesVehicleHaveAConvertibleRoofAnimation() && pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED)
					{
						// Don't break convertible rear window when roof is raised (B*1283490)
						return false;
					}
					else if (pVehicle->IsWindowDown((eHierarchyId)id))
					{
						// Don't break lowered windows (B*1347700)
						return false;
					}
				}
			}
		}
		else
		{
			// Can't break it if it doesn't exist
			return false;
		}
	}

	return true;
}

__COMMENT(static) phBound* CVehicleGlassManager::GetVehicleGlassWindowBound(const CPhysical* pPhysical, int componentId)
{
	const fragInst* pFragInst = pPhysical->GetFragInst();

	if (pFragInst)
	{
		const phArchetype* pArchetype = pFragInst->GetArchetype();

		if (AssertVerify(pArchetype))
		{
			const phBound* pBound = pArchetype->GetBound();

			if (pBound->GetType() == phBound::COMPOSITE)
			{
				return static_cast<const phBoundComposite*>(pBound)->GetBound(componentId);
			}
		}
	}

	return NULL;
}

__COMMENT(static) const CVehicleGlassVB* CVehicleGlassManager::GetVehicleGlassComponentStaticVB(const CPhysical* pPhysical, int componentId)
{
	const CVehicleGlassComponent* pComponent = g_vehicleGlassMan.FindComponent(pPhysical, componentId);

	if (pComponent)
	{
		return pComponent->GetStaticVB();
	}

	return NULL;
}

__COMMENT(static) Vec4V_Out CVehicleGlassManager::GetVehicleGlassComponentSmashSphere(const CPhysical* pPhysical, int componentId, bool bLocal)
{
	const CVehicleGlassComponent* pComponent = g_vehicleGlassMan.FindComponent(pPhysical, componentId);

	if (pComponent && pComponent->m_smashed)
	{
		Vec4V smashSphere = pComponent->m_smashSphere;

		if (!bLocal)
		{
			smashSphere = TransformSphere(pComponent->m_matrix, smashSphere);
		}

		return smashSphere;
	}

	return Vec4V(V_ZERO);
}

__COMMENT(static) const CVehicleGlassComponent* CVehicleGlassManager::GetVehicleGlassComponent(const CPhysical* pPhysical, int componentId)
{
	const CVehicleGlassComponent* pComponent = g_vehicleGlassMan.FindComponent(pPhysical, componentId);

	if (pComponent)
	{
		return pComponent;
	}

	return NULL;
}

#if GTA_REPLAY

audSound** CVehicleGlassManager::GetAnyFallLoopSound()
{
	//for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	//{
	//	audSound*& pFallLoop = pComponent->m_audioEntity.GetFallLoopRef();

	//	if (pFallLoop)
	//	{
	//		return &pFallLoop;
	//	}
	//}

	return NULL;
}

#endif // GTA_REPLAY

CVehicleGlassComponent* CVehicleGlassManager::FindComponent(const CPhysical* pPhysical, int componentId)
{
	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
	{
		if (pComponent->m_regdPhy.Get() == pPhysical &&
			pComponent->m_componentId == componentId)
		{
			return pComponent;
		}
	}

	return NULL;
}

CVehicleGlassComponent* CVehicleGlassManager::CreateComponent(CPhysical* pPhysical, int componentId)
{
	const phMaterialMgr::Id mtlId = GetSmashableGlassMaterialId(pPhysical, componentId);

	if (mtlId == phMaterialMgr::MATERIAL_NOT_FOUND)
	{
		return NULL; // not smashable glass
	}

	CVehicleGlassComponent* pComponent = m_componentList.GetFromPool(m_componentPool);

	if (pComponent)
	{
		pComponent->InitComponent(pPhysical, componentId, mtlId);
		
		if (pComponent->GetNumTriangles(true) == 0) // this can happen on pc, for example
		{
			pComponent->ShutdownComponent();
			pComponent = NULL;
		}

		if (pComponent && CVehicleGlassComponentEntity::_ms_pPool->GetNoOfFreeSpaces()>0) 
		{
			CVehicleGlassComponentEntity* pVehicleGlassComponentEntity = rage_new CVehicleGlassComponentEntity(ENTITY_OWNEDBY_VFX, pComponent);
			vfxAssertf(pVehicleGlassComponentEntity, "CVehicleGlassManager::CreateComponent - invalid entry found in the way in list");
			pVehicleGlassComponentEntity->SetDrawHandler(pVehicleGlassComponentEntity->AllocateDrawHandler(NULL));

			pComponent->SetComponentEntity(pVehicleGlassComponentEntity);
			m_componentEntities.Append() = pVehicleGlassComponentEntity;
		}
	}
	else if (PARAM_vgasserts.Get())
	{
		vfxAssertf(0, "vehicle glass: failed to create component (%s)", pPhysical->GetModelName());
	}

	return pComponent;
}

#if __PS3

void* CVehicleGlassManager::GetEdgeVertexDataBuffer()
{
	// Raw access to the vertex data memory will overwrite the cache so it has to be invalidated
	InvalidateCache();

	return m_edgeVertexDataBuffer;
}

u16* CVehicleGlassManager::GetEdgeIndexDataBuffer()
{
	// Raw access to the vertex data memory will overwrite the cache so it has to be invalidated
	InvalidateCache();

	return m_edgeIndexDataBuffer;
}

void CVehicleGlassManager::PrepareCache()
{
	BANK_ONLY( if(!m_bExtractGeomEarly) return;)

	int cacheSlotsLeft = VEHICLE_GLASS_CACHE_SIZE;

	// Try to preare the cache for the hits for windows that are not visible
	for (int i = 0; i < m_numCollisionInfos; i++)
	{
		const CPhysical* pPhysical = static_cast<CPhysical*>(m_collisionInfos[i].regdEnt.Get());

		if (pPhysical && pPhysical->GetIsVisibleInSomeViewportThisFrame())
		{
			// Try to prepare the cache for this hit
			if(PrepareCacheForHit(pPhysical, m_collisionInfos[i].componentId))
			{
				// One less slot we can do this frame
				cacheSlotsLeft--;
			}

			// Check if we filled all the slots
			if(cacheSlotsLeft == 0)
			{
				break;
			}
		}
	}
	if(cacheSlotsLeft > 0)
	{
		for (int i = 0; i < m_numExplosionInfos; i++)
		{
			const CPhysical* pPhysical = static_cast<CPhysical*>(m_explosionInfos[i].regdEnt.Get());

			if (pPhysical && pPhysical->GetIsVisibleInSomeViewportThisFrame())
			{
				// Try to prepare the cache for this hit
				// Since we don't have a component use 0 which will fetch the first window
				if(PrepareCacheForHit(pPhysical))
				{
					// One less slot we can do this frame
					cacheSlotsLeft--;
				}

				// Check if we filled all the slots
				if(cacheSlotsLeft == 0)
				{
					break;
				}
			}
		}
	}
}

void CVehicleGlassManager::InvalidateCache()
{
	for(int i = 0; i < VEHICLE_GLASS_CACHE_SIZE; i++)
	{
		// If there is a running extraction job wait for it to complete
		// This will help avoid two jobs writing to the same memory issues
		if(m_edgeDataCache[i].m_pJobHandle && m_edgeDataCache[i].pGeomEdge)
		{
			m_edgeDataCache[i].pGeomEdge->FinalizeGetVertexAndIndexAsync(m_edgeDataCache[i].m_pJobHandle);
			m_edgeDataCache[i].m_pJobHandle = NULL;
		}

		// Clear the cache entry values
		m_edgeDataCache[i].pGeomEdge = NULL;
		m_edgeDataCache[i].modelIndex = 0;
		m_edgeDataCache[i].geomIndex = -1;
		m_edgeDataCache[i].lastFrameUsed = 0;
	}
}

bool CVehicleGlassManager::SetModelCachedThisFrame(const grmGeometryEdge* pGeomEdge, u32 modelIndex, int geomIndex)
{
	for(int i = 0; i < VEHICLE_GLASS_CACHE_SIZE; i++)
	{
		if(m_edgeDataCache[i].pGeomEdge == pGeomEdge && m_edgeDataCache[i].modelIndex == modelIndex && m_edgeDataCache[i].geomIndex == geomIndex)
		{
			// Indicate this model was used this frame
			m_edgeDataCache[i].lastFrameUsed = 0;

			return true;
		}
	}

	return false;
}

int CVehicleGlassManager::GetModelCache(const grmGeometryEdge* pGeomEdge, u32 modelIndex, int geomIndex)
{
	for(int i = 0; i < VEHICLE_GLASS_CACHE_SIZE; i++)
	{
		if(m_edgeDataCache[i].pGeomEdge == pGeomEdge && m_edgeDataCache[i].modelIndex == modelIndex && m_edgeDataCache[i].geomIndex == geomIndex)
		{
			// Indicate this model was used this frame
			m_edgeDataCache[i].lastFrameUsed = 0;
			
			// If the extraction job is still going wait until its done
			if(m_edgeDataCache[i].m_pJobHandle)
			{
				m_edgeDataCache[i].pGeomEdge->FinalizeGetVertexAndIndexAsync(m_edgeDataCache[i].m_pJobHandle);
				m_edgeDataCache[i].m_pJobHandle = NULL;
			}

			return i;
		}
	}

	return -1;
}

bool CVehicleGlassManager::AddModelCache(const grmGeometryEdge* pGeomEdge, u32 modelIndex, int geomIndex)
{
	// Search for the cache slot we want to overwrite
	// Always pick an empty slot first rather then overwrite one
	// When there is no empty slot just search for the oldest entry
	u32 maxLastFrameUsed = 0;
	int cacheIdx = -1;
	for(int i = 0; i < VEHICLE_GLASS_CACHE_SIZE; i++)
	{
		if(m_edgeDataCache[i].pGeomEdge == NULL)
		{
			cacheIdx = i;
			break;
		}
		else if(maxLastFrameUsed <= m_edgeDataCache[i].lastFrameUsed)
		{
			maxLastFrameUsed = m_edgeDataCache[i].lastFrameUsed;
			cacheIdx = i;
		}
	}

	// Clean the pending job
	// This could happen if we decided to cache something that didn't get used
	if(m_edgeDataCache[cacheIdx].m_pJobHandle)
	{
		m_edgeDataCache[cacheIdx].pGeomEdge->FinalizeGetVertexAndIndexAsync(m_edgeDataCache[cacheIdx].m_pJobHandle);
		m_edgeDataCache[cacheIdx].m_pJobHandle = NULL;
	}

	// Initialize the cache entry data
	m_edgeDataCache[cacheIdx].pGeomEdge = pGeomEdge;
	m_edgeDataCache[cacheIdx].modelIndex = modelIndex;
	m_edgeDataCache[cacheIdx].geomIndex = geomIndex;
	m_edgeDataCache[cacheIdx].lastFrameUsed = 0;

	sysMemSet(&m_edgeDataCache[cacheIdx].edgeVertexStreams[0], 0, sizeof(m_edgeDataCache[cacheIdx].edgeVertexStreams));

	m_edgeDataCache[cacheIdx].BoneOffset1     = 0;
	m_edgeDataCache[cacheIdx].BoneOffset2     = 0;
	m_edgeDataCache[cacheIdx].BoneOffsetPoint = 0;
	m_edgeDataCache[cacheIdx].BoneIndexOffset = 0;
	m_edgeDataCache[cacheIdx].BoneIndexStride = 1;

#if HACK_GTA4_MODELINFOIDX_ON_SPU
	m_edgeDataCache[cacheIdx].gtaSpuInfoStruct.gta4RenderPhaseID = 0; // these don't actually matter, do they?
	m_edgeDataCache[cacheIdx].gtaSpuInfoStruct.gta4ModelInfoIdx  = 0;
	m_edgeDataCache[cacheIdx].gtaSpuInfoStruct.gta4ModelInfoType = 0;
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU

	m_edgeDataCache[cacheIdx].numExtractedIndices = 0;
	m_edgeDataCache[cacheIdx].numExtractedVerts = 0;

	bool bRes = pGeomEdge->BegineGetVertexAndIndexAsync(&m_edgeDataCache[cacheIdx].m_pJobHandle,
		(Vector4*)m_edgeDataCache[cacheIdx].vertexDataBuffer,
		VEHICLE_GLASS_MAX_VERTS,
		(Vector4**)m_edgeDataCache[cacheIdx].edgeVertexStreams,
		m_edgeDataCache[cacheIdx].indexDataBuffer,
		VEHICLE_GLASS_MAX_INDICES,
		m_edgeDataCache[cacheIdx].BoneIndexesAndWeights,
		sizeof(m_edgeDataCache[cacheIdx].BoneIndexesAndWeights),
		&m_edgeDataCache[cacheIdx].BoneIndexOffset,
		&m_edgeDataCache[cacheIdx].BoneIndexStride,
		&m_edgeDataCache[cacheIdx].BoneOffset1,
		&m_edgeDataCache[cacheIdx].BoneOffset2,
		&m_edgeDataCache[cacheIdx].BoneOffsetPoint,
		(u32*)&m_edgeDataCache[cacheIdx].numExtractedVerts,
		(u32*)&m_edgeDataCache[cacheIdx].numExtractedIndices,
#if HACK_GTA4_MODELINFOIDX_ON_SPU
		&m_edgeDataCache[cacheIdx].gtaSpuInfoStruct,
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU
		NULL,
		CExtractGeomParams::extractPos
		| CExtractGeomParams::extractNorm
		| CExtractGeomParams::extractUv
		| CExtractGeomParams::extractCol
		| CExtractGeomParams::extractColNoWarning // don't warn, this is slightly annoying .. maybe fix this sometime
		);

	return bRes;
}

u8* CVehicleGlassManager::GetCacheBoneData(int cacheIdx, int& BoneOffset1, int& BoneOffset2, int& BoneOffsetPoint, int& BoneIndexOffset, int& BoneIndexStride)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	BoneOffset1 = m_edgeDataCache[cacheIdx].BoneOffset1;
	BoneOffset2 = m_edgeDataCache[cacheIdx].BoneOffset2;
	BoneOffsetPoint = m_edgeDataCache[cacheIdx].BoneOffsetPoint;
	BoneIndexOffset = m_edgeDataCache[cacheIdx].BoneIndexOffset;
	BoneIndexStride = m_edgeDataCache[cacheIdx].BoneIndexStride;
	return m_edgeDataCache[cacheIdx].BoneIndexesAndWeights;
}

int CVehicleGlassManager::GetCacheNumIndices(int cacheIdx)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	return m_edgeDataCache[cacheIdx].numExtractedIndices;
}

int CVehicleGlassManager::GetCacheNumVertices(int cacheIdx)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	return m_edgeDataCache[cacheIdx].numExtractedVerts;
}

const u16* CVehicleGlassManager::GetCacheIndices(int cacheIdx)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	return m_edgeDataCache[cacheIdx].indexDataBuffer;
}

const Vec3V* CVehicleGlassManager::GetCachePositions(int cacheIdx)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	return (const Vec3V*)m_edgeDataCache[cacheIdx].edgeVertexStreams[CExtractGeomParams::obvIdxPositions];
}

const Vec4V* CVehicleGlassManager::GetCacheTexcoords(int cacheIdx)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	return m_edgeDataCache[cacheIdx].edgeVertexStreams[CExtractGeomParams::obvIdxUVs];
}

const Vec3V* CVehicleGlassManager::GetCacheNormals(int cacheIdx)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	return (const Vec3V*)m_edgeDataCache[cacheIdx].edgeVertexStreams[CExtractGeomParams::obvIdxNormals];
}

const Vec4V* CVehicleGlassManager::GetCacheColors(int cacheIdx)
{
	vfxAssertf(m_edgeDataCache[cacheIdx].m_pJobHandle == NULL, "Vehicle glass cache is not ready!");

	return m_edgeDataCache[cacheIdx].edgeVertexStreams[CExtractGeomParams::obvIdxColors];
}

extern const rmcDrawable* GetVehicleGlassDrawable(const CPhysical* pPhysical);
bool CVehicleGlassManager::PrepareCacheForHit(const CPhysical* pPhysical, int componentId)
{
	// Get the geometry index through the window
	// If the window data is missing its a sign this vehicle has no glass
	fragInst* pFragInst = pPhysical->GetFragInst();
	if(pFragInst)
	{
		const fragType* pFragType = pFragInst->GetType();
		if (pFragType)
		{
			const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pFragType)->m_vehicleWindowData;
			if(pWindowData)
			{
				// Try to grab the window
				int geomIndex = -1;
				const fwVehicleGlassWindow* pWindow = NULL;
				if(componentId >= 0)
				{
					// If we have a component just get the related window
					pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, componentId);
				}
				else
				{
					// For explosions we don't care which window so just grab the first one
					pWindow = fwVehicleGlassWindowData::GetWindowByIndex(pWindowData, 0);
				}

				// If we found the window get the geometry index
				if (pWindow)
				{
					geomIndex = pWindow->m_geomIndex;
				}

				// If we found the geometry we can use it to prepare the cache
				if (geomIndex != -1)
				{
					const rmcDrawable* pDrawable = GetVehicleGlassDrawable(pPhysical);
					const rmcLodGroup& lodGroup = pDrawable->GetLodGroup();
					const grmModel& model = lodGroup.GetLodModel0(LOD_HIGH);
					const grmGeometry& geom = model.GetGeometry(geomIndex);
					if (geom.GetType() == grmGeometry::GEOMETRYEDGE) // We only cache edge geometry
					{
						// Check if the model is already cached
						const grmGeometryEdge* pGeomEdge = static_cast<const grmGeometryEdge*>(&geom);
						const int modelIndex = pPhysical->GetModelIndex();
						if(!SetModelCachedThisFrame(pGeomEdge, modelIndex, geomIndex))
						{
							// Model is not cached so we should add it
							ASSERT_ONLY( bool bRes = ) AddModelCache(pGeomEdge, modelIndex, geomIndex);
							vfxAssertf(bRes, "Vehicle glass failed to add model to cache");
						}

						return true;
					}
				}
			}
		}
	}

	return false;
}
#endif // __PS3

#if VEHICLE_GLASS_CLUMP_NOISE

template <typename T> __forceinline T Wrap(T x, T n)
{
	return x >= T(0) ? (x % n) : ((n - 1) - ((-(x + 1)) % n));
}

void CVehicleGlassManager::GenerateLowDiscrepancyClumpNoise()
{
	if (m_clumpNoiseData)
	{
		delete[] m_clumpNoiseData;
	}

	m_clumpNoiseData = rage_new u8[m_clumpNoiseSize*VEHICLE_GLASS_CLUMP_NOISE_BUFFER_COUNT];

	const float clumpExponent  = powf(2.0f, m_clumpExponentBias);
	const float clumpRadiusMin = m_clumpRadiusMin*(float)m_clumpNoiseSize/(float)m_clumpNoiseBuckets;
	const float clumpRadiusMax = m_clumpRadiusMax*(float)m_clumpNoiseSize/(float)m_clumpNoiseBuckets;
	const float clumpHeightMin = m_clumpHeightMin;
	const float clumpHeightMax = m_clumpHeightMax;

	float* temp = Alloca(float, m_clumpNoiseSize);

	const u64 prevSeed = g_DrawRand.GetFullSeed();

	g_DrawRand.Reset(m_smashWindowUpdateRandomSeed);

	for (int bufferIndex = 0; bufferIndex < VEHICLE_GLASS_CLUMP_NOISE_BUFFER_COUNT; bufferIndex++)
	{
		sysMemSet(temp, 0, m_clumpNoiseSize*sizeof(float));

		for (int i = 0; i < m_clumpNoiseBuckets; i++)
		{
			if (m_clumpNoiseBucketIsolate != -1 &&
				m_clumpNoiseBucketIsolate != i)
			{
				continue;
			}

			const int   clumpOffsetMin = (int)floorf((((float)i - m_clumpSpread/2.0f)*(float)m_clumpNoiseSize)/(float)m_clumpNoiseBuckets);
			const int   clumpOffsetMax = (int)ceilf ((((float)i + m_clumpSpread/2.0f)*(float)m_clumpNoiseSize)/(float)m_clumpNoiseBuckets);
			const int   clumpOffset = g_DrawRand.GetRanged(clumpOffsetMin, clumpOffsetMax);
			const float clumpRadius = g_DrawRand.GetRanged(clumpRadiusMin, clumpRadiusMax);
			const float clumpHeight = g_DrawRand.GetRanged(clumpHeightMin, clumpHeightMax);

			const int j0 = clumpOffset - (int)clumpRadius;
			const int j1 = clumpOffset + (int)clumpRadius;

			for (int j = j0; j <= j1; j++)
			{
				const float x = -1.0f + 2.0f*(float)(j - j0)/(float)(j1 - j0); // [-1..1]
				const float h = 1.0f - powf(Abs<float>(x), clumpExponent); // [0..1]

				const int jj = Wrap<int>(j, m_clumpNoiseSize);

				temp[jj] = Max<float>(h*clumpHeight, temp[jj]);
			}
		}

		for (int i = 0; i < m_clumpNoiseSize; i++)
		{
			const int ii = Wrap<int>(i + m_clumpNoiseGlobalOffset, m_clumpNoiseSize);

			m_clumpNoiseData[ii + bufferIndex*m_clumpNoiseSize] = (u8)Clamp<float>(256.0f*temp[i], 0.0f, 255.0f);
		}
	}

	g_DrawRand.SetFullSeed(prevSeed);
}

#endif // VEHICLE_GLASS_CLUMP_NOISE

#if __ASSERT

void CVehicleGlassManager::CheckGroups()
{
	CVehicleGlassComponent* pNextComponent = NULL;

	for (CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = pNextComponent)
	{
		pNextComponent = m_componentList.GetNext(pComponent);

		const CPhysical* pPhysical = pComponent->m_regdPhy.Get();

		if (!Verifyf(pPhysical, "%s: vehicle glass physical entity has been deleted", CBaseModelInfo::GetModelName(pComponent->m_modelIndex GTA_ONLY(.Get()))))
		{
			continue;
		}

		const fragInst* pFragInst = pPhysical->GetFragInst();
		if (pFragInst)
		{
			fragPhysicsLOD* pFragPhysicsLOD = pFragInst->GetTypePhysics();
			if (pFragPhysicsLOD)
			{
				const fragTypeChild* pFragChild = pFragPhysicsLOD->GetChild(pComponent->m_componentId);
				const fragType* pFragType = pFragInst->GetType();
				if (pFragType)
				{
					int boneIndex = pFragType->GetBoneIndexFromID(pFragChild->GetBoneID());

					if (pPhysical->GetIsTypeVehicle())
					{
						const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);
						boneIndex = pVehicle->GetVehicleModelInfo()->GetLodSkeletonBoneIndex(boneIndex);
					}

					if (boneIndex != pComponent->m_boneIndex)
					{
						vfxAssertf(
							0,
							"%s: matrix index %d does not match bone index %d for '%s'",
							pPhysical->GetModelName(),
							pComponent->m_boneIndex,
							boneIndex,
							pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName()
							);
					}
				}
			}
		}

		if (!pPhysical->IsBrokenFlagSet(pComponent->m_componentId))
		{
			if (pPhysical->DoesBrokenHiddenComponentExist())
			{
				vfxAssertf(
					0,
					"%s: vehicle glass component for '%s' (id=%d) is no longer broken .. CANNOT DELETE COMPONENT",
					pPhysical->GetModelName(),
					pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName(),
					pComponent->m_componentId
				);
			}
			else
			{
				vfxAssertf(
					0,
					"%s: vehicle glass component for '%s' (id=%d) broken/hidden component is gone .. CANNOT DELETE COMPONENT",
					pPhysical->GetModelName(),
					pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName(),
					pComponent->m_componentId
				);
			}

			// i'd like to delete the component here, but that would introduce a discrepancy between assert and non-assert builds
			//pComponent->ShutdownComponent();
			continue;
		}
		else if (!pPhysical->IsHiddenFlagSet(pComponent->m_componentId))
		{
			if (pComponent->m_cracked)
			{
				if (pPhysical->DoesBrokenHiddenComponentExist())
				{
					vfxAssertf(
						0,
						"%s: vehicle glass component for '%s' (id=%d) is smashed but no longer hidden",
						pPhysical->GetModelName(),
						pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName(),
						pComponent->m_componentId
					);
				}
				else
				{
					vfxAssertf(
						0,
						"%s: vehicle glass component for '%s' (id=%d) is smashed but broken/hidden component is gone",
						pPhysical->GetModelName(),
						pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName(),
						pComponent->m_componentId
					);
				}
			}
		}

		if (PARAM_vgasserts.Get()) // check matrix
		{
			const Mat34V matrix = pComponent->m_matrix;

			const ScalarV d00 = MagSquared(matrix.GetCol0());
			const ScalarV d11 = MagSquared(matrix.GetCol1());
			const ScalarV d22 = MagSquared(matrix.GetCol2());
			const ScalarV d01 = Dot(Normalize(matrix.GetCol0()), Normalize(matrix.GetCol1()));
			const ScalarV d12 = Dot(Normalize(matrix.GetCol1()), Normalize(matrix.GetCol2()));
			const ScalarV d20 = Dot(Normalize(matrix.GetCol2()), Normalize(matrix.GetCol0()));

			const float normErr = MaxElement(Abs(Vec3V(d00, d11, d22) - Vec3V(V_ONE))).Getf();
			const float skewErr = MaxElement(Abs(Vec3V(d01, d12, d20))).Getf();

			// suppress these asserts unless we specifically want them
			// see BS#647597 (matrix is zero)
			// see BS#649820 (matrix is garbage)
			// see BS#644218 (matrix has one column which is not perpendicular)
			if (normErr > 0.2f || skewErr > 0.2f)
			{
				vfxAssertf(
					0,
					"%s: vehicle glass matrix for '%s' is not orthonormal - [%f,%f,%f][%f,%f,%f][%f,%f,%f]",
					pPhysical->GetModelName(),
					pPhysical->GetDrawable()->GetSkeletonData()->GetBoneData(pComponent->m_boneIndex)->GetName(),
					VEC3V_ARGS(matrix.GetCol0()),
					VEC3V_ARGS(matrix.GetCol1()),
					VEC3V_ARGS(matrix.GetCol2())
				);
			}
		}
	}
}

#endif // __ASSERT

#if __BANK

void CVehicleGlassManager::UpdateDebug()
{
#if VEHICLE_GLASS_SMASH_TEST
	g_vehicleGlassSmashTest.UpdateDebug();
#endif // VEHICLE_GLASS_SMASH_TEST
}

void CVehicleGlassManager::RenderDebug() const
{
	m_numComponents = VEHICLE_GLASS_MAX_COMPONENTS - m_componentPool->GetNumItems();
	m_numTriangles  = VEHICLE_GLASS_MAX_TRIANGLES  - m_trianglePool ->GetNumItems();
	m_totalVBMemory = CVehicleGlassVB::GetMemoryUsed() / 1024;
	m_maxVBMemory = Max(m_maxVBMemory, m_totalVBMemory);

	if (m_renderDebug)
	{
		for (const CVehicleGlassComponent* pComponent = m_componentList.GetHead(); pComponent; pComponent = m_componentList.GetNext(pComponent))
		{
			pComponent->RenderComponentDebug();
		}

#if VEHICLE_GLASS_CLUMP_NOISE
		if (m_clumpNoiseData && m_clumpNoiseDebugDrawGraphs)
		{
			for (int bufferIndex = 0; bufferIndex < VEHICLE_GLASS_CLUMP_NOISE_BUFFER_COUNT; bufferIndex++)
			{
				for (int i = 0; i < m_clumpNoiseSize - 1; i++)
				{
					const float h0 = (float)m_clumpNoiseData[i + 0 + bufferIndex*m_clumpNoiseSize]/255.0f; // [0..1]
					const float h1 = (float)m_clumpNoiseData[i + 1 + bufferIndex*m_clumpNoiseSize]/255.0f; // [0..1]

					const float x0 = 0.1f + 0.8f*(float)(i + 0)/(float)(m_clumpNoiseSize - 1);
					const float x1 = 0.1f + 0.8f*(float)(i + 1)/(float)(m_clumpNoiseSize - 1);

					const float y0 = 0.1f + 0.6f*(float)(bufferIndex + 0)/(float)(VEHICLE_GLASS_CLUMP_NOISE_BUFFER_COUNT*2);
					const float y1 = 0.1f + 0.6f*(float)(bufferIndex + 1)/(float)(VEHICLE_GLASS_CLUMP_NOISE_BUFFER_COUNT*2);

					grcDebugDraw::Line(Vector2(x0, y1 + (y0 - y1)*h0*0.9f), Vector2(x1, y1 + (y0 - y1)*h1*0.9f), Color32(255,255,255,255));
					grcDebugDraw::Line(Vector2(x0, y1), Vector2(x1, y1), Color32(255,255,255,32));
				}
			}
		}
#endif // VEHICLE_GLASS_CLUMP_NOISE
	}

	if (m_smashWindowShowBoneMatrix)
	{
		const CEntity* pEntity = (const CEntity*)g_PickerManager.GetSelectedEntity();

		if (pEntity && pEntity->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
			const int boneIndex = pVehicle->GetBoneIndex((eHierarchyId)m_smashWindowHierarchyId);

			if (boneIndex != -1)
			{
				Mat34V matrix;
				CVfxHelper::GetMatrixFromBoneIndex_Smash(matrix, pVehicle, boneIndex);

				grcDebugDraw::Axis(matrix, 0.5f, true);

				const int componentId = pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneIndex); // NOTE -- this only gets the "root" component for this bone index

				if (componentId != -1)
				{
					const CVehicleGlassComponent* pComponent = const_cast<CVehicleGlassManager&>(*this).FindComponent(pVehicle, componentId);

					if (pComponent)
					{
						const Vec3V diff0 = Abs(matrix.GetCol0() - pComponent->m_matrix.GetCol0());
						const Vec3V diff1 = Abs(matrix.GetCol1() - pComponent->m_matrix.GetCol1());
						const Vec3V diff2 = Abs(matrix.GetCol2() - pComponent->m_matrix.GetCol2());
						const Vec3V diff3 = Abs(matrix.GetCol3() - pComponent->m_matrix.GetCol3());
						const Vec3V maxDiff = Max(diff0, diff1, diff2, diff3);

						if (MaxElement(maxDiff).Getf() > 0.01f)
						{
							grcDebugDraw::Axis(pComponent->m_matrix, 0.5f, true);
							grcDebugDraw::Sphere(pComponent->m_matrix.GetCol3(), 0.25f, Color32(255,0,0,255), false);
						}
					}
				}
			}
		}
	}
}

/*
useful rag stuff:

Physics/Physics debug tools/Measuring Tool/Turn on tool
rage - Profile Draw/RAGE pfDraw/Physics/Bounds/BoundIndices/ComponentIndices
*/

#if VEHICLE_GLASS_SMASH_TEST

PARAM(vgtest,"");

static Vec3V g_vehglassTestP[2];
static Vec3V g_vehglassTestD[2];

class CVehicleGlassTest // simple framework for testing vertex compression etc.
{
private:
	static void Update()
	{
		// NOTE -- this won't compile in non-unity builds, make sure VEHICLE_GLASS_SMASH_TEST is not checked in enabled!
		CVehicleGlassTriangle triangle;
		triangle.SetVertexP(0, g_vehglassTestP[0]);
		triangle.SetVertexD(0, g_vehglassTestD[0]);
		g_vehglassTestD[1] = triangle.GetDamagedVertexP(0) - g_vehglassTestP[0];
	}

public:
	static void AddWidgets(bkBank* pVfxBank)
	{
		pVfxBank->PushGroup("Test", false);
		{
			pVfxBank->AddVector("P[0]", &g_vehglassTestP[0], -100.0f, 100.0f, 1.0f/32.0f, Update);
			pVfxBank->AddVector("D[0]", &g_vehglassTestD[0], -0.5f, 0.5f, 1.0f/64.0f, Update);
			pVfxBank->AddSeparator();
			pVfxBank->AddVector("D[1]", &g_vehglassTestD[1], -100.0f, 100.0f, 1.0f/32.0f);
		}
		pVfxBank->PopGroup();
	}
};

#endif // VEHICLE_GLASS_SMASH_TEST

void CVehicleGlassManager::InitWidgets()
{
	const float componentPoolSize = ((float)(sizeof(CVehicleGlassComponent)*VEHICLE_GLASS_MAX_COMPONENTS))/1024.0f;
	const float trianglePoolSize = ((float)(sizeof(CVehicleGlassTriangle)*VEHICLE_GLASS_MAX_TRIANGLES))/1024.0f;

	const float collisionInfoPoolSize = ((float)(sizeof(VfxCollInfo_s)*VEHICLE_GLASS_MAX_COLLISIONS))/1024.0f;
	const float explosionInfoPoolSize = ((float)(sizeof(VfxExpInfo_s)*VEHICLE_GLASS_MAX_EXPLOSIONS))/1024.0f;

	const float vehicleGlassManagerSize = ((float)sizeof(CVehicleGlassManager))/1024.0f;

	Displayf("sizeof(CVehicleGlassComponent) = %" SIZETFMT "d bytes x %d = %.2fKB", sizeof(CVehicleGlassComponent), VEHICLE_GLASS_MAX_COMPONENTS, componentPoolSize);
	Displayf("sizeof(CVehicleGlassTriangle) = %" SIZETFMT "d bytes x %d = %.2fKB", sizeof(CVehicleGlassTriangle), VEHICLE_GLASS_MAX_TRIANGLES, trianglePoolSize);

	Displayf("sizeof(VfxCollInfo_s) = %" SIZETFMT "d bytes x %d = %.2fKB", sizeof(VfxCollInfo_s), VEHICLE_GLASS_MAX_COLLISIONS, collisionInfoPoolSize);
	Displayf("sizeof(VfxExpInfo_s) = %" SIZETFMT "d bytes x %d = %.2fKB", sizeof(VfxExpInfo_s), VEHICLE_GLASS_MAX_EXPLOSIONS, explosionInfoPoolSize);

	Displayf("sizeof(CVehicleGlassManager) = %.2fKB", vehicleGlassManagerSize);

#if __PS3
	Displayf("sizeof(edge vertex/index buffers) = %.2fKB", (float)(m_edgeVertexDataBufferSize + m_edgeIndexDataBufferSize)/1024.0f);
#endif // __PS3

#if VEHICLE_GLASS_SMASH_TEST
	Displayf(" - note: this memory usage will be much higher due to smash test");
#endif // VEHICLE_GLASS_SMASH_TEST

	bkBank* pVfxBank = vfxWidget::GetBank();

	pVfxBank->PushGroup("Smashable Vehicle Glass", false);
	{
#if VEHICLE_GLASS_SMASH_TEST
		if (PARAM_vgtest.Get())
		{
			CVehicleGlassTest::AddWidgets(pVfxBank);
		}
#endif // VEHICLE_GLASS_SMASH_TEST

		pVfxBank->AddSlider(atVarString("Num Components (%d - %.2fK)", VEHICLE_GLASS_MAX_COMPONENTS, componentPoolSize), &m_numComponents, 0, VEHICLE_GLASS_MAX_COMPONENTS, 0);
		pVfxBank->AddSlider(atVarString("Num Triangles (%d - %.2fK)", VEHICLE_GLASS_MAX_TRIANGLES, trianglePoolSize), &m_numTriangles, 0, VEHICLE_GLASS_MAX_TRIANGLES, 0);
		pVfxBank->AddSlider("Total VB memory (Kb)", &m_totalVBMemory, 0, 1000, -1);
		pVfxBank->AddSlider("Max VB memory (Kb)", &m_maxVBMemory, 0, 1000, -1);
		pVfxBank->AddToggle("Test MP memory cap", &m_testMPMemCap);
		pVfxBank->AddSeparator();

		pVfxBank->AddButton("Toggle Wireframe Mode", CRenderPhaseDebugOverlayInterface::ToggleVehicleGlassMode);
		pVfxBank->AddButton("Toggle Physics Materials", phMaterialDebug::ToggleRenderCarGlassMaterials);
		pVfxBank->AddButton("Toggle Non-Smashable Glass Physics Materials", phMaterialDebug::ToggleRenderNonCarGlassMaterials);

		pVfxBank->PushGroup("Alpha Bucket Sorting");
		pVfxBank->AddToggle("Draw Vehicle Glass In Alpha Bucket", &m_RenderVehicleGlassAlphaBucket);
		pVfxBank->AddSlider("Sorted Alpha Cam Dir Offset", &g_VehicleGlassSortedCamDirOffset, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddToggle("Debug Vehicle Glass Alpha Sorting", &m_RenderDebugCentroid);
		pVfxBank->PopGroup();

		class UpdateTessellation_cb { public: static void func()
		{
#if VEHICLE_GLASS_CLUMP_NOISE
			g_vehicleGlassMan.GenerateLowDiscrepancyClumpNoise();
#endif // VEHICLE_GLASS_CLUMP_NOISE

			if (g_vehicleGlassMan.m_smashWindowUpdateTessellationAll)
			{
				for (CVehicleGlassComponent* pComponent = g_vehicleGlassMan.m_componentList.GetHead(); pComponent; pComponent = g_vehicleGlassMan.m_componentList.GetNext(pComponent))
				{
					pComponent->m_updateTessellation = true;
				}
			}
			else if (g_vehicleGlassMan.m_smashWindowUpdateTessellation)
			{
				CVehicle* pVehicle = GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					for (CVehicleGlassComponent* pComponent = g_vehicleGlassMan.m_componentList.GetHead(); pComponent; pComponent = g_vehicleGlassMan.m_componentList.GetNext(pComponent))
					{
						if (pComponent->m_regdPhy.Get() == pVehicle)
						{
							pComponent->m_updateTessellation = true;
						}
					}
				}
			}
		}};

#if VEHICLE_GLASS_CLUMP_NOISE
		static bool s_clumpNoiseTest = false;

		class UpdateClumpNoiseTest_cb { public: static void func()
		{
			if (s_clumpNoiseTest)
			{
				g_vehicleGlassMan.m_distanceFieldThresholdMin = -0.02f;
				g_vehicleGlassMan.m_distanceFieldThresholdMax = +0.08f;
				g_vehicleGlassMan.m_clumpNoiseEnabled = true;

				if (g_vehicleGlassMan.m_clumpNoiseData == NULL)
				{
					g_vehicleGlassMan.GenerateLowDiscrepancyClumpNoise();
				}
			}
			else // restore
			{
				g_vehicleGlassMan.m_distanceFieldThresholdMin = VEHICLE_GLASS_DEFAULT_DISTANCE_THRESHOLD_TO_DETACH_MIN;
				g_vehicleGlassMan.m_distanceFieldThresholdMax = VEHICLE_GLASS_DEFAULT_DISTANCE_THRESHOLD_TO_DETACH_MAX;
				g_vehicleGlassMan.m_clumpNoiseEnabled = false;
			}

			UpdateTessellation_cb::func();
		}};

		pVfxBank->AddToggle("CLUMP NOISE TEST", &s_clumpNoiseTest, UpdateClumpNoiseTest_cb::func);
#endif // VEHICLE_GLASS_CLUMP_NOISE

		class UpdateSmashSphere_cb { public: static void func()
		{
			if (g_vehicleGlassMan.m_smashWindowUpdateTessellation)
			{
				CVehicleGlassComponent* pComponent = GetSmashWindowTargetComponent();

				if (pComponent)
				{
					pComponent->m_updateTessellation = true;
					pComponent->m_smashSphere = g_vehicleGlassMan.m_smashWindowCurrentSmashSphere;
				}
			}
		}};

		pVfxBank->PushGroup("Smash Window", false);
		{
			class RollWindowUp_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					pVehicle->RollWindowUp((eHierarchyId)g_vehicleGlassMan.m_smashWindowHierarchyId);
				}
			}};

			class RollWindowDown_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					pVehicle->RolldownWindow((eHierarchyId)g_vehicleGlassMan.m_smashWindowHierarchyId);
				}
			}};

			class SmashWindow_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					pVehicle->SmashWindow(
						(eHierarchyId)g_vehicleGlassMan.m_smashWindowHierarchyId,
						g_vehicleGlassMan.m_smashWindowForce,
						true, // ignore cracks
						g_vehicleGlassMan.m_smashWindowDetachAll
					);
				}
			}};

			class FixWindow_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					pVehicle->FixWindow((eHierarchyId)g_vehicleGlassMan.m_smashWindowHierarchyId);
				}
			}};

			class FixVehicle_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					pVehicle->Fix(true);
				}
			}};

			class DeleteWindow_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					pVehicle->DeleteWindow((eHierarchyId)g_vehicleGlassMan.m_smashWindowHierarchyId);
				}
			}};

			class SmashVehicleGlass_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					static float radius = 2.0f;
					g_vehicleGlassMan.SmashExplosion(pVehicle, pVehicle->GetTransform().GetPosition(), radius, VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW);
				}
			}};

			class DamageVehicle_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					u32 damageLevel[6] = {3,3,3,3,3,3};
					pVehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformationsFromNetwork(damageLevel);
				}
			}};

#if __DEV
			class VehicleGlassCrackTest_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					const int boneIndex = pVehicle->GetBoneIndex((eHierarchyId)g_vehicleGlassMan.m_smashWindowHierarchyId);
					const int componentId = pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneIndex); // NOTE -- this only gets the "root" component for this bone index

					if (componentId != -1)
					{
						VfxCollInfo_s info;

						info.regdEnt       = pVehicle;
						info.vPositionWld  = Vec3V(V_FLT_MAX); // force smash
						info.vNormalWld    = Vec3V(V_ZERO); // [CRACK TEST]
						info.vDirectionWld = Vec3V(V_ZERO); // [CRACK TEST]
						info.materialId    = PGTAMATERIALMGR->g_idCarGlassWeak;
						info.componentId   = componentId;
						info.weaponGroup   = WEAPON_EFFECT_GROUP_PUNCH_KICK;
						info.force         = 1.0f; // [CRACK TEST]
						info.isBloody      = false;
						info.isWater       = false;
						info.isExitFx      = false;
						info.noPtFx		   = false;
						info.noPedDamage   = false;
						info.noDecal	   = false; 
						info.isSnowball	   = false;
						info.isFMJAmmo	   = false;

						g_vehicleGlassMan.StoreCollision(info, false);
					}
				}
			}};
#endif // __DEV

			class ListVehicleBoneNames_button { public: static void func()
			{
				CVehicle* pVehicle = CVehicleGlassManager::GetSmashWindowTargetVehicle();

				if (pVehicle)
				{
					fragInst* pFragInst = pVehicle->GetFragInst();

					if (AssertVerify(pFragInst && pFragInst == pVehicle->GetFragInst()))
					{
						const fwVehicleGlassWindowData* pWindowData = static_cast<const gtaFragType*>(pFragInst->GetType())->m_vehicleWindowData;
						const phBound* pBound = pFragInst->GetArchetype()->GetBound();

						if (AssertVerify(pBound && pBound->GetType() == phBound::COMPOSITE))
						{
							const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);
							const int numBones = pVehicle->GetDrawable()->GetSkeletonData()->GetNumBones();
							const int numComponents = pBoundComposite->GetNumBounds();

							const int componentFlagsSize = (numComponents + 7)/8;
							u8* componentFlags = rage_new u8[componentFlagsSize];
							sysMemSet(componentFlags, 0, componentFlagsSize*sizeof(u8));

							Displayf("%s has %d bones and %d components:", pVehicle->GetModelName(), numBones, numComponents);

							class ArrayToString { public: static atString func(const atArray<int>& arr)
							{
								char str[1024] = "";

								for (int i = 0; i < arr.GetCount(); i++)
								{
									if (i == 0)
									{
										strcat(str, ",");
									}

									strcat(str, atVarString("%d", arr[i]).c_str());
								}

								return atString(str);
							}};

							for (int boneIndex = 0; boneIndex < numBones; boneIndex++)
							{
								const char* boneName = pVehicle->GetDrawable()->GetSkeletonData()->GetBoneData(boneIndex)->GetName();
								const int boneGroupId = pFragInst->GetGroupFromBoneIndex(boneIndex);

								atArray<int> hierarchyIds;
								char hierarchyIdsStr[1024] = "";

								for (int hierarchyId = 0; hierarchyId < VEH_NUM_NODES; hierarchyId++)
								{
									if (boneIndex == pVehicle->GetBoneIndex((eHierarchyId)hierarchyId))
									{
										hierarchyIds.PushAndGrow(hierarchyId);

										if (hierarchyIdsStr[0] != '\0')
										{
											strcat(hierarchyIdsStr, ",");
										}

										extern const char* GetVehicleHierarchyIdName(int hierarchyId);
										strcat(hierarchyIdsStr, GetVehicleHierarchyIdName(hierarchyId));
									}
								}

								if (boneGroupId != -1)
								{
									fragPhysicsLOD* pFragPhysicsLOD = pFragInst->GetTypePhysics();
									if (pFragPhysicsLOD)
									{
										const fragTypeGroup* pGroup = pFragPhysicsLOD->GetGroup(boneGroupId);

										if (pGroup->GetNumChildren() > 0)
										{
											const int firstComponentId = pGroup->GetChildFragmentIndex();
											const int lastComponentId = pGroup->GetChildFragmentIndex() + pGroup->GetNumChildren() - 1;

											char materialListStr[512] = "";
											int numWindowProxies = 0;

											const char* brokenHiddenStr = "";

											for (int componentId = firstComponentId; componentId <= lastComponentId; componentId++)
											{
												const phBound* pBoundChild = static_cast<const phBoundComposite*>(pBound)->GetBound(componentId);

												if (pBoundChild)
												{
													for (int i = 0; i < pBoundChild->GetNumMaterials(); i++)
													{
														const phMaterialMgr::Id mtlId = PGTAMATERIALMGR->UnpackMtlId(pBoundChild->GetMaterialId(i));
														char mtlIdName[64] = "";
														PGTAMATERIALMGR->GetMaterialName(mtlId, mtlIdName, sizeof(mtlIdName));

														if (materialListStr[0] != '\0')
														{
															strcat(materialListStr, ",");
														}

														strcat(materialListStr, mtlIdName);
													}
												}

												if (pWindowData)
												{
													const fwVehicleGlassWindow* pWindow = fwVehicleGlassWindowData::GetWindow(pWindowData, componentId);

													if (pWindow)
													{
														numWindowProxies++;
													}
												}

												if      (pVehicle->IsHiddenFlagSet(componentId)) { brokenHiddenStr = " - hidden"; }
												else if (pVehicle->IsBrokenFlagSet(componentId)) { brokenHiddenStr = " - broken"; }
											}

											if (strchr(materialListStr, ','))
											{
												strcpy(materialListStr, atVarString(", materials [%s]", materialListStr).c_str());
											}
											else
											{
												strcpy(materialListStr, atVarString(", material '%s'", materialListStr).c_str());
											}

											char windowProxyStr[64] = "";

											if (numWindowProxies > 0)
											{
												sprintf(windowProxyStr, ", %d window proxies", numWindowProxies);
											}

											if (firstComponentId < lastComponentId)
											{
												Displayf("  %d. '%s' [%s] (group=%d) maps to components [%d..%d]%s%s", boneIndex, boneName, hierarchyIdsStr, boneGroupId, firstComponentId, lastComponentId, materialListStr, windowProxyStr);
											}
											else
											{
												Displayf("  %d. '%s' [%s] (group=%d) maps to component %d%s%s%s", boneIndex, boneName, hierarchyIdsStr, boneGroupId, firstComponentId, materialListStr, windowProxyStr, brokenHiddenStr);
											}

											for (int componentId = firstComponentId; componentId <= lastComponentId; componentId++)
											{
												componentFlags[componentId/8] |= BIT(componentId%8);
											}
										}
									}
								}
								else
								{
									Displayf("  %d. '%s' [%s] does not map to any component", boneIndex, boneName, hierarchyIdsStr);
								}
							}

							atArray<int> componentsWithNoBones;

							for (int componentId = 0; componentId < numComponents; componentId++)
							{
								if ((componentFlags[componentId/8] & BIT(componentId%8)) == 0)
								{
									componentsWithNoBones.PushAndGrow(componentId);
								}
							}

							if (componentsWithNoBones.GetCount() > 0)
							{
								Displayf("  components with no bones: [%s]", ArrayToString::func(componentsWithNoBones).c_str());
							}

							if (pWindowData == NULL)
							{
								Displayf("  no window proxy data");
							}

							delete[] componentFlags;
						}
					}
				}
			}};

			extern const char** GetVehicleHierarchyIdNames();
			pVfxBank->AddCombo("Window Hierarchy Id", &m_smashWindowHierarchyId, VEH_NUM_NODES, GetVehicleHierarchyIdNames());
			pVfxBank->AddButton("Roll Window Up", RollWindowUp_button::func);
			pVfxBank->AddButton("Roll Window Down", RollWindowDown_button::func);
			pVfxBank->AddButton("Smash Window", SmashWindow_button::func);
			pVfxBank->AddSlider("Smash Window Force", &m_smashWindowForce, 0.0f, 1.0f, 1.0f/64.0f);
			pVfxBank->AddToggle("Smash Window Detach All", &m_smashWindowDetachAll);
			pVfxBank->AddToggle("Smash Window Show Bone Matrix", &m_smashWindowShowBoneMatrix);
			pVfxBank->AddToggle("Update Tessellation", &m_smashWindowUpdateTessellation, UpdateTessellation_cb::func);
			pVfxBank->AddToggle("Update Tessellation All", &m_smashWindowUpdateTessellationAll, UpdateTessellation_cb::func);
			pVfxBank->AddSlider("Update Random Seed", &m_smashWindowUpdateRandomSeed, 0, 0xffffffffUL, 1, UpdateTessellation_cb::func);
			pVfxBank->AddVector("Smash Sphere Centre", (Vec3V*)&m_smashWindowCurrentSmashSphere, -50.0f, 50.0f, 1.0f/32.0f, UpdateSmashSphere_cb::func);
			pVfxBank->AddSlider("Smash Sphere Radius", (float*)&m_smashWindowCurrentSmashSphere + 3, 0.0f, 1000.0f, 1.0f/32.0f, UpdateSmashSphere_cb::func);
			pVfxBank->AddButton("Fix Window", FixWindow_button::func);
			pVfxBank->AddButton("Fix Vehicle", FixVehicle_button::func);
			pVfxBank->AddButton("Delete Window", DeleteWindow_button::func);
			pVfxBank->AddButton("Smash Vehicle Glass", SmashVehicleGlass_button::func);
			pVfxBank->AddButton("Damage Vehicle", DamageVehicle_button::func);
#if __DEV
			pVfxBank->AddButton("Crack Test", VehicleGlassCrackTest_button::func);
#endif // __DEV
			pVfxBank->AddButton("List Vehicle Bone Names", ListVehicleBoneNames_button::func);
		}
		pVfxBank->PopGroup();

#if VEHICLE_GLASS_SMASH_TEST
		g_vehicleGlassSmashTest.InitWidgets(pVfxBank);
#endif // VEHICLE_GLASS_SMASH_TEST

#if VEHICLE_GLASS_DEV
		fwVehicleGlassConstructorInterface::AddWidgets(*pVfxBank);
#endif // VEHICLE_GLASS_DEV

#if VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT
		pVfxBank->AddToggle("Verbose", &m_verbose);
		pVfxBank->AddToggle("Verbose Hit", &m_verboseHit);
#endif // VEHICLE_GLASS_VERBOSE_DEBUG_OUTPUT

		class ResetInfo_button { public: static void func()
		{
			g_vehicleGlassMan.m_maxActiveComponents              = 0;
			g_vehicleGlassMan.m_maxAttachedTriangles             = 0;
			g_vehicleGlassMan.m_maxDetachedTriangles             = 0;
			g_vehicleGlassMan.m_maxAttachedTrianglesPerComponent = 0;
			g_vehicleGlassMan.m_maxDetachedTrianglesPerComponent = 0;
		}};

		pVfxBank->AddToggle("Show Info", &m_showInfo);
		pVfxBank->AddButton("Reset Info", ResetInfo_button::func);

#if !VEHICLE_GLASS_COMPRESSED_VERTICES
		pVfxBank->AddSlider("Test Normal Quant Bits", &m_testNormalQuantisationBits, 4, 32, 1);
#endif // !VEHICLE_GLASS_COMPRESSED_VERTICES
		pVfxBank->AddToggle("Use HD Drawable", &m_useHDDrawable);
#if USE_EDGE
		pVfxBank->AddToggle("Invalidate Geom Cache", &m_bInvalideateGeomCache);
		pVfxBank->AddToggle("Reuse Extracted EDGE Geom", &m_bReuseExtractedGeom);
		pVfxBank->AddToggle("Extracted EDGE Geom Early", &m_bExtractGeomEarly);
#endif // USE_EDGE
		pVfxBank->AddToggle("Force Visible", &m_forceVisible);
		pVfxBank->AddToggle("Use Static VB", &m_useStaticVB);
		pVfxBank->AddToggle("Sort", &m_sort);
		pVfxBank->AddToggle("Sort Reverse", &m_sortReverse);
		pVfxBank->AddToggle("Disable Vehicle Glass", &m_disableVehicleGlass);
		pVfxBank->AddToggle("Disable Render", &m_disableRender);
		pVfxBank->AddToggle("Disable Front Face Render", &m_disableFrontFaceRender);
		pVfxBank->AddToggle("Disable Back Face Render", &m_disableBackFaceRender);
		pVfxBank->AddToggle("Disable Pos Update", &m_disablePosUpdate);
		pVfxBank->AddToggle("Disable Spin Axis", &m_disableSpinAxis);
		pVfxBank->AddToggle("Disable Can Detach Test", &m_disableCanDetachTest);
		pVfxBank->AddToggle("Disable Detaching", &m_disableDetaching);
		pVfxBank->AddToggle("Ignore Remove All Glass", &m_ignoreRemoveAllGlass);
		pVfxBank->AddToggle("Detach All On Explosion", &m_detachAllOnExplosion);
#if VEHICLE_GLASS_RANDOM_DETACH
		pVfxBank->AddToggle("Random Detach Enabled", &m_randomDetachEnabled, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Random Detach Threshold", &m_randomDetachThreshold, 0.0f, 1.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
#endif // VEHICLE_GLASS_RANDOM_DETACH
		pVfxBank->AddToggle("Tessellation Enabled", &m_tessellationEnabled, UpdateTessellation_cb::func);
		pVfxBank->AddToggle("Tessellation Sphere Test", &m_tessellationSphereTest, UpdateTessellation_cb::func);
		pVfxBank->AddToggle("Tessellation Normalise", &m_tessellationNormalise, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Tess Scale Min", &m_tessellationScaleMin, 0.0f, 8.0f, 1.0f/512.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Tess Scale Max", &m_tessellationScaleMax, 0.0f, 8.0f, 1.0f/512.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Tess Spread Min", &m_tessellationSpreadMin, 0.0f, 2.0f, 1.0f/512.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Tess Spread Max", &m_tessellationSpreadMax, 0.0f, 2.0f, 1.0f/512.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Break Range", &m_vehicleGlassBreakRange, 0.01f, sqrtf(VEHICLE_GLASS_BREAK_RANGE_SQR), 0.01f);
		pVfxBank->AddSlider("Detach Range", &m_vehicleGlassDetachRange, 0.01f, sqrtf(VEHICLE_GLASS_DETACH_RANGE_SQR), 0.01f);
#if VEHICLE_GLASS_DEV
		const char* sourceStrings[] =
		{
			"none",
			"debug storage window proxy",
			"debug storage window data",
			"vehicle model window data",
		};
		CompileTimeAssert(NELEM(sourceStrings) == VGW_SOURCE_COUNT);

		pVfxBank->AddCombo ("Distance Field Source", &m_distanceFieldSource, NELEM(sourceStrings), sourceStrings);
		pVfxBank->AddToggle("Distance Field Uncompressed", &m_distanceFieldUncompressed);
		pVfxBank->AddToggle("Distance Field Output Files (slow)", &m_distanceFieldOutputFiles);
		pVfxBank->AddButton("Distance Field Release All Window Proxies", fwVehicleGlassConstructorInterface::ReleaseAllWindowProxies);
		pVfxBank->AddButton("Distance Field Release All Window Data", fwVehicleGlassConstructorInterface::ReleaseAllWindowData);
#endif // VEHICLE_GLASS_DEV

		const char* numVertsStrings[] =
		{
			"VEHICLE_GLASS_TEST_1_VERTEX",
			"VEHICLE_GLASS_TEST_2_VERTICES",
			"VEHICLE_GLASS_TEST_3_VERTICES",
			"VEHICLE_GLASS_TEST_CENTROID",
			"VEHICLE_GLASS_TEST_TRIANGLE",
		};
		CompileTimeAssert(NELEM(numVertsStrings) == CVehicleGlassComponent::VEHICLE_GLASS_TEST_COUNT);

		pVfxBank->AddCombo ("Distance Field Num Verts", &m_distanceFieldNumVerts, NELEM(numVertsStrings) - 1, numVertsStrings, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Distance Field Thresh Min", &m_distanceFieldThresholdMin, -0.1f, 1.0f, 1.0f/256.0f, UpdateTessellation_cb::func);
#if VEHICLE_GLASS_CLUMP_NOISE
		pVfxBank->AddSlider("Distance Field Thresh Max", &m_distanceFieldThresholdMax, -0.1f, 1.0f, 1.0f/256.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Distance Field Thresh Phase", &m_distanceFieldThresholdPhase, 0.0f, 1.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Distance Field Thresh Period", &m_distanceFieldThresholdPeriod, 1.0f, 16.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
		pVfxBank->AddToggle("Clump Noise Enabled", &m_clumpNoiseEnabled, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Noise Size", &m_clumpNoiseSize, 8, 512, 1, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Noise Buckets", &m_clumpNoiseBuckets, 1, 32, 1, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Noise Bucket Isolate", &m_clumpNoiseBucketIsolate, -1, 31, 1, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Noise Global Offset", &m_clumpNoiseGlobalOffset, -256, 256, 1, UpdateTessellation_cb::func);
		pVfxBank->AddToggle("Clump Noise Debug Draw Graphs", &m_clumpNoiseDebugDrawGraphs);
		pVfxBank->AddToggle("Clump Noise Debug Draw Angle", &m_clumpNoiseDebugDrawAngle);
		pVfxBank->AddSlider("Clump Spread", &m_clumpSpread, 0.0f, 32.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Exponent Bias", &m_clumpExponentBias, -4.0f, 4.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Radius Min", &m_clumpRadiusMin, 0.0f, 4.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Radius Max", &m_clumpRadiusMax, 0.0f, 4.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Height Min", &m_clumpHeightMin, 0.0f, 4.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
		pVfxBank->AddSlider("Clump Height Max", &m_clumpHeightMax, 0.0f, 4.0f, 1.0f/32.0f, UpdateTessellation_cb::func);
#endif // VEHICLE_GLASS_CLUMP_NOISE
		pVfxBank->AddCombo ("Sphere Test Num Verts", &m_sphereTestDetachNumVerts, NELEM(numVertsStrings), numVertsStrings, UpdateTessellation_cb::func);
		pVfxBank->AddToggle("Skip Sphere Test On Detach", &m_skipSphereTestOnDetach);
		pVfxBank->AddToggle("Skip Distance Field Test On Extract", &m_skipDistanceFieldTestOnExtract);
		pVfxBank->AddSlider("Crack Texture Amount", &m_crackTextureAmount, 0.0f, 32.0f, 1.0f/32.0f);
		pVfxBank->AddSlider("Crack Texture Scale", &m_crackTextureScale, 0.0f, 256.0f, 1.0f/32.0f);
		pVfxBank->AddSlider("Crack Texture Scale FP", &m_crackTextureScaleFP, 0.0f, 256.0f, 1.0f/32.0f);
		pVfxBank->AddSlider("Crack Texture Bump Amount", &m_crackTextureBumpAmount, 0.0f, 2.0f, 1.0f/32.0f);
		pVfxBank->AddSlider("Crack Texture Bumpiness", &m_crackTextureBumpiness, 0.0f, 8.0f, 1.0f/32.0f);
		pVfxBank->AddColor ("Crack Tint", &m_crackTint);
		pVfxBank->AddSlider("Falling Pieces Tint Scale", &m_fallingTintScale, 1.0f, 50.0f, 1.0f);
		pVfxBank->AddToggle("Allow Find Geometry Without Window", &m_allowFindGeometryWithoutWindow);
		pVfxBank->AddToggle("Impact Effect Particle", &m_impactEffectParticle);
		pVfxBank->AddToggle("Ground Effect Audio", &m_groundEffectAudio);
		pVfxBank->AddToggle("Ground Effect Particle", &m_groundEffectParticle);
		pVfxBank->AddToggle("All Glass Is Weak", &m_allGlassIsWeak);
		pVfxBank->AddToggle("Only Apply Decals", &m_onlyApplyDecals);
		pVfxBank->AddToggle("Never Apply Decals", &m_neverApplyDecals);
		pVfxBank->AddToggle("Defer Hits", &m_deferHits);
#if !USE_GPU_VEHICLEDEFORMATION
		pVfxBank->AddToggle("Apply Vehicle Damage On Update", &m_applyVehicleDamageOnUpdate);
		pVfxBank->AddToggle("Force Vehicle Damage Update", &m_forceVehicleDamageUpdate);
#endif // !USE_GPU_VEHICLEDEFORMATION
		pVfxBank->AddToggle("Smash Windows When Convertible Roof Anim Plays", &m_smashWindowsWhenConvertibleRoofAnimPlays);
		pVfxBank->AddToggle("Render Debug", &m_renderDebug);
		pVfxBank->AddToggle("Render Debug Errors Only", &m_renderDebugErrorsOnly);
		pVfxBank->AddToggle("Render Debug Triangle Normals", &m_renderDebugTriangleNormals);
		pVfxBank->AddToggle("Render Debug Vert Normals", &m_renderDebugVertNormals);
		pVfxBank->AddToggle("Render Debug Exposed Vert", &m_renderDebugExposedVert);
		pVfxBank->AddToggle("Render Debug Smash Sphere", &m_renderDebugSmashSphere);
		pVfxBank->AddToggle("Render Debug Explosion Smash Spheres History", &m_renderDebugExplosionSmashSphereHistory);
		pVfxBank->AddToggle("Render Debug Offset Triangle", &m_renderDebugOffsetTriangle);
		pVfxBank->AddSlider("Render Debug Offset Spread", &m_renderDebugOffsetSpread, -2.0f, 2.0f, 1.0f/128.0f);
		pVfxBank->AddSlider("Render Debug Field Opacity", &m_renderDebugFieldOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		pVfxBank->AddSlider("Render Debug Field Offset", &m_renderDebugFieldOffset, 0.0f, 1.0f, 1.0f/128.0f);
		pVfxBank->AddToggle("Render Debug Field Colours", &m_renderDebugFieldColours);
		pVfxBank->AddToggle("Render Debug Tri Count", &m_renderDebugTriCount);
		pVfxBank->AddToggle("Render Debug Ground Plane Normal", &m_renderGroundPlane);
		pVfxBank->AddSlider("Time Scale", &m_timeScale, 0.0f, 4.0f, 1.0f/32.0f);

#if __DEV
		pVfxBank->PushGroup("Settings", false);
		{
			pVfxBank->AddSlider("Triangle Gravity Scale", &VEHICLEGLASSPOLY_GRAVITY_SCALE, -1.0f, 1.0f, 1.0f/32.0f, NullCB, "Scales the effect of gravity to simulate air resistance");
			pVfxBank->AddSlider("Triangle Rotation Speed", &VEHICLEGLASSPOLY_ROTATION_SPEED, 0.0f, 100.0f, 0.25f, NullCB, "How quickly the smashed triangles rotate");
			pVfxBank->AddSlider("Velocity Mult - Collision", &VEHICLEGLASSPOLY_VEL_MULT_COLN, -50.0f, 50.0f, 0.5f, NullCB, "Multiplier for the smashed triangle's initial velocity (from a collision)");
			pVfxBank->AddSlider("Velocity Mult - Explosion", &VEHICLEGLASSPOLY_VEL_MULT_EXPL, 0.0f, 20.0f, 0.25f, NullCB, "Multiplier for the smashed triangle's initial velocity (from an explosion)");
			pVfxBank->AddSlider("Velocity Rand - Collision", &VEHICLEGLASSPOLY_VEL_RAND_COLN, 0.0f, 10.0f, 0.25f, NullCB, "Randomness scale for the smashed triangle's initial velocity (from a collision)");
			pVfxBank->AddSlider("Velocity Rand - Explosion", &VEHICLEGLASSPOLY_VEL_RAND_EXPL, 0.0f, 10.0f, 0.25f, NullCB, "Randomness scale for the smashed triangle's initial velocity (from an explosion)");
			pVfxBank->AddSlider("Velocity Dampening", &VEHICLEGLASSPOLY_VEL_DAMP, 0.0f, 1.0f,0.01f);
			pVfxBank->AddSlider("Group Radius Mult", &VEHICLEGLASSGROUP_RADIUS_MULT, 0.0f, 20.0f, 0.5f, NullCB, "Multiplier for the radius of the smash");
			pVfxBank->AddSlider("Exposed Edges Smash Radius", &VEHICLEGLASSGROUP_EXPOSED_SMASH_RADIUS, 0.0f, 10.0f, 0.001f);

			pVfxBank->AddSlider("Force - Weapon Impact", &VEHICLEGLASSFORCE_WEAPON_IMPACT, 0.0f, 1.0f, 0.05f, NullCB, "The force that a weapon impact passes to the smash");
			pVfxBank->AddSlider("Force - Siren Smash", &VEHICLEGLASSFORCE_SIREN_SMASH, 0.0f, 1.0f, 0.05f, NullCB, "The force that a siren destroying passes to the smash");
			pVfxBank->AddSlider("Force - Kick Elbow Window", &VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW, 0.0f, 1.0f, 0.05f, NullCB, "The force that a kick or elbow passes to the smash");
			pVfxBank->AddSlider("Force - Window Deform", &VEHICLEGLASSFORCE_WINDOW_DEFORM, 0.0f, 1.0f, 0.05f, NullCB, "The force that a deformed window passes to the smash");
			pVfxBank->AddSlider("Force - No Texture", &VEHICLEGLASSFORCE_NO_TEXTURE_THRESH, 0.0f, 1.0f, 0.05f, NullCB, "Force value that causes no textures to be applied if it is exceeded");

			pVfxBank->AddSlider("Physical Coln Impulse - Min Threshold", &VEHICLEGLASSTHRESH_PHYSICAL_COLN_MIN_IMPULSE, 0.0f, 100000.0f, 250.0f, NullCB, "The collision impulse at which a minimum (0.0) force is produced");
			pVfxBank->AddSlider("Physical Coln Impulse - Max Threshold", &VEHICLEGLASSTHRESH_PHYSICAL_COLN_MAX_IMPULSE, 0.0f, 100000.0f, 250.0f, NullCB, "The collision impulse at which a maximum (1.0) force is produced");

			pVfxBank->AddSlider("Vehicle Speed - Min Threshold", &VEHICLEGLASSTHRESH_VEH_SPEED_MIN, 0.0f, 100.0f, 2.0f, NullCB, "The vehicle speed at which a minimum (0.0) force is produced");
			pVfxBank->AddSlider("Vehicle Speed - Max Threshold", &VEHICLEGLASSTHRESH_VEH_SPEED_MAX, 0.0f, 100.0f, 2.0f, NullCB, "The vehicle speed at which a maximum (1.0) force is produced");

			pVfxBank->AddSlider("Tessellation Scale - Force Smash", &VEHICLEGLASSTESSELLATIONSCALE_FORCE_SMASH, 1.0f, 20.0f, 1.0f/8.0f, NullCB, "Tessellation scale applied to force-smash events");
			pVfxBank->AddSlider("Tessellation Scale - Explosion", &VEHICLEGLASSTESSELLATIONSCALE_EXPLOSION, 1.0f, 20.0f, 1.0f/8.0f, NullCB, "Tessellation scale applied to explosions");
		}
		pVfxBank->PopGroup();
#endif // __DEV
	}
	pVfxBank->PopGroup();
}

bool CVehicleGlassManager::GetIsAllGlassWeak() const
{
#if VEHICLE_GLASS_SMASH_TEST
	if (g_vehicleGlassSmashTest.m_smashTest && // smash test forces all glass to be weak, so we can verify that it could smash properly if we forced it to
		g_vehicleGlassSmashTest.m_smashTestAllGlassIsWeak)
	{
		return true;
	}
#endif // VEHICLE_GLASS_SMASH_TEST

	return m_allGlassIsWeak;
}

__COMMENT(static) CVehicle* CVehicleGlassManager::GetSmashWindowTargetVehicle()
{
	CEntity* pEntity = (CEntity*)g_PickerManager.GetSelectedEntity();

	if (pEntity && pEntity->GetIsTypeVehicle())
	{
		return static_cast<CVehicle*>(pEntity);
	}

	CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();

	if (pPlayerInfo)
	{
		return pPlayerInfo->GetPlayerPed()->GetMyVehicle();
	}

	return NULL;
}

__COMMENT(static) CVehicleGlassComponent* CVehicleGlassManager::GetSmashWindowTargetComponent()
{
	CVehicle* pVehicle = GetSmashWindowTargetVehicle();

	if (pVehicle)
	{
		const int boneIndex = pVehicle->GetBoneIndex((eHierarchyId)g_vehicleGlassMan.m_smashWindowHierarchyId);

		if (boneIndex != -1)
		{
			const int rootComponentId = pVehicle->GetFragInst()->GetComponentFromBoneIndex(boneIndex);

			if (rootComponentId != -1)
			{
				return g_vehicleGlassMan.FindComponent(pVehicle, rootComponentId);
			}
		}
	}

	return NULL;
}

#endif // __BANK
