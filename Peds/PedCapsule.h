// FILE :    PedCapsule.h
// PURPOSE : Load in capsule information for varying types of peds
// CREATED : 10-11-2010

#ifndef PED_CAPSULE_H
#define PED_CAPSULE_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"
#include "phcore/constants.h"

// Framework headers

// Game headers

#define USE_SPHERES 1		//1 for spheres, 0 for boxes

namespace rage
{
	class phBound;
	class phBoundComposite;
	USE_GEOMETRY_CURVED_ONLY(class phBoundCurvedGeometry;)
}

enum eRagdollType
{
	RD_MALE = 0,
	RD_FEMALE,
	RD_MALE_LARGE,
	RD_CUSTOM
};

class CBipedCapsuleInfo;
class CQuadrupedCapsuleInfo;
class CBirdCapsuleInfo;
class CFishCapsuleInfo;
class CPedModelInfo;

class CBaseCapsuleInfo
{
public:
	CBaseCapsuleInfo(){ m_CapsuleType = CAPTYPE_INVALID; }
	virtual ~CBaseCapsuleInfo(){}

	enum CapType
	{
		CAPTYPE_INVALID,
		CAPTYPE_BIPED,
		CAPTYPE_BIRD,
		CAPTYPE_FISH,
		CAPTYPE_QUADRUPED,

		CAPTYPE_FIRST_BIPED = CAPTYPE_BIPED,
		CAPTYPE_LAST_BIPED = CAPTYPE_BIRD
	};

	enum BoundCompositeParts {
		BOUND_MAIN_CAPSULE,
		BOUND_CAPSULE_PROBE,
		BOUND_CAPSULE_PROBE_2,
	};

	virtual phBoundComposite* GeneratePedBound(CPedModelInfo &rPedModelInfo, phBoundComposite* pCompositeBound=0) const = 0;
	bool IsQuadruped() const							{ return m_CapsuleType == CAPTYPE_QUADRUPED; }
	bool IsBiped() const								{ return m_CapsuleType >= CAPTYPE_FIRST_BIPED && m_CapsuleType <= CAPTYPE_LAST_BIPED; }
	bool IsFish() const									{ return m_CapsuleType == CAPTYPE_FISH; }
	bool IsBird() const									{ return m_CapsuleType == CAPTYPE_BIRD; }
	inline const CBipedCapsuleInfo* GetBipedCapsuleInfo() const;
	inline const CQuadrupedCapsuleInfo* GetQuadrupedCapsuleInfo() const;
	inline const CFishCapsuleInfo* GetFishCapsuleInfo() const;

	//Extents data used by higher level code
	virtual float GetMaxSolidHeight()	const = 0;
	virtual float GetMinSolidHeight()	const = 0;
	virtual float GetMinProbeHeight()	const = 0;
	virtual float GetHalfWidth()		const = 0;	//Replacing GetRadius, which didn't give you what you'd expect

	// Get the hash
	u32 GetHash() { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name from the metadata manager
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	const float	GetMass() const					{ return m_Mass; }
	const float GetGroundInstanceDamage() const { return m_GroundInstanceDamage; }
	const float	GetGroundToRootOffset() const	{ return m_GroundToRootOffset; }
	const float& GetGroundToRootOffsetRef() const{ return m_GroundToRootOffset; }	// Returning by reference to allow loading straight into vector registers.
	const float	GetProbeRadius() const			{ return m_ProbeRadius;	}
	const float GetProbeYOffset() const			{ return m_ProbeYOffset; }

	const bool	GetUsesRagdollReactionIfShoved() const	{ return m_UsesRagdollReactionIfShoved;	}
	const bool	GetDiesOnEnteringRagdoll() const { return m_DiesOnEnteringRagdoll; }
	bool        GetUseInactiveRagdollCollision() const { return m_UseInactiveRagdollCollision; }
	eRagdollType GetRagdollType() const			{ return m_RagdollType; }

	const bool ShouldFlattenCollisionNormals() const { return m_FlattenCollisionNormals; }
	const float GetMaxNormalZForFlatten() const { return m_MaxNormalZForFlatten; }
	const float GetMinNormalZForFlatten() const { return m_MinNormalZForFlatten; }
	const float GetMaxNormalZForFlatten_Movable() const { return m_MaxNormalZForFlatten_Movable; }
	const float GetMinHeightForFlatten() const { return m_MinHeightForFlatten; }
	const float GetMinHeightForFlattenInteriorOffset() const { return m_MinHeightForFlattenInteriorOffset; }

protected:

	atHashWithStringNotFinal m_Name;
	u8		m_CapsuleType;					// enum CapType
	bool	m_FlattenCollisionNormals;
	bool	m_UsesRagdollReactionIfShoved;
	bool	m_DiesOnEnteringRagdoll;
	bool	m_UseInactiveRagdollCollision;

	float	m_Mass;
	float	m_GroundInstanceDamage;
	float	m_GroundToRootOffset;
	float	m_ProbeRadius;
	float	m_ProbeYOffset;
	float	m_MaxNormalZForFlatten;
	float	m_MinNormalZForFlatten;
	float	m_MaxNormalZForFlatten_Movable;
	float	m_MinHeightForFlatten;
	float	m_MinHeightForFlattenInteriorOffset;
	eRagdollType m_RagdollType;

	PAR_PARSABLE;
};

class CBipedCapsuleInfo : public CBaseCapsuleInfo
{
	float	m_Radius;
	float	m_RadiusRunning;
	float	m_RadiusCrouched;
	float	m_RadiusInCover;
	float	m_RadiusCrouchedinCover;
	float	m_RadiusMobilePhone;
	float   m_RadiusFPSHeadBlocker;
	float	m_RadiusWeaponBlocked;
	float	m_RadiusGrowSpeed;
	float	m_HeadHeight;
	float	m_KneeHeight;
	float	m_CapsuleZOffset;
	float	m_HeadHeightCrouched;
	float	m_CrouchedCapsuleOffsetZ;
	float	m_GroundOffsetExtend;
	float	m_BoatBlockerHeadHeight;
	float	m_BoatBlockerZOffset;
	bool	m_UseLowerLegBound;
	bool	m_UseBoatBlocker;
	bool	m_UseFPSHeadBlocker;

	virtual float GetHalfWidth() const			{ return m_Radius; }
	virtual float GetMaxSolidHeight() const		{ return m_HeadHeight; }
	virtual float GetMinSolidHeight() const		{ return m_KneeHeight; }
	virtual float GetMinProbeHeight() const		{ return -(GetGroundToRootOffset() + m_GroundOffsetExtend); }
	virtual phBoundComposite* GeneratePedBound(CPedModelInfo &rPedModelInfo, phBoundComposite* pCompositeBound=0) const;
#if USE_GEOMETRY_CURVED
	static phBound* InitPhysCreateCurvedGeomentryBound(float fRadius, float fHeight, float fMargin,phBoundCurvedGeometry* pCurvedBound);
#endif
public:
	const float	GetRadius() const				{ return m_Radius; }
	const float GetRadiusRunning() const		{ return m_RadiusRunning; }
	const float GetRadiusCrouched() const		{ return m_RadiusCrouched; }
	const float GetRadiusInCover() const			{ return m_RadiusInCover; }
	const float GetRadiusCrouchedInCover() const	{ return m_RadiusCrouchedinCover; }
	const float GetRadiusMobilePhone() const	{ return m_RadiusMobilePhone; }
	const float GetRadiusFPSHeadBlocker() const	{ return m_RadiusFPSHeadBlocker; }
	const float GetRadiusWeaponBlocked() const	{ return m_RadiusWeaponBlocked; }
	const float GetRadiusGrowSpeed() const		{ return m_RadiusGrowSpeed; }

	const float	GetHeadHeight() const			{ return m_HeadHeight; }
	const float	GetHeadHeightCrouched() const	{ return m_HeadHeightCrouched; }

	const float	GetKneeHeight()	const			{ return m_KneeHeight; }
	
	const float	GetCapsuleZOffset()	const		{ return m_CapsuleZOffset; }	
	const float	GetCrouchedCapsuleOffsetZ()	const	{ return m_CrouchedCapsuleOffsetZ; }

	const float	GetGroundOffsetExtend() const	{ return m_GroundOffsetExtend; }

	const float	GetBoatBlockerHeadHeight() const	{ return m_BoatBlockerHeadHeight; }
	const float	GetBoatBlockerZOffset() const		{ return m_BoatBlockerZOffset; }

	bool GetUseLowerLegBound() const	{ return m_UseLowerLegBound; }
	bool GetUseBoatBlocker() const	{ return m_UseBoatBlocker; }
	bool GetUseFPSHeadBlocker() const { return m_UseFPSHeadBlocker; }

	CBipedCapsuleInfo() { m_CapsuleType = CAPTYPE_BIPED; }
	virtual ~CBipedCapsuleInfo(){}

	PAR_PARSABLE;
};

class CBirdCapsuleInfo : public CBipedCapsuleInfo
{
public:
	CBirdCapsuleInfo() { m_CapsuleType = CAPTYPE_BIRD; }
	virtual ~CBirdCapsuleInfo() {}

	PAR_PARSABLE;
};

class CFishCapsuleInfo : public CBaseCapsuleInfo
{
	float	m_Radius;
	float	m_Length;
	float	m_ZOffset;
	float	m_YOffset;
	virtual phBoundComposite* GeneratePedBound(CPedModelInfo &rPedModelInfo, phBoundComposite* pCompositeBound=0) const;
	virtual float GetHalfWidth() const			{ return m_Radius; }
	virtual float GetMaxSolidHeight() const		{ return GetGroundToRootOffset() + m_ZOffset+m_Radius; }
	virtual float GetMinSolidHeight() const		{ return GetGroundToRootOffset() + m_ZOffset-m_Radius; }
	virtual float GetMinProbeHeight() const		{ return 0.0f; }		//Fish don't probe, won't have collisions with the probe bound

public:
	const float GetMainBoundLength() const { return m_Length; }

	CFishCapsuleInfo() { m_CapsuleType = CAPTYPE_FISH; }
	virtual ~CFishCapsuleInfo() {}

	PAR_PARSABLE;
};

class CQuadrupedCapsuleInfo : public CBaseCapsuleInfo
{	
	//Use m_GroundToRootOffset in base for height
	float	m_Radius;
	float	m_Length;
	float	m_ZOffset;
	float	m_YOffset;
	float	m_MinSolidHeight;
	float	m_ProbeDepthFromRoot;
	float	m_BlockerRadius;
	float	m_BlockerLength;
	float	m_BlockerYOffset;
	float	m_BlockerZOffset;
	float	m_BlockerXRotation;
	float	m_PropBlockerY;
	float	m_PropBlockerZ;
	float	m_PropBlockerRadius;
	float	m_PropBlockerLength;
	float	m_NeckBoundRadius;
	float	m_NeckBoundLength;	
	float	m_NeckBoundRotation;
	float	m_NeckBoundHeightOffset;
	float	m_NeckBoundFwdOffset;
	bool	m_PropBlocker;
	bool	m_Blocker;
	bool	m_NeckBound;
	bool	m_UseHorseMapCollision;
	s8		m_PropBlockerSlot;
	s8		m_BlockerSlot;
	s8		m_NeckBoundSlot;

	virtual phBoundComposite* GeneratePedBound(CPedModelInfo &rPedModelInfo, phBoundComposite* pCompositeBound=0) const;
	virtual float GetHalfWidth() const			{ return m_Radius; }
	virtual float GetMaxSolidHeight() const		{ return GetGroundToRootOffset() + m_Radius + m_ZOffset; }
	virtual float GetMinSolidHeight() const		{ return m_MinSolidHeight; }
	virtual float GetMinProbeHeight() const		{ return -m_ProbeDepthFromRoot; }
public:
	void OnPostLoad();

	const bool GetUseHorseMapCollision() const { return m_UseHorseMapCollision; }
	const bool GetUseNeckBound() const {return m_NeckBound;}
	const float GetNeckBoundRotation() const {return m_NeckBoundRotation;}
	const float GetNeckBoundLength() const {return m_NeckBoundLength;}
	s8 GetNeckBoundSlot() const {return m_NeckBoundSlot;}
	const float GetMainBoundLength() const { return m_Length; }
	const float GetMainBoundRadius() const { return m_Radius; }

	s8 GetPropBlockerSlot() const { return m_PropBlockerSlot; }
	bool GetUsePropBlocker() const { return m_PropBlocker; }

	s8 GetBlockerSlot() const { return m_BlockerSlot; }
	bool GetUseBlocker() const { return m_Blocker; }

	CQuadrupedCapsuleInfo() : m_PropBlockerSlot(-1), m_BlockerSlot(-1), m_NeckBoundSlot(-1){ m_CapsuleType = CAPTYPE_QUADRUPED; }
	virtual ~CQuadrupedCapsuleInfo(){}

	PAR_PARSABLE;
};

class CPedCapsuleInfoManager
{
public:
	// Initialise
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	static CPedCapsuleInfoManager &Instance() { return m_CapsuleManagerInstance; };

	// Access the Capsule information
	static const CBaseCapsuleInfo* const GetInfo(const u32 uNameHash)
	{
		if(uNameHash != 0)
		{
			for (s32 i = 0; i < m_CapsuleManagerInstance.m_aPedCapsule.GetCount(); i++ )
			{
				if(m_CapsuleManagerInstance.m_aPedCapsule[i]->GetHash() == uNameHash)
				{
					return m_CapsuleManagerInstance.m_aPedCapsule[i];
				}
			}
		}
		return NULL;
	}

	void Append(const char* pFileName);

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load(const char* pFileName);

#if __BANK
	// Save the data
	static void Save();
	static void Refresh();
#endif // __BANK

	atArray<CBaseCapsuleInfo*> m_aPedCapsule;

	static CPedCapsuleInfoManager m_CapsuleManagerInstance;

	PAR_SIMPLE_PARSABLE;
};


const CBipedCapsuleInfo* CBaseCapsuleInfo::GetBipedCapsuleInfo() const
{
	return IsBiped() ? static_cast<const CBipedCapsuleInfo*>(this) : NULL;
}


const CQuadrupedCapsuleInfo* CBaseCapsuleInfo::GetQuadrupedCapsuleInfo() const
{
	return IsQuadruped() ? static_cast<const CQuadrupedCapsuleInfo*>(this) : NULL;
}

const CFishCapsuleInfo* CBaseCapsuleInfo::GetFishCapsuleInfo() const
{
	return IsFish() ? static_cast<const CFishCapsuleInfo*>(this) : NULL;
}

#endif
