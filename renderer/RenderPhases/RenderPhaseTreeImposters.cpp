//
// filename:	RenderPhaseWater.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseTreeImposters.h"

#include <stdarg.h>
// C headers
// Rage headers
#include "grcore/allocscope.h"
#include "grmodel/shaderfx.h"
#include "profile/timebars.h"
#include "profile/profiler.h"
// Game headers
#include "camera/viewports/Viewport.h"
#include "game/clock.h"
#include "modelinfo/modelinfo.h"
//#include "Renderer/DeferredLightingHelper.h"
#include "renderer/DrawLists/drawlistMgr.h"
#include "Renderer/occlusion.h"
#include "renderer/renderListGroup.h"
#include "renderer/treeimposters.h"
#include "renderer/TreeImposters.h"
#include "scene/scene.h"
#include "shaders/shaderlib.h"
#include "Vfx/VisualEffects.h"

#if USE_TREE_IMPOSTERS

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
// Custom draw list commands
class dlCmdStaticRenderImposterModel : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_StaticRenderImposterModel,
	};

	dlCmdStaticRenderImposterModel(CBaseModelInfo* pModelInfo, int pass)
		: m_pModelInfo(pModelInfo)
		, m_pass(pass)
	{
	}

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	inline void Execute();
	static void ExecuteStatic(dlCmdBase &cmd)	{ ((dlCmdStaticRenderImposterModel &) cmd).Execute(); }

private:
	CBaseModelInfo* m_pModelInfo;
	int m_pass;
};

class dlCmdRenderLeafColour : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_RenderLeafColour,
	};

	dlCmdRenderLeafColour(int cache_index)
		: m_cache_index(cache_index)
	{
	}

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	inline void Execute()						{ CTreeImposters::RenderLeafColour(m_cache_index); }
	static void ExecuteStatic(dlCmdBase &cmd)	{ ((dlCmdRenderLeafColour &) cmd).Execute(); }

private:
	int m_cache_index;
};


// --- Globals ------------------------------------------------------------------

EXT_PF_GROUP(BuildRenderList);

PF_TIMER(CRenderPhaseTreeImpostersRL,BuildRenderList);

EXT_PF_GROUP(RenderPhase);

PF_TIMER(CRenderPhaseTreeImposters,RenderPhase);

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

void CRenderPhaseTreeImposters::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdStaticRenderImposterModel);
	DLC_REGISTER_EXTERNAL(dlCmdRenderLeafColour);
}

static grcBlendStateHandle s_hDepthOnlyBlendState = grcStateBlock::BS_Invalid;


static void StaticRenderImposterModel(CBaseModelInfo* pModelInfo, int pass)
{
	rmcDrawable* pDrawable = pModelInfo->GetDrawable();

//	if (pDrawable)
	{
		grcStateBlock::SetBlendState(grcStateBlock::BS_Add);
		grcStateBlock::SetRasterizerState( grcStateBlock::RS_NoBackfaceCull);

		Matrix34 mat;
		mat.Identity();

		//float f=float(pass);
		//CShaderLib::SetEmissiveColorize(Vector4(f,f,f,f));

		s32 previousGroupId = grmShaderFx::GetForcedTechniqueGroupId();

		if (pass==-1)
			grmShaderFx::SetForcedTechniqueGroupId(CTreeImposters::GetImposterTechniqueGroupId());
		else
			grmShaderFx::SetForcedTechniqueGroupId(CTreeImposters::GetImposterDeferredTechniqueGroupId());

		static bool debugDrawLeaves=true;
		static bool debugDrawTrunk=false;

		if (debugDrawLeaves)
		{
			// Don't pass this through if the lod is NULL, it crashes in the draw fn otherwise.
			if (pDrawable->GetLodGroup().ContainsLod(LOD_HIGH)){
				grcStateBlock::SetBlendState(s_hDepthOnlyBlendState);

				pDrawable->Draw(mat, CRenderer::GenerateBucketMask(CRenderer::RB_CUTOUT), 0, 0);

				grcStateBlock::SetBlendState(grcStateBlock::BS_Add);
				pDrawable->Draw(mat, CRenderer::GenerateBucketMask(CRenderer::RB_CUTOUT), 0, 0);
			} else {
				Assertf(false, "No lod meshes in model: %s", pModelInfo->GetModelName());
			}
		}
		if (debugDrawTrunk) pDrawable->Draw(mat, CRenderer::GenerateBucketMask(CRenderer::RB_OPAQUE), 0, 0);

		grmShaderFx::SetForcedTechniqueGroupId(previousGroupId);

		//CShaderLib::SetEmissiveColorize(Vector4(1,1,1,1));
	}
};

inline void dlCmdStaticRenderImposterModel::Execute()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	StaticRenderImposterModel(m_pModelInfo, m_pass);
}




CRenderPhaseTreeImposters::CRenderPhaseTreeImposters(CViewport* pViewport)
: CRenderPhase(pViewport, "Tree Imposters", DL_RENDERPHASE_TREE, 0.0f, true)
{
	grcBlendStateDesc bsDesc;
	bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;	
	s_hDepthOnlyBlendState = grcStateBlock::CreateBlendState(bsDesc);
	Assert(s_hDepthOnlyBlendState != grcStateBlock::BS_Invalid);
}

void CRenderPhaseTreeImposters::UpdateViewport()
{
	CViewport *pVp = GetViewport();

	CTreeImposters::ms_viewport_fov = Max(pVp->GetGrcViewport().GetTanHFOV(), pVp->GetGrcViewport().GetTanVFOV());
}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseTreeImposters::BuildDrawList()											
{ 
	CImposterCacheInfo* entry=CTreeImposters::GetCacheEntryToRender();
	if (NULL == entry)
		return;

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(entry->m_modelIndex));
	if (NULL == pModelInfo->GetDrawable())
		return;

	DrawListPrologue();

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

	PF_FUNC(CRenderPhaseTreeImposters);

	static dev_float debugScale1=1.0f;
	static dev_float debugScale2=1.0f;

	float t_radius = (pModelInfo->GetBoundingBoxMax() - pModelInfo->GetBoundingBoxMin()).Mag()*0.5f*debugScale1;
	Vector3 t_centre = ((pModelInfo->GetBoundingBoxMax() + pModelInfo->GetBoundingBoxMin()) / 2.0f);

	entry->m_radius=t_radius;
	entry->m_centre=t_centre;

	grcViewport t_viewport;

	static dev_float debugZoom=0.5f;

	t_viewport.Ortho(-t_radius*debugZoom, t_radius*debugZoom, -t_radius*debugZoom, t_radius*debugZoom, 1.0f, 1.0f+t_radius*2.0f );
	t_viewport.SetWorldIdentity();

#if __PPU
	DLC( dlCmdLockRenderTarget, (0, CTreeImposters::GetPaletteTarget(), CTreeImposters::GetSharedDepthTarget() ));
#else
	DLC( dlCmdLockRenderTarget, (0, CTreeImposters::GetCacheRenderTarget(entry->m_index), CTreeImposters::GetSharedDepthTarget() ));
#endif

	Matrix34 t_camera_mat;
	t_camera_mat.LookAt(t_centre-Vector3(0,0,-1)*(t_radius+1.0f)*debugScale2, t_centre, Vector3(0,1,0));
	t_viewport.SetCameraMtx(RCC_MAT34V(t_camera_mat));

	DLC(dlCmdSetCurrentViewport, (&t_viewport));

	static dev_float cleardepth=1.0f;
	DLC( dlCmdClearRenderTarget, (true,Color32(0x00000000),true,cleardepth,true,0));

	DLC(dlCmdStaticRenderImposterModel, (pModelInfo, -1));

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DLC ( dlCmdUnLockRenderTarget, (0));

	t_viewport.Ortho(-t_radius, t_radius, -t_radius, t_radius, 1.0f, 1.0f+t_radius*2.0f );
	t_viewport.SetWorldIdentity();

	//lock render target
	DLC( dlCmdLockRenderTarget, (0, CTreeImposters::GetCacheRenderTarget(entry->m_index), CTreeImposters::GetSharedDepthTarget() ));

	for (int i=0;i<8;i++)
	{
		Matrix34 t_camera_mat;
		t_camera_mat.LookAt(t_centre-CTreeImposters::GetImposterViewDir(i)*(t_radius+1.0f)*debugScale2, t_centre, CTreeImposters::GetImposterViewUp(i));
		t_viewport.SetCameraMtx(RCC_MAT34V(t_camera_mat));

		DLC(dlCmdSetCurrentViewport, (&t_viewport));

		static dev_float cleardepth=1.0f;

		if (i==0)
			DLC( dlCmdClearRenderTarget, (true,Color32(0x00000000),true,cleardepth,true,0));
		else
			DLC( dlCmdClearRenderTarget, (false,Color32(0x00000000),true,cleardepth,true,0));

		DLC(dlCmdStaticRenderImposterModel, (pModelInfo, i));

		DLC(dlCmdSetCurrentViewportToNULL, ());
	}

	DLC(dlCmdRenderLeafColour, (entry->m_index));

	//auto resolve on 360
	DLC ( dlCmdUnLockRenderTarget, (0));

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DrawListEpilogue();
}

#endif // USE_TREE_IMPOSTERS