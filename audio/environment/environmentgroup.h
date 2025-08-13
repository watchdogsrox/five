//
// audio/environment/environmentgroup.h
//
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//

#ifndef NAENVIRONMENTGROUP_H
#define NAENVIRONMENTGROUP_H

#include "audioengine/environmentgroup.h"
#include "audioengine/widgets.h"
#include "audio/audioentity.h"
#include "audio/gameobjects.h"

#include "bank/bank.h"
#include "vector/vector3.h"
#include "vector/matrix34.h"

#include "pathserver/PathServer_DataTypes.h"
#include "system/criticalsection.h"
#include "Task/System/TaskHelpers.h"

class CInteriorInst;
class naAudioEntity;

class naEnvironmentGroupCriticalSection
{
public:
	naEnvironmentGroupCriticalSection()
	{
		if (sm_UseLock)
		{
			sm_Lock.Lock();
		}
	}

	~naEnvironmentGroupCriticalSection()
	{
		if (sm_UseLock)
		{
			sm_Lock.Unlock();
		}
	}

	static void SetUseLock(const bool useLock) { sm_UseLock = useLock; }
	static bool GetUseLock() { return sm_UseLock; }

private:
	static sysCriticalSectionToken sm_Lock;
	static bool sm_UseLock;
};

class naEnvironmentGroup : public audEnvironmentGroupInterface
{
public:
	enum mixgroupState
	{
		State_Default = 0,
		State_Adding,
		State_Removing,
		State_Added,
		State_Removed,
		State_Transitioning,
	};

	FW_REGISTER_LOCKING_CLASS_POOL(naEnvironmentGroup, naEnvironmentGroupCriticalSection);

	naEnvironmentGroup();
	virtual ~naEnvironmentGroup();
	void Init(naAudioEntity* entity, f32 cheapDistance,
			  u32 sourceEnvironmentUpdateTime = 0, u32 sourceEnvironmentCheapUpdateTime = 4000,
			  f32 sourceEnvironmentMinimumDistanceUpdate = 0.5f, u32 maxUpdateTime = 1000);

	void SetSourceEnvironmentUpdates(u32 sourceEnvironmentUpdateTime, u32 sourceEnvironmentCheapUpdateTime = 4000,
									 f32 sourceEnvironmentMinimumDistanceUpdate = 0.5f);
	void SetNotOccluded(bool notOccluded){m_NotOccluded = notOccluded;};

	// For use with unloading metadata chunks.  Clears out the InteriorSettings/InteriorRoom *'s, as well as the fwInteriorLocation.
	void ClearInteriorInfo();

	void SetInteriorInfoWithEntity(const CEntity* entity);
	void SetInteriorInfoWithInteriorLocation(const fwInteriorLocation interiorLocation);
	void SetInteriorInfoWithInteriorInstance(const CInteriorInst* pIntInst, const s32 roomIdx);
	void SetInteriorInfoWithInteriorHashkey(const u32 intSettingsHash, const u32 intRoomHash);
	void SetInteriorInfoWithInteriorGO(const InteriorSettings* interiorSettings, const InteriorRoom* intRoom);
	void SetInteriorMetrics(const InteriorSettings* intSettings, const InteriorRoom* intRoom);
	void SetInteriorLocation(const fwInteriorLocation intLoc);

	void SetUpdateTime(u32 updateTime) { Assign(m_SourceEnvironmentUpdateTime,updateTime); }

	void SetMixGroupFadeTime(f32 fadeTime) { m_FadeTime = fadeTime;};
	void SetApplyValue(u8 applyValue) { m_EnvironmentGameMetric.SetMixGroupApplyValue(applyValue);};
	void SetMixGroupState(mixgroupState state) { m_MixgroupState =  state; }
	void SetSource(const Vec3V& source);

	const CInteriorInst* GetInteriorInst() const;
	s32 GetRoomIdx() const;
	const Vector3& GetPosition() const { return m_Position; }
	void CachePositionLastUpdate() { m_PositionLastUpdate = m_Position; }
	const Vector3& GetPositionLastFrame() const { return m_PositionLastUpdate; }
	u8 GetMaxPathDepth() { return m_MaxPathDepth; }
	u32 GetLastUpdateTime() const { return m_LastUpdateTime; }
	void SetMaxUpdateTime(const u32 maxTime) { m_MaxUpdateTime = maxTime; }
	u32 GetMaxUpdateTime() const { return m_MaxUpdateTime; }

	void SetPlayPosition(const Vector3& playPosition) { m_EnvironmentGameMetric.SetOcclusionPathPosition(VECTOR3_TO_VEC3V(playPosition)); }
	void SetPathDistance(const f32 distance) { m_EnvironmentGameMetric.SetOcclusionPathDistance(distance); }
	void SetUseOcclusionPath(const bool usePath) { m_EnvironmentGameMetric.SetUseOcclusionPath(usePath); }
	bool GetUseOcclusionPath() { return m_EnvironmentGameMetric.GetUseOcclusionPath(); }

	bool IsUnderCover(){ return m_IsUnderCover;}
	bool HasGotUnderCoverResults(){ return m_HasGotUnderCoverResult;}

	f32 GetOcclusionValue() const { return m_OcclusionValue; }
	void SetExtraOcclusionValue(const f32 extraOcclusion) { m_ExtraOcclusion = extraOcclusion; }
	void SetMaxPathDepth(const u8 maxPathDepth) { m_MaxPathDepth = maxPathDepth; }
	void SetLastUpdateTime(u32 timeInMs) { m_LastUpdateTime = timeInMs; }
	void SetUsePortalOcclusion(const bool usePortalOcclusion) { m_UsePortalOcclusion = usePortalOcclusion; }

	void	Update(u32 UNUSED_PARAM(timeInMs)){};
	void	FullyUpdate(u32 timeInMs);
	void	UpdateSourceEnvironment(u32 timeInMs, const CEntity* owningEntity = NULL);
	void	ForceSourceEnvironmentUpdate(const CEntity* owningEntity);
	void	UpdateOcclusion(u32 timeInMs, const bool isPlayerEnvGroup);
	void	SetLinkedEnvironmentGroup(naEnvironmentGroup* linkedEnvironmentGroup) { m_LinkedEnvironmentGroup = linkedEnvironmentGroup; }
	naEnvironmentGroup*	GetLinkedEnvironmentGroup() const { return m_LinkedEnvironmentGroup; }

	void SetForcedSameRoomAsListenerRatio(const f32 ratio) { m_ForcedSameRoomAsListenerRatio = ratio; }
	void SetFillsInteriorSpace(const bool shouldFill) { m_FillsInteriorSpace = shouldFill; }
	bool IsCurrentlyFillingListenerSpace() const { return m_IsCurrentlyFillingListenerSpace; }

	void SetMaxLeakage(const f32 maxLeakage) { m_MaxLeakage = maxLeakage; }
	void SetMinLeakageDistance(const u16 minLeakageDistance) { m_MinLeakageDistance = minLeakageDistance; }
	void SetMaxLeakageDistance(const u16 maxLeakageDistance) { m_MaxLeakageDistance = maxLeakageDistance; }

	void SetLeaksInTaggedCutscenes(const bool shouldLeak) { m_LeaksInTaggedCutscenes = shouldLeak; }

	// For simple occlusiongroups like collision and vfx that are close together we share updates
	void SetUseCloseProximityGroupsForUpdates(bool useCloseGroups) { m_UseCloseProximityGroupsForUpdates = useCloseGroups; }
	bool GetUseCloseProximityGroupsForUpdates() { return m_UseCloseProximityGroupsForUpdates; }

	void SetUseInteriorDistanceMetrics(bool useInteriorDistanceMetrics) { m_UseInteriorDistanceMetrics = useInteriorDistanceMetrics; }
	bool GetUseInteriorDistanceMetrics() { return m_UseInteriorDistanceMetrics; }

	mixgroupState GetMixGroupState() { return m_MixgroupState; }

	bool ShouldCloseProximityGroupBeUsedForUpdate(naEnvironmentGroup* closeEnvironmentGroup, u32 timeInMs);

	f32 GetMixGroupFadeTime() {return m_FadeTime; }
	u8  GetApplyValue() { return m_EnvironmentGameMetric.GetMixGroupApplyValue();}
	bool IsInADifferentInteriorToListener();

	// Returns the cached interior location, whether it's portalAttached or roomAttached
	fwInteriorLocation GetInteriorLocation() const { return m_InteriorLocation; }
	const InteriorSettings* GetInteriorSettings() const { return m_InteriorSettings; }
	const InteriorRoom* GetInteriorRoom() const { return m_InteriorRoom; }

	// returns the cached interior location, unless it's portalAttached, in which case it finds a roomAttached version based on the players location
	fwInteriorLocation GetRoomAttachedInteriorLocation() const;

	void SetReverbOverrideSmall(const u8 smallReverb) { m_ReverbOverrideSmall = smallReverb; }
	void SetReverbOverrideMedium(const u8 mediumReverb) { m_ReverbOverrideMedium = mediumReverb; }
	void SetReverbOverrideLarge(const u8 largeReverb) { m_ReverbOverrideLarge = largeReverb; }

	void SetForceFullyOccluded(const bool forceFullyOccluded) { m_ForceFullOcclusion = forceFullyOccluded; }

	void SetForceMaxPathDepth(const bool forceMaxPathDepth) { m_ForceMaxPathDepth = forceMaxPathDepth; }
	bool GetForceMaxPathDepth() const { return m_ForceMaxPathDepth; }

	void SetIsVehicle(const bool isVehicle) { m_IsVehicle = isVehicle; }

	void SetShouldForceUpdate(const bool forceUpdate) { m_ForceUpdate = forceUpdate; }
	bool GetShouldForceUpdate() const { return m_ForceUpdate; }

	static void InitClass();
	static void ShutdownClass();

	static naEnvironmentGroup* Allocate(const char* debugIdentifier);

	virtual void UpdateMixGroup();


#if __BANK
	static void DebugPrintEnvironmentGroupPool();
	static void DebugDrawEnvironmentGroupPool();
	static void AddWidgets(bkBank &bank);
	void DrawDebug();
	void SetAsDebugEnvironmentGroup(bool debugMeBaby){m_DebugMeBaby=debugMeBaby;};
	bool GetIsDebugEnvironmentGroup() const { return m_DebugMeBaby; }
	s32 GetDebugTextPosY() const { return const_cast<naEnvironmentGroup*>(this)->m_DebugTextPosY += 10; }
#endif


private:

	// For optimization reasons we don't use the audSmoother class, but just do it internally
	f32 GetSmoothedValue(u32 timeInMs, f32 input, f32 valueLastFrame, f32 smootherRate, bool hasHadUpdate);

	void CancelAnyPendingSourceEnvironmentRequest();
	void CancelAnyPendingWorldProbes();

	Vector3 m_Position;
	Vector3 m_PositionLastUpdate;
	Vector3 m_SourceEnvironmentPositionLastUpdate;

	f32 m_OcclusionValue;
	f32 m_ExtraOcclusion;
	
	f32 m_ReverbSmallUnsmoothed;
	f32 m_ReverbMediumUnsmoothed;
	f32 m_ReverbLargeUnsmoothed;
	f32 m_ReverbSmall;
	f32 m_ReverbMedium;
	f32 m_ReverbLarge;

	f32 m_InSameRoomAsListenerRatio;
	f32 m_ForcedSameRoomAsListenerRatio;

	f32 m_CheapDistanceSqrd;
	u32 m_LastUpdateTime;
	u32 m_MaxUpdateTime;

	f32 m_FadeTime;
	mixgroupState m_MixgroupState;

	naEnvironmentGroup* m_LinkedEnvironmentGroup;

	u32 m_SourceEnvironmentTimeLastUpdated;
	f32 m_SourceEnvironmentMinimumDistanceUpdateSquared;
	TPathHandle m_SourceEnvironmentPathHandle;
	
	fwInteriorLocation m_InteriorLocation;
	const InteriorSettings* m_InteriorSettings;
	const InteriorRoom* m_InteriorRoom;

	f32 m_MaxLeakage;
	u16 m_MinLeakageDistance;
	u16 m_MaxLeakageDistance;

	u16 m_SourceEnvironmentUpdateTime;
	u16 m_SourceEnvironmentCheapUpdateTime;

	u8 m_ReverbOverrideSmall;
	u8 m_ReverbOverrideMedium;
	u8 m_ReverbOverrideLarge;

	u8 m_MaxPathDepth;

	bool m_HasGotUnderCoverResult : 1;
	bool m_IsUnderCover : 1;
	bool m_NotOccluded : 1;
	bool m_ForceSourceEnvironmentUpdate : 1;
	bool m_FillsInteriorSpace : 1;
	bool m_IsCurrentlyFillingListenerSpace : 1;
	bool m_LeaksInTaggedCutscenes : 1;
	bool m_HasValidRoomInfo : 1;
	bool m_UseInteriorDistanceMetrics : 1;
	bool m_UsePortalOcclusion : 1;
	bool m_UseCloseProximityGroupsForUpdates : 1;
	bool m_HasUpdatedSourceEnvironment :1;	// Needed for the smoother so the 1st update doesn't smooth up from an initialized value
	bool m_HasUpdatedOcclusion :1;
	bool m_ForceFullOcclusion :1;
	bool m_ForceMaxPathDepth :1;
	bool m_IsVehicle :1;

	bool m_ForceUpdate :1;

#if __BANK
	s32 m_DebugTextPosY;
	bool m_DebugMeBaby;
	BANK_ONLY(static sysCriticalSectionToken sm_DebugPrintToken);
#endif

};

 // NAENVIRONMENTGROUP_H
#endif

