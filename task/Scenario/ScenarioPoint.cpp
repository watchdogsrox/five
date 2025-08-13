//
// task/Scenario/ScenarioPoint.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "task/Scenario/ScenarioPoint.h"

//Framework headers
#include "ai/task/taskchannel.h"
#include "fwscene/stores/mapdatastore.h"

//Game headers
#include "ai/ambient/AmbientModelSetManager.h"
#include "scene/2dEffect.h"
#include "scene/Entity.h"
#include "task/Scenario/ScenarioDebug.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPointGroup.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/info/ScenarioInfo.h"
#include "task/Scenario/info/ScenarioInfoManager.h"
#include "Vehicles/cargen.h"

AI_OPTIMISATIONS()

// We currently fit CScenarioPoint in 48 bytes and it would be unfortunate if
// we added something that made it go up to 64 bytes. If we really have to, we can
// do it and disable this assert, but we should first look carefully and see if
// there isn't some other way of squeezing it in. With the current pool sizes,
// this corresponds to more than 80 kB of memory.
#if __PS3 || __XENON
#if !REGREF_VALIDATION
CompileTimeAssert(sizeof(CScenarioPoint) <= 48);
#endif
#endif	// __PS3 || __XENON

///////////////////////////////////////////////////////////////////////////////
//  CSpawnPointOverrideExtension
///////////////////////////////////////////////////////////////////////////////
AUTOID_IMPL(CSpawnPointOverrideExtension);

void CSpawnPointOverrideExtension::ApplyToPoint(CScenarioPoint& point)
{
	//If there is a scenario type override
	if (m_ScenarioType.GetHash())
	{
		s32 scenariotype = SCENARIOINFOMGR.GetScenarioIndex(m_ScenarioType, true, true);
		if(!taskVerifyf(scenariotype!=-1,"Override with unknown scenario type (%s) [%d][0x%08X]", m_ScenarioType.GetCStr(), m_ScenarioType.GetHash() , m_ScenarioType.GetHash() ))
		{
			// This will arbitrarily use type 0 instead, which is not going to be correct, but should avoid a crash.
			scenariotype = 0;
		}
		point.SetScenarioType((unsigned)scenariotype);
	}

	//If there is a group override
	if (m_Group.GetHash())
	{
		point.SetScenarioGroup((u16)SCENARIOPOINTMGR.FindGroupByNameHash(m_Group));
	}
	
	//If there is a start or end time override
	if (m_iTimeStartOverride || m_iTimeEndOverride)
	{
		point.SetTimeStartOverride(m_iTimeStartOverride);
		point.SetTimeEndOverride(m_iTimeEndOverride);
	}

	if(m_ModelSet.GetHash() != 0)
	{
		m_ModelSet = CScenarioManager::LegacyDefaultModelSetConvert(m_ModelSet);
		point.DetermineModelSetId(m_ModelSet, CScenarioManager::IsVehicleScenarioType(point.GetScenarioTypeVirtualOrReal()));
	}

	// We apparently have two different enums for this now, and we probably shouldn't assume
	// that they are kept up to date.
	if(m_AvailabilityInMpSp == CSpawnPoint::kOnlyMp)
	{
		point.SetAvailability(CScenarioPoint::kOnlyMp);
	}
	else if(m_AvailabilityInMpSp == CSpawnPoint::kOnlySp)
	{
		point.SetAvailability(CScenarioPoint::kOnlySp);
	}
	else
	{
		taskAssert(m_AvailabilityInMpSp == CSpawnPoint::kBoth);
	}

	if(m_Flags.AreAnySet())
	{
		point.SetAllFlagsFromBitSet(m_Flags);
	}

	if(m_Radius > 0.0f)
	{
		point.SetRadius(m_Radius);
	}

	if(m_TimeTillPedLeaves != 0.0f)
	{
		point.SetTimeTillPedLeaves(m_TimeTillPedLeaves);
	}
}

void CSpawnPointOverrideExtension::InitEntityExtensionFromDefinition(const fwExtensionDef* definition, fwEntity* entity)
{
	fwExtension::InitEntityExtensionFromDefinition( definition, entity );
	const CExtensionDefSpawnPointOverride&	ScenarioPointDef = *smart_cast< const CExtensionDefSpawnPointOverride* >( definition );

	m_ScenarioType = ScenarioPointDef.m_ScenarioType;
	m_iTimeStartOverride = ScenarioPointDef.m_iTimeStartOverride;
	m_iTimeEndOverride = ScenarioPointDef.m_iTimeEndOverride;
	m_Group = ScenarioPointDef.m_Group;
	m_ModelSet = ScenarioPointDef.m_ModelSet;
	m_AvailabilityInMpSp = ScenarioPointDef.m_AvailabilityInMpSp;
	m_Flags = ScenarioPointDef.m_Flags;
	m_Radius = ScenarioPointDef.m_Radius;
	m_TimeTillPedLeaves = ScenarioPointDef.m_TimeTillPedLeaves;
}

///////////////////////////////////////////////////////////////////////////////
//  CScenarioPoint
///////////////////////////////////////////////////////////////////////////////
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CScenarioPoint, CONFIGURED_FROM_FILE, 0.58f, atHashString("CScenarioPoint",0x30a617de));

u8 CScenarioPoint::sm_FailedCollisionDelay = 5;
dev_float CScenarioPoint::ms_fRadiusUnitsPerMeter = 1.0f; 

#if SCENARIO_DEBUG

int CScenarioPoint::sm_NumEditorPoints;

CScenarioPoint* CScenarioPoint::CreateForEditor()
{
	sm_NumEditorPoints++;
	sysMemStartDebug();
	CScenarioPoint* pt = rage_new CScenarioPoint;
	sysMemEndDebug();
	return pt;
}


void CScenarioPoint::DestroyForEditor(CScenarioPoint& pt)
{
	// Shouldn't happen when everything is working correctly, but if it does, fail an
	// assert and delete safely. Better than crashing for the scenario editor users.
	if(_ms_pPool->IsValidPtr(&pt))
	{
		Assert(0);
		DestroyFromPool(pt);
		return;
	}

	Assert(sm_NumEditorPoints > 0);
	sm_NumEditorPoints--;

	Assert(!_ms_pPool->IsValidPtr(&pt));
	sysMemStartDebug();
	delete &pt;
	sysMemEndDebug();
}

#endif	// SCENARIO_DEBUG

CScenarioPoint::CScenarioPoint()
: fwRefAwareBaseNonVirt()
, m_pEntity(NULL)
, m_iInterior(CScenarioPointManager::kNoInterior)
, m_vPositionAndDirection(V_ZERO)
, m_iUses(0)
, m_uInteriorState(IS_Unknown)
, m_iScenarioGroup(CScenarioPointManager::kNoGroup)
, m_iProbability(kProbDefaultFromType)
, m_iTimeStartOverride(0)
, m_iTimeEndOverride(24)
, m_uAvailableInMpSp(kBoth)
, m_Flags(0)
, m_RtFlags(0)
, m_iTimeTillPedLeaves(kTimeTillPedLeavesNone)
, m_iFailedCollisionCheckTimeCount(0)
#if SCENARIO_DEBUG
, m_bEditable(false)
#endif // SCENARIO_DEBUG
, m_CarGenId(kCarGenIdInvalid)
, m_iRadius(0)
, m_iRequiredIMapId(CScenarioPointManager::kNoRequiredIMap)
{
	SetType(0);
	SetModelSetIndex(CScenarioPointManager::kNoCustomModelSet);
}

CScenarioPoint::CScenarioPoint(CSpawnPoint* basePoint, CEntity* attachedTo /*= NULL*/)
: fwRefAwareBaseNonVirt()
, m_pEntity(attachedTo)
, m_iInterior(CScenarioPointManager::kNoInterior)
, m_vPositionAndDirection(V_ZERO)
, m_iUses(0)
, m_uInteriorState(IS_Unknown)
, m_iScenarioGroup(CScenarioPointManager::kNoGroup)
, m_iProbability(kProbDefaultFromType)
, m_iTimeStartOverride(0)
, m_iTimeEndOverride(24)
, m_uAvailableInMpSp(kBoth)
, m_Flags(0)
, m_RtFlags(0)
, m_iTimeTillPedLeaves(kTimeTillPedLeavesNone)
, m_iFailedCollisionCheckTimeCount(0)
#if SCENARIO_DEBUG
, m_bEditable(false)
#endif // SCENARIO_DEBUG
, m_CarGenId(kCarGenIdInvalid)
, m_iRadius(0)
, m_iRequiredIMapId(CScenarioPointManager::kNoRequiredIMap)
{
	SetType(0);
	SetModelSetIndex(CScenarioPointManager::kNoCustomModelSet);

	if (basePoint)
	{
		Vector3 pos;
		basePoint->GetPos(pos);
		m_vPositionAndDirection.SetXYZ(VECTOR3_TO_VEC3V(pos));
		m_iUses = basePoint->iUses;
		m_vPositionAndDirection.SetWf(basePoint->m_fDirection);
		m_uInteriorState = basePoint->iInteriorState;
		SetType(basePoint->iType);
		m_iScenarioGroup = basePoint->iScenarioGroup;
		m_iTimeStartOverride = basePoint->iTimeStartOverride;
		m_iTimeEndOverride = basePoint->iTimeEndOverride;
		m_uAvailableInMpSp = basePoint->iAvailableInMpSp;
		m_iTimeTillPedLeaves = basePoint->iTimeTillPedLeaves;

		SetHighPriority(basePoint->m_bHighPriority);
		SetExtendedRange(basePoint->m_bExtendedRange);
		SetShortRange(basePoint->m_bShortRange);
		m_Flags = basePoint->m_Flags;

		basePoint->m_uiModelSetHash = CScenarioManager::LegacyDefaultModelSetConvert(basePoint->m_uiModelSetHash);
		DetermineModelSetId(basePoint->m_uiModelSetHash, CScenarioManager::IsVehicleScenarioType(GetType()));

#if SCENARIO_DEBUG
		m_bEditable = basePoint->bEditable;
#endif // SCENARIO_DEBUG
	}
}

CScenarioPoint::~CScenarioPoint()
{
	CleanUp();
}

CScenarioPoint::CScenarioPoint(const CScenarioPoint& tobe)
{
	*this = tobe;
}

CScenarioPoint& CScenarioPoint::operator=(const CScenarioPoint& tobe)
{ 			 
	if (this == &tobe)
		return *this;

	// Copying a point that has a car generator ID, or for which otherwise
	// CScenarioManager::OnAddScenario() has been called, is problematic.
	// Possibly it could be permitted if we treat the copy as not having
	// been added, i.e. clear out the OnAdd flag and the car generator ID,
	// but for now, the goal has been to avoid this.
	Assert(!tobe.GetOnAddCalled());
	Assert(tobe.m_CarGenId == kCarGenIdInvalid);
		
	//Runtime data
	m_pEntity = tobe.m_pEntity;
	m_RtFlags = tobe.m_RtFlags;
	m_iUses = 0;//tobe.m_iUses; dont need to copy this because this is not the same pointer ... 
	m_uInteriorState = tobe.m_uInteriorState;
	m_CarGenId = tobe.m_CarGenId;
#if SCENARIO_DEBUG
	m_bEditable = tobe.m_bEditable;
#endif // SCENARIO_DEBUG
	m_iFailedCollisionCheckTimeCount = tobe.m_iFailedCollisionCheckTimeCount;

	//PSO data
	m_vPositionAndDirection = tobe.m_vPositionAndDirection;
	m_Flags = tobe.m_Flags;
	m_iScenarioGroup = tobe.m_iScenarioGroup;
	SetModelSetIndex(tobe.GetModelSetIndex());
	m_iProbability = tobe.m_iProbability;
	m_uAvailableInMpSp = tobe.m_uAvailableInMpSp;
	m_iTimeStartOverride = tobe.m_iTimeStartOverride;
	m_iTimeEndOverride = tobe.m_iTimeEndOverride;
	m_iInterior = tobe.m_iInterior;
	m_iRadius = tobe.m_iRadius;
	SetType(tobe.GetType());
	m_iTimeTillPedLeaves = tobe.m_iTimeTillPedLeaves;
	m_iRequiredIMapId = tobe.m_iRequiredIMapId;

	return *this; 
}

#if SCENARIO_DEBUG
bool CScenarioPoint::IsEditable() const 
{ 
	return m_bEditable;
}

void CScenarioPoint::SetEditable(bool val)
{
	m_bEditable = val;
}

void CScenarioPoint::FindDebugColourAndText(Color32 *col, char *string, int buffSize) const
{
	Assert(col);
	Assert(string);
	col->Set(150,222,209,255); //egg shell blue B* 316623

	const char* spawnInfo = "";
	if((m_pEntity && (!m_pEntity->m_nFlags.bWillSpawnPeds && !IsEntityOverridePoint())) || IsFlagSet(CScenarioPointFlags::NoSpawn))
	{
		spawnInfo = " NoSpawn";
	}

	char chainInfo[32];
	chainInfo[0] = '\0';

	if (IsChained())
	{
		if (IsUsedChainFull())
		{
			formatf(chainInfo, " InFullChain%d", SCENARIOPOINTMGR.FindPointsChainedIndex(*this));
		}
		else
		{
			formatf(chainInfo, " InChain%d", SCENARIOPOINTMGR.FindPointsChainedIndex(*this));
		}
	}

	char clusterInfo[32];
	clusterInfo[0] = '\0';
	if (IsInCluster())
	{
		formatf(clusterInfo, " Cluster%d", SCENARIOPOINTMGR.FindPointsClusterIndex(*this));
	}

	const char* hiPriInfo = "";
	if (IsHighPriority())
	{
		hiPriInfo = " HiPri";
	}

	const char* inWaterInfo = "";
	if (IsInWater())
	{
		inWaterInfo = " InWater";
	}

	const char* investigationInfo = "";
	if (DoesAllowInvestigation())
	{
		investigationInfo = " AllowsInvestigation";
	}

	const char* openDoor = "";
	if (GetOpenDoor())
	{
		openDoor = " OpenDoor";
	}

 	const char* rsc = "";
	if (!IsFromRsc())
	{
		rsc = " NonRsc";
		col->Set(222,150,209,255); //Non RSC things are a different colour
	}

	char ptrAddress[32];
	ptrAddress[0] = '\0';
	if (CScenarioDebug::ms_bRenderPointAddress)
	{
		formatf(ptrAddress, " [%p]", this);
	}

	formatf(string, buffSize, "Sp:%s, %d%s%s%s%s%s%s%s%s%s", GetScenarioName(), m_iUses, spawnInfo, chainInfo, clusterInfo, hiPriInfo, inWaterInfo, investigationInfo, openDoor, rsc, ptrAddress);
}

const char* CScenarioPoint::GetScenarioName() const
{
	return CScenarioManager::GetScenarioName(GetType());
}

#endif	// SCENARIO_DEBUG

void CScenarioPoint::PostPsoPlace(void* data)
{
	//ONLY PUT NON-PSO data init in here ...
	Assert(data);
	CScenarioPoint* initData = reinterpret_cast<CScenarioPoint*>(data);

	//From base fwRefAwareBase
	initData->m_pKnownRefHolderHead = NULL;

	//Local members ... 
	initData->m_pEntity = NULL;
	initData->m_RtFlags = 0;
	initData->m_iUses = 0;
	initData->m_uInteriorState = IS_Unknown;
	initData->m_CarGenId = kCarGenIdInvalid;
#if SCENARIO_DEBUG
	initData->m_bEditable = false;
#endif // SCENARIO_DEBUG
	initData->m_iFailedCollisionCheckTimeCount = 0;
}

void CScenarioPoint::CleanUp()
{
	Assert(!GetOnAddCalled());
	Assert(m_CarGenId == kCarGenIdInvalid);

	// If this point has a CScenarioPointExtraData object associated with it, destroy it.
	SCENARIOPOINTMGR.DestroyExtraData(*this);

	ClearAllKnownRefs();

	CPedPopulation::InvalidateCachedScenarioPoints();
}

void CScenarioPoint::Reset()
{
	m_vPositionAndDirection = Vec4V(V_ZERO);
	m_iInterior = CScenarioPointManager::kNoInterior;
	m_iUses = 0;
	m_uInteriorState = IS_Unknown;
	SetModelSetIndex(CScenarioPointManager::kNoCustomModelSet);
	SetType(0);
	m_iScenarioGroup = CScenarioPointManager::kNoGroup;
	m_iTimeStartOverride = 0;
	m_iTimeEndOverride = 24;
	m_uAvailableInMpSp = kBoth;
	m_Flags.Reset();
	m_iTimeTillPedLeaves = kTimeTillPedLeavesNone;
	m_iRequiredIMapId = CScenarioPointManager::kNoRequiredIMap;
#if SCENARIO_DEBUG
	m_bEditable = false;
#endif // SCENARIO_DEBUG
	m_iRadius = 0;
}

unsigned CScenarioPoint::GetScenarioTypeVirtualOrReal() const
{
	return GetType();
}


int CScenarioPoint::GetScenarioTypeReal(unsigned indexWithinTypeGroup) const
{
	return SCENARIOINFOMGR.GetRealScenarioType(GetType(), indexWithinTypeGroup);
}


void CScenarioPoint::SetScenarioType(unsigned scenarioType)
{
	SetType(scenarioType);
}

// Logic here is now slightly more complex as script can change the priority of a point group now.
bool CScenarioPoint::IsHighPriority() const
{
	return CScenarioPointPriorityManager::GetInstance().CheckPriority(*this);
}

#if __ASSERT

bool CScenarioPoint::IsRealScenarioTypeValidForThis(int realScenarioType) const
{
	if(realScenarioType < 0)
	{
		return false;
	}
	if(SCENARIOINFOMGR.IsVirtualIndex(realScenarioType))
	{
		return false;
	}
	const int scenarioTypeFromPoint = GetScenarioTypeVirtualOrReal();
	if(!SCENARIOINFOMGR.IsVirtualIndex(scenarioTypeFromPoint))
	{
		if(scenarioTypeFromPoint != realScenarioType)
		{
			// This next check is basically a hack, and is here because when we use CTaskParkedVehicleScenario,
			// we create a temporary CScenarioPoint on the stack, and it won't have the
			// proper type set, it's just initialized to 0 by the constructor.
			if(scenarioTypeFromPoint != 0)
			{
				return false;
			}
		}
	}

	// Note: one additional check we could do is to see if realScenarioType is one
	// of the possible real scenario types for scenarioTypeFromPoint.

	return true;
}

#endif	// __ASSERT

void CScenarioPoint::SetWorldPosition(Vec3V_In pos)
{
	Vec3V localPosV = pos;
	if(m_pEntity)
	{
		localPosV = m_pEntity->GetTransform().UnTransform(localPosV);
	}
	SetLocalPosition(localPosV);
}


Vec3V_Out CScenarioPoint::GetWorldPosition() const
{
	const Vec3V posV = m_vPositionAndDirection.GetXYZ();

  if (m_pEntity)
  {
		Vec3V scaledV = Scale(posV, m_pEntity->GetTransform().GetScaleV());
    return VECTOR3_TO_VEC3V(m_pEntity->TransformIntoWorldSpace(VEC3V_TO_VECTOR3(scaledV))) ;
  }
  return posV;
}


void CScenarioPoint::SetWorldDirection(float dir)
{
	if(m_pEntity)
	{
		const Vec3V worldFwdVecV = RotateAboutZAxis(Vec3V(V_Y_AXIS_WZERO), ScalarV(dir));
		const Vec3V localFwdVecV = m_pEntity->GetTransform().UnTransform3x3(worldFwdVecV);
		const float x = localFwdVecV.GetXf();
		const float y = localFwdVecV.GetYf();
		if(x*x + y*y > square(0.01f))
		{
			dir = rage::Atan2f(-x, y);
		}
		else
		{
			dir = 0.0f;
		}
	}

	SetLocalDirection(dir);
}


Vec3V_Out CScenarioPoint::GetWorldDirectionFwd() const
{
    Vec3V vDir(V_Y_AXIS_WZERO);
    vDir = RotateAboutZAxis(vDir, m_vPositionAndDirection.GetW());
    if( m_pEntity )
    {
        vDir = m_pEntity->GetTransform().Transform3x3(vDir);
    }
    return vDir;
}

void CScenarioPoint::GetLocalDirection(QuatV_Ref quatOut) const
{
#if 0
	// TODO -- to avoid LHS stalls, use this:
	// (need to #inlude "fwmaths/vectorutil.h")
	quatOut = QuatV(Vec2V(V_ZERO), SinCos(m_vPositionAndDirection.GetW()*ScalarV(V_HALF)));
#else
	const float halfAngle = m_vPositionAndDirection.GetWf()*0.5f;

	float cosHalfAngle, sinHalfAngle;
	cos_and_sin(cosHalfAngle, sinHalfAngle, halfAngle);

	quatOut.SetXf(0.0f);
	quatOut.SetYf(0.0f);
	quatOut.SetZf(sinHalfAngle);
	quatOut.SetWf(cosHalfAngle);
#endif
}

void CScenarioPoint::AddUse()
{ 
	++m_iUses;
	taskAssertf(m_iUses > 0,"Scenario point usage incorrect");
}

void CScenarioPoint::RemoveUse() 
{ 
	taskAssertf(m_iUses > 0,"Scenario point usage incorrect");
	--m_iUses;
}

u8 CScenarioPoint::GetUses() const
{ 
    return m_iUses;
}

bool CScenarioPoint::HasTimeOverride() const 
{ 
	return (m_iTimeStartOverride !=0 || m_iTimeEndOverride != 0); 
}

u8 CScenarioPoint::GetTimeStartOverride() const
{ 
	return m_iTimeStartOverride; 
}

void CScenarioPoint::SetTimeStartOverride(u8 over)
{
	m_iTimeStartOverride = over;
}

u8 CScenarioPoint::GetTimeEndOverride() const 
{ 
	return m_iTimeEndOverride; 
}

void CScenarioPoint::SetTimeEndOverride(u8 over)
{
	m_iTimeEndOverride = over;
}

float CScenarioPoint::GetTimeTillPedLeaves() const
{
	if(m_iTimeTillPedLeaves == kTimeTillPedLeavesInfinite)
	{
		return -1.0f;
	}
	return (float)m_iTimeTillPedLeaves;
}

bool CScenarioPoint::HasTimeTillPedLeaves() const
{	
	return (m_iTimeTillPedLeaves != kTimeTillPedLeavesNone);
}

bool CScenarioPoint::IsInteriorState(InteriorState query) const 
{ 
	return (m_uInteriorState == static_cast<u32>(query)); 
}

void CScenarioPoint::SetInteriorState(InteriorState state) 
{ 
	m_uInteriorState = state; 
}

unsigned CScenarioPoint::GetModelSetIndex() const
{
	return (m_ModelSetIdHiBits << 8) | m_ModelSetId;
}

void CScenarioPoint::SetModelSetIndex(unsigned index)
{
	Assertf(index < MAX_MODELSET_TYPES, "CScenarioPoint::SetModelSetIndex:  modelset index %d is greater than maximum %d", index, MAX_MODELSET_TYPES);
	m_ModelSetIdHiBits = (index >> 8) & MODELSET_TYPE_EXTENSION_MASK;
	m_ModelSetId = index & 0xff;
}

bool CScenarioPoint::IsAvailability(AvailabilityMpSp query) const
{
	return (m_uAvailableInMpSp == static_cast<u32>(query));
}

void CScenarioPoint::SetAvailability(AvailabilityMpSp avail)
{
	Assign(m_uAvailableInMpSp, avail);
}

#if SCENARIO_DEBUG
u8 CScenarioPoint::GetAvailability() const //Used by the editor only.
{
	return m_uAvailableInMpSp;
}
#endif // SCENARIO_DEBUG

unsigned CScenarioPoint::GetScenarioGroup() const
{
	return m_iScenarioGroup;
}

void CScenarioPoint::SetScenarioGroup(unsigned sgroup)
{
	Assign(m_iScenarioGroup, sgroup);
}

unsigned CScenarioPoint::GetRequiredIMap() const
{
	return m_iRequiredIMapId;
}

void CScenarioPoint::SetRequiredIMap(unsigned index)
{
	Assign(m_iRequiredIMapId, index);
}


void CScenarioPoint::SetProbabilityOverride(float f)
{
	if(f <= 0.0f)
	{
		ClearProbabilityOverride();
		return;
	}

	int probVal = (int)floorf(f*(100.0f/(float)kProbResolution) + 0.5f);
	probVal = Max(1, probVal);
	Assert(probVal < (1 << 5));
	Assign(m_iProbability, probVal);
}


void CScenarioPoint::InitArchetypeExtensionFromDefinition(const fwExtensionDef* definition, fwArchetype* archetype)
{
	const CExtensionDefSpawnPoint& def = *smart_cast<const CExtensionDefSpawnPoint*>(definition);

	m_vPositionAndDirection.SetXYZ(VECTOR3_TO_VEC3V(def.m_offsetPosition));

	SetRadius(def.m_radius);
	s32 scenariotype = SCENARIOINFOMGR.GetScenarioIndex(def.m_spawnType, true, true);

	if(!taskVerifyf(scenariotype!=-1,"Unknown scenario type (%s) at coords (%.2f, %.2f, %.2f) [%d][0x%08X]", def.m_spawnType.GetCStr(), def.m_offsetPosition.x, def.m_offsetPosition.y, def.m_offsetPosition.z, def.m_spawnType.GetHash() , def.m_spawnType.GetHash() ))
	{
		// This will arbitrarily use type 0 instead, which is not going to be correct, but should avoid a crash.
		scenariotype = 0;
	}
	SetType(scenariotype);

	if(def.m_interior.GetHash() != 0)
	{
		SetInterior(SCENARIOPOINTMGR.FindOrAddInteriorName(def.m_interior.GetHash()));
	}
	else
	{
		SetInterior(CScenarioPointManager::kNoInterior);
	}

	if(def.m_requiredImap.GetHash() != 0)
	{
		SetRequiredIMap(SCENARIOPOINTMGR.FindOrAddRequiredIMapByHash(def.m_requiredImap));
	}
	else
	{
		SetRequiredIMap(CScenarioPointManager::kNoRequiredIMap);
	}

	Matrix34 mat;
	mat.FromQuaternion(Quaternion(def.m_offsetRotation.x, def.m_offsetRotation.y, def.m_offsetRotation.z, def.m_offsetRotation.w));
	Vector3 vDir;
	vDir = mat.b;
	m_vPositionAndDirection.SetWf(rage::Atan2f(-vDir.x, vDir.y));

	m_iUses = 0;
	Assert(def.m_start < (1 << 5));		// Only use 5 bits for this, expected range is 0..24.
	Assert(def.m_end < (1 << 5));		// Only use 5 bits for this, expected range is 0..24.
	m_iTimeStartOverride = def.m_start;
	m_iTimeEndOverride =  def.m_end;
	m_uInteriorState = IS_Unknown;

	SetProbabilityOverride(def.m_probability);
	SetTimeTillPedLeaves(def.m_timeTillPedLeaves);

	// This part was added just to fix up existing data in temporarily bad PSO files.
	// Once the PSO files have been rebuilt, it should be possible to remove this
	// (it should never be hit).
	unsigned int avail = def.m_availableInMpSp;
	if(avail != kBoth && avail != kOnlyMp && avail != kOnlySp)
	{
		avail = kBoth;
	}

	Assign(m_uAvailableInMpSp, avail);
	SetExtendedRange(def.m_extendedRange);
	SetShortRange(def.m_shortRange);
	SetHighPriority(def.m_highPri);
	m_Flags = def.m_flags;

	Assign(m_iScenarioGroup, SCENARIOPOINTMGR.FindGroupByNameHash(def.m_group));
	
	atHashWithStringNotFinal usedModelSet = CScenarioManager::LegacyDefaultModelSetConvert(def.m_pedType);
	if (!DetermineModelSetId(usedModelSet, CScenarioManager::IsVehicleScenarioType(scenariotype)))
	{
		if( archetype )
		{
			taskAssertf(0,	"Scenario (%s) on object (%s) has unknown model override (%s). NOTE: If you save out this region the model set link will be lost!", def.m_spawnType.GetCStr(), archetype->GetModelName(), usedModelSet.GetCStr() );
		}
		else
		{
			taskAssertf(0,	"Scenario (%s) at coords (%.2f, %.2f, %.2f) has unknown model override (%s). NOTE: If you save out this region the model set link will be lost!", def.m_spawnType.GetCStr(), def.m_offsetPosition.x, def.m_offsetPosition.y, def.m_offsetPosition.z, usedModelSet.GetCStr() );
		}
	}
}

bool CScenarioPoint::DetermineModelSetId(atHashWithStringNotFinal modelSetHash, const bool isVehicleType)
{
	SetModelSetIndex(CScenarioPointManager::kNoCustomModelSet);
	int foundIndex = -1;
	bool validInit = true;

	// If there is a model set override, check if it's valid.
	if(modelSetHash!=CScenarioManager::MODEL_SET_USE_POPULATION)
	{
		// Look up the ped or vehicle model set from the hash, depending on what type of scenario this is.
		CAmbientModelSetManager::ModelSetType setType = CAmbientModelSetManager::kPedModelSets;
		if(isVehicleType)
		{
			setType = CAmbientModelSetManager::kVehicleModelSets;
		}

		foundIndex = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(setType, modelSetHash);

		if(foundIndex == -1)
		{
			validInit = false;
			foundIndex = CScenarioPointManager::kNoCustomModelSet;
		}
		SetModelSetIndex(foundIndex);
	}
	return validInit;
}

#if SCENARIO_DEBUG

void CScenarioPoint::CopyToDefinition(fwExtensionDef * definition) const
{
	CExtensionDefSpawnPoint& def = *smart_cast<CExtensionDefSpawnPoint*>(definition);

	def.m_offsetPosition = VEC3V_TO_VECTOR3(m_vPositionAndDirection.GetXYZ());
	def.m_radius = GetRadius();

	def.m_spawnType = SCENARIOINFOMGR.GetHashForScenario(GetType());

	Quaternion q;
	q.FromRotation(ZAXIS, m_vPositionAndDirection.GetWf());
	def.m_offsetRotation.Set(q.xyzw);

	def.m_start = m_iTimeStartOverride;
	def.m_end = m_iTimeEndOverride;
	def.m_availableInMpSp = (CSpawnPoint::AvailabilityMpSp)m_uAvailableInMpSp;
	//mjc - fix m_uInteriorState

	def.m_pedType = CScenarioManager::MODEL_SET_USE_POPULATION;
	if (GetModelSetIndex() != CScenarioPointManager::kNoCustomModelSet)
	{
		// Look up the ped or vehicle model set from the hash, depending on what type of scenario this is.
		CAmbientModelSetManager::ModelSetType setType = CAmbientModelSetManager::kPedModelSets;
		if(CScenarioManager::IsVehicleScenarioType(GetType()))
		{
			setType = CAmbientModelSetManager::kVehicleModelSets;
		}
		def.m_pedType = CAmbientModelSetManager::GetInstance().GetModelSet(setType, GetModelSetIndex()).GetHash();
	}

	if(HasProbabilityOverride())
	{
		def.m_probability = GetProbabilityOverride();
	}
	else
	{
		def.m_probability = 0.0f;
	}
	def.m_timeTillPedLeaves = GetTimeTillPedLeaves();

	def.m_extendedRange= HasExtendedRange();
	def.m_shortRange = HasShortRange();
	def.m_highPri = IsTrulyHighPriority();

	def.m_flags.Reset();
	def.m_flags = m_Flags;

	if(m_iScenarioGroup != CScenarioPointManager::kNoGroup)
	{
		def.m_group = SCENARIOPOINTMGR.GetGroupByIndex(m_iScenarioGroup - 1).GetNameHashString();
	}
	else
	{
		def.m_group.Clear();
	}

	if(m_iInterior != CScenarioPointManager::kNoInterior)
	{
		def.m_interior = SCENARIOPOINTMGR.GetInteriorNameHash(m_iInterior - 1);
	}
	else
	{
		def.m_interior.Clear();
	}

	if (m_iRequiredIMapId != CScenarioPointManager::kNoRequiredIMap)
	{
		def.m_requiredImap = SCENARIOPOINTMGR.GetRequiredIMapHash(m_iRequiredIMapId);
	}
	else
	{
		def.m_requiredImap.Clear();
	}
}

void CScenarioPoint::PrepSaveObject(const CScenarioPoint& prepFrom)
{
	//copy everything except the runtime data.
	Reset();

	SetModelSetIndex(prepFrom.GetModelSetIndex());
	m_iProbability = prepFrom.m_iProbability;
	m_uAvailableInMpSp = prepFrom.m_uAvailableInMpSp;
	m_iTimeStartOverride = prepFrom.m_iTimeStartOverride;
	m_iTimeEndOverride = prepFrom.m_iTimeEndOverride;
	m_iInterior = prepFrom.m_iInterior;
	m_iRadius = prepFrom.m_iRadius;
	SetType(prepFrom.GetType());
	m_iTimeTillPedLeaves = prepFrom.m_iTimeTillPedLeaves;
	m_iRequiredIMapId = prepFrom.m_iRequiredIMapId;
	m_iScenarioGroup = prepFrom.m_iScenarioGroup;
	m_Flags = prepFrom.m_Flags;
	m_vPositionAndDirection = prepFrom.m_vPositionAndDirection;
}

#endif	// SCENARIO_DEBUG

CCarGenerator* CScenarioPoint::GetCarGen() const
{
	CCarGenerator* pCarGen = NULL;
	if(m_CarGenId != CScenarioPoint::kCarGenIdInvalid)
	{
		pCarGen = CTheCarGenerators::GetCarGenerator(m_CarGenId);
		taskAssert(pCarGen);
	}
	return pCarGen;
}


void CScenarioPoint::SetTimeTillPedLeaves(float timeInSeconds)
{
	if(timeInSeconds < 0.0f)
	{
		// Any negative number is interpreted as infinite.
		m_iTimeTillPedLeaves = (u8)kTimeTillPedLeavesInfinite;
		return;
	}
	else if(timeInSeconds <= SMALL_FLOAT)
	{
		// No override, use what's in CScenarioInfo.
		m_iTimeTillPedLeaves = kTimeTillPedLeavesNone;
		return;
	}

	// If it's a small but non-zero value, we wouldn't want it to be stored as zero (kTimeTillPedLeavesNone).
	timeInSeconds = Max(timeInSeconds, 0.5f);

	// Round, convert to integer, and clamp.
	const unsigned int rounded = (unsigned int)floorf(timeInSeconds + 0.5f);
	const unsigned int clamped = Min(rounded, (unsigned int)kTimeTillPedLeavesMax);

	// Note: I was considering a more sophisticated representation, using
	// different quantization for different ranges of values. In the end, unless we
	// really need large values, high precision, or a few more bits for other things,
	// it didn't seem to be worth the extra code complexity.

	m_iTimeTillPedLeaves = (u8)clamped;
}


u8 CScenarioPoint::GetFailedCollisionTicks()
{
	// Start with the main game timer in ms.
	unsigned int ms = fwTimer::GetTimeInMilliseconds();

	// Shifting by 10 is close to dividing by 1000, i.e. giving us a value in seconds.
	unsigned int ticks = (ms >> 10);

	// Return it, but never let it go to zero (as we use that to indicate no collision history).
	return (u8)(ticks ? ticks : 1);
}

unsigned CScenarioPoint::GetType() const
{
	return (m_iTypeHiBits << 8) | m_iType;
}

void CScenarioPoint::SetType(unsigned index)
{
	Assertf(index < MAX_SCENARIO_TYPES, "CScenarioPoint::SetType:  Type index %d is greater than maximum %d", index, MAX_SCENARIO_TYPES);
	m_iTypeHiBits = (index >> 8) & SCENARIO_TYPE_EXTENSION_MASK;
	m_iType = index & 0xff;
}

//-----------------------------------------------------------------------------
// CScenarioPointExtraData

FW_INSTANTIATE_CLASS_POOL(CScenarioPointExtraData, CONFIGURED_FROM_FILE, atHashString("CScenarioPointExtraData",0x13889110));

CScenarioPointExtraData::CScenarioPointExtraData()
		: m_RequiredImapIndexInMapDataStore(kRequiredImapIndexInMapDataStoreNotSet)
{
}

int CScenarioPointExtraData::GetRequiredImapIndexInMapDataStore() const
{
	if(m_RequiredImapIndexInMapDataStore < CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound)
	{
		return m_RequiredImapIndexInMapDataStore;
	}
	else
	{
		return -1;
	}
}

void CScenarioPointExtraData::SetRequiredImapHash(atHashValue hashVal)
{
	// In __BANK builds, store the hash for later retrieval.
#if SCENARIO_DEBUG
	m_RequiredImapHash = hashVal;
#endif // SCENARIO_DEBUG

	// If we have some valid hash value, try to look up the related map data store index.
	if(hashVal.GetHash() != 0)
	{
		Assertf(fwMapDataStore::GetStore().GetMaxSize() < CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound,
				"fwMapDataStore has size %d larger than %d, not supported by CScenarioPointExtraData.",
				fwMapDataStore::GetStore().GetMaxSize(), CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound);

		const int mapDataSlot = fwMapDataStore::GetStore().FindSlotFromHashKey(hashVal.GetHash()).Get();
		if(mapDataSlot >= 0)
		{
			Assert(mapDataSlot < CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound);
			m_RequiredImapIndexInMapDataStore = (u16)mapDataSlot;
		}
		else
		{
			m_RequiredImapIndexInMapDataStore = CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound;
		}
	}
	else
	{
		m_RequiredImapIndexInMapDataStore = kRequiredImapIndexInMapDataStoreNotSet;
	}
}

//-----------------------------------------------------------------------------

// End of file 'task/Scenario/ScenarioPoint.cpp'
