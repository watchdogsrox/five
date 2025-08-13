#include "TaskVehicleFlying.h"

// Game headers
#include "scene/world/GameWorldHeightMap.h"
#include "TaskVehiclePlayer.h"
#include "TaskVehicleGoToPlane.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/Planes.h"
#include "Vehicles/heli.h"

// Framework headers:
#include "fwscene/stores/staticboundsstore.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

PARAM(logVehicleCrashTasks, "Log when vehicle crash and destroy tasks are created");

CTaskVehicleCrash::Tunables CTaskVehicleCrash::sm_Tunables;

IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleCrash, 0x1f62ccb1);

////////////////////////////////////////////////////////////////////////////

CTaskVehicleCrash::CTaskVehicleCrash( CEntity *pCulprit, s32 crashFlags, u32 weaponHash)	:
m_culprit(pCulprit),
m_iWeaponHash(weaponHash),
m_iCrashFlags(crashFlags),
m_fOutOfControlRoll(0.0f),
m_fOutOfControlYaw(0.0f),
m_fOutOfControlPitch(0.0f),
m_fOutOfControlThrottle(0.0f),
m_uOutOfControlStart(0),
m_uOutOfControlRecoverStart(0),
m_iOutOfControlRecoverPeriods(0),
m_bOutOfControlRecovering(false)
{
	if (PARAM_logVehicleCrashTasks.Get())
	{
		aiDebugf1("CTaskVehicleCrash - constructed");
		sysStack::PrintStackTrace();
	}

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_CRASH);
}


////////////////////////////////////////////////////////////////////////////

CTaskVehicleCrash::~CTaskVehicleCrash()
{

}

////////////////////////////////////////////////////////////////////////////

// Helper class which gets done here since this might get called from pre-compute impacts to avoid a frame delay in
// starting the explosion.
void CTaskVehicleCrash::DoBlowUpVehicle(CVehicle* pVehicle)
{
	// FinishBlowingUpVehicle will be called on clone by syncing code....
	if(!pVehicle->IsNetworkClone() && !IsCrashFlagSet(CF_ExplosionAdded))
	{
		switch(pVehicle->GetVehicleType())
		{
		case VEHICLE_TYPE_PLANE:
			static_cast<CPlane*>(pVehicle)->FinishBlowingUpVehicle(m_culprit.GetEntity(), IsCrashFlagSet(CF_InACutscene), IsCrashFlagSet(CF_AddExplosion), false, m_iWeaponHash);
			break;
		case VEHICLE_TYPE_HELI:
			static_cast<CHeli*>(pVehicle)->FinishBlowingUpVehicle(m_culprit.GetEntity(), IsCrashFlagSet(CF_InACutscene), IsCrashFlagSet(CF_AddExplosion), false, m_iWeaponHash);
			break;
		case VEHICLE_TYPE_AUTOGYRO:
			static_cast<CAutogyro*>(pVehicle)->FinishBlowingUpVehicle(m_culprit.GetEntity(), IsCrashFlagSet(CF_InACutscene), IsCrashFlagSet(CF_AddExplosion), false, m_iWeaponHash);
			break;
		case VEHICLE_TYPE_BLIMP:
			static_cast<CBlimp*>(pVehicle)->FinishBlowingUpVehicle(m_culprit.GetEntity(), IsCrashFlagSet(CF_InACutscene), IsCrashFlagSet(CF_AddExplosion), false, m_iWeaponHash);
			break;
		default:
			Assertf(0, "Non PLANE, HELI or AUTOGYRO has been told to crash");
			break;
		}

		SetCrashFlag(CF_ExplosionAdded, true);
	}
}

////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleCrash::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Crash)
			FSM_OnEnter
				Crash_OnEnter(pVehicle);
			FSM_OnUpdate
				return Crash_OnUpdate(pVehicle);
		// Wrecked state
		FSM_State(State_Wrecked)
			FSM_OnEnter
				Wrecked_OnEnter(pVehicle);
			FSM_OnUpdate
				return Wrecked_OnUpdate(pVehicle);
			FSM_OnExit
				Wrecked_OnExit(pVehicle);
		// exit state
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//Out of control tuning variables, could do with tidying up.

dev_float sfOutOfControlInitialMaxThrottle = 1.0f;
dev_float sfOutOfControlInitialMaxYaw = 0.15f;
dev_float sfOutOfControlInitialMaxRoll = 1.2f;
dev_float sfOutOfControlInitialMaxPitch = -0.4f;

dev_float sfOutOfControlThrottleDropAfterRecoveryMin = 0.0f;
dev_float sfOutOfControlThrottleDropAfterRecoveryMax = 1.4f;

dev_float sfOutOfControlAfterRecoveryMult = 1.8f;

dev_float sfOutOfControlThrottleControlAdjustmentNeg = -0.8f;
dev_float sfOutOfControlThrottleControlAdjustmentPos = 0.3f;

int siOutOfControlRecoveryPeriodsMax = 3;

int siOutOfControlInitialFailPeriodsMin = 100;
int siOutOfControlInitialFailPeriodsMax = 400;

int siOutOfControlFailPeriodsMin = 1200;
int siOutOfControlFailPeriodsMax = 3500;

int siOutOfControlRecoverPeriodsMin = 600;
int siOutOfControlRecoverPeriodsMax = 1200;
////////////////////////////////////////////////////////////////////////////

void	CTaskVehicleCrash::Crash_OnEnter				(CVehicle* pVehicle)
{
	m_uOutOfControlStart = fwTimer::GetTimeInMilliseconds();
	switch(pVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_PLANE:
		{
			bool bPlaneDippingDown = pVehicle->InheritsFromPlane() && static_cast<CPlane*>(pVehicle)->GetDipStraightDownWhenCrashing();
			if (bPlaneDippingDown)
			{
				m_fOutOfControlPitch = sfOutOfControlInitialMaxPitch;
			}
			else
			{
				m_fOutOfControlYaw = fwRandom::GetRandomNumberInRange(-sfOutOfControlInitialMaxYaw, sfOutOfControlInitialMaxYaw);

				if( !pVehicle->InheritsFromPlane() ||
					static_cast< CPlane* >( pVehicle )->GetAllowRollAndYawWhenCrashing() )
				{
					m_fOutOfControlRoll = fwRandom::GetRandomNumberInRange(sfOutOfControlInitialMaxRoll, sfOutOfControlInitialMaxRoll);
				}
				else
				{
					m_fOutOfControlRoll = 0.0f;
				}

				m_fOutOfControlPitch = fwRandom::GetRandomNumberInRange(sfOutOfControlInitialMaxPitch, 0.0f);

				m_iOutOfControlRecoverPeriods = siOutOfControlRecoveryPeriodsMax;
				m_uOutOfControlRecoverStart = m_uOutOfControlStart + fwRandom::GetRandomNumberInRange(siOutOfControlInitialFailPeriodsMin, siOutOfControlInitialFailPeriodsMax);
			}

			static dev_float sfRandomMinThrottleWeighting = 0.2f;
			m_fOutOfControlThrottle = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sfRandomMinThrottleWeighting ? 0.0f : sfOutOfControlInitialMaxThrottle;
			m_bOutOfControlRecovering = false;

			pVehicle->SetLodMultiplier(3.0f);
			break;
		}
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		{
			break;
		}
	default:
		break;
	}
}

////////////////////////////////////////////////////////////////////////////
int iConsecutiveExplosionDelay = 200;
aiTask::FSM_Return	CTaskVehicleCrash::Crash_OnUpdate				(CVehicle* pVehicle)
{
	// the vehicle may already be wrecked in MP, if the task has started up again after the vehicle migrates 
	if (NetworkInterface::IsGameInProgress() && pVehicle->GetStatus() == STATUS_WRECKED)
	{
		SetState(State_Exit);
		return FSM_Continue;
	}

	// reset consecutive explosion flag from very short time, in case the flag being turned on by the original explosion
	if(fwTimer::GetTimeInMilliseconds() < (m_uOutOfControlStart+iConsecutiveExplosionDelay))
	{
		SetCrashFlag(CF_HitByConsecutiveExplosion, false);
	}

	if((m_iCrashFlags&CF_BlowUpInstantly) || (m_iCrashFlags&CF_HitByConsecutiveExplosion))
	{
		SetState(State_Wrecked);
	}
	else
	{
		// Since a plane or heli could get far enough away from the player that collision isn't streamed in, we
		// need to check the height map to decide when to make the vehicle explode in that case.
		if(CheckForCollisionWithGroundWhenFarFromPlayer(pVehicle) && (pVehicle->GetIsHeli() || pVehicle->GetIsAircraft()))
		{
			pVehicle->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, false, true);
			SetState(State_Wrecked);
			return FSM_Continue;
		}

		switch(pVehicle->GetVehicleType())
		{
		case VEHICLE_TYPE_PLANE:
			if(pVehicle->pHandling->GetSeaPlaneHandlingData() && pVehicle->GetIsInWater())
			{
				return FSM_Quit;
			}
			else
			{
				return Plane_SteerToCrashAndBurn(static_cast<CPlane*>(pVehicle));
			}
		case VEHICLE_TYPE_HELI:
		case VEHICLE_TYPE_BLIMP:
			if( pVehicle->pHandling->GetSeaPlaneHandlingData() && pVehicle->m_nFlags.bPossiblyTouchesWater )
			{
				return FSM_Quit;
			}
			else
			{
				return Helicopter_SteerToCrashAndBurn( static_cast<CHeli*>( pVehicle ) );
			}
		default:
			SetState(State_Wrecked);
			break;
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////

void CTaskVehicleCrash::Wrecked_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleNoDriver(CTaskVehicleNoDriver::NO_DRIVER_TYPE_WRECKED));
}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCrash::Wrecked_OnUpdate(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_NO_DRIVER))
	{
		//Apply some extra deformation to main body
		if (CVehicleDeformation::ms_iExtraExplosionDeformations > 0)
		{
			CVehicleDamage::AddVehicleExplosionDeformations(pVehicle, NULL, DAMAGE_TYPE_EXPLOSIVE, CVehicleDeformation::ms_iExtraExplosionDeformations, CVehicleDeformation::ms_fExtraExplosionsDamage);
		}
		DoBlowUpVehicle(pVehicle);
		SetState(State_Exit);
	}

	return FSM_Continue;
}

dev_float dfWingDeformationMulti = 1.0f;

void CTaskVehicleCrash::Wrecked_OnExit(CVehicle* pVehicle)
{

	float fDeformationMag = CVehicleDeformation::ms_fExtraExplosionsDamage*(pVehicle->GetMass()*InvertSafe(pVehicle->pHandling->m_fDeformationDamageMult,0.0f));

	//Apply some deformation to wings if it's a plane
	float fWingDeformationMag = fDeformationMag * dfWingDeformationMulti;
	if(pVehicle->InheritsFromPlane())
	{
		CPlane *pPlane = (CPlane *) pVehicle;
		if(!pPlane->GetAircraftDamage().HasSectionBrokenOff(pPlane, CAircraftDamage::WING_L))
		{
			eHierarchyId nId = pPlane->GetAircraftDamage().GetHierarchyIdFromSection(CAircraftDamage::WING_L);
			if(nId > VEH_INVALID_ID)
			{
				int iBoneIndex = pPlane->GetBoneIndex(nId);
				if(iBoneIndex > -1)
				{
					Matrix34 matWing;
					pPlane->GetGlobalMtx(iBoneIndex, matWing);
					Vector3 vecOffset = matWing.d;
					Vector3 vecForceDirection = vecOffset - VEC3V_TO_VECTOR3(pPlane->GetVehiclePosition());
					vecForceDirection.NormalizeSafe(vecForceDirection);
					pVehicle->GetVehicleDamage()->GetDeformation()->ApplyCollisionImpact( vecForceDirection*fWingDeformationMag, vecOffset, NULL, true);
				}
			}
		}

		if(!pPlane->GetAircraftDamage().HasSectionBrokenOff(pPlane, CAircraftDamage::WING_R))
		{
			eHierarchyId nId = pPlane->GetAircraftDamage().GetHierarchyIdFromSection(CAircraftDamage::WING_R);
			if(nId > VEH_INVALID_ID)
			{
				int iBoneIndex = pPlane->GetBoneIndex(nId);
				if(iBoneIndex > -1)
				{
					Matrix34 matWing;
					pPlane->GetGlobalMtx(iBoneIndex, matWing);
					Vector3 vecOffset = matWing.d;
					Vector3 vecForceDirection = vecOffset - VEC3V_TO_VECTOR3(pPlane->GetVehiclePosition());
					vecForceDirection.NormalizeSafe(vecForceDirection);
					pVehicle->GetVehicleDamage()->GetDeformation()->ApplyCollisionImpact( vecForceDirection*fWingDeformationMag, vecOffset, NULL, true);
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleCrash::Plane_SteerToCrashAndBurn(CPlane *pPlane)
{
	Assert(pPlane->GetVehicleType()==VEHICLE_TYPE_PLANE);

	if(fwTimer::GetTimeInMilliseconds() > m_uOutOfControlRecoverStart)
	{
		if(m_bOutOfControlRecovering)
		{
			m_bOutOfControlRecovering = false;

			if(m_iOutOfControlRecoverPeriods < siOutOfControlRecoveryPeriodsMax-1)
			{
                if( pPlane->GetAllowRollAndYawWhenCrashing() )
                {
				    m_fOutOfControlRoll = fwRandom::GetRandomNumberInRange(-sfOutOfControlInitialMaxRoll, sfOutOfControlInitialMaxRoll) * sfOutOfControlAfterRecoveryMult;
                    m_fOutOfControlYaw = fwRandom::GetRandomNumberInRange(-sfOutOfControlInitialMaxYaw, sfOutOfControlInitialMaxYaw) * sfOutOfControlAfterRecoveryMult;
                }
				m_fOutOfControlPitch = fwRandom::GetRandomNumberInRange(-sfOutOfControlInitialMaxPitch, sfOutOfControlInitialMaxPitch) * sfOutOfControlAfterRecoveryMult;
				m_fOutOfControlThrottle = fwRandom::GetRandomNumberInRange(sfOutOfControlThrottleDropAfterRecoveryMin, sfOutOfControlThrottleDropAfterRecoveryMax);
				m_uOutOfControlRecoverStart = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(siOutOfControlFailPeriodsMin, siOutOfControlFailPeriodsMax);
			}
		}
		else
		{
			if(m_iOutOfControlRecoverPeriods > 0)
			{
				m_uOutOfControlRecoverStart = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(siOutOfControlRecoverPeriodsMin, siOutOfControlRecoverPeriodsMax);

				m_bOutOfControlRecovering = true;

				m_iOutOfControlRecoverPeriods--;
				if(m_iOutOfControlRecoverPeriods < 0)
					m_iOutOfControlRecoverPeriods = 0;
			}
		}
	}

	float fYawControl = 0.0f;
	float fPitchControl = 0.0f;
	float fRollControl = 0.0f;

	if(!m_bOutOfControlRecovering)
	{
		if(m_iOutOfControlRecoverPeriods < siOutOfControlRecoveryPeriodsMax)
		{    
			fYawControl = m_fOutOfControlYaw;
			fPitchControl = m_fOutOfControlPitch;
			fRollControl = m_fOutOfControlRoll;

			m_fOutOfControlThrottle += fwRandom::GetRandomNumberInRange(sfOutOfControlThrottleControlAdjustmentNeg, sfOutOfControlThrottleControlAdjustmentPos);// * fTimeStep
			m_fOutOfControlThrottle = Clamp(m_fOutOfControlThrottle, 0.0f, 2.0f);

			pPlane->SetThrottleControl(m_fOutOfControlThrottle);
		}
		else //When we are first crashing, lerp in the out of control inputs.
		{
			float fFractionOftimeBeforeRecovery = (float)(fwTimer::GetTimeInMilliseconds() - m_uOutOfControlStart) / (float)(m_uOutOfControlRecoverStart - m_uOutOfControlStart);

            if( pPlane->GetAllowRollAndYawWhenCrashing() )
            {
			    fYawControl = Lerp(fFractionOftimeBeforeRecovery, 0.0f, m_fOutOfControlYaw);
                fRollControl = Lerp(fFractionOftimeBeforeRecovery, 0.0f, m_fOutOfControlRoll);
            }
			fPitchControl = Lerp(fFractionOftimeBeforeRecovery, 0.0f, m_fOutOfControlPitch);
			
		}
	}


	pPlane->SetYawControl( Clamp(fYawControl, -1.0f, 1.0f) );
	pPlane->SetPitchControl( Clamp(fPitchControl, -1.0f, 1.0f) );
	pPlane->SetRollControl( Clamp(fRollControl, -1.0f, 1.0f) );


	// If we've hit a building we blow up
	if (!pPlane->m_nPhysicalFlags.bRenderScorched && pPlane->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypes(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING ) )
	{
		SetState(State_Wrecked);
	}

	if(pPlane->HasContactWheels())//if we've touched the ground then blow up as well
	{
		SetState(State_Wrecked);
	}

	return FSM_Continue;
}	


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SteerToCrashAndBurn
// PURPOSE :  Make the heli sterr to the ground and crash.
/////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return CTaskVehicleCrash::Helicopter_SteerToCrashAndBurn(CHeli *pHeli)
{
    pHeli->SetIsOutOfControl();

	// If we've hit a building we blow up
	const CCollisionHistory* pCollisionHistory = pHeli->GetFrameCollisionHistory();
	if(!pHeli->m_nPhysicalFlags.bRenderScorched
		&& pCollisionHistory->GetCollisionImpulseMagSum() > 5.0f*pHeli->GetMass() )
	{
		const CCollisionRecord * pColRecord = pCollisionHistory->GetMostSignificantCollisionRecordOfTypes(ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING);
		
		if( pColRecord != NULL && pColRecord->m_MyCollisionNormal.z > 0.3f )
		{
			SetState(State_Wrecked);
		}
	}

	if(pHeli->HasContactWheels())//if we've touched the ground then blow up as well
	{
		//If the heli is blowing up instantly or in a cutscene, just wreck it.
		if(!IsCrashFlagSet(CF_BlowUpInstantly) && !IsCrashFlagSet(CF_AddExplosion) && !IsCrashFlagSet(CF_InACutscene) && !pHeli->m_nVehicleFlags.bExplodesOnNextImpact)
		{
			//If the heli touches ground while not moving very fast, no need to explode it.
			const Vector3& vVelocity = pHeli->GetVelocity();
			if(vVelocity.Mag2() < square(sm_Tunables.m_MinSpeedForWreck))
			{
				pHeli->FinishBlowingUpVehicle(m_culprit.GetEntity(), false, false, false, m_iWeaponHash);
				SetState(State_Exit);
				return FSM_Continue;
			}
		}

		//Wreck the heli.
		SetState(State_Wrecked);
	}

	// If heli landed upsidedown, we still want it to blow up
	if((pHeli->IsUpsideDown() || pHeli->IsOnItsSide()) && pCollisionHistory->GetCollisionImpulseMagSum() > 0.0f)
	{
		//If the heli is blowing up instantly or in a cutscene, just wreck it.
		if(!IsCrashFlagSet(CF_BlowUpInstantly) && !IsCrashFlagSet(CF_AddExplosion) && !IsCrashFlagSet(CF_InACutscene) && !pHeli->m_nVehicleFlags.bExplodesOnNextImpact)
		{
			pHeli->FinishBlowingUpVehicle(m_culprit.GetEntity(), false, false, false, m_iWeaponHash);
			SetState(State_Exit);
			return FSM_Continue;
		}

		//Wreck the heli.
		SetState(State_Wrecked);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////

bool CTaskVehicleCrash::CheckForCollisionWithGroundWhenFarFromPlayer(CVehicle* pVehicle)
{
	// If the crashing vehicle is so far from the player that map collision isn't available, use
	// the height map If we are below the height found from averaging the max and min from the
	// height map at that position, blow up the vehicle at that point.

	Vec3V vVehiclePos = pVehicle->GetVehiclePosition();
	if(g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(vVehiclePos, fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
	{
		return false;
	}
	else
	{
		float x = vVehiclePos.GetXf();
		float y = vVehiclePos.GetYf();
		float fMinHeightAtPos = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(x, y);
		float fMaxHeightAtPos = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(x, y);
		float fAvgHeightAtPos = 0.6f*(fMinHeightAtPos+fMaxHeightAtPos);

		if(vVehiclePos.GetZf() < fAvgHeightAtPos)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}

////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleCrash::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Crash&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Crash",
		"State_Wrecked",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif


/////////////////////////////////////////////////////////////////////////////////
static dev_float s_fMetersBelowToLand = 10.0f;
CTaskVehicleLand::CTaskVehicleLand(const sVehicleMissionParams& params, const s32 iHeliFlags, const float fHeliRequestedOrientation) :
CTaskVehicleGoToHelicopter(params, iHeliFlags, fHeliRequestedOrientation)
{
	m_iMinHeightAboveTerrainCached = 0;
	m_fSlowDownDistanceMin = s_fMetersBelowToLand - 1.0f;


	m_ShapeTestResults = rage_new WorldProbe::CShapeTestSingleResult[4];

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_LAND);
}

/////////////////////////////////////////////////////////////////////////////////

CTaskVehicleLand::~CTaskVehicleLand()
{
	delete[] m_ShapeTestResults;
}


/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleLand::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Land state
		FSM_State(State_Land)
			FSM_OnEnter
				Land_OnEnter(pVehicle);
			FSM_OnUpdate
				return Land_OnUpdate(pVehicle);
	FSM_End
}

void	CTaskVehicleLand::Land_OnEnter(CVehicle* pVehicle)
{
	CHeli* pHeli = (CHeli *)pVehicle;
	pHeli->SwitchEngineOn();

	m_iMinHeightAboveTerrainCached = m_iMinHeightAboveTerrain;
	m_fPreviousTargetHeight = 0.0f;
	m_fTimeOnTheGround = 0.0f;

	pHeli->GetHeliIntelligence()->SetRetractLandingGearTime(0xffffffff);
	CLandingGear::eLandingGearPublicStates eGearState = pHeli->GetLandingGear().GetPublicState();
	if ( eGearState == CLandingGear::STATE_RETRACTING || eGearState == CLandingGear::STATE_LOCKED_UP )
	{
		pHeli->GetLandingGear().ControlLandingGear(pHeli, CLandingGear::COMMAND_DEPLOY);
	}
}

bool CTaskVehicleLand::CheckForPedCollision(CVehicle& in_Vehicle) const
{
	const Vector3 vHeliPosition = VEC3V_TO_VECTOR3(in_Vehicle.GetVehiclePosition());
	CEntityScannerIterator entityList = in_Vehicle.GetIntelligence()->GetPedScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		Assert(pEntity->GetIsTypePed());
		const CPed* pPed = static_cast<const CPed*>(pEntity);
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// Don't process vehicles that have been hidden
		if (in_Vehicle.InheritsFromHeli() && (m_iHeliFlags & HF_IgnoreHiddenEntitiesDuringLand) && !pPed->GetIsVisible())
		{
			continue;
		}

		if ( !pPed->GetIsInVehicle() && (pPed->IsAPlayerPed() || pPed->GetMyVehicle() != &in_Vehicle) )
		{
			static float s_HeightDeltaHeightThreshold = 10.0f;
			float fDeltaHeight = vHeliPosition.z - vPedPosition.z;
			if ( fDeltaHeight < s_HeightDeltaHeightThreshold )
			{
				Vec2V vHeliPos2d = Vec2V(vHeliPosition.x, vHeliPosition.y);
				Vec2V vPedPos2d = Vec2V(vPedPosition.x, vPedPosition.y);

				ScalarV fDistSquared = DistSquared(vPedPos2d, vHeliPos2d);
				static float s_BoundRadiusScalar = 0.33f;
				ScalarV fDistSquaredThreshold = ScalarV(square(in_Vehicle.GetBoundRadius() * s_BoundRadiusScalar));

				if ( ( fDistSquared < fDistSquaredThreshold).Getb() )
				{
					return true;
				}
			}	
		}		
	}
	return false;
}

bool CTaskVehicleLand::CheckForVehicleCollision(CVehicle& in_Vehicle) const
{
	const Vector3 vHeliPosition = VEC3V_TO_VECTOR3(in_Vehicle.GetVehiclePosition());
	CEntityScannerIterator entityList = in_Vehicle.GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		Assert(pEntity->GetIsTypeVehicle());
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
		const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

		// Don't process vehicles that have been hidden
		if (in_Vehicle.InheritsFromHeli() && (m_iHeliFlags & HF_IgnoreHiddenEntitiesDuringLand) && !pVehicle->GetIsVisible())
		{
			continue;
		}

		static float s_HeightDeltaHeightThreshold = 15.0f;
		float fDeltaHeight = vHeliPosition.z - vVehiclePosition.z;
		if ( fDeltaHeight < s_HeightDeltaHeightThreshold )
		{
			Vec2V vHeliPos2d = Vec2V(vHeliPosition.x, vHeliPosition.y);
			Vec2V vVehPos2d = Vec2V(vVehiclePosition.x, vVehiclePosition.y);

			ScalarV fDistSquared = DistSquared(vVehPos2d, vHeliPos2d);
			

			ScalarV fDistSquaredThreshold;
			if ( pVehicle->InheritsFromHeli() )
			{
				// wtf probably never going to work just add both bounds so they will never touch
				fDistSquaredThreshold = ScalarV(square(in_Vehicle.GetBoundRadius() + pVehicle->GetBoundRadius() ) );
			}
			else
			{
				// get probably too close 1 radius away.  could still be in other's radius
				fDistSquaredThreshold = ScalarV(square(in_Vehicle.GetBoundRadius()/2.0f));
			}

			if ( ( fDistSquared < fDistSquaredThreshold).Getb() )
			{
				return true;
			}
		}	
	}
	return false;

}
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Land_OnUpdate
// PURPOSE :  Will manouver a heli to fly to certain coordinates and land
/////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return CTaskVehicleLand::Land_OnUpdate(CVehicle* pVehicle)
{	
	Assert(pVehicle->InheritsFromHeli());

	m_bRunningLandingTask = false;

	CHeli* pHeli = (CHeli *)pVehicle;

	const Vector3 vHeliPosition = VEC3V_TO_VECTOR3(pHeli->GetVehiclePosition());
	float DistanceXY = (m_Params.GetTargetPosition() - vHeliPosition).XYMag();
	
	// When we get within range we stop turning and fix our orientation.
	if(DistanceXY < 50.0f && m_iHeliFlags & HF_AttainRequestedOrientation)
	{
		//	VECTORMATH FIX - Could use pHeli->GetTransform().GetHeading() here?
		Vector3 vecForward(VEC3V_TO_VECTOR3(pHeli->GetTransform().GetB()));
		m_fHeliRequestedOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
	}


	// If we're close enough and we're not moving we'll start our decent 
	// Only start the descent if we've got collision though, since we rely on the ground to stop us -JM
	bool bCollisionLoaded = true;
	bool bShouldDescend = false;
	if(DistanceXY < 10.0f && pHeli->GetAiXYSpeed() < 2.5f)
	{
		bShouldDescend = true;
		bCollisionLoaded = pHeli->IsCollisionLoadedAroundPosition();
		if (bCollisionLoaded)
		{
			m_iMinHeightAboveTerrain = 0;
			Vector3 vTargetPos = m_Params.GetTargetPosition();
			vTargetPos.z = 0.0f;
			m_Params.SetTargetPosition(vTargetPos);
		}
		else
		{
			m_iMinHeightAboveTerrain = m_iMinHeightAboveTerrainCached;
			Vector3 vTargetPos = m_Params.GetTargetPosition();
			vTargetPos.z = vHeliPosition.z;
			m_Params.SetTargetPosition(vTargetPos);
		}
	}


	if ( pHeli->HasContactWheels() ) 
	{
		m_fTimeOnTheGround += GetTimeStep();
	}
	else
	{
		m_fTimeOnTheGround = 0.0f;
	}
	
	static float s_TimeThresholdForTimeOnGround = 1.0f;
	bool bOnTheGround = m_fTimeOnTheGround > s_TimeThresholdForTimeOnGround;

	float fGroundHeight = m_fPreviousTargetHeight + s_fMetersBelowToLand;
	float fHeightAboveGroundTarget = vHeliPosition.z - fGroundHeight;
	if(fabsf(fHeightAboveGroundTarget) <= 1.0f ) // really close to estimated ground height
	{
		// if the heli touches the ground we switch everything off.
		if ( bOnTheGround )
		{
			pHeli->SetThrottleControl(0.0f);
			pHeli->SetYawControl(0.0f);
			pHeli->SetPitchControl(0.0f);
			pHeli->SetRollControl(0.0f);

			if (!IsDrivingFlagSet(DF_DontTerminateTaskWhenAchieved))
			{
				pHeli->SwitchEngineOff();
				m_bMissionAchieved = true;
				return FSM_Quit;
			}
		}
	}

	if (!bOnTheGround)
	{
		Vector3 vIntermediateTargetPos = m_Params.GetTargetPosition();
		// don't actually descent until shoulddescend is true
		vIntermediateTargetPos.z = vHeliPosition.z;

		if (bShouldDescend && bCollisionLoaded)
		{
			m_bRunningLandingTask = true;

			const Vector3 vHeliForward = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetForward());
			const Vector3 vHeliRight = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetRight());

			Vector3 vHeliOffsets[eNumGroundProbes];
			const float fBoundRadius = pHeli->GetBoundRadius();

			// these are meant to form a kind of cage around the helicopter
			// so left right projected out a bit.  Forward projected almost to nose of heli
			// and back just slid slightly back.  
			vHeliOffsets[eGroundProbe_Front] = vHeliForward * fBoundRadius/2;
			vHeliOffsets[eGroundProbe_Back] = -vHeliForward;
			vHeliOffsets[eGroundProbe_Right] = vHeliRight * fBoundRadius/4;
			vHeliOffsets[eGroundProbe_Left] = -vHeliRight * fBoundRadius/4;

			if (   m_ShapeTestResults[eGroundProbe_Front].GetResultsReady()
				&& m_ShapeTestResults[eGroundProbe_Back].GetResultsReady()
				&& m_ShapeTestResults[eGroundProbe_Left].GetResultsReady()
				&& m_ShapeTestResults[eGroundProbe_Right].GetResultsReady())
			{
				float fMinGroundHeight = FLT_MAX;
				float fMaxGroundHeight = -FLT_MAX;
				bool bHitSomething = false;
				int minindex = 0;
				for(int i = 0; i < eNumGroundProbes; i++ )
				{
					if (m_ShapeTestResults[i].GetNumHits() > 0)
					{
						bHitSomething = true;
						float fGroundHeight = m_ShapeTestResults[i][0].GetPosition().GetZf();

						if ( fGroundHeight < fMinGroundHeight)
						{
							fMinGroundHeight = fGroundHeight;
							minindex = i;
						}

						if ( fGroundHeight > fMaxGroundHeight )
						{
							fMaxGroundHeight = fGroundHeight;
						}
					}
					m_ShapeTestResults[i].Reset();
				}

				if ( bHitSomething )
				{
					m_fPreviousTargetHeight = fMaxGroundHeight - s_fMetersBelowToLand;
				}
				else
				{
					m_fPreviousTargetHeight = vIntermediateTargetPos.z - 25.0f;
				}

				static float s_MaxVarience = 2.0f;
				if ( fMaxGroundHeight - fMinGroundHeight > s_MaxVarience )
				{
					Vector3 vNewTarget = (*GetTargetPosition()) + vHeliOffsets[minindex];
					SetTargetPosition(&vNewTarget); // steer towards min
				}				
			}

			if ( !m_ShapeTestResults[eGroundProbe_Front].GetWaitingOnResults()
					&& !m_ShapeTestResults[eGroundProbe_Back].GetWaitingOnResults()
					&& !m_ShapeTestResults[eGroundProbe_Left].GetWaitingOnResults()
					&& !m_ShapeTestResults[eGroundProbe_Right].GetWaitingOnResults())						
			{	
				static dev_float s_fMetersBelowToAimFor = 50.0f;
			
				const Vector3 vStartPos(vHeliPosition.x, vHeliPosition.y, vHeliPosition.z );
				const Vector3 vEndPos(vIntermediateTargetPos.x, vIntermediateTargetPos.y, vIntermediateTargetPos.z - s_fMetersBelowToAimFor);
					
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_RIVER_TYPE);
				probeDesc.SetExcludeInstance(pHeli->GetCurrentPhysicsInst());

				for ( int i = 0; i < eNumGroundProbes; i++ )
				{
					probeDesc.SetResultsStructure(&m_ShapeTestResults[i]);
					probeDesc.SetMaxNumResultsToUse(1);			
					probeDesc.SetStartAndEnd(vStartPos + vHeliOffsets[i], vEndPos + vHeliOffsets[i]);
					WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST); 
				}
			}
			
			vIntermediateTargetPos.z = m_fPreviousTargetHeight;

			static float s_TurbulanceHeight = 7.0f;
			if ( vHeliPosition.z - fGroundHeight < s_TurbulanceHeight )
			{
				pHeli->DisableTurbulenceThisFrame(true);
			}

			static float s_DontLandOnPedsHeight = 7.0f;
			if ( vHeliPosition.z - fGroundHeight < s_DontLandOnPedsHeight )
			{
				if ( CheckForPedCollision(*pHeli))
				{
					// mark landing blocked
					pHeli->GetHeliIntelligence()->MarkLandingAreaBlocked();

					// pause in air don't land on peds
					vIntermediateTargetPos.z = m_fPreviousTargetHeight + (s_DontLandOnPedsHeight * 0.95f);
				}
			}

			static float s_DontLandOnVehiclesHeight = 10.0f;
			if ( vHeliPosition.z - fGroundHeight < s_DontLandOnVehiclesHeight )
			{
				if ( CheckForVehicleCollision(*pHeli))
				{
					// mark landing blocked
					pHeli->GetHeliIntelligence()->MarkLandingAreaBlocked();

					// pause in air don't land on vehicles
					vIntermediateTargetPos.z = m_fPreviousTargetHeight + (s_DontLandOnVehiclesHeight * 0.95f);
				}
			}

		}
		
		SteerToTarget(pHeli, vIntermediateTargetPos, false);
	}

	return FSM_Continue;
}

//
//
//
#if !__FINAL
const char * CTaskVehicleLand::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Land&&iState<=State_Land);
	static const char* aStateNames[] = 
	{
		"State_Land"
	};

	return aStateNames[iState];
}
#endif


/////////////////////////////////////////////////////////////////////////////////

CTaskVehicleHover::CTaskVehicleHover()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_HOVER);
}

/////////////////////////////////////////////////////////////////////////////////

CTaskVehicleHover::~CTaskVehicleHover()
{

}


/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleHover::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pVehicle);
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		// Hover state
		FSM_State(State_HoverHeli)
			FSM_OnEnter
				HoverHeli_OnEnter(pVehicle);
			FSM_OnUpdate
				return HoverHeli_OnUpdate(pVehicle);
			FSM_OnExit
				HoverHeli_OnExit(pVehicle);

		FSM_State(State_HoverVtol)
			FSM_OnEnter
				HoverVtol_OnEnter(pVehicle);
			FSM_OnUpdate
				return HoverVtol_OnUpdate(pVehicle);
	FSM_End
}

//////////////////////////////////////////////////////////////////////
//State_Start
//////////////////////////////////////////////////////////////////////
void CTaskVehicleHover::Start_OnEnter(CVehicle* /*pVehicle*/)
{

}

CTask::FSM_Return CTaskVehicleHover::Start_OnUpdate(CVehicle* pVehicle)
{
	if (pVehicle->InheritsFromHeli())
	{
		SetState(State_HoverHeli);
		return FSM_Continue;
	}
	else if (pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		if (pPlane->GetVerticalFlightModeAvaliable())
		{
			SetState(State_HoverVtol);
			return FSM_Continue;
		}
	}
	
	Assertf(0, "Tried hovering on a vehicle that doesn't support it! %s", pVehicle->GetModelName());
	return FSM_Quit;
}

//////////////////////////////////////////////////////////////////////
//State_HoverHeli
//////////////////////////////////////////////////////////////////////
void CTaskVehicleHover::HoverHeli_OnEnter	(CVehicle* pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CHeli*>(pVehicle));
	CHeli *pHeli= static_cast<CHeli*>(pVehicle);

	// Work out some inputs that will try to keep the heli's speed as close to stationary as possible.
	Vector3 vecForward(VEC3V_TO_VECTOR3(pHeli->GetTransform().GetB()));
	float HeliRequestedOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);

	Vector3 v = VEC3V_TO_VECTOR3(pHeli->GetTransform().GetPosition());

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = 0.0f;
	params.SetTargetPosition(v);
	params.m_fTargetArriveDist = 0.0f;

	if ( !pVehicle->IsInAir() )
	{
		// prevent getting lift we are on the ground
		pHeli->SetCollectiveControl(0.0f);
	}

	SetNewTask(rage_new CTaskVehicleGoToHelicopter(params, CTaskVehicleGoToHelicopter::HF_AttainRequestedOrientation, HeliRequestedOrientation) );
}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleHover::HoverHeli_OnUpdate	(CVehicle* pVehicle)
{

	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CHeli*>(pVehicle));
	CHeli *pHeli= static_cast<CHeli*>(pVehicle);
	if ( !pVehicle->IsInAir() )
	{
		// prevent getting lift we are on the ground
		pHeli->SetCollectiveControl(0.0f);
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER))
	{
		// If the subtask has terminated - restart
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskVehicleHover::HoverHeli_OnExit(CVehicle* UNUSED_PARAM(pVehicle))
{
}

//////////////////////////////////////////////////////////////////////
//State_HoverVtol
//////////////////////////////////////////////////////////////////////
void CTaskVehicleHover::HoverVtol_OnEnter	(CVehicle* pVehicle)
{
	//cast the vehicle into the applicable type
	Assert(dynamic_cast<CPlane*>(pVehicle));
	CPlane *pPlane= static_cast<CPlane*>(pVehicle);

	// Work out some inputs that will try to keep the heli's speed as close to stationary as possible.
	Vector3 vecForward(VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB()));
	float RequestedOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);

	Vector3 v = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition());

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = 0.0f;
	params.SetTargetPosition(v);
	params.m_fTargetArriveDist = 0.0f;
	SetNewTask(rage_new CTaskVehicleGoToPlane(params, 30, 20, true, true, RequestedOrientation));
}

//////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleHover::HoverVtol_OnUpdate	(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_PLANE))
	{
		// If the subtask has terminated - restart
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	return FSM_Continue;
}

//
//
//
#if !__FINAL
const char * CTaskVehicleHover::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_HoverVtol);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_HoverHeli",
		"State_HoverVtol"
	};

	return aStateNames[iState];
}
#endif

int CTaskVehicleFleeAirborne::ComputeFlightHeightAboveTerrain(const CVehicle& in_Vehicle, int iMinHeight)
{
	Vec3V vPosition = in_Vehicle.GetTransform().GetPosition();
	ScalarV vTerrainZ = ScalarV(CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPosition.GetXf(), vPosition.GetYf()));
	int iFlightHeight = static_cast<int>((vPosition.GetZ()	- vTerrainZ).Getf());
	iFlightHeight = Max(iFlightHeight, iMinHeight);

#if __ASSERT
	const int NUM_SYNCED_HEIGHT_BITS	= 12;
	const int MAX_VALUE_SYNCED_HEIGHT	= (1u<<NUM_SYNCED_HEIGHT_BITS)-1;
	if(NetworkInterface::IsGameInProgress() && iFlightHeight > MAX_VALUE_SYNCED_HEIGHT)
	{
		taskAssertf(0,"%s iFlightHeight %d out of sync range %d. iMinHeight %d. vTerrainZ %.2f, vPosition %.2f, %.2f, %.2f",
			in_Vehicle.GetDebugName(),
			iFlightHeight,
			MAX_VALUE_SYNCED_HEIGHT,
			iMinHeight,
			vTerrainZ.Getf(),
			vPosition.GetX().Getf(),
			vPosition.GetY().Getf(),
			vPosition.GetZ().Getf());
	}
#endif
	return iFlightHeight;
}


CTaskVehicleFleeAirborne::CTaskVehicleFleeAirborne(const sVehicleMissionParams& params, const int iFlightHeight /* = 30 */, const int iMinHeightAboveTerrain /* = 20 */, const bool bUseDesiredOrientation /* = false */, const float fDesiredOrientation /* = 0.0f */, const float fDesiredSlope /*= 0.0f*/)
: CTaskVehicleMissionBase(params)
, m_iFlightHeight(iFlightHeight)
, m_iMinHeightAboveTerrain(iMinHeightAboveTerrain)
, m_bUseDesiredOrientation(bUseDesiredOrientation)
, m_fDesiredOrientation(fDesiredOrientation)
, m_fDesiredSlope(fDesiredSlope)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FLEE_AIRBORNE);
}

CTaskVehicleFleeAirborne::~CTaskVehicleFleeAirborne()
{

}

aiTask::FSM_Return CTaskVehicleFleeAirborne::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter(pVehicle);
			FSM_OnUpdate
				return Flee_OnUpdate(pVehicle);
	FSM_End
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleFleeAirborne::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Flee&&iState<=State_Flee);
	static const char* aStateNames[] = 
	{
		"State_Flee",
	};

	return aStateNames[iState];
}

// void CTaskVehicleFleeAirborne::Debug() const
// {
// }
#endif

Vector3::Return CTaskVehicleFleeAirborne::CalculateTargetCoords(const CVehicle* pVehicle)
{
	Vector3 vTargetPos;
	Vector3 vFleeOrigin;
	FindTargetCoors(pVehicle, vFleeOrigin);
	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());

	Vector3 vDelta = vVehiclePos - vFleeOrigin;

	if ( vDelta.Mag() <= FLT_EPSILON )
	{
		vDelta = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
	}
	else
	{
		vDelta.Normalize();
	}

	//just set this to some far distance in the opposite direction away from the source
	vTargetPos = vVehiclePos + (vDelta * 1000.0f);

	vTargetPos.z = Max( CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vTargetPos.x, vTargetPos.y) + m_iMinHeightAboveTerrain, vVehiclePos.z);
	vTargetPos.z = Max( m_fDesiredSlope * 1000.0f + vVehiclePos.z, vTargetPos.z);

	//give these a buffer of a bit so that vehicles that somehow reach their target don't go out of bounds.
	const static float s_fLimitBuffer = 2000.0f;
	vTargetPos.x = Clamp(vTargetPos.x, WORLDLIMITS_XMIN + s_fLimitBuffer, WORLDLIMITS_XMAX - s_fLimitBuffer);
	vTargetPos.y = Clamp(vTargetPos.y, WORLDLIMITS_YMIN + s_fLimitBuffer, WORLDLIMITS_YMAX - s_fLimitBuffer);
	vTargetPos.z = Clamp(vTargetPos.z, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX);

	return vTargetPos;
}

void CTaskVehicleFleeAirborne::Flee_OnEnter(CVehicle* pVehicle)
{
	Vector3 vTargetPos = CalculateTargetCoords(pVehicle);
	sVehicleMissionParams params = m_Params;
	params.SetTargetPosition(vTargetPos);
	params.ClearTargetEntity();

	if (pVehicle->InheritsFromHeli())
	{
		const float fDesiredOrientationForHeli = m_bUseDesiredOrientation ? m_fDesiredOrientation : -1.0f;
		SetNewTask(rage_new CTaskVehicleGoToHelicopter(params, 0, fDesiredOrientationForHeli, m_iMinHeightAboveTerrain));
		return;
	}
	else if (pVehicle->InheritsFromPlane())
	{	
		SetNewTask(rage_new CTaskVehicleGoToPlane(params, m_iFlightHeight, m_iMinHeightAboveTerrain, false, m_bUseDesiredOrientation, m_fDesiredOrientation));
		if ( !pVehicle->PopTypeIsMission() )
		{
			static u32 s_RetractLandingGearTimer = 3000;
			static_cast<CPlane*>(pVehicle)->GetPlaneIntelligence()->SetRetractLandingGearTime(fwTimer::GetTimeInMilliseconds() + s_RetractLandingGearTimer);
		}
		return;
	}

	Assertf(0, "TASK_VEHICLE_FLEE_AIRBORNE used on a non-flying vehicle! Not supported");
}

aiTask::FSM_Return CTaskVehicleFleeAirborne::Flee_OnUpdate(CVehicle* pVehicle)
{
	Vector3 vTargetPos = CalculateTargetCoords(pVehicle);
	CTask* pSubtask = GetSubTask();
	if (pSubtask && pSubtask->IsVehicleMissionTask())
	{
		CTaskVehicleMissionBase* pVehMission = static_cast<CTaskVehicleMissionBase*>(pSubtask);
		pVehMission->SetTargetPosition(&vTargetPos);
	}

	return FSM_Continue;
}
