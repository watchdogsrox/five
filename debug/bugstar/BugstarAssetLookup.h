

#ifndef INC_BUGSTAR_ASSET_LOOKUP_H_
#define INC_BUGSTAR_ASSET_LOOKUP_H_

#include "BugstarIntegrationSwitch.h"

#if BUGSTAR_INTEGRATION_ACTIVE

#include "atl\map.h"

namespace rage {

	class parTree;
}

class CBugstarAssetLookup
{
public:
	void Init(const char* pRestAddr, const char* pCacheFile, const char* assetId);
	void BuildMap(parTree* pTree, const char* assetId);
	u32 GetBugstarId(const char* name) const;

protected:
	atMap<atHashValue, u32> ms_idTable;
};



#endif // BUGSTAR_INTEGRATION_ACTIVE
#endif // INC_BUILD_LOOKUP_H_
