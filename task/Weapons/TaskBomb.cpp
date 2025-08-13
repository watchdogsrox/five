// File header
#include "Task/Weapons/TaskBomb.h"

// Game headers
#include "Network/General/NetworkPendingProjectiles.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "System/control.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Weapons/Weapon.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "weapons/projectiles/Projectile.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskBomb
//////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskBomb::ms_PlantInFrontClipId(ATSTRINGHASH("plant_vertical", 0x03009a70e));
fwMvClipId CTaskBomb::ms_PlantOnGroundClipId(ATSTRINGHASH("plant_floor", 0x0f968b075));
dev_float CTaskBomb::ms_fVerticalProbeDistance = 1.0f;
dev_float CTaskBomb::ms_fMaxPlantGroundNormalZ = 0.95f;
dev_float CTaskBomb::ms_fUphillMaxPlantGroundNormalZ = 0.998f;

CTaskBomb::CTaskBomb()
: m_weaponControllerType(CWeaponController::WCT_Fire)
, m_fDuration(-1)
, m_pTargetEntity(NULL)
, m_vTargetPosition(Vector3::ZeroType)
, m_vTargetNormal(Vector3::ZeroType)
, m_bHasToBeAttached(false)
, m_bClonePlantInFront(false)
, m_bHasPriorPosition(false)
{
	SetInternalTaskType(CTaskTypes::TASK_BOMB);
}

CTaskBomb::CTaskBomb(const CWeaponController::WeaponControllerType weaponControllerType, float fDuration, bool bHasToBeAttached)
: m_weaponControllerType(weaponControllerType)
, m_fDuration(fDuration)
, m_vTargetPosition(Vector3::ZeroType)
, m_vTargetNormal(Vector3::ZeroType)
, m_bHasToBeAttached(bHasToBeAttached)
, m_bClonePlantInFront(false)
, m_bHasPriorPosition(false)
{
	SetInternalTaskType(CTaskTypes::TASK_BOMB);
}

CTaskBomb::~CTaskBomb()
{
}

CTaskBomb::FSM_Return CTaskBomb::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableDynamicCapsuleRadius, true );

	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);

		if(m_ClipId == ms_PlantOnGroundClipId)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_FPSPlantingBombOnFloor, true);
		}
	}

	return FSM_Continue;
}

CTaskBomb::FSM_Return CTaskBomb::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	weaponAssert( pPed->GetWeaponManager() );

	FSM_Begin
		FSM_State( State_Init )
			FSM_OnUpdate
				return StateInit_OnUpdate( pPed );
		FSM_State( State_SlideAndPlant )
			FSM_OnEnter
				return StateSlideAndPlant_OnEnter( pPed );
			FSM_OnUpdate
				return StateSlideAndPlant_OnUpdate( pPed );
			FSM_OnExit
				return StateSlideAndPlant_OnExit( pPed );
		FSM_State( State_Swap )
			FSM_OnEnter
				return StateSwap_OnEnter( pPed );
			FSM_OnUpdate
				return StateSwap_OnUpdate( pPed );
		FSM_State( State_Quit )
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskInfo*	CTaskBomb::CreateQueriableState() const
{
	return rage_new CClonedBombInfo( GetState(), m_vTargetPosition, m_vTargetNormal, m_bClonePlantInFront );
}

CTask::FSM_Return CTaskBomb::UpdateClonedFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State( State_Init )
			FSM_OnUpdate
				return StateInit_OnUpdateClone( pPed );
		FSM_State( State_SlideAndPlant )
			FSM_OnEnter
				return StateSlideAndPlant_OnEnter( pPed );
			FSM_OnUpdate
				return StateSlideAndPlant_OnUpdate( pPed );
			FSM_OnExit
				return StateSlideAndPlant_OnExit( pPed );
		FSM_State( State_Swap )
	FSM_End
}

#if !__FINAL
void CTaskBomb::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Sphere( m_vTargetPosition, 0.1f, Color_white, false );
	grcDebugDraw::Line( m_vTargetPosition, m_vTargetPosition + m_vTargetNormal, Color_blue );
#endif // DEBUG_DRAW

	// Base class
	CTask::Debug();
}

const char * CTaskBomb::GetStaticStateName( s32 iState  )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"SlideAndPlant",
		"Swap",
		"Quit",
	};

	return aStateNames[ iState ];
}
#endif // !__FINAL

CTaskBomb::FSM_Return CTaskBomb::StateInit_OnUpdate( CPed* pPed )
{
	if( pPed->GetWeaponManager()->GetRequiresWeaponSwitch() )
	{
		// Swap weapon, as we have the incorrect (or holstered) weapon equipped
		SetState( State_Swap );
		return FSM_Continue;
	}
	else
	{
		static dev_float sfProbeRadius = 0.1f;
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestSingleResult capsuleResult;
		capsuleDesc.SetResultsStructure( &capsuleResult );
		capsuleDesc.SetIncludeFlags( (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PROJECTILE_TYPE & ~ArchetypeFlags::GTA_PED_TYPE) );
		capsuleDesc.SetTypeFlags( ArchetypeFlags::GTA_WEAPON_TEST );
		capsuleDesc.SetExcludeEntity( pPed );
		capsuleDesc.SetIsDirected( true );	// Only interested in first contact.

		Vec3V vStart = pPed->GetTransform().GetPosition();
		vStart.SetZf( vStart.GetZf() );

		float fDesiredHeading = pPed->GetDesiredHeading();
		Vec3V vPedForward( -rage::Sinf( fDesiredHeading ), rage::Cosf( fDesiredHeading ), 0.0f );
		vPedForward = NormalizeSafe( vPedForward, pPed->GetTransform().GetB() );
		Vec3V vEnd = vStart + Scale( vPedForward, ScalarV( ms_fVerticalProbeDistance ) );

		capsuleDesc.SetCapsule( VEC3V_TO_VECTOR3( vStart ), VEC3V_TO_VECTOR3( vEnd ), sfProbeRadius );

		// Clear any previous resets
		if( WorldProbe::GetShapeTestManager()->SubmitTest( capsuleDesc ) )
		{
			// Store the intersection position and normal.
			m_pTargetEntity = CPhysics::GetEntityFromInst( capsuleResult[0].GetHitInst() );
			m_vTargetPosition = capsuleResult[0].GetHitPosition();
			m_vTargetNormal = capsuleResult[0].GetHitNormal();

			m_ClipId = ms_PlantInFrontClipId;
			m_bClonePlantInFront = true; //set this to sync remote
			m_bHasPriorPosition = false;

			// Switch to move to state
			SetState( State_SlideAndPlant );
			return FSM_Continue;
		}

		// If this needs to be force planted
		if( !m_bHasToBeAttached )
		{
			m_vTargetPosition = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
			m_vTargetNormal.Zero();

			// Plant on the ground
			m_ClipId = ms_PlantOnGroundClipId;

#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_FPSPlantingBombOnFloor, true);
			}
#endif
			static dev_float sf_ActionModeTime = 3.0f;
			if (!pPed->IsUsingStealthMode())
				pPed->SetUsingActionMode(true, CPed::AME_Combat, sf_ActionModeTime);

			SetState( State_SlideAndPlant );
			return FSM_Continue;
		}
	}

	// Can't be placed
	return FSM_Quit;
}

CTaskBomb::FSM_Return CTaskBomb::StateInit_OnUpdateClone( CPed* pPed )
{
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if (!pWeaponInfo || !pWeaponInfo->GetIsProjectile())
	{
		//wait until clone has equipped the bomb
		return FSM_Continue;
	}

	//Check if clone has been blocked from getting to placement position used on the remote
	Vector3 vCloneExpectedPosition		= NetworkInterface::GetLastPosReceivedOverNetwork(pPed);
	Vector3 vCloneActualPosition		= VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	float fCloneDistanceFromExpectedSq	= (vCloneExpectedPosition - vCloneActualPosition).Mag2();
	const float MAX_CLONE_DISTANCE_FROM_EXPECTED_PLACEMENT_POSITION = 0.5f;

	if(fCloneDistanceFromExpectedSq > rage::square(MAX_CLONE_DISTANCE_FROM_EXPECTED_PLACEMENT_POSITION) )
	{
		NetworkInterface::GoStraightToTargetPosition(pPed);
	}

	if( GetStateFromNetwork() == State_SlideAndPlant )
	{
		CTaskFSMClone* pMainTask = this;
		
		if (GetParent() && GetParent()->IsClonedFSMTask())
		{
			pMainTask = static_cast<CTaskFSMClone*>(GetParent());
		}

		ProjectileInfo* pProjectileInfo = pMainTask ? CNetworkPendingProjectiles::GetProjectile(pPed, pMainTask->GetNetSequenceID(), false) : NULL;

		if (pProjectileInfo)
		{
			CTaskInfo* pInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_BOMB, PED_TASK_PRIORITY_MAX, false);
			m_vTargetNormal.Zero();

			if (pInfo)
			{
				CClonedBombInfo* pTaskBombInfo = static_cast<CClonedBombInfo*>(pInfo);

				m_vTargetPosition = pTaskBombInfo->GetTargetPosition();
				
				if(pTaskBombInfo->GetPlantInFront())
				{
					// Plant in front
					m_ClipId = ms_PlantInFrontClipId;
					m_vTargetNormal = pTaskBombInfo->GetTargetNormal();
					m_vTargetNormal.Normalize();
				}
				else
				{
					// Plant on the ground
					m_ClipId = ms_PlantOnGroundClipId;
				}
			}
			else
			{
				m_vTargetPosition = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );
				// Plant on the ground
				m_ClipId = ms_PlantOnGroundClipId;
			}

			SetState(State_SlideAndPlant);
		}
	}

	return FSM_Continue;
}

CTaskBomb::FSM_Return CTaskBomb::StateSlideAndPlant_OnEnter( CPed* pPed )
{
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if( !pWeapon )
	{
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
	if( !pWeaponInfo )
	{
		return FSM_Quit;
	}

	// when parent task TaskThrowAndAimProjectile starts, it blocks weapon syncing on the clone
	// if we swap weapon to stickybomb and immediately start TaskThrowAndAimProjectile we can 
	// block changing weapons before the inventory has changed the weapon to stickybombs.
	// if we catch that and our inventory says we are waiting for the weapon to be applied, 
	// force it...
	if( pPed->IsNetworkClone() )
	{
		if(pWeaponInfo->GetHash() != WEAPONTYPE_STICKYBOMB)
		{
			CNetObjPed* netObjPed = static_cast<CNetObjPed*>(pPed->GetNetworkObject());
			if(netObjPed && netObjPed->CacheWeaponInfo())
			{
				if(netObjPed->GetCachedWeaponInfo().m_weapon == WEAPONTYPE_STICKYBOMB)
				{
					netObjPed->UpdateCachedWeaponInfo(true);

					const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					if( !pWeapon )
					{
						return FSM_Quit;
					}

					pWeaponInfo = pWeapon->GetWeaponInfo();
					if( !pWeaponInfo )
					{
						return FSM_Quit;
					}
				}
			}
		}
	}

	Assert( !m_vTargetPosition.IsZero() );
	
	Vector3 vSlideToPosition;
	GetTargetPosition( vSlideToPosition );

	float fDesiredHeading = m_vTargetNormal.IsZero() ? pPed->GetCurrentHeading() : GetTargetHeading();

	static dev_float SLIDE_SPEED = 1000.0f;

#if FPS_MODE_SUPPORTED
	FPSStreamingFlags fpsStreamingFlag = FPS_StreamDefault;
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		fpsStreamingFlag = FPS_StreamRNG;
	}
#endif

	CTaskSlideToCoord* pSlideToCoordTask = rage_new CTaskSlideToCoord( vSlideToPosition, fDesiredHeading, SLIDE_SPEED, m_ClipId, pWeaponInfo->GetAppropriateWeaponClipSetId(pPed FPS_MODE_SUPPORTED_ONLY(, fpsStreamingFlag)), NORMAL_BLEND_IN_DELTA );

	static dev_float HEADING_SPEED = PI * 2.0f;
	pSlideToCoordTask->SetHeadingIncrement( HEADING_SPEED );

	static dev_float POS_ACCURACY = 0.045f;
	float fAccuracy = POS_ACCURACY;

#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		// Need more precise target position in FPS mode so we don't clip hand through the wall
		static dev_float POS_ACCURACY_FPS = 0.01f;
		fAccuracy = POS_ACCURACY_FPS;
	}
#endif	//FPS_MODE_SUPPORTED

	pSlideToCoordTask->SetPosAccuracy( fAccuracy );

	// Just use zero sliding if the ground physical is moving
	pSlideToCoordTask->SetUseZeroVelocityIfGroundPhsyicalIsMoving( true );

	SetNewTask( pSlideToCoordTask );

	m_bHasPriorPosition = false;
	return FSM_Continue;
}

CTaskBomb::FSM_Return CTaskBomb::StateSlideAndPlant_OnUpdate( CPed* pPed )
{
	pPed->SetPedResetFlag( CPED_RESET_FLAG_PlacingCharge, true );

	// Have we finished the task?
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) )
	{
		return FSM_Quit;
	}

	bool bStillHasPriorPosition = false;
	CTask* pSubTask = GetSubTask();
	if( pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_SLIDE_TO_COORD )
	{
		const CClipHelper* pClipHelper = ((CTaskSlideToCoord*)pSubTask)->GetSubTaskClip();
		if( pClipHelper )
		{
			bool bSimulatedAttach = false;

			CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if( pWeaponObject )
			{
				CWeapon* pWeapon = pWeaponObject ? pWeaponObject->GetWeapon() : NULL;
				const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;

				// Should we check to see if we should attach to something?
				if( pWeaponInfo && pClipHelper->IsEvent( CClipEventTags::On ) )
				{
					const float fProjectileWidth = pWeaponObject->GetBoundingBoxMax().GetZ() - pWeaponObject->GetBoundingBoxMin().GetZ();
					Vec3V vDirection;
					Vec3V vStart;
					Vec3V vEnd;		
					if (m_bHasPriorPosition)
					{
						vStart = m_vPriorBombPosition;
						Vec3V vPosition = pWeaponObject->GetTransform().GetPosition();
						vDirection = Normalize(vPosition - vStart);
						vEnd = vPosition + Scale( vDirection, ScalarV( fProjectileWidth*2.0f ) );		
					}
					else
					{
						vDirection = Negate( pWeaponObject->GetTransform().GetC() );
						vStart = pWeaponObject->GetTransform().GetPosition() - Scale( vDirection, ScalarV( fProjectileWidth ) );
						vEnd = pWeaponObject->GetTransform().GetPosition() + Scale( vDirection, ScalarV( fProjectileWidth*2.0f ) );		
					}
#if __BANK
					CTask::ms_debugDraw.AddSphere( vStart, fProjectileWidth, Color_green, 250, atStringHash( "STICKY_WEAPON_PROBE_START_SPHERE" ), false );
					CTask::ms_debugDraw.AddSphere( vEnd, fProjectileWidth, Color_green4, 250, atStringHash( "STICKY_WEAPON_PROBE_END_SPHERE" ), false );
					CTask::ms_debugDraw.AddLine( vStart, vEnd, Color_pink, 250, atStringHash( "STICKY_WEAPON_PROBE_LINE" ) );
#endif 	
					DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("probe", vStart, vEnd, Color_pink, Color32(0,0,0)));
					WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
					WorldProbe::CShapeTestFixedResults<> capsuleResults;
					capsuleDesc.SetResultsStructure( &capsuleResults );
					capsuleDesc.SetCapsule( VEC3V_TO_VECTOR3( vStart ), VEC3V_TO_VECTOR3( vEnd ), fProjectileWidth );
					capsuleDesc.SetIncludeFlags( ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PROJECTILE_TYPE & ~ArchetypeFlags::GTA_PED_TYPE );
					capsuleDesc.SetTypeFlags( ArchetypeFlags::GTA_WEAPON_TEST );
					capsuleDesc.SetExcludeEntity( pWeaponObject );
					capsuleDesc.SetIsDirected( true );
					if( WorldProbe::GetShapeTestManager()->SubmitTest( capsuleDesc ) )
					{
						CEntity* pHitEntity = NULL;
						int nNumHitResults = capsuleResults.GetNumHits();
						int nBestHitIdx = -1;
						float fDot = -1.0f;
						float fBestHitDot = FLT_MAX;
						for( int i = 0; i < nNumHitResults; ++i )
						{
							pHitEntity = CPhysics::GetEntityFromInst( capsuleResults[i].GetHitInst() );
							if( pHitEntity && CProjectile::ShouldStickToEntity( pHitEntity, pPed, fProjectileWidth, capsuleResults[i].GetHitPositionV(), capsuleResults[i].GetHitComponent(), capsuleResults[i].GetHitMaterialId() ) )
							{
								fDot = Dot( capsuleResults[i].GetHitPositionV(), vDirection ).Getf();
								if( fDot < fBestHitDot )
								{
									nBestHitIdx = i;
									fBestHitDot = fDot;
								}
							}
						}
					
						// Check to see if we found a collision and use the best candidate
						if( nBestHitIdx > -1 )
						{
							CTaskFSMClone* pParentTask = (GetParent() && GetParent()->IsClonedFSMTask()) ? static_cast<CTaskFSMClone*>(GetParent()) : NULL;

							if (pPed->IsNetworkClone() && pParentTask)
							{
								if (!CProjectileManager::FireOrPlacePendingProjectile(pPed, pParentTask->GetNetSequenceID()))
								{
									return FSM_Quit; // No longer on pending list so likely detonated or removed by other process while animating slide so let the clone quit
								}
							}
							else
							{
								// Instead of going through fire which needs to wait to collect an impact, simply set the impact information here
								Matrix34 weaponMtx = MAT34V_TO_MATRIX34( pWeaponObject->GetMatrix() );
								CProjectile* pNewProjectile = CProjectileManager::CreateProjectile( pWeaponInfo->GetAmmoInfo()->GetHash(), pWeaponInfo->GetHash(), pPed, weaponMtx, pWeaponInfo->GetDamage(), pWeaponInfo->GetDamageType(), pWeaponInfo->GetEffectGroup() );
								if( pNewProjectile )
								{
									// Project the new hit position onto the contact plane 
									Vec3V vHitNormal = capsuleResults[nBestHitIdx].GetHitNormalV();
									if (pHitEntity)
									{
										CProjectile::FindAveragedStickySurfaceNormal( GetPed(), *pHitEntity, capsuleResults[nBestHitIdx].GetPosition(), vHitNormal, fProjectileWidth );
									}

									// Vec3V vHitDirection = pWeaponObject->GetTransform().GetPosition() - capsuleResults[nBestHitIdx].GetHitPositionV(); // Unnecessary due to commented-out part on next line.
									Vec3V vHitPosition = capsuleResults[nBestHitIdx].GetPosition();//pWeaponObject->GetTransform().GetPosition() - Scale( vHitNormal, Dot( vHitDirection, vHitNormal ) );


									// Push back the position slightly to match up with the projectile mesh (since all objects use the root position)
									vHitPosition = vHitPosition + Scale( vHitNormal, ScalarV( fProjectileWidth * 0.5f ) );
#if __BANK
									CTask::ms_debugDraw.AddSphere( vHitPosition, 0.05f, Color_red, 250, atStringHash( "STICKY_WEAPON_PROBE_HIT_POSITION" ), false );
									CTask::ms_debugDraw.AddArrow( vHitPosition, vHitPosition + capsuleResults[nBestHitIdx].GetHitNormalV(), 0.25f, Color_red, 250, atStringHash( "STICKY_WEAPON_PROBE_HIT_DIRECTION" ) );
#endif 		
									pNewProjectile->InitLifetimeValues();
									pNewProjectile->StickToEntity( pHitEntity, vHitPosition, vHitNormal, capsuleResults[nBestHitIdx].GetHitComponent(), capsuleResults[nBestHitIdx].GetHitMaterialId() );
									pWeapon->UpdateAmmoAfterFiring(pPed, pWeaponInfo, 1);

									// Mark that we have attached
									bSimulatedAttach = true;
								}
							}

							// Ensure the equipped object is destroyed
							pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
						}
					}

					bStillHasPriorPosition  = true;
					m_vPriorBombPosition = pWeaponObject->GetTransform().GetPosition();
				}

				// Otherwise lets check to see if we should fire
				if( !bSimulatedAttach && pClipHelper->IsEvent( CClipEventTags::Fire ) )
				{
					Vec3V vDirection = Negate(pWeaponObject->GetTransform().GetC());
					Vec3V vStart = pWeaponObject->GetTransform().GetPosition();
					Vec3V vEnd = vStart + Scale( vDirection, ScalarV( 0.1f ) );

					Assertf(IsFiniteAll(vDirection) && IsFiniteAll(vStart) && IsFiniteAll(vEnd),	
						"Invalid bullet parameters initialised in CTaskBomb::StateSlideAndPlant_OnUpdate:"
						"\tDirection:	<%5.2f, %5.2f, %5.2f>"
						"\tStart:		<%5.2f, %5.2f, %5.2f>"
						"\tEnd:			<%5.2f, %5.2f, %5.2f>",
						VEC3V_ARGS(vDirection),VEC3V_ARGS(vStart),VEC3V_ARGS(vEnd));

					if (IsFiniteAll(vStart) && IsFiniteAll(vEnd))
					{
						if( pWeapon->Fire( CWeapon::sFireParams( pPed, MAT34V_TO_MATRIX34( pWeaponObject->GetMatrix()), &RCC_VECTOR3(vStart), &RCC_VECTOR3(vEnd) ) ) )
							pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
					}
				}
			}
			
			if( !pPed->IsNetworkClone() && pClipHelper->IsEvent<crPropertyAttributeBool>( CClipEventTags::Object, CClipEventTags::Create, true ) )
			{
				// When the reload anim event happens, create the projectile 	
				if( !pPed->GetInventory()->GetIsWeaponOutOfAmmo( pPed->GetWeaponManager()->GetEquippedWeaponInfo() ) )
				{					
					pPed->GetWeaponManager()->CreateEquippedWeaponObject();
				}				
			}
			else if( pClipHelper->IsEvent( CClipEventTags::Interruptible ) && CheckForPlayerMovement(*pPed) )
			{
				return FSM_Quit;
			}
		}
	}

	m_bHasPriorPosition = bStillHasPriorPosition;

	return FSM_Continue;
}

CTaskBomb::FSM_Return CTaskBomb::StateSlideAndPlant_OnExit( CPed* pPed )
{
	Assert(pPed);

	if(pPed->IsNetworkClone())
	{
		return FSM_Continue;
	}

	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if (pWeaponInfo && !pWeaponInfo->GetIsUnarmed()) //don't remove the unarmed weapon if it is the equipped weapon here (lavalley) - fixes url:bugstar:1920233 
	{
		s32 iAmmo = pPed->GetInventory()->GetWeaponAmmo( pWeaponInfo );
		if( iAmmo == 0 )
		{
			// Out of ammo - swap weapon
			pPed->GetInventory()->RemoveWeapon(pWeaponInfo->GetHash());
			pPed->GetWeaponManager()->EquipBestWeapon();
		}
	}

	return FSM_Continue;
}

CTaskBomb::FSM_Return CTaskBomb::StateSwap_OnEnter( CPed* UNUSED_PARAM( pPed ) )
{
	s32 iFlags = SWAP_HOLSTER | SWAP_DRAW | SWAP_TO_AIM;
	SetNewTask( rage_new CTaskSwapWeapon( iFlags ) );
	return FSM_Continue;
}

CTaskBomb::FSM_Return CTaskBomb::StateSwap_OnUpdate( CPed* UNUSED_PARAM( pPed ) )
{
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) )
	{
		// Set state to init to decide what to do next
		SetState( State_Init );
	}

	return FSM_Continue;
}

bool CTaskBomb::CheckForPlayerMovement(CPed& ped)
{
	if (ped.IsLocalPlayer())
	{
		CControl* pControl = ped.GetControlFromPlayer();

		if (pControl->GetPedJump().IsPressed())
		{
			return true;
		}

		Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), - pControl->GetPedWalkUpDown().GetNorm());
		float fInputMag = vecStick.Mag();

		if (fInputMag > 0.1f)
		{
			return true;
		}
	}
	return false;
}

static dev_float OFFSET_FROM_TARGET = 0.4367f;
void CTaskBomb::GetTargetPosition( Vector3& vMoveTarget ) const
{
	float fOffset = OFFSET_FROM_TARGET;

#if FPS_MODE_SUPPORTED
	// FPS anims are slightly different and require a larger offset value
	const CPed *pPed = GetPed();
	if (pPed && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		static dev_float fFPSOffset = 0.6f;
		fOffset = fFPSOffset;
	}
#endif	//FPS_MODE_SUPPORTED

	// Work out the point we want to move to
	vMoveTarget = m_vTargetPosition;
	vMoveTarget += m_vTargetNormal * fOffset;
}

float CTaskBomb::GetTargetHeading() const
{
	Vector3 vNormal( m_vTargetNormal.x, m_vTargetNormal.y, 0.0f );
	vNormal.NormalizeFast();
	return rage::Atan2f( vNormal.x, -vNormal.y );
}

//////////////////////////////////////////////////////////////////////////
// CClonedBombInfo
//////////////////////////////////////////////////////////////////////////

CClonedBombInfo::CClonedBombInfo( s32 state, const Vector3& targetPosition, const Vector3& targetNormal, bool bPlantInFront )
: m_targetPosition(targetPosition)
, m_bPlantInFront(bPlantInFront)
, m_targetNormal(Vector3(0.0f, 0.0f, 0.0f))
{     
	if(bPlantInFront)
	{
		m_targetNormal = targetNormal;
	}
	SetStatusFromMainTaskState( state ); 
}

CTaskFSMClone* CClonedBombInfo::CreateCloneFSMTask()
{
	return rage_new CTaskBomb();
}
