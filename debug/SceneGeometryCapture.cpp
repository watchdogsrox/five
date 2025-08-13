// ==============================
// debug/SceneGeometryCapture.cpp
// (c) 2013 Rockstar
// ==============================

#if __BANK

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "file/asset.h"
#include "file/remote.h"
#include "file/stream.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/image.h"
#include "grcore/viewport.h"
#include "grmodel/model.h"
#include "grmodel/shader.h"
#include "grmodel/shadergroup.h"
#include "parser/manager.h"
#include "rmcore/lodgroup.h"
#include "spatialdata/aabb.h"
#include "spatialdata/sphere.h"
#include "string/stringutil.h"
#include "vector/geometry.h"
#include "vectormath/classes.h"

#include "fwdebug/picker.h"
#include "fwgeovis/geovis.h"
#include "fwscene/lod/LodTypes.h"
#include "fwscene/world/EntityContainer.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/AssetAnalysis/GeometryCollector.h" // for area code boxes
#include "debug/AssetAnalysis/StreamingIteratorManager.h" // for area code boxes
#include "debug/DebugGeometryUtil.h"
#include "debug/MarketingTools.h"
#include "debug/SceneGeometryCapture.h"
#include "modelinfo/BaseModelInfo.h"
#include "Objects/DummyObject.h"
#include "Objects/Object.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"
#include "physics/WorldProbe/WorldProbe.h"
#include "renderer/Renderer.h"
#include "scene/AnimatedBuilding.h"
#include "scene/Building.h"
#include "scene/Entity.h"
//#include "scene/EntityBatch.h"
#include "scene/EntityTypes.h"
#include "scene/LoadScene.h"
#include "scene/world/GameWorldHeightMap.h"
#include "vehicles/vehicle.h"

namespace rage { XPARAM(nopopups); }

#define SGC_DEFAULT_PATH "assets:"
#define SGC_DEFAULT_NAME "default"
#define SGC_DEFAULT_RADIUS 5.0f
#define SGC_DEFAULT_HEIGHT 0.0f
#define SGC_DEFAULT_COLOUR Color32(0,255,0,255)
#define SGC_MAX_SPHERES 2000
#define SGC_PRIVATE_RDR_STUFF (1 && RDR_VERSION) // TODO -- clean this up .. it might be useful for other people too
#define SGC_PANORAMA (1)

class CSceneGeometryCapture
{
public:
	CSceneGeometryCapture() : m_panoramaPath(SGC_DEFAULT_PATH), m_geometryPath(SGC_DEFAULT_PATH) {}

	class Sphere
	{
	public:
		Sphere(const char* name = SGC_DEFAULT_NAME) : m_name(name), m_sphere(V_ZERO), m_height(SGC_DEFAULT_HEIGHT), m_colour(SGC_DEFAULT_COLOUR)
		{
			sysMemSet(m_areaCode, 0, sizeof(m_areaCode));
		}

		atString m_name;
		char m_areaCode[4]; // rdr-specific
		Vec4V m_sphere;
		float m_height; // camera height
		Color32 m_colour;
		PAR_SIMPLE_PARSABLE;
	};

	atString m_panoramaPath;
	atString m_geometryPath;
	atArray<Sphere> m_spheres;
	PAR_SIMPLE_PARSABLE;
};

#include "debug/SceneGeometryCapture_parser.h"

namespace SceneGeometryCapture {

static CSceneGeometryCapture* g = NULL;

static char         g_metafilePath[128]              = "assets:/capture.meta";
static bkCombo*     g_selectedSphereCombo            = NULL;
static char         g_selectedSphereComboFilter[128] = "";
static atArray<int> g_selectedSphereComboList;
static int          g_selectedSphereComboIndex       = 0; // index into g_selectedSphereComboList
static char         g_selectedSphereName[128]        = SGC_DEFAULT_NAME;
static Vec3V        g_selectedSphereCentre           = Vec3V(V_ZERO);
static float        g_selectedSphereRadius           = SGC_DEFAULT_RADIUS;
static float        g_selectedSphereHeight           = SGC_DEFAULT_HEIGHT;
static Color32      g_selectedSphereColour           = SGC_DEFAULT_COLOUR;
static int          g_selectedSphereIndex            = -1;
static bool         g_warpToCameraHeight             = false;
static float        g_cameraHeightOffset             = 10.0f;
static bool         g_captureSelectedSphereOnly      = false;
static int          g_captureAutoListIndex           = -1;
static int          g_captureAutoSphereIndex         = -1;
static int          g_captureAutoStreamFrames        = 0;
#if SGC_PANORAMA
static bool         g_capturePanoramaEnabled         = false;
static bool         g_capturePanoramaShowFPS         = true;
static Vec2V        g_capturePanoramaShowFPSPos      = Vec2V(32.0f, 100.0f);
static Color32      g_capturePanoramaShowFPSColour   = Color32(255,255,255,255);
static bool         g_capturePanoramaShowFPSBkgQuad  = true;
static float        g_capturePanoramaShowFPSScale    = 6.0f;
static int          g_capturePanoramaSteps           = 12;
static int          g_capturePanoramaFrameIndex      = -1;
static int          g_capturePanoramaAutoTimer       = -1;
static int          g_capturePanoramaAutoTimerDelay  = 3;
static float        g_capturePanoramaCameraFacing    = 0.0f;
static char         g_capturePanoramaPath[128]       = SGC_DEFAULT_PATH;
#endif // SGC_PANORAMA
static char         g_captureGeometryPath[128]       = SGC_DEFAULT_PATH;
static u32          g_captureEntityLodFlags          = LODTYPES_FLAG_HD | LODTYPES_FLAG_ORPHANHD;
static u32          g_captureEntityPoolFlags         = ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_OBJECT | ENTITY_TYPE_MASK_DUMMY_OBJECT | ENTITY_TYPE_MASK_MLO /*| ENTITY_TYPE_MASK_PED*/ | ENTITY_TYPE_MASK_VEHICLE;
static bool         g_captureProps                   = true;
static bool         g_captureTrees                   = true;
static bool         g_captureAlpha                   = true;
static bool         g_captureWater                   = true;
static bool         g_captureDecals                  = false;
//static bool         g_captureTerrain                 = true;
static u32          g_captureDrawableLodFlags        = BIT(LOD_HIGH);
static bool         g_captureClipToSpheres           = true;
static bool         g_captureVerbose                 = false;
static float        g_drawOpacity                    = 1.0f;
static float        g_drawDistanceMin                = 400.0f;
static float        g_drawDistanceMax                = 500.0f;

static int MakeNameUnique(char* newName)
{
	int index = -1;

	if (g)
	{
		while (true)
		{
			bool bUnique = true;

			for (int i = 0; i < g->m_spheres.GetCount(); i++)
			{
				if (stricmp(newName, g->m_spheres[i].m_name.c_str()) == 0)
				{
					if (index == -1)
					{
						index = i;
					}

					bUnique = false;
					break;
				}
			}

			if (!bUnique)
			{
				char* lastNonNumericChar = newName + strlen(newName) - 1;

				while (lastNonNumericChar >= newName && isdigit(*lastNonNumericChar))
				{
					lastNonNumericChar--;
				}

				const int numeric = atoi(++lastNonNumericChar);

				strcpy(lastNonNumericChar, atVarString("%d", numeric + 1).c_str());
			}
			else
			{
				break;
			}
		}
	}

	return index; // index of first sphere with name conflict, or -1 indicating no name conflict
}

static void UpdateCombo_cb();

static void UpdateSelectedSphere_cb()
{
	if (g && g_selectedSphereIndex >= 0 && g_selectedSphereIndex < g->m_spheres.GetCount())
	{
		CSceneGeometryCapture::Sphere& sphere = g->m_spheres[g_selectedSphereIndex];

		sphere.m_name   = atString(g_selectedSphereName);
		sphere.m_sphere = Vec4V(g_selectedSphereCentre, ScalarV(g_selectedSphereRadius));
		sphere.m_height = g_selectedSphereHeight;
		sphere.m_colour = g_selectedSphereColour;
	}
}

static void UpdateSelectedSphereCameraHeight_cb()
{
	if (g_warpToCameraHeight && camInterface::GetDebugDirector().IsFreeCamActive())
	{
		camFrame& cam = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();
		const Vec3V pos(g_selectedSphereCentre.GetXY(), ScalarV(g_selectedSphereHeight + g_cameraHeightOffset));
		cam.SetPosition(RCC_VECTOR3(pos));
	}
}

static void UpdateSelectedSphereCamera_cb()
{
	UpdateSelectedSphereCameraHeight_cb();
	UpdateSelectedSphere_cb();
}

static void UpdateSelectedSphereName_cb()
{
	if (g && g_selectedSphereIndex >= 0 && g_selectedSphereIndex < g->m_spheres.GetCount())
	{
		CSceneGeometryCapture::Sphere& sphere = g->m_spheres[g_selectedSphereIndex];

		char newName[128] = "";
		strcpy(newName, g_selectedSphereName);
		const int index = MakeNameUnique(newName);

		sphere.m_name = atString(newName);

		if (index >= 0)
		{
			Displayf("sphere name %s was used (index = %d), changing name to %s", g_selectedSphereName, index, newName);
			strcpy(g_selectedSphereName, newName);
		}
	}

	UpdateSelectedSphere_cb();
	UpdateCombo_cb();
}

static void UpdateSelectedSphereIndex_cb()
{
	if (g && g_selectedSphereIndex >= 0 && g->m_spheres.GetCount() > 0)
	{
		g_selectedSphereIndex = Min<int>(g_selectedSphereIndex, g->m_spheres.GetCount() - 1);

		const CSceneGeometryCapture::Sphere& sphere = g->m_spheres[g_selectedSphereIndex];

		strcpy(g_selectedSphereName, sphere.m_name.c_str() ? sphere.m_name.c_str() : SGC_DEFAULT_NAME);

		g_selectedSphereCentre = sphere.m_sphere.GetXYZ();
		g_selectedSphereRadius = sphere.m_sphere.GetWf();
		g_selectedSphereHeight = sphere.m_height;
		g_selectedSphereColour = sphere.m_colour;
	}
	else
	{
		strcpy(g_selectedSphereName, SGC_DEFAULT_NAME);

		g_selectedSphereCentre = Vec3V(V_ZERO);
		g_selectedSphereRadius = SGC_DEFAULT_RADIUS;
		g_selectedSphereHeight = SGC_DEFAULT_HEIGHT;
		g_selectedSphereColour = SGC_DEFAULT_COLOUR;
		g_selectedSphereIndex  = -1;
	}

	g_selectedSphereComboIndex = 0;

	for (int i = 0; i < g_selectedSphereComboList.GetCount(); i++)
	{
		if (g_selectedSphereComboList[i] == g_selectedSphereIndex)
		{
			g_selectedSphereComboIndex = i;
			break;
		}
	}
}

static void SelectedSphereSnapToGround_button()
{
	if (g && g_selectedSphereIndex >= 0 && g->m_spheres.GetCount() > 0)
	{
		Vec3V vStart = g->m_spheres[g_selectedSphereIndex].m_sphere.GetXYZ();
		Vec3V vEnd = vStart;
		vStart.SetZf(+8000.0f);
		vEnd.SetZf(-8000.0f);

		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestHitPoint probeHitPoint;
		WorldProbe::CShapeTestResults probeResult(probeHitPoint);
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(RCC_VECTOR3(vStart), RCC_VECTOR3(vEnd));
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

		if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			Displayf("probe hit pos=%f,%f,%f, normal=%f,%f,%f", VEC3V_ARGS(probeResult[0].GetHitPositionV()), VEC3V_ARGS(probeResult[0].GetHitNormalV()));
			g_selectedSphereCentre = probeResult[0].GetHitPositionV();
			g->m_spheres[g_selectedSphereIndex].m_sphere = Vec4V(g_selectedSphereCentre, ScalarV(g_selectedSphereRadius));
		}
		else
		{
			Displayf("probe failed");
		}
	}
}

#if SGC_PANORAMA
static void UpdateCameraFacing_cb()
{
	if (camInterface::GetDebugDirector().IsFreeCamActive())
	{
		camFrame& cam = camInterface::GetDebugDirector().GetFreeCamFrameNonConst();
		cam.SetWorldMatrixFromHeadingPitchAndRoll(DtoR*g_capturePanoramaCameraFacing, 0.0f, 0.0f);
	}
}
#endif // SGC_PANORAMA

static void LoadMetafile_button()
{
	if (ASSET.Exists(g_metafilePath, NULL))
	{
		bool bCreated = false;

		if (g == NULL)
		{
			g = rage_new CSceneGeometryCapture();
			bCreated = true;
		}

		if (PARSER.InitAndLoadObject(g_metafilePath, NULL, *g))
		{
			g_selectedSphereIndex = -1;
			UpdateSelectedSphereIndex_cb();
			UpdateCombo_cb();
			Displayf("%s loaded (%d spheres)", g_metafilePath, g->m_spheres.GetCount());

			// check that there are no duplicate names
			for (int i = 0; i < g->m_spheres.GetCount(); i++)
			{
				for (int j = i + 1; j < g->m_spheres.GetCount(); j++)
				{
					if (stricmp(g->m_spheres[i].m_name.c_str(), g->m_spheres[j].m_name.c_str()) == 0)
					{
						const bool nopopups = PARAM_nopopups.Get();
						PARAM_nopopups.Set(NULL);
						char msg[512] = "";
						sprintf(msg, "Name collisions found in %s - spheres %d and %d both have the name '%s' .. please fix meta file and reload", g_metafilePath, i, j, g->m_spheres[i].m_name.c_str());
						fiRemoteShowMessageBox(msg, sysParam::GetProgramName(), MB_ICONERROR | MB_OK | MB_TOPMOST, IDOK);
						PARAM_nopopups.Set(nopopups ? "" : NULL);

						g->m_spheres.clear();
						g_selectedSphereIndex = -1;
						UpdateSelectedSphereIndex_cb();
						UpdateCombo_cb();
						delete g;
						g = NULL;
						break;
					}
				}

				if (g == NULL)
				{
					break;
				}
			}

			for (int i = 0; i < g->m_spheres.GetCount(); i++)
			{
				if (g->m_spheres[i].m_height == SGC_DEFAULT_HEIGHT)
				{
					g->m_spheres[i].m_height = g->m_spheres[i].m_sphere.GetZf();
				}
			}

			if (g->m_panoramaPath.length() == 0) { g->m_panoramaPath = SGC_DEFAULT_PATH; }
			if (g->m_geometryPath.length() == 0) { g->m_geometryPath = SGC_DEFAULT_PATH; }

#if SGC_PANORAMA
			strcpy(g_capturePanoramaPath, g->m_panoramaPath.c_str());
#endif // SGC_PANORAMA
			strcpy(g_captureGeometryPath, g->m_geometryPath.c_str());
		}
		else if (bCreated)
		{
			Displayf("%s failed to load", g_metafilePath);
			delete g;
			g = NULL;
		}
	}
	else
	{
		Displayf("%s does not exist", g_metafilePath);
	}
}

static void SaveMetafile_button()
{
	if (g)
	{
#if SGC_PANORAMA
		g->m_panoramaPath = g_capturePanoramaPath;
#else
		g->m_panoramaPath = "";
#endif
		g->m_geometryPath = g_captureGeometryPath;

		if (PARSER.SaveObject(g_metafilePath, NULL, g))
		{
			Displayf("%s saved (%d spheres)", g_metafilePath, g->m_spheres.GetCount());
		}
		else
		{
			Displayf("%s failed to load", g_metafilePath);
		}
	}
}

static void SelectedSphereCombo_cb()
{
	if (g_selectedSphereComboIndex >= 0 && g_selectedSphereComboIndex < g_selectedSphereComboList.GetCount())
	{
		g_selectedSphereIndex = g_selectedSphereComboList[g_selectedSphereComboIndex];
		UpdateSelectedSphereIndex_cb();
	}
}

static void UpdateCombo_cb()
{
	if (g)
	{
		int prevSelectedSphereIndex = -1;

		if (g_selectedSphereComboIndex >= 0 && g_selectedSphereComboIndex < g_selectedSphereComboList.GetCount())
		{
			prevSelectedSphereIndex = g_selectedSphereComboList[g_selectedSphereComboIndex];
		}

		atArray<const char*> names;
		char* emptyNameCopy = rage_new char[1];
		strcpy(emptyNameCopy, "");
		names.PushAndGrow(emptyNameCopy);
		g_selectedSphereComboList.Reset();
		g_selectedSphereComboList.PushAndGrow(-1);

		for (int i = 0; i < g->m_spheres.GetCount(); i++)
		{
			if (g_selectedSphereComboFilter[0] == '\0' || stristr(g->m_spheres[i].m_name.c_str(), g_selectedSphereComboFilter))
			{
				char temp[256] = "";
				sprintf(temp, "%d. %s", i, g->m_spheres[i].m_name.c_str());
				char* nameCopy = rage_new char[strlen(temp) + 1];
				strcpy(nameCopy, temp);
				names.PushAndGrow(nameCopy);
				g_selectedSphereComboList.PushAndGrow(i);
			}
		}

		g_selectedSphereComboIndex = 0;

		for (int i = 0; i < g_selectedSphereComboList.GetCount(); i++)
		{
			if (g_selectedSphereComboList[i] == prevSelectedSphereIndex)
			{
				g_selectedSphereComboIndex = i;
				break;
			}
		}

		g_selectedSphereCombo->UpdateCombo(NULL, &g_selectedSphereComboIndex, names.GetCount(), &names[0], SelectedSphereCombo_cb);

		for (int i = 0; i < names.GetCount(); i++)
		{
			delete[] names[i];
		}
	}
}

static void WarpToSphere(int sphereIndex, float warpHeight, bool warpHeightIsRelative)
{
	if (g && sphereIndex >= 0 && sphereIndex < g->m_spheres.GetCount())
	{
		const CSceneGeometryCapture::Sphere& sphere = g->m_spheres[sphereIndex];
		CPed* pPlayerPed = FindPlayerPed();

		if (pPlayerPed)
		{
			pPlayerPed->Teleport(VEC3V_TO_VECTOR3(sphere.m_sphere.GetXYZ()), 0.0f);
		}

		Mat34V camMtx;

		if (g_warpToCameraHeight
#if SGC_PANORAMA
			|| g_capturePanoramaEnabled
#endif // SGC_PANORAMA
			)
		{
			const Vec3V camFront = Vec3V(V_Y_AXIS_WZERO);
			const Vec3V camUp    = Vec3V(V_Z_AXIS_WZERO);
			const Vec3V camLeft  = Vec3V(V_X_AXIS_WZERO);
			const Vec3V camPos   = Vec3V(sphere.m_sphere.GetXY(), ScalarV(sphere.m_height + g_cameraHeightOffset));

			camMtx = Mat34V(camLeft, camFront, camUp, camPos);
		}
		else
		{
			const Vec3V camFront = -Vec3V(V_Z_AXIS_WZERO);
			const Vec3V camUp    = Vec3V(V_Y_AXIS_WZERO);
			const Vec3V camLeft  = Cross(camFront, camUp);
			const Vec3V camPos   = sphere.m_sphere.GetXYZ() + Vec3V(0.0f, 0.0f, warpHeight*(warpHeightIsRelative ? sphere.m_sphere.GetWf() : 1.0f));

			camMtx = Mat34V(camLeft, camFront, camUp, camPos);
		}

		camInterface::GetDebugDirector().ActivateFreeCam();
		camInterface::GetDebugDirector().GetFreeCamFrameNonConst().SetWorldMatrix(RCC_MATRIX34(camMtx), false); // set both orientation and position
	}
}

static void WarpToSelectedSphere_button()
{
	WarpToSphere(g_selectedSphereIndex, 1.6f, true);
}

static void WarpToAndSelectNext_button()
{
	if (g && g_selectedSphereComboIndex < g_selectedSphereComboList.GetCount() - 1)
	{
		const int sphereIndex = g_selectedSphereComboList[++g_selectedSphereComboIndex];

		if (sphereIndex >= 0 && sphereIndex < g->m_spheres.GetCount())
		{
			g_selectedSphereIndex = sphereIndex;
		}
		else
		{
			g_selectedSphereIndex = -1;
		}

		UpdateSelectedSphereIndex_cb();
		WarpToSelectedSphere_button();
	}
}

static void SelectClosestSphere_button()
{
	if (g && g->m_spheres.GetCount() > 0)
	{
		const grcViewport* vp = gVpMan.GetCurrentGameGrcViewport();
		const Vec3V camPos = vp->GetCameraPosition();
		ScalarV distMin1(V_FLT_MAX); // distance to centre
		ScalarV distMin2(V_FLT_MAX); // distance to surface
		int index = -1;

		for (int i = 0; i < g->m_spheres.GetCount(); i++)
		{
			const CSceneGeometryCapture::Sphere& sphere = g->m_spheres[i];

			if (vp->IsSphereVisible(sphere.m_sphere))
			{
				const ScalarV dist1 = Mag(sphere.m_sphere.GetXYZ() - camPos);
				const ScalarV dist2 = Max(ScalarV(V_ZERO), dist1 - sphere.m_sphere.GetW());

				if (IsLessThanOrEqualAll(dist2, distMin2))
				{
					if (IsLessThanAll(dist2, distMin2) | IsLessThanAll(dist1, distMin1))
					{
						distMin1 = dist1;
						distMin2 = dist2;
						index = i;
					}
				}
			}
		}

		if (index != -1)
		{
			g_selectedSphereIndex = index;
			UpdateSelectedSphereIndex_cb();
		}
		else
		{
			Displayf("no spheres are visible");
		}
	}
}

// copied from RenderPhaseCascadeShadows.cpp (TODO -- move this function to GtaPicker)
static FASTRETURNCHECK(bool) _GetEntityApproxAABB(spdAABB& aabb, const CEntity* pEntity)
{
	if (pEntity && !pEntity->GetIsRetainedByInteriorProxy())
	{
		fwBaseEntityContainer* pContainer = pEntity->GetOwnerEntityContainer();
		if (pContainer)
		{
			const u32 index = pContainer->GetEntityDescIndex(pEntity);
			if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
			{
				pContainer->GetApproxBoundingBox(index, aabb);
				return true;
			}
		}
	}

	return false;
}

static Vec4V g_AddNewSphere(V_ZERO);
static char  g_AddNewSphereName[128] = "";

static void AddNewSphere_button()
{
	if (g == NULL)
	{
		g = rage_new CSceneGeometryCapture();
	}

	if (g->m_spheres.GetCount() >= SGC_MAX_SPHERES)
	{
		Displayf("too many spheres - max is %d", SGC_MAX_SPHERES);
		return;
	}

	const char* name = SGC_DEFAULT_NAME;
	Color32 colour = SGC_DEFAULT_COLOUR;
	
	if (g_AddNewSphereName[0])
	{
		name = g_AddNewSphereName;
	}
	else if (g_selectedSphereIndex >= 0 && g_selectedSphereIndex < g->m_spheres.GetCount())
	{
		name   = g->m_spheres[g_selectedSphereIndex].m_name.c_str();
		colour = g->m_spheres[g_selectedSphereIndex].m_colour;
	}
	else
	{
		name   = g_selectedSphereName;
		colour = g_selectedSphereColour;
	}

	char newName[128] = "";
	strcpy(newName, name);
	MakeNameUnique(newName);

	g->m_spheres.PushAndGrow(CSceneGeometryCapture::Sphere(newName));
	g_selectedSphereIndex = g->m_spheres.GetCount() - 1;

	CSceneGeometryCapture::Sphere& sphere = g->m_spheres[g_selectedSphereIndex];

	if (IsZeroAll(g_AddNewSphere))
	{
		spdAABB aabb;
		if (_GetEntityApproxAABB(aabb, (CEntity*)g_PickerManager.GetSelectedEntity()))
		{
			sphere.m_sphere = Vec4V(aabb.GetCenter(), Mag(aabb.GetExtent()));
		}
		else
		{
			sphere.m_sphere = Vec4V(gVpMan.GetCurrentGameGrcViewport()->GetCameraPosition(), ScalarV(SGC_DEFAULT_RADIUS));
		}
	}
	else
	{
		sphere.m_sphere = g_AddNewSphere;
	}

	sphere.m_height = sphere.m_sphere.GetZf();
	sphere.m_colour = colour;

	UpdateSelectedSphereIndex_cb();
	UpdateCombo_cb();

	for (int i = 0; i < g_selectedSphereComboList.GetCount(); i++)
	{
		if (g_selectedSphereComboList[i] == g_selectedSphereIndex)
		{
			g_selectedSphereComboIndex = i;
			break;
		}
	}
}

static void DeleteSelectedSphere_button()
{
	if (g && g_selectedSphereIndex >= 0 && g_selectedSphereIndex < g->m_spheres.GetCount())
	{
		for (int i = g_selectedSphereIndex + 1; i < g->m_spheres.GetCount(); i++)
		{
			g->m_spheres[i - 1] = g->m_spheres[i];
		}

		g->m_spheres.Pop();

		if (g_selectedSphereIndex >= g->m_spheres.GetCount())
		{
			g_selectedSphereIndex = -1;
		}

		UpdateSelectedSphereIndex_cb();
		UpdateCombo_cb();
	}
}

static void DeleteAllSpheres_button()
{
	if (g)
	{
		const bool nopopups = PARAM_nopopups.Get();
		PARAM_nopopups.Set(NULL);
		const int result = fiRemoteShowMessageBox("Are you sure you want to delete all spheres?", sysParam::GetProgramName(), MB_ICONQUESTION | MB_OKCANCEL | MB_TOPMOST, IDCANCEL);
		PARAM_nopopups.Set(nopopups ? "" : NULL);

		if (result == IDOK)
		{
			const int numSpheres = g->m_spheres.GetCount();
			g->m_spheres.clear();
			g_selectedSphereIndex = -1;
			UpdateSelectedSphereIndex_cb();
			UpdateCombo_cb();
			delete g;
			g = NULL;
			Displayf("%d spheres deleted", numSpheres);
		}
		else
		{
			Displayf("aborted");
		}
	}
}

#if SGC_PRIVATE_RDR_STUFF
static void AddAreaCodeSpheres_button()
{
	class CAreaCodeBounds : public spdAABB
	{
	public:
		char m_areaCode[4];
		bool m_used;
	};
	atArray<CAreaCodeBounds> bounds;
	char line[512] = "";

	// load area bounds
	{
		fiStream* fp = fiStream::Open("assets:/non_final/rdr3_area_bounds.txt");

		if (fp)
		{
			// e.g.: area 'lev' bounds = [3885.510742,-3909.626709,35.058342],[4099.395020,-3589.768311,107.753555], centre = 3992.452881,-3749.697510,71.405945, radius = 195.793564
			while (fgetline(line, sizeof(line), fp))
			{
				const char* temp = strstr(line, "area '");
				if (temp == NULL) continue;
				temp += strlen("area '");
				if (temp[3] != '\'') continue;
				const char* areaCode = temp;
				temp = strstr(temp, "bounds = [");
				if (temp == NULL) continue;
				const char* x0 = temp + strlen("bounds = [");
				temp = strchr(x0, ','); if (temp == NULL) continue; const char* y0 = temp + 1;
				temp = strchr(y0, ','); if (temp == NULL) continue; const char* z0 = temp + 1;
				temp = strchr(z0, '['); if (temp == NULL) continue; const char* x1 = temp + 1;
				temp = strchr(x1, ','); if (temp == NULL) continue; const char* y1 = temp + 1;
				temp = strchr(y1, ','); if (temp == NULL) continue; const char* z1 = temp + 1;

				CAreaCodeBounds ab;
				memcpy(ab.m_areaCode, areaCode, 3);
				ab.m_areaCode[3] = '\0';
				const Vec3V bmin = Vec3V((float)atof(x0), (float)atof(y0), (float)atof(z0));
				const Vec3V bmax = Vec3V((float)atof(x1), (float)atof(y1), (float)atof(z1));
				ab.Set(bmin, bmax);
				ab.m_used = false;
				bounds.PushAndGrow(ab);
			}

			fp->Close();
		}
	}

	// load area names
	{
		fiStream* fp = fiStream::Open("assets:/non_final/rdr3_area_names.txt");

		if (fp)
		{
			// e.g.:     area_01/adl - K08     - The Grizzlies (intro) - Adler Ranch
			while (fgetline(line, sizeof(line), fp))
			{
				const char* temp = strchr(line, '/');
				if (temp == NULL) continue;
				temp++;
				CAreaCodeBounds* ab = NULL;

				if (isalpha(temp[0]) && isalpha(temp[1]) && isalpha(temp[2]) && temp[3] == ' ')
				{
					for (int i = 0; i < bounds.GetCount(); i++)
					{
						if (memcmp(bounds[i].m_areaCode, temp, 3) == 0)
						{
							ab = &bounds[i];
							break;
						}
					}
				}
				else
				{
					continue;
				}

				temp = strstr(temp, " - "); // grid
				if (temp == NULL) continue;
				temp += strlen(" - ");
				const int row = *(temp++);
				if (row < 'F' || row > 'U') continue;
				const int col = atoi(temp);
				if (col < 2 || col > 18) continue;

				temp = strstr(temp, " - "); // district name
				if (temp == NULL) continue;
				temp += strlen(" - ");

				temp = strstr(temp, " - "); // area name
				if (temp == NULL) continue;
				temp += strlen(" - ");

				char areaName[256] = "";
				strcpy(areaName, temp);
				for (char* s = areaName; *s; s++) { if (strchr("\\/:*?\"<>|", *s)) { *s = '_'; } }

				float x = (float)gv::WORLD_BOUNDS_MIN_X + (float)gv::WORLD_TILE_SIZE*(1.0f + 2.0f*(float)(col - 2));
				float y = (float)gv::WORLD_BOUNDS_MIN_Y + (float)gv::WORLD_TILE_SIZE*(1.0f + 2.0f*(float)('U' - row));
				float z = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(x, y);
				float r = 80.0f;

				if (ab)
				{
					x = ab->GetCenter().GetXf();
					y = ab->GetCenter().GetYf();
					z = ab->GetCenter().GetZf();
					r = Mag(ab->GetExtent()).Getf();
					ab->m_used = true;
				}

				g_AddNewSphere = Vec4V(x, y, z, r);
				strcpy(g_AddNewSphereName, areaName);

				bool bUnique = true;

				if (g)
				{
					for (int i = 0; i < g->m_spheres.GetCount(); i++)
					{
						if (IsEqualAll(g_AddNewSphere, g->m_spheres[i].m_sphere))
						{
							Displayf("skipping '%s' %sbecause it coincides with sphere %d", areaName, ab ? "" : "(no bounds) ", i);
							bUnique = false; // don't add duplicate spheres - our area positions are quantised to tile grid, so not very accurate ..
							break;
						}
					}
				}

				if (bUnique)
				{
					Displayf("adding '%s' %s@ %f,%f,%f (r=%f)", g_AddNewSphereName, ab ? "" : "(no bounds) ", VEC4V_ARGS(g_AddNewSphere));
					AddNewSphere_button();

					if (g && strcmp(g->m_spheres.Top().m_name.c_str(), g_AddNewSphereName) == 0)
					{
						if (ab)
						{
							strcpy(g->m_spheres.Top().m_areaCode, ab->m_areaCode);
						}
						else
						{
							g->m_spheres.Top().m_colour = Color32(255,0,0,255);
						}
					}
				}
			}

			fp->Close();
		}
	}

	for (int i = 0; i < bounds.GetCount(); i++)
	{
		if (!bounds[i].m_used)
		{
			g_AddNewSphere = Vec4V(bounds[i].GetCenter(), Mag(bounds[i].GetExtent()));
			strcpy(g_AddNewSphereName, bounds[i].m_areaCode);

			Displayf("adding '%s' @ %f,%f,%f (r=%f)", g_AddNewSphereName, VEC4V_ARGS(g_AddNewSphere));
			AddNewSphere_button();

			if (g && strcmp(g->m_spheres.Top().m_name.c_str(), g_AddNewSphereName) == 0)
			{
				strcpy(g->m_spheres.Top().m_areaCode, bounds[i].m_areaCode);
			}
		}
	}

	g_AddNewSphere = Vec4V(V_ZERO);
	g_AddNewSphereName[0] = '\0';

	g_selectedSphereComboIndex = 0;
}
#endif // SGC_PRIVATE_RDR_STUFF

CDumpGeometryToOBJ* g_obj = NULL;

static bool CaptureGeometryBegin()
{
	return true;
}

static void CaptureGeometry(int sphereIndex)
{
	if (!AssertVerify(g_obj == NULL))
	{
		delete g_obj;
	}

	g_obj = rage_new CDumpGeometryToOBJ(atVarString("%s/%s.obj", g_captureGeometryPath, g->m_spheres[sphereIndex].m_name.c_str()).c_str());
	
	static spdSphere s_sphere;
	static int s_sphereIndex = -1;
	static int s_entityIndex = 0;

	class p { public: static bool func(void* item, void*)
	{
		if (item)
		{
			const CEntity* pEntity = static_cast<CEntity*>(item);
			const CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
			
			if (pModelInfo && (g_captureEntityLodFlags & BIT(pEntity->GetLodData().GetLodType())) != 0)
			{
				if (!g_captureProps && pModelInfo->GetIsProp())
				{
					return false;
				}

				if (!g_captureTrees && pModelInfo->GetIsTree())
				{
					return false;
				}

				//if (!g_captureTerrain && pModelInfo->GetIsTerrain())
				//{
				//	return false;
				//}

				static int s_actualNumTriangles = 0;
				static int s_numTrianglesConsidered = 0;

				if (pEntity->GetIsTypePed())
				{
					// TODO -- see CutSceneManager::CaptureSceneToObj for how to capture ped geometry
				}
			//	else if (pEntity->GetIsTypeInstanceList() || pEntity->GetIsTypeGrassInstanceList())
			//	{
			//		// TODO
			//	}
				else
				{
					spdAABB aabb;
					pEntity->GetAABB(aabb);

					if (aabb.IntersectsSphere(s_sphere))
					{
						class AddTrianglesForEntity_geometry { public: static bool func(const grmModel& model, const grmGeometry&, const grmShader& shader, const grmShaderGroup& shaderGroup, int lodIndex, void*)
						{
							if (g_captureDrawableLodFlags & BIT(lodIndex))
							{
								if (!g_captureAlpha && false) // TODO -- use CRenderer::RB_ALPHA, not shader name
								{
									return false;
								}

								if (!g_captureWater && strstr(shader.GetName(), "water")) // TODO -- use CRenderer::RB_WATER, not shader name
								{
									return false;
								}

								if (!g_captureDecals && strstr(shader.GetName(), "decal")) // TODO -- use CRenderer::RB_DECAL, not shader name
								{
									return false;
								}

								const u32 mask = CRenderer::GetSubBucketMask(model.ComputeBucketMask(shaderGroup));

								if (mask & CRenderer::GenerateSubBucketMask(CRenderer::RB_MODEL_DEFAULT))
								{
									return true;
								}
							}

							return false;
						}};

						class AddTrianglesForEntity_triangle { public: static void func(Vec3V_In v0, Vec3V_In v1, Vec3V_In v2, int, int, int, void*)
						{
							if (g_captureClipToSpheres)
							{
								const Vec3V verts[3] = {v0, v1, v2};
								const Vec3V faceNormal = NormalizeSafe(Cross(v1 - v0, v2 - v0), Vec3V(V_ZERO), Vec3V(V_FLT_SMALL_4));

								if (IsZeroAll(faceNormal)) // if the triangle is nearly degenerate, just test the bounding box ..
								{
									if (!spdAABB(Min(v0, v1, v2), Max(v0, v1, v2)).IntersectsSphere(s_sphere))
									{
										return;
									}
								}
								else if (!geomSpheres::TestSphereToTriangle(s_sphere.GetV4(), verts, faceNormal)) // .. otherwise do the proper sphere-triangle test
								{
									return;
								}
							}

							g_obj->AddTriangle(v0, v1, v2);
							s_actualNumTriangles++;
						}};

						g_obj->GroupBegin(atVarString("%s_%04d", pEntity->GetModelName(), s_entityIndex++).c_str());
						s_actualNumTriangles = 0;
						s_numTrianglesConsidered = GeometryUtil::AddTrianglesForEntity(pEntity, -1, AddTrianglesForEntity_geometry::func, AddTrianglesForEntity_triangle::func);
						g_obj->GroupEnd();
					}
				}

				if (g_captureVerbose)
				{
					Displayf("sphere %d added %d out of %d triangles for %s at %f,%f,%f", s_sphereIndex, s_actualNumTriangles, s_numTrianglesConsidered, pEntity->GetModelName(), VEC3V_ARGS(pEntity->GetTransform().GetPosition()));
				}
			}
		}

		return true;
	}};

	s_sphere = spdSphere(g->m_spheres[sphereIndex].m_sphere);
	s_sphereIndex = sphereIndex;
	s_entityIndex = 0;

	// add origin group
	g_obj->GroupBegin("origin");
	g_obj->AddTriangle(s_sphere.GetCenter(), s_sphere.GetCenter(), s_sphere.GetCenter());
	g_obj->GroupEnd();

	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_BUILDING           ) { CBuilding        ::GetPool()->ForAll(p::func, NULL); }
	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_ANIMATED_BUILDING  ) { CAnimatedBuilding::GetPool()->ForAll(p::func, NULL); }
	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_OBJECT             ) { CObject          ::GetPool()->ForAll(p::func, NULL); }
	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_DUMMY_OBJECT       ) { CDummyObject     ::GetPool()->ForAll(p::func, NULL); }
	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_MLO                ) { CInteriorInst    ::GetPool()->ForAll(p::func, NULL); }
	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_PED                ) { CPed             ::GetPool()->ForAll(p::func, NULL); }
	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_VEHICLE            ) { CVehicle         ::GetPool()->ForAll(p::func, NULL); }
//	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_INSTANCE_LIST      ) { CEntityBatch     ::GetPool()->ForAll(p::func, NULL); }
//	if (g_captureEntityPoolFlags & ENTITY_TYPE_MASK_GRASS_INSTANCE_LIST) { CGrassBatch      ::GetPool()->ForAll(p::func, NULL); }
}

static void CaptureGeometryEnd()
{
	if (AssertVerify(g_obj))
	{
		g_obj->Close();
		g_obj = NULL;
	}
}

static void CaptureGeometrySelectedSphere_button()
{
	if (g && g_selectedSphereIndex >= 0 && g_selectedSphereIndex < g->m_spheres.GetCount())
	{
		if (CaptureGeometryBegin())
		{
			CaptureGeometry(g_selectedSphereIndex);
			CaptureGeometryEnd();
		}
	}
}

template <bool bSelectedSphereOnly> static void CaptureGeometryStart_button()
{
	if (bSelectedSphereOnly && !g_capturePanoramaEnabled)
	{
		CaptureGeometrySelectedSphere_button();
		return;
	}

	if (g && g_captureAutoListIndex == -1)
	{
		g_captureSelectedSphereOnly = bSelectedSphereOnly;
		g_captureAutoListIndex = 1;
		g_captureAutoSphereIndex = -1;
		g_captureAutoStreamFrames = 0;
#if SGC_PANORAMA
		g_capturePanoramaFrameIndex = -1;
		g_capturePanoramaAutoTimer = -1;
#endif // SGC_PANORAMA
		Displayf("starting auto capture ...");

		if (g_capturePanoramaEnabled)
		{
			CMarketingTools::WidgetHudHideAll();
		}

		camInterface::GetDebugDirector().ActivateFreeCam();
		fwTimer::StartUserPause();
	}
}

static void CaptureGeometryStop_button()
{
	if (g && g_captureAutoListIndex != -1)
	{
		g_captureAutoListIndex = -1;
		g_captureAutoSphereIndex = -1;
		g_captureAutoStreamFrames = 0;
#if SGC_PANORAMA
		g_capturePanoramaFrameIndex = -1;
		g_capturePanoramaAutoTimer = -1;
#endif // SGC_PANORAMA
		Displayf("auto capture stopped.");

		if (g_capturePanoramaEnabled)
		{
			CMarketingTools::WidgetHudShowAll();
		}

		g_LoadScene.Stop();
	}
}

static void CaptureGeometryUpdate()
{
	if (g && g_captureAutoListIndex >= 0)
	{
		if (g_captureSelectedSphereOnly)
		{
			while (g_captureAutoListIndex < g_selectedSphereComboList.GetCount())
			{
				if (g_selectedSphereComboList[g_captureAutoListIndex] != g_selectedSphereIndex)
				{
					g_captureAutoListIndex++;
				}
				else
				{
					break;
				}
			}
		}

		if (g_captureAutoListIndex < g_selectedSphereComboList.GetCount())
		{
#if SGC_PANORAMA
			if (g_capturePanoramaFrameIndex >= 0) // capturing panoramic frame
			{
				if (!g_LoadScene.IsLoaded())
				{
					// wait for streaming ..
				}
				else if (g_capturePanoramaAutoTimer > 0)
				{
					g_capturePanoramaAutoTimer--; // wait for rendering ..
				}
				else
				{
					grcImage* pScreenImage = GRCDEVICE.CaptureScreenShot(NULL);

					if (AssertVerify(pScreenImage))
					{
						pScreenImage->SavePNG(atVarString("%s/%s_%03d.png", g_capturePanoramaPath, g->m_spheres[g_captureAutoSphereIndex].m_name.c_str(), g_capturePanoramaFrameIndex).c_str(), 0.0f);
						pScreenImage->Release();
					}

					g_capturePanoramaFrameIndex++;

					if (g_capturePanoramaFrameIndex < g_capturePanoramaSteps)
					{
						g_capturePanoramaCameraFacing = (360.0f/(float)g_capturePanoramaSteps)*(float)g_capturePanoramaFrameIndex;
						UpdateCameraFacing_cb();

						Displayf("starting panoramic frame %d", g_capturePanoramaFrameIndex);
						const Vec3V camPos = +gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol3();
						const Vec3V camDir = -gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol2();
						g_LoadScene.Start(camPos, camDir, 16000.0f, true, 0, CLoadScene::LOADSCENE_OWNER_DEBUG);
						g_capturePanoramaAutoTimer = g_capturePanoramaAutoTimerDelay;
					}
					else
					{
						Displayf("panorama captured.");
						g_capturePanoramaFrameIndex = -1; // done capturing panoramic frames .. next update we'll advance the sphere

						g_LoadScene.Stop();
						g_captureAutoListIndex++;
						g_captureAutoSphereIndex = -1;
					}
				}
			}
			else
#endif // SGC_PANORAMA
			if (g_captureAutoStreamFrames == 0) // warp to next sphere
			{
				g_captureAutoSphereIndex = g_selectedSphereComboList[g_captureAutoListIndex];

				if (!AssertVerify(g_captureAutoSphereIndex >= 0 && g_captureAutoSphereIndex < g->m_spheres.GetCount()))
				{
					// something happened, maybe the combo list changed?
					Displayf("sphere index %d is out of range - should be [0..%d], auto capture stopped", g_captureAutoSphereIndex, g->m_spheres.GetCount() - 1);
					g_captureAutoListIndex = -1;
					g_captureAutoSphereIndex = -1;
					return;
				}

				const Vec4V sphere = g->m_spheres[g_captureAutoSphereIndex].m_sphere;

				WarpToSphere(g_captureAutoSphereIndex, 25.0f, false);

				g_LoadScene.Start(sphere.GetXYZ(), Vec3V(V_ZERO), sphere.GetWf(), false, 0, CLoadScene::LOADSCENE_OWNER_DEBUG);
				g_captureAutoStreamFrames = 1;
				Displayf("teleported to sphere %d '%s' at %f,%f,%f (r=%f)", g_captureAutoSphereIndex, g->m_spheres[g_captureAutoSphereIndex].m_name.c_str(), VEC4V_ARGS(sphere));
			}
			else if (g_LoadScene.IsLoaded()) // warp is complete .. capture geometry
			{
				if (CaptureGeometryBegin())
				{
					CaptureGeometry(g_captureAutoSphereIndex);
					CaptureGeometryEnd();
				}

				Displayf("captured sphere %d '%s' (streamed in %d frames)", g_captureAutoSphereIndex, g->m_spheres[g_captureAutoSphereIndex].m_name.c_str(), g_captureAutoStreamFrames);
				g_captureAutoStreamFrames = 0;
#if SGC_PANORAMA
				if (g_capturePanoramaEnabled)
				{
					g_capturePanoramaFrameIndex = 0;
					g_capturePanoramaCameraFacing = 0.0f;
					UpdateCameraFacing_cb();

					Displayf("starting panoramic frame %d", g_capturePanoramaFrameIndex);
					const Vec3V camPos = +gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol3();
					const Vec3V camDir = -gVpMan.GetCurrentGameGrcViewport()->GetCameraMtx().GetCol2();
					g_LoadScene.Start(camPos, camDir, 16000.0f, true, 0, CLoadScene::LOADSCENE_OWNER_DEBUG);
					g_capturePanoramaAutoTimer = g_capturePanoramaAutoTimerDelay;
				}
				else
#endif // SGC_PANORAMA
				{
					g_LoadScene.Stop();
					g_captureAutoListIndex++;
					g_captureAutoSphereIndex = -1;
				}
			}
			else
			{
				g_captureAutoStreamFrames++;
			}
		}
		else
		{
			CaptureGeometryStop_button();
		}
	}
}

} // namespace SceneGeometryCapture

// ================================================================================================

//PARAM(SceneGeometryCapture,"");
void SceneGeometryCapture::AddWidgets()
{
	//if (PARAM_SceneGeometryCapture.Get())
	{
		bkBank& bk = BANKMGR.CreateBank("Scene Geometry Capture");

		bk.AddText("Metafile Path", &g_metafilePath[0], sizeof(g_metafilePath), false);
		bk.AddButton("Load Metafile", LoadMetafile_button);
		bk.AddButton("Save Metafile", SaveMetafile_button);
#if SGC_PANORAMA
		bk.AddToggle("Capture Panorama Enabled", &g_capturePanoramaEnabled);
		bk.AddToggle("Capture Panorama Show FPS", &g_capturePanoramaShowFPS);
		bk.AddVector(" -- fps text position", &g_capturePanoramaShowFPSPos, 0.0f, 2000.0f, 1.0f);
		bk.AddColor (" -- fps text colour", &g_capturePanoramaShowFPSColour);
		bk.AddToggle(" -- fps background quad", &g_capturePanoramaShowFPSBkgQuad);
		bk.AddSlider(" -- fps text scale", &g_capturePanoramaShowFPSScale, 1.0f, 16.0f, 1.0f/32.0f);
		bk.AddSlider("Capture Panorama Steps", &g_capturePanoramaSteps, 1, 64, 1);
		bk.AddSlider("Capture Panorama Delay", &g_capturePanoramaAutoTimerDelay, 0, 10, 1);
		bk.AddSlider("Capture Panorama Facing", &g_capturePanoramaCameraFacing, 0.0f, 360.0f, 30.0f, UpdateCameraFacing_cb);
		bk.AddText("Capture Panorama Path", &g_capturePanoramaPath[0], sizeof(g_capturePanoramaPath), false);
#endif // SGC_PANORAMA
		bk.AddText("Capture Geometry Path", &g_captureGeometryPath[0], sizeof(g_captureGeometryPath), false);
		bk.AddButton("Capture Geometry Start", CaptureGeometryStart_button<false>);
		bk.AddButton("Capture Geometry Stop", CaptureGeometryStop_button);
		bk.AddButton("Capture Geometry (selected sphere only)", CaptureGeometryStart_button<true>);//CaptureGeometrySelectedSphere_button);
		bk.AddTitle("");
		const char* strings[] = {""};
		g_selectedSphereComboList.PushAndGrow(-1);
		bk.AddText("Filter", &g_selectedSphereComboFilter[0], sizeof(g_selectedSphereComboFilter), false, UpdateCombo_cb);
		g_selectedSphereCombo = bk.AddCombo("Selected Sphere", &g_selectedSphereComboIndex, NELEM(strings), strings, SelectedSphereCombo_cb);
		bk.AddText("Selected Sphere Name", &g_selectedSphereName[0], sizeof(g_selectedSphereName), false, UpdateSelectedSphereName_cb);
		bk.AddVector("Selected Sphere Centre", &g_selectedSphereCentre, -16000.0f, 16000.0f, 1.0f/32.0f, UpdateSelectedSphereCamera_cb);
		bk.AddSlider("Selected Sphere Radius", &g_selectedSphereRadius, 0.0f, 64.0f, 1.0f/32.0f, UpdateSelectedSphere_cb);
		bk.AddSlider("Selected Sphere Height", &g_selectedSphereHeight, (float)gv::WORLD_BOUNDS_MIN_Z, (float)gv::WORLD_BOUNDS_MAX_Z, 1.0f/32.0f, UpdateSelectedSphereCamera_cb);
		bk.AddButton("Selected Sphere Snap to Ground", SelectedSphereSnapToGround_button);
		bk.AddColor("Selected Sphere Colour", &g_selectedSphereColour, UpdateSelectedSphere_cb);
		bk.AddSlider("Selected Sphere Index", &g_selectedSphereIndex, -1, SGC_MAX_SPHERES - 1, 1, UpdateSelectedSphereIndex_cb);
		bk.AddTitle("");
		bk.AddToggle("Warp to Camera Height", &g_warpToCameraHeight, UpdateSelectedSphereCameraHeight_cb);
		bk.AddSlider("Camera Height Offset", &g_cameraHeightOffset, -100.0f, 100.0f, 1.0f/32.0f, UpdateSelectedSphereCameraHeight_cb);
		bk.AddButton("Warp to Selected Sphere", WarpToSelectedSphere_button);
		bk.AddButton("Warp to and Select Next", WarpToAndSelectNext_button);
		bk.AddButton("Select Closest Sphere", SelectClosestSphere_button);
		bk.AddButton("Add New Sphere", AddNewSphere_button);
		bk.AddButton("Delete Selected Sphere", DeleteSelectedSphere_button);
		bk.AddButton("Delete All Spheres", DeleteAllSpheres_button);
#if SGC_PRIVATE_RDR_STUFF
		bk.AddTitle("");
		bk.AddButton("*** Start Area Codes", CStreamingIteratorManager::StartAreaCodeBoundsCollection);
		bk.AddButton("*** Add Area Codes", AddAreaCodeSpheres_button);
#endif // SGC_PRIVATE_RDR_STUFF
		bk.AddTitle("");
		bk.AddToggle("Capture Entity Type - Buildings", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_BUILDING);
		bk.AddToggle("Capture Entity Type - Animated Buildings", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_ANIMATED_BUILDING);
		bk.AddToggle("Capture Entity Type - Objects", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_OBJECT);
		bk.AddToggle("Capture Entity Type - Dummy Objects", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_DUMMY_OBJECT);
		bk.AddToggle("Capture Entity Type - MLO Interiors", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_MLO);
		bk.AddToggle("Capture Entity Type - Peds", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_PED);
		bk.AddToggle("Capture Entity Type - Vehicles", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_VEHICLE);
	//	bk.AddToggle("Capture Entity Type - Instances", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_INSTANCE_LIST);
	//	bk.AddToggle("Capture Entity Type - Grass Instances", &g_captureEntityPoolFlags, ENTITY_TYPE_MASK_GRASS_INSTANCE_LIST);
		bk.AddSeparator("");
		bk.AddToggle("Capture Entity Lod - HD", &g_captureEntityLodFlags, LODTYPES_FLAG_HD);
		bk.AddToggle("Capture Entity Lod - ORPHANHD", &g_captureEntityLodFlags, LODTYPES_FLAG_ORPHANHD);
		bk.AddToggle("Capture Entity Lod - LOD", &g_captureEntityLodFlags, LODTYPES_FLAG_LOD);
		bk.AddToggle("Capture Entity Lod - SLOD1", &g_captureEntityLodFlags, LODTYPES_FLAG_SLOD1);
		bk.AddToggle("Capture Entity Lod - SLOD2", &g_captureEntityLodFlags, LODTYPES_FLAG_SLOD2);
		bk.AddToggle("Capture Entity Lod - SLOD3", &g_captureEntityLodFlags, LODTYPES_FLAG_SLOD3);
		bk.AddToggle("Capture Entity Lod - SLOD4", &g_captureEntityLodFlags, LODTYPES_FLAG_SLOD4);
		bk.AddSeparator("");
		bk.AddToggle("Capture Drawable Lod - HIGH", &g_captureDrawableLodFlags, BIT(LOD_HIGH));
		bk.AddToggle("Capture Drawable Lod - MED", &g_captureDrawableLodFlags, BIT(LOD_MED));
		bk.AddToggle("Capture Drawable Lod - LOW", &g_captureDrawableLodFlags, BIT(LOD_LOW));
		bk.AddToggle("Capture Drawable Lod - VLOW", &g_captureDrawableLodFlags, BIT(LOD_VLOW));
		bk.AddSeparator("");
		bk.AddToggle("Capture Props", &g_captureProps);
		bk.AddToggle("Capture Trees", &g_captureTrees);
		bk.AddToggle("Capture Alpha", &g_captureAlpha);
		bk.AddToggle("Capture Water", &g_captureWater);
		bk.AddToggle("Capture Decals", &g_captureDecals);
	//	bk.AddToggle("Capture Terrain", &g_captureTerrain);
		bk.AddSeparator("");
		bk.AddToggle("Capture Only Triangles Within Sphere", &g_captureClipToSpheres);
		bk.AddToggle("Capture Verbose", &g_captureVerbose);
		bk.AddTitle("");
		bk.AddSlider("Draw Opacity", &g_drawOpacity, 0.0f, 1.0f, 1.0f/32.0f);
		bk.AddSlider("Draw Dist Min", &g_drawDistanceMin, 0.0f, 16000.0f, 1.0f/8.0f);
		bk.AddSlider("Draw Dist Max", &g_drawDistanceMax, 0.0f, 16000.0f, 1.0f/8.0f);
	}
}

void SceneGeometryCapture::Update()
{
#if SGC_PANORAMA
	if (g_capturePanoramaEnabled && g_capturePanoramaShowFPS && g_capturePanoramaShowFPSColour.GetAlpha() > 0)
	{
		grcDebugDraw::Text(Vector2(g_capturePanoramaShowFPSPos.GetXf(), g_capturePanoramaShowFPSPos.GetYf()), DD_ePCS_Pixels, g_capturePanoramaShowFPSColour, atVarString("FPS = %f", 1.0f/Max<float>(0.001f, fwTimer::GetSystemTimeStep())).c_str(), g_capturePanoramaShowFPSBkgQuad, g_capturePanoramaShowFPSScale, g_capturePanoramaShowFPSScale);
	}
#endif // SGC_PANORAMA

	CaptureGeometryUpdate();

	if (g && g_drawOpacity > 0.0f)
	{
#if SGC_PANORAMA
		if (g_capturePanoramaEnabled)
		{
			return;
		}
#endif // SGC_PANORAMA

		const grcViewport* vp = gVpMan.GetCurrentGameGrcViewport();
		const Vec3V camPos = vp->GetCameraPosition();
		const Vec3V camDir = -vp->GetCameraMtx().GetCol2();

		for (int i = 1; i < g_selectedSphereComboList.GetCount(); i++)
		{
			const int sphereIndex = g_selectedSphereComboList[i];

			if (sphereIndex >= 0 && sphereIndex < g->m_spheres.GetCount())
			{
				const CSceneGeometryCapture::Sphere& sphere = g->m_spheres[sphereIndex];
				const float dist = Max(ScalarV(V_ZERO), Mag(sphere.m_sphere.GetXYZ() - camPos) - sphere.m_sphere.GetW()).Getf();

				if ((dist < g_drawDistanceMax || sphereIndex == g_selectedSphereIndex) && vp->IsSphereVisible(sphere.m_sphere))
				{
					float opacity = g_drawOpacity;

					if (dist > g_drawDistanceMin && g_drawDistanceMin < g_drawDistanceMax && sphereIndex != g_selectedSphereIndex)
					{
						opacity *= (dist - g_drawDistanceMax)/(g_drawDistanceMin - g_drawDistanceMax);
					}

					if (sphereIndex != g_selectedSphereIndex)
					{
						opacity *= 0.5f;
					}

					Color32 colour = sphere.m_colour;
					colour.SetAlpha((u8)(0.5f + (float)colour.GetAlpha()*opacity));

					grcDebugDraw::Sphere(sphere.m_sphere.GetXYZ(), sphere.m_sphere.GetWf(), colour, false, 1, 16, true);

					if (Dot(sphere.m_sphere.GetXYZ() - camPos, camDir).Getf() > 0.1f)
					{
						const Vec3V centre = sphere.m_sphere.GetXYZ();

						// cross
						grcDebugDraw::Line(centre - Vec3V(V_X_AXIS_WZERO), centre + Vec3V(V_X_AXIS_WZERO), Color32(255,0,0,255));
						grcDebugDraw::Line(centre - Vec3V(V_Y_AXIS_WZERO), centre + Vec3V(V_Y_AXIS_WZERO), Color32(0,255,0,255));
						grcDebugDraw::Line(centre,                         centre + Vec3V(V_Z_AXIS_WZERO), Color32(0,0,255,255));

						// text
						grcDebugDraw::Text(centre, colour, 0, 0, atVarString("%d. %s", sphereIndex, sphere.m_name.c_str()).c_str(), true);
					}
#if RDR_VERSION
					if (sphereIndex == g_selectedSphereIndex && sphere.m_areaCode[0])
					{
						const u32 key = ((u32)sphere.m_areaCode[0]) | (((u32)sphere.m_areaCode[1])<<8) | (((u32)sphere.m_areaCode[2])<<16);
						const int numBoxes = GeomCollector::GetAreaCodeNumBoxes(key);

						for (int k = 0; k < numBoxes; k++)
						{
							const spdAABB& box = GeomCollector::GetAreaCodeBox(key, k);
							Color32 boxColour(255,255,255,255);

							switch (box.GetUserInt1())
							{
							case 1: boxColour = Color32(  0,255,  0,255); break; // same area code from both archetype and mapdata paths (expected case)
							case 2: boxColour = Color32(  0,255,255,255); break; // area code from mapdata path, no code in archetype path
							case 3: boxColour = Color32(255,  0,255,255); break; // area code from archetype path, no code in mapdata path
							case 4: boxColour = Color32(255,255,  0,255); break; // area code from mapdata path, different code in archetype path
							case 5: boxColour = Color32(255,  0,  0,255); break; // area code from archetype path, different code in mapdata path
							}

							grcDebugDraw::BoxAxisAligned(box.GetMin(), box.GetMax(), boxColour, false);
							grcDebugDraw::Line(sphere.m_sphere.GetXYZ(), box.GetCenter(), boxColour, boxColour); // hack for me to more easily spot crap under the map ..
						}
					}
#endif // RDR_VERSION
				}
			}
		}
	}
}

bool SceneGeometryCapture::IsCapturingPanorama()
{
#if SGC_PANORAMA
	return g_capturePanoramaEnabled;
#else
	return false;
#endif
}

bool SceneGeometryCapture::ShouldSkipEntity(const CEntity* pEntity)
{
#if SGC_PANORAMA
	if (Unlikely(g_capturePanoramaEnabled))
	{
		if (pEntity && pEntity->GetIsTypePed())
		{
			return true;
		}
	}
#else
	(void)pEntity;
#endif

	return false;
}

#endif // __BANK
