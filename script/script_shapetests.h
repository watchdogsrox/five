// 
// script/script_shapetests.h 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef SCRIPT_SCRIPT_SHAPETESTS_H
#define SCRIPT_SCRIPT_SHAPETESTS_H

// Rage headers
#include "atl/queue.h"
#include "fwtl/Pool.h"
#include "entity/extensiblebase.h"

//Game headers
#include "physics/WorldProbe/worldprobe.h"

// ***************************************************************************
//	CScriptShapeTestManager

// This makes use of WorldProbe
// CScriptShapeTestRequests exist in a pool
// Requests must be polled for every frame, or they will be destroyed

#define SCRIPTSHAPETESTMANAGER_REQUESTPOOLSIZE  (10)		//Total number of requests that can be in the system

#if WORLD_PROBE_DEBUG
// This macro allows recording the context from which a test was submitted for labeling shape tests
// when debug drawing is enabled.
#define CreateShapeTestRequest(...) DebugCreateShapeTestRequest(__FILE__, __LINE__, __VA_ARGS__)
#endif // WORLD_PROBE_DEBUG


typedef int ScriptShapeTestGuid;

//Corresponds to an enum with the same name in commands_shapetest.sch
enum SHAPETEST_STATUS
{
	SHAPETEST_STATUS_NONEXISTENT,		// shape test results are discarded after some time has elapsed, or requests may be rejected due to hitting the pool limit
	SHAPETEST_STATUS_RESULTS_NOTREADY,	// Not ready yet: try again next frame
	SHAPETEST_STATUS_RESULTS_READY		// The result is ready and the results have been returned to you
};

class CScriptShapeTestManager
{
public:

	//Interface for script commands
#if WORLD_PROBE_DEBUG
	static ScriptShapeTestGuid	DebugCreateShapeTestRequest(const char *strCodeFile, u32 nCodeLine, WorldProbe::CShapeTestDesc& shapeTestParams, bool bSynchronousTest=false);
#else
	static ScriptShapeTestGuid	CreateShapeTestRequest(WorldProbe::CShapeTestDesc& shapeTestParams, bool bSynchronousTest=false); //Returns guid. Don't set a results structure. It is done internally
#endif


	static SHAPETEST_STATUS		GetShapeTestResult(ScriptShapeTestGuid shapeTestGuid, WorldProbe::CShapeTestHitPoint& outNearestResultTemporary); //This clears the pool entry too
	static void					AbortShapeTestRequest(ScriptShapeTestGuid shapeTestGuid);

	//Interface for gameSkeleton
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update(); //Loop through pool deleting un-polled requests
};

#endif //SCRIPT_SCRIPT_SHAPETESTS_H
