/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HudTools.cpp
// PURPOSE : common hud related tools (code pulled from NewHud)
// AUTHOR  : James Chagaris
// STARTED : 1/23/2013
//
/////////////////////////////////////////////////////////////////////////////////

//game
#include "Frontend/HudTools.h"

#include "frontend/ProfileSettings.h"
#include "Camera/Base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "Camera/Viewports/ViewportManager.h"
#include "Frontend/NewHud.h"
#include "Renderer/Rendertargets.h"
#include "renderer/RenderPhases/RenderPhaseLensDistortion.h"

#include "input/headtracking.h"

//OPTIMISATIONS_OFF()
FRONTEND_OPTIMISATIONS()

#define ASPECT_RATIO_4_3 (4.0f/3.0f)
#define ASPECT_RATIO_16_9 (16.0f/9.0f)

#if __BANK
bool CHudTools::bDisplayHud = true;
bool CHudTools::bForceWidescreenStretch = false;
#endif
bool CHudTools::bJustRestarted = false;

grcBlendStateHandle CHudTools::ms_StandardSpriteBlendStateHandle = grcStateBlock::BS_Invalid;
static const strStreamingObjectName	FRONTEND_TXD_PATH("platform:/textures/frontend",0xE703E6C4);

//PURPOSE: Return safe zone size, calculated from frontend preference
float CHudTools::GetSafeZoneSize()
{
#if RSG_ORBIS
	return CPauseMenu::GetOrbisSafeZone();
#else
//	int size = CProfileSettings::IsInstantiated() ? CProfileSettings::GetInstance().GetInt(CProfileSettings::DISPLAY_SAFEZONE_SIZE) : 0; // return max safety if uninitialized
	int size = SAFEZONE_SLIDER_MAX - CPauseMenu::GetMenuPreference(PREF_SAFEZONE_SIZE);
#if RSG_PC
	float fSafezone = (float)(100-size)*0.01f;
	return fSafezone;
#else
	return 0.95f - ((float)size) * 0.01f;
#endif // RSG_PC
#endif
}

// the minimum Safe zone for all supported platforms
// NOTE: PS3 uses 85, Xbox 90. Artists want to see both for tuning purposes...
// copied from grcsetup.cpp - it would be nice if that was made available to me instead of having my own version
void CHudTools::GetMinSafeZone(float &x0, float &y0, float &x1, float &y1, float aspectRatio, bool bScript)
{
	// profile setting is 0-15
	float safezoneSizeX = GetSafeZoneSize();
	float safezoneSizeY = GetSafeZoneSize();

	if(aspectRatio < 1.0f)
	{
		safezoneSizeX = 1.0f - ((1.0f - safezoneSizeX) + (1.0f - aspectRatio));
	}

	float width		= (float)VideoResManager::GetUIWidth();
	float height	= (float)VideoResManager::GetUIHeight();

	float safeW = width * safezoneSizeX;
	float safeH = height * safezoneSizeY;  
	float offsetW = (width - safeW) * 0.5f;
	float offsetH = (height - safeH) * 0.5f;

	// Round down to lowest area
	x0 = ceilf(offsetW);
	y0 = ceilf(offsetH);
	x1 = floorf(width - offsetW);
	y1 = floorf(height - offsetH);

	x0 /= width;
	y0 /= height;
	x1 /= width;
	y1 /= height;

	if(bScript && IsSuperWideScreen())
	{
		float fDifference = ASPECT_RATIO_16_9 / GetAspectRatio();
		float fMaxBounds = width * fDifference;
		float fResDif = width - fMaxBounds;
		float fOffsetAbsolute = fResDif * 0.5f;
		float fOffsetRelative = fOffsetAbsolute / width;

		x0 += fOffsetRelative;
		x1 -= fOffsetRelative;
	}
}

#if RSG_DURANGO
void CHudTools::GetFakeLoadingScreenSafeZone(float &x0, float &y0, float &x1, float &y1)
{
	const float FAKE_SAFE_ZONE_PCT = .075f;

	x0 = FAKE_SAFE_ZONE_PCT;
	y0 = FAKE_SAFE_ZONE_PCT;
	x1 = 1 - FAKE_SAFE_ZONE_PCT;
	y1 = 1 - FAKE_SAFE_ZONE_PCT;
}
#endif

float CHudTools::GetDifferenceFrom_16_9_ToCurrentAspectRatio()
{
	float fOffsetValue = 0.0f;

	if(!IsSuperWideScreen())
	{
		const float fAspectRatio = GetUIWidth() / GetUIHeight();
		const float fMarginRatio = (1.0f - GetSafeZoneSize()) * 0.5f;
		const float fDifferenceInMarginSize = fMarginRatio * (ASPECT_RATIO_16_9 - fAspectRatio);
		fOffsetValue = (1.0f - (fAspectRatio/ASPECT_RATIO_16_9) - fDifferenceInMarginSize) * 0.5f;
	}

	return fOffsetValue;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHudTools::GetMinSafeZoneForScaleformMovies()
// PURPOSE: same as GetMinSafeZone but in addition adjusts safe zone values so that
//			they match the real render space of the scaleform movie
/////////////////////////////////////////////////////////////////////////////////////
void CHudTools::GetMinSafeZoneForScaleformMovies(float &x0, float &y0, float &x1, float &y1, float aspectRatio)
{
	GetMinSafeZone(x0, y0, x1, y1, aspectRatio);

	float fSafeZoneAdjust = CHudTools::GetDifferenceFrom_16_9_ToCurrentAspectRatio();

	x0 += fSafeZoneAdjust;
	x1 -= fSafeZoneAdjust;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHudTools::BeginHudScaleformMethod()
// PURPOSE: wrapper for the hud proxy method call to scaleform
/////////////////////////////////////////////////////////////////////////////////////
bool CHudTools::BeginHudScaleformMethod(eNewHudComponents eHudComponentId, const char *pMethodName)
{
	if (CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "CONTAINER_METHOD", CNewHud::GetHudComponentMovieId(eHudComponentId)))
	{
		CScaleformMgr::AddParamInt(eHudComponentId);
		CScaleformMgr::AddParamString(pMethodName);

		CNewHud::AllowRestartHud();  // we have invoked the hud so we will want to restart at next opportunity

		return true;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHudTools::CallHudScaleformMethod()
// PURPOSE: wrapper for the hud proxy method call to scaleform
/////////////////////////////////////////////////////////////////////////////////////
void CHudTools::CallHudScaleformMethod(eNewHudComponents eHudComponentId, const char *pMethodName, bool bInstantly)
{
	if (CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "CONTAINER_METHOD", CNewHud::GetHudComponentMovieId(eHudComponentId)))
	{
		CScaleformMgr::AddParamInt(eHudComponentId);
		CScaleformMgr::AddParamString(pMethodName);

		CScaleformMgr::EndMethod(bInstantly);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHudTools::GetHudPosFromWorldPos()
// PURPOSE: converts world position to a screen position based on the HUD
/////////////////////////////////////////////////////////////////////////////////////
Vector2 CHudTools::GetHudPosFromWorldPos(Vector3::Vector3Param vWorldPositon, eTEXT_ARROW_ORIENTATION *pArrowDir)
{
	Vector3 vWorldPositionV(vWorldPositon);
	Vector2 vNewScreenPos;

	bool bVisible = (gVpMan.GetCurrentViewport()->GetGrcViewport().IsSphereVisible(vWorldPositionV.x, vWorldPositionV.y, vWorldPositionV.z, 0.01f) != 0);

	gVpMan.GetCurrentViewport()->GetGrcViewport().Transform((Vec3V&)vWorldPositon, vNewScreenPos.x, vNewScreenPos.y);

	// convert to 0-1 space:
	vNewScreenPos.x /= SCREEN_WIDTH;
	vNewScreenPos.y /= SCREEN_HEIGHT;

	if (bVisible)  // if visible, then set the new position, locking to edge of screen
	{
		if (vNewScreenPos.x > 1.0f-OFFSET_FROM_EDGE_OF_SCREEN)
			vNewScreenPos.x = 1.0f-OFFSET_FROM_EDGE_OF_SCREEN;

		if (vNewScreenPos.x < OFFSET_FROM_EDGE_OF_SCREEN)
			vNewScreenPos.x = OFFSET_FROM_EDGE_OF_SCREEN;

		if (vNewScreenPos.y > 1.0f-OFFSET_FROM_EDGE_OF_SCREEN)
			vNewScreenPos.y = 1.0f-OFFSET_FROM_EDGE_OF_SCREEN;

		if (vNewScreenPos.y < OFFSET_FROM_EDGE_OF_SCREEN)
			vNewScreenPos.y = OFFSET_FROM_EDGE_OF_SCREEN;

		if (pArrowDir)
			*pArrowDir = HELP_TEXT_ARROW_NORMAL;
	}
	else
	{
		if (pArrowDir)
			*pArrowDir = HELP_TEXT_ARROW_NORMAL;

		//
		// work out the angle between the world point and the cam position taking into account the cam rotation:
		//
		Vector3 vCamRot(0,0,0);
		Vector3 vCamPos(0,0,0);

		const camBaseCamera *pCam = camInterface::GetDominantRenderedCamera();

		if (pCam)
		{	
			vCamPos = pCam->GetFrame().GetPosition();
			const Matrix34& worldMatrix = pCam->GetFrame().GetWorldMatrix();
			worldMatrix.ToEulersYXZ(vCamRot);

			Vector3 vecFirst = Vector3(Vector3(vCamPos.x, vCamPos.y, 0.0f));
			Vector3 vecSecond = Vector3(Vector3(vWorldPositionV.x, vWorldPositionV.y, 0.0f));

			Vector3 vDiff = vecFirst - vecSecond;

			float fDist = vDiff.Mag();
			float fHeight = vWorldPositionV.z - vCamPos.z;

			float fPitchToWorldPos;
			float fHeadingToWorldPos;

			if (fDist > 0)
				fPitchToWorldPos = rage::Atan2f(fHeight/fDist,1);
			else
				fPitchToWorldPos = 0;

			float dX = vWorldPositionV.x - vCamPos.x;
			float dY = vWorldPositionV.y - vCamPos.y;
			if (dY != 0)
			{
				fHeadingToWorldPos = rage::Atan2f(dX, dY);
			}
			else
			{
				if (dX < 0)
					fHeadingToWorldPos = -(90.0f * DtoR);
				else
					fHeadingToWorldPos = (90.0f * DtoR);
			}

			if (fHeadingToWorldPos < 0.0f)
				fHeadingToWorldPos += (360.0f * DtoR);

			float fAngleToToWorldPos = (rage::Atan2f(rage::Cosf(vCamRot.x)*rage::Sinf(fPitchToWorldPos)-rage::Sinf(vCamRot.x)*rage::Cosf(fPitchToWorldPos)*rage::Cosf((fHeadingToWorldPos*-1)-vCamRot.z), rage::Sinf((fHeadingToWorldPos*-1)-vCamRot.z)*rage::Cosf(fPitchToWorldPos))) * RtoD;

			fAngleToToWorldPos -= 45.0f;  // so 0 is the top middle of the screen

			// ensure we have an angle between 0 and 360 degrees:
			while (fAngleToToWorldPos < 0.0f)
				fAngleToToWorldPos += 360.0f;

			while (fAngleToToWorldPos > 360.0f)
				fAngleToToWorldPos -= 360.0f;

			if (fAngleToToWorldPos >= 0.0f && fAngleToToWorldPos <= 90.0f)
			{
				vNewScreenPos.x = (fAngleToToWorldPos / 90.0f);
				vNewScreenPos.y = OFFSET_FROM_EDGE_OF_SCREEN;

				vNewScreenPos.x = Min(1.0f-OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.x);
				vNewScreenPos.x = Max(OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.x);

				if (pArrowDir)
					*pArrowDir = HELP_TEXT_ARROW_NORTH;
			}
			else
			if (fAngleToToWorldPos > 90.0f && fAngleToToWorldPos <= 180.0f)
			{
				fAngleToToWorldPos -= 90.0f;

				vNewScreenPos.x = 1.0f-OFFSET_FROM_EDGE_OF_SCREEN;
				vNewScreenPos.y = (fAngleToToWorldPos / 90.0f);

				vNewScreenPos.y = Min(1.0f-OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.y);
				vNewScreenPos.y = Max(OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.y);

				if (pArrowDir)
					*pArrowDir = HELP_TEXT_ARROW_EAST;
			}
			else
			if (fAngleToToWorldPos > 180.0f && fAngleToToWorldPos <= 270.0f)
			{
				fAngleToToWorldPos -= 180.0f;

				vNewScreenPos.x = 1.0f-(fAngleToToWorldPos/ 90.0f);
				vNewScreenPos.y = 1.0f-OFFSET_FROM_EDGE_OF_SCREEN;

				vNewScreenPos.x = Min(1.0f-OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.x);
				vNewScreenPos.x = Max(OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.x);

				if (pArrowDir)
					*pArrowDir = HELP_TEXT_ARROW_SOUTH;
			}
			else
			if (fAngleToToWorldPos > 270.0f && fAngleToToWorldPos <= 360.0f)
			{
				fAngleToToWorldPos -= 270.0f;

				vNewScreenPos.x = OFFSET_FROM_EDGE_OF_SCREEN;
				vNewScreenPos.y = 1.0f-(fAngleToToWorldPos / 90.0f);

				vNewScreenPos.y = Min(1.0f-OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.y);
				vNewScreenPos.y = Max(OFFSET_FROM_EDGE_OF_SCREEN, vNewScreenPos.y);

				if (pArrowDir)
					*pArrowDir = HELP_TEXT_ARROW_WEST;
			}
		}
	}

	return (vNewScreenPos);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHudTools::AdjustForWidescreen()
// PURPOSE:	adjusts the pos, size & wrap values for widescreen format if used
//			also returns true or false whether widescreen is in use or not
/////////////////////////////////////////////////////////////////////////////////////
bool CHudTools::AdjustForWidescreen(eWIDESCREEN_FORMAT WidescreenSetting, Vector2 *pPosition, Vector2 *pSize, Vector2 *pWrap, bool UNUSED_PARAM(bApplyExtraScale))
{
	if (WidescreenSetting == WIDESCREEN_FORMAT_TO_4_3_FROM_16_9)
	{
		if (!GetWideScreen())
		{
			if (pPosition)
			{
				pPosition->x -= ( ( (4.0f / 3.0f)-1.0f ) * 0.5f);
			}

			if (pSize)
			{
				pSize->x *= (4.0f / 3.0f);
			}

			return true;
		}

		return false;
	}

	const float oldPosX = pPosition ? pPosition->x : 0.5f;
#if SUPPORT_MULTI_MONITOR
	const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();

	// move to the central screen
	if (0)
	{
		const fwRect area = mon.getArea();
		const Vector2 vOffset( area.left, area.bottom );
		const Vector2 vSize( area.GetWidth(), area.GetHeight() );
		if (pPosition)
			*pPosition = *pPosition * vSize + vOffset;
		if (pSize)
			*pSize = *pSize * vSize;
		if (pWrap)
			*pWrap = *pWrap * vSize.x;
	}
#endif	//SUPPORT_MULTI_MONITOR

#if __DEV
	if (bForceWidescreenStretch)
		return false;
#endif

	if (!GetWideScreen() || WidescreenSetting == WIDESCREEN_FORMAT_STRETCH)  // change nothing if we are not on a widescreen monitor
		return false;

	Assertf (WidescreenSetting < MAX_COMPLEX_WIDESCREEN_FORMATS, "Hud - invalid widescreen format set");

	if (WidescreenSetting == WIDESCREEN_FORMAT_AUTO)
	{
		if (oldPosX > 0.5f)
			WidescreenSetting = WIDESCREEN_FORMAT_RIGHT;
		else if (oldPosX < 0.5f)
			WidescreenSetting = WIDESCREEN_FORMAT_LEFT;
		else
			WidescreenSetting = WIDESCREEN_FORMAT_CENTRE;
	}

	float fAspectRatio = GetAspectRatio();
#if RSG_PC
	if(fAspectRatio > ASPECT_RATIO_16_9)
	{
		fAspectRatio = ASPECT_RATIO_16_9;
	}
#endif // RSG_PC

	if (rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		fAspectRatio *= CRenderPhaseLensDistortion::GetAspectRatio();
	}

	const float fScaler = (ASPECT_RATIO_4_3) / fAspectRatio,
	fAdjust = (1.f - fScaler);

	switch (WidescreenSetting)
	{
	case WIDESCREEN_FORMAT_CENTRE:
		{
			if (pSize)
				pSize->x *= fScaler;

			if (pPosition)
			{
				pPosition->x *= fScaler;
				pPosition->x += fAdjust*0.5f;
			}

			if (pWrap)
			{
				pWrap->x *= fScaler;
				pWrap->y *= fScaler;
				pWrap->x += fAdjust*0.5f;
				pWrap->y += fAdjust*0.5f;
			}

			break;
		}
	case WIDESCREEN_FORMAT_LEFT:
		{
			if (pSize)
				pSize->x *= fScaler;

			if (pPosition)
				pPosition->x *= fScaler;

			if (pWrap)
			{
				pWrap->x *= fScaler;
				pWrap->y *= fScaler;
			}

			break;
		}
	case WIDESCREEN_FORMAT_RIGHT:
		{
			if (pSize)
				pSize->x *= fScaler;

			if (pPosition)
			{
				pPosition->x *= fScaler;
				pPosition->x += fAdjust;
			}

			if (pWrap)
			{
				pWrap->x *= fScaler;
				pWrap->y *= fScaler;
				pWrap->x += fAdjust;
				pWrap->y += fAdjust;
			}

			break;
		}

	case WIDESCREEN_FORMAT_SIZE_ONLY:
		{
			// adjust the size only
			if (pSize)
				pSize->x *= fScaler;
			break;
		}

		//		case WIDESCREEN_FORMAT_STRETCH:
	default:  // dont do anything for any other format
		{
			// dont need to do anything if we are just stretching  -- should never actually get here
			break;
		}
	}

	return true;
}

eWIDESCREEN_FORMAT CHudTools::GetFormatFromString(const char* hAlign)
{
	if (uiVerify(hAlign))
	{
		return GetFormatFromString(hAlign[0]);
	}
	return WIDESCREEN_FORMAT_CENTRE;
}

eWIDESCREEN_FORMAT CHudTools::GetFormatFromString(const char hAlign)
{
	switch(hAlign)
	{
	case 'c':
	case 'C':
		return WIDESCREEN_FORMAT_CENTRE;

	case 'L':
	case 'l':
		return WIDESCREEN_FORMAT_LEFT;

	case 'S':
	case 's':
		return WIDESCREEN_FORMAT_STRETCH;

	case 'r':
	case 'R':
		return WIDESCREEN_FORMAT_RIGHT;

	}

	return WIDESCREEN_FORMAT_CENTRE;
}

// Similar to AdjustForWidescreen, except assumes the values coming IN are [0,1] values for a 16:9 tuning, then adjusts them to match the current ratio
void CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(eWIDESCREEN_FORMAT WidescreenSetting, Vector2* pPosition, Vector2* pSize /*= NULL*/ WIN32PC_ONLY(, bool bIgnoreMultiHead/* = false*/))
{
	if (WidescreenSetting == WIDESCREEN_FORMAT_AUTO)
	{
		const float oldPosX = pPosition ? pPosition->x : 0.5f;
		if (oldPosX > 0.5f)
			WidescreenSetting = WIDESCREEN_FORMAT_RIGHT;
		else if (oldPosX < 0.5f)
			WidescreenSetting = WIDESCREEN_FORMAT_LEFT;
		else
			WidescreenSetting = WIDESCREEN_FORMAT_CENTRE;
	}

	float fPhysicalAspect = GetAspectRatio();
#if RSG_PC

	// clamp the ratio to 16:9 because we move the elements in at high ratios
	if(CHudTools::IsSuperWideScreen())
	{
		fPhysicalAspect = ASPECT_RATIO_16_9;
	}
#endif // RSG_PC

	if (rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		fPhysicalAspect *= CRenderPhaseLensDistortion::GetAspectRatio();
	}

	const float fScalar = (ASPECT_RATIO_16_9) / fPhysicalAspect,
		fAdjustPos = (1.f - fScalar);

	switch (WidescreenSetting)
	{
	case WIDESCREEN_FORMAT_CENTRE:
		{
			if (pSize)
				pSize->x *= fScalar;

			if (pPosition)
			{
				pPosition->x *= fScalar;
				pPosition->x += fAdjustPos*0.5f;
			}

			break;
		}
	case WIDESCREEN_FORMAT_LEFT:
		{
			if (pSize)
				pSize->x *= fScalar;

			if (pPosition)
				pPosition->x *= fScalar;

			break;
		}
	case WIDESCREEN_FORMAT_RIGHT:
		{
			if (pSize)
				pSize->x *= fScalar;

			if (pPosition)
			{
				pPosition->x *= fScalar;
				pPosition->x += fAdjustPos;
			}

			break;
		}

	case WIDESCREEN_FORMAT_SIZE_ONLY:
		{
			// adjust the size only
			if (pSize)
				pSize->x *= fScalar;
			break;
		}

		//		case WIDESCREEN_FORMAT_STRETCH:
	default:  // dont do anything for any other format
		{
			// dont need to do anything if we are just stretching  -- should never actually get here
			break;
		}
	}

	AdjustForSuperWidescreen(pPosition, pSize WIN32PC_ONLY(, bIgnoreMultiHead));
}

// PURPOSE: Adjusts the values coming in for super widescreen (if we're in super widescreen)
void CHudTools::AdjustForSuperWidescreen(Vector2* WIN32PC_ONLY(pPosition), Vector2* WIN32PC_ONLY(pSize, bool bIgnoreMultiHead/* = false*/))
{
#if RSG_PC
	if ( !CHudTools::IsSuperWideScreen(bIgnoreMultiHead) )
	{
		return;
	}

	float fDifference = (ASPECT_RATIO_16_9 / GetAspectRatio(bIgnoreMultiHead));
	if( pPosition )
		pPosition->x = 0.5f - ((0.5f - pPosition->x) * fDifference);

	if( pSize )
		pSize->x *= fDifference;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CHudTools::CalculateHudPosition()
// PURPOSE:	calculates position of a hud element relative to the safe zone given alignment information. 
//		alignX : 'L'=left, 'R'=right, 'C'=centre or 'I'=ignore
//		alignY : 'T'=top, 'B'=bottom, 'C'=centre or 'I'=ignore
/////////////////////////////////////////////////////////////////////////////////////
Vector2 CHudTools::CalculateHudPosition(Vector2 offset, Vector2 size, u8 alignX, u8 alignY, bool bScript)
{
	Vector2 origin(0.0f, 0.0f);
	Vector2 safeZoneMin, safeZoneMax;

	GetMinSafeZone(safeZoneMin.x, safeZoneMin.y, safeZoneMax.x, safeZoneMax.y, 1.0f, bScript);

	if(alignX == 'L')
		origin.x = safeZoneMin.x;
	else if (alignX == 'R')
		origin.x = safeZoneMax.x - size.x;
	else if (alignX == 'C')
		origin.x = (safeZoneMax.x + safeZoneMax.x - size.x)/2.0f;

	if(alignY == 'T')
		origin.y = safeZoneMin.y;
	else if (alignY == 'B')
		origin.y = safeZoneMax.y - size.y;
	else if (alignY == 'C')
		origin.y = (safeZoneMax.y + safeZoneMax.y - size.y)/2.0f;
	return origin + offset;
}

// PURPOSE: return aspect ratio of screen
float CHudTools::GetAspectRatio(bool MULTI_MONITOR_ONLY(bPhysicalAspect)/*=false*/)
{
#if __CONSOLE
	return (GetWideScreen() ? 16.0f/9.0f : 4.0f/3.0f);
#else
	Settings gameSettings = CSettingsManager::GetInstance().GetUISettings();
	eAspectRatio aspectRatio = gameSettings.m_video.m_AspectRatio;
	switch (aspectRatio)
	{
	case AR_3to2:
		return 3.0f / 2.0f;
	case AR_4to3:
		return 4.0f / 3.0f;
	case AR_5to3:
		return 5.0f / 3.0f;
	case AR_5to4:
		return 5.0f / 4.0f;
	case AR_16to9:
		return 16.0f / 9.0f;
	case AR_16to10:
		return 16.0f / 10.0f;
	case AR_17to9:
		return 17.0f / 9.0f;
	case AR_21to9:
		return 21.0f / 9.0f;
	default:
		break;
	}

	float fWidth = (float)VideoResManager::GetUIWidth();
	float fHeight = (float)VideoResManager::GetUIHeight();

#if SUPPORT_MULTI_MONITOR
	if(!bPhysicalAspect && GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		fWidth = (float)mon.getWidth();
		fHeight = (float)mon.getHeight();
	}
#endif // SUPPORT_MULTI_MONITOR

	return fWidth / fHeight;
#endif //__CONSOLE
}

float CHudTools::GetUIWidth()
{
	float fWidth = (float)VideoResManager::GetUIWidth();

#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		fWidth = (float)mon.getWidth();
	}
#endif // SUPPORT_MULTI_MONITOR

	if(IsSuperWideScreen())
	{
		float fDifference = ASPECT_RATIO_16_9 / GetAspectRatio();
		fWidth = fWidth * fDifference;
	}

	return fWidth;
}

// Because the GRCDEVICE returns the back buffers aspect (which can be swapped) we should base our UI measurements off this function instead.
bool CHudTools::GetWideScreen()
{
	const float WIDESCREEN_ASPECT = 1.5f;
#if __CONSOLE
	float fWidth = (float)VideoResManager::GetUIWidth();
	float fHeight = (float)VideoResManager::GetUIHeight();

	float fAspectRatio = fWidth / fHeight;

	return fAspectRatio > WIDESCREEN_ASPECT;
#else //__CONSOLE
	float fLogicalAspectRatio = GetAspectRatio();
	float fPhysicalAspectRatio = GetUIWidth() / GetUIHeight();

	if(fPhysicalAspectRatio <= WIDESCREEN_ASPECT)
		return false;
	
	return fLogicalAspectRatio > WIDESCREEN_ASPECT;
#endif //__CONSOLE
}

float CHudTools::GetUIHeight()
{
	float fHeight = (float)VideoResManager::GetUIHeight();;

#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		fHeight = (float)mon.getHeight();
	}
#endif // SUPPORT_MULTI_MONITOR

	return fHeight;
}

// PURPOSE: Get multiplier to get from default 16:9 aspect ratio to current aspect ratio
float CHudTools::GetAspectRatioMultiplier( bool bIgnoreMultiHead/*=false*/ )
{
	// aspect ratio is assumed to work for 16:9 screens
	return  GetAspectRatio(bIgnoreMultiHead) / (16.0f/9.0f);
}

float CHudTools::CovertToCenterAtAspectRatio(float val, bool bIgnoreMultiHead/*=false*/)
{
	return 0.5f + ((val-0.5f) * GetAspectRatioMultiplier(bIgnoreMultiHead));
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::IsAspectRatio_4_3_or_16_9()
// PURPOSE: if not truly a 4:3 or a 16:9 resolution, the full screen scaleform movies
//			need to	render slightly different than normal.  This returns false if we
//			are	doing that so various other systems can alter their positions
//			aswell
/////////////////////////////////////////////////////////////////////////////////////
bool CHudTools::IsAspectRatio_4_3_or_16_9()
{
	const float fAspectRatio = GetAspectRatio();
	return (fAspectRatio == ASPECT_RATIO_4_3) || (fAspectRatio == ASPECT_RATIO_16_9);
}

bool CHudTools::IsSuperWideScreen(bool bIgnoreMultiHead)
{
#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
		return false;
#endif // SUPPORT_MULTI_MONITOR

	const float fAspectRatio = GetAspectRatio(bIgnoreMultiHead);
	return fAspectRatio > ASPECT_RATIO_16_9;
}

const strStreamingObjectName& CHudTools::GetFrontendTXDPath()
{
	return FRONTEND_TXD_PATH;
}

const grcBlendStateHandle& CHudTools::GetStandardSpriteBlendState()
{
	if(ms_StandardSpriteBlendStateHandle == grcStateBlock::BS_Invalid)
	{
		// stateblocks
		grcBlendStateDesc bsd;

		//	The remaining BlendStates should all have these two flags set
		bsd.BlendRTDesc[0].BlendEnable = true;
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_SRCALPHA;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;

		ms_StandardSpriteBlendStateHandle = grcStateBlock::CreateBlendState(bsd);
	}

	return ms_StandardSpriteBlendStateHandle;
};

//Calculates the intersection point between two finite length line segments
//http://www.cs.swan.ac.uk/~cssimon/line_intersection.html
//out_intersection is set to the intersection point if it exists
//Returns true if the lines intersect
bool CHudTools::LineSegmentIntersection(const Vector2& p1, const Vector2& p2, const Vector2& p3, const Vector2& p4, Vector2& out_intersection, float* out_ta, float* out_tb)
{
	const float denom = ((p4.x - p3.x) * (p1.y - p2.y)) - ((p1.x - p2.x) * (p4.y - p3.y));
	if (denom == 0) //colinear
	{
		return false;
	}

	const float a1 = ((p3.y - p4.y) * (p1.x - p3.x)) + ((p4.x - p3.x) * (p1.y - p3.y));
	const float b1 = ((p1.y - p2.y) * (p1.x - p3.x)) + ((p2.x - p1.x) * (p1.y - p3.y));
	const float ta = a1 / denom;
	const float tb = b1 / denom;

	if (ta >= 0 && ta <= 1 && tb >= 0 && tb <= 1)
	{
		out_intersection = p1 + (p2 - p1) * ta;
		if (out_ta) *out_ta = ta;
		if (out_tb) *out_tb = tb;
		return true;
	}

	return false;
}

bool CHudTools::LineRectIntersection(const Vector2& p1, const Vector2& p2, const fwRect& rect, atFixedArray<Vector2, 2>& out_intersections)
{
	out_intersections.Reset();

	//No intersection if both points are within the rect
	if ((p1.x < rect.left || p1.x > rect.right || p1.y > rect.bottom || p1.y < rect.top) &&
		(p2.x < rect.left || p2.x > rect.right || p2.y > rect.bottom || p2.y < rect.top))
	{
		return false;
	}

	atFixedArray<float, 2> tvals;
	Vector2 IntersectPoint(0,0);
	float IntersectTval = 1.0f;
	const Vector2 TopLeft(rect.left, rect.top);
	const Vector2 TopRight(rect.right, rect.top);
	const Vector2 BottomRight(rect.right, rect.bottom);
	const Vector2 BottomLeft(rect.left, rect.bottom);

	//Top 
	if (LineSegmentIntersection(p1, p2, TopLeft, TopRight, IntersectPoint, &IntersectTval))
	{
		out_intersections.Push(IntersectPoint);
		tvals.Push(IntersectTval);
	}
	//Right
	if (LineSegmentIntersection(p1, p2, TopRight, BottomRight, IntersectPoint, &IntersectTval))
	{
		out_intersections.Push(IntersectPoint);
		tvals.Push(IntersectTval);
	}
	//Bottom
	if (out_intersections.GetCount() < 2 && LineSegmentIntersection(p1, p2, BottomRight, BottomLeft, IntersectPoint, &IntersectTval))
	{
		out_intersections.Push(IntersectPoint);
		tvals.Push(IntersectTval);
	}
	//Left
	if (out_intersections.GetCount() < 2 && LineSegmentIntersection(p1, p2, BottomLeft, TopLeft, IntersectPoint, &IntersectTval))
	{
		out_intersections.Push(IntersectPoint);
		tvals.Push(IntersectTval);
	}

	//Order intersections
	if (tvals.GetCount() == 2 && tvals[0] > tvals[1])
	{
		const Vector2 temp = out_intersections[0];
		out_intersections[0] = out_intersections[1];
		out_intersections[1] = temp;
	}

	return out_intersections.GetCount() > 0;
}

// eof

