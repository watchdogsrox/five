// Main headers
#include "Vehicles/Metadata/VehicleLayoutInfo.h"

// Rage headers
#include "fwanimation/animmanager.h"
#include "fwanimation/clipsets.h"
#include "parser/manager.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Peds/PlayerInfo.h"
#include "Network/NetworkInterface.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/vehicle_channel.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo_parser.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Task/Combat/Cover/TaskCover.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

PARAM(mpvehicleanimdependencies, "[vehicleai] - Makes vehicle anims streamed in along with the vehicle in MP games");

////////////////////////////////////////////////////////////////////////////////

const CAnimRateSet* CAnimRateSet::GetInfoFromName(const char* name)
{
	const CAnimRateSet* pInfo = CVehicleMetadataMgr::GetAnimRateSet(atStringHash(name));
	vehicleAssertf(pInfo, "CBicycleInfo [%s] doesn't exist in data", name);
	return pInfo;
}


////////////////////////////////////////////////////////////////////////////////

const char* CAnimRateSet::GetNameFromInfo(const CAnimRateSet* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

float CAnimRateSet::sAnimRateTuningPair::GetRate() const
{
	const bool bUseMPAnimRates = CAnimSpeedUps::ShouldUseMPAnimRates();
	float fRate = bUseMPAnimRates ? m_MPRate : m_SPRate;
	if (bUseMPAnimRates)
	{
		// Hard coded for B*1604888 because we don't support tunable meta patching
		TUNE_GROUP_FLOAT(PLAYER_TUNE, MP_VEHICLE_ENTRY_EXIT_RATE, 0.9f, 0.0f, 2.0f, 0.01f);
		fRate *= MP_VEHICLE_ENTRY_EXIT_RATE;//CAnimSpeedUps::ms_Tunables.m_MultiplayerEnterExitJackVehicleRateModifier;
	}
	return fRate;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleCoverBoundInfo::IsBoundActive(const CVehicleCoverBoundInfo& rActiveBound, const CVehicleCoverBoundInfo& rOtherBound)
{
	if (rActiveBound.m_Name == rOtherBound.m_Name)
	{
		return true;
	}

	const s32 iNumBoundExclusions = rActiveBound.m_ActiveBoundExclusions.GetCount();
	if (iNumBoundExclusions > 0)
	{
		for (s32 i=0; i<iNumBoundExclusions; ++i)
		{
			if (rActiveBound.m_ActiveBoundExclusions[i].GetHash() == rOtherBound.m_Name)
			{
				return false;
			}
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CVehicleCoverBoundInfo::RenderCoverBound(const CVehicle& rVeh, const atArray<CVehicleCoverBoundInfo>& rCoverBoundArray, s32 iIndex, bool bTreatAsActive, bool bRenderInGreen)
{
	const CVehicleCoverBoundInfo& rCoverBoundInfo = rCoverBoundArray[iIndex];
	Mat34V mtx = rVeh.GetTransform().GetMatrix();
	Vec3V vNewPosition = Transform(mtx, VECTOR3_TO_VEC3V(rCoverBoundInfo.m_Position));
	mtx.SetCol3(vNewPosition);
	const Vec3V vMin = - Vec3V(rCoverBoundInfo.m_Width * 0.5f, rCoverBoundInfo.m_Length * 0.5f, rCoverBoundInfo.m_Height * 0.5f);
	const Vec3V vMax = Vec3V(rCoverBoundInfo.m_Width * 0.5f, rCoverBoundInfo.m_Length * 0.5f, rCoverBoundInfo.m_Height * 0.5f);
	TUNE_GROUP_BOOL(COVER_BOUND_TUNE, RENDER_SOLID, false);
	Color32 color = bRenderInGreen ? Color_green : (bTreatAsActive ? Color_orange  : Color_red);
	CCoverDebug::GetDebugContext(CCoverDebug::DEFAULT).AddOBB(vMin, vMax, mtx, color);
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

const CVehicleCoverBoundOffsetInfo* CVehicleCoverBoundOffsetInfo::GetInfoFromName(const char* name)
{
	const CVehicleCoverBoundOffsetInfo* pInfo = CVehicleMetadataMgr::GetVehicleCoverBoundOffsetInfo(atStringHash(name));
	vehicleAssertf(pInfo, "CVehicleCoverBoundOffsetInfo [%s] doesn't exist in data", name);
	return pInfo;
}


////////////////////////////////////////////////////////////////////////////////

const char* CVehicleCoverBoundOffsetInfo::GetNameFromInfo(const CVehicleCoverBoundOffsetInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CBicycleInfo* CBicycleInfo::GetInfoFromName(const char* name)
{
	const CBicycleInfo* pInfo = CVehicleMetadataMgr::GetBicycleInfo(atStringHash(name));
	vehicleAssertf(pInfo, "CBicycleInfo [%s] doesn't exist in data", name);
	return pInfo;
}


////////////////////////////////////////////////////////////////////////////////

const char* CBicycleInfo::GetNameFromInfo(const CBicycleInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CPOVTuningInfo* CPOVTuningInfo::GetInfoFromName(const char* name)
{
	const CPOVTuningInfo* pInfo = CVehicleMetadataMgr::GetPOVTuningInfo(atStringHash(name));
	vehicleAssertf(pInfo, "CBicycleInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

float CPOVTuningInfo::SqueezeWithinLimits(float fVal, float fMin, float fMax)
{
	if (fVal < fMin)
	{
		return 0.0f;
	}
	else if (fVal > fMax)
	{
		return 1.0f;
	}
	else
	{
		return Clamp((fMax - fMin) / (fMax - fMin), 0.0f, 1.0f);
	}
}

////////////////////////////////////////////////////////////////////////////////

float CPOVTuningInfo::ComputeRestrictedDesiredLeanAngle(float fDesiredLeanAngle) const
{
	const float fBodyXLeanRatio = Abs(fDesiredLeanAngle - 0.5f) * 2.0f;
	const float fBodyLeanXLimitClampDelta = (1.0f - fBodyXLeanRatio) * m_BodyLeanXDeltaFromCenterMinBikeLean + fBodyXLeanRatio * m_BodyLeanXDeltaFromCenterMaxBikeLean;
	fDesiredLeanAngle = Clamp(fDesiredLeanAngle, fBodyLeanXLimitClampDelta, 1.0f - fBodyLeanXLimitClampDelta);
	return fDesiredLeanAngle;
}

////////////////////////////////////////////////////////////////////////////////

void CPOVTuningInfo::ComputeMinMaxPitchAngle(float fSpeedRatio, float& fMinPitch, float& fMaxPitch) const
{
	fMinPitch = m_MinForwardsPitchSlow * (1.0f - fSpeedRatio) + m_MinForwardsPitchFast * fSpeedRatio;
	fMaxPitch = m_MaxForwardsPitchSlow* (1.0f - fSpeedRatio) + m_MaxForwardsPitchFast * fSpeedRatio;
}

////////////////////////////////////////////////////////////////////////////////

float CPOVTuningInfo::ComputeStickCenteredBodyLeanY(float fLeanAngle, float fSpeed) const
{
	TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, BIKE_CENTRED_SPEED_THRESHOLD_MIN, 0.0f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, BIKE_CENTRED_SPEED_THRESHOLD_MAX, 1.0f, -1.0f, 2.0f, 0.01f);
	const float fStickLerpRatioFromBikeAngle = Abs(fLeanAngle - 0.5f) * 2.0f;
	const float fBikeCentredStickYSlow = (1.0f - fStickLerpRatioFromBikeAngle) * m_StickCenteredBodyLeanYSlowMin + fStickLerpRatioFromBikeAngle * m_StickCenteredBodyLeanYSlowMax;
	const float fBikeCentredStickYFast = (1.0f - fStickLerpRatioFromBikeAngle) * m_StickCenteredBodyLeanYFastMin + fStickLerpRatioFromBikeAngle * m_StickCenteredBodyLeanYFastMax;
	const float fSpeedLerpRatio = Clamp((fSpeed - BIKE_CENTRED_SPEED_THRESHOLD_MIN) / (BIKE_CENTRED_SPEED_THRESHOLD_MAX - BIKE_CENTRED_SPEED_THRESHOLD_MIN), 0.0f, 1.0f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, FORCED_MIN_RATIO_LERP_LIMIT, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_VEHICLE_TUNE, FORCED_MAX_RATIO_LERP_LIMIT, 0.45f, 0.0f, 1.0f, 0.01f);
	const float fRemappedSpeedLerpRatio = SqueezeWithinLimits(fSpeedLerpRatio, FORCED_MIN_RATIO_LERP_LIMIT, FORCED_MAX_RATIO_LERP_LIMIT);
	const float fCentredStickY = (1.0f - fRemappedSpeedLerpRatio) * fBikeCentredStickYSlow + fRemappedSpeedLerpRatio * fBikeCentredStickYFast;
	return fCentredStickY;
}

////////////////////////////////////////////////////////////////////////////////

const char* CPOVTuningInfo::GetNameFromInfo(const CPOVTuningInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleSeatInfo* CVehicleLayoutInfo::GetSeatInfo(int iIndex) const
{
	if(vehicleVerifyf(iIndex > -1 && iIndex < GetNumSeats(),"Invalid seat index"))
	{	
		return m_Seats[iIndex].m_SeatInfo;
	}
	else
	{
		return NULL; 
	}
}

const CVehicleSeatAnimInfo* CVehicleLayoutInfo::GetSeatAnimationInfo(int iIndex) const
{
	if(vehicleVerifyf(iIndex > -1 && iIndex < GetNumSeats(),"Invalid seat index"))
	{
		return m_Seats[iIndex].m_SeatAnimInfo;
	}
	else
	{
		return NULL; 
	}

}

const CVehicleEntryPointInfo* CVehicleLayoutInfo::GetEntryPointInfo(int iIndex) const
{
	if(vehicleVerifyf(iIndex > -1 && iIndex < GetNumEntryPoints(),"Invalid entry point index"))
	{
		return m_EntryPoints[iIndex].m_EntryPointInfo;
	}
	else
	{
		return NULL; 
	}
}

const CVehicleEntryPointAnimInfo* CVehicleLayoutInfo::GetEntryPointAnimInfo(int iIndex) const
{
	if(vehicleVerifyf(iIndex > -1 && iIndex < GetNumEntryPoints(),"Invalid seat index"))
	{
		return m_EntryPoints[iIndex].m_EntryPointAnimInfo;
	}
	else
	{
		return NULL; 
	}
}

bool CVehicleLayoutInfo::ValidateEntryPoints()					
{ 
	for(UInt32 i = 0; i<m_EntryPoints.GetCount(); i++) 
	{ 
		if (m_EntryPoints[i].m_EntryPointInfo == NULL) 
		{ 
			return(false);
		}
	}

	return(true);
}

const CAnimRateSet& CVehicleLayoutInfo::GetAnimRateSet() const 
{ 
	if (m_AnimRateSet)
	{
		return *m_AnimRateSet;
	}
	return *CVehicleMetadataMgr::GetInstance().GetAnimRateSet(0); 
}

int CVehicleLayoutInfo::GetAllStreamedAnimDictionaries(s32* pAnimDictIndicesList, int iAnimDictIndicesListSize) const
{
    bool bStreamAllAnimsForMP = PARAM_mpvehicleanimdependencies.Get() && NetworkInterface::IsNetworkOpen();

	if (!bStreamAllAnimsForMP && CVehicleClipRequestHelper::ms_Tunables.m_DisableVehicleDependencies && GetStreamAnims())
	{
		return 0;
	}

	int iCount = 0;
	ASSERT_ONLY(int iNumMissingGroups = 0;)

	for(int iSeatIndex = 0; iSeatIndex < GetNumSeats(); iSeatIndex++)
	{
		const CVehicleSeatAnimInfo* pSeatAnimInfo = GetSeatAnimationInfo(iSeatIndex);

		while(pSeatAnimInfo)
		{
			fwMvClipSetId seatClipSetId = pSeatAnimInfo->GetSeatClipSetId();

			// Go through all the anim dictionaries in the clip set and request them to be streamed in
			if(seatClipSetId != CLIP_SET_ID_INVALID)
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(seatClipSetId);
				while (pClipSet)
				{
					s32 iAnimDictIndex = pClipSet->GetClipDictionaryIndex().Get();
					vehicleAssert(iAnimDictIndex>-1);

					// Only add streamed anim dictionaries to the list
					const fwClipDictionaryMetadata *pClipDictionaryMetadata = fwClipSetManager::GetClipDictionaryMetadata(pClipSet->GetClipDictionaryName());
					vehicleAssertf(pClipDictionaryMetadata, "Missing dictionary entry (%s), check anim metadata", pClipSet->GetClipDictionaryName().GetCStr());
					if (pClipDictionaryMetadata && (pClipDictionaryMetadata->GetStreamingPolicy() == SP_STREAMING) )
					{
						// See if this already exists in the list
						bool bFound = false;

						for(int iAnimSearch = 0; iAnimSearch < iCount && !bFound; iAnimSearch++)
						{
							bFound = (iAnimDictIndex == pAnimDictIndicesList[iAnimSearch]);
						}

						if(!bFound)
						{
							if(vehicleVerifyf(iCount < iAnimDictIndicesListSize, "Out of room to fill out anim dict indices list - Check clipset count on layout %s in vehiclelayouts.meta", GetName().GetCStr()))
							{
								pAnimDictIndicesList[iCount++] = iAnimDictIndex;
							}
							else
							{
								ASSERT_ONLY(++iNumMissingGroups);
							}
						}	
					}

					// Load the fallbacks aswell if they exist
					pClipSet = pClipSet->GetFallbackSet();
				}
			}

			pSeatAnimInfo = NULL;
		}
	}

	// Check entry point anims
	for(int iEntryPointIndex = 0; iEntryPointIndex < GetNumEntryPoints(); iEntryPointIndex++)
	{
		const CVehicleEntryPointAnimInfo* pAnimInfo = GetEntryPointAnimInfo(iEntryPointIndex);

		while(pAnimInfo)
		{
			fwMvClipSetId entryClipSetId = pAnimInfo->GetEntryPointClipSetId();

			// Go through all the anim dictionaries in the clip set and request them to be streamed in
			if(entryClipSetId != CLIP_SET_ID_INVALID)
			{
				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(entryClipSetId);
				while (pClipSet)
				{
					s32 iAnimDictIndex = pClipSet->GetClipDictionaryIndex().Get();
					vehicleAssert(iAnimDictIndex>-1);

					// Only add streamed anim dictionaries to the list
					const fwClipDictionaryMetadata *pDictMetadata = fwClipSetManager::GetClipDictionaryMetadata(pClipSet->GetClipDictionaryName());
					vehicleAssertf(pDictMetadata, "Missing dictionary entry (%s), check anim metadata", pClipSet->GetClipDictionaryName().GetCStr());
					if (pDictMetadata && (pDictMetadata->GetStreamingPolicy() == SP_STREAMING) )
					{
						// See if this already exists in the list
						bool bFound = false;

						for(int iAnimSearch = 0; iAnimSearch < iCount && !bFound; iAnimSearch++)
						{
							bFound = (iAnimDictIndex == pAnimDictIndicesList[iAnimSearch]);
						}

						if(!bFound)
						{
							if(vehicleVerifyf(iCount < iAnimDictIndicesListSize, "Out of room to fill out anim dict indices list - Check clipset count on layout %s in vehiclelayouts.meta", GetName().GetCStr()))
							{
								pAnimDictIndicesList[iCount++] = iAnimDictIndex;
							}
							else
							{
								ASSERT_ONLY(++iNumMissingGroups);
							}
						}	
					}

					// Load the fallbacks aswell if they exist
					pClipSet = pClipSet->GetFallbackSet();
				}
			}
			pAnimInfo = NULL;
		}
	}

#if __ASSERT
	if (iNumMissingGroups > 0)
	{
		Assertf(0, "Missing %i anim dictionaries because MAX_VEHICLE_STR_DEPENDENCIES is set to %i, needs to be %i to load all dictionaries", iNumMissingGroups, iAnimDictIndicesListSize, iNumMissingGroups+iAnimDictIndicesListSize);
	}
#endif // __ASSERT

	return iCount;
}

////////////////////////////////////////////////////////////////////////////////

const CClipSetMap* CClipSetMap::GetInfoFromName(const char* name)
{
	const CClipSetMap* pInfo = CVehicleMetadataMgr::GetClipSetMap(atStringHash(name));
	vehicleAssertf(pInfo, "CClipSetMap [%s] doesn't exist in data", name);
	return pInfo;
}


////////////////////////////////////////////////////////////////////////////////

const char* CClipSetMap::GetNameFromInfo(const CClipSetMap* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName().GetCStr();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////