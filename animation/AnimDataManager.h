#ifndef ANIM_OBJECT_MANAGER_H
#define ANIM_OBJECT_MANAGER_H

// Game headers
#include "system/GameDataMgr.h"

// Game headers
#include "Animation/AnimData.h"

//////////////////////////////////////////////////////////////////////////
// CAnimObjectManager
//////////////////////////////////////////////////////////////////////////

class CAnimDataManager : public CGameDataMgr
{
	friend class CAnimGroupManager;
	friend class CAnimGroup;

public:

	// Access to the static manager
	static CAnimDataManager& GetInstance() { return ms_instance; }

	void InitSession();
	void ShutdownSession();
	
private:
		
	// Static manager object
	static CAnimDataManager ms_instance;
};

#endif // ANIM_OBJECT_MANAGER_H
