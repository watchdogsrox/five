///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	GtaMaterialDebug.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 December 2008
// 
///////////////////////////////////////////////////////////////////////////////

#if __BANK

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "GtaMaterialDebug.h"

// rage
#include "file/packfile.h"
#include "grcore/debugdraw.h"
#include "phbound/boundbox.h"
#include "phbound/boundcapsule.h"
#include "phbound/boundcomposite.h"
#include "phbound/boundcylinder.h"
#include "phbound/boundbvh.h"
#include "system/nelem.h"

// framework
#include "fwrenderer/renderlistbuilder.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwsys/timer.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaminginfo.h"

// game
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "Timecycle/Timecycle.h"
#include "Objects/ProceduralInfo.h"
#include "Peds/PedDebugVisualiser.h"
#include "Physics/GtaInst.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "Vfx/Systems/VfxMaterial.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CLASS phMaterialDebug
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Init
///////////////////////////////////////////////////////////////////////////////

#ifndef NAVGEN_TOOL
void phMaterialDebug::Init()
{
	// widget pointers
	m_pProcObjCombo = NULL;
	m_procObjComboNeedsUpdate = false;

	// debug
	m_disableSeeThru = false;
	m_disableShootThru = false;
	m_zeroPenetrationResistance = false;
	m_renderNonClimbableCollision = false;
	m_quickDebugMode = MTLDEBUGMODE_OFF;

	// settings - how to render
	m_constantColour = Color32(255, 0, 0, 255);
	m_currColour = Color32(255, 255, 255, 255);
	m_currColourLit = m_currColour;
	m_vertexShift = 0.0f;
	m_colourMode1 = MTLCOLOURMODE_DEFAULT1;
	m_colourMode2 = MTLCOLOURMODE_OFF;
	m_currColourMode = m_colourMode1;
	m_flashPolys = false;
	m_solidPolys = false;
	m_litPolys = false;
	m_renderBackFaces = false;
	m_printObjectNames = false;
	m_polyAlpha = 255;
	m_polyRange = 20.0f;
	m_polyRangeXY = false;
	m_polyProjectToScreen = false;

	// settings - what to render - general
	m_renderAllPolys = false;
	m_renderBoundingSpheres = false;
	m_renderBoundingBoxes = false;

	// settings - what to render - bound options
	m_renderCompositeBounds = true;
	m_renderBVHBounds = true;
	m_renderGeometryBounds = true;
	m_renderBoxBounds = true;
	m_renderSphereBounds = true;
	m_renderCapsuleBounds = true;
	m_renderCylinderBounds = true;

	m_renderWeaponMapBounds = true;
	m_renderMoverMapBounds = true;

	// settings - what to render - material options
	m_renderMaterials = false;
	m_includeMaterialsAbove = false;
	m_renderCarGlassMaterials = false;
	m_renderNonCarGlassMaterials = false;
	m_currMaterialId = 0;

	m_renderFxGroups = false;
	m_currFxGroup = 0;

	m_renderMaterialFlags = false;
	m_currMaterialFlag = 0;

	// settings - what to render - poly options
	m_renderBVHPolyPrims = true;
	m_renderBVHSpherePrims = true;
	m_renderBVHCapsulePrims = true;
	m_renderBVHBoxPrims = true;

	m_renderProceduralTags = false;
	m_currProceduralTag = 0;

	m_renderRoomIds = false;
	m_currRoomId = 0;

	m_renderPolyFlags = false;
	m_currPolyFlag = 0;

	m_renderPedDensityInfo = false;
	m_renderPavementInfo = false;
	m_renderPedDensityValues = false;
	m_renderMaterialNames = false;
	m_renderVfxGroupNames = false;

	// exclusions
	m_excludeMap = false;
	m_excludeBuildings = false;
	m_excludeObjects = false;
	m_excludeFragObjects = false;
	m_excludeVehicles = false;
	m_excludeFragVehicles = false;
	m_excludePeds = false;
	m_excludeFragPeds = false;
	m_excludeNonWeaponTest = false;

	m_excludeInteriors = false;
	m_excludeExteriors = false;

	// toggle in the "optimizations" bank
	m_renderStairsCollision = false;
	m_renderOnlyWeaponMapBounds = false;
	m_renderOnlyMoverMapBounds = false;
}


///////////////////////////////////////////////////////////////////////////////
// Update
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::Update()
{
	m_currColourMode = m_colourMode1;

	// flash the rendering - return if we shouldn't be rendering at this time
	if (m_flashPolys)
	{
		float currTime = 0.001f*fwTimer::GetTimeInMilliseconds();
		s32 currTimeInt = (s32)currTime;
		float flashTime = currTime - currTimeInt;
		if (flashTime<0.5f)
		{
			m_currColourMode = m_colourMode2;
		}
	}

	// do the rendering from the update
	// we only issue calls to debug draw that get rendered from the render thread
	Render();
}


///////////////////////////////////////////////////////////////////////////////
// UpdateProcObjComboBox
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::UpdateProcObjComboBox()
{
	//if the combo box doesn't exist don't continue.
	if(m_pProcObjCombo == NULL)
	{
		m_procObjComboNeedsUpdate = true;//mark that this needs to be updated.
		return;
	}

	// update the procedural object tag name combo box
	for (u32 i=0; i<MAX_PROCEDURAL_TAGS; i++)
	{
		m_pProcObjCombo->SetString(i, g_procInfo.GetProcTagLookup(i).GetName());
	}

	m_procObjComboNeedsUpdate = false; //combo is now updated
}


///////////////////////////////////////////////////////////////////////////////
// UserRenderBound
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::UserRenderBound(phInst* pInst, phBound* pBound, Mat34V_In vMtx, bool isCompositeChild)
{
	float fBackupRange = m_polyRange;
	m_polyRange = 1000.0f;
	m_renderAllPolys = true;
	RenderBound(pInst, pBound, vMtx, isCompositeChild);
	m_renderAllPolys = false;
	m_polyRange = fBackupRange;
}


///////////////////////////////////////////////////////////////////////////////
// InitWidgets
///////////////////////////////////////////////////////////////////////////////
#if __BANK
void LoadRumbleProfiles()
{
	PGTAMATERIALMGR->LoadRumbleProfiles();
}

void phMaterialDebug::InitWidgets(bkBank& bank)
{
	// debug mode combo box setup
	const char* debugModeList[] = 
	{
		"Off",
		"Constant",
		"Bound",
		"Entity",
		"Packfile",
		"VfxGroup",
		"Glass",
		"Flags"
	};
	CompileTimeAssert(NELEM(debugModeList) == MTLCOLOURMODE_NUM);

	// material name combo box setup
	u32 numMaterials = PGTAMATERIALMGR->GetNumMaterials();
	const char** materialList = (const char**)alloca(numMaterials*sizeof(const char*));
	for (u32 i=0; i<numMaterials; i++)
	{
		materialList[i] = PGTAMATERIALMGR->GetMaterial(i).GetName();
	}

	// procedural tag combo box setup
	const char* procTagList[MAX_PROC_OBJ_INFOS];
	for (u32 i=0; i<MAX_PROC_OBJ_INFOS; i++)
	{
		procTagList[i] = 0;
	}

	// material flag combo box setup
	const char* mtlFlagList[] = 
	{
		"See Through",
		"Shoot Through",
		"Shoot Through Fx",
		"No Decal",
		"Porous",
		"Heats Tyre"
	};
	CompileTimeAssert(NELEM(mtlFlagList) == MTLFLAG_NUM_FLAGS);

	// poly flag combo box setup
	const char* polyFlagList[] = 
	{
		"Stairs",
		"Not Climbable",
		"See Through",
		"Shoot Through",
		"Not Cover",
		"Walkable Path",
		"No Cam Collision",
		"Shoot Through Fx",
		"No Decal",
		"No Navmesh",
		"No Ragdoll",
		"<Reserved For Tool> / Vehicle Wheel",
		"No Particles",
		"Too Steep For Player",
		"No Network Spawn",
		"No Cam Collision Allow Clip"
	};
	CompileTimeAssert(NELEM(polyFlagList) == POLYFLAG_NUM_FLAGS);

	// widget menu
	bank.PushGroup("GTA Materials", false);
	{
		bank.AddButton("Reload rumble profiles", LoadRumbleProfiles);
		bank.PushGroup("Debug", false);
		{
			bank.AddToggle("Disable See Thru", &m_disableSeeThru);
			bank.AddToggle("Disable Shoot Thru", &m_disableShootThru);
			bank.AddToggle("Zero Penetration Resistance", &m_zeroPenetrationResistance);
		}
		bank.PopGroup();

		// settings - how to render the polys
		bank.PushGroup("How to Render", false);
		{
			bank.AddToggle("Disable Alpha", &fwRenderListBuilder::GetRenderPassDisableBits(), BIT(RPASS_ALPHA));
			bank.AddSeparator();
			bank.AddColor("Constant Colour", &m_constantColour);
			bank.AddCombo("Colour Mode 1", (int*)&m_colourMode1, MTLCOLOURMODE_NUM, debugModeList);
			bank.AddCombo("Colour Mode 2", (int*)&m_colourMode2, MTLCOLOURMODE_NUM, debugModeList);
			bank.AddToggle("Flash", &m_flashPolys);
			bank.AddToggle("Solid", &m_solidPolys);
			bank.AddToggle("Lit", &m_litPolys);
			bank.AddToggle("Render Back Faces", &m_renderBackFaces);
			bank.AddSlider("Vertex Shift", &m_vertexShift, -1.0f, 1.0f, 0.01f);
			bank.AddSlider("Poly Alpha", &m_polyAlpha, 0, 255, 1);
			bank.AddSlider("Range", &m_polyRange, 2.0f, 1000.0f, 1.0f);
			bank.AddToggle("Range XY", &m_polyRangeXY);
			bank.AddToggle("Project To Screen", &m_polyProjectToScreen);
			bank.AddToggle("Print Object Names", &m_printObjectNames);
		}
		bank.PopGroup();

		// settings - what polys to render
		bank.PushGroup("What to Render", false);
		{
			// general data
			bank.AddToggle("All Polys", &m_renderAllPolys);
			bank.AddToggle("All Bounding Spheres", &m_renderBoundingSpheres);
			bank.AddToggle("All Bounding Boxes", &m_renderBoundingBoxes);

			// bound options
			bank.AddTitle("Bound Options");
			bank.AddToggle("All Composite Bounds", &m_renderCompositeBounds);
			bank.AddToggle("All BVH Bounds", &m_renderBVHBounds);
			bank.AddToggle("All Geometry Bounds", &m_renderGeometryBounds);
			bank.AddToggle("All Box Bounds", &m_renderBoxBounds);
			bank.AddToggle("All Sphere Bounds", &m_renderSphereBounds);
			bank.AddToggle("All Capsule Bounds", &m_renderCapsuleBounds);
			bank.AddToggle("All Cylinder Bounds", &m_renderCylinderBounds);

			bank.AddToggle("All Weapon Map Bounds", &m_renderWeaponMapBounds);
			bank.AddToggle("All Mover Map Bounds", &m_renderMoverMapBounds);
			bank.AddToggle("All Stair/Slope Map Bounds", &m_renderStairSlopeMapBounds);

			// material data
			bank.AddTitle("Material Options");

			bank.AddToggle("All Car Glass Materials", &m_renderCarGlassMaterials);
			bank.AddToggle("All Non-Car Glass Materials", &m_renderNonCarGlassMaterials);

			bank.AddToggle("This Material Type", &m_renderMaterials);
			bank.AddCombo("Material Type", (int*)&m_currMaterialId, numMaterials, materialList);
			bank.AddToggle("Include Materials Above This Type", &m_includeMaterialsAbove);

			bank.AddToggle("This Fx Group", &m_renderFxGroups);
			bank.AddCombo("Fx Group", &m_currFxGroup, NUM_VFX_GROUPS, g_fxGroupsList);

			bank.AddToggle("This Material Flag", &m_renderMaterialFlags);
			bank.AddCombo("Material Flag", &m_currMaterialFlag, MTLFLAG_NUM_FLAGS, mtlFlagList);

			// poly options
			bank.AddTitle("Poly Options");

			bank.AddToggle("All BVH Poly Primitives", &m_renderBVHPolyPrims);
			bank.AddToggle("All BVH Sphere Primitives", &m_renderBVHSpherePrims);
			bank.AddToggle("All BVH Capsule Primitives", &m_renderBVHCapsulePrims);
			bank.AddToggle("All BVH Box Primitives", &m_renderBVHBoxPrims);

			bank.AddToggle("This Procedural Tag", &m_renderProceduralTags);
			m_pProcObjCombo = bank.AddCombo("Procedural Tag", &m_currProceduralTag, NUM_VFX_GROUPS, procTagList);

			bank.AddToggle("This Room ID", &m_renderRoomIds);
			bank.AddSlider("Room ID", &m_currRoomId, 0, 31, 1);

			bank.AddToggle("This Poly Flag", &m_renderPolyFlags);
			bank.AddCombo("Poly Flag", &m_currPolyFlag, POLYFLAG_NUM_FLAGS, polyFlagList);

			bank.AddToggle("Render Ped Density Info", &m_renderPedDensityInfo);
			bank.AddToggle("Render Pavement Info", &m_renderPavementInfo);
			bank.AddToggle("Render Ped Density Values", &m_renderPedDensityValues);
			bank.AddToggle("Render Material Names", &m_renderMaterialNames);
			bank.AddToggle("Render Vfx Group Names", &m_renderVfxGroupNames);

			// exclusions
			bank.AddTitle("Exclusions");

			bank.AddToggle("Exclude Map", &m_excludeMap);
			bank.AddToggle("Exclude Buildings", &m_excludeBuildings);
			bank.AddToggle("Exclude Objects", &m_excludeObjects);
			bank.AddToggle("Exclude Frag Objects", &m_excludeFragObjects);
			bank.AddToggle("Exclude Vehicles", &m_excludeVehicles);
			bank.AddToggle("Exclude Frag Vehicles", &m_excludeFragVehicles);
			bank.AddToggle("Exclude Peds", &m_excludePeds);
			bank.AddToggle("Exclude Frag Peds", &m_excludeFragPeds);
			bank.AddToggle("Exclude Non-Weapon Test", &m_excludeNonWeaponTest);

			bank.AddToggle("Exclude Interiors", 	&m_excludeInteriors);
			bank.AddToggle("Exclude Exteriors",		&m_excludeExteriors);
		}
		bank.PopGroup();
	}
	bank.PopGroup();

	bkBank* pBankOptimize = BANKMGR.FindBank("Optimization");
	Assert(pBankOptimize);
	if (pBankOptimize)
	{
		pBankOptimize->AddToggle("Render Stairs Collision", &m_renderStairsCollision, ToggleRenderStairsCollision);
		pBankOptimize->AddToggle("Render Only Weapon Map Bounds", &m_renderOnlyWeaponMapBounds, ToggleRenderOnlyWeaponMapBounds);
		pBankOptimize->AddToggle("Render Only Mover Map Bounds", &m_renderOnlyMoverMapBounds, ToggleRenderOnlyMoverMapBounds);
	}
}

#endif // __BANK

#endif // NAVGEN_TOOL


///////////////////////////////////////////////////////////////////////////////
// Render
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::Render()
{
	if (m_renderAllPolys || m_renderBoundingSpheres || m_renderBoundingBoxes ||
		m_renderMaterials || m_renderCarGlassMaterials || m_renderNonCarGlassMaterials ||
		m_renderFxGroups || m_renderMaterialFlags || m_renderProceduralTags ||
		m_renderRoomIds || m_renderPedDensityInfo || m_renderPavementInfo || m_renderPolyFlags)
	{
		// go through each object in the world
		for (int levelIndex = 0; levelIndex < CPhysics::GetLevel()->GetMaxObjects(); ++levelIndex)
		{
			// skip if an object doesn't exist at this index
			if (CPhysics::GetLevel()->IsNonexistent(levelIndex))
			{
				continue;
			}

			// store the inst and bound info
			phInst* pInst = CPhysics::GetLevel()->GetInstance(levelIndex);

			// return if we're rendering anything that's been excluded
			u32 instType = pInst->GetClassType();
			if ((instType==PH_INST_MAPCOL && m_excludeMap) ||
				(instType==PH_INST_BUILDING && m_excludeBuildings) ||
				(instType==PH_INST_VEH && m_excludeVehicles) ||
				(instType==PH_INST_FRAG_VEH && m_excludeFragVehicles) ||
				(instType==PH_INST_OBJECT && m_excludeObjects) ||
				(instType==PH_INST_FRAG_OBJECT && m_excludeFragObjects) ||
				(instType==PH_INST_PED && m_excludePeds) ||
				(instType==PH_INST_FRAG_PED && m_excludeFragPeds))
			{
				continue;
			}

			CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
			if (pEntity)
			{
				s32 storeIndex = pEntity->GetIplIndex();
				fwBoundDef* pDef = g_StaticBoundsStore.GetSlot(strLocalIndex(storeIndex));
				if (pDef && pDef->IsDummy())
				{
					if (m_excludeInteriors){
						continue;
					}
				} else
				{
					if (m_excludeExteriors){
						continue;
					}
				}
			}

			phBound* pBound = pInst->GetArchetype()->GetBound();
			Mat34V vCurrMtx = pInst->GetMatrix();
			
			// skip if this bound is out or range
			Vec3V vCamPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
			if (IsBoundInRange(pBound, vCurrMtx, vCamPos)==false)
			{
				continue;
			}

			UpdateCurrColourInst(pInst);

			RenderBound(pInst, pBound, vCurrMtx, false);

			if (m_renderBoundingSpheres || m_renderBoundingBoxes)
			{
				RenderBoundingVolumes(pInst);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBound
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBound(phInst* pInst, phBound* pBound, Mat34V_In vCurrMtx, bool isCompositeChild)
{
	Vec3V vCamPos = VECTOR3_TO_VEC3V(camInterface::GetPos());

	// recurse with any composite bounds
	if (pBound->GetType() == phBound::COMPOSITE)
	{
		phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
		for (s32 i=0; i<pBoundComposite->GetNumBounds(); i++)
		{
			if (ShouldRenderBound(pInst, pBoundComposite, i, false))
			{
				// check this bound is still valid (smashable code may have removed it)
				phBound* pChildBound = pBoundComposite->GetBound(i);
				if (pChildBound)
				{
					// calc the new matrix
					Mat34V vMtxNew;
					Transform(vMtxNew, vCurrMtx, pBoundComposite->GetCurrentMatrix(i));

					// render the bound
					RenderBound(pInst, pChildBound, vMtxNew, true);
				}
			}
		}
	}

	if (ShouldRenderBound(pInst, pBound, 0, isCompositeChild)==false)
	{
		return;
	}

	// only continue if we are dealing with a BVH
	if (pBound->GetType()==phBound::BVH)
	{
		phBoundBVH* pBoundGeomBvh = (phBoundBVH*)pBound;

		if (m_printObjectNames)
		{
			CEntity* pEntity = (CEntity*)pInst->GetUserData();
			if (pEntity && pEntity->GetArchetype())
			{
				if (pInst->GetArchetype())
				{
					fwModelId modelId = pEntity->GetModelId();
					if (modelId.IsValid())
					{
						grcDebugDraw::AddDebugOutput(CModelInfo::GetBaseModelInfoName(modelId));
					}
				}
			}
		}

		// put the centre position into inst space
		Mat34V vInvInstMtx;
		InvertTransformOrtho(vInvInstMtx, pInst->GetMatrix());
		Vec3V vCamPosInstSpace = Transform(vInvInstMtx, vCamPos);

		// cull the bound so that it only contains polys within our test sphere
		//pBoundGeomBvh->CullSpherePolys(camPosInstSpace, m_polyRange);

		// new rage will need this
		phBoundCuller culler;
		culler.SetArrays(m_culledPolys, MAX_CULLED_POLY_VERTS);
		if (m_polyRangeXY)
		{
			pBoundGeomBvh->CullOBBPolys(culler, Mat34V(Mat33V(V_IDENTITY), vCamPosInstSpace), Vec3V(m_polyRange, m_polyRange, FLT_MAX));
		}
		else
		{
			pBoundGeomBvh->CullSpherePolys(culler, vCamPosInstSpace, ScalarVFromF32(m_polyRange));
		}

		UpdateCurrColourBound(phBound::BVH);

		// go through this object's active polys (non culled)
		for (s32 i=culler.GetNumCulledPolygons()-1; i>=0; i--)
		{
			phPolygon::Index polyIndex = culler.GetCulledPolygonIndex(i);
			const phPrimitive& bvhPrimitive = pBoundGeomBvh->GetPrimitive(polyIndex);

			phMaterialIndex boundMtlId = pBoundGeomBvh->GetPolygonMaterialIndex(polyIndex);
			phMaterialMgr::Id mtlId = pBoundGeomBvh->GetMaterialId(boundMtlId);
			phMaterialGta* pMtl = (phMaterialGta*)&PGTAMATERIALMGR->GetMaterial(mtlId);

			if (m_renderBVHPolyPrims && bvhPrimitive.GetType() == PRIM_TYPE_POLYGON)
			{
				RenderPoly(bvhPrimitive.GetPolygon(), pMtl, mtlId, pBoundGeomBvh, vCurrMtx, vCamPos);
			}
			else if (m_renderBVHSpherePrims && bvhPrimitive.GetType() == PRIM_TYPE_SPHERE)
			{
				RenderBVHSpherePrim(bvhPrimitive.GetSphere(), pMtl, mtlId, pBoundGeomBvh, vCurrMtx, vCamPos);
			}
			else if (m_renderBVHCapsulePrims && bvhPrimitive.GetType() == PRIM_TYPE_CAPSULE)
			{
				RenderBVHCapsulePrim(bvhPrimitive.GetCapsule(), pMtl, mtlId, pBoundGeomBvh, vCurrMtx, vCamPos);
			}
			else if (m_renderBVHBoxPrims && bvhPrimitive.GetType() == PRIM_TYPE_BOX)
			{
				RenderBVHBoxPrim(bvhPrimitive.GetBox(), pMtl, mtlId, pBoundGeomBvh, vCurrMtx, vCamPos);
			}
		}
	}
	else if (pBound->GetType() == phBound::GEOMETRY)
	{
		phBoundGeometry* pBoundGeom = (phBoundGeometry*)pBound;

		if (m_printObjectNames)
		{
			fwModelId modelId = ((CEntity*)pInst->GetUserData())->GetModelId();
			if (modelId.IsValid())
			{
				grcDebugDraw::AddDebugOutput(CModelInfo::GetBaseModelInfoName(modelId));
			}
		}

		UpdateCurrColourBound(phBound::GEOMETRY);

		// go through this object's active polys (non culled)
		for (s32 i=pBoundGeom->GetNumPolygons()-1; i>=0; i--)
		{
			const phPolygon& currPoly = pBoundGeom->GetPolygon(i);
			phMaterialIndex boundMtlId = pBoundGeom->GetPolygonMaterialIndex(i);
			phMaterialMgr::Id mtlId = pBoundGeom->GetMaterialId(boundMtlId);
			phMaterialGta* pMtl = (phMaterialGta*)&PGTAMATERIALMGR->GetMaterial(mtlId);
			
			RenderPoly(currPoly, pMtl, mtlId, pBoundGeom, vCurrMtx, vCamPos);
		}
	}
	else if (pBound->GetType() == phBound::BOX)
	{
		phBoundBox* pBoundBox = (phBoundBox*)pBound;

		if (m_printObjectNames)
		{
			fwModelId modelId = ((CEntity*)pInst->GetUserData())->GetModelId();
			if (modelId.IsValid())
			{
				grcDebugDraw::AddDebugOutput(CModelInfo::GetBaseModelInfoName(modelId));
			}
		}

		phMaterialMgr::Id mtlId = pBoundBox->GetMaterialId(0);
		phMaterialGta* pMtl = (phMaterialGta*)&PGTAMATERIALMGR->GetMaterial(mtlId);

		UpdateCurrColourBound(phBound::BOX);

		RenderBoundBox(pMtl, mtlId, pBoundBox, vCurrMtx, vCamPos);

		if (m_renderMaterialNames)
		{
			char mtlName[128] = "";
			PGTAMATERIALMGR->GetMaterialName(mtlId, mtlName, sizeof(mtlName));
			grcDebugDraw::Text(vCurrMtx.d(), Color_yellow, mtlName);
		}
	}
	else if (pBound->GetType() == phBound::CAPSULE)
	{
		phBoundCapsule* pBoundCapsule = (phBoundCapsule*)pBound;

		phMaterialMgr::Id mtlId = pBoundCapsule->GetMaterialId(0);
		phMaterialGta* pMtl = (phMaterialGta*)&PGTAMATERIALMGR->GetMaterial(mtlId);

		UpdateCurrColourBound(phBound::CAPSULE);

		RenderBoundCapsule(pMtl, mtlId, pBoundCapsule, vCurrMtx, vCamPos);

		if (m_renderMaterialNames)
		{
			char mtlName[128] = "";
			PGTAMATERIALMGR->GetMaterialName(mtlId, mtlName, sizeof(mtlName));
			grcDebugDraw::Text(vCurrMtx.d(), Color_yellow, mtlName);
		}
	}
	else if (pBound->GetType() == phBound::SPHERE)
	{
		phBoundSphere* pBoundSphere = (phBoundSphere*)pBound;

		phMaterialMgr::Id mtlId = pBoundSphere->GetMaterialId(0);
		phMaterialGta* pMtl = (phMaterialGta*)&PGTAMATERIALMGR->GetMaterial(mtlId);

		UpdateCurrColourBound(phBound::SPHERE);

		RenderBoundSphere(pMtl, mtlId, pBoundSphere, vCurrMtx, vCamPos);

		if (m_renderMaterialNames)
		{
			char mtlName[128] = "";
			PGTAMATERIALMGR->GetMaterialName(mtlId, mtlName, sizeof(mtlName));
			grcDebugDraw::Text(vCurrMtx.d(), Color_yellow, mtlName);
		}
	}
	else if (pBound->GetType() == phBound::CYLINDER)
	{
		phBoundCylinder* pBoundCylinder = (phBoundCylinder*)pBound;

		phMaterialMgr::Id mtlId = pBoundCylinder->GetMaterialId(0);
		phMaterialGta* pMtl = (phMaterialGta*)&PGTAMATERIALMGR->GetMaterial(mtlId);

		UpdateCurrColourBound(phBound::CYLINDER);

		RenderBoundCylinder(pMtl, mtlId, pBoundCylinder, vCurrMtx, vCamPos);

		if (m_renderMaterialNames)
		{
			char mtlName[128] = "";
			PGTAMATERIALMGR->GetMaterialName(mtlId, mtlName, sizeof(mtlName));
			grcDebugDraw::Text(vCurrMtx.d(), Color_yellow, mtlName);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBVHBoxPrim
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBVHBoxPrim(const phPrimBox& bvhBoxPrim, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In UNUSED_PARAM(vCamPos))
{
	// try to render the poly
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		UpdateCurrColourPoly(mtlId);

		Vec3V vVerts[4];
		vVerts[0] = pBoundPoly->GetVertex(bvhBoxPrim.GetVertexIndex(0));
		vVerts[1] = pBoundPoly->GetVertex(bvhBoxPrim.GetVertexIndex(1));
		vVerts[2] = pBoundPoly->GetVertex(bvhBoxPrim.GetVertexIndex(2));
		vVerts[3] = pBoundPoly->GetVertex(bvhBoxPrim.GetVertexIndex(3));

		Mat34V vBoxMtxLcl;
		Vec3V vBoxSize;
		ScalarV vBoxMaxMargin;
		geomBoxes::ComputeBoxDataFromOppositeDiagonals(vVerts[0], vVerts[1], vVerts[2], vVerts[3], vBoxMtxLcl, vBoxSize, vBoxMaxMargin);

		// calc min and max in box matrix space
		Mat34V vBoxMtxWld;
		Transform(vBoxMtxWld, vMtx, vBoxMtxLcl);
		Vec3V vBoxMax = vBoxSize * ScalarV(V_HALF);
		Vec3V vBoxMin = -vBoxMax;

		// render
		grcDebugDraw::BoxOriented(vBoxMin, vBoxMax, vBoxMtxWld, m_currColourLit, m_solidPolys);
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBVHSpherePrim
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBVHSpherePrim(const phPrimSphere& bvhSpherePrim, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In UNUSED_PARAM(vCamPos))
{
	// try to render the poly
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		UpdateCurrColourPoly(mtlId);

		Vec3V vSphereCentre = pBoundPoly->GetVertex(bvhSpherePrim.GetCenterIndex());
		vSphereCentre = Transform(vMtx, vSphereCentre);

		float sphereRadius = bvhSpherePrim.GetRadius();

		grcDebugDraw::Sphere(vSphereCentre, sphereRadius, m_currColourLit, m_solidPolys);
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBVHCapsulePrim
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBVHCapsulePrim(const phPrimCapsule& bvhCapsulePrim, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In UNUSED_PARAM(vCamPos))
{
	// try to render the poly
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		UpdateCurrColourPoly(mtlId);

		Vec3V vStartPosWld = pBoundPoly->GetVertex(bvhCapsulePrim.GetEndIndex0());
		Vec3V vEndPosWld = pBoundPoly->GetVertex(bvhCapsulePrim.GetEndIndex1());
		vStartPosWld = Transform(vMtx, vStartPosWld);
		vEndPosWld = Transform(vMtx, vEndPosWld);

		grcDebugDraw::Capsule(vStartPosWld, vEndPosWld, bvhCapsulePrim.GetRadius(), m_currColourLit, m_solidPolys);
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderPoly
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderPoly(const phPolygon& currPoly, phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundPolyhedron* pBoundPoly, Mat34V_In vMtx, Vec3V_In vCamPos)
{
	// store some camera info
	/// Vec3V vCamForward = VECTOR3_TO_VEC3V(camInterface::GetFront());

	// try to render the poly
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		// store vertex info (transforming into world space)
		int numVerts = POLY_MAX_VERTICES;
		Vec3V vWldVerts[4];
		vWldVerts[0] = pBoundPoly->GetVertex(currPoly.GetVertexIndex(0));
		vWldVerts[1] = pBoundPoly->GetVertex(currPoly.GetVertexIndex(1));
		vWldVerts[2] = pBoundPoly->GetVertex(currPoly.GetVertexIndex(2));

		vWldVerts[0] = Transform(vMtx, vWldVerts[0]);
		vWldVerts[1] = Transform(vMtx, vWldVerts[1]);
		vWldVerts[2] = Transform(vMtx, vWldVerts[2]);

		if (numVerts>3)
		{
			vWldVerts[3] = pBoundPoly->GetVertex(currPoly.GetVertexIndex(3));
			vWldVerts[3] = Transform(vMtx, vWldVerts[3]);
		}

		// get centre of poly
		Vec3V vCentre = vWldVerts[0] + vWldVerts[1] + vWldVerts[2];
		if (numVerts>3)
		{
			vCentre += vWldVerts[3];
			vCentre *= ScalarV(V_QUARTER);
		}
		else
		{
			vCentre *= ScalarV(V_THIRD);
		}

		float nDotL = 1.0f;
		if (m_polyProjectToScreen)
		{
			const grcViewport& gameVP = *gVpMan.GetUpdateGameGrcViewport();

			const Vec3V   camPos   = +gameVP.GetCameraMtx().GetCol3();
			const Vec3V   camDir   = -gameVP.GetCameraMtx().GetCol2();
			const ScalarV camNearZ = ScalarV(gameVP.GetNearClip() + 0.01f);

			Vec3V* p = (Vec3V*)vWldVerts;

			for (int i = 0; i < numVerts; i++)
			{
				const ScalarV z = Dot(p[i] - camPos, camDir);

				if (IsGreaterThanAll(z, ScalarV(V_ZERO)))
				{
					p[i] = camPos + (p[i] - camPos)*(camNearZ/z);
				}
				else
				{
					return; // don't render polygon if it's partially behind camera
				}
			}
		}
		else
		{
			// calc the poly lighting
			Vec3V vWldNormal = pBoundPoly->GetPolygonUnitNormal(ptrdiff_t_to_int(&currPoly - pBoundPoly->GetPolygonPointer()));
			vWldNormal = Transform3x3(vMtx, vWldNormal);

			Vec3V vLightDir = g_timeCycle.GetDirectionalLightDirection();
			nDotL = Dot(vLightDir, vWldNormal).Getf();
			nDotL = Max(0.0f, nDotL);

			// return if not facing the camera
			if (m_renderBackFaces==false)
			{
				Vec3V vCamToVtx = vWldVerts[0] - vCamPos;
				vCamToVtx = Normalize(vCamToVtx);
				if (Dot(vCamToVtx, vWldNormal).Getf()>0.0f)
				{
					return;
				}
			}

			// bring verts closer to the camera to avoid fighting
			if (m_vertexShift!=0.0f)
			{
				for (int i=0; i<numVerts; i++)
				{
					Vec3V vCamToVtx = vWldVerts[i] - vCamPos;
					vCamToVtx = Normalize(vCamToVtx);
					Vec3V vCamShift = vCamToVtx*ScalarVFromF32(m_vertexShift);
					vWldVerts[i] -= vCamShift;
				}
			}
		}

		UpdateCurrColourPoly(mtlId, nDotL);

		// render
		grcDebugDraw::Poly(vWldVerts[0], vWldVerts[1], vWldVerts[2], m_currColourLit, true, m_solidPolys);
		if (numVerts>3)
		{
			grcDebugDraw::Poly(vWldVerts[0], vWldVerts[2], vWldVerts[3], m_currColourLit, true, m_solidPolys);
		}

		// print out int value of per poly attributes
		if (m_renderPedDensityValues)
		{
			s32 pedDensity = PGTAMATERIALMGR->UnpackPedDensity(mtlId);
			if (pedDensity>0)
			{
				static char txt[8];
				formatf(txt, "%u", pedDensity);
				grcDebugDraw::Text(vCentre, Color_green, txt);
			}
		}

		if (m_renderMaterialNames)
		{
			char mtlName[128] = "";
			PGTAMATERIALMGR->GetMaterialName(mtlId, mtlName, sizeof(mtlName));
			grcDebugDraw::Text(vCentre, Color_yellow, mtlName);
		}

		if (m_renderVfxGroupNames)
		{
			VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(mtlId);
			const char* vfxGroupName = g_fxGroupsList[vfxGroup];
			grcDebugDraw::Text(vCentre, Color_yellow, vfxGroupName);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBoundCapsule
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBoundCapsule(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundCapsule* pBoundCapsule, 
										 Mat34V_In vMtx, Vec3V_In UNUSED_PARAM(vCamPos))
{
	// try to render the capsule
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		UpdateCurrColourPoly(mtlId);

		// render
		Vec3V vCentre = pBoundCapsule->GetCentroidOffset();
		vCentre = Transform(vMtx, vCentre);
		ScalarV vHalfLength = ScalarVFromF32(pBoundCapsule->GetLength()*0.5f);
		Vec3V vStartPos = vCentre - (vMtx.GetCol1()*vHalfLength);
		Vec3V vEndPos = vCentre + (vMtx.GetCol1()*vHalfLength);

		grcDebugDraw::Capsule(vStartPos, vEndPos, pBoundCapsule->GetRadius(), m_currColourLit, m_solidPolys);
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBoundSphere
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBoundSphere(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundSphere* pBoundSphere, 
										 Mat34V_In vMtx, Vec3V_In UNUSED_PARAM(vCamPos))
{
	// try to render the capsule
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		UpdateCurrColourPoly(mtlId);

		// render
		Vec3V vCentre = pBoundSphere->GetCentroidOffset();
		vCentre = Transform(vMtx, vCentre);

		grcDebugDraw::Sphere(vCentre, pBoundSphere->GetRadius(), m_currColourLit, m_solidPolys);
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBoundCylinder
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBoundCylinder(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundCylinder* pBoundCylinder, 
											Mat34V_In vMtx, Vec3V_In UNUSED_PARAM(vCamPos))
{
	// try to render the cylinder
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		UpdateCurrColourPoly(mtlId);

		// render
		Vec3V vCentre = pBoundCylinder->GetCentroidOffset();
		vCentre = Transform(vMtx, vCentre);
		ScalarV vHalfHeight = pBoundCylinder->GetHalfHeightV();
		Vec3V vStartPos = vCentre - (vMtx.GetCol1()*vHalfHeight);
		Vec3V vEndPos = vCentre + (vMtx.GetCol1()*vHalfHeight);

		static bool cap = true;
		grcDebugDraw::Cylinder(vStartPos, vEndPos, pBoundCylinder->GetRadius(), m_currColourLit, cap, m_solidPolys);
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBoundBox
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBoundBox(phMaterialGta* pMtl, phMaterialMgr::Id mtlId, phBoundBox* pBoundBox, 
									 Mat34V_In vMtx, Vec3V_In UNUSED_PARAM(vCamPos))
{
	// try to render the capsule
	if (ShouldRenderPoly(pMtl, mtlId))
	{
		UpdateCurrColourPoly(mtlId);

		Vec3V vBoxMax = pBoundBox->GetBoundingBoxMax();
		Vec3V vBoxMin = pBoundBox->GetBoundingBoxMin();

		Mat34V vCentreMtx = vMtx;
		//centreMtx.SetCol3(Transform(centreMtx, pBoundBox->GetCentroidOffset()));

		// render
		grcDebugDraw::BoxOriented(vBoxMin, vBoxMax, vCentreMtx, m_currColourLit, m_solidPolys);
	}
}


///////////////////////////////////////////////////////////////////////////////
// RenderBoundingVolumes
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::RenderBoundingVolumes(phInst* pInst)
{
	CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
	Assert(pEntity);
	if (pEntity->IsArchetypeSet())
	{		
		Vec3V vCentre = VECTOR3_TO_VEC3V(pEntity->GetBoundCentre());
		float radius = pEntity->GetBoundRadius();

		if (m_renderBoundingSpheres)
		{
			m_currColour = Color32(0, 255, 0, m_polyAlpha);

			grcDebugDraw::Sphere(vCentre, radius, m_currColour, false);
		}

		if (m_renderBoundingBoxes)
		{
			m_currColour = Color32(0, 255, 0, m_polyAlpha);

			Vec3V vBBMin = VECTOR3_TO_VEC3V(pEntity->GetBoundingBoxMin());
			Vec3V vBBMax = VECTOR3_TO_VEC3V(pEntity->GetBoundingBoxMax());

			Vec3V vVerts[8];
			vVerts[0] = Vec3V(vBBMin.GetX(), vBBMin.GetY(), vBBMin.GetZ());
			vVerts[1] = Vec3V(vBBMax.GetX(), vBBMin.GetY(), vBBMin.GetZ());
			vVerts[2] = Vec3V(vBBMax.GetX(), vBBMax.GetY(), vBBMin.GetZ());
			vVerts[3] = Vec3V(vBBMin.GetX(), vBBMax.GetY(), vBBMin.GetZ());
			vVerts[4] = Vec3V(vBBMin.GetX(), vBBMin.GetY(), vBBMax.GetZ());
			vVerts[5] = Vec3V(vBBMax.GetX(), vBBMin.GetY(), vBBMax.GetZ());
			vVerts[6] = Vec3V(vBBMax.GetX(), vBBMax.GetY(), vBBMax.GetZ());
			vVerts[7] = Vec3V(vBBMin.GetX(), vBBMax.GetY(), vBBMax.GetZ());

			Mat34V vMtx = pEntity->GetMatrix();
			for (int i=0; i<7; i++)
			{
				vVerts[i] = Transform(vMtx, vVerts[i]);
			}

			grcDebugDraw::Line(vVerts[0], vVerts[1], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[1], vVerts[2], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[2], vVerts[3], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[3], vVerts[0], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[4], vVerts[5], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[5], vVerts[6], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[6], vVerts[7], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[7], vVerts[4], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[0], vVerts[4], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[1], vVerts[5], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[2], vVerts[6], m_currColour, m_currColour);
			grcDebugDraw::Line(vVerts[3], vVerts[7], m_currColour, m_currColour);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// ShouldRenderBound
///////////////////////////////////////////////////////////////////////////////

bool phMaterialDebug::ShouldRenderBound(phInst* pInst, phBound* pBound, s32 childId, bool isCompositeChild)
{
	// check if we should render weapon or mover bounds
	bool passedArchetypeTest = false;
	
	if (isCompositeChild)
	{
		// all composite child bounds should be set to render as they would have failed their previous check if they weren't allowed to
		passedArchetypeTest = true;
	}
	else
	{
		const u32 typeFlags = CPhysics::GetLevel()->GetInstanceTypeFlags(pInst->GetLevelIndex());
		bool isWeaponMapType = (typeFlags & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) > 0;
		bool isMoverMapType = (typeFlags & ArchetypeFlags::GTA_MAP_TYPE_MOVER) > 0;
		bool isStairSlopeMapType = (typeFlags & ArchetypeFlags::GTA_STAIR_SLOPE_TYPE) > 0;

		if (isWeaponMapType || isMoverMapType || isStairSlopeMapType)
		{
			if ((isWeaponMapType && m_renderWeaponMapBounds) || (isMoverMapType && m_renderMoverMapBounds) || (isStairSlopeMapType && m_renderStairSlopeMapBounds))
			{
				passedArchetypeTest = true;
			}
		}
		else
		{
			// not a map bound - always render
			passedArchetypeTest = true;
		}
	}

	if (pBound->GetType()==phBound::COMPOSITE)
	{
		if (m_renderCompositeBounds)
		{
			// composite bounds need to check whether to render 'weapon' or 'mover' collision by looking at the composite type and include flags
			phBoundComposite* pBoundComposite = (phBoundComposite*)pBound;
			if (pBoundComposite->GetTypeAndIncludeFlags())
			{
				// the composite bound has type and include flags per child - check them
				bool isWeaponMapType = (pBoundComposite->GetTypeFlags(childId) & ArchetypeFlags::GTA_MAP_TYPE_WEAPON) > 0;
				bool isMoverMapType = (pBoundComposite->GetTypeFlags(childId) & ArchetypeFlags::GTA_MAP_TYPE_MOVER) > 0;
				bool isStairSlopeMapType = (pBoundComposite->GetTypeFlags(childId) & ArchetypeFlags::GTA_STAIR_SLOPE_TYPE) > 0;
				bool isWeaponTest = (pBoundComposite->GetIncludeFlags(childId) & ArchetypeFlags::GTA_WEAPON_TEST) > 0;

				if (!isWeaponTest && m_excludeNonWeaponTest)
				{
					return false;
				}
				else if (isWeaponMapType || isMoverMapType || isStairSlopeMapType)
				{
					if ((isWeaponMapType && m_renderWeaponMapBounds) || (isMoverMapType && m_renderMoverMapBounds) || (isStairSlopeMapType && m_renderStairSlopeMapBounds))
					{
						return true;
					}
				}
				else
				{
					// not a map bound - always render
					return true;
				}
			}
			else
			{
				// the composite bound does not have type and include flags per child - use the archetype flags instead
				return passedArchetypeTest;
			}
		}
	}
	else 
	{
		// check if we passed the weapon or mover test
		if (passedArchetypeTest)
		{
			// check if we should render the bound type
			if (m_renderBVHBounds && pBound->GetType()==phBound::BVH)
			{		
				return true;
			}
			else if (m_renderGeometryBounds && pBound->GetType()==phBound::GEOMETRY)
			{
				return true;
			}
			else if (m_renderBoxBounds && pBound->GetType()==phBound::BOX)
			{
				return true;
			}
			else if (m_renderSphereBounds && pBound->GetType()==phBound::SPHERE)
			{
				return true;
			}
			else if (m_renderCapsuleBounds && pBound->GetType()==phBound::CAPSULE)
			{
				return true;
			}
			else if (m_renderCylinderBounds && pBound->GetType()==phBound::CYLINDER)
			{
				return true;
			}
		}
	}

	// don't render this bound
	return false;
}


///////////////////////////////////////////////////////////////////////////////
// ShouldRenderPoly
///////////////////////////////////////////////////////////////////////////////

bool phMaterialDebug::ShouldRenderPoly(phMaterialGta* pMtl, phMaterialMgr::Id perPolyMtlId)
{
	phMaterialMgr::Id mtlId = PGTAMATERIALMGR->GetMaterialId(*pMtl);

	// check if this bound should be rendered
	if (m_renderAllPolys)
	{
		return true;
	}
	else 
	{
		if (m_renderCarGlassMaterials)
		{
			if (mtlId == PGTAMATERIALMGR->g_idCarGlassWeak        ||
				mtlId == PGTAMATERIALMGR->g_idCarGlassMedium      ||
				mtlId == PGTAMATERIALMGR->g_idCarGlassStrong      ||
				mtlId == PGTAMATERIALMGR->g_idCarGlassBulletproof ||
				mtlId == PGTAMATERIALMGR->g_idCarGlassOpaque)
			{
				return true;
			}
		}
		else if (m_renderNonCarGlassMaterials)
		{
			if (!PGTAMATERIALMGR->GetIsSmashableGlass(mtlId))
			{
				return true;
			}
		}
		else if (m_renderMaterials)
		{
			if (m_includeMaterialsAbove)
			{
				if (mtlId >= (phMaterialMgr::Id)m_currMaterialId)
				{
					return true;
				}
			}
			else
			{
				if (mtlId == (phMaterialMgr::Id)m_currMaterialId)
				{
					return true;
				}
			}
		}
		else if (m_renderFxGroups)
		{
			if (PGTAMATERIALMGR->GetMtlVfxGroup(mtlId) == m_currFxGroup)
			{
				return true;
			}
		}
		else if (m_renderMaterialFlags)
		{
			if (pMtl->GetFlag(m_currMaterialFlag))
			{
				return true;
			}
		}
		else if (m_renderProceduralTags)
		{
			if (PGTAMATERIALMGR->UnpackProcId(perPolyMtlId) == m_currProceduralTag)
			{
				return true;
			}
		}
		else if (m_renderRoomIds)
		{
			s32 roomId = PGTAMATERIALMGR->UnpackRoomId(perPolyMtlId);
			if (m_currRoomId == roomId)
			{
				return true;
			}
		}
		else if (m_renderPedDensityInfo)
		{
			if (PGTAMATERIALMGR->UnpackPedDensity(perPolyMtlId)!=0 || PGTAMATERIALMGR->GetMtlVfxGroup(mtlId)==VFXGROUP_PAVING)
			{
				return true;
			}
		}
		else if(m_renderPavementInfo)
		{
			if ( ((PGTAMATERIALMGR->UnpackPolyFlags(perPolyMtlId) & (1<<POLYFLAG_WALKABLE_PATH))!=0) || PGTAMATERIALMGR->GetMtlVfxGroup(mtlId)==VFXGROUP_PAVING)
			{
				return true;
			}
		}
		else if (m_renderPolyFlags)
		{
			if (PGTAMATERIALMGR->UnpackPolyFlags(perPolyMtlId) & (1<<m_currPolyFlag))
			{
				return true;
			}
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
// IsBoundInRange
///////////////////////////////////////////////////////////////////////////////

bool phMaterialDebug::IsBoundInRange(phBound* pBound, Mat34V_In vMtx, Vec3V_In vCamPos)
{
	Vec3V vCentre = pBound->GetCentroidOffset();
	vCentre = Transform(vMtx, vCentre);
	Vec3V vCamToCentre = vCentre - vCamPos;
	if (m_polyRangeXY)
	{
		vCamToCentre.SetZf(0.0f);
	}
	float distSqr = MagSquared(vCamToCentre).Getf();
	float minDist = pBound->GetRadiusAroundCentroid() + m_polyRange;
	if (distSqr > minDist*minDist)
	{
		return false;
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
// UpdateCurrColourInst
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::UpdateCurrColourInst(const phInst* pInst)
{
	if (m_currColourMode==MTLCOLOURMODE_OFF)
	{
		m_currColour = Color32(0, 0, 0, 0);
	}
	else if (m_currColourMode==MTLCOLOURMODE_CONSTANT)
	{
		m_currColour = m_constantColour;
	}
	else if (m_currColourMode==MTLCOLOURMODE_ENTITY)
	{
		m_currColour = Color32(0, 0, 0, 0); // in case we can't get the entity

		const CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
		if (pEntity)
		{
			g_DrawRand.Reset((u32)(size_t)pEntity);
			u8 colR = (u8)g_DrawRand.GetRanged(0, 255);
			u8 colG = (u8)g_DrawRand.GetRanged(0, 255);
			u8 colB = (u8)g_DrawRand.GetRanged(0, 255);

			m_currColour = Color32(colR, colG, colB, m_polyAlpha);
		}
	}
	else if (m_currColourMode==MTLCOLOURMODE_PACKFILE)
	{
		m_currColour = Color32(0, 0, 0, 0); // in case we can't get the entity

		const CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
		if (pEntity && pEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS)
		{
			const strIndex streamingIndex = g_StaticBoundsStore.GetStreamingIndex(strLocalIndex(pEntity->GetIplIndex()));

			if (streamingIndex.IsValid())
			{
				const strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
				const strStreamingFile* pFile = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

				if (pFile)
				{
					g_DrawRand.Reset(atStringHash(pFile->m_packfile->GetDebugName(), 0));
					u8 colR = (u8)g_DrawRand.GetRanged(0, 255);
					u8 colG = (u8)g_DrawRand.GetRanged(0, 255);
					u8 colB = (u8)g_DrawRand.GetRanged(0, 255);

					m_currColour = Color32(colR, colG, colB, m_polyAlpha);
				}
			}			
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// UpdateCurrColourBound
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::UpdateCurrColourBound(int boundType)
{
	if (m_currColourMode==MTLCOLOURMODE_BOUND)
	{
		if (boundType==phBound::BVH)
		{
			m_currColour = Color32(255, 128, 0, m_polyAlpha);					// orange
		}
		else if (boundType==phBound::GEOMETRY)
		{
			m_currColour = Color32(0, 255, 0, m_polyAlpha);						// green
		}
		else if (boundType==phBound::BOX)
		{
			m_currColour = Color32(255, 255, 0, m_polyAlpha);					// yellow
		}
		else if (boundType==phBound::CAPSULE)
		{
			m_currColour = Color32(0, 255, 255, m_polyAlpha);					// magenta
		}
		else if (boundType==phBound::SPHERE)
		{
			m_currColour = Color32(255, 0, 0, m_polyAlpha);						// red
		}
		else if (boundType==phBound::CYLINDER)
		{
			m_currColour = Color32(0, 0, 255, m_polyAlpha);						// blue
		}
		else 
		{
			Assertf(0, "debug bound colour not set up (type %d)", boundType);
			m_currColour = Color32(255, 255, 255, m_polyAlpha);					// white
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// UpdateCurrColourPoly
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::UpdateCurrColourPoly(const phMaterialMgr::Id mtlId, float nDotL)
{
	if(m_renderPedDensityInfo)
	{
		s32 iDensity;
		if(PGTAMATERIALMGR->GetMtlVfxGroup(mtlId)==VFXGROUP_PAVING)
		{
			iDensity = 3;
		}
		else
		{
			iDensity = PGTAMATERIALMGR->UnpackPedDensity(mtlId);
		}
		float s = ((float)iDensity) / (float)MAX_PED_DENSITY;
		int c = (int) (s * 255.0f);
		m_currColour = Color32(c, 0, 0, 255);
		m_currColour.SetAlpha(m_polyAlpha);
	}
	else if(m_renderPavementInfo)
	{
		m_currColour = Color_magenta;
	}
	else if (m_currColourMode==MTLCOLOURMODE_VFXGROUP)
	{
		VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(mtlId);

		m_currColour = g_vfxMaterial.GetVfxGroupDebugCol(vfxGroup);
		m_currColour.SetAlpha(m_polyAlpha);
	}
	else if (m_currColourMode==MTLCOLOURMODE_GLASS)
	{
		phMaterialMgr::Id unpackedMtlId = PGTAMATERIALMGR->UnpackMtlId(mtlId);
		if (unpackedMtlId == PGTAMATERIALMGR->g_idGlassShootThrough)		
		{ 
			m_currColour = Color32(0, 255, 0, m_polyAlpha);						// green	
		} 
		else if (unpackedMtlId == PGTAMATERIALMGR->g_idGlassBulletproof ||
				 unpackedMtlId == PGTAMATERIALMGR->g_idCarGlassBulletproof)		
		{ 
			m_currColour = Color32(255, 0, 0, m_polyAlpha);						// red	
		} 
		else if (unpackedMtlId == PGTAMATERIALMGR->g_idGlassOpaque ||
				 unpackedMtlId == PGTAMATERIALMGR->g_idCarGlassOpaque)		
		{ 
			m_currColour = Color32(0, 0, 255, m_polyAlpha);						// blue	
		} 
		else if (unpackedMtlId==PGTAMATERIALMGR->g_idCarGlassWeak)
		{ 
			m_currColour = Color32(0, 255, 255, m_polyAlpha);					// cyan
		} 	
		else if (unpackedMtlId == PGTAMATERIALMGR->g_idCarGlassMedium) 
		{ 
			m_currColour = Color32(255, 0, 255, m_polyAlpha); 					// magenta
		} 
		else if (unpackedMtlId == PGTAMATERIALMGR->g_idCarGlassStrong)
		{ 
			m_currColour = Color32(255, 255, 0, m_polyAlpha); 					// yellow
		}
		else
		{
			m_currColour = Color32(255, 255, 255, m_polyAlpha);					// white  
		}
	}
	else if (m_currColourMode==MTLCOLOURMODE_FLAGS)
	{
		if (PGTAMATERIALMGR->GetIsSeeThrough(mtlId))
		{
			m_currColour = Color32(0, 255, 255, m_polyAlpha);					// cyan
		}
		else if (PGTAMATERIALMGR->GetIsShootThrough(mtlId))
		{
			m_currColour = Color32(0, 255, 0, m_polyAlpha);						// green
		}
		else if (PGTAMATERIALMGR->GetIsNoDecal(mtlId))
		{
			m_currColour = Color32(255, 0, 255, m_polyAlpha); 					// magenta
		}
		else 
		{
			m_currColour = Color32(255, 255, 255, m_polyAlpha);					// white  
		}
	}

	if (m_litPolys)
	{
		// apply lighting
		float unlitR = m_currColour.GetRedf();
		float unlitG = m_currColour.GetGreenf();
		float unlitB = m_currColour.GetBluef();

		m_currColourLit.SetRed(static_cast<u8>(((unlitR*0.5f)+(nDotL*unlitR*0.5f))*255));
		m_currColourLit.SetGreen(static_cast<u8>(((unlitG*0.5f)+(nDotL*unlitG*0.5f))*255));
		m_currColourLit.SetBlue(static_cast<u8>(((unlitB*0.5f)+(nDotL*unlitB*0.5f))*255));
	}
	else
	{
		m_currColourLit = m_currColour;
	}
}


///////////////////////////////////////////////////////////////////////////////
// ToggleQAMode
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::ToggleQAMode()
{
	PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode = (PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode+1) % MTLDEBUGMODE_NUM;
	if (PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode==MTLDEBUGMODE_BOUND_WIREFRAME)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_BOUND;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_flashPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_litPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderBackFaces = false;
		PGTAMATERIALMGR->GetDebugInterface().m_printObjectNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_vertexShift = 0.06f;
		PGTAMATERIALMGR->GetDebugInterface().m_polyRange = 20.0f;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMaterialNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderVfxGroupNames = false;
	}
	else if (PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode==MTLDEBUGMODE_BOUND_WIREFRAME_MTLNAME)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_BOUND;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_flashPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_litPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderBackFaces = false;
		PGTAMATERIALMGR->GetDebugInterface().m_printObjectNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_vertexShift = 0.06f;
		PGTAMATERIALMGR->GetDebugInterface().m_polyRange = 20.0f;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMaterialNames = true;
		PGTAMATERIALMGR->GetDebugInterface().m_renderVfxGroupNames = false;
	}
	else if (PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode==MTLDEBUGMODE_BOUND_WIREFRAME_VFXGROUPNAME)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_BOUND;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_flashPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_litPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderBackFaces = false;
		PGTAMATERIALMGR->GetDebugInterface().m_printObjectNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_vertexShift = 0.06f;
		PGTAMATERIALMGR->GetDebugInterface().m_polyRange = 20.0f;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMaterialNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderVfxGroupNames = true;
	}
	else if (PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode==MTLDEBUGMODE_VFXGROUP_SOLID)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_VFXGROUP;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_flashPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_litPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_renderBackFaces = false;
		PGTAMATERIALMGR->GetDebugInterface().m_printObjectNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_vertexShift = 0.06f;
		PGTAMATERIALMGR->GetDebugInterface().m_polyRange = 20.0f;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMaterialNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderVfxGroupNames = false;
	}
	else if (PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode==MTLDEBUGMODE_FLAGS_SOLID)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_FLAGS;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_flashPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_litPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_renderBackFaces = false;
		PGTAMATERIALMGR->GetDebugInterface().m_printObjectNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_vertexShift = 0.06f;
		PGTAMATERIALMGR->GetDebugInterface().m_polyRange = 20.0f;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMaterialNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderVfxGroupNames = false;
	}
	else if (PGTAMATERIALMGR->GetDebugInterface().m_quickDebugMode==MTLDEBUGMODE_VFXGROUP_FLAGS_SOLID)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_VFXGROUP;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_FLAGS;
		PGTAMATERIALMGR->GetDebugInterface().m_flashPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_litPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_renderBackFaces = false;
		PGTAMATERIALMGR->GetDebugInterface().m_printObjectNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_vertexShift = 0.06f;
		PGTAMATERIALMGR->GetDebugInterface().m_polyRange = 20.0f;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMaterialNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderVfxGroupNames = false;
	}
	else
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_vertexShift = 0.0f;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMaterialNames = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderVfxGroupNames = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
// ToggleRenderNonClimbableCollision
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::ToggleRenderNonClimbableCollision()
{
	PGTAMATERIALMGR->GetDebugInterface().m_renderNonClimbableCollision = !PGTAMATERIALMGR->GetDebugInterface().m_renderNonClimbableCollision;
	if (PGTAMATERIALMGR->GetDebugInterface().m_renderNonClimbableCollision)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderPolyFlags = true;
		PGTAMATERIALMGR->GetDebugInterface().m_currPolyFlag = POLYFLAG_NOT_CLIMBABLE;
	}
	else
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderPolyFlags = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
// ToggleRenderStairsCollision
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::ToggleRenderStairsCollision()
{
	if (PGTAMATERIALMGR->GetDebugInterface().m_renderStairsCollision)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderPolyFlags = true;
		PGTAMATERIALMGR->GetDebugInterface().m_currPolyFlag = POLYFLAG_STAIRS;
	}
	else
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderPolyFlags = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
// ToggleRenderOnlyWeaponMapBounds
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::ToggleRenderOnlyWeaponMapBounds()
{
	if (PGTAMATERIALMGR->GetDebugInterface().m_renderOnlyWeaponMapBounds)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderOnlyMoverMapBounds = false;

		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_renderWeaponMapBounds = true;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMoverMapBounds = false;
	}
	else
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderWeaponMapBounds = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMoverMapBounds = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
// ToggleRenderOnlyMoverMapBounds
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::ToggleRenderOnlyMoverMapBounds()
{
	if (PGTAMATERIALMGR->GetDebugInterface().m_renderOnlyMoverMapBounds)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderOnlyWeaponMapBounds = false;

		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_renderWeaponMapBounds = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMoverMapBounds = true;
	}
	else
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderWeaponMapBounds = false;
		PGTAMATERIALMGR->GetDebugInterface().m_renderMoverMapBounds = false;
	}
}

///////////////////////////////////////////////////////////////////////////////
// ToggleRenderPedDensity
///////////////////////////////////////////////////////////////////////////////
void phMaterialDebug::SetRenderPedDensity(bool b)
{
	m_renderPedDensityInfo = b;
	m_solidPolys = b;
}

///////////////////////////////////////////////////////////////////////////////
// ToggleRenderPedDensity
///////////////////////////////////////////////////////////////////////////////
void phMaterialDebug::SetRenderPavement(bool b)
{
	m_renderPavementInfo = b;
	m_solidPolys = b;
}


///////////////////////////////////////////////////////////////////////////////
// ToggleRenderCarGlassMaterials
///////////////////////////////////////////////////////////////////////////////

void phMaterialDebug::ToggleRenderCarGlassMaterials()
{
	if (PGTAMATERIALMGR->GetDebugInterface().m_renderCarGlassMaterials)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_DEFAULT1;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_renderCarGlassMaterials = false;
	}
	else
	{
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_GLASS;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_renderCarGlassMaterials = true;
	}
}

void phMaterialDebug::ToggleRenderNonCarGlassMaterials()
{
	if (PGTAMATERIALMGR->GetDebugInterface().m_renderNonCarGlassMaterials)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_DEFAULT1;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_renderNonCarGlassMaterials = false;
	}
	else
	{
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_CONSTANT;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode2 = MTLCOLOURMODE_OFF;
		PGTAMATERIALMGR->GetDebugInterface().m_renderNonCarGlassMaterials = true;
	}
}

void phMaterialDebug::ToggleRenderFindCollisionHoles()
{
	const bool bExcludeDynamicObjects = true;

	if (PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys)
	{
		if (PGTAMATERIALMGR->GetDebugInterface().m_solidPolys) // solid -> wireframe
		{
			PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = false;
		}
		else // turn off
		{
			PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = false;
			PGTAMATERIALMGR->GetDebugInterface().m_polyRangeXY = false;
			PGTAMATERIALMGR->GetDebugInterface().m_polyProjectToScreen = false;
			PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_DEFAULT1;
		}
	}
	else // turn on (solid)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_solidPolys = true;
		PGTAMATERIALMGR->GetDebugInterface().m_polyRangeXY = true;
		PGTAMATERIALMGR->GetDebugInterface().m_polyProjectToScreen = true;
		PGTAMATERIALMGR->GetDebugInterface().m_colourMode1 = MTLCOLOURMODE_CONSTANT;
	}

	if (bExcludeDynamicObjects)
	{
		PGTAMATERIALMGR->GetDebugInterface().m_excludeFragObjects	= PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys;
		PGTAMATERIALMGR->GetDebugInterface().m_excludeVehicles		= PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys;
		PGTAMATERIALMGR->GetDebugInterface().m_excludeFragVehicles	= PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys;
		PGTAMATERIALMGR->GetDebugInterface().m_excludePeds			= PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys;
		PGTAMATERIALMGR->GetDebugInterface().m_excludeFragPeds		= PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys;
	}
}

void phMaterialDebug::ToggleRenderFindCollisionHolesWeaponMapBounds()
{
	ToggleRenderFindCollisionHoles();

	PGTAMATERIALMGR->GetDebugInterface().m_renderWeaponMapBounds = true;
	PGTAMATERIALMGR->GetDebugInterface().m_renderMoverMapBounds = !PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys;
}

void phMaterialDebug::ToggleRenderFindCollisionHolesMoverMapBounds()
{
	ToggleRenderFindCollisionHoles();

	PGTAMATERIALMGR->GetDebugInterface().m_renderMoverMapBounds = true;
	PGTAMATERIALMGR->GetDebugInterface().m_renderWeaponMapBounds = !PGTAMATERIALMGR->GetDebugInterface().m_renderAllPolys;
}

#endif // __BANK
