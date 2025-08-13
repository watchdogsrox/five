//
//	10/09/2007	- Andrzej:	- CWheelInstanceRenderer added;
//
//
//
//
//
// 
#if !__SPU
// Rage headers
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "math/amath.h"
#include "math/vecmath.h"
#include "math/intrinsics.h"
#include "Math/Polynomial.h"
#include "rmptfx/ptxmanager.h"
#include "phBound/boundbvh.h"
#include "phbound/bounddisc.h"
#include "phbound/boundcurvedgeom.h"
#include "physics/gtaInst.h"
#include "physics/colliderdispatch.h"
#include "profile/cputrace.h"
#include "phbound/boundcomposite.h"

// Framework headers
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxreg.h"
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"
#include "fwsys/timer.h"
#include "fwvehicleai/pathfindtypes.h"

// Game headers
#include "animation/AnimBones.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/cinematic/CinematicDirector.h"
#include "control/record.h"
#include "debug/MarketingTools.h"
#include "event/ShockingEvents.h"
#include "game/modelIndices.h"
#include "Game/Weather.h"
#include "modelInfo/vehicleModelInfo.h"
#include "peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/Pickup.h"
#include "pickups/data/PickupData.h"
#include "renderer/Water.h"
#include "scene/world/GameWorld.h"
#include "scene/ExtraContent.h"
#include "Shaders/CustomShaderEffectVehicle.h"
#include "Stats/StatsMgr.h"
#include "Task/System/TaskHelpers.h" // for CRoutePoint
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Physics/TaskNM.h"
#include "task/Physics/TaskNMBrace.h"
#include "TimeCycle/TimeCycle.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/automobile.h"
#include "vehicles/bike.h"
#include "vehicles/door.h"
#include "vehicles/heli.h"
#include "vehicles/planes.h"
#include "vehicles/virtualroad.h"
#include "vehicles/Trailer.h"
#include "vehicleAI/pathfind.h"
#include "vehicleAI/vehicleintelligence.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxTrail.h"
#include "Vfx/Systems/VfxWheel.h"
#include "Vfx/VfxHelper.h"

AI_OPTIMISATIONS() 
VEHICLE_OPTIMISATIONS()

PARAM(disablewheelintegrationtask, "Force wheel integration to happen on main thread.");

#else

#include "math/vecmath.h"
#include "fwmaths/randomSPU.h"
//#include "profile/cputrace.h"
#include "vector/quaternion.h"
#include "handlingMgr.h"

#endif // __SPU

dev_float sfTractionPeakAngle = 1.0f;
dev_float sfTractionPeakAngleInv = sfTractionPeakAngle;
dev_float sfTractionMinAngle = 2.5f;
dev_float sfTractionMinAngleInv = 1.0f / 2.5f; //matts - must use constant for SPU (was 1.0f / sfTractionMinAngle)

dev_float sfTractionSpringSpeedMult = 0.5f;

dev_float sfSuspensionHealthLimit1 = 500.0f;
dev_float sfSuspensionHealthSpringMult1 = 0.7f;
dev_float sfSuspensionHealthDampMult1 = 0.8f;

dev_float sfSuspensionHealthLimit2 = 100.0f;
dev_float sfSuspensionHealthSpringMult2 = 0.4f;
dev_float sfSuspensionHealthDampMult2 = 0.6f;

dev_float sfTyreDeflatePeriod = 0.2f;
dev_float sfTyreOnFirePeriod = 10.0f;

dev_float sfFrictionStd = 0.08f;
dev_float sfFrictionHigh = 0.15f;
dev_float sfFrictionBicycleHigh = 0.17f;
dev_float sfFrictionBicycleHighDamageTyre = 0.4f;
dev_float sfFrictionCarDmagedTyre = 0.013f;
dev_float sfFrictionFreeWheel = 0.02f;	// Need some friction or will never stop
dev_float sfFreeWheelFrictionIncreaseSpeed = 1.5f; // Speed at which to increase free wheel friction
dev_float sfFreeWheelFrictionMiniumumSpeed = 0.5f; // Speed at which to have normal friction when free wheeling.

dev_float sfFrictionDamageStart = 0.25f;
dev_float sfFrictionStdInAir = 0.1f;			// mass independent
dev_float sfFrictionStdInAirDriven = 2.0f;		// mass independent

dev_float sfInvInertiaForIntegratorAccel = 100.0f;
dev_float sfInvInertiaForIntegratorBrake = 100.0f;
dev_float sfInvInertiaForIntegratorAccelInAir = 150.0f;
dev_float sfInvInertiaForIntegratorBrakeInAir = 10.0f;

bank_float CWheel::ms_fTyreTempHeatMult = 1.5f;//1.0f;
bank_float CWheel::ms_fTyreTempCoolMult = 0.12f;//0.25f;
bank_float CWheel::ms_fTyreTempCoolMovingMult = 0.25f;
bank_float CWheel::ms_fTyreTempBurstThreshold = 59.0f;	// Tyres burst past this point

dev_float CWheel::ms_fDownforceMult = 0.035f;
dev_float CWheel::ms_fDownforceMultSpoiler = 0.070f;
dev_float CWheel::ms_fDownforceMultBikes = 0.120f;

bank_bool CWheel::ms_bUseWheelIntegrationTask = true;

dev_float CWheel::ms_fWheelExtensionRate = 5.0f;

#define sfWheelImpactSpeedDriveOverPedLimitSq (0.05f)
#define sfThrottleDriveOverThreshold (0.3f)

dev_float sfDriveThroughPedSpeedSq = 49.0f;

dev_float sfWheelImpactNormalFwdLimit = 0.2f;
dev_float sfWheelImpactNormalFwdLimitQuadBike = 0.8f;
dev_float sfWheelImpactNormalLimitBike = 0.3f;
dev_float sfWheelImpactNormalLimitCar = 0.2f;
dev_float sfWheelImpactNormalLimitBikeUp = 0.2f;
dev_float sfWheelImpactRadiusLimit = 0.95f;
#define sfWheelImpactNormalSideLimit (0.94f)			// ~ 20 degrees
#define sfWheelImpactNormalSideLimitActivate (0.75f)
#define sfWheelImpactNormalSideLimitBike (0.75f)
#define sfWheelImpactNormalSideLimitCarVsCar (0.50f)	// 60 degrees
dev_float sfWheelImpactNormalSideLimitBobsleigh = 1.0f;	// Special case for bobsleighs... never count impacts as side impacts
dev_bool sbForceVerticalWheelImpactNormal = false;
dev_float sfWheelGroundSideFrictionMult = 0.1f;
dev_float sfWheelGroundSideElasticityMult = 0.0f;
dev_float sfWheelToRoadFrictionMult = 0.0f;
dev_float sfWheelToRoadElasticityMult = 0.0f;

dev_float sfSnowGripAdjust = 0.2f;

u32 CWheel::uXmasWeather = 0;
phMaterialMgr::Id CWheel::m_MaterialIdStuntTrack = phMaterialMgr::DEFAULT_MATERIAL_ID;
dev_bool CWheel::ms_bUseExtraWheelDownForce = true;

#if !__SPU
// PURPOSE:	Vector containing some constants that are beneficial to load as an aligned
//			vector:
Vec4V g_WheelImpactNormalSideLimitsV(sfWheelImpactNormalSideLimit, sfWheelImpactNormalSideLimitCarVsCar, sfWheelImpactNormalSideLimitBike, sfWheelImpactNormalSideLimitActivate);
static ScalarV_Out sGetWheelImpactNormalSideLimitV()			{ return g_WheelImpactNormalSideLimitsV.GetX(); }
static ScalarV_Out sGetWheelImpactNormalSideLimitCarVsCarV()	{ return g_WheelImpactNormalSideLimitsV.GetY(); }
static ScalarV_Out sGetWheelImpactNormalSideLimitBikeV()		{ return g_WheelImpactNormalSideLimitsV.GetZ(); }
static ScalarV_Out sGetWheelImpactNormalSideLimitActivateV()	{ return g_WheelImpactNormalSideLimitsV.GetW(); }
#endif	// !__SPU

dev_float sfMaxCompressionChangeWheelStill = 0.1f;
dev_float sfWheelLastPosAddRadius = 0.5f;

dev_float sfWheelForceDeltaVCap = 20.0f;

dev_float sfWheelImpactBikeUpLimit = 0.05f;
dev_float sfWheelImpactBikeUpLimitInReverse = 0.2f;

#define sfReduceTreadWidthMaxSpeed (10.0f*10.0f)

dev_float sfWheelImpactUpLimitAfterExterWheelCollision = 0.2f;
dev_float sfWheelFrictionDamageToDisableActivationOfImpacts = 0.2f; //severly damaged suspension can be constantly bottoming out causing issues with driveability, so if the suspension is badly damaged don't to bottom out impacts.

dev_float sfAllowableTyreClipAmount = 0.85f;

bank_bool CWheel::ms_bActivateWheelImpacts = true;

#if __BANK
bank_bool CWheel::ms_bDontCompressOnRecentlyBrokenFragments = true;
#endif

bank_float CWheel::ms_fWheelBurstDamageChance = 0.82f;
bank_float CWheel::ms_fFrictionDamageThreshForBurst = 0.22f;
bank_float CWheel::ms_fFrictionDamageThreshForBreak = 0.36f;

#if !__SPU

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CWheel, WHEEL_POOL_SIZE, 0.33f, atHashString("Wheels",0x975ac445));

eHierarchyId CWheel::ms_aScriptWheelIds[SC_WHEEL_NUM] =
{
	VEH_WHEEL_LF,	//SC_WHEEL_CAR_FRONT_LEFT,
	VEH_WHEEL_RF,	//SC_WHEEL_CAR_FRONT_RIGHT,
	VEH_WHEEL_LM1,	//SC_WHEEL_CAR_MID_LEFT,
	VEH_WHEEL_RM1,	//SC_WHEEL_CAR_MID_RIGHT,
	VEH_WHEEL_LR,	//SC_WHEEL_CAR_REAR_LEFT,
	VEH_WHEEL_RR,	//SC_WHEEL_CAR_REAR_RIGHT,
	BIKE_WHEEL_F,	//SC_WHEEL_BIKE_FRONT,
	BIKE_WHEEL_R	//SC_WHEEL_BIKE_REAR,
};

dev_float REDUCE_GRIP_MULT = 0.11f;
//static bool TEST_IMPLICIT_FORCES = false;

//
CWheel::CWheel()
{
	GetDynamicFlags().ClearFlag(WF_DUMMY_TRANSITION);
	m_fPreviousTransitionCompression = m_fTransitionCompression = -1.0f;

	m_uLastPopTireCheck = 0;

    ////////////////
    // config vars
    m_WheelId = VEH_CHASSIS;
    m_nOppositeWheelIndex = -1;
    for(int i = 0; i < MAX_WHEEL_BOUNDS_PER_WHEEL; i++)
    {
        m_nFragChild[i] = -1;
    }
    m_fWheelRadius = 1.0f;
    m_fWheelRimRadius = 1.0f;
    m_fInitialRimRadius = 1.0f;
    m_pHandling = nullptr;
    m_pParentVehicle = nullptr;

    m_fSusLength = 1.0f;
    m_fMaxTravelDelta = 1.0f;
    m_fStaticDelta = 0.0f;
    m_fStaticForce = 0.0f;
    m_fFrontRearSelector = 0.0f;

    m_fMassMultForAccelerationInv = 1.0f;

    /////////////////
    // vector description of this wheel
    m_vecAxleAxis.Set(1.0f, 0.0f, 0.0f);
	m_aWheelLines.A.Zero();
	m_aWheelLines.B.Zero();

    m_fCompression = -101.0f;
    m_fCompressionOld = -101.0f;
    m_fWheelCompression = -101.0f;

    m_fRotAng = 0.0f;
    m_fRotAngVel = 0.0f;
    m_fRotSlipRatio = 0.0f;
    m_fTyreTemp = 0.0f;

    m_vecHitPos.Zero();
    m_vecHitCentrePos.Zero();
    m_vecHitCentrePosOld.Zero();
    m_vecHitNormal.Zero();
    m_pHitPhysical = NULL;
	m_pPrevHitPhysical = NULL;
    m_vecHitPhysicalOffset.Zero();
    m_pHitPed = NULL;
    m_nHitMaterialId = 0;
    m_nHitComponent = 0;
    m_fMaterialGrip = 1.0f;
	m_bSuspensionForceModified = false;
	//m_nHydraulicsSuccessfulBounceCount = 0;
	//m_bFirstBounce = 0;
    m_fMaterialWetLoss = 1.0f;
    m_fMaterialDrag = 0.0f;
    m_fMaterialTopSpeedMult = 1.0f;
    m_fGripMult = 1.0f;

    m_vecGroundContactVelocity.Zero();
    m_vecTyreContactVelocity.Zero();
    m_fEffectiveSlipAngle = 0.0f;
	m_fTyreLoad = 0.0f;
	m_fTyreCamberForce = 0.0f;
    m_vecGroundVelocity.Zero();

    m_fFwdSlipAngle = 0.0f;
    m_fSideSlipAngle = 0.0f;

    m_fSteerAngle = 0.0f;
    m_fBrakeForce = 0.0f;
    m_fDriveForce = 0.0f;
    m_fAbsTimer = 0.0f;
    m_fBrakeForceOld = 0.0f;
    m_fDriveForceOld = 0.0f;

    m_fFrictionDamage = 0.0f;

    ResetDamage();

    m_nDynamicFlags.ClearAllFlags();
    m_nConfigFlags.ClearAllFlags();

    m_fOnFireTimer = 0.0f;

    m_fExtraSinkDepth = 0.0f;
    m_fWheelImpactOffset = 0.0f;
	m_fWheelImpactOffsetOriginal = 0.0f;

	m_fSuspensionRaiseAmount = 0.0f;
	m_fSuspensionTargetRaiseAmount = 0.0f;
	m_fSuspensionForwardOffset = 0.0f;
	m_fSuspensionRaiseRate = 1.0f;
	m_WheelState = m_WheelTargetState = WHS_IDLE;

	m_fBurstCheckTimer = 0.0f;

	m_fDownforceMult	= CWheel::ms_fDownforceMult;
	m_fReducedGripMult	= REDUCE_GRIP_MULT;
	m_fExtraWheelDrag	= 0.0f;

	if( m_MaterialIdStuntTrack == phMaterialMgr::DEFAULT_MATERIAL_ID )
	{
		m_MaterialIdStuntTrack = PGTAMATERIALMGR->g_idStuntRampSurface;
	}

	m_bIsWheelInWater = false;
	m_bUpdateWheelRotation = true;
	m_bDisableWheelForces = false;
	m_bDisableBottomingOut = false;
    m_IncreaseSuspensionForceWithSpeed = false;

    m_fTyreWearRate = 0.0f;
    m_fMaxGripDiffFromWearRate = 0.1f;
    m_fWearRateScale = 1.0f;
    m_TyreBurstDueToWear = false; 

	m_ReduceSuspensionForce = false;

#if __BANK
    m_fLastCalculatedTractionLoss = 1.0f;
#endif //#if __BANK
}

CWheel::~CWheel()
{
	ptfxReg::UnRegister(this, true);
}

dev_float sfWheelRadiusSinkDepth = 0.15f;
dev_float sfBikeWheelRadiusSinkDepth = 0.25f;
void CWheel::Init(CVehicle* pParentVehicle, eHierarchyId wheelId, float fRadius, float fForceMult, int nConfigFlags, s8 oppositeWheelIndex /*= -1*/)
{
	// check this wheel exists
	Assertf(pParentVehicle->GetBoneIndex(wheelId) > -1, "Vehicle %s is missing a wheel", pParentVehicle->GetVehicleModelInfo()->GetModelName());

	m_WheelId = wheelId;
	m_nOppositeWheelIndex = oppositeWheelIndex;
	m_pHandling = pParentVehicle->pHandling;
    m_pParentVehicle = pParentVehicle;

	if(const fragInst* vehicleFragInst = m_pParentVehicle->GetVehicleFragInst())
	{
		int iBoneIndex=m_pParentVehicle->GetBoneIndex(m_WheelId);
		if(iBoneIndex!=-1)
		{
			int nGroup = vehicleFragInst->GetGroupFromBoneIndex(iBoneIndex);
			if(nGroup > -1)
			{
				fragTypeGroup* pGroup = vehicleFragInst->GetTypePhysics()->GetGroup(nGroup);
				int iChild = pGroup->GetChildFragmentIndex();
				for(int k = 0; k < pGroup->GetNumChildren() && k < MAX_WHEEL_BOUNDS_PER_WHEEL; k++)
				{
					Assert(iChild + k < 128);
					m_nFragChild[k] = (s8)(iChild+k);
				}
			}
		}
	}

	if(GetFragChild() > -1)
	{
		Assertf(GetFragChild() > 0, "Vehicle %s has wheel as first child? does your chassis have collision?", m_pParentVehicle->GetVehicleModelInfo()->GetModelName());

		phBound* pBound = m_pParentVehicle->GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds()->GetBound(GetFragChild());
	
		m_fWheelRadius = pBound->GetBoundingBoxMax().GetZf();
        m_fWheelRimRadius = fRadius;
		m_fWheelWidth = 2.0f*pBound->GetBoundingBoxMax().GetXf();

		// check if this car has any instanced wheels
		CVehicleModelInfo* pModelInfo = m_pParentVehicle->GetVehicleModelInfo();

		//check to see if we have any instanced wheels
		bool instancedWheelsFound = false;
		for(int i = 0; i < NUM_VEH_CWHEELS_MAX; i++)
		{
			if(pModelInfo->GetStructure()->m_nWheelInstances[i][0] > -1)
			{
				instancedWheelsFound = true;
				break;
			}
		}

		if(instancedWheelsFound)
		{
			nConfigFlags |= WCF_INSTANCED;
		}
	}
	else
	{
		m_fWheelRadius = fRadius;
		m_fWheelRimRadius = fRadius - 0.1f;
		m_fWheelWidth = 0.25f;
	}

    m_fInitialRimRadius = m_fWheelRimRadius;

	dev_float fLowPoweredVehicle = 0.12f;
	if(m_pParentVehicle->m_Transmission.GetDriveForce() < fLowPoweredVehicle)
	{
		nConfigFlags |= WCF_DONT_REDUCE_GRIP_ON_BURNOUT;
	}

	m_nConfigFlags = nConfigFlags;
	GetDynamicFlags().SetFlag(WF_HIT_PREV);

	m_fStaticForce = fForceMult;

	m_fFrontRearSelector = GetConfigFlags().IsFlagSet(WCF_REARWHEEL) ? -1.0f : 1.0f;

	m_deepSurfaceInfo.Init();
	m_liquidAttachInfo.Init();



	CVehicleWeaponHandlingData* pWeaponHandling = m_pParentVehicle->pHandling->GetWeaponHandlingData();
	if(pWeaponHandling)
	{
		// don't add the wheel impact offset to the front wheels on the half-track
		if( !GetConfigFlags().IsFlagSet( WCF_STEER ) ||
			m_pParentVehicle->pHandling->hFlags & HF_STEER_ALL_WHEELS )
		{
			m_fWheelImpactOffset = pWeaponHandling->GetWheelImpactOffset();
		}
	}

	if(m_pParentVehicle->InheritsFromAutomobile() && !m_pParentVehicle->m_nVehicleFlags.bTyresDontBurst)
	{
		// Make car tyres sink into the ground slightly.
		float fWheelRadius = m_fWheelRadius - m_fWheelRimRadius;
		m_fWheelImpactOffset -= fWheelRadius * sfWheelRadiusSinkDepth;
	}
	else if(m_pParentVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE && !m_pParentVehicle->m_nVehicleFlags.bTyresDontBurst)
	{
		// Make bike tyres sink into the ground slightly.
		float fWheelRadius = m_fWheelRadius - m_fWheelRimRadius;
		m_fWheelImpactOffset -= fWheelRadius * sfBikeWheelRadiusSinkDepth;
	}

	m_fWheelImpactOffsetOriginal = m_fWheelImpactOffset;

	m_bReduceLongSlipRatioWhenDamaged	 = MI_CAR_SLAMVAN3.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_SLAMVAN3.GetModelIndex();
	m_bHasDonkHydraulics = m_pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_LOWRIDER_DONK_HYDRAULICS );

	m_bIgnoreMaterialMaxSpeed = m_pParentVehicle->pHandling->mFlags & MF_HAS_TRACKS && !( m_pParentVehicle->pHandling->hFlags & HF_STEER_ALL_WHEELS );

	m_IncreaseSuspensionForceWithSpeed = ( m_pParentVehicle->pHandling->GetCarHandlingData() && m_pParentVehicle->pHandling->GetCarHandlingData()->aFlags & CF_INCREASE_SUSPENSION_FORCE_WITH_SPEED ) || m_pParentVehicle->GetModelIndex() == MI_CAR_VISERIS.GetModelIndex();
	
}

////////////////////////////
// Do initial setup of probes for this wheel
//
dev_float sfWheelTwistAxleMult = 0.16f;
dev_float sfWheelTwistAxleStart = 0.21f;
dev_float sfWheelSkewedFrictionMult = 3.4f;
dev_float sfWheelSkewedFrictionNonMissionAiMult = 1.6f;
dev_float sfWheelSkewedFrictionMissionExtraMult = 0.1f;
dev_float sfHydraulicsMaxSuspensionDelta = 1.0f;
//bool sbHydraulicForcesIncreaseWithBounces = false;

//
void CWheel::SuspensionSetup(const void* pDamageTexture)
{
	// Calculate suspension values based on parent group transforms
	// Needed if wheels are children of articulated frag groups
	int iWheelBoneIndex  = m_pParentVehicle->GetBoneIndex(GetHierarchyId());

	// First find default transform of wheel to parent
	const crSkeletonData& skeletonData = m_pParentVehicle->GetSkeletonData();
	Matrix34 matWheelTransform(Matrix34::IdentityType);
	if(iWheelBoneIndex > -1)
	{
		const crBoneData* pBoneData = skeletonData.GetBoneData(iWheelBoneIndex);

		// Use default translation because the wheels local matrix gets messed around with for rendering
		Quaternion defaultRotation = RCC_QUATERNION(pBoneData->GetDefaultRotation());
		matWheelTransform.FromQuaternion(defaultRotation);
		matWheelTransform.d = RCC_VECTOR3(pBoneData->GetDefaultTranslation());

		// Now figure out where parent is
		// This requires the skeleton's local matrices to be up to date!
		pBoneData = pBoneData->GetParent();
		while(pBoneData)
		{
			Matrix34 matParentTransform;
			
			if(!m_nConfigFlags.IsFlagSet(WCF_CENTRE_WHEEL))
			{
				matParentTransform = m_pParentVehicle->GetLocalMtx(pBoneData->GetIndex());
			}
			else
			{
				// The centre wheel's parent bone might rotate, so use the default rotation
				matParentTransform.FromQuaternion(QUATV_TO_QUATERNION(pBoneData->GetDefaultRotation()));
			}

			matParentTransform.d = RCC_VECTOR3(pBoneData->GetDefaultTranslation());

			matWheelTransform.Dot(matParentTransform);
			pBoneData = pBoneData->GetParent();
		}

#if __DEV
		static dev_bool bDrawWheelMatrices = false;
		if(bDrawWheelMatrices)
		{
			Matrix34 matDraw = matWheelTransform;
			Matrix34 m = MAT34V_TO_MATRIX34(m_pParentVehicle->GetMatrix());
			matDraw.Dot(m);
#if DEBUG_DRAW
			grcDebugDraw::Axis(matDraw,0.25f);
#endif
		}
#endif
	}

	//TUNE_GROUP_BOOL( HYDRAULICS_TUNE, SCALE_FORCE_WITH_SUCCESSFUL_BOUNCES, false );
	//sbHydraulicForcesIncreaseWithBounces = SCALE_FORCE_WITH_SUCCESSFUL_BOUNCES;

	Vector3 vecPos = matWheelTransform.d;

	vecPos.y += GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) ? -m_fSuspensionForwardOffset : m_fSuspensionForwardOffset;

	if( m_fSuspensionTargetRaiseAmount != m_fSuspensionRaiseAmount )
	{
		float raiseScale = 1.0f;

		if( m_pParentVehicle->InheritsFromAutomobile() )
		{
			raiseScale = Min( 1.0f, static_cast< CAutomobile* >( m_pParentVehicle )->GetHydraulicsRate() );
		}

		float maxDelta = ( sfHydraulicsMaxSuspensionDelta ) * m_fSuspensionRaiseRate * fwTimer::GetTimeStep() * raiseScale;
		float suspensionDiff = m_fSuspensionTargetRaiseAmount - m_fSuspensionRaiseAmount;
		bool targetStateBounce = ( m_WheelTargetState >= WHS_BOUNCE_START &&
								   m_WheelTargetState <= WHS_BOUNCE_END );

		bool currentStateBounce = ( m_WheelState >= WHS_BOUNCE_START &&
								    m_WheelState <= WHS_BOUNCE_END );

		bool wheelHit = ( m_nDynamicFlags.IsFlagSet( WF_HIT_PREV ) ||
						  m_nDynamicFlags.IsFlagSet( WF_HIT ) );
		
		if( wheelHit &&
		    ( ( suspensionDiff > 0.0f && 
			    targetStateBounce ) ||
			  currentStateBounce ) )
		{
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, wheelRaiseForceScale, 2.0f, 0.0f, 10.0f, 0.01f );

			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, wheelRaiseForceMax, 8.0f, 0.0f, 200.0f, 0.5f );
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, wheelCompressionForceScale, 1.0f, 0.0f, 50.0f, 0.1f );

			// Reduce force + extension rate based on vehicle velocity
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, maxDeltaRateReductionFactor, 0.0f, 0.0f, 1.0f, 0.02f );
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, maxForceReductionFactor, 0.0f, 0.0f, 1.0f, 0.02f );

			// Max velocities for reducing force and delta
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, maxSqrVelocityForJump, 50.0f, 1.0f, 5000.0f, 1.0f );
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, maxSqrVelocityScaleRaiseRate, 50.0f, 1.0f, 5000.0f, 1.0f );
			
			// Increase forces to jump all the wheels off the ground
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, jumpBounceScale, 5.0f, 0.0f, 20.0f, 0.1f );

			// Increase forces to bounce the front or back wheels off the ground
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, frontBackBounceScale, 1.3f, 0.0f, 50.0f, 0.1f );
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, frontBackBounceFrontWheelScale, 1.25f, 0.0f, 50.0f, 0.1f );

			// Reduce the bounce forces if we have increased the spring force
			TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, forceReduceionWhenSuspensionForceModified, 1.0f, 0.0f, 1.0f, 0.1f );
			
			float forceScale	= wheelRaiseForceScale - 0.7f;
			float maxForce		= wheelRaiseForceMax * m_pParentVehicle->GetMass();
			
			forceScale *= ( m_fSuspensionRaiseRate * m_pParentVehicle->GetMass() * raiseScale );
			forceScale *= 1.0f + ( m_fCompression * wheelCompressionForceScale );

			if( m_bSuspensionForceModified )
			{
				forceScale *= forceReduceionWhenSuspensionForceModified;
			}

			forceScale *= ( m_fSuspensionHealth * 0.001f );

			if( m_WheelTargetState == WHS_BOUNCE_ALL ||
				m_WheelState == WHS_BOUNCE_ALL )
			{
				float vehicleSpeed		= m_pParentVehicle->GetVelocity().Mag2();
				float deltaReduction	= 1.0f - Min( maxDeltaRateReductionFactor, vehicleSpeed / maxSqrVelocityScaleRaiseRate );
				float bounceForceScale	= ( jumpBounceScale - ( jumpBounceScale * Min( maxForceReductionFactor, vehicleSpeed / maxSqrVelocityForJump ) ) ) * deltaReduction;

				maxDelta	*= deltaReduction;
				forceScale	*= bounceForceScale;
				maxForce	*= bounceForceScale;

				if( CPhysics::GetNumTimeSlices() == 1 )
				{
					float timeScale = fwTimer::GetInvTimeStep() / 60.0f;
					forceScale *= timeScale;
					maxForce *= timeScale;
				} 
			}
			else if( m_WheelTargetState == WHS_BOUNCE_FRONT_BACK ||
					 m_WheelState == WHS_BOUNCE_FRONT_BACK )
			{
				forceScale	*= frontBackBounceScale + 0.7f;
				maxForce	*= frontBackBounceScale + 0.7f;

				if( !GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) )
				{
					forceScale	*= frontBackBounceFrontWheelScale;
					maxForce	*= frontBackBounceFrontWheelScale;
				}
			}
			
			forceScale = Min( forceScale, maxForce );

			Vector3 wheelRaiseForce = matWheelTransform.c * forceScale;
			Vector3 forcePosition = VEC3V_TO_VECTOR3( m_pParentVehicle->GetTransform().Transform3x3( VECTOR3_TO_VEC3V( matWheelTransform.d ) ) );
			
			m_pParentVehicle->ApplyForce( wheelRaiseForce, forcePosition );
		}

		m_fSuspensionRaiseAmount += Clamp( suspensionDiff, -maxDelta, maxDelta );
	}

	float currentRaiseAmount = m_pHandling->m_fSuspensionRaise + m_fSuspensionRaiseAmount;
	vecPos.z -= currentRaiseAmount;

	float fStartOffset = (m_pHandling->m_fSuspensionUpperLimit + currentRaiseAmount);
	float fEndOffset = (m_pHandling->m_fSuspensionLowerLimit - m_fWheelRadius);

	// Amphibious quadbikes can tuck in their wheels. This involves raising the suspension quite high up.
	// If the end of the suspension starts to go higher than the upper one, move the upper one up as well
	// So that the lower end does not go higher than the upper end.
	if( ( m_pParentVehicle->InheritsFromAmphibiousQuadBike() && !static_cast<CAmphibiousQuadBike*>(m_pParentVehicle)->IsWheelsFullyOut() ) ||
		( m_pParentVehicle->InheritsFromSubmarineCar() && m_fSuspensionRaiseAmount < 0.0f ) )
	{
		float fOffsetDiff = fStartOffset - fEndOffset;
		static dev_float sfMinDiffBetweenStartAndEnd = 0.3f;
		static dev_float sfMinDiffBetweenStartAndEndSubCar = 0.5f;

		float minDiff = m_pParentVehicle->InheritsFromAmphibiousQuadBike() ? sfMinDiffBetweenStartAndEnd : sfMinDiffBetweenStartAndEndSubCar;
		if( fOffsetDiff < minDiff )
		{
			fStartOffset += ( minDiff - fOffsetDiff );
		}
	}

	Vector3 vecStart = vecPos;
	Vector3 vecEnd = vecPos;

	if( m_fSuspensionForwardOffset == 0.0f )
	{
		vecStart += ( matWheelTransform.c * fStartOffset );
		vecEnd += ( matWheelTransform.c * fEndOffset );
	}
	else
	{
		vecStart += ( Vector3( 0.0f, 0.0f, 1.0f ) * fStartOffset );
		vecEnd += ( Vector3( 0.0f, 0.0f, 1.0f ) * fEndOffset );
	}

    if( m_pHandling->GetCarHandlingData() )
    {
        float camberOffset = GetConfigFlags().IsFlagSet(WCF_REARWHEEL) ? rage::Tanf( -m_pHandling->GetCarHandlingData()->m_fCamberRear ) : rage::Tanf( -m_pHandling->GetCarHandlingData()->m_fCamberFront );
        camberOffset *= m_fWheelRadius;

        if( GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) )
        {
            camberOffset *= -1.0f;
        }

        vecEnd.x += ( camberOffset );
//        vecStart.x += ( camberOffset );
    }

	Vector3 vecSidePos = vecPos;
	if(vecSidePos.x > 0.0f)
	{
		vecSidePos.x -= 0.5f;
	}
	else
	{
		vecSidePos.x += 0.5f;
	}

	Vector3 vecTemp;
	float fSuspensionSkewed = 0.0f;

	if(!GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && !GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL) && !GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) && m_pParentVehicle->m_nVehicleFlags.bCanDeformWheels) // Don't want to do this on bikes 
	{
		Vector3 vDeformation;
		bool hasLandingGearDeformation = ( m_pParentVehicle->InheritsFromPlane() && ((CPlane *)m_pParentVehicle)->GetWheelDeformation( this, vDeformation ) ) ||
										 ( m_pParentVehicle->InheritsFromHeli() && ((CHeli *)m_pParentVehicle)->GetWheelDeformation( this, vDeformation ) );

		if( hasLandingGearDeformation )
		{
			vecStart += vDeformation;
			fSuspensionSkewed += rage::Abs(vDeformation.Dot(matWheelTransform.a)) + rage::Abs(vDeformation.Dot(matWheelTransform.c));

			vecEnd += vDeformation;
			fSuspensionSkewed += rage::Abs(vDeformation.Dot(matWheelTransform.a)) + rage::Abs(vDeformation.Dot(matWheelTransform.c));

			vecPos += vDeformation;
			vecSidePos += vDeformation;

		}
		else if(pDamageTexture)
		{
			vecTemp = VEC3V_TO_VECTOR3(m_pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pDamageTexture, VECTOR3_TO_VEC3V(vecStart), false));
			vecStart += vecTemp;
			fSuspensionSkewed += rage::Abs(vecTemp.Dot(matWheelTransform.a)) + rage::Abs(vecTemp.Dot(matWheelTransform.c));

			vecTemp = VEC3V_TO_VECTOR3(m_pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pDamageTexture, VECTOR3_TO_VEC3V(vecEnd), false));
			vecEnd += vecTemp;
			fSuspensionSkewed += rage::Abs(vecTemp.Dot(matWheelTransform.a)) + rage::Abs(vecTemp.Dot(matWheelTransform.c));

			vecPos += VEC3V_TO_VECTOR3(m_pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pDamageTexture, VECTOR3_TO_VEC3V(vecPos), false));
			vecSidePos += VEC3V_TO_VECTOR3(m_pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pDamageTexture, VECTOR3_TO_VEC3V(vecSidePos), false));
		}
	}

	// the calculated skew amount is going to be dependent on the position of the wheel
	// so need to undo that offset scale influence by dividing by vecStart (the wheel offset)
	Assert(vecStart.IsNonZero());

	if( !NetworkInterface::IsGameInProgress() )
	{
		m_fFrictionDamage = fSuspensionSkewed * sfWheelSkewedFrictionMult * vecStart.InvMag();
		if( m_pParentVehicle->m_nVehicleFlags.bMissionVehToughAxles || 
			(CTheScripts::GetPlayerIsOnAMission() && m_pParentVehicle->GetDriver() && m_pParentVehicle->GetDriver()->IsPlayer()) ||
			m_pParentVehicle->PopTypeIsMission() )
		{
			m_fFrictionDamage *= sfWheelSkewedFrictionMissionExtraMult;
			m_fFrictionDamage = rage::Clamp( m_fFrictionDamage, 0.0f ,sfFrictionDamageStart );
		}
		else if (!m_pParentVehicle->GetDriver() || (m_pParentVehicle->GetDriver() && !m_pParentVehicle->GetDriver()->IsPlayer()))
		{
			m_fFrictionDamage *= sfWheelSkewedFrictionNonMissionAiMult;
		}
	}

	if( GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) )
	{
		vecStart.x = 0.0f;
		vecEnd.x = 0.0f;
	}

	m_aWheelLines.Set(vecStart, vecEnd);
	m_vecSuspensionAxis = vecStart - vecEnd;
	m_vecSuspensionAxis.Normalize();

	Vector3 vecAxleAxisWithDirection = m_vecAxleAxis = vecSidePos - vecPos;

	vecAxleAxisWithDirection.Normalize();
	
	if( vecPos.x > 0.0f && !GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) )
	{
		m_vecAxleAxis *= -1.0f;
	}

	// only twist direction of wheel axle once wheel has been damaged sufficiently
	// scripts have the option to disable axle twist as well
	// this is the effect that pulls the car steering to the side when it's damaged
// 	if(m_fFrictionDamage < sfWheelTwistAxleStart || m_pParentVehicle->m_nVehicleFlags.bMissionVehToughAxles)
// 		m_vecAxleAxis.y = 0.0f;
// 	else
// 		m_vecAxleAxis.y *= sfWheelTwistAxleMult;

	//Remove any axle twist as its contributing to wheels vibrating when vehicles are heavily damaged.
	m_vecAxleAxis.y = 0.0f;

	// ensure suspension and axle axes are never parallel (causes QNANs)
	float fAxleDotSusAxis = vecAxleAxisWithDirection.Dot(m_vecSuspensionAxis);
	if(IsClose(fabs(fAxleDotSusAxis), 1.0f))
	{
		m_vecSuspensionAxis.RotateX(vecAxleAxisWithDirection.z * SMALL_FLOAT);
		m_vecSuspensionAxis.RotateY(vecAxleAxisWithDirection.x * SMALL_FLOAT);
		m_vecSuspensionAxis.RotateZ(vecAxleAxisWithDirection.y * SMALL_FLOAT);
		m_vecSuspensionAxis.Normalize();
	}

	// want to get axle axis perpendicular to suspension axis
	vecTemp.Cross(m_vecSuspensionAxis, m_vecAxleAxis);
	m_vecAxleAxis.Cross(vecTemp, m_vecSuspensionAxis);
	m_vecAxleAxis.Normalize();

	m_fSusLength = (vecEnd - vecStart).Mag();
	m_fStaticDelta = m_fSusLength * (-m_pHandling->m_fSuspensionLowerLimit) / (m_pHandling->m_fSuspensionUpperLimit - m_pHandling->m_fSuspensionLowerLimit + m_fWheelRadius);
	m_fMaxTravelDelta = m_fSusLength * (m_pHandling->m_fSuspensionUpperLimit - m_pHandling->m_fSuspensionLowerLimit) / (m_pHandling->m_fSuspensionUpperLimit - m_pHandling->m_fSuspensionLowerLimit + m_fWheelRadius);

	if(GetConfigFlags().IsFlagSet(WCF_EXTEND_ON_UPDATE_SUSPENSION))
	{
		m_fCompression = 0.0f;
		m_fCompressionOld = 0.0f;
		m_fWheelCompression = 0.0f;
		GetConfigFlags().ClearFlag(WCF_EXTEND_ON_UPDATE_SUSPENSION);
	}
	else
	{		
		if( m_fCompression < -100.0f ||
			( m_pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DROP_SUSPENSION_WHEN_STOPPED ) &&
			  !m_pParentVehicle->m_nVehicleFlags.bEngineOn ) )
		{
			m_fCompression = m_fStaticDelta;
			m_fCompressionOld = m_fStaticDelta;
			m_fWheelCompression = m_fStaticDelta;
		}
	}

	return;
}

void CWheel::UpdateSuspension(const void* pDamageTexture, bool bSyncSkelToArtBody)
{
	SuspensionSetup(pDamageTexture);
	if(bSyncSkelToArtBody)
	{
		// Local matrices need to be kept up to date
		// IMPORTANT!!! Doing this per wheel is dumb!
		fragInstGta * pVehicleFragInst = m_pParentVehicle->GetVehicleFragInst();
		pVehicleFragInst->SyncSkeletonToArticulatedBody(true);
	}
}

////////////////////////////
// Setup probe segments for parent car to test
//
bool CWheel::ProbeGetTransfomedSegment(const Matrix34* pMatrix, phSegment& pProbeSegment)
{
	// want to move probes for steering wheels here if we have multiple probes

	pMatrix->Transform(m_aWheelLines.A, pProbeSegment.A);
	pMatrix->Transform(m_aWheelLines.B, pProbeSegment.B);

#if __DEV
	if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
	{
		CPhysics::ms_debugDrawStore.AddLine(RCC_VEC3V(pProbeSegment.A), RCC_VEC3V(pProbeSegment.B), Color32(1.0f,0.0f,0.0f),(fwTimer::GetTimeStepInMilliseconds()*3 )/2);
	}
#endif

	return true;
}

////////////////////////
// Manage the results from the probe batch test done by the parent vehicle with the world
//

RAGETRACE_DECL(CWheel_ProbeProcessResults);


bool CWheel::ProbeProcessResults(const Matrix34* pMatrix, WorldProbe::CShapeTestHitPoint& probeResult)
{
	RAGETRACE_SCOPE(CWheel_ProbeProcessResults);

	//Assert(IsDummy || !(ms_bUseImpactResults && GetFragChild() >= 0));

	CPhysical* pPhysical = NULL;
	m_fCompressionOld = m_fCompression;
	m_fCompression = 0.0f;

	//////////////
	// if we've got multiple probes per wheel then we need to resolve them here
	if(probeResult.GetHitDetected())
	{
		CEntity* pOtherEntity = NULL;
		if(probeResult.GetGenerationID() != phInst::INVALID_INDEX)// we put a fake number in the generation ID to signify this is a fake intersection for the virtual road probes
			pOtherEntity = CPhysics::GetEntityFromInst(probeResult.GetHitInst());

		// if we're hitting another vehicle, want to ignore impacts with wheels, heli rotors etc..
		bool bIgnoreComponent = false;
		if(pOtherEntity==m_pParentVehicle)
		{
			bIgnoreComponent = true;
		}
		else if(pOtherEntity && pOtherEntity->GetIsTypeVehicle())
		{
			CVehicle* pOtherVehicle = (CVehicle*)pOtherEntity;
			for(int nWheel=0; nWheel<pOtherVehicle->GetNumWheels(); nWheel++)
			{
				if(probeResult.GetHitComponent()==pOtherVehicle->GetWheel(nWheel)->GetFragChild())
					bIgnoreComponent = true;
			}
			if(pOtherVehicle->GetIsRotaryAircraft())
			{
				if(probeResult.GetHitComponent()==((CRotaryWingAircraft*)pOtherVehicle)->GetMainRotorDisc())
					bIgnoreComponent = true;
			}
		}

		// in MP collisions between certain vehicles can be disabled (eg ghost vehicles in a race)
		if (!bIgnoreComponent && m_pParentVehicle && pOtherEntity && NetworkInterface::AreInteractionsDisabledInMP(*m_pParentVehicle, *pOtherEntity))
		{
			bIgnoreComponent = true;
		}

		if(!bIgnoreComponent)
		{
			if(pOtherEntity && pOtherEntity->GetIsPhysical())
			{
				Assert(pOtherEntity!=m_pParentVehicle);
				pPhysical = (CPhysical*)pOtherEntity;
			}

			GetDynamicFlags().SetFlag(WF_HIT);
            m_vecHitPos = probeResult.GetHitPosition();
			m_vecHitCentrePos = probeResult.GetHitPosition();

			m_vecHitNormal = probeResult.GetHitNormal();
			m_nHitMaterialId = (u8)(probeResult.GetHitMaterialId() & 0xFF);
			m_nHitComponent = probeResult.GetHitComponent();

			GetDynamicFlags().ChangeFlag(WF_TOUCHING_PAVEMENT, IsMaterialIdPavement(probeResult.GetHitMaterialId()));

			m_fMaterialGrip = PGTAMATERIALMGR->GetMtlTyreGrip(m_nHitMaterialId);
			Assert(m_fMaterialGrip > 0.0f && m_fMaterialGrip < 5.0f);
			m_fMaterialWetLoss = (1.0f - g_weather.GetWetness()) + g_weather.GetWetness() * PGTAMATERIALMGR->GetMtlWetGripAdjust(m_nHitMaterialId);
	
			const u32 xmasWeather = uXmasWeather;//g_weather.GetTypeIndex(ATSTRINGHASH("XMAS", 0xaac9c895));
			const bool isXmasWeather = (g_weather.GetPrevTypeIndex() == xmasWeather) || (g_weather.GetNextTypeIndex() == xmasWeather);

			//Add snow grip loss here.
			if ((CNetwork::IsGameInProgress() REPLAY_ONLY(|| CReplayMgr::AllowXmasSnow())) && isXmasWeather)
			{
				float fSnowGripMult = 1.0f - g_weather.GetSnow() * sfSnowGripAdjust;
				m_fMaterialWetLoss = (PGTAMATERIALMGR->GetMtlWetGripAdjust(m_nHitMaterialId) * fSnowGripMult);
			}

			Assert(m_fMaterialWetLoss > 0.0f && m_fMaterialWetLoss < 2.0f);
            m_fMaterialDrag = PGTAMATERIALMGR->GetMtlTyreDrag(m_nHitMaterialId);
            Assert(m_fMaterialDrag >= 0.0f && m_fMaterialDrag <= 1.0f);
            m_fMaterialTopSpeedMult = PGTAMATERIALMGR->GetMtlTopSpeedMult(m_nHitMaterialId);
            Assert(m_fMaterialTopSpeedMult >= 0.0f && m_fMaterialTopSpeedMult <= 10.0f);

			m_fCompression = 1.0f - probeResult.GetHitTValue();
			m_fCompression *= m_fSusLength;

			// we have to deal with t values < 0.0 so hit pos may be above the suspension line
			// clamp it so we don't get weird torques in this case
			if(probeResult.GetHitTValue() < 0.0f)
			{
				pMatrix->Transform(m_aWheelLines.A, m_vecHitPos);
				m_vecHitCentrePos = m_vecHitPos;
			}
		}
	}

	m_fWheelCompression = GetCalculatedWheelCompression();

    m_pHitPhysical = pPhysical;
	if(m_pHitPhysical)
	{
		m_pParentVehicle->m_nVehicleFlags.bMayHaveWheelGroundForcesToApply = true;
	}

    if(pPhysical)
    {
		m_vecHitPhysicalOffset.Set(m_vecHitPos);
    }

	return true;
}

void CWheel::SetCompressionFromGroundHeightLocalAndHitPos(float fGroundHeightLocal, Vector3::Vector3Param vHitPos, Vector3::Vector3Param vHitNormal)
{
	// Wheel compression members
	float fCompression = m_fSusLength * (fGroundHeightLocal - m_aWheelLines.B.z) / (m_aWheelLines.A.z - m_aWheelLines.B.z);
	fCompression = rage::Min(fCompression, m_fSusLength);
	m_fCompression = m_fCompressionOld = fCompression;

	m_fWheelCompression = GetCalculatedWheelCompression();

	GetDynamicFlags().ClearFlag(WF_DUMMY_TRANSITION);

	// Other wheel member variables that need to be set
	GetDynamicFlags().SetFlag(WF_HIT);
	m_vecHitPos = vHitPos;
	m_vecHitCentrePos = vHitPos;
	m_vecHitNormal = vHitNormal;
	m_nHitMaterialId = 0;
	m_nHitComponent = 0;
	m_pHitPhysical = NULL;
	GetDynamicFlags().ClearFlag(WF_TOUCHING_PAVEMENT);
}

void CWheel::SetCompressionFromHitPos(const Matrix34* pParentMat, const Vector3& vecPos, bool bIsAHit, const Vector3& vecNormal)
{
	if(bIsAHit)
	{
		Vector3 vecStart(m_aWheelLines.A);
		Vector3 vecEnd(m_aWheelLines.B);
		pParentMat->Transform(vecStart);
		pParentMat->Transform(vecEnd);

		// Wheel compression members
		float fCompression = m_fSusLength * (vecPos.z - vecEnd.z) / (vecStart.z - vecEnd.z);
		fCompression = rage::Min(fCompression, m_fSusLength);
		m_fCompression = m_fCompressionOld = fCompression;
		m_fWheelCompression = GetCalculatedWheelCompression();

		// Other wheel member variables that need to be set
		GetDynamicFlags().SetFlag(WF_HIT);
		m_vecHitPos = vecPos;
		m_vecHitCentrePos = vecPos;
		m_vecHitNormal.Set(vecNormal);
		m_nHitMaterialId = 0;
		m_nHitComponent = 0;
		m_pHitPhysical = NULL;
		GetDynamicFlags().ClearFlag(WF_TOUCHING_PAVEMENT);
	}
	else
	{
		Reset();
		m_fWheelCompression = m_fCompression = m_fCompressionOld = 0.0f;
	}
}


dev_bool sbRecalculateCompression = false;
//
void CWheel::ProcessImpactResults()
{
	if(m_pParentVehicle->m_nVehicleFlags.bWheelsDisabled || m_pParentVehicle->m_nVehicleFlags.bWheelsDisabledOnDeactivation)
	{
		if(GetDynamicFlags().IsFlagSet(WF_HIT_PREV))
		{
			const Mat34V & mtxVeh = m_pParentVehicle->GetVehicleFragInst()->GetMatrix();
			Vec3V vHitCentrePos = RCC_VEC3V(m_aWheelLines.B) + ScalarV(m_fCompressionOld) * RCC_VEC3V(m_vecSuspensionAxis);
			vHitCentrePos = Transform(mtxVeh,vHitCentrePos);
			SetWheelHit( m_fCompressionOld, vHitCentrePos, RCC_VEC3V(m_vecSuspensionAxis), false);
			//grcDebugDraw::Cross(vHitCentrePos,0.2f,Color_green,-1);
		}
		return;
	}

	// recalculate compression, using m_vecHitCentrePos
	if(GetDynamicFlags().IsFlagSet(WF_HIT) && sbRecalculateCompression)
	{
		Vector3 vecLocalHitCentrePos;
		RCC_MATRIX34(m_pParentVehicle->GetVehicleFragInst()->GetMatrix()).UnTransform(m_vecHitCentrePos, vecLocalHitCentrePos);
		vecLocalHitCentrePos.Subtract(m_aWheelLines.B);
		m_fCompression = vecLocalHitCentrePos.Dot(m_vecSuspensionAxis);
	}

	m_fWheelCompression = GetCalculatedWheelCompression();

	m_fMaterialGrip = PGTAMATERIALMGR->GetMtlTyreGrip(m_nHitMaterialId);

	Assert(m_fMaterialGrip > 0.0f && m_fMaterialGrip < 5.0f);
	m_fMaterialWetLoss = (1.0f - g_weather.GetWetness()) + g_weather.GetWetness() * PGTAMATERIALMGR->GetMtlWetGripAdjust(m_nHitMaterialId);
	
	//   cache off "g_weather.GetTypeIndex(ATSTRINGHASH("XMAS", 0xaac9c895));" at load time and do a direct comparison here.
	const u32 xmasWeather = uXmasWeather;//g_weather.GetTypeIndex(ATSTRINGHASH("XMAS", 0xaac9c895));
	const bool isXmasWeather = (g_weather.GetPrevTypeIndex() == xmasWeather) || (g_weather.GetNextTypeIndex() == xmasWeather);

	//Add snow grip loss here.
	if (CNetwork::IsGameInProgress() && isXmasWeather)
	{
		float fSnowGripMult = 1.0f - g_weather.GetSnow() * sfSnowGripAdjust;
		m_fMaterialWetLoss = (PGTAMATERIALMGR->GetMtlWetGripAdjust(m_nHitMaterialId) * fSnowGripMult);
	}
	
	Assert(m_fMaterialWetLoss > 0.0f && m_fMaterialWetLoss < 2.0f);
    m_fMaterialDrag = PGTAMATERIALMGR->GetMtlTyreDrag(m_nHitMaterialId);
    Assert(m_fMaterialDrag >= 0.0f && m_fMaterialDrag <= 1.0f);
    m_fMaterialTopSpeedMult = PGTAMATERIALMGR->GetMtlTopSpeedMult(m_nHitMaterialId);
    Assert(m_fMaterialTopSpeedMult >= 0.0f && m_fMaterialTopSpeedMult <= 10.0f);

	//if( CVehicle::sm_bInDetonationMode )
	//{
	//	m_fMaterialGrip = 1.0f;
	//	m_fMaterialDrag = 0.0f;
	//	m_fMaterialTopSpeedMult = 1.0f;
	//}

	if(m_pHitPhysical)
	{
		bool allowedToSleepOnThisPhysical = false;
		if(m_vecHitNormal.z > 0.0f)
		{
			if(m_pHitPhysical->GetIsTypeObject())
			{
				const CObject* pObject = static_cast<const CObject*>(m_pHitPhysical.Get());
				// Don't disallow sleeping on broken off fragments. We will know if they are deleted because the RegdPhy m_pHitPhysical will
				//  get NULLed out. We are making the assumption that broken off fragments do not get teleported. Keeping a vehicle awake is
				//  expensive and it's a common occurrence for broken fragments to find their way under vehicle wheels. 
				// ^ Same logic applies for pickups, we should be deleting them upon pickup and not teleporting them.
				if(pObject->GetFragParent() || pObject->IsPickup() || pObject->IsObjectAPressurePlate())
				{
					allowedToSleepOnThisPhysical = true;
				}
			}
			else if(m_pHitPhysical->GetIsTypeVehicle())
			{
				const CVehicle* pParentVehicle = static_cast<const CVehicle*>(m_pHitPhysical.Get());
				// Allow sleeping on wrecked vehicles assuming nobody will teleport them out from under us. Same logic as broken frags
				if( pParentVehicle->GetStatus() == STATUS_WRECKED)
				{
					allowedToSleepOnThisPhysical = true;
				}
			}
		}
		else
		{
			// If the car is on its back and the object is above the vehicle, it's alright to sleep. 
			allowedToSleepOnThisPhysical = true;
		}

		if(!allowedToSleepOnThisPhysical)
		{
			m_pParentVehicle->m_nVehicleFlags.bDontSleepOnThisPhysical = true;
		}

		m_pParentVehicle->m_nVehicleFlags.bRestingOnPhysical = true;
	}
}

//float IntersectSegSphere(Vec3V_In vA, Vec3V_In vB, Vec3V_In vC, ScalarV_In fR)
//{
//	// RealQuadratic below requires floats, so there is some Vec3V to float conversion...
//	// maybe we should have a version of RealQuadratic that takes ScalarV.
//	const Vec3V vA0 = vA - vC;
//	const Vec3V vB0 = vB - vC;
//	const Vec3V vAB = vB - vA;
//	const ScalarV fRSq = square(fR);
//	if(IsGreaterThanAll(MagSquared(vA0),fRSq) && IsLessThanAll(MagSquared(vB0),fRSq))
//	{
//		// r^2 = fA + fB * t + fC * t^2
//		const float fA0x = vA0.GetXf(), fA0y = vA0.GetYf(), fA0z = vA0.GetZf();
//		const float fABx = vAB.GetXf(), fABy = vAB.GetYf(), fABz = vAB.GetZf();
//		float fA = square(fA0x) + square(fA0y) + square(fA0z) - fRSq.Getf();
//		float fB = 2.0f * (fA0x * fABx + fA0y * fABy + fA0z * fABz);
//		float fC = square(fABx) + square(fABy) + square(fABz);
//		Assert(fC>0.01f);
//		float fT0, fT1;
//		int nSol = RealQuadratic(fB/fC,fA/fC,&fT0,&fT1);
//		// Since we check for the segment starting outside the sphere and ending inside, there should be two solutions.
//		// By convention in RealQuadratic, the second solution should be between 0 and 1 given this setup.
//		if(nSol==2 && fT1>=0.0f && fT1<1.0f)
//		{
//			return fT1;
//		}
//	}
//	return -1.0f;
//}

void CWheel::SetWheelHit(float fCompression, Vec3V_In vHitPos, Vec3V_In vHitNormal, const bool bSetToDefaultMaterial)
{
	GetDynamicFlags().SetFlag(WF_HIT);
	m_vecHitPos = RCC_VECTOR3(vHitPos);
	m_vecHitCentrePos = RCC_VECTOR3(vHitPos);
	m_vecHitNormal = RCC_VECTOR3(vHitNormal);

	if(bSetToDefaultMaterial)
	{
		m_nHitMaterialId = (u8)(PGTAMATERIALMGR->g_idTarmac);
	}

	m_nHitComponent = 0;
	GetDynamicFlags().ClearFlag(WF_TOUCHING_PAVEMENT);
	m_fMaterialGrip = PGTAMATERIALMGR->GetMtlTyreGrip(m_nHitMaterialId);
	m_fMaterialWetLoss = 1.0f;
	m_fMaterialDrag = PGTAMATERIALMGR->GetMtlTyreDrag(m_nHitMaterialId);
	m_fMaterialTopSpeedMult = PGTAMATERIALMGR->GetMtlTopSpeedMult(m_nHitMaterialId);
	m_fCompression = fCompression;
	m_fWheelCompression = GetCalculatedWheelCompression();
	m_pHitPhysical = NULL;
}

void CWheel::StartDummyTransition()
{
	BANK_ONLY(if(CVehicleAILodManager::ms_bUseDummyWheelTransition))
	{
		GetDynamicFlags().SetFlag(WF_DUMMY_TRANSITION);
		GetDynamicFlags().ClearFlag(WF_DUMMY_TRANSITION_PREV);
		m_fPreviousTransitionCompression = m_fTransitionCompression = GetCompression();
	}
}

#if __DEV
bool gbAlwaysRemoveWheelImpacts = false;
#endif

CWheel::ProcessImpactCommonData::ProcessImpactCommonData(const CVehicle& rParentVehicle)
{
	// Put various constants that we need in a vector register. This should be much
	// cheaper than loading them individually from unaligned float variables.
	const Vec4V constantsV(Vec::V4VConstant<
			FLOAT_TO_INT(0.0f),// not needed anymore
			FLOAT_TO_INT(1.0f/sfReduceTreadWidthMaxSpeed),
			FLOAT_TO_INT(sfWheelImpactSpeedDriveOverPedLimitSq),
			FLOAT_TO_INT(1.0f)
			>());

	// Assert(fabsf(constantsV.GetYf() - 1.0f/sfReduceTreadWidthMaxSpeed) <= 0.0001f);
	// Assert(fabsf(constantsV.GetZf() - sfWheelImpactSpeedDriveOverPedLimitSq) <= 0.0001f);

	const CPed* pDriver = rParentVehicle.GetDriver();
	const CTrailer* pAttachedTrailer = rParentVehicle.GetAttachedTrailer();

	// Get the velocity and throttle.
	const Vec3V velocityV = VECTOR3_TO_VEC3V(rParentVehicle.GetVelocity());
	const ScalarV throttleV(rParentVehicle.GetThrottle());

	// Separate the constants.
	const ScalarV wheelImpactSpeedDriveOverPedLimitSqV = constantsV.GetZ();
	const ScalarV invReduceTreadWidthMaxSpeedV = constantsV.GetY();
	const ScalarV oneV = constantsV.GetW();	// Could use V_ONE if we have something better to put in the W component of constantW.

	// Square the velocity.
	const ScalarV velocitySqV = MagSquared(velocityV);

	// Compute the tread width multiplier.
	//	float fTreadWidthMult = ((fVelocitySq < sfReduceTreadWidthMaxSpeed) && !configFlags.IsFlagSet(WCF_BIKE_WHEEL)) ? (fVelocitySq / sfReduceTreadWidthMaxSpeed) : 1.0f;
	const ScalarV treadWidthMultIfNotBikeWheelV = Min(Scale(velocitySqV, invReduceTreadWidthMaxSpeedV), oneV);

	//	bool bIsStill = fVelocitySq < sfWheelImpactSpeedDriveOverPedLimitSq;
	const BoolV isStillV = IsLessThan(velocitySqV, wheelImpactSpeedDriveOverPedLimitSqV);

	// Combine together the computed values into one vector.
	const Vec4V computedValuesV(
			velocitySqV,
			treadWidthMultIfNotBikeWheelV,
			ScalarV(0.0f),// not needed
			ScalarV(isStillV.GetIntrin128()));

	// Store everything.
	m_Velocity = velocityV;
	m_ComputedValues = computedValuesV;
	m_bPlayerVehicle = pDriver && pDriver->IsPlayer();
	m_pDriver = pDriver;
	m_pAttachedTrailer = pAttachedTrailer;

#if __ASSERT
	// Should hold up:
	//	static dev_float sfThrottleDriveOverThreshold = 0.3f;
	//	const float fVelocitySq1 = rParentVehicle.GetVelocity().Mag2();
	//	Assert(fabsf(GetVelocitySq() - fVelocitySq1) <= 0.0001f);
	//	bool bIsStill = (fVelocitySq1 < sfWheelImpactSpeedDriveOverPedLimitSq);
	//	Assert((bIsStill != 0) == (GetIsStill() != 0));
	//	Assert((bDriveOverVehicle != 0) == (GetDriveOverVehicle() != 0));
#endif
}

//
void CWheel::ProcessImpact(const ProcessImpactCommonData& commonData, phCachedContactIterator &impacts, const atUserArray<CVehicle*> &aVehicleFlattenNormals)
{
#if __BANK
	// Config variables
	TUNE_GROUP_BOOL(WHEEL_TUNE, ACTIVATE_BEST_CONTACT, true);
#endif
#if __DEV
	static bool DEBUG_DRAW_WHEEL_IMPACTS = false;
#endif
	
	unsigned int dynamicFlags = GetDynamicFlags().GetAllFlags();

	// Sometimes we will have precomputeimpacts called twice, due to pushes,
	// a wheel can get a good hit against a car the first time but not the second time. 
	// So ignore hits with cars from pre push precomputeimpacts, as they can cause vehicles to drive over each other.
	if((dynamicFlags & WF_HIT) && m_pHitPhysical && m_pHitPhysical->GetIsTypeVehicle())
	{
		dynamicFlags &= ~WF_HIT;

		m_fCompression = m_fCompressionOld;
		m_pHitPhysical = NULL;
	}

	// Cache some values
	const ScalarV sOldCompression(m_fCompressionOld);
	const Mat34V & mtxVeh = m_pParentVehicle->GetVehicleFragInst()->GetMatrix();
	const bool bParentNotWrecked = (m_pParentVehicle->GetStatus() != STATUS_WRECKED);
	const Vec3V vWheelLineB(m_aWheelLines.B);
	const Vec3V vSuspensionAxis(m_vecSuspensionAxis);
	const int iWheelFragChild = GetFragChild();
	const unsigned int configFlags = (unsigned int)GetConfigFlags().GetAllFlags();
	bool bUseBikeNormalLimit = false;
	bool bUseHigherSideImpactThreshold = false;

	Vec3V vVehicleUp = mtxVeh.c();
	ScalarV sUpNormalLimit(V_ZERO);
	bool bBikeReversing = false;
	bool bBeingPickedUp = false;
	if(configFlags & (WCF_BIKE_WHEEL ))
	{
		if(configFlags & WCF_BIKE_CONSTRAINED_COLLIDER)
		{
			vVehicleUp = VECTOR3_TO_VEC3V(static_cast<CBike*>(m_pParentVehicle)->GetBikeWheelUp());
			bBikeReversing = m_pParentVehicle->GetThrottle() < 0.0f || ( DotProduct(VEC3V_TO_VECTOR3(m_pParentVehicle->GetTransform().GetB()), m_pParentVehicle->GetVelocity()) < 0.0f);
			if( bBikeReversing )
			{
				sUpNormalLimit = ScalarVFromF32(sfWheelImpactBikeUpLimitInReverse);
			}
		}
		CBike *pBike = static_cast<CBike*>(m_pParentVehicle);
		bUseBikeNormalLimit = (configFlags & (WCF_BIKE_FALLEN_COLLIDER )) || pBike->m_nBikeFlags.bGettingPickedUp;
		bBeingPickedUp = pBike->m_nBikeFlags.bGettingPickedUp;
	}

	const bool bPlayerVehicle = commonData.m_bPlayerVehicle;

	//when we are going slower reduce how much we lower the compression when hitting the side of the tyre, this improves stability when parked on bumpy ground.
	const ScalarV treadWidthMultV = (configFlags & WCF_BIKE_WHEEL) ? ScalarV(V_ONE) : commonData.GetTreadWidthMultIfNotBikeWheelV();

#if __ASSERT
	// Should hold up:
	//	const float fTreadWidthMult1 = (configFlags & WCF_BIKE_WHEEL) ? 1.0f : commonData.GetTreadWidthMultIfNotBikeWheel();
	//	const float fVelocitySq = commonData.GetVelocitySq();
	//	float fTreadWidthMult1 = ((fVelocitySq < sfReduceTreadWidthMaxSpeed) && !configFlags.IsFlagSet(WCF_BIKE_WHEEL)) ? (fVelocitySq / sfReduceTreadWidthMaxSpeed) : 1.0f;
	//	Assertf(fabsf(fTreadWidthMult1 - fTreadWidthMult) <= 0.00001f, "%f != %f", fTreadWidthMult1, fTreadWidthMult);
#endif

	const ScalarV sWheelRadiusIncBurst = LoadScalar32IntoScalarV(GetWheelRadiusIncBurst());
	const ScalarV sWheelRadius = GetWheelRadiusV();
	const ScalarV sWheelRadiusSq = Scale(sWheelRadius, sWheelRadius);
	ScalarV susLengthV = LoadScalar32IntoScalarV(m_fSusLength);

    float wheelRimRadius = m_fWheelRimRadius;

    if( m_pHandling->GetCarHandlingData() &&
        ( m_pHandling->GetCarHandlingData()->aFlags & CF_FIX_OLD_BUGS ) )
    {
        wheelRimRadius = m_fInitialRimRadius;
    }

	ScalarV susLengthAllowTyreClip = Add( susLengthV, Scale(Subtract(sWheelRadiusIncBurst, LoadScalar32IntoScalarV( wheelRimRadius )), LoadScalar32IntoScalarV(sfAllowableTyreClipAmount)) );

    if(m_pParentVehicle->pHandling->hFlags & HF_TYRES_CAN_CLIP)
    {	
		susLengthV = susLengthAllowTyreClip;
	}

	// Cap the compression
	m_fCompression = Min(m_fCompression, Subtract(susLengthV, sWheelRadiusIncBurst).Getf());
	const ScalarV sStartingCompression(m_fCompression);

	if(m_pParentVehicle->pHandling->hFlags & HF_TYRES_RAISE_SIDE_IMPACT_THRESHOLD)
	{
		bUseHigherSideImpactThreshold = true;
	}

	// Check how many child bounds this wheel has (often this will just be one).
	int numWheelBounds = 1;
	for(int i = 1; i < MAX_WHEEL_BOUNDS_PER_WHEEL; i++)
	{
		if(GetFragChild(i) >= 0)
		{
			numWheelBounds++;
		}
		else
		{
			break;
		}
	}


	bool hackRemoveBlockerBounds = ( m_pParentVehicle->InheritsFromTrailer() &&
		( ( MI_TRAILER_TRAILERLARGE.IsValid() && m_pParentVehicle->GetModelIndex() == MI_TRAILER_TRAILERLARGE ) ||
		  ( MI_TRAILER_TRAILERSMALL2.IsValid() && m_pParentVehicle->GetModelIndex() == MI_TRAILER_TRAILERSMALL2 ) ) );

	// Iterate over contacts
	bool bDontActivateImpacts = false;
	bool bBestContactShouldBeIgnoredDueToCompressionDelta = false;
	phContact* pContactToActivate = NULL;
	for(phCachedContactIteratorElement* pNextContact = impacts.ResetToFirstActiveContact();
			pNextContact; pNextContact = impacts.NextActiveContact())
	{
		const int myComponent = pNextContact->m_MyComponent;

		for(int i = 1; i < numWheelBounds; i++)
		{
			if(myComponent == GetFragChild(i))
			{ 
				if( hackRemoveBlockerBounds &&
					i == numWheelBounds - 1 )
				{
					impacts.DisableImpact();
				}

				phInst * pOtherInstance = impacts.GetOtherInstance();
				CEntity * pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);

				float fOriginalFriction = 1.0f;
				if(bPlayerVehicle)
				{
					fOriginalFriction = impacts.GetFriction();
					//make sure we slide off things when our tyres hit.
					impacts.SetFriction(0.0f);
					impacts.SetElasticity(0.0f);
				}

				// Activate impacts when a vehicle is not supposed to drive over another vehicle.
				if(pOtherInstance) 
				{  
					int  iOtherClassType = pOtherInstance->GetClassType();
					if(iOtherClassType == PH_INST_FRAG_VEH || iOtherClassType == PH_INST_VEH)
					{
						// If we are hitting a forklift check if we are hitting the forks.
						if(pOtherEntity->GetModelIndex() == MI_CAR_FORKLIFT)
						{
							phBound* pOtherBound = pOtherInstance->GetArchetype()->GetBound();
							if(pOtherBound->GetType()==phBound::COMPOSITE)
							{
								phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pOtherBound);
								if(pBoundComposite->GetTypeFlags(impacts.GetOtherComponent())&ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE)
								{
									bDontActivateImpacts = true;
									break;
								}
							}
						}

						const CVehicle* pOtherVehicle = static_cast<const CVehicle*>(pOtherInstance->GetUserData());
						if((configFlags & WCF_BIKE_WHEEL) && ((!pOtherVehicle->InheritsFromTrain() && i == 0) || i > 0))
						{
							bDontActivateImpacts = true;
							break;
						}
						else
						{
							if( bPlayerVehicle && (pOtherEntity == m_pPrevHitPhysical || pOtherVehicle->InheritsFromTrain()) )
							{
								impacts.DisableImpact();
							}
							else if( m_pParentVehicle->GetModelIndex() != MI_CAR_RCBANDITO ||
									 ( !pOtherVehicle->InheritsFromPlane() &&
									   !pOtherVehicle->InheritsFromHeli() ) )
							{
								bDontActivateImpacts = true;
								break;
							}
						}
	
					}
					else if(iOtherClassType != PH_INST_FRAG_PED && iOtherClassType != PH_INST_PED && iOtherClassType != PH_INST_PED_LEGS && iOtherClassType != PH_INST_PROJECTILE)
					{
						if(commonData.m_pDriver || m_pParentVehicle->InheritsFromTrailer())
						{
							// If the normal is down then we dont want to activate the impact
							const ScalarV fUpNormal = Dot(pNextContact->m_MyNormal,vVehicleUp);
							// If the bike is vertical we dont want to activate the impact.
							const ScalarV fVehUpNormal = Dot(vVehicleUp,Vec3V(V_Z_AXIS_WONE));

							if((configFlags & WCF_BIKE_WHEEL))
							{
								if( ( IsLessThanAll(Abs(fUpNormal),LoadScalar32IntoScalarV(sfWheelImpactNormalLimitBike)) && IsGreaterThanAll(Abs(fVehUpNormal),LoadScalar32IntoScalarV(sfWheelImpactNormalLimitBikeUp)) ) ||
									( m_pParentVehicle->InheritsFromBike() && static_cast< CBike* >( m_pParentVehicle )->m_fOffStuntTime > 0.0f ) )
								{
									bDontActivateImpacts = true;
									break;
								}
							}
							else 
							{
								if( !m_pParentVehicle->HasRamp() && 
									( IsLessThanAll(Abs(fUpNormal),LoadScalar32IntoScalarV(sfWheelImpactNormalLimitCar)) || (m_pParentVehicle->pHandling->hFlags & HF_ALT_EXT_WHEEL_BOUNDS_BEH) || (m_pParentVehicle->pHandling->hFlags & HF_EXT_WHEEL_BOUNDS_COL) ) )
								{
									bDontActivateImpacts = true;
									impacts.SetFriction(fOriginalFriction);

									break;
								}
							}
						}
						else
						{
							bDontActivateImpacts = true;
							break;
						}

						bool bDisableBlockerImpact = true;

						// Since the Bandito has small wheels that end up clipping through the ground a lot, make the wheel blockers collide against the ground if we get side collisions
						if( iOtherClassType != PH_INST_EXPLOSION &&
							( ( MI_CAR_RCBANDITO.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_RCBANDITO ) || 
							  ( MI_CAR_BRUISER.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_BRUISER ) ||
							  ( m_pParentVehicle->InheritsFromAutomobile() && m_pParentVehicle->pHandling->GetCarHandlingData() && m_pParentVehicle->pHandling->GetCarHandlingData()->aFlags & CF_ENABLE_WHEEL_BLOCKER_SIDE_IMPACTS ) ) )
						{
							TUNE_GROUP_BOOL( BANDITO_DEBUG, ENABLE_WHEEL_BLOCKER_MAP_COLLISION, true );
							if( ENABLE_WHEEL_BLOCKER_MAP_COLLISION )
							{
								const Vec3V vNormal( pNextContact->m_MyNormal );
								const ScalarV fSideNormal = Abs( Dot( vNormal, mtxVeh.a() ) );

								TUNE_GROUP_FLOAT( BANDITO_DEBUG, SIDE_NORMAL_FOR_BLOCKER_COLLISION, 0.8f, 0.0f, 1.0f, 0.01f );
								if( fSideNormal.Getf() >= SIDE_NORMAL_FOR_BLOCKER_COLLISION )
								{
									bDisableBlockerImpact = false;
								}
							}
						}

						if( bDisableBlockerImpact )
						{
							impacts.DisableImpact();
						}
					}
				}

				break;
			}
		}

		if(myComponent == iWheelFragChild)
		{
			bool bDisableImpact = true;			
			//grcDebugDraw::Cross(impacts.GetOtherPosition(),0.2f,Color_red,-1);

			// Cache the hit instance and entity
			phInst * pOtherInstance = impacts.GetOtherInstance();
			CEntity * pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);

			if(pOtherEntity && pOtherEntity->GetIsTypeObject())
			{
				//Pop the tires for certain impacts.
				//Note: ProcessImpactPopTires() checks GetIsTypeObject(), but we do it here as well
				//so we can avoid the function call in the common case where we are not hitting an object,
				//for performance reasons.
				if (ProcessImpactPopTires( impacts, *pOtherEntity))
				{
					impacts.DisableImpact();
					continue;
				}	

				// disable the impact for some pickups that are collectable in vehicle
				if (static_cast<CObject*>(pOtherEntity)->IsPickup())
				{
					CPickup* pPickup = static_cast<CPickup*>(pOtherEntity);

					if (pPickup->GetPickupData() && pPickup->GetPickupData()->GetCollectableInVehicle())
					{
						impacts.DisableImpact();
						continue;
					}
				}
			}
			if( pOtherEntity && pOtherEntity->GetIsTypeVehicle() )
			{
				static_cast<CVehicle*>(pOtherEntity)->ApplyWeaponBladeWheelImpacts( this, impacts.GetOtherComponent() );
			}

			int nOtherInstType = PH_INST_GTA;
			if(pOtherInstance)
			{
				nOtherInstType = pOtherInstance->GetClassType();
			}

			// Stop wheels penetrating the ground.
			const int kLifeTimeCull = 1;
			if(pNextContact->m_Contact->GetLifetime() > kLifeTimeCull && nOtherInstType != PH_INST_PED)
			{				
				impacts.DisableImpact();
				continue;
			}

			// Respect "doesn't affect vehicles" flag
			if(IsFragInstClassType(nOtherInstType))
			{
				const fragInst* otherFrag = static_cast<const fragInst*>(pOtherInstance);
				fragPhysicsLOD* otherPhysics = otherFrag->GetTypePhysics();
				fragTypeChild* otherChild = otherPhysics->GetAllChildren()[pNextContact->m_OtherComponent];
				fragTypeGroup* otherGroup = otherPhysics->GetAllGroups()[otherChild->GetOwnerGroupPointerIndex()];
				if (otherGroup->GetDoesntAffectVehicles() )
				{
					impacts.DisableImpact();
					continue;
				}

#if __BANK
				if(ms_bDontCompressOnRecentlyBrokenFragments)
#else
				if(1)
#endif
				{
					// At the moment the wheel integrator treats objects the wheel is touching as immovable. This can create unrealistic movement
					//   when breaking fragments if a bit gets under the wheel. Here I am trying to avoid compressing the wheel these situations.
					//   The car must be moving fast, the fragment must be light and must have broken off recently. We don't want this to trigger 
					//   when driving over a fragment on the ground. 
					const float fMinVehicleVelocitySqToIgnoreBrokenFrags = 5.0f*5.0f;
					const float fMaxFragMassToIgnore = 200.0f;
					const float fTimeToIgnoreBrokenFrags = 0.5f;
					const float fVelocitySq = commonData.GetVelocitySq();
					if(fVelocitySq > fMinVehicleVelocitySqToIgnoreBrokenFrags)
					{
						if(const fragCacheEntry* otherCachEntry = otherFrag->GetCacheEntry())
						{
							if(otherCachEntry->GetHierInst()->age < fTimeToIgnoreBrokenFrags)
							{
								if(const phCollider* pOtherCollider = impacts.GetOtherCollider())
								{
									if(pOtherCollider->GetMass() < fMaxFragMassToIgnore)
									{
										// Keep the contact and make the vehicle infinitely massive so it cannot change the trajectory of the vehicle at all. 
										impacts.SetMassInvScales(0.0f,1.0f);
										continue;
									}
								}
							}
						}
					}
				}
			}

			// Don't let the wheel collide with a trailer we're attached to
			const CTrailer* pTrailer = commonData.m_pAttachedTrailer;
			if(pTrailer)
			{
				if (pOtherEntity == pTrailer)
				{					
					impacts.DisableImpact();
					continue;
				}
			}

			// Calc new compression
			const ScalarV sCurrentCompression = bBestContactShouldBeIgnoredDueToCompressionDelta ? sStartingCompression : LoadScalar32IntoScalarV(m_fCompression);// reset the current compression to the original compression so the next good impact should get set rather than this one.

			const ScalarV deltaZV = Dot(Subtract(pNextContact->m_OtherPosition, mtxVeh.GetCol3()), mtxVeh.GetCol2());
			Assert(IsFiniteAll(deltaZV));
			const Vec3V vMyPos = UnTransformOrtho(mtxVeh,pNextContact->m_MyPosition);
			Vec3V vDelta = GetFromTwo<Vec::X1,Vec::Y1,Vec::Z2>(vMyPos,Vec3V(deltaZV));
			vDelta = vDelta - vWheelLineB;

			const Vec3V vDeltaSq = square(vDelta);
			const ScalarV sDeltaSqY = ScalarV(vDeltaSq.GetY());
			const ScalarV sDeltaSqX = ScalarV(vDeltaSq.GetX());

			const ScalarV compTemp1V(vDelta.GetZ());

			// The code below does what this block used to do:
			//	ScalarV sCompression(vDelta.GetZ());
			//	if(IsLessThanAll(sDeltaSqY, sWheelRadiusSq))
			//	{
			//		sCompression -= sWheelRadius - Sqrt(sWheelRadiusSq - sDeltaSqY);
			//		const ScalarV sHalfTreadWidth = Scale(ScalarVFromF32(m_fWheelWidth),ScalarV(V_HALF));
			//		ScalarV sHalfTreadWidthSq = square(sHalfTreadWidth);
			//		if(IsLessThanAll(sDeltaSqX, sHalfTreadWidthSq))
			//		{
			//			//work out the change in compression if this impact is on the side of the tyre
			//			sCompression -= sHalfTreadWidth - Sqrt(sHalfTreadWidthSq - Scale(sDeltaSqX, ScalarVFromF32(fTreadWidthMult)) );
			//		}
			//		else//must be on the edge of the tyre
			//		{
			//			sCompression -= sHalfTreadWidth;
			//		}
			//	}
			//	else
			//	{
			//		sCompression.Setf(-100.0f);
			//	}
			// It uses vector selection masks to avoid branches, and computes the two square roots
			// at once, using a Vec2V.

			// Note: we could precompute this from m_fWheelWidth and store it in CWheel:
			const ScalarV wheelWidthV = ScalarVFromF32(m_fWheelWidth);
			const ScalarV sHalfTreadWidth = Scale(wheelWidthV, ScalarV(V_HALF));
			const ScalarV sHalfTreadWidthSq = Scale(sHalfTreadWidth, sHalfTreadWidth);

			const ScalarV toCalcRootFor1V = sWheelRadiusSq - sDeltaSqY;
			const ScalarV toCalcRootFor2V = SubtractScaled(sHalfTreadWidthSq, sDeltaSqX, treadWidthMultV);

			const Vec2V toCalcRootFor12V(toCalcRootFor1V, toCalcRootFor2V);
			const Vec2V rootsV = Sqrt(toCalcRootFor12V);

			const ScalarV compTemp2V = compTemp1V - sWheelRadius - sHalfTreadWidth + rootsV.GetX();

			//work out the change in compression if this impact is on the side of the tyre
			const BoolV onSideOfTyreV = IsLessThan(sDeltaSqX, sHalfTreadWidthSq);	// else must be on the edge of the tyre
			const ScalarV compTemp3V = compTemp2V + And(ScalarV(onSideOfTyreV.GetIntrin128()), rootsV.GetY());

			ScalarV sCompression = SelectFT(IsLessThan(sDeltaSqY, sWheelRadiusSq), ScalarV(-100.0f), compTemp3V);

			// Get some info about the other instance
			bool bOtherInstanceIsTrain = false;
			const CVehicle * pOtherVehicle = NULL;

			bool bIgnore = false;

			bool bIsImpactWithVeh = false;
			if(nOtherInstType == PH_INST_FRAG_VEH || nOtherInstType == PH_INST_VEH)
			{
				bIsImpactWithVeh = true;

				Assert(static_cast<const CVehicle*>(pOtherInstance->GetUserData()) == pOtherEntity);
				pOtherVehicle = static_cast<const CVehicle*>(pOtherEntity);

				//Store the fact that we are driving on another vehicle.
				m_pParentVehicle->m_nVehicleFlags.bDrivingOnVehicle = true;

				// Ignore impacts with wheels on other vehicles.
				const int otherNumWheels = pOtherVehicle->GetNumWheels();
				for(int i=0; i<otherNumWheels; i++)
				{
					if( pNextContact->m_OtherComponent==pOtherVehicle->GetWheel(i)->GetFragChild() )
					{
						bIgnore = true;
						break;
					}
				}

				if( !bIgnore &&
					m_pParentVehicle->HasRamp() &&
					( pOtherVehicle->GetVehicleType() == VEHICLE_TYPE_CAR ||
					  pOtherVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE ||
					  pOtherVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER || 
					  pOtherVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE ||
					  pOtherVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE ||
					  pOtherVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE ) )
				{
					Vec3V otherVehiclePosition = pOtherVehicle->GetTransform().GetPosition();
					otherVehiclePosition = m_pParentVehicle->GetTransform().UnTransform( otherVehiclePosition );
					static float sfRampCarMinZOffset = -0.5f;
					static float sfRampCarMaxVelocityForWheelImpact = 400.0f;

					if( pOtherVehicle->m_hasHitRampCar ||
						otherVehiclePosition.GetZf() > sfRampCarMinZOffset ||
						m_pParentVehicle->GetVelocity().Mag2() > sfRampCarMaxVelocityForWheelImpact )
					{
						bIgnore = true;
						break;
					}
				}

				bOtherInstanceIsTrain = pOtherVehicle->InheritsFromTrain();

				if(!bIgnore)
				{
					const int numFlattenNormals = aVehicleFlattenNormals.GetCount();
					for(int i = 0; i < numFlattenNormals; i++)
					{
						if(aVehicleFlattenNormals[i] == pOtherVehicle)
						{
							bIgnore = true;
							break;
						}
					}
				}
			}
			else if(nOtherInstType == PH_INST_FRAG_PED)// Don't let the tyre drive over peds if vehicle is not moving or if the vehicle is moving fast.
			{
				// B*1756261: Don't allow peds to trigger the suspension when the vehicle is on its side; let the impact be treated normally.
				if(m_pParentVehicle->IsOnItsSide() || m_pParentVehicle->IsUpsideDown())
				{
					continue;				
				}

				if(!commonData.m_pDriver && (configFlags & (WCF_BIKE_WHEEL | WCF_BIKE_FALLEN_COLLIDER)) && !bBeingPickedUp)// we dont want bike wheels of fallen colliders to hit peds.
				{
					bIgnore = true;
				}
				else if(commonData.GetIsStill() && m_pPrevHitPhysical != pOtherEntity)// if we're already on this ped then we want to process the impact normally
				{
					continue;
				}
				else if(commonData.GetVelocitySq() > sfDriveThroughPedSpeedSq && !(configFlags & (WCF_BIKE_WHEEL | WCF_QUAD_WHEEL)) && (pOtherEntity && pOtherEntity->GetIsTypePed() && static_cast<const CPed*>(pOtherEntity)->GetPedType() != PEDTYPE_ANIMAL))
				{
					bIgnore = true;
				}
			}
			// B*1756261: Don't allow peds to trigger the suspension when the vehicle is on its side; let the impact be treated normally.
			else if(nOtherInstType == PH_INST_PED)
			{			
				//if(rParentVehicle.IsOnItsSide() || rParentVehicle.IsUpsideDown())
				{
					continue;				
				}
			}
			else if(nOtherInstType == PH_INST_OBJECT || nOtherInstType == PH_INST_FRAG_OBJECT)
			{
				// Question: is it intentional that this affects all subsequent impacts, rather than just this one?

				sUpNormalLimit = ScalarVFromF32(sfWheelImpactUpLimitAfterExterWheelCollision);
			}

			if(commonData.GetVelocitySq() == 0.0f && impacts.GetOtherCollider() != NULL && !impacts.GetOtherCollider()->GetSleep()->IsAsleep())
			{
				if(const phCollider* pOtherCollider = impacts.GetOtherCollider())
				{
					if(pOtherCollider->GetSleep() && !pOtherCollider->GetSleep()->IsAsleep())
					{
						// If we're inactive and the other object isn't, wake up. 
						m_pParentVehicle->m_nVehicleFlags.bDontSleepOnThisPhysical = m_pParentVehicle->m_nVehicleFlags.bRestingOnPhysical = true;
					}
				}
			}

			// Check for valid normal
			const Vec3V vNormal(pNextContact->m_MyNormal);
			const ScalarV fSideNormal = Abs(Dot(vNormal,mtxVeh.a()));

			static dev_float sfChimeraNormalLimit = 0.7f;

			// Select the correct fSideNormalLimit
			ScalarV fSideNormalLimit;
			if(bUseBikeNormalLimit)
			{
				fSideNormalLimit = sGetWheelImpactNormalSideLimitBikeV();
			}
			else if( bIsImpactWithVeh && !bOtherInstanceIsTrain &&!bUseHigherSideImpactThreshold )
			{
				fSideNormalLimit = sGetWheelImpactNormalSideLimitCarVsCarV();
			}
			else if( ( ( configFlags & WCF_QUAD_WHEEL ) && NULL == m_pParentVehicle->GetDriver() && m_pParentVehicle->GetNumWheels() == 3 ) || 
                     m_pParentVehicle->GetModelIndex() == MI_CAR_BARRAGE ) // If quad with three wheels (like chimera) with no driver on
			{
				fSideNormalLimit = ScalarVFromF32(sfChimeraNormalLimit);
			}
			else
			{
				fSideNormalLimit = sGetWheelImpactNormalSideLimitV();
			}

			const unsigned int bSideImpact = IsGreaterThanAll(fSideNormal,fSideNormalLimit);
			if(bSideImpact)
			{
				dynamicFlags |= WF_SIDE_IMPACT;

				bIgnore = true;
			}

			bool bContactShouldBeIgnoredDueToCompressionDelta = false;

			if( m_pParentVehicle->InheritsFromAmphibiousQuadBike() && static_cast<CAmphibiousQuadBike*>(m_pParentVehicle)->IsWheelsFullyIn() )
			{
				bIgnore = true;
			}
				
			if(commonData.GetIsStill() && !bIgnore && IsGreaterThanAll(Subtract(sCompression, sOldCompression), ScalarV(sfMaxCompressionChangeWheelStill)) )
			{
				if((dynamicFlags & WF_HIT))// if we already have a hit ignore this one
				{
					bIgnore = true;
				}
				else // we don't have a hit so use this one but if another hit comes along use that
				{
					if(!m_pParentVehicle->InheritsFromAmphibiousQuadBike() && !static_cast<CAmphibiousQuadBike*>(m_pParentVehicle)->IsWheelsFullyOut())
					{
						bContactShouldBeIgnoredDueToCompressionDelta = true;
					}
				}
			}

			if(!bIgnore)
			{
				const ScalarV fUpNormal = Dot(vNormal,vVehicleUp);
				const BoolV greatCompressionV = IsGreaterThan(sCompression,sCurrentCompression);
				const BoolV greatNormalV = IsGreaterThanOrEqual(fUpNormal,sUpNormalLimit);
				if(IsFalse(And(greatCompressionV, greatNormalV)))
				{
					// Debug code to catch contacts which should have impacts activated but there is already a deeper contact. Might be caused by hitting a vehicle with a deeper contact before this.
// 					if(IsTrue(greatNormalV) && pContactToActivate == NULL && commonData.m_bPlayerVehicle)
// 					{
// 						ScalarV susLengthForBottomOutV = IsMaterialIdTrainTrack(impacts.GetOtherMaterialId()) ? susLengthAllowTyreClip : susLengthV;
// 						const BoolV bottomedOutV = IsGreaterThanOrEqual(Add(sCompression, sWheelRadiusIncBurst), susLengthForBottomOutV);
// 						const BoolV normalSideLimitFulfilledV = IsLessThan(fSideNormal, sGetWheelImpactNormalSideLimitActivateV());
// 						const bool suspensionStraight = IsTrue(IsGreaterThanOrEqual(Dot(vSuspensionAxis, Vec3V(V_Z_AXIS_WONE)), ScalarV(0.99f)));// make sure the suspension is vaguely straight.
// 						Assert(!(IsTrue(And(bottomedOutV, normalSideLimitFulfilledV)) && suspensionStraight));
// 
// 					}

					bIgnore = true;
				}
			}

			if(!bIgnore)
			{
				// Get value here to avoid LHS
				const phMaterialMgr::Id unpackedId = PGTAMATERIALMGR->UnpackMtlId(impacts.GetOtherMaterialId());

				dynamicFlags |= WF_HIT;

				if(bContactShouldBeIgnoredDueToCompressionDelta)
				{
					bBestContactShouldBeIgnoredDueToCompressionDelta = true;
				}
				else if(bBestContactShouldBeIgnoredDueToCompressionDelta)
				{
					bBestContactShouldBeIgnoredDueToCompressionDelta = false;
					//deactivate a previous best contact
					if(pContactToActivate)
					{
						pContactToActivate->ActivateContact(false);
					}
				}

				m_vecHitPos = VEC3V_TO_VECTOR3(pNextContact->m_OtherPosition);

				StoreScalar32FromScalarV(m_fCompression,sCompression);

	
				if( !m_bDisableWheelForces )
				{
					// Runtime dev behavior... re-enable if desired to tweak
					//if(sbForceVerticalWheelImpactNormal) 
					//    m_vecHitNormal.Set(ZAXIS);
					//else
						m_vecHitNormal = RCC_VECTOR3(vNormal);

					m_nHitMaterialId = (u8)unpackedId;

					m_nHitComponent = static_cast<u16>(pNextContact->m_OtherComponent);
				}


				if(IsMaterialIdPavement(impacts.GetOtherMaterialId()) &&
					!m_bDisableWheelForces )
				{
					dynamicFlags |= WF_TOUCHING_PAVEMENT;
				}
				else
				{
					dynamicFlags &= ~WF_TOUCHING_PAVEMENT;
				}

				if( IsMaterialIdStairs(impacts.GetOtherMaterialId()) &&
					!m_bDisableWheelForces )
				{
					if(m_pParentVehicle->GetVehicleAudioEntity())
					{
						m_pParentVehicle->GetVehicleAudioEntity()->SetOnStairs();
					}
				}

				// store the hit position as if it was on the centre line of the suspension
				Vec3V vHitCentrePos = AddScaled(vWheelLineB, vSuspensionAxis, sCompression);
				vHitCentrePos = Transform(mtxVeh,vHitCentrePos);
				m_vecHitCentrePos.Set(RCC_VECTOR3(vHitCentrePos));

				if( m_bDisableWheelForces )
				{
					impacts.DisableImpact();
					continue;
				}

#if __DEV
				bool bAddedcontact = false;
#endif
				if(BANK_ONLY(ACTIVATE_BEST_CONTACT &&) ( m_fTyreHealth >= TYRE_HEALTH_DEFAULT || commonData.m_bPlayerVehicle ) && bParentNotWrecked && nOtherInstType != PH_INST_FRAG_VEH && nOtherInstType != PH_INST_FRAG_PED && !bDontActivateImpacts && (commonData.m_pDriver || ( configFlags & (WCF_BIKE_WHEEL | WCF_QUAD_WHEEL) ) || ( m_pParentVehicle->GetModelIndex() == MI_PLANE_STARLING )))// don't activate impacts on wrecked vehicles.
				{
					ScalarV susLengthForBottomOutV = IsMaterialIdTrainTrack(impacts.GetOtherMaterialId()) ? susLengthAllowTyreClip : susLengthV;
					TUNE_GROUP_BOOL(TRAILER_HACKS, ENABLE_SUSPENSION_BOTTOM_OUT, false);
					bool enableBottomOut = !( MI_TRAILER_TRAILERSMALL2.IsValid() && m_pParentVehicle->GetModelIndex() == MI_TRAILER_TRAILERSMALL2 ) || ENABLE_SUSPENSION_BOTTOM_OUT;
					const BoolV bottomedOutV = And(IsGreaterThanOrEqual(Add(sCompression, sWheelRadiusIncBurst), susLengthForBottomOutV), BoolV( enableBottomOut ));
					const BoolV normalSideLimitFulfilledV = IsLessThan(fSideNormal, sGetWheelImpactNormalSideLimitActivateV());
					const BoolV normalSideLimit = And( BoolV( ( m_pParentVehicle->pHandling->mFlags & MF_IS_RC ) != 0 ), IsGreaterThan( fSideNormal, sGetWheelImpactNormalSideLimitCarVsCarV() ) );
					if( IsTrue( Or( And(bottomedOutV, normalSideLimitFulfilledV), normalSideLimit ) ) && IsTrue(IsGreaterThanOrEqual(Dot(vSuspensionAxis, Vec3V(V_Z_AXIS_WONE)), ScalarV(0.99f))))// make sure the suspension is vaguely straight.
					{
#if __DEV
						bAddedcontact = true;
#endif
						//deactivate a previous best contact
						if(pContactToActivate)
						{
							 pContactToActivate->ActivateContact(false);
						}

#if __BANK
						TUNE_GROUP_BOOL(TRAILER_HACKS, RENDER_BOTTOM_OUT, false);
						if(RENDER_BOTTOM_OUT)
						{
							char zText[100];
							sprintf(zText, "Bottomed out!");
							grcDebugDraw::Text(m_pParentVehicle->GetTransform().GetPosition() + Vec3V(0.0f, 0.0f, 2.0f), Color_white, zText);
						}
#endif

						//only activate impacts if we've bottomed out.
						if( !m_bDisableBottomingOut )
						{
							bDisableImpact = false;
						}

						pContactToActivate = &impacts.GetContact();

						Vector3 vSusAxisWorld = m_vecSuspensionAxis;
						MAT34V_TO_MATRIX34(mtxVeh).Transform3x3(vSusAxisWorld);

						ScalarV sDepth = impacts.GetDepthV();
						// Depth is not 1:1 scaled with our bottomed out distance it seems
						const float fAllowedTravel = susLengthForBottomOutV.Getf() - GetWheelRadiusIncBurst();

						Vector3 vWheelMaxPos = m_aWheelLines.B;
						MAT34V_TO_MATRIX34(mtxVeh).Transform(vWheelMaxPos);

						Vector3 vCompressionDistance = m_vecHitPos - vWheelMaxPos;
						float fDistanceAlongsuspension = vSusAxisWorld.Dot(vCompressionDistance);


						Vector3 vecHitNormal = m_vecHitNormal;
						Vector3 VehFwd = VEC3V_TO_VECTOR3(mtxVeh.b());
						Vector3 VehUp = VEC3V_TO_VECTOR3(mtxVeh.c());
						Vector3 NewVec, NewVec2, NewVec3;

						// Get the hit normal in the plane of the fwd vector of the vehicle
						NewVec.Cross(VehFwd, VehUp);
						NewVec2.Cross(vecHitNormal, NewVec);
						NewVec3.Cross(NewVec, NewVec2);
						NewVec3.NormalizeSafe(ZAXIS);

						vecHitNormal = NewVec3;

						const Vec3V velocityV = commonData.m_Velocity;

						//We want to use the normal that will give us the smoothest collision reaction.
						const ScalarV dotSuspensionV = Dot(velocityV, VECTOR3_TO_VEC3V(vSusAxisWorld));
						const ScalarV dotNewV = Dot(velocityV, VECTOR3_TO_VEC3V(vecHitNormal));
						const ScalarV dotOldV = Dot(velocityV, VECTOR3_TO_VEC3V(m_vecHitNormal));

						if( IsGreaterThanAll(dotOldV, dotNewV) && IsGreaterThanAll(dotOldV, dotSuspensionV))
						{
							Vector3 vSusAxisWorldNormal = vSusAxisWorld;
							vSusAxisWorldNormal.Normalize();

							float fSuspensionContribution = vSusAxisWorldNormal.Dot(m_vecHitNormal);
							float fAllowedDistSusp = (fDistanceAlongsuspension - fAllowedTravel) * fSuspensionContribution;
							sDepth.Setf((sDepth.Getf() * (1.0f - fSuspensionContribution)) + fAllowedDistSusp);

#if __DEV	
							if(DEBUG_DRAW_WHEEL_IMPACTS)
							{
								Vec3V vMyPos = impacts.GetMyPosition();
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(VECTOR3_TO_VEC3V(vSusAxisWorld), impacts.GetDepthV()), Color_blue, 2000);
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(VECTOR3_TO_VEC3V(vecHitNormal), impacts.GetDepthV()), Color_red, 2000);
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(velocityV, impacts.GetDepthV()), Color_yellow, 2000);
							}
#endif
						}
						else if( IsGreaterThanAll(dotSuspensionV, dotNewV) )// pick the normal that points with the velocity vector as much as possible, this is so we get smoother impacts
						{
							sDepth.Setf(fDistanceAlongsuspension - fAllowedTravel);
							impacts.SetMyNormal(vSusAxisWorld);
#if __DEV	
							if(DEBUG_DRAW_WHEEL_IMPACTS)
							{
								Vec3V vMyPos = impacts.GetMyPosition();
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(VECTOR3_TO_VEC3V(vSusAxisWorld), impacts.GetDepthV()), Color_orange, 2000);
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(VECTOR3_TO_VEC3V(vecHitNormal), impacts.GetDepthV()), Color_pink, 2000);
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(velocityV, impacts.GetDepthV()), Color_green, 2000);
							}
#endif
						}
						else
						{
							float fNewNormalDepth = vecHitNormal.Dot(m_vecHitNormal);
							sDepth.Setf(sDepth.Getf() * fNewNormalDepth);

							Vector3 vSusAxisWorldNormal = vSusAxisWorld;
							vSusAxisWorldNormal.Normalize();

							// Try and work out how much the wheel is sweeping along the hit normal, before we set the hit normal to be along the suspension sweep but this only worked when landing straight down.
							float fSuspensionContribution = vSusAxisWorldNormal.Dot(vecHitNormal);
							float fAllowedDistSusp = (fDistanceAlongsuspension - fAllowedTravel) * fSuspensionContribution;
							sDepth.Setf((sDepth.Getf() * (1.0f - fSuspensionContribution)) + fAllowedDistSusp);

							impacts.SetMyNormal(vecHitNormal);
#if __DEV	
							if(DEBUG_DRAW_WHEEL_IMPACTS)
							{
								Vec3V vMyPos = impacts.GetMyPosition();
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(VECTOR3_TO_VEC3V(vSusAxisWorld), impacts.GetDepthV()), Color_orange, 2000);
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(VECTOR3_TO_VEC3V(vecHitNormal), impacts.GetDepthV()), Color_black, 2000);
								CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(velocityV, impacts.GetDepthV()), Color_LimeGreen, 2000);
							}
#endif
						}

						impacts.SetDepth(sDepth);

						if(m_fTyreHealth >= TYRE_HEALTH_DEFAULT)
						{
							impacts.SetFriction(0.0f);
							impacts.SetElasticity(0.0f);
						}
						
#if __DEV
						if(DEBUG_DRAW_WHEEL_IMPACTS)
						{
							DrawWheelImpact(*m_pParentVehicle,impacts,Color_brown,Color_black,Color_purple);
						}
#endif
					}
				}

#if __DEV
				if(DEBUG_DRAW_WHEEL_IMPACTS && !bAddedcontact)
				{
					DrawWheelImpact(*m_pParentVehicle,impacts,Color_AntiqueWhite,Color_DarkOrange,Color_LimeGreen);
				}
#endif

                if(pOtherEntity && pOtherEntity->GetIsPhysical() && pOtherEntity != m_pParentVehicle)
                {
                    // set intersection pointer and regref it
                    if(m_pHitPhysical != pOtherEntity)
                    {
						m_pHitPhysical = static_cast<CPhysical*>(pOtherEntity);
						m_vecHitPhysicalOffset.Set(m_vecHitPos);
						m_pParentVehicle->m_nVehicleFlags.bMayHaveWheelGroundForcesToApply = true;
                    }
                }
                else if(m_pHitPhysical)
                {
                    m_pHitPhysical = NULL;
                }
			}
			else
			{
				if(DEV_ONLY(!gbAlwaysRemoveWheelImpacts &&) bUseBikeNormalLimit && bSideImpact)
				{
					impacts.SetElasticity(0.0f);
					bDisableImpact = false;
				}
			}

			// store a pointer to any ped that is colliding with the wheel
			if(nOtherInstType==PH_INST_FRAG_PED)
			{
				if (pOtherEntity->GetIsTypePed())
				{
					CPed* pHitPed = static_cast<CPed*>(pOtherEntity);
					m_pHitPed = pHitPed;

					// Activate animated corpses without enforcing the contact
					if (pHitPed->IsDead())
					{
						// Script can request that mission peds don't activate their ragdolls when dead and hit by a 
						// vehicle for performance reasons:
						if(pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollOnVehicleCollisionWhenDead)
							&& pHitPed->GetRagdollState()==RAGDOLL_STATE_ANIM)
						{
							bDisableImpact = true;
							pHitPed->AddCollisionRecordBeforeDisablingContact(pHitPed->GetRagdollInst(), m_pParentVehicle, pNextContact);
						}
					}
				}
			}

			if(pOtherVehicle && (m_pParentVehicle->pHandling->mFlags & MF_HAS_TRACKS))
			{
				Vec3V myNormal;
				impacts.GetMyNormal(myNormal);

				const ScalarV fZero(V_ZERO);
				if(IsGreaterThanAll(myNormal.GetZ(),fZero) && m_pParentVehicle->GetMass() > pOtherVehicle->GetMass())
				{
					static float fSinkDepthRate = 0.1f;
					static float fMaxSinkDepth = 0.3f;
					static bool fAllowImpactsOnVehicle = true;

                    m_fExtraSinkDepth = fSinkDepthRate * (m_pParentVehicle->GetMass() / pOtherVehicle->GetMass());
					m_fExtraSinkDepth = Min(m_fExtraSinkDepth, fMaxSinkDepth);
					if(fAllowImpactsOnVehicle)
					{
						bDisableImpact = !fAllowImpactsOnVehicle;

						impacts.SetMyNormal(Vec3V(V_Z_AXIS_WZERO));
						impacts.SetDepth(0.0f);
					}
				}
			}
			
			if(bDisableImpact)
			{
				impacts.DisableImpact();
			}
		}
	}

	if( m_bDisableWheelForces )
	{
		dynamicFlags &= ~WF_HIT;
		dynamicFlags &= ~WF_HIT_PREV;
	}


	if(bDontActivateImpacts && pContactToActivate)
	{
		pContactToActivate->ActivateContact(false);
	}

	// Store the dynamic flags back.
	GetDynamicFlags().SetAllFlags(dynamicFlags);
}

#if __DEV
//
void CWheel::DrawWheelImpact(const CVehicle & rParentVehicle, phCachedContactIterator & impacts, Color32 col1, Color32 col2, Color32 col3) const
{
	Matrix34 matParent = MAT34V_TO_MATRIX34(rParentVehicle.GetVehicleFragInst()->GetMatrix());
	Vector3 vWheelB = m_aWheelLines.B;
	matParent.Transform(vWheelB);

	const int iExpiryTime = (fwTimer::GetTimeStepInMilliseconds() * 2000);
	CPhysics::ms_debugDrawStore.AddSphere(impacts.GetOtherPosition(), 0.02f, col1, iExpiryTime);
	Vec3V vecTempMyNormal;
	impacts.GetMyNormal(vecTempMyNormal);
	//Vec3V vOtherPos = impacts.GetOtherPosition();
	Vec3V vMyPos = impacts.GetMyPosition();
	CPhysics::ms_debugDrawStore.AddLine(vMyPos, vMyPos + Scale(vecTempMyNormal, impacts.GetDepthV()), col2, iExpiryTime);
	CPhysics::ms_debugDrawStore.AddSphere(vMyPos, 0.02f, col2, iExpiryTime);
	CPhysics::ms_debugDrawStore.AddSphere(VECTOR3_TO_VEC3V(vWheelB), 0.02f, col3, iExpiryTime);
}
#endif

bool CWheel::ProcessImpactPopTires(phCachedContactIterator& impacts, const CEntity & rOtherEntity)
{
	//Ensure the entity is an object.
	//Note: if you change this, the call to this function from CWheel::ProcessImpact() will
	//have to change too, since we are taking advantage of this to avoid a function call.
	if(!rOtherEntity.GetIsTypeObject())
	{
		return false;
	}
	
	//Ensure the object pops tires.
	const CObject* pObject = static_cast<const CObject *>(&rOtherEntity);
	phMaterialMgr::Id otherMaterialId = PGTAMATERIALMGR->UnpackMtlId( impacts.GetOtherMaterialId() );

	bool tyreBurstProp = otherMaterialId == PGTAMATERIALMGR->g_idPhysVehicleTyreBurst;

	if( tyreBurstProp )
	{
		// check the direction we're going as we only want to burst tyres one way
		if( Dot( rOtherEntity.GetTransform().GetForward(), VECTOR3_TO_VEC3V( m_pParentVehicle->GetVelocity() ) ).Getf() > 0.0f )
		{
			tyreBurstProp = false;
		}
	}

	if( !pObject->m_nObjectFlags.bPopTires &&
		!tyreBurstProp )
	{
		return false;
	}
	
	//At this point, we know we drove over an object that pops tires.
	//From now on, we want to disable impacts.  We may want this to
	//be an object flag in the future, if we need collisions between
	//tire popping objects and wheels.  However, we do not want this
	//for the spike strip, so we are disabling the impact every time.
	
	//Check if the tire is not flat.
	if(!GetIsFlat())
	{
		//Check if we should pop the tire.
		//For now, the criteria for this is:
		//	1) No tires popped, 100% chance.
		//	2) At least one tire popped, 30% chance.
		bool bPopTire = (m_pParentVehicle->GetNumFlatTires() == 0);
		if(!bPopTire)
		{
			//Check if we haven't checked whether to pop this tire in a while.
			u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
			u32 uTimeSinceLastPopTireCheck = uCurrentTime - m_uLastPopTireCheck;
			static u32 s_uTimeBetweenPopTireChecks = (u32)((3.0f) * 1000);
			if(uTimeSinceLastPopTireCheck > s_uTimeBetweenPopTireChecks)
			{
				//Set the time.
				m_uLastPopTireCheck = uCurrentTime;

				//Check if we should pop the tire.
				const float s_fChancesToPopTire = m_pParentVehicle->IsLawEnforcementVehicle() ? 0.15f : 0.3f;
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= s_fChancesToPopTire)
				{
					bPopTire = true;
				}
			}
		}
		if(bPopTire)
		{
			//Generate the local position and normal.
			Vector3 vMyPosition = VEC3V_TO_VECTOR3(impacts.GetMyPosition());
			Vector3 vMyNormal;
			impacts.GetMyNormal(vMyNormal);
			Vector3 vMyPositionLocal = VEC3V_TO_VECTOR3(m_pParentVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vMyPosition)));
			Vector3 vMyNormalLocal = VEC3V_TO_VECTOR3(m_pParentVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vMyNormal)));

			//Generate the damage to force the tire to become flat.
			float fDamage = m_fTyreHealth - TYRE_HEALTH_FLAT;

			//Generate the damage type and wheel index.
			//These are only used if the damage type is bullet for stats purposes, so they don't matter.
			eDamageType nDamageType = DAMAGE_TYPE_COLLISION;
			int iWheelIndex = 0;

			//Pop the tire.
			ApplyTyreDamage(&rOtherEntity, fDamage, vMyPositionLocal, vMyNormalLocal, nDamageType, iWheelIndex);
		}
	}
	
	return true;
}

#if __DEV
void CWheel::DebugDrawSingleImpact(phCachedContactConstIterator& impact)
{
	Vec3V vImpactPos = impact.GetOtherPosition();
	static dev_bool bDrawDepth = false;
	if(bDrawDepth)
	{
		static dev_s32 iNumFramesToDrawDepth = 1;
		Vec3V vStart = vImpactPos;
		Vec3V vRenderNormal;
		impact.GetMyNormal(vRenderNormal);
		Vec3V vEnd = vStart - Scale(vRenderNormal, impact.GetDepthV());
		u32 iExpiryTime = iNumFramesToDrawDepth * fwTimer::GetTimeStepInMilliseconds();
		CPhysics::ms_debugDrawStore.AddLine(vStart,vEnd,Color_green,iExpiryTime);
	}

	static dev_bool bRenderNormal = false;
	if(bRenderNormal)
	{
		static dev_float fDebugDrawScaleNormal = 0.2f;
		static dev_s32 iNumFramesToDrawNormal = 10;
		Vec3V vStart = vImpactPos;
		Vec3V vRenderNormal;
		impact.GetMyNormal(vRenderNormal);
		Vec3V vEnd = vStart + Scale(vRenderNormal, ScalarVFromF32(fDebugDrawScaleNormal));
		u32 iExpiryTime = iNumFramesToDrawNormal * fwTimer::GetTimeStepInMilliseconds();
		CPhysics::ms_debugDrawStore.AddLine(vStart,vEnd,Color_red,iExpiryTime);
	}

	static dev_bool bDebugDrawRelVel = false;
	if(bDebugDrawRelVel)
	{
		Vec3V vRelVelocity = impact.GetRelVelocity();
		static dev_float fDebugDrawScale = 1.0f;
		static dev_s32 iNumFramesToDraw = 10;
		Vec3V vStart = vImpactPos;
		Vec3V vEnd = vStart + Scale(vRelVelocity, ScalarVFromF32(fDebugDrawScale));
		u32 iExpiryTime = iNumFramesToDraw * fwTimer::GetTimeStepInMilliseconds();
		CPhysics::ms_debugDrawStore.AddLine(vStart,vEnd,Color_orange,iExpiryTime);
	}	

	static dev_bool bDebugDrawTargetRelVel = false;
	if(bDebugDrawTargetRelVel)
	{		
		Vec3V vTargetRelVelocity = impact.GetContact().GetTargetRelVelocity();
		static dev_float fDebugDrawScale = 1.0f;
		static dev_s32 iNumFramesToDraw = 10;
		Vec3V vStart = vImpactPos;
		Vec3V vEnd = vStart + Scale(vTargetRelVelocity, ScalarVFromF32(fDebugDrawScale));
		u32 iExpiryTime = iNumFramesToDraw * fwTimer::GetTimeStepInMilliseconds();
		CPhysics::ms_debugDrawStore.AddLine(vStart,vEnd,Color_white,iExpiryTime);
	}
}
#endif

RAGETRACE_DECL(CWheel_ProcessPreSimUpdate);

//
//
void CWheel::ProcessPreSimUpdate( const void *basePtr, phCollider* vehicleCollider, Mat34V_In vehicleMatrix, float fWheelExtension )
{
	RAGETRACE_SCOPE(CWheel_ProcessPreSimUpdate);	

	Assert(!m_pParentVehicle->IsDummy());
	Assert(m_pParentVehicle->GetSkeleton());
	
	int nBoneIndex = m_pParentVehicle->GetBoneIndex(m_WheelId);
	if(nBoneIndex < 0)
		return;

	m_fCompressionOld = m_fCompression;
	m_fCompression = rage::Max(0.0f,m_fCompression-fWheelExtension);

	if(!m_pParentVehicle->m_nVehicleFlags.bMayHaveWheelGroundForcesToApply) // We clear these in CWheel::IntegrationFinalProcess
	{
		m_pHitPed = NULL;
		CPhysical *pHitPhysical = m_pHitPhysical;
		m_pPrevHitPhysical = pHitPhysical;
		m_pHitPhysical = NULL;
	}
	bool bBoundsChanged = false;

	if(GetConfigFlags().IsFlagSet(WCF_UPDATE_SUSPENSION))
	{
		UpdateSuspension(basePtr);

		if( m_fSuspensionTargetRaiseAmount == m_fSuspensionRaiseAmount )
		{
			GetConfigFlags().ClearFlag(WCF_UPDATE_SUSPENSION);
		}
		bBoundsChanged = true;
	}

	// Compute the maximum wheel sweep matrices
	Mat34V currentWheelMatrix[MAX_WHEEL_BOUNDS_PER_WHEEL];
	Mat34V lastWheelMatrix;
	CalculateMaximumExtents(lastWheelMatrix,currentWheelMatrix[0],vehicleCollider);

	// Update the matrices in the composite
	fragInstGta * pVehicleFragInst = m_pParentVehicle->GetVehicleFragInst();
	phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pVehicleFragInst->GetArchetype()->GetBound());
	const int nFragChild = GetFragChild();
	Assert(nFragChild > -1);

	pBoundComposite->SetLastMatrix(nFragChild, lastWheelMatrix);
	pBoundComposite->SetCurrentMatrix(nFragChild, currentWheelMatrix[0]);

	float fOriginalBoundRadius = pBoundComposite->GetRadiusAroundCentroid();

	for(int i = 1; i < MAX_WHEEL_BOUNDS_PER_WHEEL; i++)
	{
		if(GetFragChild(i) >= 0)
		{ 
			currentWheelMatrix[i] = currentWheelMatrix[0];

			if(GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL))
			{	
				if (!pVehicleFragInst->GetCacheEntry()->HasClonedBounds())
				{
					bBoundsChanged = m_pParentVehicle->CloneBounds();
				}

				Assert(m_pParentVehicle->InheritsFromBike());
				CBike *pBike = static_cast<CBike*>(m_pParentVehicle);

				if(m_pParentVehicle->GetDriver() || pBike->m_nBikeFlags.bGettingPickedUp)
				{	
					currentWheelMatrix[i].Setd(VECTOR3_TO_VEC3V(m_aWheelLines.A));

					Vec3V vOffsetDelta = Scale(currentWheelMatrix[i].GetCol1(), ScalarVFromF32(GetWheelRadius()*0.5f*GetFrontRearSelector()));

					currentWheelMatrix[i].Setd(currentWheelMatrix[i].d()+vOffsetDelta);

					// make sure our extra wheel bound is a certain height above the ground so curbs don't cause as much of a problem.
					static dev_float sfPlayerBikeWheelRadiusMinimumOffsetMult = 0.75f;
					float fBikeWheelRadiusMinimumOffsetMult = 1.0f;
					
					if(m_pParentVehicle->GetDriver() && m_pParentVehicle->GetDriver()->IsLocalPlayer())// For the local player we activate impacts when the suspension bottoms out so we are less likely to collide this extra wheel bound, so we can have this at a more reasonable height off the ground.
					{
						if( GetConfigFlags().IsFlagSet(WCF_REARWHEEL) &&
							( CPhysics::ms_bInStuntMode ||
							  ( MI_BIKE_GARGOYLE.IsValid() && m_pParentVehicle->GetModelIndex() == MI_BIKE_GARGOYLE ) || 
							  ( MI_BIKE_BF400.IsValid() && m_pParentVehicle->GetModelIndex() == MI_BIKE_BF400 ) ||
							  ( MI_BIKE_SANCTUS.IsValid() && m_pParentVehicle->GetModelIndex() == MI_BIKE_SANCTUS )) )
						{
							static dev_float sfPlayerBikeRearWheelRadiusMinimumOffsetMultInStuntMode = 1.25f;
							fBikeWheelRadiusMinimumOffsetMult = sfPlayerBikeRearWheelRadiusMinimumOffsetMultInStuntMode;
						}
						else
						{
							if( !GetConfigFlags().IsFlagSet(WCF_REARWHEEL) &&
								( m_pParentVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR ||
								  m_pParentVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2 ) )
							{
								//fBikeWheelRadiusMinimumOffsetMult = 0.5f;
							}
							else
							{
								fBikeWheelRadiusMinimumOffsetMult = sfPlayerBikeWheelRadiusMinimumOffsetMult;
							}
						}
					}

					Vec3V vPos = currentWheelMatrix[i].GetCol3();
					float fMinimumOffset = (m_aWheelLines.A.z - m_fStaticDelta + m_fWheelRadius * fBikeWheelRadiusMinimumOffsetMult);
					if(vPos.GetZf() < fMinimumOffset)
					{
						vPos.SetZf(fMinimumOffset);
						currentWheelMatrix[i].SetCol3(vPos);
					}

					pBoundComposite->SetLastMatrix(GetFragChild(i), currentWheelMatrix[i]);
					pBoundComposite->SetCurrentMatrix(GetFragChild(i), currentWheelMatrix[i]);

					phBound *pBound = pBoundComposite->GetBound(GetFragChild(i));
					if(pBound && pBound->GetType() == phBound::DISC)
					{
						phBoundDisc *pBoundDisc = static_cast<phBoundDisc*>(pBound);
						static dev_float sfWheelsWithDriverRadiusMult = 0.5f;
						float fRadius = pBoundDisc->GetRadius();
						if(fRadius > ((GetWheelRadius()*sfWheelsWithDriverRadiusMult)+0.01f) &&
							( GetConfigFlags().IsFlagSet(WCF_REARWHEEL) ||
							  !MI_BIKE_OPPRESSOR.IsValid() || 
							  m_pParentVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR ) )//add a little due to rounding errors.
						{
							pBoundDisc->SetDiscRadius(GetWheelRadius()*sfWheelsWithDriverRadiusMult);
							bBoundsChanged = true;
						}
					}
				}
				else
				{
					Vector3 vWheelPos = m_aWheelLines.A;

					if(GetConfigFlags().IsFlagSet(WCF_BIKE_FALLEN_COLLIDER))
					{
						static dev_float sfStaticDeltaMultA = 0.9f; // Allow a small amount of suspension movement whenfallen before hitting the extra bound.
						vWheelPos.z -= m_fStaticDelta * sfStaticDeltaMultA;
					}

					currentWheelMatrix[i].Setd(VECTOR3_TO_VEC3V(vWheelPos));

					pBoundComposite->SetLastMatrix(GetFragChild(i), currentWheelMatrix[i]);
					pBoundComposite->SetCurrentMatrix(GetFragChild(i), currentWheelMatrix[i]);

					phBound *pBound = pBoundComposite->GetBound(GetFragChild(i));
					if(pBound && pBound->GetType() == phBound::DISC)
					{
						phBoundDisc *pBoundDisc = static_cast<phBoundDisc*>(pBound);
						float fRadius = pBoundDisc->GetRadius();
						if(fRadius+pBoundDisc->GetMargin() < GetWheelRadius())
						{
							pBoundDisc->SetDiscRadius(GetWheelRadius() - (pBoundDisc->GetMargin()));
							bBoundsChanged = true;
						}
					}
				}
			}
			else
			{	
				bool bModifyMargin = m_pParentVehicle->GetDriver() && m_pParentVehicle->GetDriver()->IsLocalPlayer();

				if (!pVehicleFragInst->GetCacheEntry()->HasClonedBounds() && bModifyMargin)
				{
					bBoundsChanged = m_pParentVehicle->CloneBounds();
				}
				phBound *pBound = pBoundComposite->GetBound(GetFragChild(i));

				//Handle the monster truck wheels.
				if(m_pParentVehicle->pHandling && m_pParentVehicle->pHandling->hFlags & HF_EXT_WHEEL_BOUNDS_COL)
				{
					if(i == WHEEL_CLIMB_BOUND)
					{
						Vector3 vWheelPos = m_aWheelLines.B;

						vWheelPos.z += GetWheelRadiusIncBurst() + m_fCompression; // set the wheel to the current compression
						currentWheelMatrix[i].Setd(VECTOR3_TO_VEC3V(vWheelPos));
					}
					else
					{
						Vector3 vWheelPos = m_aWheelLines.A;
						
						if(pBound && pBound->GetType() == phBound::DISC)
						{
							phBoundDisc *pBoundDisc = static_cast<phBoundDisc*>(pBound);
							float fMargin = pBoundDisc->GetMargin();
							float fHalfWheelWidth = GetWheelWidth() * 0.5f;
							if(m_pParentVehicle->GetDriver())
							{
								static dev_float sfWheelWidthMult = 0.8f;
								if(fMargin > fHalfWheelWidth * sfWheelWidthMult && bModifyMargin)
								{
									pBoundDisc->SetMargin(fHalfWheelWidth * sfWheelWidthMult);
									bBoundsChanged = true;
								}
								vWheelPos = m_aWheelLines.B;
								
								if(!GetWasTouching() && m_pParentVehicle->GetTransform().GetC().GetZf() <= 0.0f)
								{
									vWheelPos.z += GetWheelRadiusIncBurst()*1.4f + m_fCompression;// lower the wheel when the tyre isn't touching
								}
								else
								{
									float fSpeed = m_pParentVehicle->GetVelocity().Mag2();
									fSpeed = rage::Clamp(fSpeed, 1.7f, 2.2f);
									
									vWheelPos.z += GetWheelRadiusIncBurst()*fSpeed + m_fCompression;// Move wheel to the top of it's travel
								} 

								static dev_float sfWheelExtensionAmount = 0.20f;
								const Vec3V vOffsetDelta = Scale(currentWheelMatrix[i].GetCol1(), ScalarVFromF32(GetWheelRadius()*sfWheelExtensionAmount*GetFrontRearSelector()));

								vWheelPos += VEC3V_TO_VECTOR3(vOffsetDelta);
							}
						}
						currentWheelMatrix[i].Setd(VECTOR3_TO_VEC3V(vWheelPos));

					}
				}
				else
				{	
					Vector3 vWheelPos = m_aWheelLines.A;

					if(pBound && pBound->GetType() == phBound::DISC)
					{
						phBoundDisc *pBoundDisc = static_cast<phBoundDisc*>(pBound);
						float fMargin = pBoundDisc->GetMargin();
						float fHalfWheelWidth = GetWheelWidth() * 0.5f;
						if(m_pParentVehicle->GetDriver())
						{
							static dev_float sfWheelWidthMult = 1.1f;
							static dev_float sfWheelWidthShrinkMult = 0.1f;
							float fWheelWidthMult = sfWheelWidthMult;
							if(m_pParentVehicle->pHandling && m_pParentVehicle->pHandling->hFlags & HF_ALT_EXT_WHEEL_BOUNDS_SHRINK)
							{
								fWheelWidthMult = sfWheelWidthShrinkMult;

								if(fMargin > fHalfWheelWidth * fWheelWidthMult && bModifyMargin)
								{
									pBoundDisc->SetMargin(fHalfWheelWidth * fWheelWidthMult);
									bBoundsChanged = true;
								}
								vWheelPos.z += m_fStaticDelta + (fHalfWheelWidth);
							}
							else 
							{
								if(fMargin < fHalfWheelWidth * fWheelWidthMult && bModifyMargin)
								{
									pBoundDisc->SetMargin(fHalfWheelWidth * fWheelWidthMult);
									bBoundsChanged = true;
								}

								vWheelPos.z += m_fStaticDelta + (GetWheelRadius() - GetWheelRadiusIncBurst());// we want the wheel to be right at the top of its travel.
							}
						}
						else
						{
							if(fMargin > fHalfWheelWidth && bModifyMargin)
							{
								pBoundDisc->SetMargin(fHalfWheelWidth);
								bBoundsChanged = true;
							}
							static dev_float sfStaticDeltaMult = 0.25f;
							vWheelPos.z += m_fStaticDelta*sfStaticDeltaMult;
						}
					}

					currentWheelMatrix[i].Setd(VECTOR3_TO_VEC3V(vWheelPos));

					if(m_pParentVehicle->pHandling && m_pParentVehicle->pHandling->hFlags & HF_ALT_EXT_WHEEL_BOUNDS_BEH)
					{
						static dev_float sfWheelExtensionAmount = 0.4f;
						static dev_float sfWheelShrinkExtensionAmount = 0.6f;
						Vec3V vOffsetDelta;
						if(m_pParentVehicle->pHandling && m_pParentVehicle->pHandling->hFlags & HF_ALT_EXT_WHEEL_BOUNDS_SHRINK)
						{
							vOffsetDelta = Scale(currentWheelMatrix[i].GetCol1(), ScalarVFromF32(GetWheelRadius()*sfWheelShrinkExtensionAmount*GetFrontRearSelector()));
						}
						else
						{
							vOffsetDelta = Scale(currentWheelMatrix[i].GetCol1(), ScalarVFromF32(GetWheelRadius()*sfWheelExtensionAmount*GetFrontRearSelector()));
						}

						currentWheelMatrix[i].Setd(currentWheelMatrix[i].d() + vOffsetDelta);
					}
				}

				pBoundComposite->SetCurrentMatrix(GetFragChild(i), currentWheelMatrix[i]);
				pBoundComposite->SetLastMatrix(GetFragChild(i), currentWheelMatrix[i]);

			}
		}
	}

	if(bBoundsChanged)
	{
		// recalculate bounding box and sphere for the composite bound
		pBoundComposite->CalculateCompositeExtents();
		if(!m_pParentVehicle->IsColliderArticulated())
		{
			PHLEVEL->UpdateCompositeBvh(m_pParentVehicle->GetFragInst()->GetLevelIndex());
		}

		// we have probably changed the bound radius of the fragInst so need to tell the level that's changed if it has.
		if(pBoundComposite->GetRadiusAroundCentroid() != fOriginalBoundRadius)
			CPhysics::GetLevel()->UpdateObjectLocationAndRadius(m_pParentVehicle->GetFragInst()->GetLevelIndex(),(Mat34V_Ptr)(NULL));
	}


	// Update the matrices in the articulated collider
	if(vehicleCollider && vehicleCollider->IsArticulated())
	{
		phArticulatedCollider* articulatedCollider = static_cast<phArticulatedCollider*>(vehicleCollider);

		// Make sure that this vehicle owns its link attachment matrices, we don't want to set matrices on the fragType
		Assert(pVehicleFragInst->GetCacheEntry());
		Assert(pVehicleFragInst->GetCacheEntry()->GetHierInst()->linkAttachment);
		Assert(articulatedCollider->GetLinkAttachmentMatrices());
		Assert(pVehicleFragInst->GetCacheEntry()->GetHierInst()->linkAttachment->GetElements() == articulatedCollider->GetLinkAttachmentMatrices());

		phArticulatedBody* body = articulatedCollider->GetBody();
		for(int i = 0; i < MAX_WHEEL_BOUNDS_PER_WHEEL; i++)
		{
			int nWheelFragChild = GetFragChild(i);
			if(nWheelFragChild >= 0)
			{ 
				int bodyPartIndex = articulatedCollider->GetLinkFromComponent(nWheelFragChild);
				Assert(bodyPartIndex>=0);
				Mat34V bodyPartMatrix(body->GetLink(bodyPartIndex).GetMatrix());
				Transpose3x3(bodyPartMatrix,bodyPartMatrix);
				Mat34V linkAttachmentMtx(currentWheelMatrix[i]);
				Transform(linkAttachmentMtx,vehicleMatrix,linkAttachmentMtx);
				linkAttachmentMtx.Setd(linkAttachmentMtx.d()-articulatedCollider->GetPosition());
				UnTransformOrtho(linkAttachmentMtx,bodyPartMatrix,linkAttachmentMtx);
				articulatedCollider->SetLinkAttachmentMatrix(nWheelFragChild,linkAttachmentMtx);
			}
		}
	}

	// Update the broken flag each frame since we don't know exactly when parts break off
	if(pBoundComposite->GetBound(nFragChild) == NULL)
	{
		GetDynamicFlags().SetFlag(WF_BROKEN_OFF);
	}

	m_fExtraSinkDepth = 0.0f;
}


void CWheel::UpdateCompressionOnDeactivation()
{
	if(m_pParentVehicle->m_nVehicleFlags.bWheelsDisabledOnDeactivation)
	{
		m_fWheelCompression = m_fCompression =  m_fCompressionOld;
	}
}

//
void CWheel::ProcessAsleep( const Vector3& parentVelocity, const float fTimeStep, const float fGravityForce, bool updateWheelAngVelFromVehicleVel)
{
	m_fEffectiveSlipAngle = 0.0f;
    m_fTyreLoad = 0.0f;
	m_fTyreCamberForce = 0.0f;
	m_fFwdSlipAngle = 0.0f;
	m_fSideSlipAngle = 0.0f;
	m_fAbsTimer = 0.0f;
	m_fRotSlipRatio = 0.0f;
	if(!m_pParentVehicle->m_nVehicleFlags.bWheelsDisabled && !m_pParentVehicle->m_nVehicleFlags.bWheelsDisabledOnDeactivation)
	{
		m_fCompressionOld = m_fCompression;
	}
	
	// Update the angular velocity of the wheel
	if(updateWheelAngVelFromVehicleVel && m_fWheelRadius > 0.0f)
	{
		Vector3 vForward = VEC3V_TO_VECTOR3(m_pParentVehicle->GetMatrix().GetCol1());
		float fCurrentFwdSpeed = parentVelocity.Dot(vForward);
		float newRotAngVel = -fCurrentFwdSpeed / m_fWheelRadius;
		m_fRotAngVel = newRotAngVel;

		// rotate wheel
		if(m_bUpdateWheelRotation)
		{
			m_fRotAng += newRotAngVel * fTimeStep;
			m_fRotAng = fwAngle::LimitRadianAngleFast(m_fRotAng);
		}
	}
	// B*1811074: Keep wheel rotation velocity and angle in sync with animated wheels.
	else if(m_pParentVehicle->m_nVehicleFlags.bAnimateWheels)
	{		
		Matrix34 wheelMat = m_pParentVehicle->GetLocalMtx(m_pParentVehicle->GetBoneIndex(m_WheelId));		
		float prevRotAng = m_fRotAng;
		m_fRotAng = wheelMat.GetEulers().x;
		m_fRotAngVel = (m_fRotAng - prevRotAng) / fTimeStep;
		m_fRotAng = fwAngle::LimitRadianAngleFast(m_fRotAng);			
	}
	else
	{
		// Don't apply braking forces to wheels that aren't moving
		if(m_fRotAngVel != 0.0f)
		{
			float fRotAngVelOld = m_fRotAngVel;
			float fBrakeForce = m_fBrakeForce * fGravityForce;

			float fInvInertia = sfInvInertiaForIntegratorBrake;
			if(GetDynamicFlags().IsFlagSet(WF_HIT_PREV))
			{
				fBrakeForce += m_pHandling->m_fTractionCurveMax * m_fStaticForce * fGravityForce;
			}
			else
			{
				// only reset these if the wheel is off the ground
				m_vecHitCentrePosOld.Zero();
				// apply lower std friction if wheel is in the air
				float fInAirFriction = GetConfigFlags().IsFlagSet(WCF_POWERED) ? sfFrictionStdInAirDriven : sfFrictionStdInAir;
				float fDamagedFriction = GetFrictionDamageScaledBySpeed(parentVelocity) * fGravityForce;
				fBrakeForce += rage::Max(fInAirFriction, fDamagedFriction);
				fInvInertia = sfInvInertiaForIntegratorBrakeInAir;
			}

			float deltaRotAngVelocity = fBrakeForce * fInvInertia * fTimeStep;
			float newRotAngVel;
			if(deltaRotAngVelocity > rage::Abs(fRotAngVelOld))
				newRotAngVel = 0.0f;
			else if(fRotAngVelOld > 0.0f)
				newRotAngVel = fRotAngVelOld - deltaRotAngVelocity;
			else
				newRotAngVel = fRotAngVelOld + deltaRotAngVelocity;


			m_fRotAngVel = newRotAngVel;

			// rotate wheel
			if(m_bUpdateWheelRotation)
			{
				m_fRotAng += newRotAngVel * fTimeStep;
				m_fRotAng = fwAngle::LimitRadianAngleFast(m_fRotAng);
			}
		}
	}

	// tyres cool if vehicle is asleep
	ProcessTyreTemp(parentVelocity, fTimeStep);
}

void CWheel::ProcessLowLODAudio()
{
	m_fEffectiveSlipAngle = 0.0f;
	m_fTyreLoad = 0.0f;
	m_fTyreCamberForce = 0.0f;
	m_fFwdSlipAngle = 0.0f;
	m_fSideSlipAngle = 0.0f;
}
#endif // __SPU

float CWheel::GetCalculatedWheelCompression()
{
	float fBurstCompression = GetTyreHealth() < TYRE_HEALTH_DEFAULT ? GetTyreBurstCompression() : 0.0f;

	m_fWheelImpactOffset = m_fWheelImpactOffsetOriginal * m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV;// Reduce the depth modification 

	fBurstCompression += m_fExtraSinkDepth;

	return rage::Min(m_fCompression, m_fMaxTravelDelta + fBurstCompression - m_fWheelImpactOffset);
}

void CWheel::UpdateDummyTransitionCompression(float timeStep)
{
	if(GetHasDummyTransition())
	{
		// Don't do anything the first frame since PreComputeImpacts hasn't had a chance to run and we are still using dummy compression
		if(!GetDynamicFlags().IsFlagSet(WF_DUMMY_TRANSITION_PREV))
		{
			GetDynamicFlags().SetFlag(WF_DUMMY_TRANSITION_PREV);
		}
		else
		{
			bool endDummyTransition = true;
			// If there is no ground under us, stop the transition or we will be driving on nothing 
			if(GetIsTouching())
			{
				const float groundCompression = GetCompression();
				const float desiredCompressionChange = groundCompression - m_fTransitionCompression;
				const float previousDesiredCompressionChange = groundCompression - m_fPreviousTransitionCompression;
				// Stop the transition if we're about to change directions. This prevents us from getting stuck in here if the compression
				//   is fluctuating wildly. In that case though we don't really care about a smooth transition from dummy->real. 
				if(SameSign(desiredCompressionChange,previousDesiredCompressionChange))
				{
					// If the desired change is less than the maximum we don't need to do anything
					dev_float dummyTransitionSpeed = 0.2f;
					const float maxCompressionChange = dummyTransitionSpeed*timeStep;
					if(Abs(desiredCompressionChange) > maxCompressionChange)
					{
						// Store the current transition compression in the previous and 
						const float compressionChange = Clamp(desiredCompressionChange,-maxCompressionChange,maxCompressionChange);
						m_fPreviousTransitionCompression = m_fTransitionCompression;
						m_fCompression = m_fTransitionCompression = m_fTransitionCompression + compressionChange;
						m_fWheelCompression = GetCalculatedWheelCompression();
						endDummyTransition = false;
					}
				}
			}

			if(endDummyTransition)
			{
				EndDummyTransition();
			}
		}
	}
}

dev_bool g_UseAdditionalWheelSweep = false;
dev_bool g_WheelSweepOut = false;


void CWheel::CalcWheelMatrices(Mat34V_InOut lastWheelMatrixOut, Mat34V_InOut currentWheelMatrixOut, phCollider *pParentCollider)
{
	// Use local variables so we can operate on registers
	Mat34V tempLastMtx, tempCurMtx;
	ScalarV fWheelRadius(m_fWheelRadius);

	// Create fCompressionAsFloat here to avoid LHS when it is converted to ScalarV
	// need to clamp compression because dummy vehicles can get large values
	// and we don't want to actually move the wheels using those
	const float fCompressionAsFloat = rage::Min(m_fSusLength,m_fCompression);

	tempCurMtx.Setc(RCC_VEC3V(m_vecSuspensionAxis));
	tempCurMtx.Seta(RCC_VEC3V(m_vecAxleAxis)); // should be already normalised
	tempCurMtx.Setb(Cross(tempCurMtx.c(), tempCurMtx.a()));
	tempCurMtx.Setb(Normalize(tempCurMtx.b()));


	if(GetConfigFlags().IsFlagSet(WCF_ROTATE_BOUNDS))
	{
        Mat34VRotateLocalZ(tempCurMtx, ScalarVFromF32(m_fSteerAngle));
    }


#if 0 // Development tweaking behavior, restore if desired by default
    //apply positive camber to the tyres so the top of the tyre will hit a vertical wall first and the car wont drive up it
    static dev_float sfPositiveCamber = 0.0f;
    if(sfPositiveCamber != 0.0f && !GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL))
    {
        tempCurMtx.RotateY(sfPositiveCamber*vecAxleAxisWithDirection.x);
    }
#endif

	tempCurMtx.Setd(RCC_VEC3V(m_aWheelLines.A));
    tempLastMtx.Set3x3(tempCurMtx);

#if 0 // Development tweaking behavior, restore if desired
    if(g_UseAdditionalWheelSweep)
    {
        // don't want to sweep from the top of the wheel, but from the centre of the wheel, so don't add on wheel radius
        Vec3V vExtendSweep(0.0f, 0.0f, sfWheelLastPosAddRadius*m_fWheelRadius);

        tempLastMtx.Setd(tempCurMtx.d()+vExtendSweep);
    }
    else
#endif
    {
	    // Project the last matrix of the wheel forward by one frame's motion, so CCD will sweep the wheel downwards instead of diagonally
	    tempLastMtx.Setd(tempCurMtx.d());
    }

#if 0 // Development tweaking behavior, restore if desired
    if(g_WheelSweepOut)
    {
	    // want the wheels to sweep down from a position further towards the inside of the vehicle
	    if(!GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && !GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL))
	    {
		    float fSweepOut = m_fWheelWidth * (GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) ? 1.0f : -1.0f);
		    tempLastMtx.Setd(tempLastMtx.d()+Vec3V(V_X_AXIS_WZERO)*ScalarV(fSweepOut));
	    }
    }
/* Possibly useful for riding up kerbs on bikes
    else
    {//for bikes we want to sweep the wheel out from towards the center of the bike
        float fSweepOut = m_fWheelRadius * (GetConfigFlags().IsFlagSet(WCF_REARWHEEL) ? 1.0f : -1.0f);
        tempLastMtx.d.AddScaled(YAXIS, fSweepOut);
    }
*/
#endif

#if 0 // Development tweaking behavior, restore if desired by default
	static dev_bool bSetLastMatrixProperly = false;
	if(bSetLastMatrixProperly && m_fCompressionOld >= 0.0f)
	{
		tempLastMtx.d = m_aWheelLines.B;
		float fCompression = m_fCompressionOld;
		if(fCompression > m_fSusLength)
			fCompression = m_fSusLength;
		tempLastMtx.d += tempLastMtx.c * (fCompression + m_fWheelRadius);
	}
#endif

	tempCurMtx.Setd(RCC_VEC3V(m_aWheelLines.B));

    //Don't sweep out all the way to the edge of the vehicle so the tyres don't get collisions before the bodywork
    if(!GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && !GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL))
    {
        static dev_float reduceSweepAmount = 0.2f;
        float fSweepOut = m_fWheelWidth * (GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) ? reduceSweepAmount : -reduceSweepAmount);
		tempCurMtx.Setd(tempCurMtx.d()+Vec3V(V_X_AXIS_WZERO)*ScalarV(fSweepOut));
		tempLastMtx.Setd(tempLastMtx.d()+Vec3V(V_X_AXIS_WZERO)*ScalarV(fSweepOut));
    }

	// Convert to ScalarV here to avoid LHS
	ScalarV fCompression(fCompressionAsFloat);
	tempCurMtx.Setd(tempCurMtx.d() + tempCurMtx.c() * (fCompression + fWheelRadius));

#if !__SPU
	// Make sure we sweep from the last matrix, rather then the last safe matrix
	if(pParentCollider && !CPhysics::ms_bInStuntMode)
	{
		if(pParentCollider->GetCurrentlyPenetratingCount() > 0)
		{
			Transform(tempLastMtx, pParentCollider->GetLastInstanceMatrix(), tempLastMtx);
			UnTransformOrtho(tempLastMtx, pParentCollider->GetLastSafeInstanceMatrix(), tempLastMtx);
		}
	}
#endif

	// Copy to the output
	currentWheelMatrixOut = tempCurMtx;
	lastWheelMatrixOut = tempLastMtx;
}

float CWheel::GetReduceGripMultiplier()
{
	if (NetworkInterface::IsInCopsAndCrooks() && m_pParentVehicle && m_pParentVehicle->GetExplosionEffectSlick() && CPlayerSpecialAbilityManager::ms_CNCAbilityOilSlickFrictionReduction > 0.f)
	{
		return CPlayerSpecialAbilityManager::ms_CNCAbilityOilSlickFrictionReduction;
	}
	return REDUCE_GRIP_MULT;
}

///////////////////////////////////////////////////////////////////////////////
// Find the wheel static position.  This should later take a parameter telling
// how much load the wheel is under and find the balance position using that.
// But, for now there are bigger glitches to track down

Vec3V_Out CWheel::GetWheelStaticPositionLocal() const
{
	Vec3V vSusCompressed(RCC_VEC3V(m_aWheelLines.A));
	Vec3V vSusExtended(RCC_VEC3V(m_aWheelLines.B));
	ScalarV t((m_fStaticDelta+m_fWheelRadius)/m_fSusLength);
	return Lerp(t,vSusExtended,vSusCompressed);
}


////////////////////
// Calculate vectors for traction based on ground normal and direction of wheel
//
void CWheel::CalculateTractionVectorsNew( const Matrix34& matParent, Vector3& vecFwd, Vector3& vecSide, Vector3& vecHitDelta, float fSteerAngle, Vector3 localSpeed )//, Vector3 angVelocity, Vector3 linearVelocity )
{
	if( vecFwd.IsZero() )
	{
		matParent.UnTransform3x3( localSpeed );

		vecFwd.Cross( m_vecSuspensionAxis, m_vecAxleAxis );
		vecSide = m_vecAxleAxis;

		if( Abs( fSteerAngle ) > 0.0001f )
		{
			Matrix34 matTemp;
			matTemp.MakeRotateZ(fSteerAngle);
			matTemp.Transform3x3(vecFwd);
			matTemp.Transform3x3(vecSide);

			matTemp.UnTransform3x3(localSpeed);
		}

		float slipAngle = rage::Atan2f( localSpeed.x, localSpeed.y );

		static float defaultMaxSlipAngle = 20.0f * DtoR;
		TUNE_GROUP_FLOAT( WHEEL_SLIP_ANGLE, maxSlipAngle, defaultMaxSlipAngle, 0.0f, 1.0f, 0.01f );
		slipAngle = Clamp( slipAngle, -maxSlipAngle, maxSlipAngle );

		Matrix34 matTemp;
		matTemp.MakeRotateZ( slipAngle );
		matTemp.Transform3x3( vecFwd );

		matParent.Transform3x3( vecSide );
		matParent.Transform3x3( vecFwd );
		vecSide.Normalize();
		vecFwd.Normalize();
	}


	if(GetDynamicFlags().IsFlagSet(WF_HIT))
	{
		if( m_vecHitCentrePosOld.IsZero() )
		{
			vecHitDelta = m_vecHitCentrePosOld;
		}
		else
		{
			vecHitDelta = m_vecHitCentrePosOld - m_vecHitCentrePos;
		}
	}
	else
	{
		vecHitDelta.Zero();
	}
}

void CWheel::CalculateTractionVectors(const Matrix34& matParent, Vector3& vecFwd, Vector3& vecSide, Vector3& vecHitDelta, float fSteerAngle)
{
	if(vecFwd.IsZero())
	{
		vecSide = m_vecAxleAxis;

		if(rage::Abs(fSteerAngle) > 0.001f)
		{
			Matrix34 matTemp;
			matTemp.MakeRotateZ(fSteerAngle);
			matTemp.Transform3x3(vecSide);
		}
		matParent.Transform3x3(vecSide);

		Vector3 vecHitNormal = matParent.c;
		if(GetDynamicFlags().IsFlagSet(WF_HIT) || GetDynamicFlags().IsFlagSet(WF_HIT_PREV))
		{
			vecHitNormal =  m_vecHitNormal;
		}

		// check vehicle's not completely lying on it's side (relative to collision)
		if(rage::Abs(vecSide.Dot(vecHitNormal)) < 0.99f)
		{
			vecFwd.Cross(vecHitNormal, vecSide);
			vecFwd.Normalize();

			vecSide.Cross(vecFwd, vecHitNormal);
			vecSide.Normalize();
		}
		else
		{
			// if it's on it's side try and do a reasonable calculation using fwd vec instead of side vec first
			vecFwd.Set(0.0f, 1.0f, 0.0f);
			Matrix34 matTemp = matParent;

			float fSteer = m_fSteerAngle;
			if(!GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) || GetConfigFlags().IsFlagSet(WCF_BIKE_CONSTRAINED_COLLIDER))
			{
				fSteer *= rage::Max(0.0f, vecHitNormal.Dot(matParent.c));
			}

			matTemp.RotateLocalZ(fSteer);
			matTemp.Transform3x3(vecFwd);

			vecSide.Cross(matParent.b, vecHitNormal);
			vecSide.Normalize();

			vecFwd.Cross(vecHitNormal, vecSide);
			vecFwd.Normalize();
		}
	}

	if(GetDynamicFlags().IsFlagSet(WF_HIT))
	{
		if(m_vecHitCentrePosOld.IsZero())
			vecHitDelta = m_vecHitCentrePosOld;
		else
			vecHitDelta = m_vecHitCentrePosOld - m_vecHitCentrePos;
	}
	else
		vecHitDelta.Zero();
}


//
// fMult is per wheel traction multiplier, from tyre setup, so just affects traction coefficients, but not the angle they occur
// fLoss is traction loss due to road surface, or wetness, lowers traction coefficients, but also lowers the angle the max occurs
//
float CWheel::GetTractionCoefficientFromRatio(float fRatio, float fMult, float fLoss)
{
	float fAbsRatio = rage::Abs(fRatio);
	float fTractionCoeff = 0.0f;

	if( !( m_pHandling->hFlags & HF_HAS_RALLY_TYRES ) )
	{
		// calculate traction value
		if(fAbsRatio < sfTractionPeakAngle*fLoss)
			fTractionCoeff = m_pHandling->m_fTractionCurveMax * (fAbsRatio / (sfTractionPeakAngle*fLoss));
		else if(fAbsRatio < sfTractionMinAngle*fLoss)
			fTractionCoeff = m_pHandling->m_fTractionCurveMax + (m_pHandling->m_fTractionCurveMin - m_pHandling->m_fTractionCurveMax) * (fAbsRatio - sfTractionPeakAngle*fLoss) / (sfTractionMinAngle*fLoss);
		else
			fTractionCoeff = m_pHandling->m_fTractionCurveMin;
	}
	else // if we are using rally tyres grip increases with slip angle.
	{
		// calculate traction value
		if(fAbsRatio < sfTractionPeakAngle*fLoss)
		{
			fTractionCoeff = m_pHandling->m_fTractionCurveMin * (fAbsRatio / (sfTractionPeakAngle*fLoss));
		}
		else if(fAbsRatio < sfTractionMinAngle*fLoss)
		{
			fTractionCoeff = m_pHandling->m_fTractionCurveMin + (m_pHandling->m_fTractionCurveMax - m_pHandling->m_fTractionCurveMin) * (fAbsRatio - sfTractionPeakAngle*fLoss) / (sfTractionMinAngle*fLoss);
		}
		else
		{
			fTractionCoeff = m_pHandling->m_fTractionCurveMax;
		}
	}

	fTractionCoeff *= fMult * fLoss;

	if( m_pParentVehicle->InheritsFromAutomobile() &&
		static_cast<CAutomobile*>( m_pParentVehicle )->m_nAutomobileFlags.bInWheelieMode &&
		static_cast<CAutomobile*>( m_pParentVehicle )->m_nAutomobileFlags.bInWheelieModeWheelsUp &&
		GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
		GetConfigFlags().IsFlagSet( WCF_POWERED ) )
	{
		static dev_float sfWheelieModeTractionIncrease = 2.5f;
		static dev_float sfWheelieModeMaxTraction = 4.0f;

		float tractionIncrease = 0.5f + ( sfWheelieModeTractionIncrease * ( Min( 1.0f, (float)m_pParentVehicle->GetVariationInstance().GetEngineModifier() * 0.01f ) ) );

		fTractionCoeff *= tractionIncrease;
		fTractionCoeff = Min( fTractionCoeff, sfWheelieModeMaxTraction );
	}

	return fTractionCoeff;
}


#if __DEV && !__SPU
//////////////////////////
// Debug rendering of what the tyres are doing
//
Vector2 vecStartPos[10] =
{
	// LF					// RF						
	Vector2(0.3f, 0.2f),	Vector2(0.7f, 0.2f),	
	
	// LR					// RR
	Vector2(0.3f, 0.8f),	Vector2(0.7f, 0.8f),

	// LM1					// RM1						
	Vector2(0.2f, 0.3f),	Vector2(0.8f, 0.3f),

	// LM2					// RM2						
	Vector2(0.2f, 0.5f),	Vector2(0.8f, 0.5f),

	// LM3					// RM3						
	Vector2(0.2f, 0.7f),	Vector2(0.8f, 0.7f)
};
CompileTimeAssert(NUM_VEH_CWHEELS_MAX == 10);

void CWheel::DrawForceLimits(float fCurrent, float fCurrentSpring, float fLoadForce, float fNormalLoad, bool bLateral, float fLoss)
{
    Color32 drawColour(1.0f, 1.0f, 0.0f, 1.0f);

	Color32 drawRed(255,0,0,255);
	Color32 drawPurple(128,0,128,255);
	Color32 drawBlue(0,0,255,255);
	Color32 drawBlack(0,0,0,255);

	static const float fScreenWidth = 1.0f;
	static const float fScreenHeight = 1.0f;

	static const float fLineWidthMult = 0.01f;
	static const float fLineLengthMult = 0.035f;

	Vector2 vecStart = vecStartPos[m_WheelId - VEH_WHEEL_LF];
	vecStart.x *= fScreenWidth;
	vecStart.y *= fScreenHeight;

	const int iExpiryTime = (fwTimer::GetTimeStepInMilliseconds() * 3) / 2;
	if(bLateral)
	{
		float fMarkerWidth = fLineWidthMult*fScreenHeight;
		float fEndPos = sfTractionMinAngle*fLineLengthMult*fScreenWidth;
		float fPeakPos = sfTractionPeakAngle*fLoss*fLineLengthMult*fScreenWidth;
		float fCurrentPos = fCurrent*fLineLengthMult*fScreenWidth;
		fCurrentPos = Clamp(fCurrentPos, -fEndPos, fEndPos);
		float fCurrentSpringPos = fCurrentSpring*fLineLengthMult*fScreenWidth;


        //render slip angle
        Vector2 endPosition = Vector2(0.0f, -0.1f);
        endPosition.Rotate( fCurrent/sfTractionMinAngle );
        endPosition += Vector2(vecStart.x , vecStart.y);
        CPhysics::ms_debugDrawStore.AddLine( Vector2(vecStart.x, vecStart.y), endPosition, drawRed, iExpiryTime );

		
		//CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fEndPos, vecStart.y),Vector2( vecStart.x + fEndPos, vecStart.y), drawTyreTempColour,iExpiryTime);
		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fPeakPos, vecStart.y - fMarkerWidth),Vector2( vecStart.x + fPeakPos, vecStart.y + fMarkerWidth), drawColour,iExpiryTime);
		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fPeakPos, vecStart.y - fMarkerWidth),Vector2( vecStart.x - fPeakPos, vecStart.y + fMarkerWidth), drawColour,iExpiryTime);

		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fCurrentPos, vecStart.y - fMarkerWidth),Vector2( vecStart.x + fCurrentPos, vecStart.y + fMarkerWidth), drawPurple,iExpiryTime);
		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - 0.5f*fMarkerWidth, vecStart.y + fCurrentSpringPos),Vector2( vecStart.x + 0.5f*fMarkerWidth, vecStart.y + fCurrentSpringPos), drawBlue,iExpiryTime);
	

	}
	else
	{
		float fMarkerWidth = fLineWidthMult*fScreenWidth;
		float fEndPos = sfTractionMinAngle*fLineLengthMult*fScreenHeight;
		float fPeakPos = sfTractionPeakAngle*fLoss*fLineLengthMult*fScreenHeight;
		float fCurrentPos = -fCurrent*fLineLengthMult*fScreenHeight;
		fCurrentPos = Clamp(fCurrentPos, -fEndPos, fEndPos);
		//float fCurrentSpringPos = -fCurrentSpring*fLineLengthMult*fScreenWidth;

		//CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x, vecStart.y - fEndPos),Vector2( vecStart.x, vecStart.y + fEndPos), drawTyreTempColour,iExpiryTime);
		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fMarkerWidth, vecStart.y + fPeakPos),Vector2( vecStart.x + fMarkerWidth, vecStart.y + fPeakPos), drawColour,iExpiryTime);
		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fMarkerWidth, vecStart.y - fPeakPos),Vector2( vecStart.x + fMarkerWidth, vecStart.y - fPeakPos), drawColour,iExpiryTime);

		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fMarkerWidth, vecStart.y + fCurrentPos),Vector2( vecStart.x + fMarkerWidth, vecStart.y + fCurrentPos), drawPurple,iExpiryTime);
		//CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - 0.5f*fMarkerWidth, vecStart.y + fCurrentSpringPos),Vector2( vecStart.x + 0.5f*fMarkerWidth, vecStart.y + fCurrentSpringPos), drawBlue,iExpiryTime);
		
		float fCurrentLoadPos = (1.0f-(fLoadForce/fNormalLoad))*fLineLengthMult*fScreenWidth;
		
		CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - 0.5f*fMarkerWidth, vecStart.y + fCurrentLoadPos),Vector2( vecStart.x + 0.5f*fMarkerWidth, vecStart.y + fCurrentLoadPos), drawBlack,iExpiryTime);

    }
}

void CWheel::DrawForceLimitsImproved(float fCurrentLat, float fCurrentLong, float fCurrentSpring, float fLoadForce, float fNormalLoad, float fLoss)
{
    Color32 drawColour(1.0f, 1.0f, 0.0f, 1.0f);

    Color32 drawRed(255,0,0,255);
    Color32 drawPurple(128,0,128,255);
    Color32 drawBlue(0,0,255,255);


    static const float fScreenWidth = 1.0f;
    static const float fScreenHeight = 1.0f;

    float fLineWidthMult = 0.03f;
    float fLineLengthMult = 0.03f;

    fLineWidthMult *= (fLoadForce/fNormalLoad);
    fLineLengthMult *= (fLoadForce/fNormalLoad);

    Vector2 vecStart = vecStartPos[m_WheelId - VEH_WHEEL_LF];
    vecStart.x *= fScreenWidth;
    vecStart.y *= fScreenHeight;

    const int iExpiryTime = (fwTimer::GetTimeStepInMilliseconds() * 3) / 2;
    {
        float fMarkerWidth = fLineWidthMult*fScreenHeight;
        float fEndPos = sfTractionMinAngle*fLineLengthMult*fScreenWidth;
        float fPeakPos = sfTractionPeakAngle*fLoss*fLineLengthMult*fScreenWidth;
        float fCurrentPos = fCurrentLat*fLineLengthMult*fScreenWidth;
        fCurrentPos = Clamp(fCurrentPos, -fEndPos, fEndPos);

        float fCurrentSpringNorm = (m_fSusLength - fCurrentSpring)/m_fSusLength;
        float fCurrentSpringPos = fCurrentSpringNorm*fLineLengthMult*fScreenWidth;


        //render slip angle
        Vector2 endPosition = Vector2(0.0f, -0.1f);
        endPosition.Rotate( fCurrentLat/sfTractionMinAngle );
        endPosition += Vector2(vecStart.x , vecStart.y);
        CPhysics::ms_debugDrawStore.AddLine( Vector2(vecStart.x, vecStart.y), endPosition, drawRed, iExpiryTime );


        CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fPeakPos, vecStart.y - fMarkerWidth),Vector2( vecStart.x + fPeakPos, vecStart.y + fMarkerWidth), drawColour,iExpiryTime);
        CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fPeakPos, vecStart.y - fMarkerWidth),Vector2( vecStart.x - fPeakPos, vecStart.y + fMarkerWidth), drawColour,iExpiryTime);

        CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x + fCurrentPos, vecStart.y - fMarkerWidth),Vector2( vecStart.x + fCurrentPos, vecStart.y + fMarkerWidth), drawPurple,iExpiryTime);
        CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - 0.5f*fMarkerWidth, vecStart.y + fCurrentSpringPos),Vector2( vecStart.x + 0.5f*fMarkerWidth, vecStart.y + fCurrentSpringPos), drawBlue,iExpiryTime);
    }
    {
        float fMarkerWidth = fLineWidthMult*fScreenWidth;
        float fEndPos = sfTractionMinAngle*fLineLengthMult*fScreenHeight;
        float fPeakPos = sfTractionPeakAngle*fLoss*fLineLengthMult*fScreenHeight;
        float fCurrentPos = -fCurrentLong*fLineLengthMult*fScreenHeight;
        fCurrentPos = Clamp(fCurrentPos, -fEndPos, fEndPos);

        CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fMarkerWidth, vecStart.y + fPeakPos),Vector2( vecStart.x + fMarkerWidth, vecStart.y + fPeakPos), drawColour,iExpiryTime);
        CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fMarkerWidth, vecStart.y - fPeakPos),Vector2( vecStart.x + fMarkerWidth, vecStart.y - fPeakPos), drawColour,iExpiryTime);

        CPhysics::ms_debugDrawStore.AddLine(Vector2(vecStart.x - fMarkerWidth, vecStart.y + fCurrentPos),Vector2( vecStart.x + fMarkerWidth, vecStart.y + fCurrentPos), drawPurple,iExpiryTime);
    }
}

#endif

#if VEHICLE_USE_INTEGRATION_FOR_WHEELS

void Wi_START() {;}


dev_bool USE_SMOOTHED_COMPRESSION = true;
dev_bool FORCE_SMOOTHED_COMPRESSION = true;
dev_bool IGNORE_DAMPING_FOR_GRIP = true;
static dev_bool ENABLE_BUMPSTOP = false;
dev_float COMPRESSION_SPEED_CLAMP = 0.5f;
dev_float COMPRESSION_SPEED_CLAMP_GROUND_PHYSICAL = 0.05f;
static bool sb_increaseSuspensionForceWithDownforce;
//
void CWheel::IntegrateSuspensionAccel(phCollider* pVehCollider, Vector3& vecForceResult, Vector3& vecAccelResult, Vector3& vecAngAccelResult, float& fSuspensionForce, float& compressionDelta, const Matrix34& matUpdated, const Vector3& vecVel, const WheelIntegrationTimeInfo& timeInfo, const CWheel* pOppositeWheel, const float fGravity)
{
	Matrix34 matOriginal = RCC_MATRIX34(pVehCollider->GetMatrix());

	float fNewCompression = m_fCompression;
	float fDeltaCompression = m_fCompression - m_fCompressionOld;

	// smooth original compression ratio across integration time
	if((FORCE_SMOOTHED_COMPRESSION || (USE_SMOOTHED_COMPRESSION && m_pHandling->hFlags &HF_SMOOTHED_COMPRESSION)))
	{
		float fTimeRatio = timeInfo.fTimeInIntegration * timeInfo.fProbeTimeStep_Inv;
		fNewCompression = fTimeRatio * (m_fCompression - m_fCompressionOld) + m_fCompressionOld;
		// Don't change fDeltaCompression since we want that to remain on a consistent timscale whether we are smoothing the compression or not
	}

	// supplied t-values are not limited to 0.0f, so we need to limit the compression values we use to calculate spring force
	// (but not the compression values we use to  calculate damping!)
	float fNewCompressionClamped = rage::Min(fNewCompression, m_fSusLength);

	// tyre deflation mods compression results
	if(m_fTyreHealth < TYRE_HEALTH_DEFAULT)
	{
		fNewCompressionClamped -= GetTyreBurstCompression();
		fNewCompressionClamped = rage::Max(0.0f, fNewCompressionClamped);
	}
	// tyres sink into soft ground
	fNewCompressionClamped -= m_fExtraSinkDepth;
    fNewCompressionClamped += m_fWheelImpactOffset;
	fNewCompressionClamped = rage::Max(0.0f, fNewCompressionClamped);	

	//WARNING - not sure about the compression speed calculation here
	float fCompressionSpeed = 0.0f;
	float fCompressionSpeedClamp = m_pHitPhysical ? COMPRESSION_SPEED_CLAMP_GROUND_PHYSICAL : COMPRESSION_SPEED_CLAMP*m_fSusLength;


	fDeltaCompression += compressionDelta * timeInfo.fTimeInIntegration_Inv * timeInfo.fProbeTimeStep;
	fCompressionSpeed = rage::Min(fDeltaCompression, fCompressionSpeedClamp) * timeInfo.fProbeTimeStep_Inv;
	
    float fCompression = fNewCompressionClamped;// - m_fStaticDelta;

    if( !m_pHandling->GetCarHandlingData() ||
        !( m_pHandling->GetCarHandlingData()->aFlags & CF_FIX_OLD_BUGS ) )
    {
        fCompression -= m_fStaticDelta;
    }

	//calculate anti-roll bar force
	float fAntiRollBarForce = 0.0f;
	if(m_pHandling->m_fAntiRollBarForce > 0.0f && pOppositeWheel)
	{
		float ARBConstant = m_pHandling->m_fAntiRollBarForce * m_pHandling->GetAntiRollBarBias(m_fFrontRearSelector) * pVehCollider->GetMass() * fGravity;
		fAntiRollBarForce = -ARBConstant*(pOppositeWheel->GetWheelCompression()-GetWheelCompression());
	}

	// suspension health affects static force, and damping
	float fSusSpringForce = m_fStaticForce;
	if(m_fSuspensionHealth < sfSuspensionHealthLimit2)
	{
		fSusSpringForce = m_fStaticForce * sfSuspensionHealthSpringMult2;
	}
	else if(m_fSuspensionHealth < sfSuspensionHealthLimit1)
	{
		fSusSpringForce = m_fStaticForce * sfSuspensionHealthSpringMult1;
	}

    float fSpringClamp = 1.0f;
    float fBumpStopMult = 1.0f;
    //increase the spring clamp on bikes so the wheels don't go through the floor. Also increase spring rate when suspension is past its limits
    static dev_float sfSpringClamp = 10.0f;
    fSpringClamp = sfSpringClamp;


    if(ENABLE_BUMPSTOP)
    {
    	static dev_float sfBumpStopStart = 0.03f;
        float fBumpStopCompression = fNewCompression + m_fWheelRadius + sfBumpStopStart;
        if(fBumpStopCompression >= m_fSusLength)
        {
            if(fCompressionSpeed > 0.0 || fNewCompression + m_fWheelRadius >= m_fSusLength)
            {
                static dev_float sfBumpStopExtraStiffness = 2.5f;
                float amountOverFraction = (fBumpStopCompression - m_fSusLength)/m_fSusLength;
                fBumpStopMult = 1.0f + (amountOverFraction * sfBumpStopExtraStiffness);
            }

        }
    }

	static dev_float sfSpringMult = 0.2f;

	float fSuspensionMult = (GetConfigFlags().IsFlagSet(WCF_TRACKED_WHEEL) && (m_fFrontRearSelector <= 1.0f && m_fFrontRearSelector >= -1.0f)) ? sfSpringMult : 1.0f; // Reduce the stiffness of middle wheels on tracked vehicles, to make them bounce less when doing over bumps.

	TUNE_GROUP_BOOL( VEHICLE_SPOILER_BUG, increaseSuspensionForceWithDownforce, true );
	sb_increaseSuspensionForceWithDownforce = increaseSuspensionForceWithDownforce;

	if( sb_increaseSuspensionForceWithDownforce )
	{
		if( !GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && 
			!GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL) &&
			m_fDownforceMult > CWheel::ms_fDownforceMult )
		{
			TUNE_GROUP_FLOAT( VEHICLE_SPOILER_BUG, springStrengthIncreaseFactor, 4.0f, 0.0f, 100.0f, 0.1f );
			fSuspensionMult *= 1.0f + ( ( m_fDownforceMult - CWheel::ms_fDownforceMult ) * springStrengthIncreaseFactor );
		}
	}

    if( m_IncreaseSuspensionForceWithSpeed )
    {
        TUNE_GROUP_FLOAT( VEHICLE_SUSPENSION_BUG, springStrengthIncreaseFactor, 4.0f, 0.0f, 100.0f, 0.1f );
        TUNE_GROUP_FLOAT( VEHICLE_SUSPENSION_BUG, maxSpeedForFullStrengthIncrease2, 1600.0f, 0.0f, 10000.0f, 0.1f );
		TUNE_GROUP_FLOAT( VEHICLE_SUSPENSION_BUG, maxSuspensionForce, 6.0f, 0.0f, 10000.0f, 0.1f );
        fSuspensionMult *= 1.0f + ( ( Min( 1.0f, vecVel.Mag2() / maxSpeedForFullStrengthIncrease2 ) ) * springStrengthIncreaseFactor );

		float maxSuspMult = maxSuspensionForce / m_pHandling->m_fSuspensionForce;
		fSuspensionMult = Min( fSuspensionMult, maxSuspMult );
    }

	float fSpringRate = m_pHandling->m_fSuspensionForce * fSuspensionMult * m_pHandling->GetSuspensionBias(m_fFrontRearSelector) * fBumpStopMult;

    if( !m_pHandling->GetCarHandlingData() ||
        !( m_pHandling->GetCarHandlingData()->aFlags & CF_FIX_OLD_BUGS ) )
    {
        fSusSpringForce = rage::Clamp( fSusSpringForce + fCompression * fSpringRate, 0.0f, fSpringClamp );
    }
    else
    {
		float fStaticForce = m_fStaticForce;
		if(m_pHandling->GetCarHandlingData() && 
			m_pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_REDUCED_SUSPENSION_FORCE &&
			m_ReduceSuspensionForce )
		{
			fStaticForce = m_fStaticForce * sfSuspensionHealthSpringMult2;//Reduce the suspension force to the minimum so cars look stanced. Not sure if we should be using fSusSpringForce here but in the interest of not changing too much I'm using m_fStaticForce
		}

		fSusSpringForce = Min( fStaticForce, ( fCompression / m_fStaticDelta ) * fStaticForce );

        float fCompressionFromStatic = Max( 0.0f, fCompression - m_fStaticDelta );

        fSusSpringForce = rage::Clamp( fSusSpringForce + ( fCompressionFromStatic * fSpringRate ), 0.0f, fSpringClamp );
    }
	
	if( m_bSuspensionForceModified )
	{
		TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, scaleSuspensionForceWhenLanding, 4.5f, 0.0f, 50.0f, 0.1f );
		TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, maxSuspensionForceWhenLanding, 1.0f, 0.0f, 50.0f, 0.1f );

		fSusSpringForce *= scaleSuspensionForceWhenLanding;

		if( m_WheelTargetState != WHS_BOUNCE_ACTIVE &&
			m_WheelTargetState != WHS_BOUNCE_ALL_ACTIVE )
		{
			if( m_WheelTargetState == WHS_BOUNCE_LANDED &&
				fSusSpringForce > maxSuspensionForceWhenLanding )
			{
				float fDamage = fSusSpringForce - maxSuspensionForceWhenLanding;
				static float damageThreshold = 0.95f;

				if( fDamage > damageThreshold )
				{
					m_fSuspensionHealth = Max( m_fSuspensionHealth - fDamage, 0.0f );
				}
			}
		}

		fSusSpringForce = Min( fSusSpringForce, maxSuspensionForceWhenLanding );
	}

	fSusSpringForce *= pVehCollider->GetMass() * fGravity;
	fSusSpringForce += fAntiRollBarForce;//add the anti-roll bar force

	float fSusDampingForce = fCompressionSpeed * m_pHandling->m_fSuspensionReboundDamp * pVehCollider->GetMass() * fGravity;

	if( m_bSuspensionForceModified )
	{
		TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, scaleDampingForceWhenLanding, 0.7f, 0.0f, 50.0f, 0.1f );
		fSusDampingForce *= scaleDampingForceWhenLanding;
	}

	// use different damping rate for compression
	// Using different damping scheme on unbalanced suspensions can cause jittering, which happens on some of plane models, such as Duster. Disable quadratic damping for planes
	if(fCompressionSpeed > 0.0 && !GetConfigFlags().IsFlagSet(WCF_PLANE_WHEEL))
    {
        fSusDampingForce = sqrt(fCompressionSpeed) * m_pHandling->m_fSuspensionCompDamp * pVehCollider->GetMass() * fGravity;
    }

	// suspension health affects static force, and damping
	if(m_fSuspensionHealth < sfSuspensionHealthLimit2)
	{
		fSusDampingForce *= sfSuspensionHealthDampMult2;
	}
	else if(m_fSuspensionHealth < sfSuspensionHealthLimit1)
	{
		fSusDampingForce *= sfSuspensionHealthDampMult1;
	}

	// need to modify damping by handling bias (already included in static force for springs)
	fSusDampingForce *= m_pHandling->GetSuspensionBias(m_fFrontRearSelector);


    // limit rebound damping to negate forces applied by springs (i.e. don't suck down onto the ground)
    if(fSusDampingForce < -fSusSpringForce)
	    fSusDampingForce = -fSusSpringForce;

	Vector3 vecSusNormal = m_vecHitNormal;
	Vector3 fvDotFwd = matUpdated.b.DotV(m_vecHitNormal);

	Vector3 fvFwdSpeed = matUpdated.b.DotV(vecVel-m_vecGroundVelocity);
	// the following code can cause issues when wheel matrix doesn't line up with vehicle matrix, which happens for some airplane models. Disable it for planes for now.
 	if((fvDotFwd * fvFwdSpeed).IsLessThanDoNotUse(VEC3_ZERO) && !GetConfigFlags().IsFlagSet(WCF_PLANE_WHEEL))
 	{
 		vecSusNormal.Subtract(fvDotFwd * matUpdated.b);
 		vecSusNormal.Normalize();
 	}

	vecForceResult = vecSusNormal * (fSusSpringForce + fSusDampingForce);
	// want to pass back total suspension force value, used to calculate grip
	if(GetConfigFlags().IsFlagSet(WCF_BIKE_CONSTRAINED_COLLIDER) || IGNORE_DAMPING_FOR_GRIP)
		fSuspensionForce = fSusSpringForce + rage::Min(0.0f, fSusDampingForce);
	else
		fSuspensionForce = fSusSpringForce + fSusDampingForce;

	// divide force by mass to get acceleration
	vecAccelResult = vecForceResult * pVehCollider->GetInvMass();

	// angular acceleration is a bit more complex
	// want to apply suspension force at hit position
	Vector3 vecTorque;

    static dev_bool useAntiPitchAndDive = true;
    if( useAntiPitchAndDive && !GetConfigFlags().IsFlagSet(WCF_BIKE_FALLEN_COLLIDER) )
    {
		float rollCentreHeightBias = m_pHandling->GetRollCentreHeightBias(m_fFrontRearSelector);

        if( m_pParentVehicle->InheritsFromAutomobile() &&
            m_pHandling->GetCarHandlingData() )
        {
            CAutomobile* pAutomobile = static_cast< CAutomobile* >( m_pParentVehicle );

            if( m_pHandling->GetCarHandlingData()->aFlags & CF_REDUCE_BODY_ROLL_WITH_SUSPENSION_MODS )
            {
                rollCentreHeightBias += pAutomobile->GetFakeSuspensionLoweringAmount();
            }
        }

		if( m_bHasDonkHydraulics )
		{
			rollCentreHeightBias += m_fSuspensionRaiseAmount;
		}

        // Work out where the roll centre is
        Vector3 pitchAndRollMod( 0.0f, 0.0f, rollCentreHeightBias );
        //transform the anti pitch and dive into world space.
        matOriginal.Transform3x3(pitchAndRollMod);

        //work out the amount of torque that we generate
        vecTorque = (m_vecHitCentrePos + pitchAndRollMod) - matOriginal.d;
        vecTorque.Cross(vecForceResult);
    }
    else
    {
        vecTorque = m_vecHitCentrePos - matOriginal.d;
        vecTorque.Cross(vecForceResult);
    }

	matUpdated.UnTransform3x3(vecTorque, vecAngAccelResult);	

	if(GetConfigFlags().IsFlagSet(WCF_BIKE_CONSTRAINED_COLLIDER))
    {
		vecAngAccelResult.And(VEC3_ANDY); // zero y component
    }

	vecAngAccelResult.Multiply(RCC_VECTOR3(pVehCollider->GetInvAngInertia()));
	matUpdated.Transform3x3(vecAngAccelResult);
}
static dev_float sfFrictionMultScaleUpSpeed = 15.0f;
float CWheel::GetFrictionDamageScaledBySpeed(const Vector3& vVelocity)
{
	if(vVelocity.Mag2() < sfFrictionMultScaleUpSpeed)
	{
		return m_fFrictionDamage * (vVelocity.Mag2()/sfFrictionMultScaleUpSpeed);
	}
	else
	{
		return m_fFrictionDamage;
	}
}

#define svScaleRotThreshold Vector3(5.0f,5.0f,5.0f)
#define svScaleRotThresholdRangeInv Vector3(0.2f,0.2f,0.2f)
#define svScaleRotThresholdMult Vector3(0.5f,0.5f,0.5f)
#define svScaleRotThresholdOneWheelOffGroundMult Vector3(4.0f,4.0f,4.0f)

#define svQuadScaleRotThresholdMult Vector3(1.5f,1.5f,1.5f)

dev_bool sbScaleRotForBikes = true;
dev_bool sbScaleRotForBikesOneWheelOffGround = true;
dev_bool sbScaleRotForQuads = true;
dev_bool sbScaleRotForCars = false;
static bool sbUseNewTractionVectors = false;

void CWheel::IntegrateTyreAccel(phCollider* pVehCollider, Vector3& vecForceResult, Vector3& vecAccelResult, Vector3& vecAngAccelResult, Vector3& vecFwd, Vector3& vecSide, float& fWheelAngSlipDtResult, float fSuspensionForce, const Matrix34& matUpdated, const Vector3& vecVel, const Vector3& vecAngVel, float fWheelAngSlip, const WheelIntegrationTimeInfo& timeInfo, const CWheel *pOppositeWheel, const float fGravity)
{
	// zero results in case we jump out early
	vecForceResult.Zero();
	vecAccelResult.Zero();
	vecAngAccelResult.Zero();

	// if no weight on the wheels then do a small amount of traction (could return here instead?)
	if(fSuspensionForce <= 0.01f * pVehCollider->GetMass() * fGravity)
		return;

	//phCollider* pVehCollider = m_pParentVehicle->GetCollider();	Assert(pVehCollider); if(pVehCollider==NULL) return;
	Matrix34 matOriginal = RCC_MATRIX34(pVehCollider->GetMatrix());

	// smooth original values across time
	float fTimeRatio = timeInfo.fTimeInIntegration * timeInfo.fProbeTimeStep_Inv;

	float fSteerAngle = m_fSteerAngle;
	float fDriveForce = (fTimeRatio*m_fDriveForce + (1.0f - fTimeRatio)*m_fDriveForceOld) * pVehCollider->GetMass() * fGravity;
	float fBrakeForce = (fTimeRatio*m_fBrakeForce + (1.0f - fTimeRatio)*m_fBrakeForceOld) * pVehCollider->GetMass() * fGravity;
	

	// calc local speed at contact point
	Vector3 vecLocalSpeed;
	vecLocalSpeed.Subtract(m_vecHitCentrePos, matOriginal.d);
	vecLocalSpeed.CrossNegate(vecAngVel);
	vecLocalSpeed.Subtract(m_vecGroundVelocity);
	vecLocalSpeed.Add(vecVel);

	// calc fwd and side vectors for traction
	Vector3 vecHitDelta;
	TUNE_GROUP_BOOL( WHEEL_SLIP_ANGLE, testNewSlipAngle, false );
	sbUseNewTractionVectors = testNewSlipAngle; 

	if( sbUseNewTractionVectors || GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) )
	{
		CalculateTractionVectorsNew(RCC_MATRIX34(pVehCollider->GetMatrix()), vecFwd, vecSide, vecHitDelta, fSteerAngle, vecLocalSpeed);
	}
	else
	{
		CalculateTractionVectors(RCC_MATRIX34(pVehCollider->GetMatrix()), vecFwd, vecSide, vecHitDelta, fSteerAngle);
	}


	Vector3 vecTempSpeedAlongFwd = vecLocalSpeed.DotV(vecFwd);


	// add a scale to rotational velocity influence for bikes, effectively make them behave as if they were longer, to enhance stability
	if(((sbScaleRotForBikes && vecTempSpeedAlongFwd.IsGreaterThan(svScaleRotThreshold) && (GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL))) ||  
		(sbScaleRotForCars && !(GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL)) && !(GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL)) )))
	{
		Vector3 vecScale = (vecTempSpeedAlongFwd - svScaleRotThreshold) * svScaleRotThresholdRangeInv;
		vecScale.Min(vecScale, VEC3_IDENTITY);
		vecScale.Max(vecScale, VEC3_ZERO);
		vecScale.Multiply(svScaleRotThresholdMult);
		vecScale.Add(VEC3_IDENTITY);
		vecLocalSpeed.Multiply(vecScale);
	}

	// add a scale to rotational velocity influence for bikes, effectively make them behave as if they were longer, to enhance stability
	if( sbScaleRotForQuads && (GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL)) )
	{
		Vector3 vecScale = VEC3_IDENTITY;
		vecScale.Multiply(svQuadScaleRotThresholdMult);
		vecLocalSpeed.Multiply(vecScale);
	}

	// add a scale to rotational velocity influence for bikes when their opposite wheel is off the ground
	if( sbScaleRotForBikesOneWheelOffGround && (GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL)) && (pOppositeWheel && !pOppositeWheel->GetIsTouching()))
	{
		Vector3 vecScale = VEC3_IDENTITY;
		vecScale.Multiply(svScaleRotThresholdOneWheelOffGroundMult);
		vecLocalSpeed.Multiply(vecScale);
	}

	// calc and clamp force (grip) multiplier
	float fForceMult = fSuspensionForce;
	// For motorbikes we don't want large torques when one of the wheels gets grip, it produces undesirable effects
	if(GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL))
	{
		static dev_float sfNonTouchingOppositeMult = 0.5f;

		if(pOppositeWheel)
		{
			if(pOppositeWheel->GetIsTouching())
			{
				if(fForceMult < pOppositeWheel->GetTyreLoad()* fGravity)
				{
					fForceMult = pOppositeWheel->GetTyreLoad() * fGravity;
				}
			}
			else
			{
				fForceMult *= sfNonTouchingOppositeMult;// Reduce the grip of tyres that are gripping if the opposite tyre isn't touching the ground
			}
		}

		//Limit force overall.
		if(fForceMult > 2.0f*pVehCollider->GetMass() * fGravity)
			fForceMult = 2.0f*pVehCollider->GetMass() * fGravity;
	}
	else
	{
		if(fForceMult > 4.0f*pVehCollider->GetMass() * fGravity)
			fForceMult = 4.0f*pVehCollider->GetMass() * fGravity;
	}


	// calculate speed for damping
	Vector3 fSpeedAlongFwd = vecLocalSpeed.DotV(vecFwd);
	Vector3 fSpeedAlongSide = vecLocalSpeed.DotV(vecSide);


    // this is a bit of a hack, really want to apply v small friction force all the time or a bigger force with high friction wheels
    float fFrictionMult = sfFrictionStd;

    if( (m_pHandling->hFlags & HF_FREEWHEEL_NO_GAS) )
    {   
        fFrictionMult = sfFrictionFreeWheel;

        // Make sure when we are free wheeling at low speed we still stop.
        float speed = vecTempSpeedAlongFwd.Mag();
        if(speed < sfFreeWheelFrictionIncreaseSpeed)
        {
            float fFreeWheelFrictionIncreaseRatio = 1.0f - ((sfFreeWheelFrictionIncreaseSpeed - speed - sfFreeWheelFrictionMiniumumSpeed) / sfFreeWheelFrictionIncreaseSpeed);
            fFrictionMult += Clamp(sfFrictionBicycleHigh * fFreeWheelFrictionIncreaseRatio, sfFrictionFreeWheel, sfFrictionBicycleHigh);
        }
    }
    else if(GetConfigFlags().IsFlagSet(WCF_HIGH_FRICTION_WHEEL))
    {
        fFrictionMult = sfFrictionHigh;
    }

	if(m_fTyreHealth < TYRE_HEALTH_DEFAULT && GetConfigFlags().IsFlagSet(WCF_BICYCLE_WHEEL))	// Bicycle on burst tyre
	{
		float speed = vecTempSpeedAlongFwd.Mag();
		if(speed < sfFreeWheelFrictionIncreaseSpeed)
		{
			fFrictionMult += sfFrictionBicycleHigh * (1.0f - (m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV));
		}
		else
		{
			fFrictionMult += sfFrictionBicycleHighDamageTyre * (1.0f - (m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV));
		}
		fBrakeForce += rage::Max(GetFrictionDamageScaledBySpeed(vecVel), fFrictionMult) * pVehCollider->GetMass() * fGravity;
	}
	else if(GetDynamicFlags().IsFlagSet(WF_ON_GAS) && m_fTyreHealth < TYRE_HEALTH_DEFAULT && m_fFrictionDamage < sfFrictionDamageStart)
	{
		float speed = vecTempSpeedAlongFwd.Mag();
		if(speed > sfFreeWheelFrictionIncreaseSpeed)
		{
			float fFrictionBrakeMult = sfFrictionCarDmagedTyre * (1.0f - (m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV));
			fBrakeForce += rage::Max(GetFrictionDamageScaledBySpeed(vecVel), fFrictionBrakeMult) * pVehCollider->GetMass() * fGravity; 
		}
	}
	else if(!GetDynamicFlags().IsFlagSet(WF_ON_GAS) || m_fFrictionDamage > sfFrictionDamageStart)
	{
		fBrakeForce += rage::Max(GetFrictionDamageScaledBySpeed(vecVel), fFrictionMult) * pVehCollider->GetMass() * fGravity;
	}

    //now apply a braking force due to material drag.
	if( m_fMaterialDrag > 0.0f )
	{
		float tractionLossMult = !CPhysics::ms_bInArenaMode ? GetHandlingData()->m_fTractionLossMult : GetHandlingData()->m_fTractionLossMult * 0.25f;
		fBrakeForce += m_fMaterialDrag * tractionLossMult * pVehCollider->GetMass() * fGravity * m_fStaticForce;
	}

	if( m_fExtraWheelDrag > 0.0f )
	{
		static dev_float increaseExtraDragWithVelocityFactor = 20.0f;
		static dev_float maxSpeedForincreaseExtraDragWithVelocityFactor = 30.0f;
		static dev_float dragReductionTimeScale = 0.1f;

		float speedFactor = Min( 1.0f, Abs( m_fRotAngVel ) / maxSpeedForincreaseExtraDragWithVelocityFactor );
		float dragIncrease = increaseExtraDragWithVelocityFactor * speedFactor * speedFactor;

		fBrakeForce += m_fExtraWheelDrag * pVehCollider->GetMass() * fGravity * m_fStaticForce * dragIncrease;
		m_fExtraWheelDrag = Max( 0.0f, m_fExtraWheelDrag - ( timeInfo.fTimeInIntegration * speedFactor * dragReductionTimeScale ) );
	}

	//if we're going above materials top speed reduce the drive force
	if( m_fMaterialTopSpeedMult < 1.0f &&
		!m_bIgnoreMaterialMaxSpeed )
	{
		float tractionLossMult = !CPhysics::ms_bInArenaMode ? GetHandlingData()->m_fTractionLossMult : GetHandlingData()->m_fTractionLossMult * 0.25f;
		float fMaterialTopSpeedMult = 1.0f - ((1.0f - m_fMaterialTopSpeedMult) * tractionLossMult);

		float fMaxVelFlat = GetHandlingData()->m_fEstimatedMaxFlatVel * fMaterialTopSpeedMult;

		float fNonVecSpeedAlongFwd = vecTempSpeedAlongFwd.Mag();
		if(fNonVecSpeedAlongFwd > fMaxVelFlat)
		{
			static dev_float sfEaseOffThrottleSpeed = 80.0f;
			static dev_float sfEaseOnBrakesSpeed = 160.0f;
			static dev_float sfBicycleEaseOffThrottleSpeed = 30.0f;
			static dev_float sfBicycleEaseOnBrakesSpeed = 60.0f;

			float fEaseOffThrottleSpeed = sfEaseOffThrottleSpeed;
			float fEaseOnBrakesSpeed = sfEaseOnBrakesSpeed;

			if(GetConfigFlags().IsFlagSet(WCF_BICYCLE_WHEEL))
			{
				fEaseOffThrottleSpeed = sfBicycleEaseOffThrottleSpeed;
				fEaseOnBrakesSpeed = sfBicycleEaseOnBrakesSpeed;
			}


			float fSpeedOverMax = fNonVecSpeedAlongFwd - fMaxVelFlat;

			if(fDriveForce > 0.0f)
			{
				float fSpeedOverMaxThrottle = Clamp(fSpeedOverMax, 0.0f, fEaseOffThrottleSpeed);
				fDriveForce *= (1.0f - (fSpeedOverMaxThrottle / fEaseOffThrottleSpeed));
			}

			float fSpeedOverMaxBrake = Clamp(fSpeedOverMax, 0.0f, fEaseOnBrakesSpeed);
			fBrakeForce += (fSpeedOverMaxBrake / fEaseOnBrakesSpeed) * pVehCollider->GetMass() * fGravity * m_fStaticForce;
		}
	}

    // drive force gets canceled out my braking force
    if(fBrakeForce > rage::Abs(fDriveForce))
        fDriveForce = 0.0f;
    else
    {
        if(fDriveForce > fBrakeForce)
            fDriveForce -= fBrakeForce;
        else
            fDriveForce += fBrakeForce;
        fBrakeForce = 0.0f;
    }

    if(m_fAbsTimer > 0.0f)
        fBrakeForce = 0.0f;


	// calculate spring delta's
	float fSpringAlongFwd = 0.0f;
	float fSpringAlongSide = 0.0f;

	if(m_pHandling->m_fTractionSpringDeltaMax > 0.0f && m_vecGroundVelocity.IsZero())// when on another vehicle the wheel would move around a bit with the traction spring on. If we see oscilation I will have to fix this
	{
		if(m_vecHitCentrePosOld.IsZero())
			vecHitDelta = m_vecHitCentrePosOld;
		else
			vecHitDelta = m_vecHitCentrePosOld - m_vecHitCentrePos;

		// calculate wheel contact point with updated matrix
		Vector3 vecEstimatedHitPos;
		matOriginal.UnTransform(m_vecHitCentrePos, vecEstimatedHitPos);
		matUpdated.Transform(vecEstimatedHitPos);

		// get diff of how much contact would have moved
		vecEstimatedHitPos = vecEstimatedHitPos - m_vecHitCentrePos;
		// get rid of component along ground normal
		vecEstimatedHitPos -= m_vecHitNormal * vecEstimatedHitPos.DotV(m_vecHitNormal);

		// add to current hit delta
		vecHitDelta -= vecEstimatedHitPos;

		Vector3 vSpeedAlongFwdAbs = fSpeedAlongFwd;
		vSpeedAlongFwdAbs.Abs();
		Vector3 vBrakeForce(fBrakeForce,fBrakeForce,fBrakeForce);
		vBrakeForce.Abs();
		float fInvGravity = 1.0f/fGravity;
		vBrakeForce.Scale(pVehCollider->GetInvMass() * fInvGravity);
		if(fBrakeForce == 0.0f || vSpeedAlongFwdAbs.IsGreaterThan(vBrakeForce))
		{
			Vector3 fFwdDelta = vecHitDelta.DotV(vecFwd);
			vecHitDelta -= fFwdDelta * vecFwd;
		}

#if __DEV
		else
			vecHitDelta = vecHitDelta; // just want to be able to breakpoint this else
#endif

		if(vecHitDelta.Mag2() > m_pHandling->m_fTractionSpringDeltaMax*m_pHandling->m_fTractionSpringDeltaMax*sfTractionMinAngle*sfTractionMinAngle)
		{
			vecHitDelta.NormalizeFast();
			vecHitDelta *= m_pHandling->m_fTractionSpringDeltaMax*sfTractionMinAngle;
		}

		fSpringAlongFwd = vecHitDelta.Dot(vecFwd);
		fSpringAlongSide = vecHitDelta.Dot(vecSide);
	}

	float fTotalSpeedAcrossTyre = vecLocalSpeed.Mag() - m_vecHitNormal.Dot(vecLocalSpeed);
	Vector3 vecTyreForce(VEC3_ZERO);

	IntegrateCalculateForces(pVehCollider, vecTyreForce, fTotalSpeedAcrossTyre, fSpeedAlongFwd.GetX(), fSpeedAlongSide.GetX(), fSpringAlongFwd, fSpringAlongSide, fWheelAngSlip, fBrakeForce, fDriveForce, fForceMult, timeInfo.fIntegrationTimeStep, fGravity);

	Assert(vecTyreForce==vecTyreForce);

	float fDontSlideOffSlopeForce = 0.0f;

	//For bikes we don't want to slide down slopes or naturally drive down slopes to the side when riding along
	if(GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL))
	{
		Vector3 vecHitNormal = m_vecHitNormal;
		Vector3 BikeSide = matOriginal.a;
		Vector3 NewVec, NewVec2, NewVec3;

		// Get the hit normal in the plane of the side vector of the bike
		NewVec.Cross(BikeSide, ZAXIS);
		NewVec2.Cross(vecHitNormal, NewVec);
		NewVec3.Cross(NewVec, NewVec2);
		NewVec3.NormalizeSafe(ZAXIS);

		vecHitNormal = NewVec3;

		//calculate how much of a slope we are on to the side
		float slopeAngle = rage::Sinf(rage::AcosfSafe(vecHitNormal.Dot(ZAXIS)));

		//calculate how much force we need to stop the bike sliding down the slope to the side.
		fDontSlideOffSlopeForce = pVehCollider->GetMass() * m_fStaticForce* fGravity * slopeAngle;

		//make sure we apply our force in the correct direction
		Vector3 NewSideVec;
		NewSideVec.Cross(matOriginal.b, ZAXIS);
		float newdotf = NewSideVec.Dot(vecHitNormal);
		if(newdotf > 0.0f)
		{
			fDontSlideOffSlopeForce = -fDontSlideOffSlopeForce;
		}
	}

	vecForceResult = vecTyreForce.y*vecFwd + vecTyreForce.x*vecSide + fDontSlideOffSlopeForce*vecSide;

	Assert(vecForceResult==vecForceResult);

	// divide force by mass to get acceleration
	vecAccelResult = vecForceResult * m_fMassMultForAccelerationInv * pVehCollider->GetInvMass();

    // angular acceleration is a bit more complex
    static bool useRollCentre = true;
    Vector3 vecTorque;

    //Do we want to use the roll centre to work out how much torque to apply to the vehicle?
    if( useRollCentre && !GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && !GetConfigFlags().IsFlagSet(WCF_TRACKED_WHEEL) && !GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) )
	{
		Vector3 vecAxleAxisWithDirection(m_vecAxleAxis);

		if(!GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
		{
			vecAxleAxisWithDirection *= -1.0f;
		}

        // Work out where the roll centre is
		float rollCentreHeightBias = m_pHandling->GetRollCentreHeightBias(m_fFrontRearSelector);

		if( m_bHasDonkHydraulics )
		{
			rollCentreHeightBias += m_fSuspensionRaiseAmount;
		}

        if( m_pParentVehicle->InheritsFromAutomobile() &&
            m_pHandling->GetCarHandlingData() )
        {
            CAutomobile* pAutomobile = static_cast<CAutomobile*>( m_pParentVehicle );

            if( m_pHandling->GetCarHandlingData()->aFlags & CF_REDUCE_BODY_ROLL_WITH_SUSPENSION_MODS )
            {
                rollCentreHeightBias += pAutomobile->GetFakeSuspensionLoweringAmount();
            }
        }

        Vector3 rollMod( (m_fWheelWidth * 0.5f) * vecAxleAxisWithDirection.x, 0.0f, rollCentreHeightBias);
        //transform the roll centre into world space.
        matOriginal.Transform3x3(rollMod);

        //work out the amount of torque that we generate
        vecTorque = (m_vecHitCentrePos + rollMod) - matOriginal.d;
        vecTorque.Cross(vecForceResult);
    }
    else if( GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && !GetConfigFlags().IsFlagSet(WCF_BIKE_FALLEN_COLLIDER) )
    {
        // Work out where the roll centre is
        Vector3 pitchAndRollMod( 0.0f, 0.0f, m_pHandling->GetRollCentreHeightBias(m_fFrontRearSelector));
        //transform the anti pitch and dive into world space.
        matOriginal.Transform3x3(pitchAndRollMod);

        //work out the amount of torque that we generate
        vecTorque = (m_vecHitCentrePos + pitchAndRollMod) - matOriginal.d;
        vecTorque.Cross(vecForceResult);
    }
    else
    {
        vecTorque = m_vecHitCentrePos - matOriginal.d;//matUpdated.d;
        vecTorque.Cross(vecForceResult);
    }

	matUpdated.UnTransform3x3(vecTorque, vecAngAccelResult);

	if(GetConfigFlags().IsFlagSet(WCF_BIKE_CONSTRAINED_COLLIDER))
    {
		vecAngAccelResult.y = 0.0f;
    }

	vecAngAccelResult.Multiply(pVehCollider->GetUseSolverInvAngInertiaResetOverrideInWheelIntegrator() ? RCC_VECTOR3(pVehCollider->GetSolverInvAngInertiaResetOverride()) : RCC_VECTOR3(pVehCollider->GetInvAngInertia()));
	matUpdated.Transform3x3(vecAngAccelResult);

	float fAngSlipDelta = vecTyreForce.z - fWheelAngSlip;
	fWheelAngSlipDtResult = fAngSlipDelta * timeInfo.fIntegrationTimeStep_Inv;
}

#if __XENON
	#define ReciprocalEstimate 1
#else
	#define ReciprocalEstimate 0
#endif

//
//
dev_bool USE_LOAD_COEFFICIENT_MOD	= true;
dev_bool USE_EXTREME_LOAD_COEFFICIENT_MOD = false;
dev_bool USE_LOAD_STIFFNESS_MOD	= false;
dev_bool USE_SPEED_COEFFICIENT_MOD = false;
dev_bool USE_LOW_SPEED_COEFFICIENT_MOD = true;
dev_bool USE_SPEED_STIFFNESS_MOD	= true;
dev_bool USE_LATERAL_SPRING = true;
dev_bool USE_LONGITUDINAL_SPRING = true;
dev_bool USE_WET_MOD_BIAS = true;
dev_bool USE_CAMBER_MOD = true;
dev_bool USE_NEGATIVE_CAMBER_MOD = false;

dev_float GRIP_CHEAT_MULT_1 = 1.15f;
dev_float GRIP_CHEAT_MULT_2 = 1.30f;
dev_float BURNOUT_GRIP_MULT = 0.3f;

dev_float REDUCE_GRIP_ON_RIM_MULT_FRONT = 0.55f; 
dev_float REDUCE_GRIP_ON_RIM_MULT_REAR = 0.38f;
dev_float REDUCE_GRIP_ON_RIM_MULT_RANDOM= 0.05f;
dev_float RIMS_LATERAL_SLIP_RAND = 0.01f;

dev_float REDUCE_GRIP_DRIFT_TYRES_FRONT = 0.75f; 
dev_float REDUCE_GRIP_DRIFT_TYRES_REAR = 0.58f;

dev_float sfTractionCoeffLoadMult = 0.1f;
dev_float sfTractionCoeffExtremeLoadThreshold = 2.1f;
dev_float sfTractionCoeffExtremeLoadMult = 3.0f;
dev_float sfTractionCoeffLoadCap = 5.0f;
dev_float sfTractionStiffnessSpeedMult = 0.1f;
dev_float sfTractionStiffnessSpeedCap = 5.0f;
dev_float sfTractionCamberMult = 3.2f;
dev_float sfTractionCamberMultBikes = 0.0f;
dev_float sfTractionCoeffFrictionDamageMult = 1.0f;
dev_float sfSteerAngleCamberMult = -0.5f;
dev_float sfMaxSteerAngleForCamber = 45.0f;
dev_float sfCamberMultMaterialGripLimit = 0.95f;

dev_float sfLongSlipIncrementRate = 2.0f;
dev_float sfLongSlipIncrementRateBraking = 2.0f;
dev_float sfLongSlipIncrementRateBrakingLocked = 10.0f;
dev_float sfLongSlipBrakingLockedThreshhold = 0.9f;
dev_float sfLongSlipRatioDecrementRate = 5.0f;
dev_float sfLongSlipRatioReductionWithDamagedHydraulics = 0.55f;

dev_float sfAccelRatioLimit = 5.0f;
dev_float sfBrakeRatioLimit = 10.0f;
dev_float sfBrakeRatioLimitAbs = 1.5f;
dev_float sfBrakeAbsLimit = 1.5f;
dev_float sfBrakeAbsTimer = 0.0625f;
dev_float sfBrakeAbsMinSpeed = 5.0f;

dev_float sfCamberForceMinSpeed = 5.0f;

dev_float sfTractionSpeedMult = 0.08f;
dev_float sfTractionSpeedMin = 0.25f;
dev_float sfTractionSpeedLossMult = 5.0f;// Magic number to reduce skidding when off road or going up hills

dev_float CWheel::GetReduceGripOnRimMultFront()
{
	return REDUCE_GRIP_ON_RIM_MULT_FRONT;
}

dev_float CWheel::GetReduceGripOnRimMultRear()
{
	return REDUCE_GRIP_ON_RIM_MULT_REAR;
}

//
float CWheel::IntegrateCalculateForces(phCollider* pVehCollider, Vector3& vecForceResult, float fSpeed, float fSpeedAlongFwd, float fSpeedAlongSide, float fSpringAlongFwd, float fSpringAlongSide, float fRotSlipRatio, float fBrakeForce, float fDriveForce, float fLoadForce, float fIntegrationTimeStep, const float fGravity)
{
	Assert(fLoadForce > 0.0f);
	// fLoadForce * invMass * invGravity * invStaticForce
	float fLoadMultiplier = fLoadForce * pVehCollider->GetInvMass() /(m_fStaticForce*fGravity);
	float fStiffnessMultiplier = 1.0f;
    float fCamberMultiplier = 1.0f;

	//Reduce grip when pulling away from a stand still
	if(USE_LOW_SPEED_COEFFICIENT_MOD)
	{
		if(m_pHandling->m_fLowSpeedTractionLossMult > 0.0f && 
			!CPhysics::ms_bInArenaMode &&
			( !m_pParentVehicle->InheritsFromAutomobile() ||
			  !static_cast< CAutomobile* >(m_pParentVehicle)->m_nAutomobileFlags.bInWheelieMode ) )
		{
			if(fDriveForce && GetDynamicFlags().IsFlagSet(WF_FULL_THROTTLE))// only do this when using full throttle
			{
				//don't want to reduce grip too much when off road.
				float tractionLossMult = GetHandlingData()->m_fTractionLossMult;
				float fMaterialLoss = (1.0f + ((1.0f - m_fMaterialGrip) * tractionLossMult * sfTractionSpeedLossMult));

				const Mat34V rColliderMatrix = pVehCollider->GetMatrix();

				// don't want to reduce grip too much when going up a hill.
				ScalarV pitch = MagXY(rColliderMatrix.GetCol1());
				BoolV reverse = IsLessThan(SplatZ(rColliderMatrix.GetCol2()), ScalarV(V_ZERO));
				pitch = SelectFT(reverse, pitch, -pitch);
				pitch = Arctan2(SplatZ(rColliderMatrix.GetCol1()), pitch);

				float fPitchLoss = 1.0f + ((fabs(pitch.Getf())/PI) * sfTractionSpeedLossMult);

				float fTractionSpeedMin = (sfTractionSpeedMin / m_pHandling->m_fLowSpeedTractionLossMult);
				float fTractionSpeedMult = sfTractionSpeedMult / m_pHandling->m_fLowSpeedTractionLossMult;

				fLoadMultiplier *= rage::Min( 1.0f, (fTractionSpeedMin + rage::Min(1.0f - fTractionSpeedMin, fTractionSpeedMult * rage::Abs(fSpeed))) * fMaterialLoss * fPitchLoss);
			}
		}
	}

	// friction coefficient drops off linearly with load, +10% load => -1% friction
	if(USE_LOAD_COEFFICIENT_MOD && fLoadForce > 1.0f)
	{
		fLoadMultiplier *= 1.0f - sfTractionCoeffLoadMult * rage::Min(sfTractionCoeffLoadCap, fLoadMultiplier - 1.0f);

		if(USE_EXTREME_LOAD_COEFFICIENT_MOD)
		{
        	//When the load gets very high load reduce the loadmultiplier even more. This is so a car on two wheels is less likely to roll the vehicle as the friction from the wheels on the ground is very low.
        	if(fLoadMultiplier >= sfTractionCoeffExtremeLoadThreshold)
        	    fLoadMultiplier *= 1.0f - sfTractionCoeffExtremeLoadMult * rage::Min(sfTractionCoeffLoadCap, fLoadMultiplier - sfTractionCoeffExtremeLoadThreshold);
		}
	}
	if(USE_LOAD_STIFFNESS_MOD)
	{
		fStiffnessMultiplier = 1.0f + fLoadMultiplier;
	}
	// friction coefficient drops off linearly with velocity -10% per 10m/s
	if(USE_SPEED_COEFFICIENT_MOD)
	{
		fLoadMultiplier *= rage::Max(0.2f, 1.0f - 0.01f * fSpeed);
	}

	//Reduce the grip of the tyre when we have damage to the tyre.
	fLoadMultiplier *= 1.0f - rage::Min(0.95f, GetFrictionDamageScaledBySpeed(VEC3V_TO_VECTOR3(pVehCollider->GetVelocity())) * sfTractionCoeffFrictionDamageMult);

	if(USE_SPEED_STIFFNESS_MOD)
	{
		fStiffnessMultiplier *= 1.0f + rage::Min(sfTractionStiffnessSpeedCap, sfTractionStiffnessSpeedMult*rage::Abs(fSpeed));
	}

	// final friction force = friction coefficient x load
	fLoadMultiplier *= m_fStaticForce *  pVehCollider->GetMass() * fGravity;

#if ReciprocalEstimate
	float fLoadMultiplierInv = __fres(fLoadMultiplier);
#else
	float fLoadMultiplierInv = 1.0f / fLoadMultiplier;
#endif

	// calculate for front / rear traction bias
	float fTractionBias = m_pHandling->GetTractionBias(m_fFrontRearSelector);

	// calculate traction loss from surface, wetness, damage (burst tyres)
	float fTractionLoss = 1.0f;

	// Modify grip loss based on handling
	float tractionLossMult = !CPhysics::ms_bInArenaMode ? GetHandlingData()->m_fTractionLossMult : GetHandlingData()->m_fTractionLossMult * 0.25f;
	fTractionLoss += (m_fMaterialGrip - 1.0f) * tractionLossMult;

	if(USE_WET_MOD_BIAS)
	{
		fTractionBias *= m_fMaterialWetLoss;
	}
		
	fTractionLoss *= m_fMaterialWetLoss;

    if( m_fTyreWearRate > 0.0f )
    {
        fTractionLoss *= Min( 10.0f, ( 1.0f - ( ( 1.0f - m_fTyreWearRate ) * m_fMaxGripDiffFromWearRate ) ) );
    }

	// loose traction from burst tyres
	if(m_fTyreHealth <= 0.0f)	// on rim
    {
        fTractionLoss *= (Selectf(m_fFrontRearSelector, REDUCE_GRIP_ON_RIM_MULT_FRONT, REDUCE_GRIP_ON_RIM_MULT_REAR) + fwRandom::GetRandomNumberInRange(-REDUCE_GRIP_ON_RIM_MULT_RANDOM, REDUCE_GRIP_ON_RIM_MULT_RANDOM));

        //make sure traction doesn't go below 0
        if(fTractionLoss < 0.0f)
        {
            fTractionLoss = 0.001f;
        }
    }
	else if(m_pParentVehicle->m_bDriftTyres)//Reduce the grip of the tyres if script have set this vehicle to have drift tyres
	{
		fTractionBias *= (Selectf(m_fFrontRearSelector, REDUCE_GRIP_DRIFT_TYRES_FRONT, REDUCE_GRIP_DRIFT_TYRES_REAR));
	}
	else if(m_fTyreHealth < TYRE_HEALTH_DEFAULT)	// on burst tyre
	{
		if(GetConfigFlags().IsFlagSet(WCF_BICYCLE_WHEEL))
		{
			fTractionLoss *= 0.6f + 0.4f * (m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV);
		}
		else
		{
            if( m_fTyreWearRate > 0.0f )
            {
                if( m_fTyreHealth > TYRE_HEALTH_HIGH_GRIP )
                {
                    fTractionLoss *= 0.9f + 0.1f * ( m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV );
                }
                else if( m_fTyreHealth > TYRE_HEALTH_MEDIUM_GRIP )
                {
                    fTractionLoss *= 0.8f + 0.2f * ( m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV );
                }
                else
                {
                    fTractionLoss *= 0.4f + 0.6f * ( m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV );
                }
            }
            else
            {
			    fTractionLoss *= 0.4f + 0.6f * (m_fTyreHealth * TYRE_HEALTH_DEFAULT_INV);
            }
		}
	}

#if __BANK
    m_fLastCalculatedTractionLoss = fTractionLoss;
#endif //#if __BANK

	// lose traction from wheel damage
	if(GetFrictionDamageScaledBySpeed(VEC3V_TO_VECTOR3(pVehCollider->GetVelocity())) > 0.0f)
		fTractionBias *= 0.5f + 0.5f*rage::Max(0.0f, 1.0f - GetFrictionDamageScaledBySpeed(VEC3V_TO_VECTOR3(pVehCollider->GetVelocity())));
	if(GetDynamicFlags().IsFlagSet(WF_BURNOUT) && !GetConfigFlags().IsFlagSet(WCF_DONT_REDUCE_GRIP_ON_BURNOUT))
		fTractionBias *= BURNOUT_GRIP_MULT;
    if( GetDynamicFlags().IsFlagSet(WF_REDUCE_GRIP))
        fTractionBias *= m_fReducedGripMult;

	fTractionBias *= GetGripMult();

	Assert(fTractionLoss > 0.0f);

	// cheats to increase overall grip
	if(GetDynamicFlags().IsFlagSet(WF_CHEAT_GRIP2))
		fTractionBias *= GRIP_CHEAT_MULT_2;
	else if(GetDynamicFlags().IsFlagSet(WF_CHEAT_GRIP1))
		fTractionBias *= GRIP_CHEAT_MULT_1;

#if ReciprocalEstimate
	float fTractionLossInv = __fres(fTractionLoss);
	float fTractionBiasInv = __fres(fTractionBias);
#else
	float fTractionLossInv = 1.0f / fTractionLoss;
	float fTractionBiasInv = 1.0f / fTractionBias;
#endif

	//
	// LATERAL
	//
	float fLateralSlipAngle = rage::Atan2f(-fSpeedAlongSide, rage::Max(1.0f, rage::Abs(fSpeedAlongFwd)));

    if(m_fTyreHealth <= 0.0f)	// on rim
    {
        fLateralSlipAngle += fwRandom::GetRandomNumberInRange(-RIMS_LATERAL_SLIP_RAND, RIMS_LATERAL_SLIP_RAND) * rage::Max(1.0f, rage::Abs(fSpeedAlongFwd));
    }

	float fLateralSlipRatio = fLateralSlipAngle * (m_pHandling->m_fTractionCurveLateral_Inv * sfTractionMinAngleInv);
	// tyres stiffen up at higher speeds and loads
	fLateralSlipRatio *= fStiffnessMultiplier;

	if(!GetDynamicFlags().IsFlagSet(WF_NO_LATERAL_SPRING) && rage::Abs(fSpringAlongSide) > 0.0f && rage::Abs(fSpeed)*sfTractionSpringSpeedMult < 1.0f && USE_LATERAL_SPRING)
	{
		float tractionSpringToUse = m_pHandling->m_fTractionSpringDeltaMax_Inv;
		if (m_pParentVehicle->IsNetworkClone())
		{
			// don't do this for clones, as they'll deviate quite a bit for some models
			tractionSpringToUse = INIT_FLOAT;
		}

		float fSpringInfluence = 1.0f - rage::Abs(fSpeed)*sfTractionSpringSpeedMult;
		Assert(fSpringInfluence >= -0.0001f);

		float fSpringSideRatio = fSpringAlongSide * (tractionSpringToUse);// * sfTractionMinAngle);
		fSpringSideRatio *= fSpringInfluence;

		if(fSpringAlongSide * fLateralSlipRatio < 0.0f)
			fLateralSlipRatio += fSpringSideRatio;
		else if(fLateralSlipRatio > 0.0f)
			fLateralSlipRatio = rage::Max(fLateralSlipRatio, fSpringSideRatio);
		else
			fLateralSlipRatio = rage::Min(fLateralSlipRatio, fSpringSideRatio);
	}

	float fCamber = 0.0f;

    //if we have some camber, reduce the traction of the tire. This is to make it easier to catch cars that are rolling
    if(USE_CAMBER_MOD && !GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && m_fMaterialGrip >= sfCamberMultMaterialGripLimit)//don't do this on a bike and don't do this on low grip materials, to promote rolling when offroad.
    {   
        Vector3 vSusAxisWorld = m_vecSuspensionAxis;

		Vector3 vecAxleAxisWithDirection(m_vecAxleAxis);

		if(!GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
		{
			vecAxleAxisWithDirection *= -1.0f;
		}

        MAT34V_TO_MATRIX34(pVehCollider->GetMatrix()).Transform3x3(vSusAxisWorld);

        fCamber = 1.0f - m_vecHitNormal.Dot(vSusAxisWorld);

        fCamber += ((GetSteeringAngle()/sfMaxSteerAngleForCamber) * sfSteerAngleCamberMult)*vecAxleAxisWithDirection.x;

        //increase grip if we have negative camber
        if(USE_NEGATIVE_CAMBER_MOD)
        {  
            Vector3 vAxleAxisWorld = vecAxleAxisWithDirection;
            MAT34V_TO_MATRIX34(pVehCollider->GetMatrix()).Transform3x3(vAxleAxisWorld);
            float direction = m_vecHitNormal.Dot(vAxleAxisWorld);

            //work out if this is positive or negative camber
            fCamber = direction > 0.0f ? fCamber : -fCamber;
        }

        fCamberMultiplier *= 1.0f - sfTractionCamberMult * rage::Min(sfTractionCoeffLoadCap, fCamber );

        fTractionLoss *= fCamberMultiplier;

        //make sure traction doesn't go below 0
        if(fTractionLoss < 0.0f)
        {
            fTractionLoss = 0.001f;
        }
    }

	if(m_pHandling->m_fCamberStiffnesss > 0.0f)
	{
		ScalarV roll = MagXY(pVehCollider->GetMatrix().GetCol0());
		fCamber = Arctan2(SplatZ(pVehCollider->GetMatrix().GetCol0()), roll).Getf();

		//for bikes increase grip as bike rolls
		if(sfTractionCamberMultBikes > 0.0f)
		{
			fCamberMultiplier *= 1.0f - sfTractionCamberMultBikes * rage::Min(sfTractionCoeffLoadCap, fCamber );

			fTractionLoss *= fCamberMultiplier;

			//make sure traction doesn't go below 0
			if(fTractionLoss < 0.0f)
			{
				fTractionLoss = 0.001f;
			}
		}
	    
	}



	// calculate traction value
	//	float fLateralCoeff = GetTractionCoefficientFromRatio(fLateralSlipRatio);

	//
	// LONGITUDINAL
	//
	float fLongSlipRatio_FromWheelSpeed = fRotSlipRatio;
	float fLongSlipRatio = 0.0f;
	if(fDriveForce)
	{
        
		Assert(fBrakeForce==0.0f);
		float fDriveForceSign = (fDriveForce > 0.0f) ? -1.0f : 1.0f;
		// desired friction coefficient
		float fDriveCoeff = rage::Abs(fDriveForce) * fLoadMultiplierInv;

		// see if it's within min grip range
		if(fDriveCoeff < m_pHandling->m_fTractionCurveMin * fTractionBias*fTractionLoss)
		{
			fLongSlipRatio = fDriveCoeff * fDriveForceSign * (m_pHandling->m_fTractionCurveMax_Inv * sfTractionMinAngleInv * fTractionBiasInv * fTractionLossInv);
		}
		// else see if slip ratio from wheel rotation is still below the peak (or has changed slip direction)
		else if(rage::Abs(fLongSlipRatio_FromWheelSpeed) < (fTractionLoss * sfTractionPeakAngle * sfTractionMinAngleInv) || fDriveForceSign * fLongSlipRatio_FromWheelSpeed < 0.0f)
		{
			fLongSlipRatio = fDriveCoeff * fDriveForceSign * (m_pHandling->m_fTractionCurveMax_Inv * sfTractionMinAngleInv * fTractionBiasInv * fTractionLossInv);
		}

		if(fLongSlipRatio)
		{
			// as long as slip direction hasn't changed, if slip ratio is decreasing, want to smooth the transition
			if(fLongSlipRatio * fLongSlipRatio_FromWheelSpeed >= 0.0f
				&& rage::Abs(fLongSlipRatio) < rage::Abs(fLongSlipRatio_FromWheelSpeed))
			{
				static bool sb_dontIncreaseLongSlipRatioWithDownForce = false;
				bool inWheelieMode = m_pParentVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>( m_pParentVehicle )->m_nAutomobileFlags.bInWheelieMode;
				bool bUseHardRevLimitFlag = CVehicle::UseHardRevLimitFlag( m_pHandling, m_pParentVehicle->IsTunerVehicle(), m_pParentVehicle->m_Transmission.IsCurrentGearATopGear() );
				if( m_fSuspensionRaiseAmount != 0.0f ||
					( sb_dontIncreaseLongSlipRatioWithDownForce &&
					  m_pHandling->m_fDownforceModifier > 1.0f ) ||
					( bUseHardRevLimitFlag &&
					  m_pParentVehicle->m_Transmission.GetRevRatio() > 1.0f ) ||
					inWheelieMode )
				{
					//fLongSlipRatio = fLongSlipRatio_FromWheelSpeed + (fLongSlipRatio - fLongSlipRatio_FromWheelSpeed);
				}
				else
				{
					fLongSlipRatio = fLongSlipRatio_FromWheelSpeed + (fLongSlipRatio - fLongSlipRatio_FromWheelSpeed) * rage::Min(1.0f, sfLongSlipRatioDecrementRate * fIntegrationTimeStep);
				}
			}
		}
		// else we're exceeding available traction, need to calculate our new slip ratio, and corresponding friction coefficient
		else
		{
			float fLongCoeff = GetTractionCoefficientFromRatio(fLongSlipRatio_FromWheelSpeed * sfTractionMinAngle, fTractionBias, fTractionLoss);
			if(fLongCoeff < fDriveCoeff)
			{
				fLongSlipRatio = fLongSlipRatio_FromWheelSpeed + fDriveForceSign * (fDriveCoeff - fLongCoeff) * fIntegrationTimeStep * sfLongSlipIncrementRate;
			}
			else
			{
				fLongSlipRatio = fDriveForceSign * (sfTractionPeakAngle + (sfTractionMinAngle - sfTractionPeakAngle) * (m_pHandling->m_fTractionCurveMax - fDriveCoeff)*m_pHandling->m_fTractionCurveMaxMinusMin_Inv);
				fLongSlipRatio *= sfTractionMinAngleInv;
			}

			float fAccelRatioLimit = sfAccelRatioLimit;
			if(GetDynamicFlags().IsFlagSet(WF_CHEAT_TC) && !GetDynamicFlags().IsFlagSet(WF_BURNOUT))
			{
				fAccelRatioLimit = 1.0f * (sfTractionMinAngleInv * fTractionLossInv);
			}

			fLongSlipRatio = rage::Clamp(fLongSlipRatio, -fAccelRatioLimit, fAccelRatioLimit);
		}

		if( m_bReduceLongSlipRatioWhenDamaged &&
			m_fSuspensionHealth < sfSuspensionHealthLimit1 )
		{
			fLongSlipRatio *= sfLongSlipRatioReductionWithDamagedHydraulics;
		}
	}
	else
	{
		float fBrakeForceSign = (fSpeedAlongFwd > 0.0f) ? 1.0f : -1.0f;

		// need to limit brake force when the wheel comes to rest
		// B*1904924: Only use the min speed of 0.25f for bicycle wheels. This tweak was causing other vehicles to report incorrect slip angles, which caused other systems (audio, vfx) to not work correctly.
        float sfSpeedMin = GetConfigFlags().IsFlagSet(WCF_BICYCLE_WHEEL) ? 0.25f : 0.0f;
		float fBrakeForceLimited = rage::Min(fBrakeForce, rage::Max(rage::Abs(fSpeedAlongFwd), sfSpeedMin) *  pVehCollider->GetMass() * fGravity);

		// desired friction coefficient
		float fLongCoeff = rage::Abs(fBrakeForceLimited) * fLoadMultiplierInv;

		// see if it's within min grip range
		if(fLongCoeff < m_pHandling->m_fTractionCurveMin * fTractionBias*fTractionLoss)
			fLongSlipRatio = fLongCoeff * fBrakeForceSign * (m_pHandling->m_fTractionCurveMax_Inv * sfTractionMinAngleInv * fTractionBiasInv * fTractionLossInv);
		else if(rage::Abs(fLongSlipRatio_FromWheelSpeed) < (fTractionLoss * sfTractionPeakAngle * sfTractionMinAngleInv) || fBrakeForceSign * fLongSlipRatio_FromWheelSpeed < 0.0f)
			fLongSlipRatio = fLongCoeff * fBrakeForceSign * (m_pHandling->m_fTractionCurveMax_Inv * sfTractionMinAngleInv * fTractionBiasInv * fTractionLossInv);
		else
		{
			float fBrakeCoeff = fLongCoeff;
			fLongCoeff = GetTractionCoefficientFromRatio(fLongSlipRatio_FromWheelSpeed * sfTractionMinAngle, fTractionBias, fTractionLoss);
			if(fLongCoeff < fBrakeCoeff)
			{
				// increment slip ratio more quickly once wheels lockup
				float fIncRate = Selectf(fLongCoeff - sfLongSlipBrakingLockedThreshhold, sfLongSlipIncrementRateBrakingLocked, sfLongSlipIncrementRateBraking);
				fLongSlipRatio = fLongSlipRatio_FromWheelSpeed + fBrakeForceSign * (fBrakeCoeff - fLongCoeff) * fIntegrationTimeStep * fIncRate;
			}
			else
			{
				fLongSlipRatio = fBrakeForceSign * (sfTractionPeakAngle + (sfTractionMinAngle - sfTractionPeakAngle) * (m_pHandling->m_fTractionCurveMax - fBrakeCoeff)*m_pHandling->m_fTractionCurveMaxMinusMin_Inv);
				fLongSlipRatio *= sfTractionMinAngleInv;
			}
		}

		if(rage::Abs(fSpringAlongFwd) > 0.0f && rage::Abs(fSpeed)*sfTractionSpringSpeedMult < 1.0f && USE_LONGITUDINAL_SPRING)
		{
			float tractionSpringToUse = m_pHandling->m_fTractionSpringDeltaMax_Inv;
			if (m_pParentVehicle->IsNetworkClone())
			{
				// don't do this for clones, as they'll deviate quite a bit for some models
				tractionSpringToUse = INIT_FLOAT;
			}

			float fSpringInfluence = 1.0f - rage::Abs(fSpeed)*sfTractionSpringSpeedMult;
			Assert(fSpringInfluence >= -0.0001f);

			float fSpringFwdRatio = -fSpringAlongFwd * (tractionSpringToUse);// * sfTractionMinAngleInv);
			fSpringFwdRatio *= fSpringInfluence;

			if(fSpringFwdRatio * fLongSlipRatio < 0.0f)
				fLongSlipRatio += fSpringFwdRatio;
			else if(fLongSlipRatio > 0.0f)
				fLongSlipRatio = rage::Max(fLongSlipRatio, fSpringFwdRatio);
			else
				fLongSlipRatio = rage::Min(fLongSlipRatio, fSpringFwdRatio);
		}

		float fBrakeRatioLimit = sfBrakeRatioLimit;
		bool bUseAbs = false;
		if(GetDynamicFlags().IsFlagSet(WF_ABS) && rage::Abs(fSpeedAlongFwd) > sfBrakeAbsMinSpeed && m_fFrictionDamage < sfFrictionDamageStart)
		{
			fBrakeRatioLimit = sfBrakeRatioLimitAbs * (sfTractionMinAngleInv * fTractionLossInv);
			bUseAbs = true;
		}

		if(fBrakeForceSign > 0.0f && fLongSlipRatio > fBrakeRatioLimit)
		{
			fLongSlipRatio = fBrakeRatioLimit;
			if(bUseAbs)
				GetDynamicFlags().SetFlag(WF_ABS_ACTIVE);
		}
		else if(fBrakeForceSign < 0.0f && fLongSlipRatio < -fBrakeRatioLimit)
		{
			fLongSlipRatio = -fBrakeRatioLimit;
			if(bUseAbs)
				GetDynamicFlags().SetFlag(WF_ABS_ACTIVE);
		}
	}

#if __DEV && !__SPU
	if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING && ((CVehicle*)pVehCollider->GetInstance()->GetUserData())->GetStatus()==STATUS_PLAYER)
	{
		float fNormalLoad = (pVehCollider->GetMass()/4.0f)*10;
		DrawForceLimitsImproved(fLateralSlipRatio * sfTractionMinAngle, fLongSlipRatio * sfTractionMinAngle, m_fCompression, fLoadForce, fNormalLoad, fTractionLoss);
	}
#endif

	TUNE_GROUP_BOOL( WHEEL_SLIP_ANGLE, clampLaterialRatio, false );
	if( clampLaterialRatio )
	{
		fLateralSlipRatio = rage::Clamp(fLateralSlipRatio, -1.0f, 1.0f);
	}


	// need to combine longitudinal and lateral coefficients
	vecForceResult.x = fLateralSlipRatio;
	vecForceResult.y = -fLongSlipRatio;
	vecForceResult.z = 0.0f;

	Assert(vecForceResult == vecForceResult);

	if(GetDynamicFlags().IsFlagSet(WF_CHEAT_SC) && ! GetDynamicFlags().IsFlagSet(WF_BURNOUT))// && GetFlag(WF_REARWHEEL))
	{
		float fTractionClamp = sfTractionMinAngleInv * fTractionLossInv;
		vecForceResult.x = fLateralSlipRatio = rage::Clamp(fLateralSlipRatio, -fTractionClamp, fTractionClamp);

		if(vecForceResult.Mag2() > sfTractionMinAngleInv*sfTractionMinAngleInv)
		{
			float fLongSlipLimit = rage::square(sfTractionMinAngleInv) - rage::square(fLateralSlipRatio);
			fLongSlipLimit = rage::Max(0.0001f, fLongSlipLimit);
			fLongSlipLimit = rage::Sqrtf(fLongSlipLimit);
			fLongSlipLimit = rage::Max(0.05f, fLongSlipLimit);
			vecForceResult.y = rage::Clamp(vecForceResult.y, -fLongSlipLimit, fLongSlipLimit);
		}
	}
	Assert(vecForceResult == vecForceResult);

	float fTotalRatio = 0.0f;
	float fTotalCoefficient = 0.0f;

	// watch out for divide by zero
	if(vecForceResult.Mag2() > 0.0f)
	{
		fTotalRatio = vecForceResult.Mag();
		fTotalCoefficient = GetTractionCoefficientFromRatio(fTotalRatio * sfTractionMinAngle, fTractionBias, fTractionLoss);

		if(fBrakeForce > 0.0f && fSpeed > 1.0f
		&& fTotalRatio > 0.9f*sfTractionMinAngle*fTractionLoss
		&& rage::Abs(fLongSlipRatio*fSpeedAlongSide) > rage::Abs(fLateralSlipRatio*fSpeedAlongFwd))
		{
			vecForceResult.Set(-fSpeedAlongSide, -fSpeedAlongFwd, 0.0f);
			vecForceResult.Normalize();
			vecForceResult.Scale(fLoadMultiplier * fTotalCoefficient);
		}
		else
		{
			vecForceResult.Scale(fLoadMultiplier * fTotalCoefficient / fTotalRatio);
		}
	}
	else
	{
		vecForceResult.Zero();
	}
	Assert(vecForceResult == vecForceResult);
	
	//Camber stiffness is the force generated by the tyre when it's leaning over.
	if(m_pHandling->m_fCamberStiffnesss > 0.0f && rage::Abs(fSpeedAlongFwd) > sfCamberForceMinSpeed)
	{
		m_fTyreCamberForce = fCamber * -m_pHandling->m_fCamberStiffnesss * fLoadMultiplier * fTractionBias * fTractionLoss;
		vecForceResult.x += m_fTyreCamberForce;
	}
	else
	{
		m_fTyreCamberForce = 0.0f;
	}


	m_fSideSlipAngle = rage::Abs(fLateralSlipRatio) * sfTractionMinAngle;
	m_fFwdSlipAngle = rage::Abs(fLongSlipRatio) * sfTractionMinAngle;
	m_fEffectiveSlipAngle = rage::Abs(fTotalRatio) * sfTractionMinAngle;
    m_fTyreLoad = fLoadMultiplier / fGravity;

	// use z component to hold wheel rotation velocity result, calculated from full longitudinal slip angle
	vecForceResult.z = fLongSlipRatio;

	return (fTotalCoefficient * fLoadMultiplier);
}

//
//
//
#if !__SPU
void CWheel::IntegrationFinalProcess(phCollider* pVehCollider, ScalarV_In timeStep)
{
	TUNE_GROUP_FLOAT( ARENA_MODE_CRUSH_DAMAGE, stationarySpeedSquaredThreshold, 1.0f, 0.0f, 1000.0f, 0.1f );
	TUNE_GROUP_FLOAT( ARENA_MODE_CRUSH_DAMAGE, relativeVelocitySquaredForFullCrushDamage, 20.0f, 0.0f, 1000.0f, 0.1f );
	TUNE_GROUP_FLOAT( ARENA_MODE_CRUSH_DAMAGE, increaseCrushDamageMult, 15.0f, 0.0f, 1000.0f, 0.1f );

	// apply opposite force to the thing this wheel is sitting on
	if(m_pHitPhysical && m_pHitPhysical->GetCurrentPhysicsInst() && m_pHitPhysical->GetCurrentPhysicsInst()->IsInLevel())
	{
		// check we're not applying forces to ourself!
		Assert(!pVehCollider || !pVehCollider->GetInstance() || m_pHitPhysical != pVehCollider->GetInstance()->GetUserData());

		Vector3 vecApplyForce = m_vecSolverForce;
        if (!Verifyf(m_vecSolverForce == m_vecSolverForce, "Bad force <%f, %f, %f> computed by the wheel integrator on '%s'", VEC3V_ARGS(RCC_VEC3V(m_vecSolverForce)), m_pHandling->m_handlingName.TryGetCStr()))
		{
			return;
		}
		if(vecApplyForce.Mag2() >  m_pHitPhysical->GetMass()*m_pHitPhysical->GetMass() * sfWheelForceDeltaVCap*sfWheelForceDeltaVCap)
		{
			vecApplyForce.NormalizeFast();
			vecApplyForce.Scale(m_pHitPhysical->GetMass() * sfWheelForceDeltaVCap);
		}

		Vector3 vecHitPosition = m_vecHitPhysicalOffset;
		Vector3 vecOffsetPosition = vecHitPosition - VEC3V_TO_VECTOR3(m_pHitPhysical->GetTransform().GetPosition());

		if( !m_pHitPed && m_pHitPhysical->GetType() == ENTITY_TYPE_PED )
        {
            CPhysical *pHitPhysical = m_pHitPhysical;
            m_pHitPed = static_cast<CPed*>(pHitPhysical);
        }

        if(m_pHitPed)
		{
			CVehicle* pMyVehicle = (CVehicle*)(pVehCollider->GetInstance()->GetUserData());
			if(m_pHitPed->GetUsingRagdoll())
			{
				u32 nHitComponent = m_pHitPed==m_pHitPhysical ? m_nHitComponent : 0;

				ApplyTireForceToPed(m_pHitPed, pVehCollider, (u16)nHitComponent);
				Assert(m_pHitPed->GetPedIntelligence());
				Assert(m_pHitPed->GetPedIntelligence()->GetEventScanner());
				m_pHitPed->GetPedIntelligence()->GetEventScanner()->GetCollisionEventScanner().ProcessRagdollImpact(m_pHitPed, 0.0f, pMyVehicle,
					ZAXIS, (u16)nHitComponent, PGTAMATERIALMGR->g_idRubber);
			}
			else if (m_pHitPed->IsDead())
			{
				// Give the animated corpse some fake damage as an easy way to wake it up
				CTask* pTaskActive = m_pHitPed->GetPedIntelligence()->GetTaskActive();
				if (pTaskActive)
				{
					CDamageInfo damageInfo;
					pTaskActive->RecordDamage(damageInfo);
				}
			}
			else if (pMyVehicle && m_pHitPed->GetAttachParent() != pMyVehicle )
			{

				if (CTaskNMBehaviour::CanUseRagdoll(m_pHitPed, RAGDOLL_TRIGGER_IMPACT_CAR, pMyVehicle))
				{
					// Add the nm brace task
					nmEntityDebugf(m_pHitPed.Get(), "Starting nm brace task from collision with wheel of vehicle %s(%p)", pMyVehicle ? pMyVehicle->GetModelName() : "none", pMyVehicle);
					Vector3 vPedVelocity(m_pHitPed->GetVelocity());
					if (MagSquared(m_pHitPed->GetGroundVelocityIntegrated()).Getf() > vPedVelocity.Mag2())
					{
						vPedVelocity = VEC3V_TO_VECTOR3(m_pHitPed->GetGroundVelocityIntegrated());
					}
					CEventSwitch2NM event(10000, rage_new CTaskNMBrace(500, 10000, pMyVehicle, CTaskNMBrace::BRACE_DEFAULT, vPedVelocity));
					m_pHitPed->SwitchToRagdoll(event);
				}
				else
				{
					float fVelocity2 = pMyVehicle->GetVelocityIncludingReferenceFrame().Mag2();
					if(fVelocity2 > square(CTaskNMBehaviour::sm_Tunables.m_VehicleMinSpeedForContinuousPushActivation))
					{
						// note that the ped is being pushed by the vehicle
						m_pHitPed->SetPedResetFlag(CPED_RESET_FLAG_CapsuleBeingPushedByVehicle, true);
					}
				}
			}
		}
		else
		{
            // fragable objects can mean we are trying to hit things too far away from the centroid, so double check we are ok.;
            Vec3V vWorldCentroid;
            float fCentroidRadius = 0.0f;
            if(m_pHitPhysical->GetCurrentPhysicsInst() && m_pHitPhysical->GetCurrentPhysicsInst()->GetArchetype())
            {
                vWorldCentroid = m_pHitPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetWorldCentroid(m_pHitPhysical->GetCurrentPhysicsInst()->GetMatrix());
                fCentroidRadius = m_pHitPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
            }
            else
                fCentroidRadius = m_pHitPhysical->GetBoundCentreAndRadius(RC_VECTOR3(vWorldCentroid));


            Vec3V vWorldPos = m_pHitPhysical->GetTransform().GetPosition() + RCC_VEC3V(vecOffsetPosition);
            if(DistSquared(vWorldPos, vWorldCentroid).Getf() <= fCentroidRadius*fCentroidRadius)
            {
				float fBreakingForce = m_vecSolverForce.Mag(); 

				if (fragInst* pHitFrag = m_pHitPhysical->GetFragInst())
				{
					const fragPhysicsLOD* pPhysics = pHitFrag->GetTypePhysics();
					fragTypeChild* pChild = pPhysics->GetAllChildren()[m_nHitComponent];
					fragTypeGroup* pGroup = pPhysics->GetAllGroups()[pChild->GetOwnerGroupPointerIndex()];
					fBreakingForce *= pGroup->GetVehicleScale();
				}

				CVehicle *pParentVehicle = NULL;

				if(pVehCollider->GetInstance() && CPhysics::GetEntityFromInst(pVehCollider->GetInstance()) && CPhysics::GetEntityFromInst(pVehCollider->GetInstance())->GetIsTypeVehicle())
				{
					pParentVehicle = (CVehicle *)CPhysics::GetEntityFromInst(pVehCollider->GetInstance());
				}

				static dev_float sfHeliDamageVelocity = 5.0f*5.0f;
				static dev_float sfStationarySpeedThreasholdSqr = 1.0f;
				float parentVehicleVelocityMag2 = pParentVehicle->GetVelocity().Mag2();
				if( pParentVehicle &&
					( !MI_BOAT_TUG.IsValid() || m_pHitPhysical->GetModelIndex() != MI_BOAT_TUG ) &&
					( !pParentVehicle->InheritsFromHeli() ||
					  ( parentVehicleVelocityMag2 > sfHeliDamageVelocity ) ) ) //stop heli's damaging vehicles when still, it looks weird.
				{
#if __DEV
					if(CVehicle::ms_nVehicleDebug == VEH_DEBUG_DAMAGE)
					{
						static dev_float drawForceScale = (0.002f);
						static dev_u32 framesToDraw = 1;
						static dev_float sphereRadius = 0.02f;
						Vec3V forceEnd = VECTOR3_TO_VEC3V(vecHitPosition + (vecApplyForce*drawForceScale));
						CPhysics::ms_debugDrawStore.AddSphere(RCC_VEC3V(vecHitPosition), sphereRadius, Color_LightSeaGreen, (fwTimer::GetTimeStepInMilliseconds()*framesToDraw));
						CPhysics::ms_debugDrawStore.AddLine(RCC_VEC3V(vecHitPosition), forceEnd, Color_green, (fwTimer::GetTimeStepInMilliseconds()*framesToDraw));
					}
#endif

					bool bApplyWheelForce = true;

					if( m_pHitPhysical->GetType() == ENTITY_TYPE_VEHICLE)
					{
						CVehicle *pVehicle = static_cast<CVehicle*>(m_pHitPhysical.Get());

						// Do not apply wheel force to trailer, as it will cause the trailer to wobble out of control
						if( pVehicle->InheritsFromTrailer() || pVehicle->InheritsFromBike() || pVehicle->HasRamp() || pVehicle->InheritsFromSubmarine())
						{
							bApplyWheelForce = false;
						}

						// NOTE: B*7770589  it appears "boattrailer"  need apply wheel force
						static atHashString const boatTrailerHash = ATSTRINGHASH("BOATTRAILER", 0x1F3D44B5);
						if (pVehicle->GetModelNameHash() == boatTrailerHash)
						{
							bApplyWheelForce = true;
						}

						Vector3 normalisedForce(vecApplyForce);
						normalisedForce.NormalizeFast();

						static dev_float WHEEL_DAMAGE_VEHICLE_DEFORM_MULT = 0.85f;
						static dev_float TRACK_DAMAGE_VEHICLE_DEFORM_MULT = 1.8f;
						static dev_float STATIONARY_VEHICLE_DEFORM_MULT = 0.1f;

						float fDamageMult = WHEEL_DAMAGE_VEHICLE_DEFORM_MULT;
						CEntity* inflicter = NULL;

						if( pParentVehicle->GetIncreaseWheelCrushDamage() )
						{
							fDamageMult = 0.0f;

							if( parentVehicleVelocityMag2 > sfStationarySpeedThreasholdSqr )
							{
								fDamageMult = increaseCrushDamageMult;
								inflicter = pParentVehicle;

								float relativeVelocityMag2 = ( pParentVehicle->GetVelocity() - pVehicle->GetVelocity() ).Mag2();

								fDamageMult *= Min( 1.0f, relativeVelocityMag2 / relativeVelocitySquaredForFullCrushDamage );
							}
						}

						if( fDamageMult > 0.0f )
						{
							pVehicle->GetVehicleDamage()->ApplyDamage( inflicter, DAMAGE_TYPE_COLLISION, WEAPONTYPE_RAMMEDBYVEHICLE,
																	   m_vecSolverForce.Mag()*fDamageMult*2.0f*timeStep.Getf(), vecHitPosition, -normalisedForce,
																	   -normalisedForce, m_nHitComponent, 0, -1, false, true, 0.0f, false, false, false, false, pParentVehicle->GetIncreaseWheelCrushDamage() );

							float fDeformationMult = WHEEL_DAMAGE_VEHICLE_DEFORM_MULT;

							if( pParentVehicle->pHandling->mFlags & MF_HAS_TRACKS ||
								( pParentVehicle->GetVehicleModelInfo() && pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_CRUSHES_OTHER_VEHICLES ) ) )
							{
								fDeformationMult = TRACK_DAMAGE_VEHICLE_DEFORM_MULT;
							}

							if( !pParentVehicle->GetDriver() && parentVehicleVelocityMag2 < sfStationarySpeedThreasholdSqr )
							{
								fDeformationMult *= STATIONARY_VEHICLE_DEFORM_MULT;
							}

							pVehicle->GetVehicleDamage()->GetDeformation()->ApplyCollisionImpact( -m_vecSolverForce*fDeformationMult*2.0f*timeStep.Getf(), vecHitPosition, NULL );
						}
					}
					
					if(bApplyWheelForce)
					{
						// B*1788622: Prevent the bulldozer from applying a huge downward force onto vehicles it is driving over.
						CVehicle* pMyVehicle = (CVehicle*)(pVehCollider->GetInstance()->GetUserData());
						if(pMyVehicle->GetModelIndex() == MI_CAR_BULLDOZER)
							vecApplyForce *= 0.5f;

						if( m_pHitPhysical->GetIsTypeObject() &&
							m_pPrevHitPhysical == m_pHitPhysical )
						{
							CObject* pObject = static_cast<CObject*>( m_pHitPhysical.Get() );

							if( pObject->IsObjectAPressurePlate() &&
								pObject->GetFragInst() &&
								pObject->GetFragInst()->GetCacheEntry() &&
								pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider )
							{
								phArticulatedCollider* pArticulatedCollider = pObject->GetFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider;

								if( pArticulatedCollider->GetLinkFromComponent( m_nHitComponent ) > 0 )
								{
									pObject->SetDriveToTarget( false );
								}
								bApplyWheelForce = false;
							}
						}

						if( bApplyWheelForce )
						{
							m_pHitPhysical->ApplyExternalForce( -vecApplyForce, vecOffsetPosition, m_nHitComponent, 0, NULL, fBreakingForce, timeStep.Getf() ASSERT_ONLY( , true ) );
						}
					}
				}

            }
		}
	}

	//clear these for the next frame.
	m_pHitPed = NULL;
	CPhysical *pHitPhysical = m_pHitPhysical;
	m_pPrevHitPhysical = pHitPhysical;
	m_pHitPhysical = NULL;
}

void CWheel::ApplyTireForceToPed(CPed *pPed, phCollider *pVehCollider, u16 hitComponent)
{
	phArticulatedBody *body = pPed->GetRagdollInst()->GetArticulatedBody();
	Assert(body);

	// Check that the cached hitComponent is still valid and that we're a human
	if (hitComponent >= body->GetNumBodyParts() || pPed->GetRagdollInst()->GetARTAssetID() == -1)
	{
		return;
	}

	// Note that the ped is in contact with the wheel
	pPed->GetRagdollInst()->SetContactedWheel(true);

	// Check if this is the first contact with part of the spine. 
	static u32 applyTwitchTime = 300;
	u32 timeSinceTwitchStarted = fwTimer::GetTimeInMilliseconds() - pPed->GetRagdollInst()->GetTimeTireImpulseWasApplied();
	bool bApplyTwitch = timeSinceTwitchStarted <= applyTwitchTime || 
		(pPed->GetRagdollInst()->GetWheelTwitchStarted() && pPed->GetRagdollInst()->GetTireImpulseAppliedRatio() < 0.0f);
	eAnimBoneTag boneTag;
	if (!bApplyTwitch && !pPed->GetRagdollInst()->GetWheelTwitchStarted())
	{
		boneTag = pPed->GetBoneTagFromRagdollComponent(hitComponent);
		if (boneTag == BONETAG_PELVISROOT || boneTag == BONETAG_PELVIS || boneTag == BONETAG_PELVIS1 || boneTag == BONETAG_SPINE_ROOT || 
			boneTag == BONETAG_SPINE0 || boneTag == BONETAG_SPINE1 || boneTag == BONETAG_SPINE2 || boneTag == BONETAG_SPINE3 ||
			boneTag == BONETAG_R_CLAVICLE || boneTag == BONETAG_L_CLAVICLE || boneTag == BONETAG_R_THIGH || boneTag == BONETAG_L_THIGH)
		{
			bApplyTwitch = true;

			// Init wheel impulse tracking params
			pPed->GetRagdollInst()->SetTimeTireImpulseWasApplied(fwTimer::GetTimeInMilliseconds());
			pPed->GetRagdollInst()->SetTireImpulseAppliedRatio(0.0f);
			pPed->GetRagdollInst()->SetWheelTwitchStarted(true);
			timeSinceTwitchStarted = 0;
		}
	}

	// Spread the impulses over a short period of time
	float integrateMult = 0.0f;
	if (bApplyTwitch)
	{
		if (timeSinceTwitchStarted > applyTwitchTime)
		{
			integrateMult = 1.0f - pPed->GetRagdollInst()->GetTireImpulseAppliedRatio();
		}
		else
		{
			integrateMult = (1.0f - (float)(applyTwitchTime - timeSinceTwitchStarted) / (float) applyTwitchTime)
				- pPed->GetRagdollInst()->GetTireImpulseAppliedRatio();
		}
		pPed->GetRagdollInst()->SetTireImpulseAppliedRatio(integrateMult + pPed->GetRagdollInst()->GetTireImpulseAppliedRatio());
	}

	// If the vehicle is moving quickly, apply a slight impulse in the direction of the vehicle velocity, otherwise
	// apply a slight downward force to the ped an upward jolt
	float fMinSpeedForPush = CTaskNMBehaviour::GetWheelMinSpeedForPush();
	float vehSpeedSq = 0.0f;
	if (pVehCollider)
	{
		vehSpeedSq = MagSquared(pVehCollider->GetVelocity()).Getf();
	}

	if (vehSpeedSq < fMinSpeedForPush*fMinSpeedForPush)  // Slow vehicle
	{
		// Apply a downward force to the hit component
		//static float sfDownForceMag = 0.0f;
		//pPed->ApplyForce(-Vector3(ZAXIS) * sfDownForceMag, Vector3(Vector3::ZeroType), hitComponent);

		// Apply an upward jolt to the limbs over a shot period of time
		if (bApplyTwitch && body)
		{
			float fImpulseMultLimbs = CTaskNMBehaviour::GetWheelImpulseMultLimbs();
			float fImpulseMultSpine = CTaskNMBehaviour::GetWheelImpulseMultSpine();
			for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
			{
				boneTag = pPed->GetBoneTagFromRagdollComponent(iLink);
				if (boneTag == BONETAG_PELVISROOT || boneTag == BONETAG_PELVIS || boneTag == BONETAG_PELVIS1 || boneTag == BONETAG_SPINE_ROOT || 
					boneTag == BONETAG_SPINE0 || boneTag == BONETAG_SPINE1 || boneTag == BONETAG_SPINE2 || boneTag == BONETAG_SPINE3)
				{
					pPed->ApplyImpulse(Vector3(ZAXIS) * integrateMult * fImpulseMultSpine * body->GetMass(iLink).Getf(), Vector3(Vector3::ZeroType), iLink);
				}
				else if (iLink != hitComponent)
				{
					pPed->ApplyImpulse(Vector3(ZAXIS) * integrateMult * fImpulseMultLimbs * body->GetMass(iLink).Getf(), Vector3(Vector3::ZeroType), iLink);
				}
			}
		}
	}
	else // Fast vehicle
	{
		// Apply a slight impulse in the direction of the vehicle velocity
		if (bApplyTwitch && body)
		{
			float fFastCarPushImpulseMult = CTaskNMBehaviour::GetWheelFastCarPushImpulseMult();
			Vector3 vecVehDir = RCC_VECTOR3(pVehCollider->GetVelocity());
			vecVehDir.Normalize();
			for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
			{
				boneTag = pPed->GetBoneTagFromRagdollComponent(iLink);
				if (boneTag == BONETAG_PELVISROOT || boneTag == BONETAG_PELVIS || boneTag == BONETAG_PELVIS1 || boneTag == BONETAG_SPINE_ROOT || 
					boneTag == BONETAG_SPINE0 || boneTag == BONETAG_SPINE1 || boneTag == BONETAG_SPINE2 || boneTag == BONETAG_SPINE3)
				{
					pPed->ApplyImpulse(vecVehDir * integrateMult * fFastCarPushImpulseMult * body->GetMass(iLink).Getf(), Vector3(Vector3::ZeroType), iLink);
				}
			}
		}
	}
}

#endif // !__SPU


//
//
dev_float WHEEL_ANG_VEL_OVERRUN = 1.1f;
dev_float WHEEL_SPRING_DRAG_DECAY = 0.9f;
dev_float sfWheelRollingSpinFrac = 0.1f;
dev_float sfSpringSidePushDotTrigger = 0.7f;
dev_float sfSpringSidePushZTrigger = 0.7f;
dev_float sfAbsActiveSlipRatio = 5.0f;

eHierarchyId aOppositeWheelId[ 10 ] =
{
	VEH_WHEEL_RF,	//VEH_WHEEL_LF,
	VEH_WHEEL_LF,	//VEH_WHEEL_RF,
	VEH_WHEEL_RR,	//VEH_WHEEL_LR,
	VEH_WHEEL_LR,	//VEH_WHEEL_RR,
	VEH_WHEEL_RM1,	//VEH_WHEEL_LM1,
	VEH_WHEEL_LM1,	//VEH_WHEEL_RM1,
	VEH_WHEEL_RM2,	//VEH_WHEEL_LM2,
	VEH_WHEEL_LM2,	//VEH_WHEEL_RM2,
	VEH_WHEEL_RM3,	//VEH_WHEEL_LM3,
	VEH_WHEEL_LM3,	//VEH_WHEEL_RM3,
};

//
void CWheel::IntegrationPostProcess(phCollider* pVehCollider, float fWheelAngSlip, float fMaxVelInGear, int nGear, float fTimeStep, unsigned frameCount, Vector3& vecPushToSide, Vector3& vecFwd, Vector3& vecSide, const float fGravity)
{
	float fRotAngVelOld = m_fRotAngVel;
	Matrix34 matOriginal = RCC_MATRIX34(pVehCollider->GetMatrix());

	float fSteerAngle = m_fSteerAngle;
	float fDriveForce = m_fDriveForce * pVehCollider->GetMass() * fGravity;
	float fBrakeForce = m_fBrakeForce * pVehCollider->GetMass() * fGravity;
	float fWheelRadius = m_fWheelRadius > 0.0f ? m_fWheelRadius : 1.0f;
	float fMaxAngVelInGear = -fMaxVelInGear / fWheelRadius;

	// this is a bit of a hack, really want to apply v small friction force all the time
	const float fFrictionMult = (m_pHandling->hFlags & HF_FREEWHEEL_NO_GAS) ? sfFrictionFreeWheel : sfFrictionStd;
	if(!GetDynamicFlags().IsFlagSet(WF_ON_GAS) || m_fFrictionDamage > sfFrictionDamageStart)
	{
		if(GetIsTouching())
		{
			fBrakeForce += rage::Max(fFrictionMult, GetFrictionDamageScaledBySpeed(VEC3V_TO_VECTOR3(pVehCollider->GetVelocity()))) * pVehCollider->GetMass() * fGravity;
		}
		else
		{
			if( !m_nConfigFlags.IsFlagSet(WCF_AMPHIBIOUS_WHEEL) || !m_bIsWheelInWater )
			{
				float fInAirFriction = GetConfigFlags().IsFlagSet(WCF_POWERED) ? sfFrictionStdInAirDriven : sfFrictionStdInAir;
				fBrakeForce += fInAirFriction;
			}
		}
	}

	// drive force gets canceled out my braking force
	if(fBrakeForce > rage::Abs(fDriveForce))
		fDriveForce = 0.0f;
	else
	{
		if(fDriveForce > fBrakeForce)
			fDriveForce -= fBrakeForce;
		else
			fDriveForce += fBrakeForce;
		fBrakeForce = 0.0f;
	}

	// calc local speed at contact point
	Vector3 vecCurrentLocalVel;
	vecCurrentLocalVel = VEC3V_TO_VECTOR3(pVehCollider->GetLocalVelocity(m_vecHitCentrePos));
	vecCurrentLocalVel -= m_vecGroundVelocity;

	bool clampRotAngVel = false;

	if(GetIsTouching())
	{
		// calc fwd and side vectors for traction
		Vector3 vecHitDelta;

		// calc local speed at contact point
		if( sbUseNewTractionVectors || GetConfigFlags().IsFlagSet( WCF_CENTRE_WHEEL ) )
		{
			CalculateTractionVectorsNew( RCC_MATRIX34(pVehCollider->GetMatrix()), vecFwd, vecSide, vecHitDelta, fSteerAngle, vecCurrentLocalVel );
		}
		else
		{
			CalculateTractionVectors(RCC_MATRIX34(pVehCollider->GetMatrix()), vecFwd, vecSide, vecHitDelta, fSteerAngle);
		}
		

		// calculate speed for damping
		float fSpeedAlongFwd = vecCurrentLocalVel.Dot(vecFwd);
		float fSpeedAlongSide = vecCurrentLocalVel.Dot(vecSide);

		// calculate spring delta's
		float fSpringAlongSide = 0.0f;

		if(m_pHandling->m_fTractionSpringDeltaMax > 0.0f && m_vecGroundVelocity.IsZero())// when on another vehicle the wheel would move around a bit with the traction spring on. If we see oscillation I will have to fix this
		{
			bool bDraggingHitPoint = false;

			if(m_vecHitCentrePosOld.IsZero())
				vecHitDelta.Zero();
			else
				vecHitDelta = m_vecHitCentrePosOld - m_vecHitCentrePos;

			// if wheels should be rolling, then get rid of fwd component of spring
			float fInvGravity = 1.0f/fGravity;
			if(fBrakeForce == 0.0f || (rage::Abs(fBrakeForce) * pVehCollider->GetInvMass() * fInvGravity) < rage::Abs(fSpeedAlongFwd))
			{
				float fFwdDelta = vecHitDelta.Dot(vecFwd);
				vecHitDelta -= fFwdDelta * vecFwd;
				bDraggingHitPoint = true;
			}

			// limit length of spring / check if we're dragging it along
			if(vecHitDelta.Mag2() > m_pHandling->m_fTractionSpringDeltaMax*m_pHandling->m_fTractionSpringDeltaMax)
			{
				vecHitDelta.NormalizeFast();
				vecHitDelta *= m_pHandling->m_fTractionSpringDeltaMax;
				bDraggingHitPoint = true;
			}

			if(bDraggingHitPoint && vecCurrentLocalVel.Mag2() > 1.0f)
			{
				float fDragDecay = rage::Powf(WHEEL_SPRING_DRAG_DECAY, rage::Abs(RCC_MATRIX34(pVehCollider->GetMatrix()).b.Dot(vecCurrentLocalVel)) * fTimeStep);
				vecHitDelta *= fDragDecay;
			}

			fSpringAlongSide = vecHitDelta.Dot(vecSide);

			// save spring contact stuff for next update
			if(m_vecHitCentrePosOld.IsZero())
				m_vecHitCentrePosOld = m_vecHitCentrePos;
			else if(bDraggingHitPoint)
				m_vecHitCentrePosOld = m_vecHitCentrePos + vecHitDelta;


			if(vecCurrentLocalVel.Mag2() < 1.0f && rage::Abs(m_vecHitNormal.Dot(RCC_VECTOR3(pVehCollider->GetMatrix().GetCol3ConstRef()))) > sfSpringSidePushDotTrigger && m_vecHitNormal.z < sfSpringSidePushZTrigger)
			{
				vecPushToSide = m_vecHitNormal;
				PushToSide(pVehCollider, m_vecHitNormal);
			}
		}

		// use gears here so that we can wheel spin when rolling backwards down a hill
		float fSpeedSign = nGear > 0 ? 1.0f : -1.0f; //fSpeedAlongFwd > 0.0f ? 1.0f : -1.0f;

		// If we're braking then use old speed along fwd method
		if(fBrakeForce > 0.0f)
		{
			fSpeedSign = fSpeedAlongFwd > 0.0f ? 1.0f : -1.0f;
		}

		if(!GetConfigFlags().IsFlagSet(WCF_BICYCLE_WHEEL) && GetDynamicFlags().IsFlagSet(WF_ABS) && GetDynamicFlags().IsFlagSet(WF_ABS_ACTIVE) && (frameCount&3)==0)
        {
            static dev_bool clampAbsSlip = false;

            if( clampAbsSlip )
            {
			    fWheelAngSlip = Clamp( fWheelAngSlip, sfAbsActiveSlipRatio*-1.0f, sfAbsActiveSlipRatio );
            }
            else
            {
                fWheelAngSlip = sfAbsActiveSlipRatio * fSpeedSign;
            }
        }
		m_fFwdSlipAngle = fWheelAngSlip * sfTractionMinAngle;
		m_fRotSlipRatio = fWheelAngSlip;

		float fCurrentFwdSpeed = vecCurrentLocalVel.Dot(vecFwd);

		if( !m_bDisableWheelForces ||
			!GetConfigFlags().IsFlagSet( WCF_BIKE_WHEEL ) )
		{
			m_fRotAngVel = -fCurrentFwdSpeed / fWheelRadius;
		}

        Assert( m_fRotAngVel == m_fRotAngVel );

		// accelerating in direction of travel (wheel spin)
		if(fSpeedSign * m_fRotSlipRatio < -0.9f && fSpeedSign * m_fDriveForce > 0.0f)
		{
			clampRotAngVel = true;

            if( m_pHandling->GetCarHandlingData() &&
                m_pHandling->GetCarHandlingData()->aFlags & CF_BLOCK_INCREASED_ROT_VELOCITY_WITH_DRIVE_FORCE &&
				( !m_pParentVehicle->InheritsFromAutomobile() ||
				  !static_cast< CAutomobile* >( m_pParentVehicle )->IsInBurnout() ) )
            {
				static dev_float sf_maxSpeedForSpeedIncrease = 10.0f;

				float speedFactor = 1.0f - Min( 1.0f, Abs( fSpeedAlongFwd ) / sf_maxSpeedForSpeedIncrease );

				if( fSpeedSign > 0.0f )
				{
					m_fRotAngVel = rage::Min( m_fRotAngVel, fRotAngVelOld - m_fDriveForce * sfInvInertiaForIntegratorAccel * fTimeStep * speedFactor );
				}
				else
				{
					m_fRotAngVel = rage::Max( m_fRotAngVel, fRotAngVelOld - m_fDriveForce * sfInvInertiaForIntegratorAccel * fTimeStep * speedFactor );
				}
            }
            else
            {
                if(fSpeedSign > 0.0f)
                {
                    m_fRotAngVel = rage::Min(m_fRotAngVel, fRotAngVelOld - m_fDriveForce * sfInvInertiaForIntegratorAccel * fTimeStep);
                }
                else
                {
                    m_fRotAngVel = rage::Max(m_fRotAngVel, fRotAngVelOld - m_fDriveForce * sfInvInertiaForIntegratorAccel * fTimeStep);
                }
            }
		}
		// braking (lock up)
		else if(fSpeedSign * m_fRotSlipRatio > 0.9f)
		{
			m_fRotAngVel = (1.0f - rage::Min(1.0f, fSpeedSign*m_fRotSlipRatio)) * -fCurrentFwdSpeed / fWheelRadius;

			if(GetDynamicFlags().IsFlagSet(WF_ABS_ALT) && rage::Abs(fSpeedAlongFwd) > sfBrakeAbsMinSpeed && m_fFrictionDamage < sfFrictionDamageStart)
			{
				if(fSpeedSign * m_fRotSlipRatio > sfBrakeAbsLimit)
					m_fAbsTimer = sfBrakeAbsTimer;
			}
		}
		// just rolling
		else
		{
			bool bHasHoldGearWheelSpinFlag = CVehicle::HasHoldGearWheelSpinFlag( m_pHandling, m_pParentVehicle );
			if( !bHasHoldGearWheelSpinFlag
#if __BANK
				|| CVehicle::ms_bDebugIgnoreHoldGearWithWheelspinFlag
#endif
				)
			{
				m_fRotAngVel = ( 1.0f - sfWheelRollingSpinFrac*fSpeedSign*m_fRotSlipRatio ) * -fCurrentFwdSpeed / fWheelRadius;
			}
			else
			{
				m_fRotAngVel = ( 1.0f - fSpeedSign*m_fRotSlipRatio ) * -fCurrentFwdSpeed / fWheelRadius;
			}
		}

		// Store some values for the effects to use
		m_vecGroundContactVelocity = vecCurrentLocalVel;
        m_vecTyreContactVelocity = m_fRotAngVel*fWheelRadius*vecFwd;

		//////////////////////////
		// Now do side traction
		//////////////////////////
		m_fSideSlipAngle = 0.0f;
		if(rage::Abs(fSpeedAlongFwd) > 0.1f)
			m_fSideSlipAngle = rage::Atan2f(-fSpeedAlongSide, rage::Abs(fSpeedAlongFwd));
		if(rage::Abs(fSpringAlongSide) > 0.0f)
			m_fSideSlipAngle += rage::Atan2f(fSpringAlongSide, fWheelRadius);

		m_fSideSlipAngle *= m_pHandling->m_fTractionCurveLateral_Inv;
	}
	else
	{
		if(fDriveForce)
		{
			m_fRotAngVel += -fDriveForce * fTimeStep * sfInvInertiaForIntegratorAccelInAir;
			if(fMaxAngVelInGear > 0.0f && m_fRotAngVel > fMaxAngVelInGear)
				m_fRotAngVel = fMaxAngVelInGear;
			else if(fMaxAngVelInGear < 0.0f && m_fRotAngVel < fMaxAngVelInGear)
				m_fRotAngVel = fMaxAngVelInGear;
		}
		else if(fBrakeForce > 0.0f)
		{
			if(fBrakeForce * sfInvInertiaForIntegratorBrakeInAir * fTimeStep > rage::Abs(m_fRotAngVel))
				m_fRotAngVel = 0.0f;
			else if(m_fRotAngVel > 0.0f)
				m_fRotAngVel -= fBrakeForce * fTimeStep * sfInvInertiaForIntegratorBrakeInAir;
			else
				m_fRotAngVel += fBrakeForce * fTimeStep * sfInvInertiaForIntegratorBrakeInAir;
		}

		//if( !m_pHandling->GetCarHandlingData() ||
		//	!( m_pHandling->GetCarHandlingData()->aFlags & CF_HOLD_GEAR_WITH_WHEELSPIN ) )
		{
			m_fRotSlipRatio = 0.0f;
		}
		//else
		//{
		//	m_fRotSlipRatio = -1.0f;
		//}

		m_vecHitCentrePosOld.Zero();
		m_fEffectiveSlipAngle = 0.0f;
        m_fTyreLoad = 0.0f;
		m_fTyreCamberForce = 0.0f;
		m_fFwdSlipAngle = 0.0f;
		m_fSideSlipAngle = 0.0f;
		m_fExtraSinkDepth = 0.0f;

	}

	if( clampRotAngVel ||
		( m_pHandling->GetCarHandlingData() &&
		  m_pHandling->GetCarHandlingData()->aFlags & CF_ALLOW_TURN_ON_SPOT ) )
	{
		bool bUseHardRevLimitFlag = CVehicle::UseHardRevLimitFlag( m_pHandling, m_pParentVehicle->IsTunerVehicle(), m_pParentVehicle->m_Transmission.IsCurrentGearATopGear() );
		bool inWheelieMode = m_pParentVehicle->InheritsFromAutomobile() && static_cast<CAutomobile*>( m_pParentVehicle )->m_nAutomobileFlags.bInWheelieMode;

		if( m_pHandling->GetCarHandlingData() &&
			( ( GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
				m_pHandling->GetCarHandlingData()->aFlags & CF_DIFF_REAR ) ||
				( !GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
				  m_pHandling->GetCarHandlingData()->aFlags & CF_DIFF_FRONT ) ) )
		{
			// if we have a differential a wheel should be able to spin twice as fast as the max speed in gear as the other wheel could be stopped
			// We'll handle this in the differential code so that we know the final wheel speed of both wheels.

			eHierarchyId nOpposingWheelId = aOppositeWheelId[ m_WheelId - VEH_WHEEL_LF ];
			CWheel* pOtherWheel = m_pParentVehicle->GetWheelFromId( nOpposingWheelId );
			
			if( pOtherWheel )
			{
				float averageWheelSpeed = ( m_fRotAngVel + pOtherWheel->GetRotSpeed() ) * 0.5f;
				float maxWheelSpeed = fMaxAngVelInGear * WHEEL_ANG_VEL_OVERRUN;

				float wheelSpeedScale = Clamp( maxWheelSpeed / averageWheelSpeed, 0.0f, 1.0f );

				if( wheelSpeedScale < 1.0f )
				{
					m_fRotAngVel *= wheelSpeedScale;
					m_vecTyreContactVelocity = m_fRotAngVel*fWheelRadius*vecFwd;
					pOtherWheel->SetRotSpeed( pOtherWheel->GetRotSpeed() * wheelSpeedScale );
					pOtherWheel->ScaleTyreContactVelocity( wheelSpeedScale );

				}
			}
			else
			{
				Assertf( pOtherWheel, "There is a differential on this axle but only 1 wheel, that seems wrong" );
				m_fRotAngVel = Clamp( m_fRotAngVel, -Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN*4.0f ), Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN*4.0f ) );
				m_vecTyreContactVelocity = m_fRotAngVel*fWheelRadius*vecFwd;
			}
		}
		else if( bUseHardRevLimitFlag ||
				 inWheelieMode )
		{
			// if we have a hard rev limit we want to make sure we drop the actual wheel speed below the rev limit before we apply power again.
			// only overreving by 10% doesn't seem to make this happen
			m_fRotAngVel = Clamp( m_fRotAngVel, -Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN*2.0f ), Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN*2.0f ) );
			m_vecTyreContactVelocity = m_fRotAngVel*fWheelRadius*vecFwd;
		}
		else
		{
			m_fRotAngVel = Clamp( m_fRotAngVel, -Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN ), Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN ) );
			m_vecTyreContactVelocity = m_fRotAngVel*fWheelRadius*vecFwd;
		}
	}

	ProcessTyreTemp( VEC3V_TO_VECTOR3( pVehCollider->GetVelocity() ), fTimeStep );
	
	// rotate wheel
    if(m_bUpdateWheelRotation)
    {
        m_fRotAng += m_fRotAngVel * fTimeStep;
        m_fRotAng = fwAngle::LimitRadianAngleFast(m_fRotAng);
    }

	// store hit flag of previous step
	GetDynamicFlags().ClearFlag(WF_HIT_PREV);
	if(GetDynamicFlags().IsFlagSet(WF_HIT))
		GetDynamicFlags().SetFlag(WF_HIT_PREV);

	// clear hit flag before we do probes again
	GetDynamicFlags().ClearFlag(WF_HIT);
	GetDynamicFlags().ClearFlag(WF_SIDE_IMPACT);

    TUNE_GROUP_BOOL( VEHICLE_TYRE_WEAR, OVERRIDE_TYRE_WEAR, false );
    TUNE_GROUP_FLOAT( VEHICLE_TYRE_WEAR, OVERRIDE_TYRE_WEAR_RATE, 1.0f, 0.0f, 1000.0f, 0.0001f );

    if( OVERRIDE_TYRE_WEAR )
    {
        m_fTyreWearRate = OVERRIDE_TYRE_WEAR_RATE;
    }

    if( m_fTyreWearRate > 0.0f &&
        m_fTyreHealth > TYRE_HEALTH_FLAT )
    {
        UpdateTyreWear( fTimeStep );
    }
}

static dev_float sfFrictionDamageValueToStartHeatingUpTyres = 0.2f;
static dev_float sfVelocityToReduceCoolingOfTyres = 2.0f * 2.0f;
void CWheel::ProcessTyreTemp(const Vector3& vecVelocity, float fTimeStep)
{
	// tyres heat up when spinning (on specific surfaces, and not when burst) 
	if((GetDynamicFlags().IsFlagSet(WF_TYRES_HEAT_UP) || (m_fFrictionDamage > sfFrictionDamageValueToStartHeatingUpTyres && m_fTyreHealth > 0.0f)) && !GetDynamicFlags().IsFlagSet(WF_BURNOUT_NON_DRIVEN_WHEEL) ) // && (GetDynamicFlags().IsFlagSet(WF_BURNOUT)
	{
		m_fTyreTemp += ms_fTyreTempHeatMult * fTimeStep * rage::Abs(m_fRotSlipRatio); 
	}

	float fTyreTempCoolMult = ms_fTyreTempCoolMovingMult;
	if((GetDynamicFlags().IsFlagSet(WF_BURNOUT) || ((vecVelocity.Mag2() < sfVelocityToReduceCoolingOfTyres) && GetDynamicFlags().IsFlagSet(WF_ON_GAS))) )
	{
		fTyreTempCoolMult = ms_fTyreTempCoolMult;
	}

	// Tyres always cooling to ambient temp of 0
	float fCoolRate = Max(m_fTyreTemp, 0.1f) * fTyreTempCoolMult;
	m_fTyreTemp -= fCoolRate * fTimeStep;

	// clamp tyre temp
	m_fTyreTemp = rage::Clamp(m_fTyreTemp, 0.0f, ms_fTyreTempBurstThreshold);
}

void CWheel::UpdateTyreWear( float fTimeStep )
{
    if( m_fEffectiveSlipAngle > 0.5f || 
        m_fTyreHealth <= TYRE_HEALTH_FLAT )
    {
        float normalisedTyreLoad = m_fTyreLoad / ( m_pParentVehicle->GetMass() * m_fStaticForce );
        float fTyreWear = m_fEffectiveSlipAngle * normalisedTyreLoad * m_fTyreWearRate * m_fWearRateScale;
        fTyreWear *= fTimeStep;
        m_fTyreHealth = Max( 0.0f, m_fTyreHealth - fTyreWear );
    }
}

void Wi_END() {;}

#if !__SPU

//float CWheel::GetMassAlongVector(phCollider* pVehCollider, const Vector3& vecNormal, const Vector3& vecWorldPos)
//{
//	Vector3 vecCgOffset = vecWorldPos - RCC_VECTOR3(pVehCollider->GetPosition());
//	Vector3 vecTemp = vecCgOffset;
//	vecTemp.Cross(vecNormal);
//	//	vecTemp.Multiply(pVehCollider->GetInvAngInertia());
//
//	fragInst* pFragInst = dynamic_cast<fragInst*>(pVehCollider->GetInstance());
//	if(pFragInst)
//		vecTemp.Multiply(pFragInst->GetTypePhysics()->GetArchetype()->GetInvAngInertia());
//	else
//		vecTemp.Multiply(RCC_VECTOR3(pVehCollider->GetInvAngInertia()));
//
//	// if bike is constrained about the y (roll) axis, then ang inertia is effectively infinite about that axis
//	if(GetConfigFlags().IsFlagSet(WCF_BIKE_CONSTRAINED_COLLIDER))
//		vecTemp.y = 0.0f;
//
//	vecTemp.Cross(vecCgOffset);
//
//	float fMassRot = vecTemp.Dot(vecNormal);
//	fMassRot = 1.0f / (pVehCollider->GetInvMass() + fMassRot);
//
//	return fMassRot;
//}

#endif

#endif //#if VEHICLE_USE_INTEGRATION_FOR_WHEELS

void CWheel::UpdateContactsAfterNetworkBlend(const Matrix34& matOld, const Matrix34& matNew)
{
	if(GetIsTouching() || GetWasTouching())
	{
		Vector3 vecLocalHitPos;
		matOld.UnTransform(m_vecHitPos, vecLocalHitPos);
		matNew.Transform(vecLocalHitPos, m_vecHitPos);

		matOld.UnTransform(m_vecHitCentrePos, vecLocalHitPos);
		matNew.Transform(vecLocalHitPos, m_vecHitCentrePos);

		matOld.UnTransform(m_vecHitCentrePosOld, vecLocalHitPos);
		matNew.Transform(vecLocalHitPos, m_vecHitCentrePosOld);
	}
}

void CWheel::UpdateCompressionInsideIntegrator(const Matrix34& matUpdated, const phCollider* pCollider, float& compressionDelta)
{

	// Need to convert our CG orientated matrix to a instance one for the shapetest
	Matrix34 matInstanceUpdated = matUpdated;

	// Assume handling data same for all wheels here
	Vector3 vCgOffset = RCC_VECTOR3(m_pHandling->m_vecCentreOfMassOffset);


	matInstanceUpdated.Transform3x3(vCgOffset);
	matInstanceUpdated.d -= vCgOffset;	

	{
		if(GetDynamicFlags().IsFlagSet(WF_HIT))
		{
			const Matrix34& matOriginal = RCC_MATRIX34(pCollider->GetMatrix());

			////////////////////
			// use updated matrix to reevaluate compression ratio

			// calculate wheel contact point with updated matrix
			Vector3 vecEstimatedHitPos;
			matOriginal.UnTransform(m_vecHitCentrePos, vecEstimatedHitPos);
			matUpdated.Transform(vecEstimatedHitPos);

			// get diff of how much contact would have moved
			vecEstimatedHitPos = vecEstimatedHitPos - m_vecHitCentrePos;

			// only want to use component of diff along the collision normal
			vecEstimatedHitPos = m_vecHitNormal * m_vecHitNormal.DotV(vecEstimatedHitPos);

			// get component of this along suspension line
			Vector3 vecWheelLine = m_vecSuspensionAxis;
			matUpdated.Transform3x3(vecWheelLine);

			compressionDelta = -vecWheelLine.Dot(vecEstimatedHitPos);
		}
	}
#if !__SPU && __DEV
	static dev_bool bOutputText = false;
	if(bOutputText)
	{
		if(GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) && !GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
		{
			Printf("Compression delta: %f\n",compressionDelta);
		}
	}
#endif

}



//
//
#if !__SPU
void CWheel::ProcessWheelMatrixForBike()
{
	// Bike wheels steer from the forks, and suspension is a special case as well.  Code is in CBike::PreRender().
	Assert(m_pParentVehicle->InheritsFromBike());
	if(!m_pParentVehicle->m_nVehicleFlags.bAnimateWheels)
	{
		int nBoneIndex = m_pParentVehicle->GetBoneIndex(m_WheelId);
		if(nBoneIndex >= 0)
		{
			Matrix34& wheelMatrix = m_pParentVehicle->GetLocalMtxNonConst(nBoneIndex);
			wheelMatrix.Identity3x3();
			wheelMatrix.RotateLocalX(m_fRotAng);
			wheelMatrix.d.Set(RCC_VECTOR3(m_pParentVehicle->GetSkeletonData().GetBoneData(nBoneIndex)->GetDefaultTranslation()));		
		}
	}
}

void CWheel::ProcessWheelMatrixForTrike()
{
	// This is to process the centre wheel in a similar fashion to bikes (i.e. don't translate the wheel)
	if(!m_pParentVehicle->m_nVehicleFlags.bAnimateWheels)
	{
		int nBoneIndex = m_pParentVehicle->GetBoneIndex(m_WheelId);
		if(nBoneIndex >= 0)
		{
			Matrix34& wheelMatrix = m_pParentVehicle->GetLocalMtxNonConst(nBoneIndex);
			wheelMatrix.Identity3x3();
			wheelMatrix.RotateLocalX(m_fRotAng);
			wheelMatrix.d.Set(RCC_VECTOR3(m_pParentVehicle->GetSkeletonData().GetBoneData(nBoneIndex)->GetDefaultTranslation()));		
		}
	}
}

void CWheel::ProcessWheelMatrixForAutomobile(crSkeleton & rSkeleton)
{
	float fSteeringWheelMult = m_pParentVehicle->GetVehicleModelInfo()->GetSteeringWheelMult(m_pParentVehicle->GetDriver());
	ProcessWheelMatrixCore(rSkeleton,
		*m_pParentVehicle->GetVehicleModelInfo()->GetStructure(), 
		m_pParentVehicle->GetWheels(), m_pParentVehicle->GetNumWheels(), 
		m_pParentVehicle->m_nVehicleFlags.bAnimateWheels,
		m_fSteerAngle * fSteeringWheelMult );
}

void CWheel::ProcessAnimatedWheelDeformationForAutomobile(crSkeleton & rSkeleton)
{
	if(m_pParentVehicle->m_nVehicleFlags.bCanDeformWheels && m_pParentVehicle->m_nVehicleFlags.bAnimateWheels)
	{
		ProcessWheelDeformationMatrixCore(rSkeleton, *m_pParentVehicle->GetVehicleModelInfo()->GetStructure(), m_pParentVehicle->m_nVehicleFlags.bAnimateWheels);
	}
}
#endif

#if !__SPU

void CWheel::ProcessVFx(bool UNUSED_PARAM(bContainsLocalPlayer))
{
	if (fwTimer::IsGamePaused())
	{
		return;
	}

	if (m_pParentVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT || m_pParentVehicle->GetIsRotaryAircraft())
	{
		return;
	}

	if (m_pParentVehicle->GetStatus()==STATUS_WRECKED)
	{
		if (GetDynamicFlags().IsFlagSet(WF_ON_FIRE) || GetTyreShouldBurst(m_pParentVehicle->GetVelocity()) )
		{
			ProcessWheelBurst();
		}
		return;
	}

	// when a vehicle is teleported its wheel contact positions aren't teleported with it 
	// this can cause issues when relocating particle effects so we set a flag to stop wheel vfx being updated this frame
	if (GetDynamicFlags().IsFlagSet(WF_TELEPORTED_NO_VFX))
	{
		GetDynamicFlags().ClearFlag(WF_TELEPORTED_NO_VFX);
		return;
	}

	// reject any middle wheels
	eHierarchyId hierarchyId = GetHierarchyId();
	bool isMiddle = hierarchyId==VEH_WHEEL_LM1 ||
					hierarchyId==VEH_WHEEL_LM2 ||
					hierarchyId==VEH_WHEEL_LM3 ||
					hierarchyId==VEH_WHEEL_RM1 ||
					hierarchyId==VEH_WHEEL_RM2 ||
					hierarchyId==VEH_WHEEL_RM3;


	bool dontRejectMiddleWheels = false;
	if ((MI_VAN_RIOT_2.IsValid() && m_pParentVehicle->GetModelIndex()==MI_VAN_RIOT_2) ||
		(MI_CAR_CARACARA.IsValid() && m_pParentVehicle->GetModelIndex()==MI_CAR_CARACARA))
	{
		dontRejectMiddleWheels = true;
	}


	if (isMiddle && dontRejectMiddleWheels==false)
	{
		return;
	}

#if __BANK
	// debug wheel matrix rendering
	g_vfxWheel.RenderDebugWheelMatrices(this, m_pParentVehicle);

	bool isLeft = hierarchyId==VEH_WHEEL_LF || 
				  hierarchyId==VEH_WHEEL_LR ||
				  hierarchyId==VEH_WHEEL_LM1 ||
				  hierarchyId==VEH_WHEEL_LM2 ||
				  hierarchyId==VEH_WHEEL_LM3;

	bool isFront = hierarchyId==VEH_WHEEL_LF || 
				   hierarchyId==VEH_WHEEL_RF;

	bool isRear = hierarchyId==VEH_WHEEL_LR || 
				   hierarchyId==VEH_WHEEL_RR;


	// debug wheel fx disabling
	if (isFront)
	{
		if ((g_vfxWheel.GetDisableWheelFL() && isLeft) ||
			(g_vfxWheel.GetDisableWheelFR() && !isLeft))
		{
			return;
		}
	}
	
	if (isMiddle)
	{
		if ((g_vfxWheel.GetDisableWheelML() && isLeft) ||
			(g_vfxWheel.GetDisableWheelMR() && !isLeft))
		{
			return;
		}
	}

	if (isRear)
	{
		if ((g_vfxWheel.GetDisableWheelRL() && isLeft) ||
			(g_vfxWheel.GetDisableWheelRR() && !isLeft))
		{
			return;
		}
	}
#endif

	// process deep surface info (only if the engine is one and the physics is active)
	if (m_pParentVehicle->IsEngineOn())
	{
		if (m_pParentVehicle->GetCurrentPhysicsInst()->IsInLevel())
		{
			if (CPhysics::GetLevel()->IsActive(m_pParentVehicle->GetCurrentPhysicsInst()->GetLevelIndex()))
			{
				m_deepSurfaceInfo.Process(RCC_VEC3V(m_vecHitCentrePos), RCC_VEC3V(m_vecHitNormal), m_nHitMaterialId, VFXRANGE_DEEP_SURFACE_WHEEL_SQR);
			}
		}
	}

	// check if this is the player vehicle
	float distSqrToCam = CVfxHelper::GetDistSqrToCamera(RCC_VEC3V(m_vecHitCentrePos));

	// get the vehicle fx info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(m_pParentVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// return if not in range of the maximum wheel decal or particle effect range
	float maxDecalRange = VFXRANGE_WHEEL_SKIDMARK * pVfxVehicleInfo->GetWheelGenericRangeMult() * g_vfxWheel.GetWheelSkidmarkRangeScale();
	float maxPtFxRange = Max(VFXRANGE_WHEEL_FRICTION, Max(VFXRANGE_WHEEL_DISPLACEMENT_HI, VFXRANGE_WHEEL_BURNOUT)) * pVfxVehicleInfo->GetWheelGenericRangeMult() * g_vfxWheel.GetWheelPtFxLodRangeScale();
	float maxVfxRange = Max(maxDecalRange, maxPtFxRange);
	float maxVfxRangeSqr = maxVfxRange*maxVfxRange;
	if (distSqrToCam>maxVfxRangeSqr)
	{
		return;
	}

	bool isVehicleAsleep = m_pParentVehicle->IsAsleep() && (!m_pParentVehicle->ContainsPlayer());

#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld() && !CReplayMgr::IsSettingUp() && isVehicleAsleep && m_pParentVehicle->GetVelocity().Mag2() > 0.0f)
		isVehicleAsleep = false;
#endif // GTA_REPLAY

	if ( (GetDynamicFlags().IsFlagSet(WF_HIT_PREV) && !isVehicleAsleep) )
	{
		Vec3V vVfxVecHitPos = RCC_VEC3V(m_vecHitPos);
		Vec3V vVfxVecHitCentrePos = RCC_VEC3V(m_vecHitCentrePos);
		Vec3V vVfxVecHitNormal = RCC_VEC3V(m_vecHitNormal);

		// hack - adjust the position of the contact positions for the f1 cars
		{
			Vec3V vVehicleRight(m_pParentVehicle->GetTransform().GetA());
			Vec3V vOffset = Vec3V(V_ZERO);
			if ((MI_CAR_FORMULA.IsValid()  && m_pParentVehicle->GetModelIndex() == MI_CAR_FORMULA) ||
				(MI_CAR_FORMULA2.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_FORMULA2))	
			{
				float rightOffset = 0.15f;
				if (GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
				{
					rightOffset = 0.20f;
				}
				vOffset = vVehicleRight * ScalarVFromF32(rightOffset);
			}
			else if (MI_CAR_OPENWHEEL1.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_OPENWHEEL1)
			{
				float rightOffset = 0.15f;
				if (GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
				{
					rightOffset = 0.17f;
				}
				vOffset = vVehicleRight * ScalarVFromF32(rightOffset);
			}
			else if (MI_CAR_OPENWHEEL2.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_OPENWHEEL2)
			{
				float rightOffset = 0.15f;
				if (GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
				{
					rightOffset = 0.22f;
				}
				vOffset = vVehicleRight * ScalarVFromF32(rightOffset);
			}
			else if (MI_CAR_VETO.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_VETO)
			{
				float rightOffset = 0.06f;
				vOffset = vVehicleRight * ScalarVFromF32(rightOffset);
			}
			else if (MI_CAR_VETO2.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_VETO2)
			{
				float rightOffset = 0.07f;
				vOffset = vVehicleRight * ScalarVFromF32(rightOffset);
			}

			if (GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
			{
				vVfxVecHitPos -= vOffset;
				vVfxVecHitCentrePos -= vOffset;
			}
			else // Right Wheel
			{
				vVfxVecHitPos += vOffset;
				vVfxVecHitCentrePos += vOffset;
			}
		}


		// check that the wheel contact position is close to the actual wheel
		Vec3V vWheelBonePos;
		float wheelRadius = GetWheelPosAndRadius( RC_VECTOR3(vWheelBonePos));
		Vec3V vWheelDiff = vWheelBonePos - vVfxVecHitPos;
		float wheelDiffDist = Mag(vWheelDiff).Getf();
		if (wheelDiffDist>wheelRadius*2.0f)
		{
			//grcDebugDraw::Sphere(vWheelBonePos, 0.1f, Color32(0.0f, 1.0f, 0.0f, 1.0f), true, -1000);
			//grcDebugDraw::Sphere(vVfxVecHitPos, 0.1f, Color32(1.0f, 0.0f, 0.0f, 1.0f), true, -1000);
			//grcDebugDraw::Line(vWheelBonePos, vVfxVecHitPos, Color32(1.0f, 1.0f, 0.0f, 1.0f), -1000);
			return;
		}

		// check if tyre is in water
		bool inDeepWater = false;
		bool inShallowWater = false;
		float waterZ = 0.0f;
		if (m_pParentVehicle->m_Buoyancy.GetWaterLevelIncludingRivers(RCC_VECTOR3(vVfxVecHitCentrePos), &waterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
		{
			if (waterZ > vVfxVecHitCentrePos.GetZf())
			{
				// wheel is under water
				if (waterZ - vVfxVecHitCentrePos.GetZf()<0.5f)
				{
					// wheel is only just in water - need to produce water effects
					inShallowWater = true;
				}
				else
				{
					inDeepWater = true;
				}
			}
		}

		if (inShallowWater)
		{
			GetDynamicFlags().SetFlag(WF_INSHALLOWWATER);
		}
		else
		{
			GetDynamicFlags().ClearFlag(WF_INSHALLOWWATER);
		}

		if (inDeepWater)
		{
			GetDynamicFlags().SetFlag(WF_INDEEPWATER);
		}
		else
		{
			GetDynamicFlags().ClearFlag(WF_INDEEPWATER);
		}

		// store the collision vfx group
		VfxGroup_e colnVfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(m_nHitMaterialId);

		// check if snow is active
		if (g_vfxWheel.GetUseSnowWheelVfxWhenUnsheltered() && !m_pParentVehicle->GetIsShelteredFromRain())
		{
			colnVfxGroup = VFXGROUP_SNOW_COMPACT;
		}

		float vfxSlipVal = Abs(m_fEffectiveSlipAngle)*sfTractionPeakAngleInv;
		float vfxGroundSpeedVal = m_vecGroundContactVelocity.Mag();

		if(GetRotSpeed() > 0.0f || vfxSlipVal > 0.0f || vfxGroundSpeedVal > 0.0f BANK_ONLY( || g_vfxWheel.GetDisableNoMovementOpt()))
		{
			// check if we're in a puddle
			bool isInPuddle = colnVfxGroup==VFXGROUP_PUDDLE;

			// check for being in any 'liquid' materials
			VfxLiquidType_e liquidType = VFXLIQUID_TYPE_NONE;
			if (colnVfxGroup==VFXGROUP_MUD_DEEP)
			{
				liquidType = VFXLIQUID_TYPE_MUD;
				SetWetnessInfo(liquidType);
			}
			else if (colnVfxGroup==VFXGROUP_LIQUID_WATER || inDeepWater || inShallowWater || isInPuddle)
			{
				liquidType = VFXLIQUID_TYPE_WATER;
				SetWetnessInfo(liquidType);
			}
			else
			{
				// check the 'liquid' decals
				u32 foundDecalTypeFlag = 0;
				s8 decalLiquidType = -1;
				if (g_decalMan.IsPointInLiquid(1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID, decalLiquidType, vVfxVecHitCentrePos, 0.2f, false, false, DECAL_FADE_OUT_TIME, foundDecalTypeFlag))
				{
					liquidType = (VfxLiquidType_e)decalLiquidType;
					SetWetnessInfo(liquidType);
				}
			}

			// default the decal and ptfx vfx groups
			VfxGroup_e decalVfxGroup = colnVfxGroup;
			VfxGroup_e ptfxVfxGroup = colnVfxGroup;

			// deal with being in a liquid
			if (liquidType!=VFXLIQUID_TYPE_NONE)
			{
				// note that puddles just want to use the puddle vfx group settings instead
				if (!isInPuddle)
				{
					// override the ptfx vfx group with the liquid vfx group
					VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(liquidType);
					ptfxVfxGroup = liquidInfo.vfxGroup;

					// on a non-porous material we want to override the decal vfx group with the liquid vfx group
					if (PGTAMATERIALMGR->GetMtlFlagIsPorous(m_nHitMaterialId)==false)
					{
						VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(m_liquidAttachInfo.GetLiquidType());
						decalVfxGroup = liquidInfo.vfxGroup;
					}
				}
			}
			// otherwise deal with being wet from a liquid
			else if (m_liquidAttachInfo.GetTimer()>0.0f)
			{
				// on a non-porous material we want to override the decal vfx group with the liquid vfx group
				if (PGTAMATERIALMGR->GetMtlFlagIsPorous(m_nHitMaterialId)==false)
				{
					VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(m_liquidAttachInfo.GetLiquidType());
					decalVfxGroup = liquidInfo.vfxGroup;
				}
			}

			float currRoadWetness = g_weather.GetWetness();
			if ((m_pParentVehicle->GetVehicleAudioEntity() && m_pParentVehicle->GetVehicleAudioEntity()->GetIsInUnderCover()) || m_pParentVehicle->GetIsInInterior())
			{
				// sheltered from the rain - reset the wetness
				currRoadWetness = 0.0f;
			}

			// adjust the wetness value if the tyres are above a threshold temp
			float wetSubtract = 0.0f;
			if (m_fTyreTemp>VFXWHEEL_WET_REDUCE_TEMP_MIN)
			{
				wetSubtract = (m_fTyreTemp-VFXWHEEL_WET_REDUCE_TEMP_MIN)/(VFXWHEEL_WET_REDUCE_TEMP_MAX-VFXWHEEL_WET_REDUCE_TEMP_MIN);
			}

			currRoadWetness = rage::Max(0.0f, currRoadWetness-wetSubtract);

			// no wet roads in interiors (big tunnels etc)
			if ((m_pParentVehicle && m_pParentVehicle->GetIsInInterior()) || g_timeCycle.GetCurrUpdateKeyframe().GetVar(TCVAR_NO_WEATHER_FX)>0.0f)
			{
				currRoadWetness = 0.0f;
			}
	
#if __BANK
			if (g_vfxWheel.GetWetRoadOverride()>0.0f)
			{
				currRoadWetness = g_vfxWheel.GetWetRoadOverride();
			}
#endif

			// calculate the wheel vfx values
			float vfxPressureVal = m_fTyreLoad/GetWheelWidth();

			// output the wheel vfx values
#if __BANK
			if (g_vfxWheel.GetOutputWheelVfxValues())
			{
				CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
				bool isPlayerCar = pPlayerPed && pPlayerPed->GetMyVehicle()==m_pParentVehicle;

				if (isPlayerCar)
				{
					vfxDebugf1("Wheel %d - slip:%.3f - pressure:%.3f - groundspeed:%.3f", m_WheelId, vfxSlipVal, vfxPressureVal, vfxGroundSpeedVal);
				}
			}
#endif

			if (!inShallowWater)
			{
				ProcessWheelVFxDry(pVfxVehicleInfo, ptfxVfxGroup, vVfxVecHitPos, vVfxVecHitCentrePos, vVfxVecHitNormal, vfxSlipVal, vfxGroundSpeedVal, currRoadWetness, distSqrToCam);
			}

			ProcessWheelVFxWet(pVfxVehicleInfo, ptfxVfxGroup, vVfxVecHitPos, vVfxVecHitCentrePos, vVfxVecHitNormal, vfxSlipVal, vfxGroundSpeedVal, currRoadWetness, inShallowWater, waterZ, distSqrToCam);
			
			//We only want to process skid vfx during gameplay e.g. Without replay or only during recording
			REPLAY_ONLY( if( !CReplayMgr::IsEditModeActive() ) )
			{
				ProcessSkidmarkVFx(pVfxVehicleInfo, decalVfxGroup, vVfxVecHitCentrePos, vVfxVecHitNormal ,vfxSlipVal, vfxPressureVal, currRoadWetness, distSqrToCam);
			}
		}
	}

	if (GetDynamicFlags().IsFlagSet(WF_ON_FIRE) || GetTyreShouldBurst(m_pParentVehicle->GetVelocity()) )
	{
		ProcessWheelBurst();
	}

	// check if this wheel has hit a ped
	if (m_pHitPed)
	{
		ProcessPedCollision();
	}
	else
	{	
		GetDynamicFlags().ClearFlag(WF_SQUASHING_PED);
	}

	// update the wheel wetness
	m_liquidAttachInfo.Update(fwTimer::GetTimeStep());
}


void CWheel::ProcessPedCollision()
{
	// don't process vehicles that aren't almost upright
	if (m_pParentVehicle->GetTransform().GetC().GetZf() < 0.9f)
	{
		return;
	}

	// get the current wheel position and radius
	Vector3 wheelPos;
	float wheelRadius = GetWheelPosAndRadius(wheelPos);

	//grcDebugDraw::Sphere(wheelPos, 0.15f, Color32(1.0f, 0.0f, 0.0f, 1.0f));

	// get the end probe position
	Vector3 endProbePos = wheelPos;
	endProbePos.z -= wheelRadius+0.25f;

	// Test if the ped is under the wheel.
	WorldProbe::CShapeTestHitPoint probeIsect;
	WorldProbe::CShapeTestResults probeResult(probeIsect);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(wheelPos, endProbePos);
	probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	probeDesc.SetIncludeEntity(m_pHitPed);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		if (m_pHitPed->GetPedVfx() == NULL || (m_pHitPed->GetPedVfx() && m_pHitPed->GetPedVfx()->GetBloodVfxDisabled()))
		{
			return;
		}

		//grcDebugDraw::Sphere(wheelPos, 0.20f, Color32(1.0f, 1.0f, 0.0f, 1.0f));

		// test if this is a body or head part
		s32 boneTag = m_pHitPed->GetBoneTagFromRagdollComponent(probeResult[0].GetHitComponent());
		if (boneTag==BONETAG_ROOT		|| boneTag==BONETAG_PELVIS		|| 
			boneTag==BONETAG_SPINE0		|| boneTag==BONETAG_SPINE1		|| boneTag==BONETAG_SPINE2		|| boneTag==BONETAG_SPINE3		|| 
			boneTag==BONETAG_NECK		|| boneTag==BONETAG_HEAD		|| boneTag==BONETAG_NECKROLL	|| 
			boneTag==BONETAG_R_CLAVICLE	|| boneTag==BONETAG_L_CLAVICLE  || boneTag==BONETAG_R_UPPERARM	|| boneTag==BONETAG_L_UPPERARM	||
			boneTag==BONETAG_R_THIGH	|| boneTag==BONETAG_L_THIGH)
		{
			//grcDebugDraw::Sphere(wheelPos, 0.25f, Color32(0.0f, 1.0f, 0.0f, 1.0f));

			// play a blood effect to simulate the wheel squashing the ped
			if (!GetDynamicFlags().IsFlagSet(WF_SQUASHING_PED))
			{
				Vector3 fxPos = probeResult[0].GetHitPosition();
				g_vfxBlood.TriggerPtFxWheelSquash(this, m_pParentVehicle, RCC_VEC3V(fxPos));
				SetWetnessInfo(VFXLIQUID_TYPE_BLOOD);
				GetDynamicFlags().SetFlag(WF_SQUASHING_PED);
			}
		}
	}
}


void CWheel::ProcessWheelVFxDry(CVfxVehicleInfo* pVfxVehicleInfo, VfxGroup_e mtlVfxGroup, Vec3V_In vVfxVecHitPos, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxGroundSpeedVal, float currRoadWetness, float distSqrToCam)
{
	// return if we don't want to be processing dry wheel effect
	if (currRoadWetness>VFXWHEEL_WETROAD_DRY_MAX_THRESH)
	{
		return;
	}

	// calc the wet evo
	float wetEvo = CVfxHelper::GetInterpValue(currRoadWetness, 0.0f, VFXWHEEL_WETROAD_DRY_MAX_THRESH);

	// set the main tyre state (based on the max dry threshold)
	VfxTyreState_e tyreState = VFXTYRESTATE_OK_DRY;
	if (m_fTyreHealth==0.0f)
	{
		tyreState = VFXTYRESTATE_BURST_DRY;
	}

	// get the vehicle fx info
	VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(tyreState, mtlVfxGroup);

	ProcessWheelVFx(pVfxVehicleInfo, pVfxWheelInfo, vVfxVecHitPos, vVfxVecHitCentrePos, vVfxVecHitNormal, vfxSlipVal, vfxGroundSpeedVal, wetEvo, false, false, 0.0f, distSqrToCam);
}


void CWheel::ProcessWheelVFxWet(CVfxVehicleInfo* pVfxVehicleInfo, VfxGroup_e mtlVfxGroup, Vec3V_In vVfxVecHitPos, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxGroundSpeedVal, float currRoadWetness, bool inWater, float waterZ, float distSqrToCam)
{
	// return if we don't want to be processing wet wheel effect
	if (currRoadWetness<VFXWHEEL_WETROAD_WET_MIN_THRESH && !inWater)
	{
		return;
	}

	// calc the wet evo
	float wetEvo = CVfxHelper::GetInterpValue(currRoadWetness, VFXWHEEL_WETROAD_WET_MIN_THRESH, 1.0f);

	// set the main tyre state (based on the max dry threshold)
	VfxTyreState_e tyreState = VFXTYRESTATE_OK_WET;
	if (m_fTyreHealth==0.0f)
	{
		tyreState = VFXTYRESTATE_BURST_WET;
	}

	// get the vehicle fx info
	VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(tyreState, mtlVfxGroup);

	ProcessWheelVFx(pVfxVehicleInfo, pVfxWheelInfo, vVfxVecHitPos, vVfxVecHitCentrePos, vVfxVecHitNormal, vfxSlipVal, vfxGroundSpeedVal, wetEvo, true, inWater, waterZ, distSqrToCam);
}


void CWheel::ProcessWheelVFx(CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vVfxVecHitPos, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxGroundSpeedVal, float wetEvo, bool isWet, bool inWater, float waterZ, float distSqrToCam)
{
	// check for 'deep' materials (e.g. snow)
	Vec3V vWheelPos = vVfxVecHitPos;
	Vec3V vWheelNormal = vVfxVecHitNormal;

	// when the tyre is burst make sure we use the centre of the wheel so the sparks line up
	if (m_fTyreHealth==0.0f)
	{
		vWheelPos = vVfxVecHitCentrePos;
	}

	// relocate to the water surface 
	if (inWater)
	{
		vWheelPos.SetZf(waterZ);
	}

	// friction effect
	g_vfxWheel.UpdatePtFxWheelFriction(this, m_pParentVehicle, pVfxVehicleInfo, pVfxWheelInfo, vWheelPos, distSqrToCam, vfxSlipVal, wetEvo, isWet);

	// displacement effect (eg splashing through water, sand)
	g_vfxWheel.UpdatePtFxWheelDisplacement(this, m_pParentVehicle, pVfxVehicleInfo, pVfxWheelInfo, vWheelPos, vWheelNormal, distSqrToCam, vfxGroundSpeedVal, wetEvo, isWet);

	// burnout effect
	g_vfxWheel.UpdatePtFxWheelBurnout(this, m_pParentVehicle, pVfxVehicleInfo, pVfxWheelInfo, vWheelPos, distSqrToCam, vfxSlipVal, wetEvo, isWet);

	// lights
	g_vfxWheel.ProcessWheelLights(this, m_pParentVehicle, pVfxVehicleInfo, pVfxWheelInfo, vWheelPos, distSqrToCam, vfxSlipVal);
}

void CWheel::ProcessSkidmarkVFx(CVfxVehicleInfo* pVfxVehicleInfo, VfxGroup_e mtlVfxGroup, Vec3V_In vVfxVecHitCentrePos, Vec3V_In vVfxVecHitNormal, float vfxSlipVal, float vfxPressureVal, float currRoadWetness, float distSqrToCam)
{
	// skidmarks (never do on glass though)
#if __BANK
	if (g_vfxWheel.GetDisableSkidmarks()==false)
#endif
	{
		if (!PGTAMATERIALMGR->GetIsShootThrough(m_nHitMaterialId))
		{
			//		grcDebugDraw::Sphere(vVfxVecHitCentrePos, 0.1f, Color32(1.0f, 0.0f, 0.0f, 1.0f), true);

			// set the tyre state
			VfxTyreState_e tyreState = VFXTYRESTATE_OK_DRY;
			if (m_fTyreHealth==0.0f)
			{
				if (currRoadWetness>=VFXWHEEL_WETROAD_WET_MIN_THRESH)
				{
					tyreState = VFXTYRESTATE_BURST_WET;
				}
				else
				{
					tyreState = VFXTYRESTATE_BURST_DRY;
				}
			}
			else
			{
				if (currRoadWetness>=VFXWHEEL_WETROAD_WET_MIN_THRESH)
				{	
					tyreState = VFXTYRESTATE_OK_WET;
				}
			}

			if (pVfxVehicleInfo->GetWheelSkidmarkRearOnly())
			{
				// get the vehicle direction
				Vec3V vDir = RCC_VEC3V(m_pParentVehicle->GetVelocity());
				vDir = Normalize(vDir);

				float dot = Dot(vDir, m_pParentVehicle->GetTransform().GetB()).Getf();
	
				if (dot>=0.0f)
				{
					eHierarchyId hierarchyId = GetHierarchyId();
					if (hierarchyId!=VEH_WHEEL_LR && hierarchyId!=VEH_WHEEL_RR)
					{
						return;
					}
				}
				else
				{
					eHierarchyId hierarchyId = GetHierarchyId();
					if (hierarchyId!=VEH_WHEEL_LF && hierarchyId!=VEH_WHEEL_RF)
					{
						return;
					}
				}
			}

			vfxSlipVal *= pVfxVehicleInfo->GetWheelSkidmarkSlipMult();
			vfxPressureVal *= pVfxVehicleInfo->GetWheelSkidmarkPressureMult();

			// return if not in range
			VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(tyreState, mtlVfxGroup);

			float maxDecalRangeSqr = VFXRANGE_WHEEL_SKIDMARK * pVfxVehicleInfo->GetWheelGenericRangeMult() * g_vfxWheel.GetWheelSkidmarkRangeScale();
			maxDecalRangeSqr *= maxDecalRangeSqr;
			if (distSqrToCam > maxDecalRangeSqr)
			{
				return;
			}

			// check if we should be generating a skidmark
			if (vfxSlipVal>pVfxWheelInfo->skidmarkSlipThreshMin || vfxPressureVal>pVfxWheelInfo->skidmarkPressureThreshMin || m_liquidAttachInfo.GetTimer()>0.0f)
			{		
				float decalLife = -1.0f;
				float decalAlpha = 0.0f;
				if (m_liquidAttachInfo.GetTimer()>0.0f)
				{
					decalAlpha = m_liquidAttachInfo.GetCurrAlpha();
					decalLife = m_liquidAttachInfo.GetDecalLife();
				}
				else 
				{
					float slipDecalAlpha = 0.0f;
					if (vfxSlipVal>pVfxWheelInfo->skidmarkSlipThreshMin)
					{
						slipDecalAlpha = CVfxHelper::GetInterpValue(vfxSlipVal, pVfxWheelInfo->skidmarkSlipThreshMin, pVfxWheelInfo->skidmarkSlipThreshMax);
					}

					float pressureDecalAlpha = 0.0f;
					if (vfxPressureVal>pVfxWheelInfo->skidmarkPressureThreshMin)
					{
						pressureDecalAlpha = CVfxHelper::GetInterpValue(vfxPressureVal, pVfxWheelInfo->skidmarkPressureThreshMin, pVfxWheelInfo->skidmarkPressureThreshMax);
					}

					decalAlpha = Min(1.0f, slipDecalAlpha+pressureDecalAlpha) * VFXWHEEL_SKID_MAX_ALPHA;
				}

				// calc width multiplier based on the angle of the wheel to the road surface (i.e. thin when tilted - car on 2 wheels)
				float widthMultTilt = DotProduct(VEC3V_TO_VECTOR3(vVfxVecHitNormal), VEC3V_TO_VECTOR3(m_pParentVehicle->GetTransform().GetC()));

				static float angleThresh = 0.5f;
				if (widthMultTilt<angleThresh)
				{
					return;
				}
				widthMultTilt *= widthMultTilt;

				// calc width mutliplier based on skid direction relative to the car forward direction (i.e. thin when skidding side on)
				Vector3 wheelSkidDir = m_vecGroundContactVelocity;
				wheelSkidDir.Normalize();
				float widthMultSkidDir = Abs(DotProduct(VEC3V_TO_VECTOR3(m_pParentVehicle->GetTransform().GetB()), wheelSkidDir));
				if (m_fTyreHealth==0.0f)
				{
					// rim skids need to be thinner 
					widthMultSkidDir = (widthMultSkidDir*widthMultSkidDir*0.95f) + 0.05f;
				}
				else
				{
					// tyre skids need to be thicker 
					widthMultSkidDir = (widthMultSkidDir*0.5f) + 0.5f;
				}

				// calc final width multiplier
				float widthMult = widthMultTilt;// * widthMultSkidDir;

				// calc standard width of this wheel
				float actualWheelWidth = GetWheelWidth();

				float skidWidthMult = VFXWHEEL_SKID_WIDTH_MULT;
				if (m_pParentVehicle->GetVehicleType()==VEHICLE_TYPE_BICYCLE)
				{
					// bicycle wheels widths are much larger that they are visually as the physics can't handle small widths
					// so we use a different multiplier here to compensate
					skidWidthMult = VFXWHEEL_SKID_WIDTH_MULT_BICYCLE;
				}

				float wheelWidth = (actualWheelWidth*skidWidthMult) * widthMultSkidDir;

				// check for 'deep' materials (e.g. snow)
				Vec3V vWheelPos = vVfxVecHitCentrePos;
				Vec3V vWheelNormal = vVfxVecHitNormal;

				if (m_deepSurfaceInfo.IsActive())
				{
					vWheelPos = m_deepSurfaceInfo.GetPosWld();
					vWheelNormal = m_deepSurfaceInfo.GetNormalWld();
				}

 				// move forward to compensate for the decals being projected a frame behind
 				Vec3V vVelocity = RCC_VEC3V(m_pParentVehicle->GetVelocity());
				ScalarV vDeltaTime = ScalarVFromF32(CVfxHelper::GetSmoothedTimeStep());
 				vWheelPos += vVelocity*vDeltaTime;

				// check which decal to use
				s32 decalRenderSettingIndex = -1;
				s32 decalRenderSettingCount = 0;
				if (pVfxVehicleInfo->GetWheelGenericDecalSet()==2)
				{
					decalRenderSettingIndex = pVfxWheelInfo->decal2RenderSettingIndex;
					decalRenderSettingCount = pVfxWheelInfo->decal2RenderSettingCount;
				}
				else if (pVfxVehicleInfo->GetWheelGenericDecalSet()==3)
				{
					decalRenderSettingIndex = pVfxWheelInfo->decal3RenderSettingIndex;
					decalRenderSettingCount = pVfxWheelInfo->decal3RenderSettingCount;
				}
				else
				{
					vfxAssertf(pVfxVehicleInfo->GetWheelGenericDecalSet()==1, "invalid wheel decal set specified for %s", m_pParentVehicle->GetDebugName());
					decalRenderSettingIndex = pVfxWheelInfo->decal1RenderSettingIndex;
					decalRenderSettingCount = pVfxWheelInfo->decal1RenderSettingCount;
				}

				// hack - halftrack has tracks on the rear wheels
				if (MI_CAR_HALFTRACK.IsValid() && m_pParentVehicle->GetModelIndex()==MI_CAR_HALFTRACK) 
				{					
					eHierarchyId hierarchyId = GetHierarchyId();
					if (hierarchyId==VEH_WHEEL_LR || hierarchyId==VEH_WHEEL_RR)
					{
						decalRenderSettingIndex = pVfxWheelInfo->decal3RenderSettingIndex;
						decalRenderSettingCount = pVfxWheelInfo->decal3RenderSettingCount;
						wheelWidth *= 1.25f;
					}
				}

				// hack 
				if ((MI_CAR_SCARAB.IsValid() && m_pParentVehicle->GetModelIndex()==MI_CAR_SCARAB) || 
					(MI_CAR_SCARAB2.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_SCARAB2) || 
					(MI_CAR_SCARAB3.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_SCARAB3))
				{
					g_decalMan.FindRenderSettingInfo(DECALID_SCARAB_WHEEL_TRACKS, decalRenderSettingIndex, decalRenderSettingCount);
				}
				else if ((MI_CAR_MINITANK.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_MINITANK))
				{
					g_decalMan.FindRenderSettingInfo(DECALID_MINITANK_WHEEL_TRACKS, decalRenderSettingIndex, decalRenderSettingCount);
				}
				else if (m_pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_FORMULA_VEHICLE) ||
						 (MI_CAR_VETO.IsValid()  && m_pParentVehicle->GetModelIndex() == MI_CAR_VETO) ||
						 (MI_CAR_VETO2.IsValid() && m_pParentVehicle->GetModelIndex() == MI_CAR_VETO2))
				{
					decalRenderSettingIndex = pVfxWheelInfo->decalSlickRenderSettingIndex;
					decalRenderSettingCount = pVfxWheelInfo->decalSlickRenderSettingCount;
				}

				VfxTrailInfo_t vfxTrailInfo;
				vfxTrailInfo.id							= fwIdKeyGenerator::Get(this, 0);
				vfxTrailInfo.type						= VFXTRAIL_TYPE_SKIDMARK;
				vfxTrailInfo.regdEnt					= m_pHitPhysical;
				vfxTrailInfo.componentId				= m_nHitComponent;
				vfxTrailInfo.vWorldPos					= vWheelPos;
				vfxTrailInfo.vNormal					= vWheelNormal;
				vfxTrailInfo.vDir						= Vec3V(V_ZERO);			// not required as we have an id
				vfxTrailInfo.vForwardCheck				= Vec3V(V_ZERO);
				vfxTrailInfo.decalRenderSettingIndex	= decalRenderSettingIndex;
				vfxTrailInfo.decalRenderSettingCount	= decalRenderSettingCount;
				vfxTrailInfo.decalLife					= decalLife;
				vfxTrailInfo.width						= wheelWidth*widthMult;
				vfxTrailInfo.col						= Color32(pVfxWheelInfo->decalColR, pVfxWheelInfo->decalColG, pVfxWheelInfo->decalColB, static_cast<u8>(decalAlpha*255));
				vfxTrailInfo.pVfxMaterialInfo			= NULL;
				vfxTrailInfo.mtlId			 			= (u8)PGTAMATERIALMGR->UnpackMtlId(m_nHitMaterialId);
				vfxTrailInfo.liquidPoolStartSize		= 0.0f;
				vfxTrailInfo.liquidPoolEndSize			= 0.0f;
				vfxTrailInfo.liquidPoolGrowthRate		= 0.0f;
				vfxTrailInfo.createLiquidPools			= false;
				vfxTrailInfo.dontApplyDecal 			= false;

				g_vfxTrail.RegisterPoint(vfxTrailInfo);

				CVehicleModelInfo* pModelInfo = m_pParentVehicle->GetVehicleModelInfo();
				if (m_pParentVehicle->GetVehicleType()!=VEHICLE_TYPE_BIKE && m_pParentVehicle->GetVehicleType()!=VEHICLE_TYPE_BICYCLE && 
                    ((GetConfigFlags().IsFlagSet(WCF_REARWHEEL) && (m_pParentVehicle->pHandling->mFlags & MF_DOUBLE_REAR_WHEELS)) ||		// have we got double rear wheels?
                    (!GetConfigFlags().IsFlagSet(WCF_REARWHEEL) && (m_pParentVehicle->pHandling->mFlags & MF_DOUBLE_FRONT_WHEELS))) &&	// have we got double front wheels?
                    pModelInfo->GetStructure()->m_bWheelInstanceSeparateRear)
				{
					vfxTrailInfo.id = fwIdKeyGenerator::Get(this, 1);

					Vec3V vVehicleRight(m_pParentVehicle->GetTransform().GetA());

					if (GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
					{
						vfxTrailInfo.vWorldPos += vVehicleRight * ScalarVFromF32(GetWheelWidth()*1.1f);
					}
					else
					{
						vfxTrailInfo.vWorldPos -= vVehicleRight * ScalarVFromF32(GetWheelWidth()*1.1f);
					}

					g_vfxTrail.RegisterPoint(vfxTrailInfo);
				}
			}
		}
	}
}


void CWheel::ProcessAudio()
{

}


// want to do this properly based on steering radius
dev_float TEMP_STEER_WHEEL_MULT = 0.75f;
//
void CWheel::SetSteerAngle(float fAngle)
{
    CCarHandlingData* carHandlingData = m_pHandling->GetCarHandlingData();

    float frontToeIn = 0.0f;
    float rearToeIn = 0.0f;

    if( carHandlingData )
    {
        frontToeIn  = carHandlingData->m_fToeFront;
        rearToeIn   = carHandlingData->m_fToeRear;
    }

    float newSteerAngle = 0.0f;
    if(GetConfigFlags().IsFlagSet(WCF_STEER))
    {
        if(GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
        {
            fAngle -= frontToeIn;
        }
        else
        {
            fAngle += frontToeIn;
        }
        newSteerAngle = fAngle;
    }
    else
    {
        if(GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
        {
            m_fSteerAngle = -rearToeIn;
        }
        else
        {
            m_fSteerAngle = rearToeIn;
        }
        return;
    }

	////////////////////
	// do some quasi Ackermann steering thing, cheap for now, I'll do it properly later
	bool scaleSteerAngle = false;
	if(!GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && !GetConfigFlags().IsFlagSet(WCF_CENTRE_WHEEL))
	{
		if(GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
		{
			if(GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				scaleSteerAngle = (m_fSteerAngle > 0.0f);
			}
			else
			{
				scaleSteerAngle = (m_fSteerAngle < 0.0f);
			}
		}
		else
		{
			if(GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				scaleSteerAngle = (m_fSteerAngle < 0.0f);
			}
			else
			{
				scaleSteerAngle = (m_fSteerAngle > 0.0f);
			}
		}
	}

	if(scaleSteerAngle)
	{
		m_fSteerAngle = newSteerAngle * TEMP_STEER_WHEEL_MULT;
	}
	else
	{
		m_fSteerAngle = newSteerAngle;
	}
}

//
void CWheel::SetBrakeAndDriveForce( const float fBrakeForce, const float fDriveForce, const float fThrottle, const float fSpeedSqr )
{
	// Clear the flags once, we most likely won't have to touch them again
	GetDynamicFlags().ClearFlag(WF_ON_GAS | WF_FULL_THROTTLE | WF_BURNOUT | WF_BURNOUT_NON_DRIVEN_WHEEL);

	m_fBrakeForceOld = m_fBrakeForce;
    if(m_pHandling)
    {
		//Setup drive forces
		m_fDriveForceOld = m_fDriveForce;
		m_fDriveForce = 0.0f;

		if(fabs(fThrottle) >= 1.0f)
		{
			GetDynamicFlags().SetFlag(WF_FULL_THROTTLE);
		}

		if(fDriveForce!=0.0f)
		{
			// only set drive force if this is a driven wheel
			if( GetConfigFlags().IsFlagSet(WCF_POWERED) )
			{
                // if we have a centre diff apply the force evenly to the wheels and let the diff sort it out later
                if( m_pHandling->GetCarHandlingData() &&
                    m_pHandling->GetCarHandlingData()->aFlags & CF_DIFF_CENTRE )
                {
                    m_fDriveForce = fDriveForce;
                }
                else
                {
				    m_fDriveForce = fDriveForce * m_pHandling->GetDriveBias(m_fFrontRearSelector);
                }
			}
			// store whether the gas pedal was down for all wheels, not just driven ones
			GetDynamicFlags().SetFlag(WF_ON_GAS);
		}

		//setup brake forces
	    if(fThrottle > 0.9f && fBrakeForce > 0.1f)
	    {
		    // Attempting a burnout
		    if( !GetConfigFlags().IsFlagSet(WCF_TRACKED_WHEEL) &&
				( GetConfigFlags().IsFlagSet(WCF_POWERED) && (GetConfigFlags().IsFlagSet(WCF_REARWHEEL) || m_pHandling->m_fDriveBiasRear < 0.1f) ))
		    {
			    // Rear wheel, flag to lower grip and kill the brakes
			    m_fBrakeForce = 0.0f;
     
			    // Must be under a max speed to start a burnout 
			    if(fSpeedSqr < (CAutomobile::CAR_BURNOUT_SPEED*CAutomobile::CAR_BURNOUT_SPEED) || GetDynamicFlags().IsFlagSet(WF_BURNOUT))
                {
				    GetDynamicFlags().SetFlag(WF_BURNOUT);
                }
		    }
		    else
		    {
				m_fDriveForce = 0.0f;
				
				if(fSpeedSqr < (CAutomobile::CAR_BURNOUT_SPEED*CAutomobile::CAR_BURNOUT_SPEED) || GetDynamicFlags().IsFlagSet(WF_BURNOUT))
				{
					GetDynamicFlags().SetFlag(WF_BURNOUT_NON_DRIVEN_WHEEL);
				}
				
				TUNE_GROUP_FLOAT(WHEEL_TUNE, BURNOUT_FRONT_BRAKE_FORCE, 1.0f, 0.0f, 1.0f, 0.01f);
				m_fBrakeForce = m_pHandling->GetBrakeBias(m_fFrontRearSelector) * fBrakeForce * BURNOUT_FRONT_BRAKE_FORCE;
			}
	    }
	    else
	    {
		    m_fBrakeForce = m_pHandling->GetBrakeBias(m_fFrontRearSelector)*fBrakeForce;
	    }
	}
}

//
//
//

void CWheel::SetHandBrakeForce(float fForce)
{
	if(GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
	{
		m_fBrakeForce += fForce;
		GetDynamicFlags().ClearFlag(WF_ABS|WF_ABS_ALT);
		// handBrake overrides engine
		m_fDriveForce = 0.0f;
	}
}

void CWheel::Reset(bool bExtend)
{
	GetDynamicFlags().ClearFlag(WF_RESET_STATE);
	// set wheel as was touching so it doesn't think it's hanging in mid air if it was teleported
    GetDynamicFlags().SetFlag(WF_HIT_PREV);

    if(GetFragChild() > 0)// Vehicles that rely on probes for their suspension don't need to set this as their compression is up to date.
    {
        GetDynamicFlags().SetFlag(WF_RESET);
    }
	
	GetDynamicFlags().ClearFlag(WF_SIDE_IMPACT);

	m_fCompression = m_fCompressionOld = m_fWheelCompression = (bExtend ? 0.0f : m_fStaticDelta);

    m_fRotAng = 0.0f;
	m_fRotAngVel = 0.0f;

	m_vecHitCentrePosOld.Zero();

	m_pHitPhysical = NULL;
	m_pPrevHitPhysical = NULL;

	m_fMaterialGrip = 1.0f;
	m_fMaterialWetLoss = 1.0f;
    m_fMaterialDrag = 0.0f;
    m_fMaterialTopSpeedMult = 1.0f;

	m_fExtraSinkDepth = 0.0f;
}

void CWheel::SetMaterialFlags()
{
	// set whether the material that the wheel is on causes the tyre to heat up
	GetDynamicFlags().ClearFlag(WF_TYRES_HEAT_UP);
	if (PGTAMATERIALMGR->GetMtlFlagHeatsTyre(m_nHitMaterialId))
	{
		GetDynamicFlags().SetFlag(WF_TYRES_HEAT_UP);
	}
}

float CWheel::GetWheelCompressionIncBurstAndSink() const
{
	float fWheelCompression = m_fWheelCompression;
	if(m_fTyreHealth < TYRE_HEALTH_DEFAULT && m_fTyreWearRate <= 0.0f )
	{
		fWheelCompression -= GetTyreBurstCompression();
		fWheelCompression = rage::Max(0.0f, fWheelCompression);
	}
	
	// tyres sink into soft ground
	fWheelCompression -= m_fExtraSinkDepth;
    fWheelCompression += m_fWheelImpactOffset;
	fWheelCompression = rage::Max(0.0f, fWheelCompression);

	return fWheelCompression;
}

dev_float sfTyreDisintegrateVel = 10.0f;
dev_float sfTyreBurstCheckInterval = 1.0f / 30.0f;
//
void CWheel::ProcessWheelDamage(float fTimeStep)
{
	if(m_fTyreHealth > 0.0f && m_fTyreHealth < TYRE_HEALTH_DEFAULT && 
        ( m_fTyreWearRate == 0.0f || m_fTyreHealth < TYRE_HEALTH_FLAT ) )
	{
		SetTyreHealth(m_fTyreHealth - fTimeStep * (TYRE_HEALTH_DEFAULT / sfTyreDeflatePeriod));
		if(m_fTyreHealth < TYRE_HEALTH_FLAT)
		{
            // Bicycle tyres don't burst
			bool bBurst = false;
			if(rage::Abs(m_fRotAngVel)*m_fWheelRadius > sfTyreDisintegrateVel && m_nDynamicFlags.IsFlagSet(WF_HIT))
			{
				m_fBurstCheckTimer += fTimeStep;
				while(m_fBurstCheckTimer >= sfTyreBurstCheckInterval)
				{
					m_fBurstCheckTimer -= sfTyreBurstCheckInterval;

					float fRandom = g_DrawRand.GetFloat();
					if (NetworkInterface::IsGameInProgress())
					{
						mthRandom rnd(m_pParentVehicle ? (m_pParentVehicle->GetRandomSeed() * (NetworkInterface::GetSyncedTimeInMilliseconds() / 100)) : 0);
						fRandom = rnd.GetFloat();
					}

					if (fRandom < 0.01f)
					{
						bBurst = DoBurstWheelToRim();
					}
                    else if( m_fTyreWearRate > 0.0f && !m_TyreBurstDueToWear )
                    {
                        g_vfxWheel.TriggerPtFxWheelPuncture( this, m_pParentVehicle, RCC_VEC3V( VEC3_ZERO ), RCC_VEC3V( VEC3_ZERO ), m_pParentVehicle->GetBoneIndex( m_WheelId ) );
                        m_pParentVehicle->GetVehicleAudioEntity()->TriggerTyrePuncture( m_WheelId );
                        m_TyreBurstDueToWear = true;
                    }
				}

			}
			else			
            {
				m_fBurstCheckTimer = 0;
            }
			
			if(!bBurst )
			{
                if( m_fTyreWearRate == 0.0f )
                {
				    SetTyreHealth(TYRE_HEALTH_FLAT + TYRE_HEALTH_FLAT_ADD);
                }
                else
                {
                    SetTyreHealth( TYRE_HEALTH_FLAT_ADD );
                }
			}
		}
	}
}

bool CWheel::GetTyreShouldBurst(const Vector3& vecVelocity) const
{
    bool hasDownforceBias = ( GetHandlingData()->GetCarHandlingData() && ( GetHandlingData()->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS ) );

	return ( ( (m_fTyreTemp>=ms_fTyreTempBurstThreshold && !hasDownforceBias ) && (GetDynamicFlags().IsFlagSet(WF_BURNOUT)  || (m_fFrictionDamage > sfFrictionDamageValueToStartHeatingUpTyres) || vecVelocity.Mag2() < sfVelocityToReduceCoolingOfTyres)) || GetTyreShouldBurstFromDamage() );
}

bool CWheel::GetTyreShouldBurstFromDamage() const
{
	return( GetDynamicFlags().IsFlagSet(WF_WITHIN_DAMAGE_REGION) );
}

bool CWheel::GetTyreShouldBreakFromDamage() const
{
	return( GetDynamicFlags().IsFlagSet(WF_WITHIN_HEAVYDAMAGE_REGION) );
}

bool CWheel::DoBurstWheelToRim()
{
	if(m_pParentVehicle->m_nVehicleFlags.bTyresDontBurst || m_pParentVehicle->m_bTyresDontBurstToRim || m_pParentVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		return false;

	SetTyreHealth(0.0f);
	// tyre shreds - do particle fx for tyre falling apart (if the car is moving fast enough)
	Vector3 wheelPos;
	GetWheelPosAndRadius(wheelPos);

	Assert(m_pParentVehicle->GetBoneIndex(m_WheelId) > -1);
	g_vfxWheel.TriggerPtFxWheelBurst(this, m_pParentVehicle, RCC_VEC3V(wheelPos), m_pParentVehicle->GetBoneIndex(m_WheelId), GetTyreShouldBurst(m_pParentVehicle->GetVelocity()), m_fOnFireTimer>=sfTyreOnFirePeriod);
	m_pParentVehicle->GetVehicleAudioEntity()->TriggerTyrePuncture(m_WheelId, false);

    if( m_fTyreWearRate > 0.0f )
    {
        // tell the shader that we need to render the wheel without the tyre
        CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>( m_pParentVehicle->GetDrawHandler().GetShaderEffect() );
        Assert( pShader );
        // flag that tyres need to be deformed
        pShader->SetEnableTyreDeform( true );
    }

	return true;
}

void CWheel::SetWheelOnFire(bool bNetworkCheck)
{
	if (m_pParentVehicle)
	{
		if (NetworkInterface::IsGameInProgress() && bNetworkCheck && m_pParentVehicle->IsNetworkClone())
			return;

		if (!GetDynamicFlags().IsFlagSet(WF_ON_FIRE) && m_fTyreHealth>0.0f && !m_pParentVehicle->m_nVehicleFlags.bTyresDontBurst && m_pParentVehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE /*Disable trye burning on pedal bikes, B*735048*/)
		{
			//@@: location CWHEEL_SETWHEELONFIRE
			GetDynamicFlags().SetFlag(WF_ON_FIRE);
		}
	}
}

bool CWheel::IsWheelOnFire() const
{
	return GetDynamicFlags().IsFlagSet(WF_ON_FIRE);
}

void CWheel::ClearWheelOnFire()
{
	if (GetDynamicFlags().IsFlagSet(WF_ON_FIRE))
	{
		GetDynamicFlags().ClearFlag(WF_ON_FIRE);
		m_fOnFireTimer = 0.0f;
	}
}

 void CWheel::UpdateGroundVelocity()
 {
     if(m_pHitPhysical)
     {
         m_vecGroundVelocity = m_pHitPhysical->GetLocalSpeed(m_vecHitPos, true);
     }
     else
     {
         m_vecGroundVelocity.Zero();
     }
 }


void CWheel::ProcessWheelBurst()
{
	if (m_fTyreHealth<=0.0f REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
		return;

	if(m_pParentVehicle->m_nVehicleFlags.bTyresDontBurst)
		return;

	int nBoneIndex = m_pParentVehicle->GetBoneIndex(m_WheelId);

	Matrix34 wheelMtx;
	CVfxHelper::GetMatrixFromBoneIndex(RC_MAT34V(wheelMtx), m_pParentVehicle, nBoneIndex);

	bool isBeingExtinguished = false;
	if (GetDynamicFlags().IsFlagSet(WF_ON_FIRE) && !GetDynamicFlags().IsFlagSet(WF_BROKEN_OFF))
	{
		if(nBoneIndex != -1)
		{
			// check if the fuel tank is underwater
			bool isUnderwater = false;
			if (m_pParentVehicle->GetIsInWater())
			{
				float waterZ;
				if(m_pParentVehicle->m_Buoyancy.GetWaterLevelIncludingRivers(wheelMtx.d, &waterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
				{
					if (waterZ>=wheelMtx.d.z)
					{
						GetDynamicFlags().ClearFlag(WF_ON_FIRE);
						isUnderwater = true;
					}
				}
			}

			if (!isUnderwater && m_pParentVehicle->GetIsVisible())
			{
				// register the fire
				Vec3V vPosLcl(0.0f, 0.0f, 0.0f);
				if (m_pParentVehicle->GetVehicleType()!=VEHICLE_TYPE_BIKE && m_pParentVehicle->GetVehicleType()!=VEHICLE_TYPE_BICYCLE)
				{
					vPosLcl.SetXf(-GetWheelWidth()*0.5f);
				}

				float fireEvo = CVfxHelper::GetInterpValue(m_fOnFireTimer, 0.0f, 2.0f);

				// Check if the fire culprit is NULL
				bool bFireCulpritCleared = m_pParentVehicle->GetVehicleDamage()->GetEntityThatSetUsOnFire() == NULL && m_pParentVehicle->m_Transmission.GetEntityThatSetUsOnFire() == NULL;

				CFire* pFire = g_fireMan.RegisterVehicleTyreFire(m_pParentVehicle, nBoneIndex, vPosLcl, this, PTFXOFFSET_WHEEL_FIRE, fireEvo, bFireCulpritCleared);
				isBeingExtinguished = pFire && pFire->GetFlag(FIRE_FLAG_IS_BEING_EXTINGUISHED);
			}
		}
			
		if (isBeingExtinguished)
		{
			// speed up the life of the fire then clear it when done
			m_fOnFireTimer += fwTimer::GetTimeStep()*3.0f;
			if (m_fOnFireTimer>=sfTyreOnFirePeriod)
			{
				ClearWheelOnFire();
			}
		}
		else
		{
			m_fOnFireTimer += fwTimer::GetTimeStep();
		}
	}

	if(m_fOnFireTimer>=sfTyreOnFirePeriod || GetTyreShouldBurst(m_pParentVehicle->GetVelocity()))
	{
		const float defaultchance = 0.01f;
		const float damageChance = GetFrictionDamage() >= ms_fFrictionDamageThreshForBurst ? ms_fWheelBurstDamageChance : 0.0f;
		const bool damageBurst = GetTyreShouldBurstFromDamage();
		float randChance = damageBurst ? damageChance : defaultchance;
		if (m_pParentVehicle->IsLawEnforcementVehicle())
		{
			randChance *= 0.25f;
		}
		if (g_DrawRand.GetFloat() < randChance)
		{
			if (!m_pParentVehicle->IsNetworkClone())
			{
				// tyre bursts - do ptfx for tyre falling apart (if the car is moving fast enough)
				SetTyreHealth(0.0f);
				if(nBoneIndex != -1)
				{
					g_vfxWheel.TriggerPtFxWheelBurst(this, m_pParentVehicle, RCC_VEC3V(wheelMtx.d), nBoneIndex, GetTyreShouldBurst(m_pParentVehicle->GetVelocity()), m_fOnFireTimer>=sfTyreOnFirePeriod);
				}
				GetDynamicFlags().ClearFlag(WF_ON_FIRE);
				m_fOnFireTimer = 0.0f;

				// tell the shader that we need to render the wheel without the tyre
				CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(m_pParentVehicle->GetDrawHandler().GetShaderEffect());
				Assert(pShader);
				// flag that tyres need to be deformed
				pShader->SetEnableTyreDeform(true);

				m_pParentVehicle->GetVehicleAudioEntity()->TriggerTyrePuncture(m_WheelId, false);

				// Force the vehicle to wake up
				m_pParentVehicle->ActivatePhysics();
			}
		}

		if(damageBurst)
		{
			GetDynamicFlags().ClearFlag(WF_WITHIN_DAMAGE_REGION);
		}
	}
}

float CWheel::GetSuspensionPos(Vector3& vecPosResult)
{
	vecPosResult = m_aWheelLines.A;

	if( !m_bSuspensionForceModified )
	{
		return 1.5f*m_fWheelRadius;
	}
	else
	{
		static float scaleWheelRadiusWhenUsingHydraulics = 3.0f;
		return scaleWheelRadiusWhenUsingHydraulics*m_fWheelRadius;
	}
}

void CWheel::ApplySuspensionDamage(float fDamage)
{
    // Don't damage the suspension if tyres cant be burst
    if(m_pParentVehicle->m_nVehicleFlags.bTyresDontBurst)
        return;

    m_fSuspensionHealth -= fDamage;

    if(m_fSuspensionHealth < 0.0f)
    {
        // break off wheel?
        m_fSuspensionHealth = 0.0f;
    }
}

float CWheel::GetWheelPosAndRadius(Vector3& vecResult) const
{
	Matrix34 mWheel;
	m_pParentVehicle->GetGlobalMtx(m_pParentVehicle->GetBoneIndex(m_WheelId), mWheel);
	vecResult = mWheel.d;

	// if wheel is burst and tyre has disapeared, just use rim radius
	return m_fTyreHealth <= 0.0f ? m_fWheelRimRadius : m_fWheelRadius;
}

bool CWheel::GetWheelMatrixAndBBox(Matrix34& matResult, Vector3& vecBoxMaxResult) const
{
	m_pParentVehicle->GetGlobalMtx(m_pParentVehicle->GetBoneIndex(m_WheelId), matResult);

	// if wheel is burst and tyre has disapeared, just use rim radius
	float fRadius = m_fTyreHealth <= 0.0f ? m_fWheelRimRadius : m_fWheelRadius;

	vecBoxMaxResult.Set(0.5f*m_fWheelWidth, fRadius, fRadius);
	return true;
}

void CWheel::ApplyTyreDamage(const CEntity* pInflictor, float fDamage, const Vector3& vecPosLocal, const Vector3& vecNormLocal, eDamageType nDamageType, int nWheelIndex, bool bNetworkCheck)
{
	// script can set a flag to stop car tyres bursting
	if(m_pParentVehicle->m_nVehicleFlags.bTyresDontBurst)
		return;

	if (NetworkInterface::IsGameInProgress() && bNetworkCheck && m_pParentVehicle->IsNetworkClone())
		return;

	if (fDamage <= 0.0f && m_fTyreWearRate == 0.0f)
	{
		return;
	}

	// do initial tyre puncture fx
	if (m_fTyreHealth==TYRE_HEALTH_DEFAULT)
	{
		if(!vecNormLocal.IsZero())
		{
			// damage has just been applied to a fully healthy tyre
			g_vfxWheel.TriggerPtFxWheelPuncture(this, m_pParentVehicle, RCC_VEC3V(vecPosLocal), RCC_VEC3V(vecNormLocal), m_pParentVehicle->GetBoneIndex(m_WheelId));
			m_pParentVehicle->GetVehicleAudioEntity()->TriggerTyrePuncture(m_WheelId);
		}

		CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(m_pParentVehicle->GetDrawHandler().GetShaderEffect());
		Assert(pShader);
		// flag that tyres need to be deformed
		pShader->SetEnableTyreDeform(true);

		if (DAMAGE_TYPE_BULLET==nDamageType && pInflictor && pInflictor->GetIsTypePed() && ((const CPed*)pInflictor)->IsPlayer())
		{
			CStatsMgr::RegisterTyresPoppedByGunshot(m_pParentVehicle->GetRandomSeed(), (u8)nWheelIndex, m_pParentVehicle->GetNumWheels());
		}

		m_pParentVehicle->NotifyWheelPopped(this);
	}

	if(m_fTyreHealth > fDamage + TYRE_HEALTH_FLAT)
		SetTyreHealth(m_fTyreHealth - fDamage);
	else if(m_fTyreHealth > 0.0f && m_fTyreWearRate == 0.0f)
		SetTyreHealth(TYRE_HEALTH_FLAT + TYRE_HEALTH_FLAT_ADD);
	else
	{
		SetTyreHealth( Max( m_fTyreHealth - fDamage, 0.0f ) );
	}

	// make sure health is less than full if tyres are damaged
	if(m_pParentVehicle->GetHealth()>=CREATED_VEHICLE_HEALTH)
	{
		if (!m_pParentVehicle->IsNetworkClone())
		{
			m_pParentVehicle->ChangeHealth(-1.0f);
		}
	}
}

void CWheel::SetTyreWearRate( float wearRate )
{
    m_fTyreWearRate = wearRate;
#if !__NO_OUTPUT
    if( m_fTyreWearRate < 0.0f ||
        m_fTyreWearRate > 1.0f )
    {
        Assertf( false, "SetTyreWearRate being set to invalid value [%f] [%s][%s]", m_fTyreWearRate, m_pParentVehicle ? m_pParentVehicle->GetModelName() : "", m_pParentVehicle && m_pParentVehicle->GetNetworkObject() ? m_pParentVehicle->GetNetworkObject()->GetLogName() : "" );
        sysStack::PrintStackTrace();
    }
#endif
}

void CWheel::UpdateTyreHealthFromNetwork(float fHealth) 
{ 
	if (fHealth == 0.0f && m_fTyreHealth > 0.0f && m_pParentVehicle)
	{
		// tyre shreds - do particle fx for tyre falling apart (if the car is moving fast enough)
		Vector3 wheelPos;
		GetWheelPosAndRadius(wheelPos);

		Assert(m_pParentVehicle->GetBoneIndex(m_WheelId) > -1);
		g_vfxWheel.TriggerPtFxWheelBurst(this, m_pParentVehicle, RCC_VEC3V(wheelPos), m_pParentVehicle->GetBoneIndex(m_WheelId), GetTyreShouldBurst(m_pParentVehicle->GetVelocity()), m_fOnFireTimer>=sfTyreOnFirePeriod);
		m_pParentVehicle->GetVehicleAudioEntity()->TriggerTyrePuncture(m_WheelId, false);
	}

	SetTyreHealth(fHealth); 
}

void CWheel::ResetDamage()
{
	m_fSuspensionHealth = SUSPENSION_HEALTH_DEFAULT;
	m_fFrictionDamage = 0.0f;
	m_fTyreTemp = 0.0f;
	m_fRotSlipRatio = 0.0f;
    m_TyreBurstDueToWear = false;

	// shouldn't need to recreate wheel or tyre or anything because fragment should have been reset
	SetTyreHealth(TYRE_HEALTH_DEFAULT);
	ClearWheelOnFire();

	GetDynamicFlags().ClearFlag(WF_BROKEN_OFF);

	GetDynamicFlags().ClearFlag(WF_WITHIN_DAMAGE_REGION);
	GetDynamicFlags().ClearFlag(WF_WITHIN_HEAVYDAMAGE_REGION);

	GetConfigFlags().ClearFlag(WCF_DONT_RENDER_HUB);
}
#endif // __SPU

dev_float sfPushMult = 0.02f;

void CWheel::PushToSide(phCollider* pVehCollider, const Vector3& vecPushDirn)
{
	Vector3 vecPush;
	vecPush.DotV(vecPushDirn, RCC_VECTOR3(pVehCollider->GetMatrix().GetCol0ConstRef()));
	vecPush.Scale(sfPushMult);
	vecPush.Multiply(RCC_VECTOR3(pVehCollider->GetMatrix().GetCol0ConstRef()));

	m_vecHitCentrePosOld.Add(vecPush);
}

bool CWheel::IsMaterialIdPavement(phMaterialMgr::Id materialId) const
{
	if((PGTAMATERIALMGR->UnpackPolyFlags(materialId) & (1 << POLYFLAG_WALKABLE_PATH)) != 0)
	{
		return true;
	}

#if !__SPU
	phMaterialMgr::Id unpackedMaterialId = phMaterialMgrGta::UnpackMtlId(materialId);
	if(unpackedMaterialId == PGTAMATERIALMGR->g_idConcretePavement)
	{
		return true;
	}
	else if(unpackedMaterialId == PGTAMATERIALMGR->g_idBrickPavement)
	{
		return true;
	}

	if(PGTAMATERIALMGR->GetMtlVfxGroup(materialId) == VFXGROUP_PAVING)
	{
		return true;
	}
#endif

	return false;
}

bool CWheel::IsMaterialIdTrainTrack(phMaterialMgr::Id materialId) const
{

#if !__SPU
	phMaterialMgr::Id unpackedMaterialId = phMaterialMgrGta::UnpackMtlId(materialId);
	if(unpackedMaterialId == PGTAMATERIALMGR->g_idTrainTrack)
	{
		return true;
	}
#endif

	return false;
}


bool CWheel::IsMaterialIdStairs(phMaterialMgr::Id materialId) const
{
	if((PGTAMATERIALMGR->UnpackPolyFlags(materialId) & (1 << POLYFLAG_STAIRS)) != 0)
	{
		return true;
	}

	return false;
}

bool CWheel::HasInnerWheel() const
{
	return ((GetConfigFlags().IsFlagSet(WCF_REARWHEEL) && (m_pHandling->mFlags & MF_DOUBLE_REAR_WHEELS)) ||		// have we got double rear wheels?
		(!GetConfigFlags().IsFlagSet(WCF_REARWHEEL) && (m_pHandling->mFlags & MF_DOUBLE_FRONT_WHEELS)));
}


static float sfReachedTargetTollerance = 0.0001f;
static float sfReduceRateWhenOffGround = 0.5f;
static float sfMaxRateWhenOffGround = 1.0f;

void CWheel::UpdateHydraulics( int nTimeSlice )
{
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, lowerAllSuspensionRate, 0.2f, 0.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, lowerSuspensionRate, 0.4f, 0.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, raiseSuspensionRate, 0.5f, 0.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, bounceAllRate, 80.0f, 1.0f, 200.0f, 0.5f );
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, bounceRate, 1.5f, 0.0f, 200.0f, 0.1f );
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, freeRateScale, 1.0f, 0.0f, 50.0f, 0.1f );
	TUNE_GROUP_FLOAT(HYDRAULICS_TUNE, returnFromBounceScale, 2.0f, 0.0f, 50.0f, 0.1f );
	
	bool allHydraulicsRaised = m_pParentVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised;
	CAutomobile* pAutomobile = static_cast< CAutomobile* >( m_pParentVehicle );

	if( ( m_WheelState >= WHS_BOUNCE_FIRST_ACTIVE &&
	 	  m_WheelState <= WHS_BOUNCE_LAST_ACTIVE ) ||
		( m_WheelState !=
		  m_WheelTargetState ) )
	{
        //for the camera shake triggering on landing.
        static bool camShakeApplied = false; //does camera has already being shaken for this landing?

		if( !m_bHasDonkHydraulics )
		{
			switch( m_WheelTargetState )
			{
				case WHS_IDLE:
				{
					if( m_WheelState != WHS_LOCK_UP_ALL &&
						m_WheelState != WHS_LOCK_UP )
					{
						m_fSuspensionRaiseRate = raiseSuspensionRate;
					}
					else
					{
						m_fSuspensionRaiseRate = lowerSuspensionRate;
					}
					break;
				}
				case WHS_LOCK_UP_ALL:
				{
					m_fSuspensionRaiseRate = raiseSuspensionRate;

					if( m_fCompression <= 0.0f )
					{
						pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = true;
						pAutomobile->m_nAutomobileFlags.bHydraulicsJump = true;
					}
					break;
				}
				case WHS_LOCK_UP:
				{
					if( CPhysics::GetIsFirstTimeSlice( nTimeSlice ) )
					{
						m_fSuspensionRaiseRate *= raiseSuspensionRate;
					}
					break;
				}
				case WHS_LOCK_DOWN_ALL:
				case WHS_LOCK_DOWN:
				{
					m_fSuspensionRaiseRate = lowerAllSuspensionRate;
					break;
				}
				case WHS_BOUNCE_LEFT_RIGHT:
				case WHS_BOUNCE_FRONT_BACK:
				case WHS_BOUNCE:
				{
					if( CPhysics::GetIsFirstTimeSlice( nTimeSlice ) )
					{
						m_fSuspensionRaiseRate *= bounceRate;
					}
					pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising = true;
					break;
				}
				case WHS_BOUNCE_ALL:
				{
					if( CPhysics::GetIsFirstTimeSlice( nTimeSlice ) )
					{
						m_fSuspensionRaiseRate *= bounceAllRate;
					}
					pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising = true;
					break;
				}
				case WHS_BOUNCE_ALL_ACTIVE:
				{
					static float maxCompressionForWheelLandingBounceAll = 0.01f;
					if( m_fCompression <= maxCompressionForWheelLandingBounceAll )
					{
						m_WheelTargetState = WHS_BOUNCE_ALL_LANDING;
						camShakeApplied = false; //reset the camShakeApplied flag.
					}
					pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising = true;
					m_fSuspensionRaiseRate = raiseSuspensionRate;
					break;
				}
				case WHS_BOUNCE_ALL_LANDING:
				{
					static float minCompressionForWheelFreeAfterBounceAll = 0.18f;
					static float maxCompressionForModifiedSuspensionAfterBounceAll = 0.1f;
					static float minCompressionForModifiedSuspensionAfterBounceAll = 0.01f;
					static float maxCompressionLandingAllDiff = 0.0002f;
					static float minCompressionToApplyCamShake = 0.18f; //the minimum compression level to signal the camera shake

					if(!camShakeApplied && m_fCompression > minCompressionToApplyCamShake)
					{
						//update the camShakeApplied flag
						camShakeApplied = true;

						//we need to compute a fake impact damage. In the cinematic
						const float minRelativeDamageForShake = camInterface::GetCinematicDirector().GetVehicleImpactShakeMinDamage();
						const float fakeImpactDamage = m_pParentVehicle->pHandling->m_fMass * (minRelativeDamageForShake + 1.0f);
						const Vector3 fakeImpactDirection(0.0f,0.0f, -1.0f);

						camInterface::GetCinematicDirector().RegisterVehicleDamage(m_pParentVehicle, fakeImpactDirection, fakeImpactDamage);
					}

					if( m_fCompression > minCompressionForWheelFreeAfterBounceAll ||
						( m_bSuspensionForceModified && 
						  m_fCompression > minCompressionForModifiedSuspensionAfterBounceAll &&
						  m_fCompression < maxCompressionForModifiedSuspensionAfterBounceAll &&
						  Abs( m_fCompressionOld - m_fCompression ) < maxCompressionLandingAllDiff ) )
					{
						m_WheelTargetState = WHS_LOCK_UP_ALL;
					}
				
					pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = true;
					m_fSuspensionRaiseRate = raiseSuspensionRate;
					break;
				}
				case WHS_BOUNCE_ACTIVE:
				{
					static float maxCompressionForWheelLandingBounce = 0.01f;
					if( m_fCompression <= maxCompressionForWheelLandingBounce &&
						m_fSuspensionTargetRaiseAmount < m_fSuspensionRaiseAmount )
					{
						m_WheelTargetState = WHS_BOUNCE_LANDING;
						m_bSuspensionForceModified = true;

						//reset the cam shake flag
						camShakeApplied = false;
					}
				
					pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising = true;

					if( m_fSuspensionTargetRaiseAmount > m_fSuspensionRaiseAmount )
					{
						m_fSuspensionRaiseRate = raiseSuspensionRate;
					}
					else
					{
						m_fSuspensionRaiseRate = lowerSuspensionRate;
					}

					m_fSuspensionRaiseRate *= returnFromBounceScale;
					break;
				}
				case WHS_BOUNCE_LANDING:
				{
					static float minCompressionForWheelFreeAfterBounce = 0.25f;
					static float maxCompressionForModifiedSuspensionAfterBounce = 0.1f;
					static float minCompressionForModifiedSuspensionAfterBounce = 0.01f;
					static float maxCompressionLandingDiff = 0.0002f;

					static float minCompressionToApplyCamShake = 0.05f; //the minimum compression level to signal the camera shake
					if(!camShakeApplied && m_fCompression > minCompressionToApplyCamShake)
					{
						//update the camShakeApplied flag
						camShakeApplied = true;

						const eHierarchyId hierarchyId = GetHierarchyId();
						const bool isFront = hierarchyId==VEH_WHEEL_LF || 
											 hierarchyId==VEH_WHEEL_RF;

						//we need to compute a fake impact damage. In the cinematic
						const float minRelativeDamageForShake = camInterface::GetCinematicDirector().GetVehicleImpactShakeMinDamage();
						const float fakeImpactDamage = m_pParentVehicle->pHandling->m_fMass * (minRelativeDamageForShake + 1.0f);
						const Vector3 fakeImpactDirection(0.0f,0.0f, isFront ? -1.0f : 1.0f);

						camInterface::GetCinematicDirector().RegisterVehicleDamage(m_pParentVehicle, fakeImpactDirection, fakeImpactDamage);
					}

					if( m_fCompression > minCompressionForWheelFreeAfterBounce ||
						( m_bSuspensionForceModified && 
						  m_fCompression > minCompressionForModifiedSuspensionAfterBounce &&
						  m_fCompression < maxCompressionForModifiedSuspensionAfterBounce &&
						  Abs( m_fCompressionOld - m_fCompression ) < maxCompressionLandingDiff ) )
					{
						m_WheelTargetState = WHS_BOUNCE_LANDED;
					}
				
					pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = true;

					if( m_fSuspensionTargetRaiseAmount > m_fSuspensionRaiseAmount )
					{
						m_fSuspensionRaiseRate = raiseSuspensionRate;
					}
					else
					{
						m_fSuspensionRaiseRate = lowerSuspensionRate;
					}

					m_fSuspensionRaiseRate *= returnFromBounceScale;
					break;
				}
				case WHS_BOUNCE_LANDED:
				{
					static float maxCompressionForWheelFreeAfterBounce = 0.17f;
					static float maxCompressionForLandedAfterBounce = 0.01f;
					if( m_fCompression > maxCompressionForLandedAfterBounce &&
						m_fCompression < maxCompressionForWheelFreeAfterBounce )
					{
						m_WheelTargetState = WHS_FREE;

						if( m_fSuspensionTargetRaiseAmount <= 0.0f )
						{
							m_bSuspensionForceModified = false;
							GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
						}
					}
					else
					{
						pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = true;
					}

					if( m_fSuspensionTargetRaiseAmount > m_fSuspensionRaiseAmount )
					{
						m_fSuspensionRaiseRate = raiseSuspensionRate;
					}
					else
					{
						m_fSuspensionRaiseRate = lowerSuspensionRate;
					}

					m_fSuspensionRaiseRate *= returnFromBounceScale;

					break;
				}
				case WHS_FREE:
				{
					if( m_fSuspensionTargetRaiseAmount > m_fSuspensionRaiseAmount )
					{
						m_fSuspensionRaiseRate = raiseSuspensionRate;
					}
					else
					{
						m_fSuspensionRaiseRate = lowerSuspensionRate;
					}
					m_fSuspensionRaiseRate *= freeRateScale;
					break;
				}
				case WHS_LOCK:
				{
					m_fSuspensionRaiseRate = 1.0f;
					break;
				}
				default:
					break;
			}
		}

		if( !m_nDynamicFlags.IsFlagSet( WF_HIT_PREV ) &&
		    !m_nDynamicFlags.IsFlagSet( WF_HIT ) )
		{
			m_fSuspensionRaiseRate = Clamp( m_fSuspensionRaiseRate * sfReduceRateWhenOffGround, lowerSuspensionRate, sfMaxRateWhenOffGround );
		}

		if(	Abs( m_fSuspensionRaiseAmount - m_fSuspensionTargetRaiseAmount ) < sfReachedTargetTollerance )
		{
			m_WheelState = m_WheelTargetState;

			if( allHydraulicsRaised &&
				m_WheelState != WHS_LOCK_UP_ALL && 
				m_WheelState != WHS_LOCK_UP )
			{
				allHydraulicsRaised = false;
			}
			else if( !allHydraulicsRaised &&
					 m_WheelState == WHS_LOCK_UP_ALL )
			{
				allHydraulicsRaised = true;
			}

			if( m_WheelState == WHS_BOUNCE ||
				m_WheelState == WHS_BOUNCE_FRONT_BACK ||
				m_WheelState == WHS_BOUNCE_LEFT_RIGHT )
			{
				m_fSuspensionRaiseRate = 1.0f;
				m_WheelTargetState = WHS_BOUNCE_ACTIVE;
			}
			else if( m_WheelState == WHS_BOUNCE_ALL  )
			{
				m_fSuspensionRaiseRate = 1.0f;
				m_WheelTargetState = WHS_BOUNCE_ALL_ACTIVE;

				pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = true;
				allHydraulicsRaised = true;
			}

			m_pParentVehicle->m_nVehicleFlags.bAreHydraulicsAllRaised = allHydraulicsRaised;
		}
		else
		{
			m_pParentVehicle->m_nVehicleFlags.bPlayerModifiedHydraulics = true;
			GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
		}
	}

	if( m_bSuspensionForceModified &&
		m_WheelTargetState != WHS_BOUNCE_LANDING &&
		m_WheelState != WHS_BOUNCE_LANDING &&
		m_WheelTargetState != WHS_BOUNCE_LANDED &&
		m_WheelState != WHS_BOUNCE_LANDED )
	{
		static float maxCompressionForWheelFreeAfterBounce = 0.25f;
		static float minCompressionForWheelAfterLanding = 0.05f;
		static float maxCompressionDiff = 0.002f;

		if( m_fCompression < maxCompressionForWheelFreeAfterBounce &&
			m_fCompression > minCompressionForWheelAfterLanding &&
			Abs( m_fCompressionOld - m_fCompression ) < maxCompressionDiff )
		{
			m_bSuspensionForceModified = false;
			GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
		}
	}

}

bool CWheel::UpdateHydraulicsFlags( CAutomobile* pAutomobile )
{
	if( pAutomobile->m_nAutomobileFlags.bHydraulicsJump )
	{
		pAutomobile->m_nAutomobileFlags.bHydraulicsJump = false;

		if( GetCompression() <= 0.0f ) 
		{
			pAutomobile->m_nAutomobileFlags.bHydraulicsJump = true;
			return true;
		}
	}

	if( pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding )
	{
		pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = false;

		if( GetCompression() <= 0.0f ) 
		{
			pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = true;
			return true;
		}
	}

	if( m_bSuspensionForceModified &&
		!pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding &&
		!pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising )
	{
		if( GetCompression() <= 0.0f ) 
		{
			pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding = true;
		}
		else
		{
			pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising = true;
		}
		return true;
	}
	return false;
}


void CWheel::UpdateDownforce()
{
    TUNE_GROUP_FLOAT( VEHICLE_DOWNFORCE_TUNE, percentageOfMaxSpeedForFullDownforce, 0.9f, 0.0f, 2.0f, 0.1f );
    TUNE_GROUP_FLOAT( VEHICLE_DOWNFORCE_TUNE, percentageOfMaxSpeedForMinDownforce, 0.2f, 0.0f, 2.0f, 0.1f );
    TUNE_GROUP_FLOAT( VEHICLE_DOWNFORCE_TUNE, minDownForceScale, 0.3f, 0.0f, 2.0f, 0.1f );

    if( m_pHandling->GetCarHandlingData() &&
        m_pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
    {
        TUNE_GROUP_FLOAT( VEHICLE_DOWNFORCE_TUNE, DownforceModifierFront, 1.0f, 0.0f, 2.0f, 0.1f );
        TUNE_GROUP_FLOAT( VEHICLE_DOWNFORCE_TUNE, DownforceModifierRear, 1.0f, 0.0f, 2.0f, 0.1f );
        TUNE_GROUP_BOOL( VEHICLE_DOWNFORCE_TUNE, OverrideDownforceModifier, false );

        static dev_float sfBrokenWingDownforceScale = 0.2f;

        float downforceModifier = m_pHandling->m_fDownforceModifier;
        if( m_WheelId == VEH_WHEEL_LF || m_WheelId == VEH_WHEEL_RF )
        {
            int frontWingBoneIndex = m_pParentVehicle->GetBoneIndex( VEH_EXTRA_6 );
            int childIndex = frontWingBoneIndex == -1 ? -1 : m_pParentVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex( frontWingBoneIndex );
            bool wingBroken = false;

            if( !m_pParentVehicle->GetIsExtraOn( VEH_EXTRA_6 ) ||
                ( childIndex != -1 &&
                  m_pParentVehicle->GetVehicleFragInst()->GetChildBroken( childIndex ) ) )
            {
                wingBroken = true;
            }

            if( OverrideDownforceModifier )
            {
                downforceModifier *= DownforceModifierFront;
            }
            else
            {
                downforceModifier *= m_pParentVehicle->m_fDownforceModifierFront;
            }

            if( wingBroken )
            {
                downforceModifier *= sfBrokenWingDownforceScale;
            }
        }
        else if( m_WheelId == VEH_WHEEL_LR || m_WheelId == VEH_WHEEL_RR )
        {
            int rearWingBoneIndex = m_pParentVehicle->GetBoneIndex( VEH_EXTRA_1 );
            int childIndex = rearWingBoneIndex == -1 ? -1 : m_pParentVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex( rearWingBoneIndex );
            bool wingBroken = false;

            if( !m_pParentVehicle->GetIsExtraOn( VEH_EXTRA_1 ) ||
                ( childIndex != -1 &&
                  m_pParentVehicle->GetVehicleFragInst()->GetChildBroken( childIndex ) ) )
            {
                wingBroken = true;
            }


            if( OverrideDownforceModifier )
            {
                downforceModifier *= DownforceModifierRear;
            }
            else
            {
                downforceModifier *= m_pParentVehicle->m_fDownforceModifierRear;
            }

            if( wingBroken )
            {
                downforceModifier *= sfBrokenWingDownforceScale;
            }
        }
        
        Vec3V velocity = VECTOR3_TO_VEC3V( m_pParentVehicle->GetVelocity() );
        float velocityMag = Dot( m_pParentVehicle->GetTransform().GetForward(), velocity ).Getf();

        velocityMag *= velocityMag;

        float velocityFactor = Min( ( velocityMag / ( m_pHandling->m_fInitialDriveMaxFlatVel * m_pHandling->m_fInitialDriveMaxFlatVel * percentageOfMaxSpeedForFullDownforce ) ), 1.0f );
        velocityFactor *= velocityFactor;

        float downForceScale = 0.0f;
        downForceScale = minDownForceScale;
        velocityFactor = Max( velocityFactor, 0.0f );
        downForceScale = ( minDownForceScale + ( ( 1.0f - minDownForceScale ) * velocityFactor ) ) * downforceModifier;

        m_fDownforceMult = ms_fDownforceMult + ( ms_fDownforceMultSpoiler * downForceScale );

        static dev_float sfSuspensionRaiseAtFullDownforce = 0.08f;
        float fSuspensionRaise = ( velocityFactor * sfSuspensionRaiseAtFullDownforce );

        if( fSuspensionRaise != m_fSuspensionRaiseAmount )
        {
            m_fSuspensionRaiseAmount = fSuspensionRaise;
            GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
        }
    }
    else if( ms_bUseExtraWheelDownForce &&
	     	 m_pHandling->m_fDownforceModifier > 1.0f )
	{
		Vec3V velocity = VECTOR3_TO_VEC3V( m_pParentVehicle->GetVelocity() );
		float velocityMag = Dot( m_pParentVehicle->GetTransform().GetForward(), velocity ).Getf();

		TUNE_GROUP_BOOL( VEHICLE_DOWNFORCE_TUNE, useOldSystem, true );

		float velocityFactor = Min( ( velocityMag / ( m_pHandling->m_fInitialDriveMaxFlatVel * percentageOfMaxSpeedForFullDownforce ) ), 1.0f );
		velocityFactor *= velocityFactor;

		float downForceScale = 0.0f;

		if( ( !useOldSystem || 
			  m_pHandling->m_fDownforceModifier > 100.0f ) )
		{
			downForceScale = minDownForceScale;

			if( CNetwork::IsGameInProgress() )
			{
				velocityFactor = Max( velocityFactor - percentageOfMaxSpeedForMinDownforce, 0.0f );
				downForceScale = ( minDownForceScale + ( ( 1.0f - minDownForceScale ) * velocityFactor ) ) * ( m_pHandling->m_fDownforceModifier * 0.01f );
			}
		}
		else
		{
			downForceScale = ( velocityFactor * m_pHandling->m_fDownforceModifier );
		}

		if(!m_pParentVehicle->GetVariationInstance().HasSpoiler() ||
            m_pParentVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_SPOILER_MOD_DOESNT_INCREASE_GRIP ) )
		{
			m_fDownforceMult = ( ms_fDownforceMult * downForceScale );
		}
		else
		{
			if( sb_increaseSuspensionForceWithDownforce )
			{
				m_fDownforceMult = ms_fDownforceMult + ( ( ms_fDownforceMultSpoiler - ms_fDownforceMult ) * 0.25f );
				m_fDownforceMult *= downForceScale;
			}
			else
			{
				m_fDownforceMult = ( ms_fDownforceMultSpoiler * downForceScale );
			}
		}
	}
}

void CWheel::SetReducedGripLevel( int reducedGripLevel )
{
	m_fReducedGripMult = GetReduceGripMultiplier() + ( ( 1.0f - GetReduceGripMultiplier()) * (float)reducedGripLevel ) / 4.0f;
}

//float sfDesiredIntegrationStepSize = 0.02f;
float sfDesiredFrameRate = 60.0f;


RAGETRACE_DECL(WheelIntegrator_ProcessIntegration);

#if !__SPU
DEV_ONLY(u32 g_WheelTasksInProgress = 0;)
#endif

#if !SPU_WHEEL_INTEGRATION

bool CWheelIntegrator::RunFromDependency(const sysDependency& dependency)
{
	phCollider *pCollider = static_cast<phCollider*>(dependency.m_Params[0].m_AsPtr);
	CVehicle *pVehicle = static_cast<CVehicle*>(dependency.m_Params[1].m_AsPtr);
	WheelIntegrationState* pState = static_cast<WheelIntegrationState*>(dependency.m_Params[2].m_AsPtr);
	float fTimeStep = dependency.m_Params[3].m_AsFloat;

	Vector3 vecForce = pVehicle->GetInternalForce();
	Vector3 vecTorque = pVehicle->GetInternalTorque();
	CWheel* const* ppWheels = pVehicle->GetWheels();
	int nNumWheels = pVehicle->GetNumWheels();
	float fMaxVelInGear = pVehicle->m_Transmission.GetMaxVelInGear();
	int nGear = pVehicle->m_Transmission.GetGear();
	float fGravity = pVehicle->GetGravityForWheellIntegrator();

	TUNE_GROUP_BOOL( STUNT_PLANE, STUNT_ROLL_REDUCE_GRAVITY, false );

	if( STUNT_ROLL_REDUCE_GRAVITY ||
		( CPhysics::ms_bInStuntMode &&
		  pVehicle->InheritsFromPlane() ) ||
		( pVehicle->InheritsFromPlane() &&
		  pVehicle->pHandling->GetFlyingHandlingData() &&
		  pVehicle->pHandling->GetFlyingHandlingData()->m_fExtraLiftWithRoll > 0.0f ) )
	{
		TUNE_GROUP_FLOAT( STUNT_PLANE, REDUCE_GRAVITY_WITH_ROLL_MIN, 0.5f, 0.0f, 1.0f, 0.1f );
		Vector3 vecTransformA = VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetA() );

		fGravity *= ( REDUCE_GRAVITY_WITH_ROLL_MIN + ( ( 1.0f - Abs( vecTransformA.GetZ() ) ) * ( 1.0f - REDUCE_GRAVITY_WITH_ROLL_MIN ) ) );
	}

	WheelIntegrationOutput& output = pState->output;
	u32* pJobPending = &pState->jobPending;

	WheelIntegrationLocals locals;
	ProcessIntegrationTask(pCollider, vecForce, vecTorque, locals, ppWheels, nNumWheels, fMaxVelInGear, nGear, fTimeStep, fwTimer::GetFrameCount(), fGravity);
	output = locals.output;

#if __DEV
	u32* pWheelTasksInProgress = static_cast<u32*>(dependency.m_Params[4].m_AsPtr);
	sysInterlockedDecrement(pWheelTasksInProgress);
#endif

	sysInterlockedDecrement(pJobPending);

	return true;
}

#endif

//
//
dev_bool RE_APPLY_GRAVITY_SUBTRACT_VEL = false;
//
void CWheelIntegrator::ProcessIntegrationTask(phCollider* pCollider, const Vector3& vecInternalForce, const Vector3& vecInternalTorque, WheelIntegrationLocals& locals, CWheel* const* ppWheels, int nNumWheels, float fMaxVelInGear, int nGear, float fTimeStep, unsigned frameCount, float fGravity)
{
	PPU_ONLY(RAGETRACE_SCOPE(WheelIntegrator_ProcessIntegration));
	Assert(pCollider);

	// store pointers to some data we'll need through the integration
	InternalState internalState;
	internalState.m_pCollider = pCollider;
	internalState.m_ppWheels = ppWheels;
	internalState.m_nNumWheels = nNumWheels;

	// transform internal force into an acceleration
	internalState.m_vecInternalAccel = vecInternalForce * pCollider->GetInvMass();
	// torque needs to be untransformed, scaled, and transformed back
	RCC_MATRIX34(pCollider->GetMatrix()).UnTransform3x3(vecInternalTorque, internalState.m_vecInternalAngAccel);
	internalState.m_vecInternalAngAccel.Multiply(RCC_VECTOR3((pCollider->GetInvAngInertia())));
	RCC_MATRIX34(pCollider->GetMatrix()).Transform3x3(internalState.m_vecInternalAngAccel);

	// try and achieve a consistent step size with varying frame rate
	//float fNumLoops = fTimeStep / sfDesiredIntegrationStepSize;
	float fNumLoops = fTimeStep * sfDesiredFrameRate;
	//int nNumLoops = (int)rage::Max(1.0f, rage::ceil(fNumLoops));
	int nNumLoops = (int)rage::Max(1.0f, rage::Floorf(fNumLoops + 1.0f));		// JW : this is equivalent, I think...

	float dt = fTimeStep / (float)nNumLoops;
	float t = dt;

	// setup initial state
	locals.mat = RCC_MATRIX34(pCollider->GetMatrix());
	locals.output.vel = RCC_VECTOR3(pCollider->GetVelocity());
	locals.angVel = RCC_VECTOR3(pCollider->GetAngVelocity());



	WheelIntegrationTimeInfo timeInfo;
	timeInfo.fProbeTimeStep = fTimeStep;
	timeInfo.fProbeTimeStep_Inv = 1.0f / fTimeStep;
	timeInfo.fIntegrationTimeStep = dt;
	timeInfo.fIntegrationTimeStep_Inv = 1.0f / dt;

    bool bWheelReset = false;
	float fAccelerationFromResetWheels = 0.0f;
	Assertf(nNumWheels <= INTEGRATION_MAX_NUM_WHEELS, "Too many wheels - vehicle integration for wheel physics");
	for(int i=0; i<nNumWheels; i++)
	{
		CWheel& wheel = *ppWheels[i];
		
		// This limits how much we can change compression dummy vehicle mode
		wheel.UpdateDummyTransitionCompression(fTimeStep);

		// Set up the integration locals
		locals.aWheelAngSlip[i] = wheel.GetRotSlipRatio();
		locals.calcFwd[i].Zero();
		locals.calcSide[i].Zero();
		locals.compressionDelta[i] = 0.0f;

		// want to store average forces that were applied by the wheels
		wheel.m_vecSolverForce.Zero();

        if(wheel.GetWasReset())
        {
            wheel.m_nDynamicFlags.ClearFlag(WF_RESET);

			if(!wheel.GetIsTouching())
			{
				bWheelReset = true;
				// Note: the static force is actually the acceleration proportional to gravity
				fAccelerationFromResetWheels += wheel.GetStaticForce() * fGravity;
			}
        }
	}

	// subtract acceleration due to gravity - this will get re-applied inside integration loop
#if APPLY_GRAVITY_IN_INTEGRATOR
	internalState.m_vecAccelDueToGravity.Set(0.0f, 0.0f, -fGravity);

	// If wheels have been reset and not touching the ground, add the static accelerations of those wheels to gravity
	// This is for accommodating the cases that wheel contacts are not computed properly at frame that wheels are reset
	if(bWheelReset)
    {
	    internalState.m_vecAccelDueToGravity.z += fAccelerationFromResetWheels;
		internalState.m_vecAccelDueToGravity.z = Min(internalState.m_vecAccelDueToGravity.z, 0.0f);
    }

	// still want to be able to test this version
	if(RE_APPLY_GRAVITY_SUBTRACT_VEL)
		locals.output.vel -= Vector3(internalState.m_vecAccelDueToGravity) * fTimeStep;
#else
	internalState.m_vecAccelDueToGravity = VEC3_ZERO;
#endif

	for(int i=0; i<nNumLoops; i++, t+=dt)
	{
		// reset abs
		for(int nWheel=0; nWheel<nNumWheels; nWheel++)
			ppWheels[nWheel]->UpdateAbsTimer(dt);

		timeInfo.fTimeInIntegration = t;
		timeInfo.fTimeInIntegration_Inv = 1.0f / t;

		WheelIntegrationDerivative a, b, c, d;

		WheelIntegrationEvaluate(a, locals, timeInfo, 0.0f,    NULL, internalState, fGravity);		
		WheelIntegrationEvaluate(b, locals, timeInfo, dt*0.5f, &a,   internalState, fGravity);
		WheelIntegrationEvaluate(c, locals, timeInfo, dt*0.5f, &b,   internalState, fGravity);
		WheelIntegrationEvaluate(d, locals, timeInfo, dt,      &c,   internalState, fGravity);


		const Vector3 vecVel = 1.0f/6.0f * (a.vel + 2.0f*(b.vel + c.vel) + d.vel);
		const Vector3 vecAngVel = 1.0f/6.0f * (a.angVel + 2.0f*(b.angVel + c.angVel) + d.angVel);

		const Vector3 vecAccel = 1.0f/6.0f * (a.accel + 2.0f*(b.accel + c.accel) + d.accel);
		const Vector3 vecAngAccel = 1.0f/6.0f * (a.angAccel + 2.0f*(b.angAccel + c.angAccel) + d.angAccel);

		AddVelToMatrix(locals.mat, locals.mat, vecVel, vecAngVel, dt);
		locals.output.vel = locals.output.vel + vecAccel * dt;
		locals.angVel = locals.angVel + vecAngAccel * dt;


		float fLoopInc = float(i) / float(i+1);

        if( !ppWheels[ 0 ]->m_pHandling->GetCarHandlingData() ||
            !( ppWheels[ 0 ]->m_pHandling->GetCarHandlingData()->aFlags & CF_FIX_OLD_BUGS ) )
        {
            for( int i = 0; i < nNumWheels; i++ )
            {
                float fWheelAngSlipDt = 1.0f / 6.0f * ( a.aWheelAngSlipDt[ i ] + 2.0f*( b.aWheelAngSlipDt[ i ] + c.aWheelAngSlipDt[ i ] ) + d.aWheelAngSlipDt[ i ] );
                locals.aWheelAngSlip[ i ] = locals.aWheelAngSlip[ i ] + fWheelAngSlipDt * dt;

                Vector3 vecWheelForce = 1.0f / 6.0f * ( a.aWheelForce[ i ] + 2.0f*( b.aWheelForce[ i ] + c.aWheelForce[ i ] ) + d.aWheelForce[ i ] );
                ppWheels[ i ]->m_vecSolverForce = fLoopInc*ppWheels[ i ]->m_vecSolverForce + ( 1.0f - fLoopInc )*vecWheelForce;
            }
        }
        else
        {
		    for(int j=0; j<nNumWheels; j++)
		    {
			    float fWheelAngSlipDt = 1.0f/6.0f * (a.aWheelAngSlipDt[j] + 2.0f*(b.aWheelAngSlipDt[j] + c.aWheelAngSlipDt[j]) + d.aWheelAngSlipDt[j]);
			    locals.aWheelAngSlip[j] = locals.aWheelAngSlip[j] + fWheelAngSlipDt * dt;

			    Vector3 vecWheelForce = 1.0f/6.0f * (a.aWheelForce[j] + 2.0f*(b.aWheelForce[j] + c.aWheelForce[j]) + d.aWheelForce[j]);
			    ppWheels[j]->m_vecSolverForce = fLoopInc*ppWheels[j]->m_vecSolverForce + (1.0f - fLoopInc)*vecWheelForce;
		    }
        }
	}

	ProcessResultInternal(pCollider,locals,ppWheels,nNumWheels,fMaxVelInGear,nGear,fTimeStep,frameCount,&locals.output.vel, fGravity);
}

void CWheelIntegrator::ProcessIntegration(phCollider* pCollider, CVehicle* pVehicle, WheelIntegrationState& state, float fTimeStep, unsigned frameCount)
{
	(void) frameCount;

	const bool bIsArticulated = pCollider->IsArticulated();
	if(bIsArticulated)
	{
		phArticulatedCollider* pArticulatedCollider = static_cast<phArticulatedCollider*>(pCollider);

		// This is needed so that propagation doesn't need to occur during the wheel integration task.  If propagation was needed
		// then joints would also need to get DMAd over. If this wheel code ever starts to apply impulses or get velocities for parts other
		// than the root, joints will also need to get DMAd over regardless of whether EnsureVelocitiesFullyPropagated is called.
		pArticulatedCollider->GetBody()->EnsureVelocitiesFullyPropagated();
		PHARTICULATEDBODY_MULTITHREADED_VALDIATION_ONLY(pArticulatedCollider->GetBody()->SetReadOnlyOnMainThread(true);)
	}

	Assert(!state.jobPending);
	if (CWheel::ms_bUseWheelIntegrationTask)
	{
		state.dependency.Init( CWheelIntegrator::RunFromDependency, 0, 0);

		state.dependency.m_Priority = sysDependency::kPriorityHigh;
		state.dependency.m_Params[0].m_AsPtr = pCollider;
		state.dependency.m_Params[1].m_AsPtr = pVehicle;
		state.dependency.m_Params[2].m_AsPtr = &state;
		state.dependency.m_Params[3].m_AsFloat = fTimeStep;
		DEV_ONLY(state.dependency.m_Params[4].m_AsPtr = &g_WheelTasksInProgress;)

		sysInterlockedIncrement(&state.jobPending);

		sysDependencyScheduler::Insert( &state.dependency );
		DEV_ONLY(sysInterlockedIncrement(&g_WheelTasksInProgress);)

		return;
	}

#if __BANK && !__SPU	// TEST PPU integrate path
	WheelIntegrationLocals locals;
	ProcessIntegrationTask(pCollider, pVehicle->GetInternalForce(), pVehicle->GetInternalTorque(), locals, pVehicle->GetWheels(),
		pVehicle->GetNumWheels(), pVehicle->m_Transmission.GetMaxVelInGear(), pVehicle->m_Transmission.GetGear(),
		fTimeStep, frameCount, pVehicle->GetGravityForWheellIntegrator());
	state.output = locals.output;
#endif	// dev && !spu
}

RAGETRACE_DECL(WheelIntegrator_ProcessResult);

#if !__SPU	
void CWheelIntegrator::ProcessResult(WheelIntegrationState& state, Vector3* pForceApplied)
{
	PPU_ONLY(RAGETRACE_SCOPE(WheelIntegrator_ProcessResult));

	while(true)
	{
		volatile u32 *pDependencyRunning = &state.jobPending;
		if(*pDependencyRunning == 0)
		{
			break;
		}

		sysIpcYield(PRIO_NORMAL);
	}

	if (pForceApplied)
		*pForceApplied = state.output.vel;
}

void CWheelIntegrator::ProcessResultsFinal(phCollider* pCollider, CWheel** ppWheels, int nNumWheels, ScalarV_In timeStep)
{
	for(int i=0; i < nNumWheels; i++)
	{
		ppWheels[i]->IntegrationFinalProcess(pCollider,timeStep);
	}
}
#endif // !__SPU

//
//
dev_bool sbTestSkipWheelForces = false;

//
void CWheelIntegrator::ProcessResultInternal(phCollider* pCollider, WheelIntegrationLocals& locals, CWheel* const* ppWheels, int nNumWheels, float fMaxVelInGear, int nGear, float fTimeStep, unsigned frameCount, Vector3* pForceApplied, float fGravity)
{
	if(sbTestSkipWheelForces)
	{
#if APPLY_GRAVITY_IN_INTEGRATOR&&!__SPU
		if(pCollider)
		{
			if(pCollider->IsArticulated())
				pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, false);
			else
				pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
		}
#endif // APPLY_GRAVITY_IN_INTEGRATOR&&!__SPU
		return;
	}

	// Fixup accumulated error
	// Small error in matrix orthonormality causes significant glitching
	static dev_float sfError = 0.00001f;
	if(!RCC_MATRIX34(pCollider->GetMatrix()).IsOrthonormal(sfError))
	{
		Matrix34 matNew = RCC_MATRIX34(pCollider->GetMatrix());
		matNew.Normalize();
		pCollider->SetMatrix(RCC_MAT34V(matNew));
	}

	// Make sure the ang momemtum stays correct
	if(!pCollider->IsArticulated())
	{
		pCollider->SetVelocity(pCollider->GetVelocity().GetIntrin128());
		pCollider->SetAngVelocity(pCollider->GetAngVelocity().GetIntrin128());
	}

	Vector3 vecV = RCC_VECTOR3(pCollider->GetVelocity());
	Vector3 vecDeltaV = locals.output.vel - vecV;

	Vector3 vecAngV = RCC_VECTOR3(pCollider->GetAngVelocity());
	Vector3 vecDeltaAngV = locals.angVel - vecAngV;
	if( nNumWheels == 2 &&
		!ppWheels[ 0 ]->m_bDisableWheelForces )
	{
		// todo add a flag in the wheel and set it if the bike has no rider and the velocity is low or there is no THROTTLE input or no rider
		static float maxForceMag = 0.035f;

		if( pForceApplied->Mag2() < maxForceMag )
		{
			static float sf_minDeltaV		= 10.0f;
			const float minDeltaV		= sf_minDeltaV * fTimeStep;
			const float minDeltaAngV	= sf_minDeltaV * fTimeStep;

			for( int i = 0; i < 3; i++ )
			{
				if( vecDeltaV[ i ] * vecV[ i ] < 0.0f && 
					Abs( vecDeltaV[ i ] ) > Abs( vecV[ i ] ) &&
					Abs( vecDeltaV[ i ] ) < minDeltaV )
				{
					vecDeltaV[ i ] = -vecV[ i ];
				}

				if( vecDeltaAngV[ i ] * vecAngV[ i ] < 0.0f && 
					Abs( vecDeltaAngV[ i ] ) > Abs( vecAngV[ i ] ) &&
					Abs( vecDeltaAngV[ i ] ) < minDeltaAngV )
				{
					vecDeltaAngV[ i ] = -vecAngV[ i ];
				}
			}
		}
	}

    Assert(locals.output.vel==locals.output.vel);
    Assert(vecDeltaV==vecDeltaV);

   	Vector3 vecPushToSide;
	for(int i=0; i < nNumWheels; i++)
	{
		vecPushToSide.Zero();
		ppWheels[i]->IntegrationPostProcess(pCollider, locals.aWheelAngSlip[i], fMaxVelInGear, nGear, fTimeStep, frameCount, vecPushToSide, locals.calcFwd[i], locals.calcSide[i], fGravity);

		if(vecPushToSide.IsNonZero())
		{
			eHierarchyId nOpposingWheelId = aOppositeWheelId[ppWheels[i]->GetHierarchyId() - VEH_WHEEL_LF];
			CWheel* pOtherWheel = NULL;
			for(int j=0; j < nNumWheels; j++)
			{
				if(ppWheels[j]->GetHierarchyId()==nOpposingWheelId)
				{
					pOtherWheel = ppWheels[j];
					break;
				}
			}
			if(pOtherWheel)
				pOtherWheel->PushToSide(pCollider, vecPushToSide);
		}
	}

	static dev_bool sb_disabledDiffsInWheel = false;

	if( !sb_disabledDiffsInWheel )
	{
		ApplyDifferentials( ppWheels, nNumWheels, fMaxVelInGear, fTimeStep );
	}

	pCollider->ApplyForceCenterOfMass(vecDeltaV * pCollider->GetMass() / fTimeStep, ScalarV(fTimeStep).GetIntrin128ConstRef());

	if(pForceApplied)
	{
		pForceApplied->Set(vecDeltaV * pCollider->GetMass() / fTimeStep);
#if APPLY_GRAVITY_IN_INTEGRATOR
		Vector3 vecGravity(0.0f, 0.0f, -fGravity);
        pForceApplied->Subtract(vecGravity * pCollider->GetMass());
#endif
	}

		RCC_MATRIX34(pCollider->GetMatrix()).UnTransform3x3(vecDeltaAngV);
		vecDeltaAngV.Multiply(RCC_VECTOR3(pCollider->GetAngInertia()));
		RCC_MATRIX34(pCollider->GetMatrix()).Transform3x3(vecDeltaAngV);

		pCollider->ApplyTorque(vecDeltaAngV / fTimeStep, ScalarV(fTimeStep).GetIntrin128ConstRef());

}

void CWheelIntegrator::AddVelToMatrix(Matrix34& matResult, const Matrix34& matInit, const Vector3& vel, const Vector3& angVel, float dt)
{
	matResult = matInit;

	// Move the collider's position from its velocity.
	matResult.d.AddScaled(vel, dt);

	// Find the rotation and change the matrix orientation.
	Vector3 rotation(angVel);
	rotation.Scale(dt);
	float rotMag = rotation.Mag2();
	if (rotMag>SMALL_FLOAT)
	{
		rotMag = sqrtf(rotMag);
		Vector3 unitRotation(rotation);
		unitRotation.InvScale(rotMag);
		matResult.RotateUnitAxis(unitRotation,rotMag);
	}
}


dev_bool ADD_DERIVATIVE_VEL = true;

//
void CWheelIntegrator::WheelIntegrationEvaluate(WheelIntegrationDerivative &output, 
												WheelIntegrationLocals& initialLocals, 
												const WheelIntegrationTimeInfo& timeInfo, 
												float fDerivativeTimeStep, 
												const WheelIntegrationDerivative* pD,
												InternalState& istate,
												const float fGravity)
{
	WheelIntegrationLocals newLocals;
	if(pD)
	{
		if(ADD_DERIVATIVE_VEL)
			AddVelToMatrix(newLocals.mat, initialLocals.mat, pD->vel, pD->angVel, fDerivativeTimeStep);
		else
			AddVelToMatrix(newLocals.mat, initialLocals.mat, initialLocals.output.vel, initialLocals.angVel, fDerivativeTimeStep);

		newLocals.output.vel = initialLocals.output.vel + pD->accel*fDerivativeTimeStep;
		newLocals.angVel = initialLocals.angVel + pD->angAccel*fDerivativeTimeStep;
		for(int i=0; i<istate.m_nNumWheels; i++)
			newLocals.aWheelAngSlip[i] = initialLocals.aWheelAngSlip[i] + pD->aWheelAngSlipDt[i]*fDerivativeTimeStep;
	}
	else
	{
		newLocals.mat = initialLocals.mat;
		newLocals.output.vel = initialLocals.output.vel;
		newLocals.angVel = initialLocals.angVel;
		for(int i=0; i<istate.m_nNumWheels; i++)
			newLocals.aWheelAngSlip[i] = initialLocals.aWheelAngSlip[i];
	}

	//	WheelIntegrationDerivative output;
	output.vel = newLocals.output.vel;
	output.angVel = newLocals.angVel;
	output.accel = istate.m_vecAccelDueToGravity + istate.m_vecInternalAccel;
	output.angAccel = istate.m_vecInternalAngAccel;
	for(int i=0; i<istate.m_nNumWheels; i++)
		output.aWheelAngSlipDt[i] = 0.0f;

	// calculate acceleration for each wheel
	float fSuspensionForce = 0.0f;
	float fWheelAngSlipDt = 0.0f;
	Vector3 vecAccel, vecAngAccel, vecForce;


	// Don't do shapetest on first interation because our impact data will be accurate
	const bool bFirstRk4 = fDerivativeTimeStep == 0.0f;
	const bool bFirstLoop = bFirstRk4 &&									// This is the first step of an RK4
		timeInfo.fTimeInIntegration == timeInfo.fIntegrationTimeStep;	// This is the first RK4 integrate of the whole integration task (i.e. the first loop)

	for(int i=0; i<istate.m_nNumWheels; i++)
	{

		//REIMPLEMENTED BROKEN VERSION OF THIS CODE AS REQUESTED BY THE STUNTING COMMUNITY.
		// Add on some downforce 
		//	- not sure what this down force is for, but it causes the plane to constantly nosedown (B*967652), change it to only apply when wheel touches ground - nma
		Vector3 vBiasDownAccelPerWheel = VEC3_ZERO;
        Vector3 vExtraAngularAccelPerWheel = VEC3_ZERO;
		if(istate.m_nNumWheels > 0)
		{
			if(istate.m_ppWheels[i]->GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) || istate.m_ppWheels[i]->GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL))
			{
				// We could change this later so that bikes and quads can have variable downforce based on mods or something.
				istate.m_ppWheels[i]->m_fDownforceMult = CWheel::ms_fDownforceMultBikes;

				TUNE_GROUP_FLOAT( WHEEL_STUNT_TUNE, maxDotToInIncreaseWheelDownforce, 0.8f, -1.0f, 1.0f, 0.01f );

				if(istate.m_ppWheels[i]->m_nHitMaterialId == CWheel::m_MaterialIdStuntTrack && 
					newLocals.mat.c.Dot( Vector3( 0.0f, 0.0f, 1.0f ) ) < maxDotToInIncreaseWheelDownforce &&
					( istate.m_ppWheels[i]->GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) ||
					  istate.m_ppWheels[i]->GetIsTouching() ) ) // don't do this for quad bike wheels that aren't still touching something. 
				{
					TUNE_GROUP_FLOAT( WHEEL_STUNT_TUNE, stuntMaterialDownforceIncrease, 2.5f, 0.1f, 20.0f, 0.1f );
					istate.m_ppWheels[i]->m_fDownforceMult *= stuntMaterialDownforceIncrease;
				}

				vBiasDownAccelPerWheel = -newLocals.mat.c * Abs(newLocals.output.vel.Dot(newLocals.mat.b)) * istate.m_ppWheels[i]->m_fDownforceMult / ((float)istate.m_nNumWheels);
			}
			else
			{
				float fDownforceMult = istate.m_ppWheels[i]->m_fDownforceMult;//istate.m_ppWheels[i]->GetConfigFlags().IsFlagSet(WCF_SPOILER) ? CWheel::ms_fDownforceMultSpoiler : CWheel::ms_fDownforceMult;
				vBiasDownAccelPerWheel = -newLocals.mat.c * Abs(newLocals.output.vel.Dot(newLocals.mat.b)) * fDownforceMult / ((float)istate.m_nNumWheels);

                if( istate.m_ppWheels[ i ]->GetHandlingData()->GetCarHandlingData() &&
                    istate.m_ppWheels[ i ]->GetHandlingData()->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
                {
                    vExtraAngularAccelPerWheel = istate.m_ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) ? -newLocals.mat.b : newLocals.mat.b;
                    vExtraAngularAccelPerWheel.Cross( vBiasDownAccelPerWheel );
                }
			}
		}

		if(!bFirstLoop)
		{
			istate.m_ppWheels[i]->UpdateCompressionInsideIntegrator(newLocals.mat, istate.m_pCollider, initialLocals.compressionDelta[i]);
		}

		if(istate.m_ppWheels[i]->GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) || istate.m_ppWheels[0]->GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL))//bikes always apply down force
		{
			output.accel += vBiasDownAccelPerWheel;
		}


		if(istate.m_ppWheels[i]->GetIsTouching() && (!istate.m_ppWheels[i]->GetDynamicFlags().IsFlagSet(WF_BROKEN_OFF) || istate.m_ppWheels[i]->GetDynamicFlags().IsFlagSet(WF_DUMMY)) )
		{
			if(!istate.m_ppWheels[i]->GetConfigFlags().IsFlagSet(WCF_BIKE_WHEEL) && !istate.m_ppWheels[0]->GetConfigFlags().IsFlagSet(WCF_QUAD_WHEEL))
			{
				output.accel += vBiasDownAccelPerWheel;
                output.angAccel += vExtraAngularAccelPerWheel;
			}

			//get the opposite wheel, for use with anti roll bars
			s16 oppositeWheelId = istate.m_ppWheels[i]->GetOppositeWheelIndex();
			CWheel *pOppositeWheel = NULL;
			if(oppositeWheelId >= 0 && oppositeWheelId < istate.m_nNumWheels)
				pOppositeWheel = istate.m_ppWheels[oppositeWheelId];

			istate.m_ppWheels[i]->IntegrateSuspensionAccel(istate.m_pCollider, vecForce, vecAccel, vecAngAccel, fSuspensionForce, initialLocals.compressionDelta[i], newLocals.mat, newLocals.output.vel, timeInfo, pOppositeWheel, fGravity);
			Assert(vecAccel==vecAccel);
			Assert(vecAngAccel==vecAngAccel);
			Assert(vecForce==vecForce);
			output.accel += vecAccel;
			output.angAccel += vecAngAccel;
			output.aWheelForce[i] = vecForce;

			PDR_ONLY(char vehDebugString[1024];)
			PDR_ONLY(sprintf(vehDebugString,"WheelAngAccelPostSuspension %d", i);)
			PDR_ONLY(debugPlayback::RecordTaggedVectorValue(*istate.m_pCollider->GetInstance(), RCC_VEC3V(output.angAccel), vehDebugString, debugPlayback::eVectorTypeVelocity));

			istate.m_ppWheels[i]->IntegrateTyreAccel(istate.m_pCollider, vecForce, vecAccel, vecAngAccel, initialLocals.calcFwd[i], initialLocals.calcSide[i], fWheelAngSlipDt, fSuspensionForce, newLocals.mat, newLocals.output.vel, newLocals.angVel, newLocals.aWheelAngSlip[i], timeInfo, pOppositeWheel, fGravity);
			Assert(vecAccel==vecAccel);
			Assert(vecAngAccel==vecAngAccel);
			Assert(vecForce==vecForce);
			Assert(fWheelAngSlipDt==fWheelAngSlipDt);
			output.accel += vecAccel;
			output.angAccel += vecAngAccel;
			output.aWheelAngSlipDt[i] = fWheelAngSlipDt;
			output.aWheelForce[i] += vecForce;

			PDR_ONLY(sprintf(vehDebugString,"WheelAngAccelPostTyre %d", i);)
			PDR_ONLY(debugPlayback::RecordTaggedVectorValue(*istate.m_pCollider->GetInstance(), RCC_VEC3V(output.angAccel), vehDebugString, debugPlayback::eVectorTypeVelocity));
		}
		else
		{
			output.aWheelForce[i].Zero();
		}
	}
}

void CWheelIntegrator::ApplyDifferentials( CWheel* const* ppWheels, int nNumWheels, float fMaxVelInGear, float fTimeStep )
{
    for( int i = 0; i < nNumWheels; i++ )
    {
        CCarHandlingData* carHandling = ppWheels[ i ]->m_pHandling->GetCarHandlingData();
        float fWheelRadius = ppWheels[ i ]->GetWheelRadius();
        float fMaxAngVelInGear = -fMaxVelInGear / ( fWheelRadius > 0.0f ? fWheelRadius : 1.0f );

        if( carHandling &&
            ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_LEFTWHEEL ) )
        {
            int oppositeWheelIndex = ppWheels[ i ]->GetOppositeWheelIndex();

            if( oppositeWheelIndex != -1 &&
                ( ( ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
                    carHandling->aFlags & CF_DIFF_REAR ) ||
                  ( !ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
                    carHandling->aFlags & CF_DIFF_FRONT ) ) )
            {
                float combinedWheelSpeed = Abs( ppWheels[ i ]->GetRotSpeed() + ppWheels[ oppositeWheelIndex ]->GetRotSpeed() );
                float maxCombinedWheelSpeed = Abs( fMaxAngVelInGear * WHEEL_ANG_VEL_OVERRUN * 2.0f );

                if( combinedWheelSpeed > maxCombinedWheelSpeed )
                {
                    ppWheels[ i ]->SetRotSpeed( ppWheels[ i ]->GetRotSpeed() * ( maxCombinedWheelSpeed / combinedWheelSpeed ) );
                    ppWheels[ oppositeWheelIndex ]->SetRotSpeed( ppWheels[ oppositeWheelIndex ]->GetRotSpeed() * ( maxCombinedWheelSpeed / combinedWheelSpeed ) );
                }

                // if we have a differential a wheel should be able to spin twice as fast as the max speed in gear as the other wheel could be stopped
                // We'll handle this in the differential code so that we know the final wheel speed of both wheels.
                //m_fRotAngVel = Clamp( m_fRotAngVel, -Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN*2.0f ), Abs( fMaxAngVelInGear*WHEEL_ANG_VEL_OVERRUN*2.0f ) );
            }
        }
    }

    TUNE_GROUP_BOOL( WHEEL_SLIP_DIFF, LOCK_DIFFERENTIALS, false );

    for( int i = 0; i < nNumWheels; i++ )
    {
        CCarHandlingData* carHandling = ppWheels[ i ]->m_pHandling->GetCarHandlingData();

        // get the total rotational velocity of the wheel
        if( carHandling &&
            ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_LEFTWHEEL ) )
        {
            int oppositeWheelIndex = ppWheels[ i ]->GetOppositeWheelIndex();

            if( oppositeWheelIndex != -1 &&
                ( ( ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
                    carHandling->aFlags & CF_DIFF_LOCKING_REAR ) ||
                ( !ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
                  carHandling->aFlags & CF_DIFF_LOCKING_FRONT ) ) )
            {
				float lockRatio = 1.0f;

				if( ( ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
						carHandling->aFlags & CF_DIFF_LIMITED_REAR ) ||
						( !ppWheels[ i ]->GetConfigFlags().IsFlagSet( WCF_REARWHEEL ) &&
						carHandling->aFlags & CF_DIFF_LIMITED_FRONT ) )
				{
					float wheelRotSpeed			= ppWheels[ i ]->GetRotSpeed();
					float oppositeWheelRotSpeed = ppWheels[ oppositeWheelIndex ]->GetRotSpeed();

					if( wheelRotSpeed == oppositeWheelRotSpeed )
					{
						lockRatio = 0.0f;
					}
					else
					{
						static dev_float minRotDiffForLocking = 0.1f;
						static dev_float maxRotDiffForLocking = 0.1f;

						float rotDiff = Abs( ( wheelRotSpeed - oppositeWheelRotSpeed ) / ( Abs( wheelRotSpeed ) + Abs( oppositeWheelRotSpeed ) ) );
						lockRatio = Clamp( ( rotDiff - minRotDiffForLocking ) / maxRotDiffForLocking, 0.0f, 1.0f );
					}
				}

				if( LOCK_DIFFERENTIALS || 
					lockRatio > 0.0f )
				{
					// need to recalculate GetRotSlipRatio
					float prevRotSpeed		= ppWheels[ i ]->GetRotSpeed();
					float prevRotSlipRatio	= ppWheels[ i ]->GetRotSlipRatio();

					float oppositePrevSpeed		= ppWheels[ oppositeWheelIndex ]->GetRotSpeed();
					float oppositePrevSlipRatio	= ppWheels[ oppositeWheelIndex ]->GetRotSlipRatio();

					if( ( !ppWheels[ i ]->GetIsTouching() &&
						  !ppWheels[ i ]->GetWasTouching() ) ||
						( !ppWheels[ oppositeWheelIndex ]->GetIsTouching() &&
						  !ppWheels[ oppositeWheelIndex ]->GetWasTouching() ) )
					{
						continue;
					}

					float averageWheelSpeed = ( prevRotSpeed + oppositePrevSpeed ) * 0.5f;

					float groundSpeed = prevRotSpeed + ( prevRotSpeed * prevRotSlipRatio );
					float newWheelSlipRot = ( ( averageWheelSpeed - groundSpeed ) / ( groundSpeed != 0.0f ? groundSpeed : 1.0f ) );// *Sign( prevRotSlipRatio );
					Assert( newWheelSlipRot == newWheelSlipRot );
					Assert( averageWheelSpeed == averageWheelSpeed );
					
					float speedDiff = prevRotSpeed - averageWheelSpeed;
					float slipDiff = prevRotSlipRatio - newWheelSlipRot;

					ppWheels[ i ]->SetRotSpeed( prevRotSpeed - ( speedDiff * lockRatio ) );
					ppWheels[ i ]->SetRotSlipRatio( prevRotSlipRatio - ( slipDiff * lockRatio ) );

					groundSpeed = oppositePrevSpeed + ( oppositePrevSpeed * oppositePrevSlipRatio );
					newWheelSlipRot = ( ( averageWheelSpeed - groundSpeed ) / ( groundSpeed != 0.0f ? groundSpeed : 1.0f ) );// * Sign( oppositePrevSlipRatio );

					Assert( newWheelSlipRot == newWheelSlipRot );

					speedDiff = oppositePrevSpeed - averageWheelSpeed;
					slipDiff = oppositePrevSlipRatio - newWheelSlipRot;

					ppWheels[ oppositeWheelIndex ]->SetRotSpeed( oppositePrevSpeed - ( speedDiff * lockRatio ) );
					ppWheels[ oppositeWheelIndex ]->SetRotSlipRatio( oppositePrevSlipRatio - ( slipDiff * lockRatio ) );

					//if(m_bUpdateWheelRotation)
					{
						float rotAngle = ppWheels[ i ]->GetRotAngle() + ( ppWheels[ i ]->GetRotSpeed() * fTimeStep );
						rotAngle = fwAngle::LimitRadianAngleFast( rotAngle );
						ppWheels[ i ]->SetRotAngle( rotAngle );

						rotAngle = ppWheels[ oppositeWheelIndex ]->GetRotAngle() + ( ppWheels[ oppositeWheelIndex ]->GetRotSpeed() * fTimeStep );
						rotAngle = fwAngle::LimitRadianAngleFast( rotAngle );
						ppWheels[ oppositeWheelIndex ]->SetRotAngle( rotAngle );
					}
				}
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if !__SPU
//
// CWheelInstanceRenderer stuff starts here:
//


class rmcInstanceWheelData : public rmcInstanceDataBase
{
public:
	rmcInstanceWheelData()							{}
	virtual ~rmcInstanceWheelData()					{}
	rmcInstanceWheelData(datResource&)				{}

	IMPLEMENT_PLACE_INLINE(rmcInstanceWheelData);


public:
	virtual int				GetRegisterCount() const		{ return( (sizeof(m_WorldMatrix)/16) + (sizeof(m_ProjViewWorld)/16) + (sizeof(m_tyreDeformParams)/16) + (sizeof(m_tyreDeformParams2)/16) );	}
	virtual /*const*/ float*	GetRegisterPointer() const	{ return( (/*const*/ float*)&m_WorldMatrix	);		}


	virtual const Matrix44& GetWorldMatrix() const			{ return(m_WorldMatrix);		}
	const Vector4&	        GetTyreDeformParams()	const	{ return(m_tyreDeformParams);	}
	const Vector4&	        GetTyreDeformParams2()	const	{ return(m_tyreDeformParams2);	}

	virtual void SetWorldMatrix(const Matrix44& m)
	{ 
		this->m_WorldMatrix = m;

#if GS_INSTANCED_SHADOWS
		if (!grcViewport::GetInstancing())
#endif
		{
			Mat44V M	= MATRIX44_TO_MAT44V(m);
			Mat44V Proj = grcViewport::GetCurrent()->GetProjection();
			Mat44V View = grcViewport::GetCurrent()->GetViewMtx();

			Mat44V ModelView;
			Multiply(ModelView, View, M);
			Multiply(m_ProjViewWorld, Proj, ModelView);
		}
	}

	//
	// tyreDeformParams (wheel local coords):
	// x = tyreDeformScaleX:		"left-right" side deform scale <0;1> (bigger = tyre more flat)
	// y = tyreDeformScaleYZ:		"up-down" deform scale towards alloy <0; 0.20f> (bigger = tyre more sticks to alloy)
	// z = tyreDeformContactPointX:	balances flat tyre point: 0=none, >0 - goes outside, <0 - goes inside
	// w = drawTyre:				1.0=yes, 0.0=no
	//
	void SetTyreDeformParams(const Vector4& tyreDeform)		{	this->m_tyreDeformParams = tyreDeform;	}

	//
	// tyreDeformParams2:
	// x = tyreInnerRadius (local coords)
	// y = groundPosZ (worldSpace)
	// z = 0.0f
	// w = 0.0f;
	//
	void SetTyreDeformParams2(const Vector4& tyreDeform2)	{	this->m_tyreDeformParams2 = tyreDeform2;	}



public:
	// things shared among all instances:
	static grmModel*		GetInstModel()									{ return(ms_pModel);						}
	static void				SetInstModel(const grmModel *m)					{ ms_pModel=(grmModel*)m;				}
	static grmShaderGroup*	GetInstShaderGroup()							{ return(ms_pShaderGroup);				}
	static void				SetInstShaderGroup(const grmShaderGroup *sg)	{ ms_pShaderGroup=(grmShaderGroup*)sg;	}
	static s32				GetInstBucket()									{ return(ms_shaderBucket);				}
	static void				SetInstBucket(s32 b)							{ ms_shaderBucket=b;					}


private:
	static DECLARE_MTR_THREAD grmModel			*ms_pModel;			// geometry model used to draw instanced stuff
	static DECLARE_MTR_THREAD grmShaderGroup	*ms_pShaderGroup;	// shadergroup used to draw instanced stuff
	static DECLARE_MTR_THREAD s32				ms_shaderBucket;	// shaderbucket used by instanced models

protected:
	Matrix44				m_WorldMatrix;
	Mat44V					m_ProjViewWorld;
	Vector4					m_tyreDeformParams;
	Vector4					m_tyreDeformParams2;

};

DECLARE_MTR_THREAD grmModel*		rmcInstanceWheelData::ms_pModel = NULL;		// geometry model used to draw instanced stuff
DECLARE_MTR_THREAD grmShaderGroup*	rmcInstanceWheelData::ms_pShaderGroup=NULL;	// shadergroup used to draw instanced stuff
DECLARE_MTR_THREAD s32			rmcInstanceWheelData::ms_shaderBucket=0;


//
//
//
////////////////////////////////////////////////////////////////////////


#define WHEEL_PREINST_RENDERER_MAX_INSTANCES		(12)	// max 12 instanced wheels per 1 vehicle

s32 CWheelInstanceRenderer::ms_currInstanceIndices[NUMBER_OF_RENDER_THREADS];
rmcInstanceWheelData* CWheelInstanceRenderer::ms_pInstances[NUMBER_OF_RENDER_THREADS];
atArray<datRef<const rmcInstanceDataBase> >	CWheelInstanceRenderer::ms_instArrays[NUMBER_OF_RENDER_THREADS];
#if __D3D11 || RSG_ORBIS
CWheelInstanceData_UsageInfo CWheelInstanceRenderer::ms_InstanceUsageInfos[NUMBER_OF_RENDER_THREADS];
#endif//__D3D11 || RSG_ORBIS

//
//
//
//
bool CWheelInstanceRenderer::Initialise()
{
	if(!ms_pInstances[0])
	{
		// allocate wheel instances array:
		for (int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
		{
			ms_pInstances[i] = rage_new rmcInstanceWheelData[WHEEL_PREINST_RENDERER_MAX_INSTANCES];
			ms_instArrays[i].Reset();
			ms_instArrays[i].Reserve(WHEEL_PREINST_RENDERER_MAX_INSTANCES);
		}
	}
	return(TRUE);
}

//
//
//
//
void CWheelInstanceRenderer::Shutdown()
{
	for (int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
	{
		delete[] ms_pInstances[i];
		ms_pInstances[i] = NULL;
		ms_instArrays[i].Reset();
	}
}



//
//
// call this once before every wheel render loop:
//
void CWheelInstanceRenderer::InitBeforeRender()
{
//	CWheelInstanceRenderer::Initialise();	// initialise if necessary
	Assert(ms_pInstances[0]);	// make sure Initialise() was called

	ms_currInstanceIndices[g_RenderThreadIndex] = 0;
	ms_instArrays[g_RenderThreadIndex].ResetCount();
}

//
//
// adds 1 wheel instance to render:
//
void CWheelInstanceRenderer::AddWheelModel(const Matrix34& childMatrix,
								const grmModel *pModel, const grmShaderGroup *pShaderGroup, s32 shaderBucket,
								Vector4::Vector4Param tyreDeformParams, Vector4::Vector4Param tyreDeformParams2)
{
	Assert(pModel);
	Assert(pShaderGroup);

	// create the instance matrix
	Matrix44 instMat;
	instMat.FromMatrix34(childMatrix);

	// create the instance data and set the matrix
	new(&ms_pInstances[g_RenderThreadIndex][ms_currInstanceIndices[g_RenderThreadIndex]]) rmcInstanceWheelData();
	rmcInstanceWheelData *pWheelInstData = &ms_pInstances[g_RenderThreadIndex][ms_currInstanceIndices[g_RenderThreadIndex]];
	pWheelInstData->SetWorldMatrix(instMat);
	pWheelInstData->SetTyreDeformParams(tyreDeformParams);
	pWheelInstData->SetTyreDeformParams2(tyreDeformParams2);

	// add the instance data to the instance array
	ms_instArrays[g_RenderThreadIndex].Push(ms_pInstances[g_RenderThreadIndex]+ms_currInstanceIndices[g_RenderThreadIndex]);

	if(!ms_currInstanceIndices[g_RenderThreadIndex])
	{	// first model in render loop:
		rmcInstanceWheelData::SetInstModel(pModel);
		rmcInstanceWheelData::SetInstShaderGroup(pShaderGroup);
		rmcInstanceWheelData::SetInstBucket(shaderBucket);

	#if __D3D11 || RSG_ORBIS
		// DX11 TODO:- Pre-calculate these hashes.
		static u32 matWheelTransformHash = ATSTRINGHASH("matWheelWorld", 0xBD720B18);
		static u32 matWheelTransformHash_Instance = ATSTRINGHASH("matWheelTransform_Instance", 0xBAA42BE6);

		static u32 tyreDeformParams = ATSTRINGHASH("tyreDeformParams", 0x5DC44EB2);
		static u32 tyreDeformParams_Instance = ATSTRINGHASH("tyreDeformParams_Instance", 0xDE595608);

		// DX11 TODO:- Move this to better place to optimize the LookupVarForInstancing(). CVehicleModelInfo maybe.

		// Set up which constant buffers the instance data is sent to.
		ms_InstanceUsageInfos[g_RenderThreadIndex].m_ShaderGroupVariables[0] = pShaderGroup->LookupVarForInstancing(matWheelTransformHash, matWheelTransformHash_Instance);
		ms_InstanceUsageInfos[g_RenderThreadIndex].m_ShaderGroupVariables[1] = pShaderGroup->LookupVarForInstancing(tyreDeformParams, tyreDeformParams_Instance);
	#endif //__D3D11
	}
	else
	{	// make sure all instanced models use the same model & shadowgroup:
		Assert(rmcInstanceWheelData::GetInstModel()			== pModel);
		Assert(rmcInstanceWheelData::GetInstShaderGroup()	== pShaderGroup);
		Assert(rmcInstanceWheelData::GetInstBucket()		== shaderBucket);
	}


	// one more instance has been added
	ms_currInstanceIndices[g_RenderThreadIndex]++;

}// end of AddWheelModel()....



//
//
//
//
void CWheelInstanceRenderer::RenderAllWheels()
{
	if(ms_instArrays[g_RenderThreadIndex].GetCount())
	{
		grcViewport::SetCurrentWorldIdentity();
	#if !(__D3D11 || RSG_ORBIS)
		rmcInstance::InstDrawUnskinned(&ms_instArrays[g_RenderThreadIndex], rmcInstanceWheelData::GetInstModel(), *rmcInstanceWheelData::GetInstShaderGroup(), rmcInstanceWheelData::GetInstBucket(), 64);
	#else //!(__D3D11 || RSG_ORBIS)
		rmcInstance::InstDrawUnskinned(&ms_instArrays[g_RenderThreadIndex], rmcInstanceWheelData::GetInstModel(), *rmcInstanceWheelData::GetInstShaderGroup(), rmcInstanceWheelData::GetInstBucket(), &ms_InstanceUsageInfos[g_RenderThreadIndex]);
	#endif //!(__D3D11 || RSG_ORBIS)
		ms_currInstanceIndices[g_RenderThreadIndex] = 0;
		ms_instArrays[g_RenderThreadIndex].ResetCount();
	}
}// end of RenderAllWheels()...

#endif //!__SPU...

