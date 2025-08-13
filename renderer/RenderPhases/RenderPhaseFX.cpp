//
// description:	Collection of effects related renderphases.
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseFX.h"

#include <stdarg.h>
// C headers
// Rage headers
#include "grcore/allocscope.h"
#include "profile/group.h"
#include "grmodel/shaderfx.h"

// Game headers
#include "camera/Viewports/ViewportManager.h"
#include "peds/Ped.h"
#include "Renderer/occlusion.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/sprite2d.h"
#include "scene/scene.h"
#include "scene/EntityIterator.h"
#include "scene/world/GameWorld.h"
#include "shaders/CustomShaderEffectPed.h"
#include "timecycle/TimeCycleConfig.h"
#include "vfx/particles/PtFxManager.h"
#include "vfx/VisualEffects.h"
#include "renderer/Lights/lights.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

EXT_PF_GROUP(BuildRenderList);
PF_TIMER(RenderPhaseSeeThroughRL,BuildRenderList);

EXT_PF_GROUP(RenderPhase);
PF_TIMER(RenderPhaseSeeThrough,RenderPhase);

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------



RenderPhaseSeeThrough* RenderPhaseSeeThrough::sm_phase = NULL;

float RenderPhaseSeeThrough::sm_heatScale[TB_COUNT];

grcRenderTarget *RenderPhaseSeeThrough::sm_color;
grcRenderTarget *RenderPhaseSeeThrough::sm_blur;
grcRenderTarget *RenderPhaseSeeThrough::sm_depth;
	
s32 RenderPhaseSeeThrough::sm_TechniqueGroupId;
	
bool RenderPhaseSeeThrough::sm_enabled;
bool sm_pedQuadDontScale;
float RenderPhaseSeeThrough::sm_pedQuadSize;
float RenderPhaseSeeThrough::sm_pedQuadStartDistance;
float RenderPhaseSeeThrough::sm_pedQuadEndDistance;
float RenderPhaseSeeThrough::sm_pedQuadScale;

float RenderPhaseSeeThrough::sm_fadeStartDistance;
float RenderPhaseSeeThrough::sm_fadeEndDistance;
float RenderPhaseSeeThrough::sm_maxThickness;

float RenderPhaseSeeThrough::sm_minNoiseAmount;
float RenderPhaseSeeThrough::sm_maxNoiseAmount;
float RenderPhaseSeeThrough::sm_hiLightIntensity;
float RenderPhaseSeeThrough::sm_hiLightNoise;

Color32 RenderPhaseSeeThrough::sm_colorNear;
Color32 RenderPhaseSeeThrough::sm_colorFar;
Color32 RenderPhaseSeeThrough::sm_colorVisibleBase;
Color32 RenderPhaseSeeThrough::sm_colorVisibleWarm;
Color32 RenderPhaseSeeThrough::sm_colorVisibleHot;

ptxShaderTemplateOverride RenderPhaseSeeThrough::sm_ptxPointShaderOverride;
ptxShaderTemplateOverride RenderPhaseSeeThrough::sm_ptxTrailShaderOverride;

// state blocks for ped quad rendering
grcRasterizerStateHandle	RenderPhaseSeeThrough::sm_pedQuadsRS = grcStateBlock::RS_Invalid;
grcBlendStateHandle			RenderPhaseSeeThrough::sm_pedQuadsBS = grcStateBlock::BS_Invalid;
grcDepthStencilStateHandle	RenderPhaseSeeThrough::sm_pedQuadsDS = grcStateBlock::DSS_Invalid;

// state blocks for main pass 
grcRasterizerStateHandle	RenderPhaseSeeThrough::sm_mainPassRS = grcStateBlock::RS_Invalid;
grcBlendStateHandle			RenderPhaseSeeThrough::sm_mainPassBS = grcStateBlock::BS_Invalid;
grcDepthStencilStateHandle	RenderPhaseSeeThrough::sm_mainPassDS = grcStateBlock::DSS_Invalid;
grcRasterizerStateHandle	RenderPhaseSeeThrough::sm_exitMainPassRS = grcStateBlock::RS_Invalid;
grcBlendStateHandle			RenderPhaseSeeThrough::sm_exitMainPassBS = grcStateBlock::BS_Invalid;
grcDepthStencilStateHandle	RenderPhaseSeeThrough::sm_exitMainPassDS = grcStateBlock::DSS_Invalid;

#if __BANK
bool g_debugRenderSeeThroughQuads = true;
#endif // __BANK

// --- Code ---------------------------------------------------------------------

// ********************************
// ******** "Heat" Vision *********
// ********************************
void RenderPhaseSeeThrough::InitClass()
{
	sm_TechniqueGroupId = grmShaderFx::FindTechniqueGroupId("seethrough");
	Assert(-1 != sm_TechniqueGroupId);

	InitRenderStateBlocks();
}

void RenderPhaseSeeThrough::InitRenderStateBlocks() 
{
	// ped quad rendering
	grcDepthStencilStateDesc dssDesc;
	dssDesc.DepthFunc = grcRSV::CMP_LESSEQUAL;
	sm_pedQuadsDS = grcStateBlock::CreateDepthStencilState(dssDesc);

	grcRasterizerStateDesc rsDesc;
	rsDesc.CullMode = grcRSV::CULL_NONE;
	sm_pedQuadsRS = grcStateBlock::CreateRasterizerState(rsDesc);

	grcBlendStateDesc bsDesc;
	sm_pedQuadsBS = grcStateBlock::CreateBlendState(bsDesc);

	// main pass rendering
	sm_mainPassDS = grcStateBlock::CreateDepthStencilState(dssDesc);
	sm_mainPassRS = grcStateBlock::RS_Default;
	sm_mainPassBS = grcStateBlock::CreateBlendState(bsDesc);

	sm_exitMainPassDS = grcStateBlock::CreateDepthStencilState(dssDesc);
	sm_exitMainPassRS = grcStateBlock::RS_Default;
	sm_exitMainPassBS = grcStateBlock::CreateBlendState(bsDesc);

}

void RenderPhaseSeeThrough::InitSession()
{
	sm_enabled = false;
	sm_pedQuadDontScale = false;
}

void RenderPhaseSeeThrough::UpdateVisualDataSettings()
{
	sm_pedQuadSize = g_visualSettings.Get("seeThrough.PedQuadSize", 0.5f);
	sm_pedQuadStartDistance = g_visualSettings.Get("seeThrough.PedQuadStartDistance", 0.0f);
	sm_pedQuadEndDistance = g_visualSettings.Get("seeThrough.PedQuadEndDistance", 1500.0f);
	sm_pedQuadScale = g_visualSettings.Get("seeThrough.PedQuadScale", 12.0f);

	sm_fadeStartDistance = g_visualSettings.Get("seeThrough.FadeStartDistance", 100);
	sm_fadeEndDistance = g_visualSettings.Get("seeThrough.FadeEndDistance", 500.0f);
	SetMaxThickness(g_visualSettings.Get("seeThrough.MaxThickness", 10000.0f)); // Use the Set function so that we clamp to [1,10000]
	
	sm_minNoiseAmount = g_visualSettings.Get("seeThrough.MinNoiseAmount", 0.25f);
	sm_maxNoiseAmount = g_visualSettings.Get("seeThrough.MaxNoiseAmount", 1.0f);
	sm_hiLightIntensity = g_visualSettings.Get("seeThrough.HiLightIntensity", 0.5f);
	sm_hiLightNoise = g_visualSettings.Get("seeThrough.HiLightNoise", 0.5f);

	sm_colorNear = Color32(g_visualSettings.GetColor("seeThrough.ColorNear"));
	sm_colorFar = Color32(g_visualSettings.GetColor("seeThrough.ColorFar"));
	sm_colorVisibleBase = Color32(g_visualSettings.GetColor("seeThrough.ColorVisibleBase"));
	sm_colorVisibleWarm = Color32(g_visualSettings.GetColor("seeThrough.ColorVisibleWarm"));
	sm_colorVisibleHot = Color32(g_visualSettings.GetColor("seeThrough.ColorVisibleHot"));
}

#if __BANK
void RenderPhaseSeeThrough::AddWidgets(bkGroup& group)
{
	bkGroup &bank = *group.AddGroup("SeeThrough Effect", false);
	bank.AddToggle("Enable", &sm_enabled);

	bank.AddToggle("Render see through quads", &g_debugRenderSeeThroughQuads);

	bank.AddToggle("Don't scale ped quads on distance",&sm_pedQuadDontScale);

	bank.AddSlider("Ped quad size", &sm_pedQuadSize, 0.0f, 100.0f, 0.5f);
	bank.AddSlider("Ped quad scale start distance", &sm_pedQuadStartDistance, 0.0f, 10000.0f, 1.0f);
	bank.AddSlider("Ped quad scale end distance", &sm_pedQuadEndDistance, 0.0f, 10000.0f, 1.0f);
	bank.AddSlider("Ped quad scale factor", &sm_pedQuadScale, 0.0f, 100.0f, 0.5f);
	
	bank.AddSlider("Fade start distance", &sm_fadeStartDistance, 0.0f, 10000.0f, 1.0f);
	bank.AddSlider("Fade end distance", &sm_fadeEndDistance, 0.0f, 10000.0f, 1.0f);
	bank.AddSlider("Maximum see through thickness", &sm_maxThickness, 1.0f, 10000.0f, 1.0f);

	bank.AddSlider("Near noise", &sm_minNoiseAmount, 0.0f, 2.0f, 0.1f);
	bank.AddSlider("Far noise", &sm_maxNoiseAmount, 0.0f, 2.0f, 0.1f);
	bank.AddSlider("HiLight intensity", &sm_hiLightIntensity, 0.0f, 10.0f, 0.1f);
	bank.AddSlider("HiLight noise", &sm_hiLightNoise, 0.0f, 2.0f, 0.1f);

	bank.AddColor("Near color", &sm_colorNear);
	bank.AddColor("Far color", &sm_colorFar);
	bank.AddColor("Visible color base", &sm_colorVisibleBase);
	bank.AddColor("Visible color warm", &sm_colorVisibleWarm);
	bank.AddColor("Visible color hot", &sm_colorVisibleHot);
	
	bank.AddSlider("Heat Scale, Dead",&sm_heatScale[TB_DEAD],0.0f,1.0f,1.0f/32.0f);
	bank.AddSlider("Heat Scale, Cold",&sm_heatScale[TB_COLD],0.0f,1.0f,1.0f/32.0f);
	bank.AddSlider("Heat Scale, Warm",&sm_heatScale[TB_WARM],0.0f,1.0f,1.0f/32.0f);
	bank.AddSlider("Heat Scale, Hot",&sm_heatScale[TB_HOT],0.0f,1.0f,1.0f/32.0f);
}
#endif // __BANK

class dlCmdDrawSeeThroughQuads : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_DrawSeeThroughQuads,
	};

	struct pedQuadPoint
	{
		Vector3 pos;
		float scale;
		float heat;
	};
	 
	s32 GetCommandSize()						{ return(sizeof(*this)); }
	dlCmdDrawSeeThroughQuads(const grcViewport *viewport) 
	{ 
		CPed::Pool* pedPool= CPed::GetPool();
		const int maxPedCount = pedPool->GetNoOfUsedSpaces();
		actualPedCount = 0;
		totalPedCount = maxPedCount;
		
		pedquads = gDCBuffer->AllocateObjectFromPagedMemory<pedQuadPoint>(DPT_LIFETIME_RENDER_THREAD, sizeof(pedQuadPoint) * maxPedCount);
		pedQuadPoint* pDest = pedquads;
		
		Vec3V camPos = viewport->GetCameraPosition();
		
		CEntityIterator entityIterator( IteratePeds );
		CPed* pPed = static_cast<CPed*>(entityIterator.GetNext());
		while(pPed)
		{
			CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
			const float heat = CCustomShaderEffectPed::GetPedHeat(pPed, pModelInfo->GetThermalBehaviour());;
			Vec3V diff = pPed->GetTransform().GetPosition() - camPos;
			const float camDist2 = MagSquared(diff).Getf();
			const float distCull = 50.0f *50.0f;
			if( heat > 0.0f && camDist2 > distCull)
			{
				pDest->pos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				pDest->heat = heat; 
				
				const Vector3 boundMin = pPed->GetBoundingBoxMin();
				const Vector3 boundMax = pPed->GetBoundingBoxMax();

				float xx = boundMax.x - boundMin.x;
				float yy = boundMax.y - boundMin.y;
				float zz = boundMax.z - boundMin.z;
				
				pDest->scale = Max(xx,Max(yy,zz)) * 0.5f;
				pDest++;
				actualPedCount++;
			}
			pPed = static_cast<CPed*>(entityIterator.GetNext());
		}
		
	}
	
	static void ExecuteStatic(dlCmdBase &cmd) { ((dlCmdDrawSeeThroughQuads &) cmd).Execute(); }
	
	void Execute()
	{ 
		pedQuadPoint* points = pedquads;

		if( actualPedCount > 0 )
		{
			GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

			grcViewport::SetCurrentWorldIdentity();

			// store some viewport and camera info
			const grcViewport* pCurrViewport = grcViewport::GetCurrent();
			const Matrix34 &camMtx = RCC_MATRIX34(pCurrViewport->GetCameraMtx());
			const Vector3 camRight = camMtx.a;
			const Vector3 camUp = camMtx.b;
			const Vector3 camPos = camMtx.d;
			
			const float pedQuadSize = RenderPhaseSeeThrough::GetPedQuadSize();
			const float pedQuadEndDistance = RenderPhaseSeeThrough::GetPedQuadEndDistance();
			const float pedQuadStartdistance = RenderPhaseSeeThrough::GetPedQuadStartDistance();
			const float pedQuadScale = RenderPhaseSeeThrough::GetPedQuadScale();

			RenderPhaseSeeThrough::SetGlobals();

			CSprite2d seeThroughQuad;
			seeThroughQuad.BeginCustomList(CSprite2d::CS_SEETHROUGH,NULL);
			
			RenderPhaseSeeThrough::SetPedQuadsRenderState();

			grcDrawMode drawMode = (__WIN32PC || RSG_DURANGO) ? drawTriStrip : drawQuads;

#if __WIN32PC || RSG_DURANGO
			grcBegin(drawMode,6*actualPedCount);
#else
			grcBegin(drawMode,4*actualPedCount);
#endif
			const float sizeScale = pedQuadSize * pedQuadScale * pCurrViewport->GetTanHFOV() * 2.0f;
			const float quadDist = pedQuadEndDistance-pedQuadStartdistance;

			for(int i=0;i<actualPedCount;i++)
			{
				const Vector3 pos = points->pos;
				const float scale = points->scale;
				const float heat = points->heat * 1.0f/255.0f;
				points++;

				const float dist = (camPos - pos).Mag();
				const float distFade = (1.0f - Clamp( (pedQuadEndDistance-dist)/quadDist, 0.0f, 1.0f)) * sizeScale;

				const Vector3 right	= sm_pedQuadDontScale ? camRight * pedQuadSize : camRight * distFade;
				const Vector3 up = sm_pedQuadDontScale ? camUp * pedQuadSize : camUp * distFade;
				
				const Vector3 p1 = pos - right * scale + up * scale;
				const Vector3 p2 = pos + right * scale + up * scale;
#if __WIN32PC || RSG_DURANGO
				const Vector3 p3 = pos - right * scale - up * scale;
				const Vector3 p4 = pos + right * scale - up * scale;
#else				
				const Vector3 p3 = pos + right * scale - up * scale;
				const Vector3 p4 = pos - right * scale - up * scale;
#endif // __WIN32PC

#if __WIN32PC || RSG_DURANGO
				// place degenerative triangles for pc
				grcVertex(	p1.GetX(), p1.GetY(), p1.GetZ(), 0.0f, 0.0f, 1.0f, Color32(heat,heat,heat), 0.0f, 1.0f);
#endif
				grcVertex(	p1.GetX(), p1.GetY(), p1.GetZ(), 0.0f, 0.0f, 1.0f, Color32(heat,heat,heat), 0.0f, 1.0f);
				grcVertex(	p2.GetX(), p2.GetY(), p2.GetZ(), 0.0f, 0.0f, 1.0f, Color32(heat,heat,heat), 0.0f, 1.0f);
				grcVertex(	p3.GetX(), p3.GetY(), p3.GetZ(), 0.0f, 0.0f, 1.0f, Color32(heat,heat,heat), 0.0f, 1.0f);
				grcVertex(	p4.GetX(), p4.GetY(), p4.GetZ(), 0.0f, 0.0f, 1.0f, Color32(heat,heat,heat), 0.0f, 1.0f);

#if __WIN32PC || RSG_DURANGO
				grcVertex(	p4.GetX(), p4.GetY(), p4.GetZ(), 0.0f, 0.0f, 1.0f, Color32(heat,heat,heat), 0.0f, 1.0f);
#endif
			}
			
			grcEnd();
			
			seeThroughQuad.EndCustomList();
		}
	}
	
private:
	int actualPedCount;
	int totalPedCount;
	pedQuadPoint *pedquads;
};


RenderPhaseSeeThrough::RenderPhaseSeeThrough(CViewport* pGameViewport)
: CRenderPhaseScanned(pGameViewport, "SeeThrough", DL_RENDERPHASE_SEETHROUGH_MAP, 0.0f)
{
	SetRenderFlags(0);

	SetVisibilityType( VIS_PHASE_SEETHROUGH_MAP );
	GetEntityVisibilityMask().SetAllFlags( VIS_ENTITY_MASK_PED );
	static bool firstTime = true;
	if (firstTime)
	{
		firstTime = false;
		DLC_REGISTER_EXTERNAL(dlCmdDrawSeeThroughQuads);
	}

	Assert(sm_phase == NULL);
	sm_phase = this;

	m_disableOnPause = false;

	ResetHeatScale();

	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase( RPTYPE_MAIN, this ));

}

RenderPhaseSeeThrough::~RenderPhaseSeeThrough()
{
	if (sm_color) delete sm_color;
	if (sm_blur) delete sm_blur;
	if (sm_depth) delete sm_depth;

	sm_phase = NULL;
}

void RenderPhaseSeeThrough::UpdateViewport()
{
	if( false == sm_enabled )
		return;

	SetActive(true);
	CRenderPhaseScanned::UpdateViewport();
}

void RenderPhaseSeeThrough::BuildRenderList()
{
	PF_FUNC(RenderPhaseSeeThroughRL);

	if( false == sm_enabled )
		return;

	COcclusion::BeforeSetupRender(false, false, SHADOWTYPE_NONE, true, GetGrcViewport());

	CScene::SetupRender(GetEntityListIndex(), GetRenderFlags(), GetGrcViewport(), this);

	COcclusion::AfterPreRender();
	
	static bool shaderOverrideInit = false;
	if(shaderOverrideInit == false)
	{
		shaderOverrideInit = true;
		
		ptxShaderTemplate* pointShader = ptxShaderTemplateList::GetShader("ptfx_sprite");
		sm_ptxPointShaderOverride.Init(pointShader->GetGrmShader());
		grcEffectTechnique pointTechnique = pointShader->GetGrmShader()->LookupTechnique("RGBA_Thermal", true);
		sm_ptxPointShaderOverride.OverrideAllTechniquesWith(pointTechnique);

	
		ptxShaderTemplate* trailShader = ptxShaderTemplateList::GetShader("ptfx_trail");
		sm_ptxTrailShaderOverride.Init(trailShader->GetGrmShader());

		int techCount = sm_ptxTrailShaderOverride.GetOverrideTechniqueCount();
		for (int i = 0; i < techCount; i++)
		{
			sm_ptxTrailShaderOverride.GetTechniqueOverride(i).bDisable = true;
		}

	}
}

void RenderPhaseSeeThrough::ptFxSimpleRender(grcRenderTarget* backBuffer, grcRenderTarget *depthBuffer)
{
	ptxShaderTemplate* pointShader = ptxShaderTemplateList::GetShader("ptfx_sprite");
	ptxShaderTemplate* trailShader = ptxShaderTemplateList::GetShader("ptfx_trail");
	
	pointShader->SetShaderOverride(&sm_ptxPointShaderOverride);
	trailShader->SetShaderOverride(&sm_ptxTrailShaderOverride);
	
	g_ptFxManager.RenderSimple(backBuffer, depthBuffer);

	pointShader->SetShaderOverride(NULL);
	trailShader->SetShaderOverride(NULL);
}

void RenderPhaseSeeThrough::SetGlobals()
{
	Vector4 params(sm_fadeStartDistance, sm_fadeEndDistance, sm_maxThickness,1.0f);
	CShaderLib::ForceScalarGlobals2(params);
}

void RenderPhaseSeeThrough::BuildDrawList()											
{ 
	PF_FUNC(RenderPhaseSeeThrough);

	if(!HasEntityListIndex())
		return;

	if( false == sm_enabled )
		return;

	DrawListPrologue();

	DLC(dlCmdLockRenderTarget, (0, sm_color, sm_depth));

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

	DLC_Add(RenderPhaseSeeThrough::PrepareMainPass);

	XENON_ONLY(DLC_Add(dlCmdEnableHiZ, true);)
	DLC(dlCmdClearRenderTarget, (true,Color32(0x00000000),true,1.0f,true,128));

	DLCPushTimebar("Render");

#if MULTIPLE_RENDER_THREADS
	CViewportManager::DLCRenderPhaseDrawInit();
#endif

	DLC_Add(Lights::SetupDirectionalAndAmbientGlobals);

	Vector4 params(sm_fadeStartDistance, sm_fadeEndDistance, sm_maxThickness,1.0f);
	DLC_Add(CShaderLib::ForceScalarGlobals2,params);

	DLC_Add(CShaderLib::SetFogRayTexture);

	DLC (dlCmdShaderFxPushForcedTechnique, (sm_TechniqueGroupId));

	RENDERLIST_DUMP_PASS_INFO_BEGIN("RenderPhaseSeeThrough");
	CRenderListBuilder::AddToDrawList(
		GetEntityListIndex(),
			CRenderListBuilder::RENDER_OPAQUE|
			CRenderListBuilder::RENDER_FADING|
			CRenderListBuilder::RENDER_DONT_RENDER_PLANTS|
			CRenderListBuilder::RENDER_DONT_RENDER_TREES|
			CRenderListBuilder::RENDER_DONT_RENDER_DECALS_CUTOUTS,
		CRenderListBuilder::USE_SIMPLE_LIGHTING);
	RENDERLIST_DUMP_PASS_INFO_END();

	DLC (dlCmdShaderFxPopForcedTechnique, ());

	DLC_Add(RenderPhaseSeeThrough::FinishMainPass);

	if (CPed::GetPool()->GetNoOfUsedSpaces() BANK_ONLY(&& g_debugRenderSeeThroughQuads))
	{
		DLC (dlCmdDrawSeeThroughQuads, (gVpMan.GetUpdateGameGrcViewport()));
	}
	
	DLC_Add(ptFxSimpleRender,sm_color, sm_depth);

	DLC_Add(CShaderLib::ResetScalarGlobals2);

	DLC_Add(CVisualEffects::RenderSky, (CVisualEffects::RM_SEETHROUGH), false, false, -1, -1, -1, -1, true, 1.0f);

	DLCPopTimebar();

	grcResolveFlags flags;
	flags.MipMap = false;

	DLC(dlCmdUnLockRenderTarget, (0,&flags));

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DrawListEpilogue();
}

u32 RenderPhaseSeeThrough::GetCullFlags() const
{
	u32 flags =
		CULL_SHAPE_FLAG_RENDER_EXTERIOR		|
		CULL_SHAPE_FLAG_RENDER_INTERIOR		|
		CULL_SHAPE_FLAG_TRAVERSE_PORTALS;

	return flags;
}


void RenderPhaseSeeThrough::SetPedQuadsRenderState() 
{
	grcStateBlock::SetRasterizerState(sm_pedQuadsRS);
	grcStateBlock::SetBlendState(sm_pedQuadsBS);
	grcStateBlock::SetDepthStencilState(sm_pedQuadsDS);
}

void RenderPhaseSeeThrough::PrepareMainPass()
{
	grcStateBlock::SetRasterizerState(sm_mainPassRS);
	grcStateBlock::SetBlendState(sm_mainPassBS);
	grcStateBlock::SetDepthStencilState(sm_mainPassDS);
}

void RenderPhaseSeeThrough::FinishMainPass() 
{
	grcStateBlock::SetRasterizerState(sm_exitMainPassRS);
	grcStateBlock::SetBlendState(sm_exitMainPassBS);
	grcStateBlock::SetDepthStencilState(sm_exitMainPassDS);
}
