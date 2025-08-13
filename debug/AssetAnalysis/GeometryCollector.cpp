// =========================================
// debug/AssetAnalysis/GeometryCollector.cpp
// (c) 2013 RockstarNorth
// =========================================

#include "file/stream.h"

#include "debug/AssetAnalysis/GeometryCollector.h"

#if __BANK

/*
TODO
- use CDumpGeometryToOBJ
- use GeometryUtil to get vertex and triangle data
- hook up something like siLog to catch errors etc. which come from this system or streaming iterators itself, and save this along with the geometry
*/

#include "atl/array.h"
#include "atl/map.h"
#include "bank/bank.h"
#include "file/stream.h"
#include "grcore/edgeExtractgeomspu.h"
#include "grcore/indexbuffer.h"
#include "grcore/vertexbuffer.h"
#include "grcore/vertexbuffereditor.h"
#include "grmodel/shadergroup.h"
#include "rmcore/lodgroup.h"
#include "string/string.h"
#include "string/stringutil.h"
#include "system/memory.h"
#include "vectormath/classes.h"

#include "fragment/drawable.h"
#include "fragment/type.h"

#include "fwscene/mapdata/mapdata.h"
#include "fwscene/mapdata/mapdatacontents.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwutil/xmacro.h"
#include "timecycle/tcbox.h"
#include "timecycle/tcmodifier.h"

#include "debug/AssetAnalysis/AssetAnalysisUtil.h"
#include "debug/AssetAnalysis/GeometryCollector.h"
#include "debug/DebugArchetypeProxy.h"
#include "debug/DebugGeometryUtil.h"
#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/MloModelInfo.h"
#include "physics/gtaArchetype.h"
#include "renderer/GtaDrawable.h"
#include "scene/Entity.h"
#include "scene/world/GameWorldHeightMap.h"
#include "timecycle/TimeCycle.h"

namespace GeomCollector {

#define ENTITY_LOD_TEST 0

#if ENTITY_LOD_TEST
const float g_entitylodmax = 200.0f;
const int g_entitylodbucketcount = 64;
static int g_entitylodbuckets[g_entitylodbucketcount + 1] = {-1};
#endif // ENTITY_LOD_TEST

enum
{
	COLLECT_WATER_GEOMETRY             = BIT(0),
	COLLECT_WATER_FAR_CLIP_TCMODIFIERS = BIT(1),
	COLLECT_ALL_TCMODIFIERS            = BIT(2),
	COLLECT_MIRROR_GEOMETRY            = BIT(3),
	COLLECT_CABLE_GEOMETRY             = BIT(4),
	COLLECT_TREE_GEOMETRY              = BIT(5),
	COLLECT_PORTALS                    = BIT(6),
	COLLECT_LIGHTS                     = BIT(7),
	COLLECT_PROPGROUP_GEOMETRY         = BIT(8),
	COLLECT_ANIMATED_BUILDINGS_BOUNDS  = BIT(9),
};

static bool g_collectorEnabled         = false;
static bool g_collectorVerbose         = false;
static char g_collectorGeometryDir[80] = "assets:/non_final/geometry";
static u32  g_collectorFlags           = COLLECT_WATER_GEOMETRY | COLLECT_WATER_FAR_CLIP_TCMODIFIERS | COLLECT_MIRROR_GEOMETRY | COLLECT_CABLE_GEOMETRY /*| COLLECT_TREE_GEOMETRY*/ | COLLECT_PORTALS | COLLECT_LIGHTS | COLLECT_PROPGROUP_GEOMETRY | COLLECT_ANIMATED_BUILDINGS_BOUNDS;

static bool                g_collectorAllTimecycleTCModifiersOBJInited = false;
static CDumpGeometryToOBJ* g_collectorAllTimecycleTCModifiersOBJ[TCVAR_NUM] = {NULL};
static int                 g_collectorAllTimecycleTCModifiersNum[TCVAR_NUM] = {0};
static CDumpGeometryToOBJ* g_collectorWaterFarClipTCModifiersOBJ = NULL;
static CDumpGeometryToOBJ* g_collectorAnimatedBuildingsBoundsOBJ = NULL;

atMap<u32,CDumpGeometryToOBJ*> g_collectorAllTimecycleTCGroupsMap;

class CGCGeometry
{
public:
	CGCGeometry() {}
	CGCGeometry(const char* shaderName, int drawableLOD, int lodModelIndex, int geometryIndex)
		: m_shaderName(shaderName)
		, m_drawableLOD(drawableLOD)
		, m_lodModelIndex(lodModelIndex)
		, m_geometryIndex(geometryIndex)
	{}

	atString GetPath(const char* drawableName) const
	{
		return atVarString("geom_%s[%d][%d][%d]_%s.obj", drawableName, m_drawableLOD, m_lodModelIndex, m_geometryIndex, m_shaderName.c_str());
	}

	atString m_shaderName;
	int      m_drawableLOD;
	int      m_lodModelIndex;
	int      m_geometryIndex;
};

class CGCDrawable
{
public:
	CGCDrawable() {}
	CGCDrawable(const char* assetPath, const char* drawableName)
		: m_assetPath(assetPath)
		, m_drawableName(drawableName)
		, m_numInstances(0)
	{}

	atArray<CGCGeometry> m_geoms;
	atString             m_assetPath;
	atString             m_drawableName;
	int                  m_numInstances;
};

class CGCDrawableGroup
{
public:
	CGCDrawableGroup() {}
	CGCDrawableGroup(const char* base, const char* shaderNameFilter, bool bIsPropGroup, bool bIsLights, u32 portalFlags = 0)
		: m_base(base)
		, m_dir(atVarString("%s/models/%s", g_collectorGeometryDir, base))
		, m_shaderNameFilter(shaderNameFilter)
		, m_isPropGroup(bIsPropGroup)
		, m_isLights(bIsLights)
		, m_drawablesRemaining(0)
		, m_entityListFile(NULL)
		, m_portalFlags(portalFlags)
		, m_portalOBJ(NULL)
	{}

	void ProcessDrawableInternal(const char* assetPath, const char* drawableName, const grmShaderGroup* shaderGroup, const Drawable* drawable, const Fragment* fragment = NULL);
	void ProcessMapDataInternal(fwMapDataContents* mapData, u32 contentFlags, bool bIsScript);

	void AddEntityInternal(const CBaseModelInfo* modelInfo, Mat34V_In matrix, float scaleXY, float scaleZ, const char* ext);

	void FinishAndResetInternal();

	const char*          m_base; // e.g. "water"
	atString             m_dir; // e.g. "assets:/non_final/geometry/models/water"
	const char*          m_shaderNameFilter;
	bool                 m_isPropGroup; // ignore shader name, just include based on drawable name matching prop group
	bool                 m_isLights; // ignore shader name, just include based on whether there are lights
	atArray<CGCDrawable> m_drawables;
	int                  m_drawablesRemaining;
	fiStream*            m_entityListFile;
	u32                  m_portalFlags;
	CDumpGeometryToOBJ*  m_portalOBJ;
};

static atArray<CGCDrawableGroup> g_collectorDrawableGroups;

static void Init()
{
	if (g_collectorDrawableGroups.GetCount() == 0)
	{
		const bool bCollectPortals = (g_collectorFlags & COLLECT_PORTALS) != 0;

		if (g_collectorFlags & COLLECT_WATER_GEOMETRY)
		{
			g_collectorDrawableGroups.PushAndGrow(CGCDrawableGroup("water", "water", false, false, bCollectPortals ? INTINFO_PORTAL_WATER_SURFACE : 0));
		}

		if (g_collectorFlags & COLLECT_MIRROR_GEOMETRY)
		{
			g_collectorDrawableGroups.PushAndGrow(CGCDrawableGroup("mirror", "mirror", false, false, bCollectPortals ? INTINFO_PORTAL_MIRROR : 0));
		}

		if (g_collectorFlags & COLLECT_CABLE_GEOMETRY)
		{
			g_collectorDrawableGroups.PushAndGrow(CGCDrawableGroup("cable", "cable", false, false));
		}

		if (g_collectorFlags & COLLECT_TREE_GEOMETRY)
		{
			g_collectorDrawableGroups.PushAndGrow(CGCDrawableGroup("trees", "trees", false, false));
		}

		if (g_collectorFlags & COLLECT_LIGHTS)
		{
			g_collectorDrawableGroups.PushAndGrow(CGCDrawableGroup("lights", "lights", false, true));
		}

		if (g_collectorFlags & COLLECT_PROPGROUP_GEOMETRY)
		{
			g_collectorDrawableGroups.PushAndGrow(CGCDrawableGroup("propgroup", "propgroup", true, false));
		}
	}

	if (g_collectorWaterFarClipTCModifiersOBJ == NULL && (g_collectorFlags & COLLECT_WATER_FAR_CLIP_TCMODIFIERS) != 0)
	{
		g_collectorWaterFarClipTCModifiersOBJ = rage_new CDumpGeometryToOBJ(atVarString("%s/collected/water/water_no_reflection.obj", g_collectorGeometryDir).c_str());
	}	

	if (!g_collectorAllTimecycleTCModifiersOBJInited && (g_collectorFlags & COLLECT_ALL_TCMODIFIERS) != 0)
	{
		sysMemSet(g_collectorAllTimecycleTCModifiersOBJ, 0, sizeof(g_collectorAllTimecycleTCModifiersOBJ));
		sysMemSet(g_collectorAllTimecycleTCModifiersNum, 0, sizeof(g_collectorAllTimecycleTCModifiersNum));

		for (int i = 0; i < NELEM(g_collectorAllTimecycleTCModifiersOBJ); i++)
		{
			// TODO -- enabling all tcvars runs out of stream handles .. even enabling just the water tcvars runs out of memory though ..
		//	if (i >= TCVAR_WATER_REFLECTION_FIRST &&
		//		i <= TCVAR_WATER_REFLECTION_LAST)
			if (i == TCVAR_WATER_REFLECTION_FAR_CLIP ||
				i == TCVAR_WATER_REFLECTION_HEIGHT_OVERRIDE ||
				i == TCVAR_WATER_REFLECTION_LOD)
			{
				g_collectorAllTimecycleTCModifiersOBJ[i] = rage_new CDumpGeometryToOBJ(atVarString("%s/collected/timecycle/tcmodifier_%s.obj", g_collectorGeometryDir, g_varInfos[i].name).c_str(), "../../../materials3.mtl");
				g_collectorAllTimecycleTCModifiersOBJ[i]->MaterialBegin("white");
			}
		}

		g_collectorAllTimecycleTCModifiersOBJInited = true;
	}

	if (g_collectorAnimatedBuildingsBoundsOBJ == NULL && (g_collectorFlags & COLLECT_ANIMATED_BUILDINGS_BOUNDS) != 0)
	{
		g_collectorAnimatedBuildingsBoundsOBJ = rage_new CDumpGeometryToOBJ(atVarString("%s/collected/animated_buildings.obj", g_collectorGeometryDir).c_str());
	}
}

static void ProcessDrawable(const char* assetPath, const char* drawableName, const Drawable* drawable, const Fragment* fragment = NULL)
{
	if (drawable)
	{
		const grmShaderGroup* shaderGroup = drawable->GetShaderGroupPtr();

		if (shaderGroup)
		{
			Init();

			for (int i = 0; i < g_collectorDrawableGroups.GetCount(); i++)
			{
				g_collectorDrawableGroups[i].ProcessDrawableInternal(assetPath, drawableName, shaderGroup, drawable, fragment);
			}
		}
	}
}

void CGCDrawableGroup::ProcessDrawableInternal(const char* assetPath, const char* drawableName, const grmShaderGroup* shaderGroup, const Drawable* drawable, const Fragment* fragment)
{
	int numLights = 0;

	if (m_isPropGroup)
	{
		if (!IsInPropGroups(drawableName))
		{
			return;
		}
	}
	else if (m_isLights)
	{
		if (fragment)
		{
			const gtaFragType* gtaFrag = dynamic_cast<const gtaFragType*>(fragment);

			if (gtaFrag)
			{
				numLights = gtaFrag->m_lights.GetCount();
			}
		}
		else
		{
			const gtaDrawable* gtaDraw = dynamic_cast<const gtaDrawable*>(drawable);

			if (gtaDraw)
			{
				numLights = gtaDraw->m_lights.GetCount();
			}
		}

		if (numLights == 0)
		{
			return;
		}
	}
	else
	{
		bool bShaderMatch = false;

		for (int shaderIndex = 0; shaderIndex < shaderGroup->GetCount(); shaderIndex++)
		{
			const char* shaderName = shaderGroup->GetShader(shaderIndex).GetName();

			if (strstr(shaderName, m_shaderNameFilter))
			{
				bShaderMatch = true;
				break;
			}
		}

		if (!bShaderMatch)
		{
			return;
		}
	}

	if (g_collectorVerbose)
	{
		if (numLights > 0)
		{
			Displayf("### collecting geometry for %s (%d lights)", assetPath, numLights);
		}
		else
		{
			Displayf("### collecting geometry for %s", assetPath);
		}
	}

	const rmcLodGroup& lodGroup = drawable->GetLodGroup();
	CGCDrawable ed(assetPath, drawableName);

	for (int lodIndex = 0; lodIndex < LOD_COUNT; lodIndex++)
	{
		if (lodIndex > 0 && (m_isPropGroup || m_isLights))
		{
			break; // no lods for prop groups or lights
		}

		if (lodGroup.ContainsLod(lodIndex))
		{
			const rmcLod& lod = lodGroup.GetLod(lodIndex);

			for (int lodModelIndex = 0; lodModelIndex < lod.GetCount(); lodModelIndex++)
			{
				const grmModel* pModel = lod.GetModel(lodModelIndex);

				if (pModel)
				{
					const grmModel& model = *pModel;

					for (int geomIndex = 0; geomIndex < model.GetGeometryCount(); geomIndex++)
					{
						const int shaderIndex = model.GetShaderIndex(geomIndex);
						const char* shaderName = shaderGroup->GetShader(shaderIndex).GetName();

						if (!(m_isPropGroup || m_isLights) && strstr(shaderName, m_shaderNameFilter) == NULL)
						{
							continue;
						}

						if (m_isLights)
						{
							if (geomIndex == 0)
							{
								shaderName = "lights";
							}
							else
							{
								break;
							}
						}

						const CGCGeometry eg(shaderName, lodIndex, lodModelIndex, geomIndex);
						const grmGeometry& geom = model.GetGeometry(geomIndex);

						char path[512] = "";
						sprintf(path, "%s/%s", m_dir.c_str(), eg.GetPath(drawableName).c_str());
						strlwr(path);
						fiStream* f = fiStream::Create(path);

						if (Verifyf(f, "failed to create %s", path))
						{
							if (m_isLights) // just extract light descriptions, no geometry
							{
								class ProcessLight { public: static void func(fiStream* f, const CLightAttr& light)
								{
									const char* lightTypeStr = "UNKNOWN";

									switch (light.m_lightType)
									{
									case LIGHT_TYPE_NONE        : lightTypeStr = "NONE"; break;
									case LIGHT_TYPE_POINT       : lightTypeStr = "POINT"; break;
									case LIGHT_TYPE_SPOT        : lightTypeStr = "SPOT"; break;
									case LIGHT_TYPE_CAPSULE     : lightTypeStr = "CAPSULE"; break;
									case LIGHT_TYPE_DIRECTIONAL : lightTypeStr = "DIRECTIONAL"; break;
									case LIGHT_TYPE_AO_VOLUME   : lightTypeStr = "AO_VOLUME"; break;
									}

									Vec3V posn;
									light.GetPos(RC_VECTOR3(posn));
									const Vec3V colour = VECTOR3_TO_VEC3V(light.m_colour.GetVector3())*ScalarV(light.m_intensity);
									fprintf(f, "v %f %f %f # [light] type=%s flags=%d radius=%f colour=%f,%f,%f coronaSize=%f", VEC3V_ARGS(posn), lightTypeStr, light.m_flags, light.m_falloff, VEC3V_ARGS(colour), light.m_coronaIntensity > 0.0f ? light.m_coronaSize : 0.0f);

									if (light.m_lightType == LIGHT_TYPE_SPOT)
									{
										fprintf(f, " angle=%f dir=%f,%f,%f", light.m_coneOuterAngle, light.m_direction.x, light.m_direction.y, light.m_direction.z);
									}
									else if (light.m_lightType == LIGHT_TYPE_CAPSULE)
									{
										fprintf(f, " dir=%f,%f,%f extent=%f", light.m_direction.x, light.m_direction.y, light.m_direction.z, light.m_extents.x);
									}

									fprintf(f, "\n");
								}};

								if (fragment)
								{
									const gtaFragType* gtaFrag = dynamic_cast<const gtaFragType*>(fragment);

									for (int i = 0; i < gtaFrag->m_lights.GetCount(); i++)
									{
										ProcessLight::func(f, gtaFrag->m_lights[i]);
									}
								}
								else
								{
									const gtaDrawable* gtaDraw = dynamic_cast<const gtaDrawable*>(drawable);

									for (int i = 0; i < gtaDraw->m_lights.GetCount(); i++)
									{
										ProcessLight::func(f, gtaDraw->m_lights[i]);
									}
								}
							}
							else
							{
#if __PS3
								if (geom.GetType() == grmGeometry::GEOMETRYEDGE)
								{
									const grmGeometryEdge* pGeomEdge = static_cast<const grmGeometryEdge*>(&geom);

									int   s_bufferSizeV = GeometryUtil::EXTRACT_MAX_VERTICES;//g_vehicleGlassMan.GetEdgeVertexDataBufferSize()/sizeof(Vec3V); // assume only positions in stream
									void* s_bufferDataV = NULL;//g_vehicleGlassMan.GetEdgeVertexDataBuffer();
									int   s_bufferSizeI = GeometryUtil::EXTRACT_MAX_INDICES;//g_vehicleGlassMan.GetEdgeIndexDataBufferSize()/sizeof(u16);
									void* s_bufferDataI = NULL;//g_vehicleGlassMan.GetEdgeIndexDataBuffer();

									GeometryUtil::AllocateExtractData((Vec4V**)&s_bufferDataV, (u16**)&s_bufferDataI);

									// check up front how many verts are in processed geometry and assert if too many
									int numI = 0;
									int numV = 0;

									const EdgeGeomPpuConfigInfo* edgeGeomInfos = pGeomEdge->GetEdgeGeomPpuConfigInfos();
									const int count = pGeomEdge->GetEdgeGeomPpuConfigInfoCount();

									for (int i = 0; i < count; i++)
									{
										numI += edgeGeomInfos[i].spuConfigInfo.numIndexes;
										numV += edgeGeomInfos[i].spuConfigInfo.numVertexes;
									}

									if (numI > s_bufferSizeI)
									{
										Assertf(0, "%s: index buffer has more indices (%d) than system can handle (%d)", drawableName, numI, s_bufferSizeI);
										f->Close();
										continue;
									}

									if (numV > s_bufferSizeV)
									{
										Assertf(0, "%s: vertex buffer has more vertices (%d) than system can handle (%d)", drawableName, numV, s_bufferSizeV);
										f->Close();
										continue;
									}

									static Vec4V* edgeVertexStreams[CExtractGeomParams::obvIdxMax] ;
									sysMemSet(&edgeVertexStreams[0], 0, sizeof(edgeVertexStreams));

									u16* indices = (u16*)s_bufferDataI;

									u8  BoneIndexesAndWeights[8192];
									int BoneOffset1     = 0;
									int BoneOffset2     = 0;
									int BoneOffsetPoint = 0;
									int BoneIndexOffset = 0;
									int BoneIndexStride = 1;

#if HACK_GTA4_MODELINFOIDX_ON_SPU && __PS3
									CGta4DbgSpuInfoStruct gtaSpuInfoStruct;
									gtaSpuInfoStruct.gta4RenderPhaseID = 0;
									gtaSpuInfoStruct.gta4ModelInfoIdx  = 0;//pPhysical->GetModelIndex();
									gtaSpuInfoStruct.gta4ModelInfoType = 0;//pPhysical->GetBaseModelInfo()->GetModelType();
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU && __PS3

									int numVerts = 0;
									const int numIndices = pGeomEdge->GetVertexAndIndex(
										(Vector4*)s_bufferDataV,
										s_bufferSizeV,
										(Vector4**)edgeVertexStreams,
										indices,
										s_bufferSizeI,
										BoneIndexesAndWeights,
										sizeof(BoneIndexesAndWeights),
										&BoneIndexOffset,
										&BoneIndexStride,
										&BoneOffset1,
										&BoneOffset2,
										&BoneOffsetPoint,
										(u32*)&numVerts,
#if HACK_GTA4_MODELINFOIDX_ON_SPU
										&gtaSpuInfoStruct,
#endif // HACK_GTA4_MODELINFOIDX_ON_SPU
										NULL,
										CExtractGeomParams::extractPos
									);

									const Vec3V* positions = (const Vec3V*)edgeVertexStreams[CExtractGeomParams::obvIdxPositions];

									for (int i = 0; i < numIndices; i += 3)
									{
										const Vec3V p0 = positions[indices[i + 0]];
										const Vec3V p1 = positions[indices[i + 1]];
										const Vec3V p2 = positions[indices[i + 2]];

										fprintf(f, "v %f %f %f\n", VEC3V_ARGS(p0));
										fprintf(f, "v %f %f %f\n", VEC3V_ARGS(p1));
										fprintf(f, "v %f %f %f\n", VEC3V_ARGS(p2));
									}

									for (int i = 0; i < numIndices; i += 3)
									{
										fprintf(f, "f %d %d %d\n", 1 + i, 2 + i, 3 + i);
									}
								}
								else
#endif // __PS3
								{
									const grcIndexBuffer* pIndexBuffer = geom.GetIndexBuffer();
									const grcVertexBuffer* pVertexBuffer = geom.GetVertexBuffer();
									const u16* indices = pIndexBuffer->LockRO();
									const int numIndices = pIndexBuffer->GetIndexCount();

									PS3_ONLY(++g_AllowVertexBufferVramLocks); // PS3: attempting to edit VBs in VRAM
									grcVertexBufferEditor* pvbEditor = rage_new grcVertexBufferEditor(const_cast<grcVertexBuffer*>(pVertexBuffer), true, true);
									const int vertexCount = pvbEditor->GetVertexBuffer()->GetVertexCount();

									int numTriangles = 0;

									for (int i = 0; i < numIndices; i += 3)
									{
										const u16 index0 = indices[i + 0];
										const u16 index1 = indices[i + 1];
										const u16 index2 = indices[i + 2];

										if (Max<u16>(index0, index1, index2) < vertexCount)
										{
											const Vec3V p0 = VECTOR3_TO_VEC3V(pvbEditor->GetPosition(index0));
											const Vec3V p1 = VECTOR3_TO_VEC3V(pvbEditor->GetPosition(index1));
											const Vec3V p2 = VECTOR3_TO_VEC3V(pvbEditor->GetPosition(index2));

											fprintf(f, "v %f %f %f\n", VEC3V_ARGS(p0));
											fprintf(f, "v %f %f %f\n", VEC3V_ARGS(p1));
											fprintf(f, "v %f %f %f\n", VEC3V_ARGS(p2));
											numTriangles++;
										}
										else
										{
											Assertf(0, "%s: indices out of range! %d,%d,%d should be < %d (0x%p + %d)", drawableName, index0, index1, index2, vertexCount, indices, i*3);
											break;
										}
									}

									for (int i = 0; i < numTriangles; i++)
									{
										fprintf(f, "f %d %d %d\n", 1 + 3*i, 2 + 3*i, 3 + 3*i);
									}

									PS3_ONLY(--g_AllowVertexBufferVramLocks); // PS3: finished with editing VBs in VRAM
									pIndexBuffer->UnlockRO();
									delete pvbEditor;
								}
							}

							f->Close();
						}

						ed.m_geoms.PushAndGrow(eg);
					}
				}
			}
		}
	}

	m_drawables.PushAndGrow(ed);
	m_drawablesRemaining++;
}

template <typename T> static void ProcessDrawable_T(T& store, int slot, int entry, const Drawable* drawable, const Fragment* fragment = NULL)
{
	const atString assetPath = AssetAnalysis::GetAssetPath(store, slot, entry);
	char drawableName[256] = "";

	if (strstr(assetPath.c_str(), "/$") == NULL)
	{
		strcpy(drawableName, strrchr(assetPath.c_str(), '/') + 1);

		char* ext = strrchr(drawableName, '.');

		if (ext)
		{
			if (strcmp(ext, ".idr") == 0 ||
				strcmp(ext, ".ift") == 0)
			{
				*ext = '\0';
			}
		}
	}
	else
	{
		strcpy(drawableName, store.GetName(strLocalIndex(slot))); // huh? that's not good .. this will work for regular drawables at least
	}

	ProcessDrawable(assetPath.c_str(), drawableName, drawable, fragment);
}

// ================================================================================================

static void ProcessMapData(int slot)
{
	const fwMapDataDef* def = g_MapDataStore.GetSlot(strLocalIndex(slot));
	fwMapDataContents* mapData = def ? def->m_pObject : NULL;

	if (mapData)
	{
		Init();

		for (int i = 0; i < g_collectorDrawableGroups.GetCount(); i++)
		{
			g_collectorDrawableGroups[i].ProcessMapDataInternal(mapData, def->GetContentFlags(), def->GetIsScriptManaged());
		}

		if (g_collectorWaterFarClipTCModifiersOBJ)
		{
			const fwPool<tcBox>* pPool = tcBox::GetPool();

			if (AssertVerify(pPool))
			{
				for (int i = 0; i < pPool->GetSize(); i++)
				{
					const tcBox* pBox = pPool->GetSlot(i);

					if (pBox && pBox->GetIplId() == slot)
					{
						const tcModifier* pModifier = g_timeCycle.GetModifier(pBox->GetIndex());

						if (pModifier)
						{
							bool bHasModifierWaterReflectionFarClip = false;

							for (int j = 0; j < pModifier->GetModDataCount(); j++)
							{
								const int varId = pModifier->GetModDataArray()[j].varId;

								if (varId == TCVAR_WATER_REFLECTION_FAR_CLIP)
								{
									const float valA = pModifier->GetModDataArray()[j].valA;
									const float valB = pModifier->GetModDataArray()[j].valB;

									if (valA == 0.0f &&
										valB == 0.0f)
									{
										bHasModifierWaterReflectionFarClip = true;
									}
									else // why would we change the far clip to something other than zero? let's warn if this happens ..
									{
										Displayf("### mapdata \"%s\" timecycle modifier box %d/%d sets TCVAR_WATER_REFLECTION_FAR_CLIP values = [%f,%f]", g_MapDataStore.GetName(strLocalIndex(slot)), i, j, valA, valB);
									}
								}
							}

							if (bHasModifierWaterReflectionFarClip)
							{
								g_collectorWaterFarClipTCModifiersOBJ->AddBox(pBox->GetBoundingBox().GetMin(), pBox->GetBoundingBox().GetMax());
							}
						}
					}
				}
			}
		}

		if (g_collectorFlags & COLLECT_ALL_TCMODIFIERS)
		{
			const fwPool<tcBox>* pPool = tcBox::GetPool();

			if (AssertVerify(pPool))
			{
				for (int i = 0; i < pPool->GetSize(); i++)
				{
					const tcBox* pBox = pPool->GetSlot(i);

					if (pBox && pBox->GetIplId() == slot)
					{
						const tcModifier* pModifier = g_timeCycle.GetModifier(pBox->GetIndex());

						if (pModifier)
						{
							bool bHasTCModifier[TCVAR_NUM];
							sysMemSet(bHasTCModifier, 0, sizeof(bHasTCModifier));

							for (int j = 0; j < pModifier->GetModDataCount(); j++)
							{
								const int varId = pModifier->GetModDataArray()[j].varId;

								if (varId >= 0 && varId < TCVAR_NUM)
								{
									bHasTCModifier[varId] = true;
								}
							}

							for (int i = 0; i < NELEM(bHasTCModifier); i++)
							{
								if (bHasTCModifier[i])
								{
									CDumpGeometryToOBJ*& pDumpGeom = g_collectorAllTimecycleTCModifiersOBJ[i];
									const bool bCreateAll = false; // requires > 128 stream handles!
									const bool bCreateGroups = true;

									if (pDumpGeom == NULL && bCreateAll)
									{
										pDumpGeom = rage_new CDumpGeometryToOBJ(atVarString("%s/collected/timecycle/tcmodifier_%s.obj", g_collectorGeometryDir, g_varInfos[i].name).c_str(), "../../../materials3.mtl");
										pDumpGeom->MaterialBegin("white");
									}

									if (pDumpGeom)
									{
										pDumpGeom->AddBox(pBox->GetBoundingBox().GetMin(), pBox->GetBoundingBox().GetMax());
									}

									if (bCreateGroups)
									{
										const u32 groupHash = atHashString(g_varInfos[i].debugGroupName).GetHash();
										CDumpGeometryToOBJ** pDumpGeomPtr = g_collectorAllTimecycleTCGroupsMap.Access(groupHash);

										if (pDumpGeomPtr)
										{
											pDumpGeom = *pDumpGeomPtr;
										}
										else
										{
											char groupName[256] = "";
											strcpy(groupName, g_varInfos[i].debugGroupName);
											for (char* s = groupName; *s; s++)
											{
												if (*s == ' ' || *s == '/')
												{
													*s = '_';
												}
											}

											pDumpGeom = rage_new CDumpGeometryToOBJ(atVarString("%s/collected/timecycle/tcgroup_%s.obj", g_collectorGeometryDir, groupName).c_str(), "../../../materials3.mtl");
											pDumpGeom->MaterialBegin("white");

											g_collectorAllTimecycleTCGroupsMap[groupHash] = pDumpGeom;
										}

										pDumpGeom->AddBox(pBox->GetBoundingBox().GetMin(), pBox->GetBoundingBox().GetMax());
									}

									g_collectorAllTimecycleTCModifiersNum[i]++;
								}
							}
						}
					}
				}
			}
		}

#if ENTITY_LOD_TEST
		const spdAABB bounds(Vec3V(+2616.0f - 250.0f, -1296.0f - 250.0f, -1000.0f), Vec3V(+2616.0f + 250.0f, -1296.0f + 250.0f, +1000.0f));

		if (g_entitylodbuckets[0] == -1)
		{
			sysMemSet(g_entitylodbuckets, 0, sizeof(g_entitylodbuckets));
		}

		for (int i = 0; i < mapData->GetNumEntities(); i++)
		{
			const CEntity* entity = static_cast<const CEntity*>(mapData->GetEntities()[i]);

			if (entity && entity->GetLodData().IsHighDetail())
			{
				spdAABB box;
				entity->GetAABB(box);

				if (box.IntersectsAABB(bounds))
				{
					float f = (float)entity->GetLodDistance()/g_entitylodmax; // [0..1]
					f *= (float)g_entitylodbucketcount; // [0..n]
					int fi = (int)f;
					if (fi < g_entitylodbucketcount)
					{
						g_entitylodbuckets[fi]++;
					}
					else
					{
						g_entitylodbuckets[g_entitylodbucketcount]++;
					}
				}
			}
		}
#endif // ENTITY_LOD_TEST

		if (g_collectorAnimatedBuildingsBoundsOBJ)
		{
			for (int i = 0; i < mapData->GetNumEntities(); i++)
			{
				const CEntity* entity = static_cast<const CEntity*>(mapData->GetEntities()[i]);

				if (entity)
				{
					CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

					if (modelInfo)
					{
						const atString mapDataPath = AssetAnalysis::GetAssetPath(g_MapDataStore, slot);

						if (entity->GetIsTypeAnimatedBuilding())
						{
							const atString archetypePath = AssetAnalysis::GetArchetypePath(CDebugArchetype::GetDebugArchetypeProxyForModelInfo(modelInfo));
							const atString drawablePath  = AssetAnalysis::GetDrawablePath(modelInfo);

							const Vec3V position = entity->GetTransform().GetPosition();

							g_collectorAnimatedBuildingsBoundsOBJ->AddText("#mapdata=%s%s", mapDataPath.c_str(), def->GetIsScriptManaged() ? " (script)" : "");
							g_collectorAnimatedBuildingsBoundsOBJ->AddText("#archetype=%s", archetypePath.c_str());
							g_collectorAnimatedBuildingsBoundsOBJ->AddText("#drawable=%s", drawablePath.c_str());
							g_collectorAnimatedBuildingsBoundsOBJ->AddText("#position=%f,%f,%f", VEC3V_ARGS(position));

							spdAABB box;
							entity->GetAABB(box);

							g_collectorAnimatedBuildingsBoundsOBJ->AddBox(box.GetMin(), box.GetMax());
						}

						if (modelInfo->GetModelType() == MI_TYPE_MLO)
						{
							const atArray<fwEntityDef*>* interiorEntityDefs = &static_cast<CMloModelInfo*>(modelInfo)->GetEntities();

							if (interiorEntityDefs) // this can be NULL?
							{
								for (int j = 0; j < interiorEntityDefs->GetCount(); j++)
								{
									const fwEntityDef* interiorEntityDef = interiorEntityDefs->operator[](j);

									if (interiorEntityDef)
									{
										const u32 interiorModelIndex = CModelInfo::GetModelIdFromName(interiorEntityDef->m_archetypeName).GetModelIndex();

										if (interiorModelIndex != fwModelId::MI_INVALID)
										{
											const CBaseModelInfo* interiorModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(interiorModelIndex)));

											if (interiorModelInfo &&
												interiorModelInfo->GetIsTypeObject() == false &&
												interiorModelInfo->GetModelType() != MI_TYPE_COMPOSITE &&
												interiorModelInfo->GetClipDictionaryIndex() != -1 &&
												interiorModelInfo->GetHasAnimation())
											{
												const QuatV localQuat(-interiorEntityDef->m_rotation.x, -interiorEntityDef->m_rotation.y, -interiorEntityDef->m_rotation.z, interiorEntityDef->m_rotation.w);
												const Vec3V localPos(interiorEntityDef->m_position);
												Mat34V localMatrix;
												Mat34VFromQuatV(localMatrix, localQuat, localPos);
												Mat34V interiorMatrix;
												Transform(interiorMatrix, entity->GetMatrix(), localMatrix);
											//	const float interiorMatrixScaleXY = interiorEntityDef->m_scaleXY;
											//	const float interiorMatrixScaleZ = interiorEntityDef->m_scaleZ;

												const atString interiorArchetypePath = AssetAnalysis::GetArchetypePath(CDebugArchetype::GetDebugArchetypeProxyForModelInfo(interiorModelInfo));
												const atString interiorDrawablePath  = AssetAnalysis::GetDrawablePath(interiorModelInfo);

												const Vec3V interiorEntityPosition = interiorMatrix.GetCol3();

												g_collectorAnimatedBuildingsBoundsOBJ->AddText("#mapdata=%s%s", mapDataPath.c_str(), def->GetIsScriptManaged() ? " (script)" : "");
												g_collectorAnimatedBuildingsBoundsOBJ->AddText("#archetype=%s", interiorArchetypePath.c_str());
												g_collectorAnimatedBuildingsBoundsOBJ->AddText("#drawable=%s", interiorDrawablePath.c_str());
												g_collectorAnimatedBuildingsBoundsOBJ->AddText("#position=%f,%f,%f", VEC3V_ARGS(interiorEntityPosition));
												g_collectorAnimatedBuildingsBoundsOBJ->AddText("#MLO=%s", modelInfo->GetModelName());

												g_collectorAnimatedBuildingsBoundsOBJ->AddBox(interiorEntityPosition - Vec3V(V_FIVE), interiorEntityPosition + Vec3V(V_FIVE));
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

void CGCDrawableGroup::ProcessMapDataInternal(fwMapDataContents* mapData, u32 contentFlags, bool bIsScript)
{
	// init
	{
		if (m_entityListFile == NULL)
		{
			const atVarString path("%s/_entitylist.txt", m_dir.c_str());
			m_entityListFile = fiStream::Create(path.c_str());

			if (!Verifyf(m_entityListFile, "failed to create %s", path.c_str()))
			{
				return;
			}
		}

		if (m_portalFlags != 0)
		{
			if (m_portalOBJ == NULL)
			{
				m_portalOBJ = rage_new CDumpGeometryToOBJ(atVarString("%s/collected/%s/%s_portals.obj", g_collectorGeometryDir, m_base, m_base).c_str());
			}
		}
	}

	for (int i = 0; i < mapData->GetNumEntities(); i++)
	{
		const CEntity* entity = static_cast<const CEntity*>(mapData->GetEntities()[i]);

		if (entity == NULL)
		{
			continue;
		}

		CBaseModelInfo* modelInfo = entity->GetBaseModelInfo();

		if (modelInfo == NULL)
		{
			continue;
		}

		char ext[256] = "";

		if (contentFlags & fwMapData::CONTENTFLAG_ENTITIES_LOD)
		{
			if (ext[0] == '\0') { strcat(ext, "#"); }
			strcat(ext, "[LOD]");
		}

		if (bIsScript)
		{
			if (ext[0] == '\0') { strcat(ext, "#"); }
			strcat(ext, "[SCRIPT]");
		}

		AddEntityInternal(modelInfo, entity->GetMatrix(), entity->GetTransform().GetScaleXYV().Getf(), entity->GetTransform().GetScaleZV().Getf(), ext);

		if (modelInfo->GetModelType() == MI_TYPE_MLO && !m_isPropGroup)
		{
			const atArray<fwEntityDef*>* interiorEntityDefs = &static_cast<CMloModelInfo*>(modelInfo)->GetEntities();

			if (interiorEntityDefs) // this can be NULL?
			{
				for (int j = 0; j < interiorEntityDefs->GetCount(); j++)
				{
					const fwEntityDef* interiorEntityDef = interiorEntityDefs->operator[](j);

					if (interiorEntityDef)
					{
						const u32 interiorModelIndex = CModelInfo::GetModelIdFromName(interiorEntityDef->m_archetypeName).GetModelIndex();

						if (interiorModelIndex != fwModelId::MI_INVALID)
						{
							const CBaseModelInfo* interiorModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(interiorModelIndex)));

							if (interiorModelInfo)
							{
								int interiorProxyIndex = -1;
								CDebugArchetype::GetDebugArchetypeProxyForModelInfo(interiorModelInfo, &interiorProxyIndex);

								if (interiorProxyIndex != -1)
								{
									const QuatV localQuat(-interiorEntityDef->m_rotation.x, -interiorEntityDef->m_rotation.y, -interiorEntityDef->m_rotation.z, interiorEntityDef->m_rotation.w);
									const Vec3V localPos(interiorEntityDef->m_position);
									Mat34V localMatrix;
									Mat34VFromQuatV(localMatrix, localQuat, localPos);
									Mat34V interiorMatrix;
									Transform(interiorMatrix, entity->GetMatrix(), localMatrix);
									const float interiorMatrixScaleXY = interiorEntityDef->m_scaleXY;
									const float interiorMatrixScaleZ = interiorEntityDef->m_scaleZ;

									char interiorExt[256] = "";
									strcpy(interiorExt, ext);
									if (interiorExt[0] == '\0') { strcat(interiorExt, "#"); }
									strcat(interiorExt, atVarString("[MLO=%s]", modelInfo->GetModelName()).c_str());

									AddEntityInternal(interiorModelInfo, interiorMatrix, interiorMatrixScaleXY, interiorMatrixScaleZ, interiorExt);
								}
								else
								{
									//siLog(">> !ERROR: can't find archetype proxy for interior model \"%s\" in \"%s\"", interiorModelInfo->GetModelName(), modelInfo->GetModelName());
								}
							}
							else
							{
								//siLog(">> !ERROR: CModelInfo::GetBaseModelInfo returned NULL for interior model \"%s\" in \"%s\"", interiorModelName, modelInfo->GetModelName());
							}
						}
						else
						{
							//siLog(">> !ERROR: CModelInfo::GetModelIdFromName returned MI_INVALID for interior model \"%s\" in \"%s\"", interiorModelName, modelInfo->GetModelName());
						}
					}
				}
			}

			if (m_portalOBJ) // portals
			{
				const CMloModelInfo* pMloModelInfo = static_cast<const CMloModelInfo*>(modelInfo);

				for (int i = 0; i < pMloModelInfo->GetPortals().GetCount(); i++)
				{
					const CMloPortalDef& portalData = pMloModelInfo->GetPortals()[i];

					if (portalData.m_flags & m_portalFlags)
					{
						Vec3V corners[4];

						for (int j = 0; j < 4; j++)
						{
							corners[j] = Transform(entity->GetMatrix(), RCC_VEC3V(portalData.m_corners[j]));
						}

						fprintf(
							m_entityListFile,
							"- portal:[%f,%f,%f][%f,%f,%f][%f,%f,%f][%f,%f,%f]\n",
							VEC3V_ARGS(corners[3]),
							VEC3V_ARGS(corners[2]),
							VEC3V_ARGS(corners[1]),
							VEC3V_ARGS(corners[0])
						);
						m_entityListFile->Flush();

						m_portalOBJ->AddQuad(corners[3], corners[2], corners[1], corners[0]);
					}
				}
			}
		}
	}
}

void CGCDrawableGroup::AddEntityInternal(const CBaseModelInfo* modelInfo, Mat34V_In matrix, float scaleXY, float scaleZ, const char* ext)
{
	for (int i = 0; i < m_drawables.GetCount(); i++)
	{
		const CGCDrawable& ed = m_drawables[i];
		const char* drawableName = ed.m_drawableName.c_str();

		if (stricmp(modelInfo->GetModelName(), drawableName) == 0)
		{
			for (int j = 0; j < ed.m_geoms.GetCount(); j++)
			{
				const CGCGeometry& eg = ed.m_geoms[j];
				const atString geomPath = eg.GetPath(ed.m_drawableName.c_str());

				const Vec3V col0 = matrix.GetCol0();
				const Vec3V col1 = matrix.GetCol1();
				const Vec3V col2 = matrix.GetCol2();
				const Vec3V col3 = matrix.GetCol3();
				char line[1024] = "";
				sprintf(line, "[%f,%f,%f][%f,%f,%f][%f,%f,%f][%f,%f,%f][%f,%f]:%s%s", VEC3V_ARGS(col0), VEC3V_ARGS(col1), VEC3V_ARGS(col2), VEC3V_ARGS(col3), scaleXY, scaleZ, geomPath.c_str(), ext);
				strlwr(line);

				if (strstr(eg.m_shaderName, "water")) // water should not be rotated ..
				{
					if (strcmp(eg.m_shaderName, "water_foam") != 0 && // .. unless it's foam
						strcmp(eg.m_shaderName, "water_terrainfoam") != 0)
					{
						if (MaxElement(Abs(col0 - Vec3V(V_X_AXIS_WZERO))).Getf() > 0.00001f ||
							MaxElement(Abs(col1 - Vec3V(V_Y_AXIS_WZERO))).Getf() > 0.00001f ||
							MaxElement(Abs(col2 - Vec3V(V_Z_AXIS_WZERO))).Getf() > 0.00001f)
						{
							strcat(line, " # rotated"); // this is an art bug (water drawables cannot have rotation)
						}
					}
				}

				fprintf(m_entityListFile, "%s\n", line);
				m_entityListFile->Flush();
			}

			if (m_drawables[i].m_numInstances++ == 0)
			{
				m_drawablesRemaining--;
			}

			if (g_collectorVerbose && !m_isPropGroup)
			{
				Displayf("%s: %s, remaining %d", m_base, drawableName, m_drawablesRemaining);
			}

			break;
		}
	}
}

void CGCDrawableGroup::FinishAndResetInternal()
{
	if (m_entityListFile)
	{
		Displayf("finalising mapdata geometry collection for %s ..", m_dir.c_str());
		char temp[256] = "";

		if (m_drawablesRemaining != 0)
		{
			sprintf(temp, "### %d of %d drawables in %s group are missing", m_drawablesRemaining, m_drawables.GetCount(), m_base);
		}
		else
		{
			sprintf(temp, "all %d drawables in %s group are accounted for", m_drawables.GetCount(), m_base);
		}

		fprintf(m_entityListFile, "%s\n", temp);
		Displayf("%s", temp);

		if (m_drawablesRemaining != 0)
		{
			for (int i = 0; i < m_drawables.GetCount(); i++)
			{
				if (m_drawables[i].m_numInstances == 0)
				{
					sprintf(temp, "    %s", m_drawables[i].m_assetPath.c_str());
					fprintf(m_entityListFile, "%s\n", temp);
					Displayf("%s", temp);
				}
			}
		}

		/*if (g_siLogStrings.GetCount() > 0)
		{
			fprintf(m_entityListFile, "\nstreaming iterators encountered %d errors:\n", g_siLogStrings.GetCount());
			Displayf("streaming iterators encountered %d errors", g_siLogStrings.GetCount());

			for (int i = 0; i < g_siLogStrings.GetCount(); i++)
			{
				fprintf(m_entityListFile, "%s\n", g_siLogStrings[i].c_str());
			}

			g_siLogStrings.clear();
			g_siLogRecordErrors = false;
		}*/

		m_entityListFile->Close();
		m_entityListFile = NULL;
	}

	m_drawables.clear();
	m_drawablesRemaining = 0;

	if (m_portalOBJ)
	{
		m_portalOBJ->Close();
		delete m_portalOBJ;
		m_portalOBJ = NULL;
	}
}

// ================================================================================================

void AddWidgets(bkBank& bk)
{
	bk.PushGroup("Geometry Collector", false);
	{
		bk.AddToggle("Enabled"                    , &g_collectorEnabled);
		bk.AddToggle("Verbose"                    , &g_collectorVerbose);
		bk.AddText  ("Geometry Dir"               , &g_collectorGeometryDir[0], sizeof(g_collectorGeometryDir), false);
		bk.AddToggle("Collect Water Geometry"     , &g_collectorFlags, COLLECT_WATER_GEOMETRY);
		bk.AddToggle("Collect Water TC Modifiers" , &g_collectorFlags, COLLECT_WATER_FAR_CLIP_TCMODIFIERS);
		bk.AddToggle("Collect All TC Modifiers"   , &g_collectorFlags, COLLECT_ALL_TCMODIFIERS);
		bk.AddToggle("Collect Mirror Geometry"    , &g_collectorFlags, COLLECT_MIRROR_GEOMETRY);
		bk.AddToggle("Collect Cable Geometry"     , &g_collectorFlags, COLLECT_CABLE_GEOMETRY);
		bk.AddToggle("Collect Tree Geometry"      , &g_collectorFlags, COLLECT_TREE_GEOMETRY);
		bk.AddToggle("Collect Portals"            , &g_collectorFlags, COLLECT_PORTALS);
		bk.AddToggle("Collect Lights"             , &g_collectorFlags, COLLECT_LIGHTS);
		bk.AddToggle("Collect Prop Group Geometry", &g_collectorFlags, COLLECT_PROPGROUP_GEOMETRY);
		bk.AddToggle("Collect Animated Buildings" , &g_collectorFlags, COLLECT_ANIMATED_BUILDINGS_BOUNDS);
	}
	bk.PopGroup();
}

void ProcessDwdSlot(int slot)
{
	if (g_collectorEnabled)
	{
		const Dwd* dwd = g_DwdStore.Get(strLocalIndex(slot));

		if (dwd)
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				ProcessDrawable_T(g_DwdStore, slot, i, dwd->GetEntry(i));
			}
		}
	}
}

void ProcessDrawableSlot(int slot)
{
	if (g_collectorEnabled)
	{
		ProcessDrawable_T(g_DrawableStore, slot, INDEX_NONE, g_DrawableStore.Get(strLocalIndex(slot)));
	}
}

void ProcessFragmentSlot(int slot)
{
	if (g_collectorEnabled)
	{
		const Fragment* fragment = g_FragmentStore.Get(strLocalIndex(slot));

		if (fragment)
		{
			ProcessDrawable_T(g_FragmentStore, slot, AssetAnalysis::FRAGMENT_COMMON_DRAWABLE_ENTRY, fragment->GetCommonDrawable(), fragment);
			ProcessDrawable_T(g_FragmentStore, slot, AssetAnalysis::FRAGMENT_CLOTH_DRAWABLE_ENTRY, fragment->GetClothDrawable());

			for (int i = 0; i < fragment->GetNumExtraDrawables(); i++)
			{
				ProcessDrawable_T(g_FragmentStore, slot, i, fragment->GetExtraDrawable(i));
			}
		}
	}
}

void ProcessMapDataSlot(int slot)
{
	if (g_collectorEnabled)
	{
		ProcessMapData(slot);
	}
}

void Start()
{
	g_collectorEnabled = true;
}

void FinishAndReset()
{
	if (g_collectorEnabled)
	{
#if ENTITY_LOD_TEST
		for (int i = 0; i < g_entitylodbucketcount; i++)
		{
			Displayf("%d HD entities with lod distance %f..%f", g_entitylodbuckets[i], g_entitylodmax*(float)i/(float)g_entitylodbucketcount, g_entitylodmax*(float)(i + 1)/(float)g_entitylodbucketcount);
		}

		Displayf("%d HD entities with lod distance %f..INF", g_entitylodbuckets[g_entitylodbucketcount], g_entitylodmax);
#endif // ENTITY_LOD_TEST

		if (g_collectorWaterFarClipTCModifiersOBJ)
		{
			g_collectorWaterFarClipTCModifiersOBJ->Close();
			delete g_collectorWaterFarClipTCModifiersOBJ;
			g_collectorWaterFarClipTCModifiersOBJ = NULL;
		}

		for (int i = 0; i < NELEM(g_collectorAllTimecycleTCModifiersOBJ); i++)
		{
			CDumpGeometryToOBJ*& pDumpGeom = g_collectorAllTimecycleTCModifiersOBJ[i];

			if (pDumpGeom)
			{
				Displayf("finishing %s", g_varInfos[i].name);

				pDumpGeom->MaterialEnd();

				pDumpGeom->AddText("g 1000");
				pDumpGeom->AddText("usemtl map");
				pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MIN_X, (float)gv::WORLD_BOUNDS_MAX_Y, 0.0f);
				pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MAX_X, (float)gv::WORLD_BOUNDS_MAX_Y, 0.0f);
				pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MAX_X, (float)gv::WORLD_BOUNDS_MIN_Y, 0.0f);
				pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MIN_X, (float)gv::WORLD_BOUNDS_MIN_Y, 0.0f);
				pDumpGeom->AddText("vt 0 1");
				pDumpGeom->AddText("vt 1 1");
				pDumpGeom->AddText("vt 1 0");
				pDumpGeom->AddText("vt 0 0");
				pDumpGeom->AddText("f -1/-1 -2/-2 -3/-3 -4/-4");

				pDumpGeom->Close();
				delete pDumpGeom;
				pDumpGeom = NULL;
			}

			g_collectorAllTimecycleTCModifiersOBJInited = false;
		}

		if (g_collectorAllTimecycleTCGroupsMap.GetNumUsed() > 0)
		{
			for (atMap<u32,CDumpGeometryToOBJ*>::Iterator iter = g_collectorAllTimecycleTCGroupsMap.CreateIterator(); !iter.AtEnd(); iter.Next())
			{
				CDumpGeometryToOBJ*& pDumpGeom = iter.GetData();

				if (pDumpGeom)
				{
					const char* groupName = "?";

					for (int i = 0; i < TCVAR_NUM; i++)
					{
						if (atHashString(g_varInfos[i].debugGroupName).GetHash() == iter.GetKey())
						{
							groupName = g_varInfos[i].debugGroupName;
							break;
						}
					}

					Displayf("finishing %s", groupName);

					pDumpGeom->MaterialEnd();

					pDumpGeom->AddText("g 1000");
					pDumpGeom->AddText("usemtl map");
					pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MIN_X, (float)gv::WORLD_BOUNDS_MAX_Y, 0.0f);
					pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MAX_X, (float)gv::WORLD_BOUNDS_MAX_Y, 0.0f);
					pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MAX_X, (float)gv::WORLD_BOUNDS_MIN_Y, 0.0f);
					pDumpGeom->AddText("v %f %f %f", (float)gv::WORLD_BOUNDS_MIN_X, (float)gv::WORLD_BOUNDS_MIN_Y, 0.0f);
					pDumpGeom->AddText("vt 0 1");
					pDumpGeom->AddText("vt 1 1");
					pDumpGeom->AddText("vt 1 0");
					pDumpGeom->AddText("vt 0 0");
					pDumpGeom->AddText("f -1/-1 -2/-2 -3/-3 -4/-4");

					pDumpGeom->Close();
					delete pDumpGeom;
					pDumpGeom = NULL;
				}
			}

			g_collectorAllTimecycleTCGroupsMap.Kill();
		}

		for (int i = 0; i < NELEM(g_collectorAllTimecycleTCModifiersOBJ); i++)
		{
			if (g_collectorAllTimecycleTCModifiersNum[i] > 0)
			{
				Displayf("### %d tcmodifiers affect %s", g_collectorAllTimecycleTCModifiersNum[i], g_varInfos[i].name);
				g_collectorAllTimecycleTCModifiersNum[i] = 0;
			}
		}

		if (g_collectorAnimatedBuildingsBoundsOBJ)
		{
			g_collectorAnimatedBuildingsBoundsOBJ->Close();
			delete g_collectorAnimatedBuildingsBoundsOBJ;
			g_collectorAnimatedBuildingsBoundsOBJ = NULL;
		}

		for (int i = 0; i < g_collectorDrawableGroups.GetCount(); i++)
		{
			g_collectorDrawableGroups[i].FinishAndResetInternal();
		}

		g_collectorDrawableGroups.clear();

		fiStream* finished = fiStream::Create("x:/sync.gc1");

		if (finished)
		{
			finished->Close();
		}
	}
}

} // namespace GeomCollector

#endif // __BANK

// ================================================================================================

#if __BANK || (__WIN32PC && !__FINAL)

namespace GeomCollector {

static atArray<CPropGroup> g_propGroups;
static bool                g_propGroupsLoaded = false;

// NOTE -- this function must be kept in sync between 3 files:
// x:\gta5\src\dev\game\debug\AssetAnalysis\GeometryCollector.cpp
// x:\gta5\src\dev\tools\GeometryCollectorTool\src\GeometryCollectorTool.cpp
// x:\gta5\src\dev\tools\HeightMapImageProcessor\src\HeightMapImageProcessor.cpp
void LoadPropGroups(bool bSeparateGeomGroups = true, bool WIN32PC_ONLY(bVerbose) = true)
{
	if (!g_propGroupsLoaded)
	{
		const char* propGroupListPath = "assets:/non_final/propgroups/list.txt";
		fiStream* pGroupList = fiStream::Open(propGroupListPath, true);

		if (Verifyf(pGroupList, "failed to open \"%s\"\n", propGroupListPath))
		{
			char line[128] = "";

			while (fgetline(line, sizeof(line), pGroupList))
			{
				const atVarString propGroupPath("assets:/non_final/propgroups/%s.txt", line);
				fiStream* pGroup = fiStream::Open(propGroupPath.c_str(), true);

				if (Verifyf(pGroup, "failed to open \"%s\"\n", propGroupPath.c_str()))
				{
					char* pExt = strrchr(line, '.');

					if (pExt)
					{
						*pExt = '\0';
					}

					g_propGroups.PushAndGrow(CPropGroup(line));

					float fPropRadiusMax = 0.0f;

					while (fgetline(line, sizeof(line), pGroup))
					{
						const char* pPropName = strtok(line, ",\t ");

						if (pPropName)
						{
							const char* pPropRadiusStr = strtok(NULL, ",\t ");
							const float fPropRadius = pPropRadiusStr ? (float)atof(pPropRadiusStr) : 0.0f;

							g_propGroups.back().m_entries.PushAndGrow(CPropGroupEntry(pPropName, fPropRadius));
							fPropRadiusMax = Max<float>(fPropRadius, fPropRadiusMax);
						}
					}
#if __WIN32PC
					if (bVerbose)
					{
						fprintf(stdout, "loaded prop group \"%s\"\n", g_propGroups.back().m_name.c_str());

						for (int i = 0; i < g_propGroups.back().m_entries.GetCount(); i++)
						{
							fprintf(stdout, "  %s (radius=%f)\n", g_propGroups.back().m_entries[i].m_name.c_str(), g_propGroups.back().m_entries[i].m_fRadius);
						}
					}
#endif // __WIN32PC
					if (bSeparateGeomGroups && fPropRadiusMax > 0.0f)
					{
						pGroup->Seek(0); // rewind

						sprintf(line, "%s_geom", g_propGroups.back().m_name.c_str());
						g_propGroups.PushAndGrow(CPropGroup(line));

						while (fgetline(line, sizeof(line), pGroup))
						{
							const char* pPropName = strtok(line, ",\t ");

							if (pPropName)
							{
								g_propGroups.back().m_entries.PushAndGrow(CPropGroupEntry(pPropName, 0.0f));
							}
						}
#if __WIN32PC
						if (bVerbose)
						{
							fprintf(stdout, "loaded prop group \"%s\"\n", g_propGroups.back().m_name.c_str());
							fprintf(stdout, "  all radii set to zero\n");
						}
#endif // __WIN32PC
					}

					pGroup->Close();
				}
#if __WIN32PC
				else
				{
					fprintf(stderr, "failed to open \"%s\"\n", propGroupPath.c_str());
				}
#endif // __WIN32PC
			}

			pGroupList->Close();
		}
#if __WIN32PC
		else
		{
			fprintf(stderr, "failed to open \"%s\"\n", propGroupListPath);
		}
#endif // __WIN32PC

		g_propGroupsLoaded = true;
	}
}

const atArray<CPropGroup>& GetPropGroups()
{
	LoadPropGroups();

	return g_propGroups;
}

bool IsInPropGroups(const char* pModelName)
{
	LoadPropGroups();

	for (int i = 0; i < g_propGroups.GetCount(); i++)
	{
		const CPropGroup& group = g_propGroups[i];

		for (int j = 0; j < group.m_entries.GetCount(); j++)
		{
			if (stricmp(pModelName, group.m_entries[j].m_name.c_str()) == 0)
			{
				return true;
			}
		}
	}

	return false;
}

} // namespace GeomCollector

#endif // __BANK || (__WIN32PC && !__FINAL)
