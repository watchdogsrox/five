//
// filename:	RenderPhaseEntitySelect.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseEntitySelect.h"

// RAGE headers
#include "profile/timebars.h"

// Framework headers
#include "fwscene/scan/RenderPhaseCullShape.h"

// Game headers
#include "debug/GtaPicker.h"
#include "debug/TextureViewer/TextureViewer.h"
#include "renderer/Debug/EntitySelect.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "renderer/renderListGroup.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/rendertargets.h"
#include "renderer/Water.h"
#include "renderer/River.h"
#include "renderer/Mirrors.h"
#include "shaders/ShaderLib.h"
#include "vfx/VisualEffects.h"
#include "game/Clock.h"

//For UpdateViewport
#include "camera/viewports/Viewport.h"
#include "scene/portals/PortalTracker.h"


#if ENTITYSELECT_ENABLED_BUILD
// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals ----------------------------------------------------------
bool CRenderPhaseEntitySelect::sm_inBuildDrawList = false;

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

//--------------------------------------------------------------------


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// name:		CRenderPhaseEntitySelect::CRenderPhaseEntitySelect
// description:	Constructor for Entity Select render phase
CRenderPhaseEntitySelect::CRenderPhaseEntitySelect(CViewport *pGameViewport)
: CRenderPhaseScanned(pGameViewport, "Entity Selector", DL_RENDERPHASE_DEBUG, 0.0f)
{
	//m_RenderSettings = 0;
	SetEntityListIndex(gRenderListGroup.RegisterRenderPhase( RPTYPE_DEBUG, this ));

	SetRenderFlags(
		RENDER_SETTING_RENDER_WATER |
		RENDER_SETTING_RENDER_ALPHA_POLYS |
		RENDER_SETTING_RENDER_DECAL_POLYS |
		RENDER_SETTING_RENDER_CUTOUT_POLYS );

#if __D3D11 || RSG_ORBIS
	m_wasActiveLastFrame = false;
#endif // __D3D11 || RSG_ORBIS
}

CRenderPhaseEntitySelect::~CRenderPhaseEntitySelect()
{
	// Move this over once we shut down the original one
	ReleaseRenderingResources();
}

void CRenderPhaseEntitySelect::UpdateViewport()
{
	CRenderPhaseScanned::UpdateViewport();

	bool bActive = false;

	// any system which requires entity select must be queried here
	if (CGtaPickerInterface                ::IsEntitySelectRequired() ||
		CDebugTextureViewerInterface       ::IsEntitySelectRequired() ||
		CRenderPhaseDebugOverlayInterface  ::IsEntitySelectRequired() ||
		CRenderPhaseCascadeShadowsInterface::IsEntitySelectRequired())
	{
		if (HasEntityListIndex() && CEntitySelect::IsEntitySelectPassEnabled())
		{
			bActive = true;
		}
	}

	SetActive(bActive);

	if (!bActive)
		return;

	CViewport *pVp = GetViewport();

	const Vector3& cameraPosition = pVp->GetFrame().GetPosition();

	if (m_pPortalVisTracker)
	{
		if (pVp->GetAttachEntity())
			m_pPortalVisTracker->UpdateUsingTarget(pVp->GetAttachEntity(), cameraPosition);
		else
			m_pPortalVisTracker->Update(cameraPosition);
	}
}

void CRenderPhaseEntitySelect::BuildRenderList()
{
	if (!IsActive())// || !HasEntityListIndex() || !CEntitySelect::IsEntitySelectPassEnabled())
		return;

	CRenderer::ConstructRenderListNew(GetRenderFlags(), this);
	CRenderPhaseScanned::BuildRenderList();
}

void CRenderPhaseEntitySelect::BuildDrawList() 
{ 
	if (!IsActive())// || !HasEntityListIndex() || !CEntitySelect::IsEntitySelectPassEnabled())
	{
#if __D3D11 || RSG_ORBIS
		m_wasActiveLastFrame = false;
#endif
		return;
	}

	DrawListPrologue();

	sm_inBuildDrawList = true;

	DLCPushTimebar("Setup");
	//Update EntitySelect vars that will later be used in drawing. (Should not be delayed drawlist call)
	CEntitySelect::Update();

#if USE_INVERTED_PROJECTION_ONLY
	grcViewport viewport = GetGrcViewport();
	viewport.SetInvertZInProjectionMatrix( true );
#else
	const grcViewport& viewport = GetGrcViewport();
#endif

	DLC(dlCmdSetCurrentViewport, (&viewport));

	//Update some shader variables. (Some may not be necessary and can be removed.)
	DLC_Add(CShaderLib::UpdateGlobalGameScreenSize);
	DLC_Add(CShaderLib::ApplyDayNightBalance);

#if __D3D11 || RSG_ORBIS
	if(!m_wasActiveLastFrame)
		DLC_Add(CEntitySelect::ClearResolveTargets);
	m_wasActiveLastFrame = true;
#endif // __D3D11 || RSG_ORBIS

	//Allow main entity select class to handle the necessary setup for the render pass.
	// note - actually the render states will be overridden by dlCmdClearRenderTarget below, so i'll set them again ..
	DLC_Add(CEntitySelect::PrepareGeometryPass);

	DLCPopTimebar(); //Setup


	DLCPushTimebar("Render");
	{
		const bool clearDepth = true;
		const bool clearStencil = true;
		const float clearDepthCol = USE_INVERTED_PROJECTION_ONLY ? 0.0f : 1.0f;
		const Color32 clearColor((u32)CEntityIDHelper::GetInvalidEntityId()); // This is ok as all planes will be the same colour.
		DLC(dlCmdClearRenderTarget, (true, clearColor, clearDepth, clearDepthCol, clearStencil, DEFERRED_MATERIAL_CLEAR));

		DLC_Add(CEntitySelect::SetRenderStates);

		if (CEntitySelect::IsUsingShaderIndexExtension())
		{
			DLC_Add(grmModel::SetCustomShaderParamsFunc, (grmModel::CustomShaderParamsFuncType)CEntitySelect::BindEntityIDShaderParamFunc);
		}

#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
		CRenderer::SetTessellationVars(viewport, (float)VideoResManager::GetSceneWidth()/(float)VideoResManager::GetSceneHeight());
#endif // RAGE_SUPPORT_TESSELLATION_TECHNIQUES

		//Draw opaque stuff
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseEntitySelect");
		CRenderListBuilder::AddToDrawList(GetEntityListIndex(), 
			CRenderListBuilder::RENDER_OPAQUE|
			CRenderListBuilder::RENDER_ALPHA|
			CRenderListBuilder::RENDER_FADING|
			CRenderListBuilder::RENDER_WATER|
			CRenderListBuilder::RENDER_DISPL_ALPHA|
			CRenderListBuilder::RENDER_DONT_RENDER_PLANTS,
			CRenderListBuilder::USE_DEBUGMODE);
		RENDERLIST_DUMP_PASS_INFO_END();

		if (CEntitySelect::IsUsingShaderIndexExtension())
		{
			DLC_Add(grmModel::SetCustomShaderParamsFunc, (grmModel::CustomShaderParamsFuncType)NULL);
		}
	}
	DLCPopTimebar(); //Render


	DLCPushTimebar("Finalize");
	{
		//Allow main entity select class to handle the necessary post-draw steps necessary for the render pass.
		DLC_Add(CEntitySelect::FinishGeometryPass);

#if __XENON
		DLC(dlCmdSetShaderGPRAlloc, (0, 0));
#endif

		DLC(dlCmdSetCurrentViewportToNULL, ());
	}
	DLCPopTimebar(); //Finalize

	sm_inBuildDrawList = false;

	DrawListEpilogue();
}

u32 CRenderPhaseEntitySelect::GetCullFlags() const
{
	return 	CULL_SHAPE_FLAG_RENDER_EXTERIOR				|
		CULL_SHAPE_FLAG_RENDER_INTERIOR				|
		CULL_SHAPE_FLAG_ENABLE_OCCLUSION;
}

void CRenderPhaseEntitySelect::InitializeRenderingResources()
{
	CEntitySelect::Init(
#	if __PS3
		// Xenon use edram, the others want to ignore any MSAA settings, so only share on PS3
		GBuffer::GetTarget(GBUFFER_RT_0), GBuffer::GetDepthTarget()
#	endif
		);
}

void CRenderPhaseEntitySelect::ReleaseRenderingResources()
{
	CEntitySelect::ShutDown();
}

void CRenderPhaseEntitySelect::BuildDebugDrawList()
{
	DLC_Add(CEntitySelect::DrawDebug);
}

#if __BANK
void CRenderPhaseEntitySelect::AddWidgets(bkGroup & group)
{
	CEntitySelect::AddWidgets(group);
}
#endif // __BANK


#endif //ENTITYSELECT_ENABLED_BUILD
