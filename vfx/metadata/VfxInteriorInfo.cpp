///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxInteriorInfo.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	22 August 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxInteriorInfo.h"
#include "VfxInteriorInfo_parser.h"

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

CVfxInteriorInfoMgr g_vfxInteriorInfoMgr;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxInteriorSetup
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GetRoomSetup
///////////////////////////////////////////////////////////////////////////////

CVfxRoomSetup* CVfxInteriorSetup::GetRoomSetup(u32 roomHashName)
{
	CVfxRoomSetup** ppVfxRoomSetup = m_vfxRoomSetups.SafeGet(roomHashName);
	if (ppVfxRoomSetup)
	{
		return *ppVfxRoomSetup;
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxInteriorInfoMgr
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void CVfxInteriorInfoMgr::LoadData()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VFXINTERIORINFO_FILE);
	vfxAssertf(DATAFILEMGR.IsValid(pData), "vfx interior info file is not available");
	if (DATAFILEMGR.IsValid(pData))
	{
		fwPsoStoreLoader::LoadDataIntoObject(pData->m_filename, META_FILE_EXT, *this);
		//PARSER.SaveObject(pData->m_filename, "meta", this);
		parRestRegisterSingleton("Vfx/InteriorInfo", *this, pData->m_filename);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetInfo
///////////////////////////////////////////////////////////////////////////////

CVfxInteriorInfo* CVfxInteriorInfoMgr::GetInfo(u32 interiorHashName, u32 roomHashName)
{
	// initialise the name of the interior info
	atHashWithStringNotFinal interiorInfoHashName("VFXINTERIORINFO_DEFAULT",0x824EA138);

	// search for this interior being setup
	CVfxInteriorSetup** ppVfxInteriorSetup = m_vfxInteriorSetups.SafeGet(interiorHashName);
	if (ppVfxInteriorSetup)
	{
		// use this info from this interior 
		CVfxInteriorSetup* pVfxInteriorSetup = (*ppVfxInteriorSetup);
		interiorInfoHashName = pVfxInteriorSetup->GetInteriorInfoName();

		// search for this interior's room being setup
		CVfxRoomSetup* pVfxRoomSetup = pVfxInteriorSetup->GetRoomSetup(roomHashName);
		if (pVfxRoomSetup)
		{
			// use this info from this room 
			interiorInfoHashName = pVfxRoomSetup->GetInteriorInfoName();
		}
	}
	
	// search for the interior info
	CVfxInteriorInfo** ppVfxInteriorInfo = m_vfxInteriorInfos.SafeGet(interiorInfoHashName);
	if (ppVfxInteriorInfo)
	{
		return *ppVfxInteriorInfo;
	}

	// didn't find it - use the default
	vfxAssertf(0, "vfx interior info not found");

	return NULL;
}
