//
// Title	:	Bike.cpp
// Author	:	Richard Jobling/William Henderson/Alexander Roger
// Started	:	25/08/99 (from Automobile.cpp)
//

// Rage headers 
#include "crskeleton/skeleton.h"
#include "grcore/texture.h"
#include "math/vecMath.h"
#include "physics/sleep.h"
#include "physics/constraintrotation.h"
#include "profile/timebars.h"
#include "phBound/support.h"
#include "phbound/boundcomposite.h"

#if __BANK
#include "bank/bank.h"
#endif

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/random.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwsys/timer.h"

// Game headers
#include "animation/AnimDefines.h"
#include "animation/MoveVehicle.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/Task/TaskVehicleMissionBase.h"
#include "control/gamelogic.h"
#include "control/garages.h"
#include "control/record.h"
#include "control/remote.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "event/EventDamage.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "game/modelIndices.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsInterface.h"
#include "game/zones.h"
#include "modelInfo/modelInfo.h"
#include "modelInfo/vehicleModelInfo.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/NetworkInterface.h"
#include "Peds/PedDebugVisualiser.h"
#include "peds/pedIntelligence.h"
#include "peds/Ped.h"
#include "peds/PlayerInfo.h"
#include "peds/QueriableInterface.h"
#include "physics/gtaArchetype.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "profile/cputrace.h"
#include "renderer/renderer.h"
#include "renderer/zoneCull.h"
#include "script/script.h"
#include "system/controlMgr.h"
#include "system/pad.h"	
#include "task/vehicle/taskexitvehicle.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicles/automobile.h"
#include "vehicles/bike.h"
#include "vehicles/handlingMgr.h" 
#include "vehicles/vehiclepopulation.h" 
#include "vehicles/virtualroad.h" 
#include "vehicles/VehicleGadgets.h"
#include "vehicles/VehicleFactory.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/Systems/VfxVehicle.h"
#include "weapons/explosion.h"
#include "weapons/weapondamage.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "scene/world/GameWorld.h"
#include "network/Objects/Entities/NetObjPhysical.h"
#include "event/EventNetwork.h"
#include "system/PadGestures.h"


AI_VEHICLE_OPTIMISATIONS()
ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS() 


#define BIKE_ON_FIRE_HEALTH (250)
#define BIKE_SUSPENSION_INI (99999.99f)
//#define BIKE_RAILTRACK_SURFACETYPE (SURFACE_TYPE_GRAVEL)
#define BIKE_RAILTRACK_SURFACETYPE (SURFACE_TYPE_RAILTRACK)

float BIKE_ON_STAND_STEER_ANGLE = 0.3f;

// How long until the engine cuts out in water
dev_float BIKE_DROWN_ENGINE_TIME = 5.0f;

bank_float CBike::ms_fFoliageBikeDragCoeff = 0.5f;
bank_float CBike::ms_fFoliageBikeDragDistanceScaleCoeff = 0.8f;
bank_float CBike::ms_fFoliageBikeBoundRadiusScaleCoeff = 0.05f;

bank_float CBike::ms_fMaxTimeUnstableBeforeKnockOff = 0.5f;

bank_bool CBike::ms_bNewLeanMethod = false;

bank_bool CBike::ms_bLeanBikeWhenDoingBurnout = true;

bank_float CBike::ms_fBikeWaterHeightTolerance = 0.4f;

bank_float CBike::ms_fBikeJumpFullPowerTime = 500.0f;


#if USE_SIXAXIS_GESTURES
bank_float CBike::ms_fMotionControlPitchMin = -0.8f;
bank_float CBike::ms_fMotionControlPitchMax = 0.8f;
bank_float CBike::ms_fMotionControlRollMin = -0.5f;
bank_float CBike::ms_fMotionControlRollMax = 0.5f;
bank_float CBike::ms_fMotionControlPitchDownDeadzone = -0.5f;
bank_float CBike::ms_fMotionControlPitchUpDeadzone = 0.7f;
#endif

bool CBike::ms_bEnableSkeletonFixup = true;
float CBike::ms_fSkeletonFixupMaxRot = 0.05f;
float CBike::ms_fSkeletonFixupMaxTrans = 0.05f;
float CBike::ms_fSkeletonFixupMaxRotChange = 0.01f;
float CBike::ms_fSkeletonFixupMaxTransChange = 0.01f;

bank_bool CBike::ms_bSecondNewLeanMethod = true;
bank_float CBike::ms_fSecondNewLeanZAxisCosine = 0.5f;			// 60 degrees
bank_float CBike::ms_fSecondNewLeanGroundNormalCosine = 0.15f;	// ~81 degrees
bank_float CBike::ms_fSecondNewLeanMaxAngularVelocityGroundSlow = 1.3f;
bank_float CBike::ms_fSecondNewLeanMaxAngularVelocityGroundFast = 2.5f;
bank_float CBike::ms_fSecondNewLeanMaxAngularVelocityAirSlow = 0.3f;
bank_float CBike::ms_fSecondNewLeanMaxAngularVelocityAirFast = 0.9f;
bank_float CBike::ms_fSecondNewLeanMaxAngularVelocityPickup = 3.0f;

bank_float CBike::ms_fMaxGroundTiltAngleSine = 0.94f;			// 70 degrees
bank_float CBike::ms_fMinAbsoluteTiltAngleSineForKnockoff = 0.5f; // 30 degrees

bank_float CBike::ms_TimeBeforeLeavingStuntMode = 1.0f;

static Vector3 BIKE_ROT_DAMP_V(0.3f, 0.3f, 0.4f);
static Vector3 BIKE_ROT_DAMP_V2(0.1f, 0.1f, 0.5f);

static Vector3 BIKE_NEW_ROT_DAMP_C(0.04f,0.01f, 0.02f);
static Vector3 BIKE_NEW_ROT_DAMP_V(2.3f, 0.3f, 1.2f);
static Vector3 BIKE_NEW_ROT_DAMP_V2(0.7f, 0.1f, 2.5f);
static Vector3 BIKE_NEW_ROT_DAMP_V2_IN_AIR(0.7f, 0.1f, 0.7f);

static Vector3 BIKE_NEW_LINEAR_DAMP_C(0.01f,0.01f, 0.01f);
static Vector3 BIKE_NEW_LINEAR_DAMP_V(0.02f, 0.02f, 0.00f);

static Vector3 BICYCLE_NEW_ROT_DAMP_C(0.06f,0.01f, 0.02f);
static Vector3 BICYCLE_NEW_ROT_DAMP_V(3.3f, 0.3f, 1.2f);
static Vector3 BICYCLE_NEW_ROT_DAMP_V2(0.7f, 0.1f, 0.7f);

static Vector3 BICYCLE_NEW_LINEAR_DAMP_C(0.00f,0.00f, 0.00f);
static Vector3 BICYCLE_NEW_LINEAR_DAMP_V(0.02f, 0.02f, 0.02f);
static Vector3 BICYCLE_NEW_LINEAR_DAMP_V_IN_AIR(0.02f, 0.02f, 0.00f);

bool CBike::ms_bDisableTrickForces = false;

PF_PAGE(GTA_BikeDynamics, "Bike Dynamics");
PF_GROUP(Bike_Dynamics);
PF_LINK(GTA_BikeDynamics, Bike_Dynamics);


PF_VALUE_FLOAT(BikeSteerInput, Bike_Dynamics);
PF_VALUE_FLOAT(BikeLean, Bike_Dynamics);

PF_VALUE_FLOAT(BikeSideAng, Bike_Dynamics);


//PF_VALUE_FLOAT(BikeRearWheelCompression, Bike_Dynamics);
//PF_VALUE_FLOAT(BikeRearWheelTouching, Bike_Dynamics);

//PF_VALUE_FLOAT(BikeGroundSpeed, Bike_Dynamics);

//PF_VALUE_FLOAT(BikeGear, Bike_Dynamics);
//PF_VALUE_FLOAT(BikeDriveForce, Bike_Dynamics);

#define MANCHEZ2_AMBIENT_OCCLUDER_OFFSET (Vec3V(0.0f, 0.0f, -0.05f))
BANK_SWITCH_NT(static, static const) Vec3V Manchez2_AmbientVolume_Offset = MANCHEZ2_AMBIENT_OCCLUDER_OFFSET;

//
//
//
CBike::CBike(const eEntityOwnedBy ownedBy, u32 popType, VehicleType vehType) : CVehicle(ownedBy, popType, vehType),
m_vecAverageGroundNormal(0.0f,0.0f,1.0f)
{
	m_nBikeFlags.bWaterTight = false;
	m_nBikeFlags.bGettingPickedUp = false;
	m_nBikeFlags.bOnSideStand = false;
	m_nBikeFlags.bJumpPressed = false;
	m_nBikeFlags.bJumpReleased = false;
	m_nBikeFlags.bLateralJump = false;
	m_nBikeFlags.bHasFrontSuspension = m_nBikeFlags.bHasRearSuspension = false;
	m_nBikeFlags.bJumpOffTrain = false;
	m_nBikeFlags.bInputsUpToDate = false;
	m_nBikeFlags.bUsingMouseSteeringInput = false;

	m_fHeightAboveRoad = 0.0f;

	// Init some AI stuff
	GetIntelligence()->LastTimeNotStuck = fwTimer::GetTimeInMilliseconds();	// So that it doesn't reverse straight away.
	GetIntelligence()->LastTimeHonkedAt = 0;

	// oops, need to make sure these are initialised!
	m_nVehicleFlags.bIsVan = false;
	m_nVehicleFlags.bIsBig = false;
	m_nVehicleFlags.bIsBus = false;
	m_nVehicleFlags.bLowVehicle = false;

	m_nVehicleFlags.bUseDeformation = false;

	m_fLeanFwdInput   = 0.0f;
	m_fLeanSideInput  = 0.0f;

    m_fAnimLeanFwd    = 0.0f;
    m_fAnimSteerRatio = 0.0f;

	m_fGroundLeanAngle = 0.0f;
	m_fBikeLeanAngle = 0.0f;

	m_vecFwdDirnStored.Set(YAXIS);
	m_vecBikeWheelUp.Zero();
	m_fFwdDirnMult = 0.0f;

	m_fTimeUnstable = 0.0f;

	m_fTimeInWater = 0.0f;

	m_uHandbrakeLastPressedTime = 0;
	m_uJumpLastPressedTime = 0;
	m_uJumpLastReleasedTime = 0;
	m_fJumpLaunchPitch = 0.0f;
	m_fJumpLaunchHeading = 0.0f;

	m_fOffStuntTime = 0.0f;

	m_fOnTrainLaunchDesiredPitch = 0.0f;

	for(int i = 0; i < NUM_BIKE_CWHEELS_MAX; i++)
	{
		m_pBikeWheels[i] = NULL;
	}

    m_RotateUpperForks = false;

	m_fRearBrake = 0.0f;

	m_fCurrentGroundClearanceAngle = FLT_MAX;
	m_fCurrentGroundClearanceHeight = FLT_MAX;
	
	m_vecSkeletonFixupPreviousTranslation = Vec3V(V_ZERO);
	m_fSkeletonFixupPreviousAngle = 0.0f;

	m_fBurnoutTorque = 0.0f;
}
//////////////////////////////////////////////////////////////////////

//
//
//
CBike::~CBike()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	for(int i = 0; i < NUM_BIKE_CWHEELS_MAX; i++)
	{
		if(m_pBikeWheels[i])
		{
			delete m_pBikeWheels[i];
			m_pBikeWheels[i] = NULL;
		}
	}
}

int CBike::InitPhys()
{
	CVehicle::InitPhys();

	m_fOrigBuoyancyForceMult = m_Buoyancy.m_fForceMult;

	InitConstraint();

	// sets m_nBikeFlags.bOnSideStand to false, also sets steer and lean angles
	SetBikeOnSideStand(true, true);

    // Check if we need to rotate the upper forks separately from the handlebars
    if(GetSkeleton())
    {
        crSkeleton* pSkel = GetSkeleton();
        int nForksUpper = GetBoneIndex(BIKE_FORKS_U);
        if(nForksUpper != -1)
        {
            const crBoneData* pBoneDataF = pSkel->GetSkeletonData().GetBoneData(nForksUpper);
            if(pBoneDataF)
            {
                const crBoneData* pParentBoneDataF = pBoneDataF->GetParent();

                const crBoneData* pForkBoneData = NULL;
                if(GetBoneIndex(VEH_STEERING_WHEEL) != -1)
                {
                    pForkBoneData = pSkel->GetSkeletonData().GetBoneData(GetBoneIndex(VEH_STEERING_WHEEL));
                }

                if(pForkBoneData && pForkBoneData!=pParentBoneDataF)
                { 
                    m_RotateUpperForks = true; 
                }
            }
        }

		// Hacky way of determining if we have front and rear suspension. Better here than sprinkled throughout Bike code
		m_nBikeFlags.bHasFrontSuspension = (GetBoneIndex(BIKE_FORKS_L) != -1);
		m_nBikeFlags.bHasRearSuspension = (pSkel->GetSkeletonData().GetBoneData(GetBoneIndex(BIKE_WHEEL_R))->GetParent()->GetIndex() != 0);
    }

	static dev_bool sbOverrideAngInertia = true;
	static dev_float sfSolverInvAngularMultX = 1.0f;
	static dev_float sfSolverInvAngularMultY = 0.0f;

	if(sbOverrideAngInertia && GetCollider() && !CPhysics::ms_bInStuntMode && 
		( !pHandling->GetSpecialFlightHandlingData() || HasGlider() ) )
	{
		Vector3 vecSolverAngularInertia = VEC3V_TO_VECTOR3(GetCollider()->GetInvAngInertia());
		vecSolverAngularInertia.x *= sfSolverInvAngularMultX;
		vecSolverAngularInertia.y *= sfSolverInvAngularMultY;
		vecSolverAngularInertia.z = 0.0f;

		GetCollider()->SetSolverInvAngInertiaResetOverride(vecSolverAngularInertia);
	}

	// Speed up the Sanchez in MP
	if(GetModelIndex() == MI_BIKE_SANCHEZ || GetModelIndex() == MI_BIKE_SANCHEZ2 )
	{
		if(NetworkInterface::IsGameInProgress())
		{
			CVehicleFactory::ModifyVehicleTopSpeed(this, SANCHEZ_MP_DRIVE_FORCE_ADJUSTMENT);
			if(pHandling)
			{
				pHandling->m_fTractionLossMult = 0.0f;
			}
		}
		else
		{
			//return the traction loss mult to the default.
			if(pHandling)
			{
				pHandling->m_fTractionLossMult = 0.5f;
			}
		}
	}
	

	if( GetModelIndex() == MI_BIKE_DEATHBIKE2 )
	{
		m_nVehicleFlags.bCanBreakOffWheelsWhenBlowUp = false;
	}

	return INIT_OK;
}

void CBike::RemovePhysics()
{
    CVehicle::RemovePhysics();
}

void CBike::InitDummyInst()
{
	Assert(!m_pDummyInst);
	Assert(GetVehicleModelInfo()->GetPhysics());

	// Check to see if this vehicle has the dummy bound already in the frag type, in which case we don't need a dummy inst
	if (HasDummyBound())
	{
		return;
	}

	const Matrix34 matrix = MAT34V_TO_MATRIX34(GetMatrix());
	phInstGta* pNewInst = rage_new phInstGta(PH_INST_VEH);
	Assert(pNewInst);
	pNewInst->Init(*GetVehicleModelInfo()->GetPhysics(), matrix);
	Assert(pNewInst->GetArchetype());
	Assert(pNewInst->GetArchetype()->GetTypeFlags() == ArchetypeFlags::GTA_BOX_VEHICLE_TYPE);

	m_pDummyInst = pNewInst;
	m_pDummyInst->SetUserData((void*)this);
	m_pDummyInst->SetInstFlag(phfwInst::FLAG_USERDATA_ENTITY, true);
}

void CBike::InitWheels()
{
	m_ppWheels = m_pBikeWheels;

	// initialise wheels
	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)GetBaseModelInfo();

	m_nNumWheels = 0;
	int nFlags = 0;

	m_ppWheels[m_nNumWheels] = rage_new CWheel();
	if (Verifyf(m_ppWheels[m_nNumWheels],"Could not create a wheel! Wheel pool size probably needs increasing"))
	{
		nFlags = WCF_POWERED|WCF_REARWHEEL|WCF_BIKE_WHEEL;
		if(GetVehicleType()==VEHICLE_TYPE_BICYCLE)
		{
			nFlags |= WCF_BICYCLE_WHEEL;
		}
		else
		{
			nFlags |= WCF_HIGH_FRICTION_WHEEL;
		}
		m_ppWheels[m_nNumWheels]->Init(this, BIKE_WHEEL_R, pModelInfo->GetRimRadius(false), 0.5f*pHandling->GetSuspensionBias(-1.0f), nFlags, 1);
		m_nNumWheels ++;
	}

	m_ppWheels[m_nNumWheels] = rage_new CWheel();
	if (Verifyf(m_ppWheels[m_nNumWheels],"Could not create a wheel! Wheel pool size probably needs increasing"))
	{
		nFlags = WCF_STEER|WCF_BIKE_WHEEL;
		if(GetVehicleType()==VEHICLE_TYPE_BICYCLE)
		{
			nFlags |= WCF_BICYCLE_WHEEL;
		}
		else
		{
			nFlags |= WCF_HIGH_FRICTION_WHEEL;
		}
		m_ppWheels[m_nNumWheels]->Init(this, BIKE_WHEEL_F, pModelInfo->GetRimRadius(true), 0.5f*pHandling->GetSuspensionBias(1.0f), nFlags, 0);
		m_ppWheels[m_nNumWheels]->SetSteerAngle(GetSteerAngle());
		m_nNumWheels++;
	}

	// now need to setup wheels (suspension and stuff)
	SetupWheels(NULL);
	InitCacheWheelsById();

	float fTempHeight;
	CalculateHeightsAboveRoad(GetModelId(), &m_fHeightAboveRoad, &fTempHeight);

#ifdef CHECK_VEHICLE_SETUP
#if !__NO_OUTPUT
    if(GetWheel(0) && GetWheel(1))
    {
        float fFrontLength = rage::Abs(GetWheel(0)->GetProbeSegment().A.y);
        float fRearLength = rage::Abs(GetWheel(1)->GetProbeSegment().A.y);

        float fWheelbase = fFrontLength + fRearLength;

        float fSuspensionBiasChange = (fWheelbase+pHandling->m_vecCentreOfMassOffset.GetYf())/fWheelbase;

        modelinfoDisplayf( "%s If Centered Suggested Suspension Bias: %.2f", GetModelName(), fSuspensionBiasChange-0.5f);
        modelinfoDisplayf( "%s Suggested Suspension Bias: %.2f Current: %.2f ", GetModelName(), (fRearLength+pHandling->m_vecCentreOfMassOffset.GetYf())/fWheelbase, pHandling->m_fSuspensionBiasFront*0.5f );
    }
#endif // !__NO_OUTPUT
#endif
}

dev_bool sbForceBikeCG = true;
//
void CBike::InitCompositeBound()
{
	CVehicle::InitCompositeBound();

	phBoundComposite* pBoundComp = (phBoundComposite*)GetVehicleFragInst()->GetArchetype()->GetBound();

	if(GetModelIndex() == MI_BIKE_THRUST)// The exhaust pipe on the Thrust is colliding with the ground and knocking people off their bikes, so make it not collide with the ground.
	{
		u32 nIncludeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_INCLUDE_TYPES;
		nIncludeFlags &= ~ArchetypeFlags::GTA_ALL_MAP_TYPES;
		pBoundComp->SetIncludeFlags(0, nIncludeFlags);
	}

	if(sbForceBikeCG)
		pBoundComp->SetCGOffset(pHandling->m_vecCentreOfMassOffset);
}

void CBike::SetupWheels(const void* basePtr)
{
	for(int i=0; i<GetNumWheels(); i++)
	{
		GetWheel(i)->SuspensionSetup(basePtr);
	}

	if(!IsColliderArticulated())
	{	
		// If the suspension changed, the maximum extents used by non-articulated vehicles needs to changes as well.
		CalculateNonArticulatedMaximumExtents();
	}
}


bool CBike::IsInAir(bool bCheckForZeroVelocity) const
{
    if(GetIsAnyFixedFlagSet())
        return false;

    if (GetIsInWater() || HasContactWheels() || (GetVelocity().IsZero() && bCheckForZeroVelocity))
        return false;

    return true;
}

void CBike::ProcessProbes(const Matrix34& testMat)
{
	if(m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
	{
		return;
	}

	bool bIsDummy = IsDummy();

	if(!bIsDummy && GetWheel(0)->GetFragChild() > 0)
	{
		m_nVehicleFlags.bDontSleepOnThisPhysical = false;
		for(int i=0; i<GetNumWheels(); i++)
			GetWheel(i)->ProcessImpactResults();

		return;
	}

	static phSegment testSegment[NUM_BIKE_CWHEELS_MAX];
	static WorldProbe::CShapeTestFixedResults<NUM_BIKE_CWHEELS_MAX> testResults;

	testResults.Reset();

	for(int i=0; i<GetNumWheels(); i++)
	{
		GetWheel(i)->ProbeGetTransfomedSegment(&testMat, testSegment[i]);
	}

	const CVehicleFollowRouteHelper* pFollowRoute = NULL;
	if(bIsDummy)
	{
		//CVehicleNodeList * pNodeList = GetIntelligence()->GetNodeList();
		//if(!pNodeList || pNodeList->GetPathNodeAddr(pNodeList->GetTargetNodeIndex()).IsEmpty())

		//if we're on the back of a trailer, look for the cab's follow route
		if (m_nVehicleFlags.bHasParentVehicle && CVehicle::IsEntityAttachedToTrailer(this) && GetDummyAttachmentParent()
			&& GetDummyAttachmentParent()->GetDummyAttachmentParent())
		{
			pFollowRoute = GetDummyAttachmentParent()->GetDummyAttachmentParent()->GetIntelligence()->GetFollowRouteHelper();
		}
		else
		{
			pFollowRoute = GetIntelligence()->GetFollowRouteHelper();
		}

		if (!pFollowRoute)
		{
			// If our road nodes are empty then we can't use the virtual road surface
			// So switch back to a real car now
			if(!TryToMakeFromDummy())
			{
				// We couldn't switch back, so have no road to collide with
				// Just turn off physics or we'll fall through the map
				DeActivatePhysics();
			}
		}
	}

	if(bIsDummy)
	{
		Assert( !m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy) );

		if (pFollowRoute)
		{
			CVirtualRoad::TestProbesToVirtualRoadFollowRouteAndCenterPos(*pFollowRoute, GetVehiclePosition(), testResults, testSegment, GetNumWheels(), GetTransform().GetC().GetZ(), ShouldTryToUseVirtualJunctionHeightmaps());
			HandleDummyProbesBottomingOut(testResults);
		}
	}
	else
	{
		int nTestTypes = (ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
		static bool USE_PROBE_BATCH = false;
		if(USE_PROBE_BATCH)
		{
			//Vector3 vecCenter;
			//testMat.Transform(m_vecSegsCenterOffset, vecCenter);
			//Matrix34 matSegBox = testMat;
			//matSegBox.d = m_vecBoxHalfExtents;

			WorldProbe::CShapeTestBatchDesc batchProbeDesc;
			batchProbeDesc.SetExcludeInstance(GetCurrentPhysicsInst());
			batchProbeDesc.SetIncludeFlags(nTestTypes);
			batchProbeDesc.SetExcludeInstance(GetCurrentPhysicsInst());
			ALLOC_AND_SET_PROBE_DESCRIPTORS(batchProbeDesc,GetNumWheels());
			for(int i=0; i<GetNumWheels(); ++i)
			{
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&testResults);
				probeDesc.SetFirstFreeResultOffset(i);
				probeDesc.SetMaxNumResultsToUse(1);
				probeDesc.SetStartAndEnd(testSegment[i].A,testSegment[i].B);

				batchProbeDesc.AddLineTest(probeDesc);
			}
			WorldProbe::GetShapeTestManager()->SubmitTest(batchProbeDesc);
		}
		else
		{
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetResultsStructure(&testResults);
			probeDesc.SetExcludeInstance(GetCurrentPhysicsInst());
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc.SetTypeFlags(TYPE_FLAGS_ALL);
			for(int i=0; i<GetNumWheels(); ++i)
			{
				probeDesc.SetStartAndEnd(testSegment[i].A, testSegment[i].B);
				probeDesc.SetFirstFreeResultOffset(i);
				probeDesc.SetMaxNumResultsToUse(1);

				WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			}		
		}
	}

	for(int i=0; i<GetNumWheels(); i++)
	{
		GetWheel(i)->ProbeProcessResults(&testMat, testResults[i]);
	}

	// Check if we should move the vehicle bounding box to prevent
	// ground penetrations.
	if(CVehicleAILodManager::ms_convertUseFixupCollisionWithWheelSurface)
	{
		if(bIsDummy)
		{
			FixupIntersectionWithWheelSurface(testResults);
		}
	}

	m_nVehicleFlags.bVehicleColProcessed = true;
}

bool CBike::IsUnderwater() const
{
	if(GetIsInWater() && (m_Buoyancy.GetAbsWaterLevel() - GetTransform().GetPosition().GetZf()) > ms_fBikeWaterHeightTolerance)
	{
		return true;
	}

	return false;
}

void CBike::InitDoors()
{
	CVehicle::InitDoors();

	m_pDoors = m_aBikeDoors;

	m_pDoors[0].Init(this, VEH_DOOR_DSIDE_F, 0.0f, -0.5f*PI, CCarDoor::AXIS_Y|CCarDoor::BIKE_DOOR);
	m_pDoors[0].SetBikeDoorConstrained();
	m_nNumDoors = 1;
}



// remember there's a car version of this code as well
dev_float sfBikeNotSlowResistanceMult = 0.9f;
dev_float sfBikeSlowResistanceMult = 1.0f;
dev_float sfBikeSlowResistanceMultNetwork = 1.0f;
dev_float sfBikeRiderResistanceMultMax = 1.0f;
dev_float sfBikeRiderResistanceMultMin = 0.90f;
//
void CBike::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), this );	
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

//	m_nFixLeftHand = false;
//	m_nFixRightHand = false;

	// service the audio entity - this should be done once a frame 
	// we want to ALWAYS do it, even if car is totally stationary and inert
//	m_VehicleAudioEntity.Update();
//	CCarAI::ClearIndicatorsForCar(this);
	if (PopTypeGet() == POPTYPE_RANDOM_PARKED && GetDriver())
	{
		// The vehicle now has a driver so it is no longer just a placed vehicle (as it
		// can drive away now).
		PopTypeSet(POPTYPE_RANDOM_AMBIENT);
	}

	// Generate a horn shocking event, if the horn is being played
	if(IsHornOn() && ShouldGenerateMinorShockingEvent())
	{
		CEventShockingHornSounded ev(*this);
		CShockingEventsManager::Add(ev);
	}
	
	ProcessCarAlarm(fFullUpdateTimeStep);
	UpdateInSlownessZone();

	ProcessSirenAndHorn(true);

	if(GetStatus() == STATUS_PLAYER)
	{
		ProcessDirtLevel();
	}

	//TODO
	// if peds haven't been warned of this cars approach by now - do it!
/*	if((!m_nVehicleFlags.bWarnedPeds))
	{
		CCarAI::ScanForPedDanger(this, GetIntelligence()->FindUberMissionForCar() );
	}
*/

	if (!m_nVehicleFlags.bHasProcessedAIThisFrame)
	{
		ProcessIntelligence(fullUpdate, fFullUpdateTimeStep);
	}

	CalcAnimSteerRatio();
	
	// there's a limit to how far a bike on it's stand can lean over!
	if(m_nBikeFlags.bOnSideStand)
	{
		static dev_float sfKnockBikeOverDeltaVelLimit = 1.5f;
		const CCollisionRecord *pCollRecord = GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE);
		const float fDeltaVelMag = pCollRecord ? pCollRecord->m_fCollisionImpulseMag * GetInvMass() : 0.0f;

		if(rage::Abs(GetTransform().GetA().GetZf()) > 0.35f || rage::Abs(GetTransform().GetB().GetZf())>0.5f || fDeltaVelMag > sfKnockBikeOverDeltaVelLimit)
		{
			SetBikeOnSideStand(false);
		}
	}
	
	// check for an apply vehicle damage
	GetVehicleDamage()->Process(fwTimer::GetTimeStep());


	if(fragInst* pFragInst = GetVehicleFragInst())
	{
		phArchetypeDamp* pCurrentArchetype = (phArchetypeDamp*)pFragInst->GetArchetype();

		Vector3 vLinearDampC(BICYCLE_NEW_LINEAR_DAMP_C);
		Vector3 vLinearDampV(IsInAir() ? BICYCLE_NEW_LINEAR_DAMP_V_IN_AIR : BICYCLE_NEW_LINEAR_DAMP_V);
		Vector3 vLinearDampV2(m_fDragCoeff, m_fDragCoeff, IsInAir() ? 0.0f : m_fDragCoeff );

		if(GetVehicleType()==VEHICLE_TYPE_BIKE)
		{
			float fAirResistance = m_fDragCoeff;

			if ((!PopTypeIsMission() || NetworkInterface::IsGameInProgress()) && GetDriver() && GetDriver()->IsPlayer())
			{
				CPlayerInfo *pPlayer = GetDriver()->GetPlayerInfo();
				if (pPlayer && pPlayer->m_fForceAirDragMult > 0.0f)
				{
					float fNewAirResistance = m_fDragCoeff * pPlayer->m_fForceAirDragMult;
					if(fNewAirResistance > fAirResistance)
					{
						fAirResistance = fNewAirResistance;
					}
				}

				CControl *pControl = GetDriver()->GetControlFromPlayer();
				if(pControl)
				{
					float fLeanInput = 1.0f + rage::Clamp(m_fLeanFwdInput, -1.0f, 0.0f);
					fAirResistance *= sfBikeRiderResistanceMultMin + fLeanInput * (sfBikeRiderResistanceMultMax - sfBikeRiderResistanceMultMin);
				}
			}

			// Update air resistance for slip stream
			float fSlipStreamEffect = GetSlipStreamEffect();
			
			vLinearDampV2.Set(fAirResistance, fAirResistance, IsInAir() ? 0.0f : fAirResistance );
			vLinearDampV2 -= (vLinearDampV2 * fSlipStreamEffect);
		}

		if(GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C, BICYCLE_NEW_ROT_DAMP_C);
			pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, BICYCLE_NEW_ROT_DAMP_V);
			pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2, BICYCLE_NEW_ROT_DAMP_V2);

			vLinearDampC = BICYCLE_NEW_LINEAR_DAMP_C;
			vLinearDampV = IsInAir() ? BICYCLE_NEW_LINEAR_DAMP_V_IN_AIR : BICYCLE_NEW_LINEAR_DAMP_V;


			if(GetDriver() && GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsTuckedOnBicycleThisFrame))
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, TUCK_DAMPING_MULT, 0.25f, 0.0f, 1.0f, 0.01f);
				vLinearDampC *= TUCK_DAMPING_MULT;
				vLinearDampV *= TUCK_DAMPING_MULT;
				vLinearDampV2 *= TUCK_DAMPING_MULT;
			}
		}
		else
		{
			pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C, BIKE_NEW_ROT_DAMP_C);
			pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, BIKE_NEW_ROT_DAMP_V);
			pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2,IsInAir() ? BIKE_NEW_ROT_DAMP_V2_IN_AIR : BIKE_NEW_ROT_DAMP_V2);

		}

		//increase damping depending on the amount of passengers the bike has
		vLinearDampC *= (1.0f + static_cast<float>(GetNumberOfPassenger()));
		vLinearDampV *= (1.0f + static_cast<float>(GetNumberOfPassenger()));
		vLinearDampV2 *= (1.0f + static_cast<float>(GetNumberOfPassenger())); 

		pCurrentArchetype->ActivateDamping(phArchetypeDamp::LINEAR_C, vLinearDampC);
		pCurrentArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V, vLinearDampV);

		TUNE_GROUP_FLOAT(VEHICLE_DAMPING, BIKE_V2_DAMP_MULT, 1.0f, 0.0f, 5.0f, 0.01f);
		pCurrentArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, vLinearDampV2 * BIKE_V2_DAMP_MULT);
	}
	

	// Update the timer determining how dangerously the vehicle is being driven
	UpdateDangerousDriverEvents(fwTimer::GetTimeStep());

	if(GetHandBrake())
	{
		m_uHandbrakeLastPressedTime = fwTimer::GetTimeInMilliseconds();
	}

#if __BANK
	// draw a speedo for player car
	if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
		&& (GetStatus()==STATUS_PLAYER))
	{
		char tmp[512];
		float fGroundClearance = GetHeightAboveRoad() + GetBoundingBoxMin().z;
		formatf(tmp,512, "Mass=%g Inertia=%g GroundClearance(%1.3f)", GetMass(), GetAngInertia().Mag(), fGroundClearance );
		grcDebugDraw::PrintToScreenCoors(tmp, 5,16);

        Vector3 dampingC = ((phArchetypeDamp*)GetCurrentPhysicsInst()->GetArchetype())->GetDampingConstant(phArchetypeDamp::ANGULAR_C);
        Vector3 dampingV = ((phArchetypeDamp*)GetCurrentPhysicsInst()->GetArchetype())->GetDampingConstant(phArchetypeDamp::ANGULAR_V);
        Vector3 dampingV2 = ((phArchetypeDamp*)GetCurrentPhysicsInst()->GetArchetype())->GetDampingConstant(phArchetypeDamp::ANGULAR_V2);

        formatf(tmp,512, "C=%1.3f, V=%1.3f, V2=%1.3f", dampingC.Mag(), dampingV.Mag(), dampingV2.Mag());
        grcDebugDraw::PrintToScreenCoors(tmp, 5,17);

		float fFrontLength = rage::Abs(GetWheel(1)->GetProbeSegment().A.y);
		float fRearLength = rage::Abs(GetWheel(0)->GetProbeSegment().A.y);
		float fWheelbase = fFrontLength + fRearLength;
		float fFrontSFF = fFrontLength / GetHeightAboveRoad();
		float fFrontHandlingSFF = fFrontLength / (GetHeightAboveRoad() + pHandling->m_vecCentreOfMassOffset.GetZf());
		float fRearSFF = fRearLength / GetHeightAboveRoad();
		float fRearHandlingSFF = fRearLength / (GetHeightAboveRoad() + pHandling->m_vecCentreOfMassOffset.GetZf());
		float fRearMinSFF = (fRearLength - pHandling->GetBikeHandlingData()->m_fLeanBakCOMMult) / (GetHeightAboveRoad() + pHandling->m_vecCentreOfMassOffset.GetZf());
		formatf(tmp,512, "Cg Z (%1.2f:%1.2f) Length(%1.2f, F:%1.2f R%1.2f)", GetHeightAboveRoad(), GetHeightAboveRoad() + pHandling->m_vecCentreOfMassOffset.GetZf(), fWheelbase, fFrontLength, fRearLength);
		grcDebugDraw::PrintToScreenCoors(tmp, 5,18);
		formatf(tmp,512, "SFF: Model(%1.2f R:%1.2f) With COG Moved(%1.2f R:%1.2f) Min (R:%1.2f)", fFrontSFF, fRearSFF, fFrontHandlingSFF, fRearHandlingSFF, fRearMinSFF);
		grcDebugDraw::PrintToScreenCoors(tmp, 5,19);

		float fBaseWeightDistribution = 100.0f*GetWheel(0)->GetProbeSegment().A.y / (GetWheel(0)->GetProbeSegment().A.y - GetWheel(1)->GetProbeSegment().A.y);
		float fHandlingWeightDistribution = 100.0f*(GetWheel(0)->GetProbeSegment().A.y - pHandling->m_vecCentreOfMassOffset.GetYf()) / (GetWheel(0)->GetProbeSegment().A.y - GetWheel(1)->GetProbeSegment().A.y);
		formatf(tmp,512, "Weight Balance %2.1f:%2.1f With COG Moved(%2.1f:%2.1f)", fBaseWeightDistribution, (100.0f - fBaseWeightDistribution), fHandlingWeightDistribution, (100.0f - fHandlingWeightDistribution));
		grcDebugDraw::PrintToScreenCoors(tmp, 5,20);

        formatf(tmp,512, "Steering %2.1f Wheel Steering %2.1f", GetSteerAngle(), GetWheelFromId(BIKE_WHEEL_F)->GetSteeringAngle());
        grcDebugDraw::PrintToScreenCoors(tmp, 5,21);

        phArchetypeDamp* pCurrentArchetype = (phArchetypeDamp*)GetCurrentPhysicsInst()->GetArchetype();

        formatf(tmp,512, "LINEAR_C  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_C).x,
                                                          pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_C).y,
                                                          pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_C).z);
        grcDebugDraw::PrintToScreenCoors(tmp, 5,23);

        formatf(tmp,512, "LINEAR_V  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V).x,
                                                           pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V).y,
                                                           pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V).z);
        grcDebugDraw::PrintToScreenCoors(tmp, 5,24);

		if(GetDriver() && GetDriver()->IsPlayer())
		{
			float fScriptedDragMult = 0.0f;
			CPlayerInfo *pPlayer = GetDriver()->GetPlayerInfo();
			if (pPlayer && pPlayer->m_fForceAirDragMult > 0.0f)
			{
				fScriptedDragMult = pPlayer->m_fForceAirDragMult;
			}
			bool bIsActive =  pPlayer->m_fForceAirDragMult > 0.0f && pPlayer->m_fForceAirDragMult != 1.0f;
			
			formatf(tmp,512, "LINEAR_V2  %2.5f, %2.5f, %2.5f (drag coeff %2.5f, scripted drag mult %2.5f, scripted drag active %s)", 
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).x,
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).y,
				pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).z,
				m_fDragCoeff, fScriptedDragMult,
				bIsActive ? "YES" : "NO");
		}
		else
		{
			formatf(tmp,512, "LINEAR_V2  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).x,
															   pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).y,
															   pCurrentArchetype->GetDampingConstant(phArchetypeDamp::LINEAR_V2).z);
		}
        grcDebugDraw::PrintToScreenCoors(tmp, 5,25);

        formatf(tmp,512, "ANGULAR_C  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_C).x,
                                                          pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_C).y,
                                                          pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_C).z);
        grcDebugDraw::PrintToScreenCoors(tmp, 5,26);

        formatf(tmp,512, "ANGULAR_V  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V).x,
                                                           pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V).y,
                                                           pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V).z);
        grcDebugDraw::PrintToScreenCoors(tmp, 5,27);

        formatf(tmp,512, "ANGULAR_V2  %2.5f, %2.5f, %2.5f", pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V2).x,
                                                           pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V2).y,
                                                           pCurrentArchetype->GetDampingConstant(phArchetypeDamp::ANGULAR_V2).z);
        grcDebugDraw::PrintToScreenCoors(tmp, 5,28);

		char vehDebugString[1024];
		sprintf(vehDebugString,"KERS=%1.3f",  m_Transmission.GetKERSRemaining());
		grcDebugDraw::PrintToScreenCoors(vehDebugString, 4, 29);
	}
#endif

#if __DEV
	bool displayDebugInfo = (!CVehicleIntelligence::ms_debugDisplayFocusVehOnly || CDebugScene::FocusEntities_IsInGroup(this));
	if(displayDebugInfo)
	{
		const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		if(CVehicleAILodManager::ms_displayDummyVehicleMarkers)
		{
			if(IsDummy())
			{
				// Draw our position.
				grcDebugDraw::Line(vThisPosition,Vector3(vThisPosition.x, vThisPosition.y,vThisPosition.z + 4.0f),Color_green);
			}
		}

		if(CVehiclePopulation::ms_displayVehicleDirAndPath)
		{
			// Draw our direction.
			const Vector3 dir(VEC3V_TO_VECTOR3(GetTransform().GetB()));
			grcDebugDraw::Line(vThisPosition,(vThisPosition+(dir*4.0f)),Color32(0xff,0xc0,0xc0,0xff));

			CVehicleNodeList * pNodeList = GetIntelligence()->GetNodeList();
			if(pNodeList)
			{
				// Draw our current path.
				// Go through all of the path segments and render them.
				for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1; i++)
				{
					const CNodeAddress addr1 = pNodeList->GetPathNodeAddr(i);
					const CNodeAddress addr2 = pNodeList->GetPathNodeAddr(i+1);

					if(	!addr1.IsEmpty() && ThePaths.IsRegionLoaded(addr1) &&
						!addr2.IsEmpty() && ThePaths.IsRegionLoaded(addr2))
					{
						Vector3 node1Coors;
						const CPathNode* pNode1 = ThePaths.FindNodePointer(addr1);
						Assert(pNode1);
						pNode1->GetCoors(node1Coors); 

						static float radius = 0.5f;
						grcDebugDraw::Sphere(node1Coors, radius, Color32(1.0f,1.0f,0.0f,0.5f));

						Vector3 node2Coors;
						const CPathNode* pNode2 = ThePaths.FindNodePointer(addr2);
						Assert(pNode2);
						pNode2->GetCoors(node2Coors);

						static float radius2 = 0.25f;
						grcDebugDraw::Sphere(node2Coors, radius2, Color32(0.0f,1.0f,1.0f,1.0f));

						const float p1 = static_cast<float>(i) / static_cast<float>(CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
						const float p2 = static_cast<float>(i+1) / static_cast<float>(CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
						grcDebugDraw::Line(node1Coors, node2Coors, Color32(0.0f,1.0f-p1,p1,1.0f),Color32(0.0f,1.0f-p2,p2,1.0f));
					}
				}
			}
		}

		if(CVehicleAILodManager::ms_displayDummyVehicleNonConvertReason)
		{
			if(IsDummy())
			{
				// only want to do text for vehicles close to camera
				// Set origin to be the debug cam, or the player..
				//camDebugDirector& debugDirector = camInterface::GetDebugDirector();
				//Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
				Vector3 vDiff = vThisPosition - camInterface::GetPos();
				float fDist = vDiff.Mag();
				const float vehicleVisualizationRange = 60.0f;
				if(fDist < vehicleVisualizationRange)
				{
					Vector3 WorldCoors = vThisPosition + Vector3(0,0,1.0f);

					float fScale = 1.0f - (fDist / vehicleVisualizationRange);
					u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);
					Color32 colour = CRGBA(255,192,96,iAlpha);

					char vehDebugString[1024];
					sprintf(vehDebugString,"Nonconvert Reason: %s", GetNonConversionReason());
					grcDebugDraw::Text( WorldCoors, colour, vehDebugString);
				}
			}
		}
	}
#endif // __DEV
 
	CVehicle::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
}

bool CBike::UsesDummyPhysics(const eVehicleDummyMode vdm) const
{
	switch(vdm)
	{
		case VDM_REAL:
			return false;
		case VDM_DUMMY:
			return true;
		case VDM_SUPERDUMMY:
			return true;
		default:
			Assert(false);
			return false;
	}
}

void CBike::SwitchToFullFragmentPhysics(const eVehicleDummyMode prevMode)
{
    if(m_leanConstraintHandle.IsValid())
        RemoveConstraint();

	CVehicle::SwitchToFullFragmentPhysics(prevMode);

	if(!m_leanConstraintHandle.IsValid())
		InitConstraint();

	//Take the bike off the stand so it doesn't lean over when changing lod mode
	if(GetVelocity().Mag2() > rage::square(5.0f))
	{
		SetBikeOnSideStand(false, true);
	}

}

void CBike::SwitchToDummyPhysics(const eVehicleDummyMode prevMode)
{    
    if(m_leanConstraintHandle.IsValid())
        RemoveConstraint();

	CVehicle::SwitchToDummyPhysics(prevMode);

	if(!m_leanConstraintHandle.IsValid())
		InitConstraint();

	//Take the bike off the stand so it doesn't lean over when changing lod mode
	if(GetVelocity().Mag2() > rage::square(3.0f))
	{
		SetBikeOnSideStand(false, true);
	}
}

void CBike::SwitchToSuperDummyPhysics(const eVehicleDummyMode prevMode)
{
	if(m_leanConstraintHandle.IsValid())
		RemoveConstraint();

	CVehicle::SwitchToSuperDummyPhysics(prevMode);
}

//////////////////////////////////////////////////////////////////////////

bool CBike::ShouldOrientateVehicleUpForPedEntry(const CPed* pPed) const
{
	if( pPed != NULL && GetLayoutInfo()->GetUsePickUpPullUp() )
	{
		// THIS PARAMETER SHOULD BE IN SYNC WITH CTaskEnterVehicle::Tunables.m_MinMagForBikeToBeOnSide
		TUNE_GROUP_FLOAT( VEHICLE_TUNE, fMinMagForBikeToBeOnSide, 0.78f, 0.0f, 5.0f, 0.1f );

		const float fSide = GetTransform().GetA().GetZf();
		if( Abs(fSide) > fMinMagForBikeToBeOnSide )
		{
			const float fHeightDiff = pPed->GetTransform().GetPosition().GetZf() -1.0f - GetTransform().GetPosition().GetZf();
			TUNE_GROUP_FLOAT(BIKE_TUNE, MIN_HEIGHT_DIFF_TO_JUST_GET_ON_DIRECTLY, 0.5f, 0.0f, 1.0f, 0.01f);
			if( fHeightDiff < 0.0f && Abs(fHeightDiff) > MIN_HEIGHT_DIFF_TO_JUST_GET_ON_DIRECTLY )
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

float CBike::ProcessDriveForce(float fTimeStep, float fDriveWheelSpeedsAverage, Vector3::Vector3Param vDriveWheelGroundVelocitiesAverage, int nNumDriveWheels, int nNumDriveWheelsOnGround)
{
	float fDriveForce = 0.0f;
	if(m_nVehicleFlags.bEngineOn)
		fDriveForce = m_Transmission.Process(this, GetMatrixRef(), RCC_VEC3V(GetVelocity()), fDriveWheelSpeedsAverage, vDriveWheelGroundVelocitiesAverage, nNumDriveWheels, nNumDriveWheelsOnGround, fTimeStep);

	return fDriveForce;
}

bool CBike::GetIsOffRoad() const
{
	if (!GetLayoutInfo())
		return false;
	
	return GetLayoutInfo()->GetName() == ATSTRINGHASH("LAYOUT_BIKE_DIRT", 0x8e304960);
}

// This has to live in here as the ped task needs the information as soon as possible and the vehicle task tree is processed after the ped.
/////////////////////////////////////////////////////////////////////////
void CBike::GetBikeLeanInputs(float& fInputX, float& fInputY)
{
	if(m_nBikeFlags.bInputsUpToDate)
	{
		fInputX = m_fLeanSideInput;
		fInputY = m_fLeanFwdInput;
	}
	else
	{
		if(GetDriver() && GetDriver()->GetControlFromPlayer())
		{
			CControl *pPlayerControl = GetDriver()->GetControlFromPlayer();

			pPlayerControl->SetVehicleSteeringExclusive();
			float fSideInput = -pPlayerControl->GetVehicleSteeringLeftRight().GetNorm();
			float fFwdInput = pPlayerControl->GetVehicleSteeringUpDown().GetNorm();

#if RSG_PC
			const float c_fMouseAdjustmentScale = 30.0f;
			if(pPlayerControl->WasKeyboardMouseLastKnownSource())
			{
				if(pPlayerControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
				{
					// Undo fixup applied for scaled axis, without affecting scripted minigame inputs.
					fSideInput *= c_fMouseAdjustmentScale;
					m_nBikeFlags.bUsingMouseSteeringInput = true;
				}
				else if(pPlayerControl->GetVehicleSteeringLeftRight().GetSource().m_Device == IOMS_MKB_AXIS)// Keyboard steering input
				{
					m_nBikeFlags.bUsingMouseSteeringInput = false;
				}

				if(pPlayerControl->GetVehicleSteeringUpDown().GetSource().m_Device == IOMS_MOUSE_SCALEDAXIS)
				{
					// Undo fixup applied for scaled axis, without affecting scripted minigame inputs.
					fFwdInput *= c_fMouseAdjustmentScale;
				}
			}
			else
			{
				m_nBikeFlags.bUsingMouseSteeringInput = false;
			}


			bool bMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == CControl::eMSM_Vehicle;
			bool bCameraMouseSteering = CControl::GetMouseSteeringMode(PREF_MOUSE_DRIVE) == CControl::eMSM_Camera;

			if(m_nBikeFlags.bUsingMouseSteeringInput && ((bMouseSteering && !CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn()) || (bCameraMouseSteering && CControlMgr::GetMainPlayerControl().GetDriveCameraToggleOn())))
			{
				float fTimeStep = fwTimer::GetTimeStep();

				TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, BIKE_STEERING_DEADZONE_CENTERING_SPEED, 2.0f, 0.0f, 30.0f, 0.01f);
				TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, BIKE_STEERING_ASSISTED_CENTERING_SPEED, 2.2f, 0.0f, 30.0f, 0.01f);
				TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, BIKE_STEERING_MULTIPLIER, 1.0f, 0.0f, 30.0f, 0.01f);
				TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, BIKE_STEERING_INPUT_DEADZONE, 0.05f, 0.0f, 1.0f, 0.001f);

				float fAutoCenterMult = CPauseMenu::GetMenuPreference(PREF_MOUSE_AUTOCENTER_BIKE)/10.0f;


				if (fabs(fSideInput) <= BIKE_STEERING_INPUT_DEADZONE)
				{
					if(m_fLeanSideInput > 0.0f)
					{
						m_fLeanSideInput = Clamp(m_fLeanSideInput - BIKE_STEERING_DEADZONE_CENTERING_SPEED * fAutoCenterMult * fTimeStep, 0.0f, 1.0f);
					}
					else
					{
						m_fLeanSideInput = Clamp(m_fLeanSideInput + BIKE_STEERING_DEADZONE_CENTERING_SPEED * fAutoCenterMult * fTimeStep,-1.0f, 0.0f);
					}
				}
				else if( Sign(fSideInput) == Sign(m_fLeanSideInput) || rage::Abs(m_fLeanSideInput) <= BIKE_STEERING_INPUT_DEADZONE)
				{
					m_fLeanSideInput += (fSideInput * BIKE_STEERING_MULTIPLIER);
				}
				else// Trying to recenter
				{
					if(m_fLeanSideInput > 0.0f)
					{
						m_fLeanSideInput = Clamp(m_fLeanSideInput + (fSideInput * BIKE_STEERING_ASSISTED_CENTERING_SPEED), 0.0f, 1.0f);
					}
					else
					{
						m_fLeanSideInput = Clamp(m_fLeanSideInput + (fSideInput * BIKE_STEERING_ASSISTED_CENTERING_SPEED),-1.0f, 0.0f);
					}
				}

				TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, BIKE_LEANING_DEADZONE_CENTERING_SPEED, 0.3f, 0.0f, 30.0f, 0.01f);
				TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, BIKE_LEANING_ASSISTED_CENTERING_SPEED, 0.4f, 0.0f, 30.0f, 0.01f);
				TUNE_GROUP_FLOAT(MOUSE_STEERING_TUNE, BIKE_LEANING_MULTIPLIER, 1.0f, 0.0f, 30.0f, 0.01f);


				if (fabs(fFwdInput) <= FLT_EPSILON)
				{
					if(m_fLeanFwdInput > 0.0f)
					{
						m_fLeanFwdInput = Clamp(m_fLeanFwdInput - BIKE_LEANING_DEADZONE_CENTERING_SPEED * fAutoCenterMult * fTimeStep, 0.0f, 1.0f);
					}
					else
					{
						m_fLeanFwdInput = Clamp(m_fLeanFwdInput + BIKE_LEANING_DEADZONE_CENTERING_SPEED * fAutoCenterMult * fTimeStep,-1.0f, 0.0f);
					}
				}
				else if( Sign(fFwdInput) == Sign(m_fLeanFwdInput) || rage::Abs(m_fLeanFwdInput) <= FLT_EPSILON)
				{
					m_fLeanFwdInput += (fFwdInput * BIKE_LEANING_MULTIPLIER);
					m_fLeanFwdInput = Clamp(m_fLeanFwdInput, -1.0f, 1.0f);
				}
				else// Trying to recenter
				{
					if(m_fLeanFwdInput > 0.0f)
					{
						m_fLeanFwdInput = Clamp(m_fLeanFwdInput + (fFwdInput * BIKE_LEANING_ASSISTED_CENTERING_SPEED), 0.0f, 1.0f);
					}
					else
					{
						m_fLeanFwdInput = Clamp(m_fLeanFwdInput + (fFwdInput * BIKE_LEANING_ASSISTED_CENTERING_SPEED),-1.0f, 0.0f);
					}
				}
			}
			else
			{
				m_fLeanSideInput = fSideInput;
				m_fLeanFwdInput = fFwdInput;
			}

#else
			m_fLeanSideInput = fSideInput;
			m_fLeanFwdInput = fFwdInput;
#endif

#if USE_SIXAXIS_GESTURES
			const bool bSixAxisValid = CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_BIKE) && CControlMgr::GetPlayerPad() && CControlMgr::GetPlayerPad()->GetPadGesture();
			if(bSixAxisValid)
			{
				if(m_fLeanSideInput == 0.0f && pPlayerControl->GetVehicleSteeringLeftRight().IsEnabled())
				{
					m_fLeanSideInput = -CControlMgr::GetPlayerPad()->GetPadGesture()->GetPadRollInRange(CBike::ms_fMotionControlRollMin,CBike::ms_fMotionControlRollMax,true);
				}

				if(m_fLeanFwdInput == 0.0f && pPlayerControl->GetVehicleSteeringUpDown().IsEnabled())
				{
					m_fLeanFwdInput = CControlMgr::GetPlayerPad()->GetPadGesture()->GetPadPitchInRange(CBike::ms_fMotionControlPitchMin,CBike::ms_fMotionControlPitchMax,true,true);
				}
			}
#endif // USE_SIXAXIS_GESTURES

			if(!InheritsFromBicycle())
			{
				bool bDuckDisabled = false;
				if(GetDriver()->GetPlayerInfo()->CanPlayerPerformVehicleMelee())
				{
					bDuckDisabled = true;
				}

				if (!bDuckDisabled && pPlayerControl->GetVehicleDuck().GetNorm() > CTaskMotionInAutomobile::ms_Tunables.m_DuckControlThreshold)
				{
					if (Abs(m_fLeanSideInput) < CTaskMotionInAutomobile::ms_Tunables.m_DuckBikeSteerThreshold)
					{
						m_fLeanFwdInput = -1.0f;
					}
				}
			}

		}

		fInputX = m_fLeanSideInput; 
		fInputY = m_fLeanFwdInput;
		m_nBikeFlags.bInputsUpToDate = true;
	}
}

void CBike::ProcessBikeJumping()
{
	// Bike Jumping
	if(pHandling->GetBikeHandlingData()->m_fJumpForce > 0.0f )
	{
		if( m_uJumpLastReleasedTime != 0 )
		{
			float fTimeSinceReleased = (float) (fwTimer::GetTimeInMilliseconds() - m_uJumpLastReleasedTime);

			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_BEFORE_CANCELLING_LATERAL_JUMP__WITH_WHEELS_ON_GROUND, 400.0f, 0.0f, 3000.0f, 0.1f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, TIME_BEFORE_CANCELLING_LATERAL_JUMP, 250.0f, 0.0f, 3000.0f, 0.1f);

			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_BEFORE_RAISING_REAR_WHEEL, 250.0f, 0.0f, 3000.0f, 0.1f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_BEFORE_CANCELLING_RAISE_REAR_WHEEL, 700.0f, 0.0f, 3000.0f, 0.1f);

			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_BEFORE_CANCELLING_AUTO_HEADING, 250.0f, 0.0f, 3000.0f, 0.1f);
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_TIME_BEFORE_AUTO_HEADING, 150.0f, 0.0f, 3000.0f, 0.1f);

			if(!GetWheel(0)->GetIsTouching() && !GetWheel(1)->GetIsTouching() )
			{
				float bRearWheelRaiseMult = 1.0f;
				if(GetDriver() && GetDriver()->IsLocalPlayer())
				{
					if( m_fLeanFwdInput > 0.0f )
					{
						bRearWheelRaiseMult = 1.0f - m_fLeanFwdInput;
					}
				}

				if( (fTimeSinceReleased < MIN_TIME_BEFORE_CANCELLING_RAISE_REAR_WHEEL) && 
					(fTimeSinceReleased > MIN_TIME_BEFORE_RAISING_REAR_WHEEL && GetVelocity().z > 0.0f))
				{
					float currentPitch = GetTransform().GetPitch();
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, JUMP_AUTO_LEVEL_MULT, -85.0f, -1000.0f, 1000.0f, 0.1f);
					ApplyInternalTorque( ((currentPitch-m_fJumpLaunchPitch) * JUMP_AUTO_LEVEL_MULT * bRearWheelRaiseMult) * GetAngInertia().y * VEC3V_TO_VECTOR3(GetTransform().GetA()));
				}

				if( (fTimeSinceReleased < MIN_TIME_BEFORE_CANCELLING_AUTO_HEADING) &&
					(fTimeSinceReleased > MIN_TIME_BEFORE_AUTO_HEADING) )
				{
					float currentHeading = GetTransform().GetHeading();
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, JUMP_AUTO_HEADING_MULT, -25.0f, -1000.0f, 1000.0f, 0.1f);
					ApplyInternalTorque( rage::SubtractAngleShorter(currentHeading, m_fJumpLaunchHeading) * JUMP_AUTO_HEADING_MULT * GetAngInertia().z * VEC3V_TO_VECTOR3(GetTransform().GetC()));
				}
			}

			float fSpeedSq = GetVelocity().XYMag2();
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, JUMP_MAX_SPEED_SQ_FOR_LATERAL_JUMP, 20.0f, -1000.0f, 1000.0f, 0.1f);

			float fTimeBeforeCancellingLateralJump = TIME_BEFORE_CANCELLING_LATERAL_JUMP;
			if(fSpeedSq < JUMP_MAX_SPEED_SQ_FOR_LATERAL_JUMP)
			{
				fTimeBeforeCancellingLateralJump *= 1.0f - (fSpeedSq/JUMP_MAX_SPEED_SQ_FOR_LATERAL_JUMP);
			}
			else
			{
				fTimeBeforeCancellingLateralJump = 0.0f;
			}

			
			if( ((GetWheel(0)->GetIsTouching() || GetWheel(1)->GetIsTouching()) && fTimeSinceReleased > MIN_TIME_BEFORE_CANCELLING_LATERAL_JUMP__WITH_WHEELS_ON_GROUND) || (!GetWheel(0)->GetIsTouching() && !GetWheel(1)->GetIsTouching()) )
			{
				if( fTimeSinceReleased > fTimeBeforeCancellingLateralJump || GetVelocity().z < 0.0f )
				{
						m_nBikeFlags.bLateralJump = false;
						m_uJumpLastReleasedTime = 0;
				}
			}
		}

		// Make sure at least one wheel is on the ground and the button has been pressed or released(released is cleared in this function so should only be valid for one frame)
		if( (GetWheel(0)->GetIsTouching() || GetWheel(1)->GetIsTouching()) && (m_nBikeFlags.bJumpReleased || m_nBikeFlags.bJumpPressed) )
		{
			// Scale the jump force by the amount of time the button has been pressed.
			float fTimeSincePressed = (float) (fwTimer::GetTimeInMilliseconds() - m_uJumpLastPressedTime);
			
			float fJumpForceScale = fTimeSincePressed / ms_fBikeJumpFullPowerTime;
			TUNE_GROUP_BOOL(BICYCLE_TUNE, USE_VELOCITY_BASED_JUMP_HEIGHT, true);
			if (USE_VELOCITY_BASED_JUMP_HEIGHT)
			{
				const float fXYBikeVelocity = GetVelocity().XYMag();
				//taskDisplayf("fBikeVelocity = %.2f", fXYBikeVelocity);

				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_JUMP_HEIGHT_SCALE, 0.8f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MIN_JUMP_HEIGHT_VELOCITY, 1.0f, 0.0f, 25.0f, 0.01f);

				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MID_JUMP_HEIGHT_SCALE, 0.9f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MID_JUMP_HEIGHT_VELOCITY, 5.0f, 0.0f, 25.0f, 0.01f);

				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_JUMP_HEIGHT_SCALE, 1.0f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, MAX_JUMP_HEIGHT_VELOCITY, 10.0f, 0.0f, 25.0f, 0.01f);

				if (fXYBikeVelocity < MIN_JUMP_HEIGHT_VELOCITY)
				{
					const float fScaleNorm = Clamp(Abs(fXYBikeVelocity / MIN_JUMP_HEIGHT_VELOCITY), 0.0f, 1.0f);
					fJumpForceScale = (1.0f - fScaleNorm) * MIN_JUMP_HEIGHT_SCALE + fScaleNorm * MID_JUMP_HEIGHT_SCALE;
				}
				else
				{
					const float fScaleNorm = Clamp(Abs((fXYBikeVelocity - MID_JUMP_HEIGHT_VELOCITY) / (MAX_JUMP_HEIGHT_SCALE - MID_JUMP_HEIGHT_VELOCITY)), 0.0f, 1.0f);
					fJumpForceScale = (1.0f - fScaleNorm) * MID_JUMP_HEIGHT_SCALE + fScaleNorm * MAX_JUMP_HEIGHT_SCALE;
				}
			}
			float fClampedJumpForceScale = Clamp(fJumpForceScale, 0.0f, 1.0f);

			float fFinalJumpForce = pHandling->GetBikeHandlingData()->m_fJumpForce;

			//Use the wheelie ability stat to modify the height of jumps
			float fAbilityMult = 1.0f;
			if(GetDriver() && GetDriver()->IsLocalPlayer())
			{
				StatId stat = STAT_WHEELIE_ABILITY.GetStatId();
				float fWheelieStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
				float fMinWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MinWheelieAbility;
				float fMaxWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MaxWheelieAbility;

				fAbilityMult = ((1.0f - fWheelieStatValue) * fMinWheelieAbility + fWheelieStatValue * fMaxWheelieAbility)/100.0f;
			}
			fFinalJumpForce *= fAbilityMult;

			// Lower the jumping force the more tilted the bike is
			const ScalarV upwardsTiltSine = Max(GetMatrixRef().GetCol1().GetZ(),ScalarV(V_ZERO));
			const ScalarV forceScale = Lerp(upwardsTiltSine,ScalarV(V_ONE),ScalarV(V_HALF));
			fFinalJumpForce *= forceScale.Getf();

			if(GetDriver() && GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_ShouldLaunchBicycleThisFrame))// Jump the bike
			{
				GetDriver()->SetPedResetFlag(CPED_RESET_FLAG_ShouldLaunchBicycleThisFrame,false);
				static dev_float MIN_TIME_PRESSED_BEFORE_ALLOW_BUNNYHOP_MS = 0;
				if (fTimeSincePressed > MIN_TIME_PRESSED_BEFORE_ALLOW_BUNNYHOP_MS)
				{
					Vector3 vJumpImpulse( 0.0f, 0.0f, 1.0f);

					Vector3  vFwd = VEC3V_TO_VECTOR3(GetTransform().GetB());
					Vector3  vSide = VEC3V_TO_VECTOR3(GetTransform().GetA());

					if(GetDriver() && GetDriver()->IsLocalPlayer())
					{
						if( rage::Abs(m_fLeanSideInput) > FLT_EPSILON || rage::Abs(m_fLeanFwdInput) > FLT_EPSILON)
						{
							m_nBikeFlags.bLateralJump = true;

							Matrix33 rotationMatrix;
							rotationMatrix.Identity();
							TUNE_GROUP_FLOAT(BICYCLE_TUNE, LATERAL_JUMP_MULT, 0.5f, -10.0f, 10, 0.1f);
							TUNE_GROUP_FLOAT(BICYCLE_TUNE, LONGITUDINAL_JUMP_MULT, 0.5f, -10.0f, 10, 0.1f);

							rotationMatrix.RotateUnitAxis(vFwd, (-m_fLeanSideInput * fAbilityMult * LATERAL_JUMP_MULT));
							rotationMatrix.RotateUnitAxis(vSide, (m_fLeanFwdInput * fAbilityMult * LONGITUDINAL_JUMP_MULT));

							rotationMatrix.Transform(vJumpImpulse);
						}
					}

					m_fJumpLaunchPitch = GetTransform().GetPitch();
					m_fJumpLaunchHeading = GetTransform().GetHeading();
					TUNE_GROUP_FLOAT(BICYCLE_TUNE, JUMP_IMPULSE_FWD_POSITION, 0.2f, -10.0f, 10, 0.1f);

					vJumpImpulse *= fClampedJumpForceScale * fFinalJumpForce * ms_fBikeGravityMult * GetMass();
					ApplyImpulse( vJumpImpulse, vFwd * JUMP_IMPULSE_FWD_POSITION );

					physicsDisplayf("Frame: %i, Jump force(%1.3f, %1.3f, %1.3f)", fwTimer::GetFrameCount(), vJumpImpulse.x, vJumpImpulse.y, vJumpImpulse.z );

					//clear the jump flags and timer
					m_nBikeFlags.bJumpReleased = false;
					m_uJumpLastPressedTime = 0;
					m_uJumpLastReleasedTime = fwTimer::GetTimeInMilliseconds();
				}
			}
			else if(m_nBikeFlags.bJumpPressed) // compress the bike
			{
				//Compress the bike when holding the button.
				static dev_float sfBikeCompressionBeforeJumpForce = -1.5f;
				static dev_float sfBikeCompressionMaxExtentTimeRatio = 0.15f;// how long before we are pushing down with maximum force

				float fMaxCompressionTime = 1.0f + sfBikeCompressionMaxExtentTimeRatio;
				float fClampedPressureForceTime = Clamp(fJumpForceScale, 0.0f, fMaxCompressionTime);


				//if the buttons been held too long release the pressure pushing down on the bike
				if(fClampedPressureForceTime < sfBikeCompressionMaxExtentTimeRatio)//start depressing the bike
				{
					fClampedPressureForceTime = fClampedPressureForceTime / sfBikeCompressionMaxExtentTimeRatio;
				}
				else if(fClampedPressureForceTime < 1.0f)//maximum depression of bike
				{
					fClampedPressureForceTime = 1.0f;
				}
				else//start releasing the bike so we aren't constantly applying a force down.
				{
					fClampedPressureForceTime = ((fMaxCompressionTime) - fClampedPressureForceTime) / sfBikeCompressionMaxExtentTimeRatio;
				}

				ApplyForceCg(Vector3( 0.0f, 0.0f, fClampedPressureForceTime * sfBikeCompressionBeforeJumpForce * fFinalJumpForce * ms_fBikeGravityMult * GetMass()));
			}
		}
	}
}

RAGETRACE_DECL(CBike_ProcessPhysics);

dev_float BIKE_TEST_GROUND_LEAN_SMOOTHING = 0.6f;
dev_bool BIKE_USE_ABS = true;
dev_bool sbPlayerBikeOldSpeedSteer = false;
#if __DEV
bank_bool sbBikeDebugDisplayLeanVectors = false;
#endif
dev_float BIKE_SPEED_STEER_REDUCTION_MULT = 0.96f;

dev_float BIKE_SINK_TIME						= 2.0f;
dev_float BIKE_SINK_STEP						= 0.005f;	// How much is the fForceMult decreased by each frame once bike starts sinking
dev_float BIKE_SINK_FORCE_MULT_MULTIPLIER	= 0.1f;			// Buoyancy forceMult is decreased by the sink step until it reaches this fraction of the original
dev_float BIKE_SINK_CRASH_EVENT_RANGE			= 35.0f;	// Magic number to force the perception range for peds reacting to a sinking bike.
dev_s32 BIKE_SINK_EVENT_SUBMERGE_PERCENT		= 20;		// Magic number for the bike to count as submerged for event purposes.
//
ePhysicsResult CBike::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	if(m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
	{
		return PHYSICS_DONE;
	}

	if( m_nBikeFlags.bOnSideStand )
	{
		for(int i=0; i < GetNumWheels(); i++)
		{ 
			GetWheel(i)->GetDynamicFlags().SetFlag(WF_NO_LATERAL_SPRING);
		}
	}

	RAGETRACE_SCOPE(CBike_ProcessPhysics);

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessPhysicsOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	if(GetCurrentPhysicsInst()==NULL || !GetCurrentPhysicsInst()->IsInLevel())
		return PHYSICS_DONE;

	// Cache out some values
	CPed * pDriver = GetDriver();
	float fBrakeForce = GetBrakeForce();

	// If we're on moving ground, compute the relative velocity, just take the first physical as the ground physical
	Vector3 vBikeVelocity = GetVelocityIncludingReferenceFrame();

    // Some bikes have their forks as children of the handle bars so don't need rotating.
    if(m_RotateUpperForks)
    {
		if(GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && (m_nVehicleFlags.bAnimateWheels==0))
		{
			float fSteeringWheelMult = GetVehicleModelInfo()->GetSteeringWheelMult( GetDriver() );

			//if( !HasGlider() )
			//{
			//	fSteeringWheelMult *= ( 1.0f - GetSpecialFlightModeRatio() );
			//}
	        SetComponentRotation(BIKE_FORKS_U, ROT_AXIS_LOCAL_Z, GetSteerAngle() * fSteeringWheelMult, true); 
		}
    }

	if(CVehicle::ProcessPhysics(fTimeStep,bCanPostpone,nTimeSlice) == PHYSICS_POSTPONE)
	{
		return PHYSICS_POSTPONE;
	}

	// postpone if we're sitting on something physical
	for(int i=0; i<GetNumWheels(); i++)
	{
		if(GetWheel(i)->GetHitPhysical() && bCanPostpone)
		{
			return PHYSICS_POSTPONE;
		}
	}

	// if network has re-spotted this car and we don't want it to collide with other network vehicles for a while
	ProcessRespotCounter(fTimeStep);


	bool bDoStabilisation = false;

	bool bPedEnteringSeat = false;
	CComponentReservation* pComponentReservation = GetComponentReservationMgr() ? GetComponentReservationMgr()->GetSeatReservationFromSeat(0) : NULL;
	if(pComponentReservation && pComponentReservation->GetPedUsingComponent())
	{
		bPedEnteringSeat = true;
	}

	if( pDriver || m_nBikeFlags.bOnSideStand || m_nBikeFlags.bGettingPickedUp || bPedEnteringSeat )
	{
		bDoStabilisation = ( HasGlider() || GetSpecialFlightModeRatio() < 0.75f );
	}
	if( GetAttachParentForced() )
	{
		bDoStabilisation = false;
	}

	// calculate drive force to be sent to the wheels
	float fDriveWheelRotSpeedsAverage = 0.0f;
	Vector3 vDriveWheelGroundVelocitiesAverage(Vector3::ZeroType);
	int nNumDriveWheels = 0;
	int nNumDriveWheelsOnGround = 0;
	for(int i=0; i<GetNumWheels(); i++)
	{
		CWheel & wheel = *GetWheel(i);
		if(wheel.GetIsDriveWheel())
		{
			fDriveWheelRotSpeedsAverage += wheel.GetGroundSpeed();
			vDriveWheelGroundVelocitiesAverage += *(wheel.GetGroundBeneathWheelsVelocity());
			nNumDriveWheels++;
			if(wheel.GetIsTouching())
				nNumDriveWheelsOnGround++;
		}
		else if(GetStatus()==STATUS_PLAYER) //On the mission where we drive along a train we want to vary the jumps
		{
			static dev_float sfLaunchOffTrainMinPitch = -0.20f;
			static dev_float sfLaunchOffTrainMaxPitch = 0.30f;
	
			if( wheel.GetIsTouching() )
			{
				m_nBikeFlags.bJumpOffTrain = false;
			}
			else if ( wheel.GetWasTouching() )
			{
				if(wheel.GetPrevHitPhysical() && wheel.GetPrevHitPhysical()->GetIsTypeVehicle() && !m_nBikeFlags.bJumpOffTrain)
				{
					const CVehicle *pPrevHitVehicle = static_cast<const CVehicle*>(wheel.GetPrevHitPhysical());

					if(pPrevHitVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN && pPrevHitVehicle->PopTypeIsMission())
					{
						if(pPrevHitVehicle->GetVelocity().Mag2() < GetVelocity().Mag2())
						{
							m_nBikeFlags.bJumpOffTrain = true;

							m_fOnTrainLaunchDesiredPitch = fwRandom::GetRandomNumberInRange(sfLaunchOffTrainMinPitch, sfLaunchOffTrainMaxPitch);
						}
					}
				}
			}
		}
		else
		{
			m_nBikeFlags.bJumpOffTrain = false;
		}

		// take this opportunity to tell each of the wheels if this bike is currently being artificially constrained / stabilized
		if(bDoStabilisation || GetSpecialFlightModeRatio() > 0.0f )
		{
			if( !pHandling->GetSpecialFlightHandlingData() ||
				!( pHandling->GetSpecialFlightHandlingData()->m_flags & SF_FORCE_SPECIAL_FLIGHT_WHEN_DRIVEN ) )
			{
				wheel.GetConfigFlags().SetFlag( WCF_BIKE_CONSTRAINED_COLLIDER );
			}
			wheel.GetConfigFlags().ClearFlag(WCF_BIKE_FALLEN_COLLIDER);
		}
		else
		{
			wheel.GetConfigFlags().ClearFlag(WCF_BIKE_CONSTRAINED_COLLIDER);
			wheel.GetConfigFlags().SetFlag(WCF_BIKE_FALLEN_COLLIDER);
		}
	}

	//On the mission where we drive along a train we want to vary the jumps
	if(m_nBikeFlags.bJumpOffTrain)
	{
		static dev_float sfInAirOffTrainMult = -100.0f;

		//Adjust Pitch
		float currentPitch = GetTransform().GetPitch();
#if __ASSERT
		Vector3 vecTorque = (currentPitch-m_fOnTrainLaunchDesiredPitch) * sfInAirOffTrainMult * GetAngInertia().y * VEC3V_TO_VECTOR3(GetTransform().GetA());
		Vector3 vecAngAccel = VEC3V_TO_VECTOR3(GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecTorque)));
		vecAngAccel.Multiply(GetInvAngInertia());
		Assertf(vecAngAccel.Mag2() < square(150.0f), "Large internal torque detected [%3.2f,%3.2f,%3.2f], currentPitch %3.2f, desiredPitch %3.2f, InAirMult %3.2f",
			vecTorque.x, vecTorque.y, vecTorque.z, currentPitch, m_fOnTrainLaunchDesiredPitch, sfInAirOffTrainMult);
#endif
		ApplyInternalTorque( (currentPitch-m_fOnTrainLaunchDesiredPitch) * sfInAirOffTrainMult * GetAngInertia().y * VEC3V_TO_VECTOR3(GetTransform().GetA()));
	}

	float fInvNumDriveWheels = 1.0f/nNumDriveWheels;
	fDriveWheelRotSpeedsAverage *= fInvNumDriveWheels;
	vDriveWheelGroundVelocitiesAverage *= fInvNumDriveWheels;

	float fDriveForce = ProcessDriveForce(fTimeStep, fDriveWheelRotSpeedsAverage, vDriveWheelGroundVelocitiesAverage, nNumDriveWheels, nNumDriveWheelsOnGround);

	// If the driver is being jacked knock the passengers off
	if( (pDriver && pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE_SEAT)) &&
		pDriver->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT) == CTaskExitVehicleSeat::State_BeJacked )
	{
		for( s32 iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); iSeat++ )
		{
			CPed* pPassenger = m_SeatManager.GetPedInSeat(iSeat);
			if( pPassenger && iSeat!=GetDriverSeat() && !pPassenger->IsInjured() )
			{
				pPassenger->KnockPedOffVehicle(false);
			}
		}
	}

	// skip default physics update, gravity and air resistance applied here
	if (ProcessIsAsleep())
	{
		float gravityForce = m_fGravityForWheelIntegrator*GetMass();
		bool isInactiveRecording = IsRunningCarRecording();
		for(int i=0; i<GetNumWheels(); i++)
		{
			GetWheel(i)->SetSteerAngle(GetSteerAngle());
			GetWheel(i)->SetBrakeAndDriveForce(GetBrake() * fBrakeForce, fDriveForce, GetThrottle(), 0.0f);

			if(GetHandBrake() && !(pHandling->hFlags & HF_NO_HANDBRAKE))
				GetWheel(i)->SetHandBrakeForce(pHandling->m_fHandBrakeForce);

			GetWheel(i)->ProcessAsleep(vBikeVelocity, fTimeStep, gravityForce, isInactiveRecording);
            GetWheel(i)->SetGripMult(1.0f);
		}

		UpdateStoredFwdVector(MAT34V_TO_MATRIX34(GetMatrix()));

		m_vecInternalForce.Zero();
		m_vecInternalTorque.Zero();

    	// Some bikes have their forks as children of the handle bars so don't need rotating.
    	if(m_RotateUpperForks)
    	{
			if(GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && (m_nVehicleFlags.bAnimateWheels==0))
			{
				float fSteeringWheelMult = GetVehicleModelInfo()->GetSteeringWheelMult(GetDriver());
	        	SetComponentRotation(BIKE_FORKS_U, ROT_AXIS_LOCAL_Z, GetSteerAngle() * fSteeringWheelMult, true);
			}
		}

		return PHYSICS_DONE;
	}

	// Make sure the possbilyTouchesWater flag is up to date before acting on it...
	if(m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate)
	{
		// If bPossiblyTouchesWater is false we are either well above the surface of the water or in a tunnel.
		if (!m_nFlags.bPossiblyTouchesWater)
		{
			// we can't set these flags for network clones of script objects directly as they are synced
			if(!IsNetworkClone() || !(GetNetworkObject() && GetNetworkObject()->IsScriptObject(true)))
			{
				SetIsInWater( false );
				m_nVehicleFlags.bIsDrowning = false;
			}
			m_Buoyancy.ResetBuoyancy();
			m_fTimeInWater = 0.0f;
			m_Buoyancy.m_fForceMult = m_fOrigBuoyancyForceMult;
		}
		// else process the buoyancy class
		else 
		{
			float fBuoyancyAccel = 0.0f;
			SetIsInWater( m_Buoyancy.Process(this, fTimeStep, false, CPhysics::GetIsLastTimeSlice(nTimeSlice), &fBuoyancyAccel) );

			// we can't set these flags for network clones of script objects directly as they are synced
			if(!IsNetworkClone() || !(GetNetworkObject() && GetNetworkObject()->IsScriptObject(true)))
			{
				m_nVehicleFlags.bIsDrowning = false;
			}

			bool bEngineUnderWater = false;
			s32 nEngineBoneId = GetVehicleModelInfo()->GetBoneIndex(VEH_ENGINE);
			if(nEngineBoneId != -1)// Bicycles can be without an engine bone.
			{
				Matrix34 matEngineBone;
				GetGlobalMtx(nEngineBoneId, matEngineBone);
				if(m_Buoyancy.GetAbsWaterLevel() > matEngineBone.d.z)
					bEngineUnderWater = true;
			}
			bool bCanStartSinking = (fBuoyancyAccel > m_fGravityForWheelIntegrator*0.75f && (!HasContactWheels()||m_Buoyancy.GetSubmergedLevel()>=0.9f))
				|| (m_fTimeInWater > BIKE_DROWN_ENGINE_TIME) || (pHandling->m_fPercentSubmerged > 100.0f && m_Buoyancy.GetSubmergedLevel()>0.9f)
				|| (bEngineUnderWater && m_Buoyancy.GetSubmergedLevel() > 0.6f);

			if(bCanStartSinking)
			{
				m_fTimeInWater += fTimeStep;

				// Use a damage event to knock the passengers off the bike.
				if((!HasContactWheels() || m_Buoyancy.GetSubmergedLevel()>0.95f) && !IsRunningCarRecording())
				{
					float fWaterHeight = FLT_MAX;
					bool bDeepWaterUnderPed = false;
					for( s32 iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); iSeat++ )
					{
						CPed* pPassenger = m_SeatManager.GetPedInSeat(iSeat);
						if( pPassenger && !pPassenger->IsInjured() && !NetworkUtils::IsNetworkCloneOrMigrating(pPassenger) &&
							( !pPassenger->IsPlayer() ||
							  pPassenger->IsLocalPlayer() ) )
						{
							if (fWaterHeight == FLT_MAX)
							{
								TUNE_GROUP_FLOAT(EXIT_BIKE_TUNE, fWaterSearchProbeLength, 10.0f, 0.0f, 10000.0f, 0.0001f);
								TUNE_GROUP_FLOAT(EXIT_BIKE_TUNE, fWaterDepthRequired, 2.0f, 0.0f, 10000.0f, 0.0001f);
								bDeepWaterUnderPed = CTaskExitVehicleSeat::CheckForWaterUnderPed(pPassenger, this, fWaterSearchProbeLength, fWaterDepthRequired, fWaterHeight);
							}

							// If the water is deep enough have the ped ragdoll and let the bike sink away
							if (bDeepWaterUnderPed)
							{
								pPassenger->KnockPedOffVehicle(false, 0.0f, 1000);
							}
							// Otherwise, if the water is too shallow then jump out of the bike
							else if (!pPassenger->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) && !pPassenger->GetPedResetFlag(CPED_RESET_FLAG_DisableShallowWaterBikeJumpOutThisFrame))
							{
								VehicleEnterExitFlags vehicleFlags;
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
								vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
								CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskExitVehicle(this, vehicleFlags, fwRandom::GetRandomNumberInRange(0.0f, 1.0f)));
								pPassenger->GetPedIntelligence()->AddEvent(givePedTask);
							}
						}
					}
				}

				if((m_fTimeInWater>BIKE_DROWN_ENGINE_TIME || (!HasContactWheels()&&m_Buoyancy.GetStatus()==FULLY_IN_WATER))
					&& !IsRunningCarRecording() && !NetworkUtils::IsNetworkCloneOrMigrating(this))
				{
					// Turn engine off.
					if(m_nVehicleFlags.bEngineOn && !IsNetworkClone() && bEngineUnderWater)
						SwitchEngineOff();

					const bool wasWrecked = (GetStatus() == STATUS_WRECKED);

					// Don't allow the engine to be restarted.
					SetIsWrecked();

					m_nVehicleFlags.bIsDrowning = true;

					m_Buoyancy.m_fForceMult -= BIKE_SINK_STEP;
					if(m_Buoyancy.m_fForceMult < BIKE_SINK_FORCE_MULT_MULTIPLIER * m_fOrigBuoyancyForceMult)
						m_Buoyancy.m_fForceMult = BIKE_SINK_FORCE_MULT_MULTIPLIER * m_fOrigBuoyancyForceMult;

					if(m_nVehicleFlags.GetIsSirenOn())
						TurnSirenOn(true,false);

					//It was not wrecked before and isWrecked from sink lets check if the plane, bike? was damaged by a ped.
					if (GetNetworkObject() && !wasWrecked && GetStatus() == STATUS_WRECKED)
					{
						bool triggerNetworkEvent = false;

						//Make sure if a remote driver is driving your vehicle that
						//  he gets charged with the correct insurance amount.
						CPed* damager = GetVehicleWasSunkByAPlayerPed();
						if (damager)
						{
							SetWeaponDamageInfo(damager, WEAPONTYPE_UNARMED, fwTimer::GetTimeInMilliseconds());
							triggerNetworkEvent = true;
						}

						if(triggerNetworkEvent)
						{
							CNetObjPhysical::NetworkDamageInfo damageInfo(GetWeaponDamageEntity(),GetHealth(),0.0f,0.0f,false,GetWeaponDamageHash(),0,false,true,false);
							static_cast<CNetObjPhysical*>(GetNetworkObject())->UpdateDamageTracker(damageInfo);
						}
					}

				}
			}

			if(!GetIsInWater())
			{
				m_fTimeInWater = 0.0f;		// Reset timer if we aren't in water
				m_Buoyancy.m_fForceMult = m_fOrigBuoyancyForceMult;
			}
		}
	}

	for( int i = 0; i < m_pVehicleGadgets.size(); i++)
	{
		m_pVehicleGadgets[i]->ProcessPhysics(this,fTimeStep,nTimeSlice);
	}
	if(m_pVehicleWeaponMgr)
	{
		m_pVehicleWeaponMgr->ProcessPhysics(this,fTimeStep,nTimeSlice);
	}

	// knock AI peds off motorbikes if the motorbike is upside down
	if(GetTransform().GetC().GetZf() < 0.0f && pDriver && !pDriver->IsPlayer() && !pDriver->IsInjured() && pDriver->m_PedConfigFlags.GetCantBeKnockedOffVehicle()!=KNOCKOFFVEHICLE_NEVER)
	{
		pDriver->KnockPedOffVehicle(true);
	}


	// If the driver is injured or dead, consider knocking passengers off if bike is at angle / going over a certain speed
	if( (!pDriver || pDriver->IsInjured()) )
	{ 
		TUNE_GROUP_FLOAT(BIKE_TUNE, VELOCITY_TO_KICK_PLAYER_PASSENGERS, 13.0f, 0.0f, 50.0f, 0.01f);
		bool bSpeedShouldKickPlayer = vBikeVelocity.Mag2() > rage::square(VELOCITY_TO_KICK_PLAYER_PASSENGERS);
		bool bAngleShouldKick = GetTransform().GetC().GetZf() < 0.707f;

		if (bAngleShouldKick || bSpeedShouldKickPlayer)
		{
			for( s32 iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); iSeat++ )
			{
				CPed* pPassenger = m_SeatManager.GetPedInSeat(iSeat);
				if( pPassenger && iSeat!=GetDriverSeat() && !pPassenger->IsInjured() )
				{
					if ( bAngleShouldKick || (pPassenger->IsPlayer() && bSpeedShouldKickPlayer) )
					{
						pPassenger->KnockPedOffVehicle(false);
					}
				}
			}
		}
	}



	float fLeanCgFwd = 0.0f;
	if(GetStatus()==STATUS_PLAYER)
	{
		ProcessDriverInputsForStability(fTimeStep, fLeanCgFwd, vBikeVelocity);
	}
	else if(pDriver)// for AI do some autoleveling if in the air.
	{
		if(IsInAir())
		{
			AutoLevelAndHeading(pHandling->GetBikeHandlingData()->m_fInAirSteerMult);
		}
	}

	// process cg position
	if(GetVehicleFragInst()->GetCached() && GetStatus() != STATUS_WRECKED)
	{		
		Vector3 vecCgOffset(RCC_VECTOR3(pHandling->m_vecCentreOfMassOffset));
		if(pDriver)
		{
			fLeanCgFwd *= Selectf(fLeanCgFwd, pHandling->GetBikeHandlingData()->m_fLeanFwdCOMMult, pHandling->GetBikeHandlingData()->m_fLeanBakCOMMult);
			vecCgOffset.y += fLeanCgFwd;
		}
		else
		{
			static dev_float sfBikeNoDriverCOMRaise = 0.1f;
			if(GetVehicleType() == VEHICLE_TYPE_BIKE)
			{
				vecCgOffset.Set(0.0f, 0.0f, sfBikeNoDriverCOMRaise);// Raise COM when no driver to stop the bike tipping around when being walked over.
			}
			else
			{
				vecCgOffset.Set(0.0f, 0.0f, -sfBikeNoDriverCOMRaise);
			}
		}

		GetVehicleFragInst()->GetArchetype()->GetBound()->SetCGOffset(RCC_VEC3V(vecCgOffset));
		phCollider *pCollider = GetCollider();
		if(pCollider)
			pCollider->SetColliderMatrixFromInstance();
	}

	float fSideSpeed = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetA()), vBikeVelocity);
	static dev_float sfMaxSideSpeedForStabilisingForce = 7.0f;

	// do extra stability while braking (like a rudder effect)
	if( !IsInAir() && m_vehControls.m_brake > 0.0f && vBikeVelocity.Mag2() > 0.1f )
	{
		Vector3 vecBrakeStabiliseTorque(VEC3V_TO_VECTOR3(GetTransform().GetC()));

		if( fSideSpeed < sfMaxSideSpeedForStabilisingForce )
		{
			vecBrakeStabiliseTorque.Scale(fSideSpeed * m_vehControls.m_brake * pHandling->GetBikeHandlingData()->m_fBrakingStabilityMult * GetAngInertia().z);
		}
		else
		{
			vecBrakeStabiliseTorque.Scale(sfMaxSideSpeedForStabilisingForce * m_vehControls.m_brake * pHandling->GetBikeHandlingData()->m_fBrakingStabilityMult * GetAngInertia().z);
		}

#if __ASSERT
		Vector3 vecTorque = vecBrakeStabiliseTorque;
		Vector3 vecAngAccel = VEC3V_TO_VECTOR3(GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecTorque)));
		vecAngAccel.Multiply(GetInvAngInertia());
		Assertf(vecAngAccel.Mag2() < square(150.0f), "Large internal torque detected [%3.2f,%3.2f,%3.2f], sideSpeed %3.2f, brake %3.2f, brakingMult %3.2f",
			vecTorque.x, vecTorque.y, vecTorque.z, fSideSpeed, m_vehControls.m_brake, pHandling->GetBikeHandlingData()->m_fBrakingStabilityMult);
#endif
		ApplyInternalTorque(vecBrakeStabiliseTorque);
	}

// Apply a torque so we are less likely to spin out
	float fFwdSpeed = DotProduct(vBikeVelocity, VEC3V_TO_VECTOR3(GetTransform().GetB()));
	float fSideSlipAngle = rage::Abs(rage::Atan2f(fSideSpeed, fFwdSpeed));

	static dev_float sfStartStabilisingTorqueAngle = 0.25f;
	static dev_float sfSpeedToStartStabilising = 2.5f;
	static dev_float sfMaxStabilisingTorqueAngle = 0.8f;

	if( !IsInAir() && fSideSlipAngle > sfStartStabilisingTorqueAngle && !GetHandBrake() && fFwdSpeed > sfSpeedToStartStabilising)
	{
		fSideSlipAngle = Clamp(fSideSlipAngle, 0.0f, sfMaxStabilisingTorqueAngle);// make sure the angle doesn't get too big as it will mean we apply too much torque.
		float fSlipOverAngle = fSideSlipAngle - sfStartStabilisingTorqueAngle;

		float fSideSpeed = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetA()), vBikeVelocity);
		Vector3 vecBrakeStabiliseTorque(VEC3V_TO_VECTOR3(GetTransform().GetC()));

// Reduce the stabilizing force after the handbrake has been used.
		static dev_float sfHandbrakeStabiliseFalloffTime = 2000.0f;	// milliseconds
		float fStabiliseMult = 1.0f;

		float fTime = (float) (fwTimer::GetTimeInMilliseconds() - m_uHandbrakeLastPressedTime);
		if(fTime >= 0.0f && fTime < sfHandbrakeStabiliseFalloffTime)
		{
			fTime /= sfHandbrakeStabiliseFalloffTime;

			fStabiliseMult *= fTime;
		}

		float fAccel = rage::Clamp(fSideSpeed * fStabiliseMult * fSlipOverAngle * pHandling->GetBikeHandlingData()->m_fBrakingStabilityMult, -25.0f, 25.0f);
		vecBrakeStabiliseTorque.Scale(fAccel * GetAngInertia().z);

		ApplyInternalTorque(vecBrakeStabiliseTorque);
	}

	ProcessBikeJumping();
		
	// Reduce how much you can wheelie on the Thrust to fix B*1868615
	if(GetModelIndex() == MI_BIKE_THRUST)
	{
		if(GetWheel(0)->GetIsTouching() && !GetWheel(1)->GetIsTouching())
		{
			static dev_float sfBikeLeanMaxSpeed = 25.0f;
			static dev_float sfBikeLeanMaxSpeedBlendOut = 10.0f;
			static dev_float sfBikeLeanMaxSpeedforceReduction = 0.5f;
			if( fFwdSpeed > sfBikeLeanMaxSpeed )
			{
				float fWheelieMult = 0.0f;
				if(fFwdSpeed < (sfBikeLeanMaxSpeed + sfBikeLeanMaxSpeedBlendOut))
				{
					fWheelieMult = (fFwdSpeed - sfBikeLeanMaxSpeed)/(sfBikeLeanMaxSpeed + sfBikeLeanMaxSpeedBlendOut);
				}
				fDriveForce *= sfBikeLeanMaxSpeedforceReduction + fWheelieMult * sfBikeLeanMaxSpeedforceReduction;
			}
		}
	}

	
	// update inputs for each wheel
	float fSpeedSqr = vBikeVelocity.Mag2();
	for(int i=0; i<GetNumWheels(); i++)
	{
		GetWheel(i)->SetSteerAngle(GetSteerAngle());
		GetWheel(i)->SetMaterialFlags();

		float fTotalBrakeForce = GetBrake() * fBrakeForce;
		
		float fDriveForceMult = HasGlider() ? 1.0f : 1.0f - GetSpecialFlightModeRatio(); // for the hover bike reduce wheel drive force when it is hover mode.
		float fGripMult = 1.0f;

		if(GetWheel(i)->GetFrontRearSelector() < 0.0f)//rear tyre, check rear brake
		{
			if(m_fRearBrake > GetBrake())// rear brake is higher than the normal brake so use the higher rear brake force instead.
			{
				fTotalBrakeForce = fBrakeForce * m_fRearBrake;
			}
		
			//Make it easier to ride up slopes from slow speed on bicycles
			if(GetVehicleType() == VEHICLE_TYPE_BICYCLE)
			{
				static dev_float fMultiplyTorqueSpeed = 5.0f;
				static dev_float fMultiplyTorqueAmount = 10.0f;
				static dev_float fMultiplyGripAmount = 2.0f;
				if(fFwdSpeed < fMultiplyTorqueSpeed)
				{
					float fVerticality = DotProduct(ZAXIS, VEC3V_TO_VECTOR3(GetTransform().GetB()));
					if(fVerticality > 0.0f)
					{
						float fSpeedMultiplier = ((fMultiplyTorqueSpeed - fFwdSpeed)/fMultiplyTorqueSpeed) * fVerticality;
						fGripMult = 1.0f + fSpeedMultiplier * fMultiplyGripAmount;
						fDriveForceMult = 1.0f + fSpeedMultiplier * fMultiplyTorqueAmount;
					}
				}
				// GTAV - HACK - Please remove this. Rather than the stoppie distance being skill based. We'll hack in a random number so that they can't be too long.
				if( ms_bDisableTrickForces &&
					GetWheel(1)->GetIsTouching() && !GetWheel(0)->GetIsTouching() )
				{
					fTotalBrakeForce += fTotalBrakeForce * fwRandom::GetRandomNumberInRange( -0.15f, 0.15f );
				}
			}
		}

		GetWheel(i)->SetBrakeAndDriveForce(fTotalBrakeForce, fDriveForce * fDriveForceMult, GetThrottle(), fSpeedSqr);

        GetWheel(i)->SetGripMult(fGripMult);

        GetWheel(i)->UpdateGroundVelocity();


		if(BIKE_USE_ABS && GetCheatFlag(VEH_CHEAT_ABS|VEH_SET_ABS))
		{
			if(CPhysics::GetIsFirstTimeSlice(nTimeSlice))
				GetWheel(i)->GetDynamicFlags().ClearFlag(WF_ABS_ACTIVE);

			GetWheel(i)->GetDynamicFlags().SetFlag(WF_ABS);
		}
		else
			GetWheel(i)->GetDynamicFlags().ClearFlag(WF_ABS);		

		if((GetHandBrake() && !(pHandling->hFlags & HF_NO_HANDBRAKE)) || m_fRearBrake > 0.0f )
		{
			GetWheel(i)->GetDynamicFlags().ClearFlag(WF_CHEAT_TC);
			GetWheel(i)->GetDynamicFlags().ClearFlag(WF_CHEAT_SC);
			GetWheel(i)->SetHandBrakeForce(pHandling->m_fHandBrakeForce);
		}
		else
		{
			if( !(pHandling->hFlags & HF_FORCE_NO_TC_OR_SC) )
			{
				GetWheel(i)->GetDynamicFlags().SetFlag(	WF_CHEAT_TC );
				GetWheel(i)->GetDynamicFlags().SetFlag(	WF_CHEAT_SC );
			}
		}
	}

	// if speed is Zero collision will not have been processed -> need to force if not skipping physics
	if(!m_nVehicleFlags.bVehicleColProcessed)
	{
		ProcessProbes(MAT34V_TO_MATRIX34(GetMatrix()));
	}

#if __DEV
	//PF_SET(BikeSteerInput, GetSteerAngle());
	//PF_SET(BikeLean, -m_fBikeLeanAngle);

	if(GetWheel(0)->GetIsDriveWheel())
	{
		//PF_SET(BikeRearWheelSpeed, GetWheel(0)->GetGroundSpeed());
	}

	if(GetWheel(1)->GetIsDriveWheel())
	{
		//PF_SET(BikeRearWheelSpeed, GetWheel(1)->GetGroundSpeed());
	}

	//PF_SET(BikeGroundSpeed, vBikeVelocity.Mag());
	//PF_SET(BikeGear, m_Transmission.GetGear());
	//PF_SET(BikeDriveForce, fDriveForce);
#endif

	if(phCollider* pCollider = GetCollider())
	{
#if APPLY_GRAVITY_IN_INTEGRATOR
		if(pCollider->IsArticulated())
			pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, true);
		else
			pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
#else
		pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, false);
		pCollider->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
#endif

        // Make sure the wheels are setup for Franklin's special ability.
        ProcessSlowMotionSpecialAbilityPrePhysics(pDriver);

		CWheelIntegrator::ProcessIntegration(GetCollider(), this, m_WheelIntegrator, fTimeStep, fwTimer::GetFrameCount());
		return PHYSICS_NEED_SECOND_PASS;
	}

	return ProcessPhysics2(fTimeStep, nTimeSlice);
}

RAGETRACE_DECL(CBike_ProcessPostPhysics);

//
ePhysicsResult CBike::ProcessPhysics2(float fTimeStep, int nTimeSlice)
{
	Assert(!m_vehicleAiLod.IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy));

	RAGETRACE_SCOPE(CBike_ProcessPostPhysics);	

	bool bDoStabilisation = false;

	bool bPedEnteringSeat = false;
	CComponentReservation* pComponentReservation = GetComponentReservationMgr() ? GetComponentReservationMgr()->GetSeatReservationFromSeat(0) : NULL;
	if(pComponentReservation && pComponentReservation->GetPedUsingComponent())
	{
		bPedEnteringSeat = true;
	}
	if( GetDriver() || m_nBikeFlags.bOnSideStand || m_nBikeFlags.bGettingPickedUp || bPedEnteringSeat )
	{
		bDoStabilisation = ( HasGlider() || GetSpecialFlightModeRatio() < 0.75f );
	}
	if( GetAttachParentForced() )
	{
		bDoStabilisation = false;
	}

	Vector3 vecTyreForceApplied;
	vecTyreForceApplied.Zero();

	phCollider* collider = GetCollider();
	if (collider)
	{
		CWheelIntegrator::ProcessResult(m_WheelIntegrator, &vecTyreForceApplied);
#if PHARTICULATEDBODY_MULTITHREADED_VALDIATION
		if(collider->IsArticulated())
		{
			static_cast<phArticulatedCollider*>(collider)->GetBody()->SetReadOnlyOnMainThread(false);
		}
#endif // PHARTICULATEDBODY_MULTITHREADED_VALDIATION
    }

	Vector3 vBikeVelocity = GetVelocityIncludingReferenceFrame();

	if(GetSkeleton() && !IsDummy())
	{
		Mat34V matrix = GetVehicleFragInst()->GetMatrix();
		float fWheelExtension = CWheel::ms_fWheelExtensionRate*fTimeStep;

		for(int i=0; i<GetNumWheels(); i++)
		{
			if(GetWheel(i)->GetFragChild() > -1)
				GetWheel(i)->ProcessPreSimUpdate(NULL, collider, matrix, fWheelExtension);
		}
	}

	// Non-articulated vehicles already should have maximum extents calculated.
	// Articulated vehicles will be out of date until they are integrated prior to collision. Since this 
	//   is called from the PreSimUpdate there isn't anything testing against the extents until collision.
	if(GetVehicleFragInst()->GetCached() && CVehicle::ms_bAlwaysUpdateCompositeBound)
	{
		static_cast<phBoundComposite*>(GetVehicleFragInst()->GetCacheEntry()->GetBound())->CalculateCompositeExtents(true);
		if(CVehicle::ms_bUseAutomobileBVHupdate)
		{
			CPhysics::GetLevel()->UpdateCompositeBvh(GetVehicleFragInst()->GetLevelIndex());
		}
		else
		{
			CPhysics::GetLevel()->RebuildCompositeBvh(GetVehicleFragInst()->GetLevelIndex());
		}

		CPhysics::GetLevel()->UpdateObjectLocationAndRadius(GetVehicleFragInst()->GetLevelIndex(),(Mat34V_Ptr)(NULL));
	}

// Calculate average ground normal

    u32 nNumWheelsOnGround = 0;

	static dev_float fHitNormalSmoothing = 0.3f;
	for(int i=0; i<GetNumWheels(); i++)
	{
		if(GetWheel(i)->GetWasTouching())
		{
			m_vecAverageGroundNormal += (GetWheel(i)->GetHitNormal()*fHitNormalSmoothing)*fTimeStep;
			nNumWheelsOnGround++;
		}
	}

	static dev_bool sbForceGroundNormal = true;
	static dev_float BIKE_VELOCITY_TO_USE_GROUND_NORMAL_SQ = 1.0f;
	if(sbForceGroundNormal && vBikeVelocity.Mag2() > BIKE_VELOCITY_TO_USE_GROUND_NORMAL_SQ)
	{
		m_vecAverageGroundNormal.Set(0.0f,0.0f,1.0f);
	}
	else
	{
		if(m_vecAverageGroundNormal.IsZero())
			m_vecAverageGroundNormal.Set(0.0f,0.0f,1.0f);
		else
			m_vecAverageGroundNormal.Normalize();
	}

    float fDesiredLeanAngle = 0.0f;
    DoStabilisation(bDoStabilisation, fDesiredLeanAngle, nNumWheelsOnGround, vBikeVelocity, vecTyreForceApplied, fTimeStep);
    m_vecInternalForce.Zero();
    m_vecInternalTorque.Zero();
	// stability can set internal torques and forces for processing next timeslice

	// apply stabilising force to keep bike upright
	ProcessStability(bDoStabilisation, fDesiredLeanAngle, vBikeVelocity, fTimeStep);

	// wheel ratios have been reset, so ProcessEntityColision need to be 
	// called again before wheel ratios can been used
	m_nVehicleFlags.bVehicleColProcessed = false;
	m_nVehicleFlags.bRestingOnPhysical = false;


    // Make sure the wheels are reset from Franklin's special ability.
    ProcessSlowMotionSpecialAbilityPostPhysics();

	// increase the Solver ang inertia so when the chassis hits the ground we dont spin.
	if(GetCollider())
	{	
//		phCollider* pCollider = GetCollider(); 
//
//		if( pCollider )
//		{
//			TUNE_GROUP_FLOAT( BIKE_STUNT_MODIFIERS, MAX_GRAVITY_REDUCTION, 0.0f, 0.0f, 2.0f, 0.1f );
//			//pCollider->SetGravityFactor( 1.0f - ( MAX_GRAVITY_REDUCTION * ( m_fOffStuntTime / ms_TimeBeforeLeavingStuntMode ) ) );
//			//ms_fBikeGravityMult = pCollider->GetGravityFactor();
//			static float initialGravity = GetGravityForWheellIntegrator();
//			float gravityScale = 1.0f - ( MAX_GRAVITY_REDUCTION * ( m_fOffStuntTime / ms_TimeBeforeLeavingStuntMode ) );
//			SetGravityForWheellIntegrator( gravityScale * initialGravity );
//		}
	
//		static dev_float sfAngularVelocityClamp = 0.25f;
//		Vector3 vAngVelocity = RCC_VECTOR3(GetCollider()->GetAngVelocity());
//		static float sfPreviousAngVelocityZ = 0.0f;
//		
//	
//		float zAngularDeltaVelocity = sfPreviousAngVelocityZ - vAngVelocity.z;
//
//		zAngularDeltaVelocity = rage::Clamp(zAngularDeltaVelocity, -sfAngularVelocityClamp, sfAngularVelocityClamp);
//
//		vAngVelocity.SetZ(sfPreviousAngVelocityZ - zAngularDeltaVelocity);
//
//		GetCollider()->SetAngVelocity(vAngVelocity);
//		SetAngVelocity(vAngVelocity);
//
//
//		sfPreviousAngVelocityZ = vAngVelocity.z;
//
//#if __DEV
//
//		PF_SET(BikeSideAng, zAngularDeltaVelocity);
//#endif


#if __DEV

		//PF_SET(BikeRearWheelTouching, (float)GetWheel(0)->GetWasTouching());
		//PF_SET(BikeRearWheelCompression, (float)GetWheel(0)->GetWheelCompression());
#endif
		

		dev_float BIKE_VELOCITY_TO_USE_ANG_INERTIA_OVERRIDE_SQ = 9.0f;
		GetCollider()->UseSolverInvAngInertiaResetOverride( ((GetDriver() && vBikeVelocity.Mag2() > BIKE_VELOCITY_TO_USE_ANG_INERTIA_OVERRIDE_SQ)? true : false) );
	}
	
	if(CPhysics::GetIsLastTimeSlice(nTimeSlice))
	{
		// Turn back on lateral springs, in case they were turned off due to collision.
		for(int i=0; i < GetNumWheels(); i++)
		{ 
			GetWheel(i)->GetDynamicFlags().ClearFlag(WF_NO_LATERAL_SPRING);
		}
	}


	if(m_nVehicleFlags.bMayHaveWheelGroundForcesToApply)
	{
		return PHYSICS_NEED_THIRD_PASS;
	}
	else
	{
		return PHYSICS_DONE;
	}
}

void CBike::ProcessPreComputeImpacts( phContactIterator rawImpacts )
{
	int count = rawImpacts.CountElements();
	phCachedContactIterator impacts;
	phCachedContactIteratorElement *buffer = Alloca( phCachedContactIteratorElement, count );

	impacts.InitFromIterator( rawImpacts, buffer, count );
	impacts.Reset();

	while( !impacts.AtEnd() )
	{
		if( IsWeaponBladeImpact( impacts ) )
		{
			phInst* otherInstance = impacts.GetOtherInstance();
			CEntity* pOtherEntity = otherInstance ? (CEntity*)otherInstance->GetUserData() : NULL;

			//if( pOtherEntity &&
			//	( pOtherEntity->GetIsTypeBuilding() || pOtherEntity->GetIsAnyFixedFlagSet() ) )
			if( !pOtherEntity ||
				( !pOtherEntity->GetIsTypeVehicle() &&
				  !pOtherEntity->GetIsTypePed() ) )
			{
				impacts.DisableImpact();
			}
			else if( !CVehicle::sm_bDisableWeaponBladeForces ||
					 !pOtherEntity->GetIsTypePed() )
			{
				Vec3V contactNormal;
				impacts.GetMyNormal( contactNormal );

				contactNormal = GetTransform().UnTransform3x3( contactNormal );
				contactNormal.SetYf( 0.0f );

				if( contactNormal.GetXf() > 0.0f )
				{
					contactNormal = NormalizeSafe( contactNormal, Vec3V( 1.0f, 0.0f, 0.0f ) );
				}
				else
				{
					contactNormal = NormalizeSafe( contactNormal, Vec3V( -1.0f, 0.0f, 0.0f ) );
				}
				contactNormal = GetTransform().Transform3x3( contactNormal );

				impacts.SetMyNormal( contactNormal );
				impacts.SetFriction( 0.0f );
			}
		}
		++impacts;
	}

	impacts.Reset();

	CVehicle::ProcessPreComputeImpacts( rawImpacts );
}


//
ePhysicsResult CBike::ProcessPhysics3(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	// If the ground wheel force application has been deferred, do it now
	if(phCollider* pCollider = GetCollider())
	{
		CWheelIntegrator::ProcessResultsFinal( pCollider, m_ppWheels, GetNumWheels(), ScalarVFromF32(fTimeStep) );
	}
	m_nVehicleFlags.bMayHaveWheelGroundForcesToApply = false;

	return PHYSICS_DONE;
}

void CBike::ProcessPostPhysics()
{
	CVehicle::ProcessPostPhysics();

	m_fVerticalVelocityToKnockOffThisBike = CVehicle::ms_fVerticalVelocityToKnockOffVehicle;

	if( !HasGlider() )
	{
		m_fVerticalVelocityToKnockOffThisBike += ( ms_fVerticalVelocityToKnockOffHoverBike - m_fVerticalVelocityToKnockOffThisBike ) * GetSpecialFlightModeRatio();
	}
}

void CBike::DoStabilisation(bool bDoStabilisation, float &fDesiredLeanAngle, u32 nNumWheelsOnGround, Vector3 &vBikeVelocity, Vector3 &vTyreForceApplied, float fTimeStep)
{
    const float speed = Abs(vBikeVelocity.Mag()); 
    if(bDoStabilisation)
    {
        Vector3 vecGroundRight;
        vecGroundRight.Cross(VEC3V_TO_VECTOR3(GetTransform().GetB()), m_vecAverageGroundNormal);
        vecGroundRight.Normalize();

        //calculate ground lean angle
        float fGroundLeanAngle = -rage::Atan2f(vecGroundRight.z, vecGroundRight.XYMag());

        //something to smooth leaning?
        float timeStep = fTimeStep;
        // If we are using Franklin's special ability we shouldn't slow down the leaning.
        if( GetDriver() && GetDriver()->GetSpecialAbility() && GetDriver()->GetSpecialAbility()->GetType() == SAT_CAR_SLOWDOWN )
        {
            if( GetDriver()->GetSpecialAbility()->IsActive() )
            {
                timeStep = fwTimer::GetNonPausableCamTimeStep()/CPhysics::GetNumTimeSlices();
            }
        }

        float fGroundLeanRate = rage::Powf(BIKE_TEST_GROUND_LEAN_SMOOTHING, timeStep);
        m_fGroundLeanAngle = fGroundLeanRate*m_fGroundLeanAngle + (1.0f - fGroundLeanRate)*fGroundLeanAngle;

		bool bBikeConsideredStill = IsConsideredStill(vBikeVelocity);

	    float fBikeLeanAngle = 0.0f;
        //deal with the bike when on its stand
        if(m_nBikeFlags.bGettingPickedUp || m_nBikeFlags.bOnSideStand)
        {
            if(m_nBikeFlags.bGettingPickedUp)
            {
                float fDoorAngle = GetDoor(0)->GetDoorAngle();
                fBikeLeanAngle = fDoorAngle;
				if(!m_nBikeFlags.bOnSideStand)
				{
					m_fGroundLeanAngle = 0.0f;
				}
            }
            else if(m_nBikeFlags.bOnSideStand)
            {
                fBikeLeanAngle = pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle;
                GetDoor(0)->SetBikeDoorConstrained();
            }
        }//debug force leaning
        else if(CVehicle::ms_bForceBikeLean)
        {
			//reduce lean at low speed.
            static dev_float sfReduceLeanAtSpeedBelow = 10.0f;
			//static dev_float sfSpeedToConsiderBikeStill = 0.5f;

            float fLeanMultiplier = 1.0f;

			TUNE_GROUP_BOOL(BIKE_TUNE, DISABLE_LEAN, false);
			if (!DISABLE_LEAN && GetLayoutInfo() && GetLayoutInfo()->GetBikeLeansUnlessMoving())
			{
				if(bBikeConsideredStill)
				{
					fBikeLeanAngle = pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle;
				}
				else
				{
					if(speed < sfReduceLeanAtSpeedBelow)
					{
						fLeanMultiplier = speed/sfReduceLeanAtSpeedBelow;
					}

					fBikeLeanAngle = (-pHandling->GetBikeHandlingData()->m_fMaxBankAngle * m_vehControls.m_steerAngle / pHandling->m_fSteeringLock) * fLeanMultiplier;
				}
			}
			else
			{
				if(speed < sfReduceLeanAtSpeedBelow)
					fLeanMultiplier = speed/sfReduceLeanAtSpeedBelow;

				fBikeLeanAngle = (-pHandling->GetBikeHandlingData()->m_fMaxBankAngle * m_vehControls.m_steerAngle / pHandling->m_fSteeringLock) * fLeanMultiplier;
			}
        }
        else
        {
            float fSideForce = 0.0f;
            float fGravityMult = 1.0f; 
            if( GetDriver() && GetDriver()->GetSpecialAbility() && GetDriver()->GetSpecialAbility()->GetType() == SAT_CAR_SLOWDOWN )
            {
                if(GetDriver()->GetSpecialAbility()->IsActive())
                {
                    static dev_float sfGravityMult = 3.0f;
                    fGravityMult = sfGravityMult;
                }
            }
            float fGravitiyForce = rage::Abs(m_fGravityForWheelIntegrator*fGravityMult) * GetMass();

            if(nNumWheelsOnGround==0)
            {
				static dev_float MIN_STEER_ANGLE_FOR_AIR_LEAN = 0.1f;
				if(Abs(m_vehControls.m_steerAngle) > MIN_STEER_ANGLE_FOR_AIR_LEAN)
				{
					fSideForce = -(m_vehControls.m_steerAngle/pHandling->m_fSteeringLock) * 0.5f * fGravitiyForce;
				}
            }
            else
            {
                // smooth ground right
                // Use collider matrix because it is most up to date
                Matrix34 mat(Matrix34::IdentityType);
                mat.RotateZ(GetTransform().GetHeading());

                fSideForce = vTyreForceApplied.Dot(mat.a);

                float fLRInput = 0.0f;
                bool bUseSteerForLean = true;
    #if USE_SIXAXIS_GESTURES
                // Sixaxis overwrites stick lean in this case
                // Allows seperate control of handlebars + lean
                if(GetDriver() && GetDriver()->IsLocalPlayer() && CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_BIKE))
                {
                    CPadGesture* gesture = CControlMgr::GetPlayerPad()->GetPadGesture();
                    if(gesture)
                    {
                        fLRInput = -gesture->GetPadRollInRange(CBike::ms_fMotionControlRollMin,CBike::ms_fMotionControlRollMax,true);
                        bUseSteerForLean = false;
                    }					
                }
    #endif // USE_SIXAXIS_GESTURES
                if (bUseSteerForLean)
                {
					// No sixaxis - just use stick
					fLRInput = (m_vehControls.m_steerAngle/pHandling->m_fSteeringLock);
                }

                static dev_float MIN_BIKE_LEAN_VELOCITY_MAX_EST_MULT = 0.5f;
				static dev_float MIN_BICYCLE_LEAN_VELOCITY_MAX_EST_MULT = 0.75f;

				float fMinBikeLeanVelocity = 0.0f;
				if(GetVehicleType() == VEHICLE_TYPE_BICYCLE)
				{
					fMinBikeLeanVelocity = pHandling->m_fInitialDriveMaxFlatVel * MIN_BICYCLE_LEAN_VELOCITY_MAX_EST_MULT;
				}
				else
				{
					fMinBikeLeanVelocity = pHandling->m_fInitialDriveMaxFlatVel * MIN_BIKE_LEAN_VELOCITY_MAX_EST_MULT;
				}

				//don't take into account the camber force of the tyre when working out lean.
				for(int i=0; i<GetNumWheels(); i++)
				{
					fSideForce -= GetWheel(i)->GetCamberForce();
				}

                if(speed > fMinBikeLeanVelocity )
                {
                    fSideForce -= fLRInput * pHandling->GetBikeHandlingData()->m_fStickLeanMult * fGravitiyForce;
                }
                else
                {
                    static dev_float MIN_BIKE_LEAN_SIDE_FORCE_SCALE = 0.0f;
                    float sideForceScale = 1.0f - ((fMinBikeLeanVelocity - speed) / fMinBikeLeanVelocity);
                    fSideForce -= (fLRInput * pHandling->GetBikeHandlingData()->m_fStickLeanMult * fGravitiyForce) * sideForceScale;
                    fSideForce *= MIN_BIKE_LEAN_SIDE_FORCE_SCALE + Clamp(sideForceScale, 0.0f, 1.0f - MIN_BIKE_LEAN_SIDE_FORCE_SCALE);
                }

				if(GetVehicleType() == VEHICLE_TYPE_BICYCLE)
				{
					// Want to avoid lean with no steering.
					static dev_float MIN_BIKE_LEAN_SIDE_FORCE_LR_INPUT_BICYCLE = 0.01f;
					if( Abs(fLRInput) < MIN_BIKE_LEAN_SIDE_FORCE_LR_INPUT_BICYCLE )
					{
						fSideForce = 0.0f;
					}
				}
				else
				{
					// Want to avoid small oscillations in bike lean angle around about 0
					static dev_float MIN_BIKE_LEAN_SIDE_FORCE = 6.0f;
					static dev_float MIN_BIKE_LEAN_SIDE_FORCE_LR_INPUT = 0.00001f;
					if( Abs(fLRInput) < MIN_BIKE_LEAN_SIDE_FORCE_LR_INPUT && Abs(fSideForce / GetMass()) < MIN_BIKE_LEAN_SIDE_FORCE)
					{
						fSideForce = 0.0f;
					}
				}

            }

            float fFwdSpeed = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetB()), vBikeVelocity);
			const bool bShouldPreventBikeFromLeaningOver = GetDriver() && GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PreventBikeFromLeaning);

			if(ms_bLeanBikeWhenDoingBurnout && rage::Abs(m_fBurnoutTorque) > 0.0f)
			{
				fBikeLeanAngle = rage::Atan2f(-m_fBurnoutTorque, fGravitiyForce);
			}
			else if (!bShouldPreventBikeFromLeaningOver && GetLayoutInfo() && GetLayoutInfo()->GetBikeLeansUnlessMoving() && bBikeConsideredStill)// If the bike is still, we want to keep the bike at its default lean angle until it begins to move
			{
				fBikeLeanAngle = pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle;
			}
			else if(bShouldPreventBikeFromLeaningOver || fFwdSpeed < 0.0f)//Going in reverse so don't lean
            {
                fBikeLeanAngle = 0.0f;
            }
            else
			{
				fBikeLeanAngle = rage::Atan2f(fSideForce, fGravitiyForce);
			}

            GetDoor(0)->SetBikeDoorConstrained();
    #if __DEV
            static float sbForceUprightAngle = -10.0f;
            if(sbForceUprightAngle > -HALF_PI)
                fBikeLeanAngle = sbForceUprightAngle;
    #endif
        }

        // for network clones we calculate the lean angle based on orientation updates received from the remote machine
        // and what the network blender is doing.
        if(IsNetworkClone() && !GetNetworkObject()->NetworkBlenderIsOverridden())
        {
            m_fBikeLeanAngle = fBikeLeanAngle = static_cast<CNetObjBike *>(GetNetworkObject())->GetLeanAngle();
        }

        // This smooths the m_fBikeLeanAngle
        // Lower desLeanReturnFrac = quicker change in lean angle
		float fDesLeanReturnFrac = pHandling->GetBikeHandlingData()->m_fDesLeanReturnFrac;

		if (m_nBikeFlags.bGettingPickedUp)
		{
			if (GetVehicleType() == VEHICLE_TYPE_BICYCLE)
			{
				TUNE_GROUP_FLOAT(BIKE_TUNE, PICK_PULL_BICYCLE_LEAN_RETURN_FRAC, 0.0f, 0.0f, 1.0f, 0.0001f);
				fDesLeanReturnFrac = PICK_PULL_BICYCLE_LEAN_RETURN_FRAC;
			}
			else
			{
				TUNE_GROUP_FLOAT(BIKE_TUNE, PICK_PULL_BIKE_LEAN_RETURN_FRAC, 0.0f, 0.0f, 1.0f, 0.0001f);
				fDesLeanReturnFrac = PICK_PULL_BIKE_LEAN_RETURN_FRAC;
			}
		}

        float fBikeLeanRate = 0.0f;
		if(!m_nBikeFlags.bGettingPickedUp)
		{
			bool bLocalPlayer = GetDriver() && GetDriver()->IsLocalPlayer();
			if(bLocalPlayer)
			{
				TUNE_FLOAT(s_AIBikeLeanTune, 0.1f, 0.0f, 10.0f, 0.01f);
				fDesLeanReturnFrac = Clamp(fDesLeanReturnFrac + s_AIBikeLeanTune, 0.0f, 0.99f);
			}

			fBikeLeanRate = rage::Powf(fDesLeanReturnFrac, timeStep);

			//smooth out leaning at extents 
			if(rage::Abs(fBikeLeanAngle) > rage::Abs(m_fBikeLeanAngle) && Sign(fBikeLeanAngle) == Sign(m_fBikeLeanAngle))
			{   
				float fLimitedLean = rage::Abs(m_fBikeLeanAngle);
				if(fLimitedLean > pHandling->GetBikeHandlingData()->m_fMaxBankAngle)
					fLimitedLean = pHandling->GetBikeHandlingData()->m_fMaxBankAngle;

				float fLeanFraction = fLimitedLean / pHandling->GetBikeHandlingData()->m_fMaxBankAngle;
	            
				float fLeanAdjust = (1.0f - fBikeLeanRate) * SlowIn( fLeanFraction );
				fBikeLeanRate += fLeanAdjust;
			} 
			else if(rage::Abs(fBikeLeanAngle) < rage::Abs(m_fBikeLeanAngle) && (!bLocalPlayer || (bLocalPlayer && fabs(m_fLeanSideInput) < 0.01f)) )//speed up leaning back to the center when there is no steer input.
			{
				static dev_float sfRecenterLeanSpeedUp = 0.97f;
				fBikeLeanRate *= sfRecenterLeanSpeedUp;
			}
		}
		else
		{
			fBikeLeanRate = rage::Powf(fDesLeanReturnFrac, timeStep);
		}

        m_fBikeLeanAngle = fBikeLeanRate*m_fBikeLeanAngle + (1.0f - fBikeLeanRate)*fBikeLeanAngle;

        if(!m_nBikeFlags.bGettingPickedUp && !bBikeConsideredStill)
        {
            if(m_fBikeLeanAngle > pHandling->GetBikeHandlingData()->m_fMaxBankAngle)
                m_fBikeLeanAngle = pHandling->GetBikeHandlingData()->m_fMaxBankAngle;
            else if(m_fBikeLeanAngle < -pHandling->GetBikeHandlingData()->m_fMaxBankAngle)
                m_fBikeLeanAngle = -pHandling->GetBikeHandlingData()->m_fMaxBankAngle;
        }

        fDesiredLeanAngle = m_fGroundLeanAngle + m_fBikeLeanAngle;
/*
        // Deal with riding on roofs
        if(GetTransform().GetC().GetZf() < 0.0f)
        {
            fDesiredLeanAngle += PI;
        }
*/
        // limit angle
        fDesiredLeanAngle = fwAngle::LimitRadianAngleFast(fDesiredLeanAngle);


    #if __DEV
        if(GetStatus() == STATUS_PLAYER && sbBikeDebugDisplayLeanVectors)
        {
            Vector3 vecDisplayPos, vecDisplayLean, vecDisplayRight, vecDisplayUp;
            Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
            vecDisplayPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) - 0.5f*vecForward;
            vecDisplayRight.Cross(vecForward, Vector3(0.0f,0.0f,1.0f));
            vecDisplayRight.Normalize();
            vecDisplayUp.Cross(vecDisplayRight, vecForward);
            vecDisplayUp.Normalize();

            // RED == Ground normal
            grcDebugDraw::Line(vecDisplayPos, vecDisplayPos + m_vecAverageGroundNormal, Color32(1.0f,0.0f,0.0f), -1);

            // GREEN == Lean angle due to ground
            vecDisplayPos -= 0.1f*vecForward;
            vecDisplayLean = rage::Cosf(m_fGroundLeanAngle) * vecDisplayUp + rage::Sinf(m_fGroundLeanAngle) * vecDisplayRight;
            grcDebugDraw::Line(vecDisplayPos, vecDisplayPos + vecDisplayLean, Color32(0.0f,1.0f,0.0f), -1);

            // BLUE == Lean angle due to player input
            vecDisplayPos -= 0.1f*vecForward;
            vecDisplayLean = rage::Cosf(m_fBikeLeanAngle) * vecDisplayUp + rage::Sinf(m_fBikeLeanAngle) * vecDisplayRight;
            grcDebugDraw::Line(vecDisplayPos, vecDisplayPos + vecDisplayLean, Color32(0.0f,0.0f,1.0f), -1);

            // CYAN == Total desired lean angle
            vecDisplayPos -= 0.1f*vecForward;
            vecDisplayLean = rage::Cosf(fDesiredLeanAngle) * vecDisplayUp + rage::Sinf(fDesiredLeanAngle) * vecDisplayRight;
            grcDebugDraw::Line(vecDisplayPos, vecDisplayPos + vecDisplayLean, Color32(0.0f,1.0f,1.0f), -1);

            // WHITE == Current lean angle
            vecDisplayPos -= 0.1f*vecForward;
            float fRoll = -GetTransform().GetRoll();
            vecDisplayLean = rage::Cosf(fRoll) * vecDisplayUp + rage::Sinf(fRoll) * vecDisplayRight;
            grcDebugDraw::Line(vecDisplayPos, vecDisplayPos + vecDisplayLean, Color32(1.0f,1.0f,1.0f), -1);

            // yellow == Tyre force applied
            vecDisplayPos -= 0.1f*vecForward;
            vecDisplayLean = vTyreForceApplied;
            grcDebugDraw::Line(vecDisplayPos, vecDisplayPos + vTyreForceApplied, Color32(1.0f,1.0f,0.0f), -1);
        }
    #endif
    }
    else
    {
        Vector3 RightVector(VEC3V_TO_VECTOR3(GetTransform().GetA()));
        float fFlatMag = GetTransform().GetC().GetZf() > 0.0f ? RightVector.XYMag() : -1.0f*RightVector.XYMag();
        m_fBikeLeanAngle = rage::Atan2f(-RightVector.z, fFlatMag);
        fDesiredLeanAngle = m_fBikeLeanAngle;
        GetDoor(0)->SetBikeDoorUnConstrained(m_fBikeLeanAngle);

        UpdateStoredFwdVector(MAT34V_TO_MATRIX34(GetMatrix()));
        
		m_fAnimLeanFwd = 0.0f;

		if(!GetDriver() && !m_leanConstraintHandle.IsValid())
		{
			static dev_float sfNoDriverAutoCentreMax = ( DtoR * 5.0f);
			float fDesiredChange = rage::Clamp(-GetSteerAngle(), -sfNoDriverAutoCentreMax, sfNoDriverAutoCentreMax);
			SetSteerAngle(fDesiredChange + GetSteerAngle());
		}
    }
}

void CBike::UpdatePhysicsFromEntity(bool bWarp)
{
	CVehicle::UpdatePhysicsFromEntity(bWarp);

	// reset stored forward direction
	UpdateStoredFwdVector(MAT34V_TO_MATRIX34(GetMatrix()));
}

//static float BIKE_BASE_FORCE_MULT = 10.0f;
//static float BIKE_BASE_FORCE_CAP = 5.0f;

static Vector3 svAngInertiaRaiseFactorV(0.0f,0.0f,0.0f);
static Vector3 svAngInertiaRaiseFactorC(25.0f,25.0f,25.0f);

//dev_float sfBikeMinGroundClearance = 0.25f;
bank_float sfBikeAvoidUpsideDownTorque = 10.0f;
bank_bool sbApplyLeanTorqueAboutWheels = false;
bank_bool sbApplyLeanTorqueInternal = false;

float CBike::ComputeInstabilityModifier() const
{
	// Don't accumulate instability when driverless
	if(!m_nBikeFlags.bGettingPickedUp && GetDriver() &&
		( !HasGlider() || GetSpecialFlightModeRatio() < 0.5f ) )
	{
		const CWheel& frontWheel = *GetWheelFromId(BIKE_WHEEL_F);
		const CWheel& rearWheel = *GetWheelFromId(BIKE_WHEEL_R);
		// We're always stable in midair (at least for now)
		if(frontWheel.GetWasTouching() || rearWheel.GetWasTouching() || GetFrameCollisionHistory()->GetNumCollidedEntities() > 0)
		{
			const Mat34V& bikeMatrix = GetMatrixRef();
			const Vec3V bikeRight = bikeMatrix.GetCol0();

			if( m_fOffStuntTime == 0.0f )
			{
				const Vec3V bikeForward = bikeMatrix.GetCol1();
				const Vec3V bikeUp = bikeMatrix.GetCol2();

				// Choose the most upright ground normal
				Vec3V groundUp = Vec3V(V_NEG_FLT_MAX);
				if(frontWheel.GetWasTouching())
				{
					groundUp = SelectFT(IsGreaterThan(RCC_VEC3V(frontWheel.GetHitNormal()).GetZ(),groundUp.GetZ()),groundUp,RCC_VEC3V(frontWheel.GetHitNormal()));
				}
				if(rearWheel.GetWasTouching())
				{
					groundUp = SelectFT(IsGreaterThan(RCC_VEC3V(rearWheel.GetHitNormal()).GetZ(),groundUp.GetZ()),groundUp,RCC_VEC3V(rearWheel.GetHitNormal()));
				}
				if(const CCollisionRecord* pCollisionRecord = GetFrameCollisionHistory()->GetMostSignificantCollisionRecord())
				{
					groundUp = SelectFT(IsGreaterThan(RCC_VEC3V(pCollisionRecord->m_MyCollisionNormal).GetZ(),groundUp.GetZ()),groundUp,RCC_VEC3V(pCollisionRecord->m_MyCollisionNormal));
				}
				// Prevent bad normals from getting through
				groundUp = SelectFT(IsEqual(groundUp,Vec3V(V_NEG_FLT_MAX)),groundUp,Vec3V(V_Z_AXIS_WZERO));

				// Compute the current tilt angle
				// For now the maximum tilt is just a constant, regardless of speed
				// To be instantly knocked off you must be 70 degrees offset from the ground normal AND 30 degrees offset from the z-axis.
				//   This ensures that a stray normal won't knock you off under normal conditions but we can still use the ground normal
				//   to prevent getting knocked off when going up ramps. 
				const ScalarV maxGroundTiltAngleSine = ScalarVFromF32(ms_fMaxGroundTiltAngleSine);
				const ScalarV groundTiltAngleSine = Abs(Dot(bikeForward,groundUp));
				const ScalarV minAbsoluteTiltAngleSineForKnockoff = ScalarVFromF32(ms_fMinAbsoluteTiltAngleSineForKnockoff);
				const ScalarV absoluteTiltAngleSine = Abs(bikeForward.GetZ());
				if(And(IsGreaterThan(groundTiltAngleSine,maxGroundTiltAngleSine),IsGreaterThan(absoluteTiltAngleSine,minAbsoluteTiltAngleSineForKnockoff)).Getb())
				{
					// Instantly knock the ped off if they're pointing straight up or down
					return 1000.0f;
				}

				// If the ped somehow manages to land upside down softly enough to not get knocked off, knock them off here.
				const ScalarV minBikeUpZ = Negate(ScalarV(V_HALF)); // 120 degrees
				if(IsLessThanAll(bikeUp.GetZ(),minBikeUpZ))
				{
					// Instantly knock the ped off if we're totally upside down
					return 1000.0f;
				}
			}

			// Low speed instability checks
			const float unstableLowSpeed = 5.0f;
			if(GetVelocity().Mag2() < unstableLowSpeed*unstableLowSpeed)
			{
				// Check if the ped is leaning over too far
				const ScalarV maxBikeLeanSine = ScalarVFromF32(0.707f); // 45 degrees
				if(IsGreaterThanAll(Abs(bikeRight.GetZ()),maxBikeLeanSine))
				{
					// Allow the ped to be at this angle for a short period of time before knocking them off
					return 1.0f;
				}
			}
		}
	}

	return 0.0f;
}

//
void CBike::ProcessStability(bool bDoStabilisation, float fDesiredLeanAngle, const Vector3& vecBikeVelocity, float fTimeStep)
{
    bool bApplyConstraint = bDoStabilisation;
	
	if( m_fOffStuntTime > 0.0f )
	{
		m_fOffStuntTime = Max( 0.0f, m_fOffStuntTime - fTimeStep );
	}

    if(bDoStabilisation)
    {
        bool bDoNewRollMethod = ms_bNewLeanMethod && !m_nBikeFlags.bGettingPickedUp;
        if(bDoNewRollMethod)
        {
            NewLeanMethod(fDesiredLeanAngle, fTimeStep);
            bApplyConstraint = false;
        }
        else
        {
            OldLeanMethod(fDesiredLeanAngle, fTimeStep);
        }		

        if(!IsDummy())
        {
            EnforceMinimumGroundClearance();
        }
    }
    else
    {
        // reset damping to std
        phArchetypeDamp* pCurrentArchetype = (phArchetypeDamp*)GetCurrentPhysicsInst()->GetArchetype();

        fragInstGta* pFragInst = GetVehicleFragInst();

        if(physicsVerifyf(pFragInst,"Expect bike to be a frag inst"))
        {
            pFragInst->GetTypePhysics()->ApplyDamping(pCurrentArchetype);
        }


        // reset stored forward direction
        UpdateStoredFwdVector(MAT34V_TO_MATRIX34(GetMatrix()));

        if(rage::Abs(GetTransform().GetA().GetZf()) < 0.3f && GetTransform().GetC().GetZf() < 0.0f && vecBikeVelocity.Mag2() < 1.0f && GetSpecialFlightModeRatio() < 0.5f )
        {
            Vector3 ForwardVector(VEC3V_TO_VECTOR3(GetTransform().GetB()));
            float fApplyTorque = Selectf(ForwardVector.z, -sfBikeAvoidUpsideDownTorque, sfBikeAvoidUpsideDownTorque);
            ApplyTorque(ForwardVector * GetAngInertia().y * fApplyTorque);
        }
    }

	// Determine if the bike is unstable
	float instabilityModifier = ComputeInstabilityModifier();
	if(instabilityModifier > 0.0f)
	{
		TUNE_GROUP_INT(BIKE_TUNE, MIN_TIME_RAGDOLL_MP, 1000, 0, 5000, 100);
		const u32 uMinTime = NetworkInterface::IsGameInProgress() ? MIN_TIME_RAGDOLL_MP : 2000;
		// If the bike is unstable keep track of how long. Add in the multiplier since some things we want to knock the ped off the bike faster
		m_fTimeUnstable += fTimeStep*instabilityModifier;
		if(m_fTimeUnstable > ms_fMaxTimeUnstableBeforeKnockOff)
		{
			for(s32 iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); iSeat++ )
			{
				if(CPed* pPassenger = m_SeatManager.GetPedInSeat(iSeat))
				{
					pPassenger->KnockPedOffVehicle(false, 0.0f, uMinTime);
				}
			}
			m_fTimeUnstable = 0.0f;
		}
	}
	else
	{
		// If the bike is stable, clear the timer
		m_fTimeUnstable = 0.0f;
	}

    ProcessConstraint(bApplyConstraint);
}

void CBike::NewLeanMethod( float fDesiredLeanAngle, float fTimeStep)
{
    u32 nNumWheelsOnGround = 0;

// Scale up ang inertia with rot speed of each wheel
    float fTotalWheelRotation = 0.0f;
    for(int iWheelIndex = 0; iWheelIndex < GetNumWheels(); iWheelIndex++)
    {
        CWheel* pWheel = GetWheel(iWheelIndex);
        physicsFatalAssertf(pWheel,"NULL bike wheel!");

        fTotalWheelRotation += pWheel->GetRotSpeed();

        if(pWheel->GetWasTouching())
        {
            nNumWheelsOnGround++;
        }
    }

    fragInstGta* pFragInst = GetVehicleFragInst();

    if(physicsVerifyf(pFragInst,"Expect bike to be a frag inst"))
    {
        // Get original ang inertia
        Vector3 vInertia = GetFragInst()->GetTypePhysics()->GetArchetype()->GetAngInertia();

        // Modify by rot speed to prevent roll at higher speeds
        vInertia.AddScaled(svAngInertiaRaiseFactorV, rage::Abs(fTotalWheelRotation));

        vInertia.Add(svAngInertiaRaiseFactorC);

        //pFragInst->GetArchetype()->SetAngInertia(vInertia);
        if(GetCollider())
        {
            GetCollider()->SetInertia(ScalarVFromF32(pFragInst->GetArchetype()->GetMass()).GetIntrin128(), vInertia.xyzw);
        }
    }

    if(nNumWheelsOnGround > 0)
    {
    // Apply torque to get to desired lean angle
        const float fCurrentLeanAngle = -rage::Asinf(GetTransform().GetA().GetZf());
        float fLeanTorque = fDesiredLeanAngle - fCurrentLeanAngle;
        fLeanTorque = fwAngle::LimitRadianAngle(fLeanTorque);

        // This is the desired ang speed to get there in 1 step
        fLeanTorque /= fTimeStep;

        // Subtract our current speed
        Vector3 ForwardVector(VEC3V_TO_VECTOR3(GetTransform().GetB()));
        fLeanTorque -= DotProduct(ForwardVector, GetAngVelocity());

        // Reduce this as we go vertical
        fLeanTorque *= 1.0f - rage::Abs(ForwardVector.z);

        fLeanTorque *= GetAngInertia().y;

        if(sbApplyLeanTorqueAboutWheels)
        {
            // Rotate about bottom of bikes BB
            // Get average wheel positions
            Vector3 vWheel0 = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
            Vector3 vWheel1 = vWheel0;
            Assert(GetWheel(0));
            Assert(GetWheel(1));


            float fRadius = GetWheel(0)->GetWheelPosAndRadius(vWheel0);
            GetWheel(1)->GetWheelPosAndRadius(vWheel1);

            Vector3 vRotatePosition = vWheel0 + vWheel1;
            vRotatePosition.Scale(0.5f);
            vRotatePosition -= fRadius * VEC3V_TO_VECTOR3(GetTransform().GetC());

            if(sbApplyLeanTorqueInternal)
            {
                ApplyInternalTorqueAndForce(VEC3V_TO_VECTOR3(GetTransform().GetB()) * fLeanTorque, vRotatePosition);
            }
            else
            {
                ApplyTorqueAndForce(VEC3V_TO_VECTOR3(GetTransform().GetB()) * fLeanTorque, vRotatePosition);
            }
        }
        else
        {
            if(sbApplyLeanTorqueInternal)
            {
                ApplyInternalTorque(VEC3V_TO_VECTOR3(GetTransform().GetB()) * fLeanTorque);
            }
            else
            {
                ApplyTorque(VEC3V_TO_VECTOR3(GetTransform().GetB()) * fLeanTorque);
            }

        }
    }

    phArchetypeDamp* pCurrentArchetype = (phArchetypeDamp*)GetCurrentPhysicsInst()->GetArchetype();

    if(physicsVerifyf(pFragInst,"Expect bike to be a frag inst"))
    {
        pFragInst->GetTypePhysics()->ApplyDamping(pCurrentArchetype);
    }


    pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C, BIKE_NEW_ROT_DAMP_C);
    pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, BIKE_NEW_ROT_DAMP_V);
	pCurrentArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2, IsInAir() ? BIKE_NEW_ROT_DAMP_V2_IN_AIR : BIKE_NEW_ROT_DAMP_V2);



}

void CBike::OldLeanMethod( float fDesiredLeanAngle, float fTimeStep)
{
    phInst* pPhysInst = GetCurrentPhysicsInst();

    Matrix34 stabiliseMatrix = MAT34V_TO_MATRIX34(GetMatrix());

	if(ms_bSecondNewLeanMethod)
	{
		const CWheel& frontWheel = *GetWheelFromId(BIKE_WHEEL_F);
		const CWheel& rearWheel = *GetWheelFromId(BIKE_WHEEL_R);

		// At this point in the frame the IsTouching flag has been pushed to WasTouching and then set to false
		bool isFrontWheelTouchingGround = frontWheel.GetWasTouching();
		bool isRearWheelTouchingGround = rearWheel.GetWasTouching();
		bool isEitherWheelTouchingGround = isFrontWheelTouchingGround || isRearWheelTouchingGround;
		const Vec3V vZAxis = Vec3V(V_Z_AXIS_WZERO);

		Mat34V_ConstRef currentMatrix = pPhysInst->GetMatrix();
		const Vec3V currentForward = currentMatrix.GetCol1();

		// Get a ground normal, we will use this to compute our ideal right vector. The ideal right vector should be perpendicular
		//   to the ground as well as perpendicular to the forward
		Vec3V groundNormal;
		if(isEitherWheelTouchingGround && !m_nBikeFlags.bGettingPickedUp)
		{
			if( frontWheel.GetMaterialId() == CWheel::m_MaterialIdStuntTrack ||
				rearWheel.GetMaterialId() == CWheel::m_MaterialIdStuntTrack )
			{
				m_fOffStuntTime = ms_TimeBeforeLeavingStuntMode;
			}

			if(isFrontWheelTouchingGround && isRearWheelTouchingGround)
			{
				Vec3V frontGroundNormal = RCC_VEC3V(frontWheel.GetHitNormal());
				Vec3V rearGroundNormal = RCC_VEC3V(rearWheel.GetHitNormal());

				// If either ground normal more than 25 degrees off of the last lean-less up vector, use whichever is less wrong. 
				// The idea of this code is to prevent stray normals from giving us a weird ground normal. This could also be done by 
				//  keeping track of the previous ground normal and slowly interpolating it. 
				const ScalarV badGroundNormalTolerance = ScalarVFromF32(0.9f);
				ScalarV frontGroundNormalDotOldUp = Dot(frontGroundNormal,RC_VEC3V(m_vecBikeWheelUp));
				ScalarV rearGroundNormalDotOldUp = Dot(rearGroundNormal,RC_VEC3V(m_vecBikeWheelUp));
				if(Or(IsLessThan(frontGroundNormalDotOldUp,badGroundNormalTolerance),IsLessThan(rearGroundNormalDotOldUp,badGroundNormalTolerance)).Getb())
				{
					groundNormal = SelectFT(IsLessThan(frontGroundNormalDotOldUp,rearGroundNormalDotOldUp),frontGroundNormal,rearGroundNormal);
				}
				else
				{
					groundNormal = rage::Add(frontGroundNormal,rearGroundNormal);
				}
			}
			else if(isFrontWheelTouchingGround)
			{
				groundNormal = RCC_VEC3V(frontWheel.GetHitNormal());
			}
			else
			{
				groundNormal = RCC_VEC3V(rearWheel.GetHitNormal());
			}
			groundNormal = NormalizeSafe(groundNormal,vZAxis);

			// We don't want to have any lean on ground that isn't steep. Map any ground normal under a given angle with the z-axis (ms_fSecondNewLeanZAxisCosine) to the z-axis,
			//   don't touch any ground normal above a certain angle with the z-axis (ms_fSecondNewLeanGroundNormalCosine) and interpolate any ground normal with an angle
			//   that's in between. 
			const ScalarV minCosineToUseZAxis = ScalarVFromF32(ms_fSecondNewLeanZAxisCosine);
			const ScalarV maxCosineToUseGroundNormal = ScalarVFromF32(ms_fSecondNewLeanGroundNormalCosine);
			ScalarV groundNormalZaxisCosine = groundNormal.GetZ();
			const ScalarV groundNormalInterpolation = Clamp(Range(groundNormalZaxisCosine,maxCosineToUseGroundNormal,minCosineToUseZAxis),ScalarV(V_ZERO),ScalarV(V_ONE));
			groundNormal = NormalizeSafe(Lerp(groundNormalInterpolation,groundNormal,vZAxis),vZAxis);
		}
		else
		{
			groundNormal = vZAxis;
		}
		
		bool beingDrivenUpsideDown = (IsLessThanAll(currentMatrix.GetCol2().GetZ(),ScalarV(V_ZERO)) && GetDriver()) && m_fOffStuntTime == 0.0f;

		// Don't do anything if the bike's forward and ground normal are too close
		const ScalarV forwardAndGroundNormalCosineTolerance = ScalarVFromF32(0.939f); // ~20 degrees
		if(!beingDrivenUpsideDown && IsLessThanAll(Abs(Dot(currentForward,groundNormal)),forwardAndGroundNormalCosineTolerance))
		{
			// Store the wheel up before applying lean
			if( m_fOffStuntTime == 0.0f )
			{
				RC_VEC3V(m_vecBikeWheelUp) = NormalizeSafe(Cross(Cross(currentForward,vZAxis),currentForward),vZAxis);
			}
			else
			{
				if( m_fOffStuntTime > ms_TimeBeforeLeavingStuntMode * 0.5f )
				{
					RC_VEC3V(m_vecBikeWheelUp) = groundNormal;
				}
				else
				{
					Vec3V offStuntNormal = NormalizeSafe(Cross(Cross(currentForward,vZAxis),currentForward),vZAxis);
					Vec3V onStuntNormal  = groundNormal;

					ScalarV stuntFactor = ScalarV( m_fOffStuntTime / ( ms_TimeBeforeLeavingStuntMode * 0.5f ) );
					RC_VEC3V(m_vecBikeWheelUp) = ( onStuntNormal * stuntFactor ) + ( offStuntNormal * ( ScalarV( 1.0f ) - stuntFactor ) ); 
				}
			}

			// Compute the ideal right and up vector for 0 degrees of lean
			Vec3V idealRight = Normalize(Cross(currentForward,groundNormal));
			Vec3V idealUp = Cross(idealRight,currentForward);

			// Rotate the ideal matrix around our forward by the desired lean angle
			Mat34V idealMatrix = Mat34V(idealRight,currentForward,idealUp,Vec3V(V_ZERO));
			const ScalarV desiredLeanAngle = ScalarVFromF32(fDesiredLeanAngle);
			Assert(RCC_MATRIX34(idealMatrix).IsOrthonormal());
			Mat34VRotateLocalY(idealMatrix,desiredLeanAngle);

			// Determine how much angular velocity it would take to correct the bike this frame
			QuatV currentQuat = QuatVFromMat34V(currentMatrix);
			QuatV idealQuat = QuatVFromMat34V(idealMatrix);
			Vec3V requiredAngularVelocity = CalculateAngVelocityFromQuaternions(currentQuat,idealQuat,ScalarVFromF32(1.0f/fTimeStep));
			ScalarV requiredAngularVelocityMag = Mag(requiredAngularVelocity);

			// Figure out how fast we can rotate the bike
			ScalarV maxAngularVelocitySlow;
			ScalarV maxAngularVelocityFast;
			if(m_nBikeFlags.bGettingPickedUp)
			{
				// NOTE: This is a bit hacky. At the moment it looks very bad when you pick up bikes with luggage around the rear wheel. 
				//         The bike sinks into the ground very fast because the ground clearance clamps the vertex z. Enabling collision
				//         makes it look even worse. Clamping the velocity makes it a bit smoother. 
				if(GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIKE_CLAMP_PICKUP_LEAN_RATE))
				{
					maxAngularVelocityFast = maxAngularVelocitySlow = ScalarVFromF32(ms_fSecondNewLeanMaxAngularVelocityPickup);
				}
				else
				{
					maxAngularVelocityFast = maxAngularVelocitySlow = ScalarV(V_FLT_LARGE_4);
				}
			}
			else if(isEitherWheelTouchingGround || GetFrameCollisionHistory()->GetNumCollidedEntities() > 0)
			{
				maxAngularVelocitySlow = ScalarVFromF32(ms_fSecondNewLeanMaxAngularVelocityGroundSlow);
				maxAngularVelocityFast = ScalarVFromF32(ms_fSecondNewLeanMaxAngularVelocityGroundFast);
			}
			else
			{
				maxAngularVelocitySlow = ScalarVFromF32(ms_fSecondNewLeanMaxAngularVelocityAirSlow);
				maxAngularVelocityFast = ScalarVFromF32(ms_fSecondNewLeanMaxAngularVelocityAirFast);
			}

			// The faster the bike is going the faster it can stabilize
			const ScalarV speed = Mag(VECTOR3_TO_VEC3V(GetVelocity()));
			const ScalarV speedToUseSlowAngVelocity = ScalarV(V_ZERO);
			const ScalarV speedToUseFastAngVelocity = ScalarV(20.0f);
			ScalarV maxAngularVelocityMag = Ramp(	Clamp(speed,speedToUseSlowAngVelocity,speedToUseFastAngVelocity),
													speedToUseSlowAngVelocity,speedToUseFastAngVelocity,
													maxAngularVelocitySlow,maxAngularVelocityFast);

			// Check if rotating to the ideal matrix exceeds our max angular velocity
			// Getting picked up a is a special case since it relies on stabilization code but the bike isn't moving
			Mat33V finalRotationMatrix;
			if(IsGreaterThanAll(requiredAngularVelocityMag,maxAngularVelocityMag))
			{
				// Using the ideal matrix exceeds our maximum angular velocity, so just interpolate by as much as we can
				ScalarV interpolationValue = InvScale(maxAngularVelocityMag,requiredAngularVelocityMag);
				Mat33VFromQuatV(finalRotationMatrix, SlerpSafe(interpolationValue, currentQuat, idealQuat, idealQuat));
			}
			else
			{
				// Our change in rotation isn't big enough to matter so just use the ideal
				finalRotationMatrix = idealMatrix.GetMat33ConstRef();
			}

			// Since we want to rotate about the bottom of the bike instead of the center (so the wheels don't move)
			//   we need to adjust the position of the bike. 
			const Vec3V localRotationCenter = Vec3V(0.0f,0.0f,-GetHeightAboveRoad());
			const Vec3V worldRotationOffset = Subtract(Multiply(finalRotationMatrix,localRotationCenter),Transform3x3(currentMatrix,localRotationCenter));
			Mat34V finalMatrix = Mat34V(finalRotationMatrix,rage::Subtract(currentMatrix.GetCol3(), worldRotationOffset));
			Assertf(RCC_MATRIX34(finalMatrix).IsOrthonormal(),	"Invalid bike matrix."
																"\n\tForward: %f, %f, %f"
																"\n\tGround Normal: %f, %f, %f", VEC3V_ARGS(currentForward), VEC3V_ARGS(groundNormal));
			RC_MAT34V(stabiliseMatrix) = finalMatrix;
		}
	}
	else
	{
		// update and maintain m_vecFwdDirnStored
		UpdateStoredFwdVector(stabiliseMatrix, fTimeStep);

		//if(!bSkipStabilisationMatrix)
		{
			stabiliseMatrix.a.Cross(m_vecFwdDirnStored, ZAXIS);
			stabiliseMatrix.a.Normalize();
			stabiliseMatrix.c.Cross(stabiliseMatrix.a, stabiliseMatrix.b);
			stabiliseMatrix.c.Normalize();
			stabiliseMatrix.a.Cross(stabiliseMatrix.b, stabiliseMatrix.c);
			stabiliseMatrix.a.Normalize();

			//Keep a record of the non rotated up vector
			m_vecBikeWheelUp = stabiliseMatrix.c;

			stabiliseMatrix.RotateLocalY(fDesiredLeanAngle);

		}

		stabiliseMatrix.d = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

		Vector3 vecGroundOffset(0.0f,0.0f, -GetHeightAboveRoad());
		Vector3 vecOldOffset = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vecGroundOffset)));
		Vector3 vecNewOffset;
		stabiliseMatrix.Transform3x3(vecGroundOffset, vecNewOffset);

		vecGroundOffset = vecNewOffset - vecOldOffset;
		stabiliseMatrix.d -= vecGroundOffset;
	}



    pPhysInst->SetMatrix(RC_MAT34V(stabiliseMatrix));
    CPhysics::GetLevel()->UpdateObjectLocation(pPhysInst->GetLevelIndex());	

    phCollider *pCollider = GetCollider(); 
    if(pCollider)
        pCollider->SetColliderMatrixFromInstance();
}

dev_float sfSlowBikeMinGroundClearance = 0.25f;
dev_float sfBikeGroundClearanceSpeedAdjust = 0.35f; 


static dev_float sfGroundClearanceVelocityShrinkStartSq = 3.5f * 3.5f;
static dev_float sfGroundClearanceVelocityShrinkRangeSq = 14.5f * 14.5f;

void CBike::EnforceMinimumGroundClearance()
{
    float fMinGroundClearance = sfSlowBikeMinGroundClearance;

    if(GetDriver() && GetDriver()->IsPlayer()) // Only modify the bounds based on speed for the players vehicle.
    {
        //scale the amount of ground clearance we need based on our speed.
        float fVelocityMagnitude = GetVelocity().Mag2();
        if(fVelocityMagnitude > sfGroundClearanceVelocityShrinkStartSq)
        {
            float fVelocityAboveMinimum = fVelocityMagnitude - sfGroundClearanceVelocityShrinkStartSq;
            float fVelocitySpeedAdjustScale = fVelocityAboveMinimum / sfGroundClearanceVelocityShrinkRangeSq;

            if(fVelocitySpeedAdjustScale > 1.0f)
            {
                fVelocitySpeedAdjustScale = 1.0f;
            }

            fMinGroundClearance += sfBikeGroundClearanceSpeedAdjust * fVelocitySpeedAdjustScale;
        }
    }
	
	// If we're not leaning much just use a 
	static dev_float minimumLeanForLeanedClearance = DtoR*15.0f;
	float leanAngleForClearance = Abs(m_fBikeLeanAngle) > minimumLeanForLeanedClearance ? m_fBikeLeanAngle : 0.0f;
	leanAngleForClearance = Clamp( leanAngleForClearance, -PI, PI );
	EnforceMinimumGroundClearanceInternal(leanAngleForClearance, fMinGroundClearance);
}


void CBike::EnforceMinimumGroundClearanceInternal(float leanAngle, float clearance)
{
	// Don't recompute anything if we're using nearly the same height and angle
	if(IsClose(leanAngle,m_fCurrentGroundClearanceAngle,0.01f) && IsClose(clearance,m_fCurrentGroundClearanceHeight,0.01f))
	{
		return;
	}
	m_fCurrentGroundClearanceHeight = clearance;
	m_fCurrentGroundClearanceAngle = leanAngle;

	// Grab the write lock to prevent shapetests on other threads from reading these geometry bounds during modification. Grab before cloning
	//   since we'll have to grab the lock there anyways. 
	PHLOCK_SCOPEDWRITELOCK;

	//make sure we have cloned bounds before we start modifying them.
	CloneBounds();

	ScalarV cos,sin;
	SinAndCosFast(sin,cos,Negate(ScalarVFromF32(leanAngle)));
	const Vec3V localGroundNormal = Vec3V(sin,ScalarV(V_ZERO),cos);

	const ScalarV bikeCenterAboveLocalRoad = Scale(cos,ScalarVFromF32(GetHeightAboveRoad()));
	const ScalarV minimumHeightAboveLocalRoad = Negate(Max(Subtract(bikeCenterAboveLocalRoad,ScalarVFromF32(clearance)),ScalarV(V_ZERO)));

	Assert(GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE);
	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetCurrentPhysicsInst()->GetArchetype()->GetBound());
	const phBoundComposite* pBoundCompOrig = static_cast<const phBoundComposite*>(GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds());

	if(pBoundComp == NULL || pBoundCompOrig == NULL)
		return;

	Assert(pBoundComp->GetNumBounds() == pBoundCompOrig->GetNumBounds());

#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
	const int needsUpdateListSize = pBoundComp->GetNumBounds();
	phBoundGeometry ** needsUpdateList = Alloca(phBoundGeometry*,needsUpdateListSize);
	int needsUpdateListCount = 0;
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION

	for(int nBound=0; nBound < pBoundComp->GetNumBounds(); nBound++)
	{
		phBound* pBound = pBoundComp->GetBound(nBound);
		const phBound* pBoundOrig = pBoundCompOrig->GetBound(nBound);
		if(	pBound && pBound->GetType()==phBound::GEOMETRY && 
			pBoundOrig && pBoundOrig->GetType()==phBound::GEOMETRY)
		{
			phBoundGeometry* pBoundGeom = static_cast<phBoundGeometry*>(pBound);
			const phBoundGeometry* pBoundGeomOrig = static_cast<const phBoundGeometry*>(pBoundOrig);
			// want to deform the shrunk geometry verticies, ignore the preshrunk ones since we want shape tests to hit the original bike
			if(pBoundGeom->GetShrunkVertexPointer() && pBoundGeomOrig->GetShrunkVertexPointer())
			{
				bool shrunkVertsChanged = false;
				Mat34V_ConstRef componentMatrix = pBoundComp->GetCurrentMatrix(nBound);
				for(int nVert=0; nVert < pBoundGeom->GetNumVertices(); nVert++)
				{
					// Get the original vertex in the bike's space
					const Vec3V originalLocalVertex = pBoundGeomOrig->GetShrunkVertex(nVert);
					const Vec3V originalBikeVertex = Transform(componentMatrix,originalLocalVertex);

					// Figure out how far to shift it to reach the minimum
					const ScalarV originalHeightAboveLocalRoad = Dot(originalBikeVertex,localGroundNormal);
					const ScalarV shiftAlongNormal = Max(Subtract(minimumHeightAboveLocalRoad,originalHeightAboveLocalRoad),ScalarV(V_ZERO));

					// Transform the clamped vertex back into its local space
					const Vec3V clampedBikeVertex = AddScaled(originalBikeVertex,localGroundNormal,shiftAlongNormal);
					const Vec3V clampedLocalVertex = UnTransformOrtho(componentMatrix,clampedBikeVertex);
					shrunkVertsChanged |= pBoundGeom->SetShrunkVertex(nVert, clampedLocalVertex);
				}

#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
				if(shrunkVertsChanged)
				{
					Assert(needsUpdateListCount < needsUpdateListSize);
					needsUpdateList[needsUpdateListCount] = pBoundGeom;
					needsUpdateListCount++;
				}
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
			}
		}
	}

#if REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
	if (needsUpdateListCount > 0)
	{
		//sysTimer timer;
		fragMemStartCacheHeapFunc(GetVehicleFragInst()->GetCacheEntry());
		{
			for (int i = 0 ; i < needsUpdateListCount ; i++)
				needsUpdateList[i]->RecomputeOctantMap();
		}
		fragMemEndCacheHeap();
		//float time = timer.GetUsTime();
		//Displayf("BIKE Took %f to regenerate octant maps for %d bounds.",time,needsUpdateListCount);
	}
#endif // REGENERATE_OCTANT_MAP_AFTER_DEFORMATION
}


dev_float sfBikeFwdDirnDecayRate = 2.0f;
dev_float sfBikeUseFwdFwdZLimit = 0.9659f;	// cos(15deg)
dev_float sfBikeUseFwdUpZLimit	= 0.2588f;	// sin(15deg)


void CBike::UpdateStoredFwdVector(const Matrix34& mat, float fTimeStep)
{
	// we want to use an alternative, smoothed fwd matrix to calculated stabilise matrix, to deal with going vertical and flipping over
	if(rage::Abs(mat.c.z) > sfBikeUseFwdUpZLimit && 
	   rage::Abs(mat.b.z) < sfBikeUseFwdFwdZLimit)
	{
		// smooth transition back to real fwd vector
		if(fTimeStep >= 0.0f) 
		{
			m_fFwdDirnMult -= sfBikeFwdDirnDecayRate*fTimeStep;
			m_fFwdDirnMult = rage::Clamp(m_fFwdDirnMult, 0.0f, 1.0f);
		}
		else
			m_fFwdDirnMult = 0.0f;

		// need to check if the rider can be knocked off
		bool bCantBeKnockedOff = IsArchetypeSet() && (GetDriver() && GetDriver()->m_PedConfigFlags.GetCantBeKnockedOffVehicle()==KNOCKOFFVEHICLE_NEVER);

		Vector3 vecFwdDirn(VEC3V_TO_VECTOR3(GetTransform().GetB()));
		// flip desired fwd vector if upside down (don't do this if the rider can't be knocked off, so bike will flip the right way up instead)
		static dev_bool bDisableFlip = false;
		if(mat.c.z < 0.0f && !bCantBeKnockedOff && !m_nBikeFlags.bOnSideStand && !m_nBikeFlags.bGettingPickedUp && !bDisableFlip)
		{
			vecFwdDirn.Scale(-1.0f);
		}

		m_vecFwdDirnStored.Scale(m_fFwdDirnMult);
		m_vecFwdDirnStored.AddScaled(vecFwdDirn, 1.0f - m_fFwdDirnMult);
	}
	else
	{
		m_fFwdDirnMult = 1.0f;
	}
}

dev_float sfBikeLeanMinSpeed = 1.0f;
dev_float sfBikeLeanBlendSpeed = 10.0f;
dev_float sfBikeLeanForceSpeedMult = 0.2f;
dev_float sfBikeStoppieStabiliserMult = -5.0f;
dev_float sfBikeWheelieForceCgMult = -0.5f;
dev_float sfBikeBurnoutRotForce = -2.5f;
dev_float sfBikeBurnoutRotLimit = 1.0f;
dev_bool sbBikeWheelieSteerAboutZ = true;
dev_bool sbBikeWheelieMinusActive = false;
dev_float sfBikeWheeliePlusTrigger = 0.05f;
dev_float sfBikeWheeliePlusCap = 2.0f;
dev_float sfBikeWheelieMinusTrigger = 0.05f;
dev_float sfBikeWheelieMinusCap = 0.3f;
dev_float sfBikeWheelieMaxStableAngle = 0.85f;
dev_float sfBikeWheelieStabPow = 6.0f;
//
void CBike::ProcessDriverInputsForStability(float fTimeStep, float& fLeanCgFwd, Vector3 &vecBikeVelocity)
{
	bool bMoving = false;
	float fSpeedBlend = 0.0f;
	float fFwdSpeed = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetB()), vecBikeVelocity);

	bool bBothWheelsOffTheGround = (!GetWheel(0)->GetIsTouching() && !GetWheel(1)->GetIsTouching());
	bool bIsBicycle = GetVehicleType() == VEHICLE_TYPE_BICYCLE;

	m_fBurnoutTorque = 0.0f;//reset burnout torque

	if(fFwdSpeed > sfBikeLeanMinSpeed)
	{
		bMoving = true;
		if(fFwdSpeed > sfBikeLeanBlendSpeed) 
			fSpeedBlend = 1.0f;
		else
			fSpeedBlend = fFwdSpeed / sfBikeLeanBlendSpeed;
	}

	// Scale the wheelie ability by the players stats.
	float fWheelieAbilityMult = 1.0f;
	float fWheelieSteeringWobble = 0.0f;
	float fInAirAbilityMult = 1.0f;
	if(GetDriver() && GetDriver()->IsLocalPlayer())
	{
		StatId stat = STAT_WHEELIE_ABILITY.GetStatId();
		float fWheelieStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
		float fMinWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MinWheelieAbility;
		float fMaxWheelieAbility = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MaxWheelieAbility;

		fInAirAbilityMult = fWheelieAbilityMult = ((1.0f - fWheelieStatValue) * fMinWheelieAbility + fWheelieStatValue * fMaxWheelieAbility)/100.0f;

		if(bIsBicycle)
		{
			fWheelieSteeringWobble = 1.0f - (fWheelieAbilityMult / (fMaxWheelieAbility/100.0f));

			if( ms_bDisableTrickForces )
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, REDUCE_BICYCLE_WHEELIE_ABILITY_DELTA_MULT, 1.0f, 0.0f, 1.0f, 0.01f);
				fWheelieAbilityMult = (fMaxWheelieAbility/100.0f) - (((fMaxWheelieAbility/100.0f) - fWheelieAbilityMult) * REDUCE_BICYCLE_WHEELIE_ABILITY_DELTA_MULT);
			}
		}

		if( !bIsBicycle ||
			!ms_bDisableTrickForces )
		{
			//We don't want the lean back and forward forces set by player stats to be as different.
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, REDUCE_WHEELIE_ABILITY_DELTA_MULT, 0.5f, 0.0f, 1.0f, 0.01f);
			fWheelieAbilityMult = (fMaxWheelieAbility/100.0f) - (((fMaxWheelieAbility/100.0f) - fWheelieAbilityMult) * REDUCE_WHEELIE_ABILITY_DELTA_MULT);
		}

		fWheelieAbilityMult = rage::Clamp(fWheelieAbilityMult, 0.1f, 2.0f);

		if(bBothWheelsOffTheGround)
		{
			static dev_float sfInAirLeaningMult = 0.4f;
			fWheelieAbilityMult *= sfInAirLeaningMult;
			fWheelieAbilityMult *= 1.0f - GetSpecialFlightModeRatio();
		}
	}

	// apply torque from rider leaning fwd/back
	if(bMoving || bBothWheelsOffTheGround)
	{
        static bool sOldLean = true;
        if(sOldLean)
        {
		    if(m_fAnimLeanFwd < 0.0f)
			{	
				if( GetBrake() > 0.0f || bBothWheelsOffTheGround) // Don't let people lean forward unless they are braking or their wheels are off the ground.
				{
					float fLeanForce = m_fAnimLeanFwd;
					float fRotSpeed = DotProduct(GetAngVelocity(), VEC3V_TO_VECTOR3(GetTransform().GetA())) * sfBikeLeanForceSpeedMult;
					if(fRotSpeed < 0.0f)
					{
						fLeanForce = rage::Min(0.0f, fLeanForce - fRotSpeed);
					}

					float fSpeedBlendMult = fSpeedBlend;
					if(bBothWheelsOffTheGround)
					{
						fSpeedBlendMult = 1.0f;
						fSpeedBlendMult *= 1.0f - GetSpecialFlightModeRatio();
					}

					fLeanForce *= GetAngInertia().x * pHandling->GetBikeHandlingData()->m_fLeanFwdForceMult * fSpeedBlendMult * fWheelieAbilityMult;
					ApplyInternalTorque(fLeanForce * VEC3V_TO_VECTOR3(GetTransform().GetA()));

					if((GetWheel(1)->GetIsTouching() || bBothWheelsOffTheGround) && ( !ms_bDisableTrickForces || !bIsBicycle ) )
					{
						if( !ms_bDisableTrickForces ||
							GetWheel(1)->GetTyreHealth() > TYRE_HEALTH_FLAT )
						{
							ApplyInternalForceCg(fLeanForce*sfBikeWheelieForceCgMult*ZAXIS*ms_fBikeGravityMult);
						}
					}
				}
		    }
		    else
			{
				bool bCanLeanBackwards = GetWheeliesEnabled();
				if(GetDriver() && bIsBicycle)
				{
					bCanLeanBackwards = GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_CanDoBicycleWheelie);
				}

				if(bCanLeanBackwards || bBothWheelsOffTheGround)// only lean back when moving forwards or the wheels are off the ground.
				{
					float fLeanForce = m_fAnimLeanFwd;
					float fRotSpeed = DotProduct(GetAngVelocity(), VEC3V_TO_VECTOR3(GetTransform().GetA())) * sfBikeLeanForceSpeedMult;
					if(fRotSpeed > 0.0f)
						fLeanForce = rage::Max(0.0f, fLeanForce - fRotSpeed);

					// Reduce the lean force if the rear wheel is significantly underwater. Buoyancy and resistance forces make doing wheelies really easy
					//   non-shallow water. 
					if(m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate && m_nFlags.bPossiblyTouchesWater)
					{
						float fWaterZ = 0.0f;
						Vec3V rearWheelPosition = Transform(GetMatrixRef(),VECTOR3_TO_VEC3V(CWheel::GetWheelOffset(GetVehicleModelInfo(),BIKE_WHEEL_R)));
						if(m_Buoyancy.GetWaterLevelIncludingRivers(VEC3V_TO_VECTOR3(rearWheelPosition), &fWaterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
						{
							static dev_float sfWaterLevelAboveWheelCenterSubmerged = 0.1f;
							static dev_float sfWheelSubmergedLeaningMult = 0.1f;
							if(rearWheelPosition.GetZf() + sfWaterLevelAboveWheelCenterSubmerged < fWaterZ)
							{
								fLeanForce *= sfWheelSubmergedLeaningMult;
							}
						}
					}

					float fForceMult = 1.0f;

					if(!bBothWheelsOffTheGround && !( pHandling->hFlags & HF_LOW_SPEED_WHEELIES ) )// Scale the lean by the throttle
					{
						fForceMult = rage::Abs(GetThrottle());
					}
					else if(bIsBicycle)
					{
						if(m_nBikeFlags.bLateralJump)
						{
							TUNE_GROUP_FLOAT(BICYCLE_TUNE, IN_AIR_LATERAL_JUMP_LEAN_BACK_FORCE_MULT, 0.1f, 0.0f, 1.0f, 0.01f);
							fForceMult *= IN_AIR_LATERAL_JUMP_LEAN_BACK_FORCE_MULT;
						}
						else
						{
							TUNE_GROUP_FLOAT(BICYCLE_TUNE, IN_AIR_LEAN_BACK_FORCE_MULT, 0.5f, 0.0f, 1.0f, 0.01f);
							fForceMult *= IN_AIR_LEAN_BACK_FORCE_MULT;
						}
					}

					fLeanForce *= GetAngInertia().x*pHandling->GetBikeHandlingData()->m_fLeanBakForceMult * fWheelieAbilityMult * fForceMult;

					// For low speed wheelies, add extra control checks for severe angles
					if( ( pHandling->hFlags & HF_LOW_SPEED_WHEELIES ) && GetWheel(0)->GetIsTouching() )
					{
						Vector3 vBikeForwardInGroundLocal = VEC3V_TO_VECTOR3(GetTransform().GetB());
						Vector3 vGroundNormalAtRearWheel = GetWheel(0)->GetHitNormal();
						float fBikeForwardDotGroundNormal = vBikeForwardInGroundLocal.Dot(vGroundNormalAtRearWheel);
						float fWheelieAngle = rage::Asinf(fBikeForwardDotGroundNormal);

						static dev_float sfStabiliseAngleMult = -0.025f;
						static dev_float sfThrottleStabiliseAngle = 50.0f;
						static dev_float sfFallOffAngle = 65.0f;

						// If the wheelie is at an angle where the player is about to fall over, don't apply any lean
						if( fWheelieAngle > DtoR * sfFallOffAngle && 0.0f == GetThrottle() )
						{
							fLeanForce = 0.0f;
						}
						// At high angles and low speeds, pushing the throttle would lean the bike back too much and the player would fall over
						// So instead, apply a very slight lean force forward
						else if(fWheelieAngle > DtoR * sfThrottleStabiliseAngle && GetThrottle() != 0.0f )
						{
							fLeanForce *= sfStabiliseAngleMult;
						}
					}

					fLeanForce *= 1.0f - GetSpecialFlightModeRatio();

					ApplyInternalTorque( fLeanForce* VEC3V_TO_VECTOR3(GetTransform().GetA())*ms_fBikeGravityMult);

					if(GetWheel(0)->GetIsTouching() || bBothWheelsOffTheGround)
						ApplyInternalForceCg(-fLeanForce*sfBikeWheelieForceCgMult*ZAXIS*ms_fBikeGravityMult);
				}
		    }
        }
        else if(GetDriver() && GetDriver()->IsLocalPlayer())
        {

            float fPitchInputClamped = rage::Clamp(m_fLeanFwdInput, -0.4f, 0.4f);		
			float fLeanInputClamped = rage::Clamp(-m_fLeanSideInput, -0.4f, 0.4f);		

			//leaning fwd and back
            float fPitchTorque = fPitchInputClamped;
            fPitchTorque /= fTimeStep;

            static float pitchTorqueMult = 0.5f; 

            static float spitchVelMult = 5.0f;
            fPitchTorque = fPitchTorque * (rage::Min(GetVelocity().Mag()/spitchVelMult, 1.0f));

            fPitchTorque *= pitchTorqueMult;
            fPitchTorque *= GetAngInertia().y;

            ApplyTorque(VEC3V_TO_VECTOR3(GetTransform().GetA()) * fPitchTorque);

			//leaning left right
            float fLeanTorque = fLeanInputClamped;
            fLeanTorque /= fTimeStep;

            static float leanTorqueMult = 0.1f;

            static float sleanVelMult = 5.0f;
            fLeanTorque = fLeanTorque * (rage::Min(GetVelocity().Mag()/sleanVelMult, 1.0f));

            fLeanTorque *= leanTorqueMult;
            fLeanTorque *= GetAngInertia().x;

            ApplyTorque(VEC3V_TO_VECTOR3(GetTransform().GetB()) * fLeanTorque);
        }
	}

	Assert(GetStatus()==STATUS_PLAYER);

	if(!GetDriver() || !GetDriver()->IsLocalPlayer())
	{
		return;
	}

	float fLeanSide = 0.0f;
	float fLeanSideClamped = 0.0f;
	if(GetDriver() && GetDriver()->IsLocalPlayer() &&
		( HasGlider() || GetSpecialFlightModeRatio() < 0.1f ) )
	{
		fLeanCgFwd = m_fLeanFwdInput * -1.0f;
		fLeanSide = -m_fLeanSideInput;
#if RSG_PC
		fLeanCgFwd = Clamp(fLeanCgFwd, -1.0f, 1.0f);
		fLeanSide = Clamp(fLeanSide, -1.0f, 1.0f);
#endif
		fLeanSideClamped = rage::Clamp(fLeanSide, -0.4f, 0.4f);		
		fLeanSideClamped *= 1.0f - GetSpecialFlightModeRatio();
	}

	CBikeHandlingData* pBikeHandling = pHandling->GetBikeHandlingData();

	// check we've got the wheels the right way round
	Assert(GetWheel(0)->GetHierarchyId()==BIKE_WHEEL_R);
	Assert(GetWheel(1)->GetHierarchyId()==BIKE_WHEEL_F);

	TUNE_GROUP_FLOAT( VEHICLE_GLIDER, MAX_FORCE_REDUCTION_WHEN_GLIDING, 1.0f, 0.0f, 1.0f, 0.1f );

	// back wheel on ground, front wheel not = Wheelie
	if(GetWheel(0)->GetIsTouching() && !GetWheel(1)->GetIsTouching() && bMoving)
	{
		float fWheelieSteering = fLeanSideClamped;

		if(bIsBicycle)
		{
			TUNE_GROUP_FLOAT(BICYCLE_TUNE, WHEELIE_STEERING_WOBBLE_MULT, 40.0f, 0.0f, 200.0f, 0.1f);
			float fSteeringWobble = fwRandom::GetRandomNumberInRange(-fWheelieSteeringWobble, fWheelieSteeringWobble) * WHEELIE_STEERING_WOBBLE_MULT;
			fWheelieSteering = fLeanSideClamped + fSteeringWobble;
		}

		if(sbBikeWheelieSteerAboutZ)
			ApplyInternalTorque(fWheelieSteering*pBikeHandling->m_fWheelieSteerMult*fWheelieAbilityMult*GetAngInertia().z*ZAXIS);
		else
			ApplyInternalTorque(fWheelieSteering*pBikeHandling->m_fWheelieSteerMult*fWheelieAbilityMult*GetAngInertia().z* VEC3V_TO_VECTOR3(GetTransform().GetC()));



		Vector3 vBikeForwardInGroundLocal = VEC3V_TO_VECTOR3(GetTransform().GetB());
		Vector3 vGroundNormalAtRearWheel = GetWheel(0)->GetHitNormal();
		float fBikeForwardDotGroundNormal = vBikeForwardInGroundLocal.Dot(vGroundNormalAtRearWheel);
		float fWheelieAngle = rage::Asinf(fBikeForwardDotGroundNormal) - pBikeHandling->m_fWheelieBalancePoint;

		float fWheelieStabiliseForce = 0.0f;
		if(fWheelieAngle > sfBikeWheeliePlusTrigger)
		{
			fWheelieStabiliseForce = ((fWheelieAngle - sfBikeWheeliePlusTrigger) / (sfBikeWheeliePlusCap - sfBikeWheeliePlusTrigger) );
			fWheelieStabiliseForce = Clamp(fWheelieStabiliseForce, 0.0f, 1.0f);
			fWheelieStabiliseForce *= -1.0f;
		}
		else if(sbBikeWheelieMinusActive && fWheelieAngle < -sfBikeWheelieMinusTrigger)
		{
			fWheelieStabiliseForce = ((-fWheelieAngle - sfBikeWheelieMinusTrigger) / (sfBikeWheelieMinusCap - sfBikeWheelieMinusTrigger) );
			fWheelieStabiliseForce = Clamp(fWheelieStabiliseForce, 0.0f, 1.0f);
		}

		// blend stabilization force out smoothly at low speeds
		if( bIsBicycle && ms_bDisableTrickForces )
		{
			fWheelieStabiliseForce *= pBikeHandling->m_fRearBalanceMult * Powf(fWheelieAbilityMult, sfBikeWheelieStabPow) * GetAngInertia().x * fSpeedBlend;
		}
		else
		{
			fWheelieStabiliseForce *= pBikeHandling->m_fRearBalanceMult * GetAngInertia().x * fSpeedBlend;
		}
		fWheelieStabiliseForce *= 1.0f - ( GetSpecialFlightModeRatio() * MAX_FORCE_REDUCTION_WHEN_GLIDING );

		ApplyInternalTorque(fWheelieStabiliseForce* VEC3V_TO_VECTOR3(GetTransform().GetA())*ms_fBikeGravityMult);

	}
	// front wheel on ground, back wheel not = Stoppie
	else if(GetWheel(1)->GetIsTouching() && !GetWheel(0)->GetIsTouching() && bMoving)
	{
		static dev_float sfSteeringMult = 0.1f;
		ApplyInternalTorque(sfSteeringMult*fLeanSideClamped*pBikeHandling->m_fWheelieSteerMult*GetAngInertia().z*ZAXIS);

		float fSideSpeed = DotProduct(vecBikeVelocity, VEC3V_TO_VECTOR3(GetTransform().GetA()));
		if(rage::Abs(fSideSpeed) > 0.001f && GetBrake() > 0.0f)
		{
			if(fSideSpeed > 0.0f)
				fSideSpeed = rage::Sqrtf(fSideSpeed);
			else
				fSideSpeed = -1.0f*rage::Sqrtf(rage::Abs(fSideSpeed));

			ApplyInternalTorque(fSideSpeed*sfBikeStoppieStabiliserMult*GetAngInertia().z*ZAXIS);
		}

		float fWheelieAngle = rage::Asinf(GetTransform().GetB().GetZf()) - pBikeHandling->m_fStoppieBalancePoint;
		float fWheelieStabiliseForce = 0.0f;
		if(fWheelieAngle < -sfBikeWheeliePlusTrigger)
		{
			fWheelieStabiliseForce = ((-fWheelieAngle - sfBikeWheeliePlusTrigger) / (sfBikeWheeliePlusCap - sfBikeWheeliePlusTrigger) );
			fWheelieStabiliseForce = Clamp(fWheelieStabiliseForce, 0.0f, 1.0f);
		}
		else if(sbBikeWheelieMinusActive && fWheelieAngle  > sfBikeWheelieMinusTrigger)
		{
			fWheelieStabiliseForce = ((fWheelieAngle - sfBikeWheelieMinusTrigger) / (sfBikeWheelieMinusCap - sfBikeWheelieMinusTrigger) );
			fWheelieStabiliseForce = Clamp(fWheelieStabiliseForce, 0.0f, 1.0f);
			fWheelieStabiliseForce *= -1.0f;
		}

		// blend stabilization force out smoothly at low speeds
		fWheelieStabiliseForce *= pBikeHandling->m_fFrontBalanceMult * GetAngInertia().x * fSpeedBlend;
		ApplyInternalTorque(fWheelieStabiliseForce* VEC3V_TO_VECTOR3(GetTransform().GetA())*ms_fBikeGravityMult);

	}
	// both wheels off the ground
	else if(!GetWheel(0)->GetIsTouching() && !GetWheel(1)->GetIsTouching())
	{
		//This doesn't feel like its necessary now the bikes are more stable
		static dev_float sfAutLoevelControlDeadZone = 0.01f;
		if(GetVehicleType() == VEHICLE_TYPE_BIKE && rage::Abs(fLeanSide) < sfAutLoevelControlDeadZone && rage::Abs(fLeanCgFwd) < sfAutLoevelControlDeadZone)// don't auto level if the stick is being fiddled with.
		{
			AutoLevelAndHeading( pBikeHandling->m_fInAirSteerMult * ( 1.0f - GetSpecialFlightModeRatio() ) );
		}

		if(!m_nBikeFlags.bLateralJump)
		{
			fInAirAbilityMult *= ( 1.0f - GetSpecialFlightModeRatio() );
			ApplyInternalTorque(fLeanSide*fInAirAbilityMult*pBikeHandling->m_fInAirSteerMult*GetAngInertia().z* VEC3V_TO_VECTOR3(GetTransform().GetC())*ms_fBikeGravityMult);
		}
	}
	// burnout controls (lets you rotate on the spot)
	else if(GetThrottle() > 0.9f && GetBrake() > 0.9f && !GetHandBrake() && m_nVehicleFlags.bEngineOn)
	{
		Vector3 UpVector(VEC3V_TO_VECTOR3(GetTransform().GetC()));
		float fRotSpeed = DotProduct(GetAngVelocity(), UpVector);
		m_fBurnoutTorque = fLeanSide * sfBikeBurnoutRotForce * pHandling->m_fTractionCurveMax * GetAngInertia().z;

		if( (fRotSpeed < sfBikeBurnoutRotLimit && fLeanSide < 0.0f) ||	(fRotSpeed > -sfBikeBurnoutRotLimit && fLeanSide > 0.0f) )
		{
			ApplyInternalTorque(m_fBurnoutTorque * UpVector);
		}
	}
}

//
//
//
//
dev_float sfBikeAnimLeanFromSteerInput	=  0.75f;	// 0.5f;
dev_float sfBikeAnimLeanFromBikeLean	= -0.75f;	//-0.5f;
dev_float sfBikeAnimSteerBlendFrac		=  0.04f;	// 0.1f;
//
void CBike::CalcAnimSteerRatio()
{
	if(GetDriver())
	{
		float fAnimSteerBlend = rage::Powf(sfBikeAnimSteerBlendFrac, fwTimer::GetTimeStep());

		float fCurrentSteerRatio = sfBikeAnimLeanFromSteerInput * (GetSteerAngle() * pHandling->m_fSteeringLockInv);
		fCurrentSteerRatio += sfBikeAnimLeanFromBikeLean * (m_fBikeLeanAngle * pHandling->GetBikeHandlingData()->m_fFullAnimAngleInv);
		fCurrentSteerRatio = rage::Clamp(fCurrentSteerRatio, -1.0f, 1.0f);

		m_fAnimSteerRatio = fAnimSteerBlend * m_fAnimSteerRatio + (1.0f - fAnimSteerBlend) * fCurrentSteerRatio;
	}
	else
	{
		m_fAnimSteerRatio = 0.0f;
	}
}

void CBike::InitConstraint()
{
	if(physicsVerifyf(!m_leanConstraintHandle.IsValid(),"Bike already has a constraint set up"))
	{
		physicsAssertf(GetCurrentPhysicsInst(),"Can't call InitConstraint with no phys inst");

		phConstraintRotation::Params leanConstraintParams;
		leanConstraintParams.instanceA = GetCurrentPhysicsInst();
		leanConstraintParams.minLimit = 0.0f;
		leanConstraintParams.maxLimit = 0.0f;


		m_leanConstraintHandle = PHCONSTRAINT->Insert(leanConstraintParams);

		if(!physicsVerifyf(m_leanConstraintHandle.IsValid(),"Failed to create bike constraint"))
		{
			return;
		}

		ProcessConstraint(true);
	}
}

void CBike::ProcessConstraint(bool bConstrainLean)
{
	if( IsInAir() &&
		GetDriver() &&
		GetSpecialFlightModeRatio() > 0.0f )
	{
		RemoveConstraint();
	}
	else if(bConstrainLean)
	{
		// Do we need to create a constraint?
		if(!m_leanConstraintHandle.IsValid())
		{
			InitConstraint();
		}

		phConstraintRotation* pLeanConstraint = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer(m_leanConstraintHandle) );

		if(physicsVerifyf(pLeanConstraint,"Unexpected NULL constraint pointer"))
		{
			Vec3V vConstraintDirection(GetTransform().GetB());
			pLeanConstraint->SetWorldAxis(vConstraintDirection);
		}
	}
	else
	{
		// Do not want to constrain lean
		// So remove constraint
		RemoveConstraint();
	}
}

void CBike::RemoveConstraint()
{
	if(m_leanConstraintHandle.IsValid())
	{
		PHCONSTRAINT->Remove(m_leanConstraintHandle);
		m_leanConstraintHandle.Reset();
	}
}

Vec3V_Out ComputeWheelBottom(const CWheel& wheel, float suspensionRaise, Mat34V_In wheelMatrix, Vec3V_In groundPosition, Vec3V_In groundNormal)
{
	// Find the closest point on on rim to the ground, then translate it downwards by the ground normal
	ScalarV rimRadius = ScalarVFromF32(wheel.GetWheelRimRadius() + suspensionRaise);
	Vec3V tyreOffset = Scale(groundNormal,ScalarVFromF32(wheel.GetTyreThickness()));
	Vec3V rimPosition = DiscTriHelper_GetClosestOnCircle(groundPosition,wheelMatrix.GetCol3(),wheelMatrix.GetCol0(),rimRadius);
	return Subtract(rimPosition,tyreOffset);
}

void CBike::FixUpSkeletonNoSuspension()
{
	Assert(!m_nBikeFlags.bHasFrontSuspension && !m_nBikeFlags.bHasRearSuspension);

	crSkeleton* pSkeleton = GetSkeleton();

	// We're going to modify the local matrix of the root bone so that the bike wheels sit on the ground better
	const crBoneData* pBoneData = pSkeleton->GetSkeletonData().GetBoneData(0);
	Mat34V_Ref rootMatrix = pSkeleton->GetLocalMtx(0);
	Mat34V originalRootMatrix;
	if(GetMoveVehicle().IsAnimating())
	{
		originalRootMatrix = rootMatrix;
	}
	else
	{
		// Reset the root matrix if the bike isn't animating
		Mat33VFromQuatV(originalRootMatrix.GetMat33Ref(),pBoneData->GetDefaultRotation());
		originalRootMatrix.SetCol3(pBoneData->GetDefaultTranslation());
	}

	if(ms_bEnableSkeletonFixup)
	{
		const Mat34V bikeMatrix = GetMatrix();

		// Get the state of the front wheel. 
		// The upper fork or handlebars will have the rotation for the wheel, it would be nice to have a better solution for this
		const CWheel& frontWheel = *GetWheelFromId(BIKE_WHEEL_F);
		const int frontRotationBoneIndex = m_RotateUpperForks ? GetBoneIndex(BIKE_FORKS_U) : GetBoneIndex(BIKE_HANDLEBARS);
		Mat34V frontWheelMatrix = Mat34V(pSkeleton->GetLocalMtx(frontRotationBoneIndex).GetMat33ConstRef(),VECTOR3_TO_VEC3V(CWheel::GetWheelOffset(GetVehicleModelInfo(),BIKE_WHEEL_F)));
		Transform(frontWheelMatrix,originalRootMatrix,frontWheelMatrix);
		Vec3V frontGroundNormal,frontGroundPosition,frontWheelBottom;
		const float sfTyreHealthMinusFlatHealth(TYRE_HEALTH_DEFAULT - TYRE_HEALTH_FLAT);
		if(frontWheel.GetIsTouching())
		{
			// Transform the contact information into the bikes local space
			frontGroundNormal = UnTransform3x3Ortho(bikeMatrix,RCC_VEC3V(frontWheel.GetHitNormal()));
			frontGroundPosition = UnTransformOrtho(bikeMatrix,RCC_VEC3V(frontWheel.GetHitPos()));

			float fTyreHealth = rage::Max(frontWheel.GetTyreHealth(),TYRE_HEALTH_FLAT) - TYRE_HEALTH_FLAT;// We want to remove any suspension raise when the tyre is flat
			float fSuspensionRaise = pHandling->m_fSuspensionRaise * (fTyreHealth / sfTyreHealthMinusFlatHealth);// don't account for the suspension raise when the wheels are damaged, as it causes the burst tyres to be in the ground. 
			frontWheelBottom = ComputeWheelBottom(frontWheel, fSuspensionRaise , frontWheelMatrix, frontGroundPosition, frontGroundNormal);
			//grcDebugDraw::Sphere(Transform(bikeMatrix,frontWheelBottom),0.03f,Color_red);
			//grcDebugDraw::Arrow(RCC_VEC3V(frontWheel.GetHitPos()), RCC_VEC3V(frontWheel.GetHitPos()) + RCC_VEC3V(frontWheel.GetHitNormal()),0.03f, Color_orange);
		}

		// Get the state of the rear wheel
		// Right now the rear wheel is never rotated, we can change that in the future though
		const CWheel& rearWheel = *GetWheelFromId(BIKE_WHEEL_R);
		Mat34V rearWheelMatrix = Mat34V(Mat33V(V_IDENTITY),VECTOR3_TO_VEC3V(CWheel::GetWheelOffset(GetVehicleModelInfo(),BIKE_WHEEL_R)));
		Transform(rearWheelMatrix,originalRootMatrix,rearWheelMatrix);
		Vec3V rearGroundNormal, rearGroundPosition, rearWheelBottom;
		if(rearWheel.GetIsTouching())
		{
			// Transform the contact information into the bikes local space
			rearGroundNormal = UnTransform3x3Ortho(bikeMatrix,RCC_VEC3V(rearWheel.GetHitNormal()));
			rearGroundPosition = UnTransformOrtho(bikeMatrix,RCC_VEC3V(rearWheel.GetHitPos()));

			float fTyreHealth = rage::Max(rearWheel.GetTyreHealth(),TYRE_HEALTH_FLAT) - TYRE_HEALTH_FLAT;// We want to remove any suspension raise when the tyre is flat
			float fSuspensionRaise = pHandling->m_fSuspensionRaise * (fTyreHealth / sfTyreHealthMinusFlatHealth) ;// don't account for the suspension raise when the wheels are damaged, as it causes the burst tyres to be in the ground.
			rearWheelBottom = ComputeWheelBottom(rearWheel, fSuspensionRaise, rearWheelMatrix, rearGroundPosition, rearGroundNormal);
			//grcDebugDraw::Sphere(Transform(bikeMatrix,rearWheelBottom),0.03f,Color_blue);
			//grcDebugDraw::Arrow(RCC_VEC3V(rearWheel.GetHitPos()), RCC_VEC3V(rearWheel.GetHitPos()) + RCC_VEC3V(rearWheel.GetHitNormal()),0.03f, Color_orange);
		}

		ScalarV rotationAngle = ScalarV(V_ZERO);
		Vec3V translation = Vec3V(V_ZERO);
		if(frontWheel.GetIsTouching() && rearWheel.GetIsTouching())
		{
			// Both wheels are touching the ground, we need to move and rotate the bike

			// First shift the bike along the rear wheel's ground normal so that the rear wheel is lined up with the ground
			Vec3V initialTranslation = Scale(Dot(Subtract(rearGroundPosition,rearWheelBottom),rearGroundNormal),rearGroundNormal);
			rearGroundPosition = Subtract(rearGroundPosition,initialTranslation);
			frontGroundPosition = Subtract(frontGroundPosition,initialTranslation);

			// Next estimate how much we need to rotate the bike about the bottom of the rear wheel so that the front wheel's bottom touches the front wheel's ground
			Vec3V rearBottomToFrontBottom = Subtract(frontWheelBottom,rearWheelBottom);
			// Find the vector perpendicular to the vector from the rear wheel bottom to the front wheel bottom and the X-axis. This is because we're rotating about the x-axis and the rear wheel bottom
			Vec3V rotationDirection = Cross(rearBottomToFrontBottom,Vec3V(V_X_AXIS_WZERO));
			// Intersect that vector with the front wheel's ground plane to figure out about where we would hit the ground when rotating about the x-axis and rear wheel
			ScalarV tVal = InvScaleSafe(Dot(frontGroundNormal, Subtract(frontGroundPosition,frontWheelBottom)), Dot(frontGroundNormal,rotationDirection), ScalarV(V_ZERO));
			Vec3V estimatedRotationIntersection = AddScaled(frontWheelBottom,rotationDirection,tVal);
			
			// Using where the front wheel bottom is and where it hits the front ground we can estimate a rotation angle
			ScalarV rotationAngleMag = InvScale(Dist(estimatedRotationIntersection,frontWheelBottom),Mag(rearBottomToFrontBottom));
			rotationAngle = SelectFT(IsGreaterThan(tVal,ScalarV(V_ZERO)),rotationAngleMag,Negate(rotationAngleMag));

			// Since we aren't rotating about the center of the bike we need to add an additional offset
			translation = rage::Add(Subtract(rearWheelBottom,RotateAboutXAxis(rearWheelBottom,rotationAngle)), initialTranslation);
		}
		// If only one wheel is touching then we can just shift the bike along the wheel's ground normal until it is sitting on the ground
		else if(frontWheel.GetIsTouching())
		{
			translation = Scale(Dot(Subtract(frontGroundPosition,frontWheelBottom),frontGroundNormal),frontGroundNormal);
		}
		else if(rearWheel.GetIsTouching())
		{
			translation = Scale(Dot(Subtract(rearGroundPosition,rearWheelBottom),rearGroundNormal),rearGroundNormal);
		}

		// Increase how faster we can fixup the bike the lower the speed is
		const ScalarV speed = Mag(RCC_VEC3V(GetVelocity()));
		const ScalarV lowSpeed = ScalarV(V_ZERO);
		const ScalarV highSpeed = ScalarV(V_ONE);
		const ScalarV speedModifier = Ramp(Clamp(speed,lowSpeed,highSpeed),lowSpeed,highSpeed,ScalarV(V_FOUR),ScalarV(V_ONE));

		// Clamp the rotation to get our final rotation about the x-axis
		const ScalarV maxRot(ms_fSkeletonFixupMaxRot);
		const ScalarV maxRotChange = Scale(ScalarV(ms_fSkeletonFixupMaxRotChange),speedModifier);
		ScalarV previousRotation = ScalarVFromF32(m_fSkeletonFixupPreviousAngle);
		ScalarV rotationChange = Subtract(rotationAngle,previousRotation);
		ScalarV finalRotation = Clamp(rage::Add(previousRotation,Clamp(rotationChange,Negate(maxRotChange),maxRotChange)),Negate(maxRot),maxRot);
		m_fSkeletonFixupPreviousAngle = finalRotation.Getf();

		// Clamp the translation to get the final offset
		const ScalarV maxTrans(ms_fSkeletonFixupMaxTrans);
		const ScalarV maxTransChange = Scale(ScalarV(ms_fSkeletonFixupMaxTransChange),speedModifier);
		Vec3V previousTranslation = m_vecSkeletonFixupPreviousTranslation;
		Vec3V translationChange = Subtract(translation,previousTranslation);
		Vec3V finalTranslation = ClampMag(rage::Add(previousTranslation,ClampMag(translationChange,ScalarV(V_ZERO),maxTransChange)),ScalarV(V_ZERO),maxTrans);
		m_vecSkeletonFixupPreviousTranslation = finalTranslation;

		// Add the final fixup matrix to the skeleton
		Mat34V fixupMatrix(Mat33V(V_IDENTITY),finalTranslation);
		Mat34VRotateLocalX(fixupMatrix,finalRotation);

		Transform(rootMatrix,originalRootMatrix,fixupMatrix);
	}
	else
	{
		rootMatrix = originalRootMatrix;
	}
}

//
//
//
//
ePrerenderStatus CBike::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	if(!IsDummy())
	{
		// vehicle damage fx
		GetVehicleDamage()->PreRender();
	}

	// update wheel bone matrices
	for(int i=0; i<GetNumWheels(); i++)
	{
		m_ppWheels[i]->ProcessWheelMatrixForBike();
	}	

	bool updateWheels = ( HasGlider() || GetSpecialFlightModeRatio() == 0.0f );

	if( !m_nVehicleFlags.bAnimateWheels && !( (fwTimer::IsGamePaused()) ||  m_nDEflags.bFrozenByInterior) &&
		updateWheels )
	{
		// moving of the suspension geometry is all special case code for bikes
		if(GetSkeleton())
		{
			if(m_nBikeFlags.bHasFrontSuspension || m_nBikeFlags.bHasRearSuspension)
			{
				crSkeleton* pSkel = GetSkeleton();

				// Front wheel first
				//
				CWheel* pWheelF = GetWheelFromId(BIKE_WHEEL_F);

				if(m_nBikeFlags.bHasFrontSuspension)// don't fixup the tyre position on fixed bikes
				{
					int nWheelIndexF = GetBoneIndex(BIKE_WHEEL_F);
					const crBoneData* pBoneDataF = pSkel->GetSkeletonData().GetBoneData(nWheelIndexF);
					const crBoneData* pParentBoneDataF = pBoneDataF->GetParent();

					float fZDiff = pWheelF->GetWheelCompressionFromStaticIncBurstAndSink();

					if( GetSpecialFlightModeRatio() > 0.0f &&
						!HasGlider() )
					{
						fZDiff = GetSpecialFlightModeRatio() < 1.0f ? fZDiff : pWheelF->GetCompression() - pWheelF->GetStaticDelta();;
						const phSegment& wheelLines = pWheelF->GetProbeSegment();

						float minCompression = wheelLines.A.z - wheelLines.B.z;
						minCompression *= GetSpecialFlightModeRatio();
						minCompression -= pWheelF->GetWheelRadius() + pWheelF->GetStaticDelta();

						if( GetSpecialFlightModeRatio() == 1.0f )
						{
							fZDiff = minCompression;
						}
						else
						{
							fZDiff = Max( fZDiff, minCompression );
						}
					}

					const crBoneData* pLowerForkBoneData = NULL;
					if(GetBoneIndex(BIKE_FORKS_L) != -1)
						pLowerForkBoneData = pSkel->GetSkeletonData().GetBoneData(GetBoneIndex(BIKE_FORKS_L));
					
					if(pLowerForkBoneData)
					{
						Matrix34& lowerForkBoneMatrix = RC_MATRIX34(pSkel->GetLocalMtx(pLowerForkBoneData->GetIndex()));
						lowerForkBoneMatrix.FromQuaternion(RCC_QUATERNION(pLowerForkBoneData->GetDefaultRotation()));
						lowerForkBoneMatrix.d = RCC_VECTOR3(pLowerForkBoneData->GetDefaultTranslation());
					}

					const crBoneData* pForkBoneData = NULL;
					if(GetBoneIndex(BIKE_FORKS_U) != -1)
						pForkBoneData = pSkel->GetSkeletonData().GetBoneData(GetBoneIndex(BIKE_FORKS_U));

					if(pForkBoneData && pForkBoneData!=pParentBoneDataF)
					{
						Matrix34 forkBoneMatrix( RC_MATRIX34(pSkel->GetLocalMtx(pForkBoneData->GetIndex())));
						forkBoneMatrix.FromQuaternion(RCC_QUATERNION(pForkBoneData->GetDefaultRotation()));

						if(forkBoneMatrix.c.z > 0.0f)
							fZDiff /= forkBoneMatrix.c.z;
					}

					Matrix34& parentBoneMatrixF = RC_MATRIX34(pSkel->GetLocalMtx(pParentBoneDataF->GetIndex()));
					parentBoneMatrixF.d = RCC_VECTOR3(pParentBoneDataF->GetDefaultTranslation());
					parentBoneMatrixF.d += parentBoneMatrixF.c * fZDiff;
				}

				// Rear wheel after
				//
				CWheel* pWheelR = GetWheelFromId(BIKE_WHEEL_R);
				int nWheelIndexR = GetBoneIndex(BIKE_WHEEL_R);
				const crBoneData* pBoneDataR = pSkel->GetSkeletonData().GetBoneData(nWheelIndexR);
				const crBoneData* pParentBoneDataR = pBoneDataR->GetParent();
				Matrix34& parentBoneMatrixR = RC_MATRIX34(pSkel->GetLocalMtx(pParentBoneDataR->GetIndex()));

				// hard tail option
				if(!m_nBikeFlags.bHasRearSuspension)
				{
					float originalAngle;
					if (!GetMoveVehicle().IsAnimating())
					{
						parentBoneMatrixR.d = RCC_VECTOR3(pParentBoneDataR->GetDefaultTranslation());
						parentBoneMatrixR.Identity3x3();
						originalAngle = 0.0f;
						parentBoneMatrixR.d.z += pHandling->m_fSuspensionRaise;
					}
					else
					{
						originalAngle = -parentBoneMatrixR.GetEulers().x;
					}
					Vector3 vecFrontWheelPivot = pWheelF->GetProbeSegment().B;
					vecFrontWheelPivot.z += pWheelF->GetWheelCompressionIncBurstAndSink();

					float fZDiff = pWheelR->GetWheelCompressionFromStaticIncBurstAndSink();

					if( GetSpecialFlightModeRatio() > 0.0f &&
						!HasGlider() )
					{
						fZDiff = GetSpecialFlightModeRatio() < 1.0f ? fZDiff : pWheelR->GetCompression() - pWheelR->GetStaticDelta();
						const phSegment& wheelLines = pWheelR->GetProbeSegment();

						float minCompression = ( wheelLines.A.z - wheelLines.B.z );
						minCompression *= GetSpecialFlightModeRatio();
						minCompression -= pWheelR->GetWheelRadius() + pWheelR->GetStaticDelta();

						if( GetSpecialFlightModeRatio() == 1.0f )
						{
							fZDiff = minCompression;
						}
						else
						{
							fZDiff = Max( fZDiff, minCompression );
						}
					}

					float fAng = rage::Atan2f( fZDiff, vecFrontWheelPivot.y - Transform(RCC_MAT34V(parentBoneMatrixR),pBoneDataR->GetDefaultTranslation()).GetYf()) - originalAngle;

					// At higher speeds apply less of a fixup. This reduces the jerkiness caused by following the terrain too well with no suspension
					const float speedForFullFixup = 6.0f;
					const float speedForNoFixup = 15.0f;
					const float speed = GetVelocity().Mag();
					const float fixupInterpolation = Clamp(rage::Range(speed,speedForNoFixup,speedForFullFixup),0.0f,1.0f);
					fAng *= fixupInterpolation;

					parentBoneMatrixR.RotateLocalX(-fAng);

					float fTrans = rage::Sinf(fAng) * vecFrontWheelPivot.y;
					parentBoneMatrixR.d.z += fTrans;
				}
				else
				{
					float fAng = pWheelR->GetWheelCompressionFromStaticIncBurstAndSink() / RCC_VECTOR3(pBoneDataR->GetDefaultTranslation()).Mag();
					fAng = rage::Asinf(Clamp(fAng, -1.0f, 1.0f));

					parentBoneMatrixR.FromQuaternion(RCC_QUATERNION(pParentBoneDataR->GetDefaultRotation()));
					parentBoneMatrixR.RotateLocalX(-fAng);
				}
			}
			else
			{
				FixUpSkeletonNoSuspension();
			}
		}
	}

	return CVehicle::PreRender(bIsVisibleInMainViewport);
	
}// end of CBike::PreRender()...


void CBike::PreRender2(const bool bIsVisibleInMainViewport)
{
	if (m_StartedAnimDirectorPreRenderUpdate)
	{
		GetAnimDirector()->WaitForPreRenderUpdateToComplete();
		m_StartedAnimDirectorPreRenderUpdate = false;
	}

	//Make sure the windscreen bound rotates on bagger and sovereign.
	if(GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE && (m_nVehicleFlags.bAnimateWheels==0))
	{
		int windscreenBoneIndex = -1;
		if((windscreenBoneIndex = GetBoneIndex(VEH_WINDSCREEN)) == -1)// Sovereign windscreen is names windscreen.
		{
			//HACK
			windscreenBoneIndex = GetBoneIndex(VEH_MISC_F); // bagger windscreen is named misc_F.
		}

		if(GetVehicleFragInst() && windscreenBoneIndex > -1)
		{
			int nComponentIndex = GetVehicleFragInst()->GetComponentFromBoneIndex(windscreenBoneIndex);

			if(nComponentIndex > -1)
			{
				Mat34V matWindscreen;
				GetGlobalMtx(windscreenBoneIndex, RC_MATRIX34(matWindscreen));

				// B*1912648 & 1913761: Don't give the bounds a zeroed-out matrix. That causes trouble. The windscreen bone can get zeroed out if the glass is smashed.
				if(!(IsZeroAll(matWindscreen.a()) && IsZeroAll(matWindscreen.b()) && IsZeroAll(matWindscreen.c())))
				{
					Mat34V vehicleMat = GetMatrix();
					Mat34V matWindscreenLocal;
					UnTransformFull(matWindscreenLocal, vehicleMat, matWindscreen);

					GetVehicleFragInst()->GetCacheEntry()->GetBound()->SetCurrentMatrix(nComponentIndex, matWindscreenLocal);
					GetVehicleFragInst()->GetCacheEntry()->GetBound()->SetLastMatrix(nComponentIndex, matWindscreenLocal);
				}
			}
		}
	}

	CVehicle::PreRender2(bIsVisibleInMainViewport);

	u32 iFlags = 0;
	if( m_nVehicleFlags.bForceBrakeLightOn )
	{
		iFlags |= LIGHTS_FORCE_BRAKE_LIGHTS;
	}

	Vec4V ambientVolumeParams = Vec4V(V_ZERO_WONE);

	// lower AO Volume for selected vehicles:
	const u32 vehNameHash = GetBaseModelInfo()->GetModelNameHash();

	if(vehNameHash == MID_MANCHEZ2)
	{
		ambientVolumeParams = Vec4V(Manchez2_AmbientVolume_Offset, ScalarV(V_ONE));
	}

	DoVehicleLights(iFlags, ambientVolumeParams);

	// Sets the indicator bits in the Vehicle flags straight after they have been queried for the lights.
	m_nVehicleFlags.bForceBrakeLightOn = false;
	m_nBikeFlags.bInputsUpToDate = false;


	// ground disturbance vfx
	if (MI_BIKE_OPPRESSOR2.IsValid() && GetModelIndex()==MI_BIKE_OPPRESSOR2)
	{
		if (m_nVehicleFlags.bEngineOn || IsRunningCarRecording())
		{
			if (GetSpecialFlightModeRatio()==1.0f)
			{
				//g_vfxVehicle.UpdatePtFxGlideHaze(this, ATSTRINGHASH("veh_ba_oppressor_glide_haze", 0xB697BB5));
				g_vfxVehicle.UpdatePtFxPlaneAfterburner(this, VEH_MISC_N, 0);
				g_vfxVehicle.ProcessGroundDisturb(this);
			}
		}
	}

}


///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : BlowUpCar
// PURPOSE :  Does everything needed to destroy a car
///////////////////////////////////////////////////////////////////////////////////

void CBike::BlowUpCar( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool bDelayedExplosion )
{
	CVehicle::BlowUpCar(pCulprit);
#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow cars up a 2nd time
	}

	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;	//	If the flag is set for don't damage this car (usually during a cutscene)
	}

	// Disable the COM offset
	GetVehicleFragInst()->GetArchetype()->GetBound()->SetCGOffset(Vec3V(V_ZERO_WONE));
	phCollider *pCollider = GetCollider();
	if(pCollider)
		pCollider->SetColliderMatrixFromInstance();

	// we can't blow up bikes controlled by another machine
	// but we still have to change their status to wrecked
    // so the bike doesn't blow up if we take control of an
    // already blown up bike
	if (IsNetworkClone())
    {
		Assertf(bNetCall, "Trying to blow up clone %s", GetNetworkObject()->GetLogName());

        KillPedsInVehicle(pCulprit, weaponHash);
    	KillPedsGettingInVehicle(pCulprit);

		m_nPhysicalFlags.bRenderScorched = TRUE;  
		SetTimeOfDestruction();
		
		SetIsWrecked();

		// Break lights, windows and sirens
		GetVehicleDamage()->BlowUpCarParts(pCulprit);

		// Switch off the engine. (For sound purposes)
		this->SwitchEngineOff(false);
		this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
		this->m_nVehicleFlags.bLightsOn = FALSE;

		//Check to see that it is the player
		if (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
		{
			CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
		}

		return;
    }

	if (NetworkUtils::IsNetworkCloneOrMigrating(this))
	{
		// the vehicle is migrating. Create a weapon damage event to blow up the vehicle, which will be sent to the new owner. If the migration fails
		// then the vehicle will be blown up a little later.
		CBlowUpVehicleEvent::Trigger(*this, pCulprit, bAddExplosion, weaponHash, bDelayedExplosion);
		return;
	}

	//Total damage done for the damage trackers
	float totalDamage = GetHealth() + m_VehicleDamage.GetEngineHealth() + m_VehicleDamage.GetPetrolTankHealth();
	for(s32 i=0; i<GetNumWheels(); i++)
	{
		totalDamage += m_VehicleDamage.GetTyreHealth(i);
		totalDamage += m_VehicleDamage.GetSuspensionHealth(i);
	}
	totalDamage = totalDamage > 0.0f ? totalDamage : 1000.0f;

	//AddToMoveSpeedZ(0.13f);
	ApplyImpulseCg(Vector3(0.0f, 0.0f, 6.5f));
	SetWeaponDamageInfo(pCulprit, weaponHash, fwTimer::GetTimeInMilliseconds());
  
	//Set the destruction information.
	SetDestructionInfo(pCulprit, weaponHash);
	
	SetIsWrecked();

	m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off

	// do it before SpawnFlyingComponent() so this bit is propagated to all vehicle parts before they go flying:
	// let know pipeline, that we don't want any extra passes visible for this clump anymore:
//	CVisibilityPlugins::SetClumpForAllAtomicsFlag((RpClump*)this->m_pRwObject, VEHICLE_ATOMIC_PIPE_NO_EXTRA_PASSES);

	SetHealth(0.0f);	// Make sure this happens before AddExplosion or it will blow up twice

	KillPedsInVehicle(pCulprit, weaponHash);

	// Break lights, windows and sirens
	GetVehicleDamage()->BlowUpCarParts(pCulprit);
	
	// Switch off the engine. (For sound purposes)
	this->SwitchEngineOff(false);
	this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
	this->m_nVehicleFlags.bLightsOn = FALSE;

	if( bAddExplosion )
	{
		AddVehicleExplosion(pCulprit, bInACutscene, bDelayedExplosion);
	}

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
	}

	g_decalMan.Remove(this);

	CPed* fireCulprit = NULL;
	if (pCulprit && pCulprit->GetIsTypePed())
	{
		fireCulprit = static_cast<CPed*>(pCulprit);
	}
	g_vfxVehicle.ProcessWreckedFires(this, fireCulprit, FIRE_DEFAULT_NUM_GENERATIONS);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlayCarHorn
// PURPOSE :  Play the car horn if not already triggered.  Also shouts abuse
/////////////////////////////////////////////////////////////////////////////////

#define MIN_SPEAK_TIME			1500
#define SPEAK_TIME_OFFSET		3000

#define MIN_NO_HORN_TIME		150


void CBike::PlayCarHorn(bool /*bOneShot*/,u32 hornTypeHash)
{
	//u32 r;

	if(!IsAlarmActivated())
	{
		//if(!IsHornOn()) 
		{
// 			if(m_NoHornCount > 0 && !bOneShot)
// 			{
// 				m_NoHornCount--;
// 				return;
// 			}

			//m_NoHornCount = u8(MIN_NO_HORN_TIME + (fwRandom::GetRandomNumber() & 0x7f));
			
	//		r = CGeneral::GetRandomNumber() & 0xf;	// 
			
			//r = m_NoHornCount & 0x7;


			//if(r < 2 || r < 4)
			{
				// Horn only
				if(m_VehicleAudioEntity)
				{
					m_VehicleAudioEntity->SetHornType(hornTypeHash);
					m_VehicleAudioEntity->PlayVehicleHorn(0.96f, true);
				}
			}

			if(ShouldGenerateMinorShockingEvent())
			{
				CEventShockingHornSounded ev(*this);
				CShockingEventsManager::Add(ev);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlayHornIfNecessary
// PURPOSE :  Checks to see if reason to play car horn / shout abuse
/////////////////////////////////////////////////////////////////////////////////

void CBike::PlayHornIfNecessary(void)
{
	// Only do for simple and physics cars - not player, abandoned etc
	if((GetIntelligence()->GetSlowingDownForPed() || GetIntelligence()->GetSlowingDownForCar()) && (!HasCarStoppedBecauseOfLight(true)))
	{
		if(fwRandom::GetRandomNumberInRange(0.f,1.f) > 0.8f)
		{
			PlayCarHorn();
		}
	}
}

// Name			:	ScanForCrimes
// Purpose		:	Checks area surrounding car for crimes being commited. These are reported immediately
// Parameters	:	None
// Returns		:	Nothing

/* // this will probably go back in again (cop bikes?)

#define COP_CAR_CRIME_DETECTION_RANGE (20.0f)

void CBike::ScanForCrimes()
{
//	float fDistance;
	Vector3 vecEvent;
//	s32 i;
	eEventType EventType = EVENT_NULL;
//	eCrimeType	CrimeWitnessed;

	
	// If the player drives a cop with car alarm activated we set the wanted status
	// to a set minimum in one go.
	if (FindPlayerVehicle() && FindPlayerVehicle()->GetIsAutomobile())
	{
		if ( ((CAutomobile *)FindPlayerVehicle())->IsAlarmActivated() )
		{
			// Test whether we are close enough to notice player
			Vector3	Dist;
			Dist = FindPlayerVehicle()->GetPosition() - this->GetPosition();
			if (Dist.Mag2() < COP_CAR_CRIME_DETECTION_RANGE*COP_CAR_CRIME_DETECTION_RANGE)
			{
				FindPlayerPed()->GetPlayerInfo()->pPed->SetWantedLevelNoDrop(WANTED_LEVEL1);
			}
		}
	}
}
*/






/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlaceOnRoadProperly
// PURPOSE :  If the car is placed kinda on the road this function will
//			  do it properly
// RETURNS :  true if collision was actually found for front AND rear point.
/////////////////////////////////////////////////////////////////////////////////
bool CBike::PlaceOnRoadProperly(CBike* pBike, Matrix34 *pMat, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, bool& setWaitForAllCollisionsBeforeProbe, u32 MI, phInst* pTestException, bool useVirtualRoad, CNodeAddress* aNodes, s16 * aLinks, s32 numNodes, float HeightSampleRangeUp, float HeightSampleRangeDown, bool bDontChangeMatrix)
{
	setWaitForAllCollisionsBeforeProbe = false;

	const ScalarV fOne(V_ONE);
	fwModelId modelId((strLocalIndex(MI)));
	Assert(modelId.IsValid());
	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(modelId);
	Assert(pModelInfo);
	Assert(pModelInfo->GetIsBike());
	gtaFragType *pFragType = pModelInfo->GetFragType();
	Assert(pFragType);
	fragDrawable *pDrawable = pFragType->GetCommonDrawable();
	Assert(pDrawable);
	Assert(pDrawable->GetSkeletonData());
	crSkeletonData* pSkelData = pDrawable->GetSkeletonData();
	Vector3 vecResults[2];
	eHierarchyId nWheelIds[2] = { BIKE_WHEEL_F, BIKE_WHEEL_R };
	for(int i=0; i<2; i++)
	{
		vecResults[i].Zero();
		int nBoneId = pModelInfo->GetBoneIndex(nWheelIds[i]);

		if(nBoneId!=-1)
		{
			vecResults[i] = RCC_VECTOR3(pSkelData->GetBoneData(nBoneId)->GetDefaultTranslation());

			const crBoneData* pParentBoneData = pSkelData->GetBoneData(nBoneId)->GetParent();
			while(pParentBoneData)
			{
				Matrix34 matParent;
				matParent.FromQuaternion(RCC_QUATERNION(pParentBoneData->GetDefaultRotation()));
				matParent.d = RCC_VECTOR3(pParentBoneData->GetDefaultTranslation());
				matParent.Transform(vecResults[i]);

				pParentBoneData = pParentBoneData->GetParent();
			}
		}
	}

	Vector3 posFront = vecResults[0];
	Vector3 posRear  = vecResults[1];

	// Deal with the vehicle being upside down. Flip it round.
	if (pMat->c.z < 0.0f)
	{
		if(bDontChangeMatrix)
			return false;

		pMat->a = -pMat->a;
		pMat->c = -pMat->c;
	}

	Vector3		FrontPoint, RearPoint;
	Vector3		VecAlongLength, ResultCoors;
	Vector3		Up;
	bool		bHeightFoundFront = false;
	bool		bHeightFoundRear = false;
	float		HeightFound = pMat->d.z;

	float	CarLengthFront = posFront.y;
	float	CarLengthRear = -posRear.y;


	// Calculate front point
	VecAlongLength = pMat->b;

	// Calculate rear point
	RearPoint.x = pMat->d.x - (VecAlongLength.x * CarLengthRear);
	RearPoint.y = pMat->d.y - (VecAlongLength.y * CarLengthRear);

	// Set new front point
	FrontPoint.x = pMat->d.x + (VecAlongLength.x * CarLengthFront);
	FrontPoint.y = pMat->d.y + (VecAlongLength.y * CarLengthFront);

	// Read the heights of the map for the front and rear points
	FrontPoint.z = pMat->d.z;
	RearPoint.z = pMat->d.z;

	// Do the 4 line scans in one batch. Should be a lot faster.
	static phSegment		testSegments[4];
	static WorldProbe::CShapeTestFixedResults<4> testResults;

	testResults.Reset();

	testSegments[0].Set(FrontPoint + Vector3(0.0f, 0.0f, HeightSampleRangeUp), FrontPoint);
	testSegments[1].Set(FrontPoint, FrontPoint - Vector3(0.0f, 0.0f, HeightSampleRangeDown));
	testSegments[2].Set(RearPoint + Vector3(0.0f, 0.0f, HeightSampleRangeUp), RearPoint);
	testSegments[3].Set(RearPoint, RearPoint - Vector3(0.0f, 0.0f, HeightSampleRangeDown));

	Vector3 vecCenter = (RearPoint + FrontPoint) * 0.5f;
	float	Radius = rage::Max(CarLengthRear, CarLengthFront) + MAX(HeightSampleRangeUp, HeightSampleRangeDown);
	Matrix34 matSegBox;
	matSegBox.Identity();
	matSegBox.d = Vector3(Radius, Radius, Radius);

	// Determine if we are forced to use the real road surface instead of the virtual road.
	// This happens when the nodes are in tunnels.
	if(useVirtualRoad)
	{
		if(aNodes && (numNodes >= 3))
		{
			bool inTunnel = false;
			for(int i = 0; i < 3; ++i)
			{
				const CPathNode* pNode = ThePaths.FindNodePointer(aNodes[i]);
				Assert(pNode);
				if(pNode->m_2.m_inTunnel)
				{
					inTunnel = true;
					break;
				}
			}

			if(inTunnel)
			{
				// We will need to store data about interior and that data is only in the collision info so
				// we need to use the world collision mesh and not the virtual road thus we also need to make
				// sure the world collision mesh around us is loaded.
				const bool worldCollisionLoadedHere = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(RCC_VEC3V(vecCenter), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
				if(worldCollisionLoadedHere)
				{
					useVirtualRoad = false;
				}
				else
				{
					if(pBike)
					{
						pBike->GetPortalTracker()->SetWaitForAllCollisionsBeforeProbe(true);
					}
					else
					{
						setWaitForAllCollisionsBeforeProbe = true;
					}
				}
			}
		}
	}

	const CVehicleFollowRouteHelper* pFollowRoute = NULL;
	if (pBike)
	{
		if (pBike->m_nVehicleFlags.bHasParentVehicle && CVehicle::IsEntityAttachedToTrailer(pBike) && pBike->GetDummyAttachmentParent()
			&& pBike->GetDummyAttachmentParent()->GetDummyAttachmentParent())
		{
			pFollowRoute = pBike->GetDummyAttachmentParent()->GetDummyAttachmentParent()->GetIntelligence()->GetFollowRouteHelper();
		}
		else
		{
			pFollowRoute = pBike->GetIntelligence()->GetFollowRouteHelper();
		}
	}

	if(useVirtualRoad && pFollowRoute)
	{
		CVirtualRoad::TestProbesToVirtualRoadFollowRouteAndCenterPos(*pFollowRoute, pBike->GetVehiclePosition(), testResults, testSegments, 4, fOne, pBike->ShouldTryToUseVirtualJunctionHeightmaps());
	}
	else if(useVirtualRoad && numNodes==3) // as used by vehicle population code
	{
		CVehicleNodeList nodeList;
		nodeList.SetPathNodeAddr(NODE_OLD, aNodes[0]);
		nodeList.SetPathLinkIndex(LINK_OLD_TO_NEW, aLinks[0]);
		nodeList.SetPathNodeAddr(NODE_NEW, aNodes[1]);
		nodeList.SetPathLinkIndex(LINK_NEW_TO_FUTURE, aLinks[1]);
		nodeList.SetPathNodeAddr(NODE_FUTURE, aNodes[2]);
		nodeList.SetTargetNodeIndex(NODE_NEW);

		CVirtualRoad::TestProbesToVirtualRoadNodeListAndCenterNode(nodeList, NODE_NEW, testResults, testSegments, 4, fOne);
	}
	else
	{
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&testResults);
		probeDesc.SetExcludeInstance(pTestException);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
		probeDesc.SetTypeFlags(TYPE_FLAGS_ALL);
		for(int i=0; i<4; i++)
		{
			probeDesc.SetStartAndEnd(testSegments[i].A, testSegments[i].B);
			probeDesc.SetFirstFreeResultOffset(i);
			probeDesc.SetMaxNumResultsToUse(1);

			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		}

		//	Single probes are MUCH faster than a batch with a large bound radius - the dummy car switching is using 100 to -100 range?! 
		// CPhysics::GetLevel()->TestProbeBatch(4, (const phSegment **)segmentPrts, vecCenter, Radius, matSegBox, &intersectionPtrs[0], pTestException,
		// 	ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	}

	int nWheelProbeSelect[2];
	nWheelProbeSelect[0] = -1;	// wheel 0 is rear wheel on bikes
	nWheelProbeSelect[1] = -1;	// wheel 1 is front wheel

	if(testResults[0].GetHitDetected())
	{
		bHeightFoundFront = true;
		HeightFound = testResults[0].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN;
		nWheelProbeSelect[1] = 0;
	}
	if(testResults[1].GetHitDetected())
	{
		if(bHeightFoundFront)
		{	// Take the nearest of the 2 heights
			if ( rage::Abs(FrontPoint.z - HeightFound) > rage::Abs(FrontPoint.z - testResults[1].GetHitPosition().z) )
			{
				HeightFound = testResults[1].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN;
				nWheelProbeSelect[1] = 1;
			}
		}
		else
		{
			HeightFound = testResults[1].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN;
			nWheelProbeSelect[1] = 1;
		}
		bHeightFoundFront = true;
	}

	// store data about interior if any was found from the front probes (for when adding vehicle to world)
	if(!useVirtualRoad)
	{
		if (nWheelProbeSelect[1] > -1){
			phInst* pPhInst = testResults[nWheelProbeSelect[1]].GetHitInst();
			Assert(pPhInst);
			//CInteriorInst* pIntInst = reinterpret_cast<CInteriorInst*>(pPhInst->GetUserData());
			
			s32 roomIdx = PGTAMATERIALMGR->UnpackRoomId(testResults[nWheelProbeSelect[1]].GetHitMaterialId());

			if (roomIdx > 0)
			{
				CEntity* pHitEntity  = reinterpret_cast<CEntity*>(pPhInst->GetUserData());
				CInteriorProxy* pInteriorProxy = CInteriorProxy::GetInteriorProxyFromCollisionData(pHitEntity, &pPhInst->GetPosition());
				Assert(pInteriorProxy);

				CInteriorInst* pIntInst = NULL;
				if (pInteriorProxy)
				{
					pIntInst = pInteriorProxy->GetInteriorInst();
				}

				if (pIntInst && pIntInst->CanReceiveObjects())
				{
					pDestInteriorInst = pIntInst;
					destRoomIdx = roomIdx;
				} else
				{
					return(false);				// if interior not ready then immediately bail out
				}
			}
		}
	}

	if (bHeightFoundFront)
	{
		// If we didn't find anything we keep the value we already had. We've probably
		// found a hole in the map.
		FrontPoint.z = HeightFound;
	}

	if(testResults[2].GetHitDetected())
	{
		bHeightFoundRear = true;
		HeightFound = testResults[2].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN;
		nWheelProbeSelect[0] = 2;
	}
	if(testResults[3].GetHitDetected())
	{
		if (bHeightFoundRear)
		{	// Take the nearest of the 2 heights
			if ( rage::Abs(RearPoint.z - HeightFound) > rage::Abs(RearPoint.z - testResults[3].GetHitPosition().z) )
			{
				HeightFound = testResults[3].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN;
				nWheelProbeSelect[0] = 3;
			}
		}
		else
		{
			HeightFound = testResults[3].GetHitPosition().z + CONCAVE_DISTANCE_MARGIN;
			nWheelProbeSelect[0] = 3;
		}
		bHeightFoundRear = true;
	}
	if (bHeightFoundRear)
	{
		// If we didn't find anything we keep the value we already had. We've probably
		// found a hole in the map.
		RearPoint.z = HeightFound;
	}


	float	frontHeightAboveGround, rearHeightAboveGround;
	CVehicle::CalculateHeightsAboveRoad(modelId, &frontHeightAboveGround, &rearHeightAboveGround);

	if(bHeightFoundFront)
	{
		FrontPoint.z += frontHeightAboveGround + (pBike ? pBike->GetWheelFromId(BIKE_WHEEL_F)->GetWheelImpactOffset() : 0.0f);
	}
	if(bHeightFoundRear)
	{
		RearPoint.z += rearHeightAboveGround + (pBike ? pBike->GetWheelFromId(BIKE_WHEEL_R)->GetWheelImpactOffset() : 0.0f);
	}

	if(!bDontChangeMatrix)
	{
		// Generate the new coordinates and matrix
		// Side vector
		pMat->a.x = (FrontPoint.y - RearPoint.y);
		pMat->a.y = -(FrontPoint.x - RearPoint.x);
		pMat->a.z = 0.0f;
		pMat->a.Normalize();
		// Front vector
		Vector3	FrontVec = FrontPoint - RearPoint;
		FrontVec.Normalize();
		pMat->b.x = FrontVec.x;
		pMat->b.y = FrontVec.y;
		pMat->b.z = FrontVec.z;
		// Up vector
		Up = CrossProduct(pMat->a, pMat->b);
		pMat->c.x = Up.x;
		pMat->c.y = Up.y;
		pMat->c.z = Up.z;

		float fLeanAngle = 0.0f;
		if(pBike && (!CVehicleRecordingMgr::IsPlaybackGoingOnForCar(pBike) || CVehicleRecordingMgr::IsPlaybackSwitchedToAiForCar(pBike)))
		{
			if(pBike->IsConsideredStill(pBike->GetVelocity()) && pBike->m_fBikeLeanAngle != 0.0f && pBike->pHandling->GetBikeHandlingData())
			{
				fLeanAngle = pBike->pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle;
			}

			pBike->m_fBikeLeanAngle = fLeanAngle;
			pMat->RotateLocalY(pBike->m_fBikeLeanAngle);
		}
		
		// Write the new coordinates and matrix
		pMat->d = (FrontPoint * CarLengthRear + RearPoint * CarLengthFront) / (CarLengthRear + CarLengthFront);

		Assert(pMat->c.z > 0.0f);
	}

	// fix up wheel compression ratios so the wheels touch the intersection points correctly
	if(pBike)
	{
		// fix up wheel compression
		for(int i=0; i<2; i++)
		{
			if(pBike->GetWheel(i) && nWheelProbeSelect[i] > -1)
			{
				Vector3 vHitPosition = testResults[nWheelProbeSelect[i]].GetHitPosition();
				vHitPosition.z += CONCAVE_DISTANCE_MARGIN;

				pBike->GetWheel(i)->SetCompressionFromHitPos(pMat, vHitPosition, testResults[nWheelProbeSelect[i]].GetHitDetected());
			}
		}
	}

	return (bHeightFoundFront && bHeightFoundRear && (rage::Abs(pMat->b.z) < 0.5f));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlaceOnRoadAdjustInternal
// PURPOSE :  If the car is placed kinda on the road this function will
//			  do it properly
/////////////////////////////////////////////////////////////////////////////////

bool CBike::PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression)
{
	Matrix34 Mat = MAT34V_TO_MATRIX34(GetMatrix());
	CInteriorInst*	pDestInteriorInst = 0;		
	s32			destRoomIdx = -1;
	bool setWaitForAllCollisionsBeforeProbe = false;
	bool bResult = PlaceOnRoadProperly(this, &Mat, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, GetModelIndex(), GetCurrentPhysicsInst(), false, NULL, NULL, 0, HeightSampleRangeUp, HeightSampleRangeDown, bJustSetCompression);
	if(bResult && !bJustSetCompression)
	{
		SetMatrix(Mat);

		if(!GetDriver())
		{
			SetBikeOnSideStand(true, true);
		}
	}
	return bResult;
}



void CBike::SetBikeOnSideStand(bool bOn, bool bSetSteeringAndLeanAngles, float fOnStandSteerAngle, float fOnStandLeanAngle)
{
	m_nBikeFlags.bOnSideStand = bOn;

	if(bSetSteeringAndLeanAngles)
	{
		if(bOn)
		{
			fOnStandSteerAngle = ((fOnStandSteerAngle == FLT_MAX) ? pHandling->GetBikeHandlingData()->m_fBikeOnStandSteerAngle : fOnStandSteerAngle);
			fOnStandLeanAngle  = ((fOnStandLeanAngle  == FLT_MAX) ? pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle  : fOnStandLeanAngle);

			if(!m_RotateUpperForks)
			{
				//Account for bikes with forks that rotate as a child of the steering wheel
				fOnStandSteerAngle *= InvertSafe(GetVehicleModelInfo()->GetSteeringWheelMult(GetDriver()), 1.0f);
			}
			SetSteerAngle(fOnStandSteerAngle);
			m_fBikeLeanAngle = fOnStandLeanAngle;
			if(GetDoor(0))
				GetDoor(0)->SetBikeDoorConstrained();
		}
		else
		{
			SetSteerAngle(0.0f);
			m_fBikeLeanAngle = 0.0f;
		}

		if(GetWheelFromId(BIKE_WHEEL_F))
			GetWheelFromId(BIKE_WHEEL_F)->SetSteerAngle(GetSteerAngle());
	}
}

bool CBike::WantsToBeAwake()
{
	Assertf(m_vehicleAiLod.GetDummyMode()!=VDM_SUPERDUMMY || !CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode,"Don't bother to call this virtual function if in SD LOD with physics turned off.");

	static dev_float sfBikeLeanStandSleepLimitMult = 0.025f;

	bool bWantsToBeAwake = m_nVehicleFlags.bDoorRatioHasChanged  || m_nBikeFlags.bGettingPickedUp || GetThrottle()!=0.0f;
	if(GetDriver())
	{
		if (GetDriver()->GetTransform().GetC().GetZf() < -0.5f || GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PreventBikeFromLeaning))
		{
			bWantsToBeAwake = true;
		}
	}
	const float fBikeOnStandLean = pHandling->GetBikeHandlingData()->m_fBikeOnStandLeanAngle;
	const float fBikeOnStandLeanAbs = rage::Abs(fBikeOnStandLean);
	const float fCurrentBikeLeanAngle = rage::Abs(m_fBikeLeanAngle);

	const bool bTestBikeOnStandLeanAngle = m_nBikeFlags.bOnSideStand || (GetDriver() && GetLayoutInfo() && GetLayoutInfo()->GetBikeLeansUnlessMoving());
	if(bTestBikeOnStandLeanAngle && ( (rage::Sign(m_fBikeLeanAngle) != rage::Sign(fBikeOnStandLean)) || !(fCurrentBikeLeanAngle > (fBikeOnStandLeanAbs - (fBikeOnStandLeanAbs*sfBikeLeanStandSleepLimitMult)) || fCurrentBikeLeanAngle < (fBikeOnStandLeanAbs + (fBikeOnStandLeanAbs*sfBikeLeanStandSleepLimitMult)))) )//If the bike is lent to within 10% of where it should be then we can sleep
		bWantsToBeAwake = true;
	if(Abs(m_fAnimLeanFwd) > 0.1f) //Let the player lean backwards and forwards on the bike to wake it up.
		bWantsToBeAwake = true;
	if(m_nBikeFlags.bJumpReleased || m_nBikeFlags.bJumpPressed)
		bWantsToBeAwake = true;

	return bWantsToBeAwake;
}

void CBike::Teleport(const Vector3& vecSetCoors, float fSetHeading, bool bCalledByPedTask, bool bTriggerPortalRescan, bool bCalledByPedTask2, bool bWarp, bool UNUSED_PARAM(bKeepRagdoll), bool UNUSED_PARAM(bResetPlants))
{
	CVehicle::Teleport(vecSetCoors, fSetHeading, bCalledByPedTask, bTriggerPortalRescan, bCalledByPedTask2, bWarp);

	UpdateStoredFwdVector(MAT34V_TO_MATRIX34(GetMatrix()));

	if(!GetDriver() && fSetHeading >= -PI && fSetHeading <= TWO_PI)
	{
		SetBikeOnSideStand(true, true);
	}
}

void CBike::KnockAllPedsOffBike()
{
	for(int iSeat = 0; iSeat < m_SeatManager.GetMaxSeats(); iSeat++)
	{
		CPed* pPed = m_SeatManager.GetPedInSeat(iSeat);
		if(pPed)
		{
			pPed->KnockPedOffVehicle(true);
		}
	}
}

bool CBike::IsConsideredStill(const Vector3& vVelocity, bool bIgnorePreventLeanTest) const
{
	static dev_float STILL_SPEED = 1.1f;
	static dev_float STILL_SPEED_IN_REVERSE = 0.9f;

	float fStillSpeedSqd = rage::square(STILL_SPEED);
	if (GetVehicleType() == VEHICLE_TYPE_BICYCLE)
	{
		if (!bIgnorePreventLeanTest && GetDriver() && GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_PreventBicycleFromLeaningOver))
		{
			return false;
		}

		if (GetLayoutInfo()->GetBicycleInfo())
		{
			fStillSpeedSqd = GetLayoutInfo()->GetBicycleInfo()->GetSpeedToTriggerBicycleLean();
		}
	}
	else if (GetLayoutInfo()->GetUseStillToSitTransition())
	{
		if ((GetThrottle() - GetBrake()) > 0.0f)
		{
			fStillSpeedSqd = square(CTaskMotionInAutomobile::ms_Tunables.m_MinVelStillStart);
		}
		else
		{
			fStillSpeedSqd = square(CTaskMotionInAutomobile::ms_Tunables.m_MinVelStillStop);
		}
	}

	static float sfMaxDeployRatioToBeConsideredStill = 0.5f;

	if( !HasGlider() &&
		GetSpecialFlightModeRatio() > sfMaxDeployRatioToBeConsideredStill )
	{
		return false;
	}

	float fFwdSpeed = DotProduct(VEC3V_TO_VECTOR3(GetTransform().GetB()), vVelocity);
	if(fFwdSpeed < 0.0f)
	{
		fStillSpeedSqd = rage::square(STILL_SPEED_IN_REVERSE);
	}

	const float fSpeedSqd = Abs(vVelocity.Mag2()); 
	return fSpeedSqd < fStillSpeedSqd ? true : false;
}

void CBike::CleanupMissionState()
{
	CVehicle::CleanupMissionState();

	m_nBikeFlags.bWaterTight = FALSE;
}

#if __BANK
void CBike::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Bike");
		bank.AddToggle("Enable new lean method",&ms_bNewLeanMethod);
		bank.AddToggle("Lean bike when doing Burnouts", &ms_bLeanBikeWhenDoingBurnout);

		bank.AddToggle("Apply lean torque about wheels",&sbApplyLeanTorqueAboutWheels);
		bank.AddToggle("Apply torques internally",&sbApplyLeanTorqueInternal);

		bank.AddSlider("Max unstable time before knockoff",&ms_fMaxTimeUnstableBeforeKnockOff,0.0f,10.0f,0.1f);

		const Vector3 vMax(50.0f,50.0f,50.0f);
		const Vector3 vDel(0.001f,0.001f,0.001f);
		bank.PushGroup("Ang inertia raise", false);
			bank.AddSlider("Inertia vel mult V",&svAngInertiaRaiseFactorV,VEC3_ZERO,vMax,vDel);
			bank.AddSlider("Inertia vel mult C",&svAngInertiaRaiseFactorC,VEC3_ZERO,100.0f*vMax,100.0f*vDel);
		bank.PopGroup();
		bank.PushGroup("Damping");
			bank.AddSlider("BIKE_NEW_ROT_DAMP_C",&BIKE_NEW_ROT_DAMP_C,VEC3_ZERO,vMax,vDel);
			bank.AddSlider("BIKE_NEW_ROT_DAMP_V",&BIKE_NEW_ROT_DAMP_V,VEC3_ZERO,vMax,vDel);
			bank.AddSlider("BIKE_NEW_ROT_DAMP_V2",&BIKE_NEW_ROT_DAMP_V2,VEC3_ZERO,vMax,vDel);
			bank.AddSlider("BIKE_NEW_ROT_DAMP_V2_IN_AIR",&BIKE_NEW_ROT_DAMP_V2_IN_AIR,VEC3_ZERO,vMax,vDel);

            bank.AddSlider("BIKE_NEW_LINEAR_DAMP_C",&BIKE_NEW_LINEAR_DAMP_C,VEC3_ZERO,vMax,vDel);
            bank.AddSlider("BIKE_NEW_LINEAR_DAMP_V",&BIKE_NEW_LINEAR_DAMP_V,VEC3_ZERO,vMax,vDel);
		bank.PopGroup();

		bank.PushGroup("Skeleton Fixup");
			bank.AddToggle("Enable Skeleton Fixup",&ms_bEnableSkeletonFixup);
			bank.AddSlider("Max Rotation", &ms_fSkeletonFixupMaxRot,0.0f,5.0f,0.01f);
			bank.AddSlider("Max Translation", &ms_fSkeletonFixupMaxTrans,0.0f,5.0f,0.01f);
			bank.AddSlider("Max Rotation Change", &ms_fSkeletonFixupMaxRotChange,0.0f,5.0f,0.01f);
			bank.AddSlider("Max Translation Change", &ms_fSkeletonFixupMaxTransChange,0.0f,5.0f,0.01f);
		bank.PopGroup();

		bank.PushGroup("Second New Lean");
			bank.AddToggle("Enable Second New Lean", &ms_bSecondNewLeanMethod);
			bank.AddSlider("Use Z Axis cosine", &ms_fSecondNewLeanZAxisCosine, 0.0f, 1.0f, 0.05f);
			bank.AddSlider("Use Ground Normal cosine", &ms_fSecondNewLeanGroundNormalCosine, 0.0f, 1.0f, 0.05f);
			bank.PushGroup("Max Angular Velocities");
				bank.AddSlider("Ground - Low Speed", &ms_fSecondNewLeanMaxAngularVelocityGroundSlow, 0.0f, 30.0f, 0.1f);
				bank.AddSlider("Ground - High Speed", &ms_fSecondNewLeanMaxAngularVelocityGroundFast, 0.0f, 30.0f, 0.1f);
				bank.AddSlider("Air - Low Speed", &ms_fSecondNewLeanMaxAngularVelocityAirSlow, 0.0f, 30.0f, 0.1f);
				bank.AddSlider("Air - High Speed", &ms_fSecondNewLeanMaxAngularVelocityAirFast, 0.0f, 30.0f, 0.1f);
				bank.AddSlider("Pickup", &ms_fSecondNewLeanMaxAngularVelocityPickup, 0.0f, 30.0f, 0.1f);
			bank.PopGroup();
		bank.PopGroup();

		bank.PushGroup("Stability");
		bank.AddSlider("Max Ground Tilt Angle Sine",&ms_fMaxGroundTiltAngleSine,0.0f,1.0f,0.05f);
		bank.AddSlider("Min Absolute Tilt Angle Sine",&ms_fMinAbsoluteTiltAngleSineForKnockoff,0.0f,1.0f,0.05f);
		bank.PopGroup();
#if __DEV
		bank.AddToggle("Display lean vectors", &sbBikeDebugDisplayLeanVectors);
#endif
		bank.AddButton("Knock focus ped off bike", &CBike::KnockFocusPedOffBike);
	bank.PopGroup();
}


// Knock the focus ped off of the bike through RAG.  Technically should work on quads, too.
void CBike::KnockFocusPedOffBike()
{
	// Grab the focus ped.
	CPed* pFocusPed = CPedDebugVisualiserMenu::GetFocusPed();

	if (pFocusPed)
	{
		if (pFocusPed->GetMyVehicle() && pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			// Same as commands_ped.cpp's CommandKnockPedOffBike()
			// - not using the above KnockAllPedsOffBike() because technically
			// this function doesn't require the ped's vehicle to be of type bike and so is more general.
			CVehicle* pVehicle = pFocusPed->GetMyVehicle();
			if (pVehicle->GetVehicleModelInfo()->GetCanPassengersBeKnockedOff())
			{
				pFocusPed->KnockPedOffVehicle(true);
			}
		}
	}
}


#endif
