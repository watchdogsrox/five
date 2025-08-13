//
// name:        NetworkDebugMsgs.cpp
// description: Messages used for network debugging
// written by:  John Gurney
//


#include "network/Debug/NetworkDebugMsgs.h"


namespace rage
{
#if !__FINAL
	NET_MESSAGE_IMPL( msgDebugTimeScale );
	NET_MESSAGE_IMPL( msgDebugTimePause );
#endif // __FINAL
#if __BANK
	NET_MESSAGE_IMPL( msgDebugNoTimeouts );
	NET_MESSAGE_IMPL( msgDebugAddFailMark );
	NET_MESSAGE_IMPL( msgSyncDRDisplay );
#endif // __BANK
}

