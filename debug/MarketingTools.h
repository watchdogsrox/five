//
// debug/MarketingTools.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef MARKETING_TOOLS_H
#define MARKETING_TOOLS_H

#include "frontend/NewHud.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "weapons/Explosion.h"

class CPed;
namespace rage {
	class bkGroup;
}

class CMarketingTools
{
#if __BANK //Marketing tools are only available in BANK builds.

public:
	static void		Init(unsigned initMode);
	static void		Update();
	static void		UpdateLights();
	static void		InitWidgets();
	static void		InitHudComponentWidgets();

	static bool		IsActive() { return ms_IsActive; }
	static bool		ShouldDisableBloodForPed(const CPed& ped);

private:
	static void		UpdateKeyboardInput();
	static void		UpdatePadInput();
	static void		UpdateAudioControls();
	static void		UpdateGfxControls();
	static void		UpdateLightControls();
	static void		UpdateHudControls();
	static void		WidgetDropExplosion();

	static void		WidgetAddLight();
	static void		WidgetDeleteLight();
	static void		WidgetChangeEditIndex();
	static void		WidgetRefreshLightWidgets();
public:
	static void		WidgetHudShowAll();
	static void		WidgetHudHideAll();
private:
	static void		WidgetChangedHudToggles();
	static void		WidgetAddCharacterControls();

	static u32	ms_InvincibilityMode;
	static u32	ms_BloodMode;
	static u32	ms_AmmoMode;
	static u32	ms_ForcedWeatherType;
	static float	ms_SlowMotionSpeed;
	static bool		ms_IsActive;
	static bool		ms_ShouldMuteMusic;
	static bool		ms_ShouldMuteSpeech;
	static bool		ms_ShouldMuteSfx;
	static bool		ms_ShouldMuteAmbience;
	static bool		ms_OverrideMotionBlur;
	static bool		ms_LightsEnabled;
	static bool		ms_WasHudShown;
	static bool		ms_DisplayHud;
	static bool		ms_DisplayMiniMap;
	static bool		ms_DisplayMarkers;
	static bool		ms_DisplayHudComponent[MAX_NEW_HUD_COMPONENTS];
	static float	ms_OverrideMotionBlurStrength;
	static CExplosionManager::CExplosionArgs * ms_ExplosionSetup;

	static const int MAX_DEBUG_LIGHTS = 10;
	static struct CLightDebugSetup
	{
				CLightDebugSetup()				{ Reset(); }
		void	Reset();
		void	AddWidgets(bkBank & bank);

		Vector3	m_LightColour;
		Vector3	m_LightPosition;
		Vector3	m_LightDirection;
		Vector3	m_LightTangent;
		float	m_LightRange;
		float	m_LightInnerCone;
		float   m_LightOuterCone;
		float   m_LightConeAngle;
		float	m_LightIntensity;
		float	m_LightFalloffExponent;
		float   m_LightCapsuleExtent;
		int 	m_LightType;
		bool    m_IsCoronaOnly;
		bool	m_IsLightAttached;
		bool    m_IsLightOrientedToCam;
 		bool	m_IsDynamicShadowLight;
		bool	m_IsFXLight;
	} ms_LightArray[MAX_DEBUG_LIGHTS];

	static u32			ms_LightArrayNumUsed;
	static u32			ms_LightArrayEditIndex;
	static rage::bkGroup *	ms_LightControlGroup;
	static rage::bkGroup *	ms_LightWidgetGroup;
	static rage::bkGroup *	ms_CharacterControlsGroup;
	static rage::bkGroup *	ms_CharacterWidgetGroup;

#else // !__BANK - Stub out everything.

public:
	static bool		IsActive() { return false; }
	static bool		ShouldDisableBloodForPed(const CPed& UNUSED_PARAM(ped)) { return false; }

#endif //__BANK
};

#endif // MARKETING_TOOLS_H
