#ifndef _EXPLOSION_INST_H_
#define _EXPLOSION_INST_H_

// Rage headers
#include "vector/vector3.h"

// Framework headers
#include "fwtl/pool.h"

// Game headers
#include "audio/explosionaudioentity.h"
#include "network/General/NetworkFXId.h"
#include "Physics/WaterTestHelper.h"
#include "scene/Entity.h"
#include "scene/RegdRefTypes.h"
#include "weapons/WeaponEnums.h"

// Forward declarations
class CEntity;
class phGtaExplosionInst;

// Defines
#define MAX_ENTITIES_TO_APPLY_DAMAGE (32)

enum ePhysicalType 
{
	PT_SPHERE			= 0,
	PT_DIRECTED
};


class CExplosionOcclusionCache
{
public:
	class COcclusionResult
	{
	public:
		enum Status
		{
			OCCLUDED,
			NOT_OCCLUDED,
			OUTDATED
		};
		COcclusionResult(rage::phHandle handle) : m_Handle(handle), m_Status(OUTDATED) {}
		COcclusionResult() {}

		Status GetStatus() const
		{
			if(CPhysics::GetTimeSliceId() >= m_InvalidTimeSliceId)
			{
				return OUTDATED;
			}
			else
			{
				return m_Status;
			}
		}

		void SetStatus(Status status, u32 lifeTime)
		{
			m_InvalidTimeSliceId = CPhysics::GetTimeSliceId() + lifeTime;
			m_Status = status;
		}

		rage::phHandle GetHandle() const { return m_Handle; }

	private:
		rage::phHandle m_Handle;
		u32 m_InvalidTimeSliceId;
		Status m_Status;
	};

	COcclusionResult* FindOcclusionResult(rage::phHandle handle);
	COcclusionResult::Status FindOcclusionStatus(rage::phHandle handle);
	void UpdateOcclusionStatus(rage::phHandle handle, COcclusionResult::Status status, u32 lifeTime);

	static u32 ComputeOcclusionResultLifeTime(const CEntity* pEntity, COcclusionResult::Status status);

#if __BANK
	static bool sm_EnableOcclusionCache;
	static bool sm_EnableLifeTimeOverride;
	static u32 sm_LifeTimeOverride;
#endif // __BANK
private:
	static const int OCCLUSION_CACHE_SIZE = 32;
	atFixedArray<COcclusionResult,OCCLUSION_CACHE_SIZE> m_Cache;
};

// Classes
class phGtaExplosionInst
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		FW_REGISTER_CLASS_POOL(phGtaExplosionInst);

							phGtaExplosionInst		(eExplosionTag ExplosionTag, CEntity *pExplosionOwner, CEntity *pExplodingEntity, CEntity *pIgnoreDamageEntity, CEntity* pAttachEntity, s32 attachBoneTag, const Vector3& vPosWld, const Vector3& vDirWld, float sizeScale, u32 camShakeNameHash, float camShake, float camShakeRollOffScaling, bool makesSound, bool noVfx, bool inAir, u32 vfxTagHash, const CNetFXIdentifier& networkIdentifier, bool bNoDamage, bool bAttachedToVehicle, bool bDetonatingOtherPlayersExplosive, u32 weaponHash, bool bDisableDamagingOwner, fwInteriorLocation intLoc);
							~phGtaExplosionInst		();

		void				Update					();


		ePhysicalType		GetPhysicalType			();
		eWeaponEffectGroup	GetWeaponGroup			();
		eDamageType			GetDamageType			();

		bool				ShouldApplyContinuousDamage();
		bool				ShouldPostProcessCollisionsWithNoForce();
		bool				ShouldDamageVehicles	();
		bool				ShouldDamageObjects		();
		bool				ShouldEnforceDamagedEntityList();

		float				CalculateExplosionDamage (Vec3V_In vHitPosition, bool bIsTypePed = false);

		// damage entity query functions
		bool				AddDamagedEntity		(CEntity* pEntity);
		void				RemoveDamagedEntity		(CEntity* pEntity);
		bool				IsEntityAlreadyDamaged	(CEntity* pEntity);

		// functions that trigger other systems
		void				CreateAudio				();
		void 				CreateCameraShake		();

		bool				CanActivate				() const;

		// accessors
		eExplosionTag		GetExplosionTag			() const					{return m_explosionTag;}
		eExplosionTag		GetOriginalExplosionTag	() const					{return m_originalExplosionTag;}
		void				SetOriginalExplosionTag	(eExplosionTag val)			{m_originalExplosionTag = val;}

		CEntity*			GetExplosionOwner		() const					{return m_pExplosionOwner;}
		CEntity*			GetExplodingEntity		() const					{return m_pExplodingEntity;}
		CEntity*			GetIgnoreDamageEntity	() const					{return m_pIgnoreDamageEntity;}

		CEntity*			GetAttachEntity			() const					{return m_pAttachEntity;}
		s32					GetAttachBoneIndex		() const					{return m_attachBoneIndex;}
		Vec3V_Out			GetPosLcl				() const					{return m_vPosLcl;}
		Vec3V_Out			GetDirLcl				() const					{return m_vDirLcl;}
		Vec3V_Out			GetPosWld				() const					{return m_vPosWld;}
		Vec3V_Out			GetDirWld				() const					{return m_vDirWld;}

		float				GetSizeScale			() const					{return m_sizeScale;}
		float				GetUnderwaterDepth		() const					{return m_underwaterDepth;}
		WaterTestResultType	GetUnderwaterType		() const					{return m_underwaterType;}
				
		bool				GetMakesSound			() const					{return m_bMakesSound;}
		bool				GetNoVfx				() const					{return m_bNoVfx;}
		bool				GetInAir				() const					{return m_bInAir;}
		bool				ShouldApplyDamage		() const					{return !m_bNoDamage;}
		bool				GetAttachedToVehicle	() const					{return m_bAttachedToVehicle;}
		bool				GetDetonatingOtherPlayersExplosive() const			{return m_bDetonatingOtherPlayersExplosive;}
		bool				GetNeedsRemoved			() const					{return m_bNeedsRemoved;}
		void				SetNeedsRemoved			(bool remove)				{m_bNeedsRemoved = remove;}
		bool				GetDisableDamagingOwner () const					{return m_bDisableDamagingOwner;}
		bool				CanReportCrime			() const					{return m_bReportCrimes;}
		void				SetCanReportCrimes		(bool bCanReportCrimes)		{m_bReportCrimes = bCanReportCrimes;}

		u32					GetVfxTagHash			() const					{return m_vfxTagHash;}

		CNetFXIdentifier&	GetNetworkIdentifier	()							{return m_networkIdentifier;}

		u32					GetWeaponHash			() const					{return m_weaponHash;}

		u32					GetSpawnTimeMS			() const					{return m_SpawnTimeMS;}

		CExplosionOcclusionCache& GetOcclusionCache() { return m_OcclusionCache; }

		fwInteriorLocation	GetInteriorLocation		() const					{return m_interiorLocation;}

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		eExplosionTag			m_explosionTag;
		eExplosionTag			m_originalExplosionTag; 					// if it's been overridden on impact

		RegdEnt 				m_pExplosionOwner;
		RegdEnt 				m_pExplodingEntity;
		RegdEnt 				m_pIgnoreDamageEntity;
		
		RegdEnt					m_pAttachEntity;							// position, direction and attachment			
		s32						m_attachBoneIndex;
		Vec3V					m_vPosLcl;
		Vec3V					m_vDirLcl;
		Vec3V					m_vPosWld;
		Vec3V					m_vDirWld;	

		float					m_sizeScale;
		u32						m_camShakeNameHash;
		float					m_fCamShake;
		float					m_fCamShakeRollOffScaling;
		float					m_underwaterDepth;
		WaterTestResultType		m_underwaterType;

		bool					m_bMakesSound;								// flags
		bool					m_bNoVfx;
		bool					m_bInAir;
		bool					m_bNoDamage;
		bool					m_bAttachedToVehicle;
		bool					m_bDetonatingOtherPlayersExplosive;
		bool					m_bNeedsRemoved;
		bool					m_bDisableDamagingOwner;
		bool					m_bReportCrimes;
		bool					m_bIsAttachEntityValid;

		u32						m_vfxTagHash;

		RegdEnt					m_pAlreadyDamagedEntities[MAX_ENTITIES_TO_APPLY_DAMAGE];

		CNetFXIdentifier		m_networkIdentifier;						// used to identify this explosion across the network 

		u32						m_weaponHash;

		u32						m_SpawnTimeMS;

		CExplosionOcclusionCache m_OcclusionCache;

		fwInteriorLocation		m_interiorLocation;							// Used for audio occlusion when we have no attach entities
}; // phGtaExplosionInst


#endif
