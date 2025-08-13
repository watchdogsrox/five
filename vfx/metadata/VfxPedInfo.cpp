///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxPedInfo.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	22 August 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxPedInfo.h"
#include "VfxPedInfo_parser.h"

// rage
#include "parser/manager.h"
#include "parser/restparserservices.h"

// framework
#include "fwscene/stores/psostore.h"
#include "fwsys/fileexts.h"
#include "vfx/channel.h"

// game
#include "ModelInfo/PedModelInfo.h"
#include "Scene/DataFileMgr.h"
#include "Peds/Ped.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Systems/VfxPed.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxPedInfoMgr g_vfxPedInfoMgr;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  STRUCT VfxPedFootInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GetDecalInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedFootDecalInfo* VfxPedFootInfo::GetDecalInfo() const
{
	return g_vfxPedInfoMgr.GetPedFootDecalInfo(m_decalInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  GetPtFxInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedFootPtFxInfo* VfxPedFootInfo::GetPtFxInfo() const
{
	return g_vfxPedInfoMgr.GetPedFootPtFxInfo(m_ptFxInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  STRUCT VfxPedWadeInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GetDecalInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedWadeDecalInfo* VfxPedWadeInfo::GetDecalInfo() const
{
	return g_vfxPedInfoMgr.GetPedWadeDecalInfo(m_decalInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  GetPtFxInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedWadePtFxInfo* VfxPedWadeInfo::GetPtFxInfo() const
{
	return g_vfxPedInfoMgr.GetPedWadePtFxInfo(m_ptFxInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  STRUCT VfxPedVfxGroupInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GetFootInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedFootInfo* VfxPedVfxGroupInfo::GetFootInfo() const
{
	return g_vfxPedInfoMgr.GetPedFootInfo(m_footInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  GetWadeInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedWadeInfo* VfxPedVfxGroupInfo::GetWadeInfo() const
{
	return g_vfxPedInfoMgr.GetPedWadeInfo(m_wadeInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  STRUCT VfxPedSkeletonBoneInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GetBoneWadeInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedBoneWadeInfo* VfxPedSkeletonBoneInfo::GetBoneWadeInfo() const
{
	return g_vfxPedInfoMgr.GetPedBoneWadeInfo(m_boneWadeInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  GetBoneWaterInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedBoneWaterInfo* VfxPedSkeletonBoneInfo::GetBoneWaterInfo() const
{
	return g_vfxPedInfoMgr.GetPedBoneWaterInfo(m_boneWaterInfoName);
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxPedInfo
///////////////////////////////////////////////////////////////////////////////

CVfxPedInfo::CVfxPedInfo() 
{

}


///////////////////////////////////////////////////////////////////////////////
//  GetVfxGroupInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedVfxGroupInfo* CVfxPedInfo::GetVfxGroupInfo(VfxGroup_e vfxGroup) const
{
	for (int i=0; i<m_vfxGroupInfos.GetCount(); i++)
	{
		if (vfxGroup==m_vfxGroupInfos[i].m_vfxGroup)
		{
			return &m_vfxGroupInfos[i];
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetFootVfxInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedFootInfo* CVfxPedInfo::GetFootVfxInfo(VfxGroup_e vfxGroup) const
{
	const VfxPedVfxGroupInfo* pPedVfxGroupInfo = GetVfxGroupInfo(vfxGroup);
	if (pPedVfxGroupInfo)
	{
		return pPedVfxGroupInfo->GetFootInfo();
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetWadeVfxInfo
///////////////////////////////////////////////////////////////////////////////

const VfxPedWadeInfo* CVfxPedInfo::GetWadeVfxInfo(VfxGroup_e vfxGroup) const
{
	const VfxPedVfxGroupInfo* pPedVfxGroupInfo = GetVfxGroupInfo(vfxGroup);
	if (pPedVfxGroupInfo)
	{
		return pPedVfxGroupInfo->GetWadeInfo();
	}

	return NULL;
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxPedInfoMgr
///////////////////////////////////////////////////////////////////////////////

CVfxPedInfoMgr::CVfxPedInfoMgr() 
{
#if __DEV
	m_vfxPedLiveEditUpdateData.Reset();
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void CVfxPedInfoMgr::LoadData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VFXPEDINFO_FILE);
	vfxAssertf(DATAFILEMGR.IsValid(pData), "vfx ped info file is not available");
	if (DATAFILEMGR.IsValid(pData))
	{
		fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, *this);
		//PARSER.SaveObject(pData->m_filename, "meta", this);
		parRestRegisterSingleton("Vfx/PedInfo", *this, pData->m_filename);
	}

	SetupWaterDripInfos();

#if __ASSERT
	for (atBinaryMap<CVfxPedInfo*, atHashString>::Iterator pedInfoIterator=m_vfxPedInfos.Begin(); pedInfoIterator!=m_vfxPedInfos.End(); ++pedInfoIterator)
	{
		CVfxPedInfo*& pVfxPedInfo = *pedInfoIterator;

		// make sure all vfx groups are defined and in the correct order (or none at all)
		const atHashString& keyHashString = pedInfoIterator.GetKey();
		int numVfxGroupInfos = pVfxPedInfo->GetVfxGroupNumInfos();
		vfxAssertf(numVfxGroupInfos==0 || numVfxGroupInfos==NUM_VFX_GROUPS, "vfx ped info %s doesn't define all vfx groups", keyHashString.GetCStr());
		for (int i=0; i<pVfxPedInfo->GetVfxGroupNumInfos(); i++)
		{
			const VfxPedVfxGroupInfo* pVfxPedVfxGroupInfo = pVfxPedInfo->GetVfxGroupInfo(i);
			vfxAssertf(pVfxPedVfxGroupInfo->m_vfxGroup==i, "vfx ped info %s doesn't define the vfx groups in the correct order (%d)", keyHashString.GetCStr(), i);

			// make sure that all the refernced foot and wade infos exist
			const VfxPedFootInfo* pVfxPedFootInfo = pVfxPedVfxGroupInfo->GetFootInfo();
			vfxAssertf(pVfxPedFootInfo, "vfx ped info %s defines a foot info that doesn't exist %s", keyHashString.GetCStr(), pVfxPedVfxGroupInfo->m_footInfoName.GetCStr());
			const VfxPedWadeInfo* pVfxPedWadeInfo = pVfxPedVfxGroupInfo->GetWadeInfo();
			vfxAssertf(pVfxPedWadeInfo, "vfx ped info %s defines a wade info that doesn't exist %s", keyHashString.GetCStr(), pVfxPedVfxGroupInfo->m_wadeInfoName.GetCStr());
		}

		// make sure the ped skeleton bone data doesn't exceed the max
		int numSkeletonBoneInfos = pVfxPedInfo->GetPedSkeletonBoneNumInfos();
 		vfxAssertf(numSkeletonBoneInfos<=VFXPED_MAX_VFX_SKELETON_BONES, "vfx ped info %s has more skeleton bones than the max", keyHashString.GetCStr());
		for (int i=0; i<numSkeletonBoneInfos; i++)
		{
			const VfxPedSkeletonBoneInfo* pVfxPedSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
			const VfxPedBoneWadeInfo* pVfxPedBoneWadeInfo = pVfxPedSkeletonBoneInfo->GetBoneWadeInfo();
			vfxAssertf(pVfxPedBoneWadeInfo, "vfx skeleton bone %d defines a bone wade info that doesn't exist %s", i, pVfxPedSkeletonBoneInfo->m_boneWadeInfoName.GetCStr());
			const VfxPedBoneWaterInfo* pVfxPedBoneWaterInfo = pVfxPedSkeletonBoneInfo->GetBoneWaterInfo();
			vfxAssertf(pVfxPedBoneWaterInfo, "vfx skeleton bone %d defines a bone water info that doesn't exist %s", i, pVfxPedSkeletonBoneInfo->m_boneWaterInfoName.GetCStr());
		}
	}

	for (atBinaryMap<VfxPedFootInfo*, atHashString>::Iterator pedFootInfoIterator=m_vfxPedFootInfos.Begin(); pedFootInfoIterator!=m_vfxPedFootInfos.End(); ++pedFootInfoIterator)
	{
		VfxPedFootInfo*& pVfxPedFootInfo = *pedFootInfoIterator;
		const atHashString& keyHashString = pedFootInfoIterator.GetKey();

		const VfxPedFootPtFxInfo* pVfxPedFootPtFxInfo = pVfxPedFootInfo->GetPtFxInfo();			
		vfxAssertf(pVfxPedFootPtFxInfo, "vfx ped foot info %s defines a ped foot ptfx info that doesn't exist %s", keyHashString.GetCStr(), pVfxPedFootInfo->m_ptFxInfoName.GetCStr());
		const VfxPedFootDecalInfo* pVfxPedFootDecalInfo = pVfxPedFootInfo->GetDecalInfo();
		vfxAssertf(pVfxPedFootDecalInfo, "vfx ped foot info %s defines a ped foot decal info that doesn't exist %s", keyHashString.GetCStr(), pVfxPedFootInfo->m_decalInfoName.GetCStr());
	}

	for (atBinaryMap<VfxPedWadeInfo*, atHashString>::Iterator pedWadeInfoIterator=m_vfxPedWadeInfos.Begin(); pedWadeInfoIterator!=m_vfxPedWadeInfos.End(); ++pedWadeInfoIterator)
	{
		VfxPedWadeInfo*& pVfxPedWadeInfo = *pedWadeInfoIterator;
		const atHashString& keyHashString = pedWadeInfoIterator.GetKey();

		const VfxPedWadePtFxInfo* pVfxPedWadePtFxInfo = pVfxPedWadeInfo->GetPtFxInfo();			
		vfxAssertf(pVfxPedWadePtFxInfo, "vfx ped wade info %s defines a ped wade ptfx info that doesn't exist %s", keyHashString.GetCStr(), pVfxPedWadeInfo->m_ptFxInfoName.GetCStr());
		const VfxPedWadeDecalInfo* pVfxPedWadeDecalInfo = pVfxPedWadeInfo->GetDecalInfo();
		vfxAssertf(pVfxPedWadeDecalInfo, "vfx ped wade info %s defines a ped wade decal info that doesn't exist %s", keyHashString.GetCStr(), pVfxPedWadeInfo->m_decalInfoName.GetCStr());
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  SetupWaterDripInfos
///////////////////////////////////////////////////////////////////////////////

void CVfxPedInfoMgr::SetupWaterDripInfos()
{
	for (atBinaryMap<CVfxPedInfo*, atHashString>::Iterator pedInfoIterator=m_vfxPedInfos.Begin(); pedInfoIterator!=m_vfxPedInfos.End(); ++pedInfoIterator)
	{
		CVfxPedInfo*& pVfxPedInfo = *pedInfoIterator;

		int numSkeletonBoneInfos = pVfxPedInfo->GetPedSkeletonBoneNumInfos();

		// set up the water drip infos
		int numWaterDripBones = 0;
		for (int i=0; i<numSkeletonBoneInfos; i++)
		{
			const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
			const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();
			if (pBoneWaterInfo && pBoneWaterInfo->m_waterDripPtFxEnabled)
			{
				numWaterDripBones++;
			}
		}

		pVfxPedInfo->m_waterDripPtFxToSkeltonBoneIds.Reset();
		pVfxPedInfo->m_waterDripPtFxToSkeltonBoneIds.Reserve(numWaterDripBones);
		pVfxPedInfo->m_waterDripPtFxToSkeltonBoneIds.Resize(numWaterDripBones);

		pVfxPedInfo->m_waterDripPtFxFromSkeltonBoneIds.Reset();
		pVfxPedInfo->m_waterDripPtFxFromSkeltonBoneIds.Reserve(numSkeletonBoneInfos);
		pVfxPedInfo->m_waterDripPtFxFromSkeltonBoneIds.Resize(numSkeletonBoneInfos);

		numWaterDripBones = 0;
		for (int i=0; i<numSkeletonBoneInfos; i++)
		{
			const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
			const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();
			if (pBoneWaterInfo && pBoneWaterInfo->m_waterDripPtFxEnabled)
			{
				pVfxPedInfo->m_waterDripPtFxToSkeltonBoneIds[numWaterDripBones] = i;
				pVfxPedInfo->m_waterDripPtFxFromSkeltonBoneIds[i] = numWaterDripBones;
				numWaterDripBones++;
			}
			else
			{
				pVfxPedInfo->m_waterDripPtFxFromSkeltonBoneIds[i] = -1;
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PreLoadCallback
///////////////////////////////////////////////////////////////////////////////

void CVfxPedInfoMgr::PreLoadCallback(parTreeNode* UNUSED_PARAM(pNode))
{
#if __DEV
	int vfxPedInfoCount = m_vfxPedInfos.GetCount();
	if (vfxPedInfoCount>0)
	{
		m_vfxPedLiveEditUpdateData.Reserve(vfxPedInfoCount);
		m_vfxPedLiveEditUpdateData.Resize(vfxPedInfoCount);

		int count = 0;
		for (atBinaryMap<CVfxPedInfo*, atHashString>::Iterator vehInfoIterator=m_vfxPedInfos.Begin(); vehInfoIterator!=m_vfxPedInfos.End(); ++vehInfoIterator)
		{
			CVfxPedInfo*& pVfxPedInfo = *vehInfoIterator;

			m_vfxPedLiveEditUpdateData[count].pOrig = pVfxPedInfo;
			m_vfxPedLiveEditUpdateData[count].hashName = vehInfoIterator.GetKey().GetHash();
			m_vfxPedLiveEditUpdateData[count].pNew = NULL;

			count++;
		}

		vfxAssertf(count==vfxPedInfoCount, "count mismatch");
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  PostLoadCallback
///////////////////////////////////////////////////////////////////////////////

void CVfxPedInfoMgr::PostLoadCallback()
{
#if __DEV
	int vfxPedLiveEditUpdateDataCount = m_vfxPedLiveEditUpdateData.GetCount();
	if (vfxPedLiveEditUpdateDataCount>0)
	{
		int vfxPedInfoCount = m_vfxPedInfos.GetCount();

		int count = 0;
		for (atBinaryMap<CVfxPedInfo*, atHashString>::Iterator vehInfoIterator=m_vfxPedInfos.Begin(); vehInfoIterator!=m_vfxPedInfos.End(); ++vehInfoIterator)
		{
			CVfxPedInfo*& pVfxPedInfo = *vehInfoIterator;

			u32 hashName = vehInfoIterator.GetKey().GetHash();
			for (int j=0; j<m_vfxPedLiveEditUpdateData.GetCount(); j++)
			{
				if (m_vfxPedLiveEditUpdateData[j].hashName==hashName)
				{
					m_vfxPedLiveEditUpdateData[j].pNew = pVfxPedInfo;
					break;
				}
			}

			count++;
		}

		vfxAssertf(count==vfxPedInfoCount, "count mismatch");

		CModelInfo::UpdateVfxPedInfos();

		SetupWaterDripInfos();

		m_vfxPedLiveEditUpdateData.Reset();
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  UpdatePedModelInfo
///////////////////////////////////////////////////////////////////////////////

#if __DEV
void CVfxPedInfoMgr::UpdatePedModelInfo(CPedModelInfo* pPedModelInfo)
{
	int vfxPedLiveEditUpdateDataCount = g_vfxPedInfoMgr.m_vfxPedLiveEditUpdateData.GetCount();

	for (int i=0; i<vfxPedLiveEditUpdateDataCount; i++)
	{
		if (g_vfxPedInfoMgr.m_vfxPedLiveEditUpdateData[i].pOrig==pPedModelInfo->GetVfxInfo())
		{
			pPedModelInfo->SetVfxInfo(g_vfxPedInfoMgr.m_vfxPedLiveEditUpdateData[i].pNew);
			return;
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  GetPedFootDecalInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedFootDecalInfo* CVfxPedInfoMgr::GetPedFootDecalInfo(u32 hashName)
{
	VfxPedFootDecalInfo** ppVfxPedFootDecalInfo = m_vfxPedFootDecalInfos.SafeGet(hashName);
	if (ppVfxPedFootDecalInfo)
	{
		return *ppVfxPedFootDecalInfo;
	}

	vfxAssertf(0, "vfx ped foot decal info not found");

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  GetPedFootPtFxInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedFootPtFxInfo* CVfxPedInfoMgr::GetPedFootPtFxInfo(u32 hashName)
{
	VfxPedFootPtFxInfo** ppVfxPedFootPtFxInfo = m_vfxPedFootPtFxInfos.SafeGet(hashName);
	if (ppVfxPedFootPtFxInfo)
	{
		return *ppVfxPedFootPtFxInfo;
	}

	vfxAssertf(0, "vfx ped foot ptfx info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPedFootInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedFootInfo* CVfxPedInfoMgr::GetPedFootInfo(u32 hashName)
{
	VfxPedFootInfo** ppVfxPedFootInfo = m_vfxPedFootInfos.SafeGet(hashName);
	if (ppVfxPedFootInfo)
	{
		return *ppVfxPedFootInfo;
	}

	vfxAssertf(0, "vfx ped foot info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPedWadeDecalInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedWadeDecalInfo* CVfxPedInfoMgr::GetPedWadeDecalInfo(u32 hashName)
{
	VfxPedWadeDecalInfo** ppVfxPedWadeDecalInfo = m_vfxPedWadeDecalInfos.SafeGet(hashName);
	if (ppVfxPedWadeDecalInfo)
	{
		return *ppVfxPedWadeDecalInfo;
	}

	vfxAssertf(0, "vfx ped wade decal info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPedWadePtFxInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedWadePtFxInfo* CVfxPedInfoMgr::GetPedWadePtFxInfo(u32 hashName)
{
	VfxPedWadePtFxInfo** ppVfxPedWadePtFxInfo = m_vfxPedWadePtFxInfos.SafeGet(hashName);
	if (ppVfxPedWadePtFxInfo)
	{
		return *ppVfxPedWadePtFxInfo;
	}

	vfxAssertf(0, "vfx ped wade ptfx info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPedWadeInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedWadeInfo* CVfxPedInfoMgr::GetPedWadeInfo(u32 hashName)
{
	VfxPedWadeInfo** ppVfxPedWadeInfo = m_vfxPedWadeInfos.SafeGet(hashName);
	if (ppVfxPedWadeInfo)
	{
		return *ppVfxPedWadeInfo;
	}

	vfxAssertf(0, "vfx ped wade info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPedBoneWadeInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedBoneWadeInfo* CVfxPedInfoMgr::GetPedBoneWadeInfo(u32 hashName)
{
	VfxPedBoneWadeInfo** ppVfxPedBoneWadeInfo = m_vfxPedBoneWadeInfos.SafeGet(hashName);
	if (ppVfxPedBoneWadeInfo)
	{
		return *ppVfxPedBoneWadeInfo;
	}

	vfxAssertf(0, "vfx ped bone wade info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPedBoneWaterInfo
///////////////////////////////////////////////////////////////////////////////

VfxPedBoneWaterInfo* CVfxPedInfoMgr::GetPedBoneWaterInfo(u32 hashName)
{
	VfxPedBoneWaterInfo** ppVfxPedBoneWaterInfo = m_vfxPedBoneWaterInfos.SafeGet(hashName);
	if (ppVfxPedBoneWaterInfo)
	{
		return *ppVfxPedBoneWaterInfo;
	}

	vfxAssertf(0, "vfx ped bone water info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetInfo
///////////////////////////////////////////////////////////////////////////////

CVfxPedInfo* CVfxPedInfoMgr::GetInfo(u32 hashName)
{
	CVfxPedInfo** ppVfxPedInfo = m_vfxPedInfos.SafeGet(hashName);
	if (ppVfxPedInfo)
	{
		return *ppVfxPedInfo;
	}

	vfxAssertf(0, "vfx ped info not found");

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetInfo
///////////////////////////////////////////////////////////////////////////////

CVfxPedInfo* CVfxPedInfoMgr::GetInfo(CPed* pPed)
{
	CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(pPed->GetBaseModelInfo());
	vfxAssertf(pPedModelInfo, "can't get ped model info");
	CVfxPedInfo* pVfxPedInfo = pPedModelInfo->GetVfxInfo();
	vfxAssertf(pVfxPedInfo, "can't get vfx ped info from model info");
	return pVfxPedInfo;
}


///////////////////////////////////////////////////////////////////////////////
//  SetupDecalRenderSettings
///////////////////////////////////////////////////////////////////////////////

void CVfxPedInfoMgr::SetupDecalRenderSettings()
{
	// set up vfx foot decals
	for (atBinaryMap<VfxPedFootDecalInfo*, atHashWithStringNotFinal>::Iterator pedFootDecalInfoIterator=m_vfxPedFootDecalInfos.Begin(); pedFootDecalInfoIterator!=m_vfxPedFootDecalInfos.End(); ++pedFootDecalInfoIterator)
	{
		VfxPedFootDecalInfo*& pVfxPedFootDecalInfo = *pedFootDecalInfoIterator;

		pVfxPedFootDecalInfo->m_decalRenderSettingIndexBare = -1;
		if (pVfxPedFootDecalInfo->m_decalIdBare>0)
		{
			g_decalMan.FindRenderSettingInfo(pVfxPedFootDecalInfo->m_decalIdBare, pVfxPedFootDecalInfo->m_decalRenderSettingIndexBare, pVfxPedFootDecalInfo->m_decalRenderSettingCountBare);
		}

		pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe = -1;
		if (pVfxPedFootDecalInfo->m_decalIdShoe>0)
		{
			g_decalMan.FindRenderSettingInfo(pVfxPedFootDecalInfo->m_decalIdShoe, pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe, pVfxPedFootDecalInfo->m_decalRenderSettingCountShoe);
		}

		pVfxPedFootDecalInfo->m_decalRenderSettingIndexBoot = -1;
		if (pVfxPedFootDecalInfo->m_decalIdBoot>0)
		{
			g_decalMan.FindRenderSettingInfo(pVfxPedFootDecalInfo->m_decalIdBoot, pVfxPedFootDecalInfo->m_decalRenderSettingIndexBoot, pVfxPedFootDecalInfo->m_decalRenderSettingCountBoot);
		}

		pVfxPedFootDecalInfo->m_decalRenderSettingIndexHeel = -1;
		if (pVfxPedFootDecalInfo->m_decalIdHeel>0)
		{
			g_decalMan.FindRenderSettingInfo(pVfxPedFootDecalInfo->m_decalIdHeel, pVfxPedFootDecalInfo->m_decalRenderSettingIndexHeel, pVfxPedFootDecalInfo->m_decalRenderSettingCountHeel);
		}

		pVfxPedFootDecalInfo->m_decalRenderSettingIndexFlipFlop = -1;
		if (pVfxPedFootDecalInfo->m_decalIdFlipFlop>0)
		{
			g_decalMan.FindRenderSettingInfo(pVfxPedFootDecalInfo->m_decalIdFlipFlop, pVfxPedFootDecalInfo->m_decalRenderSettingIndexFlipFlop, pVfxPedFootDecalInfo->m_decalRenderSettingCountFlipFlop);
		}

		pVfxPedFootDecalInfo->m_decalRenderSettingIndexFlipper = -1;
		if (pVfxPedFootDecalInfo->m_decalIdFlipper>0)
		{
			g_decalMan.FindRenderSettingInfo(pVfxPedFootDecalInfo->m_decalIdFlipper, pVfxPedFootDecalInfo->m_decalRenderSettingIndexFlipper, pVfxPedFootDecalInfo->m_decalRenderSettingCountFlipper);
		}
	}

	// set up vfx wade decals
	for (atBinaryMap<VfxPedWadeDecalInfo*, atHashWithStringNotFinal>::Iterator pedWadeDecalInfoIterator=m_vfxPedWadeDecalInfos.Begin(); pedWadeDecalInfoIterator!=m_vfxPedWadeDecalInfos.End(); ++pedWadeDecalInfoIterator)
	{
		VfxPedWadeDecalInfo*& pVfxPedWadeDecalInfo = *pedWadeDecalInfoIterator;

		pVfxPedWadeDecalInfo->m_decalRenderSettingIndex = -1;
		if (pVfxPedWadeDecalInfo->m_decalId>0)
		{
			g_decalMan.FindRenderSettingInfo(pVfxPedWadeDecalInfo->m_decalId, pVfxPedWadeDecalInfo->m_decalRenderSettingIndex, pVfxPedWadeDecalInfo->m_decalRenderSettingCount);
		}
	}
}