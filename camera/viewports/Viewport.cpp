//
// name:		viewport.h
// description:	viewport class
//

#include "camera/viewports/Viewport.h"

// C++ hdrs
#include "stdio.h"

// Rage headers
#include "bank/bkmgr.h"
#include "grcore/allocscope.h"
#include "grcore/device.h"
#include "grcore/texturegcm.h"
#include "grcore/font.h"
#include "grcore/im.h"
#include "grcore/texture.h"
#include "grcore/viewport.h"
#include "input/headtracking.h"
#include "profile/timebars.h"
#include "system/param.h"
#include "System/system.h"
#include "vector/color32.h"

// Framework headers
#include "fwmaths/random.h"
#include "fwsys/timer.h"

// Game headers
#include "Cutscene/CutsceneManagerNew.h"
#include "Debug/Debug.h"
#include "Debug/TiledScreenCapture.h"
#include "Network/NetworkInterface.h"
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/scripted/ScriptDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "control/gamelogic.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "renderer/Mirrors.h"
#include "renderer/Occlusion.h"
#include "renderer/RenderTargetMgr.h"
#include "renderer/renderThread.h"
#include "renderer/rendertargets.h"
#include "renderer/shadows/shadows.h"
#include "renderer/RenderPhases/RenderPhaseLensDistortion.h"
#include "scene/portals/portal.h"
#include "scene/portals/portalTracker.h"
#include "scene/scene.h"
#include "script/script.h"
#include "script/script_hud.h"

CAMERA_OPTIMISATIONS()

//===========================
// VIEWPORT CLASS SUPPORT
//===========================

s32 CViewport::sm_IdViewports = 1; // an incrementing id for viewports.... changed to one so that zero can be invalid.

//---------------------------------------------
//
CViewport::CViewport()
: m_LastDeviceTimestamp(0)
{
#if __DEV
	m_threadId = sysIpcGetCurrentThreadId();
#endif

	m_bWasActiveLastFrame	= false;

	m_grcViewport.SetNormWindow(0.0f,0.0f,1.0f,1.0f);

	UpdateAspectRatio();  // sets the aspect ratio based on what mode we are in
	//m_grcViewport.Perspective(VIEWPORT_DEFAULT_FOVY,aspectRatio,VIEWPORT_DEFAULT_NEAR,VIEWPORT_DEFAULT_FAR);
	m_grcViewport.SetCameraIdentity();
	m_grcViewport.SetWorldIdentity();

	SetPriority(VIEWPORT_PRIORITY_1ST);
	SetActive(false);

	m_id = sm_IdViewports++; // forever incrementing id to ensure uniqueness

	m_bDrawWidescreenBorders = false;

	m_pInteriorProxy = NULL;
	m_roomIdx = 0;

	m_pAttachEntity = NULL;
}

//---------------------------------------------
//
CViewport::~CViewport()
{
	SetActive(false);
}

//---------------------------------------------
//
void CViewport::InitSession()
{
	static s32 gViewportNumInitSessions = 0;
	
	gViewportNumInitSessions++;

	SetAttachEntity(NULL);
}

//---------------------------------------------
//
void CViewport::ShutdownSession()
{
}

void CViewport::ResetWindow()
{
	m_grcViewport.ResetWindow();
}

void CViewport::UpdateAspectRatio()
{
	const GridMonitor &screen = GRCDEVICE.GetMonitorConfig().m_WholeScreen;
	float fAspectRatio = screen.fPhysicalAspect;
	float fDefaultFOV = VIEWPORT_DEFAULT_FOVY;
#if RSG_PC
	if (rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		fAspectRatio *= CRenderPhaseLensDistortion::GetAspectRatio();
		fDefaultFOV = CRenderPhaseLensDistortion::GetFOV();
	}
	// override input - always the default value in current code
	fAspectRatio *= GRCDEVICE.GetWidth()/(GRCDEVICE.GetHeight() * screen.getLogicalAspect());

#endif // RSG_PC
	if (m_grcViewport.IsPerspective())
		m_grcViewport.Perspective(fDefaultFOV,fAspectRatio,VIEWPORT_DEFAULT_NEAR,VIEWPORT_DEFAULT_FAR);
}

//---------------------------------------------
//
void CViewport::Process()
{
	ProcessWindow();
}

//---------------------------------------------
//
void CViewport::ProcessWindow()
{
//	m_grcViewport.RegenerateScreen();	// This deals with resolution changes (window resize). Without it the radar blips mess up.
//	m_grcViewport.SetCurrent();			// This deals with resolution changes (window resize). Without it the radar blips mess up.

	grcWindow win = m_grcViewport.GetUnclippedWindow();
	Vector2 pos((float)win.GetNormX(),(float)win.GetNormY());
	Vector2 size(win.GetNormWidth(),win.GetNormHeight());
	Vector2 origPos(pos);
	Vector2 origSize(size);

	if (GetLerp2dTopLeft()->IsActive())
	{
		GetLerp2dTopLeft()->Get(pos);
	}

	if (GetLerp2dBotRight()->IsActive())
	{
		Vector2 r;
		GetLerp2dBotRight()->Get(r);
		size = r-pos;
	}

	if (origPos != pos || size != origSize)
	{
		m_grcViewport.SetNormWindow(pos.x,pos.y,size.x,size.y);	
	}
}

//-----------------------------------------------------------
// Supplied with dest coords, this viewport will go here
void CViewport::LerpTo(Vector2 *pTopLeft, Vector2 *pBotRight,s32 time, CLerp2d::eStyles moveStyle)
{
	grcWindow win = m_grcViewport.GetUnclippedWindow();
	Vector2 tl(win.GetNormX(),win.GetNormY());
	Vector2 br(	tl.x+win.GetNormWidth(),tl.y+win.GetNormHeight());
	GetLerp2dTopLeft()->Set(tl,*pTopLeft,time,moveStyle);
	GetLerp2dBotRight()->Set(br,*pBotRight,time,moveStyle);
}

//-----------------------------------------------------
// Is a viewport clipped or onscreen or offscreen
// algorithm can be faster if required.
eViewportOnScreenCode CViewport::GetOnScreenStatus()
{
	grcWindow w = m_grcViewport.GetWindow();
	float x1 = w.GetNormX();
	float y1 = w.GetNormY();
	float x2 = x1 + w.GetNormWidth();
	float y2 = y1 + w.GetNormHeight();

	if (x1 >= 1.0f || x2 <= 0.0f || y1 >= 1.0f || y2 <= 0.0f)
		return VIEWPORT_ONSCREEN_CODE_WHOLLY_OFFSCREEN;

	if (x1 <= 1.0f && x2 <= 1.0f && x1 >= 0.0f && x2 >= 0.0f &&
		y1 <= 1.0f && y2 <= 1.0f && y1 >= 0.0f && y2 >= 0.0f)
		return VIEWPORT_ONSCREEN_CODE_WHOLLY_ONSCREEN;

	return VIEWPORT_ONSCREEN_CODE_PARTLY_ONSCREEN;
}

void CViewportPrimaryOrtho::InitSession()
{
	CViewport::InitSession();

	m_grcViewport.SetNormWindow(0.0f, 0.0f, 1.0f, 1.0f);

	m_grcViewport.Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	SetActive(true);
	SetPriority(CViewport::VIEWPORT_PRIORITY_LAST);

	bool bFadeOut = false;
	if (gbFirstTimeOnlyFade)
	{
		gbFirstTimeOnlyFade = false;
		bFadeOut = true;
	}

	if (bFadeOut)
	{
		camInterface::SetFixedFade(1.0f);
	}
	else
	{
		camInterface::FadeIn(VIEWPORT_LEVEL_INIT_FADE_TIME);//VIEWPORT_INTRO_FADE_WAIT);
	}
}

void CViewportPrimaryOrtho::ShutdownSession()
{
	CViewport::ShutdownSession();

	camInterface::FadeOut(VIEWPORT_LEVEL_EXIT_FADE_TIME,false);  

	SetFadeFirstTime();  // so we fade in initially again
}

//----------------------------------
//
void CViewportGame::InitSession()
{
	CViewport::InitSession();
	SetActive(true);
	SetPriority(CViewport::VIEWPORT_PRIORITY_1ST);
}

//----------------------------------
//
void CViewportFrontend3DScene::InitSession()
{
	// Initialize base class
	CViewport::InitSession();

	SetActive(true);
	SetPriority(CViewport::VIEWPORT_PRIORITY_LAST);
/*
#if __DEV
	switch(CPlayerSettingsMgr::sm_ViewportPriority)
	{
	case 0: SetPriority(CViewport::VIEWPORT_PRIORITY_1ST);  break;
	case 1: SetPriority(CViewport::VIEWPORT_PRIORITY_2ND);  break;
	case 2: SetPriority(CViewport::VIEWPORT_PRIORITY_3RD);  break;
	case 3: SetPriority(CViewport::VIEWPORT_PRIORITY_4TH);  break;
	case 4: SetPriority(CViewport::VIEWPORT_PRIORITY_5TH);  break;
	case 5: SetPriority(CViewport::VIEWPORT_PRIORITY_6TH);  break;
	case 6: SetPriority(CViewport::VIEWPORT_PRIORITY_7TH);  break;
	case 7: SetPriority(CViewport::VIEWPORT_PRIORITY_8TH);  break;
	case 8: SetPriority(CViewport::VIEWPORT_PRIORITY_9TH);  break;
	case 9: SetPriority(CViewport::VIEWPORT_PRIORITY_LAST); break;
	}	
#endif // __DEV
*/
	// Window Definition
	float fAspectRatio = GRCDEVICE.GetMonitorConfig().m_WholeScreen.fPhysicalAspect;
	float fDefaultFOV = VIEWPORT_DEFAULT_FOVY;
#if RSG_PC
	if (rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		fAspectRatio *= CRenderPhaseLensDistortion::GetAspectRatio();
		fDefaultFOV = CRenderPhaseLensDistortion::GetFOV();
	}
#endif // RSG_PC
	m_grcViewport.Perspective(fDefaultFOV, fAspectRatio, VIEWPORT_DEFAULT_NEAR, VIEWPORT_DEFAULT_FAR);
}

void CViewport::PropagateCameraFrame(const camFrame& frame)
{
#if SUPPORT_MULTI_MONITOR
	const u32 uDeviceTimestamp = GRCDEVICE.GetMonitorConfig().m_ChangeTimestamp;
	if (m_LastDeviceTimestamp!= uDeviceTimestamp)
	{
		m_LastDeviceTimestamp = uDeviceTimestamp;
		fwRect rect = GRCDEVICE.GetMonitorConfig().getCentralMonitor().getArea();
		float cx=0.f, cy=0.f;
		rect.GetCentre(cx,cy);
		m_grcViewport.SetPerspectiveShear( 1.f-2.f*cx,  2.f*cy-1.f );
	}
#endif	//SUPPORT_MULTI_MONITOR

#if __BANK
	TiledScreenCapture::UpdateViewportFrame(m_grcViewport);
#endif // __BANK

	m_Frame.CloneFrom(frame);

	//Convert to RAGE's coordinate system for the projection matrix.
	Matrix34 rageWorldMatrix;
	frame.ComputeRageWorldMatrix(rageWorldMatrix);

	grcViewport& grcVP = GetNonConstGrcViewport();

	//***** Propagate the (RAGE) world matrix to the RAGE viewport *****
	grcVP.SetCameraMtx(RCC_MAT34V(rageWorldMatrix));

	//Aspect ratio is maintained to be good for the window size.
	float aspectRatio = GRCDEVICE.GetMonitorConfig().m_WholeScreen.fPhysicalAspect;
	Assertf((aspectRatio >= 0.0f), "The window size for a viewport looks very small");
	float fFOV = frame.GetFov();
	if (rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		aspectRatio *= CRenderPhaseLensDistortion::GetAspectRatio();
		fFOV = CRenderPhaseLensDistortion::GetFOV();
	}

	//***** Propagate the frustum values to the RAGE viewport *****
#if __BANK
	if (TiledScreenCapture::UpdateViewportPerspective(grcVP, fFOV, aspectRatio, frame.GetNearClip(), frame.GetFarClip()))
#endif // __BANK
	{
		grcVP.Perspective(frame.GetFov(), aspectRatio, frame.GetNearClip(), frame.GetFarClip());

		//NOTE: The portal tracker is concerned only with translational discontinuities.
		if(frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || frame.GetFlags().IsFlagSet(camFrame::Flag_RequestPortalRescan))
		{
			ForcePortalTrackerRescan(rageWorldMatrix.d);
		}
	}

#if __DEV
	if(frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) && frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
	{
		cameraDebugf1("Cut position and orientation");
	}
	else if(frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition))
	{
		cameraDebugf1("Cut position");
	}
	else if(frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation))
	{
		cameraDebugf1("Cut orientation");
	}
#endif // __DEV
}

void CViewport::PropagateRevisedCameraFarClip(float farClip)
{
	m_Frame.SetFarClip(farClip);

	grcViewport& grcVP = GetNonConstGrcViewport();

	//Aspect ratio is maintained to be good for the window size.
	float aspectRatio = GRCDEVICE.GetMonitorConfig().m_WholeScreen.fPhysicalAspect;
	Assertf((aspectRatio >= 0.0f), "The window size for a viewport looks very small");
	float fFOV = m_Frame.GetFov();
	if (rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		aspectRatio *= CRenderPhaseLensDistortion::GetAspectRatio();
		fFOV = CRenderPhaseLensDistortion::GetFOV();
	}

	//***** Propagate the revised frustum values to the RAGE viewport *****
#if __BANK
	if (TiledScreenCapture::UpdateViewportPerspective(grcVP, fFOV, aspectRatio, m_Frame.GetNearClip(), farClip))
#endif // __BANK
	{
		grcVP.Perspective(fFOV, aspectRatio, m_Frame.GetNearClip(), farClip);
	}
}

void CViewport::ForcePortalTrackerRescan(const Vector3& cameraPosition)
{
	s32 numRenderPhases = RENDERPHASEMGR.GetRenderPhaseCount();
	for(s32 i=0; i<numRenderPhases; i++)
	{
		CRenderPhase& renderPhase = (CRenderPhase&)RENDERPHASEMGR.GetRenderPhase(i);
		if(renderPhase.IsScanRenderPhase())
		{
			CRenderPhaseScanned& scanPhase = (CRenderPhaseScanned&)renderPhase;
			if(scanPhase.IsUpdatePortalVisTrackerPhase())
			{
				CPortalTracker* portalTracker = scanPhase.GetPortalVisTracker();
				if(portalTracker)
				{
					portalTracker->RequestRescanNextUpdate();
					portalTracker->Update(cameraPosition);

					if(portalTracker->GetInteriorInst())
					{
						portalTracker->GetInteriorInst()->ForceFadeIn();
					}
				}
			}
		}
	}
}

//Draws a box of a colour on the viewport that is active.
void CViewport::DrawBoxOnCurrentViewport(Color32 colour)
{
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "DrawBoxOnCurrentViewport() cannot be called from outside the renderthread");// this command is forbidden outside the render thread

	PF_PUSH_TIMEBAR("Draw Box On Viewport");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	CSprite2d::DrawRect(fwRect(0.0f, 0.0f, 1.0f, 1.0f), colour);

	PF_POP_TIMEBAR();
}

//-----------------------------------------------------------
void CLerp2d::Init()
{
	m_bIsActive = false;
	m_timeStart = -1;
	m_duration  = -1;
	m_style		= BAD;
	m_from.Zero();
	m_to.Zero();
}

//-----------------------------------------------------------
void CLerp2d::Set(const Vector2& start, const Vector2& end, s32 duration, eStyles type)
{
	m_bIsActive	= true;
	m_timeStart = fwTimer::GetNonPausableCamTimeInMilliseconds();
	m_duration  = duration;
	m_style		= type;
	m_from		= start;
	m_to		= end;
}

//-----------------------------------------------------------
void CLerp2d::Get(Vector2& res)
{
	s32 time = fwTimer::GetNonPausableCamTimeInMilliseconds();
	s32 deltaT = time-m_timeStart;
	float t = (float)deltaT/(float)m_duration;

	if (t>1.0f)
	{
		m_bIsActive = false;
		t = 1.0f;
	}
	else if (t<0.0f)
		t = 0.0f;

	switch (m_style)
	{
	case SMOOTHED:
		t = SlowInOut(t);
		res.Lerp(t, m_from, m_to);
		break;

	case LINEAR:
		res.Lerp(t, m_from, m_to);
		break;

	default : 
		Assert(false);
		break;
	}
}
