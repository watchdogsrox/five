//
// task/Scenario/ScenarioPoint.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIO_POINT_H
#define TASK_SCENARIO_SCENARIO_POINT_H

#include "task/Scenario/scdebug.h" // for SCENARIO_DEBUG
#include "task/Scenario/ScenarioPointFlags.h"
#include "scene/2dEffect.h"			// For CSpawnPoint::AvailabilityMpSp.

//Rage headers
#include "atl/creator.h"

//Framework includes
#include "fwtl/Pool.h"
#include "fwtl/regdrefs.h"
#include "entity/extensionlist.h"
#include "vector/color32.h"
#include "vectormath/quatv.h"		// For QuatV_Ref.

class CCarGenerator;
class CEntity;
struct CSpawnPoint;

namespace rage
{
	class fwArchetype;
	class fwExtensionDef;
}

#define INVALID_SCENARIO_POINT_INDEX (-1)
// If SCENARIO_TYPE_BITS changes, update bitfields for CSpawnPoint::iType and CCarGenerator::m_Scenario
#define SCENARIO_TYPE_BITS 9
#define MAX_SCENARIO_TYPES (1 << SCENARIO_TYPE_BITS)
#define SCENARIO_TYPE_EXTENSION_BITS (SCENARIO_TYPE_BITS - 8)
#define SCENARIO_TYPE_EXTENSION_MASK ((1 << SCENARIO_TYPE_EXTENSION_BITS) - 1)

#define MODELSET_TYPE_BITS 9
#define MAX_MODELSET_TYPES (1 << MODELSET_TYPE_BITS)
#define MODELSET_TYPE_EXTENSION_BITS (MODELSET_TYPE_BITS - 8)
#define MODELSET_TYPE_EXTENSION_MASK ((1 << MODELSET_TYPE_EXTENSION_BITS) - 1)

//-----------------------------------------------------------------------------
class CScenarioPoint;

struct CSpawnPointOverrideExtension : public fwExtension
{
	FW_REGISTER_CLASS_POOL(CSpawnPointOverrideExtension);
	EXTENSIONID_DECL(CSpawnPointOverrideExtension, fwExtension);

	atHashString	m_ScenarioType;
	u8				m_iTimeStartOverride;
	u8				m_iTimeEndOverride;
	atHashString	m_Group;
	atHashString	m_ModelSet;
	CSpawnPoint::AvailabilityMpSp m_AvailabilityInMpSp;
	atFixedBitSet<32, u32> m_Flags;
	float			m_Radius;
	float			m_TimeTillPedLeaves;

	void ApplyToPoint(CScenarioPoint& point);
	void InitEntityExtensionFromDefinition(const fwExtensionDef* definition, fwEntity* entity);
};

//-----------------------------------------------------------------------------

class CScenarioPoint : public fwRefAwareBaseNonVirt
{
public:
	// We no longer do this, because we don't want to always allocate from the pool.
	// Instead.
	//	FW_REGISTER_CLASS_POOL(CScenarioPoint);

	static CScenarioPoint* CreateFromPool()
	{
		CScenarioPoint* pt = _ms_pPool->New();
		return rage_placement_new(pt) CScenarioPoint;
	}
	static CScenarioPoint* CreateFromPool(CSpawnPoint* basePoint, CEntity* attachedTo)
	{
		CScenarioPoint* pt = _ms_pPool->New();
		return rage_placement_new(pt) CScenarioPoint(basePoint, attachedTo);
	}

	static void DestroyFromPool(CScenarioPoint& pt)
	{
#if SCENARIO_DEBUG
		// Shouldn't happen when everything is working correctly, but if it does, fail an
		// assert and delete safely. Better than crashing for the scenario editor users.
		if(!_ms_pPool->IsValidPtr(&pt))
		{
			Assert(0);
			DestroyForEditor(pt);
			return;
		}
#endif	// SCENARIO_DEBUG

		Assert(_ms_pPool->IsValidPtr(&pt));
		pt.~CScenarioPoint();
		_ms_pPool->Delete(&pt);
	}

#if SCENARIO_DEBUG
	static CScenarioPoint* CreateForEditor();
	static void DestroyForEditor(CScenarioPoint& pt);
	static int sm_NumEditorPoints;
#endif // SCENARIO_DEBUG

	typedef fwPool<CScenarioPoint> Pool;
	static Pool* _ms_pPool;

	static void InitPool(const MemoryBucketIds membucketId = MEMBUCKET_DEFAULT, int redZone = 0);
	static void InitPool(int size, const MemoryBucketIds membucketId = MEMBUCKET_DEFAULT, int redZone = 0);
	static void ShutdownPool() {delete _ms_pPool; _ms_pPool = NULL;}
	static Pool* GetPool() {return _ms_pPool;}


	         CScenarioPoint  ();
    explicit CScenarioPoint  (CSpawnPoint* basePoint, CEntity* attachedTo = NULL);
			~CScenarioPoint ();
			 CScenarioPoint(const CScenarioPoint& tobe);
			 CScenarioPoint& operator=(const CScenarioPoint& tobe);

	//////////////////////////////////////////////////////////////////////////
	enum InteriorState
	{
		IS_Unknown = 0,
		IS_Outside,
		IS_Inside,
	};

	enum AvailabilityMpSp
	{
		kBoth,		// Scenario exists both in single player and multiplayer.
		kOnlySp,	// Only in single player.
		kOnlyMp		// Only in multiplayer.
	};

	enum SpecialTimeValues // PURPOSE:	Special values for m_iTimeTillPedLeaves.
	{
		kTimeTillPedLeavesNone = 0,			// No override.
		kTimeTillPedLeavesMax = 254,		// Max possible finite time.
		kTimeTillPedLeavesInfinite = 255	// Stay a while... stay forever.
	};

	enum SpecialProbabilityValues
	{
		kProbDefaultFromType = 0			// No override
	};
	static const int kProbResolution = 5;	// 5% steps, requiring the range 0..20 to represent 0-100%.

	//////////////////////////////////////////////////////////////////////////
#if SCENARIO_DEBUG
	bool IsEditable() const;
	void SetEditable(bool val);
	void FindDebugColourAndText(Color32 *col, char *string, int buffSize) const;
	const char* GetScenarioName() const;
#endif // SCENARIO_DEBUG

	static void PostPsoPlace(void* data);
	void CleanUp(); //called because the pso in-place destruct does not call destructors.
	void Reset(); //It is invalid to call Reset on ScenarioPoints that have valid m_pEntity ptrs. Only use should be in CTaskAmbulancePatrol

	//Type
	unsigned GetScenarioTypeVirtualOrReal() const;
	int GetScenarioTypeReal(unsigned indexWithinTypeGroup) const;

	void SetScenarioType(unsigned scenarioType);

#if __ASSERT
	bool IsRealScenarioTypeValidForThis(int realScenarioType) const;
#endif

	//Position
	void		SetPosition(Vec3V_In pos)		{ SetLocalPosition(pos); }				// Deprecated
	void		SetLocalPosition(Vec3V_In pos)	{ m_vPositionAndDirection.SetXYZ(pos); }
	void		SetWorldPosition(Vec3V_In pos);
    Vec3V_Out	GetPosition() const				{ return GetWorldPosition(); }
	Vec3V_Out	GetLocalPosition() const		{ return m_vPositionAndDirection.GetXYZ(); }
	Vec3V_Out	GetWorldPosition() const;

	// Direction
	void		SetDirection(float dir)			{ SetLocalDirection(dir); }				// Deprecated
	void		SetLocalDirection(float dir)	{ m_vPositionAndDirection.SetWf(dir); }
	void		SetWorldDirection(float dir);
	Vec3V_Out	GetDirection() const			{ return GetWorldDirectionFwd(); }		// Deprecated
	float		GetDirectionf() const			{ return GetLocalDirectionf(); }		// Deprecated
    void		GetDirection(QuatV_Ref quatOut) const { GetLocalDirection(quatOut); } 	// Deprecated
    Vec3V_Out	GetWorldDirectionFwd() const;
	float		GetLocalDirectionf() const		{ return m_vPositionAndDirection.GetWf(); }
    void		GetLocalDirection(QuatV_Ref quatOut) const;

	//Interior
	void		SetInterior(unsigned int id) { Assert(id <= 0xff); m_iInterior = (u8)id; }
	unsigned int GetInterior() const { return m_iInterior; }

	//Radius
	void		SetRadius(float fRadius) { m_iRadius = (u8)Clamp(rage::round(fRadius * ms_fRadiusUnitsPerMeter), 0, 255); } //fRadius is in meters
	float		GetRadius() const { return m_iRadius / ms_fRadiusUnitsPerMeter; } //returns value in meters

	//Uses
    void AddUse();
    void RemoveUse();
    u8   GetUses() const;

	//Entity
    CEntity*		GetEntity() const { return m_pEntity; }
	void			SetEntity(CEntity* ent) { 
		Assertf(!IsRunTimeFlagSet(CScenarioPointRuntimeFlags::OnAddCalled), "Entity changed after adding to the ScenarioPointManager. "
			" Please set the entity before adding to the ScenarioPointManager"); 
		m_pEntity = ent;
	}

	//Entity override
	bool			IsEntityOverridePoint() const { return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::EntityOverridePoint); }
	void			SetIsEntityOverridePoint(bool b) { SetRunTimeFlag(CScenarioPointRuntimeFlags::EntityOverridePoint, b); }

	//Time
	bool	HasTimeOverride() const;
	u8		GetTimeStartOverride() const;
	void	SetTimeStartOverride(u8 over);
	u8		GetTimeEndOverride() const;
	void	SetTimeEndOverride(u8 over);
	float	GetTimeTillPedLeaves() const;
	bool	HasTimeTillPedLeaves() const;
	void	SetTimeTillPedLeaves(float timeInSeconds);
	
	//Interior State (should be ignored for attached points)
	bool	IsInteriorState(InteriorState query) const;
	void	SetInteriorState(InteriorState state);

	//Model
	unsigned	 GetModelSetIndex() const;
	void SetModelSetIndex(unsigned index);

	unsigned GetType() const;
	void SetType(unsigned index);

	// Availability
	bool	IsAvailability(AvailabilityMpSp query) const;
	void	SetAvailability(AvailabilityMpSp avail);
#if SCENARIO_DEBUG
	u8		GetAvailability() const; //Used by the editor only.
#endif // SCENARIO_DEBUG

	//Scenario Group
	unsigned	 GetScenarioGroup() const;
	void SetScenarioGroup(unsigned sgroup);

	// IMAP requirements
	unsigned	 GetRequiredIMap() const;
	void SetRequiredIMap(unsigned index);

	//Priority
	void SetHighPriority(bool b) { SetFlag(CScenarioPointFlags::HighPriority, b); }
	// Logic here is now slightly more complex as script can change the priority of a point group now.
	// TrulyHighPriority is the priority disregarding script's overrides.
	bool IsTrulyHighPriority() const { return IsFlagSet(CScenarioPointFlags::HighPriority); }
	bool IsHighPriority() const;

	//Range
	void SetExtendedRange(bool b) { SetFlag(CScenarioPointFlags::ExtendedRange, b); }
	void SetShortRange(bool b) { SetFlag(CScenarioPointFlags::ShortRange, b); }
	bool HasExtendedRange() const { return IsFlagSet(CScenarioPointFlags::ExtendedRange); }
	bool HasShortRange() const  { return IsFlagSet(CScenarioPointFlags::ShortRange); }

	//Probability
	bool HasProbabilityOverride() const { return m_iProbability != kProbDefaultFromType; }
	float GetProbabilityOverride() const { Assert(HasProbabilityOverride()); return ((float)m_iProbability)*((float)kProbResolution)*0.01f; }
	int GetProbabilityOverrideAsIntPercentage() const { Assert(HasProbabilityOverride()); return m_iProbability*kProbResolution; }
	void ClearProbabilityOverride() { m_iProbability = kProbDefaultFromType; }
	void SetProbabilityOverride(float f);

	//Loading ... 
	void InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype);
	bool DetermineModelSetId(atHashWithStringNotFinal modelSetHash, const bool isVehicleType);

#if SCENARIO_DEBUG
	void CopyToDefinition(fwExtensionDef * definition) const;
	void PrepSaveObject(const CScenarioPoint& prepFrom);
#endif	// SCENARIO_DEBUG

	void SetOpenDoor(bool b)		{ SetFlag(CScenarioPointFlags::OpenDoor, b); }

	bool GetOnAddCalled() const { return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::OnAddCalled); }
	bool HasHistory() const { return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::HasHistory); }
	bool IsChained() const { return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::IsChained); }
	bool IsUsedChainFull() const { return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::ChainIsFull); }
	bool IsInCluster() const { return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::IsInCluster); }
	bool IsFromRsc() const { return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::FromRsc); }
	void MarkAsFromRsc() { SetRunTimeFlag(CScenarioPointRuntimeFlags::FromRsc, true); }
	bool IsInWater() const { return IsFlagSet(CScenarioPointFlags::InWater); }
	bool DoesAllowInvestigation() const { return IsFlagSet(CScenarioPointFlags::AllowInvestigation); }
	bool GetOpenDoor() const	{ return IsFlagSet(CScenarioPointFlags::OpenDoor); }

	// Point Obstruction
	void SetObstructed()
	{
		SetRunTimeFlag(CScenarioPointRuntimeFlags::Obstructed, true);
		ReportFailedCollision();
	}
	bool IsObstructed() const				{ return IsRunTimeFlagSet(CScenarioPointRuntimeFlags::Obstructed); }
	bool IsObstructedAndReset()
	{
		if(IsRunTimeFlagSet(CScenarioPointRuntimeFlags::Obstructed)) 
		{ 
			if(HasFailedCollisionRecently()) 
			{ 
				return true; 
			} 
			SetRunTimeFlag(CScenarioPointRuntimeFlags::Obstructed, false); 
			ClearFailedCollision(); 
		} 
		return false; 
	}

	bool HasCarGen() const { return m_CarGenId != CScenarioPoint::kCarGenIdInvalid; }
	CCarGenerator* GetCarGen() const;

	//Flags
	static const int kMaxNumFlagBits = 32;  // only using a atFixed32 bit set so need to make sure we only use as many as we can ... 
	bool IsFlagSet(CScenarioPointFlags::Flags f) const { return m_Flags.IsSet(f); }
	void SetFlag(CScenarioPointFlags::Flags f, bool b) { m_Flags.Set(f, b); }
	u32 GetAllFlags() const { Assert(m_Flags.GetNumBits() == kMaxNumFlagBits); return m_Flags.GetWord(0); }
	void SetAllFlags(u32 flags) { Assert(m_Flags.GetNumBits() == kMaxNumFlagBits); m_Flags.SetBits(flags); }
	void SetAllFlagsFromBitSet(const atFixedBitSet<kMaxNumFlagBits>& bitset) { m_Flags = bitset; }

	//Runtime flags
	bool IsRunTimeFlagSet(CScenarioPointRuntimeFlags::Flags f) const { return (m_RtFlags & (1 << f)) != 0; }
	void SetRunTimeFlag(CScenarioPointRuntimeFlags::Flags f, bool b)
	{
		if(b)
		{
			m_RtFlags |= (1 << f);
		}
		else
		{
			m_RtFlags &= ~(1 << f);
		}
	}

	// PURPOSE:	Report that we've failed collision checks.
	void ReportFailedCollision() { m_iFailedCollisionCheckTimeCount = GetFailedCollisionTicks(); }

	// PURPOSE:	Reset collision failure information.
	void ClearFailedCollision() { m_iFailedCollisionCheckTimeCount = 0; }

	// PURPOSE:	Check if we failed a collision check recently.
	bool HasFailedCollisionRecently() const
	{
		u8 count = m_iFailedCollisionCheckTimeCount;
		if(count)	// 0 is special, indicating no failure.
		{
			// initialize our delay time
			u8 iDelay = sm_FailedCollisionDelay;
			// Check if we failed due to obstruction
			if (IsRunTimeFlagSet(CScenarioPointRuntimeFlags::Obstructed))
			{
				const u8 iObstructedDelay = 15;
				iDelay += iObstructedDelay;
			}
			// Get the time that has elapsed, in "ticks". Since we have a limited number of bits this
			// will wrap around before too long, but that should be OK as long as we check back fairly
			// frequently, and in the worst case, we'll just end up waiting a little longer. This works
			// best if sm_FailedCollisionDelay is small compared to the range of m_iFailedCollisionCheckTimeCount.
			u8 elapsed = GetFailedCollisionTicks() - m_iFailedCollisionCheckTimeCount;
			return elapsed <= iDelay;
		}
		return false;
	}

	// PURPOSE:	Check to see if it's been reported that we've failed collision since
	//			the last time ClearFailedCollision() was called - unlike HasFailedCollisionRecently(),
	//			this doesn't time out.
	unsigned int /*bool*/ WasFailingCollision() const { return m_iFailedCollisionCheckTimeCount; }	

	// PURPOSE:	Roughly the number of "ticks" to wait after failing collision.
	// NOTES:	With the current GetFailedCollisionTicks(), each tick is about one second.
	//			This member is public only for making it more convenient for CPedPopulation to expose a widget.
	static u8	sm_FailedCollisionDelay;

private:
	friend class CScenarioManager; //For access to m_CarGenId & kCarGenIdInvalid

	// PURPOSE:	Get the value of an 8 bit timer that we use for avoiding repeating collision checks.
	// RETURNS:	A value between 1 and 255, roughly incremented by one for each second that elapses.
	static u8 GetFailedCollisionTicks();

	static const u16 kCarGenIdInvalid = 0x7fff;

	static dev_float ms_fRadiusUnitsPerMeter; 

	//Runtime ... should be padding in the PSO file
	CEntity*	m_pEntity;				//entity I am attached to ...
	u16			m_RtFlags:(CScenarioPointRuntimeFlags::Flags_NUM_ENUMS);	// Flags from CScenarioPointRuntimeFlags.
	u16			m_iUses : 3;			// The number of peds/dummy peds using them	
	u16			m_uInteriorState : 2;	// Whether this point is inside or outside, ignored for attached points (value from enum InteriorState)
	u16			m_iTypeHiBits : SCENARIO_TYPE_EXTENSION_BITS;
	u16			m_ModelSetIdHiBits : MODELSET_TYPE_EXTENSION_BITS;
	u16			m_CarGenId : 15;		// <<-- spare bits here - For CScenarioVehicleInfo scenarios, this is the ID number of the car generator that the scenario point created and owns.

#if SCENARIO_DEBUG
	u16			m_bEditable : 1;		// Whether this point is editable (parented to the world, from scenario point file)
#endif // SCENARIO_DEBUG
	u8			m_iFailedCollisionCheckTimeCount;	// If we have failed collision checks, this is used to implement a timer. Otherwise, it's zero.

	//PSO data
	u8			m_iType;				// The type of the spawnpoint, from SpawnPointTypes
	u8			m_ModelSetId;			// kNoCustomModelSet or the model set index 
	u8			m_iInterior;			// Index of the interior this point is in (+1), or kNoInterior ie 0 if outside or unknown. This relates to CScenarioPointManager::m_InteriorNames.	
	u8			m_iRequiredIMapId;		// kNoRequiredIMap or the map data index 
	u8			m_iProbability;			// kProbDefaultFromType, or a probability divided by kProbResolution.
	u8			m_uAvailableInMpSp;		// Whether it's available in multiplayer, single player, or both (value from enum AvailabilityMpSp).
	u8			m_iTimeStartOverride;	
	u8			m_iTimeEndOverride;		
	u8			m_iRadius;				// The radius of area associated with a given scenario point.  Does not necessarily correspond to meters, see ms_fRadiusUnitPerMeter.
	u8			m_iTimeTillPedLeaves;	// Time until leaving this point, overriding CScenarioInfo::m_TimeTilPedLeaves (use accessors to account for special values).	
	u16			m_iScenarioGroup;		// kNoGroup ie 0, or one plus the index of the scenario group it belongs to
	atFixedBitSet<kMaxNumFlagBits,u32> m_Flags;		// Flags from CScenarioPointFlags.
	Vec4V   	m_vPositionAndDirection;// Position in X, Y, and Z, with the direction in W.

	PAR_SIMPLE_PARSABLE
};

namespace rage
{
// CScenarioPoint is normally pool allocated, but the parser needs to create instances on the temp heap
// (because it doesn't call the normal overridden operator delete)
// When it constructs objects it does so via atPtrCreator, so specialize that fn to not use the pool
template<>
inline void* atPtrCreator< void, CScenarioPoint >()
{
	char* storage = rage_aligned_new (__alignof(CScenarioPoint)) char[sizeof(CScenarioPoint)];
	rage_placement_new(storage) CScenarioPoint;
	return storage;
};
}

///////////////////////////////////////////////////////////////////////////////
// ScenarioPoint based fwRegdRef. Use these to store local "pointers" to ScenarioPoint objects
//      doing so will ensure that when the point is deleted you are not using a bad pointer as
//      the internal pointer will be set to NULL for all references if the object is deleted.
//
// These should be true drop in replacements for the equivalent pointers.
// You should be able to replace "CScenarioPoint* m_target" with "RegdScenarioPnt m_target"
// and have everything else just work (tm).
// Remember to remove all REGREFs and TIDYREFs on m_target once you
// have done this though!
// If you run into any weird compilation errors you can usually just replace
// "m_target" with "m_target.Get()" and things will be fine.
///////////////////////////////////////////////////////////////////////////////
typedef	fwRegdRef<class CScenarioPoint, fwRefAwareBaseNonVirt> RegdScenarioPnt;
typedef	fwRegdRef<const class CScenarioPoint, fwRefAwareBaseNonVirt> RegdConstScenarioPnt;

//-----------------------------------------------------------------------------

/*
PURPOSE
	Since we have lots of CScenarioPoint objects and not all of them necessarily
	require data for less commonly used objects, some data has been put in
	CScenarioPointExtraData objects instead. These are associated with the CScenarioPoint
	objects by CScenarioPointManager.
*/
class CScenarioPointExtraData
{
public:
	CScenarioPointExtraData();

	FW_REGISTER_CLASS_POOL(CScenarioPointExtraData);

	// PURPOSE:	Check if some required IMAP has been set for this point.
	bool HasRequiredImap() const
	{
		return m_RequiredImapIndexInMapDataStore != kRequiredImapIndexInMapDataStoreNotSet;
	}

	// PURPOSE:	If a required IMAP has been set, and was found, return the index
	//			of that IMAP in fwMapDataStore, otherwise, return -1.
	int GetRequiredImapIndexInMapDataStore() const;

	// PURPOSE:	Set the hash of the name of an IMAP file that needs to be loaded in order
	//			for this point to be considered usable.
	void SetRequiredImapHash(atHashValue hashVal);

#if SCENARIO_DEBUG
	// PURPOSE:	Get the hash of the required IMAP file.
	atHashValue GetRequiredImapHash() const { return m_RequiredImapHash; }
#endif	// SCENARIO_DEBUG

protected:
	
	// PURPOSE:	Value used for m_RequiredImapIndexInMapDataStore when no IMAP file
	//			has been specified.
	static const u16 kRequiredImapIndexInMapDataStoreNotSet = 0xffff;

#if SCENARIO_DEBUG
	// PURPOSE:	Hash value of the required IMAP.
	// NOTES:	Hopefully we won't need this in release builds, as we should be able
	//			to just look up the index in fwMapDataStore and store that. In development
	//			builds we probably want to keep track of this so we can remember strings
	//			for the editor and debug drawing, even if they are not valid IMAP names.
	atHashValue		m_RequiredImapHash;
#endif	// SCENARIO_DEBUG

	// PURPOSE:	Index of a slot in fwMapDataStore representing a required IMAP file,
	//			or kRequiredImapIndexInMapDataStoreNotFound or kRequiredImapIndexInMapDataStoreNotSet.
	u16				m_RequiredImapIndexInMapDataStore;
	u16				m_Padding;
};

//-----------------------------------------------------------------------------

#endif	// TASK_SCENARIO_SCENARIO_POINT_H

// End of file 'task/Scenario/ScenarioPoint.h'
