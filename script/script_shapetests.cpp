// 
// script/script_shapetests.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

// Rage headers
#include "fwsys/gameSkeleton.h"
#include "fwscript/scriptguid.h"

// Game headers
#include "script_shapetests.h"

//Only to be used by CScriptShapeTestManager
struct CScriptShapeTestResults : public fwExtensibleBase
{
	FW_REGISTER_CLASS_POOL(CScriptShapeTestResults);

	DECLARE_RTTI_DERIVED_CLASS(CScriptShapeTestResults, fwExtensibleBase);
public:
	CScriptShapeTestResults();
	~CScriptShapeTestResults();

	//Results (contains the handle from WorldProbe)
	WorldProbe::CShapeTestResults			m_results;

	//Progress tracking
	bool									m_polledThisFrame; //If this request is not polled every frame, we delete it
};

FW_INSTANTIATE_CLASS_POOL(CScriptShapeTestResults, SCRIPTSHAPETESTMANAGER_REQUESTPOOLSIZE, atHashString("ScriptShapeTestResult",0x302e7b2f));
INSTANTIATE_RTTI_CLASS(CScriptShapeTestResults,0x6f37e9db);

CScriptShapeTestResults::CScriptShapeTestResults()
	: m_results(), m_polledThisFrame(true)
{
}

CScriptShapeTestResults::~CScriptShapeTestResults()
{
    scrDebugf2( "CScriptShapeTestResults::~CScriptShapeTestResults(): %p", this );

	// Don't want to leak script resources.
	fwScriptGuid::DeleteGuid(*this);
}

//CScriptShapeTestManager
void CScriptShapeTestManager::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		CScriptShapeTestResults::InitPool(MEMBUCKET_GAMEPLAY);
	}
	else if(initMode == INIT_SESSION)
	{
		
	}
}

void CScriptShapeTestManager::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		CScriptShapeTestResults::ShutdownPool();
	}
	else if(shutdownMode == SHUTDOWN_SESSION)
	{
		//Deleting a queued request in the pool will remove it from the queue too
		CScriptShapeTestResults::GetPool()->DeleteAll();
	}
}

#if WORLD_PROBE_DEBUG
ScriptShapeTestGuid	CScriptShapeTestManager::DebugCreateShapeTestRequest(const char *strCodeFile, u32 nCodeLine, WorldProbe::CShapeTestDesc& shapeTestParams, bool bSynchronousTest)
#else
ScriptShapeTestGuid CScriptShapeTestManager::CreateShapeTestRequest(WorldProbe::CShapeTestDesc& shapeTestParams, bool bSynchronousTest)
#endif
{
	if( CScriptShapeTestResults::GetPool()->GetNoOfFreeSpaces() == 0 ) 
		return 0;

	CScriptShapeTestResults* pRequest = rage_new CScriptShapeTestResults();
	int guid = fwScriptGuid::CreateGuid(*pRequest); //If guid creation fails then the request will be cleaned up next frame due to being unpolled

	shapeTestParams.SetResultsStructure(&pRequest->m_results);

#if WORLD_PROBE_DEBUG
	WorldProbe::GetShapeTestManager()->DebugSubmitTest(strCodeFile, nCodeLine, shapeTestParams, bSynchronousTest ? WorldProbe::PERFORM_SYNCHRONOUS_TEST : WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
#else
	WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestParams, bSynchronousTest ? WorldProbe::PERFORM_SYNCHRONOUS_TEST : WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
#endif

	if( pRequest->m_results.GetResultsStatus() == WorldProbe::SUBMISSION_FAILED )
	{
		delete pRequest;
		return 0;
	}

	return guid;
}

SHAPETEST_STATUS CScriptShapeTestManager::GetShapeTestResult(ScriptShapeTestGuid shapeTestGuid, WorldProbe::CShapeTestHitPoint& outNearestResultTemporary)
{
	CScriptShapeTestResults* pRequest = fwScriptGuid::FromGuid<CScriptShapeTestResults>(shapeTestGuid);

    scrDebugf2( "CScriptShapeTestManager::GetShapeTestResult guid: %d Results: %p Status: %d", shapeTestGuid, pRequest, pRequest ? (int)pRequest->m_results.GetResultsStatus() : 0 );

	if(!pRequest) return SHAPETEST_STATUS_NONEXISTENT;
	
	switch( pRequest->m_results.GetResultsStatus() )
	{
		case WorldProbe::WAITING_ON_RESULTS:
			pRequest->m_polledThisFrame = true;
			return SHAPETEST_STATUS_RESULTS_NOTREADY;

		case WorldProbe::RESULTS_READY:
			outNearestResultTemporary.Copy( pRequest->m_results[0] ); //This may or may not be a hit
			delete pRequest;
			return SHAPETEST_STATUS_RESULTS_READY;

		case WorldProbe::TEST_NOT_SUBMITTED:
		case WorldProbe::SUBMISSION_FAILED:
		case WorldProbe::TEST_ABORTED:
            physicsAssertf( false, "Script-initiated shape test results in a bad state: %d", pRequest->m_results.GetResultsStatus() );
            delete pRequest;
            return SHAPETEST_STATUS_NONEXISTENT;
		default:
            // B*6605259 - this code crashes the game sometimes as it appears to call the destructor for a vehicle
            // it does seem that this is possible without any bad stuff appearing in the logs 
            // but it does so I've added some more logging
            if( fwScriptGuid::IsType<fwEntity>( shapeTestGuid ) )
            {
                fwEntity* pEntity = fwScriptGuid::FromGuid<fwEntity>( shapeTestGuid );
                const char* pModelName = "";
                pModelName = pEntity->GetModelName();
                Displayf( "CScriptShapeTestManager::GetShapeTestResult guid is an entity: %p %s",
                          pEntity, pModelName );
            }
            physicsAssertf( false, "Script-initiated shape test results in an invalid state: %d", pRequest->m_results.GetResultsStatus() );
            delete pRequest;

			return SHAPETEST_STATUS_NONEXISTENT;
	}
}

void CScriptShapeTestManager::AbortShapeTestRequest(ScriptShapeTestGuid shapeTestGuid)
{
	CScriptShapeTestResults* pRequest = fwScriptGuid::FromGuid<CScriptShapeTestResults>(shapeTestGuid);

    scrDebugf2( "CScriptShapeTestManager::AbortShapeTestRequest guid: %d Results: %p", shapeTestGuid, pRequest );

	if(!pRequest) return;

	//Delete it, releasing the pool slot. Its fwScriptGuid will be deleted with it
	//Its destructor will tell the shape test manager to abort it too
	delete pRequest;
}

//Loop through queue and pool updating the requests
void CScriptShapeTestManager::Update()
{
	//For all requests in the pool:
	//1. Destroy requests that the script has ignored this frame
	//2. Ask WorldProbe for results if we are waiting
	CScriptShapeTestResults::Pool* requestPool = CScriptShapeTestResults::GetPool();

	for(int i = 0 ; i < requestPool->GetSize(); i++)
	{
		CScriptShapeTestResults* pRequest = requestPool->GetSlot(i);

		if(!pRequest) continue;

		//The script didn't poll for the request this frame, so destroy it
		if(!pRequest->m_polledThisFrame)
		{
			delete pRequest;
			continue;
		}
		else
		{
			pRequest->m_polledThisFrame = false;
		}
	}
}
