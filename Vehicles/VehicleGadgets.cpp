#include "Vehicles/VehicleGadgets.h"

// Rage headers
#include "crskeleton/jointdata.h"
#include "fragment/typegroup.h" 
#include "pharticulated/articulatedcollider.h"
#include "phbound/BoundComposite.h"
#include "phbound/boundbox.h"
#include "physics/constraintspherical.h"
#include "physics/constraintdistance.h"
#include "physics/constraintfixed.h"
#include "physics/constraintRotation.h"

// Framework headers
#include "ai/aichannel.h"
#include "fwpheffects/ropedatamanager.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "grcore/debugdraw.h"

// Game headers
#include "Camera/Base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "Camera/Cinematic/CinematicDirector.h"
#include "Camera/Debug/DebugDirector.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Camera/viewports/ViewportManager.h"
#include "Camera/scripted/ScriptDirector.h"
#include "control/replay/Misc/RopePacket.h"
#include "control/replay/Vehicle/VehiclePacket.h"
#include "Debug/DebugScene.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Game/ModelIndices.h"
#include "FwPhEffects/ropemanager.h"
#include "modelinfo/VehicleModelInfoFlags.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/CollisionRecords.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "Physics/PhysicsHelpers.h"
#include "Physics/Rope.h"
#include "physics/Tunable.h"
#include "Network/Objects/Entities/NetObjObject.h"
#include "Renderer/Lights/AsyncLightOcclusionMgr.h"
#include "Renderer/Lights/lights.h"
#include "Scene/World/GameWorld.h"
#include "Scene/Entity.h"
#include "Shaders/CustomShaderEffectVehicle.h"
#include "streaming/streaming.h"
#include "System/Control.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Motion/Vehicle/TaskMotionInTurret.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "VehicleAi/Task/TaskVehiclePlayer.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/VehicleDefines.h"
#include "Vehicles/Vehicle_Channel.h"
#include "Vehicles/Trailer.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Misc/WaterCannon.h"
#include "Weapons/Weapon.h"
#include "Weapons/Info/VehicleWeaponInfo.h"

VEHICLE_OPTIMISATIONS()
PHYSICS_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

using namespace rage;

float CVehicleGadgetHandlerFrame::ms_fTransverseOffset = 0.75f;
float CVehicleGadgetHandlerFrame::ms_fLongitudinalOffset = 0.5f;
float CVehicleGadgetHandlerFrame::ms_fTestSphereRadius = 0.3f;
float CVehicleGadgetHandlerFrame::ms_fFrameUpwardOffsetAfterAttachment = 0.4f;

#if __BANK
bool CVehicleGadgetForks::ms_bShowAttachFSM = false;
bool CVehicleGadgetForks::ms_bDrawForkAttachPoints = false;
bool CVehicleGadgetForks::ms_bHighlightExtraPalletBoundsInUse = false;
bool CVehicleGadgetForks::ms_bDrawContactsInExtraPalletBounds = false;
bool CVehicleGadgetForks::ms_bRenderDebugInfoForSelectedPallet = false;
bool CVehicleGadgetForks::ms_bRenderTuningValuesForSelectedPallet = false;
float CVehicleGadgetForks::ms_fPalletAttachBoxWidth = 0.8f;
float CVehicleGadgetForks::ms_fPalletAttachBoxLength = 0.8f;
float CVehicleGadgetForks::ms_fPalletAttachBoxHeight = 0.2f;
float CVehicleGadgetForks::ms_vPalletAttachBoxOriginX = 0.0f;
float CVehicleGadgetForks::ms_vPalletAttachBoxOriginY = 0.0f;
float CVehicleGadgetForks::ms_vPalletAttachBoxOriginZ = 0.0f;

bool CVehicleGadgetHandlerFrame::ms_bShowAttachFSM = false;
bool CVehicleGadgetHandlerFrame::ms_bDebugFrameMotion = false;

void CVehicleGadget::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Vehicle gadgets");

		bank.PushGroup("Forklift");
			bank.AddToggle("Show attachment FSM state", &CVehicleGadgetForks::ms_bShowAttachFSM);
			bank.AddToggle("Draw attach points on forks", &CVehicleGadgetForks::ms_bDrawForkAttachPoints);
			bank.AddToggle("Highlight extra pallet bounds in use", &CVehicleGadgetForks::ms_bHighlightExtraPalletBoundsInUse);
			bank.AddToggle("Draw contacts between forks and extra pallet bounds", &CVehicleGadgetForks::ms_bDrawContactsInExtraPalletBounds);
			bank.AddToggle("Render debug info for selected pallet object", &CVehicleGadgetForks::ms_bRenderDebugInfoForSelectedPallet);
			bank.AddToggle("Render tuning values for pallet attach box", &CVehicleGadgetForks::ms_bRenderTuningValuesForSelectedPallet);
			bank.AddSlider("Width of angled area for attach", &CVehicleGadgetForks::ms_fPalletAttachBoxWidth, 0.01f, 10.0f, 0.01f);
			bank.AddSlider("Length of angled area for attach", &CVehicleGadgetForks::ms_fPalletAttachBoxLength, 0.01f, 10.0f, 0.01f);
			bank.AddSlider("Height of angled area for attach", &CVehicleGadgetForks::ms_fPalletAttachBoxHeight, 0.01f, 10.0f, 0.01f);
			bank.AddSlider("Angled area origin X", &CVehicleGadgetForks::ms_vPalletAttachBoxOriginX, -10.0f, 10.0f, 0.01f);
			bank.AddSlider("Angled area origin Y", &CVehicleGadgetForks::ms_vPalletAttachBoxOriginY, -10.0f, 10.0f, 0.01f);
			bank.AddSlider("Angled area origin Z", &CVehicleGadgetForks::ms_vPalletAttachBoxOriginZ, -10.0f, 10.0f, 0.01f);
		bank.PopGroup(); // "Forklift".

		bank.PushGroup("Handler");
			bank.AddToggle("Show attachment FSM state", &CVehicleGadgetHandlerFrame::ms_bShowAttachFSM);
			bank.AddToggle("Debug frame motion", &CVehicleGadgetHandlerFrame::ms_bDebugFrameMotion);
			bank.AddSlider("Attach test sphere radius", &CVehicleGadgetHandlerFrame::ms_fTestSphereRadius, 0.0f, 5.0f, 0.01f);
			bank.AddSlider("Test spheres transverse offset", &CVehicleGadgetHandlerFrame::ms_fTransverseOffset, -2.0f, 2.0f, 0.01f);
			bank.AddSlider("Test spheres longitudinal offset", &CVehicleGadgetHandlerFrame::ms_fLongitudinalOffset, -2.0f, 2.0f, 0.01f);
			bank.AddSlider("Frame height after attach", &CVehicleGadgetHandlerFrame::ms_fFrameUpwardOffsetAfterAttachment, 0.0f, 5.9f, 0.1f);
		bank.PopGroup(); // "Handler".

		CVehicleRadar::InitWidgets(bank);

		bank.PushGroup("Magnet");
		
			bank.AddToggle("Show Debug", &CVehicleGadgetPickUpRope::ms_bShowDebug);

			bank.AddToggle("Override magnet parameters", &CVehicleGadgetPickUpRopeWithMagnet::ms_bOverrideParameters);
			bank.AddSlider("Magnet Strength", &CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetStrength, 0.0f, 1000.0f, 0.01f);
			bank.AddSlider("Magnet Falloff", &CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetFalloff, -10.0f, 10.0f, 0.01f);
			bank.AddSlider("Reduced Magnet Strength", &CVehicleGadgetPickUpRopeWithMagnet::ms_fReducedMagnetStrength, 0.0f, 1000.0f, 0.01f);
			bank.AddSlider("Reduced Magnet Falloff", &CVehicleGadgetPickUpRopeWithMagnet::ms_fReducedMagnetFalloff, -10.0f, 10.0f, 0.01f);
			
			bank.AddSlider("Magnet Pull Strength", &CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetPullStrength, 0.0f, 1000.0f, 0.01f);
			bank.AddSlider("Magnet Pull Rope Length", &CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetPullRopeLength, -10.0f, 10.0f, 0.01f);

			bank.AddToggle("Enable damping", &CVehicleGadgetPickUpRope::ms_bEnableDamping);
			bank.AddSlider("Spring Constant", &CVehicleGadgetPickUpRope::ms_fSpringConstant, -100.0f, 100.0f, 0.01f);
			bank.AddSlider("Damping Constant", &CVehicleGadgetPickUpRope::ms_fDampingConstant, -100.0f, 100.0f, 0.01f);
			bank.AddSlider("Damping factor", &CVehicleGadgetPickUpRope::ms_fDampingFactor, 0.0f, 1000.0f, 0.01f);
			bank.AddSlider("Max. damp force", &CVehicleGadgetPickUpRope::ms_fMaxDampAccel, -10000.0f, 10000.0f, 0.01f);
		bank.PopGroup();

		bank.PushGroup("Pickup rope");
			bank.AddToggle("Log callstack on detach", &CVehicleGadgetPickUpRope::ms_logCallStack);
		bank.PopGroup();

	bank.PopGroup(); // "Vehicle gadgets".
}
#endif // __BANK


//////////////////////////////////////////////////////////////////////////
// CVehicleTracks
CVehicleTracks::CVehicleTracks(u32 iWheelMask)
{
	m_fWheelAngleLeft = 0.0f;
	m_fWheelAngleRight = 0.0f;
	m_iWheelMask = iWheelMask;
}

void CVehicleTracks::ProcessPhysics(CVehicle* UNUSED_PARAM(pParent), const float UNUSED_PARAM(fTimeStep), const int UNUSED_PARAM(nTimeSlice))
{	
}

void CVehicleTracks::ProcessPreRender(CVehicle* pParent)
{	
	CCustomShaderEffectVehicle* pShaderFx =
		static_cast<CCustomShaderEffectVehicle*>(pParent->GetDrawHandler().GetShaderEffect());

	if(pShaderFx && !fwTimer::IsGamePaused())
	{
		// Figure out how much to displace each track by
		// use average speed on each wheel
		Vector2 vAngSpeed(0.0f,0.0f);
		s32 iNumRightWheels = 0;
		s32 iNumLeftWheels = 0;

		for(s32 i = 0; i < pParent->GetNumWheels(); i++)
		{
			if(BIT(i) & m_iWheelMask)
			{
				CWheel* pWheel = pParent->GetWheel(i);
				Assertf(pWheel,"Unexpected null wheel");

				if( pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) &&
					pWheel->GetConfigFlags().IsFlagSet( WCF_TRACKED_WHEEL ) )
				{
					vAngSpeed.y += pWheel->GetRotSpeed();
					iNumLeftWheels ++;
				}
				else if( pWheel->GetConfigFlags().IsFlagSet(WCF_TRACKED_WHEEL) )
				{
					vAngSpeed.x += pWheel->GetRotSpeed();
					iNumRightWheels++;
				}			
			}
		}

		// Scale ang speed to get average
		if(iNumLeftWheels > 0)
			vAngSpeed.y = vAngSpeed.y / (float) iNumLeftWheels;

		if(iNumRightWheels > 0)
			vAngSpeed.x = vAngSpeed.x / (float) iNumRightWheels;
		else
		{
			// If no right wheels set right displacement to same as left
			// so it looks like just one track
			vAngSpeed.x = vAngSpeed.y;
		}

		// Going to override the track wheel's rotation angle so progress a local copy
		m_fWheelAngleLeft += vAngSpeed.y * fwTimer::GetTimeStep();
		m_fWheelAngleRight += vAngSpeed.x * fwTimer::GetTimeStep();

		m_fWheelAngleLeft = fwAngle::LimitRadianAngleFast(m_fWheelAngleLeft);
		m_fWheelAngleRight = fwAngle::LimitRadianAngleFast(m_fWheelAngleRight);

		// Need to go from metres to UVs
		// Use multiplier in handling
		CVehicleWeaponHandlingData* pWeaponHandling = pParent->pHandling->GetWeaponHandlingData();
		if(Verifyf(pWeaponHandling,"Vehicle %s missing handling data. Need weapon handling to animate UV tracks.",pParent->GetVehicleModelInfo()->GetGameName()))
		{
			const float fScaleFactor = fwTimer::GetTimeStep() * pWeaponHandling->GetUvAnimationMult();
			vAngSpeed.Scale(fScaleFactor);
			pShaderFx->TrackUVAnimAdd(vAngSpeed);

			audVehicleAudioEntity* vehicleAudioEntity = pParent->GetVehicleAudioEntity();

			if(vehicleAudioEntity)
			{
				vehicleAudioEntity->SetVehicleCaterpillarTrackSpeed(vAngSpeed.Mag());
			}
		}
	}

	for(s32 i = 0; i < pParent->GetNumWheels(); i++)
	{
		if(BIT(i) & m_iWheelMask)
		{
			CWheel* pWheel = pParent->GetWheel(i);
			
			// Need to ensure wheels aren't exactly in sync or it will look really fake
			mthRandom rand;
			rand.Reset(pParent->GetRandomSeed() + i);
			float fOffset = (rand.GetFloat() * TWO_PI) - PI;
			
			if (Verifyf(pWheel,"Unexpected null wheel"))
			{
				if(pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) &&
					!pWheel->GetConfigFlags().IsFlagSet(WCF_STEER) )
					pWheel->SetRotAngle(m_fWheelAngleLeft + fOffset);
				else if( !pWheel->GetConfigFlags().IsFlagSet(WCF_STEER) )
					pWheel->SetRotAngle(m_fWheelAngleRight + fOffset);
			}
		}
	}

	// Also want to spin the misc components round
	// These are cogs attached to the track
	CVehicleWeaponHandlingData* pWeaponHandling = pParent->pHandling->GetWeaponHandlingData();
	if(Verifyf(pWeaponHandling,"Vehicle %s missing handling data. Need weapon handling to spin extra wheels.",pParent->GetVehicleModelInfo()->GetGameName()))
	{
		const float fExtraWheelMult = pWeaponHandling->GetMiscGadgetVar();
		if(fExtraWheelMult*fExtraWheelMult > 0.0f)
		{
			if( pParent->GetModelIndex() != MI_CAR_SCARAB && 
				pParent->GetModelIndex() != MI_CAR_SCARAB2 &&
				pParent->GetModelIndex() != MI_CAR_SCARAB3 )
			{
				const float fLeftSpinnerAngle = m_fWheelAngleLeft*fExtraWheelMult;
				const float fRightSpinnerAngle = m_fWheelAngleRight*fExtraWheelMult;
				pParent->SetComponentRotation( VEH_MISC_A, ROT_AXIS_LOCAL_X, fLeftSpinnerAngle, true );
				pParent->SetComponentRotation( VEH_MISC_B, ROT_AXIS_LOCAL_X, fLeftSpinnerAngle, true );
				pParent->SetComponentRotation( VEH_MISC_C, ROT_AXIS_LOCAL_X, fRightSpinnerAngle, true );
				pParent->SetComponentRotation( VEH_MISC_D, ROT_AXIS_LOCAL_X, fRightSpinnerAngle, true );
			}
			else
			{
				float fLeftSpinnerAngle = m_fWheelAngleLeft*fExtraWheelMult;
				float fRightSpinnerAngle = m_fWheelAngleRight*fExtraWheelMult;
				pParent->SetComponentRotation( VEH_MISC_A, ROT_AXIS_LOCAL_X, fLeftSpinnerAngle, true );
				pParent->SetComponentRotation( VEH_MISC_B, ROT_AXIS_LOCAL_X, fLeftSpinnerAngle, true );
				fLeftSpinnerAngle *= 3.0f;
				pParent->SetComponentRotation( VEH_MISC_C, ROT_AXIS_LOCAL_X, fLeftSpinnerAngle, true );
				pParent->SetComponentRotation( VEH_MISC_D, ROT_AXIS_LOCAL_X, fLeftSpinnerAngle, true );

				pParent->SetComponentRotation( VEH_MISC_E, ROT_AXIS_LOCAL_X, fRightSpinnerAngle, true );
				pParent->SetComponentRotation( VEH_MISC_F, ROT_AXIS_LOCAL_X, fRightSpinnerAngle, true );
				fRightSpinnerAngle *= 3.0f;
				pParent->SetComponentRotation( VEH_MISC_G, ROT_AXIS_LOCAL_X, fRightSpinnerAngle, true );
				pParent->SetComponentRotation( VEH_MISC_H, ROT_AXIS_LOCAL_X, fRightSpinnerAngle, true );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// Class CVehicleLeanHelper
/////////////////////////////////////////////////////////////////////////////////////
CVehicleLeanHelper::CVehicleLeanHelper()
{

}

static dev_float sfVehicleLeanTorque = 10.0f;	// Needs to be in handling
static dev_float sfVehiclePlayerLeanTorqueBoat = 6.0f;	// Needs to be in handling
static dev_float sfVehiclePlayerLeanTorque = 2.0f;	// Needs to be in handling

dev_float CVehicleLeanHelper::ms_fAngInertiaRaiseFactor = 0.f;
dev_float CVehicleLeanHelper::ms_fLeanMinSpeed = 1.0f;
dev_float CVehicleLeanHelper::ms_fLeanBlendSpeed = 10.0f;
dev_float CVehicleLeanHelper::ms_fLeanForceSpeedMult = 0.2f;
dev_float CVehicleLeanHelper::ms_fWheelieForceCgMult = -0.5f;
dev_float CVehicleLeanHelper::ms_fWheelieOvershootAngle = 0.05f;
dev_float CVehicleLeanHelper::ms_fWheelieOvershootCap = 0.5f;
dev_float CVehicleLeanHelper::ms_fStoppieStabiliserMult = -5.0f;


void CVehicleLeanHelper::ProcessPhysics2(CVehicle* pParent, const float UNUSED_PARAM(fTimestep))
{
	const float kfSpeedThresholdForLean = 5.0f;
	if(!pParent->GetIsJetSki() || pParent->GetVelocity().Mag2()>rage::square(kfSpeedThresholdForLean))
	{
		ProcessLeanSideways(pParent);
	}
	float fFwdInput = 0.0f;

	// This input isn't stored on the vehicle so need to do the control processing ourselves
	if(pParent->GetDriver() && pParent->GetDriver()->IsLocalPlayer())
	{
		CPed* pPlayerPed = pParent->GetDriver();
		CControl* pControl = pPlayerPed->GetControlFromPlayer();
		fFwdInput = pControl->GetVehicleSteeringUpDown().GetNorm();
	}

	ProcessLeanFwd(pParent,fFwdInput);
}

void CVehicleLeanHelper::ProcessLeanSideways(CVehicle* pParent)
{
	// Look up the vehilce's bike handling
	// This is used for the lean variables
	// **The vehicle does not need to be a bike**
	CHandlingData* pHandling = pParent->pHandling;
	CBikeHandlingData* pBikeHandling = pHandling ? pParent->pHandling->GetBikeHandlingData() : NULL;

	if(!vehicleVerifyf(pBikeHandling,"%s Couldn't find bike handling for CVehicleLeanHelper",pParent->GetVehicleModelInfo()->GetModelName()))
	{
		return;
	}

    Assert(pParent->GetTransform().IsOrthonormal());

	float fLRInput = pParent->GetSteerAngle() / pHandling->m_fSteeringLock;

    // LEAN
    if(pParent->GetDriver() && pBikeHandling->m_fMaxBankAngle > 0.0f)
    {
	    // Determine a desired lean angle from steering inputs
	    float fDesiredLeanAngle = 0.0f;

	    // Apply torque to get to desired lean angle
	    const float fCurrentLeanAngle = -rage::AsinfSafe(pParent->GetTransform().GetA().GetZf());
	    float fLeanTorque = fDesiredLeanAngle - fCurrentLeanAngle;
	    fLeanTorque = fwAngle::LimitRadianAngle(fLeanTorque);

	    // Reduce this as we go vertical
	    fLeanTorque *= 1.0f - rage::Abs(pParent->GetTransform().GetB().GetZf());

        static dev_bool enableRotSpeed = true;
        if(enableRotSpeed)
        {
            float fRotSpeed = DotProduct(pParent->GetAngVelocity(), VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()));

            static dev_float sfRotSpeedMult = -1.0f;
            fLeanTorque += fRotSpeed * sfRotSpeedMult;
        }

	    fLeanTorque = rage::Clamp(fLeanTorque*sfVehicleLeanTorque,-9.9f,9.9f);

	    fLeanTorque *= pParent->GetAngInertia().y;

		float fPlayerLeanTorque = sfVehiclePlayerLeanTorque;
		if(pParent->InheritsFromBoat())
		{
			 fPlayerLeanTorque = sfVehiclePlayerLeanTorqueBoat;
		}

        //add some player input
        fLeanTorque += (-fLRInput * pParent->GetAngInertia().y*fPlayerLeanTorque);

	    static dev_bool bApplyTorqueInternal = false;
	    if(bApplyTorqueInternal)
	    {
		    pParent->ApplyInternalTorque(VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()) * fLeanTorque);
	    }
	    else
	    {
		    pParent->ApplyTorque(VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()) * fLeanTorque);
	    }
    }
}

void CVehicleLeanHelper::ProcessLeanFwd(CVehicle* pParent, float fLeanInput)
{
	CHandlingData* pHandling = pParent->pHandling;
	CBikeHandlingData* pBikeHandlingData = pHandling ? pHandling->GetBikeHandlingData() : NULL;

	// SHIFT CG fwd / backward
	if(vehicleVerifyf(pBikeHandlingData,"CVehicleLeanHelper requires bike handling data"))
	{
		bool bMoving = false;
		float fSpeedBlend = 0.0f;
		const Vector3& vMoveSpeed = pParent->GetVelocity();

		if(!pParent->InheritsFromBoat()) //boats can lean at 0 speed
		{
			if(vMoveSpeed.Mag2() > square(ms_fLeanMinSpeed))
			{
				bMoving = true;
				if(vMoveSpeed.Mag2() > square(ms_fLeanBlendSpeed))
					fSpeedBlend = 1.0f;
				else
					fSpeedBlend = vMoveSpeed.Mag() / ms_fLeanBlendSpeed;
			}

			fLeanInput *= fSpeedBlend;
		}

		bool bTrike	= pParent->InheritsFromQuadBike() && pParent->GetNumWheels() == 3;
		// Figure out whether front and/or back wheels are touching
		bool bFrontTouching = false;
		bool bBackTouching	= false;
		bool bAllBackTouching = true;

		for(s32 iWheelIndex = 0; iWheelIndex < pParent->GetNumWheels(); iWheelIndex++)
		{
			Assert(pParent->GetWheel(iWheelIndex));

			if(pParent->GetWheel(iWheelIndex)->GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				bool bWheelTouching = pParent->GetWheel(iWheelIndex)->GetDynamicFlags().IsFlagSet(WF_HIT_PREV);
				bBackTouching |= bWheelTouching;

				if( !bWheelTouching )
				{
					bAllBackTouching = false;
				}
			}
			else
			{
				bFrontTouching |= pParent->GetWheel(iWheelIndex)->GetDynamicFlags().IsFlagSet(WF_HIT_PREV);
			}
		}

		// apply torque from rider leaning fwd/back
		if( ( !bTrike || ( bBackTouching && bAllBackTouching ) ) && pParent->GetWheeliesEnabled() ) 
		{
			if( bMoving && fLeanInput != 0.0f )
			{
				float fLeanForce = fLeanInput;
				float fRotSpeed = DotProduct(pParent->GetAngVelocity(), VEC3V_TO_VECTOR3(pParent->GetTransform().GetA())) * ms_fLeanForceSpeedMult;

				if(fLeanInput < 0.0f)
				{	
					if(fRotSpeed < 0.0f)
						fLeanForce = rage::Min(0.0f, fLeanForce - fRotSpeed);
				
					fLeanForce *= pParent->GetAngInertia().x*pHandling->GetBikeHandlingData()->m_fLeanFwdForceMult;

					if(pParent->InheritsFromBike() && pParent->GetWheel(1) && pParent->GetWheel(1)->GetIsTouching())
						pParent->ApplyInternalForceCg(fLeanForce*ms_fWheelieForceCgMult*ZAXIS);
				}
				else
				{
					if(fRotSpeed > 0.0f)
						fLeanForce = rage::Max(0.0f, fLeanForce - fRotSpeed);
				
					fLeanForce *= pParent->GetAngInertia().x*pHandling->GetBikeHandlingData()->m_fLeanBakForceMult;

					if( ( pParent->InheritsFromBike() && pParent->GetWheel(0)->GetIsTouching() )
						|| ( pParent->InheritsFromAutomobile() && pParent->GetWheelFromId(VEH_WHEEL_LR) && pParent->GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() && pParent->GetWheelFromId(VEH_WHEEL_RR) && pParent->GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() ) )
						pParent->ApplyInternalForceCg(-fLeanForce*ms_fWheelieForceCgMult*ZAXIS);
				}


				if(MI_QUADBIKE_BLAZER4.IsValid() && pParent->GetModelIndex() == MI_QUADBIKE_BLAZER4)
				{
					static dev_float sfMaxLeanSpeedSqr = 625.0f;
					static dev_float sfSpeedBlendOutMult = 0.005f;

					float fLateralSpeed = rage::Abs(Dot(vMoveSpeed, VEC3V_TO_VECTOR3(pParent->GetTransform().GetA())));
					float fYawSpeed = rage::Abs(Dot(pParent->GetAngVelocity(), VEC3V_TO_VECTOR3(pParent->GetTransform().GetC())));

					static dev_float sfLateralSpeedLimitForWheelie = 0.4f;
					static dev_float sfYawSpeedLimitForWheelie = 0.4f;

					if( bAllBackTouching &&
						( fLateralSpeed > sfLateralSpeedLimitForWheelie || fYawSpeed > sfYawSpeedLimitForWheelie ) )
					{
						fLeanForce = 0.0f;
					}

					if(vMoveSpeed.Mag2() > sfMaxLeanSpeedSqr)
					{
						fLeanForce /= vMoveSpeed.Mag2() * sfSpeedBlendOutMult;
					}
				}
			
				pParent->ApplyInternalTorque(fLeanForce* VEC3V_TO_VECTOR3(pParent->GetTransform().GetA()));
			}
		}
		
		float fInputX = -pParent->m_vehControls.m_steerAngle * pParent->pHandling->m_fSteeringLockInv;
		float fInputXClamped = rage::Clamp(fInputX, -0.4f, 0.4f);		

		CBikeHandlingData* pBikeHandling = pHandling->GetBikeHandlingData();

		// check we've got the wheels the right way round
		// back wheel on ground, front wheel not = Wheelie
		if(bBackTouching && !bFrontTouching && bMoving && ( !bTrike || bAllBackTouching ) )
		{
			pParent->ApplyInternalTorque(fInputXClamped*pBikeHandling->m_fWheelieSteerMult*pParent->GetAngInertia().z*ZAXIS);

			float fWheelieAngle = rage::Asinf(pParent->GetTransform().GetB().GetZf()) - pBikeHandling->m_fWheelieBalancePoint;
			float fWheelieStabiliseForce = 0.0f;
			if(fWheelieAngle > ms_fWheelieOvershootAngle)
			{
				fWheelieStabiliseForce = ((fWheelieAngle - ms_fWheelieOvershootAngle) / (ms_fWheelieOvershootCap - ms_fWheelieOvershootAngle) );
				fWheelieStabiliseForce = Clamp(fWheelieStabiliseForce, 0.0f, 1.0f);
				fWheelieStabiliseForce *= -1.0f;
			}

			// blend stabilization force out smoothly at low speeds
			fWheelieStabiliseForce *= pBikeHandling->m_fRearBalanceMult * pParent->GetAngInertia().x * fSpeedBlend;
			pParent->ApplyInternalTorque(fWheelieStabiliseForce* VEC3V_TO_VECTOR3(pParent->GetTransform().GetA()));
		}
		// front wheel on ground, back wheel not = Stoppie
		else if(bFrontTouching && !bBackTouching && bMoving)
		{
			pParent->ApplyInternalTorque(0.5f*fInputXClamped*pBikeHandling->m_fWheelieSteerMult*pParent->GetAngInertia().z*ZAXIS);

			float fSideSpeed = DotProduct(VEC3V_TO_VECTOR3(pParent->GetTransform().GetA()), vMoveSpeed);
			if(rage::Abs(fSideSpeed) > 0.001f && pParent->GetBrake() > 0.0f)
			{
				if(fSideSpeed > 0.0f)
					fSideSpeed = rage::Sqrtf(fSideSpeed);
				else
					fSideSpeed = -1.0f*rage::Sqrtf(rage::Abs(fSideSpeed));

				pParent->ApplyInternalTorque(fSideSpeed*ms_fStoppieStabiliserMult*pParent->GetAngInertia().z*ZAXIS);
			}

			float fWheelieAngle = rage::Asinf(pParent->GetTransform().GetB().GetZf()) - pBikeHandling->m_fStoppieBalancePoint;
			float fWheelieStabiliseForce = 0.0f;
			if(fWheelieAngle < -ms_fWheelieOvershootAngle)
			{
				fWheelieStabiliseForce = ((-fWheelieAngle - ms_fWheelieOvershootAngle) / (ms_fWheelieOvershootCap - ms_fWheelieOvershootAngle) );
				fWheelieStabiliseForce = Clamp(fWheelieStabiliseForce, 0.0f, 1.0f);
			}

			// blend stabilization force out smoothly at low speeds
			fWheelieStabiliseForce *= pBikeHandling->m_fFrontBalanceMult * pParent->GetAngInertia().x * fSpeedBlend;
			pParent->ApplyInternalTorque(fWheelieStabiliseForce* VEC3V_TO_VECTOR3(pParent->GetTransform().GetA()));
		}
		// both wheels off the ground
		else if(!bFrontTouching && !bBackTouching && !bTrike)
		{
			pParent->ApplyInternalTorque(fInputX*pBikeHandling->m_fInAirSteerMult*pParent->GetAngInertia().z* VEC3V_TO_VECTOR3(pParent->GetTransform().GetC()));
		}

		// process cg position
		if(pParent->GetVehicleFragInst()->GetCached() && pParent->GetCollider() && ( !bTrike || bBackTouching ) )
		{
			float fCgOffset = -fLeanInput;
			fCgOffset *= Selectf(fCgOffset, pHandling->GetBikeHandlingData()->m_fLeanFwdCOMMult, pHandling->GetBikeHandlingData()->m_fLeanBakCOMMult);
			Vector3 vecCgOffset(RCC_VECTOR3(pHandling->m_vecCentreOfMassOffset));
			vecCgOffset.y += fCgOffset;
			pParent->GetVehicleFragInst()->GetArchetype()->GetBound()->SetCGOffset(RCC_VEC3V(vecCgOffset));
			pParent->GetCollider()->SetColliderMatrixFromInstance();
		}
		// do extra stability while braking (like a rudder effect)
		if(pParent->GetBrake() > 0.0f && pParent->GetVelocity().Mag2() > 1.0f)
		{
			float fSideSpeed = DotProduct(VEC3V_TO_VECTOR3(pParent->GetTransform().GetA()), pParent->GetVelocity());
			Vector3 vecBrakeStabiliseTorque(VEC3V_TO_VECTOR3(pParent->GetTransform().GetC()));
			vecBrakeStabiliseTorque.Scale(fSideSpeed * pParent->GetBrake() * pBikeHandlingData->m_fBrakingStabilityMult * pParent->GetAngInertia().z);

			pParent->ApplyInternalTorque(vecBrakeStabiliseTorque);
		}
	} // if bike handling data	

}

CFixedVehicleWeapon::CFixedVehicleWeapon()
{
	m_iWeaponBone = VEH_INVALID_ID;
	m_iTimeLastFired = 0;
	m_bFireIsQueued = false;
	m_bForceShotInWeaponDirection = false;
	m_pVehicleWeapon = NULL;
	m_pFiringPed = NULL;
	m_pQueuedTargetEnt = NULL;
	m_vQueuedTargetPos.Zero();
	m_fProbeDist = 0.0f;
	m_v3ReticlePosition.Zero();
	m_vOverrideWeaponDirection.Zero();
	m_vTurretForwardLastUpdate.Zero();
	m_bUseOverrideWeaponDirection = false;
	m_fAmmoBeltAngToTurn = 0.0f;
	m_fOverrideLaunchSpeed = -1.0f;
}

CFixedVehicleWeapon::CFixedVehicleWeapon(u32 nWeaponHash, eHierarchyId iWeaponBone)
: m_iWeaponBone(iWeaponBone), m_iTimeLastFired(0), m_bFireIsQueued(false),m_bForceShotInWeaponDirection(false)
{
	m_pVehicleWeapon = NULL;
	m_fProbeDist = 0.0f;
	// the weapon can be constructed with null parameters when only used for serialising in MP 
	if(iWeaponBone != VEH_INVALID_ID && Verifyf(nWeaponHash > 0,"Vehicle has weapon bone but weapon hash is invalid"))
	{
		if((Verifyf(CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash) != NULL,"Weapon doesn't exist in rave data")))
		{
			m_pVehicleWeapon = rage_new CWeapon(nWeaponHash);

//#if !__FINAL
//			if (NetworkInterface::IsGameInProgress())
//				weaponDisplayf("CFixedVehicleWeapon::CFixedVehicleWeapon rage_new CWeapon GetNoOfUsedSpaces[%d]/GetSize[%d]",CWeapon::GetPool()->GetNoOfUsedSpaces(),CWeapon::GetPool()->GetSize());
//#endif

			const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
			Assert( pWeaponInfo );

			m_fProbeDist = pWeaponInfo->GetRange();
			if(pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE )
			{
				const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
				Assert(pAmmoInfo);
				m_fProbeDist = ((CAmmoProjectileInfo*)pAmmoInfo)->GetLifeTime() * ((CAmmoProjectileInfo*)pAmmoInfo)->GetLaunchSpeed();
			}

			// Might need to explicitly stream in projectile model, CWeapon doesn't do this for us
			strLocalIndex iRequiredModelLocalIndex = GetModelLocalIndexForStreaming();
			if(iRequiredModelLocalIndex.IsValid())
			{
				m_projectileRequester.Request(iRequiredModelLocalIndex,CModelInfo::GetStreamingModuleId(),STRFLAG_FORCE_LOAD);
			}
		}
	}
	m_pQueuedTargetEnt = NULL;
	m_vQueuedTargetPos.Zero();
	m_v3ReticlePosition.Zero();
	m_vOverrideWeaponDirection.Zero();
	m_vTurretForwardLastUpdate.Zero();
	m_bUseOverrideWeaponDirection = false;
	m_fOverrideLaunchSpeed = -1.0f;
}

CFixedVehicleWeapon::~CFixedVehicleWeapon()
{
	if(m_pVehicleWeapon)
	{
		delete m_pVehicleWeapon;
	}
}

void CFixedVehicleWeapon::ProcessControl(CVehicle* pParent)
{
	// Special case where plane can break off wing
	if( pParent->GetVehicleType() == VEHICLE_TYPE_PLANE )
	{
		// Hard-coded mappings between bone ids and wing sides. Sorry.
		static CAircraftDamage::eAircraftSection eSection = CAircraftDamage::INVALID_SECTION;

		const bool bIsBombushka = MI_PLANE_BOMBUSHKA.IsValid() && pParent->GetModelIndex() == MI_PLANE_BOMBUSHKA;
		const bool bIsMicrolight = MI_PLANE_MICROLIGHT.IsValid() && pParent->GetModelIndex() == MI_PLANE_MICROLIGHT;
		const bool bIsMogul = MI_PLANE_MOGUL.IsValid() && pParent->GetModelIndex() == MI_PLANE_MOGUL;
		const bool bIsMolotok = MI_PLANE_MOLOTOK.IsValid() && pParent->GetModelIndex() == MI_PLANE_MOLOTOK;
		const bool bIsTula = MI_PLANE_TULA.IsValid() && pParent->GetModelIndex() == MI_PLANE_TULA;
		const bool bIsVolatol = MI_PLANE_VOLATOL.IsValid() && pParent->GetModelIndex() == MI_PLANE_VOLATOL;

		const atHashWithStringNotFinal uLazerWeaponHash = ATSTRINGHASH("VEHICLE_WEAPON_PLAYER_LAZER", 0xE2822A29);
		const bool bIsLazerWeapon = m_pVehicleWeapon && m_pVehicleWeapon->GetWeaponHash() == uLazerWeaponHash;

		const atHashWithStringNotFinal uDogfighterWeaponHash = ATSTRINGHASH("VEHICLE_WEAPON_DOGFIGHTER_MG", 0x5F1834E2);
		const bool bIsDogfighterWeapon = m_pVehicleWeapon && m_pVehicleWeapon->GetWeaponHash() == uDogfighterWeaponHash;
		const bool bIsDogfighterChassisWeapon = bIsDogfighterWeapon && bIsMolotok;

		const atHashWithStringNotFinal uStrikeforceWeaponHash = ATSTRINGHASH("VEHICLE_WEAPON_STRIKEFORCE_CANNON", 0x38F41EAB);
		const bool bIsStrikeforceChassisWeapon = m_pVehicleWeapon && m_pVehicleWeapon->GetWeaponHash() == uStrikeforceWeaponHash;

		// Weapons attached to chassis
		if (bIsBombushka || bIsMicrolight || bIsMogul || bIsTula || bIsLazerWeapon || bIsDogfighterChassisWeapon || bIsVolatol || bIsStrikeforceChassisWeapon)
		{
			eSection = CAircraftDamage::INVALID_SECTION;
		}
		// Weapons attached to wings (alternating sides)
		else
		{
			if(	m_iWeaponBone == VEH_WEAPON_1A || m_iWeaponBone == VEH_WEAPON_1C || 
				m_iWeaponBone == VEH_WEAPON_2A || m_iWeaponBone == VEH_WEAPON_2C || m_iWeaponBone == VEH_WEAPON_2E || m_iWeaponBone == VEH_WEAPON_2G ||
				m_iWeaponBone == VEH_WEAPON_3A || m_iWeaponBone == VEH_WEAPON_3C || 
				m_iWeaponBone == VEH_WEAPON_4A || m_iWeaponBone == VEH_WEAPON_4C || 
				m_iWeaponBone == VEH_WEAPON_5A || m_iWeaponBone == VEH_WEAPON_5C || m_iWeaponBone == VEH_WEAPON_5E || m_iWeaponBone == VEH_WEAPON_5G ||
				m_iWeaponBone == VEH_WEAPON_5I || m_iWeaponBone == VEH_WEAPON_5K || m_iWeaponBone == VEH_WEAPON_5M ||
				m_iWeaponBone == VEH_WEAPON_6A || m_iWeaponBone == VEH_WEAPON_6C ) 
			{
				eSection = CAircraftDamage::WING_L;
			}
			else
			{
				eSection = CAircraftDamage::WING_R;
			}
		}

		if( eSection != CAircraftDamage::INVALID_SECTION )
		{
			CPlane* pPlane = static_cast<CPlane*>(pParent);
			if( pPlane->GetAircraftDamage().HasSectionBrokenOff( pPlane, eSection ) )
			{
				SetEnabled(pPlane, false );
			}
		}
	}

	// Pump the weapon
	if(m_pVehicleWeapon)
	{
		m_pVehicleWeapon->Process(pParent, fwTimer::GetTimeInMilliseconds());
	}

	// Make sure the streaming requester is being processed
	m_projectileRequester.HasLoaded();
}

void CFixedVehicleWeapon::TriggerEmptyShot(CVehicle* pParent) const
{	
	m_pVehicleWeapon->GetAudioComponent().TriggerEmptyShot(pParent, this);
}

void CFixedVehicleWeapon::Fire(const CPed* pFiringPed, CVehicle* pParent, const Vector3& vTargetPosition, const CEntity* pTarget, bool bForceShotInWeaponDirection)
{
	weaponDebugf3("CFixedVehicleWeapon::Fire pFiringPed[%p] pParent[%p] vTargetPosition[%f %f %f] pTarget[%p] bForceShotInWeaponDirection[%d]",pFiringPed,pParent,vTargetPosition.x,vTargetPosition.y,vTargetPosition.z,pTarget,bForceShotInWeaponDirection);

	m_pFiringPed = pFiringPed;
	m_pQueuedTargetEnt = pTarget;
	m_vQueuedTargetPos = vTargetPosition;
	m_bForceShotInWeaponDirection = bForceShotInWeaponDirection;

	if ( pParent->GetIsVisibleInSomeViewportThisFrame() )
	{
		m_bFireIsQueued = true;
	}
	else
	{
		FireInternal(pParent);
		m_bFireIsQueued = false;
		m_bForceShotInWeaponDirection = false;
		m_pQueuedTargetEnt = NULL;
		m_vQueuedTargetPos.Zero();
	}
}

void CFixedVehicleWeapon::SpinUp(CPed* pFiringPed)
{
	// Need to set firing ped in order to load spin up audio properly
	m_pFiringPed = pFiringPed;
	
	aiFatalAssertf(m_pVehicleWeapon,"NULL m_pVehicleWeapon");
	m_pVehicleWeapon->SpinUp(pFiringPed);
}

const CWeaponInfo* CFixedVehicleWeapon::GetWeaponInfo() const
{
	aiFatalAssertf(m_pVehicleWeapon,"NULL m_pVehicleWeapon");
	return m_pVehicleWeapon->GetWeaponInfo();
}

u32 CFixedVehicleWeapon::GetHash() const
{
	aiFatalAssertf(m_pVehicleWeapon,"NULL m_pVehicleWeapon");
	return m_pVehicleWeapon->GetWeaponHash();
}

bool CFixedVehicleWeapon::GetReticlePosition(Vector3& vReticlePositionWorld) const
{
	if( m_v3ReticlePosition.IsZero() )
		return false;

	vReticlePositionWorld = m_v3ReticlePosition;
	return true;
}

void CFixedVehicleWeapon::GetForceShotInWeaponDirectionEnd(CVehicle* pParent, Vector3& vForceShotInWeaponDirectionEnd)
{
	Assertf(pParent,"CFixedVehicleWeapon::GetForceShotInWeaponDirectionEnd pParent==NULL");
	if (!pParent)
		return;

	Assertf(m_pVehicleWeapon,"CFixedVehicleWeapon::GetForceShotInWeaponDirectionEnd m_pVehicleWeapon==NULL");
	if(!m_pVehicleWeapon)
		return;

	s32 nWeaponBoneIndex = pParent->GetBoneIndex((eHierarchyId)m_iWeaponBone);
	Assertf(nWeaponBoneIndex >= 0,"CFixedVehicleWeapon::GetForceShotInWeaponDirectionEnd nWeaponBoneIndex[%d] < 0",nWeaponBoneIndex);
	if(nWeaponBoneIndex < 0)
		return;

	Matrix34 weaponMat;
	pParent->GetGlobalMtx(nWeaponBoneIndex, weaponMat);

	ApplyBulletDirectionOffset(weaponMat, pParent);

	Vector3 vWeaponForward = m_bUseOverrideWeaponDirection ? m_vOverrideWeaponDirection : weaponMat.b;

	vForceShotInWeaponDirectionEnd = weaponMat.d + (vWeaponForward * m_pVehicleWeapon->GetRange());
}

const float fAmmoBeltRadiansPerBullet = 0.241660973353061f; // 26 bullets in the ammo belt. This constant corresponds to 2PI radians / 26 bullets.
bool CFixedVehicleWeapon::FireInternal(CVehicle* pParent)
{
	if(m_pVehicleWeapon == NULL)
		return false;

	if(pParent->GetSkeleton()==NULL)
		return false;

	s32 nWeaponBoneIndex = pParent->GetBoneIndex((eHierarchyId)m_iWeaponBone);
	if(nWeaponBoneIndex < 0)
		return false;

	if(m_projectileRequester.IsValid() && !m_projectileRequester.HasLoaded())
		return false;

	// This matches for individual CFixedVehicleWeapons, but not CFixedVehicleWeapons as part of a CVehicleWeaponBattery
	s32 iVehicleWeaponMgrIndex = pParent->GetVehicleWeaponMgr()->GetIndexOfVehicleWeapon(this);
	if (iVehicleWeaponMgrIndex != -1 && pParent->GetRestrictedAmmoCount(m_handlingIndex) == 0)
	{
		TriggerEmptyShot(pParent);
		return false;
	}

	Matrix34 weaponMat;
	pParent->GetGlobalMtx(nWeaponBoneIndex, weaponMat);

	// Boom
	CWeapon::sFireParams params( pParent, weaponMat );
	params.iVehicleWeaponBoneIndex = nWeaponBoneIndex;
	params.iVehicleWeaponIndex = m_weaponIndex;
	params.fDesiredTargetDistance = m_pVehicleWeapon->GetRange();
	params.fOverrideLaunchSpeed = m_fOverrideLaunchSpeed;

	Vector3 vEnd( Vector3::ZeroType );

	bool bIsLocalPlayer = m_pFiringPed.Get() ? m_pFiringPed->IsLocalPlayer()  : false;
    bool bIsPlayer      = m_pFiringPed.Get() ? m_pFiringPed->IsPlayer()       : false;
	bool bIsClone		= m_pFiringPed.Get() ? m_pFiringPed->IsNetworkClone() : false;

	bool bVehicleHomingEnabled = true;
	if (bIsLocalPlayer)
	{
		bVehicleHomingEnabled = m_pFiringPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingEnabled();
	}

	// Get the seat the vehicle weapon is in 
	bool bIsHomingWeapon = false;
	bool bSetTargetEntity = true;
	const CWeaponInfo* pWeaponInfo = m_pVehicleWeapon->GetWeaponInfo();
	if( pWeaponInfo && pWeaponInfo->GetIsHoming() && bVehicleHomingEnabled)
	{
		bIsHomingWeapon = true;
		float fTimeBeforeHoming = 0.0f;
		const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
		if( pAmmoInfo && pAmmoInfo->GetClassId() == CAmmoRocketInfo::GetStaticClassId() )
		{
			const CAmmoRocketInfo* pRocketInfo = static_cast<const CAmmoRocketInfo*>(pAmmoInfo);
			if(pRocketInfo)
			{
				fTimeBeforeHoming = pRocketInfo->GetTimeBeforeHoming();
			}
		}

		// Players have special conditions before homing weapon can actually home 
		if( bIsLocalPlayer )
		{
			CPlayerPedTargeting& rTargeting = m_pFiringPed->GetPlayerInfo()->GetTargeting();
			if( rTargeting.GetTimeAimingAtTarget() < fTimeBeforeHoming )
			{
				bSetTargetEntity = false;	
			}
		}
	}
	
	if( bSetTargetEntity && m_pQueuedTargetEnt )
		params.pTargetEntity = m_pQueuedTargetEnt.Get();

	// Cache off the target extents if there is a target
	if( m_pQueuedTargetEnt )
	{
		const CEntity* pTargetEntity = m_pQueuedTargetEnt.Get();
		if( pTargetEntity->GetIsTypePed() )
		{
			// Use the vehicle extents 
			if( ((const CPed*)pTargetEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
				pTargetEntity = ((const CPed*)pTargetEntity)->GetMyVehicle();
		}

		params.pTargetEntity = pTargetEntity;
	}

	// Always use the weapon mtx for homing weapons or player aircraft... 
	// otherwise the projectile will exit the vehicle at a strange angle
	bool bIsStaticReticule = pWeaponInfo && ( pParent->GetHoverMode() || pWeaponInfo->GetIsStaticReticulePosition() );
	bool bAssistedAimVehicleLockon = pWeaponInfo && pWeaponInfo->IsAssistedAimVehicleWeapon() && m_pQueuedTargetEnt;
	bool bUseWeaponBoneForward = pWeaponInfo && pWeaponInfo->GetUseVehicleWeaponBoneForward();
	const bool bCloneMountedTurretFire = bIsClone && pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE) && m_vQueuedTargetPos.IsZero();

	if( bCloneMountedTurretFire || m_bForceShotInWeaponDirection || bUseWeaponBoneForward || bIsHomingWeapon || ( bIsPlayer && pParent->GetIsAircraft() && !bIsStaticReticule ) || (pParent->InheritsFromSubmarineCar() && !bAssistedAimVehicleLockon && !bIsStaticReticule))
	{
		ApplyBulletDirectionOffset(weaponMat, pParent);

		Vector3 vWeaponForward = m_bUseOverrideWeaponDirection ? m_vOverrideWeaponDirection : weaponMat.b;

		vEnd = weaponMat.d + (vWeaponForward * m_pVehicleWeapon->GetRange());

		//TODO - Implement a proper bullet bending system in B*3161131, this is hack for the flow_play demos
		if (pParent->InheritsFromAmphibiousQuadBike())
		{
			// Check out of water timer to stop this flicking on and off when jumping across waves
			CAmphibiousQuadBike* pAmphiVeh = static_cast<CAmphibiousQuadBike*>(pParent);
			if (pAmphiVeh->m_Buoyancy.GetStatus() != NOT_IN_WATER || (pAmphiVeh->IsInAir() && pAmphiVeh->GetBoatHandling()->GetOutOfWaterTime() < 1.0f))
			{
				vEnd.z = weaponMat.d.z;
			}
		}

		params.pvEnd = &vEnd;

#if !__NO_OUTPUT
		weaponDebugf3("CFixedVehicleWeapon::FireInternal--vEnd[%f %f %f] A bCloneMountedTurretFire[%d] m_bForceShotInWeaponDirection[%d] bIsHomingWeapon[%d] otheraircraft[%d] othersubmarine[%d]",vEnd.x,vEnd.y,vEnd.z,bCloneMountedTurretFire,m_bForceShotInWeaponDirection,bIsHomingWeapon,( bIsPlayer && pParent->GetIsAircraft() && !bIsStaticReticule ),(pParent->InheritsFromSubmarineCar() && !bAssistedAimVehicleLockon && !bIsStaticReticule));
#endif
	}
	else if( m_pQueuedTargetEnt )
	{
		vEnd = VEC3V_TO_VECTOR3( m_pQueuedTargetEnt->GetTransform().GetPosition() );
		params.pvEnd = &vEnd;
		weaponDebugf3("CFixedVehicleWeapon::FireInternal--vEnd[%f %f %f] B -- m_pQueuedTargetEnt",vEnd.x,vEnd.y,vEnd.z);
	}
	else if( !m_vQueuedTargetPos.IsZero() )
	{
#if DEBUG_DRAW
		TUNE_GROUP_BOOL(TURRET_TUNE, SYNCING_bDrawQueuedTargetPos, false);
		if (SYNCING_bDrawQueuedTargetPos)
		{
			static float fReticuleSphereRadius = 0.5f;
			static bool bSolidReticule = false;
			grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_vQueuedTargetPos),fReticuleSphereRadius,Color_orange,bSolidReticule,CTaskVehicleMountedWeapon::iTurretTuneDebugTimeToLive);
		}
#endif

		params.pvEnd = &m_vQueuedTargetPos;
		weaponDebugf3("CFixedVehicleWeapon::FireInternal--vEnd[%f %f %f] C -- !m_vQueuedTargetPos.IsZero() -- m_vQueuedTargetPos",m_vQueuedTargetPos.x,m_vQueuedTargetPos.y,m_vQueuedTargetPos.z);
	}
	else
	{
#if DEBUG_DRAW
		TUNE_GROUP_BOOL(TURRET_TUNE, SYNCING_bDrawReticulePosition, false);
		if (SYNCING_bDrawReticulePosition)
		{
			static float fReticuleSphereRadius = 0.5f;
			static bool bSolidReticule = false;
			grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_v3ReticlePosition),fReticuleSphereRadius,Color_green,bSolidReticule,CTaskVehicleMountedWeapon::iTurretTuneDebugTimeToLive);
		}
#endif

		params.pvEnd = &m_v3ReticlePosition;
		weaponDebugf3("CFixedVehicleWeapon::FireInternal--vEnd[%f %f %f] D -- m_v3ReticlePosition",m_v3ReticlePosition.x,m_v3ReticlePosition.y,m_v3ReticlePosition.z);
	}

	// Figure out if the end position is outside the dot threshold and adjust the fire direction if behind (Player only)
	if( bIsLocalPlayer && pWeaponInfo && pWeaponInfo->GetEnforceFiringAngularThreshold() && !m_bForceShotInWeaponDirection )
	{
		Vector3 vShotDir = *params.pvEnd - weaponMat.d;
		vShotDir.Normalize();

		vEnd = m_bUseOverrideWeaponDirection ? m_vOverrideWeaponDirection : weaponMat.b;

		if( bIsLocalPlayer )
		{
			Matrix34 testMat = weaponMat;
			Vector3 vTestStart = weaponMat.d;
			Vector3 vTestEnd( Vector3::ZeroType );
			if( m_pVehicleWeapon->CalcFireVecFromAimCamera( m_pFiringPed.Get(), testMat, vTestStart, vTestEnd ) )
			{
				vEnd = vTestEnd - weaponMat.d;
				vEnd.Normalize();
			}
		}

		// do not allow any shots behind the turret
		static dev_float sfDotThreshold = 45.0f * DtoR;
		if( vShotDir.Dot( vEnd ) < sfDotThreshold )
			params.pvEnd = &m_v3ReticlePosition;
	}

	// If a turret weapon, assign the ped as the firing entity instead of the parent vehicle
	if (pWeaponInfo && (pWeaponInfo->GetIsTurret() || pWeaponInfo->GetForcePedAsFiringEntity()))
	{
		params.pFiringEntity = (CEntity*)(m_pFiringPed.Get());
	}

	if (NetworkInterface::IsGameInProgress() && m_pFiringPed.Get() && m_pFiringPed.Get()->IsPlayer() && !m_pFiringPed.Get()->GetIsDrivingVehicle())
	{
		params.pFiringEntity = (CEntity*)(m_pFiringPed.Get());
	}

	const bool bFired = m_pVehicleWeapon->Fire(params);
	if(bFired)
	{
		if (m_pFiringPed.Get())
		{
			CFiringPattern& firingPattern = m_pFiringPed->GetPedIntelligence()->GetFiringPattern();
			firingPattern.ProcessFired();
		}

		m_iTimeLastFired = fwTimer::GetTimeInMilliseconds();

		// Get the impulse from the weapon data
		float fImpulse = 0.0f;	
		const CVehicleWeaponInfo* pVehWeaponInfo = m_pVehicleWeapon->GetWeaponInfo()->GetVehicleWeaponInfo();	
		if(pVehWeaponInfo)
		{
			fImpulse = pVehWeaponInfo->GetKickbackImpulse();
		}

		// Apply impulse at weapon location
		if(fImpulse > 0.0f)
		{
			Vector3 vImpulse = m_bUseOverrideWeaponDirection ? -m_vOverrideWeaponDirection : -weaponMat.b;

			vImpulse.Scale(fImpulse * pParent->GetMass());

			pParent->ApplyImpulse(vImpulse,weaponMat.d - VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition()));
		}

		// If this weapon has an "ammo belt" rotate it by one bullet.
		s32 nAmmoBeltBoneIndex = pParent->GetBoneIndex(VEH_TURRET_AMMO_BELT);
		if(nAmmoBeltBoneIndex >= 0)
		{
			m_fAmmoBeltAngToTurn = fAmmoBeltRadiansPerBullet;
		}

		// This matches for individual CFixedVehicleWeapons, but not CFixedVehicleWeapons as part of a CVehicleWeaponBattery
		if (iVehicleWeaponMgrIndex != -1 && pParent->GetRestrictedAmmoCount(m_handlingIndex) > 0)
		{
			pParent->ReduceRestrictedAmmoCount(m_handlingIndex);
		}

		CCustomShaderEffectVehicle* pShaderFx = static_cast<CCustomShaderEffectVehicle*>( pParent->GetDrawHandler().GetShaderEffect() );

		if( pShaderFx )
		{
			TUNE_GROUP_FLOAT( VEHICLE_FIXED_WEAPON, AMMO_ROTATE_RATE, 0.075f, -10.0f, 10.0f, 0.01f );
			Vector2 ammoAnim = Vector2( 0.0f, AMMO_ROTATE_RATE );

			// HACK as for some reason we can't set all the vehicle weapons up to work using the same values
			if( MI_CAR_DUNE3.IsValid() && pParent->GetModelIndex() == MI_CAR_DUNE3 )
			{
				ammoAnim.x = ammoAnim.y;
				ammoAnim.y = 0.0f;
			}
			// url:bugstar:3865220 - Mogul - Ammo belts are not moving when turret MG gun is fired:
			else if( MI_PLANE_MOGUL.IsValid() && pParent->GetModelIndex() == MI_PLANE_MOGUL )
			{
				ammoAnim.x = -ammoAnim.y;
				ammoAnim.y = 0.0f;
			}

			pShaderFx->TrackAmmoUVAnimAdd( ammoAnim );
		}
	}

	return bFired;
}

strLocalIndex CFixedVehicleWeapon::GetModelLocalIndexForStreaming() const
{
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
	if(aiVerifyf(pWeaponInfo,"NULL Weapon Info"))
	{
		const CAmmoInfo* pInfo = pWeaponInfo->GetAmmoInfo();
		if(pInfo)
		{
			fwModelId modelId((strLocalIndex(pInfo->GetModelIndex())));
			strLocalIndex localIndex = CModelInfo::AssignLocalIndexToModelInfo(modelId);
			return localIndex;
		}
	}
	return strLocalIndex(-1);
}

float g_RotaryTurretAudioSpinSpeed = 1.f;

void CFixedVehicleWeapon::ProcessPreRender(CVehicle* pParent)
{
	static float fMaxRecoilDuration = 0.5f;

	// Get the kickback displacement amplitude from the weapon data
	float fKickbackAmp = 0.0f;
	const CVehicleWeaponInfo* pVehWeaponInfo = m_pVehicleWeapon ? m_pVehicleWeapon->GetWeaponInfo()->GetVehicleWeaponInfo() : NULL;
	if(pVehWeaponInfo)
	{
		fKickbackAmp = pVehWeaponInfo->GetKickbackAmplitude();
	}

	if(fKickbackAmp > 0.0f)
	{
		Assert(m_pVehicleWeapon);

		// Do recoil on cannon end
		float fTurretKickback = 0.0f;
		s32 iTimeSinceFired = fwTimer::GetTimeInMilliseconds() - m_iTimeLastFired;
		float fTimeBetweenShots = m_pVehicleWeapon->GetWeaponInfo()->GetTimeBetweenShots(); 
		if (pVehWeaponInfo->GetKickbackOverrideTiming() > 0.0f)
		{
			fTimeBetweenShots /= pVehWeaponInfo->GetKickbackOverrideTiming();
		}
		float fRecoilTime = Min(fTimeBetweenShots, fMaxRecoilDuration);
		if(iTimeSinceFired < (s32)(fRecoilTime*1000.0f))
		{
			float fFirePhase = ((float)iTimeSinceFired ) / (fRecoilTime * 1000.0f);
			if(fFirePhase > 1.0f)
			{
				fFirePhase = 1.0f;
			}

			// Try a half sine wave
			float fSin = rage::Sinf(fFirePhase*PI);		
			fTurretKickback = pParent->GetBoundingBoxMax().y * fKickbackAmp * fSin;

            if( pParent->GetModelIndex() == MI_TANK_KHANJALI )
            {
                static dev_float boltMovement = 0.15f;
                static dev_float boltPhaseScale = 1.2f;
                Vector3 vBoltRecoil = -YAXIS * boltMovement * ( 1.0f - Min( 1.0f, fFirePhase * boltPhaseScale ) );
                pParent->SetComponentRotation( VEH_MISC_G, ROT_AXIS_X, 0.0f, true, NULL, &vBoltRecoil );
            }
		}

		Vector3 vKickback = -YAXIS * fTurretKickback;
		pParent->SetComponentRotation(m_iWeaponBone,ROT_AXIS_X,0.0f,true,NULL,&vKickback);
	}	


	// Make the rotary cannons attached to certain vehicles spin while the guns are being fired (done via weapon spin code)
	if(IsWeaponBoneValidForRotaryCannon())
	{	
		s32 iRotBoneOffset = GetRotationBoneOffset();
		eHierarchyId iWeaponRotBoneId = (eHierarchyId)((s32)m_iWeaponBone + iRotBoneOffset);
		s32 nWeaponRotBoneIndex = pParent->GetBoneIndex(iWeaponRotBoneId);			
		if(m_pVehicleWeapon && nWeaponRotBoneIndex >= 0)
		{
			float fBarrelSpin = m_pVehicleWeapon->ProcessWeaponSpin(pParent, nWeaponRotBoneIndex);
			if(fBarrelSpin != 0.f)
			{
				pParent->SetComponentRotation(iWeaponRotBoneId, ROT_AXIS_LOCAL_Y, fBarrelSpin);
			}
		}
	}

	// If this weapon has an "ammo belt" rotate it by one bullet.
	s32 nAmmoBeltBoneIndex = pParent->GetBoneIndex(VEH_TURRET_AMMO_BELT);
	if(nAmmoBeltBoneIndex >= 0)
	{
		if(m_fAmmoBeltAngToTurn > 0.0f)
		{
			pParent->SetComponentRotation(VEH_TURRET_AMMO_BELT, ROT_AXIS_LOCAL_Y, fAmmoBeltRadiansPerBullet-m_fAmmoBeltAngToTurn);

			TUNE_FLOAT(kfAmmoBeltSpinRate, 2.0f, 0.0, 10.0f, 0.1f);
			m_fAmmoBeltAngToTurn -= kfAmmoBeltSpinRate * fwTimer::GetTimeStep();

			if(m_fAmmoBeltAngToTurn < 0.0f) m_fAmmoBeltAngToTurn = 0.0f;
		}
	}
}

void CFixedVehicleWeapon::ProcessPostPreRender(CVehicle* pParent)
{
#if DEBUG_DRAW
	TUNE_GROUP_BOOL(TURRET_TUNE, bDrawCalculatedReticlePositionProbe, false);
	static float fArrowHeadSize = 0.5f;
	static int iTimeToLive = 1;
#endif

	// Cache of vehicle reticle world position
	if( m_pVehicleWeapon && pParent->ContainsLocalPlayer() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		aiFatalAssertf(pParent, "CVehicle parent is NULL.");
		s32 nWeaponBoneIndex = pParent->GetBoneIndex((eHierarchyId)m_iWeaponBone);
		if(nWeaponBoneIndex >= 0)
		{
			Matrix34 weaponMat;
			pParent->GetGlobalMtx(nWeaponBoneIndex, weaponMat);
			const CWeaponInfo* pWeaponInfo = m_pVehicleWeapon->GetWeaponInfo();
			const bool bIsStaticReticule = pWeaponInfo && ( pParent->GetHoverMode() || pWeaponInfo->GetIsStaticReticulePosition() );
			const bool bUseWeaponBoneForward = pWeaponInfo && pWeaponInfo->GetUseVehicleWeaponBoneForward();
			const bool bIsTampa3 = MI_CAR_TAMPA3.IsValid() && pParent->GetModelIndex() == MI_CAR_TAMPA3;
			const bool bIsKhanjali = MI_TANK_KHANJALI.IsValid() && pParent->GetModelIndex() == MI_TANK_KHANJALI;
			bool bIsFPSInTurret = pWeaponInfo && pWeaponInfo->GetIsTurret() && camInterface::GetCinematicDirector().IsRendering() && camInterface::GetCinematicDirector().GetFirstPersonVehicleCamera();

			if (bIsTampa3 || bIsKhanjali)
			{
				const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
				if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
				{
					const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
					const bool shouldAffectAiming					= pMountedCamera->ShouldAffectAiming();
					if(shouldAffectAiming)
					{
						bIsFPSInTurret = true;
					}
				}
			}

			Vector3 vReticuleProbeStart = weaponMat.d;
			TUNE_GROUP_BOOL(TURRET_TUNE, bTampa3EnableNewTurretForwardCalculations, true);
			TUNE_GROUP_FLOAT(TURRET_TUNE, fTampa3TurretProbeStartOffsetZ, 1.060f, -100.f, 100.f, 0.1f);
			bool bTampa3Turret = bTampa3EnableNewTurretForwardCalculations && bIsTampa3 && pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("VEHICLE_WEAPON_TAMPA_FIXEDMINIGUN",0xDAC57AAD);
			if (bTampa3Turret)
			{
				vReticuleProbeStart = VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition());
				vReticuleProbeStart.SetZ(vReticuleProbeStart.GetZ() + fTampa3TurretProbeStartOffsetZ);
			}

			if( (pParent->GetIsAircraft() && !bIsStaticReticule) || (pParent->InheritsFromSubmarineCar() && !bIsStaticReticule) || (bUseWeaponBoneForward && !bIsStaticReticule))
			{
				// If we're slightly adjusting the pitch in the weapon info, also adjust the weapon matrix for the reticule calculation probe
				if(pWeaponInfo && pWeaponInfo->GetBulletDirectionPitchOffset() != 0.0f)
				{
					weaponMat.RotateLocalX(DEGREES_TO_RADIANS(pWeaponInfo->GetBulletDirectionPitchOffset()));
				}

				Vector3 vWeaponForward;
				if (m_bUseOverrideWeaponDirection)
				{
					vWeaponForward = m_vOverrideWeaponDirection;
				}
				else 
				{
					vWeaponForward = weaponMat.b;

					if (bTampa3Turret)
					{
						vWeaponForward = GetForwardNormalBasedOnGround(pParent);
					}
				}

				f32 fProbeDist = GetProbeDist();
				Vector3 vReticuleProbeEnd = vReticuleProbeStart + vWeaponForward * fProbeDist;

				if( fProbeDist > 0.0f )
				{
					WorldProbe::CShapeTestProbeDesc probeDesc;
					probeDesc.SetStartAndEnd(vReticuleProbeStart,vReticuleProbeEnd);
					probeDesc.SetIncludeFlags( ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PICKUP_TYPE);
					probeDesc.SetExcludeEntity(pParent);

					WorldProbe::CShapeTestHitPoint probeIsect;
					WorldProbe::CShapeTestResults probeResults(probeIsect);
					probeDesc.SetResultsStructure(&probeResults);

					if( WorldProbe::GetShapeTestManager()->SubmitTest( probeDesc ) )
					{
						for( int i = 0; i < probeResults.GetNumHits(); i++ )
						{
							phMaterialMgr::Id materialId = probeResults[ i ].GetMaterialId();

							if( materialId != PGTAMATERIALMGR->g_idPhysVehicleSpeedUp &&
								materialId != PGTAMATERIALMGR->g_idPhysVehicleSlowDown &&
								materialId != PGTAMATERIALMGR->g_idPhysVehicleRefill &&
								materialId != PGTAMATERIALMGR->g_idPhysVehicleBoostCancel &&
								materialId != PGTAMATERIALMGR->g_idPhysPropPlacement 
								)
							{
								fProbeDist = ( probeResults[ 0 ].GetHitPosition() - vReticuleProbeStart ).Mag();
								break;
							}
						}
					}
#if DEBUG_DRAW
					if (bDrawCalculatedReticlePositionProbe)
					{
						grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vReticuleProbeStart), VECTOR3_TO_VEC3V(vReticuleProbeEnd),fArrowHeadSize,Color_magenta2,iTimeToLive);
					}
#endif
				}

				m_v3ReticlePosition = vReticuleProbeStart + vWeaponForward * fProbeDist;

				//TODO - Implement a proper bullet bending system in B*3161131, this is hack for the flow_play demos
				if (pParent->InheritsFromAmphibiousQuadBike())
				{
					// Check out of water timer to stop this flicking on and off when jumping across waves
					CAmphibiousQuadBike* pAmphiVeh = static_cast<CAmphibiousQuadBike*>(pParent);
					if (pAmphiVeh->m_Buoyancy.GetStatus() != NOT_IN_WATER || (pAmphiVeh->IsInAir() && pAmphiVeh->GetBoatHandling()->GetOutOfWaterTime() < 1.0f))
					{
						m_v3ReticlePosition.z = vReticuleProbeStart.z;
					}
				}
			}
			// Determine the exact location the reticule is pointing at and use that position
			else if( bIsStaticReticule && (!camInterface::GetCinematicDirector().IsRendering() || bIsFPSInTurret))
			{
				Vector2 v2ReticulePostion =  pWeaponInfo->GetReticuleHudPosition();
				if (bIsFPSInTurret)
				{
					v2ReticulePostion += pWeaponInfo->GetReticuleHudPositionOffsetForPOVTurret();
				}

				float fViewportX = v2ReticulePostion.x * (float)gVpMan.GetGameViewport()->GetGrcViewport().GetWidth();
				float fViewportY = v2ReticulePostion.y * (float)gVpMan.GetGameViewport()->GetGrcViewport().GetHeight();
				
				Vector3	vStart, vEnd;
				gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform( fViewportX, fViewportY, (Vec3V&)vStart, (Vec3V&)vEnd );
			
#if DEBUG_DRAW
				if (bDrawCalculatedReticlePositionProbe)
				{
					grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd),fArrowHeadSize,Color_magenta2,iTimeToLive);
				}
#endif

				// check for nans in position passed in.
				if( vStart == vStart && vEnd == vEnd )
				{
					WorldProbe::CShapeTestProbeDesc probeDesc;
					WorldProbe::CShapeTestFixedResults<> probeResult;
					probeDesc.SetResultsStructure( &probeResult );
					probeDesc.SetStartAndEnd( vStart, vEnd );
					probeDesc.SetExcludeEntity( pParent );
					probeDesc.SetIncludeFlags( ArchetypeFlags::GTA_PROJECTILE_INCLUDE_TYPES & ~ArchetypeFlags::GTA_PICKUP_TYPE);

					bool hitSomething = false;

					// If we hit something use that as the target
					if( WorldProbe::GetShapeTestManager()->SubmitTest( probeDesc ) )
					{
						for( int i = 0; i < probeResult.GetNumHits(); i++ )
						{
							phMaterialMgr::Id materialId = probeResult[ i ].GetMaterialId();

							if( materialId != PGTAMATERIALMGR->g_idPhysVehicleSpeedUp &&
								materialId != PGTAMATERIALMGR->g_idPhysVehicleSlowDown &&
								materialId != PGTAMATERIALMGR->g_idPhysVehicleRefill &&
								materialId != PGTAMATERIALMGR->g_idPhysVehicleBoostCancel &&
								materialId != PGTAMATERIALMGR->g_idPhysPropPlacement 
								)
							{
								m_v3ReticlePosition = probeResult[ i ].GetHitPosition();
								hitSomething = true;
								break;
							}
						}
					}
					// Otherwise scale the start in the direction by the weapon range
					if( !hitSomething )
					{
						Vector3 vDirection = vEnd - vStart;
						vDirection.Normalize();
						m_v3ReticlePosition = vStart + ( vDirection * pWeaponInfo->GetRange() );
					}
				}					
			}
			else
			{
				// Calculate the firing vector from the weapon camera that goes through the target position
				m_pVehicleWeapon->CalcFireVecFromAimCamera( NULL, weaponMat, vReticuleProbeStart, m_v3ReticlePosition );
			}
		}
	}

#if DEBUG_DRAW
	TUNE_GROUP_BOOL(TURRET_TUNE, bDrawCalculatedReticlePosition, false);
	if (bDrawCalculatedReticlePosition)
	{
		static float fReticuleSphereRadius = 0.5f;
		static bool bSolidReticule = false;
		grcDebugDraw::Sphere(VECTOR3_TO_VEC3V(m_v3ReticlePosition),fReticuleSphereRadius,Color_magenta,bSolidReticule,iTimeToLive);
	}
#endif

	// Need to queue weapon fire so that weapon matrix is definitely as up to date as can be
	if(m_bFireIsQueued)
	{
		FireInternal(pParent);
		m_bFireIsQueued = false;
		m_bForceShotInWeaponDirection = false;
		m_pQueuedTargetEnt = NULL;
		m_vQueuedTargetPos.Zero();
	}

	ClearOverrideWeaponDirection();
}

Vector3 CFixedVehicleWeapon::GetForwardNormalBasedOnGround(const CVehicle* pVehicle)
{
	Vector3 vAverageNormal;
	vAverageNormal.Zero();

#if DEBUG_DRAW
	TUNE_GROUP_BOOL(TURRET_TUNE, bDrawForwardNormalCalculations, false);
	static u32 uTimeToLive = 1;
#endif

	TUNE_GROUP_BOOL(TURRET_TUNE, bTampa3TurretFwdCalculateAverageWithVehForward,true);
	TUNE_GROUP_BOOL(TURRET_TUNE, bTampa3TurretJustUseVehForward,false);
	TUNE_GROUP_BOOL(TURRET_TUNE, bTampa3TurretFwdLerpBetweenFrames,true);
	TUNE_GROUP_BOOL(TURRET_TUNE, bTampa3TurretFwdLerpBetweenFramesOnlyZ,true);
	TUNE_GROUP_FLOAT(TURRET_TUNE, fTampa3TurretFwdLerpRate, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, fTampa3TurretFwdVerticalRotationOffsetDegrees, -1.5f,-360.0f,360.0f,1.0f);

	u32 uWheelsFound=0;
	for (u32 i=0; i < pVehicle->GetNumWheels(); i++)
	{
		const CWheel* pWheel = pVehicle->GetWheel(i);
		if (pWheel)
		{
			Vector3 vGroundNormal = pWheel->GetHitNormal();

#if DEBUG_DRAW
			Vector3 vGroundPos = pWheel->GetHitPos();
			static float fArrowLength = 3.0f;
			static float fArrowHeadSize = 0.20f;
			if (bDrawForwardNormalCalculations)
				grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vGroundPos), VECTOR3_TO_VEC3V(vGroundPos + vGroundNormal * fArrowLength), fArrowHeadSize, Color_green, uTimeToLive);
#endif

			vAverageNormal += vGroundNormal;
			++uWheelsFound;
		}
	}

	if (uWheelsFound == 0 || bTampa3TurretJustUseVehForward)
	{
		return VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
	}

	vAverageNormal /= (float)uWheelsFound;

	Vector3 vVehPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	Vector3 vVehForward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
	Vector3 vVehRight = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetRight());
	Vector3 vNewForward;
	vNewForward.Cross(vAverageNormal,vVehRight);

#if DEBUG_DRAW
	static float fVehArrowLength = 3.0f;
	static float fForwardArrowLength = 10.0f;
	static float fVehArrowHeadSize = 0.25f;
	if (bDrawForwardNormalCalculations)
	{
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vVehPos), VECTOR3_TO_VEC3V(vVehPos + vAverageNormal * fVehArrowLength), fVehArrowHeadSize, Color_green3, uTimeToLive);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vVehPos), VECTOR3_TO_VEC3V(vVehPos + vVehRight * fVehArrowLength), fVehArrowHeadSize, Color_blue, uTimeToLive);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vVehPos), VECTOR3_TO_VEC3V(vVehPos + vVehForward * fForwardArrowLength), fVehArrowHeadSize, Color_blue, uTimeToLive);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vVehPos), VECTOR3_TO_VEC3V(vVehPos + vNewForward * fForwardArrowLength), fVehArrowHeadSize, (bTampa3TurretFwdCalculateAverageWithVehForward || bTampa3TurretFwdLerpBetweenFrames)  ? Color_yellow : Color_red, uTimeToLive);
	}
#endif

	if (bTampa3TurretFwdCalculateAverageWithVehForward)
	{
		vNewForward = (vNewForward + vVehForward) / 2;

#if DEBUG_DRAW
		if (!bTampa3TurretFwdLerpBetweenFrames && bDrawForwardNormalCalculations)
		{
			grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vVehPos), VECTOR3_TO_VEC3V(vVehPos + vNewForward * fForwardArrowLength), fVehArrowHeadSize, Color_red, uTimeToLive);
		}
#endif
	}

	if (bTampa3TurretFwdLerpBetweenFrames)
	{
		if (m_vTurretForwardLastUpdate.IsZero())
			m_vTurretForwardLastUpdate = vNewForward;

		if (bTampa3TurretFwdLerpBetweenFramesOnlyZ)
		{
			float fNewZ = Lerp(fTampa3TurretFwdLerpRate,m_vTurretForwardLastUpdate.GetZ(),vNewForward.GetZ());
			vNewForward.SetZ(fNewZ);
		}
		else
		{
			const ScalarV scLerpRate(LoadScalar32IntoScalarV(fTampa3TurretFwdLerpRate));
			vNewForward = VEC3V_TO_VECTOR3(Lerp(scLerpRate,VECTOR3_TO_VEC3V(m_vTurretForwardLastUpdate),VECTOR3_TO_VEC3V(vNewForward)));
		}

		m_vTurretForwardLastUpdate = vNewForward;
	}

	Matrix34 rotMat;
	rotMat.Identity();
	rotMat.d = vVehPos;
	rotMat.a = vVehRight;
	rotMat.b = vNewForward;
	rotMat.c = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetUp());
	rotMat.RotateLocalX(DEGREES_TO_RADIANS(fTampa3TurretFwdVerticalRotationOffsetDegrees));

	vNewForward = rotMat.b;

#if DEBUG_DRAW
	if (bDrawForwardNormalCalculations)
	{
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vVehPos), VECTOR3_TO_VEC3V(vVehPos + vNewForward * fForwardArrowLength), fVehArrowHeadSize, Color_red, uTimeToLive);
	}
#endif

	return vNewForward;
}

void CFixedVehicleWeapon::SetOverrideWeaponDirection(const Vector3 &vOverrideDirection)
{
	m_bUseOverrideWeaponDirection = true;
	m_vOverrideWeaponDirection = vOverrideDirection;
}

void CFixedVehicleWeapon::SetOverrideLaunchSpeed(float fOverrideLaunchSpeed)
{
	m_fOverrideLaunchSpeed = fOverrideLaunchSpeed;
}

bool CFixedVehicleWeapon::IsWeaponBoneValidForRotaryCannon()
{
	return  (m_iWeaponBone >= VEH_WEAPON_1_FIRST && m_iWeaponBone <= VEH_WEAPON_1_LAST) ||
			(m_iWeaponBone >= VEH_WEAPON_2_FIRST && m_iWeaponBone <= VEH_WEAPON_2D)		||	// This is weird. We should just add extra rot bones.
			(m_iWeaponBone >= VEH_WEAPON_3_FIRST && m_iWeaponBone <= VEH_WEAPON_3_LAST) ||
			(m_iWeaponBone >= VEH_WEAPON_4_FIRST && m_iWeaponBone <= VEH_WEAPON_4_LAST) ||
			(m_iWeaponBone >= VEH_WEAPON_5_FIRST && m_iWeaponBone <= VEH_WEAPON_5_LAST) ||
			(m_iWeaponBone >= VEH_WEAPON_6_FIRST && m_iWeaponBone <= VEH_WEAPON_6_LAST);
}

s32 CFixedVehicleWeapon::GetRotationBoneOffset()
{
	if (m_iWeaponBone >= VEH_WEAPON_1_FIRST && m_iWeaponBone <= VEH_WEAPON_1_LAST)
	{
		return VEH_WEAPON_1_LAST + 1 - VEH_WEAPON_1_FIRST;
	}
	else if (m_iWeaponBone >= VEH_WEAPON_2_FIRST && m_iWeaponBone <= VEH_WEAPON_2D)
	{
		return VEH_WEAPON_2_LAST + 1 - VEH_WEAPON_2_FIRST;
	}
	else if (m_iWeaponBone >= VEH_WEAPON_3_FIRST && m_iWeaponBone <= VEH_WEAPON_3_LAST)
	{
		return VEH_WEAPON_3_LAST + 1 - VEH_WEAPON_3_FIRST;
	}
	else if (m_iWeaponBone >= VEH_WEAPON_4_FIRST && m_iWeaponBone <= VEH_WEAPON_4_LAST)
	{
		return VEH_WEAPON_4_LAST + 1 - VEH_WEAPON_4_FIRST;
	}
	else if (m_iWeaponBone >= VEH_WEAPON_5_FIRST && m_iWeaponBone <= VEH_WEAPON_5_LAST)
	{
		return VEH_WEAPON_5_LAST + 1 - VEH_WEAPON_5_FIRST;
	}
	else if (m_iWeaponBone >= VEH_WEAPON_6_FIRST && m_iWeaponBone <= VEH_WEAPON_6_LAST)
	{
		return VEH_WEAPON_6_LAST + 1 - VEH_WEAPON_6_FIRST;
	}
	else
	{
		vehicleAssertf(0, "Looking up a rotation offset for an unsupported weapon bone, use IsWeaponBoneValidForRotaryCannon first");
		return 0;
	}
}

#if !__NO_OUTPUT
const char* CFixedVehicleWeapon::GetName() const 
{
	if(m_pVehicleWeapon)
	{
		return m_pVehicleWeapon->GetWeaponInfo()->GetName();
	}

	return "\0";
}
#endif

void CFixedVehicleWeapon::ResetIncludeFlags(CVehicle* pParent) 
{
	eHierarchyId weaponBoneId = this->GetWeaponBone();
		
	phBound* vehicleBound = pParent->GetVehicleFragInst()->GetArchetype()->GetBound();
	if(vehicleBound && vehicleBound->GetType() == phBound::COMPOSITE)
	{		
		phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(vehicleBound);
		int iComponent = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(pParent->GetBoneIndex(weaponBoneId));		
		if(iComponent > -1)			
		{						
			u32 includeFlags = pBoundComp->GetIncludeFlags(iComponent) | ArchetypeFlags::GTA_CAMERA_TEST;
			pBoundComp->SetIncludeFlags(iComponent, includeFlags);
		}
	}
}

void CFixedVehicleWeapon::ApplyBulletDirectionOffset(Matrix34 &weaponMat, CVehicle* pParent)
{
	const CWeaponInfo* pWeaponInfo = m_pVehicleWeapon->GetWeaponInfo();

	// Yaw Offsets - Alternate depending on side of vehicle
	float fBulletDirectionOffset = pWeaponInfo->GetBulletDirectionOffset();
	if(fBulletDirectionOffset != 0.0f)
	{
		bool bLeftSide = false;

		// For vehicles with a large battery of a weapon type, some go in a sequential order rather than alternating sides.
		bool bBonesInSequentialOrder = MI_HELI_POLICE_2.IsValid() && pParent->GetModelIndex() == MI_HELI_POLICE_2; //ANNIHILATOR

		if (bBonesInSequentialOrder)
		{
			if(	m_iWeaponBone == VEH_WEAPON_1A || m_iWeaponBone == VEH_WEAPON_1B || 
				m_iWeaponBone == VEH_WEAPON_2A || m_iWeaponBone == VEH_WEAPON_2B )
			{
				bLeftSide = true;
			}
		}
		else
		{
			if(	m_iWeaponBone == VEH_WEAPON_1A || m_iWeaponBone == VEH_WEAPON_1C || 
				m_iWeaponBone == VEH_WEAPON_2A || m_iWeaponBone == VEH_WEAPON_2C || m_iWeaponBone == VEH_WEAPON_2E || m_iWeaponBone == VEH_WEAPON_2G ||
				m_iWeaponBone == VEH_WEAPON_3A || m_iWeaponBone == VEH_WEAPON_3C || 
				m_iWeaponBone == VEH_WEAPON_4A || m_iWeaponBone == VEH_WEAPON_4C || 
				m_iWeaponBone == VEH_WEAPON_5A || m_iWeaponBone == VEH_WEAPON_5C || m_iWeaponBone == VEH_WEAPON_5E || m_iWeaponBone == VEH_WEAPON_5G ||
				m_iWeaponBone == VEH_WEAPON_5I || m_iWeaponBone == VEH_WEAPON_5K || m_iWeaponBone == VEH_WEAPON_5M ||
				m_iWeaponBone == VEH_WEAPON_6A || m_iWeaponBone == VEH_WEAPON_6C ) 
			{
				bLeftSide = true;
			}
		}

		weaponMat.RotateLocalZ(bLeftSide ? DEGREES_TO_RADIANS(-fBulletDirectionOffset) : DEGREES_TO_RADIANS(fBulletDirectionOffset));
	}

	// Pitch Offsets - Apply the same for all
	bool bUseHomingOffset = false;
	if (pParent->GetDriver() && pParent->GetDriver()->IsPlayer())
	{
		TUNE_GROUP_FLOAT(ROCKET_TUNE, fMinTargetDistanceToApplyPitchLaunchOffset, 30.0f, 0.0f, 100.0f, 0.01f);
		TUNE_GROUP_FLOAT(ROCKET_TUNE, fMinTargetHeightDiffToApplyPitchLaunchOffset, 5.0f, 0.0f, 100.0f, 0.01f);

		const CPlayerPedTargeting& rPlayerTargeting = pParent->GetDriver()->GetPlayerInfo()->GetTargeting();
		bool bHasTarget = rPlayerTargeting.GetLockOnTarget() != NULL; 
		bool bPassesTargetDistanceCheck = weaponMat.d.Dist2(rPlayerTargeting.GetLockonTargetPos()) > rage::square(fMinTargetDistanceToApplyPitchLaunchOffset);
		bool bPassesTargetHeightCheck = Abs(weaponMat.d.z - rPlayerTargeting.GetLockonTargetPos().z) > fMinTargetHeightDiffToApplyPitchLaunchOffset;

		bUseHomingOffset = bHasTarget && (bPassesTargetDistanceCheck || bPassesTargetHeightCheck);
	}

	float fBulletDirectionPitchOffset = bUseHomingOffset ? pWeaponInfo->GetBulletDirectionPitchHomingOffset() : pWeaponInfo->GetBulletDirectionPitchOffset();
	if(fBulletDirectionPitchOffset != 0.0f)
	{
		weaponMat.RotateLocalX(DEGREES_TO_RADIANS(fBulletDirectionPitchOffset));
	}

}

bool CFixedVehicleWeapon::IsUnderwater(const CVehicle* pParent) const
{
	s32 nWeaponBoneIndex = pParent->GetBoneIndex(m_iWeaponBone);
	if (nWeaponBoneIndex >= 0)
	{
		Matrix34 weaponMat;
		pParent->GetGlobalMtx(nWeaponBoneIndex, weaponMat);
		Vector3 vWeaponPosition = weaponMat.d;
		float waterZ = 0.f;
		if (pParent->m_Buoyancy.GetWaterLevelIncludingRivers(vWeaponPosition, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
		{
			if (waterZ > vWeaponPosition.z)
			{
				return true;
			}
		}
	}	
	return false;
}

const float CVehicleWaterCannon::FIRETRUCK_CANNON_JET_SPEED	= 25.0f;
const float CVehicleWaterCannon::FIRETRUCK_CANNON_RANGE	= 25.0f;

CVehicleWaterCannon::CVehicleWaterCannon(u32 /*nWeaponHash*/, eHierarchyId iWeaponBone)
{
	m_iWeaponBone = iWeaponBone;
	m_iTimeLastFired = 0;
	m_bFireIsQueued = false;
	m_bForceShotInWeaponDirection = false;
 	m_pVehicleWeapon = NULL;
 	m_pQueuedTargetEnt = NULL;
	m_CannonDirLastFrame = Vec3V(V_ZERO);
	m_vQueuedTargetPos.Zero();
}

CVehicleWaterCannon::~CVehicleWaterCannon()
{
	if(m_pVehicleWeapon)
	{
		delete m_pVehicleWeapon;
	}
}

const CWeaponInfo* CVehicleWaterCannon::GetWeaponInfo() const
{
	if (m_pVehicleWeapon)
	{
		return m_pVehicleWeapon->GetWeaponInfo();
	}
	
	return NULL;
}

u32 CVehicleWaterCannon::GetHash() const
{
	if (m_pVehicleWeapon)
	{
		return m_pVehicleWeapon->GetWeaponHash();
	}
	return WEAPONTYPE_WATER_CANNON;
}

#if !__NO_OUTPUT
const char* CVehicleWaterCannon::GetName() const 
{
	return "WATER_CANNON";
}
#endif

void CVehicleWaterCannon::ProcessControl(CVehicle* pParent)
{
	CFixedVehicleWeapon::ProcessControl(pParent);	
}

//This function allows cloned vehicles to update collision of the water stream to reflect remote machine
bool  CVehicleWaterCannon::FireWaterCannonFromMatrixPositionAndAlignment(CVehicle* pParent, const Matrix34 &weaponMat)
{
	if(pParent->GetSkeleton()==NULL)
		return false;

	int nWeaponBoneIndex = pParent->GetBoneIndex((eHierarchyId)m_iWeaponBone);
	if(nWeaponBoneIndex < 0)
		return false;

	// Splash
	const bool bFired = true;
	if(bFired)
	{
		m_iTimeLastFired = fwTimer::GetTimeInMilliseconds();

		static Vec3V nozzleOffset(0.0f, 1.0f, 0.6f);
		Vec3V vNozzlePos = nozzleOffset;
		vNozzlePos = Transform(MATRIX34_TO_MAT34V(weaponMat), vNozzlePos);

		//Vec3f nozzlePos;
		//LoadAsScalar(nozzlePos, vNozzlePos);

		Vec3V waterCannonDir = VECTOR3_TO_VEC3V(weaponMat.b);
		ScalarV fireCannonSpeed(FIRETRUCK_CANNON_JET_SPEED);
		Vec3V cannonVel =  waterCannonDir * fireCannonSpeed;

		g_waterCannonMan.RegisterCannon(pParent, fwIdKeyGenerator::Get(pParent, nWeaponBoneIndex), vNozzlePos, cannonVel, m_iWeaponBone);
	}
	return bFired;
}

bool CVehicleWaterCannon::FireInternal(CVehicle* pParent)
{
 	if(pParent->GetSkeleton()==NULL)
		return false;

	int nWeaponBoneIndex = pParent->GetBoneIndex((eHierarchyId)m_iWeaponBone);
	if(nWeaponBoneIndex < 0)
		return false;

	Matrix34 weaponMat;
	pParent->GetGlobalMtx(nWeaponBoneIndex, weaponMat);

	// Splash
 	const bool bFired = true;
 	if(bFired)
 	{
 		m_iTimeLastFired = fwTimer::GetTimeInMilliseconds();

		static Vec3V nozzleOffset(0.0f, 1.0f, 0.6f);
		Vec3V vNozzlePos = nozzleOffset;
		vNozzlePos = Transform(MATRIX34_TO_MAT34V(weaponMat), vNozzlePos);

		//Vec3f nozzlePos;
		//LoadAsScalar(nozzlePos, vNozzlePos);

		Vec3V waterCannonDir = VECTOR3_TO_VEC3V(weaponMat.b);
		ScalarV fireCannonSpeed(FIRETRUCK_CANNON_JET_SPEED);
		Vec3V cannonVel =  waterCannonDir * fireCannonSpeed;

		//TODO:  network support?
		g_waterCannonMan.RegisterCannon(pParent, fwIdKeyGenerator::Get(pParent, nWeaponBoneIndex), vNozzlePos, cannonVel, m_iWeaponBone);
 	}
 	return bFired;
}

//////////////////////////////////////////////////////////////////////////
// Class CVehicleWeaponBattery
CVehicleWeaponBattery::CVehicleWeaponBattery()
	: m_MissileBatteryState(MBS_IDLE)
	, m_iLastFiredWeapon(-1)
{
	for(s32 i = 0; i < MAX_NUM_BATTERY_WEAPONS; i++)
	{
		m_VehicleWeapons[i] = NULL;
		m_MissileFlapState[i] = MFS_CLOSED;
	}
	
	m_iNumVehicleWeapons = 0;
	m_iCurrentWeaponCounter = 0;
	m_uNextLaunchTime = 0;
}

CVehicleWeaponBattery::~CVehicleWeaponBattery()
{
	DeleteAllVehicleWeapons();
}

float CVehicleWeaponBattery::sm_fOppressor2MissileAlternateWaitTimeOverride = 0.0f;

void CVehicleWeaponBattery::InitTunables()
{
	sm_fOppressor2MissileAlternateWaitTimeOverride = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("OPPRESSOR2_MISSILE_ALTERNATE_WAIT_TIME", 0x93C9EAC0), sm_fOppressor2MissileAlternateWaitTimeOverride);
}

void CVehicleWeaponBattery::DeleteAllVehicleWeapons()
{
	for(s32 i = 0; i < MAX_NUM_BATTERY_WEAPONS; i++)
	{
		if(m_VehicleWeapons[i])
		{
			delete m_VehicleWeapons[i];
		}
		m_VehicleWeapons[i] = NULL;
	}
}

#if !__NO_OUTPUT
const char* CVehicleWeaponBattery::GetName() const
{
	return m_debugName.c_str();
}
#endif

#if !__FINAL
bool CVehicleWeaponBattery::HasWeapon(const CWeapon* pWeapon) const
{
	for(s32 i = 0; i < GetNumWeaponsInBattery(); i++)
		if(m_VehicleWeapons[i]->HasWeapon(pWeapon))
			return true;
	return false;
}
#endif

void CVehicleWeaponBattery::AddVehicleWeapon(CVehicleWeapon* pNewWeapon)
{
	AssertMsg(m_iNumVehicleWeapons < MAX_NUM_BATTERY_WEAPONS, "CVehicleWeaponBattery::AddVehicleWeapon: Array is full");

	if(m_iNumVehicleWeapons < MAX_NUM_BATTERY_WEAPONS)
	{
		m_VehicleWeapons[m_iNumVehicleWeapons] = pNewWeapon;
		m_VehicleWeapons[m_iNumVehicleWeapons]->m_weaponIndex = m_iNumVehicleWeapons;
		m_iNumVehicleWeapons ++;
	}

#if !__NO_OUTPUT
	// update the debug string
	char tempBuffer[128];
	formatf(tempBuffer,128,"%s x%i",pNewWeapon->GetName(),GetNumWeaponsInBattery());
	m_debugName = tempBuffer;
#endif
}

void CVehicleWeaponBattery::ProcessControl(CVehicle* pParent)
{
	Assert(m_iCurrentWeaponCounter < m_iNumVehicleWeapons);
	
	// Are there any weapons in this battery?
	for(s32 i = 0; i < m_iNumVehicleWeapons; i++)
	{
		Assert(m_VehicleWeapons[i]);
		if( m_VehicleWeapons[i] && m_VehicleWeapons[i]->GetIsEnabled() )
		{
			m_VehicleWeapons[i]->ProcessControl(pParent);
		}		
	}
	CVehicleWeapon::ProcessControl(pParent);
}

void CVehicleWeaponBattery::ProcessPreRender(CVehicle* pParent)
{
	Assert(m_iCurrentWeaponCounter < m_iNumVehicleWeapons);

	// Are there any weapons in this battery?
	for(s32 i = 0; i < m_iNumVehicleWeapons; i++)
	{
		Assert(m_VehicleWeapons[i]);
		if( m_VehicleWeapons[i] && m_VehicleWeapons[i]->GetIsEnabled() )
		{
			m_VehicleWeapons[i]->ProcessPreRender(pParent);
		}		
	}
	CVehicleWeapon::ProcessPreRender(pParent);
}

void CVehicleWeaponBattery::ProcessPostPreRender(CVehicle* pParent)
{
	Assert(m_iCurrentWeaponCounter < m_iNumVehicleWeapons);

	// Are there any weapons in this battery?
	for(s32 i = 0; i < m_iNumVehicleWeapons; i++)
	{
		Assert(m_VehicleWeapons[i]);
		if( m_VehicleWeapons[i] && m_VehicleWeapons[i]->GetIsEnabled() )
		{
			m_VehicleWeapons[i]->ProcessPostPreRender(pParent);
		}		
	}
	CVehicleWeapon::ProcessPostPreRender(pParent);
}

void CVehicleWeaponBattery::IncrementCurrentWeaponCounter(CVehicle& rVeh)
{
	m_iCurrentWeaponCounter++;
	if (m_iCurrentWeaponCounter >= m_iNumVehicleWeapons)
	{
		m_iCurrentWeaponCounter = 0;
		m_MissileBatteryState = MBS_RELOADING;

		if (rVeh.IsTrailerSmall2() && rVeh.GetVehicleAudioEntity())
		{
			// Get remaining ammo in battery
			int iBatteryAmmo = rVeh.GetRestrictedAmmoCount(m_handlingIndex);
			if (iBatteryAmmo != 0)
			{
				rVeh.GetVehicleAudioEntity()->TriggerMissileBatteryReloadSound();
			}
		}

		for (s32 i = 0; i < MAX_NUM_BATTERY_WEAPONS; i++)
		{
			if (m_MissileFlapState[i] != MFS_CLOSED)
			{
				m_MissileFlapState[i] = MFS_CLOSING;
			}
		}
	}
}

void CVehicleWeaponBattery::Fire(const CPed* pFiringPed, CVehicle* pParent, const Vector3& vTargetPosition, const CEntity* pTarget, bool UNUSED_PARAM( bForceShotInWeaponDirection ) )
{
	if(m_iNumVehicleWeapons > 0)
	{
		bool bCanFire = true;
		bool bOutOfAmmo = false;
		
		s32 iVehicleWeaponMgrIndex = pParent->GetVehicleWeaponMgr()->GetIndexOfVehicleWeapon(this);
		// alternate rocket firing.
		const CWeaponInfo* pWeaponInfo = m_VehicleWeapons[m_iCurrentWeaponCounter]->GetWeaponInfo();
		if( pWeaponInfo )
		{
			bCanFire = fwTimer::GetTimeInMilliseconds() > m_uNextLaunchTime;
			bOutOfAmmo = pParent->IsTrailerSmall2() && pParent->GetRestrictedAmmoCount(m_handlingIndex) == 0;

			if (bOutOfAmmo)
			{
				bCanFire = false;
			}

			if (bCanFire)
			{
				const float fWaitTime = sm_fOppressor2MissileAlternateWaitTimeOverride > 0.0f ? sm_fOppressor2MissileAlternateWaitTimeOverride : pWeaponInfo->GetAlternateWaitTime();
				if (fWaitTime > 0.0f)
				{
					m_uNextLaunchTime = fwTimer::GetTimeInMilliseconds() + (u32)(1000.0f * fWaitTime);
				}
			}
		}

		// Check to see if the entire weapon battery is disabled (in parent CVehicleWeapon)
		if (!this->GetIsEnabled())
		{
			bCanFire = false;
		}	
		// Check to see if the individual CVehicleWeapon inside the battery array is disabled (which I'm not sure is possible to set?)
		else if (!m_VehicleWeapons[m_iCurrentWeaponCounter]->GetIsEnabled())
		{
			bCanFire = false;
			
			// Allow us to cycle through weapons even if they're not enabled so we can continually fire
			if (pParent->IsTrailerSmall2() && !bOutOfAmmo)
			{
				IncrementCurrentWeaponCounter(*pParent);
			}
		}
		// Check to see if we've reached a restricted ammo count for this vehicle weapon
		else if (iVehicleWeaponMgrIndex != -1 && pParent->GetRestrictedAmmoCount(m_handlingIndex) == 0)
		{
			bCanFire = false;
			m_VehicleWeapons[m_iCurrentWeaponCounter]->TriggerEmptyShot(pParent);
		}

		if(bCanFire)
		{
			Assert(m_iCurrentWeaponCounter >= 0 && m_iCurrentWeaponCounter < m_iNumVehicleWeapons);
			m_VehicleWeapons[m_iCurrentWeaponCounter]->Fire(pFiringPed, pParent, vTargetPosition, pTarget, false);
			m_MissileFlapState[m_iCurrentWeaponCounter] = MFS_OPENING;
			m_iLastFiredWeapon = m_iCurrentWeaponCounter;

			if (iVehicleWeaponMgrIndex != -1 && pParent->GetRestrictedAmmoCount(m_handlingIndex) > 0)
			{
				pParent->ReduceRestrictedAmmoCount(m_handlingIndex);
			}
			IncrementCurrentWeaponCounter(*pParent);

			CCustomShaderEffectVehicle* pShaderFx = static_cast<CCustomShaderEffectVehicle*>( pParent->GetDrawHandler().GetShaderEffect() );

			if( pShaderFx )
			{
				TUNE_GROUP_FLOAT( VEHICLE_WEAPON_BATTERY, AMMO_ROTATE_RATE, 0.075f, -10.0f, 10.0f, 0.01f );

				Vector2 ammoAnim = Vector2( 0.0f, 0.0f );

				if( m_iCurrentWeaponCounter % 2 == 0 )
				{
					ammoAnim.x = AMMO_ROTATE_RATE;
				}
				else
				{
					ammoAnim.y = AMMO_ROTATE_RATE;
				}

				pShaderFx->TrackAmmoUVAnimAdd( ammoAnim );
			}
		}
	}
}

const CWeaponInfo* CVehicleWeaponBattery::GetWeaponInfo() const 
{
	if(m_iNumVehicleWeapons > 0)
	{
		return m_VehicleWeapons[0]->GetWeaponInfo();
	}

	return NULL;
}

u32 CVehicleWeaponBattery::GetHash() const
{
	if(m_iNumVehicleWeapons > 0)
	{
		return m_VehicleWeapons[0]->GetHash();
	}

	return 0;
}

void CVehicleWeaponBattery::SetOverrideWeaponDirection(const Vector3 &vOverrideDirection)
{
	for(int i = 0; i < m_iNumVehicleWeapons; ++i)
	{
		m_VehicleWeapons[i]->SetOverrideWeaponDirection(vOverrideDirection);
	}
}

void CVehicleWeaponBattery::SetOverrideLaunchSpeed(float fOverrideLaunchSpeed)
{
	for(int i = 0; i < m_iNumVehicleWeapons; ++i)
	{
		m_VehicleWeapons[i]->SetOverrideLaunchSpeed(fOverrideLaunchSpeed);
	}
}

bool CVehicleWeaponBattery::IsUnderwater(const CVehicle* pParent) const
{
	// Only one bone needs to be underwater for the whole battery to return true
	for(int i = 0; i < m_iNumVehicleWeapons; ++i)
	{
		if (m_VehicleWeapons[i]->IsUnderwater(pParent))
		{
			return true;
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
// Class CTurret
CTurret::CTurret(eHierarchyId iBaseBone, eHierarchyId iPitchBone, float fTurretSpeed, float fTurretPitchMin, float fTurretPitchMax, float fTurretCamPitchMin, float fTurretCamPitchMax , float fBulletVelocityForGravity, float fTurretPitchForwardMin, CVehicleWeaponHandlingData::sTurretPitchLimits sPitchLimits)
: m_iBaseBoneId(iBaseBone)
, m_iPitchBoneId(iPitchBone)
, m_DesiredAngleUnclamped(Quaternion::IdentityType)
, m_DesiredAngle(Quaternion::IdentityType)
, m_AimAngle(Quaternion::IdentityType)
, m_AssociatedSeatIndex(-1)
, m_VehicleTurretAudio(NULL)
, m_fMinPitch(-HALF_PI)
, m_fMaxPitch(HALF_PI)
, m_fMinHeading(-PI)
, m_fMaxHeading(PI)
, m_fSpeedModifier(-1.0f)
, m_fTurretSpeed(fTurretSpeed)
, m_fTurretPitchMin(fTurretPitchMin)
, m_fTurretPitchMax(fTurretPitchMax)
, m_fTurretCamPitchMin(fTurretCamPitchMin)
, m_fTurretCamPitchMax(fTurretCamPitchMax)
, m_fBulletVelocityForGravity(fBulletVelocityForGravity)
, m_fTurretPitchForwardMin(fTurretPitchForwardMin)
, m_fTurretSpeedOverride(0.0f)
, m_fPerFrameHeadingModifier(-1.0f)
, m_fPerFramePitchModifier(-1.0f)
, m_PitchLimits(sPitchLimits)
, m_Flags(0)
{
}

void CTurret::ProcessPreRender(CVehicle* pParent)
{
	Assert(pParent);

	if(m_iBaseBoneId > VEH_INVALID_ID)
	{
		if(m_iPitchBoneId > VEH_INVALID_ID)
		{
			static const eRotationAxis baseRotAxis =  ROT_AXIS_Z;
			static const eRotationAxis turretTiltAxis = ROT_AXIS_X;

			Vector3 vEulers;
			m_AimAngle.ToEulers(vEulers);
			pParent->SetComponentRotation(m_iBaseBoneId,baseRotAxis,vEulers.z,true);
			pParent->SetComponentRotation(m_iPitchBoneId,turretTiltAxis,vEulers.x,true);
		}
		else
		{
			// Apply all the rotation to the base bone
			pParent->SetComponentRotation(m_iBaseBoneId,m_AimAngle,true);
		}
	}
}

void CTurret::InitAudio(CVehicle* pParent)
{
	if(!m_VehicleTurretAudio)
	{
		m_VehicleTurretAudio = (audVehicleTurret*)pParent->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_TURRET);		

		Matrix34 turretWorldMatrix;
		GetTurretMatrixWorld(turretWorldMatrix, pParent);
		m_VehicleTurretAudio->SetTurretLocalPosition(VEC3V_TO_VECTOR3(pParent->GetTransform().UnTransform(VECTOR3_TO_VEC3V(turretWorldMatrix.d))));
	}
}

void CTurret::ProcessControl(CVehicle* UNUSED_PARAM(pParent))
{
	// Move to desired aim smoothly
	const float fSpeedLimit = GetTurretSpeed() * fwTimer::GetTimeStep();

	float fOrientationDelta = m_DesiredAngle.RelAngle(m_AimAngle);
	float fInterpolationFactor = Min(Abs(fSpeedLimit / fOrientationDelta), 1.0f);
	m_AimAngle.SlerpNear(fInterpolationFactor, m_DesiredAngle);

	m_fTurretSpeedOverride = 0.0f;
}

void CTurret::AimAtLocalDir(const Vector3& vLocalTargetDir, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim)
{
	TUNE_GROUP_BOOL(TURRET_TUNE, bOverrideTurretPitchMinValues, false );
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_FWD_PITCH_MIN, 0.0f, -1.5f, 1.5f, 0.01f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_PITCH_MIN, 0.0f, -1.5f, 1.5f, 0.01f);
	if (bOverrideTurretPitchMinValues)
	{
		m_fTurretPitchForwardMin = OVERRIDE_FWD_PITCH_MIN;
		m_fTurretPitchMin = OVERRIDE_PITCH_MIN;
	}

	if (m_Flags.IsFlagSet(TF_DisableMovement))
	{
		return;
	}

	float fDesiredHeading = rage::Atan2f(-vLocalTargetDir.x, vLocalTargetDir.y);		
	float fDesiredPitch =  rage::Atan2f(vLocalTargetDir.z, vLocalTargetDir.XYMag());

	//We don't want to take gravity into account if it's the player. bDoCamCheck only == true for the player. 
	if(m_fBulletVelocityForGravity > 0.0f && !bDoCamCheck)
	{
		fDesiredPitch = CalculateTrajectoryForGravity(vLocalTargetDir,m_fBulletVelocityForGravity);
	}
	
	float fDisredPitchUnclamped = fDesiredPitch;

	if(bDoCamCheck && (!IsClose(m_fTurretCamPitchMin, 0.0f) || !IsClose(m_fTurretCamPitchMax, 0.0f) || !IsClose(m_fTurretPitchMin, 0.0f) || !IsClose(m_fTurretPitchMax, 0.0f)))
	{
		float fMinPitch = m_fTurretPitchMin;
		if(!IsClose(m_fTurretPitchForwardMin, 0.0f))
		{
			const bool bIsRearMountedTurret = pParent && pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_REAR_MOUNTED_TURRET);
			float fLerp;
			float fdesiredAngMag = Abs(fDesiredHeading);
			float fForcedPitchAng = m_fTurretPitchForwardMin;
			TUNE_GROUP_BOOL(TURRET_TUNE, DISABLE_CUSTOM_LERP_VALUES, false);
			TUNE_GROUP_BOOL(TURRET_TUNE, OVERRIDE_FORCE_USE_CUSTOM_LERP_VALUES, false);
			if (!DISABLE_CUSTOM_LERP_VALUES && (bIsRearMountedTurret || OVERRIDE_FORCE_USE_CUSTOM_LERP_VALUES))
			{
				fLerp = GetLerpValueForRearMountedTurret(fdesiredAngMag, fForcedPitchAng);
			}
			else
			{   
				// 0 - m_fTurretCamPitchMin, 1 - m_fTurretPitchForwardMin  
				fLerp = Abs(fdesiredAngMag - PI) / PI;
			}
			fMinPitch = Lerp(fLerp,m_fTurretPitchMin,fForcedPitchAng);
		}
	
		fDesiredPitch = rage::Clamp( fDesiredPitch, fMinPitch, m_fTurretPitchMax );
	}
	else
	{
		fDesiredPitch = rage::Clamp(fDesiredPitch, m_fMinPitch, m_fMaxPitch);

		bool bIsScriptControlledTurret = (MI_TRAILER_TRAILERLARGE.IsValid() && pParent->GetModelIndex() == MI_TRAILER_TRAILERLARGE) ||
										 (MI_PLANE_AVENGER.IsValid() && pParent->GetModelIndex() == MI_PLANE_AVENGER) ||
										 (MI_CAR_TERBYTE.IsValid() && pParent->GetModelIndex() == MI_CAR_TERBYTE);

		//Normalize fDesiredHeading into joint limit's space. B* 439628
		if (m_fMaxHeading > PI && fDesiredHeading < m_fMinHeading)
			fDesiredHeading += 2.0f*PI;
		else if (m_fMinHeading < ( bIsScriptControlledTurret ? -PI : PI ) && fDesiredHeading > m_fMaxHeading)
			fDesiredHeading -= 2.0f*PI;

		fDesiredHeading = rage::Clamp(fDesiredHeading, m_fMinHeading, m_fMaxHeading);
	}

	fDesiredPitch = rage::Clamp( fDesiredPitch, m_fMinPitch, m_fMaxPitch );

	if(vehicleVerifyf(IsFiniteAll(Vec3V(fDesiredPitch, 0.f, fDesiredHeading)), 
		"Turret desired angle is not finite! Local target vector: <%f,%f,%f>, Aim Eulers: <%f,%f,%f>", 
		vLocalTargetDir.x, vLocalTargetDir.y, vLocalTargetDir.z, fDesiredPitch, 0.0f, fDesiredHeading ) )
	{
		m_DesiredAngleUnclamped.FromEulers(Vector3(fDisredPitchUnclamped, 0.f, fDesiredHeading));

		Quaternion desiredAngle;
		desiredAngle.FromEulers(Vector3(fDesiredPitch, 0.f, fDesiredHeading));
		SetDesiredAngle(desiredAngle,pParent);
		if(bSnapAim)
		{
			m_AimAngle = m_DesiredAngle;
		}
	}
}

float CTurret::GetLerpValueForRearMountedTurret(float fdesiredAngMag, float& fForcedPitchAng)
{
	TUNE_GROUP_BOOL(TURRET_TUNE, bOverrideTurretPitchValues, false );
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_FWD_ANG_MIN, 20.0f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_FWD_ANG_MAX, 80.0f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_BWD_ANG_MIN, 125.0f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_BWD_ANG_MID, 139.0f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_BWD_ANG_MAX, 169.0f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(TURRET_TUNE, OVERRIDE_BWD_FORCED_MIN_PITCH, -0.597f, -1.5f, 1.5f, 0.01f);

	float FWD_ANG_MIN = (bOverrideTurretPitchValues ? OVERRIDE_FWD_ANG_MIN : m_PitchLimits.m_fForwardAngleMin ) * DtoR; // Angle (away from veh forward) at which to start lowering the turret
	float FWD_ANG_MAX = (bOverrideTurretPitchValues ? OVERRIDE_FWD_ANG_MAX : m_PitchLimits.m_fForwardAngleMax ) * DtoR; // Angle (away from veh forward) at which to finish lowering the turret
	float BWD_ANG_MIN = (bOverrideTurretPitchValues ? OVERRIDE_BWD_ANG_MIN : m_PitchLimits.m_fBackwardAngleMin) * DtoR; // Angle (away from veh forward) where we start raising turret to avoid corners
	float BWD_ANG_MID = (bOverrideTurretPitchValues ? OVERRIDE_BWD_ANG_MID : m_PitchLimits.m_fBackwardAngleMid) * DtoR; // Angle (away from veh forward) where we finish raising turret to avoid corners
	float BWD_ANG_MAX = (bOverrideTurretPitchValues ? OVERRIDE_BWD_ANG_MAX : m_PitchLimits.m_fBackwardAngleMax) * DtoR; // Angle (away from veh forward) where we finish lowering turret down again to avoid the corner
	float BWD_FORCED_MIN_PITCH = bOverrideTurretPitchValues ? OVERRIDE_BWD_FORCED_MIN_PITCH : m_PitchLimits.m_fBackwardForcedPitch;

	if (fdesiredAngMag < FWD_ANG_MIN) // Facing forwards below min angle - not allowed to start lowering the turret yet
	{
		return 1.0f;
	}
	else if (fdesiredAngMag < FWD_ANG_MAX) // Between forward min and max - can start lowering the turret
	{
		return Abs(((fdesiredAngMag - FWD_ANG_MIN) - (FWD_ANG_MAX - FWD_ANG_MIN)) / (0.0f - (FWD_ANG_MAX - FWD_ANG_MIN)));
	}
	else // Facing backwards
	{
		fForcedPitchAng = BWD_FORCED_MIN_PITCH;
		if (fdesiredAngMag < BWD_ANG_MIN || fdesiredAngMag > BWD_ANG_MAX) // Below min or above max - no pitch adjustments
		{
			return 0.0f;
		}
		else if (fdesiredAngMag < BWD_ANG_MID) // Between min and mid - start raising the turret to avoid back corners
		{
			return Abs((fdesiredAngMag - BWD_ANG_MIN) / (BWD_ANG_MID - BWD_ANG_MIN));
		}
		else // Between mid and max - start lowering the turret again
		{
			return 1.0f - Abs((fdesiredAngMag - BWD_ANG_MID) / (BWD_ANG_MAX - BWD_ANG_MID));
		}
	}
}

float CTurret::GetCurrentHeading(const CVehicle* pParent) const
{ 
	Matrix34 mtx;
	GetTurretMatrixWorld(mtx, pParent);
	Mat34V mtxv = MATRIX34_TO_MAT34V(mtx);
	return Atan2f(mtxv.GetM10f(), mtxv.GetM11f()); 
}

float CTurret::GetLocalHeading(const CVehicle& rParent) const
{
	const float fVehHeading = rParent.GetTransform().GetHeading();
	const float fTurretHeading = GetCurrentHeading(&rParent);
	const float fDelta = fwAngle::LimitRadianAngle(fVehHeading - fTurretHeading);
	return fDelta;
}

bool CTurret::IsWithinFiringAngularThreshold( float fAngularThresholdInRads ) const
{
	fAngularThresholdInRads = fwAngle::LimitRadianAngleSafe( fAngularThresholdInRads );
	float fAngle = m_DesiredAngleUnclamped.RelAngle(m_AimAngle);
	if(fAngle < fAngularThresholdInRads)
		return true;

	return false;
}

void CTurret::SetDesiredAngle(const Quaternion& desiredAngle, CVehicle* pParent)
{
	//If the desired angle == the current aim angle then allow the vehicle to sleep. Otherwise keep processing the turret. 
	pParent->m_nVehicleFlags.bWakeUpNextUpdate |= (IsCloseAll(RCC_QUATV(desiredAngle),RCC_QUATV(m_AimAngle),ScalarV(V_FLT_SMALL_3)) == 0);
	m_DesiredAngle = desiredAngle;
}

float CTurret::GetTurretSpeed() const 
{ 
	float fTurretSpeed = (m_fTurretSpeedOverride == 0.0f ? m_fTurretSpeed : m_fTurretSpeedOverride);
	const float fTurretSpeedModifier = GetTurretSpeedModifier();
	fTurretSpeed = fTurretSpeedModifier * m_fTurretSpeed;
	return fTurretSpeed; 
}

void CTurret::ComputeTurretSpeedModifier(const CVehicle* pParent)
{
	float fDesiredTurretSpeedModifier = 1.0f;
	const CPed* pSeatOccupier = pParent->IsSeatIndexValid(m_AssociatedSeatIndex) ? pParent->GetSeatManager()->GetPedInSeat(m_AssociatedSeatIndex) : pParent->GetDriver();
	const bool bSeatOccupierBeingJacked = pSeatOccupier && pSeatOccupier->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle) && pSeatOccupier->GetCarJacker();

	if (bSeatOccupierBeingJacked)
	{
		TUNE_GROUP_FLOAT(TURRET_TUNE, JACKED_TURRET_SPEED_MODIFIER, 0.3f, 0.0f, 1.0f, 0.01f);
		fDesiredTurretSpeedModifier = JACKED_TURRET_SPEED_MODIFIER;
	}
	else if (m_fTurretSpeedOverride == 0.0f && pSeatOccupier && pSeatOccupier->IsControlledByLocalOrNetworkAi())
	{
		//AI uses 10% of the handling.dat turret speed defined for the player
		TUNE_GROUP_FLOAT(TURRET_TUNE, TURRET_SPEED_MODIFIER, 0.1f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, HELD_TURRET_SPEED_MODIFIER, 0.5f, 0.0f, 1.0f, 0.01f);
		fDesiredTurretSpeedModifier = pParent->IsTurretSeat(m_AssociatedSeatIndex) ? HELD_TURRET_SPEED_MODIFIER : TURRET_SPEED_MODIFIER;
	}

	TUNE_GROUP_FLOAT(TURRET_TUNE, TURRET_SPEED_APPROACH_RATE, 0.75f, 0.0f, 10.0f, 0.01f);
	if (m_fSpeedModifier < 0.0f)
	{
		m_fSpeedModifier = fDesiredTurretSpeedModifier;
	}
	else
	{
		Approach(m_fSpeedModifier, fDesiredTurretSpeedModifier, TURRET_SPEED_APPROACH_RATE, fwTimer::GetTimeStep());
	}
}

void CTurret::AimAtWorldPos(const Vector3& vDesiredTarget, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim)
{	
	// Need to go from world space to the (non rotated) space of turret bone matrix
	Vector3 vLocalDir = ComputeLocalDirFromWorldPos(vDesiredTarget, pParent);

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, DEBUG_TARGET_LOS, false);
	if (DEBUG_TARGET_LOS)
	{
		Matrix34 matRender;
		pParent->GetGlobalMtx(pParent->GetBoneIndex(m_iPitchBoneId > VEH_INVALID_ID ? m_iPitchBoneId : m_iBaseBoneId), matRender);

		grcDebugDraw::Line( matRender.d, vDesiredTarget, Color_orange);
		Vector3 vForward = matRender.b;
		const float projectDist = 40.0f;
		Vector3 vDestinationPosition = matRender.d + (vForward * projectDist);
		grcDebugDraw::Line( matRender.d, vDestinationPosition, Color_green);

		Matrix34 matTurretWorldTest;
		GetTurretMatrixWorld(matTurretWorldTest,pParent);
		static dev_float fScale = 1.0f;
		grcDebugDraw::Axis(matTurretWorldTest,fScale,true);
	}
#endif // __BANK


	if(vLocalDir.IsNonZero())
	{
		AimAtLocalDir(vLocalDir, pParent, bDoCamCheck, bSnapAim);
	}

	if(m_VehicleTurretAudio)
	{		
		bool turretAudioAllowed = true;

		// Suppress audio on barrage rear turret
		if (pParent->GetModelNameHash() == MI_CAR_BARRAGE.GetName().GetHash() && m_iBaseBoneId != VEH_TURRET_1_BASE)
		{
			turretAudioAllowed = false;
		}

		// Suppress audio on patrolboat rear turret
		if (pParent->GetModelNameHash() == MI_BOAT_PATROLBOAT.GetName().GetHash() && m_iBaseBoneId == VEH_TURRET_1_BASE)
		{
			turretAudioAllowed = false;
		}

		if(turretAudioAllowed)	
		{
			m_VehicleTurretAudio->SetAimAngle(m_AimAngle);
		}
	}

	// Hack to make the turret non-wobbly. SetPedHasControl is set from TaskVehicleWeapon, but the trailerlarge does not run this task.
	bool bIsScriptControlledTurret = (MI_TRAILER_TRAILERLARGE.IsValid() && pParent->GetModelIndex() == MI_TRAILER_TRAILERLARGE) ||
									 (MI_PLANE_AVENGER.IsValid() && pParent->GetModelIndex() == MI_PLANE_AVENGER) || 
									 (MI_CAR_TERBYTE.IsValid() && pParent->GetModelIndex() == MI_CAR_TERBYTE);
	if(bIsScriptControlledTurret && IsPhysicalTurret())
	{
		CTurretPhysical* pPhysicalTurret = static_cast<CTurretPhysical*>(this);
		pPhysicalTurret->SetPedHasControl(true);
	}
}

Vector3 CTurret::ComputeLocalDirFromWorldPos(const Vector3& vDesiredTarget, const CVehicle* pParent) const
{
	const float fRelativeHeadingDelta = ComputeRelativeHeadingDelta(pParent);

	Matrix34 matTurret;
	matTurret.Identity();
	matTurret.RotateLocalZ(fRelativeHeadingDelta);

	Matrix34 parentMtx = MAT34V_TO_MATRIX34(pParent->GetMatrix());
	Vector3 vLocalDir;

	// Don't call GetTurretMatrixWorld here because that will give the matrix of the turret after it has been rotated
	if( aiVerifyf(m_iBaseBoneId > VEH_INVALID_ID,"No valid vehicle component ids!") )
	{
		pParent->GetDefaultBonePositionForSetup(m_iBaseBoneId,matTurret.d);		
		matTurret.Dot(parentMtx);
		matTurret.UnTransform(vDesiredTarget,vLocalDir);

#if __BANK
		TUNE_GROUP_BOOL(TURRET_DEBUG, RENDER_MAT_TURRET, false);
		if (RENDER_MAT_TURRET)
		{
			grcDebugDraw::Axis(matTurret, 1.0f, true);
		}
#endif // __BANK
	}

	if(m_iPitchBoneId > VEH_INVALID_ID)
	{
		Vector3 vLocalDirPitch;
		matTurret.Identity();
		matTurret.RotateLocalZ(fRelativeHeadingDelta);	
		pParent->GetDefaultBonePositionForSetup(m_iPitchBoneId,matTurret.d); 
		matTurret.Dot(parentMtx);
		matTurret.UnTransform(vDesiredTarget,vLocalDirPitch);
		vLocalDir.z = vLocalDirPitch.z;
	}

	return vLocalDir;
}

Vector3 CTurret::ComputeWorldPosFromLocalDir(const Vector3& vDesiredTarget, const CVehicle* pParent) const
{
	Matrix34 matTurret;
	matTurret.Identity();

	const float fRelativeHeadingDelta = ComputeRelativeHeadingDelta(pParent);
	matTurret.RotateLocalZ(fRelativeHeadingDelta);

	// Don't call GetTurretMatrixWorld here because that will give the matrix of the turret after it has been rotated
	if(aiVerifyf(m_iBaseBoneId > VEH_INVALID_ID,"No valid vehicle component ids!") )
	{
		pParent->GetDefaultBonePositionForSetup(m_iBaseBoneId,matTurret.d);
	}

	if(aiVerifyf(m_iPitchBoneId > VEH_INVALID_ID,"No valid vehicle component ids!") )
	{
		pParent->GetDefaultBonePositionForSetup(m_iPitchBoneId,matTurret.d);
	}

	Matrix34 parentMtx = MAT34V_TO_MATRIX34(pParent->GetMatrix());
	matTurret.Dot(parentMtx);
	Vector3 vTarget = vDesiredTarget;
	matTurret.Transform(vTarget);
	return vTarget;
}

float CTurret::ComputeRelativeHeadingDelta(const CVehicle* pParent) const
{
	const bool bUseRelativeHeadingDelta = pParent->GetVehicleModelInfo() && pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_TURRET_RELATIVE_AIM_CALCULATION);
	if (!bUseRelativeHeadingDelta)
		return 0.0f;

	Quaternion qDefaultGlobalVehicle;
	pParent->GetDefaultBoneRotationForSetup(VEH_CHASSIS, qDefaultGlobalVehicle);
	qDefaultGlobalVehicle.Inverse();
	Quaternion qDefaultGlobalTurretRotation;
	pParent->GetDefaultBoneRotationForSetup(m_iBaseBoneId, qDefaultGlobalTurretRotation);
	qDefaultGlobalTurretRotation.Multiply(qDefaultGlobalVehicle);
	Vector3 vEulers;
	qDefaultGlobalTurretRotation.ToEulers(vEulers);
	return vEulers.z;
}

float CTurret::CalculateTrajectoryForGravity( const Vector3& vLocalDir, float projVelocity )
{
	float gravity = -GRAVITY;
	float targetX = vLocalDir.XYMag();
	float targetY = vLocalDir.z;
	float projVelocitySq = rage::square(projVelocity);
	float sqrtTerm = (rage::square(projVelocitySq)) - gravity*(gravity*(rage::square(targetX)) + ((2*targetY)*projVelocitySq));
	if ( sqrtTerm > 0.0f)
	{
		float r1 = sqrt(sqrtTerm);
		float a1 = projVelocitySq + r1;
		float a2 = projVelocitySq - r1;
		float gravityTargetX = gravity*targetX;

		a1 = rage::Atan2f(a1, gravityTargetX);
		a2 = rage::Atan2f(a2, gravityTargetX);

		if ( a1 > m_fTurretPitchMax || a1 < m_fTurretPitchMin )
		{
			if ( a2 > m_fTurretPitchMax || a2 < m_fTurretPitchMin )
			{
				return -100.0f;
			}
			
			return a2;
		}
		else
		{
			if ( a2 > m_fTurretPitchMax || a2 < m_fTurretPitchMin )
			{
				return a1;
			}
			else
			{
				if (a2 < a1)
				{
					return a2;
				}
				else
				{
					return a1;
				}
			}
		}
	}
	return -100.0f;
}

void CTurret::GetTurretMatrixWorld(Matrix34& matOut, const CVehicle* pParent) const
{
	if(aiVerifyf(pParent,"NULL parent vehicle!"))
	{
		matOut.Identity();

		if(m_iPitchBoneId > VEH_INVALID_ID)
		{
			pParent->GetDefaultBonePositionForSetup(m_iPitchBoneId,matOut.d);						
		}

		Vector3 vBaseOffset = VEC3_ZERO;
		if (m_iBaseBoneId > VEH_INVALID_ID)
		{			
			pParent->GetDefaultBonePositionForSetup(m_iBaseBoneId,vBaseOffset);
			Quaternion qDefaultBaseOrientation;
			pParent->GetDefaultBoneRotationForSetup(m_iBaseBoneId,qDefaultBaseOrientation);

			// Mat out is now in base's space
			// So origin is at base
			matOut.d -= vBaseOffset;
			matOut.FromQuaternion(qDefaultBaseOrientation);
		}

		const float fHeading = m_AimAngle.TwistAngle(ZAXIS);
		Matrix34 matAim;
		if (pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_HEADING_ONLY_IN_TURRET_MATRIX))
		{
			matAim.FromEulersXYZ(Vector3(0.0f,0.0f,fHeading));
		}
		else
		{
			matAim.FromQuaternion(m_AimAngle);
		}
		matAim.d.Zero();
		matOut.Dot(matAim);

		matOut.d += vBaseOffset;

		const Matrix34& parentMat = RCC_MATRIX34(pParent->GetMatrixRef());
		matOut.Dot(parentMat);		
	}
}

void CTurret::InitJointLimits(const CVehicleModelInfo* pVehicleModelInfo)
{
	Assert(pVehicleModelInfo);
	CVehicleStructure* pStructure = pVehicleModelInfo->GetStructure();
	
	if(pStructure == NULL)
	{
		return;
	}

	const s32 iBaseBoneIndex = m_iBaseBoneId > VEH_INVALID_ID ? pStructure->m_nBoneIndices[m_iBaseBoneId] : -1;
	const s32 iBarrelBoneIndex = m_iPitchBoneId > VEH_INVALID_ID ? pStructure->m_nBoneIndices[m_iPitchBoneId] : -1;

	if(pVehicleModelInfo->GetDrawable() 
		&& pVehicleModelInfo->GetDrawable()->GetSkeletonData())
	{
		crBoneData* pBoneData = NULL;
		if(iBaseBoneIndex > -1)
		{
			pBoneData = pVehicleModelInfo->GetDrawable()->GetSkeletonData()->GetBoneData(iBaseBoneIndex);
			if(pBoneData && pBoneData->GetDofs() & crBoneData::HAS_ROTATE_LIMITS )
			{
				const crJointData* pJointData = pVehicleModelInfo->GetDrawable()->GetJointData();
				Vec3V minRot, maxRot;
				if (pJointData && pJointData->ConvertToEulers(*pBoneData, minRot, maxRot)) {
					bool bIsBarrage = MI_CAR_BARRAGE.IsValid() && pVehicleModelInfo->GetModelNameHash() == MI_CAR_BARRAGE.GetName().GetHash();
					if (bIsBarrage && m_iBaseBoneId == VEH_TURRET_2_BASE)
					{
						minRot = Vec3V(0.f, 0.f, -1.1f);
						maxRot = Vec3V(0.f, 0.f, 1.1f);
					}
						SetMinHeading(minRot.GetZf());
						SetMaxHeading(maxRot.GetZf());
				} else
				{
					SetMinHeading(-PI);
					SetMaxHeading(PI);
				}
			}

			if(pVehicleModelInfo->GetFragType())
			{
				s32 nGroupIndex = pVehicleModelInfo->GetFragType()->GetGroupFromBoneIndex(0, iBaseBoneIndex);
				if(nGroupIndex > -1)
				{
					fragTypeGroup* pGroup = pVehicleModelInfo->GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];
					if(pGroup)
					{
						pGroup->SetForceTransmissionScaleUp(0.0f);
						pGroup->SetForceTransmissionScaleDown(0.0f);
					}
				}
			}
		}

		// If we have a barrel set up the pitch limits
		if(iBarrelBoneIndex > -1)
		{
			pBoneData = pVehicleModelInfo->GetDrawable()->GetSkeletonData()->GetBoneData(iBarrelBoneIndex);
			if(pBoneData && pBoneData->GetDofs() & crBoneData::HAS_ROTATE_LIMITS )
			{
				const crJointData* pJointData = pVehicleModelInfo->GetDrawable()->GetJointData();
				Assert(pJointData);
				Vec3V minRot, maxRot;
				if (pJointData->ConvertToEulers(*pBoneData, minRot, maxRot))
				{
					SetMinPitch(minRot.GetXf() + phSimulator::GetAllowedAnglePenetration() );
					SetMaxPitch(maxRot.GetXf() - phSimulator::GetAllowedAnglePenetration() );
				}
				else
				{
					SetMinPitch(-PI);
					SetMaxPitch(PI);
				}
			}

			if(pVehicleModelInfo->GetFragType())
			{
				s32 nGroupIndex = pVehicleModelInfo->GetFragType()->GetGroupFromBoneIndex(0, iBarrelBoneIndex);
				if(nGroupIndex > -1)
				{
					fragTypeGroup* pGroup = pVehicleModelInfo->GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];
					if(pGroup)
					{
						pGroup->SetForceTransmissionScaleUp(0.0f);
						pGroup->SetForceTransmissionScaleDown(0.0f);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Class CTurretPhysical

CTurretPhysical::CTurretPhysical(eHierarchyId iBaseBone, eHierarchyId iPitchBone, float fTurretSpeed, float fTurretPitchMin, float fTurretPitchMax, float fTurretCamPitchMin, float fTurretCamPitchMax, float fBulletVelocityForGravity, float fTurretPitchForwardMin, CVehicleWeaponHandlingData::sTurretPitchLimits sPitchLimits)
: CTurret(iBaseBone,iPitchBone,fTurretSpeed,fTurretPitchMin,fTurretPitchMax,fTurretCamPitchMin,fTurretCamPitchMax,fBulletVelocityForGravity,fTurretPitchForwardMin,sPitchLimits)
{
	m_iBaseFragChild	= -1;
	m_iBarrelFragChild	= -1;
	m_fTimeSinceTriggerDesiredAngleBlendInOut = 0.0f;
	m_bBarrelInCollision		= false;
	m_bHasReachedDesiredAngle	= false;
	m_bPedHasControlOfTurret	= false;
	m_RenderAngleX = 0.0f;
	m_RenderAngleZ = 0.0f;

	m_bResetTurretHeading = false;
    m_bHidden = false;
}

static dev_float sfRhinoInitialTurretElevation = 0.1f;

//////////////////////////////////////////////////////////////////////////
void CTurretPhysical::InitJointLimits(const CVehicleModelInfo* pModelInfo)
{
	CTurret::InitJointLimits(pModelInfo);

	if( pModelInfo->GetModelNameHash() == MI_TANK_RHINO.GetName().GetHash() )
	{
		m_DesiredAngle.FromEulers( Vector3( sfRhinoInitialTurretElevation, 0.0f, 0.0f ) );
	}

	if(pModelInfo->GetFragType())
	{
		Assertf(pModelInfo->GetStructure(),"All vehicle model infos should have a structure but this is NULL");

		CVehicleStructure* pStructure = pModelInfo->GetStructure();

		if(pStructure == NULL)
		{
			return;
		}

		const s32 iBaseBoneIndex = m_iBaseBoneId > VEH_INVALID_ID ? pStructure->m_nBoneIndices[m_iBaseBoneId] : -1;
		const s32 iBarrelBoneIndex = m_iPitchBoneId > VEH_INVALID_ID ? pStructure->m_nBoneIndices[m_iPitchBoneId] : -1;

		if(m_iBaseBoneId > VEH_INVALID_ID)
		{
			m_iBaseFragChild = (s8)pModelInfo->GetFragType()->GetComponentFromBoneIndex(0, iBaseBoneIndex);
		}

		if(m_iPitchBoneId > VEH_INVALID_ID)
		{
			m_iBarrelFragChild = (s8)pModelInfo->GetFragType()->GetComponentFromBoneIndex(0, iBarrelBoneIndex);
		}
	}

	m_bHeldTurret			= pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);
	m_bResetTurretHeading	= pModelInfo->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_RESET_TURRET_SEAT_HEADING );
}

//////////////////////////////////////////////////////////////////////////
void CTurretPhysical::SetEnabled(CVehicle* pParent, bool bEnabled )
{ 
    CVehicleGadget::SetEnabled(pParent, bEnabled );

    if(!bEnabled)
    {
        m_fTurretSpeed = 0.0f; //Reduce turret speed to 0 as we are disabled

        DriveTurret(pParent);
    }
}

void CTurretPhysical::ResetTurretsMuscles(CVehicle* pParent)
{
	fragInst* pFragInst = pParent->GetVehicleFragInst();
	// Make sure we have reset the turrets muscles so they dont keep rotating.
	if(pFragInst)
	{
		if(fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry())
		{
			Vector3 vAimEulers;
			m_AimAngle.ToEulers(vAimEulers);
			static dev_float sfTurretBlownUpMuscleStiffness = 0.10f;
			static dev_float sfTurretBlownUpMuscleAngleStrength = 10.0f;
			static dev_float sfTurretBlownUpMuscleSpeedStrength = 40.0f;
			static dev_float sfTurretBlownUpMinAndMaxMuscleTorque = 100.0f;
			static dev_float sfTurretBlownUpSpeedStrengthFadeOut = 0.5f;
			float tempAngle;
			if(m_iBaseFragChild > -1)
			{
				DriveComponentToAngle(pCacheEntry->GetHierInst(),m_iBaseFragChild,vAimEulers.z,0,tempAngle,
					sfTurretBlownUpMuscleStiffness,
					sfTurretBlownUpMuscleAngleStrength,
					sfTurretBlownUpMuscleSpeedStrength,
					sfTurretBlownUpMinAndMaxMuscleTorque,
					false,
					sfTurretBlownUpSpeedStrengthFadeOut);
			}
			if(m_iBarrelFragChild > -1)
			{
				DriveComponentToAngle(pCacheEntry->GetHierInst(),m_iBarrelFragChild,vAimEulers.x,0,tempAngle,
					sfTurretBlownUpMuscleStiffness,
					sfTurretBlownUpMuscleAngleStrength,
					sfTurretBlownUpMuscleSpeedStrength,
					sfTurretBlownUpMinAndMaxMuscleTorque,
					false,
					sfTurretBlownUpSpeedStrengthFadeOut);
			}
		}
	}
	
}

//////////////////////////////////////////////////////////////////////////
dev_float sfBlowUpTurretProb = 0.8f;
void CTurretPhysical::BlowUpCarParts(CVehicle* pParent)
{
	fragInst* pFragInst = pParent->GetVehicleFragInst();

	ResetTurretsMuscles(pParent);

	bool bDeleteParts = !pParent->CarPartsCanBreakOff();

	float fxProb = 0.0f;
	if (pParent->GetVehicleType()==VEHICLE_TYPE_HELI)
		fxProb = 0.75f;
	else if (pParent->GetVehicleType()==VEHICLE_TYPE_CAR)
		fxProb = 0.25f;

    if( pParent->GetModelIndex() == MI_CAR_MINITANK )
    {
        return;
    }

	const CVehicleExplosionInfo* vehicleExplosionInfo = pParent->GetVehicleModelInfo()->GetVehicleExplosionInfo();

	if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sfBlowUpTurretProb*CVehicleDamage::GetVehicleExplosionBreakChanceMultiplier() &&
		m_iBarrelFragChild != -1 )
	{
		pParent->GetVehicleDamage()->BreakOffPart(m_iPitchBoneId, m_iBarrelFragChild, pFragInst, vehicleExplosionInfo, bDeleteParts, 0.0f, fxProb, false);
	}
}

void CTurretPhysical::ProcessPhysics(CVehicle* pParent, const float UNUSED_PARAM(fTimestep), const int nTimeSlice)
{
#if __BANK
	TUNE_GROUP_BOOL(TURRET_HACKS, RENDER_TURRET_DEBUG, false);
	if(RENDER_TURRET_DEBUG)
	{
		Matrix34 boneMatrix;
		pParent->GetGlobalMtx(pParent->GetBoneIndex(m_iBaseBoneId), boneMatrix);
		Vector3 desiredEulers(0.0f, 0.0f, 0.0f);
		m_DesiredAngle.ToEulers(desiredEulers, eEulerOrderXYZ);
		char zText[100];
		sprintf(zText, "Pitch Min %2.2f Max %2.2f\nHeading Min %2.2f Max %2.2f\nDesired eulers: %2.2f, %2.2f, %2.2f", m_fMinPitch, m_fMaxPitch, m_fMinHeading, m_fMaxHeading, desiredEulers.x, desiredEulers.y, desiredEulers.z);
		grcDebugDraw::Text(VECTOR3_TO_VEC3V(boneMatrix.d) + Vec3V(0.0f, 0.0f, 2.0f), Color_white, zText);
	}
#endif

#if __DEV
	static dev_bool bDisablePhysicalTurrets = false;
	if(bDisablePhysicalTurrets)
	{
		return;
	}
#endif

	// Drive the turret joints to their desired angle
    DriveTurret(pParent, nTimeSlice);
}


void CTurretPhysical::ProcessPreRender(CVehicle* pParent)
{
	Assert(pParent);
	TUNE_GROUP_BOOL(TURRET_TUNE, ALLOW_TURRET_RENDER_OVERRIDE, true);
	if (!ALLOW_TURRET_RENDER_OVERRIDE)
		return;

	if (m_iBaseBoneId <= VEH_INVALID_ID)
		return;

	if (m_iPitchBoneId <= VEH_INVALID_ID)
		return;

	if (m_Flags.IsFlagSet(TF_DisableMovement))
	{
		m_Flags.ClearFlag(TF_DisableMovement);
		return;
	}

	if (m_Flags.IsFlagSet(TF_InitialAdjust))
	{
		m_Flags.ClearFlag(TF_InitialAdjust);
		return;
	}

    int nBoneIndex = pParent->GetBoneIndex( (eHierarchyId)m_iBaseBoneId );
    if( nBoneIndex > -1 )
    {
        if( pParent->GetSkeleton() )
        {
            if( m_Flags.IsFlagSet( CTurret::TF_Hide ) )
            {
                m_bHidden = true;
                pParent->GetLocalMtxNonConst( nBoneIndex ).MakeScale( 0.0f );

                if( pParent->GetVehicleFragInst() )
                {
                    pParent->Freeze1DofJointInCurrentPosition( pParent->GetVehicleFragInst(), m_iBaseFragChild, true );
                    pParent->Freeze1DofJointInCurrentPosition( pParent->GetVehicleFragInst(), m_iBarrelFragChild, true );
                }
                return;
            }
            else if( m_bHidden )
            {
                pParent->GetLocalMtxNonConst( nBoneIndex ).MakeScale( 1.0f );

                if( pParent->GetVehicleFragInst() )
                {
                    pParent->Freeze1DofJointInCurrentPosition( pParent->GetVehicleFragInst(), m_iBaseFragChild, false );
                    pParent->Freeze1DofJointInCurrentPosition( pParent->GetVehicleFragInst(), m_iBarrelFragChild, false );
                }
                m_bHidden = false;
            }
        }
    }
	
	bool bKeepAllTurretsSynchronised = false;

    const bool bShouldBlendToDesiredAngle = m_Flags.IsFlagSet(TF_RenderDesiredAngleForPhysicalTurret);
	const bool bHaveStartedToBlendDesiredAngle = m_Flags.IsFlagSet(TF_HaveUsedDesiredAngleForRendering);

	if (!bShouldBlendToDesiredAngle && !bHaveStartedToBlendDesiredAngle )
	{
		if( pParent->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_KEEP_ALL_TURRETS_SYNCHRONISED ) &&
			pParent->GetVehicleWeaponMgr() )
		{
			int seatIndex = pParent->GetVehicleWeaponMgr()->GetSeatIndexForTurret( *this );

			if( pParent->IsSeatIndexValid( seatIndex ) )
			{
				CPed* pedInSeat = pParent->GetPedInSeat( seatIndex );
				bKeepAllTurretsSynchronised = pedInSeat ? pedInSeat->IsNetworkClone() : false;
			}
		}

		if( !bKeepAllTurretsSynchronised )
		{
			m_bBarrelInCollision = false;
			return;
		}
	}

	Vector3 vDesiredEulers;
	m_DesiredAngle.ToEulers(vDesiredEulers);

	Vector3 vAimEulers;
	m_AimAngle.ToEulers(vAimEulers);

	if (!bHaveStartedToBlendDesiredAngle)
	{
		m_RenderAngleX = vAimEulers.x;
		m_RenderAngleZ = vAimEulers.z;
	}
	
	if (bShouldBlendToDesiredAngle && !m_bBarrelInCollision )
	{
		if (m_fTimeSinceTriggerDesiredAngleBlendInOut < 0.0f)
		{
			m_fTimeSinceTriggerDesiredAngleBlendInOut = 0.0f;
		}
		m_fTimeSinceTriggerDesiredAngleBlendInOut += fwTimer::GetTimeStep();

		TUNE_GROUP_FLOAT(TURRET_TUNE, HEADING_BLEND_IN_DURATION, 0.25f, 0.0f, 1.0f, 0.01f);
		const float fHeadingLerpPercentage = rage::Clamp(m_fTimeSinceTriggerDesiredAngleBlendInOut / HEADING_BLEND_IN_DURATION, 0.0f, 1.0f);

		m_RenderAngleZ = fwAngle::LerpTowards(m_RenderAngleZ, vDesiredEulers.z, fHeadingLerpPercentage);
		TUNE_GROUP_FLOAT(TURRET_TUNE, PITCH_BLEND_IN_DURATION, 0.5f, 0.0f, 1.0f, 0.01f);
		const float fPitchLerpPercentage = rage::Clamp(m_fTimeSinceTriggerDesiredAngleBlendInOut / PITCH_BLEND_IN_DURATION, 0.0f, 1.0f);
		m_RenderAngleX = fwAngle::LerpTowards(m_RenderAngleX, vDesiredEulers.x, fPitchLerpPercentage);		
	}
	else if( bShouldBlendToDesiredAngle || bHaveStartedToBlendDesiredAngle )
	{
		if (m_fTimeSinceTriggerDesiredAngleBlendInOut > 0.0f)
		{
			m_fTimeSinceTriggerDesiredAngleBlendInOut = 0.0f;
		}
		m_fTimeSinceTriggerDesiredAngleBlendInOut -= fwTimer::GetTimeStep();

		TUNE_GROUP_FLOAT(TURRET_TUNE, HEADING_BLEND_OUT_DURATION, 0.25f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(TURRET_TUNE, PITCH_BLEND_OUT_DURATION, 0.5f, 0.0f, 1.0f, 0.01f);
		const float fHeadingLerpPercentage = rage::Clamp(Abs(m_fTimeSinceTriggerDesiredAngleBlendInOut) / HEADING_BLEND_OUT_DURATION, 0.0f, 1.0f);
		const float fPitchLerpPercentage = rage::Clamp(Abs(m_fTimeSinceTriggerDesiredAngleBlendInOut)  / PITCH_BLEND_OUT_DURATION, 0.0f, 1.0f);
		m_RenderAngleX = fwAngle::LerpTowards(m_RenderAngleX, vAimEulers.x, fPitchLerpPercentage);
		m_RenderAngleZ = fwAngle::LerpTowards(m_RenderAngleZ, vAimEulers.z, fHeadingLerpPercentage);

		if (fHeadingLerpPercentage >= 1.0f && fPitchLerpPercentage >= 1.0f)
		{
			m_Flags.ClearFlag(TF_HaveUsedDesiredAngleForRendering);
			m_fTimeSinceTriggerDesiredAngleBlendInOut = 0.0f;

			if( !bKeepAllTurretsSynchronised )
			{
				return;
			}
		}
	}
	else
	{
		m_RenderAngleZ = vAimEulers.z;
		m_RenderAngleX = vAimEulers.x;
	}

	static const eRotationAxis baseRotAxis =  ROT_AXIS_Z;
	static const eRotationAxis turretTiltAxis = ROT_AXIS_X;

	if( m_iBarrelFragChild != -1 )
	{
		m_RenderAngleX = ClampAngleToJointLimits(m_RenderAngleX, *pParent, m_iBarrelFragChild);		
	}

	if( m_iBaseFragChild != -1 )
	{
		m_RenderAngleZ = ClampAngleToJointLimits(m_RenderAngleZ, *pParent, m_iBaseFragChild);
	}

	pParent->SetComponentRotation(m_iBaseBoneId,baseRotAxis,m_RenderAngleZ,true);

	if (m_Flags.IsFlagSet(TF_UseDesiredAngleForPitch))
	{
		pParent->SetComponentRotation(m_iPitchBoneId,turretTiltAxis,m_RenderAngleX,true);
	}

	if( pParent->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_KEEP_ALL_TURRETS_SYNCHRONISED ) )
	{
		int iStartTurretBone = VEH_TURRET_1_BASE;

		if( m_iBaseBoneId >= VEH_TURRET_B1_BASE )
		{
			iStartTurretBone = VEH_TURRET_B1_BASE;
		}
		else if( m_iBaseBoneId >= VEH_TURRET_A1_BASE )
		{
			iStartTurretBone = VEH_TURRET_A1_BASE;
		}

		int iEndTurretBone = iStartTurretBone + (MAX_NUM_VEHICLE_TURRETS_PER_SLOT * 2);

		for( int i = iStartTurretBone; i < iEndTurretBone; i++ )
		{
			if( m_iBaseBoneId != i )
			{
				pParent->SetComponentRotation( (eHierarchyId)i, baseRotAxis, m_RenderAngleZ, true );
			}
			i++;

			if (m_Flags.IsFlagSet(TF_UseDesiredAngleForPitch) && 
				m_iPitchBoneId != i)
			{
				pParent->SetComponentRotation( (eHierarchyId)i, turretTiltAxis, m_RenderAngleX, true );
			}
		}
	}

	if( bShouldBlendToDesiredAngle || bHaveStartedToBlendDesiredAngle )
	{
		m_Flags.SetFlag( TF_HaveUsedDesiredAngleForRendering );
	}

	// Treat like reset flags
	m_Flags.ClearFlag( TF_RenderDesiredAngleForPhysicalTurret );
	m_Flags.ClearFlag( TF_UseDesiredAngleForPitch );

	m_bBarrelInCollision = false;
}

int iBarracksTentComponent = 11;
//
void CTurretPhysical::ProcessPreComputeImpacts( CVehicle *pVehicle, phCachedContactIterator &impacts )
{
	fwModelId barracksModelId = CModelInfo::GetModelIdFromName( "barracks" );
	impacts.Reset();
	
	bool hasDeployedOutriggers = false;

	if( ( !pVehicle->GetAreOutriggersBeingDeployed() &&
		  pVehicle->GetOutriggerDeployRatio() > 0.01f &&
		  pVehicle->GetOutriggerDeployRatio() < 0.9f ) ||
		( m_DesiredAngle.Mag2() < 0.01f &&
		  m_AimAngle.Mag2() > 0.01f ) )
	{
		hasDeployedOutriggers = true;
	}

	while( !impacts.AtEnd() )
	{
		phInst* otherInstance = impacts.GetOtherInstance();

		// GTAV FIX B*1988899 - Disable tank turret collisions with foliage.
		if( otherInstance && otherInstance->GetArchetype()->GetTypeFlags() & ArchetypeFlags::GTA_FOLIAGE_TYPE &&
			( impacts.GetMyComponent() == m_iBarrelFragChild ||
			  impacts.GetMyComponent() == m_iBaseFragChild ) )
		{
			impacts.DisableImpact();
		}
		else if( impacts.GetMyComponent() == m_iBarrelFragChild )
		{
			CEntity* pOtherEntity = otherInstance ? (CEntity*)otherInstance->GetUserData() : NULL;
			if( pOtherEntity && pOtherEntity->GetArchetype() && pOtherEntity->GetModelId() == barracksModelId )
			{
				// disable collision between tank barrel and Barracks tent to prevent unexpected collision reaction
				if( impacts.GetOtherComponent() == iBarracksTentComponent )
				{
					impacts.DisableImpact();
				}
				else
				{
					m_bBarrelInCollision = true;
				}
			}
			else
			{
				if( pOtherEntity &&
					pVehicle )
				{
					if( pOtherEntity->GetIsTypePed() &&
						pVehicle->GetIsInWater() )
					{
						Vec3V myNormal;
						impacts.GetMyNormal( myNormal );

						if( myNormal.GetZf() > 0.5f )
						{
							impacts.DisableImpact();
							break;
						}
					}
					else if( pVehicle->IsTrailerSmall2() &&
							 pOtherEntity->GetIsTypeObject() )
					{
						const CProjectile* pProjectile = static_cast<CObject*>( pOtherEntity )->GetAsProjectile();
						if( pProjectile && pProjectile->GetOwner() && pProjectile->GetOwner()->GetIsTypePed() )
						{
							const CPed* pOwnerPed = static_cast<const CPed*>( pProjectile->GetOwner() );
							if( pOwnerPed->GetVehiclePedInside() == pVehicle )
							{
								impacts.DisableImpact();
								break;
							}
						}
					}
				}

				m_bBarrelInCollision = true;
				break;
			}
		}
		else if( m_bHeldTurret )
		{
			fragInst* pFragInst = pVehicle->GetVehicleFragInst();

			if( pFragInst )
			{
				int iWeaponComponent = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex( pVehicle->GetBoneIndex( VEH_WEAPON_1A ) );

				if( impacts.GetMyComponent() == iWeaponComponent )
				{
					m_bBarrelInCollision = true;
				}
			}
		}

		++impacts;
	}

	if( !pVehicle->IsNetworkClone() &&
		m_bBarrelInCollision && 
		hasDeployedOutriggers )
	{
		pVehicle->SetDeployOutriggers( true );
		m_DesiredAngle = m_AimAngle;
	}

	impacts.Reset();
}

void CTurretPhysical::DriveTurret(CVehicle* pParent, const int nTimeSlice)
{
	ComputeTurretSpeedModifier(pParent);

	if (m_Flags.IsFlagSet(TF_DisableMovement))
		return;

    // Drive the turret joints to their desired angle

    fragInstGta* pFragInst = pParent->GetVehicleFragInst();
    // check that we have a frag inst, and that it's the inst driving the car (i.e. it's not using dummy car physics)
    if(pFragInst==NULL || !pFragInst->IsInLevel() || pFragInst!=pParent->GetCurrentPhysicsInst())
	{
		return;
	}

    if( m_Flags.IsFlagSet( CTurret::TF_Hide ) )
    {
        return;
    }

    // find the associated joint
    fragHierarchyInst* pHierInst = NULL;
    if(pFragInst && pFragInst->GetCached())
    {
        pHierInst = pFragInst->GetCacheEntry()->GetHierInst();	
    }

	static dev_float sfTurretMuscleStiffness = 1.0f;
	static dev_float sfTurretMuscleAngleStrength = 10.0f;
	static dev_float sfTurretMuscleSpeedStrength = 40.0f;
	static dev_float sfTurretMinAndMaxMuscleTorqueHeldTurrets = 200.0f;
	static dev_float sfTurretMinAndMaxMuscleTorque = 100.0f;
	static dev_float sfTurretSpeedStrengthFadeOut = 0.5f;

	static dev_float sfTurretMuscleStiffnessUpsideDown = 0.0f;
	static dev_float sfTurretMuscleAngleStrengthUpsideDown = 1.0f;
	static dev_float sfTurretMuscleSpeedStrengthUpsideDown = 0.5f;
	static dev_float sfTurretMinAndMaxMuscleTorqueUpsideDown = 1.0f;
	static dev_float sfTurretSpeedStrengthFadeOutUpsideDown = 0.5f;

	static dev_float sfTurretMuscleStiffnessInCollision = 0.5f;
	static dev_float sfTurretMuscleAngleStrengthInCollision = 1.0f;
	static dev_float sfTurretMuscleSpeedStrengthInCollision = 2.0f;
	static dev_float sfTurretMinAndMaxMuscleTorqueInCollision = 2.0f;
	static dev_float sfTurretSpeedStrengthFadeOutInCollision = 0.5f;

	static dev_float sfTurretAimingAtTargetThreshold = 0.05f;
	static dev_float sfTurretCollisionAwayFromAimingAtTargetThreshold = 0.5f;

	float fTurretSpeed = GetTurretSpeed();

	float fTurretMuscleStiffness		= sfTurretMuscleStiffness;
	float fTurretMuscleAngleStrength	= sfTurretMuscleAngleStrength;	
	float fTurretMuscleSpeedStrength	= sfTurretMuscleSpeedStrength;
	float fTurretMinAndMaxMuscleTorque	= m_bHeldTurret ? sfTurretMinAndMaxMuscleTorqueHeldTurrets : sfTurretMinAndMaxMuscleTorque;	
	float fTurretSpeedStrengthFadeOut	= sfTurretSpeedStrengthFadeOut;

	Vector3 vDesiredEulers, vAimEulers;
	m_DesiredAngle.ToEulers(vDesiredEulers);
	m_AimAngle.ToEulers(vAimEulers);

	bool bZeroPitch = false;
	bool bResetPitch = false;
	bool bResetHeading = false;
	bool bCanReachDesiredAngle = true;
	bool bIsStandingTurret = false;
	bool bLargeTurret = false;
	CPed* pPedInSeat = NULL;
	CVehicleWeaponMgr* pVehicleWeaponMgr = pParent->GetVehicleWeaponMgr();

	bool bReduceTorqueWeReachingInitialAngle =	( !MI_TRUCK_CERBERUS.IsValid() || pParent->GetModelIndex() != MI_TRUCK_CERBERUS ) &&
												( !MI_TRUCK_CERBERUS2.IsValid() || pParent->GetModelIndex() != MI_TRUCK_CERBERUS2 ) &&
												( !MI_TRUCK_CERBERUS3.IsValid() || pParent->GetModelIndex() != MI_TRUCK_CERBERUS3 );

	if( pParent->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_OUTRIGGER_LEGS ) )
	{
		//add some different min forces for this, Pitch needs to at least keep it level
		float outriggerDeployedRatio = pParent->GetOutriggerDeployRatio();
		bLargeTurret = true;

		if( pParent->GetAreOutriggersBeingDeployed() )
		{
			static dev_float sfMaxDeployRatioForReducedForce = 0.8f;
			if( outriggerDeployedRatio < sfMaxDeployRatioForReducedForce )
			{
				m_bHasReachedDesiredAngle = false;

				if( outriggerDeployedRatio < 0.5f )
				{
					bResetPitch = true;
					bResetHeading = true;
					bCanReachDesiredAngle = false;
				}
			}
		}
		else
		{
			bResetHeading = true;

			static dev_float sfMaxDeployRatioForResetPitch = 0.3f;
			static dev_float sfMinDeployRatioForReducedForce = 0.01f;

			if( outriggerDeployedRatio < sfMaxDeployRatioForResetPitch )
			{
				bResetPitch = true;
			}

			if( outriggerDeployedRatio > sfMinDeployRatioForReducedForce )
			{
				m_bHasReachedDesiredAngle = false;
				bCanReachDesiredAngle = false;
			}
		}

		//	when deploying the legs keep it straight for 70% then still use reduced forces but don't reset pitch and heading
		//	after than the reached desired target flag doesn't need forcing

		//when retracting use reduced force until 30% up then whatever 
		//always reset Pitch and heading
	}

	const s32 iSeatIndex = pVehicleWeaponMgr ? pVehicleWeaponMgr->GetSeatIndexForTurret(*this) : -1;
	if (pParent->IsSeatIndexValid(iSeatIndex) )
	{
		bIsStandingTurret = pParent->GetSeatAnimationInfo(iSeatIndex)->IsStandingTurretSeat();
		pPedInSeat = pParent->GetPedInSeat(iSeatIndex);

		CPedWeaponManager* pWeaponMgr = pPedInSeat ? pPedInSeat->GetWeaponManager() : NULL;
		if (pWeaponMgr)
		{
			s32 sTurretWeaponIndex = pVehicleWeaponMgr->GetWeaponIndexForTurret(*this);
			CVehicleWeapon* pVehicleWeapon = sTurretWeaponIndex != -1 ? pVehicleWeaponMgr->GetVehicleWeapon(sTurretWeaponIndex) : NULL;
			if (pVehicleWeapon && pWeaponMgr->GetEquippedVehicleWeapon() != pVehicleWeapon)
			{
				bResetPitch = true;
				bResetHeading = true;
			}

			bool bIsBombushka = MI_PLANE_BOMBUSHKA.IsValid() && MI_PLANE_BOMBUSHKA == pParent->GetModelIndex();
			bool bIsVolatol = MI_PLANE_VOLATOL.IsValid() && MI_PLANE_VOLATOL == pParent->GetModelIndex();
			bool bIsAkula = MI_HELI_AKULA.IsValid() && MI_HELI_AKULA == pParent->GetModelIndex();
			if (pVehicleWeapon && (bIsBombushka || bIsVolatol || bIsAkula))
			{
				int iAmmo = pParent->GetRestrictedAmmoCount(pVehicleWeapon->m_handlingIndex);
				if (iAmmo == 0)
				{
					bResetPitch = true;
					bResetHeading = true;
				}
			}
		}
	}

	if (m_bHeldTurret || m_bResetTurretHeading)
	{
		if (pParent->IsTrailerSmall2() ||
			pParent->InheritsFromPlane())
		{
			CPed* pPedEnteringSeat = NULL;
			CComponentReservation* pComponentReservation = pParent->GetComponentReservationMgr()->GetSeatReservationFromSeat(iSeatIndex);
			if (pComponentReservation && pComponentReservation->GetPedUsingComponent())
			{
				pPedEnteringSeat = pComponentReservation->GetPedUsingComponent();
			}

			if (pPedInSeat != pPedEnteringSeat && pPedEnteringSeat != NULL)
			{
				bResetHeading = true;
				bResetPitch = true;
			}
		}

		if (!pPedInSeat || (pPedInSeat->ShouldBeDead() || !pPedInSeat->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON)))
		{
			if (m_bResetTurretHeading || (
				!bIsStandingTurret && !( MI_VAN_BOXVILLE5.IsValid() && pParent->GetModelIndex() == MI_VAN_BOXVILLE5 ) ) )
			{
				bResetPitch = true;
				if( pParent->InheritsFromHeli() ||
					m_bResetTurretHeading )
				{
					bResetHeading = true;
				}
			}
			else
			{
				bZeroPitch = true;
			}
		}
#if ENABLE_MOTION_IN_TURRET_TASK
		else if (bIsStandingTurret && pPedInSeat)
		{
			const CTaskMotionInTurret* pMotionInTurretTask = static_cast<const CTaskMotionInTurret*>(pPedInSeat->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_TURRET));
			if (!pMotionInTurretTask || !pMotionInTurretTask->ShouldHaveControlOfTurret())
			{
				bZeroPitch = true;
			}
		}
#endif // ENABLE_MOTION_IN_TURRET_TASK
	}

	bool bIsTrailerLarge = MI_TRAILER_TRAILERLARGE.IsValid() && pParent->GetModelIndex() == MI_TRAILER_TRAILERLARGE;
	bool bIsAvenger = MI_PLANE_AVENGER.IsValid() && pParent->GetModelIndex() == MI_PLANE_AVENGER;
	bool bIsTerrorbyte = MI_CAR_TERBYTE.IsValid() && pParent->GetModelIndex() == MI_CAR_TERBYTE;
	bool bIsScriptControlledTurret = bIsTrailerLarge || bIsAvenger || bIsTerrorbyte;

	bool bPreventScriptedReset = pParent->m_bDontResetTurretHeadingInScriptedCamera;
	bool bScriptedCamera = camInterface::IsDominantRenderedDirector(camInterface::GetScriptDirector()) || camInterface::GetScriptDirector().IsRendering();
	if ( !bPreventScriptedReset && bScriptedCamera && !bIsScriptControlledTurret )
	{
		bResetHeading = true;
		bResetPitch = true;
	}

	// Only drive AVENGER turret to desired angle if script are manually setting AimAtWorldPos (and m_bPedHasControlOfTurret as part of that)
	if ((bIsAvenger|| bIsTerrorbyte) && !m_bPedHasControlOfTurret)
	{
		bResetHeading = true;
		bResetPitch = true;
	}
	
	bool bOverridingPitchOrHeading = bResetPitch || bResetHeading;

	if(pParent->IsUpsideDown() || (pParent->IsInAir() && !m_bHeldTurret && !pParent->GetIsBeingTowed() && !m_bResetTurretHeading && !bIsAvenger))
	{
		fTurretMuscleStiffness			= sfTurretMuscleStiffnessUpsideDown;
		fTurretMuscleAngleStrength		= sfTurretMuscleAngleStrengthUpsideDown;
		fTurretMuscleSpeedStrength		= sfTurretMuscleSpeedStrengthUpsideDown;
		fTurretMinAndMaxMuscleTorque	= sfTurretMinAndMaxMuscleTorqueUpsideDown;	
		fTurretSpeedStrengthFadeOut		= sfTurretSpeedStrengthFadeOutUpsideDown;
	}
	else if (!m_bHeldTurret)
	{
		if( ( !bOverridingPitchOrHeading || !bCanReachDesiredAngle ) && 
			( m_bBarrelInCollision || ( !m_bHasReachedDesiredAngle && bReduceTorqueWeReachingInitialAngle ) ) )
		{
			fTurretMuscleStiffness			= sfTurretMuscleStiffnessInCollision;
			fTurretMuscleAngleStrength		= sfTurretMuscleAngleStrengthInCollision;
			fTurretMuscleSpeedStrength		= sfTurretMuscleSpeedStrengthInCollision;
			fTurretMinAndMaxMuscleTorque	= sfTurretMinAndMaxMuscleTorqueInCollision;	
			fTurretSpeedStrengthFadeOut		= sfTurretSpeedStrengthFadeOutInCollision;

			// the above forces are too low for very large turrets
			if( bLargeTurret )
			{
				static dev_float sfLargeTurretMuscleScale = 2.0f;
				fTurretMuscleStiffness *= sfLargeTurretMuscleScale;
				fTurretMuscleAngleStrength *= sfLargeTurretMuscleScale;
			}
		}
		else if( !bOverridingPitchOrHeading && !m_bPedHasControlOfTurret )
		{
			if( pParent->GetModelIndex() == MI_TANK_RHINO )
			{
				// FIX B*1824554 - if there is no one in this vehicle set the desired angle to be
				// the default angle so it is high enough for peds to walk under.
				if( vDesiredEulers.x < sfRhinoInitialTurretElevation )
				{
					vDesiredEulers.x = sfRhinoInitialTurretElevation;

					Quaternion desiredAngle;
					desiredAngle.FromEulers( vDesiredEulers );
					SetDesiredAngle( desiredAngle, pParent );
				}
			}
			m_bHasReachedDesiredAngle = false;	
		}
	}
	else
	{
		// Fix B*2194975 - We also have to reduce the turret forces for handheld turrets when there is a collision otherwise bad things happen
		if( m_bBarrelInCollision )
		{
			fTurretMuscleStiffness			= sfTurretMuscleStiffnessInCollision;
			fTurretMuscleAngleStrength		= sfTurretMuscleAngleStrengthInCollision;
			fTurretMuscleSpeedStrength		= sfTurretMuscleSpeedStrengthInCollision;
			fTurretMinAndMaxMuscleTorque	= sfTurretMinAndMaxMuscleTorqueInCollision;	
			fTurretSpeedStrengthFadeOut		= sfTurretSpeedStrengthFadeOutInCollision;
		}
	}

	bool tryToFreezeJointX = false;
	bool tryToFreezeJointZ = false;

	bool frozenJointX = false;
	bool frozenJointZ = false;

	if( bIsTerrorbyte &&
		!m_bPedHasControlOfTurret )
	{
		bOverridingPitchOrHeading = true;
		bResetHeading = true;
		bResetPitch = true;
	}

	if (bOverridingPitchOrHeading)
	{
		if (bResetPitch)
		{	
			vDesiredEulers.x = 0.0f;
		}
		if (bResetHeading)
		{
			vDesiredEulers.z = 0.0f;
		}
		Quaternion desiredAngle;
		desiredAngle.FromEulers( vDesiredEulers );
		SetDesiredAngle( desiredAngle, pParent );

		if( bResetHeading &&
			( pParent->InheritsFromPlane() ||
			  bIsTerrorbyte ) )
		{
			if( rage::Abs( vAimEulers.z ) < sfTurretAimingAtTargetThreshold )
			{
				pParent->Freeze1DofJointInCurrentPosition( pFragInst, m_iBaseFragChild, true );
				frozenJointZ = true;
			}

			tryToFreezeJointZ = true;
			vDesiredEulers.z = 0.0f;
		}
		if( bResetPitch &&
			( pParent->InheritsFromPlane() ||
			  bIsTerrorbyte ) )
		{
			if( rage::Abs( vAimEulers.x ) < sfTurretAimingAtTargetThreshold )
			{
				pParent->Freeze1DofJointInCurrentPosition( pFragInst, m_iBarrelFragChild, true );
				frozenJointX = true;
			}

			tryToFreezeJointX = true;
			vDesiredEulers.x = 0.0f;
		}
	}

	if(m_iBaseFragChild > -1 &&
		!frozenJointZ )
    {
		if( pParent->InheritsFromPlane() ||
			bIsTerrorbyte )
		{
			pParent->Freeze1DofJointInCurrentPosition( pFragInst, m_iBaseFragChild, false );
		}

		const float fHeadingModifier = m_fPerFrameHeadingModifier > 0.0f ? m_fPerFrameHeadingModifier : 1.0f;
		DriveComponentToAngle(pHierInst, m_iBaseFragChild, vDesiredEulers.z, fHeadingModifier * fTurretSpeed, vAimEulers.z, 
			fTurretMuscleStiffness,
			fTurretMuscleAngleStrength,
			fTurretMuscleSpeedStrength,
			fTurretMinAndMaxMuscleTorque,
			false,
			fTurretSpeedStrengthFadeOut );

		// if we're in collision and it has forced the barrel quite a long way from its target position set that it hasn't reached the desired angle
		// so that the force applied to move it after the collision isn't too great.
		if( m_bBarrelInCollision &&
			rage::Abs( vDesiredEulers.z - vAimEulers.z ) > sfTurretCollisionAwayFromAimingAtTargetThreshold )
		{
			m_bHasReachedDesiredAngle = false;
		}

		if( !m_bHasReachedDesiredAngle &&
			rage::Abs( vDesiredEulers.z - vAimEulers.z ) < sfTurretAimingAtTargetThreshold &&
			m_bPedHasControlOfTurret &&
			bCanReachDesiredAngle )
		{
			m_bHasReachedDesiredAngle = true;
		}

		if( tryToFreezeJointZ &&
			rage::Abs( vDesiredEulers.z - vAimEulers.z ) < sfTurretAimingAtTargetThreshold )
		{
			pParent->Freeze1DofJointInCurrentPosition( pFragInst, m_iBaseFragChild, true );
			frozenJointZ = true;
		}
    }
	if(m_iBarrelFragChild > -1 &&
		!frozenJointX )
    {
		if( pParent->InheritsFromPlane() ||
			bIsTerrorbyte )
		{
			pParent->Freeze1DofJointInCurrentPosition( pFragInst, m_iBarrelFragChild, false );
		}

		const float fPitchModifier = m_fPerFramePitchModifier > 0.0f ? m_fPerFramePitchModifier : 1.0f;
		DriveComponentToAngle(pHierInst, m_iBarrelFragChild, bZeroPitch ? 0.0f : vDesiredEulers.x, fPitchModifier * fTurretSpeed, vAimEulers.x,
			fTurretMuscleStiffness,
			fTurretMuscleAngleStrength,
			fTurretMuscleSpeedStrength,
			fTurretMinAndMaxMuscleTorque,
			false,
			fTurretSpeedStrengthFadeOut );

		if( tryToFreezeJointX &&
			rage::Abs( vDesiredEulers.z - vAimEulers.z ) < sfTurretAimingAtTargetThreshold )
		{
			pParent->Freeze1DofJointInCurrentPosition( pFragInst, m_iBaseFragChild, true );
			frozenJointX = true;
		}
    }
    m_AimAngle.FromEulers(vAimEulers);

	if( pParent->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_KEEP_ALL_TURRETS_SYNCHRONISED ) )
	{
		int iStartTurretBone = VEH_TURRET_1_BASE;

		if( m_iBaseBoneId >= VEH_TURRET_B1_BASE )
		{
			iStartTurretBone = VEH_TURRET_B1_BASE;
		}
		else if( m_iBaseBoneId >= VEH_TURRET_A1_BASE )
		{
			iStartTurretBone = VEH_TURRET_A1_BASE;
		}

		int iEndTurretBone = iStartTurretBone + (MAX_NUM_VEHICLE_TURRETS_PER_SLOT * 2);

		for( int i = iStartTurretBone; i < iEndTurretBone; i++ )
		{
			if( m_iBaseFragChild > -1 )
			{
				int boneIndex = pParent->GetBoneIndex( (eHierarchyId)i );

				if( boneIndex != -1 )
				{
					int baseFragChild = pParent->GetVehicleModelInfo()->GetFragType()->GetComponentFromBoneIndex( 0, boneIndex );

					if( baseFragChild != m_iBaseFragChild )
					{
						if( !frozenJointZ )
						{
							if( pParent->InheritsFromPlane() )
							{
								pParent->Freeze1DofJointInCurrentPosition( pFragInst, baseFragChild, false );
							}

							const float fHeadingModifier = m_fPerFrameHeadingModifier > 0.0f ? m_fPerFrameHeadingModifier : 1.0f;
							DriveComponentToAngle(pHierInst, baseFragChild, vDesiredEulers.z, fHeadingModifier * fTurretSpeed, vAimEulers.z, 
								fTurretMuscleStiffness,
								fTurretMuscleAngleStrength,
								fTurretMuscleSpeedStrength,
								fTurretMinAndMaxMuscleTorque,
								false,
								fTurretSpeedStrengthFadeOut);
						}
						else
						{
							pParent->Freeze1DofJointInCurrentPosition( pFragInst, baseFragChild, true );
						}
					}
				}
			}
			i++;

			if( m_iBarrelFragChild > -1 )
			{
				int boneIndex = pParent->GetBoneIndex( (eHierarchyId)i );

				if( boneIndex != -1 )
				{
					int barrelFragChild = pParent->GetVehicleModelInfo()->GetFragType()->GetComponentFromBoneIndex( 0, boneIndex );

					if( barrelFragChild != m_iBarrelFragChild )
					{
						if( !frozenJointX )
						{
							if( pParent->InheritsFromPlane() )
							{
								pParent->Freeze1DofJointInCurrentPosition( pFragInst, barrelFragChild, false );
							}

							const float fPitchModifier = m_fPerFramePitchModifier > 0.0f ? m_fPerFramePitchModifier : 1.0f;
							DriveComponentToAngle(pHierInst, barrelFragChild, bZeroPitch ? 0.0f : vDesiredEulers.x, fPitchModifier * fTurretSpeed, vAimEulers.x,
								fTurretMuscleStiffness,
								fTurretMuscleAngleStrength,
								fTurretMuscleSpeedStrength,
								fTurretMinAndMaxMuscleTorque,
								false,
								fTurretSpeedStrengthFadeOut );
						}
						else
						{
							pParent->Freeze1DofJointInCurrentPosition( pFragInst, barrelFragChild, true );
						}
					}
				}
			}
		}
	}

	if(CPhysics::GetIsLastTimeSlice(nTimeSlice))
	{
		m_fTurretSpeedOverride		= 0.0f;
		m_bPedHasControlOfTurret	= false;
	}

	m_fPerFrameHeadingModifier = -1.0f;
	m_fPerFramePitchModifier = -1.0f;
}

static dev_float fJointSleepAngleThreshold = 0.05f;

bool CTurretPhysical::WantsToBeAwake( CVehicle *pVehicle )
{
	fragInstGta* pFragInst = pVehicle->GetVehicleFragInst();
	// check that we have a frag inst, and that it's the inst driving the car (i.e. it's not using dummy car physics)
	if(pFragInst==NULL || !pFragInst->IsInLevel() || pFragInst!=pVehicle->GetCurrentPhysicsInst())
	{
		return false;
	}

	fragHierarchyInst* pHierInst = NULL;
	if(pFragInst && pFragInst->GetCached())
	{
		pHierInst = pFragInst->GetCacheEntry()->GetHierInst();	
	}

	const phArticulatedCollider* pArticulatedCollider = pHierInst ? pHierInst->articulatedCollider : NULL;

	if( !pArticulatedCollider )
	{
		return false;
	}

	if( pHierInst &&
		m_iBaseFragChild != - 1 )
	{
		s32 linkIndex = pArticulatedCollider->GetLinkFromComponent( m_iBaseFragChild );
		if(linkIndex > 0)
		{
			phJoint1Dof* p1DofJoint = NULL;
			phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
			if( pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF )
			{
				p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

				if(p1DofJoint)
				{
					float currentAngle = p1DofJoint->GetAngle(pHierInst->body);
					float maxAngle = 0.0f;
					float minAngle = 0.0f;

					p1DofJoint->GetAngleLimits( minAngle, maxAngle );
					float targetAngle  = Min( p1DofJoint->GetMuscleTargetAngle(), maxAngle );
					targetAngle = Max( targetAngle, minAngle );

					if( Abs( currentAngle - targetAngle ) > fJointSleepAngleThreshold )
					{
						return true;
					}
				}
			}
		}
	}

	if( pHierInst && 
		m_iBarrelFragChild != - 1 )
	{
		s32 linkIndex = pArticulatedCollider->GetLinkFromComponent( m_iBarrelFragChild );
		if(linkIndex > 0)
		{
			phJoint1Dof* p1DofJoint = NULL;
			phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
			if( pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF )
			{
				p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

				if(p1DofJoint)
				{
					float currentAngle = p1DofJoint->GetAngle(pHierInst->body);
					float maxAngle = 0.0f;
					float minAngle = 0.0f;

					p1DofJoint->GetAngleLimits( minAngle, maxAngle );
					float targetAngle  = Min( p1DofJoint->GetMuscleTargetAngle(), maxAngle );
					targetAngle = Max( targetAngle, minAngle );

					if( Abs( currentAngle - targetAngle ) > fJointSleepAngleThreshold )
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CTurretPhysical::ResetIncludeFlags(CVehicle* pParent)
{
	phBound* vehicleBound = pParent->GetVehicleFragInst()->GetArchetype()->GetBound();
	if(vehicleBound && vehicleBound->GetType() == phBound::COMPOSITE)
	{		
		phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(vehicleBound);
			
		if( GetBaseFragChild() > -1 )
		{
			u32 includeFlags = pBoundComp->GetIncludeFlags(GetBaseFragChild()) | ArchetypeFlags::GTA_CAMERA_TEST;		
			pBoundComp->SetIncludeFlags(GetBaseFragChild(), includeFlags);
		}

		if( GetBarrelFragChild() > -1 )
		{			
			u32 includeFlags = pBoundComp->GetIncludeFlags(GetBarrelFragChild()) | ArchetypeFlags::GTA_CAMERA_TEST;		
			pBoundComp->SetIncludeFlags(GetBarrelFragChild(), includeFlags);		
		}
	}
}

// check that we have a frag inst, and that it's the inst driving the car (i.e. it's not using dummy car physics)
bool IsFragInstValid(const fragInstGta* pFragInst, const CVehicle& rParent) 
{ 
	return pFragInst != NULL && pFragInst->IsInLevel() && pFragInst == rParent.GetCurrentPhysicsInst(); 
}

const fragHierarchyInst* GetFragHierarchyInst(const fragInstGta& rFragInst) 
{ 
	return rFragInst.GetCached() ? rFragInst.GetCacheEntry()->GetHierInst() : NULL; 
}

const phJoint1Dof* GetJoint1Dof(const CVehicle& rParent, s32 iFragChild)
{
	fragInstGta* pFragInst = rParent.GetVehicleFragInst();

	if (!IsFragInstValid(pFragInst, rParent))
		return NULL;

	// find the associated joint
	const fragHierarchyInst* pHierInst = GetFragHierarchyInst(*pFragInst);
	if (!pHierInst)
		return NULL;

	const phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
	if (!pArticulatedCollider || !pHierInst->body)
		return NULL;

	s32 linkIndex = pArticulatedCollider->GetLinkFromComponent(iFragChild);
	if (linkIndex <= 0)
		return NULL;

	const phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
	if (!pJoint)
		return NULL;

	if (pJoint->GetJointType() != phJoint::JNT_1DOF)
		return NULL;

	return static_cast<const phJoint1Dof*>(pJoint);
}

float CTurretPhysical::ClampAngleToJointLimits(float fAngle, const CVehicle& rParent, s32 iFragChild)
{
	const phJoint1Dof* p1DofJoint = GetJoint1Dof(rParent, iFragChild);
	if (!p1DofJoint)
		return fAngle;

	float minAngle ,maxAngle;
	p1DofJoint->GetAngleLimits(minAngle, maxAngle);
	if (minAngle > maxAngle)
	{
		SwapEm(minAngle ,maxAngle);
	}
	return Clamp(fAngle, minAngle, maxAngle);
}

//////////////////////////////////////////////////////////////////////////
// Class CSearchLight

#if __BANK	
void SearchLightLightInfo::InitWidgets(bkBank &bank)
{
	bank.AddSlider("Falloff Scale", &falloffScale, 0.0f, 50.0f, 0.1f);
	bank.AddSlider("Falloff Exponent", &falloffExp, 0.0f, 256.0f, 0.1f);
	bank.AddAngle("Inner Angle", &innerAngle, bkAngleType::DEGREES,0.0f,180.0f);
	bank.AddAngle("Outer Angle", &outerAngle, bkAngleType::DEGREES,0.0f,180.0f);
	bank.AddSlider("Global Intensity", &globalIntensity, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Light Intensity Scale", &lightIntensityScale, 0.0f, 10.0f, 0.1f);
	bank.AddSlider("Volume Intensity Scale", &volumeIntensityScale, 0.0f, 10.0f, 0.001f);
	bank.AddSlider("Volume Size Scale", &volumeSizeScale, 0.0f, 10.0f, 0.001f);
	bank.AddColor("Outer Volume Color", &outerVolumeColor);
	bank.AddSlider("Outer Volume Intensity", &outerVolumeIntensity, 0.0f, 10.0f, 0.001f);
	bank.AddSlider("Outer Volume Falloff", &outerVolumeFallOffExponent, 0.0f, 50.0f, 0.001f);	
	bank.AddSlider("Light Fade Distance", &lightFadeDist, 0.0f, 200.0f, 0.1f);

	bank.PushGroup("Corona Details");

	bank.AddSlider("Corona Intensity", &coronaIntensity, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Corona Intensity Far", &coronaIntensityFar, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Corona Offset", &coronaOffset, 0.0f, 50.0f, 0.001f);
	bank.AddSlider("Corona Size", &coronaSize, 0.0f, 10000.0f, 0.1f);
	bank.AddSlider("Corona Size Far", &coronaSizeFar, 0.0f, 10000.0f, 0.1f);
	bank.AddSlider("Corona Z Bias", &coronaZBias, 0.0f, 10.0f, 0.1f);

	bank.PopGroup();

	bank.AddToggle("Enable",&enable);
	bank.AddToggle("Specular",&specular);
	bank.AddToggle("Shadow",&shadow);
	bank.AddToggle("Volume",&volume);
}
#endif // __BANK
	

SearchLightInfo::SearchLightInfo() 
{
}

void SearchLightInfo::Set(const CVisualSettings& visualSettings, const char* type)
{
	length = visualSettings.Get(type, "length", 100.0f);
	offset = visualSettings.Get(type, "offset", -0.13f);

	atString colorPath(type);
	colorPath += ".color";
	color = Color32(visualSettings.GetColor(colorPath.c_str())); // Color32(33,33,33));

	mainLightInfo.falloffScale = visualSettings.Get(type, "mainLightInfo.falloffScale", 2.0f);
	mainLightInfo.falloffExp = visualSettings.Get(type, "mainLightInfo.falloffExp", 8.0f);
	mainLightInfo.innerAngle = visualSettings.Get(type, "mainLightInfo.innerAngle", 0.0f);
	mainLightInfo.outerAngle = visualSettings.Get(type, "mainLightInfo.outerAngle", 5.0f);
	
	mainLightInfo.globalIntensity = visualSettings.Get(type, "mainLightInfo.globalIntensity", 4.0f);
	mainLightInfo.lightIntensityScale = visualSettings.Get(type, "mainLightInfo.lightIntensityScale", 10.0f);
	mainLightInfo.volumeIntensityScale = visualSettings.Get(type, "mainLightInfo.volumeIntensityScale", 0.125f);
	//Reducing this scale to reduce chance of obvious artifacts
	//and increase performance a bit
	mainLightInfo.volumeSizeScale = visualSettings.Get(type, "mainLightInfo.volumeSizeScale", 0.5f);
	
	atString outerVolumeColorPath(type);
	outerVolumeColorPath += ".color";
	mainLightInfo.outerVolumeColor = Color32(visualSettings.GetColor(outerVolumeColorPath.c_str())); // Color32(33,33,33));
	mainLightInfo.outerVolumeIntensity = visualSettings.Get(type, "mainLightInfo.outerVolumeIntensity", 0.0f);
	mainLightInfo.outerVolumeFallOffExponent = visualSettings.Get(type, "mainLightInfo.outerVolumeFallOffExponent", 50.0f);
		

	mainLightInfo.enable = visualSettings.Get(type, "mainLightInfo.enable", 1.0f) == 1.0f;
	mainLightInfo.specular = visualSettings.Get(type, "mainLightInfo.specular", 1.0f) == 1.0f;
	mainLightInfo.shadow = visualSettings.Get(type, "mainLightInfo.shadow", 1.0f) == 1.0f;	
	mainLightInfo.volume = visualSettings.Get(type, "mainLightInfo.volume", 1.0f) == 1.0f;

	mainLightInfo.coronaIntensity = visualSettings.Get(type, "mainLightInfo.coronaIntensity", 6.0f);
	mainLightInfo.coronaIntensityFar = visualSettings.Get(type, "mainLightInfo.coronaIntensityFar", 12.0f);
	mainLightInfo.coronaSize = visualSettings.Get(type, "mainLightInfo.coronaSize", 3.5f);
	mainLightInfo.coronaSizeFar = visualSettings.Get(type, "mainLightInfo.coronaSizeFar", 35.0f);
	mainLightInfo.coronaZBias = visualSettings.Get(type, "mainLightInfo.coronaZBias", 0.3f);
	mainLightInfo.coronaOffset = visualSettings.Get(type, "mainLightInfo.coronaOffset", 0.4f);

	mainLightInfo.lightFadeDist = visualSettings.Get(type, "mainLightInfo.lightFadeDist", 40.0f);
}

#if __BANK	
void SearchLightInfo::InitWidgets(bkBank &bank)
{
		
	bank.AddSlider("Length",&length,0.0f,300.0f,0.1f);
		
	bank.AddSlider("Offset",&offset,-5.0f,5.0f,0.001f);
	bank.AddColor("Color",&color);
		
	bank.PushGroup("Main light");
	mainLightInfo.InitWidgets(bank);
	bank.PopGroup();
		
}
#endif // __BANK


SearchLightInfo defaultslInfo;


void CSearchLight::UpdateVisualDataSettings()
{
	defaultslInfo.Set(g_visualSettings,"defaultsearchlight");
	CRotaryWingAircraft::GetSearchLightInfo().Set(g_visualSettings,"helisearchlight");
	CBoat::GetSearchLightInfo().Set(g_visualSettings,"boatsearchlight");
}

#if __BANK
void CSearchLight::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Search Light");

	bank.PushGroup("Default Settings");
	defaultslInfo.InitWidgets(bank);
	bank.PopGroup();

	bank.PushGroup("Heli Settings");
	CRotaryWingAircraft::GetSearchLightInfo().InitWidgets(bank);
	bank.PopGroup();

	bank.PushGroup("Boat Settings");
	CBoat::GetSearchLightInfo().InitWidgets(bank);
	bank.PopGroup();

	bank.PopGroup();
}
#endif // __BANK


const SearchLightInfo &CSearchLight::GetSearchLightInfo(CVehicle *parent)
{
	if(parent)
	{
		if(parent->GetVehicleType() == VEHICLE_TYPE_HELI)
		{
			return CRotaryWingAircraft::GetSearchLightInfo();
		}
		else if(parent->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			return CBoat::GetSearchLightInfo();
		}
	}

	return defaultslInfo;

}
CSearchLight::CSearchLight(u32 nWeaponHash, eHierarchyId iWeaponBone)
	: CFixedVehicleWeapon(nWeaponHash, iWeaponBone) 
{
	m_lightOn = false;
	m_isDamaged = false;
	m_bForceOn = false;
	m_LastSearchLightSample = fwTimer::GetTimeInMilliseconds();
	for(s32 C=0; C<SEARCHLIGHTSAMPLES; C++)
	{
		m_OldSearchLightTargetPos[C].Set(0,0,0);
	}
	m_targetPos.Set(0,0,0);
	m_CloneTargetPos.Set(0,0,0);
	m_LightBrightness = 0.0f;
	m_searchLightBlocked = false;
	m_bHasPriority = true;
	m_SweepTheta = 0.0f;
	m_fSearchLightExtendTimer.Set(0,0,0);
}

CSearchLight::~CSearchLight()
{
}

void CSearchLight::ProcessSearchLight(CVehicle* parent)
{
	CPhysical *pTarget = GetSearchLightTarget();

	if (m_lightOn && pTarget)
	{
		// Set
		Vector3 TargetPos = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
		Vector3 TargetSpeed = pTarget->GetVelocity();

		if (parent->IsNetworkClone() && !parent->ContainsPlayer())
		{
			// Get all weapons and turrets associated with our seat
			atArray<CVehicleWeapon*> weaponArray;
			atArray<CTurret*> turretArray;
			parent->GetVehicleWeaponMgr()->GetWeaponsAndTurretsForSeat(0,weaponArray,turretArray);

			int numTurrets = turretArray.size(); 
			for(int iTurretIndex = 0; iTurretIndex < numTurrets; iTurretIndex++)
			{
				m_CloneTargetPos = TargetPos;
				if(parent->GetDriver())
				{
					CTaskVehicleMountedWeapon::ComputeSearchTarget(m_CloneTargetPos, *parent->GetDriver());
				}

				turretArray[iTurretIndex]->AimAtWorldPos(m_CloneTargetPos, parent, false, true);
			}
		}

		s32	TimePassed;// WaitBeforeOpeningFire=0;
		float	Interp, LightDist; // DistSqr; // XDiff, YDiff


		if( m_targetProbeHandle.GetResultsWaitingOrReady() )
		{ // Did we have a previous probe with some results worth checking out ?
			Vector3 intersection;
			bool bReady = m_targetProbeHandle.GetResultsReady();
			bool bHit = false;

			if(bReady)
			{
				bHit = m_targetProbeHandle.GetNumHits() > 0u;
				m_searchLightBlocked = bHit;

				if(bHit)
				{
					m_targetPos = m_targetProbeHandle[0].GetHitPosition();
					TargetSpeed.Zero();
				}

				m_targetProbeHandle.Reset();
			}
		}

		if( true == m_searchLightBlocked )
		{
			TargetPos = m_targetPos;
			TargetSpeed.Zero();
		}

		TimePassed = fwTimer::GetTimeInMilliseconds() - m_LastSearchLightSample;
		bool probeTime = false;
		while (TimePassed > 1000)
		{	// Need to make a fresh prediction
			for(s32 C=SEARCHLIGHTSAMPLES-1; C>0; C--)
			{
				m_OldSearchLightTargetPos[C] = m_OldSearchLightTargetPos[C-1];
			}

			m_OldSearchLightTargetPos[0] = TargetPos + TargetSpeed * (2);	// prediction of players coordinates two secs from now
			TimePassed -= 1000;
			m_LastSearchLightSample += 1000;

			probeTime = true;
		}
		Interp = TimePassed / 1000.0f;
		m_targetPos = m_OldSearchLightTargetPos[1] * Interp + m_OldSearchLightTargetPos[2] * (1.0f - Interp);

		if( probeTime )
		{ // Launch an async probe to check for anything in between us and the targets...
			Vector3 startPos = VEC3V_TO_VECTOR3(parent->GetTransform().GetPosition());
			Vector3 endPos = m_targetPos;
			const u32 iTestTypes = ArchetypeFlags::GTA_ALL_MAP_TYPES;
			m_targetProbeHandle.Reset();
			WorldProbe::CShapeTestProbeDesc probeData;
			probeData.SetResultsStructure(&m_targetProbeHandle);
			probeData.SetStartAndEnd(startPos, endPos);
			probeData.SetContext(WorldProbe::ENotSpecified);
			probeData.SetExcludeEntity(parent);
			probeData.SetIncludeFlags(iTestTypes);
			probeData.SetIsDirected(true);
			WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}

		static dev_float hs_angle				= 75.0f;
		static dev_float hs_scaleReactionMin 	= 0.3f;
		static dev_float hs_scaleReactionMax 	= 1.0f;
		static dev_float hs_twitchFreq 		= 0.375f;
		static dev_float hs_twitchVel 			= 0.001f;

		static dev_float hs_lim_x 				= 0.02f;
		static dev_float hs_lim_y 				= 0.02f;
		static dev_float hs_lim_z 				= 0.01f;

		static dev_float hs_motion_x			= 0.0002f;
		static dev_float hs_motion_y			= 0.0002f;
		static dev_float hs_motion_z			= 0.0001f;

		static dev_float hs_slow_x 			= 1.0f;
		static dev_float hs_slow_y 			= 1.0f;
		static dev_float hs_slow_z 			= 1.0f;

		m_searchLightTilt.SetMinReactionScaling(hs_scaleReactionMin);
		m_searchLightTilt.SetMaxReactionScaling(hs_scaleReactionMax);
		m_searchLightTilt.SetTwitchFrequency(hs_twitchFreq);
		m_searchLightTilt.SetMaxTwitchSpeed(hs_twitchVel);

		m_searchLightTilt.SetMaxRotationEulers(Vector3(hs_lim_x, hs_lim_y, hs_lim_z));

		m_searchLightTilt.SetMaxVelocityDeltaEulers(Vector3(hs_motion_x, hs_motion_y, hs_motion_z));

		m_searchLightTilt.SetDecelerationScalingEulers(Vector3(hs_slow_x, hs_slow_y, hs_slow_z));

		m_searchLightTilt.Update(hs_angle);
		static dev_bool useConstantOffset = false;

		Vector3 offset;

		if( useConstantOffset )
		{
			offset.Set(1.0f,1.0f,1.0f);
		}
		else
		{
			static dev_float randomRangeMin = 0.8f;
			static dev_float randomRangeMax = 1.0;
			offset.Set(	fwRandom::GetRandomNumberInRange(randomRangeMin, randomRangeMax),
				fwRandom::GetRandomNumberInRange(randomRangeMin, randomRangeMax),
				fwRandom::GetRandomNumberInRange(randomRangeMin, randomRangeMax) );
		}

		// apply shaking
		m_searchLightTilt.ApplyShake(offset);

		m_targetPos += offset;


		// Is this within range of the actual heli ?
		const Vector3 vThisPosition = VEC3V_TO_VECTOR3(parent->GetTransform().GetPosition());

		LightDist = rage::Sqrtf((m_targetPos.x - vThisPosition.x)*(m_targetPos.x - vThisPosition.x) + (m_targetPos.y - vThisPosition.y)*(m_targetPos.y - vThisPosition.y));

		const SearchLightInfo& slInfo = GetSearchLightInfo(parent);

		const float maxDist = CTaskHelicopterStrafe::GetTunables().m_TargetOffset + slInfo.mainLightInfo.lightFadeDist;
		const float fadeDist = slInfo.mainLightInfo.lightFadeDist;
		m_LightBrightness = rage::Clamp( (maxDist - LightDist)/(fadeDist),0.0f,1.0f);
	}
}

void CSearchLight::ProcessPostPreRender(CVehicle* parent)
{
	CFixedVehicleWeapon::ProcessPostPreRender(parent);

	if( m_lightOn && (m_bForceOn || m_bHasPriority) )
	{
		const SearchLightInfo& slInfo = GetSearchLightInfo(parent);

		int nBoneIndex = parent->GetBoneIndex((eHierarchyId)m_iWeaponBone);
		if(nBoneIndex < 0)
			return;

		Matrix34 mtx;
		parent->GetGlobalMtx(nBoneIndex,mtx);
		
		Vector3 lightPos = mtx.d;
		Vector3 lightDir = mtx.b;
		Vector3 lightTangent = mtx.a;
		Vector3 t_lightPos = lightPos+(lightDir*slInfo.offset);
		Vector3 targetPos = lightPos + lightDir * slInfo.length;

		Vector3	lightColour = VEC3V_TO_VECTOR3(slInfo.color.GetRGB());

		// Main light
		if( slInfo.mainLightInfo.enable )
		{

			const float dayLightFade = CVehicle::CalculateVehicleDaylightFade();
			if(dayLightFade > 0)
			{
				float lightDist=(targetPos - t_lightPos).Mag();

				float falloff = lightDist * slInfo.mainLightInfo.falloffScale;

				int flags=LIGHTFLAG_VEHICLE | LIGHTFLAG_DONT_LIGHT_ALPHA;
				if (slInfo.mainLightInfo.volume) flags |= LIGHTFLAG_DRAW_VOLUME|LIGHTFLAG_USE_VOLUME_OUTER_COLOUR;
				if (slInfo.mainLightInfo.specular == false) flags |= LIGHTFLAG_NO_SPECULAR;
				if (slInfo.mainLightInfo.shadow) flags |= LIGHTFLAG_CAST_SHADOWS|LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS|LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS|LIGHTFLAG_MOVING_LIGHT_SOURCE;

				CLightSource* pLightSource = CAsyncLightOcclusionMgr::AddLight();
				if (pLightSource)
				{
					pLightSource->Reset();
					pLightSource->SetCommon(LIGHT_TYPE_SPOT, flags, t_lightPos, lightColour, slInfo.mainLightInfo.globalIntensity * slInfo.mainLightInfo.lightIntensityScale * dayLightFade, LIGHT_ALWAYS_ON);
					pLightSource->SetDirTangent(lightDir, lightTangent);
					pLightSource->SetRadius(falloff);
					pLightSource->SetSpotlight(slInfo.mainLightInfo.innerAngle, slInfo.mainLightInfo.outerAngle);
					pLightSource->SetLightVolumeIntensity(	slInfo.mainLightInfo.globalIntensity * slInfo.mainLightInfo.volumeIntensityScale * dayLightFade, 
						slInfo.mainLightInfo.volumeSizeScale,
						Vec4V(slInfo.mainLightInfo.outerVolumeColor.GetRGB(), ScalarV(slInfo.mainLightInfo.outerVolumeIntensity)),
						slInfo.mainLightInfo.outerVolumeFallOffExponent);
					pLightSource->SetShadowTrackingId(fwIdKeyGenerator::Get(this, 2));
					pLightSource->SetExtraFlags(EXTRA_LIGHTFLAG_VOL_USE_HIGH_NEARPLANE_FADE | EXTRA_LIGHTFLAG_CAUSTIC);
					pLightSource->SetFalloffExponent(slInfo.mainLightInfo.falloffExp);
					// NOTE: we don't want to call Lights::AddSceneLight directly - the AsyncLightOcclusionMgr will handle that for us
				}

				// Adding Corona

				// Get brightness scaler based on cam distance
				// two versions : one for "small" lights : Indicator, interior light, reversing, and brakes.
				// And one for the main one : Head and tail Light, Sirens.
				float camDist = 0.0f;
				Vec3V vPosition = RCC_VEC3V(t_lightPos);
				Vector3 camPos = VEC3V_TO_VECTOR3(gVpMan.GetUpdateGameGrcViewport()->GetCameraPosition());
				float camDistSqr = CVfxHelper::GetDistSqrToCamera(vPosition);	

				// under a veh light cutoff...
				//dev_float heightCutoffDistanceApply = 50.0f;
				dev_float heightCutoffHeightDiffereance = 2.0f;
				dev_float heightCutoffFadeDistance = 1.0f;

				const Vector3 vThisPosition = VEC3V_TO_VECTOR3(vPosition);
				const float heightDiff = vThisPosition.z - camPos.z;
				float heightFade = 1.0f;
				if( heightDiff > 0.0f )
				{
					heightFade = rage::Clamp((heightCutoffHeightDiffereance - heightDiff)*(1.0f/heightCutoffFadeDistance) ,0.0f,1.0f);
				}

				if( heightFade > 0.0f )
				{
					camDist = rage::Sqrtf(camDistSqr);
				}

				Vector3 coronaPos = lightPos+(lightDir*(slInfo.offset + slInfo.mainLightInfo.coronaOffset));

				static dev_float coronaFade_MainFadeRatio = 0.25f;
				float coronaFade_DistFadeRatio = 1.0f - coronaFade_MainFadeRatio;
				static dev_float coronaFade_BeginStart = 50.0f;
				static dev_float coronaFade_BeginEnd = 300.0f;
				static dev_float coronaFade_CutoffStart = 290.0f;
				static dev_float coronaFade_CutoffEnd = 300.0f;
				float ooFadeLength = 1.0f / (coronaFade_CutoffEnd - coronaFade_CutoffStart);
				float cutoffFade = rage::Clamp((coronaFade_CutoffStart - camDist) * ooFadeLength,0.0f,1.0f);
				float ooShutDownEnd = 1.0f /(coronaFade_BeginEnd - coronaFade_BeginStart);
				float coronaFade = ((1.0f - coronaFade_MainFadeRatio) * 0.25f +
					rage::Clamp((camDist - coronaFade_BeginStart)*ooShutDownEnd * coronaFade_DistFadeRatio,0.0f,coronaFade_DistFadeRatio)) * cutoffFade;


				//static dev_float zbiasCorona = 0.25f;

				const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
				float coronaIntensity  = Lerp(coronaFade,slInfo.mainLightInfo.coronaIntensity,slInfo.mainLightInfo.coronaIntensityFar) * currKeyframe.GetVar(TCVAR_SPRITE_BRIGHTNESS);
				float coronaSize = Lerp(coronaFade, slInfo.mainLightInfo.coronaSize,slInfo.mainLightInfo.coronaSizeFar) * currKeyframe.GetVar(TCVAR_SPRITE_SIZE);

				g_coronas.Register(RCC_VEC3V(coronaPos), 
					coronaSize, 
					lightColour, 
					coronaIntensity, 
					slInfo.mainLightInfo.coronaZBias, 
					RCC_VEC3V(lightDir),
					1.0f,
					slInfo.mainLightInfo.innerAngle,
				    slInfo.mainLightInfo.outerAngle,
					CORONA_DONT_REFLECT | CORONA_HAS_DIRECTION);

			}
		}
	}
}

void CSearchLight::SetLightOn(bool b)
{
	m_lightOn = b;
}

void CSearchLight::SetSearchLightTarget(CPhysical* pTarget) 
{
	m_SearchLightTarget.SetEntity((CEntity*)pTarget);
#if GTA_REPLAY
	if(pTarget)
		m_SearchLightTargetReplayId = pTarget->GetReplayID();
	else
		m_SearchLightTargetReplayId = NoEntityID;
#endif // GTA_REPLAY
}

bool CSearchLight::FireInternal(CVehicle* pParent)
{
	m_lightOn = !m_lightOn;
	return CFixedVehicleWeapon::FireInternal(pParent);
}
	
void CSearchLight::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_lightOn, "Searchlight on");
	m_bForceOn = m_lightOn;
	SERIALISE_SYNCEDENTITY(m_SearchLightTarget,serialiser,"Searchlight target");
}	

void CSearchLight::CopyTargetInfo( CSearchLight& searchLight)
{
	CEntity* pEntity = searchLight.m_SearchLightTarget.GetEntity();

	m_SearchLightTarget.SetEntity(pEntity);
#if GTA_REPLAY
	replayAssertf(pEntity == NULL || pEntity->GetIsPhysical(), "CSearchLight::CopyTargetInfo()....Expecting a CPhysical (See SetSearchLightTarget())");
	if(pEntity)
		m_SearchLightTargetReplayId = ((CPhysical *)pEntity)->GetReplayID();
#endif // GTA_REPLAY
	m_CloneTargetPos = searchLight.m_CloneTargetPos;
	m_targetPos = searchLight.m_targetPos;
	m_bForceOn	= searchLight.m_bForceOn; 
	m_lightOn	= searchLight.m_lightOn;
	m_bHasPriority = searchLight.m_bHasPriority;
	m_SweepTheta = searchLight.m_SweepTheta;
	m_fSearchLightExtendTimer = searchLight.m_fSearchLightExtendTimer;
	m_LastSearchLightSample = searchLight.m_LastSearchLightSample; 
	m_LightBrightness = searchLight.m_LightBrightness;
	for(s32 C=0; C<SEARCHLIGHTSAMPLES; C++)
	{
		m_OldSearchLightTargetPos[C] = searchLight.m_OldSearchLightTargetPos[C];
	}	
}



//////////////////////////////////////////////////////////////////////////
static bank_float sfRadarRotSpeed = PI / 2.0f;
static bank_float sfRadarRotLimit = 2.0f * PI;

#if __BANK	
void CVehicleRadar::InitWidgets(bkBank &bank)
{
	bank.PushGroup("Radar");
		bank.AddSlider("Rotation Speed", &sfRadarRotSpeed, -50.0f, 50.0f, 0.1f);
	bank.PopGroup();
}
#endif // __BANK

void CVehicleRadar::ProcessPostPreRender(CVehicle* parent)
{
	m_fAngle += sfRadarRotSpeed * fwTimer::GetTimeStep();
	if(m_fAngle >= sfRadarRotLimit)
	{
		m_fAngle = 0.0f;
	}
	parent->SetComponentRotation(m_iWeaponBone,ROT_AXIS_Z,m_fAngle,true);
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadget::DriveComponentToAngle(fragHierarchyInst* pHierInst, 
                                           s32 iFragChild, 
                                           float fDesiredAngle, 
                                           float fDesiredSpeed, 
                                           float& fCurrentAngleOut,
                                           float fMuscleStiffness,
                                           float fMuscleAngleStrength,
                                           float fMuscleSpeedStrength,
                                           float fMinAndMaxMuscleTorque,
                                           bool bOnlyControlSpeed,
                                           float fSpeedStrengthFadeOutAngle,
										   bool bOnlyUpdateCurrentAngle )
{
	Assert(pHierInst);
	Assert(iFragChild > -1);

	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;

	if(pArticulatedCollider == NULL || pHierInst->body == NULL)
	{
		return;
	}
	
	s32 linkIndex = pArticulatedCollider->GetLinkFromComponent(iFragChild);
	if(linkIndex > 0)
	{
		phJoint1Dof* p1DofJoint = NULL;
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

		if(p1DofJoint)
		{
			if( !bOnlyUpdateCurrentAngle )
			{
				// Set the strength and stiffness
				// Done here for tweaking
				// Todo: move to Init()

				// Need to scale some things by inertia
				const float fInvAngInertia = p1DofJoint->ResponseToUnitTorque(pHierInst->body);
				float fMinMaxTorque = fMinAndMaxMuscleTorque / fInvAngInertia;
				// is it possible (or better) to get the inertia from the FragType?

				p1DofJoint->SetStiffness(fMuscleStiffness); 		// If i set this to >= 1.0f the game crashes with a NAN root link position
				p1DofJoint->SetMinAndMaxMuscleTorque(fMinMaxTorque);
				p1DofJoint->SetMuscleAngleStrength(fMuscleAngleStrength);
				p1DofJoint->SetMuscleSpeedStrength(fMuscleSpeedStrength);

				if(bOnlyControlSpeed)
				{
					p1DofJoint->SetMuscleTargetSpeed(fDesiredSpeed);
					p1DofJoint->SetDriveState(phJoint::DRIVE_STATE_SPEED);
				}
				else
				{	
					float minAngle,maxAngle;
					p1DofJoint->GetAngleLimits(minAngle,maxAngle);
					if(minAngle > maxAngle)
					{
						SwapEm(minAngle,maxAngle);
					}
					float clampedDesiredAngle = Clamp(fDesiredAngle,minAngle,maxAngle);
					p1DofJoint->SetMuscleTargetAngle(clampedDesiredAngle);
					p1DofJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE);
					p1DofJoint->SetMuscleTargetSpeed(fDesiredSpeed);

					static dev_bool bSetTargetSpeed = true;
					if(bSetTargetSpeed)
					{
						// Smooth the desired speed out close to the target
						fCurrentAngleOut = p1DofJoint->GetAngle( pHierInst->body );
						float fAngleDiff = clampedDesiredAngle - Clamp(fCurrentAngleOut,minAngle,maxAngle);
						fAngleDiff = fwAngle::LimitRadianAngle(fAngleDiff);
						float fJointSpeedTarget = 0.0f;				
						const float kfTimeStepScale = 1.0f/(DESIRED_FRAME_RATE*fwTimer::GetTimeStep());
						fJointSpeedTarget = rage::Clamp(fDesiredSpeed*kfTimeStepScale*fAngleDiff/fSpeedStrengthFadeOutAngle,-fDesiredSpeed,fDesiredSpeed);

						// If this is set to 0 then we get damping
						// this gives us damping
						p1DofJoint->SetMuscleTargetSpeed(fJointSpeedTarget);
						p1DofJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
					}
				}
			}

            fCurrentAngleOut = p1DofJoint->GetAngle(pHierInst->body);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadget::SnapComponentToAngle(fragInstGta* pFragInst,
										  s32 iFragChild, 
										  float fDesiredAngle,
										  float& fCurrentAngleOut)
{
	Assert(pFragInst->GetCacheEntry());	// All vehicles should be in the cache

	fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();
	Assert(pHierInst);

	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
	Assert(pArticulatedCollider);

	if(pArticulatedCollider == NULL)
	{
		return;
	}

	pFragInst->GetCacheEntry()->LazyArticulatedInit();

	int linkIndex = pArticulatedCollider->GetLinkFromComponent(iFragChild);
	if(linkIndex > 0)
	{
		phJoint1Dof* p1DofJoint = NULL;
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

		if(p1DofJoint)
		{
			p1DofJoint->MatchChildToParentPositionAndVelocity(pArticulatedCollider->GetBody(),ScalarVFromF32(fDesiredAngle));
			pArticulatedCollider->GetBody()->CalculateInertias();
			pArticulatedCollider->UpdateCurrentMatrices();

			if(CPhysics::GetLevel()->IsInactive(pFragInst->GetLevelIndex()) && 
				CPhysics::GetEntityFromInst(pFragInst) && CPhysics::GetEntityFromInst(pFragInst)->GetIsPhysical())
			{
				// Activate the physics before sync skeleton to articulated collider
				((CPhysical *)CPhysics::GetEntityFromInst(pFragInst))->ActivatePhysics();
			}

			pFragInst->SyncSkeletonToArticulatedBody(true);

			fCurrentAngleOut = p1DofJoint->GetAngle(pHierInst->body);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
static dev_float fSpeedStrengthFadeOutPosition = 0.5f;

void CVehicleGadget::DriveComponentToPosition(fragHierarchyInst* pHierInst, 
                                           s32 iFragChild, 
                                           float fDesiredPosition, 
                                           float fDesiredSpeed, 
                                           float &fCurrentPositionOut,
                                           Vector3 &vecJointChildPosition,
                                           float fMuscleStiffness,
                                           float fMusclePositionStrength,
                                           float fMuscleSpeedStrength,
                                           float fMinAndMaxMuscleForce,
										   bool bOnlyControlSpeed)
{
    Assert(pHierInst);
    Assert(iFragChild > -1);

    phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
    Assert(pArticulatedCollider);

    if(pArticulatedCollider == NULL || pHierInst->body == NULL)
    {
        return;
    }

    s32 linkIndex = pArticulatedCollider->GetLinkFromComponent(iFragChild);
    if(linkIndex > 0)
    {
        phPrismaticJoint* pPrismaticJoint = NULL;
        phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
        if (pJoint && pJoint->GetJointType() == phJoint::PRISM_JNT)
            pPrismaticJoint = static_cast<phPrismaticJoint*>(pJoint);

        if(pPrismaticJoint)
        {
            // Set the strength and stiffness
            // Done here for tweaking
            // Todo: move to Init()

            // Need to scale some things by mass
            Vector3 jointLimitAxis, JointPosition;
            pPrismaticJoint->GetJointLimitAxis(pHierInst->body, 0, jointLimitAxis, JointPosition);
            const float fInvMass = pPrismaticJoint->ResponseToUnitForceOnAxis(pHierInst->body, VECTOR3_TO_VEC3V(jointLimitAxis)).Getf();
            float fMinMaxForce = fMinAndMaxMuscleForce / fInvMass;
            // is it possible (or better) to get the inertia from the FragType?

            pPrismaticJoint->SetStiffness(fMuscleStiffness);		// If i set this to >= 1.0f the game crashes with a NAN root link position
            pPrismaticJoint->SetMinAndMaxMuscleForce(fMinMaxForce);
            pPrismaticJoint->SetMusclePositionStrength(fMusclePositionStrength);
            pPrismaticJoint->SetMuscleSpeedStrength(fMuscleSpeedStrength);

            pPrismaticJoint->SetMuscleTargetPosition(fDesiredPosition);

            fCurrentPositionOut = pPrismaticJoint->ComputeCurrentTranslation(pHierInst->body);
            vecJointChildPosition = pPrismaticJoint->GetPositionChild();

            static dev_bool bSetTargetSpeed = true;
            if(bSetTargetSpeed)
            {
                // Smooth the desired speed out close to the target
                float fPositionDiff = fDesiredPosition - fCurrentPositionOut;
                float fJointSpeedTarget = 0.0f;	

                if(Sign(fPositionDiff) == Sign(fDesiredSpeed))// don't allow the joint to move past its limits		
                    fJointSpeedTarget = rage::Clamp(fDesiredSpeed*fPositionDiff/fSpeedStrengthFadeOutPosition,-fDesiredSpeed,fDesiredSpeed);

                pPrismaticJoint->SetMuscleTargetSpeed(fJointSpeedTarget);
            }
            else
            {
                pPrismaticJoint->SetMuscleTargetSpeed(fDesiredSpeed);
            }

			if(bOnlyControlSpeed)
			{
				pPrismaticJoint->SetDriveState(phJoint::DRIVE_STATE_SPEED);
			}
			else
			{
				pPrismaticJoint->SetMuscleTargetPosition(pPrismaticJoint->GetMuscleTargetPosition());
				pPrismaticJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
			}
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadget::SwingJointFreely(fragHierarchyInst* pHierInst, 
                                              s32 iFragChild)
{
    Assert(pHierInst);
    Assert(iFragChild > -1);

    phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
    if(pArticulatedCollider == NULL || pHierInst->body == NULL)
    {
        return;
    }

    s32 linkIndex = pArticulatedCollider->GetLinkFromComponent(iFragChild);
    if(linkIndex > 0)
    {
        phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
        if (pJoint)
        {
            pJoint->SetDriveState(phJoint::DRIVE_STATE_FREE);
        }
    }
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadget::DriveHiearchyIdToAngle(eHierarchyId nId, CVehicle* pParent, float fDesiredAngle, float fDesiredSpeed, float& fCurrentAngleOut)
{
	fCurrentAngleOut = 0.0f;

	// Look up frag child from hierarchy id
	s32 iBoneIndex = pParent->GetBoneIndex(nId);
	
	if(iBoneIndex == -1)
	{
		return;
	}

	if(!vehicleVerifyf(pParent->GetVehicleFragInst(),"Vehicle has no frag inst!"))
	{
		return;
	}

	if(!vehicleVerifyf(pParent->GetVehicleFragInst()->GetCacheEntry(),"Vehicle is not in fragment cache"))
	{
		return;
	}

	s32 iChildIndex = pParent->GetVehicleFragInst()->GetType()->GetComponentFromBoneIndex(0,iBoneIndex);

	DriveComponentToAngle(pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst(),iChildIndex,fDesiredAngle,fDesiredSpeed,fCurrentAngleOut);
}

//////////////////////////////////////////////////////////////////////////

float CVehicleGadget::GetJointAngle(fragHierarchyInst* pHierInst, s32 iFragChild)
{
	Assert(pHierInst);
	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
	Assert(pArticulatedCollider);

	if(!pArticulatedCollider)
	{
		return 0.0f;
	}

	s32 linkIndex = pArticulatedCollider->GetLinkFromComponent(iFragChild);
	if(linkIndex > 0)
	{
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);

		phJoint1Dof* p1DofJoint = NULL;

		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

		if(p1DofJoint)
		{
			return p1DofJoint->GetAngle(pHierInst->body);
		}
	}

	return 0.0f;
}


//////////////////////////////////////////////////////////////////////////
// CVehicleWeaponMgr
CVehicleWeaponMgr::CVehicleWeaponMgr()
{
	for(s32 i = 0; i < MAX_NUM_VEHICLE_TURRETS; i++)
	{
		m_pTurrets[i] = NULL;
		m_TurretSeatIndices[i] = -1;
		m_TurretWeaponIndices[i] = -1;
	}
	for(s32 i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
	{
		m_pVehicleWeapons[i] = NULL;
		m_WeaponSeatIndices[i] = -1;
	}

	m_iNumVehicleWeapons = 0;
	m_iNumTurrets = 0;
}

CVehicleWeaponMgr::~CVehicleWeaponMgr()
{
	for(s32 i = 0; i < MAX_NUM_VEHICLE_TURRETS; i++)
	{
		if(m_pTurrets[i])
		{
			delete m_pTurrets[i];
			m_pTurrets[i] = NULL;
		}
	}
	for(s32 i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
	{
		if(m_pVehicleWeapons[i])
		{
			delete m_pVehicleWeapons[i];
			m_pVehicleWeapons[i] = NULL;
		}
	}
}

CVehicleWeaponBattery* CVehicleWeaponMgr::GetWeaponBatteryOfType(eFireType fireType)
{
	for(int i = 0; i < GetNumVehicleWeapons(); i++)
	{
		CVehicleWeapon* pVehicleWeapon = GetVehicleWeapon(i);

		if(pVehicleWeapon->GetType() == VGT_VEHICLE_WEAPON_BATTERY)
		{
			CVehicleWeaponBattery* pBattery = static_cast<CVehicleWeaponBattery*>(pVehicleWeapon);

			if(pBattery->GetWeaponInfo()->GetFireType() == fireType)
			{
				return pBattery;
			}
		}
	}

	return NULL;
}

void CVehicleWeaponMgr::Fire(const CPed* pFiringPed, CVehicle* pParent, s32 iWeaponIndex)
{
	if(aiVerifyf(m_pVehicleWeapons[iWeaponIndex],"Invalid weapon index"))
	{
		m_pVehicleWeapons[iWeaponIndex]->Fire(pFiringPed, pParent, VEC3_ZERO);
	}
}

void CVehicleWeaponMgr::ProcessControl(CVehicle* pParent)
{
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		Assert(m_pVehicleWeapons[iWeaponIndex]);
		if( m_pVehicleWeapons[iWeaponIndex] && m_pVehicleWeapons[iWeaponIndex]->GetIsEnabled() )
		{
			m_pVehicleWeapons[iWeaponIndex]->ProcessControl(pParent);
		}
	}

	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		Assert(m_pTurrets[iTurretIndex]);
		if( m_pTurrets[iTurretIndex] && m_pTurrets[iTurretIndex]->GetIsEnabled() )
		{
			m_pTurrets[iTurretIndex]->ProcessControl(pParent);
		}
	}
}

void CVehicleWeaponMgr::ProcessPreRender(CVehicle* pParent)
{
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		Assert(m_pVehicleWeapons[iWeaponIndex]);
		if( m_pVehicleWeapons[iWeaponIndex] && m_pVehicleWeapons[iWeaponIndex]->GetIsEnabled() )
		{
			m_pVehicleWeapons[iWeaponIndex]->ProcessPreRender(pParent);
		}
	}

	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		Assert(m_pTurrets[iTurretIndex]);
		if( m_pTurrets[iTurretIndex] && m_pTurrets[iTurretIndex]->GetIsEnabled() )
		{
			m_pTurrets[iTurretIndex]->ProcessPreRender(pParent);
		}
	}
}

void CVehicleWeaponMgr::ProcessPostPreRender(CVehicle* pParent)
{
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		Assert(m_pVehicleWeapons[iWeaponIndex]);
		if( m_pVehicleWeapons[iWeaponIndex] && m_pVehicleWeapons[iWeaponIndex]->GetIsEnabled() )
		{
			m_pVehicleWeapons[iWeaponIndex]->ProcessPostPreRender(pParent);
		}
	}

	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		Assert(m_pTurrets[iTurretIndex]);
		if( m_pTurrets[iTurretIndex] && m_pTurrets[iTurretIndex]->GetIsEnabled() )
		{
			m_pTurrets[iTurretIndex]->ProcessPostPreRender(pParent);
		}
	}
}

void CVehicleWeaponMgr::ProcessPhysics(CVehicle* pParent, float fTimeStep, int nTimeSlice)
{
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		Assert(m_pVehicleWeapons[iWeaponIndex]);
		if( m_pVehicleWeapons[iWeaponIndex] && m_pVehicleWeapons[iWeaponIndex]->GetIsEnabled() )
		{
			m_pVehicleWeapons[iWeaponIndex]->ProcessPhysics(pParent,fTimeStep,nTimeSlice);
		}
	}

	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		Assert(m_pTurrets[iTurretIndex]);
		if( m_pTurrets[iTurretIndex] && m_pTurrets[iTurretIndex]->GetIsEnabled() )
		{
			m_pTurrets[iTurretIndex]->ProcessPhysics(pParent,fTimeStep,nTimeSlice);
		}
	}
}

//
void CVehicleWeaponMgr::ProcessPreComputeImpacts(CVehicle* pVehicle, phCachedContactIterator &impacts)
{
	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		Assert(m_pTurrets[iTurretIndex]);
		if( m_pTurrets[iTurretIndex] && m_pTurrets[iTurretIndex]->GetIsEnabled() )
		{
			m_pTurrets[iTurretIndex]->ProcessPreComputeImpacts(pVehicle, impacts);
		}
	}
}

CVehicleWeaponMgr* CVehicleWeaponMgr::CreateVehicleWeaponMgr(CVehicle* pVehicle)
{
	CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();

	// Verify handling data is valid
	CHandlingData* pHandling = NULL;
	pHandling = CHandlingDataMgr::GetHandlingData(pVehicleModelInfo->GetHandlingId());
	Assertf(pHandling,"Vehicle %s has no handling data!",pVehicleModelInfo->GetGameName());
	if(pHandling == NULL)
	{
		return NULL;
	}

	CVehicleWeaponHandlingData* pWeaponHandlingData = pHandling->GetWeaponHandlingData();
	if(pWeaponHandlingData == NULL)
	{
		// There is no weapon data so don't create a weapon manager
		return NULL;
	}

	vehicleDebugf2("Creating VehicleWeaponMgr for vehicle model %s due to detected weapon handling data", pVehicleModelInfo->GetModelName());
	vehicleDebugf2("----- START ANALYSIS ----- ");

	const CVehicleStructure* pStructure = (pVehicleModelInfo != NULL) ? pVehicleModelInfo->GetStructure() : NULL;

	atFixedArray<CVehicleWeapon*, MAX_NUM_VEHICLE_WEAPONS> wepMgrWeapons;
	atFixedArray<CTurret*, MAX_NUM_VEHICLE_TURRETS> wepMgrTurrets;

	// Set up the weapon batteries
	// Weapons 1A -> 1D will all belong to one WeaponBattery
	// so they will fire at the same time
	// Terminology can be confusing here
	// A vehicle can have 6 main weapons (MAX_NUM_VEHICLE_WEAPONS)
	// e.g. a rocket launcher and machine gun
	// but machine gun could actually be a battery of up to 4 bones
	{
		const s32 iBatteryBoneStart[MAX_NUM_VEHICLE_WEAPONS] = 
		{
			VEH_WEAPON_1_FIRST,
			VEH_WEAPON_2_FIRST,
			VEH_WEAPON_3_FIRST,
			VEH_WEAPON_4_FIRST,
			VEH_WEAPON_5_FIRST,
			VEH_WEAPON_6_FIRST
		};
		const s32 iBatteryBoneEnd[MAX_NUM_VEHICLE_WEAPONS] =
		{
			VEH_WEAPON_1_LAST,
			VEH_WEAPON_2_LAST,
			VEH_WEAPON_3_LAST,
			VEH_WEAPON_4_LAST,
			VEH_WEAPON_5_LAST,
			VEH_WEAPON_6_LAST
		};
		CompileTimeAssert(MAX_NUM_VEHICLE_WEAPONS == 6);		// If this changes then update arrays above

#if __ASSERT
		// Check we won't end up with too many weapons
		for(s32 i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
		{
			const s32 iPossibleNumWeaponsInBattery = iBatteryBoneEnd[i] - iBatteryBoneStart[i];
			Assert(iPossibleNumWeaponsInBattery < MAX_NUM_BATTERY_WEAPONS);
		}
#endif

		for(s32 i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
		{
			// For each weapon TYPE we might have lots of fixed weapons
			// e.g. 4x miniguns on a helicopter
			// we want the battery to count as the main vehicle weapon
			// but need to create fixedvehicleweapons for each minigun
			CVehicleWeapon* pNewVehicleWeapons[MAX_NUM_BATTERY_WEAPONS];

			// Remember to 0 this variable
			s32 iNumWeaponsFound = 0;

			// Get the weapon hash from the handling data			
			u32 uWeaponHash = pWeaponHandlingData->GetWeaponHash(i);

#if __BANK
			const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(CWeaponInfoManager::GetInfo(uWeaponHash ASSERT_ONLY(, true)));
			vehicleDebugf2("Weapon Slot %i: %s (%u)", i, pWeaponInfo ? pWeaponInfo->GetName() : "null", uWeaponHash);
#endif // __BANK

			// Each weapon slot can have a vehicle modification type assigned to it (probably VMT_ROOF for most turreted vehicles)
			// We're going to use that to cross-check and disable all other weapons assigned to this type apart from the correct one

			bool bShouldCreateWeapon = false;
			eVehicleModType uWeaponVehicleModType = pWeaponHandlingData->GetWeaponModType(i);

			bool bValidModTypeForVehicleWeapon = IsValidModTypeForVehicleWeapon(uWeaponVehicleModType);
			if (uWeaponVehicleModType != VMT_INVALID && !bValidModTypeForVehicleWeapon)
			{
				vehicleAssertf(0, "Vehicle mod type %i is not supported for vehicle weapon slots (CHASSIS / ROOF / BUMPER_F / BUMPER_R / SPOILER / WING_R / CHASSIS5 / TRUNK)", (s32)uWeaponVehicleModType);
			}

			if (uWeaponVehicleModType != VMT_INVALID && bValidModTypeForVehicleWeapon)
			{				
				vehicleDebugf2("\tWeapon slot has mod type %i assigned, so doing extra validation checks", (s32)uWeaponVehicleModType);

				// Check to see if we have a mod of this type attached to the vehicle
				CVehicleVariationInstance& variation = pVehicle->GetVehicleDrawHandler().GetVariationInstance();
				u8 modIndex = variation.GetModIndex(uWeaponVehicleModType);

				// If we're freshly spawned and don't have a kit assigned to this vehicle yet, assign the default kit (so we can scan over the required data later on)
				u16 kitIndex = variation.GetKitIndex();
				if (kitIndex == INVALID_VEHICLE_KIT_INDEX)
				{
					u32 numModKits = pVehicleModelInfo->GetNumModKits();
					if (numModKits > 0)
					{
						variation.SetKitIndex(pVehicleModelInfo->GetModKit(0));
					}
					else
					{
						vehicleAssertf(0, "We're doing vehicle modification processing for CreateVehicleWeaponMgr, but the vehicle has no valid mod kits?");
					}
				}

				// If we do have a mod of this type attached, check to see if any of the weapon slot indexes in that mod data matches the weapon slot we're currently processing
				// If they do, we've found the mod that matches the weapon slot, so create it.
				if (modIndex != INVALID_MOD)
				{
					const CVehicleKit* kit = variation.GetKit();
					if (kit)
					{
						const CVehicleModVisible& mod = kit->GetVisibleMods()[modIndex];
						if (mod.GetType() == uWeaponVehicleModType)
						{
							s8 iWeaponSlot = mod.GetWeaponSlot();
							if (iWeaponSlot > -1 && i == iWeaponSlot)
							{	
								bShouldCreateWeapon = true;
							}
							else
							{
								s8 iWeaponSlot2 = mod.GetWeaponSlotSecondary();
								if (iWeaponSlot2 > -1 && i == iWeaponSlot2)
								{	
									bShouldCreateWeapon = true;
								}
							}
							
							if (bShouldCreateWeapon)
							{
								vehicleDebugf2("\tWe found a vehicle mod associated with the type of this weapon slot, and it's the weapon index we want. Creating!");				
							}
						}
					}
					
					// If it didn't match (or there was a problem), we skip it. This is probably because either:  
					// - This is a normal mod of this type, not paired with any specific weapon slot
					// - This vehicle mod is paired with a weapon slot, but it's not the same one that we're processing right now
					if (!bShouldCreateWeapon)
					{
						vehicleDebugf2("\tWe found a vehicle mod associated with the type of this weapon slot, but it doesn't match the index we want. Skipping.");
					}
				}
				// If we don't have a mod of this type attached (but we did define a vehicle mod type to match this weapon slot), this is a weapon that's part of the default.
				// We need to scan through all potential mods of this type of this kit, and if a non-attached mod is paired with the current weapon slot being processed, we need to skip it (as to only leave the default).
				// If we didn't find any matches to this weapon slot in potential mods, that means this is the weapon that matches the default, so we create it.	
				else
				{
					bShouldCreateWeapon = true;
					const CVehicleKit* kit = variation.GetKit();
					if (kit)
					{
						for (s32 j = 0; j < kit->GetVisibleMods().GetCount(); ++j)
						{
							if (kit->GetVisibleMods()[j].GetType() == uWeaponVehicleModType)        
							{
								const CVehicleModVisible& mod = kit->GetVisibleMods()[j];
								s8 iWeaponSlot = mod.GetWeaponSlot();
								if (iWeaponSlot > -1 && i == iWeaponSlot)
								{
									bShouldCreateWeapon = false;
									break;
								}
								else
								{
									s8 iWeaponSlot2 = mod.GetWeaponSlotSecondary();
									if (iWeaponSlot2 > -1 && i == iWeaponSlot2)
									{	
										bShouldCreateWeapon = false;
										break;
									}
								}
							}
						}

						if (!bShouldCreateWeapon)
						{
							vehicleDebugf2("\tVehicle has no attached mods matching the type of this weapon slot, but an unattached mod exists matching this index. Skipping.");
						}
					}
				}
			}
			// If we didn't assign a vehicle modification type to this weapon slot, let's just continue like normal weapons.
			else
			{
				bShouldCreateWeapon = true;
			}
			
			if (bShouldCreateWeapon)
			{
				// Go through all weapon bones for this battery
				s32 iCurrentBone = iBatteryBoneStart[i];
				while(iCurrentBone <= iBatteryBoneEnd[i])
				{
					if(pStructure && pStructure->m_nBoneIndices[iCurrentBone] > VEH_INVALID_ID)
					{
						// Checks we only want to do on the first bone that skips the entire weapon slot if invalid
						if(iCurrentBone == iBatteryBoneStart[i])
						{
							if(uWeaponHash == 0)
							{
								vehicleDebugf2("\tWeapon bones exist on the vehicle, but no weapon info is assigned. Skipping weapon slot.");
								break;
							}
						}

						// Make extra sure we aren't about to overrun an array
						vehicleAssert(iNumWeaponsFound < MAX_NUM_BATTERY_WEAPONS);

						// Create the weapon
						static const atHashWithStringNotFinal searchLightWeaponName("VEHICLE_WEAPON_SEARCHLIGHT",0xCDAC517D);
						static const atHashWithStringNotFinal waterCannonWeaponName("VEHICLE_WEAPON_WATER_CANNON",0x67D18297);
						static const atHashWithStringNotFinal radarWeaponName("VEHICLE_WEAPON_RADAR",0xD276317E);

						if (uWeaponHash == searchLightWeaponName)
						{
							pNewVehicleWeapons[iNumWeaponsFound] = rage_new CSearchLight(uWeaponHash, (eHierarchyId)iBatteryBoneStart[i]);
						}
						else if(uWeaponHash == radarWeaponName)
						{
							pNewVehicleWeapons[iNumWeaponsFound] = rage_new CVehicleRadar(uWeaponHash, (eHierarchyId)iBatteryBoneStart[i]);
						}
						else if (uWeaponHash == waterCannonWeaponName)
						{
							pNewVehicleWeapons[iNumWeaponsFound] = rage_new CVehicleWaterCannon(uWeaponHash, (eHierarchyId)iBatteryBoneStart[i]);
						}
						else
						{
							if(!(vehicleVerifyf(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash) != NULL, "Weapon info assigned to vehicle weapon slot %i is invalid!", i)))
							{
								break;
							}

							pNewVehicleWeapons[iNumWeaponsFound] = rage_new CFixedVehicleWeapon(uWeaponHash, (eHierarchyId)iCurrentBone);
						}
						iNumWeaponsFound++;
					}
					iCurrentBone++;
				}

				// Now create the weapon battery, 
				// or if there is just one weapon bone for this index then just make a single weapon

				if(iNumWeaponsFound > 1)
				{
					vehicleDebugf2("\t%i weapon bones found, creating CVehicleWeaponBattery container", iNumWeaponsFound);

					// More than one weapon for this battery, so need to actually make a CWeaponBattery to hold them all
					CVehicleWeaponBattery* pBattery = rage_new CVehicleWeaponBattery();
					pBattery->m_handlingIndex = i;

					// Add the fixed weapons to the battery
					for(s32 iWeaponIndex = 0; iWeaponIndex < iNumWeaponsFound; iWeaponIndex++)
					{
						Assert(pNewVehicleWeapons[iWeaponIndex]);
						pBattery->AddVehicleWeapon(pNewVehicleWeapons[iWeaponIndex]);
					}

					wepMgrWeapons.Push(pBattery);
				}
				else if(iNumWeaponsFound == 1)
				{
					vehicleDebugf2("\t1 weapon bone found, creating lone CFixedVehicleWeapon");

					// Don't need weapon battery class because we only have one weapon bone here
					// e.g. there is only a weapon_1a but not weapon_1b, weapon_1c etc.
					Assert(pNewVehicleWeapons[0]);
					pNewVehicleWeapons[0]->m_handlingIndex = i;
					wepMgrWeapons.Push(pNewVehicleWeapons[0]);
				}
				else
				{
					vehicleDebugf2("\tNo valid weapon bones found for this slot, skipping creation");
				}		
			}
		}
	}

	// Now look for turrets and set them up
	if(pStructure)
	{
		const s32 iTurretBaseBone[MAX_NUM_VEHICLE_TURRETS] = 
		{
			VEH_TURRET_1_BASE,
			VEH_TURRET_2_BASE,
			VEH_TURRET_3_BASE,
			VEH_TURRET_4_BASE,
			VEH_TURRET_A1_BASE,
			VEH_TURRET_A2_BASE,
			VEH_TURRET_A3_BASE,
			VEH_TURRET_A4_BASE,
			VEH_TURRET_B1_BASE,
			VEH_TURRET_B2_BASE,
			VEH_TURRET_B3_BASE,
			VEH_TURRET_B4_BASE
		};
		const s32 iTurretBarrelBone[MAX_NUM_VEHICLE_TURRETS] = 
		{
			VEH_TURRET_1_BARREL,
			VEH_TURRET_2_BARREL,
			VEH_TURRET_3_BARREL,
			VEH_TURRET_4_BARREL,
			VEH_TURRET_A1_BARREL,
			VEH_TURRET_A2_BARREL,
			VEH_TURRET_A3_BARREL,
			VEH_TURRET_A4_BARREL,
			VEH_TURRET_B1_BARREL,
			VEH_TURRET_B2_BARREL,
			VEH_TURRET_B3_BARREL,
			VEH_TURRET_B4_BARREL
		};
		const s32 iTurretModBone[MAX_NUM_VEHICLE_TURRETS] = 
		{
			VEH_TURRET_1_MOD,
			VEH_TURRET_2_MOD,
			VEH_TURRET_3_MOD,
			VEH_TURRET_4_MOD,
			VEH_TURRET_A1_MOD,
			VEH_TURRET_A2_MOD,
			VEH_TURRET_A3_MOD,
			VEH_TURRET_A4_MOD,
			VEH_TURRET_B1_MOD,
			VEH_TURRET_B2_MOD,
			VEH_TURRET_B3_MOD,
			VEH_TURRET_B4_MOD
		};
		CompileTimeAssert(MAX_NUM_VEHICLE_TURRETS == 12);		// If this changes then update arrays above

		for(s32 iTurretIndex = 0; iTurretIndex < MAX_NUM_VEHICLE_TURRETS; iTurretIndex++)
		{
			vehicleDebugf2("Turret Slot %i: ", iTurretIndex);
			const s32 iBaseBoneIndex = pStructure->m_nBoneIndices[iTurretBaseBone[iTurretIndex]];
			const s32 iBarrelBoneIndex = pStructure->m_nBoneIndices[iTurretBarrelBone[iTurretIndex]];
			const s32 iModBoneIndex = pStructure->m_nBoneIndices[iTurretModBone[iTurretIndex]];

			// If the vehicle has a turret mod bone, see if we've disabled the collision previously and skip
			if(iModBoneIndex > -1)
			{
				fragInst* pFragInst = pVehicle->GetVehicleFragInst();
				vehicleAssertf(pFragInst, "Vehicle '%s' has no frag inst!", pVehicle->GetModelName());
				phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());

				int group = pFragInst->GetGroupFromBoneIndex( iModBoneIndex );
				if( group > -1 )
				{
					fragPhysicsLOD* physicsLOD = pFragInst->GetTypePhysics();
					fragTypeGroup* pGroup = physicsLOD->GetGroup(group);
					int child = pGroup->GetChildFragmentIndex();

					if(pBoundComp->GetIncludeFlags(child) == 0)
					{
						vehicleDebugf2("\tTurret mod bone collision has been disabled by physics code include flags, skipping creation");
						continue;
					}
				}
			}

			// Only make a turret if its got a base
			if(iBaseBoneIndex > -1)
			{		
				const float fTurretSpeed = pWeaponHandlingData->GetTurretSpeed(iTurretIndex);
				const float fTurretPitchMin = pWeaponHandlingData->GetTurretPitchMin(iTurretIndex);
				const float fTurretPitchMax = pWeaponHandlingData->GetTurretPitchMax(iTurretIndex);
				const float fTurretCamPitchMin = pWeaponHandlingData->GetTurretCamPitchMin(iTurretIndex);
				const float fTurretCamPitchMax = pWeaponHandlingData->GetTurretCamPitchMax(iTurretIndex);
				const float fBulletVelocityForGravity = pWeaponHandlingData->GetBulletVelocityForGravity(iTurretIndex);
				const float fTurretPitchForwardMin = pWeaponHandlingData->GetTurretPitchForwardMin(iTurretIndex);
				CVehicleWeaponHandlingData::sTurretPitchLimits sPitchLimits;
				sPitchLimits.m_fForwardAngleMin = pWeaponHandlingData->GetPitchForwardAngleMin(iTurretIndex);
				sPitchLimits.m_fForwardAngleMax = pWeaponHandlingData->GetPitchForwardAngleMax(iTurretIndex);
				sPitchLimits.m_fBackwardAngleMin = pWeaponHandlingData->GetPitchBackwardAngleMin(iTurretIndex);
				sPitchLimits.m_fBackwardAngleMid = pWeaponHandlingData->GetPitchBackwardAngleMid(iTurretIndex);
				sPitchLimits.m_fBackwardAngleMax = pWeaponHandlingData->GetPitchBackwardAngleMax(iTurretIndex);
				sPitchLimits.m_fBackwardForcedPitch = pWeaponHandlingData->GetPitchBackwardForcedPitch(iTurretIndex);

				CTurret* pNewTurret = NULL;

				// See if the turret is physical
				if(pVehicleModelInfo->GetFragType())
				{
					s32 iBaseFragChild = (s8)pVehicleModelInfo->GetFragType()->GetComponentFromBoneIndex(0, iBaseBoneIndex);
					if(iBaseFragChild > -1)
					{
						vehicleDebugf2("\tPhysical base bone found, creating CTurretPhysical");

						// The base is physical so make a physical turret
						pNewTurret = rage_new CTurretPhysical(
							(eHierarchyId)iTurretBaseBone[iTurretIndex],
							iBarrelBoneIndex > -1 ? (eHierarchyId)iTurretBarrelBone[iTurretIndex] : VEH_INVALID_ID,
							fTurretSpeed,
							fTurretPitchMin,
							fTurretPitchMax,
							fTurretCamPitchMin,
							fTurretCamPitchMax,
							fBulletVelocityForGravity,
							fTurretPitchForwardMin,
							sPitchLimits);
					}
				}
				
				if(pNewTurret == NULL)
				{
					vehicleDebugf2("\tNon-physical base bone found, creating CTurret");

					pNewTurret = rage_new CTurret(
						(eHierarchyId)iTurretBaseBone[iTurretIndex],
						iBarrelBoneIndex > -1 ? (eHierarchyId)iTurretBarrelBone[iTurretIndex] : VEH_INVALID_ID,
						fTurretSpeed,
						fTurretPitchMin,
						fTurretPitchMax,
						fTurretCamPitchMin,
						fTurretCamPitchMax,
						fBulletVelocityForGravity,
						fTurretPitchForwardMin,
						sPitchLimits);
				}
				
				Assert(pNewTurret);

				if(pNewTurret)
				{
					pNewTurret->InitJointLimits(pVehicleModelInfo);
					pNewTurret->InitAudio(pVehicle);
					wepMgrTurrets.Push(pNewTurret);
				}				
			}
			else
			{
				vehicleDebugf2("\tNo valid turret base bone found");
			}
		}
	}	

	// Now see if we actually made anything
	// if not don't bother making a weapon manager
	if(wepMgrWeapons.GetCount() == 0 && wepMgrTurrets.GetCount() == 0)
	{
		vehicleDebugf2("----- END - No Valid Weapons or Turrets Found, Skipping Creation ----- ");
		return NULL;
	}
	else
	{
		vehicleDebugf2("----- START CREATION ----- ");

		CVehicleWeaponMgr* pReturnMgr = rage_new CVehicleWeaponMgr();
		if(pReturnMgr)
		{
			const s32 iNumWeapons = wepMgrWeapons.GetCount();
			for(s32 iWeaponIndex = 0; iWeaponIndex < iNumWeapons; iWeaponIndex++)
			{
				vehicleDebugf2("Adding Weapon %i of %i:", iWeaponIndex + 1, iNumWeapons);

				Assert(wepMgrWeapons[iWeaponIndex]);	// If this is NULL something weird has happened and this function is broken
				s32 iHandlingIndex = wepMgrWeapons[iWeaponIndex]->m_handlingIndex;
				const bool bAdded = pReturnMgr->AddVehicleWeapon(wepMgrWeapons[iWeaponIndex], pWeaponHandlingData->GetWeaponSeat(iHandlingIndex));
				if(!bAdded && wepMgrWeapons[iWeaponIndex])
				{
					// Something went wrong so delete this weapon or we'll leak
					delete wepMgrWeapons[iWeaponIndex];
					wepMgrWeapons[iWeaponIndex] = NULL;
				}
			}

			const bool bIsAPC = pVehicle && MI_CAR_APC.IsValid() && pVehicle->GetModelIndex() == MI_CAR_APC;
			const bool bIsHunter = pVehicle && MI_HELI_HUNTER.IsValid() && pVehicle->GetModelIndex() == MI_HELI_HUNTER;
			const bool bIsAkula = pVehicle && MI_HELI_AKULA.IsValid() && pVehicle->GetModelIndex() == MI_HELI_AKULA;
			const bool bIsKhanjali = pVehicle && MI_TANK_KHANJALI.IsValid() && pVehicle->GetModelIndex() == MI_TANK_KHANJALI;
			const bool bIsMenacer = pVehicle && MI_CAR_MENACER.IsValid() && pVehicle->GetModelIndex() == MI_CAR_MENACER;

			//HACK: We need to hard-code some stuff for the APC. Need to get around the later assumption that turrets always come after non-turret weapons, but the missile battery *needs* to be in the second slot.
			const bool bTurretWeaponsBeforeNonTurretWeapons = bIsAPC || bIsHunter || bIsAkula || bIsKhanjali || bIsMenacer;

			const s32 iNumTurrets = wepMgrTurrets.GetCount();
			s32 iStartWeaponIndex = bTurretWeaponsBeforeNonTurretWeapons ? 0 : iNumWeapons - iNumTurrets;

			//HACK: Terrible hacks to get weapon/turret pairing properly on the HUNTER and AKULA (because the model is alternating between turrets and non-turret weapons in the indexing order...)
			if ((bIsHunter || bIsAkula) && iNumWeapons >= 2 && wepMgrWeapons[1]->m_handlingIndex == 2)
			{
				iStartWeaponIndex = 1;
			}

			if (iStartWeaponIndex < 0)
			{
				vehicleAssertf(0, "Vehicle %s has more turrets (%i) than weapons (%i), clamping iStartWeaponIndex. Please fix either model or metadata.", pVehicleModelInfo->GetModelName(), iNumTurrets, iNumWeapons);
				iStartWeaponIndex = 0;
			}

			for(s32 iTurretIndex = 0; iTurretIndex < iNumTurrets; iTurretIndex++)
			{
				vehicleDebugf2("Adding Turret %i of %i (Starting at Weapon Index %i):", iTurretIndex + 1, iNumTurrets, iStartWeaponIndex);

				Assert(wepMgrTurrets[iTurretIndex]);	// If this is NULL something weird has happened and this function is broken
				bool bAdded = false;

				// Compute the seat index for the turret starting from the last non-turret vehicle weapon (there is an assumption all the turrets come last here)
				const s32 iWeaponIndex = iStartWeaponIndex + iTurretIndex;
				if (iWeaponIndex >= 0 && iWeaponIndex < iNumWeapons)
				{
					s32 iHandlingIndex = wepMgrWeapons[iWeaponIndex]->m_handlingIndex;
					bAdded = pReturnMgr->AddTurret(wepMgrTurrets[iTurretIndex], pWeaponHandlingData->GetWeaponSeat(iHandlingIndex), iWeaponIndex);
				}
				
				if(!bAdded && wepMgrTurrets[iTurretIndex])
				{
					if (bIsKhanjali)
					{
						vehicleDebugf2("\tFailed? This is an exception for the KHANJALI without a VMT_CHASSIS mod, so cleaning up unnecessary turrets.");
					}

					// Something went wrong so delete this turret or we'll leak
					delete wepMgrTurrets[iTurretIndex];
					wepMgrTurrets[iTurretIndex] = NULL;
				}
			}		
		}

		vehicleDebugf2("----- END ----- ");
		return pReturnMgr;
	}
}

bool CVehicleWeaponMgr::AddVehicleWeapon( CVehicleWeapon* pVehicleWeapon, s32 seatIndex)
{
	AssertMsg(pVehicleWeapon, "Trying to add a NULL vehicle weapon");
	AssertMsg(m_iNumVehicleWeapons < MAX_NUM_VEHICLE_WEAPONS, "No room to add more vehicle weapons!");

	if(m_iNumVehicleWeapons < MAX_NUM_VEHICLE_WEAPONS && pVehicleWeapon)
	{
		AssertMsg(m_pVehicleWeapons[m_iNumVehicleWeapons]==NULL,"CVehicleWeaponMgr::AddVehicleWeapon: Expected NULL pointer");
		m_pVehicleWeapons[m_iNumVehicleWeapons] = pVehicleWeapon;
		m_WeaponSeatIndices[m_iNumVehicleWeapons] = seatIndex;
		vehicleDebugf2("\tSuccess! %s, Index %i, Seat %i, Handling Index %i", pVehicleWeapon->GetName(), m_iNumVehicleWeapons, seatIndex, pVehicleWeapon->m_handlingIndex);

		m_iNumVehicleWeapons++;
		return true;
	}

	vehicleDebugf2("\tFailed! Check the asserts.");
	return false;
}

s32 CVehicleWeaponMgr::GetVehicleWeaponIndexByHash(u32 iHash) const
{ 
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		if(Verifyf(m_pVehicleWeapons[iWeaponIndex],"Unexpected NULL vehicle weapon"))
		{
			if(iHash == m_pVehicleWeapons[iWeaponIndex]->GetHash())
			{
				return iWeaponIndex;
			}
		}
	}

	return -1;
}

void CVehicleWeaponMgr::DisableAllWeapons( CVehicle* pParent )
{
	for( s32 iWeaponIndex = 0; iWeaponIndex < m_iNumTurrets; iWeaponIndex++ )
	{
		if( Verifyf( m_pTurrets[iWeaponIndex], "CVehicleWeaponMgr::DisableAllWeapons - Unexpected NULL vehicle turret") )
		{
			m_pTurrets[ iWeaponIndex ]->SetEnabled( pParent, false );
		}
	}

	for( s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++ )
	{
		if( Verifyf( m_pVehicleWeapons[iWeaponIndex],"CVehicleWeaponMgr::DisableAllWeapons -Unexpected NULL vehicle weapon" ) )
		{
			m_pVehicleWeapons[ iWeaponIndex ]->SetEnabled( pParent, false );
		}
	}
}

void CVehicleWeaponMgr::DisableWeapon( bool bDisable,  CVehicle* pParent, int index )
{
	if( Verifyf( m_pTurrets[index], "CVehicleWeaponMgr::DisableWeapon - Unexpected NULL vehicle turret") )
	{
		m_pTurrets[ index ]->SetEnabled( pParent, bDisable );
	}
	
	if( Verifyf( m_pVehicleWeapons[index],"CVehicleWeaponMgr::DisableWeapon -Unexpected NULL vehicle weapon" ) )
	{
		m_pVehicleWeapons[ index ]->SetEnabled( pParent, bDisable );
	}
}



bool CVehicleWeaponMgr::AddTurret(CTurret* pTurret, s32 seatIndex, s32 weaponIndex)
{
	AssertMsg(pTurret, "Trying to add a NULL turret");
	AssertMsg(m_iNumTurrets < MAX_NUM_VEHICLE_WEAPONS, "No room to add more vehicle weapons!");
	if(m_iNumTurrets < MAX_NUM_VEHICLE_WEAPONS)
	{
		AssertMsg(m_pTurrets[m_iNumTurrets]==NULL,"CVehicleWeaponMgr::AddTurret: Expected NULL pointer");
		if (pTurret)
		{
			pTurret->SetAssociatedSeatIndex(seatIndex);
		}
		m_pTurrets[m_iNumTurrets] = pTurret;
		m_TurretSeatIndices[m_iNumTurrets] = seatIndex;
		m_TurretWeaponIndices[m_iNumTurrets] = weaponIndex;
		vehicleDebugf2("\tSuccess! Index %i, Seat %i, Weapon %i (%s)", m_iNumTurrets, seatIndex, weaponIndex, m_pVehicleWeapons[weaponIndex] ? m_pVehicleWeapons[weaponIndex]->GetName() : "");

		m_iNumTurrets++;
		return true;
	}

	vehicleDebugf2("\tFailed! Check the asserts.");
	return false;
}

void CVehicleWeaponMgr::GetWeaponsForSeat(s32 seatIndex, atArray<CVehicleWeapon*>& weaponOut)
{
	aiFatalAssertf(weaponOut.size() == 0,"Expected array size to be 0");

	// Q: Should we clear the arrays first?

	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		if(Verifyf(m_pVehicleWeapons[iWeaponIndex],"Unexpected NULL vehicle weapon"))
		{
			if(seatIndex == m_WeaponSeatIndices[iWeaponIndex])
			{
				weaponOut.Grow() = m_pVehicleWeapons[iWeaponIndex];
			}
		}
	}
}

void CVehicleWeaponMgr::GetWeaponsForSeat(s32 seatIndex, atArray<const CVehicleWeapon*>& weaponOut) const
{
	aiFatalAssertf(weaponOut.size() == 0,"Expected array size to be 0");

	// Q: Should we clear the arrays first?

	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		if(Verifyf(m_pVehicleWeapons[iWeaponIndex],"Unexpected NULL vehicle weapon"))
		{
			if(seatIndex == m_WeaponSeatIndices[iWeaponIndex])
			{
				weaponOut.Grow() = m_pVehicleWeapons[iWeaponIndex];
			}
		}
	}
}

void CVehicleWeaponMgr::GetWeaponsAndTurretsForSeat(s32 seatIndex, atArray<CVehicleWeapon*>& weaponOut, atArray<CTurret*>& turretOut)
{	
	aiFatalAssertf(turretOut.size() == 0,"Expected array size to be 0");

	// Q: Should we clear the arrays first?

	GetWeaponsForSeat(seatIndex, weaponOut);

	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if(Verifyf(m_pTurrets[iTurretIndex],"Unexpected NULL turret"))
		{
			if(seatIndex == m_TurretSeatIndices[iTurretIndex])
			{
				turretOut.Grow() = m_pTurrets[iTurretIndex];
			}
		}
	}
}

void CVehicleWeaponMgr::GetWeaponsAndTurretsForSeat(s32 seatIndex, atArray<const CVehicleWeapon*>& weaponOut, atArray<const CTurret*>& turretOut) const
{	
	aiFatalAssertf(turretOut.size() == 0,"Expected array size to be 0");

	// Q: Should we clear the arrays first?

	GetWeaponsForSeat(seatIndex, weaponOut);

	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if(Verifyf(m_pTurrets[iTurretIndex],"Unexpected NULL vehicle weapon"))
		{
			if(seatIndex == m_TurretSeatIndices[iTurretIndex])
			{
				turretOut.Grow() = m_pTurrets[iTurretIndex];
			}
		}
	}
}

CTurret* CVehicleWeaponMgr::GetFirstTurretForSeat(s32 seatIndex) const
{
	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if(Verifyf(m_pTurrets[iTurretIndex],"Unexpected NULL vehicle weapon"))
		{
			if(seatIndex == m_TurretSeatIndices[iTurretIndex])
			{
				return m_pTurrets[iTurretIndex];
			}
		}
	}
	return NULL;
}

CFixedVehicleWeapon* CVehicleWeaponMgr::GetFirstFixedWeaponForSeat(s32 seatIndex) const
{
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		if(Verifyf(m_pVehicleWeapons[iWeaponIndex],"Unexpected NULL vehicle weapon") && m_pVehicleWeapons[iWeaponIndex]->GetType() == VGT_FIXED_VEHICLE_WEAPON)
		{
			if(seatIndex == m_WeaponSeatIndices[iWeaponIndex])
			{
				return static_cast<CFixedVehicleWeapon*>(m_pVehicleWeapons[iWeaponIndex]);
			}
		}
	}
	return NULL;
}

s32 CVehicleWeaponMgr::GetNumWeaponsForSeat(s32 seatIndex) const
{
	s32 iNumWeapons = 0;
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		if(Verifyf(m_pVehicleWeapons[iWeaponIndex],"Unexpected NULL vehicle weapon"))
		{
			if(seatIndex == m_WeaponSeatIndices[iWeaponIndex])
			{
				iNumWeapons++;
			}
		}
	}
	return iNumWeapons;
}

s32 CVehicleWeaponMgr::GetNumTurretsForSeat(s32 seatIndex) const 
{
	s32 iNumTurrets = 0;
	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if(Verifyf(m_pTurrets[iTurretIndex],"Unexpected NULL vehicle weapon"))
		{
			if(seatIndex == m_TurretSeatIndices[iTurretIndex])
			{
				iNumTurrets++;
			}
		}
	}
	return iNumTurrets;
}

s32 CVehicleWeaponMgr::GetSeatIndexForTurret(const CTurret& rTurret) const 
{ 
	for (s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if (m_pTurrets[iTurretIndex] == &rTurret)
		{
			return m_TurretSeatIndices[iTurretIndex];
		}
	}
	return -1;
}

s32 CVehicleWeaponMgr::GetWeaponIndexForTurret(const CTurret& rTurret) const
{
	for (s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if (m_pTurrets[iTurretIndex] == &rTurret)
		{
			return m_TurretWeaponIndices[iTurretIndex];
		}
	}
	return -1;
}

#if GTA_REPLAY
s32 CVehicleWeaponMgr::GetSeatIndexFromWeaponIdx(s32 weaponIndex) const
{
	Assertf(weaponIndex >= 0 && weaponIndex < MAX_NUM_VEHICLE_WEAPONS, "CVehicleWeaponMgr::GetSeatIndexFromWeaponIdx invalid index");
	return m_WeaponSeatIndices[weaponIndex];
}
#endif 

void CVehicleWeaponMgr::AimTurretsAtLocalDir(const Vector3 &vLocalDir, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim)
{
	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if(m_pTurrets[iTurretIndex])
		{
			m_pTurrets[iTurretIndex]->AimAtLocalDir(vLocalDir, pParent, bDoCamCheck, bSnapAim);
		}
	}
}

void CVehicleWeaponMgr::AimTurretsAtWorldPos(const Vector3 &vWorldPos, CVehicle* pParent, const bool bDoCamCheck, const bool bSnapAim)
{
	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		if(m_pTurrets[iTurretIndex])
		{
			m_pTurrets[iTurretIndex]->AimAtWorldPos(vWorldPos,pParent, bDoCamCheck, bSnapAim);
		}
	}
}

bool CVehicleWeaponMgr::WantsToBeAwake(CVehicle* pParent)
{
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		Assert(m_pVehicleWeapons[iWeaponIndex]);
		if(m_pVehicleWeapons[iWeaponIndex])
		{
			if(m_pVehicleWeapons[iWeaponIndex]->WantsToBeAwake(pParent))
			{
				return true;
			}
		}
	}

	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		Assert(m_pTurrets[iTurretIndex]);
		if(m_pTurrets[iTurretIndex])
		{
			if(m_pTurrets[iTurretIndex]->WantsToBeAwake(pParent))
			{
				return true;
			}
		}
	}

	return false;
}

void CVehicleWeaponMgr::ResetIncludeFlags(CVehicle* pParent)
{
	for(s32 iTurretIndex = 0; iTurretIndex < m_iNumTurrets; iTurretIndex++)
	{
		Assert(m_pTurrets[iTurretIndex]);
		if(m_pTurrets[iTurretIndex])
		{		
			m_pTurrets[iTurretIndex]->ResetIncludeFlags(pParent);
		}
	}

	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		Assert(m_pVehicleWeapons[iWeaponIndex]);
		if(m_pVehicleWeapons[iWeaponIndex])
		{
			m_pVehicleWeapons[iWeaponIndex]->ResetIncludeFlags(pParent);
		}
	}
}

s32 CVehicleWeaponMgr::GetIndexOfVehicleWeapon(const CVehicleWeapon* pVehicleWeapon) const
{
	for(s32 iWeaponIndex = 0; iWeaponIndex < m_iNumVehicleWeapons; iWeaponIndex++)
	{
		if (GetVehicleWeapon(iWeaponIndex) == pVehicleWeapon)
		{
			return iWeaponIndex;
		}
	}
	return -1;
}

bool CVehicleWeaponMgr::IsValidModTypeForVehicleWeapon(eVehicleModType modType)
{
	return modType == VMT_CHASSIS || modType == VMT_ROOF || modType == VMT_BUMPER_F || modType == VMT_BUMPER_R || modType == VMT_SPOILER || modType == VMT_WING_R || modType == VMT_CHASSIS5 || modType == VMT_TRUNK;
}

//////////////////////////////////////////////////////////////////////////
// Class CVehicleTrailerAttachPoint

CVehicleTrailerAttachPoint::CVehicleTrailerAttachPoint(s32 iAttachBoneIndex)
{
	Assert(iAttachBoneIndex > -1);
	m_iAttachBone = iAttachBoneIndex;
	m_pTrailer = NULL;
	m_uIgnoreAttachTimer = 0;
}

bank_float CVehicleTrailerAttachPoint::ms_fAttachSearchRadius = 1.0f;	// Until collision is sorted on trailers
bank_float CVehicleTrailerAttachPoint::ms_fAttachRadius = 1.0f;
bank_u32 CVehicleTrailerAttachPoint::ms_uAttachResetTime = 3000;
dev_bool sbTestWarp = false;
void CVehicleTrailerAttachPoint::ProcessControl(CVehicle* pParent)
{
	Assert(pParent);
	Assert(m_iAttachBone > -1);

	if(m_pTrailer)
	{
		if(m_pTrailer->GetAttachParent() == pParent)
		{
			CVehicle* trailer = SafeCast(CVehicle, m_pTrailer.Get());
			bool lockedPersonalVehicle = NetworkInterface::IsVehicleLockedForPlayer(trailer, pParent->GetDriver()) && trailer->IsPersonalVehicle();
			if(pParent->GetDriver() && !trailer->IsNetworkClone() && lockedPersonalVehicle)
			{
				DetachTrailer(pParent);
			}
			return;
		}
		else
		{
			// last trailer has detached itself, so start scanning again
			m_pTrailer = NULL;
			m_uIgnoreAttachTimer = ms_uAttachResetTime;
		}		
	}

	// Only scan for trailer pickup if the driver is a player
	if(!pParent->m_nVehicleFlags.bScansWithNonPlayerDriver && (pParent->GetDriver() == NULL || !pParent->GetDriver()->IsAPlayerPed()))
	{
		return;
	}

	if(m_uIgnoreAttachTimer > 0)
	{
		u32 uTimeStep = fwTimer::GetTimeStepInMilliseconds();

		// Be careful not to wrap round
		m_uIgnoreAttachTimer = uTimeStep > m_uIgnoreAttachTimer ? 0 : m_uIgnoreAttachTimer - uTimeStep;

		return;
	}

	// If parent is asleep, skip test
	// We would be awake if anything was colliding with us or if we were moving
	static dev_float sfTimeToCheckForTrailersWhenAsleepAfterCreation = 1000.0f;// The mission creator expects trucks to hook up with traielrs when first placed, even if they are asleep.
	if(pParent->IsAsleep() && !(pParent->m_nVehicleFlags.bScansWithNonPlayerDriver && CTimeHelpers::GetTimeSince(pParent->m_TimeOfCreation) < sfTimeToCheckForTrailersWhenAsleepAfterCreation))
	{
		return;
	}

	// Lookup attach bone position
	Matrix34 matLocalBone;
    Matrix34 matGlobalBone;

	// NOTE: GetLocalMtx returns the local matrix of the bone relative to its parent. NOT the bone's matrix relative to the vehicle.
	//       It just happens to work out because the parent bone of the attach bones must be the root. 
    matLocalBone = pParent->GetLocalMtx(m_iAttachBone);
    Matrix34 m = MAT34V_TO_MATRIX34(pParent->GetMatrix());
    matGlobalBone.Dot(matLocalBone, m);

#if __DEV
// Render the test
	if(CTrailer::ms_bDebugDrawTrailers)
	{
		grcDebugDraw::Sphere(matGlobalBone.d, ms_fAttachSearchRadius, Color_green, false);
	}
#endif

	// Search through the level for hits
	phIterator levelIterator(phIterator::PHITERATORLOCKTYPE_READLOCK);
	levelIterator.InitCull_Sphere(matGlobalBone.d, ms_fAttachSearchRadius);
	levelIterator.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
	levelIterator.SetStateIncludeFlags(phLevelNew::STATE_FLAG_ACTIVE | phLevelNew::STATE_FLAG_INACTIVE);
	for(u16 levelIndex = CPhysics::GetLevel()->GetFirstCulledObject(levelIterator); levelIndex != phInst::INVALID_INDEX; levelIndex = CPhysics::GetLevel()->GetNextCulledObject(levelIterator))
	{
		// Don't think this check is necessary
		phInst* pInst = CPhysics::GetLevel()->GetInstance(levelIndex);
		if(CEntity* pEntity = CPhysics::GetEntityFromInst(pInst))
		{
			if(pEntity->GetIsTypeVehicle() && static_cast<CVehicle*>(pEntity)->InheritsFromTrailer())
			{
				// Lookup the attach point of the trailer, and make sure its close to our attach point
				CTrailer* pTrailer = static_cast<CTrailer*>(pEntity);

				if( !pTrailer->GetLocalPlayerCanAttach() &&
					pParent->GetDriver() &&
					pParent->GetDriver()->IsLocalPlayer() )
				{
					continue;
				}

				// Lookup attach point
				s32 iAttachIndex = pTrailer->GetBoneIndex(TRAILER_ATTACH);

				Assertf(iAttachIndex > -1 || pTrailer->GetModelIndex() == MI_PROP_TRAILER,"Trailer is missing attach point %s", pTrailer->GetVehicleModelInfo()->GetModelName());

				bool lockedPersonalVehicle = NetworkInterface::IsVehicleLockedForPlayer(pTrailer, pParent->GetDriver()) && pTrailer->IsPersonalVehicle();
			
				bool isGhostedVehicleCheck = false;
				const CVehicle* currentParent = static_cast<const CVehicle*>(pTrailer->GetAttachParent());

				if ((NetworkInterface::IsAGhostVehicle(*pParent) || NetworkInterface::IsAGhostVehicle(*pTrailer)) && currentParent && currentParent != pParent)
				{
					gnetDebug3("CVehicleTrailerAttachPoint::ProcessControl Preventing attach due to ghost check");
					isGhostedVehicleCheck = true;
				}

				if(iAttachIndex > -1 && 
					pTrailer->m_nVehicleFlags.bAutomaticallyAttaches && 
					!pTrailer->IsUpsideDown() && 
					!pTrailer->IsOnItsSide() && 
					pParent->GetVehicleModelInfo()->GetIsCompatibleTrailer(pTrailer->GetVehicleModelInfo()) && 
					!lockedPersonalVehicle &&
					!NetworkInterface::IsInMPCutscene() &&
					!isGhostedVehicleCheck) //check whether this vehicle should be able to automatically attach
				{
					// Let the network code attach remote objects
					if(pTrailer->IsNetworkClone())
					{
						if(!NetworkUtils::IsNetworkCloneOrMigrating(pParent))
						{
							CRequestControlEvent::Trigger(pTrailer->GetNetworkObject() NOTFINAL_ONLY(, "CVehicleTrailerAttachPoint::ProcessControl"));
						}
						continue;
					}

					Matrix34 matLocalAttach;
                    Matrix34 matGlobalAttach; 

                    matLocalAttach = pTrailer->GetLocalMtx(iAttachIndex);
                    Matrix34 matTemp = MAT34V_TO_MATRIX34(pTrailer->GetMatrix());
                    matGlobalAttach.Dot(matLocalAttach, matTemp);

					// Do radius check
					Vector3 vSep = matGlobalAttach.d - matGlobalBone.d;

					bool bTryAttach = vSep.Mag2() < ms_fAttachRadius * ms_fAttachRadius;

					if(bTryAttach && NetworkInterface::IsGameInProgress())// Add some leeway for reconnecting with a spherical constraint but only for MP so we don't break any missions.
					{
						static dev_float sfTrailerYOpposeDetachTolerance = 0.1f;
						float fTrailerYOpposeDetachTolerance = 0.0f;

						ScalarV sForwardDotParent = Dot(pTrailer->GetTransform().GetB(), pParent->GetTransform().GetB());

						fTrailerYOpposeDetachTolerance = CTrailer::sm_fTrailerYOpposeDetachSphericalConstraintInMP + sfTrailerYOpposeDetachTolerance;
						if(pTrailer->pHandling)
						{
							CTrailerHandlingData* pTrailerHandling = pTrailer->pHandling->GetTrailerHandlingData();
							if(pTrailerHandling && pTrailerHandling->m_fAttachedMaxDistance > 0.0f)
							{
								fTrailerYOpposeDetachTolerance = CTrailer::sm_fTrailerYOpposeDetach;
							}
						}
						
						bTryAttach = IsGreaterThanAll(sForwardDotParent, ScalarVFromF32(fTrailerYOpposeDetachTolerance)) ? true : false;
					}


					if(bTryAttach)
					{
						//We shouldn't be attaching here if either vehicle is dummy
						//want to assert though so any issues will still be easy to track down
						bool bAttached = false;
						if(AssertVerify(!pParent->IsDummy() && !pTrailer->IsDummy()))
						{
							bAttached = pTrailer->AttachToParentVehicle(pParent, sbTestWarp);
						}

						if(bAttached)
						{
							pParent->m_nVehicleFlags.bScansWithNonPlayerDriver = false;
                            // Assert(m_pTrailer); /* the trailer attachToParentVehicle command will set m_pTrailer on this gadget */
							break;
						}
					}

#if __DEV
					Color32 renderColour = bTryAttach ? Color_green : Color_red;					
					if(CTrailer::ms_bDebugDrawTrailers)
					{
						grcDebugDraw::Sphere(matGlobalBone.d, ms_fAttachRadius/2.0f, renderColour, false);
						grcDebugDraw::Sphere(matGlobalAttach.d, ms_fAttachRadius/2.0f, renderColour, false);
						grcDebugDraw::Line(matGlobalAttach.d, matGlobalBone.d, renderColour);
					}
#endif
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
// Directly set the trailer that is attached to this vehicle
void CVehicleTrailerAttachPoint::SetAttachedTrailer( CVehicle *pTrailer )
{
    if(m_pTrailer != pTrailer)
    {
		Assertf( !m_pTrailer || !pTrailer, "Attaching trailer when we already have one" );
        m_pTrailer = pTrailer;

        if(m_pTrailer == NULL)
        {
            m_uIgnoreAttachTimer = ms_uAttachResetTime;
        }
    }
}

////////////////////////////////////////////////////////////////////////////

void CVehicleTrailerAttachPoint::DetachTrailer(CVehicle* pParent)
{
	Assert(pParent);
	Assert(m_iAttachBone > -1);

	//detach the trailer if one is connected.
	if(m_pTrailer)
	{
		if(m_pTrailer->GetAttachParent() == pParent && (!m_pTrailer->GetNetworkObject() || !m_pTrailer->GetNetworkObject()->IsClone()))
		{
			if(m_pTrailer->GetIsAttached())
			{
				CPhysical *pPhyTrailer = m_pTrailer;
				CVehicle *pTrailerVeh = static_cast<CVehicle*>(pPhyTrailer);
				pTrailerVeh->GetVehicleAudioEntity()->TriggerTrailerDetach();
				m_pTrailer->DetachFromParent(0);

                if(m_pTrailer)// This pointer may have been cleared by detach from parent
                {
				    // detach at tail position
				    s32 iThisBoneIndex = pTrailerVeh->GetBoneIndex(TRAILER_ATTACH);
				    s32 iOtherBoneIndex = pParent->GetBoneIndex(VEH_ATTACH);

				    if(iThisBoneIndex != -1 && iOtherBoneIndex != -1)
				    {
					    Matrix34 matMyBone;
					    matMyBone = m_pTrailer->GetLocalMtx(iThisBoneIndex);

					    Matrix34 matOtherBone;
					    pParent->GetGlobalMtx(iOtherBoneIndex, matOtherBone);

					    Vector3 vforceDirection  = VEC3V_TO_VECTOR3(m_pTrailer->GetTransform().GetC());

					    vforceDirection.Normalize();

					    static dev_float forceMultiplier = 1.0f;//just apply a little pop force to show the player the trailers been released
					    vforceDirection *= (m_pTrailer->GetMass()*forceMultiplier);

					    s32 nEntComponent = pTrailerVeh->GetVehicleFragInst()->GetType()->GetControllingComponentFromBoneIndex(0, iThisBoneIndex); 
    					
					    pTrailerVeh->ApplyImpulse(vforceDirection, matMyBone.d, nEntComponent);
				    }
                }

			}
			
			m_pTrailer = NULL;
			m_uIgnoreAttachTimer = ms_uAttachResetTime;
		}
		else
		{
			CRequestDetachmentEvent::Trigger(*m_pTrailer);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
	
CTrailer* CVehicleTrailerAttachPoint::GetAttachedTrailer(const CVehicle* const pParent) const
{
	if(m_pTrailer)
	{
		if(m_pTrailer->GetAttachParent() == pParent)
		{
			if(m_pTrailer->GetIsAttached())
			{

				CPhysical *pPhyTrailer = m_pTrailer;
				CVehicle *pTrailerVeh = static_cast<CVehicle*>(pPhyTrailer);

				if(pTrailerVeh->GetVehicleType() == VEHICLE_TYPE_TRAILER)
					return static_cast<CTrailer*>(pTrailerVeh);
			}
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetWithJointsBase
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetWithJointsBase::CVehicleGadgetWithJointsBase(s16 iNumberOfJoints, 
                                                           float fMoveSpeed,
                                                           float fMuscleStiffness = 0.05f,
                                                           float fMuscleAngleOrPositionStrength = 10.0f,
                                                           float fMuscleSpeedStrength = 20.0f,
                                                           float fMinAndMaxMuscleTorqueOrForce = 100.0f) :
m_iNumberOfJoints(iNumberOfJoints),
m_fMoveSpeed(fMoveSpeed),
m_fMuscleStiffness(fMuscleStiffness),
m_fMuscleAngleOrPositionStrength(fMuscleAngleOrPositionStrength),
m_fMuscleSpeedStrength(fMuscleSpeedStrength),
m_fMinAndMaxMuscleTorqueOrForce(fMinAndMaxMuscleTorqueOrForce)
{
	m_JointInfo = rage_new JointInfo[m_iNumberOfJoints];

	for(s16 i = 0; i < m_iNumberOfJoints; i++)
	{
		m_JointInfo[i].m_iFragChild = 0;
		m_JointInfo[i].m_iBone = VEH_INVALID_ID;
        m_JointInfo[i].m_fDesiredAcceleration = 0.0f;

        m_JointInfo[i].m_bPrismatic = false;
		m_JointInfo[i].m_bContinuousInput = false;

        m_JointInfo[i].m_bSwingFreely = false;

        m_JointInfo[i].m_vecJointChildPosition = ORIGIN;

        m_JointInfo[i].m_PrismaticJointInfo.m_fPosition = 0.0f;
        m_JointInfo[i].m_PrismaticJointInfo.m_fDesiredPosition = 0.0f;
        m_JointInfo[i].m_PrismaticJointInfo.m_fOpenPosition = 0.0f;
        m_JointInfo[i].m_PrismaticJointInfo.m_fClosedPosition = 0.0f;
        m_JointInfo[i].m_vecJointChildPosition[0] = 0.0f;
        m_JointInfo[i].m_vecJointChildPosition[1] = 0.0f;
        m_JointInfo[i].m_vecJointChildPosition[2] = 0.0f;

        m_JointInfo[i].m_1dofJointInfo.m_eRotationAxis = ROT_AXIS_X;
        m_JointInfo[i].m_1dofJointInfo.m_fAngle = 0.0f;
        m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle = 0.0f;
		m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle = 0.0f;
		m_JointInfo[i].m_1dofJointInfo.m_fClosedAngle = 0.0f;
        m_JointInfo[i].m_1dofJointInfo.m_bOnlyControlSpeed = false;
		m_JointInfo[i].m_1dofJointInfo.m_bSnapToAngle = false;
	}
}

//////////////////////////////////////////////////////////////////////////

CVehicleGadgetWithJointsBase::~CVehicleGadgetWithJointsBase()
{
	delete [] m_JointInfo;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetWithJointsBase::InitJointLimits(const CVehicleModelInfo* pVehicleModelInfo)
{
	Assert(pVehicleModelInfo);

	if(pVehicleModelInfo->GetDrawable() 
		&& pVehicleModelInfo->GetDrawable()->GetSkeletonData())
	{
		const crBoneData* pBoneData = NULL;

		// If we have a bone set up the pitch limits
		for(s16 i = 0; i < m_iNumberOfJoints; i++)
		{	
			if(m_JointInfo[i].m_iBone > VEH_INVALID_ID)
			{
				s32 nGroupIndex = pVehicleModelInfo->GetFragType()->GetGroupFromBoneIndex(0, m_JointInfo[i].m_iBone);
				if(nGroupIndex > -1)
				{
					SetupTransmissionScale(pVehicleModelInfo, m_JointInfo[i].m_iBone);
					pBoneData = pVehicleModelInfo->GetDrawable()->GetSkeletonData()->GetBoneData(m_JointInfo[i].m_iBone);

					const crJointData* pJointData = pVehicleModelInfo->GetDrawable()->GetJointData();
					Assert(pJointData);

					Vec3V dofMin, dofMax;
					if(pBoneData && pBoneData->GetDofs() & crBoneData::HAS_ROTATE_LIMITS && pJointData->ConvertToEulers(*pBoneData, dofMin, dofMax) && (dofMax.GetZf()!=0.0f || dofMin.GetZf()!=0.0f))
					{
						m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle = dofMax.GetZf();
						m_JointInfo[i].m_1dofJointInfo.m_fClosedAngle = dofMin.GetZf();
	
						m_JointInfo[i].m_1dofJointInfo.m_eRotationAxis = ROT_AXIS_Z;
					}
					else if(pBoneData && pBoneData->GetDofs() & crBoneData::HAS_ROTATE_LIMITS && pJointData->ConvertToEulers(*pBoneData, dofMin, dofMax) && (dofMax.GetXf()!=0.0f || dofMin.GetXf()!=0.0f))
					{
						m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle = dofMax.GetXf();
						m_JointInfo[i].m_1dofJointInfo.m_fClosedAngle = dofMin.GetXf();

						m_JointInfo[i].m_1dofJointInfo.m_eRotationAxis = ROT_AXIS_X;
					}
					else if(pBoneData && pBoneData->GetDofs() & crBoneData::HAS_ROTATE_LIMITS && pJointData->ConvertToEulers(*pBoneData, dofMin, dofMax) && (dofMax.GetYf()!=0.0f || dofMin.GetYf()!=0.0f))
                    {
                        m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle = dofMax.GetYf();
                        m_JointInfo[i].m_1dofJointInfo.m_fClosedAngle = dofMin.GetYf();

                        m_JointInfo[i].m_1dofJointInfo.m_eRotationAxis = ROT_AXIS_Y;
                    }
					else if(pBoneData && pBoneData->GetDofs() & crBoneData::HAS_TRANSLATE_LIMITS && pJointData->GetTranslationLimits(*pBoneData, dofMin, dofMax) && (dofMax.GetZf()!=0.0f || dofMin.GetZf()!=0.0f))
                    {
                        m_JointInfo[i].m_PrismaticJointInfo.m_fOpenPosition = dofMax.GetZf();
                        m_JointInfo[i].m_PrismaticJointInfo.m_fClosedPosition = dofMin.GetZf();
						m_JointInfo[i].m_PrismaticJointInfo.m_bOnlyControlSpeed = true;
                        m_JointInfo[i].m_bPrismatic = true;
					}

                    if(m_JointInfo[i].m_bPrismatic == false)
                    {
                        //make sure the open angle isn't too large
                        if(m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle >= (2*PI))
                        {
                            m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle = (2*PI);
                        }
                    }

				}
			}
		}
	}

	//get fragment childs for bones
	for(s16 i = 0; i < m_iNumberOfJoints; i++)
	{	
		if(m_JointInfo[i].m_iBone > VEH_INVALID_ID)
		{
			m_JointInfo[i].m_iFragChild = (s8)pVehicleModelInfo->GetFragType()->GetComponentFromBoneIndex(0, m_JointInfo[i].m_iBone);
		}
	}

}

//////////////////////////////////////////////////////////////////////////

bool CVehicleGadgetWithJointsBase::WantsToBeAwake(CVehicle*)
{
	for(s16 i = 0; i < m_iNumberOfJoints; i++)
	{	
		if(m_JointInfo[i].m_iBone > VEH_INVALID_ID)
		{
            if(m_JointInfo[i].m_bPrismatic)
            {
                if(!rage::IsNearZero(m_JointInfo[i].m_PrismaticJointInfo.m_fPosition - m_JointInfo[i].m_PrismaticJointInfo.m_fDesiredPosition, fJointSleepAngleThreshold))
				{
                    return true;
				}
            }
            else
            {
                //only check whether our desired angle is close if we're not just driving the joints speed.
                if(!m_JointInfo[i].m_1dofJointInfo.m_bOnlyControlSpeed)
                {
			        if(!rage::IsNearZero(m_JointInfo[i].m_1dofJointInfo.m_fAngle - m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle, fJointSleepAngleThreshold) &&
					   !rage::IsNearZero(m_JointInfo[i].m_1dofJointInfo.m_fAngle + m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle - m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle, fJointSleepAngleThreshold))
					{
				        return true;
					}
                }
            }

            if(!rage::IsNearZero(m_JointInfo[i].m_fDesiredAcceleration,fJointSleepAngleThreshold))
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetWithJointsBase::DriveJointToRatio(CVehicle* pParent, s16 iJointIndex, float fTargetRatio)
{
	ENABLE_FRAG_OPTIMIZATION_ONLY(pParent->GiveFragCacheEntry();)
    Assert(iJointIndex < m_iNumberOfJoints);

    fragInstGta* pFragInst = pParent->GetVehicleFragInst();
    Assert(pFragInst && pFragInst->GetCacheEntry());

    pFragInst->GetCacheEntry()->LazyArticulatedInit();

    if(m_JointInfo[iJointIndex].m_iBone > VEH_INVALID_ID)
    {
        float fTarget = fTargetRatio * GetJoint(iJointIndex)->m_1dofJointInfo.m_fOpenAngle + (1.0f - fTargetRatio) * GetJoint(iJointIndex)->m_1dofJointInfo.m_fClosedAngle;

        GetJoint(iJointIndex)->m_bSwingFreely = false;

        GetJoint(iJointIndex)->m_1dofJointInfo.m_fDesiredAngle = fTarget;
    }
}

//////////////////////////////////////////////////////////////////////////

JointInfo *CVehicleGadgetWithJointsBase::GetJoint(s16 iJointIndex)
{
	Assert(iJointIndex < m_iNumberOfJoints);

	return &m_JointInfo[iJointIndex];
}

//////////////////////////////////////////////////////////////////////////

bool CVehicleGadgetWithJointsBase::SetJointLimits(CVehicle* pParent, s16 iJointIndex, const float fOpenAngle, const float fClosedAngle)
{
    fragInstGta* pFragInst = pParent->GetVehicleFragInst();
    // check that we have a frag inst, and that it's the inst driving the car (i.e. it's not using dummy car physics)
    if(pFragInst==NULL || !pFragInst->IsInLevel() || pFragInst!=pParent->GetCurrentPhysicsInst())
        return false;

    // find the associated joint
    fragHierarchyInst* pHierInst = NULL;
    if(pFragInst && pFragInst->GetCached())
    {
        pHierInst = pFragInst->GetCacheEntry()->GetHierInst();	
    }

    Assert(pHierInst);
    Assert(GetJoint(iJointIndex)->m_iFragChild > -1);

    phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;

    if(pArticulatedCollider == NULL || pHierInst->body == NULL)
    {
        return false;
    }

	bool bJointLimitsSet = false;
    s32 linkIndex = pArticulatedCollider->GetLinkFromComponent(GetJoint(iJointIndex)->m_iFragChild);
    if(linkIndex > 0)
    {
        phJoint1Dof* p1DofJoint = NULL;
        phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);

        Assert(pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF);

        if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
            p1DofJoint = static_cast<phJoint1Dof*>(pJoint);

        if(p1DofJoint)
        {
			bJointLimitsSet = true;
            p1DofJoint->SetAngleLimits(fClosedAngle, fOpenAngle);

            m_JointInfo[iJointIndex].m_1dofJointInfo.m_fOpenAngle = fOpenAngle;
            m_JointInfo[iJointIndex].m_1dofJointInfo.m_fClosedAngle = fClosedAngle;
        }
    }

	return bJointLimitsSet;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetWithJointsBase::ProcessPrePhysics(CVehicle* pParent) 
{
    fragInstGta* pFragInst = pParent->GetVehicleFragInst();
    // check that we have a frag inst, and that it's the inst driving the car (i.e. it's not using dummy car physics)
    if(pFragInst==NULL || !pFragInst->IsInLevel() || pFragInst!=pParent->GetCurrentPhysicsInst())
        return;

    // find the associated joint
    fragHierarchyInst* pHierInst = NULL;
    if(pFragInst && pFragInst->GetCached())
    {
        pHierInst = pFragInst->GetCacheEntry()->GetHierInst();	
    }

    for(s16 i = 0; i < m_iNumberOfJoints; i++)
    {	
        if(m_JointInfo[i].m_iBone > VEH_INVALID_ID)
        {   
            if(m_JointInfo[i].m_bSwingFreely)
            {
                SwingJointFreely(pHierInst, m_JointInfo[i].m_iFragChild);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetWithJointsBase::ProcessPhysics(CVehicle* pParent, const float UNUSED_PARAM(fTimestep), const int UNUSED_PARAM(nTimeSlice))
{
	// Drive the digger arm joints to their desired angle
	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	// check that we have a frag inst, and that it's the inst driving the car (i.e. it's not using dummy car physics)
	if(pFragInst==NULL || !pFragInst->IsInLevel() || pFragInst!=pParent->GetCurrentPhysicsInst())
		return;

	// find the associated joint
	fragHierarchyInst* pHierInst = NULL;
	if(pFragInst && pFragInst->GetCached())
	{
		pHierInst = pFragInst->GetCacheEntry()->GetHierInst();	
	}

	for(s16 i = 0; i < m_iNumberOfJoints; i++)
	{	
		if(m_JointInfo[i].m_iBone > VEH_INVALID_ID)
        {   
            if(!m_JointInfo[i].m_bSwingFreely)
            {
                if(m_JointInfo[i].m_bPrismatic)
                {
                    m_JointInfo[i].m_PrismaticJointInfo.m_fDesiredPosition += m_JointInfo[i].m_fDesiredAcceleration * m_fMoveSpeed * fwTimer::GetTimeStep();//update the desired position
     
                    //clamp to the joint extents
                    m_JointInfo[i].m_PrismaticJointInfo.m_fDesiredPosition = rage::Clamp(m_JointInfo[i].m_PrismaticJointInfo.m_fDesiredPosition, m_JointInfo[i].m_PrismaticJointInfo.m_fClosedPosition, m_JointInfo[i].m_PrismaticJointInfo.m_fOpenPosition);

                    if(m_JointInfo[i].m_iFragChild > -1)
                    {
                        DriveComponentToPosition(  pHierInst, 
                            m_JointInfo[i].m_iFragChild, 
                            m_JointInfo[i].m_PrismaticJointInfo.m_fDesiredPosition, 
                            m_JointInfo[i].m_fDesiredAcceleration, 
                            m_JointInfo[i].m_PrismaticJointInfo.m_fPosition,
                            m_JointInfo[i].m_vecJointChildPosition,
                            m_fMuscleStiffness,
                            m_fMuscleAngleOrPositionStrength,
                            m_fMuscleSpeedStrength,
                            m_fMinAndMaxMuscleTorqueOrForce,
					        m_JointInfo[i].m_PrismaticJointInfo.m_bOnlyControlSpeed);
                    }

					m_JointInfo[i].m_fSpeed = (m_JointInfo[i].m_PrismaticJointInfo.m_fPosition - m_JointInfo[i].m_fPrevPosition) * fwTimer::GetInvTimeStep();
					m_JointInfo[i].m_fPrevPosition = m_JointInfo[i].m_PrismaticJointInfo.m_fPosition;
                }
                else
                {
					if(m_JointInfo[i].m_1dofJointInfo.m_bSnapToAngle)
					{
						if(m_JointInfo[i].m_iFragChild > -1)
						{
							SnapComponentToAngle(pFragInst, 
								m_JointInfo[i].m_iFragChild, 
								m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle, 
								m_JointInfo[i].m_1dofJointInfo.m_fAngle);
						}
						m_JointInfo[i].m_1dofJointInfo.m_bSnapToAngle = false; // Should only snap it once, unless it's been asked again
					}
                    //Do we only want to control the speed of the joint?
                    else if(m_JointInfo[i].m_1dofJointInfo.m_bOnlyControlSpeed)
                    {
                        float fMoveSpeed = m_JointInfo[i].m_fDesiredAcceleration * m_fMoveSpeed;

                        if(m_JointInfo[i].m_iFragChild > -1)
                        {
                            DriveComponentToAngle(  pHierInst, 
                                m_JointInfo[i].m_iFragChild, 
                                m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle, 
                                fMoveSpeed, 
                                m_JointInfo[i].m_1dofJointInfo.m_fAngle,
                                m_fMuscleStiffness,
                                m_fMuscleAngleOrPositionStrength,
                                m_fMuscleSpeedStrength,
                                m_fMinAndMaxMuscleTorqueOrForce,
                                true);
                        }
                    }
                    else
                    {
						// Update the desired pitch
						if(m_JointInfo[i].m_bContinuousInput)
						{
							// Only pushes to a maximum offset from the current position
							// This means that a joint blocked by something immovable will only set it's desired position partway into the obstruction -still applying forces against it
							//  but will be much more responsive to going back the other way because it won't have to backtrack through the blocked region of space
							const float maxDesiredDeltaRatio = 0.1f;
							float maxDesiredDelta = Abs(m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle - m_JointInfo[i].m_1dofJointInfo.m_fClosedAngle) * maxDesiredDeltaRatio;
							float newDesiredAng = m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle + (m_JointInfo[i].m_fDesiredAcceleration * m_fMoveSpeed * fwTimer::GetTimeStep());
							float minDesiredAng = m_JointInfo[i].m_1dofJointInfo.m_fAngle - maxDesiredDelta;
							float maxDesiredAng = m_JointInfo[i].m_1dofJointInfo.m_fAngle + maxDesiredDelta;
							m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle = rage::Clamp(newDesiredAng, minDesiredAng, maxDesiredAng);
						}
						else
						{
							// Simple method - keeps pushing the desired position regardless of current
							m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle += m_JointInfo[i].m_fDesiredAcceleration * m_fMoveSpeed * fwTimer::GetTimeStep();//update the desired pitch
						}

                        m_JointInfo[i].m_fDesiredAcceleration = 0.0f;//reset the acceleration after its been used.

			            if(m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle >= (2*PI)-0.01f)//is the joint close to a full circle around, added the -0.01f just to allow some tolerance
			            {//if the joint goes all the way around then wrap instead of clamp
				            if(m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle > m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle)
					            m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle = (m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle - m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle);
				            else if(m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle < 0.0f)
					            m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle = (m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle + m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle);
			            }
			            else
			            {//clamp to the joint extents
				            m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle = rage::Clamp(m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle, m_JointInfo[i].m_1dofJointInfo.m_fClosedAngle, m_JointInfo[i].m_1dofJointInfo.m_fOpenAngle);
			            }


			            if(m_JointInfo[i].m_iFragChild > -1)
                        {
                            DriveComponentToAngle(  pHierInst, 
                                m_JointInfo[i].m_iFragChild, 
                                m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle, 
                                m_fMoveSpeed, 
                                m_JointInfo[i].m_1dofJointInfo.m_fAngle,
                                m_fMuscleStiffness,
                                m_fMuscleAngleOrPositionStrength,
                                m_fMuscleSpeedStrength,
                                m_fMinAndMaxMuscleTorqueOrForce );
                        }
                    }

					m_JointInfo[i].m_fSpeed = (m_JointInfo[i].m_1dofJointInfo.m_fAngle - m_JointInfo[i].m_fPrevPosition) * fwTimer::GetInvTimeStep();
					m_JointInfo[i].m_fPrevPosition = m_JointInfo[i].m_1dofJointInfo.m_fAngle;
                }
            }
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetWithJointsBase::SetupTransmissionScale(const CVehicleModelInfo* pVehicleModelInfo, s32 boneIndex)
{
	if(boneIndex > -1)
	{
		if(pVehicleModelInfo->GetFragType())
		{
			s32 nGroupIndex = pVehicleModelInfo->GetFragType()->GetGroupFromBoneIndex(0, boneIndex);
			if(nGroupIndex > -1)
			{
				fragTypeGroup* pGroup = pVehicleModelInfo->GetFragType()->GetPhysics(0)->GetAllGroups()[nGroupIndex];
				if(pGroup)
				{
					pGroup->SetForceTransmissionScaleUp(0.0f);
					pGroup->SetForceTransmissionScaleDown(0.0f);
				}
			}
		}
	}
}

static dev_float sfDiggerMuscleStiffness = 0.5f;
static dev_float sfDiggerMuscleAngleStrength = 15.0f;
static dev_float sfDiggerMuscleSpeedStrength = 30.0f;
static dev_float sfDiggerMinAndMaxMuscleTorqueGadgets = 200.0f;

//////////////////////////////////////////////////////////////////////////
// Class CVehicleDiggerArm
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetDiggerArm::CVehicleGadgetDiggerArm(s32 iAttachBoneIndex, float fMoveSpeed) :
CVehicleGadgetWithJointsBase(1, fMoveSpeed, sfDiggerMuscleStiffness, sfDiggerMuscleAngleStrength, sfDiggerMuscleSpeedStrength, sfDiggerMinAndMaxMuscleTorqueGadgets)//only need one joint
{
	Assert(iAttachBoneIndex > -1);
	m_VehicleDiggerAudio = NULL;
	m_bDefaultRatioOverride = false;

	GetJoint(0)->m_iBone = iAttachBoneIndex;
	GetJoint(0)->m_bContinuousInput = true;

	m_fJointPositionRatio = 0.f;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDiggerArm::InitAudio(CVehicle* pParent)
{
	m_VehicleDiggerAudio = (audVehicleDigger*)pParent->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_DIGGER);
	audAssert(m_VehicleDiggerAudio);
}

float CVehicleGadgetDiggerArm::GetJointPositionRatio()
{
	float maxAngleDisplacement = GetJoint(0)->m_1dofJointInfo.m_fOpenAngle - GetJoint(0)->m_1dofJointInfo.m_fClosedAngle;
	float positionRatio = (GetJoint(0)->m_1dofJointInfo.m_fAngle - GetJoint(0)->m_1dofJointInfo.m_fClosedAngle)/maxAngleDisplacement;

	return rage::Clamp(positionRatio, 0.0f, 1.0f);
}

void CVehicleGadgetDiggerArm::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_POSITION = 8;
	static const float MAX_POSITION = 1.7f;

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fJointPositionRatio, MAX_POSITION, SIZEOF_POSITION, "m_fJointPositionRatio");
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDiggerArm::ProcessControl(CVehicle* pParent)
{
	REPLAY_ONLY(if(CReplayMgr::IsEditModeActive()) return;)

	CVehicleGadgetWithJointsBase::ProcessControl(pParent);

	if (pParent)
	{
		if (!pParent->IsNetworkClone())
		{
			//Keep the digger bucket off the ground when no there's no input
			static dev_float fDiggerJointDefaultRatio = 0.20f;
			CCollisionHistory *pCollHistory = pParent->GetFrameCollisionHistory();
			if(pParent->GetDriver() && !m_bDefaultRatioOverride && 
				GetJoint(0)->m_fDesiredAcceleration == 0.0f && GetJointPositionRatio() < fDiggerJointDefaultRatio && 
				pCollHistory->GetCollisionImpulseMagSum() == 0.0f)
			{ 
				DriveJointToRatio(pParent, DIGGER_JOINT_BASE, fDiggerJointDefaultRatio);
			}

			//Store the joint ratio so it can be sent across the network
			m_fJointPositionRatio = GetJointPositionRatio();
		}
		else
		{
			//Clone - apply the received joint ratio
			DriveJointToRatio(pParent, DIGGER_JOINT_BASE, m_fJointPositionRatio);
			return;
		}
	}

	for(s16 i = 0; i < DIGGER_JOINT_MAX && i < m_iNumberOfJoints; i++)
	{	
		JointInfo* joint = GetJoint(i);
		if(joint->m_iBone > VEH_INVALID_ID && m_VehicleDiggerAudio)
		{
			f32 audioAcceleration = joint->m_fDesiredAcceleration;
			f32 audioMovementSpeed = audioMovementSpeed = WantsToBeAwake(pParent) ? joint->m_fSpeed : 0.0f;

			if(!joint->m_bPrismatic)
			{
				if((joint->m_1dofJointInfo.m_fAngle <= (joint->m_1dofJointInfo.m_fClosedAngle + 0.05f) && joint->m_fDesiredAcceleration < 0.0f) ||
					(joint->m_1dofJointInfo.m_fAngle >= (joint->m_1dofJointInfo.m_fOpenAngle - 0.05f) && joint->m_fDesiredAcceleration > 0.0f))
				{
					audioAcceleration = 0.0f;
					audioMovementSpeed = 0.0f;
				}
			}
			else
			{
				if((joint->m_PrismaticJointInfo.m_fPosition <= (joint->m_PrismaticJointInfo.m_fClosedPosition + 0.05f) && joint->m_fDesiredAcceleration < 0.0f) ||
					(joint->m_PrismaticJointInfo.m_fPosition >= (joint->m_PrismaticJointInfo.m_fOpenPosition - 0.05f) && joint->m_fDesiredAcceleration > 0.0f))
				{
					audioAcceleration = 0.0f;
					audioMovementSpeed = 0.0f;
				}
			}

			m_VehicleDiggerAudio->SetSpeed(i, audioMovementSpeed, audioAcceleration);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Class CVehicleArticulatedDiggerArm
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetArticulatedDiggerArm::CVehicleGadgetArticulatedDiggerArm( float fMoveSpeed) :
CVehicleGadgetWithJointsBase(DIGGER_JOINT_MAX, fMoveSpeed)
{
	m_VehicleDiggerAudio = NULL;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetArticulatedDiggerArm::InitAudio(CVehicle* pParent)
{
	m_VehicleDiggerAudio = (audVehicleDigger*)pParent->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_DIGGER);
	audAssert(m_VehicleDiggerAudio);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetArticulatedDiggerArm::SetDiggerJointBone(eDiggerJoints diggerJointType, s32 iBoneIndex)
{
	GetJoint((s16)diggerJointType)->m_iBone = iBoneIndex;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetArticulatedDiggerArm::ProcessControl(CVehicle* pParent)
{
	REPLAY_ONLY(if(CReplayMgr::IsEditModeActive()) return;)

	CVehicleGadgetWithJointsBase::ProcessControl(pParent);

	for(s16 i = 0; i < DIGGER_JOINT_MAX; i++)
	{	
		JointInfo* joint = GetJoint(i);
		if(joint->m_iBone > VEH_INVALID_ID && m_VehicleDiggerAudio)
		{
			f32 audioAcceleration = joint->m_fDesiredAcceleration;
			f32 audioMovementSpeed = WantsToBeAwake(pParent) ? joint->m_fSpeed : 0.0f;

			if(!joint->m_bPrismatic)
			{
				if((joint->m_1dofJointInfo.m_fAngle <= (joint->m_1dofJointInfo.m_fClosedAngle + 0.05f) && joint->m_fDesiredAcceleration < 0.0f) ||
					(joint->m_1dofJointInfo.m_fAngle >= (joint->m_1dofJointInfo.m_fOpenAngle - 0.05f) && joint->m_fDesiredAcceleration > 0.0f))
				{
					audioAcceleration = 0.0f;
					audioMovementSpeed = 0.0f;
				}
			}
			else
			{
				if((joint->m_PrismaticJointInfo.m_fPosition <= (joint->m_PrismaticJointInfo.m_fClosedPosition + 0.05f) && joint->m_fDesiredAcceleration < 0.0f) ||
				   (joint->m_PrismaticJointInfo.m_fPosition >= (joint->m_PrismaticJointInfo.m_fOpenPosition - 0.05f) && joint->m_fDesiredAcceleration > 0.0f))
				{
					audioAcceleration = 0.0f;
					audioMovementSpeed = 0.0f;
				}
			}

			m_VehicleDiggerAudio->SetSpeed(i, audioMovementSpeed, audioAcceleration);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

s32 CVehicleGadgetArticulatedDiggerArm::GetNumberOfSetupDiggerJoint()
{
	s16 numberOfDiggerJoints = 0;
	for(s16 i = 0; i < DIGGER_JOINT_MAX; i++)
	{	
		if(GetJoint(i)->m_iBone > VEH_INVALID_ID)
		{
			numberOfDiggerJoints++;
		}
	}

	return numberOfDiggerJoints;
}


//////////////////////////////////////////////////////////////////////////

bool    CVehicleGadgetArticulatedDiggerArm::GetJointSetup(eDiggerJoints diggerJoint)
{
    if(GetJoint(s16(diggerJoint))->m_iBone > VEH_INVALID_ID)
    {
        return true;
    }

    return false;
}


//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetParkingSensor
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetParkingSensor::CVehicleGadgetParkingSensor(CVehicle* pParent, float parkingSensorRadius, float parkingSensorLength) : 
m_BeepPlaying(false),
m_ParkingSensorRadius(parkingSensorRadius),
m_ParkingSensorLength(parkingSensorLength),
m_VehicleWidth(0.0f),
m_TimeToStopParkingBeep(0.f)
{
	Init(pParent);
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetParkingSensor::Init(CVehicle* pParent)
{
	//get the vehicle width from the distance between the RR and LR wheels
	CWheel* pWheelLR = pParent->GetWheelFromId(VEH_WHEEL_LR);
	CWheel* pWheelRR = pParent->GetWheelFromId(VEH_WHEEL_RR);
	if(pWheelLR && pWheelRR)
	{
		Vector3 posLR = CWheel::GetWheelOffset(pParent->GetVehicleModelInfo(), VEH_WHEEL_LR);
		Vector3 posRR  = CWheel::GetWheelOffset(pParent->GetVehicleModelInfo(), VEH_WHEEL_RR);

		//width between rear wheels
		m_VehicleWidth = posRR.x - posLR.x;
	}
	else //just use the width of the bounding box if we didn't find the rear wheels. rather not use this as if a doors open it will be the wrong size
	{
		m_VehicleWidth = pParent->GetBoundingBoxMax().x - pParent->GetBoundingBoxMin().x; 
	}

	Vector3 vBoundingBoxMin = pParent->GetBoundingBoxMin();
	Vector3 vBoundingBoxMax = pParent->GetBoundingBoxMax();

	// Make sure the radius isn't too big that it will pick up impacts with the ground.
	m_ParkingSensorRadius = MIN(((vBoundingBoxMax.z - vBoundingBoxMin.z)*0.5f) - 0.05f, m_ParkingSensorRadius);

	const float fSensorDistanceAboveGround = 0.1f;

	// check halfway up the bounding box
	const float fParkingSensorHeight = vBoundingBoxMin.z + m_ParkingSensorRadius + fSensorDistanceAboveGround;

	//Setup the values for the capsules
	//centre capsule
	m_ParkingSensorCapsules[0].startPosition = Vector3( 0.0f, vBoundingBoxMin.y, fParkingSensorHeight);
	m_ParkingSensorCapsules[0].endPosition = Vector3( 0.0f, vBoundingBoxMin.y - m_ParkingSensorLength, fParkingSensorHeight);

	//side capsules
	static dev_float sideCapsuleOffsetWidthMultiplier = 0.05f;//The amount to fan out the capsule as a percentage of the half width, arbitrary but should work ok
	//use the half width of the vehicle and then add a bit to fan out the capsule
	float probePositionStartX = (m_VehicleWidth/2.0f)-(m_VehicleWidth*sideCapsuleOffsetWidthMultiplier);
	float probePositionEndX = (m_VehicleWidth/2.0f)+(m_VehicleWidth*sideCapsuleOffsetWidthMultiplier);

	m_ParkingSensorCapsules[1].startPosition = Vector3( probePositionStartX, vBoundingBoxMin.y, fParkingSensorHeight);
	m_ParkingSensorCapsules[1].endPosition = Vector3( probePositionEndX, vBoundingBoxMin.y - m_ParkingSensorLength, fParkingSensorHeight);
	
	m_ParkingSensorCapsules[2].startPosition = Vector3( -probePositionStartX, vBoundingBoxMin.y, fParkingSensorHeight);
	m_ParkingSensorCapsules[2].endPosition = Vector3( -probePositionEndX, vBoundingBoxMin.y - m_ParkingSensorLength, fParkingSensorHeight);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetParkingSensor::ProcessControl(CVehicle* pParent)
{
	Assert(pParent);
	const float fThrottle = pParent->GetThrottle();
	const bool bForward = (fThrottle > 0.01f);

	//If we are going forward, stop the beep
	if(bForward)
	{
		if(m_BeepPlaying)
		{
			pParent->GetVehicleAudioEntity()->StopParkingBeepSound();
			m_BeepPlaying = false;
		}
	}
	else
	{
		//Only use parking sensors if we're the player reversing
		const bool bReversing = (fThrottle < -0.01f);
		if(bReversing && pParent->ContainsLocalPlayer() && pParent->GetVehicleAudioEntity()->IsParkingBeepEnabled())
		{
			m_TimeToStopParkingBeep = 0.f;
			WorldProbe::CShapeTestFixedResults<> testResults;
			float parkingSensorBestDepth = FLT_MAX;

			//setup the capsule test.
			const u32 iTestFlags = (ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetResultsStructure(&testResults);
			capsuleDesc.SetIncludeFlags(iTestFlags);
			capsuleDesc.SetExcludeEntity(pParent);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetDoInitialSphereCheck(true);

			//loop through the capsules and find the closest intersecting point and record it's distance
			for(s32 i = 0; i < PARKING_SENSOR_CAPSULE_MAX; i++)
			{
				capsuleDesc.SetCapsule( pParent->TransformIntoWorldSpace(m_ParkingSensorCapsules[i].startPosition), 
										pParent->TransformIntoWorldSpace(m_ParkingSensorCapsules[i].endPosition), 
										m_ParkingSensorRadius );
				
				// Reset the results structure so that we can reuse it.
				testResults.Reset();

				if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
				{
					Assert(testResults[0].GetHitDetected());

#if __BANK	
					if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
						&& (pParent->GetStatus()==STATUS_PLAYER))
					{
						grcDebugDraw::Sphere(testResults[0].GetHitPosition(),0.1f, Color_yellow);
					}
#endif

					//work out the distance from the start of the capsule/sensor
					Vector3 hitDelta = pParent->TransformIntoWorldSpace(m_ParkingSensorCapsules[i].startPosition) - testResults[0].GetHitPosition();
					float distance = hitDelta.Mag();

					//is this the closest hit we've found
					if(distance < parkingSensorBestDepth)
						parkingSensorBestDepth = distance;
				}
			}

			//if the parking sensor depth is smaller then the parking sensor length then we've hit something.
			if(parkingSensorBestDepth < m_ParkingSensorLength && !pParent->IsUpsideDown() && !pParent->IsOnItsSide())
			{
				//make a distance value between 0.0 and 1.0 with 1.0 being furthest away
				float distance  = 1.0f - ((m_ParkingSensorLength - parkingSensorBestDepth)/m_ParkingSensorLength) ;

				distance = Clamp<float>(distance, 0.f, 1.f);
				//Call some code to play a beep here
				pParent->GetVehicleAudioEntity()->UpdateParkingBeep(distance);
				m_BeepPlaying = true;
			}
			else
			{
				pParent->GetVehicleAudioEntity()->StopParkingBeepSound();
				m_BeepPlaying = false;
			}

#if __BANK
			if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
				&& (pParent->GetStatus()==STATUS_PLAYER))
			{
				static s32 x = 4;
				static s32 y = 30;

				char debugText[100];

				sprintf(debugText, "Parking Sensor capsule Depth:%1.2f", parkingSensorBestDepth);
				grcDebugDraw::PrintToScreenCoors(debugText, x,y);
			}
#endif
		}
		else
		{
			if(m_BeepPlaying)
			{
				m_TimeToStopParkingBeep += fwTimer::GetTimeStep();
				if(m_TimeToStopParkingBeep >= 3.0f)
				{
					pParent->GetVehicleAudioEntity()->StopParkingBeepSound();
					m_BeepPlaying = false;
				}
			}
		}
	}
}

//Regular tow truck
static dev_float sfTowTruckRopeLength = 1.8f;
static dev_float sfTowTruckRopeMinLength = 1.4f;
static dev_float sfTowTruckRopeAdjustableLength = sfTowTruckRopeLength - sfTowTruckRopeMinLength;

//Tow truck 2 model
static dev_float sfTowTruck2RopeLength = 1.2f;
static dev_float sfTowTruck2RopeMinLength = 0.65f;
static dev_float sfTowTruck2RopeAdjustableLength = sfTowTruck2RopeLength - sfTowTruck2RopeMinLength;

static dev_float sfTowTruckRopeLengthChangeRate = 0.5f;
static dev_s32 siTowTruckRopeType = 1;
static dev_float sfMuscleStiffness = 0.995f;//0.15f;
static dev_float sfMuscleAngleStrength = 35.0f;//16.0f;
static dev_float sfMuscleSpeedStrength = 50.0f;//21.0f; 
static dev_float sfMinAndMaxMuscleTorqueGadgets = 90.0f;//35.0f;
static dev_float sfAllowedConstraintPenetration = 0.01f;

static dev_float sfHookWeight = 50.0f;

static dev_s32 siHookRopeAttachmentBoneIndexA = 2;
static dev_s32 siHookRopeAttachmentBoneIndexB = 3;
static dev_s32 siHookPointBoneIndex = 4;

static dev_float sfVehicleSpeedSqrToEnableSphericalConstraintAndArmMovement = 36.0f;

//////////////////////////////////////////////////////////////////////////
// Class CVehicleTowArm
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetTowArm::CVehicleGadgetTowArm(s32 iTowArmBoneIndex, s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex, float fMoveSpeed) :
CVehicleGadgetWithJointsBase( (iTowArmBoneIndex > -1 ? 1 : 0), fMoveSpeed, sfMuscleStiffness, sfMuscleAngleStrength, sfMuscleSpeedStrength, sfMinAndMaxMuscleTorqueGadgets ),
m_iTowMountAChild(0),
m_iTowMountA(iTowMountABoneIndex),
m_iTowMountBChild(0),
m_iTowMountB(iTowMountBBoneIndex),
m_PropObject(NULL),
m_pTowedVehicle(NULL),
m_Setup(false),
m_bHoldsDrawableRef(false),
m_bRopesAndHookInitialized(false),
m_bJointLimitsSet(false),
m_uIgnoreAttachTimer(0),
m_VehicleTowArmAudio(NULL),
m_RopeInstanceA(NULL),
m_RopeInstanceB(NULL),
m_RopesAttachedVehiclePhysicsInst(NULL),
m_bDetachVehicle(false),
m_vTowedVehicleOriginalAngDamp(0.0f, 0.0f, 0.0f),
m_fPrevJointPosition(0.0f),
m_LastTowArmPositionRatio(1.0f),
m_vLastFramesDistance( 0.0f, 0.0f, 0.0f ),
m_HookRestingOnTruck(false),
m_vDesiredTowHookPosition( 0.0f, 0.0f, 0.0f ),
m_vStartTowHookPosition( 0.0f, 0.0f, 0.0f ),
m_vCurrentTowHookPosition( 0.0f, 0.0f, 0.0f ),
m_vLocalPosConstraintStart( 0.0f, 0.0f, 0.0f ),
m_fTowTruckPositionTransitionTime(0.0f),
m_fForceMultToApplyInConstraint(1.0f),
m_bUsingSphericalConstraint(false),
m_bTowTruckTouchingTowVehicle(false),
m_bIsTeleporting(false),
m_bIsVisible(true)
{
	Assert(iTowMountABoneIndex > -1);
	Assert(iTowMountBBoneIndex > -1);

	if(iTowArmBoneIndex > -1)
	{
		GetJoint(0)->m_iBone = iTowArmBoneIndex;
	}
}

//////////////////////////////////////////////////////////////////////////

CVehicleGadgetTowArm::~CVehicleGadgetTowArm()
{
    DeleteRopesHookAndConstraints();

    if (m_PropObject)
    {
        CGameWorld::Remove(m_PropObject);
        CObjectPopulation::DestroyObject(m_PropObject);
        m_PropObject = NULL;
    }
}

void CVehicleGadgetTowArm::DeleteRopesHookAndConstraints(CVehicle* pParent)
{
	vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::DeleteRopesHookAndConstraints - Detaching entity because rope constraint deleted.");

    DetachVehicle(pParent, false);

    //remove the constraint if we have any
    if(m_posConstraintHandle.IsValid())
    {
        PHCONSTRAINT->Remove(m_posConstraintHandle);
        m_posConstraintHandle.Reset();
    }

    if(m_sphericalConstraintHandle.IsValid())
    {
        PHCONSTRAINT->Remove(m_sphericalConstraintHandle);
        m_sphericalConstraintHandle.Reset();
    }

    if(m_rotYConstraintHandle.IsValid())
    {
        PHCONSTRAINT->Remove(m_rotYConstraintHandle);
        m_rotYConstraintHandle.Reset();
    }

	RemoveRopesAndHook();
}

bool CVehicleGadgetTowArm::IsPickUpFromTopVehicle(const CVehicle *pParent) const {return pParent->InheritsFromHeli();}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::RemoveRopesAndHook()
{
    //Displayf(":RemoveRopesAndHook, %p (%p, %p, %p)",this,m_RopeInstanceA,m_RopeInstanceB,m_PropObject.Get());
	if(m_bRopesAndHookInitialized == false)
		return;

    if(m_RopeInstanceA)
    {
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(m_RopeInstanceA->GetUniqueID());
			CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(m_RopeInstanceA->GetUniqueID());
			CReplayMgr::RecordPersistantFx<CPacketDeleteRope>( CPacketDeleteRope(m_RopeInstanceA->GetUniqueID()),	CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceA), pAttachedA, pAttachedB, false);
		}
#endif
        CPhysics::GetRopeManager()->RemoveRope(m_RopeInstanceA);
        m_RopeInstanceA = NULL;
    }

    if(m_RopeInstanceB)
    {
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(m_RopeInstanceB->GetUniqueID());
			CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(m_RopeInstanceB->GetUniqueID());
			CReplayMgr::RecordPersistantFx<CPacketDeleteRope>( CPacketDeleteRope(m_RopeInstanceB->GetUniqueID()),	CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceB), pAttachedA, pAttachedB, false);
		}
#endif
        CPhysics::GetRopeManager()->RemoveRope(m_RopeInstanceB);
        m_RopeInstanceB = NULL;
    }

    if (m_PropObject)
    {
        if(m_bHoldsDrawableRef)
        {
            m_PropObject->GetBaseModelInfo()->ReleaseDrawable();
			m_bHoldsDrawableRef = false;
        }
        CGameWorld::Remove(m_PropObject);
        //CObjectPopulation::DestroyObject(m_PropObject);
        //m_PropObject = NULL;
    }

	vehicleDisplayf("[TOWTRUCK ROPE DEBUG]CVehicleGadgetTowArm::RemoveRopesAndHook - Detaching entity because cleaning up prop and ropes.");
    
    m_bRopesAndHookInitialized = false;
    m_Setup = false;
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::Init(CVehicle* pParent, const CVehicleModelInfo* pVehicleModelInfo)
{
	//initialize the joint information
	InitJointLimits(pVehicleModelInfo);

	ChangeLod(*pParent,VDM_REAL);

	m_VehicleTowArmAudio = (audVehicleTowTruckArm*)pParent->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_TOWTRUCK_ARM);
	audAssert(m_VehicleTowArmAudio);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::AddRopesAndHook(CVehicle & parentVehicle)
{
	//Displayf(":AddRopesAndHook, %p (%p, %p, %p) %d",this,m_RopeInstanceA,m_RopeInstanceB,m_PropObject.Get(),(int)m_bRopesAndHookInitialized);

	if(!m_bIsVisible || !parentVehicle.GetFragInst() || !PHLEVEL->IsInLevel(parentVehicle.GetFragInst()->GetLevelIndex()))
	{
		// Must be in the physics level to add ropes
		return;
	}

	if(m_bRopesAndHookInitialized REPLAY_ONLY(|| CReplayMgr::IsReplayInControlOfWorld()))
	{
		return;
	}

	
	Vec3V rot(V_ZERO);
	Matrix34 matMountA, matMountB, matHook;

	//get the position of the bones at the end of the tow truck arm.
	parentVehicle.GetGlobalMtx(m_iTowMountA, matMountA);
	parentVehicle.GetGlobalMtx(m_iTowMountB, matMountB);


	ropeManager* pRopeManager = CPhysics::GetRopeManager();

	float fRopeLength = parentVehicle.GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeLength : sfTowTruckRopeLength;
	float fRopeMinLength = parentVehicle.GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeMinLength : sfTowTruckRopeMinLength;

	TUNE_INT(TOW_TRUCK_ROPE_SECTIONS, 2, 1, 10, 1);
	TUNE_INT(TOW_TRUCK_2_ROPE_SECTIONS, 2, 1, 10, 1);
	int nTowTruckSections = parentVehicle.GetModelIndex() == MI_CAR_TOWTRUCK_2 ? TOW_TRUCK_2_ROPE_SECTIONS : TOW_TRUCK_ROPE_SECTIONS;

	m_RopeInstanceA = pRopeManager->AddRope( VECTOR3_TO_VEC3V(matMountA.d), rot, fRopeLength, fRopeMinLength, fRopeLength, sfTowTruckRopeLengthChangeRate, siTowTruckRopeType, nTowTruckSections, false, true, 1.0f, false, true );
	m_RopeInstanceB = pRopeManager->AddRope( VECTOR3_TO_VEC3V(matMountB.d), rot, fRopeLength, fRopeMinLength, fRopeLength, sfTowTruckRopeLengthChangeRate, siTowTruckRopeType, nTowTruckSections, false, true, 1.0f, false, true );

	Assert( m_RopeInstanceA );
	Assert( m_RopeInstanceB );
	if( m_RopeInstanceA )
	{
		m_RopeInstanceA->SetNewUniqueId();
		m_RopeInstanceA->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, 0 );
		m_RopeInstanceA->SetSpecial(true);
		m_RopeInstanceA->SetIsTense(true);
#if GTA_REPLAY
			CReplayMgr::RecordPersistantFx<CPacketAddRope>(
				CPacketAddRope(m_RopeInstanceA->GetUniqueID(), VECTOR3_TO_VEC3V(matMountA.d), rot, fRopeLength, fRopeMinLength, fRopeLength, sfTowTruckRopeLengthChangeRate, siTowTruckRopeType, nTowTruckSections, false, true, 1.0f, false, true),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceA),
				NULL, 
				true);
#endif
	}
	if( m_RopeInstanceB )
	{
		m_RopeInstanceB->SetNewUniqueId();
		m_RopeInstanceB->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, 0 );
		m_RopeInstanceB->SetSpecial(true);
		m_RopeInstanceB->SetIsTense(true);
#if GTA_REPLAY
			CReplayMgr::RecordPersistantFx<CPacketAddRope>(
				CPacketAddRope(m_RopeInstanceB->GetUniqueID(), VECTOR3_TO_VEC3V(matMountB.d), rot, fRopeLength, fRopeMinLength, fRopeLength, sfTowTruckRopeLengthChangeRate, siTowTruckRopeType, nTowTruckSections, false, true, 1.0f, false, true),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceB),
				NULL, 
				true);
#endif
	}

    u32 nModelIndex = MI_PROP_HOOK;
	fwModelId hookModelId((strLocalIndex(nModelIndex)));
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(hookModelId);
	if(pModelInfo==NULL)
		return;

    if(m_PropObject == NULL)
    {
        m_PropObject = CObjectPopulation::CreateObject(hookModelId, ENTITY_OWNEDBY_GAME, true, true, false);
    }

	if(IsPickUpFromTopVehicle(&parentVehicle))
	{
		matHook = matMountA;
		matHook.d = (matMountA.d + matMountB.d) * 0.5f;
		if(m_PropObject)
		{
			matHook.d.z -= m_PropObject->GetBoundRadius() * 2.0f;
		}
	}
	else
	{
		matHook = matMountA;
		matHook.d.z -= fRopeLength;
	}

	if(m_PropObject)
	{
		m_PropObject->SetMatrix(matHook, true, true, true);
		CGameWorld::Add(m_PropObject, CGameWorld::OUTSIDE );

		// Broken object created so tell the replay it should record it
		REPLAY_ONLY(CReplayMgr::RecordObject(m_PropObject));
	}

#if __ASSERT
	strLocalIndex txdSlot = g_TxdStore.FindSlot(ropeDataManager::GetRopeTxdName());
	if (txdSlot != -1)
	{
		Assertf(CStreaming::HasObjectLoaded(txdSlot, g_TxdStore.GetStreamingModuleId()), "Rope txd not streamed in for vehicle %s", parentVehicle.GetModelName());
	}
#endif // __ASSERT
	
	m_bRopesAndHookInitialized = true;
}

//////////////////////////////////////////////////////////////////////////

int CVehicleGadgetTowArm::GetGadgetEntities(int entityListSize, const CEntity ** entityListOut) const
{
	if(entityListSize>0 && m_PropObject)
	{
		entityListOut[0] = m_PropObject;
		return 1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::WarpToPosition(CVehicle* pTowTruck, bool bUpdateConstraints)
{
	if(m_PropObject)
	{
		Matrix34 matHook;

		if(IsPickUpFromTopVehicle(pTowTruck))
		{
			Matrix34 matMountA, matMountB;

			//get the position of the bones at the end of the tow truck arm.
			pTowTruck->GetGlobalMtx(m_iTowMountA, matMountA);
			pTowTruck->GetGlobalMtx(m_iTowMountB, matMountB);

			matHook = matMountA;
			matHook.d = (matMountA.d + matMountB.d) * 0.5f;
			if(m_PropObject)
			{
				matHook.d.z -= m_PropObject->GetBoundRadius() * 2.0f;
			}
		}
		else
		{
			float fRopeLength = pTowTruck->GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeLength : sfTowTruckRopeLength;
			pTowTruck->GetGlobalMtx(m_iTowMountA, matHook);
			matHook.d.z -= fRopeLength;
		}

		if(m_PropObject->GetIsAttached())
		{
			m_PropObject->GetAttachmentExtension()->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true);
		}

		m_PropObject->SetMatrix(matHook, true, true, true);

		if(m_PropObject->GetIsAttached())
		{
			m_PropObject->GetAttachmentExtension()->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
		}

		if(m_Setup)
		{
			if(bUpdateConstraints)
			{
				UpdateConstraintDistance(pTowTruck);
			}
		}
	}
}

void CVehicleGadgetTowArm::Teleport(CVehicle *pTowTruck, const Matrix34 &matOld)
{
	SetIsTeleporting(true);
	if(m_pTowedVehicle)
	{
		Matrix34 matAttachedVehicle = MAT34V_TO_MATRIX34(GetAttachedVehicle()->GetMatrix());
		Matrix34 matOldTowTruckInverse = matOld;
		matOldTowTruckInverse.Inverse();
		matAttachedVehicle.Dot(matOldTowTruckInverse);
		matAttachedVehicle.Dot(MAT34V_TO_MATRIX34(pTowTruck->GetMatrix()));
	    CPhysical *pPhyTowedVehicle = m_pTowedVehicle;
	    CVehicle *pTowedVeh = static_cast<CVehicle*>(pPhyTowedVehicle);

		bool bAttachedChildIsParent = false;
		CVehicle *parent = pTowTruck;
		while(parent)
		{
			if(pTowedVeh == parent)
			{
				bAttachedChildIsParent = true;
				break;
			}
			parent = parent->GetAttachParentVehicle();
		}

		if(bAttachedChildIsParent)
		{
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::Teleport - Detaching entity because bAttachedChildIsParent.");
			DetachVehicle(pTowedVeh, true, true);
		}
		else
		{
			Matrix34 matOld = MAT34V_TO_MATRIX34(pTowedVeh->GetMatrix());
			pTowedVeh->TeleportWithoutUpdateGadgets(matAttachedVehicle.d, camFrame::ComputeHeadingFromMatrix(matAttachedVehicle), true, true);
			pTowedVeh->UpdateGadgetsAfterTeleport(matOld, false); // do not detach from gadget
			UpdateConstraintDistance(pTowTruck);
		}

	}
	else
	{
		WarpToPosition(pTowTruck);
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::BlowUpCarParts(CVehicle* pParent)
{
    //If the tow truck is being destroyed disconnect and delete the hook and ropes.
    DeleteRopesHookAndConstraints(pParent);
}

//////////////////////////////////////////////////////////////////////////

bool CVehicleGadgetTowArm::WantsToBeAwake(CVehicle *pVehicle)
{
	if(!m_bRopesAndHookInitialized)
	{
		return false;
	}

	static dev_float sfHookSpeedSqMinToStayAwake = 0.1f;
	//Need to check hook speed so playback optimizations don't try to deactivate us
	if(m_PropObject && m_PropObject->GetVelocity().Mag2() > sfHookSpeedSqMinToStayAwake)
	{
		return true;
	}

	return CVehicleGadgetWithJointsBase::WantsToBeAwake(pVehicle);
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::ChangeLod(CVehicle & parentVehicle, int lod)
{
	if(lod==VDM_REAL || lod==VDM_DUMMY)
	{
		if(!m_bRopesAndHookInitialized)
		{
			AddRopesAndHook(parentVehicle);
		}
	}
	else // VDM_SUPERDUMMY
	{
		if(!GetAttachedVehicle() && m_bRopesAndHookInitialized)
		{
			RemoveRopesAndHook();
		}
	}
}

void CVehicleGadgetTowArm::SetIsVisible(CVehicle *pParent, bool bIsVisible)
{
	m_bIsVisible = bIsVisible;
	if(bIsVisible && !m_bRopesAndHookInitialized)
	{
		AddRopesAndHook(*pParent);
	}
	else if(!bIsVisible && m_bRopesAndHookInitialized)
	{
		if(GetAttachedVehicle() && !pParent->IsNetworkClone())
		{
			ActuallyDetachVehicle(pParent, true);
		}

		if(!GetAttachedVehicle())
		{
			RemoveRopesAndHook();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::SetDesiredPitchRatio( CVehicle *pVehicle, float desiredPitchRatio)
{
	if(!m_bRopesAndHookInitialized)
	{
		AddRopesAndHook(*pVehicle);
	}

	// Don't allow the tow arm to be moved when above a certain speed
    if(!m_pTowedVehicle || pVehicle->GetVelocity().Mag2() < sfVehicleSpeedSqrToEnableSphericalConstraintAndArmMovement)
    {
		if(HasTowArm())
		{
			float maxAngleDisplacement = GetJoint(0)->m_1dofJointInfo.m_fOpenAngle - GetJoint(0)->m_1dofJointInfo.m_fClosedAngle;

			//this joint appears to be backwards so go from the open angle
			GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = GetJoint(0)->m_1dofJointInfo.m_fOpenAngle - maxAngleDisplacement * desiredPitchRatio;
		}
    }
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::SetDesiredPitch( CVehicle *pVehicle, float desiredPitch)
{
	if(!m_bRopesAndHookInitialized)
	{
		AddRopesAndHook(*pVehicle);
	}

	// Don't allow the tow arm to be moved when above a certain speed
    if(!m_pTowedVehicle || pVehicle->GetVelocity().Mag2() < sfVehicleSpeedSqrToEnableSphericalConstraintAndArmMovement)
    {
		if(HasTowArm())
		{
			GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = desiredPitch;
		}
    }
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::SetDesiredAcceleration( CVehicle *pVehicle, float acceleration)
{
	if(!m_bRopesAndHookInitialized)
	{
		AddRopesAndHook(*pVehicle);
	}

	// Don't allow the tow arm to be moved when above a certain speed
    if(!m_pTowedVehicle || pVehicle->GetVelocity().Mag2() < sfVehicleSpeedSqrToEnableSphericalConstraintAndArmMovement)
    {
		if(HasTowArm())
		{
			GetJoint(0)->m_fDesiredAcceleration = acceleration;
		}
    }
}

#if GTA_REPLAY
void CVehicleGadgetTowArm::SnapToAngleForReplay(float snapToAngle)
{
	if(HasTowArm())
	{
		GetJoint(0)->m_1dofJointInfo.m_bSnapToAngle = true;
		GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = snapToAngle;
	}
}
#endif //GTA_REPLAY

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::DetachVehicle(CVehicle *pTowTruck, bool bReattachHook, bool bImmediateDetach )
{ 
	// do the detachment now if the tow truck is fixed (ProcessPhysics will not be called)
	if (pTowTruck && (pTowTruck->GetIsAnyFixedFlagSet() || bImmediateDetach))
	{
		ActuallyDetachVehicle(pTowTruck, bReattachHook);
	}
	else
	{
		m_bDetachVehicle = true;
	}

#if GTA_REPLAY
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		if( m_RopeInstanceA )
		{
			m_RopeInstanceA->SetIsTense(true);
			phVerletCloth* pClothA = m_RopeInstanceA->GetEnvCloth()->GetCloth();
			Assert( pClothA );
			pClothA->DetachVirtualBound();

		}
		if( m_RopeInstanceB )
		{
			m_RopeInstanceB->SetIsTense(true);
			phVerletCloth* pClothB = m_RopeInstanceB->GetEnvCloth()->GetCloth();
			Assert( pClothB );
			pClothB->DetachVirtualBound();
		}
	}

	vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::DetachVehicle - Detaching vehicle");
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::ActuallyDetachVehicle( CVehicle *pTowTruck, bool bReattachHook )
{   
    //set the vehicle as detached 
    if(m_pTowedVehicle)
    {
        CPhysical *pPhyTowedVehicle = m_pTowedVehicle;
        CVehicle *pTowedVeh = static_cast<CVehicle*>(pPhyTowedVehicle);

		m_pTowedVehicle->DetachFromParent(0);

        //disable extra iterations
        if(pTowTruck->GetCurrentPhysicsInst())
        {
            pTowTruck->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
        }

        if(pTowedVeh->GetCurrentPhysicsInst())
        {
            pTowedVeh->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
        }

        //if there's no driver then just set the no driver task as abandoned
        if(!pTowedVeh->GetDriver())
        {
            //set the no driver task, to set the control inputs
			if (!pTowedVeh->IsNetworkClone())
			{
				pTowedVeh->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, 
					rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_ABANDONED), 
					VEHICLE_TASK_PRIORITY_PRIMARY, 
					false);
			}
        }

		audVehicleAudioEntity* audioEntity = pTowedVeh->GetVehicleAudioEntity();
		if(audioEntity)
		{
			audioEntity->TriggerTowingHookDetachSound();
		}

		if(m_pTowedVehicle->GetCurrentPhysicsInst())
		{
			phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(m_pTowedVehicle->GetCurrentPhysicsInst()->GetArchetype());
			pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, m_vTowedVehicleOriginalAngDamp);
		}
    
		m_pTowedVehicle->SetAllowFreezeWaitingOnCollision(true);
        m_pTowedVehicle = NULL;
    }

    m_uIgnoreAttachTimer = ms_uAttachResetTime;	

    if(bReattachHook)
    {
        //reattach the ropes to the hook, this also detaches the ropes from the vehicle
        AttachRopesToObject( pTowTruck, m_PropObject, 0, bReattachHook );
    }

}

//////////////////////////////////////////////////////////////////////////
const CVehicle* CVehicleGadgetTowArm::GetAttachedVehicle() const
{
    CPhysical *pPhyTowedVehicle = m_pTowedVehicle;
    CVehicle *pTowedVeh = static_cast<CVehicle*>(pPhyTowedVehicle);

    return pTowedVeh;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleGadgetTowArm::AttachVehicleToTowArm( CVehicle *pTowTruck, CVehicle *pVehicleToAttach, const Vector3 &vAttachPosition, s32 iComponentIndex )
{
    //convert the attach position into an offset position in the vehicle we want to connects to space.
    Vector3 vLocalAttachPosition(vAttachPosition);
    Matrix34 matVehicle = MAT34V_TO_MATRIX34(pVehicleToAttach->GetMatrix());
    matVehicle.Inverse();
    matVehicle.Transform(vLocalAttachPosition);

    AttachVehicleToTowArm( pTowTruck, pVehicleToAttach, -1, vLocalAttachPosition, iComponentIndex );
} 


#if RECORDING_VERTS
namespace rage
{
	extern dbgRecords<recordLine> g_DbgRecordLines;
	extern dbgRecords<recordSphere> g_DbgRecordSpheres;
	extern dbgRecords<recordTriangle> g_DbgRecordTriangles;
	extern dbgRecords<recordCapsule> g_DbgRecordCapsules;
	extern bool g_EnableRecording;
}
using namespace rage;
#endif

static dev_float sfBBHookStartAllowance = 0.10f;
static dev_float sfBoundingBoxOffset = 0.08f;

//////////////////////////////////////////////////////////////////////////
//Passing in a valid bone index and valid component index for different parts of the vehicle may create undesireable results
void CVehicleGadgetTowArm::AttachVehicleToTowArm( CVehicle *pTowTruck, CVehicle *pVehicleToAttach, s16 vehicleBoneIndex, const Vector3 &vVehicleOffsetPosition, s32 iComponentIndex  )
{
	if(m_pTowedVehicle == pVehicleToAttach )
	{
		return;
	}
	
	if( NetworkInterface::IsGameInProgress() &&
		pVehicleToAttach &&
		pVehicleToAttach->GetIsTypeVehicle() )
	{
		if( pVehicleToAttach->GetIsBeingTowed() ||
			pVehicleToAttach->GetIsTowing() )
		{
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachVehicleToTowArm - Can't attach to vehicle that is already being towed or towing to something.");
			return;
		}
	}

	// if we are already attached to a vehicle detach it first
	if( m_pTowedVehicle )
	{
		vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachVehicleToTowArm - Already had an attached vehicle so detaching it");
		ActuallyDetachVehicle( pTowTruck );
	}

	if(!m_bRopesAndHookInitialized)
	{
		AddRopesAndHook(*pTowTruck);
	}

	UpdateTowedVehiclePosition(pTowTruck, pVehicleToAttach);

	if(pVehicleToAttach->GetIsAnyFixedFlagSet())
	{
		pVehicleToAttach->SetFixedPhysics(false, NetworkInterface::IsGameInProgress());
	}
	pVehicleToAttach->SetAllowFreezeWaitingOnCollision(false);

    //get the absolute position of the point
    Vector3 vVehicleAbsolutePosition(vVehicleOffsetPosition);
    Matrix34 matVehicle;
    //if we've been passed in a bone get the matrix for it, otherwise use the vehicle matrix
    if(vehicleBoneIndex > -1)
    {
        // What to do if global matrices aren't up to date?
        pVehicleToAttach->GetGlobalMtx(vehicleBoneIndex, matVehicle);
    }
    else
    {
        matVehicle = MAT34V_TO_MATRIX34(pVehicleToAttach->GetMatrix());
    }

    matVehicle.Transform(vVehicleAbsolutePosition);

    // Lookup hook position
    Matrix34 matHook = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());

    //get the position of the hook point(vaguely)
    Matrix34 hookPointPositionMatrix;
    hookPointPositionMatrix = m_PropObject->GetLocalMtx(siHookPointBoneIndex);

    //get the absolute position of the hook point
    Vector3 hookPointPostion(hookPointPositionMatrix.d); 
    matHook.Transform(hookPointPostion);

    //move the hook so it contacts the vehicle.
    Vector3 distanceFromContact = (vVehicleAbsolutePosition - hookPointPostion);
    matHook.d += distanceFromContact;

    m_PropObject->SetMatrix(matHook, true, true);

    //attach the ropes directly to the vehicle
    //Get the component of the vehicle we are trying to attach to
    s32 iComponentIndexToUse = iComponentIndex;
    //if the component index has not been specified but a bone has, get the component index from the bone
    if(iComponentIndexToUse == 0 && vehicleBoneIndex != -1)
    {
        CVehicleModelInfo* pVehicleModelInfo = pVehicleToAttach->GetVehicleModelInfo();
        if(pVehicleModelInfo && pVehicleModelInfo->GetDrawable() && pVehicleModelInfo->GetDrawable()->GetSkeletonData())
        {
            iComponentIndexToUse = pVehicleModelInfo->GetFragType()->GetComponentFromBoneIndex(0, vehicleBoneIndex);
        }
    }

    //make sure we have a valid component
    if(iComponentIndexToUse == -1)
    {
        iComponentIndexToUse = 0;
    }



    //mark the vehicle as being towed
    m_pTowedVehicle = pVehicleToAttach;

    Vector3 hookVehicleOffset(matHook.d);
    matVehicle.Inverse();
    matVehicle.Transform(hookVehicleOffset);

	//calculate the start position of the hook and the final desireced postion
    if(m_pTowedVehicle)
    {
        m_fTowTruckPositionTransitionTime = 0.0f;

        Matrix34 matTowedVehcileMatrix = MAT34V_TO_MATRIX34(m_pTowedVehicle->GetMatrix());
        Matrix34 hookMatrix = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());

        matTowedVehcileMatrix.UnTransform(hookMatrix.d);

        m_vStartTowHookPosition = hookMatrix.d;
        m_vCurrentTowHookPosition = hookMatrix.d;

		if(IsPickUpFromTopVehicle(pTowTruck))
		{
			m_vDesiredTowHookPosition = VEC3_ZERO;
			m_vDesiredTowHookPosition.z = m_pTowedVehicle->GetBoundingBoxMax().z - sfBoundingBoxOffset;
		}
		else
		{
			SetDesiredHookPositionWithDamage(pVehicleToAttach, hookVehicleOffset.y, hookPointPositionMatrix.d.z);
		}

		m_vCurrentTowHookPosition  = m_vStartTowHookPosition;
    }

	AttachRopesToObject( pTowTruck, pVehicleToAttach, iComponentIndexToUse);

    //attach the hook directly to the vehicle
    //work out the correct hook rotation
    Quaternion quatRotation;
    Vector3 normalisedDirection = m_vCurrentTowHookPosition;
    normalisedDirection.Normalize();
    quatRotation.FromVectors(YAXIS, normalisedDirection, ZAXIS);


    m_PropObject->AttachToPhysicalBasic( pVehicleToAttach, vehicleBoneIndex, ATTACH_STATE_BASIC, &hookVehicleOffset, &quatRotation );


    //if there's no driver turn off the handbrake and brakes.
    if(!pVehicleToAttach->GetDriver())
    {
        //set the no driver task, to set the control inputs
		if (!pVehicleToAttach->IsNetworkClone())
		{
			pVehicleToAttach->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, 
                                                                        rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_TOWED), 
                                                                        VEHICLE_TASK_PRIORITY_PRIMARY, 
                                                                        false);
		}
    }


	//Setup the attachment extension 
	u16 uAttachFlags = ATTACH_STATE_PHYSICAL | ATTACH_FLAG_POS_CONSTRAINT | ATTACH_FLAG_DO_PAIRED_COLL;

	fwAttachmentEntityExtension *extension = pVehicleToAttach->CreateAttachmentExtensionIfMissing();
	extension->SetParentAttachment(pVehicleToAttach, pTowTruck); //set the tow truck as the parent
	extension->SetAttachFlags(uAttachFlags);
	extension->SetMyAttachBone(static_cast<s16>(iComponentIndex)); // bit of a hack! Store the component index in the attach bone (we can't get the bone from the comp index here)
	extension->SetAttachOffset(vVehicleOffsetPosition);

   //make sure the tow truck doesn't turn into a dummy
    pTowTruck->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;

	audVehicleAudioEntity* audioEntity = pVehicleToAttach->GetVehicleAudioEntity();
	if(audioEntity)
	{
		audioEntity->TriggerTowingHookAttachSound();
	}

	if(m_pTowedVehicle->GetCurrentPhysicsInst())
	{
		phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(m_pTowedVehicle->GetCurrentPhysicsInst()->GetArchetype());
		m_vTowedVehicleOriginalAngDamp = pArchetypeDamp->GetDampingConstant(phArchetypeDamp::ANGULAR_V);
	}

// NOTE: generate virtual bounds and attach to each rope
	if( m_RopeInstanceA && m_RopeInstanceB )
	{
		const phBoundComposite* pBoundCompOrig = static_cast<const phBoundComposite*>(pVehicleToAttach->GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds());
		const int iBoneIndexChassisLowLod = pVehicleToAttach->GetBoneIndex(VEH_CHASSIS_LOWLOD);
		if(iBoneIndexChassisLowLod!=-1)
		{
			int iComponent = pVehicleToAttach->GetVehicleFragInst()->GetComponentFromBoneIndex(iBoneIndexChassisLowLod);
			if( iComponent > -1 )
			{
// NOTE: always keep the rope tense ... for now
//				m_RopeInstanceA->SetIsTense(false);
//				m_RopeInstanceB->SetIsTense(false);

				Vec3V ropeVtx0 = m_RopeInstanceA->GetRopeFirstVtxWorldPosition();
				Vec3V ropeVtx1 = m_RopeInstanceA->GetRopeLastVtxWorldPosition();

				Vec3V ropeCenter = Scale( Add( ropeVtx0, ropeVtx1 ), ScalarV(V_HALF));
				Vec3V halfExtend = Scale( Subtract( ropeVtx0, ropeCenter ), ScalarVFromF32(1.5f) );
				ScalarV ropeRadSqr = Dot( halfExtend, halfExtend );

#if RECORDING_VERTS
				if( g_EnableRecording )
				{
					recordSphere rec;
					rec.m_Center = ropeCenter;
					rec.m_Radius = MagFast(halfExtend).Getf();
					g_DbgRecordSpheres.Push( rec );
				}
#endif

				Assert( pBoundCompOrig );
				const phBound* lodBound = pBoundCompOrig->GetBound(iComponent);
				Assert( lodBound );
				Assert( lodBound->GetType() == phBound::GEOMETRY );
				const phBoundGeometry* bound = (const phBoundGeometry*)lodBound;			
				const int numPolygons = bound->GetNumPolygons();
				Assert( numPolygons > 0 );

				Vec3V* vertsA = Alloca( Vec3V, numPolygons );
				Assert( vertsA );
				Vec3V* vertsB = Alloca( Vec3V, numPolygons );
				Assert( vertsB );
				Vec3V* vertsC = Alloca( Vec3V, numPolygons );
				Assert( vertsC );
				int intersectedPolygonsCount = 0;
				
				const float errorF = 0.2f; 
				ScalarV _ErrThreshold = ScalarVFromF32(errorF);
				Mat34V_ConstRef vehMatrix = pVehicleToAttach->GetCurrentPhysicsInst()->GetMatrix();
				Matrix34 vehMatrixInv = MAT34V_TO_MATRIX34(vehMatrix);
				vehMatrixInv.FastInverse();

				for( int polygonIndex = 0; polygonIndex < numPolygons; ++polygonIndex )
				{
					const phPolygon& polygon = bound->GetPolygon(polygonIndex);

					const int triVertIndex0 = polygon.GetVertexIndex(0);
					const int triVertIndex1 = polygon.GetVertexIndex(1);
					const int triVertIndex2 = polygon.GetVertexIndex(2);

	#if COMPRESSED_VERTEX_METHOD == 0
					Vec3V triVert0 = bound->GetVertex(triVertIndex0);
					Vec3V triVert1 = bound->GetVertex(triVertIndex1);
					Vec3V triVert2 = bound->GetVertex(triVertIndex2);
	#else

					Vec3V triVert0 = bound->GetCompressedVertex(triVertIndex0);
					Vec3V triVert1 = bound->GetCompressedVertex(triVertIndex1);
					Vec3V triVert2 = bound->GetCompressedVertex(triVertIndex2);
	#endif

					Vec3V a = Transform(vehMatrix,triVert0);
					Vec3V b = Transform(vehMatrix,triVert1);
					Vec3V c = Transform(vehMatrix,triVert2);

					// NOTE: weed out horizontal triangles

					ScalarV tempZ0 = Abs( Subtract( a.GetZ(), b.GetZ() ) );
					ScalarV tempZ1 = Abs( Subtract( a.GetZ(), c.GetZ() ) );
					ScalarV tempZ2 = Abs( Subtract( b.GetZ(), c.GetZ() ) );
					
					BoolV isHorizontal0 = IsLessThan( tempZ0, _ErrThreshold );
					BoolV isHorizontal1 = IsLessThan( tempZ1, _ErrThreshold );
					BoolV isHorizontal2 = IsLessThan( tempZ2, _ErrThreshold );
					
					BoolV isHorizontal = And( isHorizontal0, And(isHorizontal1, isHorizontal2) );
					if( isHorizontal.Getb() )
						continue;

					Vec3V tempVa = Subtract( a, ropeCenter );
					Vec3V tempVb = Subtract( b, ropeCenter );
					Vec3V tempVc = Subtract( c, ropeCenter );

#if RECORDING_VERTS
					if( g_EnableRecording )
					{
//						recordTriangle rec;
//						rec.m_V0 = a;
//						rec.m_V1 = b;
//						rec.m_V2 = c;
//						g_DbgRecordTriangles.Push( rec );

						recordSphere rec;
						rec.m_Radius = 0.05f;
						rec.m_Color = Color_red;

						rec.m_Center = a;
						g_DbgRecordSpheres.Push( rec );

						rec.m_Center = b;
						g_DbgRecordSpheres.Push( rec );

						rec.m_Center = c;
						g_DbgRecordSpheres.Push( rec );
					}
#endif

					ScalarV distSqrA = Dot( tempVa, tempVa );
					ScalarV distSqrB = Dot( tempVb, tempVb );
					ScalarV distSqrC = Dot( tempVc, tempVc );

					BoolV resA = IsLessThan( distSqrA, ropeRadSqr );
					BoolV resB = IsLessThan( distSqrB, ropeRadSqr );
					BoolV resC = IsLessThan( distSqrC, ropeRadSqr );

					BoolV res = resA | resB | resC;

					vertsA[intersectedPolygonsCount] = a;
					vertsB[intersectedPolygonsCount] = b;
					vertsC[intersectedPolygonsCount] = c;

					intersectedPolygonsCount += (int)res.Getb();
				} // for

				if( intersectedPolygonsCount )
				{
					Assert( m_RopeInstanceA->GetEnvCloth() );
					phVerletCloth* pClothA = m_RopeInstanceA->GetEnvCloth()->GetCloth();
					Assert( pClothA );
					Assert( !pClothA->m_VirtualBound );
					pClothA->CreateVirtualBound( intersectedPolygonsCount * 3, &vehMatrix );

					Assert( m_RopeInstanceB->GetEnvCloth() );
					phVerletCloth* pClothB = m_RopeInstanceB->GetEnvCloth()->GetCloth();
					Assert( pClothB );
					Assert( !pClothB->m_VirtualBound );
					pClothB->CreateVirtualBound( intersectedPolygonsCount * 3, &vehMatrix );

					static float capsuleRad = 0.1f;
					ScalarV _half(V_HALF);
					Vec3V _up(V_UP_AXIS_WZERO);
					for( int i = 0; i < intersectedPolygonsCount; ++i )
					{
						Mat34V capsule0Mat, capsule1Mat, capsule2Mat;

						Vec3V a = vertsA[i];
						Vec3V b = vertsB[i];
						Vec3V c = vertsC[i];
						
						Vec3V edge0 = Subtract(a,b);
						Vec3V edge1 = Subtract(a,c);
						Vec3V edge2 = Subtract(b,c);

						Vec3V centerEdge0 = Scale( Add(a, b), _half );
						Vec3V centerEdge1 = Scale( Add(a, c), _half );
						Vec3V centerEdge2 = Scale( Add(b, c), _half );

						ScalarV edge0Mag = MagFast(edge0);
						ScalarV edge1Mag = MagFast(edge1);
						ScalarV edge2Mag = MagFast(edge2);										

						capsule0Mat.SetCol3( centerEdge0 );
						capsule1Mat.SetCol3( centerEdge1 );
						capsule2Mat.SetCol3( centerEdge2 );


	#if RECORDING_VERTS
						if( g_EnableRecording )
						{
							//recordSphere rec;
							//rec.m_Radius = 0.05f;
							//rec.m_Color = Color_green;

							//rec.m_Center = centerEdge0;
							//g_DbgRecordSpheres.Push( rec );

							//rec.m_Center = centerEdge1;
							//g_DbgRecordSpheres.Push( rec );

							//rec.m_Center = centerEdge2;
							//g_DbgRecordSpheres.Push( rec );
						}
	#endif

						Vec3V at0 = NormalizeFast(Subtract(a, centerEdge0));
						Vec3V at1 = NormalizeFast(Subtract(c, centerEdge1));
						Vec3V at2 = NormalizeFast(Subtract(b, centerEdge2));

						Vec3V right0 = Cross(at0, _up);
						Vec3V right1 = Cross(at1, _up);
						Vec3V right2 = Cross(at2, _up);

						Vec3V up0 = Cross(at0, right0);
						Vec3V up1 = Cross(at1, right1);
						Vec3V up2 = Cross(at2, right2);

						capsule0Mat.SetCol1( at0 );
						capsule0Mat.SetCol0( right0 );
						capsule0Mat.SetCol2( up0 );

						capsule1Mat.SetCol1( at1 );
						capsule1Mat.SetCol0( right1 );
						capsule1Mat.SetCol2( up1 );

						capsule2Mat.SetCol1( at2 );
						capsule2Mat.SetCol0( right2 );
						capsule2Mat.SetCol2( up2 );

	#if RECORDING_VERTS
						if( g_EnableRecording )
						{
							recordCapsule rec;
							rec.m_Radius = capsuleRad;
							//rec.m_Color = Color_green;

							rec.m_Transform = capsule0Mat;
							rec.m_Length = edge0Mag.Getf();
							g_DbgRecordCapsules.Push( rec );

							rec.m_Transform = capsule1Mat;
							rec.m_Length = edge1Mag.Getf();
							g_DbgRecordCapsules.Push( rec );

							rec.m_Transform = capsule2Mat;
							rec.m_Length = edge2Mag.Getf();
							g_DbgRecordCapsules.Push( rec );
						}
	#endif

						(RC_MATRIX34(capsule0Mat)).Dot(vehMatrixInv);
						(RC_MATRIX34(capsule1Mat)).Dot(vehMatrixInv);
						(RC_MATRIX34(capsule2Mat)).Dot(vehMatrixInv);

 						pClothA->AttachVirtualBoundCapsule( capsuleRad, edge0Mag.Getf(), capsule0Mat, i*3 );
 						pClothA->AttachVirtualBoundCapsule( capsuleRad, edge1Mag.Getf(), capsule1Mat, i*3+1 );
 						pClothA->AttachVirtualBoundCapsule( capsuleRad, edge2Mag.Getf(), capsule2Mat, i*3+2 );
 
 						pClothB->AttachVirtualBoundCapsule( capsuleRad, edge0Mag.Getf(), capsule0Mat, i*3 );
 						pClothB->AttachVirtualBoundCapsule( capsuleRad, edge1Mag.Getf(), capsule1Mat, i*3+1 );
 						pClothB->AttachVirtualBoundCapsule( capsuleRad, edge2Mag.Getf(), capsule2Mat, i*3+2 );

					} // for
				} // if( intersectedPolygonsCount )

#if RECORDING_VERTS
				if( g_EnableRecording )
				{
					++g_DbgRecordLines.m_Frame;
					++g_DbgRecordTriangles.m_Frame;
					++g_DbgRecordSpheres.m_Frame;
				}
#endif

			} // if( iComponent > -1 )
		} // if(iBoneIndexChassisLowLod!=-1)
	} // if( m_RopeInstanceA && m_RopeInstanceB )

	vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachVehicleToTowArm - Attract entity attached.");

}

void CVehicleGadgetTowArm::SetDesiredHookPositionWithDamage(CVehicle *pVehicleToAttach, float hookVehicleOffsetY, float hookPointPositionZ)
{
	Vector3 vChassisBoundMax = VEC3_ZERO, vChassisBoundMin = VEC3_LARGE_FLOAT;
	float fBoundingBoxOffsetF = sfBoundingBoxOffset, fBoundingBoxOffsetR = sfBoundingBoxOffset;

	const s32 nBoneIndex = pVehicleToAttach->GetBoneIndex(VEH_CHASSIS);
	const int nGroupIndex = pVehicleToAttach->GetVehicleFragInst()->GetGroupFromBoneIndex(nBoneIndex);

	const bool bCoquette2 = MI_CAR_COQUETTE2.IsValid() && (pVehicleToAttach->GetModelIndex() == MI_CAR_COQUETTE2.GetModelIndex());

	//Use the chassis bound min as the full bounding box could be extended by an exhaust mod
	if(nGroupIndex > -1)
	{
		const phBoundComposite* pBoundComp = pVehicleToAttach->GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds();

		const fragPhysicsLOD* physicsLOD = pVehicleToAttach->GetVehicleFragInst()->GetTypePhysics();
		const fragTypeGroup* pGroup = physicsLOD->GetGroup(nGroupIndex);
		u8 iChild = pGroup->GetChildFragmentIndex();

		vChassisBoundMax = pVehicleToAttach->GetBoundingBoxMax();
		Vector3 vBoundingBoxMin = pVehicleToAttach->GetBoundingBoxMin();
		for(int i = 0; i < pGroup->GetNumChildren(); i++)
		{
			if(VEC3V_TO_VECTOR3(pBoundComp->GetBound(iChild+i)->GetBoundingBoxMin()).y < vChassisBoundMin.y)
			{
				vChassisBoundMin = VEC3V_TO_VECTOR3(pBoundComp->GetBound(iChild+i)->GetBoundingBoxMin());
			}
		}

		if(vChassisBoundMin.IsNotEqual(vBoundingBoxMin))
		{
			fBoundingBoxOffsetR = 0.0f;
		}
	}
	else
	{
		vChassisBoundMax = pVehicleToAttach->GetBoundingBoxMax();
		vChassisBoundMin = pVehicleToAttach->GetBoundingBoxMin();
	}

	//Hook from the front
	if(hookVehicleOffsetY > 0.0f)
	{
		//try and make the hook point line up with the height of the bumper
		s32 iBoneIndex = pVehicleToAttach->GetBoneIndex(VEH_BUMPER_F);
		Vector3 vBumperFDefault = VEC3_ZERO, vDamage = VEC3_ZERO;

		if(bCoquette2)// Hack to make the hook look like its touching the car, should really make this data driven.
		{
			static dev_float sfCoquetteHookOffsetF = -0.4f;
			hookPointPositionZ += sfCoquetteHookOffsetF;
			static dev_float sfCoquetteBoundingBoxOffsetF = 0.14f;
			fBoundingBoxOffsetF = sfCoquetteBoundingBoxOffsetF;
		}

		m_vDesiredTowHookPosition.y = vChassisBoundMax.y - fBoundingBoxOffsetF;
		if(iBoneIndex != -1)
		{
			m_vDesiredTowHookPosition.z = pVehicleToAttach->GetObjectMtx(iBoneIndex).d.z - (hookPointPositionZ * 0.5f);
			pVehicleToAttach->GetDefaultBonePosition(iBoneIndex, vBumperFDefault);
		}
		else
		{			
			m_vDesiredTowHookPosition.z = -hookPointPositionZ;
			vBumperFDefault = m_vDesiredTowHookPosition;
		}

		if(pVehicleToAttach->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
		{
			void* pTexLock = pVehicleToAttach->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead);
			if(pTexLock)
			{
				vDamage = VEC3V_TO_VECTOR3(pVehicleToAttach->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pTexLock, VECTOR3_TO_VEC3V(m_vDesiredTowHookPosition)));
				if(vDamage.IsNonZero())
				{
					m_vDesiredTowHookPosition.y += fBoundingBoxOffsetF;
				}

				//Calculate the offset from the bumper and BB max Y and add back to the damaged position
				float fDelta = fabs(vBumperFDefault.y - m_vDesiredTowHookPosition.y); 
				float fYDamaged = vBumperFDefault.y + vDamage.y;
				m_vDesiredTowHookPosition.y = fYDamaged + fDelta;

				pVehicleToAttach->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
			}
		}

		//don't let the hook start in the middle of the bonnet
		if(fabs(m_vStartTowHookPosition.y - vChassisBoundMax.y) > sfBBHookStartAllowance)
		{
			m_vStartTowHookPosition.y = vChassisBoundMax.y - fBoundingBoxOffsetF;
		}
	}
	else
	{
		//try and make the hook point line up with the height of the bumper
		s32 iBoneIndex = pVehicleToAttach->GetBoneIndex(VEH_BUMPER_R);
		Vector3 vBumperRDefault = VEC3_ZERO, vDamage = VEC3_ZERO;

		if(bCoquette2)// Hack to make the hook look like its touching the car, should really make this data driven.
		{
			static dev_float sfCoquetteHookOffsetR = -0.55f;
			hookPointPositionZ += sfCoquetteHookOffsetR;
			static dev_float sfCoquetteBoundingBoxOffsetR = 0.14f;
			fBoundingBoxOffsetR = sfCoquetteBoundingBoxOffsetR;
		}

		m_vDesiredTowHookPosition.y = vChassisBoundMin.y + fBoundingBoxOffsetR;
		if(iBoneIndex != -1)
		{
			m_vDesiredTowHookPosition.z = pVehicleToAttach->GetObjectMtx(iBoneIndex).d.z - (hookPointPositionZ * 0.5f);
			pVehicleToAttach->GetDefaultBonePosition(iBoneIndex, vBumperRDefault);
		}
		else
		{
			m_vDesiredTowHookPosition.z = -hookPointPositionZ;
			vBumperRDefault = m_vDesiredTowHookPosition;
		}

		if(pVehicleToAttach->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
		{
			void* pTexLock = pVehicleToAttach->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead);
			if(pTexLock)
			{
				vDamage = VEC3V_TO_VECTOR3(pVehicleToAttach->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pTexLock, VECTOR3_TO_VEC3V(m_vDesiredTowHookPosition)));
				if(vDamage.IsNonZero())
				{
					m_vDesiredTowHookPosition.y -= fBoundingBoxOffsetR;
				}

				//Calculate the offset from the bumper and BB min Y and move back from the damaged position
				float fDelta = fabs(vBumperRDefault.y - m_vDesiredTowHookPosition.y); 
				float fYDamaged = vBumperRDefault.y + vDamage.y;
				m_vDesiredTowHookPosition.y = fYDamaged - fDelta;

				pVehicleToAttach->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
			}
		}

		//don't let the hook start in the middle of the bonnet
		if(fabs(m_vStartTowHookPosition.y - vChassisBoundMin.y) > sfBBHookStartAllowance)
		{
			m_vStartTowHookPosition.y = vChassisBoundMin.y + fBoundingBoxOffsetR;
		}
	}
}

void CVehicleGadgetTowArm::EnforceConstraintLimits(phConstraintRotation* pConstraint, float fMin, float fMax, float fCurrent, bool bIsPitch)
{
    fMin -= fCurrent;
    fMax -= fCurrent;

    if(bIsPitch)
    {
        fMin = fwAngle::LimitRadianAngleForPitch(fMin);
        fMax = fwAngle::LimitRadianAngleForPitch(fMax);
    }
    else
    {
        fMin = fwAngle::LimitRadianAngle(fMin);
        fMax = fwAngle::LimitRadianAngle(fMax);
    }

    if(fMin > fMax)
    {
        SwapEm(fMin, fMax);
    }

    pConstraint->SetMaxLimit(fMax);
    pConstraint->SetMinLimit(fMin);
}

static dev_float sfForceMultToApplyInConstraint = 0.70f;
static dev_float sfConstraintBreakingStrength = 10000.0f;
static dev_float sfConstraintBreakingStrengthUnderCollision = 3000.0f;

//////////////////////////////////////////////////////////////////////////
void CVehicleGadgetTowArm::AttachRopesToObject( CVehicle* pParent, CPhysical *pObject, s32 UNUSED_PARAM(iComponentIndex)/*= 0*/, bool REPLAY_ONLY(bReattachHook) )
{
	vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachRopesToObject - Begin attaching ropes to %s", pObject ? pObject->GetDebugName() : "NULL");

    if(m_RopeInstanceA && m_RopeInstanceB && m_PropObject && pParent->GetCurrentPhysicsInst() && pObject && pObject->GetCurrentPhysicsInst() && pParent->GetSkeleton())
    {
        //If the bones aren't valid just give up.
        if(m_iTowMountA >= pParent->GetSkeleton()->GetBoneCount() || m_iTowMountB >= pParent->GetSkeleton()->GetBoneCount())
        {
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachRopesToObject - Invalid rope attach bones.");
            return;
        }

        pParent->UpdateSkeleton();

        //detach the rope from the old physics inst if it's still attached
        if( m_RopeInstanceA->GetInstanceA() == m_RopesAttachedVehiclePhysicsInst || m_RopeInstanceA->GetInstanceB() == m_RopesAttachedVehiclePhysicsInst )
        {
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachRopesToObject - Detaching rope A (%p) from object: %p | Index: %i | pParent: %p | pParent->GetCurrentPhysicsInst(): %p | pParent->GetCurrentPhysicsInst() level index: %i | pObject: %p | pObject->GetCurrentPhysicsInst(): %p | pObject->GetCurrentPhysicsInst(): %i",
				m_RopeInstanceA,
				m_RopesAttachedVehiclePhysicsInst, m_RopesAttachedVehiclePhysicsInst ? m_RopesAttachedVehiclePhysicsInst->GetLevelIndex() : -1, pParent,
				pParent->GetCurrentPhysicsInst(), pParent->GetCurrentPhysicsInst()->GetLevelIndex(), 
				pObject, pObject->GetCurrentPhysicsInst(), pObject->GetCurrentPhysicsInst()->GetLevelIndex()
				);

            m_RopeInstanceA->DetachFromObject(m_RopesAttachedVehiclePhysicsInst);
        }

        if( m_RopeInstanceB->GetInstanceA() == m_RopesAttachedVehiclePhysicsInst || m_RopeInstanceB->GetInstanceB() == m_RopesAttachedVehiclePhysicsInst )
        {
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachRopesToObject - Detaching rope B (%p) from object: %p | Index: %i | pParent: %p | pParent->GetCurrentPhysicsInst(): %p | pParent->GetCurrentPhysicsInst() level index: %i | pObject: %p | pObject->GetCurrentPhysicsInst(): %p | pObject->GetCurrentPhysicsInst(): %i",
				m_RopeInstanceA,
				m_RopesAttachedVehiclePhysicsInst, m_RopesAttachedVehiclePhysicsInst ? m_RopesAttachedVehiclePhysicsInst->GetLevelIndex() : -1, pParent,
				pParent->GetCurrentPhysicsInst(), pParent->GetCurrentPhysicsInst()->GetLevelIndex(), 
				pObject, pObject->GetCurrentPhysicsInst(), pObject->GetCurrentPhysicsInst()->GetLevelIndex()
				);

            m_RopeInstanceB->DetachFromObject(m_RopesAttachedVehiclePhysicsInst);
        }

        //remove the constraint if we have any
        if(m_posConstraintHandle.IsValid())
        {
            PHCONSTRAINT->Remove(m_posConstraintHandle);
            m_posConstraintHandle.Reset();
        }

        if(m_sphericalConstraintHandle.IsValid())
        {
            PHCONSTRAINT->Remove(m_sphericalConstraintHandle);
            m_sphericalConstraintHandle.Reset();
        }

        if(m_rotYConstraintHandle.IsValid())
        {
            PHCONSTRAINT->Remove(m_rotYConstraintHandle);
            m_rotYConstraintHandle.Reset();
        }


        //detach the tow truck from anything it is attached to
        pParent->DetachFromParent(0);
        //make sure the prop is detached
        m_PropObject->DetachFromParent(0);

		UpdateHookPosition(pParent, true);

        //get the position of the bones at the end of the tow truck arm.
        Matrix34 TruckMatrix;
        Matrix34 matLocalMountA, matMountA;
        Matrix34 matLocalMountB, matMountB;
        TruckMatrix = MAT34V_TO_MATRIX34(pParent->GetMatrix());

        matMountA = matLocalMountA = pParent->GetObjectMtx(m_iTowMountA);
        TruckMatrix.Transform(matMountA.d);

        matMountB = matLocalMountB = pParent->GetObjectMtx(m_iTowMountB);
        TruckMatrix.Transform(matMountB.d);

        //get the position of where the ropes should attach from the hook. Using local matrices as the globals are probably out of date.
        Matrix34 hookMatrix;
        Matrix34 hookRopeConnectionPointMatrixA;
        Matrix34 hookRopeConnectionPointMatrixB;
        hookMatrix = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());

        if(m_PropObject && m_PropObject->GetCurrentPhysicsInst() && m_PropObject->GetSkeleton() && 
		   siHookRopeAttachmentBoneIndexA < m_PropObject->GetSkeleton()->GetBoneCount() &&
		   siHookRopeAttachmentBoneIndexB < m_PropObject->GetSkeleton()->GetBoneCount())
        {
            hookRopeConnectionPointMatrixA = m_PropObject->GetObjectMtx(siHookRopeAttachmentBoneIndexA);
            hookRopeConnectionPointMatrixB = m_PropObject->GetObjectMtx(siHookRopeAttachmentBoneIndexB);
            hookMatrix.Transform(hookRopeConnectionPointMatrixA.d);
            hookMatrix.Transform(hookRopeConnectionPointMatrixB.d);
        }
        else
        {
            hookRopeConnectionPointMatrixA = hookMatrix;
            hookRopeConnectionPointMatrixB = hookMatrix;
        }

		vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachRopesToObject - matMountA.d: %f, %f, %f | matMountB.d:  %f, %f, %f | hookMatrix.d:  %f, %f, %f | hookRopeConnectionPointMatrixA.d:  %f, %f, %f | hookRopeConnectionPointMatrixB.d:  %f, %f, %f",
			matMountA.d.x, matMountA.d.y, matMountA.d.z,
			matMountB.d.x, matMountB.d.y, matMountB.d.z,
			hookMatrix.d.x, hookMatrix.d.y, hookMatrix.d.z,
			hookRopeConnectionPointMatrixA.d.x, hookRopeConnectionPointMatrixA.d.y, hookRopeConnectionPointMatrixA.d.z,
			hookRopeConnectionPointMatrixB.d.x, hookRopeConnectionPointMatrixB.d.y, hookRopeConnectionPointMatrixB.d.z
			);

        

        // Change the amount of force applied in the constraint depending on the mass of the tow truck and the vehicle being towed, to try and improve handling
        float forceToApply = 1.0f - sfForceMultToApplyInConstraint * rage::Clamp( (pObject->GetMass()/pParent->GetMass()) -0.1f , 0.0f, 1.0f);
        m_fForceMultToApplyInConstraint = rage::Clamp(forceToApply, 0.0f, 1.0f);

        int iTowArmComponent = 0;
		
		if(HasTowArm())
		{
			iTowArmComponent = m_JointInfo[0].m_iFragChild;
		}
        // Dummies bounds aren't composites so just connect to component 0
        if(pParent->IsDummy())
        {
            iTowArmComponent = 0;
        }

		float fRopeLength = pParent->GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeLength : sfTowTruckRopeLength;

		vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachRopesToObject - fRopeLength: %f", fRopeLength);

        //finally attach the ropes to the object
        m_RopeInstanceA->AttachObjects( VECTOR3_TO_VEC3V(matMountA.d),
            VECTOR3_TO_VEC3V(hookRopeConnectionPointMatrixA.d),
            pParent->GetCurrentPhysicsInst(),
            pObject->GetCurrentPhysicsInst(), 
            iTowArmComponent,//use the tow arm as the component
            0,//iComponentIndex, // always connect to component 0 for now
            fRopeLength,
            sfAllowedConstraintPenetration,
            m_fForceMultToApplyInConstraint, 1.0f, true);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && m_PropObject == pObject)
		{
			CReplayMgr::RecordPersistantFx<CPacketAttachEntitiesToRope>(
				CPacketAttachEntitiesToRope( m_RopeInstanceA->GetUniqueID(), VECTOR3_TO_VEC3V(matMountA.d), VECTOR3_TO_VEC3V(hookRopeConnectionPointMatrixA.d), iTowArmComponent, 0, fRopeLength, sfAllowedConstraintPenetration, m_fForceMultToApplyInConstraint, 1.0f, true, NULL, NULL, 1, bReattachHook),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceA),
				pParent,
				pObject,
				false);
		}
#endif

        phConstraintDistance* pRopePosConstraint = static_cast<phConstraintDistance*>( m_RopeInstanceA->GetConstraint() );
        if(pRopePosConstraint && !pParent->IsNetworkClone())
        {
            pRopePosConstraint->SetBreakable(true, sfConstraintBreakingStrength);
        }

        m_RopeInstanceB->AttachObjects( VECTOR3_TO_VEC3V(matMountB.d),
            VECTOR3_TO_VEC3V(hookRopeConnectionPointMatrixB.d),
            pParent->GetCurrentPhysicsInst(),
            pObject->GetCurrentPhysicsInst(), 
            iTowArmComponent,//use the tow arm as the component
            0,//iComponentIndex,// always connect to component 0 for now
            fRopeLength,
            sfAllowedConstraintPenetration,
            m_fForceMultToApplyInConstraint, 1.0f, true);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && m_PropObject == pObject)
		{
			CReplayMgr::RecordPersistantFx<CPacketAttachEntitiesToRope>(
				CPacketAttachEntitiesToRope( m_RopeInstanceB->GetUniqueID(), VECTOR3_TO_VEC3V(matMountB.d), VECTOR3_TO_VEC3V(hookRopeConnectionPointMatrixB.d), iTowArmComponent, 0, fRopeLength, sfAllowedConstraintPenetration, m_fForceMultToApplyInConstraint, 1.0f, true, NULL, NULL, 2, bReattachHook),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceB),
				pParent,
				pObject,
				false);
		}
#endif
        pRopePosConstraint = static_cast<phConstraintDistance*>( m_RopeInstanceB->GetConstraint() );
        if(pRopePosConstraint && !pParent->IsNetworkClone())
        {
            pRopePosConstraint->SetBreakable(true, sfConstraintBreakingStrength);
        }

        //keep a record of the physinst we attached to, so if it changes we can reattach to the new one.
        m_RopesAttachedVehiclePhysicsInst = pParent->GetCurrentPhysicsInst();

        //Prevent an attached object from moving around too much
        if(!m_posConstraintHandle.IsValid() && !IsPickUpFromTopVehicle(pParent))
        {
            phConstraintDistance::Params posConstraintParams;
            posConstraintParams.instanceA = pParent->GetCurrentPhysicsInst();
            posConstraintParams.instanceB = pObject->GetCurrentPhysicsInst();
            posConstraintParams.componentA = 0;
            posConstraintParams.componentB = 0;//static_cast<u16>(iComponentIndex);// always connect to component 0 for now
            posConstraintParams.breakingStrength = sfConstraintBreakingStrength;
            posConstraintParams.breakable = true;

            Vector3 vecSpringoffset;
            GetTowHookIdealPosition( pParent, vecSpringoffset, GetTowArmPositionRatio() );

            //Matrix34 objectMatrix = MAT34V_TO_MATRIX34(pObject->GetMatrix());

            MAT34V_TO_MATRIX34(pObject->GetMatrix()).UnTransform(hookMatrix.d);
            hookMatrix.d.x = 0.0f;

			//Interpolate to the desired constraint position in UpdateConstraintDistance
			if(m_pTowedVehicle)
			{
				m_vLocalPosConstraintStart = VECTOR3_TO_VEC3V(hookMatrix.d);
				posConstraintParams.localPosA = m_vLocalPosConstraintStart;
				posConstraintParams.localPosB = m_vLocalPosConstraintStart;
			}
			else
			{
				posConstraintParams.localPosA = VECTOR3_TO_VEC3V(vecSpringoffset);
				posConstraintParams.localPosB = VECTOR3_TO_VEC3V(hookMatrix.d);
			}

            posConstraintParams.constructUsingLocalPositions = true;

            posConstraintParams.massInvScaleA = m_fForceMultToApplyInConstraint;

            m_posConstraintHandle = PHCONSTRAINT->Insert(posConstraintParams);
            Assertf(m_posConstraintHandle.IsValid(), "Failed to create tow arm constraint"); 
        }

        m_bUsingSphericalConstraint = false;


        if(pObject != m_PropObject)
        {
            //if we are connected to a car then do extra iterations.
            pObject->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);
            pParent->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);

            // set up a spherical joint to try and prevent the towed vehicle moving around too much
            if(!m_sphericalConstraintHandle.IsValid())
            {
                phConstraintSpherical::Params posConstraintParams;
                posConstraintParams.instanceA = pParent->GetCurrentPhysicsInst();
                posConstraintParams.instanceB = pObject->GetCurrentPhysicsInst();
                posConstraintParams.componentA = 0;
                posConstraintParams.componentB = 0;//static_cast<u16>(iComponentIndex);
                posConstraintParams.breakingStrength = sfConstraintBreakingStrength;
                posConstraintParams.breakable = true;

                Vector3 vecSpringoffset;
                GetTowHookIdealPosition( pParent, vecSpringoffset, 1.0f );

                //Matrix34 objectMatrix = MAT34V_TO_MATRIX34(pObject->GetMatrix());

                MAT34V_TO_MATRIX34(pObject->GetMatrix()).UnTransform(hookMatrix.d);
                hookMatrix.d.x = 0.0f;

                posConstraintParams.localPosA = VECTOR3_TO_VEC3V(vecSpringoffset);
                posConstraintParams.localPosB = VECTOR3_TO_VEC3V(hookMatrix.d);
                posConstraintParams.constructUsingLocalPositions = true;

                posConstraintParams.massInvScaleA = m_fForceMultToApplyInConstraint;

                phConstraintBase* pRopePosConstraint;

                m_sphericalConstraintHandle = PHCONSTRAINT->InsertAndReturnTemporaryPointer(posConstraintParams, pRopePosConstraint);
                Assertf(m_sphericalConstraintHandle.IsValid(), "Failed to create tow arm constraint"); 

                phConstraintBase* pRotConstraint = static_cast<phConstraintDistance*>( pRopePosConstraint );
                // set this joint as initially broken as we only want to activate it when we are moving at a certain speed
                pRotConstraint->SetBroken(true);
            }


            // setup a rotation limit to reduce the towed car moving around too much
            phConstraintRotation* pRotConstraintY = NULL;

            static dev_bool enableRollConstraint = false;
            if(enableRollConstraint)
            {
                static dev_float sfRollLimit = DtoR*(10.0f);
                float fCurrentRoll = VEC3V_TO_VECTOR3(pObject->GetTransform().GetA()).AngleY(VEC3V_TO_VECTOR3(pParent->GetTransform().GetA()));

                phConstraintRotation::Params rotYConstraintParams;
                rotYConstraintParams.instanceA = pParent->GetCurrentPhysicsInst();
                rotYConstraintParams.instanceB = pObject->GetCurrentPhysicsInst();
                rotYConstraintParams.componentA = 0;
                rotYConstraintParams.componentB = 0;
                rotYConstraintParams.worldAxis = pParent->GetTransform().GetB();
                rotYConstraintParams.breakingStrength = sfConstraintBreakingStrength;
                rotYConstraintParams.breakable = true;

                m_rotYConstraintHandle = PHCONSTRAINT->Insert(rotYConstraintParams);

                pRotConstraintY = static_cast<phConstraintRotation*>( PHCONSTRAINT->GetTemporaryPointer( m_rotYConstraintHandle ) );

                if(vehicleVerifyf(pRotConstraintY, "Failed to create trailer constraint"))
                {
                    EnforceConstraintLimits(pRotConstraintY, -sfRollLimit, sfRollLimit, fCurrentRoll, false);
                }
            }
        }

		vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::AttachRopesToObject - Setting m_Setup to true.");

        m_Setup = true;
    }
}

//////////////////////////////////////////////////////////////////////////

float CVehicleGadgetTowArm::GetTowArmPositionRatio()
{
	if(m_iNumberOfJoints <= 0)
	{
		return 0.0f;
	}
    float maxAngleDisplacement = GetJoint(0)->m_1dofJointInfo.m_fOpenAngle - GetJoint(0)->m_1dofJointInfo.m_fClosedAngle;
    float positionRatio = (GetJoint(0)->m_1dofJointInfo.m_fAngle - GetJoint(0)->m_1dofJointInfo.m_fClosedAngle)/maxAngleDisplacement;

    return positionRatio;
}

void CVehicleGadgetTowArm::UpdateHookPosition(CVehicle *pTowTruck, bool bUpdateConstraints)
{
	//Make sure the hook position is relatively up to date
	if(DistSquared(m_PropObject->GetMatrix().GetCol3(), pTowTruck->GetMatrix().GetCol3()).Getf() >= square(PH_CONSTRAINT_ASSERT_DEPTH))
	{
		vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::UpdateHookPosition - Warping 'hook' to safe position as it has moved too far from the parent vehicle.");

		WarpToPosition(pTowTruck, bUpdateConstraints);
	}
}

void CVehicleGadgetTowArm::UpdateTowedVehiclePosition(CVehicle *UNUSED_PARAM(pTowTruck), CVehicle *pVehicleToAttach)
{
	if(DistSquared(m_PropObject->GetTransform().GetPosition(), pVehicleToAttach->GetTransform().GetPosition()).Getf() >= square(PH_CONSTRAINT_ASSERT_DEPTH))
	{
		//This can happen in MP or if the player approaches a tow truck that's towing a vehicle whose position isn't set yet
		Vec3V vBBMaxWorld = pVehicleToAttach->GetTransform().Transform(VECTOR3_TO_VEC3V(pVehicleToAttach->GetBoundingBoxMax()));
		Vec3V vBBMinWorld = pVehicleToAttach->GetTransform().Transform(VECTOR3_TO_VEC3V(pVehicleToAttach->GetBoundingBoxMin()));

		bool bInvalidPos = DistSquared(m_PropObject->GetTransform().GetPosition(), vBBMaxWorld).Getf() > square(PH_CONSTRAINT_ASSERT_DEPTH) &&
					     DistSquared(m_PropObject->GetTransform().GetPosition(), vBBMinWorld).Getf() > square(PH_CONSTRAINT_ASSERT_DEPTH);

		if(bInvalidPos)
		{
			Vec3V vNewPos = m_PropObject->GetTransform().Transform(-VECTOR3_TO_VEC3V(pVehicleToAttach->GetBoundingBoxMax()));
			Matrix34 matOld = MAT34V_TO_MATRIX34(pVehicleToAttach->GetMatrix());
			pVehicleToAttach->TeleportWithoutUpdateGadgets(VEC3V_TO_VECTOR3(vNewPos), camFrame::ComputeHeadingFromMatrix(MAT34V_TO_MATRIX34(pVehicleToAttach->GetMatrix())), true, true);
			pVehicleToAttach->UpdateGadgetsAfterTeleport(matOld, false); // do not detach from gadget
		}
	}
}


//////////////////////////////////////////////////////////////////////////
static dev_float sfVelocityToReduceConstraintStrengthSq = 10.0f * 10.0f;
static dev_float sfAngVelocityToReduceConstraintStrengthSq = 1.5f * 1.5f;

void CVehicleGadgetTowArm::UpdateConstraintDistance(CVehicle *pVehicle)//
{ 
	if( !m_RopeInstanceA || !m_RopeInstanceB )
		return;
    phConstraintDistance* pRopePosConstraintA = static_cast<phConstraintDistance*>( m_RopeInstanceA->GetConstraint() );
    phConstraintDistance* pRopePosConstraintB = static_cast<phConstraintDistance*>( m_RopeInstanceB->GetConstraint() );
    phConstraintSpherical* pRotConstraint = NULL;
    phConstraintDistance* pPosConstraintCone = NULL;

    if(m_sphericalConstraintHandle.IsValid())
    {
        pRotConstraint = static_cast<phConstraintSpherical*>( PHCONSTRAINT->GetTemporaryPointer(m_sphericalConstraintHandle) );
    }

    if(m_posConstraintHandle.IsValid())
    {
        pPosConstraintCone = static_cast<phConstraintDistance*>( PHCONSTRAINT->GetTemporaryPointer(m_posConstraintHandle) );
    }

    //adjust the position of the hook, lerp the hook to the desired position
    if(m_pTowedVehicle)
	   {
	    if( m_pTowedVehicle->GetMatrix().GetCol2().GetZf() < 0.0f   ||
			(pRopePosConstraintA && pRopePosConstraintA->IsBroken()  &&
	        pRopePosConstraintB && pRopePosConstraintB->IsBroken()) ||
	        ((m_bUsingSphericalConstraint && pRotConstraint && pRotConstraint->IsBroken()) ||
	        (!m_bUsingSphericalConstraint && pPosConstraintCone && pPosConstraintCone->IsBroken())) )
	    {
			if(m_pTowedVehicle && m_pTowedVehicle->IsNetworkClone())
			{
				// send an event to the owner of the m_pTowedVehicle entity requesting they detach
				CRequestDetachmentEvent::Trigger(*m_pTowedVehicle);
			}
			else
			{
				if(!pVehicle->IsNetworkClone())
				{
					vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::UpdateConstraintDistance - Detaching entity as constraint is broken.");
					DetachVehicle(pVehicle);
					return;
				}
			}
	    }

		float fMassScale = m_pTowedVehicle->GetMass()/pVehicle->GetMass();
		float fTowedVehCZ = m_pTowedVehicle->GetMatrix().GetCol2().GetZf();
		if(fTowedVehCZ > 0)
		{
			fMassScale = Clamp(fMassScale, 0.5f * fTowedVehCZ, 2.0f * fTowedVehCZ);
		}
		float fConstraintStrengthMin = fMassScale * sfConstraintBreakingStrengthUnderCollision;

		//Easier to break at higher speeds
		float fTowedVehicleVelSq = m_pTowedVehicle->GetVelocity().Mag2();
		float fVelocitySqInvScale = InvertSafe(fTowedVehicleVelSq/sfVelocityToReduceConstraintStrengthSq, 1.0f);
		float fPositionConstraintStrength = rage::Clamp(sfConstraintBreakingStrength * fVelocitySqInvScale, fConstraintStrengthMin, sfConstraintBreakingStrength);
		
		float fTowedVehicleAngVelSq = m_pTowedVehicle->GetAngVelocity().Mag2();
		float fAngVelocitySqInvScale = InvertSafe(fTowedVehicleAngVelSq/sfAngVelocityToReduceConstraintStrengthSq, 1.0f);
		float fRotationConstraintStrength = rage::Clamp(sfConstraintBreakingStrength * fAngVelocitySqInvScale, fConstraintStrengthMin, sfConstraintBreakingStrength); 

		//Network clones can't break their attachments
		if(!pVehicle->IsNetworkClone())
		{
			if(IsPickUpFromTopVehicle(pVehicle))
			{
				if(pRopePosConstraintA)
				{
					pRopePosConstraintA->SetBreakable(true, sfConstraintBreakingStrength);
				}

				if(pRopePosConstraintB)
				{
					pRopePosConstraintB->SetBreakable(true, sfConstraintBreakingStrength);
				}
			}
			else
			{
				if(pRopePosConstraintA)
				{
					pRopePosConstraintA->SetBroken(false);
					if(m_bTowTruckTouchingTowVehicle && fTowedVehicleVelSq > sfVelocityToReduceConstraintStrengthSq)
					{
						pRopePosConstraintA->SetBreakable(true, fConstraintStrengthMin);
					}
					else
					{
						pRopePosConstraintA->SetBreakable(true, fPositionConstraintStrength);
					}
				}

				if(pRopePosConstraintB)
				{
					pRopePosConstraintB->SetBroken(false);
					if(m_bTowTruckTouchingTowVehicle && fTowedVehicleVelSq > sfVelocityToReduceConstraintStrengthSq)
					{
						pRopePosConstraintB->SetBreakable(true, fConstraintStrengthMin);
					}
					else
					{
						pRopePosConstraintB->SetBreakable(true, fPositionConstraintStrength);
					}
				}

				if(pRotConstraint && m_bUsingSphericalConstraint)
				{
					pRotConstraint->SetBroken(false);
					if(m_bTowTruckTouchingTowVehicle && fTowedVehicleAngVelSq > sfAngVelocityToReduceConstraintStrengthSq)
					{
						pRotConstraint->SetBreakable(true, fConstraintStrengthMin);
					}
					else
					{
						pRotConstraint->SetBreakable(true, fRotationConstraintStrength);
					}
				}

				if(pPosConstraintCone && !m_bUsingSphericalConstraint)
				{
					pPosConstraintCone->SetBroken(false);
					if(m_bTowTruckTouchingTowVehicle && fTowedVehicleAngVelSq > sfAngVelocityToReduceConstraintStrengthSq)
					{
						pPosConstraintCone->SetBreakable(true, fConstraintStrengthMin);
					}
					else
					{
						pPosConstraintCone->SetBreakable(true, fRotationConstraintStrength);
					}
				}
			}
		}

		Matrix34 hookMatrix = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());
		Matrix34 towMatrix = MAT34V_TO_MATRIX34(m_pTowedVehicle->GetMatrix());

		if(m_PropObject->GetAttachmentExtension())
		{
			m_PropObject->GetAttachmentExtension()->SetAttachOffset(m_vCurrentTowHookPosition);

			Quaternion qRotation;
			Vector3 normalisedDirection  = m_vCurrentTowHookPosition;
			normalisedDirection.Normalize();
			qRotation.FromVectors(YAXIS, normalisedDirection, ZAXIS);
			m_PropObject->GetAttachmentExtension()->SetAttachQuat(qRotation);

			Matrix34 matAttachExt;
			matAttachExt.Identity();
			matAttachExt.FromQuaternion(m_PropObject->GetAttachmentExtension()->GetAttachQuat());
			matAttachExt.d = m_PropObject->GetAttachmentExtension()->GetAttachOffset();

			hookMatrix = matAttachExt;
			hookMatrix.Dot(towMatrix);
		}


		Matrix34 matMountA; 
		Matrix34 matMountB;

		if(m_PropObject->GetSkeleton() &&
		   siHookRopeAttachmentBoneIndexA < m_PropObject->GetSkeleton()->GetBoneCount() &&
		   siHookRopeAttachmentBoneIndexB <  m_PropObject->GetSkeleton()->GetBoneCount())
		{
			matMountA = m_PropObject->GetObjectMtx(siHookRopeAttachmentBoneIndexA);
			matMountB = m_PropObject->GetObjectMtx(siHookRopeAttachmentBoneIndexB);
		}
		else
		{
			matMountA.Identity();
			matMountB.Identity();
		}

		hookMatrix.Transform(matMountA.d);
		hookMatrix.Transform(matMountB.d);

		if(pRopePosConstraintA)
		{
			pRopePosConstraintA->SetWorldPosB(VECTOR3_TO_VEC3V(matMountA.d));
		}

		if(pRopePosConstraintB)
		{
			pRopePosConstraintB->SetWorldPosB(VECTOR3_TO_VEC3V(matMountB.d));
		}

        if(pRotConstraint && !IsPickUpFromTopVehicle(pVehicle))
        {
		    // only use the spherical joint when above a certain speed and not touching the towed vehicle
            if( pVehicle->GetVelocity().Mag2() < sfVehicleSpeedSqrToEnableSphericalConstraintAndArmMovement || (m_fTowTruckPositionTransitionTime < 1.0f) )
            {
                pRotConstraint->SetBroken(true);
                m_bUsingSphericalConstraint = false;
            }
            else if(pRotConstraint->IsBroken())
            {  
                Vector3 vecSpringoffset;
                GetTowHookIdealPosition( pVehicle, vecSpringoffset, GetTowArmPositionRatio() );

                Matrix34 pTowTruckMatrix = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());

                pTowTruckMatrix.UnTransform(hookMatrix.d);

                pRotConstraint->SetLocalPosA(VECTOR3_TO_VEC3V(hookMatrix.d));
                pRotConstraint->SetLocalPosB(VECTOR3_TO_VEC3V(m_vCurrentTowHookPosition));

                pRotConstraint->SetBroken(false);
                m_bUsingSphericalConstraint = true;
            }
        }
    }

	Matrix34 TruckMatrix = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());

	Matrix34 matTruckMountA = pVehicle->GetObjectMtx(m_iTowMountA); 
	Matrix34 matTruckMountB = pVehicle->GetObjectMtx(m_iTowMountB);
	
	TruckMatrix.Transform(matTruckMountA.d);
	TruckMatrix.Transform(matTruckMountB.d);

	if(pRopePosConstraintA)
	{
		pRopePosConstraintA->SetWorldPosA(VECTOR3_TO_VEC3V(matTruckMountA.d));
	}

	if(pRopePosConstraintB)
	{
		pRopePosConstraintB->SetWorldPosA(VECTOR3_TO_VEC3V(matTruckMountB.d));
	}

    //update the length of the distance constraint based on how high the tow arm is, also disable it when the spherical constraint is being used
    if(pPosConstraintCone && !IsPickUpFromTopVehicle(pVehicle))
    {
        if(m_bUsingSphericalConstraint)
        {
            pPosConstraintCone->SetBroken(true);
        }
        else
        {
            pPosConstraintCone->SetBroken(false); 
            float positionRatio = GetTowArmPositionRatio();

            Vector3 vecSpringoffset;
            GetTowHookIdealPosition( pVehicle, vecSpringoffset, GetTowArmPositionRatio() );

            if(pPosConstraintCone)
            {
                pPosConstraintCone->SetLocalPosA(VECTOR3_TO_VEC3V(vecSpringoffset));


                if(m_pTowedVehicle)
                {
					if(m_fTowTruckPositionTransitionTime < 1.0f)
					{   
						Vec3V vLocalPosB = pPosConstraintCone->GetLocalPosB();
						Vec3V vecDiff( VECTOR3_TO_VEC3V(m_vDesiredTowHookPosition) - m_vLocalPosConstraintStart );
						vLocalPosB = m_vLocalPosConstraintStart + (vecDiff*ScalarVFromF32(m_fTowTruckPositionTransitionTime));
						pPosConstraintCone->SetLocalPosB(vLocalPosB);
					}
					else
					{
						pPosConstraintCone->SetLocalPosB(VECTOR3_TO_VEC3V(m_vCurrentTowHookPosition));
					}
                }
                else
                {
                    pPosConstraintCone->SetLocalPosB(Vec3V(V_ZERO));
					if(!m_bIsTeleporting)
					{
						static dev_float sfFreeHookExtraMovement = 0.5f;
						positionRatio += sfFreeHookExtraMovement;//allow the hook to move a bit more if it is not connected to a car
					}
					m_bIsTeleporting = false;
                }

		        //allow the hook to move more when the tow arm is low
                static dev_float sfhookDeviation = 1.0f;

                pPosConstraintCone->SetMaxDistance( sfhookDeviation*positionRatio );
            }
        }
    }

}


//////////////////////////////////////////////////////////////////////////
bank_float sfHookVsTruckNormalThresholdForRestingOnTruck = 0.8f;
bank_float sfHookVsTruckFriction = 0.001f;
bank_float sfConstraintFriction = 0.0f;
bank_float sfConstraintPushSpeed = 0.0f;
bank_float sfTowedVehiclePushSpeed = 1.0f;
void CVehicleGadgetTowArm::ProcessPreComputeImpacts(CVehicle* UNUSED_PARAM(pVehicle), phCachedContactIterator &impacts)
{
    if(m_Setup)
    {
	    impacts.Reset();

        m_HookRestingOnTruck = false;
        m_bTowTruckTouchingTowVehicle = false;

        while(!impacts.AtEnd())
        {
            CEntity* pOtherEntity = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());

            if(impacts.IsConstraint())
            {  
                //reduce the friction on the constraint.
                impacts.SetFriction(sfConstraintFriction);
				impacts.SetDepth(Min(sfConstraintPushSpeed * fwTimer::GetTimeStep(), impacts.GetDepth()));
            }
		    else
            {
                if(pOtherEntity == m_PropObject )
                {
                    //make the truck against the hook super slippery
                    impacts.SetFriction(sfHookVsTruckFriction);

                    Vector3 truckNormal;
                    impacts.GetMyNormal(truckNormal);

                    //if the hook is resting on the back of the truck set the resting on truck variable
                    if(truckNormal.Dot(-ZAXIS) > sfHookVsTruckNormalThresholdForRestingOnTruck)
                    {
                        m_HookRestingOnTruck = true;
                    }
                }
                else if( pOtherEntity == m_pTowedVehicle )
                {
                    m_bTowTruckTouchingTowVehicle = true;
					impacts.SetDepth(Min(sfTowedVehiclePushSpeed * fwTimer::GetTimeStep(), impacts.GetDepth()));
                }
            }


            ++impacts;
        }

        impacts.Reset();
    }
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetTowArm::GetTowHookIdealPosition(CVehicle *pParent, Vector3 &vecTowHookIdealPosition_Out, float fTowArmPositionRatio)
{
	// work out where we would really like the hook
    static dev_float sfSpringYOffsetNoVehicle = -0.25f;
    static dev_float sfSpringYOffsetWithVehicle = -0.1f;

    float springYOffset = sfSpringYOffsetNoVehicle;

    if(m_pTowedVehicle)
    {
        springYOffset = sfSpringYOffsetWithVehicle;
    }

    float positionRatio = fTowArmPositionRatio;
    Matrix34 matLocalMountA = pParent->GetObjectMtx(m_iTowMountA);

	float fRopeMinLength = pParent->GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeMinLength : sfTowTruckRopeMinLength;
	float fRopeAdjustableLength = pParent->GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeAdjustableLength : sfTowTruckRopeAdjustableLength;

    vecTowHookIdealPosition_Out.Set(0.0f, matLocalMountA.d.y + springYOffset, matLocalMountA.d.z - (fRopeMinLength + (fRopeAdjustableLength * positionRatio)));
}



//////////////////////////////////////////////////////////////////////////
bank_float CVehicleGadgetTowArm::ms_fAttachSearchRadius = 0.50f;
bank_u32 CVehicleGadgetTowArm::ms_uAttachResetTime = 6000;
static dev_float sfHookAngularDamping = 0.3f;
static dev_float sfHookLinearDamping = 0.0f;

static dev_float sfHookAngularDamping2 = 0.3f;
static dev_float sfHookLinearDamping2 = 0.0f;

static dev_float sfStillHookAngularDamping = 2.0f;
static dev_float sfStillHookLinearDamping = 1.0f;

static dev_float sfStillHookAngularDamping2 = 4.0f;
static dev_float sfStillHookLinearDamping2 = 0.5f;

static dev_float sfTowedVehicleAngularDamping = 0.025f;
static dev_float fMinPositionRatioToAttach = 0.3f;
static dev_float fMaxSpeedSqForAttaching = 6.0f;

void CVehicleGadgetTowArm::ProcessPhysics(CVehicle* pParent, float fTimestep, const int nTimeSlice)
{
    CVehicleGadgetWithJointsBase::ProcessPhysics( pParent, fTimestep, nTimeSlice);
	if(!CVehicleAILodManager::ShouldDoFullUpdate(*pParent) && m_pTowedVehicle  REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		//Need to update constraints, etc. each frame if we're towing
		ProcessTowArmAndHook(pParent);
	}
}

void CVehicleGadgetTowArm::ProcessControl(CVehicle* pParent)
{
	//This needs to be in ProcessControl so we can handle attach/detach
	//of dummy tow trucks that aren't calling ProcessPhysics
	ProcessTowArmAndHook(pParent);

	// Check different modules for visibility settings and make sure those are also set for the prop object and ropes.
	const int nNumModulesToCheck = 2;
	eIsVisibleModule nModulesToCheck[nNumModulesToCheck] = 
	{
		SETISVISIBLE_MODULE_SCRIPT,
		SETISVISIBLE_MODULE_NETWORK
	};

	bool bShouldRopesBeVisible = true;

	for( int nModule = 0; nModule < nNumModulesToCheck; nModule++ )
	{
		bool bParentVisible = pParent->GetIsVisibleForModule( nModulesToCheck[ nModule ] );

		// If either parent or prop are not visible, make sure the ropes follow suite.
		if( m_PropObject )
		{
			bShouldRopesBeVisible = bParentVisible && m_PropObject->GetIsVisibleForModule( nModulesToCheck[ nModule ] );

			if( m_PropObject->GetIsVisibleForModule(nModulesToCheck[nModule]) != bParentVisible )
			{
				m_PropObject->SetIsVisibleForModule(nModulesToCheck[nModule], bParentVisible, true);
			}
		}

		if( m_pTowedVehicle && m_pTowedVehicle->GetIsVisibleForModule(nModulesToCheck[nModule]) != bParentVisible )
		{
			m_pTowedVehicle->SetIsVisibleForModule(nModulesToCheck[nModule], bParentVisible, true);		
		}
	}

	if( m_RopeInstanceA )
	{
		m_RopeInstanceA->SetDrawEnabled( bShouldRopesBeVisible );
	}

	if( m_RopeInstanceB )
	{
		m_RopeInstanceB->SetDrawEnabled( bShouldRopesBeVisible );
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		float angle = GetJoint(0)->m_1dofJointInfo.m_fAngle;
		CReplayMgr::RecordFx<CPacketTowTruckArmRotate>(CPacketTowTruckArmRotate(angle), pParent);
	}
#endif // GTA_REPLAY
}

void CVehicleGadgetTowArm::ProcessTowArmAndHook(CVehicle *pParent)
{
	float fTimestep = fwTimer::GetTimeStep();

	// Are we trying to detach a vehicle
	if(m_bDetachVehicle)
	{
		if(m_pTowedVehicle)
		{
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - Actually Detaching vehicle");
			ActuallyDetachVehicle( pParent );
		}

		m_bDetachVehicle = false;
		return;//if we've detached a vehicle, no point doing anything else
	}

	if(!m_bRopesAndHookInitialized)
	{
		vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - !m_bRopesAndHookInitialized");
		return;
	}

    if( !(m_PropObject && m_PropObject->GetCurrentPhysicsInst() &&  PHLEVEL->IsInLevel(pParent->GetCurrentPhysicsInst()->GetLevelIndex()) && m_PropObject->GetSkeleton()) )
    {
		vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - No prop object");
        return;
    }
    
	if(!m_bJointLimitsSet)
	{
		// Don't allow the tow arm to move too high as this makes towing less stable
		if(pParent->GetFragInst() && pParent->GetCurrentPhysicsInst()==pParent->GetFragInst() && HasTowArm())
		{
			static dev_float sfClosedAngleAdjust = -0.4f;
			PHSIM->ActivateObject(pParent->GetFragInst());
			if(SetJointLimits(pParent, 0, GetJoint(0)->m_1dofJointInfo.m_fOpenAngle, GetJoint(0)->m_1dofJointInfo.m_fClosedAngle - sfClosedAngleAdjust))
			{
				m_bJointLimitsSet = true;
			}
		}
	}

    //Make sure the hook is setup and the vehicle we are supposed to be attached to is still attached.
	if( !m_Setup || m_RopesAttachedVehiclePhysicsInst != pParent->GetCurrentPhysicsInst() )//check whether the vehicles physics inst has changed, if it has recreate the constraints
	{
		m_PropObject->ActivatePhysics();
        m_PropObject->SetMass(sfHookWeight);
		if(!m_bHoldsDrawableRef)
		{
			m_bHoldsDrawableRef = true;
			m_PropObject->GetBaseModelInfo()->BumpDrawableRefCount();
		}
        AttachRopesToObject( pParent, m_PropObject );
	}

	if(m_Setup)
	{
		Assert(m_PropObject);

		// Hide ropes and hook if the vehicle's hidden by script
		bool isVisible = pParent->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT);

		if( m_PropObject && m_PropObject->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT) != isVisible )
			m_PropObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, isVisible, true);

		if( m_pTowedVehicle && m_pTowedVehicle->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT) != isVisible )
			m_pTowedVehicle->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, isVisible, true);
		
		if( m_RopeInstanceA )
			m_RopeInstanceA->SetDrawEnabled(isVisible);
	
		if( m_RopeInstanceB )
			m_RopeInstanceB->SetDrawEnabled(isVisible);

        //Adjust rope length for tow arm position.
        float positionRatio = GetTowArmPositionRatio();

		if( m_RopeInstanceA && m_RopeInstanceB)
		{
			static dev_float fPosRatioEpsilon = 1.0e-02f;
			if(fabs(positionRatio - m_LastTowArmPositionRatio) > fPosRatioEpsilon && !m_HookRestingOnTruck)
			{
				float fRopeLength = pParent->GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeLength : sfTowTruckRopeLength;
				float fRopeMinLength = pParent->GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeMinLength : sfTowTruckRopeMinLength;
				float fRopeAdjustableLength = pParent->GetModelIndex() == MI_CAR_TOWTRUCK_2 ? sfTowTruck2RopeAdjustableLength : sfTowTruckRopeAdjustableLength;

				float fRopeLengthForced = Clamp(fRopeMinLength + (fRopeAdjustableLength * positionRatio), fRopeMinLength, fRopeLength);
				
				m_RopeInstanceA->StartWindingFront(); // start unwinding has to be called when ever force length is called
				m_RopeInstanceA->ForceLength(fRopeLengthForced);

				m_RopeInstanceB->StartWindingFront();
				m_RopeInstanceB->ForceLength(fRopeLengthForced);
			}
			else
			{
				m_RopeInstanceA->StopWindingFront();
				m_RopeInstanceB->StopWindingFront();
			}

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				ropeInstance* r = m_RopeInstanceA;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r), NULL, false);

				r = m_RopeInstanceB;
				CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r), NULL, false);
			}
#endif
		}

		UpdateHookPosition(pParent);

		m_LastTowArmPositionRatio = positionRatio;

		//update the constraints
        UpdateConstraintDistance(pParent);

		f32 movementSpeed = 0.0f;
		if(HasTowArm())
		{
			movementSpeed = (GetJoint(0)->m_1dofJointInfo.m_fAngle - m_fPrevJointPosition) * fwTimer::GetInvTimeStep();
			m_fPrevJointPosition = GetJoint(0)->m_1dofJointInfo.m_fAngle;
		}

		if(m_VehicleTowArmAudio)
		{			
			m_VehicleTowArmAudio->SetSpeed(movementSpeed);

			if(m_PropObject)
			{
				m_VehicleTowArmAudio->SetHookPosition(VEC3V_TO_VECTOR3(m_PropObject->GetMatrix().GetCol3()));
			}

			f32 linkAngle = 0.0f;

			if(pParent && m_pTowedVehicle)
			{
				linkAngle = Dot(pParent->GetTransform().GetForward(), m_pTowedVehicle->GetTransform().GetForward()).Getf();
			}
			
			m_VehicleTowArmAudio->SetTowedVehicleAngle(linkAngle);
		}

        if(m_pTowedVehicle)
        {
		    //lerp the hook to the desired position based on the speed the hook is currently moving
            Vector3 hookVelocity = m_pTowedVehicle->GetLocalSpeed(m_vCurrentTowHookPosition);
           
            if(m_fTowTruckPositionTransitionTime < 1.0f)
            {   
                float hookSpeed = hookVelocity.Mag2();
                Vector3 vecDiff( m_vDesiredTowHookPosition - m_vStartTowHookPosition );
                static dev_float sfSpeedModifier = 0.25f;
                m_fTowTruckPositionTransitionTime += (Max(hookSpeed * sfSpeedModifier, sfSpeedModifier))*fTimestep;
                if(m_fTowTruckPositionTransitionTime > 1.0f)
                    m_fTowTruckPositionTransitionTime = 1.0f;
                m_vCurrentTowHookPosition = m_vStartTowHookPosition + (vecDiff*m_fTowTruckPositionTransitionTime);
            }

            phInst *towedPhysicsInst = m_pTowedVehicle->GetCurrentPhysicsInst();
			if( m_RopeInstanceA && m_RopeInstanceB )
			{
				//make sure the tow vehicle is still attached
				if( ( m_RopeInstanceA->GetInstanceA() == towedPhysicsInst || 
					  m_RopeInstanceA->GetInstanceB() == towedPhysicsInst ) &&
					( m_RopeInstanceB->GetInstanceA() == towedPhysicsInst || 
					  m_RopeInstanceB->GetInstanceB() == towedPhysicsInst ) )
				{
					pParent->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;

					//Limit the pitch enough to just keep the tow truck wheels on the ground
					static dev_float sfRaisePitch = 0.001f;
					bool bHasWheelsInAir = pParent->GetNumContactWheels() < pParent->GetNumWheels();
					if(bHasWheelsInAir && HasTowArm())
					{
						float fDesiredPitch = GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle;
						fDesiredPitch = rage::Clamp(fDesiredPitch + sfRaisePitch, GetJoint(0)->m_1dofJointInfo.m_fClosedAngle, GetJoint(0)->m_1dofJointInfo.m_fOpenAngle);
						SetDesiredPitch(pParent, fDesiredPitch);
					}

					if(towedPhysicsInst)
					{
						phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(towedPhysicsInst->GetArchetype());
						pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(sfTowedVehicleAngularDamping, sfTowedVehicleAngularDamping, sfTowedVehicleAngularDamping));
					}

					return;
				}
				else
				{
					// last towed vehicle has detached itself, so make sure it' detached properly and start scanning again
					ActuallyDetachVehicle( pParent );
				}
			}
        }
        else
        {
			static dev_float fVelSqLimitLinearDamping = 4.0f;
			bool bIsMoving = pParent->GetVelocity().Mag2() > fVelSqLimitLinearDamping; 
			if(m_HookRestingOnTruck || bIsMoving)
			{
				//Try to prevent the prop from sticking to the tow truck bed and also reduce how much it swings about
				Vector3 PropPosition = VEC3V_TO_VECTOR3(m_PropObject->GetTransform().GetPosition());
				Matrix34 matTruck = MAT34V_TO_MATRIX34(pParent->GetMatrix());

				Vector3 vecSpringPos;
				GetTowHookIdealPosition( pParent, vecSpringPos, GetTowArmPositionRatio() );

				matTruck.Transform(vecSpringPos);

				Vector3 distance = PropPosition - vecSpringPos;

				static dev_float sfSpringConstant = -30.0f;
				static dev_float sfDampingConstant = 3.0f;

				// Work out the torque to apply with some damping.
				Vector3 dampingToApply(VEC3_ZERO);
				if(fTimestep > 0.0f)
				{
					dampingToApply = ((distance-m_vLastFramesDistance) / fTimestep);
				}

				Vector3 forceToApply = ((distance * sfSpringConstant) - (dampingToApply * sfDampingConstant)) * m_PropObject->GetMass();// scale the torque by mass

				static dev_float sfMaxSpringForce = 800.0f;
				//clamp the maximum force to apply to the hook
				float forceMag = forceToApply.Mag();
				if(forceMag > sfMaxSpringForce)
				{
					float forceMult = sfMaxSpringForce/forceMag;
					forceToApply *= forceMult;
				}

				// Keep track of the previous frames difference so we can do some damping
				m_vLastFramesDistance = distance;
				m_PropObject->ApplyForceCg(forceToApply);
			}

			// Apply rotational damping to prevent constant spinning
			if(m_PropObject->GetCurrentPhysicsInst())
			{
				float fAngularDamping = bIsMoving ? sfHookAngularDamping : sfStillHookAngularDamping;
				float fLinearDamping = bIsMoving ? sfHookLinearDamping : sfStillHookLinearDamping;

				float fAngular2Damping = bIsMoving ? sfHookAngularDamping2 : sfStillHookAngularDamping2;
				float fLinear2Damping = bIsMoving ? sfHookLinearDamping2 : sfStillHookLinearDamping2;

				phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(m_PropObject->GetCurrentPhysicsInst()->GetArchetype());
				pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(fAngularDamping, fAngularDamping, fAngularDamping));
				pArchetypeDamp->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(fLinearDamping, fLinearDamping, fLinearDamping));	
				pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(fAngular2Damping, fAngular2Damping, fAngular2Damping));
				pArchetypeDamp->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(fLinear2Damping, fLinear2Damping, fLinear2Damping));	
			
				if(m_PropObject->GetCollider() && m_PropObject->GetCollider()->GetSleep())
				{
					m_PropObject->GetCollider()->GetSleep()->SetVelTolerance2(0.05f);
					m_PropObject->GetCollider()->GetSleep()->SetAngVelTolerance2(0.2f);
				}
			
			}
        }

        // Only scan for trailer pickup if the driver is a player
        if( pParent->GetDriver() == NULL || !pParent->GetDriver()->IsAPlayerPed() )
        {
            return;
        }

		if(m_uIgnoreAttachTimer > 0)
		{
			u32 uTimeStep = fwTimer::GetTimeStepInMilliseconds();

			// Be careful not to wrap round
			m_uIgnoreAttachTimer = uTimeStep > m_uIgnoreAttachTimer ? 0 : m_uIgnoreAttachTimer - uTimeStep;

			return;
		}

		// If parent is asleep, skip test
		// We would be awake if anything was colliding with us or if we were moving
		if(m_PropObject->IsAsleep())
		{
			return;
		}

		// If towing is disabled then don't try to pickup anything
		if (pParent->m_nVehicleFlags.bDisableTowing)
		{
			return;
		}
		// don't try to pick up anything if the tow truck is a network clone
		if (pParent->IsNetworkClone())
		{
			return;
		}

		// don't try to pick up anything until the arm is lowered
		if(GetTowArmPositionRatio() < fMinPositionRatioToAttach)
		{
			return;
		}

		// don't try to attach if we are moving too fast
		if(pParent->GetVelocity().Mag2() > fMaxSpeedSqForAttaching)
		{
			return;
		}

		// Lookup hook position
        //get the position of the hook point
        Matrix34 hookPointPositionMatrix;
        m_PropObject->GetGlobalMtx(siHookPointBoneIndex, hookPointPositionMatrix);

		// Do a shapetest at this position, excluding our parent.
		const u32 nNumAttachPointResults = 16;
		WorldProbe::CShapeTestSphereDesc sphereDesc;
		WorldProbe::CShapeTestFixedResults<nNumAttachPointResults> sphereResults;
		sphereDesc.SetResultsStructure(&sphereResults);
		sphereDesc.SetSphere(hookPointPositionMatrix.d, ms_fAttachSearchRadius);
		sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
		sphereDesc.SetExcludeEntity(pParent);

		if(!WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
			return;

		int nClosestComponent = -1;
		CVehicle *pClosestVehicle = NULL;
		Vector3 vClosestHitLocal = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
		
		// Search for a vehicle in all hits
		for(WorldProbe::ResultIterator it = sphereResults.begin(); it < sphereResults.last_result(); ++it)
		{
			if(it->GetHitInst())
			{
				// Lookup the CEntity
				CEntity* pEntity = CPhysics::GetEntityFromInst(it->GetHitInst());

				if(pEntity && pEntity->GetIsTypeVehicle())
				{
					CVehicle *pVehicle = static_cast<CVehicle*>(pEntity);

					fwAnimDirectorComponentSyncedScene* pComponent = pVehicle->GetAnimDirector() ? static_cast<fwAnimDirectorComponentSyncedScene*>(pVehicle->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeSyncedScene)) : NULL;
				
					bool lockedPersonalVehicle = NetworkInterface::IsVehicleLockedForPlayer(pVehicle, pParent->GetDriver()) && pVehicle->IsPersonalVehicle();

					bool isGhostedVehicleCheck = false;
					const CVehicle* currentParent = static_cast<const CVehicle*>(pVehicle->GetAttachParent());

					if ((NetworkInterface::IsAGhostVehicle(*pParent) || NetworkInterface::IsAGhostVehicle(*pVehicle)) && currentParent && currentParent != pParent)
					{
						gnetDebug3("CVehicleGadgetTowArm::ProcessTowArmAndHook Preventing attach due to ghost check");
						isGhostedVehicleCheck = true;
					}

					if(pVehicle->m_nVehicleFlags.bAutomaticallyAttaches && 
						pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR && 
						!pVehicle->m_nVehicleFlags.bIsBus && 
						!pVehicle->m_nVehicleFlags.bDisableTowing && 
						(!pComponent || !pComponent->IsPlayingSyncedScene()) && 
						!lockedPersonalVehicle &&
						!NetworkInterface::IsInMPCutscene() &&
						!isGhostedVehicleCheck) //check whether this vehicle should be able to automatically attach
					{
						if(pParent->GetAttachParent() != pVehicle)// Don't allow the tow truck to be attached to something it is already attached to.
						{
							Matrix34 matVehicle = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
							if(pVehicle->IsNetworkClone())
							{	
								// we can't attach to a network clone, we need control of it to update its attachment state
								// try and request control of it
								CRequestControlEvent::Trigger(pVehicle->GetNetworkObject() NOTFINAL_ONLY(, "CVehicleGadgetTowArm::ProcessTowArmAndHook"));
							}
							else if(matVehicle.c.z > 0.0f)
							{
								Vector3 hitPosition = it->GetHitPosition();
								matVehicle.UnTransform(hitPosition);

								static dev_float sfBoundingBoxAttachmentAllowance = 0.20f;
								if(IsPickUpFromTopVehicle(pParent))
								{
									if(hitPosition.z > pVehicle->GetBoundingBoxMax().z * (1.0f - sfBoundingBoxAttachmentAllowance))
									{  
										AttachVehicleToTowArm( pParent, pVehicle, it->GetHitPosition(), it->GetHitComponent() );

										//Add a shocking to notify any relevant peds.
										CEventShockingVehicleTowed ev(*pParent, *pEntity);
										CShockingEventsManager::Add(ev);
										break;
									}
								}
								else
								{
									//Check alignment and hit position
									static dev_float fBigVehicleHeadingAngleThreshold = QUARTER_PI;
									float fHeadingAngleThreshold = pVehicle->m_nVehicleFlags.bIsBig ? fBigVehicleHeadingAngleThreshold : 0.0f;

									ScalarV testVehDotParentB = Dot(pParent->GetMatrix().GetCol1(), pVehicle->GetMatrix().GetCol1());
									if(hitPosition.y > pVehicle->GetBoundingBoxMax().y * (1.0f - sfBoundingBoxAttachmentAllowance))
									{
										if(testVehDotParentB.Getf() > fHeadingAngleThreshold && fabs(hitPosition.y - pVehicle->GetBoundingBoxMax().y) < fabs(vClosestHitLocal.y - pVehicle->GetBoundingBoxMax().y))
										{
											vClosestHitLocal = hitPosition;
											pClosestVehicle = pVehicle;
											nClosestComponent = it->GetHitComponent();
										}
									}
									else if(hitPosition.y < pVehicle->GetBoundingBoxMin().y * (1.0f - sfBoundingBoxAttachmentAllowance))
									{
										if(testVehDotParentB.Getf() < -fHeadingAngleThreshold && fabs(hitPosition.y - pVehicle->GetBoundingBoxMin().y) < fabs(vClosestHitLocal.y - pVehicle->GetBoundingBoxMin().y))
										{
											vClosestHitLocal = hitPosition;
											pClosestVehicle = pVehicle;
											nClosestComponent = it->GetHitComponent();
										}
									}
								}
                            }
                        }
                    }
                }
            }
		}

		if(pClosestVehicle)
		{
			Matrix34 matVehicle = MAT34V_TO_MATRIX34(pClosestVehicle->GetMatrix());
			matVehicle.Transform(vClosestHitLocal);
			AttachVehicleToTowArm( pParent, pClosestVehicle, vClosestHitLocal, nClosestComponent );

			//Add a shocking to notify any relevant peds.
			CEventShockingVehicleTowed ev(*pParent, *pClosestVehicle);
			CShockingEventsManager::Add(ev);
			pClosestVehicle->m_nVehicleFlags.bTowedVehBoundsCanBeRaised = true;
		}

		if(m_RopeInstanceA)
		{
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - Rope A status: m_RopeInstanceA->GetInstanceA(): %p | m_RopeInstanceA->GetInstanceB(): %p | m_RopeInstanceA->GetAttachedTo(): %p | m_RopeInstanceA->GetLength(): %f | m_RopeInstanceA->GetWorldPositionA(): %f, %f, %f | m_RopeInstanceA->GetWorldPositionB: %f, %f, %f",
				m_RopeInstanceA->GetInstanceA(),
				m_RopeInstanceA->GetInstanceB(),
				m_RopeInstanceA->GetAttachedTo(),
				m_RopeInstanceA->GetLength(),
				VEC3V_ARGS(m_RopeInstanceA->GetWorldPositionA()),
				VEC3V_ARGS(m_RopeInstanceA->GetWorldPositionB())
				);

			if(m_RopeInstanceA->GetConstraint())
			{
				vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - Rope A constraint status: m_RopeInstanceA->GetConstraint()->IsBreakable(): %i | m_RopeInstanceA->GetConstraint()->IsBroken(): %i | m_RopeInstanceA->GetConstraint()->IsAtLimit(): %i | m_RopeInstanceA->GetConstraint()->GetInstanceA(): %p | m_RopeInstanceA->GetConstraint()->GetInstanceB(): %p",
					m_RopeInstanceA->GetConstraint()->IsBreakable(),
					m_RopeInstanceA->GetConstraint()->IsBroken(),
					m_RopeInstanceA->GetConstraint()->IsAtLimit(),
					m_RopeInstanceA->GetConstraint()->GetInstanceA(),
					m_RopeInstanceA->GetConstraint()->GetInstanceB()
					);
			}
		}
		else
		{
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - No Rope A instance.");
		}


		if(m_RopeInstanceB)
		{
			vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - Rope B status: m_RopeInstanceB->GetInstanceA(): %p | m_RopeInstanceB->GetInstanceB(): %p | m_RopeInstanceB->GetAttachedTo(): %p | m_RopeInstanceB->GetLength(): %f | m_RopeInstanceB->GetWorldPositionA(): %f, %f, %f | m_RopeInstanceB->GetWorldPositionB: %f, %f, %f",
				m_RopeInstanceB->GetInstanceA(),
				m_RopeInstanceB->GetInstanceB(),
				m_RopeInstanceB->GetAttachedTo(),
				m_RopeInstanceB->GetLength(),
				VEC3V_ARGS(m_RopeInstanceB->GetWorldPositionA()),
				VEC3V_ARGS(m_RopeInstanceB->GetWorldPositionB())
				);

			if(m_RopeInstanceB->GetConstraint())
			{
				vehicleDisplayf("[TOWTRUCK ROPE DEBUG] CVehicleGadgetTowArm::ProcessTowArmAndHook - Rope B constraint status: m_RopeInstanceB->GetConstraint()->IsBreakable(): %i | m_RopeInstanceB->GetConstraint()->IsBroken(): %i | m_RopeInstanceB->GetConstraint()->IsAtLimit(): %i | m_RopeInstanceB->GetConstraint()->GetInstanceA(): %p | m_RopeInstanceB->GetConstraint()->GetInstanceB(): %p",
					m_RopeInstanceB->GetConstraint()->IsBreakable(),
					m_RopeInstanceB->GetConstraint()->IsBroken(),
					m_RopeInstanceB->GetConstraint()->IsAtLimit(),
					m_RopeInstanceB->GetConstraint()->GetInstanceA(),
					m_RopeInstanceB->GetConstraint()->GetInstanceB()
					);
			}
		}
		else
		{
			if( pParent )
			{
				vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - No Rope B instance. Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			}
		}
    }
}


//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetForks
//////////////////////////////////////////////////////////////////////////
static dev_float sfForkliftMuscleStiffness = 1.0f;
static dev_float sfForkliftMuscleAngleStrength = 200.0f;
static dev_float sfForkliftMuscleSpeedStrength = 30.0f;
static dev_float sfForkliftMinAndMaxMuscleTorque = 35.0f;

CVehicleGadgetForks::CVehicleGadgetForks(s32 iForksBoneIndex, s32 iCarriageBoneIndex, float fMoveSpeed) :
CVehicleGadgetWithJointsBase( 2, fMoveSpeed, sfForkliftMuscleStiffness, sfForkliftMuscleAngleStrength, sfForkliftMuscleSpeedStrength, sfForkliftMinAndMaxMuscleTorque ),
m_VehicleForksAudio(NULL),
m_attachState(UNATTACHED),
m_iForks(iForksBoneIndex),
m_iCarriage(iCarriageBoneIndex),
m_bForceAttach(false),
m_bCanMoveForksUp(true),
m_bCanMoveForksDown(true),
m_bForksLocked(false),
m_bForksWithinPalletMiddleBox(false),
m_bForksInRegionAboveOrBelowPallet(false),
m_bForksWereWithinPalletMiddleBox(false),
m_bForksWereInRegionAboveOrBelowPallet(false)
{
    Assert(m_iForks > -1);
    GetJoint(0)->m_iBone = iForksBoneIndex;

    Assert(m_iCarriage > -1);
    GetJoint(1)->m_iBone = iCarriageBoneIndex;

	m_PrevPrismaticJointDistance = 0.0f;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::InitJointLimits(const CVehicleModelInfo* pModelInfo)
{
	CVehicleGadgetWithJointsBase::InitJointLimits(pModelInfo);

	// We want to drive the position of the prismatic fork joint too so that they don't continually slide down under gravity.
	m_JointInfo[0].m_PrismaticJointInfo.m_bOnlyControlSpeed = false;

}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::CreateNewBoundsForPallet(phArchetypeDamp* pPhysics, float vBoundingBoxMinZ, float vBoundingBoxMaxZ, bool bAddTopAndBottomBoxBounds)
{
	rage::phBoundComposite* pNewComposite = rage_new phBoundComposite();
	int nNumOriginalParts = 0;

	physicsAssert(pPhysics->GetIncludeFlags()&ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE);

	physicsAssert(pPhysics->GetBound()->GetType() == rage::phBound::COMPOSITE);
	rage::phBoundComposite* pOldComposite = static_cast<rage::phBoundComposite*>(pPhysics->GetBound());
	nNumOriginalParts = pOldComposite->GetNumBounds();

	// Make sure we only do this when necessary.
//	if(AssertVerify(nNumOriginalParts < CVehicleGadgetForks::NUM_PALLET_BOUNDS_TOTAL))
	{
		if(bAddTopAndBottomBoxBounds)
		{
			pNewComposite->Init(nNumOriginalParts+CVehicleGadgetForks::NUM_EXTRA_PALLET_BOUNDS); // Need three extra slots for the new box bounds.
		}
		else
		{
			pNewComposite->Init(nNumOriginalParts+1);
		}

		// Copy the original bounds into the new composite.
		for(int i = 0; i < nNumOriginalParts; ++i)
		{
			pNewComposite->SetBound(i, pOldComposite->GetBound(i));
			pNewComposite->SetCurrentMatrix(i, pOldComposite->GetCurrentMatrix(i));
		}

		// Now add the boxes at the end.
		// Box1: the main bound that most things will collide with. The forklift forks will collide
		// against this bound too when they are in the regions above and below the pallet (detected by collisions
		// with box2/box3).
		Vector3 vBoxSize = VEC3V_TO_VECTOR3(pPhysics->GetBound()->GetBoundingBoxSize());
		vBoxSize.z = vBoundingBoxMaxZ - vBoundingBoxMinZ;
		phBoundBox* pNewBox = rage_new phBoundBox(vBoxSize);
		Vec3V vBoxCentre, vBoxHalfWidth;
		pPhysics->GetBound()->GetBoundingBoxHalfWidthAndCenter(vBoxHalfWidth, vBoxCentre);
		vBoxCentre.SetZf(0.5f*vBoxSize.z);
		pNewBox->SetCentroidOffset(vBoxCentre);
		pNewComposite->SetBound(nNumOriginalParts, pNewBox);
		pNewBox->Release();
		if(bAddTopAndBottomBoxBounds)
		{
			// Box2&3: the forks will collide against these bounds when they are inside the pallet (detected through
			// collisions with box1) for better stability.
			const float fSizeIncrease = 2.0f;
			vBoxSize.z *= fSizeIncrease;
			pNewBox = rage_new phBoundBox(vBoxSize);
			Vec3V vBox2Centre = vBoxCentre + Vec3V(0.0f, 0.0f, 0.5f*(vBoundingBoxMaxZ-vBoundingBoxMinZ)+0.5f*vBoxSize.z);
			pNewBox->SetCentroidOffset(vBox2Centre);
			pNewComposite->SetBound(nNumOriginalParts+1, pNewBox);
			pNewBox->Release();
			//
			pNewBox = rage_new phBoundBox(vBoxSize);
			Vec3V vBox3Centre = vBoxCentre - Vec3V(0.0f, 0.0f, 0.5f*(vBoundingBoxMaxZ-vBoundingBoxMinZ)+0.5f*vBoxSize.z);
			pNewBox->SetCentroidOffset(vBox3Centre);
			pNewComposite->SetBound(nNumOriginalParts+2, pNewBox);
			pNewBox->Release();
		}

		// Necessary after we have added the extra bounds.
		pNewComposite->CalculateCompositeExtents();

		pPhysics->SetBound(pNewComposite);
	}
	pNewComposite->Release();
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::SetTypeAndIncludeFlagsForPallet(CObject* pObject, u32 nIndexOfFirstExtraBoxBound, const bool bAddTopAndBottomBoxBounds)
{
	// Copy the original bounds into the new composite.
	// Set the collision flags for the original composite parts so that they only collide with forklift forks.
	physicsAssert(pObject->GetPhysArch());
	physicsAssert(pObject->GetPhysArch()->GetBound());
	physicsAssert(pObject->GetPhysArch()->GetBound()->GetType()==phBound::COMPOSITE);
	phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pObject->GetPhysArch()->GetBound());
	pObject->GetPhysArch()->SetTypeFlags(ArchetypeFlags::GTA_OBJECT_TYPE);
	pObject->GetPhysArch()->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES|ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE);
	if(!pCompositeBound->GetTypeAndIncludeFlags())
	{
		pCompositeBound->AllocateTypeAndIncludeFlags();

		// Set type and include flags for all the artist generated bounds.
		for(int i = 0; i < nIndexOfFirstExtraBoxBound; ++i)
		{
			pCompositeBound->SetTypeFlags(i, ArchetypeFlags::GTA_OBJECT_TYPE);
			if(bAddTopAndBottomBoxBounds)
			{
				pCompositeBound->SetIncludeFlags(i, ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE);
			}
			else
			{
				pCompositeBound->SetIncludeFlags(i, ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE|ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
			}
		}

		// ... and now for the extra bounds created at runtime.
		if(bAddTopAndBottomBoxBounds)
		{
			physicsAssert(nIndexOfFirstExtraBoxBound+NUM_EXTRA_PALLET_BOUNDS == (u32)pCompositeBound->GetNumBounds());

			u32 nMiddleBox = nIndexOfFirstExtraBoxBound + PALLET_EXTRA_BOX_BOUND_MIDDLE;
			pCompositeBound->SetTypeFlags(nMiddleBox, ArchetypeFlags::GTA_OBJECT_TYPE);
			pCompositeBound->SetIncludeFlags(nMiddleBox, ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE|ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);

			u32 nTopBox = nIndexOfFirstExtraBoxBound + PALLET_EXTRA_BOX_BOUND_TOP;
			pCompositeBound->SetTypeFlags(nTopBox, ArchetypeFlags::GTA_OBJECT_TYPE);
			pCompositeBound->SetIncludeFlags(nTopBox, ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE);

			u32 nBottomBox = nIndexOfFirstExtraBoxBound + PALLET_EXTRA_BOX_BOUND_BOTTOM;
			pCompositeBound->SetTypeFlags(nBottomBox, ArchetypeFlags::GTA_OBJECT_TYPE);
			pCompositeBound->SetIncludeFlags(nBottomBox, ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE);
		}
		else
		{
			physicsAssert(nIndexOfFirstExtraBoxBound+1 == (u32)pCompositeBound->GetNumBounds());
			pCompositeBound->SetTypeFlags(nIndexOfFirstExtraBoxBound, ArchetypeFlags::GTA_OBJECT_TYPE);
			pCompositeBound->SetIncludeFlags(nIndexOfFirstExtraBoxBound, ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE|ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::SetDesiredAcceleration( float acceleration)
{
	JointInfo* pPrismaticJointInfo = GetJoint(0);
	Assert(pPrismaticJointInfo->m_bPrismatic);
	JointInfo* pRevoluteJointInfo = GetJoint(1);
	Assert(!pRevoluteJointInfo->m_bPrismatic);

	// Don't try and push the forks below the bottom of their extents.
	if(pPrismaticJointInfo->m_PrismaticJointInfo.m_fPosition <= 0.0f)
	{
		pPrismaticJointInfo->m_PrismaticJointInfo.m_fPosition = 0.0f;
		if(acceleration < 0.0f)
			acceleration = 0.0f;
	}
	pPrismaticJointInfo->m_fDesiredAcceleration = acceleration;

	// Move the revolute joint.
    pRevoluteJointInfo->m_fDesiredAcceleration = acceleration;
}

//////////////////////////////////////////////////////////////////////////

dev_float fForkSleepAccelThreshold = 0.001f;
bool CVehicleGadgetForks::WantsToBeAwake(CVehicle*)
{
	// The forks report that they can sleep as long as the player isn't trying to raise/lower them.
	for(s16 i = 0; i < m_iNumberOfJoints; i++)
	{
		if(fabs(m_JointInfo[i].m_fDesiredAcceleration) > fForkSleepAccelThreshold)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

const int MAX_NUM_ALLOWED_ATTACHMENTS=10;
void CVehicleGadgetForks::PopulateListOfAttachedObjects()
{
	// Walk the tree of objects in a breadth-first fashion and cache a ptr to each
	// object found in "list".

	m_AttachedObjects.clear();

	if(!m_pAttachEntity)
		return;

	atQueue<CPhysical*, MAX_NUM_ALLOWED_ATTACHMENTS*2> queue;

	// Enqueue the root node (the pallet).
	physicsAssertf(m_pAttachEntity->GetIsPhysical(), "attach entity doesn't derive from CPhysical.");
	CPhysical* pAttachedPhysical = static_cast<CPhysical*>(m_pAttachEntity.Get());
	queue.Push(pAttachedPhysical);

	// Start processing the tree.
	while(!queue.IsEmpty())
	{
		// Dequeue a node and examine it.
		CPhysical*& currentNode = queue.Pop();

		// Add this node to the list of objects.
		m_AttachedObjects.PushAndGrow(currentNode);

		// Enqueue the direct child and / or sibling of this node if they exist.
		CPhysical* pChild = static_cast<CPhysical*>(currentNode->GetChildAttachment());
		CPhysical* pSibling = static_cast<CPhysical*>(currentNode->GetSiblingAttachment());
		if(pChild)
			queue.Push(pChild);
		if(pSibling)
			queue.Push(pSibling);
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::Init(CVehicle* pVehicle)
{
	// Do we have forks?
	physicsAssert(pVehicle);
	s32 iLeftForkIdx = pVehicle->GetBoneIndex(VEH_FORK_LEFT);
	s32 iRightForkIdx = pVehicle->GetBoneIndex(VEH_FORK_RIGHT);

	if(iLeftForkIdx!=-1 && iRightForkIdx!=-1)
	{
		fragInst* pFragInst = pVehicle->GetVehicleFragInst();
		Assert(pFragInst);
		int nLeftForkComponentId = pFragInst->GetComponentFromBoneIndex(iLeftForkIdx);
		int nRightForkComponentId = pFragInst->GetComponentFromBoneIndex(iRightForkIdx);

		physicsAssert(pVehicle->GetPhysArch());
		physicsAssert(pVehicle->GetPhysArch()->GetBound());
		physicsAssert(pVehicle->GetPhysArch()->GetBound()->GetType()==phBound::COMPOSITE);
		phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pVehicle->GetPhysArch()->GetBound());

		// Set the collision type and include flags for the fork components.
		u32 nForkTypeFlags = ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE;
		u32 nForkIncludeFlags = ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;
		pCompositeBound->SetTypeFlags(nLeftForkComponentId, nForkTypeFlags);
		pCompositeBound->SetTypeFlags(nRightForkComponentId, nForkTypeFlags);
		pCompositeBound->SetIncludeFlags(nLeftForkComponentId, nForkIncludeFlags);
		pCompositeBound->SetIncludeFlags(nRightForkComponentId, nForkIncludeFlags);

		// Set the type / include flags on the parent frag to actually enable any collisions with the forks.
		u32 nTypeFlags = pVehicle->GetPhysArch()->GetTypeFlags()|ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE;
		pVehicle->GetPhysArch()->SetTypeFlags(nTypeFlags);
		// Also update the cached copy in the level if we have already been added.
		if(pFragInst->IsInLevel())
		{
			CPhysics::GetLevel()->SetInstanceTypeFlags(pFragInst->GetLevelIndex(), nTypeFlags);
		}

		m_VehicleForksAudio = (audVehicleForks*)pVehicle->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_FORKS);
		audAssert(m_VehicleForksAudio);
	}
}

void CVehicleGadgetForks::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_POSITION = 8;
	static const unsigned SIZEOF_ACCELERATION = 8;
	static const float MAX_POSITION = 1.7f;

	float desiredPosition = 0.0f;
	float desiredAcceleration = 0.0f;

	if (GetJoint(0))
	{
		desiredPosition = GetJoint(0)->m_PrismaticJointInfo.m_fDesiredPosition;
		desiredAcceleration = GetJoint(0)->m_fDesiredAcceleration;
	}

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, desiredPosition, MAX_POSITION, SIZEOF_POSITION, "Desired Position");
	SERIALISE_PACKED_FLOAT(serialiser, desiredAcceleration, 1.0f, SIZEOF_ACCELERATION, "Desired acceleration");

	if (GetJoint(0))
	{
		GetJoint(0)->m_fDesiredAcceleration = desiredAcceleration;

		if (GetJoint(1))
		{
			GetJoint(1)->m_fDesiredAcceleration = desiredAcceleration;
		}

		GetJoint(0)->m_PrismaticJointInfo.m_fDesiredPosition = desiredPosition;
	}
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
#define DEBUG_FORKLIFT_FSM(x) \
if(ms_bShowAttachFSM) \
{ \
	grcDebugDraw::Text(pParent->GetTransform().GetPosition(), Color_white, "FORKLIFT ATTACH STATE: "#x, true); \
}
#else // __BANK
#define DEBUG_FORKLIFT_FSM(x)
#endif // __BANK

void CVehicleGadgetForks::ProcessControl(CVehicle* pParent)
{
    CVehicleGadgetWithJointsBase::ProcessControl( pParent);

	JointInfo* pPrismaticJointInfo = GetJoint(0);
	Assert(pPrismaticJointInfo->m_bPrismatic);
	f32 movementSpeed = (m_PrevPrismaticJointDistance - pPrismaticJointInfo->m_PrismaticJointInfo.m_fPosition) * fwTimer::GetInvTimeStep();

	if(m_VehicleForksAudio REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		m_VehicleForksAudio->SetSpeed(movementSpeed);
	}
	
	m_PrevPrismaticJointDistance = pPrismaticJointInfo->m_PrismaticJointInfo.m_fPosition;

	// Constant to decide when the forks are moving.
	static float fMoveThreshold = 0.05f;
	static float fAttachStrength = -1.0f;//50.0f;

	switch(m_attachState)
	{
	case UNATTACHED:
		DEBUG_FORKLIFT_FSM("UNATTACHED");
		break;

	case READY_TO_ATTACH:
		DEBUG_FORKLIFT_FSM("READY_TO_ATTACH");

		m_bForksInRegionAboveOrBelowPallet = false;
		m_bForksWithinPalletMiddleBox = true;

		// If the forks are moving, we can make the attachment.
		if(fabs(GetJoint(0)->m_fDesiredAcceleration) > fMoveThreshold || m_bForceAttach)
		{
			// Reset this variable (set to true by AttachPalletInstanceToForks()).
			m_bForceAttach = false;

			if(m_pAttachEntity.Get())
			{
				if(!m_pAttachEntity->GetCurrentPhysicsInst())
				{
					m_attachState = UNATTACHED;
					break;
				}

				if(!m_constraintHandle.IsValid())
				{
					// Get the component index for the fork mechanism.
					int nForkComponent = 0;
					int nGroup = pParent->GetVehicleFragInst()->GetGroupFromBoneIndex(m_iForks);
					if(nGroup > -1)
					{
						fragTypeGroup* pGroup = pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[nGroup];
						nForkComponent = pGroup->GetChildFragmentIndex();
					}

					phInst* pForkliftInst = pParent->GetVehicleFragInst();
/*
					// Figure out the best position to attach the pallet.
					int nBoneIndex = pParent->GetBoneIndex(VEH_FORKS_ATTACH);
					if(Verifyf(nBoneIndex > -1, "Couldn't find forklift's attach bone."))
					{
						Matrix34 matAttachPoint;
						pParent->GetGlobalMtx(nBoneIndex, matAttachPoint);
						float fForkliftAttachOffsetZ = 0.0f;
						const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(m_pAttachEntity->GetBaseModelInfo()->GetModelNameHash());
						if(pTuning && pTuning->CanBePickedUpByForklift())
						{
							fForkliftAttachOffsetZ = pTuning->GetForkliftAttachOffsetZ();
						}
						matAttachPoint.d.z += fForkliftAttachOffsetZ;

						Matrix34 matPallet = MAT34V_TO_MATRIX34(m_pAttachEntity->GetMatrix());
						matPallet.d.z = matAttachPoint.d.z;

						// Move the pallet up or down so that the forks don't get attached penetrating the top/bottom of the pallet.
						m_pAttachEntity->SetMatrix(matPallet, true, true, true);
					}
*/

					pForkliftInst->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);
					m_pAttachEntity->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);

					static float massInvScaleA = 1.0f;
					static float massInvScaleB = 1.0f;

					// Configure the constraint.
					static bool bUsePushes = true;
					phConstraintFixed::Params constraintParams;
					constraintParams.instanceA = pForkliftInst;
					constraintParams.instanceB = m_pAttachEntity->GetCurrentPhysicsInst();
					constraintParams.componentA = (u16)nForkComponent;
					constraintParams.componentB = (u16)PALLET_EXTRA_BOX_BOUND_MIDDLE;
					constraintParams.breakable = (fAttachStrength > 0.0f);
					constraintParams.breakingStrength = fAttachStrength;
					constraintParams.usePushes = bUsePushes;
					constraintParams.separateBias = 1.0f;
					constraintParams.massInvScaleA = massInvScaleA;
					constraintParams.massInvScaleB = massInvScaleB;

					// Add the constraint to the world and store a handle to it.
					m_constraintHandle = PHCONSTRAINT->Insert(constraintParams);
				}
			}

			// Validate the constraint.
			if(Verifyf(m_constraintHandle.IsValid(), "Failed to create forklift-pallet constraint"))
			{
				m_attachState = ATTACHING;
			}
			else
			{
				m_attachState = UNATTACHED;
			}
		}
		break;

	case ATTACHING:
		{
			DEBUG_FORKLIFT_FSM("ATTACHING");
			// Refresh the list of objects attached to the pallet.
			PopulateListOfAttachedObjects();
			// ... and set the "no collision entity" ptr on each of these objects to be the forklift.
			for(int i = 0; i < m_AttachedObjects.GetCount(); ++i)
			{
				physicsAssertf(!m_AttachedObjects[i]->GetNoCollisionEntity(), "An object attaching to the forklift already has a 'no collision entity set'");
				m_AttachedObjects[i]->SetNoCollisionEntity(pParent);
			}

			// TODO: gradually move it towards a nice attachment
			// position / orientation as the forks are moved.
			m_attachState = ATTACHED;

			// Assign the forklift as the attach parent of the pallet so that we get some way
			// to access the forklift object when we need to (e.g. pallet fragments and we need
			// to clean up the attachment).
			m_pAttachEntity->CreateAttachmentExtensionIfMissing();
			m_pAttachEntity->GetAttachmentExtension()->SetParentAttachment(m_pAttachEntity, pParent);

			m_bForksInRegionAboveOrBelowPallet = false;
			m_bForksWithinPalletMiddleBox = true;
		}
		break;

	case ATTACHED:
		DEBUG_FORKLIFT_FSM("ATTACHED");

		m_bForksInRegionAboveOrBelowPallet = false;
		m_bForksWithinPalletMiddleBox = true;

		// Monitor the constraint for breaking.
		if(m_constraintHandle.IsValid() && CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle))
		{
			if(CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle)->IsBroken())
			{
				m_attachState = DETACHING;
			}
		}

		// Test if there is any ground beneath the pallet.
		{
			bool bGroundUnderPallet = false;
			// Pallet could get deleted before the forks on cleaning up (failing a mission / dying, say).
			if(m_pAttachEntity.Get())
			{
				Assert(dynamic_cast<CPhysical*>(m_pAttachEntity.Get()));
				CCollisionHistoryIterator collisionIterator(static_cast<CPhysical*>(m_pAttachEntity.Get())->GetFrameCollisionHistory(),
					/*bIterateBuildings=*/true, /*bIterateVehicles=*/true, /*bIteratePeds=*/false, /*bIterateObjects=*/true, /*bIterateOthers=*/false);
				while(const CCollisionRecord* pColRecord = collisionIterator.GetNext())
				{
					if(pColRecord->m_pRegdCollisionEntity.Get()!=pParent && DotProduct(pColRecord->m_MyCollisionNormal, ZAXIS) > 0.9f)
					{
						bGroundUnderPallet = true;
						break;
					}
				}
			}

			// Also detach if the forks are at their lowest position or if the pallet has something to rest on.
			if(IsAtLowestPosition() || bGroundUnderPallet)
			{
				m_attachState = DETACHING;
			}
		}
		break;

	case DETACHING:
		DEBUG_FORKLIFT_FSM("DETACHING");
		DetachPalletImmediatelyAndCleanUp(pParent);
		break;

	default:
		DEBUG_FORKLIFT_FSM("ERROR: UNKNOWN FSM STATE");
		Assertf(false, "Unknown attach state.");
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::AttachPalletInstanceToForks(CEntity* pPallet)
{
	if(const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(pPallet->GetBaseModelInfo()->GetModelNameHash()))
	{
		if(pTuning->CanBePickedUpByForklift())
		{
			m_pAttachEntity = RegdEnt(pPallet);

			// We assume the pallet has already been warped to the correct position by this point.
			m_attachState = READY_TO_ATTACH;

			// Set this to true so that we can decide to ignore the position of the forks to force a transition
			// to the "ATTACHING" state.
			m_bForceAttach = true;
			return;
		}
	}

	Assertf(false, "Trying to attach inappropriate object ('%s') to forklift forks.", pPallet->GetModelName());
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::DetachPalletFromForks()
{
	if(AssertVerify(m_pAttachEntity.Get()))
	{
		// Force the FSM into this state which will take care of cleaning up the constraint and everything else.
		m_attachState = DETACHING;
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::DetachPalletImmediatelyAndCleanUp(CVehicle* pParent)
{
	if(AssertVerify(m_pAttachEntity.Get()))
	{
		if(m_constraintHandle.IsValid())
		{
			CPhysics::GetConstraintManager()->Remove(m_constraintHandle);
			m_constraintHandle.Reset();
		}

		// Reset the "no collision entity" ptr on each of the attached objects before clearing the list.
		for(int i = 0; i < m_AttachedObjects.GetCount(); ++i)
		{
			m_AttachedObjects[i]->SetNoCollisionEntity(NULL);
			m_AttachedObjects[i]->CreateAttachmentExtensionIfMissing();
			m_AttachedObjects[i]->GetAttachmentExtension()->SetParentAttachment(m_AttachedObjects[i], NULL);
		}
		m_AttachedObjects.clear();

		if(pParent && pParent->GetVehicleFragInst())
		{
			pParent->GetVehicleFragInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
		}
		if(m_pAttachEntity.Get() && m_pAttachEntity->GetCurrentPhysicsInst())
		{
			m_pAttachEntity->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
		}

		// Assign the forklift as the attach parent of the pallet so that we get some way
		// to access the forklift object when we need to (e.g. the pallet fragments and we need
		// to clean up the attachment).
		m_pAttachEntity->CreateAttachmentExtensionIfMissing();
		m_pAttachEntity->GetAttachmentExtension()->SetParentAttachment(m_pAttachEntity, NULL);
	}

	m_bForksInRegionAboveOrBelowPallet = false;
	m_bForksWithinPalletMiddleBox = true;

	m_attachState = UNATTACHED;
}

//////////////////////////////////////////////////////////////////////////

bool CVehicleGadgetForks::IsAtLowestPosition()
{
    //Joint 0 is the prismatic joint
    Assert(GetJoint(0)->m_bPrismatic);

    const float sf_ClosedPositionAllowance = 0.01f;
    if(GetJoint(0)->m_PrismaticJointInfo.m_fPosition <= GetJoint(0)->m_PrismaticJointInfo.m_fClosedPosition+sf_ClosedPositionAllowance)
        return true;

    return false;
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
void CVehicleGadgetForks::RenderDebug()
{
	// Useful place to put debug stuff that needs to be seen while the game is paused.

	bool bValidPropSelected = false;
	Vector3 vPalletAttachAreaMin, vPalletAttachAreaMax;

	if(ms_bRenderDebugInfoForSelectedPallet || ms_bDrawForkAttachPoints)
	{
		CEntity* pSelectedPallet = CDebugScene::FocusEntities_Get(0);
		if(pSelectedPallet && pSelectedPallet->GetIsTypeObject())
		{
			// Make sure this object is set up for use with the forklift.
			u32 nModelNameHash = pSelectedPallet->GetBaseModelInfo()->GetModelNameHash();
			const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(nModelNameHash);
			if(pTuning && pTuning->CanBePickedUpByForklift())
			{
				bValidPropSelected = true;

				// Display the angled area within the pallet which determines where the forks can attach. Depending
				// on the value of a RAG toggle we can render the current values for this pallet if any are set, or
				// some tuning values also from RAG to assist in choosing the right dimensions for the box.
				Vector3 vPalletAttachOrigin;
				float fHalfWidth, fHalfLength, fHalfHeight;
				Color32 colour;
				if(ms_bRenderTuningValuesForSelectedPallet)
				{
					fHalfWidth = 0.5f*ms_fPalletAttachBoxWidth;
					fHalfLength = 0.5f*ms_fPalletAttachBoxLength;
					fHalfHeight = 0.5f*ms_fPalletAttachBoxHeight;
					vPalletAttachOrigin = Vector3(ms_vPalletAttachBoxOriginX, ms_vPalletAttachBoxOriginY, ms_vPalletAttachBoxOriginZ);
					colour = Color_blue;
				}
				else
				{
					// Otherwise display the values from the tuning file.
					Vector3 vBoxDims = pTuning->GetForkliftAttachAngledAreaDims();
					fHalfWidth = 0.5f*vBoxDims.x;
					fHalfLength = 0.5f*vBoxDims.y;
					fHalfHeight = 0.5f*vBoxDims.z;
					vPalletAttachOrigin = pTuning->GetForkliftAttachAngledAreaOrigin();
					colour = Color_purple;
				}
				vPalletAttachAreaMin = Vector3(vPalletAttachOrigin.x-fHalfWidth, vPalletAttachOrigin.y-fHalfLength, vPalletAttachOrigin.z-fHalfHeight);
				vPalletAttachAreaMax = Vector3(vPalletAttachOrigin.x+fHalfWidth, vPalletAttachOrigin.y+fHalfLength, vPalletAttachOrigin.z+fHalfHeight);

				if(ms_bRenderDebugInfoForSelectedPallet)
				{
					grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vPalletAttachAreaMin), VECTOR3_TO_VEC3V(vPalletAttachAreaMax),
						pSelectedPallet->GetMatrix(), colour, false);
				}
			}
		}

		// Test if the fork attach points are within the pallet attach angled area.
		if(ms_bDrawForkAttachPoints)
		{
			// Find any nearby forklifts.
			CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
			if(pLocalPlayer)
			{
				CEntityScannerIterator entityList = pLocalPlayer->GetPedIntelligence()->GetNearbyVehicles();
				CEntity* pEnt = entityList.GetFirst();
				while(pEnt)
				{
					Assert(pEnt->GetIsTypeVehicle());
					CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
					if(pVehicle->GetModelIndex()==MI_CAR_FORKLIFT)
					{
						// Find component indices for each fork.
						s32 iLeftForkIdx = pVehicle->GetBoneIndex(VEH_FORK_LEFT);
						s32 iRightForkIdx = pVehicle->GetBoneIndex(VEH_FORK_RIGHT);
						int nLeftForkComponentId = 0;
						int nRightForkComponentId = 0;
						Vector3 vForkAttachPoints[4];
						if(iLeftForkIdx!=-1 && iRightForkIdx!=-1)
						{
							fragInst* pFragInst = pVehicle->GetVehicleFragInst();
							Assert(pFragInst);
							nLeftForkComponentId = pFragInst->GetComponentFromBoneIndex(iLeftForkIdx);
							nRightForkComponentId = pFragInst->GetComponentFromBoneIndex(iRightForkIdx);

							// Use the fork attach bones to define 4 points which we will compare with an angled area on the pallet to
							// decide if we should attach or not.
							Matrix34 leftForkBoneMtx, rightForkBoneMtx;
							pVehicle->GetGlobalMtx(iLeftForkIdx, leftForkBoneMtx);
							pVehicle->GetGlobalMtx(iRightForkIdx, rightForkBoneMtx);

							TUNE_FLOAT(FORKLIFT_ATTACH_POINTS_LENGTH, 0.3f, 0.01f, 1.0f, 0.01f);
							TUNE_FLOAT(FORKLIFT_ATTACH_POINTS_OFFSET, 0.2f, -3.0f, 3.0f, 0.01f);
							vForkAttachPoints[0] = leftForkBoneMtx.d + FORKLIFT_ATTACH_POINTS_LENGTH*leftForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
							vForkAttachPoints[1] = leftForkBoneMtx.d - FORKLIFT_ATTACH_POINTS_LENGTH*leftForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
							vForkAttachPoints[2] = rightForkBoneMtx.d - FORKLIFT_ATTACH_POINTS_LENGTH*rightForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
							vForkAttachPoints[3] = rightForkBoneMtx.d + FORKLIFT_ATTACH_POINTS_LENGTH*rightForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
						}

						// Test if points are within the angled area.
						for(int i = 0; i < 4; ++i)
						{
							Color32 colour = Color_orange;
							if(bValidPropSelected)
							{
								Vector3 vPointInBoxSpace = VEC3V_TO_VECTOR3(UnTransformFull(pSelectedPallet->GetMatrix(), VECTOR3_TO_VEC3V(vForkAttachPoints[i])));
								if(geomPoints::IsPointInBox(vPointInBoxSpace, vPalletAttachAreaMin, vPalletAttachAreaMax))
								{
									colour = Color_green;
								}
							}

							grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vForkAttachPoints[i]), 0.3f, colour, 1, true);
						}

					}

					// Advance iterator.
					pEnt = entityList.GetNext();
				}
			}
		}
	}
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetForks::ProcessPreComputeImpacts(CVehicle *pVehicle, phCachedContactIterator &impacts)
{
	static float sfForksInvMassScale = 1.0f;
	static float sfBoxInvMassScale = 1.0f;

	// Reset these values so that the forks don't get permanently stuck.
	m_bCanMoveForksUp = true;
	m_bCanMoveForksDown = true;

	bool bGoodContactWithForkA = false;
	bool bGoodContactWithForkB = false;

	bool bImpactIsWithPallet = false;
	bool bForksInKnownBound = false;

	m_bForksWereWithinPalletMiddleBox = m_bForksWithinPalletMiddleBox;
	m_bForksWereInRegionAboveOrBelowPallet = m_bForksInRegionAboveOrBelowPallet;

	if(m_attachState == READY_TO_ATTACH || m_attachState == DETACHING)
	{
		m_bForksInRegionAboveOrBelowPallet = false;
		m_bForksWithinPalletMiddleBox = true;
		bForksInKnownBound = true;
	}

	// Find component indices for each fork.
	s32 iLeftForkIdx = pVehicle->GetBoneIndex(VEH_FORK_LEFT);
	s32 iRightForkIdx = pVehicle->GetBoneIndex(VEH_FORK_RIGHT);
	int nLeftForkComponentId = 0;
	int nRightForkComponentId = 0;
	Vector3 vForkAttachPoints[4];
	Matrix34 leftForkBoneMtx, rightForkBoneMtx;
	if(iLeftForkIdx!=-1 && iRightForkIdx!=-1)
	{
		fragInst* pFragInst = pVehicle->GetVehicleFragInst();
		Assert(pFragInst);
		nLeftForkComponentId = pFragInst->GetComponentFromBoneIndex(iLeftForkIdx);
		nRightForkComponentId = pFragInst->GetComponentFromBoneIndex(iRightForkIdx);

		// Use the fork attach bones to define 4 points which we will compare with an angled area on the pallet to
		// decide if we should attach or not.
		pVehicle->GetGlobalMtx(iLeftForkIdx, leftForkBoneMtx);
		pVehicle->GetGlobalMtx(iRightForkIdx, rightForkBoneMtx);

		TUNE_FLOAT(FORKLIFT_ATTACH_POINTS_LENGTH, 0.3f, 0.01f, 1.0f, 0.01f);
		TUNE_FLOAT(FORKLIFT_ATTACH_POINTS_OFFSET, 0.2f, -3.0f, 3.0f, 0.01f);
		vForkAttachPoints[0] = leftForkBoneMtx.d + FORKLIFT_ATTACH_POINTS_LENGTH*leftForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
		vForkAttachPoints[1] = leftForkBoneMtx.d - FORKLIFT_ATTACH_POINTS_LENGTH*leftForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
		vForkAttachPoints[2] = rightForkBoneMtx.d - FORKLIFT_ATTACH_POINTS_LENGTH*rightForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
		vForkAttachPoints[3] = rightForkBoneMtx.d + FORKLIFT_ATTACH_POINTS_LENGTH*rightForkBoneMtx.b + FORKLIFT_ATTACH_POINTS_OFFSET*leftForkBoneMtx.b;
	}

	impacts.Reset();
	while(!impacts.AtEnd())
	{
		if(impacts.IsConstraint())
		{
			++impacts;
			continue;
		}

		Vector3 vecNormal;
		impacts.GetMyNormal(vecNormal);

#if DEBUG_DRAW
		// Visualise even the impacts which are being filtered out.
		if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
			grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.1f, Color_grey, false);
#endif // DEBUG_DRAW

		// Do collision for forklift (try and reject collisions with the ground).
		bool bCollisionWithGround = (vecNormal.z > 0.7f) && (DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC()), vecNormal) > 0.7f)
			&& (impacts.GetDepth() > 0.0f);

		int nImpactComponentIndex = impacts.GetMyComponent();
		bool bImpactWithForkMechanism = nImpactComponentIndex == nLeftForkComponentId || nImpactComponentIndex==nRightForkComponentId;

		if(bImpactWithForkMechanism && impacts.GetInstanceB())
		{
			// Reduce the mass ratio of the box against the forklift so we don't need to give the forklift
			// really stiff springs.
			impacts.SetMassInvScales(sfForksInvMassScale, sfBoxInvMassScale);

#if DEBUG_DRAW
			if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
				grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.08f, Color_yellow, true);
#endif // DEBUG_DRAW

			// There is a special pallet model which is designed to work really well with the forklift. Here
			// we check if the forks are in a good position to attach the pallet.
			if(impacts.GetOtherInstance()->GetUserData())
			{
				CEntity* pOtherEntity = static_cast<CEntity*>(impacts.GetOtherInstance()->GetUserData());
				if(pOtherEntity->IsArchetypeSet())
				{
					u32 nModelNameHash = pOtherEntity->GetBaseModelInfo()->GetModelNameHash();
					const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(nModelNameHash);

					bool bProcessThisObject = true;
					if(pTuning)
					{
						if(pOtherEntity->m_nFlags.bIsFrag && !pTuning->ThisFragCanBePickedUpByForkliftWhenBroken())
						{
							fragInst* pFragInst = pOtherEntity->GetFragInst();
							int nTotalNumParts = pFragInst->GetTypePhysics()->GetNumChildren();
							for(int nChild = 0; nChild < nTotalNumParts; ++nChild)
							{
								if(pFragInst->GetChildBroken(nChild))
								{
									bProcessThisObject = false;
									break;
								}
							}
						}
					}
					else
					{
						bProcessThisObject = false;
					}

					if(pTuning && pTuning->CanBePickedUpByForklift() && bProcessThisObject)
					{
						bImpactIsWithPallet = true;

						// Test if all fork attach points are within the attachment angled area of the prop.
						bool bForksWithinAttachRegion = true;
						bool bAnyForkAttachPointInAttachRegion = false;
						Vector3 vBoxDims = pTuning->GetForkliftAttachAngledAreaDims();
						float fHalfWidth = 0.5f*vBoxDims.x;
						float fHalfLength = 0.5f*vBoxDims.y;
						float fHalfHeight = 0.5f*vBoxDims.z;
						Vector3 vPalletAttachOrigin = pTuning->GetForkliftAttachAngledAreaOrigin();
						Vector3 vPalletAttachAreaMin(vPalletAttachOrigin.x-fHalfWidth, vPalletAttachOrigin.y-fHalfLength, vPalletAttachOrigin.z-fHalfHeight);
						Vector3 vPalletAttachAreaMax(vPalletAttachOrigin.x+fHalfWidth, vPalletAttachOrigin.y+fHalfLength, vPalletAttachOrigin.z+fHalfHeight);
						for(int i = 0; i < 4; ++i)
						{
							Vector3 vPointInBoxSpace = VEC3V_TO_VECTOR3(UnTransformFull(pOtherEntity->GetMatrix(), VECTOR3_TO_VEC3V(vForkAttachPoints[i])));
							if(!geomPoints::IsPointInBox(vPointInBoxSpace, vPalletAttachAreaMin, vPalletAttachAreaMax))
							{
								bForksWithinAttachRegion = false;
							}
							else
							{
								bAnyForkAttachPointInAttachRegion = true;
							}
						}

						// Have we hit one of the original parts of the geometry and are we in the right region for attachment?
						//if(nOtherComp < nNumPalletBoundsOriginal && m_bForksWithinPalletMiddleBox)
						{
							// Check if the pallet is flat enough for attachment (relative to the forks).
							static float sfPalletFlatnessTolerance = 0.95f;
							float fDotProductPalletForkliftUpVectors =
								Dot(pOtherEntity->GetMatrix().c(), pVehicle->GetMatrix().c()).Getf();
							bool bPalletFlatEnough = fabs(fDotProductPalletForkliftUpVectors) > sfPalletFlatnessTolerance;

							// Now check that the contact is in the top half of the bounding box and well
							// within the edges (check if the pallet is upside-down). Contact position is transformed into the
							// local space of the pallet's OBB.
							Vector3 vContactPosLocal = VEC3V_TO_VECTOR3(impacts.GetOtherPosition());
							MAT34V_TO_MATRIX34(pOtherEntity->GetMatrix()).UnTransform(vContactPosLocal);
							Vector3 vDiffPos = vContactPosLocal - vPalletAttachOrigin;
							float fDiffZ = vDiffPos.z;
							TUNE_FLOAT(FORKLIFT_Z_ATTACH_TOLERANCE_MIN, 0.5f, 0.0f, 1.0f, 0.1f);
							TUNE_FLOAT(FORKLIFT_Z_ATTACH_TOLERANCE_MAX, 1.0f, 0.0f, 1.0f, 0.1f);
							bool bContactWithinMarginsZ;
							if(fDotProductPalletForkliftUpVectors > 0.0f)
							{
								bContactWithinMarginsZ = (fDiffZ>FORKLIFT_Z_ATTACH_TOLERANCE_MIN*fHalfHeight) && bForksWithinAttachRegion;
							}
							else
							{
								bContactWithinMarginsZ = (fDiffZ<-FORKLIFT_Z_ATTACH_TOLERANCE_MIN*fHalfHeight) &&  bForksWithinAttachRegion;
							}

							// Stop the forks moving down if we hit the top of an object or scenery.
							if(!bAnyForkAttachPointInAttachRegion && vecNormal.z > 0.8f)
							{
								m_bCanMoveForksDown = false;
							}

							// Decide if this contact is acceptable or not.
							if(bPalletFlatEnough && bForksWithinAttachRegion && bContactWithinMarginsZ)
							{
								m_pAttachEntity = RegdEnt(pOtherEntity);
								bGoodContactWithForkA = true;
								bGoodContactWithForkB = true;
							}

							// Update these variables as well since we might not have triggered them elsewhere since the
							// impact is with the pallet top/bottom.
							if(bAnyForkAttachPointInAttachRegion)
							{
								m_bForksInRegionAboveOrBelowPallet = false;
								m_bForksWithinPalletMiddleBox = true;
								bForksInKnownBound = true;
							}
						}
					}
				}

				// If the forks are hitting something fixed while moving up, stop them moving up.
				dev_float sfMaxMassForDynamicObject = 550.0f;
				float fMass = sfMaxMassForDynamicObject;
				bool bContactOnUpperSideOfForks = false;
				if(pOtherEntity->GetIsPhysical())
				{
					CPhysical* pOtherPhysical = static_cast<CPhysical*>(pOtherEntity);
					fMass = pOtherPhysical->GetMass();
				}
				if(iLeftForkIdx!=-1 && iRightForkIdx!=-1)
				{
					// Transform the current contact into 
					Vector3 vContactPosForkLocal = VEC3V_TO_VECTOR3(impacts.GetOtherPosition());
					leftForkBoneMtx.UnTransform(vContactPosForkLocal);
					dev_float sfBoneThresholdZ = 0.0f;
					bContactOnUpperSideOfForks = vContactPosForkLocal.z > sfBoneThresholdZ;
				}
				if((pOtherEntity->GetIsAnyFixedFlagSet() || fMass>sfMaxMassForDynamicObject)
					&& bContactOnUpperSideOfForks
					&& m_attachState!=READY_TO_ATTACH)
				{
#if DEBUG_DRAW
					if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
						grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.08f, Color_green, true);
#endif // DEBUG_DRAW
					m_bCanMoveForksUp = false;
				}
			}

			// If the forks have hit the ground, stop them moving downwards.
			if(bCollisionWithGround && !bImpactIsWithPallet)
			{
#if DEBUG_DRAW
				if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
					grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.08f, Color_green, true);
#endif // DEBUG_DRAW
				m_bCanMoveForksDown = false;
			}

		} // Collision with forks.

		// Look for collisions on the carriage (the part on which the fork mechanism slides up and down).
		s32 nCarriageBoneIdx = pVehicle->GetBoneIndex(VEH_FORKS);
		if(nCarriageBoneIdx != -1)
		{
			int nCarriageComponent = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(nCarriageBoneIdx);
			int nImpactComponentIndex = impacts.GetMyComponent();
			bool bImpactWithCarriage = nImpactComponentIndex==nCarriageComponent;

			if(bImpactWithCarriage && impacts.GetInstanceB())
				{
					// Reduce the mass ratio of the box against the forklift so we don't need to give the forklift
					// really stiff springs.
//				impacts.SetMassInvScales(sfForksInvMassScale, sfBoxInvMassScale);

#if DEBUG_DRAW
					if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
						grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.08f, Color_purple, true);
#endif // DEBUG_DRAW

				// Stop the forks moving down if we hit the top of an object or scenery.
				if(vecNormal.z > 0.8f)
					{
#if DEBUG_DRAW
						if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
							grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.08f, Color_cyan, true);
#endif // DEBUG_DRAW
					m_bCanMoveForksDown = false;
				}
			}
		} // "Carriage" group.

		++impacts;
	} // Iteration over impacts.

	// If we didn't get a contact with any of the pallet's special bound regions, we need to reset the
	// required flags.
	if(!bForksInKnownBound && m_attachState==UNATTACHED)
	{
		m_bForksInRegionAboveOrBelowPallet = false;
		m_bForksWithinPalletMiddleBox = false;
	}

	if(bGoodContactWithForkA && bGoodContactWithForkB)
	{
		if(m_attachState==UNATTACHED)
		{
			m_attachState = READY_TO_ATTACH;
		}
	}
	else
	{
		// Stop the pallet reattaching if we slide the forks out while we had good contact points.
		if(m_attachState == READY_TO_ATTACH)
		{
			m_attachState = UNATTACHED;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetHandlerFrame
//////////////////////////////////////////////////////////////////////////
static dev_float sfHandlerMuscleStiffness = 1.0f;
static dev_float sfHandlerMuscleAngleStrength = 500.0f;
static dev_float sfHandlerMuscleSpeedStrength = 30.0f;
static dev_float sfHandlerMinAndMaxMuscleTorque = 35.0f;

CVehicleGadgetHandlerFrame::CVehicleGadgetHandlerFrame(s32 iFrame1BoneIndex, s32 iFrame2BoneIndex, float fMoveSpeed)
: CVehicleGadgetWithJointsBase(2, fMoveSpeed, sfHandlerMuscleStiffness, sfHandlerMuscleAngleStrength, sfHandlerMuscleSpeedStrength, sfHandlerMinAndMaxMuscleTorque),
m_attachState(UNATTACHED),
m_iFrame1(iFrame1BoneIndex),
m_iFrame2(iFrame2BoneIndex),
m_bForceAttach(false),
m_bReadyToAttach(false),
m_bTryingToMoveFrameDown(false),
m_nFramesWithoutCollisionHistory(0),
m_VehicleHandlerAudio(NULL)
{
	Assert(m_iFrame1 > -1);
	GetJoint(0)->m_iBone = iFrame1BoneIndex;

	Assert(m_iFrame2 > -1);
	GetJoint(1)->m_iBone = iFrame2BoneIndex;

	m_PrevPrismaticJointDistance = 0.0f;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetHandlerFrame::Init(CVehicle* pVehicle)
{
/*
	// Do we have forks?
	if(m_iFrame1!=-1)
	{
		fragInst* pFragInst = pVehicle->GetVehicleFragInst();
		Assert(pFragInst);
		int nGroup = pFragInst->GetGroupFromBoneIndex(m_iFrame1);
		if(nGroup > -1)
		{
			fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetAllGroups()[nGroup];

			phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pVehicle->GetPhysArch()->GetBound());

			physicsAssert(pVehicle);
			physicsAssert(pVehicle->GetPhysArch());

			// The fork group has three components, we want the last two which are the actual forks not the carriage.
			int nComponentIndex = pGroup->GetChildFragmentIndex();
			for(int i = 1; i < pGroup->GetNumChildren(); ++i)
			{
				physicsAssert(pVehicle->GetPhysArch()->GetBound());
				physicsAssert(pVehicle->GetPhysArch()->GetBound()->GetType()==phBound::COMPOSITE);

				pCompositeBound->SetIncludeFlags(nComponentIndex+i, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
				pCompositeBound->SetTypeFlags(nComponentIndex+i, ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE);
			}

			// Set the type / include flags on the parent frag to actually enable any collisions with the forks.
			u32 nTypeFlags = pVehicle->GetPhysArch()->GetTypeFlags()|ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE;
			pVehicle->GetPhysArch()->SetTypeFlags(nTypeFlags);
			// Also update the cached copy in the level if we have already been added.
			if(pFragInst->IsInLevel())
			{
				CPhysics::GetLevel()->SetInstanceTypeFlags(pFragInst->GetLevelIndex(), nTypeFlags);
			}
		}
	}
*/
	m_VehicleHandlerAudio = (audVehicleHandlerFrame*)pVehicle->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_HANDLER_FRAME);
	audAssert(m_VehicleHandlerAudio);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetHandlerFrame::InitJointLimits(const CVehicleModelInfo* pModelInfo)
{
	CVehicleGadgetWithJointsBase::InitJointLimits(pModelInfo);

	// We want to drive the position of the prismatic frame joint too so that they don't continually slide down under gravity.
	m_JointInfo[1].m_PrismaticJointInfo.m_bOnlyControlSpeed = false;
}

//////////////////////////////////////////////////////////////////////////

dev_float sfHandlerFrameMotionTolerance = 0.90f;

void CVehicleGadgetHandlerFrame::SetDesiredAcceleration( float acceleration)
{
	JointInfo* pJointInfo0 = GetJoint(0);
	Assert(!pJointInfo0->m_bPrismatic);
	JointInfo* pJointInfo1 = GetJoint(1);
	Assert(pJointInfo1->m_bPrismatic);

	// Left stick must be closer to the top / bottom before we allow the frame to move up / down.
	
	if(fabs(acceleration)<sfHandlerFrameMotionTolerance)
		acceleration = 0.0f;

	pJointInfo0->m_fDesiredAcceleration = acceleration;
	pJointInfo1->m_fDesiredAcceleration = acceleration;
}

//////////////////////////////////////////////////////////////////////////

dev_float fFrameSleepPosThreshold = 0.001f;
bool CVehicleGadgetHandlerFrame::WantsToBeAwake(CVehicle*)
{
	// The frame reports that it can sleep as long as the player isn't trying to raise/lower it.
	for(s16 i = 0; i < m_iNumberOfJoints; ++i)
	{
		// The frame reports that it needs to be awake if the player isn't trying to raise/lower it.
		if(fabs(m_JointInfo[i].m_fDesiredAcceleration) > fForkSleepAccelThreshold)
		{
			return true;
		}
		// ... or if it is being moved by script command.
		else if(m_JointInfo[i].m_bPrismatic)
		{
			if(fabs(m_JointInfo[i].m_PrismaticJointInfo.m_fDesiredPosition - m_JointInfo[i].m_PrismaticJointInfo.m_fPosition) > fFrameSleepPosThreshold)
			{
				return true;
			}
		}
		else
		{
			if(fabs(m_JointInfo[i].m_1dofJointInfo.m_fDesiredAngle - m_JointInfo[i].m_1dofJointInfo.m_fAngle) > fFrameSleepPosThreshold)
			{
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

#if __BANK
#define DEBUG_HANDLER_FRAME_FSM(x) \
	if(ms_bShowAttachFSM) \
{ \
	grcDebugDraw::Text(pParent->GetTransform().GetPosition(), Color_white, "HANDLER FRAME ATTACH STATE: "#x, true); \
}

void CVehicleGadgetHandlerFrame::DebugFrameMotion(CVehicle* pParent)
{
	if(ms_bDebugFrameMotion)
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(pParent->GetTransform().GetPosition())+Vector3(0.0f,0.0f,1.0f);
		u32 nNoOfLines = 0;
		if(ms_bShowAttachFSM) nNoOfLines++;
		char zText[256];
		Color32 colour = Color_yellow;
		colour.SetAlpha(180);

		sprintf(zText, "Can move frame up: %s", m_bCanMoveFrameUp ? "true" : "false");
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;

		sprintf(zText, "Can move frame down: %s", m_bCanMoveFrameDown ? "true" : "false");
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;

		sprintf(zText, "Trying to move frame down: %s", m_bTryingToMoveFrameDown ? "true" : "false");
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;

		sprintf(zText, "Locked: %s", m_bHandlerFrameLocked ? "true" : "false");
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;

		sprintf(zText, "Force attach: %s", m_bForceAttach ? "true" : "false");
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;

		sprintf(zText, "Ready to attach: %s", m_bReadyToAttach ? "true" : "false");
		grcDebugDraw::Text(vPos, colour, 0, nNoOfLines*grcDebugDraw::GetScreenSpaceTextHeight(), zText);
		nNoOfLines++;
	}
}

#else // __BANK
#define DEBUG_HANDLER_FRAME_FSM(x)
#endif // __BANK
void CVehicleGadgetHandlerFrame::ProcessControl(CVehicle* pParent)
{
	CVehicleGadgetWithJointsBase::ProcessControl( pParent);

	JointInfo* pJointInfo = GetJoint(1);
	Assert(pJointInfo->m_bPrismatic);

	f32 movementSpeed = WantsToBeAwake(pParent)? (m_PrevPrismaticJointDistance - pJointInfo->m_PrismaticJointInfo.m_fPosition) * fwTimer::GetInvTimeStep() : 0.0f;

	if(m_VehicleHandlerAudio)
	{
		f32 audioAcceleration = pJointInfo->m_fDesiredAcceleration;
		f32 audioMovementSpeed = movementSpeed;

		if((pJointInfo->m_PrismaticJointInfo.m_fPosition <= (pJointInfo->m_PrismaticJointInfo.m_fClosedPosition + 0.1f) && pJointInfo->m_fDesiredAcceleration < 0.0f) ||
		   (pJointInfo->m_PrismaticJointInfo.m_fPosition >= (pJointInfo->m_PrismaticJointInfo.m_fOpenPosition - 0.1f) && pJointInfo->m_fDesiredAcceleration > 0.0f))
		{
			audioAcceleration = 0.0f;
			audioMovementSpeed = 0.0f;
		}

		m_VehicleHandlerAudio->SetSpeed(audioMovementSpeed, audioAcceleration);
	}

	m_PrevPrismaticJointDistance = pJointInfo->m_PrismaticJointInfo.m_fPosition;

	// Get the component index for the frame mechanism.
	int nBoneIndex = pParent->GetBoneIndex(VEH_HANDLER_FRAME_2);
	fragInst* pHandlerInst = pParent->GetVehicleFragInst();
	Assert(pHandlerInst);
	int nFrameComponent = pHandlerInst->GetComponentFromBoneIndex(nBoneIndex);

	// Check for contacts with the frame. If there are none this frame, reset the flag which allows it to move down since
	// this will be skipped if we don't run the PreComputeImpacts() callback.
	CCollisionHistory* pCollisionHistory = pParent->GetFrameCollisionHistory();
	Assert(pHandlerInst->IsInLevel());
	if(!m_bCanMoveFrameDown && CPhysics::GetLevel()->IsActive(pHandlerInst->GetLevelIndex())
		&& (!pCollisionHistory || !pCollisionHistory->GetMostSignificantCollisionRecord()) )
	{
		m_nFramesWithoutCollisionHistory++;
		static dev_u32 nNumFramesToWait = 10;
		// Only reset the flag which allows the player to move the frame down once they have stopped pushing down on the left-stick.
		if(m_nFramesWithoutCollisionHistory > nNumFramesToWait && !m_bTryingToMoveFrameDown)
		{
			m_bCanMoveFrameDown = true;
			m_nFramesWithoutCollisionHistory = 0;
		}
	}
	else
	{
		m_nFramesWithoutCollisionHistory = 0;
	}

	if (pParent->IsNetworkClone() && m_attachEntity.GetEntity() && m_attachState == UNATTACHED)
	{
		AttachContainerInstanceToFrameWhenLinedUp(m_attachEntity.GetEntity());
	}

	// Useful for tuning the behaviour of the frame.
/*
#if __DEV
	TUNE_FLOAT(HANDLER_MuscleStiffness, 1.0f, -1000.0f, 1000.0f, 0.1f);
	TUNE_FLOAT(HANDLER_MuscleAngleStrength, 500.0f, -1000.0f, 1000.0f, 0.1f);
	TUNE_FLOAT(HANDLER_MuscleSpeedStrength, 30.0f, -1000.0f, 1000.0f, 0.1f);
	TUNE_FLOAT(HANDLER_MinAndMaxMuscleTorque, 35.0f, -1000.0f, 1000.0f, 0.1f);
	m_fMuscleStiffness = HANDLER_MuscleStiffness;
	m_fMuscleAngleOrPositionStrength = HANDLER_MuscleAngleStrength;
	m_fMuscleSpeedStrength = HANDLER_MuscleSpeedStrength;
	m_fMinAndMaxMuscleTorqueOrForce = HANDLER_MinAndMaxMuscleTorque;
#endif // __DEV
*/

	netObject* pAttachEntityNetObj = NULL;

	if (NetworkInterface::IsGameInProgress() && m_attachEntity.GetEntity())
	{
		pAttachEntityNetObj = NetworkUtils::GetNetworkObjectFromEntity(m_attachEntity.GetEntity());
	}

	// try to keep the attached container local in MP
	if (!pParent->IsNetworkClone() && pAttachEntityNetObj)
	{
		if (pAttachEntityNetObj->IsClone())
		{
			CRequestControlEvent::Trigger(pAttachEntityNetObj NOTFINAL_ONLY(, "CVehicleGadgetHandlerFrame::ProcessControl"));
		}
		else
		{
			SafeCast(CNetObjProximityMigrateable, pAttachEntityNetObj)->KeepProximityControl();
		}
	}

	// Constant to decide when the frame is moving.
	//static float fMoveThreshold = 0.05f;
	static float fAttachStrength = 500.0f;

	switch(m_attachState)
	{
	case UNATTACHED:
		DEBUG_HANDLER_FRAME_FSM("UNATTACHED");
		if(IsReadyToAttach() && m_attachEntity.GetEntity())
		{
			m_attachState = READY_TO_ATTACH;
		}
		break;

	case READY_TO_ATTACH:
		DEBUG_HANDLER_FRAME_FSM("READY_TO_ATTACH");

		// If the frame is moving, we can make the attachment.
		if(IsReadyToAttach() || m_bForceAttach)
		{
			if (m_attachEntity.GetEntity())
			{
				if(!m_constraintHandle.IsValid())
				{
					// Figure out the best position to attach the container.
					if(m_bForceAttach && Verifyf(nBoneIndex > -1, "Couldn't find attach bone on the Handler."))
					{
						Matrix34 matAttachPoint = pParent->GetLocalMtx(nBoneIndex);
						matAttachPoint.RotateLocalZ(90.0f*PI/180.0f);
						matAttachPoint.d += Vector3(0.0f, 6.02f, -2.05f);
						Matrix34 matHandler = MAT34V_TO_MATRIX34(pParent->GetMatrix());
						matAttachPoint.Dot(matHandler);

						// Move the container up or down so that the frame doesn't get attached penetrating the top/bottom of the container.
						m_attachEntity.GetEntity()->SetMatrix(matAttachPoint, true, true, true);
					}

					pHandlerInst->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);
					Assert(m_attachEntity.GetEntity()->GetCurrentPhysicsInst());
					m_attachEntity.GetEntity()->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);

					static float massInvScaleA = 0.1f;
					static float massInvScaleB = 10.0f;

					// Configure the constraint.
					static bool bUsePushes = true;
					phConstraintFixed::Params constraintParams;
					constraintParams.instanceA = pHandlerInst;
					constraintParams.instanceB = m_attachEntity.GetEntity()->GetCurrentPhysicsInst();
					constraintParams.componentA = (u16)nFrameComponent;
					constraintParams.componentB = (u16)0;
					constraintParams.breakable = (fAttachStrength > 0.0f);
					constraintParams.breakingStrength = fAttachStrength;
					constraintParams.usePushes = bUsePushes;
					constraintParams.separateBias = 1.0f;
					constraintParams.massInvScaleA = massInvScaleA;
					constraintParams.massInvScaleB = massInvScaleB;

					// Add the constraint to the world and store a handle to it.
					m_constraintHandle = PHCONSTRAINT->Insert(constraintParams);
				}
			}

			// Validate the constraint.
			if(Verifyf(m_constraintHandle.IsValid(), "Failed to create handler-container constraint"))
			{
				// Start driving the frame up to the desired position. This helps give a visual cue to the player that the
				// attachment has been made.
				if(!m_bForceAttach)
				{
					m_fFrameDesiredAttachHeight = GetCurrentHeight()+ms_fFrameUpwardOffsetAfterAttachment;
					m_fFrameDesiredAttachHeight = rage::Clamp(m_fFrameDesiredAttachHeight, pJointInfo->m_PrismaticJointInfo.m_fClosedPosition,
						pJointInfo->m_PrismaticJointInfo.m_fOpenPosition-0.05f);
					SetDesiredHeight(m_fFrameDesiredAttachHeight);
				}

				physicsAssertf(!m_attachEntity.GetEntity()->GetNoCollisionEntity(), "Container object attaching to the handler already has a 'no collision entity set'");
				m_attachEntity.GetEntity()->SetNoCollisionEntity(pParent);

				// Reset this variable (set to true by script attach command).
				m_bForceAttach = false;

				// Initialise some variables used in interpolating the container's position and orientation to the ideal.
				m_fInterpolationValue = 0.0f;
				m_matContainerInitial = MAT34V_TO_MATRIX34(m_attachEntity.GetEntity()->GetTransform().GetMatrix());

				m_attachState = ATTACHING;
			}
			else
			{
				// Reset this variable (set to true by script attach command).
				m_bForceAttach = false;
				m_bReadyToAttach = false;

				m_attachState = DETACHING;
			}
		}
		break;

	case ATTACHING:
		DEBUG_HANDLER_FRAME_FSM("ATTACHING");
		{
			// Player control is locked until we are finished attaching.
			m_bCanMoveFrameDown = false;
			m_fLockedDownwardMotionPos = Clamp(GetCurrentHeight(), pJointInfo->m_PrismaticJointInfo.m_fClosedPosition, 0.9f*pJointInfo->m_PrismaticJointInfo.m_fOpenPosition);

			// Compare with the container's current matrix and decide how much to lerp it to the ideal position/orientation.
			if(m_attachEntity.GetEntity())
			{
				// Figure out the best position to attach the container.
				int nBoneIndex = pParent->GetBoneIndex(VEH_HANDLER_FRAME_2);
				Matrix34 matIdealAttach;
				if(Verifyf(nBoneIndex > -1, "Couldn't find attach bone on the Handler."))
				{
					Matrix34 matHandler = MAT34V_TO_MATRIX34(pParent->GetMatrix());
					Matrix34 matContainer = MAT34V_TO_MATRIX34(m_attachEntity.GetEntity()->GetTransform().GetMatrix());
					matIdealAttach = pParent->GetLocalMtx(nBoneIndex);

					if(matHandler.b.FastAngle(matContainer.a) > 0.5f*PI)
						matIdealAttach.RotateLocalZ(-90.0f*PI/180.0f);
					else
						matIdealAttach.RotateLocalZ(90.0f*PI/180.0f);
					matIdealAttach.d += Vector3(0.0f, 6.02f, -2.05f);
					matIdealAttach.Dot(matHandler);
				}

				Quaternion qIdeal, qInitial;
				matIdealAttach.ToQuaternion(qIdeal);
				m_matContainerInitial.ToQuaternion(qInitial);
				qInitial.PrepareSlerp(qIdeal);
				qInitial.Slerp(m_fInterpolationValue, qIdeal);
				Matrix34 matContainerCurrent;
				matContainerCurrent.FromQuaternion(qInitial);

				// Linear interpolation.
				Vector3 vInterpDir = matIdealAttach.d - m_matContainerInitial.d;
				matContainerCurrent.d = m_matContainerInitial.d + m_fInterpolationValue*vInterpDir;
				// Vertical position looks better if moved with the frame.
				matContainerCurrent.d.z = matIdealAttach.d.z;

				// Advance the interpolation.
				m_fInterpolationValue += 0.1f;
				m_fInterpolationValue = rage::Clamp(m_fInterpolationValue, 0.0f, 1.0f);

				if(m_constraintHandle.IsValid() && CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle))
				{
					// Update the constraint to the perfect attach orientation.
					phConstraintFixed::Params constraintParams;
					CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle)->ReconstructBaseParams(constraintParams);
					CPhysics::GetConstraintManager()->Remove(m_constraintHandle);
					m_constraintHandle.Reset();

					m_attachEntity.GetEntity()->SetMatrix(matContainerCurrent, true, true, true);
					// Re-add the constraint to the world and store the new handle to it.
					m_constraintHandle = PHCONSTRAINT->Insert(constraintParams);
				}
			}

			// Monitor for the frame reaching the desired height and switch to the attached state to show we're done
			// fiddling with the attachment.
			if(GetCurrentHeight() >= (m_fFrameDesiredAttachHeight-0.02f) && m_fInterpolationValue > 0.95f)
			{
				m_attachState = ATTACHED;
			}
		}
		break;

	case ATTACHED:
		DEBUG_HANDLER_FRAME_FSM("ATTACHED");
		{
			// Only reset the flag which allows the player to move the frame down once the player has stopped pushing down on the left stick.
			CPed* pDriver = pParent->GetDriver();
			if(pDriver && pDriver->GetControlFromPlayer() && pDriver->GetControlFromPlayer()->GetVehicleSteeringUpDown().GetNorm() <= 0.0f)
			{
				m_bCanMoveFrameDown = true;
			}
			
			if (m_attachEntity.GetEntity())
			{
				// Some systems need to know if a given container is attached to the handler so we set a flag on the object here. This
				// gets reset every time the object goes to sleep.
				Assert(m_attachEntity.GetEntity()->GetIsTypeObject());
				static_cast<CObject*>(m_attachEntity.GetEntity())->SetAttachedToHandler(true);

				// B*1875926: Allow the constraint to be broken if the crate is colliding with something.
				// This prevents erratic movement due to the crate being pushed into an immovable object and fighting against the constraint.
				// Making it only happen on collision prevents the crate from flying off the handler frame if the vehicle hits something and comes to a sudden stop.
				CCollisionHistory *pCrateColHistory = static_cast<CObject*>(m_attachEntity.GetEntity())->GetFrameCollisionHistory();
				if(m_constraintHandle.IsValid() && CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle) && pCrateColHistory)
				{
					bool bCrateTouchingSomething = pCrateColHistory->GetNumCollidedEntities() > 0;

					if(bCrateTouchingSomething)
						CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle)->SetBreakable(true, fAttachStrength);		
					else			
						CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle)->SetBreakable(false);
				}
			}

			if(m_constraintHandle.IsValid() && CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle))
			{
				// Monitor the constraint for breaking.
				if(CPhysics::GetConstraintManager()->GetTemporaryPointer(m_constraintHandle)->IsBroken())
				{
					m_attachState = DETACHING;
				}
			}

			if (pParent->IsNetworkClone() && m_attachEntity.GetEntityID() == NETWORK_INVALID_OBJECT_ID)
			{
				m_attachState = DETACHING;
			}
		}
		break;

	case DETACHING:
		DEBUG_HANDLER_FRAME_FSM("DETACHING");
		if(m_constraintHandle.IsValid())
		{
			CPhysics::GetConstraintManager()->Remove(m_constraintHandle);
			m_constraintHandle.Reset();
		}

		if(pParent && pParent->GetVehicleFragInst())
		{
			pParent->GetVehicleFragInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
		}

		if(m_attachEntity.GetEntity())
		{
			// Reset the "no collision entity" ptr on the container.
			m_attachEntity.GetEntity()->SetNoCollisionEntity(NULL);

			if(m_attachEntity.GetEntity()->GetCurrentPhysicsInst())
			{
				// Wake up the container if it is asleep; we don't want it floating in mid-air.
				phInst* pPhysInst = m_attachEntity.GetEntity()->GetCurrentPhysicsInst();
				CPhysics::GetSimulator()->ActivateObject(pPhysInst);

				pPhysInst->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
			}

			m_attachEntity.Invalidate();
		}

		m_bCanMoveFrameDown = true;

		m_attachState = UNATTACHED;
		break;

	default:
		DEBUG_HANDLER_FRAME_FSM("ERROR: UNKNOWN FSM STATE");
		Assertf(false, "Unknown attach state.");
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetHandlerFrame::ProcessPreComputeImpacts(CVehicle* pHandler, phCachedContactIterator& impacts)
{
	// Reset these values so that the forks don't get permanently stuck.
	m_bCanMoveFrameUp = true;

	// Joint 1 is the prismatic joint.
	JointInfo* pJointInfo = GetJoint(1);
	Assert(pJointInfo);
	Assert(pJointInfo->m_bPrismatic);

	// Only reset the flag which allows the player to move the frame down once they have stopped pushing down on the left-stick.
	if(!m_bCanMoveFrameDown && !m_bTryingToMoveFrameDown)
	{
		m_bCanMoveFrameDown = true;
		m_bReadyToAttach = false;
	}

	impacts.Reset();
	while(!impacts.AtEnd())
	{
		if(impacts.IsConstraint() || impacts.GetDepth() < 0.0f)
		{
			++impacts;
			continue;
		}

		Vector3 vecNormal;
		impacts.GetMyNormal(vecNormal);

		// Get the component index for the frame mechanism.
		int nBoneIndex = pHandler->GetBoneIndex(VEH_HANDLER_FRAME_2);
		fragInst* pHandlerInst = pHandler->GetVehicleFragInst();
		Assert(pHandlerInst);
		int nFrameComponent = pHandlerInst->GetComponentFromBoneIndex(nBoneIndex);

		// Check for an impact with the frame/boom.
		int nImpactComponentIndex = impacts.GetMyComponent();
		bool bImpactWithFrame = (nImpactComponentIndex == nFrameComponent);

#if DEBUG_DRAW
		// Visualise even the impacts which are being filtered out.
		if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
			grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.1f, Color_grey, false);
#endif // DEBUG_DRAW

		// Check for collisions between the frame / boom and the ground.
		bool bCollisionWithGround = bImpactWithFrame && (vecNormal.z > 0.7f) &&
			(DotProduct(VEC3V_TO_VECTOR3(pHandler->GetTransform().GetC()), vecNormal) > 0.7f) && (impacts.GetDepth() > 0.0f);

		// Test if there is any ground beneath the container.
		//bool bGroundUnderContainer = false;
		// Container could get deleted before the frame on cleaning up (failing a mission / dying, say).
		if(m_attachEntity.GetEntity())
		{
			Assert(dynamic_cast<CPhysical*>(m_attachEntity.GetEntity()));
			CCollisionHistoryIterator collisionIterator(static_cast<CPhysical*>(m_attachEntity.GetEntity())->GetFrameCollisionHistory(),
				/*bIterateBuildings=*/true, /*bIterateVehicles=*/true, /*bIteratePeds=*/false, /*bIterateObjects=*/true, /*bIterateOthers=*/false);
			while(const CCollisionRecord* pColRecord = collisionIterator.GetNext())
			{
				if(pColRecord->m_pRegdCollisionEntity.Get()!=pHandler && DotProduct(pColRecord->m_MyCollisionNormal, ZAXIS) > 0.9f)
				{
					//bGroundUnderContainer = true;
					bCollisionWithGround = true;
					break;
				}
			}
		}

		if(bImpactWithFrame && impacts.GetOtherInstance())
		{
#if DEBUG_DRAW
			if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
				grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.08f, Color_yellow, true);
#endif // DEBUG_DRAW

			// Check for collision between frame and the special container and do some tests to see if we will allow attachment.
			CEntity* pOtherEntity = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());
			if(pOtherEntity && pOtherEntity->GetArchetype() && pOtherEntity->GetModelIndex()==MI_HANDLER_CONTAINER)
			{
				// Is the script telling us we can attach now?
				if(m_bForceAttach)
				{
					m_attachEntity.SetEntity(pOtherEntity);
					m_bReadyToAttach = true;
				}
			}
		}

		// If the frame (or the container when attached) has hit the ground, stop the frame moving downwards.
		if(bCollisionWithGround)
		{
#if DEBUG_DRAW
			if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING))
				grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.08f, Color_green, true);
#endif // DEBUG_DRAW
			m_bCanMoveFrameDown = false;
			
			// B*1807140: Force the frame to move up a little bit if it, or the thing attached to it, has hit the ground or another object. 
			// This prevents cases where the thing attached to the frame is being forced into something below it by the player and the object underneath begins to act erratically.
			GetJoint(1)->m_PrismaticJointInfo.m_fDesiredPosition = GetJoint(1)->m_PrismaticJointInfo.m_fPosition + 0.01f;			

			m_fLockedDownwardMotionPos = Clamp(GetCurrentHeight(), pJointInfo->m_PrismaticJointInfo.m_fClosedPosition, 0.9f*pJointInfo->m_PrismaticJointInfo.m_fOpenPosition);
		}

		++impacts;
	} // Iteration over impacts.
}

//////////////////////////////////////////////////////////////////////////

bool CVehicleGadgetHandlerFrame::TestForGoodAttachmentOrientation(CVehicle* pHandler, CEntity* pContainer)
{
	// Don't allow attachment if the container's Z vector isn't sufficiently close to straight up / down w.r.t. the Handler.
	Matrix34 matHandler = MAT34V_TO_MATRIX34(pHandler->GetMatrix());
	Matrix34 matContainer = MAT34V_TO_MATRIX34(pContainer->GetTransform().GetMatrix());
	float fAngle = fabs(matHandler.c.FastAngle(matContainer.c));
	if(fAngle > 0.2f*PI && fAngle < 0.8*PI)
		return false;

	// The bones at each corner of the frame collision in the best order for a quick reject when we aren't
	// in a good position w.r.t. the container.
	eHierarchyId nCornerIDs[4] = { VEH_HANDLER_FRAME_PICKUP_2, VEH_HANDLER_FRAME_PICKUP_4, VEH_HANDLER_FRAME_PICKUP_1, VEH_HANDLER_FRAME_PICKUP_3 };

	// Offset from the bones a little bit to make for a better attach position.
	float fTransversOffsets[4] = { +ms_fTransverseOffset, -ms_fTransverseOffset, +ms_fTransverseOffset, -ms_fTransverseOffset };
	float fLongitudinalOffsets[4] = { +ms_fLongitudinalOffset, +ms_fLongitudinalOffset, -ms_fLongitudinalOffset, -ms_fLongitudinalOffset };

	// Issue sphere shape tests at the corners of the frame collision until we find one which isn't anywhere near the container.
	WorldProbe::CShapeTestSphereDesc sphereDesc;
	sphereDesc.SetIncludeEntity(pContainer);
	sphereDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	for(int i = 0; i < 4; ++i)
	{
		int nBoneIndex = pHandler->GetBoneIndex(nCornerIDs[i]);
		if(Verifyf(nBoneIndex > -1, "Couldn't find pickup bone (%d) on handler.", i))
		{
			Matrix34 matCornerBone;
			pHandler->GetGlobalMtx(nBoneIndex, matCornerBone);

			// Use the matrix of the bone for this corner to transform our offset from model space into world space.
			Vector3 vThisCorner(fTransversOffsets[i], fLongitudinalOffsets[i], 0.0f);
			matCornerBone.Transform(vThisCorner);

			sphereDesc.SetSphere(vThisCorner, ms_fTestSphereRadius);
			if(!WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
				return false;
		}
		else
		{
			// Something wrong with the skeleton setup? Bail without allowing creation of the constraint!
			return false;
		}
	}

	// If all those tests found a hit with the container, we must be ok to attach.
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetHandlerFrame::AttachContainerInstanceToFrame(CEntity* pContainer)
{
	if(AssertVerify(pContainer->GetModelIndex()==MI_HANDLER_CONTAINER))
	{
		if(pContainer->GetIsPhysical())
		{
			CPhysical *pContrainerPhysical = (CPhysical *)pContainer;
			if(pContrainerPhysical->GetIsAttached())
			{
				pContrainerPhysical->DetachFromParent(0);
			}
		}

		m_attachEntity.SetEntity(pContainer);

		// We assume the container has already been warped to the correct position by this point.
		m_attachState = READY_TO_ATTACH;

		// Set this to true so that we can decide to ignore the position of the frame to force a transition
		// to the "ATTACHING" state.
		m_bForceAttach = true;
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetHandlerFrame::AttachContainerInstanceToFrameWhenLinedUp(CEntity* pContainer)
{
	if(AssertVerify(pContainer->GetModelIndex()==MI_HANDLER_CONTAINER))
	{
		if(pContainer->GetIsPhysical())
		{
			CPhysical *pContrainerPhysical = (CPhysical *)pContainer;
			if(pContrainerPhysical->GetIsAttached())
			{
				pContrainerPhysical->DetachFromParent(0);
			}
		}
		m_attachEntity.SetEntity(pContainer);
		m_bReadyToAttach = true;
		m_attachState = READY_TO_ATTACH;
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetHandlerFrame::DetachContainerFromFrame()
{
	if(AssertVerify(m_attachEntity.GetEntity()))
	{
		// Force the FSM into this state which will take care of cleaning up the constraint and everything else.
		m_attachState = DETACHING;
	}
}

//////////////////////////////////////////////////////////////////////////

bool CVehicleGadgetHandlerFrame::IsAtLowestPosition()
{
	// Joint 1 is the prismatic joint.
	Assert(GetJoint(1)->m_bPrismatic);

	const float sf_ClosedPositionAllowance = 0.01f;
	if(GetJoint(1)->m_PrismaticJointInfo.m_fPosition <= GetJoint(1)->m_PrismaticJointInfo.m_fClosedPosition+sf_ClosedPositionAllowance)
		return true;

	return false;
}

void CVehicleGadgetHandlerFrame::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_POSITION = 8;
	static const unsigned SIZEOF_ACCELERATION = 8;
	static const float MAX_POSITION = 5.6f;

	float desiredPosition = 0.0f;
	float desiredAcceleration = 0.0f;

	if (GetJoint(1))
	{
		desiredPosition = GetJoint(1)->m_PrismaticJointInfo.m_fDesiredPosition;
		desiredAcceleration = GetJoint(1)->m_fDesiredAcceleration;
	}

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, desiredPosition, MAX_POSITION, SIZEOF_POSITION, "Desired Position");
	SERIALISE_PACKED_FLOAT(serialiser, desiredAcceleration, 1.0f, SIZEOF_ACCELERATION, "Desired acceleration");

	if (GetJoint(1))
	{
		GetJoint(1)->m_fDesiredAcceleration = desiredAcceleration;
		GetJoint(1)->m_PrismaticJointInfo.m_fDesiredPosition = desiredPosition;
	}

	CEntity* pAttachEntity = m_attachEntity.GetEntity();

	m_attachEntity.Serialise(serialiser, "Attach entity");

	if (pAttachEntity && !m_attachEntity.GetEntity())
	{
		pAttachEntity->SetNoCollisionEntity(NULL);

		phInst* pPhysInst = pAttachEntity->GetCurrentPhysicsInst();

		if (pPhysInst)
		{
			// Wake up the container if it is asleep; we don't want it floating in mid-air.
			CPhysics::GetSimulator()->ActivateObject(pPhysInst);
			pPhysInst->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetThresher
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetThresher::CVehicleGadgetThresher(CVehicle* , s32 iReelBoneId, s32 iAugerBoneId, float fRotationSpeed) :
CVehicleGadgetWithJointsBase( 2, fRotationSpeed, sfMuscleStiffness, sfMuscleAngleStrength, sfMuscleSpeedStrength, sfMinAndMaxMuscleTorqueGadgets ),
m_iReelBoneId(iReelBoneId),
m_iAugerBoneId(iAugerBoneId),
m_fRotationSpeed(fRotationSpeed),
m_ThresherAngle(0)
{
    Assert(m_iReelBoneId > -1);
    Assert(m_iAugerBoneId > -1);

    GetJoint(0)->m_iBone = iReelBoneId;
    GetJoint(1)->m_iBone = iAugerBoneId;

    //we only want to control the speed of the joint on the combine
    GetJoint(0)->m_1dofJointInfo.m_bOnlyControlSpeed = true;
    GetJoint(1)->m_1dofJointInfo.m_bOnlyControlSpeed = true;
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetThresher::ProcessPhysics(CVehicle* pVehicle, const float fTimeStep, const int nTimeSlice)
{	
    if(pVehicle->IsEngineOn())
    {
        float fSpeed = DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));

        // update the thresher speed
        static dev_float sfMinimumRotationSpeed = 3.0f;
        if(fSpeed < sfMinimumRotationSpeed)//only rotate if we are going forwards
        {
            fSpeed = sfMinimumRotationSpeed;
        }
       
        float fThresherSpeed = fSpeed * m_fRotationSpeed;

        GetJoint(0)->m_fDesiredAcceleration = -fThresherSpeed;
        GetJoint(1)->m_fDesiredAcceleration = fThresherSpeed;
    }
    else
    {
        GetJoint(0)->m_fDesiredAcceleration = 0.0f;
        GetJoint(1)->m_fDesiredAcceleration = 0.0f;
    }

    CVehicleGadgetWithJointsBase::ProcessPhysics(pVehicle, fTimeStep, nTimeSlice);
}



//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetBombBay
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetBombBay::CVehicleGadgetBombBay( s32 iDoorOneBoneId, s32 iDoorTwoBoneId, float fSpeed) :
CVehicleGadgetWithJointsBase( 2, fSpeed, sfMuscleStiffness, sfMuscleAngleStrength, sfMuscleSpeedStrength, sfMinAndMaxMuscleTorqueGadgets ),
m_iDoorOneBoneId(iDoorOneBoneId),
m_iDoorTwoBoneId(iDoorTwoBoneId),
m_fSpeed(fSpeed),
m_bOpen(false),
m_bShouldBeOpen(false)
{
    Assert(m_iDoorOneBoneId > -1);
    Assert(m_iDoorTwoBoneId > -1);

    GetJoint(0)->m_iBone = iDoorOneBoneId;
    GetJoint(1)->m_iBone = iDoorTwoBoneId;
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBombBay::ProcessPhysics(CVehicle* pVehicle, const float fTimeStep, const int nTimeSlice)
{	
	if(m_bShouldBeOpen && !m_bOpen)
	{
		OpenDoors(pVehicle);
	}
	else if(!m_bShouldBeOpen && m_bOpen)
	{
		CloseDoors(pVehicle);
	}

    CVehicleGadgetWithJointsBase::ProcessPhysics(pVehicle, fTimeStep, nTimeSlice);
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBombBay::OpenDoors(CVehicle* pVehicle)
{
	m_bShouldBeOpen = m_bOpen = true;

    fragInstGta* pFragInst = pVehicle->GetVehicleFragInst();
    Assert(pFragInst && pFragInst->GetCacheEntry());

    pFragInst->GetCacheEntry()->LazyArticulatedInit();

    GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = GetJoint(0)->m_1dofJointInfo.m_fClosedAngle;
    GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = GetJoint(1)->m_1dofJointInfo.m_fOpenAngle;

	pVehicle->GetVehicleAudioEntity()->SetBombBayDoorsOpen(true);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBombBay::CloseDoors(CVehicle* pVehicle)
{
	m_bShouldBeOpen = m_bOpen = false;

    fragInstGta* pFragInst = pVehicle->GetVehicleFragInst();
    Assert(pFragInst && pFragInst->GetCacheEntry());

    pFragInst->GetCacheEntry()->LazyArticulatedInit();

    GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = GetJoint(0)->m_1dofJointInfo.m_fOpenAngle;
    GetJoint(1)->m_1dofJointInfo.m_fDesiredAngle = GetJoint(1)->m_1dofJointInfo.m_fClosedAngle;

	pVehicle->GetVehicleAudioEntity()->SetBombBayDoorsOpen(false);
}

void CVehicleGadgetBombBay::Serialise(CSyncDataBase& serialiser)
{
	SERIALISE_BOOL(serialiser, m_bShouldBeOpen, "Open Door");
}

//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetBoatBoom
//////////////////////////////////////////////////////////////////////////

static dev_float sfBoatMuscleStiffness = 0.995f;
static dev_float sfBoatMuscleAngleStrength = 16.0f;
static dev_float sfBoatMuscleSpeedStrength = 21.0f; 
static dev_float sfBoatMinAndMaxMuscleTorqueGadgets = 35.0f;

CVehicleGadgetBoatBoom::CVehicleGadgetBoatBoom( s32 iBoomOneBoneId, float fSpeed) :
CVehicleGadgetWithJointsBase( 1, fSpeed, sfBoatMuscleStiffness, sfBoatMuscleAngleStrength, sfBoatMuscleSpeedStrength, sfBoatMinAndMaxMuscleTorqueGadgets ),
m_iBoomBoneId(iBoomOneBoneId),
m_fRotationSpeed(fSpeed)
{
    Assert(m_iBoomBoneId > -1);

    GetJoint(0)->m_iBone = iBoomOneBoneId;
    GetJoint(0)->m_bSwingFreely = true;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBoatBoom::ProcessPhysics(CVehicle* pVehicle, const float fTimeStep, const int nTimeSlice)
{	
    CVehicleGadgetWithJointsBase::ProcessPhysics(pVehicle, fTimeStep, nTimeSlice);
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBoatBoom::SwingOutBoatBoom(CVehicle* pVehicle)
{
    fragInstGta* pFragInst = pVehicle->GetVehicleFragInst();
    Assert(pFragInst && pFragInst->GetCacheEntry());

    pFragInst->GetCacheEntry()->LazyArticulatedInit();

    SetSwingFreely(pVehicle, false);

    GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = GetJoint(0)->m_1dofJointInfo.m_fClosedAngle;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBoatBoom::SwingInBoatBoom(CVehicle* pVehicle)
{
    fragInstGta* pFragInst = pVehicle->GetVehicleFragInst();
    Assert(pFragInst && pFragInst->GetCacheEntry());

    pFragInst->GetCacheEntry()->LazyArticulatedInit();

    SetSwingFreely(pVehicle, false);

    GetJoint(0)->m_1dofJointInfo.m_fDesiredAngle = GetJoint(0)->m_1dofJointInfo.m_fOpenAngle;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBoatBoom::SwingToRatio(CVehicle* pVehicle, float fTargetRatio)
{
    SetSwingFreely(pVehicle, false);
    DriveJointToRatio(pVehicle, 0, fTargetRatio);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBoatBoom::SetSwingFreely(CVehicle* UNUSED_PARAM(pVehicle), bool bSwingFreely)
{
    GetJoint(0)->m_bSwingFreely = bSwingFreely;
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetBoatBoom::SetAllowBoomToAnimate(CVehicle* pVehicle, bool bAnimateBoom)
{
    if(bAnimateBoom)
    {
        pVehicle->m_nVehicleFlags.bAnimateJoints = true;
    }
    else
    {
        pVehicle->m_nVehicleFlags.bAnimateJoints = false;
    }

}

//////////////////////////////////////////////////////////////////////////

float CVehicleGadgetBoatBoom::GetPositionRatio()
{
    float maxAngleDisplacement = GetJoint(0)->m_1dofJointInfo.m_fOpenAngle - GetJoint(0)->m_1dofJointInfo.m_fClosedAngle;
    float positionRatio = (GetJoint(0)->m_1dofJointInfo.m_fAngle - GetJoint(0)->m_1dofJointInfo.m_fClosedAngle)/maxAngleDisplacement;

    return positionRatio;
}

static dev_float sfPickUpDefaultRopeLength = 2.5f;
static dev_float sfPickUpRopeMinLength = 1.9f;
static dev_float sfPickUpRopeMaxLength = 100.0f;
// static dev_float sfPickUpRopeAdjustableLength = sfPickUpDefaultRopeLength - sfPickUpRopeMinLength;
static dev_s32	 siPickUpRopeType = 1;
static dev_float sfPickUpRopeAllowedConstraintPenetration = 0.05f;

//////////////////////////////////////////////////////////////////////////
// Class CVehicleGadgetPickUpRope
//////////////////////////////////////////////////////////////////////////
CVehicleGadgetPickUpRope::CVehicleGadgetPickUpRope(s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex) :
CVehicleGadget(),
	m_iTowMountAChild(0),
	m_iTowMountA(iTowMountABoneIndex),
	m_iTowMountBChild(0),
	m_iTowMountB(iTowMountBBoneIndex),
	m_PropObject(NULL),
	m_pTowedEntity(NULL),
	m_Setup(false),
	m_bHoldsDrawableRef(false),
	m_bRopesAndHookInitialized(false),
	m_uIgnoreAttachTimer(0),
	m_VehicleTowArmAudio(NULL),
	m_RopeInstanceA(NULL),
	m_RopeInstanceB(NULL),
	m_RopesAttachedVehiclePhysicsInst(NULL),
	m_bDetachVehicle(false),
	m_bForceDontDetachVehicle(false),
	m_bEnableFreezeWaitingOnCollision(false),
	m_vTowedVehicleOriginalAngDamp(0.0f, 0.0f, 0.0f),
	m_vTowedVehicleOriginalAngDampC(0.0f, 0.0f, 0.0f),
	m_fPrevJointPosition(0.0f),
	m_fRopeLength(sfPickUpRopeMinLength),
	m_bAdjustingRopeLength(true),
	m_fPreviousDesiredRopeLength(sfPickUpRopeMinLength),
	m_fDetachedRopeLength(sfPickUpDefaultRopeLength),
	m_fAttachedRopeLength(sfPickUpRopeMinLength),
	m_fPickUpRopeLengthChangeRate(3.0f),
	m_bEnableDamping(true),
	m_fDampingMultiplier(1.0f),
	m_bEnsurePickupEntityUpright(false),
	m_vLastFramesDistance( 0.0f, 0.0f, 0.0f ),
	m_vDesiredPickupPropPosition( 0.0f, 0.0f, 0.0f ),
	m_vStartPickupPropPosition( 0.0f, 0.0f, 0.0f ),
	m_vCurrentPickupPropPosition( 0.0f, 0.0f, 0.0f ),
	m_fPickupPositionTransitionTime(0.0f),
	m_fExtraBoundAttachAllowance(0.0f),
	m_fForceMultToApplyInConstraint(1.0f)
{
	Assert(iTowMountABoneIndex > -1);
	Assert(iTowMountBBoneIndex > -1);
}

//////////////////////////////////////////////////////////////////////////

CVehicleGadgetPickUpRope::~CVehicleGadgetPickUpRope()
{
	DeleteRopesPropAndConstraints();

	if (m_PropObject)
	{
		CGameWorld::Remove(m_PropObject);
		CObjectPopulation::DestroyObject(m_PropObject);
		m_PropObject = NULL;
	}
}

void CVehicleGadgetPickUpRope::DeleteRopesPropAndConstraints(CVehicle* pParent)
{
	if( pParent )
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::DeleteRopesPropAndConstraints - Detaching entity because cleaning up prop and ropes. Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
	}
	DetachEntity(pParent, false);
	RemoveRopesAndProp();
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::RemoveRopesAndProp()
{
	//vehicleDisplayf(":RemoveRopesAndHook, %p (%p, %p, %p)",this,m_RopeInstanceA,m_RopeInstanceB,m_PropObject.Get());

	if(m_RopeInstanceA)
	{
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(m_RopeInstanceA->GetUniqueID());
			CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(m_RopeInstanceA->GetUniqueID());
			CReplayMgr::RecordPersistantFx<CPacketDeleteRope>( CPacketDeleteRope(m_RopeInstanceA->GetUniqueID()),	CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceA), pAttachedA, pAttachedB, false);
		}
#endif
		CPhysics::GetRopeManager()->RemoveRope(m_RopeInstanceA);
		m_RopeInstanceA = NULL;
	}

	if(m_RopeInstanceB)
	{
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(m_RopeInstanceB->GetUniqueID());
			CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(m_RopeInstanceB->GetUniqueID());
			CReplayMgr::RecordPersistantFx<CPacketDeleteRope>( CPacketDeleteRope(m_RopeInstanceB->GetUniqueID()),	CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceB), pAttachedA, pAttachedB, false);
		}
#endif
		CPhysics::GetRopeManager()->RemoveRope(m_RopeInstanceB);
		m_RopeInstanceB = NULL;
	}

	if (m_PropObject)
	{
		if(m_bHoldsDrawableRef)
		{
			m_PropObject->GetBaseModelInfo()->ReleaseDrawable();
			m_bHoldsDrawableRef = false;
		}
		CGameWorld::Remove(m_PropObject);
	}

	m_bRopesAndHookInitialized = false;
	m_Setup = false;
}

dev_float dfClampedMassRatio = 0.5f;
dev_float dfCargoContainerAttachedMass = 1500.0f;
dev_float dfSubmarineAttachedMass = 1500.0f;

void CVehicleGadgetPickUpRope::ComputeAndSetInverseMassScale(CVehicle *pParentVehicle)
{
	float fInvMassScale = 1.0f;
	if(m_pTowedEntity && m_pTowedEntity->GetCurrentPhysicsInst())
	{
		phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(m_pTowedEntity->GetCurrentPhysicsInst()->GetArchetype());
		float fOriginalMass = pArchetypeDamp->GetMass();
		float fDesiredNewMass = fOriginalMass;

		// Clamp the attached entity mass
		if(m_pTowedEntity->GetArchetype() && m_pTowedEntity->GetModelIndex()==MI_HANDLER_CONTAINER)
		{
			fDesiredNewMass = dfCargoContainerAttachedMass;
		}
		else if(GetAttachedVehicle() && GetAttachedVehicle()->GetVehicleType() == VEHICLE_TYPE_SUBMARINE)
		{
			fDesiredNewMass = dfSubmarineAttachedMass;
		}
		else if(m_pTowedEntity->GetMass() > pParentVehicle->GetMass() * dfClampedMassRatio)
		{
			fDesiredNewMass = pParentVehicle->GetMass() * dfClampedMassRatio;
		}

		fInvMassScale = fOriginalMass / fDesiredNewMass ;
	}

	if(m_RopeInstanceA && m_RopeInstanceA->GetConstraint())
	{
		phConstraintDistance* pRopePosConstraint = static_cast<phConstraintDistance*>( m_RopeInstanceA->GetConstraint() );
		pRopePosConstraint->SetMassInvScaleB(fInvMassScale);
	}

	if(m_RopeInstanceB && m_RopeInstanceB->GetConstraint())
	{
		phConstraintDistance* pRopePosConstraint = static_cast<phConstraintDistance*>( m_RopeInstanceB->GetConstraint() );
		pRopePosConstraint->SetMassInvScaleB(fInvMassScale);
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::Init(CVehicle* pParent)
{
	ChangeLod(*pParent,VDM_REAL);
	m_VehicleTowArmAudio = (audVehicleGrapplingHook*)pParent->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_GRAPPLING_HOOK);
	audAssert(m_VehicleTowArmAudio);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::AddRopesAndProp(CVehicle & parentVehicle)
{
	//vehicleDisplayf(":AddRopesAndHook, %p (%p, %p, %p) %d",this,m_RopeInstanceA,m_RopeInstanceB,m_PropObject.Get(),(int)m_bRopesAndHookInitialized);

	if(!parentVehicle.GetFragInst() || !PHLEVEL->IsInLevel(parentVehicle.GetFragInst()->GetLevelIndex()))
	{
		// Must be in the physics level to add ropes
		return;
	}

	if(m_bRopesAndHookInitialized REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
	{
		return;
	}

	Vec3V rot(V_ZERO);
	Matrix34 matMountA, matMountB, matHook;

	//get the position of the bones at the end of the tow truck arm.
	parentVehicle.GetGlobalMtx(m_iTowMountA, matMountA);
	parentVehicle.GetGlobalMtx(m_iTowMountB, matMountB);


	ropeManager* pRopeManager = CPhysics::GetRopeManager();

	TUNE_INT(TOW_TRUCK_ROPE_SECTIONS, 4, 1, 10, 1);
	m_RopeInstanceA = pRopeManager->AddRope( VECTOR3_TO_VEC3V(matMountA.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true );
	m_RopeInstanceB = pRopeManager->AddRope( VECTOR3_TO_VEC3V(matMountB.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true );

	Assert( m_RopeInstanceA );
	Assert( m_RopeInstanceB );
	if( m_RopeInstanceA )
	{
		m_RopeInstanceA->SetNewUniqueId();
		m_RopeInstanceA->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
		m_RopeInstanceA->SetSpecial(true);
		m_RopeInstanceA->SetIsTense(true);
#if GTA_REPLAY
			CReplayMgr::RecordPersistantFx<CPacketAddRope>(
				CPacketAddRope(m_RopeInstanceA->GetUniqueID(), VECTOR3_TO_VEC3V(matMountA.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceA),
				NULL, 
				true);
#endif
	}
	if( m_RopeInstanceB )
	{
		m_RopeInstanceB->SetNewUniqueId();
		m_RopeInstanceB->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
		m_RopeInstanceB->SetSpecial(true);
		m_RopeInstanceB->SetIsTense(true);
#if GTA_REPLAY
			CReplayMgr::RecordPersistantFx<CPacketAddRope>(
				CPacketAddRope(m_RopeInstanceB->GetUniqueID(), VECTOR3_TO_VEC3V(matMountB.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceB),
				NULL, 
				true);
#endif
	}

	strLocalIndex nModelIndex = strLocalIndex(MI_PROP_HOOK);
	fwModelId hookModelId(nModelIndex);
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(hookModelId);
	if(pModelInfo==NULL)
		return;

	if(m_PropObject == NULL)
	{
		m_PropObject = CObjectPopulation::CreateObject(hookModelId, ENTITY_OWNEDBY_GAME, true, true, false);		
	}

	matHook = matMountA;
	matHook.d = (matMountA.d + matMountB.d) * 0.5f;

	if(m_PropObject)
	{
		matHook.d -= VEC3V_TO_VECTOR3(parentVehicle.GetMatrix().GetCol1()) * (m_PropObject->GetBoundRadius() * 2.0f);
		m_PropObject->SetMatrix(matHook, true, true, true);
		CGameWorld::Add(m_PropObject, CGameWorld::OUTSIDE );
		m_PropObject->SetNoCollision(&parentVehicle, NO_COLLISION_RESET_WHEN_NO_BBOX);

		// Broken object created so tell the replay it should record it
		REPLAY_ONLY(CReplayMgr::RecordObject(m_PropObject));
	}

#if __ASSERT
	strLocalIndex txdSlot = g_TxdStore.FindSlot(ropeDataManager::GetRopeTxdName());
	if (txdSlot != -1)
	{
		Assertf(CStreaming::HasObjectLoaded(txdSlot, g_TxdStore.GetStreamingModuleId()), "Rope txd not streamed in for vehicle %s", pModelInfo->GetModelName());
	}
#endif // __ASSERT
	
	m_bRopesAndHookInitialized = true;
}

//////////////////////////////////////////////////////////////////////////

int CVehicleGadgetPickUpRope::GetGadgetEntities(int entityListSize, const CEntity ** entityListOut) const
{
	if(entityListSize>0 && m_PropObject)
	{
		entityListOut[0] = m_PropObject;
		return 1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::WarpToPosition(CVehicle* pParentVehicle, bool bUpdateConstraints)
{
	// Move the attached entity (if there is one) a position closer to the parent vehicle.
	if(m_pTowedEntity)
	{
		Matrix34 matEnt;
		Matrix34 matMountA, matMountB;

		//get the position of the bones at the end of the tow truck arm.
		pParentVehicle->GetGlobalMtx(m_iTowMountA, matMountA);
		pParentVehicle->GetGlobalMtx(m_iTowMountB, matMountB);

		matEnt = matMountA;
		matEnt.d = (matMountA.d + matMountB.d) * 0.5f;
		if(m_pTowedEntity)
		{
			matEnt.d.z -= m_pTowedEntity->GetBoundRadius() * 2.0f;
		}

		if(m_pTowedEntity->GetIsAttached())
		{
			m_pTowedEntity->GetAttachmentExtension()->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true);
		}

		m_pTowedEntity->SetMatrix(matEnt, true, true, true);

		if(m_pTowedEntity->GetIsAttached())
		{
			m_pTowedEntity->GetAttachmentExtension()->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
		}
	}


	// Move the pick-up prop into position relative to the attached entity (if there is one).
	if(m_PropObject && 
		( !m_PropObject->IsNetworkClone() ||
		  !m_Setup ) )
	{
		Matrix34 matProp;
		Matrix34 matMountA, matMountB;

		// Position the pick-up prop using the mount points.
		pParentVehicle->GetGlobalMtx(m_iTowMountA, matMountA);
		pParentVehicle->GetGlobalMtx(m_iTowMountB, matMountB);

		matProp = matMountA;
		matProp.d = (matMountA.d + matMountB.d) * 0.5f;

		if(!m_pTowedEntity)
		{
			matProp.d -= VEC3V_TO_VECTOR3(pParentVehicle->GetMatrix().GetCol2()) * Min(m_PropObject->GetBoundRadius() * 2.0f, m_fRopeLength - 0.1f);
		}
		else
		{
			matProp.d += m_vCurrentPickupPropPosition;
		}

		if(m_PropObject->GetIsAttached())
		{
			m_PropObject->GetAttachmentExtension()->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true);
		}

		m_PropObject->SetMatrix(matProp, true, true, true);

		if(m_PropObject->GetIsAttached())
		{
			m_PropObject->GetAttachmentExtension()->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
		}		
	}
	
	if(m_Setup)
	{
		if(bUpdateConstraints)
		{
			UpdateAttachments(pParentVehicle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::Teleport(CVehicle *pParentVehicle, const Matrix34 &matOld)
{
	if(m_pTowedEntity)
	{
		Matrix34 matAttachedVehicle = MAT34V_TO_MATRIX34(GetAttachedEntity()->GetMatrix());
		Matrix34 matOldTowTruckInverse = matOld;
		matOldTowTruckInverse.Inverse();
		matAttachedVehicle.Dot(matOldTowTruckInverse);
		matAttachedVehicle.Dot(MAT34V_TO_MATRIX34(pParentVehicle->GetMatrix()));
		CPhysical *pPhyTowedPhysical = m_pTowedEntity;
		if(pPhyTowedPhysical->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = (CVehicle *)pPhyTowedPhysical;
			bool bAttachedChildIsParent = false;
			CVehicle *parent = pParentVehicle;
			while(parent)
			{
				if(pVehicle == parent)
				{
					bAttachedChildIsParent = true;
					break;
				}
				parent = parent->GetAttachParentVehicle();
			}

			if(bAttachedChildIsParent)
			{
				DetachEntity(pVehicle, true, true);
			}
			else
			{
				Matrix34 matOld = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
				pVehicle->TeleportWithoutUpdateGadgets(matAttachedVehicle.d, camFrame::ComputeHeadingFromMatrix(matAttachedVehicle), true, true);
				pVehicle->UpdateGadgetsAfterTeleport(matOld, false); // do not detach from gadget
			}
		}
		else
		{
			pPhyTowedPhysical->Teleport(matAttachedVehicle.d, camFrame::ComputeHeadingFromMatrix(matAttachedVehicle), false, true, false, true, false, true);
		}
		UpdateAttachments(pParentVehicle);
	}
	else
	{
		WarpToPosition(pParentVehicle);
	}
}

//////////////////////////////////////////////////////////////////////////
dev_float dfPickUpRopeLengthWindingThreshold = 0.1f;
void CVehicleGadgetPickUpRope::SetRopeLength(float fDetachedRopeLength, float fAttachedRopeLength, bool fSetRopeLengthInstantly)
{
	m_fDetachedRopeLength = Clamp(fDetachedRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength);
	m_fAttachedRopeLength = Clamp(fAttachedRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength);

	if(fSetRopeLengthInstantly)
	{
		m_bAdjustingRopeLength = true;

		if(m_bAdjustingRopeLength && m_RopeInstanceA && m_RopeInstanceB)
		{
			float fDesiredRopeLength = m_pTowedEntity ? m_fAttachedRopeLength : m_fDetachedRopeLength;

			float fRopeLength = m_RopeInstanceA->GetLength();
			if(fabs(fRopeLength - fDesiredRopeLength) > dfPickUpRopeLengthWindingThreshold)
			{	
				if(fRopeLength < fDesiredRopeLength)
				{
					m_RopeInstanceA->ForceLength(fDesiredRopeLength);
					m_RopeInstanceB->ForceLength(fDesiredRopeLength);

					m_RopeInstanceA->StopWindingFront();
					m_RopeInstanceB->StopWindingFront();		 

					m_RopeInstanceA->StartUnwindingFront(); // start unwinding has to be called when ever force length is called			
					m_RopeInstanceB->StartUnwindingFront();
				}
				else if(fRopeLength > fDesiredRopeLength)
				{
					m_RopeInstanceA->ForceLength(fDesiredRopeLength);
					m_RopeInstanceB->ForceLength(fDesiredRopeLength);

					m_RopeInstanceA->StopUnwindingFront();
					m_RopeInstanceB->StopUnwindingFront();

					m_RopeInstanceA->StartWindingFront(); // start unwinding has to be called when ever force length is called
					m_RopeInstanceB->StartWindingFront();
				}
			}
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			ropeInstance* r = m_RopeInstanceA;
			CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
				CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r), NULL, false);

			r = m_RopeInstanceB;
			CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
				CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r), NULL, false);
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::Serialise(CSyncDataBase& serialiser)
{
	static const float		  MAX_SERIALIZED_ROPE_LENGTH = 100.f;
	static const unsigned int SIZEOF_ROPE_LENGTH    = 8;

	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fDetachedRopeLength, MAX_SERIALIZED_ROPE_LENGTH, SIZEOF_ROPE_LENGTH, "Rope Detach Length");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fAttachedRopeLength, MAX_SERIALIZED_ROPE_LENGTH, SIZEOF_ROPE_LENGTH, "Rope Attach Length");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fRopeLength, MAX_SERIALIZED_ROPE_LENGTH, SIZEOF_ROPE_LENGTH,"Rope Length");

	m_excludeEntity.Serialise(serialiser);
}	

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::BlowUpCarParts(CVehicle* pParent)
{
	//If the tow truck is being destroyed disconnect and delete the hook and ropes.
	DeleteRopesPropAndConstraints(pParent);
}

//////////////////////////////////////////////////////////////////////////

bool CVehicleGadgetPickUpRope::WantsToBeAwake(CVehicle * /*pVehicle*/)
{
	if(!m_bRopesAndHookInitialized)
	{
		return false;
	}
	return true;//always awake as we get a crash on the second creation of tow truck otherwise
}


//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::ChangeLod(CVehicle & parentVehicle, int lod)
{
	if(lod==VDM_REAL || lod==VDM_DUMMY)
	{
		if(!m_bRopesAndHookInitialized)
		{
			AddRopesAndProp(parentVehicle);
		}
	}
	else // VDM_SUPERDUMMY
	{
		if(!m_pTowedEntity && m_bRopesAndHookInitialized)
		{
			RemoveRopesAndProp();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::DetachEntity(CVehicle *pParentVehicle, bool bReattachHook, bool bImmediateDetach )
{ 
	if(!m_pTowedEntity)
		return;

	if( m_bForceDontDetachVehicle &&
		m_pTowedEntity &&
		m_pTowedEntity->GetIsTypeVehicle() )
	{
		return;
	}

	// do the detachment now if the tow truck is fixed (ProcessPhysics will not be called)
	if (pParentVehicle && (pParentVehicle->GetIsAnyFixedFlagSet() || bImmediateDetach))
	{
		ActuallyDetachVehicle(pParentVehicle, bReattachHook);
		m_bDetachVehicle = false;
	}
	else
	{

#if __BANK
	if( ms_logCallStack )
	{		
		size_t stackTrace[ 32 ];
		sysStack::CaptureStackTrace( stackTrace, 32 );
		sysStack::PrintCapturedStackTrace( stackTrace, 32 );
	}

	if( pParentVehicle )
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::DetachEntity - Detaching entity. Parent vehicle: %s", pParentVehicle->GetNetworkObject() ? pParentVehicle->GetNetworkObject()->GetLogName() : pParentVehicle->GetModelName() );
	}
	else
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::DetachEntity - Detaching entity." );
		sysStack::PrintStackTrace();
	}
#endif

		m_bDetachVehicle = true;
	}

	
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::ActuallyDetachVehicle( CVehicle *pParentVehicle, bool bReattachHook )
{   

	if( m_RopeInstanceA )
	{
		m_RopeInstanceA->SetIsTense(true);
		phVerletCloth* pClothA = m_RopeInstanceA->GetEnvCloth()->GetCloth();
		Assert( pClothA );
		pClothA->DetachVirtualBound();

	}
	if( m_RopeInstanceB )
	{
		m_RopeInstanceB->SetIsTense(true);
		phVerletCloth* pClothB = m_RopeInstanceB->GetEnvCloth()->GetCloth();
		Assert( pClothB );
		pClothB->DetachVirtualBound();
	}	

	//set the vehicle as detached 
	if(m_pTowedEntity)
	{
		CPhysical *pPhyTowedVehicle = m_pTowedEntity;
		CVehicle *pTowedVeh = NULL;
		if(pPhyTowedVehicle->GetIsTypeVehicle())
		{
			pTowedVeh = static_cast<CVehicle*>(pPhyTowedVehicle);
		}

		if(m_pTowedEntity->GetAttachParent() == pParentVehicle) // The towed entity might have been detached, if it is actively detaching itself (could happen to the trailer)
		{
			m_pTowedEntity->DetachFromParent(0);
		}

		//disable extra iterations
		if(pParentVehicle->GetCurrentPhysicsInst())
		{
			pParentVehicle->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
		}

		if(pTowedVeh)
		{
			if(pTowedVeh->GetCurrentPhysicsInst())
			{
				pTowedVeh->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, false);
			}

			//if there's no driver then just set the no driver task as abandoned
			//set the no driver task, to set the control inputs
			if(!pTowedVeh->GetDriver() && !pTowedVeh->IsNetworkClone())
			{
				pTowedVeh->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, 
					rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_ABANDONED), 
					VEHICLE_TASK_PRIORITY_PRIMARY, 
					false);
			}

			if(pTowedVeh->GetVehicleAudioEntity())
			{
				pTowedVeh->GetVehicleAudioEntity()->TriggerTowingHookDetachSound();
			}
				
			pTowedVeh->m_nVehicleFlags.bIsUnderMagnetControl = false;			
		}

		if(m_pTowedEntity->GetCurrentPhysicsInst())
		{
			phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(m_pTowedEntity->GetCurrentPhysicsInst()->GetArchetype());
			pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, m_vTowedVehicleOriginalAngDamp);
			pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_C, m_vTowedVehicleOriginalAngDampC);
		}

		if( m_bEnableFreezeWaitingOnCollision )
		{
			m_pTowedEntity->SetAllowFreezeWaitingOnCollision(true);
		}
		
		ComputeAndSetInverseMassScale(pParentVehicle);

		if ( NetworkInterface::IsGameInProgress() && m_pTowedEntity && m_pTowedEntity->GetIsPhysical())
		{
			netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(m_pTowedEntity);

			if (pNetObj)
			{
				static_cast<CNetObjPhysical*>(pNetObj)->OnDetachedFromPickUpHook();
			}
		}

		m_pTowedEntity = NULL;
	}

	m_uIgnoreAttachTimer = GetAttachResetTime();;	

	if(bReattachHook)
	{
		//reattach the ropes to the hook, this also detaches the ropes from the vehicle
		AttachRopesToObject( pParentVehicle, m_PropObject );
	}
}

//////////////////////////////////////////////////////////////////////////
const CVehicle* CVehicleGadgetPickUpRope::GetAttachedVehicle() const
{
	CPhysical *pPhyTowedVehicle = m_pTowedEntity;
	
	if(pPhyTowedVehicle && pPhyTowedVehicle->GetIsTypeVehicle())
	{
		return static_cast<CVehicle*>(pPhyTowedVehicle);
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
const CPhysical* CVehicleGadgetPickUpRope::GetAttachedEntity() const
{
	CPhysical *pPhyTowedVehicle = m_pTowedEntity;
	return pPhyTowedVehicle;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleGadgetPickUpRope::AttachEntityToPickUpProp( CVehicle *pPickUpVehicle, CPhysical *pEntityToAttach, const Vector3 &vAttachPosition, s32 iComponentIndex, bool bNetAssert )
{
	//convert the attach position into an offset position in the vehicle we want to connects to space.
	Vector3 vLocalAttachPosition(vAttachPosition);
	Matrix34 matVehicle = MAT34V_TO_MATRIX34(pEntityToAttach->GetMatrix());
	matVehicle.Inverse();
	matVehicle.Transform(vLocalAttachPosition);

	AttachEntityToPickUpProp( pPickUpVehicle, pEntityToAttach, -1, vLocalAttachPosition, iComponentIndex, bNetAssert );
}


#if RECORDING_VERTS
namespace rage
{
	extern dbgRecords<recordLine> g_DbgRecordLines;
	extern dbgRecords<recordSphere> g_DbgRecordSpheres;
	extern dbgRecords<recordTriangle> g_DbgRecordTriangles;
	extern dbgRecords<recordCapsule> g_DbgRecordCapsules;
	extern bool g_EnableRecording;
}
using namespace rage;
#endif

//////////////////////////////////////////////////////////////////////////
//Passing in a valid bone index and valid component index for different parts of the vehicle may create undesireable results
dev_float dfHookOffsetZ = 0.25f;
dev_float dfHookOffsetZForRoosevelt = 0.15f;
dev_float dfHookOffsetZForRhino = -0.25f;
dev_float dfHookOffsetZForPhantom = -0.725f;
dev_float dfHookOffsetZForHaulerWithoutExtra1 = -1.0f;
dev_float dfHookOffsetZForHauler = -0.5f;
dev_float dfHookOffsetYForHauler = 1.35f;
dev_float dfHookOffsetZForPacker = -0.425f;
dev_float dfHookOffsetZForBrickade = -0.9f;
dev_float dfHookOffsetZForOmnis = -0.45f;
dev_float dfHookOffsetZForWastelander = 0.04f;
dev_float dfHookOffsetYForWastelander = 1.86f;
dev_float dfHookOffsetZForRuston = 0.071f;
dev_float dfHookOffsetYForRuston = -0.675f;
dev_float dfHookOffsetXForRuston = 0.32f;
dev_float dfHookOffsetXForInfernus2 = -0.05f;
dev_float dfHookOffsetZForHalftrack = -0.95f;
dev_float dfHookOffsetYForHalftrack = 0.3f;
dev_float dfHookOffsetZForTampa3 = -0.55f;
dev_float dfHookOffsetYForTampa3 = 0.2f;
dev_float dfHookOffsetZForAPC = -0.47f;
dev_float dfHookOffsetZForTechnical3 = -0.8f;
dev_float dfHookOffsetYForTrailerSmall2 = -0.45f;
dev_float dfHookOffsetZForTrailerSmall2 = -0.3f;
dev_float dfHookOffsetZForOppressor = 0.5;
dev_float dfHookOffsetZForKhanjali = -0.55f;
dev_float dfHookOffsetZForBarrage = -0.55f;
dev_float dfHookOffsetYForBarrage = -0.55f;
dev_float dfHookOffsetZForChernobog = -0.5f;
dev_float dfHookOffsetYForChernobog = 2.25f;
dev_float dfHookOffsetZForCaracara = -0.4f;
dev_float dfHookOffsetZForSwinger = 0.1f;
dev_float dfHookOffsetYForSwinger = -0.375f;
dev_float dfHookOffsetZForPounder2 = -0.875f;
dev_float dfHookOffsetYForLocust = -0.61f;
dev_float dfHookOffsetZForLocust = 0.05f;
dev_float dfHookOffsetYForZorrusso = -0.59f;
dev_float dfHookOffsetZForZorrusso = -0.115f;
dev_float dfHookOffsetYForS80 = -0.20f;
dev_float dfHookOffsetZForS80 = 0.075f;
dev_float dfHookOffsetYForDynasty = 0.55f;
dev_float dfHookOffsetZForDynasty = -0.1f;
dev_float dfHookOffsetYForNeo = -0.64f;
dev_float dfHookOffsetZForNeo = 0.03f;
dev_float dfHookOffsetYForNovak = 0.21f;
dev_float dfHookOffsetZForNovak = -0.01f;
dev_float dfHookOffsetYForPeyote2 = -0.045f;
dev_float dfHookOffsetZForPeyote2 = 0.05f;
dev_float dfHookOffsetYForZion3 = 0.15f;
dev_float dfHookOffsetZForZion3 = 0.065f;
dev_float dfHookOffsetYForPatrolboat = -0.0f;
dev_float dfHookOffsetZForPatrolboat = 1.8f;
dev_float dfHookOffsetYForPeyote3 = -0.045f;
dev_float dfHookOffsetZForPeyote3 = -0.4f;
dev_float dfHookOffsetYForSlamtruck = 1.255f;
dev_float dfHookOffsetZForSlamtruck = -0.248f;

const Vector3 cvPickUpSubmarineOffset (0.0f, -2.6f, 1.25f);

bool CVehicleGadgetPickUpRope::ComputeIdealAttachmentOffset(const CEntity *pEntity, Vector3 &vIdealPosition)
{
	if(pEntity == NULL)
	{
		return false;
	}

	CVehicle *pVehicle = NULL;
	if(pEntity->GetIsTypeVehicle())
	{
		pVehicle = (CVehicle *)pEntity;
	}

	if(pVehicle)
	{
		bool bIsRoosevelt = ( (MI_CAR_BTYPE.IsValid() && pVehicle->GetModelIndex() == MI_CAR_BTYPE.GetModelIndex())
							  || (MI_CAR_BTYPE3.IsValid() && pVehicle->GetModelIndex() == MI_CAR_BTYPE3.GetModelIndex()) );
		if(bIsRoosevelt)
		{
			vIdealPosition = VEC3_ZERO;
			vIdealPosition.z = pVehicle->GetBoundingBoxMax().z + dfHookOffsetZForRoosevelt;
			return true;
		}
		else if(pVehicle->GetModelIndex() == MI_TANK_RHINO)
		{
			vIdealPosition = VEC3_ZERO;
			vIdealPosition.z = pVehicle->GetBoundingBoxMax().z + dfHookOffsetZForRhino;
			return true;
		}

		if( ( MI_BIKE_OPPRESSOR.IsValid() &&
			  pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR.GetModelIndex() ) ||	
			( MI_BIKE_OPPRESSOR2.IsValid() &&
			  pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2.GetModelIndex() ) )
		{
			vIdealPosition = VEC3_ZERO;
			vIdealPosition.z += dfHookOffsetZForOppressor;
			return true;
		}

		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER || pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE)
		{
			vIdealPosition = VEC3_ZERO;
			vIdealPosition.z = pVehicle->GetBoundingBoxMax().z;
			if(pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CARGOBOB_HOOK_UP_CHASSIS))
			{
				vIdealPosition.z = pVehicle->GetChassisBoundMax().z;
			}

			if( pVehicle->GetVehicleModelInfo() )
			{
				u32 modelNameHash = pVehicle->GetVehicleModelInfo()->GetModelNameHash();
				
				if( modelNameHash  == MI_CAR_PANTO.GetName().GetHash() )
				{
					vIdealPosition.z = ( pVehicle->GetBoundingBoxMax().z - pVehicle->GetPhysArch()->GetBound()->GetCentroidOffset().GetZf() ) + dfHookOffsetZ;
				}
				if( modelNameHash == MI_CAR_GLENDALE.GetName().GetHash() ||
					modelNameHash == MI_CAR_PIGALLE.GetName().GetHash() )
				{
					vIdealPosition.z = pVehicle->GetBoundingBoxMax().z;
				}

				if( modelNameHash == MI_CAR_PACKER.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForPacker;
				}
				else if( modelNameHash == MI_CAR_PHANTOM.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForPhantom;
				}
				else if( modelNameHash == MI_CAR_BRICKADE.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForBrickade;
				}
				else if( modelNameHash == MI_CAR_HAULER.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForHauler;

					if( !pVehicle->GetIsExtraOn( VEH_EXTRA_1 ) )
					{
						vIdealPosition.z += dfHookOffsetZForHaulerWithoutExtra1;
					}
					
					vIdealPosition.y += dfHookOffsetYForHauler;
				}
				else if( modelNameHash == MI_CAR_OMNIS.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForOmnis;
				}
				else if( modelNameHash == MI_CAR_WASTELANDER.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForWastelander;
					vIdealPosition.y += dfHookOffsetYForWastelander;
				}
				else if( modelNameHash == MI_CAR_RUSTON.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForRuston;
					vIdealPosition.y += dfHookOffsetYForRuston;
					vIdealPosition.x += dfHookOffsetXForRuston;
				}
				else if( modelNameHash == MI_CAR_GP1.GetName().GetHash() )
				{
					Vector3 vecBBoxMin;
					Vector3 vecBBoxMax;

					pVehicle->GetApproximateSize( vecBBoxMin, vecBBoxMax );

					vIdealPosition.z = vecBBoxMax.z;
				}
				else if( modelNameHash == MI_CAR_INFERNUS2.GetName().GetHash() )
				{
					Vector3 vecBBoxMin;
					Vector3 vecBBoxMax;

					pVehicle->GetApproximateSize( vecBBoxMin, vecBBoxMax );

					vIdealPosition.z = vecBBoxMax.z + dfHookOffsetXForInfernus2;
				}
				else if( modelNameHash == MI_CAR_HALFTRACK.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForHalftrack;
					vIdealPosition.y += dfHookOffsetYForHalftrack;
				}
				else if( modelNameHash == MI_CAR_TAMPA3.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForTampa3;
					vIdealPosition.y += dfHookOffsetYForTampa3;
				}
				else if( modelNameHash == MI_CAR_APC.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForAPC;
				}
				else if( modelNameHash == MI_CAR_TECHNICAL3.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForTechnical3;
				}
				else if( modelNameHash == MI_TRAILER_TRAILERSMALL2.GetName().GetHash() )
				{
					vIdealPosition.y += dfHookOffsetYForTrailerSmall2;
					vIdealPosition.z += dfHookOffsetZForTrailerSmall2;
				}
				else if( modelNameHash == MI_TANK_KHANJALI.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForKhanjali;
				}
				else if( modelNameHash == MI_CAR_BARRAGE.GetName().GetHash() )
				{
					vIdealPosition.y += dfHookOffsetYForBarrage;
					vIdealPosition.z += dfHookOffsetZForBarrage;
				}
                else if( modelNameHash == MI_CAR_CHERNOBOG.GetName().GetHash() )
                {
                    vIdealPosition.y += dfHookOffsetYForChernobog;
                    vIdealPosition.z += dfHookOffsetZForChernobog;
                }
				else if( modelNameHash == MI_CAR_CARACARA.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForCaracara;
				}
				else if( modelNameHash == MI_CAR_SWINGER.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForSwinger;
					vIdealPosition.y += dfHookOffsetYForSwinger;
				}
				else if( modelNameHash == MI_CAR_POUNDER2.GetName().GetHash() )
				{
					vIdealPosition.z += dfHookOffsetZForPounder2;
				}
				else if (modelNameHash == MI_CAR_LOCUST.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForLocust;
					vIdealPosition.z += dfHookOffsetZForLocust;
				}
				else if (modelNameHash == MI_CAR_ZORRUSSO.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForZorrusso;
					vIdealPosition.z += dfHookOffsetZForZorrusso;
				}
				else if (modelNameHash == MI_CAR_S80.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForS80;
					vIdealPosition.z += dfHookOffsetZForS80;
				}
				else if (modelNameHash == MI_CAR_DYNASTY.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForDynasty;
					vIdealPosition.z += dfHookOffsetZForDynasty;
				}
				else if (modelNameHash == MI_CAR_NEO.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForNeo;
					vIdealPosition.z += dfHookOffsetZForNeo;
				}
				else if (modelNameHash == MI_CAR_NOVAK.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForNovak;
					vIdealPosition.z += dfHookOffsetZForNovak;
				}
				else if (modelNameHash == MI_CAR_PEYOTE2.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForPeyote2;
					vIdealPosition.z += dfHookOffsetZForPeyote2;
				}
				else if (modelNameHash == MI_CAR_ZION3.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForZion3;
					vIdealPosition.z += dfHookOffsetZForZion3;
				}
                else if( modelNameHash == MID_PEYOTE3 )
                {
                    vIdealPosition.y += dfHookOffsetYForPeyote3;
                    vIdealPosition.z += dfHookOffsetZForPeyote3;
                } 
				else if (modelNameHash == MI_CAR_SLAMTRUCK.GetName().GetHash())
				{
					vIdealPosition.y += dfHookOffsetYForSlamtruck;
					vIdealPosition.z += dfHookOffsetZForSlamtruck;
				}
			}

			return true;
		}

		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE)
		{
			vIdealPosition = cvPickUpSubmarineOffset + ZAXIS * dfHookOffsetZ;
			return true;
		}

		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
            if( pVehicle->GetVehicleModelInfo()->GetModelNameHash() != MI_BOAT_PATROLBOAT.GetName().GetHash() )
            {
                vIdealPosition = VEC3_ZERO;
                vIdealPosition.z += 0.85f;
            }
            else
            {
                vIdealPosition.y += dfHookOffsetYForPatrolboat;
                vIdealPosition.z += dfHookOffsetZForPatrolboat;
            }
            
			return true;
		}
	}
	

	if(pEntity->GetArchetype() && pEntity->GetModelIndex()==MI_HANDLER_CONTAINER)
	{
		Matrix34 matEntityMatrix = MAT34V_TO_MATRIX34(pEntity->GetMatrix());
		float fXAxisUp = matEntityMatrix.a.z;
		float fYAxisUp = matEntityMatrix.b.z;
		float fZAxisUp = matEntityMatrix.c.z;
		float fMaxUP = Max(Abs(fXAxisUp), Abs(fYAxisUp), Abs(fZAxisUp));

		vIdealPosition = pEntity->GetBoundingBoxMin() + (pEntity->GetBoundingBoxMax() - pEntity->GetBoundingBoxMin()) * 0.5f;
		if(Abs(fXAxisUp) == fMaxUP)
		{
			if(fXAxisUp > 0.0f)
			{
				vIdealPosition.x = pEntity->GetBoundingBoxMax().x + dfHookOffsetZ;
			}
			else
			{
				vIdealPosition.x = pEntity->GetBoundingBoxMin().x - dfHookOffsetZ;
			}
		}
		else if(Abs(fYAxisUp) == fMaxUP)
		{
			if(fYAxisUp > 0.0f)
			{
				vIdealPosition.y = pEntity->GetBoundingBoxMax().y + dfHookOffsetZ;
			}
			else
			{
				vIdealPosition.y = pEntity->GetBoundingBoxMin().y - dfHookOffsetZ;
			}
		}
		else // Abs(fZAxisUp) == fMaxUP
		{
			if(fZAxisUp > 0.0f)
			{
				vIdealPosition.z = pEntity->GetBoundingBoxMax().z + dfHookOffsetZ;
			}
			else
			{
				vIdealPosition.z = pEntity->GetBoundingBoxMin().z - dfHookOffsetZ;
			}
		}

		return true;
	}
	return false;
}

void CVehicleGadgetPickUpRope::AttachEntityToPickUpProp( CVehicle *pPickUpVehicle, CPhysical *pEntityToAttach, s16 iBoneIndex, const Vector3 &vEntityPickupOffsetPosition, s32 iComponentIndex, bool bNetAssert  )
{
	if (bNetAssert && NetworkUtils::IsNetworkCloneOrMigrating(pEntityToAttach))
	{
		Assertf(0, "Trying to attach %s to pickup rope which is not local", pEntityToAttach->GetNetworkObject()->GetLogName());
		return;
	}

	if(m_pTowedEntity == pEntityToAttach)
	{
		return;
	}

	if(pEntityToAttach == m_excludeEntity.GetEntity())
	{
		return;
	}

	if(!m_PropObject)
		return;

	if( pEntityToAttach &&
		pEntityToAttach->GetIsTypeVehicle() )
	{
		CVehicle* pVehicleToAttach = static_cast< CVehicle* >( pEntityToAttach );

		if( NetworkInterface::IsGameInProgress() )
		{
			if( pVehicleToAttach->GetIsBeingTowed() ||
				pVehicleToAttach->GetIsTowing() )
			{
				vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachEntityToPickUpProp - Can't attach to vehicle that is already being towed or towing to something. Vehicle: %s", pPickUpVehicle->GetNetworkObject() ? pPickUpVehicle->GetNetworkObject()->GetLogName() : pPickUpVehicle->GetModelName() );
				return;
			}
		}
	}

	// Ensure that certain vehicle/object types are prepared for pickup.
	CVehicle *pVehicleToAttach = NULL;
	if(pEntityToAttach->GetIsTypeVehicle())
	{
		pVehicleToAttach = static_cast<CVehicle*>(pEntityToAttach);

		if(pVehicleToAttach->InheritsFromBoat())
		{
			CBoat* pBoat = static_cast<CBoat*>(pVehicleToAttach);

			// If we are attaching to a boat, make sure to break any anchor it may have. Otherwise the constraints will fight
			// when we try to lift it and it may get stuck in the air if we drop it and freeze there when low LOD anchor mode kicks in.
			pBoat->GetAnchorHelper().Anchor(false);
		}
		else if(pVehicleToAttach->InheritsFromSubmarine())
		{
			CSubmarine* pSub = static_cast<CSubmarine*>(pVehicleToAttach);
			CSubmarineHandling* pSubHandling = pSub->GetSubHandling();

			// If we are attaching to a submarine, make sure to break any anchor it may have. Otherwise the constraints will fight
			// when we try to lift it.
			pSubHandling->GetAnchorHelper().Anchor(false);
		}

		//Detach trailers from cabs
		if(pVehicleToAttach->GetAttachedTrailer())
		{
			pVehicleToAttach->GetAttachedTrailer()->DetachFromParent(0);
		}

		//Detach cabs from trailers.
		if(pVehicleToAttach->InheritsFromTrailer())
		{
			CTrailer *pTrailer = static_cast<CTrailer*>(pVehicleToAttach);
			if(pTrailer->GetIsAttached())
			{
				pTrailer->DetachFromParent(0);
			}
		}

		//Detach the vehicle if it's attached to something
		if(pVehicleToAttach->GetIsAttached())
		{
			pVehicleToAttach->DetachFromParent(0);
		}
	}
	else
	{
		if(pEntityToAttach->GetModelIndex()==MI_HANDLER_CONTAINER && pEntityToAttach->GetIsAttached())
		{
			pEntityToAttach->DetachFromParent(0);
		}
	}

	// Why would this happen?
	if(!m_bRopesAndHookInitialized)
	{
		AddRopesAndProp(*pPickUpVehicle);
	}

	// Handle cases where the entity being picked up has moved to be far from the pickup-prop.
	UpdateTowedVehiclePosition(pPickUpVehicle, pEntityToAttach);

	// We need to move the thing we're picking up.
	if(pEntityToAttach->GetIsAnyFixedFlagSet())
	{
		pEntityToAttach->SetFixedPhysics(false, NetworkInterface::IsGameInProgress());
	}

	m_bEnableFreezeWaitingOnCollision = pEntityToAttach->GetAllowFreezeWaitingOnCollision();
	pEntityToAttach->SetAllowFreezeWaitingOnCollision(false);

	// Calculate the position on the entity where the initial attachment will happen.
	Vector3 vEntityPickupPointAbsolutePosition(vEntityPickupOffsetPosition);
	Matrix34 matPickupEntity;

	// If we've been passed in a bone get the matrix for it, otherwise use the entity matrix
	if(iBoneIndex > -1 && pEntityToAttach->GetSkeleton())
	{
		// What to do if global matrices aren't up to date?
		pEntityToAttach->GetGlobalMtx(iBoneIndex, matPickupEntity);
	}
	else
	{
		matPickupEntity = MAT34V_TO_MATRIX34(pEntityToAttach->GetMatrix());
	}

	matPickupEntity.Transform(vEntityPickupPointAbsolutePosition);

	// Get the position of the pickup point.
	Matrix34 pickupPointPositionMatrix;
	pickupPointPositionMatrix.Identity();
	if(GetPickupAttachBone() > -1 && m_PropObject->GetSkeleton())
	{
		m_PropObject->GetGlobalMtx(GetPickupAttachBone(), pickupPointPositionMatrix);
	}

	pickupPointPositionMatrix.d += GetPickupAttachOffset();

	if(m_PropObject->GetModelId() == MI_PROP_HOOK)
	{
		// Move the hook so it contacts the vehicle.
		Matrix34 matPickup = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());

		Vector3 distanceFromContact = (vEntityPickupPointAbsolutePosition - pickupPointPositionMatrix.d);
		matPickup.d += distanceFromContact;

		m_PropObject->SetMatrix(matPickup, true, true);		
	}

	// Get the component of the vehicle we are trying to attach to
	s32 iComponentIndexToUse = iComponentIndex;
	// If the component index has not been specified but a bone has, get the component index from the bone
	if(iComponentIndexToUse == 0 && iBoneIndex != -1)
	{
		CBaseModelInfo* pModelInfo = pEntityToAttach->GetBaseModelInfo();
		if(pModelInfo && pModelInfo->GetDrawable() && pModelInfo->GetDrawable()->GetSkeletonData())
		{
			iComponentIndexToUse = pModelInfo->GetFragType()->GetComponentFromBoneIndex(0, iBoneIndex);
		}
	}

	// Make sure we have a valid component
	if(iComponentIndexToUse == -1)
	{
		iComponentIndexToUse = 0;
	}
		
	if(pEntityToAttach->GetCurrentPhysicsInst())
	{
		phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(pEntityToAttach->GetCurrentPhysicsInst()->GetArchetype());
		m_vTowedVehicleOriginalAngDamp = pArchetypeDamp->GetDampingConstant(phArchetypeDamp::ANGULAR_V);
		m_vTowedVehicleOriginalAngDampC = pArchetypeDamp->GetDampingConstant(phArchetypeDamp::ANGULAR_C);
	}

	// Detach the ropes from the prop and attach them directly to the picked-up entity.
	AttachRopesToObject( pPickUpVehicle, pEntityToAttach, iComponentIndexToUse);
	
	// Mark the vehicle as being towed.
	m_pTowedEntity = pEntityToAttach;
	ComputeAndSetInverseMassScale(pPickUpVehicle);

	// Calculate the start position of the hook and the final desired position.
	if(m_pTowedEntity)
	{
		m_fPickupPositionTransitionTime = 0.0f;

		Matrix34 matPickedUpEnt = MAT34V_TO_MATRIX34(m_pTowedEntity->GetMatrix());
		Matrix34 matPickup = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());
		
		matPickedUpEnt.UnTransform(matPickup.d);

		m_vStartPickupPropPosition = matPickup.d;
		m_vCurrentPickupPropPosition = matPickup.d;
			
		Vector3 vAxes[6] = {
			Vector3(0, 0, 1),
			Vector3(0, 0, -1),
			Vector3(1, 0, 0),
			Vector3(-1, 0, 0),
			Vector3(0, 1, 0),
			Vector3(0, -1, 0),
		};

		// Default to upright.
		int nBestAxis = 0;

		// Only do this if we're not wanting the picked-up entity to always be upright.
		if(!m_bEnsurePickupEntityUpright)
		{
			// Find which of the attached entity's axis is most aligned with the current magnet "up" vector and use that to specify the new "up" vector relative to the attached entity for the magnet attachment.
			Vector3 vMagnetUp = matPickup.c;
			float fAxisOnMagnetUp = -FLT_MAX;

			for(int nAxis = 0; nAxis < 6; nAxis++)
			{
				Vector3 vTransformedAxis = vAxes[nAxis];
				matPickedUpEnt.Transform3x3(vTransformedAxis);

				float fThisAxisOnMagnetUp = vTransformedAxis.Dot(vMagnetUp);
				if(fThisAxisOnMagnetUp > fAxisOnMagnetUp)
				{
					fAxisOnMagnetUp = fThisAxisOnMagnetUp;
					nBestAxis = nAxis;
				}
			}
		}

		// Calculate a matrix from the new "up" vector determined above.
		Matrix34 matDesiredPropRotation;
		matDesiredPropRotation.c = vAxes[nBestAxis];
		matDesiredPropRotation.b.Cross(vAxes[nBestAxis], abs(vAxes[nBestAxis].x) == 0 ? XAXIS : YAXIS);
		matDesiredPropRotation.a.Cross(matDesiredPropRotation.c, matDesiredPropRotation.b);
		matDesiredPropRotation.Normalize();

		// See if there's an existing hardcoded offset hack for this entity and use that if so.
		// This were used for the hook gadget so only do this if the pick-up axis is positive Z.
		bool bUsedOldOffsetCalculation = false;
		if(nBestAxis == 0 && ComputeIdealAttachmentOffset(m_pTowedEntity, m_vDesiredPickupPropPosition))
		{
			bUsedOldOffsetCalculation = true;
		}
		
		if(!bUsedOldOffsetCalculation)
		{
			// Use the new "up" vector to determine the ideal attachment offset vector from the bounding box of the attached entity.
			Vector3 vBBMin = m_pTowedEntity->GetBoundingBoxMin();
			Vector3 vBBMax = m_pTowedEntity->GetBoundingBoxMax();

			// If a vehicle, use an adjusted bounding box that doesn't include doors so the attachment will be flush with the vehicle body even if there are doors flapping around.
			if(m_pTowedEntity->GetIsTypeVehicle())
			{
				 static_cast<CVehicle *>(m_pTowedEntity.Get())->GetApproximateSize(vBBMin, vBBMax);
			}

			m_vDesiredPickupPropPosition = Vector3(0, 0, 0);		
			if(vAxes[nBestAxis].x != 0)
				m_vDesiredPickupPropPosition.x += vAxes[nBestAxis].x > 0 ? vBBMax.x : vBBMin.x;

			if(vAxes[nBestAxis].y != 0)
				m_vDesiredPickupPropPosition.y += vAxes[nBestAxis].y > 0 ? vBBMax.y : vBBMin.y;

			if(vAxes[nBestAxis].z != 0)
				m_vDesiredPickupPropPosition.z += vAxes[nBestAxis].z > 0 ? vBBMax.z : vBBMin.z;
		}		

		if( m_PropObject->GetSkeleton() )
		{
			// Take into account any offsets needed to ensure the pick-up prop is placed on top of the ideal pick-up position (and not midway through it).
			Vector3 vOffset = m_PropObject->GetObjectMtx(GetPickupAttachBone()).d + GetPickupAttachOffset();
			matDesiredPropRotation.Transform3x3(vOffset);

			m_vDesiredPickupPropPosition += -vOffset;
		}
	
		// Calculate current magnet orientation relative to the picked-up entity and specify the target orientation for the smooth transition lerp.
		matPickedUpEnt.Inverse3x3();		
		matPickup.Dot3x3(matPickedUpEnt);
		
		m_qStartPickupPropRotation.FromMatrix34(matPickup);		
		m_qCurrentPickupPropRotation.FromMatrix34(matPickup);				
		m_qDesiredPickupPropRotation.FromMatrix34(matDesiredPropRotation);	
		
		// This is really important.
		m_qStartPickupPropRotation.PrepareSlerp(m_qDesiredPickupPropRotation);
	}

	// Attach the prop directly to the vehicle.	
	m_PropObject->AttachToPhysicalBasic( pEntityToAttach, iBoneIndex, ATTACH_STATE_BASIC | ATTACH_FLAG_COL_ON, &m_vStartPickupPropPosition, &m_qStartPickupPropRotation );
		
	// Setup the attachment extension
	// In network games, it is important that this is setup on both clones and non-clones as when the pick-up rope owner detaches the entity, the change to this attachment is used to properly detach the entity on the remote machines.
	u16 uAttachFlags = ATTACH_STATE_PHYSICAL | ATTACH_FLAG_POS_CONSTRAINT | ATTACH_FLAG_DO_PAIRED_COLL;

	fwAttachmentEntityExtension *extension = pEntityToAttach->CreateAttachmentExtensionIfMissing();
	extension->SetParentAttachment(pEntityToAttach, pPickUpVehicle); //set the pickup vehicle as the parent
	extension->SetAttachFlags(uAttachFlags);
	extension->SetMyAttachBone(static_cast<s16>(iComponentIndex)); // bit of a hack! Store the component index in the attach bone (we can't get the bone from the comp index here)
	extension->SetAttachOffset(vEntityPickupOffsetPosition);
	
	//make sure the pickup vehicle doesn't turn into a dummy
	pPickUpVehicle->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;

	if(pVehicleToAttach)
	{
		//if there's no driver turn off the handbrake and brakes.
		if(!pVehicleToAttach->GetDriver())
		{
			//set the no driver task, to set the control inputs
			if (!pVehicleToAttach->IsNetworkClone())
			{
				pVehicleToAttach->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, 
					rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_TOWED), 
					VEHICLE_TASK_PRIORITY_PRIMARY, 
					false);
			}
		}

		if(pVehicleToAttach->GetVehicleAudioEntity())
		{
			audVehicleAudioEntity* audioEntity = pVehicleToAttach->GetVehicleAudioEntity();
			audioEntity->TriggerTowingHookAttachSound();
		}
	}

	if (NetworkInterface::IsGameInProgress() && m_pTowedEntity && m_pTowedEntity->GetIsPhysical())
	{
		netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(m_pTowedEntity);

		if (pNetObj)
		{
			static_cast<CNetObjPhysical*>(pNetObj)->OnAttachedToPickUpHook();
		}
	}

	// allow map objects being picked up by helicopters, using permanent archetypes (such as shipping containers etc)
	// to become decoupled from the map so they aren't destroyed when the map section is unloaded.
	if (pPickUpVehicle->InheritsFromHeli() && pEntityToAttach->GetIsTypeObject())
	{
		CObject* pObjToPickup = static_cast<CObject*>(pEntityToAttach);
	
		if (pObjToPickup->GetNetworkObject())
		{
			static_cast<CNetObjObject*>(pObjToPickup->GetNetworkObject())->OnAttachedToPickUpHook();
		}

		if (pEntityToAttach->GetIplIndex() && pObjToPickup->GetRelatedDummy())
		{
			pObjToPickup->DecoupleFromMap();
		}
	}

	//Add a shocking to notify any relevant peds.
	CEventShockingVehicleTowed ev(*pPickUpVehicle, *pEntityToAttach);
	CShockingEventsManager::Add(ev);

	vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachEntityToPickupProp - Attract entity attached. Parent Vehicle: %s", pPickUpVehicle->GetNetworkObject() ? pPickUpVehicle->GetNetworkObject()->GetLogName() : pPickUpVehicle->GetModelName() );
}

void CVehicleGadgetPickUpRope::EnforceConstraintLimits(phConstraintRotation* pConstraint, float fMin, float fMax, float fCurrent, bool bIsPitch)
{
	fMin -= fCurrent;
	fMax -= fCurrent;

	if(bIsPitch)
	{
		fMin = fwAngle::LimitRadianAngleForPitch(fMin);
		fMax = fwAngle::LimitRadianAngleForPitch(fMax);
	}
	else
	{
		fMin = fwAngle::LimitRadianAngle(fMin);
		fMax = fwAngle::LimitRadianAngle(fMax);
	}

	if(fMin > fMax)
	{
		SwapEm(fMin, fMax);
	}

	pConstraint->SetMaxLimit(fMax);
	pConstraint->SetMinLimit(fMin);
}

dev_float sfPickUpRopeForceMultToApplyInConstraint = 0.70f;
dev_float dfPickUpRopeConstraintAdditionalStrength = 500.0;
dev_float dfPickUpRopeConstraintStrengthMassMulti = 1.0;
//////////////////////////////////////////////////////////////////////////
void CVehicleGadgetPickUpRope::AttachRopesToObject( CVehicle* pParent, CPhysical *pObject, s32 UNUSED_PARAM(iComponentIndex)/*= 0*/ /*, Vector3 vAttachOffset*/)
{
	vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - Begin attaching ropes to %s parent vehicle %s", pObject ? pObject->GetDebugName() : "NULL", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );

	if(m_RopeInstanceA && m_RopeInstanceB && m_PropObject && pParent->GetCurrentPhysicsInst() && pObject && pObject->GetCurrentPhysicsInst() && pParent->GetSkeleton())
	{
		//If the bones aren't valid just give up.
		if(m_iTowMountA >= pParent->GetSkeleton()->GetBoneCount() || m_iTowMountB >= pParent->GetSkeleton()->GetBoneCount())
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - Invalid rope attach bones. parent vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			return;
		}

		if( !PHLEVEL->IsInLevel( pParent->GetCurrentPhysicsInst()->GetLevelIndex() ) )
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - Parent is not in the level. parent vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			return;
		}

		if( !PHLEVEL->IsInLevel( pObject->GetCurrentPhysicsInst()->GetLevelIndex() ) )
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - Object is not in the level. parent vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			return;
		}

		pParent->UpdateSkeleton();

		//detach the rope from the old physics inst if it's still attached
		if( m_RopeInstanceA->GetInstanceA() == m_RopesAttachedVehiclePhysicsInst || m_RopeInstanceA->GetInstanceB() == m_RopesAttachedVehiclePhysicsInst )
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - Detaching rope A (%p) from object: %p | Index: %i | pParent: %p | pParent->GetCurrentPhysicsInst(): %p | pParent->GetCurrentPhysicsInst() level index: %i | pObject: %p | pObject->GetCurrentPhysicsInst(): %p | pObject->GetCurrentPhysicsInst(): %i | parent name: %s ",
				m_RopeInstanceA,
				m_RopesAttachedVehiclePhysicsInst, m_RopesAttachedVehiclePhysicsInst ? m_RopesAttachedVehiclePhysicsInst->GetLevelIndex() : -1, pParent,
				pParent->GetCurrentPhysicsInst(), pParent->GetCurrentPhysicsInst()->GetLevelIndex(), 
				pObject, pObject->GetCurrentPhysicsInst(), pObject->GetCurrentPhysicsInst()->GetLevelIndex(), 
				pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			m_RopeInstanceA->DetachFromObject(m_RopesAttachedVehiclePhysicsInst);
			
			CNetObjPhysical* physicalObject = static_cast<CNetObjPhysical*>(pObject->GetNetworkObject());
			if (physicalObject)
			{
				physicalObject->DisableBlenderForOneFrameAfterDetaching(fwTimer::GetFrameCount());
			}
		}

		if( m_RopeInstanceB->GetInstanceA() == m_RopesAttachedVehiclePhysicsInst || m_RopeInstanceB->GetInstanceB() == m_RopesAttachedVehiclePhysicsInst )
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - Detaching rope B (%p) from object: %p | Index: %i | pParent: %p | pParent->GetCurrentPhysicsInst(): %p | pParent->GetCurrentPhysicsInst() level index: %i | pObject: %p | pObject->GetCurrentPhysicsInst(): %p | pObject->GetCurrentPhysicsInst(): %i | parent name: %s",
				m_RopeInstanceB,
				m_RopesAttachedVehiclePhysicsInst, m_RopesAttachedVehiclePhysicsInst ? m_RopesAttachedVehiclePhysicsInst->GetLevelIndex() : -1, pParent,
				pParent->GetCurrentPhysicsInst(), pParent->GetCurrentPhysicsInst()->GetLevelIndex(), 
				pObject, pObject->GetCurrentPhysicsInst(), pObject->GetCurrentPhysicsInst()->GetLevelIndex(), 
				pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			m_RopeInstanceB->DetachFromObject(m_RopesAttachedVehiclePhysicsInst);

			CNetObjPhysical* physicalObject = static_cast<CNetObjPhysical*>(pObject->GetNetworkObject());
			if (physicalObject)
			{
				physicalObject->DisableBlenderForOneFrameAfterDetaching(fwTimer::GetFrameCount());
			}
		}

		//detach the tow truck from anything it is attached to
		pParent->DetachFromParent(0);
		//make sure the prop is detached
		m_PropObject->DetachFromParent(0);

		UpdateHookPosition(pParent, true);

		//get the position of the bones at the end of the tow truck arm.
		Matrix34 TruckMatrix;
		Matrix34 matLocalMountA, matMountA;
		Matrix34 matLocalMountB, matMountB;
		TruckMatrix = MAT34V_TO_MATRIX34(pParent->GetMatrix());

		matMountA = matLocalMountA = pParent->GetObjectMtx(m_iTowMountA);
		TruckMatrix.Transform(matMountA.d);

		matMountB = matLocalMountB = pParent->GetObjectMtx(m_iTowMountB);
		TruckMatrix.Transform(matMountB.d);

		//get the position of where the ropes should attach from the hook. Using local matrices as the globals are probably out of date.		 ;
		Matrix34 hookRopeConnectionPointMatrixA;
		Matrix34 hookRopeConnectionPointMatrixB;
		Matrix34 hookMatrix = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());

		
		if(m_PropObject && m_PropObject->GetCurrentPhysicsInst() && m_PropObject->GetSkeleton() && 
			GetPropRopeAttachBoneA() < m_PropObject->GetSkeleton()->GetBoneCount() &&
			GetPropRopeAttachBoneB() < m_PropObject->GetSkeleton()->GetBoneCount())
		{
			hookRopeConnectionPointMatrixA = m_PropObject->GetObjectMtx(GetPropRopeAttachBoneA());				
			hookRopeConnectionPointMatrixB = m_PropObject->GetObjectMtx(GetPropRopeAttachBoneB());				
			hookMatrix.Transform(hookRopeConnectionPointMatrixA.d);
			hookMatrix.Transform(hookRopeConnectionPointMatrixB.d);
		}
		else
		{
			hookRopeConnectionPointMatrixA = hookMatrix;
			hookRopeConnectionPointMatrixB = hookMatrix;
		}

		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - matMountA.d: %f, %f, %f | matMountB.d:  %f, %f, %f | hookMatrix.d:  %f, %f, %f | hookRopeConnectionPointMatrixA.d:  %f, %f, %f | hookRopeConnectionPointMatrixB.d:  %f, %f, %f | parent NAME: %s",
			 matMountA.d.x, matMountA.d.y, matMountA.d.z,
			 matMountB.d.x, matMountB.d.y, matMountB.d.z,
			 hookMatrix.d.x, hookMatrix.d.y, hookMatrix.d.z,
			 hookRopeConnectionPointMatrixA.d.x, hookRopeConnectionPointMatrixA.d.y, hookRopeConnectionPointMatrixA.d.z,
			 hookRopeConnectionPointMatrixB.d.x, hookRopeConnectionPointMatrixB.d.y, hookRopeConnectionPointMatrixB.d.z, 
			 pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() 
			);
		

		// Change the amount of force applied in the constraint depending on the mass of the tow truck and the vehicle being towed, to try and improve handling
		float forceToApply = 1.0f - sfPickUpRopeForceMultToApplyInConstraint * rage::Clamp( (pObject->GetMass()/pParent->GetMass()) -0.1f , 0.0f, 1.0f);
		m_fForceMultToApplyInConstraint = rage::Clamp(forceToApply, 0.0f, 1.0f);

		int iTowArmComponent = 0;

		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - m_fRopeLength: %f Parent: %s", m_fRopeLength, pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );

		//finally attach the ropes to the object
		m_RopeInstanceA->AttachObjects( VECTOR3_TO_VEC3V(matMountA.d),
			VECTOR3_TO_VEC3V(hookRopeConnectionPointMatrixA.d),
			pParent->GetCurrentPhysicsInst(),
			pObject->GetCurrentPhysicsInst(), 
			iTowArmComponent,//use the tow arm as the component
			0,//iComponentIndex, // always connect to component 0 for now
			m_fRopeLength,
			sfPickUpRopeAllowedConstraintPenetration,
			m_fForceMultToApplyInConstraint, 1.0f, true);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && m_PropObject == pObject)
		{
			Matrix34 matInverse = MAT34V_TO_MATRIX34(pParent->GetMatrix());
			matInverse.Inverse();
			Vector3 objectSpacePosA = matMountA.d;
			matInverse.Transform(objectSpacePosA);

			matInverse = MAT34V_TO_MATRIX34(pObject->GetMatrix());
			matInverse.Inverse();
			Vector3 objectSpacePosB = hookRopeConnectionPointMatrixA.d;
			matInverse.Transform(objectSpacePosB);

			CReplayMgr::RecordPersistantFx<CPacketAttachEntitiesToRope>(
				CPacketAttachEntitiesToRope( m_RopeInstanceA->GetUniqueID(), VECTOR3_TO_VEC3V(objectSpacePosA), VECTOR3_TO_VEC3V(objectSpacePosB), iTowArmComponent, 0, m_fRopeLength, sfPickUpRopeAllowedConstraintPenetration, m_fForceMultToApplyInConstraint, 1.0f, true, NULL, NULL, 1, false),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceA),
				pParent,
				pObject,
				false);
		}
#endif

		phConstraintDistance* pRopePosConstraint = static_cast<phConstraintDistance*>( m_RopeInstanceA->GetConstraint() );
		if(pRopePosConstraint)
		{
			pRopePosConstraint->SetBreakable(false);
		}

		m_RopeInstanceB->AttachObjects( VECTOR3_TO_VEC3V(matMountB.d),
			VECTOR3_TO_VEC3V(hookRopeConnectionPointMatrixB.d),
			pParent->GetCurrentPhysicsInst(),
			pObject->GetCurrentPhysicsInst(), 
			iTowArmComponent,//use the tow arm as the component
			0,//iComponentIndex,// always connect to component 0 for now
			m_fRopeLength,
			sfPickUpRopeAllowedConstraintPenetration,
			m_fForceMultToApplyInConstraint, 1.0f, true);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && m_PropObject == pObject)
		{
			Matrix34 matInverse = MAT34V_TO_MATRIX34(pParent->GetMatrix());
			matInverse.Inverse();
			Vector3 objectSpacePosA = matMountB.d;
			matInverse.Transform(objectSpacePosA);

			matInverse = MAT34V_TO_MATRIX34(pObject->GetMatrix());
			matInverse.Inverse();
			Vector3 objectSpacePosB = hookRopeConnectionPointMatrixB.d;
			matInverse.Transform(objectSpacePosB);

			CReplayMgr::RecordPersistantFx<CPacketAttachEntitiesToRope>(
				CPacketAttachEntitiesToRope( m_RopeInstanceB->GetUniqueID(), VECTOR3_TO_VEC3V(objectSpacePosA), VECTOR3_TO_VEC3V(objectSpacePosB), iTowArmComponent, 0, m_fRopeLength, sfAllowedConstraintPenetration, sfPickUpRopeAllowedConstraintPenetration, 1.0f, true, NULL, NULL, 2, false),
				CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceB),
				pParent,
				pObject,
				false);
		}
#endif

		pRopePosConstraint = static_cast<phConstraintDistance*>( m_RopeInstanceB->GetConstraint() );
		if(pRopePosConstraint)
		{
			pRopePosConstraint->SetBreakable(false);
		}

		//keep a record of the physinst we attached to, so if it changes we can reattach to the new one.
		m_RopesAttachedVehiclePhysicsInst = pParent->GetCurrentPhysicsInst();

		if(pObject != m_PropObject)
		{
			//if we are connected to a car then do extra iterations.
			pObject->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);
			pParent->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_OPTIONAL_ITERATIONS, true);
		}

		// make sure the object being picked up is networked in MP
		if (NetworkInterface::IsGameInProgress() && pObject->GetIsTypeObject() && !pParent->IsNetworkClone() && !pObject->GetNetworkObject() && CNetObjObject::HasToBeCloned((CObject*)pObject))
		{
			char reason[100];
			formatf(reason, "Attached to vehicle rope %s", pParent->GetNetworkObject() ?  pParent->GetNetworkObject()->GetLogName() : "??");
			CNetObjObject::TryToRegisterAmbientObject(*((CObject*)pObject), true, reason);
		}

		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::AttachRopesToObject - Setting m_Setup to true.");
		m_Setup = true;
	}
}

void CVehicleGadgetPickUpRope::UpdateHookPosition(CVehicle *pParentVehicle, bool bUpdateConstraints)
{
	//Make sure the hook position is relatively up to date
	CPhysical *pEnt = m_pTowedEntity.Get() ? m_pTowedEntity.Get() : m_PropObject.Get();
	
	// Distance at which the the prop will warp to position.
	// Slightly less than the constraint depth max. distance to prevent the assert from firing on non-host machines that might have the constraints setup.
	Vector3 vLocalMountA = pParentVehicle->GetObjectMtx(m_iTowMountA).d;
	Vector3 vLocalMountB = pParentVehicle->GetObjectMtx(m_iTowMountB).d;
	Vector3 vIdealHookPos = (vLocalMountA + ((vLocalMountB - vLocalMountA) / 2.0f)) + Vector3(0.0f, 0.0f, -(m_fRopeLength - 0.1f));

	// When the ropes have not yet been setup and attached to the prop (Could happen if the prop hasn't streamed in yet), make the warp distance a lot smaller than it is otherwise (Make it hover roughly about the rope length away).
	// This is to ensure that clones that have setup correctly aren't receiving prop object positions from the host that totally violate their local constraints.
	const float fWarpDist = m_Setup ? PH_CONSTRAINT_ASSERT_DEPTH : vIdealHookPos.Mag();
	if(DistSquared(pEnt->GetMatrix().d(), pParentVehicle->GetMatrix().GetCol3()).Getf() >= square(fWarpDist))
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::UpdateHookPosition - Warping 'hook' to safe position as it has moved too far from the parent vehicle %s.", pParentVehicle->GetNetworkObject() ? pParentVehicle->GetNetworkObject()->GetLogName() : pParentVehicle->GetModelName() );
		WarpToPosition(pParentVehicle, bUpdateConstraints);		
	}
}

void CVehicleGadgetPickUpRope::UpdateTowedVehiclePosition(CVehicle *UNUSED_PARAM(pPickUpVehicle), CPhysical *pEntityToAttach)
{
	if(DistSquared(m_PropObject->GetTransform().GetPosition(), pEntityToAttach->GetTransform().GetPosition()).Getf() >= square(PH_CONSTRAINT_ASSERT_DEPTH))
	{
		//This can happen in MP or if the player approaches a tow truck that's towing a vehicle whose position isn't set yet
		Vec3V vBBMaxWorld = pEntityToAttach->GetTransform().Transform(VECTOR3_TO_VEC3V(pEntityToAttach->GetBoundingBoxMax()));
		Vec3V vBBMinWorld = pEntityToAttach->GetTransform().Transform(VECTOR3_TO_VEC3V(pEntityToAttach->GetBoundingBoxMin()));

		bool bInvalidPos = DistSquared(m_PropObject->GetTransform().GetPosition(), vBBMaxWorld).Getf() >= square(PH_CONSTRAINT_ASSERT_DEPTH) &&
						 DistSquared(m_PropObject->GetTransform().GetPosition(), vBBMinWorld).Getf() >= square(PH_CONSTRAINT_ASSERT_DEPTH);

		if(bInvalidPos)
		{
			Vec3V vNewPos = m_PropObject->GetTransform().Transform(-VECTOR3_TO_VEC3V(pEntityToAttach->GetBoundingBoxMax()));
			if(pEntityToAttach->GetIsTypeVehicle())
			{
				CVehicle *pVehicle = (CVehicle *)pEntityToAttach;
				Matrix34 matOld = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
				pVehicle->TeleportWithoutUpdateGadgets(VEC3V_TO_VECTOR3(vNewPos), camFrame::ComputeHeadingFromMatrix(MAT34V_TO_MATRIX34(pEntityToAttach->GetMatrix())), true, true);
				pVehicle->UpdateGadgetsAfterTeleport(matOld, false); // do not detach from gadget
			}
			else
			{
				pEntityToAttach->Teleport(VEC3V_TO_VECTOR3(vNewPos), camFrame::ComputeHeadingFromMatrix(MAT34V_TO_MATRIX34(pEntityToAttach->GetMatrix())), false, true, false, true, false, true);
			}
		}
	}
}

void CVehicleGadgetPickUpRope::UpdateAttachments(CVehicle *pParentVehicle)
{ 
	if( !m_RopeInstanceA || !m_RopeInstanceB )
		return;

	if(!pParentVehicle)
		return;

	phConstraintDistance* pRopePosConstraintA = static_cast<phConstraintDistance*>( m_RopeInstanceA->GetConstraint() );
	phConstraintDistance* pRopePosConstraintB = static_cast<phConstraintDistance*>( m_RopeInstanceB->GetConstraint() );

	//adjust the position of the hook, lerp the hook to the desired position
	if(m_pTowedEntity)
	{
		bool bIsRootBrokenOff = m_pTowedEntity->GetFragInst() && m_pTowedEntity->GetFragInst()->GetCacheEntry() && m_pTowedEntity->GetFragInst()->GetCacheEntry()->IsGroupBroken(0);
		if((pRopePosConstraintA && pRopePosConstraintA->IsBroken()) ||
			(pRopePosConstraintB && pRopePosConstraintB->IsBroken()) || m_pTowedEntity->GetIsFixedRoot() || bIsRootBrokenOff)
		{
			if(m_pTowedEntity && m_pTowedEntity->IsNetworkClone() && m_pTowedEntity->GetIsOnScreen())
			{
				float distanceToEntity = 0.0f;
				const netPlayer* myPlayer = NetworkInterface::GetLocalPlayer();

				if (myPlayer)
				{
					distanceToEntity = (VEC3V_TO_VECTOR3(m_pTowedEntity->GetTransform().GetPosition()) - NetworkInterface::GetPlayerCameraPosition(*myPlayer)).XYMag2();
				}		

				if(distanceToEntity < MOVER_BOUNDS_PRESTREAM * MOVER_BOUNDS_PRESTREAM)
				{
					// send an event to the owner of the attached entity requesting they detach
					CRequestDetachmentEvent::Trigger(*m_pTowedEntity);
				}
			}
			else if (!m_pTowedEntity->IsNetworkClone())
			{
				vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::UpdateAttachments - Detaching entity because rope constraint broken. Parent vehicle: %s", pParentVehicle->GetNetworkObject() ? pParentVehicle->GetNetworkObject()->GetLogName() : pParentVehicle->GetModelName() );
				DetachEntity(pParentVehicle);
			}
			return;
		}

		//Network clones can't break their attachments
		if(!pParentVehicle->IsNetworkClone())
		{			
			if(m_bAdjustingRopeLength)
			{
				if(pRopePosConstraintA)
				{
					pRopePosConstraintA->SetBreakable(false);
				}

				if(pRopePosConstraintB)
				{
					pRopePosConstraintB->SetBreakable(false);
				}
			}
			else
			{
				float fRegularStrength = Max(dfPickUpRopeConstraintAdditionalStrength + m_pTowedEntity->GetMass() * dfPickUpRopeConstraintStrengthMassMulti, pParentVehicle->GetMass() * dfPickUpRopeConstraintStrengthMassMulti);
				float fWeakStrength = Min(dfPickUpRopeConstraintAdditionalStrength + m_pTowedEntity->GetMass() * dfPickUpRopeConstraintStrengthMassMulti, pParentVehicle->GetMass() * dfPickUpRopeConstraintStrengthMassMulti);
				float fStrength = fRegularStrength;

				if(pParentVehicle->GetDriver() == NULL && !pParentVehicle->PopTypeIsMission())
				{
					fStrength = fWeakStrength;
				}

				if(pRopePosConstraintA)
				{
					pRopePosConstraintA->SetBreakable(!m_bForceDontDetachVehicle, fStrength);
				}

				if(pRopePosConstraintB)
				{
					pRopePosConstraintB->SetBreakable(!m_bForceDontDetachVehicle, fStrength);
				}
			}
		}

		// GTA V Hack: Just make the rope constraints unbreakable all the time for the magnet gadget.
		if(GetPickupType() == PICKUP_MAGNET)
		{
			if(pRopePosConstraintA)
			{
				pRopePosConstraintA->SetBreakable(false);
			}

			if(pRopePosConstraintB)
			{
				pRopePosConstraintB->SetBreakable(false);
			}
		}

		if(m_PropObject && m_PropObject->GetAttachmentExtension())
		{					
			m_PropObject->GetAttachmentExtension()->SetAttachOffset(m_vCurrentPickupPropPosition);
			m_PropObject->GetAttachmentExtension()->SetAttachQuat(m_qCurrentPickupPropRotation);					
			
			Matrix34 hookMatrix = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());
			Matrix34 towMatrix = MAT34V_TO_MATRIX34(m_pTowedEntity->GetMatrix());

			Matrix34 matAttachExt;
			matAttachExt.Identity();
			matAttachExt.FromQuaternion(m_PropObject->GetAttachmentExtension()->GetAttachQuat());
			matAttachExt.d = m_PropObject->GetAttachmentExtension()->GetAttachOffset();

			hookMatrix = matAttachExt;
			hookMatrix.Dot(towMatrix);				

			Matrix34 matMountA;
			Matrix34 matMountB;

			if(m_PropObject->GetSkeleton() && 		  
				GetPropRopeAttachBoneA() < m_PropObject->GetSkeleton()->GetBoneCount() &&
				GetPropRopeAttachBoneB() < m_PropObject->GetSkeleton()->GetBoneCount())
			{
				matMountA = m_PropObject->GetObjectMtx(GetPropRopeAttachBoneA());
				matMountB = m_PropObject->GetObjectMtx(GetPropRopeAttachBoneB());
			}
			else
			{
				matMountA.Identity();
				matMountB.Identity();
			}

			hookMatrix.Transform(matMountA.d);
			hookMatrix.Transform(matMountB.d);

			if(pRopePosConstraintA)
			{
				pRopePosConstraintA->SetWorldPosB(VECTOR3_TO_VEC3V(matMountA.d));
			}

			if(pRopePosConstraintB)
			{
				pRopePosConstraintB->SetWorldPosB(VECTOR3_TO_VEC3V(matMountB.d));
			}
		}
	}		
}


//////////////////////////////////////////////////////////////////////////
bank_float sfPickUpRopeConstraintFriction = 0.0f;
dev_float dfPickUpRopeConstraintDepthToBreak = PH_CONSTRAINT_ASSERT_DEPTH - 1.0f;

void CVehicleGadgetPickUpRope::ProcessPreComputeImpacts(CVehicle* pVehicle, phCachedContactIterator &impacts)
{
	if(m_Setup)
	{
		impacts.Reset();

		while(!impacts.AtEnd())
		{
			if(impacts.IsConstraint())
			{  
				//reduce the friction on the constraint.
				impacts.SetFriction(sfPickUpRopeConstraintFriction);

				if(NetworkInterface::IsGameInProgress() && m_pTowedEntity)
                {
					if(impacts.GetDepth() > dfPickUpRopeConstraintDepthToBreak)
					{
						if(NetworkUtils::IsNetworkCloneOrMigrating(m_pTowedEntity))
						{
							if(m_RopeInstanceA && m_RopeInstanceA->GetConstraint())
							{
								m_RopeInstanceA->GetConstraint()->SetBroken(true);
							}

							if(m_RopeInstanceB && m_RopeInstanceB->GetConstraint())
							{
								m_RopeInstanceB->GetConstraint()->SetBroken(true);
							}
							// An event will be sent to the owner of the attached entity requesting they detach in UpdateConstraintDistance
						}
						else
						{
							vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessPreComputeImpacts - Detaching entity because constraint depth too large. Parent vehicle: %s", pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : pVehicle->GetModelName() );
							DetachEntity(pVehicle);
						}
					}
				}
			}
			else
			{		
				// B*2027403: Allow the hook to fall through the cargobob if it's not colliding from the bottom. 
				// This prevents it from somehow getting into positions where it's stuck in the heli geometry and unusable.
				if(pVehicle->InheritsFromHeli() && static_cast<CHeli *>(pVehicle)->GetIsCargobob())
				{
					if(m_PropObject && impacts.GetMyInstance() == pVehicle->GetCurrentPhysicsInst() && impacts.GetOtherInstance() == m_PropObject->GetCurrentPhysicsInst())
					{
						Vec3V otherNormal;
						impacts.GetOtherNormal(otherNormal);

						float impactNormalOnVehUp = Dot(otherNormal, pVehicle->GetMatrix().c()).Getf();
						if(impactNormalOnVehUp > 0)
							impacts.DisableImpact();
					}
				}
			}

			++impacts;
		}

		impacts.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::GetPickupPropIdealPosition(CVehicle *pParent, Vector3 &vecPickupPropIdealPosition_Out)
{
	// work out where we would really like the hook
	Matrix34 matTemp;	
	pParent->GetGlobalMtx(m_iTowMountA, matTemp);
	Vector3 vLocalMountA = matTemp.d;
		
	pParent->GetGlobalMtx(m_iTowMountB, matTemp);
	Vector3 vLocalMountB = matTemp.d;

	Vector3 vMidMount = vLocalMountA + ((vLocalMountB - vLocalMountA) / 2.0f);
	Vector3 vPickupOffset = m_PropObject->GetObjectMtx(GetPickupAttachBone()).d - m_PropObject->GetObjectMtx(GetPropRopeAttachBoneA()).d;

	vecPickupPropIdealPosition_Out = vMidMount + vPickupOffset + Vector3(0.0f, 0.0f, -m_fRopeLength);
}



//////////////////////////////////////////////////////////////////////////
//static dev_float sfPickUpHookAngularDamping = 0.3f;
static dev_float sfAttachedVehicleAngularDamping = 2.5f;

void CVehicleGadgetPickUpRope::ProcessPhysics3(CVehicle* pParent, float fTimestep)
{
	CVehicleGadget::ProcessPhysics3( pParent, fTimestep );

	if(!m_PropObject)
		return;

	if(m_Setup)
	{		
		DampAttachedEntity(pParent);	

		// Reset damping status.
		m_bEnableDamping = true;
	}

	// B*2110065: Detach anything from the hook if we're a cargobob that has just gone into the water.
	if(pParent && pParent->pHandling && pParent->m_Buoyancy.GetSubmergedLevel() > pParent->pHandling->GetFlyingHandlingData()->m_fSubmergeLevelToPullHeliUnderwater)
	{
		if(!pParent->IsNetworkClone() && pParent->InheritsFromHeli() && static_cast<CHeli *>(pParent)->GetIsCargobob())
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessPhysics3 - Detaching entity because heli %s underwater.", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			DetachEntity(pParent);
		}
	}
}

bank_float bfPickUpEntityMinDimension = 0.5f;
bank_float bfPickUpEntityMaxDimension = 7.0f;
static dev_float sfPickUpEntityBBoxAttachAllowance	 = 0.25f;
static dev_float sfPickUpGlendaleBBoxAttachAllowance = 0.4f;
dev_float sfPickUpBoatContactNormalAllowance = 0.6f;


void CVehicleGadgetPickUpRope::ProcessControl(CVehicle* pParent)
{
	// Detach the attached vehicle if signaled.
	if(m_bDetachVehicle)
	{
		if(m_pTowedEntity)
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Detaching vehicle from %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
			ActuallyDetachVehicle( pParent );
		}

		m_bDetachVehicle = false;
		return;//if we've detached a vehicle, no point doing anything else
	}

	// We need these.
	if(!m_bRopesAndHookInitialized)
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Rope and hook not initialised, Parent %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
		return;
	}

	// And this.
	if(!m_PropObject)
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - No prop object. Parent vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
		return;
	}

	// Fix the pick-up prop/attached entity if the parent has been fixed (e.g. by a cutscene).
	if(pParent->GetIsAnyFixedFlagSet())
	{
		if(!m_PropObject->GetIsFixedByNetworkFlagSet() && pParent->GetIsFixedByNetworkFlagSet())
		{
			m_PropObject->SetFixedPhysics(true, true);
		}
		if(!m_PropObject->GetIsFixedUntilCollisionFlagSet() && pParent->GetIsFixedUntilCollisionFlagSet())
		{
			m_PropObject->SetIsFixedWaitingForCollision(true);
		}
		if(!m_PropObject->GetIsFixedFlagSet() && pParent->GetIsFixedFlagSet())
		{
			m_PropObject->SetFixedPhysics(true);
		}	
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Fixing pick-up prop entity because parent has been fixed (Fixed by %s). Parent vehicle: %s", 
			m_PropObject->GetIsFixedByNetworkFlagSet() ? "network" : "code/script", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );

		if(m_pTowedEntity && !m_pTowedEntity->GetIsAnyFixedFlagSet())
		{
			if(!m_pTowedEntity->GetIsFixedByNetworkFlagSet() && pParent->GetIsFixedByNetworkFlagSet())
			{
				m_pTowedEntity->SetFixedPhysics(true, true);
			}
			if(!m_pTowedEntity->GetIsFixedUntilCollisionFlagSet() && pParent->GetIsFixedUntilCollisionFlagSet())
			{
				m_pTowedEntity->SetIsFixedWaitingForCollision(true);
			}
			if(!m_pTowedEntity->GetIsFixedFlagSet() && pParent->GetIsFixedFlagSet())
			{
				m_pTowedEntity->SetFixedPhysics(true);
			}	

			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Fixing attached entity because parent has been fixed (Fixed by %s). Parent vehicle: %s", 
				m_PropObject->GetIsFixedByNetworkFlagSet() ? "network" : "code/script", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		}
	}
	else
	{
		// Be sure to reset fixed physics on the pick-up object/attached entity.
		if(m_PropObject && m_PropObject->GetIsAnyFixedFlagSet())
		{
			m_PropObject->SetFixedPhysics(false, true);
			m_PropObject->SetFixedPhysics(false, false);
			m_PropObject->SetIsFixedWaitingForCollision(false);
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Unfixing pick-up prop entity because parent has been unfixed. Parent vehicle: %s", 
				pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		}		

		if(m_pTowedEntity && m_pTowedEntity->GetIsAnyFixedFlagSet())
		{
			m_pTowedEntity->SetFixedPhysics(false, true);
			m_pTowedEntity->SetFixedPhysics(false, false);
			m_pTowedEntity->SetIsFixedWaitingForCollision(false);
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Unfixing attached entity because parent has been unfixed. Parent vehicle: %s", 
				pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		}
	}

	// Keep the attached entity from getting too far away and triggering constraint violation asserts.
	// Do this even if the physics inst/drawable hasn't yet been streamed in for the pickup-prop to prevent syncing issues where one machine has streamed it in and one hasn't.
	UpdateHookPosition(pParent);

	if(!(m_PropObject->GetCurrentPhysicsInst() && m_PropObject->GetSkeleton()))
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - No physics inst/skeleton for prop. Parent vehicle: %s", 
			pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		return;
	}

	// ghost vehicles are not allowed to pick up stuff (ghost vehicles appear in non-contact races)
	if (pParent && NetworkInterface::IsAGhostVehicle(*pParent))
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - pParent && NetworkInterface::IsAGhostVehicle(*pParent). Parent vehicle: %s", 
			pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		return;
	}

	//Make sure the hook is setup and the vehicle we are supposed to be attached to is still attached.
	if( !m_Setup || m_RopesAttachedVehiclePhysicsInst != pParent->GetCurrentPhysicsInst() )//check whether the vehicles physics inst has changed, if it has recreate the constraints
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Not setup. Doing ropes and stuff. Parent vehicle: %s", 
			pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		SetupRopeWithPickupProp(pParent);
	}

	if(m_Setup)
	{
		Assert(m_PropObject);
			
		HandleRopeVisibilityAndLength(pParent);

		UpdateAttachments(pParent);

		if(m_VehicleTowArmAudio && m_PropObject REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
		{
			m_VehicleTowArmAudio->SetHookPosition(GetPickUpPropMatrix().d);
		}

		if(m_pTowedEntity)
		{
			DoSmoothPickupTransition(pParent);

			// Check to see if the attached entity has detached itself from the ropes.
			phInst *towedPhysicsInst = m_pTowedEntity->GetCurrentPhysicsInst();
			if( m_RopeInstanceA && m_RopeInstanceB )
			{
				//make sure the tow vehicle is still attached
				if( ( m_RopeInstanceA->GetInstanceA() == towedPhysicsInst || 
					m_RopeInstanceA->GetInstanceB() == towedPhysicsInst ) &&
					( m_RopeInstanceB->GetInstanceA() == towedPhysicsInst || 
					m_RopeInstanceB->GetInstanceB() == towedPhysicsInst ) )
				{
					pParent->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;

					phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(towedPhysicsInst->GetArchetype());
					pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(sfAttachedVehicleAngularDamping, sfAttachedVehicleAngularDamping, sfAttachedVehicleAngularDamping));				
				}
				else
				{
					// last towed vehicle has detached itself, so make sure it' detached properly and start scanning again
					ActuallyDetachVehicle( pParent );
				}
			}
		}			

		ScanForPickup(pParent);

		if(m_RopeInstanceA)
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Rope A status: m_RopeInstanceA->GetInstanceA(): %p | m_RopeInstanceA->GetInstanceB(): %p | m_RopeInstanceA->GetAttachedTo(): %p | m_RopeInstanceA->GetLength(): %f | m_RopeInstanceA->GetWorldPositionA(): %f, %f, %f | m_RopeInstanceA->GetWorldPositionB: %f, %f, %f.  Parent vehicle: %s", 
				m_RopeInstanceA->GetInstanceA(),
				m_RopeInstanceA->GetInstanceB(),
				m_RopeInstanceA->GetAttachedTo(),
				m_RopeInstanceA->GetLength(),
				VEC3V_ARGS(m_RopeInstanceA->GetWorldPositionA()),
				VEC3V_ARGS(m_RopeInstanceA->GetWorldPositionB()),
				pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName()
				);

			if(m_RopeInstanceA->GetConstraint())
			{
				vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Rope A constraint status: m_RopeInstanceA->GetConstraint()->IsBreakable(): %i | m_RopeInstanceA->GetConstraint()->IsBroken(): %i | m_RopeInstanceA->GetConstraint()->IsAtLimit(): %i | m_RopeInstanceA->GetConstraint()->GetInstanceA(): %p | m_RopeInstanceA->GetConstraint()->GetInstanceB(): %p. Parent Vehicle: %s",
					m_RopeInstanceA->GetConstraint()->IsBreakable(),
					m_RopeInstanceA->GetConstraint()->IsBroken(),
					m_RopeInstanceA->GetConstraint()->IsAtLimit(),
					m_RopeInstanceA->GetConstraint()->GetInstanceA(),
					m_RopeInstanceA->GetConstraint()->GetInstanceB(),
					pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName()
					);
			}
		}
		else
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - No Rope A instance. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() );
		}
		
		
		if(m_RopeInstanceB)
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Rope B status: m_RopeInstanceB->GetInstanceA(): %p | m_RopeInstanceB->GetInstanceB(): %p | m_RopeInstanceB->GetAttachedTo(): %p | m_RopeInstanceB->GetLength(): %f | m_RopeInstanceB->GetWorldPositionA(): %f, %f, %f | m_RopeInstanceB->GetWorldPositionB: %f, %f, %f, Parent Vehicle: %s",
				m_RopeInstanceB->GetInstanceA(),
				m_RopeInstanceB->GetInstanceB(),
				m_RopeInstanceB->GetAttachedTo(),
				m_RopeInstanceB->GetLength(),
				VEC3V_ARGS(m_RopeInstanceB->GetWorldPositionA()),
				VEC3V_ARGS(m_RopeInstanceB->GetWorldPositionB()),
				pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName()
				);

			if(m_RopeInstanceB->GetConstraint())
			{
				vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - Rope B constraint status: m_RopeInstanceB->GetConstraint()->IsBreakable(): %i | m_RopeInstanceB->GetConstraint()->IsBroken(): %i | m_RopeInstanceB->GetConstraint()->IsAtLimit(): %i | m_RopeInstanceB->GetConstraint()->GetInstanceA(): %p | m_RopeInstanceB->GetConstraint()->GetInstanceB(): %p Parent Vehicle: %s",
					m_RopeInstanceB->GetConstraint()->IsBreakable(),
					m_RopeInstanceB->GetConstraint()->IsBroken(),
					m_RopeInstanceB->GetConstraint()->IsAtLimit(),
					m_RopeInstanceB->GetConstraint()->GetInstanceA(),
					m_RopeInstanceB->GetConstraint()->GetInstanceB(),
					pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName()
					);
			}
		}
		else
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ProcessControl - No Rope B instance. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRope::Trigger()
{
	const CPhysical *pAttachedEntity = GetAttachedEntity();

	if(pAttachedEntity && pAttachedEntity->IsNetworkClone())
	{
		// send an event to the owner of the attached entity requesting they detach
		CRequestDetachmentEvent::Trigger(*pAttachedEntity);
	}
	else
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::Trigger - Detaching entity.");
		DetachEntity();
	}
}

//////////////////////////////////////////////////////////////////////////

 Matrix34 CVehicleGadgetPickUpRope::GetPickUpPropMatrix()
 { 
	 return MAT34V_TO_MATRIX34(m_PropObject->GetMatrix()); 
 }

//////////////////////////////////////////////////////////////////////////

 void CVehicleGadgetPickUpRope::SetupRopeWithPickupProp(CVehicle *pParent)
 {
	 vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::SetupRopeWithPickupProp - Setting up. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 

	 m_PropObject->ActivatePhysics();
	 m_PropObject->SetMass(GetPropObjectMass());
	 if(!m_bHoldsDrawableRef)
	 {
		 m_bHoldsDrawableRef = true;
		 m_PropObject->GetBaseModelInfo()->BumpDrawableRefCount();
	 }

	 AttachRopesToObject( pParent, m_PropObject );
 }

//////////////////////////////////////////////////////////////////////////
 
 void CVehicleGadgetPickUpRope::HandleRopeVisibilityAndLength(CVehicle *pParent)
 {
	 // Hide ropes and hook if the vehicle's hidden by script
	 bool bShouldRopesBeVisible = true;
	
	 // Check different modules for visibility settings and make sure those are also set for the prop object and ropes.
	 const int nNumModulesToCheck = 2;
	 eIsVisibleModule nModulesToCheck[nNumModulesToCheck] = {
		 SETISVISIBLE_MODULE_SCRIPT,
		 SETISVISIBLE_MODULE_NETWORK
	 };

	 for(int nModule = 0; nModule < nNumModulesToCheck; nModule++)
	 {
		 bool bParentVisible = pParent->GetIsVisibleForModule(nModulesToCheck[nModule]);
		 
		 // If either parent or prop are not visible, make sure the ropes follow suite.		
		 bShouldRopesBeVisible = bParentVisible && m_PropObject->GetIsVisibleForModule(nModulesToCheck[nModule]);
		
		 if( m_PropObject && m_PropObject->GetIsVisibleForModule(nModulesToCheck[nModule]) != bParentVisible )
			 m_PropObject->SetIsVisibleForModule(nModulesToCheck[nModule], bParentVisible, true);
		
		if( m_pTowedEntity && m_pTowedEntity->GetIsVisibleForModule(nModulesToCheck[nModule]) != bParentVisible )
			 m_pTowedEntity->SetIsVisibleForModule(nModulesToCheck[nModule], bParentVisible, true);		
	 }

	 if( m_RopeInstanceA )
		 m_RopeInstanceA->SetDrawEnabled(bShouldRopesBeVisible);

	 if( m_RopeInstanceB )
		 m_RopeInstanceB->SetDrawEnabled(bShouldRopesBeVisible);

	 if(!bShouldRopesBeVisible)
	 {
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::HandleRopeVisibilityAndLength - Ropes are being made invisible. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
	 }

	 // Hide hook if player is in the attach vehicle and in first persion view
	 if(m_PropObject && m_PropObject->GetModelId() == MI_PROP_HOOK)
	 {
		 if(GetAttachedVehicle() && GetAttachedVehicle()->ContainsLocalPlayer() && camInterface::GetCinematicDirector().IsRenderingCinematicCameraInsideVehicle())
		 {
			 m_PropObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, false, true);
		 }
		 else if(!m_PropObject->GetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT))
		 {
			 m_PropObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, true, true);
		 }
	 }

	 //Adjust rope length for tow arm position.	
	 float fDesiredRopeLength = m_pTowedEntity ? m_fAttachedRopeLength : m_fDetachedRopeLength;

	 // Use a longer rope when carrying boats because boats be big.
	 if(m_pTowedEntity && m_pTowedEntity->GetIsTypeVehicle() && static_cast<CVehicle *>(m_pTowedEntity.Get())->GetVehicleType() == VEHICLE_TYPE_BOAT)
	 {
		 fDesiredRopeLength += 0.7f;
	 }
	 
	 if(fDesiredRopeLength != m_fPreviousDesiredRopeLength)
		 m_bAdjustingRopeLength = true;

	 m_fPreviousDesiredRopeLength = fDesiredRopeLength;

	 //m_fRopeLength = Clamp(fDesiredRopeLength, m_fRopeLength - m_fPickUpRopeLengthChangeRate * fwTimer::GetRagePhysicsUpdateTimeStep(), m_fRopeLength + m_fPickUpRopeLengthChangeRate * fwTimer::GetRagePhysicsUpdateTimeStep());
	
	 if(m_RopeInstanceA && m_RopeInstanceB)
	 {
#if __BANK
		 if(ms_bShowDebug)
		 {
			 char text[256];
			 sprintf(text, "Adjusting rope length: %i | fDesiredRopeLength: %f | m_fPreviousDesiredRopeLength: %f | m_RopeInstanceA->GetLength(): %f", m_bAdjustingRopeLength, fDesiredRopeLength, m_fPreviousDesiredRopeLength, m_RopeInstanceA->GetLength());
			 grcDebugDraw::Text(VEC3V_TO_VECTOR3(pParent->GetMatrix().d()), Color_white, -200, 15 + 15 * CPhysics::GetCurrentTimeSlice(), text);
		 }
#endif

		 if(m_bAdjustingRopeLength)
		 {
			 float fRopeLength = m_RopeInstanceA->GetLength();
			 if(fabs(fRopeLength - fDesiredRopeLength) > dfPickUpRopeLengthWindingThreshold)
			 {	
				if(fRopeLength < fDesiredRopeLength)
				{
					m_RopeInstanceA->StopWindingFront();
					m_RopeInstanceB->StopWindingFront();		 

					m_RopeInstanceA->StartUnwindingFront(m_fPickUpRopeLengthChangeRate); // start unwinding has to be called when ever force length is called			
					m_RopeInstanceB->StartUnwindingFront(m_fPickUpRopeLengthChangeRate);	
				}
				else if(fRopeLength > fDesiredRopeLength)
				{
					m_RopeInstanceA->StopUnwindingFront();
					m_RopeInstanceB->StopUnwindingFront();

					m_RopeInstanceA->StartWindingFront(m_fPickUpRopeLengthChangeRate); // start unwinding has to be called when ever force length is called
					m_RopeInstanceB->StartWindingFront(m_fPickUpRopeLengthChangeRate);
				}
			
			 }
			 else
			 {			 
				 m_RopeInstanceA->StopUnwindingFront();
				 m_RopeInstanceB->StopUnwindingFront();
				 m_RopeInstanceA->StopWindingFront();
				 m_RopeInstanceB->StopWindingFront();	

				 m_bAdjustingRopeLength	 = false;
			 }

	#if GTA_REPLAY
			 if(CReplayMgr::ShouldRecord())
			 {
				 ropeInstance* r = m_RopeInstanceA;
				 CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					 CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					 CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r), NULL, false);

				 r = m_RopeInstanceB;
				 CReplayMgr::RecordPersistantFx<CPacketRopeWinding>(
					 CPacketRopeWinding(r->GetLengthChangeRate(), r->GetIsWindingFront(), r->GetIsUnwindingFront(), r->GetIsUnwindingBack()),
					 CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)r), NULL, false);
			 }
	#endif

		 }

		  m_fRopeLength = m_RopeInstanceA->GetLength();
	 }
 }

 //////////////////////////////////////////////////////////////////////////

 void CVehicleGadgetPickUpRope::DoSmoothPickupTransition(CVehicle *pParent)
 {
	if(m_VehicleTowArmAudio REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
	{
		m_VehicleTowArmAudio->SetEntityIsAttached(true);

		f32 linkAngle = 0.0f;

		if(pParent && m_pTowedEntity)
		{
			linkAngle = Dot(pParent->GetTransform().GetUp(), m_pTowedEntity->GetTransform().GetUp()).Getf();
		}
		m_VehicleTowArmAudio->SetTowedVehicleAngle(linkAngle);
	}


	//if(!UseIdealAttachPosition())
	// 	return;

	// Lerp the hook to the desired position and rotation based on the speed the hook is currently moving
	Vector3 hookVelocity = m_pTowedEntity->GetLocalSpeed(m_vCurrentPickupPropPosition);	
	if(m_fPickupPositionTransitionTime < 1.0f)
	{   
		float hookSpeed = hookVelocity.Mag2();
		Vector3 vecDiff( m_vDesiredPickupPropPosition - m_vStartPickupPropPosition );
		static dev_float sfSpeedModifier = 0.25f;
		
		m_fPickupPositionTransitionTime += (hookSpeed*sfSpeedModifier) * fwTimer::GetRagePhysicsUpdateTimeStep();		
		if(m_fPickupPositionTransitionTime > 1.0f)
			m_fPickupPositionTransitionTime = 1.0f;

		m_vCurrentPickupPropPosition = m_vStartPickupPropPosition + (vecDiff*m_fPickupPositionTransitionTime);
		
		m_qCurrentPickupPropRotation.Lerp(m_fPickupPositionTransitionTime, m_qStartPickupPropRotation, m_qDesiredPickupPropRotation);
		m_qCurrentPickupPropRotation.Normalize();
	}
 }

 //////////////////////////////////////////////////////////////////////////

 bool	CVehicleGadgetPickUpRope::ms_bShowDebug = false;
 bool	CVehicleGadgetPickUpRope::ms_logCallStack = false;
 bool	CVehicleGadgetPickUpRope::ms_bEnableDamping = true;
 float	CVehicleGadgetPickUpRope::ms_fSpringConstant = -30.0f;
 float	CVehicleGadgetPickUpRope::ms_fDampingConstant = 3.0f;	
 float	CVehicleGadgetPickUpRope::ms_fDampingFactor = 0.8f;
 float	CVehicleGadgetPickUpRope::ms_fMaxDampAccel = 10.0f;

 void CVehicleGadgetPickUpRope::DampAttachedEntity(CVehicle *pParent)
 {
	 if(!ms_bEnableDamping || !m_bEnableDamping)
		 return;
	
	CPhysical *pAttachedEntity = m_pTowedEntity ? static_cast<CPhysical *>(m_pTowedEntity) : static_cast<CPhysical *>(m_PropObject);

	if(pAttachedEntity && pAttachedEntity->IsNetworkClone() && pAttachedEntity == m_PropObject)
		return;

	// Don't damp until the vehicle is in position.
	if(pAttachedEntity == m_pTowedEntity && m_fPickupPositionTransitionTime < 1.0f)
		return;

	if( pAttachedEntity && 
		( !pAttachedEntity->GetCurrentPhysicsInst() ||
		  !pAttachedEntity->GetCurrentPhysicsInst()->IsInLevel() ) )
	{
		return;
	}

	if(m_VehicleTowArmAudio REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
		m_VehicleTowArmAudio->SetEntityIsAttached(false);

	// Try to prevent the prop from sticking to the tow truck bed and also reduce how much it swings about
	Matrix34 matPropObj = MAT34V_TO_MATRIX34(m_PropObject->GetCurrentPhysicsInst()->GetMatrix());
	Vector3 propOffset = m_PropObject->GetObjectMtx(GetPickupAttachBone()).d;
	matPropObj.Transform3x3(propOffset);
	Vector3 propPosition = matPropObj.d + propOffset;//GetPickUpPropMatrix().d;//VEC3V_TO_VECTOR3(pAttachedEntity->GetTransform().GetPosition());
	
	Vector3 vecSpringPos;
	GetPickupPropIdealPosition( pParent, vecSpringPos );

#if __BANK
	if(ms_bShowDebug)
	{
		grcDebugDraw::Sphere(vecSpringPos, 0.5f, Color_orange, false, -1);
		grcDebugDraw::Sphere(propPosition, 0.5f, Color_green, false, -1);
		char text[56];
		sprintf(text, "Rope length: %f", m_fRopeLength);
		grcDebugDraw::Text(propPosition, Color_white, 0, 0, text, true, -1);
	}
#endif

	Vector3 distance = propPosition - vecSpringPos;

	// Work out the torque to apply with some damping.
	Vector3 dampingToApply(VEC3_ZERO);
	if(fwTimer::GetRagePhysicsUpdateTimeStep() > 0.0f)
	{
		dampingToApply = ((distance-m_vLastFramesDistance) / fwTimer::GetRagePhysicsUpdateTimeStep());
	}

	Vector3 forceToApply = ((distance * ms_fSpringConstant) - (dampingToApply * ms_fDampingConstant));// scale the torque by mass
	forceToApply *= m_fDampingMultiplier;

	// clamp the maximum acceleration to apply to the attached entity
	float forceMag = forceToApply.Mag();	
	if(forceMag > ms_fMaxDampAccel)
	{
		float forceMult = ms_fMaxDampAccel / forceMag;
		forceToApply *= forceMult;//ms_fDampingFactor;
	}

	// Calculate force from acceleration.
	forceToApply *= pAttachedEntity->GetMass();

	// Keep track of the previous frames difference so we can do some damping
	m_vLastFramesDistance = distance;

	Matrix34 matAttachedEnt = MAT34V_TO_MATRIX34(pAttachedEntity->GetCurrentPhysicsInst()->GetMatrix());
	Vector3 vForceOffset = propPosition - matAttachedEnt.d;
	
	// Clamp the force offset so it remains within the bounding sphere to stop ApplyForce from complaining about it.
	vForceOffset.ClampMag(0, pAttachedEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid());

	pAttachedEntity->ApplyForce(forceToApply, vForceOffset);

	Vector3 vForceDir = forceToApply;
	vForceDir.Normalize();

#if __BANK
	if(ms_bShowDebug)
	{
		grcDebugDraw::Line(propPosition, propPosition + vForceDir * 2, Color_red, Color_red, -1);
	}
#endif

	// Apply rotational damping to prevent constant spinning
	if(pAttachedEntity->GetCurrentPhysicsInst())
	{
		phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(pAttachedEntity->GetCurrentPhysicsInst()->GetArchetype());
		pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(sfHookAngularDamping, sfHookAngularDamping, sfHookAngularDamping));		
		pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(0, 0, FLT_MAX));
	}
 }

//////////////////////////////////////////////////////////////////////////

struct PickupRopeEntitySearch
{
	int nNumEntitiesInRange;
	CPhysical *pInRangeEntities[32];
	int nHitComponents[32];
	Vector3 vHitPositions[32];
	Vector3 vHitNormals[32];
	phMaterialMgr::Id nHitMaterials[32];
	Vector3 vTestStart;
	Vector3 vSphereOffset;
	float fSearchRadiusSq;
	int nClosestEnt;
	float fSmallestDist;
	CVehicle *pParent;
	CObject *pPropObject;
};

bool EntitySearchCB(CEntity *pEntity, void *data)
{
	PickupRopeEntitySearch *pData = reinterpret_cast<PickupRopeEntitySearch *>(data);
	
	if(pEntity == pData->pParent || pEntity == pData->pPropObject)
		return true;

	if(pData->nNumEntitiesInRange >= 32)
		return false;
	
	// Need to do a probe on the entity to get information about the closest point on it.
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<1> probeResults;
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	probeDesc.SetStartAndEnd(pData->vTestStart,  VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));	
	probeDesc.SetIncludeEntity(pEntity);
	probeDesc.SetExcludeEntities(NULL, 0);
	probeDesc.SetResultsStructure(&probeResults);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		// GTAV - B*2745483 - Do the occulsion test after we have found the hit position on the object so that
		// it doesn't fail to pick up objects that are placed slightly in the ground

		// Make sure there isn't something between the magnet and the entity.
		WorldProbe::CShapeTestProbeDesc occlusionDesc;
		WorldProbe::CShapeTestFixedResults<1> occlusionResults;
		occlusionDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		occlusionDesc.SetStartAndEnd(pData->vTestStart, probeResults[0].GetHitPosition());	
		const CEntity *ppExcludeEntities[2] = {pData->pParent, pData->pPropObject};
		occlusionDesc.SetExcludeEntities(ppExcludeEntities, 2);
		occlusionDesc.SetResultsStructure(&occlusionResults);
		//probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);

		if(!WorldProbe::GetShapeTestManager()->SubmitTest(occlusionDesc))
		{
			if(((pData->vTestStart + pData->vSphereOffset) - probeResults[0].GetHitPosition()).Mag2() > pData->fSearchRadiusSq)
				return true;

			pData->pInRangeEntities[pData->nNumEntitiesInRange] = static_cast<CPhysical *>(pEntity);

			pData->nHitComponents[pData->nNumEntitiesInRange] = probeResults[0].GetHitComponent();
			pData->vHitPositions[pData->nNumEntitiesInRange] = probeResults[0].GetHitPosition();
			pData->vHitNormals[pData->nNumEntitiesInRange] = VEC3V_TO_VECTOR3(probeResults[0].GetNormal());
			pData->nHitMaterials[pData->nNumEntitiesInRange] = probeResults[0].GetMaterialId();

			float fDist = (pData->vTestStart - probeResults[0].GetHitPosition()).Mag();

			// Prioritise vehicles over objects. 
			// Even if an object is the actual closest entity, if there's an in-range vehicle, the vehicle is chosen over the object.
		
			bool bIsClosest = fDist < pData->fSmallestDist;

			if(pData->nNumEntitiesInRange > 0)
			{
				// This vehicle isn't closer than something else so see if the something else is an object and if so, just say it's the closest thing.
				if(!bIsClosest && pData->pInRangeEntities[pData->nClosestEnt]->GetIsTypeObject() && pEntity->GetIsTypeVehicle())
				{
					bIsClosest = true;
				}

				// This object purports to be the closest thing, but there's already a vehicle that is "closest" so ignore it.
				if(bIsClosest && pData->pInRangeEntities[pData->nClosestEnt]->GetIsTypeVehicle() && pEntity->GetIsTypeObject())
				{
					bIsClosest = false;
				}
			}

			if(bIsClosest)
			{
				pData->nClosestEnt = pData->nNumEntitiesInRange;
				pData->fSmallestDist = fDist;
			}

			pData->nNumEntitiesInRange++;			

			//grcDebugDraw::Line(pData->vTestStart, probeResults[0].GetHitPosition(), Color_green, Color_green);
		}
	}

	return true;
}

 void CVehicleGadgetPickUpRope::ScanForPickup(CVehicle *pParent)
 {
	 // Only scan for trailer pickup if the driver is a player
	 if( pParent->GetDriver() == NULL )
	 {
		 vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ScanForPickup - Not scanning; no driver. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		 return;
	 }

	 if(m_uIgnoreAttachTimer > 0)
	 {
		 u32 uTimeStep = fwTimer::GetTimeStepInMilliseconds();

		 // Be careful not to wrap round
		 m_uIgnoreAttachTimer = uTimeStep > m_uIgnoreAttachTimer ? 0 : m_uIgnoreAttachTimer - uTimeStep;

		 return;
	 }

	 // If parent is asleep, skip test
	 // We would be awake if anything was colliding with us or if we were moving
	 if(pParent->IsAsleep())
	 {
		 vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRope::ScanForPickup - Not scanning; vehicle is asleep. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		 return;
	 }

	 // If towing is disabled then don't try to pickup anything
	 if (pParent->m_nVehicleFlags.bDisableTowing)
	 {
		 return;
	 }
	
	 /*
	 // don't try to pick up anything if the tow truck is a network clone
	 if (pParent->IsNetworkClone())
	 {
		 return;
	 }
	 */

	 // Lookup hook position
	 //get the position of the hook point
	 Matrix34 pickupPointPositionMatrix;
	 m_PropObject->GetGlobalMtx(GetPickupAttachBone(), pickupPointPositionMatrix);	

#if __BANK
	 if(ms_bShowDebug)
	 {
		 if(m_PropObject)
			 grcDebugDraw::Sphere(pickupPointPositionMatrix.d + GetEntitySearchCentreOffset(), GetEntitySearchRadius(), Color_red, false, -1);
	 }
#endif

	 // Search for pick-upable entities in range.
	 PickupRopeEntitySearch entitySearchData;
	 entitySearchData.nNumEntitiesInRange = 0;
	 entitySearchData.vTestStart = pickupPointPositionMatrix.d;
	 entitySearchData.vSphereOffset = GetEntitySearchCentreOffset();
	 entitySearchData.fSearchRadiusSq = GetEntitySearchRadius() * GetEntitySearchRadius();	 
	 entitySearchData.fSmallestDist = FLT_MAX;
	 entitySearchData.nClosestEnt = 0;
	 entitySearchData.pParent = pParent;
	 entitySearchData.pPropObject = m_PropObject;

	 spdSphere testSphere(VECTOR3_TO_VEC3V(pickupPointPositionMatrix.d), ScalarV(GetEntitySearchRadius()));
	 fwIsSphereIntersecting searchSphere(testSphere);
	 
	 u32 nEntTypeMask = 0;
	 if(GetAffectsVehicles())
		 nEntTypeMask |= ENTITY_TYPE_MASK_VEHICLE;
	 if(GetAffectsObjects())
		 nEntTypeMask |= ENTITY_TYPE_MASK_OBJECT;

	 CGameWorld::ForAllEntitiesIntersecting(&searchSphere, EntitySearchCB, (void*)&entitySearchData,
		 nEntTypeMask, (SEARCH_LOCATION_EXTERIORS),
		 SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_NONE);
	 
	 for(int nEntIndex = 0; nEntIndex < entitySearchData.nNumEntitiesInRange; nEntIndex++)
	 {
		 // Something might have happened during the previous iteration that caused it to stop needing to scan for pick ups.
		 if(!ShouldBeScanningForPickup())
			 break;

		 // Lookup the CEntity
		 CPhysical *pEntity = entitySearchData.pInRangeEntities[nEntIndex];

		 if (pEntity && NetworkInterface::AreInteractionsDisabledInMP(*pParent, *pEntity))
			 continue;

		 if( pEntity->GetPickupByCargobobDisabled() &&
			 pParent->InheritsFromHeli() &&
			 !static_cast< CHeli* >( pParent )->GetCanPickupEntitiesThatHavePickupDisabled() )
		 {
			 continue;
		 }

		 if(pEntity && pEntity->GetIsTypeVehicle())
		 {
			 CVehicle *pVehicle = static_cast<CVehicle*>(pEntity);

			 fwAnimDirectorComponentSyncedScene* pComponent = pVehicle->GetAnimDirector() ? static_cast<fwAnimDirectorComponentSyncedScene*>(pVehicle->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeSyncedScene)) : NULL;
			 if(pVehicle->m_nVehicleFlags.bAutomaticallyAttaches && !pVehicle->m_nVehicleFlags.bDisableTowing && (!pComponent || !pComponent->IsPlayingSyncedScene()) && 
				 (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER || pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT || pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE || pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR) &&
				 pVehicle->GetVehicleModelInfo() && !pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CANNOT_BE_PICKUP_BY_CARGOBOB))//check whether this vehicle should be able to automatically attach
			 {
				 if(pParent->GetAttachParent() != pVehicle)// Don't allow the tow truck to be attached to something it is already attached to.
				 {
					ProcessInRangeVehicle(pParent, pVehicle, entitySearchData.vHitPositions[nEntIndex], entitySearchData.vHitNormals[nEntIndex], entitySearchData.nHitComponents[nEntIndex], entitySearchData.nHitMaterials[nEntIndex], nEntIndex == entitySearchData.nClosestEnt);
				 }
			 }
		 }

		 bool bIsRootBrokenOff = pEntity && pEntity->GetFragInst() && pEntity->GetFragInst()->GetCacheEntry() && pEntity->GetFragInst()->GetCacheEntry()->IsGroupBroken(0);
		 bool bIsMoveable = pEntity && pEntity->GetFragInst() && pEntity->GetFragInst()->GetTypePhysics() && pEntity->GetFragInst()->GetTypePhysics()->GetMinMoveForce() == 0.0f;
		 if(pEntity && pEntity->GetIsTypeObject() && !pEntity->GetIsAnyFixedFlagSet() && !pEntity->GetIsFixedRoot() && !bIsRootBrokenOff && bIsMoveable && !((CObject *)pEntity)->m_nObjectFlags.bIsPickUp)
		 {
			 CObject *pObject = static_cast<CObject*>(pEntity);
			
			 const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(pObject->GetBaseModelInfo()->GetModelNameHash());
			 bool bCanBePickedUp = pTuning == NULL || pTuning->GetCanBePickedUpByCargobob();
	
			 if(bCanBePickedUp)
			 {
				 if(pParent->GetAttachParent() != pObject)// Don't allow the tow truck to be attached to something it is already attached to.
				 {
					ProcessInRangeObject(pParent, pObject, entitySearchData.vHitPositions[nEntIndex], entitySearchData.nHitComponents[nEntIndex], entitySearchData.nHitMaterials[nEntIndex], nEntIndex == entitySearchData.nClosestEnt);
				 }
			 }
		 }			
	 }

	 if(entitySearchData.nNumEntitiesInRange == 0)
		 ProcessNothingInRange(pParent);
 }

//////////////////////////////////////////////////////////////////////////

 bool CVehicleGadgetPickUpRope::CheckObjectDimensionsForPickup(CObject *pObject)
 {
	Vector3 vObjectExtent = pObject->GetBoundingBoxMax() - pObject->GetBoundingBoxMin();
	return vObjectExtent.x >= bfPickUpEntityMinDimension && vObjectExtent.x <= bfPickUpEntityMaxDimension &&
		 vObjectExtent.y >= bfPickUpEntityMinDimension && vObjectExtent.y <= bfPickUpEntityMaxDimension &&
		 vObjectExtent.z >= bfPickUpEntityMinDimension && vObjectExtent.z <= bfPickUpEntityMaxDimension;
 }

//////////////////////////////////////////////////////////////////////////

 CVehicleGadgetPickUpRopeWithHook::CVehicleGadgetPickUpRopeWithHook(s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex) :
	 CVehicleGadgetPickUpRope(iTowMountABoneIndex, iTowMountBBoneIndex)
 {
	 m_bEnsurePickupEntityUpright = true;
 }

//////////////////////////////////////////////////////////////////////////

 void CVehicleGadgetPickUpRopeWithHook::ProcessInRangeVehicle(CVehicle* pParent, CVehicle *pVehicle, Vector3 vPosition, Vector3 vNormal, int nComponent, phMaterialMgr::Id UNUSED_PARAM(nMaterial), bool UNUSED_PARAM(bIsClosest))
 {	 
	 bool bAttach = false;

	 if(NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
	 {	
		 // we can't attach to a network clone, we need control of it to update its attachment state
		 // try and request control of it
		 if(pVehicle->IsNetworkClone() && !pParent->IsNetworkClone())
		 {
			 CRequestControlEvent::Trigger(pVehicle->GetNetworkObject() NOTFINAL_ONLY(, "CVehicleGadgetPickUpRopeWithHook::ProcessInRangeVehicle 1"));
		 }

		 return;
	 }

	 if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER || pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE)
	 {
		 if( !pVehicle->IsUpsideDown() )
		 {
			 // Just attach to the vehicle if we are without a percentage amount of the front or back of the car
			 Matrix34 matVehicle = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
			 Vector3 hitPosition = vPosition;

			 matVehicle.UnTransform(hitPosition);

			 float fCentroidOffsetZ = pVehicle->GetPhysArch()->GetBound()->GetCentroidOffset().GetZf();

			 float fBoundingBoxHeightAllowance = pVehicle->GetBoundingBoxMax().z * (1.0f - sfPickUpEntityBBoxAttachAllowance);
 			 if(pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CARGOBOB_HOOK_UP_CHASSIS))
			 {
				fBoundingBoxHeightAllowance = pVehicle->GetChassisBoundMax().z * (1.0f - sfPickUpEntityBBoxAttachAllowance);
			 }
			 float fMaxAttachHeight = pVehicle->GetBoundingBoxMax().z;
			 if(pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE)
			 {
				 fBoundingBoxHeightAllowance = cvPickUpSubmarineOffset.z;
			 }

			 if( pVehicle->GetVehicleModelInfo() )
			 {
				 u32 modelNameHash = pVehicle->GetVehicleModelInfo()->GetModelNameHash();

				 if( modelNameHash == MI_CAR_PANTO.GetName().GetHash() )
				 {
					 fBoundingBoxHeightAllowance -= fCentroidOffsetZ;
				 }

				 if( modelNameHash == MI_CAR_GLENDALE.GetName().GetHash() || 
					 modelNameHash == MI_CAR_PIGALLE.GetName().GetHash() ||
					 modelNameHash == MI_CAR_OMNIS.GetName().GetHash() || 
					 modelNameHash == MI_CAR_INFERNUS2.GetName().GetHash() ||
					 modelNameHash == MI_CAR_TECHNICAL3.GetName().GetHash() )
				 {
					 fBoundingBoxHeightAllowance = sfPickUpGlendaleBBoxAttachAllowance;
				 }

				 if( modelNameHash == MI_TRAILER_TRAILERLARGE.GetName().GetHash() )
				 { 
					 return;
				 }

                 if( modelNameHash == MID_PEYOTE3 )
                 {
                     m_fExtraBoundAttachAllowance = 0.5f;
                 }
			 }

			fBoundingBoxHeightAllowance -= m_fExtraBoundAttachAllowance;
			fMaxAttachHeight += m_fExtraBoundAttachAllowance;

			 if( hitPosition.z > fBoundingBoxHeightAllowance && hitPosition.z < fMaxAttachHeight)
			 {  
				 bAttach = true;
			 }
		 }
	 }
	 else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
	 {
		 Matrix34 matVehicle = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
		 Vector3 hitNormal = vNormal;
		 matVehicle.UnTransform3x3(hitNormal);
		 if(hitNormal.z > sfPickUpBoatContactNormalAllowance) // do not attach on bottom of the boat
		 {
			  bAttach = true;	 
		 }
	 }
	 else // pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE
	 {
		  bAttach = true;
	 }

	 if( pVehicle == m_excludeEntity.GetEntity())
	 {
		 bAttach = false;
	 }

	 if(bAttach)
	 {	
		 AttachEntityToPickUpProp( pParent, pVehicle, vPosition, nComponent );		 		
	 }
 }

//////////////////////////////////////////////////////////////////////////

 void CVehicleGadgetPickUpRopeWithHook::ProcessInRangeObject(CVehicle* pParent, CObject *pObject, Vector3 vPosition, int nComponent, phMaterialMgr::Id UNUSED_PARAM(nMaterial), bool UNUSED_PARAM(bIsClosest))
 {
	 if(NetworkUtils::IsNetworkCloneOrMigrating(pObject))
	 {	
		 // we can't attach to a network clone, we need control of it to update its attachment state
		 // try and request control of it
		 if(pObject->IsNetworkClone())
		 {
			 CRequestControlEvent::Trigger(pObject->GetNetworkObject() NOTFINAL_ONLY(, "CVehicleGadgetPickUpRopeWithHook::ProcessInRangeObject 1"));
		 }

		 return;
	 }

	if(CheckObjectDimensionsForPickup(pObject))
	{
		if( pObject != m_excludeEntity.GetEntity() )
		{
			AttachEntityToPickUpProp( pParent, pObject, vPosition, nComponent );			
		}
	}
 }

 bool CVehicleGadgetPickUpRopeWithMagnet::ms_bOverrideParameters = false;

 float	CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetStrength = 1.0f;	
 float	CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetFalloff = -0.5f;

 float	CVehicleGadgetPickUpRopeWithMagnet::ms_fReducedMagnetStrength = 0.75f;
 float	CVehicleGadgetPickUpRopeWithMagnet::ms_fReducedMagnetFalloff = -0.5f;

 float	CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetPullStrength = 1.0f;
 float	CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetPullRopeLength = 3.0f;

 float	CVehicleGadgetPickUpRopeWithMagnet::ms_fMagnetEffectRadius = 20.0f;

//////////////////////////////////////////////////////////////////////////

CVehicleGadgetPickUpRopeWithMagnet::CVehicleGadgetPickUpRopeWithMagnet(s32 iTowMountABoneIndex, s32 iTowMountBBoneIndex) :
	CVehicleGadgetPickUpRope(iTowMountABoneIndex, iTowMountBBoneIndex),
	m_bFullyDisabled(false),
	m_bActive(false),
	m_nMode(VEH_PICK_UP_MAGNET_AMBIENT_MODE),
	m_bAffectsVehicles(true),
	m_bAffectsObjects(true),
	m_pAttractEntity(NULL),		
	m_fMagnetStrength(ms_fMagnetStrength),
	m_fMagnetFalloff(ms_fMagnetFalloff),
	m_fReducedMagnetStrength(ms_fReducedMagnetStrength),
	m_fReducedMagnetFalloff(ms_fReducedMagnetFalloff),
	m_fMagnetPullStrength(ms_fMagnetPullStrength),
	m_fMagnetPullRopeLength(ms_fMagnetPullRopeLength),	
	m_fMagnetEffectRadius(ms_fMagnetEffectRadius),
	m_pNetMagnetProp(NULL),
	m_pAudioMagnetGadget(NULL),
	m_bOwnerSetup(false)
{	
}

//////////////////////////////////////////////////////////////////////////

CVehicleGadgetPickUpRopeWithMagnet::~CVehicleGadgetPickUpRopeWithMagnet()
{
	vehicleDisplayf("[PICKUP ROPE DEBBUG] Deleting pickup rope with magnet");
	sysStack::PrintStackTrace();
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::ProcessPhysics3(CVehicle* pParent, const float fTimestep)
{
	CVehicleGadgetPickUpRope::ProcessPhysics3(pParent, fTimestep);

	if(!m_PropObject)
		return;

	if(!m_PropObject->GetCurrentPhysicsInst())
		return;

	if(!m_PropObject->GetCurrentPhysicsInst()->GetArchetype())
		return;

	if(!m_PropObject->GetCurrentPhysicsInst()->IsInLevel())
		return;

	if(!m_bActive && !pParent->IsNetworkClone())
	{
		//m_fPickUpRopeLengthChangeRate = 3.0f;
		SetRopeLength(sfPickUpDefaultRopeLength, sfPickUpRopeMinLength, false);
	}

	f32 fDist = 0.0f;

	// Deal with moving the magnet to point at the target entity and pulling in the target entity if active.
	if(m_pAttractEntity != NULL && m_pTowedEntity == NULL)
	{
		Vector3 vEntPos = VEC3V_TO_VECTOR3(m_pAttractEntity->GetMatrix().d());
		Vector3 vMagnetPos = GetPickUpPropMatrix().d;
		fDist = (vMagnetPos - vEntPos).Mag();

		if(m_bActive)
		{
			if(m_pTowedEntity == NULL)
			{
				// Don't let the entity get too far away.
				float fMaxDistSq = 40.0f * 40.0f;
				if((vEntPos - GetPickUpPropMatrix().d).Mag2() > fMaxDistSq && m_nMode != VEH_PICK_UP_MAGNET_TARGETED_MODE)
				{
					Trigger();
				}
				else
				{
					// Deploy extra rope to allow the magnet to move towards the entity.
					float fExtraRopeLength = Min(m_fMagnetPullRopeLength, fDist);
										
					SetRopeLength(sfPickUpDefaultRopeLength + fExtraRopeLength, sfPickUpRopeMinLength, false);

					// Do the full magnet effect and attachment.
					DoFullMagnet(pParent);
				}
			}
		}

		// Pull the magnet itself towards the closest entity.
		PointMagnetAtTarget(pParent, vEntPos);		
	}

	if(m_pAudioMagnetGadget)
	{
		m_pAudioMagnetGadget->UpdateMagnetStatus(VECTOR3_TO_VEC3V(GetPickUpPropMatrix().d), m_bActive, fDist);
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::ProcessPreComputeImpacts(CVehicle* pVehicle, phCachedContactIterator &impacts)
{
	CVehicleGadgetPickUpRope::ProcessPreComputeImpacts(pVehicle, impacts);

	if(m_Setup)
	{
		impacts.Reset();

		while(!impacts.AtEnd())
		{
			if(pVehicle->InheritsFromHeli() && static_cast<CHeli *>(pVehicle)->GetIsCargobob())
			{
				if(m_PropObject && impacts.GetMyInstance() == pVehicle->GetCurrentPhysicsInst() && impacts.GetOtherInstance() == m_PropObject->GetCurrentPhysicsInst())
				{
					float propUpOnVehUp = Dot(m_PropObject->GetMatrix().c(), pVehicle->GetMatrix().c()).Getf();
					if(propUpOnVehUp < -0.4f)
						impacts.DisableImpact();
				}
			}

			++impacts;
		}

		impacts.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::ProcessControl(CVehicle* pParent)
{
	// If we own both parent and magnet, and the ropes have been setup and the magnet is moving, flag as having everything setup correctly so clones can start receiving updates from us.
	if(!pParent->IsNetworkClone() && m_PropObject && !m_PropObject->IsNetworkClone())
	{
		if(m_Setup && m_PropObject->GetVelocity().Mag2() > 0)
			m_bOwnerSetup = true;
	}

	CVehicleGadgetPickUpRope::ProcessControl(pParent);

	if( m_bOwnerSetup &&
		m_Setup &&
		m_PropObject &&
		m_PropObject->IsNetworkClone() &&
		!m_pAttractEntity )
	{
		Vector3 targetPosition = NetworkInterface::GetLastPosReceivedOverNetwork( m_PropObject );
		Vector3 offset = NetworkInterface::GetLastPosReceivedOverNetwork( pParent );
		
		Vector3 currentPosition = VEC3V_TO_VECTOR3( m_PropObject->GetTransform().GetPosition() );
		float distanceFromNetwork = ( targetPosition - currentPosition ).Mag2();
		distanceFromNetwork = Min( distanceFromNetwork, 1.0f );
		
		if( distanceFromNetwork > 0.0f ) 
		{
			offset = targetPosition - offset;
			Vector3 offsetScale( 0.1f, 0.1f, 1.0f ); // we could really do with the up vector of the object on the host machine but we don't have it.
			offset *= offsetScale;
			offset.Normalize();
			offset.Scale( 5.0f );
			//targetPosition += offset;

			targetPosition = currentPosition + offset;

			bool prevActive = m_bActive;
			float prevMagnetPullStrength = m_fMagnetPullStrength;
		
			TUNE_FLOAT( MAGNET_NETWORK_BLEND_STRENGTH_MAX, 0.05f, 0.0f, 1.0f, 0.01f );	

			m_fMagnetPullStrength = ms_fMagnetPullStrength * MAGNET_NETWORK_BLEND_STRENGTH_MAX * distanceFromNetwork;
			m_bActive = true;

			PointMagnetAtTarget( pParent, targetPosition );

			m_bActive = prevActive;
			m_fMagnetPullStrength = prevMagnetPullStrength;
		}
	}

	// Allow for tuning via RAG.
	if(ms_bOverrideParameters)
	{
		m_fMagnetStrength = ms_fMagnetStrength;
		m_fMagnetFalloff = ms_fMagnetFalloff;

		m_fReducedMagnetStrength = ms_fReducedMagnetStrength;
		m_fReducedMagnetFalloff = ms_fReducedMagnetFalloff;

		m_fMagnetPullStrength = ms_fMagnetPullStrength;
		m_fMagnetPullRopeLength = ms_fMagnetPullRopeLength;
	}

	// This can happen on clones as they do not create their own prop object and instead have to wait to receive the network ID of the existing cloned one before this pointer is valid.
	if(!m_PropObject)
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::ProcessControl - No prop object. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		return;	
	}

	vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::ProcessControl - Mode: %i Parent Vehicle: %s", m_nMode, pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
	vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::ProcessControl - Attract entity: %s", m_pAttractEntity && m_pAttractEntity->GetNetworkObject() ? m_pAttractEntity->GetNetworkObject()->GetLogName() : "NULL");

	m_PropObject->m_nObjectFlags.bIsVehicleGadgetPart = true;
	m_PropObject->m_pGadgetParentVehicle = pParent;
	// Helis don't freeze when there's no collision so don't let the magnet do it either.
	// Make sure this is done here too so that cloned magnet objects are correctly set up.
	m_PropObject->m_nPhysicalFlags.bAllowFreezeIfNoCollision = false;

	if(m_PropObject->m_nParentGadgetIndex == -1)
	{
		for(int nGadget = 0; nGadget < pParent->GetNumberOfVehicleGadgets(); nGadget++)
		{
			if(pParent->GetVehicleGadget(nGadget) == this)
			{
				m_PropObject->m_nParentGadgetIndex = nGadget;
				break;
			}
		}		
	}

	// We need control of the magnet prop if we're also in control of the parent vehicle.
	if(!pParent->IsNetworkClone() && NetworkUtils::IsNetworkCloneOrMigrating(m_PropObject))
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] Requesting control of the magnet prop. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		CRequestControlEvent::Trigger(m_PropObject->GetNetworkObject() NOTFINAL_ONLY(, "CVehicleGadgetPickUpRopeWithMagnet::ProcessControl 1"));		
	}	

	// Switch off the magnet fully if there's no driver or the heli is on the ground.
	/*if(!pParent->GetDriver() || (pParent->InheritsFromHeli() && !static_cast<CHeli *>(pParent)->IsInAir(false)))
	{
		if(m_bActive)
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] Switching off magnet because no driver or on ground.");
			Trigger();
		}

		m_bFullyDisabled = true;
	}
	else*/
		m_bFullyDisabled = false;

	if(m_bActive && !m_pAttractEntity && !m_pTowedEntity)
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] Switching the magnet off because it's active, but there's no target entity. Parent Vehicle: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : pParent->GetModelName() ); 
		Trigger();
	}

#if __BANK
	if(ms_bShowDebug && m_pAttractEntity)
	{
		Vector3 parentPos = VEC3V_TO_VECTOR3(pParent->GetMatrix().d());
		Vector3 entPos = VEC3V_TO_VECTOR3(m_pAttractEntity->GetMatrix().d());
		float dist = (parentPos - entPos).Mag();

		char debugMsg[256];
		sprintf(debugMsg, "Attract ent: %p (%s) | Dist: %f | Audio Magnet Active %s", m_pAttractEntity.Get(), m_pAttractEntity->GetDebugName(), dist, m_pAudioMagnetGadget && m_pAudioMagnetGadget->IsMagnetActive()? "true" : "false");
		grcDebugDraw::Text(parentPos, Color_white, 0, 15, debugMsg, true, -1);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::Trigger()
{
	CVehicleGadgetPickUpRope::Trigger();

	if(m_pAttractEntity)
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::Trigger - toggling active.");
		m_bActive = !m_bActive;		
	}
	else
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::Trigger - No target entity so only switching off here.");

		// Can only switch off magnet if there's no target entity.
		if(m_bActive)
			m_bActive = false;
	}


	if(m_pAttractEntity && m_pAttractEntity->GetIsTypeVehicle())
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::Trigger - Flagging attract entity as under magnet control.");

		// Flag the vehicle as being pulled in by the magnet.
		static_cast<CVehicle *>(m_pAttractEntity.Get())->m_nVehicleFlags.bIsUnderMagnetControl = m_bActive;
	}

	if(!m_bActive)			
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::Trigger -Deactivated. Resetting rope length.");

		// Reset rope.
		SetRopeLength(sfPickUpDefaultRopeLength, sfPickUpRopeMinLength, false);

		// Make sure to unflag any lingering target entity so it can migrate again.
		if(m_pAttractEntity && m_pAttractEntity->GetIsTypeVehicle())
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::Trigger -Deactivated. Resetting vehicle under magnet control status.");
			static_cast<CVehicle *>(m_pAttractEntity.Get())->m_nVehicleFlags.bIsUnderMagnetControl = false;
		}

		if(m_nMode != VEH_PICK_UP_MAGNET_TARGETED_MODE)
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::Trigger -Deactivated. Resetting attract entity because not in targeted mode.");
			m_pAttractEntity = NULL;
			m_pNetAttractEntity.SetEntity(m_pAttractEntity);
			m_pNetAttractEntity.SetEntityID(NETWORK_INVALID_OBJECT_ID);
		}
	}
	
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::SetAmbientMode(bool bAffectVehicles, bool bAffectObjects)
{
	m_nMode = VEH_PICK_UP_MAGNET_AMBIENT_MODE; 
	m_bAffectsVehicles = bAffectVehicles; 
	m_bAffectsObjects = bAffectObjects; 
	m_pAttractEntity = NULL;
	m_pNetAttractEntity.SetEntity(m_pAttractEntity);
	m_pNetAttractEntity.SetEntityID(NETWORK_INVALID_OBJECT_ID);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::SetTargetedMode(CPhysical *pTargetEntity)
{
	m_nMode = VEH_PICK_UP_MAGNET_TARGETED_MODE; 
	m_pAttractEntity = pTargetEntity; 
	m_pNetAttractEntity.SetEntity(m_pAttractEntity);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::DoReducedMagnet(CVehicle* pParent, CPhysical *pPhysical, Vector3 vPosition, bool bIsClosest) //, Vector3 UNUSED_PARAM(vNormal), int UNUSED_PARAM(nComponent)
{
	if(m_bFullyDisabled)
		return;

	// This will still run while the magnet has a target and is activated so the effect is still felt on other entities, but don't override the current target entity in that case.
	// Note: Don't store the target entity if we don't own the pick-up vehicle. This will be synced from the owner so it's consistent between clients.
	if(bIsClosest && !m_bActive && !pParent->IsNetworkClone())
	{
		// This is now our target (Targeted mode has a pre-specified entity so only target the closest one if we're not doing that).
		if(m_nMode != VEH_PICK_UP_MAGNET_TARGETED_MODE)
		{
			m_pAttractEntity = pPhysical;
			m_pNetAttractEntity.SetEntity(m_pAttractEntity);
		}			
	}

	// Only affect the specified entity in targeted mode.
	if(m_nMode == VEH_PICK_UP_MAGNET_TARGETED_MODE)
	{
		if(pPhysical != m_pAttractEntity)
			return;
	}

	if(pPhysical == m_pTowedEntity)
		return;

	Vector3 vMagnetPos = GetPickUpPropMatrix().d;
	Vector3 vHitToMagnet = vMagnetPos - vPosition;	

	// Entity must be below the magnet and within a certain "cone" directly beneath it.
	if(vHitToMagnet.Dot(GetPickUpPropMatrix().c) < -0.6f)
		return;

	// The force is scaled proportional to the distance from the magnet.
	float fDist = vHitToMagnet.Mag();
	vHitToMagnet.Normalize();

	// Apply a gentle force to any entity within the magnets range. 
	// This force is not enough to cause the entity to move significanty towards the magnet; it is more to give the impression of the magnet's effect before it's engaged proper.
	if(!NetworkUtils::IsNetworkCloneOrMigrating(pPhysical))
	{	
		// Calculate the force and torque to apply to the entity.
		if(fDist < 2.5f)
			return;

		float fForceMag = m_fReducedMagnetStrength * powf(fDist, m_fReducedMagnetFalloff);
		Vector3 vForce = vHitToMagnet;
		vForce.Normalize();
		vForce.Scale(fForceMag);
		vForce.x *= 2.0f;
		vForce.y *= 2.0f;
		vForce.z *= 0.1f;
		// Scale this such that vForce represents a velocity change (This is how the old script version did it).
		vForce.Scale(pPhysical->GetMass());		
		vForce.Scale(1.0f / fwTimer::GetRagePhysicsUpdateTimeStep());
		

		// Calculate pitch and roll based on the z-offset of the magnet in the entity's space and how the to-magnet vector maps on to the entity forward and side vectors.
		Vector3 vEntToMagnetLocal = vMagnetPos;
		Matrix34 matEnt = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
		matEnt.UnTransform(vEntToMagnetLocal);
		vEntToMagnetLocal.Normalize();

		// Calculate the intensity of the pitch or roll with 0 being no pitch or roll because the entity is already orientated towards the magnet and 1 being full pitch or roll because the entity is fully out of alignment with the magnet.
		float fOffset = 1.0f - abs(vEntToMagnetLocal.z);

		float fPitch = -(vEntToMagnetLocal.Dot(YAXIS) * fOffset);
		float fRoll = vEntToMagnetLocal.Dot(XAXIS) * fOffset;

		const float fRollTorqueMult = 1.0f;
		const float fPitchTorqueMult = 1.0f;

		Vector3 vRoll = VEC3V_TO_VECTOR3(pPhysical->GetMatrix().GetCol1()) * fRoll * pPhysical->GetAngInertia().y;
		Vector3 vPitch = VEC3V_TO_VECTOR3(pPhysical->GetMatrix().GetCol0()) * fPitch * pPhysical->GetAngInertia().x;

		vRoll *= fRollTorqueMult;
		vRoll *= fForceMag;
		vRoll *= 1.0f / fwTimer::GetRagePhysicsUpdateTimeStep();

		vPitch *= fPitchTorqueMult;
		vPitch *= fForceMag;
		vPitch *= 1.0f / fwTimer::GetRagePhysicsUpdateTimeStep();

		pPhysical->ApplyForceCg(vForce);
		pPhysical->ApplyTorque(vRoll);
		pPhysical->ApplyTorque(vPitch);

#if __BANK
		if(ms_bShowDebug)
		{
			char text[256];
			sprintf(text, "Force: %f, %f, %f", vForce.x, vForce.y, vForce.z);
			grcDebugDraw::Text(vPosition, Color_orange, 0, 0, text, true, -1);

			//sprintf(text, "vTorque: %f, %f, %f", vTorque.x, vTorque.y, vTorque.z);
			//grcDebugDraw::Text(vPosition, Color_orange, 0, 15, text);

			grcDebugDraw::Sphere(vPosition, 0.5f, Color_blue, false, -1);
		}
#endif
		
	}

#if __BANK
	if(ms_bShowDebug)
	{
		grcDebugDraw::Line(vPosition, vMagnetPos, Color_blue, Color_blue, -1);
	}	
#endif
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::DoFullMagnet(CVehicle* pParent)
{
	if(m_bFullyDisabled)
		return;

	if(pParent->IsNetworkClone() && !m_pAttractEntity->IsNetworkClone())
		return;

	// We need control of the target entity so we can have our wicked way with it.
	if(!pParent->IsNetworkClone() && NetworkUtils::IsNetworkCloneOrMigrating(m_pAttractEntity))
	{
		CRequestControlEvent::Trigger(m_pAttractEntity->GetNetworkObject() NOTFINAL_ONLY(, "CVehicleGadgetPickUpRopeWithMagnet::DoMagnet 1"));
		return;
	}

	Vector3 vMagnetPos = GetPickUpPropMatrix().d;
	Vector3 vPosition = VEC3V_TO_VECTOR3(m_pAttractEntity->GetMatrix().d());
	Vector3 vEntToMagnet = vMagnetPos - vPosition;
	
	float fDist = vEntToMagnet.Mag();
	vEntToMagnet.Normalize();

	if(m_pAttractEntity == m_pTowedEntity)
		return;	

	// Entity must be below the magnet for the force to be applied (The magnet itself will still try to point towards the entity though).
	if(vEntToMagnet.Dot(GetPickUpPropMatrix().c) < 0)
		return;

	// Ensure a minimum level of force is applied or else we'll never be ever to pick up something that has moved further away.
	if(fDist > 10.0f)
		fDist = 10.0f;

	// Calculate the force and torque to apply to the entity.
	float fForceMag = m_fMagnetStrength;
	Vector3 vForce = vEntToMagnet;
	vForce.Normalize();
	vForce.Scale(fForceMag);

	// Scale this such that vForce represents a velocity change (This is how the old script version did it).
	vForce.Scale(m_pAttractEntity->GetMass());
	vForce.Scale(1.0f / fwTimer::GetRagePhysicsUpdateTimeStep());

	// Find the speed of the entity relative to the magnet.
	// Apply extra impulse to remove velocity on the entity that is moving it away from the magnet.
	Vector3 vRelEntSpeed = m_pAttractEntity->GetVelocity();
	vRelEntSpeed -= m_PropObject->GetVelocity();

	Vector3 vMagnetDir = VEC3V_TO_VECTOR3(m_PropObject->GetMatrix().c());
	Vector3 vDiff =  vEntToMagnet - vMagnetDir;
	vDiff.Normalize();

	if(vRelEntSpeed.Mag2() > 0)
	{
		// Only apply this extra impulse if the velocity of the attract entity is moving away from having its centre aligned with the magnet.
		if(vRelEntSpeed.Dot(vDiff) < 0)
		{
			float fResistance = 1.0f - (vMagnetDir.Dot(vEntToMagnet) / 0.6f);
			if(fResistance > 1.0f)
				fResistance = 1.0f;
			if(fResistance < 0.0f)
				fResistance = 0.0f;

			// Remove a chunk of the velocity from the entity.
			Vector3 vResistance = vRelEntSpeed * 0.4f;
			vResistance *= -1.0f;
			vResistance *= fResistance;
			vResistance *= m_pAttractEntity->GetMass();
					
			m_pAttractEntity->ApplyImpulseCg(vResistance);
		}
	}
	
	// Calculate pitch and roll based on the z-offset of the magnet in the entity's space and how the to-magnet vector maps on to the entity forward and side vectors.
	Vector3 vEntToMagnetLocal = vMagnetPos;
	Matrix34 matEnt = MAT34V_TO_MATRIX34(m_pAttractEntity->GetMatrix());
	matEnt.UnTransform(vEntToMagnetLocal);
	vEntToMagnetLocal.Normalize();

	// Calculate the intensity of the pitch or roll with 0 being no pitch or roll because the entity is already orientated towards the magnet and 1 being full pitch or roll because the entity is fully out of alignment with the magnet.
	float fOffset = 1.0f - abs(vEntToMagnetLocal.z);

	float fPitch = -(vEntToMagnetLocal.Dot(YAXIS) * fOffset);
	float fRoll = vEntToMagnetLocal.Dot(XAXIS) * fOffset;

	const float fRollTorqueMult = 0.5f;
	const float fPitchTorqueMult = 0.5f;

	Vector3 vRoll = VEC3V_TO_VECTOR3(m_pAttractEntity->GetMatrix().GetCol1()) * fRoll * m_pAttractEntity->GetAngInertia().y;
	Vector3 vPitch = VEC3V_TO_VECTOR3(m_pAttractEntity->GetMatrix().GetCol0()) * fPitch * m_pAttractEntity->GetAngInertia().x;

	vRoll *= fRollTorqueMult;
	vRoll *= fForceMag;
	vRoll *= 1.0f / fwTimer::GetRagePhysicsUpdateTimeStep();

	vPitch *= fPitchTorqueMult;
	vPitch *= fForceMag;
	vPitch *= 1.0f / fwTimer::GetRagePhysicsUpdateTimeStep();


	// Only attach if the entity has touched the magnet and is below it.
	const CCollisionRecord *pCol = m_pAttractEntity->GetFrameCollisionHistory()->FindCollisionRecord(m_PropObject);
	if(pCol)			
	{
		Vector3 vHitToMagnet = vMagnetPos - pCol->m_MyCollisionPos;
		vHitToMagnet.Normalize();
		if(vHitToMagnet.Dot(VEC3V_TO_VECTOR3(m_PropObject->GetMatrix().c())) > 0)
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::DoFullMagnet - Attract entity touched magnet.");
	
			if(!m_pAttractEntity->IsNetworkClone())
				CVehicleGadgetPickUpRope::AttachEntityToPickUpProp(pParent, m_pAttractEntity, vPosition, -1);		
			else
			{
				// We can't actually attach as a clone, but match magnet velocity to prevent the attracted entity from being pulled too far up and smashing into the base of the pickup vehicle.
				if(m_PropObject)
				{
					m_pAttractEntity->SetVelocity(m_PropObject->GetVelocity());
					m_pAttractEntity->SetAngVelocity(Vector3(0, 0, 0));
				}
			}

			return;		
		}
	}	

	// Apply the force.	
	m_pAttractEntity->ApplyForceCg(vForce);
	m_pAttractEntity->ApplyTorque(vRoll);
	m_pAttractEntity->ApplyTorque(vPitch);
		
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::PointMagnetAtTarget(CVehicle *pParent, Vector3 vTargetPos)
{
	if(m_bFullyDisabled)
		return;

	if(!m_PropObject)
		return;

	if(m_fMagnetPullStrength == 0.0f)
		return;

	if( !m_PropObject->GetCurrentPhysicsInst() ||
		!m_PropObject->GetCurrentPhysicsInst()->IsInLevel() )
	{
		return;
	}

	// Only let the owner do this if the magnet is inactive.	
	// If active, allow non-owners to manipulate the magnet to make the pick-up sequence look good on all machines.
	if((!pParent->IsNetworkClone() && !m_bActive) || m_bActive)
	{
		float fPropForceMag = m_fMagnetPullStrength;
		
		Vector3 vMagnetPos = GetPickUpPropMatrix().d;
		Vector3 vEntToMagnet = vMagnetPos - vTargetPos;
		vEntToMagnet.Normalize();

		// Apply the force to the bottom of the magnet to make it rotate towards the entity.
		Vector3 vForceOffset = m_PropObject->GetObjectMtx(GetPickupAttachBone()).d;
		Matrix34 matMagnet = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());
		matMagnet.Transform3x3(vForceOffset);

		Vector3 vForce = -vEntToMagnet;
		vForce.Normalize();
		vForce.Scale(fPropForceMag);
		vForce.Scale(GetPropObjectMass());
		vForce.Scale(1.0f / fwTimer::GetRagePhysicsUpdateTimeStep());

		Vector3 vMagnetDir = -VEC3V_TO_VECTOR3(m_PropObject->GetMatrix().c());
		Vector3 vDiff = (-vEntToMagnet) - vMagnetDir;
		
		// Apply extra impulse to remove all velocity that is moving the magnet away from pointing at its target to prevent it from overshooting.
		Vector3 vLocalSpeed = m_PropObject->GetLocalSpeed(vForceOffset);
		// Only interested in the velocity of the magnet relative to the parent vehicle.
		vLocalSpeed -= pParent->GetVelocity();

		Vector3 vResistance = Vector3(0, 0, 0);

		if(vLocalSpeed.Mag2() > 0)
		{
			// Only apply this extra impulse if the velocity of the magnet at the base is moving away from where it should be pointing.
			// i.e. The velocity vector and the difference between the to-entity and the magnet-z vectors are pointing in opposite directions.
			if(vLocalSpeed.Dot(vDiff) < 0)
			{
				TUNE_FLOAT(MAGNET_SWING_RESISTANCE_STRENGTH_MAX, 0.4f, 0.0f, 1000.0f, 0.01f);				
				float fResistanceStrength = Abs(vMagnetDir.Dot(-vEntToMagnet)) / MAGNET_SWING_RESISTANCE_STRENGTH_MAX;				
				if(fResistanceStrength > 1.0f)
				{
					fResistanceStrength = 1.0f;
				}

				vResistance = vLocalSpeed;
				vResistance *= -1.0f;
				vResistance *= fResistanceStrength;
				vResistance *= GetPropObjectMass();

				// Don't act along the magnet's Z.
				Matrix34 matProp = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());			
				matProp.UnTransform3x3(vResistance);
				vResistance.z = 0;
				matProp.Transform3x3(vResistance);

				m_PropObject->ApplyImpulseCg(vResistance);
			}
		}

		m_PropObject->ApplyForce(vForce, vForceOffset);
	
#if __BANK
		if(ms_bShowDebug)
		{
			char text[256];
			sprintf(text, "Force: %f, %f, %f", vForce.x, vForce.y, vForce.z);
			grcDebugDraw::Text(vMagnetPos, Color_red, 0, 15 * CPhysics::GetCurrentTimeSlice(), text, true, -1);
			grcDebugDraw::Sphere(vMagnetPos, 0.5f, Color_red, false, -1);
			grcDebugDraw::Line(vTargetPos, vMagnetPos, Color_green, Color_green, -1);

			sprintf(text, "To ent: %f, %f, %f", -vEntToMagnet.x, -vEntToMagnet.y, -vEntToMagnet.z);
			grcDebugDraw::Text(vMagnetPos, Color_red, 0, 45 + 15 * CPhysics::GetCurrentTimeSlice(), text, true, -1);

			sprintf(text, "Rope length: %f", m_fRopeLength);
			grcDebugDraw::Text(vMagnetPos, Color_white, 0, 90 + 15 * CPhysics::GetCurrentTimeSlice(), text, true, -1);
			
		}	
#endif

		// Don't let the standard damping force get applied while we're doing this.
		m_bEnableDamping = false;

	}	
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::ProcessInRangeVehicle(CVehicle* pParent, CVehicle *pVehicle, Vector3 vPosition, Vector3 UNUSED_PARAM( vNormal ), int UNUSED_PARAM( nComponent ), phMaterialMgr::Id UNUSED_PARAM(nMaterial), bool bIsClosest)
{
	if(!m_bAffectsVehicles)
		return;

	// Just do the reduced magnet stuff.
	if(!m_bActive || (m_bActive && m_pAttractEntity != pVehicle))
		DoReducedMagnet(pParent, pVehicle, vPosition, bIsClosest ); //vNormal, nComponent, bIsClosest);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::ProcessInRangeObject(CVehicle* pParent, CObject *pObject, Vector3 vPosition, int UNUSED_PARAM( nComponent ), phMaterialMgr::Id nMaterial, bool bIsClosest)
{
	if(!m_bAffectsObjects)
		return;
	
	if(CheckObjectDimensionsForPickup(pObject))
		return;

	VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(PGTAMATERIALMGR->UnpackMtlId(nMaterial));
	if(vfxGroup != VFXGROUP_METAL && vfxGroup != VFXGROUP_CAR_METAL)
		return;

	// Just do the reduced magnet stuff.
	if(!m_bActive || (m_bActive && m_pAttractEntity != pObject))
		DoReducedMagnet(pParent, pObject, vPosition, bIsClosest ); //vNormal, nComponent, bIsClosest);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::ProcessNothingInRange(CVehicle* UNUSED_PARAM(pParent))
{
	if(m_pAttractEntity && m_nMode != VEH_PICK_UP_MAGNET_TARGETED_MODE)
	{
		vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::ProcessNothingInRange - Nothing in range. Resetting attract entity.");

		if(m_pAttractEntity && m_pAttractEntity->GetIsTypeVehicle())
		{
			vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::ProcessNothingInRange - Resetting vehicle under magnet control status.");
			static_cast<CVehicle *>(m_pAttractEntity.Get())->m_nVehicleFlags.bIsUnderMagnetControl = false;
		}

		m_pAttractEntity = NULL;
		m_pNetAttractEntity.SetEntity(m_pAttractEntity);
		m_pNetAttractEntity.SetEntityID(NETWORK_INVALID_OBJECT_ID);
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::AddRopesAndProp(CVehicle &parentVehicle)
{
	//vehicleDisplayf(":AddRopesAndHook, %p (%p, %p, %p) %d",this,m_RopeInstanceA,m_RopeInstanceB,m_PropObject.Get(),(int)m_bRopesAndHookInitialized);

	vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::AddRopesAndProp - Adding ropes and prop.");

	if(!parentVehicle.GetFragInst() || !PHLEVEL->IsInLevel(parentVehicle.GetFragInst()->GetLevelIndex()))
	{
		// Must be in the physics level to add ropes
		return;
	}

	if(m_bRopesAndHookInitialized REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
	{
		return;
	}

	Vec3V rot(V_ZERO);
	Matrix34 matMountA, matMountB, matHook;

	//get the position of the bones at the end of the tow truck arm.
	parentVehicle.GetGlobalMtx(m_iTowMountA, matMountA);
	parentVehicle.GetGlobalMtx(m_iTowMountB, matMountB);


	ropeManager* pRopeManager = CPhysics::GetRopeManager();

	TUNE_INT(TOW_TRUCK_ROPE_SECTIONS, 4, 1, 10, 1);
	m_RopeInstanceA = pRopeManager->AddRope( VECTOR3_TO_VEC3V(matMountA.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true );
	m_RopeInstanceB = pRopeManager->AddRope( VECTOR3_TO_VEC3V(matMountB.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true );

	Assert( m_RopeInstanceA );
	Assert( m_RopeInstanceB );
	if( m_RopeInstanceA )
	{
		m_RopeInstanceA->SetNewUniqueId();
		m_RopeInstanceA->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
		m_RopeInstanceA->SetSpecial(true);
		m_RopeInstanceA->SetIsTense(true);
#if GTA_REPLAY
		CReplayMgr::RecordPersistantFx<CPacketAddRope>(
			CPacketAddRope(m_RopeInstanceA->GetUniqueID(), VECTOR3_TO_VEC3V(matMountA.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true),
			CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceA),
			NULL, 
			true);
#endif
	}
	if( m_RopeInstanceB )
	{
		m_RopeInstanceB->SetNewUniqueId();
		m_RopeInstanceB->SetPhysInstFlags( ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE, ArchetypeFlags::GTA_ENVCLOTH_OBJECT_INCLUDE_TYPES & (~ArchetypeFlags::GTA_VEHICLE_TYPE) );
		m_RopeInstanceB->SetSpecial(true);
		m_RopeInstanceB->SetIsTense(true);
#if GTA_REPLAY
		CReplayMgr::RecordPersistantFx<CPacketAddRope>(
			CPacketAddRope(m_RopeInstanceB->GetUniqueID(), VECTOR3_TO_VEC3V(matMountB.d), rot, m_fRopeLength, sfPickUpRopeMinLength, sfPickUpRopeMaxLength, m_fPickUpRopeLengthChangeRate, siPickUpRopeType, TOW_TRUCK_ROPE_SECTIONS, false, true, 1.0f, false, true),
			CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_RopeInstanceB),
			NULL, 
			true);
#endif
	}

	fwModelId propModelId = CModelInfo::GetModelIdFromName(strStreamingObjectName("hei_prop_heist_magnet",0x7A897074));

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(propModelId);
	if(pModelInfo==NULL)
		return;

	if(m_PropObject == NULL && !parentVehicle.IsNetworkClone())
	{
		CObjectPopulation::CreateObjectInput input(propModelId, ENTITY_OWNEDBY_GAME, true);
		input.m_bForceClone = true;

		m_PropObject = CObjectPopulation::CreateObject(input);				
		m_pNetMagnetProp.SetEntity(m_PropObject);		
	}

	matHook = matMountA;
	matHook.d = (matMountA.d + matMountB.d) * 0.5f;

	if(m_PropObject)
	{
		m_PropObject->m_nObjectFlags.bIsVehicleGadgetPart = true;
		m_PropObject->m_pGadgetParentVehicle = &parentVehicle;
		m_PropObject->m_nParentGadgetIndex = -1;
		// Helis don't freeze when there's no collision so don't let the magnet do it either.
		m_PropObject->m_nPhysicalFlags.bAllowFreezeIfNoCollision = false;

		if(m_PropObject->GetCurrentPhysicsInst())
		{
			phArchetypeDamp *pArchetypeDamp = static_cast<phArchetypeDamp *>(m_PropObject->GetCurrentPhysicsInst()->GetArchetype());
			pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(sfHookAngularDamping, sfHookAngularDamping, sfHookAngularDamping));
			pArchetypeDamp->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(0, 0, FLT_MAX));
		}

		matHook.d -= VEC3V_TO_VECTOR3(parentVehicle.GetMatrix().GetCol1()) * (m_PropObject->GetBoundRadius() * 2.0f);
		m_PropObject->SetMatrix(matHook, true, true, true);
		CGameWorld::Add(m_PropObject, CGameWorld::OUTSIDE );
		m_PropObject->SetNoCollision(&parentVehicle, NO_COLLISION_RESET_WHEN_NO_BBOX);

		// Broken object created so tell the replay it should record it
		REPLAY_ONLY(CReplayMgr::RecordObject(m_PropObject));
	}

#if __ASSERT
	strLocalIndex txdSlot = g_TxdStore.FindSlot(ropeDataManager::GetRopeTxdName());
	if (txdSlot != -1)
	{
		Assertf(CStreaming::HasObjectLoaded(txdSlot, g_TxdStore.GetStreamingModuleId()), "Rope txd not streamed in for vehicle %s", pModelInfo->GetModelName());
	}
#endif // __ASSERT

	m_bRopesAndHookInitialized = true;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::ActuallyDetachVehicle( CVehicle *pParentVehicle, bool bReattachHook )
{
	if(m_pAudioMagnetGadget && m_pTowedEntity)
	{
		m_pAudioMagnetGadget->TriggerMagnetDetach(m_pTowedEntity);
	}

	CVehicleGadgetPickUpRope::ActuallyDetachVehicle(pParentVehicle, bReattachHook);	
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::AttachEntityToPickUpProp( CVehicle *pPickUpVehicle, CPhysical *pEntityToAttach, s16 iBoneIndex, const Vector3 &vVehicleOffsetPosition, s32 iComponentIndex, bool bNetAssert)
{
	vehicleDisplayf("[PICKUP ROPE DEBUG] CVehicleGadgetPickUpRopeWithMagnet::AttachEntityToPickUpProp - Attaching %s entity to magnet.", pEntityToAttach->IsNetworkClone() ? "clone" : "local");

	// Match magnet velocity when picked up to maintain momentum.
	if(m_PropObject)
	{
		pEntityToAttach->SetVelocity(m_PropObject->GetVelocity());
		pEntityToAttach->SetAngVelocity(Vector3(0, 0, 0));
	}

	
	CVehicleGadgetPickUpRope::AttachEntityToPickUpProp(pPickUpVehicle, pEntityToAttach, iBoneIndex, vVehicleOffsetPosition, iComponentIndex, bNetAssert);

	if(m_pAudioMagnetGadget)
	{
		m_pAudioMagnetGadget->TriggerMagnetAttach(pEntityToAttach);
	}

	// Shake the camera if the entity being attached is a vehicle that contains a local player.
	TUNE_FLOAT(MAGNET_ATTACH_SHAKE, 0.25f, -10000.0f, 10000.0f, 0.1f);
	if(pEntityToAttach->GetIsTypeVehicle() && static_cast<CVehicle *>(pEntityToAttach)->ContainsLocalPlayer())
	{
		camInterface::GetGameplayDirector().Shake("SMALL_EXPLOSION_SHAKE", MAGNET_ATTACH_SHAKE);				
		CControlMgr::StartPlayerPadShake(200, 200, 200, 200, 0);
	}

}

//////////////////////////////////////////////////////////////////////////

Matrix34 CVehicleGadgetPickUpRopeWithMagnet::GetPickUpPropMatrix()
{
	Matrix34 mMagnetMainBone = MAT34V_TO_MATRIX34(m_PropObject->GetMatrix());
	if(m_PropObject->GetSkeleton())
	{		
		mMagnetMainBone = m_PropObject->GetObjectMtx(2);
		mMagnetMainBone.d += GetPickupAttachOffset();

		mMagnetMainBone.Dot(MAT34V_TO_MATRIX34(m_PropObject->GetMatrix()));
	}

	return Matrix34(mMagnetMainBone);
}

//////////////////////////////////////////////////////////////////////////

Vector3 CVehicleGadgetPickUpRopeWithMagnet::GetPickupAttachOffset()
{
	TUNE_GROUP_FLOAT(PICKUP_MAGNET, ATTACH_OFFSET, -0.5f, -1.0f, 1.0f, 0.01f);
	return Vector3(0, 0, ATTACH_OFFSET);
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::Serialise(CSyncDataBase& serialiser)
{
	CVehicleGadgetPickUpRope::Serialise(serialiser);

	ObjectId magnetId = m_pNetMagnetProp.GetEntityID();
	SERIALISE_OBJECTID(serialiser, magnetId);
	m_pNetMagnetProp.SetEntityID(magnetId);

	bool bHaveProp = !!m_PropObject;
	m_PropObject = static_cast<CObject *>(m_pNetMagnetProp.GetEntity());

	if(!bHaveProp && m_PropObject)
	{
		m_PropObject->m_nParentGadgetIndex = -1;
	}

	ObjectId entityId = m_pNetAttractEntity.GetEntityID();
	SERIALISE_OBJECTID(serialiser, entityId);
	m_pNetAttractEntity.SetEntityID(entityId);

	m_pAttractEntity = static_cast<CPhysical *>(m_pNetAttractEntity.GetEntity());

	//SERIALISE_BOOL(serialiser, m_bFullyDisabled);

	bool bLastActive = m_bActive;
	SERIALISE_BOOL(serialiser, m_bActive);

	if(bLastActive != m_bActive)
	{
		m_bActive = bLastActive;
		Trigger();
	}

	// Have to sync these values to ensure that the magnet will behave in the same way even if it changes ownership after having had tweaks applied.
	// i.e. If Player 1 makes the magnet strength a lot weaker, but then ownership transfers to Player 2, Player 2 needs to be simulating it with the same reduced strength as Player 1 was.
	SERIALISE_BOOL(serialiser, m_bEnsurePickupEntityUpright);

	SERIALISE_PACKED_FLOAT(serialiser, m_fMagnetStrength, 10.0f, 4);
	SERIALISE_PACKED_FLOAT(serialiser, m_fMagnetFalloff, 10.0f, 4);

	SERIALISE_PACKED_FLOAT(serialiser, m_fReducedMagnetStrength, 10.0f, 4);
	SERIALISE_PACKED_FLOAT(serialiser, m_fReducedMagnetFalloff, 10.0f, 4);

	SERIALISE_PACKED_FLOAT(serialiser, m_fMagnetPullStrength, 10.0f, 4);
	SERIALISE_PACKED_FLOAT(serialiser, m_fMagnetPullRopeLength, 10.0f, 4);

	SERIALISE_PACKED_FLOAT(serialiser, m_fMagnetEffectRadius, 100.0f, 4);

	// Need to know if the owner has setup the rope constraints fully yet.	
	SERIALISE_BOOL(serialiser, m_bOwnerSetup);	
}	

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetPickUpRopeWithMagnet::InitAudio(CVehicle* pParent)
{
	if(!m_pAudioMagnetGadget)
	{
		m_pAudioMagnetGadget = (audVehicleGadgetMagnet*)pParent->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_MAGNET);
	}
}

//////////////////////////////////////////////////////////////////////////

CVehicleGadgetDynamicSpoiler::CVehicleGadgetDynamicSpoiler() :
	  m_pDynamicSpoilerAudio(NULL)
	, m_fCurrentSpoilerRise(0.0f)
	, m_fCurrentStrutRise(0.0f)
	, m_fCurrentSpoilerAngle(0.0f)
	, m_bEnableDynamicSpoilers(true)
	, m_bResetSpoilerBones(false)
{

}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDynamicSpoiler::Fix()
{
	if(m_pDynamicSpoilerAudio)
	{
		m_pDynamicSpoilerAudio->StopAllSounds();
	}
	
	m_fCurrentSpoilerRise = 0.0f;
	m_fCurrentStrutRise = 0.0f;
	m_fCurrentSpoilerAngle = 0.0f;
	m_bEnableDynamicSpoilers = true;

	// Have to reset the bones when fixing as the frag skeleton doesn't get reset for clone vehicles, which leaves the spoiler in the "down" state, but positioned in the "up" state.
	m_bResetSpoilerBones = true;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDynamicSpoiler::InitAudio(CVehicle* pParent)
{
	if(!m_pDynamicSpoilerAudio)
	{
		m_pDynamicSpoilerAudio = (audVehicleGadgetDynamicSpoiler*)pParent->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_DYNAMIC_SPOILER);
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDynamicSpoiler::ProcessPrePhysics(CVehicle* pParent)
{
	// Process dynamic spoilers.	
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_STRUTS_RAISE, 0.16f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE, 0.14f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_ANGLE, -0.634f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_ANGLE_NERO, -0.5f, -10.0f, 10.0f, 0.01f);
    
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE_NERO, 0.165f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE_XA21, 0.16f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MIN_SPOILER_RAISE_FOR_ROTATION_XA21, 0.10f, 0.0f, 10.0f, 0.01f);
    TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE_FURIA, 0.141359f, 0.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE_IGNUS2, 0.141359f, 0.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE_IGNUS, 0.141359f, 0.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE_ITALIRSX, 0.0f, 0.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, MAX_SPOILER_RAISE_TENF, 0.0, 0.0f, 10.0f, 0.01f );

	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE, -0.3f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE_NERO, -0.2f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE_XA21, -0.52f, -10.0f, 10.0f, 0.01f);
    TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE_FURIA, -0.35f, -10.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE_IGNUS2, -0.35f, -10.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE_IGNUS, -0.35f, -10.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE_ITALIRSX, -0.436332f, -10.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, RAISED_SPOILER_ANGLE_TENF, -0.436332f, -10.0f, 10.0f, 0.01f );

	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_RISE_RATE, 0.1f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_ROTATE_RATE, 5.0f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_SLOW_ROTATE_RATE, 0.2f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_ROTATE_RATE_WITHOUT_AIRBRAKE, 2.0f, -10.0f, 10.0f, 0.01f);
    TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_ROTATE_RATE_FURIA, 0.5f, -10.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_ROTATE_RATE_IGNUS2, 0.5f, -10.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_ROTATE_RATE_IGNUS, 0.5f, -10.0f, 10.0f, 0.01f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_ROTATE_RATE_TENF, 0.5f, -10.0f, 10.0f, 0.01f );

	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_SPEED_ACTIVATION, 15.0f, -100.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, SPOILER_AIRBRAKE_SPEED_ACTIVATION, 10.0f, -100.0f, 100.0f, 0.01f);

    TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, FURIA_Y_OFFSET, 0.013f, -1.0f, 1.0f, 0.001f );
    TUNE_GROUP_FLOAT( DYNAMIC_SPOILERS, FURIA_Z_OFFSET, -0.055f, -1.0f, 1.0f, 0.001f );

	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, IGNUS2_Y_OFFSET, 0.013f, -1.0f, 1.0f, 0.001f );
    TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, IGNUS2_Z_OFFSET, -0.055f, -1.0f, 1.0f, 0.001f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, IGNUS_Y_OFFSET, 0.013f, -1.0f, 1.0f, 0.001f );
    TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, IGNUS_Z_OFFSET, -0.055f, -1.0f, 1.0f, 0.001f );
	TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, TENF_Y_OFFSET, 0.013f, -1.0f, 1.0f, 0.001f );
    TUNE_GROUP_FLOAT(DYNAMIC_SPOILERS, TENF_Z_OFFSET, -0.055f, -1.0f, 1.0f, 0.001f );

	TUNE_GROUP_BOOL(DYNAMIC_SPOILERS, FORCE_SPOILER_RAISE, false);

	int nSpoilerBone = pParent->GetBoneIndex(VEH_SPOILER);
	int nStrutsBone = pParent->GetBoneIndex(VEH_STRUTS);

	int nMiscMBone = pParent->GetBoneIndex(VEH_MISC_M);
	int nMiscNBone = pParent->GetBoneIndex(VEH_MISC_N);

	fragInst *pFragInst = pParent->GetVehicleFragInst();

	bool bItaliRSXorComet6 = (( MI_CAR_ITALIRSX.IsValid() && pParent->GetModelIndex() == MI_CAR_ITALIRSX ) ||
		( MI_CAR_COMET6.IsValid() && pParent->GetModelIndex() == MI_CAR_COMET6 ) ||
		( MI_CAR_COMET7.IsValid() && pParent->GetModelIndex() == MI_CAR_COMET7 ) ||
		( MI_CAR_TORERO2.IsValid() && pParent->GetModelIndex() == MI_CAR_TORERO2 ) ||
		( MI_CAR_TENF.IsValid() && pParent->GetModelIndex() == MI_CAR_TENF ) ||
		( MI_CAR_TENF2.IsValid() && pParent->GetModelIndex() == MI_CAR_TENF2 ) ||
		( MI_CAR_OMNISEGT.IsValid() && pParent->GetModelIndex() == MI_CAR_OMNISEGT )
		);

	bool enableAirbrake = ( !MI_CAR_XA21.IsValid() || pParent->GetModelIndex() != MI_CAR_XA21 ) &&
		( !MI_CAR_FURIA.IsValid() || pParent->GetModelIndex() != MI_CAR_FURIA ) &&
		( !MI_CAR_IGNUS2.IsValid() || pParent->GetModelIndex() != MI_CAR_IGNUS2 ) &&
		( !MI_CAR_IGNUS.IsValid() || pParent->GetModelIndex() != MI_CAR_IGNUS ) &&		
		!bItaliRSXorComet6;

	
	bool bHasSpoilerBones = ( nSpoilerBone != -1 && nStrutsBone != -1 );
	bool bSpoilerBonesEnabled = !pParent->GetVehicleDrawHandler().GetVariationInstance().IsBoneTurnedOff( pParent, pParent->GetBoneIndex( VEH_SPOILER ) );

	//Disable the spoiler audio if someone has hidden the spoiler(they have probably put a big wing on as a mod)
	bool bPlaySpoilerAudio = bHasSpoilerBones && bSpoilerBonesEnabled;

	if(pFragInst && pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst())
	{		
		if( bHasSpoilerBones &&
			bSpoilerBonesEnabled &&
			m_bResetSpoilerBones)
		{
			// Reset the spoiler bones to their defaults.
			// This is done after the vehicle is fixed to ensure the bones and the spoiler state are synced correctly.
			pParent->GetSkeleton()->GetLocalMtx(nSpoilerBone) = pParent->GetSkeleton()->GetSkeletonData().GetDefaultTransform(nSpoilerBone);
			pParent->GetSkeleton()->GetLocalMtx(nStrutsBone) = pParent->GetSkeleton()->GetSkeletonData().GetDefaultTransform(nStrutsBone);

			pParent->GetSkeleton()->PartialUpdate(nStrutsBone);
			pParent->GetSkeleton()->PartialUpdate(nSpoilerBone);

			if(bItaliRSXorComet6)
			{
				if( nMiscMBone != -1 )
				{
					pParent->GetSkeleton()->GetLocalMtx(nMiscMBone) = pParent->GetSkeleton()->GetSkeletonData().GetDefaultTransform(nMiscMBone);
					pParent->GetSkeleton()->PartialUpdate(nMiscMBone);
				}
				if( nMiscNBone != -1 )
				{
					pParent->GetSkeleton()->GetLocalMtx(nMiscNBone) = pParent->GetSkeleton()->GetSkeletonData().GetDefaultTransform(nMiscNBone);				
					pParent->GetSkeleton()->PartialUpdate(nMiscNBone);
				}
			}

			m_bResetSpoilerBones = false;
		}

		bool bRise = false;
		bool bRotate = false;
			
		// If the vehicle's body health drops below the threshold, put everything away and leave it there.
		if(m_bEnableDynamicSpoilers)
		{			
			// Raise up if going above speed threshold.
			Vec3V vCurrentVelocity = pParent->GetCollider() ? pParent->GetCollider()->GetVelocity() : Vec3V(V_ZERO);
			if( FORCE_SPOILER_RAISE ||
				( pParent->GetDriver() && Mag(vCurrentVelocity).Getf() > SPOILER_SPEED_ACTIVATION && Dot(vCurrentVelocity, pParent->GetMatrix().GetCol1()).Getf() > 0.8f ) )
			{
				bRise = true;
			}

			// Only rotate the spoiler when it is raised up enough.
			if( enableAirbrake &&
				m_fCurrentSpoilerRise > ( MAX_SPOILER_RAISE / 2.0f ) )
			{
				// Angle spoiler when braking.
				if(pParent->GetDriver() && pParent->m_vehControls.GetBrake() > 0.0f && Mag(vCurrentVelocity).Getf() > SPOILER_AIRBRAKE_SPEED_ACTIVATION && Dot(vCurrentVelocity, pParent->GetMatrix().GetCol1()).Getf() > 0)
				{
					bRotate = true;
				}	
			}
		}

		if(!bRise && !enableAirbrake)
		{
			bRotate = false;
		}

		// Don't animate if we're in the air.
		if(!pParent->IsInAir(false))
		{				
			if(m_pDynamicSpoilerAudio)
			{
				m_pDynamicSpoilerAudio->SetFrozen(false);
			}

			// Move and rotate bones.
			float fDesiredRiseStrut = bRise ? MAX_STRUTS_RAISE : 0.0f;
			float fDesiredRiseSpoiler = bRise ? MAX_SPOILER_RAISE : 0.0f;

			float maxSpoilerAngle		= MAX_SPOILER_ANGLE;
			float maxSpoilerRaise		= MAX_SPOILER_RAISE;
			float raisedSpoilerAngle	= RAISED_SPOILER_ANGLE;

			float minSpoilerRaiseForRotation = 0.0f;

			if( !bRotate &&
				bRise &&
				MI_CAR_NERO.IsValid() && pParent->GetModelIndex() == MI_CAR_NERO )
			{
				fDesiredRiseSpoiler = MAX_SPOILER_RAISE_NERO;
				maxSpoilerAngle		= MAX_SPOILER_ANGLE_NERO;
				raisedSpoilerAngle	= RAISED_SPOILER_ANGLE_NERO;
			}
			else if( MI_CAR_XA21.IsValid() && pParent->GetModelIndex() == MI_CAR_XA21 )
			{
				maxSpoilerRaise = MAX_SPOILER_RAISE_XA21;

				if( bRise )
				{
					fDesiredRiseSpoiler = MAX_SPOILER_RAISE_XA21;
					raisedSpoilerAngle	= RAISED_SPOILER_ANGLE_XA21;
					minSpoilerRaiseForRotation = MIN_SPOILER_RAISE_FOR_ROTATION_XA21;
				}
			}
            else if( MI_CAR_FURIA.IsValid() && pParent->GetModelIndex() == MI_CAR_FURIA )
            {
                maxSpoilerRaise = MAX_SPOILER_RAISE_FURIA;

                if( bRise )
                {
                    fDesiredRiseSpoiler = MAX_SPOILER_RAISE_FURIA;
                    raisedSpoilerAngle = RAISED_SPOILER_ANGLE_FURIA;
                    minSpoilerRaiseForRotation = MIN_SPOILER_RAISE_FOR_ROTATION_XA21;
                }
            }
			else if( MI_CAR_IGNUS2.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS2 )
            {
                maxSpoilerRaise = MAX_SPOILER_RAISE_IGNUS2;
                if( bRise )
                {
                    fDesiredRiseSpoiler = MAX_SPOILER_RAISE_IGNUS2;
                    raisedSpoilerAngle = RAISED_SPOILER_ANGLE_IGNUS2;
                    minSpoilerRaiseForRotation = MIN_SPOILER_RAISE_FOR_ROTATION_XA21;
                }
            }
			else if( MI_CAR_IGNUS.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS )
            {
                maxSpoilerRaise = MAX_SPOILER_RAISE_IGNUS;
                if( bRise )
                {
                    fDesiredRiseSpoiler = MAX_SPOILER_RAISE_IGNUS;
                    raisedSpoilerAngle = RAISED_SPOILER_ANGLE_IGNUS;
                    minSpoilerRaiseForRotation = MIN_SPOILER_RAISE_FOR_ROTATION_XA21;
                }
            }
			else if( (MI_CAR_TENF.IsValid() && pParent->GetModelIndex() == MI_CAR_TENF) || (MI_CAR_TENF2.IsValid() && pParent->GetModelIndex() == MI_CAR_TENF2))
            {
                maxSpoilerRaise = MAX_SPOILER_RAISE_TENF;
                if( bRise )
                {
                    fDesiredRiseSpoiler = MAX_SPOILER_RAISE_TENF;
                    raisedSpoilerAngle = RAISED_SPOILER_ANGLE_TENF;
                }
            }
			else if( bItaliRSXorComet6 )
			{
				maxSpoilerRaise = MAX_SPOILER_RAISE_ITALIRSX;
				if( bRise )
				{
					fDesiredRiseSpoiler = MAX_SPOILER_RAISE_ITALIRSX;
					raisedSpoilerAngle = RAISED_SPOILER_ANGLE_ITALIRSX;
				}
			}


			float fDesiredAngle = bRotate ? maxSpoilerAngle : (bRise ? raisedSpoilerAngle : 0.0f);

			float fSpoilerRiseAmount = 0;
			float fSpoilerRotateAmount = 0;

			float fRiseStrutDiff = fDesiredRiseStrut - m_fCurrentStrutRise;
			float fRiseSpoilerDiff = fDesiredRiseSpoiler - m_fCurrentSpoilerRise;
			float fAngleDiff = fDesiredAngle - m_fCurrentSpoilerAngle;

			// Raise
			// Struts
			if(abs(fRiseStrutDiff) <= SPOILER_RISE_RATE * fwTimer::GetTimeStep())
			{
				fSpoilerRiseAmount = fRiseStrutDiff;

				if( bPlaySpoilerAudio &&
					m_pDynamicSpoilerAudio )
				{
					m_pDynamicSpoilerAudio->SetSpoilerStrutState(bRise? audVehicleGadgetDynamicSpoiler::AUD_STRUT_RAISED : audVehicleGadgetDynamicSpoiler::AUD_STRUT_LOWERED);
				}
			}
			else
			{
				if(fRiseStrutDiff > 0)
					fSpoilerRiseAmount += SPOILER_RISE_RATE * fwTimer::GetTimeStep();

				if(fRiseStrutDiff < 0)
					fSpoilerRiseAmount -= SPOILER_RISE_RATE * fwTimer::GetTimeStep();

				if(bPlaySpoilerAudio &&
					m_pDynamicSpoilerAudio)
				{
					m_pDynamicSpoilerAudio->SetSpoilerStrutState(bRise? audVehicleGadgetDynamicSpoiler::AUD_STRUT_RAISING : audVehicleGadgetDynamicSpoiler::AUD_STRUT_LOWERING);
				}
			}

			if( bHasSpoilerBones && bSpoilerBonesEnabled)
			{
				pParent->GetObjectMtx(nStrutsBone).d += pParent->GetObjectMtx(nStrutsBone).c * fSpoilerRiseAmount;
			}

			m_fCurrentStrutRise += fSpoilerRiseAmount;

			// Spoiler
			// Only begin to move the spoiler once the struts are in the correct position.
			if( ( bRise && m_fCurrentStrutRise <= fDesiredRiseSpoiler ) ||
				( !bRise && m_fCurrentStrutRise <= maxSpoilerRaise ) )
			{
				fSpoilerRiseAmount = 0;
				if(abs(fRiseSpoilerDiff) <= SPOILER_RISE_RATE * fwTimer::GetTimeStep())
				{
					fSpoilerRiseAmount = fRiseSpoilerDiff;

					if( bPlaySpoilerAudio &&
						m_pDynamicSpoilerAudio )
					{
						m_pDynamicSpoilerAudio->SetSpoilerAngleState(bRotate? audVehicleGadgetDynamicSpoiler::AUD_SPOILER_ROTATED_FORWARDS : audVehicleGadgetDynamicSpoiler::AUD_SPOILER_ROTATED_BACKWARDS);
					}
				}
				else
				{
					if(fRiseSpoilerDiff > 0)
						fSpoilerRiseAmount += SPOILER_RISE_RATE * fwTimer::GetTimeStep();

					if(fRiseSpoilerDiff < 0)
						fSpoilerRiseAmount -= SPOILER_RISE_RATE * fwTimer::GetTimeStep();

					if( bPlaySpoilerAudio && 
						m_pDynamicSpoilerAudio )
					{
						m_pDynamicSpoilerAudio->SetSpoilerAngleState(bRotate? audVehicleGadgetDynamicSpoiler::AUD_SPOILER_ROTATING_FORWARDS : audVehicleGadgetDynamicSpoiler::AUD_SPOILER_ROTATING_BACKWARDS);
					}
				}

				if( bHasSpoilerBones && bSpoilerBonesEnabled)
				{
					pParent->GetObjectMtx(nSpoilerBone).d += pParent->GetObjectMtx(nStrutsBone).c * fSpoilerRiseAmount;										
				}
				m_fCurrentSpoilerRise += fSpoilerRiseAmount;
			}

			// Rotate
			if( m_fCurrentSpoilerRise >= minSpoilerRaiseForRotation )
			{
				bool bUseFastRotate = enableAirbrake && ( m_fCurrentSpoilerAngle < RAISED_SPOILER_ANGLE || fDesiredAngle < RAISED_SPOILER_ANGLE );
				float fRotateRate = bUseFastRotate ? SPOILER_ROTATE_RATE : SPOILER_SLOW_ROTATE_RATE;

				if( !enableAirbrake )
				{
					fRotateRate = SPOILER_ROTATE_RATE_WITHOUT_AIRBRAKE;

                    if( MI_CAR_FURIA.IsValid() &&
                        pParent->GetModelIndex() == MI_CAR_FURIA )
                    {
                        fRotateRate = SPOILER_ROTATE_RATE_FURIA;
                    }
					if( MI_CAR_IGNUS2.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS2 )
                    {
                        fRotateRate = SPOILER_ROTATE_RATE_IGNUS2;
                    }
					if( MI_CAR_IGNUS.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS )
                    {
                        fRotateRate = SPOILER_ROTATE_RATE_IGNUS;
                    }
				}

				if(abs(fAngleDiff) <= fRotateRate * fwTimer::GetTimeStep())
					fSpoilerRotateAmount = fAngleDiff;
				else
				{
					if(fAngleDiff > 0)
						fSpoilerRotateAmount += fRotateRate * fwTimer::GetTimeStep();

					if(fAngleDiff < 0)
						fSpoilerRotateAmount -= fRotateRate * fwTimer::GetTimeStep();
				}
			}

			if( bHasSpoilerBones && bSpoilerBonesEnabled)
			{
				pParent->GetObjectMtx(nSpoilerBone).RotateLocalX(fSpoilerRotateAmount);		

				// Update the component matrices too.
				Mat34V_Ref boneMat = pFragInst->GetCacheEntry()->GetHierInst()->skeleton->GetObjectMtx(nSpoilerBone);	

                if( MI_CAR_FURIA.IsValid() && pParent->GetModelIndex() == MI_CAR_FURIA )
                {
                    boneMat.GetCol3Ref().SetYf( boneMat.d().GetYf() + ( FURIA_Y_OFFSET * fSpoilerRotateAmount ) );
                    boneMat.GetCol3Ref().SetZf( boneMat.d().GetZf() + ( FURIA_Z_OFFSET * fSpoilerRotateAmount ) );
                }
				if( MI_CAR_IGNUS2.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS2 )
                {
                    boneMat.GetCol3Ref().SetYf( boneMat.d().GetYf() + ( IGNUS2_Y_OFFSET * fSpoilerRotateAmount ) );
                    boneMat.GetCol3Ref().SetZf( boneMat.d().GetZf() + ( IGNUS2_Z_OFFSET * fSpoilerRotateAmount ) );
                }
				if( MI_CAR_IGNUS.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS )
                {
                    boneMat.GetCol3Ref().SetYf( boneMat.d().GetYf() + ( IGNUS_Y_OFFSET * fSpoilerRotateAmount ) );
                    boneMat.GetCol3Ref().SetZf( boneMat.d().GetZf() + ( IGNUS_Z_OFFSET * fSpoilerRotateAmount ) );
                }
				pFragInst->GetCacheEntry()->GetBound()->SetCurrentMatrix(pFragInst->GetCacheEntry()->GetComponentIndexFromBoneIndex(nSpoilerBone), boneMat);

				if( bItaliRSXorComet6 )
				{
					if(nMiscNBone != -1)
					{
						pParent->GetObjectMtx(nMiscNBone).RotateLocalX(fSpoilerRotateAmount);	
					}

					if(nMiscMBone != -1)
					{
						pParent->GetObjectMtx(nMiscMBone).RotateLocalX(fSpoilerRotateAmount);	
					}
				}

				pParent->GetSkeleton()->PartialInverseUpdate(nSpoilerBone);
				pParent->GetSkeleton()->PartialInverseUpdate(nStrutsBone);
			}
			m_fCurrentSpoilerAngle += fSpoilerRotateAmount;

			// Calculate downforce multiplier for wheels based on how raised the spoiler is.
			for(int nWheel = 0; nWheel < pParent->GetNumWheels(); nWheel++)
			{
				float fExtraDownforce = CWheel::ms_fDownforceMultSpoiler - CWheel::ms_fDownforceMult;
				pParent->GetWheel(nWheel)->m_fDownforceMult = CWheel::ms_fDownforceMult + ((m_fCurrentSpoilerRise / MAX_SPOILER_RAISE) * fExtraDownforce);				
			}
		}

		if( !bPlaySpoilerAudio ) // Spoiler is not allowed to move
		{
			if(m_pDynamicSpoilerAudio)
			{
				m_pDynamicSpoilerAudio->SetFrozen(true);
			}
		}
	}
}

CVehicleGadgetDynamicFlaps::CVehicleGadgetDynamicFlaps() : m_bEnableDynamicFlaps( true ), m_bResetFlapBones( true )
{
	for( int i = 0; i < MAX_NUM_SPOILERS; i++ )
	{
		m_fDesiredFlapAmount[ i ] = 0.0f;
	}
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDynamicFlaps::Fix()
{
	m_bEnableDynamicFlaps = true;

	// Have to reset the bones when fixing as the frag skeleton doesn't get reset for clone vehicles, which leaves the spoiler in the "down" state, but positioned in the "up" state.
	m_bResetFlapBones = true;
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDynamicFlaps::InitAudio(CVehicle* UNUSED_PARAM(pParent))
{
}

//////////////////////////////////////////////////////////////////////////

void CVehicleGadgetDynamicFlaps::ProcessPrePhysics(CVehicle* pParent)
{

	// Process dynamic spoilers.
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_SPOILER_ANGLE, -0.18f, -10.0f, 10.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_SPOILER_PCT_FOR_LAT_ACCEL, 0.8f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, ACTIVATION_FORCE_MAG, 0.4f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_LATERAL_ACCEL, 1.2f, -10.0f, 10.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_SPOILER_PCT_FOR_SPEED, 0.2f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, SPOILER_SPEED_ACTIVATION, 30.0f, -100.0f, 100.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_SPOILER_PCT_FOR_STEERING, 0.4f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_SPOILER_PCT_FOR_STEERING_OUTSIDE, 0.15f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, STEERING_THRESHOLD, 0.4f, -10.0f, 10.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_SPOILER_PCT_FOR_SPIN_INSIDE, 0.4f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, MAX_SPOILER_PCT_FOR_SPIN_OUTSIDE, 0.3f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, SPIN_THRESHOLD, 0.8f, -10.0f, 10.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, THROTTLE_THRESHOLD, 0.4f, -10.0f, 10.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, SPOILER_ROTATE_RATE, 1.5f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, SPOILER_SLOW_ROTATE_RATE, 0.5f, -10.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, SPOILER_QUICK_ROTATE_RATE, 6.0f, -10.0f, 10.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, SPOILER_DOWNFORCE_MAX, CWheel::ms_fDownforceMultSpoiler, -10.0f, 10.0f, 0.01f);

	TUNE_GROUP_FLOAT(DYNAMIC_FLAPS, INACTIVE_VELOCITY_SQR, 5.0f, 0.0f, 1000.0f, 0.1f);

	const eHierarchyId nSpoilerBones[ MAX_NUM_SPOILERS ] = 
	{
		VEH_FLAP_L,
		VEH_FLAP_R,
	};

	float fTimeStep			= fwTimer::GetTimeStep();
	phCollider* pCollider	= pParent->GetCollider();

	if( !pCollider )
	{
		return;
	}

	Vec3V vCurrentVelocity	= pCollider->GetVelocity();
	const Mat34V& matrix	= pParent->GetCollider()->GetMatrix();

	for( int nSpoiler = 0; nSpoiler < MAX_NUM_SPOILERS; nSpoiler++ )
	{
		int nBoneIndex = pParent->GetBoneIndex( nSpoilerBones[ nSpoiler ] );
		if( nBoneIndex != -1 )
		{
			float fSpoilerSpeed = SPOILER_ROTATE_RATE;

			Vector3 vSpoilerXDir = -pParent->GetObjectMtx(nBoneIndex).d;
			vSpoilerXDir.y = 0;
			vSpoilerXDir.z = 0;
			vSpoilerXDir.Normalize();

			if( !pParent->IsEngineOn() ||
				MagSquared( vCurrentVelocity ).Getf() < INACTIVE_VELOCITY_SQR )
			{
				m_fDesiredFlapAmount[ nSpoiler ] = 0.0f;
			}
			// Only do this for vehicles on the ground. In-air spoiler control is done in CAutomobile::ProcessDriverInputsForStability.
			else if( !pParent->IsInAir(false) )
			{
				// Reset spoiler amount.
				m_fDesiredFlapAmount[ nSpoiler ] = 0.0f;

				// Raise outside spoiler for lateral acceleration.
				Vec3V vAcceleration = InvScale( Subtract( vCurrentVelocity, m_vPrevVelocity ), ScalarV( fTimeStep ) );	

				Vec3V vGForceV = InvScale( vAcceleration, ScalarV( -GRAVITY ) );
				vGForceV = UnTransform3x3Full( matrix, vGForceV );

				Vector3 vGForce = VEC3V_TO_VECTOR3( vGForceV );
				vGForce.y = abs( vGForce.y );

				float fLateralAcceleration = 0.0f;				
				fLateralAcceleration = -vGForce.Dot( vSpoilerXDir );	

				fLateralAcceleration -= ACTIVATION_FORCE_MAG;
				fLateralAcceleration = Clamp( fLateralAcceleration, 0.0f, MAX_LATERAL_ACCEL );

				m_fDesiredFlapAmount[ nSpoiler ] = (fLateralAcceleration / ( MAX_LATERAL_ACCEL -  ACTIVATION_FORCE_MAG ) );
				m_fDesiredFlapAmount[ nSpoiler ] *= MAX_SPOILER_PCT_FOR_LAT_ACCEL;		

				// Raise both slightly and slowly if we're going quickly in a straight line (roughly).
				if( Mag( vCurrentVelocity ).Getf() > SPOILER_SPEED_ACTIVATION && Dot( vCurrentVelocity, pParent->GetMatrix().GetCol1() ).Getf() > 0.8f )
				{
					fSpoilerSpeed = SPOILER_SLOW_ROTATE_RATE;
					m_fDesiredFlapAmount[ nSpoiler ] += MAX_SPOILER_PCT_FOR_SPEED;
				}

				// Raise inside spoiler slightly and outside a little more for steering and throttle.
				if( pParent->m_vehControls.GetThrottle() > THROTTLE_THRESHOLD && abs( pParent->m_vehControls.GetSteerAngle() ) > STEERING_THRESHOLD )
				{
					float fSteeringTowardSpoiler = pParent->m_vehControls.GetSteerAngle() * vSpoilerXDir.x;
					if( fSteeringTowardSpoiler < 0 )
					{
						// Outside.
						m_fDesiredFlapAmount[ nSpoiler ] += abs(fSteeringTowardSpoiler) * MAX_SPOILER_PCT_FOR_STEERING_OUTSIDE;
					}
					else
					{
						// Inside.
						m_fDesiredFlapAmount[ nSpoiler ] += fSteeringTowardSpoiler * MAX_SPOILER_PCT_FOR_STEERING;
					}
				}

				// Raise outside and inside if we're spinning and using the throttle.
				if( pParent->m_vehControls.GetThrottle() > THROTTLE_THRESHOLD && Abs( pParent->GetAngVelocity().z ) > SPIN_THRESHOLD )
				{
					float fSpinTowardsSpoiler = pParent->GetAngVelocity().z * vSpoilerXDir.x;	
					if( fSpinTowardsSpoiler < 0 )
					{
						// Outside, based on rate of spin.
						m_fDesiredFlapAmount[ nSpoiler ] += Min( ( abs( pParent->GetAngVelocity().z ) - SPIN_THRESHOLD) / ( 1.0f - SPIN_THRESHOLD ), 1.0f ) * MAX_SPOILER_PCT_FOR_SPIN_OUTSIDE;
					}
					else
					{
						// Inside, based on throttle.
						m_fDesiredFlapAmount[ nSpoiler ] += Min( ( abs( pParent->m_vehControls.GetThrottle() ) - THROTTLE_THRESHOLD ) / ( 1.0f - THROTTLE_THRESHOLD ), 1.0f ) * MAX_SPOILER_PCT_FOR_SPIN_INSIDE;
					}
				}

				// Raise both quickly on braking.
				if( pParent->m_vehControls.GetBrake() > 0.0f && Dot( vCurrentVelocity, pParent->GetMatrix().GetCol1() ).Getf() > 0 )
				{
					fSpoilerSpeed = SPOILER_QUICK_ROTATE_RATE;
					m_fDesiredFlapAmount[ nSpoiler ] = 1.0f;
				}			
			}
			else
			{
				if( pParent->GetDriver() && pParent->GetDriver()->IsPlayer() && !pParent->IsInAir( false ) )
				{
					m_fDesiredFlapAmount[ nSpoiler ] = 0.0f;
				}
				else if( !pParent->GetDriver() || ( pParent->GetDriver() && !pParent->GetDriver()->IsPlayer() ) )
				{
					m_fDesiredFlapAmount[ nSpoiler ] = 0.0f;
				}
				else
				{
					fSpoilerSpeed = 4.0f;
				}
			}

			m_fDesiredFlapAmount[ nSpoiler ] = Clamp( m_fDesiredFlapAmount[ nSpoiler ], 0.0f, 1.0f );

			// Smoothly animate to the desired spoiler angle.
			float fDesiredAngle = m_fDesiredFlapAmount[ nSpoiler ] * MAX_SPOILER_ANGLE;
			float fCurrentSpoilerAngle = pParent->GetObjectMtx( nBoneIndex ).GetEulers().x;
			float fDiff = fDesiredAngle - fCurrentSpoilerAngle;

			if( abs( fDiff ) <= fSpoilerSpeed * fTimeStep )
			{
				fCurrentSpoilerAngle = fDesiredAngle;
			}
			else
			{
				if( fDiff > 0 )
				{
					fCurrentSpoilerAngle += fSpoilerSpeed * fTimeStep;
				}

				if( fDiff < 0 )
				{
					fCurrentSpoilerAngle -= fSpoilerSpeed * fTimeStep;
				}
			}

			// Calculate downforce multiplier for wheels based on how raised each spoiler is.
			for( int nWheel = 0; nWheel < pParent->GetNumWheels(); nWheel++ )
			{
				Vector3 vWheelPos;
				CWheel* pWheel = pParent->GetWheel( nWheel );
				pWheel->GetWheelPosAndRadius( vWheelPos );

				if( vWheelPos.Dot( vSpoilerXDir ) > 0 )
				{
					float fExtraDownforce = SPOILER_DOWNFORCE_MAX - CWheel::ms_fDownforceMult;
					pWheel->m_fDownforceMult = CWheel::ms_fDownforceMult + ( ( fCurrentSpoilerAngle / MAX_SPOILER_ANGLE ) * fExtraDownforce );
				}
			}

			pParent->SetBoneRotation( nBoneIndex, ROT_AXIS_LOCAL_X, fCurrentSpoilerAngle, true );
		}
	}

	m_vPrevVelocity = vCurrentVelocity;
}
