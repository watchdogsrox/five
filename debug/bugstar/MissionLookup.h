

#ifndef INC_MISSION_LOOKUP_H_
#define INC_MISSION_LOOKUP_H_

#include "debug\debug.h"
#if BUGSTAR_INTEGRATION_ACTIVE

#include "atl\map.h"

class CMissionLookup
{
public:
	static void Init();

	static void BuildMap(class parTree* pTree);
	static const char* GetMissionOwner(const char* missionTag);
	static const char* GetMissionId(const char* missionTag);
	static int GetBugstarId(const char* missionTag);

protected:
	static bool findMission(const char* missionTag);

	// assume mission tags aren't more than 7 chars
	struct MissionDef
	{
		char id[8];
		int bugstarId;
	};

	static atMap<atHashValue, MissionDef> ms_missionTable;
};

#endif // BUGSTAR_INTEGRATION_ACTIVE
#endif // INC_MISSION_LOOKUP_H_
