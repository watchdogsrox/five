
// Rage headers
#include "grcore/debugdraw.h"
#include "input/keyboard.h"
#include "script/wrapper.h"

// Framework header
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxscript.h"
#include "fwscene/stores/txdstore.h"

// Game headers
#include "audio/weaponaudioentity.h"
#include "camera/viewports/ViewportManager.h"
#include "control/replay/ReplayMovieControllerNew.h"
#include "debug/vectormap.h"
#include "frontend/BusySpinner.h"
#include "frontend/GolfTrail.h"
#include "frontend/NewHud.h"
#include "frontend/UIWorldIcon.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormStore.h"
#include "frontend/ui_channel.h"
#include "network/NetworkInterface.h"
#include "network/Events/NetworkEventTypes.h"
#include "peds/ped.h"
#include "Peds/PedIntelligence/PedAiLod.h"
#include "Peds/rendering/PedVariationStream.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/renderer.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/LightEntity.h"
#include "renderer/lights/LODLights.h"
#include "renderer/occlusion.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/postprocessfx.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/PlantsMgr.h"
#include "renderer/Renderphases/RenderPhaseFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "renderer/ScreenshotManager.h"
#include "renderer/UI3DDrawManager.h"
#include "SaveLoad/GenericGameStorage.h"
#include "SaveLoad/savegame_list.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/texLod.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_areas.h"
#include "script/script_helper.h"
#include "script/script_text_construction.h"
#include "shaders/CustomShaderEffectGrass.h"
#include "streaming/streaming.h"
#include "system/ControlMgr.h"
#include "system/TamperActions.h"
#include "vehicles/vehicle.h"
#include "TimeCycle/TimeCycle.h"
#include "timecycle/TimeCycleDebug.h"
#include "Vfx/Decals/DecalManager.h"
#include "vfx/gpuparticles/PtFxGPUManager.h"
#include "Vfx/Metadata/VfxRegionInfo.h"
#include "vfx/Misc/DistantLights.h"
#include "Vfx/Misc/Checkpoints.h"
#include "vfx/Misc/GameGlows.h"
#include "Vfx/Misc/Markers.h"
#include "Vfx/Misc/MovieManager.h"
#include "Vfx/Misc/MovieMeshManager.h"
#include "Vfx/Misc/TerrainGrid.h"
#include "Vfx/Misc/ScriptIM.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxEntity.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/Systems/VfxLiquid.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/Systems/VfxScript.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/Systems/VfxWater.h"
#include "Vfx/systems/VfxWeather.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/Systems/VfxWheel.h"
#include "Vfx/VfxHelper.h"
#include "game/Clock.h"

#include "Text/Text.h"
#include "Text/TextConversion.h"
#include "Text/TextFile.h"
#include "Text/TextFormat.h"
#include "Script/script_channel.h"
#include "Script/script_debug.h"
#include "Script/script_hud.h"

#include "vfx/misc/TVPlaylistManager.h"
#include "control/replay/effects/ParticleMiscFxPacket.h"
#include "control/replay/effects/ParticleWeaponFxPacket.h"
#include "control/replay/effects/ProjectedTexturePacket.h"
#include "control/replay/Misc/InteriorPacket.h"
#include "control/Replay/Misc/ReplayPacket.h"
#include "control/replay/ReplayTrailController.h"

extern ShadowFreezing ShadowFreezer;

namespace graphics_commands
{

	static const s32 framesToLive = -1;	//	We used to use the default of 1

	static atHashWithStringNotFinal g_usePtFxAssetHashName = (u32)0;

#define ENABLE_DEBUG_DRAW __BANK

#if ENABLE_DEBUG_DRAW
#	define DEBUG_DRAW_ONLY(...)  __VA_ARGS__
#else
#	define DEBUG_DRAW_ONLY(...)
#endif

//===========================================================================
// OLD DEPRECATED DEBUG DRAW COMMANDS!!!!
//===========================================================================
	void CommandSetDrawingDebugLinesAndSpheresActive(bool DEBUG_DRAW_ONLY(bActivateLinesAndSpheres))
	{
#if ENABLE_DEBUG_DRAW
		CScriptDebug::SetDrawDebugLinesAndSpheres(bActivateLinesAndSpheres);
		scriptDisplayf("SET_DEBUG_LINES_AND_SPHERES_DRAWING_ACTIVE switched %s by %s", (bActivateLinesAndSpheres? "On" : "Off"), CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandLine(const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors) )
	{
#if ENABLE_DEBUG_DRAW
		Vector3 VecFirstCoors = Vector3 (scrVecFirstCoors);
		Vector3 VecSecondCoors = Vector3 (scrVecSecondCoors);

		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			grcDebugDraw::Line(VecFirstCoors, VecSecondCoors, Color32(0x3f, 0xff, 0x00, 0xff), framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	/*void CommandDrawDebugSphere(const scrVector &DEBUG_DRAW_ONLY(scrVecCoors), float DEBUG_DRAW_ONLY(Radius))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 TempVec = Vector3(scrVecCoors);
			grcDebugDraw::Sphere(TempVec,Radius,Color32(255,0,128,228),false);
		}
#endif // ENABLE_DEBUG_DRAW
	}*/
//===========================================================================
//===========================================================================

//===========================================================================
// NEW SHINY DEBUG DRAW COMMANDS!!!!
//===========================================================================

	//--------------------------------------------------------------------------
	// 3d debug drawing

	void CommandDebugDraw_Line(
		const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vec0 = Vector3 (scrVecFirstCoors);
			Vector3 vec1 = Vector3 (scrVecSecondCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Line(vec0, vec1, colour, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_Line2Colour(
		const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors),
		int DEBUG_DRAW_ONLY(Red0), int DEBUG_DRAW_ONLY(Green0), int DEBUG_DRAW_ONLY(Blue0), int DEBUG_DRAW_ONLY(Alpha0),
		int DEBUG_DRAW_ONLY(Red1), int DEBUG_DRAW_ONLY(Green1), int DEBUG_DRAW_ONLY(Blue1), int DEBUG_DRAW_ONLY(Alpha1))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vec0 = Vector3 (scrVecFirstCoors);
			Vector3 vec1 = Vector3 (scrVecSecondCoors);
			Color32 colour0(Red0, Green0, Blue0, Alpha0);
			Color32 colour1(Red1, Green1, Blue1, Alpha1);

			grcDebugDraw::Line(vec0, vec1, colour0, colour1, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_Poly(
		const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecThirdCoors),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vec0 = Vector3 (scrVecFirstCoors);
			Vector3 vec1 = Vector3 (scrVecSecondCoors);
			Vector3 vec2 = Vector3 (scrVecThirdCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Poly(RCC_VEC3V(vec0), RCC_VEC3V(vec1), RCC_VEC3V(vec2), colour, false, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_Poly3Colour(
		const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecThirdCoors),
		int DEBUG_DRAW_ONLY(Red0), int DEBUG_DRAW_ONLY(Green0), int DEBUG_DRAW_ONLY(Blue0), int DEBUG_DRAW_ONLY(Alpha0),
		int DEBUG_DRAW_ONLY(Red1), int DEBUG_DRAW_ONLY(Green1), int DEBUG_DRAW_ONLY(Blue1), int DEBUG_DRAW_ONLY(Alpha1),
		int DEBUG_DRAW_ONLY(Red2), int DEBUG_DRAW_ONLY(Green2), int DEBUG_DRAW_ONLY(Blue2), int DEBUG_DRAW_ONLY(Alpha2))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vec0 = Vector3 (scrVecFirstCoors);
			Vector3 vec1 = Vector3 (scrVecSecondCoors);
			Vector3 vec2 = Vector3 (scrVecThirdCoors);
			Color32 colour0(Red0, Green0, Blue0, Alpha0);
			Color32 colour1(Red1, Green1, Blue1, Alpha1);
			Color32 colour2(Red2, Green2, Blue2, Alpha2);

			grcDebugDraw::Poly(RCC_VEC3V(vec0), RCC_VEC3V(vec1), RCC_VEC3V(vec2), colour0, colour1, colour2, false, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_Sphere(
		const scrVector &DEBUG_DRAW_ONLY(scrVecCoors),
		float DEBUG_DRAW_ONLY(Radius),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Sphere(VecCoors, Radius, colour, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_BoxAxisAligned(
		const scrVector &DEBUG_DRAW_ONLY(scrVecMinCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecMaxCoors),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vec3V vMin = (Vec3V) scrVecMinCoors;
			Vec3V vMax = (Vec3V) scrVecMaxCoors;
			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::BoxAxisAligned(vMin, vMax, colour, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	// Script equivalents to CommandDebugDraw_ items...
	//		void BoxOriented(const Vector3& vMin, const Vector3& vMax, const Matrix34& mat, const Color32 colour);
	//		void Axis(const Matrix34& mtx, const float scale );
	// ...skipped due to complication of passing matrices.

	void CommandDebugDraw_Cross(
		const scrVector &DEBUG_DRAW_ONLY(scrVecCoors),
		float DEBUG_DRAW_ONLY(Size),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Cross(RCC_VEC3V(VecCoors), Size, colour, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	// 3d text rendering
	// (text is 2d in screen space but positioned in 3D)
	void CommandDebugDraw_Text(
		const char *DEBUG_DRAW_ONLY(pString),
		const scrVector &DEBUG_DRAW_ONLY(scrVecCoors),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);

			Color32 colour(Red, Green, Blue, Alpha);

			Assertf(pString!=NULL, "DRAW_DEBUG_TEXT: NULL String passed");

			grcDebugDraw::Text(VecCoors, colour, pString, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_TextWithOffset(
		const char *DEBUG_DRAW_ONLY(pString),
		const scrVector &DEBUG_DRAW_ONLY(scrVecCoors),
		int DEBUG_DRAW_ONLY(iScreenSpaceXOffset),
		int DEBUG_DRAW_ONLY(iScreenSpaceYOffset),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);

			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Text(VecCoors, colour, iScreenSpaceXOffset, iScreenSpaceYOffset, pString, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	s32 CommandDebugDraw_GetScreenSpaceTextHeight()
	{
#if ENABLE_DEBUG_DRAW
		s32 height = grcDebugDraw::GetScreenSpaceTextHeight();
		return height;
#else
		return 0;
#endif // ENABLE_DEBUG_DRAW
	}

	//--------------------------------------------------------------------------
	// 2d debug drawing
	// All functions are expecting (0,0) to (1,1) space coordinates unless specified

	void CommandDebugDraw_Line2D(
		const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector2 vec0(scrVecFirstCoors.x, scrVecFirstCoors.y);
			Vector2 vec1(scrVecSecondCoors.x, scrVecSecondCoors.y);
			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Line(vec0, vec1, colour, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_Line2D2Colour(
		const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors),
		int DEBUG_DRAW_ONLY(Red0), int DEBUG_DRAW_ONLY(Green0), int DEBUG_DRAW_ONLY(Blue0), int DEBUG_DRAW_ONLY(Alpha0),
		int DEBUG_DRAW_ONLY(Red1), int DEBUG_DRAW_ONLY(Green1), int DEBUG_DRAW_ONLY(Blue1), int DEBUG_DRAW_ONLY(Alpha1))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector2 vec0(scrVecFirstCoors.x, scrVecFirstCoors.y);
			Vector2 vec1(scrVecSecondCoors.x, scrVecSecondCoors.y);
			Color32 colour0(Red0, Green0, Blue0, Alpha0);
			Color32 colour1(Red1, Green1, Blue1, Alpha1);

			grcDebugDraw::Line(vec0, vec1, colour0, colour1, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_Poly2D(
		const scrVector &DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecSecondCoors),
		const scrVector &DEBUG_DRAW_ONLY(scrVecThirdCoors),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector2 vec0(scrVecFirstCoors.x, scrVecFirstCoors.y);
			Vector2 vec1(scrVecSecondCoors.x, scrVecSecondCoors.y);
			Vector2 vec2(scrVecThirdCoors.x, scrVecThirdCoors.y);
			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Poly(vec0, vec1, vec2, colour, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	void CommandDebugDraw_Poly2D3Colour(
		const scrVector & DEBUG_DRAW_ONLY(scrVecFirstCoors),
		const scrVector & DEBUG_DRAW_ONLY(scrVecSecondCoors),
		const scrVector & DEBUG_DRAW_ONLY(scrVecThirdCoors),
		int DEBUG_DRAW_ONLY(Red0), int DEBUG_DRAW_ONLY(Green0), int DEBUG_DRAW_ONLY(Blue0), int DEBUG_DRAW_ONLY(Alpha0),
		int DEBUG_DRAW_ONLY(Red1), int DEBUG_DRAW_ONLY(Green1), int DEBUG_DRAW_ONLY(Blue1), int DEBUG_DRAW_ONLY(Alpha1),
		int DEBUG_DRAW_ONLY(Red2), int DEBUG_DRAW_ONLY(Green2), int DEBUG_DRAW_ONLY(Blue2), int DEBUG_DRAW_ONLY(Alpha2))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector2 vec0(scrVecFirstCoors.x, scrVecFirstCoors.y);
			Vector2 vec1(scrVecSecondCoors.x, scrVecSecondCoors.y);
			Vector2 vec2(scrVecThirdCoors.x, scrVecThirdCoors.y);
			Color32 colour0(Red0, Green0, Blue0, Alpha0);
			Color32 colour1(Red1, Green1, Blue1, Alpha1);
			Color32 colour2(Red2, Green2, Blue2, Alpha2);

			grcDebugDraw::Poly(vec0, vec1, vec2, colour0, colour1, colour2, true, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	// 2d text rendering
	// The passed in vectors are 3D but we only pay attention the to x and y components.
	void CommandDebugDraw_Text2D(
		const char *DEBUG_DRAW_ONLY(pString),
		const scrVector & DEBUG_DRAW_ONLY(scrVecCoors),
		int DEBUG_DRAW_ONLY(Red), int DEBUG_DRAW_ONLY(Green), int DEBUG_DRAW_ONLY(Blue), int DEBUG_DRAW_ONLY(Alpha))
	{
#if ENABLE_DEBUG_DRAW
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector2 VecCoors2D(scrVecCoors.x, scrVecCoors.y);

			Color32 colour(Red, Green, Blue, Alpha);

			grcDebugDraw::Text(VecCoors2D, colour, pString, true, 1.0f, 1.0f, framesToLive);
		}
#endif // ENABLE_DEBUG_DRAW
	}

	//--------------------------------------------------------------------------
	// Vector map (2d, but world space coordinates) debug drawing

	void CommandVectorMapDraw_Line(
		const scrVector & DEV_ONLY(scrVecFirstCoors),
		const scrVector & DEV_ONLY(scrVecSecondCoors),
		int DEV_ONLY(Red0), int DEV_ONLY(Green0), int DEV_ONLY(Blue0), int DEV_ONLY(Alpha0),
		int DEV_ONLY(Red1), int DEV_ONLY(Green1), int DEV_ONLY(Blue1), int DEV_ONLY(Alpha1))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vec0(scrVecFirstCoors.x, scrVecFirstCoors.y, 0.0f);
			Vector3 vec1(scrVecSecondCoors.x, scrVecSecondCoors.y, 0.0f);
			Color32 colour0(Red0, Green0, Blue0, Alpha0);
			Color32 colour1(Red1, Green1, Blue1, Alpha1);

			CVectorMap::DrawLine(vec0, vec1, colour0, colour1);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_LineThick(
		const scrVector & DEV_ONLY(scrVecFirstCoors),
		const scrVector & DEV_ONLY(scrVecSecondCoors),
		int DEV_ONLY(Red0), int DEV_ONLY(Green0), int DEV_ONLY(Blue0), int DEV_ONLY(Alpha0),
		int DEV_ONLY(Red1), int DEV_ONLY(Green1), int DEV_ONLY(Blue1), int DEV_ONLY(Alpha1))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vec0(scrVecFirstCoors.x, scrVecFirstCoors.y, 0.0f);
			Vector3 vec1(scrVecSecondCoors.x, scrVecSecondCoors.y, 0.0f);
			Color32 colour0(Red0, Green0, Blue0, Alpha0);
			Color32 colour1(Red1, Green1, Blue1, Alpha1);

			CVectorMap::DrawLineThick(vec0, vec1, colour0, colour1);
		}
#endif // __DEV && __BANK
	}
	
	void CommandVectorMapDraw_Circle(
		const scrVector & DEV_ONLY(scrVecCoors),
		float DEV_ONLY(Radius),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			CVectorMap::DrawCircle(VecCoors, Radius, colour, false);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_Poly(
		const scrVector & DEV_ONLY(scrVecFirstCoors),
		const scrVector & DEV_ONLY(scrVecSecondCoors),
		const scrVector & DEV_ONLY(scrVecThirdCoors),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vec0 = Vector3 (scrVecFirstCoors);
			Vector3 vec1 = Vector3 (scrVecSecondCoors);
			Vector3 vec2 = Vector3 (scrVecThirdCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			CVectorMap::DrawPoly(vec0, vec1, vec2, colour);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_FilledRect(
		const scrVector & DEV_ONLY(scrVecMinCoors),
		const scrVector & DEV_ONLY(scrVecMaxCoors),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 vMin = Vector3 (scrVecMinCoors);
			Vector3 vMax = Vector3 (scrVecMaxCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			CVectorMap::DrawRectAxisAligned(vMin, vMax, colour, true);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_Wedge(
		const scrVector & DEV_ONLY(scrVecCoors),
		float DEV_ONLY(radiusInner),
		float DEV_ONLY(radiusOuter),
		float DEV_ONLY(thetaStart),
		float DEV_ONLY(thetaEnd),
		int DEV_ONLY(numSegments),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			CVectorMap::DrawWedge(VecCoors, radiusInner, radiusOuter, thetaStart, thetaEnd, numSegments, colour);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_Marker(
		const scrVector & DEV_ONLY(scrVecCoors),
		float DEV_ONLY(Size),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			CVectorMap::DrawMarker(VecCoors, colour, Size);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_Vehicle(
		int DEV_ONLY(vehIndex),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(vehIndex);

			if(pVehicle)
			{
				Color32 colour(Red, Green, Blue, Alpha);

				CVectorMap::DrawVehicle(pVehicle, colour);
			}
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_Ped(
		int DEV_ONLY(pedIndex),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(pedIndex, false);

			if(pPed)
			{
				Color32 colour(Red, Green, Blue, Alpha);

				CVectorMap::DrawPed(pPed, colour);
			}
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_LocalPlayerCamera(
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Color32 colour(Red, Green, Blue, Alpha);
			CVectorMap::DrawLocalPlayerCamera(colour);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_EventRipple(
		const scrVector & DEV_ONLY(scrVecCoors),
		float DEV_ONLY(finalRadius),
		int DEV_ONLY(lifetimeMs),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			CVectorMap::MakeEventRipple(VecCoors, finalRadius, lifetimeMs, colour);
		}
#endif // __DEV && __BANK
	}

	void CommandVectorMapDraw_Text(
		const char *DEV_ONLY(pString),
		const scrVector & DEV_ONLY(scrVecCoors),bool DEV_ONLY(bDrawBGQuad),
		int DEV_ONLY(Red), int DEV_ONLY(Green), int DEV_ONLY(Blue), int DEV_ONLY(Alpha))
	{
#if __DEV && __BANK
		if (grcDebugDraw::g_allowScriptDebugDraw && CScriptDebug::GetDrawDebugLinesAndSpheres())
		{
			Vector3 VecCoors = Vector3 (scrVecCoors);
			Color32 colour(Red, Green, Blue, Alpha);

			CVectorMap::DrawString(VecCoors, pString, colour, bDrawBGQuad);
		}
#endif // __DEV && __BANK
	}
//===========================================================================
//
//
//
//
grcTexture* GetTextureFromStreamedTxd(const char* pStreamedTxdName, const char* pNameOfTexture)
{
	grcTexture *pTexture = NULL;
	strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlot(pStreamedTxdName));
	if (scriptVerifyf(txdId != -1, "%s:GetTextureFromStreamedTxd - Invalid texture dictionary name %s - the txd must be inside an img file that the game loads", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName))
	{
		if (scriptVerifyf(g_TxdStore.IsValidSlot(txdId), "%s:GetTextureFromStreamedTxd - failed to get valid txd with name %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName))
		{

#if __ASSERT
			// If the TXD is part of an image, check whether it's been requested by this script
			if (g_TxdStore.IsObjectInImage(txdId))
			{
				if (CTheScripts::GetCurrentGtaScriptHandler())
				{
					scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_TEXTURE_DICTIONARY, txdId.Get()), "GetTextureFromStreamedTxd - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName);
				}
			}
//			else
//			{
//				scriptDisplayf("%s:GetTextureFromStreamedTxd - %s isn't in an image file so it must have been downloaded. For now, we'll just assume that this script can safely use this txd. Eventually we'll treat downloaded txd's as CGameScriptResource's", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName);
//			}
#endif	//	__ASSERT

			fwTxd* pTxd = g_TxdStore.Get(txdId);
			if (scriptVerifyf(pTxd, "%s:GetTextureFromStreamedTxd - Texture dictionary %s not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName))
			{
				pTexture = pTxd->Lookup(pNameOfTexture);
			}
		}
	}
	
	if (!pTexture)
	{
		scriptAssertf(0, "%s:GetTextureFromStreamedTxd - texture %s is not in texture dictionary %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pNameOfTexture, pStreamedTxdName);
		pTexture = grcTextureFactory::GetInstance().Create("none");
	}

	return pTexture;
}

//===========================================================================
void CommandDraw_Line(const scrVector & p1,const scrVector & p2, int r, int g, int b, int a)
{
	Vec3V vec0 = Vec3V(p1.x, p1.y, p1.z);
	Vec3V vec1 = Vec3V(p2.x, p2.y, p2.z);
	Color32 colour(r, g, b, a);

	ScriptIM::Line(vec0, vec1, colour);
}

void CommandDraw_Poly(const scrVector & p1, const scrVector & p2, const scrVector & p3, int r, int g, int b, int a)
{
	Vec3V vec0 = Vec3V(p1.x, p1.y, p1.z);
	Vec3V vec1 = Vec3V(p2.x, p2.y, p2.z);
	Vec3V vec2 = Vec3V(p3.x, p3.y, p3.z);
	Color32 colour(r, g, b, a);

	ScriptIM::Poly(vec0, vec1, vec2, colour);
}

void CommandDraw_TexturedPoly(const scrVector & p1, const scrVector & p2, const scrVector & p3, int r, int g, int b, int a,
								const char *pTextureDictionaryName, const char *pTextureName,
								const scrVector& svUV1, const scrVector& svUV2, const scrVector& svUV3	)
{
	grcTexture* pTexture = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);
	Assert(pTexture);

	Vec3V vec0 = Vec3V(p1.x, p1.y, p1.z);
	Vec3V vec1 = Vec3V(p2.x, p2.y, p2.z);
	Vec3V vec2 = Vec3V(p3.x, p3.y, p3.z);
	Color32 colour(r, g, b, a);

	Vector2 uv1 = Vector2(svUV1.x, svUV1.y);
	Vector2 uv2 = Vector2(svUV2.x, svUV2.y);
	Vector2 uv3 = Vector2(svUV3.x, svUV3.y);
	
	ScriptIM::PolyTex(vec0, vec1, vec2, colour, pTexture, uv1, uv2, uv3);

#if GTA_REPLAY
	atHashString texDict(pTextureDictionaryName);
	atHashString tex(pTextureName);
	CReplayTrailController::GetInstance().AddPolyFromGame(texDict.GetHash(), tex.GetHash(), vec0, vec1, vec2, colour, colour, colour, uv1, uv2, uv3);
#endif // GTA_REPLAY
}

void CommandDraw_Poly3Colours(const scrVector & p1, const scrVector & p2, const scrVector & p3,
								int r1, int g1, int b1, int a1,
								int r2, int g2, int b2, int a2,
								int r3, int g3, int b3, int a3								)
{
	Vec3V vec0 = Vec3V(p1.x, p1.y, p1.z);
	Vec3V vec1 = Vec3V(p2.x, p2.y, p2.z);
	Vec3V vec2 = Vec3V(p3.x, p3.y, p3.z);
	Color32 colour1(r1, g1, b1, a1);
	Color32 colour2(r2, g2, b2, a2);
	Color32 colour3(r3, g3, b3, a3);

	ScriptIM::Poly(vec0, vec1, vec2, colour1, colour2, colour3);
}

void CommandDraw_TexturedPoly3Colours(const scrVector & p1, const scrVector & p2, const scrVector & p3,
								const scrVector& rgb1, int a1,
								const scrVector& rgb2, int a2,
								const scrVector& rgb3, int a3,
								const char *pTextureDictionaryName, const char *pTextureName,
								const scrVector& svUV1, const scrVector& svUV2, const scrVector& svUV3)
{
	grcTexture* pTexture = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);
	Assert(pTexture);

	Vec3V vec0 = Vec3V(p1.x, p1.y, p1.z);
	Vec3V vec1 = Vec3V(p2.x, p2.y, p2.z);
	Vec3V vec2 = Vec3V(p3.x, p3.y, p3.z);

	Color32 colour1((int)rgb1.x, (int)rgb1.y, (int)rgb1.z, a1);
	Color32 colour2((int)rgb2.x, (int)rgb2.y, (int)rgb2.z, a2);
	Color32 colour3((int)rgb3.x, (int)rgb3.y, (int)rgb3.z, a3);

	Vector2 uv1 = Vector2(svUV1.x, svUV1.y);
	Vector2 uv2 = Vector2(svUV2.x, svUV2.y);
	Vector2 uv3 = Vector2(svUV3.x, svUV3.y);
	
	ScriptIM::PolyTex(vec0, vec1, vec2, colour1, colour2, colour3, pTexture, uv1, uv2, uv3);

#if GTA_REPLAY
	atHashString texDict(pTextureDictionaryName);
	atHashString tex(pTextureName);
	CReplayTrailController::GetInstance().AddPolyFromGame(texDict.GetHash(), tex.GetHash(), vec0, vec1, vec2, colour1, colour2, colour3, uv1, uv2, uv3);
#endif // GTA_REPLAY
}

void CommandDraw_Box( const scrVector &scrVecMinCoors, const scrVector &scrVecMaxCoors, int r, int g, int b, int a)
{
	Vec3V vMin = (Vec3V) scrVecMinCoors;
	Vec3V vMax = (Vec3V) scrVecMaxCoors;
	Color32 colour(r, g, b, a);

	ScriptIM::BoxAxisAligned(vMin, vMax, colour);
}

void CommandSet_BackFaceCulling(bool on)
{
	ScriptIM::SetBackFaceCulling(on);
}


void CommandSet_DepthWriting(bool on)
{
	ScriptIM::SetDepthWriting(on);
}


// --- Mission Creator photo commands -------------------------------------------------------------------------------

bool CommandBeginTakeMissionCreatorPhoto()
{
	return CPhotoManager::RequestTakeMissionCreatorPhoto();
}

s32 CommandGetStatusOfTakeMissionCreatorPhoto()
{
	return CPhotoManager::GetTakeMissionCreatorPhotoStatus();
}

void CommandFreeMemoryForMissionCreatorPhoto()
{
	CPhotoManager::RequestFreeMemoryForMissionCreatorPhoto();
}


bool CommandSaveMissionCreatorPhoto(const char *pFilename)
{
	return CPhotoManager::RequestSaveMissionCreatorPhoto(pFilename);
}

s32 CommandGetStatusOfSaveMissionCreatorPhoto(const char *pFilename)
{
	return (s32) CPhotoManager::GetSaveMissionCreatorPhotoStatus(pFilename);
}


bool CommandLoadMissionCreatorPhoto(const char* szContentID, int nFileID, int nFileVersion, int nLanguage)
{
	return CPhotoManager::RequestLoadMissionCreatorPhoto(szContentID, nFileID, nFileVersion, static_cast<rlScLanguage>(nLanguage));
}

s32 CommandGetStatusOfLoadMissionCreatorPhoto(const char *szContentID)
{
	return (s32) CPhotoManager::GetLoadMissionCreatorPhotoStatus(szContentID);
}

bool CommandBeginCreateMissionCreatorPhotoPreview()
{
	return CPhotoManager::RequestCreateMissionCreatorPhotoPreview(CPhotoManager::GetQualityOfMissionCreatorPhotoPreview());
}

s32 CommandGetStatusOfCreateMissionCreatorPhotoPreview()
{
	return CPhotoManager::GetStatusOfCreateMissionCreatorPhotoPreview(CPhotoManager::GetQualityOfMissionCreatorPhotoPreview());
}

void CommandFreeMemoryForMissionCreatorPhotoPreview()
{
	CPhotoManager::RequestFreeMemoryForMissionCreatorPhotoPreview();
}

// --- Photo gallery commands -------------------------------------------------------------------------------

bool CommandBeginTakeHighQualityPhoto()
{
	return CPhotoManager::RequestTakeCameraPhoto();
}

s32 CommandGetStatusOfTakeHighQualityPhoto()
{
	return CPhotoManager::GetTakeCameraPhotoStatus();
}

void CommandFreeMemoryForHighQualityPhoto()
{
//	CHighQualityScreenshot::FreeMemoryForHighQualityPhoto();
	CPhotoManager::RequestFreeMemoryForHighQualityCameraPhoto();
}

void CommandSetTakenPhotoIsAMugshot(bool bIsMugshot)
{
	CPhotoManager::SetCurrentPhonePhotoIsMugshot(bIsMugshot);
}

void CommandSetArenaThemeAndVariationForTakenPhoto(s32 arenaTheme, s32 arenaVariation)
{
	CPhotoManager::SetArenaThemeAndVariationForCurrentPhoto(arenaTheme, arenaVariation);
}

void CommandSetOnIslandXForTakenPhoto(bool bOnIslandX)
{
	CPhotoManager::SetOnIslandXForCurrentPhoto(bOnIslandX);
}

bool CommandSaveHighQualityPhoto(s32 /*PhotoSlotIndex*/)
{
	//	Should this just save to the first free photo slot?
	//	if none are free, is it okay to overwrite the oldest photo?
//	return CScriptHud::BeginPhotoSave();	//	 PhotoSlotIndex, false);

#if USE_SCREEN_WATERMARK
	if (PostFX::IsWatermarkEnabled() == false)
	{
		return CPhotoManager::RequestSaveCameraPhoto();
	}
	else
	{
		return true;
	}
#else
	return CPhotoManager::RequestSaveCameraPhoto();
#endif
}


s32 CommandGetStatusOfSaveHighQualityPhoto()
{
//	return (s32) CScriptHud::GetPhotoSaveStatus();
#if USE_SCREEN_WATERMARK
	if (PostFX::IsWatermarkEnabled() == false)
	{
		return (s32) CPhotoManager::GetSaveCameraPhotoStatus();
	}
	else
	{
		return (s32)MEM_CARD_COMPLETE;
	}
#else
	return (s32) CPhotoManager::GetSaveCameraPhotoStatus();
#endif
}

bool CommandUploadMugshotPhoto(s32 characterSlot)
{
	scriptAssertf(characterSlot >= 0 && characterSlot <= 4, "UPLOAD_MUGSHOT_PHOTO - %s - characterSlot should be between 0 and 4 but it's %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), characterSlot);
	return CPhotoManager::RequestUploadMugshot(characterSlot);
}

s32 CommandGetStatusOfUploadMugshotPhoto(s32 characterSlot)
{
	scriptAssertf(characterSlot >= 0 && characterSlot <= 4, "GET_STATUS_OF_UPLOAD_MUGSHOT_PHOTO - %s - characterSlot should be between 0 and 4 but it's %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), characterSlot);
	return (s32) CPhotoManager::GetUploadMugshotStatus(characterSlot);
}

bool CommandBeginCreateLowQualityCopyOfPhoto(s32 Quality)
{
	return CPhotoManager::RequestCreateLowQualityCopyOfCameraPhoto(static_cast<eLowQualityPhotoSetting>(Quality));
}

s32 CommandGetStatusOfCreateLowQualityCopyOfPhoto(s32 Quality)
{
	return CPhotoManager::GetCreateLowQualityCopyOfCameraPhotoStatus(static_cast<eLowQualityPhotoSetting>(Quality));
}

void CommandFreeMemoryForLowQualityPhoto()
{
//	CLowQualityScreenshot::RequestFreeMemoryForLowQualityPhoto();
	CPhotoManager::RequestFreeMemoryForLowQualityPhoto();
}

void CommandDrawLowQualityPhotoToPhone(bool bDrawToPhone, s32 PhotoRotation)
{
	CPhotoManager::DrawLowQualityPhotoToPhone(bDrawToPhone, static_cast<CLowQualityScreenshot::ePhotoRotation>(PhotoRotation) );
//	CLowQualityScreenshot::SetRenderToPhone(bDrawToPhone, static_cast<CLowQualityScreenshot::ePhotoRotation>(PhotoRotation) );
}


s32 CommandGetMaximumNumberOfPhotos()
{
	SCRIPT_ASSERT(0, "GET_MAXIMUM_NUMBER_OF_PHOTOS has been deprecated. Photos are no longer saved to the console's hard drive");
	return 0;
}

s32 CommandGetMaximumNumberOfCloudPhotos()
{
#if __LOAD_LOCAL_PHOTOS
	return NUMBER_OF_LOCAL_PHOTOS;
#else
	return MAX_PHOTOS_TO_LOAD_FROM_CLOUD;
#endif
}

s32 CommandGetCurrentNumberOfCloudPhotos()
{
	return CPhotoManager::GetNumberOfPhotos(false);
}

//	Actually creates a sorted list of all savegame files
bool CommandQueueOperationToCreateSortedListOfPhotos(bool UNUSED_PARAM(bScanForSaving))
{
	return CPhotoManager::RequestCreateSortedListOfPhotos(true, true);
}

s32 CommandGetStatusOfSortedListOperation(bool UNUSED_PARAM(bScanForSaving))
{
	return CPhotoManager::GetCreateSortedListOfPhotosStatus(true, true);
}

void CommandClearStatusOfSortedListOperation()
{
	CPhotoManager::ClearCreateSortedListOfPhotosStatus();
}

//	Use this when loading photos?
bool CommandDoesThisPhotoSlotContainAValidPhoto(s32 UNUSED_PARAM(SlotIndex))
{
	SCRIPT_ASSERT(0, "DOES_THIS_PHOTO_SLOT_CONTAIN_A_VALID_PHOTO has been deprecated. Photos are no longer saved to the console's hard drive");
	return false;
}

//	Use this when saving
//	This could return -1
s32 CommandGetSlotForSavingAPhotoTo()
{
	SCRIPT_ASSERT(0, "GET_SLOT_FOR_SAVING_A_PHOTO_TO has been deprecated. Photos are no longer saved to the console's hard drive");
	return -1;
}

bool CommandLoadHighQualityPhoto(s32 UNUSED_PARAM(PhotoSlotIndex))
{
	SCRIPT_ASSERT(0, "LOAD_HIGH_QUALITY_PHOTO has been deprecated");
	return false;
}

s32 CommandGetLoadHighQualityPhotoStatus(s32 UNUSED_PARAM(PhotoSlotIndex))
{
	SCRIPT_ASSERT(0, "GET_LOAD_HIGH_QUALITY_PHOTO_STATUS has been deprecated");
	return MEM_CARD_ERROR;
}


// --- Light commands -------------------------------------------------------------------------------

void CommandSetHighQualityLightingTechniques(bool bUseHighQuality)
{
	Lights::SetHighQualityTechniques(bUseHighQuality);
}

void CommandDrawLightWithRangeEx(const scrVector & scrVecCoors, int R, int G, int B, float Range, float Intensity, float FalloffExponent)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);

	CLightSource light(
		LIGHT_TYPE_POINT, 
		LIGHTFLAG_FX, 
		VecCoors, 
		Vector3(R / 256.0f, G / 256.0f, B / 256.0f), 
		Intensity, 
		LIGHT_ALWAYS_ON);
	light.SetDirTangent(Vector3(0,0,1), Vector3(0,1,0));
	light.SetRadius(Range);
	light.SetFalloffExponent(FalloffExponent);
	light.SetShadowTrackingId(fwIdKeyGenerator::Get(CTheScripts::GetCurrentGtaScriptThread()));
	Lights::AddSceneLight(light);
}

void CommandDrawLightWithRange(const scrVector & scrVecCoors, int R, int G, int B, float Range, float Intensity)
{
	CommandDrawLightWithRangeEx(scrVecCoors, R, G, B, Range, Intensity, 8.0f);
}

void CommandDrawSpotLight(const scrVector & scrVecCoors, const scrVector & scrVecDirection, int R, int G, int B, float Falloff, float Intensity, float innerConeAngle, float outerConeAngle, float exponent)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);
	Vector3 vLightDirection = Vector3(scrVecDirection);

	CLightSource light(
		LIGHT_TYPE_SPOT, 
		LIGHTFLAG_FX, 
		VecCoors, 
		Vector3(R / 256.0f, G / 256.0f, B / 256.0f), 
		Intensity, 
		LIGHT_ALWAYS_ON);
		
	Vector3 ltan;
	ltan.Cross(vLightDirection, FindMinAbsAxis(vLightDirection));
	ltan.Normalize();
		
	light.SetDirTangent(vLightDirection, ltan);
	light.SetRadius(Falloff);
	light.SetSpotlight(innerConeAngle, outerConeAngle);
	light.SetFalloffExponent(exponent);
	Lights::AddSceneLight(light);
}

void CommandDrawShadowedSpotLight(const scrVector & scrVecCoors, const scrVector & scrVecDirection, int R, int G, int B, float Falloff, float Intensity, float innerConeAngle, float outerConeAngle, float exponent, int lightIndex)
{
	Vector3 VecCoors = Vector3 (scrVecCoors);
	Vector3 vLightDirection = Vector3(scrVecDirection);

	u32 lightFlags = 
		LIGHTFLAG_FX | 
		LIGHTFLAG_CAST_SHADOWS | 
		LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS | 
		LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | 
		LIGHTFLAG_MOVING_LIGHT_SOURCE;

	CLightSource light(
		LIGHT_TYPE_SPOT, 
		lightFlags,
		VecCoors, 
		Vector3(R / 256.0f, G / 256.0f, B / 256.0f), 
		Intensity, 
		LIGHT_ALWAYS_ON);

	Vector3 ltan;
	ltan.Cross(vLightDirection, FindMinAbsAxis(vLightDirection));
	ltan.Normalize();

	light.SetDirTangent(vLightDirection, ltan);
	light.SetRadius(Falloff);
	light.SetSpotlight(innerConeAngle, outerConeAngle);
	light.SetFalloffExponent(exponent);
	
	atHashValue hash = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId().GetScriptNameHash();
	light.SetShadowTrackingId(hash.GetHash() + lightIndex);

	Lights::AddSceneLight(light);
}


void CommandFadeUpPedLight(const float seconds)
{
	Lights::StartPedLightFadeUp(seconds);
}

void CommandSetLightOverrideMaxIntensityScale(float maxIntensityScale)
{
	Lights::SetLightOverrideMaxIntensityScale(maxIntensityScale);
}

float CommandGetLightOverrideMaxIntensityScale()
{
	return Lights::GetLightOverrideMaxIntensityScale();
}

void CommandUpdateLightsOnEntity(int EntityIndex)
{
	const fwEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<fwEntity>(EntityIndex);

	if (pEntity)
	{
		LightEntityMgr::UpdateLightsForEntity(pEntity);
	}
}

void CommandDrawMarkerEx(int MarkerType, const scrVector & scrVecPosition, const scrVector & scrVecDirection, const scrVector & scrVecRotation, const scrVector & scrVecScale, int colR, int colG, int colB, int colA, bool bounce, bool faceCam, s32 RotOrder, bool rotate, const char * txdName, const char *texName, bool invert, bool usePreAlphaDepth, bool matchEntityRotOrder)
{
	Vector3 rot = (Vector3)scrVecRotation;
	rot.x *= DtoR;
	rot.y *= DtoR;
	rot.z *= DtoR;

	s32 txdIdx = -1;
	s32 texIdx = -1;
	
	if( txdName )
	{
		Assert(texName);
		strStreamingObjectName txdHash = strStreamingObjectName(txdName);
		strStreamingObjectName texHash = strStreamingObjectName(texName);

		txdIdx = g_TxdStore.FindSlotFromHashKey(txdHash.GetHash()).Get();
		texIdx = g_TxdStore.Get(strLocalIndex(txdIdx))->LookupLocalIndex(texHash.GetHash());
	}
	
	// Convert the order of Euler rotations from whatever was passed in to YXZ so that the rest of the marker code can work the same as it always did
	Quaternion quatRotation;
	CScriptEulers::QuaternionFromEulers(quatRotation, rot, static_cast<EulerAngleOrder>(RotOrder));
	rot = CScriptEulers::QuaternionToEulers(quatRotation, EULER_YXZ);

	MarkerInfo_t markerInfo;
	markerInfo.type = (MarkerType_e)MarkerType;
	markerInfo.vPos = VECTOR3_TO_VEC3V((Vector3)scrVecPosition);
	markerInfo.vDir = VECTOR3_TO_VEC3V((Vector3)scrVecDirection);
	markerInfo.vRot = Vec4V(rot.x, rot.y, rot.z, 0.0f);
	markerInfo.vScale = VECTOR3_TO_VEC3V((Vector3)scrVecScale);
	markerInfo.col = Color32(colR, colG, colB, colA);
	markerInfo.txdIdx = txdIdx;
	markerInfo.texIdx = texIdx;
	markerInfo.bounce = bounce;
	markerInfo.rotate = rotate;
	markerInfo.faceCam = faceCam;
	markerInfo.invert = invert;
	markerInfo.usePreAlphaDepth = usePreAlphaDepth;
	markerInfo.matchEntityRotOrder = matchEntityRotOrder;
	
	g_markers.Register(markerInfo);
}

void CommandDrawMarker(int MarkerType, const scrVector & scrVecPosition, const scrVector & scrVecDirection, const scrVector & scrVecRotation, const scrVector & scrVecScale, int colR, int colG, int colB, int colA, bool bounce, bool faceCam, s32 RotOrder, bool rotate, const char * txdName, const char *texName, bool invert)
{
	CommandDrawMarkerEx(MarkerType, scrVecPosition, scrVecDirection, scrVecRotation, scrVecScale, colR, colG, colB, colA, bounce, faceCam, RotOrder, rotate, txdName, texName, invert, true, false);
}

void CommandDrawMarkerSphere(const scrVector & scrVecPosition, float size, int colR, int colG, int colB, float intensity)
{
	Vec3V pos(scrVecPosition.x,scrVecPosition.y,scrVecPosition.z);
	GameGlows::AddFullScreenGameGlow(pos, size, Color32(colR,colG,colB), intensity, false, true, true);
}

bool CommandGetClosestPlaneMarkerOrientation(const scrVector& scrVecPosition, Vector3& scrVecRight, Vector3& scrVecForward, Vector3& scrVecUp)
{
	Mat34V vMtx(V_IDENTITY);
	Vec3V vPos(scrVecPosition.x,scrVecPosition.y,scrVecPosition.z);
	if (g_markers.GetClosestMarkerMtx(MARKERTYPE_PLANE, vPos, vMtx))
	{
		scrVecRight = VEC3V_TO_VECTOR3(vMtx.GetCol0());
		scrVecForward = VEC3V_TO_VECTOR3(vMtx.GetCol1());
		scrVecUp = VEC3V_TO_VECTOR3(vMtx.GetCol2());

		return true;
	}

	return false;
}

int CommandCreateCheckpoint(int CheckpointType, const scrVector & scrVecPosition, const scrVector & scrVecPointAt, float fSize, int colR, int colG, int colB, int colA, int num)
{
	CScriptResource_Checkpoint checkpoint(CheckpointType, Vector3(scrVecPosition), Vector3(scrVecPointAt), fSize, colR, colG, colB, colA, num);

	return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetId(checkpoint);
}

void CommandSetCheckpointInsideCylinderHeightScale(int UniqueCheckpointIndex, float insideCylinderHeightScale)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetInsideCylinderHeightScale(pScriptResource->GetReference(), insideCylinderHeightScale);
	}
}

void CommandSetCheckpointInsideCylinderScale(int UniqueCheckpointIndex, float insideCylinderScale)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetInsideCylinderScale(pScriptResource->GetReference(), insideCylinderScale);
	}
}

void CommandSetCheckpointCylinderHeight(int UniqueCheckpointIndex, float cylinderHeightMin, float cylinderHeightMax, float cylinderHeightDist)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetCylinderHeight(pScriptResource->GetReference(), cylinderHeightMin, cylinderHeightMax, cylinderHeightDist);
	}
}

void CommandSetCheckpointRGBA(int UniqueCheckpointIndex, int colR, int colG, int colB, int colA)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetRGBA(pScriptResource->GetReference(), static_cast<u8>(colR), static_cast<u8>(colG), static_cast<u8>(colB), static_cast<u8>(colA));
	}
}

void CommandSetCheckpointRGBA2(int UniqueCheckpointIndex, int colR, int colG, int colB, int colA)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetRGBA2(pScriptResource->GetReference(), static_cast<u8>(colR), static_cast<u8>(colG), static_cast<u8>(colB), static_cast<u8>(colA));
	}
}

void CommandSetCheckpointClipPlane_PosNorm(int UniqueCheckpointIndex, const scrVector & scrVecPosition, const scrVector & scrVecNormal)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetClipPlane(pScriptResource->GetReference(), static_cast<Vec3V>(scrVecPosition), static_cast<Vec3V>(scrVecNormal));
	}
}

void CommandSetCheckpointClipPlane_PlaneEq(int UniqueCheckpointIndex, float planeEqX, float planeEqY, float planeEqZ, float planeEqW)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetClipPlane(pScriptResource->GetReference(), Vec4V(planeEqX, planeEqY, planeEqZ, planeEqW));
	}
}

void CommandGetCheckpointClipPlane_PlaneEq(int UniqueCheckpointIndex, float& planeEqX, float& planeEqY, float& planeEqZ, float& planeEqW)
{
	planeEqX = 0.0f;
	planeEqY = 0.0f;
	planeEqZ = 0.0f;
	planeEqW = 0.0f;

	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		Vec4V vClipPlane = g_checkpoints.GetClipPlane(pScriptResource->GetReference());

		planeEqX = vClipPlane.GetXf();
		planeEqY = vClipPlane.GetYf();
		planeEqZ = vClipPlane.GetZf();
		planeEqW = vClipPlane.GetWf();
	}
}

void CommandSetCheckpointForceOldArrowPointing(int UniqueCheckpointIndex)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetForceOldArrowPointing(pScriptResource->GetReference());
	}
}

void CommandSetCheckpointDecalRotAlignedToCamRot(int UniqueCheckpointIndex)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetDecalRotAlignedToCamRot(pScriptResource->GetReference());
	}
}

void CommandSetCheckpointPreventRingFacingCam(int UniqueCheckpointIndex)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetPreventRingFacingCam(pScriptResource->GetReference());
	}
}

void CommandSetCheckpointForceDirection(int UniqueCheckpointIndex)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetForceDirection(pScriptResource->GetReference());
	}
}

void CommandSetCheckpointDirection(int UniqueCheckpointIndex, const scrVector & scrVecPointAt)
{
	scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(UniqueCheckpointIndex);
	if (pScriptResource)
	{
		g_checkpoints.SetDirection(pScriptResource->GetReference(), static_cast<Vec3V>(scrVecPointAt));
	}
}

void CommandDeleteCheckpoint(int UniqueCheckpointIndex)
{
	CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(UniqueCheckpointIndex, false, true, CGameScriptResource::SCRIPT_RESOURCE_CHECKPOINT);
}

void CommandDontRenderInGameUI(bool val)
{
	CVfxHelper::SetDontRenderInGameUI(val);	
}

void CommandForceRenderInGameUI(bool val)
{
	CVfxHelper::SetForceRenderInGameUI(val);	
}

//	Stream a texture dictionary from an img file
void CommandRequestStreamedTxd(const char *pStreamedTxdName, bool bPriority)
{
	strLocalIndex index = g_TxdStore.FindSlot(pStreamedTxdName);
	if(scriptVerifyf(index != -1, "%s: REQUEST_STREAMED_TEXTURE_DICT - Invalid texture dictionary name %s - the txd must be inside an img file that the game loads", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName))
	{
		CScriptResource_TextureDictionary textDict(index, bPriority ? u32(STRFLAG_PRIORITY_LOAD) : 0);

#if __ASSERT
		strIndex streamingIndex = g_TxdStore.GetStreamingIndex(index);
		if (streamingIndex.IsValid())
		{
			strStreamingInfo& info = strStreamingEngine::GetInfo().GetStreamingInfoRef(streamingIndex);
			size_t size = info.ComputeVirtualSize(streamingIndex, true) + info.ComputePhysicalSize(streamingIndex, true);
			scriptAssertf(size < 5 * 1024 * 1024, "Script streaming a large texture dictionary %s, %" SIZETFMT "dKb greater than 5Mb",
				pStreamedTxdName, size / 1024);
		}
#endif // __ASSERT


		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(textDict);
	}
}

//	Check if the requested txd has finished loading
bool HasStreamedTxdLoaded(const char* pStreamedTxdName)
{
	strLocalIndex index = g_TxdStore.FindSlot(pStreamedTxdName);
	if(scriptVerifyf(index != -1, "%s: HAS_STREAMED_TEXTURE_DICT_LOADED - Invalid texture dictionary name %s - the txd must be inside an img file that the game loads", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName))
	{
		if (CTheScripts::GetCurrentGtaScriptHandler())
		{
			scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_TEXTURE_DICTIONARY, index.Get()), "HAS_STREAMED_TEXTURE_DICT_LOADED - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName);
		}

		return CStreaming::HasObjectLoaded(index, g_TxdStore.GetStreamingModuleId());
	}
	return false;
}

void CommandMarkStreamedTxdAsNoLongerNeeded(const char* pStreamedTxdName)
{
	s32 index = g_TxdStore.FindSlot(pStreamedTxdName).Get();
	if(scriptVerifyf(index != -1, "%s: SET_STREAMED_TEXTURE_DICT_AS_NO_LONGER_NEEDED - Invalid texture dictionary name %s - the txd must be inside an img file that the game loads", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName))
	{
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_TEXTURE_DICTIONARY, static_cast<ScriptResourceRef>(index));
	}
}

//
// name:		GetIsWidescreen
// description:	Returns if game is running in widescreen
bool GetIsWidescreen()
{
#if __DEV
	return (CNewHud::bForceScriptWidescreen);  // dev builds return this flag, defaults to GRCDEVICE.GetWideScreen().  This is so script can toggle and refine things without rebooting the game
#else
	return CHudTools::GetWideScreen();
#endif
}

//
// name:		GetIsWidescreen
// description:	Returns if game is running in widescreen
bool GetIsHiDef()
{
#if __DEV
	return (CNewHud::bForceScriptHD);  // dev builds return this flag, defaults to GRCDEVICE.GetHiDef().  This is so script can toggle and refine things without rebooting the game
#else
	return GRCDEVICE.GetHiDef();
#endif
}

void CommandAdjustNextPosSizeAsNormalized16_9()
{
	CScriptHud::ms_bAdjustNextPosSize = true;
}

void CommandForceLoadingScreen(bool bOn)
{
	DEV_ONLY(scriptWarningf("DISPLAY_LOADING_SCREEN_NOW:: \"%s\" on \"%s\"", bOn ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGtaOldLoadingScreen::ForceLoadingRenderFunction(bOn);
}

void CommandSetNightVision(bool bOn)
{
	PostFX::SetUseNightVision(bOn);
}

bool CommandGetRequestingNighVision()
{
	return PostFX::GetGlobalUseNightVision();
}

bool CommandGetUsingNightVision()
{
	return PostFX::GetUseNightVision();
}

void CommandSetExposureTweak(bool bOn)
{
	PostFX::SetUseExposureTweak(bOn);
}

bool CommandGetUsingExposureTweak()
{
	return PostFX::GetUseExposureTweak();
}

void CommandForceExposureReadback(bool bOn)
{
	PostFX::ForceExposureReadback(bOn);
}

void CommandOverrideNVLightRange(float range)
{
	CVisualEffects::OverrideNightVisionLightRange(range);
}

void CommandSetSeeThrough(bool bOn)
{
	g_WeaponAudioEntity.ToggleThermalVision(bOn);
	RenderPhaseSeeThrough::SetState(bOn);

#if GTA_REPLAY
	//Record if the thermal vision is active for replay playback

	// Removing for B*2185265 * because thermal vision sections are now prevented from being recorded.
	// If this is to go back in, the packet will also need a "previous" state as well as the new state (since it can be set to off when already off).
	//CReplayMgr::RecordFx<CPacketWeaponThermalVision>(CPacketWeaponThermalVision(bOn));
#endif
}

bool CommandGetUsingSeeThrough()
{
	return RenderPhaseSeeThrough::GetState();
}

void CommandSeeThroughResetEffect()
{
	RenderPhaseSeeThrough::UpdateVisualDataSettings();
}
	
float CommandSeeThroughGetFadeStartDistance()
{
	return RenderPhaseSeeThrough::GetFadeStartDistance();
}

void CommandSeeThroughSetFadeStartDistance(float value)
{
	RenderPhaseSeeThrough::SetFadeStartDistance(value);
}

float CommandSeeThroughGetFadeEndDistance()
{
	return RenderPhaseSeeThrough::GetFadeEndDistance();
}

void CommandSeeThroughSetFadeEndDistance(float value)
{
	RenderPhaseSeeThrough::SetFadeEndDistance(value);
}

float CommandSeeThroughGetMaxThickness()
{
	return RenderPhaseSeeThrough::GetMaxThickness();
}

void CommandSeeThroughSetMaxThickness(float value)
{
	RenderPhaseSeeThrough::SetMaxThickness(value);
}

float CommandSeeThroughGetMinNoiseAmount()
{
	return RenderPhaseSeeThrough::GetMinNoiseAmount();
}

void CommandSeeThroughSetMinNoiseAmount(float value)
{
	RenderPhaseSeeThrough::SetMinNoiseAmount(value);
}

float CommandSeeThroughGetMaxNoiseAmount()
{
	return RenderPhaseSeeThrough::GetMaxNoiseAmount();
}

void CommandSeeThroughSetMaxNoiseAmount(float value)
{
	RenderPhaseSeeThrough::SetMaxNoiseAmount(value);
}

float CommandSeeThroughGetHiLightIntensity()
{
	return RenderPhaseSeeThrough::GetHiLightIntensity();
}

void CommandSeeThroughSetHiLightIntensity(float value)
{
	RenderPhaseSeeThrough::SetHiLightIntensity(value);
}

float CommandSeeThroughGetHiLightNoise()
{
	return RenderPhaseSeeThrough::GetHiLightNoise();
}

void CommandSeeThroughSetHiLightNoise(float value)
{
	RenderPhaseSeeThrough::SetHiLightNoise(value);
}

void CommandSeeThroughSetHeatScale(int idx,float scale)
{
	RenderPhaseSeeThrough::SetHeatScale((unsigned int)idx,scale);
}

float CommandSeeThroughGetHeatScale(int idx)
{
	return RenderPhaseSeeThrough::GetHeatScale((unsigned int)idx);
}

void CommandSeeThroughSetColorNear(int r, int g, int b)
{
	RenderPhaseSeeThrough::SetColorNear(Color32(r,g,b));
}

void CommandSeeThroughGetColorNear(int &r, int &g, int &b)
{
	Color32 col = RenderPhaseSeeThrough::GetColorNear();
	r = col.GetRed();
	g = col.GetGreen();
	b = col.GetBlue();
}

void CommandSeeThroughSetColorFar(int r, int g, int b)
{
	RenderPhaseSeeThrough::SetColorFar(Color32(r,g,b));
}

void CommandSeeThroughGetColorFar(int &r, int &g, int &b)
{
	Color32 col = RenderPhaseSeeThrough::GetColorFar();
	r = col.GetRed();
	g = col.GetGreen();
	b = col.GetBlue();
}

void CommandSetNoiseOverride(bool bOn)
{
	PostFX::SetNoiseOverride(bOn);
}

void CommandSetNoisinessOverride(float noisiness)
{
	PostFX::SetNoisinessOverride(noisiness);
}

bool CommandGetScreenPosFromWorldCoord(const scrVector & vWorldPos, float &NormalisedX, float &NormalisedY)
{
	Vector3 vecWorldPos(vWorldPos);
	return CScriptHud::GetScreenPosFromWorldCoord(vecWorldPos, NormalisedX, NormalisedY);
}

void CommandSetDistanceBlurStrengthOverride(float val)
{
	PostFX::SetDistanceBlurStrengthOverride(val);
}

void CommandResetAdaptation(int numFrames)
{
	PostFX::ResetAdaptedLuminance(numFrames);
}

bool CommandTriggerScreenBlurFadeIn(float duration)
{
	return PostFX::TriggerScreenBlurFadeIn(duration);
}

bool CommandTriggerScreenBlurFadeOut(float duration)
{
	return PostFX::TriggerScreenBlurFadeOut(duration);
}

void CommandDisableScreenBlurFade()
{
	PostFX::DisableScreenBlurFade();
}

float CommandGetScreenBlurFadeCurrentTime() 
{
	return PostFX::GetScreenBlurFadeCurrentTime();
}

bool CommandIsScreenBlurFadeRunning()
{
	return PostFX::IsScreenBlurFadeRunning();
}

void CommandSetSniperSightOverride(bool enableOverride, bool enableDof, float dofNearStart, float dofNearEnd, float dofFarStart, float dofFarEnd)
{
	Vector4 dofParams(dofNearStart, dofNearEnd, dofFarStart, dofFarEnd);
	PostFX::SetSniperSightDOFOverride(enableOverride, enableDof, dofParams);
}

void CommandSetHighDOFOverride(bool enableOverride, bool enableDof, float dofNearStart, float dofNearEnd, float dofFarStart, float dofFarEnd)
{
	Vector4 dofParams(dofNearStart, dofNearEnd, dofFarStart, dofFarEnd);
	PostFX::SetScriptHiDOFOverride(enableOverride, enableDof, dofParams);
}

void CommandSetLockAdaptiveDofDistance(bool lock)
{
	PostFX::LockAdaptiveDofDistance(lock);
}

// --- Procedural Grass -----------------------------------------------------------------------------
#if CPLANT_DYNAMIC_CULL_SPHERES
int CommandGrassAddCullSphere(const scrVector & center, float radius)
{
	u32 result = gPlantMgr.AddDynamicCullSphere(spdSphere(static_cast<Vec3V>(center), LoadScalar32IntoScalarV(radius)));
	return static_cast<int>((result != PLANTSMGR_INVALID_DYNAMIC_CULL_SPHERE_INDEX ? result + 1 : 0));
}

void CommandGrassRemoveCullSphere(int)
{
	if(Verifyf(handle > 0, "WARNING: CommandGrassRemoveCullSphere received an invalid handle! Please make sure your cull sphere handle is valid!"))
	{
		gPlantMgr.RemoveDynamicCullSphere(static_cast<int>(handle - 1));
	}
}

void CommandGrassResetCullSpheres()
{
	gPlantMgr.ResetDynamicCullSpheres();
}
#else
int CommandGrassAddCullSphere(const scrVector &, float) { return(0); }
void CommandGrassRemoveCullSphere(int)			{ }
void CommandGrassResetCullSpheres()				{ }
#endif //CPLANT_DYNAMIC_CULL_SPHERES...


void CommandProcGrassEnableCullSphere(int idx, const scrVector & center, float radius)
{
	Vector4 cullSphere(center.x, center.y, center.z, radius);
	gPlantMgr.EnableCullSphere(idx, cullSphere);
}

void CommandProcGrassDisableCullSphere(int idx)
{
	gPlantMgr.DisableCullSphere(idx);
}

bool CommandProcGrassIsCullSphereEnabled(int idx)
{
	return gPlantMgr.IsCullSphereEnabled(idx);
}

void CommandProcGrassEnableAmbScaleScan()
{
	gPlantMgr.EnableAmbScaleScan();
}

void CommandProcGrassDisableAmbScaleScan()
{
	gPlantMgr.DisableAmbScaleScan();
}

void CommandDisableProcObjectsCreation()
{
	gPlantMgr.DisableObjCreation();
}

void CommandEnableProcObjectsCreation()
{
	gPlantMgr.EnableObjCreation();
}

void CommandGrassBatchEnableFlatteningExtInAABB(const scrVector &scrVecMin, const scrVector &scrVecMax, const scrVector &scrVecLook, float groundZ)
{
	Vec3V boxMin(scrVecMin.x, scrVecMin.y, scrVecMin.z);
	Vec3V boxMax(scrVecMax.x, scrVecMax.y, scrVecMax.z);
	Vec3V look(scrVecLook.x, scrVecLook.y, scrVecLook.z);
	CCustomShaderEffectGrass::SetScriptVehicleFlatten(spdAABB(boxMin, boxMax), look, ScalarV(groundZ));
}

void CommandGrassBatchEnableFlatteningInAABB(const scrVector &scrVecMin, const scrVector &scrVecMax, const scrVector &scrVecLook)
{
	CommandGrassBatchEnableFlatteningExtInAABB(scrVecMin, scrVecMax, scrVecLook, FLT_MAX);
}

void CommandGrassBatchEnableFlatteningExtInSphere(const scrVector &scrVecCenter, float scrRadius, const scrVector &scrVecLook, float groundZ)
{
	Vec3V center(scrVecCenter.x, scrVecCenter.y, scrVecCenter.z);
	Vec3V look(scrVecLook.x, scrVecLook.y, scrVecLook.z);
	CCustomShaderEffectGrass::SetScriptVehicleFlatten(spdSphere(center, LoadScalar32IntoScalarV(scrRadius)), look, ScalarV(groundZ));
}

void CommandGrassBatchEnableFlatteningInSphere(const scrVector &scrVecCenter, float scrRadius, const scrVector &scrVecLook)
{
	CommandGrassBatchEnableFlatteningExtInSphere(scrVecCenter, scrRadius, scrVecLook, FLT_MAX);
}

void CommandGrassBatchDisableFlattening()
{
	CCustomShaderEffectGrass::ClearScriptVehicleFlatten();
}

void CommandSetFlash(float fMinExposure, float fMaxExposure, int rampUpDuration, int holdDuration, int rampDownDuration)
{
	if (rampUpDuration < 0)		{ rampUpDuration = 0; }
	if (holdDuration < 0)		{ holdDuration = 0; }
	if (rampDownDuration < 0)	{ rampDownDuration = 0; }
	PostFX::SetFlashParameters(fMinExposure, fMaxExposure, (u32)rampUpDuration, (u32)holdDuration, (u32)rampDownDuration);
}

void CommandSetMotionBlurMatrixOverride(const scrVector & scrVecWorldPos, const scrVector & scrVecNewRot, int RotOrder)
{
	Vector3 vRot(scrVecNewRot);
	Vector3 vPos(scrVecWorldPos);

	Matrix34 worldMatrix;
	worldMatrix.d = vPos;
	CScriptEulers::MatrixFromEulers(worldMatrix, vRot * DtoR, static_cast<EulerAngleOrder>(RotOrder));

	PostFX::SetMotionBlurPrevMatrixOverride(worldMatrix);
}

void CommandSetMotionBlurMaxVelocityScaler(float newScaler)
{
	PostFX::SetMotionBlurMaxVelocityScale(newScaler);
}

float CommandGetMotionBlurMaxVelocityScaler()
{
	return PostFX::GetMotionBlurMaxVelocityScale();
}

void CommandSetForceMotionBlur(bool force)
{
	PostFX::SetForceMotionBlur(force);
}

void CommandToggleDamageOverlay(bool bEnable)
{
	PostFX::ToggleBulletImpact(bEnable);
}


// --- Occlusion Queries ---------------------------------------------------------------------------------
int CommandCreateTrackedPoint()
{
	int queryId = OcclusionQueries::OQAllocate();
	scriptAssertf(queryId != 0, "%s::CREATE_TRACKED_POINT : Out of Occlusion Query Ids",CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	return queryId;
}

void CommandSetTrackedPointInfo(int queryId, const scrVector & vPos, float radius)
{
	scriptAssertf(queryId != 0, "%s::SET_TRACKED_POINT_INFO : Setting info on an unallocated query",CTheScripts::GetCurrentScriptNameAndProgramCounter());
	
	if( queryId )
	{
		const Vec3V minV(ScalarV(-radius * 0.5f));
		const Vec3V maxV(ScalarV(radius * 0.5f));
		Mat34V matrix(V_IDENTITY);
		const Vec3V pos(vPos.x, vPos.y, vPos.z);
		matrix.Setd(pos);
	
		OcclusionQueries::OQSetBoundingBox(queryId, minV, maxV, matrix);
	}
}


bool CommandIsTrackedPointVisible(int queryId)
{
	scriptAssertf(queryId != 0, "%s::IS_TRACKED_POINT_VISIBLE : Requesting visibility from an unallocated query",CTheScripts::GetCurrentScriptNameAndProgramCounter());

	int pixelCount = 0;
	
	if( queryId )
	{
		pixelCount = OcclusionQueries::OQGetQueryResult(queryId);
	}
	return (pixelCount > 10); // 10 pixels should be enough for everybody.
}

void CommandDestroyTrackedPointPosition(int queryId )
{
	scriptAssertf(queryId != 0, "%s::DESTROY_TRACKED_POINT : Destroying an unallocated query",CTheScripts::GetCurrentScriptNameAndProgramCounter());
	if( queryId)
	{
		OcclusionQueries::OQFree(queryId);
	}
}

// --- Sprite commands -----------------------------------------------------------------------------------

void CommandUseMask(bool bUseMask, bool bInvertMask)
{
#if __BANK
	if (CScriptDebug::GetDebugMaskCommands())
	{
		uiDisplayf("%s called SET_MASK_ACTIVE. bInvertMask=%s, CScriptHud::scriptTextRenderID = %d, CScriptHud::ms_iCurrentScriptGfxDrawProperties = %d", bInvertMask?"true":"false", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CScriptHud::scriptTextRenderID, CScriptHud::ms_iCurrentScriptGfxDrawProperties);
	}
#endif	//	__BANK

//	CScriptHud::GetIntroRects().GetWriteBuf().IntroRectangles[CScriptHud::GetIntroRects().GetWriteBuf().NumberOfIntroRectanglesThisFrame].SetUseMask(bUseMask);  // set for this rectangle
	CScriptHud::ms_bUseMaskForNextSprite = bUseMask;
	CScriptHud::ms_bInvertMaskForNextSprite = bInvertMask;
}

void CommandSetMask(float CentreX, float CentreY, float Width, float Height)
{
#if __BANK
	if (CScriptDebug::GetDebugMaskCommands())
	{
		uiDisplayf("%s called SET_MASK_ATTRIBUTES. CScriptHud::scriptTextRenderID = %d, CScriptHud::ms_iCurrentScriptGfxDrawProperties = %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), CScriptHud::scriptTextRenderID, CScriptHud::ms_iCurrentScriptGfxDrawProperties);
	}
#endif	//	__BANK

	Width /= 2.0f;
	Height /= 2.0f;

	float MinX = CentreX - Width;
	float MaxX = CentreX + Width;
	float MinY = CentreY - Height;
	float MaxY = CentreY + Height;

	intro_script_rectangle *pCurrentRect = CScriptHud::GetIntroRects().GetWriteBuf().GetNextFree();
	if(SCRIPT_VERIFY(pCurrentRect, "DRAW_RECT - Attempting to draw too many script graphics this frame"))
	{
		pCurrentRect->SetCommonValues(MinX, MinY, MaxX, MaxY);
		pCurrentRect->SetGfxType(SCRIPT_GFX_MASK);
		pCurrentRect->SetUseMask(false);
	}
}



void CommandDrawRect(float CentreX, float CentreY, float Width, float Height, int R, int G, int B, int A, bool bStereo)
{
	Width /= 2.0f;
	Height /= 2.0f;

	float MinX = CentreX - Width;
	float MaxX = CentreX + Width;
	float MinY = CentreY - Height;
	float MaxY = CentreY + Height;

	intro_script_rectangle *pCurrentRect = CScriptHud::GetIntroRects().GetWriteBuf().GetNextFree();
	if(SCRIPT_VERIFY(pCurrentRect, "DRAW_RECT - Attempting to draw too many script graphics this frame"))
	{
		pCurrentRect->SetCommonValues(MinX, MinY, MaxX, MaxY);
		if (bStereo)
			pCurrentRect->SetGfxType(SCRIPT_GFX_SOLID_COLOUR_STEREO);
		else
			pCurrentRect->SetGfxType(SCRIPT_GFX_SOLID_COLOUR);
		pCurrentRect->SetColour(R, G, B, A);
	}
}


bool IsValidDrawScaleformMovieCall(int iMovieId)
{
	bool isValid = true;
	if (SCRIPT_VERIFY(iMovieId > 0, "DRAW_SCALEFORM_MOVIE - Invalid Scaleform movie id"))
	{
		if (!CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId))
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE: Cannot draw - Movie %s (script id %d) is no longer valid", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			isValid = false;
		}

		if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bActive)
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE: Cannot draw - Movie id %d is no longer active", iMovieId);
			isValid = false;
		}

		if (CScriptHud::ScriptScaleformMovie[iMovieId-1].bDeleteRequested)
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE: Cannot draw - Movie %s (script id %d) has been requested to be removed", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			isValid = false;
		}
	}
	else
	{
		isValid = false;
	}

	return isValid;
}

void CommandDrawScaleformMovieInternal(int iMovieId, int iBGMovieId, float CentreX, float CentreY, float Width, float Height, int R, int G, int B, int A, int StereoFlag)
{
	if(IsValidDrawScaleformMovieCall(iMovieId))
	{
		bool bFullScreen = (Width == 1.0f) && (Height == 1.0f);
		
		Width /= 2.0f;
		Height /= 2.0f;

		float fOffsetHeight = 0.0f;
		float fOffsetWidth = 0.0f;
		const float SIXTEEN_BY_NINE = 16.0f/9.0f;
		if(bFullScreen)
		{
			float fAspectRatio = CHudTools::GetAspectRatio();
			const float fMarginRatio = 0.5f;

			const float fDifferenceInMarginSize = fMarginRatio * (SIXTEEN_BY_NINE - fAspectRatio);

			if(fAspectRatio > (4.0f/3.0f) && fAspectRatio < SIXTEEN_BY_NINE)
			{
				fOffsetHeight = abs((1.0f - (fAspectRatio/(16.0f/9.0f)) - fDifferenceInMarginSize) * 0.5f);
				if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bSkipWidthOffset)
				{
					fOffsetWidth = CScriptHud::ScriptScaleformMovie[iMovieId-1].bForceExactFit ? 0.0f : (1.0f - (fAspectRatio / SIXTEEN_BY_NINE)) * 0.5f;
				}
			}
		}

		float MinX = CentreX - Width - fOffsetWidth;
		float MaxX = CentreX + Width + fOffsetWidth;
		float MinY = CentreY - Height - fOffsetHeight;
		float MaxY = CentreY + Height + fOffsetHeight;

		intro_script_rectangle *pCurrentRect = CScriptHud::GetIntroRects().GetWriteBuf().GetNextFree();
		if(SCRIPT_VERIFY(pCurrentRect, "DRAW_RECT - Attempting to draw too many script graphics this frame"))
		{
			pCurrentRect->SetCommonValues(MinX, MinY, MaxX, MaxY);
			if (StereoFlag == 1)
				pCurrentRect->SetGfxType(SCRIPT_GFX_SCALEFORM_MOVIE_RETICULESTEREO);
			else if (StereoFlag == 2)
				pCurrentRect->SetGfxType(SCRIPT_GFX_SCALEFORM_MOVIE_STEREO);
			else
				pCurrentRect->SetGfxType(SCRIPT_GFX_SCALEFORM_MOVIE);

#if RSG_GEN9			
			pCurrentRect->SetScaleformMovieFG(CScriptHud::ScriptScaleformMovie[iMovieId-1]);
			if ( iBGMovieId > 0 && IsValidDrawScaleformMovieCall( iBGMovieId ))
			{
				pCurrentRect->SetScaleformMovieBG(CScriptHud::ScriptScaleformMovie[iBGMovieId -1]);
			}
			else
			{
				pCurrentRect->InvalidateScaleformMovieBG();
			}
#else
			iBGMovieId = 0;
			pCurrentRect->SetScaleformMovieId(iMovieId-1);
#endif //RSG_GEN9

			pCurrentRect->SetColour(R, G, B, A);
		}

#if RSG_GEN9
		CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderRequestedOnce = true;
#else
		CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderedOnce = true;
		CScriptHud::ScriptScaleformMovie[iMovieId-1].iBackgroundMaskedMovieId = -1;
#endif // RSG_GEN9	
	}
	else
	{
		CScriptHud::ms_bAdjustNextPosSize = false;	// Make sure the next drawn scaleform movie doesn't have this set to true if this is a failed call
	}
}

void CommandDrawScaleformMovie( int iMovieId, float CentreX, float CentreY, float Width, float Height, int R, int G, int B, int A, int StereoFlag )
{
	CommandDrawScaleformMovieInternal( iMovieId, -1, CentreX, CentreY, Width, Height, R, G, B, A, StereoFlag );
}

void CommandDrawScaleformMovieFullScreen(int iMovieId, int R, int G, int B, int A, int StereoFlag)
{
	if(SCRIPT_VERIFY(iMovieId > 0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN - Invalid Scaleform movie id"))
	{
		if (CScriptHud::ScriptScaleformMovie[iMovieId-1].iParentMovie != -1)
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN: Cannot draw - Movie %s (script id %d) is a child movie of %s (script id %d)", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId, CScriptHud::ScriptScaleformMovie[CScriptHud::ScriptScaleformMovie[iMovieId-1].iParentMovie].cFilename, CScriptHud::ScriptScaleformMovie[iMovieId-1].iParentMovie);
			return;
		}

		if (!CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId))
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN: Cannot draw - Movie %s (script id %d) is no longer valid", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			return;
		}

		if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bActive)
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN: Cannot draw - Movie id %d is no longer active", iMovieId);
			return;
		}

		if (CScriptHud::ScriptScaleformMovie[iMovieId-1].bDeleteRequested)
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN: Cannot draw - Movie %s (script id %d) has been requested to be removed", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			return;
		}

#if RSG_GEN9
		if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderRequestedOnce)  // on 1st render send the current display details to the movie
#else
		if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderedOnce)  // on 1st render send the current display details to the movie
#endif
		{
			CNewHud::UpdateDisplayConfig(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, SF_BASE_CLASS_SCRIPT);
		}

		// call the normal command with full screen params:
		CommandDrawScaleformMovie(iMovieId, 0.5f, 0.5f, 1.0f, 1.0f, R, G, B, A, StereoFlag);
	}
}


void CommandDrawScaleformMovieFullScreenMasked(int iMovieId, int iMovieId2, int R, int G, int B, int A)
{
	if(SCRIPT_VERIFY(iMovieId > 0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN_MASKED - Invalid Scaleform movie id"))
	{
		if(SCRIPT_VERIFY(iMovieId2 > 0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN_MASKED - Invalid Scaleform masked movie id"))
		{
			if (CScriptHud::ScriptScaleformMovie[iMovieId-1].iParentMovie != -1)
			{
				scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN_MASKED: Cannot draw - Movie %s (script id %d) is a child movie of %s (script id %d)", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId, CScriptHud::ScriptScaleformMovie[CScriptHud::ScriptScaleformMovie[iMovieId-1].iParentMovie].cFilename, CScriptHud::ScriptScaleformMovie[iMovieId-1].iParentMovie);
				return;
			}

			if (!CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId))
			{
				scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN_MASKED: Cannot draw - Movie %s (script id %d) is no longer valid", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
				return;
			}

			if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bActive)
			{
				scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN_MASKED: Cannot draw - Movie id %d is no longer active", iMovieId);
				return;
			}

			if (CScriptHud::ScriptScaleformMovie[iMovieId-1].bDeleteRequested)
			{
				scriptAssertf(0, "DRAW_SCALEFORM_MOVIE_FULLSCREEN_MASKED: Cannot draw - Movie %s (script id %d) has been requested to be removed", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
				return;
			}

#if RSG_GEN9
			if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderRequestedOnce)  // on 1st render send the current display details to the movie
			{
				CNewHud::UpdateDisplayConfig(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, SF_BASE_CLASS_SCRIPT);
				CNewHud::UpdateDisplayConfig(CScriptHud::ScriptScaleformMovie[iMovieId2-1].iId, SF_BASE_CLASS_SCRIPT);
			}

            // call the normal command with full screen params:
			CommandDrawScaleformMovieInternal(iMovieId, iMovieId2, 0.5f, 0.5f, 1.0f, 1.0f, R, G, B, A, false);
#else
			if (!CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderedOnce)  // on 1st render send the current display details to the movie
			{
				CNewHud::UpdateDisplayConfig(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, SF_BASE_CLASS_SCRIPT);
				CNewHud::UpdateDisplayConfig(CScriptHud::ScriptScaleformMovie[iMovieId2-1].iId, SF_BASE_CLASS_SCRIPT);
			}

			// call the normal command with full screen params:
			CommandDrawScaleformMovie(iMovieId, 0.5f, 0.5f, 1.0f, 1.0f, R, G, B, A, false);

			CScriptHud::ScriptScaleformMovie[iMovieId-1].iBackgroundMaskedMovieId = iMovieId2-1;
#endif // RSG_GEN9
		}
	}
}


void CommandDrawScaleformMovie3D(int iMovieId, const scrVector & vPos, const scrVector & vRot, const scrVector & vScale, const scrVector & vWorldSize, s32 rotOrder)
{
	if(SCRIPT_VERIFY(iMovieId > 0, "DRAW_SCALEFORM_MOVIE - Invalid Scaleform movie id"))
	{
		if (!CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId))
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE: Movie %s (script id %d) is not in a usable state", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			return;
		}

		Vector3 pos = Vector3(vPos);

		// add 90 degrees rotation in x so default scalerform orientation looks down they world y axis
		Vector3 rot = Vector3(( DtoR * (vRot.x+90.0f)), ( DtoR * (vRot.y)), ( DtoR * vRot.z));

		// Convert the order of Euler rotations from whatever was passed in to YXZ so that the rest of the code can work the same as it always did
		Matrix34 matRotation;
		matRotation.Identity();
		CScriptEulers::MatrixFromEulers(matRotation, rot, static_cast<EulerAngleOrder>(rotOrder));
		rot = CScriptEulers::MatrixToEulers(matRotation, EULER_YXZ);

		Vector2 scale = Vector2(vScale.x, vScale.y);
		Vector2 size = Vector2(vWorldSize.x, vWorldSize.y);

		CScaleformMgr::ChangeMovieParams(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, pos, scale, size, rot, GFxMovieView::SM_ExactFit);
		CScaleformMgr::RequestRenderMovie3D(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, false);

#if RSG_GEN9
		CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderRequestedOnce = true;
#else
		CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderedOnce = true;
		CScriptHud::ScriptScaleformMovie[iMovieId-1].iBackgroundMaskedMovieId = -1;
#endif // RSG_GEN9
	}

}

void CommandDrawScaleformMovieAttachedToEntity3D(int iMovieId, int entityIndex, int boneId, const scrVector & vPos, const scrVector & vRot, const scrVector & vScale, const scrVector & vWorldSize, s32 rotOrder)
{
	if(SCRIPT_VERIFY(iMovieId > 0, "DRAW_SCALEFORM_MOVIE - Invalid Scaleform movie id"))
	{
		if (!CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId))
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE: Movie %s (script id %d) is not in a usable state", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			return;
		}

		Vector3 vOffsetPos = Vector3(vPos);
		// add 90 degrees rotation in x so default scalerform orientation looks down they world y axis
		Vector3 vOffsetRot = Vector3(( DtoR * (vRot.x+90.0f)), ( DtoR * vRot.y), ( DtoR * vRot.z));

		// Convert the order of Euler rotations from whatever was passed in to YXZ so that the rest of the code can work the same as it always did
		Matrix34 matRotation;
		matRotation.Identity();
		CScriptEulers::MatrixFromEulers(matRotation, vOffsetRot, static_cast<EulerAngleOrder>(rotOrder));
		vOffsetRot = CScriptEulers::MatrixToEulers(matRotation, EULER_YXZ);


		Vector2 scale = Vector2(vScale.x, vScale.y);
		Vector2 size = Vector2(vWorldSize.x, vWorldSize.y);

		const CEntity* pEntity = CTheScripts::GetEntityToQueryFromGUID<CEntity>(entityIndex);

		if(pEntity)
		{
			// don't allow attachment to parent entities without a drawable
			if (pEntity && pEntity->GetDrawable()==NULL)
			{
				return;
			}

			// derive base transform
			Mat34V vMtxBase;
			CVfxHelper::GetMatrixFromBoneIndex(vMtxBase, pEntity, boneId);

			// set local offset position and rotation
			Mat34V vMtxOffset;
			vMtxOffset.SetIdentity3x3();
			RC_MATRIX34(vMtxOffset).RotateY(vOffsetRot.y);
			RC_MATRIX34(vMtxOffset).RotateX(vOffsetRot.x);
			RC_MATRIX34(vMtxOffset).RotateZ(vOffsetRot.z);
			vMtxOffset.SetCol3(RC_VEC3V(vOffsetPos));

			Mat34V vMtxFinal;
			Transform(vMtxFinal, vMtxBase, vMtxOffset);

			Vec3V vFinalPos = vMtxFinal.GetCol3();
			Vector3 finalPos = RC_VECTOR3(vFinalPos);
			Vector3 finalRot;
			RC_MATRIX34(vMtxFinal).ToEulersXYZ(finalRot);

			CScaleformMgr::ChangeMovieParams(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, finalPos, scale, size, finalRot, GFxMovieView::SM_ExactFit);
			CScaleformMgr::RequestRenderMovie3D(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, false);

#if RSG_GEN9
			CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderRequestedOnce = true;
#else
			CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderedOnce = true;
			CScriptHud::ScriptScaleformMovie[iMovieId-1].iBackgroundMaskedMovieId = -1;
#endif // RSG_GEN9
		}
	}
}

void CommandDrawScaleformMovie3DSolid(int iMovieId, const scrVector & vPos, const scrVector & vRot, const scrVector & vScale, const scrVector & vWorldSize, s32 rotOrder)
{
	if(SCRIPT_VERIFY(iMovieId > 0, "DRAW_SCALEFORM_MOVIE - Invalid Scaleform movie id"))
	{
		if (!CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId))
		{
			scriptAssertf(0, "DRAW_SCALEFORM_MOVIE: Movie %s (script id %d) is not in a usable state", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			return;
		}

		Vector3 pos = Vector3(vPos);

		// add 90 degrees rotation in x so default scalerform orientation looks down they world y axis
		Vector3 rot = Vector3(( DtoR * (vRot.x+90.0f)), ( DtoR * (vRot.y)), ( DtoR * vRot.z));

		// Convert the order of Euler rotations from whatever was passed in to YXZ so that the rest of the code can work the same as it always did
		Matrix34 matRotation;
		matRotation.Identity();
		CScriptEulers::MatrixFromEulers(matRotation, rot, static_cast<EulerAngleOrder>(rotOrder));
		rot = CScriptEulers::MatrixToEulers(matRotation, EULER_YXZ);

		Vector2 scale = Vector2(vScale.x, vScale.y);
		Vector2 size = Vector2(vWorldSize.x, vWorldSize.y);

		CScaleformMgr::ChangeMovieParams(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, pos, scale, size, rot, GFxMovieView::SM_ExactFit);
		CScaleformMgr::RequestRenderMovie3D(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, true);

#if RSG_GEN9
		CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderRequestedOnce = true;
#else
		CScriptHud::ScriptScaleformMovie[iMovieId-1].bRenderedOnce = true;
		CScriptHud::ScriptScaleformMovie[iMovieId-1].iBackgroundMaskedMovieId = -1;
#endif // RSG_GEN9
	}
}

void CommandSetScaleformMovie3DBrightness(int iMovieId, int brightness)
{
	if(SCRIPT_VERIFY(iMovieId > 0, "SET_SCALEFORM_MOVIE_BRIGHTNESS - Invalid Scaleform movie id"))
	{
		if (!CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId))
		{
			scriptAssertf(0, "SET_SCALEFORM_MOVIE_BRIGHTNESS: Movie %s (script id %d) is not in a usable state", CScriptHud::ScriptScaleformMovie[iMovieId-1].cFilename, iMovieId);
			return;
		}

		CScaleformMgr::SetMovie3DBrightness(CScriptHud::ScriptScaleformMovie[iMovieId-1].iId, brightness);
	}

}

void CommandDrawLine(float fStartX, float fStartY, float fEndX, float fEndY, float fWidth, int R, int G, int B, int A)
{
	intro_script_rectangle *pCurrentRect = CScriptHud::GetIntroRects().GetWriteBuf().GetNextFree();
	if(SCRIPT_VERIFY(pCurrentRect, "DRAW_RECT - Attempting to draw too many script graphics this frame"))
	{
		pCurrentRect->SetCommonValues(fStartX, fStartY, fEndX, fEndY);
		pCurrentRect->SetGfxType(SCRIPT_GFX_LINE);
		pCurrentRect->SetRectRotationOrWidth(fWidth);
		pCurrentRect->SetColour(R, G, B, A);
	}
}



void CommandSetScriptGfxDrawBehindPauseMenu(bool bValue)
{
	if (bValue)
	{
		CScriptHud::ms_iCurrentScriptGfxDrawProperties |= SCRIPT_GFX_VISIBLE_WHEN_PAUSED;
	}
	else
	{
		CScriptHud::ms_iCurrentScriptGfxDrawProperties &= ~SCRIPT_GFX_VISIBLE_WHEN_PAUSED;
	}
}



void CommandSetScriptGfxDrawOrder(s32 iDrawOrderValue)
{
/*
ENUM GFX_DRAW_ORDER
GFX_ORDER_BEFORE_HUD_PRIORITY_LOW = 0,
GFX_ORDER_BEFORE_HUD,  // standard
GFX_ORDER_BEFORE_HUD_PRIORITY_HIGH,

GFX_ORDER_AFTER_HUD_PRIORITY_LOW,
GFX_ORDER_AFTER_HUD,  // standard
GFX_ORDER_AFTER_HUD_PRIORITY_HIGH,

GFX_ORDER_AFTER_FADE_PRIORITY_LOW,
GFX_ORDER_AFTER_FADE,  // standard
GFX_ORDER_AFTER_FADE_PRIORITY_HIGH
ENDENUM
*/
	//
	// need to convert the script enum into the new bitmask setup:
	//
	bool bShowBehindPausemenu = (CScriptHud::ms_iCurrentScriptGfxDrawProperties & SCRIPT_GFX_VISIBLE_WHEN_PAUSED)!=0;

	switch (iDrawOrderValue)
	{
		case 0:  // GFX_ORDER_BEFORE_HUD_PRIORITY_LOW
		case 1:  // GFX_ORDER_BEFORE_HUD
		case 2:  // GFX_ORDER_BEFORE_HUD_PRIORITY_HIGH
		{
			CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_BEFORE_HUD;
			break;
		}

		case 3:  // GFX_ORDER_AFTER_HUD_PRIORITY_LOW
		case 4:  // GFX_ORDER_AFTER_HUD
		case 5:  // GFX_ORDER_AFTER_HUD_PRIORITY_HIGH
		{
			CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_HUD;
			break;
		}

		case 6:  // GFX_ORDER_AFTER_FADE_PRIORITY_LOW
		case 7:  // GFX_ORDER_AFTER_FADE
		case 8:  // GFX_ORDER_AFTER_FADE_PRIORITY_HIGH
		{
			CScriptHud::ms_iCurrentScriptGfxDrawProperties = SCRIPT_GFX_ORDER_AFTER_FADE;
			break;
		}

		default:
		{
			scriptAssertf(0, "SET_SCRIPT_GFX_DRAW_ORDER: Invalid GFX_ORDER type passed in (%d)", iDrawOrderValue);
			return;
		}
	}

	// now apply the priority:
	switch (iDrawOrderValue)
	{
		case 0:  // GFX_ORDER_BEFORE_HUD_PRIORITY_LOW
		case 3:  // GFX_ORDER_AFTER_HUD_PRIORITY_LOW
		case 6:  // GFX_ORDER_AFTER_FADE_PRIORITY_LOW
		{
			CScriptHud::ms_iCurrentScriptGfxDrawProperties |= SCRIPT_GFX_ORDER_PRIORITY_LOW;
			break;
		}

		case 2:  // GFX_ORDER_BEFORE_HUD_PRIORITY_HIGH
		case 5:  // GFX_ORDER_AFTER_HUD_PRIORITY_HIGH
		case 8:  // GFX_ORDER_AFTER_FADE_PRIORITY_HIGH
		{
			CScriptHud::ms_iCurrentScriptGfxDrawProperties |= SCRIPT_GFX_ORDER_PRIORITY_HIGH;
			break;
		}
	}

	// if it was set to draw beind pausemenu when this command was called, set it to draw again:
	if (bShowBehindPausemenu)
	{
		CommandSetScriptGfxDrawBehindPauseMenu(true);
	}

#if __BANK
	if (CNewHud::bDebugScriptGfxDrawOrder)
	{
		Displayf("%s: SET_SCRIPT_GFX_DRAW_ORDER set to %d - %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iDrawOrderValue, CScriptHud::ms_iCurrentScriptGfxDrawProperties);
	}
#endif // #if __DEV
}

void CommandSetScriptGfxAlign(s32 alignX, s32 alignY)
{
	CScriptHud::ms_CurrentScriptGfxAlignment.m_alignX = (u8)alignX;
	CScriptHud::ms_CurrentScriptGfxAlignment.m_alignY = (u8)alignY;
}

void CommandSetScriptGfxAlignParameters(float offsetX, float offsetY, float sizeX, float sizeY)
{
	CScriptHud::ms_CurrentScriptGfxAlignment.m_offset.x = offsetX;
	CScriptHud::ms_CurrentScriptGfxAlignment.m_offset.y = offsetY;
	CScriptHud::ms_CurrentScriptGfxAlignment.m_size.x = sizeX;
	CScriptHud::ms_CurrentScriptGfxAlignment.m_size.y = sizeY;
}

void CommandResetScriptGfxAlign()
{
	CScriptHud::ms_CurrentScriptGfxAlignment.Reset();
}

void CommandGetScriptGfxAlignPosition(float x, float y, float& newX, float& newY)
{
	Vector2 posn(x,y);
	Vector2 newPosn = CScriptHud::ms_CurrentScriptGfxAlignment.CalculateHudPosition(posn);
	newX = newPosn.x;
	newY = newPosn.y;
}

float CommandGetSafeZoneSize()
{
	return CHudTools::GetSafeZoneSize();
}

void DrawSpriteTex(grcTexture* pTextureToDraw, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, u32 movieIdx, EScriptGfxType gfxType, Vector2 vCoordU, Vector2 vCoordV, bool bRatioAdjust, bool bARAwareX, bool useNearest)
{
	scriptAssertf(pTextureToDraw || movieIdx != INVALID_MOVIE_HANDLE, "%s:DRAW_SPRITE - Texture does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	Width /= 2.0f;
	Height /= 2.0f;

	float fOffsetHeight = 0.0f;
	float fOffsetWidth = 0.0f;
	const float SIXTEEN_BY_NINE = 16.0f/9.0f;

	float fAspectRatio = CHudTools::GetAspectRatio();
	const float fMarginRatio = 0.5f;

	const float fDifferenceInMarginSize = fMarginRatio * (SIXTEEN_BY_NINE - fAspectRatio);

	if(fAspectRatio < SIXTEEN_BY_NINE && bRatioAdjust)
	{
		fOffsetHeight = abs((1.0f - (fAspectRatio/(16.0f/9.0f)) - fDifferenceInMarginSize) * 0.5f);
		fOffsetWidth = (1.0f - (fAspectRatio / SIXTEEN_BY_NINE)) * 0.5f;
	}

	float MinX = CentreX - Width - fOffsetWidth;
	float MaxX = CentreX + Width + fOffsetWidth;
	float MinY = CentreY - Height - fOffsetHeight;
	float MaxY = CentreY + Height + fOffsetHeight;

	intro_script_rectangle *pCurrentRect = CScriptHud::GetIntroRects().GetWriteBuf().GetNextFree();
	if(SCRIPT_VERIFY(pCurrentRect, "DRAW_SPRITE - Attempting to draw too many script graphics this frame"))
	{
		pCurrentRect->SetCommonValues(MinX, MinY, MaxX, MaxY);
		pCurrentRect->SetARAwareX(bARAwareX);
#if RSG_GEN9
		pCurrentRect->SetPointSample(useNearest);
#else
		useNearest = false;
#endif

		if (gfxType == SCRIPT_GFX_SPRITE_AUTO_INTERFACE)
		{
			gfxType = pCurrentRect->GetRenderID() == CRenderTargetMgr::RTI_MainScreen ? SCRIPT_GFX_SPRITE : SCRIPT_GFX_SPRITE_NON_INTERFACE;
		}

		//if (!strncmp(pTextureToDraw->GetName(),"dart_reti",9))
		//	pCurrentRect->SetGfxType(SCRIPT_GFX_SPRITE_STEREO);
		//else
		pCurrentRect->SetGfxType(gfxType);

		pCurrentRect->SetTexture(pTextureToDraw);
		pCurrentRect->SetRectRotationOrWidth( DtoR * Rotation);
		pCurrentRect->SetColour(R, G, B, A);
		pCurrentRect->SetTextureCoords(vCoordU.x, vCoordU.y, vCoordV.x, vCoordV.y);

		if (movieIdx != INVALID_MOVIE_HANDLE)
		{
			pCurrentRect->SetUseMovie(movieIdx);
		}
	}
}



//
// name:		CommandGetTextureResolution
// description:	Gets the textures resolution
Vector3 CommandGetTextureResolution(const char *pTextureDictionaryName, const char *pTextureName)
{
	Vector3 vTextureRes(0,0,0);
	grcTexture* pTexture = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);

	if (pTexture)
	{
		vTextureRes.x = static_cast<float>(pTexture->GetWidth());
		vTextureRes.y = static_cast<float>(pTexture->GetHeight());
	}

	return vTextureRes;
}

//
//
//
//
bool CommandOverridePedCrewLogoTexture(int pedIndex, const char *pTxdName, const char *pTextureName)
{
	bool success = false;

	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
	if(pPed)
	{
		CPedVariationData& pedVarData = pPed->GetPedDrawHandler().GetVarData();
		const u32 prevTxd = pedVarData.GetOverrideCrewLogoTxdHash();
		const u32 prevTex = pedVarData.GetOverrideCrewLogoTexHash();

		if((pTxdName[0]==0) || (pTextureName[0]==0))
		{
			if(pTxdName[0]==0)
			{
				pedVarData.ClearOverrideCrewLogoTxdHash();
			}

			if(pTextureName[0]==0)
			{
				pedVarData.ClearOverrideCrewLogoTexHash();
			}

			success = true;
		}
		else
		{
			pedVarData.SetOverrideCrewLogoTxdHash(pTxdName);
			pedVarData.SetOverrideCrewLogoTexHash(pTextureName);

			grcTexture *pTexture = NULL;
			const strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlot(pedVarData.GetOverrideCrewLogoTxdHash()));
			if (scriptVerifyf(txdId != -1, "%s:CommandOverridePedCrewLogoTexture - Invalid texture dictionary name %s - the txd must be inside an img file that the game loads", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTxdName))
			{

				if (scriptVerifyf(g_TxdStore.IsValidSlot(txdId), "%s:CommandOverridePedCrewLogoTexture - failed to get valid txd with name %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTxdName))
				{
				#if __ASSERT
					// If the TXD is part of an image, check whether it's been requested by this script
					if (g_TxdStore.IsObjectInImage(txdId))
					{
						if (CTheScripts::GetCurrentGtaScriptHandler())
						{
							scriptAssertf(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_TEXTURE_DICTIONARY, txdId.Get()), "CommandOverridePedCrewLogoTexture - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTxdName);
						}
					}
					//	else
					//	{
					//		scriptDisplayf("%s:CommandOverridePedCrewLogoTexture - %s isn't in an image file so it must have been downloaded. For now, we'll just assume that this script can safely use this txd. Eventually we'll treat downloaded txd's as CGameScriptResource's", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pStreamedTxdName);
					//	}
				#endif	//	__ASSERT

					fwTxd* pTxd = g_TxdStore.Get(txdId);
					if (scriptVerifyf(pTxd, "%s:CommandOverridePedCrewLogoTexture - Texture dictionary %s not loaded", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTxdName))
					{
						pTexture = pTxd->Lookup(pedVarData.GetOverrideCrewLogoTexHash());

						scriptAssertf(pTexture, "%s:CommandOverridePedCrewLogoTexture - crew logo texture %s doesn't exist in texture dictionary %s!", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTextureName, pTxdName);

						if(pTexture)
						{
							success = true;
						}
					}// if(pTxd)...
				} // if(g_TxdStore.IsValidSlot(txdId))...
			}// if(txdId != -1)...
		}

		if (!success)
		{
			// something went wrong, so clear crew logo overrides:
			pedVarData.ClearOverrideCrewLogoTxdHash();
			pedVarData.ClearOverrideCrewLogoTexHash();
		}

		// url:bugstar:3042656 - If the user has UGC restrictions we need to refresh as otherwise the rendering won't be re-enabled
		// due to the added check in RequestStreamPedFilesInternal (look for CheckUserContentPrivileges)
		if ((prevTxd != pedVarData.GetOverrideCrewLogoTxdHash() || prevTex != pedVarData.GetOverrideCrewLogoTexHash())
			&& !CLiveManager::CheckUserContentPrivileges())
		{
			CPedVariationStream::RequestStreamPedFiles(pPed, &pedVarData);
		}
	
	}// if(pPed)...

	return success;
}

void CommandCascadeShadowsInitSession()
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_INIT_SESSION:: \"\" on \"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter()));

	if (g_PlayerSwitch.IsActive())	// don't allow this during a player switch - it uses quadrant shadows
		return;

	CRenderPhaseCascadeShadowsInterface::Script_InitSession();
}

void CommandCascadeShadowsSetCascadeBounds(int cascadeIndex, bool bEnabled, float x, float y, float z, float radiusScale, bool interpolateToDisabled, float interpolationTime)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_CASCADE_BOUNDS:: \"%d,%s,%f,%f,%f,%f\" on \"%s\"", cascadeIndex, bEnabled ? "TRUE" : "FALSE", x, y, z, radiusScale, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBounds(cascadeIndex, bEnabled, x, y, z, radiusScale, interpolateToDisabled, interpolationTime);
}

void CommandCascadeShadowsSetCascadeBoundsSnap(bool DEV_ONLY(bEnable))
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SNAP [DEPRECATED]:: \"%s\" on \"%s\"", bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	//CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsSnap(bEnable);
}

void CommandCascadeShadowsSetCascadeBoundsHFOV(float degrees)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV:: \"%f\" on \"%s\"", degrees, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsHFOV(degrees);
}

void CommandCascadeShadowsSetCascadeBoundsVFOV(float degrees)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV:: \"%f\" on \"%s\"", degrees, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsVFOV(degrees);
}

void CommandCascadeShadowsSetCascadeBoundsScale(float scale)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE:: \"%f\" on \"%s\"", scale, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBoundsScale(scale);
}
void CommandCascadeShadowsSetSplitZExpWeight(float scale)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT:: \"%f\" on \"%s\"", scale, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetSplitZExpWeight(scale);
}
void CommandCascadeShadowsSetBoundPosition(float dist)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_BOUND_POSITION:: \"%f\"", dist));
	CRenderPhaseCascadeShadowsInterface::Script_SetBoundPosition(dist);
}
void CommandCascadeShadowEnableEntityTracker(bool enable)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER:: \"%s\" on \"%s\"", enable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::SetEntityTrackerActive(enable);
}
void CommandCascadeShadowsSetEntityTrackerScale(float scale)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE:: \"%f\" on \"%s\"", scale, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetEntityTrackerScale(scale);
}
void CommandCascadeShadowSetScreenSizeCheckEnabled(bool enable)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_SCREEN_SIZE_CHECK_ENABLED:: \"%s\" on \"%s\"", enable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::SetScreenSizeCheckEnabled(enable);
}

void CommandCascadeShadowsSetWorldHeightUpdate(bool bEnable)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE:: \"%s\" on \"%s\"", bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetWorldHeightUpdate(bEnable);
}

void CommandCascadeShadowsSetWorldHeightMinMax(float h0, float h1)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX:: \"%f,%f\" on \"%s\"", h0, h1, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetWorldHeightMinMax(h0, h1);
}

void CommandCascadeShadowsSetRecvrHeightUpdate(bool bEnable)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE:: \"%s\" on \"%s\"", bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetRecvrHeightUpdate(bEnable);
}

void CommandCascadeShadowsSetRecvrHeightMinMax(float h0, float h1)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX:: \"%f,%f\" on \"%s\"", h0, h1, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetRecvrHeightMinMax(h0, h1);
}

void CommandCascadeShadowsSetDitherRadiusScale(float scale)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE:: \"%f\" on \"%s\"", scale, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetDitherRadiusScale(scale);
}

void CommandCascadeShadowsSetDepthBias(bool bEnable, float depthBias)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_DEPTH_BIAS:: \"%s,%f\" on \"%s\"", bEnable ? "TRUE" : "FALSE", depthBias, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetDepthBias(bEnable, depthBias);
}

void CommandCascadeShadowsSetSlopeBias(bool bEnable, float slopeBias)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_SLOPE_BIAS:: \"%s,%f\" on \"%s\"", bEnable ? "TRUE" : "FALSE", slopeBias, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetSlopeBias(bEnable, slopeBias);
}

void CommandCascadeShadowsSetShadowSampleType(const char* typeStr)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE:: \"%s\" on \"%s\"", typeStr, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetShadowSampleType(typeStr);
}


void CommandCascadeShadowsClearShadowSampleType()
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_CLEAR_SHADOW_SAMPLE_TYPE::\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetShadowSampleType(NULL);
}

void CommandCascadeShadowsSetAircraftMode(bool bEnable)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_AIRCRAFT_MODE:: \"%s\" on \"%s\"", bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetAircraftMode(bEnable);
}

void CommandCascadeShadowsSetDynamicDepthMode(bool bEnable)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE:: \"%s\" on \"%s\"", bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetDynamicDepthMode(bEnable);
}
void CommandCascadeShadowsSetDynamicDepthValue(float distance)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_VALUE:: \"%f\" on \"%s\"",distance, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::SetDynamicDepthValue(distance);
}

void CommandCascadeShadowsSetFlyCameraMode(bool bEnable)
{
	DEV_ONLY(scriptWarningf("CASCADE_SHADOWS_SET_FLY_CAMERA_MODE:: \"%s\" on \"%s\"", bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseCascadeShadowsInterface::Script_SetFlyCameraMode(bEnable);
}

void CommandCascadeShadowsEnableFreezer(bool bEnable)
{
	ShadowFreezer.SetEnabled(bEnable);
}

void CommandWaterReflectionSetHeight(bool bEnable, float height)
{
	DEV_ONLY(scriptWarningf("WATER_REFLECTION_SET_HEIGHT:: \"%s,%f\" on \"%s\"", bEnable ? "TRUE" : "FALSE", height, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseWaterReflectionInterface::SetScriptWaterHeight(bEnable, height);
}

void CommandWaterReflectionSetObjectVisibility(bool bForceVisible)
{
	DEV_ONLY(scriptWarningf("WATER_REFLECTION_SET_SCRIPT_OBJECT_VISIBILITY:: \"%s\" on \"%s\"", bForceVisible ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CRenderPhaseWaterReflectionInterface::SetScriptObjectVisibility(bForceVisible);
}

void CommandGolfTrailSetEnabled(bool bEnabled)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_ENABLED:: \"%s\" on \"%s\"", bEnabled ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailEnabled(bEnabled);
}

void CommandGolfTrailSetPath(
	const scrVector & positionStart,
	const scrVector & velocityStart,
	float     velocityScale,
	float     z1,
	bool      bAscending
	)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_PATH:: on \"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailPath(
		Vec3V(positionStart.x, positionStart.y, positionStart.z),
		Vec3V(velocityStart.x, velocityStart.y, velocityStart.z),
		velocityScale,
		z1,
		bAscending
	);
}

void CommandGolfTrailSetRadius(float radiusStart, float radiusMiddle, float radiusEnd)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_RADIUS:: on \"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailRadius(radiusStart, radiusMiddle, radiusEnd);
}

void CommandGolfTrailSetColour(int rStart, int gStart, int bStart, int aStart, int rMiddle, int gMiddle, int bMiddle, int aMiddle, int rEnd, int gEnd, int bEnd, int aEnd)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_COLOUR:: on \"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailColour(
		Color32(rStart , gStart , bStart , aStart ),
		Color32(rMiddle, gMiddle, bMiddle, aMiddle),
		Color32(rEnd   , gEnd   , bEnd   , aEnd   )
	);
}

void CommandGolfTrailSetTessellation(int numControlPoints, int tessellation)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_TESSELLATION:: \"%d,%d\" on \"%s\"", numControlPoints, tessellation, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailTessellation(numControlPoints, tessellation);
}

void CommandGolfTrailSetFixedControlPointsEnabled(bool bEnable)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_FIXED_CONTROL_POINTS_ENABLED:: \"%s\" on \"%s\"", bEnable ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailFixedControlPointsEnabled(bEnable);
}

void CommandGolfTrailSetFixedControlPoint(int controlPointIndex, const scrVector &position, float radius, int R, int G, int B, int A)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_FIXED_CONTROL_POINT:: \"%s,%f,%f,%f\" on \"%s\"", controlPointIndex, position.x, position.y, position.z, CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailFixedControlPoint(controlPointIndex, Vec3V(position.x, position.y, position.z), radius, Color32(R, G, B, A));
}

void CommandGolfTrailSetShaderParams(float pixelThickness, float pixelExpansion, float fadeOpacity, float fadeExponentBias, float textureFill)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_SHADER_PARAMS:: on \"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailShaderParams(pixelThickness, pixelExpansion, fadeOpacity, fadeExponentBias, textureFill);
}

void CommandGolfTrailSetFacing(bool bFacing)
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_SET_FACING:: \"%s\" on \"%s\"", bFacing ? "TRUE" : "FALSE", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_SetTrailFacing(bFacing);
}

void CommandGolfTrailReset()
{
	//DEV_ONLY(Displayf("GOLF_TRAIL_RESET:: on \"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter()));
	CGolfTrailInterface::Script_ResetTrail();
}

float CommandGolfTrailGetMaxHeight()
{
	return CGolfTrailInterface::Script_GetTrailMaxHeight();
}

scrVector CommandGolfTrailGetVisualControlPoint(int controlPointIndex)
{
	return scrVector(CGolfTrailInterface::Script_GetTrailVisualControlPoint(controlPointIndex));
}

static void CommandDrawSpriteEx(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize, bool bARAwareX, bool useNearest = false)
{
	grcTexture* pTextureToDraw = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);

	EScriptGfxType gfxType = SCRIPT_GFX_SPRITE;

	if (DoStereorize)
		gfxType = SCRIPT_GFX_SPRITE_STEREO;

	if (CPhotoManager::IsNameOfMissionCreatorTexture(pTextureName, pTextureDictionaryName) || (SCRIPTDOWNLOADABLETEXTUREMGR.IsNameOfATexture(pTextureName)) )
	{
		gfxType = SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW;
	}

	DrawSpriteTex(pTextureToDraw, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, INVALID_MOVIE_HANDLE, gfxType, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), false, bARAwareX, useNearest);
}

void CommandDrawSpriteARX(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize, bool useNearest = false)
{
	CommandDrawSpriteEx(pTextureDictionaryName, pTextureName, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, DoStereorize, /*bARAwareX*/true, useNearest);
}

void CommandDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize, bool useNearest = false)
{
	CommandDrawSpriteEx(pTextureDictionaryName, pTextureName, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, DoStereorize, /*bARAwareX*/false, useNearest);
}

void CommandDrawSpriteNamedRT(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool useNearest = false)
{
	grcTexture* pTextureToDraw = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);

	EScriptGfxType gfxType = SCRIPT_GFX_SPRITE_NON_INTERFACE;

	if (CPhotoManager::IsNameOfMissionCreatorTexture(pTextureName, pTextureDictionaryName) || (SCRIPTDOWNLOADABLETEXTUREMGR.IsNameOfATexture(pTextureName)) )
	{
		gfxType = SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW;
	}

	DrawSpriteTex(pTextureToDraw, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, INVALID_MOVIE_HANDLE, gfxType, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), false, false, useNearest);

	REPLAY_ONLY(ReplayMovieControllerNew::OnDrawSprite(pTextureDictionaryName, pTextureName, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, false, CScriptHud::scriptTextRenderID));
}


static void CommandDrawSpriteWithUvEx(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float ux, float uy, float vx, float vy, float Rotation, int R, int G, int B, int A, bool bARAwareX, bool useNearest = false)
{
	grcTexture* pTextureToDraw = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);

	EScriptGfxType gfxType = SCRIPT_GFX_SPRITE_WITH_UV;
	if (CPhotoManager::IsNameOfMissionCreatorTexture(pTextureName, pTextureDictionaryName) || (SCRIPTDOWNLOADABLETEXTUREMGR.IsNameOfATexture(pTextureName)) )
	{
		gfxType = SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW_WITH_UV;
	}

	DrawSpriteTex(pTextureToDraw, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, INVALID_MOVIE_HANDLE, gfxType, Vector2(ux, uy), Vector2(vx, vy), false, bARAwareX, useNearest);
}

void CommandDrawSpriteWithUV(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float ux, float uy, float vx, float vy, float Rotation, int R, int G, int B, int A, bool useNearest = false)
{
	CommandDrawSpriteWithUvEx(pTextureDictionaryName, pTextureName, CentreX, CentreY, Width, Height, ux, uy, vx, vy, Rotation, R, G, B, A, /*bARAwareX*/false, useNearest);
}

void CommandDrawSpriteARXWithUV(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float ux, float uy, float vx, float vy, float Rotation, int R, int G, int B, int A, bool useNearest = false)
{
	CommandDrawSpriteWithUvEx(pTextureDictionaryName, pTextureName, CentreX, CentreY, Width, Height, ux, uy, vx, vy, Rotation, R, G, B, A, /*bARAwareX*/true, useNearest);
}

//
//
//
//
void CommandDrawSprite3DWithUV(const char *pTextureDictionaryName, const char *pTextureName, float X, float Y, float Z, float Width, float Height, float ux, float uy, float vx, float vy, float UNUSED_PARAM(Rotation), int R, int G, int B, int A)
{
	Assert(pTextureDictionaryName);
	Assert(pTextureName);

	grcTexture* pTextureToDraw = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);
	scriptAssertf(pTextureToDraw, "%s:DRAW_SPRITE_3D - Texture does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter());

	if(!pTextureToDraw)
		return;

	const Vec3V vec0 = Vec3V(X,		Y,			Z);
	const Vec3V vec1 = Vec3V(Width,	Height,		0.0f);
	const Vector2 uv0(ux,uy);
	const Vector2 uv1(vx,vy);
	const Color32 colour1(R,G,B,A);

	ScriptIM::SetDepthWriting(false);
	ScriptIM::SetBackFaceCulling(false);
		ScriptIM::Sprite3D(vec0, vec1, colour1, pTextureToDraw, uv0, uv1);
	ScriptIM::SetBackFaceCulling(true);
	//	ScriptIM::SetDepthWriting(true);
}

void CommandDrawSprite3D(const char *pTextureDictionaryName, const char *pTextureName, float X, float Y, float Z, float Width, float Height, float Rotation, int R, int G, int B, int A)
{
	CommandDrawSprite3DWithUV(pTextureDictionaryName, pTextureName, X, Y, Z, Width, Height, 0.0f, 0.0f, 1.0f, 1.0f, Rotation, R, G, B, A);
}

int CommandAddEntityIcon(int entityIndex, const char *pTextureName)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "ADD_ENTITY_ICON - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "ADD_ENTITY_ICON - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Add(pEntity, pTextureName, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
		if(pIcon)
		{
			return pIcon->GetId();
		}
	}

	return INVALID_WORLD_ICON_ID;
}

void CommandRemoveEntityIcon(int entityIndex)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "REMOVE_ENTITY_ICON - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "REMOVE_ENTITY_ICON - Trying to get an icon when the manager doesn't exist yet."))
	{
		SUIWorldIconManager::GetInstance().Remove(pEntity);
	}
}

bool CommandDoesEntityHaveIcon(int entityIndex)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "DOES_ENTITY_HAVE_ICON - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "DOES_ENTITY_HAVE_ICON - Trying to get an icon when the manager doesn't exist yet."))
	{
		return SUIWorldIconManager::GetInstance().Find(pEntity) != NULL;
	}

	return false;
}

void CommandSetEntityIconVisibility(int entityIndex, bool visibility)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "SET_ENTITY_ICON_VISIBILITY - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_VISIBILITY - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(pEntity);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_VISIBILITY - Entity id doesn't have an existing icon"))
		{
			pIcon->SetVisibility(visibility);
		}
	}
}

void CommandSetEntityIconBGVisibility(int entityIndex, bool showBG)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "SET_ENTITY_ICON_BG_VISIBILITY - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_BG_VISIBILITY - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(pEntity);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_BG_VISIBILITY - Entity id doesn't have an existing icon"))
		{
			pIcon->SetShowBG(showBG);
		}
	}
}

void CommandSetShouldEntityIconClampToScreen(int entityIndex, bool shouldClamp)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "SET_USING_ENTITY_ICON_CLAMP - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_USING_ENTITY_ICON_CLAMP - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(pEntity);
		if(SCRIPT_VERIFY(pIcon, "SET_USING_ENTITY_ICON_CLAMP - Entity id doesn't have an existing icon"))
		{
			pIcon->SetClamp(shouldClamp);
		}
	}
}

void CommandSetEntityIconRenderCount(int entityIndex, int renderCount)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "SET_ENTITY_ICON_RENDER_COUNT - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_RENDER_COUNT - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(pEntity);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_RENDER_COUNT - Entity id doesn't have an existing icon"))
		{
			pIcon->SetLifetime(renderCount);
		}
	}
}

void CommandSetEntityIconColor(int entityIndex, int r, int g, int b, int a)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "SET_ENTITY_ICON_COLOR - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_COLOR - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(pEntity);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_COLOR - Entity id doesn't have an existing icon"))
		{
			Color32 color;
			color.SetRed(r);
			color.SetGreen(g);
			color.SetBlue(b);
			color.SetAlpha(a);
			pIcon->SetColor(color);
		}
	}
}

void CommandSetEntityIconBGColor(int entityIndex, int r, int g, int b, int a)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "SET_ENTITY_ICON_BG_COLOR - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_BG_COLOR - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(pEntity);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_BG_COLOR - Entity id doesn't have an existing icon"))
		{
			Color32 color;
			color.SetRed(r);
			color.SetGreen(g);
			color.SetBlue(b);
			color.SetAlpha(a);
			pIcon->SetBGColor(color);
		}
	}
}

void CommandSetEntityIconTextLabel(int entityIndex, const char* pTextLabel)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(SCRIPT_VERIFY(pEntity, "SET_ENTITY_ICON_TEXT_LABEL - Entity id doesn't exist") &&
		SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_TEXT_LABEL - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(pEntity);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_TEXT_LABEL - Entity id doesn't have an existing icon"))
		{
			pIcon->SetText(pTextLabel);
		}
	}
}

int CommandAddEntityIconByVector(float x, float y, float z, const char *pTextureName)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "ADD_ENTITY_ICON - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Add(Vector3(x, y, z), pTextureName, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
		if(pIcon)
		{
			return pIcon->GetId();
		}
	}

	return INVALID_WORLD_ICON_ID;
}

void CommandRemoveEntityIconId(int iconId)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "REMOVE_ENTITY_ICON - Trying to get an icon when the manager doesn't exist yet."))
	{
		SUIWorldIconManager::GetInstance().Remove(iconId);
	}
}

bool CommandDoesEntityHaveIconId(int iconId)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "DOES_ENTITY_HAVE_ICON - Trying to get an icon when the manager doesn't exist yet."))
	{
		return SUIWorldIconManager::GetInstance().Find(iconId) != NULL;
	}

	return false;
}

void CommandSetEntityIconIdVisibility(int iconId, bool visibility)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_VISIBILITY - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(iconId);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_VISIBILITY - Entity id doesn't have an existing icon"))
		{
			pIcon->SetVisibility(visibility);
		}
	}
}

void CommandSetEntityIconIdBGVisibility(int iconId, bool showBG)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_ID_BG_VISIBILITY - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(iconId);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_ID_BG_VISIBILITY - Entity id doesn't have an existing icon"))
		{
			pIcon->SetShowBG(showBG);
		}
	}
}

void CommandSetShouldEntityIconIdClampToScreen(int iconId, bool shouldClamp)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_USING_ENTITY_ICON_CLAMP - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(iconId);
		if(SCRIPT_VERIFY(pIcon, "SET_USING_ENTITY_ICON_CLAMP - Entity id doesn't have an existing icon"))
		{
			pIcon->SetClamp(shouldClamp);
		}
	}
}

void CommandSetEntityIconIdRenderCount(int iconId, int renderCount)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_RENDER_COUNT - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(iconId);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_RENDER_COUNT - Entity id doesn't have an existing icon"))
		{
			pIcon->SetLifetime(renderCount);
		}
	}
}

void CommandSetEntityIconIdColor(int iconId, int r, int g, int b, int a)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_ID_COLOR - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(iconId);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_ID_COLOR - Entity id doesn't have an existing icon"))
		{
			Color32 color;
			color.SetRed(r);
			color.SetGreen(g);
			color.SetBlue(b);
			color.SetAlpha(a);
			pIcon->SetColor(color);
		}
	}
}

void CommandSetEntityIconIdBGColor(int iconId, int r, int g, int b, int a)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_ID_BG_COLOR - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(iconId);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_ID_BG_COLOR - Entity id doesn't have an existing icon"))
		{
			Color32 color;
			color.SetRed(r);
			color.SetGreen(g);
			color.SetBlue(b);
			color.SetAlpha(a);
			pIcon->SetBGColor(color);
		}
	}
}

void CommandSetEntityIconIdTextLabel(int iconId, const char* pTextLabel)
{
	if(SCRIPT_VERIFY(SUIWorldIconManager::IsInstantiated(), "SET_ENTITY_ICON_ID_TEXT_LABEL - Trying to get an icon when the manager doesn't exist yet."))
	{
		UIWorldIcon* pIcon = SUIWorldIconManager::GetInstance().Find(iconId);
		if(SCRIPT_VERIFY(pIcon, "SET_ENTITY_ICON_ID_TEXT_LABEL - Entity id doesn't have an existing icon"))
		{
			pIcon->SetText(pTextLabel);
		}
	}
}

void CommandSetDrawOrigin(const scrVector &vecOrigin, bool bIs2d)
{
	s32 NewOriginIndex = CScriptHud::GetDrawOrigins().GetWriteBuf().GetNextFreeIndex();
	if(SCRIPT_VERIFY(NewOriginIndex >= 0, "SET_DRAW_ORIGIN - too many origins this frame"))
	{
		Vector3 vOrigin(vecOrigin);
		CScriptHud::GetDrawOrigins().GetWriteBuf().Set(NewOriginIndex, vOrigin, bIs2d);
		CScriptHud::ms_IndexOfDrawOrigin = NewOriginIndex;
	}
}

void CommandClearDrawOrigin()
{
	CScriptHud::ms_IndexOfDrawOrigin = -1;
}

int CommandSetBinkMovie(const char *pMovieName)
{
	g_movieMgr.DumpDebugInfo("SetBinkMovie");

	CScriptResource_BinkMovie binkMovie(pMovieName);

	return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(binkMovie);
}

void CommandPlayBinkMovie(int idx)
{
	g_movieMgr.DumpDebugInfo("PlayBinkMovie");

	u32 handle = g_movieMgr.GetHandleFromScriptSlot(idx);
	g_movieMgr.Play(handle);
}

void CommandStopBinkMovie(int idx)
{
	g_movieMgr.DumpDebugInfo("StopBinkMovie");

	u32 handle = g_movieMgr.GetHandleFromScriptSlot(idx);
	g_movieMgr.Stop(handle);
}

void CommandReleaseBinkMovie(int BinkMovieIndex)
{
	g_movieMgr.DumpDebugInfo("ReleaseBinkMovie");

	CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_BINK_MOVIE, BinkMovieIndex);
}

void CommandDrawBinkMovie(int idx, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A)
{
	g_movieMgr.DumpDebugInfo("DrawBinkMovie");

	u32 handle = g_movieMgr.GetHandleFromScriptSlot(idx);
	DrawSpriteTex(NULL, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, handle, SCRIPT_GFX_SPRITE, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), false, false, false);
}

void CommandSetBinkMovieTime(int idx, float fMovieTime)
{
	g_movieMgr.DumpDebugInfo("SetBinkMovieTime");

	u32 handle = g_movieMgr.GetHandleFromScriptSlot(idx);
	g_movieMgr.SetTime(handle, fMovieTime);
}

float CommandGetBinkMovieTime(int idx)
{
	g_movieMgr.DumpDebugInfo("GetBinkMovieTime");

	u32 handle = g_movieMgr.GetHandleFromScriptSlot(idx);
	return g_movieMgr.GetTime(handle);
}

void CommandSetBinkMovieVolume(int idx, float fMovieVolume)
{
	u32 handle = g_movieMgr.GetHandleFromScriptSlot(idx);
	g_movieMgr.SetVolume(handle, fMovieVolume);
}

void CommandAttachBinkAudioToEntity(int movieIndex, int entityIndex)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex);
	if(pEntity)
	{
		u32 handle = g_movieMgr.GetHandleFromScriptSlot(movieIndex);
		g_movieMgr.AttachAudioToEntity(handle, pEntity);
	}
}

void CommandAttachTVAudioToEntity(int entityIndex)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex);
	if(pEntity)
	{
		CTVPlaylistManager::SetEntityForAudio(pEntity);
	}
}

void CommandSetTVAudioFrontend(bool isFrontend)
{
	CTVPlaylistManager::SetIsAudioFrontend(isFrontend);
}

void CommandSetBinkMovieAudioFrontend(int movieIndex, bool frontend)
{
	u32 handle = g_movieMgr.GetHandleFromScriptSlot(movieIndex);
	g_movieMgr.SetFrontendAudio(handle, frontend);
}

void CommandSetBinkShouldSkip(int movieIndex, bool shouldSkip)
{
	g_movieMgr.DumpDebugInfo("SetBinkShouldSkip");

	u32 handle = g_movieMgr.GetHandleFromScriptSlot(movieIndex);
	g_movieMgr.SetShouldSkip(handle, shouldSkip);
}

int CommandLoadMovieMeshSet(const char* pSetName)
{
	CScriptResource_MovieMeshSet movieMeshSet(pSetName);

	return CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(movieMeshSet);
}

void CommandReleaseMovieMeshSet(int scriptID)
{
	CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_MOVIE_MESH_SET, scriptID);
}

int CommandQueryMovieMeshSetStatus(int scriptID)
{
	MovieMeshSetHandle handle = g_movieMeshMgr.GetHandleFromScriptId(scriptID);
	eMovieMeshSetState status = g_movieMeshMgr.QuerySetStatus(handle);
	return (int)status;
}

// --- General --------------------------------------------------------------------------------------

//
// name:		CommandGetScreenResolution
// description:	Gets the current screen resolution - returns 720p to match Actionscript
void CommandGetScreenResolution(int &x, int &y)
{
	// We need to keep this returning 720p as script have hard coded stuff to work with 1280x720 (see uiutil.sch)
	x = (int)1280.0f;  //VideoResManager::GetUIWidth();
	y = (int)720.0f;  //VideoResManager::GetUIHeight();

}

//
// name:		CommandGetActualScreenResolution
// description:	Gets the actual screen resolution - returns 720p to match Actionscript
void CommandGetActualScreenResolution(int &x, int &y)
{
	// We need to keep this returning 720p as script have hard coded stuff to work with 1280x720 (see uiutil.sch)
	x = VideoResManager::GetUIWidth();
	y = VideoResManager::GetUIHeight();

}

//
// name:		CommandGetScreenAspectRatio
// description:	returns the aspect ration (width/height)
float CommandGetScreenAspectRatio()
{
	return ((float)VideoResManager::GetUIWidth() / (float)VideoResManager::GetUIHeight());
}

float CommandGetAspectRatio(bool bPhysicalAspect)
{
	return CHudTools::GetAspectRatio(bPhysicalAspect);
}

//
// name:		CommandGetViewportAspectRatio
// description:	returns the viewport aspect ratio
float CommandGetViewportAspectRatio()
{
	const CViewport* viewport	= gVpMan.GetGameViewport();
	float aspecRatio = viewport ? viewport->GetGrcViewport().GetAspectRatio() : ((float)VideoResManager::GetUIWidth() / (float)VideoResManager::GetUIHeight());

	return aspecRatio;
}


// --- General Vfx  ---------------------------------------------------------------------------------

void CommandTogglePauseRenderPhases(bool on)
{
#if !__FINAL
	scriptDisplayf("Request: TOGGLE_PAUSED_RENDERPHASES(%s)",on ? "TRUE" : "FALSE");
	OUTPUT_ONLY( scrThread::PrePrintStackTrace() );
#endif

	CPauseMenu::TogglePauseRenderPhases(on, OWNER_SCRIPT, __FUNCTION__ );
}

void CommandResetPauseRenderPhases()
{
#if !__FINAL
	scriptDisplayf("RESET_PAUSED_RENDERPHASES");
	OUTPUT_ONLY( scrThread::PrePrintStackTrace() );
#endif

	CPauseMenu::ResetPauseRenderPhases(WIN32PC_ONLY(OWNER_SCRIPT_OVERRIDE));
}

void CommandEnableTogglePauseRenderPhases(bool on)
{
#if !__FINAL
	scriptDisplayf("ENABLE_TOGGLE_PAUSED_RENDERPHASES(%s)",on ? "TRUE" : "FALSE");
	OUTPUT_ONLY( scrThread::PrePrintStackTrace() );
#endif
	CPauseMenu::EnableTogglePauseRenderPhases(on);
}

void CommandGrabPauseMenuOwnership()
{
#if !__FINAL
	scriptDisplayf("GRAB_PAUSEMENU_OWNERSHIP");
	OUTPUT_ONLY( scrThread::PrePrintStackTrace() );
#endif
	CPauseMenu::GrabPauseMenuOwnership(OWNER_SCRIPT);
}

bool CommandGetTogglePauseRenderPhaseStatus()
{
	return !CPauseMenu::GetPauseRenderPhasesStatus();
}

// --- Particle Effects -----------------------------------------------------------------------------

#include "vectormath/classes.h"

bool CommandTriggerPtFxInternal_BoneIndex(const char* pFxName, CEntity* pEntity, bool bCheckEntity, int iBoneIndex, Vector3 vFxPos, Vector3 vFxRot, float fScale, u8 iInvertAxes, bool bLocalOnly, bool bIgnoreScopeChanges)
{
	const char* pScriptName = CTheScripts::GetCurrentScriptName();
	atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);
	if (g_usePtFxAssetHashName != 0)
	{
		ptfxAssetName = g_usePtFxAssetHashName;
	}

	if (ptfxVerifyf(ptfxAssetName.GetHash(), "script is trying to play a particle effect but no particle asset is set up"))
	{
		//Record this data because it is cleared when used in ptfxScript::Trigger locally - needed for transmission across the network
		float r = 0.f, g = 0.f, b = 0.f, alpha = 0.f;
		ptfxScript::GetTriggeredColourTint(r, g, b);
		ptfxScript::GetTriggeredAlphaTint(alpha);

		bool retc = ptfxScript::Trigger(atHashWithStringNotFinal(pFxName), ptfxAssetName, pEntity, iBoneIndex, RC_VEC3V(vFxPos), RC_VEC3V(vFxRot), fScale, iInvertAxes);

#if GTA_REPLAY
		if (retc && CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketTriggeredScriptPtFx>(
				CPacketTriggeredScriptPtFx(atHashWithStringNotFinal(pFxName), ptfxAssetName, iBoneIndex, vFxPos, vFxRot, fScale, iInvertAxes),
				pEntity,
				true);
		}
#endif

		if (retc && !bLocalOnly)
		{
			if (NetworkInterface::IsGameInProgress())
			{
#if !__FINAL
				scriptDebugf1("%s: Triggering PTFX on entity over network: %s (%d) Pos: (%.2f, %.2f, %.2f) Rot: (%.2f, %.2f, %.2f), Scale:%.2f, Invert Axes:%d, boneIndex: %d",
					CTheScripts::GetCurrentScriptNameAndProgramCounter(),
					pFxName,
					atStringHash(pFxName),
					vFxPos.x, vFxPos.y, vFxPos.z,
					vFxRot.x, vFxRot.y, vFxRot.z,
					fScale,
					iInvertAxes,
					iBoneIndex);
				scrThread::PrePrintStackTrace();
#endif // !__FINAL

				bool bProceed = bCheckEntity ? (NetworkUtils::GetNetworkObjectFromEntity(pEntity) && !NetworkUtils::IsNetworkClone(pEntity)) : true;
				if (bProceed)
				{
					CNetworkPtFXEvent::Trigger(
						atHashWithStringNotFinal(pFxName),
						ptfxAssetName,
						pEntity,
						iBoneIndex,
						vFxPos,
						vFxRot,
						fScale,
						iInvertAxes,
						r, g, b,
						alpha,
						bIgnoreScopeChanges);
				}
			}
		}

		g_usePtFxAssetHashName = (u32)0;
		return retc;
	}

	return false;
}

bool CommandTriggerPtFxInternal_BoneTag(const char* pFxName, CEntity* pEntity, bool bCheckEntity, int iBoneTag, bool bCheckBoneTag, Vector3 vFxPos, Vector3 vFxRot, float fScale, u8 iInvertAxes, bool bLocalOnly, bool bIgnoreScopeChanges)
{
	if (!bCheckBoneTag || (iBoneTag>0))
	{
		int iBoneIndex = -1;
		if (bCheckBoneTag)
		{
			CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pEntity);
			if (pDynamicEntity)
			{
				iBoneIndex = pDynamicEntity->GetBoneIndexFromBoneTag(static_cast<eAnimBoneTag>(iBoneTag));
			}
		}

		return CommandTriggerPtFxInternal_BoneIndex(pFxName, pEntity, bCheckEntity, iBoneIndex, vFxPos, vFxRot, fScale, iInvertAxes, bLocalOnly, bIgnoreScopeChanges);
	}

	return false;
}

//
// name:		CommandTriggerPtFxInternal
// description:
bool CommandTriggerPtFxOnEntityBoneInternal(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneIndex, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ, bool bCheckEntity, bool bLocalOnly, bool ignoreScopeChanges = false)
{
	u8 invertAxes = 0;
	if (invertAxisX) invertAxes |= PTXEFFECTINVERTAXIS_X;
	if (invertAxisY) invertAxes |= PTXEFFECTINVERTAXIS_Y;
	if (invertAxisZ) invertAxes |= PTXEFFECTINVERTAXIS_Z;

	Vector3 fxPos = Vector3(scrVecFxPos);
	Vector3 fxRot = Vector3((DtoR * scrVecFxRot.x), (DtoR * scrVecFxRot.y), (DtoR * scrVecFxRot.z));
	CEntity* pEntity = bCheckEntity ? CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES) : NULL;

	if (!bCheckEntity || pEntity)
	{
		
		// don't allow attachment to parent entities without a drawable
		if (bCheckEntity && pEntity && pEntity->GetDrawable() == NULL)
		{
			g_usePtFxAssetHashName = (u32)0;
			return 0;
		}

		return CommandTriggerPtFxInternal_BoneIndex(pFxName, pEntity, bCheckEntity, boneIndex, fxPos, fxRot, scale, invertAxes, bLocalOnly, ignoreScopeChanges);
	}

	g_usePtFxAssetHashName = (u32)0;
	return false;
}

//
// name:		CommandTriggerPtFxInternal
// description:
bool CommandTriggerPtFxInternal(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneTag, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ, bool bCheckEntity, bool bCheckBoneTag, bool bLocalOnly, bool ignoreScopeChanges = false)
{
	u8 invertAxes = 0;
	if (invertAxisX) invertAxes |= PTXEFFECTINVERTAXIS_X;
	if (invertAxisY) invertAxes |= PTXEFFECTINVERTAXIS_Y;
	if (invertAxisZ) invertAxes |= PTXEFFECTINVERTAXIS_Z;

	Vector3 fxPos = Vector3(scrVecFxPos);
	Vector3 fxRot = Vector3(( DtoR * scrVecFxRot.x), ( DtoR * scrVecFxRot.y), ( DtoR * scrVecFxRot.z));
	CEntity* pEntity = bCheckEntity ? CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES) : NULL;

	if(!bCheckEntity || pEntity)
	{
		if (bCheckBoneTag)
		{
			SCRIPT_ASSERT(boneTag > 0, "START_PARTICLE_FX_NON_LOOPED_ON_PED_BONE - called with an invalid bone tag");
		}

		// don't allow attachment to parent entities without a drawable
		if (bCheckEntity && pEntity && pEntity->GetDrawable()==NULL)
		{
			g_usePtFxAssetHashName = (u32)0;
			return 0;
		}

		return CommandTriggerPtFxInternal_BoneTag(pFxName, pEntity, bCheckEntity, boneTag, bCheckBoneTag, fxPos, fxRot, scale, invertAxes, bLocalOnly, ignoreScopeChanges);
	}

	g_usePtFxAssetHashName = (u32)0;
	return false;
}



//
// name:		CommandTriggerPtFx
// description:
bool CommandTriggerPtFx(const char* pFxName, const scrVector &scrVecFxPos, const scrVector & scrVecFxRot, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return CommandTriggerPtFxInternal(pFxName, -1, scrVecFxPos, scrVecFxRot, -1, scale, invertAxisX, invertAxisY, invertAxisZ, false, false, true);
}

bool CommandTriggerNetworkedPtFx(const char* pFxName, const scrVector &scrVecFxPos, const scrVector & scrVecFxRot, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ, bool ignoreScopeChanges)
{
	return CommandTriggerPtFxInternal(pFxName, -1, scrVecFxPos, scrVecFxRot, -1, scale, invertAxisX, invertAxisY, invertAxisZ, false, false, false, ignoreScopeChanges);
}

//
// name:		CommandTriggerPtFxOnPedBone
// description:
int CommandTriggerPtFxOnPedBone(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneTag, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return CommandTriggerPtFxInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, boneTag, scale, invertAxisX, invertAxisY, invertAxisZ, true, true, true);
}

//
// name:		CommandTriggerNetworkedPtFxOnPedBone
// description:
int CommandTriggerNetworkedPtFxOnPedBone(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneTag, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return CommandTriggerPtFxInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, boneTag, scale, invertAxisX, invertAxisY, invertAxisZ, true, true, false);
}

//
// name:		CommandTriggerPtFxOnEntity
// description:
bool CommandTriggerPtFxOnEntity(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return CommandTriggerPtFxInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, -1, scale, invertAxisX, invertAxisY, invertAxisZ, true, false, true);
}

//
// name:		CommandTriggerNetworkedPtFxOnEntity
// description:
bool CommandTriggerNetworkedPtFxOnEntity(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return CommandTriggerPtFxInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, -1, scale, invertAxisX, invertAxisY, invertAxisZ, true, false, false);
}

//
// name:		CommandTriggerPtFxOnEntityBone
// description:
bool CommandTriggerPtFxOnEntityBone(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneIndex, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return CommandTriggerPtFxOnEntityBoneInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, boneIndex, scale, invertAxisX, invertAxisY, invertAxisZ, true, false);
}
//
// name:		CommandSetTriggeredPtFxColour
// description:
void CommandSetTriggeredPtFxColour(float r, float g, float b)
{
	ptfxScript::SetTriggeredColourTint(r, g, b);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketTriggeredScriptPtFxColour>(CPacketTriggeredScriptPtFxColour(r, g, b));
	}
#endif
}


//
// name:		CommandSetTriggeredPtFxAlpha
// description:
void CommandSetTriggeredPtFxAlpha(float a)
{
	if (SCRIPT_VERIFY(a >= 0.f && a <= 1.f, "SET_PARTICLE_FX_NON_LOOPED_ALPHA - value passing in is not in 0.0 - 1.0 range!"))
	{
		ptfxScript::SetTriggeredAlphaTint(a);		
	}
	else
	{
#if !__FINAL
		scrThread::PrePrintStackTrace();
#endif // !__FINAL
	}	
}


//
// name:		CommandSetTriggeredPtFxScale
// description:
void CommandSetTriggeredPtFxScale(float scale)
{
	ptfxScript::SetTriggeredScale(scale);
}


//
// name:		CommandSetTriggeredPtFxEmitterSize
// description:
void CommandSetTriggeredPtFxEmitterSize(const scrVector & scrVecOverrideSize)
{
	Vector3 overrideSize = Vector3(scrVecOverrideSize);
	ptfxScript::SetTriggeredEmitterSize(RCC_VEC3V(overrideSize));
}

//
// name:		CommandSetTriggeredPtFxForceVehicleInterior
// description:
void CommandSetTriggeredPtFxForceVehicleInterior(bool bIsVehicleInterior)
{
	ptfxScript::SetTriggeredForceVehicleInteriorFX(bIsVehicleInterior);
}

//
// name:		CommandStartPtFx
// description:
int CommandStartPtFx(const char* pFxName, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ, bool localOnly)
{
	u8 invertAxes = 0;
	if (invertAxisX) invertAxes |= PTXEFFECTINVERTAXIS_X;
	if (invertAxisY) invertAxes |= PTXEFFECTINVERTAXIS_Y;
	if (invertAxisZ) invertAxes |= PTXEFFECTINVERTAXIS_Z;

	Vector3 fxPos = Vector3(scrVecFxPos);
	Vector3 fxRot = Vector3(( DtoR * scrVecFxRot.x), ( DtoR * scrVecFxRot.y), ( DtoR * scrVecFxRot.z));

	CScriptResource_PTFX ptfx(pFxName, g_usePtFxAssetHashName, NULL, -1, fxPos, fxRot, scale, invertAxes);

    int ptFXID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(ptfx);

    if(NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPCutscene() && !localOnly)
    {
        const char* pScriptName = CTheScripts::GetCurrentScriptName();
		atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);
		if (g_usePtFxAssetHashName!=0)
		{
			ptfxAssetName = g_usePtFxAssetHashName;
		}

#if !__FINAL
        scriptDebugf1("%s: Starting PTFX over network: %s (%d) Pos: (%.2f, %.2f, %.2f) Rot: (%.2f, %.2f, %.2f), Scale:%.2f, Invert Axes:%d, ID: %d",
            CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
            pFxName,
            atStringHash(pFxName),
            fxPos.x, fxPos.y, fxPos.z,
            fxRot.x, fxRot.y, fxRot.z,
            scale,
            invertAxes,
            ptFXID);
        scrThread::PrePrintStackTrace();
#endif // !__FINAL
        NetworkInterface::StartPtFXOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
                                               atStringHash(pFxName),
                                               ptfxAssetName,
                                               fxPos,
                                               fxRot,
                                               scale,
                                               invertAxes,
                                               ptFXID);
    }

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && ptFXID != 0)
	{
		const char* pScriptName = CTheScripts::GetCurrentScriptName();
		atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);
		//Overriding the ptfxAssetname with any global name thats been set.
		if (g_usePtFxAssetHashName!=0)
		{
			ptfxAssetName = g_usePtFxAssetHashName;
		}

		CReplayMgr::RecordPersistantFx<CPacketStartScriptPtFx>(
			CPacketStartScriptPtFx(atHashWithStringNotFinal(pFxName), ptfxAssetName, -1, fxPos, fxRot, scale, invertAxes),
			CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)ptFXID),
			NULL, 
			true);
	}
#endif

	g_usePtFxAssetHashName = (u32)0;

	return ptFXID;
}

//
// name:		CommandStartPtFxOnPedBone
// description:
int CommandStartPtFxOnPedBone(const char* pFxName, int pedIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneTag, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	u8 invertAxes = 0;
	if (invertAxisX) invertAxes |= PTXEFFECTINVERTAXIS_X;
	if (invertAxisY) invertAxes |= PTXEFFECTINVERTAXIS_Y;
	if (invertAxisZ) invertAxes |= PTXEFFECTINVERTAXIS_Z;


	int ptFXID = 0;
	Vector3 fxPos = Vector3(scrVecFxPos);
	Vector3 fxRot = Vector3(( DtoR * scrVecFxRot.x), ( DtoR * scrVecFxRot.y), ( DtoR * scrVecFxRot.z));
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed)
	{
		if (SCRIPT_VERIFY(boneTag > 0, "START_PARTICLE_FX_LOOPED_ON_PED_BONE - called with an invalid bone tag"))
		{
			int boneIndex = pPed->GetBoneIndexFromBoneTag(static_cast<eAnimBoneTag>(boneTag));

			CScriptResource_PTFX ptfx(pFxName, g_usePtFxAssetHashName, pPed, boneIndex, fxPos, fxRot, scale, invertAxes);

			ptFXID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(ptfx);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && ptFXID != 0)
			{
				const char* pScriptName = CTheScripts::GetCurrentScriptName();
				atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);
				//Overriding the ptfxAssetname with any global name thats been set.
				//This is done in CScriptResource_PTFX so we need to ensure we mimic the same functionality
				if (g_usePtFxAssetHashName!=0)
				{
					ptfxAssetName = g_usePtFxAssetHashName;
				}

				CReplayMgr::RecordPersistantFx<CPacketStartScriptPtFx>(
					CPacketStartScriptPtFx(atHashWithStringNotFinal(pFxName), ptfxAssetName, boneIndex, fxPos, fxRot, scale, invertAxes),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)ptFXID),
					pPed, 
					true);
			}
#endif
		}
	}

	return ptFXID;
}

//
// name:		CommandStartPtFxOnEntity
// description:
int StartPtFxOnEntityInternal(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneIndex, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ, bool bLocalOnly, float colorR = 1.0f, float colorG = 1.0f, float colorB = 1.0f, bool terminateOnOwnerLeave = false)
{
	u8 invertAxes = 0;
	if (invertAxisX) invertAxes |= PTXEFFECTINVERTAXIS_X;
	if (invertAxisY) invertAxes |= PTXEFFECTINVERTAXIS_Y;
	if (invertAxisZ) invertAxes |= PTXEFFECTINVERTAXIS_Z;

	Vector3 fxPos = Vector3(scrVecFxPos);
	Vector3 fxRot = Vector3(( DtoR * scrVecFxRot.x), ( DtoR * scrVecFxRot.y), ( DtoR * scrVecFxRot.z));
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	int ptFXID = 0;
	if(pEntity)
	{
		CScriptResource_PTFX ptfx(pFxName, g_usePtFxAssetHashName, pEntity, boneIndex, fxPos, fxRot, scale, invertAxes);
		ptFXID = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(ptfx);
		if (ptFXID==0)
		{
			g_usePtFxAssetHashName = (u32)0;
			return 0;
		}

		u64 ownerPeerID  = 0;
		if (NetworkInterface::IsGameInProgress() && terminateOnOwnerLeave)
		{
			netPlayer *localPlayer = netInterface::GetLocalPlayer();

			if(localPlayer)
			{
				ownerPeerID = localPlayer->GetRlPeerId();
			}
		}

		ptfxScript::UpdateColourTint(ptFXID, Vec3V(colorR, colorG, colorB));

		if(!bLocalOnly && NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPCutscene())
		{
			const char* pScriptName = CTheScripts::GetCurrentScriptName();
			atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);
			if (g_usePtFxAssetHashName!=0)
			{
				ptfxAssetName = g_usePtFxAssetHashName;
			}

#if !__FINAL
			scriptDebugf1("%s: Starting PTFX on entity over network: %s (%d) Pos: (%.2f, %.2f, %.2f) Rot: (%.2f, %.2f, %.2f), Scale:%.2f, Invert Axes:%d, ID: %d boneIndex: %d, RGB (%f, %f, %f)",
				CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
				pFxName,
				atStringHash(pFxName),
				fxPos.x, fxPos.y, fxPos.z,
				fxRot.x, fxRot.y, fxRot.z,
				scale,
				invertAxes,
				ptFXID,
				boneIndex,
				colorR, colorG, colorB);
			scrThread::PrePrintStackTrace();
#endif // !__FINAL

			NetworkInterface::StartPtFXOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
				atStringHash(pFxName),
				ptfxAssetName,
				fxPos,
				fxRot,
				scale,
				invertAxes,
				ptFXID,
				pEntity,
				boneIndex,
				colorR,
				colorG,
				colorB,
				terminateOnOwnerLeave,
				ownerPeerID);
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			const char* pScriptName = CTheScripts::GetCurrentScriptName();
			atHashWithStringNotFinal ptfxAssetName = g_vfxScript.GetScriptPtFxAssetName(pScriptName);
			//Overriding the ptfxAssetname with any global name thats been set.
			if (g_usePtFxAssetHashName!=0)
			{
				ptfxAssetName = g_usePtFxAssetHashName;
			}

			atHashWithStringNotFinal nameHash(pFxName);

			bool recordPacket = true;
			// Remove this effect from being recorded - url:bugstar:3062605
			if(nameHash.GetHash() == 0xc1ec768e /*"scr_adversary_slipstream_formation"*/ && ptfxAssetName.GetHash() == 0x7cc6e9d1 /*"scr_bike_adversary"*/)
				recordPacket = false;

			if(recordPacket)
			{
				CReplayMgr::RecordPersistantFx<CPacketStartScriptPtFx>(
					CPacketStartScriptPtFx(nameHash, ptfxAssetName, boneIndex, fxPos, fxRot, scale, invertAxes, true, (u8)(colorR * 255.0f), (u8)(colorG * 255.0f), (u8)(colorB * 255.0f)),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)ptFXID),
					pEntity, 
					true);
			}
			else
			{
				replayDebugf1("StartPtFxOnEntityInternal - Ignoring Script effect %s", pFxName);
			}
		}
#endif
	}

	g_usePtFxAssetHashName = (u32)0;
	return ptFXID;
}


int CommandStartPtFxOnEntity(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return StartPtFxOnEntityInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, -1, scale, invertAxisX, invertAxisY, invertAxisZ, true);
}

int CommandStartPtFxOnEntityBone(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneIndex, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ)
{
	return StartPtFxOnEntityInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, boneIndex, scale, invertAxisX, invertAxisY, invertAxisZ, true);
}

int CommandStartNetworkedPtFxOnEntity(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ, float r, float g, float b,  bool terminateOnOwnerLeave)
{
	return StartPtFxOnEntityInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, -1, scale, invertAxisX, invertAxisY, invertAxisZ, false, r, g, b, terminateOnOwnerLeave);
}

int CommandStartNetworkedPtFxOnEntityBone(const char* pFxName, int entityIndex, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot, int boneIndex, float scale, bool invertAxisX, bool invertAxisY, bool invertAxisZ, float r, float g, float b, bool terminateOnOwnerLeave)
{
	return StartPtFxOnEntityInternal(pFxName, entityIndex, scrVecFxPos, scrVecFxRot, boneIndex, scale, invertAxisX, invertAxisY, invertAxisZ, false, r, g, b, terminateOnOwnerLeave);
}


//
// name:		CommandStopPtFx
// description:
void CommandStopPtFx(int scriptedPtFxId, bool localOnly)
{
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketStopScriptPtFx>(CPacketStopScriptPtFx(), CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)scriptedPtFxId), NULL, false);
		CReplayMgr::StopTrackingFx(CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)scriptedPtFxId));
	}
#endif

	if(SCRIPT_VERIFY(scriptedPtFxId>0, "STOP_PARTICLE_FX_LOOPED - Invalid script id"))
	{
        if(NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPCutscene() && !localOnly)
        {
#if !__FINAL
            scriptDebugf1("%s: Stopping PTFX over network: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scriptedPtFxId);
            scrThread::PrePrintStackTrace();
#endif // !__FINAL
            NetworkInterface::StopPtFXOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), scriptedPtFxId);
        }

		ptfxScript::SetRemoveWhenDestroyed(scriptedPtFxId, false);
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX, scriptedPtFxId);
	}
}

//
// name:		CommandRemovePtFx
// description:

void CommandRemovePtFx(int scriptedPtFxId, bool localOnly)
{
	if(SCRIPT_VERIFY(scriptedPtFxId>0, "REMOVE_PARTICLE_FX - Invalid script id"))
	{
		ptfxScript::SetRemoveWhenDestroyed(scriptedPtFxId, true);
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX, scriptedPtFxId);

        if(NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPCutscene() && !localOnly)
        {
#if !__FINAL
            scriptDebugf1("%s: Removing PTFX over network: %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scriptedPtFxId);
            scrThread::PrePrintStackTrace();
#endif // !__FINAL
            NetworkInterface::StopPtFXOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), scriptedPtFxId);
        }
	}
}

//
// name:		CommandRemovePtFxFromEntity
// description:

void CommandRemovePtFxFromEntity(int entityIndex)
{
	CEntity* pEntity = CTheScripts::GetEntityToModifyFromGUID<CEntity>(entityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(pEntity)
	{
		g_ptFxManager.RemovePtFxFromEntity(pEntity);
	}
}

//
// name:		CommandRemovePtFxInRange
// description:

void CommandRemovePtFxInRange(const scrVector & scrVecPos, float range)
{
	Vector3 pos = Vector3(scrVecPos);
	g_ptFxManager.RemovePtFxInRange(RCC_VEC3V(pos), range);
}

//
// name:		CommandForcePtFxInVehicleInterior
// description:

void CommandForcePtFxInVehicleInterior(int scriptedPtFxId, bool bIsVehicleInteriorFX)
{
	if(SCRIPT_VERIFY(scriptedPtFxId>0, "FORCE_PARTICLE_FX_IN_VEHICLE_INTERIOR - Invalid script id"))
	{
		ptfxScript::UpdateForceVehicleInteriorFX(scriptedPtFxId, bIsVehicleInteriorFX);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordPersistantFx<CPacketForceVehInteriorScriptPtFx>(CPacketForceVehInteriorScriptPtFx(bIsVehicleInteriorFX),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)scriptedPtFxId));
		}
#endif // GTA_REPLAY
	}
}

//
// name:		CommandDoesPtFxExist
// description:

bool CommandDoesPtFxExist(int scriptedPtFxId)
{
	return ptfxScript::DoesExist(scriptedPtFxId);
}


//
// name:		CommandUpdatePtFxOffsets
// description:
void CommandUpdatePtFxOffsets(int scriptedPtFxId, const scrVector & scrVecFxPos, const scrVector & scrVecFxRot)
{
	Vector3 fxPos = Vector3(scrVecFxPos);
	Vector3 fxRot = Vector3(( DtoR * scrVecFxRot.x), ( DtoR * scrVecFxRot.y), ( DtoR * scrVecFxRot.z));
	ptfxScript::UpdateOffset(scriptedPtFxId, RC_VEC3V(fxPos), RC_VEC3V(fxRot));
}


//
// name:		CommandUpdatePtFxColour
// description:
void CommandUpdatePtFxColour(int scriptedPtFxId, float r, float g, float b, bool localOnly)
{
	ptfxScript::UpdateColourTint(scriptedPtFxId, Vec3V(r, g, b));

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketUpdateScriptPtFxColour>(CPacketUpdateScriptPtFxColour(r, g, b), CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)scriptedPtFxId), NULL, false);
	}
#endif

	if(NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPCutscene() && !localOnly)
	{
		NetworkInterface::ModifyPtFXColourOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(),
			r,g,b,
			scriptedPtFxId);
	}
}


//
// name:		CommandUpdatePtFxAlpha
// description:
void CommandUpdatePtFxAlpha(int scriptedPtFxId, float a)
{
	ptfxScript::UpdateAlphaTint(scriptedPtFxId, a);
}


//
// name:		CommandUpdatePtFxScale
// description:
void CommandUpdatePtFxScale(int scriptedPtFxId, float scale)
{
	ptfxScript::UpdateScale(scriptedPtFxId, scale);
}

//
// name:		CommandUpdatePtFxEmitterSize
// description:
void CommandUpdatePtFxEmitterSize(int scriptedPtFxId, bool isOverridden, const scrVector & scrVecOverrideSize)
{
	Vector3 overrideSize = Vector3(scrVecOverrideSize);
	ptfxScript::UpdateEmitterSize(scriptedPtFxId, isOverridden, RCC_VEC3V(overrideSize));
}

//
// name:		CommandUpdatePtFxFarClipDist
// description:
void CommandUpdatePtFxFarClipDist(int scriptedPtFxId, float farClipDist)
{

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketUpdatePtFxFarClipDist>(CPacketUpdatePtFxFarClipDist(farClipDist), CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)scriptedPtFxId), NULL, false);
	}
#endif

	ptfxScript::UpdateFarClipDist(scriptedPtFxId, farClipDist);
}


//
// name:		CommandEvolvePtFx
// description:
void CommandEvolvePtFx(int scriptedPtFxId, const char* pEvoName, float evoVal, bool localOnly)
{
	ptfxScript::UpdateEvo(scriptedPtFxId, pEvoName, evoVal);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketEvolvePtFx>(CPacketEvolvePtFx(atHashValue(pEvoName).GetHash(), evoVal), CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)scriptedPtFxId), NULL, false);
	}
#endif

	if(NetworkInterface::IsGameInProgress() && !NetworkInterface::IsInMPCutscene() && !localOnly)
	{
		scriptDebugf1("CommandEvolvePtFx called for %d, localOnly: %d, evoName %s, evoVal: %.2f", scriptedPtFxId, localOnly, pEvoName, evoVal);
		NetworkInterface::ModifyEvolvePtFxOverNetwork(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId(), atHashValue(pEvoName).GetHash(), evoVal, scriptedPtFxId);
	}
}

//
// name:		CommandSetFxFlagCamInsideVehicle
// description:

void CommandSetFxFlagCamInsideVehicle(bool bSet)
{
	CVfxHelper::SetCamInVehicleFlag(bSet);
}

//
// name:		CommandSetFxFlagCamInsideNonPlayerVehicle
// description:

void CommandSetFxFlagCamInsideNonPlayerVehicle(int vehIndex, bool bSet)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex);
	if (SCRIPT_VERIFY(pVehicle, "SET_PARTICLE_FX_CAM_INSIDE_NONPLAYER_VEHICLE - Invalid Vehicle - VehicleIndex not found."))
	{
		pVehicle = (bSet == true) ? pVehicle : NULL;
		CVfxHelper::SetRenderLastVehicle(pVehicle);
	}
}

//
// name:		CommandSetFxFlagShootoutBoat
// description:

void CommandSetFxFlagShootoutBoat(int vehIndex)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex);

	if(pVehicle)
	{
		CVfxHelper::SetShootoutBoat(pVehicle);
	}
}

//
// name:		CommandClearFxFlagShootoutBoat
// description:

void CommandClearFxFlagShootoutBoat()
{
	CVfxHelper::SetShootoutBoat(NULL);
}


//
// name:		CommandSetBloodPtFxScale
// description:

void CommandSetBloodPtFxScale(float scale)
{
	g_vfxBlood.SetBloodPtFxUserZoomScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandDisableBloodVfx
// description:

void CommandDisableBloodVfx(bool disable)
{
	g_vfxBlood.SetBloodVfxDisabled(disable, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandDisableInWaterPtFx
// description:

void CommandDisableInWaterPtFx(bool disable)
{
	g_vfxEntity.SetInWaterPtFxDisabled(disable, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandDisableDownwashPtfx
// description:

void CommandDisableDownwashPtfx(bool disable)
{
	g_vfxVehicle.SetDownwashPtFxDisabled(disable, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetSlipstreamPtFxLodRangeScale
// description:

void CommandSetSlipstreamPtFxLodRangeScale(float scale)
{
	g_vfxVehicle.SetSlipstreamPtFxLodRangeScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandEnableClownBloodVfx
// description:

void CommandEnableClownBloodVfx(bool enable)
{
	if (enable)
	{
		g_vfxBlood.SetOverrideWeaponGroupId(VFXBLOOD_SPECIAL_WEAPON_GROUP_CLOWN, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
	else
	{
		g_vfxBlood.SetOverrideWeaponGroupId(-1, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
}


//
// name:		CommandEnableAlienBloodVfx
// description:

void CommandEnableAlienBloodVfx(bool enable)
{
	if (enable)
	{
		g_vfxBlood.SetOverrideWeaponGroupId(VFXBLOOD_SPECIAL_WEAPON_GROUP_ALIEN, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
	else
	{
		g_vfxBlood.SetOverrideWeaponGroupId(-1, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
}


//
// name:		CommandSetBulletImpactPtFxScale
// description:

void CommandSetBulletImpactPtFxScale(float scale)
{
	g_vfxWeapon.SetBulletImpactPtFxUserZoomScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetBulletImpactPtFxLodRangeScale
// description:

void CommandSetBulletImpactPtFxLodRangeScale(float scale)
{
	g_vfxWeapon.SetBulletImpactPtFxLodRangeScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetBulletTraceNoAngleReject
// description:

void CommandSetBulletTraceNoAngleReject(bool val)
{
	g_vfxWeapon.SetBulletTraceNoAngleReject(val, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetBangScrapePtFxLodRangeScale
// description:

void CommandSetBangScrapePtFxLodRangeScale(float scale)
{
	g_vfxMaterial.SetBangScrapePtFxLodRangeScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetFootPtFxLodRangeScale
// description:

void CommandSetFootPtFxLodRangeScale(float scale)
{
	g_vfxPed.SetFootVfxLodRangeScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetFootPtFxOverrideHashName
// description:

void CommandSetFootPtFxOverrideName(const char* pName)
{
	atHashWithStringNotFinal hashName(pName);
	if (hashName.IsNull())
	{
		g_vfxPed.SetFootPtFxOverrideHashName(hashName, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
	else 
	{
		g_vfxPed.SetFootPtFxOverrideHashName(hashName, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
}


//
// name:		CommandSetWheelPtFxLodRangeScale
// description:

void CommandSetWheelPtFxLodRangeScale(float scale)
{
	g_vfxWheel.SetWheelPtFxLodRangeScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetWheelSkidmarkRangeScale
// description:

void CommandSetWheelSkidmarkRangeScale(float scale)
{
	g_vfxWheel.SetWheelSkidmarkRangeScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandSetPtFxForceVehicleInteriorFlag
// description: Makes all effect instances set the flag to be considered in vehicle interiors

void CommandSetPtFxForceVehicleInteriorFlag(bool bSet)
{
	g_ptFxManager.SetForceVehicleInteriorFlag(bSet, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandOverridePtFxObjectWaterSplashIn
// description:

void CommandOverridePtFxObjectWaterSplashIn(bool overrideEnabled, const char* pOverridePtFxName)
{
	if (overrideEnabled)
	{
		g_vfxWater.OverrideWaterPtFxSplashObjectInName(atHashWithStringNotFinal(pOverridePtFxName), CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
	else
	{
		g_vfxWater.OverrideWaterPtFxSplashObjectInName(atHashWithStringNotFinal((u32)0), CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
	}
}



//
// name:		CommandSetPtFxNoDownsamplingThisFrame
// description:

void CommandSetPtFxNoDownsamplingThisFrame()
{
#if USE_DOWNSAMPLED_PARTICLES
	g_ptFxManager.SetDisableDownsamplingThisFrame();
#endif
}


//
// name:		CommandTriggerPtFxEMP
// description:

void CommandTriggerPtFxEMP(int vehicleIndex, float scale)
{
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	if (pVehicle)
	{
		g_vfxWeapon.TriggerPtFxEMP(pVehicle, scale);
	}
}


//
// name:		CommandUseParticleFxAsset
// description:
void CommandUseParticleFxAsset(const char* pPtFxAssetName)
{
	scriptDebugf2("USE_PARTICLE_FX_ASSET - Setting asset %s", pPtFxAssetName);
	g_usePtFxAssetHashName = pPtFxAssetName;
}


//
// name:		VerifyUsePtFxAssetHashName
// description:
void VerifyUsePtFxAssetHashName()
{
	scriptAssertf(g_usePtFxAssetHashName.GetHash()==(u32)0, "USE_PARTICLE_FX_ASSET has been called without a following START_PARTICLE_FX_ command");
	g_usePtFxAssetHashName = (u32)0;
}


//
// name:		CommandSetParticleFxOverride
// description:
void CommandSetParticleFxOverride(const char* pPtFxToOverride, const char* pPtFxToUseInstead)
{
	ptfxManager::AddPtFxOverrideInfo(pPtFxToOverride, pPtFxToUseInstead, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandResetParticleFxOverride
// description:
void CommandResetParticleFxOverride(const char* pPtFxToOverride)
{
	ptfxManager::RemovePtFxOverrideInfo(pPtFxToOverride, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}



// --- Weather/Region Ptfx -----------------------------------------------------------------------------

//
// name:		CommandWeatherRegionPtfxSetUseOverrideSettings
// description:
void CommandWeatherRegionPtfxSetUseOverrideSettings(bool bUseOverrideSettings)
{
	g_vfxWeather.GetPtFxGPUManager().SetUseOverrideSettings(bUseOverrideSettings);
}

//
// name:		CommandWeatherRegionPtfxSetOverrideCurrLevel
// description:
void CommandWeatherRegionPtfxSetOverrideCurrLevel(float overrideCurrLevel)
{
	g_vfxWeather.GetPtFxGPUManager().SetOverrideCurrLevel(overrideCurrLevel);
}

//
// name:		CommandWeatherRegionPtfxSetSetOverrideBoxOffset
// description:
void CommandWeatherRegionPtfxSetSetOverrideBoxOffset(const scrVector &scrOffset)
{
	Vector3 offset = Vector3(scrOffset);
	g_vfxWeather.GetPtFxGPUManager().SetOverrideBoxOffset(RCC_VEC3V(offset));
}

//
// name:		CommandWeatherRegionPtfxSetSetOverrideBoxSize
// description:
void CommandWeatherRegionPtfxSetSetOverrideBoxSize(const scrVector &scrSize)
{
	Vector3 size = Vector3(scrSize);
	g_vfxWeather.GetPtFxGPUManager().SetOverrideBoxSize(RCC_VEC3V(size));
}

// --- Decals -----------------------------------------------------------------------------

//
// name:		CommandWashDecalsInRange
// description:
void CommandWashDecalsInRange(const scrVector & scrVecPos, float range, float washAmount)
{	
	Vector3 pos = Vector3(scrVecPos);
	g_decalMan.WashInRange(washAmount, RCC_VEC3V(pos), ScalarV(range), NULL);
}

//
// name:		CommandWashDecalsFromVehicle
// description:
void CommandWashDecalsFromVehicle(int vehIndex, float washAmount)
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehIndex);

	if(pVehicle)
	{
		g_decalMan.Wash(washAmount, pVehicle, false);
	}
}

//
// name:		CommandFadeDecalsInRange
// description:
void CommandFadeDecalsInRange(const scrVector& scrVecPos, float range, float fadeTime)
{	
	Vector3 pos = Vector3(scrVecPos);
	g_decalMan.FadeInRange(RCC_VEC3V(pos), ScalarV(range), fadeTime, NULL);
}

//
// name:		CommandRemoveDecalsInRange
// description:
void CommandRemoveDecalsInRange(const scrVector & scrVecPos, float range)
{	
	Vector3 pos = Vector3(scrVecPos);
	g_decalMan.RemoveInRange(RCC_VEC3V(pos), ScalarV(range), NULL);
}

//
// name:		CommandRemoveDecalsFromObject
// description:

void CommandRemoveDecalsFromObject(int ObjectIndex)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	if (pObj)
	{
		g_decalMan.Remove(pObj);
	}
}

//
// name:		CommandRemoveDecalsFromObjectFacing
// description:

void CommandRemoveDecalsFromObjectFacing(int ObjectIndex, const scrVector & scrVecDir)
{
	CObject *pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);

	if (pObj)
	{
		Vector3 dir = Vector3(scrVecDir);
		g_decalMan.RemoveFacing(RCC_VEC3V(dir), pObj);
	}
}

//
// name:		CommandRemoveDecalsFromVehicle
// description:

void CommandRemoveDecalsFromVehicle(int VehicleIndex)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		g_decalMan.Remove(pVehicle);
	}
}

//
// name:		CommandRemoveDecalsFromVehicleFacing
// description:

void CommandRemoveDecalsFromVehicleFacing(int VehicleIndex, const scrVector & scrVecDir)
{
	CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);

	if (pVehicle)
	{
		Vector3 dir = Vector3(scrVecDir);
		g_decalMan.RemoveFacing(RCC_VEC3V(dir), pVehicle);
	}
}

//
// name:		CommandAddDecal
// description:

int CommandAddDecal(int renderSettingId, const scrVector & pos, const scrVector & dir, const scrVector & side, float width, float height, float colR, float colG, float colB, float colA, float life, bool isLongRange, bool isDynamic, bool useComplexColn)
{
	scriptAssertf(colR>=0.0f && colR<=1.0f, "ADD_DECAL has been passed an out or range colour component (red) (%.3f) - range is 0.0 to 1.0", colR);
	scriptAssertf(colG>=0.0f && colG<=1.0f, "ADD_DECAL has been passed an out or range colour component (green) (%.3f) - range is 0.0 to 1.0", colG);
	scriptAssertf(colB>=0.0f && colB<=1.0f, "ADD_DECAL has been passed an out or range colour component (blue) (%.3f) - range is 0.0 to 1.0", colB);
	scriptAssertf(colA>=0.0f && colA<=1.0f, "ADD_DECAL has been passed an out or range colour component (alpha) (%.3f) - range is 0.0 to 1.0", colA);

	Color32 col(colR, colG, colB, colA);

	s32 decalRenderSettingIndex;
	s32 decalRenderSettingCount;
	g_decalMan.FindRenderSettingInfo(renderSettingId, decalRenderSettingIndex, decalRenderSettingCount);	

	DecalType_e decalType;
	if (useComplexColn)
	{
		if (isLongRange)
		{
			decalType = DECALTYPE_GENERIC_COMPLEX_COLN_LONG_RANGE;
		}
		else
		{
			decalType = DECALTYPE_GENERIC_COMPLEX_COLN;
		}
	}
	else
	{
		if (isLongRange)
		{
			decalType = DECALTYPE_GENERIC_SIMPLE_COLN_LONG_RANGE;
		}
		else
		{
			decalType = DECALTYPE_GENERIC_SIMPLE_COLN;
		}
	}

	const int decalId = g_decalMan.AddGeneric(decalType, decalRenderSettingIndex, decalRenderSettingCount, VECTOR3_TO_VEC3V((Vector3)pos), VECTOR3_TO_VEC3V((Vector3)dir), VECTOR3_TO_VEC3V((Vector3)side), width, height, 0.0f, life, 0.0f, 1.0f, 1.0f, 0.0f, col, false, false, 0.0f, true, isDynamic REPLAY_ONLY(, 0.0f));

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && decalId != 0)
	{
		CReplayMgr::RecordPersistantFx<CPacketAddScriptDecal>(
			CPacketAddScriptDecal((Vector3)pos, (Vector3)dir, (Vector3)side, col, renderSettingId, width, height, life, decalType, isDynamic, decalId),
			CTrackedEventInfo<tTrackedDecalType>(decalId),
			NULL, 
			true);
	}
#endif // GTA_REPLAY

	return decalId;
}

//
// name:		CommandAddPetrolDecal
// description:

int CommandAddPetrolDecal(const scrVector & pos, float startSize, float endSize, float growRate)
{		
	Color32 col(255, 255, 255, 255);
	return g_decalMan.AddPool(VFXLIQUID_TYPE_PETROL, -1, 0, VECTOR3_TO_VEC3V((Vector3)pos), Vec3V(V_Z_AXIS_WZERO), startSize, endSize, growRate, 0, col, true);
}

//
// name:		CommandAddOilDecal
// description:

int CommandAddOilDecal(const scrVector & pos, float startSize, float endSize, float growRate)
{			
	Color32 col(255, 255, 255, 255);	
	return g_decalMan.AddPool(VFXLIQUID_TYPE_OIL, -1, 0, VECTOR3_TO_VEC3V((Vector3)pos), Vec3V(V_Z_AXIS_WZERO), startSize, endSize, growRate, 0, col, true);
}

//
// name:		CommandStartSkidmarkDecals
// description:

void CommandStartSkidmarkDecals(float width)
{
	VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(VFXTYRESTATE_OK_DRY, VFXGROUP_TARMAC);
	s32 renderSettingsIndex = g_decalMan.GetRenderSettingsIndex(pVfxWheelInfo->decal1RenderSettingIndex, pVfxWheelInfo->decal1RenderSettingCount);

	Color32 col(255, 255, 255, 255);

	g_decalMan.StartScriptedTrail(DECALTYPE_TRAIL_SKID, VFXLIQUID_TYPE_NONE, renderSettingsIndex, width, col);
}

//
// name:		CommandAddSkidmarkDecalInfo
// description:

void CommandAddSkidmarkDecalInfo(const scrVector & pos, float alphaMult)
{
	g_decalMan.AddScriptedTrailInfo(VECTOR3_TO_VEC3V((Vector3)pos), alphaMult, true);
}

//
// name:		CommandEndSkidmarkDecals
// description:

void CommandEndSkidmarkDecals()
{
	g_decalMan.EndScriptedTrail();
}

//
// name:		CommandStartPetrolTrailDecals
// description:

void CommandStartPetrolTrailDecals(float width)
{
	s32 renderSettingsIndex = -1;

	VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(VFXLIQUID_TYPE_PETROL);
	Color32 col(liquidInfo.decalColR, liquidInfo.decalColG, liquidInfo.decalColB, liquidInfo.decalColA);

	g_decalMan.StartScriptedTrail(DECALTYPE_TRAIL_LIQUID, VFXLIQUID_TYPE_PETROL, renderSettingsIndex, width, col);
}

//
// name:		CommandAddPetrolTrailDecalInfo
// description:

void CommandAddPetrolTrailDecalInfo(const scrVector & pos, float alphaMult)
{
	g_decalMan.AddScriptedTrailInfo(VECTOR3_TO_VEC3V((Vector3)pos), alphaMult, true);
}

//
// name:		CommandEndPetrolTrailDecals
// description:

void CommandEndPetrolTrailDecals()
{
	g_decalMan.EndScriptedTrail();
}

//
// name:		CommandRegisterMarkerDecal
// description:

void CommandRegisterMarkerDecal(int renderSettingId, const scrVector & pos, const scrVector & dir, const scrVector & side, float width, float height, float alpha, bool isLongRange, bool isDynamic)
{
	Color32 col(255, 255, 255, static_cast<u8>(alpha*255));

	s32 decalRenderSettingIndex;
	s32 decalRenderSettingCount;
	g_decalMan.FindRenderSettingInfo(renderSettingId, decalRenderSettingIndex, decalRenderSettingCount);	
	g_decalMan.RegisterMarker(decalRenderSettingIndex, decalRenderSettingCount, VECTOR3_TO_VEC3V((Vector3)pos), VECTOR3_TO_VEC3V((Vector3)dir), VECTOR3_TO_VEC3V((Vector3)side), width, height, col, isLongRange, isDynamic, false);
}


//
// name:		CommandRemoveDecal
// description:

void CommandRemoveDecal(int decalId)
{
	g_decalMan.Remove(decalId);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && decalId != 0)
	{
		CReplayMgr::RecordPersistantFx<CPacketRemoveScriptDecal>(
			CPacketRemoveScriptDecal(),
			CTrackedEventInfo<tTrackedDecalType>(decalId),
			NULL, 
			false);
	}
#endif // GTA_REPLAY
}

//
// name:		CommandIsDecalAlive
// description:

bool CommandIsDecalAlive(int decalId)
{
	return g_decalMan.IsAlive(decalId);
}

//
// name:		CommandGetDecalWashLevel
// description:

float CommandGetDecalWashLevel(int decalId)
{
	return g_decalMan.GetWashLevel(decalId);
}

//
// name:		CommandDisablePetrolDecalsIgniting
// description:

void CommandDisablePetrolDecalsIgniting(bool bVal)
{
	g_decalMan.SetDisablePetrolDecalsIgniting(bVal);
}

//
// name:		CommandDisablePetrolDecalsIgnitingThisFrame
// description:

void CommandDisablePetrolDecalsIgnitingThisFrame()
{
	g_decalMan.SetDisablePetrolDecalsIgnitingThisFrame();
}

//
// name:		CommandDisablePetrolDecalsRecyclingThisFrame
// description:

void CommandDisablePetrolDecalsRecyclingThisFrame()
{
	g_decalMan.SetDisablePetrolDecalsRecyclingThisFrame();
}

//
// name:		CommandDisableDecalRenderingThisFrame
// description:

void CommandDisableDecalRenderingThisFrame()
{
	g_decalMan.SetDisableRenderingThisFrame();

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketDisableDecalRendering>(CPacketDisableDecalRendering());
	}
#endif
}

//
// name:		CommandGetIsPetrolDecalInRange
// description:

bool CommandGetIsPetrolDecalInRange(const scrVector & scrVecPos, float range)
{	
	Vector3 pos = Vector3(scrVecPos);

	u32 foundDecalTypeFlag = 0;
	Vec3V vClosestPos;
	fwEntity* pClosestEntity = NULL;
	return g_decalMan.FindClosest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, RCC_VEC3V(pos), range, 0.0f, false, false, DECAL_FADE_OUT_TIME, vClosestPos, &pClosestEntity, foundDecalTypeFlag);
}

//
// name:		CommandPatchDecalDiffuseMap
// description:

void CommandPatchDecalDiffuseMap(int renderSettingId, const char* pDiffuseMapDictionaryName, const char* pDiffuseMapName)
{
	grcTexture* pDiffuseMap = GetTextureFromStreamedTxd(pDiffuseMapDictionaryName, pDiffuseMapName);
	if (pDiffuseMap)
	{
		g_decalMan.PatchDiffuseMap(renderSettingId, pDiffuseMap);
	}
}

//
// name:		CommandUnPatchDecalDiffuseMap
// description:

void CommandUnPatchDecalDiffuseMap(int renderSettingId)
{
	grcTexture* pDiffuseMap = const_cast<grcTexture*>(grcTexture::None);
	if (pDiffuseMap)
	{
		g_decalMan.PatchDiffuseMap(renderSettingId, pDiffuseMap);
	}
}

//
// name:		CommandMoveVehicleDecals
// description:

void CommandMoveVehicleDecals(int VehicleIndexFrom, int VehicleIndexTo)
{
	CVehicle* pVehicleFrom = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndexFrom);
	CVehicle* pVehicleTo = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndexTo);

	if (pVehicleFrom && pVehicleTo)
	{
		g_decalMan.UpdateAttachEntity(pVehicleFrom, pVehicleTo);
	}
}

//
// name:		CommandAddVehicleCrewEmblem
// description:

bool CommandAddVehicleCrewEmblem(int vehicleIndex, int pedIndex, int vehicleBoneIndex, const scrVector & vOffsetPos, const scrVector & vDir, const scrVector & vSide, float size, int badgeIndex, int alpha)
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
	scriptAssertf(pVehicle, "%s:ADD_VEHICLE_CREW_EMBLEM - not a valid vehicleIndex='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vehicleIndex);
	if (pVehicle)
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
		if (pPed)
		{
			CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
			scriptAssertf(pPlayerInfo, "ADD_VEHICLE_CREW_EMBLEM - the ped is not a player so crew emblem cannot be found");
		
			if (pPlayerInfo)
			{
				NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
				rlGamerHandle gamerHandle = pPlayerInfo->m_GamerInfo.GetGamerHandle();
				const rlClanDesc* pMyClan = clanMgr.GetPrimaryClan(gamerHandle);

				scriptAssertf(pMyClan && pMyClan->IsValid(), "%s:ADD_VEHICLE_CREW_EMBLEM - no valid Primary clan.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				if (pMyClan && pMyClan->IsValid())
				{
					EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)pMyClan->m_Id);

					CVehicleBadgeDesc badgeDesc;
					badgeDesc.Set(emblemDesc);

					return g_decalMan.ProcessVehicleBadge(pVehicle, badgeDesc, vehicleBoneIndex, (Vec3V)vOffsetPos, (Vec3V)vDir, (Vec3V)vSide, size, pPlayerInfo->m_GamerInfo.IsLocal(), badgeIndex, (u8)(alpha&0xff));
				}
			}
		}
	}

	return false;
}

//
// name:		CommandAddVehicleCrewEmblemUsingTexture
// description:

bool CommandAddVehicleCrewEmblemUsingTexture(int vehicleIndex, int pedIndex, const char* pTextureDictionaryName, const char* pTextureName, int vehicleBoneIndex, const scrVector & vOffsetPos, const scrVector & vDir, const scrVector & vSide, float size, int badgeIndex, int alpha)
{	
	grcTexture* pTexture = GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);
	scriptAssertf(pTexture, "%s:ADD_VEHICLE_CREW_EMBLEM_USING_TEXTURE - not a valid texture='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pTextureName);
	if (pTexture)
	{
		CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);
		scriptAssertf(pVehicle, "%s:ADD_VEHICLE_CREW_EMBLEM_USING_TEXTURE - not a valid vehicleIndex='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vehicleIndex);
		if (pVehicle)
		{
			CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
			if (pPed)
			{
				CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
				scriptAssertf(pPlayerInfo, "ADD_VEHICLE_CREW_EMBLEM_USING_TEXTURE - the ped is not a player so crew emblem cannot be found");

				if (pPlayerInfo)
				{
					CVehicleBadgeDesc badgeDesc;
					badgeDesc.Set(atHashWithStringNotFinal(pTextureDictionaryName), atHashWithStringNotFinal(pTextureName));

					return g_decalMan.ProcessVehicleBadge(pVehicle, badgeDesc, vehicleBoneIndex, (Vec3V)vOffsetPos, (Vec3V)vDir, (Vec3V)vSide, size, pPlayerInfo->m_GamerInfo.IsLocal(), badgeIndex, (u8)(alpha&0xff));
				}
			}
		}
	}

	return false;
}

//
// name:		CommandAddVehicleTournamentEmblem
// description:

bool CommandAddVehicleTournamentEmblem(int vehicleIndex, int tournamentId, int vehicleBoneIndex, const scrVector & vOffsetPos, const scrVector & vDir, const scrVector & vSide, float size, int badgeIndex, int alpha)
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex);

	scriptAssertf(tournamentId != EmblemDescriptor::INVALID_EMBLEM_ID, "ADD_VEHICLE_TOURNAMENT_EMBLEM - invalid tournament id (%d)", tournamentId);

	if (pVehicle && tournamentId != EmblemDescriptor::INVALID_EMBLEM_ID)
	{
		EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_TOURNAMENT, (EmblemId)tournamentId);

		CVehicleBadgeDesc badgeDesc;
		badgeDesc.Set(emblemDesc);

		return g_decalMan.ProcessVehicleBadge(pVehicle, badgeDesc, vehicleBoneIndex, (Vec3V)vOffsetPos, (Vec3V)vDir, (Vec3V)vSide, size, false, badgeIndex, (u8)(alpha&0xff));
	}

	return false;
}

//
// name:		CommandAbortVehicleCrewEmblem
// description:

void CommandAbortVehicleCrewEmblem(int VehicleIndex)
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		g_decalMan.AbortVehicleBadgeRequests(pVehicle);
	}
}

//
// name:		CommandRemoveVehicleCrewEmblem
// description:

void CommandRemoveVehicleCrewEmblem(int VehicleIndex, int badgeIndex)
{	
	CVehicle* pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		if(badgeIndex < 0)
		{
			g_decalMan.RemoveAllVehicleBadges(pVehicle);
		}
		else
		{
			g_decalMan.RemoveVehicleBadge(pVehicle, badgeIndex);
		}
	}
}

//
// name:		CommandGetVehicleBadgeRequestState
// description:

s32 CommandGetVehicleBadgeRequestState(int VehicleIndex, int badgeIndex)
{	
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		return (s32)g_decalMan.GetVehicleBadgeRequestState(pVehicle, badgeIndex);
	}

	scriptAssertf(0, "querying the state of an invalid vehicle");
	return (s32)DECALREQUESTSTATE_NOT_ACTIVE;
}

//
// name:		CommandDoesVehicleHaveCrewEmblem
// description:

bool CommandDoesVehicleHaveCrewEmblem(int VehicleIndex, int badgeIndex)
{	
	const CVehicle* pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);
	if (pVehicle)
	{
		return g_decalMan.DoesVehicleHaveBadge(pVehicle,badgeIndex);
	}

	return false;
}

//
// name:		CommandDisableFootprintDecals
// description:

void CommandDisableFootprintDecals(bool val)
{	
	g_decalMan.SetDisableFootprints(val, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}


//
// name:		CommandDisableCompositeShotgunDecals
// description:

void CommandDisableCompositeShotgunDecals(bool val)
{	
	g_decalMan.SetDisableCompositeShotgunImpacts(val, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}

//
// name:		CommandDisableScuffDecals
// description:

void CommandDisableScuffDecals(bool val)
{	
	g_decalMan.SetDisableScuffs(val, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}

//
// name:		CommandSetBulletImpactDecalRangeScale
// description:

void CommandSetBulletImpactDecalRangeScale(float scale)
{
	g_vfxWeapon.SetBulletImpactDecalRangeScale(scale, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}



// --- Vfx --------------------------------------------------------------------------------------------

void CommandOverrideInteriorSmokeName(const char* overrideName)
{
	g_vfxWeapon.SetIntSmokeOverrideHashName(overrideName);
}

void CommandOverrideInteriorSmokeNameLens(const char* UNUSED_PARAM(overrideName))
{
	scriptAssertf(0, "OVERRIDE_INTERIOR_SMOKE_NAME_LENS is no longer supported - please remove from the script");
}

void CommandOverrideInteriorSmokeLevel(float overrideLevel)
{
	g_vfxWeapon.SetIntSmokeOverrideLevel(overrideLevel);
}

void CommandOverrideInteriorSmokeEnd()
{
	g_vfxWeapon.ResetIntSmokeOverrides();
}

void CommandRegisterNoirLensEffect()
{
	g_vfxLens.Register(VFXLENSTYPE_NOIR, 1.0f, 0.0f, 0.0f, 1.0f);
}

void CommandDisableVehicleLights(bool value)
{
	g_distantLights.SetDisableVehicleLights(value);
}

void CommandDisableLODLights(bool value)
{
	CLODLights::SetEnabled(!value);
}

void CommandRenderShadowsLightsWithNoShadows(bool render)
{
	Lights::RenderShadowedLightsWithNoShadow(render);
}

void CommandRequestEarlyLightCheck()
{
	CParaboloidShadow::RequestJumpCut();
}

void CommandUseSnowFootVfxWhenUnsheltered(bool enable)
{
	g_vfxPed.SetUseSnowFootVfxWhenUnsheltered(enable);
}

void CommandUseSnowWheelVfxWhenUnsheltered(bool enable)
{
	g_vfxWheel.SetUseSnowWheelVfxWhenUnsheltered(enable);
}

void CommandDisableRegionVfx(bool disable)
{
	g_vfxRegionInfoMgr.SetDisableRegionVfx(disable, CTheScripts::GetCurrentGtaScriptThread()->GetThreadId());
}

// --- Timecycle --------------------------------------------------------------------------------------


void CommandPreSetInteriorAmbientCache(const char *pString)
{
	g_timeCycle.PreSetInteriorAmbientCache(pString);
}

//
// name:		CommandSetTimeCycleModifier
// description:
void CommandSetTimeCycleModifier(const char *pString)
{
	g_timeCycle.SetScriptModifierId(pString);
	PostFX::ResetAdaptedLuminance();
}

//
// name:		CommandSetTimeCycleModifierStrength
// description:
void CommandSetTimeCycleModifierStrength(float strength)
{
	g_timeCycle.SetScriptModifierStrength(strength);
}

//
// name:		CommandSetTimeCycleModifier
// description:
void CommandSetTransitionTimeCycleModifier(const char *pString, float time)
{
	scriptAssertf(time > 0.0f, "%s:SET_TRANSITION_TIMECYCLE_MODIFIER : Trying to transition to %s over 0 seconds, please use SET_TIMECYCLE_MODIFIER.", CTheScripts::GetCurrentScriptName(), pString);
	g_timeCycle.SetTransitionToScriptModifierId(pString,time);
}

//
// name:		CommandSetTransitionOutOfTimeCycleModifier
// description:
void CommandSetTransitionOutOfTimeCycleModifier(float time)
{
	g_timeCycle.SetTransitionOutOfScriptModifierId(time);
}

//
// name:		CommandClearTimeCycleModifier
// description:
void CommandClearTimeCycleModifier()
{
	g_timeCycle.ClearScriptModifierId();
	PostFX::ResetAdaptedLuminance();
}

//
// name:		CommandGetTimeCycleModifierIndex
// description:
int CommandGetTimeCycleModifierIndex()
{
	return g_timeCycle.GetScriptModifierIndex();
}


//
// name:		CommandGetTimeCycleModifierIndex
// description:
int CommandGetTransitionTimeCycleModifierIndex()
{
	return g_timeCycle.GetTransitionScriptModifierIndex();
}

//
// name:		CommandGetIsTimecycleTransitioningOut
// description:
bool CommandGetIsTimecycleTransitioningOut()
{
	return g_timeCycle.GetIsTransitioningOut();
}

//
// name:		CommandPushTimeCycleModifier
// description:
void CommandPushTimeCycleModifier()
{
	g_timeCycle.PushScriptModifier();
}

//
// name:		CommandClearPushedTimeCycleModifier
// description:
void CommandClearPushedTimeCycleModifier()
{
	g_timeCycle.ClearPushedScriptModifier();
}

//
// name:		CommandPushTimeCycleModifier
// description:
void CommandPopTimeCycleModifier()
{
	g_timeCycle.PopScriptModifier();
}

//
// name:		CommandSetCurrentPlayerTimeCycleModifier
// description:
void CommandSetCurrentPlayerTimeCycleModifier(const char *current)
{
	if( current && (current[0] != 0) )
	{
		g_timeCycle.SetPlayerModifierIdCurrent(current);
		g_timeCycle.SetPlayerModifierIdNext(current);
		g_timeCycle.SetPlayerModifierTransitionStrength(0.0f);
	}
	else
	{ // Clear any modifier
		g_timeCycle.SetPlayerModifierIdCurrent(-1);
		g_timeCycle.SetPlayerModifierIdNext(-1);
		g_timeCycle.SetPlayerModifierTransitionStrength(0.0f);
	}
}


//
// name:		CommandStartPlayerTimeCycleModifierTransition
// description:
void CommandStartPlayerTimeCycleModifierTransition(const char *current, const char *next)
{
	scriptAssertf(current, "START_PLAYER_TCMODIFIER_TRANSITION - Called with NULL current modifier.");
	scriptAssertf(next, "START_PLAYER_TCMODIFIER_TRANSITION - Called with NULL next modifier.");
	g_timeCycle.SetPlayerModifierIdCurrent(current);
	g_timeCycle.SetPlayerModifierIdNext(next);
	g_timeCycle.SetPlayerModifierTransitionStrength(0.0f);
}


//
// name:		CommandSetPlayerTimeCycleModifierTransition
// description:
void CommandSetPlayerTimeCycleModifierTransition(float delta)
{
	scriptAssertf(delta >= 0.0f, "SET_PLAYER_TCMODIFIER_TRANSITION - Called with a negative transition delta.");
	scriptAssertf(delta <= 1.0f, "SET_PLAYER_TCMODIFIER_TRANSITION - Called with a transition delta over 1.0f.");
	g_timeCycle.SetPlayerModifierTransitionStrength(delta);
}


//
// name:		CommandSetNextPlayerTimeCycleModifier
// description:
void CommandSetNextPlayerTimeCycleModifier(const char *next)
{
	scriptAssertf(next, "SET_NEXT_PLAYER_TCMODIFIER - Called with NULL next modifier.");

	int nextMod = g_timeCycle.GetPlayerModifierIdNext();
	float nextStrength = 1.0f - g_timeCycle.GetPlayerModifierTransitionStrength();
	g_timeCycle.SetPlayerModifierIdCurrent(nextMod);
	if( next && next[0] != 0 )
		g_timeCycle.SetPlayerModifierIdNext(next);
	else
		g_timeCycle.SetPlayerModifierIdNext(-1);
		
	g_timeCycle.SetPlayerModifierTransitionStrength(nextStrength);
}

//
// name:		CommandSetTimeCycleRegionOverride
// description:
void CommandSetTimeCycleRegionOverride(int regionOverride)
{
	g_timeCycle.SetRegionOverride(regionOverride);
}


//
// name:		CommandGetTimeCycleRegionOverride
// description:
int CommandGetTimeCycleRegionOverride()
{
	return g_timeCycle.GetRegionOverride();
}


//
// name:		CommandAddTimeCycleOverride
// description:
void CommandAddTimeCycleOverride(const char *modifier, const char *modOverride)
{
	g_timeCycle.AddModifierOverride(atHashString(modifier), atHashString(modOverride));
}


//
// name:		CommandRemoveModifierOverride
// description:
void CommandRemoveModifierOverride(const char *modifier)
{
	g_timeCycle.RemoveModifierOverride(atHashString(modifier));
}


//
// name:		CommandClearModifierOverrides
// description:
void CommandClearModifierOverrides(const char *)
{
	g_timeCycle.ClearModifierOverrides();
}


//
// name:		CommandSetExtraTimeCycleModifier
// description:
void CommandSetExtraTimeCycleModifier(const char *pString)
{
	g_timeCycle.SetExtraModifierId(pString);
	PostFX::ResetAdaptedLuminance();
}


//
// name:		CommandClearExtraTimeCycleModifier
// description:
void CommandClearExtraTimeCycleModifier()
{
	g_timeCycle.ClearExtraModifierId();
}


//
// name:		CommandGetExtraTimeCycleModifierIndex
// description:
int CommandGetExtraTimeCycleModifierIndex()
{
	return g_timeCycle.GetExtraModifierIndex();
}


//
// name:		CommandGetDayNightBalance
// description:
float CommandGetDayNightBalance()
{
	return g_timeCycle.GetStaleDayNightBalance();
}

// name:		CommandEnableMoonCycleOverride
// description: Enable and set the moon cycle override
void CommandEnableMoonCycleOverride(float value)
{
	g_timeCycle.SetMoonCycleOverride(Saturate(value));
}

// name:		CommandDisableMoonCycleOverride
// description: Disable the moon cycle override
void CommandDisableMoonCycleOverride()
{
	g_timeCycle.SetMoonCycleOverride(-1.0f);
}


// name:		CommandRequestScaleformMovieCommon
// description: requests that the game streams a scaleform movie in
s32 CommandRequestScaleformMovieCommon(const char *pFilename, bool bDontRenderWhilePaused, bool bIgnoreSuperWidescreen = false)
{
	s32 iReturnId = 0;

	CScaleformMgr::CreateMovieParams params(pFilename);
	params.vPos = Vector3(0,0,0);
	params.vSize = Vector2(0.0f,0.0f);
	params.bRemovable = true;
	params.bRequiresMovieView = true;
	params.iParentMovie = -1;
	params.iDependentMovie = -1;
	params.movieOwnerTag = SF_MOVIE_TAGGED_BY_SCRIPT;
	params.bDontRenderWhilePaused = bDontRenderWhilePaused;
	params.bIgnoreSuperWidescreenScaling = bIgnoreSuperWidescreen;

#if TAMPERACTIONS_ENABLED
    //@@: range COMMANDREQUESTSCALEFORMMOVIECOMMON {
	int phoneId = -1;
	if (!stricmp(pFilename, "cellphone_ifruit"))
	{
		phoneId = 0;
		params.bIgnoreSuperWidescreenScaling = true;
	}
	else if (!stricmp(pFilename, "cellphone_badger"))
	{
		phoneId = 1;
		params.bIgnoreSuperWidescreenScaling = true;
	}
	else if (!stricmp(pFilename, "cellphone_facade"))
	{
		phoneId = 2;
		params.bIgnoreSuperWidescreenScaling = true;
	}
	else if (!stricmp(pFilename, "taxi_display") ||
		!stricmp(pFilename, "cellphone_prologue") ||
		!stricmp(pFilename, "mugshot_board_01") ||
		!stricmp(pFilename, "mugshot_board_02") ||
		!stricmp(pFilename, "digiscanner"))
	{
		params.bIgnoreSuperWidescreenScaling = true;
	}    
#endif // RSG_PC

	s32 iNewId = CScaleformMgr::CreateMovie(params);

	uiAssertf(iNewId != -1, "Scaleform movie '%s' failed to stream", pFilename);

	if (iNewId != -1)
	{
 		if (!stricmp(pFilename, "INSTRUCTIONAL_BUTTONS"))
 		{
 			CBusySpinner::RegisterInstructionalButtonMovie(iNewId);  // register this "instructional button" movie with the spinner system
 		}

		CScriptResource_ScaleformMovie ScaleformMovie(iNewId, pFilename);

		if (CTheScripts::GetCurrentGtaScriptHandler())
		{
			iReturnId = CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResourceAndGetRef(ScaleformMovie);
#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_DISABLEPHONE
			if(phoneId >= 0 && TamperActions::IsPhoneDisabled(phoneId))
			{
				iReturnId = 0;
			}
#endif
    //@@: } COMMANDREQUESTSCALEFORMMOVIECOMMON
			uiDebugf1("Script %s has requested Scaleform Movie %s, returning id %d", CTheScripts::GetCurrentScriptName(), pFilename, iReturnId);

			uiAssertf(iReturnId > 0, "Not enough Scaleform script slots for movie '%s'", pFilename);
		}

		// moved from CScaleformMgr::CreateMovieInternal
		// ...we'd prefer this storing to be done more game-side even if they get spread around the code a bit
#if GTA_REPLAY
		if (!strcmp(pFilename, "cellphone_iFruit"))
			CNewHud::GetReplaySFController().SetMovieID(CUIReplayScaleformController::OverlayType::Cellphone_iFruit, iNewId);
		else if (!strcmp(pFilename, "cellphone_badger"))
			CNewHud::GetReplaySFController().SetMovieID(CUIReplayScaleformController::OverlayType::Cellphone_Badger, iNewId);
		else if (!strcmp(pFilename, "cellphone_facade"))
			CNewHud::GetReplaySFController().SetMovieID(CUIReplayScaleformController::OverlayType::Cellphone_Facade, iNewId);
		else if (!strcmp(pFilename, "observatory_scope"))
			CNewHud::GetReplaySFController().SetTelescopeMovieID(iNewId);
		else if (!strcmp(pFilename, "turret_cam"))
			CNewHud::GetReplaySFController().SetTurretCamMovieID(iNewId);
		else if (!strcmp(pFilename, "binoculars"))
			CNewHud::GetReplaySFController().SetBinocularsMovieID(iNewId);
		else if (!strcmp(pFilename, "DRONE_CAM"))
			CNewHud::GetReplaySFController().SetMovieID(CUIReplayScaleformController::OverlayType::Drone_Camera, iNewId);
#endif // GTA_REPLAY
	}

	return iReturnId;
}

s32 CommandRequestScaleformMovie(const char *pFilename)
{
	return CommandRequestScaleformMovieCommon(pFilename, false);
}

s32 CommandRequestScaleformMovieWithIgnoreSuperWidescreen(const char *pFilename)
{
	return CommandRequestScaleformMovieCommon(pFilename, false, true);
}

s32 CommandRequestScaleformMovieSkipRenderWhilePaused(const char *pFilename)
{
	return CommandRequestScaleformMovieCommon(pFilename, true);
}

//
// name:		CommandHasScaleformMovieLoaded
// description: returns whether the passed scaleform movie has loaded in yet
bool CommandHasScaleformMovieLoaded(s32 iIndex)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return false;

	bool bLoaded = false;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "HAS_SCALEFORM_MOVIE_LOADED - Invalid Scaleform movie id (%d)", iIndex))
	{
		if (CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iIndex-1].iId))
		{
			if (CScriptHud::ScriptScaleformMovie[iIndex-1].bActive)  // we must check whether the movie has been set to be active by CScriptHud
			{
				if (CScriptHud::ScriptScaleformMovie[iIndex-1].iParentMovie != -1)  // this movie has a parent so it is a child movie so we need to check whether bGfxReady is set yet
				{
					bLoaded = CScriptHud::ScriptScaleformMovie[iIndex-1].bGfxReady;  // only return that this movie has loaded if we have set bGfxReady for the child
				}
				else
				{
					bLoaded = true;  // no parent but its active, so its ready
				}
			}
		}
	}

	return bLoaded;
}

bool CommandIsActiveScaleformMovieDeleting(s32 iIndex)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return false;

	bool bResult = false;
	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "HAS_SCALEFORM_MOVIE_LOADED - Invalid Scaleform movie id (%d)", iIndex))
	{
		if (CommandHasScaleformMovieLoaded(iIndex))
		{
			bResult = CScriptHud::ScriptScaleformMovie[iIndex-1].bDeleteRequested;
		}
	}

	return bResult;
}

bool CommandIsScaleformMovieDeleting(s32 iIndex)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return false;

	bool bResult = false;
	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "HAS_SCALEFORM_MOVIE_LOADED - Invalid Scaleform movie id (%d)", iIndex))
	{
		bResult = CScriptHud::ScriptScaleformMovie[iIndex-1].bDeleteRequested;
	}

	return bResult;
}

//
// name:		CommandHasScaleformMovieFilenameLoaded
// description: returns whether the passed scaleform movie filename has loaded in yet
bool CommandHasScaleformMovieFilenameLoaded(const char *pFilename)
{
	if (pFilename)
	{
		for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)
		{
			if (CScriptHud::ScriptScaleformMovie[iCount].bActive)
			{
				if (!stricmp(CScriptHud::ScriptScaleformMovie[iCount].cFilename, pFilename))
				{
					return CommandHasScaleformMovieLoaded(iCount+1);
				}
			}
		}
	}

	return false;
}



//
// name:		CommandHasScaleformContainerMovieLoadedIntoParent
// description: returns the id of the movie that has been requested
bool CommandHasScaleformContainerMovieLoadedIntoParent(s32 iIndex)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return false;

	bool bAreAllChildrenLoaded = false;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "HAS_SCALEFORM_CONTAINER_MOVIE_LOADED_INTO_PARENT - Invalid Scaleform movie id (%d)", iIndex))
	{
		if (CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iIndex-1].iId))
		{
			for (s32 iCount = 0; iCount < NUM_SCRIPT_SCALEFORM_MOVIES; iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].bActive)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iIndex-1)
					{
						bAreAllChildrenLoaded = CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iCount].iId);

						if (!bAreAllChildrenLoaded)
						{
							return false;  // atleast one of the children isnt loaded yet, so just return false
						}
						else
						{
							if (!CommandHasScaleformMovieLoaded(iCount+1))
							{
								return false;
							}
						}
					}
				}
			}
		}
	}

	return (bAreAllChildrenLoaded);
}




//
// name:		CommandSetScaleformMovieToUseSystemTime
// description: sets the scaleform movie to use the system time instead of game time which is default
void CommandSetScaleformMovieToUseSystemTime(s32 iIndex, bool bUseSystemTime)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "SET_SCALEFORM_MOVIE_TO_USE_SYSTEM_TIME - Invalid Scaleform movie id (%d)", iIndex))
	{
		if (CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iIndex-1].iId))
		{
			if (CScriptHud::ScriptScaleformMovie[iIndex-1].bActive)  // we must check whether the movie has been set to be active by CScriptHud
			{
#if RSG_GEN9			
				if(SCRIPT_VERIFY(!CScriptHud::ScriptScaleformMovie[iIndex-1].bRenderRequestedOnce, "SET_SCALEFORM_MOVIE_TO_USE_SYSTEM_TIME - must be called on a movie before its rendered for the 1st time"))
#else
				if(SCRIPT_VERIFY(!CScriptHud::ScriptScaleformMovie[iIndex-1].bRenderedOnce, "SET_SCALEFORM_MOVIE_TO_USE_SYSTEM_TIME - must be called on a movie before its rendered for the 1st time"))
#endif // RSG_GEN9
				{
					CScriptHud::ScriptScaleformMovie[iIndex-1].bUseSystemTimer = bUseSystemTime;
				}
			}
		}
	}
}

//
// name:		CommandSetScaleformMovieToUseLargeRT
// description: sets the scaleform movie to use large Render target rendering (as seen on the yacht)
void CommandSetScaleformMovieToUseLargeRT(s32 iIndex, bool bUseLargeRT)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "SET_SCALEFORM_MOVIE_TO_USE_LARGE_RT - Invalid Scaleform movie id (%d)", iIndex))
	{
		if (CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iIndex-1].iId))
		{
			CScriptHud::ScriptScaleformMovie[iIndex-1].iLargeRT = bUseLargeRT ? 1 : 0;
		}
	}
}

//
// name:		CommandSetScaleformMovieToUseSuperLargeRT
// description: sets the scaleform movie to use large Render target rendering (as seen on the yacht)
void CommandSetScaleformMovieToUseSuperLargeRT(s32 iIndex, bool bUseSuperLargeRT)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "SET_SCALEFORM_MOVIE_TO_USE_LARGE_RT - Invalid Scaleform movie id (%d)", iIndex))
	{
		if (CScaleformMgr::IsMovieActive(CScriptHud::ScriptScaleformMovie[iIndex-1].iId))
		{
			CScriptHud::ScriptScaleformMovie[iIndex-1].iLargeRT = bUseSuperLargeRT ? 2 : 0;
		}
	}
}



//
// name:		CommandSetScaleformMovieAsNoLongerNeeded
// description: removes sets the scaleform movie as no longer needed
void CommandSetScaleformMovieAsNoLongerNeeded(s32 &iIndex)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(uiVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "SET_SCALEFORM_MOVIE_AS_NO_LONGER_NEEDED - Invalid Scaleform movie id (%d)", iIndex))
	{
		// remove the actual movie:
		{
			if (CTheScripts::GetCurrentGtaScriptHandler())
			{
				if (CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_SCALEFORM_MOVIE, iIndex) != NULL)
				{
					uiDisplayf("Script %s is attempting to set movie %s (%d) (SF %d) as no longer needed", CTheScripts::GetCurrentScriptName(), CScriptHud::ScriptScaleformMovie[iIndex-1].cFilename, iIndex, CScriptHud::ScriptScaleformMovie[iIndex-1].iId);
					CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_SCALEFORM_MOVIE, iIndex);
					iIndex = 0;
				}
				else
				{
#if !__NO_OUTPUT
					uiDisplayf("Script %s attempted to set movie %s (%d) (SF %d) as no longer needed, but the movie doesn't appear in the cleanup list for that script", CTheScripts::GetCurrentScriptName(), CScriptHud::ScriptScaleformMovie[iIndex-1].cFilename, iIndex, CScriptHud::ScriptScaleformMovie[iIndex-1].iId);

					scriptHandler *pActualOwnerOfMovie = CTheScripts::GetScriptHandlerMgr().GetScriptHandlerForResource(CGameScriptResource::SCRIPT_RESOURCE_SCALEFORM_MOVIE, iIndex, CTheScripts::GetCurrentGtaScriptHandler());
					if (pActualOwnerOfMovie != NULL)
					{
						uiDisplayf("Script %s attempted to set movie %s (%d) (SF %d) as no longer needed, but the movie appears in the cleanup list for script handler %s", CTheScripts::GetCurrentScriptName(), CScriptHud::ScriptScaleformMovie[iIndex-1].cFilename, iIndex, CScriptHud::ScriptScaleformMovie[iIndex-1].iId, pActualOwnerOfMovie->GetLogName());
					}
#endif	//	#if !__NO_OUTPUT
				}
			}
		}
	}
}



//
// name:		CommandCallScaleformMovieFunction
// description: calls a actionscript method inside the scaleform movie
void CommandCallScaleformMovieMethod(s32 iIndex, const char *pMethodName)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "CALL_SCALEFORM_MOVIE_METHOD - Invalid Scaleform movie id (%d)", iIndex))
	{
		s32 iMovieIndex = iIndex - 1;
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;
#endif
			// lets see if this movie is a parent of any children, if so, link the child to this movie:
			s32 iLinkedScriptMovieId = -1;

			for (s32 iCount = 0; ((iCount < NUM_SCRIPT_SCALEFORM_MOVIES) && (iLinkedScriptMovieId == -1)); iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].iId != -1 && CScriptHud::ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iMovieIndex)
					{
						iLinkedScriptMovieId = iCount;
					}
				}
			}

#if __DEV
			if (iLinkedScriptMovieId == -1)
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename);
			}
			else
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s linked to %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename, CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].cFilename);
			}
#endif // __DEV

			CScaleformMgr::CallMethodFromScript(CScriptHud::ScriptScaleformMovie[iMovieIndex].iId, SF_BASE_CLASS_SCRIPT, pMethodName, iLinkedScriptMovieId);
		}
	}
}

void CommandCallScaleformMovieMethodWithNumber(s32 iIndex, const char *pMethodName, float fParam1, float fParam2, float fParam3, float fParam4, float fParam5)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "CALL_SCALEFORM_MOVIE_METHOD_WITH_NUMBER - Invalid Scaleform movie id (%d)", iIndex))
	{
		s32 iMovieIndex = iIndex - 1;	
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;
#endif

			// lets see if this movie is a parent of any children, if so, link the child to this movie:
			s32 iLinkedScriptMovieId = -1;

			for (s32 iCount = 0; ((iCount < NUM_SCRIPT_SCALEFORM_MOVIES) && (iLinkedScriptMovieId == -1)); iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].iId != -1 && CScriptHud::ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iMovieIndex)
					{
						iLinkedScriptMovieId = iCount;
					}
				}
			}

#if __DEV
			if (iLinkedScriptMovieId == -1)
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename);
			}
			else
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s linked to %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename, CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].cFilename);
			}
#endif // __DEV

			CScaleformMgr::CallMethodFromScript(CScriptHud::ScriptScaleformMovie[iMovieIndex].iId, SF_BASE_CLASS_SCRIPT, pMethodName, iLinkedScriptMovieId, fParam1, fParam2, fParam3, fParam4, fParam5);
		}
	}
}


void CommandCallScaleformMovieMethodWithString(s32 iIndex, const char *pMethodName, const char *pParam1, const char *pParam2, const char *pParam3, const char *pParam4, const char *pParam5)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "CALL_SCALEFORM_MOVIE_METHOD_WITH_STRING - Invalid Scaleform movie id (%d)", iIndex))
	{
		s32 iMovieIndex = iIndex - 1;
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;
#endif
			// lets see if this movie is a parent of any children, if so, link the child to this movie:
			s32 iLinkedScriptMovieId = -1;

			for (s32 iCount = 0; ((iCount < NUM_SCRIPT_SCALEFORM_MOVIES) && (iLinkedScriptMovieId == -1)); iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].iId != -1 && CScriptHud::ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iMovieIndex)
					{
						iLinkedScriptMovieId = iCount;
					}
				}
			}

#if __DEV
			if (iLinkedScriptMovieId == -1)
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename);
			}
			else
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s linked to %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename, CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].cFilename);
			}
#endif // __DEV

			CScaleformMgr::CallMethodFromScript(CScriptHud::ScriptScaleformMovie[iMovieIndex].iId, SF_BASE_CLASS_SCRIPT, pMethodName, iLinkedScriptMovieId, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, pParam1, pParam2, pParam3, pParam4, pParam5, true);
		}
	}
}

void CommandCallScaleformMovieMethodWithNumberAndString(s32 iIndex, const char *pMethodName, float fParam1, float fParam2, float fParam3, float fParam4, float fParam5, const char *pParam1, const char *pParam2, const char *pParam3, const char *pParam4, const char *pParam5)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "CALL_SCALEFORM_MOVIE_METHOD_WITH_NUMBER_AND_STRING - Invalid Scaleform movie id (%d)", iIndex))
	{
		s32 iMovieIndex = iIndex - 1;
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;
#endif

			// lets see if this movie is a parent of any children, if so, link the child to this movie:
			s32 iLinkedScriptMovieId = -1;

			for (s32 iCount = 0; ((iCount < NUM_SCRIPT_SCALEFORM_MOVIES) && (iLinkedScriptMovieId == -1)); iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].iId != -1 && CScriptHud::ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iMovieIndex)
					{
						iLinkedScriptMovieId = iCount;
					}
				}
			}

#if __DEV
			if (iLinkedScriptMovieId == -1)
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename);
			}
			else
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s linked to %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename, CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].cFilename);
			}
#endif // __DEV

			CScaleformMgr::CallMethodFromScript(CScriptHud::ScriptScaleformMovie[iMovieIndex].iId, SF_BASE_CLASS_SCRIPT, pMethodName, iLinkedScriptMovieId, fParam1, fParam2, fParam3, fParam4, fParam5, pParam1, pParam2, pParam3, pParam4, pParam5, true);
		}
	}
}

void CommandCallScaleformMovieMethodWithLiteralString(s32 iIndex, const char *pMethodName, const char *pParam1, const char *pParam2, const char *pParam3, const char *pParam4, const char *pParam5)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "CALL_SCALEFORM_MOVIE_METHOD_WITH_LITERAL_STRING - Invalid Scaleform movie id (%d)", iIndex))
	{
		s32 iMovieIndex = iIndex - 1;
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;
#endif

			// lets see if this movie is a parent of any children, if so, link the child to this movie:
			s32 iLinkedScriptMovieId = -1;

			for (s32 iCount = 0; ((iCount < NUM_SCRIPT_SCALEFORM_MOVIES) && (iLinkedScriptMovieId == -1)); iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].iId != -1 && CScriptHud::ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iMovieIndex)
					{
						iLinkedScriptMovieId = iCount;
					}
				}
			}

#if __DEV
			if (iLinkedScriptMovieId == -1)
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename);
			}
			else
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s linked to %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename, CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].cFilename);
			}
#endif // __DEV

			CScaleformMgr::CallMethodFromScript(CScriptHud::ScriptScaleformMovie[iMovieIndex].iId, SF_BASE_CLASS_SCRIPT, pMethodName, iLinkedScriptMovieId, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, pParam1, pParam2, pParam3, pParam4, pParam5, false);
		}
	}
}


void CommandCallScaleformMovieMethodWithNumberAndLiteralString(s32 iIndex, const char *pMethodName, float fParam1, float fParam2, float fParam3, float fParam4, float fParam5, const char *pParam1, const char *pParam2, const char *pParam3, const char *pParam4, const char *pParam5)
{
	if (iIndex == 0)  // we ignore invalid ids silently
		return;

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "CALL_SCALEFORM_MOVIE_METHOD_WITH_NUMBER_AND_LITERAL_STRING - Invalid Scaleform movie id (%d)", iIndex))
	{
		s32 iMovieIndex = iIndex - 1;
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;
#endif

			// lets see if this movie is a parent of any children, if so, link the child to this movie:
			s32 iLinkedScriptMovieId = -1;

			for (s32 iCount = 0; ((iCount < NUM_SCRIPT_SCALEFORM_MOVIES) && (iLinkedScriptMovieId == -1)); iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].iId != -1 && CScriptHud::ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iMovieIndex)
					{
						iLinkedScriptMovieId = iCount;
					}
				}
			}

#if __DEV
			if (iLinkedScriptMovieId == -1)
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename);
			}
			else
			{
				sfDebugf3("Script %s called ActionScript method: %s on movie %s linked to %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename, CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].cFilename);
			}
#endif // __DEV

			CScaleformMgr::CallMethodFromScript(CScriptHud::ScriptScaleformMovie[iMovieIndex].iId, SF_BASE_CLASS_SCRIPT, pMethodName, iLinkedScriptMovieId, fParam1, fParam2, fParam3, fParam4, fParam5, pParam1, pParam2, pParam3, pParam4, pParam5, false);
		}
	}
}



//
// name:		CommandBeginScaleformScriptHudMovieMethod
// description: BEGIN scaleform movie method call for Script Hud elements
bool CommandBeginScaleformScriptHudMovieMethod(s32 iIndex, const char *pMethodName)
{
	if(SCRIPT_VERIFY(iIndex >= 0, "BEGIN_SCALEFORM_SCRIPT_HUD_MOVIE_METHOD - Invalid script hud component id - not valid in eSCRIPT_HUD_COMPONENT"))
	{
		iIndex += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

		if(SCRIPT_VERIFY(CNewHud::IsHudComponentActive(iIndex), "BEGIN_SCALEFORM_SCRIPT_HUD_MOVIE_METHOD - Invalid script hud component id - movie probably hasnt streamed in yet or is not active"))
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;

			sfDebugf3("Script %s called ActionScript method: %s on ", CTheScripts::GetCurrentScriptName(), pMethodName);
#endif // __DEV
			return (CHudTools::BeginHudScaleformMethod(static_cast<eNewHudComponents>(iIndex), pMethodName));
		}
	}

	return false;
}



//
// name:		CommandBeginScaleformMovieMethod
// description: BEGIN scaleform movie method call
bool CommandBeginScaleformMovieMethod(s32 iIndex, const char *pMethodName)
{
	if (iIndex == 0)  // we ignore invalid ids silently
	{
		uiErrorf("BEGIN_SCALEFORM_MOVIE_METHOD - Attempting to call %s on invalid movie index 0", pMethodName);
		return false;
	}

	if(scriptVerifyf(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "BEGIN_SCALEFORM_MOVIE_METHOD - Invalid Scaleform movie id (%d)", iIndex))
	{
		s32 iMovieIndex = iIndex - 1;
		{
#if __DEV
			if (pMethodName[0] == '$')
				pMethodName++;
#endif

#if RSG_GEN9
			if (!CScriptHud::ScriptScaleformMovie[iMovieIndex].bRenderRequestedOnce)
#else
			if (!CScriptHud::ScriptScaleformMovie[iMovieIndex].bRenderedOnce)
#endif
			{
				CScriptHud::ScriptScaleformMovie[iMovieIndex].bInvokedBeforeRender = true;  // flag as we are invoking before it has been rendered for the 1st time (so this flags it to update before the 1st render)
			}

			// lets see if this movie is a parent of any children, if so, link the child to this movie:
			s32 iLinkedScriptMovieId = -1;

			for (s32 iCount = 0; ((iCount < NUM_SCRIPT_SCALEFORM_MOVIES) && (iLinkedScriptMovieId == -1)); iCount++)
			{
				if (CScriptHud::ScriptScaleformMovie[iCount].iId != -1 && CScriptHud::ScriptScaleformMovie[iCount].iParentMovie != -1)
				{
					if (CScriptHud::ScriptScaleformMovie[iCount].iParentMovie == iMovieIndex)
					{
						iLinkedScriptMovieId = iCount;
					}
				}
			}

			s32 iLinkedMovie;

			if (iLinkedScriptMovieId == -1)
			{
#if __DEV
				sfDebugf3("Script %s called ActionScript method: %s on movie %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename);
#endif // __DEV
				iLinkedMovie = -1;
			}
			else
			{
#if __DEV
				sfDebugf3("Script %s called ActionScript method: %s on movie %s linked to %s", CTheScripts::GetCurrentScriptName(), pMethodName, CScriptHud::ScriptScaleformMovie[iMovieIndex].cFilename, CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].cFilename);
#endif // __DEV
				iLinkedMovie = CScriptHud::ScriptScaleformMovie[iLinkedScriptMovieId].iId;
			}

			return (CScaleformMgr::BeginMethod(CScriptHud::ScriptScaleformMovie[iMovieIndex].iId, SF_BASE_CLASS_SCRIPT, pMethodName, iLinkedMovie));
		}
	}

	return false;
}




//
// name:		CommandBeginScaleformMovieMethodOnFrontend
// description: BEGIN scaleform movie method call on pausemenu content
bool CommandBeginScaleformMovieMethodOnFrontend(const char *pMethodName)
{
	if(SCRIPT_VERIFY(CPauseMenu::IsActive(), "BEGIN_SCALEFORM_MOVIE_METHOD_ON_FRONTEND - Frontend not active"))
	{
		if(SCRIPT_VERIFY(CPauseMenu::GetContentMovieId() >= 0, "BEGIN_SCALEFORM_MOVIE_METHOD_ON_FRONTEND - Pause menu content not active"))
		{
			sfDebugf3("Script %s called ActionScript method on frontend content: %s", CTheScripts::GetCurrentScriptName(), pMethodName);

			return (CScaleformMgr::BeginMethod(CPauseMenu::GetContentMovieId(), SF_BASE_CLASS_PAUSEMENU, pMethodName, CPauseMenu::GetContentMovieId()));
		}
	}

	return false;
}


//
// name:		CommandBeginScaleformMovieMethodOnFrontendHeader
// description: BEGIN scaleform movie method call on pausemenu header
bool CommandBeginScaleformMovieMethodOnFrontendHeader(const char *pMethodName)
{
	if(SCRIPT_VERIFY(CPauseMenu::IsActive(), "BEGIN_SCALEFORM_MOVIE_METHOD_ON_FRONTEND_HEADER - Frontend not active"))
	{
		if(SCRIPT_VERIFY(CPauseMenu::GetHeaderMovieId() >= 0, "BEGIN_SCALEFORM_MOVIE_METHOD_ON_FRONTEND_HEADER - Pause menu header not active"))
		{
			sfDebugf3("Script %s called ActionScript method on frontend content: %s", CTheScripts::GetCurrentScriptName(), pMethodName);

			return (CScaleformMgr::BeginMethod(CPauseMenu::GetHeaderMovieId(), SF_BASE_CLASS_PAUSEMENU, pMethodName, CPauseMenu::GetHeaderMovieId()));
		}
	}

	return false;
}



//
// name:		CommandEndScaleformMovieMethod
// description: END scaleform movie method call - this invokes and returns nothing
void CommandEndScaleformMovieMethod()
{
	CScaleformMgr::EndMethod();
}



//
// name:		CommandEndScaleformMovieMethodReturnValue
// description: END scaleform movie method call - this invokes and returns a slot ID to retrieve the returned value later on
s32 CommandEndScaleformMovieMethodReturnValue()
{
	return (CScaleformMgr::EndMethodReturnValue());  // this will return an INT which will be the slot ID for the return value
}



//
// name:		CommandIsScaleformMovieMethodReturnValueReady
// description: returns whether a movie return method slot is ready to be retrieved
bool CommandIsScaleformMovieMethodReturnValueReady(s32 iUniqueId)
{
	return (CScaleformMgr::IsReturnValueSet(iUniqueId));
}



//
// name:		CommandGetScaleformMovieMethodReturnValueInt
// description: gets the returned value from an earlier invoke based on ID
s32 CommandGetScaleformMovieMethodReturnValueInt(s32 iUniqueId)
{
	return (CScaleformMgr::GetReturnValueInt(iUniqueId));
}



//
// name:		CommandGetScaleformMovieMethodReturnValueFloat
// description: gets the returned value from an earlier invoke based on ID
float CommandGetScaleformMovieMethodReturnValueFloat(s32 iUniqueId)
{
	return (CScaleformMgr::GetReturnValueFloat(iUniqueId));
}



//
// name:		CommandGetScaleformMovieMethodReturnValueBool
// description: gets the returned value from an earlier invoke based on ID
bool CommandGetScaleformMovieMethodReturnValueBool(s32 iUniqueId)
{
	return (CScaleformMgr::GetReturnValueBool(iUniqueId));
}



//
// name:		CommandGetScaleformMovieMethodReturnValueString
// description: gets the returned value from an earlier invoke based on ID
const char *CommandGetScaleformMovieMethodReturnValueString(s32 iUniqueId)
{
	static char cRetString[64];
	cRetString[0] = '\0';

	safecpy(cRetString, CScaleformMgr::GetReturnValueString(iUniqueId), NELEM(cRetString));

	return (const char *)&cRetString;
}



//
// name:		CommandScaleformMovieMethodPassInt
// description: Passes an INT into current building Scaleform movie method call
void CommandScaleformMovieMethodAddParamInt(s32 iParam)
{
	scriptAssertf(!CScriptTextConstruction::IsConstructingText(), "SCALEFORM_MOVIE_METHOD_ADD_PARAM_INT - called inside a BEGIN_TEXT_COMMAND_");
	CScaleformMgr::AddParamInt(iParam);
}

//
// name:		CommandScaleformMovieMethodPassFloat
// description: Passes an FLOAT into current building Scaleform movie method call
void CommandScaleformMovieMethodAddParamFloat(float fParam)
{
	scriptAssertf(!CScriptTextConstruction::IsConstructingText(), "SCALEFORM_MOVIE_METHOD_ADD_PARAM_FLOAT - called inside a BEGIN_TEXT_COMMAND_");
	CScaleformMgr::AddParamFloat(fParam);
}



//
// name:		CommandScaleformMovieMethodAddParamBool
// description: Passes an BOOL into current building Scaleform movie method call
void CommandScaleformMovieMethodAddParamBool(bool bParam)
{
	scriptAssertf(!CScriptTextConstruction::IsConstructingText(), "SCALEFORM_MOVIE_METHOD_ADD_PARAM_BOOL - called inside a BEGIN_TEXT_COMMAND_");
	CScaleformMgr::AddParamBool(bParam);
}



void CommandBeginTextCommandScaleformString(const char *pMainTextLabel)
{
	CScriptTextConstruction::BeginScaleformString(pMainTextLabel);
}

void CommandEndTextCommandScaleformString()
{
	CScriptTextConstruction::EndScaleformString(true);
}

void CommandEndTextCommandUnparsedScaleformString()
{
	CScriptTextConstruction::EndScaleformString(false);
}



//
// name:		CommandScaleformMovieMethodAddParamLiteralString
// description: Passes a literal STRING into current building Scaleform movie method call
void CommandScaleformMovieMethodAddParamLiteralString(const char *pParam)
{
	if (pParam != NULL)
	{
		scriptAssertf(!CScriptTextConstruction::IsConstructingText(), "SCALEFORM_MOVIE_METHOD_ADD_PARAM_LITERAL_STRING - called inside a BEGIN_TEXT_COMMAND_");
		CScaleformMgr::AddParamString(pParam);
	}
}


bool CommandDoesLatestBriefStringExist(int iBriefType)
{
	if (iBriefType == CMessages::PREV_MESSAGE_TYPE_DIALOG ||
		iBriefType == CMessages::PREV_MESSAGE_TYPE_HELP ||
		iBriefType == CMessages::PREV_MESSAGE_TYPE_MISSION)
	{
		return (!CMessages::IsPreviousMessageEmpty((CMessages::ePreviousMessageTypes)iBriefType, 0));
	}
	else
	{
		scriptAssertf(0, "DOES_LATEST_BRIEF_STRING_EXIST - invalid brief type %d", iBriefType);
	}

	return false;
}


//
// name:		CommandScaleformMovieMethodAddParamLatestBriefString
// description: Passes the latest brief string into current building Scaleform movie method call
void CommandScaleformMovieMethodAddParamLatestBriefString(int iBriefType)
{
	scriptAssertf(!CScriptTextConstruction::IsConstructingText(), "SCALEFORM_MOVIE_METHOD_ADD_PARAM_LATEST_BRIEF_STRING - called inside a BEGIN_TEXT_COMMAND_");

	if (iBriefType == CMessages::PREV_MESSAGE_TYPE_DIALOG ||
		iBriefType == CMessages::PREV_MESSAGE_TYPE_HELP ||
		iBriefType == CMessages::PREV_MESSAGE_TYPE_MISSION)
	{
		char cGxtText[MAX_CHARS_IN_EXTENDED_MESSAGE];

		cGxtText[0] = '\0';
		CMessages::FillStringWithPreviousMessage((CMessages::ePreviousMessageTypes)iBriefType, 0, cGxtText, NELEM(cGxtText));

		CScaleformMgr::AddParamString(cGxtText, false);
	}
	else
	{
		scriptAssertf(0, "SCALEFORM_MOVIE_METHOD_ADD_PARAM_LATEST_BRIEF_STRING - invalid brief type %d", iBriefType);
	}
}


//
// name:		CommandSetScaleformScriptHudMovieToFront
// description: sends the script hud component to the front
bool CommandSetScaleformScriptHudMovieToFront(s32 iIndex)
{
	iIndex += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	if (CNewHud::IsHudComponentActive(iIndex))
	{
#if __DEV
		sfDebugf3("Script %s sent component %s (%d) to front", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), iIndex - SCRIPT_HUD_COMPONENTS_START);
#endif // __DEV

		if (CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "SET_COMPONENT_TO_FRONT"))
		{
			CScaleformMgr::AddParamInt(iIndex);
			CScaleformMgr::EndMethod();
		}

		return true;
	}
	else
	{
#if __DEV
		scriptAssertf(0, "Script %s sent component %s (%d) to front but its not loaded!", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), iIndex - SCRIPT_HUD_COMPONENTS_START);
#endif // __DEV
	}

	return false;
}



//
// name:		CommandSetScaleformScriptHudMovieToBack
// description: sends the script hud component to the back
bool CommandSetScaleformScriptHudMovieToBack(s32 iIndex)
{
	iIndex += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	if (CNewHud::IsHudComponentActive(iIndex))
	{
#if __DEV
		sfDebugf3("Script %s sent component %s (%d) to back", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), iIndex - SCRIPT_HUD_COMPONENTS_START);
#endif // __DEV

		if (CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "SET_COMPONENT_TO_BACK"))
		{
			CScaleformMgr::AddParamInt(iIndex);
			CScaleformMgr::EndMethod();
		}

		return true;
	}
	else
	{
#if __DEV
		scriptAssertf(0, "Script %s sent component %s (%d) to back but its not loaded!", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), iIndex - SCRIPT_HUD_COMPONENTS_START);
#endif // __DEV
	}

	return false;
}



//
// name:		CommandSetScaleformScriptHudMovieRelativeToComponent
// description: sends the script hud component to a position relative to another hud or script hud component
bool CommandSetScaleformScriptHudMovieRelativeToHudComponent(s32 iIndex, s32 iTargetId, bool bInFront)
{
	iIndex += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	if (CNewHud::IsHudComponentActive(iIndex))
	{
#if __DEV
		if (bInFront)
			sfDebugf3("Script %s sent component %s in front of %s", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), CNewHud::GetHudComponentFileName(iTargetId));
		else
			sfDebugf3("Script %s sent component %s behind %s", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), CNewHud::GetHudComponentFileName(iTargetId));
#endif // __DEV

		if (CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "SET_COMPONENT_RELATIVE_TO_TARGET"))
		{
			CScaleformMgr::AddParamBool(bInFront);
			CScaleformMgr::AddParamInt(iIndex);
			CScaleformMgr::AddParamInt(iTargetId);
			CScaleformMgr::EndMethod();
		}

		return true;
	}
	else
	{
#if __DEV
		if (bInFront)
			scriptAssertf(0, "Script %s moved component %s behind %s they both are not loaded!", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), CNewHud::GetHudComponentFileName(iTargetId));
		else
			scriptAssertf(0, "Script %s moved component %s behind %s they both are not loaded!", CTheScripts::GetCurrentScriptName(), CNewHud::GetHudComponentFileName(iIndex), CNewHud::GetHudComponentFileName(iTargetId));
#endif // __DEV
	}

	return false;
}

bool CommandPassKeyboardInputToScaleform(s32 iIndex)
{
	if(SCRIPT_VERIFY(iIndex > 0 && iIndex <= NUM_SCRIPT_SCALEFORM_MOVIES, "BEGIN_SCALEFORM_MOVIE_METHOD - Invalid Scaleform movie id") NOTFINAL_ONLY(&& CControlMgr::GetKeyboard().GetKeyboardMode() == KEYBOARD_MODE_GAME) )
	{
		{
#if __DEV
			sfDebugf3("Script %s passed keyboard input to Scaleform.", CTheScripts::GetCurrentScriptName());
#endif // __DEV

#if RSG_GEN9
			if (!CScriptHud::ScriptScaleformMovie[iIndex-1].bRenderRequestedOnce)
#else
			if (!CScriptHud::ScriptScaleformMovie[iIndex-1].bRenderedOnce)
#endif
			{
				CScriptHud::ScriptScaleformMovie[iIndex-1].bInvokedBeforeRender = true;  // flag as we are invoking before it has been rendered for the 1st time (so this flags it to update before the 1st render)
			}

			const char16 *pInputBuffer = ioKeyboard::GetInputText();

			// There is no PC text.
			if(pInputBuffer == NULL || pInputBuffer[0] == '\0')
			{
				return false;
			}

			char tmpBuffer[2]; // char + null.

			for(u32 i = 0; i < ioKeyboard::TEXT_BUFFER_LENGTH && pInputBuffer[i] != '\0'; ++i)
			{

				if (SCRIPT_VERIFY(CScaleformMgr::BeginMethod(CScriptHud::ScriptScaleformMovie[iIndex-1].iId, SF_BASE_CLASS_SCRIPT, "SET_PC_KEY"), "PASS_KEYBOARD_INPUT_TO_SCALEFORM failed to pass text to scaleform!"))
				{
					// add each character.
					if(pInputBuffer[i] == '\b')
					{
						CScaleformMgr::AddParamString("BACKSPACE");
					}
					else if(pInputBuffer[i] == '\n' || pInputBuffer[i] == '\r')
					{
						CScaleformMgr::AddParamString("ENTER");
					}
					else if (pInputBuffer[i] >= 128)
					{
						scriptWarningf("Only ascii is supported for now, got character %d", pInputBuffer[i]);
					}
					else if (pInputBuffer[i]) 
					{
						tmpBuffer[0] = (char)pInputBuffer[i];
						tmpBuffer[1] = '\0'; // just in case.

						CScaleformMgr::AddParamString(tmpBuffer);
					}
					CScaleformMgr::EndMethod();
				}
			}
		}

		// If we reach here then all text has been passed to scaleform so we indicate that text was added.
		return true;
	}

	return false;
}


//
// name:		CommandSetScaleformScriptHudMovieRelativeToComponent
// description: applies the script hud offset and runs the hud component command above
bool CommandSetScaleformScriptHudMovieRelativeToComponent(s32 iIndex, s32 iTargetId, bool bInFront)
{
	iTargetId += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	return (CommandSetScaleformScriptHudMovieRelativeToHudComponent(iIndex, iTargetId, bInFront));
}




// name:		CommandRequestScaleformScriptHudMovie
// description: requests that the game streams a script hud scaleform movie in
void CommandRequestScaleformScriptHudMovie(s32 iComponentId)
{
	iComponentId += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	CNewHud::ActivateHudComponent(false, iComponentId, false, true);
}


//
// name:		CommandHasScaleformScriptHudMovieLoaded
// description: returns whether the passed scaleform script hud movie has loaded in yet
bool CommandHasScaleformScriptHudMovieLoaded(s32 iComponentId)
{
	iComponentId += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	return (CNewHud::IsHudComponentActive(iComponentId));
}



//
// name:		CommandRemoveScaleformScriptHudMovie
// description: removes scaleform script hud movie
void CommandRemoveScaleformScriptHudMovie(s32 iComponentId)
{
	iComponentId += SCRIPT_HUD_COMPONENTS_START;  // offset from script enum to code hud enum

	CNewHud::RemoveHudComponentFromActionScript(iComponentId);
}



//
// name:		CommandSetTVChannel
// description: sets the TV to the selected channel
void CommandSetTVChannel(s32 iChannel)
{
	CTVPlaylistManager::SetTVChannel(iChannel);	// See TVPlaylistManager for channel defines (especially TV_CHANNEL_ID_NONE)
}

//
// name:		CommandGetTVChannel
// description: Returns the TV channel currently playing on the TV
s32 CommandGetTVChannel()
{
	return CTVPlaylistManager::GetTVChannel();
}

//
// name:		CommandSetTVVolume
// description:
void CommandSetTVVolume(float volume)
{
	CTVPlaylistManager::SetTVVolume(volume);
}

float CommandGetTVVolume()
{
	return CTVPlaylistManager::GetTVVolume();
}
//
// name:		CommandGetPlayingTVChannelVideoHandle
// description: Gets a handle to the video currently playing on the TV
u32 CommandGetPlayingTVChannelVideoHandle()
{
	return CTVPlaylistManager::GetCurrentlyPlayingHandle();
}

void CommandDrawTVChannel(float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A)
{
	u32	handle = CTVPlaylistManager::GetCurrentlyPlayingHandle();
	if( handle != INVALID_MOVIE_HANDLE )
	{
		// dont call the command directly here as the params are not the same as the enum (fixes 1172100)
		DrawSpriteTex(NULL, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, handle, SCRIPT_GFX_SPRITE_AUTO_INTERFACE, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), true, false, false);

		CTVPlaylistManager::CacheCurrentlyPlayingColour(R, G, B, A);

		REPLAY_ONLY(ReplayMovieControllerNew::OnDrawTVChannel(CentreX, CentreY, Width, Height, Rotation, R, G, B, A, handle, CScriptHud::scriptTextRenderID));
	}
}

void CommandAssignPlaylistToChannel(s32 iChannel, const char *playlistName, bool startFromNow)
{
	CTVPlaylistManager::SetTVChannelPlaylist(iChannel, playlistName, startFromNow);
}

void CommandAssignPlaylistToChannelAtHour(s32 iChannel, const char *playlistName, int hour)
{
	scriptAssertf(hour >= 0 && hour <= 23, "SET_TV_CHANNEL_PLAYLIST_AT_HOUR() : Hour %d is out of range!", hour);
	CTVPlaylistManager::SetTVChannelPlaylistFromHour(iChannel, playlistName, hour);
}


void CommandClearPlaylistOnChannel(s32 iChannel)
{
	CTVPlaylistManager::ClearTVChannelPlaylist(iChannel);
}

bool CommandIsPlaylistOnChannel(s32 iChannel, int nameHash)
{
	return CTVPlaylistManager::IsPlaylistOnChannel(iChannel, (u32)nameHash);
}

bool CommandIsTVShowCurrentlyPlaying( int nameHash )
{
	return CTVPlaylistManager::IsTVShowCurrentlyPlaying((u32)nameHash);
}

void CommandForceBinkKeyframeSwitch( bool bOnOff )
{
	CTVPlaylistManager::SetForceKeyframeWait(bOnOff);
}

void CommandSetPlayerWatchingTVThisFrame( int playerIndex )
{
	if(playerIndex != INVALID_PLAYER_INDEX)
	{
		CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(playerIndex, 0);
		if( pPed->IsLocalPlayer() )
		{
			CTVPlaylistManager::SetWatchingTVThisFrame();
		}
	}
}

int GetCurrentTVClipNamehash(void)
{
	return(CTVPlaylistManager::GetCurrentShowNameHash());
}

// --- Bink Movie Subtitles ------------------------------------------------------------------------------

void CommandEnableMovieSubtitles(bool bOnOff)
{
	CMovieMgr::EnableSubtitles(bOnOff);
}

// --- UI 3D Draw commands -------------------------------------------------------------------------------

bool CommandUI3DSceneIsAvailable()
{
	const bool bAvailable = UI3DDRAWMANAGER.IsAvailable();
	return bAvailable;
}

bool CommandUI3DScenePushPreset(const char* pPresetName)
{
	const bool bOk = UI3DDRAWMANAGER.PushCurrentPreset(atHashString(pPresetName));
	return bOk;
}

bool CommandUI3DSceneAssignPedToSlot(const char* pPresetName, int pedIndex, int slot, const scrVector & scrPosOffset)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	Vector3 posOffset = Vector3(scrPosOffset);

	if (pPed == NULL)
	{
		return false;
	}

	// Ensure that UI peds are not timesliced each frame
	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);

	const bool bOk = UI3DDRAWMANAGER.AssignPedToSlot(atHashString(pPresetName), (u32)slot, pPed, posOffset, 0.0f);
	return bOk;
}

bool CommandUI3DSceneAssignPedToSlotWithRotOffset(const char* pPresetName, int pedIndex, int slot, const scrVector & scrPosOffset, const scrVector & scrRotOffset)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(pedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	Vector3 posOffset = Vector3(scrPosOffset);
	Vector3 rotOffset = Vector3(scrRotOffset);

	if (pPed == NULL)
	{
		return false;
	}

	// Ensure that UI peds are not timesliced each frame
	pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);

	const bool bOk = UI3DDRAWMANAGER.AssignPedToSlot(atHashString(pPresetName), (u32)slot, pPed, posOffset, rotOffset, 0.0f);
	return bOk;
}

void CommandUI3DSceneAssignGlobalLightIntensityToSlot(const char* pPresetName, int slot, float intensity)
{
	UI3DDRAWMANAGER.AssignGlobalLightIntensityToSlot(atHashString(pPresetName), (u32)slot, intensity);
}

void CommandUI3DSceneClearPatchedData()
{
	UI3DDRAWMANAGER.ClearPatchedData();
}

void CommandUI3DSceneMakeCurrentPresetPersistent(bool bEnable)
{
	UI3DDRAWMANAGER.MakeCurrentPresetPersistent(bEnable);
}

void CommandTerrainGridActivate(bool bEnable)
{
	GOLFGREENGRID.Activate(bEnable);
}

void CommandTerrainGridSetParams(const scrVector & scrPos, const scrVector & scrDir, float boxWidth, float boxHeight, float boxDepth, float gridRes, float colMult, float minHeight, float maxHeight)
{
	Vector3 pos		= Vector3(scrPos);
	Vector3 dir		= Vector3(scrDir);
	Vector3 dims	= Vector3(boxWidth, boxHeight, boxDepth);
	Vector4 params	= Vector4(gridRes, colMult, minHeight, maxHeight);

	GOLFGREENGRID.SetBoxParams(dims, pos, dir, params);
}


void CommandTerrainGridSetColours(int lowR, int lowG, int lowB, int lowA, int midR, int midG, int midB, int midA, int highR, int highG, int highB, int highA)
{
	Color32 lowHeight(lowR, lowG, lowB, lowA);
	Color32 midHeight(midR, midG, midB, midA);
	Color32 highHeight(highR, highG, highB, highA);

	GOLFGREENGRID.SetColours(lowHeight, midHeight, highHeight);
}

void CommandPlayAnimatedPostFx(const char* pFxName, int duration, bool playLooped)
{
	ANIMPOSTFXMGR.Start(atHashString(pFxName), duration, playLooped, false, false, 0, AnimPostFXManager::kScript);
}

void CommandStopAnimatedPostFx(const char* pFxName)
{
	ANIMPOSTFXMGR.Stop(atHashString(pFxName), AnimPostFXManager::kScript);
}

float CommandGetAnimatedPostFxCurrentTime(const char* pFxName)
{
#if RSG_GEN9
	return ANIMPOSTFXMGR.GetCurrentFXTime(atHashString(pFxName));
#else
	return ANIMPOSTFXMGR.GetCurrentTime(atHashString(pFxName));
#endif
}

bool CommandIsAnimatedPostFxRunning(const char* pFxName)
{
	return ANIMPOSTFXMGR.IsRunning(atHashString(pFxName));
}

void CommandStartAnimatedPostFxCrossfade(const char* pFxNameIn, const char* pFxNameOut, int duration)
{
	ANIMPOSTFXMGR.StartCrossfade(atHashString(pFxNameIn), atHashString(pFxNameOut), duration, AnimPostFXManager::kScript);
}

void CommandStopAnimatedPostFxCrossfade()
{
	ANIMPOSTFXMGR.StopCrossfade(AnimPostFXManager::kScript);
}

void CommandStopAllAnimatedPostFx()
{
	ANIMPOSTFXMGR.StopAll(AnimPostFXManager::kScript);
}

void CommandStopAnimatedPostFxAndCancelStartRequest(const char* pFxName)
{
	ANIMPOSTFXMGR.Stop(atHashString(pFxName), AnimPostFXManager::kScript);
	ANIMPOSTFXMGR.CancelStartRequest(atHashString(pFxName), AnimPostFXManager::kScript);
}

bool CommandAnimatedPostFxAddListener(const char* pFxName)
{
	return ANIMPOSTFXMGR.AddListener(atHashString(pFxName));
}

void CommandAnimatedPostFxRemoveListener(const char* pFxName)
{
	ANIMPOSTFXMGR.RemoveListener(atHashString(pFxName));
}

bool CommandAnimatedPostFxHasEventTriggered(const char* pFxName, int eventType, bool bPeekOnly, int& isRegistered)
{
	bool bIsRegistered;
	bool bOk = ANIMPOSTFXMGR.HasEventTriggered(atHashString(pFxName), (AnimPostFXEventType)eventType, bPeekOnly, bIsRegistered);
	isRegistered = static_cast<bool>(bIsRegistered);
	return bOk;
}

void CommandDisableOcclusionThisFrame()
{
	COcclusion::ScriptDisableOcclusionThisFrame();

#if GTA_REPLAY
	if(CReplayMgr::IsRecording())
	{
		CPacketDisabledThisFrame::RegisterOcclusionThisFrame();
	}
#endif
}

void CommandSetArtLightsState(bool enable)
{
	CRenderer::SetDisableArtificialLights(enable);
}

void CommandSetArtVehLightsState(bool enable)
{
	CRenderer::SetDisableArtificialVehLights(enable);
}

void CommandDisableHDTexThisFrame(void)
{
	CTexLod::DisableAmbientRequests();
}

bool CommandPhonePhotoEditorToggle(bool bEnable)
{
	// Check calls to enable or disable make sense...
	bool bAlreadyActive = PHONEPHOTOEDITOR.IsActive();

	if (bEnable && Verifyf(bAlreadyActive == false, "[PHONE_PHOTO_EDIT] trying to enable, but it's already active"))
	{
		PHONEPHOTOEDITOR.Enable();
	}
	else if (bEnable == false && Verifyf(bAlreadyActive, "[PHONE_PHOTO_EDIT] trying to disable, but it's already disabled"))
	{
		PHONEPHOTOEDITOR.Disable();
	}
	else
	{
		return false;
	}

	return true;
}

bool CommandPhonePhotoEditorIsActive()
{
	return PHONEPHOTOEDITOR.IsActive();
}

bool CommandPhonePhotoEditorSetFrameTXD(const char* pFrameTXD, bool bJustDisableCurrentFrame)
{
	// Just disable whatever frame is active now
	if (bJustDisableCurrentFrame)
	{
		PHONEPHOTOEDITOR.SetBorderTexture(NULL, true);
		return true;
	}

	// Check for valid string...
	if (pFrameTXD == NULL)
	{
		Assertf(0, "[PHONE_PHOTO_EDIT] NULL TXD string");
		return false;
	}

	// Check it actually exists...
	atHashString txdHash = atHashString(pFrameTXD);
	strLocalIndex txdIdx = g_TxdStore.FindSlotFromHashKey(txdHash.GetHash());

	if (Verifyf(txdIdx != -1, "[PHONE_PHOTO_EDIT] Couldn't find TXD \"%s\"", pFrameTXD))
	{
		// Check it's loaded
		if (Verifyf(g_TxdStore.HasObjectLoaded(txdIdx), "[PHONE_PHOTO_EDIT] TXD \"%s\" is not loaded yet", pFrameTXD))
		{
			PHONEPHOTOEDITOR.SetBorderTexture(pFrameTXD, false);
			return true;
		}
	}

	return false;
}

bool CommandPhonePhotoEditorSetText(const char* pTopText, const char* pBottomText, 	
									const scrVector& scrTopText_PosXY_SizeZ, const scrVector& scrBottomText_PosXY_SizeZ,
									int topTextR, int topTextG, int topTextB, int topTextA,
									int bottomTextR, int bottomTextG, int bottomTextB, int bottomTextA)
{


	if (pTopText == NULL && pBottomText == NULL)
		return false;

	Vector3 topTextPosSize = Vector3(scrTopText_PosXY_SizeZ);
	Vector3 bottomTextPosSize = Vector3(scrBottomText_PosXY_SizeZ);

	Color32 topTextCol(topTextR, topTextG, topTextB, topTextA);
	Color32 bottomTextCol(bottomTextR, bottomTextG, bottomTextB, bottomTextA);

	Vector4 textPos = Vector4(topTextPosSize.x, topTextPosSize.y, bottomTextPosSize.x, bottomTextPosSize.y);

	PHONEPHOTOEDITOR.SetText(pTopText, pBottomText, textPos, topTextPosSize.z, bottomTextPosSize.z, 4, 4, topTextCol, bottomTextCol);

	return true;
}

void CommandRegisterBulletImpact(const scrVector& scrWeaponWorldPos, float intensity)
{
	Vector3 weaponWorldPos = Vector3(scrWeaponWorldPos);
	PostFX::RegisterBulletImpact(weaponWorldPos, intensity);
}

void CommandForceBulletImpactOverlaysAfterHud(bool bEnable)
{
	PostFX::ForceBulletImpactOverlaysAfterHud(bEnable);
}

#if RSG_GEN9
void CommandSetSunRollAngle(float angle)
{
	g_timeCycle.SetSunRollAngle(DtoR * angle);

#if TC_DEBUG
	// Update RAG-controlled value updated every frame
	CTimeCycleDebug::SetSunRollDegreeAngle(angle);
#endif
}

void CommandSetSunYawAngle(float angle)
{
	g_timeCycle.SetSunYawAngle(DtoR * angle);

#if TC_DEBUG
	// Update RAG-controlled value updated every frame
	CTimeCycleDebug::SetSunYawDegreeAngle(angle);
#endif
}
#endif //RSG_GEN9

//

void SetupScriptCommands()
{
	//===========================================================================
	// OLD DEPRECATED DEBUG DRAW COMMANDS!!!!
	//===========================================================================
	SCR_REGISTER_SECURE(SET_DEBUG_LINES_AND_SPHERES_DRAWING_ACTIVE,0xf8ac79ed8746b0c2, CommandSetDrawingDebugLinesAndSpheresActive);
	//===========================================================================
	//===========================================================================

	//===========================================================================
	// NEW SHINY DEBUG DRAW COMMANDS!!!!
	//===========================================================================
	SCR_REGISTER_SECURE(DRAW_DEBUG_LINE,0x116faa21fee61c2c,CommandDebugDraw_Line);
	SCR_REGISTER_SECURE(DRAW_DEBUG_LINE_WITH_TWO_COLOURS,0x8a66cde1d3e54c3b,CommandDebugDraw_Line2Colour);
	SCR_REGISTER_UNUSED(DRAW_DEBUG_POLY,0x696d54b360c0af17,CommandDebugDraw_Poly);
	SCR_REGISTER_UNUSED(DRAW_DEBUG_POLY_WITH_THREE_COLOURS,0xc3578f9d9cc71f45,CommandDebugDraw_Poly3Colour);
	SCR_REGISTER_SECURE(DRAW_DEBUG_SPHERE,0xb00fc865b64b72b8,CommandDebugDraw_Sphere);
	SCR_REGISTER_SECURE(DRAW_DEBUG_BOX,0x992c462a62c36aac,CommandDebugDraw_BoxAxisAligned);
	SCR_REGISTER_SECURE(DRAW_DEBUG_CROSS,0x7f88c1f7a2bb84a8,CommandDebugDraw_Cross);
	SCR_REGISTER_SECURE(DRAW_DEBUG_TEXT,0x906f6137d122d88c,CommandDebugDraw_Text);
	SCR_REGISTER_UNUSED(DRAW_DEBUG_TEXT_WITH_OFFSET,0x232554e637753c44,CommandDebugDraw_TextWithOffset);
	SCR_REGISTER_UNUSED(DEBUGDRAW_GETSCREENSPACETEXTHEIGHT, 0x5dd864fd,CommandDebugDraw_GetScreenSpaceTextHeight);
	SCR_REGISTER_UNUSED(DRAW_DEBUG_LINE_2D,0xc1869dbe37027246,CommandDebugDraw_Line2D);
	SCR_REGISTER_UNUSED(DRAW_DEBUG_LINE_2D_WITH_TWO_COLOURS,0xa8aa43959e83267b,CommandDebugDraw_Line2D2Colour);
	SCR_REGISTER_UNUSED(DRAW_DEBUG_POLY_2D,0x539e8ec586ec1110,CommandDebugDraw_Poly2D);
	SCR_REGISTER_UNUSED(DRAW_DEBUG_POLY_2D_WITH_THREE_COLOURS,0xbed48ff0654aaeac,CommandDebugDraw_Poly2D3Colour);
	SCR_REGISTER_SECURE(DRAW_DEBUG_TEXT_2D,0x963a57df8530029d,CommandDebugDraw_Text2D);

	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_LINE,0x7aecb233286db148,CommandVectorMapDraw_Line);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_LINE_THICK,0x23544bf2543331db,CommandVectorMapDraw_LineThick);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_CIRCLE,0x950f3c7d321f2282,CommandVectorMapDraw_Circle);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_POLY,0x1e412a1301c960b8,CommandVectorMapDraw_Poly);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_RECTANGLE,0x86b887a0a6dd3d7e,CommandVectorMapDraw_FilledRect);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_WEDGE,0xe9dd48eec6654d98,CommandVectorMapDraw_Wedge);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_MARKER,0xf4f1158260c351ee,CommandVectorMapDraw_Marker);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_VEHICLE,0x9e9e3a94754bc189,CommandVectorMapDraw_Vehicle);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_PED,0x3be3177c6570663f,CommandVectorMapDraw_Ped);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_LOCAL_PLAYER_CAM,0x7c76172989638b68,CommandVectorMapDraw_LocalPlayerCamera);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_EVENT_RIPPLE,0xdb579ba66805ac59,CommandVectorMapDraw_EventRipple);
	SCR_REGISTER_UNUSED(DRAW_VECTOR_MAP_TEXT,0xf9c03dd47c3bada8,CommandVectorMapDraw_Text);
	//===========================================================================
	//===========================================================================

	SCR_REGISTER_SECURE(DRAW_LINE,0x0f3d52721510c729,CommandDraw_Line);
	SCR_REGISTER_SECURE(DRAW_POLY,0xf6dbd92bef1dc4ad,CommandDraw_Poly);
	SCR_REGISTER_SECURE(DRAW_TEXTURED_POLY,0x4e02aff84ca4348f,CommandDraw_TexturedPoly);
	SCR_REGISTER_UNUSED(DRAW_POLY_WITH_THREE_COLOURS,0x46b61194a9253a03,CommandDraw_Poly3Colours);
	SCR_REGISTER_SECURE(DRAW_TEXTURED_POLY_WITH_THREE_COLOURS,0x82dc59c0f3a73991,CommandDraw_TexturedPoly3Colours);
	SCR_REGISTER_SECURE(DRAW_BOX,0xfdbc3b555b9b5928,CommandDraw_Box);
	SCR_REGISTER_SECURE(SET_BACKFACECULLING,0xbe1b671f8bb897b3,CommandSet_BackFaceCulling);
	SCR_REGISTER_SECURE(SET_DEPTHWRITING,0xced4eae315bc88fe,CommandSet_DepthWriting);

	// --- Mission Creator photo commands -------------------------------------------------------------------------------

	SCR_REGISTER_SECURE(BEGIN_TAKE_MISSION_CREATOR_PHOTO,0xe8f09bbd8570fefe, CommandBeginTakeMissionCreatorPhoto);
	SCR_REGISTER_SECURE(GET_STATUS_OF_TAKE_MISSION_CREATOR_PHOTO,0x3e6af29dc72ef814, CommandGetStatusOfTakeMissionCreatorPhoto);
	SCR_REGISTER_SECURE(FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO,0x5a55189d6e78d557, CommandFreeMemoryForMissionCreatorPhoto);

	SCR_REGISTER_UNUSED(SAVE_MISSION_CREATOR_PHOTO,0x6e5a895bcf63fe7b, CommandSaveMissionCreatorPhoto);
	SCR_REGISTER_UNUSED(GET_STATUS_OF_SAVE_MISSION_CREATOR_PHOTO,0x2057f00e5903a713, CommandGetStatusOfSaveMissionCreatorPhoto);

	SCR_REGISTER_SECURE(LOAD_MISSION_CREATOR_PHOTO,0x13741f716cad14a7, CommandLoadMissionCreatorPhoto);
	SCR_REGISTER_SECURE(GET_STATUS_OF_LOAD_MISSION_CREATOR_PHOTO,0x2afeb54f1e26c2ef, CommandGetStatusOfLoadMissionCreatorPhoto);

	SCR_REGISTER_SECURE(BEGIN_CREATE_MISSION_CREATOR_PHOTO_PREVIEW,0xae8a42aa357d2416, CommandBeginCreateMissionCreatorPhotoPreview);
	SCR_REGISTER_SECURE(GET_STATUS_OF_CREATE_MISSION_CREATOR_PHOTO_PREVIEW,0x34d987c578caba90, CommandGetStatusOfCreateMissionCreatorPhotoPreview);
	SCR_REGISTER_SECURE(FREE_MEMORY_FOR_MISSION_CREATOR_PHOTO_PREVIEW,0x4bc56e6c59e88bee, CommandFreeMemoryForMissionCreatorPhotoPreview);

	// --- Photo gallery commands -------------------------------------------------------------------------------

	SCR_REGISTER_SECURE(BEGIN_TAKE_HIGH_QUALITY_PHOTO,0x1abcce88aeaa76d9, CommandBeginTakeHighQualityPhoto);
	SCR_REGISTER_SECURE(GET_STATUS_OF_TAKE_HIGH_QUALITY_PHOTO,0x6fb6030525709933, CommandGetStatusOfTakeHighQualityPhoto);
	SCR_REGISTER_SECURE(FREE_MEMORY_FOR_HIGH_QUALITY_PHOTO,0xee35b1932ee60b67, CommandFreeMemoryForHighQualityPhoto);

	SCR_REGISTER_SECURE(SET_TAKEN_PHOTO_IS_MUGSHOT,0xf61ced31c635227b, CommandSetTakenPhotoIsAMugshot);

	SCR_REGISTER_SECURE(SET_ARENA_THEME_AND_VARIATION_FOR_TAKEN_PHOTO,0x98b584f08145d31e, CommandSetArenaThemeAndVariationForTakenPhoto);

	SCR_REGISTER_SECURE(SET_ON_ISLAND_X_FOR_TAKEN_PHOTO,0x850af55dd4cd20ec, CommandSetOnIslandXForTakenPhoto);

	SCR_REGISTER_SECURE(SAVE_HIGH_QUALITY_PHOTO,0x4a486319fee2fc38, CommandSaveHighQualityPhoto);
	SCR_REGISTER_SECURE(GET_STATUS_OF_SAVE_HIGH_QUALITY_PHOTO,0x8e8fe906d8bac5f7, CommandGetStatusOfSaveHighQualityPhoto);

	SCR_REGISTER_UNUSED(UPLOAD_MUGSHOT_PHOTO,0x159f9df48982f08a, CommandUploadMugshotPhoto);
	SCR_REGISTER_UNUSED(GET_STATUS_OF_UPLOAD_MUGSHOT_PHOTO,0xca2ba27b5c4b31c9, CommandGetStatusOfUploadMugshotPhoto);

	SCR_REGISTER_SECURE(BEGIN_CREATE_LOW_QUALITY_COPY_OF_PHOTO,0x91e8216f9ce58f7e, CommandBeginCreateLowQualityCopyOfPhoto);
	SCR_REGISTER_SECURE(GET_STATUS_OF_CREATE_LOW_QUALITY_COPY_OF_PHOTO,0xefac41d453b8fa2b, CommandGetStatusOfCreateLowQualityCopyOfPhoto);
	SCR_REGISTER_SECURE(FREE_MEMORY_FOR_LOW_QUALITY_PHOTO,0x2cf943308fefb496, CommandFreeMemoryForLowQualityPhoto);
	SCR_REGISTER_SECURE(DRAW_LOW_QUALITY_PHOTO_TO_PHONE,0x3de812d5be11533e, CommandDrawLowQualityPhotoToPhone);

	SCR_REGISTER_SECURE(GET_MAXIMUM_NUMBER_OF_PHOTOS,0x4f09a12ba1b81917, CommandGetMaximumNumberOfPhotos);
	SCR_REGISTER_SECURE(GET_MAXIMUM_NUMBER_OF_CLOUD_PHOTOS,0x468404757b8a82c2, CommandGetMaximumNumberOfCloudPhotos);
	SCR_REGISTER_SECURE(GET_CURRENT_NUMBER_OF_CLOUD_PHOTOS,0x08b0ce958e38ac0d, CommandGetCurrentNumberOfCloudPhotos);
	SCR_REGISTER_SECURE(QUEUE_OPERATION_TO_CREATE_SORTED_LIST_OF_PHOTOS,0x04b354be7e7b0632, CommandQueueOperationToCreateSortedListOfPhotos);
	SCR_REGISTER_SECURE(GET_STATUS_OF_SORTED_LIST_OPERATION,0x78bb5808602d08aa, CommandGetStatusOfSortedListOperation);
	SCR_REGISTER_SECURE(CLEAR_STATUS_OF_SORTED_LIST_OPERATION,0xb74730632fb72ae5, CommandClearStatusOfSortedListOperation);

	SCR_REGISTER_SECURE(DOES_THIS_PHOTO_SLOT_CONTAIN_A_VALID_PHOTO,0x55518ee41f8109f8, CommandDoesThisPhotoSlotContainAValidPhoto);
	SCR_REGISTER_UNUSED(GET_SLOT_FOR_SAVING_A_PHOTO_TO,0x9d3ef24ae103677c, CommandGetSlotForSavingAPhotoTo);

	SCR_REGISTER_SECURE(LOAD_HIGH_QUALITY_PHOTO,0xce97a2ca705c4bf0, CommandLoadHighQualityPhoto);
	SCR_REGISTER_SECURE(GET_LOAD_HIGH_QUALITY_PHOTO_STATUS,0xa2702a84c74e8980, CommandGetLoadHighQualityPhotoStatus);

	// --- Light commands -------------------------------------------------------------------------------
	SCR_REGISTER_UNUSED(SET_HIGH_QUALITY_LIGHTING_TECHNIQUES,0x6244c19624fecd07, CommandSetHighQualityLightingTechniques);
	SCR_REGISTER_SECURE(DRAW_LIGHT_WITH_RANGEEX,0xb0ebad32769ef0e8, CommandDrawLightWithRangeEx);
	SCR_REGISTER_SECURE(DRAW_LIGHT_WITH_RANGE,0x634e773276bb1aec, CommandDrawLightWithRange);
	SCR_REGISTER_SECURE(DRAW_SPOT_LIGHT,0xd86164ee7a432643, CommandDrawSpotLight);
	SCR_REGISTER_SECURE(DRAW_SHADOWED_SPOT_LIGHT,0xe2dc7c9e15411e37, CommandDrawShadowedSpotLight);
	SCR_REGISTER_SECURE(FADE_UP_PED_LIGHT,0x34b11f2042ca7e3c, CommandFadeUpPedLight);
	SCR_REGISTER_SECURE(UPDATE_LIGHTS_ON_ENTITY,0xc9d2355daf3fe0c3, CommandUpdateLightsOnEntity);
	SCR_REGISTER_SECURE(SET_LIGHT_OVERRIDE_MAX_INTENSITY_SCALE,0x0a9c9cc24cd5ff43, CommandSetLightOverrideMaxIntensityScale);
	SCR_REGISTER_SECURE(GET_LIGHT_OVERRIDE_MAX_INTENSITY_SCALE,0x798d62da8796cbdf, CommandGetLightOverrideMaxIntensityScale);

	// --- Marker commands ------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(DRAW_MARKER,0x578727d52eacada6, CommandDrawMarker);
	SCR_REGISTER_SECURE(DRAW_MARKER_EX,0x3094777b6f3c614f, CommandDrawMarkerEx);
	SCR_REGISTER_SECURE(DRAW_MARKER_SPHERE,0xb5d87e1c6e14871d, CommandDrawMarkerSphere);
	SCR_REGISTER_UNUSED(GET_CLOSEST_PLANE_MARKER_ORIENTATION,0x642905aea0bf6645, CommandGetClosestPlaneMarkerOrientation);

	// --- Checkpoint commands --------------------------------------------------------------------------
	SCR_REGISTER_SECURE(CREATE_CHECKPOINT,0x89ee105c4f81a182, CommandCreateCheckpoint);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_INSIDE_CYLINDER_HEIGHT_SCALE,0xc61e1a088d32cc8b, CommandSetCheckpointInsideCylinderHeightScale);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_INSIDE_CYLINDER_SCALE,0xd6c441611fa6b9e2, CommandSetCheckpointInsideCylinderScale);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_CYLINDER_HEIGHT,0x7ff96acf7a4cd4ed, CommandSetCheckpointCylinderHeight);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_RGBA,0x64983ddd3d05b1b7, CommandSetCheckpointRGBA);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_RGBA2,0x5aa4072aeb6e6a0e, CommandSetCheckpointRGBA2);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_CLIPPLANE_WITH_POS_NORM,0x7a9381199559fbcb, CommandSetCheckpointClipPlane_PosNorm);
	SCR_REGISTER_UNUSED(SET_CHECKPOINT_CLIPPLANE_WITH_PLANE_EQ,0xd3b348c7c5f51f36, CommandSetCheckpointClipPlane_PlaneEq);
	SCR_REGISTER_UNUSED(GET_CHECKPOINT_CLIPPLANE_PLANE_EQ,0x4bc0c0cac06bcb20, CommandGetCheckpointClipPlane_PlaneEq);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_FORCE_OLD_ARROW_POINTING,0xe8ca1ce36f1cc033, CommandSetCheckpointForceOldArrowPointing);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_DECAL_ROT_ALIGNED_TO_CAMERA_ROT,0x17c03d186cd6f5fb, CommandSetCheckpointDecalRotAlignedToCamRot);
	SCR_REGISTER_UNUSED(SET_CHECKPOINT_PREVENT_RING_FACING_CAM,0x74a240b42703b6cc, CommandSetCheckpointPreventRingFacingCam);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_FORCE_DIRECTION,0x8814a1cc123e8228, CommandSetCheckpointForceDirection);
	SCR_REGISTER_SECURE(SET_CHECKPOINT_DIRECTION,0x1891f5294778a5ad, CommandSetCheckpointDirection);
	SCR_REGISTER_SECURE(DELETE_CHECKPOINT,0xb9acd8a27bcdc3e2, CommandDeleteCheckpoint);

	// --- In Game UI commands --------------------------------------------------------------------------
	SCR_REGISTER_SECURE(DONT_RENDER_IN_GAME_UI,0x3b4b2c4b99c88de8, CommandDontRenderInGameUI);
	SCR_REGISTER_SECURE(FORCE_RENDER_IN_GAME_UI,0xa6e763ec100ae7bb, CommandForceRenderInGameUI);

	// --- Texture dictionary commands ------------------------------------------------------------------
	SCR_REGISTER_SECURE(REQUEST_STREAMED_TEXTURE_DICT,0xa9911c122b3210b5, CommandRequestStreamedTxd);
	SCR_REGISTER_SECURE(HAS_STREAMED_TEXTURE_DICT_LOADED,0x9d4afed2949f7082, HasStreamedTxdLoaded);
	SCR_REGISTER_SECURE(SET_STREAMED_TEXTURE_DICT_AS_NO_LONGER_NEEDED,0x861d35d42cfd2991, CommandMarkStreamedTxdAsNoLongerNeeded);

	// --- Sprite commands ------------------------------------------------------------------------------

	SCR_REGISTER_UNUSED(SET_MASK_ACTIVE,0x3491642dfe0d9545, CommandUseMask);
	SCR_REGISTER_UNUSED(SET_MASK_ATTRIBUTES,0xbf7c3ffdec8a17db, CommandSetMask);
	SCR_REGISTER_SECURE(DRAW_RECT,0xca4c0ad3caff651e, CommandDrawRect);
	SCR_REGISTER_UNUSED(DRAW_LINE_2D,0xb8da1d7223719e67, CommandDrawLine);
	SCR_REGISTER_SECURE(SET_SCRIPT_GFX_DRAW_BEHIND_PAUSEMENU,0xe53134abb42f336f, CommandSetScriptGfxDrawBehindPauseMenu);
	SCR_REGISTER_SECURE(SET_SCRIPT_GFX_DRAW_ORDER,0x2e04b7b46a3670e5, CommandSetScriptGfxDrawOrder);
	SCR_REGISTER_SECURE(SET_SCRIPT_GFX_ALIGN,0xa5aab00fa8c570a4, CommandSetScriptGfxAlign);
	SCR_REGISTER_SECURE(RESET_SCRIPT_GFX_ALIGN,0xb5a50a903b9ab61b, CommandResetScriptGfxAlign);
	SCR_REGISTER_SECURE(SET_SCRIPT_GFX_ALIGN_PARAMS,0x98f560bc13e5293f, CommandSetScriptGfxAlignParameters);
	SCR_REGISTER_SECURE(GET_SCRIPT_GFX_ALIGN_POSITION,0x9ccca5f1ebb26c03, CommandGetScriptGfxAlignPosition);
	SCR_REGISTER_SECURE(GET_SAFE_ZONE_SIZE,0x36e90400da266164, CommandGetSafeZoneSize);
	SCR_REGISTER_SECURE(DRAW_SPRITE,0xebf08da37d86cd05, CommandDrawSprite);
	SCR_REGISTER_SECURE(DRAW_SPRITE_ARX,0x7a86232b8d047c7b, CommandDrawSpriteARX);
	SCR_REGISTER_SECURE(DRAW_SPRITE_NAMED_RENDERTARGET,0x28159a26a47145a0, CommandDrawSpriteNamedRT);
	//SCR_REGISTER_UNUSED(DRAW_SPRITE_STEREO,0x4e81d8b838ffc1c6, CommandDrawSpriteStereo);
	SCR_REGISTER_UNUSED(DRAW_SPRITE_WITH_UV,0x1018b8fa2e04e31a, CommandDrawSpriteWithUV);
	SCR_REGISTER_SECURE(DRAW_SPRITE_ARX_WITH_UV,0xaf70d3d30d433da1, CommandDrawSpriteARXWithUV);
	SCR_REGISTER_UNUSED(DRAW_SPRITE_3D,0x08e3c368c184d0cf,	CommandDrawSprite3D);
	SCR_REGISTER_UNUSED(DRAW_SPRITE_3D_WITH_UV,0x9735fee3de1ef3f8,	CommandDrawSprite3DWithUV);

	SCR_REGISTER_SECURE(ADD_ENTITY_ICON,0x891f90fbec8d4e80, CommandAddEntityIcon);
	SCR_REGISTER_UNUSED(ADD_ENTITY_ICON_BY_VECTOR,0x66e6e8a87f7930f8, CommandAddEntityIconByVector);
	SCR_REGISTER_UNUSED(REMOVE_ENTITY_ICON,0x1566b7c398221a11, CommandRemoveEntityIcon);
	SCR_REGISTER_UNUSED(REMOVE_ENTITY_ICON_ID,0xe0cfb5e0e7895cd5, CommandRemoveEntityIconId);
	SCR_REGISTER_UNUSED(DOES_ENTITY_HAVE_ICON,0xeae67965bd9cdee4, CommandDoesEntityHaveIcon);
	SCR_REGISTER_UNUSED(DOES_ENTITY_HAVE_ICON_ID,0xb2a003c2e11a9e88, CommandDoesEntityHaveIconId);
	SCR_REGISTER_SECURE(SET_ENTITY_ICON_VISIBILITY,0x83dfb8c3b0b37aed, CommandSetEntityIconVisibility);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_ID_VISIBILITY,0x6df3413003ff3da7, CommandSetEntityIconIdVisibility);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_BG_VISIBILITY,0xc21625745809ca2b, CommandSetEntityIconBGVisibility);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_ID_BG_VISIBILITY,0x8db314f42db97e79, CommandSetEntityIconIdBGVisibility);
	SCR_REGISTER_UNUSED(SET_USING_ENTITY_ICON_CLAMP,0x77532268b7427c8a, CommandSetShouldEntityIconClampToScreen);
	SCR_REGISTER_UNUSED(SET_USING_ENTITY_ICON_ID_CLAMP,0x514e722d6e51faa5, CommandSetShouldEntityIconIdClampToScreen);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_RENDER_COUNT,0x2c1e933b5fb524f6, CommandSetEntityIconRenderCount);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_ID_RENDER_COUNT,0x5173f78cdf8e2862, CommandSetEntityIconIdRenderCount);
	SCR_REGISTER_SECURE(SET_ENTITY_ICON_COLOR,0x43b60611864df57c, CommandSetEntityIconColor);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_ID_COLOR,0x0356a169fcc09d6f, CommandSetEntityIconIdColor);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_BG_COLOR,0x3009156b653a7b48, CommandSetEntityIconBGColor);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_ID_BG_COLOR,0x4e325f580a5ee789, CommandSetEntityIconIdBGColor);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_TEXT_LABEL,0xceafd9143eb64f81, CommandSetEntityIconTextLabel);
	SCR_REGISTER_UNUSED(SET_ENTITY_ICON_ID_TEXT_LABEL,0x9263d0cd13a06144, CommandSetEntityIconIdTextLabel);


	SCR_REGISTER_SECURE(SET_DRAW_ORIGIN,0x691736a810da2bd4, CommandSetDrawOrigin);
	SCR_REGISTER_SECURE(CLEAR_DRAW_ORIGIN,0xccc0a2ef3dc76a34, CommandClearDrawOrigin);

	// new Bink interface
	SCR_REGISTER_SECURE(SET_BINK_MOVIE,0x16cfd341bcf2f070, CommandSetBinkMovie);
	SCR_REGISTER_SECURE(PLAY_BINK_MOVIE,0xe178310643033958, CommandPlayBinkMovie);
	SCR_REGISTER_SECURE(STOP_BINK_MOVIE,0x58b01bea756fa105, CommandStopBinkMovie);
	SCR_REGISTER_SECURE(RELEASE_BINK_MOVIE,0xf3440233f7d3e150, CommandReleaseBinkMovie);
	SCR_REGISTER_SECURE(DRAW_BINK_MOVIE,0xfc36643f7a64338f, CommandDrawBinkMovie);
	SCR_REGISTER_SECURE(SET_BINK_MOVIE_TIME,0x46f131f72f3d182e, CommandSetBinkMovieTime);
	SCR_REGISTER_SECURE(GET_BINK_MOVIE_TIME,0x9612d31e71756fbe, CommandGetBinkMovieTime);
	SCR_REGISTER_SECURE(SET_BINK_MOVIE_VOLUME,0x6363ada3404d08c8, CommandSetBinkMovieVolume);
	SCR_REGISTER_UNUSED(ATTACH_BINK_AUDIO_TO_ENTITY,0xcabcbda01251c6d1, CommandAttachBinkAudioToEntity);
	SCR_REGISTER_SECURE(ATTACH_TV_AUDIO_TO_ENTITY,0x681f73673fd212dc, CommandAttachTVAudioToEntity);
	SCR_REGISTER_SECURE(SET_BINK_MOVIE_AUDIO_FRONTEND,0x77e1f82f6bd9fe11, CommandSetBinkMovieAudioFrontend);
	SCR_REGISTER_SECURE(SET_TV_AUDIO_FRONTEND,0xc4a210006ff80dea, CommandSetTVAudioFrontend);
	SCR_REGISTER_SECURE(SET_BINK_SHOULD_SKIP,0x01a548d39da6ccdb, CommandSetBinkShouldSkip);

	// Bink mesh set interface
	SCR_REGISTER_SECURE(LOAD_MOVIE_MESH_SET,0xfd6cee56513203fb, CommandLoadMovieMeshSet);
	SCR_REGISTER_SECURE(RELEASE_MOVIE_MESH_SET,0x553c3a3b08976718, CommandReleaseMovieMeshSet);
	SCR_REGISTER_SECURE(QUERY_MOVIE_MESH_SET_STATE,0x1d4a4f0e81be4f45, CommandQueryMovieMeshSetStatus);

	// --- General --------------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(GET_SCREEN_RESOLUTION,0xbe74ec1cd33d16ea, CommandGetScreenResolution );
	SCR_REGISTER_SECURE(GET_ACTUAL_SCREEN_RESOLUTION,0x604161eb05f29e6d, CommandGetActualScreenResolution );
	SCR_REGISTER_SECURE(GET_ASPECT_RATIO,0xf4cc509eeb975296, CommandGetAspectRatio );
	SCR_REGISTER_SECURE(GET_SCREEN_ASPECT_RATIO,0x7cd5a6ca6f7b706d, CommandGetScreenAspectRatio );
	SCR_REGISTER_UNUSED(GET_VIEWPORT_ASPECT_RATIO,0xd54d1fc5a8ffa88c, CommandGetViewportAspectRatio );
	SCR_REGISTER_SECURE(GET_IS_WIDESCREEN,0xd87c62fa7e75d2c5, GetIsWidescreen );
	SCR_REGISTER_SECURE(GET_IS_HIDEF,0x627df494e847af05, GetIsHiDef );
	SCR_REGISTER_SECURE(ADJUST_NEXT_POS_SIZE_AS_NORMALIZED_16_9,0x78bb023d65c925c4, CommandAdjustNextPosSizeAsNormalized16_9 );
#if RSG_GEN9
	SCR_REGISTER_UNUSED(DISPLAY_LOADING_SCREEN_NOW,0x929f64a7cc0a4ea6, CommandForceLoadingScreen );
#endif
	SCR_REGISTER_SECURE(SET_NIGHTVISION,0xcf840ee47fd445aa, CommandSetNightVision );
	SCR_REGISTER_SECURE(GET_REQUESTINGNIGHTVISION,0x333c7ed850043727, CommandGetRequestingNighVision );
	SCR_REGISTER_SECURE(GET_USINGNIGHTVISION,0xbf97536eedb58cbd, CommandGetUsingNightVision );
	SCR_REGISTER_SECURE(SET_EXPOSURETWEAK,0x05bad514d7c46d83, CommandSetExposureTweak );
	SCR_REGISTER_UNUSED(GET_USINGEXPOSURETWEAK,0x0d2cba1646c42532, CommandGetUsingExposureTweak );
	SCR_REGISTER_SECURE(FORCE_EXPOSURE_READBACK,0x5355f8db86bc529f, CommandForceExposureReadback );
	SCR_REGISTER_SECURE(OVERRIDE_NIGHTVISION_LIGHT_RANGE,0xf22bd86fc9446ba0, CommandOverrideNVLightRange);
	SCR_REGISTER_SECURE(SET_NOISEOVERIDE,0xbcedf4ba5f5614d0, CommandSetNoiseOverride );
	SCR_REGISTER_SECURE(SET_NOISINESSOVERIDE,0xd3bc305b83f12eb7, CommandSetNoisinessOverride );
	SCR_REGISTER_SECURE(GET_SCREEN_COORD_FROM_WORLD_COORD,0xf00526c1598a6868, CommandGetScreenPosFromWorldCoord );
	SCR_REGISTER_SECURE(GET_TEXTURE_RESOLUTION,0x7df13542ada68880, CommandGetTextureResolution);
	SCR_REGISTER_SECURE(OVERRIDE_PED_CREW_LOGO_TEXTURE,0x88ab66f9e33da3b3, CommandOverridePedCrewLogoTexture);
	SCR_REGISTER_SECURE(SET_DISTANCE_BLUR_STRENGTH_OVERRIDE,0xb27951eaa6bc6646, CommandSetDistanceBlurStrengthOverride);
	SCR_REGISTER_SECURE(SET_FLASH,0xd0447e81eab7b4ac, CommandSetFlash);
	SCR_REGISTER_SECURE(DISABLE_OCCLUSION_THIS_FRAME,0xc1a703145662449b, CommandDisableOcclusionThisFrame);
	SCR_REGISTER_SECURE(SET_ARTIFICIAL_LIGHTS_STATE,0xb3e46de3b7be51b0,CommandSetArtLightsState);
	SCR_REGISTER_SECURE(SET_ARTIFICIAL_VEHICLE_LIGHTS_STATE,0xaae59b2c50deacf5,CommandSetArtVehLightsState);
	SCR_REGISTER_SECURE(DISABLE_HDTEX_THIS_FRAME,0x6868e2852ae71199,CommandDisableHDTexThisFrame);

	// --- Occlusion Queries ----------------------------------------------------------------------------

	SCR_REGISTER_SECURE(CREATE_TRACKED_POINT,0x7f462558c0a3be7d, CommandCreateTrackedPoint);
	SCR_REGISTER_SECURE(SET_TRACKED_POINT_INFO,0xf3ac30dd9b660e10, CommandSetTrackedPointInfo);
	SCR_REGISTER_SECURE(IS_TRACKED_POINT_VISIBLE,0x0112bd5cb0c9e9ff, CommandIsTrackedPointVisible);
	SCR_REGISTER_SECURE(DESTROY_TRACKED_POINT,0x499b2a8ebb5d097a, CommandDestroyTrackedPointPosition);


	// --- Procedural Grass -----------------------------------------------------------------------------
	SCR_REGISTER_SECURE(SET_GRASS_CULL_SPHERE,0x79130f25a78a7d4b, CommandGrassAddCullSphere);
	SCR_REGISTER_SECURE(REMOVE_GRASS_CULL_SPHERE,0x697ba066ff659189, CommandGrassRemoveCullSphere);
	SCR_REGISTER_UNUSED(RESET_GRASS_CULL_SPHERES,0x666d3c434bb420a2, CommandGrassResetCullSpheres);

	SCR_REGISTER_SECURE(PROCGRASS_ENABLE_CULLSPHERE,0x2d32f9771ed0a4e0,	CommandProcGrassEnableCullSphere);
	SCR_REGISTER_SECURE(PROCGRASS_DISABLE_CULLSPHERE,0x3c2bd77477290272,	CommandProcGrassDisableCullSphere);
	SCR_REGISTER_SECURE(PROCGRASS_IS_CULLSPHERE_ENABLED,0x7b6e209afa2c80cb,	CommandProcGrassIsCullSphereEnabled);

	SCR_REGISTER_SECURE(PROCGRASS_ENABLE_AMBSCALESCAN,0xcb726669fdd0208f, CommandProcGrassEnableAmbScaleScan);
	SCR_REGISTER_SECURE(PROCGRASS_DISABLE_AMBSCALESCAN,0x6da72c1f117f2765, CommandProcGrassDisableAmbScaleScan);

	SCR_REGISTER_SECURE(DISABLE_PROCOBJ_CREATION,0xb874e508e1e4a55c,	CommandDisableProcObjectsCreation);
	SCR_REGISTER_SECURE(ENABLE_PROCOBJ_CREATION,0xc2bbfcd4bb68ff42,	CommandEnableProcObjectsCreation);

	SCR_REGISTER_UNUSED(GRASSBATCH_ENABLE_FLATTENING_EXT_IN_AABB,0x17cf30a587de1a15,	CommandGrassBatchEnableFlatteningExtInAABB);
	SCR_REGISTER_UNUSED(GRASSBATCH_ENABLE_FLATTENING_IN_AABB,0x2b2d79772634f41f,		CommandGrassBatchEnableFlatteningInAABB);
	SCR_REGISTER_SECURE(GRASSBATCH_ENABLE_FLATTENING_EXT_IN_SPHERE,0xf514f657049f7b67,	CommandGrassBatchEnableFlatteningExtInSphere);
	SCR_REGISTER_SECURE(GRASSBATCH_ENABLE_FLATTENING_IN_SPHERE,0x265b9a7b2c12c3f5,		CommandGrassBatchEnableFlatteningInSphere);
	SCR_REGISTER_SECURE(GRASSBATCH_DISABLE_FLATTENING,0xb4d34677ca14a5df,				CommandGrassBatchDisableFlattening);

	// --- Shadows --------------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_INIT_SESSION,0xa91ec7d49df9f229, CommandCascadeShadowsInitSession);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_CASCADE_BOUNDS,0x20f5f3ad1f7ec238, CommandCascadeShadowsSetCascadeBounds);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SNAP,0x238d1893aec39778, CommandCascadeShadowsSetCascadeBoundsSnap);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV,0x8055ca555061be84, CommandCascadeShadowsSetCascadeBoundsHFOV);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV,0x57b8d42b1706a4d5, CommandCascadeShadowsSetCascadeBoundsVFOV);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE,0x51c7ea47553be792, CommandCascadeShadowsSetCascadeBoundsScale);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE,0xd9afa1773b20f447, CommandCascadeShadowsSetEntityTrackerScale);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT,0x77305a4e1c762ab8, CommandCascadeShadowsSetSplitZExpWeight);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_BOUND_POSITION,0xc7f7a759dbce7eb1, CommandCascadeShadowsSetBoundPosition);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER,0x398cfb3534ff01fd, CommandCascadeShadowEnableEntityTracker);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_SCREEN_SIZE_CHECK_ENABLED,0x4ca71976d9ef25d0, CommandCascadeShadowSetScreenSizeCheckEnabled);

	
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE,0xdd7559643235238e, CommandCascadeShadowsSetWorldHeightUpdate);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX,0x0af775dddcad506a, CommandCascadeShadowsSetWorldHeightMinMax);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE,0x7e7c6e995abff4d8, CommandCascadeShadowsSetRecvrHeightUpdate);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX,0xc829526cc8e2af9f, CommandCascadeShadowsSetRecvrHeightMinMax);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE,0x36e6bd43d2b90b27, CommandCascadeShadowsSetDitherRadiusScale);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_DEPTH_BIAS,0x0b6506333d74c390, CommandCascadeShadowsSetDepthBias);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_SLOPE_BIAS,0xad503d34be39f87b, CommandCascadeShadowsSetSlopeBias);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE,0x2cd4854bf8235b97, CommandCascadeShadowsSetShadowSampleType);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_CLEAR_SHADOW_SAMPLE_TYPE,0x335e0094cfabdcea, CommandCascadeShadowsClearShadowSampleType);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_AIRCRAFT_MODE,0xea33835cb4cd38ac, CommandCascadeShadowsSetAircraftMode);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE,0x80a55b04f3bcfc3a, CommandCascadeShadowsSetDynamicDepthMode);
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_VALUE,0xab10c4fa283eeefc, CommandCascadeShadowsSetDynamicDepthValue);
	SCR_REGISTER_UNUSED(CASCADE_SHADOWS_SET_FLY_CAMERA_MODE,0xd3758c3e8218803e, CommandCascadeShadowsSetFlyCameraMode);
	
	SCR_REGISTER_SECURE(CASCADE_SHADOWS_ENABLE_FREEZER,0x06a533a461a8f65d, CommandCascadeShadowsEnableFreezer);

	// --- Water Reflection --------------------------------------------------------------------------------------
	SCR_REGISTER_UNUSED(WATER_REFLECTION_SET_HEIGHT,0x50bfc8262002222f, CommandWaterReflectionSetHeight);
	SCR_REGISTER_SECURE(WATER_REFLECTION_SET_SCRIPT_OBJECT_VISIBILITY,0x63758d6234411cf4, CommandWaterReflectionSetObjectVisibility);

	// --- Golf Trail --------------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_ENABLED,0xed2a45944c876db6, CommandGolfTrailSetEnabled);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_PATH,0xf7f4dad4bc730d37, CommandGolfTrailSetPath);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_RADIUS,0xd852e9e218ae2be2, CommandGolfTrailSetRadius);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_COLOUR,0xd87b21455cf2dff5, CommandGolfTrailSetColour);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_TESSELLATION,0x9729d6c00e75cf41, CommandGolfTrailSetTessellation);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_FIXED_CONTROL_POINT_ENABLE,0x82a234a283aa4d45, CommandGolfTrailSetFixedControlPointsEnabled);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_FIXED_CONTROL_POINT,0xb1df335b838b6f6b, CommandGolfTrailSetFixedControlPoint);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_SHADER_PARAMS,0xda2d0a320d992fa6, CommandGolfTrailSetShaderParams);
	SCR_REGISTER_SECURE(GOLF_TRAIL_SET_FACING,0x8f8e69d279a9ecf4, CommandGolfTrailSetFacing);
	SCR_REGISTER_UNUSED(GOLF_TRAIL_RESET,0x63cfd4f1e88f6ddc, CommandGolfTrailReset);

	SCR_REGISTER_SECURE(GOLF_TRAIL_GET_MAX_HEIGHT,0xd208773c3af637b5, CommandGolfTrailGetMaxHeight);
	SCR_REGISTER_SECURE(GOLF_TRAIL_GET_VISUAL_CONTROL_POINT,0xe172dc8891b21403, CommandGolfTrailGetVisualControlPoint);

	// --- See Through FX -------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(SET_SEETHROUGH,0xc2c3885d7793a4ef, CommandSetSeeThrough );
	SCR_REGISTER_SECURE(GET_USINGSEETHROUGH,0x5e1bcdaabf4860d5, CommandGetUsingSeeThrough );
	SCR_REGISTER_SECURE(SEETHROUGH_RESET,0xc263729724de98ba, CommandSeeThroughResetEffect );
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_FADE_STARTDISTANCE,0x63f2bfd6a0317fd0, CommandSeeThroughGetFadeStartDistance );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_FADE_STARTDISTANCE,0x2296de766d9d8387, CommandSeeThroughSetFadeStartDistance );
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_FADE_ENDDISTANCE,0x91ac7b68d65ee60c, CommandSeeThroughGetFadeEndDistance );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_FADE_ENDDISTANCE,0x3bd51a2b99889304, CommandSeeThroughSetFadeEndDistance );
	SCR_REGISTER_SECURE(SEETHROUGH_GET_MAX_THICKNESS,0x030ed7e4898b79a5, CommandSeeThroughGetMaxThickness );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_MAX_THICKNESS,0x4d784d143adfe4c3, CommandSeeThroughSetMaxThickness );
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_NOISE_MIN,0x1e720916dbd74d27, CommandSeeThroughGetMinNoiseAmount );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_NOISE_MIN,0x92b2207101ebf827, CommandSeeThroughSetMinNoiseAmount );
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_NOISE_MAX,0xd2992d13b8613bc3, CommandSeeThroughGetMaxNoiseAmount );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_NOISE_MAX,0xf1ffeab20bc7e3e5, CommandSeeThroughSetMaxNoiseAmount );
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_HILIGHT_INTENSITY,0xe890390a2cd6df33, CommandSeeThroughGetHiLightIntensity );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_HILIGHT_INTENSITY,0x66a5000c47e6fe05, CommandSeeThroughSetHiLightIntensity );
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_HILIGHT_NOISE,0x5a285b6f53d8b1e4, CommandSeeThroughGetHiLightNoise );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_HIGHLIGHT_NOISE,0xe69023bddb6ee5db, CommandSeeThroughSetHiLightNoise );
	SCR_REGISTER_SECURE(SEETHROUGH_SET_HEATSCALE,0x8d7262898e790499, CommandSeeThroughSetHeatScale);
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_HEATSCALE,0x324e821f2e2b9db8, CommandSeeThroughGetHeatScale);

	SCR_REGISTER_SECURE(SEETHROUGH_SET_COLOR_NEAR,0x592376d5f4bcef69, CommandSeeThroughSetColorNear);
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_COLOR_NEAR,0x3b92266f84cad8bf, CommandSeeThroughGetColorNear);
	SCR_REGISTER_UNUSED(SEETHROUGH_SET_COLOR_FAR,0xa90f752e58b4cb6b, CommandSeeThroughSetColorFar);
	SCR_REGISTER_UNUSED(SEETHROUGH_GET_COLOR_FAR,0x52d8d306e0040d8d, CommandSeeThroughGetColorFar);

	// --- Post FX --------------------------------------------------------------------------------------
	SCR_REGISTER_UNUSED(SET_MOTIONBLUR_MATRIX_OVERRIDE,0x29c72d9959d55fb6, CommandSetMotionBlurMatrixOverride);
	SCR_REGISTER_SECURE(SET_MOTIONBLUR_MAX_VEL_SCALER,0x8463d4cc9d93c39f, CommandSetMotionBlurMaxVelocityScaler);
	SCR_REGISTER_SECURE(GET_MOTIONBLUR_MAX_VEL_SCALER,0x32f13bf158097204, CommandGetMotionBlurMaxVelocityScaler);
	SCR_REGISTER_SECURE(SET_FORCE_MOTIONBLUR,0x6286a24142b5ae12, CommandSetForceMotionBlur);
	SCR_REGISTER_SECURE(TOGGLE_PLAYER_DAMAGE_OVERLAY,0xb777f991c6cf7d90, CommandToggleDamageOverlay);
	SCR_REGISTER_SECURE(RESET_ADAPTATION,0xb5cd7c8585f9a098, CommandResetAdaptation);
	SCR_REGISTER_SECURE(TRIGGER_SCREENBLUR_FADE_IN,0x4c8efafc40f3b914, CommandTriggerScreenBlurFadeIn);
	SCR_REGISTER_SECURE(TRIGGER_SCREENBLUR_FADE_OUT,0x789606e3adce39f7, CommandTriggerScreenBlurFadeOut);
	SCR_REGISTER_SECURE(DISABLE_SCREENBLUR_FADE,0x447303ba4e2014eb, CommandDisableScreenBlurFade);
	SCR_REGISTER_SECURE(GET_SCREENBLUR_FADE_CURRENT_TIME,0x240b0351428767e8, CommandGetScreenBlurFadeCurrentTime);
	SCR_REGISTER_SECURE(IS_SCREENBLUR_FADE_RUNNING,0xac1e9d879866c4ee, CommandIsScreenBlurFadeRunning);
	SCR_REGISTER_SECURE(TOGGLE_PAUSED_RENDERPHASES,0x7e8efd07a6dc03d3,	CommandTogglePauseRenderPhases);
	SCR_REGISTER_SECURE(GET_TOGGLE_PAUSED_RENDERPHASES_STATUS,0xc583550fa062b104, CommandGetTogglePauseRenderPhaseStatus);
	SCR_REGISTER_SECURE(RESET_PAUSED_RENDERPHASES,0x1c2089bf9d3135d3,	CommandResetPauseRenderPhases);
	SCR_REGISTER_UNUSED(ENABLE_TOGGLE_PAUSED_RENDERPHASES,0xc01e2e58980db181, CommandEnableTogglePauseRenderPhases);
	SCR_REGISTER_SECURE(GRAB_PAUSEMENU_OWNERSHIP,0xced819d88937903a, CommandGrabPauseMenuOwnership);
	
	SCR_REGISTER_UNUSED(SET_SNIPERSIGHT_OVERRIDE,0x69c87dbcffd51f17, CommandSetSniperSightOverride);

	SCR_REGISTER_SECURE(SET_HIDOF_OVERRIDE,0x892ad63443d79b92, CommandSetHighDOFOverride);
	SCR_REGISTER_SECURE(SET_LOCK_ADAPTIVE_DOF_DISTANCE,0x4a97cb479ff4631d, CommandSetLockAdaptiveDofDistance);

	SCR_REGISTER_SECURE(PHONEPHOTOEDITOR_TOGGLE,0x3ed60eddc1a35365, CommandPhonePhotoEditorToggle);
	SCR_REGISTER_SECURE(PHONEPHOTOEDITOR_IS_ACTIVE,0x8cd81cdaf32d145a, CommandPhonePhotoEditorIsActive);
	SCR_REGISTER_SECURE(PHONEPHOTOEDITOR_SET_FRAME_TXD,0xb7bf883d5036c603, CommandPhonePhotoEditorSetFrameTXD);
	SCR_REGISTER_UNUSED(PHONEPHOTOEDITOR_SET_TEXT,0x77c6b6c06064fc26, CommandPhonePhotoEditorSetText);

	// --- Particle Effects  ----------------------------------------------------------------------------
	SCR_REGISTER_SECURE(START_PARTICLE_FX_NON_LOOPED_AT_COORD,0xbc6a97baebcd5cd9, CommandTriggerPtFx);
	SCR_REGISTER_SECURE(START_NETWORKED_PARTICLE_FX_NON_LOOPED_AT_COORD,0xaed7808fbb732392, CommandTriggerNetworkedPtFx);
	SCR_REGISTER_SECURE(START_PARTICLE_FX_NON_LOOPED_ON_PED_BONE,0xe4084dc6af8be020, CommandTriggerPtFxOnPedBone);
	SCR_REGISTER_SECURE(START_NETWORKED_PARTICLE_FX_NON_LOOPED_ON_PED_BONE,0x171bbeb194a0227f, CommandTriggerNetworkedPtFxOnPedBone);
	SCR_REGISTER_SECURE(START_PARTICLE_FX_NON_LOOPED_ON_ENTITY,0x74a35bff0670ecd4, CommandTriggerPtFxOnEntity);
	SCR_REGISTER_SECURE(START_NETWORKED_PARTICLE_FX_NON_LOOPED_ON_ENTITY,0xd9e306ff36c32c21, CommandTriggerNetworkedPtFxOnEntity);
	SCR_REGISTER_SECURE(START_PARTICLE_FX_NON_LOOPED_ON_ENTITY_BONE,0xa491a2306f578ced, CommandTriggerPtFxOnEntityBone);	
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_NON_LOOPED_COLOUR,0x6fdd9329d15c7675, CommandSetTriggeredPtFxColour);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_NON_LOOPED_ALPHA,0x6c4a157a5f47c842, CommandSetTriggeredPtFxAlpha);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_NON_LOOPED_SCALE,0x8b176fcee209beb9, CommandSetTriggeredPtFxScale);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_NON_LOOPED_EMITTER_SIZE,0x1e2e01c00837d26e, CommandSetTriggeredPtFxEmitterSize);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_FORCE_VEHICLE_INTERIOR,0x270218794b0ff497, CommandSetTriggeredPtFxForceVehicleInterior);

	SCR_REGISTER_SECURE(START_PARTICLE_FX_LOOPED_AT_COORD,0xe1df0021f629f1d2, CommandStartPtFx);
	SCR_REGISTER_SECURE(START_PARTICLE_FX_LOOPED_ON_PED_BONE,0xe4562f4f41c8b515, CommandStartPtFxOnPedBone);
	SCR_REGISTER_SECURE(START_PARTICLE_FX_LOOPED_ON_ENTITY,0x2d649da6dc187d35, CommandStartPtFxOnEntity);
	SCR_REGISTER_SECURE(START_PARTICLE_FX_LOOPED_ON_ENTITY_BONE,0xdc82d60b1e9406a2, CommandStartPtFxOnEntityBone);
	SCR_REGISTER_SECURE(START_NETWORKED_PARTICLE_FX_LOOPED_ON_ENTITY,0x5e53429e463b220d, CommandStartNetworkedPtFxOnEntity);
	SCR_REGISTER_SECURE(START_NETWORKED_PARTICLE_FX_LOOPED_ON_ENTITY_BONE,0x91a5a4c63c2b5e5d, CommandStartNetworkedPtFxOnEntityBone);
	SCR_REGISTER_SECURE(STOP_PARTICLE_FX_LOOPED,0x0f53bc871ba89c94, CommandStopPtFx);
	SCR_REGISTER_SECURE(REMOVE_PARTICLE_FX,0x1439cb68c14277fb, CommandRemovePtFx);
	SCR_REGISTER_SECURE(REMOVE_PARTICLE_FX_FROM_ENTITY,0xc43ed93266d2a3bb, CommandRemovePtFxFromEntity);
	SCR_REGISTER_SECURE(REMOVE_PARTICLE_FX_IN_RANGE,0x3e100889f69b5149, CommandRemovePtFxInRange);
	SCR_REGISTER_SECURE(FORCE_PARTICLE_FX_IN_VEHICLE_INTERIOR,0xf5da1b9443631838, CommandForcePtFxInVehicleInterior);
	SCR_REGISTER_SECURE(DOES_PARTICLE_FX_LOOPED_EXIST,0x6b8f21f773404988, CommandDoesPtFxExist);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_LOOPED_OFFSETS,0x57d90001269a1354, CommandUpdatePtFxOffsets);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_LOOPED_EVOLUTION,0x39b111ddfa0712b3, CommandEvolvePtFx);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_LOOPED_COLOUR,0x9cca66a85447d77c, CommandUpdatePtFxColour);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_LOOPED_ALPHA,0x8364b4d1fe5492bd, CommandUpdatePtFxAlpha);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_LOOPED_SCALE,0x15aa71a3c4a07e1f, CommandUpdatePtFxScale);
	SCR_REGISTER_UNUSED(SET_PARTICLE_FX_LOOPED_EMITTER_SIZE,0x67f5ef95b2a4a92c, CommandUpdatePtFxEmitterSize);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_LOOPED_FAR_CLIP_DIST,0xa33a1883026b742e, CommandUpdatePtFxFarClipDist);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_CAM_INSIDE_VEHICLE,0xaa4afc804c4ba788, CommandSetFxFlagCamInsideVehicle);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_CAM_INSIDE_NONPLAYER_VEHICLE,0x172badec55cd3dcb, CommandSetFxFlagCamInsideNonPlayerVehicle);

	SCR_REGISTER_SECURE(SET_PARTICLE_FX_SHOOTOUT_BOAT,0x50ff2024d3503ad5, CommandSetFxFlagShootoutBoat);
	SCR_REGISTER_SECURE(CLEAR_PARTICLE_FX_SHOOTOUT_BOAT,0xe915f7585917c52f, CommandClearFxFlagShootoutBoat);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_BLOOD_SCALE,0xde1095a40e375ae9, CommandSetBloodPtFxScale);
	SCR_REGISTER_UNUSED(DISABLE_BLOOD_VFX,0xcc85bd23691047ef, CommandDisableBloodVfx);
	SCR_REGISTER_SECURE(DISABLE_IN_WATER_PTFX,0xcfe5ec0c2b0b9531, CommandDisableInWaterPtFx);
	SCR_REGISTER_SECURE(DISABLE_DOWNWASH_PTFX,0xae29d588d027bf34, CommandDisableDownwashPtfx);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_SLIPSTREAM_LODRANGE_SCALE,0xaf00e6a5fb6dce38, CommandSetSlipstreamPtFxLodRangeScale);
	SCR_REGISTER_SECURE(ENABLE_CLOWN_BLOOD_VFX,0xb74716c5f4a7623e, CommandEnableClownBloodVfx);
	SCR_REGISTER_SECURE(ENABLE_ALIEN_BLOOD_VFX,0xa5d10312d00c63de, CommandEnableAlienBloodVfx);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_BULLET_IMPACT_SCALE,0x4e7172e30cc3ca0a, CommandSetBulletImpactPtFxScale);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_BULLET_IMPACT_LODRANGE_SCALE,0x1c7591eed58e42df, CommandSetBulletImpactPtFxLodRangeScale);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_BULLET_TRACE_NO_ANGLE_REJECT,0x1c0734b49e9e6898, CommandSetBulletTraceNoAngleReject);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_BANG_SCRAPE_LODRANGE_SCALE,0xd5fbbcdda2486ad2, CommandSetBangScrapePtFxLodRangeScale);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_FOOT_LODRANGE_SCALE,0xa204f683b0528390, CommandSetFootPtFxLodRangeScale);
	SCR_REGISTER_SECURE(SET_PARTICLE_FX_FOOT_OVERRIDE_NAME,0xc3f733a1a64bb1e8, CommandSetFootPtFxOverrideName);
	SCR_REGISTER_UNUSED(SET_PARTICLE_FX_WHEEL_LODRANGE_SCALE,0x6f5f2ad41f7b4936, CommandSetWheelPtFxLodRangeScale);
	SCR_REGISTER_SECURE(SET_SKIDMARK_RANGE_SCALE,0xb18e5568eaad15f2, CommandSetWheelSkidmarkRangeScale);
	SCR_REGISTER_SECURE(SET_PTFX_FORCE_VEHICLE_INTERIOR_FLAG,0xc6730e0d14e50703, CommandSetPtFxForceVehicleInteriorFlag);

	SCR_REGISTER_UNUSED(OVERRIDE_PARTICLE_FX_WATER_SPLASH_OBJECT_IN,0x9371dd9fd1e2cb97, CommandOverridePtFxObjectWaterSplashIn);

	SCR_REGISTER_UNUSED(SET_PARTICLE_FX_NO_DOWNSAMPLING_THIS_FRAME,0x2d82eb3fe61e670a, CommandSetPtFxNoDownsamplingThisFrame);
	
	SCR_REGISTER_UNUSED(REGISTER_POSTFX_BULLET_IMPACT,0x73d010fff76748a7, CommandRegisterBulletImpact);

	SCR_REGISTER_SECURE(FORCE_POSTFX_BULLET_IMPACTS_AFTER_HUD,0x54685eea86e8e51e, CommandForceBulletImpactOverlaysAfterHud);
		

	SCR_REGISTER_SECURE(USE_PARTICLE_FX_ASSET,0x76b797b61752aab8, CommandUseParticleFxAsset);

	SCR_REGISTER_SECURE(SET_PARTICLE_FX_OVERRIDE,0xa01f69ade5032968, CommandSetParticleFxOverride);
	SCR_REGISTER_SECURE(RESET_PARTICLE_FX_OVERRIDE,0x4f743783acd5e6e6, CommandResetParticleFxOverride);
	
	SCR_REGISTER_UNUSED(TRIGGER_PARTICLE_FX_EMP,0xd513d0b2ec275792, CommandTriggerPtFxEMP);

	// --- Weather/Region Ptfx --------------------------------------------------------------------------------------

	SCR_REGISTER_SECURE(SET_WEATHER_PTFX_USE_OVERRIDE_SETTINGS,0xa2b1c1c06e1d3bb5, CommandWeatherRegionPtfxSetUseOverrideSettings);
	SCR_REGISTER_SECURE(SET_WEATHER_PTFX_OVERRIDE_CURR_LEVEL,0x5406ff5ecea3659f, CommandWeatherRegionPtfxSetOverrideCurrLevel);
	SCR_REGISTER_UNUSED(SET_WEATHER_PTFX_OVERRIDE_BOX_OFFSET,0x9fbd8dd0ee7935d9, CommandWeatherRegionPtfxSetSetOverrideBoxOffset);
	SCR_REGISTER_UNUSED(SET_WEATHER_PTFX_OVERRIDE_BOX_SIZE,0x6dc6698d47357d24, CommandWeatherRegionPtfxSetSetOverrideBoxSize);


	// --- Decals  --------------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(WASH_DECALS_IN_RANGE,0xd2ef7ba398d77b2b, CommandWashDecalsInRange);
	SCR_REGISTER_SECURE(WASH_DECALS_FROM_VEHICLE,0x23e905c5f39ef42a, CommandWashDecalsFromVehicle);
	SCR_REGISTER_SECURE(FADE_DECALS_IN_RANGE,0x152bc923d5118c6f, CommandFadeDecalsInRange);
	SCR_REGISTER_SECURE(REMOVE_DECALS_IN_RANGE,0xef071f2ff93bc37f, CommandRemoveDecalsInRange);
	SCR_REGISTER_SECURE(REMOVE_DECALS_FROM_OBJECT,0xca8bf400114ce91e,	CommandRemoveDecalsFromObject);
	SCR_REGISTER_SECURE(REMOVE_DECALS_FROM_OBJECT_FACING,0xa25856e97dbb67b4,	CommandRemoveDecalsFromObjectFacing);
	SCR_REGISTER_SECURE(REMOVE_DECALS_FROM_VEHICLE,0xf5da3f1915d29e2c, CommandRemoveDecalsFromVehicle);
	SCR_REGISTER_UNUSED(REMOVE_DECALS_FROM_VEHICLE_FACING,0x643ee33b811308be, CommandRemoveDecalsFromVehicleFacing);
	SCR_REGISTER_SECURE(ADD_DECAL,0x20f895e512ec5db6, CommandAddDecal);
	SCR_REGISTER_SECURE(ADD_PETROL_DECAL,0x44949ec4364cc53b, CommandAddPetrolDecal);
	SCR_REGISTER_SECURE(ADD_OIL_DECAL,0x126d7f89fe859a5e, CommandAddOilDecal);
	SCR_REGISTER_UNUSED(START_SKIDMARK_DECALS,0x5b721dc2b9dd7538, CommandStartSkidmarkDecals);
	SCR_REGISTER_UNUSED(ADD_SKIDMARK_DECAL_INFO,0x802082fdf21f8e9c, CommandAddSkidmarkDecalInfo);
	SCR_REGISTER_UNUSED(END_SKIDMARK_DECALS,0x18e71758e5b89d9f, CommandEndSkidmarkDecals);
	SCR_REGISTER_SECURE(START_PETROL_TRAIL_DECALS,0x9f0371728df05832, CommandStartPetrolTrailDecals);
	SCR_REGISTER_SECURE(ADD_PETROL_TRAIL_DECAL_INFO,0xa22d087a4f4d9310, CommandAddPetrolTrailDecalInfo);
	SCR_REGISTER_SECURE(END_PETROL_TRAIL_DECALS,0x1dd01e482da60fba, CommandEndPetrolTrailDecals);
	SCR_REGISTER_UNUSED(REGISTER_MARKER_DECAL,0xbf52e616037c07ee, CommandRegisterMarkerDecal);
	SCR_REGISTER_SECURE(REMOVE_DECAL,0x12b2b502c41ef25e, CommandRemoveDecal);
	SCR_REGISTER_SECURE(IS_DECAL_ALIVE,0xd39231cabdb82d64, CommandIsDecalAlive);
	SCR_REGISTER_SECURE(GET_DECAL_WASH_LEVEL,0x89f54ec4a6351b0f, CommandGetDecalWashLevel);
	SCR_REGISTER_UNUSED(SET_DISABLE_PETROL_DECALS_IGNITING,0x576f5f816e776077, CommandDisablePetrolDecalsIgniting);
	SCR_REGISTER_SECURE(SET_DISABLE_PETROL_DECALS_IGNITING_THIS_FRAME,0x19f63003b476d77a, CommandDisablePetrolDecalsIgnitingThisFrame);
	SCR_REGISTER_SECURE(SET_DISABLE_PETROL_DECALS_RECYCLING_THIS_FRAME,0x89e623b2dab6d83c, CommandDisablePetrolDecalsRecyclingThisFrame);
	SCR_REGISTER_SECURE(SET_DISABLE_DECAL_RENDERING_THIS_FRAME,0x7cb01c712d0b1639, CommandDisableDecalRenderingThisFrame);
	SCR_REGISTER_SECURE(GET_IS_PETROL_DECAL_IN_RANGE,0x6a1edf18639d033e, CommandGetIsPetrolDecalInRange);
	SCR_REGISTER_SECURE(PATCH_DECAL_DIFFUSE_MAP,0xb1004d004a7e8c8e, CommandPatchDecalDiffuseMap);
	SCR_REGISTER_SECURE(UNPATCH_DECAL_DIFFUSE_MAP,0x47671aa477611c56, CommandUnPatchDecalDiffuseMap);
	SCR_REGISTER_SECURE(MOVE_VEHICLE_DECALS,0x4e2de31af276df50, CommandMoveVehicleDecals);
	SCR_REGISTER_SECURE(ADD_VEHICLE_CREW_EMBLEM,0x288e8f521b7ab08d, CommandAddVehicleCrewEmblem);
	SCR_REGISTER_UNUSED(ADD_VEHICLE_CREW_EMBLEM_USING_TEXTURE,0x737f31e4debe2a25, CommandAddVehicleCrewEmblemUsingTexture);
	SCR_REGISTER_UNUSED(ADD_VEHICLE_TOURNAMENT_EMBLEM,0x6f193130090db577, CommandAddVehicleTournamentEmblem);
	SCR_REGISTER_SECURE(ABORT_VEHICLE_CREW_EMBLEM_REQUEST,0xe08352579c33866c, CommandAbortVehicleCrewEmblem);
	SCR_REGISTER_SECURE(REMOVE_VEHICLE_CREW_EMBLEM,0x30e6a7038dbfeb9f, CommandRemoveVehicleCrewEmblem);
	SCR_REGISTER_SECURE(GET_VEHICLE_CREW_EMBLEM_REQUEST_STATE,0x596e9ec72d8dee73, CommandGetVehicleBadgeRequestState);
	SCR_REGISTER_SECURE(DOES_VEHICLE_HAVE_CREW_EMBLEM,0xa763b3f87da3c316, CommandDoesVehicleHaveCrewEmblem);
	SCR_REGISTER_UNUSED(DISABLE_FOOTPRINT_DECALS,0x712533916b0e0346, CommandDisableFootprintDecals);
	SCR_REGISTER_SECURE(DISABLE_COMPOSITE_SHOTGUN_DECALS,0xcbdc492e30316d9b, CommandDisableCompositeShotgunDecals);
	SCR_REGISTER_SECURE(DISABLE_SCUFF_DECALS,0x7c1f0e846828c59f, CommandDisableScuffDecals);
	SCR_REGISTER_SECURE(SET_DECAL_BULLET_IMPACT_RANGE_SCALE,0xfb04b0d25eeab773, CommandSetBulletImpactDecalRangeScale);
	
	// --- Vfx  -----------------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(OVERRIDE_INTERIOR_SMOKE_NAME,0xe798481a8e535a88, CommandOverrideInteriorSmokeName);
	SCR_REGISTER_UNUSED(OVERRIDE_INTERIOR_SMOKE_NAME_LENS,0x2f8c56e571f2e329, CommandOverrideInteriorSmokeNameLens);
	SCR_REGISTER_SECURE(OVERRIDE_INTERIOR_SMOKE_LEVEL,0xfd12a532298d02c4, CommandOverrideInteriorSmokeLevel);
	SCR_REGISTER_SECURE(OVERRIDE_INTERIOR_SMOKE_END,0x700aba56d7a3860a, CommandOverrideInteriorSmokeEnd);
	SCR_REGISTER_SECURE(REGISTER_NOIR_LENS_EFFECT,0xf484cd97db3780cc, CommandRegisterNoirLensEffect);
	SCR_REGISTER_SECURE(DISABLE_VEHICLE_DISTANTLIGHTS,0xe0140518c369816e, CommandDisableVehicleLights);
	SCR_REGISTER_UNUSED(DISABLE_LODLIGHTS,0xe1b852a04b8fc6ba, CommandDisableLODLights);
	SCR_REGISTER_SECURE(RENDER_SHADOWED_LIGHTS_WITH_NO_SHADOWS,0x797e7eafcef57a43, CommandRenderShadowsLightsWithNoShadows);
	SCR_REGISTER_SECURE(REQUEST_EARLY_LIGHT_CHECK,0x0f839cce4ca00021, CommandRequestEarlyLightCheck);
	
	SCR_REGISTER_SECURE(USE_SNOW_FOOT_VFX_WHEN_UNSHELTERED,0xa968ffff56c48d0a, CommandUseSnowFootVfxWhenUnsheltered);
	SCR_REGISTER_SECURE(USE_SNOW_WHEEL_VFX_WHEN_UNSHELTERED,0x02ba4710df2fcfc3, CommandUseSnowWheelVfxWhenUnsheltered);

	SCR_REGISTER_SECURE(DISABLE_REGION_VFX,0xad6f16c0a4a10ebb, CommandDisableRegionVfx);

	// --- Timecycle  -----------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(PRESET_INTERIOR_AMBIENT_CACHE,0x074934b9f1d2dc3c, CommandPreSetInteriorAmbientCache);
	
	SCR_REGISTER_SECURE(SET_TIMECYCLE_MODIFIER,0x5c3549d308ec0b7f, CommandSetTimeCycleModifier);
	SCR_REGISTER_SECURE(SET_TIMECYCLE_MODIFIER_STRENGTH,0xac7136a72de288c8, CommandSetTimeCycleModifierStrength);
	
	SCR_REGISTER_SECURE(SET_TRANSITION_TIMECYCLE_MODIFIER,0x4ecb2825299df02e, CommandSetTransitionTimeCycleModifier);
	SCR_REGISTER_SECURE(SET_TRANSITION_OUT_OF_TIMECYCLE_MODIFIER,0xbbfaae0f5c7709c5, CommandSetTransitionOutOfTimeCycleModifier);
	SCR_REGISTER_SECURE(CLEAR_TIMECYCLE_MODIFIER,0xdeabc7ec7da2b48e, CommandClearTimeCycleModifier);
	SCR_REGISTER_SECURE(GET_TIMECYCLE_MODIFIER_INDEX,0x607b649f72d20b6a, CommandGetTimeCycleModifierIndex);
	SCR_REGISTER_SECURE(GET_TIMECYCLE_TRANSITION_MODIFIER_INDEX,0x7234e7ff7b1c3ae3, CommandGetTransitionTimeCycleModifierIndex);
	
	SCR_REGISTER_SECURE(GET_IS_TIMECYCLE_TRANSITIONING_OUT,0x31e4c810a77a13f8, CommandGetIsTimecycleTransitioningOut);

	SCR_REGISTER_SECURE(PUSH_TIMECYCLE_MODIFIER,0x75896fbc1c205df1, CommandPushTimeCycleModifier);
	SCR_REGISTER_UNUSED(CLEAR_PUSHED_TIMECYCLE_MODIFIER,0x61383c0ce774ed0c, CommandClearPushedTimeCycleModifier);
	SCR_REGISTER_SECURE(POP_TIMECYCLE_MODIFIER,0x200c8054b5d04eb5, CommandPopTimeCycleModifier);
	
	SCR_REGISTER_SECURE(SET_CURRENT_PLAYER_TCMODIFIER,0x3c6b3e8d95e0f1fa, CommandSetCurrentPlayerTimeCycleModifier);
	SCR_REGISTER_UNUSED(START_PLAYER_TCMODIFIER_TRANSITION,0x5765428e12a06eac, CommandStartPlayerTimeCycleModifierTransition);
	SCR_REGISTER_SECURE(SET_PLAYER_TCMODIFIER_TRANSITION,0xca3360db84c1b6e3, CommandSetPlayerTimeCycleModifierTransition);
	SCR_REGISTER_SECURE(SET_NEXT_PLAYER_TCMODIFIER,0x3378b05a908220ae, CommandSetNextPlayerTimeCycleModifier);
	
	SCR_REGISTER_UNUSED(SET_TIMECYCLE_REGION_OVERRIDE,0x7ff110a1aa22e294, CommandSetTimeCycleRegionOverride);
	SCR_REGISTER_UNUSED(GET_TIMECYCLE_REGION_OVERRIDE,0x62b8f6d1ef916c74, CommandGetTimeCycleRegionOverride);

	SCR_REGISTER_SECURE(ADD_TCMODIFIER_OVERRIDE,0x62fa36f96ed238bd, CommandAddTimeCycleOverride);
	SCR_REGISTER_UNUSED(REMOVE_TCMODIFIER_OVERRIDE,0x44ef7c1f400e0871, CommandRemoveModifierOverride);
	SCR_REGISTER_SECURE(CLEAR_ALL_TCMODIFIER_OVERRIDES,0x419d7af047a524f2, CommandClearModifierOverrides);

	SCR_REGISTER_SECURE(SET_EXTRA_TCMODIFIER,0xfae487ec4c051ed7,CommandSetExtraTimeCycleModifier);
	SCR_REGISTER_SECURE(CLEAR_EXTRA_TCMODIFIER,0x041f9bcedbbcdf7f,CommandClearExtraTimeCycleModifier);
	SCR_REGISTER_SECURE(GET_EXTRA_TCMODIFIER,0x1b9d803d538f5624,CommandGetExtraTimeCycleModifierIndex);

	SCR_REGISTER_UNUSED(GET_DAY_NIGHT_BALANCE,0xb0d34b375eab2f1f, CommandGetDayNightBalance);
	
	SCR_REGISTER_SECURE(ENABLE_MOON_CYCLE_OVERRIDE,0x306d379656f4f87a, CommandEnableMoonCycleOverride);
	SCR_REGISTER_SECURE(DISABLE_MOON_CYCLE_OVERRIDE,0xb77cc7d5e8c88bad, CommandDisableMoonCycleOverride);

#if RSG_GEN9
	SCR_REGISTER_UNUSED(SET_SUN_ROLL_ANGLE, 0x71993ff5d218e78a, CommandSetSunRollAngle);
	SCR_REGISTER_UNUSED(SET_SUN_YAW_ANGLE, 0x55092887fb4045fd, CommandSetSunYawAngle);
#endif // RSG_GEN9

	// --- Scaleform  -----------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(REQUEST_SCALEFORM_MOVIE,0x528279f3f1eef869, CommandRequestScaleformMovie);
	SCR_REGISTER_SECURE(REQUEST_SCALEFORM_MOVIE_WITH_IGNORE_SUPER_WIDESCREEN,0xc39d9c4d703fd9fd, CommandRequestScaleformMovieWithIgnoreSuperWidescreen);
	SCR_REGISTER_SECURE(REQUEST_SCALEFORM_MOVIE_INSTANCE,0xd9ea500adeac231a, CommandRequestScaleformMovie);
	SCR_REGISTER_SECURE(REQUEST_SCALEFORM_MOVIE_SKIP_RENDER_WHILE_PAUSED,0xa002427e922f5cf8, CommandRequestScaleformMovieSkipRenderWhilePaused);
	SCR_REGISTER_SECURE(HAS_SCALEFORM_MOVIE_LOADED,0x0347ccbd719c8adc, CommandHasScaleformMovieLoaded);
	SCR_REGISTER_SECURE(IS_ACTIVE_SCALEFORM_MOVIE_DELETING ,0xa0d8456ab267ed90, CommandIsActiveScaleformMovieDeleting);
	SCR_REGISTER_SECURE(IS_SCALEFORM_MOVIE_DELETING ,0x46ff72ad8ecb5da0, CommandIsScaleformMovieDeleting);
	SCR_REGISTER_SECURE(HAS_SCALEFORM_MOVIE_FILENAME_LOADED,0xa713b1736ec49e9d, CommandHasScaleformMovieFilenameLoaded);
	SCR_REGISTER_SECURE(HAS_SCALEFORM_CONTAINER_MOVIE_LOADED_INTO_PARENT,0x4b0e0bd65f1ec72c, CommandHasScaleformContainerMovieLoadedIntoParent);
	SCR_REGISTER_SECURE(SET_SCALEFORM_MOVIE_AS_NO_LONGER_NEEDED,0x705b098546deb18a, CommandSetScaleformMovieAsNoLongerNeeded);
	SCR_REGISTER_SECURE(SET_SCALEFORM_MOVIE_TO_USE_SYSTEM_TIME,0xd6d689b76f32f4aa, CommandSetScaleformMovieToUseSystemTime);
	SCR_REGISTER_SECURE(SET_SCALEFORM_MOVIE_TO_USE_LARGE_RT,0x7b2330f209ea5ba9, CommandSetScaleformMovieToUseLargeRT);
	SCR_REGISTER_SECURE(SET_SCALEFORM_MOVIE_TO_USE_SUPER_LARGE_RT,0x56e3c23258531169, CommandSetScaleformMovieToUseSuperLargeRT);
	SCR_REGISTER_SECURE(DRAW_SCALEFORM_MOVIE,0x694170bb080c08ff, CommandDrawScaleformMovie);
	SCR_REGISTER_SECURE(DRAW_SCALEFORM_MOVIE_FULLSCREEN,0xc4353d240dce9533, CommandDrawScaleformMovieFullScreen);
	SCR_REGISTER_SECURE(DRAW_SCALEFORM_MOVIE_FULLSCREEN_MASKED,0xb0393791863712b1, CommandDrawScaleformMovieFullScreenMasked);
	SCR_REGISTER_SECURE(DRAW_SCALEFORM_MOVIE_3D,0xa8505db724f74b62, CommandDrawScaleformMovie3D);
	SCR_REGISTER_UNUSED(DRAW_SCALEFORM_MOVIE_3D_ATTACHED_TO_ENTITY,0xa29e7109d679f12e, CommandDrawScaleformMovieAttachedToEntity3D);
	SCR_REGISTER_SECURE(DRAW_SCALEFORM_MOVIE_3D_SOLID,0x4d3ecb46a812492a, CommandDrawScaleformMovie3DSolid);
	SCR_REGISTER_UNUSED(SET_SCALEFORM_MOVIE_3D_BRIGHTNESS,0xa73844c238f80f80, CommandSetScaleformMovie3DBrightness);
	SCR_REGISTER_SECURE(CALL_SCALEFORM_MOVIE_METHOD,0x966fceabcb8fa5e7, CommandCallScaleformMovieMethod);
	SCR_REGISTER_SECURE(CALL_SCALEFORM_MOVIE_METHOD_WITH_NUMBER,0x3c6414ec6636d573, CommandCallScaleformMovieMethodWithNumber);
	SCR_REGISTER_SECURE(CALL_SCALEFORM_MOVIE_METHOD_WITH_STRING,0x2b7e260b913761aa, CommandCallScaleformMovieMethodWithString);
	SCR_REGISTER_SECURE(CALL_SCALEFORM_MOVIE_METHOD_WITH_NUMBER_AND_STRING,0x2e6fa6bfc49a1aa8, CommandCallScaleformMovieMethodWithNumberAndString);
	SCR_REGISTER_UNUSED(CALL_SCALEFORM_MOVIE_METHOD_WITH_NUMBER_AND_LITERAL_STRING,0x9aa931d176058dd3, CommandCallScaleformMovieMethodWithNumberAndLiteralString);
	SCR_REGISTER_UNUSED(CALL_SCALEFORM_MOVIE_METHOD_WITH_LITERAL_STRING,0x5a394c96a0fb1fb7, CommandCallScaleformMovieMethodWithLiteralString);

	SCR_REGISTER_SECURE(BEGIN_SCALEFORM_SCRIPT_HUD_MOVIE_METHOD,0x1d728abcf062ce8b, CommandBeginScaleformScriptHudMovieMethod);
	SCR_REGISTER_SECURE(BEGIN_SCALEFORM_MOVIE_METHOD,0xea5dea46c3ee64d3, CommandBeginScaleformMovieMethod);
	SCR_REGISTER_SECURE(BEGIN_SCALEFORM_MOVIE_METHOD_ON_FRONTEND,0xca3cb5ce1b9bfe03, CommandBeginScaleformMovieMethodOnFrontend);
	SCR_REGISTER_SECURE(BEGIN_SCALEFORM_MOVIE_METHOD_ON_FRONTEND_HEADER,0x8481f9c297e31e1e, CommandBeginScaleformMovieMethodOnFrontendHeader);
	SCR_REGISTER_SECURE(END_SCALEFORM_MOVIE_METHOD,0x6f06cf0e9ab02847, CommandEndScaleformMovieMethod);
	SCR_REGISTER_SECURE(END_SCALEFORM_MOVIE_METHOD_RETURN_VALUE,0xd452b47f164a4d79, CommandEndScaleformMovieMethodReturnValue);
	
	SCR_REGISTER_SECURE(IS_SCALEFORM_MOVIE_METHOD_RETURN_VALUE_READY,0x17e14239fb53cce3, CommandIsScaleformMovieMethodReturnValueReady);
	SCR_REGISTER_SECURE(GET_SCALEFORM_MOVIE_METHOD_RETURN_VALUE_INT,0xc2f770299dffa794, CommandGetScaleformMovieMethodReturnValueInt);
	SCR_REGISTER_UNUSED(GET_SCALEFORM_MOVIE_METHOD_RETURN_VALUE_FLOAT,0x309d739d3b353b83, CommandGetScaleformMovieMethodReturnValueFloat);
	SCR_REGISTER_SECURE(GET_SCALEFORM_MOVIE_METHOD_RETURN_VALUE_BOOL,0x5200df159ad62c73, CommandGetScaleformMovieMethodReturnValueBool);
	SCR_REGISTER_SECURE(GET_SCALEFORM_MOVIE_METHOD_RETURN_VALUE_STRING,0xad2773d0effe7b10, CommandGetScaleformMovieMethodReturnValueString);
	
	SCR_REGISTER_SECURE(SCALEFORM_MOVIE_METHOD_ADD_PARAM_INT,0x4f47e317c74c543b, CommandScaleformMovieMethodAddParamInt);
	SCR_REGISTER_SECURE(SCALEFORM_MOVIE_METHOD_ADD_PARAM_FLOAT,0xca5d23e5f0f0306f, CommandScaleformMovieMethodAddParamFloat);
	SCR_REGISTER_SECURE(SCALEFORM_MOVIE_METHOD_ADD_PARAM_BOOL,0xd7d6ba6e36aec182, CommandScaleformMovieMethodAddParamBool);
	SCR_REGISTER_SECURE(BEGIN_TEXT_COMMAND_SCALEFORM_STRING,0x4adc8b166e139423, CommandBeginTextCommandScaleformString);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_SCALEFORM_STRING,0xd1d4f8d5470afa4c, CommandEndTextCommandScaleformString);
	SCR_REGISTER_SECURE(END_TEXT_COMMAND_UNPARSED_SCALEFORM_STRING,0x50ba5780e068a6bd, CommandEndTextCommandUnparsedScaleformString);
	SCR_REGISTER_SECURE(SCALEFORM_MOVIE_METHOD_ADD_PARAM_LITERAL_STRING,0x7909d3fc3439d6fd, CommandScaleformMovieMethodAddParamLiteralString);
	SCR_REGISTER_SECURE(SCALEFORM_MOVIE_METHOD_ADD_PARAM_TEXTURE_NAME_STRING,0x35395e05c7db18d0, CommandScaleformMovieMethodAddParamLiteralString);
	SCR_REGISTER_SECURE(SCALEFORM_MOVIE_METHOD_ADD_PARAM_PLAYER_NAME_STRING,0x341872e4d54cd053, CommandScaleformMovieMethodAddParamLiteralString);
	SCR_REGISTER_SECURE(DOES_LATEST_BRIEF_STRING_EXIST,0x3e9bfa925f24e4ef, CommandDoesLatestBriefStringExist);
	SCR_REGISTER_SECURE(SCALEFORM_MOVIE_METHOD_ADD_PARAM_LATEST_BRIEF_STRING,0x511b3958c06bf91b, CommandScaleformMovieMethodAddParamLatestBriefString);


	SCR_REGISTER_SECURE(REQUEST_SCALEFORM_SCRIPT_HUD_MOVIE,0x7d223dd16b5e51f3, CommandRequestScaleformScriptHudMovie);
	SCR_REGISTER_SECURE(HAS_SCALEFORM_SCRIPT_HUD_MOVIE_LOADED,0x19b4facadac4d97c, CommandHasScaleformScriptHudMovieLoaded);
	SCR_REGISTER_SECURE(REMOVE_SCALEFORM_SCRIPT_HUD_MOVIE,0x13858af709184724, CommandRemoveScaleformScriptHudMovie);
	SCR_REGISTER_UNUSED(SET_SCALEFORM_SCRIPT_HUD_MOVIE_TO_FRONT,0xb6b203dd3530588a, CommandSetScaleformScriptHudMovieToFront);
	SCR_REGISTER_UNUSED(SET_SCALEFORM_SCRIPT_HUD_MOVIE_TO_BACK,0x75deee340f06f060, CommandSetScaleformScriptHudMovieToBack);
	SCR_REGISTER_UNUSED(SET_SCALEFORM_SCRIPT_HUD_MOVIE_RELATIVE_TO_COMPONENT,0x21e91a244f2289b7, CommandSetScaleformScriptHudMovieRelativeToComponent);
	SCR_REGISTER_UNUSED(SET_SCALEFORM_SCRIPT_HUD_MOVIE_RELATIVE_TO_HUD_COMPONENT,0x1eabaec732080ce4, CommandSetScaleformScriptHudMovieRelativeToHudComponent);

	SCR_REGISTER_SECURE(PASS_KEYBOARD_INPUT_TO_SCALEFORM,0xcfca4ff4aaca8855, CommandPassKeyboardInputToScaleform);

	// --- TV Channel Stuff -------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(SET_TV_CHANNEL,0x71dfb1e45d792d8c, CommandSetTVChannel);
	SCR_REGISTER_SECURE(GET_TV_CHANNEL,0xb6859807232d8aed, CommandGetTVChannel);	
	SCR_REGISTER_SECURE(SET_TV_VOLUME,0xcb9476bfd12cd0d4, CommandSetTVVolume);	
	SCR_REGISTER_SECURE(GET_TV_VOLUME,0xd6385c54cbbb95cd, CommandGetTVVolume);	
	SCR_REGISTER_UNUSED(GET_TV_CURRENT_VIDEO_HANDLE,0xb2915094eb118ed3, CommandGetPlayingTVChannelVideoHandle);	
	SCR_REGISTER_SECURE(DRAW_TV_CHANNEL,0x8e2a4f58a41acb36, CommandDrawTVChannel);
	SCR_REGISTER_SECURE(SET_TV_CHANNEL_PLAYLIST,0xa7ac3c9e15f0a7dd, CommandAssignPlaylistToChannel);
	SCR_REGISTER_SECURE(SET_TV_CHANNEL_PLAYLIST_AT_HOUR,0x53c6566a999a13e5, CommandAssignPlaylistToChannelAtHour);

	SCR_REGISTER_SECURE(CLEAR_TV_CHANNEL_PLAYLIST,0x7cb074c2d7222200, CommandClearPlaylistOnChannel);
	SCR_REGISTER_SECURE(IS_PLAYLIST_ON_CHANNEL,0x0ad1f14eb34b894c, CommandIsPlaylistOnChannel );
	SCR_REGISTER_SECURE(IS_TVSHOW_CURRENTLY_PLAYING,0x7b8dbf41f99c5da2, CommandIsTVShowCurrentlyPlaying );
	SCR_REGISTER_SECURE(ENABLE_MOVIE_KEYFRAME_WAIT,0xc367ddff1e85a36b, CommandForceBinkKeyframeSwitch );
	SCR_REGISTER_SECURE(SET_TV_PLAYER_WATCHING_THIS_FRAME,0x9a01624727325c8a, CommandSetPlayerWatchingTVThisFrame);

	SCR_REGISTER_SECURE(GET_CURRENT_TV_CLIP_NAMEHASH,0x93e4d3e19bae1398, GetCurrentTVClipNamehash);

	// --- Bink Movie Subtitles ------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(ENABLE_MOVIE_SUBTITLES,0x266d7c8b18f0c4db, CommandEnableMovieSubtitles);

	// --- UI 3D Draw Manager -------------------------------------------------------------------------------
	SCR_REGISTER_SECURE(UI3DSCENE_IS_AVAILABLE,0x5e115643a74047b1, CommandUI3DSceneIsAvailable);
	SCR_REGISTER_SECURE(UI3DSCENE_PUSH_PRESET,0x0c008f6397722a9d, CommandUI3DScenePushPreset);
	SCR_REGISTER_SECURE(UI3DSCENE_ASSIGN_PED_TO_SLOT,0x2ca227b8534580d8, CommandUI3DSceneAssignPedToSlot);
	SCR_REGISTER_UNUSED(UI3DSCENE_ASSIGN_PED_TO_SLOT_WITH_ROTATION,0xb290cd949bf7dfad, CommandUI3DSceneAssignPedToSlotWithRotOffset);
	SCR_REGISTER_UNUSED(UI3DSCENE_ASSIGN_LIGHT_INTENSITY_TO_SLOT,0xc1e9f480f69fdd87, CommandUI3DSceneAssignGlobalLightIntensityToSlot);
	SCR_REGISTER_SECURE(UI3DSCENE_CLEAR_PATCHED_DATA,0x700e5926e4f3c739, CommandUI3DSceneClearPatchedData);
	SCR_REGISTER_SECURE(UI3DSCENE_MAKE_PUSHED_PRESET_PERSISTENT,0x4506c9ef247a8e43, CommandUI3DSceneMakeCurrentPresetPersistent);

	// --- Terrain Grid for Mini Golf ------------------------------------------------------------------------
	SCR_REGISTER_SECURE(TERRAINGRID_ACTIVATE,0x3d6f04eb56bd779e, CommandTerrainGridActivate);
	SCR_REGISTER_SECURE(TERRAINGRID_SET_PARAMS,0xb1228141e9108e8b, CommandTerrainGridSetParams);
	SCR_REGISTER_SECURE(TERRAINGRID_SET_COLOURS,0xf48c2023a8ade554, CommandTerrainGridSetColours);
	
	// --- Animated PostFX ------------------------------------------------------------------------
	SCR_REGISTER_SECURE(ANIMPOSTFX_PLAY,0x9dcf157443ea30d6, CommandPlayAnimatedPostFx);
	SCR_REGISTER_SECURE(ANIMPOSTFX_STOP,0x06a78ba0b756c754, CommandStopAnimatedPostFx);
	SCR_REGISTER_SECURE(ANIMPOSTFX_GET_CURRENT_TIME,0x1d6ea79d4857d04a, CommandGetAnimatedPostFxCurrentTime);
	SCR_REGISTER_SECURE(ANIMPOSTFX_IS_RUNNING,0x57ba7b498f91c8c8, CommandIsAnimatedPostFxRunning);

	SCR_REGISTER_UNUSED(ANIMPOSTFX_START_CROSSFADE,0x994fc080f104bdb0, CommandStartAnimatedPostFxCrossfade);
	SCR_REGISTER_UNUSED(ANIMPOSTFX_STOP_CROSSFADE,0xa3653121f541074f, CommandStopAnimatedPostFxCrossfade);

	SCR_REGISTER_SECURE(ANIMPOSTFX_STOP_ALL,0xde903ac1b5bbc358, CommandStopAllAnimatedPostFx);

	SCR_REGISTER_SECURE(ANIMPOSTFX_STOP_AND_FLUSH_REQUESTS,0xeae90b95f698fa2d, CommandStopAnimatedPostFxAndCancelStartRequest);

	SCR_REGISTER_UNUSED(ANIMPOSTFX_ADD_LISTENER,0x4ee64f540c89eae1, CommandAnimatedPostFxAddListener);
	SCR_REGISTER_UNUSED(ANIMPOSTFX_REMOVE_LISTENER,0x7da3eab1f96b3aae, CommandAnimatedPostFxRemoveListener);
	SCR_REGISTER_UNUSED(ANIMPOSTFX_HAS_EVENT_TRIGGERED,0x3c80bff96d0c0d36, CommandAnimatedPostFxHasEventTriggered);

}
}	//	end of namespace graphics_commands

