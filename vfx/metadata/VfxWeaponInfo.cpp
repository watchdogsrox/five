///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWeaponInfo.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	14 February 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxWeaponInfo.h"
#include "VfxWeaponInfo_parser.h"

// rage
#include "parser/manager.h"
#include "parser/restparserservices.h"

// framework
#include "fwscene/stores/psostore.h"
#include "fwsys/fileexts.h"
#include "vfx/channel.h"

// game
#include "Scene/DataFileMgr.h"



///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxWeaponInfoMgr g_vfxWeaponInfoMgr;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxWeaponInfoMgr
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void CVfxWeaponInfoMgr::LoadData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VFXWEAPONINFO_FILE);
	vfxAssertf(DATAFILEMGR.IsValid(pData), "vfx weapon info file is not available");
	if (DATAFILEMGR.IsValid(pData))
	{
		fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, *this);
		//PARSER.SaveObject(pData->m_filename, "meta", this);
		parRestRegisterSingleton("Vfx/WeaponInfo", *this, pData->m_filename);
	}

// #if __ASSERT
// 	for (atBinaryMap<CVfxPedInfo*, atHashString>::Iterator pedInfoIterator=m_vfxPedInfos.Begin(); pedInfoIterator!=m_vfxPedInfos.End(); ++pedInfoIterator)
// 	{
// 		CVfxPedInfo*& pVfxPedInfo = *pedInfoIterator;
// 
// 		const atHashString& keyHashString = pedInfoIterator.GetKey();
// 
// 		// make sure all vfx groups are defined and in the correct order (or none at all)
// 		int numVfxGroupInfos = pVfxPedInfo->GetVfxGroupNumInfos();
// 		vfxAssertf(numVfxGroupInfos==0 || numVfxGroupInfos==NUM_VFX_GROUPS, "vfx ped info %s doesn't define all vfx groups", keyHashString.GetCStr());
// 		for (int i=0; i<pVfxPedInfo->GetVfxGroupNumInfos(); i++)
// 		{
// 			vfxAssertf(pVfxPedInfo->GetVfxGroupInfo(i)->m_vfxGroup==i, "vfx ped info %s doesn't define the vfx groups in the correct order (%d)", keyHashString.GetCStr(), i);
// 		}
// 
//  		// make sure the ped skeleton bone data doesn't exceed the max
//  		vfxAssertf(pVfxPedInfo->GetPedSkeletonBoneNumInfos()<=VFXPED_MAX_VFX_SKELETON_BONES, "vfx ped info %s has more skeleton bones than the max", keyHashString.GetCStr());
// 	}
// #endif
}


///////////////////////////////////////////////////////////////////////////////
//  GetWeaponSpecialMtlPtFxInfo
///////////////////////////////////////////////////////////////////////////////

VfxWeaponSpecialMtlPtFxInfo* CVfxWeaponInfoMgr::GetWeaponSpecialMtlPtFxInfo(u32 hashName)
{
	VfxWeaponSpecialMtlPtFxInfo** ppVfxWeaponSpecialMtlPtFxInfo = m_vfxWeaponSpecialMtlPtFxInfos.SafeGet(hashName);
	if (ppVfxWeaponSpecialMtlPtFxInfo)
	{
		return *ppVfxWeaponSpecialMtlPtFxInfo;
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetWeaponSpecialMtlExpInfo
///////////////////////////////////////////////////////////////////////////////

VfxWeaponSpecialMtlExpInfo* CVfxWeaponInfoMgr::GetWeaponSpecialMtlExpInfo(u32 hashName)
{
	VfxWeaponSpecialMtlExpInfo** ppVfxWeaponSpecialMtlExpInfo = m_vfxWeaponSpecialMtlExpInfos.SafeGet(hashName);
	if (ppVfxWeaponSpecialMtlExpInfo)
	{
		return *ppVfxWeaponSpecialMtlExpInfo;
	}

	return NULL;
}

