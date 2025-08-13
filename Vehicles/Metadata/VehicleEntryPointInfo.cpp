// File header
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"

#include "fwanimation/animmanager.h"
#include "fwanimation/clipsets.h"
#include "Vehicles/vehicle_channel.h"

#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "VehicleEntryInfo_parser.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "streaming/streaming.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// Class CVehicleEntryPointInfo
//////////////////////////////////////////////////////////////////////////

const CVehicleSeatInfo* CVehicleEntryPointInfo::GetSeat(s32 iIndex) const
{
	vehicleAssertf(iIndex > -1 && iIndex < m_AccessableSeats.GetCount(), "Invalid seat index");
	return m_AccessableSeats[iIndex];
}

eHierarchyId CVehicleEntryPointInfo::GetWindowId() const
{
	eHierarchyId nRet = VEH_INVALID_ID;
	switch(m_WindowId)
	{
	case FRONT_LEFT:
		nRet = VEH_WINDOW_LF;
		break;
	case FRONT_RIGHT:
		nRet = VEH_WINDOW_RF;
		break;
	case REAR_RIGHT:
		nRet = VEH_WINDOW_RR;
		break;
	case REAR_LEFT:
		nRet = VEH_WINDOW_LR;
		break;
	case FRONT_WINDSCREEN:
		nRet = VEH_WINDSCREEN;
		break;
	case REAR_WINDSCREEN:
		nRet = VEH_WINDSCREEN_R;
		break;
	case MID_LEFT:
		nRet = VEH_WINDOW_LM;
		break;
	case MID_RIGHT:
		nRet = VEH_WINDOW_RM;
		break;
	case INVALID:
		nRet = VEH_INVALID_ID;
		break;
	default:
		Assertf(0,"Unhandled window id");
		break;
	}

	return nRet;
}

const CVehicleEntryPointInfo::eSide CVehicleEntryPointInfo::GetBlockReactionSide(s32 iIndex) const
{
	vehicleAssertf(iIndex > -1 && iIndex < m_BlockJackReactionSides.GetCount(), "Invalid block reaction side index");
	return m_BlockJackReactionSides[iIndex];
}

//////////////////////////////////////////////////////////////////////////
// Class CVehicleEntryPointAnimInfo
//////////////////////////////////////////////////////////////////////////

u32 CVehicleEntryPointAnimInfo::GetEnterAnimHash() const 
{
	const u32 getInHash = ATSTRINGHASH("get_in", 0x01b922298);
	return getInHash;
}

////////////////////////////////////////////////////////////////////////////////

const CExtraVehiclePoint* CVehicleExtraPointsInfo::GetExtraVehiclePointOfType(CExtraVehiclePoint::ePointType pointType) const
{
	for (s32 i=0; i<m_ExtraVehiclePoints.GetCount(); ++i)
	{
		if (m_ExtraVehiclePoints[i].GetPointType() == pointType)
		{
			return &m_ExtraVehiclePoints[i];
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const char* CVehicleExtraPointsInfo::GetNameFromInfo(const CVehicleExtraPointsInfo* NOTFINAL_ONLY(pInfo))
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

const CVehicleExtraPointsInfo* CVehicleExtraPointsInfo::GetInfoFromName(const char* name)
{
	const CVehicleExtraPointsInfo* pInfo = CVehicleMetadataMgr::GetVehicleExtraPointsInfo(atStringHash(name));
	vehicleAssertf(pInfo, "VehicleExtraPointsInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

const char* CVehicleEntryPointInfo::GetNameFromInfo(const CVehicleEntryPointInfo* NOTFINAL_ONLY(pInfo))
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

const CVehicleEntryPointInfo* CVehicleEntryPointInfo::GetInfoFromName(const char* name)
{
	const CVehicleEntryPointInfo* pInfo = CVehicleMetadataMgr::GetVehicleEntryPointInfo(atStringHash(name));
	vehicleAssertf(pInfo, "VehicleEntryPointInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

bool CEntryAnimVariations::CheckConditions( const CScenarioCondition::sScenarioConditionData& conditionData, s32 * firstFailureCondition ) const
{
	return CConditionalHelpers::CheckConditionsHelper( m_Conditions, conditionData, firstFailureCondition );
}

////////////////////////////////////////////////////////////////////////////////

int CEntryAnimVariations::GetImmediateConditions(u32 clipSetHash, const CScenarioCondition** pArrayOut, int maxNumber/*, eAnimType animType*/) const
{
	// First, check the conditions that apply to the whole CConditionalAnims object.
	int numFound = CConditionalHelpers::GetImmediateConditionsHelper(m_Conditions, pArrayOut, maxNumber);

	// Next, look for the clip that we are playing, and add the conditions from there.
	if(numFound < maxNumber)
	{
		const ConditionalClipSetArray* clipSets = &m_EntryAnims;
		if(clipSets)
		{
			const int numClipSets = clipSets->GetCount();
			for(int j = 0; j < numClipSets; j++)
			{
				const CConditionalClipSet* pClipSet = (*clipSets)[j];
				if(pClipSet->GetClipSetHash() == clipSetHash)
				{
					numFound += pClipSet->GetImmediateConditions(pArrayOut + numFound, maxNumber - numFound);
					break;
				}
			}
		}
	}

	return numFound;
}

////////////////////////////////////////////////////////////////////////////////

bool CEntryAnimVariations::CheckClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData ) const
{
	for(int i=0; i<anims.size(); i++)
	{
		if(anims[i]->CheckConditions(conditionData))
		{
			// Check that the animation dictionary for this exists?
			strLocalIndex iDictIndex = anims[i]->GetAnimDictIndex();
			aiAssertf(iDictIndex.Get()>=0,"Failed to find animation dictionary from set '%s'\n",anims[i]->GetClipSetName());
			if (iDictIndex.Get()>=0 && CStreaming::IsObjectInImage(iDictIndex, fwAnimManager::GetStreamingModuleId()))
			{
				// As long as at least one set in this list is OK then we're fine
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

const CConditionalClipSet * CEntryAnimVariations::ChooseClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData ) const
{
#define		MAX_SETS 32
	u32			sets[MAX_SETS];

	s32			iNoValidSets = 0;

	if ( anims.size() == 0 )
	{
		return NULL;
	}

	for(int i=0; i<anims.size(); i++)
	{
		if(anims[i]->CheckConditions(conditionData) && iNoValidSets<MAX_SETS)
		{
			sets[iNoValidSets] = i;
			iNoValidSets++;
		}
	}

	if ( iNoValidSets == 0 )
	{
		return NULL;
	}

	// Pick a random group (note: these do not have individual probabilities at the moment, might be needed though?)
	u32 randomSet = sets[fwRandom::GetRandomNumberInRange(0, iNoValidSets)];
	return anims[randomSet];
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

bool CEntryAnimVariations::HasAnimationData() const
{
	if (m_EntryAnims.size())
	{
		return true;
	}

	return false;
}

#endif

////////////////////////////////////////////////////////////////////////////////

const char* CEntryAnimVariations::GetNameFromInfo(const CEntryAnimVariations* NOTFINAL_ONLY(pInfo))
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

const CEntryAnimVariations* CEntryAnimVariations::GetInfoFromName(const char* name)
{
	const CEntryAnimVariations* pInfo = CVehicleMetadataMgr::GetEntryAnimVariations(atStringHash(name));
	vehicleAssertf(pInfo, "EntryAnimVariations [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

const char* CVehicleEntryPointAnimInfo::GetNameFromInfo(const CVehicleEntryPointAnimInfo* NOTFINAL_ONLY(pInfo))
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

const CVehicleEntryPointAnimInfo* CVehicleEntryPointAnimInfo::GetInfoFromName(const char* name)
{
	const CVehicleEntryPointAnimInfo* pInfo = CVehicleMetadataMgr::GetVehicleEntryPointAnimInfo(atStringHash(name));
	vehicleAssertf(pInfo, "VehicleEntryPointAnimInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////
