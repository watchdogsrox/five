//
// name:        NetworkSoakTests.h
// description: Network soak testing functionality - used for automating testing of the network game
// written by:  Daniel Yelland
//

#ifndef NETWORK_SOAKTESTS_H
#define NETWORK_SOAKTESTS_H

#if !__FINAL

#include "script/gta_thread.h"

class CNetworkSoakTests
{
public:

    static void Init();
    static void Update();

    static int GetNumParameters();
    static int GetParameter(unsigned index);

private:

    enum SoakTestState
    {
        E_NONE,
        E_WAITING_TO_JOIN_SESSION,
        E_LAUNCH_SOAK_TEST_SCRIPT,
        E_IDLE,
    };

    static const unsigned MAX_SCRIPT_NAME = 128;
    static const unsigned MAX_PARAMS      = 10;

    static bool          m_runTest;
    static int           ms_NumParams;
    static char          ms_ScriptName[MAX_SCRIPT_NAME];
    static int           ms_Params[MAX_PARAMS];
    static SoakTestState m_State;
    static scrThreadId   m_ScriptID;
};

#endif // !__FINAL

#endif  // NETWORK_SOAKTESTS_H
