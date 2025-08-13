//
// name:        NetworkSoakTests.cpp
// description: Network soak testing functionality - used for automating testing of the network game
// written by:  Daniel Yelland
//

#if !__FINAL

#include "network/Debug/NetworkSoakTests.h"

#include "network/NetworkInterface.h"

#include "script/script.h"
#include "script/streamedscripts.h"
#include "streaming/streaming.h"

PARAM(netsoaktest, "Runs an automated test of the network game using the specified script");
PARAM(netsoaktestparams, "Specifies a comma separated list of parameter for the soak test script");

bool                             CNetworkSoakTests::m_runTest    = false;
int                              CNetworkSoakTests::ms_NumParams = 0;
CNetworkSoakTests::SoakTestState CNetworkSoakTests::m_State      = CNetworkSoakTests::E_NONE;
scrThreadId                      CNetworkSoakTests::m_ScriptID   = THREAD_INVALID;

char CNetworkSoakTests::ms_ScriptName[CNetworkSoakTests::MAX_SCRIPT_NAME];
int  CNetworkSoakTests::ms_Params    [CNetworkSoakTests::MAX_PARAMS];

void CNetworkSoakTests::Init()
{
    const char *scriptName = 0;

    m_runTest = PARAM_netsoaktest.Get(scriptName);

    if(m_runTest && scriptName)
    {
        safecpy(ms_ScriptName, scriptName, MAX_SCRIPT_NAME);

		//taken care of by -netAutoJoin=match and -netGameMode=6 -- properly handles transitions
        //NetworkInterface::GetLobby().Start(CNetworkLobby::STARTMODE_JOIN_MATCH);

        m_State = E_WAITING_TO_JOIN_SESSION;

        //parse the soak test parameters
        ms_NumParams = PARAM_netsoaktestparams.GetArray(ms_Params, MAX_PARAMS);
    }
}

void CNetworkSoakTests::Update()
{
    if(m_runTest)
    {
        switch(m_State)
        {
        case E_WAITING_TO_JOIN_SESSION:
            {
                if(NetworkInterface::IsInSession())
                {
                    m_State = E_LAUNCH_SOAK_TEST_SCRIPT;
                }
            }
            break;
        case E_LAUNCH_SOAK_TEST_SCRIPT:
            {
                if(gnetVerifyf(m_ScriptID == 0, "There is already a soak test script running!"))
                {
                    const strLocalIndex scriptID = g_StreamedScripts.FindSlot(ms_ScriptName);

                    if(gnetVerifyf(scriptID != -1, "Soak test script doesn't exist: %s", ms_ScriptName))
                    {
                        g_StreamedScripts.StreamingRequest(scriptID, STRFLAG_PRIORITY_LOAD|STRFLAG_MISSION_REQUIRED);

			            CStreaming::LoadAllRequestedObjects();

                        if(gnetVerifyf(g_StreamedScripts.HasObjectLoaded(scriptID), "Failed to stream in soak test script: %s", ms_ScriptName))
                        {
			                m_ScriptID = CTheScripts::GtaStartNewThreadOverride(ms_ScriptName, NULL, 0, GtaThread::GetSoakTestStackSize());

                            if(gnetVerifyf(m_ScriptID != 0, "Failed to start soak test script: %s", ms_ScriptName))
			                {
                                gnetDebug1("Started running soak test script: %s", ms_ScriptName);
                                m_State = E_IDLE;
                            }
                        }
                    }
                }
            }
            break;
        case E_IDLE:
            {
            }
            break;
        default:
            break;
        }
    }
}

int CNetworkSoakTests::GetNumParameters()
{
    return ms_NumParams;
}

int CNetworkSoakTests::GetParameter(unsigned index)
{
    int param = 0;

    if(gnetVerifyf(index < (unsigned)ms_NumParams, "Parameter %d has not been specified on the commandline (use -netsoaktestparams=<paramlist>)", index))
    {
        param = ms_Params[index];
    }

    return param;
}

#endif // !__FINAL
