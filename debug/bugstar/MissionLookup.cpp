#include <string.h>

#include "debug/BugstarIntegration.h"
#include "MissionLookup.h"
#include "system/filemgr.h"
#include "parser/manager.h"						// XML
#include "parser/tree.h"
#include "string/string.h"

#if BUGSTAR_INTEGRATION_ACTIVE

atMap<atHashValue, CMissionLookup::MissionDef> CMissionLookup::ms_missionTable;

void CMissionLookup::Init()
{
	parTree* pTree = CBugstarIntegration::GetSavedBugstarRequestList().LoadBugstarRequest("missions.xml");
	if(pTree)
	{
		BuildMap(pTree);
		delete pTree;
	}
	// request missions from bugstar
	CBugstarIntegration::GetSavedBugstarRequestList().Get("missions.xml", BUGSTAR_GTA_PROJECT_PATH "Missions", 24*60, 256*1024);
}

void CMissionLookup::BuildMap(parTree* pTree)
{
	parTreeNode* root = pTree->GetRoot();
	parTreeNode::ChildNodeIterator i = root->BeginChildren();

	for(; i != root->EndChildren(); ++i) {

		parTreeNode* idNode = (*i)->FindFromXPath("id");
		parTreeNode* nameNode = (*i)->FindFromXPath("scriptName");
		parTreeNode* missionIdNode = (*i)->FindFromXPath("missionId");

		if(nameNode && missionIdNode && idNode)
		{
			ms_missionTable[nameNode->GetData()].bugstarId = atoi((char*)idNode->GetData());
			safecpy(ms_missionTable[nameNode->GetData()].id, missionIdNode->GetData(), 8);
		}
	}

}

const char* CMissionLookup::GetMissionOwner(const char* )
{	
	// *Default Levels*
	return "6";
}

const char* CMissionLookup::GetMissionId(const char* missionTag)
{	
	char missionFileName[64];
	formatf(missionFileName, "%s.sc", missionTag);

	const MissionDef* pDef = ms_missionTable.Access(missionFileName);
	if(pDef)
		return pDef->id;
	return "";
}

int CMissionLookup::GetBugstarId(const char* missionTag)
{	
	char missionFileName[64];
	formatf(missionFileName, "%s.sc", missionTag);

	const MissionDef* pDef = ms_missionTable.Access(missionFileName);
	if(pDef)
		return pDef->bugstarId;
	return 0;
}

#endif //BUGSTAR_INTEGRATION_ACTIVE

