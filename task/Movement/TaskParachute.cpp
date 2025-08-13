#include "Task/Movement/TaskParachute.h"

// Rage includes
#include "grcore/debugdraw.h"
#include "fwsys/timer.h"
#include "math/angmath.h"
#include "pheffects/wind.h"
#include "vector/vector3.h"
#include "phbound/boundcomposite.h"

// Game includes
#include "animation/FacialData.h"
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/base/BaseCamera.h"
#include "Camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/gameplay/aim/AimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "Control/Gamelogic.h"
#include "Control/Replay/Replay.h"
#include "Event/Event.h"
#include "Event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Game/Config.h"
#include "Game/ModelIndices.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/pedDefines.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/rendering/PedVariationPack.h"
#include "physics/gtaInst.h"
#include "physics/physics_channel.h"
#include "physics/WorldProbe/worldprobe.h"
#include "physics/physics.h"
#include "renderer/Water.h"
#include "scene/world/gameworld.h"
#include "script/script.h"
#include "shaders/CustomShaderEffectProp.h"
#include "system/controlMgr.h"
#include "Task/General/TaskBasic.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Physics/TaskNMBuoyancy.h"
#include "Task/Physics/TaskNMFallDown.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "task/Motion/Locomotion/TaskInWater.h"
#include "Task/Movement/TaskFall.h"
#include "task/Movement/TaskGetUp.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskParachuteObject.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "task/Physics/TaskBlendFromNM.h"
#include "task/Vehicle/TaskVehicleWeapon.h"
#include "debug/DebugScene.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "vfx/systems/VfxPed.h"
#include "stats/StatsTypes.h"
#include "stats/StatsInterface.h"
#include "AI/Ambient/ConditionalAnimManager.h"
#include "network/objects/prediction/NetBlenderPed.h"
#include "fwnet/netobject.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Frontend/MobilePhone.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Task/Movement/TaskTakeOffPedVariation.h"
#include "Weapons/AirDefence.h"
#include "Weapons/Explosion.h"

AI_OPTIMISATIONS()
ANIM_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

//----------------------------------------------------------------------
RAGE_DEFINE_CHANNEL(parachute)
//----------------------------------------------------------------------

#define NUM_FRAMES_PER_FALLING_PROBE 1

const float CClonedParachuteInfo::MAXIMUM_VELOCITY_VALUE = 150.f;

//////////////////////////////////////////////////////////////////////////
// Class CClonedParachuteInfo
//////////////////////////////////////////////////////////////////////////

u8 CClonedParachuteInfo::GetPackedLandingType(s8 landingType) const
{
	return u8(landingType + 1);
}

s8 CClonedParachuteInfo::GetUnPackedLandingType(u8 landingType) const
{
	return s8(landingType - 1);
}

CClonedParachuteInfo::CClonedParachuteInfo( CClonedParachuteInfo::InitParams const & _initParams )
:
	m_pParachuteObject(_initParams.m_parachute),
	m_iFlags(_initParams.m_iFlags),
	m_fCurrentHeading(_initParams.m_fCurrentHeading),
	m_fCurrentX(_initParams.m_fCurrentX),
	m_fCurrentY(_initParams.m_fCurrentY),
	m_fTotalStickInput(_initParams.m_fTotalStickInput),
	m_fRoll(_initParams.m_fRoll),
	m_fPitch(_initParams.m_fPitch),
	m_fLeftBrake(_initParams.m_fLeftBrake),
	m_fRightBrake(_initParams.m_fRightBrake),
	m_bLeaveSmokeTrail(_initParams.m_bLeaveSmokeTrail),
	m_bCanLeaveSmokeTrail(_initParams.m_bCanLeaveSmokeTrail),
	m_SmokeTrailColor(_initParams.m_SmokeTrailColor),
	m_iSkydiveTransition(_initParams.m_iSkydiveTransition),
	m_landingType(GetPackedLandingType(_initParams.m_landingType)),
	m_parachuteLandingFlags(_initParams.m_parachuteLandingFlags),
	m_crashLandWithParachute(_initParams.m_crashLandWithParachute),
	m_pedVelocity(_initParams.m_pedVelocity),
	m_bIsParachuteOut(_initParams.m_bIsParachuteOut)
{
	// bit field check...
	Assertf(GetPackedLandingType(_initParams.m_landingType)	<	CTaskParachute::LandingData::Max+1,	"ERROR : invalid value in m_landingType");
	Assertf(_initParams.m_iSkydiveTransition				<	CTaskParachute::ST_Max,				"ERROR : invalid value in skydiveTransition");
	Assertf(_initParams.m_parachuteLandingFlags				<	(1 << 4),							"ERROR : Not enough bits in CClonedParachuteInfo to store m_parachuteLandingFlags");
	Assertf(_initParams.m_iFlags							<	(1 << 7),							"ERROR : Not enough bits in CClonedParachuteInfo to store m_iFlags");

	SetStatusFromMainTaskState(_initParams.m_state);
}

CClonedParachuteInfo::CClonedParachuteInfo()
:
	m_pParachuteObject(NULL),
	m_iFlags(0),
	m_fCurrentHeading(0.0f),
	m_fCurrentX(0.0f),
	m_fCurrentY(0.0f),
	m_fTotalStickInput(0.0f),
	m_fRoll(0.0f),
	m_fPitch(0.0f),
	m_fLeftBrake(0.0f),
	m_fRightBrake(0.0f),
	m_bLeaveSmokeTrail(false),
	m_bCanLeaveSmokeTrail(false),
	m_SmokeTrailColor(Color32(1.0f,1.0f,1.0f,1.0f)),
	m_iSkydiveTransition(0),
	m_landingType(CTaskParachute::LandingData::Max),
	m_parachuteLandingFlags(0),
	m_crashLandWithParachute(false),
	m_pedVelocity(VEC3_ZERO),
	m_bIsParachuteOut(false)
{}

CClonedParachuteInfo::~CClonedParachuteInfo()
{}

CTaskFSMClone* CClonedParachuteInfo::CreateCloneFSMTask()
{
	return rage_new CTaskParachute( m_iFlags );
}

void CClonedParachuteInfo::Serialise(CSyncDataBase& serialiser)
{
	CSerialisedFSMTaskInfo::Serialise(serialiser);

	ObjectId targetID = m_pParachuteObject.GetEntityID();
	SERIALISE_OBJECTID(serialiser, targetID, "Parachute Entity");
	m_pParachuteObject.SetEntityID(targetID);

	u8 flags = m_iFlags;
	SERIALISE_UNSIGNED(serialiser, 				flags,							datBitsNeeded<CTaskParachute::PF_MAX_Flags>::COUNT,			"Flags");
	m_iFlags = flags;

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fCurrentHeading,				1.0f,	SIZEOF_CURRENT_HEADING,								"Current Heading");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fCurrentX,					1.0f,	SIZEOF_CURRENT_X,									"Current X");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fCurrentY,					1.0f,	SIZEOF_CURRENT_Y,									"Current Y");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fTotalStickInput,				1.0f,	SIZEOF_TOTAL_STICK_INPUT,							"Total Stick Input");
	SERIALISE_PACKED_FLOAT(serialiser, 			m_fRoll,						2.0f,	SIZEOF_ROLL,										"Roll");
	SERIALISE_PACKED_FLOAT(serialiser, 			m_fPitch,						2.0f,	SIZEOF_PITCH,										"Pitch");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fLeftBrake,					1.0f,	SIZEOF_LEFT_RIGHT_BRAKE,							"Left Brake");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fRightBrake,					1.0f,	SIZEOF_LEFT_RIGHT_BRAKE,							"Right Brake");

	bool leaveSmokeTrail = m_bLeaveSmokeTrail;
	SERIALISE_BOOL(serialiser, 					leaveSmokeTrail,																			"Leave Smoke Trail");
	m_bLeaveSmokeTrail = leaveSmokeTrail;

	bool canLeaveSmokeTrail = m_bCanLeaveSmokeTrail;
	SERIALISE_BOOL(serialiser, 					canLeaveSmokeTrail,																			"Can Leave Smoke Trail");
	m_bCanLeaveSmokeTrail = canLeaveSmokeTrail;

	float fR = m_SmokeTrailColor.GetRedf();
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, fR,								1.0f,	SIZEOF_COLOR_COMPONENT,								"Smoke Trail Color (R)");
	float fG = m_SmokeTrailColor.GetGreenf();
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, fG,								1.0f,	SIZEOF_COLOR_COMPONENT,								"Smoke Trail Color (G)");
	float fB = m_SmokeTrailColor.GetBluef();
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, fB,								1.0f,	SIZEOF_COLOR_COMPONENT,								"Smoke Trail Color (B)");
	m_SmokeTrailColor.Setf(fR, fG, fB);

	u8 skydiveTransition = m_iSkydiveTransition;
	SERIALISE_UNSIGNED(serialiser, skydiveTransition,							datBitsNeeded<CTaskParachute::ST_Max>::COUNT,				"Skydive Transition");
	m_iSkydiveTransition = skydiveTransition;

	// We use +1 in the datBitsNeeded here as we pack and unpack the landingType value to convert it from -1 to 5 to 0 to 6...
	u32 landingType = m_landingType;
	SERIALISE_UNSIGNED(serialiser, landingType,									datBitsNeeded<CTaskParachute::LandingData::Max+1>::COUNT,	"Landing Type");
	m_landingType = landingType;

	u8 parachuteLandingFlags = m_parachuteLandingFlags;
	SERIALISE_UNSIGNED(serialiser, parachuteLandingFlags,						datBitsNeeded<CTaskParachute::PLF_ReduceMass>::COUNT,		"Parachute Landing Flags");
	m_parachuteLandingFlags = parachuteLandingFlags;

	bool crashLandWithParachute = m_crashLandWithParachute;
	SERIALISE_BOOL(serialiser, crashLandWithParachute,																						"Crash Land With Parachute");
	m_crashLandWithParachute = crashLandWithParachute;

	SERIALISE_PACKED_FLOAT(serialiser, m_pedVelocity.x, MAXIMUM_VELOCITY_VALUE, SIZEOF_PED_VELOCITY_COMP_X, "Ped Velocity X"); 
	SERIALISE_PACKED_FLOAT(serialiser, m_pedVelocity.y, MAXIMUM_VELOCITY_VALUE, SIZEOF_PED_VELOCITY_COMP_Y, "Ped Velocity Y"); 
	SERIALISE_PACKED_FLOAT(serialiser, m_pedVelocity.z, MAXIMUM_VELOCITY_VALUE, SIZEOF_PED_VELOCITY_COMP_Z, "Ped Velocity Z");

	bool temp = m_bIsParachuteOut;
	SERIALISE_BOOL(serialiser, temp, "Is Parachute Out");
	m_bIsParachuteOut = temp;
}

//////////////////////////////////////////////////////////////////////////
// Class CTaskParachute
//////////////////////////////////////////////////////////////////////////
// Initialise static variables

bank_s32 CTaskParachute::ms_iSeatToParachuteFromVehicle = Seat_driver;

bank_float CTaskParachute::ms_interpRateHeadingSkydive = 0.5f;

bank_float CTaskParachute::ms_fBrakingMult = 20.0f;
bank_float CTaskParachute::ms_fMaxBraking = 40.0f;

bank_float CTaskParachute::ms_fApplyCgOffsetMult = 5.0f;

bank_float CTaskParachute::ms_fAttachDepth = 0.125f;
bank_float CTaskParachute::ms_fAttachLength = 5.2f;

#if __BANK
bool CTaskParachute::ms_bVisualiseLandingScan = false;
bool CTaskParachute::ms_bVisualiseCrashLandingScan = false;
bool CTaskParachute::ms_bVisualiseControls = false;
bool CTaskParachute::ms_bVisualiseAltitude = false;
bool CTaskParachute::ms_bVisualiseInfo = false;
float CTaskParachute::ms_fDropHeight = 1000.0f;
bool CTaskParachute::ms_bCanLeaveSmokeTrail = false;
Color32	CTaskParachute::ms_SmokeTrailColor = Color_white;
#endif

Vector3	CTaskParachute::ms_vAdditionalAirSpeedNormalMin = Vector3(0.0f, 0.0f, 3.0f);
Vector3	CTaskParachute::ms_vAdditionalAirSpeedNormalMax = Vector3(0.0f, 0.0f, 0.0f);
Vector3	CTaskParachute::ms_vAdditionalDragNormalMin = Vector3(2.0f, 6.5f, 2.0f);
Vector3	CTaskParachute::ms_vAdditionalDragNormalMax = Vector3(2.0f, 6.5f, 2.0f);
Vector3	CTaskParachute::ms_vAdditionalAirSpeedBrakingMin = Vector3(0.0f, -12.0f, -2.0f);
Vector3	CTaskParachute::ms_vAdditionalAirSpeedBrakingMax = Vector3(0.0f, -5.0f, 0.0f);
Vector3	CTaskParachute::ms_vAdditionalDragBrakingMin = Vector3(2.0f, 2.0f, 2.0f);
Vector3	CTaskParachute::ms_vAdditionalDragBrakingMax = Vector3(2.0f, 100.0f, 2.0f);


// SKYDIVING
CFlightModelHelper CTaskParachute::ms_SkydivingFlightModelHelper(
	0.0f,	// Thrust
	0.0f,	// Yaw rate
	0.0f,	// Pitch rate
	0.0f,	// Roll rate
	0.0f,	// Yaw stabilise
	0.0f,	// Pitch stabilise
	0.0f,	// Roll stabilise
	0.0f,	// Form lift mult
	0.001f,	// Attack lift mult
	0.001f, // Attack dive mult
	0.0f,	// Side slip mult
	0.0f,	// throttle fall off,
	Vector3(1000.0f,0.0f,0.0f),	// ConstV
	Vector3(1000.0f,0.5f,5.0f),	// LinearV
	Vector3(1000.0f,4.0f,5.0f),	// SquaredV
	Vector3(   0.0f,0.0f,0.0f),	// ConstAV
	Vector3(   0.0f,0.0f,0.0f),	// LinearAV
	Vector3(   0.0f,0.0f,0.0f)	// SquaredAV
	);

// DEPLOYING
CFlightModelHelper CTaskParachute::ms_DeployingFlightModelHelper(
	0.0f,	// Thrust
	0.0f,	// Yaw rate
	0.0f,	// Pitch rate
	0.0f,	// Roll rate
	0.0f,	// Yaw stabilise
	0.0f,	// Pitch stabilise
	0.0f,	// Roll stabilise
	0.0f,	// Form lift mult
	0.01f,	// Attack lift mult
	0.01f, // Attack dive mult
	0.0f,	// Side slip mult
	0.0f,	// throttle fall off,
	Vector3(0.0f,0.0f,0.0f),	// ConstV
	Vector3(0.5f,0.5f,5.0f),	// LinearV
	Vector3(4.0f,4.0f,5.0f),	// SquaredV
	Vector3(0.0f,0.0f,0.0f),	// ConstAV
	Vector3(0.0f,0.0f,0.0f),	// LinearAV
	Vector3(0.0f,0.0f,0.0f)		// SquaredAV
	);

// PARACHUTING
CFlightModelHelper CTaskParachute::ms_FlightModelHelper(
	0.0f,	// Thrust
	1.0f,	// Yaw rate
	0.005f,	// Pitch rate
	0.005f,	// Roll rate
	1.0f,	// Yaw stabilise
	0.011f,	// Pitch stabilise
	0.020f,	// Roll stabilise
	0.002f,	// Form lift mult
	0.015f,	// Attack lift mult
	0.015f, // Attack dive mult
	0.0f,	// Side slip mult
	0.0f,	// throttle fall off,
	Vector3(0.0f,0.0f,0.0f),	// ConstV
	Vector3(0.0f,0.0f,0.0f),	// LinearV
	Vector3(0.0f,0.0f,0.0f),	// SquaredV
	Vector3(1.0f,1.0f,1.0f),	// ConstAV
	Vector3(1.0f,1.0f,1.0f),	// LinearAV
	Vector3(0.0f,0.0f,0.0f)		// SquaredAV
	);

// PARACHUTING
CFlightModelHelper CTaskParachute::ms_BrakingFlightModelHelper = CTaskParachute::ms_FlightModelHelper;
	
CTaskParachute::Tunables CTaskParachute::sm_Tunables;

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskParachute, 0x06ff9f98);

CTaskParachute::CTaskParachute(s32 iFlags) :
m_LandingData(),
m_vTargetPosition(V_ZERO),
m_vAverageVelocity(V_ZERO),
m_vSyncedVelocity(VEC3_ZERO),
m_vRawStick(0.0f, 0.0f),
m_fRawLeftBrake(0.0f),
m_fRawRightBrake(0.0f),
m_fStickX(0.0f),
m_fStickY(0.0f),
m_fTotalStickInput(0.0f),
m_fCurrentHeading(0.0f),
m_nSkydiveTransition(ST_None),
m_pParachuteObject(NULL),
m_parachuteObjectId(NETWORK_INVALID_OBJECT_ID),
m_pTargetEntity(NULL),
m_fLeftBrake(0.0f),
m_fRightBrake(0.0f),
m_fPitch(0.0f),
m_fRoll(0.0f),
m_fYaw(0.0f),
m_fParachuteThrust(0.0f),
m_iFlags(iFlags),
m_iParachuteModelIndex(fwModelId::MI_INVALID),
m_fBrakingMult(0.0f),
m_fAngleDifferenceZLastFrame(FLT_MAX),
m_fLeftHandGripWeight(1.0f),
m_fRightHandGripWeight(1.0f),
m_uProbeFlags(0),
m_iPedLodDistanceCache(0),
m_uNextTimeToProbeForParachuteIntersections(0),
m_bIsBraking(false),
m_bLandedProperly(false),
m_bRemoveParachuteForScript(false),
m_bForcePedToOpenChute(false),
m_cloneSubtaskLaunched(false),
m_bLeaveSmokeTrail(false),
m_bParachuteHasControlOverPed(false),
m_bCanEarlyOutForMovement(false),
m_bIncrementedStatSmokeTrail(false),
m_bIsLodChangeAllowedThisFrame(false),
m_bShouldUseLowLod(false),
m_bIsUsingLowLod(false),
m_bInactiveCollisions(false),
m_bCloneParachuteConfigured(false),
m_bCloneRagdollTaskRelaunch(false),
m_uCloneRagdollTaskRelaunchState(-1),
m_bCanCheckForPadShakeOnDeploy(true),
m_bIsUsingFirstPersonCameraThisFrame(false),
m_bDriveByClipSetsLoaded(false),
m_preSkydivingVelocity(VEC3_ZERO),
m_postSkydivingVelocity(VEC3_ZERO),
m_preDeployingVelocity(VEC3_ZERO),
m_postDeployingVelocity(VEC3_ZERO),
m_isOwnerParachuteOut(false),
m_finalDistSkydivingToDeploy(0.0f),
m_finalDistDeployingToParachuting(0.0f),
m_fFirstPersonCameraHeading(0.5f),
m_fFirstPersonCameraPitch(0.5f),
m_bAirDefenceExplosionTriggered(false),
m_uTimeAirDefenceExplosionTriggered(0),
m_pedChangingOwnership(false),
m_cachedParachuteState(0),
m_vAirDefenceFiredFromPosition(V_ZERO)
#if !__FINAL
,m_maxdist(0.0f),
m_total(0.0f),
m_count(0)
#endif
{
	parachuteDebugf1("Frame %d : %s : this %p", fwTimer::GetFrameCount(), __FUNCTION__, this);	

	m_FallingGroundProbeResults.Reset();
	SetInternalTaskType(CTaskTypes::TASK_PARACHUTE);
}
CTaskParachute::~CTaskParachute()
{
	parachuteDebugf1("Frame %d : %s : this %p", fwTimer::GetFrameCount(), __FUNCTION__, this);	
}

void CTaskParachute::Debug() const
{
#if DEBUG_DRAW
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Check if the ped is a focus entity.
	bool bIsFocusEntity = CDebugScene::FocusEntities_IsInGroup(pPed);
	
	//Check if we should visualise the controls.
	if(bIsFocusEntity && ms_bVisualiseControls)
	{
		static float fScale = 0.15f;
		static float fHalfScale = fScale * 0.5f;
		static float fThirdScale = fScale * 0.33f;

		static Vector2 vPitchDebugPos(0.25f, 0.15f);
		static Vector2 vRollDebugPos(vPitchDebugPos.x - fHalfScale, vPitchDebugPos.y + fThirdScale);
		static Vector2 vYawDebugPos(vRollDebugPos.x, vRollDebugPos.y + fThirdScale);
		static Vector2 vLeftBrakeDebugPos(0.75f, 0.15f);
		static Vector2 vRightBrakeDebugPos(vLeftBrakeDebugPos.x + fHalfScale, vLeftBrakeDebugPos.y);
		
		static Vector2 vStickYDebugPos(0.25f, 0.5f);
		static Vector2 vStickXDebugPos(vStickYDebugPos.x - fHalfScale, vStickYDebugPos.y + fHalfScale);
		static Vector2 vLeftTriggerDebugPos(0.75f, 0.5f);
		static Vector2 vRightTriggerDebugPos(vLeftTriggerDebugPos.x + fHalfScale, vLeftTriggerDebugPos.y);

		static float fEndWidth = 0.01f;

		grcDebugDraw::Meter(vPitchDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Pitch");
		grcDebugDraw::MeterValue(vPitchDebugPos, Vector2(0.0f,1.0f), fScale, m_fPitch, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vRollDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Roll");
		grcDebugDraw::MeterValue(vRollDebugPos, Vector2(1.0f,0.0f), fScale, m_fRoll, fEndWidth, Color32(255,0,0));
		
		grcDebugDraw::Meter(vYawDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Yaw");
		grcDebugDraw::MeterValue(vYawDebugPos, Vector2(1.0f,0.0f), fScale, m_fYaw, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vLeftBrakeDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Left Brake");
		grcDebugDraw::MeterValue(vLeftBrakeDebugPos, Vector2(0.0f,1.0f), fScale, m_fLeftBrake, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vRightBrakeDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Right Brake");
		grcDebugDraw::MeterValue(vRightBrakeDebugPos, Vector2(0.0f,1.0f), fScale, m_fRightBrake, fEndWidth, Color32(255,0,0));
		
		grcDebugDraw::Meter(vStickYDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Stick Y");
		grcDebugDraw::MeterValue(vStickYDebugPos, Vector2(0.0f,1.0f), fScale, m_vRawStick.y, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vStickXDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Stick X");
		grcDebugDraw::MeterValue(vStickXDebugPos, Vector2(1.0f,0.0f), fScale, m_vRawStick.x, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vLeftTriggerDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Left Trigger");
		grcDebugDraw::MeterValue(vLeftTriggerDebugPos, Vector2(0.0f,1.0f), fScale, m_fRawLeftBrake, fEndWidth, Color32(255,0,0));

		grcDebugDraw::Meter(vRightTriggerDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Right Trigger");
		grcDebugDraw::MeterValue(vRightTriggerDebugPos, Vector2(0.0f,1.0f), fScale, m_fRawRightBrake, fEndWidth, Color32(255,0,0));
	}
	
	if(bIsFocusEntity && ms_bVisualiseInfo)
	{
		if (m_pParachuteObject)
		{
			char szBuffer[128];
			int iLines = 0;
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			
			Vector3 vVelocity = m_pParachuteObject->GetVelocity();
			formatf(szBuffer, "Velocity: %.2f, %.2f, %.2f (Magnitude: %.2f)", vVelocity.x, vVelocity.y, vVelocity.z, vVelocity.Mag());
			grcDebugDraw::Text(vPedPosition,Color_red,0,iLines++*grcDebugDraw::GetScreenSpaceTextHeight(),szBuffer);
			
			formatf(szBuffer, "Speed (XY): %.2f", vVelocity.XYMag());
			grcDebugDraw::Text(vPedPosition,Color_red,0,iLines++*grcDebugDraw::GetScreenSpaceTextHeight(),szBuffer);
			
			Vector3 vAngVelocity = m_pParachuteObject->GetAngVelocity();
			formatf(szBuffer, "Ang Velocity: %.2f, %.2f, %.2f (Magnitude: %.2f)", vAngVelocity.x, vAngVelocity.y, vAngVelocity.z, vAngVelocity.Mag());
			grcDebugDraw::Text(vPedPosition,Color_red,0,iLines++*grcDebugDraw::GetScreenSpaceTextHeight(),szBuffer);

			float fCollisionImpulseMag = m_pParachuteObject->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame();
			formatf(szBuffer, "Collision Impulse Mag: %.2f", fCollisionImpulseMag);
			grcDebugDraw::Text(vPedPosition,Color_red,0,iLines++*grcDebugDraw::GetScreenSpaceTextHeight(),szBuffer);

			const float fRoll = m_pParachuteObject->GetTransform().GetRoll();
			const float fPitch = m_pParachuteObject->GetTransform().GetPitch();
			formatf(szBuffer, "Roll: %.2f, Pitch: %.2f", fRoll, fPitch);
			grcDebugDraw::Text(vPedPosition,Color_red,0,iLines++*grcDebugDraw::GetScreenSpaceTextHeight(),szBuffer);
		}
	}

	if(bIsFocusEntity && ms_bVisualiseAltitude)
	{
		static const Vector2 vAltDrawPos(0.75f,0.75f);
		static const float fBigHandLength = 20.0f/240.0f;
		static const float fSmallHandLength = 10.0f/240.0f;
		static const float fBigHandScale = 100.0f;
		static const float fSmallHandScale = 1000.0f;
		static const Vector2 UnitVector = grcDebugDraw::Get2DAspect();

		const float fSecondSurfaceInterp=0.0f;

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const float fGroundZ = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp, vPedPosition);
		const float fCurrentAltitude = vPedPosition.z - fGroundZ;

		// Draw altitude on a dial
		// Big hand 
		float fAngle1 = (fCurrentAltitude * TWO_PI / fBigHandScale);
		// Small hand
		float fAngle2 = (fCurrentAltitude * TWO_PI / fSmallHandScale);

		Vector2 vDrawPoint(rage::Sinf(fAngle1),-rage::Cosf(fAngle1));
		vDrawPoint.Scale(fBigHandLength);
		vDrawPoint.Multiply(UnitVector);
		grcDebugDraw::Line(vAltDrawPos,vAltDrawPos+vDrawPoint,Color_red);

		vDrawPoint.Set(rage::Sinf(fAngle2),-rage::Cosf(fAngle2));
		vDrawPoint.Scale(fSmallHandLength);
		vDrawPoint.Multiply(UnitVector);
		grcDebugDraw::Line(vAltDrawPos,vAltDrawPos+vDrawPoint,Color_red2);

		// Show movement speed
		static Vector2 vMoveSpeedMeterPos(0.9f,0.75f);
		static float fScale = 0.2f;
		static float fEndWidth = 0.01f;
		static float fVelocityScale = 50.0f;

		float fVelocity = m_pParachuteObject ? m_pParachuteObject->GetVelocity().Mag() : pPed->GetVelocity().Mag();

		char velString[32];
		formatf(velString,"Velocity: %.2f m/s",fVelocity);

		fVelocity /= fVelocityScale;
		fVelocity = rage::Clamp(fVelocity,0.0f,1.0f);		

		grcDebugDraw::Meter(vMoveSpeedMeterPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),velString);
		grcDebugDraw::MeterValue(vMoveSpeedMeterPos, Vector2(0.0f,1.0f),fScale, 2.0f*(fVelocity-0.5f), fEndWidth, Color32(255,0,0));	
	}
	
	//Check if we have a target.
	if(m_iFlags.IsFlagSet(PF_HasTarget))
	{
		//Grab the ped position.
		Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		
		//Calculate the target position.
		Vec3V vTargetPosition = CalculateTargetPosition();
		
		//Draw the markers.
		grcDebugDraw::Line(vPedPosition, vTargetPosition, Color_blue);
		grcDebugDraw::Sphere(vTargetPosition, 0.025f, Color_green);
		
		//Calculate the future target position.
		Vec3V vFutureTargetPosition = CalculateTargetPositionFuture();
		
		//Check if the future and current target positions differ.
		if(!IsEqualAll(vFutureTargetPosition, vTargetPosition))
		{
			//Draw the markers.
			grcDebugDraw::Line(vPedPosition, vFutureTargetPosition, Color_red);
			grcDebugDraw::Sphere(vTargetPosition, 0.025f, Color_green);
		}
	}
#endif
}

void CTaskParachute::ClearParachutePackVariationForPed(CPed& rPed)
{
	//Ensure the parachute pack variation is valid.
	const PedVariation* pVariation = FindParachutePackVariationForPed(rPed);
	if(!pVariation)
	{
		return;
	}

	//Ensure the parachute pack variation is active.
	u32 uDrawableId = rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(pVariation->m_Component);
	if(uDrawableId != pVariation->m_DrawableId)
	{
		return;
	}
	u8 uDrawableAltId = rPed.GetPedDrawHandler().GetVarData().GetPedCompAltIdx(pVariation->m_Component);
	if(uDrawableAltId != pVariation->m_DrawableAltId)
	{
		return;
	}
	u8 uTexId = rPed.GetPedDrawHandler().GetVarData().GetPedTexIdx(pVariation->m_Component);
	if(uTexId != pVariation->m_TexId)
	{
		return;
	}

	// Grab the previous variation information if it was for this component
	u32 uVariationDrawableId = 0;
	u32 uVariationDrawableAltId = 0;
	u32 uVariationTexId = 0;
	u32 uVariationPaletteId = 0;

	CPlayerInfo* pPlayerInfo = rPed.GetPlayerInfo();
	if(pPlayerInfo)
	{
		if(pPlayerInfo->GetPreviousVariationComponent() == pVariation->m_Component)
		{
			pPlayerInfo->GetPreviousVariationInfo(uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, uVariationPaletteId);
		}

		pPlayerInfo->ClearPreviousVariationData();
	}

	//Clear the parachute pack variation.
	rPed.SetVariation(pVariation->m_Component, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId, uVariationPaletteId, 0, false);
    rPed.FinalizeHeadBlend();
}

CObject* CTaskParachute::GetParachuteForPed(const CPed& rPed)
{
	//Ensure the ped is parachuting.
	if(!rPed.GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
	{
		return NULL;
	}

	//Ensure the task is valid.
	const CTaskParachute* pTask = static_cast<CTaskParachute *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
	if(!pTask)
	{
		return NULL;
	}

	return pTask->GetParachute();
}

bool CTaskParachute::GetParachutePackVariationForPed(const CPed& rPed, ePedVarComp& nComponent, u8& uDrawableId, u8& uDrawableAltId, u8& uTexId)
{
	//Ensure the parachute pack variation is valid.
	const PedVariation* pVariation = FindParachutePackVariationForPed(rPed);
	if(!pVariation)
	{
		return false;
	}

	//Set the values.
	nComponent	= pVariation->m_Component;
	uDrawableId	= (u8)pVariation->m_DrawableId;
	uDrawableAltId = (u8)pVariation->m_DrawableAltId;
	uTexId = (u8)pVariation->m_TexId;

	return true;
}

u32 CTaskParachute::GetTintIndexFromParachutePackVariationForPed(const CPed& rPed)
{
	//Assert that the parachute pack variation is active.
	taskAssert(IsParachutePackVariationActiveForPed(rPed));

	//Ensure the parachute pack variation is valid.
	const PedVariation* pVariation = FindParachutePackVariationForPed(rPed);
	if(!pVariation)
	{
		return UINT_MAX;
	}

	return CPedVariationPack::GetPaletteVar(&rPed, pVariation->m_Component);
}

bool CTaskParachute::IsParachutePackVariationActiveForPed(const CPed& rPed)
{
	//Ensure the parachute pack variation is valid.
	const PedVariation* pVariation = FindParachutePackVariationForPed(rPed);
	if(!pVariation)
	{
		return false;
	}
	
	//Ensure the parachute pack variation is active.
	u32 uDrawableId = rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(pVariation->m_Component);
	if(uDrawableId != pVariation->m_DrawableId)
	{
		return false;
	}
	u8 uDrawableAltId = rPed.GetPedDrawHandler().GetVarData().GetPedCompAltIdx(pVariation->m_Component);
	if(uDrawableAltId != pVariation->m_DrawableAltId)
	{
		return false;
	}
	u8 uTexId = rPed.GetPedDrawHandler().GetVarData().GetPedTexIdx(pVariation->m_Component);
	if(uTexId != pVariation->m_TexId)
	{
		return false;
	}
	
	return true;
}

bool CTaskParachute::IsPedDependentOnParachute(const CPed& rPed)
{
	//Ensure the ped is parachuting.
	if(!rPed.GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
	{
		return false;
	}

	//Ensure the task is valid.
	const CTaskParachute* pTask = static_cast<CTaskParachute *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
	if(!pTask)
	{
		return false;
	}

	return pTask->IsDependentOnParachute();
}

void CTaskParachute::ReleaseStreamingOfParachutePackVariationForPed(CPed& rPed)
{
	//Ensure the parachute pack variation is valid.
	const PedVariation* pVariation = FindParachutePackVariationForPed(rPed);
	if(!pVariation)
	{
		return;
	}

	//Release the preload data.
	rPed.CleanUpPreloadData();
}

void CTaskParachute::RequestStreamingOfParachutePackVariationForPed(CPed& rPed)
{
	//Ensure the parachute pack variation is valid.
	const PedVariation* pVariation = FindParachutePackVariationForPed(rPed);
	if(!pVariation)
	{
		return;
	}

	//Set the preload data.
	rPed.SetPreloadData(pVariation->m_Component, pVariation->m_DrawableId, pVariation->m_TexId);
}

void CTaskParachute::SetParachutePackVariationForPed(CPed& rPed)
{
	//Ensure the parachute pack variation is valid.
	const PedVariation* pVariation = FindParachutePackVariationForPed(rPed);
	if(!pVariation)
	{
		return;
	}

	// Cache off the current variation information for this component
	bool bCache = !IsParachutePackVariationActiveForPed(rPed);
	CPlayerInfo* pPlayerInfo = rPed.GetPlayerInfo();
	if(bCache && pPlayerInfo)
	{
		pPlayerInfo->SetPreviousVariationData(pVariation->m_Component, rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(pVariation->m_Component), 
																	   rPed.GetPedDrawHandler().GetVarData().GetPedCompAltIdx(pVariation->m_Component),
																	   rPed.GetPedDrawHandler().GetVarData().GetPedTexIdx(pVariation->m_Component),
																	   rPed.GetPedDrawHandler().GetVarData().GetPedPaletteIdx(pVariation->m_Component));
	}

	//Set the parachute pack variation.
	rPed.SetVariation(pVariation->m_Component, pVariation->m_DrawableId, pVariation->m_DrawableAltId, pVariation->m_TexId, GetTintIndexForParachutePack(rPed), 0, false);
    rPed.FinalizeHeadBlend();
}

aiTask *CTaskParachute::Copy() const
{
	CTaskParachute *pTask = rage_new CTaskParachute(m_iFlags);
	pTask->SetTarget(m_pTargetEntity, m_vTargetPosition);
	pTask->SetSkydiveTransition(m_nSkydiveTransition);
	return pTask;
}

bool CTaskParachute::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", GetStaticStateName(GetState()), __FUNCTION__);)

	CPhysics::DisableOnlyPushOnLastUpdateForOneFrame();

	//Check if we should process skydiving physics.
	if(ShouldProcessSkydivingPhysics())
	{
		//Process skydiving physics.
		ProcessSkydivingPhysics(fTimeStep);
	}
	//Check if we should process deploying physics.
	else if(ShouldProcessDeployingPhysics())
	{
		//Process deploying physics.
		ProcessDeployingPhysics();
	}
	//Check if we should process parachuting physics.
	else if(ShouldProcessParachutingPhysics())
	{
		//Process parachuting physics.
		ProcessParachutingPhysics();
	}
	
	return true;
}

void CTaskParachute::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	//Make sure that there is a valid camera at all times 
	settings.m_CameraHash = sm_Tunables.m_CameraSettings.m_SkyDivingCamera.GetHash();
	settings.m_MaxOrientationDeltaToInterpolate	= PI;

	switch(GetState())
	{
	case State_Deploying:
	case State_Parachuting:
		if(m_pParachuteObject)
		{
			settings.m_AttachEntity = m_pParachuteObject;

			const bool bUsingCloseupMode = ShouldUseCloseUpCamera();
			if(bUsingCloseupMode)
			{
				settings.m_CameraHash = sm_Tunables.m_CameraSettings.m_ParachuteCloseUpCamera.GetHash();
			}
			else
			{
				settings.m_CameraHash = sm_Tunables.m_CameraSettings.m_ParachuteCamera.GetHash();
			}
		}
		break;

	case State_BlendFromNM:
	case State_Landing:
	case State_CrashLand:
	case State_Quit:
		{
			settings.m_CameraHash = 0;
		}
		break;

	default:
		break;
	}
}

bool CTaskParachute::AddGetUpSets(CNmBlendOutSetList& sets, CPed* pPed)
{
	//Check if we are blending from NM.
	if(GetState() == State_BlendFromNM)
	{
		//Add the skydive getups.
		sets.Add(NMBS_SKYDIVE_GETUPS);

		// When the clone is recovering from a collision it must generate the get up sets itself 
		// (since there is no corresponding TaskGetUp on the owner)
		// by calling AddGetUpSets but we don't want a TaskMoveInAir
		if(pPed && !pPed->IsNetworkClone())
		{
			//Set the move task.
			CTaskMove* pTaskMove = rage_new CTaskMoveInAir();
			sets.SetMoveTask(pTaskMove);
		}

		return true;
	}
	else
	{
		//Call the base class version.
		return CTask::AddGetUpSets(sets, pPed);
	}
}

bool CTaskParachute::CalcDesiredVelocity(const Matrix34& UNUSED_PARAM(mUpdatedPed), float fTimeStep, Vector3& vDesiredVelocity) const
{
	//This section of code was added to allow the ped to maintain their velocity when walking off an edge with a parachute.
	//Previously, the ped would move immediately into animated velocity mode, which meant they would essentially fall in place
	//regardless of their initial velocity when leaving the edge.

	//Check the state.
	bool bUsePedVelocity = false;
	switch(GetState())
	{
		case State_Start:
		case State_WaitingForBaseToStream:
		case State_BlendFromNM:
		case State_WaitingForMoveNetworkToStream:
		case State_TransitionToSkydive:
		case State_WaitingForSkydiveToStream:
		case State_Skydiving:
		{
			//Use the ped velocity.
			bUsePedVelocity = true;
			break;
		}
		case State_Deploying:
		{
			//Ensure the parachute is not out.
			if(IsParachuteOut())
			{
				break;
			}

			//Use the ped velocity.
			bUsePedVelocity = true;
			break;
		}
		default:
		{
			break;
		}
	}

	//Ensure we should use the ped velocity.
	if(!bUsePedVelocity)
	{
		return false;
	}

	//Set the desired velocity.
	vDesiredVelocity = GetPed()->GetVelocity();

	//Check if we are moving up.
	if(vDesiredVelocity.z > 0.0f)
	{
		//Decrease the Z velocity, if it's positive.  We don't need people skydiving up.
		static dev_float s_fZVelocityDecreaseRate = 5.0f;
		vDesiredVelocity.z -= (s_fZVelocityDecreaseRate * fTimeStep);
		vDesiredVelocity.z = Max(vDesiredVelocity.z, 0.0f);
	}

	BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : vDesiredVelocity %f %f %f", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__, vDesiredVelocity.x, vDesiredVelocity.y, vDesiredVelocity.z);)

	return true;
}

void CTaskParachute::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut) const
{
	//Check the state.
	switch(GetState())
	{
		case State_Skydiving:
		case State_Deploying:
		{
			//Set the limits.
			fMinOut = sm_Tunables.m_PedAngleLimitsForSkydiving.m_MinPitch;
			fMaxOut = sm_Tunables.m_PedAngleLimitsForSkydiving.m_MaxPitch;
			break;
		}
		default:
		{
			//Clear the limits.
			fMinOut = 0.0f;
			fMaxOut = 0.0f;
			break;
		}
	}
}

bool CTaskParachute::IsCrashLanding() const
{
	return (GetState() == State_CrashLand);
}

bool CTaskParachute::IsLanding() const
{
	switch(GetState())
	{
		case State_Landing:
		case State_CrashLand:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

void CTaskParachute::ProcessPedImpact(const CEntity* pEntity, Vec3V_In vCollisionNormal)
{
	//Assert that we are attached.
	taskAssert(GetPed()->GetIsAttached());

	//Ensure we should not ignore the other entity.
	if(pEntity && ShouldIgnorePedCollisionWithEntity(*pEntity, vCollisionNormal))
	{
		return;
	}

	//Detach the ped.
	DetachPedFromParachute();
}

bool CTaskParachute::ShouldAbortTaskDueToLowLodLanding(void) const
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);

	// we want low lod peds to just quit out the task when they are about to land on the floor rather than land....
	if(m_bIsUsingLowLod)
	{
		bool waterCrashLand		= (GetStateFromNetwork() == State_CrashLand && m_LandingData.m_nType == LandingData::Water);
		//bool landCrashLand		= (GetStateFromNetwork() == State_CrashLand && m_LandingData.m_CrashData.m_bWithParachute);

		if((GetStateFromNetwork() == State_Landing) || waterCrashLand)
		{
			parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);

			return true;
		}
	}

	return false;
}

bool CTaskParachute::ShouldStickToFloor() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Landing:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

void CTaskParachute::SetTarget(const CPhysical* pEntity, Vec3V_In vPosition)
{
	//Assign the entity.
	m_pTargetEntity = pEntity;
	
	//Assign the position.
	m_vTargetPosition = vPosition;
	
	//Check if the target is valid.
	bool bHasTarget = false;
	if(m_pTargetEntity)
	{
		bHasTarget = true;
	}
	else if(!IsEqualAll(m_vTargetPosition, Vec3V(V_ZERO)))
	{
		bHasTarget = true;
	}
	
	//Change the flag.
	m_iFlags.ChangeFlag(PF_HasTarget, bHasTarget);
}

void CTaskParachute::SetTargetEntity(const CPhysical* pEntity)
{
	SetTarget(pEntity, Vec3V(V_ZERO));
}

void CTaskParachute::SetTargetPosition(Vec3V_In vPosition)
{
	SetTarget(NULL, vPosition);
}

CTask::FSM_Return CTaskParachute::ProcessPreFSM()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);

	//Process the flags.
	ProcessFlags();

	//Process the lod.
	ProcessPreLod();
	
	//Process the streaming.
	ProcessStreaming();
	
	//Process the ped.
	ProcessPed();

	//Process the pad shake.
	ProcessPadShake();

	//Process the parachute.
	if(!ProcessParachute())
	{
		parachuteDebugf1("Frame %d : Ped %p %s : %s : ProcessParachute failed - FSM_Quit", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	
		return FSM_Quit;
	}
	
	// Re-enable the NoCollision on the vehicle the clone jumped out of when suitable...
	ProcessCloneVehicleCollision();

	// Process the clone value smoothing...
	ProcessCloneValueSmoothing();

	//Process the camera.
	ProcessCamera();

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);

	return FSM_Continue;
}

void CTaskParachute::ProcessCloneVehicleCollision(bool const force /* = false */)
{
	CPed* ped = GetPed();

	if(!ped || !ped->IsNetworkClone())
	{
		return;
	}

	CVehicle* myVehicle = ped->GetMyVehicle();

	if(!myVehicle)
	{
		return;
	}

	if(myVehicle->GetNoCollisionEntity() != ped)
	{
		return;	
	}

	// Distance check
	Vector3 pedPos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
	Vector3 vehPos = VEC3V_TO_VECTOR3(ped->GetMyVehicle()->GetTransform().GetPosition());
	if(((pedPos - vehPos).Mag2() > (10.0f * 10.0f)) || (force))
	{
		myVehicle->SetNoCollision(NULL, 0);
		return;
	}

	// state check
	if((GetState() >= State_Deploying) || (force))
	{
		myVehicle->SetNoCollision(NULL, 0);	
	}	
}

void CTaskParachute::ProcessCloneValueSmoothing()
{
	if(!GetPed() || !GetPed()->IsNetworkClone())
	{
		return;
	}

	UpdateDataLerps();
}

void CTaskParachute::ProcessMovement()
{
	CPed *pPed = GetPed();
	
	switch(GetState())
	{
		case State_Skydiving:
		case State_Deploying:
		{
			//Clamp the pitch.
			const float fDesiredPitch = Clamp( pPed->GetDesiredPitch(), -HALF_PI, HALF_PI );
			pPed->SetDesiredPitch(fwAngle::LimitRadianAngleForPitch(fDesiredPitch));
			Assert(pPed->GetDesiredPitch() >= -HALF_PI && pPed->GetDesiredPitch() <= HALF_PI);

			//Calculate the extra changes this frame.
			//Note: These are not multiplied by the time step, because the time step has
			//already been taken into account when the desired values were calculated.
			float fExtraHeadingChangeThisFrame = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
			float fExtraPitchChangeThisFrame = pPed->GetDesiredPitch() - pPed->GetCurrentPitch();
			
			//Set the extra changes this frame.
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fExtraHeadingChangeThisFrame);
			pPed->GetMotionData()->SetExtraPitchChangeThisFrame(fExtraPitchChangeThisFrame);
			break;
		}
		default:
		{
			break;
		}
	}

	switch(GetState())
	{
		case State_Skydiving:
		case State_Deploying:
		case State_Parachuting:
		{
			// Note: we were getting some jittering sometimes when turning while skydiving & parachuting, if we
			// only get one ProcessPhysics() call per frame. Not sure exactly what the source of that
			// was, so at least for now, we request updates for each physics simulation step.
			// Would be good to get to the bottom of it, though.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);
			break;
		}
		default:
		{
			break;
		}
	}
}

void CTaskParachute::PitchCapsuleBound()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// When aborting, we get two calls to CleanUp, one with whatever state we were in before aborting with OnExit and one with state == GetDefaultStateAfterAbort() / State_Quit
	// The clone doesn't get a call to CleanUp() with state == State_Quit as the task is deleted before hand
	// This means we can DoAbort() during skydiving, get a call to CleanUp with state == State_Skydive which will set the pitch to -HALF_PI
	// We won't get the next call to CleanUp with State_Quit so the bound pitch is left set incorrect.
	if(GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		// clean up...
		pPed->SetDesiredBoundPitch(0.0f);
	}
	else
	{
		//Check the state.
		switch(GetState())
		{
			case State_Start:
			case State_WaitingForBaseToStream:
			case State_BlendFromNM:
			case State_WaitingForMoveNetworkToStream:
			case State_TransitionToSkydive:
			case State_Deploying:
			{
				//The ped could be anywhere from vertical to horizontal.

				//Grab the ped's matrix.
				Matrix34 mat;
				pPed->GetBoneMatrix(mat, BONETAG_ROOT);

				//Calculate the difference in the Z axes.
				float fPitch = Abs(mat.c.Angle(ZAXIS));

				//Set the bound pitch.
				pPed->SetDesiredBoundPitch(-fPitch);
				break;
			}
			case State_WaitingForSkydiveToStream:
			case State_Skydiving:
			{
				//The ped should be horizontal at this point.
				pPed->SetDesiredBoundPitch(-HALF_PI);
				break;
			}
			default:
			{
				//Clear the bound pitch.
				pPed->SetDesiredBoundPitch(0.0f);
				break;
			}
		}
	}
}

void CTaskParachute::OnCloneTaskNoLongerRunningOnOwner()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

    SetStateFromNetwork(State_Quit);
}

CTaskInfo* CTaskParachute::CreateQueriableState() const
{
	if(GetState() == State_Parachuting)
	{
		Assert(m_pParachuteObject.Get() != NULL);
	}
	
	//Get the smoke trail data for the clone.
	bool bCanLeaveSmokeTrail = false;
	Color32 smokeTrailColor;
	GetSmokeTrailDataForClone(bCanLeaveSmokeTrail, smokeTrailColor);

	parachuteDebugf3("CTaskParachute::CreateQueriableState m_pParachuteObject[%p] IsParachuteOut[%d]",m_pParachuteObject.Get(),IsParachuteOut());

	CClonedParachuteInfo::InitParams params(	
												GetState(), 
												m_pParachuteObject,
												(u8)m_iFlags,
												m_fCurrentHeading,
												m_fStickX,
												m_fStickY,
												m_fTotalStickInput,
												m_fRoll,
												m_fPitch,
												m_fLeftBrake,
												m_fRightBrake,
												m_bLeaveSmokeTrail,
												bCanLeaveSmokeTrail,
												smokeTrailColor,
												(u8)m_nSkydiveTransition,
												(s8)m_LandingData.m_nType,
												m_LandingData.m_uParachuteLandingFlags.GetAllFlags(),
												m_LandingData.m_CrashData.m_bWithParachute,
												GetPed()->GetVelocity(),
												IsParachuteOut()
											);

	return rage_new CClonedParachuteInfo( params );
}

void CTaskParachute::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	// Need to read the state first as we use it below to decide if we want to read something or not...
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_PARACHUTE );
    CClonedParachuteInfo *parachuteInfo = static_cast<CClonedParachuteInfo*>(pTaskInfo);

	m_vSyncedVelocity							= parachuteInfo->GetPedVelocity();
	m_parachuteObjectId							= parachuteInfo->GetParachuteId();
	m_pParachuteObject							= static_cast<CObject*>(parachuteInfo->GetParachute());
	parachuteDebugf3("CTaskParachute::ReadQueriableState m_parachuteObjectId[%d] m_pParachuteObject[%p]",m_parachuteObjectId,m_pParachuteObject.Get());
	m_iFlags									= parachuteInfo->GetFlags();
	m_fTotalStickInput							= parachuteInfo->GetTotalStickInput();
	m_bLeaveSmokeTrail							= parachuteInfo->GetLeaveSmokeTrail();
	m_nSkydiveTransition						= (SkydiveTransition)parachuteInfo->GetSkydiveTransition();

	m_fPitch									= parachuteInfo->GetPitch();
	m_fRoll										= parachuteInfo->GetRoll();
	
	m_isOwnerParachuteOut						= parachuteInfo->IsParachuteOut();

	// only use network data in these specific instances (as we're interested in what the network is saying)
	// else just use locally generated data.
	if ((m_LandingData.m_nType == LandingData::Invalid) && ((LandingData::Type)parachuteInfo->GetLandingType() != LandingData::Invalid))
	{
		m_LandingData.m_nType = (LandingData::Type)parachuteInfo->GetLandingType();
	}

	if(
		((GetState() <= State_Deploying)	&& (GetStateFromNetwork() == State_CrashLand))		||
		((GetState() == State_Parachuting)	&& (GetStateFromNetwork() == State_Landing))		|| 
		((GetState() == State_Parachuting)	&& (GetStateFromNetwork() == State_CrashLand))		||
		((GetState() == State_Landing)		&& (GetStateFromNetwork() == State_CrashLand))		||
		((GetState() == State_CrashLand)	&& (GetStateFromNetwork() == State_Landing))		
		)
	{
		m_LandingData.m_nType						= (LandingData::Type)parachuteInfo->GetLandingType();
		m_LandingData.m_uParachuteLandingFlags		= parachuteInfo->GetParachuteLandingFlags();
		m_LandingData.m_CrashData.m_bWithParachute	= parachuteInfo->GetCrashLandWithParachute();
	}

	if(!m_dataLerps[LEFT_BRAKE].GetValue())
	{
		m_fLeftBrake = parachuteInfo->GetLeftBrake();
		m_dataLerps[LEFT_BRAKE].SetValue(&m_fLeftBrake);
		m_dataLerps[LEFT_BRAKE].SetNumFrames(10);
	}

	if(!m_dataLerps[RIGHT_BRAKE].GetValue())
	{
		m_fRightBrake = parachuteInfo->GetRightBrake();
		m_dataLerps[RIGHT_BRAKE].SetValue(&m_fRightBrake);
		m_dataLerps[RIGHT_BRAKE].SetNumFrames(10);
	}

	if(!m_dataLerps[CURRENT_X].GetValue())
	{
		m_fStickX = parachuteInfo->GetCurrentX();
		m_dataLerps[CURRENT_X].SetValue(&m_fStickX);
		m_dataLerps[CURRENT_X].SetNumFrames(10);
	}

	if(!m_dataLerps[CURRENT_Y].GetValue())
	{
		m_fStickY = parachuteInfo->GetCurrentY();
		m_dataLerps[CURRENT_Y].SetValue(&m_fStickY);
		m_dataLerps[CURRENT_Y].SetNumFrames(10);
	}

	if(!m_dataLerps[CURRENT_HEADING].GetValue())
	{
		m_fCurrentHeading = parachuteInfo->GetCurrentHeading();
		m_dataLerps[CURRENT_HEADING].SetValue(&m_fCurrentHeading);
		m_dataLerps[CURRENT_HEADING].SetNumFrames(10);
	}

	m_dataLerps[LEFT_BRAKE].SetTarget(parachuteInfo->GetLeftBrake());
	m_dataLerps[RIGHT_BRAKE].SetTarget(parachuteInfo->GetRightBrake());
	m_dataLerps[CURRENT_X].SetTarget(parachuteInfo->GetCurrentX());
	m_dataLerps[CURRENT_Y].SetTarget(parachuteInfo->GetCurrentY());
	m_dataLerps[CURRENT_HEADING].SetTarget(parachuteInfo->GetCurrentHeading());

	bool bCanLeaveSmokeTrail	= parachuteInfo->GetCanLeaveSmokeTrail();
	Color32 smokeTrailColor		= parachuteInfo->GetSmokeTrailColor();
	
	//Set the smoke trail data on the clone.
	SetSmokeTrailDataOnClone(bCanLeaveSmokeTrail, smokeTrailColor);
}

bool CTaskParachute::OverridesNetworkAttachment(CPed* UNUSED_PARAM(pPed))
{
	// if we're deploying we want to locally detach the parachute from the ped and attach the ped to the parachute
	// otherwise we get gaps where the ped isn't attached at all for several frames which causes horrible pops

	// The clone detaches the parachute itself when it lands - we don't want it reattaching.
	// if we are crashing, in land or water, a ragdoll task 
	// will be fired off which will detach the parachute on 
	// the clone itself we don't then want the game trying 
	// to reattach the ped to the parachute until the attachment 
	// node is synced.

	return true;
}

void CTaskParachute::GetSmokeTrailDataForClone(bool& bCanLeaveSmokeTrail, Color32& smokeTrailColor) const
{
	//Initialize the data.
	bCanLeaveSmokeTrail = false;
	smokeTrailColor = Color_white;
	
	//Ensure the entity is valid.
	if(!GetEntity())
	{
		return;
	}
	
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//The smoke trail data should only be read and sent from locals.
	//taskAssertf(!pPed->IsNetworkClone(), "Ped is a network clone.");

	//Ensure the player info is valid.
	const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if(!pPlayerInfo)
	{
		return;
	}

	//Get the data.
	bCanLeaveSmokeTrail = pPlayerInfo->GetCanLeaveParachuteSmokeTrail();
	smokeTrailColor = pPlayerInfo->GetParachuteSmokeTrailColor();
}

void CTaskParachute::SetSmokeTrailDataOnClone(const bool bCanLeaveSmokeTrail, const Color32 smokeTrailColor)
{
	//Ensure the entity is valid.
	if(!GetEntity())
	{
		return;
	}

	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//The smoke trail data should only be received and set on clones.
	taskAssertf(pPed->IsNetworkClone(), "Ped is not a network clone.");

	//Ensure the player info is valid.
	CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if(!pPlayerInfo)
	{
		return;
	}

	//Set the data.
	pPlayerInfo->SetCanLeaveParachuteSmokeTrail(bCanLeaveSmokeTrail);
	pPlayerInfo->SetParachuteSmokeTrailColor(smokeTrailColor);
}

void  CTaskParachute::IncrementStatSmokeTrailDeployed(const u32 color)
{
	if (!m_bIncrementedStatSmokeTrail && NetworkInterface::IsGameInProgress())
	{
		taskAssert(GetPed() && !GetPed()->IsNetworkClone() && GetPed()->IsLocalPlayer());
		taskAssert(m_bLeaveSmokeTrail && CanLeaveSmokeTrail());

		if (color != CRGBA_White().GetColor())
		{
			StatId parachuteDeployed;

			if (color == CRGBA_Black().GetColor())       parachuteDeployed = StatsInterface::GetStatsModelHashId("PARACHUTE_BLACK_DEPLOYED");
			else if (color == CRGBA_Red().GetColor())    parachuteDeployed = StatsInterface::GetStatsModelHashId("PARACHUTE_RED_DEPLOYED");
			else if (color == CRGBA_Orange().GetColor()) parachuteDeployed = StatsInterface::GetStatsModelHashId("PARACHUTE_ORANGE_DEPLOYED");
			else if (color == CRGBA_Yellow().GetColor()) parachuteDeployed = StatsInterface::GetStatsModelHashId("PARACHUTE_YELLOW_DEPLOYED");
			else if (color == CRGBA_Blue().GetColor())   parachuteDeployed = StatsInterface::GetStatsModelHashId("PARACHUTE_BLUE_DEPLOYED");

			if (parachuteDeployed.IsValid())
			{
				StatsInterface::IncrementStat(parachuteDeployed, 1.0f);
			}
		}

		m_bIncrementedStatSmokeTrail = true;
	}
}

CTaskFSMClone*	CTaskParachute::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	CTaskParachute *newTask = rage_new CTaskParachute(m_iFlags);

	return newTask;
}

CTaskFSMClone*	CTaskParachute::CreateTaskForLocalPed(CPed* pPed)
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	return CreateTaskForClonePed(pPed);
}

bool CTaskParachute::IsStateChangeHandledByNetwork(s32 iState, s32 iStateFromNetwork) const
{
	// Should be handled at a higher level and automatically switched to....
	Assert(iStateFromNetwork != State_Quit);

	// the clone ped just follows it's own direction until the parachute has been deployed, unless we've crashed...
	if((iState <= State_Deploying) && (iStateFromNetwork != State_CrashLand))
	{
		return false;
	}

	// if the remote is deploying and the owner is in state parachuting - allow it to change state
	if ((iState == State_Deploying) && (iStateFromNetwork == State_Parachuting))
	{
		return true;
	}

	// always change immediately to crash land...
	if(iStateFromNetwork == State_CrashLand)
	{
		return true;
	}
	
	// check if we're ready to change...parachuting is the final state before the final land / crash states so only one we need to test is in a valid condition to change...
	if(GetState() == State_Parachuting)
	{
		// Don't want to be going backwards - have seen it happen from lagging were clone is parachuting and network goes back to deploy so clone deploys twice.
		if(iStateFromNetwork <= State_Parachuting)
		{
			return false;
		}

		if( ! IsMoveNetworkHelperInTargetState( ) )
		{
			return false;
		}
	}
	else if(GetState() == State_Landing)
	{
		if(iStateFromNetwork <= State_Landing)
		{
			return false;
		}
	}
	else if(GetState() == State_CrashLand)
	{
		if(iStateFromNetwork < State_Landing)
		{
			return false;
		}

		if(iStateFromNetwork == State_Landing)
		{
			return false; //If the authority is in State_Landing but the remote is in State_CrashLand need to return false because the data is in crash land - need to retain state -- related to url:bugstar:1909481
		}
	}

	return true;
}

bool CTaskParachute::IsPedInStateToParachute(const CPed& rPed)
{
	//Ensure parachuting is allowed.
	if(!CGameConfig::Get().AllowParachuting())
	{
		return false;
	}

	//Ensure the ped is not injured.
	if(rPed.IsInjured())
	{
		return false;
	}

	//Ensure the ped has not disabled parachuting.
	if(rPed.GetPedResetFlag(CPED_RESET_FLAG_DisableParachuting))
	{
		return false;
	}

	//Ensure the ped is not in custody.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
	{
		return false;
	}

	//Ensure the ped is not cuffed.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return false;
	}

	//Ensure the ped has a parachute.
	if(!DoesPedHaveParachute(rPed))
	{
		return false;
	}

	//Don't parachute with a jetpack attached to us.
	if(rPed.GetHasJetpackEquipped())
	{
		return false;
	}

	return true;
}

bool CTaskParachute::CanPedParachute(const CPed& rPed, float fAngle)
{
	//Grab the position.
	Vec3V vPosition = rPed.GetTransform().GetPosition();
	
	//Check if the ped is not ragdolling.
	if(!rPed.GetUsingRagdoll())
	{
		//Use the ped's head as the start position, instead of their origin.
		vPosition = VECTOR3_TO_VEC3V(rPed.GetBonePositionCached(BONETAG_HEAD));
	}
	
	return CanPedParachuteFromPosition(rPed, vPosition, false, NULL, fAngle);
}

bool CTaskParachute::CanPedParachuteFromPosition(const CPed& rPed, Vec3V_In vPosition, bool bEnsurePositionCanBeReached, Vec3V_Ptr pOverridePedPosition, float fAngle)
{
	//Ensure the ped state is valid.
	if(!IsPedInStateToParachute(rPed))
	{
		return false;
	}

	//Check if we are in ragdoll mode.
	if(rPed.GetUsingRagdoll())
	{
		//Ensure the ped is falling at a significant rate.
		if(rPed.GetVelocity().z > sm_Tunables.m_Allow.m_MinFallingSpeedInRagdoll)
		{
			return false;
		}

		//Find the NM control task.
		CTask* pTask = rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL);
		if(pTask)
		{
			//Ensure the ped has been ragdolling for a certain amount of time.
			if(pTask->GetTimeRunning() < sm_Tunables.m_Allow.m_MinTimeInRagdoll)
			{
				return false;
			}
		}
	}
	
	//Calculate the start position.
	Vector3 vStart = VEC3V_TO_VECTOR3(vPosition);

	//Calculate the direction.
	Vector3 vDirection(-ZAXIS);
	if(fAngle != 0.0f)
	{
		//Calculate the ped direction.
		Vec3V vPedVelocity = VECTOR3_TO_VEC3V(rPed.GetVelocity());
		Vec3V vPedVelocityXY = vPedVelocity;
		vPedVelocityXY.SetZ(ScalarV(V_ZERO));
		Vec3V vPedForward = rPed.GetTransform().GetForward();
		Vec3V vPedForwardXY = vPedForward;
		vPedForwardXY.SetZ(ScalarV(V_ZERO));
		Vec3V vPedDirection = NormalizeFastSafe(vPedVelocityXY, vPedForwardXY);

		//Calculate the axis of rotation.
		Vec3V vUp(V_UP_AXIS_WZERO);
		Vec3V vAxisOfRotation = Cross(vPedDirection, vUp);

		//Rotate the direction about the axis.
		QuatV qRotation = QuatVFromAxisAngle(vAxisOfRotation, ScalarVFromF32(fAngle * DtoR));
		vDirection = VEC3V_TO_VECTOR3(Transform(qRotation, VECTOR3_TO_VEC3V(vDirection)));
	}
	
	//Calculate the end position.
	Vector3 vEnd = vStart + (vDirection * sm_Tunables.m_Allow.m_MinClearDistanceBelow);
	
	//Check for water beneath the ped.
	float fWaterHeight = 0.0f;
	if(Water::GetWaterLevel(vEnd, &fWaterHeight, true, POOL_DEPTH, 999999.9f, NULL))
	{
		//Check if the water height is above the end point.
		if(fWaterHeight >= vEnd.z)
		{
			return false;
		}
	}

	//Generate the radius.
	static float s_fRadiusForProbes = 0.25f;

#if __DEV
	if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_ValidityProbes)
	{
		CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), s_fRadiusForProbes, Color_blue, 2500, 0, false);	
	}
#endif
	
	//Check for solid objects beneath the ped.
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
	capsuleDesc.SetResultsStructure(&shapeTestResults);
	capsuleDesc.SetCapsule(vStart, vEnd, s_fRadiusForProbes);
	capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
	capsuleDesc.SetExcludeEntity(&rPed);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
	{
#if __DEV
		if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_ValidityProbes)
		{
			CTask::ms_debugDraw.AddLine(shapeTestResults[0].GetPosition(), shapeTestResults[0].GetPosition() + shapeTestResults[0].GetIntersectedPolyNormal() * ScalarVFromF32(0.1f), Color_blue, 2500);	
		}
#endif
		return false;
	}

	//Check if the position must be reachable.
	if(bEnsurePositionCanBeReached)
	{
		//Grab the ped position.
		Vec3V vPedPosition;

		if(pOverridePedPosition) 
		{
			vPedPosition = *pOverridePedPosition;
		}
		else
		{
			vPedPosition = rPed.GetTransform().GetPosition();
		}

#if __DEV
		if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_ValidityProbes)
		{
			CTask::ms_debugDraw.AddCapsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), s_fRadiusForProbes, Color_blue, 2500, 0, false);
		}
#endif

		//Check for solid objects between the ped and the position.
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestFixedResults<1> shapeTestResults;
		capsuleDesc.SetResultsStructure(&shapeTestResults);
		capsuleDesc.SetCapsule(VEC3V_TO_VECTOR3(vPedPosition), vStart, s_fRadiusForProbes);
		capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
		capsuleDesc.SetExcludeEntity(&rPed);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
#if __DEV
			if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_ValidityProbes)
			{
				CTask::ms_debugDraw.AddLine(shapeTestResults[0].GetPosition(), shapeTestResults[0].GetPosition() + shapeTestResults[0].GetIntersectedPolyNormal() * ScalarVFromF32(0.1f), Color_blue, 2500);	
			}
#endif
			return false;
		}
	}
	
	return true;
}

bool CTaskParachute::DoesPedHaveParachute(const CPed& rPed)
{
	//If using reserve parachute, yes we do.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute) && rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_UseReserveParachute))
	{
		return true;
	}

	//Ensure the ped's inventory is valid.
	const CPedInventory* pPedInventory = rPed.GetInventory();
	if(!pPedInventory)
	{
		return false;
	}

	//Ensure the ped has a parachute.
	if(!pPedInventory->GetWeapon(GADGETTYPE_PARACHUTE))
	{
		return false;
	}

	return true;
}

#if DEBUG_DRAW

void CTaskParachute::DebugCloneDataSmoothing()
{
	if(GetEntity())
	{
		if(GetPed())
		{
			static const char* DataLerpNames[] =
			{
				"LEFT_BRAKE",
				"RIGHT_BRAKE",
				"CURRENT_X",
				"CURRENT_Y",
				"CURRENT_HEADING",
				"NUM_LERPERS"
			};

			char buffer[1024] = "\0";
			
			for(int i  = 0; i < NUM_LERPERS; ++i)
			{
				if(m_dataLerps[i].GetValue())
				{
					char temp[32] = "\0";
					sprintf(temp, "%s : target %f : value %f\n", DataLerpNames[i], m_dataLerps[i].GetTarget(), *(m_dataLerps[i].GetValue()));
					strcat(buffer, temp);
				}
			}

			grcDebugDraw::Text(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), Color_white, buffer, true);
		}
	}	
}

void CTaskParachute::DebugCloneTaskInformation()
{
	if(GetEntity())
	{
		if(GetPed())
		{
			bool bCanLeaveSmokeTrail = false;
			Color32 smokeTrailColor = Color_black;
			GetSmokeTrailDataForClone(bCanLeaveSmokeTrail, smokeTrailColor);

			char buffer[1024] = "\0";
			sprintf(buffer, "State %d\nParachuteObject %p\nFlags %x\nCurrent Heading %f\nCurrentX %f\nCurrentY %f\nTotalStickInput %f\nRoll %f\nPitch %f\nLeft Brake %f\nRight Brake %f\nSmoke Trail Color %d\nLanding Type %d\nSkydive Transition %d\nLanding Flags %d\nLeaveSmokeTrail %d\nCanLeaveSmokeTrail %d\n",
			GetState(), 
			m_pParachuteObject.Get(),
			m_iFlags.GetAllFlags(),
			m_fCurrentHeading,
			m_fStickX,
			m_fStickY,
			m_fTotalStickInput,
			m_fRoll,
			m_fPitch,
			m_fLeftBrake,
			m_fRightBrake,
			smokeTrailColor.GetColor(),
			m_LandingData.m_nType,
			m_nSkydiveTransition,
			m_LandingData.m_uParachuteLandingFlags.GetAllFlags(),
			m_bLeaveSmokeTrail,
			bCanLeaveSmokeTrail);

			grcDebugDraw::Text(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), Color_white, buffer);
		}
	}
}

#endif /* DEBUG_DRAW */	

bool CTaskParachute::IsInScope(const CPed* UNUSED_PARAM(pPed))
{
	return true;
}

void CTaskParachute::TaskSetState(const s32 iState)
{
	if(GetState() == State_Skydiving)
	{
		Assert(iState != State_Parachuting);
	}

	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : Setting State From %s To %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", GetStaticStateName(GetState()), GetStaticStateName(iState));)

	aiTask::SetState(iState);
}

void CTaskParachute::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_Parachuting:
			expectedTaskType = CTaskTypes::TASK_VEHICLE_GUN; 
			break;
		default:
			return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		/*CTaskFSMClone::*/CreateSubTaskFromClone(pPed, expectedTaskType);
	}
}

CTaskParachute::FSM_Return CTaskParachute::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
#if !__FINAL && __BANK

	RenderCloneSkydivingVelocityInfo();

#endif /* !__FINAL && __BANK */

	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : iState %s : iStateFromNetwork %s : iEvent %d", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__, GetStaticStateName(iState), GetStateFromNetwork() != -1 ? GetStaticStateName(GetStateFromNetwork()) : "None", iEvent);)

#if !__FINAL
//	PrintAllDataValues();
#endif /* _FINAL */

	if(iEvent == OnUpdate)
	{
		if(GetState() != State_Quit)
		{
			if(ShouldAbortTaskDueToLowLodLanding())
			{
				parachuteDebugf1("Frame %d : Ped %p %s : %s : ABort due to low lod landing! State_Quit", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	
				TaskSetState(State_Quit);
				return FSM_Continue;
			}

			int iStateFromNetwork = GetStateFromNetwork();
			if(iStateFromNetwork != -1)
			{
				if(iStateFromNetwork != iState)
				{
					if((iStateFromNetwork == State_Quit) || (IsStateChangeHandledByNetwork(iState, iStateFromNetwork)))
					{
						NOTFINAL_ONLY(parachuteDebugf3("CTaskParachute::UpdateClonedFSM--setting state from network call TaskSetState(iStateFromNetwork[%s])",GetStaticStateName(iStateFromNetwork));)
						TaskSetState(iStateFromNetwork);
						return FSM_Continue;
					}
				}
			}

			if(UpdateClonePedRagdollCorrection(GetPed()))
			{
				return FSM_Continue;
			}
		}

		UpdateClonedSubTasks(GetPed(), GetState());
	}

	//Get the actual parachute entity from the sent across network id - might have not resolved to actual entity on previous readqueriablestate.
	//This will ensure we get the actual entity/object for the parachute properly assigned as soon as it becomes relevant on the remote.
	if (!m_pParachuteObject.Get() && (m_parachuteObjectId != NETWORK_INVALID_OBJECT_ID))
	{
		netObject *networkObject = NetworkInterface::GetNetworkObject(m_parachuteObjectId);
		if (networkObject && networkObject->GetEntity()) //ensure we have a GetEntity before we reset m_parachuteObjectId to INVALID
		{
			m_pParachuteObject = static_cast<CObject*>(networkObject->GetEntity());
			parachuteDebugf3("CTaskParachute::UpdateClonedFSM--set m_parachuteObjectId[%d] m_pParachuteObject[%p]",m_parachuteObjectId,m_pParachuteObject.Get());
			m_parachuteObjectId = NETWORK_INVALID_OBJECT_ID; //reset
		}
	}

	if(iEvent == OnUpdate)
	{
		//Depend on the parachute.
		bool bDependOnParachute = (m_pParachuteObject.Get() != NULL) && !m_pParachuteObject->m_nObjectFlags.bFadeOut &&
			!m_pParachuteObject->m_nObjectFlags.bRemoveFromWorldWhenNotVisible;
		DependOnParachute(bDependOnParachute);
	}

	CPed* pPed = GetPed();

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_WaitingForBaseToStream)
			FSM_OnUpdate
				return WaitingForBaseToStream_OnUpdate();

		FSM_State(State_BlendFromNM)
			FSM_OnEnter
				BlendFromNM_OnEnter();
			FSM_OnUpdate
				return BlendFromNM_OnUpdate();

		FSM_State(State_WaitingForMoveNetworkToStream)
			FSM_OnUpdate
				return WaitingForMoveNetworkToStream_OnUpdate();
				
		FSM_State(State_TransitionToSkydive)
			FSM_OnEnter
				TransitionToSkydive_OnEnter();
			FSM_OnUpdate
				return TransitionToSkydive_OnUpdate();

		FSM_State(State_WaitingForSkydiveToStream)
			FSM_OnEnter
				WaitingForSkydiveToStream_OnEnter();
			FSM_OnUpdate
				return WaitingForSkydiveToStream_OnUpdate();

		FSM_State(State_Skydiving)
			FSM_OnEnter
				SkyDiving_OnEnter(pPed);
			FSM_OnUpdate
				return SkyDiving_OnUpdate(pPed);		
		
		FSM_State(State_Deploying)						
			FSM_OnEnter
				Deploying_OnEnter(pPed);
			FSM_OnUpdate
				Deploying_OnUpdate(pPed);

		FSM_State(State_Parachuting)
			FSM_OnEnter
				Parachuting_OnEnter();
			FSM_OnUpdate
				return Parachuting_OnUpdate(pPed);

		FSM_State(State_Landing)
			FSM_OnEnter
				Landing_OnEnter(pPed);		
			FSM_OnUpdate
				Landing_OnUpdateClone(pPed);
			FSM_OnExit
				Landing_OnExit();

		FSM_State(State_CrashLand)
			FSM_OnEnter
				CrashLanding_OnEnter(pPed);
			FSM_OnUpdate
				return CrashLanding_OnUpdate(pPed);	
		
		FSM_State(State_Quit)// needed for OnCloneNoLongerRunningOnOwner blah
			FSM_OnUpdate
				return FSM_Quit;
	
	FSM_End
}

CTask::FSM_Return CTaskParachute::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : iState %s : iEvent %d", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__, GetStaticStateName(iState), iEvent);)

#if !__FINAL
//	PrintAllDataValues();
#endif /* _FINAL */

	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	FSM_State(State_Start)
		FSM_OnEnter
			Start_OnEnter();
		FSM_OnUpdate
			return Start_OnUpdate();

	FSM_State(State_WaitingForBaseToStream)
		FSM_OnUpdate
			return WaitingForBaseToStream_OnUpdate();

	FSM_State(State_BlendFromNM)
		FSM_OnEnter
			BlendFromNM_OnEnter();
		FSM_OnUpdate
			return BlendFromNM_OnUpdate();

	FSM_State(State_WaitingForMoveNetworkToStream)
		FSM_OnUpdate
			return WaitingForMoveNetworkToStream_OnUpdate();
			
	FSM_State(State_TransitionToSkydive)
		FSM_OnEnter
			TransitionToSkydive_OnEnter();
		FSM_OnUpdate
			return TransitionToSkydive_OnUpdate();

	FSM_State(State_WaitingForSkydiveToStream)
		FSM_OnEnter
			WaitingForSkydiveToStream_OnEnter();
		FSM_OnUpdate
			return WaitingForSkydiveToStream_OnUpdate();

	FSM_State(State_Skydiving)
		FSM_OnEnter
			SkyDiving_OnEnter(pPed);
		FSM_OnUpdate
			return SkyDiving_OnUpdate(pPed);

	FSM_State(State_Deploying)
		FSM_OnEnter
			Deploying_OnEnter(pPed);
		FSM_OnUpdate
			return Deploying_OnUpdate(pPed);

	FSM_State(State_Parachuting)
		FSM_OnEnter
			Parachuting_OnEnter();
		FSM_OnUpdate
			return Parachuting_OnUpdate(pPed);
		FSM_OnExit
			Parachuting_OnExit();

	FSM_State(State_Landing)
		FSM_OnEnter
			Landing_OnEnter(pPed);
		FSM_OnUpdate
			return Landing_OnUpdate(pPed);
		FSM_OnExit
			Landing_OnExit();

	FSM_State(State_CrashLand)
		FSM_OnEnter
			CrashLanding_OnEnter(pPed);
		FSM_OnUpdate
			return CrashLanding_OnUpdate(pPed);

	FSM_State(State_Quit)
		FSM_OnUpdate
			return FSM_Quit;

FSM_End
}

CTask::FSM_Return CTaskParachute::ProcessPostFSM()
{
	//Process the lod.
	ProcessPostLod();

	ProcessFirstPersonParams();

	//Process the move parameters.
	ProcessMoveParameters();
	
	return FSM_Continue;
}

bool CTaskParachute::ProcessPostPreRender()
{
	//Process the parachute bones.
	ProcessParachuteBones();

	return CTaskFSMClone::ProcessPostPreRender();
}

// move state
const fwMvStateId CTaskParachute::ms_TransitionToSkydiveState("TransitionToSkydive",0x43C2F1DD);
const fwMvStateId CTaskParachute::ms_WaitingForSkydiveToStreamState("WaitingForSkydiveToStream",0xC0A00C20);
const fwMvStateId CTaskParachute::ms_SkydivingState("Skydiving",0x9681630B);
const fwMvStateId CTaskParachute::ms_DeployParachuteState("DeployParachute",0x11EC33F5);
const fwMvStateId CTaskParachute::ms_ParachutingState("Parachuting",0x2FEB27EB);
const fwMvStateId CTaskParachute::ms_LandingState("Landing",0xFA9B6C5);

//move control params
const fwMvFloatId CTaskParachute::ms_xDirection("xDirection",0x7E40964);
const fwMvFloatId CTaskParachute::ms_yDirection("yDirection",0x93BC2FB3);
const fwMvFloatId CTaskParachute::ms_Direction("Direction",0x34DF9828);
const fwMvFloatId CTaskParachute::ms_NormalDirection("NormalDirection",0x5C890B08);
const fwMvFloatId CTaskParachute::ms_Phase("Phase",0xA27F482B);
const fwMvFloatId CTaskParachute::ms_LeftBrake("LeftBrake",0xEB9C71D);
const fwMvFloatId CTaskParachute::ms_RightBrake("RightBrake",0xC502D63E);
const fwMvFloatId CTaskParachute::ms_CombinedBrake("CombinedBrake",0x8570c4dc);
const fwMvFloatId CTaskParachute::ms_BlendMotionWithBrake("BlendMotionWithBrake",0x97CE4CB9);
const fwMvFloatId CTaskParachute::ms_ParachutePitch("ParachutePitch",0xF91694D5);
const fwMvFloatId CTaskParachute::ms_ParachuteRoll("ParachuteRoll",0xBBBADB27);
const fwMvFloatId CTaskParachute::ms_FirstPersonCameraHeading("FirstPersonCameraHeading",0x28de494e);
const fwMvFloatId CTaskParachute::ms_FirstPersonCameraPitch("FirstPersonCameraPitch",0x9fbe1732);

//move flags 
const fwMvFlagId CTaskParachute::ms_TransitionToSkydiveFromFallGlide("TransitionToSkydiveFromFallGlide",0x9203C10F);
const fwMvFlagId CTaskParachute::ms_TransitionToSkydiveFromJumpInAirL("TransitionToSkydiveFromJumpInAirL",0xCD759AF9);
const fwMvFlagId CTaskParachute::ms_TransitionToSkydiveFromJumpInAirR("TransitionToSkydiveFromJumpInAirR",0x7D1DFA27);
const fwMvFlagId CTaskParachute::ms_LandSlow("LandSlow",0x17E0C0F);
const fwMvFlagId CTaskParachute::ms_LandRegular("LandRegular",0x18F1025A);
const fwMvFlagId CTaskParachute::ms_LandFast("LandFast",0xAD880926);
const fwMvFlagId CTaskParachute::ms_UseLowLod("UseLowLod",0x61CE4620);
const fwMvFlagId CTaskParachute::ms_FirstPersonMode("FirstPersonMode",0x8BB6FFFA);

//move requests
const fwMvRequestId CTaskParachute::ms_TransitionToSkydive("TransitionToSkydive",0x43C2F1DD); 
const fwMvRequestId CTaskParachute::ms_WaitForSkydiveToStream("WaitForSkydiveToStream",0x320EF6F1); 
const fwMvRequestId CTaskParachute::ms_Skydive("Skydive",0x3DB23531); 
const fwMvRequestId CTaskParachute::ms_DeployParachute("DeployParachute",0x11EC33F5); 
const fwMvRequestId CTaskParachute::ms_Land("Land",0xE355D038); 
const fwMvRequestId CTaskParachute::ms_Parachuting("ParachuteIdle",0x90A5E43A); 
const fwMvRequestId CTaskParachute::ms_ClipSetChanged("ClipSetChanged",0x43086e23); 

//move events
const fwMvBooleanId CTaskParachute::ms_TransitionToSkydiveEnded("TransitionToSkydiveEnded",0x4881C1B4);
const fwMvBooleanId CTaskParachute::ms_OpenChuteDone("OpenChuteDone",0xACBCADBE); 
const fwMvBooleanId CTaskParachute::ms_LandingComplete("LandDone",0xF1C7B5D5);
const fwMvBooleanId CTaskParachute::ms_TransitionToSkydiveOnEnter("TransitionToSkydive_OnEnter",0xA38763AC);
const fwMvBooleanId CTaskParachute::ms_WaitingForSkydiveToStreamOnEnter("WaitingForSkydiveToStream_OnEnter",0x2051F498);
const fwMvBooleanId CTaskParachute::ms_SkyDiveOnEnter("SkyDive_OnEnter",0x6D2876D2);
const fwMvBooleanId CTaskParachute::ms_DeployParachuteOnEnter("DeployParachute_OnEnter",0xBC8DB101);
const fwMvBooleanId CTaskParachute::ms_ParachutingOnEnter("Parachuting_OnEnter",0x8F192FD9);
const fwMvBooleanId CTaskParachute::ms_LandingOnEnter("Landing_OnEnter",0x512F3FB8);
const fwMvBooleanId CTaskParachute::ms_CanEarlyOutForMovement("CAN_EARLY_OUT_FOR_MOVEMENT",0x7E1C8464);

void CTaskParachute::Start_OnEnter()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Grab the ped.
	CPed* pPed = GetPed();

	//Try clearing any scripted exit vehicle task when running this as a temporary event response
	//to prevent the exit task from resuming and forcing a motion state change on clean up which would create a tpose B*1637805
	CTask* pTempTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
	if (pTempTask && pTempTask->FindSubTaskOfType(CTaskTypes::TASK_PARACHUTE))
	{
		CTask* pPrimaryTask = pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
		if (pPrimaryTask && pPrimaryTask->FindSubTaskOfType(CTaskTypes::TASK_EXIT_VEHICLE))
		{
			pPed->GetPedIntelligence()->ClearPrimaryTaskAtPriority(PED_TASK_PRIORITY_PRIMARY);
		}
	}
	
	//Note that the ped is not standing.
	pPed->SetIsStanding(false);
	
	//Note that the ped is in the air.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, true);
	
	//Ensure gravity is turned on for the ped.
	pPed->SetUseExtractedZ(false);
	
	//Destroy the weapon in the ped's hand.
	weaponAssert(pPed->GetWeaponManager());
	pPed->GetWeaponManager()->DestroyEquippedWeaponObject();

	//Check if the parachute pack variation is not active.
	if(!IsParachutePackVariationActive() && !pPed->IsNetworkClone())
	{
		//Set the parachute pack variation.
		SetParachutePackVariation(); 
	}

	// Set the ped's velocity to be the same as the owner to stop them drifting out...
	if(pPed->IsNetworkClone())
	{
		pPed->SetVelocity(m_vSyncedVelocity);
	}

	//Initialize the first person camera state.
	m_bIsUsingFirstPersonCameraThisFrame = IsUsingFirstPersonCamera();


	if(m_pedChangingOwnership)
	{
#if __ASSERT
		SetCanChangeState(true);
#endif
		TaskSetState(m_cachedParachuteState);
#if __ASSERT
		SetCanChangeState(false);
#endif

		m_parachuteObjectId = m_cachedParachuteObjectId;

		netObject *networkObject = NetworkInterface::GetNetworkObject(m_parachuteObjectId);
		if (networkObject)
		{
			m_pParachuteObject = static_cast<CObject*>(networkObject->GetEntity());
			SetParachuteTintIndex();
		}		
		
		// Attach a new move network since we will be skipping that step. 
		AttachMoveNetwork();

		//Set the clip set for the state.
		SetClipSetForState();

		//Set the move network state.
		SetMoveNetworkState(ms_Parachuting, ms_ParachutingOnEnter, ms_ParachutingState, true);

		m_pedChangingOwnership = false;
	}
	else
	{
		//! Kill move player when in parachute.
		KillMovePlayer();
	}
}

CTask::FSM_Return CTaskParachute::Start_OnUpdate()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if !__FINAL
	ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif /* !__FINAL */

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
		return FSM_Continue;
	}

	// make sure the clone ped is out of his vehicle before starting
	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone() && pPed->GetIsInVehicle())
	{
		CNetObjPed* pNetObj = static_cast<CNetObjPed*>(pPed->GetNetworkObject());

		if (pNetObj->GetPedsTargetVehicle() == NULL)
		{
			pNetObj->SetClonePedOutOfVehicle(true, 0);
		}

		return FSM_Continue;
	}

	//Wait for the base clip set to stream in.
	TaskSetState(State_WaitingForBaseToStream);

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::WaitingForBaseToStream_OnUpdate()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if !__FINAL
	ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif /* !__FINAL */

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
		return FSM_Continue;
	}

	bool bLoaded = m_ClipSetRequestHelperForBase.IsLoaded();

#if FPS_MODE_SUPPORTED
	//! Local player must have 1st person anims as well.
	if(bLoaded && GetPed()->IsLocalPlayer() && m_ClipSetRequestHelperForFirstPersonBase.GetClipSetId() != CLIP_SET_ID_INVALID) 
	{
		bLoaded = m_ClipSetRequestHelperForFirstPersonBase.IsLoaded();
	}
#endif

	//Check if the base clip set is loaded.
	if(bLoaded)
	{
		//Check if the ped is using a ragdoll.
		if(GetPed()->GetUsingRagdoll())
		{
			//Blend from NM.
			TaskSetState(State_BlendFromNM);
		}
		else
		{
			//Wait for the move network to stream in.
			TaskSetState(State_WaitingForMoveNetworkToStream);
		}
	}

	return FSM_Continue;
}

void CTaskParachute::BlendFromNM_OnEnter()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Create the task.
	CTask* pTask = rage_new CTaskBlendFromNM();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskParachute::BlendFromNM_OnUpdate()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if !__FINAL
	ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif /* !__FINAL */

	if(!GetPed()->IsNetworkClone())
	{
		//Start first-person camera shaking.
		StartFirstPersonCameraShakeForSkydiving();
	}

	//Check if the get-up task is valid.
	const CTaskGetUp* pTask = static_cast<CTaskGetUp *>(FindSubTaskOfType(CTaskTypes::TASK_GET_UP));
	if(pTask)
	{
		//Check if the phase has exceeded the minimum.
		static dev_float s_fMinPhase = 0.3f;
		if(pTask->GetPhase() >= s_fMinPhase)
		{
			//Check if the parachute should be deployed.
			if(CheckForDeploy())
			{
				//Force the ped to open the parachute, when possible.
				m_bForcePedToOpenChute = true;

				//Wait for the move network to stream in.
				TaskSetState(State_WaitingForMoveNetworkToStream);
				return FSM_Continue;
			}
		}
	}

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
		return FSM_Continue;
	}

	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(m_bCloneRagdollTaskRelaunch && m_uCloneRagdollTaskRelaunchState == State_WaitingForMoveNetworkToStream)
		{
			BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : Clear m_bCloneRagdollTaskRelaunch", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);)
			//B*1891327 If this blend state was set up in UpdateClonePedRagdollCorrection while in State_WaitingForMoveNetworkToStream state dont set it again here, clear flag and wait frame
			m_uCloneRagdollTaskRelaunchState = -1;
			m_bCloneRagdollTaskRelaunch = false;
			return FSM_Continue;
		}

		//Wait for the move network to stream in.
		TaskSetState(State_WaitingForMoveNetworkToStream);
		return FSM_Continue;
	}

	//Check if this is the local player.
	if(GetPed()->IsLocalPlayer())
	{
		//Sync the ped's heading to the camera direction.
		GetPed()->SetDesiredHeading(camInterface::GetGameplayDirector().GetFrame().ComputeHeading());
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::WaitingForMoveNetworkToStream_OnUpdate()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if !__FINAL
	ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif /* !__FINAL */

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
		return FSM_Continue;
	}

	//Check if the move network has streamed in.
	if(m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskParachute))
	{
		//Attach the move network.
		AttachMoveNetworkIfNecessary();

		bool bProceed = true;
		if (NetworkInterface::IsGameInProgress())
		{
			bProceed = m_networkHelper.IsNetworkActive();
		}

		if (bProceed)
		{
			// if the clone is not trying to repair the task after being in a ragdoll inducing collision
			if(!m_bCloneRagdollTaskRelaunch)
			{
				//Check if we should transition to skydive.
				if(m_nSkydiveTransition != ST_None)
				{
					//Move to the transition to skydive state.
					TaskSetState(State_TransitionToSkydive);
				}
				else
				{
					//Set the state for skydiving.
					SetStateForSkydiving();
				}
			}
			else
			{
				// whatever the network is now in, just go there immediately...
				parachuteDebugf1("m_bCloneRagdollTaskRelaunch --> TaskSetState m_uCloneRagdollTaskRelaunchState[%d]",m_uCloneRagdollTaskRelaunchState);
				TaskSetState(m_uCloneRagdollTaskRelaunchState);

				// clear the flag...
				m_uCloneRagdollTaskRelaunchState = -1;
				m_bCloneRagdollTaskRelaunch = false;
			}
		}
	}

	//Update the ped trail.
	UpdateVfxPedTrails();

	return FSM_Continue;
}

void CTaskParachute::TransitionToSkydive_OnEnter()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Set the clip set for the state.
	SetClipSetForState();

	//Set the move network state.
	SetMoveNetworkState(ms_TransitionToSkydive, ms_TransitionToSkydiveOnEnter, ms_TransitionToSkydiveState);
	
	//Set the correct transition flag.
	switch(m_nSkydiveTransition)
	{
		case ST_FallGlide:
		{
			m_networkHelper.SetFlag(true, ms_TransitionToSkydiveFromFallGlide);
			break;
		}
		case ST_JumpInAirL:
		{
			m_networkHelper.SetFlag(true, ms_TransitionToSkydiveFromJumpInAirL);
			break;
		}
		case ST_JumpInAirR:
		{
			m_networkHelper.SetFlag(true, ms_TransitionToSkydiveFromJumpInAirR);
			break;
		}
		case ST_None:
		default:
		{
			taskAssertf(false, "Bad skydive transition: %d.", m_nSkydiveTransition);
			break;
		}
	}
}

CTask::FSM_Return CTaskParachute::TransitionToSkydive_OnUpdate()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if !__FINAL
	ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif /* !__FINAL */

	//Ensure the state has been entered.
	if( ! IsMoveNetworkHelperInTargetState( ) )
	{
		return FSM_Continue;
	}

	//Check if the parachute should be deployed.
	if(CheckForDeploy())
	{
		//Move to the deploying state.
		TaskSetState(State_Deploying);
		return FSM_Continue;
	}

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
		return FSM_Continue;
	}

	//Wait for the animation to end.
	if(m_networkHelper.GetBoolean(ms_TransitionToSkydiveEnded))
	{
		//Set the state for skydiving.
		SetStateForSkydiving();
	}

	//Update the ped trail.
	UpdateVfxPedTrails();
	
	return FSM_Continue;
}

void CTaskParachute::WaitingForSkydiveToStream_OnEnter()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Set the clip set for the state.
	SetClipSetForState();

	//Set the move network state.
	SetMoveNetworkState(ms_WaitForSkydiveToStream, ms_WaitingForSkydiveToStreamOnEnter, ms_WaitingForSkydiveToStreamState);
}

CTask::FSM_Return CTaskParachute::WaitingForSkydiveToStream_OnUpdate()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if !__FINAL
	ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif /* !__FINAL */

	//Ensure the state has been entered.
	if( ! IsMoveNetworkHelperInTargetState( ) )
	{
		return FSM_Continue;
	}

	//Check if the parachute should be deployed.
	if(CheckForDeploy())
	{
		//Move to the deploying state.
		TaskSetState(State_Deploying);
		return FSM_Continue;
	}

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
		return FSM_Continue;
	}

	//Check if the clip set for skydiving is streamed in.
	if(fwClipSetManager::IsStreamedIn_DEPRECATED(GetClipSetForSkydiving()))
	{
		//Move to the skydiving state.
		TaskSetState(State_Skydiving);
	}

	//Update the ped trail.
	UpdateVfxPedTrails();

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::SkyDiving_OnEnter( CPed* pPed )
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Set the clip set for the state.
	SetClipSetForState();

	//Set the move network state.
	SetMoveNetworkState(ms_Skydive, ms_SkyDiveOnEnter, ms_SkydivingState);

	StatsInterface::RegisterStartSkydiving(pPed);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::SkyDiving_OnUpdate( CPed* pPed )
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if !__FINAL
	ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif /* !__FINAL */

	//Process the physics.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);
	
	//Ensure the state has been entered.
	if( ! IsMoveNetworkHelperInTargetState( ) )
	{
		return FSM_Continue;
	}

	if(!GetPed()->IsNetworkClone())
	{
		//Start first-person camera shaking.
		StartFirstPersonCameraShakeForSkydiving();
	}
	
	if(CheckForDeploy())
	{
		parachuteDebugf3("CTaskParachute::SkyDiving_OnUpdate--CheckForDeploy--set State_Deploying"); 
		TaskSetState(State_Deploying);
		return FSM_Continue;
	}
	
	//Process the skydiving controls.
	ProcessSkydivingControls();

#if __BANK
	if (ms_bVisualiseInfo)
	{
		char szText[128];
		formatf(szText,"Pitch : %f",pPed->GetCurrentPitch());
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),Color_green,0,0,szText);
	}
#endif

	if(pPed->IsAPlayerPed())
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		if(pControl)
		{
			Vector2 vecStick;
			vecStick.x = pControl->GetParachuteTurnLeftRight().GetNorm();
			vecStick.y = pControl->GetParachutePitchUpDown().GetNorm();
			CFlightModelHelper::MakeCircularInputSquare(vecStick);

			//The signals below allow the idle and movement clips to all be in the same move state.	

			Vector2 vConverted = vecStick; 

			//convert the out put to be in the same range as the move inputs 0 - 1
			vConverted.x = (vecStick.x + 1.0f) / 2.0f; 
			vConverted.y = (vecStick.y + 1.0f) / 2.0f; 

			CTaskMotionBase::InterpValue(m_fStickX, vConverted.x, ms_interpRateHeadingSkydive, false);

			CTaskMotionBase::InterpValue(m_fStickY, vConverted.y, ms_interpRateHeadingSkydive, false);

			//work out the ratio of inputs and blend on that
			float combinedxy = abs(vecStick.x) + abs(vecStick.y); 
			
			//total input from the stick clamped
			float totalinput = Clamp( combinedxy, 0.0f, 1.0f);  

			//use the total input to work out if we need to blend in the idle clip
			CTaskMotionBase::InterpValue(m_fTotalStickInput, totalinput, ms_interpRateHeadingSkydive, false);

			//Blend in the x or y clips (forward and backward). This value is blended against the total input and the idle clip.
			float xyInputRatio = 0.0f;

			if(combinedxy > 0.0f)
			{
				xyInputRatio = abs(vecStick.y) / combinedxy; //ratio of stick input on the y versus total input
			}
	
			CTaskMotionBase::InterpValue(m_fCurrentHeading, xyInputRatio, ms_interpRateHeadingSkydive, false);
		}
	}
	else
	{
		//Set the stick input parameters.
		//This is for the AI, so this may just be temporary as we may want them to move around during skydiving at some point.
		m_fStickX = 0.0f;
		m_fStickY = 0.0f;
		m_fTotalStickInput = 0.0f;
		m_fCurrentHeading = 0.0f;
				
		//Set the current heading based on the roll/pitch combination.
		Vector2 vHeading(m_fRoll, m_fPitch);
		if(vHeading.NormalizeSafe())
		{
			m_fCurrentHeading = CTaskMotionBasicLocomotion::ConvertRadianToSignal(fwAngle::LimitRadianAngleSafe(atan2(vHeading.y, vHeading.x) - HALF_PI));
		}
	}

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
		return FSM_Continue;
	}

	//Update the ped trail.
	UpdateVfxPedTrails();

	//Allow a lod change this frame. - TEST - block lod changing as it may causes positional syncing during deployment issues in MP.
	if(!NetworkInterface::IsGameInProgress())
	{
		AllowLodChangeThisFrame();
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::Deploying_OnEnter(CPed* pPed)
{
	if(pPed && pPed->IsNetworkClone())
	{
		Vector3 networkPos  = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
		Vector3 pedPos		= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		m_finalDistSkydivingToDeploy = (networkPos - pedPos).Mag();
	}

	BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);)	

	if(pPed && pPed->IsNetworkClone())
	{
#if !__FINAL
		ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif

		//If it is a network clone and deploying state is entered - disable remote collisions so that they cannot crash land or land independently of the local authority. (lavalley)
#if !__FINAL
		if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			parachuteDebugf3("CTaskParachute::Deploying_OnEnter pPed[%p][%s][%s] IsNetworkClone --> SetInactiveCollisions(true)",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
		}
#endif
		SetInactiveCollisions(true);
	}
	else if (pPed)
	{
		StatsInterface::RegisterPrepareParachuteDeploy(pPed);
	}

	//Set the clip set for the state.
	SetClipSetForState();

	//Set the move network state.
	SetMoveNetworkState(ms_DeployParachute, ms_DeployParachuteOnEnter, ms_DeployParachuteState);
	
	//Deploy the parachute.
	DeployParachute();
	
	//Remove the parachute from the inventory. Now that it has been deployed, it can't be re-used.
	RemoveParachuteFromInventory();

	//Trigger deployment vfx.
	TriggerVfxDeploy();

	return FSM_Continue; 
}

CTask::FSM_Return CTaskParachute::Deploying_OnUpdate(CPed* pPed)
{
#if __BANK

	// debug catch code - if we're deploying and we're drifting from the owner a large distance, freeze....
	if(pPed->IsNetworkClone())
	{
		Vector3 pedPos			= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 networkPos		= NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
		
		float dist = (pedPos - networkPos).Mag();

		TUNE_GROUP_BOOL(TASK_PARACHUTE, FreezeWhenDriftingWhenDeploying, false)
		if(FreezeWhenDriftingWhenDeploying)
		{
			if(dist > (m_finalDistSkydivingToDeploy + 3.0f))
			{
				NetworkDebug::LookAtAndPause(pPed->GetNetworkObject()->GetObjectID());
			}
		}
	}

#endif /* __BANK */

	BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);)

	//Process the physics.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);
	
	//Ensure the state has been entered.
	if( ! IsMoveNetworkHelperInTargetState( ) )
	{
		return FSM_Continue;
	}
	
	//Set the ped's desired pitch.
	float fPitch = pPed->GetCurrentPitch();
	float fMaxPitchChange = -fPitch;
	float fMaxPitchChangeThisFrame = 2.0f * GetTimeStep();
	fMaxPitchChange = Clamp(fMaxPitchChange, -fMaxPitchChangeThisFrame, fMaxPitchChangeThisFrame);
	pPed->SetDesiredPitch(fPitch + fMaxPitchChange);
	
	//Check if the parachute is out.
	if(IsParachuteOut())
	{
		BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : %s : Parachute Out", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", GetStaticStateName(GetState()), __FUNCTION__);)

		//Add physics to the parachute.
		AddPhysicsToParachute();

		//Check if we can check for a pad shake.
		if(m_bCanCheckForPadShakeOnDeploy)
		{
			//Clear the flag.
			m_bCanCheckForPadShakeOnDeploy = false;

			//Check if we are the local player.
			if(GetPed()->IsLocalPlayer())
			{
				//Apply the pad shake.
				CControlMgr::StartPlayerPadShakeByIntensity(sm_Tunables.m_PadShake.m_Deploy.m_Duration, sm_Tunables.m_PadShake.m_Deploy.m_Intensity);

				//Stop first-person camera shaking.
				StopFirstPersonCameraShaking();

				//Start first-person camera shaking.
				StartFirstPersonCameraShakeForDeploy();
			}
		}

		//Give the parachute control over the ped.
		GiveParachuteControlOverPed();

		if(!pPed->IsNetworkClone())
		{
			//Process the parachuting controls.
			ProcessParachutingControls();
		}

		StatsInterface::RegisterParachuteDeployed(pPed);
	}
	
	//Wait for the animation to finish.
	if((!m_bIsUsingLowLod && m_networkHelper.GetBoolean(ms_OpenChuteDone)) ||
		(m_bIsUsingLowLod && IsParachuteOut()))
	{
		BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : Deploying Complete : Switching To Parachute", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);)

		// ped is attached via network syncing code - just continue in this state until we're attached...
		if(pPed->IsNetworkClone())
		{
			if(!pPed->GetIsAttached())
			{
				return FSM_Continue;
			}
		}

		//The parachute should have been given control over the ped by now.
		taskAssertf(m_bParachuteHasControlOverPed, "Parachute does not have control over ped.");
		
		//Move to the parachuting state.
		TaskSetState(State_Parachuting);
	}

	//Scan for a crash landing.
	if(ScanForCrashLanding())
	{
		//Move to the crash land state.
		TaskSetState(State_CrashLand);
	}

	return FSM_Continue;
}

void CTaskParachute::Parachuting_OnEnter()
{
	CPed* pPed = GetPed();

	if(pPed && pPed->IsNetworkClone())
	{
		Vector3 networkPos  = NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
		Vector3 pedPos		= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		m_finalDistDeployingToParachuting = (networkPos - pedPos).Mag();
	}

	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), pPed, BANK_ONLY(pPed ? pPed->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Set the clip set for the state.
	SetClipSetForState();

	//Set the move network state.
	SetMoveNetworkState(ms_Parachuting, ms_ParachutingOnEnter, ms_ParachutingState);

	// Tasks / equipped weapon synced by netobj code
	if(!pPed->IsNetworkClone())
	{
		//Check if we are a player.
		if(pPed->IsPlayer())
		{
			//Create the task for ambient clips.
			CTask* pTask = CreateTaskForAmbientClips();

			//Start the task.
			SetNewTask(pTask);
		}

		//Check if we can aim.
		if(CanAim())
		{
			//Backup the equipped weapon.
			pPed->GetWeaponManager()->BackupEquippedWeapon();

			//Equip the best weapon.
			pPed->GetWeaponManager()->EquipBestWeapon();
		}
	}
	else
	{
		//If it is a network clone and parachute state is entered - disable remote collisions so that they cannot crash land or land independently of the local authority. (lavalley)
#if !__FINAL
		if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			parachuteDebugf3("CTaskParachute::Parachuting_OnEnter pPed[%p][%s][%s] IsNetworkClone --> SetInactiveCollisions(true)",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
		}
#endif
		SetInactiveCollisions(true);
	}
}

CTask::FSM_Return CTaskParachute::Parachuting_OnUpdate( CPed* pPed )
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

#if FPS_MODE_SUPPORTED
	//! Try creating phone in 1st person only - seems weird that we can't pull it out.
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !IsAiming())
	{
		CTaskMobilePhone::PhoneMode phoneMode = CPhoneMgr::CamGetState() ? CTaskMobilePhone::Mode_ToCamera : CTaskMobilePhone::Mode_ToText; 
		if (CTaskMobilePhone::CanUseMobilePhoneTask(*pPed, phoneMode ) )
		{
			if ( CTaskPlayerOnFoot::CheckForUseMobilePhone(*pPed) )
			{
				CTaskMobilePhone::CreateOrResumeMobilePhoneTask(*GetPed());
			}
		}
	}
	else
#endif
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableCellphoneAnimations, true);
	}

	if(!pPed->IsNetworkClone())
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	}
	else
	{
#if !__FINAL
		ValidateNetworkBlender(CTaskParachute::OverridesNetworkBlender(GetPed()));
#endif
	}
	
	//Ensure the state has been entered.
	if( ! IsMoveNetworkHelperInTargetState( ) )
	{
		return FSM_Continue;
	}
	
	//Add event
	CEventShockingParachuterOverhead ev(*pPed);
	CShockingEventsManager::Add(ev);

	//Ensure the ped is not a clone.
	if(!pPed->IsNetworkClone())
	{
		//Process the parachuting controls.
		ProcessParachutingControls();

		//Update the MoVE parameters.
		UpdateMoVE(m_vRawStick);

		//Start first-person camera shaking (note that this will only activate once the deploy shake has finished).
		StartFirstPersonCameraShakeForParachuting();
	}

	//Only do this for the local ped - the remote will get information through the network state sync - otherwise can end up in a completely different state
	//where the remote is crashed and the local authority isn't (lavalley).
	if (!NetworkInterface::IsGameInProgress() || !pPed->IsNetworkClone())
	{
		//Scan for a landing.
		if(ScanForLanding())
		{
			//Check if we are not using low lod.
			if(!m_bIsUsingLowLod)
			{
				//Move to the landing state.
				TaskSetState(State_Landing);
				return FSM_Continue;
			}
			else
			{
				//Finish the task.
				TaskSetState(State_Quit);
				return FSM_Continue;
			}
		}
		//Scan for a crash landing.
		else if(ScanForCrashLanding())
		{
			//Move to the crash land state.
			TaskSetState(State_CrashLand);
			return FSM_Continue;
		}
	}
	
	//Update the smoke trail.
	UpdateVfxSmokeTrail();

	//Update the canopy trail.
	UpdateVfxCanopyTrails();

	//Allow a lod change this frame.
	AllowLodChangeThisFrame();

	// Blow up and kill any parachuting peds that enter an air defence zone.
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere) && !m_bAirDefenceExplosionTriggered)
	{
		u8 uIntersectingZoneIdx = 0;
		Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		if (CAirDefenceManager::IsPositionInDefenceZone(vPedPosition, uIntersectingZoneIdx))
		{
			CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone(uIntersectingZoneIdx);
			if (pDefZone && pDefZone->IsEntityTargetable((CEntity*)pPed) && pDefZone->ShouldFireWeapon())
			{
				// Only trigger explosion on local machine (explosion is synced).
				if (!pPed->IsNetworkClone())
				{
					m_bAirDefenceExplosionTriggered = true;
					m_uTimeAirDefenceExplosionTriggered = fwTimer::GetTimeInMilliseconds();
				}

				CAirDefenceManager::FireWeaponAtPosition(uIntersectingZoneIdx, vPedPosition, m_vAirDefenceFiredFromPosition);
			}
		}
	}
	else if (m_bAirDefenceExplosionTriggered && (fwTimer::GetTimeInMilliseconds() - m_uTimeAirDefenceExplosionTriggered) > CAirDefenceManager::GetExplosionDetonationTime())
	{
		m_bAirDefenceExplosionTriggered = false;
		m_uTimeAirDefenceExplosionTriggered = 0;

		Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		CExplosionManager::CExplosionArgs explosionArgs(EXP_TAG_AIR_DEFENCE, VEC3V_TO_VECTOR3(vPedPosition));
		explosionArgs.m_bInAir = true;
		Vector3 vDirection = VEC3V_TO_VECTOR3(Normalize(vPedPosition - m_vAirDefenceFiredFromPosition));
		explosionArgs.m_vDirection = vDirection;
		explosionArgs.m_weaponHash = ATSTRINGHASH("WEAPON_AIR_DEFENCE_GUN", 0x2c082d7d); 
		explosionArgs.m_pEntIgnoreDamage = (CEntity*)pPed;
		CExplosionManager::AddExplosion(explosionArgs, NULL, true);

		CEventDamage tempDamageEvent(NULL, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_EXPLOSION);
		CPedDamageCalculator damageCalculator(NULL, 9999.9f, WEAPONTYPE_EXPLOSION, 0, false);
		damageCalculator.ApplyDamageAndComputeResponse(pPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);

		TaskSetState(State_Quit);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskParachute::Parachuting_OnExit()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	// Clone weapon synced over network.
	if(!GetPed() || GetPed()->IsNetworkClone())
	{
		return;
	}

	//Check if we can aim.
	if(CanAim())
	{
		//Restore the backup weapon.
		GetPed()->GetWeaponManager()->RestoreBackupWeapon();

		//Destroy the equipped weapon object.
		GetPed()->GetWeaponManager()->DestroyEquippedWeaponObject();
	}
}

CTask::FSM_Return CTaskParachute::Landing_OnEnter(CPed* pPed)
{
	// The network has converted us back from crash landing to landing so we keep in positional sync better.
	if(pPed->GetUsingRagdoll())
	{
		Assert(GetPreviousState() == State_CrashLand);
		Assert(pPed->IsNetworkClone());

		pPed->SwitchToAnimated();

		// re-insert the parachute move network...
		m_networkHelper.ReleaseNetworkPlayer();
		AttachMoveNetworkIfNecessary();

		float	networkHeading	= NetworkInterface::GetLastHeadingReceivedOverNetwork(pPed);	
		pPed->SetHeading(networkHeading);
	}

	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	if (pPed && pPed->IsNetworkClone())
	{
#if !__FINAL
		if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			parachuteDebugf3("CTaskParachute::Landing_OnEnter IsNetworkClone pPed[%p][%s][%s] --> SetInactiveCollisions(false)",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
		}
#endif
		SetInactiveCollisions(false);
	}

	//Set the clip set for the state.
	SetClipSetForState();
	
	//Set the move network state.
	SetMoveNetworkState(ms_Land, ms_LandingOnEnter, ms_LandingState);

	//Stop first-person camera shaking.
	StopFirstPersonCameraShaking();

	//Start first-person camera shaking.
	StartFirstPersonCameraShakeForLanding();

	//Check the landing type.
	switch(m_LandingData.m_nType)
	{
		case LandingData::Slow:
		{
			m_networkHelper.SetFlag(true, ms_LandSlow);
			break;
		}
		case LandingData::Fast:
		{
			m_networkHelper.SetFlag(true, ms_LandFast);
			break;
		}
		case LandingData::Regular:
		default:
		{
			m_networkHelper.SetFlag(true, ms_LandRegular);
			break;
		}
	}

	//Assert that the landing was clean.
	taskAssertf(m_LandingData.m_nType == LandingData::Slow ||
		m_LandingData.m_nType == LandingData::Regular ||
		m_LandingData.m_nType == LandingData::Fast,
		"Landing type is invalid: %d.", m_LandingData.m_nType);

	//We are no longer in the air.
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir, false);

	//Detach the ped and parachute.
	DetachPedAndParachute();

	//Handle the parachute landing.
	HandleParachuteLanding(m_LandingData.m_uParachuteLandingFlags);

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::Landing_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Ensure the state has been entered.
	if( ! IsMoveNetworkHelperInTargetState( ) )
	{
		return FSM_Continue;
	}

	//Use leg ik allow tags.
	GetPed()->GetIkManager().SetFlag(PEDIK_LEGS_USE_ANIM_ALLOW_TAGS);
	
	//Check if we have received the event for early out movement.
	if(m_networkHelper.GetBoolean(ms_CanEarlyOutForMovement))
	{
		//Note that we can early out for movement.
		m_bCanEarlyOutForMovement = true;
	}
	//Check if we should early out for movement.
	else if(ShouldEarlyOutForMovement())
	{
		//Check if the move network is active.
		if(m_networkHelper.IsNetworkActive())
		{
			//Grab the ped.
			CPed* pPed = GetPed();
			
			//Force the motion state.
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Walk);
			
			//Clear the move network.
			float fBlendDuration = sm_Tunables.m_Landing.m_BlendDurationForEarlyOut;
			pPed->GetMovePed().ClearTaskNetwork(m_networkHelper, fBlendDuration, CMovePed::Task_TagSyncTransition);
		}
		
		//Finish the task.
		parachuteDebugf1("Frame %d : Ped %p %s : %s : Should Early Out For movement, State_Quit!", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	
		TaskSetState(State_Quit);
		return FSM_Continue;
	}
	//Wait for the animation to finish.
	else if(m_networkHelper.GetBoolean(ms_LandingComplete))
	{
		//Force the motion state.
		GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle, true);

		//Finish the task.
		parachuteDebugf1("Frame %d : Ped %p %s : %s : Landing Complete, State_Quit!", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	
		TaskSetState(State_Quit);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::Landing_OnUpdateClone(CPed* pPed)
{
	if(!pPed || !pPed->IsNetworkClone())
	{
		Assert(false);
		return FSM_Continue;
	}

	Vector3 pedPos			= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 networkPos		= NetworkInterface::GetLastPosReceivedOverNetwork(pPed);

	//float	pedHeading		= pPed->GetTransform().GetHeading();
	//float	networkHeading	= NetworkInterface::GetLastHeadingReceivedOverNetwork(pPed);
	Vector3 pedToOwner		= pedPos - networkPos;

	// if the clone is too far away from the owner, just bail and accept the pop.
	static const float MAX_POSITION_SYNC_UPDATE_DIST = 10.0f;
	if(pedToOwner.Mag() > MAX_POSITION_SYNC_UPDATE_DIST)
	{
		return FSM_Continue;
	}

	// Get the duration...
	fwMvClipId clipId(CLIP_ID_INVALID);
	if(m_LandingData.m_nType == LandingData::Slow)
	{
		clipId.SetFromString("land_bend_knees");
	}
	else if(m_LandingData.m_nType == LandingData::Regular)
	{
		clipId.SetFromString("land_steps");
	}
	else if(m_LandingData.m_nType == LandingData::Fast)
	{
		clipId.SetFromString("land_roll");
	}
	else 
	{
		Assert(false);
		return FSM_Continue;
	}

	if(CLIP_ID_INVALID != clipId)
	{
		fwClipSet const* clipSet		= fwClipSetManager::GetClipSet(CLIP_SET_SKYDIVE_PARACHUTE);
		if(clipSet)
		{
			crClip const* clip			= clipSet->GetClip(clipId);	

			if(clip)
			{
				float duration			= clip->GetDuration();

				// take off the very end of the anim as it's standing idle....
				duration *= 0.9f;

				// can't change parachute mxtf so close to submission so just use clamped time in state.
				float phase				= Clamp(GetTimeInState() / duration, 0.0f, 1.0f);

				int remainingMilliseconds	= Max((s32)(((1.0f - phase) * duration) * 1000.f), 1);
				u32 timeStep				= fwTimer::GetTimeStepInMilliseconds();
				float lerp					= Clamp(((float)timeStep / remainingMilliseconds), 0.0f, 1.0f);
				Vector3 finalPos			= Lerp(lerp, pedPos, networkPos);

				pPed->SetPosition(finalPos, true, true, false);

				// make sure they are facing the same way....
				//pPed->SetHeading(Lerp(lerp, pedHeading, networkHeading));
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::Landing_OnExit()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	
	//@@: location CTASKPARACHUTE_LANDING_ONEXIT
	//Note that the landing was proper.
	m_bLandedProperly = true;
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskParachute::CrashLanding_OnEnter(CPed* pPed)
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	if (pPed && pPed->IsNetworkClone())
	{
#if !__FINAL
		if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			parachuteDebugf3("CTaskParachute::CrashLanding_OnEnter IsNetworkClone pPed[%p][%s][%s] --> SetInactiveCollisions(false)",pPed,pPed->GetModelName(),pPed->GetNetworkObject()->GetLogName());
		}
#endif
		SetInactiveCollisions(false);
	}

	//Detach the ped and parachute.
	//Need to handle this prior to creating the task below as peds aren't allowed
	//to ragdoll while attached
	DetachPedAndParachute();

	//Assert that the landing was a crash.
	taskAssertf(m_LandingData.m_nType == LandingData::Crash ||
	m_LandingData.m_nType == LandingData::Water,
	"Landing type is invalid: %d.", m_LandingData.m_nType);

	//Check if the ped is not a network clone.
	if(!pPed->IsNetworkClone())
	{
		//Check if we landed in water.
		if(m_LandingData.m_nType == LandingData::Water)
		{
			//Clear the parachute pack variation.
			ClearParachutePackVariation();
		}

		//Apply damage for the crash landing.
		ApplyDamageForCrashLanding();
	}

	//Create the task.
	CTask* pTask = CreateTaskForCrashLanding();

	//Start the task.
	SetNewTask(pTask);

	//Handle the parachute landing.
	HandleParachuteLanding(m_LandingData.m_uParachuteLandingFlags);

	//Stop first-person camera shaking.
	StopFirstPersonCameraShaking();
	
	return FSM_Continue; 
}

CTask::FSM_Return CTaskParachute::CrashLanding_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	// Subtask could be an NM task or an animated fall...
	if (m_Flags.IsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL)
	{
		// Let parent deal with it
		parachuteDebugf1("Frame %d : Ped %p %s : %s : TaskNMControl SubTask complete, FSM_Quit", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	
		return FSM_Quit;
	}

	return FSM_Continue;
}

/*static*/void CTaskParachute::InitialisePedInAir(CPed *pPed)
{
	CTaskComplexControlMovement * pCtrlTask;
	pCtrlTask = rage_new CTaskComplexControlMovement( rage_new CTaskMoveInAir(), rage_new CTaskFall(FF_ForceSkyDive), CTaskComplexControlMovement::TerminateOnSubtask );

	int iEventPriority = E_PRIORITY_IN_AIR + 1;
	CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pCtrlTask,false,iEventPriority);
	pPed->GetPedIntelligence()->AddEvent(event);
}

void CTaskParachute::CreateParachuteInInventory()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	CTaskParachute::GivePedParachute(pPed);
}

void CTaskParachute::RemoveParachuteFromInventory()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	//Clones will just be told to re-enter parachute via normal syncing.
	if(!pPed->IsNetworkClone())
	{
		//! If we have a reserve chute, flag us to use reserve chute so that we use it on the next go.
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseReserveParachute))
		{	
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseReserveParachute, true);
		}
		else
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute, false);
		}
	}

	//Ensure the inventory is valid.
	CPedInventory* pPedInventory = pPed->GetInventory();
	if(!pPedInventory)
	{
		return;
	}

	//Remove the parachute from the inventory.
	pPedInventory->RemoveWeapon(GADGETTYPE_PARACHUTE);
}

void CTaskParachute::DestroyParachute()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}

	//Fade out the object.
	m_pParachuteObject->m_nObjectFlags.bFadeOut = true;

	//Disable collision.  Having collision enabled here has caused issues
	//with peds landing in MP games -- they end up intersecting with it on
	//detach, and get shot to the side.  I have seen this happen in SP as well.
	m_pParachuteObject->DisableCollision(NULL, true);

	//Do not depend on the parachute.
	DependOnParachute(false);

	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is not a clone.
	if(pPed->IsNetworkClone())
	{
		return;
	}
	
	//Remove the object from the world when it is not visible.
	m_pParachuteObject->m_nObjectFlags.bRemoveFromWorldWhenNotVisible = true;

	StatsInterface::RegisterDestroyParachute(pPed);
}

void CTaskParachute::CreateParachute()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	// should have been filtered by CanCreateParachute( )
	if(!GetPed() || GetPed()->IsNetworkClone())
	{
		return;
	}

	//Ensure the model id is valid.
	fwModelId iModelId((strLocalIndex(m_iParachuteModelIndex)));
	if(!taskVerifyf(iModelId.IsValid(), "Invalid parachute model"))
	{
		return;
	}
	
	//Create the input.
	CObjectPopulation::CreateObjectInput input(iModelId, ENTITY_OWNEDBY_GAME, true);
	parachuteDebugf3("CTaskParachute::CreateParachute input.m_bForceClone = true");
	input.m_bForceClone = true;

	//Create the parachute object.
	taskAssert(!m_pParachuteObject);
	m_pParachuteObject = CObjectPopulation::CreateObject(input);
	if(!taskVerifyf(m_pParachuteObject, "Failed to create parachute"))
	{
		return;
	}
	parachuteDebugf3("CTaskParachute::CreateParachute m_ParachuteObject[%p]",m_pParachuteObject.Get());
	m_pParachuteObject->SetIsParachute(true);

	REPLAY_ONLY(CReplayMgr::RecordObject(m_pParachuteObject));

	//Make the parachute invisible.
	CTaskParachuteObject::MakeVisible(*m_pParachuteObject, false);
	
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Add the parachute to the world.
	CGameWorld::Add(m_pParachuteObject, pPed->GetInteriorLocation(), false);

	// Immediately disable collision ( can't specify physics to be off anymore in CreateObject( ) ) 
	m_pParachuteObject->DisableCollision(NULL, true);

	//Set the parachute tint index.
	SetParachuteTintIndex();
	
	//Check if the parachute is being networked.
	netObject* pNetObject = m_pParachuteObject->GetNetworkObject();
	if(pNetObject)
	{
		parachuteDebugf3("CTaskParachute::CreateParachute pNetObject SetGlobalFlag GLOBALFLAG_CLONEALWAYS GLOBALFLAG_PERSISTENTOWNER LOCALFLAG_NOREASSIGN");

		// Set network object flags so stop the object being culled out (distance culling Clone::IsInScope) and migrating...
		pNetObject->SetGlobalFlag(netObject::GLOBALFLAG_CLONEALWAYS, true);

		if (pPed->IsPlayer())
		{
			pNetObject->SetGlobalFlag(netObject::GLOBALFLAG_PERSISTENTOWNER, true);
			// Set the NOREASSIGN flag to destroy the parachute if the player that created it leaves....
			pNetObject->SetLocalFlag(netObject::LOCALFLAG_NOREASSIGN, true);
		}	
	
		// Tell the parachute network object to use the custom parachute blender - which stops it popping
		// if it goes out of sync and relies on the parachutes keeping themselves in check (unless it gets
		// _really_ far out of sync, then a pop back will take place
		SetParachuteNetworkObjectBlender();
	}
	else if (NetworkInterface::IsGameInProgress())
	{
		parachuteWarningf("CTaskParachute::CreateParachute - chute created but network object not created for it - this will cause problems");
	}
	
	//Depend on the parachute.
	DependOnParachute(true);
}

void CTaskParachute::ConfigureCloneParachute()
{
#if !__FINAL
	if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{
		const CPed* ped = GetPed();
		const char* gamertag = ped && ped->GetNetworkObject() && ped->GetNetworkObject()->GetPlayerOwner() ? ped->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "";
		parachuteDebugf3("CTaskParachute::ConfigureCloneParachute ped[%p][%s] isplayer[%d] gamertag[%s]",ped,ped ? ped->GetModelName() : "",ped ? ped->IsPlayer() : 0,gamertag);
	}
#endif

	if(!CanConfigureCloneParachute())
	{
		parachuteDebugf3("CTaskParachute::ConfigureCloneParachute--!CanConfigureCloneParachute()-->return");
		return;
	}

	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Owner dives out of helicopter successfully - goes from TaskExitVehicle->TaskFall->TaskParachute
	//Clone dives out of helicopter but gets in a collision - goes from TaskExitVehicle->TaskNM
	//Owner deploys parachute
	//Clone completes taskNM, TaskParachute is applied to the clone
	//Clone TaskParachute gets parachute object which has ped attached to it as there has been no TaskParachuteObject to stop it.
	//Which would fire an assert off in AttachParachuteToPed so we need to detach everything here...
	DetachPedAndParachute(false);
	
	//Attach the parachute to the ped.
	AttachParachuteToPed();		

	// Set the tint index...
	SetParachuteTintIndex();

	// Flag the actual object too...
	m_pParachuteObject->SetIsParachute(true);

	// Tell the parachute network object to use the custom parachute blender - which stops it popping
	// if it goes out of sync and relies on the parachutes keeping themselves in check (unless it gets
	// _really_ far out of sync, then a pop back will take place
	SetParachuteNetworkObjectBlender();

	// and we're done, so don't do it again...
	m_bCloneParachuteConfigured = true;

	parachuteDebugf3("CTaskParachute::ConfigureCloneParachute--complete--set m_bCloneParachuteConfigured = true");
}

void CTaskParachute::SetParachuteNetworkObjectBlender(void)
{
	Assert(m_pParachuteObject && (m_pParachuteObject->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) || m_pParachuteObject->GetNetworkObject()));

    if(m_pParachuteObject && m_pParachuteObject->GetNetworkObject())
    {
	    static_cast<CNetObjObject*>(m_pParachuteObject->GetNetworkObject())->SetUseParachuteBlending(true);
    }
}

void CTaskParachute::SetParachuteAndPedLodDistance(void)
{
	//Use the max lod distance.
	m_iPedLodDistanceCache = GetPed()->GetLodDistance();
	u32 uLodDistance = Max(m_iPedLodDistanceCache, PARACHUTE_AND_PED_LOD_DISTANCE);

	//Set the lod distance for the ped.
	GetPed()->SetLodDistance(uLodDistance);

	//Set the lod distance for the parachute.
	taskAssert(m_pParachuteObject);
	m_pParachuteObject->SetLodDistance(uLodDistance);
}

void CTaskParachute::ClearParachuteAndPedLodDistance(void)
{
	//Check if the lod distance has been cached.
	if(m_iPedLodDistanceCache != 0)
	{
		//Set the lod distance.
		GetPed()->SetLodDistance(m_iPedLodDistanceCache);

		//Clear the cache.
		m_iPedLodDistanceCache = 0;
	}
}

void CTaskParachute::CalculateAttachmentOffset(CObject* pObject, Vector3& vAttachmentOut)
{
	Assert(pObject);
	Vector3 vBoundBoxMinPara = pObject->GetBoundingBoxMin();
	Vector3 vBoundBoxMaxPara = pObject->GetBoundingBoxMax();	

	// Figure out attach offset based on desired constraint lengths
	float fWidth = vBoundBoxMaxPara.x - vBoundBoxMinPara.x;
	fWidth /= 2.0f;
	vAttachmentOut = VEC3_ZERO;
	vAttachmentOut.y = ms_fAttachDepth;
	vAttachmentOut.z = -rage::Sqrtf(ms_fAttachLength*ms_fAttachLength- fWidth*fWidth);
}

void CTaskParachute::AttachPedToParachute()
{
	//Ensure the parachute is valid.
	if(!taskVerifyf(m_pParachuteObject, "Parachute is invalid."))
	{
		return;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the ped is not attached.
	if(pPed->GetIsAttached())
	{
		pPed->DetachFromParent(0);
	}
	
	//Calculate the attach offset.
	Vector3 vAttachOffset = VEC3_ZERO;
	CalculateAttachmentOffset(m_pParachuteObject, vAttachOffset);
	
	//Attach the ped to the parachute.
	pPed->AttachToPhysicalBasic(m_pParachuteObject, -1, ATTACH_STATE_PED|ATTACH_FLAG_COL_ON|ATTACH_FLAG_AUTODETACH_ON_DEATH, &vAttachOffset, NULL);

	//Enable inactive collisions.
#if !__FINAL
	if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{
		parachuteDebugf3("CTaskParachute::AttachPedToParachute pPed[%p][%s][%s] --> SetInactiveCollisions(true)",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
	}
#endif
	SetInactiveCollisions(true);
}

void CTaskParachute::AttachParachuteToPed()
{
	//Ensure the parachute is valid.
	if(!taskVerifyf(m_pParachuteObject, "Parachute is invalid."))
	{
		parachuteDebugf3("CTaskParachute::AttachParachuteToPed -- m_pParachuteObject is invalid --> return");
		return;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the parachute is not attached.
	if(m_pParachuteObject->GetIsAttached())
	{
		m_pParachuteObject->DetachFromParent(0);
	}
	
	//Calculate the attach offset.
	Vector3 vAttachOffset = VEC3_ZERO;
	CalculateAttachmentOffset(m_pParachuteObject, vAttachOffset);
	
	//Negate the attach offset, because it is for the ped->parachute, and we want parachute->ped.
	vAttachOffset.Negate();
	
	//Attach the parachute to the ped.
	parachuteDebugf3("CTaskParachute::AttachParachuteToPed m_pParachuteObject[%p] pPed[%p] --> invoke AttachToPhysicalBasic",m_pParachuteObject.Get(),pPed);
	m_pParachuteObject->AttachToPhysicalBasic(pPed, -1, ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP/*|ATTACH_FLAG_COL_ON*/, &vAttachOffset, NULL);
}

void CTaskParachute::DetachParachuteFromPed()
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}
	
	//Ensure the parachute is attached.
	if(!m_pParachuteObject->GetIsAttached())
	{
		return;
	}

	//Detach the parachute from the ped.
	static_cast<phArchetypePhys*>(m_pParachuteObject->GetCurrentPhysicsInst()->GetArchetype())->SetMaxSpeed(300.0f);	
	parachuteDebugf3("CTaskParachute::DetachParachuteFromPed m_pParachuteObject[%p] pPed[%p] --> invoke DetachFromParent",m_pParachuteObject.Get(),GetPed());
	m_pParachuteObject->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);
}

void CTaskParachute::DetachPedFromParachute()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Ensure the ped is attached.
	if(!pPed->GetIsAttached())
	{
		return;
	}
	
	//Detach the ped from the parachute.
	pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);

	if(pPed->IsNetworkClone())
	{
		pPed->SetDesiredVelocity(pPed->GetVelocity());
	}

	//Disable inactive collisions.
#if !__FINAL
	if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{
		parachuteDebugf3("CTaskParachute::DetachPedFromParachute pPed[%p][%s][%s] --> SetInactiveCollisions(false)",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
	}
#endif
	SetInactiveCollisions(false);
}

void CTaskParachute::DetachPedAndParachute(bool alertAudio)
{
	//Detach the ped from the parachute.
	DetachPedFromParachute();

	//Detach the parachute from the ped.
	DetachParachuteFromPed();

	if(alertAudio)
	{
		CPed* pPed = GetPed();
		if(pPed && pPed->GetPedAudioEntity())
		{
			pPed->GetPedAudioEntity()->SetPedHasReleaseParachute();
		}
	}
	
}

void CTaskParachute::GiveParachuteControlOverPed()
{
	BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", GetStaticStateName(GetState()), __FUNCTION__);)

	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}
	
	//Ensure the parachute does not have control over the ped.
	if(m_bParachuteHasControlOverPed)
	{
		return;
	}
	
	//Note that the parachute has control over the ped.
	m_bParachuteHasControlOverPed = true;

	//Detach the parachute from the ped.
	DetachParachuteFromPed();

	//Attach the parachute to the ped.
	AttachPedToParachute();

	//Give the parachute a consistent velocity, to ensure a smooth transition.
	Vec3V vVelocity(V_ZERO);
	vVelocity = AddScaled(vVelocity, m_pParachuteObject->GetTransform().GetForward(), ScalarVFromF32(sm_Tunables.m_ParachutePhysics.m_ParachuteInitialVelocityY));
	vVelocity = AddScaled(vVelocity, m_pParachuteObject->GetTransform().GetUp(), ScalarVFromF32(sm_Tunables.m_ParachutePhysics.m_ParachuteInitialVelocityZ));
	m_pParachuteObject->SetVelocity(VEC3V_TO_VECTOR3(vVelocity));
	
	//Probe for parachute intersections.
	ProbeForParachuteIntersections();
}

void CTaskParachute::CrumpleParachute()
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not a network clone.
	if(pPed->IsNetworkClone())
	{
		return;
	}
	
	//Ensure the parachute object task is valid.
	CTaskParachuteObject* pTask = GetParachuteObjectTask();
	if(!taskVerifyf(pTask, "Parachute task is invalid."))
	{
		return;
	}

	//Crumple the parachute.
	pTask->Crumple();
}

void CTaskParachute::DeployParachute()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Ensure the parachute object task is valid.
	CTaskParachuteObject* pTask = GetParachuteObjectTask();
	if(!taskVerifyf(pTask, "Parachute task is invalid."))
	{
		return;
	}
		
	//Deploy the parachute.
	pTask->Deploy();
}

CTaskParachuteObject* CTaskParachute::GetParachuteObjectTask() const
{
	//Ensure the parachute is valid.
	CObject* pParachute = m_pParachuteObject;
	if(!pParachute)
	{
		return NULL;
	}

	//Ensure the task is valid.
	CTask* pTask = pParachute->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY);
	if(!pTask)
	{
		return NULL;
	}

	//Ensure the task is the correct type.
	if(pTask->GetTaskType() != CTaskTypes::TASK_PARACHUTE_OBJECT)
	{
		return NULL;
	}
	
	return static_cast<CTaskParachuteObject *>(pTask);
}

void CTaskParachute::AddPhysicsToParachute()
{
	//Ensure the parachute is valid.
	if(!taskVerifyf(m_pParachuteObject, "Parachute is invalid."))
	{
		return;
	}
	
	// Turn on the physics...
	m_pParachuteObject->EnableCollision();
	
	//Set the mass.
	m_pParachuteObject->SetMass(sm_Tunables.m_ParachuteMass);

	//Prevent the camera shapetests from detecting this object, as we don't want the camera to pop if it passes near to the camera
	//or collision root position.
	phInst* pParachutePhysicsInst = m_pParachuteObject->GetCurrentPhysicsInst();
	if(pParachutePhysicsInst)
	{
		phArchetype* pParachuteArchetype = pParachutePhysicsInst->GetArchetype();
		if(pParachuteArchetype)
		{
			//NOTE: This include flag change will apply to all new instances that use this archetype until it is streamed out.
			//We can live with this for camera purposes, but it's worth noting in case it causes a problem.
			pParachuteArchetype->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);

			//Remove vehicle BVH collision as well - super slow.
			pParachuteArchetype->RemoveIncludeFlags(ArchetypeFlags::GTA_VEHICLE_BVH_TYPE);
		}

		if(pParachutePhysicsInst->IsInLevel())
		{
			CPhysics::GetLevel()->SetInstanceIncludeFlags(pParachutePhysicsInst->GetLevelIndex(), pParachuteArchetype->GetIncludeFlags());
		}
	}
	
	//Ensure the physics archetype is valid.
	phArchetypeDamp* pPhysArch = m_pParachuteObject->GetPhysArch();
	if(!taskVerifyf(pPhysArch, "Parachute physics archetype is invalid."))
	{
		return;
	}
	
	//Remove damping.
	for(int iDampingType = phArchetypeDamp::LINEAR_C; iDampingType < phArchetypeDamp::ANGULAR_V2 +1; iDampingType++)
	{
		pPhysArch->ActivateDamping(iDampingType, VEC3_ZERO);
	}
	
	//Ensure the bound is valid.
	phBound* pBound = pPhysArch->GetBound();
	if(!taskVerifyf(pBound, "Parachute bound is invalid."))
	{
		return;
	}
	
	//Remove the CG offset.
	pBound->SetCGOffset(Vec3V(V_ZERO));
}

void CTaskParachute::ApplyDamageForCrashLanding()
{
	//Keep track of the damage.
	float fDamage = 0.0f;
	bool bInstantKill = false;

	//Check the landing type.
	if(m_LandingData.m_nType == LandingData::Water)
	{
		//Check if we are moving fast enough.
		static dev_float s_fMinSpeed = 15.0f;
		if(Abs(m_vAverageVelocity.GetZf()) > s_fMinSpeed)
		{
			//Calculate the damage.
			static dev_float s_fDamage = 66.0f;
			fDamage = s_fDamage;

			//Force an instant kill if we aren't holding back.
			static dev_float s_fMaxPitchToConsiderHoldingBack = -0.65f;
			bool bIsHoldingBack = (m_fPitch <= s_fMaxPitchToConsiderHoldingBack);
			bInstantKill = !bIsHoldingBack;
			if(!bInstantKill)
			{
				//Force an instant kill if we are landing in shallow water.
				bInstantKill = !CTaskMotionSwimming::CanDiveFromPosition(*GetPed(), GetPed()->GetTransform().GetPosition(), false);
			}
		}
	}
	else if(m_LandingData.m_nType == LandingData::Crash)
	{
		//For now, MP only.
		if(NetworkInterface::IsGameInProgress())
		{
			//Check if there was a ped collision.
			if(m_LandingData.m_CrashData.m_bPedCollision)
			{
				//Check if the collision normal is valid.
				if(!IsCloseAll(m_LandingData.m_CrashData.m_vCollisionNormal, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)))
				{
					static dev_float s_fMinSpeedToTakeDamage = 10.0f;
					static dev_float s_fMinSpeedForInstantKill = 20.0f;

					float fSpeed = (GetPed()->GetVelocity().Dot(VEC3V_TO_VECTOR3(m_LandingData.m_CrashData.m_vCollisionNormal)) * -1.0f);
					if(fSpeed >= s_fMinSpeedForInstantKill)
					{
						bInstantKill = true;
					}
					else if(fSpeed >= s_fMinSpeedToTakeDamage)
					{
						fDamage = (fSpeed - s_fMinSpeedToTakeDamage) * 10.0f;
					}
				}
			}
		}
	}

	//Ensure the damage is valid.
	if((fDamage <= 0.0f) && !bInstantKill)
	{
		return;
	}

	//Generate the flags.
	fwFlags32 uFlags = bInstantKill ? CPedDamageCalculator::DF_FatalMeleeDamage : CPedDamageCalculator::DF_None;

	//Apply the damage.
	u32 uWeaponHash = WEAPONTYPE_FALL;
	CEventDamage tempDamageEvent(NULL, fwTimer::GetTimeInMilliseconds(), uWeaponHash);
	int nComponent = RAGDOLL_SPINE0;
	CPedDamageCalculator damageCalculator(NULL, fDamage, uWeaponHash, nComponent, false);
	damageCalculator.ApplyDamageAndComputeResponse(GetPed(), tempDamageEvent.GetDamageResponseData(), uFlags);
}

void CTaskParachute::ApplyExtraForcesForParachuting()
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}

	//Calculate the brake.
	float fBrake = m_fBrakingMult / ms_fMaxBraking;

	//Check if we are not at a full brake.
	if(fBrake < 1.0f)
	{
		//Apply the normal forces.

		//Calculate the normal.
		float fNormal = 1.0f - fBrake;

		//Apply the turn force (from stick).
		ApplyExtraForcesFromStick(fNormal, m_pParachuteObject, sm_Tunables.m_ExtraForces.m_Parachuting.m_Normal.m_TurnFromStick);

		//Apply the turn force (from roll).
		ApplyExtraForcesFromValue(fNormal, m_pParachuteObject->GetTransform().GetRoll(), m_pParachuteObject, sm_Tunables.m_ExtraForces.m_Parachuting.m_Normal.m_TurnFromRoll);
	}

	//Check if we are braking at all.
	if(fBrake > 0.0f)
	{
		//Apply the braking forces.

		//Apply the turn force.
		ApplyExtraForcesFromStick(fBrake, m_pParachuteObject, sm_Tunables.m_ExtraForces.m_Parachuting.m_Braking.m_TurnFromStick);
	}
}

void CTaskParachute::ApplyExtraForcesFromStick(float fScale, CPhysical* pPhysical, const Tunables::ExtraForces::FromStick& rForce)
{
	//Grab the stick value.
	float fStick = rForce.m_UseVerticalAxis ? m_vRawStick.y : m_vRawStick.x;

	//Apply the extra force from the value.
	ApplyExtraForcesFromValue(fScale, fStick, pPhysical, rForce.m_FromValue);
}

void CTaskParachute::ApplyExtraForcesFromValue(float fScale, float fValue, CPhysical* pPhysical, const Tunables::ExtraForces::FromValue& rForce)
{
	//Assert that the physical is valid.
	taskAssert(pPhysical);

	//Keep track of the force.
	Vector3 vForce = VEC3_ZERO;

	//Clamp the value.
	float fValueForMin = rForce.m_ValueForMin;
	float fValueForMax = rForce.m_ValueForMax;
	fValue = Clamp(fValue, fValueForMin, fValueForMax);

	//Grab the forces.
	Vector3 vForceForMin	= rForce.m_MinValue.ToVector3();
	Vector3 vForceForZero	= rForce.m_ZeroValue.ToVector3();
	Vector3 vForceForMax	= rForce.m_MaxValue.ToVector3();

	//Check the value.
	if(fValue == 0.0f)
	{
		vForce = vForceForZero;
	}
	else if(fValue < 0.0f)
	{
		float fLerp = (fValue / fValueForMin);
		vForce = Lerp(fLerp, vForceForZero, vForceForMin);
	}
	else
	{
		float fLerp = (fValue / fValueForMax);
		vForce = Lerp(fLerp, vForceForZero, vForceForMax);
	}

	//Scale the force.
	vForce *= (fScale * pPhysical->GetMass() * -GRAVITY);

	//Check if the force is local.
	if(rForce.m_IsLocal)
	{
		//Grab the transform.
		Mat34V mTransform = pPhysical->GetMatrix();

		//Check if we should clear the Z values.
		if(rForce.m_ClearZ)
		{
			//Clear the Z components.
			Mat34V mTransformNoZ = mTransform;
			mTransformNoZ.SetM20f(0.0f);
			mTransformNoZ.SetM21f(0.0f);
			mTransformNoZ.SetM22f(0.0f);

			//Normalize the matrix.
			ReOrthonormalize3x3(mTransform, mTransformNoZ);
		}

		//Rotate the force to world coordinates.
		vForce = VEC3V_TO_VECTOR3(Transform3x3(mTransform, VECTOR3_TO_VEC3V(vForce)));
	}

	//Apply the force.
	pPhysical->ApplyForceCg(vForce);
}

bool CTaskParachute::AreCameraSwitchesDisabled() const
{
	//Check if we are deploying.
	if(GetState() == State_Deploying)
	{
		return true;
	}

	//Check if we are landing.
	if(GetState() == State_Landing)
	{
		return true;
	}

	return false;
}

void CTaskParachute::AttachMoveNetwork()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	if (pPed && pPed->IsNetworkClone())
	{
		//early out if the ped is in ragdoll as you won't be able to AttachMoveNetwork to them - it will assert
		if (pPed->GetUsingRagdoll())
			return;
	}

	//Assert that the move network is not active.
	taskAssert(!m_networkHelper.IsNetworkActive());

	//Assert that the move network is streamed in.
	taskAssert(m_networkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskParachute));

	//Create the network player.
	m_networkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskParachute);

	//Attach the network player to an empty slot.
	Assert(pPed->GetAnimDirector());

	float fBlendTime = 0.0f;
	if(!m_iFlags.IsFlagSet(PF_InstantBlend))
	{
		fBlendTime = m_iFlags.IsFlagSet(PF_UseLongBlend) ? SLOW_BLEND_DURATION : NORMAL_BLEND_DURATION;
	}

	pPed->GetMovePed().SetTaskNetwork(m_networkHelper.GetNetworkPlayer(), fBlendTime);
}

void CTaskParachute::AttachMoveNetworkIfNecessary()
{
	CPed* pPed = GetPed();
	if (pPed && pPed->IsNetworkClone())
	{
		//early out if the ped is in ragdoll as you won't be able to AttachMoveNetwork to them - it will assert
		if (pPed->GetUsingRagdoll())
			return;
	}

	//Ensure the move network is not active or that it's not part of the move network (if the ped switches to ragdoll due to a collision, it's active but not the anim task network)
	if(m_networkHelper.IsNetworkActive() && m_networkHelper.IsNetworkAttached())
	{
		return;
	}

	//If we have a network that is active, but we don't have an attached player - then release the network player and allow the AttachMoveNetwork to process properly below.  Fixes url:bugstar:1816616, lavalley.
	if(m_networkHelper.IsNetworkActive() && !m_networkHelper.IsNetworkAttached())
	{
		parachuteWarningf("CTaskParachute::AttachMoveNetworkIfNecessary--(m_networkHelper.IsNetworkActive() && !m_networkHelper.IsNetworkAttached())-->m_networkHelper.ReleaseNetworkPlayer() then AttachMoveNetwork");
		m_networkHelper.ReleaseNetworkPlayer();
	}

	//Attach the move network.
	AttachMoveNetwork();
}

void CTaskParachute::AverageBrakes(float& fLeftBrake, float& fRightBrake) const
{
	//It is pretty difficult for players to achieve the full range of brakes by themselves.
	//For this reason, if both brakes are pressed AND are within a certain range, we will
	//average the braking values and make sure they are equal (so that the player doesn't tilt).
	
	//This doesn't apply at the absolutes -- when both brakes are at zero or full.
	//This is because the player may want to make slight adjustments with either brake.
	//However, when both brakes are in-between, this is highly unlikely.

	//Ensure both brakes are non-zero.
	if(fLeftBrake == 0.0f || fRightBrake == 0.0f)
	{
		return;
	}
	
	//Ensure both brakes are non-full.
	if(fLeftBrake == 1.0f || fRightBrake == 1.0f)
	{
		return;
	}
	
	//Ensure the difference is within the threshold.
	float fDifference = Abs(fRightBrake - fLeftBrake);
	if(fDifference > sm_Tunables.m_MaxDifferenceToAverageBrakes)
	{
		return;
	}
	
	//Average the value.
	float fBrake = (fLeftBrake + fRightBrake) * 0.5f;

	//Assign the values.
	fLeftBrake	= fBrake;
	fRightBrake	= fBrake;
}

float CTaskParachute::CalculateBlendForIdleAndMotion() const
{
	return m_fTotalStickInput;
}

float CTaskParachute::CalculateBlendForMotionAndBrake() const
{
	//Calculate the amount to blend the motion anims with the braking animations.
	//There are a few situations to take into account.
	//	1) Full brake, no movement -- the braking anims should take over.
	//	2) Full brake, full movement -- the braking/movement anims should be blended equally.
	//	3) No brake, full movement -- the movement anims should take over.
	//	4) No brake, no movement -- the movement anims should take over.
	float fBlend = Clamp(1.0f - m_fLeftBrake - m_fRightBrake, 0.0f, 1.0f);
	fBlend = Clamp(fBlend + m_fTotalStickInput, 0.0f, 1.0f);

	return fBlend;
}

float CTaskParachute::CalculateBlendForXAxisTurns() const
{
	return m_fStickX;
}

float CTaskParachute::CalculateBlendForYAxisTurns() const
{
	return m_fStickY;
}

float CTaskParachute::CalculateBlendForXAndYAxesTurns() const
{
	return m_fCurrentHeading;
}

float CTaskParachute::CalculateBrakeForParachutingAi(float fFlatDistanceFromPedToTargetSq, float fAngleToTargetZ) const
{
	//Ensure we are within the braking distance.
	float fMaxDistanceForBrakeSq = square(sm_Tunables.m_ParachutingAi.m_Brake.m_MaxDistance);
	if(fFlatDistanceFromPedToTargetSq > fMaxDistanceForBrakeSq)
	{
		return 0.0f;
	}
	
	//Clamp the brake distance to values we care about.
	float fDistanceToStartBrakingSq = square(sm_Tunables.m_ParachutingAi.m_Brake.m_DistanceToStart);
	float fDistanceForFullBrakingSq = square(sm_Tunables.m_ParachutingAi.m_Brake.m_DistanceForFull);
	float fBrakeDistanceSq = Clamp(fFlatDistanceFromPedToTargetSq, fDistanceForFullBrakingSq, fDistanceToStartBrakingSq);

	//Normalize the value.
	float fValue = (fBrakeDistanceSq - fDistanceForFullBrakingSq) / (fDistanceToStartBrakingSq - fDistanceForFullBrakingSq);

	//Calculate the brake based on the distance.
	float fBrakeFromDistance = Lerp(fValue, 1.0f, 0.0f);

	//Clamp the brake angle to values we care about.
	float fAngleForMaxBrake = sm_Tunables.m_ParachutingAi.m_Brake.m_AngleForMax;
	float fAngleForMinBrake = sm_Tunables.m_ParachutingAi.m_Brake.m_AngleForMin;
	float fBrakeAngle = Clamp(fAngleToTargetZ, fAngleForMaxBrake, fAngleForMinBrake);

	//Normalize the value.
	fValue = (fBrakeAngle - fAngleForMaxBrake) / (fAngleForMinBrake - fAngleForMaxBrake);

	//Calculate the brake based on the angle.
	float fBrakeFromAngle = Lerp(fValue, 1.0f, 0.0f);

	//Calculate the brake.
	float fBrake = Max(fBrakeFromDistance, fBrakeFromAngle);
	
	return fBrake;
}

void CTaskParachute::CalculateDesiredFlightAnglesForParachuting(float fBrake, float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const
{
	//Clear the values.
	fDesiredPitch	= 0.0f;
	fDesiredRoll	= 0.0f;
	fDesiredYaw		= 0.0f;

	//The brake should range from 0 ... 1.
	if(!taskVerifyf(fBrake >= 0.0f && fBrake <= 1.0f, "The brake value is out of range: %.2f.", fBrake))
	{
		return;
	}

	//Check if the brake is full or none.
	bool bBrakeNone = (fBrake == 0.0f);
	bool bBrakeFull = (fBrake == 1.0f);

	//Calculate the desired normal flight angles for parachuting.
	float fDesiredPitchN	= 0.0f;
	float fDesiredRollN		= 0.0f;
	float fDesiredYawN		= 0.0f;
	if(!bBrakeFull)
	{
		CalculateDesiredFlightAnglesForParachutingWithLimits(sm_Tunables.m_FlightAngleLimitsForParachutingNormal, fDesiredPitchN, fDesiredRollN, fDesiredYawN);
	}

	//Calculate the desired braking flight angles for parachuting.
	float fDesiredPitchB	= 0.0f;
	float fDesiredRollB		= 0.0f;
	float fDesiredYawB		= 0.0f;
	if(!bBrakeNone)
	{
		CalculateDesiredFlightAnglesForParachutingWithLimits(sm_Tunables.m_FlightAngleLimitsForParachutingBraking, fDesiredPitchB, fDesiredRollB, fDesiredYawB);
	}

	//Calculate the weights.
	float fWeightN = 1.0f - fBrake;
	float fWeightB = fBrake;

	//Calculate the desired values based on the weights.
	fDesiredPitch	= ((fDesiredPitchN	* fWeightN) + (fDesiredPitchB	* fWeightB));
	fDesiredRoll	= ((fDesiredRollN	* fWeightN) + (fDesiredRollB	* fWeightB));
	fDesiredYaw		= ((fDesiredYawN	* fWeightN) + (fDesiredYawB		* fWeightB));
}

void CTaskParachute::CalculateDesiredFlightAnglesForParachutingWithLimits(const Tunables::FlightAngleLimitsForParachuting& rLimits, float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const
{
	//Calculate the desired pitch.
	CalculateDesiredPitchFromStickInput input(rLimits.m_MinPitch, rLimits.m_MaxPitch);
	fDesiredPitch = CalculateDesiredPitchFromStick(input);

	//Calculate the desired roll from the stick.
	float fDesiredRollFromStick = CalculateDesiredRollFromStick(rLimits.m_MaxRollFromStick);

	//Calculate the desired roll from the brake.
	float fDesiredRollFromBrake = CalculateDesiredRollFromBrake(rLimits.m_MaxRollFromBrake);

	//Calculate the desired roll.
	fDesiredRoll = Clamp(fDesiredRollFromStick + fDesiredRollFromBrake, -rLimits.m_MaxRoll, rLimits.m_MaxRoll);

	//Calculate the desired yaw from the stick.
	float fDesiredYawFromStick = CalculateDesiredYawFromStick(rLimits.m_MaxYawFromStick);

	//Calculate the desired yaw from the roll.
	float fDesiredYawFromRoll = CalculateDesiredYawFromRoll(rLimits.m_MaxYawFromRoll, rLimits.m_RollForMinYaw, rLimits.m_RollForMaxYaw);

	//Calculate the desired yaw.
	fDesiredYaw = Clamp(fDesiredYawFromStick + fDesiredYawFromRoll, -rLimits.m_MaxYaw, rLimits.m_MaxYaw);
}

void CTaskParachute::CalculateDesiredFlightAnglesForSkydiving(float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const
{
	//Calculate the desired flight angles for skydiving.
	CalculateDesiredFlightAnglesForSkydivingWithLimits(sm_Tunables.m_FlightAngleLimitsForSkydiving, fDesiredPitch, fDesiredRoll, fDesiredYaw);
}

void CTaskParachute::CalculateDesiredFlightAnglesForSkydivingWithLimits(const Tunables::FlightAngleLimitsForSkydiving& rLimits, float& fDesiredPitch, float& fDesiredRoll, float& fDesiredYaw) const
{
	//Calculate the desired pitch.
	CalculateDesiredPitchFromStickInput input(rLimits.m_MinPitch, rLimits.m_MaxPitch);
	input.m_bUseInflectionPitch = true;
	input.m_fInflectionPitch = rLimits.m_InflectionPitch;
	fDesiredPitch = CalculateDesiredPitchFromStick(input);

	//Calculate the desired roll.
	fDesiredRoll = CalculateDesiredRollFromStick(rLimits.m_MaxRoll);

	//Calculate the desired yaw.
	fDesiredYaw = CalculateDesiredYawFromStick(rLimits.m_MaxYaw);
}

float CTaskParachute::CalculateDesiredPitchFromStick(const CalculateDesiredPitchFromStickInput& rInput) const
{
	//Keep track of the desired pitch.
	float fDesiredPitch = 0.0f;

	//Grab the stick value.
	float fStick = m_vRawStick.y;

	//Check the sign.
	//This allows us to have imbalanced min/max pitch values, and still have a zero pitch when the stick is idle.
	if(fStick > 0.0f)
	{
		//Calculate the desired pitch.
		fDesiredPitch = Lerp(fStick, 0.0f, rInput.m_fMinPitch);
	}
	else if(fStick < 0.0f)
	{
		//Normalize the value.
		fStick = -fStick;

		//Check if we are not using inflection pitch.
		if(!rInput.m_bUseInflectionPitch)
		{
			//Calculate the desired pitch.
			fDesiredPitch = Lerp(fStick, 0.0f, rInput.m_fMaxPitch);
		}
		else
		{
			//Lerp to the inflection pitch for the first half.
			if(fStick <= 0.5f)
			{
				//Normalize the value.
				fStick *= 2.0f;

				//Calculate the desired pitch.
				fDesiredPitch = Lerp(fStick, 0.0f, rInput.m_fInflectionPitch);
			}
			//Lerp to the max pitch for the second half.
			else
			{
				//Normalize the value.
				fStick -= 0.5f;
				fStick *= 2.0f;

				//Calculate the desired pitch.
				fDesiredPitch = Lerp(fStick, rInput.m_fInflectionPitch, rInput.m_fMaxPitch);
			}
		}
	}

	return fDesiredPitch;
}

float CTaskParachute::CalculateDesiredRollFromBrake(float fMaxRoll) const
{
	//Calculate the delta.
	float fDelta = m_fRawRightBrake - m_fRawLeftBrake;

	//Calculate the desired roll.
	float fDesiredRoll = fDelta * fMaxRoll;

	return fDesiredRoll;
}

float CTaskParachute::CalculateDesiredRollFromStick(float fMaxRoll) const
{
	//Grab the stick value.
	float fStick = m_vRawStick.x;

	//Calculate the desired roll.
	float fDesiredRoll = fStick * fMaxRoll;

	return fDesiredRoll;
}

float CTaskParachute::CalculateDesiredYawFromRoll(float fMaxYaw, float fRollForMinYaw, float fRollForMaxYaw) const
{
	//Grab the roll value.
	float fRoll = m_pParachuteObject ? m_pParachuteObject->GetTransform().GetRoll() : 0.0f;
	
	//Negate the roll.
	//This is because positive roll means we are rolling left,
	//and negative roll means we are rolling right.
	fRoll = -fRoll;
	
	//Clamp the roll to values we care about.
	float fValue = Clamp(Abs(fRoll), fRollForMinYaw, fRollForMaxYaw);
	
	//Normalize the value.
	float fRange = Max(fRollForMaxYaw - fRollForMinYaw, FLT_EPSILON);
	fValue = (fValue - fRollForMinYaw) / fRange;
	
	//Calculate the desired yaw.
	float fDesiredYaw = Lerp(fValue, 0.0f, fMaxYaw);
	
	//Update the sign.
	fDesiredYaw *= Sign(fRoll);
	
	return fDesiredYaw;
}

float CTaskParachute::CalculateDesiredYawFromStick(float fMaxYaw) const
{
	//Grab the stick value.
	float fStick = m_vRawStick.x;

	//Calculate the desired yaw.
	float fDesiredYaw = fStick * fMaxYaw;

	return fDesiredYaw;
}

float CTaskParachute::CalculateLiftFromPitchForSkydiving() const
{
	//Grab the pitch value.
	float fPitch = m_fPitch;

	//Normalize the value.
	float fMaxPitch = sm_Tunables.m_FlightAngleLimitsForSkydiving.m_MaxPitch;
	fPitch = Clamp(fPitch, 0.0f, fMaxPitch);
	fPitch /= fMaxPitch;

	//Calculate the lift.
	float fMaxLift = sm_Tunables.m_ForcesForSkydiving.m_MaxLift;
	float fLift = Lerp(fPitch, 0.0f, fMaxLift);

	return fLift;
}

Vec3V_Out CTaskParachute::CalculateLinearV(float fBrake) const
{
	//The brake ranges from 0 ... 1.
	taskAssertf(fBrake >= 0.0f && fBrake <= 1.0f, "The brake value is out of range: %.2f.", fBrake);

	//Check if the brake is full or none.
	bool bBrakeNone = (fBrake == 0.0f);
	bool bBrakeFull = (fBrake == 1.0f);

	//Calculate the normal linear velocity.
	Vec3V vLinearVN = !bBrakeFull ? CalculateLinearVNormal() : Vec3V(V_ZERO);

	//Calculate the braking linear velocity.
	Vec3V vLinearVB = !bBrakeNone ? CalculateLinearVBraking() : Vec3V(V_ZERO);

	//Calculate the weights.
	ScalarV scWeightN = Subtract(ScalarV(V_ONE), ScalarVFromF32(fBrake));
	ScalarV scWeightB = ScalarVFromF32(fBrake);

	//Calculate the linear velocity based on the weights.
	return Add(Scale(vLinearVN, scWeightN), Scale(vLinearVB, scWeightB));
}

Vec3V_Out CTaskParachute::CalculateLinearVBraking() const
{
	//When tightly circling, it has been requested that the parachute descend quicker.

	//Calculate the difference in the brakes.
	//This value will range from 0 ... 1.
	float fDifference = Abs(m_fLeftBrake - m_fRightBrake);

	//Clamp the difference to the values that matter, and normalize it.
	//This value will range from 0 ... 1.
	fDifference = Clamp(fDifference, sm_Tunables.m_BrakingDifferenceForLinearVZMin, sm_Tunables.m_BrakingDifferenceForLinearVZMax);
	fDifference -= sm_Tunables.m_BrakingDifferenceForLinearVZMin;
	fDifference /= (sm_Tunables.m_BrakingDifferenceForLinearVZMax - sm_Tunables.m_BrakingDifferenceForLinearVZMin);

	//Lerp the value.
	float fZ = Lerp(fDifference, sm_Tunables.m_LinearVZForBrakingDifferenceMin, sm_Tunables.m_LinearVZForBrakingDifferenceMax);

	return Vec3V(0.0f, 0.0f, fZ);
}

Vec3V_Out CTaskParachute::CalculateLinearVNormal() const
{
	//When pitched down, it has been requested that the parachute descend quicker.

	//Calculate the pitch ratio.
	//This value will range from -1 ... 1.
	float fMaxPitch = sm_Tunables.m_FlightAngleLimitsForParachutingNormal.m_MaxPitch;
	float fRatio = m_fPitch / fMaxPitch;

	//Clamp the pitch ratio to the values that matter, and normalize it.
	//This value will range from 0 ... 1.
	fRatio = Clamp(fRatio, sm_Tunables.m_PitchRatioForLinearVZMin, sm_Tunables.m_PitchRatioForLinearVZMax);
	fRatio -= sm_Tunables.m_PitchRatioForLinearVZMin;
	fRatio /= (sm_Tunables.m_PitchRatioForLinearVZMax - sm_Tunables.m_PitchRatioForLinearVZMin);

	//Lerp the value.
	float fZ = Lerp(fRatio, sm_Tunables.m_LinearVZForPitchRatioMin, sm_Tunables.m_LinearVZForPitchRatioMax);

	return Vec3V(0.0f, 0.0f, fZ);
}

float CTaskParachute::CalculateParachutePitchForMove() const
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return 0.5f;
	}

	//Grab the pitch.  Use the inverse of the pitch, to match up with the other parameters.
	float fPitch = -m_pParachuteObject->GetTransform().GetPitch();

	//Clamp the pitch.
	float fMinPitch = sm_Tunables.m_MoveParameters.m_Parachuting.m_MinParachutePitch;
	float fMaxPitch = sm_Tunables.m_MoveParameters.m_Parachuting.m_MaxParachutePitch;
	fPitch = Clamp(fPitch, fMinPitch, fMaxPitch);

	//Normalize the value.
	fPitch -= fMinPitch;
	fPitch /= (fMaxPitch - fMinPitch);

	return fPitch;
}

float CTaskParachute::CalculateParachuteRollForMove() const
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return 0.5f;
	}

	//Grab the roll.  Use the inverse of the roll, to match up with the other parameters.
	float fRoll = -m_pParachuteObject->GetTransform().GetRoll();

	//Clamp the roll.
	float fMaxRoll = sm_Tunables.m_MoveParameters.m_Parachuting.m_MaxParachuteRoll;
	fRoll = Clamp(fRoll, -fMaxRoll, fMaxRoll);

	//Normalize the value.
	fRoll += fMaxRoll;
	fRoll /= (2.0f * fMaxRoll);

	return fRoll;
}

float CTaskParachute::CalculateStickForPitchForParachutingAi(float fBrake, float fFlatDistanceFromPedToTargetSq, float fAngleDifference, float fAngleDifferenceLastFrame) const
{
	//The brake ranges from 0 ... 1.
	taskAssertf(fBrake >= 0.0f && fBrake <= 1.0f, "The brake value is out of range: %.2f.", fBrake);

	//Check if the brake is full or none.
	bool bBrakeNone = (fBrake == 0.0f);
	bool bBrakeFull = (fBrake == 1.0f);

	//Calculate the normal stick.
	float fStickN = !bBrakeFull ? CalculateStickForPitchForParachutingAiWithParameters(sm_Tunables.m_ParachutingAi.m_PitchForNormal, fFlatDistanceFromPedToTargetSq, fAngleDifference, fAngleDifferenceLastFrame) : 0.0f;

	//Calculate the braking stick.
	float fStickB = !bBrakeNone ? CalculateStickForPitchForParachutingAiWithParameters(sm_Tunables.m_ParachutingAi.m_PitchForBraking, fFlatDistanceFromPedToTargetSq, fAngleDifference, fAngleDifferenceLastFrame) : 0.0f;

	//Calculate the weights.
	float fWeightN = 1.0f - fBrake;
	float fWeightB = fBrake;

	//Calculate the stick based on the weights.
	return ((fStickN * fWeightN) + (fStickB * fWeightB));
}

float CTaskParachute::CalculateStickForPitchForParachutingAiWithParameters(const Tunables::ParachutingAi::Pitch& rParameters, float UNUSED_PARAM(fFlatDistanceFromPedToTargetSq), float fAngleDifference, float fAngleDifferenceLastFrame) const
{
	//Ensure the angle difference last frame is set.
	if(fAngleDifferenceLastFrame == FLT_MAX)
	{
		return 0.0f;
	}

	//Calculate the change in angle difference this frame.
	float fChangeInAngleDifferenceThisFrame = fAngleDifference - fAngleDifferenceLastFrame;

	//Calculate the desired change in angle difference this frame.
	float fDesiredChangeInAngleDifferenceThisFrame = -fAngleDifference / rParameters.m_DesiredTimeToResolveAngleDifference * GetTimeStep();

	//Calculate the delta between the desired and actual angle differences.
	float fDelta = fDesiredChangeInAngleDifferenceThisFrame - fChangeInAngleDifferenceThisFrame;

	//Clamp the delta to values we care about.
	float fValue = Clamp(Abs(fDelta), 0.0f, rParameters.m_DeltaForMaxStickChange);
	fValue /= rParameters.m_DeltaForMaxStickChange;

	//Calculate the stick change.
	float fStickChange = fValue * Sign(fDelta);
	float fMaxStickChange = rParameters.m_MaxStickChangePerSecond * GetTimeStep();
	fStickChange = Clamp(fStickChange, -fMaxStickChange, fMaxStickChange);

	//Calculate the stick value.
	float fStick = m_vRawStick.y + fStickChange;
	fStick = Clamp(fStick, -1.0f, 1.0f);
	
	return fStick;
}

float CTaskParachute::CalculateStickForRollForParachutingAi(float fBrake, float fFlatDistanceFromPedToTargetSq, float fAngleDifference) const
{
	//The brake ranges from 0 ... 1.
	taskAssertf(fBrake >= 0.0f && fBrake <= 1.0f, "The brake value is out of range: %.2f.", fBrake);

	//Check if the brake is full or none.
	bool bBrakeNone = (fBrake == 0.0f);
	bool bBrakeFull = (fBrake == 1.0f);

	//Calculate the normal stick.
	float fStickN = !bBrakeFull ? CalculateStickForRollForParachutingAiWithParameters(sm_Tunables.m_ParachutingAi.m_RollForNormal, fFlatDistanceFromPedToTargetSq, fAngleDifference) : 0.0f;

	//Calculate the braking stick.
	float fStickB = !bBrakeNone ? CalculateStickForRollForParachutingAiWithParameters(sm_Tunables.m_ParachutingAi.m_RollForBraking, fFlatDistanceFromPedToTargetSq, fAngleDifference) : 0.0f;

	//Calculate the weights.
	float fWeightN = 1.0f - fBrake;
	float fWeightB = fBrake;

	//Calculate the stick based on the weights.
	return ((fStickN * fWeightN) + (fStickB * fWeightB));
}

float CTaskParachute::CalculateStickForRollForParachutingAiWithParameters(const Tunables::ParachutingAi::Roll& rParameters, float UNUSED_PARAM(fFlatDistanceFromPedToTargetSq), float fAngleDifference) const
{
	//Ensure the angle exceeds the minimum necessary for a turn.
	float fAngleDifferenceForMin = rParameters.m_AngleDifferenceForMin;
	float fAbsAngleDifference = Abs(fAngleDifference);
	if(fAbsAngleDifference < fAngleDifferenceForMin)
	{
		return 0.0f;
	}
	
	//Clamp the angle difference to values we care about.
	float fAngleDifferenceForMax = rParameters.m_AngleDifferenceForMax;
	float fValue = Clamp(fAbsAngleDifference, fAngleDifferenceForMin, fAngleDifferenceForMax);
	fValue -= fAngleDifferenceForMin;
	fValue /= (fAngleDifferenceForMax - fAngleDifferenceForMin);
	
	//Calculate the stick value.
	float fStickValueForMin = rParameters.m_StickValueForMin;
	float fStickValueForMax = rParameters.m_StickValueForMax;
	float fStick = Lerp(fValue, fStickValueForMin, fStickValueForMax);
	fStick *= Sign(fAngleDifference);
	
	//Clamp the stick value.
	fStick = Clamp(fStick, -1.0f, 1.0f);
	
	return fStick;
}

Vec3V_Out CTaskParachute::CalculateTargetPosition() const
{
	//Check if the target entity is valid.
	if(m_pTargetEntity)
	{
		//Transform the target position.
		return m_pTargetEntity->GetTransform().Transform(m_vTargetPosition);
	}
	else
	{
		//Use the target position.
		return m_vTargetPosition;
	}
}

Vec3V_Out CTaskParachute::CalculateTargetPositionFuture() const
{
	//Calculate the target position.
	Vec3V vTargetPosition = CalculateTargetPosition();

	//Ensure the target entity is valid.
	if(!m_pTargetEntity)
	{
		return vTargetPosition;
	}

	//Ensure we are parachuting.
	if(GetState() != State_Parachuting)
	{
		return vTargetPosition;
	}

	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return vTargetPosition;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Generate a vector from the ped to the target.
	Vec3V vPedToTarget = Subtract(vTargetPosition, vPedPosition);
	
	//Ensure the ped is above the target.
	ScalarV scOffsetZ = vPedToTarget.GetZ();
	if(IsGreaterThanAll(scOffsetZ, ScalarV(V_ZERO)))
	{
		return vTargetPosition;
	}

	//Grab the target velocity.
	Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(m_pTargetEntity->GetVelocity());

	//Grab the parachute velocity.
	Vec3V vParachuteVelocity = VECTOR3_TO_VEC3V(m_pParachuteObject->GetVelocity());

	//Ensure the ped will reach the target eventually.
	ScalarV scVelocityZ = Subtract(vParachuteVelocity.GetZ(), vTargetVelocity.GetZ());
	if(IsGreaterThanAll(scVelocityZ, ScalarV(V_ZERO)))
	{
		return vTargetPosition;
	}
	
	//Ensure the velocity difference is valid.
	scVelocityZ = Min(ScalarV(V_NEGONE), scVelocityZ);

	//Figure out how long it will take the ped to descend to the target height.
	ScalarV scTime = Scale(scOffsetZ, Invert(scVelocityZ));
	if(IsLessThanAll(scTime, ScalarV(V_ZERO)))
	{
		return vTargetPosition;
	}
	
	//Clamp the time.
	scTime = Clamp(scTime, ScalarV(V_ZERO), ScalarV(sm_Tunables.m_MaxTimeToLookAheadForFutureTargetPosition));

	//Calculate the future target position.
	Vec3V vFutureTargetPosition = AddScaled(vTargetPosition, vTargetVelocity, scTime);

	return vFutureTargetPosition;
}

float CTaskParachute::CalculateThrustFromPitchForSkydiving() const
{
	//Grab the pitch value.
	float fPitch = m_fPitch;

	//Check if we have reached the inflection point.
	float fInflectionPitch = sm_Tunables.m_FlightAngleLimitsForSkydiving.m_InflectionPitch;
	if(fPitch <= fInflectionPitch)
	{
		//Normalize the value.
		fPitch = Clamp(fPitch, 0.0f, fInflectionPitch);
		fPitch /= fInflectionPitch;

		//Calculate the thrust.
		float fMaxThrust = sm_Tunables.m_ForcesForSkydiving.m_MaxThrust;
		float fThrust = Lerp(fPitch, 0.0f, fMaxThrust);

		return fThrust;
	}
	else
	{
		//Normalize the value.
		float fMaxPitch = sm_Tunables.m_FlightAngleLimitsForSkydiving.m_MaxPitch;
		fPitch = Clamp(fPitch, fInflectionPitch, fMaxPitch);
		fPitch -= fInflectionPitch;
		fPitch /= (fMaxPitch - fInflectionPitch);

		//Calculate the thrust.
		float fMaxThrust = sm_Tunables.m_ForcesForSkydiving.m_MaxThrust;
		float fThrust = Lerp(fPitch, fMaxThrust, 0.0f);

		return fThrust;
	}
}

bool CTaskParachute::CanAim() const
{
	//Ensure aiming is not disabled.
	if(sm_Tunables.m_Aiming.m_Disabled)
	{
		return false;
	}

	//Ensure the flag is not set.
	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_DisableAimingWhileParachuting))
	{
		return false;
	}

	//Ensure the weapon manager is valid.
	if(!GetPed()->GetWeaponManager())
	{
		return false;
	}

	CTask *pPhoneTask = GetPed()->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
	if(pPhoneTask)
	{
		return false;
	}

	return true;
}

bool CTaskParachute::CanDetachParachute() const
{
	return (GetState() == State_Parachuting);
}

bool CTaskParachute::CanUseFirstPersonCamera() const
{
#if FPS_MODE_SUPPORTED
	//Check if we are the local player.
	if(GetPed()->IsLocalPlayer())
	{
		return true;
	}
#endif

	return false;
}

bool CTaskParachute::CanUseLowLod(bool bUseLowLod) const
{
	//Check if we want to use low lod.
	if(bUseLowLod)
	{
		//Ensure the state is valid.
		switch(GetState())
		{
			case State_Landing:
			case State_CrashLand:
			{
				return false;
			}
			default:
			{
				break;
			}
		}

		//Ensure the animations are streamed in.
		if(m_ClipSetRequestHelperForLowLod.IsInvalid() || !m_ClipSetRequestHelperForLowLod.IsLoaded())
		{
			return false;
		}
	}
	else
	{
		//Check if the skydiving animations need to be streamed in.
		if(ShouldStreamInClipSetForSkydiving())
		{
			//Ensure the animations are streamed in.
			if(m_ClipSetRequestHelperForSkydiving.IsInvalid() || !m_ClipSetRequestHelperForSkydiving.IsLoaded())
			{
				return false;
			}
		}

		//Check if the parachuting animations need to be streamed in.
		if(ShouldStreamInClipSetForParachuting())
		{
			//Ensure the animations are streamed in.
			if(m_ClipSetRequestHelperForParachuting.IsInvalid() || !m_ClipSetRequestHelperForParachuting.IsLoaded())
			{
				return false;
			}
		}
	}

	return true;
}

CTaskParachute::LandingData::Type CTaskParachute::ChooseLandingToUse() const
{
	//The ped is being dragged along by the parachute object through an attachment.
	//For this reason we look at the parachute velocity instead of the ped velocity.
	float fMagSq = m_pParachuteObject ? m_pParachuteObject->GetVelocity().XYMag2() : 0.0f;

	//Check if we are moving slow enough to use the slow landing.
	if(fMagSq <= square(sm_Tunables.m_Landing.m_MaxVelocityForSlow))
	{
		return LandingData::Slow;
	}

	//Check if we landed on a vehicle.
	const CEntity* pLandedOn = m_LandingData.m_pLandedOn;
	if(pLandedOn && pLandedOn->GetIsTypeVehicle())
	{
		return LandingData::Slow;
	}

	//Check if we don't have a clear runway.
	if(!HasClearRunwayForLanding())
	{
		return LandingData::Slow;
	}

	//Check if we are moving fast enough to use the fast landing.
	if(fMagSq >= square(sm_Tunables.m_Landing.m_MinVelocityForFast))
	{
		return LandingData::Fast;
	}

	return LandingData::Regular;
}

int CTaskParachute::ChooseStateForSkydiving() const
{
	//Check if the skydiving clip set is loaded.
	if(fwClipSetManager::IsStreamedIn_DEPRECATED(GetClipSetForSkydiving()))
	{
		return State_Skydiving;
	}
	else
	{
		return State_WaitingForSkydiveToStream;
	}
}

void CTaskParachute::ClearParachutePackVariation()
{
	//Clear the parachute pack variation.
	ClearParachutePackVariationForPed(*GetPed());
}

CTask* CTaskParachute::CreateTaskForAmbientClips() const
{
	// Not called for clones but just in case - tasks synced via the QI...
	if(GetPed() && GetPed()->IsNetworkClone())
	{
		return NULL;
	}

	return rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("PLAYER_IDLES"));
}

CTask* CTaskParachute::CreateTaskForAiming() const
{
	taskAssert(GetPed()->IsPlayer());

	// Not called for clones but just in case - tasks synced via the QI...
	if(GetPed() && GetPed()->IsNetworkClone())
	{
		return NULL;
	}

	return rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Player);
}

CTask* CTaskParachute::CreateTaskForCrashLanding() 
{	
	if(m_LandingData.m_nType == LandingData::Water)
	{
		return CreateTaskForWaterLanding();
	}
	else
	{
		//Check if we crash landed with a parachute.
		if(m_LandingData.m_CrashData.m_bWithParachute)
		{
			return CreateTaskForCrashLandingWithParachute(VEC3V_TO_VECTOR3(m_LandingData.m_CrashData.m_vCollisionNormal));
		}
		else
		{
			return CreateTaskForCrashLandingWithNoParachute();
		}
	}
}

CTask* CTaskParachute::CreateTaskForWaterLanding()
{
	if (CTaskNMBehaviour::CanUseRagdoll(GetPed(), RAGDOLL_TRIGGER_FALL))
	{
		return rage_new CTaskNMControl(250, 5000, rage_new CTaskNMHighFall( 750 ), CTaskNMControl::DO_BLEND_FROM_NM, 10.0f);
	}
	else
	{
		return NULL;
	}
}

CTask* CTaskParachute::CreateTaskForCrashLandingWithNoParachute()
{
	CPed* pPed = GetPed();
	if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
	{
		return rage_new CTaskNMControl(500, 5000, rage_new CTaskNMHighFall(500), CTaskNMControl::DO_BLEND_FROM_NM, 10.0f);
	}
	else
	{
		return rage_new CTaskFallAndGetUp(CEntityBoundAI::FRONT, 1.0f);
	}
}

CTask* CTaskParachute::CreateTaskForCrashLandingWithParachute(const Vector3& vCollisionNormal)
{
	CPed* pPed = GetPed();
	if (CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_FALL))
	{
		// B* 1577458 - convert to using the smart fall nm task here, as it's bright enough not to kick around like an idiot when triggered close to the ground
		if (NetworkInterface::IsGameInProgress())
		{
			return rage_new CTaskNMControl(500, 5000, rage_new CTaskNMHighFall(500, NULL, CTaskNMHighFall::HIGHFALL_JUMP_COLLISION), CTaskNMControl::DO_BLEND_FROM_NM, 10.0f);
		}
		else
		{
			return rage_new CTaskNMControl(500, 5000, rage_new CTaskNMFallDown(500, 5000, CTaskNMFallDown::TYPE_FROM_HIGH, vCollisionNormal, 1.0f), CTaskNMControl::DO_BLEND_FROM_NM, 10.0f);
		}
	}
	else
	{
		CTaskFallAndGetUp* fallAndGetUp = rage_new CTaskFallAndGetUp(CEntityBoundAI::FRONT, 1.0f);
		if(pPed->IsNetworkClone() && GetStateFromNetwork() != State_CrashLand)
		{
			// if the clone is crashing and the owner is not, tell the fall and get up to just run locally
			fallAndGetUp->SetRunningLocally(true);
		}

		return fallAndGetUp;
	}
}

void CTaskParachute::DependOnParachute(bool bDepend)
{
	//Check if we should depend on the parachute.
	if(bDepend && !IsDependentOnParachute())
	{
		taskAssert(m_pParachuteObject);

		parachuteDebugf1("Frame %d : Ped %p %s : %s : Now depending on the parachute.", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);

		//Set the flag.
		m_uRunningFlags.SetFlag(RF_IsDependentOnParachute);

		//Set the animation update flags.
		GetPed()->AddSceneUpdateFlags(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2);
		GetPed()->RemoveSceneUpdateFlags(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS);

#if __ASSERT
		ValidateSceneUpdateFlags();
#endif
	}
	else if(!bDepend && IsDependentOnParachute())
	{
		parachuteDebugf1("Frame %d : Ped %p %s : %s : No longer depending on the parachute.", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);

		//Set the animation update flags.
		GetPed()->AddSceneUpdateFlags(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS);
		GetPed()->RemoveSceneUpdateFlags(CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2);

		//Clear the flag.
		m_uRunningFlags.ClearFlag(RF_IsDependentOnParachute);

#if __ASSERT
		ValidateSceneUpdateFlags();
#endif
	}
}

fwMvClipSetId CTaskParachute::GetClipSetForBase() const
{
#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && m_ClipSetRequestHelperForFirstPersonBase.GetClipSetId() != CLIP_SET_ID_INVALID)
	{
		return m_ClipSetRequestHelperForFirstPersonBase.GetClipSetId();
	}
#endif

	return m_ClipSetRequestHelperForBase.GetClipSetId();
}

fwMvClipSetId CTaskParachute::GetClipSetForParachuting() const
{
	if(m_bIsUsingLowLod)
	{
		 return m_ClipSetRequestHelperForLowLod.GetClipSetId();
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && m_ClipSetRequestHelperForFirstPersonParachuting.GetClipSetId() != CLIP_SET_ID_INVALID)
	{
		return m_ClipSetRequestHelperForFirstPersonParachuting.GetClipSetId();
	}
#endif

	return m_ClipSetRequestHelperForParachuting.GetClipSetId();
}

fwMvClipSetId CTaskParachute::GetClipSetForSkydiving() const
{
	if(m_bIsUsingLowLod)
	{
		return m_ClipSetRequestHelperForLowLod.GetClipSetId();
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && m_ClipSetRequestHelperForFirstPersonSkydiving.GetClipSetId() != CLIP_SET_ID_INVALID)
	{
		return m_ClipSetRequestHelperForFirstPersonSkydiving.GetClipSetId();
	}
#endif

	return m_ClipSetRequestHelperForSkydiving.GetClipSetId();
}

fwMvClipSetId CTaskParachute::GetClipSetForState() const
{
	//Check the state.
	switch(GetState())
	{
		case State_TransitionToSkydive:
		case State_WaitingForSkydiveToStream:
		{
			return GetClipSetForBase();
		}
		case State_Skydiving:
		{
			return GetClipSetForSkydiving();
		}
		case State_Deploying:
		case State_Parachuting:
		case State_Landing:
		{
			return GetClipSetForParachuting();
		}
		default:
		{
			return CLIP_SET_ID_INVALID;
		}
	}
}

#if __ASSERT
void CTaskParachute::ValidateSceneUpdateFlags()
{
	fwSceneUpdateExtension* pExtension = GetPed()->GetExtension<fwSceneUpdateExtension>();
	if(pExtension)
	{
		bool bInPass1 = (pExtension->m_sceneUpdateFlags & CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS) > 0;
		bool bInPass2 = (pExtension->m_sceneUpdateFlags & CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2) > 0;
		taskAssertf((bInPass1 && !bInPass2) || (!bInPass1 && bInPass2),
			"Ped has been placed in either zero or both animation update passes, this is very bad.  Pass 1: %d, Pass 2: %d", bInPass1, bInPass2);
	}
}
#endif

atHashWithStringNotFinal CTaskParachute::GetModelForParachute(const CPed *pPed)
{
	TUNE_GROUP_BOOL(TASK_PARACHUTE, bUsePilotSchoolModel, false);
	if(bUsePilotSchoolModel)
	{
		return atHashWithStringNotFinal("pil_p_para_pilot_sp_s");
	}

	TUNE_GROUP_BOOL(TASK_PARACHUTE, bUseLTSModel, false);
	if(bUseLTSModel)
	{
		return atHashWithStringNotFinal ("lts_p_para_pilot2_sp_s",0x73268708);
	}

	// With the reserve parachute now having its own separate model, we need to check which model we want to deploy based on the ped config flags
	if (pPed && pPed->GetPlayerInfo())
	{
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseReserveParachute))
		{
			if (pPed->GetPlayerInfo()->GetPedReserveParachuteModelOverride() != 0)
			{
				return atHashWithStringNotFinal(pPed->GetPlayerInfo()->GetPedReserveParachuteModelOverride());
			}
		}
		else
		{
			if (pPed->GetPlayerInfo()->GetPedParachuteModelOverride() != 0)
			{
				return atHashWithStringNotFinal(pPed->GetPlayerInfo()->GetPedParachuteModelOverride());
			}
		}
	}

	return (!NetworkInterface::IsGameInProgress() ? sm_Tunables.m_ModelForParachuteInSP : sm_Tunables.m_ModelForParachuteInMP);
}

atHashWithStringNotFinal CTaskParachute::GetModelForParachutePack(const CPed *pPed)
{
	TUNE_GROUP_BOOL(TASK_PARACHUTE, bUsePilotSchoolPackModel, false);
	if(bUsePilotSchoolPackModel)
	{
		static atHashWithStringNotFinal s_hPilotSchoolProp("lts_P_para_bag_Pilot2_S",0x4baa1f65);
		return s_hPilotSchoolProp;
	}

	TUNE_GROUP_BOOL(TASK_PARACHUTE, bUseLTSPackModel, false);
	if(bUseLTSPackModel)
	{
		static atHashWithStringNotFinal s_hLTSProp("lts_P_para_bag_LTS_S",0x4c28bd84);
		return s_hLTSProp;
	}

	TUNE_GROUP_BOOL(TASK_PARACHUTE, bUseXmasPackModel, false);
	if(bUseXmasPackModel)
	{
		static atHashWithStringNotFinal s_hXmasProp("P_para_bag_Xmas_S",0x694d27c4);
		return s_hXmasProp;
	}

	TUNE_GROUP_BOOL(TASK_PARACHUTE, bUseVinewoodPackModel, false);
	if (bUseVinewoodPackModel)
	{
		static atHashWithStringNotFinal s_hVinewoodProp("vw_p_para_bag_vine_s", 0x93321CAD);
		return s_hVinewoodProp;
	}

	//! Just return override variation if script are overriding.
	if(pPed && pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->GetPedParachutePackModelOverride() != 0)
	{
		return atHashWithStringNotFinal(pPed->GetPlayerInfo()->GetPedParachutePackModelOverride());
	}

	static atHashWithStringNotFinal s_hProp("P_Parachute_S",0x4BB13D0D);
	return s_hProp;
}

u32 CTaskParachute::GetTintIndexForParachute(const CPed& rPed)
{
	return rPed.GetPedIntelligence()->GetCurrentTintIndexForParachute();
}

u32 CTaskParachute::GetTintIndexForParachutePack(const CPed& rPed)
{
#if __BANK
	TUNE_GROUP_BOOL(TASK_PARACHUTE, bOverrideTintIndexForParachutePack, false);
	if(bOverrideTintIndexForParachutePack)
	{
		TUNE_GROUP_INT(TASK_PARACHUTE, uOverrideTintIndexForParachutePack, 0, 0, 7, 1);
		return (u32)uOverrideTintIndexForParachutePack;
	}
#endif

	//Check if we should randomize.
	bool bRandomize = (!NetworkInterface::IsGameInProgress() && !CTheScripts::GetPlayerIsOnAMission());
	if(bRandomize)
	{
		return (u32)fwRandom::GetRandomNumberInRange(0, 8);
	}

	return (rPed.GetPedIntelligence()->GetTintIndexForParachutePack());
}

u32 CTaskParachute::GetTintIndexFromParachutePackVariation() const
{
	return GetTintIndexFromParachutePackVariationForPed(*GetPed());
}

void CTaskParachute::HandleParachuteLanding(fwFlags8 uParachuteLandingFlags)
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}
	
	//Check if we should crumple the parachute.
	if(uParachuteLandingFlags.IsFlagSet(PLF_Crumple))
	{
		//Crumple the parachute.
		CrumpleParachute();
	}
	
	//Check if we should clear the horizontal velocity of the parachute.
	if(uParachuteLandingFlags.IsFlagSet(PLF_ClearHorizontalVelocity))
	{
		//Clear the horizontal velocity.
		Vector3 vVelocity = m_pParachuteObject->GetVelocity();
		vVelocity.x = 0.0f;
		vVelocity.y = 0.0f;
		m_pParachuteObject->SetVelocity(vVelocity);
	}
	
	//Check if we should reduce the mass of the parachute.
	if(uParachuteLandingFlags.IsFlagSet(PLF_ReduceMass))
	{
		//Reduce the mass.
		m_pParachuteObject->SetMass(sm_Tunables.m_ParachuteMassReduced);
	}

	//Check if we are using a slow landing.
	if(m_LandingData.m_nType == LandingData::Slow)
	{
		//Set the velocity.
		Vector3 vVelocity = (m_LandingData.m_pLandedOn && m_LandingData.m_pLandedOn->GetIsPhysical()) ?
			static_cast<const CPhysical *>(m_LandingData.m_pLandedOn.Get())->GetVelocity() : VEC3_ZERO;
		GetPed()->SetVelocity(vVelocity);
	}

	//Destroy the parachute.
	DestroyParachute();
}

bool CTaskParachute::HasClearRunwayForLanding() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Grab the ped's position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Grab the ped's forward vector (with Z component removed).
	Vec3V vPedForward = pPed->GetTransform().GetForward();
	vPedForward.SetZ(ScalarV(V_ZERO));
	vPedForward = NormalizeFastSafe(vPedForward, Vec3V(V_ZERO));

	//Rotate the forward vector.
	float fAngleForRunway = sm_Tunables.m_Landing.m_AngleForRunway;
	QuatV qRotation = QuatVFromAxisAngle(pPed->GetTransform().GetRight(), ScalarVFromF32(fAngleForRunway));
	Vec3V vForward = Transform(qRotation, vPedForward);

	//Calculate the look-ahead for the runway.
	float fLookAheadForRunway = sm_Tunables.m_Landing.m_LookAheadForRunway;
	float fAdjustedLookAheadForRunway = fLookAheadForRunway / cosf(fAngleForRunway);

	//Generate the future ped position.
	Vec3V vFuturePedPosition = vPedPosition;
	vFuturePedPosition = AddScaled(vPedPosition, vForward, ScalarVFromF32(fAdjustedLookAheadForRunway));

	//Generate the radius for the runway probes.
	static float s_fRadiusForProbes = 0.25f;

#if DEBUG_DRAW
	//Check if we should render the runway probes.
	if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_RunwayProbes)
	{
		//Draw the capsule probe.
		grcDebugDraw::Capsule(vPedPosition, vFuturePedPosition, s_fRadiusForProbes, Color_blue, true, -1);
	}
#endif

	//Check for obstructions on the runway.
	WorldProbe::CShapeTestCapsuleDesc obstructionsCapsuleDesc;
	obstructionsCapsuleDesc.SetCapsule(VEC3V_TO_VECTOR3(vPedPosition), VEC3V_TO_VECTOR3(vFuturePedPosition), s_fRadiusForProbes);
	obstructionsCapsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	obstructionsCapsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
	obstructionsCapsuleDesc.SetExcludeEntity(pPed);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(obstructionsCapsuleDesc))
	{
		return false;
	}

	//Calculate the drop for the runway.
	float fDropForRunway = sm_Tunables.m_Landing.m_DropForRunway;
	float fAdjustedDropForRunway = fDropForRunway + (fLookAheadForRunway * tanf(fAngleForRunway));

	//Generate the drop position.
	Vec3V vDropPosition = vFuturePedPosition;
	vDropPosition.SetZ(Subtract(vDropPosition.GetZ(), ScalarVFromF32(fAdjustedDropForRunway)));

#if DEBUG_DRAW
	//Check if we should render the runway probes.
	if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_RunwayProbes)
	{
		//Draw the capsule probe.
		grcDebugDraw::Capsule(vFuturePedPosition, vDropPosition, s_fRadiusForProbes, Color_blue, true, -1);
	}
#endif

	//Check if there is a drop.
	WorldProbe::CShapeTestCapsuleDesc dropCapsuleDesc;
	dropCapsuleDesc.SetCapsule(VEC3V_TO_VECTOR3(vFuturePedPosition), VEC3V_TO_VECTOR3(vDropPosition), s_fRadiusForProbes);
	dropCapsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	dropCapsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
	dropCapsuleDesc.SetExcludeEntity(pPed);
	if(!WorldProbe::GetShapeTestManager()->SubmitTest(dropCapsuleDesc))
	{
		return false;
	}

	return true;
}

bool CTaskParachute::IsParachute(const CObject& rObject)
{
	//! If marked as parachute, trust it.
	if(rObject.GetIsParachute())
	{
		return true;
	}

	//Ensure the model info is valid.
	CBaseModelInfo* pBaseModelInfo = rObject.GetBaseModelInfo();
	if(!pBaseModelInfo)
	{
		return false;
	}

	//Ensure the object is a parachute.
	if(pBaseModelInfo->GetModelNameHash() != GetModelForParachute(NULL).GetHash())
	{
		return false;
	}

	return true;
}

bool CTaskParachute::IsAiming() const
{
	return (GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN));
}

bool CTaskParachute::IsDependentOnParachute() const
{
	return m_uRunningFlags.IsFlagSet(RF_IsDependentOnParachute);
}

bool CTaskParachute::IsParachuteInWater() const
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return false;
	}
	
	//Check if the parachute is in the water.
	if(m_pParachuteObject->GetIsInWater())
	{
		return true;
	}
	
	return false;
}

bool CTaskParachute::IsParachutePackVariationActive() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	return IsParachutePackVariationActiveForPed(*pPed);
}

bool CTaskParachute::IsPedAttachedToParachute() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return false;
	}
	
	//Ensure the attach parent matches the parachute.
	if(pPed->GetAttachParent() != m_pParachuteObject)
	{
		return false;
	}
	
	return true;
}

bool CTaskParachute::IsPedInWater() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Check if the ped is in the water.
	if(pPed->GetIsInWater())
	{
		return true;
	}

	//Check if the ped is swimming.
	if(pPed->GetIsSwimming())
	{
		return true;
	}
	
	//Check if the ped position is in the water.
	if(IsPositionInWater(pPed->GetTransform().GetPosition()))
	{
		return true;
	}

	return false;
}

bool CTaskParachute::IsPositionInWater(Vec3V_In vPosition) const
{
	//Calculate the water level.
	float fWaterZ;
	if(Water::GetWaterLevelNoWaves(VEC3V_TO_VECTOR3(vPosition), &fWaterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		//Check if the position is below the water level.
		if(vPosition.GetZf() <= fWaterZ)
		{
			return true;
		}
	}

	return false;
}

bool CTaskParachute::IsUsingFirstPersonCamera() const
{
#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, false, true, true, false))
	{
		return true;
	}
#endif

	return false;
}

void CTaskParachute::KillMovePlayer()
{
	//Ensure the ped is the local player.
	if(!GetPed()->IsLocalPlayer())
	{
		return;
	}

	//Ensure the move task is either invalid, or move player.
	CTask* pGeneralMovementTask = GetPed()->GetPedIntelligence()->GetGeneralMovementTask();
	if(pGeneralMovementTask && (pGeneralMovementTask->GetTaskType() != CTaskTypes::TASK_MOVE_PLAYER))
	{
		return;
	}

	//Check if the player is running a control movement.
	CTaskComplexControlMovement* pTask = static_cast<CTaskComplexControlMovement *>(
		GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
	if(pTask)
	{
		pTask->SetNewMoveTask(rage_new CTaskMoveStandStill());
	}
	else
	{
		GetPed()->GetPedIntelligence()->AddTaskMovement(rage_new CTaskMoveStandStill());
	}
}

void CTaskParachute::ProbeForParachuteIntersections()
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}

	//Ensure the physics inst is valid.
	phInst* pInst = m_pParachuteObject->GetCurrentPhysicsInst();
	if(!pInst)
	{
		return;
	}

	//Ensure the bound is valid.
	phBound* pBound = pInst->GetArchetype()->GetBound();
	if(!pBound)
	{
		return;
	}

	//Ensure the time is valid.
	if(fwTimer::GetTimeInMilliseconds() < m_uNextTimeToProbeForParachuteIntersections)
	{
		return;
	}

	//Set the time.
	static dev_u32 s_uTime = 1000;
	m_uNextTimeToProbeForParachuteIntersections = (fwTimer::GetTimeInMilliseconds() + s_uTime);

	//Build the include flags.
	u32 uIncludeFlags = 0;

	static dev_u32 s_uIncludeFlagsForInitialIntersections = ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE |
		ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
	if(!m_uProbeFlags.IsFlagSet(PrF_HasCheckedInitialIntersections))
	{
		uIncludeFlags |= s_uIncludeFlagsForInitialIntersections;
	}

	static dev_u32 s_uIncludeFlagsForPeds = ArchetypeFlags::GTA_PED_TYPE;
	if(!m_uProbeFlags.IsFlagSet(PrF_IsClearOfPeds))
	{
		uIncludeFlags |= s_uIncludeFlagsForPeds;
	}

	static dev_u32 s_uIncludeFlagsForParachutes = ArchetypeFlags::GTA_OBJECT_TYPE;
	if(!m_uProbeFlags.IsFlagSet(PrF_IsClearOfParachutes))
	{
		uIncludeFlags |= s_uIncludeFlagsForParachutes;
	}

	static dev_u32 s_uIncludeFlagsForVehicles = ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE;
	if(!m_uProbeFlags.IsFlagSet(PrF_IsClearOfLastVehicle))
	{
		uIncludeFlags |= s_uIncludeFlagsForVehicles;
	}

	//Ensure the include flags are valid.
	if(uIncludeFlags == 0)
	{
		return;
	}

	//Keep track of whether we are clear of peds, parachutes, and our last vehicle.
	bool bIsClearOfPeds			= true;
	bool bIsClearOfParachutes	= true;
	bool bIsClearOfLastVehicle	= true;

	//Build the exclude entities.
	const CEntity* aExcludeEntities[2];
	aExcludeEntities[0] = m_pParachuteObject.Get();
	aExcludeEntities[1] = GetPed();

	// cache my vehicle and the vehicle it's attached to if, for example a cargo-bob is carrying a van and I'm jumping out the van...
	CVehicle* myVehicle				= GetPed()->GetMyVehicle();
	CVehicle* myVehicleAttachParent = NULL;
	if(myVehicle && myVehicle->GetAttachParent() && myVehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
	{
		myVehicleAttachParent = static_cast<CVehicle*>(myVehicle->GetAttachParent());
	}

	//Submit the test.
	WorldProbe::CShapeTestBoundDesc boundTestDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	boundTestDesc.SetResultsStructure(&probeResults);
	boundTestDesc.SetBound(pBound);
	boundTestDesc.SetTransform(&RCC_MATRIX34(pInst->GetMatrix()));
	boundTestDesc.SetExcludeEntities(aExcludeEntities, 2);
	boundTestDesc.SetIncludeFlags(uIncludeFlags);
	boundTestDesc.SetTypeFlags(TYPE_FLAGS_ALL);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc))
	{
		//Iterate over the results.
		WorldProbe::ResultIterator it;
		for(it = probeResults.begin(); it < probeResults.last_result(); ++it)
		{
			//Ensure the hit entity is valid.
			CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());
			if(!pHitEntity)
			{
				continue;
			}

			//Check if the entity is a ped.
			if(pHitEntity->GetIsTypePed())
			{
				bIsClearOfPeds = false;
			}
			//Check if the entity is a parachute.
			else if(pHitEntity->GetIsTypeObject() && IsParachute(*static_cast<CObject *>(pHitEntity)))
			{
				bIsClearOfParachutes = false;
			}
			//Check if the entity is our last vehicle.
			else if(pHitEntity->GetIsTypeVehicle() && ((pHitEntity == myVehicle) || (pHitEntity == myVehicleAttachParent)))
			{
				bIsClearOfLastVehicle = false;
			}
			else
			{
				m_uProbeFlags.SetFlag(PrF_HasInvalidIntersection);
			}
		}
	}

	//Note that we have checked the initial intersections.
	m_uProbeFlags.SetFlag(PrF_HasCheckedInitialIntersections);

	//Set the flags.
	m_uProbeFlags.ChangeFlag(PrF_IsClearOfPeds,			bIsClearOfPeds);
	m_uProbeFlags.ChangeFlag(PrF_IsClearOfParachutes,	bIsClearOfParachutes);
	m_uProbeFlags.ChangeFlag(PrF_IsClearOfLastVehicle,	bIsClearOfLastVehicle);

	//Update the include flags.
	if(m_uProbeFlags.IsFlagSet(PrF_HasInvalidIntersection))
	{
		pInst->GetArchetype()->RemoveIncludeFlags(s_uIncludeFlagsForInitialIntersections);
	}
	else
	{
		if(m_uProbeFlags.IsFlagSet(PrF_IsClearOfPeds))
		{
			pInst->GetArchetype()->AddIncludeFlags(s_uIncludeFlagsForPeds);
		}
		else
		{
			pInst->GetArchetype()->RemoveIncludeFlags(s_uIncludeFlagsForPeds);
		}

		if(m_uProbeFlags.IsFlagSet(PrF_IsClearOfParachutes))
		{
			pInst->GetArchetype()->AddIncludeFlags(s_uIncludeFlagsForParachutes);
		}
		else
		{
			pInst->GetArchetype()->RemoveIncludeFlags(s_uIncludeFlagsForParachutes);
		}

		if(m_uProbeFlags.IsFlagSet(PrF_IsClearOfLastVehicle))
		{
			pInst->GetArchetype()->AddIncludeFlags(s_uIncludeFlagsForVehicles);
		}
		else
		{
			pInst->GetArchetype()->RemoveIncludeFlags(s_uIncludeFlagsForVehicles);
		}
	}

	//Update the instance include flags.
	if(pInst->IsInLevel())
	{
		CPhysics::GetLevel()->SetInstanceIncludeFlags(pInst->GetLevelIndex(), pInst->GetArchetype()->GetIncludeFlags());
	}
}

void CTaskParachute::ProcessAverageVelocity()
{
	static dev_float s_fOldPercent = 0.85f;
	static dev_float s_fNewPercent = 1.0f - s_fOldPercent;

	Vec3V vOld = Scale(m_vAverageVelocity,							ScalarVFromF32(s_fOldPercent));
	Vec3V vNew = Scale(VECTOR3_TO_VEC3V(GetPed()->GetVelocity()),	ScalarVFromF32(s_fNewPercent));

	m_vAverageVelocity = Add(vOld, vNew);
}

void CTaskParachute::ProcessCamera()
{
	//Ensure we are the local player.
	if(!GetPed()->IsLocalPlayer())
	{
		return;
	}

	//Check camera switches are disabled.
	if(AreCameraSwitchesDisabled())
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_BlockCameraSwitching, true );
	}

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
}

bool CTaskParachute::ProcessPostCamera()
{
	ProcessCameraSwitch();
	return false;
}

void CTaskParachute::ProcessCameraSwitch()
{
	//Check if the camera has changed.
	bool bWasUsingFirstPersonCameraLastFrame = m_bIsUsingFirstPersonCameraThisFrame;
	m_bIsUsingFirstPersonCameraThisFrame = IsUsingFirstPersonCamera();
	if(bWasUsingFirstPersonCameraLastFrame != m_bIsUsingFirstPersonCameraThisFrame)
	{
		//Send the request.
		m_networkHelper.SendRequest(ms_ClipSetChanged);

		//Set the clip set for the state.
		SetClipSetForState();

		//Reprocess 1st person params.
		ProcessFirstPersonParams();

		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
	}
}

void CTaskParachute::ProcessFirstPersonParams()
{
	if(m_networkHelper.IsNetworkActive())
	{
		m_networkHelper.SetFlag(IsUsingFirstPersonCamera(), ms_FirstPersonMode);

		float fFirstPersonHeading = 0.5f; //mid.
		float fFirstPersonPitch = 0.5f; //mid
		const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		if(pCamera)
		{
			Mat34V mtxCam = MATRIX34_TO_MAT34V(pCamera->GetFrame().GetWorldMatrix());

			Vector3 vDir = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform3x3(mtxCam.GetCol1()));
			vDir.z=0;
			vDir.Normalize();
			float fYaw = rage::Atan2f(vDir.x, vDir.y);
			fYaw = Clamp(fwAngle::LimitRadianAngle(fYaw), -HALF_PI, HALF_PI);
			fYaw /= HALF_PI;
			fFirstPersonHeading = (fYaw + 1.0f) * 0.5f; // convert to R [0->1] where 0 is left, 1 is right.
		
			float fPitchRatio = 0.5f;
			fPitchRatio = Clamp(pCamera->GetRelativePitch() / QUARTER_PI, -1.0f, 1.0f);
			fFirstPersonPitch = (fPitchRatio + 1.0f) * 0.5f;  
		}

		TUNE_GROUP_FLOAT(TASK_PARACHUTE, fFirstPersonCamHeadingLerpRate, 0.75f, 0.01f, 1.0f, 0.01f);
		m_fFirstPersonCameraHeading = Lerp(fFirstPersonCamHeadingLerpRate, m_fFirstPersonCameraHeading, fFirstPersonHeading);

		TUNE_GROUP_FLOAT(TASK_PARACHUTE, fFirstPersonCamHeadingPitchRate, 0.2f, 0.01f, 1.0f, 0.01f);
		m_fFirstPersonCameraPitch = Lerp(fFirstPersonCamHeadingPitchRate, m_fFirstPersonCameraPitch, fFirstPersonPitch);

		m_networkHelper.SetFloat(ms_FirstPersonCameraHeading, m_fFirstPersonCameraHeading);
		m_networkHelper.SetFloat(ms_FirstPersonCameraPitch, m_fFirstPersonCameraPitch);
	}
}

void CTaskParachute::ProcessDeployingPhysics()
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", GetStaticStateName(GetState()), __FUNCTION__);)

	//Grab the ped.
	CPed* pPed = GetPed();

	if(!pPed->IsNetworkClone())
	{
		ms_DeployingFlightModelHelper.ProcessFlightModel(pPed, 0.0f, 0.0f, 0.0f, 0.0f);
	}
	else
	{
		// use whatever velocity the owner is telling us to use now....
		pPed->SetVelocity(m_vSyncedVelocity);
		pPed->SetDesiredVelocity(m_vSyncedVelocity);

		CNetBlenderPed *netBlenderPed = SafeCast(CNetBlenderPed, pPed->GetNetworkObject()->GetNetBlender());

		// if we suddenly stop receiving positional updates from over the network stop trying to move towards that position otherwise we might start going up!
		if(netBlenderPed && !netBlenderPed->HasStoppedPredictingPosition())
		{
			TUNE_GROUP_FLOAT(TASK_PARACHUTE, DeployingPedToOwnerVelocityMuliplier, 1.5f, 0.0f, 10.0f, 0.01f);
			TUNE_GROUP_FLOAT(TASK_PARACHUTE, DeployingMinimumZVelocityDelta, 0.0f, -50.0f, 0.0f, 0.01f);

			Vector3 pedPos			= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 ownerPos		= NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
			Vector3 predictedPos	= static_cast<CNetBlenderPed*>(pPed->GetNetworkObject()->GetNetBlender())->GetCurrentPredictedPosition();

			TUNE_GROUP_BOOL(TASK_PARACHUTE, UsePredictedPositionInsteadOfLastRecieved, false);

			if(UsePredictedPositionInsteadOfLastRecieved)
			{
				ownerPos = predictedPos;
			}

			Vector3 pedToOwner		= ownerPos - pedPos;
			Vector3 pedToOwnerDir	= pedToOwner;
			pedToOwnerDir.Normalize();
			
			m_preDeployingVelocity	= pPed->GetVelocity();
			m_postDeployingVelocity	= m_preDeployingVelocity + (pedToOwnerDir * DeployingPedToOwnerVelocityMuliplier);
			
			// prevent the clone slowing to a halt or worse, moving upwards...
			if(m_postDeployingVelocity.z > DeployingMinimumZVelocityDelta)
			{
				m_postDeployingVelocity.z = DeployingMinimumZVelocityDelta;
			}

			pPed->SetVelocity(m_postDeployingVelocity);
		}
	}

	//Clamp the velocity.
	static dev_float s_fMaxVelocity = 100.0f;
	pPed->SetDesiredVelocityClamped(pPed->GetVelocity(),s_fMaxVelocity);
}

void CTaskParachute::ProcessFlags()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if we should block weapon switching.
	if(ShouldBlockWeaponSwitching())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
	}

	//Note that the ped is parachuting.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsParachuting, true);

	//Disable probes.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableProcessProbes, ShouldDisableProcessProbes());

	//Check if we should update parachute bones.
	if(ShouldUpdateParachuteBones())
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRender, true);
	}
	
	//Process giving the ped a parachute.
	ProcessGiveParachute();

	//Process the ped if they have teleported.
	ProcessHasTeleported();
}

void CTaskParachute::ProcessGiveParachute()
{
	//Ensure the ped is not a clone.
	if(GetPed()->IsNetworkClone())
	{
		return;
	}
	
	//Ensure the give parachute flag is set.
	if(!m_iFlags.IsFlagSet(PF_GiveParachute))
	{
		return;
	}
	
	//Clear the give parachute flag.
	m_iFlags.ClearFlag(PF_GiveParachute);
	
	//Give the ped a parachute.
	CreateParachuteInInventory();
}

void CTaskParachute::ProcessHasTeleported()
{
	//Ensure the has teleported flag has been set.
	if(!m_iFlags.IsFlagSet(PF_HasTeleported))
	{
		return;
	}

	//Clear the has teleported flag.
	m_iFlags.ClearFlag(PF_HasTeleported);

	//Initialize the ped in the air.
	InitialisePedInAir(GetPed());
}

void CTaskParachute::ProcessMoveParameters()
{
	//Ensure the move network is attached.
	if(!m_networkHelper.IsNetworkActive())
	{
		return;
	}
	
	//Process the move parameters for the state.
	switch(GetState())
	{
		case State_Skydiving:
		{
			ProcessMoveParametersSkydiving();
			break;
		}
		case State_Parachuting:
		{
			ProcessMoveParametersParachuting();
			break;
		}
		default:
		{
			break;
		}
	}
}

void CTaskParachute::ProcessMoveParametersParachuting()
{
	//Calculate the blend factor for the X axis turns.
	float fBlendXAxisTurns = CalculateBlendForXAxisTurns();
	
	//Calculate the blend factor for the Y axis turns.
	float fBlendYAxisTurns = CalculateBlendForYAxisTurns();
	
	//Calculate the blend factor for the X/Y axes turns.
	float fBlendXAndYAxesTurns = CalculateBlendForXAndYAxesTurns();
	
	//Calculate the blend factor between idle/motion anims.
	float fBlendIdleAndMotion = CalculateBlendForIdleAndMotion();
	
	//Calculate the blend factor between motion/brake anims.
	float fBlendMotionAndBrake = CalculateBlendForMotionAndBrake();

	//Calculate the parachute pitch.
	float fParachutePitch = CalculateParachutePitchForMove();

	//Calculate the parachute roll.
	float fParachuteRoll = CalculateParachuteRollForMove();
	
	//! convert combined range into [0.0 == brake left, 0.5 == both active, right == 1.0f]
	float fCombinedBrake = Clamp(((m_fRightBrake - m_fLeftBrake) + 1.0f) * 0.5f, 0.0f, 1.0f);

	//Set the move parameters.
	m_networkHelper.SetFloat(ms_xDirection,				fBlendXAxisTurns);
	m_networkHelper.SetFloat(ms_yDirection,				fBlendYAxisTurns);
	m_networkHelper.SetFloat(ms_Direction,				fBlendXAndYAxesTurns);
	m_networkHelper.SetFloat(ms_NormalDirection,		fBlendIdleAndMotion);
	m_networkHelper.SetFloat(ms_LeftBrake,				m_fLeftBrake);
	m_networkHelper.SetFloat(ms_RightBrake,				m_fRightBrake);
	m_networkHelper.SetFloat(ms_CombinedBrake,			fCombinedBrake);
	m_networkHelper.SetFloat(ms_BlendMotionWithBrake,	fBlendMotionAndBrake);
	m_networkHelper.SetFloat(ms_ParachutePitch,			fParachutePitch);
	m_networkHelper.SetFloat(ms_ParachuteRoll,			fParachuteRoll);
	
	//Ensure the parachute task is valid.
	CTaskParachuteObject* pTask = GetParachuteObjectTask();
	if(!pTask)
	{
		return;
	}
	
	//Process the move parameters.
	pTask->ProcessMoveParameters(fBlendXAxisTurns, fBlendYAxisTurns, fBlendXAndYAxesTurns, fBlendIdleAndMotion);
}

void CTaskParachute::ProcessMoveParametersSkydiving()
{
	//Set the move parameters.
	m_networkHelper.SetFloat(ms_xDirection,			m_fStickX);
	m_networkHelper.SetFloat(ms_yDirection,			m_fStickY);
	m_networkHelper.SetFloat(ms_NormalDirection,	m_fTotalStickInput);
	m_networkHelper.SetFloat(ms_Direction,			m_fCurrentHeading);
}

void CTaskParachute::ProcessPadShake()
{
	//Ensure we are the local player.
	if(!GetPed()->IsLocalPlayer())
	{
		return;
	}

	//Check if we are falling.
	bool bIsParachuteOut = IsParachuteOut();
	bool bIsFalling = !bIsParachuteOut && (GetState() <= State_Deploying);
	if(!bIsFalling)
	{
		return;
	}

	//Apply the pad shake.
	float fPitchForMinIntensity = sm_Tunables.m_PadShake.m_Falling.m_PitchForMinIntensity;
	float fPitchForMaxIntensity = sm_Tunables.m_PadShake.m_Falling.m_PitchForMaxIntensity;
	float fPitch = Clamp(m_fPitch, fPitchForMinIntensity, fPitchForMaxIntensity);
	float fLerp = (fPitch - fPitchForMinIntensity) / (fPitchForMaxIntensity - fPitchForMinIntensity);
	float fMinIntensity = sm_Tunables.m_PadShake.m_Falling.m_MinIntensity;
	float fMaxIntensity = sm_Tunables.m_PadShake.m_Falling.m_MaxIntensity;
	float fIntensity = Lerp(fLerp, fMinIntensity, fMaxIntensity);
	CControlMgr::StartPlayerPadShakeByIntensity(sm_Tunables.m_PadShake.m_Falling.m_Duration, fIntensity);
}

bool CTaskParachute::ProcessParachute()
{
	// No need to create a new chute if we are changing ownership of the ped, and as a result this task gets re-created
	if (IsPedChangingOwnership())
	{
		return true;
	}
	//Process the parachute object.
	ProcessParachuteObject();
	
	return true;
}

void CTaskParachute::ProcessParachuteBones()
{
	//Ensure we should update the parachute bones.
	if(!ShouldUpdateParachuteBones())
	{
		return;
	}
	
	//Update the parachute bones.
	UpdateParachuteBones();
}

void CTaskParachute::ProcessParachutingControls()
{
	// Make sure we're not a clone...
	Assert(GetPed() && !GetPed()->IsNetworkClone());

	//Check if we should use AI controls.
	if(ShouldUseAiControls())
	{
		//Process the parachuting AI controls.
		ProcessParachutingControlsForAi();
	}
	else if(ShouldUsePlayerControls())
	{
		//Process the parachuting player controls.
		ProcessParachutingControlsForPlayer();
	}
}

void CTaskParachute::ProcessParachutingControls(const Vector2& vStick, float fLeftBrake, float fRightBrake, bool bLeaveSmokeTrail, bool bDetachParachute, bool bAiming)
{
	// Make sure we're not a clone...
	Assert(GetPed() && !GetPed()->IsNetworkClone());

	//Assign the raw stick.
	m_vRawStick = vStick;
	
	//Assign the raw brakes.
	m_fRawLeftBrake		= fLeftBrake;
	m_fRawRightBrake	= fRightBrake;
	
	//Assign the smoke trail.
	m_bLeaveSmokeTrail = bLeaveSmokeTrail;
	
	//Calculate the max change values.
	const float fTimeStep = GetTimeStep();
	const float fMaxPitchChange		= fTimeStep * sm_Tunables.m_ChangeRatesForParachuting.m_Pitch;
	const float fMaxRollChange		= fTimeStep * sm_Tunables.m_ChangeRatesForParachuting.m_Roll;
	const float fMaxYawChange		= fTimeStep * sm_Tunables.m_ChangeRatesForParachuting.m_Yaw;
	const float fMaxBrakeChange		= fTimeStep * sm_Tunables.m_ChangeRatesForParachuting.m_Brake;
	
	//Smooth the brakes.
	SmoothValue(m_fLeftBrake,	fLeftBrake,		fMaxBrakeChange);
	SmoothValue(m_fRightBrake,	fRightBrake,	fMaxBrakeChange);
	
	//Check if we are braking.
	if(m_fLeftBrake > 0.0f || m_fRightBrake > 0.0f)
	{
		//Calculate the braking mult.
		float fBrakingMult = ((m_fLeftBrake) + Abs(m_fRightBrake)) * ms_fBrakingMult;
		m_fBrakingMult = Clamp(fBrakingMult, 0.0f, ms_fMaxBraking);
		
		//Note that we are braking.
		m_bIsBraking = true;
	}
	else
	{
		//Clear the braking mult.
		m_fBrakingMult = 0.0f;
		
		//Note that we are no longer braking.
		m_bIsBraking = false;
	}

	//Calculate the brake.
	float fBrake = m_fBrakingMult / ms_fMaxBraking;
	
	//Calculate the desired flight angles for parachuting.
	float fDesiredPitch	= 0.0f;
	float fDesiredRoll	= 0.0f;
	float fDesiredYaw	= 0.0f;
	CalculateDesiredFlightAnglesForParachuting(fBrake, fDesiredPitch, fDesiredRoll, fDesiredYaw);
	
	//Smooth the flight angles.
	SmoothValue(m_fPitch,	fDesiredPitch,	fMaxPitchChange);
	SmoothValue(m_fRoll,	fDesiredRoll,	fMaxRollChange);
	SmoothValue(m_fYaw,		fDesiredYaw,	fMaxYawChange);
	
	//Check if we should detach the parachute.
	if(bDetachParachute && CanDetachParachute())
	{
		//Detach the ped and the parachute.
		DetachPedAndParachute(true);

		//Clear the parachute pack variation.
		if(!GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute))
		{
			ClearParachutePackVariation();
		}
	}

	//Check if we are a player.
	if(GetPed()->IsPlayer())
	{
		//Process aiming.
		ProcessParachutingControlsForAiming(bAiming);
	}
}

void CTaskParachute::ProcessParachutingControlsForAi()
{
	// Make sure we're not a clone...
	Assert(GetPed() && !GetPed()->IsNetworkClone());

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Calculate the target position.
	Vector3 vTargetPosition = VEC3V_TO_VECTOR3(CalculateTargetPositionFuture());

	//Calculate the ped position.
	Vector3 vPedPosition = pPed->GetBonePositionCached(BONETAG_L_FOOT);
	vPedPosition.Average(pPed->GetBonePositionCached(BONETAG_R_FOOT));

	//Grab the ped's forward vector.
	Vector3 vPedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());

	//Calculate a vector from the ped to the target.
	Vector3 vPedToTarget = vTargetPosition - vPedPosition;

	//Calculate the flat distance from the ped to the target.
	float fFlatDistanceFromPedToTargetSq = vPedToTarget.XYMag2();

	//Check if the target position should be adjusted.
	float fMinDistanceToAdjustTargetSq = square(sm_Tunables.m_ParachutingAi.m_Target.m_MinDistanceToAdjust);
	if(fFlatDistanceFromPedToTargetSq > fMinDistanceToAdjustTargetSq)
	{
		//Adjust the pitch target position.
		vTargetPosition.z += sm_Tunables.m_ParachutingAi.m_Target.m_Adjustment;

		//Adjust the vector from the ped to the target.
		vPedToTarget.z += sm_Tunables.m_ParachutingAi.m_Target.m_Adjustment;
	}
	
	//Calculate the angles to the target.
	float fAngleToTargetX = Atan2f(-vPedToTarget.x, vPedToTarget.y);
	float fAngleToTargetZ = Atan2f( vPedToTarget.z, vPedToTarget.XYMag());
	
	//Calculate the ped forward angles.
	float fAnglePedForwardX = Atan2f(-vPedForward.x, vPedForward.y);
	float fAnglePedForwardZ = Atan2f( vPedForward.z, vPedForward.XYMag());
	
	//Calculate the angle differences.
	float fAngleDifferenceX = SubtractAngleShorter(fAnglePedForwardX, fAngleToTargetX);
	float fAngleDifferenceZ = SubtractAngleShorter(fAnglePedForwardZ, fAngleToTargetZ);
	
	//Calculate the brake value.
	float fBrake = CalculateBrakeForParachutingAi(fFlatDistanceFromPedToTargetSq, fAngleToTargetZ);
	
	//Calculate the stick value.
	Vector2 vStick;
	vStick.x = CalculateStickForRollForParachutingAi(fBrake, fFlatDistanceFromPedToTargetSq, fAngleDifferenceX);
	vStick.y = CalculateStickForPitchForParachutingAi(fBrake, fFlatDistanceFromPedToTargetSq, fAngleDifferenceZ, m_fAngleDifferenceZLastFrame);

	//Set the angle difference last frame.
	m_fAngleDifferenceZLastFrame = fAngleDifferenceZ;
	
	//Check if we should detach the parachute.
	bool bDetachParachute = m_bRemoveParachuteForScript;

	//Check if we are within the minimum drop distance of the target.
	float fMinDistanceToDropSq = square(sm_Tunables.m_ParachutingAi.m_Drop.m_MinDistance);
	float fMaxDistanceToDropSq = square(sm_Tunables.m_ParachutingAi.m_Drop.m_MaxDistance);
	if(fFlatDistanceFromPedToTargetSq > fMinDistanceToDropSq && fFlatDistanceFromPedToTargetSq < fMaxDistanceToDropSq)
	{
		//Check if we are within the minimum drop height of the target.
		float fMinHeightToDrop = sm_Tunables.m_ParachutingAi.m_Drop.m_MinHeight;
		float fMaxHeightToDrop = sm_Tunables.m_ParachutingAi.m_Drop.m_MaxHeight;
		if(vPedToTarget.z > fMinHeightToDrop && vPedToTarget.z < fMaxHeightToDrop)
		{
			//Check if we are within the drop facing threshold.
			float fFacingTargetDot = vPedToTarget.Dot(vPedForward);
			float fMaxDotForDrop = sm_Tunables.m_ParachutingAi.m_Drop.m_MaxDot;
			if(fFacingTargetDot < fMaxDotForDrop)
			{
				bDetachParachute = true;
			}
		}
	}
	
	//Process the parachuting controls.
	ProcessParachutingControls(vStick, fBrake, fBrake, false, bDetachParachute, false);
}

void CTaskParachute::ProcessParachutingControlsForAiming(bool bAiming)
{
	//Check if we can aim.
	bool bShouldAim = (bAiming && CanAim());
	if(bShouldAim)
	{
		//Check if our weapon is valid.
		bool bIsWeaponValid = GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetEquippedWeaponInfo() &&
			GetPed()->GetWeaponManager()->GetEquippedWeaponInfo()->CanBeUsedAsDriveBy(GetPed());
		if(!bIsWeaponValid)
		{
			bShouldAim = false;
		}
		else
		{
			//Check if the ped can driveby with the equipped weapon.
			if(!CVehicleMetadataMgr::CanPedDoDriveByWithEquipedWeapon(GetPed()))
			{
				bShouldAim = false;
			}
		}
	}

	//Check if we should aim.
	if(bShouldAim)
	{
		//Check if the control is valid.
		CControl* pControl = GetPed()->GetControlFromPlayer();
		if(pControl)
		{
			//There is a button conflict between cinematic camera and reload.
			pControl->SetInputExclusive(INPUT_VEH_CIN_CAM);
		}

		//Check if the sub-task is valid.
		CTask* pSubTask = GetSubTask();
		if(pSubTask)
		{
			//Check if the sub-task is not the right type.
			if(pSubTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_GUN)
			{
				//Request termination.
				taskAssert(pSubTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS);
				taskAssert(pSubTask->SupportsTerminationRequest());
				pSubTask->RequestTermination();
			}

			return;
		}

		//Create the task.
		pSubTask = CreateTaskForAiming();

		//Start the task.
		SetNewTask(pSubTask);
	}
	else
	{
		//Check if the sub-task is valid.
		CTask* pSubTask = GetSubTask();
		if(pSubTask)
		{
			//Check if the sub-task is not the right type.
			if(pSubTask->GetTaskType() != CTaskTypes::TASK_AMBIENT_CLIPS)
			{
				//Request termination.
				taskAssert(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN);
				taskAssert(pSubTask->SupportsTerminationRequest());
				pSubTask->RequestTermination();
			}

			return;
		}
		else
		{
			//Create the task.
			pSubTask = CreateTaskForAmbientClips();

			//Start the task.
			SetNewTask(pSubTask);
		}
	}
}

void CTaskParachute::ProcessParachutingControlsForPlayer()
{
	// Make sure we're not a clone...
	Assert(GetPed() && !GetPed()->IsNetworkClone());

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the control is valid.
	const CControl* pControl = pPed->GetControlFromPlayer();
	if(!pControl)
	{
		return;
	}

	//Read the stick input.
	Vector2 vStick;
	vStick.x = pControl->GetParachuteTurnLeftRight().GetNorm();
	vStick.y = pControl->GetParachutePitchUpDown().GetNorm();
	CFlightModelHelper::MakeCircularInputSquare(vStick);
	
	//Read the brake input.
	float fLeftBrake = pControl->GetParachuteBrakeLeft().GetNorm01();
	float fRightBrake = pControl->GetParachuteBrakeRight().GetNorm01();
	AverageBrakes(fLeftBrake, fRightBrake);

#if RSG_PC
	if(pControl->WasKeyboardMouseLastKnownSource() && pControl->GetParachutePrecisionLanding().IsDown())
	{
		fLeftBrake  = 1.0f;
		fRightBrake = 1.0f;
	}
#endif // RSG_PC
	
	//Read the smoke trail input.
	bool bLeaveSmokeTrail = pControl->GetParachuteSmoke().IsDown();
	
	//Read the detach input.
	bool bDetachParachute = pControl->GetParachuteDetach().IsPressed() || m_bRemoveParachuteForScript;

	//Check if we are aiming.
	bool bAiming = (pControl->GetPedTargetIsDown() || pControl->GetPedAttack().IsDown());
	
	//Process the parachuting controls.
	ProcessParachutingControls(vStick, fLeftBrake, fRightBrake, bLeaveSmokeTrail, bDetachParachute, bAiming);
}

void CTaskParachute::ProcessParachutingPhysics()
{
	BANK_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", GetStaticStateName(GetState()), __FUNCTION__);)

	// Work out what percentage of each of the normal and braking/landing flight models to use.
	float fBrakingPerc = m_fBrakingMult / ms_fMaxBraking;

	//Calculate the linear velocity.
	Vec3V vLinearV = CalculateLinearV(fBrakingPerc);

	// Normal flight model
	if( fBrakingPerc < 1.0f )
	{
		//Set the thrust multiplier based on the parachute thrust.
		ms_FlightModelHelper.SetThrustMult(m_fParachuteThrust);
		
		// Adjust the air speed based on the left stick input
		float fDesiredDescent = Max(m_vRawStick.y, 0.0f);
		Vector3 vAirSpeedIn = Lerp(fDesiredDescent, ms_vAdditionalAirSpeedNormalMax, ms_vAdditionalAirSpeedNormalMin);
		vAirSpeedIn = VEC3V_TO_VECTOR3(m_pParachuteObject->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAirSpeedIn)));

		//Set the linear velocity.
		ms_FlightModelHelper.SetDragCoeff(VEC3V_TO_VECTOR3(vLinearV), CFlightModelHelper::FT_Linear_V);

		// Adjust the drag applied based on the left stick input
		Vector3 vSquaredV = Lerp(fDesiredDescent, ms_vAdditionalDragNormalMax, ms_vAdditionalDragNormalMin);
		ms_FlightModelHelper.SetDragCoeff(vSquaredV, CFlightModelHelper::FT_Squared_V);
		
		//Process the flight model.
		ms_FlightModelHelper.ProcessFlightModel(m_pParachuteObject, 0.0f, m_fPitch, m_fRoll, m_fYaw, vAirSpeedIn, true, 1.0f - fBrakingPerc);
	}

	// Braking flight model
	if( fBrakingPerc > 0.0f )
	{
		// Adjust the air speed based on the left stick input
		float fDesiredDescent = (m_vRawStick.y+1.0f)*0.5f;
		Vector3 vAirSpeedBrakingIn = Lerp(fDesiredDescent, ms_vAdditionalAirSpeedBrakingMax, ms_vAdditionalAirSpeedBrakingMin);
		vAirSpeedBrakingIn = VEC3V_TO_VECTOR3(m_pParachuteObject->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vAirSpeedBrakingIn)));

		//Set the linear velocity.
		ms_BrakingFlightModelHelper.SetDragCoeff(VEC3V_TO_VECTOR3(vLinearV), CFlightModelHelper::FT_Linear_V);

		// Adjust the drag applied based on the left stick input
		Vector3 vSquaredV = Lerp(fDesiredDescent, ms_vAdditionalDragBrakingMax, ms_vAdditionalDragBrakingMin);
		ms_BrakingFlightModelHelper.SetDragCoeff(vSquaredV, CFlightModelHelper::FT_Squared_V);

		// Update the flight model
		ms_BrakingFlightModelHelper.ProcessFlightModel(m_pParachuteObject, 0.0f, m_fPitch, m_fRoll, m_fYaw, vAirSpeedBrakingIn, true, fBrakingPerc);
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Check if the ped is not in ragdoll mode.
	if(!pPed->GetUsingRagdoll())
	{
		//Apply the offset forces.
		ApplyCgOffsetForces(pPed);

		//Apply the extra forces.
		ApplyExtraForcesForParachuting();
	}
}

void CTaskParachute::ProcessParachuteObject()
{
	//Check if the parachute has control over the ped.
	if(m_bParachuteHasControlOverPed)
	{
		//Probe for intersections.
		ProbeForParachuteIntersections();
	}
	
	// override the parachute collision so it doesn't get turned on or off too early by the owner
	// if the parachute physics state gets synced before the taskparachute state
	if(GetPed() && GetPed()->IsNetworkClone())
	{
		if(m_pParachuteObject)
		{
			ProcessCloneParachuteObjectCollision();	
		}
	}

	//Ensure we should process the parachute object.
	if(!ShouldProcessParachuteObject())
	{
		parachuteDebugf3("CTaskParachute::ProcessParachuteObject--!ShouldProcessParachuteObject()-->return");
		return;
	}
	
	//Ensure we can create a parachute - clones get a synced object across the network...
	if(CanCreateParachute())
	{
		//Create the parachute object.
		CreateParachute();
	
		//Attach the parachute to the ped.
		AttachParachuteToPed();		
	}
	
	// However, clones and owners should still assign the parachute object it's task...
	if(m_pParachuteObject)
	{
		// if the clone finally has a parachute object 
		if(CanConfigureCloneParachute())
		{
			// configure it (set the render flags)...
			ConfigureCloneParachute();
		}

		// set the lod distance for the parachute and ped as I haven't had a chance to do this yet...
		SetParachuteAndPedLodDistance();

		// Add the parachuteobject task to the parachute...
		if(m_pParachuteObject->GetTask(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY) == NULL)
		{
			// TaskParachuteObject is now a cloned task so only create it on the owner....
			if(GetPed() && !GetPed()->IsNetworkClone())
			{
				//Give the parachute its task.
				CTaskParachuteObject* parachuteObjectTask = rage_new CTaskParachuteObject;
				m_pParachuteObject->SetTask(parachuteObjectTask, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY);
				
				//Update the task immediately.
				if(m_pParachuteObject->GetObjectIntelligence())
				{
					m_pParachuteObject->GetObjectIntelligence()->ForcePostCameraTaskUpdate();
				}
			}
		}
	}
#if !__FINAL
	else
	{
		parachuteDebugf3("CTaskParachute::ProcessParachuteObject--!m_pParachuteObject");
	}
#endif
}

void CTaskParachute::ProcessCloneParachuteObjectCollision()
{
	if(!GetPed() || !GetPed()->IsNetworkClone())
	{
		return;
	}

	if(!m_pParachuteObject || !m_pParachuteObject->GetNetworkObject())
	{
		return;
	}

	bool collisionShouldBeActive = false;

	switch(GetState())
	{
		case State_Start:							
		case State_WaitingForBaseToStream:			
		case State_BlendFromNM:						
		case State_WaitingForMoveNetworkToStream:	
		case State_TransitionToSkydive:				
		case State_WaitingForSkydiveToStream:		
		case State_Skydiving:						
			collisionShouldBeActive = false; 
		break; 
		case State_Deploying:						
			if(IsParachuteOut())
			{
				collisionShouldBeActive = false;  
			}
			else
			{
				collisionShouldBeActive = false;
			}
			break;
		case State_Parachuting:						
			collisionShouldBeActive = false;  
		break;
		case State_Landing:							 
		case State_CrashLand:
		case State_Quit:							
		default:						
			collisionShouldBeActive = false; 
		break;
	}

	((CNetObjEntity*)m_pParachuteObject->GetNetworkObject())->SetOverridingLocalCollision(collisionShouldBeActive, __FUNCTION__);	
}

void CTaskParachute::ProcessPed()
{
	//Process the movement for the ped.
	ProcessMovement();

	//Process the pitch for the capsule.
	PitchCapsuleBound();

	//Check if we are skydiving.
	if(GetState() < State_Deploying)
	{
		//Check if the facial data is valid.
		CFacialDataComponent* pFacialData = GetPed()->GetFacialData();
		if(pFacialData)
		{
			//Make a skydiving face.
			pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Skydive);
		}
	}

	//Process the average velocity.
	ProcessAverageVelocity();
}

bool CTaskParachute::UpdateClonePedRagdollCorrection(CPed* pPed)
{
	// network game not in progress?
	if(!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	// clones only...
	if(!pPed || !pPed->IsNetworkClone())
	{
		return false;
	}

	// Not using ragdoll, we're fine...
	if(!pPed->GetUsingRagdoll())
	{
		return false;
	}

	// valid to be in ragdoll...
	if((GetState() == State_CrashLand) || (GetState() == State_BlendFromNM))
	{
		return false;
	}

	// we were crashing but the network is trying to put us into landing so we should let it...
	if((GetPreviousState() == State_CrashLand) && (GetState() == State_Landing))
	{
		return false;
	}

	// if we continue in processing here - it will reset the state to BlendFromNM - which then will change to WaitingForMoveNetworkToStream - and this will cycle.
	// prevent the cycle here.
	if((GetPreviousState() == State_BlendFromNM) && (GetState() == State_WaitingForMoveNetworkToStream))
	{
		return false;
	}
	
	// we are about to deploy, this prevents an infinite task loop
	if (m_bForcePedToOpenChute)
	{
		return false;
	}

	// prevent cycle: Start->BlendFromNM->WaitingForMoveNetworkToStream->Start : lavalley (previously checked parachute object - but this can cycle if a parachute is removed from the player too)
	if ((GetPreviousState() == State_WaitingForMoveNetworkToStream)  && (GetState() == State_Start))
	{
		return false;
	}

	//-----------

#if !__FINAL 

	//static const char* parachuteFlagsStr[] = 
	//{
	//	"PF_HasTeleported",
	//	"PF_GiveParachute",
	//	"PF_SkipSkydiving",
	//	"PF_UseLongBlend",
	//	"PF_InstantBlend",
	//	"PF_HasTarget"
	//};

	//static const char* parachuteObjectTaskFlagsStr[] =
	//{
	//	"PORF_CanDeploy",
	//	"PORF_Deploy",
	//	"PORF_IsOut",
	//	"PORF_Crumple"
	//};

	// Display something on the TTY so we can find it for debugging....
	parachuteDebugf1("Frame %d : Task %p : %s : Ped %p %s : IsClone %d\nPerforming ragdoll correction on ped!\nCurState %s : ParachuteFlags = %d : m_pParachuteObject = %p : ParachuteObjectTask = %p state %s flags %d : IsParachuteOut() %d", 
	fwTimer::GetFrameCount(), 
	this, 
	__FUNCTION__, 
	pPed, 
	BANK_ONLY( pPed ? pPed->GetDebugName() :) "NO PED", 
	pPed->IsNetworkClone(),
	GetStaticStateName(GetState()),
	GetParachuteFlags().GetAllFlags(),
	m_pParachuteObject.Get(), 
	GetParachuteObjectTask(),
	GetParachuteObjectTask() != NULL ? CTaskParachuteObject::GetStaticStateName(GetParachuteObjectTask()->GetState()) : "NO PARACHUTE OBJECT TASK",
	GetParachuteObjectTask() != NULL ? GetParachuteObjectTask()->GetFlags().GetAllFlags() : 0,
	IsParachuteOut());

	for(int i = 0; i < PF_MAX_Flags; ++i)
	{
		if(GetParachuteFlags().GetAllFlags() & (1 << i))
		{
			parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);
		}
	}

	if(GetParachuteObjectTask())
	{
		for(int i = 0; i < CTaskParachuteObject::PORF_MAX_Flags; ++i)
		{
			if(GetParachuteObjectTask()->GetFlags().GetAllFlags() & (1 << i))
			{
				parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);
			}
		}
	}

	CNetObjPed* netObj = static_cast<CNetObjPed*>(pPed->GetNetworkObject());
	if(netObj)
	{
		netObj->DumpTaskInformation(__FUNCTION__);
	}
#endif /* !__FUNAL */

	//-----------

	// flag us as trying to repair the clone asap...
	m_bCloneRagdollTaskRelaunch			= true;
	m_uCloneRagdollTaskRelaunchState	= (s8)GetState();

	// Go back to blend from NM....
	TaskSetState(State_BlendFromNM);
	
	// Clear out the move network player (the task subnetwork already been removed by ragdolling from the ped but we want to set the m_networkHelper to inactive)
	m_networkHelper.ReleaseNetworkPlayer();

	// if we've got a parachute...
	if(m_pParachuteObject)
	{
		// just detach everything and start again....
		DetachPedAndParachute();

		// After blending from NM, we're going to be put back into whatever state the network is in
		switch(GetState())
		{
			case State_Start:
			case State_WaitingForBaseToStream:
			case State_BlendFromNM:
			case State_WaitingForMoveNetworkToStream:
			case State_TransitionToSkydive:
			case State_WaitingForSkydiveToStream:
			case State_Skydiving: 
			{
				AttachParachuteToPed();
			}
			break;
			case State_Deploying:
			{
				if(!IsParachuteOut())
				{
					// reattach - state deploy will detach and attach the ped to the parachute...
					AttachParachuteToPed(); 
				}
				else
				{
					AttachPedToParachute();
				}
			}
			break;
			case State_Parachuting:
			{
				// reattach the ped to the parachute...
				AttachPedToParachute();
			}
			break;
			case State_Landing: 
			case State_CrashLand:
			break;
			case State_Quit:	
			{
				// kill the task off, CleanUp will sort it out...
				TaskSetState(State_Quit);
				break;
			}
		}
	}

	return true;
}

void CTaskParachute::ProcessPostLod()
{
	//Ensure the move network is active.
	if(!m_networkHelper.IsNetworkActive())
	{
		return;
	}

	//Ensure a lod change is allowed this frame.
	if(!m_bIsLodChangeAllowedThisFrame)
	{
		return;
	}

	//Ensure the desired lod differs from the current lod.
	m_bShouldUseLowLod = ShouldUseLowLod();
	if(m_bShouldUseLowLod == m_bIsUsingLowLod)
	{
		return;
	}

	//Check if we can use low lod.
	bool bUseLowLod = m_bShouldUseLowLod;
	if(CanUseLowLod(bUseLowLod))
	{
		//Use low lod.
		UseLowLod(bUseLowLod);
	}
}

void CTaskParachute::ProcessPreLod()
{
	//Do not allow a lod change this frame.
	m_bIsLodChangeAllowedThisFrame = false;
}

void CTaskParachute::ProcessSkydivingControls()
{
	//Check if we should use AI controls.
	if(ShouldUseAiControls())
	{
		//Process the skydiving AI controls.
		ProcessSkydivingControlsForAi();
	}
	else if(ShouldUsePlayerControls())
	{
		//Process the skydiving player controls.
		ProcessSkydivingControlsForPlayer();
	}
}

void CTaskParachute::ProcessSkydivingControls(const Vector2& vStick)
{
	//Assign the raw stick.
	m_vRawStick = vStick;
	
	//Calculate the max change values.
	const float fTimeStep = GetTimeStep();
	const float fMaxPitchChange		= fTimeStep * sm_Tunables.m_ChangeRatesForSkydiving.m_Pitch;
	const float fMaxRollChange		= fTimeStep * sm_Tunables.m_ChangeRatesForSkydiving.m_Roll;
	const float fMaxYawChange		= fTimeStep * sm_Tunables.m_ChangeRatesForSkydiving.m_Yaw;
	
	//Calculate the desired flight angles for skydiving.
	float fDesiredPitch	= 0.0f;
	float fDesiredRoll	= 0.0f;
	float fDesiredYaw	= 0.0f;
	CalculateDesiredFlightAnglesForSkydiving(fDesiredPitch, fDesiredRoll, fDesiredYaw);
	
	//Smooth the flight angles.
	SmoothValue(m_fPitch,	fDesiredPitch,	fMaxPitchChange);
	SmoothValue(m_fRoll,	fDesiredRoll,	fMaxRollChange);
	SmoothValue(m_fYaw,		fDesiredYaw,	fMaxYawChange);
}

void CTaskParachute::ProcessSkydivingControlsForAi()
{
	//Skydiving AI goes here.
}

void CTaskParachute::ProcessSkydivingControlsForPlayer()
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the control is valid.
	const CControl* pControl = pPed->GetControlFromPlayer();
	if(!pControl)
	{
		return;
	}

	//Read the stick input.
	Vector2 vStick;
	vStick.x = pControl->GetParachuteTurnLeftRight().GetNorm();
	vStick.y = pControl->GetParachutePitchUpDown().GetNorm();
	CFlightModelHelper::MakeCircularInputSquare(vStick);
	
	//Process the skydiving controls.
	ProcessSkydivingControls(vStick);
}

void CTaskParachute::ProcessSkydivingPhysics(float fTimeStep)
{
	NOTFINAL_ONLY(parachuteDebugf1("Frame %d : Ped %p %s : %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", GetStaticStateName(GetState()), __FUNCTION__);)

	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Process the heading.
	float fHeadingChangeRate = sm_Tunables.m_ChangeRatesForSkydiving.m_Heading;
	float fHeadingChange = -m_fRoll * fHeadingChangeRate * fTimeStep;
	float fCurrentHeading = fwAngle::LimitRadianAngle(pPed->GetCurrentHeading() + pPed->GetMotionData()->GetExtraHeadingChangeThisFrame());
	float fDesiredHeading = fwAngle::LimitRadianAngle(fCurrentHeading + fHeadingChange);
	pPed->SetDesiredHeading(fDesiredHeading);
	
	//Process the pitch.
	float fMinPitch = sm_Tunables.m_PedAngleLimitsForSkydiving.m_MinPitch;
	float fMaxPitch = sm_Tunables.m_PedAngleLimitsForSkydiving.m_MaxPitch;
	float fDesiredPitch = fMaxPitch + m_fPitch * (fMinPitch - fMaxPitch);
	fDesiredPitch = rage::Clamp(fDesiredPitch, fMinPitch, fMaxPitch);
	float fCurrentPitch = pPed->GetCurrentPitch();
	float fPitchChange = fDesiredPitch - fCurrentPitch;
	float fPitchChangeRate = sm_Tunables.m_ChangeRatesForSkydiving.m_Pitch;
	fPitchChange = rage::Clamp(fPitchChange, -fPitchChangeRate * fTimeStep, fPitchChangeRate * fTimeStep);
	fDesiredPitch = fwAngle::LimitRadianAngle(fCurrentPitch + fPitchChange);
	pPed->SetDesiredPitch(fDesiredPitch);

	//Get the time since we left the vehicle.
	static dev_u32 s_uMaxTimeSinceLeftVehicleForNoDrag = 1500;
	static dev_u32 s_uMaxTimeSinceLeftVehicleForLerpedDrag = 3000;
	u32 uTimeSinceLeftVehicle = CTimeHelpers::GetTimeSince(GetPed()->GetPedIntelligence()->GetLastTimeLeftVehicle());

	//Calculate the drag mult.
	float fDragMult = 1.0f;
	if(uTimeSinceLeftVehicle < s_uMaxTimeSinceLeftVehicleForNoDrag)
	{
		fDragMult = 0.0f;
	}
	else if(uTimeSinceLeftVehicle < s_uMaxTimeSinceLeftVehicleForLerpedDrag)
	{
		fDragMult = (float)(uTimeSinceLeftVehicle - s_uMaxTimeSinceLeftVehicleForNoDrag) /
			(float)(s_uMaxTimeSinceLeftVehicleForLerpedDrag - s_uMaxTimeSinceLeftVehicleForNoDrag);
	}
	
	// Apply some lift to the ped
	ms_SkydivingFlightModelHelper.ProcessFlightModel(pPed,0.0f,0.0f,0.0f,0.0f,VEC3_ZERO,true,1.0f,fDragMult);

	//Grab the mass.
	float fMass = pPed->GetMass();

	//Calculate the thrust vector.
	Vector3 vThrust(0.0f, 1.0f, 0.0f);
	vThrust.RotateZ(pPed->GetTransform().GetHeading());

	//Apply the thrust multiplier.
	float fThrust = CalculateThrustFromPitchForSkydiving();
	vThrust *= fThrust;

	//Apply the thrust.
	ms_SkydivingFlightModelHelper.ApplyForceCgSafe(vThrust * fMass * -GRAVITY, pPed);

	//Calculate the lift vector.
	Vector3 vLift(0.0f, 0.0f, 1.0f);

	//Apply the lift multiplier.
	float fLift = CalculateLiftFromPitchForSkydiving();
	vLift *= fLift;

	//Apply the lift.
	ms_SkydivingFlightModelHelper.ApplyForceCgSafe(vLift * fMass * -GRAVITY, pPed);

	//Clamp the velocity.
	static dev_float s_fMaxVelocity = 100.0f;
	pPed->SetDesiredVelocityClamped(pPed->GetVelocity(),s_fMaxVelocity);

	pPed->SetClothIsSkydiving( true );
}

void CTaskParachute::ProcessStreaming()
{
	//Process the streaming for the move network.
	ProcessStreamingForMoveNetwork();

	//Process streaming for the model.
	ProcessStreamingForModel();

	//Process streaming for the clip sets.
	ProcessStreamingForClipSets();

	//Process streaming for the driveby sets.
	ProcessStreamingForDrivebyClipsets();
}

void CTaskParachute::ProcessStreamingForClipSets()
{
	//Always stream in the base animations.
	m_ClipSetRequestHelperForBase.Request(CLIP_SET_SKYDIVE_BASE);

	//Check if we can use the first person camera.
	if(CanUseFirstPersonCamera() && fwClipSetManager::GetClipSet(CLIP_SET_SKYDIVE_BASE_FIRST_PERSON))
	{
		m_ClipSetRequestHelperForFirstPersonBase.Request(CLIP_SET_SKYDIVE_BASE_FIRST_PERSON);
	}

	//Check if we should stream in the low lod animations.
	bool bStreamInLowLod = (m_bShouldUseLowLod || m_bIsUsingLowLod);
	if(bStreamInLowLod)
	{
		//Request the clip set.
		m_ClipSetRequestHelperForLowLod.Request(CLIP_SET_SKYDIVE_LOW_LOD);
	}
	else
	{
		//Release the clip set.
		m_ClipSetRequestHelperForLowLod.Release();
	}

	//Check if we should stream in the high lod animations.
	bool bStreamInHighLod = (!m_bShouldUseLowLod || !m_bIsUsingLowLod);
	if(bStreamInHighLod)
	{
		//Check if we should stream in the clip set for skydiving.
		if(ShouldStreamInClipSetForSkydiving())
		{
			//Request the clip set.
			m_ClipSetRequestHelperForSkydiving.Request(CLIP_SET_SKYDIVE_FREEFALL);

			//Check if we can use the first person camera.
			if(CanUseFirstPersonCamera() && fwClipSetManager::GetClipSet(CLIP_SET_SKYDIVE_FREEFALL_FIRST_PERSON))
			{
				m_ClipSetRequestHelperForFirstPersonSkydiving.Request(CLIP_SET_SKYDIVE_FREEFALL_FIRST_PERSON);
			}
		}
		else
		{
			//Release the clip set.
			m_ClipSetRequestHelperForSkydiving.Release();
			m_ClipSetRequestHelperForFirstPersonSkydiving.Release();
		}

		//Check if we should stream in the clip set for parachuting.
		if(ShouldStreamInClipSetForParachuting())
		{
			//Request the clip set.
			m_ClipSetRequestHelperForParachuting.Request(CLIP_SET_SKYDIVE_PARACHUTE);

			//Check if we can use the first person camera.
			if(CanUseFirstPersonCamera() && fwClipSetManager::GetClipSet(CLIP_SET_SKYDIVE_PARACHUTE_FIRST_PERSON))
			{
				m_ClipSetRequestHelperForFirstPersonParachuting.Request(CLIP_SET_SKYDIVE_PARACHUTE_FIRST_PERSON);
			}
		}
		else
		{
			//Release the clip set.
			m_ClipSetRequestHelperForParachuting.Release();
			m_ClipSetRequestHelperForFirstPersonParachuting.Release();
		}
	}
	else
	{
		//Release the clip sets.
		m_ClipSetRequestHelperForSkydiving.Release();
		m_ClipSetRequestHelperForFirstPersonSkydiving.Release();
		m_ClipSetRequestHelperForParachuting.Release();
		m_ClipSetRequestHelperForFirstPersonParachuting.Release();
	}
}

void CTaskParachute::ProcessStreamingForDrivebyClipsets()
{
	CPed *pPed = GetPed();

	//! Only do this for the local player for now as we need to transition between 1st and 3rd (and all weapon combinations) smoothly.
	//! Note: we don't require these are loaded in before progressing to aim state as it can pre-stream them there in any case.
	//! Also, we deliberately don't handle the case where we are given weapons after we have begun pre-streaming (as iterating over entire
	//! weapon array every frame seems unnecessary).
	if(pPed->IsLocalPlayer())
	{
		if(m_MultipleClipSetRequestHelper.GetNumClipSetRequests() == 0)
		{
			weaponAssert(pPed->GetInventory());

			// stream each clipset for all valid weapons ped has
			const CWeaponItemRepository& WeaponRepository = pPed->GetInventory()->GetWeaponRepository();
			for(int i = 0; i < WeaponRepository.GetItemCount(); i++)
			{
				const CWeaponItem* pWeaponItem = WeaponRepository.GetItemByIndex(i);
				if(pWeaponItem)
				{
					const CWeaponInfo* pWeaponInfo = pWeaponItem->GetInfo();
					const CVehicleDriveByAnimInfo* pDriveByClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash());
					if (pDriveByClipInfo)
					{
						// Don't pre-stream driveby clipsets in SP if weapon is flagged as MP driveby only
						bool bBlockClipSetRequest = (!NetworkInterface::IsGameInProgress() && pWeaponInfo && pWeaponInfo->GetIsDriveByMPOnly());
						if (!bBlockClipSetRequest)
						{
							if(!m_MultipleClipSetRequestHelper.HaveAddedClipSet(pDriveByClipInfo->GetClipSet()))
							{
								m_MultipleClipSetRequestHelper.AddClipSetRequest(pDriveByClipInfo->GetClipSet());
							}

							if(pDriveByClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()) != CLIP_SET_ID_INVALID && 
								!m_MultipleClipSetRequestHelper.HaveAddedClipSet(pDriveByClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash())))
							{
								m_MultipleClipSetRequestHelper.AddClipSetRequest(pDriveByClipInfo->GetFirstPersonClipSet(pWeaponInfo->GetHash()));
							}
						}
					}
				}
			}
		}

		if (m_MultipleClipSetRequestHelper.RequestAllClipSets())
		{
			m_bDriveByClipSetsLoaded = true;
		}
		else
		{
			m_bDriveByClipSetsLoaded = false;
		}
	}
}

void CTaskParachute::ProcessStreamingForModel()
{
	//Check if the parachute model index is invalid.
	if(m_iParachuteModelIndex == fwModelId::MI_INVALID)
	{
		fwModelId iModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(GetModelForParachute(GetPed()), &iModelId);
		m_iParachuteModelIndex = iModelId.GetModelIndex();
	}
	
	//Ensure the parachute model is valid.
	fwModelId iModelId((strLocalIndex(m_iParachuteModelIndex)));
	if(taskVerifyf(iModelId.IsValid(), "Parachute model is invalid."))
	{
		//Check if the parachute model request is not valid.
		if(!m_ModelRequestHelper.IsValid())
		{
			//Stream the parachute model.
			strLocalIndex transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId);
			m_ModelRequestHelper.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD);
		}
		
		//Ensure the parachute collision is valid.
		bool bHasPhysics = CModelInfo::GetBaseModelInfo(iModelId)->GetHasBoundInDrawable();

		if(!bHasPhysics)
		{
#if !__NO_OUTPUT
			CBaseModelInfo* modelInfo = CModelInfo::GetBaseModelInfo(iModelId);

			parachuteDebugf1("B* 1112823 - Please copy to bug - State = %d : StateFromNetwork = %d : iModelId U32 %d, Streaming Index %d, m_ParachuteModelIndex = %d : name %s = hash %d", 
			GetState(),
			GetStateFromNetwork(),
			iModelId.ConvertToU32(),
			iModelId.ConvertToStreamingIndex().Get(),
			m_iParachuteModelIndex, 
			modelInfo ? modelInfo->GetModelName() : "No model info", 
			modelInfo ? modelInfo->GetModelNameHash() : -1
			);
#endif
		}

	}
}

void CTaskParachute::ProcessStreamingForMoveNetwork()
{
	//Stream the move network.
	m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskParachute);
}

void CTaskParachute::RemovePhysicsFromParachute()
{
	//Ensure the parachute is valid.
	if(!taskVerifyf(m_pParachuteObject, "Parachute is invalid."))
	{
		return;
	}
	
	//Remove the physics.
	m_pParachuteObject->RemovePhysics();
}

void CTaskParachute::SetClipSetForState()
{
	//Assert that the move network is active.
	taskAssert(m_networkHelper.IsNetworkActive());

	//Set the clip set.
	fwMvClipSetId clipSetId = GetClipSetForState();

	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	m_networkHelper.SetClipSet(clipSetId);
}

void CTaskParachute::SetInactiveCollisions(bool bValue)
{
	//Ensure the value is changing.
	if(bValue == m_bInactiveCollisions)
	{
		return;
	}

	//Grab the physics instance.
	phInst* pInst = GetPed()->GetAnimatedInst();
	if(!pInst || !pInst->IsInLevel())
	{
		return;
	}

	//Set the flag.
	m_bInactiveCollisions = bValue;

	//Grab the level index.
	u16 uLevelIndex = pInst->GetLevelIndex();

	//Set inactive collisions.
	PHLEVEL->SetInactiveCollidesAgainstInactive(uLevelIndex, bValue);
	PHLEVEL->SetInactiveCollidesAgainstFixed(uLevelIndex, bValue);
}

void CTaskParachute::SetParachutePackVariation()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Set the parachute pack variation for the ped.
	SetParachutePackVariationForPed(*pPed);
}

void CTaskParachute::SetParachuteTintIndex()
{
	parachuteDebugf3("CTaskParachute::SetParachuteTintIndex GetTintIndexForParachute[%d]",GetTintIndexForParachute(*GetPed()));

	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		parachuteDebugf3("CTaskParachute::SetParachuteTintIndex--!m_pParachuteObject-->return");
		return;
	}

	//Ensure the custom shader effect is valid.
	fwCustomShaderEffect* pCustomShaderEffect = m_pParachuteObject->GetDrawHandler().GetShaderEffect();
	if(!pCustomShaderEffect)
	{
		parachuteDebugf3("CTaskParachute::SetParachuteTintIndex--!pCustomShaderEffect-->return");
		return;
	}

	//Ensure the custom shader effect is a prop.
	if(pCustomShaderEffect->GetType() != CSE_PROP)
	{
		parachuteDebugf3("CTaskParachute::SetParachuteTintIndex--pCustomShaderEffect->GetType() != CSE_PROP-->return");
		return;
	}
		//Set the tint on the object
		u8 tint = (u8)GetTintIndexForParachute(*GetPed());
		m_pParachuteObject->SetTintIndex(tint);

		//Set the tint index.
		CCustomShaderEffectProp* pCustomShaderEffectProp = static_cast<CCustomShaderEffectProp *>(pCustomShaderEffect);
		pCustomShaderEffectProp->SelectTintPalette(tint, m_pParachuteObject);	
}

void CTaskParachute::SetStateForSkydiving()
{
	//Set the state for skydiving.
	int iState = ChooseStateForSkydiving();
	TaskSetState(iState);
}

bool CTaskParachute::ShouldBlockWeaponSwitching() const
{
	//Check if we are parachuting, and can aim.
	if((GetState() == State_Parachuting) && CanAim())
	{
		return false;
	}

	return true;
}

bool CTaskParachute::ShouldDisableProcessProbes() const
{
	//This is done to prevent the spring strength of the ped capsule from interfering with
	//the crash landing logic during skydives.  If the player goes into a skydive and does
	//not fall very far before reaching the ground, there is a possibility that the capsule
	//spring strength will hold the player up before they hit the ground.  This was added
	//to work around this issue, since presumably we do not care about the ped capsule
	//during these states.

	//Check the state.
	if(GetState() <= State_Deploying)
	{
		return true;
	}

	return false;
}

bool CTaskParachute::ShouldEarlyOutForMovement() const
{
	//Ensure we can early out for movement.
	if(!m_bCanEarlyOutForMovement)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is a local player.
	if(!pPed->IsLocalPlayer())
	{
		return false;
	}
	
	//Ensure the control is valid.
	const CControl* pControl = pPed->GetControlFromPlayer();
	if(!pControl)
	{
		return false;
	}

	//Read the stick input.
	Vector2 vStick;
	vStick.x = pControl->GetPedWalkLeftRight().GetNorm();
	vStick.y = pControl->GetPedWalkUpDown().GetNorm();
	CFlightModelHelper::MakeCircularInputSquare(vStick);
	
	//Ensure the magnitude of the stick input exceeds the threshold.
	float fMagSq = vStick.Mag2();
	float fMinMagSq = square(sm_Tunables.m_Landing.m_MinStickMagnitudeForEarlyOutMovement);
	if(fMagSq < fMinMagSq)
	{
		return false;
	}
	
	return true;
}

bool CTaskParachute::ShouldIgnoreParachuteCollisionWithBuilding(Vec3V_In vCollisionNormal) const
{
	//Ensure the parachute is oriented near the default.
	Vec3V vParachuteUp = m_pParachuteObject->GetTransform().GetUp();
	ScalarV scThreshold = ScalarVFromF32(sm_Tunables.m_CrashLanding.m_ParachuteUpThreshold);
	if(IsLessThanAll(vParachuteUp.GetZ(), scThreshold))
	{
		return false;
	}

	//Rotate the collision normal into object space.
	Vec3V vCollisionNormalObject = m_pParachuteObject->GetTransform().UnTransform3x3(vCollisionNormal);

	//Allow some glancing blows.
	//Best way I can think of to accomplish this is by checking to see if the X-axis value on the collision normal
	//is dominant.  This will allow side collisions through, but not front/back or up/down collisions.

	Vec3V vAbsCollisionNormalObject = Abs(vCollisionNormalObject);
	if(IsLessThanAll(vAbsCollisionNormalObject.GetX(), vAbsCollisionNormalObject.GetY()))
	{
		return false;
	}

	if(IsLessThanAll(vAbsCollisionNormalObject.GetX(), vAbsCollisionNormalObject.GetZ()))
	{
		return false;
	}

	return true;
}

bool CTaskParachute::ShouldIgnoreParachuteCollisionWithEntity(const CEntity& rEntity, Vec3V_In vCollisionNormal) const
{
	//Check if the entity is a ped.
	if(rEntity.GetIsTypePed())
	{
		//Check if we should ignore the collision.
		if(ShouldIgnoreParachuteCollisionWithPed(static_cast<const CPed &>(rEntity)))
		{
			return true;
		}
	}
	//Check if the entity is a vehicle.
	else if(rEntity.GetIsTypeVehicle())
	{
		//Check if we should ignore the collision.
		if(ShouldIgnoreParachuteCollisionWithVehicle(static_cast<const CVehicle &>(rEntity)))
		{
			return true;
		}
	}
	//Check if the entity is an object.
	else if(rEntity.GetIsTypeObject())
	{
		//Check if we should ignore the collision.
		if(ShouldIgnoreParachuteCollisionWithObject(static_cast<const CObject &>(rEntity)))
		{
			return true;
		}
	}
	//Check if the entity is a building.
	else if(rEntity.GetIsTypeBuilding())
	{
		//Check if we should ignore the collision.
		if(ShouldIgnoreParachuteCollisionWithBuilding(vCollisionNormal))
		{
			return true;
		}
	}

	return false;
}

bool CTaskParachute::ShouldIgnoreParachuteCollisionWithObject(const CObject& rObject) const
{
	if(IsParachute(rObject) || rObject.IsPickup())
	{
		return true;
	}

	return false;
}

bool CTaskParachute::ShouldIgnoreParachuteCollisionWithPed(const CPed& UNUSED_PARAM(rPed)) const
{
	return true;
}

bool CTaskParachute::ShouldIgnoreParachuteCollisionWithVehicle(const CVehicle& rVehicle) const
{
	static dev_u32 s_IgnoreMyVehicleTimeParachute = 5000;
	u32 uTimeSinceLeftVehicle = CTimeHelpers::GetTimeSince(GetPed()->GetPedIntelligence()->GetLastTimeLeftVehicle());
	if(uTimeSinceLeftVehicle < s_IgnoreMyVehicleTimeParachute)
	{
		CVehicle* myVehicle				= GetPed()->GetMyVehicle();
		CVehicle* myVehicleAttachParent = NULL;
		if(myVehicle && myVehicle->GetAttachParent() && myVehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
		{
			myVehicleAttachParent = static_cast<CVehicle*>(myVehicle->GetAttachParent());
		}

		return ( (&rVehicle == myVehicle) || (&rVehicle == myVehicleAttachParent) );
	}

	return false;
}

bool CTaskParachute::ShouldIgnorePedCollisionWithEntity(const CEntity& rEntity, Vec3V_In UNUSED_PARAM(vCollisionNormal)) const
{
	//Check if the entity is a ped.
	if(rEntity.GetIsTypePed())
	{
		//Check if we should ignore the collision.
		if(ShouldIgnorePedCollisionWithPed(static_cast<const CPed &>(rEntity)))
		{
			return true;
		}
	}
	//Check if the entity is a vehicle.
	else if(rEntity.GetIsTypeVehicle())
	{
		//Check if we should ignore the collision.
		if(ShouldIgnorePedCollisionWithVehicle(static_cast<const CVehicle &>(rEntity)))
		{
			return true;
		}
	}
	//Check if the entity is an object.
	else if(rEntity.GetIsTypeObject())
	{
		//Check if we should ignore the collision.
		if(ShouldIgnorePedCollisionWithObject(static_cast<const CObject &>(rEntity)))
		{
			return true;
		}
	}

	return false;
}

bool CTaskParachute::ShouldIgnorePedCollisionWithObject(const CObject& rObject) const
{
	if(IsParachute(rObject) || rObject.IsPickup())
	{
		return true;
	}

	return false;
}

bool CTaskParachute::ShouldIgnorePedCollisionWithPed(const CPed& UNUSED_PARAM(rPed)) const
{
	return true;
}

bool CTaskParachute::ShouldIgnorePedCollisionWithVehicle(const CVehicle& rVehicle) const
{
	static dev_u32 s_IgnoreMyVehicleTimePed = 5000;
	u32 uTimeSinceLeftVehicle = CTimeHelpers::GetTimeSince(GetPed()->GetPedIntelligence()->GetLastTimeLeftVehicle());
	if(uTimeSinceLeftVehicle < s_IgnoreMyVehicleTimePed)
	{
		CVehicle* myVehicle				= GetPed()->GetMyVehicle();
		CVehicle* myVehicleAttachParent = NULL;
		if(myVehicle && myVehicle->GetAttachParent() && myVehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
		{
			myVehicleAttachParent = static_cast<CVehicle*>(myVehicle->GetAttachParent());
		}

		return ( (&rVehicle == myVehicle) || (&rVehicle == myVehicleAttachParent) );
	}

	return false;
}

bool CTaskParachute::ShouldIgnoreProbeCollisionWithEntity(const CEntity& rEntity, Vec3V_In vCollisionPosition, float fTimeStep) const
{
	//Ensure the entity is a physical.
	if(!rEntity.GetIsPhysical())
	{
		return false;
	}

	//Grab the velocity.
	Vec3V vVelocity = VECTOR3_TO_VEC3V(static_cast<const CPhysical &>(rEntity).GetVelocity());

	//Ensure the speed is valid.
	ScalarV scSpeedSq = MagSquared(vVelocity);
	static dev_float s_fMaxSpeed = 1.0f;
	ScalarV scMaxSpeedSq = ScalarVFromF32(square(s_fMaxSpeed));
	if(IsLessThanAll(scSpeedSq, scMaxSpeedSq))
	{
		return false;
	}

	//Grab the matrix.
	Mat34V mEntity = rEntity.GetMatrix();

	//Translate the matrix based on the velocity and timestep.
	Mat34V mTranslatedEntity(mEntity);
	Vec3V vTranslate = Scale(vVelocity, ScalarVFromF32(fTimeStep));
	Translate(mTranslatedEntity, mEntity, vTranslate);

	//Transform the collision position into local coordinates.
	Vec3V vLocalCollisionPosition = UnTransformFull(mTranslatedEntity, vCollisionPosition);

	//Ensure the bounding box does not contain the position.
	if(rEntity.GetBaseModelInfo()->GetBoundingBox().ContainsPoint(vLocalCollisionPosition))
	{
		return false;
	}

	return true;
}

bool CTaskParachute::ShouldProcessDeployingPhysics() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Deploying:
		{
			//Check if the parachute is not out.
			if(!IsParachuteOut())
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskParachute::ShouldProcessParachuteObject() const
{
	const CPed* pPed = GetPed();

	if (!pPed)
		return false;

	//If the remote doesn't have a chute, always be prepared to create one
	if (pPed->IsNetworkClone())
	{
		if (m_pParachuteObject && !m_bCloneParachuteConfigured)
			return true;
	}

	//Check if the state is valid.
	if(GetState() < State_Deploying)
	{
		return true;
	}

	return false;
}

bool CTaskParachute::ShouldProcessParachutingPhysics() const
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return false;
	}

	//Ensure the parachute object has a physics instance.
	if(!m_pParachuteObject->GetCurrentPhysicsInst())
	{
		return false;
	}

	//Ensure the parachute object is not attached.
	if(m_pParachuteObject->GetIsAttached())
	{
		return false;
	}
	
	//Check the state.
	switch(GetState())
	{
		case State_Deploying:
		{
			//Check if the parachute is out.
			if(IsParachuteOut())
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		case State_Parachuting:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskParachute::ShouldProcessSkydivingPhysics() const
{
	// we need to keep the clone as close as possible in sync with the owner, so even if we're not ready (i.e State < State_Skydiving) on the clone, we apply skydiving physics.
	if((GetState() == State_Skydiving) || (GetStateFromNetwork() == State_Skydiving))
	{
		return true;
	}

	return false;
}

bool CTaskParachute::ShouldStreamInClipSetForParachuting() const
{
	return true;
}

bool CTaskParachute::ShouldStreamInClipSetForSkydiving() const
{
	//Ensure we have not finished skydiving.
	if(GetState() > State_Skydiving)
	{
		return false;
	}

	return true;
}

bool CTaskParachute::ShouldUpdateParachuteBones() const
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return false;
	}
	
	//Ensure the state is valid.
	switch(GetState())
	{
		case State_BlendFromNM:
		{
			if(m_bCloneRagdollTaskRelaunch)
			{	
				break;	
			}
		}
		case State_Deploying:
		case State_Parachuting:
		{
			break;
		}
		default:
		{
			return false;
		}
	}
	
	// we're deploying / parachuting / blending from NM 
	if(GetPed() && GetPed()->IsNetworkClone())
	{
		// but we're not actually attached to the parachute (probably through a clone side only collision)
		if(GetPed()->GetAttachParent() != m_pParachuteObject)
		{
			// don't bother with the straps...
			return false;
		}
	}

	return true;
}

bool CTaskParachute::ShouldUseAiControls() const
{
	//Ensure the target is valid.
	if(!m_iFlags.IsFlagSet(PF_HasTarget))
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Check if the ped is not a player.
	if(!pPed->IsPlayer())
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskParachute::ShouldUseCloseUpCamera() const
{
	//Ensure the left brake value has exceeded the thresold.
	if(m_fRawLeftBrake < sm_Tunables.m_MinBrakeForCloseUpCamera)
	{
		return false;
	}
	
	//Ensure the right brake value has exceeded the thresold.
	if(m_fRawRightBrake < sm_Tunables.m_MinBrakeForCloseUpCamera)
	{
		return false;
	}
	
	return true;
}

bool CTaskParachute::ShouldUseLeftGripIk() const
{
	return true;
}

bool CTaskParachute::ShouldUseLowLod() const
{
	//Check if the flag is set.
	if(sm_Tunables.m_LowLod.m_AlwaysUse)
	{
		return true;
	}
	else if(sm_Tunables.m_LowLod.m_NeverUse)
	{
		return false;
	}

	//Ensure we are in MP.
	if(!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure we are not the local player.
	if(pPed->IsLocalPlayer())
	{
		return false;
	}

	// Grab the player pos.
	Vec3V vPedPos = pPed->GetTransform().GetPosition();

	//Grab the camera position - need to use camera position and not player pos as we have spectator mode....
	Vec3V vCameraPosition = VECTOR3_TO_VEC3V(camInterface::GetPos());

	//Ensure the local player is outside the distance threshold.
	ScalarV scMinDistSq = ScalarVFromF32(square(sm_Tunables.m_LowLod.m_MinDistance));
	ScalarV scDistSq = DistSquared(vPedPos, vCameraPosition);
		
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		// using low lod but I'm about to switch to high lod
		if(m_bIsUsingLowLod)
		{
			parachuteDebugf1("%s : ShouldSwitchToHighLod Ped %p %s : pedPos %f %f %f : camPos %f %f %f : scDistSq %f : scMinDistSq %f", __FUNCTION__, GetEntity(), BANK_ONLY(GetEntity() ? GetPed()->GetDebugName() :) "NO PED", VEC3V_ARGS(vPedPos), VEC3V_ARGS(vCameraPosition), scDistSq.Getf(), scMinDistSq.Getf());
		}

		return false;
	}

	// using high lod but I'm about to switch to low lod
	if(!m_bIsUsingLowLod)
	{
		parachuteDebugf1("%s : ShouldSwitchToLowLod  Ped %p %s : pedPos %f %f %f : camPos %f %f %f : scDistSq %f : scMinDistSq %f", __FUNCTION__, GetEntity(), BANK_ONLY(GetEntity() ? GetPed()->GetDebugName() :) "NO PED", VEC3V_ARGS(vPedPos), VEC3V_ARGS(vCameraPosition), scDistSq.Getf(), scMinDistSq.Getf());
	}

	return true;
}

bool CTaskParachute::ShouldUsePlayerControls() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is a local player.
	if(!pPed->IsLocalPlayer())
	{
		return false;
	}
	
	return true;
}

bool CTaskParachute::ShouldUseRightGripIk() const
{
	bool bUsingPhone = false;
	CTask *pPhoneTask = GetPed()->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
	if(pPhoneTask && pPhoneTask->GetState() != CTaskMobilePhone::State_Paused)
	{
		bUsingPhone = true;
	}

	//Ensure we are not aiming.
	if(IsAiming() || bUsingPhone)
	{
		return false;
	}

	return true;
}

void CTaskParachute::SmoothValue(float& fCurrentValue, float fDesiredValue, float fMaxChange) const
{
	//Calculate the delta.
	float fDelta = fDesiredValue - fCurrentValue;
	
	//Clamp the delta.
	fDelta = Clamp(fDelta, -fMaxChange, fMaxChange);
	
	//Update the current value.
	fCurrentValue += fDelta;
}

void CTaskParachute::StartFirstPersonCameraShake(atHashWithStringNotFinal hName)
{
#if FPS_MODE_SUPPORTED
	//Ensure this is the local player.
	if(!GetPed()->IsLocalPlayer())
	{
		return;
	}

	//Ensure first person is enabled.
	if(!GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return;
	}

	//Ensure the camera is not shaking.
	camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if((pCamera == NULL) || pCamera->IsShaking())
	{
		return;
	}

	//Start the shake.
	pCamera->Shake(hName.GetHash());
#else
	(void)hName;
#endif
}

void CTaskParachute::StartFirstPersonCameraShakeForDeploy()
{
	static atHashWithStringNotFinal s_hShake("FIRST_PERSON_DEPLOY_PARACHUTE_SHAKE");
	StartFirstPersonCameraShake(s_hShake);
}

void CTaskParachute::StartFirstPersonCameraShakeForLanding()
{
	static atHashWithStringNotFinal s_hShake("SMALL_EXPLOSION_SHAKE");
	StartFirstPersonCameraShake(s_hShake);
}

void CTaskParachute::StartFirstPersonCameraShakeForParachuting()
{
	static atHashWithStringNotFinal s_hShake("PARACHUTING_SHAKE");
	StartFirstPersonCameraShake(s_hShake);
}

void CTaskParachute::StartFirstPersonCameraShakeForSkydiving()
{
	static atHashWithStringNotFinal s_hShake("SKY_DIVING_SHAKE");
	StartFirstPersonCameraShake(s_hShake);
}

void CTaskParachute::StopFirstPersonCameraShaking(bool bStopImmediately)
{
#if FPS_MODE_SUPPORTED
	//Ensure this is the local player.
	if(!GetPed()->IsLocalPlayer())
	{
		return;
	}

	//Ensure first person is enabled.
	if(!GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return;
	}

	//Ensure first person camera is valid.
	camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	if (pCamera == NULL)
	{
		return;
	}

	//Ensure the camera is shaking.
	if(!pCamera->IsShaking())
	{
		return;
	}

	//Stop the shake.
	pCamera->StopShaking(bStopImmediately);
#else
	(void)bStopImmediately;
#endif
}

void CTaskParachute::UseLowLod(bool bUseLowLod)
{
	//Assert that we can use low lod.
	taskAssert(CanUseLowLod(bUseLowLod));

	//Assert that we are not using low lod.
	taskAssert(m_bIsUsingLowLod != bUseLowLod);

	//Set the flag.
	m_bIsUsingLowLod = bUseLowLod;

	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);

	//Set the move flag.
	m_networkHelper.SetFlag(m_bIsUsingLowLod, ms_UseLowLod);

	//Set the clip set for the state.
	SetClipSetForState();
}

#if !__FINAL

void CTaskParachute::ValidateNetworkBlender(bool const shouldBeOverridden) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is a network clone.
	if(!pPed || !pPed->IsNetworkClone())
	{
		return;
	}

	// Ped should fall naturally through the air in some states or be position synced in others...
	u32 resultCode = 0;
	if(!(pPed->GetNetworkObject() && (pPed->GetNetworkObject()->NetworkBlenderIsOverridden(&resultCode) == shouldBeOverridden)))
	{
//		Warningf("ERROR : %s : Ped Network Blender Is Incorrectly Overridden?! : State %s : should be overridden %d : resultCode %d", __FUNCTION__, GetStaticStateName(GetState()), shouldBeOverridden, resultCode);
	}

	// check the parachute object blender isn't overidden...
	if(m_pParachuteObject)
	{
		if(!(m_pParachuteObject->GetNetworkObject() && !m_pParachuteObject->GetNetworkObject()->NetworkBlenderIsOverridden(&resultCode)))
		{
//			Warningf("ERROR : %s : Parachute Object Network Blender Is Incorrectly Overridden?! : State %s : should be overridden %d : resultCode %d", __FUNCTION__, GetStaticStateName(GetState()), shouldBeOverridden, resultCode);
		}
	}
}
#endif /* !__FINAL */

void CTaskParachute::UpdateParachuteBones()
{
	TUNE_GROUP_FLOAT(TASK_PARACHUTE, fHandGripLerpRate, 0.25f, 0.0f, 1.0f, 0.01f);
	
	//Check if we should use left grip ik.
	m_fLeftHandGripWeight = rage::Lerp(fHandGripLerpRate, m_fLeftHandGripWeight, ShouldUseLeftGripIk() ? 1.0f : 0.0f);
	if(m_fLeftHandGripWeight < 0.01f)
	{
		m_fLeftHandGripWeight = 0.0f;
	}
	else
	{
		UpdateParachuteBone(P_Para_S_L_Grip, BONETAG_L_PH_HAND,	sm_Tunables.m_ParachuteBones.m_LeftGrip, m_fLeftHandGripWeight, true);
	}

	m_fRightHandGripWeight = rage::Lerp(fHandGripLerpRate, m_fRightHandGripWeight, ShouldUseRightGripIk() ? 1.0f : 0.0f);
	if(m_fRightHandGripWeight < 0.01f)
	{
		m_fRightHandGripWeight = 0.0f;
	}
	else
	{
		UpdateParachuteBone(P_Para_S_R_Grip, BONETAG_R_PH_HAND,	sm_Tunables.m_ParachuteBones.m_RightGrip, m_fRightHandGripWeight, true);
	}

	//Connect the straps.
	UpdateParachuteBone(P_Para_S_LF_pack2,	BONETAG_L_CLAVICLE, sm_Tunables.m_ParachuteBones.m_LeftWire, 1.0f);
	UpdateParachuteBone(P_Para_S_RF_pack2,	BONETAG_R_CLAVICLE, sm_Tunables.m_ParachuteBones.m_RightWire, 1.0f);
}

void CTaskParachute::UpdateParachuteBone(eParachuteBoneId nParachuteBoneId, eAnimBoneTag nPedBoneId, const Tunables::ParachuteBones::Attachment& rAttachment, float fWeight, bool bSlerpOrientation)
{
	//Convert the parachute bone ID to an index.
	s32 iParachuteBoneIndex = -1;
	if(!m_pParachuteObject->GetSkeletonData().ConvertBoneIdToIndex((u16)nParachuteBoneId, iParachuteBoneIndex))
	{
		return;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Grab the ped bone matrix.
	Mat34V mBone;
	if(!pPed->GetBoneMatrix(RC_MATRIX34(mBone), nPedBoneId))
	{
		return;
	}
	
	//Transform the offset into world space, and add it to the bone.
	Vec3V vWorldOffset = Transform3x3(mBone, Vec3V(rAttachment.m_X, rAttachment.m_Y, rAttachment.m_Z));
	Translate(mBone, mBone, vWorldOffset);

	Mat34V mParaBone;
	m_pParachuteObject->GetGlobalMtx(iParachuteBoneIndex, RC_MATRIX34(mParaBone));

	Vec3V vPedBonePosition = mBone.GetCol3();

	//Check if we should use the orientation from the parachute bone.
	if(rAttachment.m_UseOrientationFromParachuteBone)
	{
		mBone = mParaBone;
		mBone.SetCol3(vPedBonePosition);
	}
	if(bSlerpOrientation)
	{
		Quaternion pedQuat;
		MAT34V_TO_MATRIX34(mBone).ToQuaternion(pedQuat);
		Quaternion paraQuat;
		MAT34V_TO_MATRIX34(mParaBone).ToQuaternion(paraQuat);

		paraQuat.Slerp(fWeight, pedQuat);
		Matrix34 mTemp;
		mTemp.FromQuaternion(paraQuat);
		mBone = MATRIX34_TO_MAT34V(mTemp);
		mBone.SetCol3(vPedBonePosition);
	}

	Vec3V vWeightedPosition = rage::Lerp(ScalarVFromF32(fWeight), mParaBone.GetCol3(), mBone.GetCol3());
	mBone.SetCol3(vWeightedPosition);

	//Sync the parachute bone matrix.
	m_pParachuteObject->SetGlobalMtx(iParachuteBoneIndex, MAT34V_TO_MATRIX34(mBone));
}

bool CTaskParachute::ScanForCrashLanding()
{
	//Only scan for crash in SP or on local - remote will get information that it is in crash land state from the authority through the network (lavalley)
	if (NetworkInterface::IsGameInProgress())
	{
		if (!GetPed() || GetPed()->IsNetworkClone())
			return false;
	}

	//Check if the ped is in water.
	if(!IsPedInWater())
	{
		//Check if the parachute is out.
		if(IsParachuteOut())
		{
			//Scan for a crash landing with a parachute.
			if(!ScanForCrashLandingWithParachute())
			{
				return false;
			}
			
			//Set the parachute landing flags.
			m_LandingData.m_uParachuteLandingFlags = PLF_ReduceMass;
			
			//Note that a crash landing has been detected with a parachute.
			m_LandingData.m_CrashData.m_bWithParachute = true;
		}
		else
		{
			//Scan for a crash landing with no parachute.
			if(!ScanForCrashLandingWithNoParachute())
			{
				return false;
			}
			
			//Set the flags.
			m_LandingData.m_CrashData.m_bWithParachute = false;
			m_LandingData.m_CrashData.m_bPedCollision = true;
		}
		
		//Note that a crash landing has been detected.
		m_LandingData.m_nType = !m_LandingData.m_CrashData.m_bInWater ? LandingData::Crash : LandingData::Water;
	}
	else
	{
		//Note that a crash landing has been detected.
		m_LandingData.m_nType = LandingData::Water;
	}
	
	return true;
}

bool CTaskParachute::ScanForCrashLandingWithNoParachute()
{
	//Check if we have already crash landed.
	if((m_LandingData.m_nType == LandingData::Crash) && !m_LandingData.m_CrashData.m_bWithParachute)
	{
		return true;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	// Send off an asynch probe every few frames to set the ground velocity (so that the ped can damp relative to it, among other things)
	if (m_FallingGroundProbeResults.GetResultsReady() && pPed->GetCollider())
	{
		WorldProbe::ResultIterator it;
		for(it = m_FallingGroundProbeResults.begin(); it < m_FallingGroundProbeResults.end(); ++it)
		{
			if(it->GetHitDetected())
			{
				CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());
				if(pHitEntity && pHitEntity->IsCollisionEnabled())
				{
					if(it->GetHitNormal().Dot(ZAXIS) >= 0.707f)
					{
						//Check if the position is in the water.
						m_LandingData.m_CrashData.m_bInWater = IsPositionInWater(it->GetHitPositionV());

						return true;
					}
				}
			}
		}
		m_FallingGroundProbeResults.Reset();
	}
	else if (!m_FallingGroundProbeResults.GetWaitingOnResults() && (fwTimer::GetFrameCount() + (size_t)this) % NUM_FRAMES_PER_FALLING_PROBE == 0)
	{
		Assert(pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetCacheEntry() && pPed->GetRagdollInst()->GetCacheEntry()->GetBound());
		phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 pelvisMat;
		pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 

		// Send off a probe
		WorldProbe::CShapeTestProbeDesc losDesc;
		losDesc.SetResultsStructure(&m_FallingGroundProbeResults);
		Vector3 pedVel = pPed->GetVelocity();
		float depth = Max(8.0f, Abs(pedVel.z) * 0.33f);
		pedVel.NormalizeSafe();
		Vector3 vecStart = pelvisMat.d;
		Vector3 vecEnd = vecStart + pedVel * depth;
		losDesc.SetStartAndEnd(vecStart, vecEnd);
		u32 nIncludeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES;
		losDesc.SetIncludeFlags(nIncludeFlags);
		losDesc.SetExcludeInstance(pPed->GetCurrentPhysicsInst());
		WorldProbe::GetShapeTestManager()->SubmitTest(losDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}

	//Ensure the collision record is valid.
	const CCollisionRecord* pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
	if(!pColRecord)
	{
		return false;
	}

	//Check if we should ignore collision with the entity.
	const CEntity* pEntity = pColRecord->m_pRegdCollisionEntity;
	if(pEntity) 
	{
		if (pEntity->GetIsTypePed())
		{
			//Check if we should ignore the collision.
			if(static_cast<const CPed*>(pEntity)->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
			{
				return false;
			}
		}

		if (pEntity->GetIsTypeObject())
		{
			//Ensure the object is a parachute.
			if(IsParachute(*static_cast<const CObject*>(pEntity)))
			{
				return false;
			}
		}
	}

	//Calculate the collision normal threshold.
	float fTimeForMinCollisionNormalThreshold = sm_Tunables.m_CrashLanding.m_NoParachuteTimeForMinCollisionNormalThreshold;
	float fLerp = Clamp(GetTimeRunning(), 0.0f, fTimeForMinCollisionNormalThreshold);
	fLerp /= fTimeForMinCollisionNormalThreshold;
	float fCollisionNormalThreshold = Lerp(fLerp, sm_Tunables.m_CrashLanding.m_NoParachuteMaxCollisionNormalThreshold,
		sm_Tunables.m_CrashLanding.m_NoParachuteMinCollisionNormalThreshold);

	//Ensure the collision normal exceeds the threshold.
	Vector3 vCollisionNormal = pColRecord->m_MyCollisionNormal;
	if(vCollisionNormal.z < fCollisionNormalThreshold)
	{
		return false;
	}

	//Ensure the pitch exceeds the threshold.
	if(pPed->GetBoundPitch() > sm_Tunables.m_CrashLanding.m_NoParachuteMaxPitch)
	{
		return false;
	}

	//Check if the position is in the water.
	m_LandingData.m_CrashData.m_bInWater = IsPositionInWater(VECTOR3_TO_VEC3V(pColRecord->m_MyCollisionPos));
	
	//Note that a crash landing occurred.
	return true;
}

bool CTaskParachute::ScanForCrashLandingWithParachute()
{
	//Check if we have already crash landed.
	if((m_LandingData.m_nType == LandingData::Crash) && m_LandingData.m_CrashData.m_bWithParachute)
	{
		return true;
	}

	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the ped is still attached to the parachute.
	if(!IsPedAttachedToParachute())
	{
		//Note that a crash landing occurred.
		return true;
	}

	//Ensure the parachute is not in water.
	if(IsParachuteInWater())
	{
		//Note that a crash landing occurred.
		return true;
	}

	//Check if the parachute has an invalid intersection.
	if(m_uProbeFlags.IsFlagSet(PrF_HasInvalidIntersection))
	{
		//Note that a crash landing occurred.
		return true;
	}
	
	//Check if the parachute is valid.
	if(m_pParachuteObject)
	{
		//Check if the parachute collided with anything.
		const CCollisionRecord* pColRecord = m_pParachuteObject->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
		if(pColRecord)
		{
			//Keep track of whether to allow the collision.
			bool bAllowCollision = true;
			
			//Grab the collision normal.
			Vector3 vCollisionNormal = pColRecord->m_MyCollisionNormal;

			//Check if we should ignore the parachute collision with the entity.
			const CEntity* pEntity = pColRecord->m_pRegdCollisionEntity;
			if(pEntity && ShouldIgnoreParachuteCollisionWithEntity(*pEntity, VECTOR3_TO_VEC3V(vCollisionNormal)))
			{
				bAllowCollision = false;
			}
			
			//Check if the collision has been allowed.
			if(bAllowCollision)
			{
				//Set the collision normal.
				m_LandingData.m_CrashData.m_vCollisionNormal = VECTOR3_TO_VEC3V(vCollisionNormal);
				
				//Note that a crash landing occurred.
				return true;
			}
		}
	}
	
	//At this point we know the parachute has not collided with anything.
	//We do need to check if the ped is colliding with something, though.
	//The best way I can figure out to do this (without hacking physics/attachments, at least)
	//is using a capsule probe from the feet->head.
	
	//Generate the feet and head positions.
	Vec3V vFeet = GenerateFeetPosition();
	Vec3V vHead = GenerateHeadPosition();

	//Generate the future positions of the feet and head.
	//Doing this reduces the chances of intersecting with the geometry, when the ped is moving quickly.
	float fFramesToLookAhead = sm_Tunables.m_CrashLanding.m_FramesToLookAheadForProbe;
	float fTimeStep = GetTimeStep() * fFramesToLookAhead;
	Vec3V vFeetFuture = AdjustPositionBasedOnParachuteVelocity(vFeet, fTimeStep);
	Vec3V vHeadFuture = AdjustPositionBasedOnParachuteVelocity(vHead, fTimeStep);
	
	//Generate the radius.
	float fRadius = sm_Tunables.m_CrashLanding.m_ParachuteProbeRadius;
	
	//Set up a capsule probe.
	WorldProbe::CShapeTestCapsuleDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetCapsule(VEC3V_TO_VECTOR3(vFeetFuture), VEC3V_TO_VECTOR3(vHeadFuture), fRadius);
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_PARACHUTING_PED_INCLUDE_TYPES);
	
	//Generate the exclusion list.
	//This includes the ped and the parachute.
	const CEntity* pExclusionList[2];
	u8 uExclusionListCount = 0;
	pExclusionList[uExclusionListCount++] = pPed;
	if(m_pParachuteObject)
	{
		pExclusionList[uExclusionListCount++] = m_pParachuteObject;
	}
	probeDesc.SetExcludeEntities(pExclusionList, uExclusionListCount);
	
#if __BANK
	//Check if we are visualizing the crash landing scan.
	if(ms_bVisualiseCrashLandingScan)
	{
		//Draw the capsule probe.
		grcDebugDraw::Capsule(vFeetFuture, vHeadFuture, fRadius, Color_red, true, -1);
	}
#endif
	
	//Check if the probe got a hit.
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		//Iterate over the results.
		for(int i = 0; i < probeResult.GetSize(); ++i)
		{
			//Ensure a hit was detected.
			if(!probeResult[i].GetHitDetected())
			{
				continue;
			}

			//Grab the collision normal.
			Vector3 vCollisionNormal = probeResult[i].GetHitNormal();

			//Ensure we should not ignore the collision with the entity.
			const CEntity* pEntity = probeResult[i].GetHitEntity();
			if(pEntity && ShouldIgnorePedCollisionWithEntity(*pEntity, VECTOR3_TO_VEC3V(vCollisionNormal)))
			{
				continue;
			}

			//Ensure we should not ignore the probe collision with the entity.
			if(pEntity && ShouldIgnoreProbeCollisionWithEntity(*pEntity, VECTOR3_TO_VEC3V(probeResult[i].GetHitPosition()), fTimeStep))
			{
				continue;
			}

			//Set the collision normal.
			m_LandingData.m_CrashData.m_vCollisionNormal = VECTOR3_TO_VEC3V(vCollisionNormal);

			//Set the flags.
			m_LandingData.m_CrashData.m_bPedCollision = true;

			return true;
		}
	}
	
	return false;
}

bool CTaskParachute::ScanForLanding()
{
	//Scan for a landing with a parachute.
	if(!ScanForLandingWithParachute())
	{
		return false;
	}
	
	//Set the parachute landing flags.
	m_LandingData.m_uParachuteLandingFlags = PLF_Crumple|PLF_ClearHorizontalVelocity;
	
	//Choose the landing to use.
	m_LandingData.m_nType = ChooseLandingToUse();
	
	return true;
}

bool CTaskParachute::ScanForLandingWithParachute()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Calculate the forward threshold.
	float fBrake = m_fBrakingMult / ms_fMaxBraking;
	taskAssert(fBrake >= 0.0f && fBrake <= 1.0f);
	float fForwardThreshold = Lerp(fBrake,
		sm_Tunables.m_Landing.m_NormalThresholds.m_Normal.m_Forward,
		sm_Tunables.m_Landing.m_NormalThresholds.m_Braking.m_Forward);

	//Calculate the collision threshold.
	float fCollisionThreshold = Lerp(fBrake,
		sm_Tunables.m_Landing.m_NormalThresholds.m_Normal.m_Collision,
		sm_Tunables.m_Landing.m_NormalThresholds.m_Braking.m_Collision);

	//The game plan here is to generate a capsule probe from the ped's current feet position (this frame)
	//to the projected position of the ped's feet (next frame).  This gives us a bit of leeway for landing,
	//and also allows us to decrease the chances of intersecting with geometry if the ped is moving quickly.
	//If the capsule hits, we also check the normal of the geometry to ensure it is safe to land on.
	
	//Generate the feet position.
	Vec3V vFeet = GenerateFeetPosition();

	//Generate the future feet position.
	float fFramesToLookAhead = sm_Tunables.m_Landing.m_FramesToLookAheadForProbe;
	float fTimeStep = GetTimeStep() * fFramesToLookAhead;
	Vec3V vFeetFuture = AdjustPositionBasedOnParachuteVelocity(vFeet, fTimeStep);
	
	//Generate the radius.
	float fRadius = sm_Tunables.m_Landing.m_ParachuteProbeRadius;

	//Set up a capsule probe.
	WorldProbe::CShapeTestCapsuleDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetCapsule(VEC3V_TO_VECTOR3(vFeet), VEC3V_TO_VECTOR3(vFeetFuture), fRadius);
	probeDesc.SetIsDirected(true);
	probeDesc.SetDoInitialSphereCheck(true);
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_PARACHUTING_PED_INCLUDE_TYPES);

	//Generate the exclusion list.
	const CEntity* pExclusionList[2];
	u8 uExclusionListCount = 0;
	pExclusionList[uExclusionListCount++] = pPed;
	if(m_pParachuteObject)
	{
		pExclusionList[uExclusionListCount++] = m_pParachuteObject;
	}
	probeDesc.SetExcludeEntities(pExclusionList, uExclusionListCount);

#if __BANK
	//Check if we are visualizing the landing scan.
	if(ms_bVisualiseLandingScan)
	{
		//Draw the capsule probe.
		grcDebugDraw::Capsule(vFeet, vFeetFuture, fRadius, Color_green, true, -1);
	}
#endif

	//Check if the probe got a hit.
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		//Iterate over the results.
		for(int i = 0; i < probeResult.GetSize(); ++i)
		{
			//Ensure a hit was detected.
			if(!probeResult[i].GetHitDetected())
			{
				continue;
			}

			//Grab the collision normal.
			Vector3 vCollisionNormal = probeResult[i].GetHitNormal();

			//Ensure we should not ignore the collision with the entity.
			const CEntity* pEntity = probeResult[i].GetHitEntity();
			if(pEntity && ShouldIgnorePedCollisionWithEntity(*pEntity, VECTOR3_TO_VEC3V(vCollisionNormal)))
			{
				continue;
			}

			//Ensure we should not ignore the probe collision with the entity.
			if(pEntity && ShouldIgnoreProbeCollisionWithEntity(*pEntity, VECTOR3_TO_VEC3V(probeResult[i].GetHitPosition()), fTimeStep))
			{
				continue;
			}

			//Adjust the forward threshold.
			float fForwardThresholdAdjusted = fForwardThreshold;
			if(pEntity && pEntity->GetIsTypeVehicle() && CTheScripts::GetPlayerIsOnAMission())
			{
				fForwardThresholdAdjusted = 1.0f;
			}

			//Check if the thresholds are valid.
			bool bSuccess = (vCollisionNormal.z >= fCollisionThreshold) &&
				(Abs(pPed->GetTransform().GetForward().GetZf()) <= fForwardThresholdAdjusted);
			if(bSuccess)
			{
				//Set the entity we landed on.
				m_LandingData.m_pLandedOn = pEntity;

				return true;
			}
			else
			{
				//Set the crash landing data.
				m_LandingData.m_nType = LandingData::Crash;
				m_LandingData.m_CrashData.m_bWithParachute = true;
				m_LandingData.m_CrashData.m_bPedCollision = true;
				m_LandingData.m_CrashData.m_vCollisionNormal = VECTOR3_TO_VEC3V(vCollisionNormal);

				return false;
			}
		}
	}

	return false;
}

Vec3V_Out CTaskParachute::GenerateFeetPosition() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Generate the feet position.
	Vec3V vFeet;
	Vec3V vLeftFoot;
	Vec3V vRightFoot;
	if(pPed->GetBonePosition(RC_VECTOR3(vLeftFoot), BONETAG_L_FOOT) && pPed->GetBonePosition(RC_VECTOR3(vRightFoot), BONETAG_R_FOOT))
	{
		//Average the feet positions.
		vFeet = Average(vLeftFoot, vRightFoot);
	}
	else
	{
		//Use the ped's bounding box.
		vFeet = pPed->GetTransform().GetPosition() + Scale(pPed->GetTransform().GetC(), ScalarVFromF32(pPed->GetBoundingBoxMin().z));
	}

	//Move the feet position forward by one time step.
	//This is due to the skeleton/physics updating later in the frame.
	float fTimeStep = GetTimeStep();
	vFeet = AdjustPositionBasedOnParachuteVelocity(vFeet, fTimeStep);

	return vFeet;
}

Vec3V_Out CTaskParachute::GenerateHeadPosition() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Generate the head position.
	Vec3V vHead;
	if(pPed->GetBonePosition(RC_VECTOR3(vHead), BONETAG_HEAD))
	{
		//Nothing else to do.
	}
	else
	{
		//Use the ped's bounding box.
		vHead = pPed->GetTransform().GetPosition() + Scale(pPed->GetTransform().GetC(), ScalarVFromF32(pPed->GetBoundingBoxMax().z));
	}

	//Move the head position forward by one time step.
	//This is due to the skeleton/physics updating later in the frame.
	float fTimeStep = GetTimeStep();
	vHead = AdjustPositionBasedOnParachuteVelocity(vHead, fTimeStep);

	return vHead;
}

Vec3V_Out CTaskParachute::AdjustPositionBasedOnParachuteVelocity(Vec3V_In vPosition, float fTimeStep) const
{
	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return vPosition;
	}

	//Grab the parachute velocity.
	Vec3V vParachuteVelocity = VECTOR3_TO_VEC3V(m_pParachuteObject->GetVelocity());

	//Calculate the future position.
	Vec3V vFuturePosition = AddScaled(vPosition, vParachuteVelocity, ScalarVFromF32(fTimeStep));

	return vFuturePosition;
}

void CTaskParachute::UpdateMoVE(const Vector2 &vStick)
{
	//convert the out put to be in the same range as the move inputs 0 - 1
	Vector2 vConverted; 
	vConverted.x = (vStick.x + 1.0f) / 2.0f; 
	vConverted.y = (vStick.y + 1.0f) / 2.0f; 

	CTaskMotionBasicLocomotion::InterpValue(m_fStickX, vConverted.x, sm_Tunables.m_MoveParameters.m_Parachuting.m_InterpRates.m_StickX);
	CTaskMotionBasicLocomotion::InterpValue(m_fStickY, vConverted.y, sm_Tunables.m_MoveParameters.m_Parachuting.m_InterpRates.m_StickY);

	//work out the ratio of inputs and blend on that
	float combinedxy = abs(vStick.x) + abs(vStick.y); 
	float totalinput = Clamp( combinedxy, 0.0f, 1.0f);  
	float xyInputRatio = 0.0f;

	if(combinedxy > 0.0f)
	{
		xyInputRatio = abs(vStick.y) / combinedxy; 
	}

	CTaskMotionBasicLocomotion::InterpValue(m_fTotalStickInput, totalinput, sm_Tunables.m_MoveParameters.m_Parachuting.m_InterpRates.m_TotalStickInput);
	CTaskMotionBasicLocomotion::InterpValue(m_fCurrentHeading, xyInputRatio, sm_Tunables.m_MoveParameters.m_Parachuting.m_InterpRates.m_CurrentHeading);
}

void CTaskParachute::ApplyCgOffsetForces(CPed* UNUSED_PARAM(pPed))
{
	if(m_pParachuteObject)
	{
		// Figure out where ped should be relative to the parachute
		// Ped always 'hangs' down world Z
		static dev_float sfHangDist = 2.5f;

		float fHangDist = sfHangDist;

		Vector3 vOffset = -ZAXIS * fHangDist;
		
		float fMaxPitch = sm_Tunables.m_FlightAngleLimitsForParachutingNormal.m_MaxPitch;
		float fMaxRoll = sm_Tunables.m_FlightAngleLimitsForParachutingNormal.m_MaxRoll;

		float fPitchAngle = fMaxPitch;
		float fRollAngle = fMaxRoll;
		fPitchAngle *= m_fPitch;
		fRollAngle *= -m_fRoll;

		vOffset.RotateX(fPitchAngle);
		vOffset.RotateY(fRollAngle);

		vOffset = VEC3V_TO_VECTOR3(m_pParachuteObject->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vOffset)));

		static dev_bool bApplyOffsetForce = true;
		if (bApplyOffsetForce)
		{
			// Apply forces from CG offset
			Vector3 vForce = -ZAXIS*ms_fApplyCgOffsetMult*m_pParachuteObject->GetMass();
			CFlightModelHelper::ApplyTorqueSafe(vForce,vOffset,m_pParachuteObject);
		}

		// Visualise this so i can see what is going on
#if __DEV
		static bool ms_bVisualiseCgOffsetForces = false;
		if(ms_bVisualiseCgOffsetForces)
		{
			const Vector3 vParachutePosition = VEC3V_TO_VECTOR3(m_pParachuteObject->GetTransform().GetPosition());
			grcDebugDraw::Line(vParachutePosition,vParachutePosition+vOffset,Color_green,Color_green);
			grcDebugDraw::Sphere(vParachutePosition+vOffset,0.1f,Color_red);
			static dev_float fDrawForceScale = 0.2f;
			grcDebugDraw::Line(vParachutePosition+vOffset,vParachutePosition+vOffset-(ZAXIS*fDrawForceScale),Color_blue,Color_blue);
		}
#endif

	}
}

bool CTaskParachute::HasDeployBeenRequested() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}
	
	//Ensure the ped is a player.
	if(!pPed->IsAPlayerPed())
	{
		return false;
	}
	
	//Ensure the ped is a local player.
	if(!pPed->IsLocalPlayer())
	{
		return false;
	}
	
	//Ensure the control is valid.
	const CControl* pControl = pPed->GetControlFromPlayer();
	if(!pControl)
	{
		return false;
	}

	//Ensure we have waited long enough, if transitioning into a skydive.
	static float s_fMinTime = 0.25f;
	if((GetState() == State_TransitionToSkydive) &&
		(GetTimeInState() < s_fMinTime))
	{
		return false;
	}
	
	//Ensure the deploy button is pressed.
	if(!pControl->GetParachuteDeploy().IsPressed())
	{
		return false;
	}
	
	return true;
}

bool CTaskParachute::CanLeaveSmokeTrail() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}
	
	//Ensure the player info is valid.
	const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if(!pPlayerInfo)
	{
		return false;
	}
	
	//Ensure the player can leave a smoke trail.
	if(!pPlayerInfo->GetCanLeaveParachuteSmokeTrail())
	{
		return false;
	}
	
	return true;
}

void CTaskParachute::UpdateVfxSmokeTrail()
{
	//Check if we are leaving a smoke trail.
	if(m_bLeaveSmokeTrail && CanLeaveSmokeTrail())
	{
		//Ensure the ped is valid.
		CPed* pPed = GetPed();
		if(!pPed)
		{
			return;
		}
		
		//Ensure the player info is valid.
		CPlayerInfo* pInfo = pPed->GetPlayerInfo();
		if(!pInfo)
		{
			return;
		}
		
		//Grab the smoke trail color.
		Color32 cColor = pInfo->GetParachuteSmokeTrailColor();

		g_vfxPed.UpdatePtFxParachuteSmoke(pPed, cColor);

		//Increment stats for parachute deployed.
		if (!pPed->IsNetworkClone() && pPed->IsLocalPlayer())
		{
			IncrementStatSmokeTrailDeployed(cColor.GetColor());
		}
	}
}

void CTaskParachute::UpdateVfxPedTrails()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	//Ensure the player info is valid.
	CPlayerInfo* pInfo = pPed->GetPlayerInfo();
	if(!pInfo)
	{
		return;
	}

	g_vfxPed.ProcessVfxPedParachutePedTrails(pPed);
}

void CTaskParachute::UpdateVfxCanopyTrails()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	//Ensure the player info is valid.
	CPlayerInfo* pInfo = pPed->GetPlayerInfo();
	if(!pInfo)
	{
		return;
	}

	//Ensure the parachute is valid.
	if(!m_pParachuteObject)
	{
		return;
	}

	//Convert the left parachute bone ID to an index.
	s32 canopyBoneIndexL = -1;
	if(!m_pParachuteObject->GetSkeletonData().ConvertBoneIdToIndex((u16)P_Para_S_top_wing_LR5, canopyBoneIndexL))
	{
		return;
	}

	//Convert the right parachute bone ID to an index.
	s32 canopyBoneIndexR = -1;
	if(!m_pParachuteObject->GetSkeletonData().ConvertBoneIdToIndex((u16)P_Para_S_top_wing_RR5, canopyBoneIndexR))
	{
		return;
	}

	g_vfxPed.ProcessVfxPedParachuteCanopyTrails(pPed, m_pParachuteObject, canopyBoneIndexL, canopyBoneIndexR);
}

void CTaskParachute::TriggerVfxDeploy()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}

	g_vfxPed.TriggerPtFxParachuteDeploy(pPed);
}

bool CTaskParachute::CanCreateParachute()
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Clone peds rely on their owner to create the parachute object.
	if(pPed->IsNetworkClone())
	{
		return false;
	}

	//Ensure the parachute object is invalid.
	if(m_pParachuteObject)
	{
		return false;
	}

	//Ensure the parachute model has streamed in.
	if(!m_ModelRequestHelper.HasLoaded())
	{
		return false;
	}

	//Check if we are in a network game.
	if(NetworkInterface::IsGameInProgress())
	{
		//Check if we cannot register the object.
		if(!NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_OBJECT, true))
		{
			//Try to make space for the object.
			NetworkInterface::GetObjectManager().TryToMakeSpaceForObject(NET_OBJ_TYPE_OBJECT);

			//Ensure we can register the object.
			if(!NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_OBJECT, true))
			{
				return false;
			}
		}
	}
	
	return true;
}

bool CTaskParachute::CanConfigureCloneParachute() const
{
	CPed const* ped = GetPed();

	if(!ped || !ped->IsNetworkClone())
	{
#if !__FINAL
		if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			const char* gamertag = ped->GetNetworkObject() && ped->GetNetworkObject()->GetPlayerOwner() ? ped->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "";
			parachuteDebugf3("CTaskParachute::CanConfigureCloneParachute--ped[%p][%s] isplayer[%d] gamertag[%s]--!ped || !ped->IsNetworkClone()-->return false",ped,ped ? ped->GetModelName() : "",ped ? ped->IsPlayer() : 0,gamertag);
		}
#endif
		return false;
	}

	if(!m_pParachuteObject)
	{
#if !__FINAL
		if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			const char* gamertag = ped->GetNetworkObject() && ped->GetNetworkObject()->GetPlayerOwner() ? ped->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "";
			parachuteDebugf3("CTaskParachute::CanConfigureCloneParachute--ped[%p][%s] isplayer[%d] gamertag[%s]--!m_pParachuteObject-->return false",ped,ped ? ped->GetModelName() : "",ped ? ped->IsPlayer() : 0,gamertag);
		}
#endif		
		return false;
	}

	if(m_bCloneParachuteConfigured)
	{
#if !__FINAL
		if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
		{
			const char* gamertag = ped->GetNetworkObject() && ped->GetNetworkObject()->GetPlayerOwner() ? ped->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName() : "";
			parachuteDebugf3("CTaskParachute::CanConfigureCloneParachute--ped[%p][%s] isplayer[%d] gamertag[%s]--m_bCloneParachuteConfigured-->return false",ped,ped ? ped->GetModelName() : "",ped ? ped->IsPlayer() : 0,gamertag);
		}
#endif
		return false;
	}

	return true;
}

bool CTaskParachute::CanDeployParachute() const
{
	//Ensure the clip set for parachuting is loaded.
	fwMvClipSetId clipSet = GetClipSetForParachuting();
	if(clipSet == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	if(!fwClipSetManager::IsStreamedIn_DEPRECATED(GetClipSetForParachuting()))
	{
		return false;
	}

	//Ensure the parachute object task is valid.
	CTaskParachuteObject* pTask = GetParachuteObjectTask();
	if(!pTask)
	{
		return false;
	}
	
	return pTask->CanDeploy();
}

bool CTaskParachute::CheckForDeploy()
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Don't allow remote deployment to proceed until the remote parachute has been configured - lavalley
	if (pPed && pPed->IsNetworkClone())
	{
		if (!m_bCloneParachuteConfigured)
			return false;
	}

	//Ensure the parachute can be deployed.
	if(!CanDeployParachute())
	{
		return false;
	}
	
	//Check if the parachute is being forced to open.
	if(m_bForcePedToOpenChute)
	{
		return true;
	}
	
	//Check if the skydive is supposed to be skipped.
	if(m_iFlags.IsFlagSet(PF_SkipSkydiving))
	{
		return true;
	}
	
	//Ensure the ped is not a clone.
	if(pPed && !pPed->IsNetworkClone())
	{
		//Check if the player has requested a deploy.
		if(HasDeployBeenRequested())
		{
			return true;
		}
	}
	else 
	{
		// see if the network as deployed...
		if(GetStateFromNetwork() >= State_Deploying) 
		{
			return true;
		}
	}
	
	return false;
}

bool CTaskParachute::IsParachuteOut() const
{
	//Ensure the parachute object task is valid.
	CTaskParachuteObject* pTask = GetParachuteObjectTask();
	if(!pTask)
	{
		return false;
	}
	
	return pTask->IsOut();
}

#if !__FINAL
const char * CTaskParachute::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Quit);
	static const char* aStateNames[] = 
	{
		"Start",
		"WaitingForBaseToStream",
		"BlendFromNM",
		"WaitingForMoveNetworkToStream",
		"TransitionToSkydive",
		"WaitingForSkydiveToStream",
		"Skydiving",
		"Deploying",
		"Parachuting",
		"Landing",
		"CrashLand",
		"Quit"
	};

	return aStateNames[iState];
}
#endif	// !__FINAL

void CTaskParachute::CleanUp()
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if the ped is using a ragdoll.
	if(pPed->GetUsingRagdoll())
	{
		//Clean up the ragdoll.
		CTaskNMControl::CleanupUnhandledRagdoll(pPed);
	}

	SetCloneSubTaskLaunched(false);

	// If the ped is changing ownership and will get a new parachute task, don't do these things:
	if (!IsPedChangingOwnership())
	{
		//Detach the ped and the parachute.
		DetachPedAndParachute();

		// Destroy the parachute...
		DestroyParachute();

		// Set the lod level for the ped back to it's default...
		ClearParachuteAndPedLodDistance();

		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, false );

		// Clear leg IK flag
		pPed->GetIkManager().ClearFlag(PEDIK_LEGS_USE_ANIM_ALLOW_TAGS);

		//Note that the ped is no longer parachuting.
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsParachuting, false);

		//! If we no longer have a reserve chute, unflag us from using it.
		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute))
		{	
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseReserveParachute, false);
		}

		//! Reset tint index if we no longer have a chute. Only do this in SP, MP, we want to retain choice. And in SP
		//! we reset so that we choose a random chute the next time.
		if(pPed->IsLocalPlayer() && !NetworkInterface::IsGameInProgress() && !DoesPedHaveParachute(*pPed))
		{
			pPed->GetPedIntelligence()->SetTintIndexForParachute(-1);
		}
	}	

	PitchCapsuleBound();

	if(m_networkHelper.IsNetworkActive())
	{
		if (m_pedChangingOwnership)
		{
			m_networkHelper.ReleaseNetworkPlayer();
		}

		pPed->GetMovePed().ClearTaskNetwork(m_networkHelper); 
	}
	
	if(pPed->IsNetworkClone())
	{
		// Stop us from being reattached to the parachute by the owner if we've bailed early or for whatever reason...
		static_cast<CNetObjPed*>(pPed->GetNetworkObject())->ClearPendingAttachmentData();
	
		// if I'm a low lod clone (using TaskParachute's idea of low lod ~75m), then I'll be put into low lod physics when I exit this task...
		if(m_bIsUsingLowLod) 
		{
			if(!pPed->GetUsingRagdoll())
			{
				// So i need to manually set my matrix to standing up and set my bound pitch...
				pPed->SetBoundPitch(0.0f);

				// Wipe the orientation - ped heading is synced so we'll still face the right direction...
				Matrix34 temp = MAT34V_TO_MATRIX34(pPed->GetMatrix());
				temp.Identity3x3();
				pPed->SetMatrix(temp);
			}
		}
	}

	m_FallingGroundProbeResults.Reset();

	//Disable inactive collisions.
#if !__FINAL
	if ((Channel_parachute.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_parachute.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{
		parachuteDebugf3("CTaskParachute::CleanUp pPed[%p][%s][%s] --> SetInactiveCollisions(false)",pPed,pPed ? pPed->GetModelName() : "",pPed && pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");
	}
#endif
	SetInactiveCollisions(false);

	// forcibly re-enable the vehicle / ped collisions...
	ProcessCloneVehicleCollision(true);
	
	//Stop first-person camera shaking.
	StopFirstPersonCameraShaking();

	//Do not depend on the parachute.
	DependOnParachute(false);

	m_ClipSetRequestHelperForParachuting.Release();

	m_ClipSetRequestHelperForFirstPersonBase.Release();
	m_ClipSetRequestHelperForFirstPersonSkydiving.Release();
	m_ClipSetRequestHelperForFirstPersonParachuting.Release();
}

bool CTaskParachute::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Check if the event is valid.
		if(pEvent)
		{
			//Check the event type.
			switch(((CEvent *)pEvent)->GetEventType())
			{
				case EVENT_IN_AIR:
				{
					//Check if we aren't landing.
					if(!IsLanding())
					{
						return false;
					}
					
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}
	
	//Call the base class version.
	return CTask::ShouldAbort(iPriority, pEvent);
}

bool CTaskParachute::IsMoveTransitionAvailable(s32 UNUSED_PARAM(iNextState)) const
{
	parachuteDebugf1("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "NO PED", __FUNCTION__);	

	//Note: At the point at which this function is called, we are always in the AI state of the new state.
	//		We need to check that there is a move transition from the previous state to the current.

	//Grab the previous state.
	int iPreviousState = GetPreviousState();

	//Grab the state.
	int iState = GetState();

	//Check the previous state.
	switch(iPreviousState)
	{
		case State_WaitingForMoveNetworkToStream:
		{
			//Check the state.
			switch(iState)
			{
				case State_TransitionToSkydive:
				case State_WaitingForSkydiveToStream:
				case State_Skydiving:
				{
					return true;
				}
				default:
				{
					return false;
				}
			}
		}
		case State_TransitionToSkydive:
		{
			//Check the state.
			switch(iState)
			{
				case State_WaitingForSkydiveToStream:
				case State_Skydiving:
				case State_Deploying:
				{
					return true;
				}
				default:
				{
					return false;
				}
			}
		}
		case State_WaitingForSkydiveToStream:
		{
			//Check the state.
			switch(iState)
			{
				case State_Skydiving:
				case State_Deploying:
				{
					return true;
				}
				default:
				{
					return false;
				}
			}
		}
		case State_Skydiving:
		{
			//Check the state.
			switch(iState)
			{
				case State_Deploying:
				{
					return true;
				}
				default:
				{
					return false;
				}
			}
		}
		case State_Deploying:
		{
			//Check the state.
			switch(iState)
			{
				case State_Parachuting:
				{
					return true;
				}
				default:
				{
					return false;
				}
			}
		}
		case State_Parachuting:
		{
			//Check the state.
			switch(iState)
			{
				case State_Landing:
				{
					return true;
				}
				default:
				{
					return false;
				}
			}
		}
		default:
		{
			return false;
		}
	}
}

static CTaskParachute::PedVariation staticPedVariation;

const CTaskParachute::PedVariation* CTaskParachute::FindParachutePackVariationForPed(const CPed& rPed)
{
	//Grab the ped's model name hash.
	u32 uModelNameHash = rPed.GetPedModelInfo()->GetModelNameHash();

	//Ensure the variations are valid.
	const ParachutePackVariations* pVariations = FindParachutePackVariationsForModel(uModelNameHash);
	if(!pVariations)
	{
		return NULL;
	}

	TUNE_GROUP_BOOL(PARACHUTE_TEST_STUFF, bDoOverrideVariation, false);
	if(bDoOverrideVariation)
	{
		TUNE_GROUP_INT(TASK_PARACHUTE, m_Component, 5, 0, 100, 1);
		TUNE_GROUP_INT(TASK_PARACHUTE, m_DrawableId, 10, 0, 100, 1);
		TUNE_GROUP_INT(TASK_PARACHUTE, m_DrawableAltId, 0, 0, 50, 1);
		TUNE_GROUP_INT(TASK_PARACHUTE, m_TexId, 0, 0, 50, 1);

		staticPedVariation.m_Component = (ePedVarComp)m_Component;
		staticPedVariation.m_DrawableId = m_DrawableId;
		staticPedVariation.m_DrawableAltId = m_DrawableAltId;
		staticPedVariation.m_TexId = m_TexId;
		return &staticPedVariation;
	}

	//! Hack. Just return override variation if script are overriding.
	if(rPed.GetPlayerInfo() && rPed.GetPlayerInfo()->GetPedParachuteVariationComponentOverride() != PV_COMP_INVALID)
	{
		staticPedVariation.m_Component = rPed.GetPlayerInfo()->GetPedParachuteVariationComponentOverride();
		staticPedVariation.m_DrawableId = rPed.GetPlayerInfo()->GetPedParachuteVariationDrawableOverride();
		staticPedVariation.m_DrawableAltId = rPed.GetPlayerInfo()->GetPedParachuteVariationAltDrawableOverride();
		staticPedVariation.m_TexId = rPed.GetPlayerInfo()->GetPedParachuteVariationTexIdOverride();
		return &staticPedVariation;
	}

	//Check if a parachute pack is equipped.
	for(int i = 0; i < pVariations->m_Variations.GetCount(); ++i)
	{
		const PedVariation& rPedVariation = pVariations->m_Variations[i].m_ParachutePack;

		u32 uDrawableId = rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(rPedVariation.m_Component);
		if(uDrawableId == rPedVariation.m_DrawableId)
		{
			u8 uDrawableAltId = rPed.GetPedDrawHandler().GetVarData().GetPedCompAltIdx(rPedVariation.m_Component);
			if(uDrawableAltId == rPedVariation.m_DrawableAltId)
			{
				u8 uTexId = rPed.GetPedDrawHandler().GetVarData().GetPedTexIdx(rPedVariation.m_Component);
				if(uTexId == rPedVariation.m_TexId)
				{
					return &rPedVariation;
				}
			}
		}
	}

	//Check which parachute pack is valid.
	for(int i = 0; i < pVariations->m_Variations.GetCount(); ++i)
	{
		const ParachutePackVariation& rParachutePackVariation = pVariations->m_Variations[i];
		const PedVariation& rPedVariation = rParachutePackVariation.m_ParachutePack;

		if(rParachutePackVariation.m_Wearing.GetCount() == 0)
		{
			return &rPedVariation;
		}
		else
		{
			for(int j = 0; j < rParachutePackVariation.m_Wearing.GetCount(); ++j)
			{
				const PedVariationSet& rPedVariationSet = rParachutePackVariation.m_Wearing[j];

				u32 uDrawableId = rPed.GetPedDrawHandler().GetVarData().GetPedCompIdx(rPedVariationSet.m_Component);

				for(int k = 0; k < rPedVariationSet.m_DrawableIds.GetCount(); ++k)
				{
					if(uDrawableId == rPedVariationSet.m_DrawableIds[k])
					{
						return &rPedVariation;
					}
				}	
			}
		}
	}

	return NULL;
}

bool CTaskParachute::GivePedParachute(CPed* pPed)
{
	if(pPed->GetInventory())
	{
		pPed->GetInventory()->AddWeapon(GADGETTYPE_PARACHUTE);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_UseReserveParachute, false);
		return true;
	}

	return false;
}

const CTaskParachute::ParachutePackVariations* CTaskParachute::FindParachutePackVariationsForModel(u32 uModelName)
{
	//Iterate over the parachute pack variations.
	for(int i = 0; i < sm_Tunables.m_ParachutePackVariations.GetCount(); ++i)
	{
		//Check if the model name matches.
		const ParachutePackVariations& rVariations = sm_Tunables.m_ParachutePackVariations[i];
		if(uModelName == rVariations.m_ModelName)
		{
			return &rVariations;
		}
	}

	return NULL;
}

CObject* CTaskParachute::CreateParachuteBagObject(CPed *pPed, bool bAttach, bool bClearVariation)
{
	ePedVarComp nVariationComponent = PV_COMP_INVALID;
	u8 uVariationDrawableId = 0;
	u8 uVariationDrawableAltId = 0;
	u8 uVariationTexId = 0;
	CTaskParachute::GetParachutePackVariationForPed(*pPed, nVariationComponent, uVariationDrawableId, uVariationDrawableAltId, uVariationTexId);

	static eAnimBoneTag s_nAttachBone = BONETAG_SPINE3;

	atHashWithStringNotFinal hProp = CTaskParachute::GetModelForParachutePack(pPed);

	CObject *pParaBag = CTaskTakeOffPedVariation::CreateProp(pPed, nVariationComponent, CTaskTakeOffPedVariation::GetModelIdForProp(hProp));

	if(pParaBag && bAttach)
	{
		Vec3V vAttachOffset(CTaskPlayerOnFoot::sm_Tunables.m_ParachutePack.m_AttachOffsetX,
			CTaskPlayerOnFoot::sm_Tunables.m_ParachutePack.m_AttachOffsetY, CTaskPlayerOnFoot::sm_Tunables.m_ParachutePack.m_AttachOffsetZ);
		Vec3V vAttachOrientation(CTaskPlayerOnFoot::sm_Tunables.m_ParachutePack.m_AttachOrientationX * DtoR,
			CTaskPlayerOnFoot::sm_Tunables.m_ParachutePack.m_AttachOrientationY * DtoR, CTaskPlayerOnFoot::sm_Tunables.m_ParachutePack.m_AttachOrientationZ * DtoR);
		QuatV qAttachOrientation = QuatVFromEulersXYZ(vAttachOrientation);

		Vector3 vAdditionalOffset(Vector3::ZeroType);
		Quaternion qAdditionalRotation(Quaternion::IdentityType);

		CTaskTakeOffPedVariation::AttachProp(pParaBag, pPed, s_nAttachBone, vAttachOffset, qAttachOrientation, vAdditionalOffset, qAdditionalRotation );
	}

	if(bClearVariation)
	{
		CTaskTakeOffPedVariation::ClearVariation(pPed, nVariationComponent);
	}

	return pParaBag;
}

//////////////////////////////////////////////////////////////////////////
//	Initial
//////////////////////////////////////////////////////////////////////////

#if __BANK

#if !__FINAL

void CTaskParachute::RenderCloneSkydivingVelocityInfo(void)
{
	TUNE_GROUP_BOOL(TASK_PARACHUTE, RenderCloneVelocityUpdateInfo, false)

	if(RenderCloneVelocityUpdateInfo)
	{
		CPed* pPed = GetPed();

		if(pPed && pPed->IsNetworkClone())
		{
			Vector3 vel			= pPed->GetVelocity();
			Vector3 desiredVel  = pPed->GetDesiredVelocity();

			Vector3 pedPos		= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 networkPos	= NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
			Vector3 delta		= pedPos - networkPos;

			Vector3 predictedPos= static_cast<CNetBlenderPed*>(pPed->GetNetworkObject()->GetNetBlender())->GetCurrentPredictedPosition();
			float dist = (pedPos - networkPos).Mag();

			if((GetState() == State_Skydiving) || (GetState() == State_Deploying))
			{
				if(dist > m_maxdist)
				{
					m_maxdist = dist;
				}

				m_total += dist;
				m_count++;
			}
			
			Vector2 screenPos(0.1f, 0.3f);
			char buffer[128] = "";
			formatf(buffer, 128, "state %s : stateFromNetwork %s", GetStaticStateName(GetState()), GetStaticStateName(GetStateFromNetwork()));
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "dist = %.3f (x: %3f, y:%3f, z: %3f): maxdist = %3f", dist, delta.x, delta.y, delta.z, m_maxdist);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "count %d : average = %.3f", m_count, m_total / m_count);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "network ped velocity %.3f %.3f %.3f", m_vSyncedVelocity.x,  m_vSyncedVelocity.y, m_vSyncedVelocity.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "pre skydiving Velocity %.3f %.3f %.3f", m_preSkydivingVelocity.x,  m_preSkydivingVelocity.y, m_preSkydivingVelocity.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "post skydiving Velocity %.3f %.3f %.3f", m_postSkydivingVelocity.x,  m_postSkydivingVelocity.y, m_postSkydivingVelocity.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f; 

			formatf(buffer, 128, "pre deploying Velocity %.3f %.3f %.3f", m_preDeployingVelocity.x,  m_preDeployingVelocity.y, m_preDeployingVelocity.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "post deploying Velocity %.3f %.3f %.3f", m_postDeployingVelocity.x,  m_postDeployingVelocity.y, m_postDeployingVelocity.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f; 

			formatf(buffer, 128, "vel %.3f %.3f %.3f", vel.x,  vel.y, vel.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "desired vel %.3f %.3f %.3f", desiredVel.x,  desiredVel.y, desiredVel.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "pitch %.3f", m_fPitch);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "roll %.3f", m_fRoll);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "owner parachute out %d", m_isOwnerParachuteOut);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "local (clone) parachute out %d", IsParachuteOut());
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "can deploy parachute %d", CanDeployParachute());
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			Vector3 calcDesiredVel(VEC3_ZERO);
			Matrix34 notNeeded;
			bool use = CalcDesiredVelocity(notNeeded, 0.0f, calcDesiredVel);
			formatf(buffer, 128, "CalcDesiredVelocity %d : %f %f %f", use, calcDesiredVel.x, calcDesiredVel.y, calcDesiredVel.z);
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "base anim clip set streamed in %d", m_ClipSetRequestHelperForBase.IsLoaded());
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "low lod clip set streamed in %d", m_ClipSetRequestHelperForLowLod.IsLoaded());
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "sky diving clip set streamed in %d", m_ClipSetRequestHelperForSkydiving.IsLoaded());
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "parachuting clip set streamed in %d", m_ClipSetRequestHelperForParachuting.IsLoaded());
			grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "final dist skydiving to deploying  %f", m_finalDistSkydivingToDeploy);
			grcDebugDraw::Text(screenPos, Color_red, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			formatf(buffer, 128, "final dist deploying to parachuting %f", m_finalDistDeployingToParachuting);
			grcDebugDraw::Text(screenPos, Color_red, buffer, true, 0.9f, 0.9f);
			screenPos.y += 0.015f;

			if(m_pParachuteObject)
			{
				Vector3 parachutePos = VEC3V_TO_VECTOR3(m_pParachuteObject->GetTransform().GetPosition());
				Vector3 networkPos = NetworkInterface::GetLastPosReceivedOverNetwork(m_pParachuteObject);
				Vector3 delta = parachutePos - networkPos;
				float dist = delta.Mag();
			
				formatf(buffer, 128, "parachute pos %.3f %.3f %.3f", parachutePos.x, parachutePos.y, parachutePos.z );
				grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
				screenPos.y += 0.015f;

				formatf(buffer, 128, "parachute network pos %.3f %.3f %.3f", networkPos.x, networkPos.y, networkPos.z );
				grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
				screenPos.y += 0.015f;

				formatf(buffer, 128, "dist = %.3f (x: %3f, y:%3f, z: %3f)", dist, delta.x, delta.y, delta.z);
				grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
				screenPos.y += 0.015f;

				grcDebugDraw::Sphere(parachutePos, 0.5f, Color_green, false);	
				grcDebugDraw::Sphere(networkPos, 0.5f, Color_aquamarine, false);
			}

			if(GetState() == State_Landing)
			{
				static const char* landingTypes[] = 
				{
					"INVALID",
					"SLOW",
					"REGULAR",
					"FAST",
					"CRASH",
					"WATER",
					"MAX"
				};

				formatf(buffer, 128, "landing type = %s", landingTypes[m_LandingData.m_nType]);
				grcDebugDraw::Text(screenPos, Color_white, buffer, true, 0.9f, 0.9f);
				screenPos.y += 0.015f;
			}

			Vector3 offset(0.1f, 0.0f, 0.0f);

			grcDebugDraw::Line(pedPos + offset, pedPos + offset + m_preSkydivingVelocity, Color_yellow, Color_yellow);
			grcDebugDraw::Line(pedPos, pedPos + m_postSkydivingVelocity, Color_green, Color_green);

			grcDebugDraw::Line(pedPos, networkPos, m_bIsUsingLowLod ? Color_purple : Color_blue, Color_red);
			grcDebugDraw::Sphere(pedPos, 0.5f, m_bIsUsingLowLod ? Color_purple : Color_blue, false);
			grcDebugDraw::Sphere(networkPos, 0.5f, Color_red, false);	
			grcDebugDraw::Sphere(predictedPos, 0.5f, Color_yellow, false);
		}
	}
}

#endif /* !__FINAL */

void CTaskParachute::ParachuteTestCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if (GivePedParachute(pFocusPed))
	{
		//aiTask* pTaskToGiveToFocusPed = rage_new CTaskParachute(GetParachuteModelForCurrentLevel());

		//  Warp to uber height
		bool bFoundGround = false;
		Vector3 vWarpPos = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());

		const float fSecondSurfaceInterp=0.0f;

		float fGroundZ = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp,vWarpPos,&bFoundGround);
		if(!bFoundGround)
		{
			fGroundZ = 0.0f;
		}

		vWarpPos.z = fGroundZ + ms_fDropHeight;
		pFocusPed->Teleport(vWarpPos, pFocusPed->GetCurrentHeading());

		InitialisePedInAir(pFocusPed);
	}
}

void CTaskParachute::ForcePedToOpenChuteCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	aiTask* pTask = pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE);

	if (pTask)
	{
		static_cast<CTaskParachute*>(pTask)->ForcePedToOpenChute();
	}
}

void CTaskParachute::ParachuteFromVehicleTestCB()
{
	//Determine the ped to use.
	CPed* pPed = NULL;
	
	//Grab the focus entity.
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pPed = static_cast<CPed *>(pFocusEntity);
	}
	else
	{
		pPed = CGameWorld::FindLocalPlayer();
	}
	
	//Ensure the ped is valid.
	if(!pPed)
	{
		return;
	}
	
	//Give the ped a parachute.
	if(!GivePedParachute(pPed))
	{
		return;
	}
	
	//Look for the ground.
	bool bFoundGround = false;
	Vector3 vWarpPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	const float fSecondSurfaceInterp=0.0f;

	float fGroundZ = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp,vWarpPos,&bFoundGround);
	if(!bFoundGround)
	{
		fGroundZ = 0.0f;
	}
	
	//Calculate the warp position.
	vWarpPos.z = fGroundZ + ms_fDropHeight;

	//Create a vehicle.
	CVehicleFactory::CreateBank();
	CVehicleFactory::CreateCar();

	//Ensure the vehicle was created.
	CVehicle* pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	if(!pVehicle)
	{
		return;
	}
	
	//Flush the ped tasks.
	pPed->GetPedIntelligence()->FlushImmediately(true);

	//Put the ped in the vehicle.
	pPed->SetPedInVehicle(pVehicle, ms_iSeatToParachuteFromVehicle, CPed::PVF_Warp);
	
	//Teleport the vehicle.
	pVehicle->Teleport(vWarpPos, pPed->GetCurrentHeading());
}

void CTaskParachute::ApplySmokeTrailPropertiesToFocusPedCB()
{
	CEntity* pFocusEnt = CDebugScene::FocusEntities_Get(0);
	CPed* pFocusPed = NULL;
	if(pFocusEnt && pFocusEnt->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEnt);
	}
	else
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}
	
	if(!pFocusPed)
	{
		return;
	}

	CPlayerInfo* pPlayerInfo = pFocusPed->GetPlayerInfo();
	if(!pPlayerInfo)
	{
		return;
	}
	
	pPlayerInfo->SetCanLeaveParachuteSmokeTrail(ms_bCanLeaveSmokeTrail);
	pPlayerInfo->SetParachuteSmokeTrailColor(ms_SmokeTrailColor);
}

void CTaskParachute::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if(pBank)
	{
		pBank->PushGroup("Parachute");
			pBank->AddButton("Make focus ped parachute",ParachuteTestCB);
			pBank->AddButton("Make focus ped open parachute",ForcePedToOpenChuteCB);
			pBank->AddButton("Make focus ped parachute from vehicle", ParachuteFromVehicleTestCB);
			pBank->AddSlider("Seat to parachute from",&ms_iSeatToParachuteFromVehicle,0,MAX_VEHICLE_SEATS - 1,1);
			pBank->AddSlider("interp heading value skydive",&ms_interpRateHeadingSkydive, 0.0f,10.0f,0.5f);
			pBank->AddSlider("Drop height",&ms_fDropHeight,0.0f,4500.0f,1.0f);
			pBank->AddToggle("Debug altitude",&ms_bVisualiseAltitude);
			pBank->AddToggle("Debug info",&ms_bVisualiseInfo);
			pBank->AddSlider("Braking mult",&ms_fBrakingMult, 0.0f, 500.0f, 0.01f);
			
			pBank->AddSlider("Max braking",&ms_fMaxBraking, 0.0f, 100.0f, 0.001f);
			pBank->AddSlider("Apply Cg Offset Mult", &ms_fApplyCgOffsetMult, 0.0f, 50.0f, 0.11f);
			
			pBank->PushGroup("Controls");
				pBank->AddToggle("Visualise controls",&ms_bVisualiseControls);
				pBank->AddSlider("Attach Depth", &ms_fAttachDepth, -5.0f, 5.0f, 0.01f);
				pBank->AddSlider("Attach Length", &ms_fAttachLength, 3.5f, 10.0f, 0.01f);
			pBank->PopGroup();
			pBank->AddToggle("Visualise landing scan",&ms_bVisualiseLandingScan);
			pBank->AddToggle("Visualise crash landing scan",&ms_bVisualiseCrashLandingScan);

			// Braking/Landing
			pBank->PushGroup("Air Speed & Drag Modifiers");
				pBank->AddSlider("Additional Air Speed (Normal) (Min) X", &ms_vAdditionalAirSpeedNormalMin.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Normal) (Min) Y", &ms_vAdditionalAirSpeedNormalMin.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Normal) (Min) Z", &ms_vAdditionalAirSpeedNormalMin.z, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Normal) (Max) X", &ms_vAdditionalAirSpeedNormalMax.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Normal) (Max) Y", &ms_vAdditionalAirSpeedNormalMax.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Normal) (Max) Z", &ms_vAdditionalAirSpeedNormalMax.z, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Normal) (Min) X", &ms_vAdditionalDragNormalMin.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Normal) (Min) Y", &ms_vAdditionalDragNormalMin.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Normal) (Min) Z", &ms_vAdditionalDragNormalMin.z, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Normal) (Max) X", &ms_vAdditionalDragNormalMax.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Normal) (Max) Y", &ms_vAdditionalDragNormalMax.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Normal) (Max) Z", &ms_vAdditionalDragNormalMax.z, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Braking) (Min) X", &ms_vAdditionalAirSpeedBrakingMin.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Braking) (Min) Y", &ms_vAdditionalAirSpeedBrakingMin.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Braking) (Min) Z", &ms_vAdditionalAirSpeedBrakingMin.z, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Braking) (Max) X", &ms_vAdditionalAirSpeedBrakingMax.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Braking) (Max) Y", &ms_vAdditionalAirSpeedBrakingMax.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Additional Air Speed (Braking) (Max) Z", &ms_vAdditionalAirSpeedBrakingMax.z, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Braking) (Min) X", &ms_vAdditionalDragBrakingMin.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Braking) (Min) Y", &ms_vAdditionalDragBrakingMin.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Braking) (Min) Z", &ms_vAdditionalDragBrakingMin.z, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Braking) (Max) X", &ms_vAdditionalDragBrakingMax.x, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Braking) (Max) Y", &ms_vAdditionalDragBrakingMax.y, -100.0f, 100.0f, 0.01f);
				pBank->AddSlider("Air Drag (Braking) (Max) Z", &ms_vAdditionalDragBrakingMax.z, -100.0f, 100.0f, 0.01f);
			pBank->PopGroup();

			pBank->PushGroup("Parachute handling");
				ms_FlightModelHelper.AddWidgetsToBank(pBank);
			pBank->PopGroup();
			pBank->PushGroup("Braking Parachute handling");
				ms_BrakingFlightModelHelper.AddWidgetsToBank(pBank);
			pBank->PopGroup();
			pBank->PushGroup("Skydiving");
				ms_SkydivingFlightModelHelper.AddWidgetsToBank(pBank);
			pBank->PopGroup();
			pBank->PushGroup("Deploying");
				ms_DeployingFlightModelHelper.AddWidgetsToBank(pBank);
			pBank->PopGroup();
			pBank->PushGroup("Smoke Trail");
				pBank->AddToggle("Can Leave Smoke Trail", &ms_bCanLeaveSmokeTrail);
				pBank->AddColor("Smoke Trail Color", &ms_SmokeTrailColor);
				pBank->AddButton("Apply Smoke Trail Properties To Focus Ped", ApplySmokeTrailPropertiesToFocusPedCB);
			pBank->PopGroup();
		pBank->PopGroup();
	}
}

s32 CTaskParachute::GetParachuteModelForCurrentLevel()
{
	return MI_PED_PARACHUTE;
}
#endif


