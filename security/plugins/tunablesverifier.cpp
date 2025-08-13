#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "file/file_config.h"
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecgameinterface.h"
#include "script/thread.h"
#include "tunablesverifier.h"
#include "system/xtl.h"
using namespace rage;

#if RAGE_SEC_TASK_TUNABLES_VERIFIER

RAGE_DEFINE_SUBCHANNEL(net, tunables_verifier, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_tunables_verifier

#include "wolfssl/wolfcrypt/sha.h"

#if RSG_FINAL
#define TUNABLES_VERIFIER_POLLING_MS 60*1000
#else
#define TUNABLES_VERIFIER_POLLING_MS 5*1000
#endif

#define TUNABLES_VERIFIER_ID  0xF0F7C843

static unsigned int sm_tvTrashValue	= 0;
static unsigned char *sm_Hash;

void TunablesVerifierPlugin_SetHash(unsigned char *in)
{
	//@@: location TUNABLESVERIFIERPLUGIN_SETHASH
	if(!sm_Hash) {
		gnetDebug3("0x%08X - Allocating sm_Hash", TUNABLES_VERIFIER_ID);
		sm_Hash = rage_new unsigned char[SHA_DIGEST_SIZE];
	}
	sysMemCpy(sm_Hash, in, SHA_DIGEST_SIZE);
	sysMemSet(in, 0, SHA_DIGEST_SIZE);
}

bool TunablesVerifierPlugin_Destroy()
{
	//@@: location TUNABLESVERIFIERPLUGIN_DESTROY
	gnetDebug3("0x%08X - Deleting sm_Hash", TUNABLES_VERIFIER_ID);
	if(sm_Hash)
		delete [] sm_Hash;
	return true;
}

bool TunablesVerifierPlugin_Work()
{
	//@@: location TUNABLESVERIFIERPLUGIN_WORK_ENTRY
	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress();

	//@@: range TUNABLESVERIFIERPLUGIN_WORK_BODY {
	if(isMultiplayer && sm_Hash != nullptr) 
	{
		gnetDebug3("0x%08X - Entry /  MP Session", TUNABLES_VERIFIER_ID);
		Sha tSha;
		//@@: location TUNABLESVERIFIERPLUGIN_WORK_INIT_SHA
		wc_InitSha(&tSha);
		// Get the version number
		int versionNumber = atoi(CDebug::GetVersionNumber());
		
		// Append our version number
		wc_ShaUpdate(&tSha,(unsigned char*)&versionNumber, sizeof(versionNumber));

		// Create a local MemoryCheck so we can iterate through them
		MemoryCheck itr;
		//@@: location TUNABLESVERIFIERPLUGIN_WORK_GET_NUM_CHECKS
		int numChecks = Tunables::GetInstance().GetNumMemoryChecks();		
		// Fetch the memory check
		for(int i = 0; i < numChecks; i++)
		{
			Tunables::GetInstance().GetMemoryCheck(i, itr);
			unsigned int x = itr.GetVersionAndType();
			u32 xorValue = itr.GetValue() ^ MemoryCheck::GetMagicXorValue();
			// Hash our fields
			wc_ShaUpdate(&tSha,(unsigned char*)&x, sizeof(x));
			x = itr.GetAddressStart();
			wc_ShaUpdate(&tSha,(unsigned char*)&x, sizeof(x));
			x = itr.GetSize();
			wc_ShaUpdate(&tSha,(unsigned char*)&x, sizeof(x));
			x = itr.GetValue();
			wc_ShaUpdate(&tSha,(unsigned char*)&x, sizeof(x));
			x = (itr.GetFlags() ^ xorValue) & 0xFFFFFF;
			wc_ShaUpdate(&tSha,(unsigned char*)&x, sizeof(x));
		}
		// Append the number of checks that we have
		//@@: location TUNABLESVERIFIERPLUGIN_WORK_UPDATE_NUM_CHECKS
		wc_ShaUpdate(&tSha,(unsigned char*)&numChecks, sizeof(numChecks));

		// Finalize our hash
		unsigned char shaHash[SHA_DIGEST_SIZE] = {0};
		wc_ShaFinal(&tSha,shaHash);

		//@@: location TUNABLESVERIFIERPLUGIN_WORK_COMPARE_DIGESTS
		if(memcmp(shaHash, sm_Hash, SHA_DIGEST_SIZE) == 0)
		{
			gnetDebug3("0x%08X - Verified the integrity of the bonus array successfully.", TUNABLES_VERIFIER_ID);
		}
		else
		{
			gnetAssertf(false, "0x%08X - FAILED to verify the integrity of the bonus array.", TUNABLES_VERIFIER_ID);
				RageSecPluginGameReactionObject obj(
					REACT_GAMESERVER, 
					TUNABLES_VERIFIER_ID, 
					0x0D775098,
					0xBB8C514E,
					0x7B7B9E03);
				rageSecGamePluginManager::GetRageSecGameReactionObjectQueue()->Push(obj);
				RageSecInduceStreamerCrash();
		}
	}
	//@@: } TUNABLESVERIFIERPLUGIN_WORK_BODY
	return true;
}

bool TunablesVerifierPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", TUNABLES_VERIFIER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location TUNABLESVERIFIERPLUGIN_ONSUCCESS
	sm_tvTrashValue	   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}
bool TunablesVerifierPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", TUNABLES_VERIFIER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location TUNABLESVERIFIERPLUGIN_ONFAILURE
	sm_tvTrashValue	   *= sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool TunablesVerifierPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;
	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= TUNABLES_VERIFIER_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= NULL;
	rs.Configure		= NULL;
	rs.Destroy			= &TunablesVerifierPlugin_Destroy;
	rs.DoWork			= &TunablesVerifierPlugin_Work;
	rs.OnSuccess		= &TunablesVerifierPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &TunablesVerifierPlugin_OnFailure;
	
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	
	rageSecPluginManager::RegisterPluginFunction(&rs,TUNABLES_VERIFIER_ID);
	return true;
}
#endif // RAGE_SEC_TASK_TUNABLES_VERIFIER

#endif // RAGE_SEC