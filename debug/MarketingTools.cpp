//
// debug/MarketingTools.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#include "debug/MarketingTools.h"

#if __BANK //Marketing tools are only available in BANK builds.

#include "bank/bkmgr.h"
#include "bank/io.h"
#include "fwsys/timer.h"

#include "animation/debug/AnimViewer.h"
#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/marketing/MarketingDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/MiniMap.h"
#include "Frontend/MovieClipText.h"
#include "game/cheat.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "peds/PedDebugVisualiser.h"
#include "peds/pedpopulation.h"
#include "peds/Ped.h"
#include "renderer/Lights/lights.h"
#include "renderer/PostProcessFX.h"
#include "scene/lod/LodScale.h"
#include "scene/world/GameWorld.h"
#include "system/controlMgr.h"
#include "text/messages.h"
#include "vfx\misc\markers.h"
enum eInvincibilityModes
{
	VULNERABLE = 0,
	INVINCIBLE,
	INVINCIBLE_NO_REACTIONS,
	NUM_INVINCIBILITY_MODES
};

static const char* g_InvincibilityModeLabels[NUM_INVINCIBILITY_MODES] =
{
	"HELP_NOINVIN",		// VULNERABLE
	"HELP_INVIN",		// INVINCIBLE
	"MKT_INVIN_NORCT"	// INVINCIBLE_NO_REACTIONS
};

enum eBloodModes
{
	BLOOD_ENABLED = 0,
	BLOOD_DISABLED_FOR_PLAYER,
	BLOOD_DISABLED_GLOBALLY,
	NUM_BLOOD_MODES
};

static const char* g_BloodModeLabels[NUM_BLOOD_MODES] =
{
	"MKT_BLOOD",		// BLOOD_ENABLED
	"MKT_NO_P_BLOOD",	// BLOOD_DISABLED_FOR_PLAYER
	"MKT_NO_BLOOD"		// BLOOD_DISABLED_GLOBALLY
};

enum eAmmoModes
{
	LIMITED_AMMO = 0,
	UNLIMITED_AMMO,
	INFINITE_CLIPS,
	NUM_AMMO_MODES
};

static const char* g_AmmoModeLabels[NUM_BLOOD_MODES] =
{
	"MKT_LTD_AMMO",		// LIMITED_AMMO
	"MKT_UNLTD_AMMO",	// UNLIMITED_AMMO
	"MKT_INF_CLIP"		// INFINITE_CLIPS
};

static const char *g_LightTypeNames[] = 
{
	"Point", 
	"Spot",
	"Capsule"
};


// OPTIMISATIONS_OFF()

u32	CMarketingTools::ms_InvincibilityMode			= VULNERABLE;
u32	CMarketingTools::ms_BloodMode					= BLOOD_ENABLED;
u32	CMarketingTools::ms_AmmoMode					= LIMITED_AMMO;
u32	CMarketingTools::ms_ForcedWeatherType			= 0;
u32	CMarketingTools::ms_LightArrayNumUsed			= 0;
u32	CMarketingTools::ms_LightArrayEditIndex			= 0;
float	CMarketingTools::ms_SlowMotionSpeed				= 0.5f;
bool	CMarketingTools::ms_IsActive					= false;
bool	CMarketingTools::ms_ShouldMuteMusic				= false;
bool	CMarketingTools::ms_ShouldMuteSpeech			= false;
bool	CMarketingTools::ms_ShouldMuteSfx				= false;
bool	CMarketingTools::ms_ShouldMuteAmbience			= false;
bool	CMarketingTools::ms_OverrideMotionBlur			= false;
bool	CMarketingTools::ms_LightsEnabled				= false;
bool	CMarketingTools::ms_WasHudShown					= true;
bool	CMarketingTools::ms_DisplayHud					= true;
bool	CMarketingTools::ms_DisplayMiniMap				= true;
bool	CMarketingTools::ms_DisplayMarkers				= true;
bool	CMarketingTools::ms_DisplayHudComponent[MAX_NEW_HUD_COMPONENTS];
float	CMarketingTools::ms_OverrideMotionBlurStrength	= 0.0f;

CExplosionManager::CExplosionArgs *			CMarketingTools::ms_ExplosionSetup		= NULL;
struct	CMarketingTools::CLightDebugSetup	CMarketingTools::ms_LightArray[CMarketingTools::MAX_DEBUG_LIGHTS];
rage::bkGroup *								CMarketingTools::ms_LightControlGroup	= NULL;
rage::bkGroup *								CMarketingTools::ms_LightWidgetGroup	= NULL;
rage::bkGroup *								CMarketingTools::ms_CharacterWidgetGroup= NULL;
rage::bkGroup *								CMarketingTools::ms_CharacterControlsGroup= NULL;


void CMarketingTools::Init(unsigned /*initMode*/)
{
	for(s32 i=0; i<MAX_NEW_HUD_COMPONENTS; i++)
	{
		ms_DisplayHudComponent[i] = true;
	}
}

void CMarketingTools::Update()
{
	PF_PUSH_TIMEBAR("marketing tools");
	ms_IsActive = (CControlMgr::GetKeyboard().GetKeyboardMode() == KEYBOARD_MODE_MARKETING);
	if(ms_IsActive)
	{
		UpdateKeyboardInput();
		UpdatePadInput();

		UpdateAudioControls();
		UpdateGfxControls();
		UpdateHudControls();
	}
	PF_POP_TIMEBAR();
}

void CMarketingTools::UpdateLights()
{
	ms_IsActive = (CControlMgr::GetKeyboard().GetKeyboardMode() == KEYBOARD_MODE_MARKETING);
	if(ms_IsActive)
	{
		UpdateLightControls();
	}
}

void CMarketingTools::UpdateKeyboardInput()
{
	CKeyboard& keyboard = CControlMgr::GetKeyboard();

	CNumberWithinMessage NumberArray[1];

 	if(keyboard.GetKeyJustDown(KEY_O, KEYBOARD_MODE_MARKETING, "Toggle HUD and any screen overlay"))
	{
		if(ms_WasHudShown)
		{
			WidgetHudHideAll();
		}
		else
		{
			WidgetHudShowAll();
		}
	}
	else if(keyboard.GetKeyJustDown(KEY_V, KEYBOARD_MODE_MARKETING, "Cycle invincibility mode (vulnerable/invincible/invincible with reactions off)"))
	{
		ms_InvincibilityMode					= (ms_InvincibilityMode + 1) % NUM_INVINCIBILITY_MODES;
		CPlayerInfo::ms_bDebugPlayerInvincible	= (ms_InvincibilityMode != VULNERABLE);
		CPed* player							= CGameWorld::FindFollowPlayer();
		if(player)
		{
			const bool canPlayerBeDamaged	= (ms_InvincibilityMode != INVINCIBLE_NO_REACTIONS);
			player->GetPlayerInfo()->SetPlayerCanBeDamaged(canPlayerBeDamaged);
		}

		//Display confirmation debug text (if the HUD is enabled.)
		if(ms_InvincibilityMode < NUM_INVINCIBILITY_MODES)
		{
			char *string = TheText.Get(g_InvincibilityModeLabels[ms_InvincibilityMode]);
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
		}
	}
	else if(keyboard.GetKeyJustDown(KEY_J, KEYBOARD_MODE_MARKETING, "Skip to the next checkpoint"))
	{
		//TODO: Use script functionality from Game overlay.
	}
	else if(keyboard.GetKeyJustDown(KEY_H, KEYBOARD_MODE_MARKETING, "Increase LOD draw distance") &&
		(keyboard.GetKeyDown(KEY_LSHIFT, KEYBOARD_MODE_MARKETING) ||
		keyboard.GetKeyDown(KEY_RSHIFT, KEYBOARD_MODE_MARKETING)))
	{
		//Increase the global LOD scale by 0.1.
		float lodScale = g_LodScale.GetMarketingCameraScale_In();
		lodScale = Min(lodScale + 0.1f, 100.0f);
		g_LodScale.SetMarketingCameraScale_In(lodScale);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string = TheText.Get("MKT_LOD");
		const s32 lodScalePercentage = (s32)Floorf((lodScale * 100.0f) + 0.5f);

		NumberArray[0].Set(lodScalePercentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}
	else if(keyboard.GetKeyJustDown(KEY_K, KEYBOARD_MODE_MARKETING, "Decrease LOD draw distance") &&
		(keyboard.GetKeyDown(KEY_LSHIFT, KEYBOARD_MODE_MARKETING) ||
		keyboard.GetKeyDown(KEY_RSHIFT, KEYBOARD_MODE_MARKETING)))
	{
		//Decrease the global LOD scale by 0.1.
		float lodScale = g_LodScale.GetMarketingCameraScale_In();
		lodScale = Max(lodScale - 0.1f, 0.1f);
		g_LodScale.SetMarketingCameraScale_In(lodScale);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string = TheText.Get("MKT_LOD");
		const s32 lodScalePercentage = (s32)Floorf((lodScale * 100.0f) + 0.5f);

		NumberArray[0].Set(lodScalePercentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}
	else if(keyboard.GetKeyJustDown(KEY_H, KEYBOARD_MODE_MARKETING, "Heal player model"))
	{
		CCheat::HealthCheat();
	}
	else if(keyboard.GetKeyJustDown(KEY_B, KEYBOARD_MODE_MARKETING, "Cycle blood mode"))
	{
		ms_BloodMode = (ms_BloodMode + 1) % NUM_BLOOD_MODES;

		//Display confirmation debug text (if the HUD is enabled.)
		if(ms_BloodMode < NUM_BLOOD_MODES)
		{
			char *string = TheText.Get(g_BloodModeLabels[ms_BloodMode]);
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
		}
	}
	else if(keyboard.GetKeyJustDown(KEY_M, KEYBOARD_MODE_MARKETING, "Toggle marketing menu"))
	{
		//TODO: Toggle display of a custom marketing menu, containing links to other XML menus.
	}
	else if(keyboard.GetKeyJustDown(KEY_W, KEYBOARD_MODE_MARKETING, "Give player all weapons"))
	{
		CCheat::WeaponCheat1();
	}
	else if(keyboard.GetKeyJustDown(KEY_Z, KEYBOARD_MODE_MARKETING, "Trigger lightning strike"))
	{
		g_weather.ForceLightningFlash();
	}
	else if(keyboard.GetKeyJustDown(KEY_P, KEYBOARD_MODE_MARKETING, "Toggle pausing of time of day"))
	{
		const bool isPaused = CClock::GetIsPaused();
		CClock::Pause(!isPaused);
	}
	else if(keyboard.GetKeyJustDown(KEY_U, KEYBOARD_MODE_MARKETING, "Cycle player ammo mode"))
	{
		ms_AmmoMode = (ms_AmmoMode + 1) % NUM_AMMO_MODES;
		CPed * player = CGameWorld::FindFollowPlayer();
		if(player && player->GetInventory())
		{
			const bool shouldUseInfiniteAmmo	= (ms_AmmoMode != LIMITED_AMMO);
			player->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(shouldUseInfiniteAmmo);
			const bool shouldUseInfiniteClips	= (ms_AmmoMode == INFINITE_CLIPS);
			player->GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(shouldUseInfiniteClips);
		}

		//Display confirmation debug text (if the HUD is enabled.)
		if(ms_AmmoMode < NUM_AMMO_MODES)
		{
			char *string = TheText.Get(g_AmmoModeLabels[ms_AmmoMode]);
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
		}
	}
	else if(keyboard.GetKeyJustDown(KEY_F1, KEYBOARD_MODE_MARKETING, "Toggle unlimited deadeye/bullet-time"))
	{
		//TODO: Toggle unlimited bullet-time in MP3.
	}
	else if(keyboard.GetKeyJustDown(KEY_1, KEYBOARD_MODE_MARKETING, "Toggle slow-motion"))
	{
		const float slowMotionSpeed = fwTimer::GetDebugTimeScale();
		if(slowMotionSpeed < 1.0f)
		{
			//NOTE: The current marketing slow-motion speed is preserved.
			fwTimer::SetDebugTimeScale(1.0f);

			//Display confirmation debug text (if the HUD is enabled.)
			char *string = TheText.Get("MKT_NOSLOW");
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
		}
		else
		{
			fwTimer::SetDebugTimeScale(ms_SlowMotionSpeed);

			//Display confirmation debug text (if the HUD is enabled.)
			char *string = TheText.Get("MKT_SLOW");
			const s32 slowMotionPercentage = (s32)Floorf((ms_SlowMotionSpeed * 100.0f) + 0.5f);

			NumberArray[0].Set(slowMotionPercentage);
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
		}
	}
	else if(keyboard.GetKeyJustDown(KEY_2, KEYBOARD_MODE_MARKETING, "Increase slow-motion speed"))
	{
		ms_SlowMotionSpeed = Min(ms_SlowMotionSpeed + 0.1f, 0.9f);

		//NOTE: Slow-motion is activated if necessary.
		fwTimer::SetDebugTimeScale(ms_SlowMotionSpeed);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string = TheText.Get("MKT_SLOW");
		const s32 slowMotionPercentage = (s32)Floorf((ms_SlowMotionSpeed * 100.0f) + 0.5f);

		NumberArray[0].Set(slowMotionPercentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}
	else if(keyboard.GetKeyJustDown(KEY_3, KEYBOARD_MODE_MARKETING, "Decrease slow-motion speed"))
	{
		ms_SlowMotionSpeed = Max(ms_SlowMotionSpeed - 0.1f, 0.1f);

		//NOTE: Slow-motion is activated if necessary.
		fwTimer::SetDebugTimeScale(ms_SlowMotionSpeed);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string = TheText.Get("MKT_SLOW");
		const s32 slowMotionPercentage = (s32)Floorf((ms_SlowMotionSpeed * 100.0f) + 0.5f);

		NumberArray[0].Set(slowMotionPercentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}
	else if(keyboard.GetKeyJustDown(KEY_4, KEYBOARD_MODE_MARKETING, "Toggle buttons images in movie clips text"))
	{
		CMovieClipText::ms_bDontUseIcons = !CMovieClipText::ms_bDontUseIcons;
	}
	else if(keyboard.GetKeyJustDown(KEY_PAGEUP, KEYBOARD_MODE_MARKETING, "Next weather type"))
	{
		const u32 numTypes = g_weather.GetNumTypes();
		ms_ForcedWeatherType = (ms_ForcedWeatherType + 1) % numTypes;
		g_weather.ForceTypeNow(ms_ForcedWeatherType, CWeather::FT_None);
		PostFX::ResetAdaptedLuminance();
	}
	else if(keyboard.GetKeyJustDown(KEY_PAGEDOWN, KEYBOARD_MODE_MARKETING, "Previous weather type"))
	{
		const u32 numTypes = g_weather.GetNumTypes();
		ms_ForcedWeatherType = (ms_ForcedWeatherType + numTypes - 1) % numTypes;
		g_weather.ForceTypeNow(ms_ForcedWeatherType, CWeather::FT_None);
		PostFX::ResetAdaptedLuminance();
	}
	else if(keyboard.GetKeyJustDown(KEY_EQUALS, KEYBOARD_MODE_MARKETING, "Advance time one hour") ||
		keyboard.GetKeyJustDown(KEY_ADD, KEYBOARD_MODE_MARKETING, "Advance time one hour"))
	{
		CClock::AddTime(1, 0, 0);
		PostFX::ResetAdaptedLuminance();
	}
	else if(keyboard.GetKeyJustDown(KEY_MINUS, KEYBOARD_MODE_MARKETING, "Decrease time one hour") ||
		keyboard.GetKeyJustDown(KEY_SUBTRACT, KEYBOARD_MODE_MARKETING, "Decrease time one hour"))
	{
		CClock::AddTime(-1, 0, 0);
		PostFX::ResetAdaptedLuminance();
	}
	else if(keyboard.GetKeyJustDown(KEY_UP, KEYBOARD_MODE_MARKETING, "Increase ped density") &&
		(keyboard.GetKeyDown(KEY_LSHIFT, KEYBOARD_MODE_MARKETING) ||
		keyboard.GetKeyDown(KEY_RSHIFT, KEYBOARD_MODE_MARKETING)))
	{
		//Increase the global max population scale by 0.1.
		float maxPopulationScale = CPedPopulation::GetPopCycleMaxPopulationScaler();
		maxPopulationScale = Min(maxPopulationScale + 0.1f, 50.0f);
		CPedPopulation::SetPopCycleMaxPopulationScaler(maxPopulationScale);
		
		//Display confirmation debug text (if the HUD is enabled.)
		char *string = TheText.Get("MKT_PEDPOP");
		const s32 maxPopulationScalePercentage = (s32)Floorf((maxPopulationScale * 100.0f) + 0.5f);

		NumberArray[0].Set(maxPopulationScalePercentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}
	else if(keyboard.GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_MARKETING, "Decrease ped density") &&
		(keyboard.GetKeyDown(KEY_LSHIFT, KEYBOARD_MODE_MARKETING) ||
		keyboard.GetKeyDown(KEY_RSHIFT, KEYBOARD_MODE_MARKETING)))
	{
		//Decrease the global max population scale by 0.1.
		float maxPopulationScale = CPedPopulation::GetPopCycleMaxPopulationScaler();
		maxPopulationScale = Max(maxPopulationScale - 0.1f, 0.1f);
		CPedPopulation::SetPopCycleMaxPopulationScaler(maxPopulationScale);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string = TheText.Get("MKT_PEDPOP");
		const s32 maxPopulationScalePercentage = (s32)Floorf((maxPopulationScale * 100.0f) + 0.5f);

		NumberArray[0].Set(maxPopulationScalePercentage);
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
	}
	else if(keyboard.GetKeyJustDown(KEY_R, KEYBOARD_MODE_MARKETING, "Toggle marketing camera GTA/RDR control mapping") &&
		(keyboard.GetKeyDown(KEY_LMENU, KEYBOARD_MODE_MARKETING) || keyboard.GetKeyDown(KEY_RMENU, KEYBOARD_MODE_MARKETING)))
	{
		//Toggle the control mapping for the marketing cameras.
		camMarketingDirector& marketingDirector	= camInterface::GetMarketingDirector();
		const bool shouldUseRdrControlMapping	= !marketingDirector.ShouldUseRdrControlMapping();
		marketingDirector.SetShouldUseRdrControlMapping(shouldUseRdrControlMapping);

		//Display confirmation debug text (if the HUD is enabled.)
		char *string = TheText.Get(shouldUseRdrControlMapping ? "MKT_RDRCTRLMAP" : "MKT_GTACTRLMAP");
		CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);
	}
}

void CMarketingTools::UpdatePadInput()
{
	//Ensure that the player pad, rather than the debug pad, is used for RAG widget control.
	s32 playerPadIndex = CControlMgr::GetMainPlayerIndex();
	bkIo::SetPadIndex(playerPadIndex);

	CPad& debugPad = CControlMgr::GetDebugPad();

	//NOTE: Ignore further pad input here if start is held down, as special debug buttons may be pressed (in combination with start.)
	const u32 debugButtons = debugPad.GetDebugButtons();
	if(!(debugButtons & ioPad::START))
	{
		if(debugPad.GetDPadRight())
		{
			CClock::AddTime(1, 0, 0);
			PostFX::ResetAdaptedLuminance();
		}
		else if(debugPad.DPadLeftJustDown())
		{
			//Advance to next weather type and force it to remain active.
			const u32 numTypes = g_weather.GetNumTypes();
			ms_ForcedWeatherType = (ms_ForcedWeatherType + 1) % numTypes;
			g_weather.ForceTypeNow(ms_ForcedWeatherType, CWeather::FT_None);
			PostFX::ResetAdaptedLuminance();
		}
		if(debugPad.ButtonCircleJustDown())
		{
			ms_LightsEnabled = !ms_LightsEnabled;
			if (ms_LightsEnabled && ms_LightArrayNumUsed==0)
				WidgetAddLight();	// if we just turned on the lights and there is not one available then make a new light
			//Display confirmation debug text (if the HUD is enabled.)
			char *string = ms_LightsEnabled ? TheText.Get("MKT_CAMLIGHTON") : TheText.Get("MKT_CAMLIGHTOFF");
			CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string);

		}
	}
	else
	{
		if (ms_LightArrayEditIndex < ms_LightArrayNumUsed)
		{
			if (debugButtons & ioPad::LLEFT)
			{
				//control spread of cam light
				ms_LightArray[ms_LightArrayEditIndex].m_LightRange -= 0.1f;
				if (ms_LightArray[ms_LightArrayEditIndex].m_LightRange < 0.2f)
					ms_LightArray[ms_LightArrayEditIndex].m_LightRange = 0.2f;
			}
			else if (debugButtons & ioPad::LRIGHT)
			{
				ms_LightArray[ms_LightArrayEditIndex].m_LightRange += 0.1f;
				if (ms_LightArray[ms_LightArrayEditIndex].m_LightRange > 100.0f)
					ms_LightArray[ms_LightArrayEditIndex].m_LightRange = 100.0f;
			}
			if (debugButtons & ioPad::LDOWN)
			{
				//control brightness of cam light
				ms_LightArray[ms_LightArrayEditIndex].m_LightIntensity -= 0.1f;
				if (ms_LightArray[ms_LightArrayEditIndex].m_LightIntensity < 0.1f)
					ms_LightArray[ms_LightArrayEditIndex].m_LightIntensity = 0.1f;
			}
			else if (debugButtons & ioPad::LUP)
			{
				ms_LightArray[ms_LightArrayEditIndex].m_LightIntensity += 0.1f;
				if (ms_LightArray[ms_LightArrayEditIndex].m_LightIntensity > 100.0f)
					ms_LightArray[ms_LightArrayEditIndex].m_LightIntensity = 100.0f;
			}
		}
	}
}

void CMarketingTools::UpdateAudioControls()
{
	//Ensure that our audio muting preferences are applied every frame, as the marketing tools should dominate when active.
	const bool isMusicMuted = audNorthAudioEngine::GetIsMusicMuted(); 
	if(ms_ShouldMuteMusic != isMusicMuted)
	{
		audNorthAudioEngine::MuteMusic(ms_ShouldMuteMusic);
	}

	const bool isSpeechMuted = audNorthAudioEngine::GetIsSpeechMuted();
	if(ms_ShouldMuteSpeech != isSpeechMuted)
	{
		audNorthAudioEngine::MuteSpeech(ms_ShouldMuteSpeech);
	}

	const bool isSfxMuted = audNorthAudioEngine::GetIsSfxMuted();
	if(ms_ShouldMuteSfx != isSfxMuted)
	{
		audNorthAudioEngine::MuteSfx(ms_ShouldMuteSfx);
	}

	const bool isAmbienceMuted = audNorthAudioEngine::GetIsAmbienceMuted();
	if(ms_ShouldMuteAmbience != isAmbienceMuted)
	{
		audNorthAudioEngine::MuteAmbience(ms_ShouldMuteAmbience);
	}
}

void CMarketingTools::UpdateGfxControls()
{
	// stomp the existing gfx setings if the overrides are active
	if (ms_OverrideMotionBlur)
	{
		camInterface::GetGameplayDirector().DebugSetOverrideMotionBlurThisUpdate(ms_OverrideMotionBlurStrength);
		camInterface::GetMarketingDirector().DebugSetOverrideMotionBlurThisUpdate(ms_OverrideMotionBlurStrength);
	}
}

void CMarketingTools::UpdateLightControls()
{
	for(int i=0;i<ms_LightArrayNumUsed;i++)
	{
		struct CLightDebugSetup & light = ms_LightArray[i];
		if( light.m_IsLightOrientedToCam )
		{
			//Move the light with the camera frame.
			const Vector3 lightDir(0.0f, 1.0f, 0.0f);
			const Vector3 lightTan(1.0f, 0.0f, 0.0f);

			camInterface::GetMat().Transform3x3(lightDir, light.m_LightDirection);
			camInterface::GetMat().Transform3x3(lightTan, light.m_LightTangent);
		}
		if( light.m_IsLightAttached )
		{
			light.m_LightPosition = camInterface::GetPos();
		}

		if (ms_LightsEnabled)
		{
			//It looks like this light must be added every update and is automatically removed when we no longer add it.
			fwInteriorLocation outside;

			u32 lightFlags = LIGHTFLAG_CUTSCENE; 
			lightFlags |= light.m_IsDynamicShadowLight ? (LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS) : 0; 
			lightFlags |= light.m_IsCoronaOnly ? LIGHTFLAG_CORONA_ONLY : 0;
			lightFlags |= light.m_IsFXLight ? LIGHTFLAG_FX : 0;

			Vector3 normalizedColor = light.m_LightColour/255.0f;
			eLightType lightType = LIGHT_TYPE_SPOT;
			switch( light.m_LightType )
			{
				case 0: 
					lightType = LIGHT_TYPE_POINT;
					break;
				case 1:
					lightType = LIGHT_TYPE_SPOT; 
					break;
				case 2:
					lightType = LIGHT_TYPE_CAPSULE;
					break;
				default:
					break;
			};
			CLightSource lightToAdd(lightType, lightFlags, light.m_LightPosition, normalizedColor, light.m_LightIntensity, LIGHT_ALWAYS_ON);
			lightToAdd.SetRadius(light.m_LightRange);
			lightToAdd.SetFalloffExponent(light.m_LightFalloffExponent);
			if( lightType == LIGHT_TYPE_SPOT )
			{
				lightToAdd.SetSpotlight(light.m_LightConeAngle * light.m_LightInnerCone * 0.01f, light.m_LightConeAngle * light.m_LightOuterCone * 0.01f);
				lightToAdd.SetDirTangent(light.m_LightDirection, light.m_LightTangent);
			}	
			if( lightType == LIGHT_TYPE_CAPSULE )
			{
				lightToAdd.SetCapsule(light.m_LightCapsuleExtent);
				lightToAdd.SetDirTangent(light.m_LightDirection, light.m_LightTangent);
			}
			if (light.m_IsDynamicShadowLight)
			{
				lightToAdd.SetShadowTrackingId(fwIdKeyGenerator::Get( ms_LightArray, i));
			}
			
			Lights::AddSceneLight(lightToAdd);
		}
	}
}

void CMarketingTools::UpdateHudControls()
{
	for(s32 i=0; i<MAX_NEW_HUD_COMPONENTS; i++)
	{
		if(ms_DisplayHudComponent[i])
		{
			CNewHud::SetHudComponentToBeShown(i);
		}
		else
		{
			CNewHud::SetHudComponentToBeHidden(i);
		}
	}
}

void CMarketingTools::InitWidgets()
{
	if (!ms_ExplosionSetup)
		ms_ExplosionSetup = rage_new CExplosionManager::CExplosionArgs(EXP_TAG_GRENADE,VEC3_ZERO);

 	bkBank& bank = BANKMGR.CreateBank("Marketing Tools");

	bank.PushGroup("Audio controls", false);
	{
		bank.AddToggle("Mute music",				&ms_ShouldMuteMusic);
		bank.AddToggle("Mute speech",				&ms_ShouldMuteSpeech);
		bank.AddToggle("Mute sfx",					&ms_ShouldMuteSfx);
		bank.AddToggle("Mute ambient noise",		&ms_ShouldMuteAmbience);
	}
	bank.PopGroup();
	bank.PushGroup("Gfx Controls", false);
	{
		bank.AddToggle("Override Motion Blur",		&ms_OverrideMotionBlur);
		bank.AddSlider("Motion Blur Strength",		&ms_OverrideMotionBlurStrength, 0.0f, 1.0f, 0.02f);
	}
	bank.PopGroup();
	if (ms_ExplosionSetup)
	{
		bank.PushGroup("Explosion Controls", false);
		{
			bank.AddButton("Drop Explosion", datCallback(CFA(CMarketingTools::WidgetDropExplosion)));
			ms_ExplosionSetup->AddWidgets(bank);
		}
		bank.PopGroup();
	}
	ms_LightControlGroup = bank.PushGroup("Light Controls", false);
	{
		//bank.AddButton("Drop Light", datCallback(CFA(CMarketingTools::WidgetDropLight)));
		bank.AddButton("Add Light", datCallback(CFA(CMarketingTools::WidgetAddLight)));
		bank.AddSlider("Current Light", &ms_LightArrayEditIndex, 0, MAX_DEBUG_LIGHTS-1, 1, datCallback(CFA(CMarketingTools::WidgetChangeEditIndex)));
		bank.AddButton("Delete Current Light", datCallback(CFA(CMarketingTools::WidgetDeleteLight)));
	}
	bank.PopGroup();
	bank.PushGroup("Hud Controls");
	{
		bank.AddToggle("Update help text when paused", &CNewHud::bUpdateHelpTextWhenPaused);
		bank.AddButton("Show All", datCallback(CFA(CMarketingTools::WidgetHudShowAll)));
		bank.AddButton("Hide All", datCallback(CFA(CMarketingTools::WidgetHudHideAll)));
		bank.AddToggle("Display Hud", &ms_DisplayHud, datCallback(CFA(CMarketingTools::WidgetChangedHudToggles)));
		bank.AddToggle("Display MiniMap", &ms_DisplayMiniMap, datCallback(CFA(CMarketingTools::WidgetChangedHudToggles)));
		bank.AddToggle("Display Markers", &ms_DisplayMarkers, datCallback(CFA(CMarketingTools::WidgetChangedHudToggles)));
	}
	bank.PopGroup();
	ms_CharacterControlsGroup = bank.PushGroup("Character Controls");
	{
		bank.AddButton("Add Controls", datCallback(CFA(CMarketingTools::WidgetAddCharacterControls)));
		
	}
	bank.PopGroup();

	bank.PushGroup("Camera Look Around Controls");
	{
		bank.AddSlider("Adjust Control Sensitivity", &camControlHelper::ms_LookAroundSpeedScalar, 0.0f, 10.0f, 0.1f); 
		bank.AddToggle("Override Look Around Settings", &camControlHelper::ms_UseOverriddenLookAroundHelper);
		bank.AddSlider("Input Mag Power", &camControlHelper::ms_OverriddenLookAroundHelper.m_InputMagPowerFactor, 0.0f, 10.0f, 0.1f); 
		bank.AddSlider("Acceleration", &camControlHelper::ms_OverriddenLookAroundHelper.m_Acceleration, 0.0f, 100.0f, 0.1f); 
		bank.AddSlider("Deceleration", &camControlHelper::ms_OverriddenLookAroundHelper.m_Deceleration, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Max Heading Speed", &camControlHelper::ms_OverriddenLookAroundHelper.m_MaxHeadingSpeed, 0.0f, 1000.0f, 0.1f);
		bank.AddSlider("Max Pitch Speed", &camControlHelper::ms_OverriddenLookAroundHelper.m_MaxPitchSpeed, 0.0f, 1000.0f, 0.1f);
	}
	bank.PopGroup();
}

//NOTE: We must initialise the HUD component widgets separately to the other marketing widgets, as we require access to component filenames
//that only become available once the HUD has been initialised.
void CMarketingTools::InitHudComponentWidgets()
{
	bkGroup* parentGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Marketing Tools/Hud Controls"));
	if(!parentGroup)
	{
		return;
	}

	//Clean-up any existing widgets, for safety.
	bkGroup* componentGroup = static_cast<bkGroup*>(BANKMGR.FindWidget("Marketing Tools/Hud Controls/Display Hud Components"));
	if(componentGroup)
	{
		componentGroup->Destroy();
	}

	componentGroup = parentGroup->AddGroup("Display Hud Components", true);
	if(componentGroup)
	{
		for(s32 i=0; i<MAX_NEW_HUD_COMPONENTS; i++)
		{
			const char* componentName = CNewHud::GetHudComponentFileName(i);
			if(componentName)
			{
				componentGroup->AddToggle(componentName, &ms_DisplayHudComponent[i]);
			}
		}
	}
}

void CMarketingTools::CLightDebugSetup::Reset()
{
	m_LightColour.Set(255.0f,255.0f,255.0f);
	m_LightPosition.Zero();
	m_LightDirection.Set(0.0f, 0.0f, 1.0f);
	m_LightTangent.Set(0.0f, 1.0f, 0.0f);
	m_LightRange = 10.0f;
	m_LightInnerCone = 50.0f;
	m_LightOuterCone = 50.0f;
	m_LightConeAngle = 30.0f;
	m_LightIntensity = 10.0f;
	m_LightFalloffExponent = 8.0f;
	m_LightType = 1;
	m_IsCoronaOnly = false;
	m_IsLightAttached = true;
	m_IsLightOrientedToCam = true;
	m_IsDynamicShadowLight = false;
	m_IsFXLight = false;
	m_LightCapsuleExtent = 10.0f;
}


void CMarketingTools::CLightDebugSetup::AddWidgets(bkBank & bank)
{
	bank.AddCombo("Light Type",					&m_LightType, 3, g_LightTypeNames, datCallback(WidgetRefreshLightWidgets));
	if( m_LightType != 0 )
	{
		bank.AddToggle("Orient light to camera",    &m_IsLightOrientedToCam, datCallback(WidgetRefreshLightWidgets));
	}
	bank.AddToggle("Attach light to camera",	&m_IsLightAttached, datCallback(WidgetRefreshLightWidgets));
	if( m_LightType != 2 )
	{
		bank.AddToggle("Use dynamic shadows",		&m_IsDynamicShadowLight);
	}
	bank.AddToggle("Is Corona Only",			&m_IsCoronaOnly);
	bank.AddToggle("Is FX Light",				&m_IsFXLight);

	// common
	bank.AddSlider("Colour",					&m_LightColour,			 0.0f, 255.0f, 1.0f);
	bank.AddSlider("Range",						&m_LightRange,			 1.0f, 50.0f, 1.0f);
	bank.AddSlider("Intensity",					&m_LightIntensity,		 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Falloff speed",				&m_LightFalloffExponent, 0.0f, 64.0f, 0.1f);
	bank.AddSlider("Position",					&m_LightPosition,		 -12000.0f, 12000.0f, 0.1f);
	// type-specific parameters	
	if( m_LightType == 1 )
	{
		bank.AddSlider("Direction",					&m_LightDirection,		-1.0f, 1.0f, 0.01f);
		bank.AddSlider("Inner Cone percentage",		&m_LightInnerCone,		 0.0f, 100.0f, 1.0f);
		bank.AddSlider("Outer Cone percentage",     &m_LightOuterCone,		 0.0f, 100.0f, 1.0f);
		bank.AddSlider("Cone Angle",                &m_LightConeAngle,       0.0f, 179.0f, 1.0f);
	}
	if( m_LightType == 2 )
	{
		bank.AddSlider("Direction",					&m_LightDirection,		-1.0f, 1.0f, 0.01f);
		bank.AddSlider("Capsule extent",			&m_LightCapsuleExtent,    0.0f, 200.0f, 1.0f);
	} 	
}


void CMarketingTools::WidgetAddLight()
{
	if (ms_LightArrayNumUsed<MAX_DEBUG_LIGHTS)
	{
		if (ms_LightArrayNumUsed>0)
		{
			// copy the current light's settings
			ms_LightArray[ms_LightArrayNumUsed] = ms_LightArray[ms_LightArrayEditIndex];
			// detatch the current light from the camera
			ms_LightArray[ms_LightArrayEditIndex].m_IsLightAttached = false;
		}
		else
		{
			// Adding a first light
			ms_LightsEnabled = true;
			ms_LightArray[ms_LightArrayNumUsed].Reset();
		}
		// Set the light position/direction to the current camera position (leave everything else alone)
		//Move the light with the camera frame.
		const Vector3 lightDir(0.0f, 1.0f, 0.0f);
		const Vector3 lightTan(1.0f, 0.0f, 0.0f);
		camInterface::GetMat().Transform3x3(lightDir, ms_LightArray[ms_LightArrayNumUsed].m_LightDirection);
		camInterface::GetMat().Transform3x3(lightTan, ms_LightArray[ms_LightArrayNumUsed].m_LightTangent);
		ms_LightArray[ms_LightArrayNumUsed].m_LightPosition = camInterface::GetPos();
		// attach the new light to the camera
		ms_LightArray[ms_LightArrayNumUsed].m_IsLightAttached = true;
		ms_LightArray[ms_LightArrayNumUsed].m_IsLightOrientedToCam = true;

		ms_LightArrayEditIndex = ms_LightArrayNumUsed;
		ms_LightArrayNumUsed++;

		WidgetRefreshLightWidgets();
	}
}


void CMarketingTools::WidgetDeleteLight()
{
	if (ms_LightArrayNumUsed<=0)
		return;
	ms_LightArrayNumUsed--;
	for(int i=ms_LightArrayEditIndex;i<ms_LightArrayNumUsed;i++)
		ms_LightArray[i] = ms_LightArray[i-1];
	WidgetRefreshLightWidgets();
}


void CMarketingTools::WidgetChangeEditIndex()
{
	WidgetRefreshLightWidgets();
}


void CMarketingTools::WidgetRefreshLightWidgets()
{
	if (ms_LightArrayEditIndex >= ms_LightArrayNumUsed)
		ms_LightArrayEditIndex = ms_LightArrayNumUsed-1;

	bkBank * bank = BANKMGR.FindBank("Marketing Tools");
	if (ms_LightWidgetGroup)
		bank->DeleteGroup(*ms_LightWidgetGroup);
	ms_LightWidgetGroup = NULL;
	if (ms_LightArrayNumUsed>0)
	{
		bank->SetCurrentGroup(*ms_LightControlGroup);

		ms_LightWidgetGroup = bank->PushGroup("Current Light", true);
		ms_LightArray[ms_LightArrayEditIndex].AddWidgets(*bank);
		bank->PopGroup();

		bank->UnSetCurrentGroup(*ms_LightControlGroup);
	}
	else
		ms_LightArrayEditIndex = 0;
}


void CMarketingTools::WidgetDropExplosion()
{
	if (!ms_ExplosionSetup)
		return;

	// put the explosion 1m in front of the camera
	Vector3 dropPos = camInterface::GetPos();
	dropPos += camInterface::GetFront();

	ms_ExplosionSetup->m_explosionPosition = dropPos;
	ms_ExplosionSetup->m_vDirection = camInterface::GetUp();

	CExplosionManager::AddExplosion(*ms_ExplosionSetup);
}

bool CMarketingTools::ShouldDisableBloodForPed(const CPed& ped)
{
	const bool shouldDisableBlood	= (ms_BloodMode == BLOOD_DISABLED_GLOBALLY) || ((ms_BloodMode == BLOOD_DISABLED_FOR_PLAYER) &&
										(CGameWorld::FindFollowPlayer() == &ped));
	return shouldDisableBlood;
}

void CMarketingTools::WidgetHudShowAll()
{
	ms_DisplayHud		= true;
	ms_DisplayMiniMap	= true;
	ms_DisplayMarkers	= true;

	for(s32 i=0; i<MAX_NEW_HUD_COMPONENTS; i++)
	{
		ms_DisplayHudComponent[i] = true;
	}

	WidgetChangedHudToggles();

	ms_WasHudShown = true;
}

void CMarketingTools::WidgetHudHideAll()
{
	ms_DisplayHud		= false;
	ms_DisplayMiniMap	= false;
	ms_DisplayMarkers	= false;

	for(s32 i=0; i<MAX_NEW_HUD_COMPONENTS; i++)
	{
		ms_DisplayHudComponent[i] = false;
	}

	WidgetChangedHudToggles();

	ms_WasHudShown = false;
}

void CMarketingTools::WidgetChangedHudToggles()
{
	CHudTools::bDisplayHud = ms_DisplayHud;
	CMarkers::g_bDisplayMarkers	= ms_DisplayMarkers;

	CMiniMap::SetVisible(ms_DisplayMiniMap);
}

void CMarketingTools::WidgetAddCharacterControls()
{
	if (ms_CharacterWidgetGroup)
		return;

	CAnimViewer::InitModelNameCombo();
	bkBank * bank = BANKMGR.FindBank("Marketing Tools");

	//if (ms_CharacterWidgetGroup)
	//	bank->DeleteGroup(*ms_CharacterWidgetGroup);
	//ms_CharacterWidgetGroup = NULL;

	bank->SetCurrentGroup(*ms_CharacterControlsGroup);
	ms_CharacterWidgetGroup = bank->PushGroup("Character", true);
	{
		// yuck!
		bank->AddCombo("Select Model", &CAnimViewer::m_modelIndex, CAnimViewer::m_modelNames.GetCount(), &CAnimViewer::m_modelNames[0], datCallback(CFA(CAnimViewer::SelectPed)), "Lists the models");
		bank->AddButton("Set Focus to Player", CPedDebugVisualiserMenu::ChangeFocusToPlayer, "Sets the focused entity to be the players ped");
		// Add the weapon/inventory widgets
		CPedInventory::AddWidgets(*bank);
	}
	bank->PopGroup();
	bank->UnSetCurrentGroup(*ms_CharacterControlsGroup);
}


#endif // __BANK
