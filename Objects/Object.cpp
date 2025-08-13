//
// Title	:	Object.cpp
// Author	:	Richard Jobling
// Started	:	27/01/00
//
//
//
//
//
//
//
//
//
//
//
#include "object.h"

// Rage headers
#include "audioengine/engine.h"
#include "audioengine/widgets.h"
#include "bank/bkmgr.h"
#include "breakableglass/bgdrawable.h"
#include "creature/componentskeleton.h"
#include "fragment/cacheManager.h"
#include "fwanimation/directorcomponentexpressions.h"
#include "phbound/boundbvh.h"
#include "phbound/boundcomposite.h"
#include "phbound/boundgeom.h"
#include "pheffects/wind.h"
#include "phsolver/contactmgr.h"
#include "physics/constraints.h"
#include "physics/debugcontacts.h"
#include "physics/sleep.h"
#include "grmodel/geometry.h"

// Framework headers
#include "fwanimation/animdirector.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"
#include "fwmaths/Vector.h"
#include "fwscript/scriptguid.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/expressionsdictionarystore.h"

// Game headers
#include "animation/MoveObject.h"
#include "audio/ambience/ambientaudioentity.h"
#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "audio/frontendaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Cloth/Cloth.h"
#include "Cloth/ClothArchetype.h"
#include "Cloth/ClothRageInterface.h"
#include "Cloth/ClothMgr.h"
#include "Control/replay/Replay.h"
#include "Control/replay/Entity/ObjectPacket.h"
#include "debug/debugscene.h"
#include "Event/EventGroup.h"
#include "Event/EventSound.h"
#include "Frontend/MiniMap.h"
#include "Game/Clock.h"
#include "Game/modelindices.h"
#include "Game/Wind.h"
#include "game/weather.h"
#include "ModelInfo/PedModelInfo.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "objects/CoverTuning.h"
#include "objects/Door.h"
#include "objects/DoorTuning.h"
#include "objects/DummyObject.h"
#include "objects/ObjectIntelligence.h"
#include "objects/objectpopulation.h"
#include "Objects/ProceduralInfo.h"
#include "pathserver/ExportCollision.h"
#include "PathServer/PathServer.h"
#include "peds/playerinfo.h"
#include "Peds/Ped.h"
#include "performance/clearinghouse.h"
#include "Physics/GtaArchetype.h"
#include "Physics/GtaInst.h"
#include "Physics/GtaMaterial.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "physics/Tunable.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/Pickup.h"
#include "renderer/Entities/ObjectDrawHandler.h"
#include "Renderer/Water.h"
#include "scene/RefMacros.h"
#include "Scene/Scene.h"
#include "scene/entities/compEntity.h"
#include "scene/lod/LodMgr.h"
#include "script/commands_object.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "script/streamedscripts.h"
#include "shaders/CustomShaderEffectBase.h"
#include "shaders/CustomShaderEffectTint.h"
#include "streaming/streaming.h"		// CStreaming::SetMissionDoesntRequireObject()
#include "task/Animation/TaskAnims.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vehicles/Automobile.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxProjectile.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Weapons/Explosion.h"
#include "Weapons/projectiles/ProjectileRocket.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponTypes.h"
#include "glassPaneSyncing/GlassPaneManager.h"

#if __DEV
#include "Objects/ProcObjects.h" // For Flag for deletion error checks.
#endif

// network headers
#include "network/NetworkInterface.h"
#include "network/general/NetworkUtil.h"

SCENE_OPTIMISATIONS()
ENTITY_OPTIMISATIONS()

#define LARGEST_OBJECT_CLASS (sizeof(CProjectileRocket) > sizeof(CDoor) ? sizeof(CProjectileRocket) : sizeof(CDoor))
CompileTimeAssert(sizeof(CPickup)				<= LARGEST_OBJECT_CLASS);
CompileTimeAssert(sizeof(CProjectileRocket)		<= LARGEST_OBJECT_CLASS);
CompileTimeAssert(sizeof(CDoor)					<= LARGEST_OBJECT_CLASS);

INSTANTIATE_RTTI_CLASS(CObject,0x9CEC50B6);
FW_INSTANTIATE_BASECLASS_POOL(CObject, CONFIGURED_FROM_FILE, atHashString("Object",0x39958261), LARGEST_OBJECT_CLASS);

#define OBJECT_DEFAULT_HEALTH					(1000.0f)

extern const u32 g_boatCollisionHashName1;
extern const u32 g_boatCollisionHashName2;
extern const u32 g_boatCollisionHashName3;
extern const u32 g_boatCollisionHashName4;
extern const u32 g_boatCollisionHashName5;

u32 	CObject::ms_nNoTempObjects = 0;

atArray<CObject*> CObject::ms_ObjectsThatNeedProcessPostCamera(0, 32);

#if __BANK
dev_float SWAYABLE_WIND_SPEED_OVERRIDE	= 0.000f;
dev_float SWAYABLE_LO_SPEED			= 4.0f;
dev_float SWAYABLE_LO_AMPLITUDE		= 0.0f;
dev_float SWAYABLE_HI_SPEED			= 3.0f;
dev_float SWAYABLE_HI_AMPLITUDE		= 0.32f;
#endif

#define RAILLIGHTRANGE (12.0f)		// The area around the rail crossing light that has its nodes switched off.

#if !__FINAL
	bool CObject::bInObjectCreate = false;
	int CObject::inObjectDestroyCount = 0;
	bool CObject::bDeletingProcObject = false;
#endif

	u32 CObject::ms_uLastStealthBlipTime = 0;

	u32 CObject::ms_uStartTimerForCollisionAgitatedEvent = 0;

	dev_float CObject::ms_fMinMassForCollisionNoise = 50.0f;
	dev_float CObject::ms_fMinRelativeVelocitySquaredForCollisionNoise = 1.0f;
	dev_float CObject::ms_fMinMassForCollisionAgitatedEvent = 35.0f;
	dev_float CObject::ms_fMinRelativeVelocitySquaredForCollisionAgitatedEvent = 1.0f;

//CLockedObects		gLockedObjects;

u32 CObject::ms_globalDamagedScanCode = CObject::INVALID_DAMAGE_SCANCODE;
Vector3 CObject::ms_RollerCoasterVelocity = Vector3( 0.0f, 0.0f, 0.0f );
bool CObject::ms_bDisableCarCollisionsWithCarParachute = false;
bool CObject::ms_bAllowDamageEventsForNonNetworkedObjects = false;

#if __DEV

template<> void fwPool<CObject>::PoolFullCallback()
{
	CObject::DumpObjectPool();
}

#endif

#if !__FINAL

class ObjectInfo
{
public:
	enum
	{
		OBJECT_FLAG_WEAPON		= (1<<0),
		OBJECT_FLAG_NOT_WEAPON	= (1<<1)
	};

	static	const char *pFlagDesc[];

	ObjectInfo() : m_pFirstInstance(NULL), m_Count(0), m_Flags(0) {}
	CObject		*m_pFirstInstance;
	u32			m_Flags;
	int			m_Count;

	const char *GetWeaponString() const
	{
		return pFlagDesc[m_Flags];
	}
};

const char *ObjectInfo::pFlagDesc[] =
{
	"",			// Unused
	"(wep)",	// Weapon
	"",			// Not weapon (blank)
	"(both)"	// Both
};

static bool SortByInstanceCount(const ObjectInfo& a, const ObjectInfo& b) {return (a.m_Count > b.m_Count);}

// Moved into here and called from PoolFullCallback(), so I can add a RAG widget for testing
void CObject::DumpObjectPool()
{
	USE_DEBUG_MEMORY();

	int numScriptObjects = 0;
	int numNetworkObjects = 0;
	int numMapObjects = 0;

	atMap<u32, ObjectInfo> ObjLookupMap;

	CObject::Pool* pPool = CObject::GetPool();
	s32 size = (s32) pPool->GetSize();
	int iIndex = 0;
	while(size--)
	{
		CObject* pObject = pPool->GetSlot(iIndex);
		if(pObject)
		{
			// update some stats
			switch (pObject->GetOwnedBy())
			{
			case ENTITY_OWNEDBY_SCRIPT:
				numScriptObjects++;
				break;
			case ENTITY_OWNEDBY_RANDOM:
				numMapObjects++;
				break;
			default:
				break;
			}
			if (pObject->GetNetworkObject())
			{
				numNetworkObjects++;
			}

			// Create a hash of it's name.
			u32 key = atStringHash(CModelInfo::GetBaseModelInfoName(pObject->GetModelId()));

			ObjectInfo * const pInfo = ObjLookupMap.Access(key);
			if( !pInfo )
			{
				ObjectInfo info;
				info.m_pFirstInstance = pObject;
				info.m_Count = 1;
				info.m_Flags |= pObject->GetWeapon() ? ObjectInfo::OBJECT_FLAG_WEAPON : ObjectInfo::OBJECT_FLAG_NOT_WEAPON;
				ObjLookupMap.Insert(key, info);
			}
			else
			{
				pInfo->m_Flags |= pObject->GetWeapon() ? ObjectInfo::OBJECT_FLAG_WEAPON : ObjectInfo::OBJECT_FLAG_NOT_WEAPON;
				pInfo->m_Count++;
			}
		}
		iIndex++;
	}

	atArray<ObjectInfo> objectInfos;

	// Display results from the map
	for(int i=0;i<ObjLookupMap.GetNumSlots();i++)
	{
		rage::atMapEntry<rage::u32, ObjectInfo> *pEntry = ObjLookupMap.GetEntry(i);
		while (pEntry)
		{
			const ObjectInfo &info = pEntry->data;

			objectInfos.Grow() = info;

			pEntry = pEntry->next;
		}
	}

	// Sort by count
	std::sort( objectInfos.begin(), objectInfos.end(), SortByInstanceCount );

	int	instanceCount = 0;
	for(int i=0;i<objectInfos.size();i++)
	{
		const ObjectInfo &info = objectInfos[i];

		Displayf(" Object: %32.32s  %10s:- %4d instances.", info.m_pFirstInstance->GetBaseModelInfo()->GetModelName(), info.GetWeaponString(), info.m_Count);
		instanceCount += info.m_Count;
	}

	Displayf("Total:- %d Instances", instanceCount);

	Displayf("From %d CObjects, %d are owned by script and %d are map objects", pPool->GetNoOfUsedSpaces(), numScriptObjects, numMapObjects);
	Displayf("From %d CObjects, %d have network objects", pPool->GetNoOfUsedSpaces(), numNetworkObjects);
}
#endif	// !__FINAL

void CObject::DriveToJointToTarget()
{
	if( m_bDriveToMaxAngle ||
		m_bDriveToMinAngle )
	{
		if( GetFragInst() )
		{
			if( GetFragInst()->GetCacheEntry() )
			{
				GetFragInst()->GetCacheEntry()->LazyArticulatedInit();
				fragHierarchyInst* pHierInst = GetFragInst()->GetCacheEntry()->GetHierInst();

				if( pHierInst &&
					pHierInst->body )
				{
					int startIndex = m_jointToDriveIndex != (u8)-1 ? m_jointToDriveIndex : 0;
					int endIndex = m_jointToDriveIndex != (u8)-1 ? m_jointToDriveIndex + 1 : pHierInst->body->GetNumBodyParts() - 1;

					for( int jointIndex = startIndex; jointIndex < endIndex && jointIndex < pHierInst->body->GetNumBodyParts() - 1; jointIndex++ )
					{
						DriveJoint( pHierInst, jointIndex );
					}
				}
				else
				{
					DriveVelocity();
				}
			}
		}
	}
}

//void CObject::SnapJointToTargetRatio( int jointIndex, float fTargetOpenRatio )
//{
//	if( GetFragInst() )
//	{
//		if( GetFragInst()->GetCacheEntry() )
//		{
//			GetFragInst()->GetCacheEntry()->LazyArticulatedInit();
//			fragHierarchyInst* pHierInst = GetFragInst()->GetCacheEntry()->GetHierInst();
//			m_bDriveToMaxAngle = false;
//			m_bDriveToMinAngle = false;
//
//			if( pHierInst &&
//				pHierInst->body )
//			{
//				phJoint* pJoint = &pHierInst->body->GetJoint( jointIndex );
//
//				if( pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF )
//				{
//					phJoint1Dof* p1DofJoint = static_cast<phJoint1Dof*>( pJoint );
//
//					if( p1DofJoint )
//					{
//						float fMin, fMax;
//						p1DofJoint->GetAngleLimits( fMin, fMax );
//						if( fMin > fMax )
//						{
//							SwapEm( fMin, fMax );
//						}
//
//						float fTargetAngle = ( 1.0f - fTargetOpenRatio ) * ( fMax - fMin );
//
//						p1DofJoint->MatchChildToParentPositionAndVelocity( pHierInst->body, ScalarVFromF32( fTargetAngle ) );
//						pHierInst->articulatedCollider->UpdateCurrentMatrices();
//					}
//				}
//
//				if( pJoint && pJoint->GetJointType() == phJoint::PRISM_JNT )
//				{
//					phPrismaticJoint* pPrismatciJoint = static_cast<phPrismaticJoint*>( pJoint );
//
//					if( pPrismatciJoint )
//					{
//						pPrismatciJoint->MatchChildToParentPositionAndVelocity( pHierInst->body );
//						pHierInst->articulatedCollider->UpdateCurrentMatrices();
//					}
//				}
//
//				GetFragInst()->SyncSkeletonToArticulatedBody( true );
//			}
//		}
//	}
//}	

void CObject::DriveVelocity()
{
	if( fragInst* pFragInst = GetFragInst() )
	{
		if( pFragInst->IsInLevel() )
		{
			u32 includeFlags = ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES & ~ArchetypeFlags::GTA_ALL_MAP_TYPES;

			if( PHLEVEL->GetInstanceIncludeFlags( pFragInst->GetLevelIndex() ) != includeFlags )
			{
				pFragInst->GetArchetype()->SetIncludeFlags( includeFlags );

				// refresh the include flags in the level
				if( pFragInst->IsInLevel() )
				{
					PHLEVEL->SetInstanceIncludeFlags( pFragInst->GetLevelIndex(), includeFlags );
					PHSIM->GetContactMgr()->ResetWarmStartAllContactsWithInstance( pFragInst );
				}
			}
		}
	}

	if( m_bDriveToMaxAngle )
	{
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, ConstraintTorqueSpeedUp, 0.2f, 0.0f, 10000.0f, 1.0f );

		float fApplyTorque = ( ConstraintTorqueSpeedUp / fwTimer::GetTimeStep() );

		phCollider* pCollider = GetCollider();
		if( pCollider != NULL )
		{
			pCollider->ApplyTorque( fApplyTorque * GetAngInertia().z * VEC3V_TO_VECTOR3( GetTransform().GetC() ), ScalarV( fwTimer::GetTimeStep() ).GetIntrin128ConstRef() );
		}
	}
	else
	{
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, ConstraintTorqueSlowDownScale, 0.1f, 0.0f, 100000.0f, 1.0f );

		float fApplyTorque = -GetAngVelocity().GetZ();

		phCollider* pCollider = GetCollider();
		if( pCollider != NULL )
		{
			fApplyTorque *= ConstraintTorqueSlowDownScale;
			fApplyTorque /= fwTimer::GetTimeStep();
			pCollider->ApplyTorque( fApplyTorque * GetAngInertia().z * VEC3V_TO_VECTOR3( GetTransform().GetC() ), ScalarV( fwTimer::GetTimeStep() ).GetIntrin128ConstRef() );
		}
	}
}



void CObject::DriveJoint( fragHierarchyInst* pHierInst, int jointIndex )
{
	phJoint* pJoint = &pHierInst->body->GetJoint( jointIndex );

	if( pJoint && pJoint->GetJointType() == phJoint::JNT_1DOF )
	{
		phJoint1Dof* p1DofJoint = static_cast<phJoint1Dof*>( pJoint );

		float minAngle, maxAngle;
		p1DofJoint->GetAngleLimits( minAngle, maxAngle );
		if( minAngle > maxAngle )
		{
			SwapEm( minAngle, maxAngle );
		}

		bool justDriveSpeed = false;
		if( minAngle <= -PI * 1.9f &&
			maxAngle >= PI * 1.9f )
		{
			// assume this is set up to rotate all the way around
			justDriveSpeed = true;
			p1DofJoint->SetDriveState( phJoint::DRIVE_STATE_SPEED );
		}
		else
		{
			p1DofJoint->SetDriveState( phJoint::DRIVE_STATE_ANGLE_AND_SPEED );
		}

		float clampedDesiredAngle = minAngle;

		if( m_bDriveToMaxAngle )
		{
			clampedDesiredAngle = maxAngle;
		}

		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleAngleStrengthUpMin, 250.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleAngleStrengthUpMid, 250.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleAngleStrengthUpMax, 250.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleAngleStrengthDown, 50.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleSpeedStrengthUp, 30.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleSpeedStrengthDown, 20.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleSpeedUp, 0.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleSpeedDown, 0.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MuscleSpeedRotate, 10.0f, 0.0f, 10000.0f, 1.0f );

		p1DofJoint->SetMuscleTargetAngle( clampedDesiredAngle );

		if( m_bDriveToMaxAngle )
		{
			if( !justDriveSpeed )
			{
				static dev_float massForMaxStrength = 1000.0f;
				static dev_float massForMidStrength = 2000.0f;
				static dev_float massForMinStrength = 4000.0f;
				float massFactor = 0.0f;
				float muscleStrengthUp = 0.0f;

				if( GetMass() > massForMidStrength )
				{
					massFactor = ( GetMass() - massForMidStrength ) / ( massForMinStrength - massForMidStrength );
					massFactor = Clamp( massFactor, 0.0f, 1.0f );
					muscleStrengthUp = MuscleAngleStrengthUpMid - ( ( MuscleAngleStrengthUpMid - MuscleAngleStrengthUpMin ) * massFactor );
				}
				else
				{
					massFactor = ( GetMass() - massForMaxStrength ) / ( massForMidStrength - massForMaxStrength );
					massFactor = Clamp( massFactor, 0.0f, 1.0f );
					muscleStrengthUp = MuscleAngleStrengthUpMax - ( ( MuscleAngleStrengthUpMax - MuscleAngleStrengthUpMid ) * massFactor );
				}

				p1DofJoint->SetMuscleAngleStrength( muscleStrengthUp );
				p1DofJoint->SetMuscleSpeedStrength( MuscleSpeedStrengthUp );
				p1DofJoint->SetMuscleTargetSpeed( MuscleSpeedUp );
			}
			else
			{
				p1DofJoint->SetMuscleAngleStrength( 0.0f );
				p1DofJoint->SetMuscleSpeedStrength( MuscleSpeedStrengthUp );
				p1DofJoint->SetMuscleTargetSpeed( MuscleSpeedRotate );

				static Vector3 turnSpeed( 0.0f, 0.0f, 5.0f );
				SetAngVelocity( turnSpeed );
			}
		}
		else
		{
			if( !justDriveSpeed )
			{
				p1DofJoint->SetMuscleAngleStrength( MuscleAngleStrengthDown );
				p1DofJoint->SetMuscleSpeedStrength( MuscleSpeedStrengthDown );
				p1DofJoint->SetMuscleTargetSpeed( MuscleSpeedDown );
			}
			else
			{
				p1DofJoint->SetMuscleAngleStrength( 0.0f );
				p1DofJoint->SetMuscleSpeedStrength( MuscleSpeedStrengthDown );
				p1DofJoint->SetMuscleTargetSpeed( 0.0f );

				SetAngVelocity( Vector3( 0.0f, 0.0f, 0.0f ) );
			}
		}

		float currentAngle = p1DofJoint->GetAngle( pHierInst->body );
		static dev_float targetThreshold = 0.01f;
		if( Abs( clampedDesiredAngle - currentAngle ) < targetThreshold )
		{
			m_bDriveToMaxAngle = false;
			m_bDriveToMinAngle = false;
		}
	}

	if( pJoint && pJoint->GetJointType() == phJoint::PRISM_JNT )
	{
		phPrismaticJoint* pPrismaticJoint = static_cast<phPrismaticJoint*>( pJoint );

		float minAngle, maxAngle;
		pPrismaticJoint->GetAngleLimits( minAngle, maxAngle );
		if( minAngle > maxAngle )
		{
			SwapEm( minAngle, maxAngle );
		}

		float clampedDesiredAngle = minAngle;

		if( m_bDriveToMaxAngle )
		{
			clampedDesiredAngle = maxAngle;
		}

		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MusclePositionStrengthUp, 500.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MusclePositionStrengthDown, 50.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MusclePositionSpeedStrengthUp, 50.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MusclePositionSpeedStrengthDown, 20.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MusclePositionSpeedUp, 0.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, MusclePositionSpeedDown, 0.0f, 0.0f, 10000.0f, 1.0f );

		bool driveFast = m_bIsPressurePlate ? m_bDriveToMaxAngle :  ( m_bDriveToMaxAngle && maxAngle > 0.0f ) ||
																	( m_bDriveToMinAngle && maxAngle == 0.0f );

		if( driveFast )
		{
			pPrismaticJoint->SetMusclePositionStrength( MusclePositionStrengthUp );
			pPrismaticJoint->SetMuscleSpeedStrength( MusclePositionSpeedStrengthUp );
			pPrismaticJoint->SetMuscleTargetSpeed( MusclePositionSpeedUp );
		}
		else
		{
			pPrismaticJoint->SetMusclePositionStrength( MusclePositionStrengthDown );
			pPrismaticJoint->SetMuscleSpeedStrength( MusclePositionSpeedStrengthDown );
			pPrismaticJoint->SetMuscleTargetSpeed( MusclePositionSpeedDown );
		}

		pPrismaticJoint->SetMuscleTargetPosition( clampedDesiredAngle );
		pPrismaticJoint->SetDriveState( phJoint::DRIVE_STATE_ANGLE_AND_SPEED );
		pPrismaticJoint->SetMuscleTargetSpeed( 0.0f );

		float currentTranslation = pPrismaticJoint->ComputeCurrentTranslation( pHierInst->body );
		static dev_float targetTranslationThresholdFactor = 0.01f;
		static dev_float targetTranslationThresholdMax = 0.001f;
		float translationDiff = Abs( clampedDesiredAngle - currentTranslation );

		if( translationDiff < targetTranslationThresholdMax || 
			translationDiff < ( targetTranslationThresholdFactor * ( maxAngle - minAngle ) ) ||
			( IsPressurePlatePressed() && m_bDriveToMinAngle ) )
		{
			m_bDriveToMaxAngle = m_bDriveToMinAngle && m_bIsPressurePlate;
			m_bDriveToMinAngle = false;
		}
	}
}

void CObject::CreateConstraintForSetDriveToTarget()
{
	if( fragInst* pFragInst = GetFragInst() )
	{
		if( !pFragInst->GetBroken() )
		{
			pFragInst->GetArchetype()->SetIncludeFlags( ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES & ~ArchetypeFlags::GTA_ALL_MAP_TYPES );

			// refresh the include flags in the level
			if( pFragInst->IsInLevel() )
			{
				PHLEVEL->SetInstanceIncludeFlags( pFragInst->GetLevelIndex(), pFragInst->GetArchetype()->GetIncludeFlags() );
				PHSIM->GetContactMgr()->ResetWarmStartAllContactsWithInstance( pFragInst );
			}

			pFragInst->SetBroken( true );

			phConstraintHinge::Params constraint;
			constraint.instanceA = GetCurrentPhysicsInst();
			constraint.worldAnchor = GetTransform().GetPosition();
			constraint.worldAxis = GetTransform().GetUp();

			PHCONSTRAINT->Insert( constraint );

			phCollider* pCollider = GetCollider();
			if( pCollider != NULL )
			{
				TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, ConstraintMaxAngularSpeed, 20.0f, 0.0f, 10000.0f, 1.0f );
				pCollider->SetMaxAngSpeed( ConstraintMaxAngularSpeed );
			}
		}
	}
}

void CObject::SetDriveToTarget( bool maxAngle, int jointIndex )
{
    u8 prevJointToDriveIndex = m_jointToDriveIndex;
    bool prevDriveToMinAngle = m_bDriveToMinAngle;
    bool prevDriveToMaxAngle = m_bDriveToMaxAngle;

	m_jointToDriveIndex = (u8)jointIndex;

	if( GetFragInst() )
	{
		ActivatePhysics();
		
		if( !GetCollider() )
		{
			CreateConstraintForSetDriveToTarget();		
		}
	}

	if( maxAngle )
	{
        m_bDriveToMinAngle = false;
        m_bDriveToMaxAngle = jointIndex == -1 || !IsJointAtMaxAngle(jointIndex);
	}
	else
	{
		m_bDriveToMaxAngle = false;
        m_bDriveToMinAngle = jointIndex == -1 || !IsJointAtMinAngle(jointIndex) || IsPressurePlatePressed();
	}

    if (NetworkInterface::IsGameInProgress() 
        && GetNetworkObject()
        && GetNetworkObject()->IsLocallyRunningScriptObject()
        && (m_jointToDriveIndex != prevJointToDriveIndex
        || m_bDriveToMinAngle != prevDriveToMinAngle
        || m_bDriveToMaxAngle != prevDriveToMaxAngle))
    {
        gnetDebug3("CObject::SetDriveToTarget force send of ScriptGameStateData --> object: %s, joint index: %u, bDriveToMinAngle: %s, bDriveToMaxAngle: %s", GetLogName(), (u32)m_jointToDriveIndex, m_bDriveToMinAngle ? "true" : "false", m_bDriveToMaxAngle ? "true" : "false");
        CNetObjObject* pNetObj = static_cast<CNetObjObject*>(GetNetworkObject());
        pNetObj->ForceSendOfScriptGameStateData();
    }
}

bool CObject::IsPressurePlatePressed()
{
	if( m_bIsPressurePlate &&
		GetFragInst() &&
		GetFragInst()->GetCacheEntry() )
	{
		if( fragHierarchyInst* pHierInst = GetFragInst()->GetCacheEntry()->GetHierInst() )
		{
			if( pHierInst->body )
			{
				phJoint* pJoint = &pHierInst->body->GetJoint( 0 );

				if( pJoint && pJoint->GetJointType() == phJoint::PRISM_JNT )
				{
					phPrismaticJoint* pPrismaticJoint = static_cast<phPrismaticJoint*>( pJoint );

					float minAngle, maxAngle;
					pPrismaticJoint->GetAngleLimits( minAngle, maxAngle );
					float pressedThreshold = ( maxAngle - minAngle ) * 0.25f;
					float currentTranslation = pPrismaticJoint->ComputeCurrentTranslation( pHierInst->body );

					if( Abs( minAngle - currentTranslation ) < pressedThreshold ||
						currentTranslation < minAngle )
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool CObject::IsJointAtMinAngle( int jointIndex )
{
    return IsJointAtLimitAngle(jointIndex, true);
}

bool CObject::IsJointAtMaxAngle(int jointIndex)
{
    return IsJointAtLimitAngle(jointIndex, false);
}

bool CObject::IsJointAtLimitAngle(int jointIndex, bool bMinAngle)
{
	if (bMinAngle)
    {
        if (m_bDriveToMaxAngle)
        {
            return false;
        }
    }
    else
    {
        if (m_bDriveToMinAngle)
        {
            return false;
        }
    }

	if( jointIndex == -1 )
	{
		return false;
	}

	bool result = bMinAngle;

    if (GetFragInst() &&
        GetFragInst()->GetCacheEntry())
    {
        if (fragHierarchyInst* pHierInst = GetFragInst()->GetCacheEntry()->GetHierInst())
        {
            if (pHierInst->body)
            {
				if( jointIndex < pHierInst->body->GetNumBodyParts() - 1 )
				{
					phJoint* pJoint = &pHierInst->body->GetJoint( jointIndex );
					if( pJoint )
					{
						if( pJoint->GetJointType() == phJoint::JNT_1DOF )
						{
							phJoint1Dof* p1DofJoint = static_cast<phJoint1Dof*>( pJoint );

							float minAngle, maxAngle;
							p1DofJoint->GetAngleLimits( minAngle, maxAngle );
							if( minAngle > maxAngle )
							{
								SwapEm( minAngle, maxAngle );
							}

							float clampedDesiredAngle = minAngle;
							if( !bMinAngle )
							{
								clampedDesiredAngle = maxAngle;
							}

							float currentAngle = p1DofJoint->GetAngle( pHierInst->body );
							float targetThreshold = 0.1f * Min( 1.0f, fwTimer::GetTimeStep() * 30.0f );
							if( Abs( clampedDesiredAngle - currentAngle ) < targetThreshold )
							{
								return true;
							}
							else
							{
								return false;
							}
						}
						else if( pJoint->GetJointType() == phJoint::PRISM_JNT )
						{
							phPrismaticJoint* pPrismaticJoint = static_cast<phPrismaticJoint*>( pJoint );

							float minAngle, maxAngle;
							pPrismaticJoint->GetAngleLimits( minAngle, maxAngle );
							if( minAngle > maxAngle )
							{
								SwapEm( minAngle, maxAngle );
							}

							float clampedDesiredAngle = minAngle;
							if( !bMinAngle )
							{
								clampedDesiredAngle = maxAngle;
							}
							float currentTranslation = pPrismaticJoint->ComputeCurrentTranslation( pHierInst->body );
							float targetTranslationThresholdFactor = 0.1f * Max( 1.0f, fwTimer::GetTimeStep() * 30.0f );
							dev_float targetTranslationThresholdMax = 0.01f * Max( 1.0f, fwTimer::GetTimeStep() * 30.0f );
							float translationDiff = Abs( clampedDesiredAngle - currentTranslation );

							if( translationDiff < targetTranslationThresholdMax || translationDiff < ( targetTranslationThresholdFactor * ( maxAngle - minAngle ) ) )
							{
								gnetDebug3( "CObject::IsJointAtLimitAngle --> object: %s, joint index: %u, limit: %s, bDriveToMinAngle: %s, bDriveToMaxAngle: %s, IsPressurePlatePressed: %s", GetLogName(), (u32)m_jointToDriveIndex, bMinAngle ? "MinAngle" : "MaxAngle", m_bDriveToMinAngle ? "true" : "false", m_bDriveToMaxAngle ? "true" : "false", IsPressurePlatePressed() ? "true" : "false" );
								return true;
							}
							else
							{
								return false;
							}
						}
					}
				}
            }
        }
    }
    return result;
}

void CObject::UpdateDriveToTargetFromNetwork(u8 jointToDriveIdx, bool bDriveToMinAngle, bool bDriveToMaxAngle)
{
	m_jointToDriveIndex = jointToDriveIdx;
	m_bDriveToMinAngle = bDriveToMinAngle;
	m_bDriveToMaxAngle = bDriveToMaxAngle;
	
	if((m_bDriveToMinAngle || m_bDriveToMaxAngle) && !GetCollider())
	{
		CreateConstraintForSetDriveToTarget();
	}	
}

void SetArticulatedJointTargetAngle( CObject* pObject, bool useMaxAngle, int jointIndex )
{
	pObject->SetDriveToTarget( useMaxAngle, jointIndex );
}

	
//
//
//
//
CObject::CObject(const eEntityOwnedBy ownedBy)
	: CPhysical( ownedBy ), m_damageScanCode(INVALID_DAMAGE_SCANCODE)
{
#if !__FINAL
	Assert(bInObjectCreate);
#endif

	Init();

	//m_fUprootLimit = 0.0f;
}


//
//
//
//
CObject::CObject(const eEntityOwnedBy ownedBy, u32 nModelIndex, bool bCreateDrawable, bool bInitPhys, bool bCalcAmbientScales)
	: CPhysical( ownedBy ), m_damageScanCode(INVALID_DAMAGE_SCANCODE)
{
#if !__FINAL
	Assert(bInObjectCreate);
#endif

	Init();

	SetModelId(fwModelId(strLocalIndex(nModelIndex)));

	if (bInitPhys)
	{
		InitPhys();
	}

	SetCalculateAmbientScaleFlag(bCalcAmbientScales);
	if(bCreateDrawable)
		CreateDrawable();

	TellAudioAboutAudioEffectsAdd();			// Tells the audio an entity has been created that may have audio effects on it.

}


//
//
//
//
CObject::CObject(const eEntityOwnedBy ownedBy, CDummyObject* pDummy)
	: CPhysical( ownedBy ), m_damageScanCode(INVALID_DAMAGE_SCANCODE)
{
#if !__FINAL
	Assert(bInObjectCreate);
#endif

	Init();
	SetMatrix(MAT34V_TO_MATRIX34(pDummy->GetMatrix()));
	SetModelId(pDummy->GetModelId());

	InitPhys();

	SetTintIndex(pDummy->GetTintIndex());	// propagate tint index BEFORE creating drawable and its CSE
	if(pDummy)
	{
		SetRelatedDummy(pDummy, pDummy->GetInteriorLocation());
	}

	pDummy->DeleteDrawable();

	SetIplIndex(pDummy->GetIplIndex());

	m_nFlags.bRenderDamaged = pDummy->m_nFlags.bRenderDamaged;

	TellAudioAboutAudioEffectsAdd();			// Tells the audio an entity has been created that may have audio effects on it.
}



//
//
//
//
CObject::~CObject()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

#if !__FINAL
	Assert(inObjectDestroyCount>0 || bDeletingProcObject);
	Assertf(!m_pRelatedDummy, "Deleting random object without placing related dummy back in the world. This will cause a dummy leak. Retry to dump object.");
	if (m_pRelatedDummy){
		Displayf("---\nDummy:\n");
		CDebug::DumpEntity(m_pRelatedDummy);
		Displayf("Object:\n---\n");
		CDebug::DumpEntity(this);
	}
#endif

	if (m_nObjectFlags.ScriptBrainStatus != OBJECT_DOESNT_USE_SCRIPT_BRAIN)
	{
		strLocalIndex id = g_StreamedScripts.FindSlot(CTheScripts::GetScriptsForBrains().GetScriptFileName(StreamedScriptBrainToLoad));
		if (id.Get() >= 0)		// This may happen if the script hasn't registered a script brain (like the network script)
		{
			CStreaming::SetMissionDoesntRequireObject(id, g_StreamedScripts.GetStreamingModuleId());
		}

		m_nObjectFlags.ScriptBrainStatus = OBJECT_DOESNT_USE_SCRIPT_BRAIN;
		CTheScripts::GetScriptBrainDispatcher().RemoveWaitingObject(this, StreamedScriptBrainToLoad);
		StreamedScriptBrainToLoad = -1;
	}

	
	if (m_nObjectFlags.bIsPickUp) // pickups have their own pool
	{
		RemoveBlip(BLIP_TYPE_PICKUP_OBJECT);
	}
	else
	{
		RemoveBlip(BLIP_TYPE_OBJECT);
	}

	if(GetOwnedBy()==ENTITY_OWNEDBY_TEMP && ms_nNoTempObjects>0)
		ms_nNoTempObjects--;

	ClearRelatedDummy();
	ClearFragParent();

	delete m_pIntelligence;
	m_pIntelligence = NULL;

	if (m_pWeaponComponent)
	{
		if (m_pWeaponComponent->GetComponentLodState() == CWeaponComponent::WCLS_HD_AVAILABLE)
		{
			m_pWeaponComponent->Update_HD_Models(false);
		}

		if (m_pWeaponComponent->GetComponentLodState() == CWeaponComponent::WCLS_HD_REMOVING)
		{
			m_pWeaponComponent->Update_HD_Models(false);
		}
	}
	if(m_pWeapon)
	{
		delete m_pWeapon;
		m_pWeapon = NULL;
	}

	TellAudioAboutAudioEffectsRemove();			// Tells the audio an entity has been deleted that may have audio effects on it.

	if (m_nObjectFlags.bDetachedPedProp)
	{
		CBaseModelInfo* bmi = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(uPropData.m_parentModelIdx)));
		if (bmi && bmi->GetModelType() == MI_TYPE_PED)
		{
			CPedModelInfo* pmi = (CPedModelInfo*)bmi;
			strLocalIndex	DwdIdx = strLocalIndex(pmi->GetPropsFileIndex());
			if (DwdIdx != -1) 
			{
				// pack ped
				g_DwdStore.RemoveRef(DwdIdx, REF_OTHER);
				g_TxdStore.RemoveRef(strLocalIndex(g_DwdStore.GetParentTxdForSlot(DwdIdx)), REF_OTHER);
			}
			else 
			{
				// streamed ped
				strLocalIndex	dwdIdx = strLocalIndex(g_DwdStore.FindSlot(uPropData.m_propIdxHash));
				if (dwdIdx != -1)
					g_DwdStore.RemoveRef(dwdIdx, REF_OTHER);

				strLocalIndex txdIdx = strLocalIndex(g_TxdStore.FindSlot(uPropData.m_texIdxHash));
				if (txdIdx != -1)
					g_TxdStore.RemoveRef(txdIdx, REF_OTHER);
			}
		}
	}

	int processPostCameraIndex = ms_ObjectsThatNeedProcessPostCamera.Find(this);
	if(processPostCameraIndex != -1)
	{
		ms_ObjectsThatNeedProcessPostCamera.DeleteFast(processPostCameraIndex);
		Assert(ms_ObjectsThatNeedProcessPostCamera.Find(this) == -1);
	}

	if (GetNetworkObject())
	{
		Assertf(0, "Destroying object which still has a network object! (%s)", GetNetworkObject()->GetLogName());
		GetNetworkObject()->SetGameObject(NULL);
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() == false)
	{
		// Object is being deleted during normal gameplay so
		// let the replay know
		CReplayMgr::StopRecordingMapObject(this);
		CReplayMgr::StopRecordingObject(this);
	}
#endif // GTA_REPLAY
}

#if !__NO_OUTPUT
void CObject::PrintSkeletonData()
{
	size_t bytes = 0;
	Displayf("Name, Num_Bones, Bytes");

	for (s32 i = 0; i < CObject::GetPool()->GetSize(); ++i)
	{
		CObject* pEntity = CObject::GetPool()->GetSlot(i);
		if (pEntity)
		{
			pEntity->PrintSkeletonSummary();
			bytes += pEntity->GetSkeletonSize();
		}
	}
	Displayf("Total CObject Skeletons: %" SIZETFMT "d KB\n", bytes >> 10);
}
#endif

//
//
//
//
void CObject::SetModelId(fwModelId modelId)
{
	CEntity::SetModelId(modelId);

	// check if this object has a buoyancy 2d effect attached
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
	if(pModelInfo)
	{
		GET_2D_EFFECTS_NOLIGHTS(pModelInfo);
		for(int i=0; i<iNumEffects && !m_nObjectFlags.bFloater; i++)
		{
			if((*pa2dEffects)[i]->GetType()==ET_BUOYANCY)
			{
				m_nObjectFlags.bFloater = true;
				// get the height the object is supposed to float at
				Vector3 vecEffectPos;
				(*pa2dEffects)[i]->GetPos(vecEffectPos);
				uMiscData.m_fFloaterHeight = vecEffectPos.z;
			}
		}
	}

}

//
//
//
// check if object's drawable contains edgeSkinned geometry:
//
static
bool DrawableHasEdgeSkinnedGeometry(rmcDrawable* PPU_ONLY(pDrawable))
{
#if __PPU
	Assert(pDrawable);
	const rmcLod* pLod = &(pDrawable->GetLodGroup().GetLod(LOD_HIGH));
	if(pLod)
	{
		const s32 modelCount = pLod->GetCount();
		for (s32 j=0; j<modelCount; j++)
		{
			grmModel* pModel = pLod->GetModel(j);
			if(pModel)
			{
				const s32 geomCount = pModel->GetGeometryCount();
				for(s32 i=0; i<geomCount; i++)
				{
					grmGeometry *geom = &pModel->GetGeometry(i);
					if(geom->GetType() == grmGeometry::GEOMETRYEDGE)
					{
						grmGeometryEdge *geomEdge = static_cast<grmGeometryEdge*>(geom);
						if(geomEdge->IsSkinned())
							return(TRUE);
					}
				}
			}//if(pModel)...
		}// for (s32 j=0; j<modelCount; j++)..
	}//if(pLod)...
#endif //__PPU...

	return(FALSE);
}// end of DrawableHasEdgeSkinnedGeometry()...

fwDrawData* CObject::AllocateDrawHandler(rmcDrawable* pDrawable)
{
	if (m_nFlags.bIsFrag)
	{
		return rage_new CObjectFragmentDrawHandler(this, pDrawable);
	}
	else
	{
		return rage_new CObjectDrawHandler(this, pDrawable);
	}
}

void CObject::AttachCustomBoundsFromExtension()
{
// NOTE: entity extensions are in the dummy object !!!
	if( !m_pRelatedDummy )
		return;

	fwClothCollisionsExtension*	pClothCollisionsExt = ((fwEntity*)m_pRelatedDummy)->GetExtension<fwClothCollisionsExtension>();
	if( pClothCollisionsExt )
	{
		const phVerletClothCustomBounds* refCustomBounds = (const phVerletClothCustomBounds*)&pClothCollisionsExt->m_ClothCollisions;
		Assert( refCustomBounds );
		int numBounds = refCustomBounds->m_CollisionData.GetCount();

		fragInst* pFragInst = GetFragInst();
		Assert( pFragInst );
		const fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry();
		Assert( pCacheEntry );
		Assert( pCacheEntry->GetHierInst() );

		environmentCloth* pEnvCloth = pCacheEntry->GetHierInst()->envCloth;
		Assert( pEnvCloth );

		for( int i = 0; i < LOD_COUNT; ++i )
		{
			phVerletCloth* pCloth = pEnvCloth->GetCloth(i);
			if( pCloth )
			{
// TODO: allow adding extra bounds
				Assert( !pCloth->m_CustomBound );
				phBoundComposite* pCustomBound = rage_aligned_new(16) phBoundComposite;	
				Assert( pCustomBound );
				pCustomBound->Init( numBounds, true );
				pCustomBound->SetNumBounds( numBounds );	

				for( int i = 0; i < numBounds; ++i )
				{
					const phCapsuleBoundDef& capsuleData = refCustomBounds->m_CollisionData[ i ];
					clothManager::LoadCustomBound( pCustomBound, capsuleData, i );		
				}
			
				pCloth->m_CustomBound = pCustomBound;
				pCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, true);
			}
		}

//		bool res = clothManager::AttachCustomBound( pCloth, pClothControllerName, oCustomBounds );
//		if( res && bSetFlag )
//		{
//			pEnvCloth->SetFlag( environmentCloth::CLOTH_HasLocalBounds, true );
//		}
	}
}
//
//
//
//
bool CObject::CreateDrawable()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && CReplayMgr::ShouldHideMapObject(this))
	{
		return false;
	}
#endif // GTA_REPLAY

#if !PHYSICS_REQUEST_LIST_ENABLED

	if (!IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS) && !GetCurrentPhysicsInst() )
	{
		if ( InitPhys() == CPhysical::INIT_OK )
		{
			AddPhysics();

			if(m_nObjectFlags.bIsRageEnvCloth)
			{
				AddToSceneUpdate();

				AttachCustomBoundsFromExtension();
			}
		}
	}
#endif	//!PHYSICS_REQUEST_LIST_ENABLED

	bool rt = CPhysical::CreateDrawable();

	bool bCreateSkeleton = FALSE;

	//If the object has cloth then set the rain collision map flag
	if(m_nObjectFlags.bIsRageEnvCloth)
	{
		GetRenderPhaseVisibilityMask().SetFlag(VIS_PHASE_MASK_RAIN_COLLISION_MAP);
	}
	else
	{
		GetRenderPhaseVisibilityMask().ClearFlag(VIS_PHASE_MASK_RAIN_COLLISION_MAP);
	}

	if(rt && !m_nFlags.bIsFrag && GetDrawable()->GetSkeletonData())
	{
		const s32 numBones = GetDrawable()->GetSkeletonData()->GetNumBones();
		if(numBones > 1)
		{	// old path: accept all skinned objects with more than 1 bone:
			bCreateSkeleton = TRUE;
		}
		else if(numBones == 1)
		{
			if(DrawableHasEdgeSkinnedGeometry(this->GetDrawable()))
			{
				bCreateSkeleton = TRUE;
			}
		}
	}

	if(rt && bCreateSkeleton)
	{
		CreateSkeleton();

		// reset root matrix 
		if(GetSkeleton())
		{
			GetSkeleton()->GetLocalMtx(0) = GetSkeleton()->GetSkeletonData().GetDefaultTransform(0);
			UpdateSkeleton();
		}

#if NORTH_CLOTHS

		//If the object is a bendable plant then add it as a bendable plant.
		CBaseModelInfo* pModelInfo = GetBaseModelInfo();
		Assert(pModelInfo);
		if(pModelInfo->GetAttributes()==MODEL_ATTRIBUTE_IS_BENDABLE_PLANT)
		{
			//Yup, we've got an entity that ought to be a plant.
			//Let's see if there is a cloth archetype that we can use.
			const CClothArchetype* pClothArchetype = pModelInfo->GetExtension<CClothArchetype>();

			Assertf(pClothArchetype, "No cloth archetype");
			if(pClothArchetype)
			{
				//Should have an archetype now.
				//Make a cloth that we'll use to pose the skeleton.
				//Pass in the current matrix so the cloth is instantiated at the correct
				//coordinates.
				m_pCloth=CClothMgr::AddCloth(pClothArchetype,MAT34V_TO_MATRIX34(GetMatrix()));
				Assert(m_pCloth);

				//Set the userdata on the instance.
				m_pCloth->GetRageBehaviour()->GetInstance()->SetUserData(this);
				m_pCloth->GetRageBehaviour()->GetInstance()->SetInstFlag(phInstGta::FLAG_USERDATA_ENTITY, true);
			}

			// check drawable configuration: LOD0 must be skinned, LOD1 must be non-skinned:
#if __DEV
			rmcLodGroup &lodGroup = this->m_pDrawHandler->GetDrawable()->GetLodGroup();
			bool bObjectOK = TRUE;

			if(lodGroup.ContainsLod(LOD_HIGH) && lodGroup.ContainsLod(LOD_MED))
			{
				// is LOD0 skinned?
				if(!lodGroup.GetLodModel0(LOD_HIGH).GetSkinFlag())
					bObjectOK = FALSE;

				if(!lodGroup.GetLodModel0(LOD_HIGH).IsModelRelative())
					bObjectOK = FALSE;

				// is LOD1 non-skinned?
				if(lodGroup.GetLodModel0(LOD_MED).GetSkinFlag())
					bObjectOK = FALSE;

				if(lodGroup.GetLodModel0(LOD_MED).IsModelRelative())
					bObjectOK = FALSE;
			}
			else
			{
				bObjectOK = FALSE;
			}

			Assertf(bObjectOK, "Bendable object %s has incorrect LOD setup. LOD0 should be skinned and LOD1 non-skinned!", CModelInfo::GetBaseModelInfoName(GetModelIndex()));
#endif //__DEV...
		}
#endif //NORTH_CLOTHS...

	}


	if(rt && m_nFlags.bIsProcObject)
	{
		// handle tinting on procedural objects:
		fwCustomShaderEffect *pCSE = GetDrawHandler().GetShaderEffect();
		if(pCSE)
		{
			if(pCSE->GetType() == CSE_TINT)
			{
				CCustomShaderEffectTint *pCSETint = (CCustomShaderEffectTint*)pCSE;

				u32 pal = 0;

				if(this->m_procObjInfoIdx != 0xffff)
				{
					const CProcObjInfo& procObjInfo = g_procInfo.GetProcObjInfo(this->m_procObjInfoIdx);

					const u32 storedSeed = g_DrawRand.GetSeed();
					g_DrawRand.Reset( u32(size_t(pCSETint) | size_t(pCSETint)>>16) );

					if (procObjInfo.m_MinTintPalette == procObjInfo.m_MaxTintPalette)
					{
						pal = procObjInfo.m_MinTintPalette;
					}
					else if(procObjInfo.m_MinTintPalette==255 || procObjInfo.m_MaxTintPalette==255)
					{	// random palette - whole range:
						pal = g_DrawRand.GetRanged( (s32)0, (s32)pCSETint->GetMaxTintPalette() );
					}
					else
					{	// random palette - selected range:
						pal = g_DrawRand.GetRanged( (s32)procObjInfo.m_MinTintPalette, (s32)procObjInfo.m_MaxTintPalette );
					}

					g_DrawRand.Reset(storedSeed);
				}

				pCSETint->SelectTintPalette((u8)pal, this);

			}// CSE_TINT...
		}// if(pCSE)...
	}// if(rt && m_nFlags.bIsProcObject)...

	// auto-start animation if required
	if(rt)
	{
		PlayAutoStartAnim();
	}

	return rt;
}

void CObject::DeleteDrawable()
{
#if NORTH_CLOTHS
	DeleteCloth();
#endif
	CPhysical::DeleteDrawable();

	if (!GetIsOnSceneUpdate())
	{
		RemoveFromSceneUpdate();
	}
}

void CObject::PlayAutoStartAnim()
{
	CBaseModelInfo *pBaseModelInfo = GetBaseModelInfo();
	if (pBaseModelInfo && pBaseModelInfo->GetAutoStartAnim() && !m_nFlags.bRenderDamaged)
	{
		strLocalIndex clipDictionaryIndex = strLocalIndex(pBaseModelInfo->GetClipDictionaryIndex());
		if (clipDictionaryIndex.Get() != -1)
		{
			const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex.Get(), pBaseModelInfo->GetModelNameHash());
			if (Verifyf(pClip, "Couldn't get clip! '%s' '%s'", g_ClipDictionaryStore.GetName(clipDictionaryIndex), pBaseModelInfo->GetModelName()))
			{
				int flags = APF_ISLOOPED;

				const crProperty *pProperty = pClip->GetProperties()->FindProperty("AnimPlayerFlags");
				if (pProperty)
				{
					const crPropertyAttribute *pPropertyAttribute = pProperty->GetAttribute("AnimPlayerFlags");
					if (pPropertyAttribute && pPropertyAttribute->GetType() == crPropertyAttribute::kTypeInt)
					{
						const crPropertyAttributeInt *pPropertyAttributeInt = static_cast<const crPropertyAttributeInt *>(pPropertyAttribute);

						flags = pPropertyAttributeInt->GetInt();
					}
				}

				///
				if (fragInst* pInst = GetFragInst())
				{
					if (pInst && !pInst->GetCached())
					{
						pInst->PutIntoCache();
					}
				}
				///

				if (!GetSkeleton())
				{
					CreateSkeleton();
				}
				if (!GetAnimDirector())
				{
					CreateAnimDirector(*GetDrawable());
					AddToSceneUpdate();
				}

				// Object has to be on the process control list for the animation system to be updated
				if (!GetIsOnSceneUpdate())
				{
					AddToSceneUpdate();
				}

				if (Verifyf(GetDrawable(), "Couldn't get drawable!"))
				{
					if (Verifyf(GetDrawable()->GetSkeletonData(), "Couldn't get skeleton!"))
					{
						const CClipEventTags::CRandomizeRateProperty *pRateProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeRateProperty >(pClip, CClipEventTags::RandomizeRate);
						const CClipEventTags::CRandomizeStartPhaseProperty *pPhaseProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeStartPhaseProperty >(pClip, CClipEventTags::RandomizeStartPhase);

						u32 randomSeed = 0;
						if (pRateProp || pPhaseProp)
						{
							CDummyObject *pDummyObject = GetRelatedDummy();
							if (pDummyObject)
							{
								CEntity *pRootLod = (CEntity *)pDummyObject->GetRootLod();
								if (pRootLod)
								{
									randomSeed = pRootLod->GetLodRandomSeed(pRootLod);
								}
							}
						}

						float rate = 1.0f;
						if (pRateProp)
						{
							rate = pRateProp->PickRandomRate(randomSeed);
						}

						float phase = 0.0f;
						if (pPhaseProp)
						{
							phase = pPhaseProp->PickRandomStartPhase(randomSeed);
						}

						float time = pClip->ConvertPhaseToTime(phase);
						u32 basetime = NetworkInterface::IsNetworkOpen() ? (u32)NetworkInterface::GetSyncedTimeInMilliseconds() & 0x000FFFFF : fwTimer::GetTimeInMilliseconds(); //use the synchronized network time in MP to keep the animation movement aligned - only use lower bits of network time as it is too large
						time += ((static_cast<float>(basetime) / 1000.0f) * rate);
						phase = pClip->ConvertTimeToPhase(time);
						phase = phase - Floorf(phase);
						CGenericClipHelper& clipHelper = GetMoveObject().GetClipHelper();


						clipHelper.BlendInClipByDictAndHash(this, clipDictionaryIndex.Get(), pBaseModelInfo->GetModelNameHash(), INSTANT_BLEND_IN_DELTA, INSTANT_BLEND_OUT_DELTA, true, false);
						clipHelper.SetPhase(phase);
						clipHelper.SetRate(rate);

						if (fragInst* pInst = GetFragInst())
						{
							// For large inactive objects that animate we need to make them collide against other inactive objects and use
							//   external velocity
							if (pInst->GetTypePhysics()->GetMinMoveForce() < 0.0f && AssertVerify(pInst->IsInLevel()))
							{
								CPhysics::GetLevel()->SetInactiveCollidesAgainstInactive(pInst->GetLevelIndex(), true);
								pInst->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL, true);
							}
						}
					}
				}
			}
		}
	}
}

int CObject::InitPhys()
{
#if GTA_REPLAY
	if(CReplayMgr::DisallowObjectPhysics(this))
	{
		return INIT_CANCELLED;
	}
#endif // GTA_REPLAY

#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
	{
		CBaseModelInfo * pModelInfo = GetBaseModelInfo();
		if(pModelInfo && pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumEnvCloths())
			return INIT_OK;
	}
#endif	

	if(IsArchetypeSet() && GetCurrentPhysicsInst()==NULL)
	{
		// Create the physics instance for this object
		CBaseModelInfo* pModelInfo = GetBaseModelInfo();
		if (!pModelInfo->GetHasBoundInDrawable()  && pModelInfo->GetDrawableType()!=CBaseModelInfo::DT_FRAGMENT)
		{
			// the object has no physics
			return INIT_OK;
		}

		if (pModelInfo->GetIsFixed())
		{
			SetBaseFlag(fwEntity::IS_FIXED);
		}

		if(pModelInfo->GetFragType())
		{
			m_nFlags.bIsFrag = true;
			const Matrix34 mat = MAT34V_TO_MATRIX34(GetMatrix());

#if __ASSERT
			if(!mat.IsOrthonormal())
			{
				mat.Print();
				Assertf(false, "Object %s has invalid matrix! ", GetModelName());
			}
#endif


			fragInstGta* pFragInst = rage_new fragInstGta(PH_INST_FRAG_OBJECT, pModelInfo->GetFragType(), mat);
			Assertf(pFragInst, "Failed to allocate new fragInstGta.");
			if(!pFragInst)
				return INIT_OK;

			SetPhysicsInst(pFragInst, true);

#if GTA_REPLAY
			//when a part is broken off a vehicle, the cloth setup code is called in fragInst::BreakEffects() which calls InitCloth, here it avoids 
			//actually adding the cloth in this case because it's still part of the vehicle hierarchy, it then gets split into a separate object later in the function
				
			//during replay we don't call BreakEffects(), we simple spawn the object and because of the cloth flags that are set on the model data, 
			//we try to add some cloth, unfortunately the code that handles the vehicle cloth crashes later as it expect the vehicle draw handle
			//so we makes sure that any object that has a vehicle model doesn't spawn cloth during replay.

			if(CReplayMgr::IsReplayInControlOfWorld() && pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
			{
				fragCacheEntry::SuppressClothCreation(true);
			}		
#endif

			pFragInst->SetActivateOnHit( false );
			pFragInst->Insert(false);
			pFragInst->ReportMovedBySim();

#if GTA_REPLAY
			//reset this for normal operation
			fragCacheEntry::SuppressClothCreation(false);
#endif

			// Hoop jumping cloth setup.
			fragCacheEntry *cacheEntry = pFragInst->GetCacheEntry();
			if( cacheEntry )
			{
				fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
				Assert( hierInst );
				if( hierInst->envCloth )
				{
					m_nObjectFlags.bIsRageEnvCloth = true;
				}
			}

			m_nDEflags.bIsBreakableGlass = pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0;

			if(NetworkInterface::IsGameInProgress() &&  m_nDEflags.bIsBreakableGlass)
			{
				// There may be a pre existing network object associated with this
				// glass pane that we need to update pointers to now that the geometry
				// has streamed back in...
				CGlassPaneManager::RegisterGlassGeometryObject(this);
			}

			if(pModelInfo->GetUsesDoorPhysics())
			{
				// flag this inst as a door, so it doesn't collide with other doors
				pFragInst->SetInstFlag(phInstGta::FLAG_IS_DOOR, true);
			}
		}
		else if (pModelInfo->GetHasBoundInDrawable() && pModelInfo->GetHasLoaded())
		{
			if (pModelInfo->CreatePhysicsArchetype())
			{
				phInstGta* pNewInst = rage_new phInstGta(PH_INST_OBJECT);
				Assertf(pNewInst, "Failed to allocate new phInstGta.");
				if(!pNewInst)
					return INIT_OK;

				const Matrix34 &mat = MAT34V_TO_MATRIX34(GetMatrix());
				pNewInst->Init(*pModelInfo->GetPhysics(), mat);
				SetPhysicsInst(pNewInst, true);

				pModelInfo->AddRefToDrawablePhysics();

				if(pModelInfo->GetUsesDoorPhysics())
				{
					// flag this inst as a door, so it doesn't collide with other doors
					pNewInst->SetInstFlag(phInstGta::FLAG_IS_DOOR, true);
				}
			}
		}
#if PHYSICS_REQUEST_LIST_ENABLED
		else
		{
			CPhysics::AddToPhysicsRequestList(this);
		}
#endif	//PHYSICS_REQUEST_LIST_ENABLED

	}


	if(const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(GetBaseModelInfo()->GetModelNameHash()))
	{
		// If this object is going to use low LOD buoyancy all the time, make sure it is placed in the world in low LOD
		// mode otherwise it won't move with the water until we activate it.
		if(fabs(pTuning->GetLowLodBuoyancyDistance()) < SMALL_FLOAT)
		{
			m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode = 1;
		}

		// B*1787367: Allow an object's susceptability to fire damage to be tuned from the tunableobjects.meta file.
		m_nPhysicalFlags.bNotDamagedByFlames = CTunableObjectManager::GetInstance().GetTuningEntry(GetArchetype()->GetModelNameHash())->GetNotDamagedByFlames();
	}
	
	//Note that this object has initialized physics.
	m_nObjectFlags.bHasInitPhysics = true;

	return INIT_OK;
}

void CObject::AddPhysics()
{
	CPhysical::AddPhysics();

	CBaseModelInfo* pModelInfo = GetBaseModelInfo();

	if(GetCurrentPhysicsInst() && pModelInfo->GetUsesDoorPhysics())
	{
		// handled in the CDoor code, don't want to ActivatePhysics if a door (doors can't float)
	}
	else if ( (m_nObjectFlags.bFloater) || (m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen) )
	{
		if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
		{
			ActivatePhysics();
		}
	}

	if(	NetworkInterface::IsGameInProgress() && GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel() && 
		(	GetModelNameHash() == g_boatCollisionHashName5
		||	GetModelNameHash() == g_boatCollisionHashName1
		||	GetModelNameHash() == g_boatCollisionHashName2
		||	GetModelNameHash() == g_boatCollisionHashName3
		||	GetModelNameHash() == g_boatCollisionHashName4
		)
	  )
	{
		u32 typeFlags = ArchetypeFlags::GTA_MAP_TYPE_COVER | ArchetypeFlags::GTA_OBJECT_TYPE;
		u32 includeFlags = ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE | ArchetypeFlags::GTA_VEHICLE_BVH_TYPE | ArchetypeFlags::GTA_PED_TYPE
			| ArchetypeFlags::GTA_RAGDOLL_TYPE| ArchetypeFlags::GTA_HORSE_TYPE| ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;


		PHLEVEL->SetInstanceIncludeFlags( GetCurrentPhysicsInst()->GetLevelIndex(), includeFlags );
		PHLEVEL->SetInstanceTypeFlags( GetCurrentPhysicsInst()->GetLevelIndex(), typeFlags );
	} // apa_mp_apa-yacht-


	//just in time creation of buoyancy for props, etc.  See BaseModelInfo. - sls
	if(pModelInfo && pModelInfo->GetBuoyancyInfo() == NULL)
	{
		pModelInfo->CreateBuoyancyIfNeeded();
	}

	// Make sure the is in water check is up to date
	// Pass with 0 time step so no forces are applied
	SetIsInWater( m_nPhysicalFlags.bWasInWater = m_Buoyancy.Process(this, 0.0f, true, false) );

	// Only avoid attempt to avoid collisions if this is a streamed in object.
	if(GetRelatedDummy())
	{
		// Avoid collision until the end of the next physics timeslice
		m_nPhysicsTimeSliceToAvoidCollision = CPhysics::GetTimeSliceId() + 1;
	}
	else
	{
		m_nPhysicsTimeSliceToAvoidCollision = 0;
	}
	
	//Note that this object has added physics.
	m_nObjectFlags.bHasAddedPhysics = true;

	// See if there are any network objects waiting to be hooked up with this object. 
	if (NetworkInterface::IsGameInProgress() && (GetOwnedBy()==ENTITY_OWNEDBY_RANDOM) && !m_nObjectFlags.bIsPickUp && !IsADoor())
	{
		CNetObjObject::AssignAmbientObject(*this, true);
	}

	u32 objectNameHash = GetBaseModelInfo()->GetModelNameHash();
	if( objectNameHash == MI_ROLLER_COASTER_CAR_1.GetName().GetHash() || 
		objectNameHash == MI_ROLLER_COASTER_CAR_2.GetName().GetHash() )
	{
		if( GetCurrentPhysicsInst() )
		{
			PHLEVEL->SetInactiveCollidesAgainstInactive( GetCurrentPhysicsInst()->GetLevelIndex(), true );
			GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL,true);

			PHLEVEL->ReserveInstLastMatrix( GetCurrentPhysicsInst() );
			ms_RollerCoasterVelocity.Zero();
		}
	}
}

void CObject::RemovePhysics()
{
	// Detach from the parent vehicle before calling the base class version (which will not properly detach from towing vehicle)
	if(GetAttachParent() && static_cast<CEntity*>(GetAttachParent())->GetIsTypeVehicle())
	{
		CVehicle *pAttachParentVehicle = (CVehicle *)GetAttachParent();
		for(int i = 0; i < pAttachParentVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pAttachParentVehicle->GetVehicleGadget(i);

			if(pVehicleGadget->GetType() == VGT_PICK_UP_ROPE || pVehicleGadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				CVehicleGadgetPickUpRope *pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pVehicleGadget);

                if(pPickUpRope->GetAttachedEntity() == this)
                {
				    pPickUpRope->DetachEntity(pAttachParentVehicle, true, true);
                }

				break;
			}
		}
	}

	//Call the base class version.
	CPhysical::RemovePhysics();
	
	//Note that this object has removed physics.
	m_nObjectFlags.bHasAddedPhysics = false;
}

void CObject::Fix()
{
	fragInst* pFragInst = GetFragInst();
	if(pFragInst)
	{
		// Don't want stale contacts left around with bad collider pointers
		RemovePhysics();
		AddPhysics();

		// This will fix broken off frag parts
		fragCacheEntry *pCacheEntry = pFragInst->GetCacheEntry();
		if(pCacheEntry)
		{
			FRAGCACHEMGR->ReleaseCacheEntry(pCacheEntry);
		}

		if (GetSkeleton())
		{
			GetSkeleton()->Reset();
		}

		SetMatrix(MAT34V_TO_MATRIX34(GetMatrix()), true, true, true);
	}
}


//
// Stuff that always gets called from the constructor (Init to defaults)
//
void CObject::Init()
{
	sysMemSet(&m_nObjectFlags, 0x00, sizeof(m_nObjectFlags));	// clear all object flags
	SetTypeObject();
	SetVisibilityType( VIS_ENTITY_OBJECT );

	SetBaseFlag(fwEntity::HAS_PRERENDER);
	CDynamicEntity::SetMatrix(Matrix34(Matrix34::IdentityType));

	m_PlaceableTracker.Init(this);

	m_nObjectFlags.bIsPickUp = false;
	m_nObjectFlags.bIsDoor = false;
	m_nObjectFlags.bHasBeenUprooted = false;
	m_nObjectFlags.bCanBeTargettedByPlayer = false;
	m_nObjectFlags.bIsATargetPriority = false;
	m_nObjectFlags.bTargettableWithNoLos = false;

	m_nObjectFlags.bIsStealable = false;
	m_nObjectFlags.bVehiclePart = false;
	m_nObjectFlags.bPedProp = false;
	m_nObjectFlags.bFadeOut = false;
	m_nObjectFlags.bRemoveFromWorldWhenNotVisible = false;

	m_nObjectFlags.bMovedInNetworkGame = false;
	m_nObjectFlags.bBoundsNeedRecalculatingForNavigation = false;

	m_nObjectFlags.bFloater = false;
	m_nObjectFlags.bWeaponWillFireOnImpact = false;
	m_nObjectFlags.bAmbientProp = false;
	m_nObjectFlags.bDetachedPedProp = false;
	m_nObjectFlags.bFixedForSmallCollisions = false;
	m_nObjectFlags.bSkipTempObjectRemovalChecks = false;
	m_nObjectFlags.ScriptBrainStatus = OBJECT_DOESNT_USE_SCRIPT_BRAIN;

	m_nObjectFlags.bWeWantOwnership = false;
	m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = false;
	m_nObjectFlags.bCarWheel = false;
	m_nObjectFlags.bCanBePickedUp = true;
	m_nObjectFlags.bIsHelmet = false;
	m_nObjectFlags.bIsRageEnvCloth = false;
	m_nObjectFlags.bOnlyCleanUpWhenOutOfRange = false;
	m_nObjectFlags.bPopTires = false;
	m_nObjectFlags.bHasInitPhysics = false;
	m_nObjectFlags.bHasAddedPhysics = false;

	m_nObjectFlags.bIsVehicleGadgetPart = false;
	m_nObjectFlags.bIsNetworkedFragment = false;

	m_fHealth = OBJECT_DEFAULT_HEALTH;
	m_fMaxHealth = OBJECT_DEFAULT_HEALTH;

	// union remember - so don't try to initialise with anything other than 0
	uMiscData.m_fFloaterHeight = 0.0f;
	uPropData.m_parentModelIdx = 0;
	uPropData.m_propIdxHash = 0;
	uPropData.m_texIdxHash = 0;

	m_nEndOfLifeTimer = 0;	// set manually after object created

	m_bIntentionallyLeakedObject = false;
	m_pRelatedDummy = NULL;
	m_pFragParent = NULL;
	m_pGroundPhysical = NULL;
	m_relatedDummyLocation.MakeInvalid();

	m_iPathServerDynObjId = DYNAMIC_OBJECT_INDEX_NONE;
	StreamedScriptBrainToLoad = -1;

	m_pWeapon = NULL;

	m_eLastCoverOrientation = CO_Invalid;

	m_firstMovedTimer = 0;
	m_fTargetableDistance = 0.0f;

	m_pWeaponComponent = NULL;

	m_pIntelligence = NULL;

	m_vehModSlot = -1;
	m_speedBoost = 30;
	m_speedBoostDuration = 10;
	m_jointToDriveIndex = 0;

	m_bShootThroughWhenEquipped = false;
	m_bForcePostCameraAiUpdate = false;
	m_bForcePostCameraAnimUpdate = false;
	m_bForcePostCameraAnimUpdateUseZeroTimeStep = false;
	m_bFixedByScriptOrCode = false;
	m_bForceVehiclesToAvoid = false;
	m_bHasNetworkedFrags = false;
	m_bDisableObjectMapCollision = false;
	m_bIsAttachedToHandler = false;
	m_bTakesDamageFromCollisionsWithBuildings = false;
	m_bIsParachute = false;
	m_bIsCarParachute = false;
	m_bRegisteredInNetworkGame = false;
	m_bUseLightOverride = false;
    m_bProjectilesExplodeOnImpact = false;
	m_bDriveToMaxAngle = false;
	m_bDriveToMinAngle = false;
	m_bIsPressurePlate = false;
	m_bIsArticulatedProp = false;
	m_bWeaponImpactsApplyGreaterForce = false;
	m_bIsBall = false;
	m_bBallShunted = false;
	m_bExtraBounceApplied = false;
	m_bIsAlbedoAlpha = false;

#if NORTH_CLOTHS
	m_pCloth = 0;
#endif

#if DEFORMABLE_OBJECTS
	m_pDeformableData = NULL;
#endif // DEFORMABLE_OBJECTS

	REPLAY_ONLY(CReplayMgr::OnCreateEntity(this));
	REPLAY_ONLY(m_hash = CReplayMgr::GetInvalidHash());
	REPLAY_ONLY(m_MapObject = NULL);

	m_pGadgetParentVehicle = NULL;

}

//
//
//
//
bool CObject::CanBeDeleted(void) const
{
	if (GetNetworkObject() && !GetNetworkObject()->CanDelete())
		return FALSE;

	bool bAmbientObject = CNetObjObject::IsAmbientObjectType(GetOwnedBy()) && !m_nObjectFlags.bIsPickUp;

	if (NetworkUtils::IsNetworkCloneOrMigrating(this) && !bAmbientObject && !IsADoor()) // this object is being controlled by a remote machine. Doors and ambient objects are a special case, they are allowed to be removed locally.
		return FALSE;

#if __BANK

	extern	RegdEnt gpDisplayObject;

	if ( this == gpDisplayObject.Get() )
	{
		return FALSE;
	}
#endif	//__BANK

	switch(this->GetOwnedBy())
	{
		case(ENTITY_OWNEDBY_RANDOM)		:	//	This object was created as a replacement for a dummy and
			return(TRUE);			// can be deleted

		case(ENTITY_OWNEDBY_SCRIPT)	:	//	Mission created objects should exist for the duration
			return(FALSE);			//	of the mission

		case(ENTITY_OWNEDBY_TEMP)		:	// Temporarily spawned object (to be deleted soon)
			return(TRUE);

		case(ENTITY_OWNEDBY_CUTSCENE)	:	// cutscene object. should exist for duration of cutscene
			return(FALSE);

		case(ENTITY_OWNEDBY_GAME)		:	// game object. cannot be deleted because something else is
			return(FALSE);			// in control of it

		case(ENTITY_OWNEDBY_COMPENTITY)		:  //compEntity created object - so it must free it.
			return(FALSE);

		case (ENTITY_OWNEDBY_REPLAY):
			return(FALSE);

		case (ENTITY_OWNEDBY_FRAGMENT_CACHE):
			{
				if(m_nObjectFlags.bIsNetworkedFragment)
				{
					return (FALSE);
				}
				return (TRUE);
			}
		case (ENTITY_OWNEDBY_AUDIO):
		case (ENTITY_OWNEDBY_DEBUG):
		case (ENTITY_OWNEDBY_OTHER):
		case (ENTITY_OWNEDBY_PROCEDURAL):
		case (ENTITY_OWNEDBY_POPULATION):
		case (ENTITY_OWNEDBY_STATICBOUNDS):
		case (ENTITY_OWNEDBY_PHYSICS):
		case (ENTITY_OWNEDBY_IPL):
		case (ENTITY_OWNEDBY_VFX):
		case (ENTITY_OWNEDBY_NAVMESHEXPORTER):
		case (ENTITY_OWNEDBY_INTERIOR):
		case (ENTITY_OWNEDBY_LIVEEDIT):
			return (TRUE);
		default:
			Assertf( false, "This object should not have this GetOwnedBy() value" );
			break;
	}

	Assert(0);
	return(TRUE);

}//end of CObject::CanBeDeleted()...


void CObject::SetWeapon(CWeapon* pWeapon)
{
	if (m_pWeapon && m_pWeapon != pWeapon)
	{
		delete m_pWeapon;
	}

	m_pWeapon = pWeapon;

	if(m_pWeapon)
	{
		// Always update anims
		m_nDEflags.bForcePrePhysicsAnimUpdate = true;
		UpdateRenderFlags();
	}
}

void CObject::SetWeaponComponent(CWeaponComponent* pWeaponComponent)
{
	m_pWeaponComponent = pWeaponComponent; // Set the LSB to state that this is a weapon component

	if(m_pWeaponComponent)
	{
		// Always update anims
		m_nDEflags.bForcePrePhysicsAnimUpdate = true;
	}

}

void CObject::SetRelatedDummy(CDummyObject* pDummy, fwInteriorLocation loc)
{
	m_pRelatedDummy = pDummy;
	if (pDummy)
	{
		m_relatedDummyLocation = loc;
	#if GTA_REPLAY
		// The hash needs to be the hash associated with the source dummy map object.
		m_hash = pDummy->GetHash();
	#endif // GTA_REPLAY
	} else {
		m_relatedDummyLocation.MakeInvalid();
	}

}

void CObject::ClearRelatedDummy()
{
	m_pRelatedDummy = NULL;
	m_relatedDummyLocation.MakeInvalid();
}

void CObject::SetFragParent(CEntity* pEnt)
{
	Assert(!m_pFragParent);
	m_pFragParent = pEnt;
	// need to store a flag to remember we broke off a vehicle
	// if the vehicle gets deleted we need to delete all it's parts as well because they won't be able to render safely
	if(m_pFragParent && m_pFragParent->GetIsTypeVehicle())
	{
		CVehicle* veh = (CVehicle*)m_pFragParent.Get();
		veh->SetHasBeenRepainted(false);

		m_nObjectFlags.bVehiclePart = true;
		UpdateRenderFlags();	// make sure RenderFlag_USE_CUSTOMSHADOWS is set for separated wheel

		m_vehModSlot = CVehicle::GetLastBrokenOffPart();
	}
	else if (m_pFragParent && m_pFragParent->GetIsTypePed())
	{
		m_nObjectFlags.bPedProp = true;
	}
}

void CObject::ClearFragParent()
{
	m_pFragParent = NULL;
}

CTask* CObject::GetTask(u32 const tree) const
{
	if(!m_pIntelligence)
	{
		return NULL;
	}

	return m_pIntelligence->GetTaskActive(tree);
}

bool CObject::SetTask(aiTask* pTask, u32 const tree /*= OBJECT_TASK_TREE_PRIMARY*/, u32 const priority /* = OBJECT_TASK_PRIORITY_PRIMARY*/)
{
	// If the intelligence is not available then lazily create it
	if (!m_pIntelligence)
	{
		if (CObjectIntelligence::GetPool()->GetNoOfFreeSpaces() == 0)
		{
#if __DEV
			CObjectIntelligence::GetPool()->PoolFullCallback();
			Assertf(0, "Ran out of CObjectIntelligence pool, see TTY for details");
#endif // __DEV

			// Ran out of storage, fail to set the task for now, we need to cleanup the memory as ownership has been handed to us
			delete pTask;
			return false;
		}
		m_pIntelligence = rage_new CObjectIntelligence(this);
		AddToOrRemoveFromProcessPostCameraArray();
	}

	//
	// Assume we are going to be playing animations if we have a task - we may want to revisit this assumption
	//
	InitAnimLazy();

	m_pIntelligence->SetTask(pTask, tree, priority);
	return true;
}


bool CObject::InitAnimLazy()
{
	// Put into the fragment cache if it isn't already
	if (fragInst * pEntityFragInst = GetFragInst())
	{
		if (!pEntityFragInst->GetCached())
		{
			pEntityFragInst->PutIntoCache();
		}
	}

	// Lazily create the drawable if it doesn't exist
	if (!GetDrawable())
	{
		CreateDrawable();
	}

	if (GetDrawable())
	{
		// Lazily create the Skeleton if it doesn't exist
		if (!GetSkeleton())
		{
			CreateSkeleton();
		}

		animAssertf(GetSkeleton(), "Failed to create Skeleton for object '%s'", GetModelName());
		if (GetSkeleton())
		{
			// Lazily create the AnimDirector if it doesn't exist
			if (!GetAnimDirector())
			{
				CreateAnimDirector(*GetDrawable());
			}		

			animAssertf(GetAnimDirector(), "Failed to create AnimDirector for object '%s'", GetModelName());
			if (GetAnimDirector())
			{
				animDebugf2("InitAnimLazy: Successfully found skeleton and anim director for object %s(%p) - %s.", this->GetModelName(), this, this->GetDebugName());
				// Object has to be on the process control list for the animation system to be updated
				if (!GetIsOnSceneUpdate())
				{
					AddToSceneUpdate();
				}
				return true;
			}
		}
	}
	else
	{
		animDebugf2("InitAnimLazy: Not creating skeleton / anim director for object %s(%p) - %s, because the object doesn't currently have a drawable.", this->GetModelName(), this, this->GetDebugName());
#if !__NO_OUTPUT
		if (Channel_anim.FileLevel >= DIAG_SEVERITY_DEBUG2)
		{
			sysStack::PrintStackTrace();
		}
#endif // !__NO_OUTPUT
	}

	return false;
}

CDynamicEntity* CObject::GetProcessControlOrderParent() const
{
	if(m_pWeapon && GetAttachParent() && static_cast<CEntity*>(GetAttachParent())->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(GetAttachParent());
		if (pPed->IsLocalPlayer() && pPed->GetStartAnimSceneUpdateFlag()==CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS)
		{
			return pPed;
		}
	}

	return NULL;
}

u32 CObject::GetStartAnimSceneUpdateFlag() const
{
	if (GetProcessControlOrderParent()!=NULL)
	{
		return CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2;
	}

	return CPhysical::GetStartAnimSceneUpdateFlag();
}

void CObject::InstantAnimUpdate()
{
	if(!GetAnimDirector())
		return;

	bool bCacheForcePrePhysicsAnimUpdate = m_nDEflags.bForcePrePhysicsAnimUpdate;
	m_nDEflags.bForcePrePhysicsAnimUpdate = true;

	StartAnimUpdate(m_bForcePostCameraAnimUpdateUseZeroTimeStep ? 0.0f : fwTimer::GetTimeStep());

	m_nDEflags.bForcePrePhysicsAnimUpdate = bCacheForcePrePhysicsAnimUpdate;

	GetAnimDirector()->WaitForPrePhysicsUpdateToComplete();
}

void CObject::InstantAiUpdate()
{
	if(IsNetworkClone())
		return;

	if (GetObjectIntelligence())
	{
		GetObjectIntelligence()->Process();
		if (GetObjectIntelligence()->GetTaskActive(CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY) == NULL &&
			GetObjectIntelligence()->GetTaskActive(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY) == NULL)
		{
			delete m_pIntelligence;
			m_pIntelligence = NULL;
			AddToOrRemoveFromProcessPostCameraArray();
		}
	}
}

void CObject::OnActivate(phInst* pInst, phInst* pOtherInst)
{
	// Must be called first or GetCollider won't work correctly
	CPhysical::OnActivate(pInst, pOtherInst);

	if (GetDisablesPushCollisions())
	{
		if (phCollider* collider = GetCollider())
		{
			collider->DisablePushCollisions();
		}
	}

	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	if (m_pRelatedDummy && pModelInfo && pModelInfo->GetLeakObjectsFlag())
	{
		DecoupleFromMap();
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() == false)
	{
		// During normal gameplay this object has been activated so let the replay
		// know so it can hide the equivalent on playback
		CReplayMgr::RecordMapObject(this);
	}
#endif // GTA_REPLAY
}

bool CObject::RequiresProcessControl() const
{
	if(!GetIsStatic())
		return true;

	// Need to make sure we still call ProcessPrePhysics() so that we can monitor when collision has streamed in and we can
	// unfix the object again.
	if(GetIsFixedUntilCollisionFlagSet())
		return true;

	if(GetBaseModelInfo()->GetUsesDoorPhysics())
		return true;

	// an object that is cloned on other machines needs to be on the process control list so that it will try to pass control
	// of itself to other players when they get close. This could be changed in the future
	// clones also need to be on the process control list so they are ready to be moved (PrepareForActivation may not be called)
	if (GetNetworkObject())
		return true;

	// Object has to be on the process control list for intelligence to be processed
	if(GetObjectIntelligence())
		return true;

	// Object has to be on the process control list for the animation blender to be updated
	if(GetAnimDirector())
		return true;

	// deal with floating procedural objects
	if (m_nFlags.bIsFloatingProcObject)
		return true;

	if (m_nObjectFlags.bFloater)
		return true;

	if(GetWeapon())
		return true;

	// going to be deleted during process control
	if (IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
		return true;

	// Maybe should have entity flag for this
	if(m_nObjectFlags.bIsRageEnvCloth)
	{
		return true;
	}

	if(m_nDEflags.bIsBreakableGlass)
	{
		return true;
	}
	
	if(m_nObjectFlags.bRemoveFromWorldWhenNotVisible)
	{
		return true;
	}

	// Objects in low LOD buoyancy mode need to call ProcessControl() since that calls ProcessLowLodBuoyancy().
	if(m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode)
	{
		return true;
	}

	if(m_nObjectFlags.bIsNetworkedFragment)
	{
		return true;
	}

	// otherwise don't need processControl to be called
	return false;
}


//
//
//
//
//
bool CObject::ProcessControl()
{
	PF_AUTO_PUSH_TIMEBAR("CObject::ProcessControl()");
	perfClearingHouse::Increment(perfClearingHouse::OBJECTS);

	CBaseModelInfo *pBaseModelInfo = GetBaseModelInfo();
	if(pBaseModelInfo && pBaseModelInfo->GetAutoStartAnim())
	{
		CEntity *pRootLod = NULL;
		CDummyObject *pDummyObject = GetRelatedDummy();
		if(pDummyObject)
		{
			pRootLod = (CEntity *)pDummyObject->GetRootLod();
		}
		if(pRootLod && GetAnimDirector())
		{
			CGenericClipHelper& clipHelper = GetMoveObject().GetClipHelper();
			const crClip *pClip = clipHelper.GetClip();
			if(pClip)
			{
				u32 randomSeed = 0;
				if(pRootLod)
				{
					randomSeed = pRootLod->GetLodRandomSeed(pRootLod);
				}

				float rate = 1.0f;
				const CClipEventTags::CRandomizeRateProperty *pRateProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeRateProperty >(pClip, CClipEventTags::RandomizeRate);
				if(pRateProp)
				{
					rate = pRateProp->PickRandomRate(randomSeed);
				}

				float phase = 0.0f;
				const CClipEventTags::CRandomizeStartPhaseProperty *pPhaseProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeStartPhaseProperty >(pClip, CClipEventTags::RandomizeStartPhase);
				if(pPhaseProp)
				{
					phase = pPhaseProp->PickRandomStartPhase(randomSeed);
				}

				float time = pClip->ConvertPhaseToTime(phase);
				u32 basetime = NetworkInterface::IsNetworkOpen() ? (u32) NetworkInterface::GetSyncedTimeInMilliseconds() & 0x000FFFFF : fwTimer::GetTimeInMilliseconds(); //use the synchronized network time in MP to keep the animation movement aligned - only use lower bits of network time as it is too large
				time += ((static_cast< float >(basetime) / 1000.0f) * rate);
				phase = pClip->ConvertTimeToPhase(time);
				phase = phase - Floorf(phase);

				clipHelper.SetRate(rate);
				clipHelper.SetPhase(phase);
			}
		}
	}

	// Fire off an asynchronous probe to test if this physical is in a river and how deep it
	// is submerged for the buoyancy code. Don't do this if the object is attached.
	if(!GetIsAttached())
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		m_Buoyancy.SubmitRiverBoundProbe(vPos);
	}

	// do any visual effect due to collision
	DoCollisionEffects();

	// if the parent vehicle this part broke off has been deleted, delete ourselves
	REPLAY_ONLY(if(CReplayMgr::IsReplayInControlOfWorld() == false || GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
	{
		if(m_nObjectFlags.bVehiclePart && !m_nObjectFlags.bIsNetworkedFragment)
		{
			CVehicle* veh = (CVehicle*)GetFragParent();
			if (!veh || veh->GetHasBeenRepainted())
			{
				SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
			}
		}
		else if(m_nObjectFlags.bPedProp && GetFragParent()==NULL)
		{
			SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
		}

		//Check if we should remove the object from the world when it is not visible.
		if(m_nObjectFlags.bRemoveFromWorldWhenNotVisible)
		{
			//The object is not visible if it has been marked as not visible, or the alpha has faded to zero.
			if(!g_LodMgr.WithinVisibleRange(this, true) || !GetIsVisible() || (GetLodData().GetAlpha() == 0) || !GetIsOnScreen())
			{
				if (NetworkUtils::IsNetworkCloneOrMigrating(this))
				{
					gnetDebug2("Trying to delete object %s that is not owned by us!", GetLogName());
				}
				else
				{
					SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
				}				
			}
		}
	}

	const CCollisionHistory* pCollisionHistory = GetFrameCollisionHistory();
	for(const CCollisionRecord* pColRecord = pCollisionHistory->GetFirstVehicleCollisionRecord();
		pColRecord != NULL ; pColRecord = pColRecord->GetNext())
    {
		float fImpulse = pCollisionHistory->GetCollisionImpulseMagSum() * GetInvMass();

		ObjectDamage(fImpulse, NULL, NULL, pColRecord->m_pRegdCollisionEntity.Get(), WEAPONTYPE_RUNOVERBYVEHICLE);
    }

	if(m_bTakesDamageFromCollisionsWithBuildings)
	{
		const CCollisionHistory* pCollisionHistory = GetFrameCollisionHistory();
		for(const CCollisionRecord* pColRecord = pCollisionHistory->GetFirstBuildingCollisionRecord();
			pColRecord != NULL ; pColRecord = pColRecord->GetNext())
		{
			float fImpulse = pCollisionHistory->GetCollisionImpulseMagSum() * GetInvMass();

			ObjectDamage(fImpulse, NULL, NULL, pColRecord->m_pRegdCollisionEntity.Get(), WEAPONTYPE_FALL);
		}
	}

	if (GetObjectIntelligence())
	{
		// B* 1959208:	When running animations, we need to make sure we flip the move buffers before running
		//				object A.I. processes (or our MoVE signals will be a frame out)
		if (GetUpdatingThroughAnimQueue() && GetAnimDirector())
		{
			GetAnimDirector()->SwapMoveOutputBuffers();
		}

		GetObjectIntelligence()->Process();
		if (GetObjectIntelligence()->GetTaskActive(CObjectIntelligence::OBJECT_TASK_TREE_PRIMARY) == NULL &&
			GetObjectIntelligence()->GetTaskActive(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY) == NULL)
		{
			delete m_pIntelligence;
			m_pIntelligence = NULL;
			AddToOrRemoveFromProcessPostCameraArray();
		}
	}

	if(m_pWeapon)
	{
		if(!GetAttachParent() && m_nObjectFlags.bWeaponWillFireOnImpact && m_pWeapon->GetWeaponInfo()->GetIsGun() && GetCurrentPhysicsInst())
		{
			// fire when it hits the ground (only once please)
			if(pCollisionHistory->GetMaxCollisionImpulseMagLastFrame() > 0.5f*GetMass() && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > 0.7f)
			{
				weaponDebugf1("Firing weapon [0x%p] due to bWeaponWillFireOnImpact", this);

				Vector3 vecStart, vecEnd;
				Matrix34 m = MAT34V_TO_MATRIX34(GetMatrix());
				m_pWeapon->CalcFireVecFromGunOrientation(this, m, vecStart, vecEnd);
				m_pWeapon->Fire(CWeapon::sFireParams(this, m, &vecStart, &vecEnd));

				// We will play an extra sound when the player's dropped gun fires during the death scene, we do this because normal guns are muted
				CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
				if(pLocalPlayer && pLocalPlayer->GetIsDeadOrDying())
				{
					audSoundInitParams initParams;
					initParams.TimerId = 1;
					g_FrontendAudioEntity.CreateAndPlaySound(ATSTRINGHASH("PLAYER_GUN_DROPPED_SLOWMO_DEATHSCREEN", 0x15C0C58A), &initParams);
				}
				m_nObjectFlags.bWeaponWillFireOnImpact = false;
			}
		}

		CPhysical *pParentEntity = (CPhysical *) GetAttachParent();
		m_pWeapon->Process(pParentEntity, fwTimer::GetTimeInMilliseconds());
		m_pWeapon->Update_HD_Models(pParentEntity);

		// if weapon is moving fast, need to force it's bound radius to be bigger to make sure it gets rendered
		phInst* pInst = GetCurrentPhysicsInst();
		if(pInst)
		{
			// Calculate the object velocity
			Vector3 vVel(RCC_MATRIX34(pInst->GetMatrix()).d - RCC_MATRIX34(PHSIM->GetLastInstanceMatrix(pInst)).d);
			if(vVel.Mag2() > 0.01f)
				SetForceAddToBoundRadius(vVel.Mag());
		}

#if __DEV
		static int snRenderWeaponBones = -2;
		if(snRenderWeaponBones > -2)
		{
			for(int i=0; i<GetSkeleton()->GetSkeletonData().GetNumBones(); i++)
			{
				if(snRenderWeaponBones < 0 || i==snRenderWeaponBones)
				{
					Matrix34 mBone;
					GetSkeleton()->GetGlobalMtx(i, RC_MAT34V(mBone));
					grcDebugDraw::Axis(mBone, 0.3f);
				}
			}
		}
#endif // __DEV
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (m_nObjectFlags.bMovedInNetworkGame && !m_bRegisteredInNetworkGame)
		{
			ObjectHasBeenMoved(NULL, true);
		}

		if (m_firstMovedTimer > 0 && !GetNetworkObject())
		{
			// wait for while to see if the machine in control of the entity that collided with this object will claim control of it,
			// otherwise we will take control
			m_firstMovedTimer -= fwTimer::GetSystemTimeStepInMilliseconds();

			if (m_firstMovedTimer <= 0 )
			{
				m_firstMovedTimer = 0;
				CNetObjObject::TryToRegisterAmbientObject(*this, false, "Moved by a clone but not claimed");
			}
		}
		else
		{
			m_firstMovedTimer = 0;
		}
	}

	// GTAV - B*2503160 - The editor now allows objects, that should only ever be placed in water, to be placed on the ground
	// we have to force these to be active or they sink into the ground when they go to sleep.
	// We know they will be script entities and won't have the bScriptLowLodOverride set
	if( ( GetIsInWater() && !m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode ) ||
		( IsAScriptEntity() && 
		  ( m_nFlags.bIsFloatingProcObject || m_nObjectFlags.bFloater ) && 
		  !m_Buoyancy.m_buoyancyFlags.bScriptLowLodOverride ) )
	{
		phCollider* pCollider = GetCollider();
		// check if this vehicle is asleep, and wake up when necessary
		if (!pCollider || (pCollider->GetSleep() && pCollider->GetSleep()->IsAsleep()))
		{
			if(pCollider)
			{
				pCollider->GetSleep()->Reset(true);
			}
			else if(!GetIsAttached() || GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION)
				|| !(GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_BASIC || GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD) )
			{
				ActivatePhysics();
			}
		}
	}

	if( m_bDriveToMaxAngle ||
		m_bDriveToMinAngle )
	{
		phCollider* pCollider = GetCollider();

		// check if this object is asleep, and wake up when necessary
		if( !pCollider || ( pCollider->GetSleep() && pCollider->GetSleep()->IsAsleep() ) )
		{
			if( pCollider )
			{
				pCollider->GetSleep()->Reset( true );
			}
			
			ActivatePhysics();
		}
	}

	// If an object is in low LOD buoyancy mode, it is physically inactive and we place it based on the water level sampled at
	// points around the edges of the bounding box.
	bool bAttachTypeAllowsLowLodBuoyancy = !GetIsAttached() ||
		!(GetAttachmentExtension()->IsAttachStateBasicDerived() || GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD);
	if(bAttachTypeAllowsLowLodBuoyancy && m_nObjectFlags.bVehiclePart==0)
	{
		m_Buoyancy.ProcessLowLodBuoyancy(this);
	}

	if(!m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode && (m_nFlags.bIsFloatingProcObject || m_nObjectFlags.bFloater) && !GetCollider())
	{
		float waterZ;
		const Vector3 vOriginalPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		if(m_Buoyancy.GetWaterLevelIncludingRivers(vOriginalPosition, &waterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, this))
		{
			if(m_nObjectFlags.bFloater)
				waterZ -= uMiscData.m_fFloaterHeight;

			SetPosition(Vector3(vOriginalPosition.x, vOriginalPosition.y, waterZ));
		}
	}

	if( m_nDEflags.bIsBreakableGlass )
	{
		fragInst *pFragInst = (fragInst*)GetFragInst();
		if(pFragInst)
		{
			bool isActive = false;

			if(fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry())
			{
				if(fragHierarchyInst* pHierInst = pCacheEntry->GetHierInst())
				{
					for(int glassIndex = 0; glassIndex != pHierInst->numGlassInfos; ++ glassIndex)
					{
						bgGlassHandle handle = pHierInst->glassInfos[glassIndex].handle;
						if(handle != bgGlassHandle_Invalid)
						{
							bgDrawable& drawable = bgGlassManager::GetGlassDrawable(handle);
							if( drawable.GetBuffers() )
							{
								isActive = true;
								break;
							}
						}
					}
				}
			}

			if( isActive )
				UpdateWorldFromEntity();
		}
	}

	if (!GetIsStatic())	// This can be the case for objects that have AlwaysOnMovingList set (like the sam site)
	{
		CPhysical::ProcessControl();
	}

	ProcessControlLogic();

	return RequiresProcessControl();
}//end of CObject::ProcessControl()...


ePhysicsResult CObject::ProcessPhysics(float fTimeStep, bool UNUSED_PARAM(bCanPostpone), int nTimeSlice)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessPhysicsOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	// Don't reactivate if we are in low LOD anchor mode (this would happen if we call ProcessIsAsleep() while in the water).
	phCollider* pCollider = GetCollider();
	if(nTimeSlice==0)
	{
		// Only check for high->low lod conversions if the object is active and can convert. 
		if(m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode || (pCollider != NULL && m_Buoyancy.CanUseLowLodBuoyancyMode(this)))
		{
			m_Buoyancy.UpdateBuoyancyLodMode(this);
		}
	}

	DriveToJointToTarget();

	if(m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode)
		return PHYSICS_DONE;


	if (GetObjectIntelligence())
		GetObjectIntelligence()->ProcessPhysics(fTimeStep, nTimeSlice);

	if (pCollider != NULL)
	{
		// If bPossiblyTouchesWater is false we are either well above the surface of the water or in a tunnel.
		if ( m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate && !m_nFlags.bPossiblyTouchesWater)
		{
			SetIsInWater( false );
			m_Buoyancy.ResetBuoyancy();
		}
		// else process the buoyancy class
		else
		{
			SetIsInWater( m_Buoyancy.Process(this, fTimeStep, true, CPhysics::GetIsLastTimeSlice(nTimeSlice)) );
		}
	}

	if(m_nObjectFlags.bIsRageEnvCloth)
	{
		if(CPhysics::GetIsLastTimeSlice(nTimeSlice))
		{
			UpdateWorldFromEntity();
		}
	}

	return PHYSICS_DONE;
}

__forceinline bool GetPointOnAnyEdge(Vec3V_In point0, Vec3V_In point1, Vec3V_In point2, Vec3V_In testPoint, ScalarV_In squaredMargin)
{
	const Vec3V edge01 = Subtract(point1, point0);
	const Vec3V edge12 = Subtract(point2, point1);
	const Vec3V edge20 = Subtract(point0, point2);

	const Vec3V point0ToTestPoint = Subtract(testPoint, point0);
	const Vec3V point1ToTestPoint = Subtract(testPoint, point1);
	const Vec3V point2ToTestPoint = Subtract(testPoint, point2);

	const ScalarV fractionAlongEdge0 = Scale(Dot(edge01, point0ToTestPoint),InvMagSquared(edge01));
	const ScalarV fractionAlongEdge1 = Scale(Dot(edge12, point1ToTestPoint),InvMagSquared(edge12));
	const ScalarV fractionAlongEdge2 = Scale(Dot(edge20, point2ToTestPoint),InvMagSquared(edge20));

	const Vec3V point0Projected = AddScaled(point0,edge01,fractionAlongEdge0);
	const Vec3V point1Projected = AddScaled(point1,edge12,fractionAlongEdge1);
	const Vec3V point2Projected = AddScaled(point2,edge20,fractionAlongEdge2);

	const Vec3V distSquaredToEdge = Vec3V(MagSquared(Subtract(testPoint, point0Projected)),
		MagSquared(Subtract(testPoint, point1Projected)),
		MagSquared(Subtract(testPoint, point2Projected)));

	return (IsLessThanOrEqualAll(distSquaredToEdge, Vec3V(squaredMargin)) != 0);
}

bool DoGolfImpactsPuttingHack(phContactIterator impacts, const phBoundBVH* otherBoundBvh, const phPolygon& otherPoly, Vec3V_In triVert0, Vec3V_In triVert1, Vec3V_In triVert2, Vec3V_In triNorm, const phPolygon::Index /*triNumber*/, const u32 recurseCount)
{
	// Project ball position onto tri plane
	const Vec3V ballContactPosition = impacts.GetMyPosition();
	const ScalarV ballProjVerticalOffset = Dot(Subtract(ballContactPosition, triVert0), triNorm); // - Because we care about the sign of this value later
	const Vec3V ballProjOnPlane = Subtract( ballContactPosition, Scale(ballProjVerticalOffset, triNorm) );

	// Determine if we're outside the triangle by some margin AND not deep behind it by some amount
	const Vec3V triPoints[3] = {triVert0, triVert1, triVert2};
	const ScalarV ballDistanceTolerance(0.005f); // Roughly one ball radius is 0.025f
	const ScalarV ballBackfacingTolerance(-0.04f);
	const bool isInFrontOfTri = IsGreaterThanAll(ballProjVerticalOffset, ballBackfacingTolerance) != 0;
	if(!geomSpheres::TestSphereToTriangle(Vec4V(ballProjOnPlane, ballDistanceTolerance), triPoints, triNorm) && isInFrontOfTri )
	{
		// Determine which neighbor we're most towards
		const int neighborPolyNum = otherPoly.GetNeighborFromExteriorPoint(ballProjOnPlane, triPoints, triNorm);
		Assert(neighborPolyNum >= 0);
		const phPolygon::Index neighborPolyIndex = otherPoly.GetNeighboringPolyNum(neighborPolyNum);
		if(neighborPolyIndex == phPolygon::Index(BAD_INDEX))
		{
			// If we outside of the poly and there is no neighbor along the edge we're most outside of we fall back on disabling the contact
			return true;
		}
		else
		{
			const Vec3V neighborPolyNormal = Normalize(otherBoundBvh->GetPolygonNonUnitNormal(neighborPolyIndex));

			// If the neighbor's normal is basically the same as ours we'll recurse across to that neighbor and see if we're outside of that one too
			const ScalarV neighborNormalSamenessThresh(0.9f);
			if(IsGreaterThanOrEqualAll(Dot(neighborPolyNormal, triNorm), neighborNormalSamenessThresh) != 0)
			{
				// Too close - recurse for this neighbor
				const phPolygon& nextPoly = otherBoundBvh->GetPolygon(neighborPolyIndex);
				
				// I do not think it possible to infinitely recurse either back and forth across one edge or cyclically through some other set of polys
				// but we're already hackily dependent on the setup of the art for the golf areas with these shenanigans so it's easy enough to toss in a failsafe
				const u32 maxRecurse = 8;
				if(recurseCount < maxRecurse)
				{
					const int vertIndex0 = nextPoly.GetVertexIndex(0);
					const int vertIndex1 = nextPoly.GetVertexIndex(1);
					const int vertIndex2 = nextPoly.GetVertexIndex(2);
					Assert(vertIndex0 < otherBoundBvh->GetNumVertices());
					Assert(vertIndex1 < otherBoundBvh->GetNumVertices());
					Assert(vertIndex2 < otherBoundBvh->GetNumVertices());
					const Vec3V neighborTriVert0 = otherBoundBvh->GetBVHVertex(vertIndex0);
					const Vec3V neighborTriVert1 = otherBoundBvh->GetBVHVertex(vertIndex1);
					const Vec3V neighborTriVert2 = otherBoundBvh->GetBVHVertex(vertIndex2);

					return DoGolfImpactsPuttingHack(impacts, otherBoundBvh, nextPoly, neighborTriVert0, neighborTriVert1, neighborTriVert2, neighborPolyNormal, neighborPolyIndex, recurseCount + 1);
				}
				else
				{
					Warningf("Golf ball hack exceeded maximum allowed recursion!");
					return false;
				}
			}
			else
			{
				// But if the neighbor's normal IS different enough we'll change this impact to match
				// - Note that this relies a bit on the tessellation structure of the golf areas being friendly to this approach
				// - Hole areas should be modeled very flat with long polys leading into a sudden drop into the hole - a lip or bump could mess this up
				//Vec3V otherNorm;
				//impacts.GetMyNormal(otherNorm);
				//Displayf("### Golf hack: %d < %5.3f, %5.3f, %5.3f > -- < %5.3f, %5.3f, %5.3f >", neighborPolyIndex, otherNorm.GetXf(), otherNorm.GetYf(), otherNorm.GetZf(), neighborPolyNormal.GetXf(), neighborPolyNormal.GetYf(), neighborPolyNormal.GetZf());
		//		impacts.SetMyNormal(neighborPolyNormal);

				// Leaving the contact where it is but altering the normal drastically is effectively the same as disabling the contact in most cases
				// In others, we actually think it could be a source of pops - so we're going to return that this should just be disabled instead
				// -- We'll want to flip this back to false if we ever get around to our ideal of updating the contact to all the way under the ball
				return true;

				// Ideally, we might actually walk the poly side of the contact along the neighbors we recurse down,
				// actually changing the element and contact world position as we go
				// But that's likely to cause issues with something
				// - First thing to come to mind is the fact that we might appear to end up with duplicate manifolds since we use the poly element number as part of the unique ID for a manifold pair
			}
		}
	}
	else
	{
		// We know we're in front of this poly and touching it
		// - Move the other side of the contact directly underneath
		const ScalarV distMargin(0.0f);//CONCAVE_DISTANCE_MARGIN);
		const Vec3V marginOffset = Scale(triNorm, distMargin);
		const Vec3V otherPosNew = rage::Add(ballProjOnPlane, marginOffset);
		impacts.SetOtherPosition(otherPosNew);
		impacts.SetMyNormal(triNorm);
		
		const Vec3V myPos = impacts.GetMyPosition();
		const ScalarV newDepth = Dot(triNorm, Subtract(otherPosNew, myPos));
		impacts.SetDepth(newDepth);
		impacts.GetContact().SetPositiveDepth( IsGreaterThanOrEqualAll(newDepth, ScalarV(V_ZERO)) != 0 );

		return false;
	}
}

void CObject::ProcessPreComputeImpacts(phContactIterator impacts)
{
	m_bBallShunted = false;
	m_bExtraBounceApplied = false;

	//Call the base class version.
	CPhysical::ProcessPreComputeImpacts(impacts);

	bool isAvoidingCollision = IsAvoidingCollision();
	bool needsToIterate = (m_nObjectFlags.bFixedForSmallCollisions || IsUsingKinematicPhysics() || m_nDEflags.bIsGolfBall
		|| GetDisableObjectMapCollision() || isAvoidingCollision/*|| something about needing a ground physical*/
		|| IsAttachedToHandler() || (m_nObjectFlags.bVehiclePart/* && m_nObjectFlags.bCarWheel*/) || m_bIsArticulatedProp );
	
	const CTunableObjectEntry *tuneEntry = CTunableObjectManager::GetInstance().GetTuningEntry(GetArchetype()->GetModelNameHash());

	//Process the impacts.
	impacts.ResetToFirstActiveContact();
	while(!impacts.AtEnd())
	{
		//Ensure the impact is not a constraint.
		if(impacts.IsConstraint())
		{
			impacts.NextActiveContact();
			continue;
		}

		//////////////////////////////////////////////////////////////////////////

		phInst *pOtherInstance = impacts.GetOtherInstance();
		CEntity *pHitEntity = CPhysics::GetEntityFromInst(pOtherInstance);

		// B*1918379: Allow objects to be flagged so that they only collide with the low-LOD version of a vehicle's chassis (if the vehicle allows it).
		// Also prevent objects that aren't supposed to collide with the low-LOD chassis from doing so.
		if(pHitEntity && pHitEntity->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = static_cast<CVehicle *>(pHitEntity);	
			if(pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_ALLOW_OBJECT_LOW_LOD_COLLISION ))
			{
				if(tuneEntry && tuneEntry->GetCollidesWithLowLodVehicleChassis())
				{
					if(impacts.GetOtherComponent() == pVehicle->GetFragmentComponentIndex(VEH_CHASSIS) && pVehicle->GetFragmentComponentIndex(VEH_CHASSIS_LOWLOD) != -1)
					{
						impacts.DisableImpact();
						impacts.NextActiveContact();
						continue;
					}					
				}
				else
				{
					if(impacts.GetOtherComponent() == pVehicle->GetFragmentComponentIndex(VEH_CHASSIS_LOWLOD) && pVehicle->GetFragmentComponentIndex(VEH_CHASSIS) != -1)
					{
						impacts.DisableImpact();
						impacts.NextActiveContact();
						continue;
					}
				}
			}
		}	

		if(needsToIterate)
		{
			//////////////////////////////////////////////////////////////////////////
						

			// Ground physical setting for CObject's is now disabled so that we can avoid iterating the contacts at all unless one of three very uncommon flags is set (For performance reasons)
			// Note that prior to this change CObject::ProcessPreComputeImpacts wasn't being called by fragInstGta's anyway so the only difference will be to phInstGta's
			// But those probably don't actually do anything different anyway because they only actually use the ground instance to affect sleep properties in CObject::ProcessPhysics
			//  and that doesn't allow sleep on vehicles anyway (which would have been pretty much the only valid case of putting an object to sleep on another active one)
			// Bottom line - There does not appear to be a reason to store the ground instance for a CObject at this time (Using it for sleep would require an attachment system like what peds do)
			////Check that the normal is generally pointing up.
			//Vector3 vNormal;
			//impacts.GetMyNormal(vNormal);
			//if(vNormal.z >= 0.5f)
			//{
			//	//Check that this object is generally above the other entity.
			//	if(impacts.GetMyCollider() && (impacts.GetMyCollider()->GetPosition().GetZf() > VEC3V_TO_VECTOR3(impacts.GetOtherPosition()).z))
			//	{
			//		//Ensure the other object is physical.
			//		CEntity* pEntity = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());
			//		if(pEntity && pEntity->GetIsPhysical())
			//		{
			//			//Assign the ground.
			//			m_pGroundPhysical = static_cast<CPhysical *>(pEntity);
			//		}
			//	}
			//}

			// Certain objects (usually scenario entities being used by peds) have fixed physics unless the collision has high impulse.
			TUNE_GROUP_FLOAT(OBJECT_PHYSICS, fMinMomentumToDisableFixedPhysicsOnProps, 150.0f, 0.0f, 10000.0f, 0.1f);
			
			if (m_nObjectFlags.bFixedForSmallCollisions && pOtherInstance)
			{
				if (pHitEntity)
				{
					bool bTurnOffFixedPhysics = false;

					if (pHitEntity->GetIsTypeVehicle())
					{
						bTurnOffFixedPhysics = true;
					}
					else if (pHitEntity->GetIsTypeObject())
					{
						CObject* pObject = static_cast<CObject*>(pHitEntity);
						Vec3V vCollisionNormal;
						impacts.GetMyNormal(vCollisionNormal);

						ScalarV vMomentum = Dot(vCollisionNormal, VECTOR3_TO_VEC3V(pObject->GetVelocity())) * ScalarV(pObject->GetMass());

						if (IsGreaterThanAll(vMomentum, ScalarV(fMinMomentumToDisableFixedPhysicsOnProps)))
						{
							bTurnOffFixedPhysics = true;
						}
					}
					// Was hit by an entity.
					if (bTurnOffFixedPhysics)
					{
						// Was hit by a heavy object or vehicle.
						// Turn off this object's fixed physics.
						SetFixedPhysics(false);
					}
				}
			}

			// If we're kinematic, treat all collisions as if we have infinite mass
			if (IsUsingKinematicPhysics())
			{
				bool bSetMassScales = true;

				CEntity* pEnt = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());
				if (pEnt && pEnt->GetIsTypeVehicle() && pEnt->GetCollider() && ((CVehicle*)pEnt)->GetMass()>0.0f)
				{
					// optionally allow contacts above a certain threshold from vehicle hits,
					// and break the entity out of the synced scene	when these occur		
					Vec3V otherVel = pEnt->GetCollider()->GetLocalVelocity(impacts.GetOtherPosition().GetIntrin128());
					Vec3V myNormal;
					impacts.GetMyNormal(myNormal);
					float massRatio = ((CVehicle*)pEnt)->GetMass()/GetMass();

					TUNE_GROUP_FLOAT(VEHICLE_SYNCED_SCENE, fMinVehMomentumForObjectSyncedSceneAbort, 4.0f, 0.0f, 10000.0f, 0.01f);
					if ((Dot(otherVel, myNormal).Getf() * massRatio)>fMinVehMomentumForObjectSyncedSceneAbort)
					{
						// find the cutscene task and notify it to exit
						CTaskSynchronizedScene* pTask = GetObjectIntelligence() ? static_cast< CTaskSynchronizedScene* >(GetObjectIntelligence()->FindTaskByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE)) : NULL;

						if (pTask && pTask->IsSceneFlagSet(SYNCED_SCENE_VEHICLE_ABORT_ON_LARGE_IMPACT))
						{
							bSetMassScales = false;
							pTask->ExitSceneNextUpdate();
							SetShouldUseKinematicPhysics(false);
							DisableKinematicPhysics();
						}
					}
				}

				if (bSetMassScales)
				{
					impacts.SetMassInvScales(0.0f,1.0f);
#if PDR_ENABLED
					if (impacts.GetCachedManifold().GetInstanceA() == pOtherInstance)
					{
						PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "InfiniteMass(kinematic[A])", 0.0f));
					}
					else
					{
						PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "InfiniteMass(kinematic[B])", 0.0f));
					}
#endif // PDR_ENABLED
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Bit of a hack to try to deal with Wil-E-Coyote issues that are holding golf balls up as they roll directly over the holes
			if(Unlikely(m_nDEflags.bIsGolfBall))
			{
				// Ignore and remove all old contacts
				impacts.GetMyInstance()->GetArchetype()->GetBound()->EnableUseNewBackFaceCull();
				if(impacts.GetContact().GetLifetime() > 1)
				{
					PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "Golf - Old Contact"));
					phInst* pOtherInstance = impacts.GetOtherInstance();
					CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);
					AddCollisionRecordBeforeDisablingContact(GetCurrentPhysicsInst(), pOtherEntity, impacts);
					//
					impacts.DisableImpact();
					impacts.NextActiveContact();
					continue;
				}
				else
				{
					// This hack is targeted at putting - So we're going to avoid doing any of this if we're moving too fast
					ScalarV ballSpeedSqrd(V_ZERO);
					const phInst* golfBall = impacts.GetMyInstance();
					Assert(golfBall != NULL);
					const phCollider* ballCollider = PHSIM->GetCollider(golfBall->GetLevelIndex());
					if(ballCollider != NULL)
					{
						ballSpeedSqrd = MagSquared(ballCollider->GetVelocity());
					}
					//
					const ScalarV maxSpeedThresholdSqrd(64.0f);
					if(IsLessThanAll(ballSpeedSqrd, maxSpeedThresholdSqrd) != 0)
					{
						// And again, because this is for putting - we're going to straight up ignore doing anything to contacts that aren't basically vertical
						Vec3V curNormal;
						impacts.GetMyNormal(curNormal);
						const ScalarV normVerticalThresh(0.8f);
						if(IsGreaterThanAll(Dot(curNormal, Vec3V(V_UP_AXIS_WZERO)), normVerticalThresh) != 0)
						{
							// We're going to test if the ball center is directly over the triangle involved in this impact and mess with it if not
							const phArchetype* otherArch = impacts.GetOtherInstance()->GetArchetype(); Assert(otherArch != NULL);
							const phBound* otherBound = otherArch->GetBound(); Assert(otherBound != NULL);
							// Ignore non-bvh because that should be the only thing we roll on
							if(otherBound->GetType() == phBound::BVH)
							{
								const phBoundBVH* otherBoundBvh = static_cast<const phBoundBVH*>(otherBound);
								// Ignore non-triangles
								const int otherElem = impacts.GetOtherElement();
								const phPrimitive& otherPrim = otherBoundBvh->GetPrimitive(otherElem);
								if(otherPrim.GetType() == PRIM_TYPE_POLYGON)
								{
									const phPolygon& otherPoly = reinterpret_cast<const phPolygon&>(otherPrim);

									const int vertIndex0 = otherPoly.GetVertexIndex(0);
									const int vertIndex1 = otherPoly.GetVertexIndex(1);
									const int vertIndex2 = otherPoly.GetVertexIndex(2);
									Assert(vertIndex0 < otherBoundBvh->GetNumVertices());
									Assert(vertIndex1 < otherBoundBvh->GetNumVertices());
									Assert(vertIndex2 < otherBoundBvh->GetNumVertices());
									const Vec3V triVert0 = otherBoundBvh->GetBVHVertex(vertIndex0);
									const Vec3V triVert1 = otherBoundBvh->GetBVHVertex(vertIndex1);
									const Vec3V triVert2 = otherBoundBvh->GetBVHVertex(vertIndex2);

									const Vec3V triNorm = Normalize(phPolygon::ComputeNonUnitNormal(triVert0, triVert1, triVert2)); // Need to use this so the handedness will result in the same normal direction that art set up

									//////////////////////////////////////////////////////////////////////////
									// Additional side hack: Any edge contacts get their normals set to the edge normal (We just cheap out even more and use the current face normal)
									// - This is an attempt to deal with apparent pops that occur when the ball rolls into a slightly convex edge
									////const ScalarV ballContactOnPolyEdgeFactorSqrd(1.02f);
									////const ScalarV marginSqrd = Scale(otherBoundBvh->GetMarginV(), otherBoundBvh->GetMarginV());
									////if( GetPointOnAnyEdge(triVert0, triVert1, triVert2, impacts.GetOtherPosition(), Scale(marginSqrd, ballContactOnPolyEdgeFactorSqrd)) )
									////{
									////	impacts.SetMyNormal(triNorm);
									////}
									//////////////////////////////////////////////////////////////////////////

									if(DoGolfImpactsPuttingHack(impacts, otherBoundBvh, otherPoly, triVert0, triVert1, triVert2, triNorm, (const rage::phPolygon::Index)(otherElem), 0))
									{
										PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "Golf - Old Contact"));
										phInst* pOtherInstance = impacts.GetOtherInstance();
										CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);
										AddCollisionRecordBeforeDisablingContact(GetCurrentPhysicsInst(), pOtherEntity, impacts);
										//
										impacts.DisableImpact();
										impacts.NextActiveContact();
										continue;
									}
								}
							}
						}
					}
				}
			}

			if(Unlikely(IsAttachedToHandler()))
			{
				if(pOtherInstance && pOtherInstance->GetUserData() && CPhysics::GetEntityFromInst(pOtherInstance)->GetIsTypePed())
				{
					const float kfContainerMassScale = 0.0333f;
					impacts.SetMassInvScales(1.0f, kfContainerMassScale);
				}

				// B*1875926: Prevent crates that are attached to the handler from pushing other crates (or other heavy objects) around and causing problems.
				if(pOtherInstance && CPhysics::GetEntityFromInst(pOtherInstance))
				{
					CEntity *pEntity = CPhysics::GetEntityFromInst(pOtherInstance);
					if(pEntity->GetIsTypeObject() && pEntity->GetPhysArch() && pEntity->GetPhysArch()->GetMass() >= 15000)
						impacts.SetMassInvScales(1.0f, 0.0f);
				}
			}

			//////////////////////////////////////////////////////////////////////////

			if( pOtherInstance &&
				m_bIsParachute &&
				ms_bDisableCarCollisionsWithCarParachute )
			{
				CEntity*	pEntity = CPhysics::GetEntityFromInst( pOtherInstance );
				CVehicle*	pHitVeh = NULL;

				if( pEntity &&
					pEntity->GetIsTypeVehicle() )
				{
					pHitVeh = static_cast<CVehicle*>(pEntity);
				}

				if( pHitVeh )
				{
					// Disable the impact
					impacts.DisableImpact();
				}
				else if( pEntity->GetIsTypeObject() &&
						 static_cast< CObject* >( pEntity )->GetIsParachute() )
				{
					// Disable the impact
					impacts.DisableImpact();
				}
				
			}

			if (GetDisableObjectMapCollision() ||
				 m_bIsArticulatedProp )
			{
				phInst* pOtherInst = impacts.GetOtherInstance();
				if ( pOtherInst && pOtherInst->IsInLevel())
				{
					phLevelNew* pLevel = CPhysics::GetLevel();
					u16 nLevelIndex = pOtherInst->GetLevelIndex();

					if ((pLevel->GetInstanceTypeFlags(nLevelIndex)&ArchetypeFlags::GTA_MAP_TYPE_MOVER)!=0)
					{
						// Disable the impact
						impacts.DisableImpact();
						PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "Disabled Map Collision"));
					}
					else
					{
						if (pOtherInst->GetUserData())
						{
							CEntity* pEnt = (CEntity*)pOtherInst->GetUserData();
							if (pEnt)
							{
								switch (pEnt->GetType())
								{
									case ENTITY_TYPE_PED:
									{
										CPed* pPed = static_cast<CPed*>(pEnt);
										if (pPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleMapCollision))
										{
											// Disable the impact
											impacts.DisableImpact();
											PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "Anim Disabled Collision"));
										}
									}
									break;

									case ENTITY_TYPE_OBJECT:
									{
										CObject* pObj = static_cast<CObject*>(pEnt);
										if (pObj->GetDisableObjectMapCollision())
										{
											// Disable the impact
											impacts.DisableImpact();
											PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "Anim Disabled Collision"));
										}
										else if( m_bIsArticulatedProp &&
												 pObj->GetIsFixedFlagSet() )
										{
											impacts.DisableImpact();
										}
									}
									break;

									case ENTITY_TYPE_MASK_VEHICLE:
									{
										CVehicle* pVeh = static_cast<CVehicle*>(pEnt);
										if (pVeh->m_nVehicleFlags.bDisableVehicleMapCollision)
										{
											// Disable the impact
											impacts.DisableImpact();
											PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "Anim Disabled Collision"));
										}
									}
									break;

									default:
									{
										// Do nothing
									}
									break;
								}
							}
						}
					}
				}
			}

			if(isAvoidingCollision && !impacts.IsDisabled())
			{
				// If this object just streamed in and gets contacts on the first frame, disable them until we're clear.
				CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInstance);
				if(pOtherEntity && pOtherEntity->GetIsPhysical() && ShouldAvoidCollision(static_cast<const CPhysical*>(pOtherEntity)))
				{
					impacts.DisableImpact();
				}
			}

			//////////////////////////////////////////////////////////////////////////

			if(m_nObjectFlags.bVehiclePart && m_nObjectFlags.bCarWheel)
			{
				if(m_pFragParent && m_pFragParent->GetIsTypeVehicle() && ((CVehicle*)m_pFragParent.Get())->InheritsFromPlane())
				{
					impacts.SetElasticity(0.0f);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			
			// B*1760493: Force the TACO van roof sign to have a greater friction with the ground so it cannot be pushed around by peds.
			if(GetModelIndex() == MI_CAR_TACO)
			{
				// Only adjust for entities of type BUILDING.
				if(CPhysics::GetEntityFromInst(pOtherInstance) && CPhysics::GetEntityFromInst(pOtherInstance)->GetIsTypeBuilding())
				{
					// Only set friction for contacts with something underneath the sign.
					if(impacts.GetMyCollider() && (impacts.GetMyCollider()->GetPosition().GetZf() > VEC3V_TO_VECTOR3(impacts.GetOtherPosition()).z))
						impacts.SetFriction(100.0f);	
				}
			}	
		}

		if( !m_bIsPressurePlate &&
			( m_bDriveToMaxAngle ||
			  m_bDriveToMinAngle ) )
		{
			if( CEntity* pOtherEntity = CPhysics::GetEntityFromInst( pOtherInstance ) )
			{
				if( pOtherEntity->GetIsTypeVehicle() )
				{
					static_cast<CVehicle*>( pOtherEntity )->m_nVehicleFlags.bWakeUpNextUpdate = true;
				}
			}

			impacts.SetMassInvScales( 0.0f, 1.0f );
		}
		if( m_bIsPressurePlate )
		{
			impacts.SetMassInvScales( 1.0f, 0.0f );
			m_bDriveToMinAngle = true;
		}

		//Move to the next impact.
		impacts.NextActiveContact();
	}	
}

void CObject::ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const* hitInst, const Vector3& vMyHitPos, const Vector3& vOtherHitPos,
							   float fImpulseMag, const Vector3& vMyNormal, int iMyComponent, int iOtherComponent,
							   phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact)
{
	if (!GetCreature() && !m_nObjectFlags.bVehiclePart)
	{
		// This models expression dictionary should be setup as a dependency
		fwArchetype* pArchetype = fwArchetypeManager::GetArchetype(GetModelIndex());
		animAssert(pArchetype);
		if (pArchetype)
		{
			strLocalIndex iExpressionDictionaryIndex = strLocalIndex(pArchetype->GetExpressionDictionaryIndex());

			// Is the current slot valid?
			if (g_ExpressionDictionaryStore.IsValidSlot(iExpressionDictionaryIndex))
			{
				// Is the expression dictionary in the image?
				if (g_ExpressionDictionaryStore.IsObjectInImage(iExpressionDictionaryIndex))
				{
					// Is the expression dictionary streamed in?
					if (g_ExpressionDictionaryStore.HasObjectLoaded(iExpressionDictionaryIndex))
					{
						strLocalIndex iExpressionHash = strLocalIndex(pArchetype->GetExpressionHash().GetHash());
						const crExpressions* pExpressions = g_ExpressionDictionaryStore.Get(iExpressionDictionaryIndex)->Lookup(iExpressionHash.Get());
						if (pExpressions)
						{
							// Create the skeleton, anim director etc
							InitAnimLazy();

							// Set local pose only for the inverse pose node
							crCreatureComponentSkeleton* skeleton = GetCreature()->FindComponent<crCreatureComponentSkeleton>();
							if(skeleton)
							{
								skeleton->SetSuppressReset(true);
							}
						}
					}
				}
			}
		} // if (pArchetype)
	}

	//----

	// we're playing multiplayer...
	if(NetworkInterface::IsGameInProgress())
	{
		CBaseModelInfo const* pModelInfo = GetBaseModelInfo();

		// and shot an object with glass in it....
		if(pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0)
		{
			// Projectiles like missiles and grenades are created locally on all machines 
			// (CStartProjectileEvents are used to start them) so the missle / grenade entity 
			// will always be local - so if we get a projectile hitting glass we need to test 
			// if the owner (person who fired it) is local / clone...
			bool ownerNotClone = true;

			if(pHitEnt && pHitEnt->GetIsTypeObject())
			{
				if(((CObject*)pHitEnt)->GetAsProjectile())
				{
					CEntity* owner = ((CProjectile*)pHitEnt)->GetOwner();

					if(owner && owner->GetIsDynamic())
					{
						CDynamicEntity* deEnt = static_cast<CDynamicEntity*>(owner);
						if(deEnt->IsNetworkClone())
						{
							ownerNotClone = false;
						}
					}
				}
			}

			// if the object is local and our projectile catch flag isn't a clone...
			if(!NetworkUtils::IsNetworkClone(pHitEnt) && ownerNotClone)
			{	
				// so we're taking control - it's up to us to flag how it gets damaged to the other machines so we registerWithNetwork
				CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkOwner( this, vMyHitPos, (u8)iMyComponent );
			}
		}
	}

	if ( pHitEnt )
	{
		// Next, was this collision involving the player? [11/26/2012 mdawe]
		CPed* pPed = NULL;
		if (pHitEnt->GetIsTypePed())
		{
			pPed = static_cast<CPed*>(pHitEnt);
		} 
		else if (pHitEnt->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pHitEnt);
			pPed = pVehicle->GetDriver();
		}

		if (pPed && pPed->IsPlayer())
		{
			//Work out the relative velocity of the entities involved in the collision
			Vector3 moveSpeed(0.0f, 0.0f, 0.0f);
			Vector3 otherMoveSpeed(0.0f, 0.0f, 0.0f);
			f32 relativeMoveSpeed = 0.0f;

			if(	GetIsPhysical() && pHitEnt->GetIsPhysical() )
			{
				moveSpeed					= static_cast<CPhysical *>(this)->GetVelocity();
				otherMoveSpeed		= static_cast<CPhysical *>(pHitEnt)->GetVelocity();
				relativeMoveSpeed = (moveSpeed - otherMoveSpeed).Mag2();
			}
			
			//Check if we are active.
			phInst* pInst = GetCurrentPhysicsInst();
			if(pInst && pInst->IsInLevel() && CPhysics::GetLevel()->IsActive(pInst->GetLevelIndex()))
			{
				//Check if the relative move speed is enough for the agitated event.
				if(relativeMoveSpeed > ms_fMinRelativeVelocitySquaredForCollisionAgitatedEvent)
				{
					//Check if the mass is enough for the agitated event.
					float fMass = GetMass();
					if(fMass > ms_fMinMassForCollisionAgitatedEvent)
					{
						//Check if the start timer is valid.
						if(ms_uStartTimerForCollisionAgitatedEvent != 0)
						{
							//Check if we are within the ignore time.
							static dev_u32 s_uIgnoreTime = 2000;
							if((ms_uStartTimerForCollisionAgitatedEvent + s_uIgnoreTime) > fwTimer::GetTimeInMilliseconds())
							{
								//Nothing to do.
							}
							else
							{
								//Check if we are within the window.
								static dev_u32 s_uWindow = 8000;
								if((ms_uStartTimerForCollisionAgitatedEvent + s_uIgnoreTime + s_uWindow) > fwTimer::GetTimeInMilliseconds())
								{
									//Create the event.
									CEventAgitated event(pPed, AT_Griefing);
									event.SetMaxDistance(3.0f);
									GetEventGlobalGroup()->Add(event);

									//Clear the start timer.
									ms_uStartTimerForCollisionAgitatedEvent = 0;
								}
								else
								{
									//Start the timer.
									ms_uStartTimerForCollisionAgitatedEvent = fwTimer::GetTimeInMilliseconds();
								}
							}
						}
						else
						{
							//Start the timer.
							ms_uStartTimerForCollisionAgitatedEvent = fwTimer::GetTimeInMilliseconds();
						}
					}
				}
			}
		}

		// Parachute collision
		if(m_bIsParachute && GetIsAttached() )
		{
			CVehicle* pHitVeh = NULL;
			CObject* pHitObj = NULL;

			// If hit a ped, check if the ped is in a car that's potentially attached to the vehicle
			if (pHitEnt->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pHitEnt);

				if(pPed->GetIsInVehicle())
				{
					pHitVeh = pPed->GetMyVehicle();
				}
			} 
			// If hit a vehicle, store the vehicle to check if it's what the parachute is attached to
			else if (pHitEnt->GetIsTypeVehicle())
			{
				pHitVeh = static_cast<CVehicle*>(pHitEnt);
			}
			else if( pHitEnt->GetIsTypeObject() )
			{
				pHitObj = static_cast< CObject* >( pHitEnt );
			}

			CEntity* pEntity = static_cast<CEntity*>(GetAttachParent());

			// If attached to a vehicle
			if( pEntity->GetIsTypeVehicle() )
			{
				CVehicle* pAttachVehicle = static_cast<CVehicle*>(pEntity);

				// If the vehicle has a parachute
				if(pAttachVehicle->InheritsFromAutomobile() && pAttachVehicle->HasParachute() )
				{
					bool ignoreImpact = ms_bDisableCarCollisionsWithCarParachute && 
										( pHitVeh ||
										  ( pHitObj && 
											( pHitObj->GetIsParachute() ||
											  pHitObj->IsNetworkClone() ) ) );

					if( !ignoreImpact )
					{
						CAutomobile* pAttachCar = static_cast<CAutomobile*>(pAttachVehicle);

						// If the vehicle (or ped inside the vehicle) we've hit is not the one we're attached to
						// And the parachute is deployed, abort parachuting
						if(pAttachVehicle != pHitVeh && pAttachCar->IsParachuteDeployed() )
						{
							pAttachCar->RequestFinishParachuting();

							vehicleDisplayf("CObject::ProcessCollision - RequestFinishParachuting: Car Collisions with parachute disabled[%d] Name[%s] Hit entity name[%s] Network Name[%s]",
								(int)CObject::ms_bDisableCarCollisionsWithCarParachute,
								pAttachVehicle->GetDebugName(),
								pHitVeh ? pHitVeh->GetDebugName() : pHitObj ? pHitObj->GetDebugName() : "pHitVeh and pHitObj are null!",
								GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown" );
						}
					}
				}
			}
		}
	}

	if( m_bIsBall )
	{
		ProcessBallCollision( pHitEnt, vMyHitPos, fImpulseMag, vMyNormal );
	}

	CPhysical::ProcessCollision(myInst, pHitEnt, hitInst, vMyHitPos, vOtherHitPos, fImpulseMag, vMyNormal, iMyComponent, iOtherComponent,
		iOtherMaterial, bIsPositiveDepth, bIsNewContact);

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() == false)
	{
		// During normal gameplay this object has been activated so let the replay
		// know so it can hide the equivalent on playback
		CReplayMgr::RecordMapObject(this);
	}
#endif // GTA_REPLAY
}

void CObject::ProcessControlLogic()
{
}


//
// name:		PreRender
ePrerenderStatus CObject::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	TUNE_GROUP_BOOL( ARENA_DYNAMIC_PROPS, sb_EnableArticulatedPropTest, false );
	m_bBallShunted = false;
	m_bExtraBounceApplied = false;

	if( sb_EnableArticulatedPropTest )
	{
		TUNE_GROUP_BOOL( ARENA_DYNAMIC_PROPS, toggleTargetAngle, false );
		//TUNE_GROUP_BOOL( ARENA_DYNAMIC_PROPS, snapTargetAngle, false );
		TUNE_GROUP_BOOL( ARENA_DYNAMIC_PROPS, testPressurePlate, false );

		TUNE_GROUP_BOOL( ARENA_DYNAMIC_PROPS, isPressurePlatePressed, false );
		TUNE_GROUP_BOOL( ARENA_DYNAMIC_PROPS, isJointAtMinAngle, false );
		TUNE_GROUP_BOOL( ARENA_DYNAMIC_PROPS, isBall, false );

#if __BANK
		isPressurePlatePressed = IsPressurePlatePressed();
		isJointAtMinAngle = IsJointAtMinAngle( 0 );
		m_bIsBall = isBall;
#endif

		TUNE_GROUP_INT( ARENA_DYNAMIC_PROPS, toggleJointIndex, -1, -1, 10, 1  );
		static u32 snTimeToggled = fwTimer::GetFrameCount();
		//static u32 snTimeSnapped = fwTimer::GetFrameCount();
		static bool sbPreviousToggleTarget = false;
		//static bool sbPreviousSnapAngle = false;

		m_bIsPressurePlate = testPressurePlate;

		if( toggleTargetAngle != sbPreviousToggleTarget )
		{
			snTimeToggled = fwTimer::GetFrameCount();
			sbPreviousToggleTarget = toggleTargetAngle;
		}
		//if( snapTargetAngle != sbPreviousSnapAngle )
		//{
		//	snTimeSnapped = fwTimer::GetFrameCount();
		//	SnapJointToTargetRatio( 0, snapTargetAngle ? 1.0f : 0.0f );
		//}

		if( snTimeToggled == fwTimer::GetFrameCount() )
		{
			SetArticulatedJointTargetAngle( this, toggleTargetAngle, toggleJointIndex );
		}

		if( GetModelNameHash() == ATSTRINGHASH( "xs_prop_arena_bomb_m", 0x153F040D ) ||
			GetModelNameHash() == ATSTRINGHASH( "xs_prop_arena_bomb_s", 0xBF77D87C ) ||
			GetModelNameHash() == ATSTRINGHASH( "xs_prop_arena_bomb_l", 0x23A8A0E0 ) )
		{
			m_bIsBall = true;
		}
	}


#if NORTH_CLOTHS
	if (m_pCloth)
	{
		ePrerenderStatus status = PreRenderUpdateClothSkeleton();
		if (status == PRERENDER_DONE)
		{
			PreRender2();
		}

		return status;
	}
#endif // NORTH_CLOTHS

#if __BANK
#if __DEV
	if(CDebugScene::bDisplayLadderDebug)
	{
		Vector3 vX(0.5f,0.0f,0.0f);
		Vector3 vY(0.0f,0.5f,0.0f);
		CBaseModelInfo * pModelInf = GetBaseModelInfo();

		if(pModelInf->GetIsLadder())
		{
			int iNumEffects = pModelInf->GetNum2dEffectsNoLights();
			for(int c=0; c<iNumEffects; c++)
			{
				C2dEffect* pEffect = pModelInf->Get2dEffect(c);
				if(pEffect->GetType() == ET_LADDER)
				{
					CLadderInfo* ldr = pEffect->GetLadder();

					Vector3 vPosAtTop, vPosAtBottom, vNormal;
					ldr->vTop.Get(vPosAtTop);
					ldr->vBottom.Get(vPosAtBottom);
					ldr->vNormal.Get(vNormal);

					const fwTransform& t = GetTransform();

					vPosAtTop = VEC3V_TO_VECTOR3(t.Transform(RC_VEC3V(vPosAtTop)));
					vPosAtBottom = VEC3V_TO_VECTOR3(t.Transform(RC_VEC3V(vPosAtBottom)));
					vNormal = VEC3V_TO_VECTOR3(t.Transform(RC_VEC3V(vNormal)));
					vNormal.z = 0.0f;

					if(vPosAtTop.z<=vPosAtBottom.z)
					{
						grcDebugDraw::Text(vPosAtTop, Color_green, "Ladder UpsideDown");
					}

					Vector3 vMiddle = (vPosAtTop + vPosAtBottom) * 0.5f;

					grcDebugDraw::Line(vPosAtTop - vX, vPosAtTop + vX, Color_green);
					grcDebugDraw::Line(vPosAtTop - vY, vPosAtTop + vY, Color_green);
					grcDebugDraw::Line(vPosAtBottom - vX, vPosAtBottom + vX, Color_red);
					grcDebugDraw::Line(vPosAtBottom - vY, vPosAtBottom + vY, Color_red);
					grcDebugDraw::Line(vMiddle, vMiddle + vNormal, Color_DarkOrange);

					while(vPosAtBottom.z<vPosAtTop.z)
					{
						grcDebugDraw::Line(vPosAtBottom - vX*0.5f, vPosAtBottom + vX*0.5f, Color_green4);
						grcDebugDraw::Line(vPosAtBottom - vY*0.5f, vPosAtBottom + vY*0.5f, Color_green4);
						vPosAtBottom.z+=0.29f;
					}
				}
			}
		}
	}
#endif // __DEV
#endif // __BANK
	// Must have a valid skeleton?
	if (GetSkeleton())
	{
		// Handle objects marked as being clocks
		CBaseModelInfo* pModelInfo = GetBaseModelInfo();
		if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_CLOCK))
		{
			int index;
			// Update the hour hand (if present)?
			if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_HOUR_HAND, index))
			{
				// Convert from 24 hour time to 12 hour time
				float hours = (float)CClock::GetHour();
				if (hours > 12.0f)
				{
					hours = hours - 12.0f;
				}

				// Convert from minutes to hours
				float minutes = (float)CClock::GetMinute();
				float partialHour = minutes/60.0f;
				hours = hours + partialHour;

				// Convert from hours to angle
				float phase = hours/12.0f;
				float angle = phase * 2.0f * PI;
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateY(angle);
			} 
			// Update the minute hand (if present)?
			if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_MINUTE_HAND, index))
			{
				float minutes = (float)CClock::GetMinute();

				// Convert from seconds to hours
				float seconds = (float)CClock::GetSecond();
				float partialMinute = seconds/60.0f;
				minutes = minutes + partialMinute;

				// Convert from minutes to angle
				float phase = minutes/60.0f;
				float angle = phase * 2.0f * PI;
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateY(angle);
			}
			// Update the second hand (if present)?
			if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_SECOND_HAND, index))
			{
				float seconds = (float)CClock::GetSecond();

				// Convert from seconds to angle
				float phase = seconds/60.0f;
				float angle = phase * 2.0f * PI;
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateY(angle);
			}
		}
		
		// Handle objects marked as having simple one axis procedural animation
		if (pModelInfo->TestAttribute(MODEL_SINGLE_AXIS_ROTATION))
		{
			static dev_float slow_rate = 0.005f;
			static dev_float fast_rate = 0.03f;
			int index = -1;

			if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_X_AXIS_SLOW, index))
			{
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateX(fmodf(fwTimer::GetSystemTimeInMilliseconds() * slow_rate, TWO_PI));
			}
			else if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_Y_AXIS_SLOW, index))
			{
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateY(fmodf(fwTimer::GetSystemTimeInMilliseconds() * slow_rate, TWO_PI));
			}
			else if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_Z_AXIS_SLOW, index))
			{
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateZ(fmodf(fwTimer::GetSystemTimeInMilliseconds() * slow_rate, TWO_PI));
			}
			else if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_X_AXIS_FAST, index))
			{
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateX(fmodf(fwTimer::GetSystemTimeInMilliseconds() * fast_rate, TWO_PI));
			}
			else if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_Y_AXIS_FAST, index))
			{
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateY(fmodf(fwTimer::GetSystemTimeInMilliseconds() * fast_rate, TWO_PI));
			}
			else if (GetSkeletonData().ConvertBoneIdToIndex(BONETAG_Z_AXIS_FAST, index))
			{
				Matrix34 &objectBoneMat = GetObjectMtxNonConst(index);
				objectBoneMat.MakeRotateZ(fmodf(fwTimer::GetSystemTimeInMilliseconds() * fast_rate, TWO_PI));
			}
		}

	}
	//Set the fade to zero data based on the fade out flag.
	GetLodData().SetFadeToZero(m_nObjectFlags.bFadeOut);

	CEntity *pParent = (CEntity*)GetAttachParent();
	if( pParent )
	{
		// Update add to motion blur mask to
		m_nFlags.bAddtoMotionBlurMask = pParent->m_nFlags.bAddtoMotionBlurMask;

		if( GetOwnedBy()==ENTITY_OWNEDBY_SCRIPT && pParent->GetIsTypeVehicle() )
		{
			// script objects inside vehicles - to be marked as vehicles:
			SetBaseDeferredType(DEFERRED_MATERIAL_VEHICLE);
		}
	}

	// if this object broke off from a vehicle, need to turn off any extras that were dissabled on that vehicle
	if(m_pFragParent && m_pFragParent->GetIsTypeVehicle())
	{
		CVehicle* pVeh = static_cast<CVehicle*>(m_pFragParent.Get());
		if(pVeh->m_nDisableExtras && GetSkeleton())
		{
			for(int i=VEH_EXTRA_1; i<=VEH_LAST_EXTRA; i++)
			{
				if(pVeh->m_nDisableExtras &BIT(i - VEH_EXTRA_1 + 1))
				{
					// planes don't have all extras
					Assert(pVeh->GetBoneIndex(eHierarchyId(i))!=-1 || pVeh->InheritsFromPlane());
					if (pVeh->GetBoneIndex(eHierarchyId(i)) != -1)
						GetLocalMtxNonConst(pVeh->GetBoneIndex(eHierarchyId(i))).Zero();
				}
			}
		}
	}

#if STENCIL_VEHICLE_INTERIOR
	SetUseVehicleInteriorMaterial(CVfxHelper::IsEntityInCurrentVehicle(this));
#endif // STENCIL_VEHICLE_INTERIOR

	ePrerenderStatus status = CPhysical::PreRender(bIsVisibleInMainViewport);

#if GTA_REPLAY
	if( CReplayMgr::IsEditModeActive() )
	{
		return PRERENDER_DONE;
	}
#endif

	if (AreAnyHiddenFlagsSet())
	{
		status = PRERENDER_NEED_SECOND_PASS;
	}

	fwAnimDirector* pAnimDirector = GetAnimDirector();
	if (pAnimDirector && (!GetCanUseCachedVisibilityThisFrame() || bIsVisibleInMainViewport))
	{
		// Undo any refreshing on the main skeleton animation might have done. 
		// NOTE: Maybe move this to PreRender2?
		if(const fragInst* pFragInst = GetFragInst())
		{
			if(fragCacheEntry* pEntry = pFragInst->GetCacheEntry())
			{
				if(pEntry->GetHierInst()->anyGroupDamaged)
				{
					GetFragInst()->ZeroSkeletonMatricesByGroup(pEntry->GetHierInst()->skeleton,pEntry->GetHierInst()->damagedSkeleton);
				}
			}
		}

		fwAnimDirectorComponentExpressions* componentExpressions = pAnimDirector->GetComponent<fwAnimDirectorComponentExpressions>();
		if (componentExpressions && componentExpressions->HasExpressions())
		{
			pAnimDirector->StartUpdatePreRender(fwTimer::GetTimeStep());
			status = PRERENDER_NEED_SECOND_PASS;
		}
	}

	//Turn off animation on damaged objects
	if(pAnimDirector && m_nFlags.bRenderDamaged)
	{
		RemoveFromSceneUpdate();
	}

	return status;
}

void CObject::PreRender2(const bool bIsVisibleInMainViewport)
{
	fwAnimDirector* pAnimDirector = GetAnimDirector();
	if (pAnimDirector && (!GetCanUseCachedVisibilityThisFrame() || bIsVisibleInMainViewport))
	{
		fwAnimDirectorComponentExpressions* componentExpressions = pAnimDirector->GetComponent<fwAnimDirectorComponentExpressions>();
		if (componentExpressions && componentExpressions->HasExpressions())
		{
			pAnimDirector->WaitForPreRenderUpdateToComplete();
		}
	}

	if (AreAnyHiddenFlagsSet())
	{
		CVehicleGlassManager::PreRender2(this); // required to force any smashed component matrices to zero after they've been animated
	}
}

#if __BANK
void CObject::DebugProcessAttachment(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine)
#else
void CObject::ProcessAttachment()
#endif
{
	// reset forced bound radius, may get set lower down
	SetForceAddToBoundRadius(0.0f);

#if __BANK
	CPhysical::DebugProcessAttachment(strCodeFile, strCodeFunction, nCodeLine);
#else
	CPhysical::ProcessAttachment();
#endif

}

// set flags for this object & add to process control list (if necessary) so that
// it is deleted at the next possible safe opportunity
void CObject::FlagToDestroyWhenNextProcessed()
{
#if !__NO_OUTPUT
	if(m_LogDeletions)
	{
		EntityDebugfWithCallStack(this, "FlagToDestroyWhenNextProcessed"); 
	}
#endif

#if GTA_REPLAY
	replayFatalAssertf(CReplayMgr::IsOkayToDelete() || GetOwnedBy() != ENTITY_OWNEDBY_REPLAY, "Replay owned Entity being deleted during playback...but NOT by the replay system! - this is very bad");
#endif
#if __DEV
	// check if a procedural object is being deleted when it shouldn't be
	if (m_nFlags.bIsProcObject)
	{
		Assertf(g_procObjMan.IsAllowedToDelete(), "Procedural Object (Material) being FlagToDestroyWhenNextProcessed by non procedural object code");
	}
	if (m_nFlags.bIsEntityProcObject)
	{
		Assertf(g_procObjMan.IsAllowedToDelete(), "Procedural Object (Entity) being FlagToDestroyWhenNextProcessed by non procedural object code");
	}
	if (m_nObjectFlags.bIsPickUp)
	{
		Assertf(static_cast<CPickup*>(this)->IsFlagSet(CPickup::PF_Destroyed), "Pickup being destroyed outwith pickup system");
	}
	if (GetOwnedBy() == ENTITY_OWNEDBY_COMPENTITY)
	{
		Assertf(false,"Object is owned by compEntity - should not be flagged for destruction");
	}

	Assert(GetIsAnyManagedProcFlagSet() == false);

#endif

	Assertf(GetOwnedBy()!=ENTITY_OWNEDBY_INTERIOR, "Interior-owned object %s being flagged for removal!", GetModelName());
//	Assertf(GetIplIndex()==0, "Map-owned object %s being flagged for removal!", GetModelName());

	if (GetNetworkObject() && !CNetObjObject::IsAmbientObjectType(GetOwnedBy()) && !IsADoor())
	{
#if ENABLE_NETWORK_LOGGING
		NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManagerLog(), "FLAG_FOR_DESTROY_IN_NEXT_PROCESS", GetNetworkObject()->GetLogName());
		Displayf("%s FLAG_FOR_DESTROY_IN_NEXT_PROCESS", GetNetworkObject()->GetLogName());
		sysStack::PrintStackTrace();
#endif
		// this object is being controlled by a remote machine. Doors and ambient objects are a special case, they are allowed to be removed locally.
		if (NetworkUtils::IsNetworkCloneOrMigrating(this))
		{
			Assertf(false, "Object %s is being flagged for destruction as a clone or migrating. Owned by: %d", GetNetworkObject()->GetLogName(), GetOwnedBy());
			return;
		}
	}

	SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);

#if GTA_REPLAY
	// Replay makes a note of this objects MapID so it hide the version the map/streamer makes when the scene is loaded upon playback.
	CReplayMgr::RecordDestroyedMapObject(this);
#endif // GTA_REPLAY

	if (!GetIsOnSceneUpdate()){
		AddToSceneUpdate();
	}
}

//
//
// Makes the object move up or down depending on the presence of a train nearby.
//

void CObject::SetMatrixForTrainCrossing(Matrix34 *pMatr, float Angle)
{
	// Rebuild the matrix with the new angle.
	Vector3	TempForward = CrossProduct( Vector3(0.0f, 0.0f, 1.0f), pMatr->a);
	Vector3 Forward = Vector3(0.0f, 0.0f, 1.0f) * rage::Sinf(Angle) + TempForward * rage::Cosf(Angle);
	Vector3	TempUp = CrossProduct( pMatr->a, Forward );
	pMatr->c.x = TempUp.x;
	pMatr->c.y = TempUp.y;
	pMatr->c.z = TempUp.z;
	pMatr->b.x = Forward.x;
	pMatr->b.y = Forward.y;
	pMatr->b.z = Forward.z;
}

//
//
// Makes the object move up or down depending on the presence of a train nearby.
//
void CObject::ProcessTrainCrossingBehaviour()
{
	// Once in a while find out whether there is a train nearby.
/*RAGE	if (!((fwTimer::GetFrameCount() + this->RandomSeed) & 16))
	{
		CTrain *pTrain = CTrain::FindNearestTrain(GetPosition(), true);

		bool oldTrainNearby = m_nObjectFlags.bTrainNearby;
		m_nObjectFlags.bTrainNearby = false;
		if (pTrain)
		{
			if ((pTrain->GetPosition() - this->GetPosition()).XYMag() < 120.0f)
			{
				m_nObjectFlags.bTrainNearby = true;
			}
		}

		if (GetModelIndex() == MI_TRAINCROSSING1)
		{
			Vector3 posn;
			Assert(m_pRelatedDummy);
			if (oldTrainNearby && !m_nObjectFlags.bTrainNearby)
			{
				posn = m_pRelatedDummy->GetPosition();
				ThePaths.SetLinksBridgeLights(posn.x - RAILLIGHTRANGE, posn.x + RAILLIGHTRANGE, posn.y - RAILLIGHTRANGE, posn.y + RAILLIGHTRANGE, false);
			}
			else if ( (!oldTrainNearby) && m_nObjectFlags.bTrainNearby)
			{
				posn = m_pRelatedDummy->GetPosition();
				ThePaths.SetLinksBridgeLights(posn.x - RAILLIGHTRANGE, posn.x + RAILLIGHTRANGE, posn.y - RAILLIGHTRANGE, posn.y + RAILLIGHTRANGE, true);
			}
		}
	}

	if (GetModelIndex() == MI_TRAINCROSSING1) return;	// This is the post with the lights. (Doesn't rotate)

	// Rotate the barrier up or down depending on whether there is a train nearby.
	float	Angle = rage::Acosf(GetC().z);

	if (m_nObjectFlags.bTrainNearby)
	{
		Angle = MAX(Angle - 50.0f*fwTimer::GetTimeStep() * 0.005f, 0.0f);
	}
	else
	{
		Angle = MIN(Angle + 50.0f*fwTimer::GetTimeStep() * 0.005f, MAXCROSSINGANGLE);
	}

	SetMatrixForTrainCrossing(&GetMatrix(), Angle);*/
}


//
//
//
//
bool CObject::ObjectDamage(float fImpulse, const Vector3* pColPos, const Vector3* pColNormal, CEntity* pEntityResponsible, u32 WeaponUsedHash, phMaterialMgr::Id hitMaterial)
{
	((void)pColPos);
	((void)pColNormal);

//	if(!GetUsesCollision())	//	GW 29/09/06 - Sandy commented this out so that attached objects
//		return;				//	 can still be damaged (as in Dave Beddoes' Arson Attack mission)

	if (WeaponUsedHash == 0 && pEntityResponsible && pEntityResponsible->GetIsTypeVehicle())
	{
		WeaponUsedHash = WEAPONTYPE_RUNOVERBYVEHICLE;
	}

	if (WeaponUsedHash == 0)
	{
		return false;
	}

	// First check its physical flags as it may not actually get damaged
	if (CanPhysicalBeDamaged(pEntityResponsible, WeaponUsedHash) == false)
	{
		if(NetworkInterface::ShouldTriggerDamageEventForZeroDamage(this))
		{
			fImpulse = 0.0f;
		}
		else
		{
			return false;
		}
	}

	// CNC: Prevent ATMs from being damaged.
	// Doing this in code rather than from script as these aren't scripted props (they're baked in the map), 
	// so setting them as invincible from script would require a lot of iteration/searching for props on every players machine etc.
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		const u32 uModelNameHash = GetModelNameHash();
		static const u32 PROP_ATM_01 = ATSTRINGHASH("PROP_ATM_01", 0xcc179926);
		static const u32 PROP_ATM_02 = ATSTRINGHASH("PROP_ATM_02", 0xbcdefab5);
		static const u32 PROP_ATM_03 = ATSTRINGHASH("PROP_ATM_03", 0xaea85e48);
		static const u32 PROP_FLEECA_ATM = ATSTRINGHASH("PROP_FLEECA_ATM", 0x1e34b5c2);
		if (uModelNameHash == PROP_ATM_01 ||
			uModelNameHash == PROP_ATM_02 ||
			uModelNameHash == PROP_ATM_03 || 
			uModelNameHash == PROP_FLEECA_ATM)
		{
			return false;
		}
	}

    float oldHealth = m_fHealth;

	m_fHealth -= fImpulse/*RAGE *m_pObjectInfo->m_fCollisionDamageMultiplier*/;
	if(m_fHealth < 0.0f)	m_fHealth = 0.0f;

    if(oldHealth>0.0f && pEntityResponsible && (pEntityResponsible->GetIsTypePed() || pEntityResponsible->GetIsTypeVehicle()))
    {
 		SetWeaponDamageInfo(pEntityResponsible, WeaponUsedHash, fwTimer::GetTimeInMilliseconds());
    }

	//Update Newtork Damage Trackers
	if(NetworkInterface::IsGameInProgress())
    {
		CNetObjPhysical* netVictim = static_cast<CNetObjPhysical *>(GetNetworkObject());
		const float damage = oldHealth - m_fHealth;
		if(netVictim && oldHealth > 0.0f && (damage > 0.0f || NetworkInterface::ShouldTriggerDamageEventForZeroDamage(this)))
		{
			// If the object is attached to a vehicle the responsible is the driver
			CVehicle* pAttachParentVehicle = 0;
			if(GetAttachParent() && static_cast<CEntity*>(GetAttachParent())->GetIsTypeVehicle())
			{
				pAttachParentVehicle = (CVehicle *)GetAttachParent();
			}

			if (pAttachParentVehicle && pAttachParentVehicle->GetDriver() && !pEntityResponsible)
			{
				CNetObjPhysical::NetworkDamageInfo damageInfo(pAttachParentVehicle->GetDriver(), damage, 0.0f, 0.0f, false, WeaponUsedHash, -1, false, m_fHealth <= 0.0f,false, hitMaterial);
				netVictim->UpdateDamageTracker(damageInfo);
			}
			else
			{
				CNetObjPhysical::NetworkDamageInfo damageInfo(pEntityResponsible, damage, 0.0f, 0.0f, false, WeaponUsedHash, -1, false, m_fHealth <= 0.0f,false, hitMaterial);
				netVictim->UpdateDamageTracker(damageInfo);
			}
		}
		// if victim doesn't have a network object and script wants to receive damage events for them
		else if(ms_bAllowDamageEventsForNonNetworkedObjects)
		{
			netObject* firingNetEntity = NetworkUtils::GetNetworkObjectFromEntity(pEntityResponsible);
			if(firingNetEntity && !firingNetEntity->IsClone())
			{
				sEntityDamagedData dData;
				dData.VictimId.Int = CTheScripts::GetGUIDFromEntity(*this);
				dData.DamagerId.Int = CTheScripts::GetGUIDFromEntity(*pEntityResponsible);
				dData.Damage.Float = damage;
				dData.EnduranceDamage.Float = 0.0f;
				dData.VictimDestroyed.Int = m_fHealth - damage <= 0.0f;
				dData.WeaponUsed.Int = WeaponUsedHash;
				dData.VictimSpeed.Float = 0.0f;
				dData.DamagerSpeed.Float = 0.0f;
				dData.IsHeadShot.Int = 0;
				dData.WithMeleeWeapon.Int = WeaponUsedHash == WEAPONGROUP_MELEE;
				dData.IsResponsibleForCollision.Int = 0;
				dData.HitMaterial.Uns = (u8)(hitMaterial & 0x00000000000000FFLL);
				GetEventScriptNetworkGroup()->Add(CEventNetworkEntityDamage(dData));
			}
		}
    }

	// if we've wrecked a vehicle, register it....
	if(NetworkInterface::IsGameInProgress())
	{
		// if the object is a clone, the registration gets taken care of elsewhere (NetObjPhysical::SetPhysicalHealthData)
		if( GetNetworkObject() && !IsNetworkClone() ) 
		{
			// register the kill....
			NetworkInterface::RegisterKillWithNetworkTracker(this);		
		}
	}

	ProcessSniperDamage();

	return true;	// damage was inflicted

}//end of CObject::ObjectDamage()...



void CObject::SetObjectTargettable(bool bTargettable)
{
	m_nObjectFlags.bCanBeTargettedByPlayer = bTargettable;
}


bool CObject::CanBeTargetted(void) const
{
	if (m_nObjectFlags.bCanBeTargettedByPlayer)
	{
		return true;
	}
	else
	{
		return false;
	}
}



//
//
//
//
void CObject::Teleport(const Vector3& vecSetCoors, float fSetHeading, bool UNUSED_PARAM(bCalledByPedTask), bool bTriggerPortalRescan, bool UNUSED_PARAM(bCalledByPedTask2), bool bWarp, bool UNUSED_PARAM(bKeepRagdoll), bool UNUSED_PARAM(bResetPlants))
{
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfTeleportCallingObject(), vecSetCoors );

	if(fSetHeading >= -PI && fSetHeading <= TWO_PI)
		CEntity::SetHeading(fSetHeading);

	SetPosition(vecSetCoors, true, true, bWarp);

	// force portal tracker to update from new position (should eventually get data out of collisions under object... hopefully...)
	if (bTriggerPortalRescan)
	{
		GetPortalTracker()->ScanUntilProbeTrue();
		GetPortalTracker()->Update(vecSetCoors);
	}

	m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = false; // Force the m_nFlags.bPossiblyTouchesWater to be updated

	g_ptFxManager.RelocateEntityPtFx(this);

	if(IsInPathServer())
	{
		CPathServerGta::UpdateDynamicObject(this, true, true);
	}

}//end of CObject::Teleport()...

//
// name:		RemoveNextFrame
// description:	Will make sure the object gets removed next frame
//
void CObject::RemoveNextFrame()
{
	Assertf(!m_pRelatedDummy, "Planning to remove an object but it has a dummy");
#if __ASSERT
	Assert(GetIsAnyManagedProcFlagSet() == false);
	if (m_nFlags.bIsProcObject)
	{
		Assertf(0, "Trying to turn a procedural object into a ENTITY_OWNEDBY_TEMP");
	}
	else
#endif
	{
		SetOwnedBy( ENTITY_OWNEDBY_TEMP );
		m_nEndOfLifeTimer = 0;
	}
}

void CObject::DecoupleFromMap()
{
	Assertf(GetIplIndex()>0, "%s is not a map object", GetModelName());
	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	if ( Verifyf(!pModelInfo->IsStreamedArchetype(), "Trying to intentionally decouple / leak map object %s but it isn't a permanent archetype", GetModelName()) )
	{
		SetIplIndex(0);
		m_nFlags.bNeverDummy = false;
		m_nObjectFlags.bOnlyCleanUpWhenOutOfRange = true;
		m_bIntentionallyLeakedObject = true;
		ClearRelatedDummy();
		SetOwnedBy(ENTITY_OWNEDBY_TEMP);
	}
}

//////////////////////////////////////////////////////////////////////////////
// NAME :     CanBeUsedToTakeCoverBehind
// FUNCTION : Works out whether this is a good object to take cover behind.
//////////////////////////////////////////////////////////////////////////////

bool CObject::CanBeUsedToTakeCoverBehind()
{
// PH: below check removed to allow peds to hide behind objects
//	   added in missions, may need to add a flag on a per object
//	   basis if in particular missions some objects want no-one to
//     go near.
//	if (GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
//	{
//		return false;
//	}

	// Cant take cover behind objects attached to peds (like rakes)
	fwAttachmentEntityExtension *extension = GetAttachmentExtension();
	if(extension && extension->GetIsAttached() && extension->GetAttachParent() && ((CPhysical *) extension->GetAttachParent())->GetIsTypePed() )
	{
		return false;
	}

	// If fraginst, only completely intact versions provide cover
	if( m_nFlags.bIsFrag )
	{
		// Ignore fragments generated by destroyed objects
		if( GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE )
		{
			return false;
		}

		// Ignore fragments that have any part broken off
		if( GetFragInst() && GetFragInst()->GetPartBroken() )
		{
			return false;
		}
	}

	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	Assert(pModelInfo);
	if (pModelInfo->GetDoesNotProvideAICover())
	{
		return false;
	}

	Vector3 bbMin = GetBoundingBoxMin();
	Vector3 bbMax = GetBoundingBoxMax();

	// Objects of a certain height are ok.
	float	BoundingBoxHeight = bbMax.z - bbMin.z;
	float	BoundingBoxWidth = bbMax.x - bbMin.x;
	float	BoundingBoxDepth = bbMax.y - bbMin.y;

	if ( ( BoundingBoxHeight > MIN_OBJECT_COVER_BOUNDING_BOX_HEIGHT ) &&
		 ( BoundingBoxWidth < MAX_OBJECT_COVER_BOUNDING_BOX_WIDTH )  &&
		 ( BoundingBoxDepth < MAX_OBJECT_COVER_BOUNDING_BOX_WIDTH ) &&
			(( BoundingBoxWidth > MIN_OBJECT_COVER_BOUNDING_BOX_WIDTH ) ||
			 ( BoundingBoxDepth > MIN_OBJECT_COVER_BOUNDING_BOX_WIDTH )) )
	{
		// Make sure it hasn't fallen over.
		//if (GetC().z > 0.9f)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
// NAME :     AssertObjectPointerValid
// FUNCTION : Will assert if the object pointer is not valid or the object is not in
//            the world.
//////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
void AssertObjectPointerValid(CObject *pObject)
{
    Assert(IsObjectPointerValid(pObject));
    /*
	AssertObjectPointerValid_NotInWorld(pObject);

	// Make sure this dummy is actually in the world at the moment
	Assert(pObject->m_listEntryInfo.GetHeadPtr());
	*/
}

void AssertObjectPointerValid_NotInWorld(CObject *pObject)
{
    Assert(IsObjectPointerValid_NotInWorld(pObject));
    /*
	s32	Index;

	Assert(pObject);	// Null pointers are not valid

	Index = (CPools::GetObjectPool().GetJustIndex(pObject));

	Assert(Index >= 0 && Index <= CPools::GetObjectPool().GetSize());
    */
}
#endif//DEBUG

bool IsObjectPointerValid(CObject *pObject)
{
	if(!IsObjectPointerValid_NotInWorld(pObject))
	{
	    return false;
	}
	return true;
}

bool IsObjectPointerValid_NotInWorld(CObject *pObject)
{
	return CObject::GetPool()->IsValidPtr(pObject);
}

bool CObject::PlaceOnGroundProperly(float fMaxRange, bool bAlign, float heightOffGround, bool bIncludeWater, bool bUpright, bool *pInWater, bool bIncludeObjects, bool useExtendedProbe)
{
	float vStartOffset = useExtendedProbe ? 2.2f : 1.0f;

	u32 flags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_MAP_TYPE_WEAPON;

	if(bIncludeWater)
		flags |= ArchetypeFlags::GTA_RIVER_TYPE;

	if (bIncludeObjects)
	{
		flags |= ArchetypeFlags::GTA_OBJECT_TYPE;

		// use a small initial offset as the object may be in a confined place
		vStartOffset = 0.2f;
	}

	// adjust the probe so that it is relative to the bottom of the object (for very large objects the probe can fail to find the ground, or find a roof above)
	vStartOffset -= heightOffGround;

	const Vector3 vStart = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) + vStartOffset*ZAXIS;
	const Vector3 vEnd = vStart - fMaxRange*ZAXIS;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestHitPoint probeHitPoint;
	WorldProbe::CShapeTestResults probeResult(probeHitPoint);
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetIncludeFlags(flags);
	probeDesc.SetExcludeEntity(this);

	Vector3 resultNormal;
	Vector3 resultPosition;
	bool probeHit = false;

	// test world
	probeHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
	if(probeHit)
	{
		resultPosition = probeResult[0].GetHitPosition();
		resultNormal = probeResult[0].GetHitNormal();
	}
	// test sea
	if(bIncludeWater)
	{
		float waterZ;
		bool bShouldPlaySounds;
		bool bInWater = Water::GetWaterLevelNoWaves(vStart, &waterZ, 0.5f, 1500.0f, &bShouldPlaySounds);
		// If in water and either the probe hit nothing and the end of the probe is underwater or the probe hit something and the point it hit is underwater
		if(bInWater && ((!probeHit && waterZ > vEnd.z) || (probeHit && waterZ > resultPosition.z)))
		{
			resultPosition = vStart;
			resultPosition.z = waterZ;
			resultNormal = ZAXIS;
			probeHit = true;

			if (pInWater)
			{
				*pInWater = true;
			}
		}
	}

	if(probeHit)
	{
		Matrix34 newMat;
		newMat.Identity();
		if(bAlign)
		{
			newMat.MakeRotateTo(ZAXIS,resultNormal);
		}
		newMat.Rotate(newMat.c,GetTransform().GetHeading());

		if (m_nObjectFlags.bIsPickUp && !bUpright)
		{
			newMat.RotateLocalX(PI/2); // rotate pickup 90 degrees so it lies on its side
		}

		Vector3 vObjectBottom;
		
		if (heightOffGround != 0.0f)
		{
			vObjectBottom = -heightOffGround * ZAXIS;
		}
		else
		{
			if(m_nObjectFlags.bIsPickUp && !bUpright)
			{
				// need to rotate the BB min to find the bottom if we rotated the pickup above
				vObjectBottom.Set(GetBoundingBoxMin());
				vObjectBottom.RotateX(PI/2); 
				vObjectBottom *= ZAXIS;
			}
			else 
			{
				vObjectBottom = GetBoundingBoxMin().z * ZAXIS;
			}
		}

		newMat.d = resultPosition - vObjectBottom;
		SetMatrix(newMat, true, true, true);
		if (fragInst* pInst = GetFragInst())
		{
			pInst->SetResetMatrix(newMat);
		}

		return true;
	}

	return false;
}


/*
//
// name:		UpdatePortalTracker
// description:	update the portal tracker if needed
void CObject::UpdatePortalTracker()
{
	// ensure that this object is tracked into and through any interiors (and interior collisions are loaded etc.)
	if (m_pPortalTracker){
		m_pPortalTracker->Update(GetPosition());
	}
}*/

//
// name:		ObjectHasBeenMoved
// description:	Called when an object gets moved by another object or weapon, etc
// parameters:	pMover - the object that moved this one
void CObject::ObjectHasBeenMoved(CEntity* pMover, bool bRegisterWithNoMover)
{
	Assert(pMover != this);

	if (!m_bRegisteredInNetworkGame &&
		!GetNetworkObject() &&
		GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT) // mission objects are registered when created, and if they aren't then the script does not want them registered
	{
		if (NetworkInterface::IsGameInProgress() && (m_nObjectFlags.bMovedInNetworkGame || CNetObjObject::HasToBeCloned(this)))
		{
			m_nObjectFlags.bMovedInNetworkGame = true;

			const CCollisionHistory* pCollisionHistory = NULL;

			if (!pMover)
			{
				if (GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
				{
					if (GetFragParent() && GetFragParent()->GetIsPhysical())
					{
						pCollisionHistory = SafeCast(CPhysical, GetFragParent())->GetFrameCollisionHistory();
					}
				}
				else
				{
					pCollisionHistory = GetFrameCollisionHistory();
				}
			
				// we may not have a mover if the object was broken apart, in this case we need to look in the collision history
				if (!pMover && pCollisionHistory)
				{
					const CCollisionRecord* pMostSignificantCollision = pCollisionHistory->GetMostSignificantCollisionRecord();

					if (pMostSignificantCollision)
					{
						pMover = pMostSignificantCollision->m_pRegdCollisionEntity;
					}
				}
			}

			if (!pMover)
			{
				// try weapon damage instead! Sigh...
				pMover = GetWeaponDamageEntity();
			}

			// there is no mover, try again in the next update
			if (!pMover && !bRegisterWithNoMover)
			{
				return;
			}

			netObject* pMoverObj = (pMover && pMover->GetIsPhysical()) ? static_cast<CPhysical*>(pMover)->GetNetworkObject() : NULL;

			// the object has been moved for the first time, so need to store what woke this entity up if is being moved by a local object
			if (pMoverObj && !pMoverObj->IsClone())
			{
				char reason[100];
				formatf(reason, "Moved by local %s", pMoverObj->GetLogName());
				CNetObjObject::TryToRegisterAmbientObject(*this, false, reason);
			}
			else
			{
				// if there is no mover or if the mover is a clone we need to wait for a while to see if any other machines claim
				// ownership of this object, if not then we will
				m_firstMovedTimer = 500;
			}
		}

		m_bRegisteredInNetworkGame = true;
	}
	else
	{
		m_bRegisteredInNetworkGame = true;
	}

	// If we had any cover orientation set before, set it to CO_Dirty. This just
	// means that CCover::AddCoverPointFromObject() will try harder to get the attached
	// cover points updated. If the cover orientation is CO_Invalid, we shouldn't have any
	// cover points already, and we want to keep it that way unless AddCoverPointFromObject()
	// decides otherwise.
	if(GetCoverOrientation() != CO_Invalid)
	{
		SetCoverOrientation(CO_Dirty);
	}
}

//
// name:		NetworkRegisterAllObjects
// description:	Registers all objects with the network object manager when a new network
//				game starts.
//
void CObject::NetworkRegisterAllObjects()
{
	CObject *pObj = NULL;
	CObject::Pool* pObjPool = CObject::GetPool();
	s32 i = (s32) pObjPool->GetSize();

	NetworkInterface::GetObjectManagerLog().WriteEventData("REGISTER_EXISTING_GAMEWORLD_OBJECTS\r\n", "", 0);

	while(i--)
	{
		pObj = pObjPool->GetSlot(i);

		if(pObj)
		{
			if(!pObj->GetNetworkObject())
			{
				bool bRegister = false;

				// script objects are only registered if they are flagged to be networked
				CScriptEntityExtension* pExtension = pObj->GetExtension<CScriptEntityExtension>();

				if (!pObj->m_nPhysicalFlags.bNotToBeNetworked)
				{
					if (pExtension)
					{
						if (pObj->IsADoor())
						{
							Assertf(0, "Door %p found with a script extension when entering MP!", pObj);
						}
						else
						{
							bRegister = pExtension->GetIsNetworked();
						}
					}
					else if (pObj->m_nObjectFlags.bMovedInNetworkGame && CNetObjObject::HasToBeCloned(pObj) && !pObj->GetNetworkObject())
					{
						bRegister = true;
					}
				}

				if (bRegister)
				{
					Vector3 vPos = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()); 

					NetworkInterface::GetObjectManagerLog().WriteEventData("PRE_REGISTERING_OBJECT\n", "", 0);
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Object Address", "%p", pObj);
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Register Reason", "%s", pExtension ? "Script" : "Moved");
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Owned By", "%d", pObj->GetOwnedBy());
#if !__FINAL
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Model Name", "%s", pObj->GetModelName());
#endif
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Position", "%.2f, %.2f, %.2f", vPos.x, vPos.y, vPos.z);

					if(pExtension && Verifyf(pExtension->GetScriptInfo(), "No script info!"))
						NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", "%s", pExtension->GetScriptInfo()->GetScriptId().GetLogName());
					
					// register the object
					if(NetworkInterface::RegisterObject(pObj))
					{
						if(Verifyf(pObj->GetNetworkObject(), "No network object!"))
							NetworkInterface::GetObjectManagerLog().WriteDataValue("NetObj Address", "%p", pObj->GetNetworkObject());
					}
					else
					{	
#if __DEV
						NetworkInterface::GetObjectManagerLog().Log("Failed to register object %p! Reason is %s", pObj, NetworkInterface::GetLastRegistrationFailureReason());
#else
						NetworkInterface::GetObjectManagerLog().Log("Failed to register object %p!", pObj);
#endif
						Assertf(0, "Failed to register object %p! Reason is %s", pObj, NetworkInterface::GetLastRegistrationFailureReason());
					}
				}
			}
			else
			{
				Vector3 vPos = VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()); 
				NetworkLogUtils::WritePlayerText(NetworkInterface::GetObjectManagerLog(), pObj->GetNetworkObject()->GetPhysicalPlayerIndex(), "ERROR_REGISTERING_OBJECT", pObj->GetNetworkObject()->GetLogName());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Object Address", "%p", pObj);
				NetworkInterface::GetObjectManagerLog().WriteDataValue("NetObj Address", "%p", pObj->GetNetworkObject());
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Owned By", "%d", pObj->GetOwnedBy());
#if !__FINAL
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Model Name", "%s", pObj->GetModelName());
#endif
				NetworkInterface::GetObjectManagerLog().WriteDataValue("Position", "%.2f, %.2f, %.2f", vPos.x, vPos.y, vPos.z);

				// grab the script information if valid
				CScriptEntityExtension* pExtension = pObj->GetExtension<CScriptEntityExtension>();
				if(pExtension && pExtension->GetIsNetworked() && Verifyf(pExtension->GetScriptInfo(), "No script info!"))
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Script", "%s", pExtension->GetScriptInfo()->GetScriptId().GetLogName());

				Assertf(0, "Object %p already has a network object (%s)!", pObj, pObj->GetNetworkObject()->GetLogName());
			}
		}
	}

	NetworkInterface::GetObjectManagerLog().WriteEventData("FINISHED_REGISTER_EXISTING_GAMEWORLD_OBJECTS\r\n", "", 0);
}

///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __DEV
void FixObject()
{
	if(CDebugScene::FocusEntities_Get(0) && CDebugScene::FocusEntities_Get(0)->GetIsTypeObject())
	{
		CObject* pObject = static_cast<CObject*>(CDebugScene::FocusEntities_Get(0));

		pObject->Fix();
	}
}
#endif

#if __BANK

void CObject::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Objects", 0, 0, false);
#if __DEV
	bank.AddToggle("Display all objects on VectorMap", &CDebugScene::bDisplayObjectsOnVMap);
	bank.AddToggle("Display line above objects", &CDebugScene::bDisplayLineAboveObjects);
	bank.AddToggle("Display line above all entities", &CDebugScene::bDisplayLineAboveAllEntities);
	bank.AddSlider("Line length for entity debug display", &CDebugScene::fEntityDebugLineLength, 0.0f, 100.0f, 0.01f);
	bank.AddToggle("Show Ladder Info", &CDebugScene::bDisplayLadderDebug);
	bank.AddToggle("Show Door Info", &CDebugScene::bDisplayDoorInfo);

	bank.AddToggle("Display duplicate object bounding boxes", &CDebugScene::bDisplayDuplicateObjectsBB);
	bank.AddToggle("Display duplicate objects on map", &CDebugScene::bDisplayDuplicateObjectsOnVMap);
	bank.AddSlider("Swayable Wind Speed", &SWAYABLE_WIND_SPEED_OVERRIDE, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("Swayable Lo Speed", &SWAYABLE_LO_SPEED, 0.0f, 10.0f, 0.001f);
	bank.AddSlider("Swayable Lo Amplitude", &SWAYABLE_LO_AMPLITUDE, 0.0f, 10.0f, 0.001f);
	bank.AddSlider("Swayable Hi Speed", &SWAYABLE_HI_SPEED, 0.0f, 10.0f, 0.001f);
	bank.AddSlider("Swayable Hi Amplitude", &SWAYABLE_HI_AMPLITUDE, 0.0f, 10.0f, 0.001f);
	bank.AddButton("Repair focus object",FixObject);

	bank.AddSlider("Minimum mass for minimap noise", &ms_fMinMassForCollisionNoise, 0.0f, 1000000.0f, 0.1f);
	bank.AddSlider("Minimum relative velocity (squared) for minimap noise", &ms_fMinRelativeVelocitySquaredForCollisionNoise, 0.0f, 10000.0f, 1.0f);
#endif	// __DEV

	bank.AddToggle("Show Door Persistent Info", &CDebugScene::bDisplayDoorPersistentInfo);
	bank.PushGroup("Cover", false);
	bank.AddButton("Create cover tuning widgets", datCallback(CFA(CObject::CreateCoverTuningWidgetsCB)));
	bank.PopGroup();

	bank.PushGroup("Doors", false);
	bank.AddButton("Create door tuning widgets", datCallback(CFA(CObject::CreateDoorTuningWidgetsCB)));
	CDoor::AddClassWidgets(bank);
	bank.PopGroup();
}


void CObject::CreateDoorTuningWidgetsCB()
{
	bkBank* pBank = BANKMGR.FindBank("Objects");
	if(!pBank)
	{
		return;
	}
	bkWidget* pWidget = BANKMGR.FindWidget("Objects/Doors/Create door tuning widgets");
	if(pWidget == NULL)
	{
		return;
	}
	pWidget->Destroy();

	CDoorTuningManager::GetInstance().AddWidgets(*pBank);
}

void CObject::CreateCoverTuningWidgetsCB()
{
	bkBank* pBank = BANKMGR.FindBank("Objects");
	if(!pBank)
	{
		return;
	}
	bkWidget* pWidget = BANKMGR.FindWidget("Objects/Cover/Create cover tuning widgets");
	if(pWidget == NULL)
	{
		return;
	}
	pWidget->Destroy();

	CCoverTuningManager::GetInstance().AddWidgets(*pBank);
}

#endif // __BANK

// --------------- Locked object stuff ---------------------


void CLockedObects::Initialise()
{
	m_lockedObjs.Create(MAX_LOCKED_OBJECTS);

	// test stuff
/*	Vector3	pos(100.0f, 100.0f, 100.0f);
	SetObjectStatus(pos, true, 1.0f, 1);


	pos = Vector3(100.0f, 100.0f, 100.0f);
	SetObjectStatus(pos, false, 1.0f, 1);

	pos = Vector3(300.0f, 300.0f, 300.0f);
	SetObjectStatus(pos, true, -1.0f, 1);

	pos = Vector3(-100.0f, -100.0f, -50.0f);
	SetObjectStatus(pos, false, -1.0f, 1);

	pos = Vector3(100.0f, 100.0f, 100.0f);
	CLockedObjStatus *pObjStat = GetObjectStatus(pos, 1);
	if (pObjStat)
	{
		printf("%d,%d\n",pObjStat->m_bLocked, pObjStat->m_modelHashKey);
	}

	pos = Vector3(200.0f, 200.0f, 200.0f);
	pObjStat = GetObjectStatus(pos, 1);
	if (pObjStat)
	{
		printf("%d,%d\n",pObjStat->m_bLocked, pObjStat->m_modelHashKey);
	}

	pos = Vector3(300.0f, 300.0f,300.0f);
	pObjStat = GetObjectStatus(pos, 1);
	if (pObjStat)
	{
		printf("%d,%d\n",pObjStat->m_bLocked, pObjStat->m_modelHashKey);
	}

	pos = Vector3(-100.0f, -100.0f, -50.0f);
	pObjStat = GetObjectStatus(pos, 1);
	if (pObjStat)
	{
		printf("%d,%d\n",pObjStat->m_bLocked, pObjStat->m_modelHashKey);
	}*/
}

void CLockedObects::ShutdownLevel()
{
	Reset();
}

void CLockedObects::Reset()
{
	m_lockedObjs.Kill();
}

u64 CLockedObects::GenerateHash64(const Vector3 &pos){
	// Note: CLockedObjects::DebugDraw() relies on this, please keep it
	// up to date if you make changes. If it's changed to a non-reversible
	// hashing function, we may want to store the position in CLockedObjStatus
	// in debug builds.

	u16	zPart = (s16)(rage::Floorf(pos.z + 0.5f));
	u16	yPart = (s16)(rage::Floorf(pos.y + 0.5f));
	u16	xPart = (s16)(rage::Floorf(pos.x + 0.5f));

	u64 hash = 0;
	hash = hash | zPart;  // store z in high bits (47 - 32)
	hash = hash << 16;
	hash = hash | yPart; // store y in 31 - 16
	hash = hash << 16;
	hash = hash | xPart; // store x in 15 - 0

	return(hash);
}

CLockedObjStatus* CLockedObects::SetObjectStatus(CEntity *pEntity, bool bIsLocked, float targetRatio, bool unsprung, float automaticRate, float automaticDist) {
	Assert(pEntity);

	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	Assert(pModelInfo);

	return SetObjectStatus(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()), bIsLocked, targetRatio, unsprung, pModelInfo->GetHashKey(), automaticRate, automaticDist);
}

CLockedObjStatus* CLockedObects::SetObjectStatus(const Vector3 &pos, bool bIsLocked, float targetRatio, bool bUnsprung, u32 modelHash, float automaticRate, float automaticDist){

	CLockedObjStatus	newEntry;//(bIsLocked, targetRatio, modelHash);
	newEntry.m_bLocked = bIsLocked;
	newEntry.m_bUnsprung = bUnsprung;
	newEntry.m_TargetRatio = targetRatio;
	newEntry.m_modelHashKey = modelHash;
	newEntry.m_AutomaticRate = automaticRate;
	newEntry.m_AutomaticDist = automaticDist;

	u64 posHash64 = GenerateHash64(pos);

 	if (m_lockedObjs.GetNumUsed() >= MAX_LOCKED_OBJECTS)
 	{
 		Assertf(0, "CLockedObects::SetObjectStatus - big map of locked objects is full");
 		return NULL;
 	}


	// if there is a matching entry already then modify it, else add a new one
	CLockedObjStatus* pStatus = m_lockedObjs.Access(posHash64);
	if (pStatus)
	{
		Assertf(pStatus->m_modelHashKey == modelHash,"Trying to create 2 locked objects at (%.0f,%.0f,%.0f)\n",pos.x,pos.y,pos.z);
#if __DEV
		CBaseModelInfo* pNewMI = CModelInfo::GetBaseModelInfoFromHashKey(modelHash, NULL);
		CBaseModelInfo* pExistingMI = CModelInfo::GetBaseModelInfoFromHashKey(pStatus->m_modelHashKey, NULL);
		Assertf(pStatus->m_modelHashKey == modelHash, "Cannot lock the %s because the %s at this pos is already locked!", pNewMI->GetModelName(), pExistingMI->GetModelName());
#endif //__DEV
		if (pStatus->m_modelHashKey == modelHash){
			pStatus->m_TargetRatio = targetRatio;
			pStatus->m_bLocked = bIsLocked;
			pStatus->m_bUnsprung = bUnsprung;
			pStatus->m_AutomaticRate = automaticRate;
			pStatus->m_AutomaticDist = automaticDist;
		}
		return pStatus;
	} else {
		const LockedObjMap::Entry& resultEntry = m_lockedObjs.Insert(posHash64, newEntry);
		return const_cast<CLockedObjStatus*>(&resultEntry.data);
	}
}

CLockedObjStatus* CLockedObects::GetObjectStatus(const Vector3 &pos, u32 modelHash){

	((void)modelHash);

	u64 posHash64 = GenerateHash64(pos);

	CLockedObjStatus *pStatus = m_lockedObjs.Access(posHash64);

	return(pStatus);

}

CLockedObjStatus* CLockedObects::GetObjectStatus(const u64 posHash, u32 modelHash){

	((void)modelHash);

	CLockedObjStatus *pStatus = m_lockedObjs.Access(posHash);

	return(pStatus);

}

CLockedObjStatus* CLockedObects::GetObjectStatus(CEntity* pEntity) {
	Assert(pEntity);

	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	Assert(pModelInfo);

	return(GetObjectStatus(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()), pModelInfo->GetHashKey()));
}

#if !__FINAL

void CLockedObects::DebugDraw() const
{
#if DEBUG_DRAW
	LockedObjMap::ConstIterator iter = m_lockedObjs.CreateIterator();
	for(; !iter.AtEnd(); iter.Next())
	{
		const CLockedObjStatus& status = iter.GetData();

		// The current GenerateHash64() hashing function is actually reversible,
		// if we assume that the world fits within 32 km of the origin, so we can
		// decode it back to a position here.
		u64 hash = iter.GetKey();
		const s16 xPart = (u16)hash;
		hash >>= 16;
		const s16 yPart = (u16)hash;
		hash >>= 16;
		const s16 zPart = (u16)hash;
		Vec3V pos;
		pos.SetXf((float)xPart);
		pos.SetYf((float)yPart);
		pos.SetZf((float)zPart);

		char buff[1024];
		formatf(buff, "Tgt: %f\nLocked: %d\nUnsprung: %d\nModel: %08x\nAutorate: %.2f\nAutodist: %.2f",
				status.m_TargetRatio, (int)status.m_bLocked, (int)status.m_bUnsprung, status.m_modelHashKey,
				status.m_AutomaticRate, status.m_AutomaticDist);;

		grcDebugDraw::Text(pos, Color_white, 0, 0, buff);
	}
#endif	// DEBUG_DRAW
}

#endif	// !__FINAL

void CObject::UpdateRenderFlags()
{
	CPhysical::UpdateRenderFlags();

	// We might be able to override the vehicle part flag.
	if (m_nObjectFlags.bVehiclePart)
	{
#if HAS_RENDER_MODE_SHADOWS
		// Yup.
		SetRenderFlag(RenderFlag_USE_CUSTOMSHADOWS);
#endif // HAS_RENDER_MODE_SHADOWS

		SetBaseDeferredType(DEFERRED_MATERIAL_VEHICLE);
	}
}

void CObject::SetHasExploded(bool* bNetworkExplosion)
{
	CPhysical::SetHasExploded(NULL);

	// if this is a script object that is not already networked, this was intentionally done by the script that created it
	if (bNetworkExplosion && *bNetworkExplosion && GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT)
	{
		CObject* pObjectToRegister = this;

		if (GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE)
		{
			pObjectToRegister = (GetFragParent() && GetFragParent()->GetIsTypeObject()) ? static_cast<CObject*>(GetFragParent()) : NULL;
		}

		if (pObjectToRegister)
		{
			netObject* pNetObj = pObjectToRegister->GetNetworkObject();

			if (pNetObj)
			{
				if (!pNetObj->IsClone() && !pObjectToRegister->GetNetworkObject()->GetSyncData())
				{
					pObjectToRegister->GetNetworkObject()->StartSynchronising();
				}

				*bNetworkExplosion = false;
			}
			else if (NetworkInterface::IsGameInProgress() && CNetObjObject::CanBeNetworked(pObjectToRegister) && NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_OBJECT, false))
			{
				bool bCanRegisterObject = true;
				if (pObjectToRegister->GetFragInst() && 
					pObjectToRegister->GetFragInst()->GetCacheEntry() && 
					pObjectToRegister->GetFragInst()->GetCacheEntry()->GetHierInst())
				{
					
					if(const atBitSet* objGroupBroken = pObjectToRegister->GetFragInst()->GetCacheEntry()->GetHierInst()->groupBroken)
					{
						const u32 numBrokenFlags = objGroupBroken->GetNumBits();

						// Search down the broken groups and exit if the group isn't broke
						// Then we know if i == numGroups then it's fully broke
						u32 bit = 0;
						for (; bit<numBrokenFlags; ++bit)
						{
							if (!objGroupBroken->IsSet(bit))
							{
								break;
							}
						}

						if (bit == numBrokenFlags)
						{
							// fragment object is fully broken, do not allow it to register on network
							bCanRegisterObject = false;
#if ENABLE_NETWORK_LOGGING
							NetworkInterface::GetObjectManagerLog().LineBreak();
							NetworkInterface::GetObjectManagerLog().Log("#### Blocked registration of broken object with all bits set 0x%x model: %s ####\n", this, GetModelName());
							NetworkInterface::GetObjectManagerLog().LineBreak();
#endif
						}
					}
				}

				if (bCanRegisterObject)
				{
#if ENABLE_NETWORK_LOGGING
					Vector3 objPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
					NetworkInterface::GetObjectManagerLog().LineBreak();
					NetworkInterface::GetObjectManagerLog().Log("#### Registering a random object 0x%x at %f, %f, %f that has exploded ####\n", this, objPos.x, objPos.y, objPos.z);
					NetworkInterface::GetObjectManagerLog().LineBreak();
#endif
					NetworkInterface::RegisterObject(pObjectToRegister);

					*bNetworkExplosion = false;
				}
			}
		}
	}
}

// void CObject::MakeBoneSwayWithWind(C2dEffect* pEffect)
// {
// 	if (!GetFragInst() || GetFragInst()->GetBroken() || GetFragInst()->GetPartBroken())
// 	{
// 		return;
// 	}
//
// 	crSkeleton* pSkel = NULL;
// 	Assert(GetIsDynamic());
//
// 	CSwayableAttr* sw = pEffect->GetSwayable();
//
// 	CDynamicEntity* pDynEntity = static_cast<CDynamicEntity*>(this);
// 	pSkel = const_cast<crSkeleton*>(pDynEntity->GetSkeleton());
//
// 	if (pSkel==NULL && GetFragInst())
// 	{
// 		if (!GetFragInst()->GetCached() && FRAGCACHEMGR->GetNumFreeCacheEntries() > 5)
// 		{
// 			GetFragInst()->PutIntoCache();
// 			pDynEntity->CreateSkeleton();
// 		}
//
// 		pSkel = GetFragInst()->GetSkeleton();
// 		//Assert(pSkel);
// 	}
//
// 	if (pSkel)
// 	{
// 		// get the local matrix of this entity bone
// 		int boneIndex = -1;
// 		pSkel->GetSkeletonData().ConvertBoneIdToIndex((u16)sw->m_boneTag, boneIndex);
// 		Matrix34* pLocalMtx = &RC_MATRIX34(pSkel->GetLocalMtx(boneIndex));
//
// 		// get the original matrix of this entity bone
// 		CBaseModelInfo* pBaseModel = GetBaseModelInfo();
// 		const Matrix34* pOrigLocalMtx = &RCC_MATRIX34(pBaseModel->GetFragType()->GetSkeletonData().GetDefaultTransform(boneIndex));
//
// 		// apply wind to the local matrix
// 		Vector3 windVel;
// 		WIND.GetLocalVelocity(GetTransform().GetPosition(), RC_VEC3V(windVel));
// 		float windSpeed = windVel.Mag()/WEATHER_WIND_SPEED_MAX;
// 		windSpeed = MIN(windSpeed, 1.0f);
// 		windSpeed = MAX(windSpeed, 0.0f);
//
// #if __BANK
// 		if (SWAYABLE_WIND_SPEED_OVERRIDE>0.0f)
// 		{
// 			windSpeed = SWAYABLE_WIND_SPEED_OVERRIDE;
// 		}
// #endif
//
// 		if (windSpeed>0.0f)
// 		{
// 			float swayableSpeed = sw->m_swaySpeedLo + ((sw->m_swaySpeedHi-sw->m_swaySpeedLo)*windSpeed);
// 			float swayableAmplitude = sw->m_swayAmplitudeLo + ((sw->m_swayAmplitudeHi-sw->m_swayAmplitudeLo)*windSpeed);
//
// #if __BANK
// 			if (SWAYABLE_WIND_SPEED_OVERRIDE>0.0f)
// 			{
// 				swayableSpeed = SWAYABLE_LO_SPEED + ((SWAYABLE_HI_SPEED-SWAYABLE_LO_SPEED)*windSpeed);
// 				swayableAmplitude = SWAYABLE_LO_AMPLITUDE + ((SWAYABLE_HI_AMPLITUDE-SWAYABLE_LO_AMPLITUDE)*windSpeed);
// 			}
// #endif
//
// 			// using update the sway timer variable (union with m_fFloaterHeight, so beware)
// 			const float lastPhase = uMiscData.m_fSwayTimer+GetRandomSeed();
// 			uMiscData.m_fSwayTimer += fwTimer::GetTimeStep()*swayableSpeed;
// 			const float theta = rage::Sinf(uMiscData.m_fSwayTimer+GetRandomSeed());
//
// 			// move up in the z for testing
// 			pLocalMtx->a = pOrigLocalMtx->a;
// 			pLocalMtx->b = pOrigLocalMtx->b;
// 			pLocalMtx->c = pOrigLocalMtx->c;
// 			pLocalMtx->RotateLocalX(theta*windSpeed*swayableAmplitude);
//
// 		/*	char buf[255];
// 			formatf(buf, sizeof(buf)-1, "theta: %f windSpeed: %f swayableSpeed: %f swayableAmplitude: %f", theta, windSpeed, swayableSpeed, swayableAmplitude);
// 			Vector3 pos = pSkel->GetGlobalMtx(boneIndex).d;
// 			grcDebugDraw::Text(pos.x, pos.y, pos.z, Color32(255,0,0), buf);*/
//
// 			if(GetRandomSeed() < 0x5555 && swayableAmplitude > 0.32f*0.5f && theta <= 0.9f && theta > 0.f)
// 			{
// 				const float lastTheta = rage::Sinf(lastPhase);
// 				if(lastTheta > 0.9f)
// 				{
// 					Matrix34 mBone;
// 					pSkel->GetGlobalMtx(boneIndex, RC_MAT34V(mBone));
//
// 					audSoundInitParams initParams;
// 					initParams.EnvironmentGroup = GetAudioEnvironmentGroup();
// 					initParams.Position = mBone.d;
// 					dev_float swayableAmplitudeVolScalar = 1.f / 0.32f;
// 					initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(Min(1.f, swayableAmplitude * swayableAmplitudeVolScalar));
// 					g_AmbientAudioEntity.CreateAndPlaySound("AMBIENT_CREAKING_TRAFFIC_LIGHT", &initParams);
// 				}
// 			}
// 			//m_AudioEntity.UpdateCreakSound(GetPosition(), theta, swayableAmplitude*windSpeed);
// 		}
// 	}
// }

void CObject::SetActivePoseFromSkel()
{
	// At this point the skeleton is set to the target animation, give the articulated body this target animation (which we hope it will be able to physically drive to)
	fragInst* pFragInst = GetFragInst();
	if(pFragInst)
	{
		pFragInst->SetMusclesFromSkeleton(*GetSkeleton());

		// Now update the bounds with the skeleton (which is currently the target animation)
		pFragInst->PoseBoundsFromSkeleton(true, true);
		// and then modify the bounds for any parts that are attached to the articulated body (some or all of the animation may be driving articulation vs setting bone positions)
		pFragInst->PoseBoundsFromArticulatedBody();
		// next step is that the physics now kicks in, runs it's simulation, and then updates the articulated body and updates the attached bounds based on that
	}
}

// Get the lock on position for this object
void CObject::GetLockOnTargetAimAtPos(Vector3& aimAtPos) const 
{
	const CTunableObjectEntry* pTuneEntry = CTunableObjectManager::GetInstance().GetTuningEntry(GetArchetype()->GetModelNameHash());
	if (pTuneEntry && pTuneEntry->GetUsePositionForLockOnTarget())
	{
		aimAtPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	}
	else
	{
		GetBoundCentre(aimAtPos); 
	}
}

bool CObject::ShouldFixIfNoCollisionLoadedAroundPosition()
{
	// B*2755032: Allow parachute objects of script peds to be frozen waiting on collision as well
	bool bIsParachuteOfScriptPed = false;
	if (m_bIsParachute && NetworkInterface::IsGameInProgress())
	{
		const fwEntity* pChild = GetChildAttachment();
		if (pChild && pChild->GetType() == ENTITY_TYPE_PED)
		{
			const CPed* pAttachedPed = static_cast<const CPed*>(pChild);
			if (pAttachedPed && pAttachedPed->PopTypeIsMission())
			{
				bIsParachuteOfScriptPed = true;
			}
		}
	}

	return ((GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT) || bIsParachuteOfScriptPed) && GetAllowFreezeWaitingOnCollision();
}

void CObject::OnFixIfNoCollisionLoadedAroundPosition( bool bFix )
{
	if( bFix )
	{
		DeActivatePhysics();

		if(!GetIsOnSceneUpdate())
		{
			AddToSceneUpdate();
		}
	}
	else
	{
		if (m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen)
		{
			if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel() )
			{
				// If we activated frag types they might fall to the ground and break apart
				// so we place them on the ground using a probe
				if(GetCurrentPhysicsInst()->GetClassType() >= PH_INST_FRAG_GTA)
				{
                    if( !GetAttachParent() ) // if the object is attached to something else then don't place it on the ground
                    {
					    PlaceOnGroundProperly();
                    }
				}
				else
				{
					ActivatePhysics();
				}
			}
		}
	}
}

void CObject::SetupMissionState()
{
	CPhysical::SetupMissionState();

	SetOwnedBy( ENTITY_OWNEDBY_SCRIPT );
	ClearRelatedDummy();

	// Immediately check the object for cover point generation, so any coverpoints are included
	// straight away rather than in the periodic update
	CCover::AddCoverPointFromObject(this);

	// remove and read so that any audio effects use the correct position
	TellAudioAboutAudioEffectsRemove();
	TellAudioAboutAudioEffectsAdd();

	REPLAY_ONLY(CReplayMgr::RecordObject(this));
}

void CObject::CleanupMissionState()
{
	if (scriptVerifyf(GetOwnedBy() != ENTITY_OWNEDBY_RANDOM, "Calling CleanupMissionState on an ambient object (0x%p) (%s)", this, GetNetworkObject() ? GetNetworkObject()->GetLogName() : "-none-"))
	{
		bool bObjectGrabbedFromTheWorldByScript = false;
		if (GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
		{
			CScriptEntityExtension* pExtension = GetExtension<CScriptEntityExtension>();
			if (Verifyf(pExtension, "CObject::CleanupMissionState - The object is not a script entity"))
			{
				if (pExtension->GetScriptObjectWasGrabbedFromTheWorld())
				{
					s32 ScriptGUID = CTheScripts::GetGUIDFromEntity(*this);
					bObjectGrabbedFromTheWorldByScript = CTheScripts::GetHiddenObjects().RemoveFromHiddenObjectMap(ScriptGUID, true);
				}
			}
		}

		CPhysical::CleanupMissionState();

		// have to do this after CPhysical::CleanupMissionState or the owned by value will get overwritten
 		if (bObjectGrabbedFromTheWorldByScript || IsADoor())
 		{
 			SetOwnedBy( ENTITY_OWNEDBY_RANDOM );
 			scriptAssertf(GetIsAnyManagedProcFlagSet()==false, "Trying to turn a procedural object into a ENTITY_OWNEDBY_RANDOM");
 		}
 		else
		{
			SetOwnedBy( ENTITY_OWNEDBY_TEMP );	//	ENTITY_OWNEDBY_RANDOM;
			m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() + 1000000; // 16mins
			ms_nNoTempObjects++;

			scriptAssertf(GetRelatedDummy() == NULL, "CleanupMissionState - object has a dummy");
			scriptAssertf(GetIsAnyManagedProcFlagSet()==false, "Trying to turn a procedural object into a ENTITY_OWNEDBY_TEMP");
		}
	}
}

CEntity *CObject::GetPointerToClosestMapObjectForScript(float NewX, float NewY, float NewZ, float Radius, int ObjectModelHashKey, s32 TypeFlags /*= ENTITY_TYPE_MASK_BUILDING|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT*/, s32 OwnedByFlags)
{
	if (NewZ <= INVALID_MINIMUM_WORLD_Z)
	{
		NewZ = WorldProbe::FindGroundZForCoord(TOP_SURFACE, NewX, NewY);
	}
	fwModelId objectModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) ObjectModelHashKey, &objectModelId);		//	ignores return value
	//scriptAssertf( (ObjectModelIndex >= 0) && (ObjectModelIndex < NUM_MODEL_INFOS), "%s:GetPointerToClosestMapObject - this is not a valid model index", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		ClosestObjectDataStruct ClosestObjectData;
		ClosestObjectData.fClosestDistanceSquared = Radius * Radius * 4.0f;	//	set this to larger than the scan range (remember it's squared distance)
		ClosestObjectData.pClosestObject = NULL;

		ClosestObjectData.CoordToCalculateDistanceFrom = Vector3(NewX, NewY, NewZ);

		ClosestObjectData.ModelIndexToCheck = objectModelId.GetModelIndex();
		ClosestObjectData.bCheckModelIndex = true;
		ClosestObjectData.bCheckStealableFlag = false;

		ClosestObjectData.nOwnedByMask = OwnedByFlags; 

		//	used to do a 2d distance check with FindObjectsOfTypeInRange
		spdSphere testSphere(RCC_VEC3V(ClosestObjectData.CoordToCalculateDistanceFrom), ScalarV(Radius));
		fwIsSphereIntersecting searchSphere(testSphere);
		CGameWorld::ForAllEntitiesIntersecting(&searchSphere, CTheScripts::GetClosestObjectCB, (void*) &ClosestObjectData,
			TypeFlags, (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS),
			SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_SCRIPT);

	return ClosestObjectData.pClosestObject;
}

bool CObject::NeedsProcessPostCamera() const
{
	return m_bDisableObjectMapCollision || m_bForcePostCameraAiUpdate || m_bForcePostCameraAnimUpdate || (m_pIntelligence != NULL);
}

void CObject::AddToOrRemoveFromProcessPostCameraArray()
{
	if(NeedsProcessPostCamera())
	{
		if(ms_ObjectsThatNeedProcessPostCamera.Find(this) == -1)
		{
			ms_ObjectsThatNeedProcessPostCamera.PushAndGrow(this);
		}
	}
	else
	{
		int processPostCameraIndex = ms_ObjectsThatNeedProcessPostCamera.Find(this);
		if(processPostCameraIndex != -1)
		{
			ms_ObjectsThatNeedProcessPostCamera.DeleteFast(processPostCameraIndex);
			Assert(ms_ObjectsThatNeedProcessPostCamera.Find(this) == -1);
		}
	}
}

void CObject::ProcessPostCamera()
{
	// Flag reset, needs to happen before InstantAiUpdate().
	SetDisableObjectMapCollision(false);

	if (m_bForcePostCameraAiUpdate)
	{
		InstantAiUpdate();
		m_bForcePostCameraAiUpdate = false;
		AddToOrRemoveFromProcessPostCameraArray();
	}
	
	if (m_bForcePostCameraAnimUpdate)
	{
		InstantAnimUpdate();
		m_bForcePostCameraAnimUpdate = false;
		m_bForcePostCameraAnimUpdateUseZeroTimeStep = false;
		AddToOrRemoveFromProcessPostCameraArray();
	}	

	if (m_pIntelligence)
	{
		m_pIntelligence->ProcessPostCamera();
	}
}

void CObject::ProcessSniperDamage()
{
	if ((GetRelatedDummy() && camInterface::GetGameplayDirector().IsFirstPersonAiming()) REPLAY_ONLY(|| CReplayMgr::IsReplayInControlOfWorld()) )
	{

		if (m_damageScanCode==INVALID_DAMAGE_SCANCODE)	// newly damaged, so increment scan code
		{

			if ( Unlikely(ms_globalDamagedScanCode==MAX_UINT32) )
			{
				// deal with wraparound - extremely unlikely and rare
				ms_globalDamagedScanCode = INVALID_DAMAGE_SCANCODE+1;
				m_damageScanCode = ms_globalDamagedScanCode;

				CObject::Pool* pObjectPool = CObject::GetPool();
				s32 i = (s32) pObjectPool->GetSize();
				CObject* pObject = NULL;
				while(i--)
				{
					pObject = pObjectPool->GetSlot(i);
					if(pObject && pObject->m_damageScanCode!=INVALID_DAMAGE_SCANCODE)
					{
						pObject->m_damageScanCode = ms_globalDamagedScanCode;
					}
				}
			}
			else
			{
				m_damageScanCode = ++ms_globalDamagedScanCode;
			}

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketObjectSniped>(
					CPacketObjectSniped(), this);
			}
#endif // GTA_REPLAY

		}
		else
		{
			// already been shot, so push to top of list but don't increment scan code
			Assert(ms_globalDamagedScanCode != INVALID_DAMAGE_SCANCODE);
			m_damageScanCode = ms_globalDamagedScanCode;
		}

	}
}

void CObject::ScaleObject(float fScale)
{
	// increase LOD distance for scaled pickups
	if (fScale > 1.0f)
	{
		if (GetLodData().IsOrphanHd())
		{
			float fModelLodDist = GetBaseModelInfo()->GetLodDistanceUnscaled();
			SetLodDistance((u16) (4 * fModelLodDist) );
		}

		#if ENABLE_MATRIX_MEMBER
		Mat34V trans = GetMatrix();		
		SetTransform(trans);
		SetTransformScale(fScale,fScale);
		#else
		fwMatrixScaledTransform* trans = rage_new fwMatrixScaledTransform(GetMatrix());
		trans->SetScale(fScale, fScale);
		SetTransform(trans);
		#endif
	}
}

const Vector3& CObject::GetVelocity() const
{
	u32 objectNameHash = GetBaseModelInfo()->GetModelNameHash();
	if( objectNameHash == MI_ROLLER_COASTER_CAR_1.GetName().GetHash() || 
		objectNameHash == MI_ROLLER_COASTER_CAR_2.GetName().GetHash() )
	{
		return ms_RollerCoasterVelocity;
	}
	else
	{
		return CPhysical::GetVelocity();
	}
}

CCustomShaderEffectTint* CObject::GetCseTintFromObject()
{
	if( GetDrawHandlerPtr() )
	{
		fwCustomShaderEffect *fwCSE = GetDrawHandler().GetShaderEffect();
		if(fwCSE && fwCSE->GetType()==CSE_TINT)
		{
			return static_cast<CCustomShaderEffectTint*>(fwCSE);
		}
	}
	return NULL;
}

bool CObject::ShouldAvoidCollision(const CPhysical* pOtherPhysical)
{
	// Check if we wanted to avoid collision on this timeslice
	int currentPhysicsTimeSlice = CPhysics::GetTimeSliceId();
	if(currentPhysicsTimeSlice <= m_nPhysicsTimeSliceToAvoidCollision)
	{
		// In multiplayer games do not avoid collisions with the local player. This fixes an easy to reproduce issue where
		//  a remote player can stand on top of an object that streams in and the local player walks through it when approaching it.
		// Ideally we would have a list of objects that are initially colliding with this CObject but that requires more memory. 
		// This would only cause problems if objects are streaming in on top of the player which shouldn't be happening. 
		bool avoidCollisionWithPhysical = false;
		if(pOtherPhysical)
		{
			if(pOtherPhysical->GetIsTypeVehicle())
			{
				avoidCollisionWithPhysical = !(NetworkInterface::IsGameInProgress() && static_cast<const CVehicle*>(pOtherPhysical)->ContainsLocalPlayer());
			}
			if(pOtherPhysical->GetIsTypePed())
			{
                const CPed *pPed = static_cast<const CPed*>(pOtherPhysical);

                // don't avoid collisions with remote peds either - this can cause them to intersect the object they are standing on
                // if the local player warps to the area and the network blender will correct things in this case too
				avoidCollisionWithPhysical = !(NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer()) && !pPed->IsNetworkClone();
			}
		}

		if(avoidCollisionWithPhysical)
		{
			if(const fragInst* pFragInst = pOtherPhysical->GetFragInst())
			{
				if(const fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry())
				{
					// Only avoid collision if the other object has been around for a bit. In cases where a user spawns an object on top of another object we want them to collide. 
					dev_float minimumAgeToAvoidCollision = 1.0f;
					if(pFragCacheEntry->GetHierInst()->age > minimumAgeToAvoidCollision)
					{
						// If the other object is something which we want to avoid collision with, don't collide with it and avoid collision on the next timeslice as well
						m_nPhysicsTimeSliceToAvoidCollision = currentPhysicsTimeSlice + 1;
						return true;
					}
				}
			}
		}
	}
	return false;
}

void CObject::ProcessBallCollision( CEntity* pHitEnt, const Vector3& vMyHitPos,	float fImpulseMag, const Vector3& vMyNormal )
{
	if( !pHitEnt ||
		( pHitEnt->GetIsAnyFixedFlagSet() &&
		  !m_bExtraBounceApplied ) )
	{
		static dev_float sfimpulseMagScale = 20.0f;
		Vector3 extraImpulse = vMyNormal * fImpulseMag * sfimpulseMagScale;

		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, sfBallBounceScale, 1.5f, 0.0f, 2.0f, 0.1f );
		extraImpulse.z *= sfBallBounceScale;

		float impulseMag = extraImpulse.Mag();
		extraImpulse.Normalize();
		impulseMag = Clamp( impulseMag, -259.0f * GetMass(), 259.0f * GetMass() );

		ApplyForceCg( extraImpulse * impulseMag );

		m_bExtraBounceApplied = true;
	}
	else if( pHitEnt &&
			 pHitEnt->GetIsTypeVehicle() &&
			 !m_bBallShunted )
	{
		CVehicle* pVehicle = static_cast<CVehicle*>( pHitEnt );
		Vector3 extraImpulse = pVehicle->GetLocalSpeed( vMyHitPos, true );
		extraImpulse -= GetVelocity();
		float impluseMag = extraImpulse.Mag();
		extraImpulse.Normalize();
		Vector3 normalisedVelocity = extraImpulse;


		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, sfimpulseVehicleMagScale, 2.0f, 0.0f, 100.0f, 0.1f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, sfMaxVehicleMass, 3000.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, sfMinVehicleMass, 1000.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, sfMaxSpeed, 35.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, shuntImpulseScale, 5.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, forwardShuntImpulseScale, 10.0f, 0.0f, 10000.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, amountOfVelocityVectorToUse, 0.6f, 0.0f, 1.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, amountOfNormalVectorToUse, 0.4f, 0.0f, 1.0f, 1.0f );
		TUNE_GROUP_FLOAT( ARENA_DYNAMIC_PROPS, extraUpAngle, 0.15f, -1.0f, 1.0f, 1.0f );
		float vehicleMass = Clamp( pVehicle->GetMass(), sfMinVehicleMass, sfMaxVehicleMass );

		float maxSpeed = sfMaxSpeed;
		float magScale = sfimpulseVehicleMagScale;

		//if( pVehicle->GetJumpState() == CVehicle::FORWARD_JUMP )
		//{
		//	static dev_float forwardJumpSpeedScale = 1.5f;
		//	static dev_float forwardJumpMagScale = 2.5f;
		//	maxSpeed *= forwardJumpSpeedScale;
		//	magScale *= forwardJumpMagScale;
		//}

		//Make side shunts apply a larger force
		if( pVehicle->GetSideShuntForce() != 0.0f )
		{
			Vector3 shuntVec = VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetRight() ) * pVehicle->GetSideShuntForce();
			float shuntMag = shuntVec.Dot( vMyNormal );
			shuntMag = Max( 0.0f, shuntMag );

			if( shuntMag > 0.0f )
			{
				Vector3 shuntImpulse = vMyNormal * shuntMag;
				float massScale = Clamp( pVehicle->GetMass(), sfMinVehicleMass, sfMaxVehicleMass ) / ( sfMaxVehicleMass - sfMinVehicleMass );
				shuntImpulse *= massScale * GetMass() * shuntImpulseScale;

				float impulseMag = shuntImpulse.Mag();
				shuntImpulse.Normalize();
				impulseMag = Clamp( impulseMag, -259.0f * GetMass(), 259.0f * GetMass() );

				ApplyForceCg( shuntImpulse * impulseMag );
			}
		}

		//if( pVehicle->GetForwardShuntForce() > 0.0f ||
		//	pVehicle->GetJumpState() == CVehicle::FORWARD_JUMP )
		//{
		//	Vector3 shuntVec = VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetForward() ) * pVehicle->GetForwardShuntForce();
		//	float shuntMag = shuntVec.Dot( vMyNormal );
		//	shuntMag = Max( 0.0f, shuntMag );

		//	if( shuntMag > 0.0f )
		//	{
		//		Vector3 shuntImpulse = vMyNormal * shuntMag;
		//		float massScale = Clamp( pVehicle->GetMass(), sfMinVehicleMass, sfMaxVehicleMass ) / ( sfMaxVehicleMass - sfMinVehicleMass );
		//		shuntImpulse *= massScale * GetMass() * forwardShuntImpulseScale;

		//		float impulseMag = shuntImpulse.Mag();
		//		shuntImpulse.Normalize();
		//		impulseMag = Clamp( impulseMag, -259.0f * GetMass(), 259.0f * GetMass() );

		//		ApplyForceCg( shuntImpulse * impulseMag );
		//	}
		//}

		extraImpulse = ( extraImpulse * amountOfVelocityVectorToUse ) + ( vMyNormal * amountOfNormalVectorToUse );
		extraImpulse.z += extraUpAngle;

		impluseMag = Clamp( impluseMag, -maxSpeed, maxSpeed );
		extraImpulse *= vehicleMass * magScale * Max( 0.0f, vMyNormal.Dot( normalisedVelocity ) ) * impluseMag;

		float impulseMag = extraImpulse.Mag();
		extraImpulse.Normalize();
		impulseMag *= fwTimer::GetRagePhysicsUpdateTimeStep();
		impulseMag = Clamp( impulseMag, -150.0f * GetMass(), 150.0f * GetMass() );

		ApplyImpulseCg( extraImpulse * impulseMag );

		m_bBallShunted = true;
	}
}

void CObject::SetObjectIsBall( bool isBall )
{ 
	//TUNE_GROUP_FLOAT( ARENA_MODE, BALL_DAMPING_V, 0.002f, 0.0f, 1.0f, 0.1f );
	//TUNE_GROUP_FLOAT( ARENA_MODE, BALL_DAMPING_V2, 0.006f, 0.0f, 1.0f, 0.1f );

	//GetPhysArch()->ActivateDamping( phArchetypeDamp::LINEAR_V, Vector3( BALL_DAMPING_V, BALL_DAMPING_V, BALL_DAMPING_V ) );
	//GetPhysArch()->ActivateDamping( phArchetypeDamp::LINEAR_V2, Vector3( BALL_DAMPING_V2, BALL_DAMPING_V2, BALL_DAMPING_V2 ) );

	m_bIsBall = isBall; 
}



