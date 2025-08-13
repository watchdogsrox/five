//
// filename:	vehicleDamage.cpp
// description:	
//
 
// C headers

#if __PS3
// PS3 headers
#include <altivec.h>
#include <ppu_altivec_internals.h>
#endif
// Rage headers
#include "grcore/config.h"
#include "system/floattoint.h"
#include "system/timemgr.h"
#include "system/xtl.h"
#include "system/param.h"
#include "fwdebug/picker.h"
#include "math/intrinsics.h"

// Microsoft headers
#if __D3D
	#include "system/d3d9.h"
#endif

// Rage headers
#include "fragment/cacheManager.h"
#include "fragment/instance.h"
#include "fragment/type.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "grcore/image.h"
#include "grcore/texture.h"
#include "phbound/boundcomposite.h"
#include "phbound/boundgeom.h"
#include "phBound/support.h"
#include "system/param.h"
#include "fwdebug/picker.h"
#include "creature/componentskeleton.h"	// CVehicleDamage::ReattachAllPanels
#include "creature/creature.h"			// CVehicleDamage::ReattachAllPanels

// Game headers
#include "audio/vehicleaudioentity.h"
#include "audio/policescanner.h"
#include "audio/scannermanager.h"
#include "audio/scriptaudioentity.h"
#include "audio/speechaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/Gameplay/GameplayDirector.h"
#include "camera/cinematic/CinematicDirector.h"
#include "Control/replay/Replay.h"
#include "control/replay/ReplayBufferMarker.h"
#include "control/replay/ReplayInterfaceVeh.h"
#include "control/replay/vehicle/vehiclepacket.h"
#include "debug/debugglobals.h"
#include "event/events.h"
#include "event/EventDamage.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "frontend/pausemenu.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "Stats/StatsMgr.h"
#include "peds/popcycle.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "renderer/water.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/Automobile.h"
#include "vehicles/Bike.h"
#include "vehicles/Boat.h"
#include "vehicles/Heli.h"
#include "vehicles/transmission.h"
#include "vehicles/Planes.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicleDamage.h" //  Dw  - migrated form top of file
#include "vehicles/vehicleDamage_parser.h"
#include "vehicles/VehicleDefines.h"
#include "vehicles/VehicleFactory.h"
#include "vehicles/vehiclegadgets.h"
#include "vehicles/wheel.h"
#include "vehicles/Trailer.h"
#include "vehicles/Submarine.h"
#include "vehicles/Metadata/VehicleExplosionInfo.h"
#include "vfx/Systems/VfxVehicle.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "vfx/decals/DecalManager.h"
#include "shaders/CustomShaderEffectBase.h"
#include "system/controlMgr.h"
#include "system/TamperActions.h"
#include "task/Movement/TaskParachute.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "peds/Ped.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "scene/world/GameWorld.h"
#include "game/ModelIndices.h"
#include "Network/NetworkInterface.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkUtil.h"
#include "modelinfo/BaseModelInfo.h"

#if __BANK
#include "renderer/rendertargets.h"
#include "../game/script/commands_object.h"
#include "grcore/quads.h"
#endif

#if __D3D11
#include "grcore/texturedefault.h"
#endif

AI_VEHICLE_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()
WEAPON_OPTIMISATIONS()

#define NO_VEH_DAMAGE_PARAM		(!__FINAL)

#if NO_VEH_DAMAGE_PARAM
PARAM(novehicledamage, "No vehicle deformation.");
static bool bTestNoVehicleDeformation = FALSE;
#endif

// --- Defines ------------------------------------------------------------------
#define VEHICLE_DAMAGE_ON 1 
#include "modelinfo/vehiclemodelInfo.h"


#if VEHICLE_DEFORMATION_SOFT_CAR
static dev_float damageMultiply = 5.0f;
#endif // VEHICLE_DEFORMATION_SOFT_CAR

dev_float sfMinimumMassForCrushingVehiclesNetworkGame = 34000.0f;

#if VEHICLE_DEFORMATION_TIMING
	#include "system/timer.h"
	static float DTT_maxTime = 0.0f;
	static float DTT_minTime = 3000.0f;
	static float DTT_accumTime = 0.0f;

	static int DTT_maxImpact = 0;
	static int DTT_minImpact = 3000;
	static int DTT_accumImpact = 0;

	static int DTT_maxOverload = 0;
	static int DTT_minOverload = 3000;
	static int DTT_accumOverload = 0;

	static int DTT_timerHit = 0;
#endif // VEHICLE_DEFORMATION_TIMING

    PF_PAGE(VehicleDamageImpulsesPage, "Damage Impulses");
    PF_GROUP(VehicleDamageImpulses);
    PF_LINK(VehicleDamageImpulsesPage, VehicleDamageImpulses);

    PF_VALUE_FLOAT(DamageForce, VehicleDamageImpulses);
    PF_VALUE_FLOAT(DamageImpulse, VehicleDamageImpulses);
    PF_VALUE_FLOAT(DamageVelocity, VehicleDamageImpulses);

#define ENABLE_APPLY_DAMAGE_PF 0
#if ENABLE_APPLY_DAMAGE_PF
	PF_PAGE(VehicleDamageTimersPage, "Vehicle Damage Timers");
	PF_GROUP(VehicleDamageTimers);
	PF_LINK(VehicleDamageTimersPage, VehicleDamageTimers);
	PF_TIMER(ApplyDamage,VehicleDamageTimers);
	PF_TIMER(ApplyDeformations, VehicleDamageTimers);
	PF_TIMER(RecomputeOctantMap,VehicleDamageTimers);
	PF_TIMER(AddToPixel,VehicleDamageTimers);

	PF_PAGE(VehicleDamageCountersPage, "Vehicle Damage Counters");
	PF_GROUP(VehicleDamageCounters);
	PF_LINK(VehicleDamageCountersPage, VehicleDamageCounters);
	PF_COUNTER(ApplyDamage,VehicleDamageCounters);
	PF_COUNTER(ApplyDeformations, VehicleDamageCounters);
	PF_COUNTER(AddToPixel,VehicleDamageCounters);
	PF_COUNTER(RecomputeOctantMap,VehicleDamageCounters);
#endif // ENABLE_APPLY_DAMAGE_PF

IMPLEMENT_VEHICLE_DAMAGE_TUNABLES(CVehicleDamage, 0xD606BA84)
CVehicleDamage::Tunables CVehicleDamage::sm_Tunables;
// --- Constants ----------------------------------------------------------------

const float MIN_DAMAGE_RANGE = 4.975f; // tailgater = 0, min damage size multiplier
const float MAX_DAMAGE_RANGE = 68.8f - MIN_DAMAGE_RANGE; // jet = 1.0, max damage size multiplier

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------
static dev_float GlassDamageThreshold = 0.5f;
static dev_float DeformationRadiusThreshold = 0.01f;

// Rumble variables
dev_float DIST_TO_CONSIDER_MOV = 0.5f;
dev_float SPEED_TO_CONSIDER_MOV = 0.3f;

dev_float sfRumbleIntensModifier = 0.005f;
dev_float sfRumbleDurModifier = 5.0f;
dev_float sfMinRumbleDamThreshold = 4.0f;
dev_float sfRumbleDamageWhenHittingRumbleOnLightColWithObject = 3.5f;

dev_float sfMaxRumbleDur = 200;
dev_float sfMinRumbleDur = 80;

mthVecRand g_vehDeformationVecRandom;

// --- Static Globals -----------------------------------------------------------
#define NUM_GLASS_BONES (25)
#define NUM_LIGHT_BONES TAXI_IDX
static const eHierarchyId aGlassBones[NUM_GLASS_BONES] =
{
	VEH_HEADLIGHT_L,			// 00
	VEH_HEADLIGHT_R,			// 01
	VEH_TAILLIGHT_L,			// 02
	VEH_TAILLIGHT_R,			// 03
	VEH_INDICATOR_LF,			// 04
	VEH_INDICATOR_RF,			// 05
	VEH_INDICATOR_LR,			// 06
	VEH_INDICATOR_RR,			// 07
	VEH_BRAKELIGHT_L,			// 08
	VEH_BRAKELIGHT_R,			// 09
	VEH_BRAKELIGHT_M,			// 10
	VEH_REVERSINGLIGHT_L,		// 11
	VEH_REVERSINGLIGHT_R,		// 12
	VEH_NEON_L,					// 13
	VEH_NEON_R,					// 14
	VEH_NEON_F,					// 15
	VEH_NEON_B,					// 16
	VEH_EXTRALIGHT_1,			// 17
	VEH_EXTRALIGHT_2,			// 18
	VEH_EXTRALIGHT_3,			// 19
	VEH_EXTRALIGHT_4,			// 20
	
	VEH_EXTRA_5,				// 21
	VEH_EXTRA_6,				// 22
	VEH_EXTRA_9					// 23
};

float CVehicleDeformation::networkDamageModifiers[NUM_NETWORK_DAMAGE_DIRECTIONS][MAX_NET_DAMAGE_MODIFIERS] =
{
	{// FRONT_LEFT_DAMAGE
		0.05f,
		0.5f,
		1.0f,
		1.5f
	},
	{// FRONT_RIGHT_DAMAGE
		0.05f,
		0.5f,
		1.0f,
		1.5f
	},
	{// MIDDLE_LEFT_DAMAGE
		0.05f,
		0.5f,
		1.0f,
		1.5f
	},
	{// MIDDLE_RIGHT_DAMAGE
		0.05f,
		0.5f,
		1.0f,
		1.5f
	},
	{// REAR_LEFT_DAMAGE
		0.05f,
		0.1f,
		0.3f,
		1.0f
	},
	{// REAR_RIGHT_DAMAGE
		0.05f,
		0.1f,
		0.3f,
		1.0f
	},
};

// --- Static class members -----------------------------------------------------

#if USE_EDGE
	#define DAMAGE_ZERO float(0.0f)									// EDGE: RGB is in range <-128; 127>
#elif GPU_DAMAGE_WRITE_ENABLED
	#define DAMAGE_ZERO 0
#else
	#define DAMAGE_ZERO float(128.0f + YPACK*128.0f + ZPACK*128.0f)	// RGB is in range <0; 255>
#endif

atArray<VehTexCacheEntry>	CVehicleDeformation::ms_TextureCache;
bool CVehicleDeformation::ms_bUpdateDamageFromPhysicsThread		= true;

#if __BANK
	bool	CVehicleDeformation::ms_bShowTextureAndRenderTarget = false;
	float	CVehicleDeformation::ms_fDisplayTextureScale		= 3.0f;
	bool	CVehicleDeformation::ms_bDisplayTexCacheStats		= false;
	bool	CVehicleDeformation::ms_bDisplayDamageMap			= false;
	bool	CVehicleDeformation::ms_bDisplayDamageMult			= false;
	bool	CVehicleDeformation::ms_bForceDamageMapScale		= false;
	bool	CVehicleDeformation::ms_bFullDamage					= false;
	float	CVehicleDeformation::ms_fForcedDamageMapScale		= 1.0f;
	float	CVehicleDeformation::ms_fWeaponImpactDamageScale	= 0.0f;

	bank_float CVehicleDeformation::m_ImpactPositionLocal_X		 = 0.0f;
	bank_float CVehicleDeformation::m_ImpactPositionLocal_Y		 = 0.0f;
	bank_float CVehicleDeformation::m_ImpactPositionLocal_Z		 = 0.0f;
	bank_float CVehicleDeformation::m_Impulse_X					 = 0.0f;
	bank_float CVehicleDeformation::m_Impulse_Y					 = 0.0f;
	bank_float CVehicleDeformation::m_Impulse_Z					 = 0.0f;
	bank_float CVehicleDeformation::m_OffsetDamageToMods_X		 = 0.0f;
	bank_float CVehicleDeformation::m_OffsetDamageToMods_Y		 = 0.0f;
	bank_float CVehicleDeformation::m_OffsetDamageToMods_Z		 = 0.0f;
	bank_float CVehicleDeformation::m_DamageTextureOffset_X		 = 0.0f;
	bank_float CVehicleDeformation::m_DamageTextureOffset_Y		 = 0.0f;
	bank_float CVehicleDeformation::m_DamageTextureOffset_Z		 = 0.0f;
	bank_float CVehicleDeformation::ms_fDamageAmount			 = 20.0f;
	bank_float CVehicleDeformation::ms_fDamagePercent			 = 10.0f;
	bank_float CVehicleDeformation::ms_fContainerDropHeight		 = 10.0f;
	bool CVehicleDeformation::ms_bAutoFix						 = false;
	bool CVehicleDeformation::ms_bAutoSaveDamageTexture			 = false;
	bool CVehicleDeformation::ms_bUpdateBoundsEnabled			 = true;
	bool CVehicleDeformation::ms_bUpdateBonesEnabled			 = true;
	CVehicle *CVehicleDamage::ms_SourceVehicle					 = NULL;
	CVehicle *CVehicleDamage::ms_DestinationVehicle				 = NULL;

	u32 CVehicleDamage::g_DamageDebug[CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS] = {0,0,0,0,0,0};
	float CVehicleDamage::g_DamageValue[CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS] = {-1.0f,-1.0f,-1.0f,-1.0f,-1.0f,-1.0f};

	bool CVehicleDamage::ms_bDisplayDamageVectors				 = false;
	bool	CVehicleDeformation::ms_bHidePropellers				 = false;
	float	CVehicleDeformation::ms_fForcedPropellerSpeed		 = 0.0f;

	ScalarV CVehicleDeformation::ms_fVehDefColVarMultMin		= VEHICLE_DEFORMATION_COLOR_VAR_MULT_MIN;
	ScalarV CVehicleDeformation::ms_fVehDefColVarMultMax		= VEHICLE_DEFORMATION_COLOR_VAR_MULT_MAX;
	ScalarV CVehicleDeformation::ms_fVehDefColVarAddMin			= VEHICLE_DEFORMATION_COLOR_VAR_ADD_MIN;
	ScalarV CVehicleDeformation::ms_fVehDefColVarAddMax			= VEHICLE_DEFORMATION_COLOR_VAR_ADD_MAX;
	
	void CVehicleDamage::SetSourceVehicleCB()
	{
		ms_SourceVehicle = CVehicleDebug::GetFocusVehicle();
	}

	void CVehicleDamage::SetDestinationVehicleCB()
	{
		ms_DestinationVehicle = CVehicleDebug::GetFocusVehicle();
	}

	void CVehicleDamage::DisplayDamageImpulse(const Vector3& impulseDirection, const Vector3& impulsePosition, CVehicle* vehicle, float damageRadius, bool inLocalCoordinates, int framesToLive)
	{
		if (vehicle == NULL)
		{
			return;
		}
		
		Vector3 vecDamageEndWorld   = inLocalCoordinates ? VEC3V_TO_VECTOR3(vehicle->GetTransform().Transform(VECTOR3_TO_VEC3V(impulsePosition))) : impulsePosition;

		Vector3 normalizedImpulse = impulseDirection;
		normalizedImpulse.NormalizeSafe();

		Vector3 vecDamageStartWorld = vecDamageEndWorld - normalizedImpulse;

		grcDebugDraw::Sphere(vecDamageEndWorld, damageRadius * 0.1f, Color_red, false, framesToLive);
		grcDebugDraw::Sphere(vecDamageStartWorld, 0.1f, Color_green, false, framesToLive);
		grcDebugDraw::Line(vecDamageStartWorld, vecDamageEndWorld, Color_green, Color_red, framesToLive);
	}

	void CVehicleDamage::DamageCurrentCar()
	{
		Vector3 impulseLocal = Vector3(CVehicleDeformation::m_Impulse_X, CVehicleDeformation::m_Impulse_Y, CVehicleDeformation::m_Impulse_Z);
		Vector3 localPosition = Vector3(CVehicleDeformation::m_ImpactPositionLocal_X, CVehicleDeformation::m_ImpactPositionLocal_Y, CVehicleDeformation::m_ImpactPositionLocal_Z);
		DamageCurrentCar(impulseLocal, localPosition, CVehicleDeformation::ms_fDamageAmount, CVehicleDeformation::ms_bAutoFix);
	}

	void CVehicleDamage::DamageCurrentCar(const Vector3& impulseLocal, const Vector3& damagePosLocal, float damage, bool autoFix)
	{
		SetSourceVehicleCB();

		if ((ms_SourceVehicle == NULL) || (damage < 0.01f))
		{
			return;
		}

		if (autoFix)
		{
			ms_SourceVehicle->Fix();
		}

		const float fMass = ms_SourceVehicle->GetMass();
		const float fMassIndependenceFactor = (fMass / ms_SourceVehicle->pHandling->m_fDeformationDamageMult);
		const float fDeformationMag = damage * fMassIndependenceFactor;
		
		DamageVehicleByDriver(impulseLocal, damagePosLocal, fDeformationMag, DAMAGE_TYPE_UNKNOWN, ms_SourceVehicle, CVehicleDeformation::ms_bFullDamage);

		ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformations(true);
	}

	void CVehicleDamage::DamageCurrentCarByPercentage()
	{
		SetSourceVehicleCB();

		if (ms_SourceVehicle == NULL)
		{
			return;
		}

		Vector3 impulseLocal = Vector3(CVehicleDeformation::m_Impulse_X, CVehicleDeformation::m_Impulse_Y, CVehicleDeformation::m_Impulse_Z);
		Vector3 localPosition = Vector3(CVehicleDeformation::m_ImpactPositionLocal_X, CVehicleDeformation::m_ImpactPositionLocal_Y, CVehicleDeformation::m_ImpactPositionLocal_Z);

		float totalVehicleHealth = ms_SourceVehicle->GetMaxHealth();
		CVehicleDeformation::ms_fDamageAmount = totalVehicleHealth * CVehicleDeformation::ms_fDamagePercent / 100.0f;

		DamageCurrentCar(impulseLocal, localPosition, CVehicleDeformation::ms_fDamageAmount, CVehicleDeformation::ms_bAutoFix);
	}

	void CVehicleDamage::DamageDriverSideRoof()
	{
		CVehicleDeformation::m_Impulse_X = 0.0f;
		CVehicleDeformation::m_Impulse_Y = 0.0f;
		CVehicleDeformation::m_Impulse_Z = -1.0f;

		CVehicleDeformation::m_ImpactPositionLocal_X = -0.44f;
		CVehicleDeformation::m_ImpactPositionLocal_Y = -0.1f;
		CVehicleDeformation::m_ImpactPositionLocal_Z = 0.5f;

		DamageCurrentCar();
	}

	static bool randomizeCollisions = true;

	void CVehicleDamage::HeadOnCollision()
	{
		SetSourceVehicleCB();

		if (ms_SourceVehicle == NULL)
		{
			return;
		}

		HeadOnCollision(ms_SourceVehicle, CVehicleDeformation::ms_fDamageAmount, randomizeCollisions);
	}

	void CVehicleDamage::RearEndCollision()
	{
		SetSourceVehicleCB();

		if (ms_SourceVehicle == NULL)
		{
			return;
		}

		RearEndCollision(ms_SourceVehicle, CVehicleDeformation::ms_fDamageAmount, randomizeCollisions);
	}

#if VEHICLE_DEFORMATION_PROPORTIONAL
	void CVehicleDamage::UpdateDamageMultiplier()
	{
		SetSourceVehicleCB();

		if (ms_SourceVehicle == NULL)
		{
			return;
		}
		
		void *basePtr = ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture() ? ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead|grcsWrite) : NULL;

		if (basePtr)
		{
			ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->CalculateDamageMultiplier();
			ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->HandleDamageAdded(basePtr, true, true, true, true, true, true);
			ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
		}
	}
#endif
	
	void CVehicleDamage::TailRotorCollision()
	{
		CVehicleDeformation::m_Impulse_X = 1.0f;
		CVehicleDeformation::m_Impulse_Y = 1.0f;
		CVehicleDeformation::m_Impulse_Z = 1.0f;

		CVehicleDeformation::m_ImpactPositionLocal_X = 0.0f;
		CVehicleDeformation::m_ImpactPositionLocal_Y = -10.0f;
		CVehicleDeformation::m_ImpactPositionLocal_Z = 2.72f;

		DamageCurrentCar();
	}

	void CVehicleDamage::LeftSideCollision()
	{
		CVehicleDeformation::m_Impulse_X = 1.0f;
		CVehicleDeformation::m_Impulse_Y = 0.0f;
		CVehicleDeformation::m_Impulse_Z = 0.0f;

		CVehicleDeformation::m_ImpactPositionLocal_X = -0.44f;
		CVehicleDeformation::m_ImpactPositionLocal_Y = 2.0f;
		CVehicleDeformation::m_ImpactPositionLocal_Z = 0.2f;

		DamageCurrentCar();
	}

	void CVehicleDamage::RightSideCollision()
	{
		CVehicleDeformation::m_Impulse_X = -1.0f;
		CVehicleDeformation::m_Impulse_Y = 0.0f;
		CVehicleDeformation::m_Impulse_Z = 0.0f;

		CVehicleDeformation::m_ImpactPositionLocal_X = 0.44f;
		CVehicleDeformation::m_ImpactPositionLocal_Y = 2.0f;
		CVehicleDeformation::m_ImpactPositionLocal_Z = 0.2f;

		DamageCurrentCar();
	}


	void CVehicleDamage::ImplodeSubmarine()
	{
		SetSourceVehicleCB();
		
		if ((ms_SourceVehicle == NULL) || (ms_SourceVehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINE))
		{
			return;
		}

		CSubmarineHandling* subHandling = ms_SourceVehicle->GetSubHandling();

		if( subHandling )
		{
			static bool shouldImplodeFully = false;
			subHandling->Implode(ms_SourceVehicle, shouldImplodeFully);
		}
	}

	void CVehicleDamage::RandomSmash()
	{
		SetSourceVehicleCB();

		if ((ms_SourceVehicle == NULL) || (CVehicleDeformation::ms_fDamageAmount < 0.01f))
		{
			return;
		}

		if (CVehicleDeformation::ms_bAutoFix)
		{
			ms_SourceVehicle->Fix();
		}

		AddVehicleExplosionDeformations(ms_SourceVehicle, NULL, DAMAGE_TYPE_EXPLOSIVE, CVehicleDeformation::ms_iExtraExplosionDeformations, CVehicleDeformation::ms_fExtraExplosionsDamage);
	}

	void CVehicleDamage::NetworkSmashDebug()
	{
		SetSourceVehicleCB();

		if ((ms_SourceVehicle == NULL) || (CVehicleDeformation::ms_fDamageAmount < 0.01f))
		{
			return;
		}

		if (CVehicleDeformation::ms_bAutoFix)
		{
			ms_SourceVehicle->Fix();
		}
		ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformationsFromNetwork(g_DamageDebug);
	}

	void CVehicleDamage::NetworkSmashDebugRandom()
	{
		for (int i = 0; i < 6; ++i)
		{
			g_DamageDebug[i] = (int)fwRandom::GetRandomNumberInRange(0.0f, 3.999f);
		}

		NetworkSmashDebug();
	}

	void CVehicleDamage::UpdateNetworkDamageDebug()
	{
		SetSourceVehicleCB();

		if (ms_SourceVehicle == NULL)
		{
			for(int i=0;i<CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS;i++)
			{
				g_DamageValue[i] = -1.0f;
			}
		}
		else
		{
			for(int i=0;i<CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS;i++)
			{
				g_DamageValue[i] = ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->GetNetworkDamage(i);
			}
		}
	}

	void CVehicleDamage::RemoveWheel()
	{
		SetSourceVehicleCB();

		CAutomobile* automobile = dynamic_cast<CAutomobile*>(ms_SourceVehicle);
		if ((ms_SourceVehicle == NULL) || (ms_SourceVehicle->GetVehicleType() != VEHICLE_TYPE_CAR) || (automobile == NULL))
		{
			return;
		}

		s32 numWheels = automobile->GetNumWheels();

		if (numWheels == 0)
		{
			return;
		}

		static int wheelId = 0;
		float fxChance = 0.0f;
		float deleteChance = 1.0f;
		float restoreTireChance = 0.0f;

		ms_SourceVehicle->GetVehicleDamage()->BreakOffWheel(wheelId, fxChance, deleteChance, restoreTireChance);

		++wheelId;
		wheelId %= numWheels;
	}

	void CVehicleDamage::RemoveHelicopterTail()
	{
		SetSourceVehicleCB();

		CRotaryWingAircraft* helicopter = dynamic_cast<CRotaryWingAircraft*>(ms_SourceVehicle);

		if ((ms_SourceVehicle == NULL) || (ms_SourceVehicle->GetVehicleType() != VEHICLE_TYPE_HELI) || (helicopter == NULL))
		{
			return;
		}

		helicopter->BreakOffTailBoom();
	}

	void CVehicleDamage::RemoveHelicopterPropellers()
	{
		SetSourceVehicleCB();

		CRotaryWingAircraft* helicopter = dynamic_cast<CRotaryWingAircraft*>(ms_SourceVehicle);

		if ((ms_SourceVehicle == NULL) || (ms_SourceVehicle->GetVehicleType() != VEHICLE_TYPE_HELI) || (helicopter == NULL))
		{
			return;
		}

		static bool mainPropRemove = true;

		if (mainPropRemove)
			helicopter->BreakOffMainRotor();

		static bool rearPropRemove = true;

		if (rearPropRemove)
			helicopter->BreakOffRearRotor();
	}

	void CVehicleDamage::DropFiveTonContainer()
	{
		SetSourceVehicleCB();

		if ((ms_SourceVehicle == NULL) || (CVehicleDeformation::ms_fContainerDropHeight < 0.0f))
		{
			return;
		}

		if (CVehicleDeformation::ms_bAutoFix)
		{
			ms_SourceVehicle->Fix();
		}

		int newContainerIndex = 0;
		int containerID = 874602658;

		const Matrix34& matrix = RCC_MATRIX34(ms_SourceVehicle->GetMatrixRef());

		Vector3 createPosWorldSpace;
		Vector3 createPosLocalSpace = Vector3(CVehicleDeformation::m_ImpactPositionLocal_X, CVehicleDeformation::m_ImpactPositionLocal_X, CVehicleDeformation::m_ImpactPositionLocal_Z + CVehicleDeformation::ms_fContainerDropHeight);
		matrix.Transform(createPosLocalSpace, createPosWorldSpace);

		object_commands::ObjectCreationFunction(containerID, createPosWorldSpace.x, createPosWorldSpace.y, createPosWorldSpace.z, false, true, 0, newContainerIndex, true, true, false);

		float mass = 5000.0f;
		float gravityFactor = -1.0f;
		scrVector vecTranslationDamping;
		scrVector vecRotationDamping;

		static CObject *pObj = NULL;
		
		if (pObj == NULL)
		{
			pObj = CTheScripts::GetEntityToModifyFromGUID<CObject>(newContainerIndex);

			if (pObj)
			{	
				rage::phInst* instance = pObj->GetCurrentPhysicsInst();

				if (!instance)
				{
					pObj->ActivatePhysics();
					instance = pObj->GetCurrentPhysicsInst();
				}

				if(instance)
				{
					CBaseModelInfo *pModelInfo = pObj->GetBaseModelInfo();

					// check if object still has original physics archetype
					if(instance->GetArchetype() == pModelInfo->GetPhysics())
					{
						// if so then we need to clone it so that we can modify values
						phArchetypeDamp* pNewArchetype = static_cast<phArchetypeDamp*>(instance->GetArchetype()->Clone());
						// pass new archetype to the objects physinst -> should have 1 ref only
						instance->SetArchetype(pNewArchetype);
					}

					phArchetypeDamp* pGtaArchetype = static_cast<phArchetypeDamp*>(instance->GetArchetype());

					if (pGtaArchetype)
					{
						pGtaArchetype->SetMass(mass);

						if (gravityFactor != -1.0f)
							pGtaArchetype->SetGravityFactor(gravityFactor);

						if(vecTranslationDamping.x > -1.0f)
							pGtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_C, Vector3(vecTranslationDamping.x, vecTranslationDamping.x, vecTranslationDamping.x));
						if(vecTranslationDamping.y > -1.0f)
							pGtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(vecTranslationDamping.y, vecTranslationDamping.y, vecTranslationDamping.y));
						if(vecTranslationDamping.z > -1.0f)
							pGtaArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(vecTranslationDamping.z, vecTranslationDamping.z, vecTranslationDamping.z));

						if(vecRotationDamping.x > -1.0f)
							pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(vecRotationDamping.x, vecRotationDamping.x, vecRotationDamping.x));
						if(vecRotationDamping.y > -1.0f)
							pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(vecRotationDamping.y, vecRotationDamping.y, vecRotationDamping.y));
						if(vecRotationDamping.z > -1.0f)
							pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(vecRotationDamping.z, vecRotationDamping.z, vecRotationDamping.z));

						Vector3 nudge(0.0f, 0.0f, -0.2f);
						pObj->ClearBaseFlag(fwEntity::IS_FIXED);
						pObj->SetVelocity(nudge);
					}
				}
			}
		}
		else
		{
			pObj->Teleport(createPosWorldSpace);
			Vector3 nudge(0.0f, 0.0f, -0.2f);
			pObj->SetVelocity(nudge);
		}
	}

	void CVehicleDamage::DuplicateDamageCB()
	{
		CVehicleDamage::Copy(ms_SourceVehicle, ms_DestinationVehicle);
	}

	void CVehicleDamage::SaveDamageTexture()
	{
		SetSourceVehicleCB();

		if (!ms_SourceVehicle)
		{
			return;
		}

		CVehicleDeformation::SaveDamageTexture(ms_SourceVehicle, false);
	}

	void CVehicleDamage::LoadDamageTexture()
	{
		SetSourceVehicleCB();
		CVehicleDeformation::LoadDamageTexture(ms_SourceVehicle);
	}

	void CVehicleDamage::FixDamageForTests()
	{
		SetSourceVehicleCB();

		if (ms_SourceVehicle)
		{
			ms_SourceVehicle->Fix();
		}
	}

	void CVehicleDamage::BreakOffWheelsCB()
	{
		if(ms_SourceVehicle && ms_SourceVehicle->GetVehicleDamage())
		{
			for(u32 index = 0; index < ms_SourceVehicle->GetNumWheels(); index++)
			ms_SourceVehicle->GetVehicleDamage()->BreakOffWheel(index);
		}
	}

#endif //bank


void CVehicleDamage::HeadOnCollision(CVehicle* pVehicle, float damage, bool randomize)
{
	if (pVehicle == NULL)
	{
		return;
	}

	Vector3 boundingBoxMax = pVehicle->GetChassisBoundMax(true);
	
	Vector3 damagePosLocal = Vector3(0.0f, boundingBoxMax.y, 0.0f);
	Vector3 impulseLocal   = Vector3(0.0f, -1.0f, 0.0f);
	
	if (randomize)
	{
		Vector3 boundingBoxMin = pVehicle->GetChassisBoundMin(true);
		float xRange = (boundingBoxMax - boundingBoxMin).x;
		float randomVal = fwRandom::GetRandomNumberInRange(-xRange, xRange);
		damagePosLocal.x -= randomVal * 0.75f;

		static float angleMax = 30.0f / 360.0f;
		impulseLocal.RotateZ(fwRandom::GetRandomNumberInRange(-angleMax, angleMax));
	}

	const float fMass = pVehicle->GetMass();
	const float fMassIndependenceFactor = pVehicle->pHandling->m_fDeformationDamageMult < 0.00001f ? fMass: (fMass / pVehicle->pHandling->m_fDeformationDamageMult);
	const float fDeformationMag = damage * fMassIndependenceFactor;

	DamageVehicle(NULL, impulseLocal, damagePosLocal, fDeformationMag, DAMAGE_TYPE_UNKNOWN, pVehicle, true);
}


void CVehicleDamage::RearEndCollision(CVehicle* pVehicle, float damage, bool randomize)
{
	if (pVehicle == NULL)
	{
		return;
	}

	Vector3 boundingBoxMin = pVehicle->GetChassisBoundMin(true);
	Vector3 impulseLocal   = Vector3(0.0f, 1.0f, 0.0f);
	Vector3 damagePosLocal = Vector3(0.0f, boundingBoxMin.y, 0.0f);

	if (randomize)
	{
		Vector3 boundingBoxMax = pVehicle->GetChassisBoundMin(true);
		float xRange = (boundingBoxMax - boundingBoxMin).x;
		float randomVal = fwRandom::GetRandomNumberInRange(-xRange, xRange);
		damagePosLocal.x -= randomVal * 0.75f;

		static float angleMax = 30.0f / 360.0f;
		impulseLocal.RotateZ(fwRandom::GetRandomNumberInRange(-angleMax, angleMax));
	}

	const float fMass = pVehicle->GetMass();
	const float fMassIndependenceFactor = (pVehicle->pHandling->m_fDeformationDamageMult < 0.00001f ? fMass : (fMass / pVehicle->pHandling->m_fDeformationDamageMult));
	const float fDeformationMag = damage * fMassIndependenceFactor;

	DamageVehicle(NULL, impulseLocal, damagePosLocal, fDeformationMag, DAMAGE_TYPE_UNKNOWN, pVehicle, true);
}


#if VEHICLE_DEFORMATION_INVERSE_SQUARE_FIELD
dev_float sfVehDefColRadiusMult		= 0.75f;
dev_float sfVehDefColRadiusMax		= 0.75f;
#else
dev_float sfVehDefColRadiusMult		= 0.5f;
dev_float sfVehDefColRadiusMax		= 0.5f;
#endif

bank_float CVehicleDeformation::ms_fVehDefColMultX			= 0.15f;
bank_float CVehicleDeformation::ms_fVehDefColMultY			= 0.06f;
bank_float CVehicleDeformation::ms_fVehDefColMultYNeg		= 0.08f;
bank_float CVehicleDeformation::ms_fVehDefColMultZ			= 0.12f;
bank_float CVehicleDeformation::ms_fVehDefColMultZNeg		= 0.10f;
bank_float CVehicleDeformation::ms_fVehDefRoofColMultZNeg	= 0.15f;
bank_float CVehicleDeformation::ms_fDeformationCarRoofMinZ	= -0.25f;
bank_float CVehicleDeformation::ms_fDeformationSuperCarRoofMinZ = -0.04f;
bank_float CVehicleDeformation::ms_fDeformationPlaneHeadMaxZ = 0.25f;
bank_float CVehicleDeformation::ms_fRollcageMinZ			= -0.05f;
bank_bool  CVehicleDeformation::ms_bEnableRoofZDeformationClamping  = true;
bank_float CVehicleDeformation::ms_fCarModBoneDeformMult	= 1.0f;
int CVehicleDeformation::ms_iExtraExplosionDeformations		= MAX_IMPACTS_PER_VEHICLE_PER_FRAME - 3; // allow 1 to be from a rocket, and 2 for front / rear extra damage points
bank_float CVehicleDeformation::ms_fExtraExplosionsDamage	= 20.0f;
bank_float CVehicleDeformation::m_sfVehDefColMax1			= 0.5f;
bank_float CVehicleDeformation::m_sfVehDefColMax2			= 1.2f;
bank_float CVehicleDeformation::m_sfVehDefColMult2			= 0.5f;
bank_float CVehicleDeformation::ms_fLargeVehicleRadiusMultiplier = 0.6f;
bank_bool CVehicleDeformation::ms_bEnableExtraBoneDeformations = true;

#if VEHICLE_DEFORMATION_PROPORTIONAL
bank_float CVehicleDeformation::ms_fDamageMagnitudeMult = GTA_VEHICLE_DAMAGE_DELTA_SCALE;
#endif

float CVehicleDeformation::ms_SmoothedPerlinNoise[VEHICLE_DEFORMATION_NOISE_HEIGHT][VEHICLE_DEFORMATION_NOISE_WIDTH_EXPANDED] ;

__forceinline float Interpolate(float x0, float x1, float alpha)
{
	return x0 * (1 - alpha) + alpha * x1;
}
__forceinline Vec4V_Out Interpolate(Vec4V_In x0, Vec4V_In x1, Vec4V_In alpha)
{
	return Lerp(alpha, x0, x1);
}

void GenerateSmoothNoise(float smoothNoise[VEHICLE_DEFORMATION_NOISE_HEIGHT][VEHICLE_DEFORMATION_NOISE_WIDTH], float baseNoise[VEHICLE_DEFORMATION_NOISE_HEIGHT][VEHICLE_DEFORMATION_NOISE_WIDTH], int octave)
{
	int samplePeriod = 1 << octave; // calculates 2 ^ k
	float sampleFrequency = 1.0f / samplePeriod;

	for (int i = 0; i < VEHICLE_DEFORMATION_NOISE_HEIGHT; i++)
	{
		//calculate the vertical sampling indices
		int sample_i0 = (i / samplePeriod) * samplePeriod;
		int sample_i1 = (sample_i0 + samplePeriod) % VEHICLE_DEFORMATION_NOISE_HEIGHT; //wrap around
		float vertical_blend  = (i - sample_i0) * sampleFrequency;

		for (int j = 0; j < VEHICLE_DEFORMATION_NOISE_WIDTH; j++)
		{
			//calculate the horizontal sampling indices
			int sample_j0 = (j / samplePeriod) * samplePeriod;
			int sample_j1 = (sample_j0 + samplePeriod) % VEHICLE_DEFORMATION_NOISE_WIDTH; //wrap around
			float horizontal_blend = (j - sample_j0) * sampleFrequency;


			//blend the top two corners
			float top = Interpolate(baseNoise[sample_i0][sample_j0],
				baseNoise[sample_i1][sample_j0], horizontal_blend);

			//blend the bottom two corners
			float bottom = Interpolate(baseNoise[sample_i0][sample_j1],
				baseNoise[sample_i1][sample_j1], horizontal_blend);

			//final blend
			smoothNoise[i][j] = Interpolate(top, bottom, vertical_blend);
		}
	}
}

void CVehicleDeformation::InitSmoothedPerlinNoise()
{
	float whiteNoise[VEHICLE_DEFORMATION_NOISE_HEIGHT][VEHICLE_DEFORMATION_NOISE_WIDTH];

	for (int x=0; x < VEHICLE_DEFORMATION_NOISE_HEIGHT; ++x )
	{
		for (int y=0; y < VEHICLE_DEFORMATION_NOISE_WIDTH; ++y )
		{		
			ms_SmoothedPerlinNoise[x][y] = 0.0f;
			whiteNoise[x][y] = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		}
	}

	//generate smooth noise
	float smoothNoise[VEHICLE_DEFORMATION_NOISE_SMOOTHING_OCTAVES][VEHICLE_DEFORMATION_NOISE_HEIGHT][VEHICLE_DEFORMATION_NOISE_WIDTH];
	const int octaveCount = VEHICLE_DEFORMATION_NOISE_SMOOTHING_OCTAVES;
	for(int octaveIndex=0; octaveIndex < octaveCount; ++octaveIndex )
	{
		GenerateSmoothNoise(smoothNoise[octaveIndex], whiteNoise, octaveIndex);
	}
	
	const float persistance			= 0.5f;
	float amplitude					= 1.0f;
	float totalAmplitude			= 0.0f;

	//blend noise together
	for (int octave = octaveCount - 1; octave >= 0; octave--)
	{
		amplitude *= persistance;
		totalAmplitude += amplitude;

		for (int i = 0; i < VEHICLE_DEFORMATION_NOISE_HEIGHT; i++)
		{
			for (int j = 0; j < VEHICLE_DEFORMATION_NOISE_WIDTH; j++)
			{
				ms_SmoothedPerlinNoise[i][j] += smoothNoise[octave][i][j] * amplitude;
			}
		}
	}

	//normalization
	for (int i = 0; i < VEHICLE_DEFORMATION_NOISE_HEIGHT; i++)
	{
		for (int j = 0; j < VEHICLE_DEFORMATION_NOISE_WIDTH; j++)
		{
			ms_SmoothedPerlinNoise[i][j] /= totalAmplitude;
		}
	}

	//Duplicate the first 3 columns for the last
	for (int i = 0; i < VEHICLE_DEFORMATION_NOISE_HEIGHT; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			ms_SmoothedPerlinNoise[i][VEHICLE_DEFORMATION_NOISE_WIDTH + j] = ms_SmoothedPerlinNoise[i][j];
		}
	}

#if GPU_DAMAGE_WRITE_ENABLED
	CApplyDamage::InitializePerlinNoiseTexture();// this texture consumes the data generated here
#endif
}

#if __BANK
static void copyscan_image(u8 *dest, void *base, int row, int width, int stride, u8 *gammalut) 
{
	u32 *src = (u32*)((char*)base + row * stride);
	while (width--) 
	{
		u32 n = *src++;
		dest[0] = gammalut[u8(n >> 16)];
		dest[1] = gammalut[u8(n >> 8)];
		dest[2] = gammalut[u8(n)];
		dest += 3;
	}
}

bool SavePNG(const char *filename, int width, int height, void* buffer, int stride) 
{
	return grcImage::WritePNG(filename, copyscan_image, width, height, buffer, stride, 0);
}


bool SaveJPEG(const char *filename, int width, int height, u8* buffer)
{
	grcImage* image = grcImage::Create(width, height, 1, grcImage::A8R8G8B8, grcImage::STANDARD, 0, 0);

	for (int y=0; y < height; y++ )
	{
		for (int x=0; x < width; x++ )
		{
			int offset = (x + y * width) * 4;

			Color32 color;
			color.SetRed(buffer[offset+1]);
			color.SetGreen(buffer[offset+2]);
			color.SetBlue(buffer[offset+3]);
			
			image->SetPixel(x, y, color.GetColor());
		}
	}

	bool okSave = image->SaveJPEG(filename, 100);
	image->Release();
	return okSave;
}


bool LoadJPEG(const char *filename, int width, int height, u8* buffer)
{
	grcImage* image = grcImage::LoadJPEG(filename);

	if (image == NULL)
	{
		return false;
	}

	for (int y=0; y < height; y++ )
	{
		for (int x=0; x < width; x++ )
		{
			int offset = (x + y * width) * 3;

			Color32 color = image->GetPixelColor32(x, y);
			
			buffer[offset]   = color.GetRed();
			buffer[offset+1] = color.GetGreen();
			buffer[offset+2] = color.GetBlue();
		}
	}

	image->Release();
	return true;
}

int filenameCounter = 0;
#define MAX_DAMAGE_TEXTURES_PER_VEHICLE_TYPE 1

char* GetDamageTextureFilename(CVehicle* vehicle)
{
	if (vehicle == NULL)
	{
		return NULL;
	}

	size_t digitLength = (filenameCounter == 0) ? 1 : (size_t)log10((double)filenameCounter) + 1;
	size_t characterCount = strlen("DamageTexture__.jpg") + strlen(vehicle->GetModelName()) + digitLength + 1; // null terminated
	char* filenameBuf = rage_new char[characterCount];
	filenameBuf[characterCount - 1] = 0;

	sprintf(filenameBuf, "DamageTexture_%s_%d.jpg", vehicle->GetModelName(), filenameCounter);

	++filenameCounter;
	filenameCounter %= MAX_DAMAGE_TEXTURES_PER_VEHICLE_TYPE;
	return filenameBuf;
}

void CVehicleDeformation::SaveDamageTexture(CVehicle* vehicle, bool skipSaveIfSameAsLast)
{
	if((vehicle == NULL) || !vehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
	{
		return;
	}

	static CVehicle* lastVehicle = NULL;

	if (skipSaveIfSameAsLast && (vehicle == lastVehicle))
	{
		return;
	}

	lastVehicle = vehicle;

	void *basePtr = vehicle->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead);
	if (basePtr)
	{
		int width  = GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH;
		int height = GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT;

		int floatCount = (width * height);
		int byteCount  = (floatCount * 4);
	
		unsigned char* tempColourBuf = rage_new unsigned char[byteCount]; //Used for random Gaussian noise and also as a buffer to save the texture.

	#if 0
		//To save the noise texture map.
		int stride = 4 * width;
		int byteIndex = 0;

	#if GPU_DAMAGE_WRITE_ENABLED && 0
		grcTexture* pNoiseTexture = CApplyDamage::GetNoiseTexture();

		if (pNoiseTexture)
		{
			grcTextureLock lock;
			if (pNoiseTexture->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock))
			{
				for (int y=0; y < height; y++ )
				{
					for (int x=0; x < width; x++ )
					{
						const float* pNoiseVal = TEXTUREOFFSET((const float*)lock.Base, x % VEHICLE_DEFORMATION_NOISE_WIDTH, y % VEHICLE_DEFORMATION_NOISE_HEIGHT);

						float val = *pNoiseVal;
						val *= 128.0f;
						val = Clamp(val, 0.0f, 255.0f);

						tempColourBuf[byteIndex]   = (unsigned char)val;
						tempColourBuf[byteIndex+1] = (unsigned char)val;
						tempColourBuf[byteIndex+2] = (unsigned char)val;
						tempColourBuf[byteIndex+3] = 255;

						byteIndex += 4;
					}
				}

				pNoiseTexture->UnlockRect(lock);
			}
		}
	
	#else
		for (int y=0; y < height; y++ )
		{
			for (int x=0; x < width; x++ )
			{
				float val = 128.0f;
				val *= GetSmoothedPerlinNoise(x, y).Getf();
				val = Clamp(val, 0.0f, 255.0f);

				tempColourBuf[byteIndex]   = (unsigned char)val;
				tempColourBuf[byteIndex+1] = (unsigned char)val;
				tempColourBuf[byteIndex+2] = (unsigned char)val;
				tempColourBuf[byteIndex+3] = 255;

				byteIndex += 4;

			}
		}
	#endif
		const char *noiseFilename = "DamageNoise.png";
		SavePNG(noiseFilename, width, height, tempColourBuf, stride);
	#endif

		for (int y=0; y < height; y++ )
		{
			for (int x=0; x < width; x++ )
			{	
				Vec3V_Out vDamage = ReadFromPixel(basePtr, x, y);
				Vector3 damage = VEC3V_TO_VECTOR3(vDamage); // -1 to 1

				if (    IsNanInMemory(&damage.x) || !FPIsFinite(damage.x) || (abs(damage.x) > GTA_VEHICLE_MAX_DAMAGE_RESOLUTION)
					 || IsNanInMemory(&damage.y) || !FPIsFinite(damage.y) || (abs(damage.y) > GTA_VEHICLE_MAX_DAMAGE_RESOLUTION)
					 || IsNanInMemory(&damage.z) || !FPIsFinite(damage.z) || (abs(damage.z) > GTA_VEHICLE_MAX_DAMAGE_RESOLUTION)   )
				{
					Assert(false);
					return;
				}

				damage *= 128.0f;
				damage += Vector3(128.0f, 128.0f, 128.0f);

				unsigned char red   = (unsigned char) (Clamp(damage.x, 0.0f, 255.0f));
				unsigned char green = (unsigned char) (Clamp(damage.y, 0.0f, 255.0f));
				unsigned char blue  = (unsigned char) (Clamp(damage.z, 0.0f, 255.0f));

				int offset = (x + y * width) * 4;
				tempColourBuf[offset]     = 255;
				tempColourBuf[offset + 1] = red;
				tempColourBuf[offset + 2] = green;
				tempColourBuf[offset + 3] = blue;
			}
		}

		char* filenameJPG = GetDamageTextureFilename(vehicle);
		SaveJPEG(filenameJPG, width, height, tempColourBuf);
		delete filenameJPG;
		delete tempColourBuf;

		vehicle->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
	}
}



void CVehicleDeformation::LoadDamageTexture(CVehicle* vehicle)
{
	if(vehicle == NULL)
	{
		return;
	}

	int width  = GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH;
	int height = GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT;
	int floatCount = (width * height);
	int byteCount  = (floatCount * 3);

	unsigned char* tempColourBuf = rage_new unsigned char[byteCount];
	bool okLoadImage = false;

	//Try to find the next image in the series
	for (int i = 0; i < MAX_DAMAGE_TEXTURES_PER_VEHICLE_TYPE; i++)
	{
		char* filenameJPG = GetDamageTextureFilename(vehicle);
		okLoadImage = LoadJPEG(filenameJPG, width, height, tempColourBuf);
		delete filenameJPG;

		if (okLoadImage)
		{
			break;
		}
	}
	
	if (!okLoadImage)
	{
		//Nothing found.
		delete tempColourBuf;
		return;
	}

	if (!vehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
	{
		vehicle->GetVehicleDamage()->GetDeformation()->DamageTextureAllocate(false); // don't reset the damage here, it can cause problems due to async damage resetting using GPU, erasing what we are about to read in.
		Assert(vehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture());
	}

	void *basePtr = vehicle->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead|grcsWrite);

	if (basePtr)
	{
		for (int x=0; x < width; x++ )
		{
			for (int y=0; y < height; y++ )
			{
				int offset = (x + y * width) * 3;

				unsigned char red   = tempColourBuf[offset+2];
				unsigned char green = tempColourBuf[offset+1];
				unsigned char blue  = tempColourBuf[offset];

				Vector3 damage = Vector3((float)red, (float)green, (float)blue);
				damage += Vector3(-128.0f, -128.0f, -128.0f);
				damage /= 128.0f;
				WriteToPixel(basePtr, damage, x, y);
			}
		}

		vehicle->GetVehicleDamage()->GetDeformation()->HandleDamageAdded(basePtr, true, true, true, true, true, true);
		vehicle->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
	}
	delete tempColourBuf;
}

#endif


// --- Code ---------------------------------------------------------------------
#if VEHICLE_DEFORMATION_TIMING
void VD_DisplayTiming()
{
	static int StartX = 100;
	static int StartY = 10;

	int y = StartY;

	char debugText[50];

	sprintf(debugText,"Total Damage Processed : %4d",DTT_timerHit);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tMin Time : %f",DTT_minTime);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tMax Time : %f",DTT_maxTime);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tAvg Time : %f",DTT_accumTime / (float)DTT_timerHit);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tMin Impact Applied : %d",DTT_minImpact);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tMax Impact Applied : %d",DTT_maxImpact);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tAvg Impact Applied : %d",(int)((float)DTT_accumImpact / (float)DTT_timerHit));
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tMin Impact Found : %d",DTT_minOverload);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tMax Impact Found : %d",DTT_maxOverload);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;

	sprintf(debugText,"\tAvg Impact Found : %d",(int)((float)DTT_accumOverload / (float)DTT_timerHit));
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;
}
#endif

CVehicleDeformation::CVehicleDeformation()
{
	m_pParentVehicle = NULL;
	m_nTextureCacheIdx = -1;
	
	ClearStoredImpacts();
#if VEHICLE_DEFORMATION_TIMING
	m_nImpactFound = 0;
#endif // VEHICLE_DEFORMATION_TIMING

	m_fBoundRadiusScaledInv = 1.0f;

	memset(m_networkDamages,0,sizeof(m_networkDamages));
}

void CVehicleDeformation::Init(CVehicle* pVehicle)
{
#if NO_VEH_DAMAGE_PARAM
	if(PARAM_novehicledamage.Get() || bTestNoVehicleDeformation)
	{
		bTestNoVehicleDeformation=TRUE;
		return;		// return before the damage texture is hooked up
	}
#endif

	m_pParentVehicle = NULL; //ensure this is cleared out and initialized properly below

	if(pVehicle->GetDrawable()==NULL)
	{
#if __ASSERT
		Assertf(0,"CVehicleDeformation::Init GetDrawable==NULL therefore mpParentVehicle will be NULL and ApplyDeformations calls will not work after this.");
#else
		Warningf("CVehicleDeformation::Init GetDrawable==NULL therefore mpParentVehicle will be NULL and ApplyDeformations calls will not work after this.");
#endif
		return;
	}

	// only to deformation on cars (automobiles) for the moment
	if(!pVehicle->m_nVehicleFlags.bUseDeformation)
		return;

// if define is off then variables won't get initialised and all other functions will skip out if trying to apply damage
#if VEHICLE_DAMAGE_ON
	m_pParentVehicle = pVehicle;
	CVehicleModelInfo *pVMI = m_pParentVehicle->GetVehicleModelInfo();

	Assert(pVMI->GetDamageOffsetScale() > 0.0f);
	m_fBoundRadiusScaledInv = 1.0f / ( pVMI->GetBoundingSphereRadius() / pVMI->GetDamageOffsetScale() );
	
	CalculateDamageMultiplier();

#if ADD_PED_SAFE_ZONE_TO_VEHICLES
	crSkeletonData* pSkeletonData = pVMI->GetHDFragType() ? pVMI->GetHDFragType()->GetCommonDrawable()->GetSkeletonData() : pVMI->GetFragType()->GetCommonDrawable()->GetSkeletonData();
	Vector3 boundBoxMin = m_pParentVehicle->GetChassisBoundMin(false);
	Vector3 boundBoxMax = m_pParentVehicle->GetChassisBoundMax(false);
	m_PedSafeZone.ReComputePedSafeArea(pSkeletonData, boundBoxMin, boundBoxMax);
#endif

#endif //VEHICLE_DAMAGE_ON
}

void CVehicleDeformation::Shutdown()
{
	DamageTextureFree();

	m_pParentVehicle = NULL;
	m_fBoundRadiusScaledInv = 1.0f;
}

// convert from a normalized offset into a texture coordinate in UV coordinates or in Pixels (default)
Vec::Vector_4V_Out CVehicleDeformation::GetDamageTexCoordFromOffset(Vec::Vector_4V_In vecLocalNormalisedOffset, bool inPixels /* = true */)
{
	using namespace Vec;
	Vector_4V localOffset = vecLocalNormalisedOffset;
	Vector_4V zoffset = V4SplatZ(localOffset);
	Vector_4V half = V4VConstant(V_HALF);
	zoffset = V4SubtractScaled(half, half, zoffset);

	// store 2d component of offset
	Vector_4V texSampleCoords = V4And(localOffset, V4VConstant(V_MASKXY));

	// and re-normalize just this (will get scaled by previously normalized z)
	// since we have zeroed out z, texSampleCoords could now be zero magnitude, need to handle this
	texSampleCoords = V4Normalize(texSampleCoords);
	texSampleCoords = V4And(texSampleCoords, V4IsNotNanV(texSampleCoords));

	Vector_4V add = inPixels ? V4VConstantSplat<FLOAT_TO_INT(GTA_VEHICLE_DAMAGE_TEXTURE_SIZE*0.5f)>() : V4VConstantSplat<FLOAT_TO_INT(0.5f)>();
	Vector_4V mul = V4Scale(zoffset, add);
	texSampleCoords = V4AddScaled(add, mul, texSampleCoords);

	return texSampleCoords;
}

// convert from a texture coordinate into a normalized offset
Vector3 CVehicleDeformation::GetDamageOffsetFromTexCoord(const Vector3& vecTexSampleCoords, bool bCoordsInPixels)
{
	Vector3 texSampleCoords(vecTexSampleCoords);
	texSampleCoords.z = 0.0f;

	if(bCoordsInPixels)
		texSampleCoords /= GTA_VEHICLE_DAMAGE_TEXTURE_SIZE;

	texSampleCoords.x -= 0.5f;
	texSampleCoords.y -= 0.5f;
	texSampleCoords *= 2.0f;

	Vector3 localOffset(0.0f, 0.0f, 0.0f);

	localOffset.z = texSampleCoords.Mag();
	float fXYMag = localOffset.z;

	if(localOffset.z > 1.0f)
		localOffset.z = 1.0f;

	localOffset.z = 1.0f - 2.0f * localOffset.z;

	if(localOffset.z < 1.0f && localOffset.z > -1.0f)
	{
		if(fXYMag > 0.0f)
			fXYMag = rage::Sqrtf(1.0f - localOffset.z*localOffset.z) / fXYMag;
	}
	else
	{
		fXYMag = 0.0f;
	}

	localOffset.x = texSampleCoords.x*fXYMag;
	localOffset.y = texSampleCoords.y*fXYMag;

	return localOffset;
}

#if ADD_PED_SAFE_ZONE_TO_VEHICLES

ScalarV_Out CVehicleDeformation::GetPedSafeAreaMultiplier(Vec3V_In vecOffset) const
{
	return m_PedSafeZone.GetPedSafeAreaMultiplier(vecOffset);
}

void CVehicleDeformation::GetPedSafeAreaMinMax(Vector3& safeMin, Vector3& safeMax, Vector3& unSafeMin, Vector3& unSafeMax) const
{
	m_PedSafeZone.GetPedSafeAreaMinMax(safeMin, safeMax, unSafeMin, unSafeMax);
}

#endif // ADD_PED_SAFE_ZONE_TO_VEHICLES

#if __PPU
Vec3V_Out CVehicleDeformation::ReadFromVectorOffset(const void* base, Vec3V_In vecOffset, bool UNUSED_PARAM(interpolate))
{
	if (!base)
	{
		return Vec3V(VEC3_ZERO);
	}

	using namespace Vec;

	// This could be a rubbish result (if vecOffset is zero length), but thats ok, we'll mask it out later
	Vector_4V offset = vecOffset.GetIntrin128();
	Vector_4V offsetMag2 = V3MagSquaredV(offset);
	Vector_4V invOffsetMag = V4InvSqrt(offsetMag2);
	Vector_4V normOffset = V4Scale(offset, invOffsetMag);
	Vector_4V isValid = V4IsFiniteV(invOffsetMag);

	Vector_4V texCoord = GetDamageTexCoordFromOffset(normOffset);

	union { Vector_4V i; u32 u[4]; } v;

	Vector_4V texCoordInt = (Vector_4V)vec_vctuxs((vec_float4)texCoord, 0);
	v.i = texCoordInt;

	Vector_4V offsetMag = V4Scale(offsetMag2, invOffsetMag);
	Vector_4V damageDeltaScale = GetDamageMultiplierScalar();
	Vector_4V damageDeltaScaleDivRadius = V4Scale(damageDeltaScale, GetBoundRadiusScaledInvV().GetIntrin128());
	offsetMag = V4Min(damageDeltaScale, V4Scale(offsetMag, damageDeltaScaleDivRadius));

	Vector_4V texCoordFloor = (Vector_4V)vec_vcfux((vec_uint4)texCoordInt, 0);
	Vector_4V texCoordFrac  = V4Subtract(texCoord, texCoordFloor);

	Vector_4V dx = V4SplatX(texCoordFrac);
	Vector_4V dy = V4SplatY(texCoordFrac);

	unsigned byteOffset = v.u[0]*3 + v.u[1]*3*GTA_VEHICLE_DAMAGE_TEXTURE_SIZE; // LHS here!
	unsigned char *quad0 = (unsigned char *)base + byteOffset;
	unsigned char *quad1 = quad0 + GTA_VEHICLE_DAMAGE_TEXTURE_SIZE*3;

	Vector_4V pel00 = (Vector_4V)vec_lvlx(0,  quad0);
	Vector_4V pel01 = (Vector_4V)vec_lvrx(16, quad0);
	Vector_4V pel10 = (Vector_4V)vec_lvlx(0,  quad1);
	Vector_4V pel11 = (Vector_4V)vec_lvrx(16, quad1);

	pel01 = V4Or(pel00, pel01); // QW contains 2 3byte PELs
	pel11 = V4Or(pel10, pel11);

	Vector_4V perm0 = V4VConstant<0x00101010,0x01101010,0x02101010,0x10101010>();   // Also used for 0 byte!!
	Vector_4V perm1 = V4VConstant<0x03101010,0x04101010,0x05101010,0x10101010>();

	// Extract bytes to MSB of word (using MSB means no sign extension required)
	pel00 = (Vector_4V)vec_vperm((vec_uchar16)pel01, (vec_uchar16)perm0, (vec_uchar16)perm0);
	pel01 = (Vector_4V)vec_vperm((vec_uchar16)pel01, (vec_uchar16)perm0, (vec_uchar16)perm1);
	pel10 = (Vector_4V)vec_vperm((vec_uchar16)pel11, (vec_uchar16)perm0, (vec_uchar16)perm0);
	pel11 = (Vector_4V)vec_vperm((vec_uchar16)pel11, (vec_uchar16)perm0, (vec_uchar16)perm1);

	// Convert back to floats with range adjust
	pel00 = V4IntToFloatRaw<31>(pel00);
	pel01 = V4IntToFloatRaw<31>(pel01);
	pel10 = V4IntToFloatRaw<31>(pel10);
	pel11 = V4IntToFloatRaw<31>(pel11);

	// Interpolate
	pel11 = V4SubtractScaled(pel11, pel11, dx);
	pel01 = V4SubtractScaled(pel01, pel01, dx);
	pel10 = V4AddScaled(pel11, pel10, dx);
	pel00 = V4AddScaled(pel01, pel00, dx);
	pel10 = V4SubtractScaled(pel10, pel10, dy);
	pel00 = V4AddScaled(pel10, pel00, dy);

	// do the same scaling as the shader to get the correct result
	Vector_4V vecResult = V4Scale(pel00, offsetMag);

	// return zero if input was invalid
	vecResult = V4And(vecResult, isValid);

	return Vec3V(vecResult);
}


void CVehicleDeformation::WriteToPixel(void *base, const Vector3& vecDamage, int x, int y)
{
	Assert(x >= 0 && x<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE && y>=0 && y<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE);

#if GPU_DAMAGE_WRITE_ENABLED

	TexelValue_R8G8B8A8_SNORM* pTexel = TEXTUREOFFSET((TexelValue_R8G8B8A8_SNORM *)base,x,y);
	Vector3 vecDamageMult = vecDamage;
	vecDamageMult *= GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;

	vecDamageMult.x = rage::Clamp(vecDamageMult.x, -128.0f, 127.0f); // clamp component-wise
	vecDamageMult.y = rage::Clamp(vecDamageMult.y, -128.0f, 127.0f);
	vecDamageMult.z = rage::Clamp(vecDamageMult.z, -128.0f, 127.0f);

	pTexel->red   = (s8)vecDamageMult.x;
	pTexel->green = (s8)vecDamageMult.y;
	pTexel->blue  = (s8)vecDamageMult.z;
	pTexel->alpha = (s8)127;

#else
	_ivector4 pel = vec_cts( vecDamage.xyzw,7 );			// Int between -128 and +127

#if USE_EDGE

	unsigned char *pByteEntry = (unsigned char *)base + (x*3 + (y*3*GTA_VEHICLE_DAMAGE_TEXTURE_SIZE));

	__builtin_altivec_stvebx( __builtin_altivec_vspltb( (_cvector4)pel,3 ),0,pByteEntry );
	__builtin_altivec_stvebx( __builtin_altivec_vspltb( (_cvector4)pel,7 ),1,pByteEntry );
	__builtin_altivec_stvebx( __builtin_altivec_vspltb( (_cvector4)pel,11 ),2,pByteEntry );


#else
	_ivector4 bias = { 128,128,128,0 };

	pel = vec_add( pel,bias );						// 0 - 255

	float *pFloatEntry = TEXTUREOFFSET((float *)base,x,y);

	_ivector4 perm = { 0x001b1713,0x001b1713,0x001b1713,0x001b1713 };

	pel = __builtin_altivec_vperm_4si( perm,pel,(_cvector4)perm );

	pel = (_ivector4)vec_ctf( pel,0 );		// Single float splatted across 4..

	__stvewx( pel,pFloatEntry,0 );	// Store single packed float

#endif

#endif // GPU_DAMAGE_WRITE_ENABLED
	
}


void CVehicleDeformation::AddToPixel(void* base, Vec3V_In vecDamage, int x, int y)
{
#if ENABLE_APPLY_DAMAGE_PF
	PF_FUNC(AddToPixel);
	PF_INCREMENT(AddToPixel);
#endif // ENABLE_APPLY_DAMAGE_PF
	Assertf(x >= 0 && x<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE && y>=0 && y<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE, "x/y is out of bounds : %d, %d", x, y);

#if USE_EDGE

	unsigned char *pByteEntry = (unsigned char *)base + (x*3 + (y*3*GTA_VEHICLE_DAMAGE_TEXTURE_SIZE));


	_ivector4 perm = { 0x000408ff,0xffffffff,0xffffffff,0xffffffff };

	_ivector4 pel = vec_or( (_ivector4)__lvlx( pByteEntry,0 ),(_ivector4)__lvrx( pByteEntry,16 ) );

	_ivector4 damage = vec_cts( vecDamage.GetIntrin128(),31 ); // Damage range between -128 and +127 in MSB

	damage = __builtin_altivec_vperm_4si( damage,perm,(_cvector4)perm ); // Place in first 3 bytes

	pel = (_ivector4)__builtin_altivec_vaddsbs( (_cvector4)pel,(_cvector4)damage );		// Add and clamp between -128 and +127

	__builtin_altivec_stvebx( __builtin_altivec_vspltb( (_cvector4)pel,0 ),0,pByteEntry );
	__builtin_altivec_stvebx( __builtin_altivec_vspltb( (_cvector4)pel,1 ),1,pByteEntry );
	__builtin_altivec_stvebx( __builtin_altivec_vspltb( (_cvector4)pel,2 ),2,pByteEntry );
#else

	_ivector4 perm = { 0x001b1713,0x001b1713,0x001b1713,0x001b1713 };
	_ivector4 splat = { 0x00111213,0x00111213,0x00111213,0x00111213 };
	_ivector4 sign = { 0x00808080,0x00808080,0x00808080,0x00808080 };

	float *pFloatEntry = TEXTUREOFFSET((float *)base,x,y);

	_ivector4 damage = vec_cts( vecDamage.GetIntrin128(),7 ); // Damage range between -128 and +127

	damage = __builtin_altivec_vperm_4si( perm,damage,(_cvector4)perm ); // Place across vector


	_ivector4 pel = (_ivector4)__lvlx( pFloatEntry,0 );

	pel = (_ivector4)vec_ctu( (__vector4)pel,0 );					// Get 24 bit int containing X,Y,Z

	pel = __builtin_altivec_vperm_4si( splat,pel,(_cvector4)splat );	// Splat input across 4 vectors

	pel = vec_xor( pel,sign );		// Convert excess128 to 2's complement

	pel = (_ivector4)__builtin_altivec_vaddsbs( (_cvector4)pel,(_cvector4)damage );		// Add and clamp between -128 and +127

	pel = vec_xor( pel,sign );		// Convert back to excess128

	pel = (_ivector4)vec_ctf( pel,0 );			// Back to floating point

	__stvewx( pel,pFloatEntry,0 );
	
#endif
}

#else // __PPU


Vec3V_Out CVehicleDeformation::ReadFromTexPosition(const void *base, Vec2V_In xy, bool bInterpolate)
{
	if (!base)
	{
		return Vec3V(VEC3_ZERO);
	}

	Assert(xy.GetXf() >= 0.0f && xy.GetXf()<float(GTA_VEHICLE_DAMAGE_TEXTURE_SIZE) && xy.GetYf()>=0.0f && xy.GetYf()<float(GTA_VEHICLE_DAMAGE_TEXTURE_SIZE));

	const Vec2V xyi = FloatToIntRaw<0>(xy);
	int xi = xyi.GetXi();
	int yi = xyi.GetYi();
	if(bInterpolate)
	{
		Vec3V sample   = ReadFromPixel(base, xi, yi);
		Vec3V sampleR  = ReadFromPixel(base, (xi + 1) & (GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1),	yi);
		Vec3V sampleU  = ReadFromPixel(base, xi,												(yi + 1) & (GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1));
		Vec3V sampleRU = ReadFromPixel(base, (xi + 1) & (GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1),	(yi + 1) & (GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1));

		Vec2V sampleOffset = xy - RoundToNearestIntZero(xy);

		Vec4V offsets = Vec4V(sampleOffset, Vec2V(V_ONE) - sampleOffset);

		Vec3V smoothedSample = sample* offsets.GetZ() * offsets.GetW();
		smoothedSample += sampleR*offsets.GetX()*offsets.GetW();
		smoothedSample += sampleU*offsets.GetZ()*offsets.GetY();
		smoothedSample += sampleRU*offsets.GetX()*offsets.GetY();

		return smoothedSample;
	}
	else
	{
		return ReadFromPixel(base, xi, yi);
	}
}

Vec3V_Out CVehicleDeformation::ReadFromVectorOffset(const void *base, Vec3V_In vecOffset, bool interpolate) const
{
	if (!base)
	{
		return Vec3V(VEC3_ZERO);
	}

	Vec3V vecNormalisedOffset = vecOffset;
	const ScalarV vecOffsetMagSq = MagSquared(vecOffset);

	ScalarV fOffsetMag(V_ZERO);
	if(IsGreaterThanAll(vecOffsetMagSq, ScalarVConstant<FLOAT_TO_INT(0.0001f*0.0001f)>()))
	{
		fOffsetMag = Sqrt(vecOffsetMagSq);
		vecNormalisedOffset /= fOffsetMag;
	}
	else
	{
		vecNormalisedOffset.ZeroComponents();
		return vecNormalisedOffset;
	}
	
	Vec3V vecTexOffset = Vec3V(CVehicleDeformation::GetDamageTexCoordFromOffset(vecNormalisedOffset.GetIntrin128()));
	Vec3V vecResult = ReadFromTexPosition(base, vecTexOffset.GetXY(), interpolate);

	// do the same scaling as the shader to get the correct result
	vecResult *= Min(ScalarV(V_ONE), fOffsetMag * GetBoundRadiusScaledInvV());
	vecResult *= GetDamageMultiplierScalar();

#if ADD_PED_SAFE_ZONE_TO_VEHICLES
	vecResult *= GetPedSafeAreaMultiplier(vecOffset);
#endif
	return vecResult;
}

void CVehicleDeformation::WriteToPixel(void *base, const Vector3& vecDamage, int x, int y)
{
	Assert(x >= 0 && x<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE && y>=0 && y<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE);

#if GPU_DAMAGE_WRITE_ENABLED
# if VEHICLE_DEFORMATION_USE_HALF_FLOATS
	TexelValue_A16B16G16R16F* pTexel = TEXTUREOFFSET((TexelValue_A16B16G16R16F *)base,x,y);
	pTexel->red   = Float16Compressor::compress(vecDamage.x);
	pTexel->green = Float16Compressor::compress(vecDamage.y);
	pTexel->blue  = Float16Compressor::compress(vecDamage.z);
	pTexel->alpha = Float16Compressor::compress(1.0);
# else
	TexelValue_R8G8B8A8_SNORM* pTexel = TEXTUREOFFSET((TexelValue_R8G8B8A8_SNORM *)base,x,y);

	Vector3 vecDamageMult = vecDamage;
	vecDamageMult *= GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;

	vecDamageMult.x = rage::Clamp(vecDamageMult.x, -128.0f, 127.0f); // clamp component-wise
	vecDamageMult.y = rage::Clamp(vecDamageMult.y, -128.0f, 127.0f);
	vecDamageMult.z = rage::Clamp(vecDamageMult.z, -128.0f, 127.0f);

	pTexel->red   = (s8)vecDamageMult.x;
	pTexel->green = (s8)vecDamageMult.y;
	pTexel->blue  = (s8)vecDamageMult.z;
	pTexel->alpha = (s8)127;
#endif //VEHICLE_DEFORMATION_USE_HALF_FLOATS

#else
	float *pFloatEntry = TEXTUREOFFSET((float *)base,x,y);

	int nEntryX = int(vecDamage.x * 128.0f + 128.0f);
	int nEntryY = int(vecDamage.y * 128.0f + 128.0f);
	int nEntryZ = int(vecDamage.z * 128.0f + 128.0f);

	*pFloatEntry = Pack(nEntryX, nEntryY, nEntryZ);
#endif
}

#define OLD_PACKING_CODE	(__PPU || __WIN32PC || RSG_DURANGO || RSG_ORBIS)
#define NEW_PACKING_CODE	__XENON
#define	USE_VECTOR4			0


void CVehicleDeformation::AddToPixel(void* base, Vec3V_In vecDamage, int x, int y)
{
#if ENABLE_APPLY_DAMAGE_PF
	PF_FUNC(AddToPixel);
	PF_INCREMENT(AddToPixel);
#endif // ENABLE_APPLY_DAMAGE_PF
	Assertf(x >= 0 && x<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE && y>=0 && y<GTA_VEHICLE_DAMAGE_TEXTURE_SIZE, "x/y is out of bounds : %d, %d", x, y);

#if GPU_DAMAGE_WRITE_ENABLED
# if VEHICLE_DEFORMATION_USE_HALF_FLOATS
	TexelValue_A16B16G16R16F* pTexel = TEXTUREOFFSET((TexelValue_A16B16G16R16F *)base,x,y);
	pTexel->red   = Float16Compressor::compress(Float16Compressor::decompress(pTexel->red)   + vecDamage.GetXf());
	pTexel->green = Float16Compressor::compress(Float16Compressor::decompress(pTexel->green) + vecDamage.GetYf());
	pTexel->blue  = Float16Compressor::compress(Float16Compressor::decompress(pTexel->blue)  + vecDamage.GetZf());
	pTexel->alpha = Float16Compressor::compress(1.0);
# else
	TexelValue_R8G8B8A8_SNORM* pTexel = TEXTUREOFFSET((TexelValue_R8G8B8A8_SNORM *)base,x,y);
	Vector3 vecBase = Vector3((f32)pTexel->red, (f32)pTexel->green, (f32)pTexel->blue);

	Vector3 vecNew = RCC_VECTOR3(vecDamage);
	vecNew *= GTA_VEHICLE_MAX_DAMAGE_RESOLUTION;
	vecNew += vecBase;

	vecNew.x = rage::Clamp(vecNew.x, -128.0f, 127.0f);
	vecNew.y = rage::Clamp(vecNew.y, -128.0f, 127.0f);
	vecNew.z = rage::Clamp(vecNew.z, -128.0f, 127.0f);

	pTexel->red   = (s8)vecNew.x;
	pTexel->green = (s8)vecNew.y;
	pTexel->blue  = (s8)vecNew.z;
	pTexel->alpha = (s8)127;
# endif //VEHICLE_DEFORMATION_USE_HALF_FLOATS

#else
	float *pFloatEntry = TEXTUREOFFSET((float *)base, x, y);

#if	OLD_PACKING_CODE
	float initialValue = *pFloatEntry;
#endif

#if	NEW_PACKING_CODE
	// unpack what's in the texture already
	// This could be done completely in the vector unit to avoid
	// a load-hit-store when first using the vector.
#if	USE_VECTOR4
	__vector4 vecResults;
#else
	Vector3 vecResults;
#endif
	//UnpackFast(initialValue, vecResults);
	static const __vector4 multipliers = {1/65536.0f, 256.0f, 0.0f, 0.0f};
	// Load the float value and scale it down by 65536
	__vector4 temp = __lvlx(pFloatEntry, 0) * multipliers;
	vecResults = __vspltw(__vrfiz(temp), VEC_PERM_X);	// Grab the z component
	__vector4 scale256 = __vspltw(multipliers, VEC_PERM_Y);
	__vector4 yz = (temp - vecResults) * scale256;
	__vector4 yVec = __vrfiz(yz);	// Grab the y component
	// Insert the y component. 4 means insert to y. 3 shift means rotate left 3 words, which takes x to y
	vecResults = __vrlimi(vecResults, yVec, 4, 3);
	// Grab and insert the z component
	// Insert the z component. 8 means insert to x. 0 shift means rotate left 0 words, which takes x to x.
	vecResults = __vrlimi(vecResults, (yz - yVec) * scale256, 8, 0);
#if	OLD_PACKING_CODE
	Vector3 unpacked = vecResults;
#endif

	// scale into -1.0 to 1.0 range
	static const __vector4 constants = {128.0f, 1/128.0f, -1.0f, 0.99f};
	const __vector4 sOffset = __vspltw(constants, VEC_PERM_X);
	const __vector4 sScale = __vspltw(constants, VEC_PERM_Y);
	//static const __vector4 sOffset = {128.0f, 128.0f, 128.0f, 128.0f};
	//static const __vector4 sScale = {1/128.0f, 1/128.0f, 1/128.0f, 1/128.0f};
	vecResults = (vecResults - sOffset) * sScale;

	// sum results and clamp limits
	vecResults += RCC_VECTOR3(vecDamage);
	const __vector4 vecMinimum = __vspltw(constants, VEC_PERM_Z);
	const __vector4 vecMaximum = __vspltw(constants, VEC_PERM_W);
	//static const __vector4 vecMinimum = {-1.0f, -1.0f, -1.0f, -1.0f};
	//static const __vector4 vecMaximum = {0.99f, 0.99f, 0.99f, 0.99f};
	vecResults = __vmaxfp(vecResults, vecMinimum);
	vecResults = __vminfp(vecResults, vecMaximum);

#if	OLD_PACKING_CODE
	Vector3 tempResults = vecResults;
#endif

	// scale back to -128 to 128
	vecResults = vecResults * sOffset + sOffset;

	// pack the result back into the pixel
	// Truncate to integral floating-point values in preparation
	// for packing
	vecResults = __vrfiz(vecResults);

	// Multiply the x, y, and z components by the appropriate scale
	// factors.
	static const __vector4 vecScaleFactors = {1.0f, YPACK, ZPACK, 0.0f};
	vecResults *= vecScaleFactors;

	// Add the components together and store them.
	// Need to splat all inputs (or y and z inputs and the output) because
	// stvewx is unpredictable about which element it will store.
	vecResults = __vspltw(vecResults, VEC_PERM_X) + __vspltw(vecResults, VEC_PERM_Y) + __vspltw(vecResults, VEC_PERM_Z);
	__stvewx(vecResults, pFloatEntry, 0);
#endif

#if	OLD_PACKING_CODE
	// unpack what's in the texture already
	Vector3 vecBase;
	vecBase.x = initialValue;
	Unpack(vecBase);
#if	NEW_PACKING_CODE
	FastAssert(vecBase.x == unpacked.x);
	FastAssert(vecBase.y == unpacked.y);
	FastAssert(vecBase.z == unpacked.z);
#endif

	// scale into -1.0 to 1.0 range
	vecBase -= Vector3(128.0f, 128.0f, 128.0f);
	vecBase /= 128.0f;

	// sum results and clamp limits
	Vector3 vecNew = vecBase + RCC_VECTOR3(vecDamage);
	vecNew.x = rage::Clamp(vecNew.x, -1.0f, 0.99f);
	vecNew.y = rage::Clamp(vecNew.y, -1.0f, 0.99f);
	vecNew.z = rage::Clamp(vecNew.z, -1.0f, 0.99f);

#if	NEW_PACKING_CODE
	FastAssert(tempResults.x == vecNew.x);
	FastAssert(tempResults.y == vecNew.y);
	FastAssert(tempResults.z == vecNew.z);
#endif

	// scale back to -128 to 128
	int nEntryX = int(vecNew.x * 128.0f + 128.0f);
	int nEntryY = int(vecNew.y * 128.0f + 128.0f);
	int nEntryZ = int(vecNew.z * 128.0f + 128.0f);

	// pack the result back into the pixel
	float result = Pack(nEntryX, nEntryY, nEntryZ);
#if	NEW_PACKING_CODE
	FastAssert(result == *pFloatEntry);
#else
	*pFloatEntry = result;
#endif
#endif

#endif //GPU_DAMAGE_WRITE_ENABLED
}
#endif // !__PPU...

inline Vec4V_Out Get4MagnitudesOfVec2FromCenter(Vec2V_In v0, Vec2V_In v1, Vec2V_In v2, Vec2V_In v3, Vec2V_In center)
{
	Vec2V vOffsetToCenter0 = v0 - center;
	Vec2V vOffsetToCenter1 = v1 - center;
	Vec2V vOffsetToCenter2 = v2 - center;
	Vec2V vOffsetToCenter3 = v3 - center;

	Vec4V magnitudes = Vec4V( MagFast(vOffsetToCenter0), MagFast(vOffsetToCenter1), MagFast(vOffsetToCenter2), MagFast(vOffsetToCenter3)); // could do these mag2 s component-wise in parallel too, not sure the appropriate calls

	return magnitudes;
}

ScalarV_Out CVehicleDeformation::GetTexRange(const Vector3& vecOffset, const float fRadius, bool inPixels)
{
	const Vec3V vVecOffset = RCC_VEC3V(vecOffset);
	const ScalarV vRadius = ScalarVFromF32(fRadius);
	// The clamping shouldn't be required as vecOffset has been normalized earlier, 
	// but we still somehow end up with values that are ever so slightly over 1.0f (say 1.00000012f) due to FP inaccuracies.
	ScalarV damageAlpha = Arcsin(Clamp(vVecOffset.GetZ(), ScalarV(V_ZERO), ScalarV(V_ONE)));
	ScalarV damageBeta = Arctan2(-vVecOffset.GetX(), vVecOffset.GetY());

	ScalarV minAlpha = damageAlpha - vRadius;
	ScalarV maxAlpha = damageAlpha + vRadius;
	ScalarV minBeta = damageBeta - vRadius;
	ScalarV maxBeta = damageBeta + vRadius;

	Vec3V damageLimits;
	Vec3V damageTexPos[5];

	// centre position
	damageTexPos[0] = Vec3V(CVehicleDeformation::GetDamageTexCoordFromOffset(vecOffset.xyzw, inPixels));

	Vec4V sincosMinMaxAlpha = Vec4V(SinCos(minAlpha), SinCos(maxAlpha));
	Vec4V sincosMinMaxBeta = Vec4V(SinCos(minBeta), SinCos(maxBeta));

	// min alpha, min beta
	damageLimits = Vec3V(sincosMinMaxAlpha.GetY() * -sincosMinMaxBeta.GetX(),
		sincosMinMaxAlpha.GetY() * sincosMinMaxBeta.GetY(),
		sincosMinMaxAlpha.GetX()
		);
	damageTexPos[1] = Vec3V(CVehicleDeformation::GetDamageTexCoordFromOffset(damageLimits.GetIntrin128(), inPixels));

	// max alpha, min beta
	damageLimits = Vec3V(sincosMinMaxAlpha.GetW() * -sincosMinMaxBeta.GetX(),
		sincosMinMaxAlpha.GetW() * sincosMinMaxBeta.GetY(),
		sincosMinMaxAlpha.GetZ()
		);
	damageTexPos[2] = Vec3V(CVehicleDeformation::GetDamageTexCoordFromOffset(damageLimits.GetIntrin128(), inPixels));

	// min alpha, max beta
	damageLimits = Vec3V(sincosMinMaxAlpha.GetY() * -sincosMinMaxBeta.GetZ(),
		sincosMinMaxAlpha.GetY() * sincosMinMaxBeta.GetW(),
		sincosMinMaxAlpha.GetX()
		);
	damageTexPos[3] = Vec3V(CVehicleDeformation::GetDamageTexCoordFromOffset(damageLimits.GetIntrin128(), inPixels));

	// max alpha, max beta
	damageLimits = Vec3V(sincosMinMaxAlpha.GetW() * -sincosMinMaxBeta.GetZ(),
		sincosMinMaxAlpha.GetW() * sincosMinMaxBeta.GetW(),
		sincosMinMaxAlpha.GetZ()
		);
	damageTexPos[4] = Vec3V(CVehicleDeformation::GetDamageTexCoordFromOffset(damageLimits.GetIntrin128(), inPixels));

	Vec2V damageTexMinXY = damageTexPos[0].GetXY();
	Vec2V damageTexMaxXY = damageTexPos[0].GetXY();
	for(int i=1; i<5; i++)
	{
		damageTexMinXY = Min(damageTexMinXY, damageTexPos[i].GetXY());
		damageTexMaxXY = Max(damageTexMaxXY, damageTexPos[i].GetXY());
	}

	ScalarV fTexRange = Max(damageTexPos[0].GetX() - damageTexMinXY.GetX(), damageTexMaxXY.GetX() - damageTexPos[0].GetX());
	fTexRange = Max(fTexRange, damageTexPos[0].GetY() - damageTexMinXY.GetY());
	fTexRange = Max(fTexRange, damageTexMaxXY.GetY() - damageTexPos[0].GetY());

	return fTexRange;
}

void CVehicleDeformation::ApplyDamageToCircularArea(void* base, const Vector3& vecDamage, const Vector3& vecOffset, const float fRadius)
{
	Assert(base);

	if (base == NULL || vecDamage.IsZero())
	{
		return;
	}

	const Vec3V vVecDamage = RCC_VEC3V(vecDamage);
	
	ScalarV fTexRange = GetTexRange(vecOffset, fRadius, true);
	ScalarV fTexRangeInv = Invert(fTexRange);

	const Vec4V vTexMinMaxRanges = Vec4V(-fTexRange, fTexRange, -fTexRange, fTexRange);
	Vec3V vDamageCenter = Vec3V(CVehicleDeformation::GetDamageTexCoordFromOffset(vecOffset.xyzw));
	Vec4V vDamageTexMinMaxXY = Vec4V(Vec::V4Permute<Vec::X, Vec::X, Vec::Y, Vec::Y>(vDamageCenter.GetIntrin128()));
	vDamageTexMinMaxXY = RoundToNearestIntZero(vDamageTexMinMaxXY + vTexMinMaxRanges);
	vDamageTexMinMaxXY += Vec4V(V_Y_AXIS_WONE);

	const Vec2V vDamageTexMin = Vec2V(vDamageTexMinMaxXY.GetX(), vDamageTexMinMaxXY.GetZ()); 
	const Vec4V viDamageTexMinMaxXY = FloatToIntRaw<0>(vDamageTexMinMaxXY);

	int iDamageTexMinX = viDamageTexMinMaxXY.GetXi();
	int iDamageTexMaxX = viDamageTexMinMaxXY.GetYi();
	int iDamageTexMinY = viDamageTexMinMaxXY.GetZi();
	int iDamageTexMaxY = viDamageTexMinMaxXY.GetWi();

	const ScalarV fDamageRange = Mag(vVecDamage);
	const ScalarV noiseScale = BANK_SWITCH(ms_fVehDefColVarAddMax, VEHICLE_DEFORMATION_COLOR_VAR_ADD_MAX);
	const ScalarV noiseFraction = Min(ScalarV(V_FLT_SMALL_1), noiseScale/fDamageRange);
	const ScalarV noiseRange =  Clamp(BANK_SWITCH(ms_fVehDefColVarMultMax, VEHICLE_DEFORMATION_COLOR_VAR_MULT_MAX) - BANK_SWITCH(ms_fVehDefColVarMultMin, VEHICLE_DEFORMATION_COLOR_VAR_MULT_MIN), 
		ScalarV(V_ZERO), ScalarV(V_ONE));

	Vec2V fxy = vDamageTexMin;
	Vec2V fxy1, fxy2, fxy3;
	for(int y=iDamageTexMinY; y<=iDamageTexMaxY; y++, fxy += Vec2V(V_Y_AXIS_WZERO))
	{
		// Wrap the coordinate around if passes the boundary
		int yWrap = rage::Wrap(y, 0, GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1);
		fxy.SetX(vDamageTexMin.GetX());
		for(int x=iDamageTexMinX; x<=iDamageTexMaxX; x+=4, fxy += Vec2V(ScalarV(V_FOUR), ScalarV(V_ZERO)))
		{
			fxy1 = fxy + Vec2V(V_X_AXIS_WZERO);
			fxy2 = fxy1 + Vec2V(V_X_AXIS_WZERO);
			fxy3 = fxy2 + Vec2V(V_X_AXIS_WZERO);

			Vec4V vOffsetToCenterMags = Get4MagnitudesOfVec2FromCenter(fxy, fxy1, fxy2, fxy3, vDamageCenter.GetXY());
			if(!IsGreaterThanOrEqualAll(vOffsetToCenterMags, Vec4V(fTexRange)))
			{
				Vec4V ratio = vOffsetToCenterMags * fTexRangeInv;
				Vec4V damageMult = Vec4V(V_ONE) - ratio;
				damageMult = Clamp(damageMult, Vec4V(V_ZERO), Vec4V(V_ONE));

#if VEHICLE_DEFORMATION_INVERSE_SQUARE_FIELD
				damageMult *= damageMult;
#endif

				Vec3V applyDamage[4] = {
					vVecDamage * damageMult.GetX(), 
					vVecDamage * damageMult.GetY(), 
					vVecDamage * damageMult.GetZ(), 
					vVecDamage * damageMult.GetW()
				};
				Vec4V randomNoise = GetSmoothedPerlinNoise(x,y);
				Vec4V noiseMult = (randomNoise * Vec4V(noiseRange)) + Vec4V(BANK_SWITCH(ms_fVehDefColVarMultMin, VEHICLE_DEFORMATION_COLOR_VAR_MULT_MIN));
				Vec4V noiseToAdd = (randomNoise - Vec4V(V_HALF)) * Vec4V(noiseFraction);
				noiseMult += noiseToAdd;

				// Wrap the coordinate around if passes the boundary
				int xWrap0 = rage::Wrap(x, 0, GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1);
				int xWrap1 = rage::Wrap(x+1, 0, GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1);
				int xWrap2 = rage::Wrap(x+2, 0, GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1);
				int xWrap3 = rage::Wrap(x+3, 0, GTA_VEHICLE_DAMAGE_TEXTURE_SIZE-1);
				AddToPixel(base, applyDamage[0] * noiseMult.GetX(), xWrap0, yWrap);
				AddToPixel(base, applyDamage[1] * noiseMult.GetY(), xWrap1, yWrap);
				AddToPixel(base, applyDamage[2] * noiseMult.GetZ(), xWrap2, yWrap);
				AddToPixel(base, applyDamage[3] * noiseMult.GetW(), xWrap3, yWrap);
			}
		}
	}
}

#define WINDOW_DAMAGE_USE_COLLISION 1
void CVehicleDeformation::ApplyDamageToWindows(const Vector3& vecPos, const float fRadius)
{
	CSubmarineHandling* subHandling = m_pParentVehicle->GetSubHandling();
	if( ( m_pParentVehicle->InheritsFromSubmarine() ||
		  m_pParentVehicle->InheritsFromSubmarineCar() ) &&
		!subHandling->IsUnderCrushDepth( m_pParentVehicle ) )
	{
		return;
	}

#if WINDOW_DAMAGE_USE_COLLISION
	// Go over all the windows and test of intersection with the deformation sphere
	const fragInst* pFragInst = m_pParentVehicle->GetFragInst();
	const phBound* pBound = pFragInst->GetArchetype()->GetBound();

	if (pBound->GetType() == phBound::COMPOSITE)
	{
		const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);

		for (int componentId = 0; componentId < pBoundComposite->GetNumBounds(); componentId++)
		{
			if (CVehicleGlassManager::IsComponentSmashableGlass(m_pParentVehicle, componentId))
			{
				const fragTypeChild* pFragChild = pFragInst->GetTypePhysics()->GetChild(componentId);
				const int boneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragChild->GetBoneID());

				if (boneIndex != -1)
				{
					// Do a sphere vs. box bound to see if the deformation effected this window
					phBound* pWindowBound = pBoundComposite->GetBound(componentId);
					Matrix34 toWorldMat;
					m_pParentVehicle->GetGlobalMtx(boneIndex, toWorldMat);

					TUNE_GROUP_FLOAT(VEHICLE_DAMAGE,WINDOW_COLLISION_AABB_MODIFICATION,-0.05f,-1.0f,1.0f,0.01f);
					const Vec3V windowBoundAABBModification = Vec3VFromF32(WINDOW_COLLISION_AABB_MODIFICATION);
					const Vec3V windowHalfExtents = Scale(pWindowBound->GetBoundingBoxSize(),ScalarV(V_HALF));
					const Vec3V windowShrunkHalfExtents = Max(Add(windowHalfExtents,windowBoundAABBModification),Vec3V(V_ZERO));
					const Vec3V windowCenter = Average(pWindowBound->GetBoundingBoxMax(),pWindowBound->GetBoundingBoxMin());
					if(pWindowBound && geomBoxes::TestSphereToBox(RCC_VEC3V(vecPos), ScalarVFromF32(fRadius), Subtract(windowCenter,windowShrunkHalfExtents), Add(windowCenter,windowShrunkHalfExtents), toWorldMat))
					{
						// Window is hit - add a new collision to the glass system
						VfxCollInfo_s vfxCollInfo;
						vfxCollInfo.vPositionWld	= Vec3V(V_FLT_MAX);
						vfxCollInfo.vNormalWld		= Vec3V(V_Z_AXIS_WZERO);
						vfxCollInfo.vDirectionWld	= Vec3V(V_Z_AXIS_WZERO);
						vfxCollInfo.regdEnt			= m_pParentVehicle;
						vfxCollInfo.materialId		= pWindowBound->GetMaterialId(0);
						vfxCollInfo.componentId		= componentId;
						vfxCollInfo.force			= 0.25f;
						vfxCollInfo.weaponGroup		= WEAPON_EFFECT_GROUP_EXPLOSION;
						vfxCollInfo.isBloody		= false;
						vfxCollInfo.isWater			= false;
						vfxCollInfo.isExitFx		= false;
						vfxCollInfo.noPtFx		   	= false;
						vfxCollInfo.noPedDamage   	= false;
						vfxCollInfo.noDecal	   		= false; 
						vfxCollInfo.isSnowball		= false;
						vfxCollInfo.isFMJAmmo		= false;

						g_vehicleGlassMan.StoreCollision(vfxCollInfo);
					}
				}
			}
		}
	}
#else
	// Notify the glass system about an explosion which will effect the windows that intersect with the sphere
	VfxExpInfo_s vfxExpInfo;
	vfxExpInfo.vPositionWld = RCC_VEC3V(vecPos);
	vfxExpInfo.regdEnt = m_pParentVehicle;
	vfxExpInfo.radius = fRadius / (GTA_VEHICLE_DAMAGE_DELTA_SCALE * GetBoundRadiusScaledInv());
	vfxExpInfo.radius *= 1.2f; // Make the damage radius a bit bigger
	vfxExpInfo.force = 0.5f;
	vfxExpInfo.flags = 0; // Do not force the glass to detach
	g_vehicleGlassMan.StoreExplosion(vfxExpInfo);
#endif
}

dev_float sfTakeLessDamageMult = 0.5f;
dev_float sfHitByPlayerMult = 2.0f;

#if __BANK
bool CVehicleDamage::ms_bNeverAvoidVehicleExplosionChainReactions = false;
bool CVehicleDamage::ms_bAlwaysAvoidVehicleExplosionChainReactions = false;

bool CVehicleDamage::ms_bUseVehicleExplosionBreakChanceMultiplierOverride = false;
float CVehicleDamage::ms_fVehicleExplosionBreakChanceMultiplierOverride = 0.0f;
#endif // BANK

bool CVehicleDamage::ms_bDisableVehiclePartCollisionOnBreak = false;
bool CVehicleDamage::ms_bDisableVehicleExplosionBreakOffParts = false;

float CVehicleDamage::ms_fArmorModMagnitude = 0.5f;
float CVehicleDamage::ms_fLooseLatchedDoorOpenAngle = 0.025f;
float CVehicleDamage::ms_fLooseLatchedBonnetOpenAngle = 0.05f;
bank_float bfFirstPersonViewDamageRadiusMult = 2.0f;
bank_float bfFirstPersonViewDamageDeltaMult = 1.5f;
bank_float bfDeformationThresholdMPMult = 2.0f;
bank_float bfDeformationThresholdAircraftMult = 5.0f;
dev_float dfDeformationThresholdGlassMult = 8.0f;
// called when an impact occurs
bool CVehicleDeformation::ApplyCollisionImpact(const Vector3& vecImpulseWorldSpace, const Vector3& vecPosWorldSpace, CEntity* pOtherEntity, bool bFullScaleDeformation, bool bShouldApplyDamageToGlass)
{
	if (NetworkInterface::IsGameInProgress())
	{
		if(!m_pParentVehicle || !m_pParentVehicle->GetVehicleDamage() || !m_pParentVehicle->GetVehicleDamage()->CanVehicleBeDamagedBasedOnDriverInvincibility())
			return false;
	}

#if !__FINAL
	if (gbStopVehiclesDamaging)
		return false;
#endif
#if NO_VEH_DAMAGE_PARAM
	if(bTestNoVehicleDeformation)
		return false;
#endif

	if(m_pParentVehicle==NULL)
		return false;

	if(!m_pParentVehicle->m_nVehicleFlags.bUseDeformation || !m_pParentVehicle->m_nVehicleFlags.bCanBeVisiblyDamaged)
		return false;

	// Do not deform the wrecked vehicle, as it will make the broken off parts' meshes differ from their bounds
	if(m_pParentVehicle->GetStatus() == STATUS_WRECKED && !m_pParentVehicle->HasBoundUpdatePending())
		return false;

	Vector3 damageDelta = vecImpulseWorldSpace;
	
	// script can reduce the damage taken by vehicles
	if(m_pParentVehicle->m_nVehicleFlags.bTakeLessDamage)
	{
		damageDelta.Scale(sfTakeLessDamageMult);
	}
	// if hit by the player, do more damage.
	else if(pOtherEntity && pOtherEntity->GetIsDynamic() && static_cast<CDynamicEntity*>(pOtherEntity)->GetStatus()==STATUS_PLAYER)
	{
		if(m_pParentVehicle->GetStatus()!=STATUS_PLAYER && !m_pParentVehicle->PopTypeIsMission())
			damageDelta.Scale(sfHitByPlayerMult);
	}
	
	damageDelta = VEC3V_TO_VECTOR3(m_pParentVehicle->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(damageDelta)));

	Assert(damageDelta.x == damageDelta.x && damageDelta.y == damageDelta.y && damageDelta.z == damageDelta.z);

	Vector3 objSpaceVecPos = VEC3V_TO_VECTOR3(m_pParentVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vecPosWorldSpace)));
	Vector3 damageOffset = objSpaceVecPos;
	float damageOffsetOriginalLength = 0.0f;
	if( Dot(damageOffset, damageOffset) > 0.00001f )
	{
		damageOffsetOriginalLength = damageOffset.Mag();
	}
	damageOffset.NormalizeSafe();
	damageOffset.w = damageOffsetOriginalLength;
	
	bool bPlayerFirstPersonView = m_pParentVehicle->GetStatus()==STATUS_PLAYER && m_pParentVehicle->GetDriver() && m_pParentVehicle->GetDriver()->IsPlayer() && camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera();
	if(bPlayerFirstPersonView)
	{
		// scale the deformation only if it's a head on collision
		bPlayerFirstPersonView = objSpaceVecPos.y > m_pParentVehicle->GetBoundingBoxMax().y * 0.9f
			&& Abs(objSpaceVecPos.x) < m_pParentVehicle->GetBoundingBoxMax().x * 0.5f;
	}
	const float fMass = m_pParentVehicle->GetMass();
	damageDelta *= m_pParentVehicle->pHandling->m_fDeformationDamageMult / fMass;

	float armourMultiplier = m_pParentVehicle->GetVariationInstance().GetArmourDamageMultiplier();
    damageDelta *= armourMultiplier;

#if	VEHICLE_DEFORMATION_PROPORTIONAL
	//Compensate for the fact that the shader is scaling up the damage magnitude by the size of the vehicle, so apply the inverse here. Typically in the [0.5, 1.0] range;
	damageDelta *= m_pParentVehicle->GetVehicleDamage()->GetDeformation()->GetDamageMultiplierInv();
#endif

	if(bPlayerFirstPersonView)
	{
		damageDelta *= bfFirstPersonViewDamageDeltaMult;
	}
	damageDelta.x *= ms_fVehDefColMultX;
	damageDelta.y *= damageDelta.y < 0.0f ? ms_fVehDefColMultY : ms_fVehDefColMultYNeg;
	if(damageOffset.z > m_pParentVehicle->GetBoundingBoxMax().z * 0.5f && damageDelta.z < 0.0f)
	{
		damageDelta.z *= ms_fVehDefRoofColMultZNeg;
	}
	else
	{
		damageDelta.z *= damageDelta.z < 0.0f ? ms_fVehDefColMultZ : ms_fVehDefColMultZNeg;
	}

#if VEHICLE_DEFORMATION_SOFT_CAR
	damageDelta *= damageMultiply;
#endif // VEHICLE_DEFORMATION_SOFT_CAR

	float fDamageDeltaMagOrig = damageDelta.Mag();
	float fDamageDeltaMag = fDamageDeltaMagOrig;

	if(!bFullScaleDeformation)
	{
		//Always clamp the damage magnitude
		if(damageDelta.x * damageOffset.x > 0.0f)
			damageDelta.x = 0.0f;
		if(damageDelta.y * damageOffset.y > 0.0f)
			damageDelta.y = 0.0f;
		if(damageDelta.z * damageOffset.z > 0.0f)
			damageDelta.z = 0.0f;

		fDamageDeltaMag = damageDelta.Mag();
		float fDamageMag = fDamageDeltaMag;
		if(fDamageMag > m_sfVehDefColMax1)
			fDamageMag = m_sfVehDefColMax1 + m_sfVehDefColMult2*(fDamageMag - m_sfVehDefColMax1);

		if(fDamageMag > m_sfVehDefColMax2)
			fDamageMag = m_sfVehDefColMax2;

		if(fDamageMag > GTA_VEHICLE_MIN_DAMAGE_RESOLUTION)
			damageDelta.Scale(fDamageMag / fDamageDeltaMag);

		fDamageDeltaMag = fDamageMag;
	}

	// if damage we wish to apply is smaller than the accuracy of what can be stored in the texture.
	float fThresholdMulti = NetworkInterface::IsGameInProgress() ? bfDeformationThresholdMPMult : 1.0f;
	if (m_pParentVehicle->InheritsFromPlane())
	{
		fThresholdMulti = Max(fThresholdMulti, bfDeformationThresholdAircraftMult);
	}
	if(rage::Abs(damageDelta.x) < (GTA_VEHICLE_MIN_DAMAGE_RESOLUTION * fThresholdMulti) && rage::Abs(damageDelta.y) < (GTA_VEHICLE_MIN_DAMAGE_RESOLUTION * fThresholdMulti) && rage::Abs(damageDelta.z) < (GTA_VEHICLE_MIN_DAMAGE_RESOLUTION * fThresholdMulti))
		return false;

	float damageRadius = fDamageDeltaMagOrig;
	damageRadius *= sfVehDefColRadiusMult;

	if(bPlayerFirstPersonView)
	{
		damageRadius *= bfFirstPersonViewDamageRadiusMult;
	}
	
	damageRadius = rage::Min(damageRadius, sfVehDefColRadiusMax);

	if (m_pParentVehicle->InheritsFromPlane() || m_pParentVehicle->InheritsFromHeli())
	{
		damageRadius *= ms_fLargeVehicleRadiusMultiplier;
	}

	if( damageRadius < (DeformationRadiusThreshold * fThresholdMulti) )
	{
		// Too small : we're out
		return false;
	}

#if __BANK
	if (CVehicleDamage::ms_bDisplayDamageVectors)
	{
		CVehicleDamage::DisplayDamageImpulse(vecImpulseWorldSpace, vecPosWorldSpace, m_pParentVehicle, damageRadius, false, 10);
	}
#endif

	// Flag the vehicle wheels about this damage, unless it was ped melee damage.
	if(!pOtherEntity || !pOtherEntity->GetIsTypePed())
	{
		m_pParentVehicle->FlagWheelsWithDeformationImpact(VECTOR3_TO_VEC3V(damageOffset), ScalarVFromF32(damageRadius));
	}
	
#if VEHICLE_DEFORMATION_TIMING
	m_nImpactFound++;
#endif // VEHICLE_DEFORMATION_TIMING

	int nStoreImpact = -1;
	if(m_nImpactStoreIdx < VEHICLE_DEFORMATION_IMPACT_STORE_SIZE)
	{
		nStoreImpact = m_nImpactStoreIdx;
		m_nImpactStoreIdx++;
	}
	else
	{
		float fSmallestMag2 = damageDelta.Mag2();
		for(int nFindImpact=0; nFindImpact < m_nImpactStoreIdx; nFindImpact++)
		{
			if(m_ImpactStore[nFindImpact].damage.Mag32() < fSmallestMag2)
			{
				nStoreImpact = nFindImpact;
				fSmallestMag2 = m_ImpactStore[nFindImpact].damage.Mag32();
			}
		}
	}

	// grab global damage map scale:
	CVehicleModelInfo *pVMI = m_pParentVehicle->GetVehicleModelInfo();
	Assert(pVMI);
	float fDamageMapScale = pVMI ? pVMI->GetDamageMapScale() : 1.0f;
	BANK_ONLY( if(CVehicleDeformation::ms_bForceDamageMapScale) {	fDamageMapScale = CVehicleDeformation::ms_fForcedDamageMapScale; } )
	damageDelta *= fDamageMapScale;

	if(nStoreImpact > -1)
	{
		Assert(damageDelta.x==damageDelta.x && damageDelta.y==damageDelta.y && damageDelta.z==damageDelta.z);
		m_ImpactStore[nStoreImpact].damage.SetVector3(damageDelta);

		Assert(damageRadius==damageRadius);
		m_ImpactStore[nStoreImpact].damage.w = damageRadius;

		Assert(damageOffset.x==damageOffset.x && damageOffset.y==damageOffset.y && damageOffset.z==damageOffset.z);
		m_ImpactStore[nStoreImpact].offset.SetVector3(damageOffset);
		m_ImpactStore[nStoreImpact].offset.SetW(damageOffset.w);

		if((fDamageDeltaMag > GTA_VEHICLE_MIN_DAMAGE_RESOLUTION * dfDeformationThresholdGlassMult) && bShouldApplyDamageToGlass)
		{
			ApplyDamageToWindows(vecPosWorldSpace, damageRadius);
		}

		// Disable spoiler if we get a large impact near the spoiler strut bones.
		int nStrutsBone = m_pParentVehicle->GetBoneIndex(VEH_STRUTS);
		if(nStrutsBone != -1)
		{
			Matrix34 matStruts;
			m_pParentVehicle->GetGlobalMtx(nStrutsBone, matStruts);

			float fDistSq = (matStruts.d - vecPosWorldSpace).Mag2();

			TUNE_FLOAT(SPOILER_DAMAGE_THRESHOLD, 3.0f, 0.0f, 100.0f, 0.001f);
			TUNE_FLOAT(SPOILER_DAMAGE_RADIUS, 2.0f, 0.0f, 100.0f, 0.001f);
			const float fMaxSpoilerDamageRadiusSq = SPOILER_DAMAGE_RADIUS * SPOILER_DAMAGE_RADIUS;

			if(fDistSq <= fMaxSpoilerDamageRadiusSq)
			{
				m_pParentVehicle->GetVehicleDamage()->GetDynamicSpoilerDamage() += fDamageDeltaMag;
				if(m_pParentVehicle->GetVehicleDamage()->GetDynamicSpoilerDamage() >= SPOILER_DAMAGE_THRESHOLD)
				{
					for(int i = 0; i < m_pParentVehicle->GetNumberOfVehicleGadgets(); i++)
					{
						if(m_pParentVehicle->GetVehicleGadget(i)->GetType() == VGT_DYNAMIC_SPOILER)
						{
							((CVehicleGadgetDynamicSpoiler*)m_pParentVehicle->GetVehicleGadget(i))->SetDynamicSpoilerEnabled(false);
						}
					}
				}
			}
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			if(  m_pParentVehicle->IsDriverAPlayer() )
			{
				CReplayMgr::RecordFx(CPacketVehDamageUpdate_PlayerVeh(m_ImpactStore[nStoreImpact].damage, m_ImpactStore[nStoreImpact].offset, m_pParentVehicle), m_pParentVehicle);
			}
			else
			{
				CReplayMgr::RecordFx(CPacketVehDamageUpdate(m_ImpactStore[nStoreImpact].damage, m_ImpactStore[nStoreImpact].offset, m_pParentVehicle), m_pParentVehicle);
			}			
		}
#endif // GTA_REPLAY
	}

	return true;	// Return true to indicate that we actually did some deformation.
}


//static const unsigned FRONT_LEFT_DAMAGE = 0;
//static const unsigned FRONT_RIGHT_DAMAGE = 1;
//static const unsigned MIDDLE_LEFT_DAMAGE = 2;
//static const unsigned MIDDLE_RIGHT_DAMAGE = 3;
//static const unsigned REAR_LEFT_DAMAGE = 4;
//static const unsigned REAR_RIGHT_DAMAGE = 5;

bank_float fDefaultRadiusDamage[3] = { 0.2f, 0.3f, 0.4f };
bank_float DamageLevelPct[3]       = { 0.4f, 0.55f, 0.7f };
bank_float DamagePosition[CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS][2] = {	{ -1.0f,  1.0f },
																						{  1.0f,  1.0f },
																						{ -1.0f,  0.0f },
																						{  1.0f,  0.0f },
																						{ -1.0f, -1.0f },
																						{  1.0f, -1.0f } };
bank_float randomXMin = 0.9f;
bank_float randomXMax = 1.2f;

bank_float randomYMin = 1.5f; // push it toward the front/back.
bank_float randomYMax = 3.0f;

bank_float randomZMin = -0.25f;
bank_float randomZMax = 0.25f;

void CVehicleDeformation::ApplyDeformationsFromNetwork(u32 damageLevel[CVehicleDeformation::NUM_NETWORK_DAMAGE_DIRECTIONS])
{
    // override the network modifiers to match what was passed in
    ResetDamage();
	ClearStoredImpacts();

	FastAssert( m_nImpactStoreIdx < VEHICLE_DEFORMATION_IMPACT_STORE_SIZE );

    for(int i=0;i<6;i++)
    {
		Assert(damageLevel[i] <= 3);
		damageLevel[i] = MIN(3, damageLevel[i]);
		if( damageLevel[i] > 0 )
		{
			float fRandomX = fwRandom::GetRandomNumberInRange(randomXMin, randomXMax) * DamagePosition[i][0];
			float fRandomY = fwRandom::GetRandomNumberInRange(randomYMin, randomYMax) * DamagePosition[i][1];
			float fRandomZ = fwRandom::GetRandomNumberInRange(randomZMin, randomZMax);

			Vector4 offset = Vector4(fRandomX, fRandomY, fRandomZ, 0.0f);
			offset.Normalize3();
			
			float damageIntensity = DamageLevelPct[damageLevel[i] - 1];
			Vector4 damage = Vector4(-offset.x*damageIntensity, -offset.y*damageIntensity, -offset.z*damageIntensity, fDefaultRadiusDamage[damageLevel[i] - 1]);

			m_ImpactStore[m_nImpactStoreIdx].damage = damage;
			m_ImpactStore[m_nImpactStoreIdx].offset = offset;
			m_nImpactStoreIdx++;

			if(m_nImpactStoreIdx >= VEHICLE_DEFORMATION_IMPACT_STORE_SIZE)
			{
				ApplyDeformations(false, true);
				ClearStoredImpacts();
			}
		}
	}

	if(m_nImpactStoreIdx > 0)
	{
		ApplyDeformations(false, true);
	    ClearStoredImpacts();
	}

    // ensure the network damage levels are consistent, if they are not we set them to a value
    // midway between the current and previous damage values. This stops the modifiers from
    // going out of sync from the first tiny bump received.
    for(int i=0;i<6;i++)
    {
		m_networkDamages[i] = (damageLevel[i] > 0) ? networkDamageModifiers[i][damageLevel[i]-1] + ((networkDamageModifiers[i][damageLevel[i]] - networkDamageModifiers[i][damageLevel[i]-1])*0.5f) : 0.0f;
    }

	// flag vehicle glass components as needing damage update
	g_vehicleGlassMan.ApplyVehicleDamage(m_pParentVehicle);
}

#if __BANK

void CVehicleDeformation::ApplyDamageWithOffset(const Vector4 &damage, const Vector4 &offset)
{
    m_ImpactStore[0].damage = damage;
    m_ImpactStore[0].offset = offset;
    m_ImpactStore[0].offset.Normalize();
	m_ImpactStore[0].offset.w = offset.Mag();
    m_nImpactStoreIdx = 1;

    ApplyDeformations();
}

void CVehicleDeformation::RenderBank()
{
	if (CVehicleDamage::ms_SourceVehicle && ms_bShowTextureAndRenderTarget)
	{
		grcTexture *const texture = CVehicleDamage::ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->GetDamageTexture();

		if (texture)
		{
			Vector4 positionAndSize;
			positionAndSize.z = (float)texture->GetWidth() * ms_fDisplayTextureScale;
			positionAndSize.w = (float)texture->GetHeight() * ms_fDisplayTextureScale;
			positionAndSize.x = 10.0f;//0.5f*((float)VideoResManager::GetNativeWidth()  - positionAndSize.z);
			positionAndSize.y = 10.0f;//0.5f*((float)VideoResManager::GetNativeHeight() - positionAndSize.w);

			const float deviceWidth = float(  VideoResManager::GetNativeWidth() );
			const float deviceHeight = float( VideoResManager::GetNativeHeight() );

			const float normX = positionAndSize.x / deviceWidth;
			const float normWidth = positionAndSize.z / deviceWidth;
			const float normY = positionAndSize.y / deviceHeight;
			const float normHeight = positionAndSize.w / deviceHeight;

			const float x1 = normX * 2.0f - 1.0f;
			const float y1 = -(normY * 2.0f - 1.0f);
			const float x2 = (normX + normWidth) * 2.0f - 1.0f;
			const float y2 = -((normY + normHeight) * 2.0f - 1.0f);

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

			CSprite2d spriteBlit;
			spriteBlit.BeginCustomList(CSprite2d::CS_SIGNED_AS_UNSIGNED, texture);
			grcDrawSingleQuadf(
				x1, y1,	x2, y2,
				0.0f,
				0.0f, 0.0f, 1.0f, 1.0f,
				Color32(255, 255, 255, 255));
			spriteBlit.EndCustomList();

			spriteBlit.SetTexture( static_cast<grcTexture*>(NULL) );
		}

		grcRenderTarget *const rt = CVehicleDamage::ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->GetDamageRenderTarget();

		if (rt)
		{
			Vector4 positionAndSize;
			positionAndSize.z = (float)rt->GetWidth() * ms_fDisplayTextureScale;
			positionAndSize.w = (float)rt->GetHeight() * ms_fDisplayTextureScale;
			positionAndSize.x = VideoResManager::GetNativeWidth()  - positionAndSize.z - 10.0f;
			positionAndSize.y = 10.0f;

			const float deviceWidth = float(  VideoResManager::GetNativeWidth() );
			const float deviceHeight = float( VideoResManager::GetNativeHeight() );

			const float normX = positionAndSize.x / deviceWidth;
			const float normWidth = positionAndSize.z / deviceWidth;
			const float normY = positionAndSize.y / deviceHeight;
			const float normHeight = positionAndSize.w / deviceHeight;

			const float x1 = normX * 2.0f - 1.0f;
			const float y1 = -(normY * 2.0f - 1.0f);
			const float x2 = (normX + normWidth) * 2.0f - 1.0f;
			const float y2 = -((normY + normHeight) * 2.0f - 1.0f);

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

			CSprite2d spriteBlit;
			spriteBlit.BeginCustomList(CSprite2d::CS_SIGNED_AS_UNSIGNED, rt);
			grcDrawSingleQuadf(
				x1, y1,	x2, y2,
				0.0f,
				0.0f, 0.0f, 1.0f, 1.0f,
				Color32(255, 255, 255, 255));
			spriteBlit.EndCustomList();

			spriteBlit.SetTexture( static_cast<grcTexture*>(NULL) );
		}
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		grcTexture *const textureNext = CVehicleDamage::ms_SourceVehicle->GetVehicleDamage()->GetDeformation()->GetTexCache()->GetDamageDownloadTexture();

		if (textureNext)
		{
			Vector4 positionAndSize;
			positionAndSize.z = (float)rt->GetWidth() * ms_fDisplayTextureScale;
			positionAndSize.w = (float)rt->GetHeight() * ms_fDisplayTextureScale;
			positionAndSize.x = VideoResManager::GetNativeWidth()  - positionAndSize.z - 10.0f;
			positionAndSize.y = VideoResManager::GetNativeHeight()  - positionAndSize.w - 10.0f;

			const float deviceWidth = float(  VideoResManager::GetNativeWidth() );
			const float deviceHeight = float( VideoResManager::GetNativeHeight() );

			const float normX = positionAndSize.x / deviceWidth;
			const float normWidth = positionAndSize.z / deviceWidth;
			const float normY = positionAndSize.y / deviceHeight;
			const float normHeight = positionAndSize.w / deviceHeight;

			const float x1 = normX * 2.0f - 1.0f;
			const float y1 = -(normY * 2.0f - 1.0f);
			const float x2 = (normX + normWidth) * 2.0f - 1.0f;
			const float y2 = -((normY + normHeight) * 2.0f - 1.0f);

			grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

			CSprite2d spriteBlit;
			spriteBlit.BeginCustomList(CSprite2d::CS_SIGNED_AS_UNSIGNED, textureNext);
			grcDrawSingleQuadf(
				x1, y1,	x2, y2,
				0.0f,
				0.0f, 0.0f, 1.0f, 1.0f,
				Color32(255, 255, 255, 255));
			spriteBlit.EndCustomList();

			spriteBlit.SetTexture( static_cast<grcTexture*>(NULL) );
		}
#endif
	}
}
#endif

#if GTA_REPLAY
void CVehicleDeformation::SetNewImpactToStore(const Vector4& rDamage, const Vector4& rOffset)
{
	// Do not deform the wrecked vehicle, as it will make the broken off parts' meshes differ from their bounds
	if(m_pParentVehicle->GetStatus() == STATUS_WRECKED && !m_pParentVehicle->HasBoundUpdatePending())
		return;

	if(Verifyf(m_nImpactStoreIdx < VEHICLE_DEFORMATION_IMPACT_STORE_SIZE, "Run out of replay impact stores for vehicle damage."))
	{
		m_ImpactStore[m_nImpactStoreIdx].damage = rDamage;
		m_ImpactStore[m_nImpactStoreIdx].offset = rOffset;
		m_ImpactStore[m_nImpactStoreIdx].offset.w = 0.0f;
		m_ImpactStore[m_nImpactStoreIdx].offset.Normalize();
		m_nImpactStoreIdx++;
		FastAssert(m_nImpactStoreIdx <= VEHICLE_DEFORMATION_IMPACT_STORE_SIZE);
	}
}
#endif

#if VEHICLE_ROLLING_TEXTURE_ARRAY
void VehTexCacheEntry::IncrementQueuedDownload()
{
	Assert(GRCDEVICE.IsMessagePumpThreadThatCreatedTheD3DDevice());
	m_ActiveGPUIndex = (m_ActiveGPUIndex + 1) % VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE;
	m_FrameToDownloadOn[m_ActiveGPUIndex] = GRCDEVICE.GetFrameCounter() + 2 * GRCDEVICE.GetGPUCount();
	Assertf(m_ActiveCPUIndex != m_ActiveGPUIndex, "The vehicle damage texture queue has overrun itself");
}

grcTexture *VehTexCacheEntry::GetDownloadQueuedTexture()
{
	Assert(GRCDEVICE.IsMessagePumpThreadThatCreatedTheD3DDevice());
	Assertf(m_ActiveCPUIndex != m_ActiveGPUIndex, "The vehicle damage texture queue has overrun itself");
	return m_DamageTextures[m_ActiveGPUIndex].GetTexture();
}

bool VehTexCacheEntry::IsDamageQueued()
{
	return m_ActiveCPUIndex != m_ActiveGPUIndex;
}

bool VehTexCacheEntry::DamageQueuedIsReady()
{
	Assert(GRCDEVICE.IsMessagePumpThreadThatCreatedTheD3DDevice());
	int nextIndex = (m_ActiveCPUIndex + 1) % VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE;
	return (m_FrameToDownloadOn[nextIndex] <= GRCDEVICE.GetFrameCounter());
}

grcTexture *VehTexCacheEntry::GetDamageDownloadTexture()
{
	Assert(GRCDEVICE.IsMessagePumpThreadThatCreatedTheD3DDevice());
	Assertf(m_ActiveCPUIndex != m_ActiveGPUIndex, "The vehicle damage texture queue has overrun itself");
	int nextIndex = (m_ActiveCPUIndex + 1) % VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE; //ms_TextureCache[m_nTextureCacheIdx].m_Active;// 
	return m_DamageTextures[nextIndex].GetTexture();
}

void VehTexCacheEntry::AdvanceActiveTexture()
{
	Assert(GRCDEVICE.IsMessagePumpThreadThatCreatedTheD3DDevice());
	rage::sysIpcLockMutex(m_DamageMutex);
	m_ActiveCPUIndex = (m_ActiveCPUIndex + 1) % VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE;
	rage::sysIpcUnlockMutex(m_DamageMutex);
}

void VehTexCacheEntry::ClearDamageDownloads()
{
	Assert(!GRCDEVICE.IsMessagePumpThreadThatCreatedTheD3DDevice());
	rage::sysIpcLockMutex(m_DamageMutex);
	m_ActiveCPUIndex = m_ActiveGPUIndex;
	rage::sysIpcUnlockMutex(m_DamageMutex);
}
#endif //VEHICLE_ROLLING_TEXTURE_ARRAY

void CVehicleDeformation::UpdateNetworkDamageLevels()
{
    for(s32 index = 0; index < m_nImpactStoreIdx; index++)
    {
        // calculate which side of the car has been hit
        float dotProductTop   = m_ImpactStore[index].offset.Dot3(Vector3(0.0f, 1.0f, 0.0f));
        float dotProductRight = m_ImpactStore[index].offset.Dot3(Vector3(1.0f, 0.0f, 0.0f));

		int idx = FRONT_LEFT_DAMAGE;
		
        if(dotProductTop >= 0.0f)
        {
            // front half of car
            if(dotProductTop >= 0.5f)
            {
                if(dotProductRight < 0.0f)
                {
                    // left of car
                    idx = FRONT_LEFT_DAMAGE;
                }
                else
                {
                    // right of car
                    idx = FRONT_RIGHT_DAMAGE;
                }
            }
            else
            {
                if(dotProductRight < 0.0f)
                {
                    // left of car
                    idx = MIDDLE_LEFT_DAMAGE;
                }
                else
                {
                    // right of car
                    idx = MIDDLE_RIGHT_DAMAGE;
                }
            }
        }
        else
        {
            // rear half of car
            if(-dotProductTop >= 0.5f)
            {
                if(dotProductRight < 0.0f)
                {
                    // left of car
                    idx = REAR_LEFT_DAMAGE;
                }
                else
                {
                    // right of car
                    idx = REAR_RIGHT_DAMAGE;
                }
            }
            else
            {
                if(dotProductRight < 0.0f)
                {
                    // left of car
                    idx = MIDDLE_LEFT_DAMAGE;
                }
                else
                {
                    // right of car
                    idx = MIDDLE_RIGHT_DAMAGE;
                }
            }
        }
        
        m_networkDamages[idx] += m_ImpactStore[index].damage.w;
    }
}

dev_float fSmashLightDeformationThreshold = 0.02f;
dev_float fSmashSirenDeformationThreshold = 0.02f;

void CVehicleDeformation::ApplyDeformations(bool bBoundsNeedUpdating, bool UNUSED_PARAM(bNetworkDamageOnly))
{
#if ENABLE_APPLY_DAMAGE_PF
	PF_FUNC(ApplyDeformations);
	PF_INCREMENT(ApplyDeformations);
#endif // ENABLE_APPLY_DAMAGE_PF
	if (NetworkInterface::IsGameInProgress())
	{
		if(!m_pParentVehicle || !m_pParentVehicle->GetVehicleDamage() 
			|| !(m_pParentVehicle->GetVehicleDamage()->CanVehicleBeDamagedBasedOnDriverInvincibility() || bBoundsNeedUpdating))// we still want to update the ground clearance
			return;
	}

#if !__FINAL
	if (gbStopVehiclesDamaging)
		return;
#endif

#if NO_VEH_DAMAGE_PARAM
	if(bTestNoVehicleDeformation)
		return;
#endif

	if(!m_pParentVehicle)
		return;

	if(!m_nImpactStoreIdx && !bBoundsNeedUpdating)// If there has been a request for the bounds to change, just process the damage anyway
	{
		return;
	}

	ENABLE_FRAG_OPTIMIZATION_ONLY(m_pParentVehicle->GiveFragCacheEntry(NetworkInterface::IsGameInProgress());)

	if(!HasDamageTexture())
	{
		DamageTextureAllocate();
	}

	if(!HasDamageTexture())
	{
		// still no luck, bail out
		return;
	}

#if VEHICLE_DEFORMATION_TIMING
	sysTimer DTT_timer;
	u32 TimeStart = DTT_timer.GetSystemMsTime();
#endif // VEHICLE_DEFORMATION_TIMING

	if(NetworkInterface::IsGameInProgress())
	{
		UpdateNetworkDamageLevels();
	}

	bool isCar = (m_pParentVehicle->GetVehicleType() == VEHICLE_TYPE_CAR);
	bool hasRollCageMod = false;

	if (isCar && m_pParentVehicle->IsModded())
	{
		const CVehicleVariationInstance& var = m_pParentVehicle->GetVariationInstance();
		u32 chassisModEntry = var.GetMods()[VMT_CHASSIS];
		bool dummy = false;
		int index = var.GetModIndexForType(VMT_CHASSIS, m_pParentVehicle, dummy);
		hasRollCageMod = (chassisModEntry != INVALID_MOD) && (index != -1);
	}

	bool HasLandingGear = false;

	Vector3 vFrontLandingGearPos;
	if(m_pParentVehicle->InheritsFromPlane() && m_pParentVehicle->GetDefaultBonePositionForSetup(LANDING_GEAR_F, vFrontLandingGearPos))
	{
		HasLandingGear = true;
		vFrontLandingGearPos.y -= 0.5f; // subtract 0.5m for landing gear length
	}
	
	PF_PUSH_TIMEBAR_DETAIL("DrawDamage");

#if GPU_DAMAGE_WRITE_ENABLED
	CApplyDamage::EnqueueDamage(m_pParentVehicle, false);
	PF_POP_TIMEBAR_DETAIL();

	// the HandleDamageAdded will be called in CVehicle::HandleDamageUpdatedByGPU if we have impacts
	if(bBoundsNeedUpdating && m_nImpactStoreIdx == 0)
	{
		void* basePtr = LockDamageTexture(grcsRead);
		if(basePtr)
		{
			HandleDamageAdded(basePtr, true, /*!bDeformationOnly*/true, true, true, true, /*bUpdateVehicleGlass*/true);
			UnLockDamageTexture();
		}
	}
#else
	void* basePtr = LockDamageTexture(grcsRead|grcsWrite);
	if(basePtr)
	{
		for(int i=0; i < m_nImpactStoreIdx; i++)
		{
			const Vector3 damage(m_ImpactStore[i].damage.xyzw);
			const Vector3 offset(m_ImpactStore[i].offset.xyzw);
			const float radius = m_ImpactStore[i].damage.w;

			ApplyDamageToCircularArea(basePtr, damage, offset, radius);
		}
		PF_POP_TIMEBAR_DETAIL();

		HandleDamageAdded(basePtr, true, /*!bDeformationOnly*/true, true, true, true, /*bUpdateVehicleGlass*/true);
		UnLockDamageTexture();
	}
#endif

#if __BANK
	if (ms_bAutoSaveDamageTexture GPU_VEHICLE_DAMAGE_ONLY(&& !CVehicleDamage::ms_bEnableGPUDamage))
	{
		SaveDamageTexture(m_pParentVehicle, false);
	}
#endif
		
	ClearStoredImpacts();

}

float CVehicleDeformation::GetDamageMultiplier(float* sizeMult) const
{
	if (sizeMult)
	{
		*sizeMult = m_fSizeMultiplier;
	}

	return m_fDamageMultByVehicleSize;
}

void CVehicleDeformation::CalculateDamageMultiplier()
{
#if VEHICLE_DEFORMATION_PROPORTIONAL

	fragInstGta* fragInstPtr = m_pParentVehicle ? m_pParentVehicle->GetVehicleFragInst() : NULL;
	Assert(fragInstPtr);

	if (!fragInstPtr)
	{	
		m_fSizeMultiplier = 0.0f;
		m_fDamageMultByVehicleSize = GTA_VEHICLE_DAMAGE_DELTA_SCALE;
		m_fDamageMultByVehicleSizeInv = 1.0f / m_fDamageMultByVehicleSize;
		return;
	}

	Vector3 boundingBoxMin = m_pParentVehicle->GetChassisBoundMin(false);
	Vector3 boundingBoxMax = m_pParentVehicle->GetChassisBoundMax(false);
	float vehicleLength = boundingBoxMax.y - boundingBoxMin.y;
	Assert(vehicleLength > 0.1f);

	m_fSizeMultiplier = Min(1.0f, (vehicleLength - MIN_DAMAGE_RANGE) / MAX_DAMAGE_RANGE);
	m_fSizeMultiplier *= VEHICLE_DEFORMATION_PROPORTIONAL_SCALE_SLOPE;
	m_fDamageMultByVehicleSize = 1.0f + m_fSizeMultiplier;
	m_fDamageMultByVehicleSize *= ms_fDamageMagnitudeMult;
	m_fDamageMultByVehicleSizeInv = 1.0f / m_fDamageMultByVehicleSize;
#else
	m_fSizeMultiplier = 0.0f;
	m_fDamageMultByVehicleSize = GTA_VEHICLE_DAMAGE_DELTA_SCALE;
	m_fDamageMultByVehicleSizeInv = 1.0f / m_fDamageMultByVehicleSize;
#endif

}

void CVehicleDeformation::UpdateShaderDamageVars(bool bEnableDamage, float radius, float multiplier)
{
	Assert(m_pParentVehicle != NULL);
	Assert(m_pParentVehicle->GetDrawHandlerPtr() != NULL);
	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(m_pParentVehicle->GetDrawHandler().GetShaderEffect());
	Assert(pShader);

	if (pShader)
	{
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		if (bEnableDamage && m_nTextureCacheIdx != -1)
		{
			pShader->SetEnableDamage(ms_TextureCache[m_nTextureCacheIdx].m_HasDamageDrawn);
		}
		else
		{
			pShader->SetEnableDamage(false);
		}
#else
		pShader->SetEnableDamage(bEnableDamage);
#endif
		pShader->SetDamageMultiplier(multiplier);
		pShader->SetBoundRadius(radius);
		Vector4 vDeformedWheelOffsets[2];
		vDeformedWheelOffsets[0].Zero();
		vDeformedWheelOffsets[1].Zero();
		if(CWheel *pWheel = m_pParentVehicle->GetWheelFromId(VEH_WHEEL_LF))
		{
			if(!pWheel->GetDynamicFlags().IsFlagSet(WF_BROKEN_OFF))
			{
				int iWheelBone = m_pParentVehicle->GetBoneIndex(VEH_WHEEL_LF);
				if(iWheelBone > -1 && m_pParentVehicle->GetSkeletonData().GetBoneData(iWheelBone))
				{
					Vector3 vDefaultPos = VEC3V_TO_VECTOR3(m_pParentVehicle->GetSkeletonData().GetBoneData(iWheelBone)->GetDefaultTranslation());
					float fDeformationSqr = geomDistances::Distance2SegToPoint(pWheel->GetProbeSegment().A, pWheel->GetProbeSegment().B - pWheel->GetProbeSegment().A, vDefaultPos);
					if(fDeformationSqr > 0.0025f)
					{
						vDeformedWheelOffsets[0].SetVector3(pWheel->GetProbeSegment().A);
						vDeformedWheelOffsets[0].SetW(pWheel->GetWheelRadius());
					}
				}
			}
		}
		if(CWheel *pWheel = m_pParentVehicle->GetWheelFromId(VEH_WHEEL_RF))
		{
			if(!pWheel->GetDynamicFlags().IsFlagSet(WF_BROKEN_OFF))
			{
				int iWheelBone = m_pParentVehicle->GetBoneIndex(VEH_WHEEL_RF);
				if(iWheelBone > -1 && m_pParentVehicle->GetSkeletonData().GetBoneData(iWheelBone))
				{
					Vector3 vDefaultPos = VEC3V_TO_VECTOR3(m_pParentVehicle->GetSkeletonData().GetBoneData(iWheelBone)->GetDefaultTranslation());
					float fDeformationSqr = geomDistances::Distance2SegToPoint(pWheel->GetProbeSegment().A, pWheel->GetProbeSegment().B - pWheel->GetProbeSegment().A, vDefaultPos);
					if(fDeformationSqr > 0.0025f)
					{
						vDeformedWheelOffsets[1].SetVector3(pWheel->GetProbeSegment().A);
						vDeformedWheelOffsets[1].SetW(pWheel->GetWheelRadius());
					}
				}
			}
		}
		pShader->SetDamagedFrontWheelOffsets(vDeformedWheelOffsets[0], vDeformedWheelOffsets[1]);

		if (bEnableDamage && m_nTextureCacheIdx != -1)
		{
#if GPU_DAMAGE_WRITE_ENABLED && __D3D11
 #if VEHICLE_ROLLING_TEXTURE_ARRAY
			pShader->SetDamageTex(ms_TextureCache[m_nTextureCacheIdx].m_DamageRenderTarget.GetTexture());
 #else
			pShader->SetDamageTex(ms_TextureCache[m_nTextureCacheIdx].m_Payloads[1].GetTexture());
 #endif
#else
			pShader->SetDamageTex(ms_TextureCache[m_nTextureCacheIdx].m_Payload.GetTexture());
#endif
		}
		else
		{
			pShader->SetDamageTex(NULL);
		}
	}	
}

void CVehicleDeformation::HandleDamageAdded(void *basePtr, bool bEnableDamageShader, bool bUpdateBounds, bool bUpdateWheels, bool bUpdateAudio, bool bUpdateBones, bool bUpdateGlass)
{	
	if (!AssertVerify(m_pParentVehicle))
	{
		return;
	}

	Assert(!CVehicleFactory::GetFactory()->IsVehInDestroyedCache(m_pParentVehicle));

	CVehicleDamage* pVehDamage = m_pParentVehicle->GetVehicleDamage();
	Assert(pVehDamage);

	if(bUpdateBounds BANK_ONLY(&& (CVehicleDeformation::ms_bUpdateBoundsEnabled)) )
	{
		//Expensive
		ApplyDamageToBound(basePtr);
	}

	// We apply the deformation on selected bones and break each parts depending on the result.
	if(bUpdateAudio && !m_pParentVehicle->IsNetworkClone() && !m_pParentVehicle->m_nVehicleFlags.bUnbreakableLights BANK_ONLY(&& (CVehicleDeformation::ms_bUpdateBonesEnabled)))
	{
		audVehicleAudioEntity* audEntity = m_pParentVehicle->GetVehicleAudioEntity();
		Assert(audEntity);

		// Lights
		for(int i=0; i<NUM_LIGHT_BONES; i++)
		{
			const int boneIdx = m_pParentVehicle->GetBoneIndex(aGlassBones[i]);
			const bool lightState = pVehDamage->GetLightState(aGlassBones[i]);
			if( boneIdx != -1 && false == lightState)
			{
				Vector3 vertPos;
				m_pParentVehicle->GetDefaultBonePositionSimple(boneIdx,vertPos);
				const Vector3 damage = VEC3V_TO_VECTOR3(ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vertPos)));
				// We got damage, and an actual point
				if( (vertPos.Mag2() > 0.0f) && (damage.Mag2() > fSmashLightDeformationThreshold * fSmashLightDeformationThreshold) )
				{
					pVehDamage->SetLightStateImmediate(i,true);

#if GTA_REPLAY
					//don't do the break effect and audio if this is happening while the scene is precaching
					if(CReplayMgr::IsPreCachingScene() == false)
#endif
					{
						// Use damage offset as normal ?
						Vector3 normal = -damage;
						normal.Normalize();
						g_vfxVehicle.TriggerPtFxLightSmash(m_pParentVehicle,aGlassBones[i],RCC_VEC3V(vertPos),RCC_VEC3V(normal));
						audEntity->TriggerHeadlightSmash(aGlassBones[i],vertPos);
						audEntity->GetCollisionAudio().HeadLightSmash();
					}

					pVehDamage->SetUpdateLightBones(true);
				}
			}
		}

		// Extras
        const CVehicleVariationInstance& variationInstance = m_pParentVehicle->GetVariationInstance();
        if (variationInstance.GetKitIndex() != INVALID_VEHICLE_KIT_INDEX && variationInstance.GetVehicleRenderGfx())
        {
            for (u32 ii = VEH_EXTRALIGHT_1; ii <= VEH_EXTRALIGHT_4; ++ii)
            {
				const bool lightState = pVehDamage->GetLightState(ii);
				if( false == lightState )
				{
					Vector3 vertPos;
					int boneIdx = m_pParentVehicle->GetExtraLightPosition((eHierarchyId)ii, vertPos);
					if( boneIdx != -1 )
					{
						const Vector3 damage = VEC3V_TO_VECTOR3(ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vertPos)));

						// We got damage, and an actual point
						if( (vertPos.Mag2() > 0.0f) && (damage.Mag2() > fSmashLightDeformationThreshold * fSmashLightDeformationThreshold) )
						{
							pVehDamage->SetLightState(ii,true);

	#if GTA_REPLAY
							//don't do the break effect and audio if this is happening while the scene is precaching
							if(CReplayMgr::IsPreCachingScene() == false)
	#endif
							{
								// Use damage offset as normal ?
								Vector3 normal = -damage;
								normal.Normalize();
								g_vfxVehicle.TriggerPtFxLightSmash(m_pParentVehicle,ii,RCC_VEC3V(vertPos),RCC_VEC3V(normal));
								audEntity->TriggerHeadlightSmash(ii,vertPos);
								audEntity->GetCollisionAudio().HeadLightSmash();
							}

							pVehDamage->SetUpdateLightBones(true);
						}
					}
				}
			}
		}

		// sirens
		if( true == m_pParentVehicle->UsesSiren())
		{
			for(int i=VEH_SIREN_1; i<VEH_SIREN_MAX+1; i++)
			{
				const bool sirenState = pVehDamage->GetSirenState(i);
				const int boneIdx = m_pParentVehicle->GetBoneIndex((eHierarchyId)i);

				if( (boneIdx != -1) && (false == sirenState) )
				{
					Vector3 vertPos;
					m_pParentVehicle->GetDefaultBonePositionSimple(boneIdx,vertPos);
					const Vector3 damage = VEC3V_TO_VECTOR3(ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vertPos)));
					// We got damage, and an actual point
					if( (vertPos.Mag2() > 0.0f) && (damage.Mag2() > fSmashSirenDeformationThreshold * fSmashSirenDeformationThreshold) )
					{
						pVehDamage->SetUpdateSirenBones(true);
						pVehDamage->SetSirenState(i,true);

						// No Fxs
						// pop/break the glass casing
						const int boneGlassID = m_pParentVehicle->GetBoneIndex((eHierarchyId)(i + (VEH_SIREN_GLASS_1-VEH_SIREN_1)));
						const int component = m_pParentVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(boneGlassID);
						if( component != -1 )
						{
							Vec3V vSmashNormal(V_Z_AXIS_WZERO);
							g_vehicleGlassMan.SmashCollision(m_pParentVehicle,component, VEHICLEGLASSFORCE_SIREN_SMASH, vSmashNormal);
							audEntity->SmashSiren();
						}
					}
				}
			}
		}
	}
	
	if (bUpdateBones BANK_ONLY(&& (CVehicleDeformation::ms_bUpdateBonesEnabled)))
	{
		// vehicle type specific deformation stuff (e.g. moving rotors on heli's, seats in cars).
		m_pParentVehicle->ApplyDeformationToBones(basePtr);
	}

	if (bUpdateGlass BANK_ONLY(&& (CVehicleDeformation::ms_bUpdateBonesEnabled))) //re-use the same bank variable
	{
		// flag vehicle glass components as needing damage update
		g_vehicleGlassMan.ApplyVehicleDamage(m_pParentVehicle);
	}

	// Postpone the wheel deformation update as it has dependence with bone deformation update
	if(bUpdateWheels && m_pParentVehicle->m_nVehicleFlags.bCanDeformWheels BANK_ONLY(&& (CVehicleDeformation::ms_bUpdateBonesEnabled)))
	{
		m_pParentVehicle->SetupWheels(basePtr);
	}

	UpdateShaderDamageVars(bEnableDamageShader);
}

void CVehicleDeformation::ResetDamage()
{
	if (m_pParentVehicle==NULL)
		return;

	if (HasDamageTexture() == false)
		return;

	memset(m_networkDamages,0,sizeof(m_networkDamages));

#if GPU_DAMAGE_WRITE_ENABLED

	if (CVehicleDamage::ms_bEnableGPUDamage)
	{ 
		CApplyDamage::ResetDamage(m_pParentVehicle);
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		if (m_nTextureCacheIdx >= 0)
		{
			ms_TextureCache[m_nTextureCacheIdx].m_HasDamageDrawn = false;
		}
#endif //VEHICLE_ROLLING_TEXTURE_ARRAY
	}
#endif


#if USE_EDGE
	void *basePtr = LockDamageTexture(grcsRead|grcsWrite);
	if( basePtr )
	{
		float *pFloatEntry = (float *)basePtr;
		sysMemSet(pFloatEntry, 0, GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH * GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT*sizeof(float));
		HandleDamageAdded(basePtr, false, true, true, false, true, false);
		UnLockDamageTexture();
	}
	return;
#else
	
	void* basePtr = LockDamageTexture(grcsRead|grcsWrite);
	if( basePtr )
	{	
#if GPU_DAMAGE_WRITE_ENABLED
		sysMemSet((s8 *)basePtr, 0, GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH * GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT * 4 *
			(VEHICLE_DEFORMATION_USE_HALF_FLOATS ? sizeof(u16) : sizeof(s8)));
#else
		float *pFloatEntry = (float *)basePtr;

		for(int i=0; i<GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH * GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT; i++)
		{
			// write zero into each pixel
			*pFloatEntry = DAMAGE_ZERO;
			pFloatEntry++;
		}
#endif

		HandleDamageAdded(basePtr, false, true, true, false, true, false);
		UnLockDamageTexture();
	}
#endif // USE_EDGE
}

#if !__FINAL && !__TOOL && !__RESOURCECOMPILER
// Hack to work around not linking the fragment cache allocator into the physics samples.
void ValidateFragCachePointer(const void * ptr, const char * ptrName)
{
	// It's critical that this allocation comes out of the frag cache heap!
	if (!AssertVerify(fragManager::GetFragCacheAllocator()->IsValidHeaderPointer(ptr)))
		Quitf("INVALID %s pointer: %p. The game will crash soon!", ptrName, ptr);
}
#endif

void CVehicleDeformation::ApplyDamageToBound(const void* basePtr, const fragInst::ComponentBits* pOnlySubBoundGroups, const Vector3 *pSubBoundGroupsOffset) const
{
#if ENABLE_APPLY_DAMAGE_PF
	PF_FUNC(ApplyDamage);
	PF_INCREMENT(ApplyDamage);
#endif // ENABLE_APPLY_DAMAGE_PF

#if !__FINAL
	if (gbStopVehiclesDamaging)
		return;
#endif
#if NO_VEH_DAMAGE_PARAM
	if(bTestNoVehicleDeformation)
		return;
#endif

	// needs to be a fragment, and have flag set to clone bound
	if(m_pParentVehicle->GetVehicleFragInst()==NULL)
		return;

	m_pParentVehicle->CloneBounds();

	fragInst* pFragInst = m_pParentVehicle->GetVehicleFragInst();
	phBoundComposite* pCloneBound = (phBoundComposite* )pFragInst->GetArchetype()->GetBound();
	phBoundComposite* pOrigBound = (phBoundComposite* )pFragInst->GetTypePhysics()->GetCompositeBounds();
    if(pCloneBound == NULL || pOrigBound == NULL)
        return;

	Assert(pCloneBound->GetNumBounds() == pOrigBound->GetNumBounds());

	float fOriginalBoundRadius = pFragInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid();

    float fHeightAboveGround = 0.0f;
    bool bModifyGroundClearance = false;
    if(m_pParentVehicle->InheritsFromAutomobile() && !m_pParentVehicle->InheritsFromHeli() && m_pParentVehicle->GetModelIndex() != MI_DIGGER)
    {
        CAutomobile *pAutomobile = static_cast<CAutomobile*>(m_pParentVehicle);
        //modify ground clearance depending on speed
        fHeightAboveGround = -(pAutomobile->GetHeightAboveRoad() - pAutomobile->GetGroundClearance());
        bModifyGroundClearance = true;
    }
	ScalarV vHeightAboveGround = ScalarVFromF32(fHeightAboveGround);


#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
	const int needsUpdateListSize = pCloneBound->GetNumBounds();
	phBoundGeometry ** needsUpdateList = Alloca(phBoundGeometry*,needsUpdateListSize);
	int needsUpdateListCount = 0;
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION

	{
		// Grab the write lock to prevent shapetests on other threads from reading these geometry bounds during modification. 
		PHLOCK_SCOPEDWRITELOCK;

		bool reduceVehicleFront = false;
		if( bModifyGroundClearance &&
// NOTE: B*6928239   vehicels clip deeply in props in stuntmode at certain speed range ... so dont reduce front at all, even in stunt mode
			0 && CPhysics::ms_bInStuntMode &&
			!CPhysics::ms_bInArenaMode &&
			!( m_pParentVehicle->pHandling->mFlags & MF_IS_RC ) &&
			m_pParentVehicle->InheritsFromAutomobile() &&
			static_cast<CAutomobile*>( m_pParentVehicle )->GetGroundClearance() > 0.0f )
		{
			reduceVehicleFront = true;
		}

	for(int i=0; i<pCloneBound->GetNumBounds(); i++)
	{
		if(pOnlySubBoundGroups)
		{
			fragTypeChild* child = pFragInst->GetTypePhysics()->GetChild(i);
			int groupIndex = child->GetOwnerGroupPointerIndex();

			if (pOnlySubBoundGroups->IsClear(groupIndex))
			{
				continue;
			}
		}

		if( m_pParentVehicle->InheritsFromTrailer() )
		{
			CTrailer* pTrailer					= static_cast< CTrailer* >( m_pParentVehicle );
			const CTrailerLegs* pTrailerLegs	= pTrailer->GetTrailerLegs();

			if( pTrailerLegs->GetFragChild() == i )
			{
				continue;
			}
		}

        if( m_pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_OUTRIGGER_LEGS ) ) 
        {
            bool outriggerLeg = false;
            for( int j = (int)VEH_LEG_FL; j <= (int)VEH_LEG_RR; j++ )
            {
                int boneIndex = m_pParentVehicle->GetBoneIndex( (eHierarchyId)j );
                int group = pFragInst->GetGroupFromBoneIndex( boneIndex );

                if( group > -1 )
                {
                    fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetGroup( group ); 
                    int childFragmentIndex = pGroup->GetChildFragmentIndex();

                    if( i == childFragmentIndex )
                    {
                        outriggerLeg = true;
                    }
                }
            }
            if( outriggerLeg )
            {
                continue;
            }
        }

		bool arenaWeapon = false;
		
		if( m_pParentVehicle->GetMass() < 3499.0f )
		{
			for( int j = (int)VEH_FIRST_ARENA_MOD; j <= (int)VEH_LAST_ARENA_MOD; j++ )
			{
				int boneIndex = m_pParentVehicle->GetBoneIndex( (eHierarchyId)j );
				int group = pFragInst->GetGroupFromBoneIndex( boneIndex );

				if( group > -1 )
				{
					fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetGroup( group );
					int childFragmentIndex = pGroup->GetChildFragmentIndex();

					if( i == childFragmentIndex )
					{
						arenaWeapon = true;
						break;
					}
				}
			}
		}

		bool rampBone = false;
		for( int j = (int)VEH_FIRST_RAMP_MOD; j <= (int)VEH_LAST_RAMP_MOD; j++ )
		{
			int boneIndex = m_pParentVehicle->GetBoneIndex( (eHierarchyId)j );
			int group = pFragInst->GetGroupFromBoneIndex( boneIndex );

			if( group > -1 )
			{
				fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetGroup( group );
				int childFragmentIndex = pGroup->GetChildFragmentIndex();

				if( i == childFragmentIndex )
				{
					rampBone = true;
					break;
				}
			}
		}
		if( rampBone )
		{
			continue;
		}


		if(pCloneBound->GetBound(i) && pCloneBound->GetBound(i)->GetType()==phBound::GEOMETRY
		&& pOrigBound->GetBound(i) && pOrigBound->GetBound(i)->GetType()==phBound::GEOMETRY)
		{
			phBoundGeometry* pBoundGeom = (phBoundGeometry* )pCloneBound->GetBound(i);
			phBoundGeometry* pOrigBoundGeom = (phBoundGeometry* )pOrigBound->GetBound(i);

			Assert(pBoundGeom != pOrigBoundGeom);//make sure the bounds are clones of each other

			int numVertices = pOrigBoundGeom->GetNumVertices();
			Vector3* newVerts = Alloca(Vector3, numVertices);
			Vector3* newShrunkVerts = NULL;
			
			if (pOrigBoundGeom->GetShrunkVertexPointer() && pBoundGeom->GetShrunkVertexPointer())
			{
				newShrunkVerts = Alloca(Vector3, numVertices);
			}

			const Mat34V& boundMatrix = pCloneBound->GetCurrentMatrix(i);

			// MN - check the num verts in the 2 bounds are equal as the smashable
			// code may have removed a child bound and replaced it with a smashed bound
			if (pBoundGeom->GetNumVertices()==numVertices)
			{
				bool damaged = false;
				ScalarV maxDamageMagSquared = ScalarV(V_ZERO);
				const Mat34V boundMat = pOrigBound->GetCurrentMatrix(i);

				for(int vert = 0; vert < numVertices; vert++)
				{
					Vec3V vertPos = pOrigBoundGeom->GetVertex(vert);
					Vec3V vecOffset = vertPos + boundMat.GetCol3();;
					// Do not use interpolation for bound damage
					const Vec3V damage = ReadFromVectorOffset(basePtr, vecOffset, false);
					
					ScalarV damageMagSqaured = MagSquared(damage);
					damaged |= ((IsGreaterThanAll(damageMagSqaured, ScalarV(V_ZERO))) != 0);
					maxDamageMagSquared = Max(damageMagSqaured,maxDamageMagSquared);
						
					vertPos += damage;

					if(pOnlySubBoundGroups && pSubBoundGroupsOffset)
					{
						vertPos += RCC_VEC3V(*pSubBoundGroupsOffset);
					}
					
					newVerts[vert] = RCC_VECTOR3(vertPos);

					// also need to deform the shrunk geometry vertices
					if(newShrunkVerts)
					{
						// Use the same offset as the unshrunk vertices since they will only differ by a a few percent. The deformation is towards the center of the vehicle
						//   scaled by the distance to the center. The shrunk and unshrunk vertices are close enough together that their deformation vectors should be nearly identical. 

						const Vec3V shrunkVertPos = pOrigBoundGeom->GetShrunkVertex(vert);
						Vec3V newShrunkVertPos = shrunkVertPos + damage;

						if(pOnlySubBoundGroups && pSubBoundGroupsOffset)
						{
							newShrunkVertPos += RCC_VEC3V(*pSubBoundGroupsOffset);
						}

						// Only modify bounds which will actually collide with the map (we might break them off and don't want
						// them in the munged shape).
						if(bModifyGroundClearance && (pCloneBound->GetIncludeFlags(i)&ArchetypeFlags::GTA_MAP_TYPE_VEHICLE || pCloneBound->GetIncludeFlags(i)&ArchetypeFlags::GTA_OBJECT_TYPE))
						{
							//Sort out the ground clearance
							ScalarV heightLimit = -boundMatrix.GetCol3().GetZ();

							if( arenaWeapon )
							{
								if( m_pParentVehicle->InheritsFromAutomobile() && !m_pParentVehicle->InheritsFromHeli() && m_pParentVehicle->GetModelIndex() != MI_DIGGER )
								{
									static dev_float sfArenaModeWeaponHeightScale = 0.7f;
									CAutomobile *pAutomobile = static_cast<CAutomobile*>( m_pParentVehicle );
									// we don't want to raise the bounds of the arena mode weapons as much as other bounds but we do still want to raise them
									heightLimit += ScalarV( -( pAutomobile->GetHeightAboveRoad() - ( pAutomobile->GetGroundClearance() * sfArenaModeWeaponHeightScale ) ) );
								}
							}
							else
							{
								heightLimit += vHeightAboveGround;
							}

							if(IsLessThanAll(newShrunkVertPos.GetZ(), heightLimit))
							{
								newShrunkVertPos.SetZ(heightLimit);
							}

							if( reduceVehicleFront )
							{
								const CWheel* pWheel = m_pParentVehicle->GetWheelFromId( VEH_WHEEL_LF );

								if( pWheel )
								{
									int boneIndex		= m_pParentVehicle->GetBoneIndex( VEH_WHEEL_LF );
									Matrix34 wheelMtx	= m_pParentVehicle->GetLocalMtx( boneIndex );

									static float lengthLimitIncrease = 0.075f;
									const float boundOffset = boundMatrix.GetCol3().GetYf();
									const float lengthLimit =  wheelMtx.d.GetY() + pWheel->GetWheelRadius() + lengthLimitIncrease - boundOffset;

									if( newShrunkVertPos.GetYf() > lengthLimit )
									{
										newShrunkVertPos.SetY( lengthLimit );
									}
								}
							}
						}

						newShrunkVerts[vert] = RCC_VECTOR3(newShrunkVertPos);
					}
				}

				// Compute new bounding box for the bound, so that verts can move outside of the box they start with
				pBoundGeom->CalculateBoundingBox(newVerts, newShrunkVerts);

#if COMPRESSED_VERTEX_METHOD > 0
				pBoundGeom->CalculateQuantizationValues();
#endif

				for(int vert = 0; vert < numVertices; ++vert)
				{
					pBoundGeom->SetVertex(vert, RCC_VEC3V(newVerts[vert]));
				}
				bool shrunkVertsChanged = false;
				if(newShrunkVerts)
				{
					for(int vert = 0; vert < numVertices; ++vert)
					{
						shrunkVertsChanged |= pBoundGeom->SetShrunkVertex(vert, RCC_VEC3V(newShrunkVerts[vert]));
					}
				}

				pBoundGeom->CalculatePolyNormals();

#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
				if(shrunkVertsChanged)
				{
					Assert(needsUpdateListCount < needsUpdateListSize);
					needsUpdateList[needsUpdateListCount] = pBoundGeom;
					needsUpdateListCount++;
				}
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION

				if( damaged ) 
				{ // This was damaged, do we need to do something about it ?
					phMaterialMgr::Id matid = PGTAMATERIALMGR->UnpackMtlId(pOrigBoundGeom->GetMaterialId(0));
					bool glassBound = ( PGTAMATERIALMGR->GetIsSmashableGlass(matid) && !m_pParentVehicle->HasBulletResistantGlass());
					if( glassBound )
					{
						TUNE_GROUP_FLOAT(VEHICLE_DAMAGE,MIN_DEFORM_MAG_TO_SMASH_GLASS,0.001f,0.0f,1.0f,0.001f);

						BANK_SWITCH(const, static const) ScalarV vMIN_DEFORM_MAG_TO_SMASH_GLASS = ScalarVFromF32(MIN_DEFORM_MAG_TO_SMASH_GLASS);
						BANK_SWITCH(const, static const) ScalarV vMIN_DEFORM_MAG_TO_SMASH_GLASS_SQR = vMIN_DEFORM_MAG_TO_SMASH_GLASS * vMIN_DEFORM_MAG_TO_SMASH_GLASS;

						if(IsGreaterThanAll(maxDamageMagSquared, vMIN_DEFORM_MAG_TO_SMASH_GLASS_SQR))
						{
							Vec3V normal(V_Z_AXIS_WZERO);

							if(pOrigBoundGeom->GetNumPolygons() > 0)  
							{
								Vec3V normalAccum(V_ZERO);
								for( int j=0;j<pOrigBoundGeom->GetNumPolygons();j++)
								{
									normalAccum += pOrigBoundGeom->GetPolygonUnitNormal(j);
								}

								normal = m_pParentVehicle->GetTransform().Transform3x3(NormalizeSafe(normalAccum, normal));
							}

							vehicleDebugf3("CVehicleDeformation::ApplyDamageToBound - Smashing window with %f deform mag. Threshold = %f.",sqrt(maxDamageMagSquared.Getf()),MIN_DEFORM_MAG_TO_SMASH_GLASS);
							g_vehicleGlassMan.SmashCollision(m_pParentVehicle,i, VEHICLEGLASSFORCE_WINDOW_DEFORM, normal);
						}
					}
				}
			}
		}
	}
#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
	if (needsUpdateListCount > 0)
	{
#if ENABLE_APPLY_DAMAGE_PF
		PF_FUNC(RecomputeOctantMap);
		PF_INCREMENTBY(RecomputeOctantMap,needsUpdateListCount);
#endif // ENABLE_APPLY_DAMAGE_PF
		//sysTimer timer;
		fragMemStartCacheHeapFunc(pFragInst->GetCacheEntry());
		{
			for (int i = 0 ; i < needsUpdateListCount ; i++)
				needsUpdateList[i]->RecomputeOctantMap();
		}
		fragMemEndCacheHeap();
		//float time = timer.GetUsTime();
		//Displayf("Took %f to regenerate octant maps for %d bounds.",time,needsUpdateListCount);
	}
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
	}

	// recalculate bounding box and sphere for the composite bound
	pCloneBound->CalculateCompositeExtents();
	if(!m_pParentVehicle->IsColliderArticulated())
	{
		PHLEVEL->UpdateCompositeBvh(pFragInst->GetLevelIndex());
	}

	// we have probably changed the bound radius of the fragInst so need to tell the level that's changed if it has.
	if(pFragInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid() != fOriginalBoundRadius)
		CPhysics::GetLevel()->UpdateObjectLocationAndRadius(pFragInst->GetLevelIndex(),(Mat34V_Ptr)(NULL));

#if 1
	fragCacheEntry* entry = m_pParentVehicle->GetFragInst()->GetCacheEntry();
	ENABLE_FRAG_OPTIMIZATION_ONLY(if( entry ))
	{
		fragHierarchyInst* pFragHierInst = entry->GetHierInst();
		if( pFragHierInst && pFragHierInst->envCloth )
		{
			phVerletCloth* pCloth = pFragHierInst->envCloth->GetCloth();
			Assert( pCloth );

			phClothData& clothData = pCloth->GetClothData();
			if( clothData.GetPrevPositionVertexCount() )
			{
				Vec3V* RESTRICT pinVerts = clothData.GetPinVertexPointer();
				Vec3V* RESTRICT damageOffsets = clothData.GetVertexPrevPointer();
#if NO_PIN_VERTS_IN_VERLET
				int numPinVerts = clothData.GetNumPinVerts();
#else
				int numPinVerts = pCloth->GetPinCount();
#endif	
				for(int i = 0; i < numPinVerts; ++i )
				{
					damageOffsets[i] = ReadFromVectorOffset(basePtr, pinVerts[i], false);
					clothDebugf1(" Cloth vehicle damage:  %f  %f  %f ", damageOffsets[i].GetXf(), damageOffsets[i].GetYf(), damageOffsets[i].GetZf() );
				}
			}
		}	
	}
#endif // 0 
}


void CVehicleDeformation::DamageTextureAllocate(bool bResetDamage /* = true */)
{
	CCustomShaderEffectVehicle* pShader = m_pParentVehicle ? static_cast<CCustomShaderEffectVehicle*>(m_pParentVehicle->GetDrawHandler().GetShaderEffect()) : NULL;
	if (!m_pParentVehicle || !Verifyf(pShader, "Failed to find shader for damage"))
	{
		return;
	}

#if VEHICLE_DAMAGE_ON
	const int basePriority = GetTexCachePriority();
	int minPriority = basePriority;
	int entryIdx = -1;

	const int cacheSize = GetTextureCacheSize();
	for(int i=0;i<cacheSize;i++)
	{
		if( ms_TextureCache[i].m_pOwner == NULL )
		{
			entryIdx = i;
			break;
		}
		
		if( ms_TextureCache[i].m_nPriority < minPriority )
		{
			minPriority = ms_TextureCache[i].m_nPriority;
			entryIdx = i;
			// No break : we're looking for the lowest priority possible.
		}
	}

	if(entryIdx != -1)
	{
		if(ms_TextureCache[entryIdx].m_pOwner)
		{	// release from previous owner:
			CVehicleDeformation::FreeEntry(ms_TextureCache[entryIdx]);
		}

		ms_TextureCache[entryIdx].m_pOwner		= this;
		ms_TextureCache[entryIdx].m_nPriority	= basePriority;

#if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
 #if VEHICLE_ROLLING_TEXTURE_ARRAY
		VehTexCacheEntry &entry = (ms_TextureCache[entryIdx]);
		pShader->SetDamageTex(entry.m_DamageTextures[entry.m_ActiveCPUIndex].GetTexture());
 #else
		pShader->SetDamageTex(ms_TextureCache[entryIdx].m_Payloads[0].GetTexture());
 #endif
#else
		pShader->SetDamageTex(ms_TextureCache[entryIdx].m_Payload.GetTexture());
#endif

		m_nTextureCacheIdx = entryIdx;
		m_pParentVehicle->CloneBounds();

		if (bResetDamage)
		{
			ResetDamage();
		}
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		else
		{
			ms_TextureCache[entryIdx].m_HasDamageDrawn = true;
		}
#endif
	}
#endif // VEHICLE_DAMAGE_ON
}

void CVehicleDeformation::DamageTextureFree()
{
#if VEHICLE_DAMAGE_ON
	if (m_pParentVehicle && HasDamageTexture())
	{
		Assert(m_nTextureCacheIdx != -1);
		CVehicleDeformation::FreeEntry(ms_TextureCache[m_nTextureCacheIdx]);
	}
#endif // VEHICLE_DAMAGE_ON
}

#define TC_PRIORITY_DEFAULT		0x00000001
#define TC_PRIORITY_MISSION		0x00000002
#define TC_PRIORITY_OWNEDPLAYER	0x00000004
#define TC_PRIORITY_PLAYER		0x00000008

int CVehicleDeformation::GetTexCachePriority()
{
#if VEHICLE_DAMAGE_ON
	s32 priority = TC_PRIORITY_DEFAULT;

	if(!m_pParentVehicle)
		return priority;

	if (m_pParentVehicle->GetDriver())
	{
		if( m_pParentVehicle->GetDriver()->PopTypeIsMission() )
		{
			priority |= TC_PRIORITY_MISSION;
		}
		else if ( m_pParentVehicle->GetDriver()->IsPlayer() )
		{
			priority |= TC_PRIORITY_PLAYER;
		}
	}

	// Test for status, but isn't the driver test enough ?
	if( m_pParentVehicle->GetStatus() == STATUS_PLAYER )
	{
		priority |= TC_PRIORITY_PLAYER;
	}

	if( m_pParentVehicle->PopTypeIsMission() )
	{
		priority |= TC_PRIORITY_MISSION;
	}

	if( m_pParentVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer == true )
	{
		priority |= TC_PRIORITY_OWNEDPLAYER;
	}

	return priority;
#else // VEHICLE_DAMAGE_ON
	return 0;
#endif // VEHICLE_DAMAGE_ON
}

void CVehicleDeformation::FreeEntry(VehTexCacheEntry &
#if VEHICLE_DAMAGE_ON
                                    entry
#endif
                                    )
{
#if VEHICLE_DAMAGE_ON
	Assert(entry.m_pOwner);
	//entry.m_pOwner->m_bSwitchOn = false; // The damage code will turn it on, but we need to turn it off manually.
	//entry.m_pOwner->m_pDamageTexture = NULL;

	CVehicle* pVehicle = entry.m_pOwner->m_pParentVehicle;
	Assert(pVehicle);

#if GPU_DAMAGE_WRITE_ENABLED
	CPhysics::RemoveVehicleDeformation(pVehicle);
	CApplyDamage::RemoveVehicleDeformation(pVehicle);
	pVehicle->SetDamageUpdatedByGPU(false, false);
#endif

	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pVehicle->GetDrawHandler().GetShaderEffect());
	Assert(pShader);

	pShader->SetEnableDamage(false);
	pShader->SetDamageTex(NULL);

	if (fragInst* inst = pVehicle->GetFragInst())
	{
		if (fragCacheEntry* entry = inst->GetCacheEntry())
		{
			entry->DecloneBoundParts();
		}
	}

	entry.m_pOwner->m_nTextureCacheIdx = -1;
	entry.m_pOwner = NULL;
	entry.m_nPriority = -1;

#endif // VEHICLE_DAMAGE_ON
}

static u32 ConfigureTextureCacheSize(float m)
{
    float multiplier = m;
#if __PS3 || __XENON
	switch (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
	{
	case LANGUAGE_KOREAN:
        multiplier *= 0.75f;
	case LANGUAGE_JAPANESE:
        multiplier *= 0.50f;
	case LANGUAGE_CHINESE:
	case LANGUAGE_CHINESE_SIMPLIFIED:
        multiplier *= 0.25f;
	default:
		break;
	}
#endif

    u32 cacheSize = u32(VEHICLE_DEFORMATION_TEXTURE_CACHE_SIZE_INIT * multiplier);
	cacheSize = rage::Max((s32)cacheSize, 4);
    return cacheSize;
}

grcTexture* CVehicleDeformation::CreateDamageTexture(int width, int height)
{
#if GPU_DAMAGE_WRITE_ENABLED

	grcTextureFactory::TextureCreateParams param(grcTextureFactory::TextureCreateParams::STAGING,
		grcTextureFactory::TextureCreateParams::LINEAR,	grcsRead, NULL, 
		grcTextureFactory::TextureCreateParams::RENDERTARGET, grcTextureFactory::TextureCreateParams::MSAA_NONE);

#if RSG_DURANGO
	grcTexture* texture = grcTextureFactory::GetInstance().Create(width, height, grctfA8B8G8R8_SNORM, NULL, 1U, &param);

	grcTextureLock textureLock;
	texture->LockRect(0, 0, textureLock, grcsWrite);
	sysMemSet((s8 *)textureLock.Base, 0, width * height * sizeof(s8) * 4);
	texture->UnlockRect(textureLock);
#elif !RSG_PC
	sysMemStartTemp();
	TexelValue_R8G8B8A8_SNORM* buffer = rage_new TexelValue_R8G8B8A8_SNORM[width * height];
	sysMemEndTemp();

	BANK_ONLY(grcTexture::SetCustomLoadName("CVehicleDeformation::CreateDamageTexture");)
	sysMemSet((s8 *)buffer, 0, width * height * sizeof(s8) * 4);
	grcTexture* texture = grcTextureFactory::GetInstance().Create(width, height, grctfA8B8G8R8_SNORM, buffer, 1U, &param);
	BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)

	sysMemStartTemp();
	delete [] buffer;
	sysMemEndTemp();
#else
	grcTexture* texture = grcTextureFactory::GetInstance().Create(width, height,
		VEHICLE_DEFORMATION_USE_HALF_FLOATS ? grctfA16B16G16R16F : grctfA8B8G8R8_SNORM, NULL, 1U, &param);
#endif // RSG_DURANGO

#else // GPU_DAMAGE_WRITE_ENABLED

#if VEHICLEDAMAGE_USE_LINEAR_TEXTURE
	grcTextureFactory::TextureCreateParams param(grcTextureFactory::TextureCreateParams::SYSTEM, grcTextureFactory::TextureCreateParams::LINEAR);
#else 
	grcTextureFactory::TextureCreateParams param(grcTextureFactory::TextureCreateParams::SYSTEM, grcTextureFactory::TextureCreateParams::TILED);
#endif

	sysMemStartTemp();
	float* buffer = rage_new float[width * height];
	sysMemEndTemp();

	sysMemSet(buffer, 0, width * height * sizeof(float));

#if RSG_PS3
	u32 format = (u32)grcImage::R32F;
#elif RSG_XENON

#if VEHICLEDAMAGE_USE_LINEAR_TEXTURE
	u32 format = (u32)MAKELINFMT(D3DFMT_R32F);
#else 
	u32 format = (u32)D3DFMT_R32F;
#endif

#else
	u32 format = (u32)grctfR32F;
#endif

	grcTexture* texture = grcTextureFactory::GetInstance().Create(width, height, format, buffer, 1U, &param);

#if !RSG_XENON
	sysMemStartTemp();
	delete [] buffer;
	sysMemEndTemp();
#endif

#endif // GPU_DAMAGE_WRITE_ENABLED

	return texture;
}


grcRenderTarget* CVehicleDeformation::CreateDamageRenderTarget(grcTexture* texture, int width, int height, int index, bool lockable)
{
	const char* baseName =  (height > 1) ? "DamageRT_" : "BoundsRT_";
	size_t digitLength = (index == 0) ? 1 : (size_t)log10((double)index) + 1;
	size_t characterCount = strlen(baseName) + digitLength + 1; // null terminated
	char* targetName = rage_new char[characterCount];
	targetName[characterCount - 1] = 0;

	sprintf(targetName, "%s%d", baseName, index);

	grcRenderTarget* renderTarget = NULL;

	if (texture)
	{
		grcTextureObject* texObj  = texture->GetTexturePtr();
		Assert(texObj);

		renderTarget = grcTextureFactory::GetInstance().CreateRenderTarget(targetName, texObj);
		Assert(renderTarget);
		delete targetName;
		return renderTarget;
	}

#if GPU_DAMAGE_WRITE_ENABLED
# if VEHICLE_DEFORMATION_USE_HALF_FLOATS
	grcTextureFormat textureFormat = grctfA16B16G16R16F;
	int bitsPerPixel		 = 64;
# else
	grcTextureFormat textureFormat = grctfA8B8G8R8_SNORM;
	int bitsPerPixel		 = 32;
# endif
#else
	grcTextureFormat textureFormat = grctfR32F;
	int bitsPerPixel		 = 32;
#endif //GPU_DAMAGE_WRITE_ENABLED

	grcRenderTargetType type = grcrtPermanent;

	grcTextureFactory::CreateParams params;
	params.UseFloat		= true;
	params.Multisample	= 0;
	params.HasParent	= true;
	params.Parent		= NULL;
	params.IsResolvable = true;
	params.IsRenderable = true;
	params.UseHierZ		= false;
	params.Format		= textureFormat;
	params.Lockable		= lockable;

#if RSG_ORBIS
	params.ForceNoTiling   = true;
	params.EnableFastClear = true;
#endif

	grcRenderTarget* rt = grcTextureFactory::GetInstance().CreateRenderTarget(targetName, type, width, height, bitsPerPixel, &params);
	delete targetName;
	return rt;
}

void CVehicleDeformation::TexCacheInit(unsigned /*initMode*/)
{
    TexCacheBuild(false);
}

void CVehicleDeformation::TexCacheBuild(bool forMp)
{
#if VEHICLE_DAMAGE_ON
	int width				 = GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH;
	int height				 = GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT;

	sysMemUseMemoryBucket b(MEMBUCKET_RENDER);
	
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	(void)forMp;
	float multiplier = 1.f;
#else
	float multiplier = forMp ? 0.5f : 1.f;
#endif
	
	int nTextureCacheSize = ConfigureTextureCacheSize(multiplier);

	Assert(ms_TextureCache.GetCapacity()==0);
	ms_TextureCache.Reset();
	ms_TextureCache.Reserve(nTextureCacheSize);
	ms_TextureCache.Resize(nTextureCacheSize);

	const int cacheSize   = GetTextureCacheSize();
	const int bufferCount = 1;

	for(int i=0; i < cacheSize; i++)
	{
		// create a texture from the image
		ms_TextureCache[i].m_nPriority = -1;
		ms_TextureCache[i].m_pOwner    = NULL;

		for(int bufferIndex = 0; bufferIndex < bufferCount; bufferIndex++)
		{
			grcTexture* texture = NULL;

#if GPU_DAMAGE_WRITE_ENABLED

# if RSG_ORBIS
			texture = CreateDamageRenderTarget(NULL, width, height, i, true);
			ms_TextureCache[i].m_Payload.SetTexture(texture);
# elif __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
			ms_TextureCache[i].m_HasDamageDrawn = false;
			ms_TextureCache[i].m_ActiveCPUIndex = 0;
			ms_TextureCache[i].m_ActiveGPUIndex = 0;
			ms_TextureCache[i].m_DamageMutex = rage::sysIpcCreateMutex();

			ZeroMemory(ms_TextureCache[i].m_FrameToDownloadOn, sizeof(int) *VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE);

			for (long index = 0; index < VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE; index++)
			{
				texture = CreateDamageTexture(width, height);
				ms_TextureCache[i].m_DamageTextures[index].SetTexture(texture);
			}
#else
			texture = CreateDamageTexture(width, height);
			ms_TextureCache[i].m_Payloads[0].SetTexture(texture);
#endif

			bool bSeparate = false;
#  if RSG_PC
			bSeparate = CApplyDamage::UseSeparateRenderTarget();
#  endif //RSG_PC
#if VEHICLE_ROLLING_TEXTURE_ARRAY
			grcRenderTarget* copyRT = CreateDamageRenderTarget(NULL, width, height, i, false);
			ms_TextureCache[i].m_DamageRenderTarget.SetTexture(copyRT);
#else
			grcRenderTarget* copyRT = CreateDamageRenderTarget(bSeparate ? NULL : texture, width, height, i, !bSeparate);
			ms_TextureCache[i].m_Payloads[1].SetTexture(copyRT);
#endif
# else
			texture = CreateDamageTexture(width, height);
			ms_TextureCache[i].m_Payload.SetTexture(texture);
# endif // platforms

#else // GPU_DAMAGE_WRITE_ENABLED
			texture = CreateDamageTexture(width, height);
			ms_TextureCache[i].m_Payload.SetTexture(texture);
			
			void* basePtr = NULL;
			VehTexCacheEntry* entry = &(ms_TextureCache[i]);

# if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
			basePtr = entry->m_Payloads[0].AcquireBasePtr(grcsRead|grcsWrite);
# else
			basePtr = entry->m_Payload.AcquireBasePtr(grcsRead|grcsWrite);
# endif

			// assume texture is non-defragmentable (as created in MainRam)
			// in case it is, then it's necessary register this ptr with defrag system
			Assert( pgBase::IsTrackedAddress(texture)==false );

			if( basePtr )
			{	
# if GPU_DAMAGE_WRITE_ENABLED
				sysMemSet((s8 *)basePtr, 0, GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH * GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT * sizeof(s8) * 4);
# else
				float *pFloatEntry = (float *)basePtr;
				sysMemSet(pFloatEntry, 0, GTA_VEHICLE_DAMAGE_TEXTURE_WIDTH * GTA_VEHICLE_DAMAGE_TEXTURE_HEIGHT*sizeof(float));
# endif				
			}

# if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
			entry->m_Payloads[0].ReleaseBasePtr();
# else
			entry->m_Payload.ReleaseBasePtr();
# endif
#endif // GPU_DAMAGE_WRITE_ENABLED
		}
 	}

#endif // VEHICLE_DAMAGE_ON
}

void CVehicleDeformation::TexCacheUpdate()
{
#if VEHICLE_DAMAGE_ON
	const int cacheSize = GetTextureCacheSize();
	for(int i=0;i<cacheSize;i++)
	{
		if( ms_TextureCache[i].m_pOwner )
		{
			ms_TextureCache[i].m_nPriority = ms_TextureCache[i].m_pOwner->GetTexCachePriority();
		}
	}
#endif // VEHICLE_DAMAGE_ON
}

void CVehicleDeformation::TexCacheShutdown(unsigned /*shutdownMode*/)
{
#if VEHICLE_DAMAGE_ON
	Assert(GetTextureCacheSize() > 0);

	const int cacheSize = GetTextureCacheSize();
	for(int i=0;i<cacheSize;i++)
	{
		Assert(ms_TextureCache[i].m_pOwner == NULL);
		// create a texture from the image
		ms_TextureCache[i].m_nPriority = -1;
		ms_TextureCache[i].m_pOwner = NULL;

#if	GPU_DAMAGE_WRITE_ENABLED && __D3D11
#if VEHICLE_ROLLING_TEXTURE_ARRAY
		for (long index = 0; index < VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE; index++)
		{
			ms_TextureCache[i].m_DamageTextures[index].GetTexture()->Release();
			ms_TextureCache[i].m_DamageTextures[index].ClearTexture();
		}
		ms_TextureCache[i].m_DamageRenderTarget.GetTexture()->Release();
		ms_TextureCache[i].m_DamageRenderTarget.ClearTexture();
		sysIpcDeleteMutex(ms_TextureCache[i].m_DamageMutex);
		ms_TextureCache[i].m_ActiveCPUIndex = 0;
		ms_TextureCache[i].m_ActiveGPUIndex = 0;
		ZeroMemory(ms_TextureCache[i].m_FrameToDownloadOn, sizeof(int) *VEHICLE_DEFORMATION_TEXTURE_ARRAY_SIZE);

#else
		ms_TextureCache[i].m_Payloads[0].GetTexture()->Release();
		ms_TextureCache[i].m_Payloads[0].ClearTexture();

		ms_TextureCache[i].m_Payloads[1].GetTexture()->Release();
		ms_TextureCache[i].m_Payloads[1].ClearTexture();
#endif
#else
		ms_TextureCache[i].m_Payload.GetTexture()->Release();
		ms_TextureCache[i].m_Payload.ClearTexture();
#endif
	}

	ms_TextureCache.Reset();
#endif // VEHICLE_DAMAGE_ON
}

#if __BANK
void CVehicleDeformation::DisplayTexCacheStats()
{
#if VEHICLE_DAMAGE_ON
	static dev_s32 StartX = 100;
	static dev_s32 StartY = 10;
	static int maxUsage = 0;
	static int hit = 0;
	static int accum = 0;

	int y = StartY;
	int usage = 0;

	char debugText[50];

	const int cacheSize = GetTextureCacheSize();
	for(int i=0;i<cacheSize;i++)
	{
		sprintf(debugText,"Slot %2d: %p pri: %2d",i, ms_TextureCache[i].m_pOwner, ms_TextureCache[i].m_nPriority);
		grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
		y++;
		if( ms_TextureCache[i].m_pOwner )
		{
			usage++;
		}
	}
	y++;

	if( usage > maxUsage )
	{
		maxUsage = usage;
	}

	hit++;
	accum += usage;

	sprintf(debugText,"Usage avg: %2d max: %2d",(s32)((float)accum/(float)hit), maxUsage);
	grcDebugDraw::PrintToScreenCoors(debugText,StartX,y);
	y++;
#endif // VEHICLE_DAMAGE_ON
}

void CVehicleDeformation::ClearTexCache()
{
#if VEHICLE_DAMAGE_ON
	const int cacheSize = GetTextureCacheSize();
	for(int i=0;i<cacheSize;i++)
	{
		if( ms_TextureCache[i].m_pOwner )
		{
			CVehicleDeformation::FreeEntry(ms_TextureCache[i]);
		}
	}
#endif //VEHICLE_DAMAGE_ON...
}
#endif // __BANK...

float CVehicleDamage::ms_fPetrolTankFireBurnRateMin = 60.0f;
float CVehicleDamage::ms_fPetrolTankFireBurnRateMax = 150.0f;

bank_float CVehicleDamage::sfVehDamPetrolTankApplyGunDamageMult = 3.0f;
bank_float CVehicleDamage::sfVehDamPetrolTankApplyGunDamageMultAI = 1.0f;
bank_float CVehicleDamage::ms_fChanceToBreak = 0.30f;
dev_float sfVehDamPetrolTankApplyFireDamageMult = 0.5f;
dev_float sfWeaponDrivebyForceDamageFrac = 0.7f;
dev_float sfUpsideDownEngineDamageMult = 2.0f;

float CVehicleDamage::ms_fBreakingPartsReduction = 0.0f;

#if GPU_DAMAGE_WRITE_ENABLED
bool CVehicleDamage::ms_bEnableGPUDamage = true;
#if __BANK
int CVehicleDamage::ms_iForcedDamageEveryFrame = 0;
#endif

#endif

CVehicleDamage::CVehicleDamage()
{
	m_pParent = NULL;
	m_fBodyHealth = VEH_DAMAGE_HEALTH_STD;
	m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_STD;
	m_fBodyDamageScale = 1.0f;
	m_fPetrolTankDamageScale = 1.0f;
	m_fCollisionWithMapDamageScale = 1.0f;

    m_fPetrolTankLevel = VEH_DAMAGE_PETROL_LEVEL_STD;
    m_fOilLevel = VEH_DAMAGE_OIL_LEVEL_STD;

	m_pEntityThatSetUsOnFire = NULL;
	m_vecOldMoveSpeed.Zero();
	m_vecOldTurnSpeed.Zero();
	m_UpdateLightBones = false;
	m_UpdateSirenBones = false;
    m_fCountDownToTimeAnotherPartCanBreakOff = 0.0f;
	m_vPetrolSprayPosLocal.Zero();
	m_vPetrolSprayNrmLocal.Zero();
	m_bHasHandBrake = true;
	m_bDisableDamageWithPickedUpEntity = false;
	m_fPadShakeIntensity = 0.0f;
	m_uPadShakeDuration = 0;
	m_uBlowUpCarPartsPending = Break_Off_Car_Parts_Immediately;

	m_fDynamicSpoilerDamage = 0.0f;
	m_fScriptDamageScale = 1.0f;
	m_fScriptWeaponDamageScale = 1.0f;

	ResetStuckCheck();
}

CVehicleDamage::~CVehicleDamage()
{
}

dev_float VEHICLE_EXPLODE_IMPULSE_THRESHOLD = 15.0f;	// Mas normalised (so basically v delta t)
dev_float VEHICLE_EXPLODE_UPRIGHT_ANGLE_COS = 0.0f;	// Angles are from vertical. 0.707 = cos(45deg)
dev_float VEHICLE_EXPLODE_NORMAL_ANGLE_COS = 0.707f;
dev_float VEHICLE_EXPLODE_WHEN_ON_FIRE_IMPACT = 2.0f;
dev_float VEHICLE_EXPLODE_AGAINST_TRAIN_IMPULSE_THRESHOLD = 25.0f;

dev_float sfHeliCrashThreshold = 4.5f;
dev_float sfHeliCrashNoRotorsThreshold = 0.5f;
dev_float sfHeliCrashOccupentHealthMult = 10.0f;
dev_float sfHeliCrashExplode = 25.0f;

void CVehicleDamage::Init(CVehicle* pParentVehicle)
{
	m_fBodyHealth = GetDefaultBodyHealthMax(pParentVehicle); 
	m_bHasHandBrake = pParentVehicle->pHandling ?  !(pParentVehicle->pHandling->hFlags & HF_NO_HANDBRAKE) && (pParentVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pParentVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pParentVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || pParentVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
													: false;
	
	SetParent(pParentVehicle);
	GetDeformation()->Init(pParentVehicle);
	RefillOilAndPetrolTanks();
}

void CVehicleDamage::Process(float fTimeStep, bool bBoundsNeedUpdating)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif

	PrefetchObject(this);	// This is a large-ish object and we'll be updating various parts of it in this function, so prefetch the whole thing first 

	const CCollisionHistory * pColHistory = m_pParent->GetFrameCollisionHistory();
	const CCollisionRecord * pColRecord = pColHistory->GetMostSignificantCollisionRecord();
	bool bIsSuperDummy = m_pParent->IsSuperDummy();
	Vector3 vMyCollisionPos;
	if(pColRecord)
	{
		const Matrix34& myMat = RCC_MATRIX34(m_pParent->GetMatrixRef());
		myMat.Transform(pColRecord->m_MyCollisionPosLocal, vMyCollisionPos);
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if(!CanVehicleBeDamagedBasedOnDriverInvincibility())
		{
			m_Deformation.ClearStoredImpacts();
			// we still want to update the ground clearance
			if(bBoundsNeedUpdating && !bIsSuperDummy)
			{
				m_Deformation.ApplyDeformations(bBoundsNeedUpdating);
			}
			return;
		}
	}

	// Process the collision.
	m_pParent->GetIntelligence()->ProcessCollision( pColRecord );

	if( CPhysics::ms_bInArenaMode )
	{
		// check for this car getting stuck
		ProcessStuckCheck( fTimeStep );

		// Don't rumble if vehicle is stationary
		if( m_pParent && m_pParent->GetVelocity().Mag2() >= 0.001f )
		{
			ApplyPadShakeInternal();
		}
		if( bBoundsNeedUpdating && !bIsSuperDummy )
		{
			m_Deformation.ApplyDeformations( bBoundsNeedUpdating );
		}
		return;
	}

	// damage for network clones is determined by the machine which owns them
	if (m_pParent->IsNetworkClone())
	{
		const float fCollisionImpulseMagSum = pColHistory->GetCollisionImpulseMagSum();
		if(pColRecord && fCollisionImpulseMagSum > 0.0f)
		{
			ApplyDamage(pColRecord->m_pRegdCollisionEntity.Get(), DAMAGE_TYPE_COLLISION, WEAPONTYPE_RAMMEDBYVEHICLE, 
				fCollisionImpulseMagSum, vMyCollisionPos, pColRecord->m_MyCollisionNormal, 
				Vector3(0,0,0), pColRecord->m_MyCollisionComponent);
		}
	}
	else
	{
		Assert(!pColRecord || pColRecord->m_fCollisionImpulseMag <= pColHistory->GetCollisionImpulseMagSum());
		if(pColRecord && pColRecord->m_fCollisionImpulseMag > 1.0f)
		{
			// Check for vehicle colliding at high speed at an awkward angle
			// If so trigger explosion
			if(	(m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR || m_pParent->GetVehicleType() == VEHICLE_TYPE_BOAT)	// Only do for cars and boats
				&& pColRecord->m_MyCollisionNormal.z > VEHICLE_EXPLODE_NORMAL_ANGLE_COS	// We collided with something from above (ish)
				&& pColHistory->GetCollisionImpulseMagSum() > VEHICLE_EXPLODE_IMPULSE_THRESHOLD*m_pParent->GetMass()
				&& m_pParent->IsInAir()
				&& m_pParent->GetTransform().GetC().GetZf() < VEHICLE_EXPLODE_UPRIGHT_ANGLE_COS		// Check up vector of vehicle
				//&& !m_pParent->ContainsPlayer()										// Do not do this to player car, turned this off because we want cars to blow up more often in crashes
				&& !m_pParent->PopTypeIsMission()
				&& !CStuntJumpManager::IsAStuntjumpInProgress() // don't explode the car when a stunt jump is in progress
				&& !m_pParent->HasIncreasedRammingForce() 
				&& !m_pParent->HasRammingScoop() )
			{
				m_pParent->BlowUpCar(m_pParent);
			}

			// Check for large impulses from a vehicle crashing into a train
			if(m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR && pColRecord->m_pRegdCollisionEntity.Get() && pColRecord->m_pRegdCollisionEntity.Get()->GetIsTypeVehicle())
			{
				CVehicle *pOtherVehicle = static_cast<CVehicle*>(pColRecord->m_pRegdCollisionEntity.Get());
				if(pOtherVehicle->InheritsFromTrain())
				{ 
					if(pColRecord->m_fCollisionImpulseMag > VEHICLE_EXPLODE_AGAINST_TRAIN_IMPULSE_THRESHOLD*m_pParent->GetMass())
					{
						m_pParent->BlowUpCar(pOtherVehicle);
					}
				}

				if(m_pParent->IsDriverAPlayer() && pOtherVehicle->HasAliveLawPedsInIt())
				{
					REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(4000, 4000, IMPORTANCE_NORMAL, CODE, OVERLAP_VEHICLE_DAMAGE);)
				}
			}

			// Check for heli's crashing out of the sky
			if(m_pParent->GetVehicleType()==VEHICLE_TYPE_HELI && 
				(!GetIsDriveable(false) || !m_pParent->IsEngineOn() || m_pParent->GetStatus() == STATUS_ABANDONED || m_pParent->GetStatus() == STATUS_OUT_OF_CONTROL))
			{
				CEntity* pInflictor = m_pParent;

				if(m_pParent->GetIsRotaryAircraft())
				{
					CRotaryWingAircraft * pRotaryWingAircraft = static_cast<CRotaryWingAircraft*>(m_pParent);

					if( pRotaryWingAircraft->GetHeliRotorDestroyedByPed())
					{
						if (m_pParent->GetWeaponDamageEntity())
						{
							pInflictor = m_pParent->GetWeaponDamageEntity();
						}
					}
				}

				bool bHeliRotorsMissing = false;
				CHeli *pHeli = static_cast<CHeli*>(m_pParent);
				if(pHeli->GetRearRotorHealth() <= 0.0f || pHeli->GetMainRotorHealth() <= 0.0f)// explode helis that are missing rotors and have hit the ground.
				{
					bHeliRotorsMissing = true;
				}

				float fApplyDamage = pColHistory->GetCollisionImpulseMagSum() * InvertSafe(m_pParent->pHandling->m_fMass, m_pParent->GetInvMass());		

				if( !bHeliRotorsMissing &&
					!CanVehicleBeDamaged(pInflictor, m_pParent->GetWeaponDamageHash(), false) )
				{
					fApplyDamage = 0.0f;
				}

				if( fApplyDamage > sfHeliCrashThreshold || (bHeliRotorsMissing && fApplyDamage > sfHeliCrashNoRotorsThreshold) )
				{
					// if heli falls very hard, there's a chance it'll explode
					float fCrashChance = 0.5f;
					if(!pHeli->IsEngineOn() || pHeli->GetStatus() == STATUS_ABANDONED || pHeli->GetStatus() == STATUS_OUT_OF_CONTROL)
					{
						fCrashChance = 1.0f;
					}

					if( (fApplyDamage > sfHeliCrashExplode  && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= fCrashChance) || (bHeliRotorsMissing && fApplyDamage > sfHeliCrashNoRotorsThreshold) )
					{
						m_pParent->BlowUpCar(pInflictor);
					}
					else // otherwise it just hurts the occupants
					{
						if(fApplyDamage > sfHeliCrashExplode && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > 0.5f)
							m_pParent->m_Transmission.ApplyEngineDamage(m_pParent, pInflictor, DAMAGE_TYPE_COLLISION, 1000.0f, true);

						for(s32 i=0; i<m_pParent->GetSeatManager()->GetMaxSeats(); i++)
						{
							CPed* pPed = m_pParent->GetSeatManager()->GetPedInSeat(i);

							if(pPed)
							{
								if(NetworkUtils::IsNetworkCloneOrMigrating(pPed))
								{
									CWeaponDamageEvent::Trigger(pInflictor, pPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), 0, true, WEAPONTYPE_RAMMEDBYVEHICLE, fApplyDamage * sfHeliCrashOccupentHealthMult, -1, -1, CPedDamageCalculator::DF_None, 0, 0, 0);
								}
								else
								{
									CEventDamage tempDamageEvent(pInflictor, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_RAMMEDBYVEHICLE);
									CPedDamageCalculator damageCalculator(pInflictor, fApplyDamage * sfHeliCrashOccupentHealthMult, WEAPONTYPE_RAMMEDBYVEHICLE, 0, false);
									damageCalculator.ApplyDamageAndComputeResponse(pPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);

									if(tempDamageEvent.AffectsPed(pPed))
										pPed->GetPedIntelligence()->AddEvent(tempDamageEvent);
								}
							}
						}
					}
				}
			}

			CEntity* pCollidedEntity = pColRecord->m_pRegdCollisionEntity.Get();
			float fDamage = pColHistory->GetCollisionImpulseMagSum();

			ApplyDamage(pCollidedEntity, DAMAGE_TYPE_COLLISION, WEAPONTYPE_RAMMEDBYVEHICLE,
				fDamage, vMyCollisionPos, pColRecord->m_MyCollisionNormal,
				Vector3(0,0,0), pColRecord->m_MyCollisionComponent);

			// For vehicle to vehicle collisions we will trigger the car alarm
			if (pColRecord->m_pRegdCollisionEntity.Get() && pColRecord->m_pRegdCollisionEntity->GetIsTypeVehicle())
			{
				if (!m_pParent->GetDriver()) // If this car has a driver triggering the alarm would be silly (sometimes happenend in network games)
				{
					if(m_pParent->GetStatus()!=STATUS_WRECKED)
					{
						m_pParent->TriggerCarAlarm();
					}
				}
				// this should get processed for both vehicles, no need to trigger both ways?
				//((CVehicle *)m_pParent->GetLatestCollisionEntity())->TriggerCarAlarm();

				// Cops & Crooks: Vehicle collisions should incur endurance damage to Crooks
				if (pCollidedEntity && pCollidedEntity->GetIsTypeVehicle())
				{
					bool bCalculatedEnduranceDamage = false;
					float fEnduranceDamage = 0.0f;

					s32 maxSeats = m_pParent->GetSeatManager()->GetMaxSeats();
					for (s32 seatIndex = 0; seatIndex < maxSeats; ++seatIndex)
					{
						CPed* pPassenger = m_pParent->GetSeatManager()->GetPedInSeat(seatIndex);
						if (pPassenger && pPassenger->GetPlayerInfo() && pPassenger->GetPlayerInfo()->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_CROOK)
						{
							if (!bCalculatedEnduranceDamage)
							{
								if (fDamage >= sm_Tunables.m_MinImpulseForEnduranceDamage)
								{
									const float fImpulseDamageScale = ClampRange(fDamage, sm_Tunables.m_MinImpulseForEnduranceDamage, sm_Tunables.m_MaxImpulseForEnduranceDamage);
									const float fImpulseMultiplier = 1.0f - (square(1.0f - fImpulseDamageScale)); // Inverse exponential scale
									fEnduranceDamage = sm_Tunables.m_MinEnduranceDamage + (sm_Tunables.m_MaxEnduranceDamage - sm_Tunables.m_MinEnduranceDamage) * fImpulseMultiplier;

									CVehicle* pOtherVehicle = static_cast<CVehicle*>(pCollidedEntity);
									bool bIsOwnFault = false;
									const float fAngleSameDirRadians = DEGREES_TO_RADIANS(sm_Tunables.m_AngleVectorsPointSameDir);
									if (PhysicsHelpers::FindVehicleCollisionFault(m_pParent, pOtherVehicle, bIsOwnFault, fAngleSameDirRadians, sm_Tunables.m_MinFaultVelocityThreshold)
										&& bIsOwnFault)
									{
										fEnduranceDamage *= sm_Tunables.m_InstigatorDamageMultiplier;
									}
									vehicleDebugf3("[ENDURANCE] Raw vehicle damage: %f, Impulse scale: %f, Impulse multiplier: %f, Is instigator: %s, Final endurance damage: %f", fDamage, fImpulseDamageScale, fImpulseMultiplier, bIsOwnFault ? "True" : "False", fEnduranceDamage);
								}
#if !__NO_OUTPUT
								else
								{
									vehicleDebugf3("[ENDURANCE] Endurance damage not applied: fDamage (%.2f) < sm_Tunables.m_MinImpulseForEnduranceDamage (%.2f) ", fDamage, sm_Tunables.m_MinImpulseForEnduranceDamage);
								}
#endif	// !__NO_OUTPUT

								bCalculatedEnduranceDamage = true;
							}

							if (fEnduranceDamage > SMALL_FLOAT)
							{
								if (NetworkUtils::IsNetworkCloneOrMigrating(pPassenger))
								{
									CWeaponDamageEvent::Trigger(m_pParent,
										pPassenger,
										VEC3V_TO_VECTOR3(pPassenger->GetTransform().GetPosition()),
										0,
										true,
										WEAPONTYPE_RAMMEDBYVEHICLE,
										fEnduranceDamage,
										-1,
										-1,
										CPedDamageCalculator::DF_EnduranceDamageOnly,
										0,
										0,
										0);
								}
								else
								{
									CEventDamage tempDamageEvent(pCollidedEntity, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_RAMMEDBYVEHICLE);
									CPedDamageCalculator damageCalculator(pCollidedEntity, fEnduranceDamage, WEAPONTYPE_RAMMEDBYVEHICLE, 0, false);
									damageCalculator.ApplyDamageAndComputeResponse(pPassenger, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_EnduranceDamageOnly);
									
									// Trigger directional endurance hit markers.
									PostFX::RegisterBulletImpact(VEC3V_TO_VECTOR3(pCollidedEntity->GetTransform().GetPosition()), 1.0f, true);
								}
							}
						}
					}
				}
			}
		}

		const CVehicleModelInfo* pModelInfo = m_pParent->GetVehicleModelInfo();
		if (!bIsSuperDummy && pModelInfo && !pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_ELECTRIC))
		{
			ProcessOilLeak(fTimeStep);
			ProcessPetrolTankDamage(fTimeStep);
		}

		ProcessFuelConsumption(fTimeStep);

		if(m_pParent->m_nVehicleFlags.bBlowUpWhenMissingAllWheels)
		{
			int numWheels = m_pParent->GetNumWheels();
			if(numWheels > 0)
			{
				CWheel* const * wheels = m_pParent->GetWheels();
				int numBrokenOffWheels = 0;
				for(int i = 0; i < numWheels; i++)
				{
					if(wheels[i]->GetDynamicFlags().IsFlagSet(WF_BROKEN_OFF))
						numBrokenOffWheels++;
				}

				if(numBrokenOffWheels == numWheels)
				{
					m_pParent->BlowUpCar(m_pParent);
				}
			}
		}

		m_pParent->m_Transmission.ProcessEngineFire(m_pParent, fTimeStep);

		// check for this car getting stuck
		ProcessStuckCheck(fTimeStep);
	}

	// Needs to be called on clones and non-clones
	const float fPetrolTankHealth = GetPetrolTankHealth();
	if(!bIsSuperDummy && fPetrolTankHealth>VEH_DAMAGE_HEALTH_PTANK_FINISHED && fPetrolTankHealth<VEH_DAMAGE_HEALTH_PTANK_ONFIRE)
	{
		CPed* pCulPrit = NULL;
		CEntity* damager = m_pParent->GetWeaponDamageEntity();
		if (damager)
		{
			if (damager->GetIsTypePed())
			{
				pCulPrit = static_cast<CPed*>(damager);
			}
			else if (damager->GetIsTypeVehicle())
			{
				CVehicle* vehicle = static_cast<CVehicle*>(damager);

				pCulPrit = vehicle->GetDriver();
				if(!pCulPrit && vehicle->GetSeatManager())
				{
					pCulPrit = vehicle->GetSeatManager()->GetLastPedInSeat(0);
				}
			}
		}

		g_vfxVehicle.ProcessPetrolTankDamage(m_pParent, pCulPrit);
	}

	m_fCountDownToTimeAnotherPartCanBreakOff = rage::Max(0.0f,m_fCountDownToTimeAnotherPartCanBreakOff-fTimeStep);

	if(!bIsSuperDummy)
	{
		m_Deformation.ApplyDeformations(bBoundsNeedUpdating);

		int nNumWheels = m_pParent->GetNumWheels();
		for(int nWheel=0; nWheel < nNumWheels; nWheel++)
		{
			CWheel* wheel = m_pParent->GetWheel(nWheel);
			wheel->ProcessWheelDamage(fTimeStep);

			// Might break off
			const float chanceToBreak = CVehicleDamage::ms_fChanceToBreak * (m_pParent->IsLawEnforcementVehicle() ? 0.5f : 1.0f);
			const float fxChance = wheel->GetTyreHealth() > 0.0f ? 0.12f : 0.03f;
			const float deleteChance = 0.29f;
			const float burstTyreChance = 0.21f;
			if(wheel->GetTyreShouldBreakFromDamage() && !m_pParent->IsNetworkClone())
			{
				if(NetworkInterface::IsGameInProgress())
				{
					CVehicle* pInflictor = NULL;
					if(m_pParent && m_pParent->GetWeaponDamageEntity())
					{
						CEntity* pDamageEnt = m_pParent->GetWeaponDamageEntity();
						if(pDamageEnt && pDamageEnt->GetIsTypeVehicle())
						{
							pInflictor = static_cast<CVehicle*>(m_pParent->GetWeaponDamageEntity());
						}
					}
					if(pInflictor && (pInflictor->GetMass() > sfMinimumMassForCrushingVehiclesNetworkGame))
					{
						if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < (chanceToBreak * 0.1f))
						{
							BreakOffWheel(nWheel, fxChance, deleteChance, burstTyreChance);
						}
					}
				}
				else
				{
					if(wheel->GetFrictionDamage() >= CWheel::ms_fFrictionDamageThreshForBreak && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < chanceToBreak)
					{
						BreakOffWheel(nWheel, fxChance, deleteChance, burstTyreChance);
					}
				}

				wheel->GetDynamicFlags().ClearFlag(WF_WITHIN_HEAVYDAMAGE_REGION);
			}
		}

		// Don't rumble if vehicle is stationary
		if(m_pParent && m_pParent->GetVelocity().Mag2() >= 0.001f)
		{
			ApplyPadShakeInternal();
		}
	}
}

void CVehicleDamage::ProcessOilLeak(float fTimeStep)
{
	Assert(m_pParent);
    if (m_pParent->CanLeakOil())
    {
        // check if the engine health says we should leak oil
        float engineHealth = GetEngineHealth();
        if (engineHealth<ENGINE_DAMAGE_OIL_LEAKING)
        {
            // calc oil damage (0.1 at ENGINE_DAMAGE_OIL_LEAKING, 1.0 at ENGINE_DAMAGE_ONFIRE)
            float oilLeakRate = 1.0f;
            if (engineHealth>ENGINE_DAMAGE_ONFIRE)
            {
                oilLeakRate = CVfxHelper::GetInterpValue(engineHealth, ENGINE_DAMAGE_OIL_LEAKING, ENGINE_DAMAGE_ONFIRE, true);
            }
            oilLeakRate += 0.1f;// Make sure we always leak some oil when we are in the ENGINE_DAMAGE_OIL_LEAKING state.

            oilLeakRate *= ENGINE_DAMAGE_OIL_LEAK_RATE;
            m_fOilLevel -= oilLeakRate * fTimeStep;

            if(m_fOilLevel < 0.0f)
            {
                m_fOilLevel = 0.0f;
            }


            // Damage the engine when running low on oil and the engines on
            float fOilLevelFraction = m_fOilLevel/m_pParent->pHandling->m_fOilVolume;
            if( m_pParent->m_nVehicleFlags.bEngineOn && fOilLevelFraction < ENGINE_DAMAGE_OIL_FRACTION_BEFORE_DAMAGE && engineHealth > ENGINE_DAMAGE_ONFIRE)
            {
                float fOilLevelFractionDamageMult = (ENGINE_DAMAGE_OIL_FRACTION_BEFORE_DAMAGE - fOilLevelFraction)/ENGINE_DAMAGE_OIL_FRACTION_BEFORE_DAMAGE;

                //increase the amount of damage done by running the car low on oil based on the amount of oil left and how much the car is being revved
                float fDamage = ENGINE_DAMAGE_OIL_LOW * fOilLevelFractionDamageMult * fabs(m_pParent->m_Transmission.GetRevRatio());

                float damageMult = 1.0f;

                // Increase the damage done to the engine when upside down
                if(m_pParent->IsUpsideDown())
                {
                    damageMult *= sfUpsideDownEngineDamageMult;
                }

                const float damage = fDamage * damageMult * fTimeStep;//make this a damage over time effect.
                m_pParent->m_Transmission.ApplyEngineDamage(m_pParent, m_pParent, DAMAGE_TYPE_NONE, damage);
            }
        }
    }
}

// Tunings from Planes.cpp
extern float dfPlaneEngineMissFireStartingHealth;
extern float sfPlaneEngineBreakDownHealth;
extern float bfPlaneEngineMissFireMinTime;
extern float bfPlaneEngineMissFireMaxTime;
extern float bfPlaneEngineMissFireMinRecoverTime;
extern float bfPlaneEngineMissFireMaxRecoverTime;

// Tunings from Heli.cpp
extern float dfHeliEngineMissFireStartingHealth;
extern float sfHeliEngineBreakDownHealth;
extern float bfHeliEngineMissFireMinTime;
extern float bfHeliEngineMissFireMaxTime;
extern float bfHeliEngineMissFireMinRecoverTime;
extern float bfHeliEngineMissFireMaxRecoverTime;

// Global object from audio/scannerManager.cpp 
extern audScannerManager g_AudioScannerManager;

void CVehicleDamage::SpewEntityThatSetUsOnFire(const char* DEV_ONLY(text))
{
#if __DEV
	if (!m_pParent)
		return;

	if (!m_pParent->GetNetworkObject())
		return;

	gnetDebug3("'%s' - Vehicle is '%s'", text, m_pParent->GetNetworkObject()->GetLogName());

	if (!m_pEntityThatSetUsOnFire.Get())
	{
		gnetDebug3("...... m_pEntityThatSetUsOnFire is NULL.");
		sysStack::PrintStackTrace( );
		return;
	}

	CEntity* entity = m_pEntityThatSetUsOnFire.Get();

	if (!entity->GetIsPhysical())
	{
		gnetDebug3("...... m_pEntityThatSetUsOnFire is not physical.");
		return;
	}

	if (!static_cast<CPhysical*>(entity)->GetNetworkObject())
	{
		gnetDebug3("...... m_pEntityThatSetUsOnFire does not have a network object.");
		return;
	}

	gnetDebug3("...... m_pEntityThatSetUsOnFire = '%s'", static_cast<CPhysical*>(entity)->GetNetworkObject()->GetLogName());

#endif
}

void CVehicleDamage::ProcessFuelConsumption(float fTimeStep)
{
	// If we are consuming fuel and there was ever some fuel to consume, try to consume it now.
	if (CVehicle::GetConsumePetrol() && !m_pParent->HasInfiniteFuel())
	{
		float newFuelTankLevel = m_fPetrolTankLevel - (m_pParent->GetFuelConsumptionRate() * fTimeStep);

		if (newFuelTankLevel <= 0.0f)
		{
			newFuelTankLevel = 0.0f;

			if (m_pParent->IsEngineOn())
				m_pParent->SwitchEngineOff();
		}

		SetPetrolTankLevel(newFuelTankLevel);
	}

#if __BANK
	if (CVehicle::m_bDebugVisPetrolLevel)
	{
		Vec3V spherePos = m_pParent->GetVehiclePosition();
		Vec3V bumpVec = Vec3V(0.0f, 0.0f, 2.0f);

		grcDebugDraw::Sphere(spherePos + bumpVec, m_fPetrolTankLevel * 0.005f, Color_red);
	}
#endif
}

void CVehicleDamage::ProcessPetrolTankDamage(float fTimeStep)
{
	Assert(m_pParent);
    //@@: range CVEHICLEDAMAGE_PROCESSPETROLTANKDAMAGE {
	bool bForceLeakingPetrol = false;
	bool bForceExplodingPetrolTank = false;
#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_LEAKPETROL
	if(TamperActions::DoVehiclesLeakPetrol() && m_pParent->GetDriver() && m_pParent->GetDriver()->IsLocalPlayer())
	{
		bForceLeakingPetrol = true;
	}
#endif

	if (m_pParent->IsLeakingPetrol() || bForceLeakingPetrol)
	{
		// calc petrol leak rate (0.1 at VEH_DAMAGE_HEALTH_PTANK_LEAKING, 1.0 at VEH_DAMAGE_HEALTH_PTANK_ONFIRE)
		float petrolLeakRate = CVfxHelper::GetInterpValue(m_fPetrolTankHealth, VEH_DAMAGE_HEALTH_PTANK_LEAKING, VEH_DAMAGE_HEALTH_PTANK_ONFIRE, true);
		petrolLeakRate += 0.1f;// Make sure we always leak some petrol when we are in the VEH_DAMAGE_HEALTH_PTANK_LEAKING state.

		petrolLeakRate *= VEH_DAMAGE_PTANK_LEAK_RATE;
		if(bForceLeakingPetrol)
		{
			petrolLeakRate = 2.5f;
		}
		float newTankLevel = m_fPetrolTankLevel - petrolLeakRate * fTimeStep;

		if(newTankLevel <= 0.0f)
		{
			newTankLevel = 0.0f;
			bForceExplodingPetrolTank = bForceLeakingPetrol;
		}
		SetPetrolTankLevel(newTankLevel);
	}
    //@@: } CVEHICLEDAMAGE_PROCESSPETROLTANKDAMAGE

	//Is the petrol so low that we should get a miss fire
	if(m_fPetrolTankLevel > 0.0f)
	{
		if(m_pParent->m_nVehicleFlags.bCanEngineMissFire)
		{
			float fPetrolTankLevelFraction = m_fPetrolTankLevel/m_pParent->pHandling->m_fPetrolTankVolume;
			if(fPetrolTankLevelFraction < VEH_DAMAGE_PTANK_LEVEL_BEFOREMISFIRE)
			{
				//miss fires get longer when lower on petrol and miss fire recoveries get shorter.
				float fPetrolTankLevelFractionMissFireMult = (VEH_DAMAGE_PTANK_LEVEL_BEFOREMISFIRE - fPetrolTankLevelFraction)/VEH_DAMAGE_PTANK_LEVEL_BEFOREMISFIRE;
				float fPetrolTankLevelFractionRecoveryMult = 1.0f - (VEH_DAMAGE_PTANK_LEVEL_BEFOREMISFIRE - fPetrolTankLevelFraction)/VEH_DAMAGE_PTANK_LEVEL_BEFOREMISFIRE;

				if(!m_pParent->m_Transmission.GetCurrentlyMissFiring() && !m_pParent->m_Transmission.GetCurrentlyRecoveringFromMissFiring() )
				{
					m_pParent->m_Transmission.SetMissFireTime( fwRandom::GetRandomNumberInRange(0.5f, 1.5f*fPetrolTankLevelFractionMissFireMult),
						fwRandom::GetRandomNumberInRange(0.5f*fPetrolTankLevelFractionRecoveryMult, 4.0f*fPetrolTankLevelFractionRecoveryMult));
				}
			}
			else if(m_pParent->InheritsFromPlane() && GetEngineHealth() < dfPlaneEngineMissFireStartingHealth)
			{
				float fPlaneEngineHealthFractionMissFireMult = (dfPlaneEngineMissFireStartingHealth - GetEngineHealth())/(dfPlaneEngineMissFireStartingHealth - sfPlaneEngineBreakDownHealth);
				fPlaneEngineHealthFractionMissFireMult = Clamp(fPlaneEngineHealthFractionMissFireMult, 0.0f, 1.0f);
				if(!m_pParent->m_Transmission.GetCurrentlyMissFiring() && !m_pParent->m_Transmission.GetCurrentlyRecoveringFromMissFiring() )
				{
					float fMissingFireTime = fwRandom::GetRandomNumberInRange(bfPlaneEngineMissFireMinTime, bfPlaneEngineMissFireMinTime + (bfPlaneEngineMissFireMaxTime - bfPlaneEngineMissFireMinTime) * fPlaneEngineHealthFractionMissFireMult);
					float fRecoverTime = fwRandom::GetRandomNumberInRange(bfPlaneEngineMissFireMaxRecoverTime + (bfPlaneEngineMissFireMaxRecoverTime - bfPlaneEngineMissFireMinRecoverTime) * fPlaneEngineHealthFractionMissFireMult, bfPlaneEngineMissFireMaxRecoverTime);
					m_pParent->m_Transmission.SetMissFireTime(fMissingFireTime, fRecoverTime);
				}
			}
			else if(m_pParent->InheritsFromHeli() && GetEngineHealth() < dfHeliEngineMissFireStartingHealth)
			{
				float fHeliEngineHealthFractionMissFireMult = (dfHeliEngineMissFireStartingHealth - GetEngineHealth())/(dfHeliEngineMissFireStartingHealth - sfHeliEngineBreakDownHealth);
				fHeliEngineHealthFractionMissFireMult = Clamp(fHeliEngineHealthFractionMissFireMult, 0.0f, 1.0f);
				if(!m_pParent->m_Transmission.GetCurrentlyMissFiring() && !m_pParent->m_Transmission.GetCurrentlyRecoveringFromMissFiring() )
				{
					float fMissingFireTime = fwRandom::GetRandomNumberInRange(bfHeliEngineMissFireMinTime, bfHeliEngineMissFireMinTime + (bfHeliEngineMissFireMaxTime - bfHeliEngineMissFireMinTime) * fHeliEngineHealthFractionMissFireMult);
					float fRecoverTime = fwRandom::GetRandomNumberInRange(bfHeliEngineMissFireMaxRecoverTime + (bfHeliEngineMissFireMaxRecoverTime - bfHeliEngineMissFireMinRecoverTime) * fHeliEngineHealthFractionMissFireMult, bfHeliEngineMissFireMaxRecoverTime);
					m_pParent->m_Transmission.SetMissFireTime(fMissingFireTime, fRecoverTime);
				}
			}
		}	
	}
	else if(m_pParent->IsEngineOn())
	{
		//turn the engine off when completely out of petrol
		m_pParent->SwitchEngineOff();
	}

    //Process petrol tank fire
    if(IsPetrolTankBurning() || bForceExplodingPetrolTank)
    {
		//Calculate the petrol tank fire burn rate.
		float fPetrolTankFireBurnRate = bForceExplodingPetrolTank ? 0.0f : CalculatePetrolTankFireBurnRate();

        m_fPetrolTankHealth -= fPetrolTankFireBurnRate * fTimeStep * m_fPetrolTankDamageScale;

        bool bForceExplosionFromImpact = false;
        // if petrol tank is already on fire and car hit's something hard enough, explode immediately
        if(!m_pParent->GetDriver() || !m_pParent->GetDriver()->IsPlayer())
        {
            if(m_pParent->GetFrameCollisionHistory()->GetCollisionImpulseMagSum()*InvertSafe(m_pParent->pHandling->m_fMass, m_pParent->GetInvMass()) > VEHICLE_EXPLODE_WHEN_ON_FIRE_IMPACT)
                bForceExplosionFromImpact = true;
            else if(m_pParent->GetIsRotaryAircraft() && m_pParent->GetIsInWater())
                bForceExplosionFromImpact = true;
        }

        if(m_fPetrolTankHealth < VEH_DAMAGE_HEALTH_PTANK_FINISHED || bForceExplosionFromImpact || bForceExplodingPetrolTank)
        {
			u32 weaponHash = WEAPONTYPE_EXPLOSION;
			if(m_pParent->GetWeaponDamageHash())
				weaponHash = m_pParent->GetWeaponDamageHash();

            m_pParent->BlowUpCar(m_pEntityThatSetUsOnFire, false, true, false, weaponHash, false);

			m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_PTANK_FINISHED;

			// If we hit a non-mission vehicle, then deal some extra damage to that vehicle as well. (Explosion itself will also deal damage.)
			const CCollisionRecord * pColRecord = m_pParent->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
            if( pColRecord && pColRecord->m_pRegdCollisionEntity.Get() && pColRecord->m_pRegdCollisionEntity->GetIsTypeVehicle() )
            {
                CVehicle* pVehicleHit = static_cast<CVehicle*>( pColRecord->m_pRegdCollisionEntity.Get() );
                if( !pVehicleHit->IsNetworkClone()
                    && ( pVehicleHit->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicleHit->GetVehicleType() == VEHICLE_TYPE_BOAT )
                    && !pVehicleHit->ContainsPlayer()
                    && !pVehicleHit->PopTypeIsMission()
                    && pVehicleHit->GetStatus() != STATUS_WRECKED )
                {
					const float fExtraDamage = VEH_DAMAGE_HEALTH_STD * 0.35f; // 35% of full health... can be moved to a tuning value if desired
					float fNewPetrolTankHealth = pVehicleHit->GetVehicleDamage()->GetPetrolTankHealth() - ( fExtraDamage * m_fPetrolTankDamageScale );
                    pVehicleHit->GetVehicleDamage()->SetPetrolTankHealth(Max(fNewPetrolTankHealth, VEH_DAMAGE_HEALTH_PTANK_FINISHED));
                }
            }
        }
    }
}

bool CVehicleDamage::IsPetrolTankBurning() const
{
	if(m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_FINISHED && m_fPetrolTankHealth < VEH_DAMAGE_HEALTH_PTANK_ONFIRE && m_fPetrolTankLevel > 0.0f)
	{
		return true;
	}
	else
	{
		return false;
	}
}

float CVehicleDamage::CalculatePetrolTankFireBurnRate() const
{
	//Assert that the petrol tank is burning.
	vehicleAssertf(IsPetrolTankBurning(), "The petrol tank is not burning.");

	float fPetrolTankFireBurnRate = ms_fPetrolTankFireBurnRateMin+(((m_pParent->GetRandomSeed()&0xff)/255.0f)*(ms_fPetrolTankFireBurnRateMax-ms_fPetrolTankFireBurnRateMin));

	// force the burn rate for the player to be slow, to give you a chance to get out and run away
	if(m_pParent->GetDriver() && m_pParent->GetDriver()->IsPlayer())
		fPetrolTankFireBurnRate = ms_fPetrolTankFireBurnRateMin;

	return fPetrolTankFireBurnRate;
}

float CVehicleDamage::CalculateTimeUntilPetrolTankExplosion() const
{
	//Assert that the petrol tank is burning.
	vehicleAssertf(IsPetrolTankBurning(), "The petrol tank is not burning.");

	//Calculate the petrol tank fire burn rate (this value appears to be in seconds).
	float fPetrolTankFireBurnRate = CalculatePetrolTankFireBurnRate();

	//Calculate the remaining health until the explosion.
	float fRemainingHealthUntilExplosion = m_fPetrolTankHealth - VEH_DAMAGE_HEALTH_PTANK_FINISHED;

	return (fRemainingHealthUntilExplosion / fPetrolTankFireBurnRate);
}

void CVehicleDamage::PreRender()
{
	if (fwTimer::IsGamePaused())
	{
		return;
	}

	bool bIsSuperDummy = m_pParent->IsSuperDummy();
	if (!bIsSuperDummy)
	{
		g_vfxVehicle.ProcessEngineDamage(m_pParent);
	}

	for(int i=0; i<MAX_BOUNCING_PANELS; i++)
	{
		if(m_BouncingPanels[i].GetCompIndex()>-1)
			m_BouncingPanels[i].ProcessPanel(m_pParent, m_vecOldMoveSpeed, m_vecOldTurnSpeed);
	}

	m_vecOldMoveSpeed = m_pParent->GetVelocity();
	m_vecOldTurnSpeed = m_pParent->GetAngVelocity();
}

void CVehicleDamage::PreRender2()
{
	if( (true == m_pParent->m_nVehicleFlags.bHasSiren) && GetUpdateSirenBones() )
	{
		Matrix34 mat;
		mat.Zero();
		
		// We don't care about extras : they're done just up there.
		for(int i=VEH_SIREN_1; i<VEH_SIREN_MAX+1; i++)
		{
			if( GetSirenState(i) )
			{
				int boneIdx = m_pParent->GetBoneIndex((eHierarchyId)i);
				if( Verifyf(boneIdx!=-1, "Invalid bone index in vehicle siren %i (%s)", i, m_pParent->GetBaseModelInfo()->GetModelName() ))
				{
					m_pParent->SetGlobalMtx(boneIdx,mat);
				}
			}
		}
	}

	if( GetUpdateLightBones() )
	{
		Matrix34 mat;
		mat.Zero();
		
		for(int i=VEH_HEADLIGHT_L; i<VEH_LASTBREAKABLELIGHT+1; i++)
		{
			if( GetLightState(i) )
			{
				int boneIdx = m_pParent->GetBoneIndex((eHierarchyId)i);
				if(boneIdx == -1)
				{
					// Neons and extra are optional
					Assertf((i >= VEH_NEON_L && i <= VEH_NEON_B) || (i >= VEH_EXTRALIGHT_1 && i <= VEH_LASTBREAKABLELIGHT), "Invalid bone index in vehicle light loop %i (%s)", i, m_pParent->GetBaseModelInfo()->GetModelName() );
				}
				else
				{
					m_pParent->SetGlobalMtx(boneIdx,mat);
				}
			}
		}
	}
}

dev_float BIKE_KNOCKOVER_DAMAGE = 50.0f;
dev_float CAR_CRASH_SHOCKING_EVENT_COL_DELTAV = 15.0f;
dev_float CAR_CRASH_SHOCKING_EVENT_COL_IMPULSE = 50000.0f;
dev_float PLAYER_CRASH_MULTIPLIER = 3.0f;
dev_float AI_CRASH_MULTIPLIER = 1.5f;
dev_float OBJECT_CRASH_MULTIPLIER = 2.5f;
dev_float VEHICLE_CRASH_MULTIPLIER = 6.0f;

dev_float sfVehDamImpactThreshold = 15.0f;
float sfVehAircraftDamImpactThreshold = 1.5f;	//ORBIS doesn't like extern const
float sfVehBoatDamImpactThreshold = 1.5f;
dev_float sfObjectDamageSpeedThreshold = 5.0f;
dev_float sfHighSpeedObjectDamageThreshold = 0.5f;
dev_float sfLowSpeedObjectDamageThreshold = 3.0f;
dev_float sfVehDamImpactMult = 10.0f;
dev_float sfVehDamShakeThreshold = 0.1f;
dev_float sfVehTakeLessDamageMult = 0.5f;
dev_float sfAircraftFireDamageMult = 0.05f;
dev_float sfCarCollisionDamageMultiplayerMult = 0.5f;
dev_float sfCarCollisionDamagePlayerMult = 0.5f;
dev_float sfPlayerDamageMultiplayerMult = 0.5f;
dev_float sfVehDamSetWantedLevelThreshold = 3.0f;
dev_float sfMinInflictorVelocitySetWantedLevel = 1.0f;
dev_float sfAbandonedAircraftLandingDamageMult = 2.0f;
dev_float sfAmbientCarLandingDamageMulti = 2.0f;
dev_float sfVehicleRoofDamageMult = 1.5f;
dev_float sfHydraulicsBounceLandingDamageMulti = 10.0f;
dev_float sfHydraulicsBounceLandingDamageThreshold = 100.0f;
dev_float sfHydraulicsBounceRaisingDamageMulti = 5.0f;
dev_float sfSubmarineLandDamageMult = 2.0f;
dev_float sfSubmarineWaterDamageMult = 0.3f;
dev_float sfPlaneExplosionDamageCapInSP = 350.0f; //Make it able to survive from 2 rocket explosions
dev_float sfPlaneCollisionWithCarMult = 0.1f;
dev_float sfPlaneDamagedByMeleeMult = 0.5f;

// Cloud tunables
bool CVehicleDamage::sbPlaneDamageCapLargePlanesOnly = true;
float CVehicleDamage::sfPlaneExplosionDamageCapInMP = 350.0f;
float CVehicleDamage::sfSpecialAmmoFMJDamageMultiplierAgainstVehicles = 2.0f;
float CVehicleDamage::sfSpecialAmmoFMJDamageMultiplierAgainstVehicleGasTank = 0.5f;

void CVehicleDamage::InitTunables()
{
	sbPlaneDamageCapLargePlanesOnly = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("MP_EXPLOSION_DAMAGE_CAP_LARGE_PLANES_ONLY", 0x1f0d74d9), sbPlaneDamageCapLargePlanesOnly);
	sfPlaneExplosionDamageCapInMP = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PLANE_MP_EXPLOSION_DAMAGE_CAP", 0x8234442b), sfPlaneExplosionDamageCapInMP);

	sfSpecialAmmoFMJDamageMultiplierAgainstVehicles		  = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_VEH_DAMAGEMULT", 0xA349D60A), sfSpecialAmmoFMJDamageMultiplierAgainstVehicles);
	sfSpecialAmmoFMJDamageMultiplierAgainstVehicleGasTank = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_VEH_GASTANKDAMAGEMULT", 0x4C0A13A1), sfSpecialAmmoFMJDamageMultiplierAgainstVehicleGasTank);

	sm_Tunables.m_MinImpulseForEnduranceDamage = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_VEHICLE_DMG_MIN_THRESHOLD", 0xC4F8A8AD), sm_Tunables.m_MinImpulseForEnduranceDamage);
	sm_Tunables.m_MaxImpulseForEnduranceDamage = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_VEHICLE_DMG_MAX_THRESHOLD", 0xDF1E475D), sm_Tunables.m_MaxImpulseForEnduranceDamage);
	sm_Tunables.m_MinEnduranceDamage = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_VEHICLE_DMG_MIN_APPLIED", 0xB20B2506), sm_Tunables.m_MinEnduranceDamage);
	sm_Tunables.m_MaxEnduranceDamage = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_VEHICLE_DMG_MAX_APPLIED", 0xFF2B790F), sm_Tunables.m_MaxEnduranceDamage);
	sm_Tunables.m_InstigatorDamageMultiplier = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_DMG_MULTIPLIER_INSTIGATOR", 0x345B5A04), sm_Tunables.m_InstigatorDamageMultiplier);
}

bool CVehicleDamage::HasBoneCollidedWithComponent(eHierarchyId boneId, int iComponent)
{
	if( m_pParent && m_pParent->GetVehicleFragInst() )
	{
		int iBoneIndex = m_pParent->GetBoneIndex(boneId);

		if( iBoneIndex > -1 && iComponent == m_pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndex) )
		{
			return true;
		}
	}

	return false;
}

u32 CVehicleDamage::sm_TimeOfLastShotAtHeliMegaphone = 0;

float CVehicleDamage::ApplyDamage(CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPos, const Vector3& vecNorm, const Vector3& vecDirn, int nComponent, phMaterialMgr::Id nMaterialId, int nPieceIndex, const bool bFireDriveby, const bool bIsAccurate, const float fDamageRadius, const bool bAvoidExplosions, const bool bChainExplosion, const bool bMeleeDamage, const bool isFlameThrowerFire, const bool forceMinDamage )
{
	ENABLE_FRAG_OPTIMIZATION_ONLY(m_pParent->GiveFragCacheEntry(true);)

	float fApplyDamage = fDamage;
	NOTFINAL_ONLY(vehicleDebugf3("CVehicleDamage::ApplyDamage - '%s' '%s'- Damage = %f type: %d, Pos: %.2f %.2f %.2f",m_pParent->GetDebugName(), m_pParent->GetNetworkObject() ? m_pParent->GetNetworkObject()->GetLogName() : "none", fDamage, (int)nDamageType, vecPos.x, vecPos.y, vecPos.z));
	float armourMultiplier = m_pParent->GetVariationInstance().GetArmourDamageMultiplier();
	fApplyDamage *= armourMultiplier;
	fApplyDamage *= m_fScriptDamageScale;

	bool bLandingDamage = false;
	bool bNoEngineDamage = false;

	// script flag to reduce damage to all vehicles
	if(m_pParent->m_nVehicleFlags.bTakeLessDamage)
	{
		fApplyDamage *= sfVehTakeLessDamageMult;
		vehicleDebugf3("\tm_nVehicleFlags.bTakeLessDamage - Damage = %f",fApplyDamage);
	}

	if(NetworkInterface::IsGameInProgress() && pInflictor && pInflictor->GetIsTypeVehicle())
	{
		const CVehicle* pInflictorVehicle = static_cast<const CVehicle*>(pInflictor);
		if(pInflictorVehicle->InheritsFromHeli() && pInflictorVehicle->GetDriver() && pInflictorVehicle->GetDriver()->IsPlayer())
		{
			static const u32 BUZZARD_WEAPON_HASH = ATSTRINGHASH("VEHICLE_WEAPON_PLAYER_BUZZARD", 0x46b89c8e);
			if (nWeaponHash == BUZZARD_WEAPON_HASH)
			{
				// Applying modifier to Annihilator and Buzzard machine gun weapons
				TUNE_GROUP_FLOAT(DAMAGE_TUNE, BUZZARD_MP_DAMAGE_MODIFIER, 5.0f, 0.0f, 1000.0f, 0.01f);
				fApplyDamage *= BUZZARD_MP_DAMAGE_MODIFIER;
				vehicleDebugf3("\tBUZZARD_MG_MP_DAMAGE_MODIFIER - Damage = %f",fApplyDamage);
			}
		}
	}


#if __BANK
	vehicleDebugf3( "CVehicleDamage::ApplyDamage: inflictor: %s, Inflictor Pos: %.2f, %.2f, %.2f, Inflictor prev pos: %.2f, %.2f, %.2f", 
					pInflictor ? pInflictor->GetIsPhysical() ? static_cast< CPhysical* >( pInflictor )->GetNetworkObject() ? static_cast< CPhysical* >( pInflictor )->GetNetworkObject()->GetLogName() : pInflictor->GetDebugName() : "none" : "None",
					pInflictor ? pInflictor->GetTransform().GetPosition().GetXf() : 0.0f,
					pInflictor ? pInflictor->GetTransform().GetPosition().GetYf() : 0.0f,
					pInflictor ? pInflictor->GetTransform().GetPosition().GetZf() : 0.0f,
					pInflictor ? pInflictor->GetCurrentPhysicsInst() ? PHLEVEL->GetLastInstanceMatrix( pInflictor->GetCurrentPhysicsInst() ).d().GetXf() : 0.0f : 0.0f, 
					pInflictor ? pInflictor->GetCurrentPhysicsInst() ? PHLEVEL->GetLastInstanceMatrix( pInflictor->GetCurrentPhysicsInst() ).d().GetYf() : 0.0f : 0.0f,
					pInflictor ? pInflictor->GetCurrentPhysicsInst() ? PHLEVEL->GetLastInstanceMatrix( pInflictor->GetCurrentPhysicsInst() ).d().GetZf() : 0.0f : 0.0f );

	vehicleDebugf3( "CVehicleDamage::ApplyDamage: victim: %s, victim Pos: %.2f, %.2f, %.2f, victim prev pos: %.2f, %.2f, %.2f", 
		m_pParent->GetNetworkObject() ? m_pParent->GetNetworkObject()->GetLogName() : m_pParent->GetDebugName(),
		m_pParent->GetTransform().GetPosition().GetXf(),
		m_pParent->GetTransform().GetPosition().GetYf(),
		m_pParent->GetTransform().GetPosition().GetZf(),
		m_pParent->GetCurrentPhysicsInst() ? PHLEVEL->GetLastInstanceMatrix( m_pParent->GetCurrentPhysicsInst() ).d().GetXf() : 0.0f, 
		m_pParent->GetCurrentPhysicsInst() ? PHLEVEL->GetLastInstanceMatrix( m_pParent->GetCurrentPhysicsInst() ).d().GetYf() : 0.0f,
		m_pParent->GetCurrentPhysicsInst() ? PHLEVEL->GetLastInstanceMatrix( m_pParent->GetCurrentPhysicsInst() ).d().GetZf() : 0.0f );
#endif // 

	
	bool bIncludeBodyDamage = nWeaponHash == ATSTRINGHASH("VEHICLE_WEAPON_WATER_CANNON",0x67D18297) || nWeaponHash == ATSTRINGHASH("WEAPON_HIT_BY_WATER_CANNON",0xCC34325E);

	//Cache if the vehicle is already destroyed and the health.
	const bool isAlreadyWrecked = (m_pParent->GetStatus() == STATUS_WRECKED);
	float previousHealth = m_pParent->GetHealth() + GetEngineHealth() + GetPetrolTankHealth();
	if (bIncludeBodyDamage)
	{
		previousHealth += m_fBodyHealth;
	}

	for(s32 i=0; i<m_pParent->GetNumWheels(); i++)
	{
		previousHealth += GetTyreHealth(i);
		previousHealth += GetSuspensionHealth(i);
	}

	// if this is physical damage (from crashing into something)
	if(nDamageType==DAMAGE_TYPE_COLLISION)
	{
		if( pInflictor==NULL || 
			pInflictor->GetIsTypeBuilding() ||
			pInflictor->GetIsTypeAnimatedBuilding() ||
			pInflictor->GetIsTypeObject() )
		{
			fApplyDamage *= m_fCollisionWithMapDamageScale;
		}

		bool forceMinimumDamage = forceMinDamage;
		if(pInflictor==NULL || pInflictor->GetIsTypeBuilding())
		{
			const float kfMinSpeedForSubDamage2 = 0.5f;
			if(m_pParent->InheritsFromSubmarine() && m_pParent->GetVelocity().Mag2() > kfMinSpeedForSubDamage2)
			{
				if(m_pParent->GetIsInWater())
				{
					fApplyDamage *= sfSubmarineWaterDamageMult;
					vehicleDebugf3("\tSubmarine Water Collision Scaling - Damage = %f",fApplyDamage);
				}
				else
				{
					fApplyDamage *= sfSubmarineLandDamageMult;
					vehicleDebugf3("\tSubmarine Land Collision Scaling - Damage = %f",fApplyDamage);
				}
			}
			else
			{
				float normalDotUp = DotProduct(vecNorm, VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetC()));
				if(normalDotUp > 0.6f)
				{
					// Collisions from the road don't do any damage.
					if(m_pParent->GetIsAircraft())
					{
						if(m_pParent->GetStatus() == STATUS_ABANDONED)
						{
							fApplyDamage *= sfAbandonedAircraftLandingDamageMult;
							vehicleDebugf3("\tAbandoned Scaling - Damage = %f",fApplyDamage);
						}

						if( m_pParent->InheritsFromPlane() )
						{
							CPlane* pPlane = static_cast<CPlane*>( m_pParent );
							if( pPlane->GetLandingGearDamage().IsComponentBreakable( pPlane, nComponent ) )
							{
								bLandingDamage = true;
							}
						}

						if(CSeaPlaneHandlingData* pSeaPlaneHandling = m_pParent->pHandling->GetSeaPlaneHandlingData())
						{
							if((u32)nComponent == pSeaPlaneHandling->m_fLeftPontoonComponentId
								|| (u32)nComponent == pSeaPlaneHandling->m_fRightPontoonComponentId
								|| bLandingDamage)
							{
								bNoEngineDamage = true;
							}
						}
					}
					else if(m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR && m_pParent->PopTypeIsRandom() && !m_pParent->ContainsLocalPlayer() && (m_pParent->GetDriver() == NULL || m_pParent->GetDriver()->PopTypeIsRandom()))
					{
						// Apply landing damage for empty or ambient AI cars
						fApplyDamage *= sfAmbientCarLandingDamageMulti;
					}
					else if(m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR &&
						    fApplyDamage > sfHydraulicsBounceLandingDamageThreshold )
					{
						CAutomobile* pAutomobile = static_cast< CAutomobile* >( m_pParent );

						// Apply damage for cars using hydraulics
						if( pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising )
						{
							fApplyDamage *= sfHydraulicsBounceRaisingDamageMulti;
							forceMinimumDamage = true;
						}
						else if( pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding ) 
						{
							fApplyDamage *= sfHydraulicsBounceLandingDamageMulti;
							forceMinimumDamage = true;
						}
						else
						{
							return 0.0f;
						}
					}
					else if( !forceMinDamage )
					{
						vehicleDebugf3("\tRoad Collision - 0 Damage");
						return 0.0f;
					}
				}
				else if(normalDotUp < -0.6f)
				{
					fApplyDamage *= sfVehicleRoofDamageMult;
					vehicleDebugf3("\tRoof Collision - %f Damage",fApplyDamage);
				}
			}
		}

		// Exclude turret collisions from vehicle damage
		if( pInflictor )
		{
			// If hit a part of a vehicle turret, don't apply damage because turrets spin like crazy
			if(m_pParent && m_pParent->GetVehicleWeaponMgr() && m_pParent->GetVehicleFragInst() )
			{
				CVehicleWeaponMgr* pWeaponMgr = m_pParent->GetVehicleWeaponMgr();

				// Loop through the turrets on the weapon and check if we hit their barrels or their bases
				for(int i = 0; i < pWeaponMgr->GetNumTurrets(); i++)
				{
					CTurret* pTurret = pWeaponMgr->GetTurret(i);

					if(pTurret->IsPhysicalTurret())
					{
						if( HasBoneCollidedWithComponent( pTurret->GetBarrelBoneId(), nComponent ) ||
							HasBoneCollidedWithComponent( pTurret->GetBaseBoneId(), nComponent ) )
						{
							return 0.0f;
						}
					}
				}

				// Loop through the fixed weapons on the vehicle and check if we've hit the weapon
				for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
				{
					CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);

					if( pWeapon->GetType() == VGT_FIXED_VEHICLE_WEAPON )
					{
						CFixedVehicleWeapon* pFixedWeapon = static_cast<CFixedVehicleWeapon*>(pWeapon);

						if( HasBoneCollidedWithComponent( pFixedWeapon->GetWeaponBone(), nComponent ) )
						{
							return 0.0f;
						}
					}
				}
			}
		}

		// Use a lower damage threshold for vehicles vs. aircrafts so they can inflict small amounts of damage
		float fVehDamImpactThreshold = sfVehDamImpactThreshold;
		bool bInflictorIsAVehicle = pInflictor && pInflictor->GetIsTypeVehicle();
		if( bInflictorIsAVehicle && m_pParent->GetIsAircraft())
		{
			fVehDamImpactThreshold = sfVehAircraftDamImpactThreshold;
		}
		else if( bInflictorIsAVehicle && m_pParent->GetIsAquatic())
		{
			fVehDamImpactThreshold = sfVehBoatDamImpactThreshold;
		}

		if( m_bDisableDamageWithPickedUpEntity &&
			pInflictor &&
			m_pParent->InheritsFromHeli() &&
			static_cast< CHeli* >( m_pParent )->GetIsCargobob() )
		{
			CHeli* pCargobob = static_cast< CHeli* >( m_pParent );

			for( int i = 0; i < pCargobob->GetNumberOfVehicleGadgets(); i++ )
			{
				CVehicleGadget *pVehicleGadget = pCargobob->GetVehicleGadget(i);

				if( pVehicleGadget && ( pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET ) )
				{
					CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);
					
					if( pPickUpRope->GetAttachedEntity() == pInflictor )
					{
						return 0.0f;
					}
				}
			}
		}

		// if this is a titan plane reduce the amount of damage done to it in collisions with cars
		if( m_pParent->InheritsFromPlane() && 
			( m_pParent->GetModelIndex() == MI_PLANE_TITAN || m_pParent->GetModelIndex() == MI_PLANE_BOMBUSHKA || m_pParent->GetModelIndex() == MI_PLANE_VOLATOL) && 
			bInflictorIsAVehicle )
		{
			CVehicle* pInflictorVehicle = static_cast<CVehicle*>(pInflictor);

			if( pInflictorVehicle->GetVehicleType() == VEHICLE_TYPE_CAR )
			{
				fApplyDamage *= sfPlaneCollisionWithCarMult;
			}
		}

		// physical damage gets scaled by the collision damage multiplier, and by mass
		fApplyDamage *= sfVehDamImpactMult * m_pParent->pHandling->m_fCollisionDamageMult * InvertSafe(m_pParent->pHandling->m_fMass, m_pParent->GetInvMass());
		vehicleDebugf3("\tMass Scaling - Damage = %f",fApplyDamage);

		// SHAKE PAD
		PF_SET(DamageImpulse, fApplyDamage);
		PF_SET(DamageVelocity, m_pParent->GetVelocity().Mag() );

		float fRumbleDamage = fApplyDamage;

		if(pInflictor)
		{
			if(pInflictor->GetIsTypeObject())
			{
				if(fragInst* pFragInst = pInflictor->GetFragInst())
				{
					if(pFragInst->GetCacheEntry())
					{
						if(pFragInst->GetCacheEntry()->GetHierInst()->age < 0.25f)
						{
							// Recently broken fragments should do some damage
							forceMinimumDamage = true;
						}
					}

					CBaseModelInfo* pModelInfo = pInflictor->GetBaseModelInfo();
					if (pModelInfo && pModelInfo->TestAttribute(MODEL_ATTRIBUTE_RUMBLE_ON_LIGHT_COLLISION_WITH_VEHICLE))// for the highway paddles we want to have a bit of rumble
					{
						fRumbleDamage = sfRumbleDamageWhenHittingRumbleOnLightColWithObject + fApplyDamage;
					}
				}
			}
		}


		if(fApplyDamage > sfVehDamShakeThreshold && m_pParent->GetVelocity().Mag2() >= 0.001f)
			ApplyPadShake(pInflictor, fRumbleDamage);

		if(m_pParent->ContainsLocalPlayer())
			g_ScriptAudioEntity.RegisterPlayerCarCollision(m_pParent, pInflictor, fApplyDamage);

		if(forceMinimumDamage)
		{
			fApplyDamage = Max(fVehDamImpactThreshold + 1.0f, fApplyDamage);
			vehicleDebugf3("\tForce Minimum Damage - Damage = %f",fApplyDamage);
		}

		vehicleDebugf3("\tFinal Damage = %f, Threshold = %f",fApplyDamage,fVehDamImpactThreshold);
		if(fApplyDamage < fVehDamImpactThreshold)
		{
			if(pInflictor && pInflictor->GetIsTypeObject())
			{
				const bool bHitLargeFixedObject = !pInflictor->IsBaseFlagSet(fwEntity::IS_FIXED) && pInflictor->GetBoundRadius() > 0.3f;
				bool bAddMadDriverEvent = false;
				//If there wasn't much damage it could have been with an object which we will still want peds to react to.
				if (bHitLargeFixedObject)
				{
					// Different thresholds for different speeds.
					float fDamageThreshold = m_pParent->GetAiXYSpeed() > sfObjectDamageSpeedThreshold ? sfHighSpeedObjectDamageThreshold : sfLowSpeedObjectDamageThreshold;
					if (fApplyDamage > fDamageThreshold)
					{
						if (m_pParent->InheritsFromBicycle())
						{
							CEventShockingBicycleCrash ev(*m_pParent, pInflictor);
							CShockingEventsManager::Add(ev);
						}
						else
						{
							CEventShockingCarCrash ev(*m_pParent, pInflictor);
							CShockingEventsManager::Add(ev);
						}

						if(m_pParent->GetDriver() && m_pParent->GetDriver()->IsPlayer())
						{
							REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(4000, 4000, IMPORTANCE_NORMAL, CODE, OVERLAP_VEHICLE_DAMAGE);)
						}
					}
					else
					{
						bAddMadDriverEvent = true;
					}
				}
				else
				{
					bAddMadDriverEvent = true;
				}

				// Enough damage might not have been done for a full car crash event, but we still probably want peds to react somehow.
				// MadDriver is a much more light response.
				CPed* pDriver = m_pParent->GetDriver();
				if (bAddMadDriverEvent && pDriver)
				{
					if (m_pParent->InheritsFromBicycle())
					{
						CEventShockingMadDriverBicycle ev(*pDriver);
						CShockingEventsManager::Add(ev);
					}
					else
					{
						CEventShockingMadDriverExtreme ev(*pDriver);
						CShockingEventsManager::Add(ev);
					}

					if(pDriver->IsPlayer())
					{
						REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(2500, 2500, IMPORTANCE_NORMAL, CODE, OVERLAP_VEHICLE_DAMAGE);)
					}
				}

			}

			if(fApplyDamage > sfVehDamSetWantedLevelThreshold && pInflictor && m_pParent->HasAliveLawPedsInIt())
			{
				CPed* pInflictorPed = NULL;
				if(pInflictor->GetIsTypePed())
				{
					pInflictorPed = static_cast<CPed*>(pInflictor);
				}
				else if(pInflictor->GetIsTypeVehicle())
				{
					pInflictorPed = static_cast<CVehicle*>(pInflictor)->GetDriver();
				}

				if( pInflictorPed && pInflictorPed->IsLocalPlayer() && pInflictorPed->GetVehiclePedInside() )
				{
					float fVehicleVelocitySq = pInflictorPed->GetVehiclePedInside()->GetVelocity().Mag2();
					if(fVehicleVelocitySq > square(sfMinInflictorVelocitySetWantedLevel) && fVehicleVelocitySq > m_pParent->GetVelocity().Mag2() )
					{
						CWanted* pPlayerWanted = pInflictorPed->GetPlayerWanted();
						if(pPlayerWanted &&	pPlayerWanted->GetWantedLevel() == WANTED_CLEAN && pPlayerWanted->m_fMultiplier )
						{
							pPlayerWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pInflictorPed->GetTransform().GetPosition()), WANTED_LEVEL1, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_LAW_RESPONSE_EVENT);
						}
					}
				}
			}

			// Still call this function here even if there's no damage, as this is also used to detect vehicle crashes
			CStatsMgr::RegisterVehicleDamage(GetParent(), pInflictor, nDamageType, 0.0f, false);

			return 0.0f;
		}

		fApplyDamage -= fVehDamImpactThreshold;
		vehicleDebugf3("\tThreshold Reduction - Damage = %f",fApplyDamage);

		if(m_pParent->GetIsAircraft() && nDamageType == DAMAGE_TYPE_COLLISION)
		{
			// By pass the damage scales, aircrafts are vulnerable to collision impacts
		}
		else if(NetworkInterface::IsGameInProgress())
		{
			//Reduce all car collision damage in MP and other vehicle collision damage for the player only
			if(m_pParent->InheritsFromAutomobile())
			{
				fApplyDamage *= sfCarCollisionDamageMultiplayerMult;
				vehicleDebugf3("\tMP  Scaling - Damage = %f",fApplyDamage);
			}

			if(m_pParent->ContainsPlayer())
			{
				fApplyDamage *= sfPlayerDamageMultiplayerMult;
				vehicleDebugf3("\tMP Player Scaling - Damage = %f",fApplyDamage);
			}
		}
		else
		{
			if(m_pParent->InheritsFromAutomobile() && m_pParent->ContainsPlayer())
			{
				fApplyDamage *= sfCarCollisionDamagePlayerMult;
				vehicleDebugf3("\tSP Player Scaling - Damage = %f",fApplyDamage);
			}
		}

		float fMult = (m_pParent->GetDriver() && m_pParent->GetDriver()->IsAPlayerPed()) ? PLAYER_CRASH_MULTIPLIER : AI_CRASH_MULTIPLIER;

		if (pInflictor)
		{
			if (pInflictor->GetIsTypeObject() && !pInflictor->IsBaseFlagSet(fwEntity::IS_FIXED))
			{
				fMult *= OBJECT_CRASH_MULTIPLIER;
			}
			else if (pInflictor->GetIsTypeVehicle())
			{
				fMult *= VEHICLE_CRASH_MULTIPLIER;
			}
		}

		if((fMult*fDamage*InvertSafe(m_pParent->pHandling->m_fMass, m_pParent->GetInvMass())) > CAR_CRASH_SHOCKING_EVENT_COL_DELTAV  || (fMult*fDamage) > CAR_CRASH_SHOCKING_EVENT_COL_IMPULSE)
		{
			// Note: we used to pass in nWeaponHash to the shocking event, but it was actually
			// ignored, was only used for the EVisibleWeapon event.
			if (m_pParent->InheritsFromBicycle())
			{
				CEventShockingBicycleCrash ev(*m_pParent, pInflictor);
				CShockingEventsManager::Add(ev);
			}
			else
			{
				CEventShockingCarCrash ev(*m_pParent, pInflictor);
				CShockingEventsManager::Add(ev);
			}
		}

		//apply the unadjusted damage value so we can compute the velocity of the impact
		camInterface::GetGameplayDirector().RegisterVehicleDamage(m_pParent, fDamage);
		camInterface::GetCinematicDirector().RegisterVehicleDamage(m_pParent, -vecNorm, fDamage); 

		// do any visual effect due to collision
		m_pParent->DoCollisionEffects();
	}
	else
	{
		// For shots we need to report the crime
		bool bHeliOrPlaneExplosion = nDamageType == DAMAGE_TYPE_EXPLOSIVE && (m_pParent->InheritsFromHeli() || m_pParent->InheritsFromPlane());
		bool bShotVehicle = (nDamageType == DAMAGE_TYPE_BULLET || nDamageType == DAMAGE_TYPE_BULLET_RUBBER);
		bool bShotPoliceVehicleWithWaterCannon = nDamageType == DAMAGE_TYPE_WATER_CANNON && m_pParent->HasAliveLawPedsInIt();
		if( pInflictor && 
			(bHeliOrPlaneExplosion || bShotVehicle || bShotPoliceVehicleWithWaterCannon) &&
			m_pParent->HasAlivePedsInIt() )
		{
			CPed* pInflictorPed = NULL;
			if(pInflictor->GetIsTypePed())
			{
				pInflictorPed = static_cast<CPed*>(pInflictor);
			}
			else if(pInflictor->GetIsTypeVehicle())
			{
				pInflictorPed = static_cast<CVehicle*>(pInflictor)->GetDriver();
			}

			if(pInflictorPed && pInflictorPed->GetVehiclePedInside() != m_pParent)
			{
				if(bShotVehicle || bShotPoliceVehicleWithWaterCannon)
				{
					CCrime::ReportCrime(CRIME_SHOOT_VEHICLE, m_pParent, pInflictorPed);

					u32 audioTime = g_AudioEngine.GetTimeInMilliseconds();
					if(m_pParent->InheritsFromHeli() && pInflictorPed->IsLocalPlayer() && 
						m_pParent->IsLawEnforcementVehicle() && m_pParent->GetSeatManager() && g_AudioScannerManager.ShouldTryToPlayShotAtHeli(audioTime))
					{
						s32 maxSeats = m_pParent->GetSeatManager()->GetMaxSeats();
						if(maxSeats > 0)
						{
							int startSeat = audEngineUtil::GetRandomInteger() % maxSeats;
							for( s32 iSeat = 0; iSeat < maxSeats; iSeat++ )
							{
								int index = (startSeat + iSeat) % maxSeats;
								CPed* pPed = m_pParent->GetSeatManager()->GetPedInSeat(index);
								if( pPed && pPed->GetSpeechAudioEntity())
								{
									if( pPed->GetSpeechAudioEntity()->SetPedVoiceForContext(ATPARTIALSTRINGHASH("SHOT_AT_HELI_MEGAPHONE", 0x31DD519A)) )
									{
										pPed->GetSpeechAudioEntity()->SayWhenSafe("SHOT_AT_HELI_MEGAPHONE");
										break;
									}
								}
							}
						}

						sm_TimeOfLastShotAtHeliMegaphone = audioTime;
					}
				}

				if(bHeliOrPlaneExplosion && pInflictorPed->IsLocalPlayer())
				{
					CVehicle *pVehicle = static_cast<CVehicle*>(m_pParent);
					if (pVehicle && pVehicle->m_nVehicleFlags.bInfluenceWantedLevel)
					{
						CWanted* pPlayerWanted = pInflictorPed->GetPlayerWanted();
						if(pPlayerWanted &&	pPlayerWanted->MaximumWantedLevel > WANTED_LEVEL2 && pPlayerWanted->m_fMultiplier)
						{
							pPlayerWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pInflictorPed->GetTransform().GetPosition()), WANTED_LEVEL3, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_LAW_RESPONSE_EVENT);
						}
					}
				}
			}
		}

		//B*1721702/1732210/1819577: Scale explosive damage on tanks (online only). 4 rockets or 3 sticky bombs to destroy.
		bool bUseWeaponDamageMult = true;
		bool bUseMultiplayerMult = m_pParent->GetUseMPDamageMultiplierForPlayerVehicle();
		if (nDamageType == DAMAGE_TYPE_EXPLOSIVE && m_pParent->IsTank() && NetworkInterface::IsGameInProgress())
		{
			bUseMultiplayerMult = false;

			bool bInflicitorIsTank = false;
			if (pInflictor && pInflictor->GetIsTypeVehicle())
			{
				CVehicle *pVehicle = static_cast<CVehicle*>(pInflictor);
				if (pVehicle && pVehicle->IsTank())
				{
					bInflicitorIsTank = true;
				}
			}

			// Don't scale damage if inflictor is a tank (m_pParent->pHandling->m_fWeaponDamageMult already scales it down by ~0.03 to 270 damage).
			if (!bInflicitorIsTank)
			{
				//Don't use weapon damage multiplier (0.03 in handling.dat) and don't use multiplayer multiplier (halves damage done)
				bUseWeaponDamageMult = false;

				float fExplosiveTankDamageMult = 0.175f;
				static const u32 STICKYBOMB_WEAPON_HASH = ATSTRINGHASH("WEAPON_STICKYBOMB", 0x2c3731d9);
				if (nWeaponHash == STICKYBOMB_WEAPON_HASH)
				{
					fExplosiveTankDamageMult = 0.225f;

				}
				fApplyDamage *= fExplosiveTankDamageMult;
			}
		}

		// B*1909265: Anti-vehicle sniper rifle: Do 50% of vehicles health per hit (or 33.3% for tanks).
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
		if (pWeaponInfo && pWeaponInfo->GetScalesDamageBasedOnMaxVehicleHealth())
		{
			// Don't do damage multipliers
			bUseWeaponDamageMult = false;
			bUseMultiplayerMult = false;
			
			// Scale damage based on health and multiplier set in handling data (defaults to 0.5, tanks 0.34).
			fApplyDamage = m_pParent->GetMaxHealth() * m_pParent->pHandling->m_fWeaponDamageScaledToVehHealthMult;
		}

		// Add multipler for weapon damage against vehicles (B*2109304)
		fApplyDamage *= pWeaponInfo ? pWeaponInfo->GetVehicleDamageModifier() : 1.0f;

		// Add multipler for Full Metal Jacket ammunition
		if(pInflictor && pInflictor->GetIsTypePed())
		{
			const CPed* pInflictorPed = static_cast<const CPed*>(pInflictor);
			if (pInflictorPed && pWeaponInfo)
			{
				const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(pInflictorPed);
				if (pAmmoInfo && pAmmoInfo->GetIsFMJ())
				{
					fApplyDamage *= sfSpecialAmmoFMJDamageMultiplierAgainstVehicles;
				}
			}
		}
		
		// all other damage gets scaled by the weapon damage multiplier
		if (bUseWeaponDamageMult)
		{
			fApplyDamage *= m_pParent->pHandling->m_fWeaponDamageMult * m_fScriptWeaponDamageScale;
		}

		// Scale down the fire damage for Helis/Planes, so they can last longer when in contact with fire
		if(nDamageType == DAMAGE_TYPE_FIRE && (m_pParent->InheritsFromHeli() || m_pParent->InheritsFromPlane()))
		{
			fApplyDamage *= sfAircraftFireDamageMult;
		}

		// Cap explosive damage to planes in certain situations, so they take more than one hit to kill
		if( nDamageType == DAMAGE_TYPE_EXPLOSIVE && 
			( m_pParent->InheritsFromPlane() || 
			  ( m_pParent->InheritsFromHeli() && 
			    ( m_pParent->m_nVehicleFlags.bPlaneResistToExplosion ||
				  ( static_cast<CHeli*>(m_pParent) && static_cast<CHeli*>(m_pParent)->GetIsCargobob() ) ) ) ) )
		{
			if (!NetworkInterface::IsGameInProgress()) 
			{
				// Singleplayer
				fApplyDamage = Min(fApplyDamage, sfPlaneExplosionDamageCapInSP);
			}
			else if(m_pParent->m_nVehicleFlags.bPlaneResistToExplosion)
			{
				// Multiplayer
				fApplyDamage = Min(fApplyDamage, sfPlaneExplosionDamageCapInMP);
				bUseWeaponDamageMult = false;
				bUseMultiplayerMult = false;
			}
			else if (pWeaponInfo && pWeaponInfo->GetUsePlaneExplosionDamageCapInMP() &&
				NetworkInterface::IsSessionEstablished() && !NetworkInterface::IsActivitySession() && !NetworkInterface::IsTransitionActive())
			{
				// Multiplayer (Freemode only)
				CPlane* pPlane = static_cast<CPlane*>(m_pParent);
				if (pPlane && ((pPlane->IsLargePlane() || pPlane->GetSeatManager()->GetMaxSeats() > 6 ) || !sbPlaneDamageCapLargePlanesOnly) )
				{
					// Large planes (Cargoplane, Jet, Titan) and high capacity planes (Luxor, Miljet, Shamal)
					fApplyDamage = Min(fApplyDamage, sfPlaneExplosionDamageCapInMP);
					bUseWeaponDamageMult = false;
					bUseMultiplayerMult = false;
				}
			}
		}

		// Also cap explosive damage to flagged armored cars
		if (nDamageType == DAMAGE_TYPE_EXPLOSIVE && m_pParent->GetVehicleModelInfo() && m_pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_CAPPED_EXPLOSION_DAMAGE))
		{
			TUNE_GROUP_FLOAT(VEHICLE_DAMAGE, EXPLOSIVE_DAMAGE_CAP_FOR_FLAGGED_VEHICLES, 550.0f, 0.0f, 2000.0f, 1.0f);
			fApplyDamage = Min(fApplyDamage, EXPLOSIVE_DAMAGE_CAP_FOR_FLAGGED_VEHICLES);
			bUseWeaponDamageMult = false;
			bUseMultiplayerMult = false;
		}

		if(m_pParent->ContainsPlayer() && NetworkInterface::IsGameInProgress() && bUseMultiplayerMult)
		{
			if(!m_pParent->InheritsFromPlane() || nDamageType != DAMAGE_TYPE_EXPLOSIVE)
			{
				fApplyDamage *= sfPlayerDamageMultiplayerMult;
			}
		}

		// GTAV DLC - B*1765844 - reduce melee damage against planes by 50%
		if( m_pParent->InheritsFromPlane() &&
			nDamageType == DAMAGE_TYPE_MELEE )
		{
			fApplyDamage *= sfPlaneDamagedByMeleeMult;
		}

		// Inform the audio if a rocket collided with the vehicle
		if (pInflictor && pInflictor->GetIsTypeObject() && ((CObject*)pInflictor)->GetAsProjectileRocket())
		{
			if(m_pParent->GetVehicleAudioEntity())
			{
				m_pParent->GetVehicleAudioEntity()->SetHitByRocket();
			}
		}

		// B*988465- Add vibrations for weapon damage to the vehicle
		if(fApplyDamage > sfVehDamShakeThreshold && nDamageType != DAMAGE_TYPE_FIRE)
			ApplyPadShake(pInflictor, fApplyDamage);

		//if( fApplyDamage > CAR_CRASH_EVENT_DAMAGE_THRESHOLD )
		//	CShockingEventsManager::Add(ECarCrash,m_pParent->GetPosition(),m_pParent,pInflictor);
		if(m_pParent->GetIsRotaryAircraft())
		{
			CRotaryWingAircraft * pRotaryWingAircraft = static_cast<CRotaryWingAircraft*>(m_pParent);

			if( pRotaryWingAircraft->ApplyDamageToPropellers( pInflictor, fApplyDamage, nComponent ) )
			{
				if (!m_pParent->IsNetworkClone())
				{
					UpdateDamageTrackers(pInflictor, nWeaponHash, nDamageType, fDamage, isAlreadyWrecked, bMeleeDamage);
				}

				return fApplyDamage;
			}
		}
	}

	// Apply vehicle damage modifier
	if( pInflictor && pInflictor->GetIsTypeVehicle() )
	{
		CVehicle* pInflictorVehicle = static_cast<CVehicle*>( pInflictor );
		Assert( pInflictorVehicle );
		CPed* pPedDriver = pInflictorVehicle->GetDriver(); 
		if( pPedDriver && pPedDriver->IsPlayer() )
		{
			Assert( pPedDriver->GetPlayerInfo() );
			fApplyDamage *= pPedDriver->GetPlayerInfo()->GetPlayerVehicleDamageModifier();
		}
	}

	// check if this vehicle will take damage from collisions
	// do this after calculations above so we still get pad shake from collisions
	if( ( !forceMinDamage ||
		  !CPhysics::ms_bInArenaMode ) &&  
		!CanVehicleBeDamaged(pInflictor, nWeaponHash, false))
	{
		if(NetworkInterface::ShouldTriggerDamageEventForZeroDamage(m_pParent))
		{
			UpdateDamageTrackers(pInflictor, nWeaponHash, nDamageType, 0.0f, isAlreadyWrecked, bMeleeDamage, nMaterialId);
		}
		return 0.0f;
	}
	if (NetworkInterface::IsGameInProgress())
	{
		if (!CanVehicleBeDamagedBasedOnDriverInvincibility())
			return 0.0f;

		TUNE_GROUP_FLOAT( VEHICLE_DAMAGE_STUNT_MODE, COLLISION_DAMAGE_THRESHOLD, 35.0f, 0.0f, 100000.0f, 1.0f );
		if( CPhysics::ms_bInStuntMode && 
			!CPhysics::ms_bInArenaMode &&
			nDamageType == DAMAGE_TYPE_COLLISION )
		{
			if( fApplyDamage < COLLISION_DAMAGE_THRESHOLD )
			{
				return 0.0f;
			}
		}
	}

	Vector3 vecImpactPosLocal = vecPos;
	if(vecPos.IsNonZero())
		vecImpactPosLocal = VEC3V_TO_VECTOR3(m_pParent->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vecPos)));

	Vector3 vecImpactDirnLocal = vecDirn;
	if(vecDirn.IsNonZero())
		vecImpactDirnLocal = VEC3V_TO_VECTOR3(m_pParent->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecDirn)));

	Vector3 vecImpactNormLocal = vecNorm;
	if(vecNorm.IsNonZero())
		vecImpactNormLocal = VEC3V_TO_VECTOR3(m_pParent->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecNorm)));

	// some impacts come specifically from faked wheel impacts
	bool bWheelDamage = false;
	if(PGTAMATERIALMGR->GetPolyFlagVehicleWheel(nMaterialId))
		bWheelDamage = true;

	if(!bWheelDamage && m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR && nDamageType == DAMAGE_TYPE_MELEE)
	{
		if (!m_pParent->GetDriver())		// If this car has a driver triggering the alarm would be silly (sometimes happened in network games)
		{
			if(m_pParent->GetVehicleAudioEntity())
			{
				m_pParent->GetVehicleAudioEntity()->TriggerAlarm();
			}
		}
	}

	// ignore wheel impacts for damage to certain parts of the vehicle
	// Except when it is of damage type explosive
	if( (!bWheelDamage || nDamageType == DAMAGE_TYPE_EXPLOSIVE) && m_pParent->GetVehicleType() != VEHICLE_TYPE_BICYCLE) //Don't apply damage to bicyles
	{
		// apply damage to the body as a whole
		ApplyDamageToBody(pInflictor, nDamageType, nWeaponHash, fApplyDamage, vecImpactPosLocal, vecImpactNormLocal, nComponent, isFlameThrowerFire );

		if(NetworkInterface::IsGameInProgress() && pInflictor && bChainExplosion)
		{
			CPed* pInflictorPed = NULL;
			if(pInflictor->GetIsTypePed())
			{
				pInflictorPed = static_cast<CPed*>(pInflictor);

				if(pInflictorPed->GetPlayerWanted() && pInflictorPed->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL1)
				{
					bNoEngineDamage = true;
				}
			}
		}

		// apply damage specifically to the engine
		if(!bNoEngineDamage &&
			!CPhysics::ms_bInArenaMode )
		{
			ApplyDamageToEngine(pInflictor, nDamageType, fApplyDamage, vecImpactPosLocal, vecImpactNormLocal, vecImpactDirnLocal, bFireDriveby, bIsAccurate, fDamageRadius, bAvoidExplosions, nWeaponHash, bChainExplosion);
		}

		// apply damage to smashable components (mainly lights, and glass smashing due to deformation)
		ApplyDamageToGlass(fApplyDamage, vecImpactPosLocal, vecImpactNormLocal, vecImpactDirnLocal);
	}

	// can't apply damage to network clones 
	if (NetworkUtils::IsNetworkCloneOrMigrating(m_pParent))
	{
		if (fApplyDamage > 0.0f)
		{
			static_cast<CNetObjVehicle*>(m_pParent->GetNetworkObject())->CacheCollisionFault(pInflictor, nWeaponHash);
		}

		return 0.0f;
	}

	// apply damage to each wheel
	ApplyDamageToWheels(pInflictor, nDamageType, fApplyDamage, vecImpactPosLocal, vecImpactNormLocal, vecImpactDirnLocal, nComponent, nMaterialId, nPieceIndex);

	if( m_pParent->GetVehicleType() != VEHICLE_TYPE_BICYCLE )
	{
		// do other responses to damage
		ApplyDamageToOverallHealth(pInflictor, nDamageType, nWeaponHash, fApplyDamage, vecImpactPosLocal, vecImpactDirnLocal, nComponent, bChainExplosion);
	}

	if(m_pParent->InheritsFromBike())
	{
		if(fApplyDamage > BIKE_KNOCKOVER_DAMAGE)
			((CBike*)m_pParent)->m_nBikeFlags.bOnSideStand = false;
	}

	if(m_pParent->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(m_pParent);
		pPlane->GetAircraftDamage().ApplyDamageToPlane(pPlane,pInflictor, nDamageType, nWeaponHash, fApplyDamage, vecPos, vecNorm, vecDirn, nComponent, nMaterialId, nPieceIndex, bFireDriveby, bLandingDamage, fDamageRadius);
		pPlane->GetLandingGearDamage().ApplyDamageToPlane(pPlane,pInflictor, nDamageType, nWeaponHash, fApplyDamage, vecPos, vecNorm, vecDirn, nComponent, nMaterialId, nPieceIndex, bFireDriveby, bLandingDamage, fDamageRadius);

		if(bLandingDamage && !m_pParent->GetIsRotaryAircraft()) // Don't apply the landing damage to non-rotary airplane itself
		{
			return 0.0f;
		}
	}

	//Update the damage tracker for the network game.
	float newHealth = m_pParent->GetHealth() + GetEngineHealth() + GetPetrolTankHealth();
	if (bIncludeBodyDamage)
	{
		newHealth += m_fBodyHealth;
	}

	for(s32 i=0; i<m_pParent->GetNumWheels(); i++)
	{
		newHealth += GetTyreHealth(i);
		newHealth += GetSuspensionHealth(i);
	}
	UpdateDamageTrackers(pInflictor, nWeaponHash, nDamageType, previousHealth - newHealth, isAlreadyWrecked, bMeleeDamage, nMaterialId);

	// if the player ped has caused this vehicle damage then store this vehicle (so we can get it to leak petrol)
	if (pInflictor && pInflictor->GetIsTypePed())
	{
		CPed* pInflictorPed = static_cast<CPed*>(pInflictor);
		if (pInflictorPed && pInflictorPed->IsLocalPlayer())
		{
			g_vfxVehicle.SetRecentlyDamagedByPlayer(m_pParent);
		}
	}

	// if the player ped has caused this vehicle damage then store this vehicle (so we can get it to leak petrol)
	if (pInflictor && (pInflictor->GetIsTypeVehicle() || pInflictor->GetIsTypeBuilding()))
	{
		if (fApplyDamage>=VFXVEHICLE_RECENTLY_CRASHED_DAMAGE_THRESH)
		{
			g_vfxVehicle.SetRecentlyCrashed(m_pParent, fApplyDamage);
		}
	}

	if (NetworkInterface::IsGameInProgress() && !isAlreadyWrecked)
	{
		if (m_pParent->GetNetworkObject() && !m_pParent->IsNetworkClone())
		{
			CNetObjVehicle* pNetObjVehicle = static_cast<CNetObjVehicle*>(m_pParent->GetNetworkObject());
			if (pNetObjVehicle)
				pNetObjVehicle->DirtyVehicleDamageStatusDataSyncNode();
		}
	}

	return fDamage;
}

void  CVehicleDamage::UpdateDamageTrackers(CEntity* pInflictor, u32 nWeaponHash, const eDamageType nDamageType, const float totalDamage, const bool isAlreadyWrecked, const bool isMeleeDamage, phMaterialMgr::Id nMaterialId)
{
	//Check if we need to lock for future damage tracking.
	if (m_pParent->m_nVehicleFlags.bDamageTrackingLocked)
		return;

	u32 lastDamgedTime = m_pParent->GetWeaponDamagedTime();

	//Need to clear this in a network game as the WeaponDamageEntity 
	//is used to determine player kills.
	if (NetworkInterface::IsGameInProgress() && !pInflictor && (0.0f<totalDamage || NetworkInterface::ShouldTriggerDamageEventForZeroDamage(m_pParent)) && !isAlreadyWrecked)
	{
		m_pParent->ResetWeaponDamageInfo();
	}

	CEntity* damager = pInflictor;
	if(damager)
	{
		//Time last since last damage applied to the vehicle
		const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - lastDamgedTime;

		if(isAlreadyWrecked)
		{
			//Don't set weapon damage after the vehicle is already destroyed.
			damager = 0;
		}
		else if(nWeaponHash == WEAPONTYPE_RAMMEDBYVEHICLE && !pInflictor->GetIsTypeVehicle() && timeSinceLastDamage < 1000)
		{
			//Don't set weapon damage if it was rammed by a vehicle and the inflictor is not a vehicle for the first 1s.
			damager = 0;
		}
		else if(nWeaponHash == WEAPONTYPE_RAMMEDBYVEHICLE && pInflictor->GetIsTypePed())
		{
			//Don't set weapon damage if it was rammed by a vehicle and the inflictor is a ped.
			damager = 0;
		}
		else
		{
			//Previous damage entity was a vehicle
			const bool previousDamagerIsVehicle = m_pParent->GetWeaponDamageEntity() ? m_pParent->GetWeaponDamageEntity()->GetIsTypeVehicle() : false;

			//Damages done by vehicles are attributed to the 1st vehicle causing damage in the 1st place, only if the WeaponDamageHash is WEAPONTYPE_RUNOVERBYVEHICLE
			if (timeSinceLastDamage <= 1000 && nWeaponHash == WEAPONTYPE_RUNOVERBYVEHICLE && previousDamagerIsVehicle && nWeaponHash == m_pParent->GetWeaponDamageHash())
			{
				damager = m_pParent->GetWeaponDamageEntity();
			}
			else if (pInflictor->GetIsTypeBuilding() || pInflictor->GetIsTypeAnimatedBuilding() || pInflictor->GetIsTypeLight())
			{
				if (WEAPONTYPE_RAMMEDBYVEHICLE == nWeaponHash || WEAPONTYPE_EXPLOSION == nWeaponHash)
				{
					//Previous damage entity was a ped
					const bool previousDamagerIsPed = m_pParent->GetWeaponDamageEntity() ? m_pParent->GetWeaponDamageEntity()->GetIsTypePed() : false;
					if (timeSinceLastDamage <= 1000 && previousDamagerIsPed)
					{
						damager     = m_pParent->GetWeaponDamageEntity();
						nWeaponHash = m_pParent->GetWeaponDamageHash();
					}
					else if (m_pParent->GetDriver())
					{
						damager = m_pParent->GetDriver();
					}
				}
			}

			m_pParent->SetWeaponDamageInfo(damager, nWeaponHash, fwTimer::GetTimeInMilliseconds(), isMeleeDamage);
		}
	}

	//Update the network stuff
	UpdateNetworkDamageTracker(damager, totalDamage, nWeaponHash, isAlreadyWrecked, isMeleeDamage, nMaterialId);

	//Setup stats damage to vehicles
	CStatsMgr::RegisterVehicleDamage(GetParent(), damager, nDamageType, totalDamage, isAlreadyWrecked);

#if VEHICLE_DEFORMATION_STOP_AFTER_WRECKAGE
	//Check if we need to lock for future damage tracking.
	if (!m_pParent->m_nVehicleFlags.bDamageTrackingLocked && m_pParent->GetStatus() == STATUS_WRECKED)
	{
		m_pParent->m_nVehicleFlags.bDamageTrackingLocked = true;
	}
#endif
}

// Depending on the flags that have been set for this vehicle we might
// decide not to do damage.
bool CVehicleDamage::CanVehicleBeDamaged(CEntity* pInflictor, u32 nWeaponHash, bool bDoNetworkCloneCheck)
{
	CPed* pInflictorPed = NULL;
	if(pInflictor && pInflictor->GetIsTypePed())
		pInflictorPed  = (CPed*)pInflictor;

	//	Player can't damage the vehicle he is attached to
	if (pInflictorPed && pInflictorPed->IsPlayer() && pInflictorPed->GetAttachParent() == m_pParent && nWeaponHash != 0)
    {
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
		if(pWeaponInfo && pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_EXPLOSIVE && pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_FIRE)
		{
		    return false;
        }
    }

	if(pInflictor && pInflictor->GetIsTypeObject())
	{
		// Do not apply collision damage to vehicle if hits a parachute
		if(CTaskParachute::IsParachute(*((CObject *)pInflictor)))
		{
			return false;
		}
	}

	if(pInflictorPed && pInflictorPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
	{
		return false;
	}

	if (m_pParent->CanPhysicalBeDamaged(pInflictor, nWeaponHash, bDoNetworkCloneCheck) == false)
	{
		return false;
	}

	return true;
}

bool CVehicleDamage::CanVehicleBeDamagedBasedOnDriverInvincibility()
{
	if(NetworkInterface::IsGameInProgress() && m_pParent)
	{
		CPed* pDriver = m_pParent->GetDriver();
		if (pDriver && pDriver->IsPlayer() && pDriver->GetNetworkObject())
		{
			CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(pDriver->GetNetworkObject());
			if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
				return false;
		}
	}

	return true;
}

void CVehicleDamage::RefillOilAndPetrolTanks()
{
    if(m_pParent && m_pParent->pHandling)
    {
        m_fPetrolTankLevel = m_pParent->pHandling->m_fPetrolTankVolume;
        m_fOilLevel = m_pParent->pHandling->m_fOilVolume;
    }
}

void CVehicleDamage::FixVehicleDamage(bool resetFrag, bool allowNetwork)
{
	m_fBodyHealth = GetDefaultBodyHealthMax(m_pParent);

	m_fPetrolTankHealth = Max(m_pParent->GetMaxHealth(), VEH_DAMAGE_HEALTH_STD);

    RefillOilAndPetrolTanks();

    if( m_pParent->m_bHasDeformationBeenApplied )
    {
	    GetDeformation()->ResetDamage();
    }

	// remove bouncing panels
	for(int i=0; i<MAX_BOUNCING_PANELS; i++)
		m_BouncingPanels[i].m_nComponentIndex = -1;

	if(m_pParent->GetVehicleFragInst())
	{
		// Store the speed to restore it later
		Vector3 StoredSpeed = m_pParent->GetVelocity();
		Vector3 StoredTurnSpeed = m_pParent->GetAngVelocity();

		CTrailer* pAttachedTrailer = m_pParent->GetAttachedTrailer();
		
		// Don't want stale contacts left around with bad collider pointers
		if(!CPhysics::GetLevel()->IsInLevel(m_pParent->GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			CPhysics::GetSimulator()->AddActiveObject(m_pParent->GetCurrentPhysicsInst(), false);
		}

		// Ronseal...
		if (resetFrag || m_pParent->GetHavePartsBrokenOff())
			ResetFragCacheEntry();
		m_pParent->SetHavePartsBrokenOff(false);

		// Restore the speed to what it was before

        //Zero velocity if above 80m/s
        if(StoredSpeed.Mag2() > rage::square(80.0f))
        {
            //Clear the velocity.
            StoredSpeed.Zero();
        }
		m_pParent->SetVelocity(StoredSpeed);


        //Zero angular velocity if above 80m/s
        if(StoredTurnSpeed.Mag2() > rage::square(80.0f))
        {
            //Clear the angular velocity.
            StoredTurnSpeed.Zero();
        }
		m_pParent->SetAngVelocity(StoredTurnSpeed);

		if(pAttachedTrailer)
		{
			pAttachedTrailer->AttachToParentVehicle(m_pParent, false);
		}
	}

	for(int i=0; i<m_pParent->GetNumWheels(); i++)
		m_pParent->GetWheel(i)->ResetDamage();

	for(int i=0; i<m_pParent->GetNumDoors(); i++)
		m_pParent->GetDoor(i)->Fix(m_pParent);


	//Checks if any network clone plane's broken doors have been reattached above and breaks them off again: B*1931220
	if (m_pParent->IsNetworkClone() && m_pParent->GetNetworkObject() && m_pParent->InheritsFromPlane())
	{
		CNetObjPlane* pNetObjPlane = static_cast<CNetObjPlane*>(m_pParent->GetNetworkObject());

		pNetObjPlane->UpdateCloneDoorsBroken();
	}

	m_pParent->m_Transmission.SetEngineHealth(Max(m_pParent->GetMaxHealth(), ENGINE_HEALTH_MAX));
	m_pParent->m_Transmission.RefillKERS();
	m_pParent->ResetBrokenAndHiddenFlags();

	m_LightState.Reset();
	m_UpdateLightBones = false;
	m_SirenState.Reset();
	m_UpdateSirenBones = false;

	m_pParent->ResetWheelBroken();

	if (!m_pParent->IsNetworkClone() || allowNetwork)
		m_pParent->SetHealth( CREATED_VEHICLE_HEALTH, allowNetwork );

	m_fDynamicSpoilerDamage = 0.0f;
}


void CVehicleDamage::FixVehicleDeformation()
{
	GetDeformation()->ResetDamage();
}

void  CVehicleDamage::ResetFragCacheEntry()
{
	if(fragInst* pFragInst = m_pParent->GetVehicleFragInst())
	{
		// Preserve the disabled breaking flags since that gets reset when we lose our cache entry
		bool isBreakingFlagOverridden = pFragInst->IsBreakingFlagOverridden();
		bool isBreakingDisabled = pFragInst->IsBreakingDisabled();

		// This will fix broken off frag parts
		fragCacheEntry *pCacheEntry = pFragInst->GetCacheEntry();
		if(pCacheEntry)
		{
			FRAGCACHEMGR->ReleaseCacheEntry(pCacheEntry);
		}
		pFragInst->PutIntoCache();

		if(isBreakingFlagOverridden)
		{
			pFragInst->SetDisableBreakable(isBreakingDisabled);
		}

		m_pParent->ActivatePhysics();		
		
		fwAttachmentEntityExtension* pExtension = m_pParent->GetAttachmentExtension();
		if (pExtension) 
			pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true); // Necessary to stop spurious assert in SetMatrix().
		
		m_pParent->SetMatrix(MAT34V_TO_MATRIX34(m_pParent->GetMatrix()), true, true, true);

		if (pExtension) 
			pExtension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);

        if(NetworkInterface::IsGameInProgress())
        {
            NetworkInterface::OnVehiclePartDamage(m_pParent, VEH_BUMPER_F, VEH_BB_NONE);
            NetworkInterface::OnVehiclePartDamage(m_pParent, VEH_BUMPER_R, VEH_BB_NONE);
        }
	}
}

float CVehicleDamage::GetOverallHealth() const
{
	return m_pParent->GetHealth();
}
float CVehicleDamage::GetEngineHealth() const
{
	return m_pParent->m_Transmission.GetEngineHealth();
}
bool  CVehicleDamage::GetEngineOnFire() const
{
	return m_pParent->m_Transmission.GetEngineOnFire();
}
void  CVehicleDamage::ResetEngineHealth()
{
	m_pParent->m_Transmission.ResetEngineHealth();
}
void  CVehicleDamage::SetEngineHealth( float fHealth )
{
	m_pParent->m_Transmission.SetEngineHealth(fHealth);
}

void  CVehicleDamage::SetBodyHealth(float fHealth)
{
	ENABLE_FRAG_OPTIMIZATION_ONLY(m_pParent->GiveFragCacheEntry();)

	m_fBodyHealth = fHealth;
	vehicleDebugf3("%s setting body health: %.2f", m_pParent->GetLogName(), m_fBodyHealth);
}

float CVehicleDamage::GetSuspensionHealth(int i) const
{
	if(const CWheel * pWheel = m_pParent->GetWheel(i))
		return pWheel->GetSuspensionHealth();
	return 0.0f;
}
float CVehicleDamage::GetTyreHealth(int i) const
{
	if(const CWheel * pWheel = m_pParent->GetWheel(i))
		return pWheel->GetTyreHealth();
	return 0.0f;
}

int CVehicleDamage::GetAnyWheelsBurst() const
{
	for(int i=0; i<m_pParent->GetNumWheels(); i++)
	{
		if(m_pParent->GetWheel(i)->GetTyreHealth() < TYRE_HEALTH_DEFAULT)
			return true;
	}

	return false;
}



#define NUM_MISC_CAR_BREAK_PARTS (4)
eHierarchyId aMiscCarBreakParts[NUM_MISC_CAR_BREAK_PARTS] = 
{	
	VEH_BUMPER_F,
	VEH_BUMPER_R,
	VEH_WING_RF,
	VEH_WING_LF
};

#define NUM_EXTRA_TRAILER_BREAK_PARTS (VEH_LAST_BREAKABLE_EXTRA + 1 - VEH_BREAKABLE_EXTRA_1)
eHierarchyId aExtraTrailerBreakParts[NUM_EXTRA_TRAILER_BREAK_PARTS] = 
{	
	VEH_BREAKABLE_EXTRA_1,
	VEH_BREAKABLE_EXTRA_2,
	VEH_BREAKABLE_EXTRA_3,
	VEH_BREAKABLE_EXTRA_4,
	VEH_BREAKABLE_EXTRA_5,
	VEH_BREAKABLE_EXTRA_6,
	VEH_BREAKABLE_EXTRA_7,
	VEH_BREAKABLE_EXTRA_8,
	VEH_BREAKABLE_EXTRA_9,
	VEH_BREAKABLE_EXTRA_10
};

dev_float sfBlowUpCarDoorProb = 0.5f;
dev_float sfBlowUpCarWheelProb = 0.2f;
dev_float sfBlowUpBikeWheelProb = 0.5f;
dev_float sfBlowUpCarExtrasProb = 0.5f;
dev_float sfBlowUpCarDeleteCompProbNetwork = 0.6f;


void CVehicleDamage::BreakOffWheel(int wheelIndex, float ptfxProbability, float deleteProbability, float burstTyreProbability, bool dueToExplosion, bool bNetworkCheck)
{
	if (NetworkInterface::IsGameInProgress() && bNetworkCheck && m_pParent->IsNetworkClone())
		return;

	fragInst* pFragInst = m_pParent->GetVehicleFragInst();
	bool bDeleteParts = !m_pParent->CarPartsCanBreakOff();

	CWheel* pWheel = m_pParent->GetWheel(wheelIndex);
	int nFragChild = pWheel->GetFragChild();
	if(nFragChild > -1)
	{
		fragPhysicsLOD* pTypePhysics = pFragInst->GetTypePhysics();
		fragTypeChild* pChild = pTypePhysics->GetAllChildren()[nFragChild];
		if(pChild->GetOwnerGroupPointerIndex() > 0 && !pFragInst->GetChildBroken(nFragChild))
		{
			m_pParent->PartHasBrokenOff(pWheel->GetHierarchyId());

			m_pParent->SetWheelBroken(wheelIndex);

			// depending on distance, just delete component instead of having it come flying off
			if(bDeleteParts || fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < deleteProbability)
			{
				pFragInst->DeleteAbove(nFragChild);
			}
			else
			{
				fragInst* pNewFragInst = pFragInst->BreakOffAbove(nFragChild);
				if (pNewFragInst)
				{
					CEntity* pNewEntity = CPhysics::GetEntityFromInst(pNewFragInst);
					if (pNewEntity)
					{
						// Clone the vehicle archetype in order to increase damping for the wheels.
						phArchetypeDamp* pNewArch = static_cast<phArchetypeDamp*>(pNewEntity->GetPhysArch()->Clone());
						pNewArch->ActivateDamping(phArchetypeDamp::LINEAR_C,Vector3(0.3f,0.3f,0.3f));
						pNewArch->ActivateDamping(phArchetypeDamp::LINEAR_V,Vector3(0.1f,0.1f,0.1f));
						pNewArch->ActivateDamping(phArchetypeDamp::LINEAR_V2,Vector3(0.05f,0.05f,0.05f));
						pNewArch->ActivateDamping(phArchetypeDamp::ANGULAR_C,Vector3(0.02f,0.02f,0.02f));
						pNewArch->ActivateDamping(phArchetypeDamp::ANGULAR_V,Vector3(0.1f,0.1f,0.1f));
						pNewArch->ActivateDamping(phArchetypeDamp::ANGULAR_V2,Vector3(0.15f,0.15f,0.15f));

						pNewFragInst->SetArchetype(pNewArch);

						if(dueToExplosion && g_DrawRand.GetFloat() < ptfxProbability)
						{
							g_vfxVehicle.TriggerPtFxVehicleDebris(pNewEntity);
						}

						// If it is already burst we need to tell the shader to render the new wheel without the tyre
						// - And sometimes we put the tyre back just to keep things interesting
						if(pWheel->GetTyreHealth() <= 0.0f || fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= burstTyreProbability)
						{
							CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(pNewEntity->GetDrawHandler().GetShaderEffect());
							Assert(pShader);
							// Flag that tyres need to be deformed
							pShader->SetEnableTyreDeform(true);
						}
					}
				}
			}

			CVehicle::ClearLastBrokenOffPart();
		}
	}
}

void CVehicleDamage::ApplyAdditionalVelocityToVehiclePart(phInst& oldInst, phInst& newInst, const CVehicleExplosionInfo& vehicleExplosionInfo)
{
	if(CEntity* pNewEntity = CPhysics::GetEntityFromInst(&newInst))
	{
		if(pNewEntity->GetIsPhysical())
		{
			Vec3V explosionDirection = NormalizeSafe(Subtract(newInst.GetCenterOfMass(),oldInst.GetCenterOfMass()),Vec3V(V_Z_AXIS_WZERO));

			// GTAV - B*1803633 - rotate the explosion direction into world space so objects don't behave strangely when
			// the parent vehicle is upside down.
			explosionDirection = m_pParent->GetTransform().Transform3x3( explosionDirection );
			const Vec3V additionalVelocity = vehicleExplosionInfo.ComputeAdditionalPartVelocity(explosionDirection);
			CPhysical* pNewPhysical = static_cast<CPhysical*>(pNewEntity);
			pNewPhysical->ApplyImpulseCg(VEC3V_TO_VECTOR3(Scale(additionalVelocity,ScalarVFromF32(pNewPhysical->GetMass()))),CPhysical::IF_InitialImpulse);
		}
	}
}

void CVehicleDamage::BreakOffPart(eHierarchyId hierarchyId, int fragChild, fragInst *pFragInst, const CVehicleExplosionInfo* vehicleExplosionInfo, bool bDeleteParts, float fDeleteProb, float fxProb, bool bApplyAdditionalVelocityToPart)
{
	m_pParent->PartHasBrokenOff(hierarchyId);

	// depending on distance, just delete component instead of having it come flying off
	if(bDeleteParts || fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fDeleteProb || CVehicle::TooManyVehicleBreaksRecently() )
	{
		pFragInst->DeleteAbove(fragChild);
	}
	else
	{
		fragInst* pNewFragInst = pFragInst->BreakOffAbove(fragChild);
		if (pNewFragInst)
		{
			if(bApplyAdditionalVelocityToPart)
			{
				ApplyAdditionalVelocityToVehiclePart(*pFragInst,*pNewFragInst,*vehicleExplosionInfo);
			}
			CEntity* pNewEntity = CPhysics::GetEntityFromInst(pNewFragInst);
			if (pNewEntity)
			{	
				if (g_DrawRand.GetFloat()<fxProb)
				{
					g_vfxVehicle.TriggerPtFxVehicleDebris(pNewEntity);
				}
			}
		}
	}

	CVehicle::ClearLastBrokenOffPart();
}

void CVehicleDamage::BlowUpCarParts(CEntity* pInflictor, int eBreakingState)
{
	if(ms_fBreakingPartsReduction >= 1.0f)
		return;

#if __BANK
    if( m_pParent->GetFragInst() &&
        m_pParent->GetFragInst()->IsBreakingDisabled() )
    {
        return;
    }
#endif // #if __BANK

	if(eBreakingState != Break_Off_Car_Parts_Immediately && CApplyDamage::GetNumDamagePending(m_pParent) > 0)
	{
		m_uBlowUpCarPartsPending = (u8)eBreakingState;
		return;
	}

	Assert(!ms_bDisableVehiclePartCollisionOnBreak);
	ms_bDisableVehiclePartCollisionOnBreak = true;
	Assert(m_pParent->GetVehicleFragInst());

	fragInst* pFragInst = m_pParent->GetVehicleFragInst();

	bool bDeleteParts = !m_pParent->CarPartsCanBreakOff();
	
	float fxProb = 0.0f;
	if (m_pParent->GetVehicleType()==VEHICLE_TYPE_HELI)
		fxProb = 0.75f;
	else if (m_pParent->GetVehicleType()==VEHICLE_TYPE_CAR)
		fxProb = 0.25f;

	float blowUpWheelProb = sfBlowUpCarWheelProb;
	if (m_pParent->GetVehicleType()==VEHICLE_TYPE_BIKE)
		blowUpWheelProb = sfBlowUpBikeWheelProb;

	float fDistToCamSqr = LARGE_FLOAT;
	if(m_pParent->IsVisible())
	{
		// if heli's are on screen, don't delete components
		if(m_pParent->GetVehicleType()==VEHICLE_TYPE_HELI)
			fDistToCamSqr = 0.0f;
		else
		{
			fDistToCamSqr = CVfxHelper::GetDistSqrToCamera(m_pParent->GetTransform().GetPosition());
		}
	}

	// Compute a chance to deletion probability
	const CVehicleExplosionInfo* vehicleExplosionInfo = m_pParent->GetVehicleModelInfo()->GetVehicleExplosionInfo();
	const CVehicleExplosionLOD& vehicleExplosionLOD = vehicleExplosionInfo->GetVehicleExplosionLODFromDistanceSq(fDistToCamSqr);
	float fDeleteProb = vehicleExplosionLOD.GetPartDeletionChance();

	if(NetworkInterface::IsGameInProgress())
		fDeleteProb = Max(sfBlowUpCarDeleteCompProbNetwork,fDeleteProb);

	// doors
	if (!m_pParent->IsNetworkClone())
	{
		for(int i=0; i<m_pParent->GetNumDoors(); i++)
		{
			CCarDoor* door = m_pParent->GetDoor(i);
			if(door->GetFragChild() > 0 && door->GetIsIntact(m_pParent) && door->GetDoorAllowedToBeBrokenOff())
			{
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sfBlowUpCarDoorProb*GetVehicleExplosionBreakChanceMultiplier())
				{					
					float fPartFxProb = fxProb;
					float fPartDeleteProb = fDeleteProb;
					if(ms_fBreakingPartsReduction >= 0.6f)
					{				
						fPartDeleteProb = 100;
					}

					BreakOffPart( door->GetHierarchyId(), door->GetFragChild(), pFragInst, vehicleExplosionInfo, bDeleteParts, fPartDeleteProb, fPartFxProb );
				}
			}
		}
	}

	// wheels
	if (!m_pParent->IsNetworkClone())
	{
		if(m_pParent->m_nVehicleFlags.bCanBreakOffWheelsWhenBlowUp)
		{
			for(int i=0; i<m_pParent->GetNumWheels(); i++)
			{
				// GTAV - B*1808115 - bike will sink into the ground 100% of the time.
				// don't blow both wheels of a bike to stop it sinking into the ground
				bool bCanBreakOff = true;
				if(m_pParent->InheritsFromBike() && m_pParent->GetWheel(i) && m_pParent->GetWheel(i)->GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
				{
					CBike *pBike = static_cast<CBike*>(m_pParent);
					if(!pBike->m_nBikeFlags.bHasRearSuspension)
					{
						bCanBreakOff = false;
					}
				}

				if( bCanBreakOff && !m_pParent->GetWheelBroken( 1 - i ) )
				{
					if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < blowUpWheelProb*GetVehicleExplosionBreakChanceMultiplier())
					{						
						float fPartFxProb = fxProb;
						float fPartDeleteProb = fDeleteProb;
						if(ms_fBreakingPartsReduction >= 0.5f)
						{
							fPartDeleteProb = 100;
						}

						BreakOffWheel(i, fPartFxProb, fPartDeleteProb, true);
					}
				}
			}
		}
	}

	// extras and misc

	// go thru all the fragment children (there's more of them)
	for(int nChild=0; nChild<pFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
	{
		if(pFragInst->GetChildBroken(nChild))
			continue;

		int boneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragInst->GetTypePhysics()->GetAllChildren()[nChild]->GetBoneID());

		if(m_pParent->GetIsRotaryAircraft())
		{
			CRotaryWingAircraft *pRotaryAircraft = (CRotaryWingAircraft *)m_pParent;
			if(boneIndex == pRotaryAircraft->GetBoneIndex(HELI_TAIL))
			{
				if(!pRotaryAircraft->GetCanBreakOffTailBoom() || pRotaryAircraft->GetBreakOffTailBoomPending())
				continue;
			}
		}

		float blowUpCarExtrasProb = sfBlowUpCarExtrasProb*GetVehicleExplosionBreakChanceMultiplier();

		// in MP blow off all extras, so the vehicle can sync correctly - otherwise it may bounce around trying to blend to an unobtainable position
		if (m_pParent->GetNetworkObject())
		{
			blowUpCarExtrasProb = 1.0f;
		}

		// go through each of the possible extras
		if(!m_pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_STRONG))
		{
			int nExtra=VEH_EXTRA_1;

			// GTAV - B*2362495 - We can't break off VEH_EXTRA_1 on the insurgent because the turret weapon is attached to it
			if( ( MI_CAR_INSURGENT.IsValid() &&
				  m_pParent->GetModelIndex() == MI_CAR_INSURGENT.GetModelIndex() ) ||
				( MI_CAR_LIMO2.IsValid() &&
				  m_pParent->GetModelIndex() == MI_CAR_LIMO2.GetModelIndex() ) ||
				( MI_CAR_INSURGENT3.IsValid() &&
				  m_pParent->GetModelIndex() == MI_CAR_INSURGENT3.GetModelIndex() ) )
			{
				nExtra = VEH_EXTRA_2;
			}

			for(; nExtra<=VEH_LAST_EXTRA; nExtra++)
			{
				// if the bone index of this child matches the bone index of the extra that's turned off then delete this component
				if(m_pParent->GetBoneIndex((eHierarchyId)nExtra) == boneIndex)
				{
					if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= sfBlowUpCarExtrasProb)
					{
						float fPartFxProb = fxProb;
						float fPartDeleteProb = fDeleteProb;
						if(ms_fBreakingPartsReduction >= 0.2f)
						{
							fPartDeleteProb = 100;
						}

						BreakOffPart( (eHierarchyId)nExtra, nChild, pFragInst, vehicleExplosionInfo, bDeleteParts, fPartDeleteProb, fPartFxProb );
					}

					break;
				}
			}
		}

		for( int nPanel = VEH_FIRST_BREAKABLE_PANEL; nPanel <= VEH_LAST_BREAKABLE_PANEL; nPanel++ )
		{
			// if the bone index of this child matches the bone index of the extra that's turned off then delete this component
			if( m_pParent->GetBoneIndex( (eHierarchyId)nPanel ) == boneIndex )
			{
				if( fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) <= sfBlowUpCarExtrasProb )
				{
					float fPartFxProb = fxProb;
					float fPartDeleteProb = fDeleteProb;
					if( ms_fBreakingPartsReduction >= 0.2f )
					{
						fPartDeleteProb = 100;
					}

					BreakOffPart( (eHierarchyId)nPanel, nChild, pFragInst, vehicleExplosionInfo, bDeleteParts, fPartDeleteProb, fPartFxProb );
				}

				break;
			}
		}

		int nNumBreakableParts = NUM_MISC_CAR_BREAK_PARTS;
		eHierarchyId *pBreakableParts = aMiscCarBreakParts;
		if(m_pParent->InheritsFromTrailer() && (static_cast<CTrailer*>(m_pParent))->HasBreakableExtras())
		{
			nNumBreakableParts = NUM_EXTRA_TRAILER_BREAK_PARTS;
			pBreakableParts = aExtraTrailerBreakParts;
		}

		// misc components
		for(int nMisc=0; nMisc<nNumBreakableParts; nMisc++)
		{
			// if the bone index of this child matches the bone index of the extra that's turned off then delete this component
			if(m_pParent->GetBoneIndex((eHierarchyId)pBreakableParts[nMisc]) == boneIndex)
			{
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= sfBlowUpCarExtrasProb)
				{
					float fPartFxProb = fxProb;
					float fPartDeleteProb = fDeleteProb;
					if(ms_fBreakingPartsReduction >= 0.2f)
					{
						fPartDeleteProb = 100;
					}

					BreakOffPart( ((eHierarchyId)pBreakableParts[nMisc]), nChild, pFragInst, vehicleExplosionInfo, bDeleteParts, fPartDeleteProb, fPartFxProb );
				}

				break;
			}
		}
	}

    //blow up any vehicle gadgets
    for(int i = 0; i < m_pParent->GetNumberOfVehicleGadgets(); i++)
    {
        CVehicleGadget *pVehicleGadget = m_pParent->GetVehicleGadget(i);

        pVehicleGadget->BlowUpCarParts(m_pParent);
    }

	//blow up the weapons
	if(m_pParent->GetVehicleWeaponMgr())
	{
		 for(int i = 0; i < m_pParent->GetVehicleWeaponMgr()->GetNumTurrets(); i++)
		 {
			m_pParent->GetVehicleWeaponMgr()->GetTurret(i)->BlowUpCarParts(m_pParent);
		 }
	}


	BlowUpVehicleParts(pInflictor);

	ms_bDisableVehiclePartCollisionOnBreak = false;
	// reset the pending state
	m_uBlowUpCarPartsPending = Break_Off_Car_Parts_Immediately;
}

	
void CVehicleDamage::BlowUpVehicleParts(CEntity* UNUSED_PARAM(pInflictor))
{
	const bool isTaxi = CVehicle::IsTaxiModelId(m_pParent->GetModelId());
	int lightCount = isTaxi ? NUM_GLASS_BONES : NUM_LIGHT_BONES;

	if (NetworkInterface::IsGameInProgress() && m_pParent->IsNetworkClone())
		return;

	for(int i=0; i<lightCount; i++)
	{
		const int boneIdx = m_pParent->GetBoneIndex(aGlassBones[i]);
		const int lightIdx = ( i > TAXI_IDX ) ? TAXI_IDX : i;
		const bool lightState = GetLightStateImmediate(lightIdx);
		if( boneIdx != -1 && (false == lightState))
		{
			SetLightStateImmediate(lightIdx,true);
			SetUpdateLightBones(true);
		}
	}	
	
	if( true == m_pParent->UsesSiren())
	{
		for(int i=VEH_SIREN_1; i<VEH_SIREN_MAX+1; i++)
		{
			int boneID = m_pParent->GetBoneIndex((eHierarchyId)i);
			const bool sirenState = m_pParent->GetVehicleDamage()->GetSirenState(i);
			if( boneID != -1 && (false == sirenState) )
			{
				// We got damage, and an actual point
				m_pParent->GetVehicleDamage()->SetSirenState(i,true);
			}
		}

		m_pParent->GetVehicleDamage()->SetUpdateSirenBones(true);
	}

	if (m_pParent->CarPartsCanBreakOff())
	{
		g_vehicleGlassMan.SmashExplosion(m_pParent, m_pParent->GetTransform().GetPosition(), 8.0f, 1.0f);
	}

	if (m_pParent->GetVehicleType() == VEHICLE_TYPE_BICYCLE) // Blow bicycle's wheels off
	{
		fragInst* pFragInst = m_pParent->GetVehicleFragInst();

		bool bDeleteParts = !m_pParent->CarPartsCanBreakOff();

		for(int i=0; i<m_pParent->GetNumWheels(); i++)
		{
			CWheel* wheel = m_pParent->GetWheel(i);
			int nFragChild = wheel->GetFragChild();
			if(nFragChild > -1)
			{
				fragPhysicsLOD* pTypePhysics = pFragInst->GetTypePhysics();
				fragTypeChild* pChild = pTypePhysics->GetAllChildren()[nFragChild];
				if(pChild->GetOwnerGroupPointerIndex() > 0 && !pFragInst->GetChildBroken(nFragChild))
				{
					m_pParent->PartHasBrokenOff(wheel->GetHierarchyId());

					if(bDeleteParts || CVehicle::TooManyVehicleBreaksRecently())
						pFragInst->DeleteAbove(nFragChild);
					else
					{
						pFragInst->BreakOffAbove(nFragChild);
					}

					CVehicle::ClearLastBrokenOffPart();

					m_pParent->SetWheelBroken(i);
				}
			}
		}
	}
}


static float sfPopDoorMinDamage = 40.0f;
static float sfPopDoorMaxDamage = 100.0f;
static float sfPopDoorSideDamage = 50.0f;
static float sfPopDoorChance = 0.5f;
static float sfLoosenLatchDamage = 10.0f;
static float sfLoosenLatchSideDamage = 15.0f;
static float sfLoosenLatchChance = 0.8f;
static float sfBreakDoorChance = 0.2f;
static float sfPopBonnetChance = 0.5f;
static float sfPopWindScreenChance = 0.1f;
static float sfBreakMiscChance = 0.9f;
dev_float sfPlaneInstantCrashDamage = 150.0f;
dev_float sfPlaneInstantCrashFromVehicleImpactDamage = 100.0f;
dev_float sfLargePlaneInstantCrashImpactDamage = 100.0f;
dev_float sfPlaneInstantCrashFromFlyingAircraftImpactDamage = 50.0f;
dev_float sfDamagedPlaneInstantCrashDamage = 40.0f;
dev_float sfHeliInstantCrashDamage = 300.0f;
dev_float sfHeliInstantCrashFromVehicleImpactDamage = 200.0f;
dev_float sfHeliInstantCrashFromFlyingAircraftImpactDamage = 100.0f;
dev_float sfSubmarineInstantCrashDamage = 500.0f;
dev_float sfCarInstantCrashDamage = 150.0f;
dev_float sfPopDoorBulletForce = 1000.0f;
dev_float dfBlowUpVehicleChanceByBulletWhenFullyOnFire = 0.05f;

void CVehicleDamage::ApplyDamageToBody(CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, int nComponent, bool isFlameThrowerFire )
{
	((void)pInflictor);
	((void)nDamageType);
	((void)nComponent);

	// apply damage to body
	m_fBodyHealth -= fDamage * m_fBodyDamageScale;

	if(m_fBodyHealth < 0.0f)
		m_fBodyHealth = 0.0f;

	vehicleDebugf3("%s applying body damage. New body health: %.2f", m_pParent->GetLogName(), m_fBodyHealth);

    PF_SET(DamageForce, fDamage);
	const bool bIsNetworkCloneOrMigrating = NetworkUtils::IsNetworkCloneOrMigrating(m_pParent);
	if(!bIsNetworkCloneOrMigrating && (m_pParent->InheritsFromPlane() || m_pParent->InheritsFromHeli() || m_pParent->InheritsFromSubmarine() || m_pParent->GetVehicleType() == VEHICLE_TYPE_BIKE || m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR || (NetworkInterface::IsGameInProgress() && m_pParent->GetVehicleType() == VEHICLE_TYPE_TRAILER)) && nDamageType != DAMAGE_TYPE_FIRE)
	{
		float fInstantCrashDamage = FLT_MAX;
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
		if(((pWeaponInfo && pWeaponInfo->CanBlowUpVehicleAtZeroBodyHealth()) || (nDamageType == DAMAGE_TYPE_EXPLOSIVE && m_pParent->GetShouldVehicleExplodesOnExplosionDamageAtZeroBodyHealth())) && !m_pParent->DisableExplodeOnContact() && m_fBodyHealth <= 0.0f)
		{
			m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
		}
		else if(m_pParent->InheritsFromHeli())
		{
			if(pInflictor && pInflictor->GetIsTypeVehicle())
			{
				fInstantCrashDamage = sfHeliInstantCrashFromVehicleImpactDamage;

				CVehicle *fInflictorVehicle = (CVehicle *)pInflictor;
				if((fInflictorVehicle->InheritsFromPlane() || fInflictorVehicle->InheritsFromHeli()) && fInflictorVehicle->IsInAir())
				{
					fInstantCrashDamage = sfHeliInstantCrashFromFlyingAircraftImpactDamage;
				}
			}
			else
			{
				fInstantCrashDamage = sfHeliInstantCrashDamage;
			}
		}
		else if(m_pParent->InheritsFromPlane())
		{
			CPlane *pParentPlane = (CPlane *) m_pParent;
			if(pInflictor && pInflictor->GetIsTypeVehicle())
			{
				fInstantCrashDamage = sfPlaneInstantCrashFromVehicleImpactDamage;

				CVehicle *fInflictorVehicle = (CVehicle *)pInflictor;
				if((fInflictorVehicle->InheritsFromPlane() || fInflictorVehicle->InheritsFromHeli()) && fInflictorVehicle->IsInAir())
				{
					fInstantCrashDamage = sfPlaneInstantCrashFromFlyingAircraftImpactDamage;
				}
			}
			else if(pParentPlane->GetAircraftDamage().HasSectionBrokenOff(pParentPlane,CAircraftDamage::WING_L) 
				|| pParentPlane->GetAircraftDamage().HasSectionBrokenOff(pParentPlane, CAircraftDamage::WING_R)
				|| m_fBodyHealth <= 0.0f || GetEngineHealth() < sfPlaneEngineBreakDownHealth)
			{
				fInstantCrashDamage = sfDamagedPlaneInstantCrashDamage;
			}
			else if(pParentPlane->IsLargePlane() && pParentPlane->IsInAir())
			{
				fInstantCrashDamage = sfLargePlaneInstantCrashImpactDamage;
			}
			else
			{
				fInstantCrashDamage = sfPlaneInstantCrashDamage;
			}
		}
		else if(m_pParent->InheritsFromSubmarine())
		{
			if(!m_pParent->GetIsInWater() && pInflictor && pInflictor->GetIsTypeBuilding())
			{
				fInstantCrashDamage = sfSubmarineInstantCrashDamage;
			}
		}
		else if((m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR) || (m_pParent->GetVehicleType() == VEHICLE_TYPE_TRAILER))
		{
			if((pInflictor==NULL || pInflictor->GetIsTypeBuilding()) && m_pParent->IsInAir() && nDamageType == DAMAGE_TYPE_COLLISION) // is landing damage
			{
				if(m_pParent->PopTypeIsRandom() && !m_pParent->ContainsLocalPlayer() && (m_pParent->GetDriver() == NULL || m_pParent->GetDriver()->PopTypeIsRandom())) // is ambient AI car
				{
					fInstantCrashDamage = sfCarInstantCrashDamage;
				}
			}
			else if(NetworkInterface::IsGameInProgress() && pInflictor && pInflictor->GetIsTypeVehicle())
			{
				CVehicle* pInflictorVehicle = static_cast<CVehicle*>(pInflictor);
				if(pInflictorVehicle->GetMass() > sfMinimumMassForCrushingVehiclesNetworkGame &&
                    ( m_pParent->GetSpecialFlightModeRatio() < 1.0f ||
                      m_pParent->HasGlider() ) )
				{
					fInstantCrashDamage = sfCarInstantCrashDamage;
				}
			}
		}

		bool canBlowUpFromBodyDamage = ( !m_pParent->InheritsFromHeli() || !static_cast< CHeli* >( m_pParent )->GetDisableExplodeFromBodyDamage() ) && 
									   ( !m_pParent->InheritsFromPlane() || !static_cast< CPlane* >( m_pParent )->GetDisableExplodeFromBodyDamage() );

		bool canBlowUpFromBodyDamageFromCollision = ( !m_pParent->InheritsFromPlane() || !static_cast< CPlane* >( m_pParent )->GetDisableExplodeFromBodyDamageOnCollision() );

        if( ( m_fBodyHealth <= 0.0f && 
			  fDamage > 40.0f && 
			  canBlowUpFromBodyDamage &&
			  m_pParent->GetVehicleType() != VEHICLE_TYPE_CAR && 
			  ( m_pParent->GetVehicleType() != VEHICLE_TYPE_BIKE || m_pParent->m_nVehicleFlags.bCarCrushingBike ) )	|| 
			( canBlowUpFromBodyDamageFromCollision && fDamage > fInstantCrashDamage && nDamageType == DAMAGE_TYPE_COLLISION &&
              !m_pParent->HasRamp() ) )
        {
			if(m_pParent->InheritsFromPlane())
			{
				//Can the hit component break off? if so let's break it off instead of blowing up entire plane
				CPlane *pParentPlane = (CPlane *)m_pParent;

				if(pInflictor && pInflictor->GetIsTypeVehicle())
				{
					m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
				}
				else if(pParentPlane->IsLargePlane())
				{
					m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
				}
				else if(pParentPlane->GetAircraftDamage().HasSectionBrokenOff(pParentPlane,CAircraftDamage::WING_L) 
					|| pParentPlane->GetAircraftDamage().HasSectionBrokenOff(pParentPlane, CAircraftDamage::WING_R)
					|| m_fBodyHealth <= 0.0f || GetEngineHealth() < sfPlaneEngineBreakDownHealth)
				{
					m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
				}
				else if(!pParentPlane->GetAircraftDamage().BreakOffComponent(pInflictor, pParentPlane, nComponent) && !pParentPlane->GetLandingGearDamage().BreakOffComponent(pInflictor, pParentPlane, nComponent))
				{
					m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
				}
			}
			else if(nDamageType != DAMAGE_TYPE_MELEE )
			{
				m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
			}
        }
    }

	bool bBulletCanKnockDoorOpen = false;
	if (!bIsNetworkCloneOrMigrating)
	{
	if( nDamageType == DAMAGE_TYPE_BULLET)
	{
		static dev_float fMinZForPetrolTanker = -1.0f;
		// Should this be an instant explosion?
		if(  m_pParent->ShouldExplodeOnContact() && 
			!m_pParent->DisableExplodeOnContact() &&
			 m_fBodyHealth <= 0.0f &&
			 vecPosLocal.z > fMinZForPetrolTanker)
		{
			m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
		}
		else if(m_pParent->SprayPetrolBeforeExplosion() && vecPosLocal.z > fMinZForPetrolTanker)
		{
			if(m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_LEAKING)
				m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_PTANK_LEAKING - 0.1f;

			if(m_vPetrolSprayPosLocal.IsZero())
			{
				m_vPetrolSprayPosLocal = vecPosLocal; 
				m_vPetrolSprayNrmLocal = vecNormLocal;
			}
		}

		if(GetEngineHealth() < ENGINE_DAMAGE_FIRE_FULL && fwRandom::GetRandomNumberInRange(0.0f,1.0f) < dfBlowUpVehicleChanceByBulletWhenFullyOnFire)
		{
			m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
		}

		if (nWeaponHash != 0)
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
			if(pWeaponInfo && (pWeaponInfo->GetGroup() == WEAPONGROUP_SHOTGUN || pWeaponInfo->GetGroup() == WEAPONGROUP_HEAVY || pWeaponInfo->GetForceHitVehicle() >= sfPopDoorBulletForce))
			{
				bBulletCanKnockDoorOpen = true;
			}
		}
	}
	}

	if(nDamageType == DAMAGE_TYPE_COLLISION)
	{
		if(m_pParent->m_nVehicleFlags.bExplodesOnNextImpact && !bIsNetworkCloneOrMigrating)
			m_pParent->BlowUpCar(pInflictor ? pInflictor : m_pParent);
		else
			GetParent()->GetVehicleAudioEntity()->GetCollisionAudio().UpdateDeformation(fDamage, vecPosLocal, pInflictor);
	}

    const Vector3 transformC(VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetC()));
    const float transformCZ = transformC.z;

	// trigger bouncing panels (just bumpers?)
	if((m_fCountDownToTimeAnotherPartCanBreakOff <= 0.0f || transformCZ < 0.0f) && (fDamage > 20.0f || (m_fBodyHealth < 600.0f && fDamage > 5.0f)))
	{
		//Bumper damage is applied in SP and locally in MP here - remotely it comes through repliction in NetObjVehicle. lavalley
		if (!NetworkInterface::IsGameInProgress() || !m_pParent->IsNetworkClone())
		{
			// front bumper
			if(m_BouncingPanels[0].GetCompIndex()==-1 && m_pParent->GetBoneIndex(VEH_BUMPER_F)!=-1
				&& (fwRandom::GetRandomNumber() &1) == 0)
			{
				if(vecPosLocal.y > 0.8f * m_pParent->GetBoundingBoxMax().y)
				{
					// if root of bone is on the left, then bounce axis is positive
					CBouncingPanel::eBounceAxis nAxis = CBouncingPanel::BOUNCE_AXIS_X;
					// if root of bone is on the right, then bounce axis is negative
					if(m_pParent->GetSkeletonData().GetBoneData(m_pParent->GetBoneIndex(VEH_BUMPER_F))->GetDefaultTranslation().GetXf() > 0.0f)
						nAxis = CBouncingPanel::BOUNCE_AXIS_NEG_X;

					m_BouncingPanels[0].SetPanel(VEH_BUMPER_F, nAxis, fwRandom::GetRandomNumberInRange(0.2f, 0.5f));
					ResetBrokenPartCountdown();

					if(NetworkInterface::IsGameInProgress())
					{
						NetworkInterface::OnVehiclePartDamage(m_pParent, VEH_BUMPER_F, VEH_BB_BOUNCING);
					}
				}
			}

			// rear bumper
			if(m_BouncingPanels[1].GetCompIndex()==-1 && m_pParent->GetBoneIndex(VEH_BUMPER_R)!=-1
				&& (fwRandom::GetRandomNumber() &1) == 0)
			{
				if(vecPosLocal.y < 0.8f * m_pParent->GetBoundingBoxMin().y)
				{
					// if root of bone is on the left, then bounce axis is positive
					CBouncingPanel::eBounceAxis nAxis = CBouncingPanel::BOUNCE_AXIS_X;
					// if root of bone is on the right, then bounce axis is negative
					if(m_pParent->GetSkeletonData().GetBoneData(m_pParent->GetBoneIndex(VEH_BUMPER_R))->GetDefaultTranslation().GetXf() > 0.0f)
						nAxis = CBouncingPanel::BOUNCE_AXIS_NEG_X;

					m_BouncingPanels[1].SetPanel(VEH_BUMPER_R, nAxis, fwRandom::GetRandomNumberInRange(0.2f, 0.5f));
					ResetBrokenPartCountdown();

					if(NetworkInterface::IsGameInProgress())
					{
						NetworkInterface::OnVehiclePartDamage(m_pParent, VEH_BUMPER_R, VEH_BB_BOUNCING);
					}
				}
			}
		}
	}

    if( m_pParent->GetModelIndex() == MI_TANK_KHANJALI )
    {
        ApplyDamageToBreakablePanels( nDamageType, fDamage, vecPosLocal, nComponent );
    }

	fragInstGta* pFragInst = static_cast<fragInstGta*>(m_pParent->GetVehicleFragInst());
	phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
	Assert(pFragInst);
	Assert(pBoundComposite);

    float popDoorDamage = sfPopDoorMinDamage;
    float popDoorSideDamage = sfPopDoorSideDamage;

	float armourMultiplier = m_pParent->GetVariationInstance().GetArmourDamageMultiplier();

    float popDoorChance = sfPopDoorChance * armourMultiplier;
	float loosenLatchDamage = sfLoosenLatchDamage;
	float loosenLatchSideDamage = sfLoosenLatchSideDamage;
	float loosenLatchChance = sfLoosenLatchChance * armourMultiplier;
    float popBonnetChance = sfPopBonnetChance * armourMultiplier;
    float popWindscreenChance = sfPopWindScreenChance * armourMultiplier;
    float breakDoorChance = sfBreakDoorChance * armourMultiplier;
    float breakMiscChance = sfBreakMiscChance * armourMultiplier;

	float fpopDoorDamageRate = rage::Clamp((fDamage - popDoorDamage) / (sfPopDoorMaxDamage - sfPopDoorMinDamage), 0.0f, 1.0f);
	popDoorChance += (1.0f - popDoorChance) * fpopDoorDamageRate;
	popBonnetChance += (1.0f - popBonnetChance) * fpopDoorDamageRate;
	loosenLatchChance += (1.0f - loosenLatchChance) * fpopDoorDamageRate;

    Vector3 angVelocity = m_pParent->GetAngVelocity();
    static dev_float angVelocityX  = 2.0f;//speed at which any impact should cause a door to pop open when car is rolling
    float angVelocityMult = fabs(angVelocity.x)/angVelocityX;

    static dev_float upsideDownMult = 10.0f;
    if(transformCZ < 0.0f)
    {
        angVelocityMult *= upsideDownMult;
    }

    popDoorDamage *= rage::Clamp(1.0f - angVelocityMult, 0.0f, 1.0f);
    popDoorSideDamage *= rage::Clamp(1.0f - angVelocityMult, 0.0f, 1.0f);
	loosenLatchDamage *= rage::Clamp(1.0f - angVelocityMult, 0.0f, 1.0f);
	loosenLatchSideDamage *= rage::Clamp(1.0f - angVelocityMult, 0.0f, 1.0f);

    popBonnetChance = rage::Clamp(angVelocityMult, popBonnetChance, 1.0f);
    popWindscreenChance = rage::Clamp(angVelocityMult, popWindscreenChance, 1.0f);
    breakDoorChance = rage::Clamp(angVelocityMult, breakDoorChance, 1.0f);
    breakMiscChance = rage::Clamp(angVelocityMult, breakMiscChance, 1.0f);

	// Explosions have a chance to blow up parts instead of break off
	float deleteMiscChance = 0.0f;
	if(nDamageType==DAMAGE_TYPE_EXPLOSIVE)
	{
		float fDistToCamSqr = LARGE_FLOAT;
		if(m_pParent->IsVisible())
		{
			// if heli's are on screen, don't delete components
			if(m_pParent->GetVehicleType()==VEHICLE_TYPE_HELI)
				fDistToCamSqr = 0.0f;
			else
			{
				fDistToCamSqr = CVfxHelper::GetDistSqrToCamera(m_pParent->GetTransform().GetPosition());
			}
		}

		// Compute a chance to deletion probability
		const CVehicleExplosionInfo* vehicleExplosionInfo = m_pParent->GetVehicleModelInfo()->GetVehicleExplosionInfo();
		const CVehicleExplosionLOD& vehicleExplosionLOD = vehicleExplosionInfo->GetVehicleExplosionLODFromDistanceSq(fDistToCamSqr);
		deleteMiscChance = vehicleExplosionLOD.GetPartDeletionChance();

		if(NetworkInterface::IsGameInProgress())
			deleteMiscChance = Max(sfBlowUpCarDeleteCompProbNetwork,deleteMiscChance);

		const float vehicleBreakChanceMultiplier = CVehicleDamage::GetVehicleExplosionBreakChanceMultiplier();
		breakDoorChance *= vehicleBreakChanceMultiplier;
		breakMiscChance *= vehicleBreakChanceMultiplier;
		popDoorChance *= vehicleBreakChanceMultiplier;
		popBonnetChance *= vehicleBreakChanceMultiplier;
		popWindscreenChance *= vehicleBreakChanceMultiplier;

		// Any car part that breaks in this function should disable collision with the parent vehicle until they're separated
		Assert(!ms_bDisableVehiclePartCollisionOnBreak);
		ms_bDisableVehiclePartCollisionOnBreak = true;
	}

    if( (m_fCountDownToTimeAnotherPartCanBreakOff <= 0.0f || transformCZ < 0.0f) && 
        ((fDamage > 120.0f) || 
        (m_fBodyHealth < 700.0f && fDamage > 30.0f) || 
        (m_fBodyHealth < 500.0f && fDamage > 10.0f) || 
        (transformCZ < 0.0f && fDamage > 5.0f)) && 
        !pFragInst->IsBreakingDisabled()
		&& (nDamageType==DAMAGE_TYPE_COLLISION || nDamageType==DAMAGE_TYPE_EXPLOSIVE))
    {
            // misc components and breakable parts
			BreakOffMiscComponents(deleteMiscChance, breakMiscChance, vecPosLocal);
    }

    
	if( (m_fCountDownToTimeAnotherPartCanBreakOff <= 0.0f || transformCZ < 0.0f) && 
		m_pParent->GetCarDoorLocks()==CARLOCK_UNLOCKED && 
		!pFragInst->IsBreakingDisabled() && 
		(nDamageType==DAMAGE_TYPE_COLLISION || nDamageType==DAMAGE_TYPE_EXPLOSIVE || nDamageType == DAMAGE_TYPE_MELEE || (nDamageType == DAMAGE_TYPE_BULLET && bBulletCanKnockDoorOpen)) &&
		m_pParent->GetModelIndex() != MI_TRAILER_TRAILERLARGE )
	{
		for(int nDoor=0; nDoor<m_pParent->GetNumDoors(); nDoor++)
		{
			CCarDoor* pDoor = m_pParent->GetDoor(nDoor);
			bool bCanLoosenLatch = !pDoor->GetFlag(CCarDoor::LOOSE_LATCHED_DOOR);

			if(nDamageType == DAMAGE_TYPE_BULLET)
			{
				// Bullets can only knock the door open when shot directly on it
				if(pDoor->GetFragChild() != nComponent)
				{
					continue;
				}

				// Bullet can only pop doors to loosed latch position, but not knock open them (except boot and bonnet)
				if( pDoor->GetHierarchyId()!=VEH_BOOT && 
					pDoor->GetHierarchyId()!=VEH_BONNET && 
					pDoor->GetHierarchyId()!=VEH_BOOT_2 &&
					!bCanLoosenLatch)
				{
					continue;
				}
			}
			else if(nDamageType == DAMAGE_TYPE_MELEE)
			{
				// Melee strike can only knock the door to loosed latch position
				if(!bCanLoosenLatch)
				{
					continue;
				}

				// Melee strike can not pop the hoods
				if(pDoor->GetHierarchyId()==VEH_BONNET && m_pParent->GetBoneIndex(VEH_BONNET) != -1)
				{
					continue;
				}
			}

			if(	(fDamage > popDoorDamage) || 
				(m_fBodyHealth < 500.0f && fDamage > 15.0f) || 
				(transformCZ < 0.0f && fDamage > 5.0f) ||
				(bCanLoosenLatch && fDamage > loosenLatchDamage) )
			{
				if(pDoor->GetFragChild() > 0 && pBoundComposite->GetBound(pDoor->GetFragChild()) && pDoor->GetFlag(CCarDoor::DRIVEN_SHUT))
				{
					float fDoorSideOffset = pBoundComposite->GetCurrentMatrix(pDoor->GetFragChild()).GetCol3().GetXf();
					if(fDoorSideOffset > 0.5f) fDoorSideOffset = 1.0f;
					else if(fDoorSideOffset < -0.5f) fDoorSideOffset = -1.0f;
					else fDoorSideOffset = 0.0f;

					float fPopDoorSideDamage = popDoorSideDamage;
					float fPopDoorChance = popDoorChance;

					if(bCanLoosenLatch)
					{
						fPopDoorSideDamage = loosenLatchSideDamage;
						fPopDoorChance = loosenLatchChance;
					}

					if(fDoorSideOffset*vecNormLocal.x*fDamage < -popDoorSideDamage && fwRandom::GetRandomNumberInRange(0.0f,1.0f) < popDoorChance)
					{
						if(pDoor->GetDoorAllowedToBeBrokenOff() && !(m_pParent->pHandling && m_pParent->pHandling->hFlags &HF_ARMOURED))
						{
							if(!(NetworkInterface::IsGameInProgress() && 
                                (m_pParent->GetModelIndex() == MI_CAR_BENSON_TRUCK || 
                                 m_pParent->GetModelIndex() == MI_PLANE_TITAN || 
                                 m_pParent->GetModelIndex() == MI_PLANE_AVENGER ||
                                 m_pParent->GetModelIndex() == MI_CAR_INSURGENT2 ) && 
                                ( pDoor->GetHierarchyId() == VEH_BOOT || pDoor->GetHierarchyId() == VEH_BOOT_2 )) )//Benson rear door shouldn't open in MP
							{
								if(bCanLoosenLatch)
								{
									pDoor->SetLooseLatch(m_pParent);
									ResetBrokenPartCountdown();
								}
								else
								{
									pDoor->SetSwingingFree(m_pParent);
									pDoor->SetHasBeenKnockedOpen(true);
									ResetBrokenPartCountdown();
								}
							}
						}
					}
					else
					{
						const Vector3 sVecDoorBoxMinAdd(-0.2f, -0.2f, 0.1f);
						const Vector3 sVecDoorBoxMaxAdd(0.2f, 0.2f, 0.2f);
						const Vector3 sVecDoorBoxMinAddBoot(-0.2f, -0.3f, -0.4f);
						const Vector3 sVecDoorBoxMinAddBonnet(-0.2f, -0.2f, -0.4f);

						float fPopDoorChance = popDoorChance;

						Vector3 vecPosLocalDoor, vecDoorBoxMin, vecDoorBoxMax;
						RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(pDoor->GetFragChild())).UnTransform(vecPosLocal, vecPosLocalDoor);
						vecDoorBoxMax = VEC3V_TO_VECTOR3(pBoundComposite->GetBound(pDoor->GetFragChild())->GetBoundingBoxMax()) + sVecDoorBoxMaxAdd;
						vecDoorBoxMin = VEC3V_TO_VECTOR3(pBoundComposite->GetBound(pDoor->GetFragChild())->GetBoundingBoxMin()) + sVecDoorBoxMinAdd;
						
						if(pDoor->GetHierarchyId()==VEH_BOOT)
						{
							vecDoorBoxMin += sVecDoorBoxMinAddBoot - sVecDoorBoxMinAdd;
						}
						else if(pDoor->GetHierarchyId()==VEH_BONNET)
						{
							vecDoorBoxMin += sVecDoorBoxMinAddBonnet - sVecDoorBoxMinAdd;
						}

						if(bCanLoosenLatch)
						{
							fPopDoorChance = loosenLatchChance;
						}
						else if(pDoor->GetHierarchyId()==VEH_BOOT)
						{
							fPopDoorChance = popBonnetChance;
						}
						else if(pDoor->GetHierarchyId()==VEH_BONNET)
						{
							fPopDoorChance = popBonnetChance;

							// Don't pop open the bonnet from side impacts
							static dev_float sfYNormalPopLimit = 0.6f;
							if(vecNormLocal.x * vecNormLocal.x  > sfYNormalPopLimit * sfYNormalPopLimit)
							{
								fPopDoorChance = 0.0f;
							}
						}
						else if(!pDoor->GetDoorAllowedToBeBrokenOff())
						{
							fPopDoorChance = breakDoorChance;
						}

						if( geomPoints::IsPointInBox(vecPosLocalDoor, vecDoorBoxMin, vecDoorBoxMax) && fwRandom::GetRandomNumberInRange(0.0f,1.0f) < fPopDoorChance)
						{
							if(pDoor->GetDoorAllowedToBeBrokenOff()  && !(m_pParent->pHandling && m_pParent->pHandling->hFlags &HF_ARMOURED))
							{
								if(!(NetworkInterface::IsGameInProgress() && 
                                    (m_pParent->GetModelIndex() == MI_CAR_BENSON_TRUCK || 
                                     m_pParent->GetModelIndex() == MI_PLANE_TITAN || 
                                     m_pParent->GetModelIndex() == MI_PLANE_AVENGER ||
                                     m_pParent->GetModelIndex() == MI_CAR_INSURGENT2 ) && 
                                    ( pDoor->GetHierarchyId() == VEH_BOOT || pDoor->GetHierarchyId() == VEH_BOOT_2 )))//Benson rear door shouldn't open in MP
								{
									if(bCanLoosenLatch)
									{
										pDoor->SetLooseLatch(m_pParent);
										ResetBrokenPartCountdown();
									}
									else 
									{
										pDoor->SetSwingingFree(m_pParent);
										pDoor->SetHasBeenKnockedOpen(true);
										ResetBrokenPartCountdown();
									}
								}
							}
						}
					}
				}

				// Knock off the bonnet if its attachment point is above the front bumper and front bumper has been knocked off or partially detached
				if(pDoor->GetHierarchyId()==VEH_BONNET && m_pParent->GetBoneIndex(VEH_BONNET) != -1 && pDoor->GetFragChild() > 0 && !pFragInst->GetChildBroken(pDoor->GetFragChild()) && pDoor->GetIsIntact(m_pParent) && pDoor->GetDoorAllowedToBeBrokenOff() && !CVehicle::TooManyVehicleBreaksRecently())
				{
					u32 modelNameHash = m_pParent->GetVehicleModelInfo()->GetModelNameHash();

					const Matrix34& doorMatrix = m_pParent->GetLocalMtx(m_pParent->GetBoneIndex(VEH_BONNET));
					if(doorMatrix.d.y > 0.8f * m_pParent->GetBoundingBoxMax().y && 
						modelNameHash != MI_CAR_MONROE.GetName().GetHash() && 
						modelNameHash != MI_CAR_BRAWLER.GetName().GetHash() && 
						modelNameHash != MI_CAR_SENTINEL3.GetName().GetHash() &&
						modelNameHash != MI_CAR_DELUXO.GetName().GetHash() &&
                        modelNameHash != MI_CAR_Z190.GetName().GetHash() &&
                        modelNameHash != MI_CAR_ZION3.GetName().GetHash() ) // Don't knock off bonnet of Monroe, it will pop underneath the car
					{
						if((!pDoor->GetIsLatched(m_pParent) || pDoor->GetFlag(CCarDoor::DRIVEN_SWINGING)) 
							&& (m_BouncingPanels[0].GetCompIndex() == VEH_BUMPER_F || m_pParent->GetBoneIndex(VEH_BUMPER_F) == -1))
						{
							pDoor->BreakOff(m_pParent);
							ResetBrokenPartCountdown();
							continue;
						}
					}
				}
			}
		}
	}

	ms_bDisableVehiclePartCollisionOnBreak = false;

	if( ( CPhysics::ms_bInArenaMode ||
		  isFlameThrowerFire ) &&
		m_fBodyHealth <= 0.0f &&
		!m_pParent->m_nVehicleFlags.bBlownUp &&
		!bIsNetworkCloneOrMigrating)
	{
		m_pParent->BlowUpCar( pInflictor ? pInflictor : m_pParent );
	}
}

void CVehicleDamage::BreakOffMiscComponents(float fDeleteMiscChance, float fBreakMiscChance, const Vector3& vecPosLocal)
{
	fragInst* pFragInst = m_pParent->GetVehicleFragInst();
	Assert(pFragInst);

	phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
	Assert(pBoundComposite);

	int nNumBreakableParts = NUM_MISC_CAR_BREAK_PARTS;
	eHierarchyId *pBreakableParts = aMiscCarBreakParts;

	bool bIsBreakableTrailer = m_pParent->InheritsFromTrailer() && (static_cast<CTrailer*>(m_pParent))->HasBreakableExtras() && 
							   m_pParent->GetAttachParentVehicle() && m_pParent->GetAttachParentVehicle()->GetDriver() && m_pParent->GetAttachParentVehicle()->GetDriver()->IsPlayer();
	if(bIsBreakableTrailer)
	{
		nNumBreakableParts = NUM_EXTRA_TRAILER_BREAK_PARTS;
		pBreakableParts = aExtraTrailerBreakParts;
	}
	
	// go through all the fragment children (there's more of them)
	for(int nChild=0; nChild<pFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
	{
		if(pFragInst->GetChildBroken(nChild))
			continue;

		fragInst* pFragInst = m_pParent->GetVehicleFragInst();

		int boneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragInst->GetTypePhysics()->GetAllChildren()[nChild]->GetBoneID());

		for(int nMisc=0; nMisc<nNumBreakableParts; nMisc++)
		{
            eHierarchyId hierarchyID = static_cast<eHierarchyId>(pBreakableParts[nMisc]);

			// B*1808787: Prevent the rear bumper from being broken off from the trailer to stop cars from being driven and getting wedged in underneath.
			if(m_pParent->InheritsFromTrailer() && hierarchyID == VEH_BUMPER_R)
			{
				continue;
			}

			if( m_pParent->HasRamp() &&
				hierarchyID == VEH_BUMPER_F )
			{
				continue;
			}

			if(m_pParent->GetBoneIndex(hierarchyID) == boneIndex)
			{
				Vector3 vecPosLocalPanel, vecPanelBoxMin, vecPanelBoxMax;
				RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(nChild)).UnTransform(vecPosLocal, vecPosLocalPanel);
				vecPanelBoxMax = VEC3V_TO_VECTOR3(pBoundComposite->GetBound(nChild)->GetBoundingBoxMax());
				vecPanelBoxMin = VEC3V_TO_VECTOR3(pBoundComposite->GetBound(nChild)->GetBoundingBoxMin());

				if( geomPoints::IsPointInBox(vecPosLocalPanel, vecPanelBoxMin, vecPanelBoxMax) || bIsBreakableTrailer )
				{ 
					if(fwRandom::GetRandomNumberInRange(0.0f,1.0f) < fBreakMiscChance)
					{
						if(fwRandom::GetRandomNumberInRange(0.0f,1.0f) < fDeleteMiscChance)
						{
							m_pParent->PartHasBrokenOff(hierarchyID);
							pFragInst->DeleteAbove(nChild);
							ResetBrokenPartCountdown();
							CVehicle::ClearLastBrokenOffPart();
						}
						else if(!CVehicle::TooManyVehicleBreaksRecently())
						{
							m_pParent->PartHasBrokenOff(hierarchyID);
							pFragInst->BreakOffAbove(nChild);
							ResetBrokenPartCountdown();
							CVehicle::ClearLastBrokenOffPart();
						}

                        if(NetworkInterface::IsGameInProgress())
                        {
                            NetworkInterface::OnVehiclePartDamage(m_pParent, hierarchyID, VEH_BB_BROKEN);
                        }
					}
				}
			}
		}
	}
}

u8 CVehicleDamage::GetPartDamage(eHierarchyId ePart)
{
	fragInst* pFragInst = m_pParent->GetVehicleFragInst();
	if (pFragInst)
	{
		int partBoneIndex = m_pParent->GetBoneIndex(ePart);
		for(int nChild=0; nChild<pFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
		{
			int boneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragInst->GetTypePhysics()->GetAllChildren()[nChild]->GetBoneID());
			if (boneIndex == partBoneIndex)
			{
				if (pFragInst->GetChildBroken(nChild))
				{			
					return VEH_BB_BROKEN;
				}
			}
		}

		for (s32 i = 0; i < MAX_BOUNCING_PANELS; ++i)
		{
			CBouncingPanel* panel = GetBouncingPanel(i);
			if (!panel)
				continue;

			if (panel->m_nComponentIndex == ePart)
			{
				return VEH_BB_BOUNCING;
			}
		}
	}

	return VEH_BB_NONE;
}

void CVehicleDamage::SetPartDamage(eHierarchyId ePart, u8 state)
{
	if (state != VEH_BB_NONE)
	{
		fragInst* pFragInst = m_pParent->GetVehicleFragInst();
		if (pFragInst)
		{
			if (state == VEH_BB_BROKEN)
			{
				int partBoneIndex = m_pParent->GetBoneIndex(ePart);
				for(int nChild=0; nChild<pFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
				{
					int boneIndex = pFragInst->GetType()->GetBoneIndexFromID(pFragInst->GetTypePhysics()->GetAllChildren()[nChild]->GetBoneID());
					if (boneIndex == partBoneIndex)
					{
						if (!pFragInst->GetChildBroken(nChild))
						{
							pFragInst->SetBroken(true);
							m_pParent->PartHasBrokenOff(ePart);
							pFragInst->BreakOffAbove(nChild);
							ResetBrokenPartCountdown();
							CVehicle::ClearLastBrokenOffPart();
							return;
						}
					}
				}
			}
			else if (state == VEH_BB_BOUNCING)
			{
				bool bFoundPanel = false;
				bool bFoundEmptyPanel = false;
				int pos = -1;
				for (s32 i = 0; i < MAX_BOUNCING_PANELS; ++i)
				{
					CBouncingPanel* panel = GetBouncingPanel(i);
					if (!panel)
						continue;

					if (!bFoundPanel && panel->m_nComponentIndex == ePart)
						bFoundPanel = true;

					if (!bFoundEmptyPanel && panel->m_nComponentIndex == -1)
					{
						bFoundEmptyPanel = true;
						pos = i;
					}
				}

				//If the bouncing panel wasn't found, then it needs to be set on the remote in order for the bumper to hang off and look funky there also.
				if (!bFoundPanel && bFoundEmptyPanel)
				{
					m_BouncingPanels[pos].SetPanel(ePart, CBouncingPanel::BOUNCE_AXIS_X, fwRandom::GetRandomNumberInRange(0.2f, 0.5f));
					return;
				}
			}
		}

        if(NetworkInterface::IsGameInProgress())
        {
            NetworkInterface::OnVehiclePartDamage(m_pParent, ePart, state);
        }
	}
}

#if __BANK

void CVehicleDamage::SetupVehicleDoorDamageBank(bkBank& bank)
{
	bank.PushGroup("Door Damage Tweaking", false);
		bank.AddSlider("Pop door min damage",		&sfPopDoorMinDamage, 0.0f, 100.0f, 1.0f );
		bank.AddSlider("Pop door max damage",		&sfPopDoorMaxDamage, 0.0f, 500.0f, 1.0f );
		bank.AddSlider("Pop door side damage",		&sfPopDoorSideDamage, 0.0f, 100.0f, 1.0f );
		bank.AddSlider("Loosen latch damage",		&sfPopDoorChance, 0.0f, 100.0f, 1.0f );
		bank.AddSlider("Loosen latch side damage",	&sfLoosenLatchSideDamage, 0.0f, 100.0f, 1.0f );

		bank.AddSlider("Pop door chance",			&sfPopDoorChance, 0.0f, 1.0f, 0.01f );
		bank.AddSlider("Loosen latch chance",		&sfLoosenLatchChance, 0.0f, 1.0f, 0.01f );
		bank.AddSlider("Break door chance",			&sfBreakDoorChance, 0.0f, 1.0f, 0.01f );
		bank.AddSlider("Pop bonnet chance",			&sfPopBonnetChance, 0.0f, 1.0f, 0.01f );
		bank.AddSlider("Pop wind screen chance",	&sfPopWindScreenChance, 0.0f, 1.0f, 0.01f );
		bank.AddSlider("Break misc chance",			&sfBreakMiscChance, 0.0f, 1.0f, 0.01f );

		bank.AddAngle("Loose latched door open angle",				&ms_fLooseLatchedDoorOpenAngle, bkAngleType::RADIANS, 0.0, PI);
		bank.AddAngle("Loose latched bonnet open angle",				&ms_fLooseLatchedBonnetOpenAngle, bkAngleType::RADIANS, 0.0, PI);
	bank.PopGroup();
}

#endif

int CVehicleDamage::GetNumOfBrokenOffParts() const
{
	int iNumOfBrokenOffDoors = 0;

	fragInst* pFragInst = m_pParent->GetVehicleFragInst();

	Assert(pFragInst);
	if(pFragInst == NULL)
		return 0;

	for(int nChild=0; nChild<pFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
	{
		if(pFragInst->GetChildBroken(nChild))
		{
			iNumOfBrokenOffDoors++;
		}
	}

	return iNumOfBrokenOffDoors;
}

int CVehicleDamage::GetNumOfBrokenLoosenParts() const
{
	int iNumOfBrokenLoosenParts = 0;

	fragInst* pFragInst = m_pParent->GetVehicleFragInst();
	Assert(pFragInst);
	if(pFragInst == NULL)
		return 0;

	// count the number of doors or bonnets that are knocked open
	for(int nDoor=0; nDoor<m_pParent->GetNumDoors(); nDoor++)
	{
		CCarDoor* pDoor = m_pParent->GetDoor(nDoor);
		if(pDoor->GetFragChild() > 0 && !pFragInst->GetChildBroken(pDoor->GetFragChild()))
		{
			if(pDoor->GetEverKnockedOpen())
			{
				iNumOfBrokenLoosenParts++;
			}
		}
	}

	// count the number of bumpers that are broken loose
	for (s32 i = 0; i < MAX_BOUNCING_PANELS; ++i)
	{
		const CBouncingPanel &panel = m_BouncingPanels[i];
		if (panel.m_nComponentIndex == VEH_BUMPER_R || panel.m_nComponentIndex == VEH_BUMPER_F)
		{
			iNumOfBrokenLoosenParts++;
		}
	}

	return iNumOfBrokenLoosenParts;
}

float sfEngineMinDistance = 0.3f;
dev_float sfEngineMinDistanceSq = 0.3f * 0.3f;
bool CVehicleDamage::AvoidVehicleExplosionChainReactions()
{
#if __BANK
	if(ms_bNeverAvoidVehicleExplosionChainReactions)
	{
		return false;
	}
	else if(ms_bAlwaysAvoidVehicleExplosionChainReactions)
	{
		return true;
	}
#endif // __BANK

	// Disable chain reactions when the player is very wanted.

	// GTAV - B*1724681 - Increase explosion chain reactions on next gen
	//if(!NetworkInterface::IsGameInProgress())
	//{
	//	if(CWanted* pWanted = CGameWorld::FindLocalPlayerWanted())
	//	{
	//		return pWanted->GetWantedLevel() >= WANTED_LEVEL4;
	//	}
	//}
	//else
	//{
	//	return CNetObjPlayer::GetHighestWantedLevelInArea() >= WANTED_LEVEL4;
	//}	

	return CVehicle::sm_bDisableExplosionDamage;
}

float CVehicleDamage::GetVehicleExplosionBreakChanceMultiplier()
{
#if __BANK
	if(ms_bUseVehicleExplosionBreakChanceMultiplierOverride)
	{
		return ms_fVehicleExplosionBreakChanceMultiplierOverride;
	}
#endif // __BANK

	//This is set through script. Will be reset to false at the end of physics
	if(ms_bDisableVehicleExplosionBreakOffParts)
	{
		return 0.0f;
	}

	if(NetworkInterface::IsGameInProgress())
	{
		eWantedLevel wantedLevel = CNetObjPlayer::GetHighestWantedLevelInArea();
		if(wantedLevel >= WANTED_LEVEL4)
		{
			return 0.0f;
		}
		else if (wantedLevel >= WANTED_LEVEL3)
		{
			return 0.5f;
		}
	}

	return 1.0f;
}

dev_float sfTankEngineRearDamageMulti = 2.0f;
//
void CVehicleDamage::ApplyDamageToEngine(CEntity* pInflictor, eDamageType nDamageType, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, const Vector3& vecDirnLocal, const bool bFireDriveby, const bool bIsAccurate, const float fDamageRadius, const bool bAvoidExplosions, u32 nWeaponHash, const bool bChainExplosion)
{
	// can't apply damage to network clones 
	if(m_pParent->IsNetworkClone())
	{
		//Need to update entity that set us on fire in case the vehicle changes ownership
		if (fDamage > 0.0f && pInflictor)
		{
			m_pEntityThatSetUsOnFire = pInflictor;
		}

		return;
	}

	bool bInflictorIsAPed		= pInflictor && pInflictor->GetIsTypePed();
	bool bInflictorIsAVehicle	= pInflictor && pInflictor->GetIsTypeVehicle();

	/*
	// let the plane engine overheat and the fuel tank explode
	if(m_pParent->InheritsFromPlane())
	{
		// Planes handle engine and petrol tank damage themselves
		return;
	}
	*/

	if(m_pParent->InheritsFromTrailer())
	{
		if (!m_pParent->SprayPetrolBeforeExplosion() && !m_pParent->ShouldExplodeOnContact())
		{
			// Trailers have no petrol tank or engine - unless it's a tanker...
			return;
		}
	}

	//B*1721702/1732210: Don't apply rear damage multiplier for explosives in MP
	if(m_pParent->IsTank() && ((nDamageType == DAMAGE_TYPE_EXPLOSIVE && !NetworkInterface::IsGameInProgress()) || nDamageType == DAMAGE_TYPE_BULLET))
	{
		if(vecPosLocal.y < m_pParent->GetBoundingBoxMin().y * 0.5f)
		{
			fDamage *= sfTankEngineRearDamageMulti;
		}
	}

	// calc if damage should be applied to engine
	{
		bool bDamageEngine = false;

		if( nDamageType == DAMAGE_TYPE_EXPLOSIVE || (m_pParent->m_nVehicleFlags.bForceEngineDamageByBullet && nDamageType == DAMAGE_TYPE_BULLET))
		{
			bDamageEngine = true;
		}
		else if(nDamageType == DAMAGE_TYPE_MELEE && GetEngineHealth() <= ENGINE_DAMAGE_CAP_BY_MELEE)
		{
			// No more melee damage if engine health reaches the cap
		}
		else
		{
			Vector3 VecEnginePosLocal = VEC3_ZERO;
			if(m_pParent->GetBoneIndex(VEH_ENGINE)>-1)
			{
				Matrix34 matEngineLocal = m_pParent->GetLocalMtx(m_pParent->GetBoneIndex(VEH_ENGINE));
				VecEnginePosLocal = matEngineLocal.d;
			}

			//Make sure the Z is reasonable before checking if the Y is around the engine
			const float fDistZSq = square(vecPosLocal.z - VecEnginePosLocal.z);
			if(nDamageType == DAMAGE_TYPE_FIRE || nDamageType == DAMAGE_TYPE_EXPLOSIVE || fDistZSq < sfEngineMinDistanceSq)
			{
				if(VecEnginePosLocal.y > 0.0f)
				{
					if(vecPosLocal.y > VecEnginePosLocal.y - sfEngineMinDistance && vecPosLocal.y < m_pParent->GetBoundingBoxMax().y + sfEngineMinDistance)
						bDamageEngine = true;
				}
				else
				{
					if(vecPosLocal.y < VecEnginePosLocal.y + sfEngineMinDistance && vecPosLocal.y > m_pParent->GetBoundingBoxMin().y - sfEngineMinDistance)
						bDamageEngine = true;
				}
			}

			if(bFireDriveby && vecPosLocal.y > 0.0f && fwRandom::GetRandomNumberInRange(0.0f,1.0f) > sfWeaponDrivebyForceDamageFrac)
				bDamageEngine = true;

            // Damage the engine if we have had a reasonably large collision when upside down, simulating the shock on the engine.
            if(m_pParent->IsUpsideDown() && fDamage > 5.0f)
            {
                bDamageEngine = true;
            }

			// Inflict half of the damage to engine when large bodies are hit
			if(!bDamageEngine && (m_pParent->InheritsFromPlane() || m_pParent->InheritsFromHeli() || m_pParent->InheritsFromSubmarine() || m_pParent->IsTank()))
			{
				bDamageEngine = true;
				float fDistanceMult = 1.0f - (vecPosLocal.Dist(VecEnginePosLocal) / m_pParent->GetBoundRadius());
				fDistanceMult = Clamp(fDistanceMult, 0.1f, 0.5f);
				fDamage *= fDistanceMult;
			}
		}

		// If a weapon flagged ApplyVehicleDamageToEngine doesn't hit the engine, it should still apply 33% of it's damage to the engine
		if (!bDamageEngine && (bInflictorIsAPed || bInflictorIsAVehicle))
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
			if (pWeaponInfo && pWeaponInfo->GetApplyVehicleDamageToEngine())
			{
				bDamageEngine = true;
				fDamage = 0.33f * fDamage;
			}
		}

		if(bDamageEngine)
		{
			float damageMult = 1.0f;
			if( nDamageType == DAMAGE_TYPE_COLLISION )
			{
				damageMult *= m_pParent->pHandling->m_fEngineDamageMult;
			}

            // Increase the damage done to the engine when upside down
            if(m_pParent->IsUpsideDown())
            {
                damageMult *= sfUpsideDownEngineDamageMult;
            }
			
			float damage = fDamage * damageMult;

			// Scale down the damage to engines by melee hits.
			if(nDamageType == DAMAGE_TYPE_MELEE)
			{
				damage *= 0.3f;
			}

			if(m_pParent->InheritsFromPlane())
			{
				damage = ((CPlane *)m_pParent)->GetAircraftDamage().ApplyDamageToEngine(pInflictor, ((CPlane *)m_pParent), damage, nDamageType, vecPosLocal, vecNormLocal);
			}

			float currentEngineHealth = m_pParent->m_Transmission.GetEngineHealth();
			if(bAvoidExplosions && currentEngineHealth >= ENGINE_DAMAGE_ONFIRE)
			{
				float maximumAllowedDamage = currentEngineHealth - Max(currentEngineHealth-fDamage,ENGINE_DAMAGE_ONFIRE + 0.1f);
				damage = Max(maximumAllowedDamage,0.0f);
			}

			m_pParent->m_Transmission.ApplyEngineDamage(m_pParent, pInflictor, nDamageType, damage, true);
		}
	}

	if(nDamageType == DAMAGE_TYPE_BULLET && !bIsAccurate && fwRandom::GetRandomNumberInRange(0.0f,1.0f) < m_pParent->GetHealth()/VEH_DAMAGE_HEALTH_STD)
	{
		// Only accurate shots can damage petrol tank
		return;
	}

	bool bForceDamageToPetrolTank = false;

	if(nDamageType==DAMAGE_TYPE_FIRE && GetEngineHealth()<=0)
	{
		bForceDamageToPetrolTank = true;
	}
	else if(nDamageType == DAMAGE_TYPE_EXPLOSIVE)
	{
		Vector3 vecPetrolTankTestPos;
		FindClosestPetrolTankPos(vecPosLocal, &vecPetrolTankTestPos);

		if(vecPetrolTankTestPos.Dist2(vecPosLocal) > fDamageRadius*fDamageRadius)
		{
			bForceDamageToPetrolTank = true;
		}

		// Allow certain explosion types to ignore the distance check to petrol tank and always force damage
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
		if (pWeaponInfo)
		{
			const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
			if (pAmmoInfo && pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
			{
				const CAmmoProjectileInfo* pAmmoProjectileInfo = static_cast<const CAmmoProjectileInfo*>(pAmmoInfo);
				if (pAmmoProjectileInfo && pAmmoProjectileInfo->GetExplosionTag() != EXP_TAG_DONTCARE)
				{
					CExplosionTagData& explosionTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pAmmoProjectileInfo->GetExplosionTag());
					if (explosionTagData.bForcePetrolTankDamage)
					{
						bForceDamageToPetrolTank = true;
					}
				}
			}
		}
	}
	else if((m_pParent->pHandling->hFlags &HF_ARMOURED)==0)
	{
		Vector3 vecPetrolTankTestPos;
		FindClosestPetrolTankPos(vecPosLocal, &vecPetrolTankTestPos);

		if(vecPetrolTankTestPos.IsNonZero())
		{
			bool bDamagedByMonsterTruckCollision = false;

			if( nDamageType == DAMAGE_TYPE_COLLISION &&
				bInflictorIsAVehicle )
			{
				if( m_pParent->pHandling->hFlags & HF_REDUCED_DRIVE_OVER_DAMAGE )
				{
					bDamagedByMonsterTruckCollision = true;
				}
			}

			if( ( nDamageType == DAMAGE_TYPE_BULLET || nDamageType == DAMAGE_TYPE_COLLISION ) &&
				!bDamagedByMonsterTruckCollision )
			{
				Vector3 vecTankBoxHalfLimits(m_pParent->GetBoundingBoxMax().x * 0.5f, 0.5f, 0.5f);

				// GTAV - B*1894041 - the above values are much to big for a motorbike fuel tank
				if( m_pParent->GetVehicleType() == VEHICLE_TYPE_BIKE )
				{
					vecTankBoxHalfLimits.SetY( 0.25f );
					vecTankBoxHalfLimits.SetZ( 0.1f );
				}

				if( m_pParent->GetModelIndex() == MI_CAR_SCRAMJET.GetModelIndex() )
				{
					vecTankBoxHalfLimits *= 0.25f;
				}

				// GTAV - B*2787979 - reduce the size of the tugboat fuel tank so it doesn't explode to easily
				if( m_pParent->GetVehicleType() == VEHICLE_TYPE_BOAT &&
					( MI_BOAT_TUG.IsValid() && ( m_pParent->GetModelIndex() == MI_BOAT_TUG.GetModelIndex() ) ) )
				{
					vecTankBoxHalfLimits.SetX( 0.25f );
				}

				if( bInflictorIsAPed && ((CPed*)pInflictor)->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
				{
					vecTankBoxHalfLimits.z += 0.2f;
				}

				DEV_ONLY(Color32 hitPetrolTankCol = Color_green);
				Vector3 vTankBoxMin = vecPetrolTankTestPos - vecTankBoxHalfLimits;
				Vector3 vTankBoxMax = vecPetrolTankTestPos + vecTankBoxHalfLimits;
				if(	vecPosLocal.x >= vTankBoxMin.x && vecPosLocal.x <= vTankBoxMax.x && 
					vecPosLocal.y >= vTankBoxMin.y && vecPosLocal.y <= vTankBoxMax.y && 
					vecPosLocal.z >= vTankBoxMin.z && vecPosLocal.z <= vTankBoxMax.z)
				{
					if(nDamageType == DAMAGE_TYPE_BULLET || m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_LEAKING)
					{
						bForceDamageToPetrolTank = true;
						DEV_ONLY(hitPetrolTankCol = Color_orange);
					}
				}

				if(!bForceDamageToPetrolTank)
				{
					if(nDamageType == DAMAGE_TYPE_BULLET)
					{
						Vector3 vecStart = vecPosLocal - vecPetrolTankTestPos;
						// Move the start back a bit along the direction
						float fProbeLength = Dot( vecTankBoxHalfLimits, vecDirnLocal ) * 0.5f + 0.15f;
						Vector3 vecStartToEnd =  vecDirnLocal * -fProbeLength;
						Vec3V boxHalfWidths = VECTOR3_TO_VEC3V( vecTankBoxHalfLimits ); //Scale( Subtract( VECTOR3_TO_VEC3V(vecPetrolTankTestPos + vecTankBoxHalfLimits), VECTOR3_TO_VEC3V(vecPetrolTankTestPos - vecTankBoxHalfLimits)), ScalarV(V_HALF));

						if(geomBoxes::TestSegmentToBox(RCC_VEC3V(vecStart), RCC_VEC3V(vecStartToEnd), boxHalfWidths, NULL, NULL, NULL, NULL, NULL, NULL))
						{
							bForceDamageToPetrolTank = true;
							DEV_ONLY(hitPetrolTankCol = Color_orange);
						}

#if __DEV
						if(gbRenderPetrolTankDamage)
						{
							Vector3 vecWorldStart = VEC3V_TO_VECTOR3(m_pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(vecStart + vecPetrolTankTestPos)));
							Vector3 vecWorldEnd = vecWorldStart + VEC3V_TO_VECTOR3(m_pParent->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vecStartToEnd)));
							grcDebugDraw::Sphere(vecWorldStart, 0.1f, Color_red, true, -30);
							grcDebugDraw::Sphere(vecWorldEnd, 0.1f, Color_green, true, -30);
							grcDebugDraw::Line(vecWorldStart, vecWorldEnd, Color_red, Color_green, -30);
							grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vecPetrolTankTestPos - vecTankBoxHalfLimits), VECTOR3_TO_VEC3V(vecPetrolTankTestPos + vecTankBoxHalfLimits), m_pParent->GetMatrix(), hitPetrolTankCol, false, -30);
						}
#endif // __DEV

					}
					else if(nDamageType == DAMAGE_TYPE_COLLISION && m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_LEAKING)
					{
						if(m_pParent->GetVehicleType() == VEHICLE_TYPE_CAR)
						{
							CAutomobile* pAutomobile = static_cast< CAutomobile* >( m_pParent );

							dev_float collisionDamageWhenUsingHydraulicsThreshold = 5.0f;

							// don't apply fuel tank damage caused by cars using hydraulics
							if( ( pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising ||
								  pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding ) &&
								fDamage < collisionDamageWhenUsingHydraulicsThreshold )
							{
								return;	
							}
						}

						// Inflict some of the damage to petrol tank, requested from B*1218172
						bForceDamageToPetrolTank = true;
						float fDistanceMult = 1.0f - (vecPosLocal.Dist(vecPetrolTankTestPos) / m_pParent->GetBoundRadius());
						fDistanceMult = Clamp(fDistanceMult, 0.1f, 0.5f);
						fDamage *= fDistanceMult;

						// B*1569654 - reduce the amount of damage that planes take to the petrol tank from collision damage
						if( m_pParent->InheritsFromPlane() && 
							( m_pParent->GetModelIndex() == MI_PLANE_TITAN ||
							  m_pParent->GetModelIndex() == MI_PLANE_BOMBUSHKA || 
							  m_pParent->GetModelIndex() == MI_PLANE_VOLATOL) )
						{
							fDamage *= sfPlaneCollisionWithCarMult;
						}

						fDamage = Min(fDamage, m_fPetrolTankHealth - VEH_DAMAGE_HEALTH_PTANK_LEAKING);

#if __DEV
						if(gbRenderPetrolTankDamage)
						{
							Vector3 vecHitPos = VEC3V_TO_VECTOR3(m_pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(vecPosLocal)));
							grcDebugDraw::Sphere(vecHitPos, 0.1f, Color_red, true, -30);
							grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vecPetrolTankTestPos - vecTankBoxHalfLimits), VECTOR3_TO_VEC3V(vecPetrolTankTestPos + vecTankBoxHalfLimits), m_pParent->GetMatrix(), hitPetrolTankCol, false, -30);
						}
#endif // __DEV
					}
				}
#if __DEV
				else if(gbRenderPetrolTankDamage)
				{
					Vector3 vecHitPos = VEC3V_TO_VECTOR3(m_pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(vecPosLocal)));
					grcDebugDraw::Sphere(vecHitPos, 0.1f, Color_red, true, -30);
					grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vecPetrolTankTestPos - vecTankBoxHalfLimits), VECTOR3_TO_VEC3V(vecPetrolTankTestPos + vecTankBoxHalfLimits), m_pParent->GetMatrix(), hitPetrolTankCol, false, -30);
				}
#endif // __DEV
			}
			else if (nDamageType == DAMAGE_TYPE_FIRE)
			{
				// check if the fire is close enough to the petrol tank
				Vec3V vPetrolTankPosLcl = VECTOR3_TO_VEC3V(vecPetrolTankTestPos);
				Vec3V vFirePosLcl = VECTOR3_TO_VEC3V(vecPosLocal) + Vec3V(0.0f, 0.0f, 0.25f);
				Vec3V vPetrolTankToFire = vFirePosLcl-vPetrolTankPosLcl;
				if (Mag(vPetrolTankToFire).Getf()<1.0f)
				{
					// now check that the fire is within the vehicle bound
					spdAABB aabbVehicleLcl;
					aabbVehicleLcl = m_pParent->GetLocalSpaceBoundBox(aabbVehicleLcl);
					spdSphere sphereLcl(vFirePosLcl, ScalarVFromF32(0.2f));
					if (aabbVehicleLcl.IntersectsSphere(sphereLcl))
					{
						bForceDamageToPetrolTank = true;
					}
				}
			}
		}

		if(bFireDriveby && vecPosLocal.y < 0.0f && fwRandom::GetRandomNumberInRange(0.0f,1.0f) > sfWeaponDrivebyForceDamageFrac)
		{
			bForceDamageToPetrolTank = true; 
		}
	}

	if( bForceDamageToPetrolTank )
	{
		if(m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_FINISHED && !m_pParent->m_nVehicleFlags.bDisablePetrolTankDamage)
		{
			float fApplyDamageToTank = fDamage;

			bool bVehicleSkipsDamageScaling = (m_pParent->InheritsFromHeli() && ((CHeli *)m_pParent)->GetIsCargobob()) || (m_pParent->IsTank() && NetworkInterface::IsGameInProgress())
														  || (m_pParent->GetVehicleModelInfo() && m_pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_CAPPED_EXPLOSION_DAMAGE));

			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
			bool bWeaponSkipsDamageScaling = pWeaponInfo && pWeaponInfo->GetShouldSkipVehiclePetrolTankDamage(); // Homing Launcher, Valkyrie Explosive Cannon

			// HACK - B*2828467 - This flag is an easy way to identify Contraband vehicles, where we need to always force scaling for script balancing. Seperate flag would be cleaner...
			bool bVehicleForcesDamageScaling = !m_pParent->GetUseMPDamageMultiplierForPlayerVehicle();

			// Add reduction multiplier for Full Metal Jacket ammunition
			if(pInflictor && pInflictor->GetIsTypePed())
			{
				const CPed* pInflictorPed = static_cast<const CPed*>(pInflictor);
				if (pInflictorPed && pWeaponInfo)
				{
					const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(pInflictorPed);
					if (pAmmoInfo && pAmmoInfo->GetIsFMJ())
					{
						fApplyDamageToTank *= sfSpecialAmmoFMJDamageMultiplierAgainstVehicleGasTank;
					}
				}
			}
			
			if( (bVehicleSkipsDamageScaling || bWeaponSkipsDamageScaling) && !bVehicleForcesDamageScaling)
			{
				// Don't scale the petrol tank damage to Cargobob, as we want it to survive through a rocket explosion - B*1183762
				// Don't scale the petrol tank damage to tanks in MP - B*1721702/1732210
				// Don't scale the petrol tank damage of Homing Launcher or Valkyrie Explosive Cannon, as this is tuned for script without gas tank multipliers - B*2122525
			}
			else if( bInflictorIsAPed && ((CPed*)pInflictor)->IsPlayer() )
			{
				fApplyDamageToTank *= sfVehDamPetrolTankApplyGunDamageMult;	
			}
			else if( bVehicleForcesDamageScaling && bInflictorIsAVehicle && ((CVehicle*)pInflictor)->GetDriver() && ((CVehicle*)pInflictor)->GetDriver()->IsPlayer() )
			{
				// HACK - B*2828467 - We need to force scaling against Contraband vehicles for script balancing when attacking with vehicle explosives.
				fApplyDamageToTank *= sfVehDamPetrolTankApplyGunDamageMult;	
			}
			else
			{
				fApplyDamageToTank *= sfVehDamPetrolTankApplyGunDamageMultAI;
			}

            if (nDamageType == DAMAGE_TYPE_FIRE)
            {
                fApplyDamageToTank *= sfVehDamPetrolTankApplyFireDamageMult;
            }

			if(bAvoidExplosions && m_fPetrolTankHealth > VEH_DAMAGE_HEALTH_PTANK_LEAKING)
			{
				// Don't make the petrol tank start leaking if the user wants to avoid explosions.
				m_fPetrolTankHealth = Max(m_fPetrolTankHealth - ( fApplyDamageToTank * m_fPetrolTankDamageScale ),VEH_DAMAGE_HEALTH_PTANK_LEAKING + 0.1f);
			}
			else
			{
				if(m_fPetrolTankHealth - fApplyDamageToTank < VEH_DAMAGE_HEALTH_PTANK_ONFIRE)
				{
					if (nDamageType == DAMAGE_TYPE_BULLET || (nDamageType == DAMAGE_TYPE_EXPLOSIVE && !bChainExplosion))
					{
						// want to do the explosions one single place, so set very low health so it'll happen next frame.
						m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_PTANK_FINISHED + 0.1f;
						m_pEntityThatSetUsOnFire = pInflictor;
					}
					else 
					{
						// set vehicle on fire with a fixed amount of health, so it will blow up after a predictable about of time
						SetPetrolTankOnFire(pInflictor);
					}
				}
				else
				{
					m_fPetrolTankHealth -= fApplyDamageToTank * m_fPetrolTankDamageScale;

					// want to do the explosions one single place, so set very low health so it'll happen next frame.
					if(m_fPetrolTankHealth <= VEH_DAMAGE_HEALTH_PTANK_FINISHED)
					{
						m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_PTANK_FINISHED + 0.1f;
						m_pEntityThatSetUsOnFire = pInflictor;
					}
				}
			}
		}
	}
}

void CVehicleDamage::UpdateLightsOnBreakOff(int nComponentID)
{
	if (NetworkInterface::IsNetworkOpen() && !m_pParent->m_bAllowRemoteDamageOnCreation && m_pParent->IsNetworkClone())
		return;

	// Lookup bone index from child
	Assert(m_pParent->GetVehicleFragInst());
	Assert(m_pParent->GetVehicleFragInst()->GetTypePhysics()->GetNumChildren() > nComponentID);

	fragInstGta* fragInst = m_pParent->GetVehicleFragInst();
	int boneIdx = fragInst->GetType()->GetBoneIndexFromID(fragInst->GetTypePhysics()->GetAllChildren()[nComponentID]->GetBoneID());

	const bool isTaxi = CVehicle::IsTaxiModelId(m_pParent->GetModelId());
	int lightCount = isTaxi ? NUM_GLASS_BONES : NUM_LIGHT_BONES;

	if(boneIdx > -1)
	{
		const crSkeleton *skel = m_pParent->GetSkeleton();
		u32 terminalIdx = skel->GetTerminatingPartialBone(boneIdx);

		for (u32 i=boneIdx; i<terminalIdx; i++)
		{
	    	for(int j=0; j<lightCount; j++)
			{
				const u32 lightBoneIdx = (u32)m_pParent->GetBoneIndex(aGlassBones[j]);
				if( i == lightBoneIdx )
				{
					const int lightIdx = ( j > TAXI_IDX ) ? TAXI_IDX : j;
					SetLightStateImmediate(lightIdx,true);
					SetUpdateLightBones(true);
				}
			}
		}
	}
}


void  CVehicleDamage::SetPetrolTankOnFire(CEntity* pEntityResponsible)
{
	if (m_pParent->m_nVehicleFlags.bDisablePetrolTankFires==false)
	{
		if(m_fPetrolTankHealth >= VEH_DAMAGE_HEALTH_PTANK_ONFIRE)
		{
			m_fPetrolTankHealth = VEH_DAMAGE_HEALTH_PTANK_ONFIRE - 1.0f;

			m_pEntityThatSetUsOnFire = pEntityResponsible;
		}
	}
}

dev_float sfStuckDistSqr = 2.0f*2.0f;
dev_float sfStuckDistBoatSqr = 1.0f;
dev_float sfStuckJammedDistSqr = 3.0f*3.0f;
dev_float sfStuckJammedDistBoatSqr = 1.0f;
//
void CVehicleDamage::ProcessStuckCheck(float fTimeStep)
{
	Vec3f vParentPosition;
	Vec::V3LoadAsScalar(vParentPosition.GetIntrinRef(), m_pParent->GetMatrixRef().GetCol3ConstRef().GetIntrin128ConstRef());

	Vec3f vecPosDelta;
	vecPosDelta = vParentPosition - RCC_VEC3F(m_vecLastStuckPos);

	bool isWatercraft = m_pParent->GetIsAquatic();

	// we don't treat the submarine car as aquatic if it isn't in the water.
	if( isWatercraft &&
		( !m_pParent->GetIsInWater() &&
		  ( m_pParent->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR ||
		    m_pParent->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE ||
		    m_pParent->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ) ) )
	{
		isWatercraft = false;
	}

	if( !isWatercraft &&
		m_pParent->GetIsInWater() &&
		m_pParent->pHandling->GetSeaPlaneHandlingData() )
	{
		isWatercraft = true;
	}

	bool isPlane = m_pParent->GetVehicleType()==VEHICLE_TYPE_PLANE;
	bool isAbandonedBike = m_pParent->InheritsFromBike() && m_pParent->GetStatus() == STATUS_ABANDONED;

	float fDistLimitSqr = sfStuckDistSqr;
	// boats require bigger distance check because they tend to move around on the water
	if(isWatercraft)
	{
		fDistLimitSqr = sfStuckDistBoatSqr;
		vecPosDelta.SetZ( vecPosDelta.GetZ() * 0.2f);
	}

	u32 timeStepMs = (u32)(fTimeStep*1000.0f);

	u32 newStuckCounterOnRoof = m_nStuckCounterOnRoof;
	u32 newStuckCounterOnSide = m_nStuckCounterOnSide;
	u32 newStuckCounterHungUp = m_nStuckCounterHungUp;

	const CCollisionHistory* pCollisionHistory = m_pParent->GetFrameCollisionHistory();
	if(MagSquared(vecPosDelta) < fDistLimitSqr)
	{

		const float fParentCz = m_pParent->GetMatrixRef().GetCol2ConstRef().GetZf();
		// On roof check
		if(fParentCz < -0.3f)
		{
			if(newStuckCounterOnRoof + timeStepMs < BIT(16) - 1)
				newStuckCounterOnRoof = (u16)(newStuckCounterOnRoof + timeStepMs);
			else
				newStuckCounterOnRoof = (u16)(BIT(16) - 1);
		}
		else
			newStuckCounterOnRoof = 0;


		// On side check
		if(Abs(fParentCz) < 0.5f && m_pParent->GetNumContactWheels() < m_pParent->GetNumWheels() && !isAbandonedBike)
		{
			if(newStuckCounterOnSide + timeStepMs < BIT(16) - 1)
				newStuckCounterOnSide = (u16)(newStuckCounterOnSide + timeStepMs);
			else
				newStuckCounterOnSide = (u16)(BIT(16) - 1);
		}
		else
			newStuckCounterOnSide = 0;

		// Hung up check (no drive wheels on ground)
		if(!m_pParent->GetIsAnyFixedFlagSet() && (!isAbandonedBike || m_pParent->m_nVehicleFlags.bVehicleInaccesible))
		{
			bool bHungUp = false;
			if(m_pParent->GetCollider() && isWatercraft)
			{
				CSubmarineHandling* pSubHandling = m_pParent->GetSubHandling();

				if(m_pParent->GetVehicleType()==VEHICLE_TYPE_BOAT)
				{
					// Boats are 'hung up' if they aren't in water or if they are in water but their propeller isn't.
					CBoat* pBoat = static_cast<CBoat*>(m_pParent);
					bool bBoatInWater =  pBoat->m_BoatHandling.IsInWater();
					bool bPropellerInWater = pBoat->m_BoatHandling.GetPropellerSubmerged() == 1;
					bHungUp = (pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() > 0.0f && (!bBoatInWater || !bPropellerInWater));
				}
				else if( pSubHandling )
				{
					bool bSubInWater = m_pParent->GetIsInWater();
					bool bPropellerInWater = pSubHandling->GetIsPropellerSubmerged();
					bHungUp = (pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() > 0.0f && (!bSubInWater || !bPropellerInWater));
				}
			}
			else if(m_pParent->InheritsFromHeli() ||
					( m_pParent->InheritsFromPlane() &&
					  static_cast< CPlane* >( m_pParent )->GetVerticalFlightModeAvaliable() ) )
			{
				// The code further down was used for helicopters, and when flying and the wheels weren't
				// touching, they were considered hung up. There is probably no case where that check would really
				// be valid for a helicopter, so now we never consider them hung up. Feels like there are probably
				// other types of vehicles where that check isn't accurate.
				bHungUp = false;
			}
			else if(m_pParent->m_nVehicleFlags.bVehicleInaccesible)
			{
				// For bicycles and other open-seat vehicles marked 'inaccessible' by the enter vehicle task, we don't care if the wheels are touching the ground or not.
				bHungUp = true;
			}
			else
			{
				if( m_pParent->GetSpecialFlightModeRatio() == 1.0f )
				{
					bHungUp = false;
				}
				else
				{
					bHungUp = true;
					int nNumWheels = m_pParent->GetNumWheels();
					const CWheel * const * ppWheels = m_pParent->GetWheels();
					for(int i=0; i<nNumWheels; i++)
					{
						if(ppWheels[i]->GetConfigFlags().IsFlagSet(WCF_POWERED) && (m_pParent->GetWheel(i)->GetIsTouching() || m_pParent->GetWheel(i)->GetWasTouching()) )
						{
							bHungUp = false;
							break;
						}
					}

					if( bHungUp && 
						m_pParent->InheritsFromAmphibiousQuadBike() )
					{
						CAmphibiousQuadBike* amphibiousQuadBike = static_cast< CAmphibiousQuadBike* >( m_pParent );
						if( !amphibiousQuadBike->IsWheelsFullyOut() )
						{
							bHungUp = false;
						}
					}
				}
			}

			// Boats using low LOD anchor mode can't get stuck.
			if( ( m_pParent->GetVehicleType()==VEHICLE_TYPE_BOAT && static_cast<CBoat*>(m_pParent)->GetAnchorHelper().UsingLowLodMode() ) ||
				( m_pParent->InheritsFromAmphibiousAutomobile() && static_cast<CAmphibiousAutomobile*>(m_pParent)->GetAnchorHelper().UsingLowLodMode() ) )
			{
				bHungUp = false;
			}

			if( m_pParent->IsInSubmarineMode() )
			{
				bHungUp = false;
			}

			//Can't be hung up if no collision is loaded
			if(bHungUp && isWatercraft && !m_pParent->IsCollisionLoadedAroundPosition())
			{
				bHungUp = false;
			}

			if(bHungUp)
			{
				if(newStuckCounterHungUp + timeStepMs < BIT(16) - 1)
					newStuckCounterHungUp = (u16)(newStuckCounterHungUp + timeStepMs);
				else
					newStuckCounterHungUp = (u16)(BIT(16) - 1);
			}
			else
				newStuckCounterHungUp = 0;
		}
	}
	else
	{
		RC_VEC3F(m_vecLastStuckPos) = vParentPosition;
		newStuckCounterOnRoof = 0;
		newStuckCounterOnSide = 0;
		newStuckCounterHungUp = 0;
	}

	m_nStuckCounterOnRoof = (u16)newStuckCounterOnRoof;
	m_nStuckCounterOnSide = (u16)newStuckCounterOnSide;
	m_nStuckCounterHungUp = (u16)newStuckCounterHungUp;

	// do a separate jammed distance check,
	// want to allow the vehicle to move further but still be considered jammed
	vecPosDelta = vParentPosition - RCC_VEC3F(m_vecLastStuckJammedPos);

	u16 newStuckCounterJammed = m_nStuckCounterJammed;

	fDistLimitSqr = sfStuckJammedDistSqr;
	// boats require bigger distance check because they tend to move around on the water
	if( isWatercraft )
	{
        if( ( m_pParent->InheritsFromBoat() && ((CBoat*)m_pParent)->m_BoatHandling.IsInWater() ) || m_pParent->GetIsInWater() )
        {
		    fDistLimitSqr = sfStuckJammedDistBoatSqr;
		    vecPosDelta.SetZ( vecPosDelta.GetZ() * 0.2f);
        }
	}

	if( MagSquared( vecPosDelta ) < fDistLimitSqr )
	{
		// Jammed check
		// can't be jammed if you've been set to be fixed
		// can't be jammed when engine is starting
		bool bIsEngineStarting = (!m_pParent->m_nVehicleFlags.bEngineOn && m_pParent->m_nVehicleFlags.bEngineStarting);
		if(!m_pParent->GetIsAnyFixedFlagSet() && !bIsEngineStarting)
		{
			// check a lower threshold for AI vehicles because they often use low throttle values when trying to turn
			float fGasPedalThreshold = m_pParent->GetStatus()==STATUS_PLAYER ? 0.5f : 0.2f;
			float throttle = m_pParent->m_vehControls.GetThrottle();

			bool bJammed = (rage::Abs( throttle ) > fGasPedalThreshold &&
						   (!m_bHasHandBrake || !m_pParent->GetHandBrake()) && 
						   !m_pParent->GetBrake());

			float collisionThreshold = 0.0f;

			if( m_pParent->GetModelIndex() == MI_PLANE_TITAN )
			{
				collisionThreshold = 5.0f;
			}

			if( !bJammed &&
				!isAbandonedBike && // If this is an abandoned bike it could be left on its side with no wheels on the ground. If it is inaccessible that will be handled in the hung up check. If it is accessible then we should wait till the ped is on it before seeing if it is jammed.
				pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() > collisionThreshold &&
				!m_pParent->InheritsFromHeli() && // There's no point checking that it is stuck on top of something if it can take off vertically
				!m_pParent->IsInSubmarineMode() &&
				m_pParent->GetOutriggerDeployRatio() != 1.0f )
			{
				const CCollisionRecord * pColRecord = pCollisionHistory->GetMostSignificantCollisionRecord();

                bool bAllWheelsTouching = true;// I was getting "jammed" whilst driving onto a car transporter
                for(int i = 0; i < m_pParent->GetNumWheels(); i++)
                {
                    CPhysical *pHitPhysical = m_pParent->GetWheel(i)->GetHitPhysical();

					if( !m_pParent->GetWheel(i)->GetIsTouching() || 	
						// If wheel is touching a physical and it's not a trailer, consider it not touching anything for jammed checks.
						(m_pParent->GetWheel(i)->GetIsTouching() && pHitPhysical && 
						!(pHitPhysical->GetType() == ENTITY_TYPE_VEHICLE && (static_cast<CVehicle*>(pHitPhysical))->GetVehicleType() == VEHICLE_TYPE_TRAILER))
						)
                    {
                        bAllWheelsTouching = false;
						break;
                    }
                }
                
				if( !bAllWheelsTouching && pColRecord && pColRecord->m_MyCollisionNormal.z > 0.5f)
				{
					bJammed = true;
				}
			}	

			// If amphibious quad has its wheels retracted, don't count it as jammed since wheels won't be touching the ground
			if(m_pParent->InheritsFromAmphibiousQuadBike() && !static_cast<CAmphibiousQuadBike*>(m_pParent)->IsWheelsFullyOut())
			{
				bJammed = false;
			}

            if(isWatercraft)
            {
                if((m_pParent->InheritsFromBoat() && !((CBoat*)m_pParent)->m_BoatHandling.IsInWater()) || !m_pParent->GetIsInWater())
                {
					if(!static_cast<CBoat*>(m_pParent)->GetAnchorHelper().UsingLowLodMode())
					{
						bJammed = true;
					}
                }

				//Can't be jammed if no collision is loaded
				if(bJammed && !m_pParent->IsCollisionLoadedAroundPosition())
				{
					bJammed = false;
				}
            }

			if(isPlane && !m_pParent->IsInAir())
			{
				if(!((CPlane *)m_pParent)->GetIsLandingGearintact())
				{
					bJammed = true;
				}
			}

			if(m_pParent->GetVehicleType() == VEHICLE_TYPE_TRAILER)
			{
				bJammed = false;
			}

			if(bJammed)
			{
				if(newStuckCounterJammed + timeStepMs < BIT(16) - 1)
				{
					newStuckCounterJammed = (u16)(newStuckCounterJammed + timeStepMs);
				}
				else
				{
					newStuckCounterJammed = (u16)(BIT(16) - 1);
				}
			}
			// don't reset jammed check when gas pedal is lifted, just stop counting up
			//else
			//	m_nStuckCounterJammed = 0;
		}
	}
	else
	{
		RC_VEC3F(m_vecLastStuckJammedPos) = vParentPosition;
		newStuckCounterJammed = 0;
	}
	m_nStuckCounterJammed = (u16)newStuckCounterJammed;
}

void CVehicleDamage::ResetStuckCheck()
{
	m_nStuckCounterOnRoof = 0;
	m_nStuckCounterOnSide = 0;
	m_nStuckCounterHungUp = 0;
	m_nStuckCounterJammed = 0;
	m_vecLastStuckPos.Set(VEC3_LARGE_FLOAT);
	m_vecLastStuckJammedPos.Set(VEC3_LARGE_FLOAT);
}

bool CVehicleDamage::GetIsStuck(int nStuckType, u16 nRequiredTime) const
{
	switch(nStuckType)
	{
		case VEH_STUCK_ON_ROOF:
			if(m_nStuckCounterOnRoof > nRequiredTime)
				return true;
			break;
		case VEH_STUCK_ON_SIDE:
			if(m_nStuckCounterOnSide > nRequiredTime)
				return true;
			break;
		case VEH_STUCK_HUNG_UP:
			if(m_nStuckCounterHungUp > nRequiredTime)
				return true;
			break;
		case VEH_STUCK_JAMMED:
			if(m_nStuckCounterJammed > nRequiredTime)
				return true;
			break;
		default:
			Assertf(false, "Unknown stuck check type");
			break;
	}

	return false;
}

void CVehicleDamage::ResetStuckTimer(int nStuckType)
{
	switch(nStuckType)
	{
	case VEH_STUCK_ON_ROOF:
		m_nStuckCounterOnRoof = 0;
		break;
	case VEH_STUCK_ON_SIDE:
		m_nStuckCounterOnSide = 0;
		break;
	case VEH_STUCK_HUNG_UP:
		m_nStuckCounterHungUp = 0;
		break;
	case VEH_STUCK_JAMMED:
		m_nStuckCounterJammed = 0;
		break;
	case VEH_STUCK_RESET_ALL:
		m_nStuckCounterOnRoof = 0;
		m_nStuckCounterOnSide = 0;
		m_nStuckCounterHungUp = 0;
		m_nStuckCounterJammed = 0;
		break;
	default:
		Assertf(false, "Unknown stuck check type");
		break;
	}
}

float CVehicleDamage::GetStuckTimer(int nStuckType) const
{
    switch(nStuckType)
    {
    case VEH_STUCK_ON_ROOF:
        return m_nStuckCounterOnRoof;
        break;
    case VEH_STUCK_ON_SIDE:
        return m_nStuckCounterOnSide;
        break;
    case VEH_STUCK_HUNG_UP:
        return m_nStuckCounterHungUp;
        break;
    case VEH_STUCK_JAMMED:
        return m_nStuckCounterJammed;
        break;
    default:
        Assertf(false, "Unknown stuck check type");
        break;
    }

    return 0.0f;
}


static dev_float sfBrokenPartsResetTime = 1.0f;
void CVehicleDamage::ResetBrokenPartCountdown()
{
    m_fCountDownToTimeAnotherPartCanBreakOff = sfBrokenPartsResetTime;
}

dev_float dfHeliRotorUndriveableSpeed = 0.3f;

#if __BANK
#define IS_DRIVEABLE_FAILURE_DEBUG(msg) \
if(CVehicle::ms_bDebugVehicleIsDriveableFail) \
{ \
	vehicleDisplayf("IsDriveable check failed for %s at (%5.3f, %5.3f, %5.3f). Reason: %s.", m_pParent->GetModelName(), \
					m_pParent->GetVehiclePosition().GetXf(), m_pParent->GetVehiclePosition().GetYf(), m_pParent->GetVehiclePosition().GetZf(), msg); \
}
#else // __BANK
#define IS_DRIVEABLE_FAILURE_DEBUG(x)
#endif // __BANK

bool CVehicleDamage::GetIsDriveable(bool bCheckForPlayerPed, bool bIgnoreHealthCheck, bool bIgnorePetrolCheck) const
{
	if(m_pParent->GetStatus() == STATUS_WRECKED)
	{
		IS_DRIVEABLE_FAILURE_DEBUG("Status is 'wrecked'");
		return false;
	}

	float tooMuchDamage = SelectFT(IsLessThan(GetPetrolTankHealth(), bCheckForPlayerPed ? VEH_DAMAGE_HEALTH_PTANK_FINISHED : VEH_DAMAGE_HEALTH_PTANK_ONFIRE), 0.0f, 1.0f);
	tooMuchDamage = SelectFT(IsLessThanOrEqual(GetEngineHealth(), bCheckForPlayerPed ? ENGINE_DAMAGE_FINSHED : ENGINE_DAMAGE_ONFIRE), tooMuchDamage,  1.0f);

	if (tooMuchDamage > 0.0f)
	{
		IS_DRIVEABLE_FAILURE_DEBUG("Too much damage");
		return false;
	}

	// engine is dead from collision (decided in CTransmission::ApplyEngineDamage)
	// Script can set the vehicle health to zero also, B*815022
	if(!bIgnoreHealthCheck && !m_pParent->m_nVehicleFlags.bEngineOn && GetEngineHealth() <= ENGINE_DAMAGE_ONFIRE)
	{
		IS_DRIVEABLE_FAILURE_DEBUG("Engine is dead from collision");
		return false;
	}

    // make sure we have petrol left
    if(!bIgnorePetrolCheck && m_pParent->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
    {
        if(m_pParent->m_nVehicleFlags.bPetrolTankEmpty && m_pParent->pHandling->m_fPetrolTankVolume > 0.0f)
        {
            if (!m_pParent->HasInfiniteFuel())
            {
				IS_DRIVEABLE_FAILURE_DEBUG("Out of petrol");
                return false;
            }
        }
    }

	// for heli's check rotors and tail, but ignore wheels
	if(Unlikely(m_pParent->GetIsRotaryAircraft()))
	{
		CRotaryWingAircraft* pAircraft = (CRotaryWingAircraft*)m_pParent;
		if(GetEngineHealth() <= sfHeliEngineBreakDownHealth && pAircraft->GetMainRotorSpeed() < dfHeliRotorUndriveableSpeed)
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Rotary aircraft low rotor speed");
			return false;
		}
		if(pAircraft->GetMainRotorHealth() <= 0.0f || pAircraft->GetRearRotorHealth() <= 0.0f)
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Rotary aircraft rotor health");
			return false;
		}
		if(pAircraft->GetIsTailBoomBroken())
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Rotary aircraft tail boom broken");
			return false;
		}
	}
    // for planes make sure we have both wings
    else if(Unlikely(m_pParent->InheritsFromPlane()))
    {
        CPlane* pAircraft = (CPlane*)m_pParent;
        if( (pAircraft->GetAircraftDamage().GetSectionHealth( CAircraftDamage::WING_L ) <= 0.0f) || 
            (pAircraft->GetAircraftDamage().GetSectionHealth( CAircraftDamage::WING_R ) <= 0.0f) )
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Aircraft section health");
            return false;
		}
		if(GetEngineHealth() <= sfPlaneEngineBreakDownHealth && pAircraft->GetEngineSpeed() < SMALL_FLOAT)
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Aircraft engine health");
			return false;
		}
		if(pAircraft->GetNumPropellors() > 0)
		{
			bool bAnyAlivePropeller = false;
			for(int i =0; i< pAircraft->GetNumPropellors(); i++)
			{
				if(pAircraft->GetPropellerHealth(i) > 0.0f)
				{
					bAnyAlivePropeller = true;
					break;
				}
			}

			if(!bAnyAlivePropeller)
			{
				IS_DRIVEABLE_FAILURE_DEBUG("Aircraft without any alive propeller");
				return false;
			}
		}
    }
	else if(Unlikely(m_pParent->InheritsFromSubmarine()))
	{
		//Subs cannot be driven after their engine health reaches 0.0f
		float tooMuchDamage = SelectFT(IsLessThan(GetEngineHealth(), 0.0f), 0.0f, 1.0f);

		if (tooMuchDamage > 0.0f)
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Submarine too much damage");
			return false;
		}
	}
	else if( Unlikely(m_pParent->InheritsFromBoat()) )
	{
		CBoat* pBoat = static_cast< CBoat* >( m_pParent );
		if( pBoat->m_BoatHandling.GetCapsizedTimer() >= 1.0f)
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Boat capsized");
			return false;
		}
	}
	else
	{
		// check for wheels being jammed
		float tooMuchDamage = 0.0f;
											 
		CWheel* const * wheels = m_pParent->GetWheels();
		int numWheels = m_pParent->GetNumWheels();
		for(int i = 0; i < numWheels; i++)
		{
			// If a wheel has been broken off, this vehicle is undriveable for AI.
			if(!bCheckForPlayerPed && wheels[i]->GetDynamicFlags().IsFlagSet(WF_BROKEN_OFF))
			{
				IS_DRIVEABLE_FAILURE_DEBUG("Wheel broken off");
				return false;
			}

			// If frictionDamage > 1.0, set to true
			tooMuchDamage = SelectFT(IsGreaterThan(wheels[i]->GetFrictionDamage(), 1.0), tooMuchDamage, 1.0f );
		}

		if (tooMuchDamage > 0.0f)
		{
			IS_DRIVEABLE_FAILURE_DEBUG("Too much wheel damage");
			return false;
		}

	}

	return true;
}


void CVehicleDamage::ApplyDamageToWheels(CEntity* pInflictor, eDamageType nDamageType, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, const Vector3& vecDirnLocal, int nComponent, phMaterialMgr::Id nMaterialId, int nPieceIndex)
{
	((void)pInflictor);
	((void)nDamageType);
	((void)vecDirnLocal);
	((void)nComponent);


    if(!m_pParent->InheritsFromBike()) // Don't damage suspension on bikes
    {
	    Vector3 vecSusPos;
	    float fSusRadius;
	    for(int i=0; i<m_pParent->GetNumWheels(); i++)
	    {
			if(!m_pParent->GetWheel(i)->GetConfigFlags().IsFlagSet(WCF_CENTRE_WHEEL)) // Similarly to bikes, don't damage centre wheels
			{
				fSusRadius = m_pParent->GetWheel(i)->GetSuspensionPos(vecSusPos);
				if((vecPosLocal - vecSusPos).Mag2() < fSusRadius*fSusRadius)
				{
					m_pParent->GetWheel(i)->ApplySuspensionDamage(fDamage);
				}
			}
	    }
    }

	if(PGTAMATERIALMGR->GetPolyFlagVehicleWheel(nMaterialId) && PGTAMATERIALMGR->UnpackMtlId(nMaterialId)==PGTAMATERIALMGR->g_idRubber)
	{
		// wheel index is stored in the piece type
		m_pParent->GetWheel(nPieceIndex)->ApplyTyreDamage(pInflictor, fDamage, vecPosLocal, vecNormLocal, nDamageType, nPieceIndex);
	}
}


void CVehicleDamage::ApplyDamageToGlass(float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, const Vector3& vecDirnLocal)
{
	if( fDamage < GlassDamageThreshold ) // to small, not worth processing.
		return;
		
	if (NetworkInterface::IsGameInProgress() && m_pParent->IsNetworkClone())
		return;

	// calc the damage position in world space
	Vector3 worldDamagePos;
	m_pParent->TransformIntoWorldSpace(worldDamagePos, vecPosLocal);

	const bool isTaxi = CVehicle::IsTaxiModelId(m_pParent->GetModelId());
	int lightCount = isTaxi ? NUM_GLASS_BONES : NUM_LIGHT_BONES;

	// Glass
	if(  !m_pParent->m_nVehicleFlags.bUnbreakableLights )
	{
		for(int i=0; i<lightCount; i++)
		{
			const int boneIdx = m_pParent->GetBoneIndex(aGlassBones[i]);
			const int lightIdx = ( i > TAXI_IDX ) ? TAXI_IDX : i;
			const bool lightState = GetLightStateImmediate(lightIdx);
			if( boneIdx != -1 && (false == lightState))
			{
				// get the light position in world space
				Matrix34 worldLightMat;
				m_pParent->GetGlobalMtx(boneIdx, worldLightMat);

				// check if the damage is close to the light
				if((worldLightMat.d - worldDamagePos).Mag2() < 0.2f*0.2f)
				{
					// it is - smash the light
					Vector3 localLightPos = VEC3V_TO_VECTOR3(m_pParent->GetTransform().UnTransform(VECTOR3_TO_VEC3V(worldLightMat.d)));

					// find a suitable normal
					Vector3 localNormal(0.0f, 0.0f, 1.0f);
					if (!vecNormLocal.IsZero())
					{
						localNormal = vecNormLocal;
					}
					else if (!vecDirnLocal.IsZero())
					{
						localNormal = -vecDirnLocal;
					}
					else if (!localLightPos.IsZero())
					{
						localNormal = localLightPos;
					}

					localNormal.Normalize();
					// check that the normal is valid
					Assertf(localNormal.Mag()>0.997f && localNormal.Mag()<1.003f, "Normal is not valid on %s %.4f,%.4f,%.4f for light %d,%d", m_pParent->GetModelName(), localNormal.x, localNormal.y, localNormal.z,boneIdx,lightIdx);

					g_vfxVehicle.TriggerPtFxLightSmash(m_pParent, aGlassBones[i], RCC_VEC3V(localLightPos), RCC_VEC3V(localNormal));
					m_pParent->GetVehicleAudioEntity()->TriggerHeadlightSmash(aGlassBones[i],localLightPos);
					SetLightStateImmediate(lightIdx,true);
					SetUpdateLightBones(true);

					if(NetworkInterface::IsGameInProgress() && m_pParent->GetNetworkObject() && !m_pParent->GetNetworkObject()->IsClone())
					{
						CNetObjVehicle* netParent = SafeCast(CNetObjVehicle, m_pParent->GetNetworkObject());
						netParent->ForceSendVehicleDamageNode();
					}
				}
			}
		}
		
		// Extras
        const CVehicleVariationInstance& variationInstance = m_pParent->GetVariationInstance();
        if (variationInstance.GetKitIndex() != INVALID_VEHICLE_KIT_INDEX && variationInstance.GetVehicleRenderGfx())
        {
            for (u32 ii = VEH_EXTRALIGHT_1; ii <= VEH_EXTRALIGHT_4; ++ii)
            {
				const bool lightState = GetLightState(ii);
				if( false == lightState )
				{
					// get the light position in world space
					int boneIdx;
					Mat34V worldLightMat;
					m_pParent->GetExtraLightMatrix((eHierarchyId)ii, worldLightMat, boneIdx, NULL);
					if( boneIdx != -1 )
					{
						// check if the damage is close to the light
						if((VEC3V_TO_VECTOR3(worldLightMat.d()) - worldDamagePos).Mag2() < 0.2f*0.2f)
						{
							// it is - smash the light
							Vector3 localLightPos = VEC3V_TO_VECTOR3(m_pParent->GetTransform().UnTransform(worldLightMat.d()));

							// find a suitable normal
							Vector3 localNormal(0.0f, 0.0f, 1.0f);
							if (!vecNormLocal.IsZero())
							{
								localNormal = vecNormLocal;
							}
							else if (!vecDirnLocal.IsZero())
							{
								localNormal = -vecDirnLocal;
							}
							else if (!localLightPos.IsZero())
							{
								localNormal = localLightPos;
							}

							localNormal.Normalize();
							// check that the normal is valid
							Assertf(localNormal.Mag()>0.997f && localNormal.Mag()<1.003f, "Normal is not valid on %s %.4f,%.4f,%.4f for light %d,%d", m_pParent->GetModelName(), localNormal.x, localNormal.y, localNormal.z,boneIdx,ii);

							g_vfxVehicle.TriggerPtFxLightSmash(m_pParent, ii, RCC_VEC3V(localLightPos), RCC_VEC3V(localNormal));
							m_pParent->GetVehicleAudioEntity()->TriggerHeadlightSmash(ii,localLightPos);
							SetLightState(ii,true);
							SetUpdateLightBones(true);
						}
					}
				}
			}
		}

		if( m_pParent->UsesSiren() )
		{
			// Siren
			for(int i=VEH_SIREN_1; i<VEH_SIREN_MAX + 1; i++)
			{
				const int boneIdx = m_pParent->GetBoneIndex((eHierarchyId)i);
				const bool sirenState = GetSirenState(i);
				if( boneIdx != -1 && (false == sirenState))
				{
					// get the siren position in world space
					Matrix34 worldLightMat;
					m_pParent->GetGlobalMtx(boneIdx, worldLightMat);

					// check if the damage is close to the siren
					static dev_float sirenRadius = 0.10f;
					if((worldLightMat.d - worldDamagePos).Mag2() < sirenRadius*sirenRadius)
					{
						// find a suitable normal
						SetSirenState(i,true);
						SetUpdateSirenBones(true);
						
						// No Fxs
						// pop/break the glass casing
						const int boneGlassID = m_pParent->GetBoneIndex((eHierarchyId)(i + (VEH_SIREN_GLASS_1-VEH_SIREN_1)));
						const int component = m_pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(boneGlassID);
						if( component != -1 )
						{
							Vec3V vSmashNormal(V_Z_AXIS_WZERO);
							g_vehicleGlassMan.SmashCollision(m_pParent, component, VEHICLEGLASSFORCE_SIREN_SMASH, vSmashNormal);
							m_pParent->GetVehicleAudioEntity()->SmashSiren();
						}
					}
				}
			}
		}
	}
}

void CVehicleDamage::ApplyDamageToOverallHealth(CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPosLocal, const Vector3& vecDirnLocal, int nComponent, const bool bChainExplosion)
{
	((void)nComponent);

	CPed* pInflictorPed = NULL;

			// If the vehicle has already blown up there is no point doing more damage.
//	if (m_pParent->m_nPhysicalFlags.bRenderScorched)	Wait till decision on cars blowing up is made
//	{
//		return;
//	}

	if (pInflictor)		// pInflictor can be NULL
	{
		if(pInflictor->GetIsTypePed())
			pInflictorPed = (CPed*)pInflictor;
		else if(pInflictor->GetIsTypeVehicle())
			pInflictorPed = ((CVehicle*)pInflictor)->GetDriver();
	}

	// Ignore damage caused by collisions with peds, other reactions should handle this
	if( nWeaponHash == WEAPONTYPE_RAMMEDBYVEHICLE && pInflictor && pInflictor->GetIsTypePed() )
	{
		// Ignore
	}
	else
	{
		// update player damage stats
		if(fDamage > 10.0f && pInflictorPed && pInflictorPed->IsLocalPlayer())
		{
			if(m_pParent->GetStatus() != STATUS_WRECKED)
			{
				pInflictorPed->GetPlayerInfo()->HavocCaused += HAVOC_DENTCAR;
			}
		}

		// If we want to do generic vehicle hit registration with the hud do it here.

		// If the vehicle being damaged has an alive law enforcement ped in it
		// increase the wanted level to a minimum of 1
		if(pInflictorPed && pInflictorPed->IsLocalPlayer())
		{
			if(m_pParent->HasAliveLawPedsInIt())
			{
				// Make an exception if there has been a collision between player car and police car and the police car went faster. It was probably its fault.
				if (!pInflictorPed->GetVehiclePedInside() || nDamageType != DAMAGE_TYPE_COLLISION ||
						pInflictorPed->GetVehiclePedInside()->GetVelocity().Mag2() > m_pParent->GetVelocity().Mag2() )
				{
					//Notify the passengers that they were rammed in a vehicle.
					for(int i = 0; i < m_pParent->GetSeatManager()->GetMaxSeats(); ++i)
					{
						CPed* pPedInSeat = m_pParent->GetSeatManager()->GetPedInSeat(i);
						if(pPedInSeat)
						{
							pPedInSeat->GetPedIntelligence()->RammedInVehicle(pInflictorPed);
						}
					}

					const Vector3 vInflictorPosition = VEC3V_TO_VECTOR3(pInflictorPed->GetTransform().GetPosition());

					if(pInflictorPed->GetPlayerInfo()->GetWanted().GetWantedLevel() == WANTED_CLEAN && pInflictorPed->GetPlayerInfo()->GetWanted().m_fMultiplier)
					{
						if(nDamageType != DAMAGE_TYPE_COLLISION || !pInflictorPed->GetPlayerInfo()->GetWanted().IsCrimeSuppressedThisFrame(CRIME_DAMAGE_TO_PROPERTY))
						{
							pInflictorPed->GetPlayerWanted()->SetWantedLevelNoDrop(vInflictorPosition, WANTED_LEVEL1, WANTED_CHANGE_DELAY_INSTANT, WL_FROM_LAW_RESPONSE_EVENT);
						}

						// if the car was shot then report the shooting at cop crime
						if(nDamageType == DAMAGE_TYPE_BULLET || nDamageType == DAMAGE_TYPE_BULLET_RUBBER)
						{
							CCrime::ReportCrime(CRIME_SHOOT_AT_COP, m_pParent, pInflictorPed);
						}
					}
					//Moving this outside of the WANTED_CLEAN check.  I think we want scanner audio even if we already have stars.
	#if NA_POLICESCANNER_ENABLED
					if(nDamageType == DAMAGE_TYPE_COLLISION)
					{
						g_PoliceScanner.ReportPlayerCrime(vInflictorPosition, CRIME_RECKLESS_DRIVING, WANTED_CHANGE_DELAY_INSTANT);
					}
					else if(nDamageType == DAMAGE_TYPE_BULLET || nDamageType == DAMAGE_TYPE_BULLET_RUBBER)
					{
						g_PoliceScanner.ReportPlayerCrime(vInflictorPosition, CRIME_SHOOT_AT_COP, WANTED_CHANGE_DELAY_INSTANT);
					}
					else
					{
						g_PoliceScanner.ReportPlayerCrime(vInflictorPosition, CRIME_DAMAGE_TO_PROPERTY, WANTED_CHANGE_DELAY_INSTANT);
					}
	#endif // NA_POLICESCANNER_ENABLED
				}
			}
			else if(m_pParent->IsLawEnforcementVehicle() && nDamageType == DAMAGE_TYPE_COLLISION)
			{
				CCrime::ReportCrime(CRIME_DAMAGE_TO_PROPERTY, m_pParent, pInflictorPed);
			}
		}

		if(m_pParent->GetHealth() > 0.0f)
		{
			// add events to let peds in this car know about damage
			if(pInflictorPed && (m_pParent->GetStatus()==STATUS_PLAYER || m_pParent->GetStatus()==STATUS_PHYSICS))
			{
				for(int iSeat=0; iSeat<m_pParent->GetSeatManager()->GetMaxSeats(); iSeat++)
				{
					if(m_pParent->GetSeatManager()->GetPedInSeat(iSeat))
					{
						const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
						// Check if the weapon triggers disputes instead of combat.
						if (pInflictorPed && pInflictorPed->IsAPlayerPed() && pWeaponInfo && pWeaponInfo->GetDamageCausesDisputes())
						{
							// Give the ped an agitated event.
							CEventAgitated event(pInflictorPed, AT_Griefing);
							m_pParent->GetSeatManager()->GetPedInSeat(iSeat)->GetPedIntelligence()->AddEvent(event);
						}
						else
						{
							if( !m_pParent->HasRamp() && !m_pParent->HasRammingScoop() )
							{
								CEventVehicleDamageWeapon event(m_pParent, pInflictor, nWeaponHash, fDamage, vecPosLocal, vecDirnLocal);
								m_pParent->GetSeatManager()->GetPedInSeat(iSeat)->GetPedIntelligence()->AddEvent(event);
							}
						}
					}
				}
			}

			if(m_pParent->GetHealth() > fDamage)
			{
				m_pParent->ChangeHealth(-fDamage);
			}
			else
			{
				m_pParent->SetHealth(0.0f);

				if(bChainExplosion)
				{
					CCrime::ReportCrime(CRIME_CHAIN_EXPLOSION, m_pParent, pInflictorPed);
				}
				else
				{
					CCrime::ReportDestroyVehicle(m_pParent, pInflictorPed);
				}
			}
		}
	}
}

Vector3 CVehicleDamage::RecomputeImpulseFromTrain(CTrain *pTrain, const Vector3& vImpulse) const
{
	static dev_float sf_MinTrainSpeedSqToRecomputeImpulse = 4.0f;
	if(pTrain->GetNumCarriages() > 1 && pTrain->GetVelocity().Mag2() > sf_MinTrainSpeedSqToRecomputeImpulse)
	{
		// Recompute the impulse to include the mass of all train carriages
		Vector3 vTrainForward = VEC3V_TO_VECTOR3(pTrain->GetTransform().GetForward());
		Vector3 vParentFoward = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetForward());
		float vAligned = vTrainForward.Dot(vParentFoward);

		int nNumCarriages = Max(pTrain->GetNumCarriages(), 1);
		float fMassRatio = (1.0f / (float)nNumCarriages);
		fMassRatio = Max(fMassRatio * (1.0f + vAligned), fMassRatio);

		Vector3 vDeltaVelParent = vImpulse * InvertSafe(m_pParent->GetMass() * fMassRatio, m_pParent->GetInvMass());
		return m_pParent->GetMass() * vDeltaVelParent;
	}

	return vImpulse;
}


void CVehicleDamage::ApplyPadShake(const CEntity* pInflictor, float fDamage)
{
	if(fDamage < sfMinRumbleDamThreshold)
		return;

	// SHAKE PAD
	if (m_pParent && m_pParent->ContainsLocalPlayer( ))
	{
        
		if (pInflictor && pInflictor->GetIsTypePed() && ((CPed*)pInflictor)->GetIsAttached())
		{
			CEntity* pAttachToEntity = (CPhysical *) ((CPed*)pInflictor)->GetAttachParent();
			if (pAttachToEntity && pAttachToEntity->GetIsTypeVehicle() && pAttachToEntity == m_pParent)
				return; // Do not Shake with ped's attached to the vehicle
		}
		else if (pInflictor && pInflictor->GetIsTypeObject() && ((CObject*)pInflictor)->GetIsAttached())
		{
			CEntity* pAttachToEntity = (CPhysical *) ((CPed*)pInflictor)->GetAttachParent();
			if (pAttachToEntity && pAttachToEntity->GetIsTypeVehicle() && pAttachToEntity == m_pParent)
				return; // Do not Shake with objects attached to the vehicle
		}
		// Do not shake if Infictor's vehicle is not moving
		else if (pInflictor && pInflictor->GetIsTypeVehicle() && ((CVehicle*)pInflictor)->GetVelocity().Mag2() < SPEED_TO_CONSIDER_MOV && ((CVehicle*)pInflictor)->GetDistanceTravelled() < DIST_TO_CONSIDER_MOV)
			return;
		// Do not shake if Vehicle is not moving
// 		else if (m_pParent->GetVelocity().Mag2() < SPEED_TO_CONSIDER_MOV && m_pParent->GetDistanceTravelled() < DIST_TO_CONSIDER_MOV)
// 			return;

		float fIntensity = (sfRumbleIntensModifier * fDamage);
		float fDur = (sfRumbleDurModifier * fDamage);
		
#if __BANK
		if (CControlMgr::sm_bUseThisMultiplier)
			fIntensity = (CControlMgr::sm_fMultiplier * fDamage);
#endif // __BANK


		if(fIntensity > m_fPadShakeIntensity)
		{
			m_fPadShakeIntensity = fIntensity;
			m_uPadShakeDuration = (u8)rage::Clamp(fDur,sfMinRumbleDur,sfMaxRumbleDur);
		}
	}
}

void CVehicleDamage::ApplyPadShakeInternal()
{
	if(m_fPadShakeIntensity > 0.0f && m_uPadShakeDuration > 0)
	{

#if __BANK
		if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE && (m_pParent->GetStatus()==STATUS_PLAYER || m_pParent == CDebugScene::FocusEntities_Get(0)) ) 
		{
			grcDebugDraw::PrintToScreenCoors("Vehicle damage rumble", 4, 34, Color32(0.90f,0.90f,0.90f), 0, m_uPadShakeDuration/24);
		}
#endif
		CControlMgr::StartPlayerPadShakeByIntensity(m_uPadShakeDuration, m_fPadShakeIntensity);
		m_fPadShakeIntensity = 0.0f;
		m_uPadShakeDuration = 0;
	}
}

bool CVehicleDamage::GetPetrolTankPosWld(Vector3* pPetrolTankWldPosL, Vector3* pPetrolTankWldPosR, Vector3* pPetrolTankWldPosCap)
{
	Vector3 petrolTankLclPosL;
	Vector3 petrolTankLclPosR;
	Vector3 petrolTankLclPosCap;
	bool bValidPosition = GetPetrolTankPosLcl(&petrolTankLclPosL, &petrolTankLclPosR, &petrolTankLclPosCap);

	if (Verifyf(pPetrolTankWldPosL, "must pass a valid pPetrolTankWldPosL into this function"))
	{
		*pPetrolTankWldPosL = VEC3V_TO_VECTOR3(m_pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(petrolTankLclPosL)));
	}

	if (pPetrolTankWldPosR)
	{
		if (IsZeroAll(VECTOR3_TO_VEC3V(petrolTankLclPosR)))
		{
			// if the local position is zero then it isn't defined - keep the world position as zero as well
			*pPetrolTankWldPosR = petrolTankLclPosR;
		}
		else
		{
			*pPetrolTankWldPosR = VEC3V_TO_VECTOR3(m_pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(petrolTankLclPosR)));
		}
	}

	if (pPetrolTankWldPosCap)
	{
		*pPetrolTankWldPosCap = VEC3V_TO_VECTOR3(m_pParent->GetTransform().Transform(VECTOR3_TO_VEC3V(petrolTankLclPosCap)));
	}

	return bValidPosition;
}

bool CVehicleDamage::GetPetrolTankPosLcl(Vector3* pPetrolTankLclPosL, Vector3* pPetrolTankLclPosR, Vector3* pPetrolTankLclPosCap)
{
	// we used to always generate a right hand side petrol tank position when one wasn't explicitly set
	// however, url:bugstar:928172 requested that we no longer do this
	#define	FORCE_RHS_PETROL_TANK_POS (0)

	bool bValidPosition = false;

	if (Verifyf(pPetrolTankLclPosL, "must pass a valid pPetrolTankLclPosL into this function"))
	{
		if (m_pParent->GetBoneIndex(VEH_PETROLTANK)!=-1)
		{
			// get the left sided petrol tank position from the bone
			Matrix34 petrolTankMtxLcl = m_pParent->GetLocalMtx(m_pParent->GetBoneIndex(VEH_PETROLTANK));
			Vector3 petrolTankPosLcl = petrolTankMtxLcl.d;

			*pPetrolTankLclPosL = petrolTankPosLcl;

			if(pPetrolTankLclPosR)
			{
#if FORCE_RHS_PETROL_TANK_POS
				petrolTankPosLcl.x = -petrolTankPosLcl.x;
				*pPetrolTankLclPosR = petrolTankPosLcl;
#else
				pPetrolTankLclPosR->Zero();
#endif
			}

			if(pPetrolTankLclPosCap)
			{
				pPetrolTankLclPosCap->Zero();
			}

			bValidPosition = true;
		}
		
		if(!bValidPosition && (m_pParent->GetBoneIndex(VEH_PETROLTANK_L)!=-1 || m_pParent->GetBoneIndex(VEH_PETROLTANK_R)!=-1))
		{
			//If we have left and right petrol tanks set up then use them
			if(Verifyf(m_pParent->GetBoneIndex(VEH_PETROLTANK_L)!=-1, "Vehicle has a petroltank_r bone without petroltank_l - pPetrolTankLclPosL will be zero!"))
			{
				Vector3 petrolTankPosLcl;
				m_pParent->GetDefaultBonePositionSimple(m_pParent->GetBoneIndex(VEH_PETROLTANK_L), petrolTankPosLcl);

				*pPetrolTankLclPosL = petrolTankPosLcl;
			}
			else
			{
				pPetrolTankLclPosL->Zero();
			}

			if(m_pParent->GetBoneIndex(VEH_PETROLTANK_R)!=-1)
			{
				Vector3 petrolTankPosLcl;
				m_pParent->GetDefaultBonePositionSimple(m_pParent->GetBoneIndex(VEH_PETROLTANK_R), petrolTankPosLcl);

				if(pPetrolTankLclPosR)
				{
					*pPetrolTankLclPosR = petrolTankPosLcl;
				}
			}
			else if(pPetrolTankLclPosR)
			{
				pPetrolTankLclPosR->Zero();
			}

			if(pPetrolTankLclPosCap)
			{
				pPetrolTankLclPosCap->Zero();
			}

			bValidPosition = true;
		}
		
		bool bHasPetrolCap = false;
		if(!bValidPosition && m_pParent->GetBoneIndex(VEH_PETROLCAP)!=-1)
		{
			Matrix34 petrolTankMtxLcl = m_pParent->GetLocalMtx(m_pParent->GetBoneIndex(VEH_PETROLCAP));
			Vector3 petrolTankPosLcl = petrolTankMtxLcl.d;

			*pPetrolTankLclPosL = petrolTankPosLcl;
			
			if(pPetrolTankLclPosCap)
			{
				*pPetrolTankLclPosCap = petrolTankPosLcl;
			}

			if(pPetrolTankLclPosR)
			{
				pPetrolTankLclPosR->Zero();
			}

			bValidPosition = true;
		}
		else if(pPetrolTankLclPosCap && m_pParent->GetBoneIndex(VEH_PETROLCAP)!=-1)
		{
			Matrix34 petrolTankMtxLcl = m_pParent->GetLocalMtx(m_pParent->GetBoneIndex(VEH_PETROLCAP));
			Vector3 petrolTankPosCapLcl = petrolTankMtxLcl.d;

			*pPetrolTankLclPosCap = petrolTankPosCapLcl;
			bHasPetrolCap = true;
		}

		// For planes we position the fuel tanks in the wings
		if( !bValidPosition && m_pParent->InheritsFromPlane() && m_pParent->GetBoneIndex( PLANE_WING_L ) !=-1 )
		{
			const Vector3 vParentPosition = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetPosition());
			*pPetrolTankLclPosL = vParentPosition;

			// get the left wing position
			if(m_pParent->GetSkeleton())
			{
				// get the local bone position of the left wing
				*pPetrolTankLclPosL = RCC_VECTOR3(m_pParent->GetSkeleton()->GetSkeletonData().GetBoneData(m_pParent->GetBoneIndex( PLANE_WING_L ))->GetDefaultTranslation());

				if (pPetrolTankLclPosR)
				{
					*pPetrolTankLclPosR = *pPetrolTankLclPosL;
					pPetrolTankLclPosR->x = -pPetrolTankLclPosR->x;

					bValidPosition = true;
				}

				if(pPetrolTankLclPosCap && !bHasPetrolCap)
				{
					pPetrolTankLclPosCap->Zero();
				}
			}
		}

		if (!bValidPosition && m_pParent->GetBoneIndex(VEH_WHEEL_LR)!=-1)
		{
			const Vector3 vParentPosition = VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetPosition());
			*pPetrolTankLclPosL = vParentPosition;

			if (pPetrolTankLclPosR)
			{
#if FORCE_RHS_PETROL_TANK_POS
				*pPetrolTankLclPosR = vParentPosition;
#else
				pPetrolTankLclPosR->Zero();
#endif
			}

			// get the left rear wheel position and move slightly forward
			if(m_pParent->GetSkeleton())
			{
				// get the local bone position of the left rear wheel
				*pPetrolTankLclPosL = RCC_VECTOR3(m_pParent->GetSkeleton()->GetSkeletonData().GetBoneData(m_pParent->GetBoneIndex(VEH_WHEEL_LR))->GetDefaultTranslation());

				// calc the local bone position of the right rear wheel from this
				if (pPetrolTankLclPosR)
				{
#if FORCE_RHS_PETROL_TANK_POS
					*pPetrolTankLclPosR = *pPetrolTankLclPosL;
					pPetrolTankLclPosR->x = -pPetrolTankLclPosR->x;
#else
					pPetrolTankLclPosR->Zero();
#endif
				}
			}

			// move forward and out
			pPetrolTankLclPosL->y += 0.25f;
			pPetrolTankLclPosL->x -= 0.1f;

			if (pPetrolTankLclPosR)
			{
#if FORCE_RHS_PETROL_TANK_POS
				pPetrolTankLclPosR->y += 0.25f;
				pPetrolTankLclPosR->x += 0.1f;
#else
				pPetrolTankLclPosR->Zero();
#endif
			}

			if(pPetrolTankLclPosCap && !bHasPetrolCap)
			{
				pPetrolTankLclPosCap->Zero();
			}

			bValidPosition = true;
		}

	}

	return bValidPosition;
}

void CVehicleDamage::FindClosestPetrolTankPos(const Vector3& vecPosLocal, Vector3* vecPetrolTankTestPos)
{
	Vector3 vecPetrolTankPosL, vecPetrolTankPosR, vecPetrolTankPosCap;
	GetPetrolTankPosLcl(&vecPetrolTankPosL, &vecPetrolTankPosR, &vecPetrolTankPosCap );

	float fClosestTankDist2 = FLT_MAX;
	if(vecPetrolTankPosL.IsNonZero())
	{
		fClosestTankDist2 = vecPosLocal.Dist2(vecPetrolTankPosL);
		*vecPetrolTankTestPos = vecPetrolTankPosL;
	}

	if(vecPetrolTankPosR.IsNonZero())
	{
		float fDist2TankBoneR = FLT_MAX;
		fDist2TankBoneR = vecPosLocal.Dist2(vecPetrolTankPosR);
		if(fDist2TankBoneR < fClosestTankDist2)
		{
			fClosestTankDist2 = fDist2TankBoneR;
			*vecPetrolTankTestPos = vecPetrolTankPosR;
		}
	}

	if(vecPetrolTankPosCap.IsNonZero())
	{
		float fDist2TankBoneCap = FLT_MAX;
		fDist2TankBoneCap = vecPosLocal.Dist2(vecPetrolTankPosCap);
		if(fDist2TankBoneCap < fClosestTankDist2)
		{
			fClosestTankDist2 = fDist2TankBoneCap;
			*vecPetrolTankTestPos = vecPetrolTankPosCap;
		}
	}
}


//
//
dev_float sfWindscreenBreakSpdImpulse = 1.85f;
dev_float sfWindscreenBreakUpImpulse = 1.3f;
dev_float sfWindscreenBreakSideOffset = -0.4f;
dev_float sfWindscreenBreakUpOffset = -0.25f;

dev_float sfWindscreenSpeedUpMultiplier = 0.2f;
dev_float sfWindscreenSpeedFwdMultiplier = -0.12f;
//
void CVehicleDamage::PopOutWindScreen()
{  
    fragInstGta* pFragInst = m_pParent->GetVehicleFragInst();

	// pop out the windscreen
	if(m_pParent->GetBoneIndex(VEH_WINDSCREEN)!=-1 && pFragInst)
    {   
		int windowFrag = pFragInst->GetComponentFromBoneIndex( m_pParent->GetBoneIndex(VEH_WINDSCREEN) );

		if (!m_pParent->CarPartsCanBreakOff())
		{
			pFragInst->DeleteAbove(windowFrag);
		}
		else if( windowFrag >= 0 && windowFrag < pFragInst->GetType()->GetPhysics(0)->GetNumChildren())
		{
            s32 nGroup = pFragInst->GetType()->GetPhysics(0)->GetAllChildren()[windowFrag]->GetOwnerGroupPointerIndex();
            if(pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(nGroup))// Make sure windscreen isn't already broken off
            {
				m_pParent->PartHasBrokenOff(VEH_WINDSCREEN);
                fragInst* pNewFragInst = pFragInst->BreakOffAbove(windowFrag);
				CVehicle::ClearLastBrokenOffPart();

                phCollider* pWindscreenCollider = pNewFragInst ? CPhysics::GetSimulator()->GetCollider(pNewFragInst->GetLevelIndex()) : NULL;

                if(pNewFragInst && pWindscreenCollider)
                {
                    const Vector3 vecParentRight(VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetA()));
                    const Vector3 vecParentForward(VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetB()));
                    const Vector3 vecParentUp(VEC3V_TO_VECTOR3(m_pParent->GetTransform().GetC()));

                    float fFwdSpeed = DotProduct(m_pParent->GetVelocity(), vecParentForward);

                    Vector3 vecWindscreenImpulse;
                    vecWindscreenImpulse.Set(sfWindscreenBreakSpdImpulse * vecParentForward + (sfWindscreenBreakSpdImpulse * fFwdSpeed * sfWindscreenSpeedFwdMultiplier * vecParentForward));
                    vecWindscreenImpulse.Add(sfWindscreenBreakUpImpulse * vecParentUp + (sfWindscreenBreakUpImpulse * fFwdSpeed * sfWindscreenSpeedUpMultiplier * vecParentUp));
                    vecWindscreenImpulse.Scale(pNewFragInst->GetArchetype()->GetMass());

                    Vector3 vecWindscreenImpulsePos;
                    Matrix34 m = RCC_MATRIX34(pNewFragInst->GetMatrix());
                    vecWindscreenImpulsePos.Set(m.d);
                    vecWindscreenImpulsePos.Add(sfWindscreenBreakSideOffset * vecParentRight);
                    vecWindscreenImpulsePos.Add(sfWindscreenBreakUpOffset * vecParentUp);

                    pWindscreenCollider->ApplyImpulse(vecWindscreenImpulse, vecWindscreenImpulsePos);
                }
            }

		}
	}
}


//
//
//
void CVehicleDamage::PopOffRoof(const Vector3 &vRoofImpulse)
{
    // pop out the roof
    if(m_pParent->GetBoneIndex(VEH_ROOF)!=-1 && m_pParent->GetFragInst())
    {
		m_pParent->PartHasBrokenOff(VEH_ROOF);

        int roofFrag = m_pParent->GetFragInst()->GetType()->GetComponentFromBoneIndex(
            m_pParent->GetVehicleFragInst()->GetCurrentPhysicsLOD(),m_pParent->GetBoneIndex(VEH_ROOF));

        if (!m_pParent->CarPartsCanBreakOff())
        {
            m_pParent->GetFragInst()->DeleteAbove(roofFrag);
        }
        else
        {
            fragInst* pNewFragInst = m_pParent->GetFragInst()->BreakOffAbove(roofFrag);

            phCollider* pRoofCollider = pNewFragInst ? CPhysics::GetSimulator()->GetCollider(pNewFragInst->GetLevelIndex()) : NULL;

            if(pNewFragInst && pRoofCollider)
            {
                Vector3 vecRoofImpulsePos;
                Matrix34 m = RCC_MATRIX34(pNewFragInst->GetMatrix());
                Vector3 vecRoofImpulse(vRoofImpulse);

                m.Transform3x3(vecRoofImpulse);
                vecRoofImpulse.Scale(pNewFragInst->GetArchetype()->GetMass());
                vecRoofImpulsePos.Set(m.d);

                pRoofCollider->ApplyImpulse(vecRoofImpulse, vecRoofImpulsePos);
            }
        }

		CVehicle::ClearLastBrokenOffPart();
    }
}

void CVehicleDamage::UpdateNetworkDamageTracker(CEntity* pInflictor, const float fDamage, const u32 uWeaponHash, const bool vehicleIsDestroyed, const bool isMeleeDamage, phMaterialMgr::Id nMaterialId) const
{
	//Ensure a network game is in progress.
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	//Ensure the parent is valid
	if(!m_pParent || m_pParent->IsNetworkClone())
	{
		return;
	}

	//Ensure the parent net object is valid and the it is not a network clone (network clones get damage from the network update synch trees)
	CNetObjPhysical* pParentNetObj = static_cast<CNetObjPhysical *>(m_pParent->GetNetworkObject());
	if(!pParentNetObj || pParentNetObj->IsClone())
	{
		return;
	}

	//If we've wrecked a vehicle, register it.... if the vehicle is a clone, the registration gets taken care of 
	//  elsewhere (NetObjPhysical::SetPhysicalHealthData)
	NetworkInterface::RegisterKillWithNetworkTracker(m_pParent);

	//Update the network damage tracker.
	if ((fDamage > 0.0f || NetworkInterface::ShouldTriggerDamageEventForZeroDamage(m_pParent)) && !vehicleIsDestroyed)
	{
		//Register Hits on Peds for explosion weapons - projectiles and such
		CStatsMgr::RegisterExplosionHit(pInflictor, m_pParent, uWeaponHash, fDamage);

		CNetObjPhysical::NetworkDamageInfo damageInfo(pInflictor, fDamage, 0.0f, 0.0f, false, uWeaponHash, -1, false, m_pParent->GetStatus() == STATUS_WRECKED, isMeleeDamage, nMaterialId);
		pParentNetObj->UpdateDamageTracker(damageInfo);
	}
}

void CVehicleDamage::SetPetrolTankLevel( float fPetrolLevel )
{
	m_fPetrolTankLevel = fPetrolLevel; 
	m_pParent->m_nVehicleFlags.bPetrolTankEmpty = (m_fPetrolTankLevel <= 0.0f);
}

void CVehicleDamage::Copy(CVehicle *srcVehicle, CVehicle* dstVehicle)
{
	if( srcVehicle != NULL && dstVehicle != NULL && srcVehicle != dstVehicle)
	{
		Assertf(srcVehicle->GetModelIndex() == dstVehicle->GetModelIndex(), "Copying damage from different models might lead to unfortunate consequences.");
		
		CVehicleDamage *srcDmg = srcVehicle->GetVehicleDamage();
		CVehicleDamage *dstDmg = dstVehicle->GetVehicleDamage();

#if ENABLE_FRAG_OPTIMIZATION
		if(srcDmg->m_fBodyHealth < dstVehicle->GetVehicleModelInfo()->GetDefaultBodyHealth())
	{
		dstVehicle->GiveFragCacheEntry(true);
	}
#endif
		// General values
		dstVehicle->SetHealth( srcVehicle->GetHealth() );

		dstDmg->m_fBodyHealth = srcDmg->m_fBodyHealth;
		dstDmg->m_fPetrolTankHealth = srcDmg->m_fPetrolTankHealth;
		dstDmg->m_fPetrolTankLevel = srcDmg->m_fPetrolTankLevel;
		dstDmg->m_fOilLevel = srcDmg->m_fOilLevel;
		dstDmg->m_pEntityThatSetUsOnFire = srcDmg->m_pEntityThatSetUsOnFire;
		dstDmg->m_LightState = srcDmg->m_LightState;
		dstDmg->m_UpdateLightBones = srcDmg->m_UpdateLightBones;
		dstDmg->m_UpdateSirenBones = srcDmg->m_UpdateSirenBones;
		dstDmg->m_SirenState = srcDmg->m_SirenState;
		sysMemCpy(dstDmg->m_BouncingPanels, srcDmg->m_BouncingPanels,sizeof(dstDmg->m_BouncingPanels));
		dstDmg->m_vPetrolSprayPosLocal = srcDmg->m_vPetrolSprayPosLocal;
		dstDmg->m_vPetrolSprayNrmLocal = srcDmg->m_vPetrolSprayNrmLocal;

		// Engine and suspension health and fire.
		if(dstDmg->m_fPetrolTankHealth>VEH_DAMAGE_HEALTH_PTANK_FINISHED && dstDmg->m_fPetrolTankHealth<VEH_DAMAGE_HEALTH_PTANK_ONFIRE)
		{
			CPed* pCulPrit = NULL;
			if (dstDmg->m_pEntityThatSetUsOnFire.Get())
			{
				CEntity* entityThatSetUsOnFire = dstDmg->m_pEntityThatSetUsOnFire.Get();

				if (entityThatSetUsOnFire->GetIsTypePed())
				{
					pCulPrit = static_cast< CPed* >(entityThatSetUsOnFire);
				}
				else if (entityThatSetUsOnFire->GetIsTypeVehicle())
				{
					CVehicle* vehicle = static_cast< CVehicle* >(entityThatSetUsOnFire);

					pCulPrit = vehicle->GetDriver();

					if (!pCulPrit && vehicle->GetSeatManager())
					{
						pCulPrit = vehicle->GetSeatManager()->GetLastPedInSeat(0);
					}
				}
			}

			g_vfxVehicle.ProcessPetrolTankDamage(dstDmg->m_pParent, pCulPrit);
		}
		
		const float srcEngineHealth = srcVehicle->m_Transmission.GetEngineHealth();
		dstVehicle->m_Transmission.SetEngineHealth(srcEngineHealth);
		
		// Wheels & Tires
		int numWheels = Min(dstVehicle->GetNumWheels(),srcVehicle->GetNumWheels());
		for(int i=0;i<numWheels;i++)
		{
			const CWheel *srcWheel = srcVehicle->GetWheel(i);
			CWheel *dstWheel = dstVehicle->GetWheel(i);

			const float srcSuspensionHealth = srcWheel->GetSuspensionHealth();
			const float srcTyreHealth = srcWheel->GetTyreHealth();
			dstWheel->SetSuspensionHealth(srcSuspensionHealth);
			dstWheel->SetTyreHealth(srcTyreHealth);
		}
		
		CCustomShaderEffectVehicle* srcCSE = static_cast<CCustomShaderEffectVehicle*>(srcVehicle->GetDrawHandler().GetShaderEffect());
		Assert(srcCSE);
		bool srcTyreDeform = srcCSE->GetEnableTyreDeform();
		
		CCustomShaderEffectVehicle* dstCSE = static_cast<CCustomShaderEffectVehicle*>(dstVehicle->GetDrawHandler().GetShaderEffect());
		Assert(dstCSE);
		bool prevDstTyreDeform = dstCSE->GetEnableTyreDeform();			
		dstCSE->SetEnableTyreDeform(srcTyreDeform);
		
		if( srcTyreDeform != prevDstTyreDeform)
		{
			dstVehicle->ActivatePhysics();
		}
		
		// Deformation
		CVehicleDeformation * srcDeformation = srcDmg->GetDeformation();
		CVehicleDeformation * dstDeformation = dstDmg->GetDeformation();
		if( srcDeformation->HasDamageTexture() )
		{
			if( !dstDeformation->HasDamageTexture() )
				dstDeformation->DamageTextureAllocate();
				
			// Allocation can fail...
			if( dstDeformation->HasDamageTexture() )
			{
				const void *srcPtr =  srcDeformation->LockDamageTexture(grcsRead);
				void *dstPtr = dstDeformation->LockDamageTexture(grcsWrite);

				if( dstPtr && srcPtr )
				{
					sysMemCpy(dstPtr,srcPtr, GTA_DAMAGE_TEXTURE_BYTES);

					srcDeformation->ApplyDamageToBound(dstPtr);
					dstVehicle->SetupWheels(dstPtr);

					dstCSE->SetEnableDamage(true);
					dstCSE->SetBoundRadius(srcCSE->GetBoundRadius());
				}
				
				
				if( dstPtr )
					dstDeformation->UnLockDamageTexture();

				if( srcPtr )
					srcDeformation->UnLockDamageTexture();
			}
		}
		
		// Broken off parts		
		fragInst* dstFragInst = dstVehicle->GetVehicleFragInst();
		fragInst* srcFragInst = srcVehicle->GetVehicleFragInst();

		if( dstFragInst && srcFragInst )
		{
			// doors
			for(int i=0; i<srcVehicle->GetNumDoors(); i++)
			{ 
				if(srcVehicle->GetDoor(i)->GetFragChild() > 0 && !srcVehicle->GetDoor(i)->GetIsIntact(srcVehicle) )
				{
					dstFragInst->DeleteAbove(dstVehicle->GetDoor(i)->GetFragChild());
				}
			}
		
			// wheels
			for(int i=0; i<srcVehicle->GetNumWheels(); i++)
			{
				CWheel* wheel = srcVehicle->GetWheel(i);
				int nFragChild = wheel->GetFragChild();
				if(nFragChild > -1)
				{
					fragPhysicsLOD* srcTypePhysics = srcFragInst->GetTypePhysics();
					fragTypeChild* srcChild = srcTypePhysics->GetAllChildren()[nFragChild];
					if(srcChild->GetOwnerGroupPointerIndex() > 0 && srcFragInst->GetChildBroken(nFragChild))
					{
						dstVehicle->PartHasBrokenOff(wheel->GetHierarchyId());
						dstFragInst->DeleteAbove(nFragChild);
						CVehicle::ClearLastBrokenOffPart();
					}
				}
			}

			// extras and misc

			// go thru all the fragment children (there's more of them)
			for(int nChild=0; nChild<srcFragInst->GetTypePhysics()->GetNumChildren(); nChild++)
			{
				if(!srcFragInst->GetChildBroken(nChild))
					continue;

				int boneIndex = srcFragInst->GetType()->GetBoneIndexFromID(srcFragInst->GetTypePhysics()->GetAllChildren()[nChild]->GetBoneID());

				// go through each of the possible extras
				for(int nExtra=VEH_EXTRA_1; nExtra<=VEH_LAST_EXTRA; nExtra++)
				{
					// if the bone index of this child matches the bone index of the extra that's turned off then delete this component
					if(srcVehicle->GetBoneIndex((eHierarchyId)nExtra) == boneIndex)
					{
						dstVehicle->PartHasBrokenOff((eHierarchyId)nExtra);
						dstFragInst->DeleteAbove(nChild);
						CVehicle::ClearLastBrokenOffPart();
						break;
					}
				}

				// misc components
				int nNumBreakableParts = NUM_MISC_CAR_BREAK_PARTS;
				eHierarchyId *pBreakableParts = aMiscCarBreakParts;
				if(srcVehicle->InheritsFromTrailer() && (static_cast<CTrailer*>(srcVehicle))->HasBreakableExtras())
				{
					nNumBreakableParts = NUM_EXTRA_TRAILER_BREAK_PARTS;
					pBreakableParts = aExtraTrailerBreakParts;
				}

				for(int nMisc=0; nMisc<nNumBreakableParts; nMisc++)
				{
					// if the bone index of this child matches the bone index of the extra that's turned off then delete this component
					if(srcVehicle->GetBoneIndex((eHierarchyId)pBreakableParts[nMisc]) == boneIndex)
					{
						dstVehicle->PartHasBrokenOff((eHierarchyId)pBreakableParts[nMisc]);
						dstFragInst->DeleteAbove(nChild);
						CVehicle::ClearLastBrokenOffPart();
						break;
					}
				}
			}
		}
	}
}

#if __DEV

const eHierarchyId* CVehicleDamage::GetGlassBoneHierarchyIds()
{
	return aGlassBones;
}

int CVehicleDamage::GetGlassBoneHierarchyIdCount()
{
	CompileTimeAssert(NELEM(aGlassBones) == NUM_GLASS_BONES);
	return NELEM(aGlassBones);
}

#endif // __DEV

void CVehicleDamage::DamageVehicleByDriver(const Vector3& impulseLocal, const Vector3& damagePosLocal, float damageAmount, eDamageType nDamageType, CVehicle* vehicle, bool fullDamage)
{
	if (vehicle == NULL)
	{
		return;
	}

	CPed* driver = vehicle->GetDriver();
	CEntity* pInflictor = static_cast<CEntity*>(driver);

	DamageVehicle(pInflictor, impulseLocal, damagePosLocal, damageAmount, nDamageType, vehicle, fullDamage);
}

void CVehicleDamage::DamageVehicle(CEntity* pInflictor, const Vector3& impulseLocal, const Vector3& damagePosLocal, float damageAmount, eDamageType nDamageType, CVehicle* vehicle, bool fullDamage)
{
	if (vehicle == NULL)
	{
		return;
	}

	Vector3 damagePosWorldSpace = vehicle->TransformIntoWorldSpace(damagePosLocal);
	
	Vector3 normalizedImpulse = impulseLocal;
	normalizedImpulse.NormalizeSafe();

	Vector3 modulatedImpulse = normalizedImpulse * damageAmount;
	Vector3 impulseWorldSpace = VEC3V_TO_VECTOR3(vehicle->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(modulatedImpulse)));

	// Deformation
	vehicle->GetVehicleDamage()->GetDeformation()->ApplyCollisionImpact(impulseWorldSpace, damagePosWorldSpace, pInflictor, fullDamage);

	// Damage
	vehicle->GetVehicleDamage()->ApplyDamage(pInflictor, nDamageType, 0, damageAmount, damagePosWorldSpace, normalizedImpulse, modulatedImpulse, 0);
}

void CVehicleDamage::AddVehicleExplosionDeformations(CVehicle* vehicle, CEntity* pCulprit, eDamageType nDamageType, int numberOfImpulses, float explosionDamageAmount)
{
	if ((numberOfImpulses < 1) || (explosionDamageAmount < 0.0f) || (vehicle == NULL) || (vehicle->pHandling == NULL) || !CVehicleDeformation::ms_bEnableExtraBoneDeformations)
	{
		return;
	}

#if VEHICLE_DEFORMATION_PROPORTIONAL
	float sizeMultiplier   = 0.0f;
	float multiplier       = vehicle->GetVehicleDamage()->GetDeformation()->GetDamageMultiplier(&sizeMultiplier);
	explosionDamageAmount *= multiplier;
#endif

	HeadOnCollision(vehicle, explosionDamageAmount, true);
	RearEndCollision(vehicle, explosionDamageAmount, true);
	numberOfImpulses -= 2; // always smash the front and rear of the vehicle

	if (numberOfImpulses <= 0)
	{
		return;
	}

	const float fMass = vehicle->GetMass();
	const float fMassIndependenceFactor = (fMass * InvertSafe(vehicle->pHandling->m_fDeformationDamageMult,0.0f));
	const float fDeformationMag = explosionDamageAmount * fMassIndependenceFactor;

	fragInstGta* fragInstPtr = vehicle->GetVehicleFragInst();
	fragPhysicsLOD* fragPhysicsLODPtr = fragInstPtr ? fragInstPtr->GetTypePhysics() : NULL;
	CVehicleModelInfo* modelInfo = (CVehicleModelInfo *)vehicle->GetBaseModelInfo();

	if (!fragPhysicsLODPtr || !modelInfo)
	{
		return;
	}

	static float randomAngleRange = 80.0f;
	
	int extraBoneCount = 0;
	const eHierarchyId* extraBones = vehicle->GetExtraBonesToDeform(extraBoneCount);

	if ((extraBones == NULL) || (extraBoneCount == 0))
	{
#if GPU_DAMAGE_WRITE_ENABLED
		vehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformations(false);
#else
		vehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformations(true);
#endif
		return;
	}

	extraBoneCount = Min(numberOfImpulses, extraBoneCount);

	for (int i = 0; i < extraBoneCount; ++i)
	{
		eHierarchyId componentId = extraBones[i];
		int iBoneId = modelInfo->GetBoneIndex(componentId);

		if (iBoneId !=-1 )
		{
			int nChildIndex   = fragInstPtr->GetComponentFromBoneIndex(iBoneId);

			if (nChildIndex == -1)
			{
				continue;
			}

			phBound* boundPtr = fragPhysicsLODPtr->GetCompositeBounds()->GetBound(nChildIndex);

			if ((boundPtr == NULL) || (boundPtr->GetType() != phBound::GEOMETRY))
			{
				continue;
			}

			phBoundGeometry* pOrigBoundGeom =  static_cast<phBoundGeometry*>(boundPtr);

			int numVertices         = pOrigBoundGeom->GetNumVertices();
			int vertexIndexToTarget = (int)fwRandom::GetRandomNumberInRange(0, numVertices);
			Vec3V vertPos           = pOrigBoundGeom->GetVertex(vertexIndexToTarget);
			Vector3 damagePosLocal  = VEC3V_TO_VECTOR3(vertPos);

			const Matrix34 boundMatrix = MAT34V_TO_MATRIX34(fragPhysicsLODPtr->GetCompositeBounds()->GetCurrentMatrix(nChildIndex));
			boundMatrix.Transform(damagePosLocal);

			Vector3 boundingBoxMin = VEC3V_TO_VECTOR3(pOrigBoundGeom->GetBoundingBoxMin());
			Vector3 boundingBoxMax = VEC3V_TO_VECTOR3(pOrigBoundGeom->GetBoundingBoxMax());
			Vector3 boundingCornerVec = boundingBoxMax - boundingBoxMin;

			bool isYMajorAxis = (boundingCornerVec.y >= boundingCornerVec.x);
			
			float randomAlpha = fwRandom::GetRandomNumberInRange(-randomAngleRange, randomAngleRange) / 360.0f;
			float randomTheta = fwRandom::GetRandomNumberInRange(-randomAngleRange, randomAngleRange) / 360.0f;

			float damagePerImpulse = fDeformationMag;

#if VEHICLE_DEFORMATION_PROPORTIONAL
			// Make it depend on the relative size of the vehicle, 0 = tailgater, 1 = titan
			float outwardDamageChance = VEHICLE_OUTWARD_EXPLOSION_CHANCE;
			outwardDamageChance *= sizeMultiplier;

			bool bOutwardDamage = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < outwardDamageChance;
			Vector3 impulseLocal = bOutwardDamage ? damagePosLocal : -damagePosLocal;

			if (bOutwardDamage)
			{
				damagePerImpulse /= 2.0f;
			}
#else
			Vector3 impulseLocal = -damagePosLocal;
#endif
			
			if (isYMajorAxis)
			{
				//Damage the vehicle along its main axes, not pointing towards the center
				impulseLocal.y = 0.0f;
				impulseLocal.z *= 0.2f;
			}
			else
			{
				impulseLocal.x = 0.0f;
				impulseLocal.y *= 0.4f; //probably wings, you mostly want to hit them downward or upward
			}

			impulseLocal.NormalizeSafe();
			impulseLocal.RotateAboutAxis(randomAlpha, isYMajorAxis ? 'y' : 'x');
			impulseLocal.RotateZ(randomTheta);

			CVehicleDamage::DamageVehicle(pCulprit, impulseLocal, damagePosLocal, damagePerImpulse, nDamageType, vehicle, true);
		}
	}

#if GPU_DAMAGE_WRITE_ENABLED
	vehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformations(false);
#else
	vehicle->GetVehicleDamage()->GetDeformation()->ApplyDeformations(true);
#endif
}

float CVehicleDamage::GetVehicleHealthPercentage(const CVehicle* pVehicle, float maxEngineHealth, float maxPetrolTankHealth, float maxHealth, float maxMainRotorHealth, float maxRearRotorHealth, float maxTailBoomHealth ) const
{
	//For vehicles that explode on contact, the body health is the most important aspect as as soon as that reaches 0 they explode.
	if(  pVehicle->ShouldExplodeOnContact() && 
	   !pVehicle->DisableExplodeOnContact())
	{
		float fEntityHealthPercent = Max(0.0f, pVehicle->GetHealth() / pVehicle->GetMaxHealth());
		float fEngineHealthPercent = Max(0.0f, pVehicle->GetVehicleDamage()->GetEngineHealth() / maxEngineHealth);
		float fGasTankHealthPercent = Max(0.0f, pVehicle->GetVehicleDamage()->GetPetrolTankHealth() / maxPetrolTankHealth);

		float fBodyHealth = (pVehicle->GetVehicleDamage()->GetBodyHealth() / maxHealth) * 100.0f;
		static dev_float sfBodyHealthProportion = 0.9f;

		float fHealth = (((fEntityHealthPercent + fEngineHealthPercent + fGasTankHealthPercent) / 3.0f) * (1.0f-sfBodyHealthProportion)) + fBodyHealth * sfBodyHealthProportion;
		
		if(fHealth > 100)
			fHealth = 100;

		return fHealth;
	}
	else
	{
		float fHealthPercent = pVehicle->GetHealth() / pVehicle->GetMaxHealth();
		float fEngineHealthPercent = pVehicle->GetVehicleDamage()->GetEngineHealth() / maxEngineHealth;
		float fGasTankHealthPercent = pVehicle->GetVehicleDamage()->GetPetrolTankHealth() / maxPetrolTankHealth;

		if( pVehicle->InheritsFromHeli() )
		{
			const CHeli* pHeli = static_cast< const CHeli* >( pVehicle );

			float fRotorHealth = pHeli->GetMainRotorHealth() / maxMainRotorHealth;

			if( fEngineHealthPercent > fRotorHealth )
			{
				fEngineHealthPercent = fRotorHealth;
			}

			fRotorHealth = pHeli->GetRearRotorHealth() / maxRearRotorHealth;
			if( fEngineHealthPercent > fRotorHealth )
			{
				fEngineHealthPercent = fRotorHealth;
			}

			fRotorHealth = pHeli->GetTailBoomHealth() / maxTailBoomHealth;
			if( fEngineHealthPercent > fRotorHealth )
			{
				fEngineHealthPercent = fRotorHealth;
			}
		}

		if(fEngineHealthPercent < 0)
			fEngineHealthPercent = 0;

		if(fGasTankHealthPercent < 0)
			fGasTankHealthPercent = 0;

		// Save whichever value out of the engine health and the petrol tank health is the lowest. Either one of these reaching the minimum will affect driveability.
		float fLowestDriveabilityPercent = fEngineHealthPercent < fGasTankHealthPercent ? fEngineHealthPercent : fGasTankHealthPercent;

		// Calculate the contribution of the aesthetic, non-impactful body damage and the damage to elements that actually affect the driveability.
		// The idea is to allow the body health to contribute to the overall vehicle health, while ensuring that the values that affect the vehicle's driveability ultimately dictate the health level.
		// So long as the body health is larger than the either engine or petrol tank health, it will contribute to a maximum of 50% of the overall vehicle health.
		// If the engine or petrol tank health drop below the body health, the contribution of the body health to the overall vehicle health is proportionally diminished such that 
		// when either the engine health or the petrol tank health is zero, the overall vehicle health is zero.
		float fHealthContribution = 0.5f;
		float fDriveabilityContribution = 0.5f;
	
		if(fLowestDriveabilityPercent < fHealthPercent)
		{
			fHealthContribution = 0.5f - (0.5f * ((fHealthPercent - fLowestDriveabilityPercent) / fHealthPercent));
			fDriveabilityContribution = 1.0f - fHealthContribution;
		}

		float fHealth = ((fHealthPercent * fHealthContribution) + (fLowestDriveabilityPercent * fDriveabilityContribution)) * 100.0f;

		if(fHealth > 100)
			fHealth = 100;

		return fHealth;
	}
}


float CVehicleDamage::GetDefaultBodyHealthMax(const CVehicle *pParentVehicle) const
{
	CVehicleModelInfo* pModelInfo = pParentVehicle->GetVehicleModelInfo();
	return (pModelInfo ? pModelInfo->GetDefaultBodyHealth() : Max(m_pParent->GetMaxHealth(), VEH_DAMAGE_HEALTH_STD));
}

void CVehicleDamage::SetDamageScales( float bodyDamageScale, float petrolTankDamageScale, float engineDamageScale )
{
	m_fBodyDamageScale = bodyDamageScale;
	m_fPetrolTankDamageScale = petrolTankDamageScale;

	m_pParent->m_Transmission.SetEngineDamageScale( engineDamageScale );

	if( m_pParent->GetSecondTransmission() )
	{
		m_pParent->GetSecondTransmission()->SetEngineDamageScale( engineDamageScale );
	}
}


void CVehicleDamage::SetHeliDamageScales( float mainRotorDamageScale, float rearRotorDamageScale, float tailBoomDamageScale )
{
	if( m_pParent->InheritsFromHeli() )
	{
		CHeli* pHeli = static_cast< CHeli* >( m_pParent );
		pHeli->SetMainRotorDamageScale( mainRotorDamageScale );
		pHeli->SetRearRotorDamageScale( rearRotorDamageScale );
		pHeli->SetTailBoomDamageScale( tailBoomDamageScale );
	}
}

void CVehicleDamage::ApplyDamageToBreakablePanels( eDamageType nDamageType, float fDamage, const Vector3& vecPosLocal, int nComponent )
{
    static dev_float sfBodyHealthMaxForBreakablePanel = 750.0f;
    static dev_float sfMinDamageToBreakPanel = 30.0f;
    static dev_float sfMaxDamageToBreakPanel = 150.0f;

    if( !m_pParent || !m_pParent->GetVehicleFragInst() )
    {
        return;
    }

    // if body health is less than some threshold and fDamage is greater than some other
    if( m_fBodyHealth < sfBodyHealthMaxForBreakablePanel &&
        fDamage > sfMinDamageToBreakPanel )
    {
        static dev_float sfBreakOffPanelProb = 0.4f;

        if( ( nDamageType == DAMAGE_TYPE_EXPLOSIVE ||
              nDamageType == DAMAGE_TYPE_BULLET ||
              nDamageType == DAMAGE_TYPE_COLLISION ) &&
            ( fDamage > sfMaxDamageToBreakPanel ||
              fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= sfBreakOffPanelProb ) )
        {
            eHierarchyId panelToBreakOff = VEH_INVALID_ID;

            for( int nPanel = VEH_FIRST_BREAKABLE_PANEL; nPanel <= VEH_LAST_BREAKABLE_PANEL; nPanel++ )
            {
                if( HasBoneCollidedWithComponent( (eHierarchyId)nPanel, nComponent ) )
                {
                    // store the panel to break off
                    panelToBreakOff = (eHierarchyId)nPanel;
                    break;
                }
            }

            if( panelToBreakOff == VEH_INVALID_ID &&
                nDamageType == DAMAGE_TYPE_COLLISION )
            {
                float closestDistance       = FLT_MAX;
                eHierarchyId closestPanel   = VEH_INVALID_ID;

                // find the closest panel to the impact and if it is close enough break it off
                for( int nPanel = VEH_FIRST_BREAKABLE_PANEL; nPanel <= VEH_LAST_BREAKABLE_PANEL; nPanel++ )
                {
                    if( m_pParent && m_pParent->GetVehicleFragInst() )
                    {
                        int iBoneIndex = m_pParent->GetBoneIndex( (eHierarchyId)nPanel );

                        if( iBoneIndex != -1 )
                        {
                            Vector3 bonePos = RCC_VECTOR3( m_pParent->GetSkeleton()->GetSkeletonData().GetBoneData( iBoneIndex )->GetDefaultTranslation() );
                            float distance = ( bonePos - vecPosLocal ).Mag2();

                            if( distance < closestDistance )
                            {
                                closestDistance = distance;
                                closestPanel = (eHierarchyId)nPanel;
                            }
                        }
                    }
                }

                static dev_float sfDistanceTollerance = 0.3f * 0.3f;

                if( closestPanel != VEH_INVALID_ID &&
                    closestDistance < sfDistanceTollerance )
                {
                    panelToBreakOff = closestPanel;
                }
            }

            if( panelToBreakOff != VEH_INVALID_ID )
            {
                // break off panel
                BreakOffPart( panelToBreakOff, nComponent, m_pParent->GetVehicleFragInst(), m_pParent->GetVehicleModelInfo()->GetVehicleExplosionInfo(), 
                                !m_pParent->CarPartsCanBreakOff(), 0, 0.1f );
            }
        }
    }
}


