#include "Peds/Horse/horseTune.h"

#if ENABLE_HORSE

#include "horseTune_parser.h"

#include "bank/bank.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// hrsSimTune //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
hrsSimTune::hrsSimTune()
{
	m_HrsTypeName = "default";
}

void hrsSimTune::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup(m_HrsTypeName, false);
	m_SpeedTune.AddWidgets(b);
	m_FreshTune.AddWidgets(b);
	m_AgitationTune.AddWidgets(b);
	m_BrakeTune.AddWidgets(b);
	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// hrsSimTuneMgr ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
const hrsSimTune * hrsSimTuneMgr::FindSimTune(const char *pHrsTypeName) const
{
	const hrsSimTune * pSimTune = 0;

	// TODO: OPTIMIZE
	// (A hash would be a nice solution.)
	int numSimTune = m_SimTune.GetCount();
	for( int i=0; i<numSimTune; ++i )
	{
		if( !stricmp(m_SimTune[i].GetHrsTypeName(), pHrsTypeName) )
		{
			pSimTune = &m_SimTune[i];
			break;
		}
	}

	// Fall back to "default" if not found, and if the string isn't "default":
	if( !pSimTune && stricmp("default", pHrsTypeName) )
		pSimTune = FindSimTune("default");

	return pSimTune;
}


////////////////////////////////////////////////////////////////////////////////

class CHorseMountFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		//! Replace the file in the mount code. This allows us to replace the file we accidentally shipped on disc.
		HRSSIMTUNEMGR.Reload(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & UNUSED_PARAM(file)) 
	{
	}

} g_horseMountFileMounter;

void hrsSimTuneMgr::InitClass()
{
	hrsSimTuneMgrSingleton::Instantiate();	

	CDataFileMount::RegisterMountInterface(CDataFileMgr::MOUNT_TUNE_FILE, &g_horseMountFileMounter);
}

void hrsSimTuneMgr::Load(const char* pFileName)
{	
	PARSER.LoadObject(pFileName, "xml", *this, &parSettings::sm_StrictSettings);	
}

void hrsSimTuneMgr::Reload(const char* pFileName)
{
	m_SimTune.Reset();
	Load(pFileName);
}

////////////////////////////////////////////////////////////////////////////////
#if __BANK
void hrsSimTuneMgr::Save()
{
	Verifyf(PARSER.SaveObject("common:/data/mounttune", "xml", this, parManager::XML), "Failed to save mount tune");	
}

////////////////////////////////////////////////////////////////////////////////

void hrsSimTuneMgr::AddWidgets(bkBank & b)
{
	b.PushGroup("File IO", false);
	b.AddButton("Save All", datCallback(MFA(hrsSimTuneMgr::Save), this));
	b.PopGroup();
	hrsObstacleAvoidanceControl::AddStaticWidgets(b);
	b.PushGroup("Per-horse tuning", false);
	int numSimTune = m_SimTune.GetCount();
	for( int i=0; i<numSimTune; ++i )
	{
		m_SimTune[i].AddWidgets(b);
	}
	b.PopGroup();
}
#endif

////////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_HORSE