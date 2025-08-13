/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : DisplayCalibration.cpp
// PURPOSE : 
// AUTHOR  : Derek Payne
// STARTED : 09/11/2012
//
/////////////////////////////////////////////////////////////////////////////////

#include "Frontend/DisplayCalibration.h"

#include "grprofile/timebars.h"

#include "core/app.h"
#include "audio/frontendaudioentity.h"
#include "Frontend/MousePointer.h"
#include "Frontend/LoadingScreens.h"
#include "Frontend/PauseMenu.h"
#include "frontend/ProfileSettings.h"
#include "Frontend/ui_channel.h"
#include "frontend/scaleform/scaleformmgr.h"
#include "Frontend/WarningScreen.h"
#include "Frontend/NewHud.h"
#include "Renderer/PostProcessFX.h"
#include "Renderer/Util/ShaderUtil.h"
#include "Renderer/rendertargets.h"
#include "System/controlMgr.h"
#include "text/TextFile.h"
#include "Renderer/sprite2d.h"
#include "rline/rlpresence.h"
#include "system/service.h"

FRONTEND_OPTIMISATIONS()

namespace rage {
	XPARAM(unattended);
}

#if !__FINAL
PARAM(displaycalibration, "[code] force enable display calibration on startup");
PARAM(nodisplaycalibration, "[code] force disable display calibration on startup");
#endif

CScaleformMovieWrapper CDisplayCalibration::ms_MovieWrapper;
bool CDisplayCalibration::ms_bActive = false;
bool CDisplayCalibration::ms_ActivateAtStartup = true;
int CDisplayCalibration::ms_iPreviousValue = -1;

static rlPresence::Delegate g_DisplayCalibrationDlgt;

//PURPOSE: Return if display calibration should activate at startup
bool CDisplayCalibration::ShouldActivateAtStartup()
{
	return (
			( ms_ActivateAtStartup 
			|| !CProfileSettings::GetInstance().Exists(CProfileSettings::DISPLAY_GAMMA)
#if !__FINAL
			|| PARAM_displaycalibration.Get()
#endif
		)
#if !__FINAL
	&& !PARAM_nodisplaycalibration.Get() && !PARAM_unattended.Get()
#endif
	);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CDisplayCalibration::Render()
// PURPOSE: Renders the calibration screen
/////////////////////////////////////////////////////////////////////////////////////
void CDisplayCalibration::Render()
{
	if (IsActive() && ms_MovieWrapper.IsActive())  // ensure everything is set up how we need it 1st
	{
#define __DISPLAY_GREY_COLOUR (63)

		grcResolveFlags resFlags;
		resFlags.MipMap = false;
		resFlags.NeedResolve = true;

		#if __D3D11 || RSG_ORBIS
			grcRenderTarget* tempRT = CRenderTargets::GetOffscreenBuffer2();
		#else
			grcRenderTarget* tempRT = CRenderTargets::GetOffscreenBuffer1();
		#endif

		static bool bFullBlit = true;	//don't do alpha-blending with the BlitCalibration pass
		const Color32 GreyColor(__DISPLAY_GREY_COLOUR, __DISPLAY_GREY_COLOUR, __DISPLAY_GREY_COLOUR);

		CRenderTargets::UnlockUIRenderTargets();
		grcTextureFactory::GetInstance().LockRenderTarget(0, tempRT, NULL);
		GRCDEVICE.Clear(true, bFullBlit ? GreyColor : Color32(0,0,0,0), false, 0.0f, 0);
		ms_MovieWrapper.Render(true);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resFlags);

		#if DEVICE_MSAA
		CRenderTargets::ResolveOffscreenBuffer2();
		tempRT = CRenderTargets::GetOffscreenBuffer2_Resolved();
		#endif

		CRenderTargets::LockUIRenderTargets();
		if (bFullBlit) {
			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		}else {
			GRCDEVICE.Clear(true, GreyColor, false, 0, false, 0);
		}

		CSprite2d applyGamma;
		applyGamma.SetGeneralParams(Vector4(PostFX::g_gammaFrontEnd, 1.0f, 1.0f, 1.0f), Vector4(0, 0, 0, 0));

		applyGamma.BeginCustomList(CSprite2d::CS_BLIT_CALIBRATION, tempRT, 0);
		applyGamma.Draw(
			Vector2(-1.0f, 1.0f), Vector2(-1.0f, -1.0f), Vector2(1.0f, 1.0f), Vector2(1.0f, -1.0f),
			Vector2( 0.0f, 1.0f), Vector2( 0.0f,  0.0f), Vector2(1.0f, 1.0f), Vector2(1.0f,  0.0f),
			CRGBA(255,255,255,255));
		applyGamma.EndCustomList();

		if (CWarningScreen::CanRender())
		{
			CWarningScreen::Render();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CDisplayCalibration::UpdateInput
// PURPOSE:	Deals with processing of the calibration screen
/////////////////////////////////////////////////////////////////////////////////////
bool CDisplayCalibration::UpdateInput()
{
	if (!IsActive())
	{
		return false;
	}

#if !RSG_PC
	if (CControlMgr::GetPlayerPad() && !CControlMgr::GetPlayerPad()->IsConnected(PPU_ONLY(true)))
	{
		CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "NO_PAD", FE_WARNING_NONE);
		return false;
	}
#endif

	CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_DISPLAY_CALIBRATION, "DISP_CALIB_HDR", 
		CPauseMenu::sm_bWaitOnDisplayCalibrationScreen ? "DISP_CALIB" : "DISP_CALIB_SRT", 
		ms_iPreviousValue == -1 ? 
			static_cast<eWarningButtonFlags>(FE_WARNING_CONFIRM | FE_WARNING_ADJUST_STICKLEFTRIGHT) : 
			static_cast<eWarningButtonFlags>(FE_WARNING_CONFIRM | FE_WARNING_CANCEL | FE_WARNING_ADJUST_STICKLEFTRIGHT),
		false, -1, NULL, NULL, false);  // no background

	s32 iGammaValue = CPauseMenu::GetMenuPreference(PREF_GAMMA);

	eWarningButtonFlags result = CWarningScreen::CheckAllInput();
	if (result == FE_WARNING_CONFIRM)
	{
		CWarningScreen::Remove();
		return true;
	}
	else if(ms_iPreviousValue != -1 && 
		result == FE_WARNING_CANCEL )
	{
		CPauseMenu::SetMenuPreference(PREF_GAMMA, ms_iPreviousValue);
		CPauseMenu::SetValueBasedOnPreference(PREF_GAMMA, CPauseMenu::UPDATE_PREFS_FROM_MENU);
		CWarningScreen::Remove();
		return true;
	}

	else if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE) || 
			CMousePointer::IsSFMouseReleased(ms_MovieWrapper.GetMovieID(), LEFT_ARROW))
	{
		if (iGammaValue > 0)
		{
			iGammaValue--;

			CPauseMenu::SetMenuPreference(PREF_GAMMA, iGammaValue);
			CPauseMenu::SetValueBasedOnPreference(PREF_GAMMA, CPauseMenu::UPDATE_PREFS_FROM_MENU);

			g_FrontendAudioEntity.PlaySound("SLIDER_UP_DOWN", "HUD_FRONTEND_DEFAULT_SOUNDSET");

			//uiDisplayf("Adjusting Gamma %d", iGammaValue);
		}

		if(iGammaValue == 0)
		{
			ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", LEFT_ARROW, 60);
		}
		else
		{
			ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", LEFT_ARROW, 100);
			ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", RIGHT_ARROW, 100);
		}
	}

	else if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE) || 
			CMousePointer::IsSFMouseReleased(ms_MovieWrapper.GetMovieID(), RIGHT_ARROW))
	{
		if (iGammaValue < 30)
		{
			iGammaValue++;

			CPauseMenu::SetMenuPreference(PREF_GAMMA, iGammaValue);
			CPauseMenu::SetValueBasedOnPreference(PREF_GAMMA, CPauseMenu::UPDATE_PREFS_FROM_MENU);

			g_FrontendAudioEntity.PlaySound("SLIDER_UP_DOWN", "HUD_FRONTEND_DEFAULT_SOUNDSET");

			//uiDisplayf("Adjusting Gamma %d", iGammaValue);
		}

		if(iGammaValue == 30)
		{
			ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", RIGHT_ARROW, 20);
		}
		else
		{
			ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", LEFT_ARROW, 100);
			ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", RIGHT_ARROW, 100);
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CDisplayCalibration::Update
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CDisplayCalibration::Update()
{
	CDisplayCalibration::SetActive(true);

	bool bAcceptedBrightness = false;
	while (!bAcceptedBrightness && IsActive())
	{
		PF_FRAMEINIT_TIMEBARS(0);

#if RSG_PC
		CApp::CheckExit();
#endif

		sysIpcSleep(33);

		bAcceptedBrightness = CDisplayCalibration::UpdateInput();

		if (bAcceptedBrightness)
		{
			CWarningScreen::Remove();
			CDisplayCalibration::SetActive(false);
		}

		fwTimer::Update();

#if KEYBOARD_MOUSE_SUPPORT
		CMousePointer::ResetMouseInput();
		CMousePointer::Update();
		strStreamingEngine::GetLoader().LoadRequestedObjects();		// Pump the streaming system to issue new requests
		strStreamingEngine::GetLoader().ProcessAsyncFiles();
#endif

		CControlMgr::Update();
		g_SysService.UpdateClass();  // required to fix 1916535
		CWarningScreen::Update();
		CNetwork::Update(true);

#if RSG_PC && __D3D11
		gRenderThreadInterface.FlushAndDeviceCheck();
#endif // RSG_PC && __D3D11

		PF_FRAMEEND_TIMEBARS();
	}

	CDisplayCalibration::SetActive(false);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CDisplayCalibration::LoadCalibrationMovie
// PURPOSE:	streams in the movie
/////////////////////////////////////////////////////////////////////////////////////
void CDisplayCalibration::LoadCalibrationMovie(int iPreviousValue)
{
	CNewHud::EnableSafeZoneRender(0);

	if (!g_DisplayCalibrationDlgt.IsBound())
	{
		g_DisplayCalibrationDlgt.Bind(CDisplayCalibration::OnPresenceEvent);
		rlPresence::AddDelegate(&g_DisplayCalibrationDlgt);
	}

	ms_MovieWrapper.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, "PAUSE_MENU_CALIBRATION", Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
	ms_iPreviousValue = iPreviousValue;

	s32 iGammaValue = CPauseMenu::GetMenuPreference(PREF_GAMMA);
	if(iGammaValue == 0)
	{
		ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", 0, 60);
		ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", 1, 100);
	}
	else if(iGammaValue == 30)
	{
		ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", 1, 20);
		ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", 0, 100);
	}
	else
	{
		ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", 0, 100);
		ms_MovieWrapper.CallMethod("SET_ARROW_ALPHA", 1, 100);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CDisplayCalibration::RemoveCalibrationMovie
// PURPOSE:	Removes the movie
/////////////////////////////////////////////////////////////////////////////////////
void CDisplayCalibration::RemoveCalibrationMovie()
{
	if (ms_MovieWrapper.IsActive())
	{
		ms_MovieWrapper.RemoveMovie();
	}

	if (g_DisplayCalibrationDlgt.IsBound())
		g_DisplayCalibrationDlgt.Unbind();

	rlPresence::RemoveDelegate(&g_DisplayCalibrationDlgt);
}


void CDisplayCalibration::OnPresenceEvent(const rlPresenceEvent* evt)
{
#if !RSG_PC
	if (CPauseMenu::IsActive())  // only process this when pausemenu is active (we do not want to do this display calibration screen on boot up)
#endif
	{
		if (evt)
		{
			if (evt->GetId() == PRESENCE_EVENT_SIGNIN_STATUS_CHANGED)
			{
				const rlPresenceEventSigninStatusChanged* s = evt->m_SigninStatusChanged;

				if(s && ( s->SignedOffline() || s->SignedOut()))
				{
					SignInBail();
				}
				else if ( s && (s->SignedOnline() || s->SignedIn()))
				{
					SignInBail();
				}
			}
		}
	}
}

void CDisplayCalibration::SignInBail()
{
	if(ms_iPreviousValue != -1)
	{
		CPauseMenu::SetMenuPreference(PREF_GAMMA, ms_iPreviousValue);
	}

	CDisplayCalibration::RemoveCalibrationMovie();
	CDisplayCalibration::SetActive(false);
	CPauseMenu::sm_bWaitOnDisplayCalibrationScreen = false;

	CWarningScreen::Remove();
}
// eof
