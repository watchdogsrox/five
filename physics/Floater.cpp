
#include "grcore/debugdraw.h"
#include "fragmentnm/manager.h"

// Game headers
#include "animation/animbones.h" 
#include "camera/viewports/ViewportManager.h"
#include "control/replay/replay.h"
#include "Game/ModelIndices.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/ped.h"
#include "peds/Ped.h"
#include "peds/PedFactory.h"
#include "peds/PedIntelligence.h"
#include "phbound/boundcomposite.h"
#include "pharticulated/articulatedbody.h"
#include "physics/floater.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/physics_channel.h"
#include "physics/Tunable.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/River.h"
#include "renderer/water.h"
#include "script/script_cars_and_peds.h"
#include "scene/physical.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "task/Motion/TaskMotionBase.h"
#include "task/Physics/TaskNMRiverRapids.h"
#include "vehicleAi/Task/TaskVehicleGoToSubmarine.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/boat.h"
#include "vehicles/heli.h"
#include "vehicles/VehicleFactory.h"
#include "vfx/Systems/VfxPed.h"
#include "vfx/Systems/VfxVehicle.h"
#include "vfx/Systems/VfxWater.h"
#include "vfx/VfxHelper.h"
#include "phbound/bound.h"
#include "Peds/pedDefines.h"
#include "profile/profiler.h"

PHYSICS_OPTIMISATIONS()

//#include "System/FindSize.h"
//FindSize(CBuoyancy); 368

// PURPOSE: Define the fractional error allowed on the matrix axes for orthonormality when processing low LOD buoyancy.
#define REJUVENATE_ERROR 0.01f

// Statics ////////////////////////////////////////////////////////////////
#if __BANK
bool CBuoyancy::ms_bDrawBuoyancyTests = false;
bool CBuoyancy::ms_bDrawBuoyancySampleSpheres = false;
bool CBuoyancy::ms_bDisplayBuoyancyForces = false;
bool CBuoyancy::ms_bDebugDisplaySubmergedLevels = false;
bool CBuoyancy::ms_bWriteBuoyancyForceToTTY = false;
s32 CBuoyancy::ms_nSampleToDisplay = -1;

// Low LOD buoyancy.
bool CBuoyancy::ms_bIndicateLowLODBuoyancy = false;

// Disable forces for debugging.
bool CBuoyancy::ms_bDisableBuoyancyForceXY = false;
bool CBuoyancy::ms_bDisableLiftForce = false;
bool CBuoyancy::ms_bDisableKeelForce = false;
bool CBuoyancy::ms_bDisableFlowInducedForce = false;
bool CBuoyancy::ms_bDisableDragForce = false;
bool CBuoyancy::ms_bDisableSurfaceStickForce = false;
bool CBuoyancy::ms_bDisablePedWeightBeltForce = false;

bool CBuoyancy::ms_bVisualiseRiverFlowField = false;
bool CBuoyancy::ms_bDebugDrawCrossSection = false;
bool CBuoyancy::ms_bRiverBoundDebugDraw = false;
float CBuoyancy::ms_fGridSpacing = 0.5f;
float CBuoyancy::ms_fHeightOffset = 0.1f;
float CBuoyancy::ms_fScaleFactor = 5.0f;
int CBuoyancy::ms_nGridSizeX = 10;
int CBuoyancy::ms_nGridSizeY = 10;
#endif // __BANK

float CBuoyancy::ms_fCarDragCoefficient = 5.0f;
float CBuoyancy::ms_fBikeDragCoefficient = 0.05f;
float CBuoyancy::ms_fDragCoefficient = 120.0f;
float CBuoyancy::ms_fPedDragCoefficient = 30.0f;
float CBuoyancy::ms_fProjectileDragCoefficient = 5.0f;
float CBuoyancy::ms_fFlowVelocityScaleFactor = 7.5;
float CBuoyancy::ms_fVehicleMaximumSpeedToApplyRiverForces = 8.0f;

// Keel force params:
float CBuoyancy::ms_fFullKeelForceAtRudderLimit = 0.5f;
float CBuoyancy::ms_fKeelDragMult = 0.4f;
float CBuoyancy::ms_fKeelForceFactorSteerMultKeelSpheres = 0.2f;
float CBuoyancy::ms_fMinKeelForceFactor = 0.2f;
float CBuoyancy::ms_fKeelForceThrottleThreshold = 0.1f;
float CBuoyancy::ms_fMaxKeelRudderExclusionFraction = 0.1f;

float CBuoyancy::ms_fBoatAnchorLodDistance = 0.0f;
float CBuoyancy::ms_fObjectLodDistance = 50.0f;

PF_PAGE(GTA_Buoyancy, "Gta Buoyancy");
PF_GROUP(Buoyancy_forces);
PF_LINK(GTA_Buoyancy, Buoyancy_forces);
PF_GROUP(Buoyancy_timers);
PF_LINK(GTA_Buoyancy, Buoyancy_timers);
PF_GROUP(Buoyancy_general);
PF_LINK(GTA_Buoyancy, Buoyancy_general);


PF_VALUE_INT(NumWaterSamples, Buoyancy_general);

PF_TIMER(ProcessBuoyancy, Buoyancy_timers);
PF_TIMER(ProcessBuoyancyTotal, Buoyancy_timers);
PF_TIMER(ComputeBuoyancyForce, Buoyancy_timers);
PF_TIMER(ComputeLiftForce, Buoyancy_timers);
PF_TIMER(ComputeKeelForce, Buoyancy_timers);
PF_TIMER(ComputeDragForce, Buoyancy_timers);
PF_TIMER(ComputeDampingForce, Buoyancy_timers);
PF_TIMER(ComputeStickToSurfaceForce, Buoyancy_timers);
PF_TIMER(ComputeCrossSection, Buoyancy_timers);
EXT_PF_TIMER(ProcessLowLodBuoyancyTimer);

PF_VALUE_FLOAT(BuoyancyForce, Buoyancy_forces);
PF_VALUE_FLOAT(LiftForce, Buoyancy_forces);
PF_VALUE_FLOAT(KeelForce, Buoyancy_forces);
PF_VALUE_FLOAT(FlowDragForce, Buoyancy_forces);
PF_VALUE_FLOAT(BasicDragForce, Buoyancy_forces);
///////////////////////////////////////////////////////////////////////////

#if __ASSERT
#define GENERIC_FORCE_CHECK(msg, vForce) \
Displayf("***** %s EXCEEDS SAFE RANGE *****", #msg); \
CBaseModelInfo* pModelInfo = pParent->GetBaseModelInfo(); \
if(pModelInfo) \
{ \
	Displayf("Model index: %d (%s)", pParent->GetModelIndex(), pModelInfo->GetModelName()); \
} \
if(m_WaterTestHelper.GetRiverHitStored()) \
{ \
	Displayf("Sample in river."); \
} \
else \
{ \
	Displayf("Sample not in river."); \
} \
Displayf("Force=(%5.3f,%5.3f,%5.3f) (magnitude=%5.3f)", vForce.x,vForce.y,vForce.z, vForce.Mag());
#endif // __ASSERT

#if DEBUG_BUOYANCY
// Call this once for each buoyancy sample sphere to initialise variables.
#define DEBUG_SAMPLE_BUOYANCY_FORCES_INIT(nSampleId) \
if(ms_nSampleToDisplay==-1 || ms_nSampleToDisplay==nSampleId) \
{ \
	if(ms_bDisplayBuoyancyForces) \
	{ \
		m_forceDebug.m_nNumTextLines = 0; \
		if(ms_bDrawBuoyancySampleSpheres) ++m_forceDebug.m_nNumTextLines; \
		/* Display the different buoyancy forces applied to this sample. */ \
		sprintf(m_forceDebug.m_zForceDebugText, "Sample: %u", nSampleId); \
		grcDebugDraw::Text(vTestPointWorld, m_forceDebug.m_forceDebugColour, 0, m_forceDebug.m_nNumTextLines*grcDebugDraw::GetScreenSpaceTextHeight(), m_forceDebug.m_zForceDebugText); \
		++m_forceDebug.m_nNumTextLines; \
	} \
}
#else // DEBUG_BUOYANCY
#define DEBUG_SAMPLE_BUOYANCY_FORCES_INIT(nSampleId)
#endif // DEBUG_BUOYANCY

#if DEBUG_BUOYANCY
// After calling DEBUG_SAMPLE_BUOYANCY_FORCES_INIT(), use this to display each of the forces applied to a buoyancy sample sphere.
#define DEBUG_SAMPLE_BUOYANCY_FORCE(nSampleId, msg, vForce) \
if(ms_nSampleToDisplay==-1 || ms_nSampleToDisplay==nSampleId) \
{ \
	if(ms_bDisplayBuoyancyForces) \
	{ \
		sprintf(m_forceDebug.m_zForceDebugText, "%s: (%5.3f,%5.3f,%5.3f) {%5.3f}", #msg, vForce.x, vForce.y, vForce.z, vForce.Mag()); \
		grcDebugDraw::Text(vTestPointWorld, m_forceDebug.m_forceDebugColour, 0, m_forceDebug.m_nNumTextLines*grcDebugDraw::GetScreenSpaceTextHeight(), m_forceDebug.m_zForceDebugText); \
		++m_forceDebug.m_nNumTextLines; \
	} \
	if(ms_bWriteBuoyancyForceToTTY) \
	{ \
		Displayf("[CEntity: 0x%p] Sample %u: %s: (%5.3f,%5.3f,%5.3f) {%5.3f}", \
			pParent, nSampleId, #msg, vForce.x, vForce.y, vForce.z, vForce.Mag()); \
	} \
}
#else // DEBUG_BUOYANCY
#define DEBUG_SAMPLE_BUOYANCY_FORCE(nSampleId, msg, vForce)
#endif // DEBUG_BUOYANCY

// To make a change to the way we compute the depth of samples in rivers when the river poly is not horizontal safer, define a region
// around one of the big waterfalls where we allow the change to kick-in.
const Vector3 vWaterFallPosition(-540.0f, 4420.9f, 20.0f);
const float fWaterFallRadius = 20.0f;

CBuoyancy::CBuoyancy()
: m_fSubmergedLevel(0.0f),
m_fLowLODDraughtOffset(0.0f),
m_fFlowVelocityScaleFactor(ms_fFlowVelocityScaleFactor),
m_nNumComponentsWithSamples(0)
{
	m_waterLevelStatus = NOT_IN_WATER;

	m_fForceMult = 1.0f;
	m_fSampleSubmergeLevel = NULL;
	m_nNumInSampleLevelArray = 0;
	m_pBuoyancyInfoOverride = NULL;
	m_buoyancyFlags.bShouldStickToWaterSurface = false;
	m_buoyancyFlags.bPedWeightBeltActive = false;

	m_buoyancyFlags.bLowLodBuoyancyMode = false;
	m_buoyancyFlags.bWasActiveWhenLodded = false;
	m_buoyancyFlags.bScriptLowLodOverride = false;
	m_buoyancyFlags.bUseWidestRiverBoundTolerance = false;
	m_buoyancyFlags.bBuoyancyInfoUpToDate = false;
	m_buoyancyFlags.bReduceLateralBuoyancyForce = false;
	
	m_vLowLodVelocityXYZLowLodTimerW.Set(0.0f, 0.0f, 0.0f);
	SetTimeInLowLodMode(0.0f);
}

CBuoyancy::~CBuoyancy()
{
	if(m_fSampleSubmergeLevel)
	{
		delete[] m_fSampleSubmergeLevel;
	}
}

dev_float TEST_PUSH_WATER_DOWN_FROM_LIFT_SPEED_MULT = 0.1f;
dev_float TEST_PUSH_WATER_DOWN_FROM_LIFT_SPEED_LIMIT = 2.0f;
dev_float TEST_PUSH_WATER_DOWN_FROM_LIFT_APPLY_MULT = 0.001f;
dev_float MAX_SPEED_FOR_DRAG = 30.0f;

dev_bool sbTestBoatSamplePosFix = true;
dev_float sfWaterSamplePlaneMult = 10.0f;
dev_float sfWaterSampleCruiseMult = 0.95f;

//
bool CBuoyancy::Process(CPhysical* pParent, float fTimeStep, bool bArticulatedBody, bool lastTimeSlice, float* pBuoyancyAccel)
{
	// WARNING!! IF YOU ADD ANY EXTRA RETURN STATEMENTS TO THIS FUNCTION, PLEASE REMEMBER TO STOP THIS TIMER FIRST.
	PF_START(ProcessBuoyancy);

	Assert(pParent);

	// This flag will let us know whether such info as "submerged level", "avg submerge level", etc. are actually
	// up-to-date for this entity.
	m_buoyancyFlags.bBuoyancyInfoUpToDate = false;

	if(!pParent || !pParent->GetCurrentPhysicsInst())
	{
		PF_STOP(ProcessBuoyancy);
		return false;
	}

	if(!pParent->GetCurrentPhysicsInst() || !pParent->GetCurrentPhysicsInst()->IsInLevel() || !pParent->GetCollider())
	{
		PF_STOP(ProcessBuoyancy);
		return false;
	}

	// Don't process buoyancy for objects which have a basic attachment (the attach parent will be processed).
	if(pParent->GetIsAttached() && !pParent->GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION)
		&& (pParent->GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_BASIC
		||pParent->GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD))
	{
		PF_STOP(ProcessBuoyancy);
		return false;
	}

	CBuoyancyInfo* pBuoyancyInfo = GetBuoyancyInfo(pParent);

	//Assert(pBuoyancyInfo); // cloth ends up with no buoyancy info
	if(pBuoyancyInfo == NULL)
	{
		PF_STOP(ProcessBuoyancy);
		return false;
	}

	// No point going any further if buoyancy has been turned off for this instance.
	if(pParent && pParent->GetPhysArch() && pParent->GetPhysArch()->GetBuoyancyFactor() < SMALL_FLOAT)
	{
		PF_STOP(ProcessBuoyancy);
		return false;
	}

	SelectDragCoefficient(pParent);

	// Cache some frequently used data.
	phInst* pInst = pParent->GetCurrentPhysicsInst();
	phBound* pBound = NULL;

	// Useful for some verification when applying forces, etc.
	int nNumBounds = 0;
	if(pInst->GetArchetype())
	{
		pBound = pInst->GetArchetype()->GetBound();
		if(pBound)
		{
			// We at least have one bound if we are here...
			nNumBounds = 1;
			// ... if the bound is composite, we might have more.
			if(pBound->GetType()==phBound::COMPOSITE)
			{
				nNumBounds = static_cast<phBoundComposite*>(pBound)->GetNumBounds();
			}
		}
	}

	if(m_nNumInSampleLevelArray < pBuoyancyInfo->m_nNumWaterSamples)
	{
		// Our buoyancy info has changed, so water samples number no longer matches the sample submerge level array
		// Delete the sample submerge level array so it can be re created
		delete[] m_fSampleSubmergeLevel;
		m_fSampleSubmergeLevel = NULL;
		m_nNumInSampleLevelArray = 0;

		// Signal that we need to redefine a mapping between component index and the corresponding force accumulator
		// as this has probably changed.
		m_nNumComponentsWithSamples = 0;
	}

	// We also need to redefine the mapping between component index and the corresponding force accumulator if
	// we changed physics inst since it was set up (e.g. ped switches from animated to ragdoll in water).
	if(pParent && pInst && pInst!=m_pInstForComponentMap)
	{
		 m_nNumComponentsWithSamples = 0;
	}

	if(m_fSampleSubmergeLevel == NULL)
	{
		m_fSampleSubmergeLevel = rage_new float[pBuoyancyInfo->m_nNumWaterSamples];
		m_nNumInSampleLevelArray = pBuoyancyInfo->m_nNumWaterSamples;
		if(m_fSampleSubmergeLevel == NULL)
		{
			Assertf(false, "Error allocating %d water samples.", pBuoyancyInfo->m_nNumWaterSamples);
			PF_STOP(ProcessBuoyancy);
			return false;
		}

		// initialise the water sample levels
		for (int i=0; i<pBuoyancyInfo->m_nNumWaterSamples; i++)
		{
			m_fSampleSubmergeLevel[i] = 0.0f;
		}
	}

	Assert(m_nNumInSampleLevelArray >= pBuoyancyInfo->m_nNumWaterSamples);

	if(pBuoyancyAccel)
		*pBuoyancyAccel = 0.0f;

	Vector3 vTotalBuoyancyForce;
	vTotalBuoyancyForce.Zero();

	bool bInWater = false;
	bool bUnderWater = true;
	bool bInRiver = m_WaterTestHelper.GetRiverHitStored();

	Vector3 vTestPointWorld;
	Vector3 vecWaterNormal;
	float fWaterLevel;
	float fWaterSpeedVert;
	int nComponent = 0;

	// Used to stop peds counting as in water until their bodies are below a certain height during the transition into water
	//bool bOverridePedInWater = false;

	CVehicle* pVehicle = NULL;
	CBoatHandlingData* pBoatHandling = NULL;
	float dragReduction = 0.0f;

	if(pParent->GetIsTypeVehicle() && ((CVehicle*)pParent)->GetVehicleType()==VEHICLE_TYPE_BOAT)
	{
		pVehicle = static_cast<CVehicle*>(pParent);
		pBoatHandling = pVehicle->pHandling->GetBoatHandlingData();

		if( pVehicle->GetDriver() && 
			pVehicle->GetDriver()->GetPlayerInfo() )
		{
			static dev_float maxDragReduction = 0.3f;
			dragReduction = pVehicle->GetSlipStreamEffect() * maxDragReduction;
		}
	}

	CSeaPlaneHandlingData* pSeaplaneHandling = NULL;
	if(pParent->GetIsTypeVehicle())
	{
		pSeaplaneHandling = static_cast<CVehicle*>(pParent)->pHandling->GetSeaPlaneHandlingData();
	}

	if( pParent->GetIsTypeVehicle() && ((CVehicle*)pParent)->InheritsFromAmphibiousAutomobile() )
	{
		pVehicle = static_cast<CVehicle*>(pParent);
		pBoatHandling = static_cast<CVehicle*>(pParent)->pHandling->GetBoatHandlingData();

		if( pVehicle->GetDriver() &&
			pVehicle->GetDriver()->GetPlayerInfo() )
		{
			static dev_float maxDragReduction = 0.3f;
			dragReduction = pVehicle->GetSlipStreamEffect() * maxDragReduction;
		}
	}

	float fMass = pParent->GetMass();
	CPed* pPed = pParent->GetIsTypePed() ? static_cast<CPed*>(pParent) : NULL;
	if(pPed && pPed->GetRagdollState()==RAGDOLL_STATE_PHYS)
	{
		fMass = pPed->GetRagdollInst()->GetTypePhysics()->GetTotalUndamagedMass();
	}
	else if(pParent->GetIsTypeObject() && pParent->GetFragInst())
	{
		fMass = pParent->GetFragInst()->GetTypePhysics()->GetTotalUndamagedMass();
	}

	// vfx - decide whether to process
	bool isAnimatedPed = pPed && !bArticulatedBody;
	bool processFx = true;
	if(isAnimatedPed && !lastTimeSlice)
	{
		// don't process fx if this is an animated ped and not the last time slice
		processFx = false;
	}

//	bool bIsSwimmingPed = pPed && pPed->GetIsSwimming();
/*
	// Living fish also count as swimming peds for the purposes of excluding them from redundant force calculations.
	if(pPed)
	{
		CTaskMotionBase* pMotion = pPed->GetCurrentMotionTask();
		if(pMotion && pMotion->GetTaskType() == CTaskTypes::TASK_ON_FOOT_FISH)
		{
			bIsSwimmingPed = true;
		}
	}
*/
	float fWaterHeightNoWaves;
	Vector3 vecParentPos = VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition());
	WaterTestResultType waterTestResult = m_WaterTestHelper.GetWaterLevelIncludingRiversNoWaves(vecParentPos, &fWaterHeightNoWaves, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pParent); 
	if( waterTestResult == WATERTEST_TYPE_NONE )
	{
		m_waterLevelStatus = NOT_IN_WATER;
		PF_STOP(ProcessBuoyancy);
		return false;
	}

	if( waterTestResult == WATERTEST_TYPE_RIVER &&
		pBoatHandling )
	{
		spdAABB aabbObj;
		pParent->GetAABB( aabbObj );

		static dev_float sfWaterHeightTollerance = 0.5f;

		if( ( fWaterHeightNoWaves + sfWaterHeightTollerance ) < aabbObj.GetMin().GetZf() )
		{
			m_waterLevelStatus = NOT_IN_WATER;
			return false;
		}
	}

	int iSampleStart = 0;
	int iSampleEnd = pBuoyancyInfo->m_nNumWaterSamples;
	if(pPed)
	{
		if(pPed->GetUsingRagdoll())
		{
			iSampleStart = WS_PED_RAGDOLL_SAMPLE_START;

			// Currently, with a lower-LOD human or an animal just use the number of samples needed.  
			// TODO - come up with something that handles other ragdoll types humans better.
			if(bArticulatedBody && pParent->GetFragInst() && pBound)
			{
				phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pBound);
				int numSamples = iSampleEnd - iSampleStart;
				if(pCompositeBound->GetNumBounds() < numSamples)
				{
					iSampleEnd = iSampleStart + pCompositeBound->GetNumBounds();
				}
			}
		}
		else
		{
			iSampleStart = WS_PED_ANIMATED_SAMPLE_START;
			iSampleEnd = WS_PED_RAGDOLL_SAMPLE_START;
		}
	}
    else if(pParent->GetIsTypeVehicle())
    {
        float fVehicleSpeed = pParent->GetVelocity().Mag();
        if(fVehicleSpeed < ms_fVehicleMaximumSpeedToApplyRiverForces)
        {
            m_fFlowVelocityScaleFactor = ((ms_fVehicleMaximumSpeedToApplyRiverForces - fVehicleSpeed)/ms_fVehicleMaximumSpeedToApplyRiverForces) * ms_fFlowVelocityScaleFactor;
        }
        else
        {
            m_fFlowVelocityScaleFactor = 0.0f;
        }
    }

	if(!physicsVerifyf(iSampleStart < pBuoyancyInfo->m_nNumWaterSamples,"Ped has invalid buoyancy samples: iSampleStart=%d, iSampleEnd=%d, num water samples=%d",
		iSampleStart, iSampleEnd, pBuoyancyInfo->m_nNumWaterSamples))
	{
		iSampleStart = pBuoyancyInfo->m_nNumWaterSamples;
	}

	if(!physicsVerifyf(iSampleEnd <= pBuoyancyInfo->m_nNumWaterSamples,"Ped has invalid buoyancy samples: iSampleStart=%d, iSampleEnd=%d, num water samples=%d",
		iSampleStart, iSampleEnd, pBuoyancyInfo->m_nNumWaterSamples))
	{
		iSampleEnd = pBuoyancyInfo->m_nNumWaterSamples;
	}

	Assertf(iSampleEnd > 0 && iSampleEnd<=MAX_WATER_SAMPLES, "Invalid number of water samples %i, pBuoyancyInfo->m_nNumWaterSamples : %i, m_WaterSamples : %p", iSampleEnd, pBuoyancyInfo->m_nNumWaterSamples, pBuoyancyInfo->m_WaterSamples);

	TUNE_GROUP_BOOL(WATER_BUG, FORCE_NO_SAMPLES_FIX, true);
	if (iSampleEnd == 0 && FORCE_NO_SAMPLES_FIX)
	{
		PF_STOP(ProcessBuoyancy);
		return false;
	}

	// vfx - initialise data
	VfxWaterSampleData_s vfxWaterSampleData[MAX_WATER_SAMPLES];
	if (processFx)
	{
		for (int i=0; i<MAX_WATER_SAMPLES; i++)
		{
			if (i<m_nNumInSampleLevelArray)
			{
				vfxWaterSampleData[i].prevLevel = m_fSampleSubmergeLevel[i];
			}
			else
			{
				vfxWaterSampleData[i].prevLevel = 0.0f;
			}

			vfxWaterSampleData[i].isInWater = false;
		}
	}

	m_fAbsWaterLevel = -10000;
	m_fAvgAbsWaterLevel = 0.0f;
	m_fSubmergedLevel = 0.0f;
	
	float fCrossSectionalLength = ComputeCrossSectionalLengthForDrag(pParent);

#if DEBUG_BUOYANCY
    m_fTotalBuoyancyForce = 0.0f;
    m_fTotalLiftForce = 0.0f;
    m_fTotalKeelForce = 0.0f;
    m_fTotalFlowDragForce = 0.0f;
    m_fTotalBasicDragForce = 0.0f;
#endif // DEBUG_BUOYANCY

#if __WIN32PC
	m_fKeelSideForce = 0.0f;
#endif // __WIN32PC




	// Create the force accumulators. We need enough for each component which has a water sample.
	// Define a mapping between component index and the corresponding force accumulator if we haven't already done so.
	if(m_nNumComponentsWithSamples == 0)
	{
		m_componentIndexToForceAccumMap.Reset();
		for(int i = iSampleStart; i < iSampleEnd; ++i)
		{
			CWaterSample& sample = pBuoyancyInfo->m_WaterSamples[i];

			u32 nSampleComponent = sample.m_nComponent;
			if(!m_componentIndexToForceAccumMap.Access(nSampleComponent) && nSampleComponent<nNumBounds)
			{
				m_componentIndexToForceAccumMap.Insert(nSampleComponent, m_nNumComponentsWithSamples);
				++m_nNumComponentsWithSamples;
			}
		}
		m_pInstForComponentMap = pInst;
	}
	Assert(m_nNumComponentsWithSamples != 0);
	SForceAccumulator* forceAccumulators = Alloca(SForceAccumulator, m_nNumComponentsWithSamples);
	Assert(forceAccumulators);
	// Zero the accumulators.
	for(int i = 0; i < m_nNumComponentsWithSamples; ++i)
	{
		forceAccumulators[i].Clear();
	}

	Matrix34 parentMat = MAT34V_TO_MATRIX34(pInst->GetMatrix());

/// ///////////////////////////////////////////// LOOP OVER EACH SAMPLE /////////////////////////////////////////////////////////////////////////

	// Process each individual water sample.
	u32 nSamplesUsedForSubmergeLevel = 0;
	for(int iSample=iSampleStart; iSample < iSampleEnd; iSample++)
	{
		// Cache some frequently used values.
		CWaterSample& sample = pBuoyancyInfo->m_WaterSamples[iSample];
		float fSampleSize = sample.m_fSize;
		float fSampleBuoyancyMult = sample.m_fBuoyancyMult;
		int nSampleComponent = sample.m_nComponent;

		// VFX - Skip this sample.
		if(fSampleSize==0.0f)
		{
			vfxWaterSampleData[iSample].maxLevel = 0.0f;
			continue;
		}

		// If this sample is for a part of the object which has broken off then just ignore it.
		if(pParent->GetFragInst() && pParent->GetFragInst()->GetChildBroken(nSampleComponent))
		{
			continue;
		}

		m_fSampleSubmergeLevel[iSample] = 0.0f;

		// The sample offset is computed relative to the bounding box min coords. Transform it accordingly.
		Vector3 vLocalPosition = sample.m_vSampleOffset;
		if(bArticulatedBody && pParent->GetFragInst() && pBound)
		{
			phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pBound);
			nComponent = nSampleComponent;

			// set mass to the current part
			if(pPed)
			{
				phArticulatedBody *body = ((phArticulatedCollider*)pParent->GetCollider())->GetBody();
				Assert(body);
				fMass = body->GetMass(nComponent).Getf();
			}
			else if(pParent->GetIsTypeObject() && pParent->GetFragInst())
			{
				fMass = pParent->GetFragInst()->GetTypePhysics()->GetChild(nComponent)->GetUndamagedMass();
			}

			if(Verifyf(nComponent < pCompositeBound->GetNumBounds(),"Component in water sample does not exist in physics bound"))
			{
				RCC_MATRIX34(pCompositeBound->GetCurrentMatrix(nComponent)).Transform(vLocalPosition);
			}
			else
			{
				// Something has gone wrong, probably using a lower LOD version of this object which has
				// less parts than when the samples were set up. Just ignore it to avoid a crash.
				continue;
			}
		}
		else if(pBound && pBound->GetType() == phBound::COMPOSITE)
		{
			// For non-frag objects we just used the biggest component to generate samples.
			phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pBound);
			nComponent = nSampleComponent;
			if(Verifyf(nComponent < pCompositeBound->GetNumBounds(), "Component in water sample does not exist in physics bound"))
			{
				RCC_MATRIX34(pCompositeBound->GetCurrentMatrix(nComponent)).Transform(vLocalPosition);
			}
		}
		parentMat.Transform(vLocalPosition, vTestPointWorld);

		// Skip this sample if we are in a river and it is too high above the hit position returned for the river surface at the entity centre.
		if(bInRiver)
		{
			if((vTestPointWorld - vWaterFallPosition).Mag2() < rage::square(fWaterFallRadius) && 
				( !pParent->GetIsTypeVehicle() || static_cast< CVehicle* >( pParent )->GetSpecialFlightModeRatio() == 0.0f ) )
			{
				TUNE_FLOAT(fMaxHeightAboveRiverForWaterSample, 1.0f, 0.0f, 20.0f, 1.0f);
				if(vTestPointWorld.z - m_WaterTestHelper.m_vLastRiverHitPos.z > fMaxHeightAboveRiverForWaterSample)
				{
					continue;
				}
			}
		}

		DEBUG_SAMPLE_BUOYANCY_FORCES_INIT(iSample);

		vfxWaterSampleData[iSample].vTestPos = RCC_VEC3V(vTestPointWorld);
		vfxWaterSampleData[iSample].maxLevel = fSampleSize;

		////////////// COMPUTE VELOCITY OF THIS SAMPLE RELATIVE TO THE LOCAL VELOCITY OF THE WATER ////////////

		Vector2 vFlow(0.0f, 0.0f);
		Vector3 vFlowVelocity(VEC3_ZERO);
		if(bInRiver)
		{
			// Extract the flow vector from the river texture and use it to compute a force which will
			// push the object along the river.
			River::GetRiverFlowFromPosition(VECTOR3_TO_VEC3V(vTestPointWorld), vFlow);
			vFlow.Scale(1.0f/River::GetRiverPushForce());
			vFlowVelocity = Vector3(vFlow.x, vFlow.y, 0.0f);
			vFlowVelocity.Scale(m_fFlowVelocityScaleFactor);
		}

		if(bInRiver)
		{
			// To make ragdolling peds move down the river faster in rapids and waterfalls, up the perceived velocity
			// of the river in these regions.
			const float fRapidSpeed = CTaskNMRiverRapids::sm_Tunables.m_fMinRiverFlowForRapids;
			if(pPed)
			{
				Assert(pPed);
				const CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
				bool bPedRunningRapidsTask = false;
				if(pPedIntelligence && pPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_RIVER_RAPIDS))
				{
					bPedRunningRapidsTask = true;
				}
				if(bPedRunningRapidsTask && vFlowVelocity.Mag2() > fRapidSpeed*fRapidSpeed)
				{
					TUNE_FLOAT(fMaxPedRiverFlowSpeedScalarInRapids, 3.0f, 0.0f, 10.0f, 0.1f);
					TUNE_INT(nDurationOfLerp, 2000, 1, 10000, 100);

					// How long have we been in the rapids? Want to ramp up the amount we scale the flow
					// speed over a short amount of time.
					u32 nTimeInRapids = fwTimer::GetTimeInMilliseconds() - pPed->GetRiverRapidsTimer();
					nTimeInRapids = rage::Min(nTimeInRapids, (u32)nDurationOfLerp);
					float fLerpFraction = float(nTimeInRapids)/float(nDurationOfLerp);
					float fPedRiverFlowSpeedScalarInRapids = ((fMaxPedRiverFlowSpeedScalarInRapids-1.0f) * fLerpFraction) + 1.0f;

					vFlowVelocity.Scale(fPedRiverFlowSpeedScalarInRapids);
#if __BANK
					TUNE_BOOL(INDICATE_PEDS_IN_RAPIDS, false);
					if(INDICATE_PEDS_IN_RAPIDS)
					{
						grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pParent->GetMatrix().d()), 0.5f, Color32(1.0f, 0.0f, 0.0f, 0.1f), true);
						char zText[100];
						sprintf(zText, "Flow scale: %5.3f", fPedRiverFlowSpeedScalarInRapids);
						grcDebugDraw::Text(VEC3V_TO_VECTOR3(pPed->GetMatrix().d())+Vector3(0.0f,0.0f,1.5f), Color_white, 0, 0, zText);
					}
#endif // __BANK
				}
				else
				{
					pPed->ResetRiverRapidsTimer();
				}
			}

			// Transform the velocity from 2D to 3D in such a way that it matches the orientation of the polygon it is
			// attached to.
			Quaternion q;
			q.FromVectors(ZAXIS, m_WaterTestHelper.m_vLastRiverHitNormal);
			// Use the quaternion to get the direction 3D world space flow vector.
			q.Transform(vFlowVelocity);

			// Increase the river flow speed as seen by boats close to a large waterfall.
			if(m_WaterTestHelper.GetRiverHitStored())
			{
				float fDistToWaterfallSquared = (VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()) - vWaterFallPosition).Mag2();
				if(fDistToWaterfallSquared < rage::square(fWaterFallRadius))
				{
					if(pParent->GetIsTypeVehicle())
					{
						CVehicle* pVehicle = static_cast<CVehicle*>(pParent);
						if(pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT)
						{
							TUNE_FLOAT(WATERFALL_FLOW_SPEED_SCALE, 20.0f, 0.0f, 50.0f, 1.0f);
							float fWaterfallDistanceScale = (1.0f - (fDistToWaterfallSquared / rage::square(fWaterFallRadius))) * WATERFALL_FLOW_SPEED_SCALE;
							vFlowVelocity.Scale(fWaterfallDistanceScale);
						}
					}
				}
			}
		}

		// Compute the relative velocity of this sample point with respect to the flow in local space.
		Vector3 vLocalFlowVelocity = pParent->GetLocalSpeed(vTestPointWorld, true, nComponent) - vFlowVelocity;

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if __BANK
		PF_STOP(ProcessBuoyancy);
		if(ms_bDrawBuoyancySampleSpheres)
		{
			grcDebugDraw::Sphere(vTestPointWorld,fSampleSize,Color32(0,255,0),false);
			char debugText[3];
			formatf(debugText,3,"%i",iSample);
			grcDebugDraw::Text(vTestPointWorld,Color32(255,0,0),debugText);
		}
		PF_START(ProcessBuoyancy);
#endif // __BANK

		// If we are in a river, we are not in a "Water" block so work out the depth based on the probe results.
		if(bInRiver)
		{
			if(pVehicle && pVehicle->ContainsLocalPlayer())
			{
				m_WaterTestHelper.GetRiverHeightAtPosition(vTestPointWorld, &fWaterLevel);
			}
			else
			{
				// Use the plane defined by the normal of the last poly hit by the river probe and the position of the
				// point hit to find the z-coord of the river surface (to first order) at the position of the current
				// sample point.
				fWaterLevel = -(m_WaterTestHelper.m_vLastRiverHitNormal.x*(vTestPointWorld.x-m_WaterTestHelper.m_vLastRiverHitPos.x)
					+ m_WaterTestHelper.m_vLastRiverHitNormal.y*(vTestPointWorld.y-m_WaterTestHelper.m_vLastRiverHitPos.y))
					/m_WaterTestHelper.m_vLastRiverHitNormal.z + m_WaterTestHelper.m_vLastRiverHitPos.z;
			}

			// Assume that buoyancy acts vertically; we will add a force based on the flow later.
			vecWaterNormal = ZAXIS;
			fWaterSpeedVert = 0.0f;
		}
		else
		{
			fWaterLevel = fWaterHeightNoWaves;
			// TODO: this is actually really expensive. Either optimise the function or better still roll our
			// own here for buoyancy to take advantage of the fact that we are looping over a grid of water samples.
			Water::AddWaveToResult(vTestPointWorld.x, vTestPointWorld.y, &fWaterLevel, &vecWaterNormal, &fWaterSpeedVert, pParent->GetIsTypePed());
		}

		m_fAbsWaterLevel = Max(m_fAbsWaterLevel, fWaterLevel); // Track the highest water level of all the samples.
		m_fAvgAbsWaterLevel += fWaterLevel;

		float fBottomOfSampleSphere = vTestPointWorld.z - fSampleSize;

		if( fWaterLevel <= fBottomOfSampleSphere &&
			pParent->GetIsTypeVehicle() )
		{
			// always store the sample submerge level, even if it isn't submerged, for the hover mode
			// as we can use it to make it hover better above the water
			CVehicle* pVehicle = static_cast< CVehicle* >( pParent );
			if( pVehicle->GetSpecialFlightModeRatio() == 1.0f )
			{
				m_fSampleSubmergeLevel[ iSample ] = fWaterLevel - fBottomOfSampleSphere;
				m_fSampleSubmergeLevel[ iSample ] /= 2.0f;
			}
		}

		if(fWaterLevel > fBottomOfSampleSphere)
		{
			++nSamplesUsedForSubmergeLevel;
			m_fSampleSubmergeLevel[iSample] = fWaterLevel - fBottomOfSampleSphere;
			// Scale water level between 0.0 and fSize.
			m_fSampleSubmergeLevel[iSample] /= 2.0f;
			if(m_fSampleSubmergeLevel[iSample] > fSampleSize)
			{
				m_fSampleSubmergeLevel[iSample] = fSampleSize;
				vecWaterNormal = Vector3(0.0f, 0.0f, 1.0f);
			}
			else
			{
				bUnderWater = false;
			}

			// Compute depth of this sample as a fraction of its diameter.
			float fDepthFactor = (fWaterLevel - vTestPointWorld.z) / (2.0f*fSampleSize);

			float partBuoyancyMultiplier = fSampleBuoyancyMult;

			float buoyancyConstant = pBuoyancyInfo->m_fBuoyancyConstant;

			if( pParent->IsNetworkClone() &&
				pVehicle &&
				( MI_CAR_APC.IsValid() &&
				  pVehicle->GetModelIndex() == MI_CAR_APC ) )
			{
				buoyancyConstant *= 1.2f;
			}
			// Use a separate buoyancy constant for peds when simulating 
			if (pPed && pPed->GetUsingRagdoll())
			{
				// Buoyancy constant at default gravity
				static float s_ragdollBuoyancyConstant = 100.0f;
				buoyancyConstant = s_ragdollBuoyancyConstant;
				if (!IsClose(GRAVITY, 0.0f, VERY_SMALL_FLOAT))
				{
					// Take into account changes in gravity (not sure if we maybe want to do this regardless of whether the ped is ragdolling or not...)
					buoyancyConstant *= (CPhysics::GetGravitationalAcceleration() / -GRAVITY);
				}
			}

			// Control the sink rate for animated corpses
			if (pPed && pPed->GetRagdollState() < RAGDOLL_STATE_PHYS && pPed->IsDead())
			{
				pPed->GetCollider()->SetVelocity(Vec3V(V_ZERO).GetIntrin128());
				buoyancyConstant = 0.0f;
			}

			Vector3 vSpeedAtPoint, vSpeedAtPointIncFlow;
			vSpeedAtPoint = pParent->GetLocalSpeed(vTestPointWorld, true, nComponent);
			if(fDepthFactor < 2.0f)
			{
				vSpeedAtPoint.z -= fWaterSpeedVert;
			}
			vSpeedAtPointIncFlow = Vector3(vLocalFlowVelocity.x, vLocalFlowVelocity.y, pParent->GetLocalSpeed(vTestPointWorld,true,nComponent).z-fWaterSpeedVert);

			// Mostly used by Vfx below:
			Vector3 vBuoyancyForceApplied(VEC3_ZERO);
			Vector3 vDampingForceComputed(VEC3_ZERO); // NB - This value may be clamped based on the total drag computed for all samples!

			// Certain sample spheres are add purely to provide keel forces on boats.
			bool bKeelSample = pBuoyancyInfo->m_fKeelMult > 0.0f && sample.m_bKeel;

			if(bKeelSample)
			{
				// KEEL FORCE //////////////////////////////////////////////////////////////////////
				ComputeSampleKeelForce(fTimeStep, pParent, fMass, iSample, nComponent, vTestPointWorld, vLocalPosition, vecWaterNormal, vSpeedAtPoint,
					vSpeedAtPointIncFlow, vFlowVelocity);

				// No further processing for these special keel samples (specifically, we don't want VFX processing them).
				if(!sample.m_bPontoon)
				{
					continue;
				}
			}

			// BUOYANCY FORCE //////////////////////////////////////////////////////////////////
			vBuoyancyForceApplied = ComputeSampleBuoyancyForce(fTimeStep, pParent, fMass, iSample, nComponent, vTestPointWorld, vecWaterNormal,
				buoyancyConstant, partBuoyancyMultiplier, fDepthFactor);

			vTotalBuoyancyForce += vBuoyancyForceApplied;

			if(vSpeedAtPointIncFlow.Mag2() > 0.1f)
			{
				// LIFT FORCE //////////////////////////////////////////////////////////////////////
				ComputeSampleLiftForce(fTimeStep, pParent, fMass, iSample, nComponent, vTestPointWorld, vecWaterNormal,vSpeedAtPointIncFlow, fWaterSpeedVert);

				// KEEL FORCE //////////////////////////////////////////////////////////////////////
				/// TODO - Once all boats have proper keel spheres set up, remove the call to ComputeSampleKeelForce from the "else" block below.
				if(pBoatHandling && pBoatHandling->m_fKeelSphereSize <= 0.0f)
				{
					ComputeSampleKeelForce(fTimeStep, pParent, fMass, iSample, nComponent, vTestPointWorld, vLocalPosition, vecWaterNormal, vSpeedAtPoint,
						vSpeedAtPointIncFlow, vFlowVelocity);
				}

				// FLOW INDUCED DRAG ///////////////////////////////////////////////////////////////
				float fDragCoefficient = m_fDragCoefficientInUse;
				if(sample.m_bPontoon)
				{
					if(pSeaplaneHandling)
					{
						fDragCoefficient = pSeaplaneHandling->m_fPontoonDragCoefficient;
					}
				}
				fDragCoefficient -= fDragCoefficient * dragReduction;

				vDampingForceComputed = ComputeSampleDragForce(pParent, forceAccumulators, fMass, iSample, nComponent, vTestPointWorld,
					vLocalFlowVelocity, fCrossSectionalLength, fDragCoefficient);
			}

			// BASIC DRAG //////////////////////////////////////////////////////////////////////
			if(!pParent->GetIsTypeObject() || !static_cast<CObject*>(pParent)->GetAsProjectile())
			{
				float fDragMultXY = 0.0f;
				float fDragMultZUp = 0.0f;
				float fDragMultZDown = 0.0f;
				if(sample.m_bPontoon)
				{
					// Cache the current drag settings so we can run the damping force subroutine so that it damps the vertical motion only for
					// seaplane pontoons.
					fDragMultXY = pBuoyancyInfo->m_fDragMultXY;
					fDragMultZUp = pBuoyancyInfo->m_fDragMultZUp;
					fDragMultZDown = pBuoyancyInfo->m_fDragMultZDown;

					pBuoyancyInfo->m_fDragMultXY = 0.0f;
					pBuoyancyInfo->m_fDragMultZUp = pSeaplaneHandling->m_fPontoonVerticalDampingCoefficientUp;
					pBuoyancyInfo->m_fDragMultZDown = pSeaplaneHandling->m_fPontoonVerticalDampingCoefficientDown;
				}

				vDampingForceComputed += ComputeSampleDampingForce(pParent, forceAccumulators, fMass, iSample, nComponent, vTestPointWorld,
					vSpeedAtPoint, partBuoyancyMultiplier);

				if(sample.m_bPontoon)
				{
					pBuoyancyInfo->m_fDragMultXY = fDragMultXY;
					pBuoyancyInfo->m_fDragMultZUp = fDragMultZUp;
					pBuoyancyInfo->m_fDragMultZDown = fDragMultZDown;
				}
			}

			// Disturb the dynamic water we're hitting.
			vSpeedAtPoint = pParent->GetLocalSpeed(vTestPointWorld, true, nComponent);
			bool bIsSubmersible = pParent->GetIsTypeVehicle() && static_cast<CVehicle*>(pParent)->InheritsFromSubmarine();
			bool bIsTugBoat		= m_buoyancyFlags.bReduceLateralBuoyancyForce;
			bool bIsSeaHeli		= pParent->GetIsTypeVehicle() && static_cast<CVehicle*>( pParent )->InheritsFromHeli() && static_cast<CVehicle*>( pParent )->pHandling->GetSeaPlaneHandlingData();

			float fMaxDepthFactor = bIsSubmersible || bIsTugBoat ? 0.5f : 2.0f;
			float fMinDepthFactor = bIsSeaHeli ? 0.5f : 0.0f;

			if(fDepthFactor > fMinDepthFactor && fDepthFactor < fMaxDepthFactor)
			{
				// Get some factor based on the size and weight of this water sample.
				float fAddToWaterSpeedPercentage = m_fSampleSubmergeLevel[iSample];
				fAddToWaterSpeedPercentage *= m_fForceMult * partBuoyancyMultiplier * buoyancyConstant * rage::Min(1.0f, fMass / 100.0f);

				// Then scale by the speed through the water.
				static float WATER_SPEED_CAP_SQR = 40.0f*40.0f;
				static float WATER_SPEED_VERT_MAX = 2.0f;
				float fVertVel = rage::Min(vSpeedAtPoint.z, WATER_SPEED_VERT_MAX);

				if( !pParent->IsNetworkClone() || fVertVel < 0.0f )
				{
					fAddToWaterSpeedPercentage *= Clamp(fVertVel*fVertVel, 1.0f, WATER_SPEED_CAP_SQR) / WATER_SPEED_CAP_SQR;

					static float WATER_SPEED_ADD_MULT = 40.0f;
					static float WATER_SPEED_ADD_CAP = 0.3f;
					fAddToWaterSpeedPercentage = rage::Min(WATER_SPEED_ADD_CAP, WATER_SPEED_ADD_MULT * fAddToWaterSpeedPercentage);

					if(fAddToWaterSpeedPercentage > 0.0f)
						Water::ModifyDynamicWaterSpeed(vTestPointWorld.x, vTestPointWorld.y, fVertVel, fAddToWaterSpeedPercentage);
				}
			}

			// VFX - Process this sample.
			if(processFx)
			{
				// Don't want to do Vfx on the extra dinghy buoyancy samples.
				if(!(pBoatHandling && pBoatHandling->m_fDinghySphereBuoyConst>0.0f && fSampleBuoyancyMult != DINGHY_BUOYANCY_MULT_FOR_GENERIC_SAMPLES))
				{
					Vector3 vecSurfacePos(vTestPointWorld.x, vTestPointWorld.y, fWaterLevel);
					vfxWaterSampleData[iSample].isInWater = true;
					//vfxWaterSampleData[iSample].vTestPos = RCC_VEC3V(vTestPointWorld);
					vfxWaterSampleData[iSample].vSurfacePos = RCC_VEC3V(vecSurfacePos);
					//vfxWaterSampleData[iSample].maxLevel = fSampleSize;
					vfxWaterSampleData[iSample].currLevel = m_fSampleSubmergeLevel[iSample];
					vfxWaterSampleData[iSample].componentId = nComponent;
					vfxWaterSampleData[iSample].boneTag = BONETAG_INVALID;

#if __BANK
					if (g_vfxWater.RenderFloaterCalculatedSurfacePositions())
					{
						grcDebugDraw::Sphere(vecSurfacePos, 0.5f, Color32(1.0f, 1.0f, 0.0f, 1.0f), true, -1);
					}
#endif
				}
			}

			if(!bKeelSample)
			{
				// If we have collided with the map, reduce the radius of the buoyancy spheres when deciding if they are
				// in water.
				if(pParent->GetFrameCollisionHistory()->GetFirstBuildingCollisionRecord())
				{
					const float kfRadiusReductionFactor = 0.5f;
					if(fWaterLevel > vTestPointWorld.z - fSampleSize*kfRadiusReductionFactor)
					{
						bInWater = true;
					}
				}
				else
				{
					bInWater = true;
				}
			}
			m_WaterTestHelper.m_vLastWaterNormal.Set(vecWaterNormal);
			if(partBuoyancyMultiplier > 0.0f)
			{
				ComputeSampleStickToSurfaceForce(fTimeStep, pParent, fMass, iSample, nComponent, vTestPointWorld, vSpeedAtPointIncFlow);
			}

			m_fSubmergedLevel += m_fSampleSubmergeLevel[iSample] / fSampleSize;
		}
		else
		{
			bUnderWater = false;
		}

#if __BANK
		PF_STOP(ProcessBuoyancy);
		if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING && pParent->GetStatus()==STATUS_PLAYER) || ms_bDrawBuoyancyTests)
		{
			if(m_fSampleSubmergeLevel[iSample] > 0.0f)
			{
				grcDebugDraw::Line(Vector3(vTestPointWorld.x - 0.1f, vTestPointWorld.y, fWaterLevel), Vector3(vTestPointWorld.x + 0.1f, vTestPointWorld.y, fWaterLevel), Color32(0.0f,0.0f,1.0f));
				grcDebugDraw::Line(Vector3(vTestPointWorld.x, vTestPointWorld.y - 0.1f, fWaterLevel), Vector3(vTestPointWorld.x, vTestPointWorld.y + 0.1f, fWaterLevel), Color32(0.0f,0.0f,1.0f));
				grcDebugDraw::Line(Vector3(vTestPointWorld.x, vTestPointWorld.y, fWaterLevel + 0.5f), Vector3(vTestPointWorld.x, vTestPointWorld.y, fWaterLevel + 0.5f + fWaterSpeedVert),  Color32(0.0f,1.0f,0.0f));
			}
			grcDebugDraw::Line(vTestPointWorld, vTestPointWorld - Vector3(0.0f,0.0f,fSampleSize), Color32(1.0f,0.0f,0.0f), Color32(0.0f,0.0f,1.0f));
		}
		PF_START(ProcessBuoyancy);
#endif // __BANK
	} // End of iteration over each buoyancy sample.

	if(nSamplesUsedForSubmergeLevel > 0)
	{
		m_fSubmergedLevel /= (float)nSamplesUsedForSubmergeLevel;
		m_fAvgAbsWaterLevel /= (float)nSamplesUsedForSubmergeLevel;
	}
	
	for(atMap<int, int>::Iterator it = m_componentIndexToForceAccumMap.CreateIterator(); !it.AtEnd(); it.Next())
	{
		int nComponentId = it.GetKey();
		int nForceAccId = it.GetData();
		SForceAccumulator& forceAccumulator = forceAccumulators[nForceAccId];

		// Apply water based drag forces:
		Vector3 vCombinedResistiveForce = forceAccumulator.vDragForce + forceAccumulator.vDampingForce;
		Vector3 vCombinedResistiveTorque = forceAccumulator.vDragTorque + forceAccumulator.vDampingTorque;
		ClampResistiveForceAndTorque(fTimeStep, pParent, vCombinedResistiveForce, vCombinedResistiveTorque);

		if( vCombinedResistiveForce.IsNonZero() )
		{
			if( nComponentId < nNumBounds )
			{
				pParent->ApplyForceCg(vCombinedResistiveForce, fTimeStep);
			}
			else
			{
				Warningf( "Force computed on bound which this object (%s) doesn't have: componentId = %d [max=%d].", pParent->GetModelName(), nComponentId, nNumBounds);
			}
		}
		if(vCombinedResistiveTorque.IsNonZero())
		{
			// Don't want the drag affecting the pitch of the submersible due to centre of mass and centre of buoyancy not being vertically aligned.
			if(pParent->GetIsTypeVehicle() && 
			   static_cast<CVehicle*>(pParent)->InheritsFromSubmarine() )
			{
				// Transform the torque to model space so we can null the right component.
				Matrix34 submarineMatrix = MAT34V_TO_MATRIX34(pParent->GetMatrix());
				Vector3 vTorqueModelSpace;
				submarineMatrix.UnTransform(vCombinedResistiveTorque, vTorqueModelSpace);
				vTorqueModelSpace.x = 0.0f;

				// ... and back to world space.
				submarineMatrix.Transform(vTorqueModelSpace, vCombinedResistiveTorque);
			}
			pParent->ApplyTorque(vCombinedResistiveTorque, fTimeStep);
		}
		if(vCombinedResistiveForce.IsNonZero() || vCombinedResistiveTorque.IsNonZero()) 
		{
			if (nComponentId < nNumBounds)
			{
				pParent->NotifyWaterImpact(vCombinedResistiveForce, vCombinedResistiveTorque, nComponentId, fTimeStep);
			}
			else
			{
				Warningf("Force computed on bound which this object (%s) doesn't have: componentId = %d [max=%d].", pParent->GetModelName(), nComponentId, nNumBounds);
			}
		}
	}

#if DEBUG_BUOYANCY
    PF_SET(BuoyancyForce, m_fTotalBuoyancyForce);
    PF_SET(LiftForce, m_fTotalLiftForce);
    PF_SET(KeelForce, m_fTotalKeelForce);
    PF_SET(FlowDragForce, m_fTotalFlowDragForce);
    PF_SET(BasicDragForce, m_fTotalBasicDragForce);
#endif // DEBUG_BUOYANCY

	if(bUnderWater)
	{
		m_waterLevelStatus = FULLY_IN_WATER;
	}
	else if(bInWater)
	{
		m_waterLevelStatus = PARTIALLY_IN_WATER;
	}
	else
	{
		m_waterLevelStatus = NOT_IN_WATER;
	}

	if(pBuoyancyAccel)
		*pBuoyancyAccel = vTotalBuoyancyForce.z / fMass;


	// This flag will let us know whether such info as "submerged level", "avg submerge level", etc. are actually
	// up-to-date for this entity.
	m_buoyancyFlags.bBuoyancyInfoUpToDate = true;


	// VFX - Process animated peds.
	bool hasAnimatedPedSamples = false;
	if(processFx && isAnimatedPed)
	{
		// don't process fx for peds they aren't high lod 
		bool isHiLODPed = pPed && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH;
		if (!isHiLODPed)
		{
			processFx = false;
		}

		CPed* pPed = static_cast<CPed*>(pParent);
		if(pPed)
		{
			// Get the vfx ped info.
			CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
			if (pVfxPedInfo)
			{
				// don't process fx for peds that don't have room to store the vfx samples
				int numPedSkeletonBoneInfos = pVfxPedInfo->GetPedSkeletonBoneNumInfos();
				if (m_nNumInSampleLevelArray-iSampleEnd < numPedSkeletonBoneInfos)
				{
					processFx = false;
				}

				// check if we're still interested in processing fx for this ped
				if (processFx)
				{
					hasAnimatedPedSamples = true;

					// do extra water tests, and store results in extra water samples
					// where the first is stored in the last sample, the second in the second last sample etc
					for (s32 i=0; i<numPedSkeletonBoneInfos; i++)
					{
						const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
						const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();
						if (pBoneWaterInfo && pBoneWaterInfo->m_sampleSize>0.0f)
						{
							s32 currSampleIndex = m_nNumInSampleLevelArray-1-i;

							vfxWaterSampleData[currSampleIndex].prevLevel = m_fSampleSubmergeLevel[currSampleIndex];

							m_fSampleSubmergeLevel[currSampleIndex] = 0.0f;

							const float fSize = pBoneWaterInfo->m_sampleSize;

							int boneIndex = pPed->GetBoneIndexFromBoneTag(pSkeletonBoneInfo->m_boneTagA);
							if (boneIndex!=-1)
							{
								Matrix34 boneMtx;
								CVfxHelper::GetMatrixFromBoneIndex(RC_MAT34V(boneMtx), pPed, boneIndex);

								vTestPointWorld = boneMtx.d;

								float fWaterLevel;
								if(bInRiver)
								{
									// Use the plane defined by the normal of the last poly hit by the river probe and the position of the
									// point hit to find the z-coord of the river surface (to first order) at the position of the current
									// sample point.
									fWaterLevel = -(m_WaterTestHelper.m_vLastRiverHitNormal.x*(vTestPointWorld.x-m_WaterTestHelper.m_vLastRiverHitPos.x)
										+ m_WaterTestHelper.m_vLastRiverHitNormal.y*(vTestPointWorld.y-m_WaterTestHelper.m_vLastRiverHitPos.y))
										/m_WaterTestHelper.m_vLastRiverHitNormal.z + m_WaterTestHelper.m_vLastRiverHitPos.z;
								}
								else
								{
									fWaterLevel = fWaterHeightNoWaves;
									Water::AddWaveToResult(vTestPointWorld.x, vTestPointWorld.y, &fWaterLevel, &vecWaterNormal, &fWaterSpeedVert, true);
								}

								vfxWaterSampleData[currSampleIndex].vTestPos = RCC_VEC3V(vTestPointWorld);
								vfxWaterSampleData[currSampleIndex].maxLevel = fSize;

								if(fWaterLevel > vTestPointWorld.z - fSize)
								{
									m_fSampleSubmergeLevel[currSampleIndex] = fWaterLevel - (vTestPointWorld.z - fSize);

									// scale water level between 0.0 and fSize
									m_fSampleSubmergeLevel[currSampleIndex] *= 0.5f;
									if (m_fSampleSubmergeLevel[currSampleIndex] > fSize)
									{
										m_fSampleSubmergeLevel[currSampleIndex] = fSize;
									}

									// do particles for this point
									Vector3 vecSurfacePos(vTestPointWorld.x, vTestPointWorld.y, fWaterLevel);
									vfxWaterSampleData[currSampleIndex].isInWater = true;
									vfxWaterSampleData[currSampleIndex].vSurfacePos = RCC_VEC3V(vecSurfacePos);
									vfxWaterSampleData[currSampleIndex].currLevel = m_fSampleSubmergeLevel[currSampleIndex];
									vfxWaterSampleData[currSampleIndex].componentId = pPed->GetRagdollComponentFromBoneTag(pSkeletonBoneInfo->m_boneTagA);
									vfxWaterSampleData[currSampleIndex].boneTag = pSkeletonBoneInfo->m_boneTagA;

#if __BANK
									if (g_vfxWater.RenderFloaterCalculatedSurfacePositions())
									{
										grcDebugDraw::Sphere(vecSurfacePos, 0.5f, Color32(1.0f, 0.0f, 1.0f, 1.0f), true, -1);
									}
#endif
								}
							}
						}
					}
				}
			}
			else
			{
				processFx = false;
			}
		}
	}

	// vfx - process
	if (processFx REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		g_vfxWater.ProcessVfxWater(pParent, vfxWaterSampleData, hasAnimatedPedSamples, m_nNumInSampleLevelArray);
	}

	PF_STOP(ProcessBuoyancy);
	PF_SETTIMER(ProcessBuoyancyTotal, PF_READ_TIME(ProcessBuoyancy) + PF_READ_TIME(ProcessLowLodBuoyancyTimer));
	return bInWater;
}

// Helper functions for computing the various forces on each sample. ///////////////////////////////////////////////////////////////////
float CBuoyancy::ComputeCrossSectionalLengthForDrag(CPhysical* pParent)
{
	PF_START(ComputeCrossSection);
	// Compute some values used in the drag calculation. These values are constant for each buoyancy sample.
	ScalarV vCrossSectionalLength(V_ZERO);
	Vector2 vFlow(0.0f, 0.0f);
	// If we are in a river instead of a "water" block, the flow will be non-zero and defined by the river flow textures.
	if(m_WaterTestHelper.GetRiverHitStored())
	{
		River::GetRiverFlowFromPosition(pParent->GetTransform().GetPosition(), vFlow);
		vFlow.Scale(1.0f/River::GetRiverPushForce());
	}
	m_WaterTestHelper.m_vLastRiverVelocity.Set(vFlow.x, vFlow.y, 0.0f);
	m_WaterTestHelper.m_vLastRiverVelocity.Scale(m_fFlowVelocityScaleFactor);

	Vec3V vParentVel = VECTOR3_TO_VEC3V(pParent->GetVelocity());
	Vec3V vLastRiverVel = VECTOR3_TO_VEC3V(m_WaterTestHelper.m_vLastRiverVelocity);
	Vec3V vLocalFlowVelocity = vParentVel - vLastRiverVel;

	// Project all vertices of the bounding box in the direction of the velocity relative to the flow to find
	// an approximate value for the cross-sectional area seen by the flow.
	phBound* pBound = pParent->GetCurrentPhysicsInst()->GetArchetype()->GetBound();
	Vec3V vBoundBoxMin = pBound->GetBoundingBoxMin();
	Vec3V vBoundBoxMax = pBound->GetBoundingBoxMax();
	// Define the vertices.
	Vec3V vVertices0, vVertices1, vVertices2, vVertices3, vVertices4, vVertices5, vVertices6, vVertices7;
	vVertices0 = SelectFT(VecBoolV(V_F_T_F_T), vBoundBoxMin, vBoundBoxMax);
	vVertices1 = SelectFT(VecBoolV(V_F_F_F_T), vBoundBoxMin, vBoundBoxMax);
	vVertices2 = SelectFT(VecBoolV(V_T_F_F_T), vBoundBoxMin, vBoundBoxMax);
	vVertices3 = SelectFT(VecBoolV(V_T_T_F_T), vBoundBoxMin, vBoundBoxMax);
	vVertices4 = SelectFT(VecBoolV(V_F_T_T_T), vBoundBoxMin, vBoundBoxMax);
	vVertices5 = SelectFT(VecBoolV(V_F_F_T_T), vBoundBoxMin, vBoundBoxMax);
	vVertices6 = SelectFT(VecBoolV(V_T_F_T_T), vBoundBoxMin, vBoundBoxMax);
	vVertices7 = SelectFT(VecBoolV(V_T_T_T_T), vBoundBoxMin, vBoundBoxMax);

	// Compute the normal vector to project onto.
	Vec3V vProjectionNormal(V_Y_AXIS_WZERO);
	ScalarV vTempX(vLocalFlowVelocity.GetX());
	ScalarV vTempY(vLocalFlowVelocity.GetY());
	vTempX = InvScale(vTempY, vTempX);
	vTempX = Scale(vTempX, ScalarV(V_NEGONE));
	vProjectionNormal.SetX(vTempX);
	vProjectionNormal = NormalizeFastSafe(vProjectionNormal, Vec3V(V_ZERO));
	// Project the verts and find the max and min extents along the projection line.
	ScalarV vMaxExtent(V_ZERO);
	ScalarV vMinExtent(V_ZERO);
	ScalarV vProjectedDist;

	// Transform the vertices into world space so that they are in the same coord system as the projection vector.
	Mat34V matParent = pParent->GetTransform().GetMatrix();

	vVertices0 = Transform3x3(matParent, vVertices0);
	vProjectedDist = Dot(vProjectionNormal, vVertices0);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vVertices1 = Transform3x3(matParent, vVertices1);
	vProjectedDist = Dot(vProjectionNormal, vVertices1);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vVertices2 = Transform3x3(matParent, vVertices2);
	vProjectedDist = Dot(vProjectionNormal, vVertices2);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vVertices3 = Transform3x3(matParent, vVertices3);
	vProjectedDist = Dot(vProjectionNormal, vVertices3);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vVertices4 = Transform3x3(matParent, vVertices4);
	vProjectedDist = Dot(vProjectionNormal, vVertices4);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vVertices5 = Transform3x3(matParent, vVertices5);
	vProjectedDist = Dot(vProjectionNormal, vVertices5);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vVertices6 = Transform3x3(matParent, vVertices6);
	vProjectedDist = Dot(vProjectionNormal, vVertices6);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vVertices7 = Transform3x3(matParent, vVertices7);
	vProjectedDist = Dot(vProjectionNormal, vVertices7);
	vMaxExtent = SelectFT(IsGreaterThan(vProjectedDist, vMaxExtent), vMaxExtent, vProjectedDist);
	vMinExtent = SelectFT(IsLessThan(vProjectedDist, vMinExtent), vMinExtent, vProjectedDist);

	vCrossSectionalLength = vMaxExtent - vMinExtent;

	// DEBUG DISPLAY THE CROSS SECTIONAL LENGTH.
#if __DEV && DEBUG_DRAW
	PF_STOP(ProcessBuoyancy);
	Vec3V vecParentPos = pParent->GetTransform().GetPosition();
	if(ms_bDebugDrawCrossSection)
	{
		grcDebugDraw::Line(vecParentPos+Scale(vProjectionNormal, vMaxExtent),vecParentPos+Scale(vProjectionNormal,vMinExtent), Color_yellow);
	}
	PF_START(ProcessBuoyancy);
#endif // __DEV && DEBUG_DRAW

	PF_STOP(ComputeCrossSection);
	return vCrossSectionalLength.Getf();
}


void CBuoyancy::SelectDragCoefficient(CPhysical* pParent)
{
	// Decide which drag coefficient to use:
	m_fDragCoefficientInUse = ms_fDragCoefficient;

	// Peds should only get carried with the flow when they aren't standing.
	if(pParent->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pParent);
		if(!pPed->GetIsStanding())
			m_fDragCoefficientInUse = ms_fPedDragCoefficient;
	}
	else if(pParent->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pParent);
		if(pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT || pVehicle->InheritsFromAmphibiousAutomobile())
		{
			// If this is a boat, override the generic drag coefficient (probably for a box shape) with the value found
			// in the vehicle handling file.
			CBoatHandlingData* pBoatHandling = NULL;
			pBoatHandling = static_cast<CVehicle*>(pParent)->pHandling->GetBoatHandlingData();
			if(pBoatHandling)
			{
				m_fDragCoefficientInUse = pBoatHandling->m_fDragCoefficient;
			}

			// Increase the drag coefficient when the boat is close to a waterfall to make sure we get quickly pushed over the edge.
			if(m_WaterTestHelper.GetRiverHitStored())
			{
				float fDistToWaterfallSquared = (VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()) - vWaterFallPosition).Mag2();
				if(fDistToWaterfallSquared < rage::square(fWaterFallRadius))
				{
					TUNE_FLOAT(WATERFALL_DRAG_COEFFICIENT_SCALE, 10.0f, 0.0f, 500.0f, 1.0f);
					float fWaterfallDistanceScale = (1.0f - (fDistToWaterfallSquared / rage::square(fWaterFallRadius))) * WATERFALL_DRAG_COEFFICIENT_SCALE;
					m_fDragCoefficientInUse *= fWaterfallDistanceScale;
				}
			}

			m_buoyancyFlags.bReduceLateralBuoyancyForce = ( MI_BOAT_TUG.IsValid() && ( pParent->GetModelIndex() == MI_BOAT_TUG.GetModelIndex() ) ) ||
															( MI_CAR_APC.IsValid() && ( pParent->GetModelIndex() == MI_CAR_APC.GetModelIndex() ) ) ||
															( MI_PLANE_TULA.IsValid() && ( pParent->GetModelIndex() == MI_CAR_APC.GetModelIndex() ) ); 
		}
		else if(pVehicle->InheritsFromBike())
		{
			// Reduced drag on bikes.
			m_fDragCoefficientInUse = ms_fBikeDragCoefficient;
		}
		else
		{
			m_fDragCoefficientInUse = ms_fCarDragCoefficient;
		}
	}
	else if(pParent->GetIsTypeObject())
	{
		if(static_cast<CObject*>(pParent)->GetAsProjectile())
		{
			m_fDragCoefficientInUse = ms_fProjectileDragCoefficient;
		}
		// Check if this object overrides the default drag coefficient.
		else if(const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(pParent->GetBaseModelInfo()->GetModelNameHash()))
		{
			if(pTuning->GetBuoyancyDragFactor() != 1.0f)
			{
				m_fDragCoefficientInUse = ms_fDragCoefficient*pTuning->GetBuoyancyDragFactor();
			}
		}
	}
}

void ComputeSampleOffsetFromComponentCG(const CPhysical* pParent, s32 nComponent, const Vector3& vTestPointWorld, Vector3& vOffsetFromComponentCG)
{
	if(nComponent > 0)
	{
		physicsAssert(pParent->GetCurrentPhysicsInst());
		physicsAssert(pParent->GetCurrentPhysicsInst()->GetArchetype());
		physicsAssert(pParent->GetCurrentPhysicsInst()->GetArchetype()->GetBound());
		const phBound* pBound = pParent->GetCurrentPhysicsInst()->GetArchetype()->GetBound();
		if(AssertVerify(pBound->GetType() == phBound::COMPOSITE))
		{
			const phBoundComposite* pCompBound = static_cast<const phBoundComposite*>(pBound);
			physicsAssertf(nComponent >= 0 && nComponent < pCompBound->GetNumBounds(), "nComponent = %d", nComponent);
			physicsAssertf(pCompBound->GetBound(nComponent), "Component = %d on model %s", nComponent, pParent->GetModelName());

			// The offset of the centre of mass from this bound's local origin.
			Vec3V vBoundCGOffset = pCompBound->GetBound(nComponent)->GetCGOffset();

			// Transform from bound's local origin to the composite bound's local origin.
			vBoundCGOffset = Transform(pCompBound->GetCurrentMatrix(nComponent), vBoundCGOffset);

			// Now transform into world space.
			vBoundCGOffset = Transform(pParent->GetMatrix(), vBoundCGOffset);

			// Finally we can work out the offset of our world space point from the component's centre of mass.
			vOffsetFromComponentCG = vTestPointWorld - VEC3V_TO_VECTOR3(vBoundCGOffset);
		}
	}
	else
	{
		physicsAssert(pParent->GetCurrentPhysicsInst());
		physicsAssert(pParent->GetCurrentPhysicsInst()->GetArchetype());
		physicsAssert(pParent->GetCurrentPhysicsInst()->GetArchetype()->GetBound());
		const phBound* pBound = pParent->GetCurrentPhysicsInst()->GetArchetype()->GetBound();

		// The offset of the centre of mass from this bound's local origin.
		Vec3V vBoundCGOffset = pBound->GetCGOffset();

		// Now transform into world space.
		vBoundCGOffset = Transform(pParent->GetMatrix(), vBoundCGOffset);

		// Finally we can work out the offset of our world space point from the object's centre of mass.
		vOffsetFromComponentCG = vTestPointWorld - VEC3V_TO_VECTOR3(vBoundCGOffset);
	}
}

// Clamp a resistive force and torque so that we don't end up reversing an object's velocity due to drag-type forces.
void CBuoyancy::ClampResistiveForceAndTorque(float fTimeStep, const CPhysical* pParent, Vector3& vForce, Vector3& UNUSED_PARAM(vTorque))
{
	Vector3 vVelocity = pParent->GetVelocity();
	// Make sure the foliage drag force we have accumulated so far won't reverse the velocity.
	// Test for a valid timestep here, since CObject::AddPhysics() calls buoyancy.Process() with a timestep of 0.0f
	float fInvTimeStep = (Abs(fTimeStep) > 0.0001f) ? 1.0f / fTimeStep : 0.0f;
	float fMass = pParent->GetMass();
	Vector3 vMaxForce = -vVelocity * fInvTimeStep;
	// vMaxForce is the max acceleration allowed at this point; clamp it so we never generate a resistive force
	// which causes a "force too large" assert.
	vMaxForce.ClampMag(0.0f, 0.99f*DEFAULT_ACCEL_LIMIT);
	// Now we can actually turn it into the max allowed force.
	vMaxForce.Scale(fMass);
	float fMaxForceMag = vMaxForce.IsNonZero() ? vMaxForce.Mag() : 0.0f;
	vForce.ClampMag(0.0f, fMaxForceMag);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Compute the force which will actually cause the parent object to float (or not).
Vector3 CBuoyancy::ComputeSampleBuoyancyForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
										   const Vector3& vWaterNormal, float fBuoyancyConstant, float fPartBuoyancyMultiplier, float fDepthFactor)
{
	PF_START(ComputeBuoyancyForce);

	Vector3 vBuoyancyForce = m_fSampleSubmergeLevel[nSample] * m_fForceMult * fMass * vWaterNormal * fBuoyancyConstant * fPartBuoyancyMultiplier;
	// Allow scaling of buoyancy for specific objects.
	physicsAssert(pParent->GetPhysArch());
	vBuoyancyForce *= pParent->GetPhysArch()->GetBuoyancyFactor();

	if(pParent->GetIsTypeVehicle() && ( static_cast<CVehicle*>(pParent)->InheritsFromBoat() || static_cast<CVehicle*>(pParent)->InheritsFromAmphibiousAutomobile() ) )
	{
		CBoatHandlingData* pBoatHandling = static_cast<CVehicle*>(pParent)->pHandling->GetBoatHandlingData();

		if(fDepthFactor >= 1.0f && pBoatHandling)
		{
			vBuoyancyForce.Scale(pBoatHandling->m_fDeepWaterSampleBuoyancyMult);
		}
	}

#if __BANK
	if(ms_bDisableBuoyancyForceXY)
	{
		vBuoyancyForce.x = vBuoyancyForce.y = 0.0f;
	}
#endif // __BANK

	// Hack to fix B*1978951 - sometimes the tug boat would fail to accelerate past 10mph
	// doing this fixes it.
	if( m_buoyancyFlags.bReduceLateralBuoyancyForce )
	{
		vBuoyancyForce.x *= 0.1f;
		vBuoyancyForce.y *= 0.1f;

		if( pParent->GetIsTypeVehicle() && ( static_cast<CVehicle*>(pParent)->InheritsFromPlane() ) )
		{
			vBuoyancyForce.x = 0.0f;
			vBuoyancyForce.y = 0.0f;
		}
	}

	if( pParent->GetIsTypeVehicle() && ( static_cast<CVehicle*>(pParent)->InheritsFromSubmarine() ) )
	{
		CTaskVehicleMissionBase* pVehicleTask = static_cast<CVehicle*>(pParent)->GetIntelligence()->GetActiveTask();
		bool bShouldHoverAtEnd = false;
		if(pVehicleTask && (pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_SUBMARINE) )
		{
			CTaskVehicleGoToSubmarine* pTask = static_cast<CTaskVehicleGoToSubmarine *>(pVehicleTask);
			if(pTask && pTask->ShouldHoverAtEnd())
			{
				bShouldHoverAtEnd = true;
			}
		}

		if( !static_cast<CVehicle*>(pParent)->GetDriver() || bShouldHoverAtEnd )
		{
			vBuoyancyForce.x = 0.0f;
			vBuoyancyForce.y = 0.0f;
		}
	}

#if __ASSERT
	// Add some extra debug info to the log if we are going to cause some of the asserts in ApplyForce to fire.
	if(!pParent->CheckForceInRange(vBuoyancyForce, DEFAULT_ACCEL_LIMIT))
	{
		GENERIC_FORCE_CHECK("BUOYANCY FORCE", vBuoyancyForce);

		Displayf("m_fSampleSubmergeLevel[%d]: %5.3f", nSample, m_fSampleSubmergeLevel[nSample]);
		Displayf("m_fForceMult=%5.3f, fMass=%5.3f, buoyancyConstant=%5.3f, partBuoyancyMultiplier=%5.3f",
			m_fForceMult, fMass, fBuoyancyConstant, fPartBuoyancyMultiplier);
		Displayf("vecWaterNormal=(%5.3f, %5.3f, %5.3f), (magnitude=%5.3f)\n",
			vWaterNormal.x, vWaterNormal.y, vWaterNormal.z, vWaterNormal.Mag());
	}
#endif // __ASSERT
	DEBUG_SAMPLE_BUOYANCY_FORCE(nSample, "buoyancy", vBuoyancyForce);

#if __DEV
	m_fTotalBuoyancyForce += vBuoyancyForce.Mag();
#endif

	// Don't apply buoyancy force if we're in Swim FPS strafe underwater or aiming underwater (ie harpoon)
	bool bDontApplyBuoyancyForce = false;
	if (pParent->GetIsTypePed())
	{
		CPed *pPed = static_cast<CPed*>(pParent);
		if (pPed && pPed->GetIsSwimming())
		{
			if (pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
			{
				bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
				if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
				{
					bFPSMode = true;
				}
#endif	//FPS_MODE_SUPPORTED

				if ((pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsUnderwaterGun()) || bFPSMode)
				{
					bDontApplyBuoyancyForce = true;
				}
			}
		}
	}

	if (!bDontApplyBuoyancyForce)
	{
		pParent->ApplyForce(vBuoyancyForce, vTestPointWorld - VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()), nComponent, true, fTimeStep);
	}

//	// Apply weight-belt.
//	if(GetPedWeightBeltActive() && nComponent == RAGDOLL_SPINE0)
//	{
//		vBuoyancyForce = fMass * -CPhysics::GetGravitationalAcceleration() * FRAGNMASSETMGR->GetWeightBeltMultiplier(nNMAssetID) * Vector3(0.0f,0.0f,1.0f);
//
//#if __BANK
//		if(ms_bDisablePedWeightBeltForce)
//		{
//			vBuoyancyForce.Set(0.0f);
//		}
//#endif // __BANK
//
//#if __ASSERT
//		// Add some extra debug info to the log if we are going to cause some of the asserts in ApplyForce to fire.
//		if(!pParent->CheckForceInRange(vBuoyancyForce, DEFAULT_ACCEL_LIMIT))
//		{
//			GENERIC_FORCE_CHECK("WEIGHT-BELT FORCE", vBuoyancyForce);
//
//			Displayf("m_fSampleSubmergeLevel[%d]: %5.3f", nSample, m_fSampleSubmergeLevel[nSample]);
//			Displayf("weight belt multiplier=%5.3f, fMass=%5.3f\n",	FRAGNMASSETMGR->GetWeightBeltMultiplier(nNMAssetID), fMass);
//		}
//#endif // __ASSERT
//		DEBUG_SAMPLE_BUOYANCY_FORCE(nSample, "weight-belt", vBuoyancyForce);
//		pParent->ApplyForce(vBuoyancyForce, vTestPointWorld - VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()), nComponent, true, fTimeStep);
//	}

	PF_STOP(ComputeBuoyancyForce);

	return vBuoyancyForce;
}

// Calculate drag and lift through the water based on movement of individual points.
void CBuoyancy::ComputeSampleLiftForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
									   const Vector3& vWaterNormal, const Vector3& vSpeedAtPointIncFlow, float fWaterSpeedVert)
{
	PF_START(ComputeLiftForce);

	CBuoyancyInfo* pBuoyancyInfo = GetBuoyancyInfo(pParent);

	if(pBuoyancyInfo->m_fLiftMult > 0.0f && vSpeedAtPointIncFlow.Mag2() > 0.0f)
	{
		// Cache some data for better performance.
		CWaterSample& sample = pBuoyancyInfo->m_WaterSamples[nSample];

		//CBoat* pBoat = NULL;
		CBoatHandlingData* pBoatHandling = NULL;
		if(pParent->GetIsTypeVehicle() && static_cast<CVehicle*>(pParent)->GetVehicleType()==VEHICLE_TYPE_BOAT)
		{
			//pBoat = static_cast<CBoat*>(pParent);
			pBoatHandling = static_cast<CVehicle*>(pParent)->pHandling->GetBoatHandlingData();
		}
		if(pParent->GetIsTypeVehicle() && static_cast<CVehicle*>(pParent)->InheritsFromAmphibiousAutomobile())
		{
			pBoatHandling = static_cast<CVehicle*>(pParent)->pHandling->GetBoatHandlingData();
		}

		float fSpeedAlongNorm = vWaterNormal.Dot(vSpeedAtPointIncFlow);
		float fSpeedForLift = 0.0f;
		float fAngleOfAttack = 0.0f;

		Vector3 vecSpeedAcrossNormal = vSpeedAtPointIncFlow - fSpeedAlongNorm * vWaterNormal;

		if(vecSpeedAcrossNormal.Mag2() > 0.0f)
		{
			Vector3 vecLiftVec = VEC3V_TO_VECTOR3(pParent->GetTransform().GetC());
			// tilt lift vector back a bit to get some lift when traveling flat
			// might want to do this for boats only
			//vecLiftVec -= 0.3f*pParent->GetB();

			float fSpeedAlongLiftVec = -vecLiftVec.Dot(vecSpeedAcrossNormal);
			fSpeedForLift = vecSpeedAcrossNormal.Mag();
			fAngleOfAttack = rage::Atan2f(fSpeedAlongLiftVec, fSpeedForLift);
		}

		if(fSpeedForLift > 0.0f)
		{
			float fStdPlaningSpeed = 20.0f;
			if(pBoatHandling)
				fStdPlaningSpeed = sfWaterSampleCruiseMult*static_cast<CVehicle*>(pParent)->m_Transmission.GetDriveMaxVelocity();

			float fLiftForce = fSpeedForLift / fStdPlaningSpeed;
			// get lift to drop off quite steeply past desired planing speed (don't want boats to race off too fast)
			if(fLiftForce > 1.0f)
				fLiftForce = rage::Max(0.0f, 1.0f - 2.0f*(fLiftForce - 1.0f));

			float& fSampleSubmergeLevel = m_fSampleSubmergeLevel[nSample];
			fLiftForce *= rage::Min(sample.m_fSize, sfWaterSamplePlaneMult*fSampleSubmergeLevel);

			fAngleOfAttack += 0.5f;
			fAngleOfAttack = Clamp(fAngleOfAttack, -1.0f, 1.0f);

			fLiftForce *= fAngleOfAttack * fSampleSubmergeLevel * fMass;
			fLiftForce *= pBuoyancyInfo->m_fLiftMult;
			if(pBoatHandling && pBoatHandling->m_fDinghySphereBuoyConst>0.0f)
			{
				// If this boat is set up like a dinghy (with extra buoyancy spheres around the inflatable edge), don't
				// allow the extra samples to provide lift while assuming a buoyancy multiplier of 1.0 for the generic samples
				// which have had their buoyancy set to zero.
				if(sample.m_fBuoyancyMult != DINGHY_BUOYANCY_MULT_FOR_GENERIC_SAMPLES)
					fLiftForce = 0.0f;
			}
			else
			{
				fLiftForce *= sample.m_fBuoyancyMult;
			}

			Vector3 vecLiftForce = fLiftForce*vWaterNormal;

#if __BANK
			if(ms_bDisableLiftForce)
			{
				vecLiftForce.Set(0.0f);
			}
#endif // __BANK
			//fxLiftForce = vecLiftForce; // Setup FX variable.

#if __ASSERT
			// Add some extra debug info to the log if we are going to cause some of the asserts in ApplyForce to fire.
			if(!pParent->CheckForceInRange(vecLiftForce, DEFAULT_ACCEL_LIMIT))
			{
				GENERIC_FORCE_CHECK("LIFT FORCE", vecLiftForce);

				Displayf("m_fSampleSubmergeLevel[%d]: %5.3f", nSample, m_fSampleSubmergeLevel[nSample]);
				Displayf("fLiftForce=%5.3f, fAngleOfAttack=%5.3f", fLiftForce, fAngleOfAttack);
				Displayf("vecWaterNormal=(%5.3f, %5.3f, %5.3f), (magnitude=%5.3f)\n",
					vWaterNormal.x, vWaterNormal.y, vWaterNormal.z, vWaterNormal.Mag());
			}
#endif // __ASSERT
			DEBUG_SAMPLE_BUOYANCY_FORCE(nSample, "lift", vecLiftForce);

#if DEBUG_BUOYANCY
			m_fTotalLiftForce += vecLiftForce.Mag();
#endif // DEBUG_BUOYANCY

			if(fLiftForce > 0.0f)
			{
				pParent->ApplyForce(vecLiftForce, vTestPointWorld - VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()), nComponent, true, fTimeStep);
			}

			// Modify the water simulation based due to lift forces:

			// Set defaults for pushing water around.
			float fPushWaterFromLiftMult = TEST_PUSH_WATER_DOWN_FROM_LIFT_SPEED_MULT;
			float fPushWaterFromLiftCap = TEST_PUSH_WATER_DOWN_FROM_LIFT_SPEED_LIMIT;
			float fPushWaterFromLiftApplyMult = TEST_PUSH_WATER_DOWN_FROM_LIFT_APPLY_MULT;

			// If this is a boat then get handling specific values.
			if(pBoatHandling)
			{
				fPushWaterFromLiftMult = pBoatHandling->m_fAquaplanePushWaterMult;
				fPushWaterFromLiftCap =  pBoatHandling->m_fAquaplanePushWaterCap;
				fPushWaterFromLiftApplyMult = pBoatHandling->m_fAquaplanePushWaterApply;
			}

			float fSpeedDown = -m_fSampleSubmergeLevel[nSample] * rage::Min(fPushWaterFromLiftCap, fPushWaterFromLiftMult * fSpeedForLift);
			float fPushWaterDownFromLift = rage::Min(1.0f, fPushWaterFromLiftApplyMult * vecLiftForce.z);

			if(fPushWaterDownFromLift > 0.0f && fSpeedDown < fWaterSpeedVert)
				Water::ModifyDynamicWaterSpeed(vTestPointWorld.x,   vTestPointWorld.y,   fSpeedDown, fPushWaterDownFromLift);
		}
	}

	PF_STOP(ComputeLiftForce);
}

void CBuoyancy::ComputeSampleKeelForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
									   const Vector3& vLocalPosition, const Vector3& vWaterNormal, const Vector3& vSpeedAtPoint,
									   const Vector3& vSpeedAtPointIncFlow, const Vector3& vFlowVelocity)
{
	PF_START(ComputeKeelForce);

	CBuoyancyInfo* pBuoyancyInfo = GetBuoyancyInfo(pParent);

	CVehicle* pVehicle = NULL;
	CBoatHandlingData* pBoatHandling = NULL;
	if(pParent->GetIsTypeVehicle() && 
		( static_cast<CVehicle*>(pParent)->InheritsFromBoat() ||
		  static_cast<CVehicle*>(pParent)->InheritsFromAmphibiousAutomobile() ) )
	{
		pVehicle = static_cast<CVehicle*>(pParent);
		pBoatHandling = static_cast<CVehicle*>(pParent)->pHandling->GetBoatHandlingData();
	}

	// If this is a boat which is set up like a dinghy (with extra buoyancy spheres around the inflatable edge), don't
	// allow the extra samples to provide a keel force.
	if(pBoatHandling && pBoatHandling->m_fDinghySphereBuoyConst>0.0f)
	{
		if(pBuoyancyInfo->m_WaterSamples[nSample].m_fBuoyancyMult != DINGHY_BUOYANCY_MULT_FOR_GENERIC_SAMPLES)
			return;
	}

	Vector3 vecSide;
	vecSide.Cross(VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()), vWaterNormal);
	vecSide.NormalizeFast();

	Vector3 vecForward;
	vecForward.Cross(vWaterNormal, VEC3V_TO_VECTOR3(pParent->GetTransform().GetA()));
	vecForward.NormalizeFast();

	float fSideSpeed = vSpeedAtPoint.Dot(vecSide);
	float fSideSpeedIncFlow = vSpeedAtPointIncFlow.Dot(vecSide);
	float fKeelForce = -fSideSpeed * pBuoyancyInfo->m_fKeelMult * fMass;

	// Boats have special treatment for the keel force to help them turn around quicker in fast flowing
	// rivers even when low throttle is applied.
	float fKeelForceFactor = 1.0f;
	float fKeelForceAtRudderFactor = 1.0f;
    float fKeelDragMult = ms_fKeelDragMult;

	if(pBoatHandling)
	{
		// If these Keel spheres have been set up explicitly scale the keel force by submerge level.
		if(pBoatHandling && pBoatHandling->m_fKeelSphereSize > 0.0f)
		{
			fKeelForce *= m_fSampleSubmergeLevel[nSample];
        }

		float fKeelRudderExclusionFraction = ms_fMaxKeelRudderExclusionFraction;
		float fThrottle = fabs(static_cast<CVehicle*>(pParent)->GetThrottle());
		if(fThrottle > ms_fKeelForceThrottleThreshold)
		{
			float fInterpX = (fThrottle-ms_fKeelForceThrottleThreshold)*1.0f/(1.0f-ms_fKeelForceThrottleThreshold);
			fKeelForceFactor = ms_fMinKeelForceFactor*(1.0f-fInterpX) + fInterpX;
			// Square the computed coefficient to reduce the effects of the river on the keel force at
			// lower throttle values.
			fKeelForceFactor *= fKeelForceFactor;
			fKeelForceFactor = Clamp(fKeelForceFactor, ms_fMinKeelForceFactor, 1.0f);
		}

		// We want to reduce the keel force for buoyancy samples which are too close
		// to the rudder under certain conditions as this counteracts the turning force.
		float fBackOfBoatY = pParent->GetBoundingBoxMin().y;
		float fBoatLength = pParent->GetBoundingBoxMax().y-pParent->GetBoundingBoxMin().y;
		float fKeelRudderExclusionLength = fKeelRudderExclusionFraction*fBoatLength;
		if(vLocalPosition.y < (fBackOfBoatY+fKeelRudderExclusionLength))
		{
			if(vFlowVelocity.Mag2()>0.001f)
			{
				// Work out proportion of the flow of the water which is perpendicular to the keel.
				float fSideFraction = fabs(vFlowVelocity.Dot(VEC3V_TO_VECTOR3(pParent->GetTransform().GetA())));
				fSideFraction /= fabs(vFlowVelocity.Mag());
				fKeelForceAtRudderFactor = 1.0f - fSideFraction;
				// Blend the rudder keel force using a quartic interpolation.
				fKeelForceAtRudderFactor *= fKeelForceAtRudderFactor;
				fKeelForceAtRudderFactor *= fKeelForceAtRudderFactor;
				// Full keel force at rudder when moving almost parallel with the flow.
				if(fKeelForceAtRudderFactor > ms_fFullKeelForceAtRudderLimit)
					fKeelForceAtRudderFactor = 1.0f;

				fKeelForceAtRudderFactor = Clamp(fKeelForceAtRudderFactor, 0.0f, 1.0f);
			}
		}

        // If these Keel spheres have been set up reduce the keel force when not turning as its causing issues with the boats self steering
        if(pVehicle && pBoatHandling && pBoatHandling->m_fKeelSphereSize > 0.0f)
        {
            float fSteering = fabs(pVehicle->GetSteerAngle())/pVehicle->pHandling->m_fSteeringLock;
            fKeelForceAtRudderFactor *= Clamp(fSteering, ms_fKeelForceFactorSteerMultKeelSpheres, 1.0f);
        }

	}
	// Additional keel force due to the flow.
	fKeelForce -= (fSideSpeedIncFlow * fKeelDragMult * fMass) * fKeelForceFactor;


	// This will reduce the keel force for any buoyancy spheres close to the rudder.
	fKeelForce *= fKeelForceAtRudderFactor;

#if __BANK
	if(ms_bDisableKeelForce)
	{
		fKeelForce = 0.0f;
	}
#endif // __BANK

#if __ASSERT
	// Add some extra debug info to the log if we are going to cause some of the asserts in ApplyForce to fire.
	Vector3 vForce = fKeelForce*vecSide;
	if(!pParent->CheckForceInRange(vForce, DEFAULT_ACCEL_LIMIT))
	{
		GENERIC_FORCE_CHECK("KEEL FORCE", vForce);

		Displayf("m_fSampleSubmergeLevel[%d]: %5.3f", nSample, m_fSampleSubmergeLevel[nSample]);
		Displayf("fKeelForce=%5.3f, fSideSpeed=%5.3f, fSideSpeedIncFlow=%5.3f, fKeelDragMult=%5.3f, m_fKeelMult=%5.3f, fMass=%5.3f",
			fKeelForce, fSideSpeed, fSideSpeedIncFlow, fKeelDragMult, pBuoyancyInfo->m_fKeelMult, fMass);
		Displayf("vSpeedAtPoint=(%5.3f, %5.3f, %5.3f), (magnitude=%5.3f)",
			vSpeedAtPoint.x, vSpeedAtPoint.y, vSpeedAtPoint.z, vSpeedAtPoint.Mag());
		Displayf("vSpeedAtPointIncFlow=(%5.3f, %5.3f, %5.3f), (magnitude=%5.3f)\n",
			vSpeedAtPointIncFlow.x, vSpeedAtPointIncFlow.y, vSpeedAtPointIncFlow.z, vSpeedAtPointIncFlow.Mag());
	}
#endif // __ASSERT
	DEBUG_SAMPLE_BUOYANCY_FORCE(nSample, "keel", fKeelForce*vecSide);

	Vector3 vKeelForce = fKeelForce*vecSide;

#if __WIN32PC
	m_fKeelSideForce = fKeelForce;
#endif // __WIN32PC

#if DEBUG_BUOYANCY
	m_fTotalKeelForce += vKeelForce.Mag();
#endif // DEBUG_BUOYANCY

	pParent->ApplyForce(vKeelForce, vTestPointWorld - VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()), nComponent, true, fTimeStep);

	PF_STOP(ComputeKeelForce);
}

// Compute the drag forces due to motion through the water. Takes into account the relative velocity of the sample w.r.t. the flow.
Vector3 CBuoyancy::ComputeSampleDragForce(CPhysical* pParent, SForceAccumulator* forceAccumulators, float fMass, s32 nSample, s32 nComponent,
									   const Vector3& vTestPointWorld, const Vector3& vLocalFlowVelocity, float fCrossSectionalLength,
									   float fDragCoefficient)
{
	PF_START(ComputeDragForce);

	SForceAccumulator& forceAccumulator = forceAccumulators[m_componentIndexToForceAccumMap[nComponent]];
	CBuoyancyInfo* pBuoyancyInfo = GetBuoyancyInfo(pParent);

	// Compute the force induced by the flow on this buoyancy sample.
	Vector3 vFlowInducedForce = vLocalFlowVelocity;
	float fVelocityDiffSqr = vFlowInducedForce.Mag2();
	vFlowInducedForce.NormalizeSafe();
	float fFlowInducedForceMag = fDragCoefficient*fCrossSectionalLength*fVelocityDiffSqr/pBuoyancyInfo->m_nNumWaterSamples;
	// Objects could have high rotational velocity or be falling from a height which would generate a force which
	// exceeds our limits on the maximum acceleration generated by a force so clamp this force below that.
	fFlowInducedForceMag = rage::Clamp(fFlowInducedForceMag, 0.0f, 0.9f*DEFAULT_ACCEL_LIMIT*fMass);
	vFlowInducedForce.Scale(-1.0f*fFlowInducedForceMag);

	// Reduce any vertical flow induced drag on pickups as the extra collision sphere inflates the cross-section
	// seen by the flow and can cause some low mass pickups to jump out of the water.
	static dev_float sfPickupReductionFactor = 0.1f;
	if(pParent->GetIsTypeObject() && static_cast<CObject*>(pParent)->m_nObjectFlags.bIsPickUp)
	{
		vFlowInducedForce.z *= sfPickupReductionFactor;
	}

	// Reduced drag on bikes.
	if(pParent->GetIsTypeVehicle() && static_cast<CVehicle*>(pParent)->InheritsFromBike())
	{
		vFlowInducedForce *= (m_fSampleSubmergeLevel[nSample]*m_fSampleSubmergeLevel[nSample]);
	}

#if __BANK
	if(ms_bDisableFlowInducedForce)
	{
		vFlowInducedForce.Set(0.0f);
	}
#endif // __BANK

#if __ASSERT
	// Add some extra debug info to the log if we are going to cause some of the asserts in ApplyForce to fire.
	if(!pParent->CheckForceInRange(vFlowInducedForce, DEFAULT_ACCEL_LIMIT))
	{
		GENERIC_FORCE_CHECK("FLOW-INDUCED DRAG FORCE", vFlowInducedForce);

		Displayf("m_fSampleSubmergeLevel[%d]: %5.3f", nSample, m_fSampleSubmergeLevel[nSample]);
		Displayf("fDragCoefficient=%5.3f, fCrossSectionalLength=%5.3f, fVelocityDiffSqr=%5.3f",
			fDragCoefficient, fCrossSectionalLength, fVelocityDiffSqr);
		Displayf("vLocalFlowVelocity=(%5.3f, %5.3f, %5.3f), (magnitude=%5.3f)\n",
			vLocalFlowVelocity.x, vLocalFlowVelocity.y, vLocalFlowVelocity.z, vLocalFlowVelocity.Mag());
	}
#endif // __ASSERT
	DEBUG_SAMPLE_BUOYANCY_FORCE(nSample, "drag (flow)", vFlowInducedForce);

#if DEBUG_BUOYANCY                
	m_fTotalFlowDragForce += vFlowInducedForce.Mag();
#endif // DEBUG_BUOYANCY

	forceAccumulator.vDragForce += vFlowInducedForce;

	Vector3 vOffsetFromComponentCG;
	ComputeSampleOffsetFromComponentCG(pParent, 0/*nComponent*/, vTestPointWorld, vOffsetFromComponentCG);
	Vector3 vDragTorque;
	vDragTorque.Cross(vOffsetFromComponentCG, vFlowInducedForce);
	forceAccumulator.vDragTorque += vDragTorque;

	PF_STOP(ComputeDragForce);

	return vFlowInducedForce;
}

// Compute the damping forces due to motion through the water.
// TODO - Merge with drag force (i.e. the one which takes into account relative velocity of the sample to the water)?
Vector3 CBuoyancy::ComputeSampleDampingForce(CPhysical* pParent, SForceAccumulator* forceAccumulators, float fMass, s32 nSample, s32 nComponent,
										  const Vector3& vTestPointWorld, const Vector3& vSpeedAtPoint, float fPartBuoyancyMultiplier)
{
	PF_START(ComputeDampingForce);

	SForceAccumulator& forceAccumulator = forceAccumulators[m_componentIndexToForceAccumMap[nComponent]];
	CBuoyancyInfo* pBuoyancyInfo = GetBuoyancyInfo(pParent);

	Vector3 vClampedSpeedAtPoint = vSpeedAtPoint;
	if(vClampedSpeedAtPoint.Mag2() > MAX_SPEED_FOR_DRAG*MAX_SPEED_FOR_DRAG)
		vClampedSpeedAtPoint *= MAX_SPEED_FOR_DRAG / vSpeedAtPoint.Mag();

	// Separate into horizontal and vertical components.
	float fDragMultXY =  pBuoyancyInfo->m_fDragMultXY;
	float fDragMultZUp = pBuoyancyInfo->m_fDragMultZUp;
	float fDragMultZDown = pBuoyancyInfo->m_fDragMultZDown;

	float fSubmergeLevel = m_fSampleSubmergeLevel[nSample];
	if(pParent->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pParent);
		if(pVehicle->InheritsFromBike())
		{
			if(fSubmergeLevel < 0.5f*pBuoyancyInfo->m_WaterSamples[nSample].m_fSize)
			{
				PF_STOP(ComputeDampingForce);
				return Vector3(VEC3_ZERO);
			}
		}
		else if(!pVehicle->InheritsFromPlane())
		{
			if(fSubmergeLevel < 0.4f*pBuoyancyInfo->m_WaterSamples[nSample].m_fSize)
			{
				PF_STOP(ComputeDampingForce);
				return Vector3(VEC3_ZERO);
			}

			if(Unlikely(pVehicle->GetIsJetSki() && !pVehicle->GetDriver() && !pVehicle->m_Buoyancy.GetRiverBoundProbeResult()))
			{
				BankFloat kfAbandonedJetSkiDragMultiplier = 3.0f;
				fDragMultXY *= kfAbandonedJetSkiDragMultiplier;
			}
		}
	}

	Vector3 vecDragForceXY = vClampedSpeedAtPoint;
	vecDragForceXY.And(VEC3_ANDZ); // Zero Z-component.
	// Horizontal drag should be uniform in direction.
	vecDragForceXY.Normalize();
	float fSpeedMag2 = vClampedSpeedAtPoint.XYMag2();
	vecDragForceXY.Scale(-fDragMultXY * fSpeedMag2 * fSubmergeLevel * fPartBuoyancyMultiplier);

	// Vertical drag has different values for moving down into the water, or up out of the water.
	float fDragForceZ = -vClampedSpeedAtPoint.z * rage::Abs(vClampedSpeedAtPoint.z);
	if(fDragForceZ > 0.0f)
		fDragForceZ *= fDragMultZUp;
	else
		fDragForceZ *= fDragMultZDown;

	static dev_float sfWaterSampleZDragMult = 10.0f;
	float fWaterSampleDragMult = 1.0f;
	if(fDragForceZ > 0.0f)
		fWaterSampleDragMult = sfWaterSampleZDragMult;
	fDragForceZ *= rage::Min(pBuoyancyInfo->m_WaterSamples[nSample].m_fSize,
		fWaterSampleDragMult*m_fSampleSubmergeLevel[nSample]*fPartBuoyancyMultiplier);

	// Combine X, Y and Z-components again.
	Vector3 vecDragForce = vecDragForceXY;
	vecDragForce.z = fDragForceZ;

	float fDragForceMag = vecDragForce.Mag();
	static float DRAG_ACCEL_LIMIT = 100.0f;
	fDragForceMag = rage::Clamp(fDragForceMag, 0.0f, 0.9f*DRAG_ACCEL_LIMIT*fMass);
	vecDragForce.NormalizeSafe();
	vecDragForce.Scale(fDragForceMag);

#if __BANK
	if(ms_bDisableDragForce)
	{
		vecDragForce.Set(0.0f);
	}
#endif // __BANK

#if __ASSERT
	// Add some extra debug info to the log if we are going to cause some of the asserts in ApplyForce to fire.
	if(!pParent->CheckForceInRange(vecDragForce, DEFAULT_ACCEL_LIMIT))
	{
		GENERIC_FORCE_CHECK("BASIC DRAG FORCE", vecDragForce);

		Displayf("m_fSampleSubmergeLevel[%d]: %5.3f", nSample, m_fSampleSubmergeLevel[nSample]);
		Displayf("fMass=%5.3f, fDragForceZ=%5.3f, fDragMultXY=%5.3f, fDragMultZUp=%5.3f, fDragMultZDown=%5.3f",
			fMass, fDragForceZ, fDragMultXY, fDragMultZUp, fDragMultZDown);
		Displayf("vClampedSpeedAtPoint=(%5.3f, %5.3f, %5.3f), (magnitude=%5.3f)\n",
			vClampedSpeedAtPoint.x, vClampedSpeedAtPoint.y, vClampedSpeedAtPoint.z, vClampedSpeedAtPoint.Mag());
	}
#endif // __ASSERT
	DEBUG_SAMPLE_BUOYANCY_FORCE(nSample, "drag (basic)", vecDragForce);

#if DEBUG_BUOYANCY
	m_fTotalBasicDragForce += vecDragForce.Mag();
#endif // DEBUG_BUOYANCY

	forceAccumulator.vDampingForce += vecDragForce;

	Vector3 vOffsetFromComponentCG;
	ComputeSampleOffsetFromComponentCG(pParent, 0/*nComponent*/, vTestPointWorld, vOffsetFromComponentCG);
	Vector3 vDampingTorque;
	vDampingTorque.Cross(vOffsetFromComponentCG, vecDragForce);
	forceAccumulator.vDampingTorque += vDampingTorque;

	PF_STOP(ComputeDampingForce);

	return vecDragForce;
}

void CBuoyancy::ComputeSampleStickToSurfaceForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
												 const Vector3& vSpeedAtPointIncFlow)
{
	PF_START(ComputeStickToSurfaceForce);

	CBuoyancyInfo* pBuoyancyInfo = GetBuoyancyInfo(pParent);

	if(m_buoyancyFlags.bShouldStickToWaterSurface)
	{
		static dev_float fDesiredSubmergeFraction = 0.75f;
		static dev_float fStickSpringStrengthBase = 10.0f;

		const float fStickSpringStrength = fStickSpringStrengthBase * fMass / (float)pBuoyancyInfo->m_nNumWaterSamples;
		const float fStickSpringDamping = -2.0f*rage::Sqrtf(fStickSpringStrength * 9.81f);

		if(m_nNumInSampleLevelArray > 0)
		{
			Vector3 vStickForce = VEC3_ZERO;
			vStickForce.z = fStickSpringStrength * ((m_fSampleSubmergeLevel[nSample] / pBuoyancyInfo->m_WaterSamples[nSample].m_fSize)
				- fDesiredSubmergeFraction);
			vStickForce.z += vSpeedAtPointIncFlow.z * fStickSpringDamping;

#if __BANK
			if(ms_bDisableSurfaceStickForce)
			{
				vStickForce.Set(0.0f);
			}
#endif // __BANK

#if __ASSERT
			// Add some extra debug info to the log if we are going to cause some of the asserts in ApplyForce to fire.
			if(!pParent->CheckForceInRange(vStickForce, DEFAULT_ACCEL_LIMIT))
			{
				GENERIC_FORCE_CHECK("STICK-TO-SURFACE FORCE", vStickForce);

				Displayf("m_fSampleSubmergeLevel[%d]: %5.3f", nSample, m_fSampleSubmergeLevel[nSample]);
				Displayf("fStickSpringStrength=%5.3f, fMass=%5.3f, sample size=%5.3f, fStickSpringDamping=%5.3f\n",
					fStickSpringStrength, fMass, pBuoyancyInfo->m_WaterSamples[nSample].m_fSize, fStickSpringDamping);
				Displayf("vSpeedAtPointIncFlow=(%5.3f, %5.3f, %5.3f), (magnitude=%5.3f)",
					vSpeedAtPointIncFlow.x, vSpeedAtPointIncFlow.y, vSpeedAtPointIncFlow.z, vSpeedAtPointIncFlow.Mag());
			}
#endif // __ASSERT
			DEBUG_SAMPLE_BUOYANCY_FORCE(nSample, "surface stick", vStickForce);
			pParent->ApplyForce(vStickForce, vTestPointWorld-VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()), nComponent, true, fTimeStep);
		}
	}

	PF_STOP(ComputeStickToSurfaceForce);
}
///////////////////////////////////////////////////////////////////////////////////////////////

bool CBuoyancy::CanUseLowLodBuoyancyMode(const CPhysical* pPhysical) const
{
	// Only CObjects should be using low LOD buoyancy mode right now.
	physicsAssert(pPhysical->GetIsTypeObject());
	const CObject* pObject = static_cast<const CObject*>(pPhysical);

	if(pObject->m_nObjectFlags.bIsPickUp)
		return false;

	if(pObject->IsAScriptEntity() && !m_buoyancyFlags.bScriptLowLodOverride)
		return false;

	if(pObject->IsADoor())
		return false;

	if(pObject->GetAsProjectile())
		return false;

	// Don't want parts which break off vehicles to float on the surface (e.g. tail parts from helicoptors
	// shot down over water) so don't allow them to use low LOD buoyancy.
	if(pObject->m_nObjectFlags.bVehiclePart)
		return false;

	if(m_waterLevelStatus==NOT_IN_WATER)
		return false;

	return true;
}

void CBuoyancy::UpdateBuoyancyLodMode(CPhysical* pPhysical)
{
	PF_START(ProcessLowLodBuoyancyTimer);

	Matrix34 entMat = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
	float fWaterZAtEntPos;
	bool bNearWater = GetWaterLevelIncludingRivers(entMat.d, &fWaterZAtEntPos, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPhysical) != WATERTEST_TYPE_NONE;
	if(!bNearWater)
	{
		PF_STOP(ProcessLowLodBuoyancyTimer);
		PF_SETTIMER(ProcessBuoyancyTotal, PF_READ_TIME(ProcessBuoyancy) + PF_READ_TIME(ProcessLowLodBuoyancyTimer));
		return;
	}

	float fBuoyancyLodDist = ms_fObjectLodDistance;
	bool bIgnoreCollisionInLowLodMode = false;
	if(const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(pPhysical->GetBaseModelInfo()->GetModelNameHash()))
	{
		// Check if this object overrides the LOD distance.
		if(pTuning->GetLowLodBuoyancyDistance() >= 0.0f)
		{
			fBuoyancyLodDist = pTuning->GetLowLodBuoyancyDistance();
		}

		// Small objects in low-LOD buoyancy mode can be configured to ignore collisions.
		if(pTuning->IgnoreCollisionInLowLodBuoyancyMode())
		{
			bIgnoreCollisionInLowLodMode = true;
		}
	}
	if(CPed* pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer())
	{
		float fDistanceSqToPlayer = MagSquared(pPhysical->GetTransform().GetPosition() - pLocalPlayer->GetTransform().GetPosition()).Getf();
		if(fDistanceSqToPlayer > fBuoyancyLodDist*fBuoyancyLodDist && bNearWater)
		{
			if(!m_buoyancyFlags.bLowLodBuoyancyMode && CanUseLowLodBuoyancyMode(pPhysical))
			{
				// Switch to low LOD buoyancy mode - deactivate the physics instance and set the flag so the matrix gets
				// moved with the water surface.
				m_buoyancyFlags.bWasActiveWhenLodded = false;
				if(pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->IsInLevel()
					&& CPhysics::GetLevel()->IsActive(pPhysical->GetCurrentPhysicsInst()->GetLevelIndex()))
				{
					m_buoyancyFlags.bWasActiveWhenLodded = true;
					CPhysics::GetSimulator()->DeactivateObject(pPhysical->GetCurrentPhysicsInst()->GetLevelIndex());
				}

				if(bIgnoreCollisionInLowLodMode)
				{
					pPhysical->DisableCollision();
				}

				// Cache the vertical offset of the boat with respect to the water height at the entity position
				// so we get the draught right.
				SetLowLodDraughtOffset(entMat.d.z-fWaterZAtEntPos);

				SetTimeInLowLodMode((float)fwTimer::GetTimeInMilliseconds()/1000.0f);

				m_buoyancyFlags.bLowLodBuoyancyMode = true;
			}
		}
		else
		{
			if(m_buoyancyFlags.bLowLodBuoyancyMode)
			{
				// Switch back to full buoyancy mode.

				if(m_buoyancyFlags.bWasActiveWhenLodded)
				{
					phCollider* pCollider = pPhysical->GetCollider();
					// check if this vehicle is asleep, and wake up when necessary
					if(!pCollider || (pCollider->GetSleep() && pCollider->GetSleep()->IsAsleep()))
					{
						if(pCollider)
						{
							pCollider->GetSleep()->Reset(true);
						}
						else
						{
							pPhysical->ActivatePhysics();
						}
					}
				}

				if(bIgnoreCollisionInLowLodMode)
				{
					pPhysical->EnableCollision();
				}

				m_buoyancyFlags.bLowLodBuoyancyMode = false;
			}
		}
	}
	PF_STOP(ProcessLowLodBuoyancyTimer);
	PF_SETTIMER(ProcessBuoyancyTotal, PF_READ_TIME(ProcessBuoyancy) + PF_READ_TIME(ProcessLowLodBuoyancyTimer));
}

void CBuoyancy::RejuvenateMatrixIfNecessary(Matrix34& matrix)
{
	// See how far away from normal and orthogonal the matrix gets and renormalise it if necessary.
	if(!matrix.IsOrthonormal(REJUVENATE_ERROR))
	{
#if __ASSERT
		Displayf("[CBuoyancy::ProcessLowLodBuoyancy] Non orthonormal matrix detected. Rejuvenating...");
#endif // __ASSERT

		if(matrix.a.IsNonZero() && matrix.b.IsNonZero() && matrix.c.IsNonZero())
		{
			matrix.b.Normalize();
			matrix.c.Cross(matrix.a, matrix.b);
			matrix.c.Normalize();
			matrix.a.Cross(matrix.b, matrix.c);
		}
		else
		{
			// Protection against very bad matrices...
			matrix.Identity3x3();
		}
	}
}

void CBuoyancy::ProcessLowLodBuoyancy(CPhysical* pPhysical)
{
#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld())
		return;
#endif // GTA_REPLAY

	// If a boat is in low LOD anchor mode, it is physically inactive and we place it based on the water level sampled at
	// points around the edges of the bounding box.

	PF_START(ProcessLowLodBuoyancyTimer);
	if(m_buoyancyFlags.bLowLodBuoyancyMode)
	{
		const Vector3 vOriginalPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());

		// If this physical has woken up, force it back to sleep.
		if(pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->IsInLevel()
			&& CPhysics::GetLevel()->IsActive(pPhysical->GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			CPhysics::GetSimulator()->DeactivateObject(pPhysical->GetCurrentPhysicsInst()->GetLevelIndex());
		}

#if __BANK
		Color32 colour = Color_yellow;
#endif // __BANK
		
		float fWaterZAtEntPos = 0.0f;
		if(GetWaterLevelIncludingRivers(vOriginalPosition, &fWaterZAtEntPos, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pPhysical))
		{
			// If we know the optimum stable height offset for this object and it isn't at that offset yet, LERP it
			// there over a few frames.
			float fLowLodDraughtOffset = GetLowLodDraughtOffset();
			if(const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(pPhysical->GetBaseModelInfo()->GetModelNameHash()))
			{
				if(fabs(pTuning->GetLowLodBuoyancyHeightOffset()) > 0.0f)
				{
					float fDifferenceToIdeal = pTuning->GetLowLodBuoyancyHeightOffset() - fLowLodDraughtOffset;
					if(fabs(fDifferenceToIdeal) > 0.001f)
					{
						dev_float sfStabiliseFactor = 0.1f;
						fLowLodDraughtOffset += sfStabiliseFactor * fDifferenceToIdeal;
						SetLowLodDraughtOffset(fLowLodDraughtOffset);
					}
				}
			}

			// Now we actually move the object to simulate buoyancy physics.
			Matrix34 entMat = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
			Assertf(g_WorldLimits_AABB.ContainsPoint(VECTOR3_TO_VEC3V(entMat.d)), "%s is already out of the world. Coords (%5.3f, %5.3f, %5.3f).",
				pPhysical->GetModelName(), entMat.d.x, entMat.d.y, entMat.d.z);
			Vector3 vPlaneNormal;
			ComputeLowLodBuoyancyPlaneNormal(pPhysical, entMat, vPlaneNormal);
			// Figure out how much to rotate the object's matrix by to match the plane's orientation.
			Vector3 vRotationAxis;
			Assertf(entMat.c.x == entMat.c.x && entMat.c.y == entMat.c.y && entMat.c.z == entMat.c.z,
				"entMat.c has non-finite component: (%5.3f, %5.3f, %5.3f)", entMat.c.x, entMat.c.y, entMat.c.z);
			Assertf(vPlaneNormal.x == vPlaneNormal.x && vPlaneNormal.y == vPlaneNormal.y && vPlaneNormal.z == vPlaneNormal.z,
				"vPlaneNormal has non-finite component: (%5.3f, %5.3f, %5.3f)", vPlaneNormal.x, vPlaneNormal.y, vPlaneNormal.z);
			vRotationAxis.Cross(entMat.c, vPlaneNormal);
			vRotationAxis.NormalizeFast();

			float fRotationAngle = entMat.c.Angle(vPlaneNormal);

			// If we are far away from the equilibrium orientation for this object (because of big waves or other external
			// forces) we limit the amount we move towards the ideal position each frame. Only do this for
			// the first few seconds after switching to low LOD mode or it damps the motion when following the waves.
			const float kfTimeToLerpAfterSwitchToLowLod = 2.5f;
			float fTimeSpentLerping = fwTimer::GetTimeInMilliseconds()/1000.0f - GetTimeInLowLodMode();
			if(fTimeSpentLerping < kfTimeToLerpAfterSwitchToLowLod)
			{
				float fX = fTimeSpentLerping/kfTimeToLerpAfterSwitchToLowLod;
				float kfStabiliseFactor = rage::Min(fX*fX, 1.0f);
				fRotationAngle *= kfStabiliseFactor;
			}
			if(fabs(fRotationAngle) > SMALL_FLOAT && vRotationAxis.FiniteElements() && vRotationAxis.IsNonZero())
			{
				entMat.RotateUnitAxis(vRotationAxis, fRotationAngle);
				RejuvenateMatrixIfNecessary(entMat);
				pPhysical->SetMatrix(entMat);
			}
			float fNewZPos = fWaterZAtEntPos+fLowLodDraughtOffset;
			pPhysical->SetPosition(Vector3(vOriginalPosition.x, vOriginalPosition.y, fNewZPos));
			m_vLowLodVelocityXYZLowLodTimerW.Set(0.0f, 0.0f, (fNewZPos - vOriginalPosition.z)*fwTimer::GetInvTimeStep());

			// Update the "is in water" flag as would be done in CBuoyancy::Process().
			pPhysical->SetIsInWater(true);
		}
#if __BANK
		else
		{
			// Object's entity matrix is not in water if we get here.
			colour = Color_green;
			pPhysical->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = false;
		}
#endif // __BANK

#if __BANK
		// Indicate this boat is in low LOD anchor mode for debugging (green = low LOD mode but not being processed
		// because it's not in the water, yellow = low LOD mode and actively being moved by this function).
		if(CBuoyancy::ms_bIndicateLowLODBuoyancy)
		{
			grcDebugDraw::Sphere(vOriginalPosition + Vector3(0.0f, 0.0f, 2.0f), 1.0f, colour);
		}
#endif // __BANK
	}
	PF_STOP(ProcessLowLodBuoyancyTimer);
	PF_SETTIMER(ProcessBuoyancyTotal, PF_READ_TIME(ProcessBuoyancy) + PF_READ_TIME(ProcessLowLodBuoyancyTimer));
}

void CBuoyancy::ComputeLowLodBuoyancyPlaneNormal(const CPhysical* pPhysical, const Matrix34& entMat, Vector3& vPlaneNormal)
{
	// Work out the position of some test points in a triangle around the bounding box.
	Vector3 vBoundBoxMin = pPhysical->GetBoundingBoxMin();
	Vector3 vBoundBoxMax = pPhysical->GetBoundingBoxMax();

	const int nNumVerts = 3;
	Vector3 vBoundBoxBottomVerts[nNumVerts];
	vBoundBoxBottomVerts[0] = Vector3(vBoundBoxMin.x, vBoundBoxMin.y, vBoundBoxMin.z);
	vBoundBoxBottomVerts[1] = Vector3(vBoundBoxMax.x, vBoundBoxMin.y, vBoundBoxMin.z);
	vBoundBoxBottomVerts[2] = Vector3(0.5f*(vBoundBoxMin.x+vBoundBoxMax.x), vBoundBoxMax.y, vBoundBoxMin.z);

	for(int nVert = 0; nVert < nNumVerts; ++nVert)
	{
		entMat.Transform(vBoundBoxBottomVerts[nVert]);
		float fWaterZ = 0.0f;
		CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vBoundBoxBottomVerts[nVert], &fWaterZ);
		vBoundBoxBottomVerts[nVert].z = fWaterZ;

#if __BANK
		if(CBuoyancy::ms_bIndicateLowLODBuoyancy)
		{
			grcDebugDraw::Sphere(vBoundBoxBottomVerts[nVert], 0.1f, Color_red);
		}
#endif // __BANK
	}

	// Use the verts above to define a plane which can be used to set the orientation of the object.
	vPlaneNormal.Cross(vBoundBoxBottomVerts[0]-vBoundBoxBottomVerts[1], vBoundBoxBottomVerts[0]-vBoundBoxBottomVerts[2]);
	// If the object was upside-down, we might get a normal which is upside-down too. Reverse it in that case.
	if(vPlaneNormal.z < 0.0f)
	{
		vPlaneNormal.Scale(-1.0f);
	}

#if __ASSERT
	if(!Verifyf(vPlaneNormal.x == vPlaneNormal.x && vPlaneNormal.y == vPlaneNormal.y && vPlaneNormal.z == vPlaneNormal.z,
		"vPlaneNormal has non-finite component: (%5.3f, %5.3f, %5.3f)", vPlaneNormal.x, vPlaneNormal.y, vPlaneNormal.z))
	{
		Displayf("Entity: '%s'", pPhysical->GetModelName());
		Displayf("vBoundBoxMin: (%5.3f, %5.3f, %5.3f)", vBoundBoxMin.x, vBoundBoxMin.y, vBoundBoxMin.z);
		Displayf("vBoundBoxMax: (%5.3f, %5.3f, %5.3f)", vBoundBoxMax.x, vBoundBoxMax.y, vBoundBoxMax.z);
		Displayf("---------- entMat: ----------");
		Displayf("(%5.3f, %5.3f, %5.3f)", entMat.a.x, entMat.a.y, entMat.a.z);
		Displayf("(%5.3f, %5.3f, %5.3f)", entMat.b.x, entMat.b.y, entMat.b.z);
		Displayf("(%5.3f, %5.3f, %5.3f)", entMat.c.x, entMat.c.y, entMat.c.z);
		Displayf("(%5.3f, %5.3f, %5.3f)", entMat.d.x, entMat.d.y, entMat.d.z);
		Displayf("-----------------------------");
		Displayf("vBoundBoxBottomVerts[0]: (%5.3f, %5.3f, %5.3f)", vBoundBoxBottomVerts[0].x, vBoundBoxBottomVerts[0].y, vBoundBoxBottomVerts[0].z);
		Displayf("vBoundBoxBottomVerts[1]: (%5.3f, %5.3f, %5.3f)", vBoundBoxBottomVerts[1].x, vBoundBoxBottomVerts[1].y, vBoundBoxBottomVerts[1].z);
		Displayf("vBoundBoxBottomVerts[2]: (%5.3f, %5.3f, %5.3f)", vBoundBoxBottomVerts[2].x, vBoundBoxBottomVerts[2].y, vBoundBoxBottomVerts[2].z);
	}
#endif // __ASSERT
}

void CBuoyancy::ResetBuoyancy()
{
	//Assert(m_fSampleSubmergeLevel);
	if(m_fSampleSubmergeLevel)
	{
		for(int i=0; i<m_nNumInSampleLevelArray; i++)
		{
			// reset values
			m_fSampleSubmergeLevel[i] = 0.0f;
		}
	}
	m_buoyancyFlags.bBuoyancyInfoUpToDate = false;
}

float CBuoyancy::GetWaterLevelOnSample(int iSampleIndex) const
{
	if(m_fSampleSubmergeLevel != NULL)
	{
		Assert(iSampleIndex < m_nNumInSampleLevelArray);
		return m_fSampleSubmergeLevel[iSampleIndex];
	}
	else
	{
		return 0.0f;
	}
}

#if __BANK


void DisplayLowLodAngularOffsetForSelectedBoat()
{
	if(CDebugScene::FocusEntities_Get(0) && CDebugScene::FocusEntities_Get(0)->GetIsTypeVehicle()
		&& static_cast<CVehicle*>(CDebugScene::FocusEntities_Get(0))->InheritsFromBoat())
	{
		CBoat* pBoat = static_cast<CBoat*>(CDebugScene::FocusEntities_Get(0));

		if(pBoat && pBoat->GetBoatHandling())
		{
			CBoat::DisplayAngularOffsetForLowLod( pBoat );
		}
	}
}

void DisplayLowLodDraughtOffsetForSelectedBoat()
{
	if(CDebugScene::FocusEntities_Get(0) && CDebugScene::FocusEntities_Get(0)->GetIsTypeVehicle()
		&& static_cast<CVehicle*>(CDebugScene::FocusEntities_Get(0))->InheritsFromBoat())
	{
		CBoat* pBoat = static_cast<CBoat*>(CDebugScene::FocusEntities_Get(0));

		if(pBoat && pBoat->GetBoatHandling())
		{
			CBoat::DisplayDraughtOffsetForLowLod( pBoat );
		}
	}
}

void CanSelectedVehicleAnchorHereCB()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if(pEnt && pEnt->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
			if(CAnchorHelper::IsVehicleAnchorable(pVehicle))
			{
				CAnchorHelper* pAnchorHelper = CAnchorHelper::GetAnchorHelperPtr(pVehicle);
				if(AssertVerify(pAnchorHelper))
				{
					pAnchorHelper->CanAnchorHere();
				}
			}
		}
	}
}

void CalmWater()
{
	Water::MakeWaterFlat();
}

void SpawnBoatsAndSelectForMeasuringLowLodAngularOffset()
{
	// * Make water flat.
	// * Generate list of all boat models.
	// * Spawn all boats in list.
	// * Wait for boats to become still.
	//
	// Now ready for user to press 'Display angular offset for all selected boats' button.

	Water::MakeWaterFlat();
	physicsDisplayf("List of all boats:");
	{
		fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
		atArray<CVehicleModelInfo*> typeArray;
		vehModelInfoStore.GatherPtrs(typeArray);

		int nBoatNames = typeArray.GetCount();

		const int MAX_NUM_VEHICLE_NAMES = 500;
		s32 boatList[MAX_NUM_VEHICLE_NAMES];

		int nActualNum = 0;
		for(int i=0; i < nBoatNames; ++i)
		{
			if( typeArray[i]->GetVehicleType() == VEHICLE_TYPE_BOAT || 
				typeArray[i]->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || 
				typeArray[i]->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
			{
				boatList[nActualNum] = i;
				nActualNum++;
			}
		}

		nBoatNames = nActualNum;

		atArray<const char*> vehicleNames;
		fwModelId modelId;
		for(int i=0; i < nBoatNames; ++i)
		{
			vehicleNames.PushAndGrow(typeArray[boatList[i]]->GetModelName());
			physicsDisplayf("%d, %s", i, vehicleNames[i]);

			modelId = CModelInfo::GetModelIdFromName(vehicleNames[i]);
			physicsAssert(modelId.IsValid());

			//		if(CVehicle::GetPool()->GetNoOfFreeSpaces() < 5)
			//			return NULL;

			// Stream vehicle asset in.
			u32 flags = CModelInfo::GetAssetStreamFlags(modelId);
			CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE);
			CStreaming::LoadAllRequestedObjects();

			if(CModelInfo::HaveAssetsLoaded(modelId))
			{
				// if previously vehicle was deletable, then set it to be deletable
				if(!(flags & STRFLAG_DONTDELETE))
				{
					CModelInfo::SetAssetsAreDeletable(modelId);
				}

				Vector3 vRadialDirection(VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetB()));
				vRadialDirection.RotateZ(i * 2.0f*PI/float(nBoatNames));

				TUNE_FLOAT(CIRCLE_RADIUS, 20.0f, 1.0f, 100.0f, 1.0f);
				Matrix34 CreationMatrix;
				CreationMatrix.Identity();
				CreationMatrix.d = CGameWorld::FindLocalPlayerCoors() + vRadialDirection*CIRCLE_RADIUS;
				CreationMatrix.d.z += 2.0f;

				CVehicle* pNewVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_OTHER, POPTYPE_RANDOM_AMBIENT, &CreationMatrix);

				if(pNewVehicle)
				{
					pNewVehicle->m_nVehicleFlags.bCreatedUsingCheat = true;
					pNewVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = true;

					pNewVehicle->PlaceOnRoadAdjust();

					CBaseModelInfo* pModel = pNewVehicle->GetBaseModelInfo();
					Assert(pModel->GetFragType());

					int nTestTypeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;
					int	nTestTypeFlagsCars = ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE;

					Vector3 start, end;
					pNewVehicle->TransformIntoWorldSpace(start, Vector3(0.0f, pModel->GetBoundingBoxMax().y, 0.0f));
					pNewVehicle->TransformIntoWorldSpace(end, Vector3(0.0f, pModel->GetBoundingBoxMin().y, 0.0f));
					Vector3 start2 = start + Vector3(0.0f, 0.0f, 1.0f);
					Vector3 end2 = end + Vector3(0.0f, 0.0f, 1.0f);

					Vector3 startCars, endCars;
					pNewVehicle->TransformIntoWorldSpace(startCars, Vector3(0.0f, pModel->GetBoundingBoxMax().y+2.0f, 0.0f));
					pNewVehicle->TransformIntoWorldSpace(endCars, Vector3(0.0f, pModel->GetBoundingBoxMin().y-2.0f, 0.0f));

					Vector3 startCars2, endCars2;
					pNewVehicle->TransformIntoWorldSpace(startCars2, Vector3(2.0f, 0.0f, 0.0f));
					pNewVehicle->TransformIntoWorldSpace(endCars2, Vector3(-2.0f, 0.0f, 0.0f));

					Vector3 startCars3, endCars3;
					pNewVehicle->TransformIntoWorldSpace(startCars3, Vector3(0.0f, 0.0f, 2.0f));
					pNewVehicle->TransformIntoWorldSpace(endCars3, Vector3(0.0f, 0.0f, -2.0f));

					WorldProbe::CShapeTestProbeDesc probeDesc1;
					probeDesc1.SetStartAndEnd(start, end);
					probeDesc1.SetExcludeEntity(pNewVehicle);
					probeDesc1.SetIncludeFlags(nTestTypeFlags);
					probeDesc1.SetContext(WorldProbe::LOS_Unspecified);

					WorldProbe::CShapeTestProbeDesc probeDesc2;
					probeDesc2.SetStartAndEnd(start2, end2);
					probeDesc2.SetIncludeFlags(nTestTypeFlags);
					probeDesc2.SetContext(WorldProbe::LOS_Unspecified);

					WorldProbe::CShapeTestProbeDesc probeDesc3;
					probeDesc3.SetStartAndEnd(startCars, endCars);
					probeDesc3.SetExcludeEntity(pNewVehicle);
					probeDesc3.SetIncludeFlags(nTestTypeFlagsCars);
					probeDesc3.SetContext(WorldProbe::LOS_Unspecified);

					WorldProbe::CShapeTestProbeDesc probeDesc4;
					probeDesc4.SetStartAndEnd(startCars2, endCars2);
					probeDesc4.SetExcludeEntity(pNewVehicle);
					probeDesc4.SetIncludeFlags(nTestTypeFlagsCars);
					probeDesc4.SetContext(WorldProbe::LOS_Unspecified);

					WorldProbe::CShapeTestProbeDesc probeDesc5;
					probeDesc5.SetStartAndEnd(startCars3, endCars3);
					probeDesc5.SetExcludeEntity(pNewVehicle);
					probeDesc5.SetIncludeFlags(nTestTypeFlagsCars);
					probeDesc5.SetContext(WorldProbe::LOS_Unspecified);

					CScriptEntities::ClearSpaceForMissionEntity(pNewVehicle);
					CGameWorld::Add(pNewVehicle, CGameWorld::OUTSIDE);
					pNewVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
				}
			}
		}
	}
}
#endif // __BANK

#if __BANK
void CBuoyancy::AddWidgetsToBank(bkBank& bank)
{
	// Basic buoyancy debug:
	bank.AddToggle("Draw Buoyancy Tests", &ms_bDrawBuoyancyTests);
	bank.AddToggle("Draw Buoyancy Sample Spheres", &ms_bDrawBuoyancySampleSpheres);
#if __DEV
	bank.AddButton("Reload focus buoyancy samples", CPhysics::ReloadFocusEntityWaterSamplesCB);
#endif // __DEV
	bank.AddToggle("Display submerged levels selected entity.", &CBuoyancy::ms_bDebugDisplaySubmergedLevels);
	bank.AddToggle("Display buoyancy forces", &ms_bDisplayBuoyancyForces);
	bank.AddToggle("Write to TTY", &ms_bWriteBuoyancyForceToTTY);
	bank.AddSlider("Sample ID to debug (-1 for all)", &ms_nSampleToDisplay, -1, 100, 1);

	bank.PushGroup("Anchor helper");
		bank.AddToggle("Enable anchor debug", &CAnchorHelper::ms_bDebugModeEnabled);
		bank.AddButton("Call CanAnchorHere on selected vehicle", CanSelectedVehicleAnchorHereCB);
	bank.PopGroup(); // "Anchor helper"

	bank.PushGroup("Buoyancy LOD");
		bank.AddSlider("Boat anchor LOD distance", &ms_fBoatAnchorLodDistance, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Object LOD distance", &ms_fObjectLodDistance, 0.0f, 1000.0f, 1.0f);
		bank.AddToggle("Indicate objects using low LOD buoyancy", &ms_bIndicateLowLODBuoyancy);
		bank.AddButton("Calm water", CalmWater);
		bank.AddButton("Display angular offset for selected boat", DisplayLowLodAngularOffsetForSelectedBoat);
		bank.AddButton("Display draught offset for selected boat", DisplayLowLodDraughtOffsetForSelectedBoat);
	bank.PopGroup(); // "Buoyancy LOD"

	bank.PushGroup("Disable forces");
		bank.AddToggle("Disable horizontal part of buoyancy force", &ms_bDisableBuoyancyForceXY);
		bank.AddToggle("Disable lift force", &ms_bDisableLiftForce);
		bank.AddToggle("Disable keel force", &ms_bDisableKeelForce);
		bank.AddToggle("Disable flow induced force", &ms_bDisableFlowInducedForce);
		bank.AddToggle("Disable water drag force", &ms_bDisableDragForce);
		bank.AddToggle("Disable surface stick force", &ms_bDisableSurfaceStickForce);
		bank.AddToggle("Disable ped weight belt buoyancy force", &ms_bDisablePedWeightBeltForce);
	bank.PopGroup(); // "Disable forces"

	bank.PushGroup("Keel force params");
		bank.AddSlider("Full keel force at rudder limit", &ms_fFullKeelForceAtRudderLimit, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Keel drag multiplier", &ms_fKeelDragMult, 0.0f, 10.0f, 0.01f);
        bank.AddSlider("Keel force factor steer mult", &ms_fKeelForceFactorSteerMultKeelSpheres, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Min keel force factor", &ms_fMinKeelForceFactor, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Keel force throttle threshold", &ms_fKeelForceThrottleThreshold, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Max keel rudder exclusion fraction", &ms_fMaxKeelRudderExclusionFraction, 0.0f, 10.0f, 0.1f);
	bank.PopGroup(); // "Keel force params"


	// "Water" (i.e. oceans, lakes, etc.) debug:
	// NOTHING IN THIS CATEGORY YET.


	// River specific debug:
	bank.PushGroup("Rivers");
		bank.AddToggle("Disable river probes", &CWaterTestHelper::ms_bDisableRiverProbes);
		bank.AddSlider("Distance before issuing new probe", &CWaterTestHelper::ms_fDistanceThresholdForRiverProbe, 0.0f, 3.0f, 0.01f);
		bank.AddToggle("Render probe hit position", &ms_bRiverBoundDebugDraw);

		bank.AddSlider("Default river drag coefficient", &ms_fDragCoefficient, 0.0f, 10000.0f, 0.01f);
		bank.AddSlider("River drag coefficient (peds)", &ms_fPedDragCoefficient, 0.0f, 10000.0f, 0.01f);
		bank.AddSlider("Flow velocity scale factor", &ms_fFlowVelocityScaleFactor, 0.0f, 100000.0f, 0.01f);
        bank.AddSlider("Max speed to apply river forces", &ms_fVehicleMaximumSpeedToApplyRiverForces, 0.0f, 100000.0f, 0.01f);
		bank.AddToggle("Visualise cross-sectional length seen by flow", &ms_bDebugDrawCrossSection);

		bank.PushGroup("Flow field debug");
			bank.AddToggle("Visualise flow field (Enable measuring tool first!)", &ms_bVisualiseRiverFlowField);
			bank.AddSlider("Grid spacing", &ms_fGridSpacing, 0.01f, 100.0f, 0.01f);
			bank.AddSlider("Height offset", &ms_fHeightOffset, -10.0f, 10.0f, 0.1f);
			bank.AddSlider("Vector scale factor", &ms_fScaleFactor, 0.01f, 100.0f, 0.1f);
			bank.AddSlider("Grid size X", &ms_nGridSizeX, 1, 200, 1);
			bank.AddSlider("Grid size Y", &ms_nGridSizeY, 1, 200, 1);
		bank.PopGroup(); // "Flow field debug"
	bank.PopGroup(); // "Rivers"
}

void CBuoyancy::DisplayBuoyancyInfo()
{
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if(!pEnt)
		return;

	if(!pEnt->GetIsPhysical())
		return;

	const CBuoyancy& rBuoyancy = static_cast<CPhysical*>(pEnt)->m_Buoyancy;

	if(!rBuoyancy.GetBuoyancyInfoUpdatedThisFrame())
		return;

	float fSubmergedLevel = rBuoyancy.GetSubmergedLevel();

	char zText[255];
	sprintf(zText, "Submerged level: %5.3f", fSubmergedLevel);
	Color32 colour = Color_yellow;
	grcDebugDraw::Text(VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition())+Vector3(0.0f,0.0f,0.0f), colour, zText);
}
#endif //__BANK

CBuoyancyInfo* CBuoyancy::GetBuoyancyInfo( const CPhysical* pParent ) const
{
	Assert(pParent);

	if(m_pBuoyancyInfoOverride)
	{
		return m_pBuoyancyInfoOverride;
	}

	CBaseModelInfo* pModelInfo = pParent->GetBaseModelInfo();

	if(pModelInfo)
	{
		return pModelInfo->GetBuoyancyInfo();
	}

	return NULL;
}
CBuoyancyInfo::CBuoyancyInfo()
{
	m_WaterSamples = NULL;
	m_nNumWaterSamples = 0;
	m_fBuoyancyConstant = 1.1f;

	m_fLiftMult = 0.0f;
	m_fKeelMult = 0.0f;

	m_fDragMultXY = 0.0f;
	m_fDragMultZUp = 0.0f;
	m_fDragMultZDown = 0.0f;

	m_nNumBoatWaterSampleRows = 0;
	m_nBoatWaterSampleRowIndices = NULL;
}

void CBuoyancyInfo::CopyFrom(const CBuoyancyInfo& otherInfo)
{
	// If we already have water samples then clean them up
	if(m_WaterSamples != NULL)
	{
		delete[] m_WaterSamples;
		m_WaterSamples = NULL;
	}

	m_nNumWaterSamples = otherInfo.m_nNumWaterSamples;
	m_fBuoyancyConstant = otherInfo.m_fBuoyancyConstant;

	m_fLiftMult = otherInfo.m_fLiftMult;
	m_fKeelMult = otherInfo.m_fKeelMult;

	m_fDragMultXY = otherInfo.m_fDragMultXY;
	m_fDragMultZUp = otherInfo.m_fDragMultZUp;
	m_fDragMultZDown = otherInfo.m_fDragMultZDown;

	if(otherInfo.m_WaterSamples != NULL && m_nNumWaterSamples > 0)
	{
		m_WaterSamples = rage_new CWaterSample[m_nNumWaterSamples];
		for(int iSampleIndex = 0; iSampleIndex < m_nNumWaterSamples; iSampleIndex++)
		{
			m_WaterSamples[iSampleIndex].Set(otherInfo.m_WaterSamples[iSampleIndex]);
		}
	}
}

CBuoyancyInfo* CBuoyancyInfo::Clone()
{
	CBuoyancyInfo* pClone = rage_new CBuoyancyInfo;
	pClone->CopyFrom(*this);
	return pClone;
}

#if __DEV
static int s_sampleCounter = 0;
#endif

CBuoyancyInfo::~CBuoyancyInfo()
{
	if(m_WaterSamples)
	{
		Assert(m_nNumWaterSamples > 0);
		delete[] m_WaterSamples;
	}
}

void CWaterSample::Set(CWaterSample &sample)
{
	m_vSampleOffset = sample.m_vSampleOffset;
	m_fSize = sample.m_fSize;
	m_fBuoyancyMult = sample.m_fBuoyancyMult;
	m_nComponent = sample.m_nComponent;
    m_bKeel = sample.m_bKeel;
}

CWaterSample::CWaterSample()
{
	m_vSampleOffset = VEC3_ZERO;
	m_fSize = 0.0f;
	m_fBuoyancyMult = 1.0f;
	m_nComponent = 0;
    m_bKeel = FALSE;
	m_bPontoon = FALSE;

#if __DEV
	s_sampleCounter++;

	PF_SET(NumWaterSamples, s_sampleCounter);
#endif
}

CWaterSample::~CWaterSample()
{
#if __DEV
	s_sampleCounter--;

	PF_SET(NumWaterSamples, s_sampleCounter);
#endif
}

