// Title	:	Door.cpp
// Author	:	Bill Henderson
// Started	:	06/01/2000 

// Rage headers
#include "crskeleton/jointdata.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "math/vecmath.h"
#include "fragment/typegroup.h"
#include "phbound/boundcomposite.h"

// Framework headers
#include "fwmaths/angle.h"
#include "fwmaths/vector.h"

// Game headers
#include "animation/AnimBones.h"
#include "audio/vehicleaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/Gameplay/GameplayDirector.h"
#include "camera/cinematic/CinematicDirector.h"
#include "game/ModelIndices.h"
#include "modelInfo/vehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoExtensions.h"
#include "pathserver/PathServer.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "vehicles/Heli.h"
#include "vehicles/vehicle.h"
#include "vehicles/Planes.h"
#include "vehicles/automobile.h"
#include "vehicles/door.h"
#include "vehicles/vehicle_channel.h"
#include "Vfx/VehicleGlass/VehicleGlassManager.h"
#include "task/vehicle/TaskVehicleBase.h"
#include "task/vehicle/TaskEnterVehicle.h"
#include "script/script.h"
#include "control/replay/ReplaySettings.h"

// network headers
#include "network/events/NetworkEventTypes.h"

ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

RAGE_DEFINE_CHANNEL(vehicledoor)

BankFloat bfCarDoorBreakThreshold = 0.0f; // disabled
BankBool bbApplyCarDoorDeformation = true;

eHierarchyId CCarDoor::ms_aScriptDoorIds[SC_DOOR_NUM] =
{
	VEH_DOOR_DSIDE_F,	//SC_DOOR_FRONT_LEFT
	VEH_DOOR_PSIDE_F,	//SC_DOOR_FRONT_RIGHT,
	VEH_DOOR_DSIDE_R,	//SC_DOOR_REAR_LEFT,
	VEH_DOOR_PSIDE_R,	//SC_DOOR_REAR_RIGHT,
	VEH_BONNET,			//SC_DOOR_BONNET,
	VEH_BOOT			//SC_DOOR_BOOT,
};



eHierarchyId CCarDoor::ms_aScriptWindowIds[SC_WINDOW_NUM] =
{
	VEH_WINDOW_LF,	//SC_WINDOW_FRONT_LEFT
	VEH_WINDOW_RF,	//SC_WINDOW_FRONT_RIGHT,
	VEH_WINDOW_LR,	//SC_WINDOW_REAR_LEFT,
	VEH_WINDOW_RR,	//SC_WINDOW_REAR_RIGHT,
	VEH_WINDOW_LM,	//SC_WINDOW_MIDDLE_LEFT,
	VEH_WINDOW_RM,	//SC_WINDOW_MIDDLE_RIGHT,
	VEH_WINDSCREEN,	//SC_WINDSCREEN_FRONT,
	VEH_WINDSCREEN_R//SC_WINDSCREEN_REAR
};

// Helper function which centralises the generation of include flags for car doors.
u32 GenerateUnlatchedCarDoorCollisionIncludeFlags(const CVehicle* pVehicle, eHierarchyId doorId)
{
	u32 nIncludeFlags = ArchetypeFlags::GTA_CAR_DOOR_UNLATCHED_INCLUDE_TYPES;

	//Ensure that the camera shape tests ignore open car doors.
	// - For aircraft, only ignore the front doors.
	// - For gull wing doors, don't ignore any doors, doesn't work with doors going upwards.
	// - For bombushka, don't ignore the stair/door. 

	const bool isAircraft = pVehicle && pVehicle->GetIsAircraft();
	const bool isGullDoorVehicle = pVehicle && pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_GULL_WING_DOORS);
	const bool isBombushka = MI_PLANE_BOMBUSHKA.IsValid() && isAircraft && pVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA.GetModelIndex();
	const bool isVolatol = MI_PLANE_VOLATOL.IsValid() && isAircraft && pVehicle->GetModelIndex() == MI_PLANE_VOLATOL.GetModelIndex();

	if (!isGullDoorVehicle && !isBombushka && !isVolatol)
	{
		if (doorId == VEH_DOOR_DSIDE_F || doorId == VEH_DOOR_PSIDE_F ||
		   (!isAircraft && (doorId == VEH_DOOR_DSIDE_R || doorId == VEH_DOOR_PSIDE_R)))
		{
			nIncludeFlags &= ~ArchetypeFlags::GTA_CAMERA_TEST;
		}
	}

	return nIncludeFlags;
}

#if __BANK
bool CCarDoor::sm_debugDoorPhysics = false;
#endif

CCarDoor::CCarDoor()
{
	m_DoorId = VEH_INVALID_ID;
	m_linkedDoorId = VEH_INVALID_ID;
	m_nFragGroup = -1;
	m_nFragChild = -1;
    m_nBoneIndex = -1;

	m_bJustLatched = false;
	m_bDoorAllowedToBeBrokenOff = true;
	m_bDoorWindowIsRolledDown = false;
	m_bDoorHasBeenKnockedOpen = false;
	m_bForceOpenForNavigation = 0;
	m_bPlayerStandsOnDoor = false;
	m_bBreakOffNextUpdate = false;
	m_bResetComponentToLinkIndex = false;
	m_pedRemoteUsingDoor = false;
	m_overrideDoorTorque = false;
	m_iPathServerDynObjId = DYNAMIC_OBJECT_INDEX_NONE;
	m_nDoorFlags = 0;

	m_fOpenAmount = 0.0f;
	m_fClosedAmount = 0.0f;
	m_fTargetRatio = 0.0f;
	m_fOldTargetRatio = 0.0f;
	m_fCurrentRatio = 0.0f;
	m_fOldRatio = 0.0f;
	m_fCurrentSpeed = 0.0f;
	m_fOldAudioRatio = 0.f;
	m_nLastAudioTime = 0;
	m_nNetworkTimeOpened = 0;
	m_nOpenPastLimitsTime = 0;
	m_fAnglePastLimits = 0;
	m_nPastLimitsTimeThreshold = 0;
	m_vDeformation = VEC3_ZERO;

	m_uTimeSetShutAndLatched = 0;
	m_uTimeSinceBeingUsed = 0;

	m_eDoorLockState = CARLOCK_NONE;

#if __BANK
	for(int i = 0; i < 32; i++)
	{
		m_lastSetFlagsCallstack[i] = 0;
	}
	m_lastSetFlagsScript[0] = 0;
	m_drivenNoResetFlagWasSet = false;
#endif
}

CCarDoor::~CCarDoor()
{
}

void CCarDoor::Init(CVehicle *pParent, eHierarchyId nId, float fOpenAmount, float fClosedAmount, u32 nFlags)
{
	// check door exists first
	if(pParent->GetBoneIndex(nId) == -1 && !(nFlags &BIKE_DOOR))
	{
		return;
	}

	m_DoorId = nId;
	m_nDoorFlags = nFlags;
	m_fOpenAmount = fOpenAmount;
	m_fClosedAmount = fClosedAmount;

	m_fTargetRatio = 0.0f;
	m_fOldTargetRatio = 0.0f;
	m_fCurrentRatio = 0.0f;
	m_fOldRatio = 0.0f;
	m_fCurrentSpeed = 0.0f;

	m_uTimeSetShutAndLatched = 0;
	m_uTimeSinceBeingUsed = 0;

	SetFlag(DRIVEN_SHUT);

    m_nBoneIndex = (s8)pParent->GetBoneIndex(nId);

	if(pParent->GetVehicleFragInst() && m_nBoneIndex > -1)
	{
		m_nFragGroup = (s8)pParent->GetVehicleFragInst()->GetGroupFromBoneIndex(m_nBoneIndex);
		m_nFragChild = (s8)pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(m_nBoneIndex);

		// set flag so this can't break off until we want it to.
		if (m_nFragChild > 0)
		{
			// It's possible that we broke off the door for a vehicle variation before initializing the CCarDoor. 
			if(pParent->GetVehicleFragInst()->GetChildBroken(m_nFragChild))
			{
				SetFlag(IS_BROKEN_OFF);
			}

            if(nFlags &DONT_BREAK || pParent->IsNetworkClone())
            {
                pParent->GetVehicleFragInst()->SetDontBreakFlag(BIT(static_cast<s64>(m_nFragChild)));
            }

			pParent->GetVehicleFragInst()->SetDontBreakFlagAllChildren(m_nFragChild);
		}
	}

	if((nFlags &BIKE_DOOR))
	{
		return;
	}

	const crBoneData* pBoneData = NULL;
	if(pParent->GetSkeleton())
	{
		pBoneData = pParent->GetSkeletonData().GetBoneData(m_nBoneIndex);
	}

	// if this door's bone has dofs then it will be articulated, and we should get the joint limits from the bone
	if(pBoneData && pBoneData->GetDofs() > 0)
	{
		const crJointData* pJointData = pParent->GetDrawable()->GetJointData();
		Assertf(pJointData, "%s has no joint data", pParent->GetModelName());

		Vec3V dofMin, dofMax;
        if(pBoneData->GetDofs() & crBoneData::HAS_TRANSLATE_LIMITS && 
			pJointData->GetTranslationLimits(*pBoneData, dofMin, dofMax) && (dofMin.GetYf()!=0.0f||dofMax.GetYf()!=0.0f))
        {
            ClearFlag(AXIS_MASK);
            SetFlag(AXIS_SLIDE_Y|IS_ARTICULATED);

            if(rage::Abs(dofMax.GetYf()) > rage::Abs(dofMin.GetYf()))
            {
                m_fOpenAmount = dofMax.GetYf();
                m_fClosedAmount = dofMin.GetYf();
            }
            else
            {
                m_fOpenAmount = dofMin.GetYf();
                m_fClosedAmount = dofMax.GetYf();
            }
        }
		// only allowed to rotate in one axis, search in the order z, x, y
		else if(pBoneData->GetDofs() &crBoneData::HAS_ROTATE_LIMITS && pBoneData->GetDofs() &crBoneData::ROTATE_Z &&
			pJointData->ConvertToEulers(*pBoneData, dofMin, dofMax) && (dofMax.GetZf()!=0.0f || dofMin.GetZf()!=0.0f))
		{
			// reset axis mask that was passed in
			ClearFlag(AXIS_MASK);
			SetFlag(AXIS_Z|IS_ARTICULATED);

			if(rage::Abs(dofMax.GetZf()) > rage::Abs(dofMin.GetZf()))
			{
				m_fOpenAmount = dofMax.GetZf();
				m_fClosedAmount = dofMin.GetZf();
			}
			else
			{
				m_fOpenAmount = dofMin.GetZf();
				m_fClosedAmount = dofMax.GetZf();
			}
		}
		else if(pBoneData->GetDofs() &crBoneData::HAS_ROTATE_LIMITS && pBoneData->GetDofs() &crBoneData::ROTATE_X &&
		pJointData->ConvertToEulers(*pBoneData, dofMin, dofMax) && (dofMax.GetXf()!=0.0f || dofMin.GetXf()!=0.0f))
		{
			// reset axis mask that was passed in
			ClearFlag(AXIS_MASK);
			SetFlag(AXIS_X|IS_ARTICULATED);

			if(rage::Abs(dofMax.GetXf()) > rage::Abs(dofMin.GetXf()))
			{
				if( dofMax.GetXf() > 0.0f &&  // GTAV HACK - FIX B*2572335 - The back door on the rebel and rebel 2 fall off when opened.
					dofMin.GetXf() > 0.0f )		// I spawned every vehicle in the game and it is only those 2 that use this bit.
				{
					m_fOpenAmount	= dofMin.GetXf();
					m_fClosedAmount = 0.0f;
				}
				else
				{
					m_fOpenAmount = dofMax.GetXf();
					m_fClosedAmount = dofMin.GetXf();
				}
			}
			else
			{
				m_fOpenAmount = dofMin.GetXf();
				m_fClosedAmount = dofMax.GetXf();
			}
		}
		else if(pBoneData->GetDofs() &crBoneData::HAS_ROTATE_LIMITS && pBoneData->GetDofs() &crBoneData::ROTATE_Y &&
		pJointData->ConvertToEulers(*pBoneData, dofMin, dofMax) && (dofMax.GetYf()!=0.0f || dofMin.GetYf()!=0.0f))
		{
			// reset axis mask that was passed in
			ClearFlag(AXIS_MASK);
			SetFlag(AXIS_Y|IS_ARTICULATED);

			if(rage::Abs(dofMax.GetYf()) > rage::Abs(dofMin.GetYf()))
			{
				m_fOpenAmount = dofMax.GetYf();
				m_fClosedAmount = dofMin.GetYf();
			}
			else
			{
				m_fOpenAmount = dofMin.GetYf();
				m_fClosedAmount = dofMax.GetYf();
			}
		}
	}

	if( m_fOpenAmount != 0.0f &&
		m_fClosedAmount != 0.0f )
	{
		if( Abs( m_fClosedAmount ) > Abs( m_fOpenAmount ) )
		{
			m_fOpenAmount = 0.0f;
		}
		else
		{
			m_fClosedAmount = 0.0f;
		}
	}

	// if door doesn't have any physics it can't be articulated
	// though it's still fine to have pulled the open/close angles from the bone
	if(m_nFragGroup==-1 || m_nFragChild==-1)
	{
		ClearFlag(IS_ARTICULATED);
	}
}


dev_float DOOR_SWINGING_STIFFNESS = 0.01f;
dev_float DOOR_GAS_MULT = 0.5f;
dev_float DOOR_GAS_TARGET = 0.3f;
dev_float DOOR_ANGLE_STIFFNESS = 0.05f;
dev_float DOOR_SLIDE_MULT = 5.0F;
dev_float TRAILER_DOOR_STIFFNESS = 0.1f;
dev_float TRAILER_DOOR_ANGLE_TORQUE_OR_FORCE_MAX = 5000.0f;
dev_float LARGE_REAR_RAMP_DOOR_TORQUE_OR_FORCE_MAX = 350000.0f;
dev_float HIGHER_DOOR_TORQUE_OR_FORCE_MAX = 10000.0f;
dev_float DOOR_POSITION_STRENGTH = 50.0f;
dev_float DOOR_ANGLE_TORQUE_OR_FORCE_MAX = 500.0f;
dev_bool  DOOR_SPEED_TEST = true;
dev_float DOOR_SPEED_STRENGTH = 10.0f;
dev_float LARGE_BOOT_MULTIPLIER = 2.0f;
dev_float TRAILER_BONNET_AND_BOOT_MULTIPLIER = 10.0f;
dev_float REAR_BOOT_BLOWN_OPEN_MULTIPLIER = 5.0f;
dev_float AERO_BONNET_TORQUE_MULT = 10.0f;
dev_float sfRatioSpeedToLatch = 1.0f;
dev_float sfRatioSpeedToBreakBonnet = -2.0f;
dev_float sfRatioSpeedToBreakRearBonnet = -6.0f;
dev_float sfFwdSpeedToBreakBonnet = 20.0f;
dev_float sfBonnetBreakSpdImpulse = -0.85f;
dev_float sfBonnetBreakUpImpulse = 0.3f;
dev_float sfBonnetBreakFwdOffset = 0.5f;
dev_float sfBonnetBreakSideOffset = 0.4f;
dev_float sfDoorUnlatchedAngle = 0.05f; 
dev_float HELI_SLIDE_DOOR_MAX_SPEED = 1.0f;
dev_float LARGE_REAR_RAMP_DOOR_MAX_SPEED = 0.5f;
dev_float ANIMATED_LARGE_REAR_RAMP_DOOR_MAX_SPEED = 0.33f;
dev_float LARGE_REAR_RAMP_DOOR_ANGLE_TORQUE_OR_FORCE_MAX = 400.0f;
dev_float FOLDING_WING_DOOR_ANGLE_TORQUE_OR_FORCE_MAX = 1000.0f;
dev_float LARGE_REAR_RAMP_DOOR_STIFFNESS = 0.5f;
dev_float BENSON_RAMP_DOOR_CLOSING_MAX_SPEED = 0.5f;
dev_float BENSON_RAMP_DOOR_CLOSING_ANGLE_TORQUE_OR_FORCE_MAX = 100.0f;
dev_float CHERNOBOG_BONNET_TORQUE = 800.0f;
dev_float BENSON_RAMP_DOOR_CLOSING_STIFFNESS = 0.5f;
dev_float sfDoorRatioSleepThreshold = 0.08f;
dev_float sfDoorCloneReachedTargetTollerance = 0.04f;
//

void OffsetIgnusDoors(CVehicle* pParent, int nBound, float xOffset)
{
	phBoundComposite* pThisBoundComposite = static_cast<phBoundComposite*>(pParent->GetVehicleFragInst()->GetArchetype()->GetBound());
	phBoundComposite* pTemplateBoundComposite = pParent->GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds();

	Assert(pThisBoundComposite);
	Assert(pTemplateBoundComposite);

	Mat34V_ConstRef mTemplate = pTemplateBoundComposite->GetCurrentMatrix(nBound);
	Mat34V_ConstRef m = pThisBoundComposite->GetCurrentMatrix(nBound);

	Vec3V pos = mTemplate.GetCol3();

	Vec3V xOffsetV(xOffset, 0.0f, 0.0f);
	pos = Add(pos, xOffsetV);
	(const_cast<Mat34V*>(&m))->SetCol3(pos);
}

void CCarDoor::ProcessPhysics(CVehicle* pParent, float fTimeStep, int nTimeSlice)
{
#if __BANK
	bool wasSetToOpen = false;
	bool wasOpenToStart = !GetIsClosed();

	if(sm_debugDoorPhysics && GetFlag(SET_TO_FORCE_OPEN))
	{
		wasSetToOpen = true;
		physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door should be forced open. Current ratio: %f | Target ratio: %f .\n", pParent->GetDebugName(), m_fCurrentRatio, m_fTargetRatio);
		PrintDoorFlags( pParent );
	}
#endif

	CVehicleModelInfo* pVehModelInfo = pParent->GetVehicleModelInfo();
	if (NetworkInterface::IsGameInProgress() && pParent->GetVehicleModelInfo())
	{
		fwModelId modelId = CModelInfo::GetModelIdFromName( pParent->GetModelNameHash() );		 
		if ((modelId == MI_CAR_IGNUS2 || modelId == MI_CAR_IGNUS))
		{		
			int door_DSIDE_BoundIndex = -1;
			int door_PSIDE_BoundIndex = -1;

			int iBoneIndex_DSIDE = pVehModelInfo->GetBoneIndex(VEH_DOOR_DSIDE_F);
			if (iBoneIndex_DSIDE > -1)
			{
				door_DSIDE_BoundIndex = pVehModelInfo->GetFragType()->GetComponentFromBoneIndex(0, iBoneIndex_DSIDE);
			}
			int iBoneIndex_PSIDE = pVehModelInfo->GetBoneIndex(VEH_DOOR_PSIDE_F);
			if (iBoneIndex_PSIDE > -1)
			{
				door_PSIDE_BoundIndex = pVehModelInfo->GetFragType()->GetComponentFromBoneIndex(0, iBoneIndex_PSIDE);
			}

			Assert(door_DSIDE_BoundIndex != -1);
			Assert(door_PSIDE_BoundIndex != -1);

			if (door_PSIDE_BoundIndex != -1 && door_DSIDE_BoundIndex != -1)
			{
				float PSIDE_Offset = 0.12f;
				float DSIDE_Offset = -0.12f;
				float mul = m_fCurrentRatio < 0.2f ? 0.0f : 1.0f;		// if door is closed dont apply offset				
				OffsetIgnusDoors(pParent, door_DSIDE_BoundIndex, DSIDE_Offset * mul);
				OffsetIgnusDoors(pParent, door_PSIDE_BoundIndex, PSIDE_Offset * mul);
			}
		}
	}

	if( NetworkInterface::IsGameInProgress() &&
		!pParent->IsNetworkClone() )
	{
		ClearFlag( DRIVEN_NORESET_NETWORK );
	}

	// reset this flag before we try and drive doors
    ClearFlag(PROCESS_FORCE_AWAKE);

	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	// check that we have a frag inst, and that it's the inst driving the car (i.e. it's not using dummy car physics)
	if(pFragInst==NULL || !pFragInst->IsInLevel() || pFragInst!=pParent->GetCurrentPhysicsInst())
		pFragInst = NULL;

	if(GetFlag(IS_ARTICULATED) && pFragInst && GetIsIntact(pParent))
	{		
		if(m_bBreakOffNextUpdate)
		{
			m_bBreakOffNextUpdate = false;

			BreakOff(pParent);
		}

		if(GetFlag(DRIVEN_SHUT))
		{
			if(CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex()) && !GetIsLatched(pParent))
			{
				ClearFlag(DRIVEN_SHUT);
				if(GetFlag(GAS_BOOT))
				{
					vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--DRIVEN_SHUT-GAS_BOOT-->SetTargetDoorOpenRatio(DOOR_GAS_TARGET, DRIVEN_GAS_STRUT)", pParent->GetLogName() );
					SetTargetDoorOpenRatio(DOOR_GAS_TARGET, DRIVEN_GAS_STRUT);
				}
			}
		}

		if(GetFlag(DRIVEN_AUTORESET) && GetFlag(WILL_LOCK_DRIVEN) && GetIsLatched(pParent))
		{
			ClearFlag(WILL_LOCK_DRIVEN);
			ClearFlag(DRIVEN_MASK);
			SetFlag(DRIVEN_SHUT);
			vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--(GetFlag(DRIVEN_AUTORESET) && GetFlag(WILL_LOCK_DRIVEN) && GetIsLatched(pParent))-->SetFlag(DRIVEN_SHUT)", pParent->GetLogName() );
		}

		if(!GetFlag(DRIVEN_SHUT))
		{
#if __BANK					
			if(CCarDoor::sm_debugDoorPhysics && wasSetToOpen)
			{
				physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door not being driven shut.\n", pParent->GetDebugName());
				PrintDoorFlags( pParent );
			}
#endif

			// if not driven, then let the car stay asleep
			const u32 drivingDoorFlags = DRIVEN_AUTORESET | DRIVEN_NORESET | SET_TO_FORCE_OPEN | DRIVEN_NORESET_NETWORK;
			if(!GetFlag(drivingDoorFlags) && !CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex()))
			{
#if __BANK					
				if(CCarDoor::sm_debugDoorPhysics && rage::Abs(m_fCurrentSpeed) > 0.0f)
				{
					physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door going to sleep; not being driven shut, not being forced open.\n", pParent->GetDebugName());
					PrintDoorFlags( pParent );
				}
#endif
				m_fCurrentSpeed = 0.0f;//door speed should be 0 when asleep
				return;
			}

			// find the associated joint
			fragHierarchyInst* pHierInst = NULL;
			if(pFragInst && pFragInst->GetCached())
				pHierInst = pFragInst->GetCacheEntry()->GetHierInst();

			phArticulatedCollider* pArticulatedCollider = NULL;
			if(pHierInst)
				pArticulatedCollider = pHierInst->articulatedCollider;

			// B*1914885: The physics inst can get moved while the collider is not available (inactive), which means that moving doors could give incorrect results as the collider would be out of sync.
			// If we have a collider from the frag inst, but not from the parent vehicle, manually ensure the collider matrix is synced to the inst matrix in case something moved the inst prior to door physics being run (e.g. A cutscene fixup).
			if( pArticulatedCollider && 
				!pParent->GetCollider() )
			{
				if( pParent->m_nVehicleFlags.bAnimateJoints ) // B*2721547 - only do this if we are animating the vehicle. For some vehicles sometimes this can cause the driver to drift away from the vehicle
				{
					pArticulatedCollider->SetColliderMatrixFromInstance();
				}
			}

			phJoint1Dof* p1DofDoorJoint = NULL;
            phPrismaticJoint* pPrismDoorJoint = NULL;
            phJoint* pJoint = NULL;
			// If the vehicle isn't articulated then it can't have any door joints
			if(pArticulatedCollider && pArticulatedCollider->IsArticulated() && m_nFragChild > 0)
			{
				int linkIndex = pArticulatedCollider->GetLinkFromComponent(m_nFragChild);
				if(linkIndex > 0)
				{
					pJoint = &pHierInst->body->GetJoint(linkIndex - 1);

					if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
					{
						p1DofDoorJoint = static_cast<phJoint1Dof*>(pJoint);
					}
                    else if (pJoint && pJoint->GetJointType() == phJoint::PRISM_JNT)
					{
                        pPrismDoorJoint = static_cast<phPrismaticJoint*>(pJoint);
					}
				}
			}

			if( pParent->IsNetworkClone() &&
				pJoint &&
				!CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex() ) )
			{
				// keep track of current door ratio
				float fCurrentRatio = 0.0f;
				if(p1DofDoorJoint)
				{
					fCurrentRatio = p1DofDoorJoint->GetAngle(pHierInst->body);
				}
				else if (pPrismDoorJoint)
				{
					fCurrentRatio = pPrismDoorJoint->ComputeCurrentTranslation(pHierInst->body);
				}

				fCurrentRatio = ComputeRatio( fCurrentRatio );
				fCurrentRatio = ClampDoorOpenRatio( fCurrentRatio );

				if( Abs( m_fTargetRatio - fCurrentRatio ) < sfDoorCloneReachedTargetTollerance )
				{
					return;
				}
			}


			eHierarchyId eHierarchy = GetHierarchyId();
			bool bSpecialDoors = pParent->GetVehicleModelInfo() && pParent->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DAMPEN_STICKBOMB_DAMAGE ) && ( eHierarchy == VEH_DOOR_DSIDE_R || eHierarchy == VEH_DOOR_PSIDE_R );
			if( ( !bSpecialDoors || GetFlag( DRIVEN_SPECIAL ) ) && GetIsLatched( pParent ) )
			{
				vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--( ( !bSpecialDoors || GetFlag( DRIVEN_SPECIAL ) ) && GetIsLatched( pParent ) )-->BreakLatch(pParent)", pParent->GetLogName() );
				BreakLatch(pParent);
			}

			if( p1DofDoorJoint &&
				GetAxis() == AXIS_Z &&
				!pParent->IsVisible() )
			{
				Vector3 rotationAxis = p1DofDoorJoint->GetRotationAxis();

				if( Abs( rotationAxis.GetZ() ) < 
					Abs( rotationAxis.GetX() ) )
				{
					pFragInst->SyncSkeletonToArticulatedBody();
				}
			}

			if(GetFlag(DRIVEN_AUTORESET | DRIVEN_NORESET | DRIVEN_GAS_STRUT | DRIVEN_NORESET_NETWORK))
			{
#if __BANK					
				if(CCarDoor::sm_debugDoorPhysics && wasSetToOpen)
				{
					physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door DRIVEN_AUTORESET | DRIVEN_NORESET_NETWORK | DRIVEN_NORESET | DRIVEN_GAS_STRUT.\n", pParent->GetDebugName());
					PrintDoorFlags( pParent );
				}
#endif

				if(pJoint)
				{
					vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--DRIVEN_AUTORESET|DRIVEN_NORESET|DRIVEN_NORESET_NETWORK|DRIVEN_GAS_STRUT|DRIVEN_NORESET_NETWORK && pJoint -- m_fCurrentRatio[%f] m_fTargetRatio[%f] m_fOpenAmount[%f] m_fClosedAmount[%f]", pParent->GetLogName(), m_fCurrentRatio,m_fTargetRatio,m_fOpenAmount,m_fClosedAmount);

					float fTarget = m_fTargetRatio * m_fOpenAmount + (1.0f - m_fTargetRatio) * m_fClosedAmount;

                    if(p1DofDoorJoint)
                    {
					    p1DofDoorJoint->SetMuscleTargetAngle(fTarget);
                    }
                    else if(pPrismDoorJoint)
                    {
                        pPrismDoorJoint->SetMuscleTargetPosition(fTarget);
                    }

					float fMult = GetFlag(DRIVEN_GAS_STRUT) ? DOOR_GAS_MULT : 1.0f;
					
                    if( eHierarchy==VEH_BOOT && m_fOpenAmount < 0.0f)//if this is a boot and opens swinging outwards increase the multiplier
                    {
                        fMult = LARGE_BOOT_MULTIPLIER;
                    }

					if( GetAxis() == AXIS_SLIDE_Y &&
						pParent->GetVehicleType() == VEHICLE_TYPE_HELI )
					{
						fMult = DOOR_SLIDE_MULT;
					}

					if( MI_SUB_AVISA.IsValid() && pParent->GetModelIndex() == MI_SUB_AVISA)
					{
						fMult = LARGE_BOOT_MULTIPLIER;// The Avisa doors open like a boot
					}

					// Check if we have any stiffness multipliers for this door.
					float fDoorStiffnessMult = 1.0f;
					if(GetFlag(DRIVEN_USE_STIFFNESS_MULT))
					{
						if(CVehicleModelInfo* pVehicleModelInfo = pParent->GetVehicleModelInfo())
						{
							CVehicleModelInfoDoors* pOtherVehicleDoorExtension = pVehicleModelInfo->GetExtension<CVehicleModelInfoDoors>();
							if(pOtherVehicleDoorExtension)
							{
								fDoorStiffnessMult = pOtherVehicleDoorExtension->GetStiffnessMultForThisDoor(eHierarchy);
							}
						}
					}

                    float jointStiffness = fMult * DOOR_ANGLE_STIFFNESS * fDoorStiffnessMult;
                    float jointMaxTorqueOrForce = fMult * DOOR_ANGLE_TORQUE_OR_FORCE_MAX;
                    float fDoorMaxSpeed = 5.9f*PI;

                    if( (eHierarchy==VEH_BOOT || eHierarchy==VEH_BONNET) && pParent->GetVehicleType() == VEHICLE_TYPE_TRAILER)
                    {
						fMult = TRAILER_BONNET_AND_BOOT_MULTIPLIER;

						if(MI_TRAILER_TRAILERLARGE.IsValid() && 
							pParent->GetModelIndex() == MI_TRAILER_TRAILERLARGE )
						{
							//Trailer large rear door can be very heavy so increase the max force the doors can use
							jointMaxTorqueOrForce = LARGE_REAR_RAMP_DOOR_TORQUE_OR_FORCE_MAX;
							jointStiffness = LARGE_REAR_RAMP_DOOR_STIFFNESS;
							fDoorMaxSpeed = LARGE_REAR_RAMP_DOOR_MAX_SPEED;
						}
						else
						{
							// Trailers sometimes have to lift up cars on their doors, so massively increase the stiffness and max torque
							jointStiffness = TRAILER_DOOR_STIFFNESS;
							jointMaxTorqueOrForce = TRAILER_DOOR_ANGLE_TORQUE_OR_FORCE_MAX;

							fDoorMaxSpeed = 1.0f;
						}
                    }
					else if(pParent->GetVehicleType() == VEHICLE_TYPE_HELI )
					{
						if( ( (CHeli *)pParent)->GetIsCargobob() && ( eHierarchy==VEH_BOOT || eHierarchy==VEH_DOOR_DSIDE_R ) )
						{
							bool overrideDoorTorque = m_overrideDoorTorque && !pParent->IsNetworkClone();

							if( !overrideDoorTorque &&
								( !pParent->IsNetworkClone() ||
								  !pParent->IsInAir() ) )
							{
								// Make heli ramp door stiffer and open slower
								jointStiffness = LARGE_REAR_RAMP_DOOR_STIFFNESS;
								jointMaxTorqueOrForce = LARGE_REAR_RAMP_DOOR_ANGLE_TORQUE_OR_FORCE_MAX;
								fDoorMaxSpeed = LARGE_REAR_RAMP_DOOR_MAX_SPEED;
							}
						}
						else if( GetAxis() == AXIS_SLIDE_Y )
						{
							fDoorMaxSpeed = HELI_SLIDE_DOOR_MAX_SPEED;
						}
						else if( m_DoorId == VEH_FOLDING_WING_L ||
								 m_DoorId == VEH_FOLDING_WING_R )
						{
							jointStiffness = LARGE_REAR_RAMP_DOOR_STIFFNESS;
							jointMaxTorqueOrForce = FOLDING_WING_DOOR_ANGLE_TORQUE_OR_FORCE_MAX;
						}
					}
                    else if( pParent->GetVehicleType() == VEHICLE_TYPE_PLANE || pParent->GetVehicleType() == VEHICLE_TYPE_SUBMARINE )
                    {
                        //planes can be very heavy so increase the max force the doors can use
                        jointMaxTorqueOrForce = LARGE_REAR_RAMP_DOOR_TORQUE_OR_FORCE_MAX;
						if( !pParent->IsNetworkClone() &&
							( eHierarchy==VEH_BOOT || pParent->m_nVehicleFlags.bUsesLargeRearRamp) )
						{
							jointMaxTorqueOrForce = LARGE_REAR_RAMP_DOOR_ANGLE_TORQUE_OR_FORCE_MAX;
							jointStiffness = LARGE_REAR_RAMP_DOOR_STIFFNESS;
							fDoorMaxSpeed = LARGE_REAR_RAMP_DOOR_MAX_SPEED;
						}
                    }
					else if(pParent->GetModelIndex() == MI_CAR_BENSON_TRUCK && eHierarchy==VEH_BOOT && m_fTargetRatio == 0.0f)
					{
						jointStiffness = BENSON_RAMP_DOOR_CLOSING_STIFFNESS;
						jointMaxTorqueOrForce = BENSON_RAMP_DOOR_CLOSING_ANGLE_TORQUE_OR_FORCE_MAX;
						fDoorMaxSpeed = BENSON_RAMP_DOOR_CLOSING_MAX_SPEED;
					}
					else if(pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_HIGHER_DOOR_TORQUE))
					{
						//tanks are very heavy so increase the max force the doors can use
						jointMaxTorqueOrForce = HIGHER_DOOR_TORQUE_OR_FORCE_MAX;
					}
					// Check to see if this is an explosive opening of the rear doors
					else if( bSpecialDoors && GetFlag( DRIVEN_SPECIAL ) )
					{
						fMult = REAR_BOOT_BLOWN_OPEN_MULTIPLIER;
						pParent->m_nVehicleFlags.bRearDoorsHaveBeenBlownOffByStickybomb = true;
					}
                    else if( eHierarchy==VEH_BONNET && 
                             pParent->GetModelIndex() == MI_CAR_CHERNOBOG )
                    {
                        jointStiffness = LARGE_REAR_RAMP_DOOR_STIFFNESS;
                        jointMaxTorqueOrForce = CHERNOBOG_BONNET_TORQUE;
                        fDoorMaxSpeed = LARGE_REAR_RAMP_DOOR_MAX_SPEED;
                    }
					
                    //Set max torque and stiffness of joints
                    pJoint->SetStiffness(jointStiffness);
                    if(p1DofDoorJoint)
                    {
                        p1DofDoorJoint->SetMinAndMaxMuscleTorque(jointMaxTorqueOrForce);
                        p1DofDoorJoint->SetMuscleAngleStrength(fMult * DOOR_POSITION_STRENGTH);
                    }
                    else if(pPrismDoorJoint)
                    {
                        pPrismDoorJoint->SetMinAndMaxMuscleForce(jointMaxTorqueOrForce);
                        pPrismDoorJoint->SetMusclePositionStrength(fMult * DOOR_POSITION_STRENGTH);
                    }

                    
					// want to scale strengths by the angular inertia of each door, so we get consistent results with different sizes of doors
					//Matrix34 matInertia;
					//pArticulatedCollider->GetBody()->GetInertiaMatrix(matInertia, 
					
					//Airplane doors are torque/force driven, as they are much heavier and can not open/close as fast as car doors
					if(DOOR_SPEED_TEST)
					{
                        float fOldTargetRatio = m_fOldTargetRatio;

                        float fOldTarget = fOldTargetRatio * m_fOpenAmount + (1.0f - m_fTargetRatio) * m_fClosedAmount;
						float fTargetSpeed = (fTarget - fOldTarget)* 0.5f / fTimeStep;
						fTargetSpeed = rage::Clamp(fTargetSpeed, -fDoorMaxSpeed, fDoorMaxSpeed);

                
                        if(p1DofDoorJoint)
                        {        
                            p1DofDoorJoint->SetMuscleTargetSpeed(fTargetSpeed);
                            p1DofDoorJoint->SetMuscleSpeedStrength(fMult * DOOR_SPEED_STRENGTH);
                            p1DofDoorJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
                        }
                        else if(pPrismDoorJoint)
                        {
                            pPrismDoorJoint->SetMuscleTargetSpeed(fTargetSpeed);
                            pPrismDoorJoint->SetMuscleSpeedStrength(fMult * DOOR_SPEED_STRENGTH);
                            pPrismDoorJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);
                        }

					}
					else
                    {
                        pJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE);
                    }

					if( pParent->GetIsAnyFixedFlagSet() ||
						NeedsToAnimateOpen( pParent ) )
					{
						float fCurrent = 0.0f;
						
						if( !NeedsToAnimateOpen( pParent ) )
						{
							fCurrent = m_fTargetRatio * m_fOpenAmount + (1.0f - m_fCurrentRatio) * m_fClosedAmount;
						}
						else
						{
							pParent->m_forceUpdateBoundsAndBodyFromSkeleton = true;

							vehicledoorDebugf2("CCarDoor::ProcessPhysics--Forcing animate open" );

							if( GetIsLatched( pParent ) )
							{
								vehicledoorDebugf2("CCarDoor::ProcessPhysics--Breaking latch" );
								BreakLatch(pParent);
							}

							fCurrent = ( m_fTargetRatio > m_fCurrentRatio ) ? 1.0f : -1.0f;
							fCurrent *= ANIMATED_LARGE_REAR_RAMP_DOOR_MAX_SPEED;

							fCurrent *= fTimeStep;
							fCurrent += m_fCurrentRatio;
							fCurrent = Clamp( fCurrent, 0.0f, 1.0f );
							fCurrent = fCurrent * m_fOpenAmount + (1.0f - fCurrent) * m_fClosedAmount;
						}
						vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--GetIsAnyFixedFlagSet-->BreakLatch(pParent)-->DriveDoorOpen fCurrent[%f] m_fTargetRatio[%f] m_fCurrentRatio[%f]", pParent->GetLogName(),fCurrent,m_fTargetRatio,m_fCurrentRatio);
						DriveDoorOpen(fCurrent, pJoint, pParent, pArticulatedCollider); 
					}
				}
			}
			else
			{
				if(GetFlag(SET_TO_FORCE_OPEN))
				{
					vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--GetFlag(SET_TO_FORCE_OPEN)-->BreakLatch(pParent)-->SetOpenAndUnlatched", pParent->GetLogName() );
#if __BANK					
					if(CCarDoor::sm_debugDoorPhysics)
					{
						physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door about to be SetOpenAndUnlatched.\n", pParent->GetDebugName());
						PrintDoorFlags( pParent );
					}
#endif
					SetOpenAndUnlatched(pParent, CCarDoor::RELEASE_AFTER_DRIVEN);

					if(!pParent->IsVisible())// make sure we have updated the skeleton even if we aren't visible when doors are forced to be open.
					{
						fragInst* pFragInst = pParent->GetFragInst(); 
						if (pFragInst && CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex()) && pParent->IsColliderArticulated() && !pParent->InheritsFromTrain() && !pParent->InheritsFromDraftVeh())
						{
							pFragInst->SyncSkeletonToArticulatedBody();
						}
					}

					ClearFlag(SET_TO_FORCE_OPEN);

#if __BANK					
					if(sm_debugDoorPhysics)
					{
						physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Post SetOpenAndUnlatched. Current ratio: %f | Target ratio: %f .\n", pParent->GetDebugName(), m_fCurrentRatio, m_fTargetRatio);
						PrintDoorFlags( pParent );
					}
#endif
				}

				if(pJoint && !GetFlag(WILL_LOCK_DRIVEN))
				{
					pJoint->SetDriveState(phJoint::DRIVE_STATE_FREE);
					pJoint->SetStiffness(DOOR_SWINGING_STIFFNESS);

					if(GetFlag(AERO_BONNET) && p1DofDoorJoint)// rotating bonnets are only supported
					{
						float fFwdSpeed = DotProduct(VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()), pParent->GetVelocity());
						p1DofDoorJoint->ApplyTorque(pHierInst->body, fFwdSpeed * AERO_BONNET_TORQUE_MULT, ScalarV(fTimeStep).GetIntrin128ConstRef());
					}
				}
			}

			if(pJoint)
			{
				static bool onlyUpdateCurrentRatioWhenNotForcingAnim = true;
				float unclampedRatio = m_fCurrentRatio;

				// keep track of current door ratio
				float fOldRatio = m_fCurrentRatio;
				float fOldSpeed = m_fCurrentSpeed / (m_fOpenAmount - m_fClosedAmount);

				float fCurrentPosition = 0.0f;
				if(p1DofDoorJoint)
				{
					fCurrentPosition = p1DofDoorJoint->GetAngle(pHierInst->body);
					m_fCurrentSpeed = p1DofDoorJoint->GetAngularSpeed(pHierInst->body);
				}
				else if (pPrismDoorJoint)
				{
					fCurrentPosition = pPrismDoorJoint->ComputeCurrentTranslation(pHierInst->body);
					m_fCurrentSpeed = pPrismDoorJoint->ComputeVelocity(pHierInst->body);
				}
                
				unclampedRatio = ComputeRatio(fCurrentPosition);

				if( onlyUpdateCurrentRatioWhenNotForcingAnim &&
					NeedsToAnimateOpen( pParent, true ) &&
					m_fTargetRatio >= 0.5f )
				{
					float newClampedRatio = ClampDoorOpenRatio(unclampedRatio);

					if( newClampedRatio > m_fCurrentRatio )
					{
						m_fCurrentRatio = newClampedRatio;
					}
				}
				else
				{
					m_fCurrentRatio = ClampDoorOpenRatio(unclampedRatio);
				}

				vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--pJoint m_fCurrentRatio[%f]", pParent->GetLogName(),m_fCurrentRatio);
				

				static const float sfminConsiderClosed = 0.09f;
				static const float sfminDepthToBeClosed = -0.01f;
				bool networkClone = pParent->IsNetworkClone();

				if( m_DoorId == VEH_FOLDING_WING_L || 
					m_DoorId == VEH_FOLDING_WING_R )
				{
					if( m_fTargetRatio == 1.0f &&
						Abs( m_fTargetRatio - m_fCurrentRatio ) < 0.05f )
					{
						int childIndex = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex( pParent->GetBoneIndex( m_DoorId ) );
						pParent->Freeze1DofJointInCurrentPosition( pFragInst, childIndex );
					}
					else if( m_fTargetRatio == 0.0f )
					{
						int childIndex = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex( pParent->GetBoneIndex( m_DoorId ) );
						pParent->Freeze1DofJointInCurrentPosition( pFragInst, childIndex, false );
					}
				}

				if(GetFlag(WILL_LOCK_SWINGING) && m_fCurrentRatio <= sfminConsiderClosed && fOldSpeed < -sfRatioSpeedToLatch)
				{
					vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--WILL_LOCK_SWINGING-->SetShutAndLatched 1", pParent->GetLogName() );
					ClearFlag(WILL_LOCK_SWINGING);
					SetShutAndLatched(pParent);
					m_bJustLatched = true;
				}
				else if(GetFlag(WILL_LOCK_SWINGING) && unclampedRatio <= sfminDepthToBeClosed && ( !m_bResetComponentToLinkIndex || m_fTargetRatio == 0.0f ) )
				{
					vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--WILL_LOCK_SWINGING-->SetShutAndLatched 2", pParent->GetLogName() );
					ClearFlag(WILL_LOCK_SWINGING);
					SetShutAndLatched(pParent);
					m_bJustLatched = true;
				}
				else if(GetFlag(WILL_LOCK_DRIVEN) && (m_fCurrentRatio <= sfminConsiderClosed) && (!NetworkInterface::IsGameInProgress() || (m_fTargetRatio <= sfminConsiderClosed)) ) //MP Only - include target ration in consideration because target might be 1.0 but when vehicle is created might have m_fCurrentRatio of 0.0f and have this flag set which will incorrectly lock it closed - lavalley
				{
					vehicledoorDebugf2("CCarDoor::ProcessPhysics--%s--WILL_LOCK_DRIVEN-->SetShutAndLatched m_fCurrentRatio[%f] m_fTargetRatio[%f] sfminConsiderClosed[%f]", pParent->GetLogName(),m_fCurrentRatio,m_fTargetRatio,sfminConsiderClosed);
					ClearFlag(WILL_LOCK_DRIVEN);
					SetShutAndLatched(pParent);
					m_bJustLatched = true;
				}
				else if( GetFlag( RELEASE_AFTER_DRIVEN ) )
				{
					if( Abs( m_fTargetRatio - m_fCurrentRatio ) < 0.001f )
					{
						SetSwingingFree(pParent);
						ClearFlag( WILL_LOCK_SWINGING );
						ClearFlag( WILL_LOCK_DRIVEN );

						ClearFlag( RELEASE_AFTER_DRIVEN );

#if __BANK					
						if(sm_debugDoorPhysics && wasSetToOpen)
						{
							physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door set to be swinging after being forced open.\n", pParent->GetDebugName());
							PrintDoorFlags( pParent );	
						}
#endif
					}
				}
				else if(!GetFlag(DONT_BREAK) && !networkClone && GetDoorAllowedToBeBrokenOff())
				{
					if(m_fCurrentRatio > 0.5f && GetFragChild() > 0)
					{
						pParent->GetVehicleFragInst()->ClearDontBreakFlagAllChildren(GetFragChild());
					}

					if(GetFlag(AERO_BONNET) && m_fCurrentRatio >= 0.95f)
					{
						// break off bonnet
						float fFwdSpeed = DotProduct(VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()), pParent->GetVelocity());

						// Only break the bonnet off for moving too fast if the user isn't trying to open it. This allows us to have a higher
						//   joint strength without always breaking it off. 
						bool drivingBonnetOpen = (m_fTargetRatio >= 0.95f) && GetFlag(drivingDoorFlags);
						float bonnetSpeed = (fOldRatio - m_fCurrentRatio)/fTimeStep;
						float ratioSpeedtoBreakBonnet =  pParent->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_FRONT_BOOT ) ? sfRatioSpeedToBreakRearBonnet : sfRatioSpeedToBreakBonnet;
						bool bonnetMovingTooFast = bonnetSpeed < ratioSpeedtoBreakBonnet;
						// Always allow the bonnet to break if the vehicle is moving too fast, regardless of if we're driving it open
						bool vehicleMovingTooFast = fFwdSpeed > sfFwdSpeedToBreakBonnet;
						if((!drivingBonnetOpen && bonnetMovingTooFast) || vehicleMovingTooFast)
						{
							BreakOff(pParent);
						}
					}

					if( GetIsIntact(pParent) &&
						!m_bResetComponentToLinkIndex &&
						!GetIsLatched( pParent ) )
					{
						// Break off the door if it's 45 degrees or outside its limits					
						const float maxAngleOverLimit = 45.0f * DtoR;
						
						float overLimitAngle = Abs(unclampedRatio-m_fCurrentRatio)*Abs(m_fOpenAmount-m_fClosedAmount);

						bool overLimitTimed = false;

						// B*1991530: Break off doors that are pushed beyond their limits or rapidly flick between beyond the limits and not for a continuous period of time.
						if(nTimeSlice == 1)
						{
							// Door will break off if it is beyond this angle for longer than some amount of time.
							const float maxAngleOverLimitTimed = 2.0f * DtoR;											

							// Door has just been pushed past its limits so record the time so we can break it off if it stays like this for too long.
							if(overLimitAngle > maxAngleOverLimitTimed)
							{	
								if( !m_nOpenPastLimitsTime )
								{
									m_nOpenPastLimitsTime = fwTimer::GetTimeInMilliseconds();
								}
						
								m_fAnglePastLimits = overLimitAngle;
								m_nPastLimitsTimeThreshold = 0;
							}
							else
							{
								// Allow for a window of time before resetting the timer in case the door is oscillating between under and over limits.
								if(m_fAnglePastLimits > maxAngleOverLimitTimed)
								{
									m_nPastLimitsTimeThreshold++;		
								}
							
								if(m_nPastLimitsTimeThreshold >= 10 || m_fAnglePastLimits <= maxAngleOverLimitTimed)
								{
									// Reset the last time that the doors were within their limits.
									m_nOpenPastLimitsTime = 0;
									m_fAnglePastLimits = overLimitAngle;
								}
							}
							
							// If the doors are considered beyond their limits (Taking into account doors flickering between beyond and not) for over a second, break the door.
							if( m_nOpenPastLimitsTime )
							{
								overLimitTimed = ((m_fAnglePastLimits > maxAngleOverLimitTimed) && ((fwTimer::GetTimeInMilliseconds() - m_nOpenPastLimitsTime) > 1000));
							}
						}
						
						bool shouldBreakOff = (overLimitAngle > maxAngleOverLimit) || ((bfCarDoorBreakThreshold > 0.0f) && (m_vDeformation.Mag() >= bfCarDoorBreakThreshold)) ||
												overLimitTimed;

						if (shouldBreakOff)
						{
							BreakOff(pParent);
							
							m_fAnglePastLimits = 0;	
							m_nPastLimitsTimeThreshold = 0;
						}
					}
				}
			}

			// If a door is close to its target it won't keep the vehicle awake. If the door is touching something or moving then the
			//   simulator will keep it awake, otherwise we probably aren't going to get any closer to our target. 
			if(GetFlag(drivingDoorFlags) && Abs(m_fCurrentRatio - m_fTargetRatio) > sfDoorRatioSleepThreshold)
			{
				SetFlag(PROCESS_FORCE_AWAKE);
			}
		}
	}

	if(CPhysics::GetIsLastTimeSlice(nTimeSlice))
	{
		ClearFlag(DRIVEN_AUTORESET);

		m_bDoorRatioHasChanged = false;

		// Opening/closing doors causes the navigation bounds to expand/contract.
		// We also ensure that the navigation bounds are refreshed if the door's open/closed ratio changes.
		static float fMaxRatioNav = 0.01f;
		if(Abs(m_fOldRatio - m_fCurrentRatio) > fMaxRatioNav || GetForceOpenForNavigation() ||
			(GetIsClosed() && GetPathServerDynamicObjectIndex()!=DYNAMIC_OBJECT_INDEX_NONE) ||
			(!GetIsClosed() && GetPathServerDynamicObjectIndex()==DYNAMIC_OBJECT_INDEX_NONE))
		{
			pParent->m_nVehicleFlags.bDoorRatioHasChanged = true;
			m_bDoorRatioHasChanged = true;
		}				

		if(m_bForceOpenForNavigation > 0 && m_bForceOpenForNavigation != 4)
		{
			m_bForceOpenForNavigation--;
		}

		m_fOldRatio = m_fCurrentRatio;

		m_bPlayerStandsOnDoor = false;
		const bool bMiljet = MI_PLANE_MILJET.IsValid() && (pParent->GetModelIndex() == MI_PLANE_MILJET);
		const bool bLuxor2 = MI_PLANE_LUXURY_JET3.IsValid() && (pParent->GetModelIndex() == MI_PLANE_LUXURY_JET3);
		const bool bNimbus = MI_PLANE_NIMBUS.IsValid() && (pParent->GetModelIndex() == MI_PLANE_NIMBUS);
		const bool bBombushka = MI_PLANE_BOMBUSHKA.IsValid() && (pParent->GetModelIndex() == MI_PLANE_BOMBUSHKA);
		const bool bVolatol = MI_PLANE_VOLATOL.IsValid() && (pParent->GetModelIndex() == MI_PLANE_VOLATOL);
		if((pParent->GetModelIndex() == MI_PLANE_LUXURY_JET || pParent->GetModelIndex() == MI_PLANE_LUXURY_JET2 || bMiljet || bLuxor2 || bNimbus || bBombushka || bVolatol)
			&& GetIsIntact(pParent) && !GetIsLatched(pParent))
		{
			unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
			const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

			for(unsigned index = 0; index < numPhysicalPlayers; index++)
			{
				const CNetGamePlayer *pNetPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);
				const CPed* pPlayer = pNetPlayer->GetPlayerPed();

				if(pPlayer && pPlayer->GetGroundPhysical() == pParent && pPlayer->GetGroundPhysicalComponent() == GetFragChild())
				{
					m_bPlayerStandsOnDoor = true;
					break;
				}
			}
		}
	}

#if __BANK					
	if(sm_debugDoorPhysics && wasSetToOpen)
	{
		if(GetIsClosed())
		{
			physicsDisplayf("CCarDoor::ProcessPhysics - [%s] OH NO! Door was set to be forced open, but it's closed for some reason... Current ratio: %f | Target ratio: %f\n", pParent->GetDebugName(), m_fCurrentRatio, m_fTargetRatio);
		}
		else
		{
			physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door was set to be forced open, and is open. Current ratio: %f | Target ratio: %f\n", pParent->GetDebugName(), m_fCurrentRatio, m_fTargetRatio);
		}
		PrintDoorFlags( pParent );
	}

	if(sm_debugDoorPhysics && wasOpenToStart && GetIsClosed())
	{
		physicsDisplayf("CCarDoor::ProcessPhysics - [%s] Door was open at start, but is now closed. Current ratio: %f | Target ratio: %f\n", pParent->GetDebugName(), m_fCurrentRatio, m_fTargetRatio);
		PrintDoorFlags( pParent );
	}
#endif
}

void CCarDoor::UnlatchAnimatedDoors(CVehicle* pParent)
{
	if(GetFlag(DRIVEN_BY_PHYSICS) REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()))
	{
		return;
	}

	Matrix34 BoneLocal = MAT34V_TO_MATRIX34(pParent->GetSkeleton()->GetLocalMtx(m_nBoneIndex)); 
	Quaternion DefaultRotation = QUATV_TO_QUATERNION(pParent->GetSkeleton()->GetSkeletonData().GetBoneData(m_nBoneIndex)->GetDefaultRotation()); 
	Matrix34 DefaultMat(M34_IDENTITY);
    Vector3 DefaultTranslation;
	
	DefaultMat.FromQuaternion(DefaultRotation); 
	DefaultTranslation = DefaultMat.d =VEC3V_TO_VECTOR3(pParent->GetSkeleton()->GetSkeletonData().GetBoneData(m_nBoneIndex)->GetDefaultTranslation()); 
	
	DefaultMat.DotTranspose(BoneLocal); 
	Vector3 VectorAngle; 
	float openAmount = 0.0f; 
	
	DefaultMat.ToEulersFastXYZ(VectorAngle);
	
	if(GetAxis()==AXIS_X)
	{
		openAmount = VectorAngle.x;
	}
	else if(GetAxis()==AXIS_Y)
	{
		openAmount = VectorAngle.y; 
	}
	else if(GetAxis()==AXIS_Z)
	{
		openAmount = VectorAngle.z; 
	}
    else if(GetAxis()==AXIS_SLIDE_Y)
    {
       openAmount = DefaultTranslation.y - BoneLocal.d.y;
    }
	
	 m_fCurrentRatio = ComputeClampedRatio(openAmount*-1.0f);
	 vehicledoorDebugf2("CCarDoor::UnlatchAnimatedDoors %s m_fCurrentRatio[%f]", pParent ? pParent->GetLogName() : "",m_fCurrentRatio);

	 if(m_fCurrentRatio >= sfDoorUnlatchedAngle && GetIsLatched(pParent))
	 {
		BreakLatch(pParent); 
		SetSwingingFree(pParent); 
	 }
}		

bool CCarDoor::IsNetworkDoorOpenAllowed()
{
	static int kDelayNetworkOpenDoor = 1000;
	return (fwTimer::GetSystemTimeInMilliseconds() > (m_nNetworkTimeOpened + kDelayNetworkOpenDoor));
}

void CCarDoor::DriveDoorOpen(float angle , phJoint* pJoint, CVehicle* pParent, phArticulatedCollider* pArticulatedCollider )
{
	if(pJoint && pParent && pArticulatedCollider)	
	{
		vehicledoorDebugf2("CCarDoor::DriveDoorOpen %s angle[%f]", pParent->GetLogName(),angle);

		Vector3 vecTemp;
		Matrix34 matNewDoor;
		
		matNewDoor.Identity();

		bool bIsDeluxo = pParent->GetModelIndex() == MI_CAR_DELUXO;
		int axis = GetAxis();
		if( bIsDeluxo )
		{
			if( m_DoorId == VEH_DOOR_DSIDE_F || m_DoorId == VEH_DOOR_PSIDE_F)
			{
				axis = AXIS_Y;
				angle = -angle;
			}
			else if (m_DoorId == VEH_BONNET)
			{
				axis = AXIS_X;
				angle = -angle;
			}
		}

		if(axis==AXIS_X)
			matNewDoor.MakeRotateX(angle);
		else if(axis==AXIS_Y)
			matNewDoor.MakeRotateY(angle);
		else if(axis==AXIS_Z)
			matNewDoor.MakeRotateZ(angle);
		else
		{
			if(axis==AXIS_SLIDE_Y)
			{
				matNewDoor.d = RCC_VECTOR3(pParent->GetSkeleton()->GetSkeletonData().GetBoneData(m_nBoneIndex)->GetDefaultTranslation());
				matNewDoor.d.y += angle;
			}
		}

		matNewDoor.Dot3x3(RCC_MATRIX34(pArticulatedCollider->GetMatrix()));
		matNewDoor.Transpose();

		matNewDoor.d.Set(pJoint->GetChildLink(pArticulatedCollider->GetBody())->GetPosition());
		vecTemp = VEC3V_TO_VECTOR3(UnTransform3x3Ortho(pJoint->GetChildLink(pArticulatedCollider->GetBody())->GetMatrix(),VECTOR3_TO_VEC3V(pJoint->GetPositionChild())));
		matNewDoor.d.Add(vecTemp);
		matNewDoor.UnTransform3x3(pJoint->GetPositionChild(), vecTemp);
		matNewDoor.d.Subtract(vecTemp);

		pJoint->GetChildLink(pArticulatedCollider->GetBody())->SetMatrix(matNewDoor);

		m_fCurrentRatio = ComputeClampedRatio(angle);
		vehicledoorDebugf2("CCarDoor::DriveDoorOpen %s m_fCurrentRatio[%f] = ComputeClampedRatio", pParent->GetLogName(),m_fCurrentRatio);
		
		// B*1890499: Make sure that doors being opened/closed update the vehicle skeleton AND the associated bounds when they update the articulated body link, if the vehicle is inactive.
		// This code assumed that everything would get synced up correctly at various points in the frame, but if the vehicle is inactive, this doesn't always happen.
		// The joint link would tend to be overridden by the bounds matrix before the skeleton got synced to the articulated body and the bounds then synced to the skeleton. Fun times.
		// This would happen even with the fix in CL#5342971, if the vehicle was fixed due to no collision and the player got close enough to "unfix" it or more than one door was opened while it was still fixed.
		// Forcing open a second door would cause the changes made from opening the first door to be overridden as described above (BreakLatch I think forces the art. body to sync to the skeleon).
		fragInst* pFragInst = pParent->GetFragInst(); 
		if (pFragInst && pParent->IsColliderArticulated() && !CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex()) && !pParent->InheritsFromTrain() && !pParent->InheritsFromDraftVeh())
		{

			// Set the matrix of the collider's instance from the collider's matrix and the bound's centroid offset.
			if (pFragInst->IsInLevel())
			{
				if(fragCacheEntry* entry = pFragInst->GetCacheEntry() )
				{
					fragHierarchyInst& hierInst = *entry->GetHierInst();
					if (hierInst.body)
					{
						phArticulatedCollider& articulatedCollider = *hierInst.articulatedCollider;
						articulatedCollider.SetInstanceMatrixFromCollider();
					}
				}
			}

			// Update the skeleton...
			pFragInst->SyncSkeletonToArticulatedBody();

			// And now update the bound in the frag instance (if it's in the cache).
			int nDoorBoneIndex = pParent->GetBoneIndex(m_DoorId);
			if(nDoorBoneIndex > -1)
			{
				if(pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst() && pFragInst->GetCacheEntry()->GetHierInst()->skeleton && pFragInst->GetCacheEntry()->GetBound())
				{
					Mat34V_ConstRef boneMat = pFragInst->GetCacheEntry()->GetHierInst()->skeleton->GetObjectMtx(nDoorBoneIndex);
					int componentIndex = pFragInst->GetCacheEntry()->GetComponentIndexFromBoneIndex(nDoorBoneIndex);
					pFragInst->GetCacheEntry()->GetBound()->SetCurrentMatrix(componentIndex, boneMat);
				}					
			}
		}		
	}
}

#if GTA_REPLAY
void CCarDoor::ReplayUpdateDoors(CVehicle* pParent)
{
	fragInst* pFragInst = pParent->GetFragInst(); 
	if (pFragInst && !CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex()) && !pParent->InheritsFromTrain() && !pParent->InheritsFromDraftVeh())
	{
		// And now update the bound in the frag instance (if it's in the cache).
		int nDoorBoneIndex = pParent->GetBoneIndex(m_DoorId);
		if(nDoorBoneIndex > -1)
		{
			if(pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst() && pFragInst->GetCacheEntry()->GetHierInst()->skeleton && pFragInst->GetCacheEntry()->GetBound())
			{
				Mat34V_ConstRef boneMat = pFragInst->GetCacheEntry()->GetHierInst()->skeleton->GetObjectMtx(nDoorBoneIndex);
				int componentIndex = pFragInst->GetCacheEntry()->GetComponentIndexFromBoneIndex(nDoorBoneIndex);
				pFragInst->GetCacheEntry()->GetBound()->SetCurrentMatrix(componentIndex, boneMat);
			}
		}
	}
}
#endif

float CCarDoor::ClampDoorOpenRatio(float fCurrentRatio) const
{
	// The current ratio is used as the target angle for bikes in CCarDoor::::DriveDoorOpen so can be negative
	if (!(m_nDoorFlags & BIKE_DOOR))
	{
		return Clamp(fCurrentRatio, 0.0f, 1.0f);
	}
	else
	{
		return fCurrentRatio;
	}
}

float CCarDoor::ComputeRatio(float position) const
{
	return (position - m_fClosedAmount) / (m_fOpenAmount - m_fClosedAmount);
}

float CCarDoor::ComputeClampedRatio(float position) const
{
	return ClampDoorOpenRatio(ComputeRatio(position));
}

bool CCarDoor::NeedsToAnimateOpen( CVehicle* pParent, bool bIgnoreCurrentRatio )
{
#if __BANK
	if( ( pParent->GetVehicleType() == VEHICLE_TYPE_HELI &&
		  static_cast< CHeli* >(pParent)->GetIsCargobob() ) ||
		 ( pParent->GetVehicleType() == VEHICLE_TYPE_PLANE &&
		   GetHierarchyId() == VEH_DOOR_DSIDE_R &&
		   GetHierarchyId() == VEH_BOOT ) )
	{
		vehicledoorDebugf3("CCarDoor::NeedsToAnimateOpen FragChild[%d] FixedFlags[%d] HierachyId[%d] targetRatio[%f] currentRatio[%f]",
							m_nFragChild,
							pParent->GetIsAnyFixedFlagSet(),
							GetHierarchyId(),
							m_fTargetRatio,
							m_fCurrentRatio );
	}
#endif // #if __BANK

	vehicledoorDebugf3("CCarDoor::NeedsToAnimateOpen: %s", pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : "" );

	if( !bIgnoreCurrentRatio &&
		m_fTargetRatio <= m_fCurrentRatio &&
		m_fCurrentRatio >= 0.5f )
	{
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: return false: target ratio: %.2f, current ratio: %.2f", m_fTargetRatio, m_fCurrentRatio );
		return false;
	}

	if( pParent->GetModelIndex() == MI_HELI_CARGOBOB4 )
	{ 
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: cargobob4: return true" );

		if( m_fTargetRatio > 0.0f )
		{
			ClearFlag( WILL_LOCK_SWINGING );
		}
		
		return true;
	}

	if( m_nFragChild <= 0 )
	{
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: return false: m_nFragChild <= 0" );
		return false;
	}

	if( !pParent->GetIsAnyFixedFlagSet() )
	{
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: return false: no fixed flags set" );
		return false;
	}

	if( pParent->GetModelIndex() != MI_PLANE_TITAN &&
		pParent->GetModelIndex() != MI_PLANE_CARGOPLANE )
	{
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: return false: not titan or cargoplane" );
		return false;
	}

	if( !pParent->GetVehicleFragInst() )
	{
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: return false: has no frag inst" );
		return false;
	}

	if( GetHierarchyId() != VEH_DOOR_DSIDE_R &&
		GetHierarchyId() != VEH_BOOT )
	{
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: return false: wrong hierachyid: %d", (int)GetHierarchyId() );
		return false;
	}


	if( !bIgnoreCurrentRatio &&
		m_fTargetRatio == m_fCurrentRatio &&
		m_fTargetRatio == 0.0f )
	{
		vehicledoorDebugf3( "CCarDoor::NeedsToAnimateOpen: return false: door closed: target ratio: %.2f, current ratio: %.2f", m_fTargetRatio, m_fCurrentRatio );
		return false;
	}

	vehicledoorDebugf3("CCarDoor::NeedsToAnimateOpen FragChild[%d] FixedFlags[%d] HierachyId[%d] targetRatio[%f] currentRatio[%f] - returning true",
		m_nFragChild,
		pParent->GetIsAnyFixedFlagSet(),
		GetHierarchyId(),
		m_fTargetRatio,
		m_fCurrentRatio );

	if( m_fTargetRatio > 0.0f )
	{
		ClearFlag( WILL_LOCK_SWINGING );
	}

	return true;
}

void CCarDoor::ApplyDeformation(CVehicle* pParentVehicle, const void* basePtr)
{
	if ((pParentVehicle != NULL) && (basePtr != NULL))
	{
		int nDoorBoneIndex = pParentVehicle->GetBoneIndex(m_DoorId);

		if(nDoorBoneIndex > -1)
		{
			Vector3 basePos;
			pParentVehicle->GetDefaultBonePositionSimple(nDoorBoneIndex, basePos);

			if(GetAxis()==AXIS_X || GetAxis()==AXIS_Y || GetAxis()==AXIS_Z)
			{
				m_vDeformation = VEC3V_TO_VECTOR3(pParentVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(basePos)));
			}
		}
	}
}

void CCarDoor::ProcessPreRender2(CVehicle *pParent)
{
	if(pParent && pParent->GetSkeleton())
	{
		crSkeleton* pSkel = pParent->GetSkeleton();

		if(bbApplyCarDoorDeformation && GetFlag(IS_ARTICULATED) && !m_vDeformation.IsZero() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
		{
			int nDoorBoneIndex = pParent->GetBoneIndex(m_DoorId);

			if(nDoorBoneIndex > -1)
			{
				Matrix34& doorMatrixLocalRef = pParent->GetLocalMtxNonConst(nDoorBoneIndex);
				// Reset bone position
				const crBoneData* pBoneData = pSkel->GetSkeletonData().GetBoneData(nDoorBoneIndex);
				doorMatrixLocalRef.d.Set(RCC_VECTOR3(pBoneData->GetDefaultTranslation()));
				// pre translate by deformation offset
				doorMatrixLocalRef.d.Add(m_vDeformation);
				// post translate by inverse deformation offset
				Quaternion defaultRotation = RCC_QUATERNION(pBoneData->GetDefaultRotation());
				Matrix34 defaultDoorRotationMatrix;
				defaultDoorRotationMatrix.FromQuaternion(defaultRotation);
				Vector3 vecPostRotationOffset;
				defaultDoorRotationMatrix.UnTransform3x3(m_vDeformation, vecPostRotationOffset); // untransform the offset to identity space first
				doorMatrixLocalRef.Transform3x3(vecPostRotationOffset);
				doorMatrixLocalRef.d.Subtract(vecPostRotationOffset);
				pSkel->PartialUpdate(nDoorBoneIndex);
			}
		}

		if(GetFlag((u32)RENDER_INVISIBLE))
		{
			int nDoorBoneIndex = pParent->GetBoneIndex(m_DoorId);

			if(nDoorBoneIndex > -1)
			{
				Mat34V& boneMat = pSkel->GetObjectMtx(nDoorBoneIndex);
				boneMat = Mat34V(V_ZERO);
			}

		}

		bool bIsIgnus = ( ( (MI_CAR_IGNUS2.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS2.GetModelIndex()) || (MI_CAR_IGNUS.IsValid() && pParent->GetModelIndex() == MI_CAR_IGNUS.GetModelIndex()) ) ? true: false);
        bool HasFunnyDoor = (MI_CAR_FURIA.IsValid() && pParent->GetModelIndex() == MI_CAR_FURIA.GetModelIndex()) || bIsIgnus;
        if( HasFunnyDoor &&
            ( m_DoorId == VEH_DOOR_DSIDE_F || m_DoorId == VEH_DOOR_PSIDE_F ) && pParent->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY)
        {
            int nDoorBoneIndex = pParent->GetBoneIndex( m_DoorId );
			float xOffset = bIsIgnus ? 0.2f: 0.3f;
            Vec3V vecDoorTargetOffset( xOffset, 0.0f, 0.0f );
            static dev_float sfDoorOffsetRateScale = 4.0f;
            float scaledDoorRatio = Clamp( m_fCurrentRatio * sfDoorOffsetRateScale, 0.0f, 1.0f );
            Vec3V vecDoorOffset = vecDoorTargetOffset * ScalarV( scaledDoorRatio );
            static dev_float sfWindowRollDownAmount = 0.2f;
            s32 windowBoneIndex = -1;

            if( m_DoorId == VEH_DOOR_DSIDE_F )
            {
                vecDoorOffset.SetXf( vecDoorOffset.GetXf() * -1.0f );

                if( !pParent->IsWindowDown( VEH_WINDOW_LF ) )
                {
                    windowBoneIndex = pParent->GetBoneIndex( VEH_WINDOW_LF );
                }
            }
            else
            {
                if( !pParent->IsWindowDown( VEH_WINDOW_RF ) )
                {
                    windowBoneIndex = pParent->GetBoneIndex( VEH_WINDOW_RF );
                }
            }


            crSkeleton* pSkel = pParent->GetSkeleton();
            if( windowBoneIndex != -1 && pSkel )
            {
                Vec3V windowPos = pParent->GetSkeleton()->GetSkeletonData().GetBoneData( windowBoneIndex )->GetDefaultTranslation();
				float windowsZPos = windowPos.GetZf() - ( sfWindowRollDownAmount * scaledDoorRatio );
				if( bIsIgnus )
				{
					static float ratioZ = 0.17f;
					float ignusWindowZOffset = ratioZ * m_fCurrentRatio;
					windowsZPos += ignusWindowZOffset;

					float windowsXPos = windowPos.GetXf() - ( sfWindowRollDownAmount * scaledDoorRatio );

					static float ratio_DSIDE = 0.1f;
					static float ratio_PSIDE = 0.3f;

					float ratioX = (m_DoorId == VEH_DOOR_DSIDE_F ? ratio_DSIDE: ratio_PSIDE);
					float ignusWindowXoffset = ratioX * m_fCurrentRatio;
					windowsXPos += ignusWindowXoffset;
					windowPos.SetXf( windowsXPos );
				}
                windowPos.SetZf( windowsZPos );

                Mat34V& localBoneMatrix = pSkel->GetLocalMtx( windowBoneIndex );
				if(bIsIgnus)
				{	
					const Quaternion& q = RCC_QUATERNION( pParent->GetSkeleton()->GetSkeletonData().GetBoneData( windowBoneIndex )->GetDefaultRotation() );
					Matrix34 m;
					m.FromQuaternion(q);

					if( m_fCurrentRatio > 0.2f )
					{
						static float angY_DSIDE = -45.0f;
						static float angY_PSIDE = 45.0f;

						float angDegY = (m_DoorId == VEH_DOOR_DSIDE_F ? angY_DSIDE: angY_PSIDE);
						float angY = DEGREES_TO_RADIANS( angDegY );
						m.RotateLocalY( angY );

						static float angX_DSIDE = 10.0f;
						static float angX_PSIDE = 10.0f;

						float angDegX = (m_DoorId == VEH_DOOR_DSIDE_F ? angX_DSIDE: angX_PSIDE);
						float angX = DEGREES_TO_RADIANS( angDegX );
						m.RotateLocalX( angX );						
					}

					localBoneMatrix = MATRIX34_TO_MAT34V(m);
				}
                localBoneMatrix.Setd( windowPos );
            }

            Mat34V& boneMat = pSkel->GetLocalMtx( nDoorBoneIndex );
            const crBoneData* pBoneData = pSkel->GetSkeletonData().GetBoneData( nDoorBoneIndex );
            boneMat.Setd( pBoneData->GetDefaultTranslation() + ( vecDoorOffset * ScalarV( scaledDoorRatio ) ) );
            pSkel->PartialUpdate( nDoorBoneIndex );
        }
	}
}

void CCarDoor::ProcessPreRender(CVehicle *pParent)
{
#if 0
	Matrix34 matGloble;
	pParent->GetGlobalMtx(pParent->GetBoneIndex(m_DoorId), matGloble);
	Vector3 vBoxMin, vBoxMax;
	if(GetLocalBoundingBox(pParent, vBoxMin, vBoxMax))
	{
		matGloble.Transform(vBoxMin);
		matGloble.Transform(vBoxMax);
		grcDebugDraw::Sphere(vBoxMin, 0.1, Color_green, false);
		grcDebugDraw::Sphere(vBoxMax, 0.1, Color_blue, false);
	}

	if(m_vDeformation.IsNonZero())
	{
		Vector3 vDeformation = m_vDeformation;
		matGloble.Transform3x3(vDeformation);
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(matGloble.d), VECTOR3_TO_VEC3V(matGloble.d + vDeformation), 0.1f, Color_white);
	}
#endif

	if(pParent->m_nVehicleFlags.bAnimateJoints)
	{
		UnlatchAnimatedDoors(pParent);
		return;
	}

	ClearFlag(DRIVEN_BY_PHYSICS); // Acts like a reset flag, needs to be set every frame
	ClearFlag(DRIVEN_USE_STIFFNESS_MULT); // Need to set this each frame to use the stiffness multiplier

	if(pParent->GetSkeleton())
	{
		if(GetFlag(IS_ARTICULATED))
		{
			if(GetFlag(LOOSE_LATCHED_DOOR) && GetFlag(DRIVEN_SHUT) && 
				(GetHierarchyId()!=VEH_BOOT || !GetFlag(CCarDoor::DONT_BREAK)) && 
				!(GetFlag(CCarDoor::AXIS_SLIDE_X) || GetFlag(CCarDoor::AXIS_SLIDE_Y) || GetFlag(CCarDoor::AXIS_SLIDE_Z)))
			{
				float fOpenAmount = CVehicleDamage::ms_fLooseLatchedDoorOpenAngle;
				if(GetHierarchyId()==VEH_BONNET || GetHierarchyId()==VEH_BOOT)
				{
					fOpenAmount = CVehicleDamage::ms_fLooseLatchedBonnetOpenAngle;
				}

				fOpenAmount *= Sign(m_fOpenAmount - m_fClosedAmount);
				float fCurrentPosition = m_fClosedAmount + fOpenAmount;

				if(GetAxis()==AXIS_X)
					pParent->SetComponentRotation(m_DoorId, ROT_AXIS_LOCAL_X, fOpenAmount);
				else if(GetAxis()==AXIS_Y)
					pParent->SetComponentRotation(m_DoorId, ROT_AXIS_LOCAL_Y, fOpenAmount);
				else if(GetAxis()==AXIS_Z)
					pParent->SetComponentRotation(m_DoorId, ROT_AXIS_LOCAL_Z, fOpenAmount);
				else
				{
					int nDoorBoneIndex = pParent->GetBoneIndex(m_DoorId);
					if(nDoorBoneIndex > -1)
					{
						Matrix34& doorMatrix = pParent->GetLocalMtxNonConst(nDoorBoneIndex);
						doorMatrix.Identity3x3();
						if(GetAxis()==AXIS_SLIDE_Y)
						{
							doorMatrix.d = RCC_VECTOR3(pParent->GetSkeleton()->GetSkeletonData().GetBoneData(nDoorBoneIndex)->GetDefaultTranslation());
							doorMatrix.d.y += fCurrentPosition;
						}
					}
				}
			}
			else
			{
				if( NeedsToAnimateOpen( pParent ) )
				{
					// find the associated joint
					pParent->GetVehicleFragInst()->ForceArticulatedColliderMode();
					fragCacheEntry* cacheEntry = pParent->GetVehicleFragInst()->GetCacheEntry();
					
					if( cacheEntry )
					{
						fragHierarchyInst* pHierInst =  cacheEntry->GetHierInst();
					
						if( pHierInst &&
							pHierInst->body )
						{
							phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;
						
							if( pArticulatedCollider )
							{
								int linkIndex = pArticulatedCollider->GetLinkFromComponent(m_nFragChild);
								if(linkIndex > 0)
								{
									phJoint* pJoint = &pHierInst->body->GetJoint( linkIndex - 1 );

									float fCurrent = 0.0f;
						
									fCurrent = ( m_fTargetRatio > m_fCurrentRatio ) ? 1.0f : -1.0f;
									fCurrent *= ANIMATED_LARGE_REAR_RAMP_DOOR_MAX_SPEED;
									fCurrent *= fwTimer::GetTimeStep();
									fCurrent += m_fCurrentRatio;
									fCurrent = Clamp( fCurrent, 0.0f, 1.0f );
									fCurrent = fCurrent * m_fOpenAmount + (1.0f - fCurrent) * m_fClosedAmount;
								
									DriveDoorOpen(fCurrent, pJoint, pParent, pArticulatedCollider); 

									pParent->m_forceUpdateBoundsAndBodyFromSkeleton = true;
								}
							}
						}
					}
				}
			}
		}
		else
		{
			int nDoorBoneIndex = pParent->GetBoneIndex(m_DoorId);
			if(nDoorBoneIndex > -1)
			{
				// trigger door open/close audio for non-articulated doors
				if(m_fCurrentRatio == 0.0f && m_fOldRatio > 0.0f)
				{
					pParent->GetVehicleAudioEntity()->TriggerDoorCloseSound(m_DoorId, false);
				}
				else if(m_fOldRatio == 0.0f && m_fCurrentRatio > 0.0f)
				{
					pParent->GetVehicleAudioEntity()->TriggerDoorOpenSound(m_DoorId);
				}
				else if(m_fCurrentRatio == 1.0f && m_fOldRatio != 1.0f)
				{
					pParent->GetVehicleAudioEntity()->TriggerDoorFullyOpenSound(m_DoorId);
				}

				m_fOldRatio = m_fCurrentRatio;

				Matrix34& doorMatrix = pParent->GetLocalMtxNonConst(nDoorBoneIndex);
				float fCurrentPosition = m_fCurrentRatio * m_fOpenAmount + (1.0f - m_fCurrentRatio) * m_fClosedAmount;

				if(GetAxis()==AXIS_X)
					doorMatrix.MakeRotateX(fCurrentPosition);
				else if(GetAxis()==AXIS_Y)
					doorMatrix.MakeRotateY(fCurrentPosition);
				else if(GetAxis()==AXIS_Z)
					doorMatrix.MakeRotateZ(fCurrentPosition);
				else
				{
					doorMatrix.Identity3x3();
					if(GetAxis()==AXIS_SLIDE_Y)
					{
						doorMatrix.d = RCC_VECTOR3(pParent->GetSkeleton()->GetSkeletonData().GetBoneData(nDoorBoneIndex)->GetDefaultTranslation());
						doorMatrix.d.y += fCurrentPosition;
					}
				}

				// Make sure joints are unlatched on trains
				if(pParent->InheritsFromTrain())
				{
					UnlatchAnimatedDoors(pParent);
				}
			}
		}
	}
}

bool CCarDoor::GetLocalMatrix(const CVehicle * pParent, Matrix34 & mat, const bool bForceFullyOpen) const
{
	if(!GetIsIntact(pParent))
		return false;
	if(!pParent || !pParent->GetVehicleFragInst())
		return false;
	if(m_nFragChild > 0
		&& pParent->GetVehicleFragInst()->GetCacheEntry()
		&& pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst())
	{
		phBound* pBound = pParent->GetVehicleFragInst()->GetArchetype()->GetBound();
		Assertf(pBound, "Vehicle frag inst did not have a valid bound!");
		Assertf(pBound->GetType() == phBound::COMPOSITE, "Vehicle frag inst had a bound of non-composite type!");
		if(pBound && pBound->GetType() == phBound::COMPOSITE)
		{
			phBoundComposite* pThisBoundComposite = static_cast<phBoundComposite*>(pBound);
			mat = MAT34V_TO_MATRIX34(pThisBoundComposite->GetCurrentMatrix(m_nFragChild));

			if(bForceFullyOpen)
			{
				float fOpenAmount = GetOpenAmount();
				float fDoorAngle = GetDoorAngle();
				float fDeltaAngle = SubtractAngleShorter(fOpenAmount, fDoorAngle);
				fDeltaAngle = fwAngle::LimitRadianAngle(fDeltaAngle);

				Matrix34 rotMat;
				rotMat.Identity();
				rotMat.RotateFullZ(fDeltaAngle);
				mat.Dot3x3FromLeft(rotMat);
			}

			return true;
		}
	}
	return false;
}
bool CCarDoor::GetLocalBoundingBox(const CVehicle * pParent, Vector3 & vMinOut, Vector3 & vMaxOut) const
{
	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	if(m_nFragChild > 0 && pFragInst && pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst())
	{
		phBound* pBound = pFragInst->GetArchetype()->GetBound();
		Assertf(pBound, "Vehicle frag inst did not have a valid bound!");
		Assertf(pBound->GetType() == phBound::COMPOSITE, "Vehicle frag inst had a bound of non-composite type!");
		if (pBound && pBound->GetType() == phBound::COMPOSITE)
		{
			phBoundComposite * pParentComposite = static_cast<phBoundComposite*>(pBound);
			if (Verifyf(m_nFragChild < pParentComposite->GetNumBounds(), "FragChild was %d, but the parent composite bound had %d total bounds!", m_nFragChild, pParentComposite->GetNumBounds()))
			{
				//Bounds retrieved through GetBound are not guaranteed to be allocated
				phBound * pDoorBound = pParentComposite->GetBound(m_nFragChild);
				if (pDoorBound)
				{
					vMinOut = VEC3V_TO_VECTOR3( pDoorBound->GetBoundingBoxMin() );
					vMaxOut = VEC3V_TO_VECTOR3( pDoorBound->GetBoundingBoxMax() );
					return true;
				}
			}
		}
	}
	return false;
}

#if !__NO_OUTPUT
void vehicledoorDefaultPrintFn(const char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);
	diagLogDefault(RAGE_CAT2(Channel_,vehicledoor), DIAG_SEVERITY_DISPLAY, __FILE__, __LINE__, fmt, args);
	va_end(args);
}

void vehiceldoorTraceDisplayLine(size_t addr, const char *sym, size_t offset)
{
	diagDisplayf(RAGE_CAT2(Channel_,vehicledoor),RAGE_LOG_FMT("%8" SIZETFMT "x - %s+%" SIZETFMT "x"),addr,sym,offset);
}
#endif

dev_float sfDrivenSmoothRate = 0.5f;
dev_float sfMinRatioDifference = 0.1f;

dev_float sfBootUnlatchedAngle = 0.06f;
dev_float sfBonnetUnlatchedAngle = 0.05f;

//
void CCarDoor::SetTargetDoorOpenRatio(float fRatio, u32 nFlags, CVehicle *pParent)
{
	// NOTE: hack for B*7594679, dont open fully trunk of Granger2
	if (pParent && pParent->GetModelIndex() == MI_CAR_GRANGER2)
	{
		if (m_DoorId == VEH_BOOT)
		{
			const float bootMaxRatio = 0.6f;
			if (fRatio > bootMaxRatio)
			{
				fRatio = bootMaxRatio;
			}
		}
	}
#if !__NO_OUTPUT
	vehicledoorDebugf3("CCarDoor::SetTargetDoorOpenRatio pDoor[%p] pParent[%p][%s][%s] --> m_fCurrentRatio[%f] m_fTargetRatio = fRatio[%f] nFlags[0x%x]",this,pParent, pParent ? pParent->GetModelName() : "", pParent && pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : "",m_fCurrentRatio,fRatio,nFlags);
	if ((Channel_vehicledoor.FileLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_vehicledoor.TtyLevel >= DIAG_SEVERITY_DEBUG3))
	{
		if (scrThread::GetActiveThread())
			scrThread::GetActiveThread()->PrintStackTrace(vehicledoorDefaultPrintFn);

		sysStack::PrintStackTrace(vehiceldoorTraceDisplayLine);
	}
#endif

	// GTAV - B*3364042 - If the door is turned off by a vehicle mod then don't set the target door ratio.
	if( pParent &&
		pParent->GetModelNameHash() == MI_CAR_INFERNUS2.GetName().GetHash() &&
		pParent->GetVehicleDrawHandler().GetVariationInstance().IsBoneTurnedOff( pParent, m_nBoneIndex ) )
	{
		return;
	}

	// If the target ratio and target velocity is aligned
	if(Sign(fRatio - m_fCurrentRatio) == Sign(fRatio - m_fTargetRatio))
	{
		m_fOldTargetRatio = m_fTargetRatio;
	}
	// if the target speed is zero, set the target speed to the required speed for reaching the target ratio
	else if(fRatio == m_fTargetRatio)
	{
		if(Abs(fRatio - m_fCurrentRatio) < sfMinRatioDifference)
		{
			m_fOldTargetRatio = m_fTargetRatio;
		}
		else
		{
			m_fOldTargetRatio = m_fCurrentRatio;
		}
	}
	// if the target velocity is backwards, set target velocity to zero
	else
	{
		m_fOldTargetRatio = fRatio;
	}
	
	m_fTargetRatio = fRatio;

	if(!GetFlag(IS_ARTICULATED))
	{
		if(nFlags &DRIVEN_SMOOTH)
		{
			if(fRatio > m_fCurrentRatio + sfDrivenSmoothRate*fwTimer::GetTimeStep())
				m_fCurrentRatio += sfDrivenSmoothRate*fwTimer::GetTimeStep();
			else if(fRatio < m_fCurrentRatio - sfDrivenSmoothRate*fwTimer::GetTimeStep())
				m_fCurrentRatio -= sfDrivenSmoothRate*fwTimer::GetTimeStep();
			else
				m_fCurrentRatio = fRatio;
		}
		else
			m_fCurrentRatio = fRatio;

        m_fCurrentRatio = ClampDoorOpenRatio(m_fCurrentRatio);

		vehicledoorDebugf3("CCarDoor::SetTargetDoorOpenRatio m_fCurrentRatio[%f]",m_fCurrentRatio);
	}

	// set flags to control how the door is driven
	// DRIVEN_AUTORESET means this has to be called every frame
	// DRIVEN_NORESET can be called once and the door will stay at target until cleared
	ClearFlag(DRIVEN_MASK);
	SetFlag(nFlags);

	if(GetFlag(DRIVEN_NORESET | DRIVEN_GAS_STRUT | DRIVEN_NORESET_NETWORK))
	{
		m_fOldTargetRatio = m_fTargetRatio;
	}

	// GTAV - B*3335679 - When we break the latch on a bonnet or boot we modify the joint limits
	// in order for script to close them again we need to restore the initial joint limits.
	bool isNetworkOrScriptDriving = GetFlag( DRIVEN_NORESET ) || 
									( GetFlag( DRIVEN_NORESET_NETWORK ) &&
									  GetFlag( WILL_LOCK_DRIVEN ) );

	if( pParent &&
		isNetworkOrScriptDriving && fRatio == 0.0f &&
		( GetHierarchyId() == VEH_BONNET || 
		  GetHierarchyId() == VEH_BOOT ) )
	{
		fragInstGta* pFragInst = pParent->GetVehicleFragInst();

		// find the associated joint
		fragHierarchyInst* pHierInst = NULL;
		if( pFragInst && pFragInst->GetCached() )
		{
			pHierInst = pFragInst->GetCacheEntry()->GetHierInst();
		}

		phArticulatedCollider* pArticulatedCollider = NULL;
		if( pHierInst )
		{
			pArticulatedCollider = pHierInst->articulatedCollider;
		}

		phJoint1Dof* pDoorJoint = NULL;
		if( pArticulatedCollider && pArticulatedCollider->IsArticulated() && m_nFragChild > 0 )
		{
			int linkIndex = pArticulatedCollider->GetLinkFromComponent( m_nFragChild );
			if( linkIndex > 0 )
			{
				phJoint* pJoint = &pHierInst->body->GetJoint( linkIndex - 1 );
				if( pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF )
				{
					 pDoorJoint = static_cast<phJoint1Dof*>( pJoint );
				}
			}
		}

		if( pDoorJoint )
		{
			if( GetHierarchyId() == VEH_BONNET )
			{
				float fMin, fMax;
				pDoorJoint->GetAngleLimits( fMin, fMax );

				if( m_fOpenAmount > m_fClosedAmount )
				{
					pDoorJoint->SetAngleLimits( m_fClosedAmount, m_fOpenAmount );
				}
				else
				{
					pDoorJoint->SetAngleLimits( m_fOpenAmount, m_fClosedAmount );
				}
			}
			else if( GetHierarchyId() == VEH_BOOT )
			{
				float fMin, fMax;
				pDoorJoint->GetAngleLimits( fMin, fMax );

				if( m_fOpenAmount > m_fClosedAmount )
				{
					pDoorJoint->SetAngleLimits( m_fClosedAmount, m_fOpenAmount );
				}
				else
				{
					pDoorJoint->SetAngleLimits( m_fOpenAmount, m_fClosedAmount );
				}
			}
		}
	}

	if (NetworkInterface::IsGameInProgress() && (fRatio > 0.f))
		m_nNetworkTimeOpened = fwTimer::GetSystemTimeInMilliseconds();

	if(pParent)
	{
		pParent->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;
		pParent->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame = true;

		if( m_linkedDoorId != VEH_INVALID_ID )
		{
			pParent->GetDoorFromId( m_linkedDoorId )->SetTargetDoorOpenRatio( fRatio, nFlags, pParent );
		}
	}

}

void CCarDoor::SetSwingingFree(CVehicle* UNUSED_PARAM(pParent))
{
	ClearFlag(DRIVEN_MASK);
	if(GetFlag(IS_ARTICULATED))
	{
		// just set flag to make it swing when we hit ProcessPhysics
		SetFlag(DRIVEN_SWINGING);
		SetFlag(WILL_LOCK_SWINGING);
	}
}

void CCarDoor::SetShutAndLatched(CVehicle* pParent, bool bApplyForceForDoorClosed /* = true */)
{
	vehicledoorDebugf2("CCarDoor::SetShutAndLatched pParent[%p][%s][%s] --> m_fCurrentRatio[0.0]",pParent,pParent ? pParent->GetModelName() : "",pParent && pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : "");

	if( m_fTargetRatio > 0.1f &&
		NeedsToAnimateOpen( pParent, true ) )
	{
		vehicledoorDebugf2("CCarDoor::SetShutAndLatched latching door that should be force to animate open" );
		return;
	}

	ClearFlag(DRIVEN_MASK);
	SetFlag(DRIVEN_SHUT);

#if __BANK
	if( pParent &&
		NetworkInterface::IsGameInProgress() &&
		pParent->GetNetworkObject() &&
		static_cast< CNetObjPhysical*>( pParent->GetNetworkObject() )->IsInCutsceneLocallyOrRemotely() )
	{
		sysStack::PrintStackTrace();
	}
#endif // #if __BANK


	m_fTargetRatio = 0.0f;
	m_fOldTargetRatio = 0.0f;
	m_fCurrentRatio = 0.0f;
	m_fOldRatio = 0.0f;
	m_fCurrentSpeed = 0.0f;

	m_uTimeSetShutAndLatched = fwTimer::GetSystemTimeInMilliseconds();
	m_fAnglePastLimits = 0.0f;

	if(GetFlag(IS_ARTICULATED))
	{
		// force door shut and latch it
		if(!GetIsLatched(pParent))
			SetLatch(pParent);

		if(GetFragChild() > 0)
		{
			pParent->GetVehicleFragInst()->SetDontBreakFlagAllChildren(GetFragChild());
		}
	}

	if( bApplyForceForDoorClosed &&
		GetHierarchyId() != VEH_BOOT_2 &&
		pParent->GetMass() > 50.0f )
	{
		CTaskVehicleFSM::ApplyForceForDoorClosed(*pParent, (s32)m_nBoneIndex, CTaskCloseVehicleDoorFromInside::ms_Tunables.m_CloseDoorForceMultiplier);
	}

	// fully smash this door's window if it is already partially smashed 
	eHierarchyId windowId = pParent->GetWindowIdFromDoor(m_DoorId);
	if (windowId!=VEH_INVALID_ID)
	{	
		const s32 windowBoneIndex = pParent->GetBoneIndex(windowId);
		if (windowBoneIndex>=0)
		{
			const s32 windowComponentId = pParent->GetFragInst()->GetComponentFromBoneIndex(windowBoneIndex);
			if (windowComponentId>-1)
			{
				// Don't smash bullet proof windows if they're "broken" (ie have bullet hole decals).
				bool bHasBulletProofGlass = pParent->GetVehicleModelInfo() && (pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_BULLETPROOF_GLASS) || pParent->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_BULLET_RESISTANT_GLASS));
				if (pParent->IsBrokenFlagSet(windowComponentId) && !bHasBulletProofGlass)
				{
					pParent->SmashWindow(windowId, VEHICLEGLASSFORCE_WEAPON_IMPACT, true);	
				}
			}
		}

		// EMERUS has two pieces of glass on each door, run again on second bone
		if (MI_CAR_EMERUS.IsValid() && pParent->GetModelIndex() == MI_CAR_EMERUS)
		{
			eHierarchyId secondWindowId = (windowId == VEH_WINDOW_LF ? VEH_WINDOW_LM : (windowId == VEH_WINDOW_RF ? VEH_WINDOW_RM : VEH_INVALID_ID));
			if (secondWindowId != VEH_INVALID_ID)
			{
				const s32 windowBoneIndex = pParent->GetBoneIndex(secondWindowId);
				if (windowBoneIndex >= 0)
				{
					const s32 windowComponentId = pParent->GetFragInst()->GetComponentFromBoneIndex(windowBoneIndex);
					if (windowComponentId > -1 && pParent->IsBrokenFlagSet(windowComponentId)) // No bulletproof windows on EMERUS, can skip that check
					{
						pParent->SmashWindow(secondWindowId, VEHICLEGLASSFORCE_WEAPON_IMPACT, true);
					}
				}
			}
		}
	}

	if( pParent &&
		m_linkedDoorId != VEH_INVALID_ID )
	{
		pParent->GetDoorFromId( m_linkedDoorId )->SetShutAndLatched( pParent, false );
	}
}

bool CCarDoor::RecentlySetShutAndLatched(const unsigned delta)
{
	return ((fwTimer::GetSystemTimeInMilliseconds() - m_uTimeSetShutAndLatched) < delta);
}

bool CCarDoor::RecentlyBeingUsed(const unsigned delta)
{
	return ((fwTimer::GetSystemTimeInMilliseconds() - m_uTimeSinceBeingUsed) < delta);
}

void CCarDoor::SetOpenAndUnlatched(CVehicle* pParent, u32 flags)
{
	vehicledoorDebugf2("CCarDoor::SetOpenAndUnlatched pParent[%p][%s][%s]",pParent,pParent ? pParent->GetModelName() : "",pParent && pParent->GetNetworkObject() ? pParent->GetNetworkObject()->GetLogName() : "");

	ClearFlag(DRIVEN_MASK);
	SetFlag(flags);
	
	phJoint* pJoint = NULL;
	fragHierarchyInst* pHierInst = NULL;
	phArticulatedCollider *pArticulatedCollider = NULL;
	fragInst *pFragInst = pParent->GetFragInst();

	if(pFragInst)
	{
		bool bWasInactive = false;
		if(CPhysics::GetLevel()->IsInactive(pFragInst->GetLevelIndex()))
		{
			bWasInactive = true;
			pParent->ActivatePhysics();
		}

		if(GetIsLatched(pParent))
		{
			BreakLatch(pParent);
		}

		if(pFragInst->GetCached() && pFragInst->GetCacheEntry()->GetHierInst())
		{
			pHierInst = pFragInst->GetCacheEntry()->GetHierInst();
			pArticulatedCollider = pHierInst->articulatedCollider;			

			if(pArticulatedCollider && pArticulatedCollider->IsArticulated() && GetFragChild() > 0)
			{
				// B*2038766: Make sure the collider matrix matches the instance matrix if the vehicle was inactive when this was called.
				// This covers any cases where the vehicle is inactive, the inst gets moved, and then a door is opened with this method. 
				// This could conceivably happen in a network game on a clone vehicle.
				if(bWasInactive)
					pArticulatedCollider->SetColliderMatrixFromInstance();

				int linkIndex = pArticulatedCollider->GetLinkFromComponent(GetFragChild());
				if(linkIndex > 0)
				{
					pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
				}

				m_fTargetRatio = NetworkInterface::IsGameInProgress() ? 1.0f : 0.95f; //MP - ensure this is 1.0f because it is used as a multiplicative ratio in other methods so it can progressively close a door incorrectly.  Keep SP the same.
				float fOpen = m_fTargetRatio * m_fOpenAmount + (1.0f - m_fCurrentRatio) * m_fClosedAmount;
				vehicledoorDebugf2("CCarDoor::SetOpenAndUnlatched--set fOpen[%f] m_fTargetRatio[%f] m_fOpenAmount[%f] m_fCurrentRatio[%f] m_fClosedAmount[%f]",fOpen,m_fTargetRatio,m_fOpenAmount,m_fCurrentRatio,m_fClosedAmount);
				DriveDoorOpen(fOpen, pJoint, pParent, pArticulatedCollider);
			}
		}
	}


	if( pParent &&
		m_linkedDoorId != VEH_INVALID_ID )
	{
		pParent->GetDoorFromId( m_linkedDoorId )->SetOpenAndUnlatched( pParent, flags );
	}
}

void CCarDoor::SetLooseLatch(CVehicle* UNUSED_PARAM(pParent))
{
	if(GetFlag(IS_ARTICULATED) && GetFlag(DRIVEN_SHUT))
	{
		SetFlag(LOOSE_LATCHED_DOOR);
	}
}

bool CCarDoor::GetIsLatched(CVehicle* pParent)
{
	if(!GetFlag(IS_ARTICULATED) || m_nFragGroup < 0)
		return false;

	if(!GetIsIntact(pParent))
		return false;

	
	if(pParent->GetVehicleFragInst()->GetCached())
	{
		// Check to see that the latchJoints have been allocated
		fragHierarchyInst* pHierInst = pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst();
		if( pHierInst->latchedJoints != NULL )
		{
			// If so check against the associated frag group
			if( pHierInst->latchedJoints->IsSet(m_nFragGroup) )
				return true;
		}
		// Otherwise the rear door latch frags may have not been allocated so we need to check if the door *will be* latched.
		else
		{
			float fLatchStrength = pParent->GetFragInst()->GetTypePhysics()->GetGroup( m_nFragGroup )->GetLatchStrength();
			if( fLatchStrength == -1 || fLatchStrength > 0.0f )
			{
				return true;
			}
		}
	}

	return false;
}

void CCarDoor::SetDoorAllowedToBeBrokenOff(bool doorAllowedToBeBrokenOff, CVehicle* pParent)
{
	m_bDoorAllowedToBeBrokenOff = doorAllowedToBeBrokenOff;
	if(!doorAllowedToBeBrokenOff && GetFragChild() > 0)
	{

#if __BANK
		Displayf("[CAR DOOR DEBUG] [Door: %p] CCarDoor::SetDoorAllowedToBeBrokenOff - Door set to be unbreakable | %s", this, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		sysStack::PrintStackTrace();
#endif

		pParent->GetVehicleFragInst()->SetDontBreakFlagAllChildren(GetFragChild());
	}

	if( pParent &&
		m_linkedDoorId != VEH_INVALID_ID )
	{
		pParent->GetDoorFromId( m_linkedDoorId )->SetDoorAllowedToBeBrokenOff( doorAllowedToBeBrokenOff, pParent );
	}
}

void CCarDoor::SetLatch(CVehicle* pParent)
{
	if(!GetFlag(IS_ARTICULATED))
		return;

	if(!GetIsIntact(pParent))
		return;

	if(!pParent || !pParent->GetVehicleFragInst())
		return;
	
	Assert(!GetIsLatched(pParent));

    if(m_nFragChild > 0
    && pParent->GetVehicleFragInst()->GetCacheEntry()
    && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()
    && pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->latchedJoints)
	{
		pParent->GetVehicleFragInst()->CloseLatchAbove(m_nFragChild);

		// Second part of this hack
		// set the link index back to what it was
		if( m_bResetComponentToLinkIndex &&
			pParent->GetVehicleFragInst()->GetArticulatedCollider()->GetLinkFromComponent( m_nFragChild ) == 0 )
		{
			pParent->GetVehicleFragInst()->GetCacheEntry()->GetHierInst()->articulatedCollider->SetLinkForComponent( m_nFragChild, 1 );
		}

		phBoundComposite* pThisBoundComposite = static_cast<phBoundComposite*>(pParent->GetVehicleFragInst()->GetArchetype()->GetBound());
		phBoundComposite* pTemplateBoundComposite = pParent->GetVehicleFragInst()->GetTypePhysics()->GetCompositeBounds();

		// turn off collision with door and the rest of the world when it's shut
		// NOTE: The camera shape tests are explicitly included here
		bool bHasTypeAndIncludeFlags = (pThisBoundComposite->GetTypeAndIncludeFlags() != NULL);
		u32 nIncludeFlags = ArchetypeFlags::GTA_CAR_DOOR_LATCHED_INCLUDE_TYPES | ArchetypeFlags::GTA_CAMERA_TEST;
		if(bHasTypeAndIncludeFlags)
		{
			CVehicleModelInfoDoors* pDoorExtension = pParent->GetVehicleModelInfo()->GetExtension<CVehicleModelInfoDoors>();
			if( ( pDoorExtension && pDoorExtension->ContainsThisDoorWithCollision(m_DoorId) ) ||
				m_DoorId == VEH_BOOT_2 )
			{
				// Don't turn off the collision for this door if it is marked as not being covered by the vehicle's convex hull.
				nIncludeFlags = ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;
			}

			pThisBoundComposite->SetIncludeFlags(m_nFragChild, nIncludeFlags);
		}

		int nParentGroup = pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllChildren()[m_nFragChild]->GetOwnerGroupPointerIndex();
		fragTypeGroup* pParentGroup = pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[nParentGroup];
		if(pParentGroup)
		{
			for (int childIndex = 0; childIndex < pParentGroup->GetNumChildren(); ++childIndex)
			{
				int nChild = pParentGroup->GetChildFragmentIndex() + childIndex;
				pThisBoundComposite->SetCurrentMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));
				pThisBoundComposite->SetLastMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));

				//Ensure that all bounds associated with this door are flagged to include the camera shapetests.
				if(bHasTypeAndIncludeFlags)
				{
					u32 nChildIncludeFlags = nIncludeFlags | ArchetypeFlags::GTA_CAMERA_TEST;
					pThisBoundComposite->SetIncludeFlags(nChild, nChildIncludeFlags);
				}
			}
		}

		for(int groupIndex = 0; groupIndex < pParentGroup->GetNumChildGroups(); ++groupIndex)
		{
			fragTypeGroup* pGroup = pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[pParentGroup->GetChildGroupsPointersIndex() + groupIndex];
			if(pGroup)
			{
				for (int childIndex = 0; childIndex < pGroup->GetNumChildren(); ++childIndex)
				{
					int nChild = pGroup->GetChildFragmentIndex() + childIndex;
					pThisBoundComposite->SetCurrentMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));
					pThisBoundComposite->SetLastMatrix(nChild, pTemplateBoundComposite->GetCurrentMatrix(nChild));

					//Ensure that all bounds associated with this door are flagged to include the camera shapetests.
					if(bHasTypeAndIncludeFlags)
					{
						u32 nChildIncludeFlags = nIncludeFlags | ArchetypeFlags::GTA_CAMERA_TEST;
						pThisBoundComposite->SetIncludeFlags(nChild, nChildIncludeFlags);
					}
				}

				// if we've messed with the bound include flags we need to re-initialse the mod collision
				pParent->InitialiseModCollision();
			}
		}

		if(pParent->InheritsFromPlane())
		{
			static_cast<CPlane *>(pParent)->UpdateWindowBound(pParent->GetWindowIdFromDoor(m_DoorId));
		}

        // Make sure the bone is reset otherwise the door geometry wont move. We do this long-hand instead of using
		// crSkeleton::PartialReset() to avoid a single frame of the window popping up as the door is latched on vehicles
		// which lower the window via animation.
		crSkeleton* pSkel = pParent->GetSkeleton();
		Mat34V& localMat = pSkel->GetLocalMtx(m_nBoneIndex);
		localMat = pSkel->GetSkeletonData().GetDefaultTransform(m_nBoneIndex);
		pSkel->PartialUpdate(m_nBoneIndex);
	}
}

void SetIncludeFlagsIfNotCleared(phBoundComposite* pBoundComposite, int componentIndex, u32 newIncludeFlags)
{
	if(pBoundComposite->GetIncludeFlags(componentIndex) != 0)
	{
		pBoundComposite->SetIncludeFlags(componentIndex, newIncludeFlags);
	}
}

//
void CCarDoor::BreakLatch(CVehicle* pParent)
{
	if(!pParent->IsNetworkClone() && !GetFlag(IS_ARTICULATED))
		return;

	if( !GetIsLatched( pParent ) )
	{
		return;
	}

	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	//@@: location CCARDOOR_BREAKLATCH_ENTRY
	if( m_nFragChild > 0 && pFragInst && pFragInst->GetCacheEntry() )
	{
		// Force allocate the latched joint frags 
		pFragInst->ForceArticulatedColliderMode();
	
		if( pFragInst->GetCacheEntry()->GetHierInst()
			&& pFragInst->GetCacheEntry()->GetHierInst()->latchedJoints )
		{
			// GTAV - B*2404868 - Only do this for the insurgent and limo2 as it causes a crash with the Osiris (B*2290032) but the previous fix for that unfixes B*1917554
			bool hasTurretHatch = ( MI_CAR_INSURGENT.IsValid() &&
								    pParent->GetModelIndex() == MI_CAR_INSURGENT.GetModelIndex() ) ||
								  ( MI_CAR_LIMO2.IsValid() &&
								    pParent->GetModelIndex() == MI_CAR_LIMO2.GetModelIndex() ) ||
								  ( MI_VAN_BOXVILLE5.IsValid() &&
									pParent->GetModelIndex() == MI_VAN_BOXVILLE5.GetModelIndex() ) ||
								  ( MI_CAR_INSURGENT3.IsValid() &&
									pParent->GetModelIndex() == MI_CAR_INSURGENT3.GetModelIndex() ) ||
								  ( MI_CAR_MENACER.IsValid() &&
									pParent->GetModelIndex() == MI_CAR_MENACER.GetModelIndex() );

			if( !m_bResetComponentToLinkIndex &&
				hasTurretHatch &&
				pFragInst->GetArticulatedCollider() &&
				pFragInst->GetArticulatedCollider()->GetLinkFromComponent( m_nFragChild ) > 0 )
			{
				// this is a hack to fix GTAV B*1917554
				// if the door already has a link it is lost when the door closes
				// so we will need to reset the link
				m_bResetComponentToLinkIndex = true;
			}

			pFragInst->OpenLatchAbove(m_nFragChild);

			// turn on collision with door and the rest of the world when it's swinging open
			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
			Assert(pBoundComp->GetBound(m_nFragChild)->GetType() != phBound::BVH); //A door should never be a BVH
			if(pBoundComp->GetTypeAndIncludeFlags())
			{
				//Ensure that all bounds associated with this door are flagged not to include the camera shapetests.

				u32 includeFlags = GenerateUnlatchedCarDoorCollisionIncludeFlags(pParent, m_DoorId);

				if( m_bResetComponentToLinkIndex )
				{
					includeFlags &= ~ArchetypeFlags::GTA_PED_TYPE;
					includeFlags &= ~ArchetypeFlags::GTA_RAGDOLL_TYPE;
				}
				SetIncludeFlagsIfNotCleared(pBoundComp, m_nFragChild, includeFlags);

				int nParentGroup = pFragInst->GetTypePhysics()->GetAllChildren()[m_nFragChild]->GetOwnerGroupPointerIndex();
				fragTypeGroup* pParentGroup = pFragInst->GetTypePhysics()->GetAllGroups()[nParentGroup];
				if(pParentGroup)
				{
					for (int childIndex = 0; childIndex < pParentGroup->GetNumChildren(); ++childIndex)
					{
						int nChild = pParentGroup->GetChildFragmentIndex() + childIndex;
						u32 nChildIncludeFlags = includeFlags;
						SetIncludeFlagsIfNotCleared(pBoundComp, nChild, nChildIncludeFlags);
					}
				}

				for(int groupIndex = 0; groupIndex < pParentGroup->GetNumChildGroups(); ++groupIndex)
				{
					fragTypeGroup* pGroup = pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[pParentGroup->GetChildGroupsPointersIndex() + groupIndex];
					if(pGroup)
					{
						for (int childIndex = 0; childIndex < pGroup->GetNumChildren(); ++childIndex)
						{
							int nChild = pGroup->GetChildFragmentIndex() + childIndex;
							u32 nChildIncludeFlags = includeFlags;
							SetIncludeFlagsIfNotCleared(pBoundComp, nChild, nChildIncludeFlags);
						}
					}
				}
			}

			if(GetHierarchyId()==VEH_BONNET || GetHierarchyId()==VEH_BOOT)
			{
				// find the associated joint
				fragHierarchyInst* pHierInst = NULL;
				if(pFragInst && pFragInst->GetCached())
					pHierInst = pFragInst->GetCacheEntry()->GetHierInst();

				phArticulatedCollider* pArticulatedCollider = NULL;
				if(pHierInst)
					pArticulatedCollider = pHierInst->articulatedCollider;

				phJoint1Dof* pDoorJoint = NULL;
				if(pArticulatedCollider && m_nFragChild > 0)
				{
					int linkIndex = pArticulatedCollider->GetLinkFromComponent(m_nFragChild);
					if(linkIndex > 0)
					{
						phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
						if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
							pDoorJoint = static_cast<phJoint1Dof*>(pJoint);
						else
						{
							Assert(GetHierarchyId()!=VEH_BONNET && GetHierarchyId()!=VEH_BOOT); // don't currently support sliding boots or bonnets, could just add the limits back in below like 1dof doors
							return;
						}
					}
				}

				if(pDoorJoint)
				{
					//float fMin, fMax;
					//pDoorJoint->GetAngleLimits(fMin, fMax);
					if(GetHierarchyId()==VEH_BONNET)
					{
						if(m_fOpenAmount > m_fClosedAmount)
							pDoorJoint->SetAngleLimits(m_fClosedAmount + sfBonnetUnlatchedAngle, m_fOpenAmount);
						else
							pDoorJoint->SetAngleLimits(m_fOpenAmount, m_fClosedAmount - sfBonnetUnlatchedAngle);
					}
					else if(GetHierarchyId()==VEH_BOOT)
					{
						if(m_fOpenAmount > m_fClosedAmount)
							pDoorJoint->SetAngleLimits(m_fClosedAmount + sfBootUnlatchedAngle, m_fOpenAmount);
						else
							pDoorJoint->SetAngleLimits(m_fOpenAmount, m_fClosedAmount - sfBootUnlatchedAngle);
					}
				}
			}
		}
	}
	
	if( pParent &&
		m_linkedDoorId != VEH_INVALID_ID )
	{
		pParent->GetDoorFromId( m_linkedDoorId )->BreakLatch( pParent );
	}
}

bool CCarDoor::GetIsIntact(const CVehicle* ASSERT_ONLY(pParent)) const
{
#if __ASSERT
	if(pParent && pParent->GetVehicleFragInst() && m_nFragGroup >= 0)
	{
		if(pParent->GetVehicleFragInst()->GetCacheEntry() && pParent->GetVehicleFragInst()->GetCacheEntry()->IsGroupBroken(m_nFragGroup))
		{
			Assertf(GetFlag(IS_BROKEN_OFF), "Door on '%s' isn't marked as broken when it is broken on the cache entry.",pParent->GetModelName());
		}
		else
		{
			Assertf(!GetFlag(IS_BROKEN_OFF), "Door on '%s' is marked as broken when it isn't broken on the cache entry.",pParent->GetModelName());
		}
	}
#endif // __ASSERT
	return !GetFlag(IS_BROKEN_OFF);
}

fragInst* CCarDoor::BreakOff(CVehicle* pParent, bool UNUSED_PARAM(bNetEvent))
{
	if (GetFragChild() > 0)
	{
		pParent->PartHasBrokenOff(m_DoorId);
		if (pParent->CarPartsCanBreakOff() && pParent->CanCreateBrokenPart())
		{
			if(pParent && pParent->GetSkeleton() && GetFlag(IS_ARTICULATED) && m_vDeformation.IsNonZero())
			{
				int nDoorBoneIndex = pParent->GetBoneIndex(m_DoorId);

				if(nDoorBoneIndex > -1)
				{
					crSkeleton* pSkel = pParent->GetSkeleton();
					Matrix34 doorMatrixLocal = pParent->GetLocalMtx(nDoorBoneIndex);
					// Reset bone position
					Vector3 vDefaultBonePos = RCC_VECTOR3(pSkel->GetSkeletonData().GetBoneData(nDoorBoneIndex)->GetDefaultTranslation());
					doorMatrixLocal.d.Set(vDefaultBonePos);
					// pre translate by deformation offset
					doorMatrixLocal.d.Add(m_vDeformation);
					// post translate by inverse deformation offset
					Vector3 vecPostRotationOffset;
					doorMatrixLocal.Transform3x3(m_vDeformation, vecPostRotationOffset);
					doorMatrixLocal.d.Subtract(vecPostRotationOffset);

					Vector3 vOffset = doorMatrixLocal.d - vDefaultBonePos;
					doorMatrixLocal.UnTransform3x3(vOffset);
					if(vOffset.IsNonZero())
					{
						u8 nParentGroup = pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllChildren()[m_nFragChild]->GetOwnerGroupPointerIndex();
						fragInst::ComponentBits groupsBreaking;
						pParent->GetVehicleFragInst()->GetTypePhysics()->GetGroupsAbove(nParentGroup,groupsBreaking);

						CVehicleDeformation* pVehDeformation = pParent->GetVehicleDamage() ? pParent->GetVehicleDamage()->GetDeformation() : NULL;
						void* basePtr = pVehDeformation->HasDamageTexture() ? pVehDeformation->LockDamageTexture(grcsRead) : NULL;
						if(basePtr != NULL)
						{
							pParent->GetVehicleDamage()->GetDeformation()->ApplyDamageToBound(basePtr, &groupsBreaking, &vOffset);
							pVehDeformation->UnLockDamageTexture();
						}
					}
				}
			}

			fragInst* pFragInst = pParent->GetFragInst()->BreakOffAbove(GetFragChild());

			if(GetFlag(AERO_BONNET))
			{
				phCollider* pBonnetCollider = pFragInst ? CPhysics::GetSimulator()->GetCollider(pFragInst->GetLevelIndex()) : NULL;

				if(pFragInst && pBonnetCollider)
				{
					Vector3 vecForward(VEC3V_TO_VECTOR3(pParent->GetTransform().GetB()));
					float fFwdSpeed = DotProduct(vecForward, pParent->GetVelocity());

					bool bPlayerFirstPersonView = pParent->GetStatus()==STATUS_PLAYER && camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera();

					if(!bPlayerFirstPersonView)
					{
						Vector3 vecBonnetImpulse;
						vecBonnetImpulse.Set(sfBonnetBreakSpdImpulse * fFwdSpeed * vecForward);
						vecBonnetImpulse.Add(sfBonnetBreakUpImpulse * fFwdSpeed * VEC3V_TO_VECTOR3(pParent->GetTransform().GetC()));
						vecBonnetImpulse.Scale(pFragInst->GetArchetype()->GetMass());

						Vector3 vecBonnetImpulsePos;
						Matrix34 m = RCC_MATRIX34(pFragInst->GetMatrix());
						vecBonnetImpulsePos.Set(m.d);
						vecBonnetImpulsePos.Add(sfBonnetBreakFwdOffset * m.b);
						vecBonnetImpulsePos.Add(sfBonnetBreakFwdOffset * m.c);
						float fSideOffset = fwRandom::GetRandomNumberInRange(-1.0f, 1.0f);
						vecBonnetImpulsePos.Add(sfBonnetBreakSideOffset * fSideOffset * vecForward);

						pBonnetCollider->ApplyImpulse(vecBonnetImpulse, vecBonnetImpulsePos);
					}

					// only trigger the break SFX when traveling at speed
					if(fFwdSpeed > 13.f)
					{
						Matrix34 m = RCC_MATRIX34(pFragInst->GetMatrix());
						pParent->GetVehicleAudioEntity()->TriggerDoorBreakOff(m.d, m_DoorId == VEH_BONNET, pFragInst);
					}
				}
			}
			return pFragInst;
		}
		else
		{
			pParent->GetFragInst()->DeleteAbove(GetFragChild());
		}
		CVehicle::ClearLastBrokenOffPart();
	}
	return NULL;
}

void CCarDoor::Fix(CVehicle* pParent)
{
	vehicledoorDebugf2("CCarDoor::Fix --> m_fCurrentRatio[0.0] %s", pParent ? pParent->GetLogName() : "" );

	if(!GetFlag(BIKE_DOOR))
	{
		ClearFlag(DRIVEN_MASK);
		ClearFlag(LOOSE_LATCHED_DOOR);
		SetFlag(DRIVEN_SHUT);

		m_vDeformation = VEC3_ZERO;
		m_fTargetRatio = 0.0f;
		m_fOldTargetRatio = 0.0f;
		m_fCurrentRatio = 0.0f;
		m_fOldRatio = 0.0f;
		m_fCurrentSpeed = 0.0f;
		m_fOldAudioRatio = 0.0f;

		m_uTimeSetShutAndLatched = 0; //if ratio is reset here ensure this is also reset
		m_uTimeSinceBeingUsed = 0;

		if(!GetIsIntact(pParent))
		{
			pParent->GetVehicleFragInst()->RestoreAbove(GetFragChild());
			Assert(!GetFlag(IS_BROKEN_OFF));
			SetLatch(pParent);
		}
		// Reset the bone matrix in MP game, otherwise the door geometry might still stays
		// at the loose latched position for clones. Note, the SetLatch function also resets the bone matrix,
		// we don't need to do it again if SetLatch has been called- B* 2184251
		else if(NetworkInterface::IsGameInProgress())
		{
			crSkeleton* pSkel = pParent->GetSkeleton();
			Mat34V& localMat = pSkel->GetLocalMtx(m_nBoneIndex);
			localMat = pSkel->GetSkeletonData().GetDefaultTransform(m_nBoneIndex);
			pSkel->PartialUpdate(m_nBoneIndex);
		}
	}
}

void CCarDoor::SetBikeDoorUnConstrained(float fCurrentAngle)
{
	Assert(GetFlag(BIKE_DOOR));

	m_fClosedAmount = fCurrentAngle;

	// set the door as shut when it's unconstrained, so peds have to open it (pick the bike up) to get in
	vehicledoorDebugf3("CCarDoor::SetBikeDoorUnConstrained --> m_fCurrentRatio[0.0]");
	m_fTargetRatio = m_fCurrentRatio = 0.0f;
	m_fOldTargetRatio = m_fOldRatio = 0.0f;
	ClearFlag(DRIVEN_MASK);
	SetFlag(DRIVEN_SHUT);
}

void CCarDoor::SetBikeDoorConstrained()
{
	// when bike is constrained, set to to be considered as open, so peds can just get in/on without picking up
	vehicledoorDebugf3("CCarDoor::SetBikeDoorConstrained --> m_fCurrentRatio[1.0]");
	m_fTargetRatio = m_fCurrentRatio = 1.0f;
	m_fOldTargetRatio = m_fOldRatio = 1.0f;
	ClearFlag(DRIVEN_MASK);
	SetFlag(DRIVEN_NORESET);
}


#if __BANK
void CCarDoor::PrintDoorFlags( CVehicle* pParent )
{
	physicsDisplayf("CCarDoor::PrintDoorFlags - [%s] %s: %d |%s: %d | %s: %d | %s: %d | %s: %d | %s: %d | %s: %d | %s: %d | %s: %d | %s: %d | %s: %d | %s: %d | %s: %d\n", 
		pParent->GetDebugName(),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_NORESET_NETWORK), GetFlag(CCarDoor::DRIVEN_NORESET_NETWORK),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SHUT), GetFlag(CCarDoor::DRIVEN_SHUT),		
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_AUTORESET), GetFlag(CCarDoor::DRIVEN_AUTORESET),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_NORESET), GetFlag(CCarDoor::DRIVEN_NORESET),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_GAS_STRUT), GetFlag(CCarDoor::DRIVEN_GAS_STRUT),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SWINGING), GetFlag(CCarDoor::DRIVEN_SWINGING),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SMOOTH), GetFlag(CCarDoor::DRIVEN_SMOOTH),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_SPECIAL), GetFlag(CCarDoor::DRIVEN_SPECIAL),	
		CCarDoor::GetDoorFlagString(CCarDoor::WILL_LOCK_SWINGING), GetFlag(CCarDoor::DRIVEN_SWINGING),
		CCarDoor::GetDoorFlagString(CCarDoor::WILL_LOCK_DRIVEN), GetFlag(CCarDoor::DRIVEN_SMOOTH),
		CCarDoor::GetDoorFlagString(CCarDoor::LOOSE_LATCHED_DOOR), GetFlag(CCarDoor::DRIVEN_SPECIAL),	
		CCarDoor::GetDoorFlagString(CCarDoor::RELEASE_AFTER_DRIVEN), GetFlag(CCarDoor::DRIVEN_SMOOTH),
		CCarDoor::GetDoorFlagString(CCarDoor::DRIVEN_BY_PHYSICS), GetFlag(CCarDoor::DRIVEN_SPECIAL));	
}
#endif // __BANK


float CBouncingPanel::BOUNCE_SPRING_DAMP_MULT = 0.3f;
float CBouncingPanel::BOUNCE_SPRING_RETURN_MULT = 25.0f;
float CBouncingPanel::BOUNCE_ACCEL_LIMIT = 5.0f;

float CBouncingPanel::BOUNCE_HANGING_DAMP_MULT = 0.54f;
float CBouncingPanel::BOUNCE_HANGING_RETURN_MULT = 5.0f;

	
void CBouncingPanel::ResetPanel()
{
	m_nComponentIndex = -1;
}

void CBouncingPanel::SetPanel(s32 nIndex, eBounceAxis nAxis, float fMult)
{
	m_nComponentIndex = (s16)nIndex;
	m_nBounceAxis = nAxis;
	m_fBounceApplyMult = fMult;
	
	m_vecBounceAngle = Vector3(0.0f,0.0f,0.0f);
	m_vecBounceTurnSpeed = Vector3(0.0f,0.0f,0.0f);
}


dev_float sfBounceLimitElasticity = 0.5f;
// 
void CBouncingPanel::ProcessPanel(CVehicle *pVehicle, const Vector3& vecOldMoveSpeed, const Vector3& vecOldTurnSpeed, float fReturnMult, float fDampMult)
{
	if(pVehicle->GetSkeleton()==NULL)
		return;

	if(pVehicle->GetBoneIndex((eHierarchyId)m_nComponentIndex)==-1)
		return;

	Matrix34 &boneMat = pVehicle->GetLocalMtxNonConst(pVehicle->GetBoneIndex((eHierarchyId)m_nComponentIndex));

	Vector3 vecBoneOffset = VEC3V_TO_VECTOR3(pVehicle->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(boneMat.d)));
	
	Vector3 vecVelocity = pVehicle->GetLocalSpeed(vecBoneOffset);
	Vector3 vecOldVelocity = CrossProduct(vecOldTurnSpeed, vecBoneOffset) + vecOldMoveSpeed;
	Vector3 vecAccel = VEC3V_TO_VECTOR3(pVehicle->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V((vecVelocity - vecOldVelocity) * fwTimer::GetInvTimeStep())));

	if(m_nBounceAxis==BOUNCE_AXIS_X || m_nBounceAxis==BOUNCE_AXIS_NEG_X)	// x-axis
	{
		float fApplyMult = (m_nBounceAxis==BOUNCE_AXIS_NEG_X) ? m_fBounceApplyMult : -m_fBounceApplyMult;
		m_vecBounceTurnSpeed.y += fApplyMult * fwTimer::GetTimeStep() * Clamp(vecAccel.z, -BOUNCE_ACCEL_LIMIT, BOUNCE_ACCEL_LIMIT);
		m_vecBounceTurnSpeed.z += fApplyMult * fwTimer::GetTimeStep() * Clamp(vecAccel.y, -BOUNCE_ACCEL_LIMIT, BOUNCE_ACCEL_LIMIT);
	}
	else if(m_nBounceAxis==BOUNCE_AXIS_Y || m_nBounceAxis==BOUNCE_AXIS_NEG_Y)	// y-axis
	{
		float fApplyMult = (m_nBounceAxis==BOUNCE_AXIS_NEG_Y) ? m_fBounceApplyMult : -m_fBounceApplyMult;
		m_vecBounceTurnSpeed.x += fApplyMult * fwTimer::GetTimeStep() * Clamp(vecAccel.z, -BOUNCE_ACCEL_LIMIT, BOUNCE_ACCEL_LIMIT);
		m_vecBounceTurnSpeed.z += fApplyMult * fwTimer::GetTimeStep() * Clamp(vecAccel.y, -BOUNCE_ACCEL_LIMIT, BOUNCE_ACCEL_LIMIT);
	}
	else if(m_nBounceAxis==BOUNCE_AXIS_Z || m_nBounceAxis==BOUNCE_AXIS_NEG_Z)	// z-axis
	{
		float fApplyMult = (m_nBounceAxis==BOUNCE_AXIS_NEG_Z) ? m_fBounceApplyMult : -m_fBounceApplyMult;
		m_vecBounceTurnSpeed.x += fApplyMult * fwTimer::GetTimeStep() * Clamp(vecAccel.y, -BOUNCE_ACCEL_LIMIT, BOUNCE_ACCEL_LIMIT);
		m_vecBounceTurnSpeed.y += fApplyMult * fwTimer::GetTimeStep() * Clamp(vecAccel.x, -BOUNCE_ACCEL_LIMIT, BOUNCE_ACCEL_LIMIT);
	}
	
	if(fReturnMult > -1.0f && fDampMult > -1.0f)
	{
		m_vecBounceTurnSpeed -= fReturnMult * m_vecBounceAngle * fwTimer::GetTimeStep();
		m_vecBounceTurnSpeed *= rage::Powf(fDampMult, fwTimer::GetTimeStep());
	}
	else if(m_nBounceAxis==BOUNCE_AXIS_Z)	// z-axis
	{
		m_vecBounceTurnSpeed -= BOUNCE_HANGING_RETURN_MULT * m_vecBounceAngle * fwTimer::GetTimeStep();
		m_vecBounceTurnSpeed *= rage::Powf(BOUNCE_HANGING_DAMP_MULT, fwTimer::GetTimeStep());
	}
	else
	{
		m_vecBounceTurnSpeed -= BOUNCE_SPRING_RETURN_MULT * m_vecBounceAngle * fwTimer::GetTimeStep();
		m_vecBounceTurnSpeed *= rage::Powf(BOUNCE_SPRING_DAMP_MULT, fwTimer::GetTimeStep());
	}
	
	m_vecBounceAngle += m_vecBounceTurnSpeed * fwTimer::GetTimeStep();
	
	if(m_nBounceAxis==BOUNCE_AXIS_X || m_nBounceAxis==BOUNCE_AXIS_NEG_X)	// x-axis
	{
		if(boneMat.d.x * boneMat.d.y * m_vecBounceAngle.z > 0.0f)
		{
			m_vecBounceAngle.z = 0.0f;
			m_vecBounceTurnSpeed.z *= -sfBounceLimitElasticity;
		}
		if(boneMat.d.x * m_vecBounceAngle.y < 0.0f)
		{
			m_vecBounceAngle.y= 0.0f;
			m_vecBounceTurnSpeed.y *= -sfBounceLimitElasticity;
		}

		boneMat.a.y = m_vecBounceAngle.z;
		boneMat.a.z = m_vecBounceAngle.y;
		boneMat.a.x = rage::Sqrtf(rage::Max(0.1f, 1.0f - boneMat.a.y*boneMat.a.y - boneMat.a.z*boneMat.a.z));
	}
	else if(m_nBounceAxis==BOUNCE_AXIS_Y || m_nBounceAxis==BOUNCE_AXIS_NEG_Y)	// y-axis
	{
		boneMat.b.x = m_vecBounceAngle.z;
		boneMat.b.z = m_vecBounceAngle.x;
	}
	else if(m_nBounceAxis==BOUNCE_AXIS_Z || m_nBounceAxis==BOUNCE_AXIS_NEG_Z)	// z-axis
	{
		boneMat.c.y = m_vecBounceAngle.x;
		boneMat.c.x = m_vecBounceAngle.y;
	}
}

void CCarDoor::AudioUpdate()
{
	m_fOldAudioRatio = m_fCurrentRatio;
	m_bJustLatched = false;
}

void CCarDoor::TriggeredAudio(const u32 timeInMs)
{
	m_nLastAudioTime = timeInMs;
}

#if GTA_REPLAY
void CCarDoor::SetFlags(u32 flags) 
{ 
	m_nDoorFlags = flags; 
#if __BANK
	if((flags & DRIVEN_NORESET) && !GetFlag(BIKE_DOOR))
	{		
		sysStack::CaptureStackTrace(m_lastSetFlagsCallstack, 32, 1);
		strcpy(m_lastSetFlagsScript, CTheScripts::GetCurrentScriptNameAndProgramCounter());	
		m_drivenNoResetFlagWasSet = true;
	}
#endif
}
#endif

void CCarDoor::SetFlag(u32 nFlag)
{ 
	m_nDoorFlags |= nFlag; 
#if __BANK
	if((nFlag & DRIVEN_NORESET) && !GetFlag(BIKE_DOOR))
	{		
		sysStack::CaptureStackTrace(m_lastSetFlagsCallstack, 32, 1);
		strcpy(m_lastSetFlagsScript, CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m_drivenNoResetFlagWasSet = true;
	}
#endif
}

void CCarDoor::ClearFlag(u32 nFlag) 
{
	m_nDoorFlags &= ~nFlag;
}

#if __BANK
void CCarDoor::OutputDoorStuckDebug()
{
	Displayf("[CAR DOOR DEBUG] Door: %i", GetHierarchyId());
	if(m_drivenNoResetFlagWasSet)
	{
		Displayf("[CAR DOOR DEBUG] [Door: %p] Last thing to set the DRIVEN_NORESET flag on this door: %s", this, m_lastSetFlagsScript);
		sysStack::PrintCapturedStackTrace(m_lastSetFlagsCallstack, 32);
		if(GetFlag(DRIVEN_NORESET))
			Displayf("[CAR DOOR DEBUG] If the door is stuck open, it was probably because of this!");
		else
			Displayf("[CAR DOOR DEBUG] But flag got unset so the door shouldn't be stuck open...");
	}
	else
		Displayf("[CAR DOOR DEBUG] DRIVEN_NORESET wasn't set...");
}
#endif



