///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	FogVolume.cpp
//	BY	: 	Anoop Ravi Thomas
//	FOR	:	Rockstar Games
//	ON	:	25 October 2013
//	WHAT:	Fog Volumes
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "FogVolume.h"

// rage headers
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"
#include "spatialdata/transposedplaneset.h"
#include "rmptfx/ptxmanager.h"

// framework headers
#include "fwsys/gameskeleton.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// game headers
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/Lights/lights.h"
#include "renderer/occlusion.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "renderer/Util/ShaderUtil.h"
#include "timecycle/TimeCycle.h"
#include "vfx/metadata/VfxFogVolumeInfo.h"
#include "vfx/VfxHelper.h"

#if RSG_PC || RSG_DURANGO
#include "renderer/rendertargets.h"
#endif

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define FOGVOLUME_SHADERVAR_COLOR_STR							"fogVolumeColor"
#define FOGVOLUME_SHADERVAR_POSITION_STR						"fogVolumePosition"
#define FOGVOLUME_SHADERVAR_PARAMS_STR							"fogVolumeParams"
#define FOGVOLUME_SHADERVAR_TRANSFORM_STR						"fogVolumeTransform"
#define FOGVOLUME_SHADERVAR_INV_TRANSFORM_STR					"fogVolumeInvTransform"
#define FOGVOLUME_SHADERVAR_SCREENSIZE_STR						"deferredLightScreenSize"
#define FOGVOLUME_SHADERVAR_PROJECTION_PARAMS_STR				"deferredProjectionParams"
#define FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS0_STR		"deferredPerspectiveShearParams0"
#define FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS1_STR		"deferredPerspectiveShearParams1"
#define FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS2_STR		"deferredPerspectiveShearParams2"
#define FOGVOLUME_SHADERVAR_DEPTHBUFFER_STR						"fogVolumeDepthBuffer"

//fogVolumeDensity		(fogVolumeParams.x)
//fogVolumeExponent		(fogVolumeParams.y)
//fogVolumeRange		(fogVolumeParams.z)
//fogVolumeInvRange		(fogVolumeParams.w)

#define FOGVOLUME_UPDATE_BUFFER_ID (0)
#define FOGVOLUME_RENDER_BUFFER_ID (1)

#define FOGVOLUME_SHADERTECHNIQUE_DEFAULT_STR		"fogvolume_default"
#define FOGVOLUME_SHADERTECHNIQUE_ELLIPSOIDAL_STR	"fogvolume_ellipsoidal"

#define FOGVOLUME_DEFAULT_EXTEND_RADIUS_PERCENT	(2.0f)
#define FOGVOLUME_DEFAULT_DENSITY_SCALE (-2.5f)

#define FOGVOLUME_DEFAULT_LOW_LOD_DIST_FACTOR (6.0f)
#define FOGVOLUME_DEFAULT_MED_LOD_DIST_FACTOR (4.0f)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVolumeVertexBuffer g_FogVolumeVertexBuffers[FOGVOLUME_VB_TOTAL];
CFogVolumeMgr g_fogVolumeMgr;

FogVolume_t* CFogVolumeMgr::m_FogVolumeRenderBuffer = NULL;

// static variables
const char* g_fogVolumeShaderVarName[FOGVOLUME_SHADERVAR_COUNT] = 
{ 	
	FOGVOLUME_SHADERVAR_COLOR_STR,
	FOGVOLUME_SHADERVAR_POSITION_STR,	
	FOGVOLUME_SHADERVAR_PARAMS_STR, 
	FOGVOLUME_SHADERVAR_TRANSFORM_STR,
	FOGVOLUME_SHADERVAR_INV_TRANSFORM_STR,
	FOGVOLUME_SHADERVAR_SCREENSIZE_STR,
	FOGVOLUME_SHADERVAR_PROJECTION_PARAMS_STR,
	FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS0_STR,
	FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS1_STR,
	FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS2_STR,
	FOGVOLUME_SHADERVAR_DEPTHBUFFER_STR
};

const char* g_fogVolumeShaderTechniqueName[FOGVOLUME_SHADERTECHNIQUE_COUNT] = 
{	
	FOGVOLUME_SHADERTECHNIQUE_DEFAULT_STR,
	FOGVOLUME_SHADERTECHNIQUE_ELLIPSOIDAL_STR
};


grcDepthStencilStateHandle	ms_dsFogVolumeOutsideRender			= grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle	ms_dsFogVolumeInsideRender			= grcStateBlock::DSS_Invalid;
grcRasterizerStateHandle ms_rsFogVolumeOutsideRender			= grcStateBlock::RS_Invalid;
grcRasterizerStateHandle ms_rsFogVolumeInsideRender				= grcStateBlock::RS_Invalid;

sysCriticalSectionToken g_RegisterFogVolumeCS;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////


// =============================================================================================== //
// HELPER CLASSES
// =============================================================================================== //

class dlCmdSetupFogVolumesFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_SetupFogVolumesFrameInfo,
	};

	dlCmdSetupFogVolumesFrameInfo();
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdSetupFogVolumesFrameInfo &) cmd).Execute(); }
	void Execute();

private:			

	FogVolume_t* newFrameInfo;
	u32 numFogVolumes;
};

class dlCmdClearFogVolumesFrameInfo : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_ClearFogVolumesFrameInfo,
	};

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdClearFogVolumesFrameInfo &) cmd).Execute(); }
	void Execute();
};

dlCmdSetupFogVolumesFrameInfo::dlCmdSetupFogVolumesFrameInfo()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "dlCmdSetupFogVolumesFrameInfo::dlCmdSetupFogVolumesFrameInfo - not called from the update thread");
	numFogVolumes = g_fogVolumeMgr.GetNumFogVolumesRegisteredUpdate();
	if(numFogVolumes > 0)
	{
		const int sizeOfBuffer = sizeof(FogVolume_t) * numFogVolumes;
		newFrameInfo = gDCBuffer->AllocateObjectFromPagedMemory<FogVolume_t>(DPT_LIFETIME_RENDER_THREAD, sizeOfBuffer);
		sysMemCpy(newFrameInfo, g_fogVolumeMgr.GetFogVolumesUpdateBuffer(), sizeOfBuffer);
	}
	else
	{
		newFrameInfo = NULL;
	}
}

void dlCmdSetupFogVolumesFrameInfo::Execute()
{
	vfxAssertf(g_fogVolumeMgr.m_FogVolumeRenderBuffer == NULL, "m_FogVolumeRenderBuffer is not NULL, means that dlCmdClearFogVolumesFrameInfo was not executed before this");
	g_fogVolumeMgr.m_FogVolumeRenderBuffer = newFrameInfo;
	g_fogVolumeMgr.SetNumFogVolumesRegisteredRender(numFogVolumes);
}


void dlCmdClearFogVolumesFrameInfo::Execute()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "dlCmdClearFogVolumesFrameInfo::dlCmdClearFogVolumesFrameInfo - not called from the render thread");
	vfxAssertf( (g_fogVolumeMgr.GetNumFogVolumesRegisteredRender() == 0 && g_fogVolumeMgr.m_FogVolumeRenderBuffer == NULL) || (g_fogVolumeMgr.GetNumFogVolumesRegisteredRender() > 0 && g_fogVolumeMgr.m_FogVolumeRenderBuffer != NULL), 
		"m_FogVolumeRenderBuffer was set incorrectly. Num Fog Volumes = %d, m_FogVolumeRenderBuffer = %p", g_fogVolumeMgr.GetNumFogVolumesRegisteredRender(), g_fogVolumeMgr.m_FogVolumeRenderBuffer );
	g_fogVolumeMgr.m_FogVolumeRenderBuffer = NULL;
	g_fogVolumeMgr.SetNumFogVolumesRegisteredRender(0);
}

///////////////////////////////////////////////////////////////////////////////
//  CLASS CFogVolumeMgr
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  Constructor
///////////////////////////////////////////////////////////////////////////////

CFogVolumeMgr::CFogVolumeMgr()
	: m_fogVolumeShader(NULL)
{
	for(int i=0; i<FOGVOLUME_SHADERTECHNIQUE_COUNT; i++)
	{
		m_fogVolumeShaderTechniques[i] = grcetNONE;
	}
	for(int i=0; i<FOGVOLUME_SHADERVAR_COUNT; i++)
	{
		m_fogVolumeShaderVars[i] = grcevNONE;
	}

	// initialise the counts
	for (s32 i=0; i<FOGVOLUME_MAX_STORED_NUM_BUFFERS; i++)
	{
		m_numRegisteredFogVolumes[i] = 0;
	}

	m_DisableFogVolumeRender = false;

#if __BANK
	m_TestDistanceFromCam = 20.0f;
	m_TestPosition = Vec3V(V_ZERO);
	m_TestScale = Vec3V(V_ONE);
	m_TestRotation = Vec3V(V_ZERO);
	m_TestColor = Color_white;
	m_TestDensity = 1.0f;
	m_TestRange = 10.0f;
	m_TestFallOff = 4.0f;
	m_TestHDRMult = 1.0f;
	m_TestLightingType = FOGVOLUME_LIGHTING_FOGHDR;
	m_TestInteriorHash = 0;
	m_TestUseGroundFogColour = false;
	m_TestRender = false;
	m_TestFadeStart = 20.0f;
	m_TestFadeEnd = 50.0f;
	m_TestApplyFade = false;
	m_TestGenerateInFrontOfCamera = false;

	m_DebugExtendedRadiusPercent = FOGVOLUME_DEFAULT_EXTEND_RADIUS_PERCENT;
	m_DebugFogVolumeRender = false;
	m_DebugDensityScale = FOGVOLUME_DEFAULT_DENSITY_SCALE;
	m_DebugFogVolumeLowLODDistFactor = FOGVOLUME_DEFAULT_LOW_LOD_DIST_FACTOR;
	m_DebugFogVolumeMedLODDistFactor = FOGVOLUME_DEFAULT_MED_LOD_DIST_FACTOR;
	m_ForceDetailMeshIndex = -1;
	m_DisableOcclusionTest = false;
#endif

}


///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CFogVolumeMgr::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		InitShaders();
		//setup render states

		grcRasterizerStateDesc fogVolumeRasterizerState;
		fogVolumeRasterizerState.CullMode = grcRSV::CULL_BACK;
		ms_rsFogVolumeOutsideRender = grcStateBlock::CreateRasterizerState(fogVolumeRasterizerState);

		fogVolumeRasterizerState.CullMode = grcRSV::CULL_FRONT;
		ms_rsFogVolumeInsideRender = grcStateBlock::CreateRasterizerState(fogVolumeRasterizerState);

		grcDepthStencilStateDesc ds;

		ds.DepthEnable					= true;
		ds.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		ds.DepthWriteMask				= false;
		ds.StencilEnable                = false;

		ms_dsFogVolumeOutsideRender = grcStateBlock::CreateDepthStencilState(ds);

		ds.DepthEnable					= false;
		ms_dsFogVolumeInsideRender = grcStateBlock::CreateDepthStencilState(ds);
		//Low Detail Mesh
		{
			CVolumeVertexBuffer* vb = &g_FogVolumeVertexBuffers[FOGVOLUME_VB_LOWDETAIL];
			const int steps = 10;
			//const int steps = 2;
			const int stacks = 0;

			vb->GeneratePointSpotVolumeVB(steps, stacks);
		}

		//Mid Detail Mesh
		{
			CVolumeVertexBuffer* vb = &g_FogVolumeVertexBuffers[FOGVOLUME_VB_MIDDETAIL];
			const int steps = 15;
			//const int steps = 2;
			const int stacks = 0;

			vb->GeneratePointSpotVolumeVB(steps, stacks);
		}

		//High Detail Mesh
		{
			CVolumeVertexBuffer* vb = &g_FogVolumeVertexBuffers[FOGVOLUME_VB_HIDETAIL];
			const int steps = 20;
			//const int steps = 2;
			const int stacks = 0;

			vb->GeneratePointSpotVolumeVB(steps, stacks);
		}
	}
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		// initialise the counts
		for (s32 i=0; i<FOGVOLUME_MAX_STORED_NUM_BUFFERS; i++)
		{
			m_numRegisteredFogVolumes[i] = 0;
		}
	}

}


///////////////////////////////////////////////////////////////////////////////
//  InitDLCCommands
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdSetupFogVolumesFrameInfo);
	DLC_REGISTER_EXTERNAL(dlCmdClearFogVolumesFrameInfo);
}

///////////////////////////////////////////////////////////////////////////////
//  InitShaders
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::InitShaders()
{
	//Pushing the folder for loading MSAA shader
	ASSET.PushFolder("common:/shaders");
	// setup shader
	m_fogVolumeShader = grmShaderFactory::GetInstance().Create();
	const char* pName = MSAA_ONLY(GRCDEVICE.GetMSAA() > 1 ? "vfx_fogvolumeMS": ) "vfx_fogvolume";
	m_fogVolumeShader->Load(pName);
	
	ASSET.PopFolder();
	for(int i=0; i<FOGVOLUME_SHADERTECHNIQUE_COUNT; i++)
	{
		m_fogVolumeShaderTechniques[i] = m_fogVolumeShader->LookupTechnique(g_fogVolumeShaderTechniqueName[i]);

	}

	for(int i=0; i<FOGVOLUME_SHADERVAR_COUNT; i++)
	{
		m_fogVolumeShaderVars[i] = m_fogVolumeShader->LookupVar(g_fogVolumeShaderVarName[i]);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  DeleteShaders
///////////////////////////////////////////////////////////////////////////////
#if RSG_PC
void CFogVolumeMgr::DeleteShaders()
{
	delete m_fogVolumeShader; m_fogVolumeShader = NULL;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  ResetShaders
///////////////////////////////////////////////////////////////////////////////
#if RSG_PC
void CFogVolumeMgr::ResetShaders()
{
	g_fogVolumeMgr.DeleteShaders();
	g_fogVolumeMgr.InitShaders();
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::Shutdown(unsigned /*shutdownMode*/)
{

}

///////////////////////////////////////////////////////////////////////////////
//  SetupRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CFogVolumeMgr::SetupRenderthreadFrameInfo()
{
	DLC(dlCmdSetupFogVolumesFrameInfo, ());
}

///////////////////////////////////////////////////////////////////////////////
//  ClearRenderthreadFrameInfo
///////////////////////////////////////////////////////////////////////////////

void CFogVolumeMgr::ClearRenderthreadFrameInfo()
{
	DLC(dlCmdClearFogVolumesFrameInfo, ());
}


///////////////////////////////////////////////////////////////////////////////
//  PreUpdate
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::PreUpdate()
{
	if(!fwTimer::IsGamePaused())
	{
		//Reset the count
		SetNumFogVolumesRegisteredUpdate(0);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateAfterRender
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::UpdateAfterRender()
{
	if (!fwTimer::IsGamePaused())
	{

#if __BANK
		// register the debug corona?
		if (m_TestRender)
		{
			FogVolume_t testFogVolumeParams;
			testFogVolumeParams.range = m_TestRange;
			testFogVolumeParams.col = m_TestColor;
			testFogVolumeParams.scale = m_TestScale;
			testFogVolumeParams.rotation = m_TestRotation * Vec3V(ScalarV(V_TO_RADIANS));
			testFogVolumeParams.fallOff = m_TestFallOff;
			testFogVolumeParams.hdrMult = m_TestHDRMult;
			testFogVolumeParams.density = m_TestDensity * (m_TestApplyFade?CVfxHelper::GetInterpValue(Sqrtf(CVfxHelper::GetDistSqrToCamera(m_TestPosition)), m_TestFadeStart, m_TestFadeEnd):1.0f);
			testFogVolumeParams.pos = m_TestPosition;
			testFogVolumeParams.lightingType = (FogVolumeLighting_e)m_TestLightingType;
			testFogVolumeParams.useGroundFogColour = m_TestUseGroundFogColour;
			testFogVolumeParams.interiorHash = m_TestInteriorHash;

			Register(testFogVolumeParams);
		}

#endif // __BANK

	}
}

///////////////////////////////////////////////////////////////////////////////
//  Register
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::Register(const FogVolume_t& fogVolumeParams)
{
	SYS_CS_SYNC(g_RegisterFogVolumeCS);

	// check that we're in the update thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) || RMPTFXMGR.IsParticleUpdateThreadOrTask(), 
		"CFogVolumeMgr::Register - not called from the update/particle thread/particle task");

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
	spdTransposedPlaneSet8 cullPlanes;
	cullPlanes.Set(*grcVP, true, true);

	//Register only if the fogVolume is visible
	ScalarV vFogVolumeRadius = ScalarV(fogVolumeParams.range);
	spdSphere boundSphere(fogVolumeParams.pos, vFogVolumeRadius);
	if(cullPlanes.IntersectsSphere(boundSphere))
	{
		//do a quick occlusion test also
		Vec3V bbMin = fogVolumeParams.pos - (Vec3V(ScalarVFromF32(fogVolumeParams.range)) * fogVolumeParams.scale);
		Vec3V bbMax = fogVolumeParams.pos + (Vec3V(ScalarVFromF32(fogVolumeParams.range)) * fogVolumeParams.scale);

		spdAABB cullBox(bbMin, bbMax);
		bool passOcclusionTest = COcclusion::IsBoxVisibleFast(cullBox, false) BANK_ONLY(|| m_DisableOcclusionTest);

		if(passOcclusionTest)
		{
			u32& numRegisteredFogVolumes = m_numRegisteredFogVolumes[FOGVOLUME_UPDATE_BUFFER_ID];
			if(numRegisteredFogVolumes < FOGVOLUME_MAX_STORED)
			{
				m_FogVolumeRegister[numRegisteredFogVolumes] = fogVolumeParams;
				numRegisteredFogVolumes++;
			}
			else
			{
				vfxAssertf(numRegisteredFogVolumes < FOGVOLUME_MAX_STORED, "Max Number of Fog Volumes Registered for frame. Contact Anoop if there's more required");
			}
		}

	}
}


///////////////////////////////////////////////////////////////////////////////
//  RenderImmediate
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::RenderRegistered()
{
	if(m_DisableFogVolumeRender)
	{
		return;
	}

	// check that we're in the render thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CFogVolumeMgr::Register - not called from the render thread");

	GRC_VIEWPORT_AUTO_PUSH_POP();
	grcViewport::SetCurrentWorldIdentity();
	u32 numRegisteredFogVolumes = m_numRegisteredFogVolumes[FOGVOLUME_RENDER_BUFFER_ID];

	DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthResolved());

	CVolumeVertexBuffer::BeginVolumeRender();

	
	for(int i=0; i<numRegisteredFogVolumes; i++)
	{			
		RenderFogVolume(m_FogVolumeRenderBuffer[i] BANK_PARAM(false));
	}

	CVolumeVertexBuffer::EndVolumeRender();
}
///////////////////////////////////////////////////////////////////////////////
//  RenderImmediate
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::RenderImmediate(const FogVolume_t& fogVolumeParams)
{
	// check that we're in the render thread
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CFogVolumeMgr::Register - not called from the render thread");

	if(m_DisableFogVolumeRender)
	{
		return;
	}

	//Disabling this for mirror reflections, because we dont support it yet and its probably expensive
	//TODO: Enable this for next project
	//This would require some changes for setting up the depth buffer/projection params etc
	if(DRAWLISTMGR->IsExecutingMirrorReflectionDrawList())
	{
		return;
	}

	const grcViewport* grcVP = gVpMan.GetCurrentGameGrcViewport();
	spdTransposedPlaneSet8 cullPlanes;
	cullPlanes.Set(*grcVP, true, true);

	//Render only if the fogVolume is visible
	ScalarV vFogVolumeRadius = ScalarV(fogVolumeParams.range);
	spdSphere boundSphere(fogVolumeParams.pos, vFogVolumeRadius);
	if(cullPlanes.IntersectsSphere(boundSphere))
	{
		DeferredLighting::SetShaderGBufferTarget(GBUFFER_DEPTH, CRenderTargets::GetDepthResolved());

		CVolumeVertexBuffer::BeginVolumeRender();
		RenderFogVolume(fogVolumeParams BANK_PARAM(true));
		CVolumeVertexBuffer::EndVolumeRender();
	}

}


///////////////////////////////////////////////////////////////////////////////
//  RenderFogVolume
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::RenderFogVolume(const FogVolume_t& fogVolumeParams BANK_PARAM(bool bImmediateMode))
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	//Do interior check and return
	if(!CRenderPhaseDrawSceneInterface::RenderAlphaEntityInteriorHashCheck(fogVolumeParams.interiorHash))
	{
		return;
	}
	const tcKeyframeUncompressed& currRenderKeyframe = g_timeCycle.GetCurrRenderKeyframe();

	FogVolumeShaderPasses_e pass = CalculatePass(fogVolumeParams);
	grcEffectTechnique technique = CalculateTechnique(fogVolumeParams);

	grcBlendStateHandle prevBS = grcStateBlock::BS_Active;
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	
	// set the depth state and rasterizer state based on whether we are inside or outside the light volume
	switch (pass)
	{
	default:
	case FOGVOLUME_SHADERPASS_INSIDE:

		grcStateBlock::SetRasterizerState(ms_rsFogVolumeInsideRender);
		grcStateBlock::SetDepthStencilState(ms_dsFogVolumeInsideRender, 0);
		break;

	case FOGVOLUME_SHADERPASS_OUTSIDE:
		grcStateBlock::SetRasterizerState(ms_rsFogVolumeOutsideRender);
		grcStateBlock::SetDepthStencilState(ms_dsFogVolumeOutsideRender, 0);
		break;
	}
	Vec3V fogVolumeColor = fogVolumeParams.col.GetRGB() * ScalarVFromF32(fogVolumeParams.hdrMult);

	//Apply lighting type
	if(fogVolumeParams.lightingType == FOGVOLUME_LIGHTING_DIRECTIONAL)
	{
		const CLightSource &dirLight = Lights::GetRenderDirectionalLight();
		Vec3V lightDir = VECTOR3_TO_VEC3V(dirLight.GetDirection());
		Vec3V lightCol = VECTOR3_TO_VEC3V(dirLight.GetColor());
		ScalarV lightIntensity = ScalarVFromF32(dirLight.GetIntensity());
		
		ScalarV sunAmount = Saturate(-lightDir.GetZ()); 

		fogVolumeColor *= lightCol * lightIntensity * sunAmount;

	}
	else if(fogVolumeParams.lightingType == FOGVOLUME_LIGHTING_FOGHDR)
	{
		//If we dont use the directional lighting, then we have to use the Fog HDR to control the density of the fog

		const ScalarV hdrMultiplier = ScalarVFromF32(currRenderKeyframe.GetVar(TCVAR_FOG_HDR));
		fogVolumeColor *= hdrMultiplier;
	}

	

	if(fogVolumeParams.useGroundFogColour)
	{
		//if this option is used, the color from the settings will act as a tint to the fog ground
		//color from the timecycle.
		Vec3V vFogColorGround = currRenderKeyframe.GetVarV3(TCVAR_FOG_NEAR_COL_R);
		fogVolumeColor *= vFogColorGround;
	}

	// setup constants
	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_COLOR] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_COLOR], fogVolumeColor);
	}
	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_POSITION] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_POSITION], fogVolumeParams.pos);
	}

	Mat34V transform = Mat34V(V_IDENTITY), rotationTransform = Mat34V(V_IDENTITY);
	Mat34V invTransform; 

	Mat34VFromScale(transform, fogVolumeParams.scale.GetX(), fogVolumeParams.scale.GetY(), fogVolumeParams.scale.GetZ());
	Mat34VFromEulersXYZ(rotationTransform, fogVolumeParams.rotation);
	Transform(transform, rotationTransform, transform);
	Invert3x3Full(invTransform, transform);
	//Vec3V scale = fogVolumeParams.scale;
	//Vec3V rotation = fogVolumeParams.rotation;

	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_TRANSFORM] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_TRANSFORM], transform);

	}
	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_INV_TRANSFORM] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_INV_TRANSFORM], invTransform);

	}
	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PARAMS] != grcevNONE)
	{
		Vec4V params = Vec4V(
			fogVolumeParams.density * Powf(10.0f, BANK_SWITCH(m_DebugDensityScale, FOGVOLUME_DEFAULT_DENSITY_SCALE)),
			fogVolumeParams.fallOff,
			fogVolumeParams.range,
			fogVolumeParams.fogMult
			);
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PARAMS], params);
	}

	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_SCREENSIZE] != grcevNONE)
	{
		Vec4V vOOScreenSize((float)GRCDEVICE.GetWidth(), (float)GRCDEVICE.GetHeight(), 1.0f/GRCDEVICE.GetWidth(), 1.0f/GRCDEVICE.GetHeight());
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_SCREENSIZE], vOOScreenSize);
	}

	Vec4V projParams;
	Vec3V shearProj0, shearProj1, shearProj2;
	DeferredLighting::GetProjectionShaderParams(projParams, shearProj0, shearProj1, shearProj2);

	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PROJECTION_PARAMS] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PROJECTION_PARAMS], projParams);
	}

	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS0] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS0], shearProj0);
	}

	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS1] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS1], shearProj1);
	}

	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS2] != grcevNONE)
	{
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS2], shearProj2);
	}

	if(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_DEPTHBUFFER] != grcevNONE)
	{
		//using the pre alpha depth buffer here similar to volumetric fx
		m_fogVolumeShader->SetVar(m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_DEPTHBUFFER], CRenderTargets::GetDepthBuffer_PreAlpha());
	}
	
	if(ShaderUtil::StartShader("FogVolume", m_fogVolumeShader, technique, pass))
	{
		RenderVolumeMesh(fogVolumeParams, pass);
		ShaderUtil::EndShader(m_fogVolumeShader);
	}

	grcStateBlock::SetBlendState(prevBS);
#if __BANK
	if(m_DebugFogVolumeRender && fogVolumeParams.debugRender)
	{
		Mat34V transform = Mat34V(V_IDENTITY), rotationTransform = Mat34V(V_IDENTITY);

		Mat34VFromEulersXYZ(rotationTransform, fogVolumeParams.rotation);
		Mat34VFromTranslation(transform, fogVolumeParams.pos);
		Transform(transform, transform, rotationTransform);

		grcDebugDraw::Ellipsoidal(
			transform,
			fogVolumeParams.scale * ScalarVFromF32(fogVolumeParams.range),
			(bImmediateMode?Color_green:Color_red),
			false);
	}
#endif
}


void CFogVolumeMgr::RenderVolumeMesh(const FogVolume_t& fogVolumeParams, FogVolumeShaderPasses_e pass)
{
	FogVolumeMeshLOD lodLevel = CalculateMeshLOD(fogVolumeParams, pass);

	FogVolumeVertexBufferType bufferType = FOGVOLUME_VB_HIDETAIL;
	switch(lodLevel)
	{
	case FOGVOLUME_MESHLOD_INSIDE:
	case FOGVOLUME_MESHLOD_OUTSIDE_LOW:
		bufferType = FOGVOLUME_VB_LOWDETAIL;
			break;
	case FOGVOLUME_MESHLOD_OUTSIDE_MED:
		bufferType = FOGVOLUME_VB_MIDDETAIL;
		break;
	case FOGVOLUME_MESHLOD_OUTSIDE_HIGH:
	default:
		bufferType = FOGVOLUME_VB_HIDETAIL;
		break;
	}

#if __BANK
	if(m_ForceDetailMeshIndex > -1)
	{
		bufferType = (FogVolumeVertexBufferType)m_ForceDetailMeshIndex;
	}
#endif

	CVolumeVertexBuffer* vb = &(g_FogVolumeVertexBuffers[bufferType]);

	vb->Draw();

}

///////////////////////////////////////////////////////////////////////////////
//  CalculateTechnique
///////////////////////////////////////////////////////////////////////////////
grcEffectTechnique CFogVolumeMgr::CalculateTechnique(const FogVolume_t& fogVolumeParams)
{
	if( !IsEqualAll(fogVolumeParams.scale, Vec3V(V_ONE)))
	{
		return m_fogVolumeShaderTechniques[FOGVOLUME_SHADERTECHNIQUE_ELLIPSOIDAL];
	}

	return m_fogVolumeShaderTechniques[FOGVOLUME_SHADERTECHNIQUE_DEFAULT];

}

///////////////////////////////////////////////////////////////////////////////
//  CalculateRadius
///////////////////////////////////////////////////////////////////////////////
__forceinline float CFogVolumeMgr::CalculateRadius(const FogVolume_t& fogVolumeParams)
{
	const float extraRadius =  fogVolumeParams.range * (EXTEND_LIGHT(LIGHT_TYPE_POINT) + BANK_SWITCH(m_DebugExtendedRadiusPercent, FOGVOLUME_DEFAULT_EXTEND_RADIUS_PERCENT)); // EXTEND_LIGHT is needed because the actual geometry is faceted, so it is larger than the actual cone/sphere
	const float radius = fogVolumeParams.range + extraRadius;

	return radius;


}

///////////////////////////////////////////////////////////////////////////////
//  CalculatePass
///////////////////////////////////////////////////////////////////////////////
FogVolumeShaderPasses_e CFogVolumeMgr::CalculatePass(const FogVolume_t& fogVolumeParams)
{
	const float radius = CalculateRadius(fogVolumeParams);

	const Vec3V camPos = gVpMan.GetRenderGameGrcViewport()->GetCameraMtx().GetCol3();

	const Vec3V distVec = camPos - fogVolumeParams.pos;
	bool inLight = (IsLessThanAll(MagSquared(distVec), ScalarVFromF32(radius * radius))) != 0;

	if(inLight)
	{
		return FOGVOLUME_SHADERPASS_INSIDE;
	}

	return FOGVOLUME_SHADERPASS_OUTSIDE;
}


///////////////////////////////////////////////////////////////////////////////
//  CalculatePass
///////////////////////////////////////////////////////////////////////////////
FogVolumeMeshLOD CFogVolumeMgr::CalculateMeshLOD(const FogVolume_t& fogVolumeParams, FogVolumeShaderPasses_e pass)
{
	const float radius = CalculateRadius(fogVolumeParams);
	const Vec3V camPos = gVpMan.GetRenderGameGrcViewport()->GetCameraMtx().GetCol3();

	const Vec3V distVec = camPos - fogVolumeParams.pos;

	const float lowLODDist = radius * BANK_SWITCH(m_DebugFogVolumeLowLODDistFactor, FOGVOLUME_DEFAULT_LOW_LOD_DIST_FACTOR);
	const float medLODDis = radius * BANK_SWITCH(m_DebugFogVolumeMedLODDistFactor, FOGVOLUME_DEFAULT_MED_LOD_DIST_FACTOR);

	//We can use the low detail mesh when the camera is inside the mesh. It saves some amount of verts
	if(pass == FOGVOLUME_SHADERPASS_INSIDE)
	{
		return FOGVOLUME_MESHLOD_INSIDE;
	}


	if(IsGreaterThanAll(MagSquared(distVec), ScalarVFromF32(lowLODDist * lowLODDist)))
	{
		return FOGVOLUME_MESHLOD_OUTSIDE_LOW;
	}

	if(IsGreaterThanAll(MagSquared(distVec), ScalarVFromF32(medLODDis * medLODDis)))
	{
		return FOGVOLUME_MESHLOD_OUTSIDE_MED;
	}


	return FOGVOLUME_MESHLOD_OUTSIDE_HIGH;

}


///////////////////////////////////////////////////////////////////////////////
//  Accessors & Mutators
///////////////////////////////////////////////////////////////////////////////
u32 CFogVolumeMgr::GetNumFogVolumesRegisteredUpdate()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CFogVolumeMgr::GetNumFogVolumesRegisteredUpdate - not called from the update thread");
	return m_numRegisteredFogVolumes[FOGVOLUME_UPDATE_BUFFER_ID];
}
void CFogVolumeMgr::SetNumFogVolumesRegisteredUpdate(u32 numFogVolumes)
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CFogVolumeMgr::GetNumFogVolumesRegisteredUpdate - not called from the update thread");
	m_numRegisteredFogVolumes[FOGVOLUME_UPDATE_BUFFER_ID] = numFogVolumes;
}
u32 CFogVolumeMgr::GetNumFogVolumesRegisteredRender()
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CFogVolumeMgr::GetNumFogVolumesRegisteredRender - not called from the render thread");
	return m_numRegisteredFogVolumes[FOGVOLUME_RENDER_BUFFER_ID];
}
void CFogVolumeMgr::SetNumFogVolumesRegisteredRender(u32 numFogVolumes)
{
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "CFogVolumeMgr::GetNumFogVolumesRegisteredRender - not called from the render thread");
	m_numRegisteredFogVolumes[FOGVOLUME_RENDER_BUFFER_ID] = numFogVolumes;
}


#if __BANK

///////////////////////////////////////////////////////////////////////////////
// Widgets
///////////////////////////////////////////////////////////////////////////////
void CFogVolumeMgr::RenderTestFogVolume			()
{
	g_fogVolumeMgr.m_TestRender = true;

	//Let's get the current interior location of the fog volume
	g_fogVolumeMgr.m_TestInteriorHash = CVfxHelper::GetHashFromInteriorLocation(CVfxHelper::GetCamInteriorLocation());
}

void CFogVolumeMgr::RenderTestFogVolumeInFrontOfCam			()
{
	g_fogVolumeMgr.m_TestRender = true;
	Mat34V camMat = RCC_MAT34V(camInterface::GetMat());
	g_fogVolumeMgr.m_TestPosition = camMat.GetCol3() + camMat.GetCol1() * ScalarVFromF32(g_fogVolumeMgr.m_TestDistanceFromCam);
	//Let's get the current interior location of the fog volume
	g_fogVolumeMgr.m_TestInteriorHash = CVfxHelper::GetHashFromInteriorLocation(CVfxHelper::GetCamInteriorLocation());
}


void CFogVolumeMgr::RemoveTestFogVolume		()
{
	g_fogVolumeMgr.m_TestRender = false;

}

void CFogVolumeMgr::PrintTestFogVolumeParams()
{
	CVfxFogVolumeInfo fogVolumeInfoParams;

	fogVolumeInfoParams.m_fadeDistStart = g_fogVolumeMgr.m_TestFadeStart;
	fogVolumeInfoParams.m_fadeDistEnd = g_fogVolumeMgr.m_TestFadeEnd;
	fogVolumeInfoParams.m_position = g_fogVolumeMgr.m_TestPosition;
	fogVolumeInfoParams.m_rotation = g_fogVolumeMgr.m_TestRotation;
	fogVolumeInfoParams.m_scale = g_fogVolumeMgr.m_TestScale;
	fogVolumeInfoParams.m_colR = g_fogVolumeMgr.m_TestColor.GetRed();
	fogVolumeInfoParams.m_colG = g_fogVolumeMgr.m_TestColor.GetGreen();
	fogVolumeInfoParams.m_colB = g_fogVolumeMgr.m_TestColor.GetBlue();
	fogVolumeInfoParams.m_colA = g_fogVolumeMgr.m_TestColor.GetAlpha();
	fogVolumeInfoParams.m_hdrMult = g_fogVolumeMgr.m_TestHDRMult;
	fogVolumeInfoParams.m_range = g_fogVolumeMgr.m_TestRange;
	fogVolumeInfoParams.m_density = g_fogVolumeMgr.m_TestDensity;
	fogVolumeInfoParams.m_falloff = g_fogVolumeMgr.m_TestFallOff;
	fogVolumeInfoParams.m_lightingType = (FogVolumeLighting_e) g_fogVolumeMgr.m_TestLightingType;
	fogVolumeInfoParams.m_interiorHash = g_fogVolumeMgr.m_TestInteriorHash;
	fogVolumeInfoParams.m_isUnderwater = false;

	g_vfxFogVolumeInfoMgr.PrintFogVolumeParams(&fogVolumeInfoParams);
}

void CFogVolumeMgr::InitWidgets()
{
	bkBank* pVfxBank = vfxWidget::GetBank();

	pVfxBank->PushGroup("Fog Volumes");
	pVfxBank->AddSlider("Force Mesh Detail", &m_ForceDetailMeshIndex, -1, FOGVOLUME_VB_HIDETAIL, 1);
	pVfxBank->AddToggle("Disable Fog Volume Render", &m_DisableFogVolumeRender);
	pVfxBank->AddSlider("Extend Radius For Backface Technique", &m_DebugExtendedRadiusPercent, 0.0f, 100.0f, 0.01f);
	pVfxBank->AddSlider("Density Scale Adjust", &m_DebugDensityScale, -10.0f, 10.0f, 0.001f);
	pVfxBank->AddSlider("Low LOD Distance Factor", &m_DebugFogVolumeLowLODDistFactor, 1.0f, 100.0f, 0.01f);
	pVfxBank->AddSlider("Medium LOD Distance Factor", &m_DebugFogVolumeMedLODDistFactor, 1.0f, 100.0f, 0.01f);
	pVfxBank->AddToggle("Render Debug Fog Volumes", &m_DebugFogVolumeRender);
	pVfxBank->AddSlider("Num Registered Fog Volumes", &(m_numRegisteredFogVolumes[FOGVOLUME_UPDATE_BUFFER_ID]), 0, FOGVOLUME_MAX_STORED, 0);
	pVfxBank->AddToggle("Disable Occlusion Test", &m_DisableOcclusionTest);

	g_vfxFogVolumeInfoMgr.InitWidgets();

	pVfxBank->PushGroup("Test Fog Volume");
	pVfxBank->AddButton("Print Test Fog Volume Parameters", PrintTestFogVolumeParams);
	pVfxBank->AddSlider("Test Distance From Cam", &m_TestDistanceFromCam, 0.0f, 1000.0f, 0.1f);
	pVfxBank->AddVector("Test Position", &m_TestPosition, -10000.0f, 10000.0f, 0.1f);
	pVfxBank->AddVector("Test Scale", &m_TestScale, 0.0f, 1.0f, 0.001f);
	pVfxBank->AddVector("Test Rotation", &m_TestRotation, -360.0f, 360.0f, 0.01f);	
	pVfxBank->AddColor("Test Color", &m_TestColor);
	pVfxBank->AddSlider("Test Density", &m_TestDensity, 0.0f, 100.0f, 0.001f);
	pVfxBank->AddSlider("Test Range", &m_TestRange, 0.0f, 1000.0f, 0.001f);
	pVfxBank->AddSlider("Test FallOff", &m_TestFallOff, 0.0f, 256.0f, 1.0f);
	pVfxBank->AddSlider("Test HDR Mult", &m_TestHDRMult, 0.0f, 100.0f, 0.001f);
	pVfxBank->AddToggle("Apply Test Fade", &m_TestApplyFade);
	pVfxBank->AddSlider("Test Fade Start", &m_TestFadeStart, 0.0f, 10000.0f, 0.1f);
	pVfxBank->AddSlider("Test Fade End", &m_TestFadeEnd, 0.0f, 10000.0f, 0.1f);

	const char* lightingTypes[] =
	{
		"Fog HDR",
		"Directional Lighting",
		"No Lighting"
	};
	CompileTimeAssert(NELEM(lightingTypes) == FOGVOLUME_LIGHTING_NUM);
	pVfxBank->AddCombo("Force Lighting Type", &m_TestLightingType, NELEM(lightingTypes), lightingTypes);

	pVfxBank->AddToggle("Test Use Ground Fog Colour", &m_TestUseGroundFogColour);
	
	pVfxBank->AddButton("Render Test Fog Volume", RenderTestFogVolume);
	pVfxBank->AddButton("Render Test Fog Volume in Front of Camera", RenderTestFogVolumeInFrontOfCam);
	pVfxBank->AddButton("Remove Test Fog Volume", RemoveTestFogVolume);
	pVfxBank->PopGroup();

	pVfxBank->PopGroup();
}
#endif
