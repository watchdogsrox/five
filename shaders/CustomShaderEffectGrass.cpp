//
// Filename:	CustomShaderEffectGrass.cpp
// Description:	Class for controlling grass shader variables
//
//

#include "CustomShaderEffectGrass.h"

// Rage headers
#include "fwdrawlist/drawlistmgr.h"
#include "grmodel/ShaderFx.h"
#include "grmodel/Geometry.h"
#include "grcore/texturereference.h"
#include "rmcore/drawable.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/Cinematic/CinematicDirector.h"
#include "debug/debug.h"
#include "game/ModelIndices.h"
#include "objects/Object.h"
#include "objects/ProceduralInfo.h"
#include "Peds/ped.h"
#include "renderer/render_channel.h"
#include "renderer/PlantsGrassRenderer.h"
#include "shader_source/Vegetation/Grass/grass_regs.h"
#include "scene/Entity.h"
#include "scene/EntityBatch.h"
#include "scene/world/GameWorld.h"
#include "script/script_channel.h"
#include "vehicles/Vehicle.h"
#include "vehicles/Automobile.h"
#include "shaders/ShaderLib.h"
#include "renderer/DrawLists/drawlist.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"

#if HACK_RDR3
#include "renderer/Entities/TrailMapRenderer.h"
#endif

#include "../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"

RENDER_OPTIMISATIONS();

static dev_float PLAYER_COLL_RADIUS0 = 0.75f;	// radius of influence

namespace CSEGrassStatic
{
	// script vehicle:
	bool		sm_ScrVehEnabled = false;
	spdAABB		sm_ScrVehAABB(Vec3V(V_FLT_MAX), -Vec3V(V_FLT_MAX));	//Initialize as invalid AABB.
	Vec3V		sm_ScrVehB(V_ZERO_WONE);
	Vec3V		sm_ScrVehM(V_ZERO_WONE);
	float		sm_ScrVehRadius = 0.0f;
	float		sm_ScrVehGroundZ = 0.0f;

	BankBool	sm_ScrVehDebugDraw = false;
	BankBool	sm_ScrVehAllowEntityIntersection = true;

	void ClearScriptVehicle()
	{
		sm_ScrVehAABB.Invalidate();
		sm_ScrVehEnabled = false;
		sm_ScrVehB = Vec3V(V_ZERO_WONE);
		sm_ScrVehM = Vec3V(V_ZERO_WONE);
		sm_ScrVehRadius = 0.0f;
		sm_ScrVehGroundZ = 0.0f;
	}

	float ProbeForMinGroundPos(const spdAABB &aabb, ScalarV groundZ = ScalarV(V_FLT_MAX))
	{
		static dev_float fMinGroundAdjust = 0.0f;

		Vec3V boxMin = aabb.GetMin();
		Vec3V boxMax = aabb.GetMax();
		ScalarV probeDistance = (boxMax.GetZ() - boxMin.GetZ()) * ScalarV(V_NEGSIXTEEN);
		rage::atRangeArray<Vec3V, 4> probePos((Vec3V(V_ZERO_WONE)));
		probePos[0] = rage::GetFromTwo<Vec::X1, Vec::Y1, Vec::Z2>(boxMin, boxMax);
		probePos[1] = rage::GetFromTwo<Vec::X1, Vec::Y2, Vec::Z2>(boxMin, boxMax);
		probePos[2] = boxMax; //rage::GetFromTwo<Vec::X2, Vec::Y2, Vec::Z2>(boxMin, boxMax);
		probePos[3] = rage::GetFromTwo<Vec::X2, Vec::Y1, Vec::Z2>(boxMin, boxMax);

		//Make sure script ground Z is the minimum for the AABB.
		ScalarV minGroundZ(groundZ);
		for(u32 i = 0; i < 4; ++i)
		{
			WorldProbe::CShapeTestProbeDesc probeDesc;
			WorldProbe::CShapeTestHitPoint probeIsect;
			WorldProbe::CShapeTestResults probeResult(probeIsect);
			probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(probePos[i]), VEC3V_TO_VECTOR3(probePos[i] + (Vec3V(V_UP_AXIS_WZERO) * probeDistance)));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc.SetContext(WorldProbe::ENotSpecified);

			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				minGroundZ = Min(probeResult[0].GetHitPositionV().GetZ(), minGroundZ);
		}
		//scriptAssertf(groundZ != ScalarV(V_FLT_MAX), "None of the grass flattening AABB probes successfully found a ground intersection, and no value was provided!");

		return minGroundZ.Getf() + fMinGroundAdjust;	
	}

	void SetScriptVehicle(const spdSphere &sphere, Vec3V_In look, ScalarV_In groundZ = ScalarV(V_FLT_MAX))
	{
		scriptAssertf(!sm_ScrVehEnabled, "Grass Flatten AABB already set. Setting again without clearing will overwrite previous entry.");
		sm_ScrVehEnabled = true;

		sm_ScrVehAABB.Invalidate();
		sm_ScrVehAABB.GrowSphere(sphere);

		if(IsEqualAll(look, Vec3V(V_ZERO)))
		{
		#if RSG_DURANGO
			// hack for B*4230572 (XB1: Grass can be seen clipping through lift when entering facility):
			sm_ScrVehM		= Vec3V(V_ZERO);
			sm_ScrVehB		= sphere.GetCenter();
			sm_ScrVehRadius = sphere.GetRadiusf() * 2.0f;
		#else
			sm_ScrVehM = NormalizeSafe(look, Vec3V(V_FLT_MAX)) * sphere.GetRadius();
			sm_ScrVehB = sphere.GetCenter() - (sm_ScrVehM * ScalarV(V_HALF));
			sm_ScrVehRadius = sphere.GetRadiusf();
		#endif
		}
		else
		{
			sm_ScrVehM = Normalize(look) * sphere.GetRadius();
			sm_ScrVehB = sphere.GetCenter() - (sm_ScrVehM * ScalarV(V_HALF));
			sm_ScrVehRadius = sphere.GetRadiusf();
		}


		sm_ScrVehGroundZ = ProbeForMinGroundPos(sm_ScrVehAABB, groundZ);
	}

	void SetScriptVehicle(const spdAABB &aabb, Vec3V_In look, ScalarV_In groundZ = ScalarV(V_FLT_MAX))
	{
		scriptAssertf(!sm_ScrVehEnabled, "Grass Flatten AABB already set. Setting again without clearing will overwrite previous entry.");
		sm_ScrVehEnabled = true;
		sm_ScrVehAABB = aabb;

		if(IsEqualAll(look, Vec3V(V_ZERO)))
		{
		#if RSG_DURANGO
			// hack for B*4230572 (XB1: Grass can be seen clipping through lift when entering facility):
			sm_ScrVehM = Vec3V(V_ZERO);
			sm_ScrVehB = aabb.GetCenter();
		#else
			sm_ScrVehM = NormalizeSafe(look, Vec3V(V_FLT_MAX)) * aabb.GetExtent();
			sm_ScrVehB = aabb.GetCenter() - (sm_ScrVehM * ScalarV(V_HALF));
		#endif
		}
		else
		{
			sm_ScrVehM = Normalize(look) * aabb.GetExtent();
			sm_ScrVehB = aabb.GetCenter() - (sm_ScrVehM * ScalarV(V_HALF));
		}


		sm_ScrVehRadius = Mag(sm_ScrVehM).Getf();
		sm_ScrVehGroundZ = ProbeForMinGroundPos(sm_ScrVehAABB, groundZ);
	}

	bool IntersectsScriptVehicleAABB(fwEntity *entity)
	{
		if(entity && sm_ScrVehEnabled && sm_ScrVehAllowEntityIntersection)
		{
			spdAABB aabb;
			entity->GetAABB(aabb);
			return aabb.IntersectsAABB(sm_ScrVehAABB);
		}

		return false;
	}
}

namespace EBStatic
{
#if __BANK
	extern BankFloat	sDebugDrawGrassTerrainNormalConeLength;
	extern BankFloat	sDebugDrawGrassTerrainNormalConeRadius;
	extern BankBool		sDebugDrawGrassTerrainNormalConeCap;
#endif
#if RSG_PC
	extern u32			sPerfSkipInstance;
	extern bool			bPerfForceSkipInstance;
#endif //RSG_PC...
}

namespace CSEGrassBankStatic
{
	BankBool	sm_bEnableMicroMovements= true;
	BankBool	sm_bEnableWinBending	= true;
	BankBool	sm_bOverrideAO			= false;
	BankFloat	sm_fGlobalA0			= GRASS_GLOBAL_AO;
	BankBool	sm_bAlphaOverdraw		= false;

#if __BANK
	Vec3V		sm_ScrVehCreateAABBMin(V_ZERO_WONE);
	Vec3V		sm_ScrVehCreateAABBMax(V_ZERO_WONE);
	Vec4V		sm_ScrVehCreateSphere(V_ZERO_WONE);
	Vec3V		sm_ScrVehCreateLook(V_Y_AXIS_WONE);

	void ScriptVehicleClearFromRag()
	{
		CSEGrassStatic::ClearScriptVehicle();
	}

	void ScriptVehicleCreateFromAABB()
	{
		sm_ScrVehCreateLook = Normalize(sm_ScrVehCreateLook);
		ScriptVehicleClearFromRag();
		CSEGrassStatic::SetScriptVehicle(spdAABB(sm_ScrVehCreateAABBMin, sm_ScrVehCreateAABBMax), sm_ScrVehCreateLook);
	}

	void ScriptVehicleCreateFromSphere()
	{
		sm_ScrVehCreateLook = Normalize(sm_ScrVehCreateLook);
		ScriptVehicleClearFromRag();
		CSEGrassStatic::SetScriptVehicle(spdSphere(sm_ScrVehCreateSphere), sm_ScrVehCreateLook);
	}

	void ScriptVehicleCreate_GetAABBFromPed()
	{
		if(CPed *player = CGameWorld::FindLocalPlayer())
		{
			spdAABB aabb;
			player->GetAABB(aabb);
			sm_ScrVehCreateAABBMin = aabb.GetMin();
			sm_ScrVehCreateAABBMax = aabb.GetMax();

			sm_ScrVehCreateLook = player->GetTransform().GetB();
		}
	}

	void ScriptVehicleCreate_GetSphereFromPed()
	{
		if(CPed *player = CGameWorld::FindLocalPlayer())
		{
			sm_ScrVehCreateLook = player->GetTransform().GetB();

			spdAABB aabb;
			player->GetAABB(aabb);
			ScalarV radius = Mag(Normalize(sm_ScrVehCreateLook) * aabb.GetExtent());
			sm_ScrVehCreateSphere = Vec4V(player->GetTransform().GetPosition(), radius);
		}
	}

	void ScriptVehicleCreate_CreateFromPedAABB()
	{
		ScriptVehicleCreate_GetAABBFromPed();
		ScriptVehicleCreateFromAABB();
	}

	void ScriptVehicleCreate_CreateFromPedSphere()
	{
		ScriptVehicleCreate_GetSphereFromPed();
		ScriptVehicleCreateFromSphere();
	}

	void AddWidgets(bkBank &bank)
	{
		bank.PushGroup("CSEGrass");
		{
			bank.AddToggle("Enable MicroMovements",	&sm_bEnableMicroMovements);
			bank.AddToggle("Enable Wind Bending",	&sm_bEnableWinBending);
			bank.AddToggle("Alpha Overdraw",		&sm_bAlphaOverdraw);
			bank.PushGroup("Global AO");
				bank.AddToggle("Enable Edit",		&sm_bOverrideAO);
				bank.AddSlider("Global AO",			&sm_fGlobalA0,	0.0f, 1.0f, 0.01f);
			bank.PopGroup();

			bank.PushGroup("Script Vehicle");
			{
				bank.AddToggle("Debug Draw", &CSEGrassStatic::sm_ScrVehDebugDraw);
				bank.AddToggle("Allow culling entities intersecting Script Vehicle AABB", &CSEGrassStatic::sm_ScrVehAllowEntityIntersection);

				bank.PushGroup("Debug Create");
				{
					bank.AddTitle("Debug widgets to simulate script command to create invisible script vehicle.");
					bank.AddVector("AABB Min",	&sm_ScrVehCreateAABBMin, -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 0.1f);
					bank.AddVector("AABB Max",	&sm_ScrVehCreateAABBMax, -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 0.1f);
					bank.AddTitle("");
					bank.AddVector("Sphere", &sm_ScrVehCreateSphere, -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 0.1f);
					bank.AddTitle("");
					bank.AddVector("Look", &sm_ScrVehCreateLook, -1.0f, 1.0f, 0.1f);
					bank.AddTitle("");
					bank.AddButton("Create Script Vehicle From Ped AABB", &ScriptVehicleCreate_CreateFromPedAABB);
					bank.AddButton("Create Script Vehicle From Ped Sphere", &ScriptVehicleCreate_CreateFromPedSphere);
					bank.AddTitle("");
					bank.AddButton("Create Script Vehicle From AABB", &ScriptVehicleCreateFromAABB);
					bank.AddButton("Create Script Vehicle From Sphere", &ScriptVehicleCreateFromSphere);
					bank.AddButton("Clear Script Vehicle", &ScriptVehicleClearFromRag);
					bank.AddTitle("");
					bank.AddButton("Get Ped AABB", &ScriptVehicleCreate_GetAABBFromPed);
					bank.AddButton("Get Ped Sphere", &ScriptVehicleCreate_GetSphereFromPed);
				}
				bank.PopGroup();

				bank.PushGroup("Vars");
				{
					bank.AddToggle("Enable Script Vehicle",	&CSEGrassStatic::sm_ScrVehEnabled);
					bank.AddVector("Vec B",	&CSEGrassStatic::sm_ScrVehB, -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 0.1f);
					bank.AddVector("Vec M", &CSEGrassStatic::sm_ScrVehM, -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 0.1f);
					bank.AddSlider("Radius", &CSEGrassStatic::sm_ScrVehRadius, 0.0f, FLT_MAX / 1000.0f, 0.1f);
					bank.AddSlider("GroundZ", &CSEGrassStatic::sm_ScrVehGroundZ, -FLT_MAX / 1000.0f, FLT_MAX / 1000.0f, 0.1f);
				}
				bank.PopGroup();
			}
			bank.PopGroup();
		}
		bank.PopGroup();
	}

	void RenderDebug()
	{
		if(CSEGrassStatic::sm_ScrVehEnabled && CSEGrassStatic::sm_ScrVehDebugDraw)
		{
			static Color32 sYellow = Color32(1.0f, 1.0f, 0.0f, 1.0f);
			static Color32 sBlue = Color32(0.0f, 0.0f, 1.0f, 1.0f);

			//Draw AABB
			grcDebugDraw::BoxAxisAligned(CSEGrassStatic::sm_ScrVehAABB.GetMin(), CSEGrassStatic::sm_ScrVehAABB.GetMax(), sYellow, false);

			//Draw B+M vec.
			Vec3V v1 = CSEGrassStatic::sm_ScrVehB;
			Vec3V v2 = CSEGrassStatic::sm_ScrVehB + CSEGrassStatic::sm_ScrVehM;
			grcDebugDraw::Line(v1, v2, sBlue);

			ScalarV coneLen(EBStatic::sDebugDrawGrassTerrainNormalConeLength / 2.0f); //0.3 / 2.0f);
			Vec3V c1 = v2 + (CSEGrassStatic::sm_ScrVehM * coneLen);
			Vec3V c2 = v2 - (CSEGrassStatic::sm_ScrVehM * coneLen);
			grcDebugDraw::Cone(c1, c2, EBStatic::sDebugDrawGrassTerrainNormalConeRadius, sBlue, EBStatic::sDebugDrawGrassTerrainNormalConeCap); //0.07, sBlue, true);
		}
	}
#endif
}


//
//
//
//
inline u8 LookupGroupVar(grmShaderGroup *pShaderGroup, const char *name, bool mustExist)
{
	grmShaderGroupVar varID = pShaderGroup->LookupVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

inline u8 LookupGlobalVar(const char *name, bool mustExist)
{
	grcEffectGlobalVar  varID = grcEffect::LookupGlobalVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return static_cast<u8>(varID);
}

//
//
// initializes all instances of the effect class;
// to be called once, when "master" class is created;
//
bool CCustomShaderEffectGrassType::Initialise(rmcDrawable *pDrawable)
{
	Assert(pDrawable);

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();

	//Initialize CS shader params
	{
		m_CSVars.m_idVarInstanceBuffer = pShaderGroup->LookupVar("InstanceBuffer", false);
		m_CSVars.m_idVarRawInstBuffer = pShaderGroup->LookupVar("RawInstanceBuffer", false);
		m_CSVars.m_idVarUseCSOutputBuffer = pShaderGroup->LookupVar("gUseComputeShaderOutputBuffer", false);

		m_CSVars.m_idAabbMin = pShaderGroup->LookupVar("vecBatchAabbMin0");
		m_CSVars.m_idAabbDelta = pShaderGroup->LookupVar("vecBatchAabbDelta0");
		m_CSVars.m_idScaleRange = pShaderGroup->LookupVar("gScaleRange");

		//Find compute shader index
		for(int i = 0; i < pShaderGroup->GetCount(); ++i)
		{
			grmShader &shader = pShaderGroup->GetShader(i);
			if(shader.LookupTechnique("BatchCull", false) != grcetNONE)
			{
				m_CSVars.m_ShaderIndex = i;
				for(int i = 0; i < m_CSVars.m_idVarAppendInstBuffer.size(); ++i)
				{
					char buff[32];
					formatf(buff, "VisibleInstanceBuffer%d", i);
					m_CSVars.m_idVarAppendInstBuffer[i] = shader.LookupVar(buff);
				}
				DURANGO_ONLY(GRASS_BATCH_CS_DYNAMIC_BUFFERS_ONLY(m_CSVars.m_idIndirectArgs = shader.LookupVar("IndirectArgs")));
				m_CSVars.m_idInstCullParams = shader.LookupVar("gInstCullParams");
				m_CSVars.m_idNumClipPlanes = shader.LookupVar("gNumClipPlanes");
				m_CSVars.m_idClipPlanes = shader.LookupVar("gClipPlanes");

				m_CSVars.m_idCameraPosition = shader.LookupVar("gCameraPosition");
				m_CSVars.m_idLodInstantTransition = shader.LookupVar("gLodInstantTransition");
#if RSG_DURANGO || RSG_PC
				m_CSVars.m_idGrassSkipInstance = shader.LookupVar("gGrassSkipInstance",false);
#endif // RSG_DURANGO || RSG_PC...
				m_CSVars.m_idLodThresholds = shader.LookupVar("gLodThresholds");
				m_CSVars.m_idCrossFadeDistance = shader.LookupVar("gCrossFadeDistance");

				m_CSVars.m_idIsShadowPass = shader.LookupVar("gIsShadowPass");
				m_CSVars.m_idLodFadeControlRange = shader.LookupVar("gLodFadeControlRange");
				DURANGO_ONLY(m_CSVars.m_idIndirectCountPerLod = shader.LookupVar("gIndirectCountPerLod"));
				break;
			}
		}

		m_CSVars.UpdateValidity();
	}

	m_idOrientToTerrain			= LookupGroupVar(pShaderGroup, "gOrientToTerrain", false);
	m_idPlayerPos				= LookupGroupVar(pShaderGroup, "vecPlayerPos0", false);

	m_idWindBending				= LookupGroupVar(pShaderGroup, "gWindBendingGlobals", false);
	m_idLodFadeInstRange		= LookupGroupVar(pShaderGroup, "gLodFadeInstRange", false);

#if ENABLE_TRAIL_MAP
	m_idTrailMapTexture			= LookupGroupVar(pShaderGroup, "trailMapTexture0", false);
	m_idMapSizeWorldSpace		= LookupGroupVar(pShaderGroup, "mapSizeWorldSpace", false);
#endif

	//Do we want to set any of these?
	//groundPolyVertPos0
	//gStippleThreshold

	m_idCollParams				= LookupGroupVar(pShaderGroup, "vecCollParams0", false);

	//Micromovement Params
	m_idFadeAlphaDistUmTimer	= LookupGroupVar(pShaderGroup, "fadeAlphaDistUmTimer0", false);

	m_idFakedGrassNormal		= LookupGroupVar(pShaderGroup, "fakedGrassNormal0", false);

	m_idDebugSwitches			= LookupGroupVar(pShaderGroup, "bDebugSwitches0", false);

	for(u32 i=0; i<NUM_COL_VEH; i++)
	{
		char var_name[128];
		
		formatf(var_name, "bVehColl%dEnabled0", i);	// bVehColl0Enabled0
		m_idVarVehCollEnabled[i]= LookupGlobalVar(var_name, false);

		formatf(var_name, "vecVehColl%dB0", i); // vecVehColl0B0
		m_idVarVehCollB0[i]		= LookupGroupVar(pShaderGroup, var_name, false);
		
		formatf(var_name, "vecVehColl%dM0", i); // vecVehColl0M0
		m_idVarVehCollM0[i]		= LookupGroupVar(pShaderGroup, var_name, false);

		formatf(var_name, "vecVehColl%dR0", i); // "vecVehColl0R0"
		m_idVarVehCollR0[i]		= LookupGroupVar(pShaderGroup, var_name, false);
	}

#if DEVICE_MSAA
	m_idAlphaToCoverageScale	= LookupGroupVar(pShaderGroup, "gAlphaToCoverageScale", false);
#endif

	static dev_float maxVehCollisionDist	= 25.0f;	// 25m distance
	m_fMaxVehicleCollisionDist	= maxVehCollisionDist;

	return(TRUE);
}// end of CCustomShaderEffectGrass::Initialise()...

CCustomShaderEffectGrassType::~CCustomShaderEffectGrassType()
{
}

//
//
//
//
CCustomShaderEffectBase*	CCustomShaderEffectGrassType::CreateInstance(CEntity* pEntity)
{
	CCustomShaderEffectGrass *pNewShaderEffect = rage_new CCustomShaderEffectGrass;
	pNewShaderEffect->m_pType = this;

	float phaseShift = 0.0f;
	spdAABB aabb;
	Vec3V scaleRange(1.0f, 1.0f, 0.0f);
	float orientToTerrain = 0.0f;
	Vec3V lodFadeInstRange(V_ONE_WZERO);
	if(pEntity)
	{
		// calculate localized micro-movement phase shift based on xy coords of the tree:
		const Vec3V entityPos = pEntity->GetTransform().GetPosition();
		const u32 phaseShiftX = u32( Abs(entityPos.GetXf()*10.0f) )&0x7f;
		const u32 phaseShiftY = u32( Abs(entityPos.GetYf()*10.0f) )&0x7f;
		phaseShift = float(phaseShiftX+phaseShiftY)/255.0f - 1.0f;

		pEntity->GetAABB(aabb);

		if(Verifyf(pEntity->GetIsTypeGrassInstanceList(), "WARNING: Entity type (%s) is not of type Grass Batch!", pEntity->GetModelName()))
		{
			CGrassBatch *eb = static_cast<CGrassBatch *>(pEntity);
			scaleRange = eb->GetInstanceList()->m_ScaleRange;    
			orientToTerrain = eb->GetInstanceList()->m_OrientToTerrain;
			lodFadeInstRange = eb->GetInstanceList()->ComputeLodInstFadeRange(ScalarV(eb->GetDistanceToCamera()));
		}
	}
	pNewShaderEffect->m_umPhaseShift = phaseShift;

	pNewShaderEffect->m_AabbMin = aabb.GetMin();
	pNewShaderEffect->m_AabbDelta = aabb.GetMax() - aabb.GetMin();
	pNewShaderEffect->m_scaleRange = scaleRange;

	//Set W components of Vec3V vars to 0 so x64 build doesn't complain about setting shader vars to QNAN.
	pNewShaderEffect->m_AabbMin.SetWZero();
	pNewShaderEffect->m_AabbDelta.SetWZero();
	pNewShaderEffect->m_scaleRange.SetWZero();

	pNewShaderEffect->m_LodFadeInstRange = lodFadeInstRange;

	pNewShaderEffect->m_orientToTerrain = orientToTerrain;

	for(u32 i=0; i<CCustomShaderEffectGrassType::NUM_COL_VEH; i++)
	{
		pNewShaderEffect->m_bVehCollisionEnabled[i] = false;
	}

	//TODO - AJG-12.28.2012: Remove & Fixed in tools! (Might already be fixed in migration.)
	if(!pEntity->GetLodData().IsTree())
	{
		//If we reach here, scan task will not be able to identify this entity as a tree! Thus, it will not be rendered with all
		//other trees and will get SSA applied if SSA for cutouts is enabled. This is a temp hack to fix the issue.
#if !__GAMETOOL
		//Let the user know this problem was found and fixed up.
		//Note:	This currently happens quite a bit in some scenes and currently there's nothing that can be done. (Waiting to see if migration fixed the issue.)
		//		So instead of asserting, I just output a warning.
		//		If this is still a problem post-migration, we'll need to find a real solution. (And may want to change this to an assert with the steps to fix)
		renderWarningf(	"WARNING! Found a grass entity that's not marked as a tree! This would likely cause it to draw as SSA if SSA is enabled, "
						"or otherwise just not as part of the tree pass.\n\t\t\t\t  Fixing up the entity and BaseModelInfo so this is properly identified as a tree.\n"
						"\t\t\t\t  Entity Name:\t%s", pEntity->GetModelName() ? pEntity->GetModelName() : "<NULL>");
#endif // !__GAMETOOL
		//Set the entity flag properly and also fix the base model info which sets this flag so future instances of this type should not have the same problem.
		pEntity->GetLodData().SetIsTree(true);

		if(CBaseModelInfo *baseModelInfo = pEntity->GetBaseModelInfo())
			baseModelInfo->SetIsTree(true);
	}

	return(pNewShaderEffect);
}


//
//
//
//
CCustomShaderEffectGrass::CCustomShaderEffectGrass() : CCustomShaderEffectBase(sizeof(*this), CSE_GRASS)
{
	// do nothing
}

//
//
//
//
CCustomShaderEffectGrass::~CCustomShaderEffectGrass()
{
}


void CCustomShaderEffectGrass::AddToDrawList(u32 modelIndex, bool bExecute)
{
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
}

void CCustomShaderEffectGrass::AddToDrawListAfterDraw()
{
}

namespace CSEGrassStatic
{
	DECLARE_MTR_THREAD grcRasterizerStateHandle sPrevRSBlock = grcStateBlock::RS_Invalid;
	void CacheRSBlock()
	{
		Assert(sysThreadType::IsRenderThread());
		Assertf(sPrevRSBlock == grcStateBlock::RS_Invalid, "WARNING! CSEGrassStatic::CacheRSBlock() is trying to push a state block before popping the previously pushed block.");
		sPrevRSBlock = grcStateBlock::RS_Active;
	}

	void RestoreCachedRSBlock()
	{
		Assert(sysThreadType::IsRenderThread());
		Assertf(sPrevRSBlock != grcStateBlock::RS_Invalid,	"WARNING! CSEGrassStatic::RestoreCachedRSBlock() is trying to restore an invalid state block. Make sure a matching CacheRSBlock " 
			"was called 1st and that RestoreCachedRSBlock was not called multiple times in a row without a call to CacheRSBlock in between.");
		grcStateBlock::SetRasterizerState(sPrevRSBlock);
		sPrevRSBlock = grcStateBlock::RS_Invalid;
	}
}

void CCustomShaderEffectGrass::AddBatchRenderStateToDrawList() const
{
	//Setup tree render states
	const bool isShadowDrawList = static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingShadowDrawList();

	if(isShadowDrawList)
	{
		DLC_Add(&CRenderPhaseCascadeShadowsInterface::SetRasterizerState, CSM_RS_TWO_SIDED);
	}
	else
	{
		DLC_Add(&CSEGrassStatic::CacheRSBlock);
		DLC_Add(&CShaderLib::SetFacingBackwards, false);
		DLC_Add(&CShaderLib::SetDisableFaceCulling);
	}
}

void CCustomShaderEffectGrass::AddBatchRenderStateToDrawListAfterDraw() const
{
	//Restore render states
	const bool isShadowDrawList = static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingShadowDrawList();
	if(isShadowDrawList)
	{
		DLC_Add(&CRenderPhaseCascadeShadowsInterface::RestoreRasterizerState);
	}
	else
	{
		DLC_Add(&CSEGrassStatic::RestoreCachedRSBlock);
	}
}

//
//
//
//
void CCustomShaderEffectGrass::Update(fwEntity* pEntity)
{
	Assert(pEntity);

#if RSG_PC
	// IB grass density settings tied to shader settings:
	if(BANK_SWITCH( (!EBStatic::bPerfForceSkipInstance), true))
	{
		const u32 skipIdx = MIN(u32(CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShaderQuality), (u32)CSettings::High);
		const u32 skipMask[3] = {1, 3, 15};	// Low, Medium, High
		EBStatic::sPerfSkipInstance = skipMask[skipIdx];
	}
#endif //RSG_PC...


	// enable players collision only if he is close enough to the tree:
//	const Vec3V entityPos = pEntity->GetTransform().GetPosition();
	const Vec3V playerPos = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

	//Compute Fade Ranges
	if(pEntity && pEntity->GetType() == ENTITY_TYPE_GRASS_INSTANCE_LIST)
	{
		CGrassBatch *eb = static_cast<CGrassBatch *>(pEntity);
		m_LodFadeInstRange = eb->GetInstanceList()->ComputeLodInstFadeRange(ScalarV(eb->GetDistanceToCamera()));
	}

	// player collision:
	m_collPlayerScaleSqr = PLAYER_COLL_RADIUS0*PLAYER_COLL_RADIUS0;

	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if(pPlayerPed)
	{
		if(pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
		{
			static dev_float peyoteScale = 0.001f;
			m_collPlayerScaleSqr = peyoteScale*peyoteScale;
		}
	}


	if(m_pType->IsVehicleDeformable())
	{
		// find 4 nearest vehicles to camera:

		bool bCameraVehicleFPV = false;	// camera in First Person View in vehicle?
	#if FPS_MODE_SUPPORTED
		bCameraVehicleFPV = camInterface::GetCinematicDirector().IsRenderingAnyInVehicleFirstPersonCinematicCamera();
	#endif

		// enable players collision only if he is close enough to the tree:
		const float maxVehCollisionDist = m_pType->GetMaxVehicleCollisionDist();

		CVehicle*	pVehicles[CCustomShaderEffectGrassType::NUM_COL_VEH]		= {NULL,		NULL,		NULL,		NULL		};
		float		fVehicleDist[CCustomShaderEffectGrassType::NUM_COL_VEH]	= {999999.0f,	999999.0f,	999999.0f,	999999.0f	};	// vehicle dist sqr to camera

		//Grab the spatial array.
		const CSpatialArray& rSpatialArray = CVehicle::GetSpatialArray();
		static const int sMaxResults = 20;
		CSpatialArray::FindResult result[sMaxResults];
		int iFound = rSpatialArray.FindInSphere(/*entityPos*/playerPos, maxVehCollisionDist, &result[0], sMaxResults);

		int iEmptySlot = 0;
		//Iterate over the results.
		for(int iVeh = 0; iVeh < iFound; ++iVeh)
		{
			CVehicle* pVehicle = CVehicle::GetVehicleFromSpatialArrayNode(result[iVeh].m_Node);
			const VehicleType vehType = pVehicle->GetVehicleType();

			// disable vehicle collision for helis and planes:
			pVehicle = ((vehType==VEHICLE_TYPE_HELI) || (vehType==VEHICLE_TYPE_PLANE) || (vehType==VEHICLE_TYPE_BLIMP))?	NULL : pVehicle;

			if(bCameraVehicleFPV)
			{
				// BS#2057861: FPV in vehicle - always enable grass collision
			}
			else
			{
				if(pVehicle && (pVehicle->IsUpsideDown() || pVehicle->IsOnItsSide()))
				{
					pVehicle = NULL;	// no grass collision if vehicle is not firmly on the ground
				}

				// BS#2509157: check if lowrider hydraulics car is bouncing:
				bool bIsCarHydraulicsBouncing = false;
				if(pVehicle && pVehicle->InheritsFromAutomobile())
				{
					CAutomobile *pCar = (CAutomobile*)pVehicle;
					if(pCar && (pVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics || pCar->m_nAutomobileFlags.bHydraulicsBounceRaising || pCar->m_nAutomobileFlags.bHydraulicsBounceLanding || pCar->m_nAutomobileFlags.bHydraulicsJump))
					{
						bIsCarHydraulicsBouncing = true;
					}
				}
 
				const u32 modelNameHash = pVehicle? pVehicle->GetBaseModelInfo()->GetModelNameHash() : 0;

				// BS#4036767 - Deluxo - Changing from driving to flying/floating affects grass collision
				bool bIsHooveringDeluxo = false;
				if(pVehicle && modelNameHash==MID_DELUXO)
				{
					bIsHooveringDeluxo = pVehicle->HoveringCloseToGround();
				}

				if(bIsCarHydraulicsBouncing)
				{
					// do nothing - vehicle is bouncing via hydraulics
				}
				else if(pVehicle && modelNameHash==MID_BLAZER5)
				{
					// do nothing - special case for blazer5
				}
				else if(bIsHooveringDeluxo)
				{
					// do nothing - deluxo is hoovering close to the ground
				}
				else if(pVehicle && (pVehicle->GetNumContactWheels() < pVehicle->GetNumWheels()))
				{
					pVehicle = NULL;	// no grass collision if vehicle is not firmly on the ground
				}

				// B*2491123: disable collision for attached vehicles:
				if(pVehicle && pVehicle->GetIsAttached())
				{
					CPhysical* pAttachParent = (CPhysical*)pVehicle->GetAttachParent();
					if(pAttachParent)
					{
						pVehicle = NULL;	// no grass collision for attached vehicles
					}
				}
			}

			if(pVehicle)
			{	// [HACK GTAV] BS#1105797: disable grass collision for dump:
				// [HACK GTAV] BS#1945307: disable grass collision for monster;
				// [HACK GTAV] BS#1998362: disable grass collision for trailers;
				const u32 uModelNameHash = pVehicle->GetBaseModelInfo()->GetModelNameHash();
				pVehicle = (uModelNameHash==MID_DUMP)?			NULL : pVehicle;
				pVehicle = (uModelNameHash==MID_MONSTER)?		NULL : pVehicle;
				pVehicle = (uModelNameHash==MID_TRAILERS)?		NULL : pVehicle;
				pVehicle = (uModelNameHash==MID_TRAILERS2)?		NULL : pVehicle;
				pVehicle = (uModelNameHash==MID_TRAILERS3)?		NULL : pVehicle;
				pVehicle = (uModelNameHash==MID_TRAILERLOGS)?	NULL : pVehicle;
			}

			if(pVehicle)
			{
				const float distSqr = result[iVeh].m_DistanceSq;

				if(iEmptySlot < CCustomShaderEffectGrassType::NUM_COL_VEH)
				{
					fVehicleDist[iEmptySlot] = distSqr;
					pVehicles[iEmptySlot] = pVehicle;
					iEmptySlot++;
					continue;
				}

				// find vehicle's index which is most far away from the camera:
				u32		fari	= 0;
				float	farDist	= fVehicleDist[0];
				for(u32 i=1; i<CCustomShaderEffectGrassType::NUM_COL_VEH; i++)
				{
					if( fVehicleDist[i] > farDist )
					{
						farDist = fVehicleDist[i];
						fari	= i;
					}
				}
				Assert(fari < CCustomShaderEffectGrassType::NUM_COL_VEH);

				// dist2 smaller than furthest found?
				if( distSqr < farDist )
				{
					fVehicleDist[fari]	= distSqr;
					pVehicles[fari]		= pVehicle;	
				}
			}// if(pVehicle)...
		}

		spdAABB batchAABB;
		pEntity->GetAABB(batchAABB);
		const bool hasScriptVehicle = CSEGrassStatic::sm_ScrVehEnabled && (batchAABB.IntersectsAABB(CSEGrassStatic::sm_ScrVehAABB) || batchAABB.IntersectsSphere(spdSphere(Vec4V(CSEGrassStatic::sm_ScrVehAABB.GetCenter(), ScalarV(CSEGrassStatic::sm_ScrVehRadius)))));
		const u32 maxShaderVehicles = hasScriptVehicle ? CCustomShaderEffectGrassType::NUM_COL_VEH - 1 : CCustomShaderEffectGrassType::NUM_COL_VEH;
		for(u32 v=0; v<maxShaderVehicles; v++)
		{
			CVehicle *pVehicle = pVehicles[v];
			if( pVehicle )
			{
				// grab unmodified (e.g. by opened cardoors, etc.) vehicle's bbox:
				const Vector3 bboxmin = pVehicle->GetBaseModelInfo()->GetBoundingBoxMin(); //pVehicle->GetBoundingBoxMin();
				const Vector3 bboxmax = pVehicle->GetBaseModelInfo()->GetBoundingBoxMax(); //pVehicle->GetBoundingBoxMax();

				// two points: on the front and back of the car:
				float middleX = bboxmin.x + (bboxmax.x - bboxmin.x)*0.5f;
				float middleZ = bboxmin.z + (bboxmax.z - bboxmin.z)*0.5f;

				static dev_float radiusPerc = 1.05f;	// how much make bounding shape "shorter" to fit better around vehicle

				const u32 vehNameHash = pVehicle->GetBaseModelInfo()->GetModelNameHash();

				// front/rear Y shift overrides:
				float frontShiftY = 0.0f;
				float rearShiftY  = 0.0f;
				float sideShift	  = 0.0f;

				// BS#1777745: bodhi2: magic shift grass collision is not producing "halo" at the rear of vehicle:
				if(vehNameHash == MID_BODHI2)
				{
					rearShiftY = -0.7f;
				}

				// BS#1918403: Bulldozer flattens grass without being touched when bucket is up;
				if(vehNameHash == MID_BULLDOZER)
				{
					rearShiftY = -0.8f;

					// loop through gadgets until we found the digger arm
					for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
					{
						CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);
						if(pVehicleGadget->GetType() == VGT_DIGGER_ARM)
						{
							CVehicleGadgetDiggerArm *pDiggerArm = static_cast<CVehicleGadgetDiggerArm*>(pVehicleGadget);
							Assert(pDiggerArm);

							if(pDiggerArm)
							{
								const float fArmPosRatio = pDiggerArm->GetJointPositionRatio();

								static dev_float minPosRatio = 0.00f;
								static dev_float maxPosRatio = 0.36f;
								float fArmLerpRatio = Clamp((fArmPosRatio-minPosRatio)/(maxPosRatio-minPosRatio), 0.0f, 1.0f);
								frontShiftY = Lerp(fArmLerpRatio, -0.25f, 1.9f);
							}
						}
					}
				}// if(vehNameHash == MID_BULLDOZER)...


				// BS#1992186: granger side adjustments;
				if(vehNameHash == MID_GRANGER)
				{
					sideShift	= 0.42f;
				}
				else if(vehNameHash == MID_GRANGER2)
				{
					sideShift	= 0.50f;
				}
				// BS#1992186: riot side adjustments;
				else if(vehNameHash == MID_RIOT)
				{
					sideShift	= 0.40f;
				}
				// BS#1925880: zentorno side adjustments;
				else if(vehNameHash == MID_ZENTORNO)
				{
					sideShift	= 0.40f;
				}
				// BS#2278312: rhino front/rear adjustments:
				else if(vehNameHash == MID_RHINO)
				{
					frontShiftY = 2.6f;
					rearShiftY	= -0.3f;
				}
				// BS#2333618: T20 side adjustments:
				else if(vehNameHash == MID_T20)
				{
					sideShift	= 0.45f;
				}
				// BS#3120202: Large Area around Phantom2 is cleared when driving off-road:
				else if(vehNameHash == MID_PHANTOM2)
				{
					frontShiftY = 3.0f;
					sideShift	= 0.75f;
				}
				// BS#3334666 - GP1 - Large area cleared around vehicle
				else if(vehNameHash == MID_INFERNUS2)
				{
					sideShift	= 0.40f;
				}
				// BS#3334666 - GP1 - Large area cleared around vehicle
				else if(vehNameHash == MID_GP1)
				{
					sideShift	= 0.80f;
				}
				// BS#3477152 - XA-21 - Large area cleared around the vehicle
				else if(vehNameHash == MID_XA21)
				{
					sideShift	= 0.70f;
				}
				// BS#3888877 - Visione - Excessive collision of vegetation
				else if(vehNameHash == MID_VISIONE)
				{
					sideShift	= 0.50f;
				}
				// BS#4028275 - Riot2 - Excess grass & vegeatation clearance
				else if(vehNameHash == MID_RIOT2)
				{
					sideShift	= 0.6f;
					rearShiftY	= -0.4f;
				}
				// BS#4068209 - Barrage - Too much foliage cleared behind the vehicle
				else if(vehNameHash == MID_BARRAGE)
				{
					frontShiftY	= 0.4f;
					rearShiftY	= -1.0f;
					sideShift	= 0.3f;
				}
				// BS#4176577 - Chernobog - Area for flattening grass is too big
				else if(vehNameHash == MID_CHERNOBOG)
				{
					frontShiftY	= 1.0f;
					rearShiftY	= -0.5f;
					sideShift	= 0.6f;
				}
				// BS#4194918 - Khanjali - Grass flattening area is larger than vehicle
				else if(vehNameHash == MID_KHANJALI)
				{
					frontShiftY	= 1.0f;
					rearShiftY	= -0.4f;
					sideShift	= 0.3f;
				}
				// BS#198885 - Autarch - Grass collision bounds:
				else if(vehNameHash == MID_AUTARCH)
				{
					sideShift	= 0.5f;
					rearShiftY	= -0.2f;
				}
				// BS#4623613 - Excessive clearing of foliage around Mule4:
				else if(vehNameHash == MID_MULE4)
				{
					frontShiftY	= 0.5f;
					rearShiftY	= -0.4f;
					sideShift	= 0.85f;
				}
				// BS#4623697 - Pounder2 - Excessive clearing of foliage;
				else if(vehNameHash == MID_POUNDER2)
				{
					frontShiftY	= 0.5f;
					sideShift	= 0.8f;
				}
				// BS#4702207 - pbus2 has too large area that flattens grass
				else if(vehNameHash == MID_PBUS2)
				{
					frontShiftY	= 1.0f;
					rearShiftY	= -3.1f;
					sideShift	= 1.3f;
				}
				// BS#5459805 - Monster3 - Foliage grass flatten area is very large on this vehicle.
				else if(vehNameHash == MID_MONSTER3 || vehNameHash == MID_MONSTER4 || vehNameHash == MID_MONSTER5)
				{
					frontShiftY = 3.2f;
					rearShiftY	= -0.5f;
					sideShift	= 0.5f;
				}
				// BS#5388815 - Bruiser - Vehicle has a large foliage flatten area
				else if(vehNameHash == MID_BRUISER || vehNameHash == MID_BRUISER2 || vehNameHash == MID_BRUISER3)
				{
					frontShiftY	= 1.2f;
					rearShiftY	= -1.0f;
					sideShift	= 0.7f;
				}
				// BS#5354204 - Scarab - Excessive foliage suppression
				else if(vehNameHash == MID_SCARAB || vehNameHash == MID_SCARAB2 || vehNameHash == MID_SCARAB3)
				{
					frontShiftY	= 1.4f;
					rearShiftY	= -0.3f;
					sideShift	= 0.4f;
				}
				// BS#5350080 - Deveste - Vegetation cleared quite far
				else if(vehNameHash == MID_DEVESTE)
				{
					frontShiftY = -0.05f;
					rearShiftY	= 0.0f;
					sideShift	= 0.4f;
				}
				// BS#5415156 - ZR380 - Area that flattens grass is too big
				else if(vehNameHash == MID_ZR380 || vehNameHash == MID_ZR3802 || vehNameHash == MID_ZR3803)
				{
					frontShiftY	= 0.6f;
					rearShiftY	= -0.2f;
					sideShift	= 0.35f;
				}
				// BS#5423756 - Cerberus - Vehicle has a large foliage flatten area
				else if (vehNameHash == MID_CERBERUS || vehNameHash == MID_CERBERUS2 || vehNameHash == MID_CERBERUS3)
				{
					frontShiftY	= 1.6f;
					rearShiftY	= -1.0f;
					sideShift	= 0.6f;
				}
				// BS#5459616 - Slamvan4 -  Foliage suppression too large
				else if(vehNameHash == MID_SLAMVAN4 || vehNameHash == MID_SLAMVAN5 || vehNameHash == MID_SLAMVAN6)
				{
					frontShiftY	= 0.35f;
					rearShiftY	= -0.75f;
					sideShift	= 0.1f;
				}
				// BS#5476478 - Dominator5 - excess foliage collision
				else if(vehNameHash == MID_DOMINATOR3)
				{
					rearShiftY	= -0.2f;
					sideShift	= 0.2f;
				}
				else if(vehNameHash == MID_DOMINATOR4 || vehNameHash == MID_DOMINATOR5 || vehNameHash == MID_DOMINATOR6)
				{
					frontShiftY	= 0.85f;
					rearShiftY	= 0.1f;
					sideShift	= 0.25f;
				}
				// BS#5457801 - Vehicles with Scoop Mods have large foliage flatten around it
				else if(vehNameHash == MID_BRUTUS || vehNameHash == MID_BRUTUS2 || vehNameHash == MID_BRUTUS3)
				{
					frontShiftY	= 1.3f;
					rearShiftY	= -0.5f;
					sideShift	= 0.3f;
				}
				else if(vehNameHash == MID_IMPALER2 || vehNameHash == MID_IMPALER3 || vehNameHash == MID_IMPALER4)
				{
					frontShiftY	= 0.7f;
					rearShiftY	= -0.1f;
					sideShift	= 0.2f;
				}
				else if(vehNameHash == MID_IMPERATOR || vehNameHash == MID_IMPERATOR3)
				{
					frontShiftY	= 0.7f;
					rearShiftY	= -0.35f;
					sideShift	= 0.3f;
				}
				else if(vehNameHash == MID_IMPERATOR2)
				{
					frontShiftY	= 0.4f;
					rearShiftY	= -0.35f;
					sideShift	= 0.3f;
				}
				else if(vehNameHash == MID_ISSI4 || vehNameHash == MID_ISSI5 || vehNameHash == MID_ISSI6)
				{
					frontShiftY	= 0.8f;
					rearShiftY	= -0.1f;
					sideShift	= 0.3f;
				}
				else if(vehNameHash == MID_DEATHBIKE || vehNameHash == MID_DEATHBIKE2 || vehNameHash == MID_DEATHBIKE3)
				{
					sideShift	= 0.4f;
				}
				else if(vehNameHash == MID_EMERUS)
				{
					rearShiftY	= -0.3f;
					sideShift	= 0.15f;
				}
				else if(vehNameHash == MID_NEO)
				{
					sideShift	= 0.3f;
				}
				else if(vehNameHash == MID_KRIEGER)
				{
					rearShiftY	= -0.2f;
					sideShift	= 0.1f;
				}
				else if(vehNameHash == MID_S80)
				{
					frontShiftY	= -0.1f;
					rearShiftY	= -0.2f;
				}
				else if(vehNameHash == MID_IMORGON)
				{
					frontShiftY	= -0.05f;
					rearShiftY	= -0.15f;
					sideShift	= 0.1f;
				}
				else if(vehNameHash == MID_FORMULA)
				{
					frontShiftY	= -0.3f;
					sideShift	= 0.2f;
				}
				else if(vehNameHash == MID_REBLA)
				{
					sideShift	= 0.1f;
				}
				else if(vehNameHash == MID_POLBUFFALO)
				{
					frontShiftY	= -0.08f;
					rearShiftY	= 0.05f;
					sideShift	= 0.05f;
				}
				else if(vehNameHash == MID_YOUGA3)
				{
					frontShiftY	= 0.3f;
					rearShiftY	= 0.1f;
					sideShift	= 0.1f;
				}
				else if(vehNameHash == MID_YOUGA4)
				{
					frontShiftY	= 0.3f;
					rearShiftY	= -0.2f;
					sideShift	= 0.4f;
				}
				else if(vehNameHash == MID_OPENWHEEL1)
				{
					frontShiftY	= -0.05f;
					rearShiftY	= -0.2f;
					sideShift	= 0.2f;
				}
				else if(vehNameHash == MID_VERUS)
				{
					frontShiftY	= 0.15f;
					rearShiftY	= -0.3f;
					sideShift	= 0.1f;
				}
				else if(vehNameHash == MID_IGNUS)
				{
					rearShiftY	= -0.2f;
					sideShift	= 0.2f;
				}
				else if(vehNameHash == MID_DEITY)
				{
					sideShift	= 0.15f;
				}
				else if(vehNameHash == MID_ZENO)
				{
					sideShift	= 0.75f;
				}
				else if(vehNameHash == MID_COMET7)
				{
					sideShift	= 0.15f;
				}
				else if(vehNameHash == MID_BALLER7)
				{
					sideShift	= 0.15f;
				}
				else if(vehNameHash == MID_PATRIOT3)
				{
					sideShift	= 0.3f;
				}
				else if(vehNameHash == MID_TORERO2)
				{
					rearShiftY	= -0.1f;
					sideShift	= 0.4f;
				}
				else if(vehNameHash == MID_CORSITA)
				{
					frontShiftY	= 0.1f;
					rearShiftY	= -0.2f;
					sideShift	= 1.05f;
				}
				else if(vehNameHash == MID_KANJOSJ)
				{
					frontShiftY	= 0.2f;
					sideShift	= 0.2f;
				}
				else if(vehNameHash == MID_WEEVIL2)
				{
					frontShiftY	= 0.3f;
					rearShiftY	= -0.5f;
					sideShift	= 0.4f;
				}

			static dev_bool bOverrideFrontShiftY=false;
				if(bOverrideFrontShiftY)
				{
					static dev_float frontShiftYOverride = 1.9f;
					frontShiftY = frontShiftYOverride;
				}

			static dev_bool bOverrideRearShiftY=false;
				if(bOverrideRearShiftY)
				{
					static dev_float rearShiftYOverride = -0.7f;
					rearShiftY = rearShiftYOverride;
				}

			static dev_bool bOverrideSideShift=false;
				if(bOverrideSideShift)
				{
					static dev_float sideShiftOverride = 0.2f;
					sideShift = sideShiftOverride;
				}

				float   Radius = (bboxmax.x - bboxmin.x)*0.5f - sideShift;	// radius
				Vector3 ptb(middleX, bboxmax.y - Radius*radiusPerc - frontShiftY, middleZ);
				Vector3 pta(middleX, bboxmin.y + Radius*radiusPerc - rearShiftY, middleZ);


				// first point on segment:
				Vector3 ptB = pVehicle->TransformIntoWorldSpace(ptb);
				// last point on segment:
				Vector3 ptA = pVehicle->TransformIntoWorldSpace(pta);
				// segment vector:
				Vector3 vecM = ptA - ptB;

				// detect groundZ pos (from wheel hit pos):	
				Vector3 wheelHitPos(0,0,100000);
				const s32 numWheels = pVehicle->GetNumWheels();
				for(s32 i=0; i<numWheels; i++)
				{
					if(pVehicle->GetWheel(i))
					{
						// select wheelHitPos with lowest Z (to allow for steep slopes - see BS#978552, BS#1725071):
						Vector3 wheelPos = pVehicle->GetWheel(i)->GetHitPos();
						if(wheelPos.z < wheelHitPos.z)
						{
							wheelHitPos = wheelPos;
						}
					}
				}

				static dev_bool bOverrideZ = FALSE;
				static float overrideZ = 398.44f; // test for testbed
				float groundZ = bOverrideZ? overrideZ : wheelHitPos.z;

				static dev_float tweakRadiusScale = 1.0f;		// v1 collision
				SetGlobalVehCollisionParams(v, TRUE, ptB, vecM, Radius*tweakRadiusScale, groundZ, VEHCOL_DIST_LIMIT);
			}
			else
			{
				SetGlobalVehCollisionParams(v, FALSE, Vector3(0,0,0), Vector3(0,0,0), 0, 0, 0);
			}
		}//	for(u32 v=0; v<CCustomShaderEffectGrassType::NUM_COL_VEH; v++)...

		if(hasScriptVehicle)
		{
			static dev_float scrVehColDistLimit = 25.0f;	// set limit to 25m for script controlled collision
			SetGlobalVehCollisionParams(CCustomShaderEffectGrassType::NUM_COL_VEH - 1, true, VEC3V_TO_VECTOR3(CSEGrassStatic::sm_ScrVehB), VEC3V_TO_VECTOR3(CSEGrassStatic::sm_ScrVehM), CSEGrassStatic::sm_ScrVehRadius, CSEGrassStatic::sm_ScrVehGroundZ, scrVehColDistLimit);
		}
	}//m_pType->IsVehicleDeformable()...

}// end of Update()...

//
//
//
//
void CCustomShaderEffectGrass::SetGlobalVehCollisionParams(u32 n, bool bEnable, const Vector3& vecB, const Vector3& vecM, float radius, float groundZ, float vehColDistLimit)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	Assert(n<CCustomShaderEffectGrassType::NUM_COL_VEH);

	m_bVehCollisionEnabled[n] = bEnable;

	Vector4 &vehCollisionB = m_vecVehCollisionB[n]; 
	vehCollisionB.x = vecB.x;
	vehCollisionB.y = vecB.y;
	vehCollisionB.z = vecB.z;
	vehCollisionB.w = vehColDistLimit;

	Vector2 vecM2;
	vecM2.x = vecM.x;
	vecM2.y = vecM.y;
	float dotMM = vecM2.Dot(vecM2);

	Vector4 &vehCollisionM = m_vecVehCollisionM[n];
	vehCollisionM.x = vecM.x;
	vehCollisionM.y = vecM.y;
	vehCollisionM.z = vecM.z;
	vehCollisionM.w = (dotMM>0.0f)? (1.0f/dotMM) : (0.0f);

	Vector4 &vehCollisionR = m_vecVehCollisionR[n];
	vehCollisionR.x = radius;
	vehCollisionR.y = radius * radius;
	vehCollisionR.z = (radius>0.0f)? (1.0f/(radius * radius)) : (0.0f);
	vehCollisionR.w = groundZ;
}

//
//
//
//
void CCustomShaderEffectGrass::SetShaderVariables(rmcDrawable* pDrawable)
{
	if(!pDrawable)
		return;

	if(grmModel::GetForceShader())
		return;

	grmShaderGroup *shaderGroup = &pDrawable->GetShaderGroup();

	if(m_pType->m_CSVars.m_idAabbMin)
	{
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_CSVars.m_idAabbMin), m_AabbMin);
	}

	if(m_pType->m_CSVars.m_idAabbDelta)
	{
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_CSVars.m_idAabbDelta), m_AabbDelta);
	}

	if(m_pType->m_idOrientToTerrain)
	{
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idOrientToTerrain), m_orientToTerrain);
	}

	if(m_pType->m_idPlayerPos)
	{
		Vec4V playerPos(VECTOR3_TO_VEC3V(CGrassRenderer::GetGlobalPlayerPos()), LoadScalar32IntoScalarV(m_orientToTerrain));
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idPlayerPos), playerPos);
	}

	if(m_pType->m_CSVars.m_idScaleRange)
	{
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_CSVars.m_idScaleRange), m_scaleRange);
	}

	if(m_pType->m_idWindBending)
	{
		Vector2 globalWindBending;
		CGrassRenderer::GetGlobalWindBending(globalWindBending);
		Vec4V windBending((CSEGrassBankStatic::sm_bEnableWinBending ? globalWindBending.x : 0.0f), (CSEGrassBankStatic::sm_bEnableWinBending ? globalWindBending.y : 0.0f), 0.0f, 0.0f);
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idWindBending), windBending);
	}

	if(m_pType->m_idLodFadeInstRange)
	{
		Vec2V range = m_LodFadeInstRange.GetXY();
		//NOTE: NG fades from the back rather than the front... so need to compute the scale differently...
	#if __D3D11 || RSG_ORBIS
		ScalarV numInst = m_LodFadeInstRange.GetZ();
		range = Vec2V(numInst) - range;
		Vec3V shaderFadeInstRange(range, SelectFT(IsGreaterThan(range.GetX(), range.GetY()), ScalarV(V_NEGONE), ScalarV(V_ONE) / (range.GetY() - range.GetX())));
	#else
		Vec3V shaderFadeInstRange(range, SelectFT(IsLessThan(range.GetX(), range.GetY()), ScalarV(V_ONE), ScalarV(V_ONE) / (range.GetY() - range.GetX())));
	#endif
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idLodFadeInstRange), shaderFadeInstRange);
	}

#if ENABLE_TRAIL_MAP
	if(m_pType->m_idTrailMapTexture)
	{
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idTrailMapTexture), g_TrailMapRenderer.GetTrailMap()); // The map
	}

	if(m_pType->m_idMapSizeWorldSpace)
	{
		Vec4V mapSizeWorldSpace(g_TrailMapRenderer.GetTrailMapWorldSpaceWidth(), g_TrailMapRenderer.GetTrailMapWorldSpaceWidth(), 0.0f, 0.0f);
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idMapSizeWorldSpace), mapSizeWorldSpace);
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Micromovement Params

	if(m_pType->m_idFadeAlphaDistUmTimer)
	{
		//Don't care much about lod alpha fade at the moment, so turn it off.
		float alphaFadeLODDist0 = 0.0f; //In grass code, was: (f) / (f - c)
		float alphaFadeLODDist1 = 1.0f; //In grass code, was: (-1.0) / (f - c)
		float umTimer = CSEGrassBankStatic::sm_bEnableMicroMovements ? static_cast<float>(2.0 * PI * (static_cast<double>(fwTimer::GetTimeInMilliseconds()) / 1000.0)) : 0.0f;
		shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idFadeAlphaDistUmTimer), Vec4V(alphaFadeLODDist0, alphaFadeLODDist1, umTimer, 0.0f));
	}

#if DEVICE_MSAA
	if (m_pType->m_idAlphaToCoverageScale && !GRCDEVICE.GetMSAA())
	{
		shaderGroup->SetVar( static_cast<grmShaderGroupVar>(m_pType->m_idAlphaToCoverageScale), 1.0f );
	}
	//else: leave artist-tuned value
#endif

	// player collision:
	if(m_pType->m_idCollParams)
	{
		const Vector2 vecPlayerColl(m_collPlayerScaleSqr, 1.0f/m_collPlayerScaleSqr);
		shaderGroup->SetVar((grmShaderGroupVar)(m_pType->m_idCollParams), vecPlayerColl);
	}

	// vehicle collision:
	if(m_pType->IsVehicleDeformable())
	{
		for(u32 i=0; i<CCustomShaderEffectGrassType::NUM_COL_VEH; i++)
		{
			if(m_pType->m_idVarVehCollEnabled[i])
			{
				grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarVehCollEnabled[i], m_bVehCollisionEnabled[i]);
			}

			if(m_bVehCollisionEnabled[i])
			{
				if(m_pType->m_idVarVehCollB0[i])
				{
					shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarVehCollB0[i], m_vecVehCollisionB[i]);
				}

				if(m_pType->m_idVarVehCollM0[i])
				{
					shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarVehCollM0[i], m_vecVehCollisionM[i]);
				}

				if(m_pType->m_idVarVehCollR0[i])
				{
					shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idVarVehCollR0[i], m_vecVehCollisionR[i]);
				}
			}
		}
	}
	else
	{
		for(u32 i=0; i<CCustomShaderEffectGrassType::NUM_COL_VEH; i++)
		{
			if(m_pType->m_idVarVehCollEnabled[i])
			{
				grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarVehCollEnabled[i], false);
			}
		}
	}


#if __BANK
	if(CSEGrassBankStatic::sm_bOverrideAO)
	{
		if(m_pType->m_idFakedGrassNormal)
		{
			Vec4V fakedNormal(0.0f, 0.0f, 0.0f, CSEGrassBankStatic::sm_fGlobalA0);
			shaderGroup->SetVar(static_cast<grmShaderGroupVar>(m_pType->m_idFakedGrassNormal), fakedNormal);
		}
	}

	if(m_pType->m_idDebugSwitches)
	{
		Vec4V debugSwitches(V_ZERO);
		debugSwitches.SetX(CSEGrassBankStatic::sm_bAlphaOverdraw? 1.0f : 0.0f);
		shaderGroup->SetVar((grmShaderGroupVar)m_pType->m_idDebugSwitches, debugSwitches);
	}
#endif //__BANK...
}// end of SetShaderVariables()...

void CCustomShaderEffectGrass::ClearScriptVehicleFlatten()
{
	CSEGrassStatic::ClearScriptVehicle();
}

void CCustomShaderEffectGrass::SetScriptVehicleFlatten(const spdAABB &aabb, Vec3V_In look, ScalarV_In groundZ)
{
	CSEGrassStatic::SetScriptVehicle(aabb, look, groundZ);
}

void CCustomShaderEffectGrass::SetScriptVehicleFlatten(const spdSphere &sphere, Vec3V_In look, ScalarV_In groundZ)
{
	CSEGrassStatic::SetScriptVehicle(sphere, look, groundZ);
}

bool CCustomShaderEffectGrass::IntersectsScriptFlattenAABB(fwEntity *pEntity)
{
	return CSEGrassStatic::IntersectsScriptVehicleAABB(pEntity);
}

#if __BANK
void CCustomShaderEffectGrass::SetDbgAlphaOverdraw(bool enable)
{
	CSEGrassBankStatic::sm_bAlphaOverdraw = enable;
}

void CCustomShaderEffectGrass::RenderDebug()
{
	CSEGrassBankStatic::RenderDebug();
}

void CCustomShaderEffectGrass::AddWidgets(bkBank &bank)
{
	CSEGrassBankStatic::AddWidgets(bank);
}
#endif //__BANK
