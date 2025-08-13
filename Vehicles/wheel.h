//
//
//
//
#ifndef _WHEEL_H_
#define _WHEEL_H_

// Rage headers
#include "math/amath.h"
#include "phcore/materialmgr.h"
#include "phcore/segment.h"
#include "vector/vector3.h"
#include "rmcore/instance.h"
#include "physics/intersection.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwtl/pool.h"
#include "fwutil/Flags.h"

// Game headers
#include "Control/Replay/ReplaySettings.h"
#include "Modelinfo/VehicleModelInfo.h"
#include "Renderer/HierarchyIds.h"
#include "Scene/RegdRefTypes.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/Systems/VfxLiquid.h"
#include "Weapons/WeaponEnums.h"

namespace rage {
	class phArticulatedBodyPart;
}

// Forward declarations:
namespace WorldProbe
{
	class CShapeTestHitPoint;
}

class CBike;
class CHandlingData;
class CPed;
class CPhysical;
class CVehicle;
class CVehicleNodeList;
class CVehicleStructure;
class CRoutePoint;
class CTrailer;
struct VfxWheelInfo_s;

#define SPU_WHEEL_INTEGRATION (1 && (__PS3))

#define APPLY_GRAVITY_IN_INTEGRATOR	(1)

#define LARGEST_PHBOUND phBoundBox

enum eScriptWheelList
{
	SC_WHEEL_CAR_FRONT_LEFT = 0,
	SC_WHEEL_CAR_FRONT_RIGHT,
	SC_WHEEL_CAR_MID_LEFT,
	SC_WHEEL_CAR_MID_RIGHT,
	SC_WHEEL_CAR_REAR_LEFT,
	SC_WHEEL_CAR_REAR_RIGHT,
	SC_WHEEL_BIKE_FRONT,
	SC_WHEEL_BIKE_REAR,
	SC_WHEEL_NUM
};

// JP: Split wheel flags into 2 so we have room for more if needed
// In general the wheel flags change at runtime whilst the config flags stay constant
enum eWheelFlags
{
	// state flags
	WF_HIT				= BIT(0),
	WF_HIT_PREV			= BIT(1),
	WF_ON_GAS			= BIT(2),
	WF_ON_FIRE			= BIT(3),

	// Cheat flags
	WF_CHEAT_TC			= BIT(4),
	WF_CHEAT_SC			= BIT(5),
	WF_CHEAT_GRIP1		= BIT(6),
	WF_CHEAT_GRIP2		= BIT(7),

	// runtime flags
	WF_BURNOUT						= BIT(8),
	WF_BURNOUT_NON_DRIVEN_WHEEL		= BIT(9),
	WF_INSHALLOWWATER				= BIT(10),
	WF_INDEEPWATER					= BIT(11),
	WF_TYRES_HEAT_UP				= BIT(12),
	WF_ABS_ACTIVE					= BIT(13),

	// Abs modes
	// Set every frame by the vehicle so they are classed as runtime flags
	// This is because A.I. Drivers have ABS always on
	WF_ABS							= BIT(14),
	WF_ABS_ALT						= BIT(15),

	WF_SQUASHING_PED				= BIT(16),	
	WF_REDUCE_GRIP  				= BIT(17),

	// when a vehicle is teleported its wheel contact positions aren't teleported with it 
	// this can cause issues when relocating particle effects so we set a flag to stop wheel vfx being updated this frame
	WF_TELEPORTED_NO_VFX			= BIT(18),

	WF_RESET             			= BIT(19),
	WF_BROKEN_OFF                   = BIT(20),

	WF_FULL_THROTTLE				= BIT(21),
	WF_SIDE_IMPACT					= BIT(22),
	WF_DUMMY_TRANSITION				= BIT(23),
	WF_DUMMY_TRANSITION_PREV		= BIT(24),

	WF_NO_LATERAL_SPRING			= BIT(25),

	WF_WITHIN_DAMAGE_REGION			= BIT(26),
	WF_WITHIN_HEAVYDAMAGE_REGION	= BIT(27),
	WF_TOUCHING_PAVEMENT			= BIT(28),

	WF_DUMMY						= BIT(29),

	WF_FORCE_NO_SLEEP				= BIT(30), 
	WF_SLEEPING_ON_DEBRIS			= BIT(31)
};

enum eWheelConfigFlags
{
	// configuration flags
	WCF_BIKE_WHEEL										= BIT(0),
	WCF_LEFTWHEEL										= BIT(1),
	WCF_REARWHEEL										= BIT(2),
	WCF_STEER											= BIT(3),
	WCF_POWERED											= BIT(4),
	WCF_TILT_INDEP										= BIT(5),
	WCF_TILT_SOLID										= BIT(6),
	WCF_BIKE_CONSTRAINED_COLLIDER						= BIT(7),
	WCF_BIKE_FALLEN_COLLIDER							= BIT(8),	
	WCF_INSTANCED										= BIT(9),
	WCF_DONT_RENDER_STEER								= BIT(10),	// Dont render the wheel steering
	WCF_UPDATE_SUSPENSION								= BIT(11),	// Update suspension every frame	
	WCF_QUAD_WHEEL										= BIT(12),
	WCF_HIGH_FRICTION_WHEEL								= BIT(13),
	WCF_DONT_REDUCE_GRIP_ON_BURNOUT						= BIT(14), // Used for low powered vehicles
	WCF_IS_PHYSICAL										= BIT(15), // Used for wheels that are children of articulated joints
	WCF_BICYCLE_WHEEL									= BIT(16),
	WCF_TRACKED_WHEEL									= BIT(17),
	WCF_PLANE_WHEEL										= BIT(18),
	WCF_DONT_RENDER_HUB									= BIT(19),
	WCF_SPOILER											= BIT(20),  // Has the vehicle got a spoiler mod.
	WCF_ROTATE_BOUNDS									= BIT(21),
	WCF_EXTEND_ON_UPDATE_SUSPENSION						= BIT(22),	// Used to force wheels to extend when updating the suspension
	WCF_CENTRE_WHEEL									= BIT(23),	// The three wheeled cars have a centre wheel
	WCF_AMPHIBIOUS_WHEEL								= BIT(24),
	WCF_RENDER_WITH_ZERO_COMPRESSION					= BIT(25)
	// NOTE: If you add another flag (beyond 31) you need to increase the size of CWheel::m_nConfigFlags
};

enum eWheelHydraulicState
{
	WHS_INVALID = -1,
	WHS_IDLE,

	WHS_LOCK_START,
	WHS_LOCK = WHS_LOCK_START,
	WHS_LOCK_UP_ALL,
	WHS_LOCK_UP,
	WHS_LOCK_DOWN_ALL,
	WHS_LOCK_DOWN,

	WHS_LOCK_END = WHS_LOCK_DOWN,

	WHS_BOUNCE_START,
	WHS_BOUNCE_LEFT_RIGHT = WHS_BOUNCE_START,
	WHS_BOUNCE_FRONT_BACK,
	WHS_BOUNCE,
	WHS_BOUNCE_ALL,

	WHS_BOUNCE_END = WHS_BOUNCE_ALL,

	WHS_BOUNCE_FIRST_ACTIVE,

	WHS_BOUNCE_ACTIVE = WHS_BOUNCE_FIRST_ACTIVE,
	WHS_BOUNCE_LANDING,
	WHS_BOUNCE_LANDED,

	WHS_BOUNCE_ALL_ACTIVE,
	WHS_BOUNCE_ALL_LANDING,

	WHS_FREE,

	WHS_BOUNCE_LAST_ACTIVE = WHS_FREE,
};

#define VEHICLE_USE_INTEGRATION_FOR_WHEELS 1

#define WF_DYNAMIC_STATE	(WF_HIT|WF_ON_GAS)
#define WF_RESET_STATE		(WF_HIT|WF_HIT_PREV)
#define WF_RESET_CHEATS		(WF_CHEAT_TC|WF_CHEAT_SC|WF_CHEAT_GRIP1|WF_CHEAT_GRIP2)

#define MAX_WHEEL_BOUNDS_PER_WHEEL (3) 
#define WHEEL_CLIMB_BOUND (2) 
#define TRACTION_TABLE_ENTRIES (10)

#define SUSPENSION_HEALTH_DEFAULT		(1000.0f)
#define SUSPENSION_HEALTH_DEFAULT_INV	(1.0f / SUSPENSION_HEALTH_DEFAULT)
#define TYRE_HEALTH_DEFAULT				(1000.0f)
#define TYRE_HEALTH_DEFAULT_INV			(1.0f / TYRE_HEALTH_DEFAULT)
#define TYRE_HEALTH_HIGH_GRIP		(850.0f)
#define TYRE_HEALTH_MEDIUM_GRIP		(450.0f)

#define TYRE_HEALTH_FLAT				(350.0f) 
#define TYRE_HEALTH_FLAT_ADD			(0.0001f)

#if VEHICLE_USE_INTEGRATION_FOR_WHEELS
	struct WheelIntegrationTimeInfo
	{
		float fProbeTimeStep;
		float fProbeTimeStep_Inv;
		float fIntegrationTimeStep;
		float fIntegrationTimeStep_Inv;
		float fTimeInIntegration;
		float fTimeInIntegration_Inv;
	};
#endif

class CWheel
{
	friend class CVfxWheel;

#if GTA_REPLAY
	friend class CPacketVehicleUpdate;
	friend class CPacketVehicleWheelUpdate_Old;
	friend class CWheelFullData;
	friend class ReplayWheelValues;
#endif // GTA_REPLAY

public:
	CWheel();
	~CWheel();

	FW_REGISTER_CLASS_POOL(CWheel);

	void Init(CVehicle* pParentVehicle, eHierarchyId wheelId, float fRadius, float fForceMult, int nConfigFlags, s8 oppositeWheelIndex = -1);
	void InitOnSpu(CHandlingData* pHandling);
	CHandlingData* GetHandlingData() const {return m_pHandling;}

	// call this when vehicle is created, and every time vehicle is damaged
	void SuspensionSetup(const void* damageTexturePtr);
	// call this to get transformed probes before doing test
	bool ProbeGetTransfomedSegment(const Matrix34* pMatrix, phSegment& pProbeSegment);
	// call this after test to deal with results
	bool ProbeProcessResults(const Matrix34* pMatrix, WorldProbe::CShapeTestHitPoint& probeResults);
	// set compression from place properly on road
	void SetCompressionFromHitPos(const Matrix34* pParentMat, const Vector3& vecPos, bool bIsAHit, const Vector3& vecNormal = ZAXIS);
	void SetCompressionFromGroundHeightLocalAndHitPos(float fGroundHeightLocal, Vector3::Vector3Param vHitPos, Vector3::Vector3Param vHitNormal);
	void ResetCompressionFromPrev() { m_fCompression = m_fCompressionOld; }

	void UpdateSuspension(const void* pDamageTexture, bool bSyncSkelToArtBody = true);

	// called from ProcessPreComputeImpacts to intercept impacts for use in suspension
	void ProcessImpactResults();

	// PURPOSE:	Used with ProcessImpact(), to hold data that can be computed
	//			about the parent vehicle and can be shared between the wheels
	//			to avoid recomputation.
	struct ProcessImpactCommonData
	{
		explicit ProcessImpactCommonData(const CVehicle& rParentVehicle);

		float GetVelocitySq() const { return m_ComputedValues.GetXf(); }
		float GetTreadWidthMultIfNotBikeWheel() const { return m_ComputedValues.GetYf(); }
		ScalarV_Out GetTreadWidthMultIfNotBikeWheelV() const { return m_ComputedValues.GetY(); }
		int GetIsStill() const { return m_ComputedValues.GetWi(); }

		Vec3V			m_Velocity;

		// X:	fVelocitySq;
		// Y:	fTreadWidthMultIfNotBikeWheel
		// Z:	bDriveOverVehicle
		// W:	bIsStill
		Vec4V			m_ComputedValues;
		const CPed*		m_pDriver;
		const CTrailer*	m_pAttachedTrailer;
		bool			m_bPlayerVehicle;
	};
	void ProcessImpact(const ProcessImpactCommonData& commonData, phCachedContactIterator &impacts, const atUserArray<CVehicle*> &aVehicleFlattenNormals);

	void SetWheelHit(float fCompression, Vec3V_In vHitPos, Vec3V_In vHitNormal, const bool bSetToDefaultMaterial = true);
    bool ProcessImpactPopTires( phCachedContactIterator& impacts, const CEntity & rOtherEntity);
	void ProcessPreSimUpdate( const void *basePtr, phCollider* vehicleCollider, Mat34V_In vehicleMatrix, float fWheelExtension);

	//Helper function to see if impact is on the side of the wheel
	//	bool ImpactIsOnSide(CVehicle * pParentVehicle, phCachedContactConstIterator& impact) const;

	// process forces from this wheel
	void ProcessAsleep( const Vector3& parentVelocity, const float fTimeStep, const float fGravityForce, bool updateWheelAngVelFromVehicleVel);
	bool WantsToBeAwake() {return (m_fTyreHealth < TYRE_HEALTH_DEFAULT && m_fTyreHealth > TYRE_HEALTH_FLAT + TYRE_HEALTH_FLAT_ADD);}
	void UpdateCompressionOnDeactivation();

	//Called to make sure we clear audio effects when wheels are no longer processed but the vehicle has active audio
	void ProcessLowLODAudio();

	void CalculateTractionVectors(const Matrix34& matParent, Vector3& vecFwd, Vector3& vecSide, Vector3& vecHitDelta, float fSteerAngle);
	void CalculateTractionVectorsNew( const Matrix34& matParent, Vector3& vecFwd, Vector3& vecSide, Vector3& vecHitDelta, float fSteerAngle, Vector3 localSpeed );//, Vector3 angVelocity, Vector3 linearVelocity );

	float GetTractionCoefficientFromRatio(float fRatio, float fMult, float fLoss);

#if VEHICLE_USE_INTEGRATION_FOR_WHEELS
	void IntegrateSuspensionAccel(phCollider* pVehCollider, Vector3& vecForceResult, Vector3& vecAccelResult, Vector3& vecAngAccelResult, float& fSuspensionForce, float& compressionDelta, const Matrix34& mat, const Vector3& vecVel, const WheelIntegrationTimeInfo& timeInfo, const CWheel *pOppositeWheel, const float fGravity);
	void IntegrateTyreAccel(phCollider* pVehCollider, Vector3& vecForceResult, Vector3& vecAccelResult, Vector3& vecAngAccelResult, Vector3& vecFwd, Vector3& vecSide, float& fWheelAngSlipDtResult, float fSuspensionForce, const Matrix34& mat, const Vector3& vecVel, const Vector3& vecAngVel, float fWheelAngSlip, const WheelIntegrationTimeInfo& timeInfo, const CWheel *pOppositeWheel, const float fGravity);

	float IntegrateCalculateForces(phCollider* pVehCollider, Vector3& vecForceResult, float fSpeed, float fSpeedAlongFwd, float fSpeedAlongSide, float fSpringAlongFwd, float fSpringAlongSide, float fRotSlipRatio, float fBrakeForce, float fDriveForce, float fLoadForce, float fIntegrationTimeStep, const float fGravity);
	float IntegrateCalculateForceLimits(float fSpeedAlongFwd, float fSpeedAlongSide, float fSpringAlongFwd, float fSpringAlongSide, float fAngVel, float& fFwdForceLimit, float& fSideForceLimit, float fTimeRatio);

	void IntegrationPostProcess(phCollider* pVehCollider, float fWheelAngSlip, float fMaxVelInGear, int nGear, float fTimeStep, unsigned frameCount, Vector3& vecPushToSide, Vector3& vecFwd, Vector3& vecSide, const float fGravity);
#if !__SPU
	void IntegrationFinalProcess(phCollider* pVehCollider, ScalarV_In timeStep);//This process is done after the integrator thread was finished.
#endif // !__SPU
	float GetMassAlongVector(phCollider* pVehCollider, const Vector3& vecNormal, const Vector3& vecWorldPos);

	// Update the m_fCompression during the integration loop
	// Either using a shapetest or using predicted hit points
	void UpdateCompressionInsideIntegrator(const Matrix34& matUpdated, const phCollider* pCollider, float& compressionDelta);
#endif

	void UpdateContactsAfterNetworkBlend(const Matrix34& matOld, const Matrix34& matNew);

	void ProcessWheelMatrixForAutomobile(crSkeleton & rSkeleton);
	void ProcessAnimatedWheelDeformationForAutomobile(crSkeleton & rSkeleton);
	void ProcessWheelMatrixForBike();
	void ProcessWheelMatrixForTrike();
	void ProcessWheelMatrixCore(crSkeleton & rSkeleton, CVehicleStructure & rStructure, CWheel * const * ppWheels, int numWheels, bool bAnimated, float fSteerAngle);
	void ProcessWheelDeformationMatrixCore(crSkeleton & rSkeleton, CVehicleStructure & rStructure, bool bAnimated);
	void ProcessWheelScale(crSkeleton* pSkeleton, CVehicleStructure* pStructure, CWheel * const * ppWheels);

	void ProcessAudio();
	void ProcessVFx(bool bContainsLocalPlayer);
	void ProcessPedCollision();
	void ProcessWheelVFxDry( CVfxVehicleInfo* pVfxVehicleInfo, VfxGroup_e mtlVfxGroup, Vec3V_In vVfxVecHitPos, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxGroundSpeedVal, float currRoadWetness, float distSqrToCam);
	void ProcessWheelVFxWet( CVfxVehicleInfo* pVfxVehicleInfo, VfxGroup_e mtlVfxGroup, Vec3V_In vVfxVecHitPos, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxGroundSpeedVal, float currRoadWetness, bool inWater, float waterZ, float distSqrToCam);
	void ProcessWheelVFx( CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vVfxVecHitPos, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxGroundSpeedVal, float wetEvo, bool isWet, bool inWater, float waterZ, float distSqrToCam);
	void ProcessSkidmarkVFx( CVfxVehicleInfo* pVfxVehicleInfo, VfxGroup_e mtlVfxGroup, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxPressureVal, float currRoadWetness, float distSqrToCam);
	void ProcessTyreTemp(const Vector3& vecVelocity, float fTimestep);
    void UpdateTyreWear( float fTimeStep );

	void SetSteerAngle(float fAngle);
	void SetBrakeAndDriveForce(const float fForce, const float fDriveForce, const float fThrottle, const float fSpeedSqr );
	void SetBrakeForce( const float fForce ) { m_fBrakeForce = fForce; }
	void SetDriveForce( const float fForce ) { m_fDriveForce = fForce;	}

	void SetHandBrakeForce(float fForce);
	inline void UpdateAbsTimer(float fTimeStep) {m_fAbsTimer = rage::Max(0.0f, m_fAbsTimer - fTimeStep);}

#if !__SPU
	void ApplyTireForceToPed(CPed *pPed, phCollider *pVehCollider, u16 hitComponent);
#endif // !__SPU

	bool GetIsTouching() const {return m_nDynamicFlags.IsFlagSet(WF_HIT);}
	bool GetWasTouching() const {return m_nDynamicFlags.IsFlagSet(WF_HIT_PREV);}
	bool GetSideImpact() const {return m_nDynamicFlags.IsFlagSet(WF_SIDE_IMPACT);}
    bool GetWasReset() const {return m_nDynamicFlags.IsFlagSet(WF_RESET);}
	bool GetIsDriveWheel() const {return m_nConfigFlags.IsFlagSet(WCF_POWERED);}
	const Vector3& GetHitPos() const { return m_vecHitPos; }
	void ResetHitPos() { m_vecHitPos = VEC3_ZERO; }
	const Vector3& GetHitNormal() const { return m_vecHitNormal; }
	u16 GetHitComponent() const { return m_nHitComponent; }
	CPhysical* GetHitPhysical() {return (m_nDynamicFlags &WF_HIT) ? m_pHitPhysical.Get() : NULL;}
	const CPhysical* GetHitPhysical() const {return (m_nDynamicFlags &WF_HIT) ? m_pHitPhysical.Get() : NULL;}
	const CPhysical* GetPrevHitPhysical() const {return m_pPrevHitPhysical.Get();}
	CPhysical* GetPrevHitPhysical() {return m_pPrevHitPhysical.Get();}
	float GetGroundSpeed() const {return -m_fRotAngVel*m_fWheelRadius;}
	const Vector3 *GetGroundBeneathWheelsVelocity() const {return &m_vecGroundVelocity;}
	float GetRotSpeed() const {return m_fRotAngVel;}
    void SetRotSpeed( float rotAngVel ) { m_fRotAngVel = rotAngVel; } 
	float GetRotSlipRatio() const {return m_fRotSlipRatio;}
    void SetRotSlipRatio( float rotSlipRatio ) { m_fRotSlipRatio = rotSlipRatio; }
	inline void SetRotAngle(float fRotAngle) { m_fRotAng = fwAngle::LimitRadianAngleFast(fRotAngle); }
	float GetRotAngle() { return m_fRotAng; }

	void ScaleTyreContactVelocity( float scale ) { m_vecTyreContactVelocity *= scale; }

	phMaterialMgr::Id GetMaterialId() const {return m_nHitMaterialId;}
	
	void Reset(bool bExtend=false);
	void Teleport() {GetDynamicFlags().SetFlag(WF_TELEPORTED_NO_VFX);}

	fwFlags32& GetDynamicFlags() {return m_nDynamicFlags;}
	fwFlags32& GetConfigFlags() {return m_nConfigFlags;}
	const fwFlags32& GetDynamicFlags() const {return m_nDynamicFlags;}
	const fwFlags32& GetConfigFlags() const {return m_nConfigFlags;}
	void SetMaterialFlags();

	const phSegment& GetProbeSegment() const {return m_aWheelLines;}

	float GetFwdSlipAngle() const {return m_fFwdSlipAngle;}
	float GetSideSlipAngle() const {return m_fSideSlipAngle;}
	float GetCamberForce() const {return m_fTyreCamberForce;}
	float GetEffectiveSlipAngle() const {return m_fEffectiveSlipAngle;}
	const float& GetEffectiveSlipAngleRef() const { return m_fEffectiveSlipAngle; }
	float GetFrictionDamage() const { return m_fFrictionDamage; }

	float GetCompression() const {return m_fCompression;}
	float GetCompressionChange() const {return m_fCompression - m_fCompressionOld;}
	float GetMaxTravelDelta() const {return m_fMaxTravelDelta;}
	float GetSuspensionLength() const {return m_fSusLength;}

	float GetWheelCompression() const {return m_fWheelCompression;}
	float GetCalculatedWheelCompression();
	float GetWheelCompressionIncBurstAndSink() const;
	float GetWheelCompressionFromStaticIncBurstAndSink() const {return GetWheelCompressionIncBurstAndSink() - m_fStaticDelta;}
	float GetStaticDelta() const { return m_fStaticDelta; }

	float GetFrictionDamageScaledBySpeed(const Vector3& vVelocity);

	const Vector3 & GetSuspensionAxis() const { return m_vecSuspensionAxis; }

	eHierarchyId GetHierarchyId() const { return m_WheelId;}
	s16 GetOppositeWheelIndex() const {return m_nOppositeWheelIndex;}
	void SetOppositeWheelIndex( s8 oppositewheelIndex ) {m_nOppositeWheelIndex = oppositewheelIndex;}
	int GetFragChild(int iChildIndex = 0) const {return m_nFragChild[iChildIndex];}

	inline CVfxDeepSurfaceInfo& GetDeepSurfaceInfo() {return m_deepSurfaceInfo;}
	inline void SetWetnessInfo(const VfxLiquidType_e liquidType) {m_liquidAttachInfo.Set(VFXMTL_WET_MODE_WHEEL, liquidType);}

	float GetSteeringAngle() const { return m_fSteerAngle; }
	float GetBrakeForce() const { return m_fBrakeForce; }
	float GetDriveForce() const { return m_fDriveForce; }
	float GetDriveForceOld() const { return m_fDriveForceOld; }
    void ScaleDriveForce( float forceScale ) { m_fDriveForce *= forceScale;  }

	void ProcessWheelDamage(float fTimeStep);
	void SetWheelOnFire(bool bNetworkCheck = true);
	bool IsWheelOnFire() const;
	void ClearWheelOnFire();
	void ProcessWheelBurst();
	bool DoBurstWheelToRim();
	bool GetTyreShouldBurst(const Vector3& vecVelocity) const;
	bool GetTyreShouldBurstFromDamage() const;
	bool GetTyreShouldBreakFromDamage() const;

	void UpdateGroundVelocity();

	// get position of suspension, taken as original centre of wheel pos, returns radius for damage calculations
	float GetSuspensionPos(Vector3& vecPosResult);
	void ApplySuspensionDamage(float fDamage);
	float GetSuspensionHealth() const { return m_fSuspensionHealth; }
	void SetSuspensionHealth(float fHealth) { m_fSuspensionHealth = fHealth; }
	Vec3V_Out GetWheelStaticPositionLocal() const;

	float GetWheelPosAndRadius(Vector3& vecResult) const;
	float GetWheelWidth() const { return m_fWheelWidth; }
	float GetWheelRadius() const { return m_fWheelRadius; }
	ScalarV_Out GetWheelRadiusV() const { return LoadScalar32IntoScalarV(m_fWheelRadius); }
	float GetWheelRimRadius() const { return m_fWheelRimRadius; }
	void SetWheelRimRadius(float radius) { m_fWheelRimRadius = radius; }

	inline float GetWheelRadiusIncBurst() const
	{
		if(m_fTyreHealth <= 0.0f) 
			return m_fWheelRimRadius;
		else 
			return m_fWheelRimRadius + (m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV) * (m_fWheelRadius - m_fWheelRimRadius);
	}

	inline float GetTyreThickness() const
	{
		if(m_fTyreHealth <= 0.0f) 
			return 0;
		else 
			return (m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV) * (m_fWheelRadius - m_fWheelRimRadius);
	}

	void CalculateMaximumExtents(Mat34V_InOut lastWheelMatrixOut, Mat34V_InOut currentWheelMatrixOut, phCollider *pParentCollider = NULL);
	bool GetWheelMatrixAndBBox( Matrix34& matResult, Vector3& vecBoxMaxResult) const;
	void ApplyTyreDamage(const CEntity* pInflictor, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, eDamageType nDamageType, int nWheelIndex, bool bNetworkCheck = true);
	float GetTyreHealth() const {return m_fTyreHealth;}
	void SetTyreHealth(float health)
	{
		m_fTyreHealth = health;
		m_nDynamicFlags.ChangeFlag(WF_FORCE_NO_SLEEP,WantsToBeAwake());
	}
	float GetTyreBurstSideRatio() const;
	float GetTyreBurstRatio() const
	{
		if(m_fTyreHealth <= 0.0f) 
			return 1.0f;
		else if(m_fStaticDelta != 0.0f) 
			return rage::Max(0.0f, rage::Min(1.0f, m_fCompression / m_fStaticDelta)*(1.0f - m_fTyreHealth*TYRE_HEALTH_DEFAULT_INV));
		return 0.0f;
	}
	float GetTyreBurstCompression() const
	{
		if(m_fTyreHealth <= 0.0f) 
			return (m_fWheelRadius - m_fWheelRimRadius);
        else if(m_fTyreWearRate > 0.0f && m_fTyreHealth > TYRE_HEALTH_FLAT ) 
        {
            return 0.0f;
        }
		else if(m_fStaticDelta != 0.0f) 
			return rage::Max(0.0f, rage::Min(1.0f, m_fCompression / m_fStaticDelta)*(1.0f - m_fTyreHealth*TYRE_HEALTH_DEFAULT_INV)) * (m_fWheelRadius - m_fWheelRimRadius);
		return 0.0f;
	}
    float GetTyreWearRate() const
    {
        return m_fTyreWearRate;
    }
    void SetTyreWearRate( float wearRate );
    void SetWearRateScale( float wearRateScale ) { m_fWearRateScale = wearRateScale; }
    float GetWearRateScale() const { return m_fWearRateScale; }
    void SetMaxGripDiffFromWearRate( float gripDiff ) { m_fMaxGripDiffFromWearRate = gripDiff; }
    float GetMaxGripDiffFromWearRate() const { return m_fMaxGripDiffFromWearRate; }

	void UpdateTyreHealthFromNetwork(float fHealth);
	void ResetDamage();
	void PushToSide(phCollider* pVehCollider, const Vector3& vecPushDirn);
	
	bool GetIsFlat() const { return (m_fTyreHealth <= (TYRE_HEALTH_FLAT + TYRE_HEALTH_FLAT_ADD)); }

	void SetFrontRearSelector(float fFrontRearSelector) { m_fFrontRearSelector = fFrontRearSelector; }
	float GetFrontRearSelector() const { return m_fFrontRearSelector; }

	float GetMaterialGrip() const {return m_fMaterialGrip;}

	void SetStaticForce(float fForceMult) { 	m_fStaticForce = fForceMult; }
	float GetStaticForce() const { return m_fStaticForce; }	

	void SetGripMult(float fGripMult) { m_fGripMult = fGripMult; }
	float GetGripMult() { return m_fGripMult; }

	float GetTyreDrag() const { return m_fMaterialDrag; }

	float GetTyreTemp() const { return m_fTyreTemp; }

	void SetMassMultForAcceleration(float fMassMultForAccel) { Assert(fMassMultForAccel > 0.0f); m_fMassMultForAccelerationInv = (1.0f / fMassMultForAccel); }

	static eHierarchyId GetScriptWheelId(eScriptWheelList nWheel) {Assert(nWheel < SC_WHEEL_NUM); return ms_aScriptWheelIds[nWheel];}

	float GetWheelImpactOffset() const { return m_fWheelImpactOffset; }

	float GetTyreLoad() const {return m_fTyreLoad;}

	bool GetIsTouchingPavement() const { return GetDynamicFlags().IsFlagSet(WF_TOUCHING_PAVEMENT); }
	bool IsMaterialIdPavement(phMaterialMgr::Id materialId) const;
	bool IsMaterialIdTrainTrack(phMaterialMgr::Id materialId) const;
	bool IsMaterialIdStairs(phMaterialMgr::Id materialId) const;

#if !__SPU
	static Vector3 GetWheelOffset(CVehicleModelInfo* pVehicleModelInfo, eHierarchyId wheelId);
#endif // !__SPU

	bool GetHasDummyTransition() const { return GetDynamicFlags().IsFlagSet(WF_DUMMY_TRANSITION); }
	void EndDummyTransition() { GetDynamicFlags().ClearFlag(WF_DUMMY_TRANSITION); }
	void StartDummyTransition();
	void UpdateDummyTransitionCompression(float timeStep);

	bool HasInnerWheel() const;

	void SetSuspensionTargetRaiseAmount(float fSuspensionTargetRaise, float fAccelerationScale) { m_fSuspensionTargetRaiseAmount = fSuspensionTargetRaise; m_fSuspensionRaiseRate = fAccelerationScale; }
	void SetSuspensionRaiseAmount(float fSuspensionRaise) { m_fSuspensionRaiseAmount = fSuspensionRaise; m_fSuspensionTargetRaiseAmount = fSuspensionRaise; m_fSuspensionRaiseRate = 1.0f; }
	void SetSuspensionForwardOffset( float fForwardOffset ) { m_fSuspensionForwardOffset = fForwardOffset; }
	float GetSuspensionRaiseAmount() { return m_fSuspensionRaiseAmount; }
	float GetSuspensionTargetRaiseAmount() { return m_fSuspensionTargetRaiseAmount; }
	float GetSuspensionRaiseRate() { return m_fSuspensionRaiseRate; }
	eWheelHydraulicState GetSuspensionHydraulicState() { return m_WheelState; }
	eWheelHydraulicState GetSuspensionHydraulicTargetState() { return m_WheelTargetState; }
	void SetSuspensionHydraulicTargetState( eWheelHydraulicState targetState ) { m_WheelTargetState = targetState; }

	void UpdateHydraulics( int timeStep );
	bool UpdateHydraulicsFlags( CAutomobile* pAutomobile );

	void UpdateDownforce();

	void SetWheelInWater(bool bNewValue) { m_bIsWheelInWater = bNewValue; }

	void SetExtraWheelDrag( float extraDrag ) {	m_fExtraWheelDrag = extraDrag; }
	float GetExtraWheelDrag() {	return m_fExtraWheelDrag; }

private:
	friend class CWheelIntegrator;

#if __DEV && !__SPU
	void DrawForceLimits(float fCurrent, float fCurrentSpring, float fLoadForce, float fNormalLoad, bool bLateral, float fLoss=1.0f);
	void DrawForceLimitsImproved(float fCurrentLat, float fCurrentLong, float fCurrentSpring, float fLoadForce, float fNormalLoad, float fLoss=1.0f);
#endif

#if __DEV
	// This doesn't iterate.. it just draws the current impact's stuff
	void DebugDrawSingleImpact(phCachedContactConstIterator& impact);
	void DrawWheelImpact(const CVehicle & rParentVehicle, phCachedContactIterator & impacts, Color32 col1, Color32 col2, Color32 col3) const;
#endif

	// Calculates last and current matrices in space of vehicle
	// Note if wheels are not children of root bone then these matrices will need transforming into their parent's bone space
	void CalcWheelMatrices(Mat34V_InOut lastWheelMatrixOut, Mat34V_InOut currentWheelMatrixOut, phCollider *pParentCollider);

	float GetReduceGripMultiplier();

	/////////////////
	// vector description of this wheel
	Vector3 m_vecAxleAxis;			// axle axis vector (along right vector) without steering effect
	Vector3 m_vecSuspensionAxis;	// line of suspension travel
	phSegment m_aWheelLines;	// Min / Max of suspension probe lines;

	////////////////
	// intersection state
	//phIntersection m_aWheelIntersections[NUM_WHEEL_PROBES];	// intersection point with ground
	Vector3 m_vecHitPos;
	Vector3 m_vecHitCentrePos;
	Vector3 m_vecHitCentrePosOld;			// previous intersection position
	Vector3 m_vecHitNormal;

	Vector3 m_vecHitPhysicalOffset;

	Vector3 m_vecGroundVelocity;

	Vector3 m_vecSolverForce;

	////////////////
	// saved stuff used for particles and audio
	Vector3 m_vecGroundContactVelocity;
	Vector3 m_vecTyreContactVelocity;

	////////////////
	// Vfx
	CVfxDeepSurfaceInfo m_deepSurfaceInfo;
	CVfxLiquidAttachInfo m_liquidAttachInfo;
	float m_fOnFireTimer;

	////////////////
	// timers
	u32 m_uLastPopTireCheck;

	////////////////
	// Dummy transition surface (for dummy->real)
	float m_fPreviousTransitionCompression;
	float m_fTransitionCompression;

	////////////////
	// config vars
	eHierarchyId m_WheelId : 16;			// which bone in the vehicle skeleton is this wheel attached to
	s8 m_nFragChild[MAX_WHEEL_BOUNDS_PER_WHEEL];
	s8 m_nOppositeWheelIndex;	//index into m_ppWheel array for opposite wheel, -1 for no opposite wheel. Used for the antirollbar
	float m_fWheelRadius;			// radius of this wheel
	float m_fWheelRimRadius;
    float m_fInitialRimRadius;
	float m_fWheelWidth;
	CHandlingData* m_pHandling;
    CVehicle*      m_pParentVehicle;

	float m_fSusLength;
	float m_fMaxTravelDelta;
	float m_fStaticDelta;			// compression ratio vehicle should rest at
	float m_fStaticForce;
	float m_fFrontRearSelector;

	float m_fMassMultForAccelerationInv; //  a Mass multiplier used to just increase or decrease acceleration, for instance if a car had extra passengers in it

	float m_fSuspensionTargetRaiseAmount;
	float m_fSuspensionRaiseAmount; 
	float m_fSuspensionRaiseRate;
	float m_fSuspensionForwardOffset;
	eWheelHydraulicState m_WheelState;
	eWheelHydraulicState m_WheelTargetState;

	/////////////////
	// current suspension state
	float m_fCompression;			// movement of suspension from static pos
	float m_fCompressionOld;

	float m_fWheelCompression;		// movement of wheel from static pos (used for positioning rendered wheel)

	/////////////////
	// current wheel state
	float m_fRotAng;				// current rotation angle
	float m_fRotAngVel;				// current rotation angular velocity
	float m_fRotSlipRatio;
	float m_fTyreTemp;

	////////////////
	// intersection state
	RegdPhy m_pHitPhysical;			// entity wheel is touching
	RegdPhy m_pPrevHitPhysical;
	RegdPed m_pHitPed;
	float m_fMaterialGrip;
	float m_fMaterialWetLoss;
	float m_fMaterialDrag;
	float m_fMaterialTopSpeedMult;
	float m_fGripMult;				// this is the same for all wheels on the vehicle
	
	// Sink this much further into the ground (compared to the rest of the car)
	// e.g. soft mud, sand, snow etc.
	float m_fExtraSinkDepth;
	float m_fWheelImpactOffsetOriginal;
	float m_fWheelImpactOffset;

	////////////////
	// saved stuff used for particles and audio
	float m_fEffectiveSlipAngle;
	float m_fTyreLoad;

	float m_fFwdSlipAngle;
	float m_fSideSlipAngle;

	float m_fTyreCamberForce;

	//////////////
	// control inputs
	float m_fSteerAngle;			// input steer angle
	float m_fBrakeForce;			// input braking force
	float m_fDriveForce;			// input engine force
	float m_fAbsTimer;
	
	float m_fBrakeForceOld;			// need these to integrate across smaller time steps
	float m_fDriveForceOld;			

	float m_fFrictionDamage;		// persistent var, will get increased with damage
	
	float m_fSuspensionHealth;		// 1000.0 = full, 500.0 = partially broken,	100.0 = totally broken, 0.0 = wheel falls off
	float m_fTyreHealth;			// 1000.0 = full, 500.0 = flat, 0.0 = tyre disintegrates
    float m_fTyreWearRate;
    float m_fMaxGripDiffFromWearRate;
    float m_fWearRateScale;
	float m_fBurstCheckTimer;

	////////////////
	// configuration and state flags
	fwFlags32 m_nDynamicFlags;
	fwFlags32 m_nConfigFlags;
	u16 m_nHitComponent;
	u8 m_nHitMaterialId;		// we don't use anything other than the material index currently. 
    bool m_TyreBurstDueToWear;

public:

	bool	m_bSuspensionForceModified;
	bool	m_bHasDonkHydraulics;
	bool	m_bReduceLongSlipRatioWhenDamaged;
	bool	m_bIgnoreMaterialMaxSpeed;
private:
	bool	m_bIsWheelInWater;
	bool	m_bUpdateWheelRotation;
	bool	m_bDisableWheelForces;
	bool	m_bDisableBottomingOut;
    bool    m_IncreaseSuspensionForceWithSpeed;
	bool    m_ReduceSuspensionForce; //Reduced suspension force used to "stance" tuner pack vehicles
	float	m_fReducedGripMult;
	float	m_fExtraWheelDrag;

public:
	float	m_fDownforceMult;		// Multiplier for the downforce for this wheel. Was previously determined based on the WCF_SPOILER flag.

#if __BANK
    float m_fLastCalculatedTractionLoss;
#endif // #if __BANK
	
	void SetReducedGripLevel( int reducedGripLevel );
	void SetDisableWheelForces( bool disableWheelForces ) { m_bDisableWheelForces = disableWheelForces; }
	void SetDisableBottomingOut( bool disableBottomingOut ) { m_bDisableBottomingOut = disableBottomingOut; }
    bool GetDisableWheelForces() { return m_bDisableWheelForces; }
    void SetIncreaseSuspensionForceWithSpeed( bool increaseSuspensionForce ) { m_IncreaseSuspensionForceWithSpeed = increaseSuspensionForce; }
	void SetReducedSuspensionForce( const bool bReduceSuspensionForce ) { m_ReduceSuspensionForce = bReduceSuspensionForce; }
	const bool GetReducedSuspensionForce() { return m_ReduceSuspensionForce; }
private:

	// 2 bytes of padding

	//////////////////
	// statics
	static eHierarchyId ms_aScriptWheelIds[SC_WHEEL_NUM];

public:

	static bank_bool ms_bUseWheelIntegrationTask;

	static dev_float ms_fWheelExtensionRate;

	static bank_bool ms_bActivateWheelImpacts;

#if __BANK
	static bank_bool ms_bDontCompressOnRecentlyBrokenFragments;
#endif

	static bank_float ms_fWheelBurstDamageChance;
	static bank_float ms_fFrictionDamageThreshForBurst;
	static bank_float ms_fFrictionDamageThreshForBreak;
										
	static dev_float ms_fWheelIndepRotMult;
	static dev_float ms_fWheelIndepPivotDistCompression;
	static dev_float ms_fWheelIndepPivotDistExtension;

    //Air in tyre
	static bank_float ms_fTyreTempHeatMult;
	static bank_float ms_fTyreTempCoolMult;
	static bank_float ms_fTyreTempCoolMovingMult;
	static bank_float ms_fTyreTempBurstThreshold;

	static dev_float ms_fDownforceMult;
	static dev_float ms_fDownforceMultSpoiler;
	static dev_float ms_fDownforceMultBikes;

	static const eHierarchyId ms_HubComponents[VEH_WHEEL_RM3 - VEH_WHEEL_LF + 1];
	static const eHierarchyId ms_OpposingWheels[VEH_WHEEL_RM3 - VEH_WHEEL_LF + 1];

	static u32 uXmasWeather;
	static phMaterialMgr::Id m_MaterialIdStuntTrack;

	static dev_bool ms_bUseExtraWheelDownForce; 

	static dev_float GetReduceGripOnRimMultFront();
	static dev_float GetReduceGripOnRimMultRear();
};

#if !__SPU
inline Vector3 CWheel::GetWheelOffset(CVehicleModelInfo* pVehicleModelInfo, eHierarchyId wheelId)
{
	Assert(pVehicleModelInfo);
	return VEC3V_TO_VECTOR3(pVehicleModelInfo->GetWheelOffset(wheelId));
}
#endif // !__SPU


inline float CWheel::GetTyreBurstSideRatio() const
{
	return rage::Clamp(m_fSideSlipAngle, -1.0f, 1.0f);
}


#if VEHICLE_USE_INTEGRATION_FOR_WHEELS

#define INTEGRATION_MAX_NUM_WHEELS (10)

// Integration variables that are needed after integration on the main thread
// Anything in this class will be stored directly on CAutomobile
struct WheelIntegrationOutput
{
	Vector3 vel;
};

// Extra data used during the integration process
struct WheelIntegrationLocals
{
	WheelIntegrationOutput output;

	Matrix34 mat;
	Vector3 angVel;
	Vector3 calcFwd[INTEGRATION_MAX_NUM_WHEELS];
	Vector3 calcSide[INTEGRATION_MAX_NUM_WHEELS];
	float aWheelAngSlip[INTEGRATION_MAX_NUM_WHEELS];
	float compressionDelta[INTEGRATION_MAX_NUM_WHEELS];
};

struct WheelIntegrationState
{
	WheelIntegrationState() : jobPending(0) {}
	WheelIntegrationOutput output;
	sysDependency dependency;
	u32 jobPending;
};
struct WheelIntegrationDerivative
{
	Vector3 vel;
	Vector3 angVel;
	Vector3 accel;
	Vector3 angAccel;
	float aWheelAngSlipDt[INTEGRATION_MAX_NUM_WHEELS];
	Vector3 aWheelForce[INTEGRATION_MAX_NUM_WHEELS];
};


class CWheelIntegrator
{
public:
	struct InternalState {
		Vector3 m_vecAccelDueToGravity;
		Vector3 m_vecInternalAccel;
		Vector3 m_vecInternalAngAccel;
		phCollider* m_pCollider;
		CWheel* const* m_ppWheels;
		int m_nNumWheels;
	};

	static void ProcessIntegration(phCollider* pCollider, CVehicle* pVehicle, WheelIntegrationState& state, float fTimeStep, unsigned frameCount);

	static void ProcessIntegrationTask(phCollider* pCollider, const Vector3& vecInternalForce, const Vector3& vecInternalTorque, WheelIntegrationLocals& locals, CWheel* const* ppWheels, int nNumWheels, float fMaxVelInGear, int nGear, float fTimeStep, unsigned frameCount, float fGravity);
	static void ProcessResultInternal(phCollider* pCollider, WheelIntegrationLocals& locals, CWheel* const* pWheels, int nNumWheels, float fMaxVelInGear, int nGear, float fTimeStep, unsigned frameCount, Vector3* pForceApplied, float fGravity);
	
#if !__SPU
	static void ProcessResult(WheelIntegrationState& state, Vector3* pForceApplied);
	static void ProcessResultsFinal(phCollider* pCollider, CWheel** ppWheels, int nNumWheels, ScalarV_In timeStep);
#endif // !__SPU

	static void AddVelToMatrix(Matrix34& matResult, const Matrix34& matInit, const Vector3& vel, const Vector3& angVel, float dt);

	static void WheelIntegrationEvaluate(WheelIntegrationDerivative &output, 
										 WheelIntegrationLocals& initialLocals, 
										 const WheelIntegrationTimeInfo& timeInfo, 
										 float fDerivativeTimeStep, 
										 const WheelIntegrationDerivative* pD,
										 InternalState& state,									 
										 const float fGravity );

private:

	static bool RunFromDependency(const sysDependency& dependency);
	static bool RunHoverModeFromDependancy(const sysDependency& dependency);

    static void ApplyDifferentials( CWheel* const* ppWheels, int nNumWheels, float fMaxVelInGear, float fTimeStep );

//	static phCollider* ms_pCollider;
//	static CWheel* ms_pWheels;
//	static int ms_nNumWheels;

	static float ms_fDesiredIntegrationStepSize;
};
#if __PS3
class CWheelTaskInfo
{
public:
	Vector3		vecInternalForce;
	Vector3		vecInternalTorque;
	float		fTimeStep;
	float		gGameTimeStep;    	
	phCollider*	pCollider;
	float		fMaxVelInGear;	
	WheelIntegrationState* pIntegrationState;
	float		fGravity;

	u8          gFrameCount;	// Frame count is only used for ABS so doesn't matter if it loops round
	u8			nNumWheels;
	u8			nGear : 6;

	bool		bIsArticulated :1;
	bool		bIsBike :1;
};
#endif //VEHICLE_USE_INTEGRATION_FOR_WHEELS
#endif // __PS3



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !__SPU


class rmcInstanceWheelData;


#if __D3D11 || RSG_ORBIS
class CWheelInstanceData_UsageInfo : rmcInstanceData_ConstBufferUpdateDesc
{
public:
	CWheelInstanceData_UsageInfo() { m_RegisterCounts[0] = 8; m_RegisterCounts[1] = 2; }
	~CWheelInstanceData_UsageInfo() {};

	u32 GetNoOfConstantBuffers() const { return 2; }
	grmShaderGroupVar GetStartVariable(int index) const { return m_ShaderGroupVariables[index]; }
	u32 GetRegisterCount(int index) const { return m_RegisterCounts[index]; };

private:
	u32 m_RegisterCounts[2];
	grmShaderGroupVar m_ShaderGroupVariables[2]; // matWheelTransform, tyreDeformParams & tyreDeformParams2.

	friend class CWheelInstanceRenderer;
};
#endif //__D3D11 || RSG_ORBIS
//
//
//
//
class CWheelInstanceRenderer
{
public:
	CWheelInstanceRenderer()		{ }
	~CWheelInstanceRenderer()		{ }

public:
	static bool Initialise();
	static void Shutdown();

	static void InitBeforeRender();
	static void AddWheelModel(const Matrix34& childMatrix, const grmModel *pModel, const grmShaderGroup *pShaderGroup, s32 shaderBucket,
								Vector4::Vector4Param tyreDeformParams, Vector4::Vector4Param tyreDeformParams2);
	static void RenderAllWheels();

private:
	static s32											ms_currInstanceIndices[NUMBER_OF_RENDER_THREADS];
	static rmcInstanceWheelData*						ms_pInstances[NUMBER_OF_RENDER_THREADS];
	static atArray<datRef<const rmcInstanceDataBase> >	ms_instArrays[NUMBER_OF_RENDER_THREADS];
#if __D3D11 || RSG_ORBIS
	static CWheelInstanceData_UsageInfo ms_InstanceUsageInfos[NUMBER_OF_RENDER_THREADS];
	static grcEffectGlobalVar ms_WheelTessellationVar;
#if __BANK
public:
#endif //__BANK
	static int ms_WheelTessellationFactor;
	static int ms_TreeTessellationFactor;
	static float ms_TreeTessellation_PNTriangle_K;
#endif // __D3D11 || RSG_ORBIS
};
#endif //!__SPU...



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Inlined functions

__forceinline void CWheel::CalculateMaximumExtents(Mat34V_InOut lastWheelMatrixOut, Mat34V_InOut currentWheelMatrixOut, phCollider *pParentCollider)
{
	float tempCompression = m_fCompression;
	m_fCompression = 0;
	CalcWheelMatrices(lastWheelMatrixOut,currentWheelMatrixOut,pParentCollider);
	m_fCompression = tempCompression;
}


#endif//_WHEEL_H_...


