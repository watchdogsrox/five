///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWater.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	03 Jan 2007
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxWater.h"

// rage
#include "physics/simulator.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Audio/ambience/ambientaudioentity.h"
#include "Audio/boataudioentity.h"
#include "Camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "Camera/Cinematic/Camera/Mounted/CinematicMountedCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Control/Replay/Replay.h"
#include "Control/Replay/Effects/ParticleWaterFxPacket.h"
#include "Core/Game.h"
#include "Game/ModelIndices.h"
#include "Objects/Object.h"
#include "Peds/Ped.h"
#include "Physics/GtaInst.h"
#include "Renderer/Lights/Lights.h"
#include "Renderer/River.h"
#include "Renderer/Water.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/ShaderLib.h"
#include "system/controlMgr.h"
#include "fwsys/timer.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Submarine.h"
#include "Vfx/Metadata/VfxPedInfo.h"
#include "Vfx/Metadata/VfxVehicleInfo.h"
#include "Vfx/Misc/WaterCannon.h"
#include "Vfx/Particles/PtFxDefines.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
	
CVfxWater			g_vfxWater;

// rag tweakable settings
dev_float			VFXWATER_OBJ_SPLASH_SIZE_EVO_MAX				= 1.0f;

dev_bool			VFXWATER_OBJ_SPLASH_IN_ENABLED					= true;
dev_float			VFXWATER_OBJ_SPLASH_IN_RANGE					= 30.0f;
dev_float			VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_THRESH	= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_IN_SPEED_LATERAL_EVO_MIN	= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_IN_SPEED_LATERAL_EVO_MAX	= 20.0f;
dev_float			VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_EVO_MIN	= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_EVO_MAX	= 20.0f;

dev_bool			VFXWATER_OBJ_SPLASH_OUT_ENABLED					= false;
dev_float			VFXWATER_OBJ_SPLASH_OUT_RANGE					= 30.0f;
dev_float			VFXWATER_OBJ_SPLASH_OUT_SPEED_LATERAL_EVO_MIN	= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_OUT_SPEED_LATERAL_EVO_MAX	= 20.0f;
dev_float			VFXWATER_OBJ_SPLASH_OUT_SPEED_UPWARD_EVO_MIN	= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_OUT_SPEED_UPWARD_EVO_MAX	= 20.0f;

dev_bool			VFXWATER_OBJ_SPLASH_WADE_ENABLED				= false;
dev_float			VFXWATER_OBJ_SPLASH_WADE_RANGE					= 30.0f;
dev_float			VFXWATER_OBJ_SPLASH_WADE_SPEED_OBJECT_EVO_MIN	= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_WADE_SPEED_OBJECT_EVO_MAX	= 20.0f;
dev_float			VFXWATER_OBJ_SPLASH_WADE_SPEED_RIVER_EVO_MIN	= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_WADE_SPEED_RIVER_EVO_MAX	= 20.0f;

dev_bool			VFXWATER_OBJ_SPLASH_TRAIL_ENABLED				= true;
dev_float			VFXWATER_OBJ_SPLASH_TRAIL_RANGE					= 30.0f;
dev_float			VFXWATER_OBJ_SPLASH_TRAIL_SPEED_EVO_MIN			= 1.0f;
dev_float			VFXWATER_OBJ_SPLASH_TRAIL_SPEED_EVO_MAX			= 20.0f;

dev_float			VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP		= 0.0f;

dev_float			VFXWATER_RIVER_VELOCITY_MULTIPLIER_PED			= 100.0f;
dev_float			VFXWATER_RIVER_VELOCITY_MULTIPLIER_VEHICLE		= 100.0f;
dev_float			VFXWATER_RIVER_VELOCITY_MULTIPLIER_OBJECT		= 100.0f;

dev_bool			VFXWATER_ADD_FOAM								= true;
dev_float			VFXWATER_BOAT_FOAM_SPEED_MIN					= 2.0f;
dev_float			VFXWATER_BOAT_FOAM_SPEED_MAX					= 100.0f;
dev_float			VFXWATER_BOAT_FOAM_VAL_MIN						= 0.001f;
dev_float			VFXWATER_BOAT_FOAM_VAL_MAX						= 0.2f;
dev_float			VFXWATER_SUB_FOAM_SPEED_MIN						= 2.0f;
dev_float			VFXWATER_SUB_FOAM_SPEED_MAX						= 25.0f;
dev_float			VFXWATER_SUB_FOAM_VAL_MIN						= 0.250f;
dev_float			VFXWATER_SUB_FOAM_VAL_MAX						= 0.500f;
dev_float           VFXWATER_SUB_DISTANCE_BETWEEN_FOAM				= 200.0f;
dev_float           VFXWATER_SUB_FOAM_PROBABILITY					= 0.450f;
dev_float			VFXWATER_VEHICLE_FOAM_SPEED_MIN					= 2.0f;
dev_float			VFXWATER_VEHICLE_FOAM_SPEED_MAX					= 100.0f;
dev_float			VFXWATER_VEHICLE_FOAM_VAL_MIN					= 0.001f;
dev_float			VFXWATER_VEHICLE_FOAM_VAL_MAX					= 0.2f;



///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxWater
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWater::Init							(unsigned initMode)
{
    if (initMode == INIT_CORE)
	{
		m_waterPtFxOverrideSplashObjectInNameScriptThreadId = THREAD_INVALID;
		m_waterPtFxOverrideSplashObjectInName.SetHash(0);

#if __BANK
	    m_disableSplashPedLod = false;
		m_disableSplashPedEntry = false;
	    m_disableSplashPedIn = false;
	    m_disableSplashPedOut = false;
	    m_disableSplashPedWade = false;
		m_disableSplashPedTrail = false;

		m_disableSplashVehicleIn = false;
		m_disableSplashVehicleOut = false;
		m_disableSplashVehicleWade = false;
		m_disableSplashVehicleTrail = false;

		m_disableSplashObjectIn = false;
		m_disableSplashObjectOut = false;
		m_disableSplashObjectWade = false;
		m_disableSplashObjectTrail = false;

		m_disableBoatBow = false;
		m_disableBoatWash = false;
		m_disableBoatEntry = false;
		m_disableBoatExit = false;
		m_disableBoatProp = false;

	    m_overrideBoatBowScale = 0.0f;
	    m_overrideBoatWashScale = 0.0f;
		m_overrideBoatEntryScale = 0.0f;
		m_overrideBoatExitScale = 0.0f;
	    m_overrideBoatPropScale = 0.0f;

	    m_outputSplashDebug = false;
		m_renderSplashPedDebug = false;
		m_renderSplashVehicleDebug = false;
		m_renderSplashObjectDebug = false;
		m_renderBoatDebug = false;
		m_renderDebugAtSurfacePos = false;
		m_renderFloaterCalculatedSurfacePositions = false;
#endif
    }

	m_boatBowPtFxActive = false;
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWater::Shutdown							(unsigned UNUSED_PARAM(shutdownMode))
{
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWater::Update							(float UNUSED_PARAM(deltaTime))
{
	// test code for placing spheres at the water level in the vfx_test level
	// this highlights issues on 360 where as the player swims around in the water these spheres jump around
#if __BANK
	static bool doVfxTestWaterLevelTest = false;
	if (doVfxTestWaterLevelTest)
	{
		Vec3V vStartPos(-225.0f, -155.0f, 10.0f);
		Vec3V vEndPos(-245.0f, -155.0f, 10.0f);

		for (int i=0; i<10; i++)
		{
			Vec3V vTestPos = Lerp(Vec3VFromF32(i/10.0f), vStartPos, vEndPos);
			float waterZ;
			CVfxHelper::GetWaterZ(vTestPos, waterZ);

			Vec3V vWaterPos(vTestPos.GetXY(), ScalarV(waterZ));
			grcDebugDraw::Sphere(vWaterPos, 0.1f, Color32(1.0f, 1.0f, 0.0f, 1.0f));
		}
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  OverrideWaterPtFxSplashObjectInName
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::OverrideWaterPtFxSplashObjectInName	(atHashWithStringNotFinal hashName, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_waterPtFxOverrideSplashObjectInNameScriptThreadId==THREAD_INVALID || m_waterPtFxOverrideSplashObjectInNameScriptThreadId==scriptThreadId, "trying to override object water splash (in) when this is already in use by another script")) 
	{
		m_waterPtFxOverrideSplashObjectInName = hashName; 
		m_waterPtFxOverrideSplashObjectInNameScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWater::RemoveScript							(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_waterPtFxOverrideSplashObjectInNameScriptThreadId)
	{
		m_waterPtFxOverrideSplashObjectInName.SetHash(0);
		m_waterPtFxOverrideSplashObjectInNameScriptThreadId = THREAD_INVALID;
	}
}



///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////
#if __BANK
void		 	CVfxWater::InitWidgets							()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Vfx Water", false);
#if __DEV
	pVfxBank->AddTitle("SETTINGS");
	pVfxBank->AddSlider("Obj Splash Size Evo Max", &VFXWATER_OBJ_SPLASH_SIZE_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");

	pVfxBank->AddToggle("Obj Splash In Enabled", &VFXWATER_OBJ_SPLASH_IN_ENABLED);
	pVfxBank->AddSlider("Obj Splash In Range", &VFXWATER_OBJ_SPLASH_IN_RANGE, 0.0f, 250.0f, 1.0f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash In Speed Downward Thresh", &VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_THRESH, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash In Speed Lateral Evo Min", &VFXWATER_OBJ_SPLASH_IN_SPEED_LATERAL_EVO_MIN, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash In Speed Lateral Evo Max", &VFXWATER_OBJ_SPLASH_IN_SPEED_LATERAL_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash In Speed Downward Evo Min", &VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_EVO_MIN, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash In Speed Downward Evo Max", &VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");

	pVfxBank->AddToggle("Obj Splash Out Enabled", &VFXWATER_OBJ_SPLASH_OUT_ENABLED);
	pVfxBank->AddSlider("Obj Splash Out Range", &VFXWATER_OBJ_SPLASH_OUT_RANGE, 0.0f, 250.0f, 1.0f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Out Speed Lateral Evo Min", &VFXWATER_OBJ_SPLASH_OUT_SPEED_LATERAL_EVO_MIN, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Out Speed Lateral Evo Max", &VFXWATER_OBJ_SPLASH_OUT_SPEED_LATERAL_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Out Speed Upward Evo Min", &VFXWATER_OBJ_SPLASH_OUT_SPEED_UPWARD_EVO_MIN, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Out Speed Upward Evo Max", &VFXWATER_OBJ_SPLASH_OUT_SPEED_UPWARD_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");

	pVfxBank->AddToggle("Obj Splash Wade Enabled", &VFXWATER_OBJ_SPLASH_WADE_ENABLED);
	pVfxBank->AddSlider("Obj Splash Wade Range", &VFXWATER_OBJ_SPLASH_WADE_RANGE, 0.0f, 250.0f, 1.0f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Wade Speed Object Evo Min", &VFXWATER_OBJ_SPLASH_WADE_SPEED_OBJECT_EVO_MIN, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Wade Speed Object Evo Max", &VFXWATER_OBJ_SPLASH_WADE_SPEED_OBJECT_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Wade Speed River Evo Min", &VFXWATER_OBJ_SPLASH_WADE_SPEED_RIVER_EVO_MIN, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Wade Speed River Evo Max", &VFXWATER_OBJ_SPLASH_WADE_SPEED_RIVER_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");

	pVfxBank->AddToggle("Obj Splash Trail Enabled", &VFXWATER_OBJ_SPLASH_TRAIL_ENABLED);
	pVfxBank->AddSlider("Obj Splash Trail Range", &VFXWATER_OBJ_SPLASH_TRAIL_RANGE, 0.0f, 250.0f, 1.0f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Trail Speed Evo Min", &VFXWATER_OBJ_SPLASH_TRAIL_SPEED_EVO_MIN, 0.0f, 50.0f, 0.1f, NullCB, "");
	pVfxBank->AddSlider("Obj Splash Trail Speed Evo Max", &VFXWATER_OBJ_SPLASH_TRAIL_SPEED_EVO_MAX, 0.0f, 50.0f, 0.1f, NullCB, "");

	pVfxBank->AddSlider("Veh Splash Wade FrontBack Overlap", &VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP, 0.0f, 1.0f, 0.01f, NullCB, "");

	pVfxBank->AddSlider("River Velocity Multiplier Ped", &VFXWATER_RIVER_VELOCITY_MULTIPLIER_PED, 0.0f, 500.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("River Velocity Multiplier Vehicle", &VFXWATER_RIVER_VELOCITY_MULTIPLIER_VEHICLE, 0.0f, 500.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("River Velocity Multiplier Object", &VFXWATER_RIVER_VELOCITY_MULTIPLIER_OBJECT, 0.0f, 500.0f, 0.5f, NullCB, "");

	pVfxBank->AddToggle("Add Foam", &VFXWATER_ADD_FOAM);
	pVfxBank->AddSlider("Boat Foam Speed Min", &VFXWATER_BOAT_FOAM_SPEED_MIN, 0.0f, 100.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Boat Foam Speed Max", &VFXWATER_BOAT_FOAM_SPEED_MAX, 0.0f, 100.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Boat Foam Value Min", &VFXWATER_BOAT_FOAM_VAL_MIN, 0.0f, 10.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Boat Foam Value Max", &VFXWATER_BOAT_FOAM_VAL_MAX, 0.0f, 10.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Submarine Foam Speed Min", &VFXWATER_SUB_FOAM_SPEED_MIN, 0.0f, 100.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Submarine Foam Speed Max", &VFXWATER_SUB_FOAM_SPEED_MAX, 0.0f, 100.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Submarine Foam Value Min", &VFXWATER_SUB_FOAM_VAL_MIN, 0.0f, 10.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Submarine Foam Value Max", &VFXWATER_SUB_FOAM_VAL_MAX, 0.0f, 10.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Submarine Foam Distance in between", &VFXWATER_SUB_DISTANCE_BETWEEN_FOAM, 0.0f, 200.0f, 1.0f, NullCB, ""); 
	pVfxBank->AddSlider("Submarine Foam Probability", &VFXWATER_SUB_FOAM_PROBABILITY, 0.0f, 1.0f, 0.01f, NullCB, "");
	pVfxBank->AddSlider("Vehicle Foam Speed Min", &VFXWATER_VEHICLE_FOAM_SPEED_MIN, 0.0f, 100.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Vehicle Foam Speed Max", &VFXWATER_VEHICLE_FOAM_SPEED_MAX, 0.0f, 100.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Vehicle Foam Value Min", &VFXWATER_VEHICLE_FOAM_VAL_MIN, 0.0f, 10.0f, 0.5f, NullCB, "");
	pVfxBank->AddSlider("Vehicle Foam Value Max", &VFXWATER_VEHICLE_FOAM_VAL_MAX, 0.0f, 10.0f, 0.5f, NullCB, "");
	
#endif

	pVfxBank->AddTitle("");
	pVfxBank->AddTitle("DEBUG");
	pVfxBank->AddToggle("Disable Splash Ped Lod", &m_disableSplashPedLod);
	pVfxBank->AddToggle("Disable Splash Ped Entry", &m_disableSplashPedEntry);
	pVfxBank->AddToggle("Disable Splash Ped In", &m_disableSplashPedIn);
	pVfxBank->AddToggle("Disable Splash Ped Out", &m_disableSplashPedOut);
	pVfxBank->AddToggle("Disable Splash Ped Wade", &m_disableSplashPedWade);
	pVfxBank->AddToggle("Disable Splash Ped Trail", &m_disableSplashPedTrail);

	pVfxBank->AddToggle("Disable Splash Vehicle In", &m_disableSplashVehicleIn);
	pVfxBank->AddToggle("Disable Splash Vehicle Out", &m_disableSplashVehicleOut);
	pVfxBank->AddToggle("Disable Splash Vehicle Wade", &m_disableSplashVehicleWade);
	pVfxBank->AddToggle("Disable Splash Vehicle Trail", &m_disableSplashVehicleTrail);

	pVfxBank->AddToggle("Disable Splash Object In", &m_disableSplashObjectIn);
	pVfxBank->AddToggle("Disable Splash Object Out", &m_disableSplashObjectOut);
	pVfxBank->AddToggle("Disable Splash Object Wade", &m_disableSplashObjectWade);
	pVfxBank->AddToggle("Disable Splash Object Trail", &m_disableSplashObjectTrail);
	
	pVfxBank->AddToggle("Disable Boat Bow", &m_disableBoatBow);
	pVfxBank->AddToggle("Disable Boat Wash", &m_disableBoatWash);
	pVfxBank->AddToggle("Disable Boat Entry", &m_disableBoatEntry);
	pVfxBank->AddToggle("Disable Boat Exit", &m_disableBoatExit);
	pVfxBank->AddToggle("Disable Boat Prop", &m_disableBoatProp);
	
	pVfxBank->AddSlider("Override Boat Bow Scale", &m_overrideBoatBowScale, 0.0f, 10.0f, 0.1f);
	pVfxBank->AddSlider("Override Boat Wash Scale", &m_overrideBoatWashScale, 0.0f, 10.0f, 0.1f);
	pVfxBank->AddSlider("Override Boat Entry Scale", &m_overrideBoatEntryScale, 0.0f, 10.0f, 0.1f);
	pVfxBank->AddSlider("Override Boat Exit Scale", &m_overrideBoatExitScale, 0.0f, 10.0f, 0.1f);
	pVfxBank->AddSlider("Override Boat Prop Scale", &m_overrideBoatPropScale, 0.0f, 10.0f, 0.1f);

	pVfxBank->AddToggle("Output Splash Debug", &m_outputSplashDebug);
	pVfxBank->AddToggle("Render Splash Ped Debug", &m_renderSplashPedDebug);
	pVfxBank->AddToggle("Render Splash Vehicle Debug", &m_renderSplashVehicleDebug);
	pVfxBank->AddToggle("Render Splash Object Debug", &m_renderSplashObjectDebug);
	pVfxBank->AddToggle("Render Boat Debug", &m_renderBoatDebug);
	pVfxBank->AddToggle("Render Debug At Surface Pos", &m_renderDebugAtSurfacePos);
	pVfxBank->AddToggle("Render Floater Calculated Surface Positions", &m_renderFloaterCalculatedSurfacePositions);
	
	pVfxBank->PopGroup();
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxWater
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxWater					(CEntity* pParent, const VfxWaterSampleData_s* pVfxWaterSampleData, bool hasAnimatedPedSamples, s32 numInSampleLevelArray)
{
	// clean up the ped audio flags
	if(pParent && pParent->GetIsTypePed())
	{
		CPed* ped = static_cast<CPed*>(pParent);
		if(ped->GetPedAudioEntity())
		{
			ped->GetPedAudioEntity()->GetFootStepAudio().CleanWaterFlags();
		}
	}

	// do early returns for off screen entities (unless this is a boat or shark)
	const bool isVehicle = pParent->GetIsTypeVehicle();
	bool isBoat = isVehicle &&
		(((CVehicle*)pParent)->GetVehicleType()==VEHICLE_TYPE_BOAT ||
		 ((CVehicle*)pParent)->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ||
		 ((CVehicle*)pParent)->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE);

	bool isSubmarine = isVehicle && ((CVehicle*)pParent)->GetVehicleType() == VEHICLE_TYPE_SUBMARINE;

	bool isShark = pParent->GetIsTypePed() && ((CPed*)pParent)->GetPedModelInfo()->GetHashKey() == ATSTRINGHASH("A_C_SHARKTIGER", 0x6C3F072);

	if (!isBoat && !isShark)
	{
		// don't process if the entity is off screen
		if (pParent->GetIsOnScreen()==false)
		{
			return;
		}
	}

	// check if this is a player effect
	bool isPlayerVfx = false;
	CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	if (pPlayerInfo)
	{
		if (pParent->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pParent);
			if (pPlayerInfo->GetPlayerPed()==pPed)
			{
				isPlayerVfx = true;
			}
		}
		else if (pParent->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pParent);
			if (pPlayerInfo->GetPlayerPed() && pPlayerInfo->GetPlayerPed()->GetVehiclePedInside()==pVehicle)
			{
				isPlayerVfx = true;
			}
		}
	}

	// check that the splash is in range to continue
	float fxDistSqr = CVfxHelper::GetDistSqrToCamera(pParent->GetTransform().GetPosition());
	if (!isPlayerVfx)
	{
		if (isBoat)
		{
			if (fxDistSqr>VFXRANGE_WATER_SPLASH_BOAT_MAX_SQR)
			{
				return;
			}
		}
		else if (isSubmarine)
		{
			if (fxDistSqr>VFXRANGE_WATER_SPLASH_SUB_MAX_SQR)
			{
				return;
			}
		}
		else
		{
			if (fxDistSqr>VFXRANGE_WATER_SPLASH_GEN_MAX_SQR)
			{
				return;
			}
		}
	}

	// check if we need to return early
	if (pParent->GetIsTypePed())
	{
		// get the ped pointer
		CPed* pPed = static_cast<CPed*>(pParent);

		// don't process if the ped is dead
// 		if (pPed->IsDead())
// 		{
// 			return;
// 		}

		// get the vfx ped info
		CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);

		// don't process if no vfx ped info is set up
		if (pVfxPedInfo==NULL)
		{
			return;
		}

		// don't process if splash vfx aren't enabled
		if (pVfxPedInfo->GetSplashVfxEnabled()==false)
		{
			return;
		}

		// don't process if out of range (unless this is the player)
		if (!isPlayerVfx && fxDistSqr>pVfxPedInfo->GetSplashVfxRangeSqr())
		{
			return;
		}
	}
	else if (pParent->GetIsTypeObject())
	{
		// don't process if this is a procedural object
		if (pParent->GetIsAnyManagedProcFlagSet())
		{
			return;
		}

		// don't process if out of range
		float range = 0.0f;
		if (VFXWATER_OBJ_SPLASH_IN_ENABLED)		range = Max(range, VFXWATER_OBJ_SPLASH_IN_RANGE);
		if (VFXWATER_OBJ_SPLASH_OUT_ENABLED)	range = Max(range, VFXWATER_OBJ_SPLASH_OUT_RANGE);
		if (VFXWATER_OBJ_SPLASH_WADE_ENABLED)	range = Max(range, VFXWATER_OBJ_SPLASH_WADE_RANGE);
		if (VFXWATER_OBJ_SPLASH_TRAIL_ENABLED)	range = Max(range, VFXWATER_OBJ_SPLASH_TRAIL_RANGE);

		if (fxDistSqr>range*range)
		{
			return;
		}
	}

	// process boat specific effects
	if (isBoat)
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(pParent);
		if (pVehicle->GetStatus()!=STATUS_WRECKED)
		{
			//CBoat* pVehicle = dynamic_cast<CBoat*>(pParent);
			ProcessVfxBoat(pVehicle, pVfxWaterSampleData);
		}
	}
	// check if this is data from an animated ped (i.e. a non-ragdoll ped)
	else if (hasAnimatedPedSamples)
	{
		// we do - process these
		CPed* pPed = static_cast<CPed*>(pParent);
		CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
		if (pVfxPedInfo)
		{
			for (s32 i=0; i<pVfxPedInfo->GetPedSkeletonBoneNumInfos(); i++)
			{
				const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
				const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();
				if (pBoneWaterInfo && pBoneWaterInfo->m_sampleSize>0.0f)
				{
					s32 currSampleIndex = numInSampleLevelArray-1-i;
					if (pVfxWaterSampleData[currSampleIndex].isInWater)
					{
						ProcessVfxSplash(pParent, i, &pVfxWaterSampleData[currSampleIndex], fxDistSqr, isPlayerVfx);
					}
				}
			}
		}
	}
	else // general case
	{
		// process individual splash effects
		for (s32 i=0; i<numInSampleLevelArray; i++)
		{
			if (pVfxWaterSampleData[i].isInWater)
			{
				ProcessVfxSplash(pParent, i, &pVfxWaterSampleData[i], fxDistSqr, isPlayerVfx);
			}
		}

		// adding some hacked in foam where the current generic system doesn't do enough
		if (isVehicle)
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pParent);
			const bool isKosatka = (MI_SUB_KOSATKA.IsValid() && pVehicle->GetModelIndex() == MI_SUB_KOSATKA);
			if (isKosatka)
			{
				// we copy the data from the actual samples into local arrays
				bool isAnySampleNotInWater = false;
				Vec3V* positions		= Alloca(Vec3V, numInSampleLevelArray + 2);
				float* partiallyInWater = Alloca(float, numInSampleLevelArray + 2);
				for (int i = 0; i < numInSampleLevelArray; ++i)
				{
					bool partiallyInWaterSample = pVfxWaterSampleData[i].currLevel > 0.0f && pVfxWaterSampleData[i].currLevel < pVfxWaterSampleData[i].maxLevel;
					positions[i] = pVfxWaterSampleData[i].vSurfacePos;
					partiallyInWater[i] = partiallyInWaterSample ? 1.0f : 0.0f;

					if (!pVfxWaterSampleData[i].isInWater)
					{
						isAnySampleNotInWater = true;
					}
				}

				// if one of the samples isn't in water we can't rely on its vSurfacePos having a valid position
				// this will cause potential issues with the code below where 'distance' and therefore 'subdivisions' can be massive
				// this leads to a massive for loop that hangs the game (see url:bugstar:6851917)
				if (!isAnySampleNotInWater)
				{
					// since the samples don't cover the back of the submarine we create two extra artificial samples locations based on the
					// back two rows of samples we already have. For this we interpolate their bits of data, hoping we get something that makes sense.
					vfxAssert(numInSampleLevelArray == 6);
					const float extraScaleForArtificialSamples = 0.8f;
					positions[numInSampleLevelArray]     = positions[0] + (positions[0] - positions[1]) * ScalarV(extraScaleForArtificialSamples);
					positions[numInSampleLevelArray + 1] = positions[3] + (positions[3] - positions[4]) * ScalarV(extraScaleForArtificialSamples);
					partiallyInWater[numInSampleLevelArray]     = Clamp(partiallyInWater[0] + (partiallyInWater[0] - partiallyInWater[1]) * extraScaleForArtificialSamples, 0.0f, 1.0f);
					partiallyInWater[numInSampleLevelArray + 1] = Clamp(partiallyInWater[3] + (partiallyInWater[3] - partiallyInWater[4]) * extraScaleForArtificialSamples, 0.0f, 1.0f);

					// we define a polyline which we'll use to add foam in, the indices will define the local samples to use, there will be an extra
					// line created between the last index and the first one to create what hopefully looks like the full circle.
					int sampleIndices[] = { 0,1,2,5,4,3,7,6 };
					int samplesCount = 8;
					vfxAssert(samplesCount == numInSampleLevelArray + 2);

					// for each of our samples, we see how far they are apart and how many subsamples (subdivisions) we're going to use to fill up
					// the space.
					float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), VFXWATER_SUB_FOAM_SPEED_MIN, VFXWATER_SUB_FOAM_SPEED_MAX);
					for (int index = 0; index < samplesCount; ++index)
					{
						// get both of our samples that define current line
						// and their values that we'll interpolate between
						int sampleIndexA = sampleIndices[index];
						int sampleIndexB = sampleIndices[(index+1)%samplesCount];
						Vec3V waterPosA = positions[sampleIndexA];
						Vec3V waterPosB = positions[sampleIndexB];
						float partiallyInWaterA = partiallyInWater[sampleIndexA];
						float partiallyInWaterB = partiallyInWater[sampleIndexB];

						// calculate the length of the line and see how many subdivisions we
						// want to split it to get a good coverage
						float distance = Mag(waterPosB - waterPosA).Getf();
						int subdivisions = int(ceil(distance / VFXWATER_SUB_DISTANCE_BETWEEN_FOAM));
						float normalizedSectionSize = 1.0f / subdivisions;

						// for each subsample we offset it along it's segment forwards or backwards so we add foam at different positions
						// every frame to break up the dotted patterns and get a more consistent "line looking" bound.
						for (int i = 0; i < subdivisions; ++i)
						{
							float interp = float(i) * normalizedSectionSize + (normalizedSectionSize * 0.5f);
							interp += ((g_DrawRand.GetFloat() - 0.5f) * normalizedSectionSize);
							float partiallyInWater = Lerp(interp, partiallyInWaterA, partiallyInWaterB);
							float foamVal = VFXWATER_SUB_FOAM_VAL_MIN + (speedEvo*(VFXWATER_SUB_FOAM_VAL_MAX - VFXWATER_SUB_FOAM_VAL_MIN));
							foamVal *= partiallyInWater;
							if (foamVal > 0.0f && (VFXWATER_SUB_FOAM_PROBABILITY >= 1.0f || g_DrawRand.GetFloat() <= VFXWATER_SUB_FOAM_PROBABILITY))
							{
								Vec3V waterPos = Lerp(ScalarV(interp), waterPosA, waterPosB);
								Water::AddFoam(waterPos.GetXf(), waterPos.GetYf(), foamVal);
							}
						}
					}
				}
			}
		}
	}

	if (pParent->GetIsTypeVehicle())
	{
		(((CVehicle*)pParent)->GetVehicleAudioEntity())->UpdateWaterState(pVfxWaterSampleData);
	}	
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxSplash
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxSplash					(CEntity* pParent, s32 id, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool isPlayerVfx)
{
	// process peds
	if (pParent->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pParent);
		ProcessVfxSplashPed(pPed, id, pVfxWaterSampleData, fxDistSqr, isPlayerVfx);
	}
	// process vehicles
	else if (pParent->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pParent);
		ProcessVfxSplashVehicle(pVehicle, id, pVfxWaterSampleData, fxDistSqr, isPlayerVfx);	
	}
	// process objects etc
	else if (pParent->GetIsTypeObject())
	{
		CObject* pObj = static_cast<CObject*>(pParent);
		ProcessVfxSplashObject(pObj, id, pVfxWaterSampleData, fxDistSqr, isPlayerVfx);	
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxSplashPed
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxSplashPed				(CPed* pPed, s32 id, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool isPlayerVfx)
{
	// init the sample state flags
	bool goneIntoWater = false;
	bool comeOutOfWater = false;
	bool partiallyInWater = false;
	bool fullyInWater = false;

	// only play fully in water vfx (trails) if the camera is underwater (or this is the player)
	float gameCamWaterDepth = CVfxHelper::GetGameCamWaterDepth();
	if (gameCamWaterDepth>0.0f || isPlayerVfx)
	{
		fullyInWater = pVfxWaterSampleData->currLevel>=pVfxWaterSampleData->maxLevel;
	}

	// play ins and outs if the camera is above or close to the water surface
	if (gameCamWaterDepth<1.0f)
	{
		goneIntoWater = pVfxWaterSampleData->prevLevel<(pVfxWaterSampleData->maxLevel*0.5f) && pVfxWaterSampleData->currLevel>=(pVfxWaterSampleData->maxLevel*0.5f);
		comeOutOfWater = pVfxWaterSampleData->prevLevel>=(pVfxWaterSampleData->maxLevel*0.5f) && pVfxWaterSampleData->currLevel<(pVfxWaterSampleData->maxLevel*0.5f);
	}
			
	// play wades all the time
	partiallyInWater = pVfxWaterSampleData->currLevel>0.0f && pVfxWaterSampleData->currLevel<pVfxWaterSampleData->maxLevel;

	if (goneIntoWater || comeOutOfWater || partiallyInWater || fullyInWater)
	{
		// get the bone tag and bone index
		// if the water sample has a valid bone tag then this has come from an animated ped interacting with water
		s32 boneIndex = -1;
		eAnimBoneTag boneTag = BONETAG_INVALID;
		if (pVfxWaterSampleData->boneTag!=BONETAG_INVALID)
		{
			boneTag = pVfxWaterSampleData->boneTag;
			boneIndex = pPed->GetBoneIndexFromBoneTag(boneTag);
		}
		else
		{
			boneIndex = pPed->GetBoneIndexFromRagdollComponent(pVfxWaterSampleData->componentId);
			if (boneIndex!=-1)
			{
				boneTag = pPed->GetBoneTagFromBoneIndex(boneIndex);
			}
		}

		if (boneIndex==-1 || boneTag==BONETAG_INVALID)
		{
			vfxAssertf(boneIndex!=-1, "invalid bone index calculated");
			vfxAssertf(boneTag!=BONETAG_INVALID, "invalid bone tag calculated");
			return;
		}

		Mat34V pedBoneMtx;
		CVfxHelper::GetMatrixFromBoneIndex(pedBoneMtx, pPed, boneIndex);

		// calc component velocity
		Vec3V vComponentVel(V_ZERO);
		if (pPed->GetRagdollInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE)
		{
			// get the component id of this bone
			// NOTE - a ped animated water sample comes from a bone so may not have a corresponding component id
			//        in these cases the component id will come through as zero
			s32 componentId = pVfxWaterSampleData->componentId;

			// get the composite bound and check the composite id validity
			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pPed->GetRagdollInst()->GetArchetype()->GetBound());
			Assertf(componentId > -1 && componentId < pBoundComp->GetNumBounds(),"invalid component id in the FX data");

			// get the current and last component positions
			Vec3V vCurrComponentPos = pBoundComp->GetCurrentMatrix(componentId).GetCol3();
			Vec3V vPrevComponentPos = pBoundComp->GetLastMatrix(componentId).GetCol3();

			// transform these into world space
			Mat34V_ConstRef currRagdollMtx = pPed->GetRagdollInst()->GetMatrix();
			vCurrComponentPos = Transform(currRagdollMtx, vCurrComponentPos);
			Mat34V_ConstRef prevRagdollMtx = PHSIM->GetLastInstanceMatrix(pPed->GetRagdollInst());
			vPrevComponentPos = Transform(prevRagdollMtx, vPrevComponentPos);

			// calculate the velocity of this component
			vComponentVel = (vCurrComponentPos - vPrevComponentPos) * ScalarVFromF32(fwTimer::GetInvTimeStep());

			//grcDebugDraw::Line(vPrevComponentPos, vCurrComponentPos, Color32(1.0f, 0.0f, 0.0f, 1.0f));
		}
		else
		{
			Assertf(pPed->GetRagdollInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE, "Unexpected ragdoll bound type");
		}

		// calc bone speed and direction
		float boneSpeed = Mag(vComponentVel).Getf(); 
		Vec3V vBoneDir(V_X_AXIS_WZERO);
		if (boneSpeed>0.0f)
		{
			vBoneDir = Normalize(vComponentVel);
		}

		// deal with wet feet
		CPedVfx* pPedVfx = pPed->GetPedVfx();
		if (pPedVfx)
		{
			if (boneTag==BONETAG_L_FOOT)
			{
				pPedVfx->SetFootWetnessInfo(true, VFXLIQUID_TYPE_WATER);
			}
			else if (boneTag==BONETAG_R_FOOT)
			{
				pPedVfx->SetFootWetnessInfo(false, VFXLIQUID_TYPE_WATER);
			}
			if(pPed->GetPedAudioEntity())
			{
				pPed->GetPedAudioEntity()->GetFootStepAudio().UpdateWetFootInfo(boneTag);
			}
		}

		// get the river flow at this position
		Vector2 riverFlow(0.0f, 0.0f);
		Vec3V vBonePos = pedBoneMtx.GetCol3();
		River::GetRiverFlowFromPosition(vBonePos, riverFlow);
		Vec3V vRiverVel = Vec3V(VFXWATER_RIVER_VELOCITY_MULTIPLIER_PED*riverFlow.x, VFXWATER_RIVER_VELOCITY_MULTIPLIER_PED*riverFlow.y, 0.0f);

		// get the vfx ped info
		CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
		if (pVfxPedInfo==NULL)
		{
			return;
		}

		// get the corresponding skeleton bone info for this water sample
		// if a valid bone tag specified in the water sample the index is id as this has come from an animated ped interacting with water
		// the correct data is at id
		int skeletonBoneInfoIdx = -1;
		const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = NULL;
		if (pVfxWaterSampleData->boneTag==BONETAG_INVALID)
		{
			// no bone tag specified in the water sample - search for the correct data
			for (s32 i=0; i<pVfxPedInfo->GetPedSkeletonBoneNumInfos(); i++)
			{
				pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
				if (boneTag==pSkeletonBoneInfo->m_boneTagA)
				{
					skeletonBoneInfoIdx = i;
					break;
				}

				pSkeletonBoneInfo = NULL;
			}
		}
		else
		{
			skeletonBoneInfoIdx = id;
			pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(skeletonBoneInfoIdx);
		}

		if (pSkeletonBoneInfo)
		{
			const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();

			if (pBoneWaterInfo && pBoneWaterInfo->m_sampleSize>0.0f)
			{
				// process the splash
				if (boneTag==pSkeletonBoneInfo->m_boneTagA)
				{
					if (isPlayerVfx || !pBoneWaterInfo->m_playerOnlyPtFx)
					{
#if __DEV
						static int outputBoneInfoIdx = -1;
						bool outputBoneInfo = outputBoneInfoIdx>-1 && skeletonBoneInfoIdx==outputBoneInfoIdx;
						if (outputBoneInfo)
						{
							if (goneIntoWater)				vfxDebugf1("Ped Water Bone Info - In");
							else if (comeOutOfWater)		vfxDebugf1("Ped Water Bone Info - Out");
							else if (partiallyInWater)		vfxDebugf1("Ped Water Bone Info - Wade");
							else if (fullyInWater)			vfxDebugf1("Ped Water Bone Info - Trail");
						}
#endif

						if (goneIntoWater)
						{
							if (pPedVfx)
							{
								pPedVfx->SetOutOfWaterBoneTimer(skeletonBoneInfoIdx, pVfxPedInfo->GetWaterDripPtFxTimer());
							}

							if (pBoneWaterInfo->m_splashInPtFxEnabled && boneSpeed>=pVfxPedInfo->GetSplashInPtFxSpeedThresh() && fxDistSqr<=pBoneWaterInfo->m_splashInPtFxRange*pBoneWaterInfo->m_splashInPtFxRange)
							{
								g_vfxWater.TriggerPtFxSplashPedIn(pPed, pVfxPedInfo, pSkeletonBoneInfo, pBoneWaterInfo, pVfxWaterSampleData, boneIndex, vBoneDir, boneSpeed, isPlayerVfx);

								if (pPed->GetPedAudioEntity())
								{
									//pPed->GetPedAudioEntity()->GetFootStepAudio().TriggerWaveImpact();
								}
							}
						}
						else if (comeOutOfWater)
						{
							if (pBoneWaterInfo->m_splashOutPtFxEnabled && fxDistSqr<=pBoneWaterInfo->m_splashOutPtFxRange*pBoneWaterInfo->m_splashOutPtFxRange)
							{
								g_vfxWater.TriggerPtFxSplashPedOut(pPed, pVfxPedInfo, pSkeletonBoneInfo, boneIndex, vBoneDir, boneSpeed);

								if (pPed->GetPedAudioEntity())
								{
									//pPed->GetPedAudioEntity()->GetFootStepAudio().TriggerWaveImpact();
								}
							}
						}
						else if (partiallyInWater)
						{
							if (pPedVfx)
							{
								pPedVfx->SetOutOfWaterBoneTimer(skeletonBoneInfoIdx, pVfxPedInfo->GetWaterDripPtFxTimer());
							}

							if (pBoneWaterInfo->m_splashWadePtFxEnabled && fxDistSqr<=pBoneWaterInfo->m_splashWadePtFxRange*pBoneWaterInfo->m_splashWadePtFxRange)//&& !pPed->GetIsCrouching())
							{
								g_vfxWater.UpdatePtFxSplashPedWade(pPed, pVfxPedInfo, pSkeletonBoneInfo, pBoneWaterInfo, pVfxWaterSampleData, vRiverVel, vBoneDir, boneSpeed, skeletonBoneInfoIdx);
								Vec3V vDir = vRiverVel;
								vDir.SetZ(ScalarV(V_ZERO));
								float speed = Mag(vDir).Getf();
								vDir = NormalizeSafe(vDir, Vec3V(V_X_AXIS_WZERO));
								// return if not enough speed through the water
								if ((speed >= pVfxPedInfo->GetSplashWadePtFxSpeedEvoMin()) && !fullyInWater && pPed->GetPedAudioEntity())
								{
									pPed->GetPedAudioEntity()->GetFootStepAudio().TriggerWaveImpact();
								}
							}
						}
						else if (fullyInWater)
						{	
							if (pPedVfx)
							{
								pPedVfx->SetOutOfWaterBoneTimer(skeletonBoneInfoIdx, pVfxPedInfo->GetWaterDripPtFxTimer());
							}

							if (pBoneWaterInfo->m_splashTrailPtFxEnabled && fxDistSqr<=pBoneWaterInfo->m_splashTrailPtFxRange*pBoneWaterInfo->m_splashTrailPtFxRange)
							{
								g_vfxWater.UpdatePtFxSplashPedTrail(pPed, pVfxPedInfo, pSkeletonBoneInfo, pBoneWaterInfo, pVfxWaterSampleData, vRiverVel, vBoneDir, boneSpeed, skeletonBoneInfoIdx);
							}
						}
					}
				}
			}
		}
	}

	// render debug
#if __BANK
	if (m_renderSplashPedDebug)
	{
		RenderDebugSplashes(pVfxWaterSampleData, goneIntoWater, comeOutOfWater, partiallyInWater, fullyInWater);
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxSplashVehicle
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxSplashVehicle			(CVehicle* pVehicle, s32 id, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool isPlayerVfx)
{
	Assert(pVehicle->GetVehicleType()!=VEHICLE_TYPE_BOAT);

	// init the sample state flags
	bool goneIntoWater = false;
	bool comeOutOfWater = false;
	bool partiallyInWater = false;
	bool fullyInWater = false;

	// only play fully in water vfx (trails) if the camera is underwater
	float gameCamWaterDepth = CVfxHelper::GetGameCamWaterDepth();
	if (gameCamWaterDepth>0.0f || isPlayerVfx)
	{
		fullyInWater = pVfxWaterSampleData->currLevel>=pVfxWaterSampleData->maxLevel;
	}

	goneIntoWater = pVfxWaterSampleData->prevLevel==0.0f && pVfxWaterSampleData->currLevel>0.0f;
	comeOutOfWater = pVfxWaterSampleData->prevLevel>=pVfxWaterSampleData->maxLevel && pVfxWaterSampleData->currLevel<pVfxWaterSampleData->maxLevel;
	partiallyInWater = pVfxWaterSampleData->currLevel>0.0f && pVfxWaterSampleData->currLevel<pVfxWaterSampleData->maxLevel;

	if (goneIntoWater || comeOutOfWater || partiallyInWater || fullyInWater)
	{
		// get the downward speed of the vehicle
		float downwardSpeed = Max(0.0f, -pVehicle->GetVelocity().z);
		float upwardSpeed = Max(0.0f, pVehicle->GetVelocity().z);

		// get the lateral speed of the vehicle
		Vec3V vLateralVel = RCC_VEC3V(pVehicle->GetVelocity());
		vLateralVel.SetZ(V_ZERO);
		float lateralSpeed = Mag(vLateralVel).Getf();
		Vec3V vLateralDir = NormalizeSafe(vLateralVel, Vec3V(V_X_AXIS_WZERO));

		// get the river flow at this position
		Vector2 riverFlow(0.0f, 0.0f);
		River::GetRiverFlowFromPosition(pVfxWaterSampleData->vTestPos, riverFlow);
		Vec3V vRiverVel = Vec3V(VFXWATER_RIVER_VELOCITY_MULTIPLIER_VEHICLE*riverFlow.x, VFXWATER_RIVER_VELOCITY_MULTIPLIER_VEHICLE*riverFlow.y, 0.0f);

		// get the vfx vehicle info
		CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
		if (pVfxVehicleInfo)
		{
			// play all other water vfx (in/out/wade) if the camera is above or close to the water surface
			if (goneIntoWater)
			{
				if ((gameCamWaterDepth<1.0f) && pVfxVehicleInfo->GetSplashInPtFxEnabled() && downwardSpeed>pVfxVehicleInfo->GetSplashInPtFxSpeedDownwardThresh() && fxDistSqr<=pVfxVehicleInfo->GetSplashInPtFxRangeSqr())					
				{
					g_vfxWater.TriggerPtFxSplashVehicleIn(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vLateralDir, lateralSpeed, downwardSpeed);	
				}
				if ( fxDistSqr<=pVfxVehicleInfo->GetSplashInPtFxRangeSqr())
				{
					pVehicle->GetVehicleAudioEntity()->TriggerSplash(downwardSpeed);
				}
			}
			else if (comeOutOfWater)
			{
				if ((gameCamWaterDepth<1.0f) && pVfxVehicleInfo->GetSplashOutPtFxEnabled() && fxDistSqr<=pVfxVehicleInfo->GetSplashOutPtFxRangeSqr())
				{
					g_vfxWater.TriggerPtFxSplashVehicleOut(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vLateralDir, lateralSpeed, upwardSpeed);
				}
				if ( fxDistSqr<=pVfxVehicleInfo->GetSplashInPtFxRangeSqr())
				{
					pVehicle->GetVehicleAudioEntity()->TriggerSplash(downwardSpeed,true);
				}
			}
			else if (partiallyInWater)
			{
				if ((gameCamWaterDepth<1.0f) && pVfxVehicleInfo->GetSplashWadePtFxEnabled() && fxDistSqr<=pVfxVehicleInfo->GetSplashWadePtFxRangeSqr())
				{
					g_vfxWater.UpdatePtFxSplashVehicleWade(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vRiverVel, vLateralVel, id);
				}
			}
			else if (fullyInWater)
			{
			
				if ((gameCamWaterDepth>0.0f) && pVfxVehicleInfo->GetSplashTrailPtFxEnabled() && fxDistSqr<=pVfxVehicleInfo->GetSplashTrailPtFxRangeSqr())
				{
					g_vfxWater.UpdatePtFxSplashVehicleTrail(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, vRiverVel, vLateralVel, id);
				}
				pVehicle->GetVehicleAudioEntity()->SetVehRainInWaterVolume(g_SilenceVolumeLin);
			}
		}

		if (goneIntoWater || comeOutOfWater || partiallyInWater)
		{
			// water disturbance foam
			if (VFXWATER_ADD_FOAM)
			{
				float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), VFXWATER_VEHICLE_FOAM_SPEED_MIN, VFXWATER_VEHICLE_FOAM_SPEED_MAX);
				if (speedEvo>0.0f)
				{
					float foamVal = VFXWATER_VEHICLE_FOAM_VAL_MIN + (speedEvo*(VFXWATER_VEHICLE_FOAM_VAL_MAX-VFXWATER_VEHICLE_FOAM_VAL_MIN));
					Vec3V waterPos = pVfxWaterSampleData->vSurfacePos;
					Water::AddFoam(waterPos.GetXf(), waterPos.GetYf(), foamVal);
				}
			}
		}
	}	

	// render debug
#if __BANK
	if (m_renderSplashVehicleDebug)
	{
		RenderDebugSplashes(pVfxWaterSampleData, goneIntoWater, comeOutOfWater, partiallyInWater, fullyInWater);
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxSplashObject
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxSplashObject			(CObject* pObject, s32 id, const VfxWaterSampleData_s* pVfxWaterSampleData, float fxDistSqr, bool UNUSED_PARAM(isPlayerVfx))
{
	// init the sample state flags
	bool goneIntoWater = false;
	bool comeOutOfWater = false;
	bool partiallyInWater = false;
	bool fullyInWater = false;

	// only play fully in water vfx (trails) if the camera is underwater
	float gameCamWaterDepth = CVfxHelper::GetGameCamWaterDepth();
	if (gameCamWaterDepth>0.0f)
	{
		fullyInWater = pVfxWaterSampleData->currLevel>=pVfxWaterSampleData->maxLevel;
	}

	// play all other water vfx (in/out/wade) if the camera is above or close to the water surface
	if (gameCamWaterDepth<1.0f)
	{
		goneIntoWater = pVfxWaterSampleData->prevLevel==0.0f && pVfxWaterSampleData->currLevel>0.0f;
		comeOutOfWater = pVfxWaterSampleData->prevLevel>=pVfxWaterSampleData->maxLevel && pVfxWaterSampleData->currLevel<pVfxWaterSampleData->maxLevel;
		partiallyInWater = pVfxWaterSampleData->currLevel>0.0f && pVfxWaterSampleData->currLevel<pVfxWaterSampleData->maxLevel;
	}

	if (goneIntoWater || comeOutOfWater || partiallyInWater || fullyInWater)
	{
		// get the downward speed of the object
		float downwardSpeed = Max(0.0f, -pObject->GetVelocity().z);
		float upwardSpeed = Max(0.0f, pObject->GetVelocity().z);

		// get the lateral speed of the object
		Vec3V vLateralVel = RCC_VEC3V(pObject->GetVelocity());
		vLateralVel.SetZ(V_ZERO);
		float lateralSpeed = Mag(vLateralVel).Getf();
		Vec3V vLateralDir = NormalizeSafe(vLateralVel, Vec3V(V_X_AXIS_WZERO));

		// get the river flow at this position
		Vector2 riverFlow(0.0f, 0.0f);
		River::GetRiverFlowFromPosition(pVfxWaterSampleData->vTestPos, riverFlow);
		Vec3V vRiverVel = Vec3V(VFXWATER_RIVER_VELOCITY_MULTIPLIER_OBJECT*riverFlow.x, VFXWATER_RIVER_VELOCITY_MULTIPLIER_OBJECT*riverFlow.y, 0.0f);

		// stop 360 bank release complaining as all VFXWATER_OBJ_SPLASH_*_ENABLED are currently false
		(void)fxDistSqr;

		if (goneIntoWater)
		{
			if (VFXWATER_OBJ_SPLASH_IN_ENABLED && downwardSpeed>VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_THRESH && fxDistSqr<=VFXWATER_OBJ_SPLASH_IN_RANGE*VFXWATER_OBJ_SPLASH_IN_RANGE)					
			{
				g_vfxWater.TriggerPtFxSplashObjectIn(pObject, pVfxWaterSampleData, vLateralDir, lateralSpeed, downwardSpeed);
			}
		}
		else if (comeOutOfWater)
		{
			if (VFXWATER_OBJ_SPLASH_OUT_ENABLED && fxDistSqr<=VFXWATER_OBJ_SPLASH_OUT_RANGE*VFXWATER_OBJ_SPLASH_OUT_RANGE)
			{
				g_vfxWater.TriggerPtFxSplashObjectOut(pObject, pVfxWaterSampleData, vLateralDir, lateralSpeed, upwardSpeed);
			}
		}
		else if (partiallyInWater)
		{
			if (VFXWATER_OBJ_SPLASH_WADE_ENABLED && fxDistSqr<=VFXWATER_OBJ_SPLASH_WADE_RANGE*VFXWATER_OBJ_SPLASH_WADE_RANGE)
			{
				g_vfxWater.UpdatePtFxSplashObjectWade(pObject, pVfxWaterSampleData, vRiverVel, vLateralVel, id);
			}
		}
		else if (fullyInWater)
		{
			if (VFXWATER_OBJ_SPLASH_TRAIL_ENABLED && fxDistSqr<=VFXWATER_OBJ_SPLASH_TRAIL_RANGE*VFXWATER_OBJ_SPLASH_TRAIL_RANGE)
			{
				g_vfxWater.UpdatePtFxSplashObjectTrail(pObject, pVfxWaterSampleData, vRiverVel, vLateralVel, id);
			}
		}
	}	

	// render debug
#if __BANK
	if (m_renderSplashObjectDebug)
	{
		RenderDebugSplashes(pVfxWaterSampleData, goneIntoWater, comeOutOfWater, partiallyInWater, fullyInWater);
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebugSplashes
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxWater::RenderDebugSplashes				(const VfxWaterSampleData_s* pVfxWaterSampleData, bool goneIntoWater, bool comeOutOfWater, bool partiallyInWater, bool fullyInWater)
{
	Color32 col;
	if (goneIntoWater)
	{
		// green
		col = Color32(0.0f, 1.0f, 0.0f, 1.0f);
	}
	else if (comeOutOfWater)
	{
		// red
		col = Color32(1.0f, 0.0f, 0.0f, 1.0f);
	}
	else if (partiallyInWater)
	{
		// yellow to orange
		float fillRatio = pVfxWaterSampleData->currLevel/pVfxWaterSampleData->maxLevel;
		float colR = 1.0f;
		float colG = 1.0f - (fillRatio*0.5f);
		float colB = 0.3f - (fillRatio*0.3f);
		float colA = 1.0f;
		col = Color32(colR, colG, colB, colA);
	}
	else if (fullyInWater)
	{
		// blue
		col = Color32(0.0f, 0.0f, 1.0f, 1.0f);
	}

	if (m_renderDebugAtSurfacePos)
	{
		grcDebugDraw::Sphere(pVfxWaterSampleData->vSurfacePos, 0.1f, col);
	}
	else
	{
		grcDebugDraw::Sphere(pVfxWaterSampleData->vTestPos, pVfxWaterSampleData->maxLevel, col);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashPedLod
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashPedLod			(CPed* pPed, float downwardSpeed, bool isPlayerVfx)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);

	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if splash lod vfx aren't enabled
	if (pVfxPedInfo->GetSplashLodPtFxEnabled()==false)
	{
		return;
	}
	
	// check that the downward speed is large enough 
	if (downwardSpeed<pVfxPedInfo->GetSplashLodPtFxSpeedEvoMin())
	{
		return;
	}

	// check that the splash is in range to continue
	float fxDistSqr = CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition());
	if (!isPlayerVfx && (fxDistSqr>pVfxPedInfo->GetSplashLodVfxRangeSqr()))
	{
		return;
	}

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

#if __BANK
	if (m_disableSplashPedLod)
	{
		return;
	}
#endif

	// get water position
	Vec3V vWaterPos = pPed->GetTransform().GetPosition();
	float waterZ;
	CVfxHelper::GetWaterZ(vWaterPos, waterZ, pPed);
	vWaterPos.SetZf(waterZ);

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetSplashLodPtFxName());
	if (pFxInst)
	{
		pFxInst->SetBasePos(vWaterPos);

		// set evolutions
		float speedEvo = CVfxHelper::GetInterpValue(downwardSpeed, pVfxPedInfo->GetSplashLodPtFxSpeedEvoMin(), pVfxPedInfo->GetSplashLodPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// don't cull if this is a player effect
		if (isPlayerVfx)
		{
			pFxInst->SetCanBeCulled(false);
		}

		// start the fx
		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterSplashPedFx>(
				CPacketWaterSplashPedFx(pVfxPedInfo->GetSplashLodPtFxName(),
					speedEvo,
					vWaterPos,
					isPlayerVfx),
				pPed);
		}
#endif

		// play audio
		pPed->GetPedAudioEntity()->TriggerBigSplash();

#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (PED LOD): speed:%.2f\n", speedEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashPedEntry
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashPedEntry		(CPed* pPed, float downwardSpeed, bool isPlayerVfx)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);

	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if splash entry vfx aren't enabled
	if (pVfxPedInfo->GetSplashEntryPtFxEnabled()==false)
	{
		return;
	}

	// check that the downward speed is large enough 
	if (downwardSpeed<pVfxPedInfo->GetSplashEntryPtFxSpeedEvoMin())
	{
		return;
	}

	// check that the splash is in range to continue
	float fxDistSqr = CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition());
	if (!isPlayerVfx && (fxDistSqr>pVfxPedInfo->GetSplashEntryVfxRangeSqr()))
	{
		return;
	}

	// don't play if the player is in an interior
	// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
	// 	{
	// 		return;
	// 	}

#if __BANK
	if (m_disableSplashPedEntry)
	{
		return;
	}
#endif

	// get water position
//	Vec3V vWaterPos = pPed->GetTransform().GetPosition();
//	float waterZ;
//	CVfxHelper::GetWaterZ(vWaterPos, waterZ, pPed);
//	vWaterPos.SetZf(waterZ);

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetSplashEntryPtFxName());
	if (pFxInst)
	{
//		pFxInst->SetBasePos(vWaterPos);
		ptfxAttach::Attach(pFxInst, pPed, -1);

		// set evolutions
		float speedEvo = CVfxHelper::GetInterpValue(downwardSpeed, pVfxPedInfo->GetSplashEntryPtFxSpeedEvoMin(), pVfxPedInfo->GetSplashEntryPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// don't cull if this is a player effect
		if (isPlayerVfx)
		{
			pFxInst->SetCanBeCulled(false);
		}

		// start the fx
		pFxInst->Start();

// #if GTA_REPLAY
// 		if(CReplayMgr::ShouldRecord())
// 		{
// 			CReplayMgr::RecordFx<CPacketWaterSplashPedFx>(
// 				CPacketWaterSplashPedFx(pVfxPedInfo->GetSplashLodPtFxName(),
// 				pPed->GetReplayID(),
// 				speedEvo),
// 				pPed);
// 		}
// #endif

		// play audio
		pPed->GetPedAudioEntity()->TriggerBigSplash();

#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (PED ENTRY): speed:%.2f\n", speedEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashPedIn
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashPedIn				(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* UNUSED_PARAM(pBoneWaterInfo), const VfxWaterSampleData_s* pVfxWaterSampleData, s32 UNUSED_PARAM(boneIndex), Vec3V_In vBoneDir, float boneSpeed, bool isPlayerVfx)
{
#if __BANK
	if (m_disableSplashPedIn)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetSplashInPtFxName());
	if (pFxInst)
	{
// 		// get the bone matrix
// 		Mat34V vBoneMtx;
// 		CVfxHelper::GetMatrixFromBoneIndex(vBoneMtx, pPed, boneIndex);
// 
// 		// calc the world matrix
// 		Mat34V vFxMtx;
// 		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, vBoneMtx.GetCol3(), vBoneDir, Vec3V(V_Z_AXIS_WZERO));
// 
// 		// calc the relative matrix
// 		Mat34V vRelFxMat;
// 		CVfxHelper::CreateRelativeMat(vRelFxMat, vBoneMtx, vFxMtx);
// 
// 		// set the matrices
// 		ptfxAttach::Attach(pFxInst, pPed, boneIndex);
// 		pFxInst->SetOffsetMtx(vRelFxMat);

		// calc the world matrix
		Mat34V vFxMtx;
		Vec3V vDir = vBoneDir;
		vDir.SetZ(ScalarV(V_ZERO));
		vDir = NormalizeSafe(vDir, Vec3V(V_X_AXIS_WZERO));
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// set the matrices
		pFxInst->SetBaseMtx(vFxMtx);

		// set the position etc
		ptfxAttach::Attach(pFxInst, pPed, -1, PTFXATTACHMODE_INFO_ONLY);

		// set evolutions
		float speedEvo = CVfxHelper::GetInterpValue(boneSpeed, pVfxPedInfo->GetSplashInPtFxSpeedEvoMin(), pVfxPedInfo->GetSplashInPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		if (pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_SPINE)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spine", 0x110935EB), 1.0f);
		}
		else
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spine", 0x110935EB), 0.0f);
		}

		// don't cull if this is a player effect
		if (isPlayerVfx)
		{
			pFxInst->SetCanBeCulled(false);
		}

		// start the fx
		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterSplashPedInFx>(
				CPacketWaterSplashPedInFx(pVfxWaterSampleData->vSurfacePos, 
					vBoneDir,
					boneSpeed,
					(s32)pSkeletonBoneInfo->m_limbId,
					isPlayerVfx),
				pPed);
		}
#endif

#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (PED IN): speed:%.2f\n", speedEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashPedOut
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashPedOut			(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, s32 boneIndex, Vec3V_In vBoneDir, float boneSpeed)
{
#if __BANK
	if (m_disableSplashPedOut)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetSplashOutPtFxName());
	if (pFxInst)
	{
		// get the bone matrix
		Mat34V vBoneMtx;
		CVfxHelper::GetMatrixFromBoneIndex(vBoneMtx, pPed, boneIndex);

		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, vBoneMtx.GetCol3(), vBoneDir, Vec3V(V_Z_AXIS_WZERO));

		// calc the relative matrix
		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, vBoneMtx, vFxMtx);

		// set the matrices
		ptfxAttach::Attach(pFxInst, pPed, boneIndex);
		pFxInst->SetOffsetMtx(vRelFxMat);

		// set evolutions
		float speedEvo = CVfxHelper::GetInterpValue(boneSpeed, pVfxPedInfo->GetSplashOutPtFxSpeedEvoMin(), pVfxPedInfo->GetSplashOutPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
		
		if (pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_SPINE)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spine", 0x110935EB), 1.0f);
		}
		else
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spine", 0x110935EB), 0.0f);
		}

		// start the fx
		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterSplashPedOutFx>(
				CPacketWaterSplashPedOutFx(vBoneDir,
					boneIndex,
					boneSpeed,
					(s32)pSkeletonBoneInfo->m_limbId),
				pPed);
		}
#endif


#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (PED OUT): speed:%.2f\n", speedEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSplashPedWade
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSplashPedWade				(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* pBoneWaterInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vBoneDir, float boneSpeed, s32 ptFxOffset)
{
#if __BANK
	if (m_disableSplashPedWade)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	// calc the direction of the wade
	Vec3V vDir = (vBoneDir*ScalarVFromF32(boneSpeed)) - vRiverVel;
	vDir.SetZ(ScalarV(V_ZERO));
	float speed = Mag(vDir).Getf();
	vDir = NormalizeSafe(vDir, Vec3V(V_X_AXIS_WZERO));

	// return if not enough speed through the water
	if (speed < pVfxPedInfo->GetSplashWadePtFxSpeedEvoMin())
	{
		return;
	}

	vfxAssertf(ptFxOffset>=0 && ptFxOffset<VFXPED_MAX_VFX_SKELETON_BONES, "ped bone particle offset is out of range (%d)", ptFxOffset);

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_WATER_WADE+ptFxOffset, true, pVfxPedInfo->GetSplashWadePtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// calc the relative matrix
		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, pPed->GetMatrix(), vFxMtx);

		// set the matrices
		ptfxAttach::Attach(pFxInst, pPed, -1);
		pFxInst->SetOffsetMtx(vRelFxMat);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pBoneWaterInfo->m_boneSize, 0.0f, 1.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float speedEvo = CVfxHelper::GetInterpValue(speed, pVfxPedInfo->GetSplashWadePtFxSpeedEvoMin(), pVfxPedInfo->GetSplashWadePtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		if (pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_SPINE)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spine", 0x110935EB), 1.0f);
		}
		else
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spine", 0x110935EB), 0.0f);
		}

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterSplashPedWadeFx>(
				CPacketWaterSplashPedWadeFx(pVfxWaterSampleData->vSurfacePos,
					vBoneDir,
					vRiverVel,
					boneSpeed,
					pSkeletonBoneInfo->m_limbId,
					ptFxOffset),
				pPed,
				true);
		}
#endif // GTA_REPLAY
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSplashPedTrail
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSplashPedTrail			(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo, const VfxPedBoneWaterInfo* UNUSED_PARAM(pBoneWaterInfo), const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In UNUSED_PARAM(vRiverVel), Vec3V_In UNUSED_PARAM(vBoneDir), float boneSpeed, s32 ptFxOffset)
{
#if __BANK
	if (m_disableSplashPedTrail)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	vfxAssertf(ptFxOffset>=0 && ptFxOffset<VFXPED_MAX_VFX_SKELETON_BONES, "ped bone particle offset is out of range (%d)", ptFxOffset);

	// return if the bone isn't moving fast enough
	if (boneSpeed<pVfxPedInfo->GetSplashTrailPtFxSpeedEvoMin())
	{
		return;
	}

	s32 boneIndexB = pPed->GetBoneIndexFromBoneTag(pSkeletonBoneInfo->m_boneTagB);
	if (boneIndexB==BONETAG_INVALID)
	{
		vfxAssertf(0, "invalid bone index generated from bone tag (%s - %d)", pPed->GetDebugName(), pSkeletonBoneInfo->m_boneTagB);
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_WATER_TRAIL+ptFxOffset, true, pVfxPedInfo->GetSplashTrailPtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
 		// get the matrix of the target bone
		vfxAssertf(pPed, "NULL ped pointer passed in");
		Mat34V vBoneMtxB;
		CVfxHelper::GetMatrixFromBoneIndex(vBoneMtxB, pPed, boneIndexB);

		// set the positions of both bones of the water surface
		Vec3V vBonePosA = pVfxWaterSampleData->vTestPos;
		Vec3V vBonePosB = vBoneMtxB.GetCol3();

		// calculate the ptfx matrix (positioned halfway between the bones pointing to the target bone)
		Vec3V vDir = vBonePosB - vBonePosA;
		Vec3V vPos = vBonePosA + (vDir*ScalarV(V_HALF));
		float length = Mag(vDir).Getf();
		vDir = NormalizeSafe(vDir, Vec3V(V_X_AXIS_WZERO));

 		// calculate the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, vPos, vDir, Vec3V(V_Z_AXIS_WZERO));
		pFxInst->SetBaseMtx(vFxMtx);

		// calc the relative matrix
		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, pPed->GetMatrix(), vFxMtx);

		// set the matrices
		ptfxAttach::Attach(pFxInst, pPed, -1);
		pFxInst->SetOffsetMtx(vRelFxMat);

		// set evolutions
		float lengthEvo = length;
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("length", 0x957E2539), lengthEvo);

		float depth;
		CVfxHelper::GetWaterDepth(vPos, depth);
		float depthEvo = 1.0f - CVfxHelper::GetInterpValue(depth, pVfxPedInfo->GetSplashTrailPtFxDepthEvoMin(), pVfxPedInfo->GetSplashTrailPtFxDepthEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

		float speedEvo = CVfxHelper::GetInterpValue(boneSpeed, pVfxPedInfo->GetSplashTrailPtFxSpeedEvoMin(), pVfxPedInfo->GetSplashTrailPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashVehicleIn
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashVehicleIn		(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float downwardSpeed)
{
#if __BANK
	if (m_disableSplashVehicleIn)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetSplashInPtFxName());
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		Vec3V vDir = vLateralDir;
		vDir.SetZ(ScalarV(V_ZERO));
		vDir = NormalizeSafe(vDir, Vec3V(V_X_AXIS_WZERO));
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// set the matrices
		pFxInst->SetBaseMtx(vFxMtx);

		// set the position etc
		ptfxAttach::Attach(pFxInst, pVehicle, -1, PTFXATTACHMODE_INFO_ONLY);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, pVfxVehicleInfo->GetSplashInPtFxSizeEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float lateralSpeedEvo = CVfxHelper::GetInterpValue(lateralSpeed, pVfxVehicleInfo->GetSplashInPtFxSpeedLateralEvoMin(), pVfxVehicleInfo->GetSplashInPtFxSpeedLateralEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_l", 0x1ACEB730), lateralSpeedEvo);

		float downwardSpeedEvo = CVfxHelper::GetInterpValue(downwardSpeed, pVfxVehicleInfo->GetSplashInPtFxSpeedDownwardEvoMin(), pVfxVehicleInfo->GetSplashInPtFxSpeedDownwardEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_d", 0x82B6875E), downwardSpeedEvo);

		// start the fx
		pFxInst->Start();

#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (VEH): speed_l:%.2f speed_d:%.2f\n", lateralSpeedEvo, downwardSpeedEvo);
		}
#endif

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{ 
			CReplayMgr::RecordFx<CPacketWaterSplashVehicleFx>(
				CPacketWaterSplashVehicleFx(pVfxVehicleInfo->GetSplashInPtFxName(), MAT34V_TO_MATRIX34(vFxMtx), sizeEvo, lateralSpeedEvo, downwardSpeedEvo), 
				pVehicle);
		}
#endif // GTA_REPLAY
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashVehicleOut
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashVehicleOut		(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float upwardSpeed)
{
#if __BANK
	if (m_disableSplashVehicleOut)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetSplashOutPtFxName());
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vLateralDir, Vec3V(V_Z_AXIS_WZERO));

		ptfxAttach::Attach(pFxInst, pVehicle, -1, PTFXATTACHMODE_INFO_ONLY);
		pFxInst->SetBaseMtx(vFxMtx);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, pVfxVehicleInfo->GetSplashOutPtFxSizeEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float lateralSpeedEvo = CVfxHelper::GetInterpValue(lateralSpeed, pVfxVehicleInfo->GetSplashOutPtFxSpeedLateralEvoMin(), pVfxVehicleInfo->GetSplashOutPtFxSpeedLateralEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_l", 0x1ACEB730), lateralSpeedEvo);

		float upwardSpeedEvo = CVfxHelper::GetInterpValue(upwardSpeed, pVfxVehicleInfo->GetSplashOutPtFxSpeedUpwardEvoMin(), pVfxVehicleInfo->GetSplashOutPtFxSpeedUpwardEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_u", 0xB664EE5A), upwardSpeedEvo);

		// start the fx
		pFxInst->Start();

#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (VEH): speed_l:%.2f speed_u:%.2f\n", lateralSpeedEvo, upwardSpeedEvo);
		}
#endif

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{ 
			CReplayMgr::RecordFx<CPacketWaterSplashVehicleFx>(
				CPacketWaterSplashVehicleFx(pVfxVehicleInfo->GetSplashOutPtFxName(), MAT34V_TO_MATRIX34(vFxMtx), sizeEvo, lateralSpeedEvo, upwardSpeedEvo), 
				pVehicle);
		}
#endif // GTA_REPLAY
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSplashVehicleWade
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSplashVehicleWade		(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset)
{
#if __BANK
	if (m_disableSplashVehicleWade)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	// calc the accumulated velocity of the wade
	Vec3V vAccumVel = vLateralVel - vRiverVel;
	vAccumVel.SetZ(ScalarV(V_ZERO));
	float accumSpeed = Mag(vAccumVel).Getf();

	// calc the accumulated direction of the wade
	Vec3V vAccumDir = NormalizeSafe(vAccumVel, Vec3V(V_X_AXIS_WZERO));

	// calc the different speed contributions
	float vehicleSpeedDot = Abs(Dot(vLateralVel, vAccumVel).Getf());
	float riverSpeedDot = Abs(Dot(-vRiverVel, vAccumVel).Getf());

	float vehicleSpeedRatio = vehicleSpeedDot / (vehicleSpeedDot+riverSpeedDot);
	float riverSpeedRatio = riverSpeedDot / (vehicleSpeedDot+riverSpeedDot);

	float vehicleSpeed = vehicleSpeedRatio * accumSpeed;
	float riverSpeed = riverSpeedRatio * accumSpeed;
	
	// return if not enough speed through the water
	if (accumSpeed<pVfxVehicleInfo->GetSplashWadePtFxSpeedVehicleEvoMin() && accumSpeed<pVfxVehicleInfo->GetSplashWadePtFxSpeedRiverEvoMin())
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_WADE+ptFxOffset, true, pVfxVehicleInfo->GetSplashWadePtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vAccumDir, Vec3V(V_Z_AXIS_WZERO));

		// calc the relative matrix
		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, pVehicle->GetMatrix(), vFxMtx);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);
		pFxInst->SetOffsetMtx(vRelFxMat);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, pVfxVehicleInfo->GetSplashWadePtFxSizeEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float speedEvo = CVfxHelper::GetInterpValue(vehicleSpeed, pVfxVehicleInfo->GetSplashWadePtFxSpeedVehicleEvoMin(), pVfxVehicleInfo->GetSplashWadePtFxSpeedVehicleEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_v", 0xFF7680DC), speedEvo);

		speedEvo = CVfxHelper::GetInterpValue(riverSpeed, pVfxVehicleInfo->GetSplashWadePtFxSpeedRiverEvoMin(), pVfxVehicleInfo->GetSplashWadePtFxSpeedRiverEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_r", 0xC1200430), speedEvo);

		// front back evolutions
		Vec3V vSampleDirWld = pVfxWaterSampleData->vTestPos - pVehicle->GetTransform().GetPosition();
		vSampleDirWld.SetZ(ScalarV(V_ZERO));
		vSampleDirWld = NormalizeSafe(vSampleDirWld, Vec3V(V_X_AXIS_WZERO));

		float dot = Dot(vAccumDir, vSampleDirWld).Getf();
		float frontEvo = 0.0f;
		if (dot>-VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP)
		{
			frontEvo = (dot+VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP)/(1.0f+VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP);
		}

		dot = -dot;
		float backEvo = 0.0f;
		if (dot>-VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP)
		{
			backEvo = (dot+VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP)/(1.0f+VFXWATER_VEH_SPLASH_WADE_FRONTBACK_OVERLAP);
		}
		
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("front", 0x89F230A2), frontEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("back", 0x37418674), backEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}

// 	if (ptFxOffset==0)
// 	{
// 		vfxDebugf1("accumSpeed=%.2f, vehicleSpeed=%.2f, riverSpeed=%.2f", accumSpeed, vehicleSpeed, riverSpeed);
// 	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSplashVehicleTrail
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSplashVehicleTrail		(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In UNUSED_PARAM(vRiverVel), Vec3V_In UNUSED_PARAM(vLateralVel), s32 ptFxOffset)
{
#if __BANK
	if (m_disableSplashVehicleTrail)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	// return if the vehicle isn't moving fast enough
	Vec3V vVehicleVel = RCC_VEC3V(pVehicle->GetVelocity());
	float vehSpeed = Mag(vVehicleVel).Getf();
	if (vehSpeed<pVfxVehicleInfo->GetSplashTrailPtFxSpeedEvoMin())
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_TRAIL+ptFxOffset, true, pVfxVehicleInfo->GetSplashTrailPtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		Vec3V vVehicleDir = NormalizeSafe(vVehicleVel, Vec3V(V_X_AXIS_WZERO));

		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vTestPos, vVehicleDir, Vec3V(V_Z_AXIS_WZERO));

		// calc the relative matrix
		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, pVehicle->GetMatrix(), vFxMtx);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);
		pFxInst->SetOffsetMtx(vRelFxMat);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, pVfxVehicleInfo->GetSplashTrailPtFxSizeEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float speedEvo = CVfxHelper::GetInterpValue(vehSpeed, pVfxVehicleInfo->GetSplashTrailPtFxSpeedEvoMin(), pVfxVehicleInfo->GetSplashTrailPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{ 
			CReplayMgr::RecordFx<CPacketWaterSplashVehicleTrailFx>(
				CPacketWaterSplashVehicleTrailFx(pVfxVehicleInfo->GetSplashTrailPtFxName(), ptFxOffset, MAT34V_TO_MATRIX34(vRelFxMat), sizeEvo, speedEvo), 
				pVehicle);
		}
#endif // GTA_REPLAY
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashObjectIn
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashObjectIn		(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float downwardSpeed)
{
#if __BANK
	if (m_disableSplashObjectIn)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	ptxEffectInst* pFxInst = NULL;
	if (m_waterPtFxOverrideSplashObjectInName.GetHash()!=0)
	{
		pFxInst = ptfxManager::GetTriggeredInst(m_waterPtFxOverrideSplashObjectInName);
	}
	else
	{
		pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("water_splash_obj_in",0x3D2C5F7));
	}

	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		Vec3V vDir = vLateralDir;
		vDir.SetZ(ScalarV(V_ZERO));
		vDir = NormalizeSafe(vDir, Vec3V(V_X_AXIS_WZERO));
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// set the matrices
		pFxInst->SetBaseMtx(vFxMtx);

		// set the position etc
		ptfxAttach::Attach(pFxInst, pObject, -1, PTFXATTACHMODE_INFO_ONLY);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, VFXWATER_OBJ_SPLASH_SIZE_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float lateralSpeedEvo = CVfxHelper::GetInterpValue(lateralSpeed, VFXWATER_OBJ_SPLASH_IN_SPEED_LATERAL_EVO_MIN, VFXWATER_OBJ_SPLASH_IN_SPEED_LATERAL_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_l", 0x1ACEB730), lateralSpeedEvo);

		float downwardSpeedEvo = CVfxHelper::GetInterpValue(downwardSpeed, VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_EVO_MIN, VFXWATER_OBJ_SPLASH_IN_SPEED_DOWNWARD_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_d", 0x82B6875E), downwardSpeedEvo);

		// start the fx
		pFxInst->Start();

#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (OBJ): speed_l:%.2f speed_d:%.2f\n", lateralSpeedEvo, downwardSpeedEvo);

		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashObjectOut
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashObjectOut		(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vLateralDir, float lateralSpeed, float upwardSpeed)
{
#if __BANK
	if (m_disableSplashObjectOut)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("water_splash_obj_out",0x149A86D6));
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vLateralDir, Vec3V(V_Z_AXIS_WZERO));

		ptfxAttach::Attach(pFxInst, pObject, -1, PTFXATTACHMODE_INFO_ONLY);
		pFxInst->SetBaseMtx(vFxMtx);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, VFXWATER_OBJ_SPLASH_SIZE_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float lateralSpeedEvo = CVfxHelper::GetInterpValue(lateralSpeed, VFXWATER_OBJ_SPLASH_OUT_SPEED_LATERAL_EVO_MIN, VFXWATER_OBJ_SPLASH_OUT_SPEED_LATERAL_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_l", 0x1ACEB730), lateralSpeedEvo);

		float upwardSpeedEvo = CVfxHelper::GetInterpValue(upwardSpeed, VFXWATER_OBJ_SPLASH_OUT_SPEED_UPWARD_EVO_MIN, VFXWATER_OBJ_SPLASH_OUT_SPEED_UPWARD_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_u", 0xB664EE5A), upwardSpeedEvo);

		// start the fx
		pFxInst->Start();

#if __BANK
		if (m_outputSplashDebug)
		{
			vfxDebugf1("SPLASH (OBJ): speed_l:%.2f speed_u:%.2f\n", lateralSpeedEvo, upwardSpeedEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSplashObjectWade
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSplashObjectWade		(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vRiverVel, Vec3V_In vLateralVel, s32 ptFxOffset)
{
#if __BANK
	if (m_disableSplashObjectWade)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	// calc the accumulated velocity of the wade
	Vec3V vAccumVel = vLateralVel - vRiverVel;
	vAccumVel.SetZ(ScalarV(V_ZERO));
	float accumSpeed = Mag(vAccumVel).Getf();

	// calc the accumulated direction of the wade
	Vec3V vAccumDir = NormalizeSafe(vAccumVel, Vec3V(V_X_AXIS_WZERO));

	// calc the different speed contributions
	float objectSpeedDot = Abs(Dot(vLateralVel, vAccumVel).Getf());
	float riverSpeedDot = Abs(Dot(-vRiverVel, vAccumVel).Getf());
	
	float objectSpeedRatio = objectSpeedDot / (objectSpeedDot+riverSpeedDot);
	float riverSpeedRatio = riverSpeedDot / (objectSpeedDot+riverSpeedDot);
	
	float objectSpeed = objectSpeedRatio * accumSpeed;
	float riverSpeed = riverSpeedRatio * accumSpeed;

	// return if not enough speed through the water
	if (accumSpeed<VFXWATER_OBJ_SPLASH_WADE_SPEED_OBJECT_EVO_MIN && accumSpeed<VFXWATER_OBJ_SPLASH_WADE_SPEED_RIVER_EVO_MIN)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pObject, PTFXOFFSET_OBJECT_WADE+ptFxOffset, true, atHashWithStringNotFinal("water_splash_obj_wade",0xD6855416), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vSurfacePos, vAccumDir, Vec3V(V_Z_AXIS_WZERO));

		// calc the relative matrix
		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, pObject->GetMatrix(), vFxMtx);

		ptfxAttach::Attach(pFxInst, pObject, -1);
		pFxInst->SetOffsetMtx(vRelFxMat);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, VFXWATER_OBJ_SPLASH_SIZE_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float speedEvo = CVfxHelper::GetInterpValue(objectSpeed, VFXWATER_OBJ_SPLASH_WADE_SPEED_OBJECT_EVO_MIN, VFXWATER_OBJ_SPLASH_WADE_SPEED_OBJECT_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_o", 0x9A3D360B), speedEvo);

		speedEvo = CVfxHelper::GetInterpValue(riverSpeed, VFXWATER_OBJ_SPLASH_WADE_SPEED_RIVER_EVO_MIN, VFXWATER_OBJ_SPLASH_WADE_SPEED_RIVER_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed_r", 0xC1200430), speedEvo);

// 		// front back evolutions
// 		Vec3V vSampleDirWld = pVfxWaterSampleData->vTestPos - pObject->GetTransform().GetPosition();
// 		vSampleDirWld.SetZ(ScalarV(V_ZERO));
// 		vSampleDirWld = NormalizeSafe(vSampleDirWld, Vec3V(V_X_AXIS_WZERO));
// 
// 		float dot = Dot(vAccumDir, vSampleDirWld).Getf();
// 		float frontEvo = 0.0f;
// 		if (dot>-VFXWATER_OBJ_SPLASH_WADE_FRONTBACK_OVERLAP)
// 		{
// 			frontEvo = (dot+VFXWATER_OBJ_SPLASH_WADE_FRONTBACK_OVERLAP)/(1.0f+VFXWATER_OBJ_SPLASH_WADE_FRONTBACK_OVERLAP);
// 		}
// 
// 		dot = -dot;
// 		float backEvo = 0.0f;
// 		if (dot>-VFXWATER_OBJ_SPLASH_WADE_FRONTBACK_OVERLAP)
// 		{
// 			backEvo = (dot+VFXWATER_OBJ_SPLASH_WADE_FRONTBACK_OVERLAP)/(1.0f+VFXWATER_OBJ_SPLASH_WADE_FRONTBACK_OVERLAP);
// 		}
// 
// 		pFxInst->SetEvoValue("front", frontEvo);
// 		pFxInst->SetEvoValue("back", backEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}

// 	if (ptFxOffset==0)
// 	{
// 		vfxDebugf1("accumSpeed=%.2f, objectSpeed=%.2f, riverSpeed=%.2f", accumSpeed, objectSpeed, riverSpeed);
// 	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSplashObjectTrail
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSplashObjectTrail		(CObject* pObject, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In UNUSED_PARAM(vRiverVel), Vec3V_In UNUSED_PARAM(vLateralVel), s32 ptFxOffset)
{
#if __BANK
	if (m_disableSplashObjectTrail)
	{
		return;
	}
#endif

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	// disable on projectiles
	const CProjectile* pProjectile = pObject->GetAsProjectile();
	if (pProjectile)
	{
		return;
	}

	// return if the object isn't moving fast enough
	Vec3V vObjectVel = RCC_VEC3V(pObject->GetVelocity());
	float objSpeed = Mag(vObjectVel).Getf();
	if (objSpeed<VFXWATER_OBJ_SPLASH_TRAIL_SPEED_EVO_MIN)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pObject, PTFXOFFSET_OBJECT_TRAIL+ptFxOffset, true, atHashWithStringNotFinal("water_splash_obj_trail",0x3BF317AF), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		Vec3V vObjectDir = NormalizeSafe(vObjectVel, Vec3V(V_X_AXIS_WZERO));

		// calc the world matrix
		Mat34V vFxMtx;
		CVfxHelper::CreateMatFromVecYAlign(vFxMtx, pVfxWaterSampleData->vTestPos, vObjectDir, Vec3V(V_Z_AXIS_WZERO));

		// calc the relative matrix
		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, pObject->GetMatrix(), vFxMtx);

		ptfxAttach::Attach(pFxInst, pObject, -1);
		pFxInst->SetOffsetMtx(vRelFxMat);

		// set evolutions
		float sizeEvo = CVfxHelper::GetInterpValue(pVfxWaterSampleData->maxLevel, 0.0f, VFXWATER_OBJ_SPLASH_SIZE_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		float speedEvo = CVfxHelper::GetInterpValue(objSpeed, VFXWATER_OBJ_SPLASH_TRAIL_SPEED_EVO_MIN, VFXWATER_OBJ_SPLASH_TRAIL_SPEED_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxBoat
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxBoat					(CVehicle* pVehicle, const VfxWaterSampleData_s* pVfxWaterSampleData)
{	
	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	ProcessVfxBoatBow(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData);
	ProcessVfxBoatEntry(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData);
	ProcessVfxBoatExit(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData);

	// render debug
#if __BANK
	if (m_renderBoatDebug)
	{
		CBuoyancyInfo* pBuoyancyInfo = pVehicle->m_Buoyancy.GetBuoyancyInfo(pVehicle);
		vfxAssert(pBuoyancyInfo);
		int numWaterSamples = pBuoyancyInfo->m_nNumWaterSamples;
		for (int i=0; i<numWaterSamples; i++)
		{
			const VfxWaterSampleData_s* pCurrVfxWaterSampleData = &pVfxWaterSampleData[i];

			if (pCurrVfxWaterSampleData->isInWater)
			{
				// init the sample state flags
				bool goneIntoWater = pCurrVfxWaterSampleData->prevLevel<(pCurrVfxWaterSampleData->maxLevel*0.5f) && pCurrVfxWaterSampleData->currLevel>=(pCurrVfxWaterSampleData->maxLevel*0.5f);
				bool comeOutOfWater = pCurrVfxWaterSampleData->prevLevel>=(pCurrVfxWaterSampleData->maxLevel*0.5f) && pCurrVfxWaterSampleData->currLevel<(pCurrVfxWaterSampleData->maxLevel*0.5f);
				bool partiallyInWater = pCurrVfxWaterSampleData->currLevel>0.0f && pCurrVfxWaterSampleData->currLevel<pCurrVfxWaterSampleData->maxLevel;
				bool fullyInWater = pCurrVfxWaterSampleData->currLevel>=pCurrVfxWaterSampleData->maxLevel;

				RenderDebugSplashes(pCurrVfxWaterSampleData, goneIntoWater, comeOutOfWater, partiallyInWater, fullyInWater);
			}
		}
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxBoatBow
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxBoatBow				(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetBoatBowPtFxRangeSqr())
	{
		return;
	}
	
	CBuoyancyInfo* pBuoyancyInfo = pVehicle->m_Buoyancy.GetBuoyancyInfo(pVehicle);
	vfxAssert(pBuoyancyInfo);
	int numWaterSampleRows = pBuoyancyInfo->m_nNumBoatWaterSampleRows;

	//Assert(m_nNumSamplesToUse==25);

	// find out the boat direction
	Vec3V vDir = RCC_VEC3V(pVehicle->GetVelocity());
	vDir = Normalize(vDir);

	float dot = Dot(vDir, pVehicle->GetTransform().GetB()).Getf();

	// process each sample row (j)
	if (dot>=0.0f)
	{
		for (s32 j=numWaterSampleRows-1; j>=0; j--)
		{		
			if (ProcessVfxBoatBowRow(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, j, true, true, false))
			{
				break;
			}
		}

		for (s32 j=0; j<numWaterSampleRows; j++)
		{		
			if (ProcessVfxBoatBowRow(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, j, true, false, true))
			{
				break;
			}
		}
	}
	else
	{
		for (s32 j=0; j<numWaterSampleRows; j++)
		{		
			if (ProcessVfxBoatBowRow(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, j, false, true, false))
			{
				break;
			}
		}

		for (s32 j=numWaterSampleRows-1; j>=0; j--)
		{		
			if (ProcessVfxBoatBowRow(pVehicle, pVfxVehicleInfo, pVfxWaterSampleData, j, false, false, true))
			{
				break;
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxBoatBowRow
///////////////////////////////////////////////////////////////////////////////

bool			CVfxWater::ProcessVfxBoatBowRow				(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData, s32 row, bool goingForward, bool doBowFx, bool doWakeFx)
{
	CBuoyancyInfo* pBuoyancyInfo = pVehicle->m_Buoyancy.GetBuoyancyInfo(pVehicle);
	vfxAssert(pBuoyancyInfo);
	int numWaterSampleRows = pBuoyancyInfo->m_nNumBoatWaterSampleRows;
	int numWaterSamples = pBuoyancyInfo->m_nNumWaterSamples;

	int firstIndexInRow = pBuoyancyInfo->m_nBoatWaterSampleRowIndices[row];
	int numIndicesInRow;
	if (row==numWaterSampleRows-1)
	{
		numIndicesInRow = numWaterSamples - firstIndexInRow;
	}
	else
	{
		numIndicesInRow = pBuoyancyInfo->m_nBoatWaterSampleRowIndices[row+1] - firstIndexInRow;
	}

	int leftWashWaterSampleIndex = pBuoyancyInfo->m_nBoatWaterSampleRowIndices[0];
	int rightWashWaterSampleIndex = pBuoyancyInfo->m_nBoatWaterSampleRowIndices[1] - 1;

	// find the left and right samples in this row
	s32 leftSampleIndex = -1;
	s32 rightSampleIndex = -1;
	for (s32 i=0; i<numIndicesInRow; i++)
	{
		s32 currIndex = firstIndexInRow+i;

		if (pVfxWaterSampleData[currIndex].isInWater)
		{
			if (leftSampleIndex==-1)
			{
				// left sample not set yet - set now
				leftSampleIndex = i;
			}

			// set the right sample 
			rightSampleIndex = i;
		}
	}

	if (leftSampleIndex==-1 && rightSampleIndex==-1)
	{
		return false;
	}

	// 
	for (s32 i=0; i<2; i++)
	{
		s32 bowRegisterOffset = PTFXOFFSET_VEHICLE_BOAT_BOW_L+row;
		s32 washRegisterOffset = PTFXOFFSET_VEHICLE_BOAT_WASH_L;
		s32 currSampleIndex = leftSampleIndex;
		Vec3V vDir = -Vec3V(V_X_AXIS_WZERO);
		if (i==1)
		{
			bowRegisterOffset = PTFXOFFSET_VEHICLE_BOAT_BOW_R+row;
			washRegisterOffset = PTFXOFFSET_VEHICLE_BOAT_WASH_R;
			currSampleIndex = rightSampleIndex;
			vDir.SetX(ScalarV(V_ONE));
		}
		vDir = Transform3x3(pVehicle->GetMatrix(), vDir);

		s32 currIndex = firstIndexInRow+currSampleIndex;

		if (currSampleIndex!=-1)
		{
			const VfxWaterSampleData_s* pCurrVfxWaterSampleData = &pVfxWaterSampleData[currIndex];

			if (doBowFx)
			{
				g_vfxWater.UpdatePtFxBoatBow(pVehicle, pVfxVehicleInfo, bowRegisterOffset, pCurrVfxWaterSampleData, vDir, goingForward);
			}

			if (doWakeFx)
			{
				if (goingForward && ((i==0 && currSampleIndex==leftWashWaterSampleIndex) || (i==1 && currSampleIndex==rightWashWaterSampleIndex)))
				{
					g_vfxWater.UpdatePtFxBoatWash(pVehicle, pVfxVehicleInfo, washRegisterOffset, pCurrVfxWaterSampleData);
				}
			}

			// water disturbance foam
			if (VFXWATER_ADD_FOAM)
			{
				float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), VFXWATER_BOAT_FOAM_SPEED_MIN, VFXWATER_BOAT_FOAM_SPEED_MAX);
				if (speedEvo>0.0f)
				{
					float foamVal = VFXWATER_BOAT_FOAM_VAL_MIN + (speedEvo*(VFXWATER_BOAT_FOAM_VAL_MAX-VFXWATER_BOAT_FOAM_VAL_MIN));
					Vec3V waterPos = pVfxWaterSampleData[currSampleIndex].vSurfacePos;
					Water::AddFoam(waterPos.GetXf(), waterPos.GetYf(), foamVal);
				}
			}
		}
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  GetEntryWaterSampleIndex
///////////////////////////////////////////////////////////////////////////////

s8				CVfxWater::GetEntryWaterSampleIndex(CVehicle* pVehicle)
{
	CBuoyancyInfo* pBuoyancyInfo = pVehicle->m_Buoyancy.GetBuoyancyInfo(pVehicle);
	vfxAssert(pBuoyancyInfo);
	s8 numWaterSampleRows = pBuoyancyInfo->m_nNumBoatWaterSampleRows;
	if (numWaterSampleRows <= 0)
	{
		return -1;
	}
	s8 lastRowWaterSampleIndex = pBuoyancyInfo->m_nBoatWaterSampleRowIndices[numWaterSampleRows - 1];
	s8 secondLastRowWaterSampleIndex = pBuoyancyInfo->m_nBoatWaterSampleRowIndices[numWaterSampleRows - 2];
	s8 secondLastRowNumWaterSamples = lastRowWaterSampleIndex - secondLastRowWaterSampleIndex;
	return secondLastRowWaterSampleIndex + (secondLastRowNumWaterSamples)/2;
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxBoatEntry
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxBoatEntry				(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData)
{
	bool vfxDistanceValid = CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())<=pVfxVehicleInfo->GetBoatEntryPtFxRangeSqr();
	int entryWaterSampleIndex = GetEntryWaterSampleIndex(pVehicle);
	if (entryWaterSampleIndex < 0)
	{
		return;
	}

	const VfxWaterSampleData_s* pVfxWaterSampleDataEntry = &pVfxWaterSampleData[entryWaterSampleIndex];

	bool goneIntoWater = pVfxWaterSampleDataEntry->prevLevel==0.0f && pVfxWaterSampleDataEntry->currLevel>0.0f;
	bool comeOutOfWater = pVfxWaterSampleData->prevLevel>=pVfxWaterSampleData->maxLevel && pVfxWaterSampleData->currLevel<pVfxWaterSampleData->maxLevel;

	if (pVfxWaterSampleDataEntry->isInWater)
	{
		if (goneIntoWater)
		{
			if (vfxDistanceValid)
			{
				g_vfxWater.TriggerPtFxBoatEntry(pVehicle, pVfxVehicleInfo, pVfxWaterSampleDataEntry->vSurfacePos);
				// If there is a player in this boat, give their control pad a shake to let them know they've slapped the water.
				if(pVehicle->ContainsLocalPlayer())
				{
					const u32 snRumbleDuration = 50;
					const float sfRumbleIntensityScale = 0.8f;
					float fRumbleScale = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetBoatEntryPtFxSpeedEvoMin(),
						pVfxVehicleInfo->GetBoatEntryPtFxSpeedEvoMax());
					CControlMgr::StartPlayerPadShakeByIntensity(snRumbleDuration, fRumbleScale*sfRumbleIntensityScale);
				}
			}

			if(pVehicle->GetVehicleAudioEntity())
			{
				pVehicle->GetVehicleAudioEntity()->TriggerBoatEntrySplash(goneIntoWater, comeOutOfWater);
			}
//			grcDebugDraw::Sphere(pVfxWaterSampleDataEntry->currPos, pVfxWaterSampleDataEntry->maxLevel*0.5f, Color32(0.0f, 1.0f, 0.0f));
		}
//		else
//		{
//			grcDebugDraw::Sphere(pVfxWaterSampleDataEntry->surfacePos, pVfxWaterSampleDataEntry->maxLevel*0.5f, Color32(1.0f, 0.0f, 0.0f));
//		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxBoatExit
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::ProcessVfxBoatExit				(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, const VfxWaterSampleData_s* pVfxWaterSampleData)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetBoatExitPtFxRangeSqr())
	{
		return;
	}

	int exitWaterSampleIndex = GetEntryWaterSampleIndex(pVehicle);
	if (exitWaterSampleIndex < 0)
	{
		return;
	}

	const VfxWaterSampleData_s* pVfxWaterSampleDataExit = &pVfxWaterSampleData[exitWaterSampleIndex];
	if (!pVfxWaterSampleDataExit->isInWater && pVfxWaterSampleDataExit->prevLevel>0.0f)
	{
		g_vfxWater.TriggerPtFxBoatExit(pVehicle, pVfxVehicleInfo, pVfxWaterSampleDataExit->vTestPos);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxBoatBow
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxBoatBow					(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, s32 registerOffset, const VfxWaterSampleData_s* pVfxWaterSampleData, Vec3V_In vDir, bool goingForward)
{
#if __BANK
	if (m_disableBoatBow)
	{
		return;
	}
#endif

	vfxAssertf(pVfxWaterSampleData->isInWater, "boat bow water sample is not in water - the position may not be valid");

	// check if we require boat bow particle effects
	if (pVfxVehicleInfo->GetBoatBowPtFxEnabled()==false)
	{
		return;
	}

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	bool useMountedCameraPtFx = pCamera && pCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()) && 
								pVehicle->GetDriver() && pVehicle->GetDriver()->IsLocalPlayer() && 
								pVfxVehicleInfo->GetBoatBowPtFxForwardMountedName().GetHash()!=0;

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	if (goingForward)
	{
		if (useMountedCameraPtFx)
		{
			pFxInst = ptfxManager::GetRegisteredInst(pVehicle, registerOffset, true, pVfxVehicleInfo->GetBoatBowPtFxForwardMountedName(), justCreated);
		}
		else
		{
			pFxInst = ptfxManager::GetRegisteredInst(pVehicle, registerOffset, true, pVfxVehicleInfo->GetBoatBowPtFxForwardName(), justCreated);
		}
	}
	else
	{
		pFxInst = ptfxManager::GetRegisteredInst(pVehicle, registerOffset, true, pVfxVehicleInfo->GetBoatBowPtFxReverseName(), justCreated);
	}

	// check the effect exists
	if (pFxInst)
	{
		m_boatBowPtFxActive = true;

		// calc the world matrix
		Mat34V fxOffsetMat;
		CVfxHelper::CreateMatFromVecYAlign(fxOffsetMat, pVfxWaterSampleData->vSurfacePos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// apply the reverse offset
		if (!goingForward)
		{
			Vec3V vPos = fxOffsetMat.GetCol3();
			Vec3V vSide = fxOffsetMat.GetCol1();
			fxOffsetMat.SetCol3(vPos + (vSide*ScalarVFromF32(pVfxVehicleInfo->GetBoatBowPtFxReverseOffset())));
		}

		// make the matrix relative to the entity
		Mat34V invEntityMat;
		InvertTransformOrtho(invEntityMat, pVehicle->GetMatrix());	
		Transform(fxOffsetMat, invEntityMat, fxOffsetMat);

		// position mounted camera effects at a fixed position
		if (useMountedCameraPtFx && goingForward)
		{
			fxOffsetMat.SetCol3(pVfxVehicleInfo->GetBoatBowPtFxForwardMountedOffset());
		}

		// speed evolution
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetBoatBowPtFxSpeedEvoMin(), pVfxVehicleInfo->GetBoatBowPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		audBoatAudioEntity* boatAudioEntity = NULL;

		if(pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_BOAT)
		{
			boatAudioEntity = static_cast<audBoatAudioEntity*>(pVehicle->GetVehicleAudioEntity());
			boatAudioEntity->SetSpeed(speedEvo);
		}

		// keel evolution 
		Vec3V vBoatRight = pVehicle->GetTransform().GetMatrix().GetCol0();
		vBoatRight.SetZ(V_ZERO);
		vBoatRight = NormalizeSafe(vBoatRight, Vec3V(V_X_AXIS_WZERO));

		Vec3V vBoatUp = pVehicle->GetTransform().GetMatrix().GetCol2();
		float rightTilt = Dot(vBoatUp, vBoatRight).Getf();

		static float TILT_MULT = 5.0f;
		float keelEvo = 0.0f;
		if (registerOffset>=PTFXOFFSET_VEHICLE_BOAT_BOW_L && registerOffset<=PTFXOFFSET_VEHICLE_BOAT_BOW_L_5)
		{
			if (rightTilt>0.0f)
			{
				keelEvo = Clamp(rightTilt*TILT_MULT, 0.0f, 1.0f);
			}
		}
		else
		{
			if (rightTilt<0.0f)
			{
				keelEvo = Clamp(-rightTilt*TILT_MULT, 0.0f, 1.0f);
			}
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("keelforce", 0xB275E626), keelEvo);

		if (boatAudioEntity)
		{
			boatAudioEntity->SetKeelForce(keelEvo);
		}

		// shootout evolution
		if (pVehicle==CVfxHelper::GetShootoutBoat())
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("shootout", 0x28618C59), 1.0f);
		}
		else
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("shootout", 0x28618C59), 0.0f);
		}

		// flip the x axis for left hand effects
		if (registerOffset>=PTFXOFFSET_VEHICLE_BOAT_BOW_L && registerOffset<=PTFXOFFSET_VEHICLE_BOAT_BOW_L_5)
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
		}

		pFxInst->SetOffsetMtx(fxOffsetMat);

		// set the position etc
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		// start the fx
		if (justCreated)
		{
#if __BANK
			if (m_overrideBoatBowScale>0.0f)
			{
				pFxInst->SetUserZoom(m_overrideBoatBowScale);
			}
			else
#endif
			{
				Assertf(pVehicle->pHandling && pVehicle->pHandling->GetBoatHandlingData(),"Boat is missing handling data!");
				pFxInst->SetUserZoom(pVfxVehicleInfo->GetBoatBowPtFxScale());
			}

			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
		}
#endif
	
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterBoatBowFx>(
				CPacketWaterBoatBowFx(pVfxWaterSampleData->vSurfacePos,
					vDir,
					(u8)registerOffset,
					goingForward),
				pVehicle);
		}
#endif
	}

	// debug rendering to visualise the forces
//	static float keelScale = 0.005f;
//	Vec3V vDebugKeelEndPos = RCC_VEC3V(pVfxWaterSampleData->surfacePos) + (RCC_VEC3V(pVfxWaterSampleData->keelForce)*ScalarVFromF32(keelScale));
//	grcDebugDraw::Line(pVfxWaterSampleData->surfacePos, RCC_VECTOR3(vDebugKeelEndPos), Color32(1.0f, 0.0f, 0.0f, 1.0f), Color32(1.0f, 0.0f, 0.0f, 1.0f));
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxBoatWash
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxBoatWash					(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, s32 registerOffset, const VfxWaterSampleData_s* pVfxWaterSampleData)
{
#if __BANK
	if (m_disableBoatWash)
	{
		return;
	}
#endif

	// check if we require boat wash particle effects
	if (pVfxVehicleInfo->GetBoatWashPtFxEnabled()==false)
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetBoatWashPtFxRangeSqr())
	{
		return;
	}

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pVehicle, registerOffset, true, pVfxVehicleInfo->GetBoatWashPtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V fxOffsetMat;
		Vec3V vDir = pVehicle->GetTransform().GetB();
		CVfxHelper::CreateMatFromVecYAlign(fxOffsetMat, pVfxWaterSampleData->vSurfacePos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// make the matrix relative to the entity
		Mat34V invEntityMat;
		InvertTransformOrtho(invEntityMat, pVehicle->GetMatrix());	
		Transform(fxOffsetMat, invEntityMat, fxOffsetMat);

		// speed evolution
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetBoatWashPtFxSpeedEvoMin(), pVfxVehicleInfo->GetBoatWashPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// flip the x axis for left hand effects
		if (registerOffset==PTFXOFFSET_VEHICLE_BOAT_WASH_L)
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
		}

		pFxInst->SetOffsetMtx(fxOffsetMat);

		// set the position etc
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		// start the fx
		if (justCreated)
		{
#if __BANK
			if (m_overrideBoatWashScale>0.0f)
			{
				pFxInst->SetUserZoom(m_overrideBoatWashScale);
			}
			else
#endif
			{
				Assertf(pVehicle->pHandling && pVehicle->pHandling->GetBoatHandlingData(),"Boat is missing handling data!");
				pFxInst->SetUserZoom(pVfxVehicleInfo->GetBoatWashPtFxScale());
			}
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
		}
#endif

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterBoatWashFx>(
				CPacketWaterBoatWashFx(pVfxWaterSampleData->vSurfacePos, registerOffset),
				pVehicle);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxBoatEntry
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxBoatEntry				(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos)
{
#if __BANK
	if (m_disableBoatEntry)
	{
		return;
	}
#endif

	// check if we require boat entry particle effects
	if (pVfxVehicleInfo->GetBoatEntryPtFxEnabled()==false)
	{
		return;
	}

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetBoatEntryPtFxName());
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V fxOffsetMat;
		Vec3V vDir = pVehicle->GetTransform().GetB();
		CVfxHelper::CreateMatFromVecYAlign(fxOffsetMat, vPos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// make the matrix relative to the entity
		Mat34V invEntityMat;
		InvertTransformOrtho(invEntityMat, pVehicle->GetMatrix());	
		Transform(fxOffsetMat, invEntityMat, fxOffsetMat);

		// set the position etc
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetMtx(fxOffsetMat);

		// speed evolution
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetBoatEntryPtFxSpeedEvoMin(), pVfxVehicleInfo->GetBoatEntryPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

#if __BANK
		if (m_overrideBoatEntryScale>0.0f)
		{
			pFxInst->SetUserZoom(m_overrideBoatEntryScale);
		}
		else
#endif
		{
			Assertf(pVehicle->pHandling && pVehicle->pHandling->GetBoatHandlingData(),"Boat is missing handling data!");
			pFxInst->SetUserZoom(pVfxVehicleInfo->GetBoatEntryPtFxScale());
		}

		// start the fx
		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterBoatEntryFx>(
				CPacketWaterBoatEntryFx(vPos),
				pVehicle);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxBoatExit
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxBoatExit				(CVehicle* pVehicle, const CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos)
{
#if __BANK
	if (m_disableBoatExit)
	{
		return;
	}
#endif

	// check if we require boat exit particle effects
	if (pVfxVehicleInfo->GetBoatExitPtFxEnabled()==false)
	{
		return;
	}

	// don't play if the player is in an interior
	// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
	// 	{
	// 		return;
	// 	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetBoatExitPtFxName());
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V fxOffsetMat;
		Vec3V vDir = pVehicle->GetTransform().GetB();
		CVfxHelper::CreateMatFromVecYAlign(fxOffsetMat, vPos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// make the matrix relative to the entity
		Mat34V invEntityMat;
		InvertTransformOrtho(invEntityMat, pVehicle->GetMatrix());	
		Transform(fxOffsetMat, invEntityMat, fxOffsetMat);

		// set the position etc
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetMtx(fxOffsetMat);

		// speed evolution
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetBoatExitPtFxSpeedEvoMin(), pVfxVehicleInfo->GetBoatExitPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

#if __BANK
		if (m_overrideBoatExitScale>0.0f)
		{
			pFxInst->SetUserZoom(m_overrideBoatExitScale);
		}
		else
#endif
		{
			Assertf(pVehicle->pHandling && pVehicle->pHandling->GetBoatHandlingData(),"Boat is missing handling data!");
			pFxInst->SetUserZoom(pVfxVehicleInfo->GetBoatExitPtFxScale());
		}

		// start the fx
		pFxInst->Start();

// #if GTA_REPLAY
// 		if(CReplayMgr::ShouldRecord())
// 		{
// 			CReplayMgr::RecordFx<CPacketWaterBoatExitFx>(
// 				CPacketWaterBoatExitFx(vPos),
// 				pVehicle);
// 		}
// #endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxBoatPropeller
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxBoatPropeller				(CVehicle* pVehicle, Vec3V_In vPropOffset)
{
#if __BANK
	if (m_disableBoatProp)
	{
		return;
	}
#endif

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// check if we require boat propeller particle effects
	if (pVfxVehicleInfo->GetBoatPropellerPtFxEnabled()==false)
	{
		return;
	}

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	float fScale = pVfxVehicleInfo->GetBoatPropellerPtFxScale();
	if (fScale == 0.0f)
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetBoatPropellerPtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_PROPELLER, true, pVfxVehicleInfo->GetBoatPropellerPtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		float forwardEvo = 0.0f;
		float backwardEvo = 0.0f;
		Vec3V vBoatVelocity = RCC_VEC3V(pVehicle->GetVelocity());
		float boatSpeed = Mag(vBoatVelocity).Getf();

		CTransmission* pTransmission = &pVehicle->m_Transmission;
		CTransmission* pTransmission2 = pVehicle->GetSecondTransmission();
		if (pVehicle->InheritsFromAmphibiousAutomobile() && pTransmission2)
		{
			pTransmission = pTransmission2;
		}

		float dot = Dot(vBoatVelocity, pVehicle->GetTransform().GetB()).Getf();
		if (dot<0.0f)
		{
			backwardEvo = CVfxHelper::GetInterpValue(boatSpeed, pVfxVehicleInfo->GetBoatPropellerPtFxBackwardSpeedEvoMin(), pVfxVehicleInfo->GetBoatPropellerPtFxBackwardSpeedEvoMax());
			backwardEvo *= pTransmission->GetRevRatio() > 0.2f? Abs(pVehicle->GetThrottle()) : 0.0f;
			vfxAssertf(backwardEvo>=0.0f, "backward evo out of range");
			vfxAssertf(backwardEvo<=1.0f, "backward evo out of range");
		}
		else
		{
			forwardEvo = CVfxHelper::GetInterpValue(boatSpeed, pVfxVehicleInfo->GetBoatPropellerPtFxForwardSpeedEvoMin(), pVfxVehicleInfo->GetBoatPropellerPtFxForwardSpeedEvoMax());
			forwardEvo *= pTransmission->GetRevRatio() > 0.2f? Abs(pVehicle->GetThrottle()) : 0.0f;
			vfxAssertf(forwardEvo>=0.0f, "forward evo out of range");
			vfxAssertf(forwardEvo<=1.0f, "forward evo out of range");
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("forward", 0x32F58784), forwardEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("backward", 0x32CEDAF3), backwardEvo);

		CBoatHandling* pBoatHandling = pVehicle->GetBoatHandling();
		if (pBoatHandling)
		{
			float depthEvo = CVfxHelper::GetInterpValue(pBoatHandling->GetPropellerDepth(), pVfxVehicleInfo->GetBoatPropellerPtFxDepthEvoMin(), pVfxVehicleInfo->GetBoatPropellerPtFxDepthEvoMax());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);
		}

		if (pVehicle==CVfxHelper::GetShootoutBoat())
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("shootout", 0x28618C59), 1.0f);
		}
		else
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("shootout", 0x28618C59), 0.0f);
		}

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetPos(vPropOffset);

		// check if the effect has just been created 
		if (justCreated)
		{
#if __BANK
			if (m_overrideBoatPropScale>0.0f)
			{
				pFxInst->SetUserZoom(m_overrideBoatPropScale);
			}
			else
#endif
			{
				pFxInst->SetUserZoom(fScale);
			}

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
		else
		{
			// set the propeller offset again - it can sometimes be calc'd incorrectly in the first instance 
			// so we keep it updated every frame
			pFxInst->SetOffsetPos(vPropOffset);

#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPropeller
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxPropeller				(CVehicle* pVehicle, s32 propId, s32 boneIndex, float propSpeed)
{
	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// check if we require boat propeller particle effects
	if (pVfxVehicleInfo->GetBoatPropellerPtFxEnabled()==false)
	{
		return;
	}

	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	float fScale = pVfxVehicleInfo->GetBoatPropellerPtFxScale();
	if (fScale == 0.0f)
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetBoatPropellerPtFxRangeSqr())
	{
		return;
	}

	// get the propeller matrix
	Mat34V boneMtx;
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, boneIndex);

	// get the water depth
	float waterDepth;
	CVfxHelper::GetWaterDepth(boneMtx.GetCol3(), waterDepth);
	if (waterDepth==0.0f)
	{
		return;
	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_PROPELLER+propId, true, pVfxVehicleInfo->GetBoatPropellerPtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		float forwardEvo = CVfxHelper::GetInterpValue(propSpeed, pVfxVehicleInfo->GetBoatPropellerPtFxForwardSpeedEvoMin(), pVfxVehicleInfo->GetBoatPropellerPtFxForwardSpeedEvoMax());
	 	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("forward", 0x32F58784), forwardEvo);

		float backwardEvo = CVfxHelper::GetInterpValue(propSpeed, pVfxVehicleInfo->GetBoatPropellerPtFxBackwardSpeedEvoMin(), pVfxVehicleInfo->GetBoatPropellerPtFxBackwardSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("backward", 0x32CEDAF3), backwardEvo);

		float depthEvo = CVfxHelper::GetInterpValue(waterDepth, pVfxVehicleInfo->GetBoatPropellerPtFxDepthEvoMin(), pVfxVehicleInfo->GetBoatPropellerPtFxDepthEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

		// check if the effect has just been created 
		if (justCreated)
		{
#if __BANK
			if (m_overrideBoatPropScale>0.0f)
			{
				pFxInst->SetUserZoom(m_overrideBoatPropScale);
			}
			else
#endif
			{
				pFxInst->SetUserZoom(fScale);
			}

			// it has just been created - start it and set it's position
			pFxInst->Start();

#if GTA_REPLAY
			// WIP...doesn't work properly yet
// 			if(!CReplayMgr::IsEditModeActive())
// 			{
// 				CReplayMgr::RecordFx<CVehicle, CPacketWaterBoatPropellerFx>(pVehicle,
// 					CPacketWaterBoatPropellerFx(atHashWithStringNotFinal(pVfxVehicleInfo->GetBoatPropellerPtFxName()),
// 						pVehicle->GetReplayID(),
// forwardEvo,
// backwardEvo,
// depthEvo,
// fScale));
// 			}
#endif
		}
		else
		{
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxSplashHeliBlade
///////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSplashHeliBlade			(CEntity* pParent, Vec3V_In vPos)
{
	// don't play if the player is in an interior
// 	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
// 	if (pPlayerPed && pPlayerPed->GetIsInInterior())
// 	{
// 		return;
// 	}

	Vec3V vParentPos = pParent->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vParentPos)>VFXRANGE_WATER_SPLASH_BLADES_SQR)
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("water_heli_blades",0xC753E339));
	if (pFxInst)
	{
		// calc the world matrix
		Vec3V vDir = vPos - vParentPos;
		vDir = Normalize(vDir);
		Mat34V fxOffsetMat;
		CVfxHelper::CreateMatFromVecYAlign(fxOffsetMat, vPos, vDir, Vec3V(V_Z_AXIS_WZERO));

		// make the matrix relative to the entity
		Mat34V invEntityMat;
		InvertTransformOrtho(invEntityMat, pParent->GetMatrix());	
		Transform(fxOffsetMat, invEntityMat, fxOffsetMat);

		// set the position etc
		pFxInst->SetBaseMtx(pParent->GetMatrix());
		pFxInst->SetOffsetMtx(fxOffsetMat);

		// start the fx
		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterSplashHeliFx>(CPacketWaterSplashHeliFx(vPos), pParent);
		}
#endif // GTA_REPLAY
	}
}


/////////////////////////////////////////////////////////////////////////////////
// UpdatePtFxSubmarineDive
/////////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSubmarineDive				(CSubmarine* pSubmarine, float diveEvo)
{
	// don't update effect if the vehicle is off screen
	if (!pSubmarine->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	Vec3V vSubmarinePos = pSubmarine->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vSubmarinePos)>VFXRANGE_WATER_SUB_DIVE_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pSubmarine, PTFXOFFSET_VEHICLE_SUBMARINE_DIVE, false, atHashWithStringNotFinal("veh_sub_dive",0x9FCE7CEA), justCreated);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pSubmarine, -1);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("dive", 0xE7E43AD4), diveEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}


#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketSubDiveFx>(CPacketSubDiveFx(diveEvo), pSubmarine);
		}
#endif // GTA_REPLAY


	}
}


/////////////////////////////////////////////////////////////////////////////////
// UpdatePtFxSubmarineAirLeak
/////////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxSubmarineAirLeak				(CSubmarine* pSubmarine, float damageEvo)
{
	// don't update effect if the vehicle is off screen
	if (!pSubmarine->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	Vec3V vSubmarinePos = pSubmarine->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vSubmarinePos)>VFXRANGE_WATER_SUB_AIR_LEAK_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pSubmarine, PTFXOFFSET_VEHICLE_SUBMARINE_AIR_LEAK, false, atHashWithStringNotFinal("veh_sub_leak",0xead32a2e), justCreated);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pSubmarine, -1);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375e1), damageEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// TriggerPtFxSubmarineCrush
/////////////////////////////////////////////////////////////////////////////////

void			CVfxWater::TriggerPtFxSubmarineCrush				(CSubmarine* pSubmarine)
{
	// return if the vehicle is off screen
	if (!pSubmarine->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	Vec3V vSubmarinePos = pSubmarine->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vSubmarinePos)>VFXRANGE_WATER_SUB_CRUSH_SQR)
	{
		return;
	}

	// register the fx system
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_sub_crush",0x07d00dc1));
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pSubmarine, -1);

		pFxInst->Start();
	}
}


/////////////////////////////////////////////////////////////////////////////////
// UpdatePtFxWaterCannonJet
/////////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxWaterCannonJet			(CWaterCannon* pWaterCannon, bool registeredThisFrame, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In UNUSED_PARAM(vVelocity), bool collOccurred, Vec3V_In vCollPosition, Vec3V_In vCollNormal, float vehSpeed)
{
	// don't process if the vehicle is invisible
	if (pWaterCannon && pWaterCannon->m_regdVehicle && !pWaterCannon->m_regdVehicle->GetIsVisible())
	{
		return;
	}

	Mat34V boneMtx;
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pWaterCannon->m_regdVehicle, boneIndex);

	Vec3V vWldPos = Transform(boneMtx, vOffsetPos);

	if (CVfxHelper::GetDistSqrToCamera(vWldPos)>VFXRANGE_WATER_CANNON_JET_SQR)
	{
		return;
	}

	// register the fx system
	ptxEffectInst* pFxInst = NULL;

	if (registeredThisFrame)
	{
		bool justCreated;
		atHashWithStringNotFinal hashName = atHashWithStringNotFinal("water_cannon_jet",0x7E44F582);
		pFxInst = ptfxManager::GetRegisteredInst(pWaterCannon, PTFXOFFSET_WATERCANNON_JET, false, hashName, justCreated);

		float speedEvo = CVfxHelper::GetInterpValue(vehSpeed, 0.0f, 20.0f);

		if (pFxInst)
		{
			pFxInst->SetOffsetPos(vOffsetPos);

			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

			ptfxAttach::Attach(pFxInst, pWaterCannon->m_regdVehicle, boneIndex);

			// check if the effect has just been created 
			if (justCreated)
			{
				// it has just been created - start it and set it's position
				pFxInst->Start();
			}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			else
			{
				// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
				vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
			}
#endif
		}

		fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pWaterCannon, PTFXOFFSET_WATERCANNON_JET);
		g_AmbientAudioEntity.RegisterEffectAudio(hashName, effectUniqueID, RCC_VECTOR3(vWldPos), pWaterCannon->m_regdVehicle);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterCannonJetFx>(CPacketWaterCannonJetFx(boneIndex, vOffsetPos, speedEvo), pWaterCannon->m_regdVehicle);
		}
#endif
	}
	else
	{
		// find the effect inst
		ptfxRegInst* pRegFxInst = ptfxReg::Find(pWaterCannon, PTFXOFFSET_WATERCANNON_JET);
		if (pRegFxInst)
		{
			pFxInst = pRegFxInst->m_pFxInst;
		}
	}

	// set the collision plane
	if (pFxInst && collOccurred)
	{
		g_ptFxManager.GetColnInterface().RegisterUserPlane(pFxInst, vCollPosition, vCollNormal, 5.0f, 0);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// UpdatePtFxWaterCannonSpray
/////////////////////////////////////////////////////////////////////////////////

void			CVfxWater::UpdatePtFxWaterCannonSpray			(CWaterCannon* pWaterCannon, Vec3V_In vPosition, Vec3V_In vNormal, Vec3V_In vDir)
{
	// don't process if the vehicle is invisible
	if (pWaterCannon && pWaterCannon->m_regdVehicle && !pWaterCannon->m_regdVehicle->GetIsVisible())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(vPosition)>VFXRANGE_WATER_CANNON_SPRAY_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pWaterCannon, PTFXOFFSET_WATERCANNON_SPRAY, false, atHashWithStringNotFinal("water_cannon_spray",0xC4881C2F), justCreated);

	if (pFxInst)
	{
		Vec3V vPos, vRight, vForward, vUp;
		vPos =  vPosition;
		vUp =  vNormal;
		vForward = vDir;
		vRight = Cross(vForward, vUp);
		vRight = Normalize(vRight);
		vForward = Cross(vUp, vRight);
		vForward = Normalize(vForward);

		Mat34V fxMat;
		fxMat.SetCol0(vRight);
		fxMat.SetCol1(vForward);
		fxMat.SetCol2(vUp);
		fxMat.SetCol3(vPos);

		pFxInst->SetBaseMtx(fxMat);

		// evos
		float angleDot = Dot(vNormal, -vDir).Getf();
		float angleEvo = CVfxHelper::GetInterpValue(angleDot, WATER_CANNON_SPRAY_ANGLE_EVO_THRESH_MIN, WATER_CANNON_SPRAY_ANGLE_EVO_THRESH_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle", 0x0f3805b5), angleEvo);

		float distEvo = 0.0f;
		if (pWaterCannon && pWaterCannon->m_regdVehicle)
		{
			float dist = Dist(pWaterCannon->m_regdVehicle->GetTransform().GetPosition(), vPosition).Getf();
			distEvo = CVfxHelper::GetInterpValue(dist, WATER_CANNON_SPRAY_DIST_EVO_THRESH_MIN, WATER_CANNON_SPRAY_DIST_EVO_THRESH_MAX);
		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("dist", 0xd27ac355), distEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWaterCannonSprayFx>(CPacketWaterCannonSprayFx(fxMat, angleEvo, distEvo), pWaterCannon->m_regdVehicle);
		}
#endif
	}
}







