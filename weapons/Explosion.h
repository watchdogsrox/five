#ifndef _EXPLOSION_H_
#define _EXPLOSION_H_

// Rage headers
#include "atl/array.h"
#include "fragment/typechild.h"
#include "fragment/typegroup.h"
#include "vector/vector3.h"
#include "parser/macros.h"
#include "physics/collider.h"
#include "physics/instbehavior.h"
#include "phbound/boundcapsule.h"
#include "weapons/explosioninst.h"

// Game headers
#include "physics/WorldProbe/worldprobe.h"
#include "scene/entity.h"
#include "scene/RegdRefTypes.h"
#include "Weapons/WeaponTypes.h"
#include "vehicles/wheel.h"

// Forward declarations
class phInstGta;
class phGtaExplosionInst;
class CProjectile;

//---------------------------------------------
//  CExplosionTagData
//  Parsable instance of a type of explosion
//  Data can be found in explosion.meta
//---------------------------------------------
class CExplosionTagData
{
public:

	CExplosionTagData();

	atString		name;

	float  			damageAtCentre;
	float  			damageAtEdge;
	float  			networkPlayerModifier;
	float  			networkPedModifier;

	float			endRadius;
	float			initSpeed;
	float			decayFactor;
	float			forceFactor;
	float			fRagdollForceModifier;
	float			fSelfForceModifier;

	float			directedWidth;
	float			directedLifeTime;

	float			camShake;
	float			camShakeRollOffScaling;

	float			shockingEventVisualRangeOverride;
	float			shockingEventAudioRangeOverride;

	float			fragDamage;

	bool			minorExplosion;

	bool			bAppliesContinuousDamage;
	bool			bPostProcessCollisionsWithNoForce;
	bool			bDamageVehicles;
	bool			bDamageObjects;

	bool			bOnlyAffectsLivePeds;
	bool			bIgnoreExplodingEntity;
	bool			bAlwaysAllowFriendlyFire;
	bool			bNoOcclusion;

	bool			explodeAttachEntityWhenFinished;

	bool			bCanSetPedOnFire;
	bool			bCanSetPlayerOnFire;
	bool			bSuppressCrime;
	bool			bUseDistanceDamageCalc;
	bool			bPreventWaterExplosionVFX;
	bool			bIgnoreRatioCheckForFire;
	bool			bAllowUnderwaterExplosion;
	bool			bForceVehicleExplosion;

	float			midRadius;
	float  			damageAtMid;

	bool			bApplyVehicleEMP;
	bool			bApplyVehicleSlick;
	bool			bApplyVehicleSlowdown;
	bool			bApplyVehicleTyrePop;
	bool			bApplyVehicleTyrePopToAllTyres;

	bool			bForcePetrolTankDamage;

	bool			bIsSmokeGrenade;
	bool			bForceNonTimedVfx;

	bool			bDealsEnduranceDamage;
	bool			bApplyFlashGrenadeEffect;
	bool			bEnforceDamagedEntityList;
	bool			bAlwaysCollideVehicleOccupants;

	u32				GetCamShakeNameHash() const	{	return camShakeName.GetHash();	}
	u32				GetVFXTagHash() const		{	return vfxTagHashName.GetHash();	}

private:

	atHashString	camShakeName;
	atHashString	vfxTagHashName;
	
	PAR_SIMPLE_PARSABLE;
};

// Defines
// max explosions active at any given time
#define EXP_MAX_BROKEN_FRAG_INFOS	(128)

class phGtaExplosionCollisionInfo
{
public:
	phInst *m_OtherInst;
	Vec3V m_HitPos;
	Vec3V m_ForceDir;
	ScalarV m_ForceMag;
	ScalarV m_UncappedForceMag;
	int m_Component;
	bool m_bSelfCollision;
};

//-------------------------------------------------------------------------
// Explosion behaviour derived from basic rage explosion behaviour
// to implement virtual functions for explosion responses
//-------------------------------------------------------------------------
class CVehicle;

class phInstBehaviorExplosionGta : public  phInstBehavior
{
public:
	phInstBehaviorExplosionGta();
	~phInstBehaviorExplosionGta();

	virtual void Reset();

	virtual bool IsActive() const;

	virtual void Update(float TimeStep);

	virtual bool CollideObjects(Vec::V3Param128 timeStep, phInst* myInst, phCollider* myCollider, phInst* otherInst, phCollider* otherCollider, phInstBehavior* otherInstBehavior);

	static void ProcessAllCollisions();

	void AddPedsAttachedToCar( CVehicle* pVehicle, bool bShouldCheckIfPedIgnoresExplosions = false );
	virtual bool ActivateWhenHit() const;

	// Explosion specific accessors.
	float GetWidth() const;
	float GetRadius() const;
	ScalarV_Out GetRadiusV() const;
	float GetRadiusSpeed() const;

	void SetExplosionInst(phGtaExplosionInst *ExplosionInst);

	// <COMBINE: phInstBehavior::IsForceField>
	virtual bool IsForceField () const { return true; }

	typedef std::pair<phInstBehaviorExplosionGta*, phInst*> CollidePair;

	void ComputeLosOffset();
	static bank_float m_LosOffsetToInst;
	static bank_float m_LosOffsetAlongUp;
protected:

	bool DoOcclusionTest(const CEntity* pEntity);

	void ProcessCollisionsForOneFrame(CollidePair* pairs, int pairCount, WorldProbe::CShapeTestResults& shapeTestResults);

	bool TestAgainstSphere(Vector3::Param localCenter, Vector3::Param radius);

	void AddCollidedEntity(CEntity* pEntity, phInst* pInst);

	void AddCollidedVehicleOccupantsToList(CEntity* pEntity, bool bForceAddVehicleOccupants = false);
	
	static const s32 MAX_INTERSECTIONS = 16;
	virtual bool FindIntersections( phInst *OtherInst, phIntersection* aIntersections, s32& iNumIntersections, const s32 iMaxIntersections );
	virtual bool FindIntersectionsOriginal(phInst *OtherInst, Vec3V_InOut otherInstLosPosition, WorldProbe::CShapeTestResults& refResults, const s32 iMaxIntersections, bool ignoreBoundOffset = false);

	// Derived classes might want to modify or adjust the force before it gets applied.
	virtual void PreApplyForce(phGtaExplosionCollisionInfo &collisionInfo, eExplosionTag explosionTag) const;

	// Derived classes might want to use this to do game-specific things like trigger damage.
	virtual void PostCollision(phGtaExplosionCollisionInfo& collisionInfo) const;
	
	void ApplyExplosionImpulsesToAnimatedPed(CPed *pPed, Vec3V_In vecApplyForceIn, Vec3V_In vecExplCenter);

	Vec3V m_LosOffset;
	float m_Radius, m_RadiusSpeed, m_Width; // width used by directed explosions
	float m_ExplosionTimer;
	static float m_MagClamp;
	static float m_pitchTorqueMin;
	static float m_pitchTorqueMax;
	static float m_NMBodyScale;
	static float m_sideScale;
	static float m_twistTorqueMax;
	static float m_blanketForceScale;

	phGtaExplosionInst *m_ExplosionInst;

	static CollidePair ms_CollidedInsts[];
	static int ms_NumCollidedInsts;

};


//-------------------------------------------------------------------------
// Class used to cache smashables that might be hit by an explosion
// saves searching for smashables each frame as the explosion grows
//-------------------------------------------------------------------------
class CCachedSmashables
{
public:
	CCachedSmashables();
	~CCachedSmashables();

	void CacheSmashablesAffectedByExplosion( const phGtaExplosionInst* pExplosionInst, const Vector3& vCentre );
	void UpdateCachedSmashables( const Vector3& vCentre, const float speedRatio, const float fNewRadius, const float fForceFactor );
	void Reset();

private:
	void TryToReplaceFurthestEntity(CEntity* pEntity, const Vector3& vCentre);
	enum {MAX_EXPLOSION_SMASHABLES = 8};
	RegdEnt m_apPossibleEntities[MAX_EXPLOSION_SMASHABLES];
};

#define DEFAULT_EXPLOSION_SCALE 1.0f

//
//
//
class CExplosionManager
{
public:

	// struct passed in to AddExplosion and CRequestExplosionEvent, to avoid a large parameter list. 
	class CExplosionArgs
	{
	public:

		CExplosionArgs(eExplosionTag explosionTag, const Vector3 &explosionPosition) :
			m_explosionTag(explosionTag),
			m_explosionPosition(explosionPosition),
			m_pExplodingEntity(NULL),
			m_pEntExplosionOwner(NULL),
			m_pEntIgnoreDamage(NULL),
			m_activationDelay(0), 
			m_sizeScale(DEFAULT_EXPLOSION_SCALE),
			m_camShakeNameHash(0),
			m_fCamShake(-1.0f), 
			m_fCamShakeRollOffScaling(-1.0f),
			m_bInAir(false), 
			m_bMakeSound(true),
			m_bNoFx(false),
			m_vDirection(Vector3(0.0f, 0.0f, 1.0f)),
			m_pAttachEntity(NULL),
			m_attachBoneTag(0),
			m_originalExplosionTag(EXP_TAG_DONTCARE),
			m_vfxTagHash(0),
			m_pRelatedDummyForNetwork(NULL),
			m_bIsLocalOnly(false),
			m_bNoDamage(false),
			m_bAttachedToVehicle(false),
			m_bDetonatingOtherPlayersExplosive(false),
			m_weaponHash(WEAPONTYPE_EXPLOSION),
			m_bDisableDamagingOwner(false)
		{
		}

		CExplosionArgs(CExplosionArgs& args) :
			m_explosionTag(args.m_explosionTag),
			m_explosionPosition(args.m_explosionPosition),
			m_pExplodingEntity(args.m_pExplodingEntity),
			m_pEntExplosionOwner(args.m_pEntExplosionOwner),
			m_pEntIgnoreDamage(args.m_pEntIgnoreDamage),
			m_activationDelay(args.m_activationDelay), 
			m_sizeScale(args.m_sizeScale),
			m_camShakeNameHash(args.m_camShakeNameHash),
			m_fCamShake(args.m_fCamShake), 
			m_fCamShakeRollOffScaling(args.m_fCamShakeRollOffScaling),
			m_bMakeSound(args.m_bMakeSound),
			m_bNoFx(args.m_bNoFx),
			m_bInAir(args.m_bInAir), 
			m_vDirection(args.m_vDirection),
			m_pAttachEntity(args.m_pAttachEntity),
			m_attachBoneTag(args.m_attachBoneTag),
			m_originalExplosionTag(args.m_originalExplosionTag),
			m_vfxTagHash(args.m_vfxTagHash),
			m_weaponHash(args.m_weaponHash),
			m_pRelatedDummyForNetwork(args.m_pRelatedDummyForNetwork),
			m_networkIdentifier(args.m_networkIdentifier),
			m_bIsLocalOnly(args.m_bIsLocalOnly),
			m_bNoDamage(args.m_bNoDamage),
			m_bAttachedToVehicle(args.m_bAttachedToVehicle),
			m_bDetonatingOtherPlayersExplosive(args.m_bDetonatingOtherPlayersExplosive),
			m_bDisableDamagingOwner(args.m_bDisableDamagingOwner),
            m_interiorLocation(args.m_interiorLocation)
		{
		}

#if __BANK
		void				AddWidgets(bkBank & bank);
#endif // __BANK

		eExplosionTag		m_explosionTag;
		Vector3				m_explosionPosition;
		RegdEnt				m_pExplodingEntity;
		RegdEnt				m_pEntExplosionOwner;
		RegdEnt				m_pEntIgnoreDamage;
		u32					m_activationDelay; 
		float				m_sizeScale;
		u32					m_camShakeNameHash;
		float				m_fCamShake; 
		float				m_fCamShakeRollOffScaling;
		bool				m_bMakeSound;
		bool				m_bNoFx;
		bool				m_bInAir;
		Vector3				m_vDirection;
		RegdEnt				m_pAttachEntity;
		s32					m_attachBoneTag;
		eExplosionTag		m_originalExplosionTag;
		u32					m_vfxTagHash;
		u32					m_weaponHash;
		fwInteriorLocation	m_interiorLocation;

		// network stuff:
		class CDummyObject*	m_pRelatedDummyForNetwork; 
		CNetFXIdentifier	m_networkIdentifier;
		bool				m_bIsLocalOnly;
		bool				m_bNoDamage;
		bool				m_bAttachedToVehicle;
		bool				m_bDetonatingOtherPlayersExplosive;
		bool				m_bDisableDamagingOwner;
	};


public:
	enum {MAX_SPHERE_EXPLOSIONS = 6, MAX_DIRECTED_EXPLOSIONS = 12};
	enum {MAX_EXPLOSIONS = (MAX_SPHERE_EXPLOSIONS + MAX_DIRECTED_EXPLOSIONS)};
	enum {ABSOLUTE_MAX_ACTIVATED_OBJECTS = 18};

	enum eFlashGrenadeStrength
	{
		NoEffect = 0,
		StrongEffect,
		WeakEffect
	};

	// Cloud tunables
	static void InitTunables(); 

	static void Init();
	static void Init(unsigned initMode);
	static void Shutdown();
	static void Shutdown(unsigned shutdownMode);

	static void Update(float fTimeStep);
	
	// Find explosions
	static phGtaExplosionInst* FindFirstExplosionInAngledArea(const eExplosionTag TestExplosionTag, Vector3& vPos1, Vector3& vPos2, float areaWidth);
	static phGtaExplosionInst* FindFirstExplosionInSphere(const eExplosionTag TestExplosionTag, Vector3& vCentre, float fRadius);

	// Fill query list of explosion instances by tag
	static int FindExplosionsByTag(const eExplosionTag TestExplosionTag, phGtaExplosionInst** out_explosionInstanceList, const int iMaxNumExplosionInstances);


	// Owner of explosions
	static CEntity* GetOwnerOfExplosionInAngledArea(const eExplosionTag TestExplosionTag, Vector3& vPos1, Vector3& vPos2, float areaWidth);
	static CEntity* GetOwnerOfExplosionInSphere(const eExplosionTag TestExplosionTag, Vector3& vCentre, float fRadius);

	// Testing for and removing explosions around particular areas, used at the start of cutscenes, on reset etc...
	static bool TestForExplosionsInArea( const eExplosionTag TestExplosionTag, Vector3& vMin, Vector3 &vMax, bool bIncludeDelayedExplosions = true );
	static bool TestForExplosionsInAngledArea( const eExplosionTag TestExplosionTag, Vector3& vPos1, Vector3& vPos2, float areaWidth);
	static bool TestForExplosionsInSphere( const eExplosionTag TestExplosionTag, Vector3& vCentre, float fRadius );
	// Returns true if any were removed
	static bool RemoveExplosionsInArea( const eExplosionTag TestExplosionTag, const Vector3& Centre, float Radius );

	//Flags all explosions for the specified entity for removal during the post sim update
	static void FlagsExplosionsForRemoval(CEntity* pEntity);

	// Removes all explosions for the specified entity
	static void ClearExplosions(CEntity* pEntity);
	// Removes all explosions
	static void	ClearAllExplosions();
	// Set explosions owned by this entity to not report crimes
	static void DisableCrimeReportingForExplosions(CEntity* pEntity);

	static bool	AddExplosion(CExplosionArgs& explosionArgs, CProjectile* pProjectile = NULL, bool bForceExplosion = false);


	static bool InsideExplosionUpdate() { return sm_InsideExplosionUpdate; }
	static void SetInsideExplosionUpdate(bool insideExplosionUpdate) { sm_InsideExplosionUpdate = insideExplosionUpdate; }

	// Determines if too many instances have already been activated by explosions to add this one
	static bool AllowActivations() { return sm_ActivatedObjects.GetCount() < sm_MaximumActivatedObjects BANK_ONLY(|| !sm_EnableLimitedActivation); }
	// Records an object as having been activated by an explosion until it deactivates
	static void RegisterExplosionActivation(u16 levelIndex);
	// Remove the cost of activating objects which are no longer active
	static void RemoveDeactivatedObjects();

	static bool AllowBreaking() { return sm_NumExplosionBreaksThisFrame < sm_MaxExplosionBreaksPerFrame; }
	static void RegisterExplosionBreak() { ++sm_NumExplosionBreaksThisFrame; }
	static void ResetExplosionBreaksThisFrame() { sm_NumExplosionBreaksThisFrame = 0; }

	static int CountExplosions();

	// used in network game:
	static void ActivateNetworkExplosion(const CNetFXIdentifier& networkIdentifier);
	static void RemoveNetworkExplosion(const CNetFXIdentifier& networkIdentifier);

	static void ExplosionImpactCallback( phInst *pInst,
		ScalarV_In fForceMag,
		ScalarV_In fUncappedForceMag,
		const float fExplosionRadius,
		Vec3V_In vForceDir,
		Vec3V_In vHitPos,
		CEntity *pExplosionOwner,
		CEntity *pExplodingEntity,
		Vec3V_InOut vExplosionPosition,
		phGtaExplosionInst* pExplosionInst,
		int iComponent );

	static void ExplosionPreModifyForce(phGtaExplosionCollisionInfo& collisionInfo, eExplosionTag explosionTag);
	static bool IsFriendlyFireAllowed(CPed* pVictimPed, phGtaExplosionInst* pExplosionInst);
	static bool IsFriendlyFireAllowed(CVehicle* pVictimVehicle, phGtaExplosionInst* pExplosionInst);

#if __BANK
	static bool ms_needToSetupRagWidgets;			//signifies if we should setup rag widgets
	static void AddWidgets();						// add debug bank widgets
	static void DebugCreateExplosion( CEntity* pEntity, const Vector3& vPosition, const Vector3& vNormal, CEntity* pAttachEntity );

	static bool ms_debugRenderExplosions;
	static bool ms_debugWeaponExplosions;
	static eExplosionTag ms_eDebugExplosionTag;
	static float ms_sizeScale;
	static bool ms_attachToObjectHit;
	static bool ms_ClampAreaApproximation;
//	static float ms_fTimer;

	// Pointer to our widget group
	static rage::bkGroup* m_pBankGroup;
#endif

	// Spawns an explosion defined by phGtaExplosionInst, the explosion can be delayed by up to iActivationDelay milliseconds
	static phInstBehaviorExplosionGta * SpawnCustomExplosion( phGtaExplosionInst *pExplosionInst, s32 iActivationDelay );

	static ePhysicalType GetPhysicalType(eExplosionTag expTag);

	static bool IsTimedExplosionInProgress(const CEntity* pEntity);

protected:
	// Spawns an explosion defined by phGtaExplosionInst, the explosion can be delayed by up to iActivationDelay milliseconds
	static phInstBehaviorExplosionGta * SpawnExplosion( phGtaExplosionInst *pExplosionInst, s32 iActivationDelay );
#if __DEV
	static void DumpExplosionInfo();
#endif // __DEV

	static bool StartExplosion( const s32 ExplosionIndex);
	static void RemoveExplosion( const s32 ExplosionIndex);

	static void ApplyPedDamage( phInst *pInst,
		CPed *pPed, 
		CEntity *pEntity, 
		CEntity *pExplosionOwner, 
		phGtaExplosionInst* pExplosionInst, 
		float fDamage, 
		ScalarV_In fForceMag, 
		Vec3V_In vForceDir, 
		Vec3V_In vHitPos,
		Vec3V_In vExplosionPosition, 
		int iComponent, 
		bool bEntityAlreadyDamaged,
		u32 weaponHash );

	static phGtaExplosionInst* CreateExplosionOfType( CEntity *pExplodingEntity, CEntity *pEntExplosionOwner, CEntity *pEntIgnoreDamage, eExplosionTag ExplosionTag, float sizeScale, const Vector3& vecExplosionPosition, u32 camShakeNameHash, float fCamShake, float fCamShakeRollOffScaling, bool bMakeSound, bool noFx, bool inAir, const Vector3& vDirection, CEntity* pAttachEntity, s32 attachBoneTag, u32 vfxTagHash, CNetFXIdentifier& networkIdentifier, bool bNoDamage, bool bAttachedToVehicle, bool m_bDetonatingOtherPlayersExplosive, u32 weaponHash, bool bbDisableDamagingOwner, fwInteriorLocation intLoc);
//	static void CreateGroundVFX( phGtaExplosionInst* pExplosionInst);
//	static void CreateFires( phGtaExplosionInst* pExplosionInst);

	static void ProcessFlashGrenadeEffect( const CPed* pPed, const bool bVehicleOccupant = false );
	static CExplosionManager::eFlashGrenadeStrength CalculateFlashGrenadeEffectStrengthOnFoot( const CPed* pPed );
	static CExplosionManager::eFlashGrenadeStrength CalculateFlashGrenadeEffectStrengthInVehicle( const CPed* pPed );
	static void TriggerFlashGrenadeEffectOnLocalPlayer( const eFlashGrenadeStrength effectStrength );
	static void TriggerTyrePopOnWheel( CWheel* affectedWheel BANK_ONLY(, bool bRenderDebug));

	static phGtaExplosionInst*	m_apExplosionInsts		[MAX_EXPLOSIONS];
	static phInstGta*			m_pExplosionPhysInsts	[MAX_EXPLOSIONS];
	static s32				m_aActivationDelay		[MAX_EXPLOSIONS];
	static CCachedSmashables	m_aCachedSmashables		[MAX_EXPLOSIONS];
	static phInstBehaviorExplosionGta * m_ExplosionBehaviors;

	static bool	sm_InsideExplosionUpdate;

	BANK_ONLY(static bool												sm_EnableLimitedActivation;)
	static atFixedArray<phHandle, ABSOLUTE_MAX_ACTIVATED_OBJECTS>	sm_ActivatedObjects;
	static bank_s32													sm_MaximumActivatedObjects;

	static int		sm_NumExplosionBreaksThisFrame;
	static bank_s32	sm_MaxExplosionBreaksPerFrame;

	// Explosion tracking.		
	static atQueue<u32, 25>		ms_recentExplosions;
	static Vector3				ms_vExplosionTrackCentre;	

	// Cloud tunables
	static float sm_fSpecialAmmoExplosiveShotgunPedLocalDamageMultiplier;
	static float sm_fSpecialAmmoExplosiveShotgunPedRemoteDamageMultiplier;
	static float sm_fSpecialAmmoExplosiveShotgunPlayerDamageMultiplier;
	static float sm_fSpecialAmmoExplosiveSniperPedLocalDamageMultiplier;
	static float sm_fSpecialAmmoExplosiveSniperPedRemoteDamageMultiplier;
	static float sm_fSpecialAmmoExplosiveSniperPlayerDamageMultiplier;
	static u32 sm_uFlashGrenadeStrongEffectVFXLength;
	static u32 sm_uFlashGrenadeWeakEffectVFXLength;
	static float sm_fFlashGrenadeVehicleOccupantDamageMultiplier;
	static float sm_fStunGrenadeVehicleOccupantDamageMultiplier;
	
public:
	static float				m_version;
};



//-----------------------------------------------------------------
//  CExplosionInfoManager
//  Parses explosion.meta/Explosion.psc to obtain explosion types.
//-----------------------------------------------------------------
class CExplosionInfoManager
{
public:
	// Initialise
	static void Init(const char* pFileName);

	static void Load(const char* pFileName);
	static void Append(const char* pFileName);

#if __BANK
	//Used for widgets in RAG
	static void	AddWidgets(bkBank& bank);
	static void	SaveCallback();
#endif

	// Shutdown
	static void Shutdown();

    //Singleton instance
	static CExplosionInfoManager m_ExplosionInfoManagerInstance;

	// Accessor method for CExplosionTagData
	// Should be run on the Singleton Instance
    CExplosionTagData& GetExplosionTagData(eExplosionTag tagIndex);

	float GetExplosionEndRadius(const CExplosionTagData& expTagData);
	float GetExplosionForceFactor(const CExplosionTagData& expTagData);

	s32 GetSize() const {return m_aExplosionTagData.size();}

private:

	//Contains the actual explosion tag data
	atArray<CExplosionTagData>	m_aExplosionTagData;

	//Failure response for someone trying to access data outside of the array
	CExplosionTagData m_Dummy;

	// Delete the data
	static void Reset();

	PAR_SIMPLE_PARSABLE;
};

#endif//_EXPLOSION_H_
