#include "BugstarAssetLookup.h"
#include "debug/BugstarIntegration.h"
#include "parser/manager.h"						// XML
#include "parser/tree.h"

#if BUGSTAR_INTEGRATION_ACTIVE

void CBugstarAssetLookup::Init(const char* pRestAddr, const char* pCacheFile, const char* assetId)
{
	USE_DEBUG_MEMORY();
	
	parTree* pTree = CBugstarIntegration::GetSavedBugstarRequestList().LoadBugstarRequest(pCacheFile);
	if(pTree)
	{
		BuildMap(pTree, assetId);
		delete pTree;
	}

	// request builds from bugstar
	CBugstarIntegration::GetSavedBugstarRequestList().Get(pCacheFile, pRestAddr, 24*60, 32*1024);
}

void CBugstarAssetLookup::BuildMap(parTree* pTree, const char* assetId)
{
	parTreeNode* root = pTree->GetRoot();
	parTreeNode::ChildNodeIterator i = root->BeginChildren();

	for(; i != root->EndChildren(); ++i) {

		parTreeNode* idNode = (*i)->FindFromXPath("id");
		parTreeNode* buildNameNode = (*i)->FindFromXPath(assetId);

		if(buildNameNode && idNode)
		{
			ms_idTable[buildNameNode->GetData()] = atoi((char*)idNode->GetData());
		}
	}

}

u32 CBugstarAssetLookup::GetBugstarId(const char* name) const
{	
	const u32* id = ms_idTable.Access(name);
	if(id)
		return *id;
	return 0;
}

#endif //BUGSTAR_INTEGRATION_ACTIVE

