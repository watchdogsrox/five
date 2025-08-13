///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxVehicleInfo.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	22 August 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxVehicleInfo.h"
#include "VfxVehicleInfo_parser.h"

// rage
#include "parser/manager.h"
#include "parser/restparserservices.h"

// framework
#include "fwscene/stores/psostore.h"
#include "fwsys/fileexts.h"
#include "vfx/channel.h"

// game
#include "ModelInfo/VehicleModelInfo.h"
#include "Scene/DataFileMgr.h"
#include "Vehicles/Vehicle.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxVehicleInfoMgr g_vfxVehicleInfoMgr;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxVehicleInfo
///////////////////////////////////////////////////////////////////////////////

CVfxVehicleInfo::CVfxVehicleInfo() 
{
}

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxVehicleInfoMgr
///////////////////////////////////////////////////////////////////////////////

CVfxVehicleInfoMgr::CVfxVehicleInfoMgr() 
{
#if __DEV
	m_vfxVehicleLiveEditUpdateData.Reset();
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

class CVfxVehicleInfoFileMounter: public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		switch(file.m_fileType)
		{
			case CDataFileMgr::VFXVEHICLEINFO_FILE:
				return g_vfxVehicleInfoMgr.AppendData(file.m_filename);
			default:
			break;
		}
		return false;
	}
	virtual void UnloadDataFile(const CDataFileMgr::DataFile& /*file*/)
	{
		
	}
}g_vfxVehicleInfoFileMounter;

void CVfxVehicleInfoMgr::LoadData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VFXVEHICLEINFO_FILE);
	vfxAssertf(DATAFILEMGR.IsValid(pData), "vfx vehicle info file is not available");
	if (DATAFILEMGR.IsValid(pData))
	{
		fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, *this);
		//PARSER.SaveObject(pData->m_filename, "meta", this);
		parRestRegisterSingleton("Vfx/VehicleInfo", *this, pData->m_filename);
	}
	CDataFileMount::RegisterMountInterface(CDataFileMgr::VFXVEHICLEINFO_FILE,&g_vfxVehicleInfoFileMounter,eDFMI_UnloadFirst);
}

bool CVfxVehicleInfoMgr::AppendData(const char* fileName)
{
	fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CVfxVehicleInfoMgr>();
	fwPsoStoreLoadInstance inst;
	loader.Load(fileName,inst);

	bool bResult = inst.IsLoaded();
	if(Verifyf(bResult,"[CVfxVehicleInfoMgr] Load %s failed!",fileName))
	{
		CVfxVehicleInfoMgr& temp_inst = *reinterpret_cast<CVfxVehicleInfoMgr*>(inst.GetInstance());
		atBinaryMap<CVfxVehicleInfo*, atHashWithStringNotFinal> &to = g_vfxVehicleInfoMgr.m_vfxVehicleInfos;
		atBinaryMap<CVfxVehicleInfo*, atHashWithStringNotFinal> &from = temp_inst.m_vfxVehicleInfos;
		for(int i=0; i< from.GetCount();i++)
		{
			CVfxVehicleInfo** val = from.GetItem(i);
			const atHashWithStringNotFinal* key = from.GetKey(i);
			if(!to.SafeInsert(*key,*val))
			{
				delete *val;
			}
		}
		to.FinishInsertion();
	}
	return bResult;
}

///////////////////////////////////////////////////////////////////////////////
//  PreLoadCallback
///////////////////////////////////////////////////////////////////////////////

void CVfxVehicleInfoMgr::PreLoadCallback(parTreeNode* UNUSED_PARAM(pNode))
{
#if __DEV
	int vfxVehicleInfoCount = m_vfxVehicleInfos.GetCount();
	if (vfxVehicleInfoCount>0)
	{
		m_vfxVehicleLiveEditUpdateData.Reserve(vfxVehicleInfoCount);
		m_vfxVehicleLiveEditUpdateData.Resize(vfxVehicleInfoCount);

		int count = 0;
		for (atBinaryMap<CVfxVehicleInfo*, atHashString>::Iterator vehInfoIterator=m_vfxVehicleInfos.Begin(); vehInfoIterator!=m_vfxVehicleInfos.End(); ++vehInfoIterator)
		{
			CVfxVehicleInfo*& pVfxVehicleInfo = *vehInfoIterator;

			m_vfxVehicleLiveEditUpdateData[count].pOrig = pVfxVehicleInfo;
			m_vfxVehicleLiveEditUpdateData[count].hashName = vehInfoIterator.GetKey().GetHash();
			m_vfxVehicleLiveEditUpdateData[count].pNew = NULL;

			count++;
		}

		vfxAssertf(count==vfxVehicleInfoCount, "count mismatch");
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  PostLoadCallback
///////////////////////////////////////////////////////////////////////////////

void CVfxVehicleInfoMgr::PostLoadCallback()
{
#if __DEV
	int vfxVehicleLiveEditUpdateDataCount = m_vfxVehicleLiveEditUpdateData.GetCount();
	if (vfxVehicleLiveEditUpdateDataCount>0)
	{
		int vfxVehicleInfoCount = m_vfxVehicleInfos.GetCount();

		int count = 0;
		for (atBinaryMap<CVfxVehicleInfo*, atHashString>::Iterator vehInfoIterator=m_vfxVehicleInfos.Begin(); vehInfoIterator!=m_vfxVehicleInfos.End(); ++vehInfoIterator)
		{
			CVfxVehicleInfo*& pVfxVehicleInfo = *vehInfoIterator;

			u32 hashName = vehInfoIterator.GetKey().GetHash();
			for (int j=0; j<m_vfxVehicleLiveEditUpdateData.GetCount(); j++)
			{
				if (m_vfxVehicleLiveEditUpdateData[j].hashName==hashName)
				{
					m_vfxVehicleLiveEditUpdateData[j].pNew = pVfxVehicleInfo;
					break;
				}
			}

			count++;
		}

		vfxAssertf(count==vfxVehicleInfoCount, "count mismatch");

		CModelInfo::UpdateVfxVehicleInfos();

		m_vfxVehicleLiveEditUpdateData.Reset();
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateVehicleModelInfo
///////////////////////////////////////////////////////////////////////////////

#if __DEV
void CVfxVehicleInfoMgr::UpdateVehicleModelInfo(CVehicleModelInfo* pVehicleModelInfo)
{
	int vfxVehicleLiveEditUpdateDataCount = g_vfxVehicleInfoMgr.m_vfxVehicleLiveEditUpdateData.GetCount();

	for (int i=0; i<vfxVehicleLiveEditUpdateDataCount; i++)
	{
		if (g_vfxVehicleInfoMgr.m_vfxVehicleLiveEditUpdateData[i].pOrig==pVehicleModelInfo->GetVfxInfo())
		{
			pVehicleModelInfo->SetVfxInfo(g_vfxVehicleInfoMgr.m_vfxVehicleLiveEditUpdateData[i].pNew);
			return;
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  GetInfo
///////////////////////////////////////////////////////////////////////////////

CVfxVehicleInfo* CVfxVehicleInfoMgr::GetInfo(atHashString name)
{
	CVfxVehicleInfo** ppVfxVehicleInfo = m_vfxVehicleInfos.SafeGet(name.GetHash());
	if (ppVfxVehicleInfo)
	{
		return *ppVfxVehicleInfo;
	}

	vfxAssertf(0, "vfx vehicle info not found for vehicle %s", name.GetCStr());

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  GetInfo
///////////////////////////////////////////////////////////////////////////////

CVfxVehicleInfo* CVfxVehicleInfoMgr::GetInfo(CVehicle* pVehicle)
{
	CVehicleModelInfo* pVehModelInfo = static_cast<CVehicleModelInfo*>(pVehicle->GetBaseModelInfo());
	vfxAssertf(pVehModelInfo, "can't get vehicle model info");
	CVfxVehicleInfo* pVfxVehicleInfo = pVehModelInfo->GetVfxInfo();
	vfxAssertf(pVfxVehicleInfo, "can't get vfx vehicle info from model info");
	return pVfxVehicleInfo;
}
