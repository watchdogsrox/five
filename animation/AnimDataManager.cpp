// Class header
#include "Animation/AnimDataManager.h"

// Game headers
#include "Animation/AnimData.h"
#include "system/memory.h"

//////////////////////////////////////////////////////////////////////////
// CAnimObjectManager
//////////////////////////////////////////////////////////////////////////

// Static initialisation
CAnimDataManager CAnimDataManager::ms_instance;

void CAnimDataManager::InitSession()
{
	sysMemUseMemoryBucket b(MEMBUCKET_ANIMATION);

	CGameDataMgr::Init("Anim", "platform:/data/metadata/anim.dat", ANIMDATA_SCHEMA_VERSION);
}

void CAnimDataManager::ShutdownSession()
{
	CGameDataMgr::Shutdown();
}
