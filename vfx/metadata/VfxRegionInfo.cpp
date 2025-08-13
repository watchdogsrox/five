///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxRegionInfo.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	22 August 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxRegionInfo.h"
#include "VfxRegionInfo_parser.h"

// rage
#include "parser/manager.h"
#include "parser/restparserservices.h"

// framework
#include "fwscene/stores/psostore.h"
#include "fwsys/fileexts.h"
#include "vfx/channel.h"

// game
#include "Game/Clock.h"
#include "Game/Weather.h"
#include "Peds/PopCycle.h"
#include "Scene/DataFileMgr.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxRegionInfoMgr g_vfxRegionInfoMgr;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxRegionGpuPtFxInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GetGpuPtFxActive
///////////////////////////////////////////////////////////////////////////////

bool CVfxRegionGpuPtFxInfo::GetGpuPtFxActive() const
{
	float currTemperature = g_weather.GetTemperature(CVfxHelper::GetGameCamPos().GetZf());
	return g_weather.GetSun()>=m_gpuPtFxSunThresh && 
		   g_weather.GetWind()>=m_gpuPtFxWindThresh && 
		   currTemperature>=m_gpuPtFxTempThresh &&
		   CClock::IsTimeInRange(int(m_gpuPtFxTimeMin), int(m_gpuPtFxTimeMax));
}

///////////////////////////////////////////////////////////////////////////////
//  GetGpuPtFxMaxLevel
///////////////////////////////////////////////////////////////////////////////

float CVfxRegionGpuPtFxInfo::GetGpuPtFxMaxLevel() const
{
	return m_gpuPtFxMaxLevelNoWind + (g_weather.GetWind()*(m_gpuPtFxMaxLevelFullWind-m_gpuPtFxMaxLevelNoWind));
}

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

bool CVfxRegionGpuPtFxInfo::Update(bool isCurrRegion)
{
	// set the target level (all zero unless this is the current region)
	m_gpuPtFxTargetLevel = 0.0f;
	if (isCurrRegion && GetGpuPtFxActive())
	{
		m_gpuPtFxTargetLevel = GetGpuPtFxMaxLevel();					
	}

	Approach(m_gpuPtFxCurrLevel, m_gpuPtFxTargetLevel, m_gpuPtFxInterpRate, TIME.GetSeconds());

	return m_gpuPtFxCurrLevel>0.0f;
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxRegionInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  ResetGpuPtFxInfos
///////////////////////////////////////////////////////////////////////////////

void CVfxRegionInfo::ResetGpuPtFxInfos()
{
	for (int i=0; i<m_vfxRegionGpuPtFxDropInfos.GetCount(); i++)
	{
		CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxDropInfos[i];
		if (pVfxRegionGpuPtFxInfo->GetGpuPtFxEnabled())
		{
			pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel = 0.0f;
			pVfxRegionGpuPtFxInfo->m_gpuPtFxTargetLevel = 0.0f;
		}
	}

	for (int i=0; i<m_vfxRegionGpuPtFxMistInfos.GetCount(); i++)
	{
		CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxMistInfos[i];
		if (pVfxRegionGpuPtFxInfo->GetGpuPtFxEnabled())
		{
			pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel = 0.0f;
			pVfxRegionGpuPtFxInfo->m_gpuPtFxTargetLevel = 0.0f;
		}
	}

	for (int i=0; i<m_vfxRegionGpuPtFxGroundInfos.GetCount(); i++)
	{
		CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxGroundInfos[i];
		if (pVfxRegionGpuPtFxInfo->GetGpuPtFxEnabled())
		{
			pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel = 0.0f;
			pVfxRegionGpuPtFxInfo->m_gpuPtFxTargetLevel = 0.0f;
		}
	}

	m_pActiveVfxRegionGpuPtFxDropInfo = NULL;
	m_pActiveVfxRegionGpuPtFxMistInfo = NULL;
	m_pActiveVfxRegionGpuPtFxGroundInfo = NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateGpuPtFxInfos
///////////////////////////////////////////////////////////////////////////////

bool CVfxRegionInfo::UpdateGpuPtFxInfos(bool isCurrRegion)
{
	// check if there's an active region gpu ptfx info
	if (m_pActiveVfxRegionGpuPtFxDropInfo)
	{
		// update the active region gpu ptfx info
		if (m_pActiveVfxRegionGpuPtFxDropInfo->Update(isCurrRegion)==false)
		{
			// the region gpu ptfx info is no longer active
			m_pActiveVfxRegionGpuPtFxDropInfo = NULL;
		}
	}

	// if we don't have an active region gpu ptfx info we need to see if there is an active one
	if (m_pActiveVfxRegionGpuPtFxDropInfo==NULL)
	{
		for (int i=0; i<m_vfxRegionGpuPtFxDropInfos.GetCount(); i++)
		{
			CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxDropInfos[i];
			if (pVfxRegionGpuPtFxInfo->GetGpuPtFxEnabled())
			{
				if (pVfxRegionGpuPtFxInfo->Update(isCurrRegion))
				{
					m_pActiveVfxRegionGpuPtFxDropInfo = pVfxRegionGpuPtFxInfo;
					break;
				}
			}
		}
	}



	// check if there's an active region gpu ptfx info
	if (m_pActiveVfxRegionGpuPtFxMistInfo)
	{
		// update the active region gpu ptfx info
		if (m_pActiveVfxRegionGpuPtFxMistInfo->Update(isCurrRegion)==false)
		{
			// the region gpu ptfx info is no longer active
			m_pActiveVfxRegionGpuPtFxMistInfo = NULL;
		}
	}

	// if we don't have an active region gpu ptfx info we need to see if there is an active one
	if (m_pActiveVfxRegionGpuPtFxMistInfo==NULL)
	{
		for (int i=0; i<m_vfxRegionGpuPtFxMistInfos.GetCount(); i++)
		{
			CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxMistInfos[i];
			if (pVfxRegionGpuPtFxInfo->GetGpuPtFxEnabled())
			{
				if (pVfxRegionGpuPtFxInfo->Update(isCurrRegion))
				{
					m_pActiveVfxRegionGpuPtFxMistInfo = pVfxRegionGpuPtFxInfo;
					break;
				}
			}
		}
	}



	// check if there's an active region gpu ptfx info
	if (m_pActiveVfxRegionGpuPtFxGroundInfo)
	{
		// update the active region gpu ptfx info
		if (m_pActiveVfxRegionGpuPtFxGroundInfo->Update(isCurrRegion)==false)
		{
			// the region gpu ptfx info is no longer active
			m_pActiveVfxRegionGpuPtFxGroundInfo = NULL;
		}
	}

	// if we don't have an active region gpu ptfx info we need to see if there is an active one
	if (m_pActiveVfxRegionGpuPtFxGroundInfo==NULL)
	{
		for (int i=0; i<m_vfxRegionGpuPtFxGroundInfos.GetCount(); i++)
		{
			CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxGroundInfos[i];
			if (pVfxRegionGpuPtFxInfo->GetGpuPtFxEnabled())
			{
				if (pVfxRegionGpuPtFxInfo->Update(isCurrRegion))
				{
					m_pActiveVfxRegionGpuPtFxGroundInfo = pVfxRegionGpuPtFxInfo;
					break;
				}
			}
		}
	}



 	return m_pActiveVfxRegionGpuPtFxDropInfo!=NULL || 
		   m_pActiveVfxRegionGpuPtFxMistInfo!=NULL || 
		   m_pActiveVfxRegionGpuPtFxGroundInfo!=NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  VerifyGpuPtFxInfos
///////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void CVfxRegionInfo::VerifyGpuPtFxInfos(bool isActiveRegion)
{
	if (isActiveRegion)
	{
		vfxAssertf(m_pActiveVfxRegionGpuPtFxDropInfo || m_pActiveVfxRegionGpuPtFxMistInfo || m_pActiveVfxRegionGpuPtFxGroundInfo, "active region has an inactive gpu ptfx info");

		// go through the gpu ptfx infos
		if (m_pActiveVfxRegionGpuPtFxDropInfo)
		{
			u32 numActive = 0;
			for (int i=0; i<m_vfxRegionGpuPtFxDropInfos.GetCount(); i++)
			{
				CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxDropInfos[i];
				if (pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel>0.0f)
				{
					numActive++;
				}
			}

			vfxAssertf(numActive==1, "active region has %d gpu ptfx infos with a non-zero curr level", numActive);
		}

		if (m_pActiveVfxRegionGpuPtFxMistInfo)
		{
			// go through the gpu ptfx infos
			u32 numActive = 0;
			for (int i=0; i<m_vfxRegionGpuPtFxMistInfos.GetCount(); i++)
			{
				CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxMistInfos[i];
				if (pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel>0.0f)
				{
					numActive++;
				}
			}

			vfxAssertf(numActive==1, "active region has %d gpu ptfx infos with a non-zero curr level", numActive);

		}

		if (m_pActiveVfxRegionGpuPtFxGroundInfo)
		{
			// go through the gpu ptfx infos
			u32 numActive = 0;
			for (int i=0; i<m_vfxRegionGpuPtFxGroundInfos.GetCount(); i++)
			{
				CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxGroundInfos[i];
				if (pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel>0.0f)
				{
					numActive++;
				}
			}

			vfxAssertf(numActive==1, "active region has %d gpu ptfx infos with a non-zero curr level", numActive);
		}
	}
	else
	{
		vfxAssertf(m_pActiveVfxRegionGpuPtFxDropInfo==NULL, "inactive region has an active gpu ptfx info");
		vfxAssertf(m_pActiveVfxRegionGpuPtFxMistInfo==NULL, "inactive region has an active gpu ptfx info");
		vfxAssertf(m_pActiveVfxRegionGpuPtFxGroundInfo==NULL, "inactive region has an active gpu ptfx info");

		// go through the gpu ptfx infos
		for (int i=0; i<m_vfxRegionGpuPtFxDropInfos.GetCount(); i++)
		{
			CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxDropInfos[i];
			vfxAssertf(pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel==0.0f, "inactive region has a gpu ptfx info with a non-zero curr level");
		}

		// go through the gpu ptfx infos
		for (int i=0; i<m_vfxRegionGpuPtFxMistInfos.GetCount(); i++)
		{
			CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxMistInfos[i];
			vfxAssertf(pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel==0.0f, "inactive region has a gpu ptfx info with a non-zero curr level");
		}

		// go through the gpu ptfx infos
		for (int i=0; i<m_vfxRegionGpuPtFxGroundInfos.GetCount(); i++)
		{
			CVfxRegionGpuPtFxInfo* pVfxRegionGpuPtFxInfo = &m_vfxRegionGpuPtFxGroundInfos[i];
			vfxAssertf(pVfxRegionGpuPtFxInfo->m_gpuPtFxCurrLevel==0.0f, "inactive region has a gpu ptfx info with a non-zero curr level");
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxRegionInfoMgr
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void CVfxRegionInfoMgr::LoadData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VFXREGIONINFO_FILE);
	vfxAssertf(DATAFILEMGR.IsValid(pData), "vfx region info file is not available");
	if (DATAFILEMGR.IsValid(pData))
	{
		fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, *this);
		//PARSER.SaveObject(pData->m_filename, "meta", this);
		parRestRegisterSingleton("Vfx/RegionInfo", *this, pData->m_filename);
	}

	m_disableRegionVfxScriptThreadId = THREAD_INVALID;
	m_disableRegionVfx = false;

	m_pActiveVfxRegionInfo = NULL;

#if __BANK
	for (int i=0; i<MAX_DEBUG_REGION_NAMES; i++)
	{
		m_vfxRegionNames[i] = NULL;
	}

	int currIdx = 0;
	for (atBinaryMap<CVfxRegionInfo*, atHashString>::Iterator regionInfoIterator=m_vfxRegionInfos.Begin(); regionInfoIterator!=m_vfxRegionInfos.End(); ++regionInfoIterator)
	{
		if (vfxVerifyf(currIdx<MAX_DEBUG_REGION_NAMES, "too many vfx regions defined"))
		{
			m_vfxRegionNames[currIdx] = regionInfoIterator.GetKey().TryGetCStr();
		}

		currIdx++;
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  GetInfo
///////////////////////////////////////////////////////////////////////////////

CVfxRegionInfo* CVfxRegionInfoMgr::GetInfo(atHashWithStringNotFinal hashName)
{
	if (m_disableRegionVfx)
	{
		return NULL;
	}

	// look up the region 
	CVfxRegionInfo** ppVfxRegionInfo = m_vfxRegionInfos.SafeGet(hashName);
	if (ppVfxRegionInfo)
	{
		return *ppVfxRegionInfo;
	}

	// look up the default region instead
	ppVfxRegionInfo = m_vfxRegionInfos.SafeGet(VFXREGIONINFO_DEFAULT_HASHNAME);
	if (ppVfxRegionInfo)
	{
		vfxAssertf(0, "vfx region info not found (%s) - using default instead", hashName.GetCStr());
		return *ppVfxRegionInfo;
	}

	// no default region exists - just return NULL
	vfxAssertf(0, "vfx region info not found (%s) - no default found either", hashName.GetCStr());
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateGpuPtFx
///////////////////////////////////////////////////////////////////////////////

void CVfxRegionInfoMgr::UpdateGpuPtFx(bool isActive, s32 BANK_ONLY(forceRegionIdx))
{
	if (m_disableRegionVfx)
	{
		isActive = false;
	}

	// get the current region that we're in
	CVfxRegionInfo* pCurrVfxRegionInfo = NULL;
	if (isActive)
	{
#if __BANK
		if (forceRegionIdx>-1)
		{
			pCurrVfxRegionInfo = g_vfxRegionInfoMgr.GetInfo(m_vfxRegionNames[forceRegionIdx]);
		}
		else
#endif
		{
			pCurrVfxRegionInfo = g_vfxRegionInfoMgr.GetInfo(CPopCycle::GetCurrentZoneVfxRegionHashName());
		}
	}

	// check if the system is active this frame
	if (isActive)
	{
		// it is active this frame
		// check if there's an active region
		if (m_pActiveVfxRegionInfo)
		{
			// update the active region
			if (m_pActiveVfxRegionInfo->UpdateGpuPtFxInfos(m_pActiveVfxRegionInfo==pCurrVfxRegionInfo)==false)
			{
				// the region is no longer active
				m_pActiveVfxRegionInfo = NULL;
			}
		}

		// if we don't have an active region we need to set the current region as the active one and update it
		if (m_pActiveVfxRegionInfo==NULL)
		{
			m_pActiveVfxRegionInfo = pCurrVfxRegionInfo;

			// update the current region
			if (pCurrVfxRegionInfo->UpdateGpuPtFxInfos(true)==false)
			{
				// the region is no longer active
				m_pActiveVfxRegionInfo = NULL;
			}
		}
	}
	else
	{
		// it isn't active this frame
		// if there's an active region we need to reset it
		if (m_pActiveVfxRegionInfo)
		{
			m_pActiveVfxRegionInfo->ResetGpuPtFxInfos();
			m_pActiveVfxRegionInfo = NULL;
		}
	}

#if __ASSERT
	// verify the data is as expected
	for (atBinaryMap<CVfxRegionInfo*, atHashString>::Iterator regionInfoIterator=m_vfxRegionInfos.Begin(); regionInfoIterator!=m_vfxRegionInfos.End(); ++regionInfoIterator)
	{
		CVfxRegionInfo*& pVfxRegionInfo = *regionInfoIterator;

		if (pVfxRegionInfo==m_pActiveVfxRegionInfo)
		{
			// check that this region should be active
			pVfxRegionInfo->VerifyGpuPtFxInfos(true);

		}
		else
		{
			// check that this region should not be active
			pVfxRegionInfo->VerifyGpuPtFxInfos(false);
		}
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void CVfxRegionInfoMgr::RemoveScript(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_disableRegionVfxScriptThreadId)
	{
		m_disableRegionVfx = false;
		m_disableRegionVfxScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetDisableRegionVfx
///////////////////////////////////////////////////////////////////////////////

void CVfxRegionInfoMgr::SetDisableRegionVfx(bool val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_disableRegionVfxScriptThreadId==THREAD_INVALID || m_disableRegionVfxScriptThreadId==scriptThreadId, "trying to disable region vfx when this is already in use by another script")) 
	{
		m_disableRegionVfx = val; 
		m_disableRegionVfxScriptThreadId = scriptThreadId;
	}
}