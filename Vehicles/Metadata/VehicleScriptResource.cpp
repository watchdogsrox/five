// Framework headers
#include "entity/archetypemanager.h"
#include "fwAnimation/animmanager.h"
#include "fwAnimation/clipsets.h"

// File header
#include "ModelInfo/BaseModelInfo.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "script/script_channel.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleScriptResource.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////

CVehicleScriptResource::CVehicleScriptResource(atHashWithStringNotFinal VehicleModelHash, s32 iRequestFlags)
: m_VehicleModelHash(VehicleModelHash)
{
	RequestAnims(iRequestFlags);
}

//////////////////////////////////////////////////////////////////////////////

CVehicleScriptResource::~CVehicleScriptResource()
{
	m_assetRequesterVehicle.ClearRequiredFlags(STRFLAG_MISSION_REQUIRED);
}

//////////////////////////////////////////////////////////////////////////////

void CVehicleScriptResource::RequestAnimsFromClipSet(const fwMvClipSetId &clipSetId)
{
	// Go through all the anim dictionaries in the clip set and request them to be streamed in
	if(clipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		while (pClipSet)
		{
			if (m_assetRequesterVehicle.GetRequestCount() >= m_assetRequesterVehicle.GetRequestMaxCount())
			{
				scriptDisplayf("Frame : %i, Vehicle asset requester full for vehicle %s, max num assets %i, requested clipset %s", fwTimer::GetFrameCount(), m_VehicleModelHash.TryGetCStr() ? m_VehicleModelHash.TryGetCStr() : "NULL", m_assetRequesterVehicle.GetRequestMaxCount(), clipSetId.TryGetCStr());
				return;
			}

			scriptDisplayf("Frame : %i, Requesting clipset %s", fwTimer::GetFrameCount(), clipSetId.TryGetCStr());

			s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(pClipSet->GetClipDictionaryName().GetHash()).Get();
			if(Verifyf(animDictIndex>-1, "Can't find clip dictionary [%s]", pClipSet->GetClipDictionaryName().GetCStr()))
			{
				if (m_assetRequesterVehicle.RequestIndex(animDictIndex, fwAnimManager::GetStreamingModuleId()) == -1)	// make sure we are not duplicating anim requests
					m_assetRequesterVehicle.PushRequest(animDictIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_MISSION_REQUIRED);		
			}

			// Load the fallbacks aswell if they exist
			pClipSet = fwClipSetManager::GetClipSet(pClipSet->GetFallbackId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

void CVehicleScriptResource::RequestAnims(s32 iRequestFlags)
{
	fwModelId VehicleModelId;
	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) m_VehicleModelHash, &VehicleModelId);
	if (pBaseModelInfo && pBaseModelInfo->GetModelType() == MI_TYPE_VEHICLE)
	{
		const CVehicleLayoutInfo* pLayoutInfo = static_cast<CVehicleModelInfo*>(pBaseModelInfo)->GetVehicleLayoutInfo();
		if (pLayoutInfo)
		{
			for(int iSeatIndex = 0; iSeatIndex < pLayoutInfo->GetNumSeats(); iSeatIndex++)
			{
				const CVehicleSeatAnimInfo* pSeatAnimInfo = pLayoutInfo->GetSeatAnimationInfo(iSeatIndex);

				while(pSeatAnimInfo)
				{
					if(iRequestFlags & VRF_RequestSeatAnims)
					{
						fwMvClipSetId seatClipSetId = pSeatAnimInfo->GetSeatClipSetId();

						// Go through all the anim dictionaries in the clip set and request them to be streamed in
						if(seatClipSetId != CLIP_SET_ID_INVALID)
						{
							fwClipSet* pClipSet = fwClipSetManager::GetClipSet(seatClipSetId);
							if(pClipSet)
							{
								vehicleAssert(pClipSet->GetClipDictionaryIndex().Get()>-1);

								// Only add streamed anim dictionaries to the list
								const fwClipDictionaryMetadata *pClipDictionaryMetadata = fwClipSetManager::GetClipDictionaryMetadata(pClipSet->GetClipDictionaryName());
								vehicleAssertf(pClipDictionaryMetadata, "Missing dictionary entry (%s), check anim metadata", pClipSet->GetClipDictionaryName().GetCStr());
								if (pClipDictionaryMetadata && (pClipDictionaryMetadata->GetStreamingPolicy() & SP_STREAMING) )
								{
									RequestAnimsFromClipSet(seatClipSetId);
								}
							}
						}
					}

					// If desired, get our exit to aim animations
					if(iRequestFlags & VRF_RequestExitToAimAnims)
					{
						atHashWithStringNotFinal seatAimInfoName = pSeatAnimInfo->GetExitToAimInfoName();

						int iNumVehicleAimInfos = CTaskExitVehicleSeat::ms_Tunables.m_ExitToAimVehicleInfos.GetCount();
						for(int i = 0; i < iNumVehicleAimInfos; i++)
						{
							// Check if the desired aim info name for this seat matches this aim vehicle info
							if(seatAimInfoName == CTaskExitVehicleSeat::ms_Tunables.m_ExitToAimVehicleInfos[i].m_Name)
							{
								int iNumVehicleSeatAimInfos = CTaskExitVehicleSeat::ms_Tunables.m_ExitToAimVehicleInfos[i].m_Seats.GetCount();
								for(int j = 0; j < iNumVehicleSeatAimInfos; j++)
								{
									// Get the info for this seat's exit to aim
									CTaskExitVehicleSeat::ExitToAimSeatInfo* pExitToAimSeatInfo = &CTaskExitVehicleSeat::ms_Tunables.m_ExitToAimVehicleInfos[i].m_Seats[j];

									// Verify the one handed clip set is not null then request it
									if(pExitToAimSeatInfo->m_OneHandedClipSetName.IsNotNull())
									{
										RequestAnimsFromClipSet(fwMvClipSetId(pExitToAimSeatInfo->m_OneHandedClipSetName));
									}

									// Verify the two handed clip set is not null then request it
									if(pExitToAimSeatInfo->m_TwoHandedClipSetName.IsNotNull())
									{
										RequestAnimsFromClipSet(fwMvClipSetId(pExitToAimSeatInfo->m_TwoHandedClipSetName));
									}
								}

								break;
							}
						}
					}

					pSeatAnimInfo = NULL;
				}
			}

			if(iRequestFlags & VRF_RequestEntryAnims)
			{
				// Check entry point anims
				for(int iEntryPointIndex = 0; iEntryPointIndex < pLayoutInfo->GetNumEntryPoints(); iEntryPointIndex++)
				{
					const CVehicleEntryPointAnimInfo* pAnimInfo = pLayoutInfo->GetEntryPointAnimInfo(iEntryPointIndex);

					while(pAnimInfo)
					{
						fwMvClipSetId commonClipSetId = pAnimInfo->GetCommonClipSetId();
						if(commonClipSetId != CLIP_SET_ID_INVALID)
						{
							fwClipSet* pCommonClipSet = fwClipSetManager::GetClipSet(commonClipSetId);
							if(pCommonClipSet)
							{
								const fwClipDictionaryMetadata* pCommonDictMetadata = fwClipSetManager::GetClipDictionaryMetadata(pCommonClipSet->GetClipDictionaryName());
								if (pCommonDictMetadata && (pCommonDictMetadata->GetStreamingPolicy() & SP_STREAMING) )
								{
									RequestAnimsFromClipSet(commonClipSetId);
								}
							}
						}

						fwMvClipSetId entryClipSetId = pAnimInfo->GetEntryPointClipSetId();
						if ((iRequestFlags & VRF_RequestAlternateEntryAnims) && (pAnimInfo->GetAlternateEntryPointClipSet() != CLIP_SET_ID_INVALID))
						{
							entryClipSetId = pAnimInfo->GetAlternateEntryPointClipSet();
						}

						// Go through all the anim dictionaries in the clip set and request them to be streamed in
						if(entryClipSetId != CLIP_SET_ID_INVALID)
						{
							fwClipSet* pClipSet = fwClipSetManager::GetClipSet(entryClipSetId);
							if(pClipSet)
							{
								vehicleAssert(pClipSet->GetClipDictionaryIndex().Get()>-1);

								// Only add streamed anim dictionaries to the list
								const fwClipDictionaryMetadata *pDictMetadata = fwClipSetManager::GetClipDictionaryMetadata(pClipSet->GetClipDictionaryName());
								vehicleAssertf(pDictMetadata, "Missing dictionary entry (%s), check anim metadata", pClipSet->GetClipDictionaryName().GetCStr());
								if (pDictMetadata && (pDictMetadata->GetStreamingPolicy() & SP_STREAMING) )
								{
									RequestAnimsFromClipSet(entryClipSetId);
								}
							}
						}
						pAnimInfo = NULL;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

CVehicleScriptResourceManager::VehicleResources CVehicleScriptResourceManager::ms_VehicleResources;

//////////////////////////////////////////////////////////////////////////////

void CVehicleScriptResourceManager::Init()
{
}

//////////////////////////////////////////////////////////////////////////////

void CVehicleScriptResourceManager::Shutdown()
{
	for(s32 i = 0; i < ms_VehicleResources.GetCount(); i++)
	{
		delete ms_VehicleResources[i];
	}
	ms_VehicleResources.Reset();
}

//////////////////////////////////////////////////////////////////////////////

s32 CVehicleScriptResourceManager::RegisterResource(atHashWithStringNotFinal VehicleModelHash, s32 iRequestFlags)
{
	s32 iIndex = GetIndex(VehicleModelHash);
	if(iIndex == -1)
	{
		iIndex = ms_VehicleResources.GetCount();
		ms_VehicleResources.PushAndGrow(rage_new CVehicleScriptResource(VehicleModelHash, iRequestFlags));
	}
	return ms_VehicleResources[iIndex]->GetHash();
}

//////////////////////////////////////////////////////////////////////////////

void CVehicleScriptResourceManager::UnregisterResource(atHashWithStringNotFinal VehicleModelHash)
{
	s32 iIndex = GetIndex(VehicleModelHash);
	if(iIndex != -1)
	{
		delete ms_VehicleResources[iIndex];
		ms_VehicleResources.DeleteFast(iIndex);
	}
}

//////////////////////////////////////////////////////////////////////////////

CVehicleScriptResource* CVehicleScriptResourceManager::GetResource(atHashWithStringNotFinal VehicleModelHash)
{
	s32 iIndex = GetIndex(VehicleModelHash);
	if(iIndex != -1)
	{
		return ms_VehicleResources[iIndex];
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////

s32 CVehicleScriptResourceManager::GetIndex(atHashWithStringNotFinal VehicleModelHash)
{
	for(s32 i = 0; i < ms_VehicleResources.GetCount(); i++)
	{
		if(VehicleModelHash.GetHash() == ms_VehicleResources[i]->GetHash())
		{
			return i;
		}
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////////
