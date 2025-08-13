// Title	:	Planes.cpp
// Author	:	Alexander Roger
// Started	:	10/04/2003 
//
//
//
//
// C headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Rage headers
#include "audiosoundtypes/soundcontrol.h"
#include "audioengine/widgets.h"
#include "crskeleton/skeleton.h"
#include "pheffects/wind.h"

#include "pharticulated/joint.h"
#include "pharticulated/joint1dof.h"
#include "pharticulated/articulatedbody.h"
#include "pharticulated/articulatedcollider.h"
#include "phbound/boundcomposite.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/random.h"
#include "fwmaths/vector.h" 

#if !__NO_OUTPUT
#include "fwnet/netchannel.h"
#endif

// Game headers
#include "audio/ambience/ambientaudioentity.h"
#include "audio/environment/environmentgroup.h"
#include "audio/northaudioengine.h"
#include "audio/planeaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "control/gamelogic.h"
#include "control/replay/Replay.h"
#include "camera/gameplay/gameplaydirector.h" 
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "vehicleAi/vehicleintelligence.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicleAi/task/TaskVehicleFlying.h"
#include "vehicleAi/task/TaskVehiclePlayer.h"
#include "vehicleAi/FlyingVehicleAvoidance.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "game/modelIndices.h"
#include "Stats/StatsMgr.h"
#include "game/wind.h"
#include "game/weather.h"
#include "modelInfo/vehicleModelInfo.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/NetworkInterface.h"
#include "pedgroup/pedGroup.h"
#include "peds/ped.h"
#include "peds/pedIntelligence.h"
#include "peds/Ped.h"
#include "peds/PopCycle.h"
#include "physics/physics.h"
#include "physics/gtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "physics/gtaInst.h"
#include "physics/PhysicsHelpers.h"
#include "renderer/lights/AsyncLightOcclusionMgr.h"
#include "renderer/ApplyDamage.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "script/script.h"
#include "Stats/StatsInterface.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "system/pad.h"
#include "task/Physics/TaskNMRelax.h"
#include "timecycle/TimeCycleConfig.h"
#include "peds/PlayerInfo.h"
#include "vehicles/heli.h"
#include "vehicles/planes.h"
#include "vehicles/train.h"
#include "vehicles/metadata/VehicleEntryPointInfo.h"
#include "vehicles/metadata/VehicleLayoutInfo.h"
#include "vehicles/vehicle_channel.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/VehicleGadgets.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "vfx/Systems/VfxBlood.h"
#include "vfx/Systems/VfxWeapon.h"
#include "weapons/explosion.h"
#include "Task/Vehicle/TaskInVehicle.h"


ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()
AUDIO_VEHICLES_OPTIMISATIONS()

#if !__NO_OUTPUT
RAGE_DEFINE_SUBCHANNEL(net, damage_plane, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_damage_plane
#endif

//s32	CPlane::GenPlane_Status = PGEN_NOPLANE;
//s32   CPlane::GenPlane_ModelIndex;
//u32	CPlane::GenPlane_LastTimeGenerated = 0;
//bool	CPlane::GenPlane_Active = true;

bank_float CPlane::ms_fMinEngineSpeed = 0.4f;
bank_float CPlane::ms_fStdEngineSpeed = 0.5f;
bank_float CPlane::ms_fMaxEngineSpeed = 1.0f;
bank_float bfMinJetOnGroundEngineSpeed = 0.20f;
bank_float bfStdJetOnGroundEngineSpeed = 0.25f;
bank_float bfMaxJetOnGroundEngineSpeed = 1.0f;
bank_float CPlane::ms_fEngineSpeedChangeRate = 0.08f; // rate of change
dev_float dfEngineSpeedPassiveChangeRate = 0.005f; // rate of change
bank_float CPlane::ms_fEngineSpeedDropRateInWater = 0.24f;
bank_float CPlane::ms_fEngineSpeedDropRateWhenMissFiring = 1.0f;
dev_float CPlane::ms_fControlSmoothSpeed = 5.0f;
dev_float dfControlSmoothSpeedForAI = 1.0f;
bank_float CPlane::ms_fCeilingLiftCutoffRange = 60.0f;
bank_float CPlane::ms_fPropRenderSpeed= 55.0f;
//bank_float CPlane::ms_fEngineSpeedPropMult = 5.0f; //brings the propellors up to full speed faster

bool CPlane::ms_ReduceDragWithGroundEffect = false;


bank_float CPlane::ms_fTopSpeedDampRate = 0.35f;
bank_float CPlane::ms_fTopSpeedDampMaxHeight = 270.0f;
bank_float CPlane::ms_fTopSpeedDampMinHeight = 230.0f;

#if RSG_PC
bank_float sfPlaneRandomNoiseIdleMultForPlayer = 0.1f;
bank_float sfPlaneRandomNoiseThrottleMultForPlayer = 0.075f;
#else
bank_float sfPlaneRandomNoiseIdleMultForPlayer = 0.3f;
bank_float sfPlaneRandomNoiseThrottleMultForPlayer = 0.2f;
#endif

static dev_float sfPlaneRandomNoiseIdleMultForAI = 0.3f;
static dev_float sfPlaneRandomNoiseThrottleMultForAI = 0.2f;

#if USE_SIXAXIS_GESTURES
bank_float CPlane::MOTION_CONTROL_PITCH_MIN = -1.0f;
bank_float CPlane::MOTION_CONTROL_PITCH_MAX = 1.0f;
bank_float CPlane::MOTION_CONTROL_ROLL_MIN = -1.75f;
bank_float CPlane::MOTION_CONTROL_ROLL_MAX = 1.75f;
bank_float CPlane::MOTION_CONTROL_YAW_MULT = 2.5f;
#endif

//////////////////////////////////////////////////////////////////////////
// Class CAircraftDamageBase 
//////////////////////////////////////////////////////////////////////////

CAircraftDamageBase::CAircraftDamageBase()
	: m_pLastDamageInflictor(NULL)
{
}

CAircraftDamageBase::~CAircraftDamageBase()
{
}

static dev_float sfPlaneCollisionDamageMult = 200.0f;
bank_float bfPlaneLandingDamageMult = 0.75f;

void CAircraftDamageBase::ApplyDamageToPlane(CPlane* pParent, CEntity* pInflictor, eDamageType nDamageType, u32 nWeaponHash, float fDamage, const Vector3& vecPos, const Vector3& vecNorm, const Vector3& UNUSED_PARAM(vecDirn), int nComponent, phMaterialMgr::Id UNUSED_PARAM(nMaterialId), int UNUSED_PARAM(nPieceIndex), const bool UNUSED_PARAM(bFireDriveby), const bool bLandingDamage, const float fDamageRadius)
{
	// Figure out which section we hit
	// This is all hardcoded as a link between components and sections
	// If we want crazy plane variations this will have to modernise

	// The damage for collisions is the impulse mag sum applied
	// Want to divide by plane mass or else collisions against static objects
	// can be a bit too large
	if(nWeaponHash == WEAPONTYPE_RAMMEDBYVEHICLE)
	{
		Assert(pParent->GetMass() > 0.0f);
		fDamage *= sfPlaneCollisionDamageMult/pParent->GetMass();
	}

	if(bLandingDamage)
	{
		// if landing gear is unbreakable, quick return here
		if(pParent->m_nVehicleFlags.bUnbreakableLandingGear)
		{
			return;
		}

		fDamage *= bfPlaneLandingDamageMult;
	}

	int nNumHitSections = 0;
	int pHitSectionIndices[CAircraftDamage::NUM_DAMAGE_SECTIONS];
	FindAllHitSections(nNumHitSections, pHitSectionIndices, pParent, nDamageType, vecPos, nComponent, fDamageRadius);

	if(nNumHitSections > 1)
	{
		fDamage /= nNumHitSections;
	}
	bool canPartsBreakOff = pParent->CarPartsCanBreakOff();
	for(int iHitSectionIdx = 0; iHitSectionIdx < nNumHitSections; iHitSectionIdx++)
	{
		if(pHitSectionIndices[iHitSectionIdx] != INVALID_SECTION && AssertVerify(!pParent->IsNetworkClone()))
		{
			// Hit a damagable part of the plane
			ApplyDamageToSection(pParent, pHitSectionIndices[iHitSectionIdx], nComponent, fDamage, nDamageType, vecPos, vecNorm);

			if( GetSectionHealth(pHitSectionIndices[iHitSectionIdx]) <= 0.0f && 
				!HasSectionBrokenOff(pParent, iHitSectionIdx) && 
				canPartsBreakOff &&
				!pParent->GetVehicleFragInst()->IsBreakingDisabled() )
			{
				// Just killed this part, so make sure its allowed to be broken
				// Look up the group
				fragInstGta* pInst = pParent->GetVehicleFragInst();
				if(physicsVerifyf(pInst,"All vehicles need a frag inst"))
				{
					if(fDamage > GetInstantBreakOffDamageThreshold(pHitSectionIndices[iHitSectionIdx]) || nDamageType == DAMAGE_TYPE_EXPLOSIVE)
					{
						int nGroup = -1;
						fragTypeGroup* pGroup = NULL;
						eHierarchyId nId = GetHierarchyIdFromSection(pHitSectionIndices[iHitSectionIdx]);
						if((nId >= PLANE_FIRST_BREAKABLE_PART && nId < PLANE_FIRST_BREAKABLE_PART + PLANE_NUM_BREAKABLES)
							|| ( nId >= FIRST_LANDING_GEAR && nId < FIRST_LANDING_GEAR + NUM_LANDING_GEARS && pParent->GetModelIndex() != MI_PLANE_MICROLIGHT ))
						{
							// Lookup the frag group from the hierarchy id and see if it has broken off
							// First we need a bone index
							int iBoneIndex = pParent->GetBoneIndex(nId);

							if(iBoneIndex > -1)
							{
								nGroup = pInst->GetGroupFromBoneIndex(iBoneIndex);
								if(nGroup > -1 && pInst->GetTypePhysics())
								{
									pGroup = pInst->GetTypePhysics()->GetAllGroups()[nGroup];
								}
							}
						}

						if(nGroup != -1 && pGroup != NULL)
						{
							// This manually breaks off group
							// but this is not allowed if group has child groups
							//pInst->BreakOffAboveGroup(nGroup);
							BreakOffAboveGroupRecursive(nGroup,pInst);

							// Stop all children in this group breaking
							// Don't use the clear all children function because this clears child groups as well
							// which we don't want to do
							// e.g. if wing can fall off don't want to allow engines to fall off if wing if their health is still OK
							int iChild = pGroup->GetChildFragmentIndex();
							for(int i = 0; i < pGroup->GetNumChildren(); i++)
							{
								pInst->ClearDontBreakFlag(BIT(iChild + i));					
							}

							PostBreakOffSection(pParent, pHitSectionIndices[iHitSectionIdx], nDamageType);
						}
					}			
				}
			}

		}
		else if(nComponent == 0 && AssertVerify(!pParent->IsNetworkClone()))
		{
			ApplyDamageToChassis(pParent, fDamage, nDamageType, nComponent, vecPos);
		}
	}

	//don't override with a null inflictor
	if(pInflictor)
	{
		m_pLastDamageInflictor = pInflictor;
	}
}

void CAircraftDamageBase::InitCompositeBound(CPlane* pParent)
{
	// Find the breakable components and make sure they are flagged not to break off
	// This is really general initphys stuff but don't want to have to do this iteration twice
	// So combined InitCompositeBound and InitPhys


	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pParent->GetVehicleFragInst()->GetArchetype()->GetBound());	
	Assert(pBoundComp);
	Assert(pParent->GetVehicleFragInst()->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE);


	// Stop the frag system breaking off the breakable components

	// Find the breakable groups
	int nStartId = GetFirstBreakablePart();
	fragInstGta* pFragInst = pParent->GetVehicleFragInst();
	FastAssert(pFragInst);	// All vehicles must have a frag inst
	for(int i = 0; i < GetNumOfBreakablePart(); i++)
	{
		eHierarchyId nId = (eHierarchyId)(nStartId + i);
		int iBoneIndex = pParent->GetBoneIndex(nId);
		if(iBoneIndex > -1)
		{
			int nGroup = pFragInst->GetGroupFromBoneIndex(iBoneIndex);
			if(nGroup > -1)
			{
				fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetAllGroups()[nGroup];
				int iChild = pGroup->GetChildFragmentIndex();
				// Make sure no children in this group will break off
				for(int k = 0; k < pGroup->GetNumChildren(); k++)
				{
					pFragInst->SetDontBreakFlag(BIT(iChild + k));
					pBoundComp->SetIncludeFlags(iChild+k,ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
				}
			}
		}
	}


	if( pParent->GetModelIndex() == MI_PLANE_MOGUL ||
		pParent->GetModelIndex() == MI_PLANE_NOKOTA ||
		pParent->GetModelIndex() == MI_PLANE_ROGUE ||
		pParent->GetModelIndex() == MI_PLANE_MOLOTOK ||
		pParent->GetModelIndex() == MI_PLANE_PYRO ||
		pParent->GetModelIndex() == MI_PLANE_BOMBUSHKA || 
		pParent->GetModelIndex() == MI_PLANE_AVENGER ||
		pParent->GetModelIndex() == MI_PLANE_VOLATOL )
	{
		pParent->m_nVehicleFlags.bUseDeformation = false;
	}
}

void CAircraftDamageBase::BreakOffSection(CPlane* pParent, int nHitSection, bool bDisappear, bool bNetworkAllowed)
{
	if (pParent && pParent->IsNetworkClone() && !bNetworkAllowed)
		return;

	if (pParent && !pParent->CarPartsCanBreakOff())
	{
		bDisappear = true;
	}

	fragInstGta* pInst = pParent->GetVehicleFragInst();
	if(physicsVerifyf(pInst,"All vehicles need a frag inst"))
	{
		int nGroup = -1;
		fragTypeGroup* pGroup = NULL;
		eHierarchyId nId = GetHierarchyIdFromSection(nHitSection);
		bool canBreakOffLandingGear = pParent->GetModelIndex() != MI_PLANE_AVENGER;

		if((nId >= PLANE_FIRST_BREAKABLE_PART && nId < PLANE_FIRST_BREAKABLE_PART + PLANE_NUM_BREAKABLES)
			|| ( canBreakOffLandingGear && nId >= FIRST_LANDING_GEAR && nId < FIRST_LANDING_GEAR + NUM_LANDING_GEARS ))
		{
			// Lookup the frag group from the hierarchy id and see if it has broken off
			// First we need a bone index
			int iBoneIndex = pParent->GetBoneIndex(nId);

			if(iBoneIndex > -1)
			{
				nGroup = pParent->GetFragInst()->GetGroupFromBoneIndex(iBoneIndex);
				pGroup = pInst->GetTypePhysics()->GetAllGroups()[nGroup];
			}
		}

		if(nGroup != -1 && pGroup != NULL)
		{
			if(bDisappear)
			{
				pInst->DeleteAboveGroup(nGroup);
			}
			else
			{
				BreakOffAboveGroupRecursive(nGroup,pInst);

				int iChild = pGroup->GetChildFragmentIndex();

				// Stop all children in this group breaking
				// Don't use the clear all children function because this clears child groups as well
				// which we don't want to do
				// e.g. if wing can fall off don't want to allow engines to fall off if wing if their health is still OK
				for(int i = 0; i < pGroup->GetNumChildren(); i++)
				{
					pInst->ClearDontBreakFlag(BIT(iChild + i));					
				}
			}
			PostBreakOffSection(pParent, nHitSection, DAMAGE_TYPE_NONE);
		}
	}
}

float fBrokenOffWheelAngularDampingC = 0.35f;
float fBrokenOffWheelAngularDampingV = 0.25f;
float fBrokenOffWheelAngularDampingV2 = 0.35f;

void CAircraftDamageBase::BreakOffAboveGroupRecursive(int nGroup, fragInst* pFragInst)
{
	Assert(nGroup > -1 && nGroup < pFragInst->GetTypePhysics()->GetNumChildGroups());
	fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetAllGroups()[nGroup];
	// Break off all child groups as well
	for(int i = 0; i < pGroup->GetNumChildGroups(); i++)
	{
		int iChildGroup = pGroup->GetChildGroupsPointersIndex() + i;
		BreakOffAboveGroupRecursive(iChildGroup, pFragInst);
	}

	if (fragCacheEntry* entry = pFragInst->GetCacheEntry())
	{
		if(entry->GetHierInst() && entry->GetHierInst()->groupBroken->IsClear(nGroup))//make sure the part isn't already broken off
		{
			fragInst *pBrokenOffInst = pFragInst->BreakOffAboveGroup(nGroup);
			if(CEntity* pEntity = CPhysics::GetEntityFromInst(pBrokenOffInst))
			{
				if(pEntity->GetIsTypeObject() && ((CObject *)pEntity)->m_nObjectFlags.bCarWheel)
				{
					phArchetypeDamp* pGtaArchetype = (phArchetypeDamp*)pBrokenOffInst->GetArchetype();
					pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(fBrokenOffWheelAngularDampingC, fBrokenOffWheelAngularDampingC, fBrokenOffWheelAngularDampingC));
					pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(fBrokenOffWheelAngularDampingV, fBrokenOffWheelAngularDampingV, fBrokenOffWheelAngularDampingV));
					pGtaArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(fBrokenOffWheelAngularDampingV2, fBrokenOffWheelAngularDampingV2, fBrokenOffWheelAngularDampingV2));
				}
			}
		}
	}
}


bool CAircraftDamageBase::HasSectionBrokenOff(const CPlane* pParent, int iSection) const
{
	Assert(pParent);
	Assert(pParent->GetFragInst());
	Assert(pParent->GetFragInst()->GetType());

	eHierarchyId nId = GetHierarchyIdFromSection(iSection);
	if(nId >= GetFirstBreakablePart() && nId < GetFirstBreakablePart() + GetNumOfBreakablePart())
	{
		// Lookup the frag group from the hierarchy id and see if it has broken off
		// First we need a bone index
		int iBoneIndex = pParent->GetBoneIndex(nId);

		if(iBoneIndex > -1)
		{
			int iGroup = pParent->GetFragInst()->GetGroupFromBoneIndex(iBoneIndex);
			if(iGroup > -1)
			{
				Assert(pParent->GetFragInst()->GetCacheEntry());
				Assert(pParent->GetFragInst()->GetTypePhysics()->GetNumChildGroups() > iGroup);
				Assert(pParent->GetFragInst()->GetCacheEntry()->GetHierInst());
				return pParent->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(iGroup);
			}
		}
	}

	return false;
}

void CAircraftDamageBase::PostBreakOffSection(CPlane* UNUSED_PARAM(pParent), int UNUSED_PARAM(nSection), eDamageType UNUSED_PARAM(nDamageType))
{
}

void CAircraftDamageBase::ApplyDamageToChassis(CPlane* UNUSED_PARAM(pParent), float UNUSED_PARAM(fDamage), eDamageType UNUSED_PARAM(nDamageType), int UNUSED_PARAM(nComponent), const Vector3 &UNUSED_PARAM(vHitPos))
{
}

//////////////////////////////////////////////////////////////////////////
// Class CAircraftDamage 
//////////////////////////////////////////////////////////////////////////

#if __BANK
bool CAircraftDamage::ms_bDrawFlamePosition = false;
#endif

float CAircraftDamage::ms_fMinCausingFlameDamage = 250.0f;
float CAircraftDamage::ms_fMaxCausingFlameDamage = 1000.0f;
float CAircraftDamage::ms_fMinFlameLifeSpan = 3.0f;
float CAircraftDamage::ms_fMaxFlameLifeSpan = 4.0f;
float CAircraftDamage::ms_fFlameDamageMultiplier = 1.1f;
float CAircraftDamage::ms_fFlameMergeableDistance = 5.0f;

const CAircraftDamage::eAircraftSection CAircraftDamage::sm_iHierarchyIdToSection[PLANE_NUM_DAMAGE_PARTS] = 
{
	CAircraftDamage::RUDDER,			// 	PLANE_RUDDER,
    CAircraftDamage::RUDDER_2,			// 	PLANE_RUDDER,
	CAircraftDamage::ELEVATOR_L,		// 	PLANE_ELEVATOR_L,
	CAircraftDamage::ELEVATOR_R,		// 	PLANE_ELEVATOR_R,
	CAircraftDamage::AILERON_L,			// 	PLANE_AILERON_L,
	CAircraftDamage::AILERON_R,			// 	PLANE_AILERON_R,
	CAircraftDamage::AIRBRAKE_L,		//	PLANE_AIRBRAKE_L
	CAircraftDamage::AIRBRAKE_R,		//	PLANE_AIRBRAKE_R
	CAircraftDamage::WING_L,			// 	PLANE_WING_L,
	CAircraftDamage::WING_R,			// 	PLANE_WING_R,
	CAircraftDamage::TAIL,				// 	PLANE_TAIL,	
	CAircraftDamage::ENGINE_L,			// 	PLANE_ENGINE_L,
	CAircraftDamage::ENGINE_R,			// 	PLANE_ENGINE_R
};
CompileTimeAssert(PLANE_NUM_DAMAGE_PARTS == 13);

const float CAircraftDamage::sm_fInitialComponentHealth[CAircraftDamage::NUM_DAMAGE_SECTIONS] = 
{
	5000.0f, // WING_L,
	5000.0f, // WING_R,
	2500.0f, // TAIL,
	1000.0f, // ENGINE_L,
	1000.0f, // ENGINE_R,
	300.0f, // ELEVATOR_L,
	300.0f, // ELEVATOR_R,
	300.0f, // AILERON_L,
	300.0f, // AILERON_R,
	300.0f, // RUDDER,
    300.0f, // RUDDER_2,
	300.0f, // AIRBRAKE_L,
	300.0f  // AIRBRAKE_R,
};


const float CAircraftDamage::sm_fInstantBreakOffDamageThreshold[CAircraftDamage::NUM_DAMAGE_SECTIONS] = 
{
	200.0f,	// WING_L
	200.0f,	// WING_R
	100.0f,	// TAIL
	50.0f,	// ENGINE_L
	50.0f,	// ENGINE_R
	5.0f,	// ELEVATOR_L
	5.0f,	// ELEVATOR_R
	5.0f,	// AILERON_L
	5.0f,	// AILERON_R
	15.0f,	// RUDDER
	15.0f,	// RUDDER_2
	15.0f,	// AIRBRAKE_L,
	15.0f	// AIRBRAKE_R,
};

CompileTimeAssert(CAircraftDamage::NUM_DAMAGE_SECTIONS == 13);

#define AIRCRAFT_DAMAGE_FLAME_POOL_SIZE 34

FW_INSTANTIATE_CLASS_POOL(CAircraftDamage::DamageFlame, AIRCRAFT_DAMAGE_FLAME_POOL_SIZE, atHashString("AircraftFlames",0x998f00ac));

CAircraftDamage::CAircraftDamage()
: CAircraftDamageBase()
, m_bDestroyedByPed(false)
, m_bIgnoreBrokenOffPartForAIHandling(false)
, m_bControlPartsBreakOffInstantly(true)
, m_bHasCustomSectionDamageScale(false)
{
	for(int i = 0 ; i < NUM_DAMAGE_SECTIONS; i++)
	{
		m_fHealth[i]			= sm_fInitialComponentHealth[i];
		m_fHealthDamageScale[i] = 1.0f;
		m_fBrokenOffSmokeLifeTime[i] = 0.0f;
	}

	m_pDamageFlames.Reset();
	m_fEngineDegradeTimer = 0.0f;
}

CAircraftDamage::~CAircraftDamage()
{
	for(int index = 0; index < m_pDamageFlames.GetCount(); index++)
	{
		delete m_pDamageFlames[index];
	}
	m_pDamageFlames.Reset();
}

int CAircraftDamage::GetSectionFromHierarchyId(eHierarchyId iComponent)
{
	int i = ((int) iComponent) - PLANE_FIRST_DAMAGE_PART;
	
	if(i > -1 && i < PLANE_NUM_DAMAGE_PARTS)
	{
		return sm_iHierarchyIdToSection[i];
	}

	return INVALID_SECTION;
}

void  CAircraftDamage::CheckDestroyedByPed(CEntity* inflictor, CPlane* pParent, const int sectionBroken)
{
	if (!pParent)
		return;

	if (!inflictor)
		return;

	if (pParent->IsNetworkClone())
		return;

	if (pParent->GetStatus() == STATUS_WRECKED)
		return;

	switch (sectionBroken)
	{
	case ENGINE_L:
	case ENGINE_R:
	case WING_L:
	case WING_R:
	case TAIL:
		{
			if (inflictor->GetIsTypePed())
			{
				m_bDestroyedByPed = true;
			}
			else if (inflictor->GetIsTypeVehicle())
			{
				CVehicle* vehicle = static_cast<CVehicle* >(inflictor);

				if (vehicle->GetDriver())
				{
					m_bDestroyedByPed = true;
				}
			}
		}
		break;

	default:
		break;
	}
}

bool CAircraftDamage::BreakOffComponent(CEntity* inflictor, CPlane* pParent, int iComponent, bool bNetworkAllowed)
{
	int iSectionToBreakOff = GetSectionFromChildIndex(pParent, iComponent);
	if(iSectionToBreakOff != INVALID_SECTION)
	{
		SetSectionHealth(iSectionToBreakOff, Min(0.0f, GetSectionHealth(iSectionToBreakOff)));
		BreakOffSection(pParent, iSectionToBreakOff, bNetworkAllowed);

		//Check if the breakable component is critical to maintain the plane in the Aircraft flying
		CheckDestroyedByPed(inflictor, pParent, iSectionToBreakOff);

		return true;
	}
	return false;
}

int CAircraftDamage::GetSectionFromChildIndex(const CPlane* pParent, int iChildIndex)
{
	// Lookup bone index from child
	Assert(pParent);
	Assert(pParent->GetVehicleFragInst());
	Assert(pParent->GetVehicleFragInst()->GetTypePhysics()->GetNumChildren() > iChildIndex);

	int iBoneIndex = pParent->GetVehicleFragInst()->GetType()->GetBoneIndexFromID(
		pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllChildren()[iChildIndex]->GetBoneID());

	if(iBoneIndex > -1)
	{
		for(int i =0; i < PLANE_NUM_DAMAGE_PARTS; i++)
		{
			eHierarchyId nId = (eHierarchyId)(i + PLANE_FIRST_DAMAGE_PART);
			if(pParent->GetBoneIndex(nId) == iBoneIndex)
			{
				return GetSectionFromHierarchyId(nId);
			}
		}
	}

	return INVALID_SECTION;
}

// We store a map of hierarchyIds to damage sections
// so multiple hierarchy ids map to a single section
// This does the reverse, finding the first hierarchy id associated with a section
eHierarchyId CAircraftDamage::GetHierarchyIdFromSection(int iSection) const
{
	for(int i = 0; i < PLANE_NUM_DAMAGE_PARTS; i++)
	{
		if(sm_iHierarchyIdToSection[i] == iSection)
		{
			return (eHierarchyId)(i + PLANE_FIRST_DAMAGE_PART);
		}
	}
	return VEH_INVALID_ID;
}

float CAircraftDamage::GetSectionHealthAsFractionActual(eAircraftSection iSection) const
{
	Assert(iSection < NUM_DAMAGE_SECTIONS);
	Assert(iSection > -1);
	return GetSectionHealth(iSection) / sm_fInitialComponentHealth[iSection]; 
}

float CAircraftDamage::GetSectionHealthAsFraction(eAircraftSection iSection) const
{
	Assert(iSection < NUM_DAMAGE_SECTIONS);
	Assert(iSection > -1);
	return Max(GetSectionHealth(iSection), 0.0f) / sm_fInitialComponentHealth[iSection]; 
}

void CAircraftDamage::SetSectionHealthAsFraction(eAircraftSection iSection, float fHealth)
{
	Assert(iSection < NUM_DAMAGE_SECTIONS);
	Assert(iSection > -1);
	SetSectionHealth(iSection, fHealth * sm_fInitialComponentHealth[iSection]);
}

extern float sfEngineMinDistance;
float sfPlaneEngineBreakDownHealth = 200.0f; // Also referenced in CVehicleDamage::ProcessPetrolTankDamage
bank_float dfEngineDegradeStartingHealth = ENGINE_DAMAGE_PLANE_DAMAGE_START;
bank_float dfEngineDegradeMinDamage = 4.0f;
bank_float dfEngineDegradeMaxDamage = 8.0f;
float dfPlaneEngineMissFireStartingHealth = 600.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float bfPlaneEngineMissFireMinTime = 0.5f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float bfPlaneEngineMissFireMaxTime = 1.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float bfPlaneEngineMissFireMinRecoverTime = 10.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage
float bfPlaneEngineMissFireMaxRecoverTime = 20.0f; // Referenced in CVehicleDamage::ProcessPetrolTankDamage

void CAircraftDamage::ProcessPhysics(CPlane* pParent, float fTimeStep)
{
	// Process damage fire
	for(int i = 0; i < m_pDamageFlames.GetCount(); i++)
	{
		m_pDamageFlames[i]->fLifeSpan -= fTimeStep;
		if(m_pDamageFlames[i]->fLifeSpan<=0.0f)
		{
			delete m_pDamageFlames[i];
			m_pDamageFlames.Delete(i);
			i--;

#if __ASSERT
			for(int debugId = 1; debugId < m_pDamageFlames.size(); debugId++)
			{
				Assertf(m_pDamageFlames[debugId-1]->iEffectIndex < m_pDamageFlames[debugId]->iEffectIndex, 
					"CAircraftDamage::ProcessPhysics Effect IDs are not sorted properly, the id at slot %d is %d and at slot %d is %d",
					debugId-1, m_pDamageFlames[debugId-1]->iEffectIndex, debugId, m_pDamageFlames[debugId]->iEffectIndex);
				Assertf(debugId <= m_pDamageFlames [debugId]->iEffectIndex,
					"CAircraftDamage::ProcessPhysics Effect IDs are not sorted properly, the id at slot %d is %d",
					debugId, m_pDamageFlames[debugId]->iEffectIndex);
			}
#endif
		}
	}

	for(int i = 0 ; i < NUM_DAMAGE_SECTIONS; i++)
	{
		m_fBrokenOffSmokeLifeTime[i] -= fTimeStep;
		m_fBrokenOffSmokeLifeTime[i] = Max(m_fBrokenOffSmokeLifeTime[i], 0.0f);
	}

	if (pParent && pParent->IsNetworkClone())
		return;

	// Process engine degrade
	float planeDamageThreshold = pParent->m_Transmission.GetPlaneDamageThresholdOverride();
	if (planeDamageThreshold==0.0f)
	{
		planeDamageThreshold = dfEngineDegradeStartingHealth;
	}

	if(pParent->CanEngineDegrade() && pParent->m_nVehicleFlags.bEngineOn
		&& pParent->GetVehicleDamage()->GetEngineHealth() > sfPlaneEngineBreakDownHealth
		&& pParent->GetVehicleDamage()->GetEngineHealth() < planeDamageThreshold)
	{
		if(m_fEngineDegradeTimer > 1.0f)
		{
			float fEngineDegradeDamage = dfEngineDegradeMinDamage + (dfEngineDegradeMaxDamage - dfEngineDegradeMinDamage) 
				* ((pParent->GetVehicleDamage()->GetEngineHealth() - sfPlaneEngineBreakDownHealth) / (planeDamageThreshold - sfPlaneEngineBreakDownHealth));
			fEngineDegradeDamage = Clamp(fEngineDegradeDamage, dfEngineDegradeMinDamage, dfEngineDegradeMaxDamage);

			Vector3 vEnginePosLocal = VEC3_ZERO;
			if(pParent->GetBoneIndex(VEH_ENGINE) >= 0)
			{
				Matrix34 matEngineLocal = pParent->GetLocalMtx(pParent->GetBoneIndex(VEH_ENGINE));
				vEnginePosLocal = matEngineLocal.d;
				vEnginePosLocal.x = 0.0f;
			}
			
			if(GetSectionHealth(ENGINE_R) > 0.0f)
			{
				Vector3 vRightEnginePos = vEnginePosLocal + XAXIS * (sfEngineMinDistance - SMALL_FLOAT);
				float fRightEngineDamageMult = 1.0f - GetSectionHealthAsFraction(ENGINE_R);
				pParent->GetVehicleDamage()->ApplyDamageToEngine(pParent, DAMAGE_TYPE_COLLISION, fEngineDegradeDamage * fRightEngineDamageMult, vRightEnginePos, -ZAXIS, ZAXIS, false, true, 0.0f);
			}

			if(GetSectionHealth(ENGINE_L) > 0.0f)
			{
				Vector3 vLeftEnginePos = vEnginePosLocal - XAXIS * (sfEngineMinDistance - SMALL_FLOAT);
				float fLeftEngineDamageMult	= 1.0f - GetSectionHealthAsFraction(ENGINE_L);
				pParent->GetVehicleDamage()->ApplyDamageToEngine(pParent, DAMAGE_TYPE_COLLISION, fEngineDegradeDamage * fLeftEngineDamageMult, vLeftEnginePos, -ZAXIS, ZAXIS, false, true, 0.0f);
			}
			m_fEngineDegradeTimer = 0.0f;
		}

		m_fEngineDegradeTimer += fTimeStep;
	}

	if( (pParent->m_nVehicleFlags.bCanEngineMissFire || pParent->GetVehicleDamage()->GetEngineHealth() <= 0.0f )
		&& pParent->m_nVehicleFlags.bEngineOn
		&& pParent->GetVehicleDamage()->GetEngineHealth() < sfPlaneEngineBreakDownHealth
		&& !pParent->IsNetworkClone())
	{
		pParent->SwitchEngineOff();
		static_cast<audPlaneAudioEntity*>(pParent->m_VehicleAudioEntity)->TriggerBackFire(); 
	}
}

void CAircraftDamage::ProcessVfx(CPlane* pParent)
{
	for (int i=0; i<m_pDamageFlames.GetCount(); i++)
	{
		Assert(m_pDamageFlames[i]->fLifeSpan>0.0f);
		CAircraftDamage::DamageFlame* pDamageFlame = m_pDamageFlames[i];
		g_vfxVehicle.UpdatePtFxPlaneDamageFire(pParent, RCC_VEC3V(pDamageFlame->vLocalOffset), pDamageFlame->fLifeSpan, ms_fMaxFlameLifeSpan, pDamageFlame->iEffectIndex);
	}

	for (int i=0; i<NUM_DAMAGE_SECTIONS; i++)
	{
		if(m_fBrokenOffSmokeLifeTime[i] > 0.0f && HasSectionBrokenOff(pParent, i))
		{
			eHierarchyId nId = GetHierarchyIdFromSection(i);
			int nBoneIndex = pParent->GetBoneIndex(nId);
			if(nBoneIndex > -1)
			{
				const crBoneData* pParentBoneData = pParent->GetVehicleFragInst()->GetType()->GetSkeletonData().GetBoneData(nBoneIndex)->GetParent();
				int nParentIndex = pParent->GetVehicleFragInst()->GetComponentFromBoneIndex(pParentBoneData->GetIndex());
				if(nParentIndex > -1 && !pParent->GetVehicleFragInst()->GetChildBroken(nParentIndex))
				{
					Vector3 vPos;
					if(pParent->GetDefaultBonePositionForSetup(nId, vPos))
					{
						g_vfxVehicle.UpdatePtFxAircraftSectionDamageSmoke(pParent, RCC_VEC3V(vPos), i);
					}
				}
			}
		}
	}
}

float CAircraftDamage::ApplyDamageToEngine(CEntity* inflictor, CPlane *pParent, float fDamage, eDamageType nDamageType, const Vector3 &vecPosLocal, const Vector3 &vecNormLocal)
{
	if (pParent && pParent->IsNetworkClone())
		return 0.f;

	float fOriginalDamage = fDamage;

	eAircraftSection nEngineSection = ENGINE_L;
	eHierarchyId nLeftEngineId = GetHierarchyIdFromSection(ENGINE_L);
	int iLeftEngineBoneIndex = pParent->GetBoneIndex(nLeftEngineId);

	eHierarchyId nRightEngineId = GetHierarchyIdFromSection(ENGINE_R);
	int iRightEngineBoneIndex = pParent->GetBoneIndex(nRightEngineId);

	if(iLeftEngineBoneIndex >= 0 && iRightEngineBoneIndex >= 0)
	{
		Matrix34 matLeftEngineLocal = pParent->GetLocalMtx(iLeftEngineBoneIndex);
		Matrix34 matRightEngineLocal = pParent->GetLocalMtx(iRightEngineBoneIndex);

		if(matLeftEngineLocal.d.Dist2(vecPosLocal) < matRightEngineLocal.d.Dist2(vecPosLocal))
		{
			nEngineSection = ENGINE_L;
		}
		else
		{
			nEngineSection = ENGINE_R;
		}
	}
	else if(iLeftEngineBoneIndex >= 0)
	{
		nEngineSection = ENGINE_L;
	}
	else if(iRightEngineBoneIndex >= 0)
	{
		nEngineSection = ENGINE_R;
	}
	else
	{
		if(vecPosLocal.x < 0.0f)
		{
			nEngineSection = ENGINE_L;
		}
		else
		{
			nEngineSection = ENGINE_R;
		}
	}

	// Scale the damage by the engine health and engine section ratio
	float fDamageMult = (sm_fInitialComponentHealth[nEngineSection] * 2.0f) / ENGINE_HEALTH_MAX;
	fDamage *= fDamageMult;

	float fOriginalHealth = GetSectionHealth(nEngineSection);
	Vector3 vecPosGlobal, vecNormGlobal;
	const Matrix34& matParent = RCC_MATRIX34(pParent->GetMatrixRef());
	matParent.Transform(vecPosLocal, vecPosGlobal);
	matParent.Transform3x3(vecNormLocal, vecNormGlobal);
	ApplyDamageToSection(pParent, nEngineSection, 0, fDamage, nDamageType, vecPosGlobal, vecNormGlobal);

	// inflict unconsumed damage to the other engine
	if(nDamageType == DAMAGE_TYPE_EXPLOSIVE)
	{
		float fInflictedDamage = fDamage - Max(fOriginalHealth, 0.0f);
		if(fInflictedDamage > 0.0f)
		{
			eAircraftSection nOtherEngineSection = nEngineSection == ENGINE_L ? ENGINE_R : ENGINE_L;
			ApplyDamageToSection(pParent, nOtherEngineSection, 0, fInflictedDamage, nDamageType, vecPosGlobal, vecNormGlobal);
		}
	}

	// Try to cut out one engine before all engines cut out
	if(pParent->GetVehicleDamage()->GetEngineHealth() > sfPlaneEngineBreakDownHealth && GetSectionHealth(ENGINE_L) > 0.0f && GetSectionHealth(ENGINE_R) > 0.0f)
	{
		float fEngineBreakDownHealthAsFraction = sfPlaneEngineBreakDownHealth / ENGINE_HEALTH_MAX;
		if(GetSectionHealthAsFraction(ENGINE_L) < GetSectionHealthAsFraction(ENGINE_R) && GetSectionHealthAsFraction(ENGINE_L) < fEngineBreakDownHealthAsFraction)
		{
			SetSectionHealth(ENGINE_L, 0.0f);
		}
		else if(GetSectionHealthAsFraction(ENGINE_R) < GetSectionHealthAsFraction(ENGINE_L) && GetSectionHealthAsFraction(ENGINE_R) < fEngineBreakDownHealthAsFraction)
		{
			SetSectionHealth(ENGINE_R, 0.0f);
		}
	}

	//Check if any of the engines were destroyed.
	if (GetSectionHealth(ENGINE_L) <= 0.0f)
	{
		CheckDestroyedByPed(inflictor, pParent, ENGINE_L);
	}
	else if(GetSectionHealth(ENGINE_R) <= 0.0f)
	{
		CheckDestroyedByPed(inflictor, pParent, ENGINE_R);
	}

	// Find out the amount of damage applies to main engine
	if(pParent->GetVehicleDamage()->GetEngineHealth() > sfPlaneEngineBreakDownHealth)
	{
		float fExpectedMainEngineHealth;
		if(GetSectionHealth(ENGINE_L) > 0.0f && GetSectionHealth(ENGINE_R) > 0.0f)
		{
			fExpectedMainEngineHealth = (GetSectionHealthAsFraction(ENGINE_L) + GetSectionHealthAsFraction(ENGINE_R)) * 0.5f * ENGINE_HEALTH_MAX;
		}
		else if(fOriginalHealth > 0.0f && GetSectionHealth(nEngineSection) <= 0.0f)
		{
			fExpectedMainEngineHealth = (GetSectionHealthAsFraction(ENGINE_L) + GetSectionHealthAsFraction(ENGINE_R)) * 0.5f * ENGINE_HEALTH_MAX;
		}
		else
		{
			fExpectedMainEngineHealth = Max(GetSectionHealthAsFraction(ENGINE_L), GetSectionHealthAsFraction(ENGINE_R)) * ENGINE_HEALTH_MAX;
		}
		return Max(pParent->GetVehicleDamage()->GetEngineHealth() - fExpectedMainEngineHealth, 0.0f);
	}
	else
	{
		return fOriginalDamage;
	}
}

#if __BANK && DEBUG_DRAW
void CAircraftDamage::DebugDraw(CPlane* pParent)
{
	if(ms_bDrawFlamePosition)
	{
		for (int i=0; i<m_pDamageFlames.GetCount(); i++)
		{
			CAircraftDamage::DamageFlame* pDamageFlame = m_pDamageFlames[i];
			Assert(pDamageFlame->iSectionIndex < NUM_DAMAGE_SECTIONS);
			Assert(pDamageFlame->iSectionIndex >= -1); // VEH_INVALID_ID (-1) indicates for chassis
			Vector3 vPos;
			Matrix34 matParent;
			pParent->GetMatrixCopy(matParent); 
			matParent.Transform(pDamageFlame->vLocalOffset, vPos);
			float t = pDamageFlame->fLifeSpan/ms_fMaxFlameLifeSpan;
			Vector3 vGreen = VEC3V_TO_VECTOR3(Color_green.GetRGB());
			Vector3 vRed = VEC3V_TO_VECTOR3(Color_red.GetRGB());
			Color32 color (vGreen + (vRed - vGreen) * t);
			grcDebugDraw::Sphere(vPos, 0.1f, color, false);
		}
	}
}
#endif

void CAircraftDamage::UpdateBuoyancyOnPartBreakingOff(CPlane* pParent, int nComponentID)
{
	// Here we can scale the buoyancy of any breakable plane parts to alter how they float once broken off as a separate
	// inst. We couldn't do this in GenerateWaterSamplesForPlane() because we didn't have a mapping between bound
	// components and bones / hierarchy IDs at that point.
	CBuoyancyInfo* pBuoyancyInfo = pParent->GetBaseModelInfo()->GetBuoyancyInfo();
	Assert(pBuoyancyInfo);
	// Search for the water sample index associated with this part.
	int nSectionID = GetSectionFromChildIndex(pParent, nComponentID);
	for(int i = 0; i < pBuoyancyInfo->m_nNumWaterSamples; ++i)
	{
		CWaterSample& sample = pBuoyancyInfo->m_WaterSamples[i];
		if(nSectionID == CAircraftDamage::GetSectionFromChildIndex(pParent, sample.m_nComponent))
		{
			switch(nSectionID)
			{
			case WING_L:
			case WING_R:
			case TAIL:
			case ENGINE_L:
			case ENGINE_R:
			case ELEVATOR_L:
			case ELEVATOR_R:
			case AILERON_L:
			case AILERON_R:
			case RUDDER:
			case RUDDER_2:
			case AIRBRAKE_L:
			case AIRBRAKE_R:
				// All these parts get their buoyancy scaled right down so that they sink when they break off.
				sample.m_fBuoyancyMult = 0.0f;
				break;

			default:
				// Not all components have a damage section ID. They come through here.
				break;
			}
		}
	}
}

static dev_float sfDamageModifier = 0.2f; // this is how much the damage affects handling BEFORE a part is totally destroyed
static dev_float sfLiftDamageModifier = 0.1f;
static dev_float sfAileronDisabledMult = 0.7f;
// So you get handling modifier in range 1 -> (1-sfDamageModifier) before anything has fallen off
float CAircraftDamage::GetRollMult(CPlane* pParent) const
{
	bool bIgnoreBrokenOffParts = GetShouldIgnoreBrokenParts( pParent );

	float fLeftMult = bIgnoreBrokenOffParts ? 1.0f : GetSectionHealthAsFraction(AILERON_L);
	if(fLeftMult > 0.0f || !HasSectionBrokenOff(pParent,AILERON_L))
	{		
		fLeftMult = 1.0f - (sfDamageModifier*(1.0f - fLeftMult ));		
	}

	float fRightMult = bIgnoreBrokenOffParts ? 1.0f : GetSectionHealthAsFraction(AILERON_R);
	if(fRightMult > 0.0f || !HasSectionBrokenOff(pParent,AILERON_R))
	{
		fRightMult = 1.0f - (sfDamageModifier*(1.0f - fRightMult ));
	}

    //Ailerons can be disable from script, so change the handling due to that
    if(pParent->GetLeftAileronDisabled())
    {
        fLeftMult *= sfAileronDisabledMult;
    }

    if(pParent->GetRightAileronDisabled())
    {
        fRightMult *= sfAileronDisabledMult;
    }

	return Clamp((fLeftMult + fRightMult) / 2.0f, 0.0f, 1.0f);
}

float CAircraftDamage::GetPitchMult(CPlane* pParent) const
{
	bool bIgnoreBrokenOffParts = GetShouldIgnoreBrokenParts( pParent );

	if( !bIgnoreBrokenOffParts &&
		HasSectionBrokenOff(pParent, ELEVATOR_L) && HasSectionBrokenOff(pParent, ELEVATOR_R))
	{
		return 0.0f;
	}

	float fLeftMult = 1.0f;
	float fRightMult = 1.0f;
	
	if( !bIgnoreBrokenOffParts )
	{
		fLeftMult -= (sfDamageModifier*(1.0f - GetSectionHealthAsFraction(ELEVATOR_L)));
		fRightMult -= (sfDamageModifier*(1.0f - GetSectionHealthAsFraction(ELEVATOR_R)));
	}

	return Clamp((fLeftMult + fRightMult) / 2.0f, 0.0f, 1.0f);
}

float CAircraftDamage::GetYawMult(CPlane* pParent) const
{
	bool bIgnoreBrokenOffParts = GetShouldIgnoreBrokenParts( pParent );

	float fMult = bIgnoreBrokenOffParts ? 1.0f : GetSectionHealthAsFraction(RUDDER);
	if(fMult > 0.0f || !HasSectionBrokenOff(pParent, RUDDER))
	{
		fMult = 1.0f - (sfDamageModifier*(1.0f - fMult));
	}

	return Clamp(fMult, 0.0f, 1.0f);
}

float CAircraftDamage::GetLiftMult(CPlane* pParent) const
{
	bool bIgnoreBrokenOffParts = GetShouldIgnoreBrokenParts( pParent ); 

	if( !bIgnoreBrokenOffParts &&
		( HasSectionBrokenOff(pParent,WING_L) || GetHierarchyIdFromSection(WING_L) == VEH_INVALID_ID ) &&
		( HasSectionBrokenOff(pParent, WING_R) || GetHierarchyIdFromSection(WING_R) == VEH_INVALID_ID ) )
	{
		return 0.0f;
	}

	float fEngineMult = GetThrustMult(pParent);

	float fLeftMult = bIgnoreBrokenOffParts ? 1.0f : GetSectionHealthAsFraction(WING_L);
	if(fLeftMult > 0.0f || !HasSectionBrokenOff(pParent,WING_L))
	{
		fLeftMult = 1.0f - (sfLiftDamageModifier*(1.0f - fLeftMult));
	}

	float fRightMult = bIgnoreBrokenOffParts ? 1.0f : GetSectionHealthAsFraction(WING_R);
	if(fRightMult > 0.0f || !HasSectionBrokenOff(pParent, WING_R))
	{
		fRightMult = 1.0f - (sfLiftDamageModifier*(1.0f - fRightMult));
	}

	float fGlideMulti = 1.0f;
	if(fEngineMult == 0.0f && pParent->pHandling && pParent->pHandling->GetFlyingHandlingData())
	{
		fGlideMulti = pParent->pHandling->GetFlyingHandlingData()->m_fEngineOffGlideMulti;
	}

	return Clamp((fLeftMult + fRightMult + fEngineMult) * fGlideMulti / 3.0f, 0.0f, 1.0f);
}

dev_float dfEngineWarmUpMaxTime = 5.0f;

float CAircraftDamage::GetThrustMult(CPlane* pParent) const
{
	if(pParent->m_Transmission.GetCurrentlyMissFiring())
	{
		return 0.0f;
	}

	if(!pParent->IsEngineOn())
	{
		return 0.0f;
	}

	if(pParent->GetStatus() == STATUS_WRECKED || (HasSectionBrokenOff(pParent,CAircraftDamage::WING_L) && HasSectionBrokenOff(pParent, CAircraftDamage::WING_R)))
	{
		return 0.0f;
	}

	float fLeftEngineMult = GetSectionHealthAsFraction(ENGINE_L);
	if(fLeftEngineMult > 0.0f || !HasSectionBrokenOff(pParent,ENGINE_L))
	{
		fLeftEngineMult = 1.0f - (sfDamageModifier*(1.0f - fLeftEngineMult));
	}

	float fRightEngineMult = GetSectionHealthAsFraction(ENGINE_R);
	if(fRightEngineMult > 0.0f || !HasSectionBrokenOff(pParent, ENGINE_R))
	{
		fRightEngineMult = 1.0f - (sfDamageModifier*(1.0f - fRightEngineMult));
	}


	float fMainEngineMult = (pParent->GetVehicleDamage()->GetEngineHealth() - sfPlaneEngineBreakDownHealth) / (ENGINE_HEALTH_MAX - sfPlaneEngineBreakDownHealth);
	fMainEngineMult = Clamp(fMainEngineMult, 0.0f, 1.0f);
	fMainEngineMult = 1.0f - (sfDamageModifier*(1.0f - fMainEngineMult));
	float fEngineHealthMult = Min((fLeftEngineMult + fRightEngineMult) / 2.0f, fMainEngineMult);
	float fMarmUpMult = Clamp(pParent->GetWarmUpTime() / dfEngineWarmUpMaxTime, 0.0f, 1.0f);
	return Clamp(fEngineHealthMult * fMarmUpMult, 0.0f, 1.0f);
}

void CAircraftDamage::Fix()
{
	for(int i = 0 ; i < NUM_DAMAGE_SECTIONS; i++)
	{
		m_fHealth[i] = sm_fInitialComponentHealth[i];
		m_fBrokenOffSmokeLifeTime[i] = 0.0f;
	}

	// clear out all the damage fire
	for(int index = 0; index < m_pDamageFlames.GetCount(); index++)
	{
		delete m_pDamageFlames[index];
	}
	m_pDamageFlames.Reset();
	m_fEngineDegradeTimer = 0.0f;
}

void CAircraftDamage::BlowingUpVehicle(CPlane *UNUSED_PARAM(pParent))
{
	for(int i = 0 ; i < NUM_DAMAGE_SECTIONS; i++)
	{
		m_fBrokenOffSmokeLifeTime[i] = 0.0f;
	}

	// clear out all the damage fire
	for(int index = 0; index < m_pDamageFlames.GetCount(); index++)
	{
		delete m_pDamageFlames[index];
	}
	m_pDamageFlames.Reset();
}


void CAircraftDamage::ApplyDamageToSection(CPlane* pParent, int iSection, int iComponent, float fDamage, eDamageType nDamageType, const Vector3 &vHitPos, const Vector3 &vHitNorm)
{
	FastAssert(iSection < NUM_DAMAGE_SECTIONS);

	if(nDamageType == DAMAGE_TYPE_COLLISION)
	{
		ComputeFlameDamageAndCreatNewFire(pParent, iSection, iComponent, fDamage, vHitPos);
	}
	else if(nDamageType == DAMAGE_TYPE_EXPLOSIVE)
	{
		// If any controlling parts gets hit by explosion, they should break off instantly
		if( m_bControlPartsBreakOffInstantly &&
			( iSection == ELEVATOR_L || iSection == ELEVATOR_R 
			  || iSection == AILERON_L || iSection == AILERON_R
			  || iSection == RUDDER || iSection == RUDDER_2
			  || iSection == AIRBRAKE_L || iSection == AIRBRAKE_R) )
		{
			fDamage = Max(fDamage, m_fHealth[iSection]);
		}

		//if (iSection==WING_L || iSection==WING_R)
		{
			VfxCollInfo_s vfxCollInfo;
			vfxCollInfo.regdEnt = pParent;
			vfxCollInfo.vPositionWld = VECTOR3_TO_VEC3V(vHitPos);
			vfxCollInfo.vNormalWld = VECTOR3_TO_VEC3V(vHitNorm);
			vfxCollInfo.vDirectionWld = -VECTOR3_TO_VEC3V(vHitNorm);
			vfxCollInfo.materialId = PGTAMATERIALMGR->g_idCarMetal;
			vfxCollInfo.roomId = -1;
			vfxCollInfo.componentId = iComponent;
			vfxCollInfo.weaponGroup = WEAPON_EFFECT_GROUP_EXPLOSION;
			vfxCollInfo.force = 100.0f;
			vfxCollInfo.isBloody = false;
			vfxCollInfo.isWater = false;
			vfxCollInfo.isExitFx = false;
			vfxCollInfo.noPtFx = false;
			vfxCollInfo.noPedDamage = false;
			vfxCollInfo.noDecal = false;
			vfxCollInfo.isSnowball = false;
			vfxCollInfo.isFMJAmmo = false;

			// play any weapon impact effects
			g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, DAMAGE_TYPE_EXPLOSIVE, NULL);
		}
	}

	m_fHealth[iSection] -= fDamage * m_fHealthDamageScale[iSection];
	if(iSection == ENGINE_L || iSection == ENGINE_R)
	{
		m_fHealth[iSection] = rage::Max(ENGINE_DAMAGE_FINSHED,m_fHealth[iSection]);
	}
	else
	{
		m_fHealth[iSection] = rage::Max(0.0f,m_fHealth[iSection]);
	}
}

void CAircraftDamage::ApplyDamageToChassis(CPlane* pParent, float fDamage, eDamageType nDamageType, int nComponent, const Vector3 &vHitPos)
{
	if(nDamageType == DAMAGE_TYPE_COLLISION)
	{
		ComputeFlameDamageAndCreatNewFire(pParent, INVALID_SECTION, nComponent, fDamage, vHitPos);
	}
}

CAircraftDamage::DamageFlame *CAircraftDamage::NewFlame()
{
	if(m_pDamageFlames.IsFull())
	{
		Assertf(false, "CAircraftDamage::m_pDamageFlames is full, the collection size is %d", m_pDamageFlames.GetMaxCount());
		return NULL;

	}
	DamageFlame *pNewFlame = rage_new DamageFlame();
	if(pNewFlame == NULL)
	{
		Assertf(false, "CAircraftDamage::DamageFlame pool is full, the pool size is %d", DamageFlame::GetPool()->GetSize());
		return NULL;
	}
	
	// Find unique effect id
	int id = 0;
	for(int i = 0; i < m_pDamageFlames.size(); i++)
	{
		if(i == m_pDamageFlames[i]->iEffectIndex)
		{
			id++;
			continue;
		}
		break;
	}
	pNewFlame->iEffectIndex = id;
	DamageFlame *&prNewFlame = m_pDamageFlames.Insert(id);
	prNewFlame = pNewFlame;

#if __ASSERT
	for(int debugId = 1; debugId < m_pDamageFlames.size(); debugId++)
	{
		Assertf(m_pDamageFlames[debugId-1]->iEffectIndex < m_pDamageFlames[debugId]->iEffectIndex, 
			"CAircraftDamage::NewFlame Effect IDs are not sorted properly, the id at slot %d is %d and at slot %d is %d",
			debugId-1, m_pDamageFlames[debugId-1]->iEffectIndex, debugId, m_pDamageFlames[debugId]->iEffectIndex);
		Assertf(debugId <= m_pDamageFlames [debugId]->iEffectIndex,
			"CAircraftDamage::NewFlame Effect IDs are not sorted properly, the id at slot %d is %d",
			debugId, m_pDamageFlames[debugId]->iEffectIndex);
	}
#endif

	return pNewFlame;
}

CAircraftDamage::DamageFlame *CAircraftDamage::FindCloseFlame(int iSection, const Vector3 &vHitPosLocal)
{
	for(int index = 0; index < m_pDamageFlames.GetCount(); index++)
	{
		if(m_pDamageFlames[index]->iSectionIndex == iSection 
			&& m_pDamageFlames[index]->vLocalOffset.Dist2(vHitPosLocal) < (ms_fFlameMergeableDistance * ms_fFlameMergeableDistance))
		{
			return m_pDamageFlames[index];
		}
	}

	return NULL;
}

void CAircraftDamage::ComputeFlameDamageAndCreatNewFire(CPlane* pParent, int iSection, int nComponent, float &fDamage, const Vector3 &vHitPos)
{
	FastAssert(iSection < NUM_DAMAGE_SECTIONS);

	if(iSection != INVALID_SECTION && HasSectionBrokenOff(pParent, iSection))
	{
		return;
	}

	for (int index = 0; index < m_pDamageFlames.GetCount(); index++)
	{
		if(m_pDamageFlames[index]->iSectionIndex == iSection)
		{
			fDamage *= ms_fFlameDamageMultiplier;
		}
	}

	if(fDamage > ms_fMinCausingFlameDamage)
	{
		Vector3 vPositionLocal;
		const Matrix34& matParent = RCC_MATRIX34(pParent->GetMatrixRef());
		matParent.UnTransform(vHitPos, vPositionLocal);

		// Move the fire position towards to the center of hit component
		phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pParent->GetVehicleFragInst()->GetArchetype()->GetBound());
		Vector3 vComponentCenterLocal = VEC3V_TO_VECTOR3(pBoundComp->GetCurrentMatrix(nComponent).GetCol3());

		vPositionLocal += (vComponentCenterLocal - vPositionLocal) * 0.1f;

		if(DamageFlame *pCloseFlame = FindCloseFlame(iSection, vPositionLocal))
		{
			float t = (fDamage - ms_fMinCausingFlameDamage) / (ms_fMaxCausingFlameDamage - ms_fMinCausingFlameDamage);
			t = Min(t, 1.0f);
			float fNewLifeSpan = ms_fMinFlameLifeSpan + t * (ms_fMaxFlameLifeSpan - ms_fMinFlameLifeSpan);
			pCloseFlame->fLifeSpan = Max(pCloseFlame->fLifeSpan, fNewLifeSpan);
		}
		else if(DamageFlame *pNewFlame = NewFlame())
		{
			pNewFlame->vLocalOffset = vPositionLocal;
#if __ASSERT
			spdAABB tempBox;
			spdAABB boundBox = pParent->GetLocalSpaceBoundBox(tempBox);
			boundBox.GrowUniform(ScalarV(0.1f));
			Assertf(boundBox.ContainsPoint(VECTOR3_TO_VEC3V(pNewFlame->vLocalOffset)), 
				"The damage flame offset is not within the bounding box, model %s, box [%3.2f, %3.2f, %3.2f][%3.2f, %3.2f, %3.2f], hitPos[%3.2f, %3.2f, %3.2f], hitPosLocal[%3.2f, %3.2f, %3.2f], section %d, comp %d, damage %3.2f",
				pParent->GetModelName(), 
				boundBox.GetMin().GetXf(), boundBox.GetMin().GetYf(), boundBox.GetMin().GetZf(),
				boundBox.GetMax().GetXf(), boundBox.GetMax().GetYf(), boundBox.GetMax().GetZf(),
				vHitPos.x, vHitPos.y, vHitPos.z,
				vPositionLocal.x, vPositionLocal.y, vPositionLocal.z,
				iSection, nComponent, fDamage);
#endif
			pNewFlame->iSectionIndex = iSection;
			float t = (fDamage - ms_fMinCausingFlameDamage) / (ms_fMaxCausingFlameDamage - ms_fMinCausingFlameDamage);
			t = Min(t, 1.0f);
			pNewFlame->fLifeSpan = ms_fMinFlameLifeSpan + t * (ms_fMaxFlameLifeSpan - ms_fMinFlameLifeSpan);
		}
	}
}

dev_float dfPlaneSectionBrokenOffSmokeMaxLifeTime = 3.0f;
dev_float dfPlaneSectionBrokenOffSmokeMinLifeTime = 1.5f;
dev_float dfPlaneSectionBrokenOffEngineHealth = 500.0f;

void CAircraftDamage::PostBreakOffSection(CPlane* pParent, int nHitSection, eDamageType nDamageType)
{
	FastAssert(nHitSection < NUM_DAMAGE_SECTIONS);
	for(int i = 0; i < m_pDamageFlames.GetCount(); i++)
	{
		DamageFlame *pFlame = m_pDamageFlames[i];
		if(pFlame->iSectionIndex == nHitSection)
		{
			delete pFlame;
			m_pDamageFlames.Delete(i);
			i--;

#if __ASSERT
			for(int debugId = 1; debugId < m_pDamageFlames.size(); debugId++)
			{
				Assertf(m_pDamageFlames[debugId-1]->iEffectIndex < m_pDamageFlames[debugId]->iEffectIndex, 
					"CAircraftDamage::ProcessPhysics Effect IDs are not sorted properly, the id at slot %d is %d and at slot %d is %d",
					debugId-1, m_pDamageFlames[debugId-1]->iEffectIndex, debugId, m_pDamageFlames[debugId]->iEffectIndex);
				Assertf(debugId <= m_pDamageFlames [debugId]->iEffectIndex,
					"CAircraftDamage::ProcessPhysics Effect IDs are not sorted properly, the id at slot %d is %d",
					debugId, m_pDamageFlames[debugId]->iEffectIndex);
			}
#endif
		}
	}

	if(nDamageType != DAMAGE_TYPE_WATER_CANNON)
	{
		m_fBrokenOffSmokeLifeTime[nHitSection] = fwRandom::GetRandomNumberInRange(dfPlaneSectionBrokenOffSmokeMinLifeTime, dfPlaneSectionBrokenOffSmokeMaxLifeTime);
	}

	switch(nHitSection)
	{
		case WING_L:
		{
			if(HasSectionBrokenOff(pParent, AILERON_L))
			{
				SetSectionHealth(AILERON_L, Min(0.0f, GetSectionHealth(AILERON_L)));
				PostBreakOffSection(pParent, AILERON_L, nDamageType);
			}

			if(HasSectionBrokenOff(pParent, AIRBRAKE_L))
			{
				SetSectionHealth(AIRBRAKE_L, Min(0.0f, GetSectionHealth(AIRBRAKE_L)));
				PostBreakOffSection(pParent, AIRBRAKE_L, nDamageType);
			}

			if( MI_PLANE_TULA.IsValid() &&
				pParent->GetModelIndex() == MI_PLANE_TULA )
			{
				pParent->BreakOffRotor(0);
				pParent->BreakOffRotor(1);
				pParent->BreakOffRotor(2);
				pParent->BreakOffRotor(3);
			}
			break;
		}
		case WING_R:
		{
			if(HasSectionBrokenOff(pParent, AILERON_R))
			{
				SetSectionHealth(AILERON_R, Min(0.0f, GetSectionHealth(AILERON_R)));
				PostBreakOffSection(pParent, AILERON_R, nDamageType);
			}

			if(HasSectionBrokenOff(pParent, AIRBRAKE_R))
			{
				SetSectionHealth(AIRBRAKE_R, Min(0.0f, GetSectionHealth(AIRBRAKE_R)));
				PostBreakOffSection(pParent, AIRBRAKE_R, nDamageType);
			}
			break;
		}
		case TAIL:
		{
			if(HasSectionBrokenOff(pParent, ELEVATOR_L))
			{
				SetSectionHealth(ELEVATOR_L, Min(0.0f, GetSectionHealth(ELEVATOR_L)));
				PostBreakOffSection(pParent, ELEVATOR_L, nDamageType);
			}

			if(HasSectionBrokenOff(pParent, ELEVATOR_R))
			{
				SetSectionHealth(ELEVATOR_R, Min(0.0f, GetSectionHealth(ELEVATOR_R)));
				PostBreakOffSection(pParent, ELEVATOR_R, nDamageType);
			}

			if(HasSectionBrokenOff(pParent, RUDDER))
			{
				SetSectionHealth(RUDDER, Min(0.0f, GetSectionHealth(RUDDER)));
				PostBreakOffSection(pParent, RUDDER, nDamageType);
			}

			if(HasSectionBrokenOff(pParent, RUDDER_2))
			{
				SetSectionHealth(RUDDER_2, Min(0.0f, GetSectionHealth(RUDDER_2)));
				PostBreakOffSection(pParent, RUDDER_2, nDamageType);
			}
			break;
		}
		case ELEVATOR_L:
		case AILERON_L:
		{
			float fDamageToEngine = Max(GetSectionHealth(ENGINE_L) - dfPlaneSectionBrokenOffEngineHealth, 0.0f);
			if(fDamageToEngine > 0.0f && nDamageType != DAMAGE_TYPE_EXPLOSIVE)
			{
				Matrix34 matBreakOffSectionLocal = pParent->GetLocalMtx(pParent->GetBoneIndex(GetHierarchyIdFromSection(nHitSection)));
				float fDamageMult = ENGINE_HEALTH_MAX / (sm_fInitialComponentHealth[ENGINE_L] * 2.0f);
				pParent->GetVehicleDamage()->ApplyDamageToEngine(pParent, DAMAGE_TYPE_COLLISION, fDamageToEngine * fDamageMult, matBreakOffSectionLocal.d, -ZAXIS, ZAXIS, false, true, 0.0f);
				SetSectionHealth(ENGINE_L, Min(GetSectionHealth(ENGINE_L), dfPlaneSectionBrokenOffEngineHealth));
			}
			break;
		}
		case ELEVATOR_R:
		case AILERON_R:
		{
			float fDamageToEngine = Max(GetSectionHealth(ENGINE_R) - dfPlaneSectionBrokenOffEngineHealth, 0.0f);
			if(fDamageToEngine > 0.0f && nDamageType != DAMAGE_TYPE_EXPLOSIVE)
			{
				Matrix34 matBreakOffSectionLocal = pParent->GetLocalMtx(pParent->GetBoneIndex(GetHierarchyIdFromSection(nHitSection)));
				float fDamageMult = ENGINE_HEALTH_MAX / (sm_fInitialComponentHealth[ENGINE_R] * 2.0f);
				pParent->GetVehicleDamage()->ApplyDamageToEngine(pParent, DAMAGE_TYPE_COLLISION, fDamageToEngine * fDamageMult, matBreakOffSectionLocal.d, -ZAXIS, ZAXIS, false, true, 0.0f);
				SetSectionHealth(ENGINE_R, Min(GetSectionHealth(ENGINE_R), dfPlaneSectionBrokenOffEngineHealth));
			}
			break;
		}
	}

	if(pParent)
	{
		static_cast<audVehicleAudioEntity*>(pParent->GetAudioEntity())->BreakOffSection(nHitSection);
	}
}

void CAircraftDamage::FindAllHitSections(int &nNumHitSections, int *pHitSectionIndices, const CPlane *pParent, eDamageType nDamageType, const Vector3& vecPos, int nComponent, float fDamageRadius)
{
	nNumHitSections = 0;
	if(nDamageType == DAMAGE_TYPE_EXPLOSIVE && fDamageRadius > 0.0f)
	{
		// Make at least half of the plane getting affected by explosion
		fDamageRadius = Max(fDamageRadius, vecPos.Dist(VEC3V_TO_VECTOR3(pParent->GetMatrix().d())));
		float fShortestDistanceToExplosionSquared = LARGE_FLOAT;
		int nSmallComponentIndex = -1;
		for(int iSectionIdx = 0; iSectionIdx < NUM_DAMAGE_SECTIONS; iSectionIdx++)
		{
			eHierarchyId iHierId = GetHierarchyIdFromSection(iSectionIdx);
			int iBoneIdx = pParent->GetBoneIndex(iHierId);
			if(iBoneIdx >= 0)
			{
				Matrix34 matSection;
				pParent->GetGlobalMtx(iBoneIdx, matSection);
				float fDistanceToExplosionSquared = matSection.d.Dist2(vecPos);
				if(fDistanceToExplosionSquared < (fDamageRadius * fDamageRadius))
				{
					if(NetworkInterface::IsGameInProgress() && !pParent->m_nVehicleFlags.bPlaneResistToExplosion)
					{
						pHitSectionIndices[nNumHitSections] = iSectionIdx;
						nNumHitSections++;
					}
					else
					{
						switch(iSectionIdx)
						{
							// apply the damage to large component all the time
							case WING_L:
							case WING_R:
							case TAIL:
							case ENGINE_L:
							case ENGINE_R:
							// break all air brakes if hit
							case AIRBRAKE_L:
							case AIRBRAKE_R:
							{
								pHitSectionIndices[nNumHitSections] = iSectionIdx;
								nNumHitSections++;
								break;
							}
							// Only blow off one small component at a time
							default:
							{
								if(fDistanceToExplosionSquared < fShortestDistanceToExplosionSquared)
								{
									if(nSmallComponentIndex < 0)
									{
										nSmallComponentIndex = nNumHitSections;
										nNumHitSections++;
									}
									pHitSectionIndices[nSmallComponentIndex] = iSectionIdx;
									fShortestDistanceToExplosionSquared = fDistanceToExplosionSquared;
								}
								break;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		pHitSectionIndices[0] = GetSectionFromChildIndex(pParent,nComponent);
		nNumHitSections = 1;
	}
}

bool CAircraftDamage::GetShouldIgnoreBrokenParts( CPlane* pParent ) const
{ 
	return  ( m_bIgnoreBrokenOffPartForAIHandling &&
	  	      pParent->GetDriver() &&
			  !pParent->IsDriverAPlayer() ); 
}

#if __BANK
const char* strSectionNames[] = 
{
	"WING_L",
	"WING_R",
	"TAIL",
	"ENGINE_L",
	"ENGINE_R",
	"ELEVATOR_L",
	"ELEVATOR_R",
	"AILERON_L",
	"AILERON_R",
	"RUDDER",
    "RUDDER_2",
	"AIRBRAKE_L",
	"AIRBRAKE_R",
	"LANDING_GEAR"
};
void CAircraftDamage::DisplayDebugOutput(CPlane* UNUSED_PARAM(pParent)) const
{
	char vehDebugString[512];

	static dev_s32 StartX = 20;
	static dev_s32 StartY = 10;

	int y = StartY;

	for(int i =0; i < NUM_DAMAGE_SECTIONS; i++)
	{
		formatf(vehDebugString,512,"%s %.2f\n",strSectionNames[i],GetSectionHealth((eAircraftSection)i));
		grcDebugDraw::PrintToScreenCoors(vehDebugString,StartX,y);
		y++;
	}
}
#endif


//////////////////////////////////////////////////////////////////////////
// Class CLandingGearDamage
//////////////////////////////////////////////////////////////////////////

const float CLandingGearDamage::sm_fInitialComponentHealth[CLandingGearDamage::NUM_DAMAGE_SECTIONS] = 
{
	2600.0f, //	GEAR_F,
	1300.0f, // GEAR_RL,
	1300.0f, // GEAR_LM1,
	1300.0f, // GEAR_RM,
	1300.0f, // GEAR_RM1,
	1300.0f // GEAR_RR,
};


const CLandingGearDamage::eLandingGearSection CLandingGearDamage::sm_iHierarchyIdToSection[NUM_LANDING_GEARS] = 
{
	CLandingGearDamage::GEAR_F,
	CLandingGearDamage::GEAR_RL,
	CLandingGearDamage::GEAR_LM1,
	CLandingGearDamage::GEAR_RM,
	CLandingGearDamage::GEAR_RM1,
	CLandingGearDamage::GEAR_RR
};

CompileTimeAssert(NUM_LANDING_GEARS == 6);

const float CLandingGearDamage::sm_fInstantBreakOffDamageThreshold[NUM_DAMAGE_SECTIONS] =
{
    2400.0f, // GEAR_F,
	900.0f, // GEAR_RL,
	900.0f, // GEAR_LM1,
	900.0f,  // GEAR_RM,
	900.0f,  // GEAR_RM1,
	900.0f  // GEAR_RR, 
};

CLandingGearDamage::CLandingGearDamage()
{
	for(int i = 0 ; i < NUM_DAMAGE_SECTIONS; i++)
	{
		m_fHealth[i] = sm_fInitialComponentHealth[i];
		m_fHealthDamageScale[i] = 1.0f;
	}
	m_bHasCustomLandingGearSectionDamageScale = false;
}

// Query damage on sections
float CLandingGearDamage::GetSectionHealthAsFraction(eLandingGearSection iSection) const
{
	Assert(iSection < NUM_DAMAGE_SECTIONS);
	Assert(iSection > -1);
	return Max(GetSectionHealth(iSection), 0.0f) / sm_fInitialComponentHealth[iSection]; 
}

bool CLandingGearDamage::HasSectionBroken(const CPlane* UNUSED_PARAM(pParent), eLandingGearSection UNUSED_PARAM(iSection)) const
{
	// TODO: Check if only the joint is broken
	return false;
}

// Specifically damage a section
void CLandingGearDamage::SetSectionHealthAsFraction(eLandingGearSection iSection, float fHealth)
{
	Assert(iSection < NUM_DAMAGE_SECTIONS);
	Assert(iSection > -1);
	SetSectionHealth(iSection, fHealth * sm_fInitialComponentHealth[iSection]);
}

dev_float dfLandingGearOffMissionDamageMulti = 2.0f;
void CLandingGearDamage::ApplyDamageToSection(CPlane* pParent, int iSection, int UNUSED_PARAM(iComponent), float fDamage, eDamageType UNUSED_PARAM(nDamageType), const Vector3 &UNUSED_PARAM(vHitPos), const Vector3 &UNUSED_PARAM(vHitNorm))
{
	if(pParent->m_nVehicleFlags.bUnbreakableLandingGear)
	{
		return;
	}
	FastAssert(iSection < NUM_DAMAGE_SECTIONS);
	if(!CTheScripts::GetPlayerIsOnAMission())
	{
		fDamage *= dfLandingGearOffMissionDamageMulti;
	}
	m_fHealth[iSection] -= fDamage * m_fHealthDamageScale[iSection];
	m_fHealth[iSection] = rage::Max(0.0f,m_fHealth[iSection]);
}

void CLandingGearDamage::BreakSection(CPlane* UNUSED_PARAM(pParent), eLandingGearSection UNUSED_PARAM(nHitSection))
{
	// TODO: Only break the joint and let landing gear swing freely
}

void CLandingGearDamage::Fix()
{
	for(int i = 0 ; i < NUM_DAMAGE_SECTIONS; i++)
	{
		m_fHealth[i] = sm_fInitialComponentHealth[i];
	}
}

void CLandingGearDamage::BreakAll(CPlane* pParent)
{
	for(int i=0; i<pParent->GetNumWheels(); i++)
	{
		CWheel *pWheel = pParent->GetWheel(i);
		if(pWheel)
		{
			BreakOffComponent(NULL, pParent, pWheel->GetFragChild(), true);
		}
	}
}

bool CLandingGearDamage::BreakOffComponent(CEntity* UNUSED_PARAM(inflictor), CPlane* pParent, int iComponent, bool bNetworkAllowed)
{
	if( pParent->GetModelIndex() != MI_PLANE_MICROLIGHT )
	{
		int iSectionToBreakOff = GetSectionFromChildIndex(pParent, iComponent);
		if(iSectionToBreakOff != INVALID_SECTION)
		{
			SetSectionHealth(iSectionToBreakOff, Min(0.0f, GetSectionHealth(iSectionToBreakOff)));
			BreakOffSection(pParent, iSectionToBreakOff, false, bNetworkAllowed);
			return true;
		}
	}
	return false;
}


void CLandingGearDamage::PostBreakOffSection(CPlane* UNUSED_PARAM(pParent), int nSection, eDamageType UNUSED_PARAM(nDamageType))
{
	// Request by B* 1060213, break all landing gear on next impact when the front landing gear breaks off
	if(nSection == GEAR_F)
	{
		for(int i = 0 ; i < NUM_DAMAGE_SECTIONS; i++)
		{
			m_fHealth[i] = rage::Min(m_fHealth[i], 0.0f);
		}
	}
}

void CLandingGearDamage::ProcessPhysics(CPlane* pParent)
{
	// Request by B* 1060213, break all landing gear on next impact when the front landing gear breaks off
	if(HasSectionBrokenOff(pParent, GEAR_F))
	{
		pParent->SetBrake(1.0f);
		for(int i=0; i<pParent->GetNumWheels(); i++)
		{
			CWheel *pWheel = pParent->GetWheel(i);
			if(pWheel && pWheel->GetIsTouching())
			{
				BreakOffComponent(NULL, pParent, pWheel->GetFragChild());
			}
		}
	}
}

bool CLandingGearDamage::IsComponentBreakable(const CPlane *pParent, int iComponent)
{
	return GetSectionFromChildIndex(pParent, iComponent) != INVALID_SECTION;
}

// Child index is 1:1 match to composite bound component
int CLandingGearDamage::GetSectionFromChildIndex(const CPlane* pParent, int iChildIndex)
{
	// Lookup bone index from child
	Assert(pParent);
	Assert(pParent->GetVehicleFragInst());
	Assert(pParent->GetVehicleFragInst()->GetTypePhysics()->GetNumChildren() > iChildIndex);

	int iBoneIndex = pParent->GetVehicleFragInst()->GetType()->GetBoneIndexFromID(
		pParent->GetVehicleFragInst()->GetTypePhysics()->GetAllChildren()[iChildIndex]->GetBoneID());

	if(iBoneIndex > -1)
	{
		for(int i =0; i < NUM_LANDING_GEARS; i++)
		{
			eHierarchyId nId = (eHierarchyId)(i + FIRST_LANDING_GEAR);
			if(pParent->GetBoneIndex(nId) == iBoneIndex)
			{
				return GetSectionFromHierarchyId(nId);
			}
		}
	}

	// The impact might be applied on wheels, which should be child of the landing gear. Check parent bone as well.
	const crBoneData* pParentBoneData = pParent->GetVehicleFragInst()->GetType()->GetSkeletonData().GetBoneData(iBoneIndex)->GetParent();
	if(pParentBoneData)
	{
		for(int iSectionIdx = 0; iSectionIdx < NUM_DAMAGE_SECTIONS; iSectionIdx++)
		{
			eHierarchyId iHierId = GetHierarchyIdFromSection(iSectionIdx);
			int iSectionBoneIdx = pParent->GetBoneIndex(iHierId);
			if(iSectionBoneIdx == pParentBoneData->GetIndex())
			{
				return iSectionIdx;
				break;
			}
		}
	}

	return INVALID_SECTION;
}

int CLandingGearDamage::GetSectionFromHierarchyId(eHierarchyId iComponent)
{
	int i = ((int) iComponent) - FIRST_LANDING_GEAR;

	if(i > -1 && i < NUM_LANDING_GEARS)
	{
		return sm_iHierarchyIdToSection [i];
	}

	return INVALID_SECTION;
}

// This will give the first hierarchy id associated with a damage section
// In theory multiple components could be hooked up to one damage section so be warned
eHierarchyId CLandingGearDamage::GetHierarchyIdFromSection(int nLandingGearSection) const
{
	for(int i = 0; i < NUM_LANDING_GEARS; i++)
	{
		if(sm_iHierarchyIdToSection[i] == nLandingGearSection)
		{
			return (eHierarchyId)(i + FIRST_LANDING_GEAR);
		}
	}
	return VEH_INVALID_ID;
}

void CLandingGearDamage::FindAllHitSections(int &nNumHitSections, int *pHitSectionIndices, const CPlane *pParent, eDamageType nDamageType, const Vector3& vecPos, int nComponent, float fDamageRadius)
{
	nNumHitSections = 0;

	if(nDamageType == DAMAGE_TYPE_EXPLOSIVE && fDamageRadius > 0.0f)
	{
		// Don't apply any explosion damage to landing gears if they are locked up
		if(pParent->GetLandingGear().GetPublicState() == CLandingGear::STATE_LOCKED_UP)
		{
			return;
		}
		// Make at least half of the plane getting affected by explosion
		fDamageRadius = Max(fDamageRadius, vecPos.Dist(VEC3V_TO_VECTOR3(pParent->GetMatrix().d())));

		for(int iSectionIdx = 0; iSectionIdx < NUM_DAMAGE_SECTIONS; iSectionIdx++)
		{
			eHierarchyId iHierId = GetHierarchyIdFromSection(iSectionIdx);
			int iBoneIdx = pParent->GetBoneIndex(iHierId);
			if(iBoneIdx >= 0)
			{
				Matrix34 matSection;
				pParent->GetGlobalMtx(iBoneIdx, matSection);
				if(matSection.d.Dist2(vecPos) < (fDamageRadius * fDamageRadius))
				{
					pHitSectionIndices[nNumHitSections] = iSectionIdx;
					nNumHitSections++;
				}
			}
		}
	}
	else
	{
		pHitSectionIndices[0] = GetSectionFromChildIndex(pParent,nComponent);
		nNumHitSections = 1;
	}
}


//////////////////////////////////////////////////////////////////////////
// Class CPlane
//
CPlane::CPlane(const eEntityOwnedBy ownedBy, u32 popType) : CAutomobile(ownedBy, popType, VEHICLE_TYPE_PLANE)
{
	m_fYawControl = 0.0f;
	m_fPitchControl = 0.0f;
	m_fRollControl = 0.0f;
	m_fThrottleControl = 0.0f;
	m_fAirBrakeControl = 0.0f;

	m_fScriptThrottleControl = 1.0f;
	m_fVirtualSpeedControl = 1.0f;
	m_fScriptTurbulenceMult = 1.0f;
	m_fScriptPilotSkillNoiseScalar = 1.0f;

	m_fYaw = 0.0f;
	m_fRoll = 0.0f;
	m_fPitch = 0.0f;
	m_fBrake = 0.0f;

	m_fVisualRoll = 0.0f;
	m_fVisualPitch = 0.0f;
	m_fCurrentFlapAngle = 0.0f;

	m_fJoystickPitch = 0.0f;
	m_fJoystickRoll = 0.0f;

	m_fEngineSpeed = 0.0f;

	m_TakeOffDirection = 0.0f;
	m_FlightDirection = 0.0f;
	m_FlightDirectionAvoidingTerrain = 0.0f;
	m_LowestFlightHeight = 15.0f;
	m_DesiredHeight = 25.0f;
	m_MinHeightAboveTerrain = 20.0f;
	m_OldTilt = 0.0f;
	m_OnGroundTimer = 0;
	m_fPreviousRoll = 0.0f;
	m_OldOrientation = 0.0f;

	m_bVtolRollVariablesInitialised = false;
	m_VtolRollIntegral = 0;
	m_VtolRollPreviousError = 0;

	m_nPhysicalFlags.bFlyer = true;

		
	m_ExtendedRemovalRange = (s16)(2.5f * CVehiclePopulation::GetCullRange(1.0f, 1.0f));		// planes get removed further from the camera
	m_nVehicleFlags.bNeverUseSmallerRemovalRange = true;
	m_nVehicleFlags.bCanMakeIntoDummyVehicle = true;

	m_nVehicleFlags.bIsBig=true;

	m_iNumPropellers = 0;

    m_bDisableRightAileron = false;
    m_bDisableLeftAileron = false;
	m_bIsPropellerDamagedThisFrame = false;
	m_bIsEngineTurnedOffByPlayer = false;
	m_bIsEngineTurnedOffByScript = false;
	m_bEnableThrustFallOffThisFrame = true;
	m_bDisableAutomaticCrashTask = false;
	m_bBreakOffPropellerOnContact = false;
	m_bDisableVerticalFlightModeTransition = false;
	m_bDisableExplodeFromBodyDamageOnCollision = false;
	m_bDisableExplodeFromBodyDamage = false;

	m_fTurbulenceMagnitude = 0.0f;
	m_fTurbulenceNoise = 0.0f;
	m_fTurbulenceTimeSinceLastCycle = 0.0f;
	m_fTurbulenceRecoverTimer = 0.0f;
	m_vTurbulenceAirflow = VEC3_ZERO;


	m_fTurnEngineOffTimer = 0.0f;
	m_fEngineWarmUpTimer = 0.0f;
	m_fLeftLightRotation = 0.0f;
	m_fRightLightRotation = 0.0f;

    m_fCurrentVerticalFlightRatio = 1.0f;
    m_fDesiredVerticalFlightRatio = 1.0f;

	m_fRollInputBias	= 0.0f;
	m_fPitchInputBias	= 0.0f;

	m_nVehicleFlags.bExplodesOnHighExplosionDamage = false;
	m_nVehicleFlags.bInteriorLightOn = true;
	m_IndividualPropellerFlags = m_IndividualPropellerFlags | 0xff;

	m_windowBoneCached = false;
    m_allowRollAndYawWhenCrashing = true;
	m_dipStraightDownWhenCrashing = false;

	for(int i = 0; i < 16; i++)
	{
		m_windowBoneIndices[i] = -1;
	}

	//m_prevTorque = Vector3( 0.0f, 0.0f, 0.0f );

	CFlyingVehicleAvoidanceManager::AddVehicle(RegdVeh(this));
}

//
//
//
CPlane::~CPlane()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	CFlyingVehicleAvoidanceManager::RemoveVehicle(RegdVeh(this));
}

CPlaneIntelligence *CPlane::GetPlaneIntelligence() const
{
	Assert(dynamic_cast<CPlaneIntelligence*>(m_pIntelligence));

	return static_cast<CPlaneIntelligence*>(m_pIntelligence);
}

static bool sbEnablePropellerMods = false;

void CPlane::SetModelId(fwModelId modelId)
{
	CAutomobile::SetModelId(modelId);
	eRotationAxis rotationAxis = !GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE ) ? ROT_AXIS_LOCAL_Y : ROT_AXIS_LOCAL_Z;

	// Figure out which propellers are valid, and set them up
	for(int nPropIndex = 0 ; nPropIndex < PLANE_NUM_PROPELLERS; nPropIndex++ )
	{
		eHierarchyId nId = (eHierarchyId)(PLANE_PROP_1 + nPropIndex);
		if(GetBoneIndex(nId) > -1 && physicsVerifyf(m_iNumPropellers < PLANE_NUM_PROPELLERS,"Out of room for plane propellers"))
		{
			// Found a valid propeller
			m_propellers[m_iNumPropellers].Init(nId, rotationAxis, this);
			m_propellerCollision[m_iNumPropellers].Init(nId,this);
            m_fPropellerHealth[m_iNumPropellers] = VEH_DAMAGE_HEALTH_STD;
			m_iNumPropellers ++;
		}
	}	

	m_fCurrentVerticalFlightRatio = GetVerticalFlightModeAvaliable() ? 1.0f: 0.0f;

	// If this is a seaplane, set up the anchor helper and the extension class for instance variables.
	if(pHandling->GetSeaPlaneHandlingData())
	{
		CSeaPlaneExtension* pExtension = GetExtension<CSeaPlaneExtension>();
		if(!pExtension)
		{
			pExtension = rage_new CSeaPlaneExtension();
			Assert(pExtension);
			GetExtensionList().Add(*pExtension);
		}

		if(pExtension)
		{
			pExtension->GetAnchorHelper().SetParent(this);
		}

		// for the VTOL seaplane we want the wings to default to the normal position
		m_fCurrentVerticalFlightRatio = 0.0f;
	}

	m_fDesiredVerticalFlightRatio = m_fCurrentVerticalFlightRatio;

	// Call InitCompositeBound again, as it has dependency with propellers
	InitCompositeBound();
}

Vec3V_Out CPlane::ComputeNosePosition() const
{
	Vec3V vStartCoords = GetVehiclePosition();
	ScalarV fOffsetScalar = ScalarV( GetVehicleModelInfo()->GetBoundingBoxMax().y);
	const Vec3V vNoseOffset = GetVehicleForwardDirection() * fOffsetScalar;
	return vStartCoords + vNoseOffset;
}

dev_float dfLargePlaneMass = 30000.0f;
bool CPlane::IsLargePlane() const
{
	// This mass limit passes for the Cargoplane, Jet and Titan
	return pHandling->m_fMass >= dfLargePlaneMass;
}

dev_float dfFastPlaneThrust = 1.0f;
bool CPlane::IsFastPlane() const
{
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	if (pFlyingHandling)
	{
		// This thrust limit passes for the Besra, Hydra, Lazer and Stunt
		return pFlyingHandling->m_fThrust >= dfFastPlaneThrust;
	}

	return false;
}

bool CPlane::AnyPlaneEngineOnFire() const
{
	return GetVehicleDamage()->GetEngineOnFire()
		|| (GetAircraftDamage().GetSectionHealth(CAircraftDamage::ENGINE_L) < ENGINE_DAMAGE_ONFIRE && GetAircraftDamage().GetSectionHealth(CAircraftDamage::ENGINE_L) > ENGINE_DAMAGE_FINSHED)
		|| (GetAircraftDamage().GetSectionHealth(CAircraftDamage::ENGINE_R) < ENGINE_DAMAGE_ONFIRE && GetAircraftDamage().GetSectionHealth(CAircraftDamage::ENGINE_R) > ENGINE_DAMAGE_FINSHED);
}

//
//
// CPlane::ProcessControl()
// 
// not decided what we want to do in here yet:
// probably not too much, unless we moving flyingControls calls
// here.  want to use specific vars for player/AI inputs so
// have to organise something
//
//

void CPlane::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	BANK_ONLY(const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), this );	
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity(), vThisPosition );

#if __BANK
	// draw a speedo for player car
	if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING && (GetStatus()==STATUS_PLAYER))
	{
		float fSpeed = DotProduct(GetVelocity(), VEC3V_TO_VECTOR3(GetTransform().GetB()));///GAME_VELOCITY_CONST;
		float fRoll = ( RtoD * GetTransform().GetRoll());

		char debugText[100];
		formatf(debugText,100, "Speed: %g, Height: %g, RollAngle: %g\n%s", fSpeed, vThisPosition.z, fRoll,m_landingGear.GetDebugStateName());
		grcDebugDraw::PrintToScreenCoors(debugText, 40,25);
		
		CFlightModelHelper tempHelper;
		tempHelper.DebugDrawControlInputs(this,m_fEngineSpeed,m_fPitchControl,m_fRollControl,m_fYawControl);		
	}

	if(CVehicle::ms_nVehicleDebug==VEH_DEBUG_DAMAGE)
	{
		m_aircraftDamage.DisplayDebugOutput(this);
	}
#endif

	if( GetDriver() && GetDriver()->IsLocalPlayer() && IsEngineOn() && IsInAir() && ShouldGenerateEngineShockingEvents() )
	{
		if ( !CPopCycle::IsCurrentZoneAirport() )
		{
			CEventShockingPlaneFlyby ev(*this);
			CShockingEventsManager::Add(ev);
		}
	}
	else if( IsPlayerDrivingOnGround() )
	{
		CEventShockingMadDriver madDriverEvent(*GetDriver());
		CShockingEventsManager::Add(madDriverEvent);
	}

	UpdatePropellers();
	SmoothInputs();

	m_landingGear.ProcessControl(this);
	
	// Then go ahead and call the parent version of process control.
	CAutomobile::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
}

static dev_float ms_fUpdateRotorBoundSpeedRatioThreshold = 0.1f;

bool CPlane::NeedUpdatePropellerBound(int iPropellerIndex) const
{
	float fSpeedRatio = m_propellers[iPropellerIndex].GetSpeed()/ms_fPropRenderSpeed;
	return fSpeedRatio <= ms_fUpdateRotorBoundSpeedRatioThreshold;
}

void CPlane::UpdatePropellers()
{
	float fDesEngineSpeed = ms_fStdEngineSpeed;
	bool keepEngineOnInWater = pHandling->GetSeaPlaneHandlingData() ||
							   !( m_iNumPropellers == 0 || m_fCurrentVerticalFlightRatio < 1.0f );

	if(GetDriver() && !GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
	{		
		if(!m_nVehicleFlags.bEngineOn || (GetIsInWater() && HasContactWheels() == false && !keepEngineOnInWater )
			|| m_Transmission.GetCurrentlyMissFiring())
		{
			if(IsInAir())
			{
				fDesEngineSpeed = GetWindBlowPropellorSpeed();
			}
			else
			{
				fDesEngineSpeed = 0.0f;
			}
		}
		else
		{
			if(!IsInAir() && GetNumPropellors() == 0)
			{
				fDesEngineSpeed = bfStdJetOnGroundEngineSpeed;
				if(m_fThrottleControl > 0.0f)
					fDesEngineSpeed = bfStdJetOnGroundEngineSpeed + m_fThrottleControl*(bfMaxJetOnGroundEngineSpeed - bfStdJetOnGroundEngineSpeed);
				else if(m_fThrottleControl < 0.0f)
					fDesEngineSpeed = bfStdJetOnGroundEngineSpeed + m_fThrottleControl*(bfStdJetOnGroundEngineSpeed - bfMinJetOnGroundEngineSpeed);
			}
			else
			{
				if(m_fThrottleControl > 0.0f)
					fDesEngineSpeed = ms_fStdEngineSpeed + m_fThrottleControl*(ms_fMaxEngineSpeed - ms_fStdEngineSpeed);
				else if(m_fThrottleControl < 0.0f)
					fDesEngineSpeed = ms_fStdEngineSpeed + m_fThrottleControl*(ms_fStdEngineSpeed - ms_fMinEngineSpeed);
			}
		}
	}
	else if(!GetIsUsingScriptAutoPilot() && !IsFullThrottleActive())
	{
		fDesEngineSpeed = 0.0f;
		if(m_fEngineSpeed == 0.0f && !IsNetworkClone() && !m_nVehicleFlags.bKeepEngineOnWhenAbandoned && !IsRunningCarRecording())
		{
			SwitchEngineOff();
		}
	}

	float fEngineSpeedDropRate      = ms_fEngineSpeedChangeRate;
    float fEngineSpeedIncreaseRate  = ms_fEngineSpeedChangeRate;

	if( GetIsInWater() && 
		!keepEngineOnInWater )
	{
		fEngineSpeedDropRate = Max(fEngineSpeedDropRate, ms_fEngineSpeedDropRateInWater);
	}
	if(m_Transmission.GetCurrentlyMissFiring() && GetVehicleDamage()->GetEngineHealth() > (sfPlaneEngineBreakDownHealth + dfEngineDegradeMaxDamage))
	{
		fEngineSpeedDropRate = Max(fEngineSpeedDropRate, ms_fEngineSpeedDropRateWhenMissFiring);
	}
	if(!m_nVehicleFlags.bEngineOn && IsInAir() && fDesEngineSpeed > 0.0f)
	{
		fEngineSpeedDropRate = Min(fEngineSpeedDropRate, dfEngineSpeedPassiveChangeRate);
	}
    if( GetModelIndex() == MI_PLANE_AVENGER )
    {
        fEngineSpeedIncreaseRate *= 2.0f;
    }

	m_fEngineSpeed += rage::Clamp(fDesEngineSpeed - m_fEngineSpeed,-fEngineSpeedDropRate*fwTimer::GetTimeStep(),fEngineSpeedIncreaseRate*fwTimer::GetTimeStep());

	if(m_fEngineSpeed == 0.0f)
	{
		m_fEngineWarmUpTimer = 0.0f;
	}
	else if(m_fEngineSpeed >= ms_fMinEngineSpeed || m_iNumPropellers == 0 /*Jet engine*/)
	{
		m_fEngineWarmUpTimer += fwTimer::GetTimeStep();
	}
	m_fEngineWarmUpTimer = Clamp(m_fEngineWarmUpTimer, 0.0f, dfEngineWarmUpMaxTime);

    int iPropsLeft = m_iNumPropellers;
	for(int i =0; i< m_iNumPropellers; i++)
    { 
        if(m_fPropellerHealth[i] <= 0.0f)
        {
            iPropsLeft--;
        }

		if(IsIndividualPropellerOn(i))
		{
			f32 propSpeedMult = GetPropellorSpeedMult(i);
			// we push this here to prevent an issue from the plane audio entity grabbing the propSpeedMult from the 
			// audio update thread, as it can block in the anim
			static_cast<audPlaneAudioEntity*>(m_VehicleAudioEntity)->SetPropellorSpeedMult(i, propSpeedMult); 
			m_propellers[i].UpdatePropeller(propSpeedMult * ms_fPropRenderSpeed, fwTimer::GetTimeStep());
		}
		else
		{
			f32 propSpeed = m_propellers[i].GetSpeed();
			// stop propeller in short time
			propSpeed *= 1.0f - fwTimer::GetTimeStep();
			f32 propSpeedMult = propSpeed / ms_fPropRenderSpeed;
			static_cast<audPlaneAudioEntity*>(m_VehicleAudioEntity)->SetPropellorSpeedMult(i, propSpeedMult); 
			m_propellers[i].UpdatePropeller(propSpeed, fwTimer::GetTimeStep());
		}

		// B*1954149: Modify the propeller disc bound include flags based on the propeller speed.
		// Disc bound should not collide with anything if the propeller isn't moving.
		if(GetVehicleFragInst() && GetVehicleFragInst()->GetArchetype()->GetBound())
		{
			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());	

			if(pBoundComp->GetTypeAndIncludeFlags() && m_propellerCollision[i].GetFragDisc() > -1)
			{
				u32 nRotorIncludeFlags = ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;
				nRotorIncludeFlags &= ~(ArchetypeFlags::GTA_AI_TEST | ArchetypeFlags::GTA_SCRIPT_TEST | ArchetypeFlags::GTA_WHEEL_TEST);

				if( GetModelIndex() == MI_PLANE_MICROLIGHT &&
					( !IsIndividualPropellerOn( i ) ||
					( !sbEnablePropellerMods && i > 0 ) ) )
				{
					nRotorIncludeFlags = 0;
					//m_propellers[ i ].SetPropellerVisible( false );
				}
				pBoundComp->SetIncludeFlags(m_propellerCollision[i].GetFragDisc(), nRotorIncludeFlags);
			}
		}
	}  
    
    //if we're out of props then turn the engine off
    if(m_iNumPropellers > 0 && iPropsLeft == 0 && !IsNetworkClone())
    {
        SwitchEngineOff();
        //also kill the engine if it isn't already
        if(m_Transmission.GetEngineHealth() > ENGINE_DAMAGE_ONFIRE)
        {
            m_Transmission.SetEngineHealth(ENGINE_DAMAGE_ONFIRE);
        }
    }
}

dev_float fWindBlowingPropellorMaxSpeed = 0.025f;
dev_float dfWindBlowingPropellorTopWindSpeed = 10.0f;
dev_float dfWindBlowingPropellorMinAirSpeedRatio = 0.05f;
dev_float dfWindBlowingPropellorMaxAirSpeedRatio = 0.25f;

float CPlane::GetWindBlowPropellorSpeed()
{
	float fWindBlowingPropellorSpeed = 0.0f;

	if(m_fEngineSpeed < fWindBlowingPropellorMaxSpeed)
	{
		Vector3 vVelocity = GetVelocity();
		Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);
		WIND.GetLocalVelocity(GetTransform().GetPosition(), RC_VEC3V(vecWindSpeed), false, false);

		Vector3 vHeading = VEC3V_TO_VECTOR3(GetMatrix().GetCol1());
		float fForwardSpeed = Abs(vVelocity.Dot(vHeading));
		float fForwardWindSpeed = Abs(vecWindSpeed.Dot(vHeading));
		if(fForwardSpeed > pHandling->m_fEstimatedMaxFlatVel * dfWindBlowingPropellorMinAirSpeedRatio)
		{
			fWindBlowingPropellorSpeed = Clamp(fForwardSpeed / (pHandling->m_fEstimatedMaxFlatVel * dfWindBlowingPropellorMaxAirSpeedRatio) + fForwardWindSpeed / dfWindBlowingPropellorTopWindSpeed, dfWindBlowingPropellorMinAirSpeedRatio, 1.0f) * fWindBlowingPropellorMaxSpeed;
		}
	}

	return fWindBlowingPropellorSpeed;
}


float CPlane::GetPropellorSpeedMult(int i) const
{
	float OneMinusEngineSpeedCubed = 1.0f - m_fEngineSpeed;
	OneMinusEngineSpeedCubed *= OneMinusEngineSpeedCubed * OneMinusEngineSpeedCubed;

	float fSectionHealthMult = 1.0f;
	bool bEngineSectionDead = false;
	if(m_iNumPropellers > 1)
	{
		if(m_propellers[i].GetBoneIndex() >= 0)
		{
			Matrix34 propellerMat = GetObjectMtx(m_propellers[i].GetBoneIndex());

			if(propellerMat.d.x < 0.0f)
			{
				fSectionHealthMult = GetAircraftDamage().GetSectionHealthAsFraction(CAircraftDamage::ENGINE_L) * 2.0f;
			}
			else
			{
				fSectionHealthMult = GetAircraftDamage().GetSectionHealthAsFraction(CAircraftDamage::ENGINE_R) * 2.0f;
			}

			if(fSectionHealthMult <= 0.0f || GetVehicleDamage()->GetEngineHealth() < sfPlaneEngineBreakDownHealth)
			{
				bEngineSectionDead = true;
			}
		}
	}

	float OneMinusEngineHealthCubed = 1.0f - Clamp((GetVehicleDamage()->GetEngineHealth() * fSectionHealthMult) / ENGINE_HEALTH_MAX, 0.0f, 1.0f);
	OneMinusEngineHealthCubed *= OneMinusEngineHealthCubed * OneMinusEngineHealthCubed;
	static float fMinEngineHealthMult = 0.5f;
	if(bEngineSectionDead)
	{
		OneMinusEngineHealthCubed = 1.0;
	}
	else
	{
		OneMinusEngineHealthCubed = Clamp(OneMinusEngineHealthCubed, 0.0f, fMinEngineHealthMult);
	}

	float fPropSpeedMult = (1.0f - OneMinusEngineSpeedCubed) * (1.0f - OneMinusEngineHealthCubed);

	static float fFourBladesPropellerSpeedMulti = 31.2f / ms_fPropRenderSpeed;

	if(GetModelIndex() == MI_PLANE_TITAN || (MI_PLANE_VELUM2.IsValid() && GetModelIndex() == MI_PLANE_VELUM2.GetModelIndex()))
	{
		fPropSpeedMult *= fFourBladesPropellerSpeedMulti;
	}

	if( GetModelIndex() == MI_PLANE_STARLING )
	{
		fPropSpeedMult = Min( 1.0f, Dot( GetTransform().GetForward(), VECTOR3_TO_VEC3V( GetVelocity() ) ).Getf() / 20.0f );
		fPropSpeedMult *= fPropSpeedMult;

		if( GetStatus() == STATUS_WRECKED )
		{
			fPropSpeedMult = 0.0f;
		}

		float currentPropSpeed = m_propellers[0].GetSpeed() / ms_fPropRenderSpeed;
		fPropSpeedMult = currentPropSpeed + ( ( fPropSpeedMult - currentPropSpeed ) * 0.2f );
	}

	return fPropSpeedMult;
}

static dev_float dfPlaneJoystickSmoothSpeed = 5.0f;

void CPlane::SmoothInputs()
{
	float fRandomControlYaw = 0.0f;
	float fRandomControlPitch = 0.0f;
	float fRandomControlRoll = 0.0f;
	bool isPlayerInControl = false;
	static dev_float sfMinVelocityForRandomControl = 10.0f;

	if(GetDriver() && GetDriver()->IsPlayer())
	{
		if(((GetIntelligence()->GetActiveTask() &&
			GetIntelligence()->GetActiveTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_PLAYER_DRIVE_PLANE) || IsNetworkClone()) &&
			GetVelocity().Mag2() > sfMinVelocityForRandomControl )
		{
			isPlayerInControl = true;
			CPlayerInfo* pPlayerInfo = GetDriver()->GetPlayerInfo();
			physicsFatalAssertf( pPlayerInfo, "Expected a player info!" ); // Assumed to be a player
			fRandomControlYaw = pPlayerInfo->GetRandomControlYaw();
			fRandomControlPitch = pPlayerInfo->GetRandomControlPitch();
			fRandomControlRoll = pPlayerInfo->GetRandomControlRoll();
		}
	}

	float fMaxDelta = fwTimer::GetTimeStep() * (isPlayerInControl ? ms_fControlSmoothSpeed : dfControlSmoothSpeedForAI);

	m_fYaw += rage::Clamp(m_fYawControl + fRandomControlYaw - m_fYaw, -fMaxDelta, fMaxDelta);
	m_fRoll += rage::Clamp(m_fRollControl + fRandomControlRoll - m_fRoll, -fMaxDelta, fMaxDelta);
	m_fPitch += rage::Clamp(m_fPitchControl + fRandomControlPitch - m_fPitch, -fMaxDelta, fMaxDelta);
	if( IsDriverAPlayer() )
	{
		if( isPlayerInControl )
		{
			m_fBrake += rage::Clamp(-m_fThrottleControl - m_fBrake, -fMaxDelta, fMaxDelta);
		}
		else
		{
			// GTAV - B*1771418 - air brakes flicker too much on vehicle select pre race screen
			fMaxDelta *= 0.5f;
			m_fBrake += rage::Clamp(-m_fThrottleControl - m_fBrake, -fMaxDelta, fMaxDelta);
		}
	}
	else
	{
		m_fBrake += rage::Clamp(Max(-m_fThrottleControl, m_fAirBrakeControl) - m_fBrake, -fMaxDelta, fMaxDelta);
	}
	m_fBrake = rage::Clamp(m_fBrake, 0.0f, 1.0f);

	// smooth out the joystick control
	if(GetDriver() && GetDriver()->IsInFirstPersonVehicleCamera())
	{
		TUNE_GROUP_FLOAT(JOYSTICK_IK, PlaneJoyStickLerp, 0.1f, 0.0f, 1.0f, 0.001f);
		m_fJoystickPitch = rage::Lerp(PlaneJoyStickLerp, m_fJoystickPitch, m_fPitchControl);
		m_fJoystickRoll = rage::Lerp(PlaneJoyStickLerp, m_fJoystickRoll, m_fRollControl);

		CTaskMotionBase *pCurrentMotionTask = GetDriver()->GetCurrentMotionTask();
		if (pCurrentMotionTask && pCurrentMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_IN_AUTOMOBILE)
		{
			const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(pCurrentMotionTask);
			float fAnimWeight = pAutoMobileTask->GetSteeringWheelWeight();
			m_fJoystickPitch *= fAnimWeight;
			m_fJoystickRoll *= fAnimWeight;
		}
	}
	else
	{
		// smooth out the joystick control
		fMaxDelta = fwTimer::GetTimeStep() * dfPlaneJoystickSmoothSpeed;
		m_fJoystickPitch += rage::Clamp(m_fPitchControl + fRandomControlPitch - m_fJoystickPitch, -fMaxDelta, fMaxDelta);
		m_fJoystickRoll += rage::Clamp(m_fRollControl + fRandomControlRoll - m_fJoystickRoll, -fMaxDelta, fMaxDelta);
	}
}

//
//
//
void CPlane::DisableAileron(bool bLeftAileron, bool bDisableAileron)
{
    if(bLeftAileron)
    {
        m_bDisableLeftAileron = bDisableAileron;
    }
    else
    {
        m_bDisableRightAileron = bDisableAileron;
    }

}

//
//
//
bool CPlane::IsPropeller (int nComponent) const
{
	for(int i = 0; i < m_iNumPropellers; i++)
	{
		if(m_propellerCollision[i].IsPropellerComponent(this, nComponent))
		{
			// Allow collision with blade bound when its stationary
			if(m_propellerCollision[i].GetFragDisc() != nComponent && m_propellers[i].GetSpeed() == 0.0f)
			{
				return false;
			}

			return true;
		}
	}
	return false;
}

bool CPlane::ShouldKeepRotorInCenter(CPropeller *pPropeller) const
{
	for(int i = 0; i < m_iNumPropellers; i++)
	{
		if(&m_propellers[i] == pPropeller)
		{
			if(m_propellerCollision[i].GetFragChild() >= 0)
			{
				int iParentGroupIndex = GetFragInst()->GetTypePhysics()->GetChild(m_propellerCollision[i].GetFragChild())->GetOwnerGroupPointerIndex();
				if(iParentGroupIndex != 0xFF)
				{
					fragTypeGroup* pParentGroup = GetFragInst()->GetTypePhysics()->GetGroup(iParentGroupIndex);
					// Keep the rotor in center when it's attached to the chassis
					if(pParentGroup && pParentGroup->GetParentGroupPointerIndex() == 0)
					{
						return true;
					}
				}
			}
			break;
		}
	}
	return false;
}

bool CPlane::IsStalling() const
{
	if(m_VehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
	{
		return static_cast<audPlaneAudioEntity*>(m_VehicleAudioEntity)->GetIsStallWarningOn();
	}
	return false;
}

//
//
//
void CPlane::ProcessFlightHandling(float fTimeStep)
{
	if(fTimeStep<=0.0f)
		return;
	if (IsFullThrottleActive())
	{
		SetThrottleControl(10);
		if (!GetDriver() || !GetDriver()->IsPlayer())
		{
			SetYawControl(0);
			SetPitchControl(0);
			SetRollControl(0);
		}
	}

	if (m_nVehicleFlags.bIsDrowning && !IsNetworkClone())
	{
		m_fEngineSpeed = 0.0f;
		m_fThrottleControl = 0.0f;
		SwitchEngineOff();
	}

    SetDampingForFlight(m_fCurrentVerticalFlightRatio);

	if(GetStatus() != STATUS_WRECKED && !(m_aircraftDamage.HasSectionBrokenOff(this,CAircraftDamage::WING_L) && m_aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_R)))
	{
		//if we can go into vertical flight mode then process both flight models
		if(GetVerticalFlightModeAvaliable())
		{
			if(m_fCurrentVerticalFlightRatio > 0.0f)//no point processing a flight model if it's completely turned off
			{
				ProcessVerticalFlightModel(fTimeStep, m_fCurrentVerticalFlightRatio);
			}
			if(m_fCurrentVerticalFlightRatio < 1.0f)//no point processing a flight model if it's completely turned off
			{
				ProcessFlightModel(fTimeStep, 1.0f - m_fCurrentVerticalFlightRatio);
			}
		}
		else
		{   
			//normal plane just process normal flight model.
			ProcessFlightModel(fTimeStep);
		}

		if(GetDriver() && GetDriver()->IsLocalPlayer())
		{
			CControl* pControl = GetDriver()->GetControlFromPlayer();

			if(pControl)
			{
				// If hold L2 long enough, turn the engine off - B*931846
				float fPlayerThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();
				if(fPlayerThrottleControl < -0.9f && IsInAir())
				{
					m_fTurnEngineOffTimer += fTimeStep;
				}
				else
				{
					if(fPlayerThrottleControl > 0.9f && IsInAir())
					{
						ResetEngineTurnedOffByPlayer();
					}
					else if(Abs(fPlayerThrottleControl) > 0.9f && !IsInAir())
					{
						ResetEngineTurnedOffByPlayer();
					}
					m_fTurnEngineOffTimer = 0.0f;
				}

				if(m_fTurnEngineOffTimer > 1.0f && IsEngineOn() && IsInAir() &&
					( !GetVerticalFlightModeAvaliable() || m_fCurrentVerticalFlightRatio < 1.0f ) )
				{
					SwitchEngineOff();
					m_bIsEngineTurnedOffByPlayer = true;
				}
			}
		}
	}

    Assert(GetTransform().IsOrthonormal());

}

const int diAfterburnerEffectProbeResults = 10;
void CPlane::ProcessPrePhysics()
{
	CAutomobile::ProcessPrePhysics();
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

	for (int i = 0; i < PLANE_NUM_AFTERBURNERS; ++i)
	{
		// Blow away everything behind the jet
		if (sm_bJetWashEnabled &&
			m_nVehicleFlags.bEngineOn && GetVehicleModelInfo() && GetVehicleModelInfo()->GetBoneIndex(PLANE_AFTERBURNER + i) != -1 && pFlyingHandling)
		{
			//Find out the thrust force
			Vector3 vBlowDirection = VEC3V_TO_VECTOR3(GetTransform().GetB());
			vBlowDirection.Negate();
			float fBlowForce = m_fGravityForWheelIntegrator*GetEngineSpeed()*GetMass()*m_aircraftDamage.GetThrustMult(this)*pFlyingHandling->m_fAfterburnerEffectForceMulti;

			s32 afterburnerBoneId = GetVehicleModelInfo()->GetBoneIndex(PLANE_AFTERBURNER + i);
			Matrix34 matGlobal;
			GetGlobalMtx(afterburnerBoneId, matGlobal);

			//setup the capsule test.
			WorldProbe::CShapeTestHitPoint testHitPoints[diAfterburnerEffectProbeResults];
			WorldProbe::CShapeTestResults capsuleResults(testHitPoints, diAfterburnerEffectProbeResults);
			const u32 iTestFlags = (ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetResultsStructure(&capsuleResults);
			capsuleDesc.SetIncludeFlags(iTestFlags);
			capsuleDesc.SetExcludeEntity(this);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetDoInitialSphereCheck(true);
			capsuleDesc.SetCapsule(matGlobal.d, matGlobal.d + vBlowDirection * pFlyingHandling->m_fAfterburnerEffectDistance, pFlyingHandling->m_fAfterburnerEffectRadius);
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

			for (WorldProbe::ResultIterator it = capsuleResults.begin(); it < capsuleResults.last_result(); ++it)
			{
				if (it->IsAHit())
				{
					CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());
					bool bProcessHit = (pHitEntity && pHitEntity->GetIsPhysical() && pHitEntity->IsCollisionEnabled());

					if (NetworkInterface::IsGameInProgress() && bProcessHit)
					{
						if (pHitEntity->GetIsTypePed())
						{
							CPed* pVictim = static_cast<CPed*>(pHitEntity);
							bProcessHit = NetworkInterface::IsFriendlyFireAllowed(pVictim, GetDriver());
						}
						else if (pHitEntity->GetIsTypeVehicle())
						{
							CVehicle* pVictimVehicle = static_cast<CVehicle*>(pHitEntity);
							bProcessHit = NetworkInterface::IsFriendlyFireAllowed(pVictimVehicle, GetDriver());
						}
					}

					if (bProcessHit)
					{
						CPhysical *pHitPhysical = (CPhysical *)pHitEntity;

						Vector3 vWorldCentroid;
						float fCentroidRadius = 0.0f;
						if (pHitEntity->GetCurrentPhysicsInst() && pHitEntity->GetCurrentPhysicsInst()->GetArchetype())
						{
							vWorldCentroid = VEC3V_TO_VECTOR3(pHitEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetWorldCentroid(pHitEntity->GetCurrentPhysicsInst()->GetMatrix()));
							fCentroidRadius = pHitEntity->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
						}
						else
						{
							fCentroidRadius = pHitEntity->GetBoundCentreAndRadius(vWorldCentroid);
						}

						Vector3 vOffset = it->GetHitPosition() - VEC3V_TO_VECTOR3(pHitPhysical->GetMatrix().GetCol3());
						if (it->GetHitPosition().Dist2(vWorldCentroid) > square(fCentroidRadius))
						{
							Vector3 vOffsetToCentroid = it->GetHitPosition() - vWorldCentroid;
							vOffsetToCentroid *= (fCentroidRadius - 0.1f) / it->GetHitPosition().Dist(vWorldCentroid);
							Vector3 vHitPositionAdjusted = vWorldCentroid + vOffsetToCentroid;
							vOffset = vHitPositionAdjusted - VEC3V_TO_VECTOR3(pHitPhysical->GetMatrix().GetCol3());
						}

						if (pHitEntity->GetIsTypePed())
						{
							CPed* pHitPed = static_cast<CPed*>(pHitEntity);
							if (pHitPed && !pHitPed->GetUsingRagdoll() && !pHitPed->GetIsParachuting() && !pHitPed->IsNetworkClone())
							{
								CEventSwitch2NM event(10000, rage_new CTaskNMRelax(1000, 10000));
								pHitPed->SwitchToRagdoll(event);

								//Calculate and apply damage from jetwash
								{
									u32 weaponHash = WEAPONTYPE_FALL;
									int nComponent = RAGDOLL_SPINE0;
									float fDamage = 50.f;

									//Apply damage.
									CEventDamage tempDamageEvent(GetDriver(), fwTimer::GetTimeInMilliseconds(), weaponHash);
									CPedDamageCalculator damageCalculator(NULL, fDamage, weaponHash, nComponent, false);
									damageCalculator.ApplyDamageAndComputeResponse(pHitPed, tempDamageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);
								}
							}
						}

						float fForceMulti = 1.0f - Clamp(it->GetHitPosition().Dist(matGlobal.d) / (pFlyingHandling->m_fAfterburnerEffectDistance + pFlyingHandling->m_fAfterburnerEffectRadius), 0.1f, 1.0f);
						float fAcceleration = fBlowForce * fForceMulti / pHitPhysical->GetMass();
						fAcceleration = Min(fAcceleration, DEFAULT_ACCEL_LIMIT - 0.1f);
						Vector3 vBlowForce = vBlowDirection * (pHitPhysical->GetMass() * fAcceleration);
						pHitPhysical->ApplyForce(vBlowForce, vOffset, it->GetHitComponent());
					}
				}
			}
#if __BANK
			if (ms_bDebugAfterburnerEffect)
			{
				grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(matGlobal.d), VECTOR3_TO_VEC3V(matGlobal.d + vBlowDirection * pFlyingHandling->m_fAfterburnerEffectDistance), pFlyingHandling->m_fAfterburnerEffectRadius, Color_red, false);
			}
#endif
		}

	}
}


static dev_float sfNozzleAdjustAngle = -( DtoR * 90.0f);
static dev_float sfRudderAdjustAngleForVTOL = ( DtoR * 30.0f);
static dev_float sfRudderAdjustAngleForTULA = ( DtoR * 25.0f);

static dev_float sf_VerticalFlightTransitionSpeed = 1.0f;
static dev_float sf_VerticalFlightTransitionSpeedTULA = 0.25f;
dev_float dfPlaneLandingPadShakingIntensityMult = 0.1f;
ePhysicsResult CPlane::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	TUNE_GROUP_BOOL( VEHICLE_TURBULENCE, DISABLE_PLANE_TURBULENCE, false );

	m_landingGear.ProcessPhysics(this);
	m_aircraftDamage.ProcessPhysics(this, fTimeStep);
	m_landingGearDamage.ProcessPhysics(this);

	if( !DISABLE_PLANE_TURBULENCE )
	{
		ProcessTurbulence(fTimeStep);
	}

    //Work out the position of the nozzles for vertical flight mode use
    if(GetVerticalFlightModeAvaliable())
    {
		float verticalFlightModeTransitionSpeed = sf_VerticalFlightTransitionSpeed;

		if( ( MI_PLANE_TULA.IsValid() &&
			  GetModelIndex() == MI_PLANE_TULA ) ||
			GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE ) )
		{
			verticalFlightModeTransitionSpeed = sf_VerticalFlightTransitionSpeedTULA;
		}

		if( !m_bDisableVerticalFlightModeTransition )
		{
			if(m_fCurrentVerticalFlightRatio < m_fDesiredVerticalFlightRatio)
			{
				m_fCurrentVerticalFlightRatio += verticalFlightModeTransitionSpeed * fTimeStep;
				m_fCurrentVerticalFlightRatio = rage::Clamp(m_fCurrentVerticalFlightRatio, 0.0f, m_fDesiredVerticalFlightRatio);
			}
			else if(m_fCurrentVerticalFlightRatio > m_fDesiredVerticalFlightRatio)
			{
				m_fCurrentVerticalFlightRatio -= verticalFlightModeTransitionSpeed * fTimeStep;
				m_fCurrentVerticalFlightRatio = rage::Clamp(m_fCurrentVerticalFlightRatio, m_fDesiredVerticalFlightRatio, 1.0f);
			}
		}
    }

	// Seaplanes need to be forced to sink if required when destroyed.
	if(pHandling->GetSeaPlaneHandlingData())
	{
		CSeaPlaneExtension* pSeaPlaneExtension = this->GetExtension<CSeaPlaneExtension>();
		Assert(pSeaPlaneExtension);
		if(pSeaPlaneExtension && m_nVehicleFlags.bBlownUp && pSeaPlaneExtension->m_nFlags.bSinksWhenDestroyed)
		{
			// If we are anchored then release anchor.
			if(pSeaPlaneExtension->GetAnchorHelper().IsAnchored())
			{
				pSeaPlaneExtension->GetAnchorHelper().Anchor(false);
			}

			if(m_Buoyancy.m_fForceMult > 0.0f)
			{
				const float kfSinkForceMultStep = 0.002f;
				m_Buoyancy.m_fForceMult -= kfSinkForceMultStep;
			}
			else
			{
				m_Buoyancy.m_fForceMult = 0.0f;
			}
		}
	}

	ePhysicsResult eResult = CAutomobile::ProcessPhysics(fTimeStep,bCanPostpone,nTimeSlice);
	m_bIsPropellerDamagedThisFrame = false;
	m_bEnableThrustFallOffThisFrame = true;

	if(GetNumWheels() > 0 && GetCollider())
	{
#if APPLY_GRAVITY_IN_INTEGRATOR
		// Turn the gravity off completely for articulated body, this fix the issue that plane tends to fall down after switching to articulated.
		if(GetCollider()->IsArticulated())
		{
			GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
			GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, true);
		}
		else
			GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
#else
		GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, false);
		GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
#endif
	}

	if(GetDriver() && GetDriver()->IsLocalPlayer())
	{
		int nNumberWheelsJustTouchGround = 0;
		int nNumberWheelsTouchGround = 0;
		for(int i=0; i<GetNumWheels(); i++)
		{
			CWheel *pWheel = GetWheel(i);
			if(pWheel && pWheel->GetIsTouching())
			{
				nNumberWheelsTouchGround++;
				if(!pWheel->GetWasTouching())
				{
					nNumberWheelsJustTouchGround++;
				}
			}
		}

		if(nNumberWheelsJustTouchGround > 0 && nNumberWheelsTouchGround < 2)
		{
			CControlMgr::StartPlayerPadShakeByIntensity(400, float(nNumberWheelsJustTouchGround) * 0.5f * dfPlaneLandingPadShakingIntensityMult);
		}
	}

	// Start the crash task if a AI plane lost one or both wings, or engine will never start again, or we're already below min height map somehow
	if(GetDriver() && !GetDriver()->IsPlayer() && !m_bDisableAutomaticCrashTask)
	{
		bool bEngineWillNeverStart = !IsEngineOn() && GetVehicleDamage()->GetEngineHealth() <= sfPlaneEngineBreakDownHealth;
		Vec3V vVehiclePos = GetTransform().GetPosition();
		float fMinHeightAtPos = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vVehiclePos.GetXf(), vVehiclePos.GetYf());
		bool bBelowMinHeight = vVehiclePos.GetZf() < fMinHeightAtPos;
		if(m_aircraftDamage.HasSectionBrokenOff(this,CAircraftDamage::WING_L) || m_aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_R) || bEngineWillNeverStart || bBelowMinHeight)
		{
			// Create a crash task if one doesn't already exist
			if (!IsNetworkClone())
			{
				aiTask *pActiveTask = GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_CRASH);
				if(!pActiveTask)
				{
					CEntity* pLastDamageInflictor = m_aircraftDamage.GetLastDamageInflictor();
					CTaskVehicleCrash *pCarTask = rage_new CTaskVehicleCrash(pLastDamageInflictor);

					pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_BlowUpInstantly, false);
					pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_InACutscene, false);
					pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_AddExplosion, true);

					GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_CRASH);
				}
				// Do not automatically crash a second time!
				m_bDisableAutomaticCrashTask = true;
			}
		}
	}

	// Tilt-able wings
	if(GetVerticalFlightModeAvaliable())
	{
		if( ( MI_PLANE_TULA.IsValid() &&
			  GetModelIndex() == MI_PLANE_TULA ) ||
			GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE ) )
		{
			fragInst *pVehicleFragInst = GetVehicleFragInst();
			float currentAngle = GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE ) ? ( 1.0f - m_fCurrentVerticalFlightRatio ) * sfNozzleAdjustAngle : -m_fCurrentVerticalFlightRatio * sfNozzleAdjustAngle;

			for( int i = 0; i < 2; i++ )
			{
				int iBoneIndex = GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE ) ? GetBoneIndex( (eHierarchyId)( (int)PLANE_ENGINE_L + i ) ) : GetBoneIndex( (eHierarchyId)( (int)PLANE_WING_L + i ) );

				if( iBoneIndex > -1 )
				{
					int iFragGroup = (s8)pVehicleFragInst->GetGroupFromBoneIndex(iBoneIndex);
					int iFragChild = -1;

					if(iFragGroup > -1)
					{
						fragTypeGroup* pGroup = pVehicleFragInst->GetTypePhysics()->GetAllGroups()[iFragGroup];
						iFragChild = (s8)pGroup->GetChildFragmentIndex();
					}
					else
					{
						iFragChild = (s8)pVehicleFragInst->GetComponentFromBoneIndex(iBoneIndex);
					}

					SetBoneRotation( iBoneIndex, ROT_AXIS_LOCAL_X, currentAngle, true, NULL, NULL );
					GetSkeleton()->PartialUpdate(iBoneIndex);

					if(iBoneIndex > -1 && iFragGroup > -1 && iFragChild > -1)
					{
						if(	pVehicleFragInst->GetArchetype() &&
							pVehicleFragInst->GetArchetype()->GetBound() &&
							phBound::IsTypeComposite(pVehicleFragInst->GetArchetype()->GetBound()->GetType()))
						{	
							phBoundComposite *pBound = (phBoundComposite *)pVehicleFragInst->GetArchetype()->GetBound();
							fragTypeGroup* pGroup = pVehicleFragInst->GetTypePhysics()->GetAllGroups()[iFragGroup];
							UpdateLinkAttachmentMatricesRecursive(pBound, pGroup );
						}
					}
				}
			}
		}
	}


	for(int i =0; i< m_iNumPropellers; i++)
	{ 		
		m_propellerCollision[i].UpdateBound(this, m_propellers[i].GetBoneIndex(), m_propellers[i].GetAngle(), m_propellers[i].GetAxis(), NeedUpdatePropellerBound(i));		
	}  


#if __BANK
	if(GetDriver() && GetDriver()->IsPlayer() && NetworkInterface::IsGameInProgress() && GetPhysArch() && GetPhysArch()->GetBound() && GetPhysArch()->GetBound()->GetType() == phBound::COMPOSITE)
	{
		phBoundComposite *pCompositeBound = static_cast<phBoundComposite *>(GetPhysArch()->GetBound());
		pCompositeBound->CalculateCompositeExtents();
		bool bBadBoundingBox = false;
		if(pCompositeBound->GetRadiusAroundCentroid() > 5.0f * GetBaseModelInfo()->GetBoundingSphereRadius())
		{
			bBadBoundingBox = true;
		}


		if(VEC3V_TO_VECTOR3(pCompositeBound->GetBoundingBoxMax() - pCompositeBound->GetBoundingBoxMin()).Mag2() >
			17.0f * (GetBaseModelInfo()->GetBoundingBoxMax() - GetBaseModelInfo()->GetBoundingBoxMin()).Mag2())
		{
			bBadBoundingBox = true;
		}

		Assertf(!bBadBoundingBox, "[B*2075251] %s: Physics calculated a bounding box bigger than expected (Radius around centroid: %f expected: %f) (Bound box size: %f expected: %f).",
			GetDebugName(),
			pCompositeBound->GetRadiusAroundCentroid(),
			GetBaseModelInfo()->GetBoundingSphereRadius(),
			VEC3V_TO_VECTOR3(pCompositeBound->GetBoundingBoxMax() - pCompositeBound->GetBoundingBoxMin()).Mag2(),
			(GetBaseModelInfo()->GetBoundingBoxMax() - GetBaseModelInfo()->GetBoundingBoxMin()).Mag2()
			);

		if(bBadBoundingBox)
		{
			if(GetNetworkObject())
			{
				Displayf("[B*2075251] Vehicle (%s Net: %s)", GetDebugName(), GetNetworkObject()->GetLogName());
			}

			for(int nBound = 0; nBound < pCompositeBound->GetNumBounds(); nBound++)
			{
				Mat34V currMat = pCompositeBound->GetCurrentMatrix(nBound);
				Mat34V lastMat = pCompositeBound->GetLastMatrix(nBound);
				Displayf("\ncomponent %i bound matrices set:\n\nCurrent:\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\nLast:\n%f, %f, %f, %f\n%f, %f, %f, %f\n%f, %f, %f, %f\n",					
					nBound,
					currMat.GetCol0().GetXf(), currMat.GetCol1().GetXf(), currMat.GetCol2().GetXf(), currMat.GetCol3().GetXf(),
					currMat.GetCol0().GetYf(), currMat.GetCol1().GetYf(), currMat.GetCol2().GetYf(), currMat.GetCol3().GetYf(),
					currMat.GetCol0().GetZf(), currMat.GetCol1().GetZf(), currMat.GetCol2().GetZf(), currMat.GetCol3().GetZf(),
					lastMat.GetCol0().GetXf(), lastMat.GetCol1().GetXf(), lastMat.GetCol2().GetXf(), lastMat.GetCol3().GetXf(),
					lastMat.GetCol0().GetYf(), lastMat.GetCol1().GetYf(), lastMat.GetCol2().GetYf(), lastMat.GetCol3().GetYf(),
					lastMat.GetCol0().GetZf(), lastMat.GetCol1().GetZf(), lastMat.GetCol2().GetZf(), lastMat.GetCol3().GetZf()
					);
			}
		}

	}
#endif

	if( !IsInAir() &&
		MI_PLANE_STARLING.IsValid() && 
		GetModelIndex() == MI_PLANE_STARLING )
	{
		Vec3V groundNormal = VECTOR3_TO_VEC3V( GetWheel(0)->GetHitNormal() + GetWheel(1)->GetHitNormal() ) * ScalarV( 0.5f );
		if( Dot( GetTransform().GetC(), groundNormal ).Getf() > 0.3f )
		{
			static dev_float sfTorqueScale = 50.0f;
			Vec3V rightVector = Cross( GetTransform().GetB(), groundNormal );
			float torque = sfTorqueScale * GetAngInertia().y * ( GetTransform().GetA().GetZf() - rightVector.GetZf() );

			Vector3 torqueVector = VEC3V_TO_VECTOR3( GetTransform().GetB() ) * torque;
			
			ApplyInternalTorque( torqueVector );
		}
	}

	UpdateMeredithVent();

	int handlebarsBone = GetBoneIndex( PLANE_HANDLEBARS );
	if( GetOwnedBy() != ENTITY_OWNEDBY_CUTSCENE &&
		handlebarsBone != -1 )
	{
		static dev_float maxRollScale = 0.1f;
		static dev_float maxPitchScale = 0.1f;
		static dev_float maxVisualRateOfChange = 1.0f;

		m_fVisualRoll	= m_fVisualRoll + ( Clamp( m_fRoll - m_fVisualRoll, -maxVisualRateOfChange * fTimeStep, maxVisualRateOfChange * fTimeStep ) );
		m_fVisualPitch	= m_fVisualPitch + ( Clamp( m_fPitch - m_fVisualPitch, -maxVisualRateOfChange * fTimeStep, maxVisualRateOfChange * fTimeStep ) );
		SetComponentRotation( PLANE_HANDLEBARS, ROT_AXIS_LOCAL_Y, m_fVisualRoll * maxRollScale, true ); 
		SetComponentRotation( PLANE_HANDLEBARS, ROT_AXIS_LOCAL_X, m_fVisualPitch * maxPitchScale, false ); 

		fragInst *pVehicleFragInst = GetVehicleFragInst();

		int iFragGroup = (s8)pVehicleFragInst->GetGroupFromBoneIndex(handlebarsBone);
		int iFragChild = -1;

		if(iFragGroup > -1)
		{
			fragTypeGroup* pGroup = pVehicleFragInst->GetTypePhysics()->GetAllGroups()[iFragGroup];
			iFragChild = (s8)pGroup->GetChildFragmentIndex();
		}
		else
		{
			iFragChild = (s8)pVehicleFragInst->GetComponentFromBoneIndex(handlebarsBone);
		}

		GetSkeleton()->PartialUpdate(handlebarsBone);

		if(iFragGroup > -1 && iFragChild > -1)
		{
			if(	pVehicleFragInst->GetArchetype() &&
				pVehicleFragInst->GetArchetype()->GetBound() &&
				phBound::IsTypeComposite(pVehicleFragInst->GetArchetype()->GetBound()->GetType()))
			{
				phBoundComposite *pBound = (phBoundComposite *)pVehicleFragInst->GetArchetype()->GetBound();
				fragTypeGroup* pGroup = pVehicleFragInst->GetTypePhysics()->GetAllGroups()[iFragGroup];
				UpdateLinkAttachmentMatricesRecursive(pBound, pGroup );
			}
		}
	}

	static dev_s32 maxNumberOfWingFlaps = 2;
	for( int i = 0; i < maxNumberOfWingFlaps; i++ )
	{
		eHierarchyId flapBoneId = (eHierarchyId)( (int)PLANE_WINGFLAP_L + i );
		int flapBone = GetBoneIndex( flapBoneId );

		if( flapBone != -1 )
		{
			static dev_float sf_maxRotationAngle = DtoR * 25.0f;
			static dev_float maxFlapRateOfChange = 0.1f;

			const CLandingGear&  gear = GetLandingGear();
			float targetFlapAngle = gear.GetGearDeployRatio();

			targetFlapAngle = Max( targetFlapAngle, -m_fThrottleControl );

			targetFlapAngle *= sf_maxRotationAngle;
			m_fCurrentFlapAngle = m_fCurrentFlapAngle + ( Clamp( targetFlapAngle - m_fCurrentFlapAngle, -maxFlapRateOfChange * fTimeStep, maxFlapRateOfChange * fTimeStep ) );
			SetComponentRotation( flapBoneId, ROT_AXIS_LOCAL_X, m_fCurrentFlapAngle, true ); 
		}
	}

	return eResult;
}

bool CPlane::WantsToBeAwake()
{
	// TODO - doesn't this run the risk of not being awake if two of the four controls
	// have opposite nonzero values?  Not sure how likely that would be in practice,
	// but fabsf of each element would fix that.
    bool bRet = (m_fThrottleControl + m_fYawControl + m_fPitchControl + m_fRollControl + fabs(m_fCurrentVerticalFlightRatio - m_fDesiredVerticalFlightRatio))!=0.0f
		 || m_landingGear.WantsToBeAwake(this) || m_bIsPropellerDamagedThisFrame;
   
    if( !bRet )
    {
        bRet = m_fDesiredVerticalFlightRatio != m_fCurrentVerticalFlightRatio;
    }

	if(!bRet)
	{
		return CAutomobile::WantsToBeAwake();
	}

	return bRet;
}

dev_float dfLandingGearFriction = 1.0f;
dev_float dfGroundFrictionWithoutLandingGear = 0.6f;
void CPlane::ProcessPreComputeImpacts(phContactIterator impacts)
{
	CAutomobile::ProcessPreComputeImpacts(impacts);

	// Only do propeller processing if engine is on
	impacts.Reset();

	const int numWheels = GetNumWheels();

	Vector3 vCgPosition = RCC_VECTOR3(pHandling->m_vecCentreOfMassOffset);

	MAT34V_TO_MATRIX34(GetMatrix()).Transform(vCgPosition);

	bool bLandingGearIntact = GetIsLandingGearintact();
	bool bLandingGearLockedUp = GetLandingGear().GetPublicState() == CLandingGear::STATE_LOCKED_UP;
	bool bIsInAir = IsInAir();

	while(!impacts.AtEnd())
	{
		CEntity *pOtherEnt = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());
		for(int i = 0; i < m_iNumPropellers; i++)
		{
			if(impacts.IsDisabled())
			{
				// B*1954149: Allow impacts with projectiles to get be processed by the propellers.
				if(!pOtherEnt || (pOtherEnt && !(pOtherEnt->GetIsTypeObject() && pOtherEnt->GetBaseModelInfo() && pOtherEnt->GetBaseModelInfo()->GetModelType() == MI_TYPE_WEAPON)))
					break;
			}
			m_propellerCollision[i].ProcessPreComputeImpacts(this,impacts,Min(m_propellers->GetSpeed(), m_fEngineSpeed));
		}

		GetLandingGear().ProcessPreComputeImpacts(this, impacts);

		if(!impacts.IsDisabled())
		{
			// Those impacts are generate when the contact wheel is fully compressed. We try to reduce amount of bounce up or rotation due to those impacts, also plane should loss some speed.
			for(int i = 0; i < numWheels; ++i)
			{     
				CWheel *wheel = m_ppWheels[i];
				for(int k = 0; k < MAX_WHEEL_BOUNDS_PER_WHEEL; k++)
				{
					if(wheel->GetFragChild(k) == impacts.GetMyComponent())
					{
						impacts.SetElasticity(0.0f);
						impacts.SetFriction(Max(dfLandingGearFriction, impacts.GetFriction()));
					}
				}
			}
			phInst* otherInstance = impacts.GetOtherInstance();
			CEntity* pOtherEntity = otherInstance ? (CEntity*)otherInstance->GetUserData() : NULL;

			// increase the body vs ground friction when landing gear is broken off
			if(pOtherEntity && pOtherEntity->GetIsTypeBuilding())
			{
				if( !bLandingGearIntact && !bIsInAir )
				{
					impacts.SetElasticity(0.0f);
					impacts.SetFriction(Max(dfGroundFrictionWithoutLandingGear, impacts.GetFriction()));
				}

				if( bLandingGearLockedUp )
				{
					impacts.SetElasticity(0.0f);
					impacts.SetDepth(0.0f);
				}

				if(pOtherEntity->GetArchetype() && (pOtherEntity->GetModelId() == MI_GROUND_LIGHT_RED || pOtherEntity->GetModelId() == MI_GROUND_LIGHT_GREEN || pOtherEntity->GetModelId() == MI_GROUND_LIGHT_YELLOW))
				{
					impacts.DisableImpact();
				}
			}
		}

		impacts++;
	}


	if(IsLargePlane() && (GetStatus() == STATUS_WRECKED || (m_aircraftDamage.HasSectionBrokenOff(this,CAircraftDamage::WING_L) && m_aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_R))))
	{
		impacts.Reset();
		while(!impacts.AtEnd())
		{
			if(!impacts.IsDisabled())
			{
				phInst* otherInstance = impacts.GetOtherInstance();
				CEntity* pOtherEntity = otherInstance ? (CEntity*)otherInstance->GetUserData() : NULL;
				// Disable push for large wreck plane colliding against static bounds
				if(pOtherEntity && pOtherEntity->GetIsTypeBuilding())
				{
					impacts.SetDepth(0.0f);
				}
			}
			impacts++;
		}
	}
}

//
//
//
//
static dev_float sfRudderAdjustAngle = ( DtoR * 10.0f);
static dev_float sfAileronAdjustAngle = ( DtoR * 30.0f);
static dev_float sfElevatorAdjustAngle = ( DtoR * 20.0f);
static dev_float sfElevatorAdjustAngleForVTOL = ( DtoR * 10.0f);
static dev_float sfAirbrakeAdjustAngle = ( DtoR * 30.0f);

ePrerenderStatus CPlane::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	// Spin the rotors
	for(int i =0; i < m_iNumPropellers; i++)
	{
		m_propellers[i].PreRender(this);
	}
	
	if(GetBoneIndex(VEH_HBGRIP_L) >= 0.0f || GetBoneIndex(VEH_HBGRIP_R) >= 0.0f)
	{
		TUNE_GROUP_FLOAT(JOYSTICK_IK, PlaneJoyStickRotX, 10.0f, 0.0f, 20.0f, 0.001f);
		TUNE_GROUP_FLOAT(JOYSTICK_IK, PlaneJoyStickRotY, 6.5f, 0.0f, 20.0f, 0.001f);
		float fJoystickAdjustAngleX = ( DtoR * PlaneJoyStickRotX);
		float fJoystickAdjustAngleY = ( DtoR * PlaneJoyStickRotY);

		SetComponentRotation(VEH_CAR_STEERING_WHEEL, ROT_AXIS_LOCAL_X, m_fJoystickPitch * fJoystickAdjustAngleX, true);
		SetComponentRotation(VEH_CAR_STEERING_WHEEL, ROT_AXIS_LOCAL_Y, m_fJoystickRoll * fJoystickAdjustAngleY, false);
	}

#if 0
	static dev_bool bRenderWheelMatrices = false;

	if(bRenderWheelMatrices)
	{
		for(int i = 0 ; i < GetNumWheels() ; i++)
		{
			if(GetWheel(i)->GetHierarchyId() > VEH_INVALID_ID)
			{
				int iBoneIndex = GetBoneIndex(GetWheel(i)->GetHierarchyId());
				if(iBoneIndex > -1)
				{
					Matrix34 matDraw = GetLocalMtx(iBoneIndex);
					const crBoneData* pParent = GetSkeletonData().GetBoneData(iBoneIndex)->GetParent();
					while(pParent)
					{
						int iParentBoneIndex = pParent->GetIndex();
						matDraw.Dot(GetLocalMtx(iParentBoneIndex));
						pParent = GetSkeletonData().GetBoneData(iParentBoneIndex)->GetParent();
					}
					matDraw.Dot(GetMatrix());
					grcDebugDraw::Axis(matDraw,0.3f);

					// Also draw global matrix
					static dev_bool bDrawGlobal = false;
					if(bDrawGlobal)
					{
						grcDebugDraw::Sphere(GetGlobalMtx(iBoneIndex).d,0.3f,Color_green,false);
					}
				}
			}
		}
	}
#endif

#if GTA_REPLAY
	if (!CReplayMgr::IsEditModeActive())
#endif
	{
		if( !IsAnimated() )
		{
			// Elevators (pitch)
			SetComponentRotation(PLANE_ELEVATOR_L,ROT_AXIS_LOCAL_X,-m_fPitch*sfElevatorAdjustAngle,true);
			SetComponentRotation(PLANE_ELEVATOR_R,ROT_AXIS_LOCAL_X,-m_fPitch*sfElevatorAdjustAngle,true);

			// Nozzle
			if(GetVerticalFlightModeAvaliable())
			{
				if( ( !MI_PLANE_TULA.IsValid() ||
					  GetModelIndex() != MI_PLANE_TULA ) &&
					!GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE ) )
				{
					SetComponentRotation(PLANE_NOZZLE_F,ROT_AXIS_LOCAL_X,(1.0f - m_fCurrentVerticalFlightRatio)*sfNozzleAdjustAngle,true);
					SetComponentRotation(PLANE_NOZZLE_R,ROT_AXIS_LOCAL_X,(1.0f - m_fCurrentVerticalFlightRatio)*sfNozzleAdjustAngle,true);

					// Adjust rudder (yaw)
					SetComponentRotation(PLANE_RUDDER,ROT_AXIS_LOCAL_Z,m_fYaw*sfRudderAdjustAngleForVTOL,true);
					SetComponentRotation(PLANE_RUDDER_2,ROT_AXIS_LOCAL_Z,m_fYaw*sfRudderAdjustAngleForVTOL,true);
				}
				else
				{
					// Adjust rudder (yaw)
					SetComponentRotation(PLANE_RUDDER,ROT_AXIS_LOCAL_Z,m_fYaw*sfRudderAdjustAngleForTULA,true);
					SetComponentRotation(PLANE_RUDDER_2,ROT_AXIS_LOCAL_Z,m_fYaw*sfRudderAdjustAngleForTULA,true);
				}
			
				// Elevators (pitch)
				SetComponentRotation(PLANE_ELEVATOR_L,ROT_AXIS_LOCAL_X,-m_fPitch*sfElevatorAdjustAngleForVTOL,true);
				SetComponentRotation(PLANE_ELEVATOR_R,ROT_AXIS_LOCAL_X,-m_fPitch*sfElevatorAdjustAngleForVTOL,true);

			}
			else
			{
				// Elevators (pitch)
				SetComponentRotation(PLANE_ELEVATOR_L,ROT_AXIS_LOCAL_X,-m_fPitch*sfElevatorAdjustAngle,true);
				SetComponentRotation(PLANE_ELEVATOR_R,ROT_AXIS_LOCAL_X,-m_fPitch*sfElevatorAdjustAngle,true);

				// Adjust rudder (yaw)
				SetComponentRotation(PLANE_RUDDER,ROT_AXIS_LOCAL_Z,m_fYaw*sfRudderAdjustAngle,true);
				SetComponentRotation(PLANE_RUDDER_2,ROT_AXIS_LOCAL_Z,m_fYaw*sfRudderAdjustAngle,true);
			}

			// Ailerons (roll)
			if(m_bDisableLeftAileron)
			{
				SetComponentRotation(PLANE_AILERON_L,ROT_AXIS_LOCAL_X,0.0f*sfAileronAdjustAngle,true);
			}
			else
			{
				SetComponentRotation(PLANE_AILERON_L,ROT_AXIS_LOCAL_X,m_fRoll*sfAileronAdjustAngle,true);
			}

			if(m_bDisableRightAileron)
			{
				SetComponentRotation(PLANE_AILERON_R,ROT_AXIS_LOCAL_X,0.0f*sfAileronAdjustAngle,true);
			}
			else
			{
				SetComponentRotation(PLANE_AILERON_R,ROT_AXIS_LOCAL_X,-m_fRoll*sfAileronAdjustAngle,true);
			}

			// Air brakes
			SetComponentRotation(PLANE_AIRBRAKE_L,ROT_AXIS_LOCAL_X,-m_fBrake*sfAirbrakeAdjustAngle,true);
			SetComponentRotation(PLANE_AIRBRAKE_R,ROT_AXIS_LOCAL_X,-m_fBrake*sfAirbrakeAdjustAngle,true);
		}
		
	}

	// vfx
	if (!IsDummy())
	{
		if ((m_nVehicleFlags.bEngineOn || IsRunningCarRecording()) && 
			!m_nVehicleFlags.bIsDrowning && 
			!m_nVehicleFlags.bDisableParticles)
		{
			// GTAV - FIX B*1739284 - Allow script to force the afterburner on
			if( m_nVehicleFlags.bForceAfterburnerVFX )
			{
				float prevThrottle = GetThrottle();
				SetThrottle( 1.0f );
				for (int i = 0; i < PLANE_NUM_AFTERBURNERS; ++i)
				{
					g_vfxVehicle.UpdatePtFxPlaneAfterburner(this, eHierarchyId(PLANE_AFTERBURNER + i), i);
				}
				SetThrottle( prevThrottle );
			}
			else
			{
				for (int i = 0; i < PLANE_NUM_AFTERBURNERS; ++i)
				{
					g_vfxVehicle.UpdatePtFxPlaneAfterburner(this, eHierarchyId(PLANE_AFTERBURNER + i), i);
				}
			}


			g_vfxVehicle.UpdatePtFxPlaneWingTips(this);

			m_aircraftDamage.ProcessVfx(this);
		}

#if __BANK && DEBUG_DRAW
		m_aircraftDamage.DebugDraw(this);
#endif
	}

	ePrerenderStatus result = CAutomobile::PreRender(bIsVisibleInMainViewport);

	return result;
}

void CPlane::DoEmergencyLightEffects(crSkeleton *pSkeleton, s32 boneId, const ConfigPlaneEmergencyLightsSettings &lightParam, float distFade, float rotAmount)
{
	CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();
	const s32 boneIdx = pModelInfo->GetBoneIndex(boneId);

	if (boneIdx > -1)
	{
		Matrix34 &localMtx = GetLocalMtxNonConst(boneIdx);
		localMtx.MakeRotateZ(rotAmount);

		Matrix34 worldMtx;
		pSkeleton->PartialUpdate(boneIdx);
		pSkeleton->GetGlobalMtx(boneIdx, RC_MAT34V(worldMtx));

		fwInteriorLocation interiorLocation = this->GetInteriorLocation();

		CLightSource* pLightSource = CAsyncLightOcclusionMgr::AddLight();
		if (pLightSource)
		{
			pLightSource->Reset();
			pLightSource->SetCommon(
				LIGHT_TYPE_SPOT, 
				LIGHTFLAG_VEHICLE | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS, 
				worldMtx.d, 
				VEC3V_TO_VECTOR3(lightParam.color),
				lightParam.intensity * distFade, 
				LIGHT_ALWAYS_ON);

			pLightSource->SetShadowFadeDistance(20);
			pLightSource->SetDirTangent(worldMtx.b, worldMtx.a);
			pLightSource->SetRadius(lightParam.falloff);
			pLightSource->SetFalloffExponent(lightParam.falloffExponent);
			pLightSource->SetSpotlight(lightParam.innerAngle, lightParam.outerAngle);
			pLightSource->SetInInterior(interiorLocation);
			pLightSource->SetShadowTrackingId(fwIdKeyGenerator::Get(this) + boneIdx);

			// NOTE: we don't want to call Lights::AddSceneLight directly - the AsyncLightOcclusionMgr will handle that for us
		}
	}
}

void CPlane::DoControlPanelLightEffects(s32 boneId, const ConfigPlaneControlPanelSettings &lightParam, float distFade)
{
	CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();
	const s32 boneIdx = pModelInfo->GetBoneIndex(boneId);

	if (boneIdx > -1 && (boneIdx < GetSkeleton()->GetBoneCount()))
	{
		Matrix34 worldMtx;
		GetGlobalMtx(boneIdx, worldMtx);

		fwInteriorLocation interiorLocation = this->GetInteriorLocation();

		CLightSource light(
			LIGHT_TYPE_POINT, 
			LIGHTFLAG_VEHICLE | LIGHTFLAG_NO_SPECULAR, 
			worldMtx.d, 
			VEC3V_TO_VECTOR3(lightParam.color),
			lightParam.intensity * distFade, 
			LIGHT_ALWAYS_ON);
		light.SetDirTangent(worldMtx.b, worldMtx.a);
		light.SetRadius(lightParam.falloff);
		light.SetInInterior(interiorLocation);
		light.SetFalloffExponent(lightParam.falloffExponent);

		Lights::AddSceneLight(light);
	}
}

void CPlane::DoInsideHullLightEffects(s32 boneId, const ConfigPlaneInsideHullSettings &lightParam, float distFade)
{
	CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();
	const s32 boneIdx = pModelInfo->GetBoneIndex(boneId);

	if (boneIdx > -1)
	{
		Matrix34 worldMtx;
		GetGlobalMtx(boneIdx, worldMtx);

		fwInteriorLocation interiorLocation = this->GetInteriorLocation();

		CLightSource light(
			LIGHT_TYPE_POINT, 
			LIGHTFLAG_VEHICLE | LIGHTFLAG_NO_SPECULAR, 
			worldMtx.d, 
			VEC3V_TO_VECTOR3(lightParam.color),
			lightParam.intensity * distFade, 
			LIGHT_ALWAYS_ON);
		light.SetDirTangent(worldMtx.b, worldMtx.a);
		light.SetRadius(lightParam.falloff);
		light.SetFalloffExponent(lightParam.falloffExponent);
		light.SetSpotlight(lightParam.innerAngle, lightParam.outerAngle);
		light.SetInInterior(interiorLocation);

		Lights::AddSceneLight(light);
	}
}

void CPlane::CacheWindowBones()
{
	const crSkeletonData* pSkelData = GetVehicleModelInfo()->GetFragType()->GetCommonDrawable()->GetSkeletonData();
	if (pSkelData)
	{
		u32 index = 0;

		const char * boneNames[] = {	
			"window_lf",
			"window_lf1",
			"window_lf2",
			"window_lf3",
			"window_rf",
			"window_rf1",
			"window_rf2",
			"window_rf3",
			"window_lr",
			"window_lr1",
			"window_lr2",
			"window_lr3",
			"window_rr",
			"window_rr1",
			"window_rr2",
			"window_rr3"
		};

		for (u32 i = 0; i < NELEM(boneNames); i++)
		{
			const crBoneData* boneData = pSkelData->FindBoneData(boneNames[i]);
			if (boneData)
			{
				m_windowBoneIndices[index] = (s16)boneData->GetIndex();
				index++;
			}
		}
	}

	m_windowBoneCached = true;
}

static dev_float	PlaneLights_ExteriorFadeStart = 0.0f;
static dev_float	PlaneLights_ExteriorFadeEnd = 1000.0f;

static dev_float	PlaneLights_InteriorFadeDistance = 5.0f;
static dev_float	PlaneLights_InteriorFadeCutoff = 50.0f;

BANK_ONLY(bank_float bfPlaneTurbulenceOn = true;)

void CPlane::PreRender2(const bool bIsVisibleInMainViewport)
{
#if GTA_REPLAY
	if (!CReplayMgr::IsEditModeActive())
#endif
	{
        if( !IsAnimated() )
        {
		    // Update the langing gear aux door after CVehicle::PreRender(), so the bone matrix won't be overwritten 
		    // by syncing to articulated body. We'll do this in PreRender2(), to give the animation the most time 
		    // to be done, as the landing gear PreRender() will access bones causing waits on the animation.
		    m_landingGear.PreRenderDoors(this);
        }
	}

	// The engine state seems to be dependent on the button (holding L2 will switch engine off even in air)
	// So employ similar trick used by trains. If plane is occupied and light status is true (dependent on TOD)
	// then force lights on
	m_OverrideLights = GetVehicleLightsStatus() && GetDriver() ? FORCE_CAR_LIGHTS_ON : NO_CAR_LIGHT_OVERRIDE;

	CAutomobile::PreRender2(bIsVisibleInMainViewport);

	if (m_nVehicleFlags.bEngineOn || IsRunningCarRecording())
	{
		// downwash vfx
		if (GetVerticalFlightModeAvaliable())
		{
			g_vfxVehicle.ProcessPlaneDownwash(this);
		}

		// ground disturbance vfx
		g_vfxVehicle.ProcessGroundDisturb(this);
	}
	
	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	const Vector3& camPos = camInterface::GetPos();
	const float dist = camPos.Dist(vThisPosition);

	float exteriorFadeDist = Saturate((dist - PlaneLights_ExteriorFadeStart) / (PlaneLights_ExteriorFadeEnd - PlaneLights_ExteriorFadeStart));
	float interiorFadeDist = 1.0f - rage::Saturate((dist - PlaneLights_InteriorFadeCutoff) / PlaneLights_InteriorFadeDistance);

	const bool bVehicleIsLuxe2 = (		GetVehicleModelInfo()->GetModelNameHash() == MI_PLANE_LUXURY_JET3.GetName().GetHash()
									||	GetVehicleModelInfo()->GetModelNameHash() == MI_PLANE_NIMBUS.GetName().GetHash());

	// Disable emissives when wrecked
	if (GetStatus() == STATUS_WRECKED && bVehicleIsLuxe2) 
	{
		if(GetBoneIndex(VEH_EMISSIVES) != -1)
		{
			GetLocalMtxNonConst(GetBoneIndex(VEH_EMISSIVES)).Zero3x3(); 
		}
	}

	// Special luxor lights
	if ((GetStatus() != STATUS_WRECKED) && bVehicleIsLuxe2)
	{
		extern ConfigLightSettings g_PlaneLuxe2Cabin;
		extern ConfigLightSettings g_PlaneLuxe2CabinStrip;
		extern ConfigLightSettings g_PlaneLuxe2CabinTV;
		extern ConfigLightSettings g_PlaneLuxe2CabinLOD;
		extern ConfigLightSettings g_PlaneLuxe2CabinWindow;

		CVehicleModelInfo *pModelInfo = GetVehicleModelInfo();
		bool enteringInsideExitingAVehicle = IsEnteringInsideOrExiting();
		const bool enableHD = (enteringInsideExitingAVehicle && camInterface::IsRenderingFirstPersonCamera());

		if ((camInterface::IsRenderedCameraInsideVehicle() || enableHD) BANK_ONLY( || camInterface::GetDebugDirector().IsFreeCamActive()))
		{
			for (u32 l = VEH_SIREN_7; l <= VEH_SIREN_9; l++ )
			{
				AddLight(LIGHT_TYPE_SPOT, pModelInfo->GetBoneIndex(l), g_PlaneLuxe2Cabin, interiorFadeDist);
			}

			for (u32 l = VEH_SIREN_10; l <= VEH_SIREN_11; l++ )
			{
				AddLight(LIGHT_TYPE_CAPSULE, pModelInfo->GetBoneIndex(l), g_PlaneLuxe2CabinStrip, interiorFadeDist);
			}

			for (u32 l = VEH_SIREN_13; l <= VEH_SIREN_15; l++ )
			{
				AddLight(LIGHT_TYPE_SPOT, pModelInfo->GetBoneIndex(l), g_PlaneLuxe2CabinTV, interiorFadeDist);
			}

			if (!m_windowBoneCached)
			{
				CacheWindowBones();
			}

			for (u32 l = 0; l < NELEM(m_windowBoneIndices); l++ )
			{
				AddLight(LIGHT_TYPE_SPOT, m_windowBoneIndices[l], g_PlaneLuxe2CabinWindow, interiorFadeDist);
			}

		}
		else
		{
			AddLight(LIGHT_TYPE_CAPSULE, pModelInfo->GetBoneIndex(VEH_SIREN_12), g_PlaneLuxe2CabinLOD, interiorFadeDist);
			AddLight(LIGHT_TYPE_SPOT, pModelInfo->GetBoneIndex(VEH_SIREN_9), g_PlaneLuxe2Cabin, interiorFadeDist);
		}
	}

	// lights
	if( (GetStatus() != STATUS_WRECKED) && (m_nVehicleFlags.bEngineOn || (GetDriver() && (m_bIsEngineTurnedOffByPlayer || m_bIsEngineTurnedOffByScript))) )
	{
#if RSG_PC
		int frameCount = (int)((float)fwTimer::GetTimeInMilliseconds_ScaledNonClipped()/(1000.0f/30.0f));
#else
		int frameCount = fwTimer::GetFrameCount_ScaledNonClipped();
#endif
		bool blinkLight = ((frameCount + GetRandomSeed()) & 31) <= 0;
		bool blinkLight2 = ((frameCount + GetRandomSeed() + 12) & 31) <= 0;
		bool blinkLight3 = ((frameCount + GetRandomSeed() + 15) & 31) <= 0;
		bool blinkLight4 = ((frameCount + GetRandomSeed() + 24) & 31) <= 0;

		extern ConfigVehiclePositionLightSettings	g_PlanePosLights;
		extern ConfigVehicleWhiteLightSettings		g_PlaneWhiteHeadLights;
		extern ConfigVehicleWhiteLightSettings		g_PlaneWhiteTailLights;

		const bool bVehicleIsSwift2 = (GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_SWIFT2.GetName().GetHash() 
									|| GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_SUPER_VOLITO.GetName().GetHash()
									|| GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_SUPER_VOLITO2.GetName().GetHash()
									|| GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_VOLATUS.GetName().GetHash());
		const bool bVehicleIsCargoPlane = (GetVehicleModelInfo()->GetModelNameHash() == MI_PLANE_CARGOPLANE.GetName().GetHash());
		
		bool addBlinkLights = true;
		if (bVehicleIsSwift2) { addBlinkLights = !IsInsideVehicleModeEnabled();  }

		if (addBlinkLights)
		{
			if (blinkLight)		DoPosLightEffects	(VEH_SIREN_1, g_PlanePosLights, true,false,exteriorFadeDist);
			if (blinkLight2)	DoPosLightEffects	(VEH_SIREN_2, g_PlanePosLights,false,false,exteriorFadeDist);

			if (blinkLight3)	DoWhiteLightEffects	(VEH_SIREN_3, g_PlaneWhiteTailLights,false,exteriorFadeDist);
			if (blinkLight4)	DoWhiteLightEffects	(VEH_SIREN_4, g_PlaneWhiteHeadLights, true,exteriorFadeDist);
		}

		crSkeleton *pSkeleton = GetSkeleton();
				
		if (m_nVehicleFlags.bInteriorLightOn)
		{
			extern ConfigPlaneControlPanelSettings	  g_PlaneControlPanelLights;

			if (g_PlaneControlPanelLights.intensity > 0.0f)
			{
				for (u32 l = VEH_SIREN_5; l <= VEH_SIREN_6; l++)
				{
					DoControlPanelLightEffects(l, g_PlaneControlPanelLights, interiorFadeDist);
				}
			}

			if (bVehicleIsCargoPlane)
			{
				extern ConfigPlaneEmergencyLightsSettings g_PlaneLeftEmergencyLights;
				extern ConfigPlaneEmergencyLightsSettings g_PlaneRightEmergencyLights;
				extern ConfigPlaneInsideHullSettings	  g_PlaneInsideHullLights;

				if(!fwTimer::IsGamePaused())
				{
					m_fLeftLightRotation = fmod(m_fLeftLightRotation + g_PlaneLeftEmergencyLights.rotation * fwTimer::GetTimeStep(), TWO_PI);
					m_fRightLightRotation = fmod(m_fRightLightRotation + g_PlaneRightEmergencyLights.rotation * fwTimer::GetTimeStep(), TWO_PI);
				}

				if (g_PlaneLeftEmergencyLights.intensity > 0.0f || g_PlaneRightEmergencyLights.intensity > 0.0f)
				{
					for (u32 l = VEH_SIREN_7; l <= VEH_SIREN_12; l++ )
					{
						const bool evenSiren = ((l - VEH_SIREN_7) % 2 == 0);
						DoEmergencyLightEffects(
							pSkeleton, 
							l, 
							(evenSiren) ? g_PlaneLeftEmergencyLights : g_PlaneRightEmergencyLights, 
							interiorFadeDist, 
							(evenSiren) ? m_fLeftLightRotation : m_fRightLightRotation);
					}
				}

				if (g_PlaneInsideHullLights.intensity > 0.0f)
				{
					for (u32 l = VEH_SIREN_13; l <= VEH_SIREN_18; l++)
					{
						DoInsideHullLightEffects(l, g_PlaneInsideHullLights, interiorFadeDist);
					}
				}
			}
		}
	}

	// GTAV - B*1983577 - If any of the landing gear covers have bones but no collision hide them if the plane has been blown up
	if( m_nVehicleFlags.bBlownUp )
	{
		for( int i = 0; i < NUM_LANDING_GEAR_DOORS; i++ )
		{
			int nBoneIndex = GetBoneIndex( (eHierarchyId)( LANDING_GEAR_DOOR_FL + i ) );
			int nComponent = GetFragInst()->GetComponentFromBoneIndex(nBoneIndex);

			if( nComponent == -1 &&
				nBoneIndex > -1 )
			{
				crSkeleton *pSkeleton = GetSkeleton();

				Matrix34 boneMat;
				Matrix34 mat;
				mat.Zero();

				pSkeleton->GetGlobalMtx( nBoneIndex, RC_MAT34V(boneMat) );
				mat.d = boneMat.d;
				pSkeleton->SetGlobalMtx( nBoneIndex, RCC_MAT34V(mat) );
			}
		}
	}
}

bool CPlane::IsTurbulenceOn()
{
	return IsInAir() && m_fTurbulenceRecoverTimer <= 0.0f && m_fTurbulenceTimer > 0.0f;
}

bool CPlane::GetCanMakeIntoDummy(eVehicleDummyMode dummyMode)
{
	if ( dummyMode != VDM_SUPERDUMMY )
	{
		return CAutomobile::GetCanMakeIntoDummy(dummyMode);
	}
	return false;
}

bool CPlane::TryToMakeIntoDummy(eVehicleDummyMode dummyMode, bool bSkipClearanceTest)
{
	if ( dummyMode != VDM_SUPERDUMMY )
	{
		return CAutomobile::TryToMakeIntoDummy(dummyMode, bSkipClearanceTest);
	}
	return false;
}

static dev_float TurbulenceMaxFrequency = 2.0f;
static dev_float TurbulenceMinFrequency = 0.5f;
static dev_float TurbulenceBlendingRate = 2.0f;
static dev_float TurbulenceNoiseManitude = 0.9f;

float TurbulenceSideWindThreshold = 3.0f;
static float sfBuffetingBodyHealthThreshold = 950.0f;

bank_float bfTurbulenceTimeMin = 5.0f;
bank_float bfTurbulenceTimeMax = 10.0f;
bank_float bfTurbulenceRecoverTimeMin = 10.0f;
bank_float bfTurbulenceRecoverTimeMax = 20.0f;
bank_float sfPlaneTurbulencePedVibMult = 0.025f;
bank_float sfPlaneTurbulencePedVibMin = 0.005f;

dev_float dfTurbulenceRollFrequencyMulti = 2.0f;

void CPlane::ProcessTurbulence(float fTimeStep)
{
	bool bResetAndQuit = false;

	float fWeatherMult = WeatherTurbulenceMult();

	CControl *pControl = NULL;
	if(GetStatus()==STATUS_PLAYER && GetDriver() && GetDriver()->IsPlayer())
	{
		pControl = GetDriver()->GetControlFromPlayer();
	}
	bool bControlInactive = pControl && CTaskVehiclePlayerDrive::IsThePlayerControlInactive(pControl);

	if(GetDriver() && GetDriver()->IsPlayer() && !bControlInactive BANK_ONLY(&& bfPlaneTurbulenceOn)  && GetAircraftDamage().GetLiftMult(this) > 0.0f)
	{
		if(m_fTurbulenceRecoverTimer <= 0.0f)
		{
			if(m_fTurbulenceTimer > 0.0f)
			{
				m_fTurbulenceTimer -= fTimeStep;
			}
			// Recompute turbulence time
			else
			{
				// Recompute next turbulence values
				m_fTurbulenceTimer = fwRandom::GetRandomNumberInRange(bfTurbulenceTimeMin, bfTurbulenceTimeMax);
				m_fTurbulenceTimer *= fWeatherMult;
				m_fTurbulenceRecoverTimer = fwRandom::GetRandomNumberInRange(bfTurbulenceRecoverTimeMin, bfTurbulenceRecoverTimeMax);
				m_fTurbulenceRecoverTimer *= WeatherTurbulenceMultInv();
			}
		}
		else
		{
			m_fTurbulenceRecoverTimer -= fTimeStep;
		}
	}

	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	CFlyingHandlingData* pVerticleHandlingData = NULL;
	
	if( GetVerticalFlightModeAvaliable() && m_fCurrentVerticalFlightRatio > 0.0f )
	{
		 pVerticleHandlingData = pHandling->GetVerticalFlyingHandlingData();
	}

	if(pFlyingHandling == NULL)
	{
		bResetAndQuit = true;
	}

	if(!IsInAir() || GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		bResetAndQuit = true;
	}

	if(GetStatus() == STATUS_WRECKED || (m_aircraftDamage.HasSectionBrokenOff(this,CAircraftDamage::WING_L) && m_aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_R)))
	{
		bResetAndQuit = true;
	}

	if(bResetAndQuit)
	{
		m_fTurbulenceMagnitude = 0.0f;
		m_fTurbulenceNoise = 0.0f;
		m_fTurbulenceTimeSinceLastCycle = 0.0f;
		m_vTurbulenceAirflow = VEC3_ZERO;
		m_fTurbulenceRecoverTimer = 0.0f;
		m_fTurbulenceTimer = 0.0f;
		return;
	}

	float fTurbulenceMagnitudeMax = pFlyingHandling->m_fTurublenceMagnitudeMax;

	if( pVerticleHandlingData )
	{
		fTurbulenceMagnitudeMax *= ( 1.0f - m_fCurrentVerticalFlightRatio );
		fTurbulenceMagnitudeMax += ( pVerticleHandlingData->m_fTurublenceMagnitudeMax * m_fCurrentVerticalFlightRatio );
	}

	float fTurbulenceT = (m_fTurbulenceMagnitude + m_fTurbulenceNoise * m_fTurbulenceMagnitude * TurbulenceNoiseManitude) / fTurbulenceMagnitudeMax;
	
	fTurbulenceT = Clamp(fTurbulenceT, 0.0f, 1.0f);

	float fFrequency = TurbulenceMinFrequency + fTurbulenceT * (TurbulenceMaxFrequency - TurbulenceMinFrequency);
	float fHalfCycle = 0.5f / fFrequency;
	float fFullCycle = fHalfCycle * 2.0f;

	m_fTurbulenceTimeSinceLastCycle += fTimeStep;
	bool bHalfCycleJustCompleted = (m_fTurbulenceTimeSinceLastCycle - fTimeStep) < fHalfCycle && m_fTurbulenceTimeSinceLastCycle >= fHalfCycle;
	bool bFullCycleJustCOmpleted = m_fTurbulenceTimeSinceLastCycle > fFullCycle;

	// If turbulence time just starts and the old turbulence had blended out, then restart new turbulence right away
	if(IsTurbulenceOn() && m_fTurbulenceMagnitude == 0.0f)
	{
		bFullCycleJustCOmpleted = true;
	}

	if(bFullCycleJustCOmpleted)
	{
		Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);

		WIND.GetLocalVelocity(GetTransform().GetPosition(), RC_VEC3V(vecWindSpeed), false, false);
		vecWindSpeed *= ComputeWindMult(m_fCurrentVerticalFlightRatio);
		vecWindSpeed += m_vTurbulenceAirflow;

		if(IsTurbulenceOn())
		{
			m_fTurbulenceMagnitude = fTurbulenceMagnitudeMax * fWeatherMult  * m_fScriptTurbulenceMult;
		}
		else // Fade out the turbulence
		{
			float fTurbulenceMagDelta = -m_fTurbulenceMagnitude;
			fTurbulenceMagDelta = Clamp(fTurbulenceMagDelta, -TurbulenceBlendingRate * m_fTurbulenceTimeSinceLastCycle, TurbulenceBlendingRate * m_fTurbulenceTimeSinceLastCycle);
			m_fTurbulenceMagnitude += fTurbulenceMagDelta;
		}

		m_fTurbulenceNoise = fwRandom::GetRandomNumberInRange(-1.0f, 0.0f);
		fTurbulenceT = (m_fTurbulenceMagnitude + m_fTurbulenceNoise * m_fTurbulenceMagnitude * TurbulenceNoiseManitude) / fTurbulenceMagnitudeMax;
		fTurbulenceT = Clamp(fTurbulenceT, 0.0f, 1.0f);
		fFrequency = TurbulenceMinFrequency + fTurbulenceT * (TurbulenceMaxFrequency - TurbulenceMinFrequency);

		m_fTurbulenceTimeSinceLastCycle = 0.0f;

		if(GetStatus()==STATUS_PLAYER && GetDriver() && GetDriver()->IsLocalPlayer())
		{
			// Shake the controller pad for first half cycle
			float fPedShakeIntensity = fTurbulenceT * sfPlaneTurbulencePedVibMult;
			if(fPedShakeIntensity >= sfPlaneTurbulencePedVibMin)
			{
				float fHalfCycle = 0.5f / fFrequency;
				u32 fPedShakeTime = (u32)(fHalfCycle * 1000.0f);
				CControlMgr::StartPlayerPadShakeByIntensity(fPedShakeTime, fPedShakeIntensity);
			}
		}
	}
	else if(bHalfCycleJustCompleted)
	{

		m_fTurbulenceNoise = fwRandom::GetRandomNumberInRange(-1.0f, 0.0f);
		fTurbulenceT = (m_fTurbulenceMagnitude + m_fTurbulenceNoise * m_fTurbulenceMagnitude * TurbulenceNoiseManitude) / fTurbulenceMagnitudeMax;
		fTurbulenceT = Clamp(fTurbulenceT, 0.0f, 1.0f);
		fFrequency = TurbulenceMinFrequency + fTurbulenceT * (TurbulenceMaxFrequency - TurbulenceMinFrequency);
		float timeElapsedSinceHalfCycle = m_fTurbulenceTimeSinceLastCycle - fHalfCycle;
		m_fTurbulenceTimeSinceLastCycle = 0.5f / fFrequency + timeElapsedSinceHalfCycle;

		if(GetStatus()==STATUS_PLAYER && GetDriver() && GetDriver()->IsLocalPlayer())
		{
			// Shake the controller pad for second half cycle
			float fPedShakeIntensity = fTurbulenceT * sfPlaneTurbulencePedVibMult;
			if(fPedShakeIntensity >= sfPlaneTurbulencePedVibMin)
			{
				float fHalfCycle = 0.5f / fFrequency;
				u32 fPedShakeTime = (u32)(fHalfCycle * 1000.0f);
				CControlMgr::StartPlayerPadShakeByIntensity(fPedShakeTime, fPedShakeIntensity);
			}
		}
	}

	// Quit if the turbulence is completely blended out
	if(m_fTurbulenceMagnitude == 0.0f)
	{
		m_vTurbulenceAirflow = VEC3_ZERO;
		return;
	}

	float fSin = rage::Sinf(fFrequency * m_fTurbulenceTimeSinceLastCycle * 2.0f * PI);
	Matrix34 matPlane;
	GetMatrixCopy(matPlane);
	matPlane.RotateLocalY(m_fTurbulenceNoise * HALF_PI + HALF_PI * 0.5f);
	m_vTurbulenceAirflow = matPlane.c * (fTurbulenceT * fTurbulenceMagnitudeMax * fSin);

	// Apply turbulence

	float fTimeStepInv = 1.0f / fTimeStep;
	Vector3 vTurbulenceAcceleration = m_vTurbulenceAirflow * fTimeStepInv;

	float fTurbulenceForceMulti = pFlyingHandling->m_fTurublenceForceMulti;
	if( pVerticleHandlingData )
	{
		fTurbulenceForceMulti *= ( 1.0f - m_fCurrentVerticalFlightRatio );
		fTurbulenceForceMulti += ( pVerticleHandlingData->m_fTurublenceForceMulti * m_fCurrentVerticalFlightRatio );
	}
	ApplyInternalForceCg( vTurbulenceAcceleration * GetMass() * fTurbulenceForceMulti );

	float fSinRolling = rage::Sinf(fFrequency * m_fTurbulenceTimeSinceLastCycle * 2.0f * dfTurbulenceRollFrequencyMulti * PI);

	Vector3 vecAngInertia = GetAngInertia();
	float fTurbulenceImpulseDistributeBias = fwRandom::GetRandomNumberInRange(0.0f, 0.5f) * fSinRolling;
	float fTurbulenceRollTorqueMulti = pFlyingHandling->m_fTurublenceRollTorqueMulti;

	if( pVerticleHandlingData )
	{
		fTurbulenceRollTorqueMulti *= ( 1.0f - m_fCurrentVerticalFlightRatio );
		fTurbulenceRollTorqueMulti += ( m_fCurrentVerticalFlightRatio * pVerticleHandlingData->m_fTurublenceRollTorqueMulti );
	}

	ApplyInternalTorque(vTurbulenceAcceleration * (0.5f + fTurbulenceImpulseDistributeBias) * vecAngInertia.y * fTurbulenceRollTorqueMulti, VEC3V_TO_VECTOR3(GetTransform().GetA()));
	ApplyInternalTorque(vTurbulenceAcceleration * (0.5f - fTurbulenceImpulseDistributeBias) * vecAngInertia.y * fTurbulenceRollTorqueMulti, VEC3V_TO_VECTOR3(GetTransform().GetA()) * -1.0f);

	if(vTurbulenceAcceleration.z > 0.0f)
	{
		float fTurbulencePitchTorqueMulti = pFlyingHandling->m_fTurublencePitchTorqueMulti;

		if( pVerticleHandlingData )
		{
			fTurbulencePitchTorqueMulti *= ( 1.0f - m_fCurrentVerticalFlightRatio );
			fTurbulencePitchTorqueMulti += ( m_fCurrentVerticalFlightRatio * pVerticleHandlingData->m_fTurublencePitchTorqueMulti );
		}

		ApplyInternalTorque(vTurbulenceAcceleration * fTurbulencePitchTorqueMulti * vecAngInertia.x, VEC3V_TO_VECTOR3(GetTransform().GetB()));
	}

#if 0
	{
		Vector3 vecVelocityToDraw = GetVelocity();
		if(vecVelocityToDraw.Mag2() > 25.0f)
		{
			vecVelocityToDraw.Normalize();
			vecVelocityToDraw *= 5.0f;
		}
		grcDebugDraw::Arrow(GetMatrix().d() + GetMatrix().c(), GetMatrix().d() + GetMatrix().c() + VECTOR3_TO_VEC3V(vecVelocityToDraw), 0.1f, Color_green);

		Vector3 vTurbulenceToDraw = m_vTurbulenceAirflow;
		grcDebugDraw::Arrow(GetMatrix().d() + GetMatrix().a(), GetMatrix().d() + GetMatrix().a() + VECTOR3_TO_VEC3V(vTurbulenceToDraw * (0.5f + fTurbulenceImpulseDistributeBias) ), 0.1f, Color_red);
		grcDebugDraw::Arrow(GetMatrix().d() - GetMatrix().a(), GetMatrix().d() - GetMatrix().a() + VECTOR3_TO_VEC3V(vTurbulenceToDraw * (0.5f - fTurbulenceImpulseDistributeBias) ), 0.1f, Color_red);
		if(vTurbulenceForce.z > 0.0f)
		{
			grcDebugDraw::Arrow(GetMatrix().d() + GetMatrix().b(), GetMatrix().d() + GetMatrix().b() + VECTOR3_TO_VEC3V(vTurbulenceToDraw), 0.1f, Color_red);
		}
	}
#endif
}


int CPlane::InitPhys()
{
	CAutomobile::InitPhys();

	m_landingGear.InitPhys(this, LANDING_GEAR_F, LANDING_GEAR_RM1, LANDING_GEAR_LM1, LANDING_GEAR_RR, LANDING_GEAR_RL, LANDING_GEAR_RM);
#if __WIN32PC
	m_fWingForce = 0.0f;
#endif // __WIN32PC
	return INIT_OK;
}

void CPlane::InitWheels()
{
	CAutomobile::InitWheels();

	float fTotalStaticForce = 0.0f;
	for(int i = 0; i < GetNumWheels(); i++)
	{
		Assert(GetWheel(i));
		GetWheel(i)->GetConfigFlags().SetFlag(WCF_UPDATE_SUSPENSION);
		fTotalStaticForce += GetWheel(i)->GetStaticForce();
	}

	// Scale the wheels' static forces to match the gravity (1.0f)
	if(Abs(fTotalStaticForce - 1.0f) > SMALL_FLOAT)
	{
		float fStaticForceScale = 1.0f / fTotalStaticForce;
		for(int i = 0; i < GetNumWheels(); i++)
		{
			GetWheel(i)->SetStaticForce(GetWheel(i)->GetStaticForce() * fStaticForceScale);
		}
	}

	m_landingGear.InitWheels(this);
}


void CPlane::InitCompositeBound()
{
	CAutomobile::InitCompositeBound();

	// Turn collision on for landing gear
	m_landingGear.InitCompositeBound(this);

	// Turn on collision for all breakable part children
	m_aircraftDamage.InitCompositeBound(this);
	m_landingGearDamage.InitCompositeBound(this);

	if( sbEnablePropellerMods &&
		GetModelIndex() == MI_PLANE_MICROLIGHT )
	{
		for( int i = 1; i < m_iNumPropellers; i++ )
		{
			EnableIndividualPropeller( i, false );
		}
	}

	UpdatePropellerCollision();

	for(int i = 0; i < GetNumDoors(); i++)
	{
		CCarDoor* pDoor = GetDoor(i);
		eHierarchyId nDoorId = pDoor->GetHierarchyId();
		eHierarchyId nWindowId = GetWindowIdFromDoor(nDoorId);
		UpdateWindowBound(nWindowId);
	}

	if( GetModelIndex() == MI_PLANE_STRIKEFORCE )
	{
		m_nVehicleFlags.bUseDeformation = false;
	}
}

void CPlane::UpdateWindowBound(eHierarchyId eWindowId)
{
	if(eWindowId < 0 || eWindowId >= VEH_NUM_NODES)
	{
		return;
	}

	s32 nWindowBone = GetBoneIndex(eWindowId);
	if(nWindowBone > -1)
	{
		int nGroup = GetVehicleFragInst()->GetGroupFromBoneIndex(nWindowBone);
		if(nGroup > -1)
		{
			const fragPhysicsLOD* physicsLOD = GetVehicleFragInst()->GetTypePhysics();
			fragTypeGroup* pGroup = physicsLOD->GetGroup(nGroup);
			int iChild = pGroup->GetChildFragmentIndex();

			u32 nIncludeFlags = IsBrokenFlagSet(iChild)
				? (ArchetypeFlags::GTA_CAR_DOOR_LATCHED_INCLUDE_TYPES | ArchetypeFlags::GTA_CAMERA_TEST) // turn off the collision if the window is smashed
				: ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;

			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());	
			Assert(pBoundComp);

			for(int k = 0; k < pGroup->GetNumChildren(); k++)
			{
				pBoundComp->SetIncludeFlags(iChild+k, nIncludeFlags);
			}
		}
	}

}

///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : BlowUpCar
// PURPOSE :  Does everything needed to destroy a car
///////////////////////////////////////////////////////////////////////////////////

void CPlane::BlowUpCar( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool bNetCall, u32 weaponHash, bool bDelayedExplosion )
{
	//Use previous damage entity that has actually destroyed the engine or the wings.
	if (GetStatus() != STATUS_WRECKED && !IsNetworkClone() && GetDestroyedByPed())
	{
		if (pCulprit && !pCulprit->GetIsTypePed() && !pCulprit->GetIsTypeVehicle())
		{
			pCulprit = GetWeaponDamageEntity();
		}
	}

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

	/// don't damage if this flag is set (usually during a cutscene)
	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;	
	}

	bool blowUpInstantly = true;
	// set the plane/heli to go out of control if not already
	if (GetStatus()!=STATUS_PLAYER)
	{	
		blowUpInstantly = false;//let the plane crash before blowing up except for the player
	}
	else
	{
		if(GetDriver() && GetDriver()->IsLocalPlayer())	
		{
			audNorthAudioEngine::NotifyLocalPlayerPlaneCrashed();
		}
	}
	
	// set the engine temp super high so we get nice sounds as the chassis cools down
	m_EngineTemperature = MAX_ENGINE_TEMPERATURE;

	// If this is a seaplane, it looks better if it sinks after being blown up.
	if(CSeaPlaneExtension* pSeaPlaneExtension = GetExtension<CSeaPlaneExtension>())
	{
		pSeaPlaneExtension->m_nFlags.bSinksWhenDestroyed = 1;
	}
	
#if !__NO_OUTPUT
	if (NetworkInterface::IsGameInProgress() && !IsNetworkClone() && GetNetworkObject())
	{
		netDebug1("**************************************************************************************************");
		netDebug1("PLANE BLOWN UP: %s", GetNetworkObject()->GetLogName());
		sysStack::PrintStackTrace();
		netDebug1("**************************************************************************************************");
	}
#endif //  !__NO_OUTPUT

	// Everything now goes through a crash task
	// Create a crash task if one doesn't already exist
	if (!IsNetworkClone() && !(blowUpInstantly && bAddExplosion))
	{
		aiTask *pActiveTask = GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_CRASH, VEHICLE_TASK_PRIORITY_CRASH);
		if(pActiveTask)
		{
			// Blow up the plane if it receives consecutive explosion impacts
			if(weaponHash == WEAPONTYPE_EXPLOSION)
			{
				CTaskVehicleCrash *pCrashTask = static_cast<CTaskVehicleCrash *>(pActiveTask);
				pCrashTask->SetCrashFlag(CTaskVehicleCrash::CF_HitByConsecutiveExplosion, true);
			}
		}
		else
		{
			CTaskVehicleCrash *pCarTask = rage_new CTaskVehicleCrash( pCulprit, 0, weaponHash);

			if(pHandling->GetSeaPlaneHandlingData() && GetIsInWater())
			{
				blowUpInstantly = true;
			}
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_BlowUpInstantly, blowUpInstantly);
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_InACutscene, bInACutscene);
			pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_AddExplosion, bAddExplosion);

			GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_CRASH);
			return;
		}
	}
	//Because this task is not run in Clones we need to Finish Blowing Up the Vehicle.
	else
	{
		FinishBlowingUpVehicle(pCulprit, bInACutscene, bAddExplosion, bNetCall, weaponHash, bDelayedExplosion);
	}
}

//////////////////////////////////////////////////////////////////////////
// Damage the vehicle even more, close to these bones during BlowUpCar
//////////////////////////////////////////////////////////////////////////
const int PLANE_BONE_COUNT_TO_DEFORM = 30;
const eHierarchyId ExtraPlaneBones[PLANE_BONE_COUNT_TO_DEFORM] = {
	VEH_CHASSIS, VEH_CHASSIS, PLANE_WING_L, PLANE_WING_R,  //default CPU code path has 4 max impacts, do the most important ones first
	VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS,  
	VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, 
	PLANE_WING_L, PLANE_WING_L, PLANE_WING_L, PLANE_WING_L,
	PLANE_WING_R, PLANE_WING_R, PLANE_WING_R, PLANE_WING_R,
	PLANE_ELEVATOR_L, PLANE_ELEVATOR_R, PLANE_TAIL,
	PLANE_PROP_1, PLANE_PROP_2, PLANE_PROP_3, PLANE_PROP_4, 
	PLANE_WINGTIP_1, PLANE_WINGTIP_2, PLANE_WING_RL, PLANE_WING_RR };

const eHierarchyId* CPlane::GetExtraBonesToDeform(int& extraBoneCount)
{
	extraBoneCount = PLANE_BONE_COUNT_TO_DEFORM;
	return ExtraPlaneBones;
}

//
//
//This should only be called from the crash task
const float PLANE_BLOW_OFF_WING_PROBABILITY = 0.75;
void CPlane::FinishBlowingUpVehicle( CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool bDelayedExplosion )
{
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

	/// don't damage if this flag is set (usually during a cutscene)
	if (m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;	
	}
	
	// we can't blow up cars controlled by another machine
	// but we still have to change their status to wrecked
	// so the car doesn't blow up if we take control of an
	// already blown up car
	if (IsNetworkClone())
	{
		Assertf(bNetCall, "Trying to blow up clone %s", GetNetworkObject()->GetLogName());

		KillPedsInVehicle(pCulprit, weaponHash);
		KillPedsGettingInVehicle(pCulprit);

		SetIsWrecked();

		//Check to see that it is the player
		if (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
		{
			CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
			CCrime::ReportDestroyVehicle(this, static_cast<CPed*>(pCulprit));
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

	SetIsWrecked();

	// increment player stats
	if (( pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer() ) || pCulprit == FindPlayerVehicle())
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->HavocCaused += HAVOC_BLOWUPCAR;
	}

	// remove the plane
	if (GetStatus() == STATUS_PLAYER)
	{
		for (s32 i=0; i<GetSeatManager()->GetMaxSeats(); i++)
		{
			if (GetSeatManager()->GetPedInSeat(i))
			{
				GetSeatManager()->GetPedInSeat(i)->SetPedResetFlag( CPED_RESET_FLAG_DontRenderThisFrame, true );
			}
		}

		SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, false, true );
		DisableCollision();
		SetVelocity(Vector3(0,0,0));
		SetAngVelocity(Vector3(0,0,0));
	}

	SetWeaponDamageInfo(pCulprit, weaponHash, fwTimer::GetTimeInMilliseconds());

	//Set the destruction information.
	SetDestructionInfo(pCulprit, weaponHash);
	
	if(GetModelIndex() != MI_PLANE_JET)
	{
		m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off
	}

	SetHealth(0.0f);			// Make sure this happens before AddExplosion or it will blow up twice

//	Vector3	Temp = vThisPosition;

	KillPedsInVehicle(pCulprit, weaponHash);

	// Switch off the engine. (For sound purposes)
	this->SwitchEngineOff(false);
	this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
	this->m_nVehicleFlags.bLightsOn = FALSE;
	this->TurnSirenOn(FALSE);
	this->m_nAutomobileFlags.bTaxiLight = FALSE;

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this);

		CPed* pInflictorPed = pCulprit->GetIsTypeVehicle() ? static_cast<CVehicle*>(pCulprit)->GetDriver() : static_cast<CPed*>(pCulprit);
		CCrime::ReportDestroyVehicle(this, pInflictorPed);
	}

	if( bAddExplosion )
	{
		AddVehicleExplosion(pCulprit, bInACutscene, bDelayedExplosion);
	}

	// knock bits off the car
	GetVehicleDamage()->BlowUpCarParts(pCulprit, CVehicleDamage::Break_Off_Car_Parts_Pending_Bound_Update);
	// Break lights, windows and sirens
	GetVehicleDamage()->BlowUpVehicleParts(pCulprit);

	if(!GetVehicleDamage()->IsBlowUpCarPartsPending())
	{
		// if we are not postpone the parts breaking, then do it now
		BlowUpPlaneParts();
	}

	g_decalMan.Remove(this);

	CPed* fireCulprit = NULL;
	if (pCulprit && pCulprit->GetIsTypePed())
	{
		fireCulprit = static_cast<CPed*>(pCulprit);
	}
	g_vfxVehicle.ProcessWreckedFires(this, fireCulprit, FIRE_DEFAULT_NUM_GENERATIONS);

	m_aircraftDamage.BlowingUpVehicle(this);
}

void CPlane::PostBoundDeformationUpdate()
{
	if(GetVehicleDamage() && GetVehicleDamage()->IsBlowUpCarPartsPending() && CApplyDamage::GetNumDamagePending(this) == 0)
	{
		BlowUpPlaneParts();
	}

	CVehicle::PostBoundDeformationUpdate();
}

void CPlane::BlowUpPlaneParts()
{
	//Destroy all propellers
	fragInstGta* pFragInst = GetVehicleFragInst();
	vehicleAssert(pFragInst);

	for(int i = 0; i < m_iNumPropellers; i++)
	{
		int nRotorChild = m_propellerCollision[i].GetFragChild();
		if(nRotorChild != -1 && !pFragInst->GetChildBroken(nRotorChild))
		{
			BreakOffRotor(i);
		}		
	}

	//Damage and break off all the breakable parts of the plane and landing gear
	bool bWingDestructionSkipped = false;
	for(int i = 0; i < CAircraftDamage::NUM_DAMAGE_SECTIONS; i++)
	{
		// Wings don't blow off under certain chances
		if(NetworkInterface::IsGameInProgress() && !bWingDestructionSkipped && (i == CAircraftDamage::WING_L || i == CAircraftDamage::WING_R))
		{
			if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > PLANE_BLOW_OFF_WING_PROBABILITY)
			{
				bWingDestructionSkipped = true;
				continue;
			}
		}

		int nBoneIndex = GetBoneIndex(GetAircraftDamage().GetHierarchyIdFromSection(i));
		int nComponent = GetFragInst()->GetComponentFromBoneIndex(nBoneIndex);
		if(nComponent > -1)
		{
			GetAircraftDamage().BreakOffSection(this, i);
		}
	}

	if( GetModelIndex() != MI_PLANE_MICROLIGHT &&
		GetModelIndex() != MI_PLANE_AVENGER )
	{
		for(int i = 0; i < CLandingGearDamage::NUM_DAMAGE_SECTIONS; i++)
		{
			int nBoneIndex = GetBoneIndex(GetLandingGearDamage().GetHierarchyIdFromSection(i));
			int nComponent = GetFragInst()->GetComponentFromBoneIndex(nBoneIndex);
			if(nComponent > -1)
			{
				GetLandingGearDamage().BreakOffSection(this, i);
			}
		}
	}

	// Break all screens and emissives in the plane
	if(GetBoneIndex(VEH_EMISSIVES) != -1)
	{
		GetLocalMtxNonConst(GetBoneIndex(VEH_EMISSIVES)).Zero3x3(); 
	}
}


// Copy and pasted parts from above - [HACK_GTAV] B* 1673245
void CPlane::DamageForWorldExtents()
{
	fragInstGta* pFragInst = GetVehicleFragInst();

	if (Verifyf(pFragInst, "NULL frag inst for vehicle!"))
	{
		// Make sure the plane can be broken up.
		pFragInst->SetDisableBreakable(false);

		// Cache off the plane's velocity.
		Vector3 vVel = GetVelocity();
		
		// Zero out the plane's velocity to give the broken parts the appearance of having been ripped backwards.
		SetVelocity(Vector3(0.0, 0.0, 0.0));
		
		// Break off one of the wings.
		for(int iSection = 0; iSection < CAircraftDamage::NUM_DAMAGE_SECTIONS; iSection++)
		{
			if (iSection == CAircraftDamage::AILERON_L || iSection == CAircraftDamage::ELEVATOR_L || iSection == CAircraftDamage::AIRBRAKE_L || iSection == CAircraftDamage::WING_L)
			{
				int nBoneIndex = GetBoneIndex(GetAircraftDamage().GetHierarchyIdFromSection(iSection));
				int nComponent = GetFragInst()->GetComponentFromBoneIndex(nBoneIndex);
				if(nComponent > -1)
				{
					GetAircraftDamage().SetSectionHealth(iSection, 0.0f);
					GetAircraftDamage().BreakOffSection(this, iSection);
				}
			}
		}

		// Restore the plane's velocity.
		SetVelocity(vVel);
	}
}


///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DragPlaneToNewCoordinates
// PURPOSE :  Should be used for planes taxiing on the runway. The point along the path
//			  is specified and the plane is dragged to these coordinates.
///////////////////////////////////////////////////////////////////////////////////

void CPlane::DragPlaneToNewCoordinates(CEntity *pEntity, Vector3 &pos)
{
#define FRONTWHEEL_Y (20.0f)
#define REARWHEEL_Y (-5.0f)

	//Vector3	front = pEntity->TransformIntoWorldSpace(Vector3(0.0f, FRONTWHEEL_Y, 0.0f));
	Vector3	rear = pEntity->TransformIntoWorldSpace(Vector3(0.0f, REARWHEEL_Y, 0.0f));

	Vector3 newBVec = pos - rear;
	newBVec.z = 0.0f;
	newBVec.Normalize();

	Matrix34 newMat;

	newMat.b = -newBVec;
	newMat.c = Vector3(0.0f, 0.0f, 1.0f);
	newMat.a.Cross(newMat.b, newMat.c);

	newMat.d = pos - (FRONTWHEEL_Y * newBVec);

	static float hardCodedZ = 15.85f;
	newMat.d.z = hardCodedZ;

	pEntity->SetMatrix(newMat);
}

///////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FlyPlaneToNewCoordinates
// PURPOSE :  Should be used for planes flying around. Some points along the path
//			  are specified and the planes matrix is constructed accordingly.
///////////////////////////////////////////////////////////////////////////////////

void CPlane::FlyPlaneToNewCoordinates(CEntity *pEntity, Vector3 &pos, Vector3 &posAhead, Vector3 &posWayAhead)
{
	Matrix34 newMat;

	newMat.d = pos;
	newMat.b = pos - posAhead;
	newMat.b.z = 0.0f;
	newMat.b.Normalize();

	Vector3	dirAlongSpline = posWayAhead - pos;
	dirAlongSpline.Normalize();
	static float rollAmount = 2.0f;
	Vector3	wayAheadNorm = posWayAhead-pos;
	wayAheadNorm.Normalize();
	float	roll = rollAmount * wayAheadNorm.CrossZ(newMat.b);

	newMat.c = Vector3(roll * newMat.b.y, -roll * newMat.b.x, 1.0f);
	newMat.c.Normalize();

	newMat.a.Cross(newMat.b, newMat.c);

	pEntity->SetMatrix(newMat);
}

bank_float bfWindyWindMultForPlane = 1.5f;
bank_float bfRainingWindMultForPlane = 1.25f;
bank_float bfSnowingWindMultForPlane = 1.25f;
bank_float bfSandStormWindMultForPlane = 1.25f;
bank_float bfTurbulenceWindMultForPlane = 0.0f; // Turn off the wind effects completely when turbulence kicks in
float CPlane::ComputeWindMult(float fRatioOfVerticalFlightToUse)
{
	// Turn off wind effect when plane is landed
	if(!IsInAir())
	{
		return 0.0f;
	}
	// Turn off wind effect when plane is wrecked or lost all its wings
	if(GetStatus() == STATUS_WRECKED || (m_aircraftDamage.HasSectionBrokenOff(this,CAircraftDamage::WING_L) && m_aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_R)))
	{
		return 0.0f;
	}

	float fMult = 1.0f;
	if(pHandling && pHandling->GetFlyingHandlingData())
	{
		if(GetVerticalFlightModeAvaliable() && pHandling->GetVerticalFlyingHandlingData() && fRatioOfVerticalFlightToUse > 0.0f)
		{
			float fWindMultFromHandling = (pHandling->GetFlyingHandlingData()->m_fWindMult * (1.0f - fRatioOfVerticalFlightToUse)) + (pHandling->GetVerticalFlyingHandlingData()->m_fWindMult * fRatioOfVerticalFlightToUse);
			fMult *= fWindMultFromHandling * m_fScriptTurbulenceMult;
		}
		else
		{
			fMult *= pHandling->GetFlyingHandlingData()->m_fWindMult * m_fScriptTurbulenceMult;
		}
	}
	if(g_weather.IsWindy())
	{
		fMult *= bfWindyWindMultForPlane;
	}
	if(g_weather.IsRaining())
	{
		fMult *= bfRainingWindMultForPlane;
	}
	if(g_weather.IsSnowing())
	{
		fMult *= bfSnowingWindMultForPlane;
	}
	if(g_weather.GetSandstorm() > 0.2f)
	{
		fMult *= bfSandStormWindMultForPlane;
	}
	if(IsTurbulenceOn())
	{
		fMult *= bfTurbulenceWindMultForPlane;
	}
	return fMult;
}

bank_float bfCalmWeatherTurbulenceMultForPlane = 0.5f;
bank_float bfWindyTurbulenceMultForPlane = 0.05f;
bank_float bfRainingTurbulenceMultForPlane = 0.5f;
bank_float bfSnowingTurbulenceMultForPlane = 0.4f;
bank_float bfSandStormTurbulenceMultForPlane = 0.5f;

float CPlane::WeatherTurbulenceMult()
{
	float fMult = bfCalmWeatherTurbulenceMultForPlane;
	if(g_weather.IsWindy())
	{
		fMult += bfWindyTurbulenceMultForPlane;
	}
	if(g_weather.IsRaining())
	{
		fMult += bfRainingTurbulenceMultForPlane;
	}
	if(g_weather.IsSnowing())
	{
		fMult += bfSnowingTurbulenceMultForPlane;
	}
	if(g_weather.GetSandstorm() > 0.2f)
	{
		fMult += bfSandStormTurbulenceMultForPlane;
	}
	return Min(fMult, 1.0f);
}

float CPlane::WeatherTurbulenceMultInv()
{
	float fMult = 1.0f;
	if(g_weather.IsWindy())
	{
		fMult -= bfWindyTurbulenceMultForPlane;
	}
	if(g_weather.IsRaining())
	{
		fMult -= bfRainingTurbulenceMultForPlane;
	}
	if(g_weather.IsSnowing())
	{
		fMult -= bfSnowingTurbulenceMultForPlane;
	}
	if(g_weather.GetSandstorm() > 0.2f)
	{
		fMult -= bfSandStormTurbulenceMultForPlane;
	}
	return Max(fMult, 0.0f);
}

void CPlane::EnableIndividualPropeller(int iPropellerIndex, bool bEnable)
{
	if(iPropellerIndex >= 0 && iPropellerIndex < PLANE_NUM_PROPELLERS)
	{
		int iPropellerMask = 1 << iPropellerIndex;
		if(bEnable)
		{
			m_IndividualPropellerFlags = m_IndividualPropellerFlags | iPropellerMask;
		}
		else
		{
			m_IndividualPropellerFlags = m_IndividualPropellerFlags & (~iPropellerMask);
		}
	}
}

bool CPlane::IsIndividualPropellerOn(int iPropellerIndex) const
{
	if(iPropellerIndex >= 0 && iPropellerIndex < PLANE_NUM_PROPELLERS)
	{
		int iPropellerMask = 1 << iPropellerIndex;
		return (m_IndividualPropellerFlags & iPropellerMask) != 0;
	}

	return false;
}

// These DoesBullet/ProjectileHit... methods were copied directly from CHeli so that plane propellers exhibit the same behavior as helis.
// If we ever get around to componentising the vehicles, it'd be really nice to have this behaviour in its own module and shared by planes and helis.

bool CPlane::DoesBulletHitPropellerBound(int iComponent) const
{
	bool bIsDiscBound;
	int iPropellerIndex;
	return DoesBulletHitPropellerBound(iComponent, bIsDiscBound, iPropellerIndex);
}

bool CPlane::DoesBulletHitPropellerBound(int iComponent, bool &bHitDiscBound, int &iPropellerIndex) const
{
	for(int i =0; i < m_iNumPropellers; i++)
	{	
		if(m_propellerCollision[i].GetFragDisc() == iComponent)
		{
			bHitDiscBound = true;
			iPropellerIndex = i;
			return true;
		}
		else if(m_propellerCollision[i].GetFragGroup() > -1)
		{
			fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[m_propellerCollision[i].GetFragGroup()];
			for(int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++)
			{
				if(iComponent == m_propellerCollision[i].GetFragChild() + iChild)
				{
					bHitDiscBound = false;
					iPropellerIndex = i;
					return true;
				}
			}
		}
	}
	return false;
}

static dev_float ms_fPlaneBlockBulletSpeedRatioThreshold = 0.1f;

bool CPlane::DoesBulletHitPropeller(int iEntryComponent, const Vector3 &UNUSED_PARAM(vEntryPos), int iExitComponent, const Vector3 &UNUSED_PARAM(vExitPos)) const
{
	if(iEntryComponent != iExitComponent)
	{
		return false;
	}

	bool bHitDiscBound;
	int iPropellerIndex;

	if(DoesBulletHitPropellerBound(iEntryComponent, bHitDiscBound, iPropellerIndex))
	{
		float fSpeedRatio = m_propellers[iPropellerIndex].GetSpeed();
		if(bHitDiscBound)
		{
			return fSpeedRatio > ms_fPlaneBlockBulletSpeedRatioThreshold
				&& fSpeedRatio > fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		}
		else
		{
			return fSpeedRatio <= ms_fPlaneBlockBulletSpeedRatioThreshold;
		}
	}

	return false;
}

bool CPlane::DoesProjectileHitPropeller(int iComponent) const
{
	bool bHitDiscBound;
	int iPropellerIndex;

	if(DoesBulletHitPropellerBound(iComponent, bHitDiscBound, iPropellerIndex))
	{
		float fSpeedRatio = m_propellers[iPropellerIndex].GetSpeed();
		if(bHitDiscBound)
		{
			return fSpeedRatio > ms_fPlaneBlockBulletSpeedRatioThreshold
				&& fSpeedRatio > fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		}
		else
		{
			return fSpeedRatio <= ms_fPlaneBlockBulletSpeedRatioThreshold;
		}
	}

	return false;
}


float CPlane::ms_fPlaneControlLaggingMinBlendingRate = 1.0f;
float CPlane::ms_fPlaneControlLaggingMaxBlendingRate = 10.0f;
float CPlane::ms_fPlaneControlAbilityControlDampMin = 0.7f;
float CPlane::ms_fPlaneControlAbilityControlDampMax = 1.0f;
#if RSG_PC
float CPlane::ms_fPlaneControlAbilityControlRandomMin = 0.0f;
float CPlane::ms_fPlaneControlAbilityControlRandomMax = 1.0f;
float CPlane::ms_fPlaneControlAbilityControlRandomRollMult = 0.32f;
float CPlane::ms_fPlaneControlAbilityControlRandomPitchMult = 0.16f;
float CPlane::ms_fPlaneControlAbilityControlRandomYawMult = 0.16f;
float CPlane::ms_fPlaneControlAbilityControlRandomThrottleMult = 0.32f;
#else
float CPlane::ms_fPlaneControlAbilityControlRandomMin = 0.0f;
float CPlane::ms_fPlaneControlAbilityControlRandomMax = 1.0f;
float CPlane::ms_fPlaneControlAbilityControlRandomRollMult = 0.64f;
float CPlane::ms_fPlaneControlAbilityControlRandomPitchMult = 0.32f;
float CPlane::ms_fPlaneControlAbilityControlRandomYawMult = 0.32f;
float CPlane::ms_fPlaneControlAbilityControlRandomThrottleMult = 0.64f;
#endif 
float CPlane::ms_fTimeBetweenRandomControlInputsMin = 3.0f;
float CPlane::ms_fTimeBetweenRandomControlInputsMax = 6.0f;
float CPlane::ms_fRandomControlLerpDown			= 0.98f;
float CPlane::ms_fMaxAbilityToAdjustDifficulty		= 0.6f;
bank_bool  CPlane::ms_bDebugAfterburnerEffect = false;

#if __BANK
void CPlane::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Planes");
		bank.AddSlider("Ceiling cut off range", &ms_fCeilingLiftCutoffRange, 0.0f, 100.0f,0.01f);
		bank.AddSlider("Lagging control min blending rate", &ms_fPlaneControlLaggingMinBlendingRate, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Lagging control max blending rate", &ms_fPlaneControlLaggingMaxBlendingRate, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlDampMin", &ms_fPlaneControlAbilityControlDampMin, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlDampMax", &ms_fPlaneControlAbilityControlDampMax, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlRandomMin", &ms_fPlaneControlAbilityControlRandomMin, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlRandomMax", &ms_fPlaneControlAbilityControlRandomMax, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlRandomRollMult", &ms_fPlaneControlAbilityControlRandomRollMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlRandomPitchMult", &ms_fPlaneControlAbilityControlRandomPitchMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlRandomYawMult", &ms_fPlaneControlAbilityControlRandomYawMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fPlaneControlAbilityControlRandomThrottleMult", &ms_fPlaneControlAbilityControlRandomThrottleMult, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fTimeBetweenRandomControlInputsMin", &ms_fTimeBetweenRandomControlInputsMin, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fTimeBetweenRandomControlInputsMax", &ms_fTimeBetweenRandomControlInputsMax, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("ms_fRandomControlLerpDown", &ms_fRandomControlLerpDown, 0.0f, 100.0f, 0.1f);

		bank.AddSlider("Engine idling speed", &ms_fStdEngineSpeed, 0.0f,1.0f,0.001f);
		bank.AddSlider("Full reverse engine speed", &ms_fMinEngineSpeed, 0.0f,1.0f,0.001f);
		bank.AddSlider("Full forward engine speed", &ms_fMaxEngineSpeed, 0.0f,1.0f,0.001f);
		bank.AddSlider("Jet on ground idle engine speed", &bfStdJetOnGroundEngineSpeed, 0.0f,1.0f,0.001f);
		bank.AddSlider("Jet on ground full reverse engine speed", &bfMinJetOnGroundEngineSpeed, 0.0f,1.0f,0.001f);
		bank.AddSlider("Jet on ground full forward engine speed", &bfMaxJetOnGroundEngineSpeed, 0.0f,1.0f,0.001f);
		bank.AddSlider("Propeller speed mult", &ms_fPropRenderSpeed, 0.0f,100.0f,0.01f);
		bank.AddToggle("Debug afterburner effect", &ms_bDebugAfterburnerEffect);

		bank.PushGroup("Speed Damping");
			bank.AddSlider("ms_fTopSpeedDampRate", &ms_fTopSpeedDampRate, -100.0f, 1000.0f,0.1f);
			bank.AddSlider("ms_fTopSpeedDampMaxHeight", &ms_fTopSpeedDampMaxHeight, -100.0f, 1000.0f,0.1f);
			bank.AddSlider("ms_fTopSpeedDampMinHeight", &ms_fTopSpeedDampMinHeight, -100.0f, 1000.0f,0.1f);
		bank.PopGroup();
		
		bank.PushGroup("Damage");
			bank.AddSlider("Min damage to cause fire", &CAircraftDamage::ms_fMinCausingFlameDamage, 0.0f, 1000.0f, 1.0f);
			bank.AddSlider("Max damage to cause fire", &CAircraftDamage::ms_fMaxCausingFlameDamage, 0.0f, 1000.0f, 1.0f);
			bank.AddSlider("Min damage flame life span", &CAircraftDamage::ms_fMinFlameLifeSpan, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("Max damage flame life span", &CAircraftDamage::ms_fMaxFlameLifeSpan, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("flame damage multiplier", &CAircraftDamage::ms_fFlameDamageMultiplier, 1.0f, 10.0f, 0.01f);
			bank.AddSlider("flame mergeable distance", &CAircraftDamage::ms_fFlameMergeableDistance, 0.0f, 10.0f, 0.01f);
			bank.AddToggle("Draw flame positions", &CAircraftDamage::ms_bDrawFlamePosition);
			bank.AddSlider("Turbulence body health threshold", &sfBuffetingBodyHealthThreshold, 0.0f, 1000.0f, 1.0f);
			bank.AddSlider("Engine degrade min damage", &dfEngineDegradeMinDamage, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Engine degrade max damage", &dfEngineDegradeMaxDamage, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Engine miss fire min time", &bfPlaneEngineMissFireMinTime, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Engine miss fire max time", &bfPlaneEngineMissFireMaxTime, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Engine miss fire min recover time", &bfPlaneEngineMissFireMinRecoverTime, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Engine miss fire max recover time", &bfPlaneEngineMissFireMaxRecoverTime, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Engine speed drop rate when miss firing", &ms_fEngineSpeedDropRateWhenMissFiring, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("Landing impact damage multiplier", &bfPlaneLandingDamageMult, 0.0f, 10.0f, 0.01f);
		bank.PopGroup();

		bank.PushGroup("Turbulence");
			bank.AddToggle("Turbulence on", &bfPlaneTurbulenceOn);
			bank.AddSlider("Turbulence min during",&bfTurbulenceTimeMin,0.0f,20.0f,0.01f);
			bank.AddSlider("Turbulence max during",&bfTurbulenceTimeMax,0.0f,20.0f,0.01f);
			bank.AddSlider("Turbulence min recover time",&bfTurbulenceRecoverTimeMin,0.0f,20.0f,0.01f);
			bank.AddSlider("Turbulence max recover time",&bfTurbulenceRecoverTimeMax,0.0f,20.0f,0.01f);
			bank.AddSlider("Wind multiplier in windy condition",&bfWindyWindMultForPlane,0.0f,10.0f,0.01f);
			bank.AddSlider("Wind multiplier in raining condition",&bfRainingWindMultForPlane,0.0f,10.0f,0.01f);
			bank.AddSlider("Wind multiplier in snow condition",&bfSnowingWindMultForPlane,0.0f,10.0f,0.01f);
			bank.AddSlider("Wind multiplier in sand storm condition",&bfSandStormWindMultForPlane,0.0f,10.0f,0.01f);
			bank.AddSlider("Turbulence multiplier in windy condition",&bfWindyTurbulenceMultForPlane,0.0f,1.0f,0.01f);
			bank.AddSlider("Turbulence multiplier in raining condition",&bfRainingTurbulenceMultForPlane,0.0f,1.0f,0.01f);
			bank.AddSlider("Turbulence multiplier in snow condition",&bfSnowingTurbulenceMultForPlane,0.0f,1.0f,0.01f);
			bank.AddSlider("Turbulence multiplier in sand storm condition",&bfSandStormTurbulenceMultForPlane,0.0f,1.0f,0.01f);
			bank.AddSlider("Controller vibration scale when turbulence",&sfPlaneTurbulencePedVibMult,0.0f,1.0f,0.01f);
		bank.PopGroup();
	bank.PopGroup();
	CLandingGear::InitWidgets(bank);
}
#endif

dev_float PLANE_STALL_ANGLE = ( DtoR * 20.0f);
dev_float PLANE_STALL_MIN_VERTICAL_SPEED = -9.8f;
dev_float PLANE_WING_MAX_ANGLE_OF_ATTACK = 15.0f;
dev_float PLANE_WING_MAX_SIDE_SLIP = 15.0f;
dev_float PLANE_WING_MAX_FORCE = 70.0f;
dev_float fNozeDownAngSpeed = 180.0f;

dev_float sfPlaneControlPilotSchoolMulti = 1.0f;
float FullRumbleSideWindSpeed = 100.0f;
dev_float sfPlaneStallBiasSpeed = 1.0f;

dev_float sfPlaneVirtualLiftSpeedLimit = 2.5f;
dev_float sfReverseThrustMult = 0.25f;
dev_float sfReverseThrustMultInReverse = 0.45f;
dev_float sfForwardThrustMultOnGround = 0.75f;
dev_float sfPlaneTakeOffPitchMulti = 1.0f;
dev_float sfPlaneThrustNoseDivingMulti = 0.5f;
dev_float dfPlaneThrottleControlLimit = 10.0f;
dev_float dfPlaneBrakeControlLimit = 5.0f;

void CPlane::CalculateYawAndPitchcontrol( CControl* pControl, float& fYawControl, float& fPitchControl )
{
	// CONTROLS
	if(fYawControl==FLY_INPUT_NULL)
	{
		fYawControl = 0.0f;
		if(pControl)
		{
			fYawControl = pControl->GetVehicleFlyRollLeftRight().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
		}
	}
	if(fPitchControl==FLY_INPUT_NULL)
	{
		fPitchControl = 0.0f;
		if(pControl)
		{
			fPitchControl = pControl->GetVehicleFlyPitchUpDown().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
			// Comment it out to make PC have the same control as in PS2 in JoyPad
		}
	}

	// stuff to convert from a nice circular joystick input, into a nice square representation
	float fJoyTheta = rage::Atan2f(fPitchControl, fYawControl);

	float fMaxMag = 1.0f;
	if( fJoyTheta > -PI/4.0f && fJoyTheta <= PI/4.0f )
	{
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta);
	}
	else if( fJoyTheta > PI/4.0f && fJoyTheta <= 0.75f * PI)
	{
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta - PI/2.0f);
	}
	else if( fJoyTheta > 0.75f * PI )
	{
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta - PI);
	}
	else if( fJoyTheta  <= -0.75f*PI )
	{
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta + PI);
	}
	else if( fJoyTheta > -0.75f*PI && fJoyTheta < -PI/4.0f)
	{
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta + PI/2.0f);
	}

	fYawControl *= fMaxMag;
	fPitchControl *= -fMaxMag;

	if(!IsInAir())
	{
		fPitchControl *= sfPlaneTakeOffPitchMulti;
	}
}


void CPlane::CalculateDamageEffects( float& fPitchFromDamageEffect, float& fYawFromDamageEffect, float& fRollFromDamageEffect, float fPitchControl, float fYawControl, float fRollControl )
{
	static dev_float sfRollControlLostWingEffect = 1.5f;
	static dev_float sfRollControlFlapDamageEffect = 5.0f;
	static dev_float sfPitchControlAileronDamageEffect = 3.0f;
	static dev_float sfYawControlRudderDamageEffect = 3.0f;
	static dev_float sfRollControlLostEngineEffect = 0.25f;
	static dev_float sfYawControlLostEngineEffect = 0.1f;

	if(IsInAir())
	{
		bool applyRollFromDamage = true;
		
		if( GetVerticalFlightModeAvaliable() &&
			 m_fDesiredVerticalFlightRatio == 1.0f )
		{
			// don't apply roll effect if we're in vertical flight mode and we have no wings
			if( ( GetAircraftDamage().HasSectionBrokenOff( this, CAircraftDamage::WING_L ) || GetAircraftDamage().GetHierarchyIdFromSection( CAircraftDamage::WING_L ) == VEH_INVALID_ID ) &&
				( GetAircraftDamage().HasSectionBrokenOff( this, CAircraftDamage::WING_R ) || GetAircraftDamage().GetHierarchyIdFromSection( CAircraftDamage::WING_R ) == VEH_INVALID_ID ) )
			{
				applyRollFromDamage = false;
			}
		}
		 
		// Wing Damage
		//if we're in the air and we've lost a wing make the plane spin.
		if( applyRollFromDamage &&
			GetAircraftDamage().HasSectionBrokenOff(this, CAircraftDamage::WING_L))
		{
			fRollFromDamageEffect -= sfRollControlLostWingEffect;
		}
		else if( applyRollFromDamage &&
				 GetAircraftDamage().HasSectionBrokenOff(this, CAircraftDamage::WING_R))
		{
			fRollFromDamageEffect += sfRollControlLostWingEffect;
		}

		if(m_iNumPropellers > 1 && GetEngineSpeed() > 0.0f)
		{
			float fRollControlLostPowerEffect = 0.0f;
			float fYawControlLostPowerEffect = 0.0f;

			// Engine Damage
			//if we lost an engine, also make the plane spin.
			if(GetVehicleDamage()->GetEngineHealth() > sfPlaneEngineBreakDownHealth)
			{
				if(GetAircraftDamage().GetSectionHealth(CAircraftDamage::ENGINE_L) <= 0.0f)
				{
					fRollControlLostPowerEffect -= sfRollControlLostEngineEffect;
					fYawControlLostPowerEffect -= sfYawControlLostEngineEffect;
				}
				else if(GetAircraftDamage().GetSectionHealth(CAircraftDamage::ENGINE_R) <= 0.0f)
				{
					fRollControlLostPowerEffect += sfRollControlLostEngineEffect;
					fYawControlLostPowerEffect += sfYawControlLostEngineEffect;
				}
			}

			// Propeller Damage
			//if we lost any propeller, also make the plane spin.
			{
				int nPropellersLostOnRightSide = 0;
				int nPropellersLostOnLeftSide = 0;
				for(int i = 0; i < m_iNumPropellers; i++) 
				{
					if(m_fPropellerHealth[i] <= 0.0f)
					{
						if(i < m_iNumPropellers / 2)
						{
							nPropellersLostOnLeftSide++;
						}
						else
						{
							nPropellersLostOnRightSide++;
						}
					}
				}

				if(nPropellersLostOnRightSide + nPropellersLostOnLeftSide < m_iNumPropellers 
					&& nPropellersLostOnRightSide != nPropellersLostOnLeftSide)
				{
					if(nPropellersLostOnLeftSide > nPropellersLostOnRightSide)
					{
						fRollControlLostPowerEffect -= sfRollControlLostEngineEffect;
						fYawControlLostPowerEffect -= sfYawControlLostEngineEffect;
					}
					else
					{
						fRollControlLostPowerEffect += sfRollControlLostEngineEffect;
						fYawControlLostPowerEffect += sfYawControlLostEngineEffect;
					}
				}
			}

			fRollFromDamageEffect += Clamp(fRollControlLostPowerEffect, -sfRollControlLostEngineEffect, sfRollControlLostEngineEffect);
			fYawFromDamageEffect += Clamp(fYawControlLostPowerEffect, -sfYawControlLostEngineEffect, sfYawControlLostEngineEffect);
		}

		float fBodyHealthMult = 0.0f;
		if(GetHealth() < sfBuffetingBodyHealthThreshold)
		{
			CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

			fBodyHealthMult = 1.0f - GetHealth() / GetMaxHealth();
			fBodyHealthMult = Clamp(fBodyHealthMult, 0.0f, 1.0f);
			fBodyHealthMult *= pFlyingHandling->m_fBodyDamageControlEffectMult;
		}

		// Flap (pitch) Damage causes rolling noise
		float fHealthMult = Max(1.0f - m_aircraftDamage.GetPitchMult(this), fBodyHealthMult);
		fRollFromDamageEffect += fwRandom::GetRandomNumberInRange(-1.0f, 1.0f) * sfRollControlFlapDamageEffect * fHealthMult * (0.5f * fPitchControl + 0.5f);

		// Aileron (roll) Damage causes pitching noise
		fHealthMult = Max(1.0f - m_aircraftDamage.GetRollMult(this), fBodyHealthMult);
		fPitchFromDamageEffect += fwRandom::GetRandomNumberInRange(-1.0f, 1.0f) * sfPitchControlAileronDamageEffect * fHealthMult * (0.5f * fRollControl + 0.5f);

		// Rudder (yaw) Damage causes yaw noise
		fHealthMult = Max(1.0f - m_aircraftDamage.GetYawMult(this), fBodyHealthMult);
		fYawFromDamageEffect += fwRandom::GetRandomNumberInRange(-1.0f, 1.0f) * sfYawControlRudderDamageEffect * fHealthMult * (0.5f * fYawControl + 0.5f);

		// B*3843387 - make damage not affect the handling of the schnuppe as much
		if( GetModelIndex() == MI_PLANE_STARLING )
		{
			fPitchFromDamageEffect *= 0.2f;
			fYawFromDamageEffect *= 0.5f;
			fRollFromDamageEffect *= 0.5f;
		}
	}
}

void CPlane::CalculateThrust( float fThrottleControl, float fYawControl, float fForwardSpeed, float fThrustHealthMult, float fOverallMultiplier, bool bPlayerDriver )
{
	// THRUST
	Vector3 vecTransformB = VEC3V_TO_VECTOR3(GetTransform().GetB());
	Vector3 vecThrust = vecTransformB;
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

	// only let pilot backwards thrust when on the ground
	if( pFlyingHandling->m_fThrust == 0.0f &&
		!IsInAir() )
	{
		if( fThrottleControl >= 0.0f )
		{
			fThrottleControl = Abs( fYawControl );
		}

		static dev_float zeroThrustOnGroundForce = 1.25f;
		fThrottleControl *= zeroThrustOnGroundForce;

		float fFallOff = pFlyingHandling->m_fThrustFallOff * fForwardSpeed * fForwardSpeed;
		fThrottleControl *= Max( 0.0f, ( 1.0f -fFallOff ) );
	}
	else if(IsInAir() || fThrottleControl > 0.0f)
	{
		if(fForwardSpeed > 0.0f)
		{
			if( fThrottleControl > 0.0f ||
				GetModelIndex() != MI_PLANE_SEABREEZE ||
			 	GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() == 0.0f )
			{
				// Rescales throttle so full brake = 0.0f throttle
				fThrottleControl = pFlyingHandling->m_fThrust*0.5f*(fThrottleControl + 1.0f);
			}

			if( m_bEnableThrustFallOffThisFrame )
			{
				// let player driven planes
				// use old code path
				if( bPlayerDriver )
				{
					float fFallOff = pFlyingHandling->m_fThrustFallOff*fForwardSpeed*fForwardSpeed;
					fThrottleControl *= Max( 0.0f, (1.0f -fFallOff) );
				}
				else
				{
					// if we're trying to accelerate then falloff
					// allow deceleration
					if( DotProduct(vecThrust, GetVelocity()) * fThrottleControl > 0.0f )
					{
						// Make throttle fall off at high speed
						// clamp to max of 1.0f
						float fFallOff = Min(pFlyingHandling->m_fThrustFallOff*fForwardSpeed*fForwardSpeed, 1.0f);
						fThrottleControl *= (1.0f -fFallOff);
					}
				}
			}
		}
	}
	// need to do another dotProduct here rather than use fForwardSpd because we don't want the influence of the windspeed
	else if(fThrottleControl < 0.0f && DotProduct(vecThrust, GetVelocity()) < 0.02f && IsInAir())
	{
		fThrottleControl = pFlyingHandling->m_fThrust*MIN(0.0f, (fThrottleControl - 8.0f*0.97f*fForwardSpeed));
	}
	else if(fForwardSpeed < 0.0f)
	{
		fThrottleControl = pFlyingHandling->m_fThrust*fThrottleControl;

		// Make throttle fall off at high speed
		float fFallOff = pFlyingHandling->m_fThrustFallOff*fForwardSpeed*fForwardSpeed;
		fThrottleControl *= (1.0f -fFallOff);
	}

	//Reduce throttle when using reverse, so we can't stop as quick on a runway.
	if(fThrottleControl >= 0.0f)
	{
		if(!IsInAir())
		{
			fThrottleControl *= sfForwardThrustMultOnGround;
		}
	}
	else
	{
		if(fForwardSpeed > 0.0f)
		{
			fThrottleControl *= sfReverseThrustMult;
		}
		else
		{
			fThrottleControl *= sfReverseThrustMultInReverse;
		}
	}

	// limit height that this plane can fly
	Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	if( HeightAboveCeiling(vThisPosition.z) > 0.0f )
	{
		fThrottleControl = Min( fThrottleControl, 0.0f );
		SetRocketBoosting(false);
	}

	//Reduce throttle when upside down
	if((GetTransform().GetC().GetZf() < 0.0f) && GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		fThrottleControl = 0.0f;
	}

	//Increase throttle when nose diving
	if(IsInAir() && fThrottleControl > 0.0 && fThrottleControl < 2.0 && vecTransformB.z < 0.0f && IsEngineOn())
	{
		fThrottleControl += fThrottleControl * Abs(vecTransformB.z) * sfPlaneThrustNoseDivingMulti;
	}

	//Remove thrust based on how many propellers are alive
	float fPropMult = 1.0f;
	for(int i = 0; i< m_iNumPropellers; i++)
	{
		if( m_fPropellerHealth[i] <= 0.0f )
		{
			fPropMult -= (1.0f/m_iNumPropellers);

			if( GetModelIndex() == MI_PLANE_MICROLIGHT )
			{
				fPropMult = 0.0f;
			}
		}
	}

	Vector3 vThrust( vecThrust*m_fGravityForWheelIntegrator*fThrottleControl*GetMass()*fThrustHealthMult*fPropMult*fOverallMultiplier );
#if __ASSERT
	Assertf(vThrust.Mag2() < square(150.0f * rage::Max(1.0f, GetMass())), 
		"ProcessFlightModel force over limit, force [%3.2f, %3.2f, %3.2f], thrust[%3.2f, %3.2f, %3.2f], throttle %3.2f, healthMult %3.2f, propMult %3.2f, overall %3.2f",
		vThrust.x, vThrust.y, vThrust.z, vecThrust.x, vecThrust.y, vecThrust.z, fThrottleControl, fThrustHealthMult, fPropMult, fOverallMultiplier); 
#endif

	ApplyInternalForceCg(vThrust);
}

void CPlane::CalculateSideSlip( Vector3 vecAirSpeed, float fOverallMultiplier )
{
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

	// SIDEFORCES
	Vector3 vecSlideSlipForce = VEC3V_TO_VECTOR3(GetTransform().GetA());
	// first do sideslip angle of attack
	float fSideSlipAngle = -1.0f*DotProduct(vecSlideSlipForce, vecAirSpeed);
	fSideSlipAngle = rage::Clamp(fSideSlipAngle, -PLANE_WING_MAX_SIDE_SLIP, PLANE_WING_MAX_SIDE_SLIP);//limit the slip angle so we dont apply massive forces when hitting objects

	// doing sideways stuff

	// sideways force from sidesliping
	vecSlideSlipForce *= pFlyingHandling->m_fSideSlipMult * fSideSlipAngle * rage::Abs( fSideSlipAngle );
#if __ASSERT
	{
		Vector3 vForce = vecSlideSlipForce * GetMass() * m_fGravityForWheelIntegrator * fOverallMultiplier;
		Assertf(vForce.Mag2() < square(150.0f * rage::Max(1.0f, GetMass())), 
			"ProcessFlightModel force over limit, force [%3.2f, %3.2f, %3.2f], SlideSlipForce [%3.2f, %3.2f, %3.2f], mult %3.2f, angle %3.2f, overall %3.2f",
			vForce.x, vForce.y, vForce.z, vecSlideSlipForce.x, vecSlideSlipForce.y, vecSlideSlipForce.z, pFlyingHandling->m_fSideSlipMult, fSideSlipAngle, fOverallMultiplier); 
	}
#endif

	ApplyInternalForceCg(vecSlideSlipForce*GetMass()*m_fGravityForWheelIntegrator*fOverallMultiplier);
}

Vector3 CPlane::CalculateYaw( Vector3 vecTailOffset, Vector3 vecWindSpeed, float fOverallMultiplier, float fYawControl, float fVirtualFwdSpd, float fYawFromDamageEffect )
{
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	Vector3 vecAngInertia = GetAngInertia();

	// control force from steering and stabilising force from rudder
	Vector3 vecRudderForce = VEC3V_TO_VECTOR3( GetTransform().GetA() );
	float fSideSlipAngle = -1.0f*DotProduct(vecRudderForce, GetLocalSpeed(vecTailOffset) - ComputeWindMult(0.0f)*vecWindSpeed - m_vTurbulenceAirflow);

	fSideSlipAngle = rage::Clamp(fSideSlipAngle, -PLANE_WING_MAX_SIDE_SLIP, PLANE_WING_MAX_SIDE_SLIP);//limit the slip angle so we dont apply massive forces when hitting objects

	float fHealthMult = m_aircraftDamage.GetYawMult(this);

	float virtualSpeedForYaw = fVirtualFwdSpd;
	const float sfOnGroundYawLowSpeedThresh = pFlyingHandling->m_fOnGroundYawBoostSpeedCap;
	const float sfOnGroundYawLowSpeedPeak = pFlyingHandling->m_fOnGroundYawBoostSpeedPeak;
	// If we are on the ground increase the yaw force, so we can turn more easily
	if(!IsInAir() && fVirtualFwdSpd <= sfOnGroundYawLowSpeedThresh)
	{
		// This ends up being a piecemeal linear function that goes from 0 at 0 m/s to 4 times the normal yaw force of 20 m/s at 5 m/s and 
		//  then falls back down to the normal yaw force of 20 m/s as we approach 20 m/s
		// The result is a dramatic increase in turning power on the runway for all planes that fades in and out nicely around a low speed threshold (Since the overall speed value remains continuous at all points)
		if(fVirtualFwdSpd <= sfOnGroundYawLowSpeedPeak)
		{
			virtualSpeedForYaw = (fVirtualFwdSpd / sfOnGroundYawLowSpeedPeak) * sfOnGroundYawLowSpeedThresh * (sfOnGroundYawLowSpeedThresh / sfOnGroundYawLowSpeedPeak);
		}
		else
		{
			virtualSpeedForYaw = sfOnGroundYawLowSpeedThresh * (sfOnGroundYawLowSpeedThresh / fVirtualFwdSpd);
		}
	}

	float fRudderForceFactor = pFlyingHandling->m_fYawMult*fYawControl*virtualSpeedForYaw + pFlyingHandling->m_fYawStabilise*fSideSlipAngle*rage::Abs(fSideSlipAngle);
	fRudderForceFactor += fYawFromDamageEffect * pFlyingHandling->m_fYawMult * fVirtualFwdSpd; // Intentionally not using virtualSpeedForYaw here
	fRudderForceFactor *= fHealthMult;

	vecRudderForce *= fRudderForceFactor*vecAngInertia.z*m_fGravityForWheelIntegrator;

	Vector3 vecTorque = vecTailOffset;
	vecTorque.Cross(vecRudderForce*fOverallMultiplier);

#if __ASSERT
	Vector3 vecAngAccel = VEC3V_TO_VECTOR3(GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecTorque)));
	vecAngAccel.Multiply(GetInvAngInertia());
	Assertf(vecAngAccel.Mag2() < square(150.0f), "vecAngAccel.Mag2() < square(150.0f), A=%.2f,%.2f,%.2f, %.2f YawMult, %.2f YawControl, %.2f ForwardSpd, %.2f YawStab, %.2f fSideSlipAngle, %.2f AngInertia.z %.2f overall",
		GetTransform().GetA().GetXf(), GetTransform().GetA().GetYf(), GetTransform().GetA().GetZf(),
		pFlyingHandling->m_fYawMult, fYawControl, fVirtualFwdSpd, pFlyingHandling->m_fYawStabilise, fSideSlipAngle, vecAngInertia.z, fOverallMultiplier);
#endif
	return vecTorque;
}

Vector3 CPlane::CalculateRoll( float fOverallMultiplier, float fRollControl, float fRollFromDamageEffect, float fVirtualFwdSpd )
{
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	Vector3 vecAngInertia = GetAngInertia();

	// ROLL    
	Vector3 vecRollForce = VEC3V_TO_VECTOR3( GetTransform().GetA() );

	float fHealthMult = m_aircraftDamage.GetRollMult(this);
	float fRollForceFactor = pFlyingHandling->m_fRollMult * fRollControl * fVirtualFwdSpd;
	fRollForceFactor *= fHealthMult;

	fRollForceFactor += fRollFromDamageEffect * pFlyingHandling->m_fRollMult * fVirtualFwdSpd;

	if(!IsInAir())// if we are on the ground reduce the roll force, so we don't tip over the plane as easily.
	{
		static dev_float sfOnGroundRollReductionFactor = 0.2f;
		fRollForceFactor *= sfOnGroundRollReductionFactor;
	}

	vecRollForce *= fRollForceFactor * vecAngInertia.y * m_fGravityForWheelIntegrator;

	Vector3 vecTorque = VEC3V_TO_VECTOR3( GetTransform().GetC() );
	vecTorque.Cross( vecRollForce * fOverallMultiplier );

#if __ASSERT
	Vector3 vecAngAccel = VEC3V_TO_VECTOR3(GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecTorque)));
	vecAngAccel.Multiply(GetInvAngInertia());
	Assertf(vecAngAccel.Mag2() < square(150.0f), "vecAngAccel.Mag2() < square(150.0f), A=%.2f,%.2f,%.2f C=%.2f,%.2f,%.2f, %.2f RollMult, %.2f RollControl, %.2f ForwardSpd, %.2f AngInertia.y %.2f overall",
		GetTransform().GetA().GetXf(), GetTransform().GetA().GetYf(), GetTransform().GetA().GetZf(),
		GetTransform().GetC().GetXf(), GetTransform().GetC().GetYf(), GetTransform().GetC().GetZf(),
		pFlyingHandling->m_fRollMult, fRollControl, fVirtualFwdSpd, vecAngInertia.y, fOverallMultiplier);
#endif
	return vecTorque;
}

Vector3 CPlane::CalculateStuntRoll( float UNUSED_PARAM( fOverallMultiplier ), float fRollControl, float fTimeStep )
{
	TUNE_GROUP_FLOAT( STUNT_PLANE, STUNT_ROLL_MAX_SPEED, 50.0f, 0.0f, 100.0f, 0.1f );
	TUNE_GROUP_FLOAT( STUNT_PLANE, STUNT_ROLL_CONTROL_DEAD_ZONE, 0.0f, 0.0f, 10.0f, 0.1f );
	TUNE_GROUP_FLOAT( STUNT_PLANE, STUNT_ROLL_TORQUE_SCALE, 1.0f, 0.0f, 10.0f, 0.1f );
	TUNE_GROUP_BOOL( STUNT_PLANE, USE_ROLL_VELOCITY, true );
	TUNE_GROUP_BOOL( STUNT_PLANE, USE_STUNT_ROLL_LOCK, false );
	TUNE_GROUP_INT( STUNT_PLANE, STICK_HELD_DURATION, 250, 0, 10000, 0 );

	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

	static float targetRollAngle = 0.0f;
	if( fRollControl > 0.0f )
	{
		fRollControl = Max( fRollControl - STUNT_ROLL_CONTROL_DEAD_ZONE, 0.0f );
	}
	else
	{
		fRollControl = Min( fRollControl + STUNT_ROLL_CONTROL_DEAD_ZONE, 0.0f );
	}
	fRollControl /= 1.0f - STUNT_ROLL_CONTROL_DEAD_ZONE;
	fRollControl *= -1.0f;
	fRollControl *= Abs( fRollControl );
	float currentRoll = GetTransform().GetRoll();

	static u32 anglePressedDurationTime = 0;
	static bool useTargetRoll = false;
	float rollLockControl = fRollControl;

	if( fRollControl != 0.0f &&
		( Abs( fRollControl * PI * 0.5f ) > Abs( targetRollAngle ) ||
		  targetRollAngle * fRollControl < 0.0f ) )
	{
		useTargetRoll = false;
		anglePressedDurationTime = fwTimer::GetTimeInMilliseconds();

		rollLockControl *= 100.0f;
		rollLockControl = Clamp( rollLockControl, -1.0f, 1.0f );
		targetRollAngle = rollLockControl * PI * 0.5f;

		if( Abs( currentRoll ) > Abs( targetRollAngle ) ||
			currentRoll * targetRollAngle < 0.0f )
		{
			targetRollAngle = 0.0f;
		}
	}

	if( fRollControl == 0 &&
		fwTimer::GetTimeInMilliseconds() - anglePressedDurationTime < 250 )
	{
		if( Abs( currentRoll ) < Abs( targetRollAngle ) )
		{
			useTargetRoll = true;
		}
		anglePressedDurationTime = 0;
	}

	float torque = 0.0f;

	if( useTargetRoll &&
		USE_STUNT_ROLL_LOCK )
	{
		currentRoll += PI;
		torque = -( ( targetRollAngle + PI ) - currentRoll );
	}
	else
	{
		torque = pFlyingHandling->m_fRollMult * -fRollControl;
	}
	torque /= fTimeStep;

	Vector3 angularVelocity = GetAngVelocity();
	float rollVelocity = angularVelocity.Dot( VEC3V_TO_VECTOR3( GetTransform().GetB() ) );

	float angularMomentum = angularVelocity.y * Abs( angularVelocity.y ) * GetAngInertia().y;

	if( USE_ROLL_VELOCITY )
	{
		static bool useSqrVel = true; 
		if( useSqrVel )
		{
			angularMomentum = rollVelocity * Abs( rollVelocity ) * ( GetAngInertia().y * 0.5f );
		}
		else
		{
			angularMomentum = rollVelocity * GetAngInertia().y;
		}
	}

	//torque -= GetAngVelocity().y * STUNT_ROLL_TORQUE_SCALE;
	torque = Clamp( torque, -STUNT_ROLL_MAX_SPEED, STUNT_ROLL_MAX_SPEED );
	torque *= GetAngInertia().y;
	torque -= angularMomentum * STUNT_ROLL_TORQUE_SCALE;

	Vector3 vecTorque( 0.0f, torque, 0.0f );
	vecTorque = VEC3V_TO_VECTOR3( GetTransform().Transform3x3( VECTOR3_TO_VEC3V( vecTorque ) ) );

	return vecTorque;
}

Vector3 CPlane::ApplyRollStabilisation( float fOverallMultiplier )
{
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

	// also need to add some stabilisation for roll (so plane naturally levels out)
	Vector3 vecForward = VEC3V_TO_VECTOR3( GetTransform().GetB() );
	Vector3 vecRightVectorWithoutRoll;
	vecRightVectorWithoutRoll.Cross(vecForward, Vector3(0.0f,0.0f,1.0f));

	float fRollOffset = 1.0f;
	if(GetTransform().GetC().GetZf() > 0.0f)
	{
		if(GetTransform().GetA().GetZf() > 0.0f)
		{
			fRollOffset = -1.0f;
		}
	}
	else
	{
		vecRightVectorWithoutRoll *= -1.0f;
		//	Is it right that the following code is done if C.z > 0.0f or not?
		if(GetTransform().GetA().GetZf() > 0.0f)
		{
			fRollOffset = -1.0f;
		}
	}

	const Vector3 vecRight = VEC3V_TO_VECTOR3( GetTransform().GetA() );;
	fRollOffset *= 1.0f - DotProduct(vecRightVectorWithoutRoll, vecRight);
	// roll stabilisation recedes as we go vertical
	fRollOffset *= 1.0f - rage::Abs(vecForward.z);


	Vector3 vecTorque = VEC3V_TO_VECTOR3( GetTransform().GetC() );
	vecTorque.Cross(pFlyingHandling->m_fRollStabilise*fRollOffset*GetAngInertia().y*0.5f*m_fGravityForWheelIntegrator*vecRight*fOverallMultiplier);

#if __ASSERT
	Vector3 vecAngAccel = VEC3V_TO_VECTOR3(GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecTorque)));
	vecAngAccel.Multiply(GetInvAngInertia());
	Assertf(vecAngAccel.Mag2() < square(150.0f), "vecAngAccel.Mag2() < square(150.0f),  vecTorque.Cross(pFlyingHandling->m_fRollStabilise*fRollOffset*vecAngInertia.y*0.5f*-GRAVITY*vecRight*fOverallMultiplier)");
#endif

	return vecTorque;
}

Vector3 CPlane::CalculatePitch( Vector3 vecWindSpeed, Vector3 vecTailOffset, Vector3 vecAirSpeed, float fOverallMultiplier, float fPitchControl, float fPitchFromDamageEffect, float fVirtualFwdSpd, bool bClampAngleOfAttack )
{
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

	// PITCH
	// calculate tailplane angle of attack
	Vector3 vecPitchTorque	= VEC3V_TO_VECTOR3( GetTransform().GetC() );
	Vector3 vecTransformB	= VEC3V_TO_VECTOR3( GetTransform().GetB() );

	// Add bias speed to help plane nose down easily during the stall
	Vector3 sfBiasVelocity = vecPitchTorque;
	sfBiasVelocity.z = 0.0f;
	sfBiasVelocity *= sfPlaneStallBiasSpeed;
	// the bias velocity is aligned with forward direction
	if( DotProduct( sfBiasVelocity, vecTransformB ) < 0.0f )
	{
		sfBiasVelocity *= -1.0f;
	}

	if( fPitchControl > 0.0f &&
		HasContactWheels() &&
		( GetModelIndex() == MI_PLANE_HOWARD ||
		  GetModelIndex() == MI_PLANE_NOKOTA ) )
	{
		static dev_float sfScaleNegPitchWhenOnGround = 0.2f;
		fPitchControl *= sfScaleNegPitchWhenOnGround;
	}

	float fAngleOfAttack = -1.0f*DotProduct(vecPitchTorque, GetLocalSpeed(vecTailOffset) + sfBiasVelocity - ComputeWindMult(0.0f)*vecWindSpeed - m_vTurbulenceAirflow);

	if(bClampAngleOfAttack)
	{
		fAngleOfAttack = rage::Clamp(fAngleOfAttack, -PLANE_WING_MAX_ANGLE_OF_ATTACK, PLANE_WING_MAX_ANGLE_OF_ATTACK);//limit the angle of attack so we dont apply massive forces when hitting objects
	}

	float fPitchTorqueFactor = pFlyingHandling->m_fPitchMult*fPitchControl*fVirtualFwdSpd + pFlyingHandling->m_fPitchStabilise*fAngleOfAttack*rage::Abs(fAngleOfAttack);

	// As elevators take damage apply less pitch
	float fHealthMult = m_aircraftDamage.GetPitchMult(this);
	fPitchTorqueFactor *= fHealthMult;

	fPitchTorqueFactor += fPitchFromDamageEffect * pFlyingHandling->m_fPitchMult * fVirtualFwdSpd;

	vecPitchTorque *= fPitchTorqueFactor * GetAngInertia().x * m_fGravityForWheelIntegrator;

	// Prevent plane take off backwards
	if( DotProduct( vecTransformB, vecAirSpeed ) < 0.0f )
	{
		vecPitchTorque.z = Min( 0.0f, vecPitchTorque.z );
	}

	Vector3 vecTorque = vecTailOffset;
	vecTorque.Cross( vecPitchTorque * fOverallMultiplier );

#if __ASSERT
	Vector3 vecAngAccel = VEC3V_TO_VECTOR3( GetTransform().UnTransform3x3( VECTOR3_TO_VEC3V( vecTorque ) ) );
	vecAngAccel.Multiply( GetInvAngInertia() );
	Assertf( vecAngAccel.Mag2() < square( 150.0f ), "vecAngAccel.Mag2() < square(150.0f), C=%.2f,%.2f,%.2f, TF=%.2f,%.2f,%.2f, %.2f RudderForceFactor, %.2f PitchMult, %.2f PitchControl, %.2f fVirtualFwdSpd, %.2f PitchStab, %.2f AngleOfAttack, %.2f vecAngInertia.x",
		GetTransform().GetC().GetXf(), GetTransform().GetC().GetYf(), GetTransform().GetC().GetZf(),
		vecTailOffset.x, vecTailOffset.y, vecTailOffset.z,
		fPitchTorqueFactor, pFlyingHandling->m_fPitchMult, fPitchControl, fVirtualFwdSpd, pFlyingHandling->m_fPitchStabilise, fAngleOfAttack, GetAngInertia().x );

#endif

	return vecTorque;
}

void CPlane::CalculateLandingGearDrag( float fForwardSpd, float fOverallMultiplier )
{
	Vector3 drag = m_landingGear.GetDragMult(this) * -fForwardSpd * VEC3V_TO_VECTOR3( GetTransform().GetB() ) * GetMass();
#if __ASSERT
	{
		Vector3 vForce = drag*fOverallMultiplier;
		Assertf(vForce.Mag2() < square(150.0f * rage::Max(1.0f, GetMass())), 
			"ProcessFlightModel force over limit, force [%3.2f, %3.2f, %3.2f], dragMult %3.2f, spd %3.2f, overall %3.2f",
			vForce.x, vForce.y, vForce.z, m_landingGear.GetDragMult(this), fForwardSpd, fOverallMultiplier); 
	}
#endif

	ApplyInternalForceCg( drag * fOverallMultiplier );
}


void CPlane::CalculateLift( Vector3 vecAirSpeed, float fOverallMultiplier, float fPitchControl, float fVirtualFwdSpd, bool bHadCollision )
{
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();

	Vector3 vecTransformA = VEC3V_TO_VECTOR3( GetTransform().GetA() );
	Vector3 vecTransformB = VEC3V_TO_VECTOR3( GetTransform().GetB() );
	Vector3 vecTransformC = VEC3V_TO_VECTOR3( GetTransform().GetC() );

	// LIFT
	// calculate main wing angle of attack
	float fAngleOfAttack = DotProduct( vecTransformC, vecAirSpeed ) / rage::Max( 0.01f, vecAirSpeed.Mag() );
	fAngleOfAttack = -1.0f * rage::Asinf( Clamp( fAngleOfAttack, -1.0f, 1.0f ) );
	fAngleOfAttack *= 1.0f + ( Max( 0.0f, ( Abs( vecTransformA.GetZ() )  - Abs( fPitchControl ) ) ) * pFlyingHandling->m_fExtraLiftWithRoll );

	float fHealthMult = m_aircraftDamage.GetLiftMult(this);
	// Also take into account landing gear
	fHealthMult *= m_landingGear.GetLiftMult(this);

	float fFormLift = pFlyingHandling->m_fFormLiftMult;

	float fAttackMult = pFlyingHandling->m_fAttackLiftMult;
	if(vecTransformC.z < 0.0f) // If strictly upside down (IsUpsideDown function is too fuzzy)
	{
		fAttackMult = fAngleOfAttack >= 0.0f ? pFlyingHandling->m_fAttackLiftMult : pFlyingHandling->m_fAttackDiveMult;
	}

	float fVirtualForwardSpdSqr = fVirtualFwdSpd * fVirtualFwdSpd;

	if( !bHadCollision )
	{
		// If the lift force magnitude going to be less than the gravity, we virtually boost up the forward speed to generate the lift
		// force that its magnitude is closer to the gravity. This prevents the unexpected dropping during the flight
		float fSpeedSqrRequiredForLift = 1.0f / Max(Abs(fFormLift + fAttackMult*fAngleOfAttack), SMALL_FLOAT);
		if(IsInAir() && fSpeedSqrRequiredForLift > fVirtualForwardSpdSqr && vecTransformC.z > 0.0f)
		{
			if(square(fVirtualFwdSpd + sfPlaneVirtualLiftSpeedLimit) >= fSpeedSqrRequiredForLift)
			{
				fVirtualForwardSpdSqr = fSpeedSqrRequiredForLift;
			}
		}
	}
	float fMass = GetMass();
	float fWingForce = ( fFormLift + fAttackMult * fAngleOfAttack ) * fVirtualForwardSpdSqr * fMass * m_fGravityForWheelIntegrator;

	fWingForce *= fHealthMult;

	fWingForce = rage::Clamp( fWingForce, -PLANE_WING_MAX_FORCE * GetMass(), PLANE_WING_MAX_FORCE * GetMass() );

	Vector3 vecLiftForce = fWingForce*vecTransformC;

	// Cut the lift force if plane flies over the height limit
	const float fThisPositionZ = GetTransform().GetPosition().GetZf();
	float fHeightAboveCeiling = HeightAboveCeiling( fThisPositionZ );
	if( fHeightAboveCeiling > 0.0f )
	{
		vecLiftForce.z = Min( 0.0f, vecLiftForce.z );
		SetRocketBoosting( false );

		// More gravity need to be applied if plane further approaches to the world ceiling
		if( GetVelocity().z > 0.0f )
		{
			float fCeilingApproachingRate = fHeightAboveCeiling / ( WORLDLIMITS_ZMAX - fThisPositionZ + fHeightAboveCeiling );
			fCeilingApproachingRate = Clamp( fCeilingApproachingRate, 0.0f, 1.0f );

#if __ASSERT
			{
				Vector3 vForce = fMass * -m_fGravityForWheelIntegrator * 2.0f * fCeilingApproachingRate * ZAXIS;
				Assertf( vForce.Mag2() < square( 150.0f * rage::Max( 1.0f, GetMass() ) ), 
					"ProcessFlightModel force over limit, force [%3.2f, %3.2f, %3.2f], rate %3.2f",
					vForce.x, vForce.y, vForce.z, fCeilingApproachingRate ); 
			}
#endif

			ApplyInternalForceCg( fMass * -m_fGravityForWheelIntegrator * 2.0f * fCeilingApproachingRate * ZAXIS );
		}
	}

	// Cut the lift force if plane is flying backwards
	if( DotProduct( vecTransformB, vecAirSpeed ) < 0.0f )
	{
		vecLiftForce.z = Min( 0.0f, vecLiftForce.z );
	}

	// Cut the lift force if attack angle pass over the stall angle
	// Only stall the plane if its being flown by a player.
	if( GetDriver() && GetDriver()->IsAPlayerPed() )
	{
		if( fAngleOfAttack > PLANE_STALL_ANGLE && vecAirSpeed.z > PLANE_STALL_MIN_VERTICAL_SPEED )
		{
			vecLiftForce.z = Min( 0.0f, fWingForce, vecLiftForce.z );
		}
	}

#if __ASSERT
	{
		Vector3 vForce = vecLiftForce * fOverallMultiplier;
		Assertf( vForce.Mag2() < square(150.0f * rage::Max(1.0f, GetMass())), 
			"ProcessFlightModel force over limit, force [%3.2f, %3.2f, %3.2f], wingForce %3.2f, lift %3.2f, attactMult %3.2f, angle %3.2f, spdSqr %3.2f, overall %3.2f",
			vForce.x, vForce.y, vForce.z, fWingForce, fFormLift, fAttackMult, fAngleOfAttack, fVirtualForwardSpdSqr, fOverallMultiplier ); 
	}
#endif
	ApplyInternalForceCg( vecLiftForce * fOverallMultiplier );

#if __WIN32PC
	m_fWingForce = fWingForce;
#endif // __WIN32PC
}

float CPlane::GetGroundEffectOnDrag()
{
	float dragReduction = 0.0f;

	if( ms_ReduceDragWithGroundEffect && 
		IsInAir() )
	{
		float x = GetVehiclePosition().GetXf();
		float y = GetVehiclePosition().GetYf();

		float fMinHeightAtPos = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(x, y);
		float fMaxHeightAtPos = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(x, y);
		float fAvgHeightAtPos = 0.5f * ( fMinHeightAtPos + fMaxHeightAtPos );

		TUNE_GROUP_FLOAT( VEHICLE_PLANE_GROUND_EFFECT, HEIGHT_ABOVE_GROUND_SCALE, 1.25f, 0.0f, 10.0f, 0.1f );
		float boundSize = GetBoundRadius() * 2.0f;
		float distanceFromGround = GetVehiclePosition().GetZf() - ( boundSize * HEIGHT_ABOVE_GROUND_SCALE ) - fAvgHeightAtPos;

		if( distanceFromGround < 0.0f )
		{
			dragReduction = Max( 0.0f, 1.0f + ( distanceFromGround / boundSize ) );
		}
	}

	return dragReduction; 
}

void CPlane::ProcessFlightModel(float fTimeStep, float fOverallMultiplier)
{
	// better make sure we've got some handling data, otherwise we're screwed for flying
	CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
	if(pFlyingHandling == NULL)
	{
		return;
	}

	if(fTimeStep <= 0.0f)
	{
		return;
	}

	CControl *pControl = NULL;
	if(GetStatus()==STATUS_PLAYER && GetDriver() && GetDriver()->IsPlayer())
	{
		pControl = GetDriver()->GetControlFromPlayer();
	}

	bool bControlInactive = pControl && CTaskVehiclePlayerDrive::IsThePlayerControlInactive(pControl);
	bool bPlayerDriver = GetDriver() && GetDriver()->IsPlayer();

	Vector3 vecAirSpeed(0.0f, 0.0f, 0.0f);
	Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);
	if (!PopTypeIsMission())
	{
		WIND.GetLocalVelocity(GetTransform().GetPosition(), RC_VEC3V(vecWindSpeed), false, false);
		vecAirSpeed -= ComputeWindMult(0.0f)*vecWindSpeed + m_vTurbulenceAirflow;
	}
	vecAirSpeed += GetVelocity();

	float fMass = GetMass();

#if __BANK	
	if(GetStatus()==STATUS_PLAYER && CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
	{
		Vector3 vecWindSocPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(GetTransform().GetC()) * GetBaseModelInfo()->GetBoundingBoxMax().z;
		grcDebugDraw::Line(vecWindSocPos, vecWindSocPos + 10.0f*vecWindSpeed, Color32(0,0,255));
		grcDebugDraw::Line(vecWindSocPos, vecWindSocPos - 10.0f*vecAirSpeed, Color32(0,255,0));
	}
#endif

	// Get local copies so we can mess about with them
	float fPitchControl = m_fPitchControl;
	float fYawControl = m_fYawControl;
	float fRollControl = m_fRollControl;
	float fThrottleControl = Clamp(m_fThrottleControl, -dfPlaneThrottleControlLimit, dfPlaneThrottleControlLimit);
	float fVirtualSpeedControl = m_fVirtualSpeedControl;
	float fBrakeControl = Clamp(m_fAirBrakeControl, -dfPlaneBrakeControlLimit, dfPlaneBrakeControlLimit);

	Vector3 vecTransformB = VEC3V_TO_VECTOR3(GetTransform().GetB());

	if(fThrottleControl==FLY_INPUT_NULL)
	{
		fThrottleControl = 0.0f;
		if(pControl)
		{
			fThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();
		}
	}

	if( fRollControl == FLY_INPUT_NULL )
	{
		fRollControl = fYawControl;
	}

	// reset this
	m_fVirtualSpeedControl = 1.0f;

	// MISC STUFF
	Vector3 vecRudderArm = vecTransformB;
	Vector3 vecTailOffset = vecRudderArm;
	float rudderOffset = GetBaseModelInfo()->GetBoundingBoxMin().y;

	if( GetModelIndex() == MI_PLANE_VOLATOL ) 
	{
		static dev_float sfMaxRudderOffset = 18.0f;
		rudderOffset = Clamp( rudderOffset, -sfMaxRudderOffset, sfMaxRudderOffset );
	}
	vecTailOffset *= rudderOffset;

	float fForwardSpd = vecAirSpeed.Dot(vecRudderArm);
	if ( bPlayerDriver )
	{
		fForwardSpd = Clamp(fForwardSpd, -pHandling->m_fEstimatedMaxFlatVel, pHandling->m_fEstimatedMaxFlatVel); // Clamp the forward speed to prevent unexpected controlling force
	}

	// virtual speed is our speed scaled by our Virtual speed control
	// in general the Virtual speed should be a multiplier to scale the speed
	// to some value large enough that the plane would have enough lift to fly
	// I use the virtual speed for all torques and for the lift force calculation
	// the basic idea is the plane moves at correct rate but handles as if moving at
	// the min speed to stay in the air
	float fThrustHealthMult = m_aircraftDamage.GetThrustMult(this);
	float fVirtualFwdSpd = fForwardSpd * fVirtualSpeedControl;
	fVirtualFwdSpd = Clamp(fVirtualFwdSpd, -pHandling->m_fEstimatedMaxFlatVel, pHandling->m_fEstimatedMaxFlatVel); // Clamp the forward speed to prevent unexpected controlling force
	float fTopSpeed = pHandling->m_fEstimatedMaxFlatVel;
	float fCurrentSpeed = vecAirSpeed.Mag();

	CalculateYawAndPitchcontrol( pControl, fYawControl, fPitchControl );

	// Apply the driver influence to controls
	if(bPlayerDriver && IsInAir(false) && !bControlInactive && !m_nVehicleFlags.bUsedForPilotSchool BANK_ONLY(&& !CVehicleFactory::ms_bflyingAce))
	{
		ModifyControlsBasedOnFlyingStats(GetDriver(), pFlyingHandling, fYawControl, fPitchControl, fRollControl, fThrottleControl, fTimeStep);
	}

	// For player, the braking is done by applying negative throttle. No brake control needed for player driven plane.
	if(bPlayerDriver && !bControlInactive)
	{
		fBrakeControl = 0.0f;
	}

	// DAMAGE EFFECT
	float fPitchFromDamageEffect = 0.0f;
	float fYawFromDamageEffect = 0.0f;
	float fRollFromDamageEffect = 0.0f;

	CalculateDamageEffects( fPitchFromDamageEffect, fYawFromDamageEffect, fRollFromDamageEffect, fPitchControl, fYawControl, fRollControl );
	CalculateThrust( fThrottleControl, fYawControl, fForwardSpd, fThrustHealthMult, fOverallMultiplier, bPlayerDriver );

	static float s_fAirBrakeMult = 0.01f;
	Vector3 vAirBrake( -vecTransformB * m_fGravityForWheelIntegrator * fBrakeControl * fMass * fThrustHealthMult * fOverallMultiplier * Max(fForwardSpd, 0.0f) * s_fAirBrakeMult);
	ApplyInternalForceCg(vAirBrake);

	TUNE_GROUP_BOOL( STUNT_PLANE, REDUCE_ROLL_WITH_PITCH_INPUT, true );
	TUNE_GROUP_BOOL( STUNT_PLANE, USE_STUNT_PLANE_ROLL, false );
	TUNE_GROUP_FLOAT( STUNT_PLANE, REDUCE_ROLL_WITH_PITCH_INPUT_AMOUNT, 0.3f, 0.0f, 2.0f, 0.1f );

	if( REDUCE_ROLL_WITH_PITCH_INPUT && 
		pFlyingHandling->m_fExtraLiftWithRoll )
	{
		fRollControl *= Max( 0.0f, Abs( fRollControl ) - Abs( fPitchControl * REDUCE_ROLL_WITH_PITCH_INPUT_AMOUNT ) );
	}

	CalculateSideSlip( vecAirSpeed, fOverallMultiplier );

	Vector3 vecTorque = CalculateYaw( vecTailOffset, vecWindSpeed, fOverallMultiplier, fYawControl, fVirtualFwdSpd, fYawFromDamageEffect );

	if( !USE_STUNT_PLANE_ROLL )
	{
		vecTorque += CalculateRoll( fOverallMultiplier, fRollControl, fRollFromDamageEffect, fVirtualFwdSpd );
	}
	else
	{
		vecTorque += CalculateStuntRoll( fOverallMultiplier, fRollControl, fTimeStep );
	}

	bool bHadCollision = GetFrameCollisionHistory() && GetFrameCollisionHistory()->GetCollisionImpulseMagSum() > 0.0f;
	bool bClampAngleOfAttack = true;

	if(!NetworkInterface::IsGameInProgress() && GetDriver() && GetDriver()->IsLocalPlayer())
	{
		bClampAngleOfAttack = bHadCollision || fCurrentSpeed <= fTopSpeed;
	}

	if( !bHadCollision )
	{
		vecTorque += ApplyRollStabilisation( fOverallMultiplier );
	}

	vecTorque += CalculatePitch( vecWindSpeed, vecTailOffset, vecAirSpeed, fOverallMultiplier, fPitchControl, fPitchFromDamageEffect, fVirtualFwdSpd, bClampAngleOfAttack );

//	static dev_bool restrictMaxTorqueDelta = true;
//
//	if( restrictMaxTorqueDelta &&
//		GetModelIndex() == MI_PLANE_VOLATOL )
//	{
//		static Vector3 maxTorque( 10.0, 10.0f, 10.0f );
//		Vector3 maxTorqueDelta = GetAngInertia() * maxTorque;
//
//		for( int i = 0; i < 3; i++ )
//		{
//			//vecTorque[ i ] = Clamp( vecTorque[ i ], m_prevTorque[ i ] - maxTorqueDelta[ i ], m_prevTorque[ i ] + maxTorqueDelta[ i ] );
//			vecTorque[ i ] = Clamp( vecTorque[ i ], -maxTorqueDelta[ i ], maxTorqueDelta[ i ] );
//		}
//
////		m_prevTorque = vecTorque;
//	}


	ApplyInternalTorque(vecTorque);


	CalculateLift( vecAirSpeed, fOverallMultiplier, fPitchControl, fVirtualFwdSpd, bHadCollision );

	CalculateLandingGearDrag( fForwardSpd, fOverallMultiplier );

 }


void CPlane::DriveDesiredVerticalFlightModeRatio( float driveToVerticalFlightMode )
{
    //Work out the position of the nozzles for vertical flight mode use
    m_fDesiredVerticalFlightRatio += driveToVerticalFlightMode * sf_VerticalFlightTransitionSpeed * fwTimer::GetTimeStep();

    m_fDesiredVerticalFlightRatio = rage::Clamp(m_fDesiredVerticalFlightRatio, 0.0f, 1.0f);
}


void CPlane::SetDesiredVerticalFlightModeRatio(float verticalFlightModeRatio)
{
    Assert( verticalFlightModeRatio >= 0.0f && verticalFlightModeRatio <= 1.0f);

#if !__NO_OUTPUT
	if( NetworkInterface::IsGameInProgress() && !IsNetworkClone() && GetNetworkObject() &&
		verticalFlightModeRatio != verticalFlightModeRatio)
	{
		Displayf("[CPlane::SetDesiredVerticalFlightModeRatio] new: %.2f old: %.2f", verticalFlightModeRatio, m_fDesiredVerticalFlightRatio );
		sysStack::PrintStackTrace();
	}
#endif //  !__NO_OUTPUT

    m_fDesiredVerticalFlightRatio = verticalFlightModeRatio;
}

void CPlane::SetVerticalFlightModeRatio(float verticalFlightModeRatio)
{
	Assert( verticalFlightModeRatio >= 0.0f && verticalFlightModeRatio <= 1.0f);
	m_fDesiredVerticalFlightRatio = verticalFlightModeRatio;
	m_fCurrentVerticalFlightRatio = verticalFlightModeRatio;
}

bool CPlane::GetVerticalFlightModeAvaliable() const
{
	if( GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_VERTICAL_FLIGHT_MODE ) )
	{
		return true;
	}

    s32 nBoneIndex = GetBoneIndex((eHierarchyId)PLANE_NOZZLE_F);

    if( nBoneIndex < 0 &&
		( !MI_PLANE_TULA.IsValid() ||
		  GetModelIndex() != MI_PLANE_TULA ) )
	{
        return false;
	}

    return true;
}

const bool CPlane::IsInVerticalFlightMode() const
{ 
	if (GetVerticalFlightModeAvaliable()) 
	{ 
		return (GetVerticalFlightModeRatio() >= 1.0f);
	} 
	
	return false; 
} 

dev_float fDeepWaterResistanceV = 2.0f;
dev_float fDeepWaterResistanceV2 = 1.0f;
dev_float fDeepWaterResistanceDepth = -100.0f;

dev_float fShallowWaterResistanceV = 1.0f;
dev_float fShallowWaterResistanceV2 = 0.1f;
dev_float fShallowWaterResistanceDepth = -20.0f;

void CPlane::SetDampingForFlight(float fRatioOfVerticalFlightToUse)
{
    CFlyingHandlingData* pFlyingHandling = pHandling->GetFlyingHandlingData();
    if(pFlyingHandling == NULL)
    {
        return;
    }

    bool bIsArticulated = false;
    if(GetCollider())
    {
        bIsArticulated = GetCollider()->IsArticulated();
    }

    float fDampingMultiplier = 1.0f;

    //Increase damping for certain characters
	//static dev_float sfDampingPilotSchool = 1.0f;
	if(GetDriver())
	{
		StatId stat = STAT_FLYING_ABILITY.GetStatId();
		float fFlyingStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
		float fMinPlaneDamping = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MinPlaneDamping;
		float fMaxPlaneDamping = CPlayerInfo::GetPlayerStatInfoForPed(*GetDriver()).m_MaxPlaneDamping;

		// Swapped the max damping and min damping, as better skills should drive faster, which means less damping
		fDampingMultiplier = ((1.0f - fFlyingStatValue) * fMaxPlaneDamping + fFlyingStatValue * fMinPlaneDamping)/100.0f;
	}
    float fLV = 0.0f;
    Vector3 vecAV(0.0f, 0.0f, 0.0f);
    Vector3 vecSpeedRes(0.0f, 0.0f, 0.0f);
    float fRatioOfNormalFlightToUse = 1.0f;

	// drop the top speed of the fast planes by 15% when close to the ground - B*655365
	float fTopSpeed = pHandling->m_fEstimatedMaxFlatVel;
	float fCurrentSpeed = GetVelocity().Mag();
	float fSpeedRate = fCurrentSpeed / fTopSpeed;
	float fCurrentHeight = GetTransform().GetPosition().GetZf();

	if(fSpeedRate > (1.0f - ms_fTopSpeedDampRate) && fCurrentHeight < ms_fTopSpeedDampMaxHeight)
	{
		float fHeightMultiplier = (ms_fTopSpeedDampMaxHeight - fCurrentHeight) / (ms_fTopSpeedDampMaxHeight - ms_fTopSpeedDampMinHeight);
		fHeightMultiplier = Clamp(fHeightMultiplier, 0.0f, 1.0f);
		float fSpeedMultiplier = (fSpeedRate - (1.0f - ms_fTopSpeedDampRate)) / ms_fTopSpeedDampRate;
		fSpeedMultiplier = Clamp(fSpeedMultiplier, 0.0f, 1.0f);
		fLV += ms_fTopSpeedDampRate * fHeightMultiplier * fSpeedMultiplier;
	}

    //Lerp between damping settings when changing between vertical and normal flight mode
    if(GetVerticalFlightModeAvaliable())
    {
        CFlyingHandlingData* pVerticalFlyingHandling = pHandling->GetVerticalFlyingHandlingData();
        if(pVerticalFlyingHandling == NULL)
        {
            return;
        }

        fRatioOfNormalFlightToUse = 1.0f - fRatioOfVerticalFlightToUse;

        float fLVNormal =  bIsArticulated ? 0.5f*pFlyingHandling->m_fMoveRes : pFlyingHandling->m_fMoveRes;
        float fLVVertical =  bIsArticulated ? 0.5f*pVerticalFlyingHandling->m_fMoveRes : pVerticalFlyingHandling->m_fMoveRes;

        fLV += ((fLVNormal*fRatioOfNormalFlightToUse) + (fLVVertical*fRatioOfVerticalFlightToUse)) * fDampingMultiplier;

        if(MagSquared(pFlyingHandling->m_vecTurnRes).Getf() > 0.0f || MagSquared(pVerticalFlyingHandling->m_vecTurnRes).Getf() > 0.0f)
        {
            Vector3 vecAVNormal =  bIsArticulated ? 0.5f*RCC_VECTOR3(pFlyingHandling->m_vecTurnRes) : RCC_VECTOR3(pFlyingHandling->m_vecTurnRes);
            Vector3 vecAVVertical =  bIsArticulated ? 0.5f*RCC_VECTOR3(pVerticalFlyingHandling->m_vecTurnRes) : RCC_VECTOR3(pVerticalFlyingHandling->m_vecTurnRes);

            vecAV = ((vecAVNormal*fRatioOfNormalFlightToUse) + (vecAVVertical*fRatioOfVerticalFlightToUse)) * fDampingMultiplier;
        }

        if(MagSquared(pFlyingHandling->m_vecSpeedRes).Getf() > 0.0f || MagSquared(pVerticalFlyingHandling->m_vecSpeedRes).Getf() > 0.0f)
        {
            Vector3 vecSpeedResNormal = RCC_VECTOR3(pFlyingHandling->m_vecSpeedRes);
            Vector3 vecSpeedResVertical = RCC_VECTOR3(pVerticalFlyingHandling->m_vecSpeedRes);

            vecSpeedRes = ((vecSpeedResNormal*fRatioOfNormalFlightToUse) + (vecSpeedResVertical*fRatioOfVerticalFlightToUse)) * fDampingMultiplier;
        }
    }
    else
    {
        fLV += (bIsArticulated ? 0.5f*pFlyingHandling->m_fMoveRes : pFlyingHandling->m_fMoveRes) * fDampingMultiplier;

        if(MagSquared(pFlyingHandling->m_vecTurnRes).Getf() > 0.0f )
        {
            vecAV = (bIsArticulated ? 0.5f*RCC_VECTOR3(pFlyingHandling->m_vecTurnRes) : RCC_VECTOR3(pFlyingHandling->m_vecTurnRes)) * fDampingMultiplier;
        }

        if(MagSquared(pFlyingHandling->m_vecSpeedRes).Getf() > 0.0f)
        {
            vecSpeedRes = RCC_VECTOR3(pFlyingHandling->m_vecSpeedRes) * fDampingMultiplier;
        }
    }

    //Damp roll if we aren't trying to roll
    static dev_float sfRollNoInput = 0.01f;
    if(rage::Abs(GetRollControl()) < sfRollNoInput)
    {
        static dev_float sfExtraRollDamp = 4.0f;
        vecAV.y *= 1.0f + sfExtraRollDamp * fRatioOfNormalFlightToUse;
        vecSpeedRes.y *= 1.0f + sfExtraRollDamp * fRatioOfNormalFlightToUse;
    }

	if(GetIsInWater())
	{
		if(GetVehiclePosition().GetZf() < fShallowWaterResistanceDepth)
		{
			fLV = Max(fLV, fShallowWaterResistanceV);
		}

		if(GetVehiclePosition().GetZf() < fDeepWaterResistanceDepth)
		{
			fLV = Max(fLV, fDeepWaterResistanceV);
		}
	}

	Vector3 vLV = Vector3(fLV, fLV, fLV);
	// reset the damping if plane is wrecked or lost all its wings, so it can fall like rock
	if(!GetIsInWater() && (GetStatus() == STATUS_WRECKED || (m_aircraftDamage.HasSectionBrokenOff(this,CAircraftDamage::WING_L) && m_aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_R))))
	{
		vLV.z = 0.0f;
		vecAV = Vector3(CVehicleModelInfo::STD_VEHICLE_ANGULAR_V_COEFF, CVehicleModelInfo::STD_VEHICLE_ANGULAR_V_COEFF, CVehicleModelInfo::STD_VEHICLE_ANGULAR_V_COEFF);
		vecSpeedRes = Vector3(CVehicleModelInfo::STD_VEHICLE_ANGULAR_V_COEFF, CVehicleModelInfo::STD_VEHICLE_ANGULAR_V_COEFF, CVehicleModelInfo::STD_VEHICLE_ANGULAR_V_COEFF);
	}

	phArchetypeDamp* pArch = (phArchetypeDamp*)GetVehicleFragInst()->GetArchetype();

    // Now setup the damping
    pArch->ActivateDamping(phArchetypeDamp::LINEAR_V, vLV);

	CPlayerInfo *pPlayer = GetDriver() ? GetDriver()->GetPlayerInfo() : NULL;
	float fLV2 = 0.0f;
	if(GetIsInWater())
	{
		if(GetVehiclePosition().GetZf() < fShallowWaterResistanceDepth)
		{
			fLV2 = Max(fLV2, fShallowWaterResistanceV2);
		}

		if(GetVehiclePosition().GetZf() < fDeepWaterResistanceDepth)
		{
			fLV2 = Max(fLV2, fDeepWaterResistanceV2);
		}
	}
	else
	{
		fLV2 = m_fDragCoeff;
		if (pPlayer && pPlayer->m_fForceAirDragMult > 0.0f)
		{
			fLV2 = m_fDragCoeff * pPlayer->m_fForceAirDragMult;
		}
	}

	Vector3 vecLinearV2Damping;
	vecLinearV2Damping.Set(fLV2, fLV2, fLV2);
	TUNE_GROUP_FLOAT(VEHICLE_DAMPING, PLANE_V2_DAMP_MULT, 1.0f, 0.0f, 5.0f, 0.01f);

	if( GetDriver() &&
		GetDriver()->IsAPlayerPed() )
	{
		float fSlipStreamEffect = GetSlipStreamEffect();
		float fGroundEffect = GetGroundEffectOnDrag();

		vecLinearV2Damping -= (vecLinearV2Damping * Min( 1.0f, fSlipStreamEffect + fGroundEffect ) );
	}

	pArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, vecLinearV2Damping * PLANE_V2_DAMP_MULT);

    if(vecAV.Mag2() > 0.0f)
    {
        pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V, vecAV);
    }

    if(vecSpeedRes.Mag2() > 0.0f )
    {
        pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V2, vecSpeedRes);
    }
}

dev_float PLANE_VERTICAL_MINIMUM_CONTROL_SPEED = (1.0f);
dev_float PLANE_VERTICAL_RUDDER_MAX_ANGLE_OF_ATTACK = ( DtoR * 30.0f);
void CPlane::ProcessVerticalFlightModel(float fTimeStep, float fOverallMultiplier)
{
	int numberOfContactWheels		= GetNumContactWheels();
	int minNumberOfContactWheels	= (int)( (float)GetNumWheels() * 0.5f ) + 1;

	const Vec3V velocity = VECTOR3_TO_VEC3V(GetVelocity());
    if( numberOfContactWheels >= minNumberOfContactWheels && m_fThrottleControl == 0.0f && 
		IsLessThanOrEqualAll( Abs( velocity ), Vec3VFromF32( PLANE_VERTICAL_MINIMUM_CONTROL_SPEED ) ) )
    {
        return;
    }

	if(!IsEngineOn())
	{
		return;
	}

    // better make sure we've got some handling data, otherwise we're screwed for flying
    CFlyingHandlingData* pFlyingHandling = pHandling->GetVerticalFlyingHandlingData();
    if(pFlyingHandling == NULL)
        return;

    if(fTimeStep <= 0.0f)
        return;

    CControl *pControl = NULL;
    if(GetStatus()==STATUS_PLAYER && GetDriver() && GetDriver()->IsPlayer())
        pControl = GetDriver()->GetControlFromPlayer();

    Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

    Vector3 vecAirSpeed(0.0f, 0.0f, 0.0f);
    Vector3 vecWindSpeed(0.0f, 0.0f, 0.0f);
    if (!PopTypeIsMission())
    {
        WIND.GetLocalVelocity(RCC_VEC3V(vThisPosition), RC_VEC3V(vecWindSpeed), false, false);
        vecAirSpeed -= ComputeWindMult(1.0f)*vecWindSpeed + m_vTurbulenceAirflow;
    }
    vecAirSpeed += GetVelocity();

    float fMass = GetMass();
    Vector3 vecAngInertia = GetAngInertia();

#if __BANK	
    if(GetStatus()==STATUS_PLAYER && CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
    {
        Vector3 vecWindSocPos = vThisPosition + VEC3V_TO_VECTOR3(GetTransform().GetC()) * GetBaseModelInfo()->GetBoundingBoxMax().z;
        grcDebugDraw::Line(vecWindSocPos, vecWindSocPos + 10.0f*vecWindSpeed, Color32(0,0,255));
        grcDebugDraw::Line(vecWindSocPos, vecWindSocPos - 10.0f*vecAirSpeed, Color32(0,255,0));
    }
#endif

    // Get local copies so we can mess about with them
    float fPitchControl = GetPitchControl();
    float fYawControl = GetYawControl();
    float fRollControl = -GetRollControl();
    float fThrottleControl = GetThrottleControl();

    {
        // CONTROLS
        if(fPitchControl==FLY_INPUT_NULL)
        {
            fPitchControl = 0.0f;
            if(pControl)
                fPitchControl = pControl->GetVehicleFlyPitchUpDown().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
        }
        if(fRollControl==FLY_INPUT_NULL)
        {
            fRollControl = 0.0f;
            if(pControl)
                fRollControl = pControl->GetVehicleFlyRollLeftRight().GetNorm(ioValue::ALWAYS_DEAD_ZONE);
        }
        if(fYawControl==FLY_INPUT_NULL)
        {
            fYawControl = 0.0f;
            if(pControl)
			{
				// commented out as there are no mappings to these functions but I have left them here so they are easy to add again.
//                 if(pControl->GetVehicleLookRight().IsDown() && !pControl->GetVehicleLookLeft().IsDown())
//                     fYawControl = 1.0f;
//                 if(pControl->GetVehicleLookLeft().IsDown() && !pControl->GetVehicleLookRight().IsDown())	
//                     fYawControl = -1.0f; 
                // 2nd stick controls option for chris
                if(ABS(pControl->GetVehicleGunLeftRight().GetNorm()) > 0.008f)
                    fYawControl = pControl->GetVehicleGunLeftRight().GetNorm();
            }
        }
        if(fThrottleControl==FLY_INPUT_NULL)
        {
            fThrottleControl = 0.0f;
            if(pControl)
                fThrottleControl = pControl->GetVehicleFlyThrottleUp().GetNorm01() - pControl->GetVehicleFlyThrottleDown().GetNorm01();
        }

		// Apply the driver influence to controls
		if(GetDriver() && GetDriver()->IsPlayer() && !m_nVehicleFlags.bUsedForPilotSchool BANK_ONLY(&& !CVehicleFactory::ms_bflyingAce))
		{
			ModifyControlsBasedOnFlyingStats(GetDriver(), pFlyingHandling, fYawControl, fPitchControl, fRollControl, fThrottleControl, fTimeStep);
		}

        ////////////////////////
        // ADDED NOISE

        if(IsInAir())
        {
			static dev_float sfRandomRollNoiseLower = -1.5f;//prefer the random noise moving the plane forward then back
			static dev_float sfRandomRollNoiseUpper = 1.0f; 

			float fRandomNoiseIdleMult		= sfPlaneRandomNoiseIdleMultForAI;
			float fRandomNoiseThrottleMult	= sfPlaneRandomNoiseThrottleMultForAI;

			if(GetDriver() && GetDriver()->IsPlayer())
			{
				fRandomNoiseIdleMult	 = sfPlaneRandomNoiseIdleMultForPlayer;
				fRandomNoiseThrottleMult = sfPlaneRandomNoiseThrottleMultForPlayer;
			}

			float fDamageRate = 1.0f - GetHealth()/GetMaxHealth();
			fDamageRate = Clamp(fDamageRate, 0.0f, 1.0f);

			float fRandomNoiseMult = fRandomNoiseIdleMult + fThrottleControl * fRandomNoiseThrottleMult + fDamageRate * pFlyingHandling->m_fBodyDamageControlEffectMult;

			float fRandomRollNoise = fwRandom::GetRandomNumberInRange( sfRandomRollNoiseLower*fRandomNoiseMult, sfRandomRollNoiseUpper*fRandomNoiseMult);
			ApplyInternalTorque(fRandomRollNoise*ZAXIS*vecAngInertia.x, VEC3V_TO_VECTOR3(GetTransform().GetB()));

			float fRandomPitchNoise = fwRandom::GetRandomNumberInRange( sfRandomRollNoiseLower*fRandomNoiseMult, sfRandomRollNoiseUpper*fRandomNoiseMult);
			ApplyInternalTorque(fRandomPitchNoise*ZAXIS*vecAngInertia.y, VEC3V_TO_VECTOR3(GetTransform().GetA()));

        }

		float fPitchFromDamageEffect = 0.0f;
		float fYawFromDamageEffect = 0.0f;
		float fRollFromDamageEffect = 0.0f;

		static dev_float sfRollControlLostWingEffect = 1.5f;
		static dev_float sfRollControlFlapDamageEffect = 15.0f;
		static dev_float sfPitchControlAileronDamageEffect = 0.0f;
		static dev_float sfYawControlRudderDamageEffect = 3.0f;
		static dev_float sfControlBodyDamageEffectMulti = 0.1f;
		static dev_float sfHoverThrottleWhenOnGroundMulti = 0.5f;

		if(IsInAir())
		{
			// Wing Damage
			//if we're in the air and we've lost a wing make the plane spin.
			if(GetAircraftDamage().HasSectionBrokenOff(this, CAircraftDamage::WING_L))
			{
				fRollFromDamageEffect -= sfRollControlLostWingEffect;
			}
			else if(GetAircraftDamage().HasSectionBrokenOff(this, CAircraftDamage::WING_R))
			{
				fRollFromDamageEffect += sfRollControlLostWingEffect;
			}

			float fBodyHealthMult = 0.0f;
			if(GetHealth() < sfBuffetingBodyHealthThreshold)
			{
				fBodyHealthMult = 1.0f - GetHealth() / GetMaxHealth();
				fBodyHealthMult = Clamp(fBodyHealthMult, 0.0f, 1.0f);
				fBodyHealthMult *= sfControlBodyDamageEffectMulti;
			}

			// Flap (pitch) Damage causes rolling noise
			float fHealthMult = Max(1.0f - m_aircraftDamage.GetPitchMult(this), fBodyHealthMult);
			fRollFromDamageEffect += fwRandom::GetRandomNumberInRange(-1.0f, 1.0f) * sfRollControlFlapDamageEffect * fHealthMult * (0.5f * fPitchControl + 0.5f);

			// Aileron (roll) Damage causes pitching noise
			fHealthMult = Max(1.0f - m_aircraftDamage.GetRollMult(this), fBodyHealthMult);
			fPitchFromDamageEffect += fwRandom::GetRandomNumberInRange(-1.0f, 1.0f) * sfPitchControlAileronDamageEffect * fHealthMult * (0.5f * fRollControl + 0.5f);

			// Rudder (yaw) Damage causes yaw noise
			fHealthMult = Max(1.0f - m_aircraftDamage.GetYawMult(this), fBodyHealthMult);
			fYawFromDamageEffect += fwRandom::GetRandomNumberInRange(-1.0f, 1.0f) * sfYawControlRudderDamageEffect * fHealthMult * (0.5f * fYawControl + 0.5f);
		}


        ///////////////////////
        // LIFT&THRUST
        Vector3 vecLift(VEC3V_TO_VECTOR3(GetTransform().GetC()));

        float fSpeedThroRotor = vecAirSpeed.Dot(vecLift);
        if(fSpeedThroRotor < 0.0f)
            fSpeedThroRotor *= 2.0f;

        // copy input argument to local var
        float fThrottleMult = fThrottleControl;
        if(fThrottleMult > 1.0f)
            fThrottleMult = 1.0f + (fThrottleMult - 1.0f)*pFlyingHandling->m_fThrust;

        // limit height that this plane can fly
        vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());	//	Just in case the position has changed since the start of the function
   
		float fHealthMult = m_aircraftDamage.GetThrustMult(this);

		//Remove thrust based on how many propellers are alive
		float fPropMult = 1.0f;
		for(int i = 0; i< m_iNumPropellers; i++)
		{
			if( m_fPropellerHealth[i] <= 0.0f )
			{
				fPropMult -= (1.0f/m_iNumPropellers);
			}
		}

		fThrottleMult *= fHealthMult * fPropMult;
        fThrottleMult -= (pFlyingHandling->m_fThrustFallOff*fSpeedThroRotor);
        fThrottleMult = Clamp( fThrottleMult, -2.0f, 2.0f );

		//make the plane auto hover
        float hoverThrottle = 1.0f - fThrottleMult;
        hoverThrottle = Max( 0.0f, hoverThrottle );

		float propellerSpeed = 0.0f;
		for(int nPropIndex = 0 ; nPropIndex < PLANE_NUM_PROPELLERS; nPropIndex++ )
		{
			eHierarchyId nId = (eHierarchyId)(PLANE_PROP_1 + nPropIndex);
			if( GetBoneIndex(nId) > -1 )
			{
				// Found a valid propeller
				propellerSpeed += m_propellers[ nPropIndex ].GetSpeed();
			}
		}	

		if( GetNumContactWheels() )
		{
			hoverThrottle *= sfHoverThrottleWhenOnGroundMulti;
		}
		if( m_nFlags.bPossiblyTouchesWater )
		{
			hoverThrottle = 0.0f;
		}

        hoverThrottle = Clamp(hoverThrottle, 0.0f, 1.0f);
		hoverThrottle *= fHealthMult;

		if( m_iNumPropellers > 0 &&
			propellerSpeed == 0.0f )
		{
			hoverThrottle = 0.0f;
		}

		Vector3 hoverForce = vecLift*m_fGravityForWheelIntegrator*hoverThrottle*fHealthMult*fMass*fOverallMultiplier;
        ApplyInternalForceCg( hoverForce );

		if( fThrottleMult > 0.0f && HeightAboveCeiling( vThisPosition.z ) > 0.0f )
		{
			fThrottleMult *= 10.0f / ( HeightAboveCeiling( vThisPosition.z ) + 10.0f );
		}

        //Normal lift force
		if( m_iNumPropellers > 0 &&
			propellerSpeed == 0.0f )
		{
			fThrottleMult = 0.0f;
		}

		Vector3 liftForce = vecLift*m_fGravityForWheelIntegrator*pFlyingHandling->m_fThrust*fThrottleMult*fMass*fOverallMultiplier;
		ApplyInternalForceCg( liftForce );
		
		if( fPropMult != 1.0f )
		{
			Vector3 averageActivePropOffset( 0.0f, 0.0f, 0.0f );
			int numActiveProps = 0;
			for(int i = 0; i< m_iNumPropellers; i++)
			{
				if( m_fPropellerHealth[i] > 0.0f )
				{
					averageActivePropOffset += GetObjectMtx( m_propellers[ i ].GetBoneIndex() ).d;
					numActiveProps++;
				}
			}

			if( numActiveProps > 0 )
			{
				averageActivePropOffset /= (float)numActiveProps;
				ApplyInternalTorque( liftForce, averageActivePropOffset );
			}
		}


        // Additional force when pitching or rolling
        vecLift = -fPitchControl * VEC3V_TO_VECTOR3(GetTransform().GetB());
        vecLift.z = 0.0f;
        ApplyInternalForceCg(vecLift*m_fGravityForWheelIntegrator*fThrottleMult*fMass*fOverallMultiplier);

        vecLift = -fRollControl * VEC3V_TO_VECTOR3(GetTransform().GetA());
        vecLift.z = 0.0f;
        ApplyInternalForceCg(vecLift*m_fGravityForWheelIntegrator*fThrottleMult*fMass*fOverallMultiplier);

        // only apply stabilising force when plane is right way up and not losing any wings
		float fPitchFromVelocityDamping = 0.0f;
		float fRollFromVelocityDamping = 0.0f;

        float fForceOffset = 0.0f;

        if(GetTransform().GetC().GetZf() > 0.0f && !GetAircraftDamage().HasSectionBrokenOff(this, CAircraftDamage::WING_L) && !GetAircraftDamage().HasSectionBrokenOff(this, CAircraftDamage::WING_R))
        {
            Vector3 vecTempUp(0.0f,0.0f,1.0f);
            vecTempUp += ComputeWindMult(1.0f)*vecWindSpeed + m_vTurbulenceAirflow;
            vecTempUp.Normalize();

            Vector3 vecRight(VEC3V_TO_VECTOR3(GetTransform().GetA()));
            Vector3 vecUp(VEC3V_TO_VECTOR3(GetTransform().GetC()));
            fForceOffset = -Clamp(vecRight.Dot(vecTempUp), -pFlyingHandling->m_fFormLiftMult, pFlyingHandling->m_fFormLiftMult);
            ApplyInternalTorque(pFlyingHandling->m_fAttackLiftMult*fForceOffset*vecAngInertia.y*vecUp*fOverallMultiplier, vecRight);

            vecUp = VEC3V_TO_VECTOR3(GetTransform().GetC()); // not sure if ApplyInternalTorque could have changed GetC()
            Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
            fForceOffset = -Clamp(vecForward.Dot(vecTempUp), -pFlyingHandling->m_fFormLiftMult, pFlyingHandling->m_fFormLiftMult);
            ApplyInternalTorque(pFlyingHandling->m_fAttackLiftMult*fForceOffset*vecAngInertia.x*vecUp*fOverallMultiplier, vecForward);

			// Add some tilt to help damping velocity when no control is given
			static float sfVelocityDampingPitchMult = 1.0f;
			static float sfVelocityDampingRollMult = 1.0f;
			static float sfVelocityDampingTiltDampingSpeed = 10.0f;
			if(fPitchControl == 0.0f && fRollControl == 0.0f)
			{
				float fSideSpeed = DotProduct(vecAirSpeed, VEC3V_TO_VECTOR3(GetTransform().GetA()));
				float fFwdSpeed = DotProduct(vecAirSpeed, VEC3V_TO_VECTOR3(GetTransform().GetB()));
				fPitchFromVelocityDamping = Sign(fFwdSpeed) * sfVelocityDampingPitchMult * Min(Abs(fFwdSpeed) / sfVelocityDampingTiltDampingSpeed, 1.0f);
				fRollFromVelocityDamping = Sign(fSideSpeed) * sfVelocityDampingRollMult * Min(Abs(fSideSpeed) / sfVelocityDampingTiltDampingSpeed, 1.0f);
			}
        }

        Vector3 vecUp(VEC3V_TO_VECTOR3(GetTransform().GetC()));
        Vector3 vecForward(VEC3V_TO_VECTOR3(GetTransform().GetB()));
        ApplyInternalTorque(vecUp*(fPitchControl + fPitchFromDamageEffect + fPitchFromVelocityDamping)*pFlyingHandling->m_fPitchMult*vecAngInertia.x*fOverallMultiplier, vecForward);

        vecUp = VEC3V_TO_VECTOR3(GetTransform().GetC()); // not sure if ApplyInternalTorque could have changed GetC()
        Vector3 vecRight(VEC3V_TO_VECTOR3(GetTransform().GetA()));
        ApplyInternalTorque(vecUp*(fRollControl + fRollFromDamageEffect + fRollFromVelocityDamping)*pFlyingHandling->m_fRollMult*vecAngInertia.y*fOverallMultiplier, vecRight);


        {
            Vector3 vecTailAirSpeed(VEC3_ZERO);
            // get tail position and add contribution from rotational velocity
            {
                vecTailAirSpeed.z = GetBaseModelInfo()->GetBoundingBoxMin().z;
                vecTailAirSpeed = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vecTailAirSpeed)));
                vecTailAirSpeed.CrossNegate(GetAngVelocity());
            }
            vecTailAirSpeed.Add(vecAirSpeed);

            float fSideSpeed = DotProduct(vecTailAirSpeed, VEC3V_TO_VECTOR3(GetTransform().GetA()));
            float fFwdSpeed = DotProduct(vecTailAirSpeed, VEC3V_TO_VECTOR3(GetTransform().GetB()));
            float fAirSpeedSqr = vecAirSpeed.Mag2();
 
            float fSideSlipAngle = -rage::Atan2f(fSideSpeed, fFwdSpeed);
            fSideSlipAngle = rage::Clamp(fSideSlipAngle, -PLANE_VERTICAL_RUDDER_MAX_ANGLE_OF_ATTACK, PLANE_VERTICAL_RUDDER_MAX_ANGLE_OF_ATTACK);

            // control force from steering and stabilising force from rudder
            Vector3 vecRudderForce = VEC3V_TO_VECTOR3(GetTransform().GetA());

            vecRudderForce *= (pFlyingHandling->m_fYawMult*(fYawControl+fYawFromDamageEffect) + pFlyingHandling->m_fYawStabilise*fSideSlipAngle*fAirSpeedSqr)*vecAngInertia.z;
            ApplyInternalTorque(vecRudderForce*fOverallMultiplier, -VEC3V_TO_VECTOR3(GetTransform().GetB()));
        }
    }
}

void CPlane::ResetVtolRollVariables()
{
	m_bVtolRollVariablesInitialised = false;
	m_VtolRollIntegral = 0;
	m_VtolRollPreviousError = 0;
}

bool CPlane::GetDestroyedByPed( )
{
	if (m_aircraftDamage.GetDestroyedByPed())
		return true;

	static const u32 LAST_DAMAGE_TRESHOLD = 3*1000;
	const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - GetWeaponDamagedTime();
	CEntity* lastdamager = GetWeaponDamageEntity();
	if (lastdamager && timeSinceLastDamage <= LAST_DAMAGE_TRESHOLD)
	{
		if (lastdamager->GetIsTypePed())
			return true;

		if (lastdamager->GetIsTypeVehicle() && static_cast<CVehicle*>(lastdamager)->GetDriver())
			return true;
	}

	return false;
}

void CPlane::ApplyDeformationToBones(const void* basePtr)
{
	CAutomobile::ApplyDeformationToBones(basePtr);

	// Apply deformation to after burner
	CVehicleModelInfo* pModelInfo = GetVehicleModelInfo();
	for (int i = 0; i < PLANE_NUM_AFTERBURNERS; ++i)
	{
		s32 afterburnerBoneId = pModelInfo->GetBoneIndex(PLANE_AFTERBURNER + i);
		if (afterburnerBoneId != -1)
		{
			Matrix34 &mtxLocal = GetLocalMtxNonConst(afterburnerBoneId);

			const Vec3V basePos = GetSkeletonData().GetBoneData(afterburnerBoneId)->GetDefaultTranslation();
			const Vec3V damage = GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, basePos);

			mtxLocal.d = VEC3V_TO_VECTOR3(basePos + damage);
		}
	}

	for(int i =0; i< m_iNumPropellers; i++)
	{
		m_propellers[i].ApplyDeformation(this,basePtr);
	}

	m_landingGear.ApplyDeformation(this, basePtr);
}

void CPlane::Fix(bool resetFrag, bool allowNetwork)
{		
	CAutomobile::Fix(resetFrag, allowNetwork);
	m_landingGear.Fix(this);

	m_aircraftDamage.Fix();
	m_landingGearDamage.Fix();
	
	for(int i =0; i< m_iNumPropellers; i++)
	{
		m_propellers[i].Fix();
        m_fPropellerHealth[i] = VEH_DAMAGE_HEALTH_STD; 
	}
	
	if(GetVerticalFlightModeAvaliable())
	{
		SetDesiredVerticalFlightModeRatio( 1.0f );
	}

	ResetEngineTurnedOffByPlayer();
}

bool CPlane::GetWheelDeformation(const CWheel *pWheel, Vector3 &vDeformation) const
{
	return m_landingGear.GetWheelDeformation(this, pWheel, vDeformation);
}

bool CPlane::IsEntryPointUseable(s32 iEntryPointIndex) const
{
	if (!IsEntryIndexValid(iEntryPointIndex))
	{
		return false;
	}

	const CVehicleEntryPointAnimInfo* pAnimInfo = GetEntryAnimInfo(iEntryPointIndex);
	const CModelSeatInfo* pModelSeatinfo = GetVehicleModelInfo()->GetModelSeatInfo();	
	if (pModelSeatinfo && pAnimInfo && pAnimInfo->GetUseNewPlaneSystem())
	{
		const CAircraftDamage& aircraftDamage = GetAircraftDamage();
		const CVehicleEntryPointInfo* pEntryPointInfo = pModelSeatinfo->GetLayoutInfo()->GetEntryPointInfo(iEntryPointIndex);
		taskFatalAssertf(pEntryPointInfo, "CPlane::IsEntryPointUseable() : Plane doesn't have valid entry point info");
		if (pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
		{
			if (aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_L))
			{
				return false;
			}
		}
		else if (pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_RIGHT)
		{
			if (aircraftDamage.HasSectionBrokenOff(this, CAircraftDamage::WING_R))
			{
				return false;
			}
		}
	}
	return true;
}

bool CPlane::IsPlayerDrivingOnGround() const
{
	// Check for local player driver
	if( GetDriver() && GetDriver()->IsLocalPlayer() )
	{
		// Check for valid driving on the ground status
		const float fNegligibleVelSquared = 0.005f;
		if(	IsEngineOn() && !GetIsAnyFixedFlagSet() && !GetIsInWater() && HasContactWheels() && GetVelocity().Mag2() > fNegligibleVelSquared )
		{
			return true;
		}
	}
	return false;
}


bool CPlane::GetIsLandingGearintact() const
{
	for(int i = 0; i < NUM_LANDING_GEARS; i++)
	{
		if(GetLandingGearDamage().HasSectionBrokenOff(this, i))
		{
			return false;
		}
	}
	return true;
}


bool CPlane::AreControlPanelsIntact(bool bCheckForZeroHealth) const
{
	const CAircraftDamage& aircraftDamage = GetAircraftDamage();
	for(int iSection = CAircraftDamage::ENGINE_L; iSection < CAircraftDamage::NUM_DAMAGE_SECTIONS; iSection++)
	{
		if(aircraftDamage.HasSectionBrokenOff(this, iSection) || ( bCheckForZeroHealth && aircraftDamage.GetSectionHealth(iSection) <= 0.0f))
		{
			return false;
		}
	}

	return true;
}

dev_float sfDamageStrongMult = 0.4f;
dev_float sfCrashingMult = 10.0f;
dev_float sfDamageNoDriverMult = 3.0f;

dev_float sfRotorDamagePerSec = 100.0f; // rotor breaks out in 10s
dev_float sfRotorDamageFromImpactMag = 15.0f;
dev_float sfRotorEngineDamageMult = 0.1f;

dev_float sfUpsideDownRotorDamageMult = 5.0f;

// Use collision information to damage rotors
void CPlane::ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const * hitInst, const Vector3& vMyHitPos, const Vector3& vOtherHitPos,
							  float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
							  phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact)
{
    fragInstGta* pFragInst = GetVehicleFragInst();
    vehicleAssert(pFragInst);

    float fDamageMult = 1.0f;
    if(m_nPhysicalFlags.bNotDamagedByCollisions)
    {
        fDamageMult = 0.0f;
    }
    else if(m_nVehicleFlags.bTakeLessDamage)
    {
        fDamageMult = sfDamageStrongMult;
    }
    else if(!GetDriver())
    {
        fDamageMult = sfDamageNoDriverMult;
    }

    fDamageMult *= pHandling->m_fEngineDamageMult;
    float armourMultiplier = GetVariationInstance().GetArmourDamageMultiplier();
	fDamageMult *= armourMultiplier;

    // Increase the damage done to the engine when upside down
    if(IsUpsideDown())
    {
        fDamageMult *= sfUpsideDownRotorDamageMult;
    }

    bool bDamageDone = false;
    bool bNetworkClone = IsNetworkClone();
	bool bToggleFlightModeOnContact = GetVerticalFlightModeAvaliable() && !IsInVerticalFlightMode() && HasContactWheels();

    for(int i = 0; i < m_iNumPropellers; i++)
    {
        // check for main rotor impacts
        int nRotorChild = m_propellerCollision[i].GetFragChild();
        if(GetIsVisible() && iMyComponent==nRotorChild && !pFragInst->GetChildBroken(nRotorChild))
        {
			if( bToggleFlightModeOnContact && 
				( !pHitEnt ||
				  pHitEnt->GetIsTypeBuilding() ) )
			{
				SetDesiredVerticalFlightModeRatio( 1.0f );
				bToggleFlightModeOnContact = false;
			}

			if(m_bBreakOffPropellerOnContact && pHitEnt && pHitEnt->GetIsTypeBuilding())
			{
				BreakOffRotor(i);
			}

			if(m_propellers[i].GetSpeed() > 0.0f)
			{
				if (pHitEnt)// && CPhysics::GetIsLastTimeSlice(CPhysics::GetCurrentTimeSlice()))// && (fwTimer::GetSystemFrameCount()%4)==0)
				{
					// do collision effects
					Vector3 collPos = vMyHitPos;
					Vector3 collNormal = vMyNormal;
					//Vector3 collNormalNeg = -vMyNormal;
					Vector3 collDir(0.0f, 0.0f, 0.0f);

					Vector3 collPtToVehCentre = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) - collPos;
					collPtToVehCentre.Normalize();
					Vector3 collVel = CrossProduct(collPtToVehCentre, VEC3V_TO_VECTOR3(GetTransform().GetC()));
					collVel.Normalize();

					float scrapeMag = 100000.0f;
					float accumImpulse = 100000.0f;

					g_vfxMaterial.DoMtlScrapeFx(this, 0, pHitEnt, 0, RCC_VEC3V(collPos), RCC_VEC3V(collNormal), RCC_VEC3V(collVel), PGTAMATERIALMGR->g_idCarMetal, iOtherMaterial, RCC_VEC3V(collDir), scrapeMag, accumImpulse, VFXMATERIAL_LOD_RANGE_SCALE_HELI, 1.0f, 1.0);
					g_vfxMaterial.DoMtlScrapeFx(pHitEnt, 0, this, 0, RCC_VEC3V(collPos), RCC_VEC3V(collNormal), RCC_VEC3V(collVel), iOtherMaterial, PGTAMATERIALMGR->g_idCarMetal, RCC_VEC3V(collDir), scrapeMag, accumImpulse, VFXMATERIAL_LOD_RANGE_SCALE_HELI, 1.0f, 1.0);

					if (pHitEnt->GetIsTypePed())
					{
						g_vfxBlood.UpdatePtFxBloodMist(static_cast<CPed*>(pHitEnt));
					}

					pHitEnt->ProcessFxEntityCollision(collPos, collNormal, 0, accumImpulse);
				}
				// rotor take damage
				if(m_nVehicleFlags.bCanBeVisiblyDamaged && !bNetworkClone)
				{
					float fDamage = sfRotorDamagePerSec * fwTimer::GetTimeStep();
					fDamage += fImpulseMag * GetInvMass() * sfRotorDamageFromImpactMag;
					fDamage *= fDamageMult;

					// !!! GTA V HACK !!!
					if(GetModelId() == MI_PLANE_VELUM || GetModelId() == MI_PLANE_VELUM2)
					{
						fDamage *= 0.01f;
					}

					m_fPropellerHealth[i] -= fDamage;
					bDamageDone = true;

					if(m_fPropellerHealth[i] <= 0.0f)
					{
						BreakOffRotor(i);
					}

					const float damage = fDamage * sfRotorEngineDamageMult;
					m_Transmission.ApplyEngineDamage(this, this, DAMAGE_TYPE_COLLISION, damage);
					m_bIsPropellerDamagedThisFrame = true;
				}
			}
        }
    }

    // make sure overall health is less than full if any plane components are damaged
    if(bDamageDone && GetHealth() >= CREATED_VEHICLE_HEALTH)
    {
        if (!IsNetworkClone())
        {
            ChangeHealth(-1.0f);
        }
    }

    return CAutomobile::ProcessCollision(myInst, pHitEnt, hitInst, vMyHitPos,vOtherHitPos,fImpulseMag,vMyNormal,iMyComponent,iOtherComponent,
		iOtherMaterial, bIsPositiveDepth, bIsNewContact);
}

	
void CPlane::BreakOffRotor(int nRotor)
{
    int iChild = m_propellerCollision[nRotor].GetFragChild();
    if (GetVehicleFragInst() && iChild > -1 )
    {
        m_propellers[nRotor].SetSpeed(0.0f);
        m_fPropellerHealth[nRotor] = 0.0f;

		if( !GetVehicleFragInst()->GetChildBroken(iChild) )
		{
			// rotor destruction effect
			g_vfxVehicle.TriggerPtFxPropellerDestroy(this, m_propellers[nRotor].GetBoneIndex());

			//GetVehicleFragInst()->BreakOffAbove(iChild);
			GetVehicleFragInst()->DeleteAbove(iChild);

			if(m_VehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
			{
				static_cast<audPlaneAudioEntity*>(m_VehicleAudioEntity)->TriggerPropBreak();
			}
		}
    }
}

const bool CPlane::IsRotorBroken(int nRotor)
{
	int iChild = m_propellerCollision[nRotor].GetFragChild();
	if (GetVehicleFragInst() && iChild > -1)
		return GetVehicleFragInst()->GetChildBroken(iChild);

	return false;
}

dev_float dfWaterImpactDamageMult = 50.0f;
dev_float dfWaterImpactDamageMultSeaPlane = 25.0f;

dev_u32 snRumbleDuration = 100;
dev_float sfRumbleIntensityScale = 0.3f;

extern float sfVehAircraftDamImpactThreshold;
dev_float sfSeaPlaneWaterDamImpactThreshold = 15.0f;
dev_float sfSeabreazeWaterDamImpactThreshold = 170.0f;

void CPlane::NotifyWaterImpact(const Vector3& vecForce, const Vector3& vecTorque, int nComponent, float fTimeStep)
{
	if (IsNetworkClone())
		return;

	const Matrix34& matPlane = RCC_MATRIX34(GetMatrixRef());
	Vector3 vecForceLocal;
	matPlane.UnTransform3x3(vecForce, vecForceLocal);
	Vector3 vecTorqueLocal;
	matPlane.UnTransform3x3(vecTorque, vecTorqueLocal);

	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(GetVehicleFragInst()->GetArchetype()->GetBound());
	Vector3 vComponentCenterLocal = VEC3V_TO_VECTOR3(pBoundComp->GetCurrentMatrix(nComponent).GetCol3());
	Vector3 vComponentCenterGlobal;
	matPlane.Transform(vComponentCenterLocal, vComponentCenterGlobal);

	Vector3 vForceLocalFromTorque;
	vForceLocalFromTorque.Cross(vecTorqueLocal, vComponentCenterLocal);
	float fWaterImpactDamageMult = pHandling->GetSeaPlaneHandlingData() ? dfWaterImpactDamageMultSeaPlane : dfWaterImpactDamageMult;
	float fDamage = (vecForceLocal.Mag() + vForceLocalFromTorque.Mag()) * fTimeStep * GetInvMass() * fWaterImpactDamageMult;
	float fWaterImpactDamageThreshold = pHandling->GetSeaPlaneHandlingData() ? sfSeaPlaneWaterDamImpactThreshold : sfVehAircraftDamImpactThreshold;

	if( GetModelIndex() == MI_PLANE_SEABREEZE )
	{
		fWaterImpactDamageThreshold = sfSeabreazeWaterDamImpactThreshold;
	}

	if(fDamage >= fWaterImpactDamageThreshold)
	{
		// the water impact type is equivalent to water cannon impact
		GetAircraftDamage().ApplyDamageToPlane(this, NULL, DAMAGE_TYPE_WATER_CANNON, 0, fDamage, vComponentCenterGlobal, VEC3_ZERO, VEC3_ZERO, nComponent);
		GetLandingGearDamage().ApplyDamageToPlane(this, NULL, DAMAGE_TYPE_WATER_CANNON, 0, fDamage, vComponentCenterGlobal, VEC3_ZERO, VEC3_ZERO, nComponent);
	}

	// If there is a player in this plane, give their control pad a shake to let them know they've hit water.
	if(ContainsLocalPlayer() && !GetWasInWater())
	{
		CControlMgr::StartPlayerPadShakeByIntensity(snRumbleDuration, fDamage*sfRumbleIntensityScale);
	}
}

void CPlane::SwitchEngineOn(bool bNoDelay )
{
    if(bNoDelay)
    {
        m_fEngineSpeed = ms_fStdEngineSpeed;
    }

    CVehicle::SwitchEngineOn(bNoDelay);
}

void CPlane::ModifyControlsBasedOnFlyingStats( CPed* pPilot, CFlyingHandlingData* pFlyingHandling, float &fYawControl, float &fPitchControl, float &fRollControl, float& fThrottleControl, float fTimeStep )
{
	Assert( pFlyingHandling );
	Assert( pPilot );

	StatId stat = STAT_FLYING_ABILITY.GetStatId();
	float fFlyingStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(stat)) / 100.0f, 0.0f, 1.0f);
	float fMinPlaneControlAbility = CPlayerInfo::GetPlayerStatInfoForPed(*pPilot).m_MinPlaneControlAbility;
	float fMaxPlaneControlAbility = CPlayerInfo::GetPlayerStatInfoForPed(*pPilot).m_MaxPlaneControlAbility;

	float fPlaneControlAbilityMult = ((1.0f - fFlyingStatValue) * fMinPlaneControlAbility + fFlyingStatValue * fMaxPlaneControlAbility)/100.0f;
	fPlaneControlAbilityMult = Clamp(fPlaneControlAbilityMult/ms_fMaxAbilityToAdjustDifficulty, 0.0f, 1.0f);

	const float fPlaneDifficulty = pFlyingHandling->m_fInputSensitivityForDifficulty * fTimeStep;

	if( fPlaneControlAbilityMult < 1.0f )
	{
		CPlayerInfo* pPlayerInfo = pPilot->GetPlayerInfo();
		physicsFatalAssertf( pPlayerInfo, "Expected a player info!" ); // Assumed to be a player

		float fYawControlModified = fYawControl;
		float fPitchControlModified = fPitchControl;
		float fRollControlModified = fRollControl;
		float fThrottleControlModified = fThrottleControl;

		// Limited controls for inexperienced pilots
		float fAbilityMult = Clamp(ms_fPlaneControlAbilityControlDampMin + ((ms_fPlaneControlAbilityControlDampMax - ms_fPlaneControlAbilityControlDampMin) * fPlaneControlAbilityMult), 0.0f, 1.0f);
		fYawControlModified *= fAbilityMult;
		fPitchControlModified *= fAbilityMult;
		fRollControlModified *= fAbilityMult;

		// Input lagging for inexperienced pilots
		float fControlLaggingBlendingRate = (1.0f - fPlaneControlAbilityMult) * ms_fPlaneControlLaggingMinBlendingRate + fPlaneControlAbilityMult * ms_fPlaneControlLaggingMaxBlendingRate;
		const float fLaggedYawControl = pPlayerInfo->GetLaggedYawControl();
		const float fLaggedPitchControl = pPlayerInfo->GetLaggedPitchControl();
		const float fLaggedRollControl = pPlayerInfo->GetLaggedRollControl();
		
		fYawControlModified = Clamp(fYawControlModified, fLaggedYawControl - fControlLaggingBlendingRate * fTimeStep, fLaggedYawControl + fControlLaggingBlendingRate * fTimeStep);
		fPitchControlModified = Clamp(fPitchControlModified, fLaggedPitchControl - fControlLaggingBlendingRate * fTimeStep, fLaggedPitchControl + fControlLaggingBlendingRate * fTimeStep);
		fRollControlModified = Clamp(fRollControlModified, fLaggedRollControl - fControlLaggingBlendingRate * fTimeStep, fLaggedRollControl + fControlLaggingBlendingRate * fTimeStep);

		pPlayerInfo->SetLaggedYawControl(fYawControlModified);
		pPlayerInfo->SetLaggedPitchControl(fPitchControlModified);
		pPlayerInfo->SetLaggedRollControl(fRollControlModified);

		// Random control inputs for inexperienced pilots
		float fTimeBetweenRandomControlInputs = pPlayerInfo->GetTimeBetweenRandomControlInputs();
		float fRandomControlYaw = pPlayerInfo->GetRandomControlYaw();
		float fRandomControlPitch = pPlayerInfo->GetRandomControlPitch();
		float fRandomControlRoll = pPlayerInfo->GetRandomControlRoll();
		float fRandomControlThrottle = pPlayerInfo->GetRandomControlThrottle();
		
		fTimeBetweenRandomControlInputs -= fTimeStep;
		// Select a new set of randomised controls
		if( fTimeBetweenRandomControlInputs <= 0.0f )
		{
			float fAbilityRandomMult = Clamp(ms_fPlaneControlAbilityControlRandomMin + ((ms_fPlaneControlAbilityControlRandomMax - ms_fPlaneControlAbilityControlRandomMin) * (1.0f - fPlaneControlAbilityMult)), 0.0f, 1.0f);
			fRandomControlYaw = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult*ms_fPlaneControlAbilityControlRandomYawMult, fAbilityRandomMult*ms_fPlaneControlAbilityControlRandomYawMult);
			fRandomControlPitch = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult*ms_fPlaneControlAbilityControlRandomPitchMult, fAbilityRandomMult*ms_fPlaneControlAbilityControlRandomPitchMult);
			fRandomControlRoll = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult*ms_fPlaneControlAbilityControlRandomRollMult, fAbilityRandomMult*ms_fPlaneControlAbilityControlRandomRollMult);
			fRandomControlThrottle = fwRandom::GetRandomNumberInRange(-fAbilityRandomMult*ms_fPlaneControlAbilityControlRandomThrottleMult, 0.0f);
			fTimeBetweenRandomControlInputs = Clamp(ms_fTimeBetweenRandomControlInputsMin + ((ms_fTimeBetweenRandomControlInputsMax - ms_fTimeBetweenRandomControlInputsMin) * (1.0f - fPlaneControlAbilityMult)), 0.0f, 1.0f);
		}
		// Modify the controls and update the lerp speed
		fYawControlModified += fRandomControlYaw;
		fPitchControlModified += fRandomControlPitch;
		fRollControlModified += fRandomControlRoll;
		fThrottleControlModified += fRandomControlThrottle;

		fYawControl += (fYawControlModified - fYawControl) * m_fScriptPilotSkillNoiseScalar;
		fPitchControl += (fPitchControlModified - fPitchControl) * m_fScriptPilotSkillNoiseScalar;
		fRollControl += (fRollControlModified - fRollControl) * m_fScriptPilotSkillNoiseScalar;
		fThrottleControl += (fThrottleControlModified - fThrottleControl) * m_fScriptPilotSkillNoiseScalar;

		fRandomControlYaw = Lerp(fPlaneDifficulty, fRandomControlYaw, 0.0f);
		fRandomControlPitch = Lerp(fPlaneDifficulty, fRandomControlPitch, 0.0f);
		fRandomControlRoll = Lerp(fPlaneDifficulty, fRandomControlRoll, 0.0f);
		fRandomControlThrottle = Lerp(fPlaneDifficulty, fRandomControlThrottle, 0.0f);

		// Store the values back on the player info
		pPlayerInfo->SetTimeBetweenRandomControlInputs(fTimeBetweenRandomControlInputs);
		pPlayerInfo->SetRandomControlYaw(fRandomControlYaw);
		pPlayerInfo->SetRandomControlPitch(fRandomControlPitch);
		pPlayerInfo->SetRandomControlRoll(fRandomControlRoll);
		pPlayerInfo->SetRandomControlThrottle(fRandomControlThrottle);
	}
}

void CPlane::UpdateMeredithVent()
{
	fragInst* pFragInst = GetVehicleFragInst();

	fragHierarchyInst* pHierInst = pFragInst->GetCacheEntry()->GetHierInst();
	phArticulatedCollider* pArticulatedCollider = pHierInst->articulatedCollider;

	if(pArticulatedCollider == NULL)
	{
		return;
	}

	int iBoneIndex = GetBoneIndex( PLANE_EXHAUST_VENT );

	if( iBoneIndex == -1 )
	{
		return;
	}

	//pFragInst->GetCacheEntry()->LazyArticulatedInit();

	int fragChild = GetVehicleFragInst()->GetComponentFromBoneIndex( iBoneIndex );
	int linkIndex = pArticulatedCollider->GetLinkFromComponent( fragChild );

	if( linkIndex > 0 )
	{
		phJoint1Dof* p1DofJoint = NULL;
		phJoint* pJoint = &pHierInst->body->GetJoint(linkIndex - 1);
		if (pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF)
		{
			p1DofJoint = static_cast<phJoint1Dof*>(pJoint);
		}

		if(p1DofJoint)
		{
			float speedPercentage = Clamp( GetVelocity().Dot( VEC3V_TO_VECTOR3( GetTransform().GetForward() ) ) / pHandling->m_fEstimatedMaxFlatVel, 0.0f, 0.7f );

			// First figure out what our closed angle is
			// Assume open angle is 0
			float fMin, fMax;
			p1DofJoint->GetAngleLimits( fMin,fMax );

			float fClosedAngle = 0.0f;
			float fOpenAngle = 0.0f;
			if( -fMin > fMax )
			{
				fClosedAngle = fMin;
				fOpenAngle = fMax;
			}
			else
			{
				fClosedAngle = fMax;
				fOpenAngle = fMin;
			}

			TUNE_GROUP_BOOL( STUNT_PLANE, OVERRIDE_MEREDITH_VENT, false );
			TUNE_GROUP_FLOAT( STUNT_PLANE, OVERRIDE_MEREDITH_VENT_SPEED, 0.0f, 0.0f, 0.7f, 0.1f );

			if( OVERRIDE_MEREDITH_VENT )
			{
				speedPercentage = OVERRIDE_MEREDITH_VENT_SPEED;
			}

			float fTargetAngle = fClosedAngle + ( ( speedPercentage * 1.42857f ) * ( fOpenAngle - fClosedAngle ) );
			//float fTargetSpeed = 1.0f;

			p1DofJoint->SetMuscleTargetAngle( fTargetAngle );

			// Set the strength and stiffness
			// Done here for tweaking
			// Todo: move to Init()

			// Need to scale some things by inertia
			const float fInvAngInertia = p1DofJoint->ResponseToUnitTorque( pHierInst->body );
			float fMinMaxTorque = 100.0f / fInvAngInertia;
			// is it possible (or better) to get the inertia from the FragType?

			p1DofJoint->SetStiffness( 0.75f );		// If i set this to >= 1.0f the game crashes with a NAN root link position
			p1DofJoint->SetMinAndMaxMuscleTorque( fMinMaxTorque );
			p1DofJoint->SetMuscleAngleStrength( 1000.0f );
			p1DofJoint->SetMuscleSpeedStrength( 1500.0f );
			p1DofJoint->SetDriveState( phJoint::DRIVE_STATE_ANGLE );

			//float fCurrentAngle = p1DofJoint->GetAngle( pHierInst->body );

			//// Smooth the desired speed out close to the target
			//float fAngleDiff = fTargetAngle - fCurrentAngle;
			//fAngleDiff = fwAngle::LimitRadianAngle(fAngleDiff);
			//float fJointSpeedTarget = 0.0f;				
			//fJointSpeedTarget = rage::Clamp(fTargetSpeed*fAngleDiff/sfSpeedStrengthFadeOutAngle,-fTargetSpeed,fTargetSpeed);

			//// If this is set to 0 then we get damping
			//// this gives us damping
			//p1DofJoint->SetMuscleTargetSpeed(fJointSpeedTarget);
			//p1DofJoint->SetDriveState(phJoint::DRIVE_STATE_ANGLE_AND_SPEED);

			//if(fabs(fCurrentAngle) < fabs(fOpenAngle))// if the joint is past its lower limit clamp to 0 so we get a slightly more accurate ratio.
			//{
			//	fCurrentAngle = 0.0f;
			//}
		}
	}
}

void CPlane::UpdatePropellerCollision()
{
	u32 nRotorIncludeFlags = ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES;
	// don't let rotors hit ped bounds, only ragdoll bounds, since it's a non-physical collision anyway and we don't want peds to stand on rotors
	// Remove this to fix the ped not hit by rotors. The issue that ped able to stand on rotors has been fixed in the other place.
	//nRotorIncludeFlags &= ~ArchetypeFlags::GTA_PED_TYPE;
	nRotorIncludeFlags &= ~(ArchetypeFlags::GTA_AI_TEST | ArchetypeFlags::GTA_SCRIPT_TEST | ArchetypeFlags::GTA_WHEEL_TEST);
	u32 nBladeIncludeFlags = ArchetypeFlags::GTA_WEAPON_AND_PROJECTILE_INCLUDE_TYPES | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE;


	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>( GetVehicleFragInst()->GetArchetype()->GetBound() );	

	for( int i = 0; i < m_iNumPropellers; i++ )
	{
		if( m_propellerCollision[ i ].GetFragGroup() > -1 )
		{
			int includeFlags = nBladeIncludeFlags;

			if( GetModelIndex() == MI_PLANE_MICROLIGHT &&
				( !IsIndividualPropellerOn( i ) ||
				  ( !sbEnablePropellerMods && i > 0 ) ) )
			{
				includeFlags = 0;
				//m_propellers[ i ].SetPropellerVisible( false );
			}
			//else
			//{
			//	m_propellers[ i ].SetPropellerVisible( true );
			//}

			fragTypeGroup* pGroup = GetVehicleFragInst()->GetTypePhysics()->GetAllGroups()[ m_propellerCollision[ i ].GetFragGroup() ];
			if( pGroup->GetNumChildren() > 1 )
			{
				for( int iChild = 0; iChild < pGroup->GetNumChildren(); iChild++ )
				{
					pBoundComp->SetIncludeFlags( m_propellerCollision[ i ].GetFragChild() + iChild, includeFlags );
				}
			}
		}

		if( m_propellerCollision[i].GetFragDisc() > 0 )	// don't set GTA_ALL_SHAPETEST_TYPES so probes won't hit rotors
		{
			int includeFlags = nRotorIncludeFlags;

			if( GetModelIndex() == MI_PLANE_MICROLIGHT &&
				( !IsIndividualPropellerOn( i ) ||
				( !sbEnablePropellerMods && i > 0 ) ) )
			{
				includeFlags = 0;
			}

			pBoundComp->SetIncludeFlags( m_propellerCollision[i].GetFragDisc(), includeFlags );
		}
	}
}

#if __WIN32PC
void CPlane::ProcessForceFeedback()
{
	// Used to tune the plane force to range more appropriate for the device input.
	TUNE_FLOAT(PLANE_FORCE_FEEDBACK_SCALER, 0.0000625f, 0.0f, 1.0f, 0.0000001f);

	// Used to tune how quick the force feedback response is.
	TUNE_FLOAT(PLANE_FORCE_FEEDBACK_RESPONSE, 1.0f, 0.01f, 5.0f, 0.01f);

	// Used to tune the velocity effect on the force feedback.
	TUNE_FLOAT(PLANE_VELOCITY_FORCE_FEEDBACK_SCALER, 0.0067f, 0.0000f, 1.0f, 0.0001f);

	// Used to tune the resistance in the device's resistance force.
	TUNE_FLOAT(PLANE_VELOCITY_FORCE_FEEDBACK_RESISTANCE_SCALER, 0.06f, 0.01f, 1.0f, 0.01f);

	const ioValue& leftRightValue = CControlMgr::GetMainCameraControl(false).GetVehicleFlyRollLeftRight();
	const ioValue& upDownValue    = CControlMgr::GetMainCameraControl(false).GetVehicleFlyPitchUpDown();


	float force            = m_fWingForce * PLANE_FORCE_FEEDBACK_SCALER;

	// Whilst we are only applying force feedback to the y axis of the device, we want to add resistance to the x axis of the device.
	Vector2 inputVelocity  = Vector2( leftRightValue.GetNorm(ioValue::NO_DEAD_ZONE) - leftRightValue.GetLastNorm(ioValue::NO_DEAD_ZONE),
		                              upDownValue.GetNorm(ioValue::NO_DEAD_ZONE)    - upDownValue.GetLastNorm(ioValue::NO_DEAD_ZONE) );

	inputVelocity         /= fwTimer::GetTimeStep();
	inputVelocity         *= PLANE_VELOCITY_FORCE_FEEDBACK_RESISTANCE_SCALER;

	force *= Clamp(GetVelocity().Mag() / PLANE_VELOCITY_FORCE_FEEDBACK_SCALER, 0.0f, 1.0f);

	float xResistance      = (1.0f/(1.0f + exp(-inputVelocity.x/PLANE_FORCE_FEEDBACK_RESPONSE))-0.5f) * 2.0f;
	float forceCompensated = (1.0f/(1.0f + exp((-force - inputVelocity.y)/PLANE_FORCE_FEEDBACK_RESPONSE))-0.5f) * 2.0f;

	CControlMgr::ApplyDirectionalForce(xResistance, forceCompensated, 100);
}
#endif // __WIN32PC
