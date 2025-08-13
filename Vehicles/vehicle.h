// Title	:	Vehicle.h
// Author	:	Richard Jobling/William Henderson
// Started	:	25/08/99
//
//
//
//
#ifndef _VEHICLE_H_
#define _VEHICLE_H_

// Rage headers
#include "fragment/cache.h"
#include "fragment/type.h"
#include "fwmaths/random.h"
#include "fwanimation/animdirector.h"

// Framework headers
#include "fwmaths/vector.h"
#include "fwtl/pool.h"
#include "fwvehicleai/pathfindtypes.h"
#include "entity/extensionlist.h"

// Game headers
#include "ai/spatialarray.h"
#include "audio/vehicleaudioentity.h"
#include "Debug/DebugDrawStore.h"
#include "game/ModelIndices.h"
#include "modelinfo/VehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoFlags.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "modelinfo/VehicleModelInfoFlags.h"
#include "Pathserver/PathServer_DataTypes.h"
#include "physics/gtaInst.h"
#include "renderer/HierarchyIds.h"
#include "renderer/lights/lights.h"
#include "renderer/Entities/VehicleDrawHandler.h"
#include "scene/physical.h"
#include "scene/ContinuityMgr.h"
#include "vehicleAI/VehicleAILod.h"
#include "vehicles/AnchorHelper.h"
#include "vehicles/componentReserve.h"
#include "vehicles/door.h"
#include "vehicles/handlingMgr.h"
#include "vehicles/transmission.h"
#include "vehicles/vehicleDamage.h"
#include "vehicles/VehicleDefines.h"
#include "vehicles/VehicleFlags.h"
#include "vehicles/vehicleLightSwitch.h"
#include "vehicles/wheel.h"
#include "weapons/weaponTypes.h"
#include "system/poolallocator.h"
#include "System/PadGestures.h"						// for define

#define MAX_CARGO_VEHICLES 6

#define MAX_NUM_COVER_DOORS 4

#define VEH_WINDOW_STATE_DIRTY -2

class CAmbientModelVariations;
class CAmbientModelSetFilter;
class CFire;
class CPed;
class CWheel;
class CAnimPlayer;
class CControl;
class CMoveVehicle;
class CCustomShaderEffectVehicle;
class CPropeller;
class CVehicleGadget;
class CVehicleWeaponMgr;
class CVehicleSeatAnimInfo;
class CVehicleEntryPointAnimInfo;
class CVehicleIntelligence;
class CTaskManager;
class CTrailer;
struct vehicleLightSettings;
class CStoredVehicleVariations;
struct ConfigLightSettings;
class CoronaAndLightList;
class CBoatHandling;
class CVehicleWeaponBattery;

namespace rage
{
	class fwPtrList;
	struct grcTextureLock;
}

#if !__FINAL
#define DEBUG_VEH_NAME_STRING_LENGTH (16)
#endif

class CVisualSettings;
class CSubmarineHandling;

class CVehControls
{
public:
	CVehControls()
		:
		m_steerAngle		(0.0f),
		m_secondSteerAngle 	(0.0f),
		m_throttle			(0.0f),
		m_brake				(0.0f),
		m_handBrake			(true),
		m_nitrous			(false),
		m_KERS				(false)
	{;}
	
	void Copy(const CVehControls& rOther)
	{
		m_steerAngle = rOther.m_steerAngle;
		m_secondSteerAngle = rOther.m_secondSteerAngle;
		m_throttle = rOther.m_throttle;
		m_brake = rOther.m_brake;
		m_handBrake = rOther.m_handBrake;
		m_nitrous = rOther.m_nitrous;
		m_KERS = rOther.m_KERS;
	}

	void	Reset()
	{
		m_steerAngle		= 0.0f;
		m_secondSteerAngle 	= 0.0f;
		m_throttle			= 0.0f;
		m_brake				= 0.0f;
		m_handBrake			= true;
		m_nitrous			= false;
		m_KERS				= false;
	}

	inline void	SetSteerAngle	(float val)		{Assertf(val == val, "Provided steer Angle is NaN");  m_steerAngle = val;}
	float	GetSteerAngle		(void) const	{return m_steerAngle;}
	void	SetSecondSteerAngle	(float val)		{m_secondSteerAngle = val;}
	float	GetSecondSteerAngle	(void) const	{return m_secondSteerAngle;}
	void	SetThrottle			(float val)		{Assertf(m_throttle >= -1.0f && m_throttle <= 1.0f, "Throttle out of range (%f)", m_throttle); Assertf(val >= -1.0f && val <= 1.0f, "Throttle out of range (%f)", val); m_throttle = val;}
	float	GetThrottle			(void) const	{return m_throttle;}
	void	SetBrake			(float val)		{Assertf(m_brake >=0.0f && m_brake <= 1.0f, "Brake out of range (%f)", m_brake); Assertf(val >=0.0f && val <= 1.0f, "Brake out of range (%f)", val); m_brake = val;}
	float	GetBrake			(void) const	{return m_brake;}
	void	SetHandBrake		(bool val)		{m_handBrake = val;}
	bool	GetHandBrake		(void) const	{return m_handBrake;}
	void	SetNitrous			(bool val)		{m_nitrous = val;}
	bool	GetNitrous			(void) const	{return m_nitrous;}
	void	SetKERS				(bool val)		{m_KERS = val;}
	bool	GetKERS				(void) const	{return m_KERS;}

	float	m_steerAngle;
	float	m_secondSteerAngle;	// used for steering 2nd set of wheels or elevators etc..
	float	m_throttle;
	float	m_brake;
	bool	m_handBrake;
	bool	m_nitrous;
	bool	m_KERS;
};

enum eVehicleTwinHeadLightMode
{
	VTHL_MODE_SINGLE = 0,
	VTHL_MODE_SINGLE_TEXTURED,
	VTHL_MODE_SINGLE_WITH_FAKE_SPLIT,
	VTHL_MODE_2_LIGHTS,
	VTHL_MODE_SINGLE_TEXTURED_WITH_FAKE_SPLIT,
	VTHL_MODE_2_LIGHTS_TEXTURED,
	VTHL_NUM_MODES
};

enum eVehicleTwinTailLightMode
{
	VTTL_MODE_SINGLE = 0,
	VTTL_MODE_SINGLE_FILLER_ONLY,
	VTTL_MODE_SINGLE_WITH_FAKE_SPLIT,
	VTTL_NUM_MODES
};

class CHeadlightTuningData
{
public:
#if __BANK
	static void	SetupHeadlightTuningBank(bkBank& bank);
#endif // __DEV
	static float	ms_globalHeadlightIntensityMult;
	static float	ms_globalHeadlightDistMult;
	static float	ms_globalConeInnerAngleMod;
	static float	ms_globalConeOuterAngleMod;
	static float	ms_globalOnlyOneLightMod;
	static float	ms_globalFake2LightAngleMod;	
	static float	ms_globalFake2LightsDisplacementMod;
	static float	ms_globalFake2LightAngleModSubmarine;
	static float	ms_globalFake2LightsDisplacementModSubmarine;	
	static bool		ms_useFullBeamOrDippedBeamMods;
	static float	ms_fullBeamHeadlightIntensityMult;
	static float	ms_fullBeamHeadlightDistMult;
	static float	ms_fullBeamHeadlightCoronaIntensityMult;
	static float	ms_fullBeamHeadlightCoronaSizeMult;
	static float	ms_aimFullBeamMod;
	static float	ms_aimDippedBeamMod;
	static bool		ms_makeHeadlightsCastFurtherForPlayer;
	static float	ms_playerHeadlightIntensityMult;
	static float	ms_playerHeadlightDistMult;
	static float	ms_playerHeadlightExponentMult;
	static float	ms_aimFullBeamAngle;
	static float	ms_aimDippedBeamAngle;

	//Volume lights
	static float	ms_volumeIntensityScale;
	static float	ms_volumeSizeScale;
	static Color32	ms_outerVolumeColor;
	static float	ms_outerVolumeIntensity;
	static float	ms_outerVolumeFallOffExponent;

	static eVehicleTwinHeadLightMode ms_VehicleTwinHeadLightMode;
	static eVehicleTwinTailLightMode ms_VehicleTwinTailLightMode;
#if __BANK	
	static u8		ms_ForceHeadlightShadows;
	static bool		ms_useVehicleLodDist;
	static bool		ms_globalForceRecessedCorona;
#endif // __BANK	

	static void Set(const CVisualSettings& visualSettings);
};

class CNeonTuningData
{
public:
#if __BANK
	static void	SetupNeonTuningBank(bkBank& bank);
#endif // __BANK

	static float	ms_globalNeonIntensity;
	static float	ms_globalNeonRadius;
	static float	ms_globalNeonRadiusFallOffExponent;
	static float	ms_globalNeonCapsuleExtentSides;
	static float	ms_globalNeonCapsuleExtentFrontBack;
	static float	ms_globalNeonClipPlaneHeight;
	static float	ms_globalNeonBikeClipPlaneHeight;
	static Color32	ms_defaultNeonColor;

#if __BANK	
	static bool		ms_neonsForcedOn;
#endif // __BANK	

	static void Set(const CVisualSettings& visualSettings);
};

#if __BANK
	extern void SetupVehicleBank(bkBank& bank);
#endif // __BANK



enum eVehicleLodState {
	VLS_HD_NA			= 0,		//	- cannot go into HD at all
	VLS_HD_NONE			= 1,		// 	- no HD resources currently Loaded
	VLS_HD_REQUESTED	= 2,		//  - HD resource requests are in flight
	VLS_HD_AVAILABLE	= 3,		//  - HD resources available & can be used
	VLS_HD_REMOVING		= 4			//  - HD not available & scheduled for removal
};

class CSeaPlaneExtension : public fwExtension
{
public:
	EXTENSIONID_DECL(CSeaPlaneExtension, fwExtension);

	CSeaPlaneExtension();
	~CSeaPlaneExtension() {}

	CAnchorHelper& GetAnchorHelper() { return m_AnchorHelper; }

protected:
	CAnchorHelper m_AnchorHelper;

public:
	struct CSeaPlaneFlags
	{
		u8 bSinksWhenDestroyed	: 1;
	};
	CSeaPlaneFlags m_nFlags;

	float m_fTimeOnWater;
};

enum eBobbleHeadType {
	BHT_START = 0,
	BHT_HEAD = BHT_START,
	BHT_HAND,
	BHT_ENGINE,

	BHT_MISC_1,
	BHT_MISC_2,
	BHT_MISC_3,
	BHT_MISC_4,
	BHT_MISC_5,
	BHT_MISC_6,
	BHT_MISC_7,
	BHT_MISC_8,

	BHT_MAX
};

#define MAX_NUM_BOBBLE_HEADS (8)

class CBobbleHead
{
public:
	CBobbleHead() : m_vehicle( NULL ), m_bobbleHeadVelocity( V_ZERO ), m_bobbleHeadVehiclePrevVelocity( V_ZERO ), m_bobbleHeadPrevDistance( V_ZERO ), m_bobbleHeadDistance( FLT_MAX ) {}
	void Reset();
	void Update();

	CVehicle* GetVehicle() { return m_vehicle; }
	void SetVehicle( CVehicle* pVehicle ) { m_vehicle = pVehicle; }

	eBobbleHeadType GetType() { return m_type; }
	void SetType( eBobbleHeadType type ) { m_type = type; }

	float GetDistanceFromPlayer() { return m_bobbleHeadDistance; }

	void SetVelocity( Vec3V& velocity ) { m_bobbleHeadVelocity = velocity; }

private:
	eHierarchyId	GetBoneIdFromType();

	Vec3V			m_bobbleHeadVelocity;
	Vec3V			m_bobbleHeadVehiclePrevVelocity;
	Vec3V			m_bobbleHeadPrevDistance;
	float			m_bobbleHeadDistance;
	CVehicle*		m_vehicle;
	eBobbleHeadType m_type;
	
	static Vec3V	ms_bobbleHeadStiffness;
	static ScalarV	ms_bobbleHeadMass;
	static ScalarV	ms_bobbleHeadMassInv;
	static Vec3V	ms_bobbleHeadDamping;
	static Vec3V	ms_bobbleHeadLimits;

	static Vec3V	ms_bobbleArmStiffness;
	static ScalarV	ms_bobbleArmMass;
	static ScalarV	ms_bobbleArmMassInv;
	static Vec3V	ms_bobbleArmDamping;
	static Vec3V	ms_bobbleArmLimits;

	static Vec3V	ms_bobbleEngineStiffness;
	static ScalarV	ms_bobbleEngineMass;
	static ScalarV	ms_bobbleEngineMassInv;
	static Vec3V	ms_bobbleEngineDamping;
	static Vec3V	ms_bobbleEngineLimits;

};

class CWeaponBlade
{
public:
	CWeaponBlade() : m_currentRPM( 0.0f ), m_mass( 0.0f ), m_bladeAngle( 0.0f ), m_fragChild( -1 ), m_modBone( -1 ), m_fastBone( -1 ) {}
	void Init( int modBone, int fastBone, int fragChild, eRotationAxis rotationAxis, float maxRotationSpeed, float mass );
	void Reset(){};
	void Update( CVehicle* parentVehicle );
	bool ApplyImpacts( CVehicle* parentVehicle, CEntity* otherEntity, Vec3V myNormal, Vec3V myPosition, Vec3V otherPosition, int bladeIndex, int hitComponent, int otherComponent );
	void ApplyWheelImpact( CVehicle* parentVehicle, CWheel* wheel, int hitComponent );
	void SetRpmToMax() { m_currentRPM = m_maxRPM; }
	void UpdateBladeRpmWhenEngineSetToOff();


	float GetCurrentRPM() const { return m_currentRPM; }
	int GetModBone() const { return m_modBone; }
	int GetFastBone() const { return m_fastBone; }

	int GetFragChild() { return m_fragChild; }
	
	static bool ms_bDisableRetractingWeaponBlades;

private:
	float m_currentRPM;
	float m_mass;
	float m_bladeAngle;
	float m_maxRPM;
	int m_fragChild;
	int m_modBone;
	int m_fastBone;
	eRotationAxis m_rotationAxis;
};

struct ScriptOverridesNitrous
{
	bool isOverrideActive;
	float durationModifier;
	float powerModifier;
	float rechargeRateModifier;
	bool disableDefaultSound;

	void Init()
	{
		isOverrideActive = false;
		durationModifier = 0.0f;
		powerModifier = 0.0f;
		rechargeRateModifier = 0.0f;
		disableDefaultSound = false;
	}
};

#define VEHICLE_REAR_DOOR_BLOWN_DOOR_FLAGS  (CCarDoor::DRIVEN_NORESET | CCarDoor::RELEASE_AFTER_DRIVEN | CCarDoor::DRIVEN_SPECIAL)
class CVehicle : public CPhysical
{
	DECLARE_RTTI_DERIVED_CLASS(CVehicle, CPhysical);

	friend struct CVehicleOffsetInfo;

protected:
	// This is PROTECTED as we can't create a base CVehicle instance
	explicit CVehicle(const eEntityOwnedBy ownedBy, u32 popType, VehicleType vehType);
	friend class CVehicleFactory;

public:

#if DEBUG_DRAW
	static CDebugDrawStore	  ms_debugDraw;
#endif

	static const float sm_fInvalidPlaceOnRoadHeight;
	static float sm_fDoorPushMassScale;
	static bool sm_bCanForceCarRecordingToDummy;
	static bool sm_bForceRecordedVehicleToBeInactive;
	static bool sm_bForceRecordedVehicleToBeActive;
	static bool sm_bSlipstreamingEnabled;
	~CVehicle();

	REGISTER_POOL_ALLOCATOR(CVehicle, false);

	virtual void SetModelId(fwModelId modelId);

	virtual void DeleteDrawable();
	virtual CDynamicEntity*	GetProcessControlOrderParent() const;
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);	
    virtual void ProcessPrePhysics();

	virtual fwDrawData* AllocateDrawHandler(rmcDrawable* pDrawable);

#if !__SPU
	virtual fwMove *CreateMoveObject();
	virtual const fwMvNetworkDefId &GetAnimNetworkMoveInfo() const;
#endif // !__SPU

#if !__NO_OUTPUT
	static void PrintSkeletonData();
#endif

	virtual bool CanBeDeleted(void) const { return CanBeDeletedSpecial(true); }
	virtual void StartAnimUpdate(float fTimeStep);
	virtual void StartAnimUpdateAfterCamera(float fTimeStep);
	bool CanBeDeletedNetwork(const bool bLogClearCarFromAreaFailures = false) const;
	bool CanBeDeletedSpecial(bool bTakeAccountOfMissionPopType, bool bDoNetworkChecks = true, bool bCheckForPedsEnteringAndExiting = true, bool bCheckPedStates = true, bool bCheckInterestingVehicles = true, bool bAllowCopRemovalWithWantedLevel = false, const bool bReturnCarsContainingAPlayer = false, const bool bAllowRemovalOfCarsInGarage = false, const bool bLogClearCarFromAreaFailures = false) const;

	virtual void FlagToDestroyWhenNextProcessed(void);

	inline CVehicleModelInfo* GetVehicleModelInfo() const {return (CVehicleModelInfo*)GetBaseModelInfo();}	

	inline CComponentReservationManager* GetComponentReservationMgr() {return &m_ComponentReservationMgr;}
	inline const CComponentReservationManager* GetComponentReservationMgr() const {return &m_ComponentReservationMgr;}

	CVehicleVariationInstance& GetVariationInstance() { return GetVehicleDrawHandler().GetVariationInstance(); }
	const CVehicleVariationInstance& GetVariationInstance() const{ return GetVehicleDrawHandler().GetVariationInstance(); }
	void SetVariationInstance(const CStoredVehicleVariations& storedVariation);
	bool IsModded() const { return GetVariationInstance().GetNumMods() > 0; }
	void GetExhaustMatrix(eHierarchyId exhaustId, Mat34V_InOut mat, s32& boneIndex) const;
	bool GetExhaustMatrixFromBoneIndex(Mat34V_InOut mat, s32 boneIndex) const;
	bool GetThrustMatrixFromBoneIndex(Mat34V_InOut mat, s32 boneIndex) const;
	void GetExtraLightMatrix(eHierarchyId extraLightId, Mat34V_InOut mat, s32& boneIndex, const char** boneName) const;
	void GetExtraLightMatrix(const crSkeleton *skeleton, eHierarchyId extraLightId, Mat34V_InOut mat, s32& boneIndex) const;
	
	int GetExtraLightPosition(eHierarchyId extraLightId, Vector3& vecResult) const;
	bool AllowBonnetSlide() const;

	virtual void GetGlobalMtx(int boneIdx, Matrix34& matrix) const;

	// Get the lock on position for this vehicle
	virtual void GetLockOnTargetAimAtPos( Vector3& aimAtPos) const;

    bool HasExtraLights() const;
	bool HasNeons() const;
    bool HasDoubleExtraLights() const;

	void PartHasBrokenOff(eHierarchyId id);
	bool CanCreateBrokenPart();
	static void ClearLastBrokenOffPart() { ms_slotTurnedOff = -1; ms_boneTurnedOff = -1; }
	static s8 GetLastBrokenOffPart() { return ms_slotTurnedOff; }

	void SetWheelBroken(u32 wheelIndex);
	const bool GetWheelBroken(u32 wheelIndex) const;
	const u32  GetWheelBrokenIndex() const { return m_wheelBrokenIndex; }
	void ResetWheelBroken() { m_wheelBrokenIndex = 0; }

	void TriggerHydraulicBounceSound();
	void TriggerHydraulicActivationSound();
	void TriggerHydraulicDeactivationSound();

	void SetFlashLocallyDuringRespotting(bool shouldFlash) { m_bFlashLocallyDuringRespotting = shouldFlash; }
	bool GetFlashRemotelyDuringRespotting() { return m_bFlashRemotelyDuringRespotting; }
	void SetFlashRemotelyDuringRespotting(bool shouldFlash) { m_bFlashRemotelyDuringRespotting = shouldFlash; }
	
	void  SetTopSpeedPercentage(float topSpeedPercent) { m_VehicleTopSpeedPercent = topSpeedPercent; }
	float GetTopSpeedPercentage() { return m_VehicleTopSpeedPercent; }
	void  SetOverrideArriveDistForVehPersuitAttack(float topSpeedPercent) { m_fOverrideArriveDistForVehPersuitAttack = topSpeedPercent; }
	float GetOverrideArriveDistForVehPersuitAttack() const { return m_fOverrideArriveDistForVehPersuitAttack; }	
	float GetTargetGravityScale() { return m_targetGravityScale; }
	float GetCachedStickY() { return m_cachedStickYfromNetwork; }
	
	void SetTargetGravityScale(float val) { m_targetGravityScale = val; }
	void SetCachedStickY(float val) { m_cachedStickYfromNetwork = val; }
    void SetHoverModeWingRatio(float val) { m_currentWingRatio = val; }

//////////////////////////////////////////////////////////////////////////
// Position
	virtual void	Teleport(const Vector3& vecSetCoors, float fSetHeading=-10.0f, bool bCalledByPedTask=false, bool bTriggerPortalRescan = true, bool bCalledByPedTask2 = false, bool bWarp=true, bool bKeepRagdoll = false, bool bResetPlants=true);
	void			TeleportWithoutUpdateGadgets(const Vector3& vecSetCoors, float fSetHeading, bool bTriggerPortalRescan, bool bWarp);
	void			ResetAfterTeleport();//called inside teleport
    void            UpdateGadgetsAfterTeleport( const Matrix34 &matOld, bool bDetachFromParent = true, bool bUpdateTrailer = true, bool bUpdateTrailerParent = true, bool translateOnly = false);
    void            UpdateAttachedClonePedsAfterWarp();
	void			UpdateInertiasAfterTeleport();

	void			UpdateAfterCutsceneMovement();

	Vec3V_Out GetVehiclePosition() const { return GetMatrixRef().GetCol3(); }
	Vec3V_Out GetVehicleForwardDirection() const { return GetMatrixRef().GetCol1(); }
	Vec3V_Out GetVehicleUpDirection() const { return GetMatrixRef().GetCol2(); }
	Vec3V_Out GetVehicleRightDirection() const { return GetMatrixRef().GetCol0(); }

	Vec3V_ConstRef GetVehicleForwardDirectionRef() const { return GetMatrixRef().GetCol1ConstRef(); }

	// Accessors that can be overridden to provide different information under for different circumstances
	virtual Vec3V_Out GetVehiclePositionForDriving() const { return GetVehiclePosition(); }
	virtual Vec3V_Out GetVehicleForwardDirectionForDriving() const { return GetVehicleForwardDirection(); }
	virtual Vec3V_Out GetVehicleRightDirectionForDriving() const { return GetVehicleRightDirection(); }
	virtual Vec3V_ConstRef GetVehicleForwardDirectionForDrivingRef() const { return GetVehicleForwardDirectionRef(); }

//////////////////////////////////////////////////////////////////////////
// Base accessors
	VehicleType GetVehicleType() const			{Assertf((m_vehicleType > VEHICLE_TYPE_NONE) && (m_vehicleType < VEHICLE_TYPE_NUM), "Bad vehicle type on '%s'", GetModelName()); return m_vehicleType;}
#if RSG_BANK
	const char* GetVehicleTypeString(VehicleType vehicleType) const;
#endif
	// Some useful queries for casting
	// Lets you easily figure out what the base class might be to avoid long 'if' statements
	bool GetIsAircraft() const { return (GetIsRotaryAircraft() || GetVehicleType() == VEHICLE_TYPE_PLANE);}
	bool GetIsRotaryAircraft() const { return (GetVehicleType() >= VEHICLE_TYPE_HELI && GetVehicleType() <= VEHICLE_TYPE_AUTOGYRO);}
	bool GetIsAquatic() const {return InheritsFromBoat() || InheritsFromSubmarine() || InheritsFromSubmarineCar() || InheritsFromAmphibiousAutomobile();}
	bool GetIsAquaticMode() const {return InheritsFromBoat() || InheritsFromSubmarine() || IsInSubmarineMode();}
	bool GetIsLandVehicle() const {return !GetIsAquatic() && !GetIsAircraft();}
	bool GetIsHeli() const { return GetVehicleType() == VEHICLE_TYPE_HELI; }
	
	virtual bool GetHoverMode() const { return false; }
	virtual bool IsInSubmarineMode() const { return false; }
	
	bool GetIsSmallDoubleSidedAccessVehicle() const
	{
		return InheritsFromBike() || InheritsFromQuadBike() || GetIsJetSki() || InheritsFromAmphibiousQuadBike();
	}
	bool GetIsJetSki() const;
	bool IsBikeOrQuad() const { return (GetVehicleType() == VEHICLE_TYPE_BIKE || GetVehicleType() == VEHICLE_TYPE_QUADBIKE || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE); }

	// Remember aircraft inherit from CAutomobile!
	bool InheritsFromAutomobile() const {return (GetVehicleType() >= VEHICLE_TYPE_CAR && GetVehicleType() <= VEHICLE_TYPE_AUTOGYRO);}
	bool InheritsFromBoat() const { return (GetVehicleType() == VEHICLE_TYPE_BOAT); }
	bool InheritsFromAmphibiousAutomobile() const { return (GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ); }
	bool InheritsFromBike() const { return (GetVehicleType() >= VEHICLE_TYPE_BIKE && GetVehicleType() <= VEHICLE_TYPE_BICYCLE); }
	bool InheritsFromBicycle() const { return GetVehicleType() == VEHICLE_TYPE_BICYCLE; }
	bool InheritsFromTrain() const { return (GetVehicleType() == VEHICLE_TYPE_TRAIN); }
	bool InheritsFromHeli() const { return (GetVehicleType() >= VEHICLE_TYPE_HELI && GetVehicleType() <= VEHICLE_TYPE_BLIMP) ; }
	bool InheritsFromAutogyro() const { return (GetVehicleType() == VEHICLE_TYPE_AUTOGYRO) ; }
	bool InheritsFromBlimp() const { return (GetVehicleType() == VEHICLE_TYPE_BLIMP) ; }
	bool InheritsFromPlane() const { return (GetVehicleType() == VEHICLE_TYPE_PLANE) ; }
	bool InheritsFromSubmarine() const { return (GetVehicleType() == VEHICLE_TYPE_SUBMARINE) ; }
	bool InheritsFromTrailer() const { return (GetVehicleType() == VEHICLE_TYPE_TRAILER) ; }
	bool InheritsFromQuadBike() const { return (GetVehicleType() == VEHICLE_TYPE_QUADBIKE) ; }
	bool InheritsFromAmphibiousQuadBike() const { return (GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE) ; }
	bool InheritsFromDraftVeh() const { return (GetVehicleType() == VEHICLE_TYPE_DRAFT); }
	bool InheritsFromSubmarineCar() const { return (GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR); }

	bool IsTrailerSmall2() const { return InheritsFromTrailer() && MI_TRAILER_TRAILERSMALL2.IsValid() && GetModelIndex() == MI_TRAILER_TRAILERSMALL2; }

	virtual void PopTypeSet(ePopType newPopType);// Does some pop count tracking...

	CMoveVehicle &GetMoveVehicle()			{ Assert(GetAnimDirector()); return (CMoveVehicle&) (GetAnimDirector()->GetMove()); }
	const CMoveVehicle &GetMoveVehicle() const { Assert(GetAnimDirector()); return (const CMoveVehicle&) (GetAnimDirector()->GetMove()); }

	void ForcePostCameraAnimUpdate(bool bForce, bool bUseZeroTimestep) 
	{ 
		if (bUseZeroTimestep)
		{
			m_nVehicleFlags.bForcePostCameraAnimUpdateUseZeroTimeStep = bForce ? 1 : 0;
		}
		else
		{
			m_nVehicleFlags.bForcePostCameraAnimUpdate = bForce ? 1 : 0;
		}
	}

	bool GetWillEngineStart() const { return !m_nVehicleFlags.bEngineWontStart; }
	bool GetActiveForPedNavitation() const { return m_nVehicleFlags.bActiveForPedNavigation; }
	void SetActiveForPedNavitation(bool b) { m_nVehicleFlags.bActiveForPedNavigation = b; }

	// Access/Mutate whether the vehicle has generated cover points
	bool GetHasGeneratedCover() const { return m_nVehicleFlags.bHasGeneratedCover; }
	void SetHasGeneratedCover(bool bGeneratedCover) { m_nVehicleFlags.bHasGeneratedCover = bGeneratedCover; }

	// Get the last cover orientation
	CoverOrientation GetCoverOrientation() const { return m_eLastCoverOrientation; }
	void SetCoverOrientation(CoverOrientation eOrientation ) { m_eLastCoverOrientation = eOrientation; }

	// Access/Mutate the position at last cover generation
	const Vector3& GetLastCoverPosition() const { return m_vLastCoverPosition; }
	void SetLastCoverPosition(const Vector3& vPosition) { m_vLastCoverPosition = vPosition; }

	// Access/Mutate the forward direction at last cover generation
	const Vector3& GetLastCoverForwardDirection() const { return m_vLastCoverForwardDirection; }
	void SetLastCoverForwardDirection(const Vector3& vDirection) { m_vLastCoverForwardDirection = vDirection; }

	// Access/Mutate the door intact status at last cover generation
	bool GetLastCoverDoorIntactStatus(int i) const { if(AssertVerify(i >= 0 && i < MAX_NUM_COVER_DOORS)){ return m_bLastCoverDoorIntactList[i]; } return false;}
	void SetLastCoverDoorIntactStatus(int i, bool bStatus) { if(AssertVerify(i >= 0 && i < MAX_NUM_COVER_DOORS)){ m_bLastCoverDoorIntactList[i] = bStatus; } }

	// Access/Mutate the door ratio at last cover generation
	float GetLastCoverDoorRatio(int i) const { if(AssertVerify(i >= 0 && i < MAX_NUM_COVER_DOORS)){ return m_fLastCoverDoorRatioList[i]; } return 0.0f;}
	void SetLastCoverDoorRatio(int i, float fRatio) { if(AssertVerify(i >= 0 && i < MAX_NUM_COVER_DOORS)){ m_fLastCoverDoorRatioList[i] = fRatio; } }

//////////////////////////////////////////////////////////////////////////
// Flags (probably want to move elsewhere
	bool GetCheatFlag(u32 nFlag) const	{return (m_nVehicleCheats & nFlag) != 0;}
	void SetCheatFlag(u32 nFlag)			{m_nVehicleCheats |= nFlag;}
	void ClearCheatFlag(u32 nFlag)		{m_nVehicleCheats &= ~nFlag;} 
	void SetCheatFlags(u32 nFlags)		{m_nVehicleCheats = nFlags;}

	float GetCheatPowerIncrease() const { return m_fCheatPowerIncrease; }
	void SetCheatPowerIncrease(float val) { m_fCheatPowerIncrease = val; }

	bool UsesSiren(void) const { return m_nVehicleFlags.bHasSiren; }
	bool UsesTandemSeating() const {return (pHandling->mFlags &MF_TANDEM_SEATING)  != 0;}

	void SetScriptMutedSirens(bool bMutedSirens) { m_nVehicleFlags.SetSirenMutedByScript(bMutedSirens); }

	void SetHasGlowingBrakes(bool bNewHasGlowingBrakes) { m_nVehicleFlags.bHasGlowingBrakes = bNewHasGlowingBrakes; }
	bool GetHasGlowingBrakes() const { return (m_nVehicleFlags.bHasGlowingBrakes); }

	inline bool IsLawEnforcementVehicle() const { return (m_nVehicleFlags.bIsLawEnforcementVehicle); }
	inline bool IsLawEnforcementCar() const { return (m_nVehicleFlags.bIsLawEnforcementCar); }
	static bool IsLawEnforcementVehicleModelId(fwModelId modelId);
	static bool IsLawEnforcementCarModelId(fwModelId modelId);
	static bool IsTaxiModelId(fwModelId modelId);
	static bool IsBusModelId(fwModelId modelId);
	static bool IsTractorModelId(fwModelId modelId);
	static bool IsMowerModelId(fwModelId modelId);

	bool HasBulletResistantGlass() const;
	bool HasIncreasedRammingForce();
	bool HasIncreasedRammingForceVsAllVehicles();
	float GetIncreasedRammingForceScale() {	return pHandling->GetCarHandlingData() ? pHandling->GetCarHandlingData()->m_fIncreasedRammingForceScale : 1.0f; }
	bool HasRammingScoop() const;
	bool HasRamp() const;
	bool HasSuperBrakes() const;

	bool IsTunerVehicle() const;
	bool IsTank() const;
	bool IsTaxi() const;
	bool IsTrike() const;
    bool IsReverseTrike() const;
	bool FleesFromCombat() const;
	bool ReportCrimeIfStandingOn() const { return m_nVehicleFlags.bReportCrimeIfStandingOn; }

	void SetIsScheduled(bool bIsScheduled) { m_nVehicleFlags.bScheduledForCreation = bIsScheduled; }
	bool GetIsScheduled() const { return m_nVehicleFlags.bScheduledForCreation; }

	const CAIHandlingInfo* GetAIHandlingInfo() const;

	// LOD-related flags
	bool GetFlagLodFarFromPopCenter() const { return m_nVehicleFlags.bLodFarFromPopCenter; }
	bool GetFlagSuperDummyWheelCompressionSet() const { return m_nVehicleFlags.bLodSuperDummyWheelCompressionSet; }
	void SetFlagSuperDummyWheelCompressionSet(bool bIsSet) { m_nVehicleFlags.bLodSuperDummyWheelCompressionSet = bIsSet; }
	
//////////////////////////////////////////////////////////////////////////
// Process functions
	virtual bool RequiresProcessControl() const {return true;}
	virtual bool WantsToBeAwake();
	bool ForceNoSleep();
	bool ProcessIsAsleep();
	void ProcessPlayerControlInputsForBrakeAndGasPedal(CControl* pControl, bool bForceBrake);

	virtual bool ProcessControl();
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	virtual void DoProcessControl_OnDeferredTaskCompletion(bool fullUpdate, float fFullUpdateTimeStep);
	bool ProcessRumble(const CWheel* const * wheels, s32 numWheels, float velocity);

#if HAS_TRIGGER_RUMBLE
	void ProcessTriggerRumble( const CWheel* const * wheels, s32 numWheels, float velocity, phMaterialMgr::Id padRumbleMaterialId, u32 padNextRumbleTime );
#endif // #if HAS_TRIGGER_RUMBLE

	void ProcessRespotCounter(float fTimeStep);


	void UpdateDoorsForNavigation();

#if __BANK
	virtual void RenderDebug(void) const;
	void RenderDebugBoneInfo(void) const;
	void InitDebugName();

	static void ToggleScriptedAutopilot();
#endif // __DEV

//////////////////////////////////////////////////////////////////////////
// Controls
	void	SetSteerAngle		(float fAngle)		{m_vehControls.SetSteerAngle(fAngle);};
	float	GetSteerAngle		(void) const		{return m_vehControls.GetSteerAngle();}
	void	SetSecondSteerAngle	(float fAngle)		{m_vehControls.SetSecondSteerAngle(fAngle);}	// 4wheel steer. need to store variable separately so we can smooth the transition on and off
	float	GetSecondSteerAngle	(void) const		{return m_vehControls.GetSecondSteerAngle();}
	void	SetThrottle			(float fPedal)		{m_vehControls.SetThrottle(fPedal);}
	float	GetThrottle			(void) const		
	{
		if( pHandling->GetCarHandlingData() &&
			pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT &&
			m_vehControls.GetThrottle() >= 0.0f &&
			Abs( m_vehControls.GetSteerAngle() ) > 0.1f )
		{
			float throttle = Clamp( m_vehControls.GetThrottle() + ( ( 1.0f - m_vehControls.GetThrottle() ) * ( Abs( m_vehControls.GetSteerAngle() ) ) ), -1.0f, 1.0f );
			return throttle;
		}
		return m_vehControls.GetThrottle();
	}
	void	SetBrake			(float fPedal)		{m_vehControls.SetBrake(fPedal);}
	float	GetBrake			(void) const		{return m_vehControls.GetBrake();}	
	bool	GetHandBrake		(void) const		{return m_vehControls.GetHandBrake();}
	void	SetHandBrake		(bool bHandBrake)	{ if(!GetHandBrake() && bHandBrake) m_uHandbrakeOnTime = fwTimer::GetTimeInMilliseconds();
														m_vehControls.SetHandBrake(bHandBrake); }
	void	SetNitrous			( bool bNitrous )	{ m_vehControls.SetNitrous( bNitrous ); }
	void	ToggleNitrous		()					{ m_vehControls.SetNitrous( !m_vehControls.GetNitrous() ); }
	bool	GetNitrousActive	(void) const		{ return m_nitrousActive; }
	void	SetNitrousActive	( bool active ) 	{ m_nitrousActive = active;	}

	void	SetKERS				( bool bKERS )		{ m_vehControls.SetKERS( bKERS ); }
	void	ToggleKERS			()					{ m_vehControls.SetKERS( !m_vehControls.GetKERS() ); }
	bool	GetKERSActive		(void) const		{ return m_vehControls.GetKERS(); }

	bool GetIsHandBrakePressed(const CControl* pControl) const;
	virtual float GetSteerRatioForAnim() { return (m_vehControls.m_steerAngle / pHandling->m_fSteeringLock); } // overridden for bikes atm


//////////////////////////////////////////////////////////////////////////
// Physics
	virtual int  InitPhys();
	virtual void InitCompositeBound();
	virtual void InitDummyInst() { }
	virtual void RemoveDummyInst();
	virtual bool UsesDummyPhysics(const eVehicleDummyMode vdm) const;
	virtual void RemovePhysics();

	void UpdateCollisionOnPartBrokenOff(int componentIndex, fragInst* newFragInst);

	virtual bool HasDummyConstraint() const {return false;}
	virtual void ChangeDummyConstraints(const eVehicleDummyMode UNUSED_PARAM(dummyMode), bool UNUSED_PARAM(bIsStatic)) { }
	virtual void UpdateDummyConstraints(const Vec3V * UNUSED_PARAM(pDistanceConstraintPos)=NULL, const float * UNUSED_PARAM(pDistanceConstraintMaxLength)=NULL) { }
	void HandleDummyProbesBottomingOut(const WorldProbe::CShapeTestResults & dummyProbeResults);
	virtual void ProcessTimeStartedFailingRealConversion() {}
	virtual void ResetRealConversionFailedData() {}
	virtual bool HasShrunkDummyConstraintsAsMuchAsPossible() const {return false;}

	void SetGravityForWheellIntegrator(float fGravity);
	float GetGravityForWheellIntegrator(){return m_fGravityForWheelIntegrator;}

	// PURPOSE:	Make sure that the next call to CAutomobile::UpdateAirResistance() sets up the damping parameters properly.
	// NOTES:	Should be called if m_fDragCoeff is changed, or if the damping parameters need to get updated
	//			for example because we have a new instance or something like that.
	void RefreshAirResistance() { m_AirResistanceState = 0; }

	phInstGta* GetDummyInst() { return m_pDummyInst; }
	const phInstGta* GetDummyInst() const { return m_pDummyInst; }
	bool HasDummyBound() const { return m_nVehicleFlags.bHasDummyBound; }

	void SetDummyMtlId(phMaterialMgr::Id mtlId) {m_dummyMtlId = mtlId;}
	phMaterialMgr::Id GetDummyMtlId() const {return m_dummyMtlId;}

	inline fragInstGta* GetVehicleFragInst() const {return m_pVehicleFragInst;}
	fragInst** GetVehicleFragInstPointerAddress(){return reinterpret_cast<fragInst**>(&m_pVehicleFragInst);} //used to tell the SPU the offset in the class
	virtual fragInst* GetFragInst() const { return reinterpret_cast<fragInst*>(m_pVehicleFragInst); }

private:
#if __BANK
	static bool sm_PreComputeImpacts;
#endif

public:
	virtual ePhysicsResult ProcessPhysics(float UNUSED_PARAM(fTimeStep), bool UNUSED_PARAM(bCanPostpone), int UNUSED_PARAM(nTimeSlice));
	virtual bool ProcessShouldFindImpactsWithPhysical(const phInst* pOtherInst) const;
	virtual void ProcessPreComputeImpacts(phContactIterator impacts);
	void ProcessPreComputeImpactsSuperDummyCar(phContactIterator& impacts);
	virtual void ProcessPostPhysics();

	virtual bool TestNoCollision(const phInst *pOtherInst);

	void ProcessLowLODAudioRequirements(float fTimeStep);

    void FixStuckInGeometry(phCachedContactIterator& iterator);

	bool CloneBounds();

	void HandleDeactivateForPlayback();

	PPUVIRTUAL void LosingFragCacheEntry();

	PPUVIRTUAL void OnActivate(phInst* pInst, phInst* pOtherInst);
	PPUVIRTUAL void OnDeactivate(phInst* pInst);
	virtual void DeActivatePhysics();

	virtual void UpdateEntityFromPhysics(phInst *pInst, int nLoop);
	virtual void UpdatePhysicsFromEntity(bool bWarp=false);

	virtual bool ShouldLoadCollision() const { return !(m_nPhysicalFlags.bDontLoadCollision || IsDummy()); }

	virtual void ProcessCollision(
		phInst const * myInst, 
		CEntity* pHitEnt, 
		phInst const * hitInst,
		const Vector3& vMyHitPos, 
		const Vector3& vOtherHitPos, 
		float fImpulseMag, 
		const Vector3& vMyNormal,
		int iMyComponent,
		int iOtherComponent,
		phMaterialMgr::Id iOtherMaterial,
		bool bIsPositiveDepth,
		bool bIsNewContact);

	// returns true if bits of the vehicle can break off as fragments
	bool CarPartsCanBreakOff() const;

	// PURPOSE: To check if a vehicle should be orientated up for
	// ped entry. For example, a bike might be in a bad state to 
	// play the enter anim, so it's best to re-orientate it and then
	// play the anim.
	// PARAMS:
	// - pVehicle: Vehicle to check if should be orientated up.
	// RETURNS: It should return true if the vehicle should be orientated up.
	// By default it returns false.
	virtual bool ShouldOrientateVehicleUpForPedEntry(const CPed* UNUSED_PARAM(pPed)) const { return false; }

	virtual const Vector3& GetVelocity() const;
	virtual const Vector3& GetAngVelocity() const;
	Vector3 GetVelocityIncludingReferenceFrame() const;
	Vector3 GetReferenceFrameVelocity() const;
	Vector3 GetRelativeVelocity(const CPed& rPed) const;

	virtual void SetVelocity(const Vector3& vel);
	virtual void SetAngVelocity(const Vector3& vecTurnSpeed);

	void SetInactivePlaybackVelocity(const Vector3& vel);
	void SetInactivePlaybackAngVelocity(const Vector3& angVel);


	// internal forces that are applied within the wheel integrator
	void ApplyInternalForceCg(const Vector3& vecForce);
	void ApplyInternalTorque(const Vector3& vecTorque);
	void ApplyInternalTorque(const Vector3& vecForce, const Vector3& vecOffset);

	// PURPOSE:	Apply the given torque and apply the necessary force to result in rotation about the given world position.
	void ApplyInternalTorqueAndForce(const Vector3& vecTorque, const Vector3& vecWorldPosition);

	// We sum the drag we wish to apply for each contact with a foliage bound and then apply a single force later so that
	// we have control over the total drag force. Called from PreComputeImpacts() on the low LOD chassis bound if available.
	void AddToFoliageDrag(const int nComponent, const Vector3& vImpactPosition, const Vector3& vCentreOfFoliageBound, const float fRadiusOfFoliageBound);
	// Apply the drag force for this time step when driving through plants with GTA_TYPE_FOLIAGE set (the shrubbery!) with clamping.
	void ApplyFoliageDrag();
	void ApplyWindDisturbance();

	void AutoLevelAndHeading( float fInAirSteerMult, bool bAdjustRoll = false, bool bAdjustHeading = true );

	// flying stuff
	float HeightAboveCeiling(float Height) const;

	// Fixed waiting for collision update
	virtual bool ShouldFixIfNoCollisionLoadedAroundPosition();
	virtual void OnFixIfNoCollisionLoadedAroundPosition(bool bFix);
	virtual bool IsCollisionLoadedAroundPosition();
	
	bool CanPedsStandOnTop() const;
	bool ShouldExplodeOnContact() const { return GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXPLODE_ON_CONTACT); }
	bool DisableExplodeOnContact() const { return m_nVehicleFlags.bDisableExplodeOnContact; }
	bool SprayPetrolBeforeExplosion() const { return GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPRAY_PETROL_BEFORE_EXPLOSION); }
	u32	 GetExplosionDelay(bool bDelayedExplosion) const { return bDelayedExplosion ? (u32)(fwRandom::GetRandomNumberInRange(CVehicle::ms_fVehicleExplosionDelayMin, CVehicle::ms_fVehicleExplosionDelayMax) * 1000.0f): 0; }

	// Utility function which performs a shape test in the physics level using the vehicle's collision bound as a probe object.
	static int TestVehicleBoundForCollision(
		const Matrix34* pMatrix,
		const CVehicle* pVehicle, u32 nModelIndex=fwModelId::MI_INVALID,
		const CEntity** pExceptions=NULL, int nNumExceptions=0,
		WorldProbe::CShapeTestResults* pBoundTestResults=NULL,
		u32 includeFlags=INCLUDE_FLAGS_ALL,
		bool bUseBoxBound = false,
		bool bDoHeightFixup = true);

	// Function to check if we should disable an impact between a ped and the vehicle they're exiting
	static bool ShouldDisableImpactForPedExitingVehicle(const CPed& rPed, const CVehicle& rVeh, s32 iImpactComponentIndex);
	static eHierarchyId GetDoorHierachyIdFromImpact(const CVehicle& rVeh, s32 iImpactComponentIndex);

	static bool GetMinMaxBoundsFromDummyVehicleBound(const CVehicle& vehicle, Vec3V_Ref vMin, Vec3V_Ref vMax);

	// B*1935020: Calculates a bounding box that takes into account mod kit parts and ignores doors to provide a base size for the vehicle that is more accurate to its current state than the entity AABB or modelinfo bounds size.
	const void GetApproximateSize(Vector3 &vecBBoxMin, Vector3 &vecBBoxMax) const;

	enum eVehicleOrientation
	{
		VO_Upright,
		VO_UpsideDown,
		VO_OnSide
	};

	static eVehicleOrientation GetVehicleOrientation(const CVehicle& rVeh);

	// glider functions
	enum EGliderState
	{
		NOT_GLIDING,
		DEPLOYING_WINGS,
		GLIDING,
		RETRACTING_WINGS,

		GLIDER_STATE_MAX
	};

	EGliderState GetGliderState() const { return m_gliderState; }

	virtual void ProcessOtherAttachments();
	
	float CalculateInvMassScale(float fVehicleSpeed, bool bIsOtherInstanceFirst, const CVehicle& rOtherVehicle) const;

	void UpdateSlipStreamTimer(float fTimeStep);
	void UpdateProducingSlipStreamTimer(float fTimeStep);
	float GetSlipStreamEffect() const;
	float GetTimeInSlipStream() const { return m_fTimeInSlipStream; }

	virtual bool CanStraddlePortals() const { return true; }

	static bool UseHardRevLimitFlag(CHandlingData* pHandling, bool bIsTunerVehicle, bool bIsCurrentGearTopGear )
	{
		FastAssert(pHandling);
		bool bHasHardRevLimitFlag = pHandling->GetCarHandlingData() && (pHandling->GetCarHandlingData()->aFlags & CF_HARD_REV_LIMIT);
		if( bHasHardRevLimitFlag && bIsTunerVehicle )
		{
			if( !bIsCurrentGearTopGear )
			{
				bHasHardRevLimitFlag = false;
			}
		}
		return bHasHardRevLimitFlag;
	}

	static bool HasHoldGearWheelSpinFlag(CHandlingData* phandling, CVehicle* pParentVehicle)
	{
		FastAssert(phandling);
		FastAssert(pParentVehicle);
		bool bHasHoldGearWheelSpinFlag = phandling->GetCarHandlingData() && (phandling->GetCarHandlingData()->aFlags & CF_HOLD_GEAR_WITH_WHEELSPIN );
		if(bHasHoldGearWheelSpinFlag && pParentVehicle->m_bDriftTyres )
		{
			bHasHoldGearWheelSpinFlag  = false;
		}
		return bHasHoldGearWheelSpinFlag;
	}

#if GPU_DAMAGE_WRITE_ENABLED
	void SetDamageUpdatedByGPU(bool bDamageWasUpdatedByGPU, bool bEnableDamage);
	bool WasDamageUpdatedByGPU() const { return m_bDamageWasUpdatedByGPU; }
	void HandleDamageUpdatedByGPU();
	virtual void PostBoundDeformationUpdate();
	virtual bool HasBoundUpdatePending() const;
#endif

//////////////////////////////////////////////////////////////////////////
// Population and Dummy mode conversion management support.
	void			DelayedRemovalTimeReset			(u32 extraTimeMs = 0);

	void			SetDelayedRemovalTime			(u32 time)
	{
		// Store the current time.
		m_delayedRemovalResetTimeMs = fwTimer::GetTimeInMilliseconds();

		m_delayedRemovalAmountMs = time;
	}

	bool			DelayedRemovalTimeHasPassed		(float removalTimeScale = 1.0f, u32 removalTimeExtraMs = 0) const;
	float			DelayedRemovalTimeGetPortion	(float removalTimeScale = 1.0f) const;

	u32				GetTimeLastVisible() const { return m_iTimeLastVisible; }
	u32				GetNumVisiblePixels() const { return m_nNumPixelsVisible; }
	bool			OffScreenRemovalTimeHasPassed(s32& timeRemaining, float removalTimeScale = 1.0f) const;

	bool			IsDummy() const { return m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodDummy | CVehicleAILod::AL_LodSuperDummy); }
	bool			IsSuperDummy() const { return (m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy)); }
    bool            ShouldAlwaysTryToBeReal() const;
	bool			ShouldTryToUseVirtualJunctionHeightmaps() const {return IsDummy(); }
	eVehicleDummyMode		ProcessOverriddenDummyMode(eVehicleDummyMode desiredDummyMode);
	eVehicleDummyMode GetOverriddenDummyMode(eVehicleDummyMode desiredDummyMode, bool &zeroVelocitiesOut);
	bool			ProcessHaveDummyConstraintsChanged(ScalarV exceedLimit, bool failOnNoConstraints) const;
	virtual bool	GetCanMakeIntoDummy(eVehicleDummyMode dummyMode);
	virtual bool	TryToMakeIntoDummy(eVehicleDummyMode dummyMode, bool bSkipClearanceTest=false);
	virtual bool	TryToMakeFromDummy(const bool bSkipClearanceTest=false);
	bool			TryToMakeFromDummyIncludingParents(const bool bSkipClearanceTest=false);
	bool			TestForDummyInstClearance() const;
	bool			ConsiderParkedForLodPurposes() const 
	{ 
		return (PopTypeGet() == POPTYPE_RANDOM_PARKED) && !m_nVehicleFlags.bHasParentVehicle && !InheritsFromTrailer();
	}
	bool			IsParkedSuperDummy() const
	{
		return IsSuperDummy() && ConsiderParkedForLodPurposes();
	}
	bool ShouldSkipUpdatesInTimeslicedLod() const { return m_nVehicleFlags.bLodShouldSkipUpdatesInTimeslicedLod; }
	void IncrementNumTimeslicedUpdatesSkipped() { ++m_iNumTimeslicedUpdatesSkipped; }
	void ResetNumTimeslicedUpdatesSkipped() { m_iNumTimeslicedUpdatesSkipped = 0; }

	virtual void SwitchToDummyPhysics(const eVehicleDummyMode prevMode);
	virtual void SwitchToSuperDummyPhysics(const eVehicleDummyMode prevMode);
	virtual void SwitchToFullFragmentPhysics(const eVehicleDummyMode prevMode);

    // PURPOSE: Called to switch gravity off for the specified phys inst
    void SetNoGravity(phInst *physInst, bool noGravity);

	void UpdateFlagsForTimeslicedLod();

	// PURPOSE:	Called when switching to timesliced updates (AL_LodTimeslicing is set).
	void SwitchAwayFromTimeslicedLod();

	// PURPOSE:	Called when switching out of timesliced updates (AL_LodTimeslicing is cleared).
	void SwitchToTimeslicedLod();

	void ProcessInterpolation(bool fullUpdate, float fFullUpdateTimeStep);

	// PURPOSE:	Interpolate the position of the vehicle towards a target, during timesliced LOD.
	void ProcessInterpolationForTimeslicedLod();

	// PURPOSE:	Set a new target position and orientation to use as an interpolation target
	//			during timesliced updates. If updates continue, the vehicle should be at the
	//			target after the specified time has elapsed.
	void SetInterpolationTargetAndTime(Vec3V_In posV, QuatV_In orientationV, ScalarV_In timeToInterpolateV)
	{
		Vec4V posAndTimeV(posV);
		posAndTimeV.SetW(timeToInterpolateV);
		m_InterpolationTargetAndTime = posAndTimeV;
		m_InterpolationTargetOrientation = orientationV;

		// When we set a new target, clear out the previously stored current orientation, forcing
		// it to get recomputed. This simplifies things because we don't have to worry about using
		// some old value.
		m_InterpolationCurrentOrientation.ZeroComponents();
	}

	// PURPOSE:	Clear any interpolation target set by SetInterpolationTargetAndTime().
	void ClearInterpolationTargetAndTime()
	{
		m_InterpolationTargetAndTime.ZeroComponents();
		// Could clear m_InterpolationTargetOrientation too here, but it shouldn't be necessary - the important
		// part is that the time component (W in m_InterpolationTargetAndTime) is zero.

		// Clear the current orientation too, so we don't accidentally reuse some old value.
		m_InterpolationCurrentOrientation.ZeroComponents();
	}

	Vec4V_Out GetInterpolationTargetAndTime() const { return m_InterpolationTargetAndTime; }
	QuatV_Out GetInterpolationTargetOrientation() const { return m_InterpolationTargetOrientation; }

	// PURPOSE:	Compute a new interpolation target by extrapolating the current velocity,
	//			and optionally applying some dampening to gradually slow down.
	// PARAMS:	allowSpeedReduction		- If true, allow the speed to be dampened.
	void SetInterpolationTargetFromExtrapolatedVelocity(bool allowSpeedReduction);

	// PURPOSE:	Call SetSuperDummyVelocity(), with a velocity computed from the vehicle's current position and its interpolation target.
	void SetSuperDummyVelocityFromInterpolationTarget();

	bool ProcessSuperDummy(float fUpdateTime, bool bAttachPhase = false);

	bool TryToDeactivateSuperDummy();

	bool DoBoundingBoxIntersectionTest(s32 iArchtypeFlags, int iNumExpections = 0, const CEntity ** pIntersectionException = NULL, const bool bDoHeightFixup = true, float* fIntersectDepth = NULL, Matrix34* pTestMatrix = NULL) const;

    void LerpCloneSuperDummyMatrix(Matrix34 &matrix);

#if __BANK
	void        SetNonConversionReason(const char* val) const { m_pNonConversionReasonString = val; }
	const char* GetNonConversionReason() const                { return m_pNonConversionReasonString; }

    void        SetLastVehPopRemovalFailReason(const char *reason) const;
    const char *GetLastVehPopRemovalFailReason() const { return m_pLastVehPopRemovalFailReason; }
    unsigned    GetLastVehPopRemovalFailFrame()  const { return m_iLastVehPopRemovalFailFrame; }
#endif // __BANK

	// Move a dummy vehicle so that it isn't intersecting a ground surface
	void FixupIntersectionWithWheelSurface(WorldProbe::CShapeTestResults& testResults);

	// Pretend occupant management support.
	virtual bool CanSetUpWithPretendOccupants() const;
	virtual void SetUpWithPretendOccupants();
	virtual bool IsUsingPretendOccupants() const;
	virtual bool TryToMakePedsIntoPretendOccupants();

	bool HasNetworkClonedOccupant();
	void RemoveAllOccupants();

	bool TryToMakePedsFromPretendOccupants(bool onlyAddDriver = false, const int* in_pMaxNumPedsToMakeReal = NULL, const CAmbientModelSetFilter * pModelsFilter = NULL);
	void TryToGiveEventReponseTaskToOccupant(CPed* pPed);
	
	void SetStartNodes(const CNodeAddress &from, const CNodeAddress &to, s16 link, s8 lane)
	{
		m_StartNodes[0] = from;
		m_StartNodes[1] = to;
		m_StartLink = link;
		m_StartLane = lane;
	}

	void GetStartNodes(CNodeAddress &rFrom, CNodeAddress &rTo, s16 &rLink, s8 &rLane) const
	{
		rFrom = m_StartNodes[0];
		rTo = m_StartNodes[1];
		rLink = m_StartLink;
		rLane = m_StartLane;
	}

	void ClearStartNodes()
	{
		m_StartNodes[0].SetEmpty();
		m_StartNodes[1].SetEmpty();
		m_StartLink = -1;
		m_StartLane = -1;
	}

	bool HasStartNodes() const
	{
		return !m_StartNodes[0].IsEmpty();
	}

	// Default task
	void GiveDefaultTask();

//////////////////////////////////////////////////////////////////////////
// AI
	void InitAi();
	void PostCreationAiUpdate();

	void ProcessIntelligence(bool fullUpdate, float fFullUpdateTimeStep);
	void ProcessPostCamera();

	void ForcePostCameraAiUpdate(bool bForce) { m_nVehicleFlags.bForcePostCameraAiSecondaryTaskUpdate = bForce; }

	CVehicleIntelligence *GetIntelligence() const {return m_pIntelligence;}

	void WeAreBlockingSomeone();
	bool ShouldBeRemovedByAmbientPed() const;
	bool HasCarStoppedBecauseOfLight(bool bLookFurther = false) const;	// function itself is in pathfind.cpp
	void UpdateJunctions();												// Add vehicle to the junction it is next to encounter.
	void SetHandlingCheatValuesForPoliceCarInPursuit(bool bCheat);		

	void UpdateInSlownessZone();	// Works out whether this vehicle is in a slowness zone.

	void SetDrivenDangerouslyThisUpdate(bool val) { m_nVehicleFlags.bDrivenDangerouslyThisUpdate = val; }
	float GetDrivenDangerouslyTime() const { return m_fDrivenDangerouslyTime; }
	void SetDrivenDangerouslyExtremeThisUpdate() { m_nVehicleFlags.bDrivenDangerouslyExtreme = true; }
	void UpdateDangerousDriverEvents(float fTimeStep);	// Update timer and generate shocking events

	void GiveWarning();// Flash the headlights, etc.

	CTaskManager *GetTaskManager();

	// Vehicle lodding control class (this is AI/physics LOD, not rendering LOD)
	const CVehicleAILod& GetVehicleAiLod() const { return m_vehicleAiLod; }
	CVehicleAILod& GetVehicleAiLod() { return m_vehicleAiLod; }

    void SelectAppropriateGearForSpeed();

    void SetPlayerDriveTask(CPed *pPed);

	void CacheAiData();
	void InvalidateCachedAiData();

	Vector3 GetAiVelocity() const { Assertf(IsFiniteAll(m_CachedAiData.m_VelocityAndXYSpeed), "Cached Vehicle AI Velocity is invalid."); return VEC3V_TO_VECTOR3(m_CachedAiData.m_VelocityAndXYSpeed.GetXYZ()); }
	const Vec3V& GetAiVelocityConstRef() const { return RCC_VEC3V(RCC_VECTOR3(m_CachedAiData.m_VelocityAndXYSpeed)); /* using a double cast here because we currently don't have a direct conversion from const Vec4V& to const Vec3V& */ }
	float GetAiXYSpeed() const { Assertf(IsFiniteAll(m_CachedAiData.m_VelocityAndXYSpeed), "Cached Vehicle AI XY speed is invalid."); return m_CachedAiData.m_VelocityAndXYSpeed.GetWf(); }
	bool IsCachedAiDataValid() const { return IsFiniteAll(m_CachedAiData.m_VelocityAndXYSpeed); }

	// Used to check if certain shocking events (horn sounded, mad driver) should be allowed to be generated.
	bool ShouldGenerateMinorShockingEvent() const;

	// Right now just the tank is considered dangerous like this, but it could be a model info flag if necessary.
	bool GetIsConsideredDangerousForShockingEvents() const { return IsTank(); }

	CVehicle* GetVehicleDrivingOn() const;

	// PURPOSE:	Prefetch the memory where the pointer to the matrix is stored - not the matrix itself.
	void PrefetchPtrToMatrix() const { PrefetchDC(&m_MatrixTransformPtr); }

	// PURPOSE:	Prefetch the memory where the pointer to the model info is stored - not the model info itself.
	void PrefetchPtrToModelInfo() const { PrefetchDC(&m_pArchetype); }

	virtual bool GetBurnoutMode() const { return false; }

	bool GetBlockingOfNonTemporaryEvents() const { return (GetBlockingOfNonTemporaryEventsThisFrame()); }
	bool GetBlockingOfNonTemporaryEventsThisFrame() const;

	bool GetIsUsingScriptAutoPilot() const;

#if __ASSERT
	void VerifyCache();
#endif

//////////////////////////////////////////////////////////////////////////
// Occupants

	CPed *PickRandomPassenger();
	bool PickRandomPassengers(int iNumToPick, atArray<int>& aiSeatIndecesOut);
	CPed *GetPedInSeat(int iSeat) const;

	// Get Whether the vehicle has any occupants in the vehicle ped spawn queue
	bool HasScheduledOccupants() const { return m_SeatManager.GetNumScheduledOccupants() > 0;}
	// Let the vehicle know that it has a ped waiting to be spawned
	void AddScheduledOccupant() { m_SeatManager.AddScheduledOccupant(); }
	// Let the vehicle know that a waiting ped has been spawned
	void RemoveScheduledOccupant() { m_SeatManager.RemoveScheduledOccupant(); }

	bool ContainsPlayer() const { return m_SeatManager.GetNumPlayers() > 0; }
	bool IsDriverAPlayer() const;
	bool ContainsLocalPlayer( ) const;
	bool ContainsPed(const CPed* pPed) const;
	s32  GetNumberOfPassenger() const;

	bool HasMissionCharsInIt() const;
	bool HasPedsInIt() const;
	bool HasAlivePedsInIt() const;
	bool HasAliveLawPedsInIt() const;
	bool HasAliveVictimInIt() const;

	void KillPedsInVehicle(CEntity *pCulprit, u32 weaponHash);
	void KillPedsGettingInVehicle(CEntity *pCulprit);

	bool ShufflePassengersToMakeSpace();

	bool CanBeDriven() const;

	// apparently this is used to flag occupants of cop veh to not fly through the windscreen when player has a wanted level > 1
	void EveryoneInsideWillFlyThroughWindscreen(bool bSetVal);

	// Seats with collision are used for open top vehicles to knock peds off
	void ProcessSeatCollisionFlags();
	void ProcessSeatCollisionBound(bool bReset = false);

	bool IsInDriveableState() const;

	bool IsInEnterableState(bool bIgnoreSideTest = false) const;

//////////////////////////////////////////////////////////////////////////
// Get in/out
	bool CanPedEnterCar(const CPed *pEnteringPed) const;	
	virtual bool CanPedStepOutCar(const CPed* pPed, bool bIgnoreSpeedCheck = false, bool bDoGroundCheckForHelis = false) const;
	virtual bool CanPedJumpOutCar(CPed* pPed, bool bForceSpeedCheck = false) const;
	bool CanPedExitCar();

	virtual bool UseBikeGetOn() const {return false;}

	void SetCarDoorLocks(eCarLockState NewLockState);
	void SetRemoteCarDoorLocks(eCarLockState NewLockState);
	bool CanPedOpenLocks(const CPed *pEnteringPed) const;
	eCarLockState GetCarDoorLocks() const {return m_eDoorLockState;}

	void ResetCarDoorKnockedOpenStatus();

	// entry position and attachment matrix calculations
	void GetVehicleGetInOffset(Vector3& vOffset, float& fHeadingRotation, int iEntryPointIndex) const;
	void GetSeatAttachmentOffsets(Vector3& vTransOffset, Quaternion& qRotOffset, s32 iSeatIndex) const;

// 
	// Accessor functions for the entry/exit points
	const CEntryExitPoint*		GetEntryExitPoint(s32 i) const;
	s32					GetEntryExitPointIndex(const CEntryExitPoint* pEntryExitPoint) const;
	// Returns the entry point position	
	s32					GetNumberEntryExitPoints() const;
	bool				IsEntryPointIndexValid(s32 i) const;

	static float GetDoorRatioToConsiderDoorOpen(const CVehicle& rVeh, bool bIsCombatEntry);
	static bool IsEntryPointClearForPed(const CVehicle& vehicle, const CPed& ped, s32 iEntryPoint, const Vector3* pvPosModifier = NULL, bool bDoCapsuleTest = false);
	static bool IsDoorClearForPed(const Vector3& vEntryPosition, const CVehicle& vehicle, const CPed& ped, s32 iEntryPoint);

	bool HasWaterEntry() const;

//////////////////////////////////////////////////////////////////////////
// Wheels, doors and roof
	// Virtual wheel stuff
	virtual void InitWheels(){;}
	virtual void SetupWheels(const void* UNUSED_PARAM(damageTexture)){;}
	virtual bool IsInAir(bool UNUSED_PARAM(bCheckForZeroVelocity) = true) const {return false;}
	virtual bool HasLandedStably() const;

	int GetNumWheels() const { return m_nNumWheels; }
	__forceinline CWheel* GetWheel(int i) { if(i < m_nNumWheels) return m_ppWheels[i];	else return NULL; }
	__forceinline const CWheel* GetWheel(int i) const { if(i < m_nNumWheels) return m_ppWheels[i];	else return NULL; }
	CWheel* GetWheelFromId(eHierarchyId nId);
	const CWheel* GetWheelFromId(eHierarchyId nId) const;
	const u32 GetWheelIndexFromId(eHierarchyId nId) const;
	int GetNumContactWheels() const;	// More expensive than HasContactWheels(), use only if necessary.
	bool HasContactWheels() const;
	bool IsWheelContactPhysicalMoving() const;
	void ResetSuspension();
	void TeleportWheels();
	CWheel * const * GetWheels() { return m_ppWheels; }
	const CWheel * const * GetWheels() const { return m_ppWheels; }
	bool GetAreWheelsJustProbes() const { return m_WheelsAreJustProbes; }
	void InitialiseWeaponBlades();
	void InitialiseWeaponBladeCollision( int bladeMod, int bladeIndex );
	void InitialiseSpikeCollision();
	void InitialiseRammingScoops();
	void InitialiseRamps();
	void InitialiseRammingBars();
	int GetNumRammingScoopBones() const;
	int GetNumFrontSpikeBones() const;
	int GetNumRampBones() const;
	int GetNumRammingBarBones() const;

	// Virtual door stuff
	virtual void InitDoors();
	int GetNumDoors() const { return m_nNumDoors; }
	CCarDoor* GetDoor(int i) { if(i < m_nNumDoors) return &m_pDoors[i];		else return NULL; }
	const CCarDoor* GetDoor(int i) const { if(i < m_nNumDoors) return &m_pDoors[i];		else return NULL; }
	
	CCarDoor* GetDoorFromId(eHierarchyId nId);
	const CCarDoor* GetDoorFromId(eHierarchyId nId) const;

	CCarDoor* GetDoorFromBoneIndex(int iBoneIndex);
	const CCarDoor* GetDoorFromBoneIndex(int iBoneIndex) const;
	CCarDoor* GetSecondDoorFromEntryPointIndex(s32 iEntryPointIndex);
	const CCarDoor* GetSecondDoorFromEntryPointIndex(s32 iEntryPointIndex) const { return const_cast<CVehicle*>(this)->GetSecondDoorFromEntryPointIndex(iEntryPointIndex); }

	CCarDoor* GetArticulatedDoorFromComponent(int componentIndex);

	bool CanDoorsBeDamaged(void) const;

	eCarLockState GetDoorLockStateFromDoorIndex(int i) const;
	eCarLockState GetDoorLockStateFromEntryPoint(int i) const;
	eCarLockState GetDoorLockStateFromSeatIndex(int i) const;

	void SetDoorLockStateFromDoorIndex(int i, eCarLockState eState);
	void SetDoorLockStateFromDoorID(eHierarchyId eDoorID, eCarLockState eState);
	void SetDoorLockStateFromEntryPoint(int i, eCarLockState eState);
	void SetDoorLockStateFromSeatIndex(int i, eCarLockState eState);


	bool IsPedUsingDoor();
	bool IsPedUsingDoor( CCarDoor* pDoor );

	const bool GetRemoteDriverDoorClosing() const { return m_remoteDriverDoorClosing; }
	void SetRemoteDriverDoorClosing(bool value) { m_remoteDriverDoorClosing = value; }

	bool DoesVehicleHaveAConvertibleRoofAnimation() const;
	bool DoesVehicleHaveATransformAnimation() const;
    s32 GetConvertibleRoofState() const;
	float GetConvertibleRoofProgress() const;
	bool IsDoingInstantRoofTransition() const;
    void LowerConvertibleRoof(bool bInstantlyLowerRoof);
    void RaiseConvertibleRoof(bool bInstantlyRaiseRoof);
	bool ShouldLowerConvertibleRoof() const;
	void ToggleRoofCollision(bool bSwitchOn);

	void TransformToSubmarine( bool bInstantlyTransform );
	void TransformToCar( bool bInstantlyTransform );
	void PreventSubmarineCarTransform(bool prevent) { m_bSubmarineCarTransformPrevented = prevent; }
	s32 GetTransformationState() const;
	void SetTransformRateForCurrentAnimation(float fRate);

	s32 GetSeatIndexFromDoor(const CCarDoor* pDoor) const;

//////////////////////////////////////////////////////////////////////////
// Bones and components
	inline int GetBoneIndex(eHierarchyId nComponent) const {return GetVehicleModelInfo()->GetBoneIndex(nComponent);}
	inline bool HasComponent(eHierarchyId nComponent) const {return GetVehicleModelInfo()->GetBoneIndex(nComponent)!=-1;}
	bool GetDefaultBonePositionForSetup(eHierarchyId nComponent, Vector3& vecResult) const;
	bool GetDefaultBoneRotationForSetup(eHierarchyId nComponent, Quaternion& quatResult) const;
	void GetDefaultBonePosition(s32 boneIndex, Vector3& vecResult) const;
	void GetDefaultBoneRotation(s32 boneIndex, Quaternion& quatResult) const;
	void GetDefaultBonePositionSimple(s32 boneIndex, Vector3& vecResult) const;

	const char* GetHierarchyName(eHierarchyId nId) const;

	void SetComponentRotation(eHierarchyId compId, eRotationAxis nAxis, float fAngle, bool bSetRotate=true, Vector3* pVecRotateAboutOffset=NULL, Vector3* pVecShiftPos=NULL);
	void SetComponentRotation(eHierarchyId compId, const Quaternion& q, bool bSetRotate=true, Vector3* pVecRotateAboutOffset=NULL, Vector3* pVecShiftPos=NULL);
	void SetBoneRotation(int nBoneIndex, eRotationAxis nAxis, float fAngle, bool bSetRotate=true, Vector3* pVecRotateAboutOffset=NULL, Vector3* pVecShiftPos=NULL);
	void SetBoneRotation(int nBoneIndex, const Quaternion& q, bool bSetRotate=true, Vector3* pVecRotateAboutOffset=NULL, Vector3* pVecShiftPos=NULL);
	void SetTransmissionRotation(eHierarchyId compId, eHierarchyId wheelIdL, eHierarchyId wheelIdR);
	void SetTransmissionRotationForTrike(eHierarchyId compId, eHierarchyId wheelIdL);
	void SetSuspensionTransformation(eHierarchyId compId, eHierarchyId wheelId, SuspensionType nType, const Vec3V *pDeformation = NULL);

	void UpdateLinkAttachmentMatricesRecursive( phBoundComposite* pBoundComp, fragTypeGroup* pGroup );

	// Extra component stuff (e.g hoods on convertibles, rubbish in veh, stuff on the back of trucks)
	void InitExtraFlags(u32 nExtraCommands);
	void InitFragmentExtras();
	void InitLightExtras();
	void InitAllExtras();
	void TurnOffExtra(eHierarchyId nId, bool bTurnOff);
	bool GetIsExtraOn(eHierarchyId nId) const;
	inline bool GetDoesExtraExist(eHierarchyId nId) const {return HasComponent(nId);}
	bool HasFragmentComponent(eHierarchyId hierarchyId) const;
	int GetFragmentComponentIndex(eHierarchyId hierarchyId) const;	// Returns a valid component index only if the component still exists
	void RemoveFragmentComponent(eHierarchyId hierarchyId);
    void UpdateExtras();

	bool CarHasRoof(bool bCheckForRoofDown = false) const;
	bool HasRaisedRoof() const;
	bool IsGangCar() const;
	bool IsPersonalVehicle();
	bool IsPersonalOperationsVehicle();
	int GetPersonalVehicleOwnerId();

	void DeployFoldingWings(bool bDeploy, bool bInstant);
	bool GetAreFoldingWingsDeployed() const;

	//Make sure if a remote driver is driving your vehicle that 
	//  he gets charged with the correct insurance amount. 
	CPed* GetVehicleWasSunkByAPlayerPed();

#if !__FINAL
	bool IsLocalPersonalVehicle();
	bool IsRemotePersonalVehicle();
#endif

	// the following function is only really valid for cars/bikes and is redefined there,
	// but the colbox limit should be approx ok for other vehicles
	virtual float GetHeightAboveRoad() const { return -1.0f * GetBaseModelInfo()->GetBoundingBoxMin().z; }

	const CBoardingPoints * GetBoardingPoints() const;
	bool GetClosestBoardingPoint(const Vector3 & vWorldSpaceInputPos, CBoardingPoint & boardingPointLocalSpace) const;

	// Sets the composite bounding box to the maximum extents possible due to wheel movement
	void CalculateNonArticulatedMaximumExtents(bool bDontSweepWheels = false);

	bool IsColliderArticulated() const;

	virtual inline bool IsPropeller(int UNUSED_PARAM(nComponent)) const {return false;}
	virtual inline bool ShouldKeepRotorInCenter(CPropeller *UNUSED_PARAM(pPropeller)) const {return false;}

    void RequestDials(const bool overriden = false);
	static bool DialsOverriddenByScript(void) {return ms_bDialsScriptOveridden;}
	static void ResetDialsOverriddenByScript(void) {ms_bDialsScriptOveridden = false;}

//////////////////////////////////////////////////////////////////////////
// Rendering and Fx// 

	CVehicleDrawHandler& GetVehicleDrawHandler() {return reinterpret_cast<CVehicleDrawHandler&>(GetDrawHandler());}
	const CVehicleDrawHandler& GetVehicleDrawHandler() const {return reinterpret_cast<CVehicleDrawHandler&>(GetDrawHandler());}

	// 16 levels allowed: 0.0f=full clean, 7.0f=medium dirty, 15.0f=full dirty, etc.
	inline void	 SetBodyDirtLevel(float d)	{ m_fBodyDirtLevel  = d;}
	inline float GetBodyDirtLevel() const	{ return m_fBodyDirtLevel; }
	inline void SetBodyDirtColor(Color32 c)	{ m_bodyDirtColor = c; }
	inline Color32 GetBodyDirtColor() const	{ return m_bodyDirtColor; }
	// returns u8 number 0-15:
	inline u8 GetBodyDirtLevelUInt8()		{ return static_cast<u8>( static_cast<u32>(GetBodyDirtLevel())&0x0F ); }
	void ProcessDirtLevel();

	// access to vehicle body colours:
	inline u8 GetBodyColour1() const	{ return GetVehicleDrawHandler().GetVariationInstance().GetColor1(); }
	inline u8 GetBodyColour2() const	{ return GetVehicleDrawHandler().GetVariationInstance().GetColor2(); }
	inline u8 GetBodyColour3() const	{ return GetVehicleDrawHandler().GetVariationInstance().GetColor3(); }
	inline u8 GetBodyColour4() const	{ return GetVehicleDrawHandler().GetVariationInstance().GetColor4(); }
	inline u8 GetBodyColour5() const	{ return GetVehicleDrawHandler().GetVariationInstance().GetColor5(); }
	inline u8 GetBodyColour6() const	{ return GetVehicleDrawHandler().GetVariationInstance().GetColor6(); }
	inline void	SetBodyColour1(u8 c)	{ GetVehicleDrawHandler().GetVariationInstance().SetColor1(c); }
	inline void	SetBodyColour2(u8 c)	{ GetVehicleDrawHandler().GetVariationInstance().SetColor2(c); }
	inline void	SetBodyColour3(u8 c)	{ GetVehicleDrawHandler().GetVariationInstance().SetColor3(c); }
	inline void	SetBodyColour4(u8 c)	{ GetVehicleDrawHandler().GetVariationInstance().SetColor4(c); }
	inline void	SetBodyColour5(u8 c)	{ GetVehicleDrawHandler().GetVariationInstance().SetColor5(c); }
	inline void	SetBodyColour6(u8 c)	{ GetVehicleDrawHandler().GetVariationInstance().SetColor6(c); }
	s32 GetLiveryId() const;
	void SetLiveryId(s32 id);
	void SetLiveryModFrag(strLocalIndex fragSlotIdx);
	s32 GetLivery2Id() const;
	void SetLivery2Id(s32 id2);
	void UpdateBodyColourRemapping(bool repainted = true);			// updates body colours with shaders

	// Access to neon colours / on/off flags
	inline Color32 GetNeonColour() const	{ return GetVariationInstance().GetNeonColour(); }
	inline void	SetNeonColour(Color32 c)	{ GetVariationInstance().SetNeonColour(c); }
	inline bool IsNeonLOn() const			{ return GetVariationInstance().IsNeonLOn(); }
	inline void SetNeonLOn(bool on)			{ GetVariationInstance().SetNeonLOn(on); }
	inline bool IsNeonROn() const			{ return GetVariationInstance().IsNeonROn(); }
	inline void SetNeonROn(bool on)			{ GetVariationInstance().SetNeonROn(on); }
	inline bool IsNeonFOn() const			{ return GetVariationInstance().IsNeonFOn(); }
	inline void SetNeonFOn(bool on)			{ GetVariationInstance().SetNeonFOn(on); }
	inline bool IsNeonBOn() const			{ return GetVariationInstance().IsNeonBOn(); }
	inline void SetNeonBOn(bool on)			{ GetVariationInstance().SetNeonBOn(on); }

	inline void	SetXenonLightColor(u8 c)	{ GetVehicleDrawHandler().GetVariationInstance().SetXenonLightColor(c); }
	inline u8 GetXenonLightColor() const	{ return GetVehicleDrawHandler().GetVariationInstance().GetXenonLightColor(); }

	bool IsAirExplosion(const Vector3& expPos);

	bool IsCamInsideVehicleInterior();

	// HD drawing
	static int	 ms_maxHdVehiclesInSp;
	static int   ms_maxHdVehiclesInMp;
	static float ms_afVehicleHdDistanceMP[CContinuityState::SCENE_STREAMING_PRESSURE_TOTAL];
	static float ms_afVehicleHdDistanceSP[CContinuityState::SCENE_STREAMING_PRESSURE_TOTAL];
	static void RetrieveAndCacheCloudTunables();

	void GetHDModeRequests(bool &requestDraw, bool & requestStream);	// Tells you whether we should draw based on distance
	virtual void	Update_HD_Models();	// handle requests for high detail models
	virtual inline bool GetIsCurrentlyHD() const { return (m_vehicleLodState == VLS_HD_AVAILABLE); }
	void SetVehicleLodState(eVehicleLodState state) { m_vehicleLodState = state; }
	eVehicleLodState GetVehicleLodState() { return(m_vehicleLodState); }

	void SetRespotCounter(u32 counter); // when veh gets re-spotted in a network game, want to disable collisions with other network vehicles 
	u16 GetRespotCounter() { return m_nNetworkRespotCounter; }
	bool IsBeingRespotted() const { return m_nNetworkRespotCounter > 0; }
	bool IsRunningSecondaryRespotCounter() const { return m_uSecondaryRespotOverrideTimeout > 0; }
	void ResetRespotCounter(bool bForceResetDamagedByAnything = true);

	bool ShouldPreRenderBeyondSmallRadius() const;

	void ResetSkeletonForMods();

	// Short-hand for checking if the jumping car flag is set
	bool HasJump();
	float GetJumpForceModifier();

	// Short-hand for checking if the rocket car flag is set
	bool HasRocketBoost();
	bool HasNitrousBoost();
	bool HasSideShunt() const;
	float GetNitrousPowerModifier();
	float GetNitrousDurationModifier();
	float GetNitrousRechargeModifier();
	bool IsRocketBoosting() const { return m_bIsRocketBoosting; }
	void SetRocketBoosting(bool bNewValue);
	void SetRocketBoostedFromNetwork() { m_bRocketBoostedFromNetwork = true; }
	void SetRocketBoostedStoppedFromNetwork()  { m_bRocketBoostedStoppedFromNetwork = true; }

	f32 GetRocketBoostRemaining() const { return m_fRocketBoostRemaining; }
	void SetRocketBoostRemaining(f32 f) { m_fRocketBoostRemaining = f; }
	f32 GetRocketBoostCapacity() const { return pHandling->m_fRocketBoostCapacity * ( InheritsFromPlane() ? ( 1.0f + (ms_fRocketBoostMaxIncrease * GetVariationInstance().GetGearboxModifier() * 0.01f ) ) : 1.0f ); }
	
	void SetRocketBoostRechargeRate(float fNewValue) { m_fRocketBoostRechargeRate = fNewValue; }

	float GetRocketBoostRechargeRate() { return m_fRocketBoostRechargeRate; }

	// Short-hand for checking if the parachute car flag is set
	bool HasParachute() { return GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_PARACHUTE); }
	bool HasGlider() const { return GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_GLIDER); }
	float GetGliderDeployedRatio() { return m_wingDeployedRatio; }
	float GetGliderNormaliseDeployedRatio() const;
	float GetSpecialFlightModeRatio() const;
	void ApplyParachuteStabilisation( Vector3& vStabilisationAxis, float fAngle, float fMaxTorque = 0.0f, float pitchStabilisationMult = sfPitchStabilisationMult, float rollStabilisationMult = sfRollStabilisationMult );
	void ApplyHoverModeStabilisation( Vector3& vDesiredUp, float normalisedDeployedRatio );
	void ApplyHoverModeThrust( CSpecialFlightHandlingData* flightHandling, Vector3 thrustDirection, float fTimeStep, float normalisedDeployedRatio, float forwardVelocity, float normalisedForwardVelocity, float fThrottle, float fStickY, float fStickX, bool strafeMode );
	int CalculateWheelHoverForce( CWheel* wheel, Vector3& wheelForce, Vector3& upVec, float normalisedForwardVelocity, float normalisedDeployedRatio, float fStickY, float invTimeStep, int sampleIndex, bool worksUpsideDown );
	void UpdateHoverModeBones();
	Vector3 CalculateGroundHeight( float rightOffset, float forwardOffset, float probeLength );
	Vector3 CalculateDesiredUp( float normalisedDeployedRatio, float normalisedForwardVelocity, float fStickX, float fStickY );
	void ApplyLowSpeedMovement( float normalisedDeployedRatio, float normalisedVelocity, bool anchorPosition );

	virtual CTransmission* GetSecondTransmission() { return NULL; }

	bool IsPlaceOnRoadHeightOverriden() { return m_fOverridenPlaceOnRoadHeight != sm_fInvalidPlaceOnRoadHeight; }
	float GetOverridenPlaceOnRoadHeight() { return m_fOverridenPlaceOnRoadHeight; }
	void SetOverridenPlaceOnRoadHeight(float fHeight) { m_fOverridenPlaceOnRoadHeight = fHeight; }
	void ResetOverridenPlaceOnRoadHeight() { m_fOverridenPlaceOnRoadHeight = sm_fInvalidPlaceOnRoadHeight; }

	bool GetHomingCanLockOnToObjects() const { return m_bHomingCanLockOnToObjects; }
	void SetHomingCanLockOnToObjects(bool bValue)  { m_bHomingCanLockOnToObjects = bValue; }

	void SetScriptMaxSpeed( float maxSpeed ) { m_scriptMaxSpeed = maxSpeed; }
	float GetScriptMaxSpeed() { return m_scriptMaxSpeed; }

	bool IsShuntModifierActive() const { return m_shuntModifierActive; }

//////////////////////////////////////////////////////////////////////////
// Lights
public:
	enum eHeadlightCoverState
	{
		LIGHTCOVER_CLOSED = 0,
		LIGHTCOVER_OPEN,
		LIGHTCOVER_OPENING,
		LIGHTCOVER_CLOSING,		
	};

	eHeadlightCoverState GetHeadlightCoverState() { return m_lightCoverState; }

protected:
	eHeadlightCoverState m_lightCoverState;
	float m_lightCoverOpenRatio;

	void ProcessLightCovers();


	bool ShouldLightsBeOn() const;
	void UpdateLightsOnFlag();

public:
	bool GetVehicleLightsStatus();
	bool UpdateVehicleLightOverrideStatus();
	bool GetFullBeamStatus();

	void CalculateInitialLightStateBasedOnClock();
	void UpdateLightRandomisation();
	void DoVehicleLights(u32 nLightFlags, Vec4V_In ambientVolumeParams = Vec4V(V_ZERO_WONE));
	void ProcessIndicators(bool &bTurningLeft, bool &bTurningRight, bool bForcedOn);

	void DoAdditionalLightsEffect(s32 lightId, float fade, const ConfigLightSettings* lightConfig, fwInteriorLocation interiorLoc, bool restrictRadius, CoronaAndLightList& coronaAndLightList);
	void DoVehicleLightsAsync(const Matrix34 &matVehicle, u32 nLightFlags, Vec4V_In ambientVolumeParams, const fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);

#if ENABLE_FRAG_OPTIMIZATION
	void DoHeadLightsEffect(bool leftOn, bool rightOn, s32 boneIdLeft, s32 boneIdRight, float fade, float daynightFade, float coronaFade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool bExtraBright, bool noLight, const vehicleLightSettings *settings, float intensityMultiplierHeadLeft, float intensityMultiplierHeadRight, fwInteriorLocation interiorLoc, bool useVolumetricLights, bool forceLowResVolumetricLights, bool reduceHeadlightCoronas, CoronaAndLightList& coronaAndLightList );
	void DoExtraLightsEffect(s32 boneId1, s32 boneId2, s32 boneId3, s32 boneId4, bool On1, bool On2, bool On3, bool On4,bool doLight, float fade, float daynightFade, float coronaFade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, const vehicleLightSettings *settings, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList );
	void DoIndicatorLightEffect(s32 boneId, float fade, float daynightFade, float coronaFade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, const vehicleLightSettings *settings, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
	void DoTailLightsEffect(s32 boneIdLeft, s32 boneIdMiddle, s32 boneIdRight, bool leftOn, bool middleOn, bool rightOn, float fade, float daynightFade, float coronaFade, bool noCorona, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool isBrakes, const vehicleLightSettings *settings, float intensityMultiplierTailLeft, float intensityMultiplierTailRight, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);	
	void DoReversingLightsEffect(s32 boneIdLeft, s32 boneIdRight, bool leftOn, bool rightOn, float fade, float daynightFade, float coronaFade, bool noCorona, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, const vehicleLightSettings *settings, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
	void DoInteriorLightEffect(s32 boneId, float fade, CVehicleModelInfo* pModelInfo, fwInteriorLocation interiorLoc, bool isHeli = false);
	void DoInteriorLightEffect(s32 boneId, float fade, CVehicleModelInfo* pModelInfo, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList, bool isHeli = false);
	void DoTaxiLightEffect(float fade, float daynightFade, CVehicleModelInfo* pModelInfo, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
	void DoNeonLightEffect(u32 neonFlags, float fade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, fwInteriorLocation interiorLocation, CoronaAndLightList& coronaAndLightList);
	void DoSirenLightsEffect(float fade, float daynightFade, float coronaFade, bool noActualLights, const CVehicleModelInfo* const pModelInfo, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
#else
	void DoHeadLightsEffect(bool leftOn, bool rightOn, s32 boneIdLeft, s32 boneIdRight, float fade, float daynightFade, float coronaFade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool bExtraBright, bool noLight, const vehicleLightSettings *settings, float intensityMultiplierHeadLeft, float intensityMultiplierHeadRight, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, bool useVolumetricLights, bool forceLowResVolumetricLights, bool reduceHeadlightCoronas, CoronaAndLightList& coronaAndLightList );
	void DoExtraLightsEffect(s32 boneId1, s32 boneId2, s32 boneId3, s32 boneId4, bool On1, bool On2, bool On3, bool On4,bool doLight, float fade, float daynightFade, float coronaFade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, const vehicleLightSettings *settings, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList );
	void DoIndicatorLightEffect(s32 boneId, float fade, float daynightFade, float coronaFade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, const vehicleLightSettings *settings, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
	void DoTailLightsEffect(s32 boneIdLeft, s32 boneIdMiddle, s32 boneIdRight, bool leftOn, bool middleOn, bool rightOn, float fade, float daynightFade, float coronaFade, bool noCorona, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool isBrakes, const vehicleLightSettings *settings, float intensityMultiplierTailLeft, float intensityMultiplierTailRight, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
	void DoReversingLightsEffect(s32 boneIdLeft, s32 boneIdRight, bool leftOn, bool rightOn, float fade, float daynightFade, float coronaFade, bool noCorona, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, const vehicleLightSettings *settings, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
	void DoInteriorLightEffect(s32 boneId, float fade, CVehicleModelInfo* pModelInfo, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, bool isHeli = false);
	void DoInteriorLightEffect(s32 boneId, float fade, CVehicleModelInfo* pModelInfo, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList, bool isHeli = false);
	void DoTaxiLightEffect(float fade, float daynightFade, CVehicleModelInfo* pModelInfo, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
	void DoNeonLightEffect(u32 neonFlags, float fade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, crSkeleton *pSkeleton, fwInteriorLocation interiorLocation, CoronaAndLightList& coronaAndLightList);
	void DoSirenLightsEffect(float fade, float daynightFade, float coronaFade, bool noActualLights, const CVehicleModelInfo* const pModelInfo, crSkeleton *pSkeleton, fwInteriorLocation interiorLoc, CoronaAndLightList& coronaAndLightList);
#endif

	static inline float CalculateVehicleDaylightFade()
	{
		// One bit per hour, we wants veh lights from 19 till 6
		// 0000000000111111111122222
		// 0123456789012345678901234
		//const int vehTimeFlags = 0b111110000000000000111111; since only SN/gcc support this notation, let's have an hex value instead.
		const int vehTimeFlags = 0xF8003F;

		return Lights::CalculateTimeFade(vehTimeFlags);

	}
	void DoVehicleLightsNoLights();

	void SetHazardLights(bool set) { m_nVehicleFlags.bHazardLights = set; }
	void SetIndicatorLights(bool set) { m_nVehicleFlags.bIndicatorLights = set; }
	void SetInteriorLight(bool set) { m_nVehicleFlags.bInteriorLightOn = set; }
	void SetForceInteriorLight(bool force) { m_bInteriorLightForceOn = force; }

	void TurnSirenOn(bool on, bool randomDelay = true);

//////////////////////////////////////////////////////////////////////////
// Damage
	inline CVehicleDamage* GetVehicleDamage() {return &m_VehicleDamage;}
	inline const CVehicleDamage* GetVehicleDamage() const {return &m_VehicleDamage;}

	bool HasInfiniteFuel() const;
	float GetFuelConsumptionRate() const;

	virtual void BlowUpCar(CEntity* UNUSED_PARAM(pCulprit), bool UNUSED_PARAM(bInACutscene)=false, bool UNUSED_PARAM(bAddExplosion)=true, bool UNUSED_PARAM(bNetCall)=false, u32 UNUSED_PARAM(weaponHash) = WEAPONTYPE_EXPLOSION, bool UNUSED_PARAM(bDelayedExplosion) = false);
	virtual void AddVehicleExplosion(CEntity *pCulprit, bool bInACutscene, bool bDelayedExplosion);
	virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount) { extraBoneCount = 0; return NULL; }
	void MakeRealOrFixForExplosion();

	virtual void Fix(bool resetFrag = true, bool allowNetwork = false);
	virtual void SetupDamageAfterLoad() {;}
	virtual void ApplyDeformationToBones(const void* UNUSED_PARAM(basePtr)) {;}
	void FixVehicleDeformation() { m_VehicleDamage.FixVehicleDeformation(); }
	
	bool IsUpsideDown() const;
	bool IsOnItsSide() const;
	bool IsRightWayUp() const;

	void SetCanBeDamaged(bool DamageFlag) {	m_nPhysicalFlags.bNotDamagedByAnything = !DamageFlag; }
	void ExtinguishCarFire(bool finishPtFx=false, bool bClampVehicleHealth = true);
	void ClearCulpritFromCarFires(const CEntity* pCulprit);

    bool CanLeakPetrol() const;
	bool HasScriptForcedLeakingOff() const;
	bool HasScriptForcedLeakingOn() const;
    bool CanLeakOil() const;
	bool IsLeakingPetrol() const;
	bool IsLeakingOil() const;
	bool CanEngineDegrade() const;
	bool IsOnFire() const;

	bool AreTyresDamaged() const;
	bool IsSuspensionDamaged() const;

	bool IsLeftHeadLightDamaged() const { return m_VehicleDamage.GetLightState(VEH_HEADLIGHT_L); }
	bool IsRightHeadLightDamaged() const { return m_VehicleDamage.GetLightState(VEH_HEADLIGHT_R); }
	bool AreBothHeadLightsDamaged() const { return IsLeftHeadLightDamaged() && IsRightHeadLightDamaged(); }
	
	int GetNumberOfDamagedHeadlights() const 
	{ 
		int count = IsLeftHeadLightDamaged() ? 1 : 0;
		
		if (IsRightHeadLightDamaged())
		{
			++count;
		}
		return count;
	}
	
	u8 GetNumFlatTires() const;
	virtual void NotifyWheelPopped(CWheel *UNUSED_PARAM(pWheel)) {}
	void FlagWheelsWithDeformationImpact(Vec3V_In damageOffset, ScalarV_In damageRadius);

	Vec3V_Out GetWindowNormalForSmash(eHierarchyId windowId) const;
	void SmashWindow(eHierarchyId iWindowHierarchy, float force, bool bIgnoreCracks = false, bool bDetachAll = false);
	void FixWindow(eHierarchyId iWindowHierarchy);
	bool HasWindow(eHierarchyId iWindowHierarchy) const;
	bool HasWindowToSmash(eHierarchyId iWindowHierarchy, bool bIgnoreCracks = false) const;
	bool HasAnyWindowsToSmash() const;
	void DeleteWindow(eHierarchyId iWindowHierarchy);
	void SetWindowComponentDirty(eHierarchyId id ) {Assert(id >= VEH_FIRST_WINDOW && id <= VEH_LAST_WINDOW);m_iWindowComponent[id - VEH_FIRST_WINDOW] = VEH_WINDOW_STATE_DIRTY;}
	void SetAllWindowComponentsDirty(){	for (u32 i=VEH_FIRST_WINDOW; i<=VEH_LAST_WINDOW; ++i) m_iWindowComponent[i - VEH_FIRST_WINDOW] = VEH_WINDOW_STATE_DIRTY;}
	void CacheSmashableWindows();

	static eHierarchyId GetWindowIdFromDoor(eHierarchyId doorId);
	static eHierarchyId GetDoorIdFromWindow(eHierarchyId windowId);

	bool IsWindowDown(eHierarchyId eWindowId) const;
	void RolldownWindows();
	void RolldownWindow(eHierarchyId iWindowHierarchy, bool bCalledFromPreRender=false);
	void RollupWindows(bool bSkeletonReset = true);
	void RollWindowUp(eHierarchyId iWindowHierarchy);

	bool IsAlarmActivated() const {return (CarAlarmState && CarAlarmState!=CAR_ALARM_SET && GetStatus()!=STATUS_WRECKED);};
	void SetCarAlarm(bool set) { if(set) CarAlarmState = CAR_ALARM_SET; else CarAlarmState = CAR_ALARM_NONE;}
#if GTA_REPLAY
	bool IsReplayOverrideWheelsRot() const			{ return m_replayOverrideWheelsRot; }
	void SetIsReplayOverrideWheelsRot(bool overRot)	{ m_replayOverrideWheelsRot = overRot;}
	void SetCarAlarmState(u16 state)	{	CarAlarmState = state;	}

	void IncreaseNumAppliedVehicleDamage(void)	{ m_appliedVehicleDamage++; }
	u8   GetNumAppliedVehicleDamage(void)		{ return m_appliedVehicleDamage; }
	void SetNumAppliedVehicleDamage(const u8 s)	{ m_appliedVehicleDamage = s; }
	void ResetNumAppliedVehicleDamage(void)		{ m_appliedVehicleDamage=0; }
#endif // GTA_REPLAY
	void ProcessCarAlarm(const float fFullUpdateTimeStep);
	bool TriggerCarAlarm();

	void SetIsWrecked();
	virtual void SetIsAbandoned(const CPed *pOriginalDriver = NULL);
	virtual void SetIsOutOfControl();
	void SetIsPlayerDisabled();

	bool TestExplosionPosAndBlowRearDoorsopen(const Vector3& vExplosionPos );

	struct DestructionInfo
	{
		void Clear()
		{
			m_Source = NULL;
			m_Weapon = 0;
			m_Time = 0;
		}

		RegdEnt	m_Source;
		u32		m_Weapon;
		u32		m_Time;
	};

	void ClearDestructionInfo() { m_DestructionInfo.Clear(); }

	CEntity* GetSourceOfDestruction() const { return m_DestructionInfo.m_Source; }
	void SetSourceOfDestruction(CEntity* pSource) { m_DestructionInfo.m_Source = pSource; }

	u32 GetCauseOfDestruction() const { return m_DestructionInfo.m_Weapon; }
	void SetCauseOfDestruction(const u32 uWeapon) { m_DestructionInfo.m_Weapon = uWeapon; }

	u32 GetTimeOfDestruction() const { return m_DestructionInfo.m_Time; }
	void SetTimeOfDestruction()	{ SetTimeOfDestruction(fwTimer::GetTimeInMilliseconds()); }
	void SetTimeOfDestruction(const u32 uTime) { m_DestructionInfo.m_Time = uTime; }
	
	void SetDestructionInfo(CEntity* pSource, const u32 uWeapon);

	bool CalculateTimeUntilExplosion(float& fTime) const;

	//These are the damaged bounds (if damaged), otherwise the VEH_CHASSIS bounds or overall vehicle bounds
	Vector3 GetChassisBoundMin(bool includeDamage = false) const;
	Vector3 GetChassisBoundMax(bool includeDamage = false) const;
	
//////////////////////////////////////////////////////////////////////////
// Audio
	audVehicleAudioEntity* GetVehicleAudioEntity() { return(m_VehicleAudioEntity); }
	const audVehicleAudioEntity* GetVehicleAudioEntity() const { return(m_VehicleAudioEntity); }
	audPlaceableTracker *GetPlaceableTracker() { return &m_PlaceableTracker; }
	const audPlaceableTracker *GetPlaceableTracker() const { return &m_PlaceableTracker; }

	void ProcessSirenAndHorn(bool bHornAvailable);
	void StopHorn();
	bool IsHornOn() const;
	
	// Functions having to do with the engine temperature and starting
	bool IsEngineOn() const { return m_nVehicleFlags.bEngineOn; }
	void SwitchEngineOn(bool bNoDelay = false, bool bNetCheck = true, bool forceFail = false);
	void SwitchEngineOff(bool bNetCheck = true);
	void UpdateEngineTemperature(float fTimeStep);
	void SetRandomEngineTemperature(float value = fwRandom::GetRandomNumberInRange(0.0f, 1.0f));

	float ComputeEngineWontStartProbability();

	//Check if this vehicle can add an event for peds to comment on it.
	void AddCommentOnVehicleEvent();

//////////////////////////////////////////////////////////////////////////
// Vehicle Gadgets (weapons etc)
	const CVehicleWeaponMgr* GetVehicleWeaponMgr() const { return m_pVehicleWeaponMgr; }
	CVehicleWeaponMgr* GetVehicleWeaponMgr() { return m_pVehicleWeaponMgr; }
	void CreateVehicleWeaponMgr();
	void DestroyVehicleWeaponWhenInactive() { m_destroyWeaponMgr = true; };
	void DestroyVehicleWeaponMgr();
	void RecreateVehicleWeaponMgr() { DestroyVehicleWeaponMgr(); CreateVehicleWeaponMgr(); }
	void SetupWeaponModCollision();

	void InitialiseSeatCollision( phBoundComposite* pBoundComp );

	//Function to see if a specific vehicle seat has any turrets or weapons associated with it
	bool GetSeatHasWeaponsOrTurrets(s32 seatIndex) const;

	void AddVehicleGadget(CVehicleGadget* pGadget);
	CVehicleGadget* GetVehicleGadget(int iGadgetIndex) const;
	int GetNumberOfVehicleGadgets() const {return m_pVehicleGadgets.size();}
	int GetGadgetEntities(int entityListSize, const CEntity ** entityListOut) const;

	void UpdateUserEmissiveMultiplier(bool bNewEngineOnValue, bool bOldEngineOnValue);

	// Allow script to set an ammo limit for a particular vehicle weapon index
	void SetRestrictedAmmoCount(s32 iWeaponIndex, s32 iAmmoCount) {vehicleAssert(iWeaponIndex >= 0 && iWeaponIndex < MAX_NUM_VEHICLE_WEAPONS); m_iRestrictedAmmoCount[iWeaponIndex] = iAmmoCount;}
	s32 GetRestrictedAmmoCount(s32 iWeaponIndex) const {vehicleAssert(iWeaponIndex >= 0 && iWeaponIndex < MAX_NUM_VEHICLE_WEAPONS); return m_iRestrictedAmmoCount[iWeaponIndex];}
	void ReduceRestrictedAmmoCount(s32 iWeaponIndex) {vehicleAssert(iWeaponIndex >= 0 && iWeaponIndex < MAX_NUM_VEHICLE_WEAPONS); m_iRestrictedAmmoCount[iWeaponIndex]--;}

	// Network-synced ammo counts for bombs and countermeasures (can't be tied to a specific player)
	void SetBombAmmoCount(s32 iAmmoCount) { m_iBombAmmoCount = iAmmoCount; }
	s32	 GetBombAmmoCount() const { return m_iBombAmmoCount; }
	void SetCountermeasureAmmoCount(s32 iAmmoCount) { m_iCountermeasureAmmoCount = iAmmoCount; }
	s32	 GetCountermeasureAmmoCount() const { return m_iCountermeasureAmmoCount; }

//////////////////////////////////////////////////////////////////////////
// Script
	virtual void SetupMissionState();
	virtual void CleanupMissionState();

//////////////////////////////////////////////////////////////////////////
// Misc
	inline bool IsRunningCarRecording() const { return m_nVehicleFlags.bIsInRecording; }
	bool CanBeInactiveDuringRecording() const;
	inline bool IsInactiveForRecording() const { return m_nVehicleFlags.bIsInRecording && m_nVehicleFlags.bIsDeactivatedByPlayback; }

	virtual bool TreatAsOnFootForGarages() const {return false;} // bmx will be treated as on foot in the garage code for some reason?

	void DisableWheelCollisions(bool bDisableCameraCollision = false);
	void EnableWheelCollisions();
	void SetupWheelCollisions(phBoundComposite * pVehCompositeBound);

	void FreezeAllTurretJoints( gtaFragType* type, bool freeze );
    void FreezeAllJoints( gtaFragType* type );
	void Freeze1DofJointInCurrentPosition(fragInstGta* pFragInst, int iChildIndex);
	void Freeze1DofJointInCurrentPosition(fragInstGta* pFragInst, int typeComponentIndex, bool freeze);
	// just have an empty function so it can be called from vehicle pointers
	virtual void PlayCarHorn(bool UNUSED_PARAM(bOneShot), u32 UNUSED_PARAM( hornTypeHash )= 0 ) {};

	bool PlaceOnRoadAdjust(bool bJustSetCompression = false, float hightSampleRangeUp = PLACEONROAD_DEFAULTHEIGHTUP ) {return PlaceOnRoadAdjust(hightSampleRangeUp,PLACEONROAD_DEFAULTHEIGHTDOWN, bJustSetCompression);}
	bool PlaceOnRoadAdjust(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression = false);

	//bool			IsInTunnel() const;

	inline TDynamicObjectIndex GetPathServerDynamicObjectIndex() const { return m_iPathServerDynObjId; }
	inline void SetPathServerDynamicObjectIndex(const TDynamicObjectIndex i) { m_iPathServerDynObjId = i; }

	// Targeting
	virtual float GetLockOnTargetableDistance() const { return m_fTargetableDistance; }
	virtual void SetLockOnTargetableDistance(float fTargetableDistance) { m_fTargetableDistance = fTargetableDistance; }
	virtual void SetHomingLockOnState(CEntity::eHomingLockOnState state);
	CEntity::eHomingLockOnState GetHomingLockOnState() const     { return m_eHomingLockOnState; }
    CEntity::eHomingLockOnState GetHomingLockedOntoState() const { return m_eHomingLockedOntoState; }
	virtual void SetHomingProjectileDistance( float fHomingProjectileDistance );
	float GetHomingProjectileDistance() const { return m_fHomingProjectileDistance; }
    void SetLockOnTarget(CPhysical *pTarget) { m_pLockOnTarget = pTarget; }
    CPhysical *GetLockOnTarget() const { return m_pLockOnTarget; }
    void UpdateHomingLockedOntoState(CEntity::eHomingLockOnState state);
	void UpdateLockOnStatus();
	void SetLastTimeHomedAt(u32 time) { m_uLastTimeHomedAt = time; }
	u32 GetLastTimeHomedAt() const { return m_uLastTimeHomedAt; }

	static const u32 MAX_NUM_EXCLUSIVE_DRIVERS = 2;
	void SetExclusiveDriverPed(CPed *pPed, s32 i=0);
	CPed * GetExclusiveDriverPed(s32 i=0) const { Assert(i<MAX_NUM_EXCLUSIVE_DRIVERS); return m_pExclusiveDriverPeds[i]; }
	bool IsAnExclusiveDriverPedOrOpenSeat(const CPed* ped ) const;
	bool IsAnExclusiveDriverPed(const CPed* ped) const;
	bool HasAnExclusiveDriver() const;

	bool HasExpandedMods();

    float GetBrakeForce( bool includeModifier = false ) const;

	// Event Generation Helpers
	bool ShouldAlertNearbyWhenVehicleIsEntered() const			{ return m_nVehicleFlags.bAlertWhenEntered; }
	void SetShouldAlertNearbyWhenVehicleIsEntered(bool bSet)	{ m_nVehicleFlags.bAlertWhenEntered = bSet; }

	bool ShouldGenerateEngineShockingEvents() const					{ return m_nVehicleFlags.bGeneratesEngineShockingEvents; }
	void SetShouldGenerateEngineShockingEvents(bool bSet)			{ m_nVehicleFlags.bGeneratesEngineShockingEvents = bSet; }

	const Vector3& GetInternalForce() { return m_vecInternalForce; }
	const Vector3& GetInternalTorque() { return m_vecInternalTorque; }

#if !__FINAL
	void SetDebugName(const char* strName);
	virtual const char* GetDebugName() const;
#endif

	void SetNeonSuppressionState(bool val)	{ m_bSuppressNeons = val; }
	bool GetNeonSuppressionState() const	{ return m_bSuppressNeons; }

//////////////////////////////////////////////////////////////////////////
// Attachments

	// Returns the physical attachment parent if it is a vehicle.
	CVehicle* GetAttachParentVehicle() const;

	// Returns the dummy attached parent or physical attachment parent if it is a vehicle
	CVehicle* GetAttachParentVehicleDummyOrPhysical() const;

	// Trailers
	bool HasTrailer() const { return (GetAttachedTrailer() != NULL) || HasDummyAttachmentChildren(); }
	CTrailer* GetAttachedTrailer() const;
	CTrailer* GetAttachedTrailerOrDummyTrailer() const;
	CTrailer* GetParentTrailer() const { return m_parentTrailer; }
	void SetParentTrailer(CTrailer* trailer) { m_parentTrailer = trailer; }
	static bool IsEntityAttachedToTrailer(const CEntity* pEntity);

	void SetIsCargoVehicle(bool isCargoVehicle);

	float GetParentTrailerAttachStrength() const {return m_fParentTrailerAttachStrength;}
	Vector3::Return GetParentTrailerEntity1Offset() const {return m_vParentTrailerEntity1Offset;}
	Vector3::Return GetParentTrailerEntity2Offset() const {return m_vParentTrailerEntity2Offset;}
	Quaternion GetParentTrailerEntityRotation() const {return m_qParentTrailerEntityRotation;}
	
	void SetParentTrailerAttachStrength(float fStrength) {m_fParentTrailerAttachStrength = fStrength;}
	void SetParentTrailerEntity1Offset(const Vector3& vOffset) {m_vParentTrailerEntity1Offset = vOffset;}
	void SetParentTrailerEntity2Offset(const Vector3& vOffset) {m_vParentTrailerEntity2Offset = vOffset;}
	void SetParentTrailerEntityRotation(const Quaternion& qRot) {m_qParentTrailerEntityRotation = qRot;}

	// Dummy attachment accessors
	CVehicle * GetDummyAttachmentParent() const { return m_DummyAttachmentParent; }
	CVehicle* GetDummyAttachmentChild(const int index) const { return m_DummyAttachmentChildren[index]; }
	int GetMaxNumDummyAttachmentChildren() const {return m_DummyAttachmentChildren.GetMaxCount();}
	bool HasDummyAttachmentChildren() const { return m_nDummyAttachmentChildren>0; }
	int FindDummyAttachmentChild(const CVehicle* const pVehicle);

	// Dummy attachment modifiers
	int DummyAttachChild(CVehicle & vehChild);
	void DummyDetachChild(const CVehicle * pChildVehicle);
	void DummyDetachChild(int iChildSlot);
	void DummyDetachAllChildren();
	void InitDummyAttachmentChildren();

	//Tow truck
	const CEntity* GetEntityBeingTowed() const;
	void DetachFromParentVehicleGadget();
	bool GetIsBeingTowed() const;
	bool GetIsTowing() const;
	bool HasTowingGadget() const;

	//Submarine / Submarine car
	virtual CSubmarineHandling* GetSubHandling() { return NULL; }
    virtual const CSubmarineHandling* GetSubHandling() const  { return NULL; }
	virtual CBoatHandling*		GetBoatHandling() { return NULL; }
	virtual const CBoatHandling*	GetBoatHandling() const { return NULL; }

//////////////////////////////////////////////////////////////////////////
// Special ability code
    
	void ProcessSlowMotionSpecialAbilityPrePhysics(const CPed *pDriver);
    void ProcessSlowMotionSpecialAbilityPostPhysics();

	void ProcessSlowMotionVehiclePrePhysics(const float fFxStrength);

	void SetFullThrottleTime(u32 iDurationMs) { m_iFullThrottleRecoveryTime = NetworkInterface::GetNetworkTime() + iDurationMs; }
	bool IsFullThrottleActive() { return m_bFullThrottleActive; }
    static float ms_fBrakeVarianceMaxModifier;
    static float ms_fEngineVarianceMaxModifier;
	static float ms_fThrustDropOffVarianceMaxModifier;
    static float ms_fSuspensionVarianceMaxModifier;
	static float ms_fGearboxClutchSpeedVarianceMaxModifier;
	static float ms_fHydraulicVarianceUpperBoundMaxModifier;
	static float ms_fHydraulicVarianceLowerBoundMaxModifier;
	static float ms_fHydraulicVarianceUpperBoundMinModifier;
	static float ms_fHydraulicVarianceLowerBoundMinModifier;
	static float ms_fHydraulicRateMax;
	static float ms_fHydraulicRateMin;
	static float ms_fPlaneHandlingVarianceMaxModifier;
	static float ms_fRocketBoostMaxIncrease;

	void SetIsInPopulationCache(bool val) { m_nVehicleFlags.bIsInPopulationCache = val; }
	bool GetIsInPopulationCache() const { return m_nVehicleFlags.bIsInPopulationCache; }

	bool GetHavePartsBrokenOff() const { return m_nVehicleFlags.bPartsBrokenOff; }
	void SetHavePartsBrokenOff(bool val) { m_nVehicleFlags.bPartsBrokenOff = val; }

	bool GetHasBeenRepainted() const { return m_nVehicleFlags.bHasBeenRepainted; }
	void SetHasBeenRepainted(bool val) { m_nVehicleFlags.bHasBeenRepainted = val; }

	float GetLodMultiplier() const { return m_fLodMult; }
	void SetLodMultiplier(float multiplier) { m_fLodMult = multiplier; }

    s8 GetClampedRenderLod() const { return m_clampedRenderLod; }
    void SetClampedRenderLod(s8 lod) { m_clampedRenderLod = lod; }

	bool IsForcedHd() const { return m_nVehicleFlags.bForceHd; }
	void SetForceHd(bool val) { m_nVehicleFlags.bForceHd = val; }
	void SetScriptForceHd(bool val) { m_nVehicleFlags.bScriptForceHd = val; }
	bool GetScriptForceHd()			{ return m_nVehicleFlags.bScriptForceHd; }

	bool CanSaveInGarage() const { return m_nVehicleFlags.bCanSaveInGarage; }
	void SetCanSaveInGarage(bool val) { m_nVehicleFlags.bCanSaveInGarage = val; }

	void SetAnimateWheels(bool val) { m_nVehicleFlags.bAnimateWheels = val; }
	bool GetAnimateWheels() const { return m_nVehicleFlags.bAnimateWheels; }

	void SetAnimatePropellers(bool val) { m_nVehicleFlags.bAnimatePropellers = val; }
	bool GetAnimatePropellers() const { return m_nVehicleFlags.bAnimatePropellers; }

	void SetClearWaitingOnCollisionOnPlayerEnter( bool val ) { m_bClearWaitingOnCollisionOnPlayerEnter = val; }
	bool GetClearWaitingOnCollisionOnPlayerEnter() { return m_bClearWaitingOnCollisionOnPlayerEnter; }

protected:
	// Used in wheel setup to calculate the offset of iCurrentWheel relative to iFrontId and iRearId
	// Returns a value between -1.0f and 1.0f
	// 1.0f = front of car
	// -1.0f = rear of car
	// 0.0f = exact centre
	float CalcFrontRearSelector(eHierarchyId iCurrentWheel, eHierarchyId iFrontId, eHierarchyId iRearId);

	SuspensionType GetSuspensionType(bool bRearSuspension) const;

	virtual bool PlaceOnRoadAdjustInternal(float UNUSED_PARAM(HeightSampleRangeUp), float UNUSED_PARAM(HeightSampleRangeDown), bool UNUSED_PARAM(bJustSetCompression)) { return false; }

	virtual u32 GetMainSceneUpdateFlag() const;
	virtual u32 GetStartAnimSceneUpdateFlag() const;

    // Used to toggle wheel contact on and off
    void SetEnableWheelContacts(bool enableContacts);

	// Finding wheels by ID
	void InitCacheWheelsById();
	void ClearCacheWheelsById();
	CWheel* GetWheelFromIdFromArray(eHierarchyId nId);
	const CWheel* GetWheelFromIdFromArray(eHierarchyId nId) const;

	bool GetHasLandingGear();

#if __WIN32PC
	virtual void ProcessForceFeedback();
#endif // __WIN32PC

	void ProcessVehicleCollision(CVehicle* pOtherVehicle);

//////////////////////////////////////////////////////////////////////////
// Seat interface
public:
	const CSeatManager* GetSeatManager() const {return &m_SeatManager;}

	// Quick accessors to the seat animation info
	// Because they are used all over
	const CVehicleSeatAnimInfo* GetSeatAnimationInfo(const CPed* pPed) const;
	const CVehicleSeatAnimInfo* GetSeatAnimationInfo(s32 iSeatIndex) const;	

	const CVehicleSeatInfo* GetSeatInfo(const CPed* pPed) const;
	const CVehicleSeatInfo* GetSeatInfo(s32 iSeatIndex) const;

	const CVehicleEntryPointAnimInfo* GetEntryAnimInfo(s32 iEntryPointIndex) const;
	const CVehicleEntryPointInfo* GetEntryInfo(s32 iEntryPointIndex) const;

	const CVehicleLayoutInfo* GetLayoutInfo() const;
	const CPOVTuningInfo* GetPOVTuningInfo() const;
	const CVehicleCoverBoundOffsetInfo* GetVehicleCoverBoundOffsetInfo() const;

	void ResetSeatManagerForReuse();
	void ResetExclusiveDriverForReuse();

	bool IsSeatIndexValid(s32 iSeatIndex) const { return iSeatIndex >= 0 && iSeatIndex < m_SeatManager.GetMaxSeats(); }
	bool IsEntryIndexValid(s32 iEntryIndex) const;

	// Do we have at least one entry we can use to get to this seat
	bool CanSeatBeAccessedViaAnEntryPoint(s32 iSeatIndex) const;
	CSeatAccessor::SeatAccessType GetSeatAccessType(s32 iSeatIndex) const;

	bool IsTurretSeat(s32 iSeatIndex) const;
	s32 GetFirstTurretSeat() const;
	s32 GetDriverSeat() const;
	__forceinline CPed* GetDriver() const { return m_SeatManager.GetDriver(); }
	CPed* GetLastDriver() const;

	void SetLastDriverFromNetwork(CPed *pPed) { m_SeatManager.SetLastDriverFromNetwork(pPed); }

	// Add ped in the given seat
	bool AddPedInSeat(CPed *pPed, s32 iSeat, bool bTestConversionCollision = true, bool bShouldApplyForce = true);
	// Remove the ped from the seat
	void RemovePedFromSeat(CPed *pPed, bool bChangeCarStatus = true, bool bDestroyWeaponMgr = true);	
	// Notify that a ped in this vehicle has changed into / from a player
	void ChangePedsPlayerStatus(const CPed &pPed, bool bBecomingPlayer) { m_SeatManager.ChangePedsPlayerStatus(pPed,bBecomingPlayer); }

	s32 GetDirectEntryPointIndexForPed(const CPed* pPed) const;
	s32 GetDirectEntryPointIndexForSeat(s32 iSeat) const;
	s32 GetIndirectEntryPointIndexForSeat(s32 iSeat) const;
	s32 ChooseAppropriateJumpOutDirectEntryPointIndexForTurretDirection(s32 iSeat) const;
	s32 ChooseAppropriateEntryPointForOnBoardJack(const CPed& rPed, s32 iSeat) const;
	s32 FindSeatIndexForFirstHatchEntryPoint() const;

	// For allowing scripts to control whether peds should fall off bikes / quads after a hard fall.
	void SetAllowKnockOffVehicleForLargeVertVel(bool bValue) { m_nVehicleFlags.bAllowKnockOffVehicleForLargeVertVel = bValue; }
	void SetVelocityThresholdBeforeKnockOffVehicle(float fValue) { m_fVerticalVelocityToKnockOffThisBike = fValue; }
	float GetVelocityThresholdBeforeKnockOffVehicle() const { return m_fVerticalVelocityToKnockOffThisBike; }

	const Vector3 & GetSuperDummyVelocity() const { return m_vSuperDummyVelocity; }
	void SetSuperDummyVelocity(const Vector3& vVelocity);
	void SetVelocityFromSuperDummyVelocity();

	void RestoreVelocityFromInactivePlayback();

	void SetForwardSpeed(float fSpeed);
	void SetForwardSpeedXY( float fSpeed );
	void SetSpeedToRestoreAfterFix(float fSpeed) { m_fSpeedToRestoreAfterFix = fSpeed; };

	u8 GetNumInternalWindshieldHits() const {return m_nNumInternalWindshieldHits;}
	void AddInternalWindshieldHit() {m_nNumInternalWindshieldHits++;}
	u8 GetNumInternalWindshieldRearHits() const {return m_nNumInternalWindshieldRearHits;}
	void AddInternalWindshieldRearHit() {m_nNumInternalWindshieldRearHits++;}

	bool GetIsShelteredFromRain();
	bool HasCollisionLoadedAroundVehicle();

// Legacy
	// Gets max number of passengers for the vehicle
    s32 GetMaxPassengers() const {return m_SeatManager.GetMaxSeats() > 0 ? m_SeatManager.GetMaxSeats()-1 : 0; }

	// Add driver/passenger if one doesn't exist
	CPed* SetUpDriver(bool bUseExistingNodes = false, bool canLeaveVehicle = true, u32 desiredModel = fwModelId::MI_INVALID, const CAmbientModelVariations* variations = NULL);
	CPed* SetupPassenger(s32 seat_num, bool canLeaveVehicle = true, u32 desiredModel = fwModelId::MI_INVALID, const CAmbientModelVariations* variations = NULL);

	// Swat Specific Setup
	CPed* SetUpSwatDriver(bool bUseExistingNodes = false, bool canLeaveVehicle = true, u32 desiredModel = fwModelId::MI_INVALID);
	CPed* SetupSwatPassenger(s32 seat_num, bool canLeaveVehicle = true, u32 desiredModel = fwModelId::MI_INVALID);

	bool IsDriver(const CPed* pPed) const;
	bool IsDriver(u32 iModelIndex) const;

	// Animation
	void InitAnimLazy();
	void SetDriveMusclesToAnimation(bool bDriveMuscles) {m_nVehicleFlags.bDriveMusclesToAnim = bDriveMuscles;}

	void NotifyPassengersOfRadioTrackChange();

	bool	GetHasBeenSeen() {return m_nVehicleFlags.bHasBeenSeen;}	
	bool	GetIsInReusePool() const { return m_nVehicleFlags.bIsInVehicleReusePool; }
	void	SetIsInReusePool(bool isInReuse)	{ m_nVehicleFlags.bIsInVehicleReusePool = isInReuse;}
#if __BANK
	bool	GetWasReused() const { return m_nVehicleFlags.bWasReused; }
	void	SetWasReused(bool reused)	{ m_nVehicleFlags.bWasReused = reused;}

    void    LogPretendOccupantsFail() const;

    void DrawDownforceDebug();
    void DrawQADebug();
#endif // __BANK

	u32 GetNpcHashForDriver(u32 driverModelIndex) const;

    const Vector3& GetParentCargenPos() const { return m_parentCargenPos; }
    void SetParentCargenPos(const Vector3& pos) { m_parentCargenPos = pos; }

#if ENABLE_FRAG_OPTIMIZATION
	bool GetHasFragCacheEntry() const { return m_nVehicleFlags.bHasFragCacheEntry; }
	void TryToReleaseFragCacheEntry();
	void GiveFragCacheEntry(bool lock = false);
#endif

#if __ASSERT
	bool IsVehicleAllowedInMultiplayer() const;
#endif /* __ASSERT */
	static bool IsTypeAllowedInMP(u32);

	bool GetNoDamageFromExplosionsOwnedByDriver() const { return m_bNoDamageFromExplosionsOwnedByDriver; }
	void SetNoDamageFromExplosionsOwnedByDriver(bool bNoDamage) { m_bNoDamageFromExplosionsOwnedByDriver = bNoDamage; }

	// B*2425992: Vehicle occupants will take explosive damage even if the vehicle is flagged as not damaged by anything/explosion proof.
	bool GetVehicleOccupantsTakeExplosiveDamage() const { return m_bVehicleOccupantsTakeExplosiveDamage; }
	void SetVehicleOccupantsTakeExplosiveDamage(bool bVehOccupantsTakeExplosiveDamage) { m_bVehicleOccupantsTakeExplosiveDamage = bVehOccupantsTakeExplosiveDamage; }

	bool GetCanEjectPassengersIfLocked() const { return m_bCanEjectPassengersIfLocked; }
	void SetCanEjectPassengersIfLocked(bool bCanEjectPassengersIfLocked) { m_bCanEjectPassengersIfLocked = bCanEjectPassengersIfLocked; }

	bool GetVehicleAllowHomingMissleLockOnSynced() const { return m_bAllowHomingMissleLockOnSynced; }
	void SetVehicleAllowHomingMissleLockOnSynced(bool bAllowHomingMissleLockOn) { m_bAllowHomingMissleLockOnSynced = bAllowHomingMissleLockOn; }

	// Allow script to disable the damage reduction multiplier we apply in MP when attacking a vehicle containing a player
	bool GetUseMPDamageMultiplierForPlayerVehicle() const { return m_bUseMPDamageMultiplierForPlayerVehicle; }
	void SetUseMPDamageMultiplierForPlayerVehicle(bool bUseMultiplier) { m_bUseMPDamageMultiplierForPlayerVehicle = bUseMultiplier; }

	// Allow script force a vehicle to explode when hitting zero body health with explosive damage (used in CVehicleDamage::ApplyDamageToBody).
	bool GetShouldVehicleExplodesOnExplosionDamageAtZeroBodyHealth() const { return m_bShouldVehicleExplodesOnExplosionDamageAtZeroBodyHealth; }
	void SetShouldVehicleExplodesOnExplosionDamageAtZeroBodyHealth(bool bShouldExplode) { m_bShouldVehicleExplodesOnExplosionDamageAtZeroBodyHealth = bShouldExplode; }

	bool IsEnteringInsideOrExiting();
	void AddLight(eLightType type, s32 boneIdx, const ConfigLightSettings &lightParam, float distFade);

	float GetSlowDownBoostDuration(){ return m_fSlowdownDuration; }
	float GetBoostAmount(){ return mf_BoostAmount; }
	float GetSpeedUpBoostDuration(){ return mf_BoostAppliedTimer; }
	void SetBeastVehicle( bool beastVehicle ) { m_BeastVehicle = beastVehicle; }
	bool GetIsBeastVehicle(){ return m_BeastVehicle; }

	void SetRampOrRammingAllowed( bool rampOrRammingAllowed ) { m_RampOrRammingAllowed = rampOrRammingAllowed; }
	bool GetRampOrRammingAllowed() { return m_RampOrRammingAllowed; }

	void SetEasyToLand( bool easyToLand ) { m_easyToLand = easyToLand; }

	void SetScriptRammingImpulseScale( float scale ) { m_ScriptRampImpulseScale = scale; }
	float GetScriptRammingImpulseScale()	{ return m_ScriptRampImpulseScale; }

	void InitialiseModCollision();

	void SetParachuteModelOverride( u32 uModelHash ) { m_ParachuteModelHash = uModelHash; }
	u32 GetParachuteModelOverride() { return m_ParachuteModelHash; }

//////////////////////////////////////////////////////////////////////////
// Emblem material interface.

	void SetEmblemMaterialGroup(bool isLocalPlayer) const { m_EmblemMaterialGroup = isLocalPlayer ? 1 : 0; }
	void ClearEmblemMaterialGroup() const { m_EmblemMaterialGroup = 0; }
	bool IsLocalPlayerEmblemMaterialGroup() const { return m_EmblemMaterialGroup == 1; }

//////////////////////////////////////////////////////////////////////////

	bool ProcessBoostPadImpact( phCachedContactIterator& impacts, CEntity* pOtherEntity );

	void UpdateWeaponBones(const CVehicleWeaponBattery* pBattery, const int* weaponBoneIndex, bool bScaleIndividualWeaponBones = false, bool bRotateGrillBone = false, float hiddenOffset = 0.11f, float fGrillOpenOffset = 0.07f, float fRackOpenOffset = 0.1f, float fGrillOpenRotation = 28.0f);
	void UpdateWeaponBatteryBones();
	void UpdateMissileFlapBonesOld(CVehicleWeaponBattery* pBattery, s32 iBatteryAmmo);
	void UpdateMissileFlapBones(CVehicleWeaponBattery* pBattery);
	void UpdateMissileBatteryState(CVehicleWeaponBattery* pBattery, s32 iLastFiredMissile);
	float UpdateMissileFlapStateAndGetDesiredRotation(s32 i, s32 iLastFiredMissile, float fullyOpenFlapRotation, float openingRotation, float closingRotation, CVehicleWeaponBattery* pBattery);

// static vehicle functions
public:
	static void InitSystem();
	static void ShutdownSystem();

	// a static version of GetHeightAboveRoad so we can calculate the vehicle's matrix on the road before it's actually created
	static void CalculateHeightsAboveRoad(fwModelId, float *pFrontHeight, float *pRearHeight);

	static void UpdateVisualDataSettings();

	static void StartAiUpdate(CVehicle* pVehicle);
	static void StartAiUpdate();
	static void PreGameWorldAiUpdate(CVehicle* pVehicle, const bool bFullUpdate);
	static void PreGameWorldAiUpdate();
	static void FinishAiUpdate(CVehicle* pVehicle, const bool bFullUpdate);
	static void FinishAiUpdate();

	static void UpdateLightsOnFlagsForAllVehicles();
	static void ProcessAfterCameraUpdate();
	static void ProcessPostVisibility();

	void UpdateInSpatialArray();

	static CVehicle* GetVehicleFromSpatialArrayNode(CSpatialArrayNode* node)
	{
		return reinterpret_cast<CVehicle*>( reinterpret_cast<char*>(node) - OffsetOf(CVehicle, m_spatialArrayNode) );
	}

	static const CVehicle* GetVehicleFromSpatialArrayNode(const CSpatialArrayNode* node)
	{
		return reinterpret_cast<const CVehicle*>( reinterpret_cast<const char*>(node) - OffsetOf(CVehicle, m_spatialArrayNode) );
	}

	CSpatialArrayNode& GetSpatialArrayNode() { return m_spatialArrayNode; }
	const CSpatialArrayNode& GetSpatialArrayNode() const { return m_spatialArrayNode; }
	static CSpatialArray& GetSpatialArray() { return *ms_spatialArray; }

	static void SetLightsCutoffDistanceTweak(float tweak) { ms_fLightCutoffDistanceTweak = tweak; }
	static float GetLightsCutoffDistanceTweak() { return ms_fLightCutoffDistanceTweak; }

    static void RequestHdForVehicle(CVehicle* veh) { ms_requestedHdVeh = veh; ms_lastTimeRequestedHd = fwTimer::GetTimeInMilliseconds(); }

#if __DEV 
	static void PoolFullCallback(void* pItem);
#endif // __DEV

	static bool TooManyVehicleBreaksRecently() { return ms_VehicleBreaksPerSecond >= ms_MaxVehicleBreaksPerSecond; }
	static float GetNumVehicleBreaksPerSecond() { return ms_VehicleBreaksPerSecond; }
	static void RegisterVehicleBreak();
	static void UpdateVehicleBreaksPerSecond();

	static float GetDayNightFade();
	static float GetLightOffValue(s32 light, float dayNightFade);

	static void SetCanEnterLockedForSpecialEditionVehicles(bool bCanEnter) { ms_CanEnterLockedForSpecialEditionVehicles = bCanEnter; }
	static bool GetCanEnterLockedForSpecialEditionVehicles() { return ms_CanEnterLockedForSpecialEditionVehicles; }

	static void SetDisableHDVehicles(bool disableHDVehicles = true)			{ ms_DisableHDVehicles = disableHDVehicles; }

	static bool GetConsumePetrol() { return m_consumePetrol; }
	static void SetConsumePetrol(bool value) { m_consumePetrol = value; }

	static bool IsInsideVehicleModeEnabled() { return (ms_insideVehicleMode > 0); } 
	static bool IsInsideSpecificVehicleModeEnabled(u32 vehicleHash) { return (ms_insideVehicleMode == vehicleHash); } 
	
	static void SetBobbleHeadVelocity( Vec3V& velocity );

    void ProcessBoostPhysics( float fTimeStep );

	float GetSideShuntForce() const { return m_sideShuntForce; }
	//float GetForwardShuntForce() const { return m_fowardShuntForce; }
	bool GetHitBySideShunt() const { return m_hitBySideShunt;	}
	CVehicle* GetLastSideShuntVehicle() const { return m_lastSideShuntVehicle; }
	void SetHitBySideShunt( bool hit, CVehicle* sideShuntVehicle ) { m_hitBySideShunt = hit; m_lastSideShuntVehicle = sideShuntVehicle;	}
	bool GetHitByWeaponBlade() const { return m_hitByWeaponBlade; }
	void SetHitByWeaponBlade( bool hit ) { m_hitByWeaponBlade = hit; }
	CWeaponBlade& GetWeaponBlade(int bladeIdx) { return m_weaponBlades[bladeIdx]; }	

	void SetOverrideNitrousLevel(bool overrideNitrous);
	void SetOverrideNitrousLevel(bool overrideNitrous, float durationModOverride, float powerModOverride, float rechargeRateModOverride, bool disableDefaultSound = false);
	bool GetOverrideNitrousLevel() { return m_scriptOverridesNitrous.isOverrideActive; }
	bool GetOverrideNitrousSound() { return m_scriptOverridesNitrous.disableDefaultSound; }
	
	void FullyChargeNitrous();
	void ClearNitrous();

	int GetNumWeaponBlades() { return m_numWeaponBlades; }
	void ApplyWeaponBladeWheelImpacts( CWheel* wheel, int hitComponent );

	void DetachTombstone();
	void HideTombstone( bool hide );
	bool HasTombstone() const;

protected:
    bool IsAnimated();
	void ProcessRocketBoost();
	bool CanToggleRocketBoost();
	u32 GetBoostToggleTime() {return m_boostToggledTime;}

private:
	static void	CalculateRumbleFrequencies( s32& freq0, s32& freq1, phRumbleProfile* rumbleProfile, float rumbleMult, float velFreqScale, float velModifier, bool bIgnoreVelocityForRumble );
	void UpdateBobbleHead( int boneIndex, eBobbleHeadType type );
	static CBobbleHead* GetBobbleHead( CVehicle* pVehicle, eBobbleHeadType type );
	static CBobbleHead* AddBobbleHead( CVehicle* pVehicle, eBobbleHeadType type );

	void ApplyBeastModeToImpacts( phCachedContactIterator& impacts );
	CVehicle* FindRammingScoopVehicle( phCachedContactIterator& impacts );
	void ApplyRammingScoopToImpact( CVehicle* pOtherVehicle, phCachedContactIterator& impacts );
	void ApplyRammingBarToImpact( CPhysical* pOtherEntity, phCachedContactIterator& impacts );
	void UpdateInvMassForRammingBarImpacts( phCachedContactIterator& impacts );
	CVehicle* FindRampVehicle( phCachedContactIterator& impacts );
	void ModifyPedImpacts( phCachedContactIterator& impacts );
	bool ShouldApplyRampToImpact( CVehicle* pRampCar, CVehicle* pOtherVehicle, Vec3V& rampCarNormal );
	void ApplyRampToImpacts( CVehicle* pOtherVehicle, phCachedContactIterator& impacts );
	void ProcessBoostVFX();
	void ProcessRamBarVFX();
	void ProcessWeapons();
	void ProcessNitrousControl();
	void UpdateBladeRpmOnEngineOff();
	void ApplySpikeImpacts( phCachedContactIterator& impacts, CEntity* otherEntity );
	void ApplySpikeDamageToPed( CPed* pPed, Vec3V& pedNormal, float vehicleVelocity );
	void ApplySpikeDamageToVehicle( CVehicle* pOtherVehicle, int otherComponent, Vec3V& normal, float vehicleVelocity );
	void ProcessCarJump();
    void ProcessVehiclePumpedUpJump( float fTimeStep );
	void ProcessTombstone();

	CVehicle* CheckForSideShuntImpact( phCachedContactIterator& impacts, CVehicle* otherVehicle );

	// Rocket boost //////////////////////////////

	// Boost state flag
	bool m_bIsRocketBoosting;
	bool m_bRocketBoostedFromNetwork;
	bool m_bRocketBoostedStoppedFromNetwork;
	bool m_bCanRechargeRocketBoost;
	bool m_bTriggerRefillBoostEffect;
	bool m_bHasRocketBoostFXStarted;

	// Remaining boost
	float m_fRocketBoostRemaining;

	// Rocket boost recharge rate
	float m_fRocketBoostRechargeRate;
	u32 m_boostToggledTime;

	// End Rocket boost //////////////////////////

	bool m_nitrousActive;
	ScriptOverridesNitrous m_scriptOverridesNitrous;

	// Place on road probe heights ///////////////
	float m_fOverridenPlaceOnRoadHeight;

	bool m_bHomingCanLockOnToObjects; // Flags whether the vehicle weapons can lock on to script flagged objects

	// parachute params
	u32 m_ParachuteModelHash;

	void ProcessGlider();
	void UpdateWingBones( float targetRatio, bool instant );

	void ProcessGliderPhysics( float fTimeStep );
	void ProcessHoverMode( float fTimeStep, float normalisedDeployedRatio, CSpecialFlightHandlingData* flightHandling, float fStickX, float fStickY );

	void ProcessSpecialFlightMode();
	void ProcessOutriggers();
	void ProcessArenaMode();
	void UpdateSideShunt( float fTimeStep );

	void UpdateOutriggers( float fTimeStep ); 

	EGliderState m_gliderState;
	float		 m_wingDeployedRatio;
	float		 m_specialFlightModeTargetRatio;
	float		 m_targetGravityScale;
	float		 m_previousNormalisedVelocity;
    float        m_previousThrottle;
	float        m_cachedStickYfromNetwork;
    float        m_currentWingRatio;

	float m_scriptMaxSpeed;
	bool  m_disableHoverModeFlight;
	bool  m_destroyWeaponMgr;
	bool  m_hoveringCloseToGround;

	bool m_forceFixLinkMatrices;

protected:
    bool ProcessFoliageImpact( phInst* otherInstance, Vec3V myPosition, int myComponent, int otherComponent, int otherElement );
	void ForceUpdateLinkMatrices();

	float m_sideShuntForce;
//	float m_fowardShuntForce;
	float m_nozzleAngle;
	float m_specialFlightModeRatio;
	float m_outriggerRatio;
	RegdVeh m_lastSideShuntVehicle;
	bool m_deployOutriggers;
	bool m_hitBySideShunt;
	bool m_shuntModifierActive;
	bool m_hitByWeaponBlade;

public:
	void StartGliding();
	void FinishGliding();

	void StartGlidingInstant();
	void FinishGlidingInstant();

	void SetDisableHoverModeFlight( bool disable ) { m_disableHoverModeFlight = disable; }
	bool GetDisableHoverModeFlight() const { return m_disableHoverModeFlight; }

	bool GetAreOutriggersFullyDeployed() const { return m_outriggerRatio == 1.0f; }
	float GetOutriggerDeployRatio() const { return m_outriggerRatio; }
	void SetDeployOutriggers( bool deploy );
	bool GetAreOutriggersBeingDeployed() const { return m_deployOutriggers; }

	bool HoveringCloseToGround() const { return m_hoveringCloseToGround; }
	void SetFixLinkMatrices() { m_forceFixLinkMatrices = true; }

	void SetShuntLeftFromNetwork() { m_sideShuntForce = -SIDE_SHUNT_DURATION; m_VehicleAudioEntity->TriggerSideShunt(); }
	void SetShuntRightFromNetwork() { m_sideShuntForce = SIDE_SHUNT_DURATION; m_VehicleAudioEntity->TriggerSideShunt(); }

	// Car jump //////////////////////////////////////

	// Available jump states
	enum ECarJumpState
	{
		NOT_JUMPING,
		JUMPING,
		IN_AIR_STABILISATION,
		//FORWARD_JUMP,
		//BACKWARDS_JUMP,

		HOVER_CAR_END
	};

private:

	// Tracks the current state of the jump
	ECarJumpState m_carJumpState;

	// Flag accessible to the world to start/cancel jump
	bool m_bDoingJump;
	//bool m_bDoingForwardJump;

	// Timer used for transitioning between jump states
	float m_fCarJumpTimer;

	// Ground normal for measuring angles to stabilise vehicle mid-jump
	Vector3 m_vCarJumpGroundNormal;

public:
	// Jump recharge time
	static bank_float ms_fJumpRechargeTime;

	ECarJumpState GetJumpState() { return m_carJumpState; }
	void SetJumpState( ECarJumpState jumpState ) { m_carJumpState = jumpState; }

	// Car jump state accessor/mutator
	bool GetIsDoingJump() {	return m_bDoingJump; }
	void SetIsDoingJump( bool bNewValue ) {	m_bDoingJump = bNewValue; }
	//void SetIsDoingForwardJump( bool bNewValue ) { m_bDoingForwardJump = bNewValue; }
	//bool GetIsDoingForwardJump() { return m_bDoingForwardJump; }
	bool CanTriggerForwardJump();
	bool IsJumpFullyCharged() const { return m_fJumpRechargeTimer >= ms_fJumpRechargeTime; }
	f32 GetJumpRechargeFraction() const { return Clamp( m_fJumpRechargeTimer / ms_fJumpRechargeTime, 0.f, 1.0f ); }
	void SetDoHigherJump( bool bNewValue ) { m_bDoHigherJump = bNewValue; }
	void SetJumpRechargeTimer( f32 recharge )	{ m_fJumpRechargeTimer = recharge; }
	f32 GetJumpRechargeTimer()	{ return m_fJumpRechargeTimer; }

	void RechargeJumpTimer() { m_fJumpRechargeTimer = ms_fJumpRechargeTime; }

	// Updates the recharge bar on the jumping car
	void UpdateJumpingCarRechargeTimer();

	CObject* GetDetachedTombStone() { return m_detachedTombstone; }
	const CObject* GetDetachedTombStone() const { return m_detachedTombstone; }
private:

	// Jump recharge timer
	float m_fJumpRechargeTimer;

	// Extra jump strength, to be set by script and used on missions
	bool m_bDoHigherJump;

	// End car jump //////////////////////////////////

public:
    void TriggerPumpedUpJump( float fHeightScale );

    // NEW JUMP FOR COPS AND CROOKS PUMPED UP ABILITY
    // Available jump states
    enum EPumpedUpJumpState
    {
        PUMPED_UP_JUMP_IDLE,

        PUMPED_UP_JUMP_BEGIN,
        PUMPED_UP_JUMP_COMPRESS_SUSPENSION = PUMPED_UP_JUMP_BEGIN,
        PUMPED_UP_JUMP_EXTEND_SUSPENSION,
        PUMPED_UP_JUMP_ACCELERATE, // might not need this state 

        PUMPED_UP_JUMP_END,
    };

private:
    EPumpedUpJumpState m_pumpedUpJumpState;
    float m_fInitialSuspensionRaiseRate;
    float m_fPumpedUpJumpTimer;
    float m_fJumpHeightScale;

//////////////////////////////////////////////////////////////////////////
// Member variables
public:
	// PURPOSE:	Timesliced update interpolation target position (XYZ) and time (W) in seconds to get there.
	Vec4V m_InterpolationTargetAndTime;

	// PURPOSE:	Timesliced update interpolation target orientation, as a quaternion.
	QuatV m_InterpolationTargetOrientation;

	// PURPOSE:	When interpolating in timesliced mode, this is the current orientation,
	//			stored off so we don't have to recompute it from the matrix.
	QuatV m_InterpolationCurrentOrientation;

	//these are used to re-create trailer attachments
	//when converting from superdummy mode
	Vector3 m_vParentTrailerEntity1Offset;
	Vector3 m_vParentTrailerEntity2Offset;
	Quaternion m_qParentTrailerEntityRotation;

	CVehicleDamage m_VehicleDamage;
	CTransmission m_Transmission;

	CHandlingData* pHandling;
	CVehicleFlags m_nVehicleFlags;

	// PURPOSE:	Time passed since the last full AI update, in seconds. Used to compute
	//			the proper update time step during AI timeslicing.
	float m_fTimeSinceLastAiUpdate;
	u32 m_iNumTimeslicedUpdatesSkipped;

	audVehicleAudioEntity* m_VehicleAudioEntity;
	audPlaceableTracker m_PlaceableTracker;

	fragInstGta* m_pVehicleFragInst;
	phInstGta* m_pDummyInst;

	float m_fParentTrailerAttachStrength;

	float m_fSteerInput;	// input from controller
	float m_fSteerInputBias;// the amount by which the input from controller is biased (to be used by script to modify the controls)
	CVehControls m_vehControls;
	u32 m_uHandbrakeOnTime; // When was the handbrake pressed. Used to envelope handbrake after press
	bool m_bInvertControls;

	// population / creation / dummy
	s16 m_CarGenThatCreatedUs;
	s16 m_ExtendedRemovalRange;
	u32 m_TimeOfCreation;			// GetTimeInMilliseconds when this vehicle was created.

	//CVehicle* m_pNextVehOnSamePathLink;
	u32 m_delayedRemovalResetTimeMs;
	u32 m_delayedRemovalAmountMs;
	u32 m_LastTimeWeHadADriver;		// Can be used to remove certain vehicles if they haven't had a driver for a while. (0 Means:never had a driver at all)
	u32 m_dummyConvertAttemptTimeMs;
	u32	m_realConvertAttemptTimeMs;
	u32 m_nRealVehicleIncludeFlags;
	u32 m_nCloneFlaggedForWreckedOrEmptyDeletionTime;

	// rendering / effects
	s32 m_Swankness;
	float m_fBodyDirtLevel;	// Dirt level of vehicle body texture: 0.0f=fully clean, 15.0f=maximum dirt visible, it may be altered at any time while vehicle's cycle of lige
	Color32 m_bodyDirtColor;

	// lights
	CVehicleLightSwitch m_VehicleLightSwitch;
	u32 m_FlashHeadLights;			// Time at which we will stop flashing our headlights.
	u32 m_TimeFullBeamSwitchedOn;	// When were the beam lights switched on.
	u32 m_KeepLightsOnForWeatherTime;
	float m_HeadlightMultiplier;
	u8 m_OverrideLights : 3;			// uses enum NO_CAR_LIGHT_OVERRIDE, FORCE_CAR_LIGHTS_OFF, FORCE_CAR_LIGHTS_ON, SET_CAR_LIGHTS_OFF, SET_CAR_LIGHTS_ON
	u8 m_HeadlightShadows : 2;		// uses bit flags HEADLIGHTS_CAST_DYNAMIC_SHADOWS, HEADLIGHTS_CAST_STATIC_SHADOWS, HEADLIGHTS_CAST_FULL_SHADOWS
	u8 m_AmbientScaleOverriden : 1; 
	u8 m_bInteriorLightForceOn : 1;	// Force interior lights to be on (like m_nVehicleFlags.bInteriorLightOn, but ignores day/night fade)
	u8 m_padding : 1;
	bool m_usePlayerLightSettings;
	u8 m_prevNaturalAmbientScale;
	u8 m_prevArtificialAmbientScale;
	// misc
	u32 m_nFixedWaitingForCollisionCheckTimer;
	CEntity::eHomingLockOnState m_eHomingLockOnState;     // Is this vehicle locked onto a target?  - Value gets reset every frame and set by other systems
    CEntity::eHomingLockOnState m_eHomingLockedOntoState; // Is something locked onto this vehicle? - Value gets reset every frame and set by other systems
	float m_fTargetableDistance;				          // The los targeting checks for lock on pass within this distance
	float m_fHomingProjectileDistance;		              // The distance to the closest homing projectile
	s8 m_nHomingLockOnStateResetFrame;			          // Number of frames before the lock on state is reset
    s8 m_nHomingLockedOntoStateResetFrame;                // Number of frames before the lock on state is reset
	s8 m_nHomingProjectileDistanceResetFrame;	          // Number of frames before the lock on distance is reset
	u8 m_AirResistanceState;							  // Used to reduce work we have to do for AI vehicles in UpdateAirResistance().
	u32 m_uLastTimeHomedAt;								  // Last time a homing projectile was aimed at this vehicle.
    RegdPhy m_pLockOnTarget;                              // The object being targetted
	RegdPhy m_pGroundPhysical;                            // Ground physical used when wheels aren't touching the ground (i.e. when vehicle is flipped)
	u32 m_uGroundPhysicalTime;							  // Time ground physical was registered
	u32 m_iFullThrottleRecoveryTime;					  // Time until the Full Throttle effect stops being applied
	bool m_bFullThrottleActive;					  // Time until the Full Throttle effect stops being applied

	RegdPed m_pExclusiveDriverPeds[MAX_NUM_EXCLUSIVE_DRIVERS];		// If set, only these peds can drive this vehicle.

	u32 m_wheelBrokenIndex;

	float m_fFrictionOverride;

    float m_fDragCoeff;

	float m_EngineTemperature;		// Between ambient temperature and 100 degrees. Affects engine starting.
	u32 m_EngineSwitchedOnTime;	// This is in place until the audio scripts take over the responsibility of switching on the engine

	// bit masks for the different extra rules setups
	u32 m_nDisableExtras;
	static u32 ms_nExtrasMasks[EXTRAS_MASK_NUM_MASKS];

	// last activation
	u32 m_iLastActivationTime;
	u32 m_iNextValidHornTime;

	static Vector3 smVecSelfRightBiasCGOffset;
	static bool	sm_bUseNewSelfRighting;

	static bool sm_bEnableHeadlightShadows;
	static bool sm_bEnableSirenShadows;

	static bool sm_bJetWashEnabled;
	static bool sm_bDisableExplosionDamage;
	static bool sm_bDisableWeaponBladeForces;
	static bool sm_bDoubleClickForJump;
	//static bool sm_bInDetonationMode;
	//static bool sm_bShuntOnStick;

	bool m_bProducingSlipStreamForOther;
	ObjectId m_VehicleProducingSlipstream;
	float m_fTimeInSlipStream;
	float m_fTimeProducingSlipStream;
	float m_fSlipStreamRechargeAndDechargeTimer;
	float m_fTimeInAir;
	float m_fTimeInAirCrash;
	float m_fTimeTouchingPavement;

	float m_fSpeedToRestoreAfterFix;
	float m_fPreviousSpeedForDash;

    u32 m_dialsRequestFrame;

	u16 CarAlarmState;

	u8 m_failedEngineStartAttempts;
	bool m_failNextEngineStartAttempt;

	bool m_bInAirCrash;

	bool m_bAllowRemoteDamageOnCreation;

	bool m_bNoDamageFromExplosionsOwnedByDriver;

	bool m_pedRemoteUsingDoor;
	
	bool m_remoteDriverDoorClosing;

	bool m_forceLightsOnAtNightRandomisationTime;
	bool m_forceLightsOffAtMorningRandomisationTime;

	bool m_bSuppressNeons;

	bool m_bVehicleOccupantsTakeExplosiveDamage;

	bool m_bCanEjectPassengersIfLocked;

	bool m_bAllowHomingMissleLockOnSynced;

	bool m_bSpecialEnterExit;

	bool m_bDontProcessVehicleGlass;

	bool m_bTyresDontBurstToRim;

	bool m_bDriftTyres; // Sets the tyres to be easier to drift with

	bool m_bUseDesiredZCruiseSpeedForLanding;

	bool m_bDontResetTurretHeadingInScriptedCamera;

	bool m_bDontOpenRearDoorsOnExplosion;

	bool m_bOnlyStartVehicleEngineOnThrottle;

	bool m_bClearWaitingOnCollisionOnPlayerEnter;

	bool m_bUseMPDamageMultiplierForPlayerVehicle;
	bool m_bShouldVehicleExplodesOnExplosionDamageAtZeroBodyHealth;

	bool m_bBlockDueToEnterExit;

	bool m_bDisablePlayerCanStandOnTop;

	bool m_bSubmarineCarTransformPrevented;

	bool m_bDisableWantedConesResponse;

    bool m_bHasDeformationBeenApplied;

	fwUniqueObjId m_iPrevSpeedBoostObjectID;

	fwUniqueObjId m_iCurrentSpeedBoostObjectID;

	static dev_float FLIGHT_CEILING_PLANE;
	static dev_float FLIGHT_MICROLIGHT_CEILING_PLANE;
	static float ms_fFlightCeilingScripted;

	// Air defence flag/explosion timer
	bool  m_bAirDefenceExplosionTriggered;
	u32   m_uTimeAirDefenceExplosionTriggered;

	float m_VehicleTopSpeedPercent;
	float m_fOverrideArriveDistForVehPersuitAttack;

	int   m_LaunchedInAirTimer;
	int   m_ForcedMaximumUpdateRateTimer;

    u32 m_kersToggledTime;

	// debug
#if __DEV
	phCollider *m_pDebugColliderDO_NOT_USE;
#endif
#if __BANK
	static bool* GetConsumePetrolPtr() { return &m_consumePetrol; }

	static bool m_bDebugVisPetrolLevel;

	static void UpdateVehicleHandlingInfo(CVehicleModelInfo* pVehicleModelInfo);
	bool GetCoordsAndAlphaForDebugText(Vector3 &vecReturnWorldCoors, u8 &ReturnAlpha) const;
	static bool m_bDebugEngineTemperature;
	static bool ms_bDebugWheelTemperatures;
	static bool ms_bDebugWheelFlags;
	static bool ms_bDebugIgnoreHoldGearWithWheelspinFlag;
	static bool ms_bDebugVehicleHealth;
	static bool ms_bDebugVehicleWeapons;
	static bool ms_bDebugVehicleStatus;
	static bool ms_bDebugVehicleIsDriveableFail;
    static u32 ms_uZeroToSixtyStartTime;
    static u32 ms_uZeroToSixtyEndTime;

    static float ms_fBestZeroToSixtyTime;
    static float ms_fBestSixtyToZeroTime;

	static float ms_fHighestGForce;
#endif
	

#if USE_PHYSICAL_HISTORY
	CPhysicalHistory m_physicalHistory;
#endif

	Vec3V m_vecInitialSpawnPosition;

#if __BANK
	ePopType m_CreationPopType;
#endif

    float m_fDownforceModifierFront;
    float m_fDownforceModifierRear;

protected:
	// Cache all this at the beginning of the AI update so we can avoid recomputing it later
	// This is a seperate struct in case we want to move it out of CVehicle at some point
	struct CachedAiData
	{
		Vec4V m_VelocityAndXYSpeed;
	};

	CachedAiData m_CachedAiData;
	Vector3 m_vecInternalForce;
	Vector3 m_vecInternalTorque;

	Vector3 m_vFoliageDragForce;
	Vector3 m_vFoliageDragTorque;

	// Velocity for superdummy (*only* used when CVehicleAiLodManager::ms_bDisablePhysicsInSuperDummyMode is true)
	Vector3 m_vSuperDummyVelocity;

	// Velocities for inactive vehicle that's following the playback
	Vector3 m_vInactivePlaybackVelocity;
	Vector3 m_vInactivePlaybackAngVelocity;

	// The last position the object tried to generate cover at
	Vector3 m_vLastCoverPosition;

	// The last forward direction the object tried to generate cover at
	Vector3 m_vLastCoverForwardDirection;

    Vector3 m_parentCargenPos;

	// veh ai
	CVehicleIntelligence	*m_pIntelligence;
	CVehicleAILod m_vehicleAiLod;
 
	VehicleType m_vehicleType;
	// array of pointers to wheels
	CWheel** m_ppWheels;
	int m_nNumWheels;
	u8 m_WheelIndexById[NUM_VEH_CWHEELS_MAX];
	bool m_WheelsAreJustProbes;

	//Need to make door array protected to make sure that doors are accessed by function by referencing either the 
	//eDoors enum or the hierarchyId enum. 
	CCarDoor *m_pDoors;
	int m_nNumDoors;	
	bool m_bWindowsRolledDown[VEH_LAST_WINDOW-VEH_WINDSCREEN_R];
	s32 m_iWindowComponent[VEH_LAST_WINDOW - VEH_FIRST_WINDOW + 1];


	CVehicleWeaponMgr* m_pVehicleWeaponMgr;
	atArray<CVehicleGadget*> m_pVehicleGadgets;

	float m_fVerticalVelocityToKnockOffThisBike;

	float m_fGravityForWheelIntegrator; // Use the SetGravityForWheelIntegrator function to set this.

	CSeatManager m_SeatManager;

	CComponentReservationManager m_ComponentReservationMgr;

	// The last orientation the object tried to generate cover at
	CoverOrientation m_eLastCoverOrientation;

	// The last door intact status for generated cover
	bool m_bLastCoverDoorIntactList[MAX_NUM_COVER_DOORS];

	// The last door ratios for generated cover
	float m_fLastCoverDoorRatioList[MAX_NUM_COVER_DOORS];

	TDynamicObjectIndex m_iPathServerDynObjId;

	// PURPOSE:	Used between PreRender() and PreRender2() to ensure that we wait for eactly
	//			the animation updates we started.
	bool m_StartedAnimDirectorPreRenderUpdate;

	// Used for when a remotely owned quad bike toggles it's tuck in state for its wheels 
	bool m_bToggleTuckInWheelsFromNetwork;

	// Allow script to set an ammo limit for a particular vehicle weapon
	s32 m_iRestrictedAmmoCount[MAX_NUM_VEHICLE_WEAPONS];

	// Network-synced ammo counts for bombs and countermeasures (can't be tied to a specific player)
	s32 m_iBombAmmoCount;
	s32 m_iCountermeasureAmmoCount;

	RegdObj m_detachedTombstone;

#if __ASSERT
	enum WhichVelFlags
	{
		EDIT_MODE_ACTIVE,
		REPLAY_IS_LOADING,
		HAS_COLLIDER,
		DISABLE_SUPERDUMMY_PHYSICS,
		IS_SUPERDUMMY,
		IS_DOING_PLAYBACK,
	};
	atFixedBitSet32 m_WhichVelocityFlags; // TODO: Russ should delete this as soon as B* 1037671 and friends are resolved
#endif

#if __BANK
	mutable const char *m_pNonConversionReasonString;
    mutable const char *m_pLastVehPopRemovalFailReason;
    mutable u32         m_iLastVehPopRemovalFailFrame;
#endif

#if !__FINAL
	// Debug name that can be set from script and displayed to help designers debug problems.
	char m_strDebugName[DEBUG_VEH_NAME_STRING_LENGTH];
#endif

	CTrailer* m_parentTrailer;

	CSpatialArrayNode m_spatialArrayNode;
	friend class CSpatialArray;

#if __ASSERT
    u32 m_notInWorldTimer;
#endif // __ASSERT

#if DEBUG_DRAW
public:
	Vec3V m_vCreatedPos;
	Vec3V m_vCreatedDir;
#endif

#if __BANK
public:
	enum VehicleCreationMethod
	{
		VEHICLE_CREATION_RANDOM =0,
		VEHICLE_CREATION_PATHS,
		VEHICLE_CREATION_CARGEN,
		VEHICLE_CREATION_CRIMINAL,
		VEHICLE_CREATION_EMERGENCY,
		VEHICLE_CREATION_SCRIPT,
		VEHICLE_CREATION_TRAILER,
		VEHICLE_CREATION_INVALID
	};

	VehicleCreationMethod m_CreationMethod;

	u32 m_iCreationCost;
#endif

#if __BANK
public:
	enum RealPedFailReason
	{
		RPFR_SUCCEEDED,
		RPFR_IS_NET_CLONE,
		RPFR_IS_NOT_NET_OBJECT,
		RPFR_NET_OBJ_REGISTER_FAILED,
		RPFR_PED_POOL_FULL,
		RPFR_NO_FALLBACK_PED,
		RPFR_ADD_POLICE_FAILED,
		RPFR_NET_REQUEST_DRIVER_MODEL,
		RPFR_NET_SET_UP_DRIVER,
		RPFR_FILTERED_SETUP_DRIVER,
		RPFR_UNFILTERED_SETUP_DRIVER,
		RPFR_ADD_FOR_TRAIN,
		RPFR_ADD_FOR_VEH
	};
	int m_RealPedFailReason;

    static const char *GetRealPedFailReason(int realPedFailReason);
#endif // __BANK

private:
#if GTA_REPLAY
	// During replay playback we don't have any physics instances to contain
	// the vehicles velocity so store it here instead.
	Vector3	m_replayVelocity;
	//
	u8 m_appliedVehicleDamage;
	bool m_replayOverrideWheelsRot;
#endif // GTA_REPLAY

#if GPU_DAMAGE_WRITE_ENABLED
	bool m_bDamageWasUpdatedByGPU;
	bool m_bEnableDamage;
#endif

	// Dummy attachments
	RegdVeh m_DummyAttachmentParent;
	atRangeArray<RegdVeh, MAX_CARGO_VEHICLES> m_DummyAttachmentChildren;
	int m_nDummyAttachmentChildren;

	phMaterialMgr::Id m_dummyMtlId;			// the id of the material that the car is on (valid when this is a dummy and the wheels aren't active to query the material from)

	sirenInstanceData* m_sirenData;

	s32 m_iPrevCurrentMissile;
	u32 m_iTimeLastVisible;
	u32 m_nNumPixelsVisible;

	u32 m_uRespottingCollisionCarPedFrameCount;
	u32 m_uSecondaryRespotOverrideTimeout;

	u32	m_nVehicleCheats;
	float m_fCheatPowerIncrease;
	float m_fDrivenDangerouslyTime;
	float m_fLodMult;

#ifdef __BANK
	public:
	float m_fDebugCheatPowerIncrease;
	private:
#endif
	
//  lighting configuration
	float m_timeLightsTurnOn;
	float m_timeLightsTurnOff;
	
	eVehicleLodState	m_vehicleLodState;	

	//Destruction
	DestructionInfo m_DestructionInfo;

	eCarLockState m_eDoorLockState;

	CNodeAddress m_StartNodes[2];
	s16 m_StartLink;

	u16 m_nNetworkRespotCounter;			// when veh gets re-spotted in a network game, want to disable collisions with other network vehicles 

	s8 m_StartLane;
	u8 m_nNumInternalWindshieldHits;		//After shooting your own windshield too many times, it breaks
	u8 m_nNumInternalWindshieldRearHits;	//After shooting your own rear windshield too many times, it breaks

	u8 m_lightingConfig;
	u8 m_sirenConfig;
    s8 m_clampedRenderLod;

	bool m_bRespottingCollisionCarPed;
	bool m_bFlashRemotelyDuringRespotting;
	bool m_bFlashLocallyDuringRespotting;

	static bool m_consumePetrol;
	float m_triggerRumbleTime;		// The amount of time the triggers have been continuously rumbling.

	mutable u8			m_EmblemMaterialGroup	: 1; // 0 no particular group, 1 local player.
	u8					m_BeastVehicle			: 1; 
	u8					m_RampOrRammingAllowed	: 1; 
	ATTR_UNUSED u8		m_nUnused				: 5;

	float				mf_BoostAppliedTimer;
	float				mf_BoostAmount;
	float				m_fSlowdownDuration;
	float				m_ScriptRampImpulseScale;

	bool m_allowSpecialFlightMode;

public:
	bool m_easyToLand;
	bool m_forceUpdateBoundsAndBodyFromSkeleton;
	bool m_hasHitRampCar;
	bool m_disableRampCarImpactDamage;
	bool m_applyRampCarSideImpulse;
	bool m_normaliseRampHitVelocity;
	bool m_activateBoostEffect;
    bool m_resetBoostObjects;

//////////////////////////////////////////////////////////////////////////
// static vehicle members
	static bool ms_forceActualVehicleLightsOff;
	static dev_bool ms_forceVehicleLightsOff;
	static dev_float ms_fVehicleSubmergeSleepGlobalZ;
	static bank_bool ms_bUseAutomobileBVHupdate;
	static bank_bool ms_bAlwaysUpdateCompositeBound;
	static bank_float ms_fAOHeightCutoff;
	
	// Foliage constants:
	static bank_float ms_fFoliageVehicleDragCoeff;
	static bank_float ms_fFoliageDragDistanceScaleCoeff;
	static bank_float ms_fFoliageBoundRadiusScaleCoeff;
	static bank_float ms_fMinVehicleSpeedForFoliageDrag;
	static bank_float ms_fMaxAngAccel;
	static dev_float ms_fVerticalVelocityToKnockOffVehicle;
	static dev_float ms_fVerticalVelocityToKnockOffHoverBike;
	static bank_float ms_fFoliageMaxDoorAccel;
	static bank_float ms_fFoliageDoorForceScaleCoeff;
#if __DEV
	static Vector3 ms_vFoliageDragForceAppliedThisFrame;
	static float ms_fFoliageDragForceMagSqrAppliedThisFrame;
#endif // __DEV

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	static CPhysical::CSecondSurfaceConfig ms_vehicleSecondSurfaceConfig;
	static CPhysical::CSecondSurfaceConfig ms_vehicleSecondSurfaceConfigLessSink;
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

	// This number decreases as more cars start using wheel shapetests
	static s32 ms_nNumAvailableShapetests;

	static float ms_fLightCutoffDistanceTweak;

	static bank_bool ms_bSuperLowLod_DisableCollisions;
	static bank_bool ms_bSuperLowLod_DisableGravity;

	static bank_bool  ms_bWreckVehicleOnFirstExplosion;
	static bank_float ms_fVehicleExplosionDelayMax;
	static bank_float ms_fVehicleExplosionDelayMin;


	static dev_float ms_fBicyclePavementVelocitySquaredThreshold;
	static dev_float ms_fVehiclePavementVelocitySquaredThreshold;
	static dev_float ms_fTimeTouchingPavementBeforeAddingEvent;

	static const u32 MAX_NUM_DOOR_COMPONENTS = 8;
	static eHierarchyId ms_aDoorComponentIds[MAX_NUM_DOOR_COMPONENTS];

	static eHierarchyId ms_aBobbleHeadIds[ BHT_MAX ];

	static CSpatialArray* ms_spatialArray;

	static bank_float ms_fSpeedToStartSlipstream;
	static bank_float ms_fSpeedToStartSlipstreamBoat;
	static bank_float ms_fMaxSpeedOfSlipStream;
	static bank_float ms_fSlipstreamMaxDragReduction;
	static bank_float ms_fSlipstreamMaxDistance;
	static bank_float ms_fSlipstreamMaxTime;
	static bank_float ms_fSlipstreamMaxTimeBeforeBeingDisabled;
	static bool ms_bSlipstreamApplyMaxTimeBeforeBeingDisabled;
	static bank_bool ms_bRealisticSlipstreamMode;

	static bank_float ms_fBikeGravityMult;
	static bank_float ms_fQuadbikeGravityMult;
	static bank_float ms_fOffRoadAbilityGravityMult;
	static bank_float ms_fOffRoadAbility2GravityMult;

#if USE_SIXAXIS_GESTURES
	// six-axis motion control multipliers
	static bank_float ms_fDefaultMotionContolPitchMin;
	static bank_float ms_fDefaultMotionContolPitchMax;
	static bank_float ms_fDefaultMotionContolRollMin;
	static bank_float ms_fDefaultMotionContolRollMax;
	static bank_float ms_fDefaultMotionContolAftertouchMult;
	static bank_float ms_fDefaultMotionContolSteerSmoothRate;
#endif // USE_SIXAXIS_GESTURES

	static bank_bool ms_bForceBikeLean;
#if __BANK
	static int ms_nVehicleDebug;
	static float ms_fHydraulicSuspension;	
    static float ms_fForcedGroundClearance;
	static bool ms_bForceSlowZone;
	static bool ms_bPrecomputeExtraWheelBoundImpacts;
    static bool ms_bVehicleDownforceDebug;
    static bool ms_bVehicleQADebug;

	static float ms_fVehicleDebugScale;
	static bool ms_bDebugDrawRevRatio;
	static bool ms_bDebugDrawSpeedRatio;
	static bool ms_bDebugDrawDriveForce;
	static bool ms_bDebugDrawClutchRatio;
	static bool ms_bDebugDrawWheelSpeed;
	static bool ms_bDebugDrawManifoldPressure;
	static bool ms_bDebugDrawLargeText;
#if STENCIL_VEHICLE_INTERIOR
	static bool ms_StencilOutInterior;
#endif // STENCIL_VEHICLE_INTERIOR
#endif

	static const u32 ms_MaxNumVehicleBreaksInWindow = 200; // GetNumVehicleBreaksPerSecond is limited by sm_MaxNumVehicleBreaksInWindow/m_fVehicleBreakWindowSize so it may be necessary to increase this if m_fVehicleBreakWindowSize increases
	static atQueue<u32,ms_MaxNumVehicleBreaksInWindow> ms_RecentVehicleBreakTimes;
	static float ms_VehicleBreaksPerSecond;
	static bank_float ms_MaxVehicleBreaksPerSecond;
	static bank_u32 ms_VehicleBreakWindowSize;
	static bank_u32 ms_uTimeToIgnoreButtonHeldDown;

	static bank_float sm_WheelDamageRadiusThreshold;
	static bank_float sm_WheelDamageRadiusThreshWheelDamageMax;
	static bank_float	sm_WheelDamageRadiusThreshWheelDamageScale;
	static bank_float sm_WheelDamageRadiusScale;
	static bank_float sm_WheelSelfRadiusScale;
	static bank_float sm_WheelHeavyDamageRadiusScale;
	static bank_float sm_WheelSelfHeavyWheelRadiusScale;

	static bank_float sm_HairScaleDisableThreshold;

	static void ForceActualVehicleLightsOff(bool value) { ms_forceActualVehicleLightsOff = value; }
	
	//Called from CPhysics::SetGravitationalAcceleration();
	static void SetGravitationalAcceleration(const float fGravitationalAcceleration);
    static void SetGravityScale(float fGravityScale);
    static void ResetGravityScale();

	static float ms_fGravityScale;

    static RegdVeh ms_requestedHdVeh;
    static u32 ms_lastTimeRequestedHd;

	static s8 ms_slotTurnedOff;
	static s32 ms_boneTurnedOff;

	static bool ms_CanEnterLockedForSpecialEditionVehicles;
	static bool ms_DisableHDVehicles;
	static bool	ms_bDialsScriptOveridden;
    
	struct sirenPriorityItem {
		CVehicle *vehicle;
		Vec3V vehPos;
		int priority;
	};
	
	static atFixedArray<sirenPriorityItem, 16> sirenVehicleList;
	static atFixedArray<CBobbleHead, MAX_NUM_BOBBLE_HEADS> ms_bobbleHeads;
	
	static void CalcSirenPriorities();

	static void PreVisUpdate();

	static u32 ms_insideVehicleMode;

	static atHashString ms_speedBoostAirScreenEffect;
	static atHashString ms_speedBoostScreenEffect;
	static atHashString ms_slowDownScreenEffect;

	static bool ms_DisableSlowDownEffect;
	static bool ms_DisableSpeedBoostEffect;

	static ObjectId		ms_VehicleIdToLookFor;
	static u32			ms_iSlipstreamingVehiclesFound;

	//online tunable for bikes camera collision flag
	static bool		ms_enableBikesCameraCollisionFlag;

	static float	ms_rampCarCollisionDamageScale;
	static float	ms_scoopCarCollisionDamageScale;

	static bool		ms_modifyPhantomCollisionWithPeds;

	static bool		ms_enablePatrolBoatSingleTimeSliceFix;

	static const float ROCKET_BOOST_RECHARGE_RATE;
	static const float ROCKET_BOOST_RECHARGE_RATE_PLANE;
	static const float SIDE_SHUNT_DURATION;

	static float sfPitchStabilisationMult;
	static float sfRollStabilisationMult;

public:
	static void SetFormationLeader( CVehicle* pLeader, Vector3& offset, float radius )
	{ 
		ms_formationLeader = pLeader;
		ms_formationOffset = offset;
		ms_formationRadius = radius;
	}
	static void ResetFormationLeader() { ms_formationLeader = NULL; }

	static void SetUseBoostButtonForWheelRetract( bool useBoost ) { ms_useBoostButtonForRetract = useBoost; }

private:
	static CVehicle*	ms_formationLeader;
	static Vector3		ms_formationOffset;
	static float		ms_formationRadius;

	static bool			ms_useBoostButtonForRetract;

	// Ruiner 2 weapon animations
	float m_fCurrentWeaponGrillOffset;
	float m_fCurrentWeaponRackOffset;

	bool m_bWheeliesEnabled; // Flag for script to disable/enable wheelies on bikes/quadbikes/trikes

public:
	bool GetWheeliesEnabled() { return m_bWheeliesEnabled; }
	void SetWheeliesEnabled( bool bNewValue ) { m_bWheeliesEnabled = bNewValue; }

	void SetSpecialFlightModeRatio( float modeRatio ) { m_specialFlightModeRatio = modeRatio; }
	void SetSpecialFlightModeTargetRatio( float modeRatio ) { m_specialFlightModeTargetRatio = modeRatio; }
	float GetSpecialFlightModeTargetRatio() { return m_specialFlightModeTargetRatio; }

	bool GetSpecialFlightModeAllowed()	{ return m_allowSpecialFlightMode; }
	void SetSpecialFlightModeAllowed( bool allowSpecialFlight ) { m_allowSpecialFlightMode = allowSpecialFlight; }

	// Slides a part of the vehicle attached to the bone to a desired position
	// Params:
	// iBoneIndex - Bone that controls the part we want to slide
	// fDesiredOffset - Offset to which we want to slide
	// rfCurrentOffset - Current offset of the mechanical part
	// vSlideDirection - the axis on which the part slides (in local space)
	// fMovementSpeed - speed at which to move (default is 1.0f)
	void SlideMechanicalPart(int iBoneIndex, float fDesiredOffset, float& rfCurrentOffset, Vector3 vSlideDirection, float fMovementSpeed = 1.0f);
    void OffsetBonePosition( int iBoneIndex, float fDesiredOffset, Vector3 vOffsetDirection );
	void RotateMechanicalPart(int iBoneIndex, float fDesiredOffset, float& rfCurrentOffset, eRotationAxis axis, float fMovementSpeed = 1.0f);

	void ProcessJetWash();

	bool IsWeaponBladeImpact( phCachedContactIterator& impacts );

	void SetIncreaseWheelCrushDamage( bool increaseDamage ) { m_bIncreaseWheelCrushDamage = increaseDamage;	}
	bool GetIncreaseWheelCrushDamage() { return m_bIncreaseWheelCrushDamage; }

	float GetDownforceModifierFront() { return m_fDownforceModifierFront; }
	float GetDownforceModifierRear() { return m_fDownforceModifierRear; }
	void  SetDownforceModifierFront(float val) { m_fDownforceModifierFront = val; }
	void  SetDownforceModifierRear(float val) { m_fDownforceModifierRear = val; }

#if !__NO_OUTPUT
	void LogDeletionFailReason(const char *pReasonString, const bool bLogClearCarFromAreaFailures) const;
#endif	//	!__NO_OUTPUT

	enum VEHICLE_WEAPON_BLADES
	{
		VWB_REAR = 0,
		VWB_LEFT,
		VWB_RIGHT,
		VWB_FRONT,

		MAX_NUM_WEAPON_BLADES,
	};

private:
	// weapon blades
	int m_numWeaponBlades;
	CWeaponBlade m_weaponBlades[ MAX_NUM_WEAPON_BLADES ];

public:
	void ApplyExplosionEffectEMP(u32 weaponHash = 0);
	void ApplyExplosionEffectSlick() { m_bExplosionEffectSlick = true; m_uLastHitByExplosionEffectSlick = fwTimer::GetTimeInMilliseconds(); }
	void ApplyExplosionEffectSlowdown( Vector3 explosionPosition, float explosionRadius );

	bool GetExplosionEffectEMP() const { return m_bExplosionEffectEMP; }
	bool GetExplosionEffectSlick() const { return m_bExplosionEffectSlick; }

	bool GetWeaponModHasElectricEffect() const { return m_bWeaponModHasElectricEffect; }
	u8 GetNumSequentialSirenPresses() const { return m_uNumSequentialSirenPresses; }
	void ResetNumSequentialSirenPresses() { m_uNumSequentialSirenPresses = 0u; }

private:
	void ProcessExplosionEffects();

	bool m_bExplosionEffectEMP;
	bool m_bExplosionEffectSlick;
	bool m_bIncreaseWheelCrushDamage;
	bool m_bWeaponModHasElectricEffect;
	bool m_bSirenSwitchPending;
	bool m_bUseIncreasedEMPDuration;
	u8 m_uNumSequentialSirenPresses;

	u32 m_uLastHitByExplosionEffectEMP;
	u32 m_uLastHitByExplosionEffectSlick;
};

template<> inline CVehicle* fwPool<CVehicle>::GetSlot(s32 index)
{
	CVehicle* veh = (CVehicle*)fwBasePool::GetSlot(index);
	if (!veh || veh->GetIsInPopulationCache())
		return NULL;

	return veh;
}

template<> inline const CVehicle* fwPool<CVehicle>::GetSlot(s32 index) const
{
	CVehicle* veh = (CVehicle*)fwBasePool::GetSlot(index);
	if (!veh || veh->GetIsInPopulationCache())
		return NULL;

	return veh;
}

SuspensionType GetSuspensionTypeFromFlags(int iModelFlags, bool bRearSuspension);
void SetSuspensionTransformation(crSkeleton* pSkeleton, eHierarchyId compId, eHierarchyId wheelId, SuspensionType nType, CVehicleStructure* pStructure, CHandlingData *pHandlingData, const Vec3V *pDeformation = NULL);
void SetTransmissionRotation(crSkeleton* pSkeleton, eHierarchyId compId, eHierarchyId wheelIdL, eHierarchyId wheelIdR, CVehicleStructure* pStructure);
void SetTransmissionRotationForTrike(crSkeleton* pSkeleton, eHierarchyId compId, eHierarchyId wheelIdL, CVehicleStructure* pStructure);

// ------------------------------------------------------------------------------


#ifdef DEBUG
	void AssertVehiclePointerValid(CVehicle* pVeh);
#else
	#define AssertVehiclePointerValid( A )
#endif

#ifdef DEBUG
	void AssertVehiclePointerValid_NotInWorld(CVehicle* pVeh);
#else
	#define AssertVehiclePointerValid_NotInWorld( A )
#endif

bool IsVehiclePointerValid(CVehicle* pVeh);
bool IsVehiclePointerValid_NotInWorld(CVehicle* pVeh);




//
// name:		GetPosition
// description:	Get position from matrix (should exist for vehicle)
/*inline const Vector3& CVehicle::GetPosition() const 
{
	Assert(m_pMat);
	return m_pMat->GetVector(3);
}*/

//
// name:		GetPosition
// description:	Get position from matrix (should exist for vehicle)
/*inline Vector3& CVehicle::GetPosition() 
{
	Assert(m_pMat);
	return *(Vector3*)&m_pMat->GetVector(3);
}*/

#endif // _VEHICLE_H_


// VEHICLE MOD TO WEAPONS
//Standard
//VMT_CHASSIS		Body Armor
//VMT_ROOF			Roof Spikes
//VMT_BUMPER_F		Front Bumper
//VMT_BUMPER_R		Rear Bumper
//VMT_SKIRT			Side Skirts
//VMT_BONNET		Bonnet
//VMT_GRILL			Grill
//VMT_SPOILER		Spoiler
//VMT_EXHAUST		Exhaust
//VMT_WING_L
//
//Supermod
//VMT_PLTHOLDER
//VMT_PLTVANITY
//VMT_INTERIOR1		Bobblheads
//VMT_INTERIOR2		Rollcage
//VMT_INTERIOR3
//VMT_INTERIOR4
//VMT_INTERIOR5
//VMT_SEATS			Seats
//VMT_STEERING		Steering Wheel
//VMT_KNOB			Gear Knobs
//VMT_PLAQUE
//VMT_ICE
//VMT_TRUNK			Rear Mounted Grenade Launcher ( issi & interceptor )
//VMT_HYDRO
//VMT_ENGINEBAY1
//VMT_ENGINEBAY2	NITROUS	Phyical model + Stat
//VMT_ENGINEBAY3	JUMP	Phyical model + Stat
//VMT_CHASSIS2		Front mountd mods - Scoop S / M / L, Ram Bar S / M / L, Spikes ( Front )
//VMT_CHASSIS3		Spikes ( body )
//VMT_CHASSIS4		Side & Rear Blades
//VMT_CHASSIS5		Fixed Forward Machine Gun
//VMT_DOOR_L
//VMT_DOOR_R
//VMT_LIVERY_MOD		Livery
