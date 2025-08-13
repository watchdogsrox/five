/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HudTools.h
// PURPOSE : common hud related tools (code pulled from NewHud)
// AUTHOR  : James Chagaris
// STARTED : 1/23/2013
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_HUD_TOOLS_H_
#define INC_HUD_TOOLS_H_

//rage
#include "atl/array.h"
#include "fwmaths/Rect.h"
#include "grcore/monitor.h"
#include "streaming/streamingmodule.h"
#include "vector/vector2.h"
#include "vector/vector3.h"

//game
#include "Text/Messages.h"



enum eNewHudComponents
{
	NEW_HUD = 0,

	NEW_HUD_WANTED_STARS,
	NEW_HUD_WEAPON_ICON,
	NEW_HUD_CASH,
	NEW_HUD_MP_CASH,
	NEW_HUD_MP_MESSAGE,
	NEW_HUD_VEHICLE_NAME,
	NEW_HUD_AREA_NAME,
	NEW_HUD_UNUSED, // Used to be district, and then vehicle class.
	NEW_HUD_STREET_NAME,
	NEW_HUD_HELP_TEXT,
	NEW_HUD_FLOATING_HELP_TEXT_1,
	NEW_HUD_FLOATING_HELP_TEXT_2,
	NEW_HUD_CASH_CHANGE,
	NEW_HUD_RETICLE,
	NEW_HUD_SUBTITLE_TEXT,
	NEW_HUD_RADIO_STATIONS,
	NEW_HUD_SAVING_GAME,
	NEW_HUD_GAME_STREAM,
	NEW_HUD_WEAPON_WHEEL,
	NEW_HUD_WEAPON_WHEEL_STATS,

//	NEW_HUD_DEBUG,

	MAX_NEW_HUD_COMPONENTS
};

enum eWIDESCREEN_FORMAT
{
	WIDESCREEN_FORMAT_STRETCH = 0,	// stretch from 4:3 to 16:9
	WIDESCREEN_FORMAT_CENTRE,		// keep 4:3 perspective centred in 16:9
	WIDESCREEN_FORMAT_LEFT,			// keep 4:3 perspective but align to the left in 16:9
	WIDESCREEN_FORMAT_RIGHT,		// keep 4:3 perspective but align to the right in 16:9

	WIDESCREEN_FORMAT_AUTO,			// 4:3 & automatically decide where based on original screen position

	MAX_BASIC_WIDESCREEN_FORMATS,

	// code internal states:
	WIDESCREEN_FORMAT_FILL,			// keeps 4:3 perspective and fills the sides totally black
	WIDESCREEN_FORMAT_SIZE_ONLY,	// adjusts it for widescreen but doesnt adjust position

	WIDESCREEN_FORMAT_TO_4_3_FROM_16_9,	// adjusts it for widescreen but doesnt adjust position

	MAX_COMPLEX_WIDESCREEN_FORMATS
};

class CHudTools
{
public:
	static float GetSafeZoneSize();
	static void GetMinSafeZone(float &x0, float &y0, float &x1, float &y1, float aspectRatio=1.0f, bool bScript = false);
	static void GetMinSafeZoneForScaleformMovies(float &x0, float &y0, float &x1, float &y1, float aspectRatio=1.0f);
	static bool HasJustRestarted() { return bJustRestarted; }
	static void SetAsRestarted(bool bRestart) { bJustRestarted = bRestart; }
	static Vector2 GetHudPosFromWorldPos(Vector3::Vector3Param vWorldPositon, eTEXT_ARROW_ORIENTATION  *pArrowDir = NULL);

	static bool AdjustForWidescreen(eWIDESCREEN_FORMAT WidescreenSetting, Vector2 *pPosition, Vector2 *pSize = NULL, Vector2 *pWrap = NULL, bool bApplyExtraScale = false);
	
	static eWIDESCREEN_FORMAT GetFormatFromString(const char* hAlign);
	static eWIDESCREEN_FORMAT GetFormatFromString(const char hAlign);

	// PURPOSE: Similar to AdjustForWidescreen, except assumes the values coming IN are [0,1] values for a 16:9 tuning, then adjusts them to match the current ratio
	static void AdjustNormalized16_9ValuesForCurrentAspectRatio(eWIDESCREEN_FORMAT WidescreenSetting, Vector2* pPosition, Vector2* pSize = NULL WIN32PC_ONLY(, bool bIgnoreMultiHead = false));
	// PURPOSE: Adjusts the values coming in for super widescreen (if we're in super widescreen)
	static void AdjustForSuperWidescreen(Vector2* pPosition, Vector2* pSize WIN32PC_ONLY(, bool bIgnoreMultiHead = false));
	// PURPOSE: calculate position on screen relative to safe zone
	static Vector2 CalculateHudPosition(Vector2 m_offset, Vector2 m_size, u8 m_alignX, u8 m_alignY, bool bScript = false);
	// PURPOSE: return aspect ratio of screen
	static float GetAspectRatio(bool MULTI_MONITOR_ONLY(bPhysicalAspect) = false);
	// PURPOSE: Get multiplier to get from default 16:9 aspect ratio to current aspect ratio
	static float GetAspectRatioMultiplier(bool bIgnoreMultiHead = false);
	static float CovertToCenterAtAspectRatio(float val, bool bIgnoreMultiHead = false); // val should be in screen space, so 0-1
	static float GetUIWidth();
	static float GetUIHeight();
	static bool GetWideScreen();

	static bool BeginHudScaleformMethod(eNewHudComponents eHudComponentId, const char *pMethodName);
	static void CallHudScaleformMethod(eNewHudComponents eHudComponentId, const char *pMethodName, bool bInstanty = false);
	DURANGO_ONLY( static void CHudTools::GetFakeLoadingScreenSafeZone(float &x0, float &y0, float &x1, float &y1); )
	static float GetDifferenceFrom_16_9_ToCurrentAspectRatio();
	static bool IsAspectRatio_4_3_or_16_9();
	static bool IsSuperWideScreen(bool bIgnoreMultiHead = false);

	static const strStreamingObjectName& GetFrontendTXDPath();
	static const grcBlendStateHandle& GetStandardSpriteBlendState();

#if __BANK
	static bool bDisplayHud;
	static bool bForceWidescreenStretch;
#endif
	static grcBlendStateHandle ms_StandardSpriteBlendStateHandle;

public:

	static bool LineSegmentIntersection(const Vector2& line1_p1, const Vector2& line1_p2, const Vector2& line2_p1, const Vector2& line2_p2, Vector2& out_intersection, float* out_ta = NULL, float* out_tb = NULL);
	static bool LineRectIntersection(const Vector2& line_p1, const Vector2& line_p2, const fwRect& rect, atFixedArray<Vector2, 2>& out_intersections);

private:
	static bool bJustRestarted;
};

#endif  // INC_HUD_TOOLS_H_

// eof

