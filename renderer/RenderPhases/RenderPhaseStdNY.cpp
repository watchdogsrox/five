//
// filename:	RenderPhaseStdNY.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseStdNY.h"

// C headers
// Rage headers
#include "grcore/allocscope.h"
#include "grcore/debugdraw.h"
#include "grcore/quads.h"
#include "grcore/fastquad.h"
#include "profile\profiler.h"
#include "profile/timebars.h"
// Game headers
#include "camera/viewports/ViewportManager.h"
#include "camera/viewports/Viewport.h"
#include "control/replay/Replay.h"
#include "frontend/BusySpinner.h"
#include "FrontEnd/HudTools.h"
#include "FrontEnd/WarningScreen.h"
#include "Renderer\occlusion.h"
#include "renderer/PostProcessFX.h"
#include "renderer/Water.h"
#include "renderer\renderListGroup.h"
#include "renderer/ScreenshotManager.h"
#include "frontend/Credits.h"
#include "Frontend\MobilePhone.h"
#include "Frontend\PauseMenu.h"
#include "game\clock.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "Scene\scene.h"
#include "Script\script.h"
#include "Vfx\VisualEffects.h"
#if RSG_XENON
#include "grcore/wrapper_d3d.h"
#endif
#if __PPU
#include "Renderer/PostProcessFX.h"
#endif // __PPU
// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------
EXT_PF_GROUP(RenderPhase);

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

// ************************
// **** FRONTEND PHASE ****
// ************************

CRenderPhaseFrontEnd::CRenderPhaseFrontEnd(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "FrontEnd", DL_RENDERPHASE_FRONTEND, 0.0f, false)
{
}

void CRenderPhaseFrontEnd::UpdateViewport()
{
	CRenderPhase::UpdateViewport();

	// Update viewport
	CViewport *pVp = GetViewport();

	// Then make sure it is an ortho version.
	if (pVp->GetGrcViewport().IsPerspective())
	{
		grcWindow win = pVp->GetGrcViewport().GetUnclippedWindow();
		grcViewport t; // DW - this is done so that the constructor is called, it seems to be required to make the 2D rendering safe.
		GetGrcViewport() = t;
		GetGrcViewport().SetWindow(win.GetX(),win.GetY(),win.GetWidth(),win.GetHeight());
		GetGrcViewport().Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	}
}
//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhaseFrontEnd::BuildDrawList()											
{ 
	DrawListPrologue();

	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

	DLC_Add(CRenderTargets::LockUIRenderTargets);

	if (!CWarningScreen::IsActive() || CWarningScreen::AllowSpinner())  // dont render a standalone busy spinner when we have a warningscreen active - fixes 1570349, 1571137
	{
		if (CBusySpinner::CanRender())  // fix for 1673887 - only add the spinner DLC if the movie is still fully active at this stage
		{
			DLC_Add(CBusySpinner::Render);
		}
	}

	DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);

	PS3_ONLY(DLC_Add(CRenderTargets::Rescale);)

	DLC(dlCmdSetCurrentViewportToNULL, ());
	DrawListEpilogue();
}


// ***************************
// **** Phone Model PHASE ****
// ***************************

int CRenderPhasePhoneModel::ms_scissorRect[4];
int CRenderPhasePhoneModel::ms_prevScissorRect[4];

#if __BANK
bool CRenderPhasePhoneModel::ms_bDrawScissorRect = false;
#endif

//
// name:		CRenderPhaseRadar::CRenderPhasePhoneModel
// description:	Constructor for phone model render phase
CRenderPhasePhoneModel::CRenderPhasePhoneModel(CViewport* pGameViewport)
: CRenderPhase(pGameViewport, "Phone Model", DL_RENDERPHASE_PHONE, 0.0f, true)
{
}

#if __XENON
static D3DRECT gPhoneDrawRect;
#endif 

GRC_ALLOC_SCOPE_DECLARE_GLOBAL(static, s_CRenderPhasePhoneModel_AllocScope)

void CRenderPhasePhoneModel::PhoneAAPrologue(const Vector3& phonePos, float phoneScale, float phoneRadius)
{
	GRC_ALLOC_SCOPE_PUSH(s_CRenderPhasePhoneModel_AllocScope)

	// nothing is bound at this point, on next-gen at least
	//CRenderTargets::UnlockUIRenderTargets();

	Vector3 rotation;
	EulerAngleOrder rotationOrder;
	CPhoneMgr::GetRotation(rotation, rotationOrder);
	const float scale = phoneScale;
	Vector3 position = phonePos;

	// compute phone  min/max bounding rect
	grcViewport vp;
#if USE_INVERTED_PROJECTION_ONLY
	vp.SetInvertZInProjectionMatrix(true,false);
#endif // USE_INVERTED_PROJECTION_ONLY
	SetupPhoneViewport(&vp, position);
	Matrix34 M;
	CScriptEulers::MatrixFromEulers(M, Vector3(-DEG2RAD(90.0f), 0.0f, 0.0f), rotationOrder);
	M.Scale(scale, scale, scale);
	M.d = Vector3(0.0f, 0.0f, position.z);
	vp.SetWorldMtx(MATRIX34_TO_MAT34V(M));
	const float radius = phoneRadius;
	const Vector3 pTL = -Vector3(radius,0.0f,radius);
	const Vector3 pBR = Vector3(radius,0.0f,radius);
	float x0, y0, x1, y1;
	vp.Transform(VECTOR3_TO_VEC3V(pTL), x0, y0);
	vp.Transform(VECTOR3_TO_VEC3V(pBR), x1, y1);

	const int deviceWidth = VideoResManager::GetUIWidth();
	const int deviceHeight = VideoResManager::GetUIHeight();
	const float vpW = (float)vp.GetWidth();
	const float vpH = (float)vp.GetHeight();
	x0 = x0*(float)(deviceWidth)/vpW;
	y0 = y0*(float)(deviceHeight)/vpH;
	x1 = x1*(float)(deviceWidth)/vpW;
	y1 = y1*(float)(deviceHeight)/vpH;
 
	Vec2V vmin = Vec2V(Min(x0, x1), Min(y0, y1));
	Vec2V vmax = Vec2V(Max(x0, x1), Max(y0, y1));
	
#if __BANK
	if (ms_bDrawScissorRect)
	{
		Vec2V vdims = Vec2V(1.0f/(float)deviceWidth, 1.0f/(float)deviceHeight);
		grcDebugDraw::RectAxisAligned(vmin*vdims, vmax*vdims, Color32(0xffff0000), false);
	}
#endif

	ms_scissorRect[0] = Clamp((int)(vmin.GetXf()) - 1,0,deviceWidth-1);
	ms_scissorRect[1] = Clamp((int)(vmin.GetYf()) - 1,0,deviceHeight-1);
	ms_scissorRect[2] = Max(Clamp((int)(vmax.GetXf()-vmin.GetXf()) + 2,0,Min((int)deviceWidth - ms_scissorRect[0], deviceWidth)), 1);
	ms_scissorRect[3] = Max(Clamp((int)(vmax.GetYf()-vmin.GetYf()) + 2,0,Min((int)deviceHeight - ms_scissorRect[1], deviceHeight)), 1);

#if SUPPORT_MULTI_MONITOR
	// cut around the central monitor borders
	const GridMonitor& mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
	ms_scissorRect[0] = Max(ms_scissorRect[0], (int)mon.uLeft);
	ms_scissorRect[1] = Max(ms_scissorRect[1], (int)mon.uTop);
	ms_scissorRect[2] = Min(ms_scissorRect[2], Max(0, (int)mon.uRight - ms_scissorRect[0]));
	ms_scissorRect[3] = Min(ms_scissorRect[3], Max(0, (int)mon.uBottom - ms_scissorRect[1]));
#endif //SUPPORT_MULTI_MONITOR

	//ms_scissorRect[2] = Min(ms_scissorRect[2], ms_scissorRect[2]-ms_scissorRect[3]);
	//ms_scissorRect[3] = Min(ms_scissorRect[3], ms_scissorRect[2]-ms_scissorRect[1]));

#if __XENON
	// resolve a copy of whatever we've rendered so far, we're about to reuse
	// the edram region where this lives
	grcRenderTarget* pBBCopy = CRenderTargets::GetBackBufferCopy();
	D3DRECT	srcRect =	{	Max((((int)vmin.GetXf()-GPU_RESOLVE_ALIGNMENT-1)/GPU_RESOLVE_ALIGNMENT)*GPU_RESOLVE_ALIGNMENT, 0), 
							Max((((int)vmin.GetYf()-GPU_RESOLVE_ALIGNMENT-1)/GPU_RESOLVE_ALIGNMENT)*GPU_RESOLVE_ALIGNMENT, 0),
							Min((((int)vmax.GetXf()+GPU_RESOLVE_ALIGNMENT-1)/GPU_RESOLVE_ALIGNMENT)*GPU_RESOLVE_ALIGNMENT, deviceWidth), 
							Min((((int)vmax.GetYf()+GPU_RESOLVE_ALIGNMENT-1)/GPU_RESOLVE_ALIGNMENT)*GPU_RESOLVE_ALIGNMENT, deviceHeight) };
	srcRect.x2 = Max((int)srcRect.x2, 1);
	srcRect.y2 = Max((int)srcRect.y2, 1);
	D3DPOINT dstPoint = { srcRect.x1, srcRect.y1 };
	CHECK_HRESULT( GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, &srcRect, pBBCopy->GetTexturePtr(), &dstPoint, 0, 0, NULL, 0.0f, 0L, NULL ) );
	gPhoneDrawRect = srcRect;

	ms_scissorRect[0] = Min((int)srcRect.x1, deviceWidth-2);
	ms_scissorRect[1] = Min((int)srcRect.y1, deviceHeight-2);
	ms_scissorRect[2] = Max((int)(srcRect.x2-ms_scissorRect[0]), 2);
	ms_scissorRect[3] = Max((int)(srcRect.y2-ms_scissorRect[1]), 2);
#endif //__XENON


#if __D3D11 || RSG_ORBIS
	grcRenderTarget *colorRT = CRenderTargets::GetOffscreenBuffer2();
#else
	grcRenderTarget *colorRT = CRenderTargets::GetOffscreenBuffer1();
#endif
	grcRenderTarget *depthRT = CRenderTargets::GetUIDepthBuffer();


	grcTextureFactory::GetInstance().LockRenderTarget(0, colorRT, depthRT);


#if __XENON	
	// cache current scissor rect
	GRCDEVICE.GetScissor(ms_prevScissorRect[0], ms_prevScissorRect[1], ms_prevScissorRect[2], ms_prevScissorRect[3]);
	ms_prevScissorRect[2] = Min(ms_prevScissorRect[2], GRCDEVICE.GetWidth());
	ms_prevScissorRect[3] = Min(ms_prevScissorRect[3], GRCDEVICE.GetHeight());
#endif
//	GRCDEVICE.ClearRect(srcRect.x1, srcRect.y1, Max((int)(srcRect.x2-srcRect.x1), 1), Max((int)(srcRect.y2-srcRect.y1), 1), true, Color32(0x00000000), true, grcDepthFarthest, true, 0x0);
//	GRCDEVICE.SetScissor(srcRect.x1, srcRect.y1, Max((int)(srcRect.x2-srcRect.x1), 1), Max((int)(srcRect.y2-srcRect.y1), 1));
	GRCDEVICE.SetScissor(ms_scissorRect[0], ms_scissorRect[1], ms_scissorRect[2], ms_scissorRect[3]);
#if __D3D11 || RSG_ORBIS
	// use the fastest clear path for MSAA, relying on stencil for culling pixels out
	float depth = USE_INVERTED_PROJECTION_ONLY ? grcDepthClosest : grcDepthFarthest; 
	GRCDEVICE.Clear(true, Color32(0x00000000), true, depth, true, 0x0);
#else
	GRCDEVICE.ClearRect(ms_scissorRect[0], ms_scissorRect[1], ms_scissorRect[2], ms_scissorRect[3], true, Color32(0x00000000), true, grcDepthFarthest, true, 0x0);
#endif
}

void CRenderPhasePhoneModel::PhoneAAEpilogue()
{
#if __D3D11 || RSG_ORBIS
	grcRenderTarget *colorRT = CRenderTargets::GetOffscreenBuffer2();
#else
	grcRenderTarget *colorRT = CRenderTargets::GetOffscreenBuffer1();
#endif
	CSprite2d::CustomShaderType blitMode = CSprite2d::CS_BLIT_AAAlpha;

	grcResolveFlags flags;
#if __XENON
	flags.NeedResolve = false;
	D3DRECT srcRect = gPhoneDrawRect;
	D3DPOINT dstPoint = { srcRect.x1, srcRect.y1 };
	CHECK_HRESULT( GRCDEVICE.GetCurrent()->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, &srcRect, colorRT->GetTexturePtr(), &dstPoint, 0, 0, NULL, 0.0f, 0L, NULL ) );
#endif //__XENON

	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &flags);

#if DEVICE_MSAA
	if (GRCDEVICE.GetMSAA())
	{
#	if DEVICE_EQAA
		eqaa::ResolveArea area(ms_scissorRect[0], ms_scissorRect[1], ms_scissorRect[2], ms_scissorRect[3]);
#	endif
		CRenderTargets::ResolveOffscreenBuffer2();
		colorRT = CRenderTargets::GetOffscreenBuffer2_Resolved();
		blitMode = CSprite2d::CS_BLIT;	//it's already AA
	}
#endif // DEVICE_MSAA

	CRenderTargets::LockUIRenderTargets();
	GRCDEVICE.SetScissor(ms_scissorRect[0], ms_scissorRect[1], ms_scissorRect[2], ms_scissorRect[3]);
	float depth = USE_INVERTED_PROJECTION_ONLY ? grcDepthClosest : grcDepthFarthest; 
	GRCDEVICE.Clear(false, Color32(0x00000000), true, depth, true, 0);

#if __XENON
	// blit backbuffer back
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	CSprite2d copyTarget;
	const Vector2 TexSizeBB(1.0f/float(CRenderTargets::GetBackBufferCopy()->GetWidth()), 1.0f/float(CRenderTargets::GetBackBufferCopy()->GetHeight()));
	copyTarget.SetTexelSize(TexSizeBB);
	copyTarget.BeginCustomList(CSprite2d::CS_BLIT_NOFILTERING, CRenderTargets::GetBackBufferCopy());
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f+TexSizeBB.x,0.0f+TexSizeBB.y,1.0f+TexSizeBB.x,1.0f+TexSizeBB.y,Color32(255,255,255,255));
	copyTarget.EndCustomList();
#endif

	grcStateBlock::SetBlendState(grcStateBlock::BS_CompositeAlpha);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	const int deviceWidth = VideoResManager::GetUIWidth();
	const int deviceHeight = VideoResManager::GetUIHeight();
	// the AA pass is going to create some feathering along the edges, so tighten the scissor rect to avoid blitting those pixels back
	GRCDEVICE.SetScissor(Min(ms_scissorRect[0]+2, deviceWidth-1), Min(ms_scissorRect[1]+2, deviceHeight-1), Max(ms_scissorRect[2]-2, 1), Max(ms_scissorRect[3]-2, 1));

	CSprite2d UIAASprite;
	UIAASprite.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	const Vector2 TexSize(1.0f/float(colorRT->GetWidth()), 1.0f/float(colorRT->GetHeight()));
	UIAASprite.SetTexelSize(TexSize);
	UIAASprite.BeginCustomList(blitMode, colorRT);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
	UIAASprite.EndCustomList();

#if __XENON
	// Restore whatever was set before rendering the phone
	GRCDEVICE.SetScissor(ms_prevScissorRect[0], ms_prevScissorRect[1], ms_prevScissorRect[2], ms_prevScissorRect[3]);
#else
	GRCDEVICE.SetScissor(0, 0, colorRT->GetWidth(), colorRT->GetHeight());
#endif

	GRC_ALLOC_SCOPE_POP(s_CRenderPhasePhoneModel_AllocScope)
}

//----------------------------------------------------
// set ups viewport
void CRenderPhasePhoneModel::SetupPhoneViewport(grcViewport* vp, Vector3& phonePos)
{
	float aspect = CHudTools::GetAspectRatio();
#if SUPPORT_MULTI_MONITOR
	const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getCentralMonitor();
	if (GRCDEVICE.GetMonitorConfig().isMultihead() && !mon.bLandscape)
	{
		// not sure why, but this works
		aspect = mon.fPhysicalAspect;
	}
#endif //SUPPORT_MULTI_MONITOR
	vp->Perspective(70.0f, aspect, 0.01f, 400.0f);

	// Calculate shear X and Y to have the phone at the required position
	// but with a vanishing point positioned at the centre of the phone.
	float winX,winY;
	vp->Transform(RCC_VEC3V(phonePos),winX,winY);
	const float height = (float)vp->GetHeight();
	const float width = (float)vp->GetWidth();
	const float normX = winX/width;
	const float normY = winY/height;
	const float spaceX = -(normX*2.0f - 1.0f);
	const float spaceY = normY*2.0f - 1.0f;
	vp->SetPerspectiveShear(spaceX * CHudTools::GetAspectRatioMultiplier(), spaceY);
}

//----------------------------------------------------
// build and execute the drawlist for this renderphase
void CRenderPhasePhoneModel::BuildDrawList()											
{ 
#if __PS3
//	if (CHighQualityScreenshot::IsProcessingScreenshot())
	if (CPhotoManager::IsProcessingScreenshot())
		return;
#endif	//	__PS3

	if (CPauseMenu::IsActive())  // don't render mobile phone when frontend is active
		return;
	if (!GetViewport()->IsActive())
		return;
	if (CPhoneMgr::CamGetState())
		return;

	if (CPhoneMgr::IsDisplayed() == false)
		return;

	DrawListPrologue();

#if RSG_PC
	DLC_Add(CShaderLib::SetStereoParams,Vector4(0,0,1,0));
#endif
	DLC_Add(CShaderLib::SetUseFogRay, false);
	DLC_Add(CShaderLib::SetFogRayTexture);

	if (CPhoneMgr::UseAATechnique())
	{
		DLC_Add(PhoneAAPrologue, *CPhoneMgr::GetPosition(), CPhoneMgr::GetScale(), CPhoneMgr::GetRadius());

		CPhoneMgr::Render(CPhoneMgr::DRAWPHONE_BODY_ONLY);

		DLC_Add(PhoneAAEpilogue);

		CPhoneMgr::Render(CPhoneMgr::DRAWPHONE_SCREEN_ONLY);
	}
	else
	{
		DLC_Add(CRenderTargets::LockUIRenderTargets);

		float depth = USE_INVERTED_PROJECTION_ONLY ? grcDepthClosest : grcDepthFarthest; 
		DLC ( dlCmdClearRenderTarget, (false, Color32(0x00000000), true, depth, true, 0));

		CPhoneMgr::Render(CPhoneMgr::DRAWPHONE_ALL);
	}

	DLC(dlCmdSetCurrentViewportToNULL, ());

	DLC_Add(CRenderTargets::UnlockUIRenderTargets, false);

	DLC_Add(CShaderLib::SetUseFogRay, true);

#if RSG_PC
	DLC_Add(CShaderLib::SetStereoParams,Vector4(0,0,0,0));
#endif

	DrawListEpilogue();
}





