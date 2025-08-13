#include "security/ragesecengine.h"

#if USE_RAGESEC

#include "file/file_config.h"
#include "net/status.h"
#include "net/task.h"
#include "security/ragesecplugin.h"
#include "security/ragesecenginetasks.h"
#include "security/ragesecgameinterface.h"
#include "security/obfuscatedtypes.h"
#include "script/thread.h"
#include "revolvingcheckerplugin.h"

using namespace rage;

RAGE_DEFINE_SUBCHANNEL(net, ragesec, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_ragesec

#if RAGE_SEC_TASK_REVOLVING_CHECKER

#define REVOLVING_CHECKER_POLLING_MS 1000*5
#define REVOLVING_CHECKER_ID  0x4592F40E

static u32 sm_rcTrashValue	= 0;

bool RevolvingCheckerPlugin_Create()
{
	gnetDebug3("0x%08X - Created", REVOLVING_CHECKER_ID);
	//@@: location REVOLVINGCHECKERPLUGIN_WORK_CREATE
	sm_rcTrashValue   += sysTimer::GetSystemMsTime();
	//sm_rcSelector.Set(fwRandom::GetRandomNumber16());
	return true;
}

bool RevolvingCheckerPlugin_Configure()
{
	gnetDebug3("0x%08X - Configure", REVOLVING_CHECKER_ID);
	//@@: location REVOLVINGCHECKERPLUGIN_WORK_CONFIGURE
	sm_rcTrashValue   -= sysTimer::GetSystemMsTime();
	return true;
}

#if RSG_PC
// This pragma is *intentional*; it allows the compiler not to optimize code
// that it would normally collapse. This collapsing would cause several
// of the Arxan guards firing at an individual location below to fire at a single
// time, which is undesirable.
#pragma optimize("", off)
#define REVOLVING_NUM_LOCATIONS 100
bool RevolvingCheckerPlugin_NestedWork(int a, int b, int c, int d)
{
	//@@: range REVOLVINGCHECKERPLUGIN_WORK_NESTED_BODY {
	int selector = d % REVOLVING_NUM_LOCATIONS;
#if !__NO_OUTPUT
	static int output_selector = 0;
	
	selector = output_selector;

	if(output_selector >= REVOLVING_NUM_LOCATIONS) {
		output_selector = 0;
	} else{
		output_selector++;
	}

#endif
	if(selector == 0x000) {
		//@@: location REVOLVING_LOCATION_000
		c=b|b;c=d*b;d=c+d;b=b|b;a=b*d;
	}
	if(selector == 0x001) {
		//@@: location REVOLVING_LOCATION_001
		a=a^a;c=d^c;b=b|d;c=d&c;d=d^a;
	}
	if(selector == 0x002) {
		//@@: location REVOLVING_LOCATION_002
		a=b*b;c=d+c;a=d*b;d=c^d;d=c*d;
	}
	if(selector == 0x003) {
		//@@: location REVOLVING_LOCATION_003
		b=d*d;a=d*c;b=a&d;c=b|b;d=d&c;
	}
	if(selector == 0x004) {
		//@@: location REVOLVING_LOCATION_004
		a=d*c;d=c*d;d=c&b;b=a|d;d=b&d;
	}
	if(selector == 0x005) {
		//@@: location REVOLVING_LOCATION_005
		d=b+d;c=d^a;c=d^a;d=d^b;a=c*b;
	}
	if(selector == 0x006) {
		//@@: location REVOLVING_LOCATION_006
		c=b^b;b=b+a;c=d*c;a=c*b;d=a+a;
	}
	if(selector == 0x007) {
		//@@: location REVOLVING_LOCATION_007
		a=b+b;d=c&c;b=a&d;a=c&d;a=b*c;
	}
	if(selector == 0x008) {
		//@@: location REVOLVING_LOCATION_008
		d=a*b;c=a^b;c=a+a;a=a|c;a=b^b;
	}
	if(selector == 0x009) {
		//@@: location REVOLVING_LOCATION_009
		c=c&d;b=d^b;c=b*d;d=d+d;a=b*c;
	}
	if(selector == 0x00a) {
		//@@: location REVOLVING_LOCATION_010
		b=a+d;c=b^d;a=d^c;c=b*c;d=d*d;
	}
	if(selector == 0x00b) {
		//@@: location REVOLVING_LOCATION_011
		a=c+c;c=c^a;d=d&d;a=d+d;d=a^d;
	}
	if(selector == 0x00c) {
		//@@: location REVOLVING_LOCATION_012
		a=b&a;c=c*b;a=a&a;c=c*a;a=a&d;
	}
	if(selector == 0x00d) {
		//@@: location REVOLVING_LOCATION_013
		b=a&a;b=c+c;a=a^c;a=b|b;c=b|d;
	}
	if(selector == 0x00e) {
		//@@: location REVOLVING_LOCATION_014
		d=b*b;b=a+d;d=c|c;c=b&d;d=d+b;
	}
	if(selector == 0x00f) {
		//@@: location REVOLVING_LOCATION_015
		c=b|c;a=c&d;d=d+c;b=b+d;b=c^a;
	}
	if(selector == 0x010) {
		//@@: location REVOLVING_LOCATION_016
		a=b&c;d=c|d;a=c|a;a=c|d;c=d|a;
	}
	if(selector == 0x011) {
		//@@: location REVOLVING_LOCATION_017
		d=a&b;a=c+b;d=b^b;b=c&c;c=c*c;
	}
	if(selector == 0x012) {
		//@@: location REVOLVING_LOCATION_018
		c=d^c;c=a|d;b=d|b;d=d+d;c=a*c;
	}
	if(selector == 0x013) {
		//@@: location REVOLVING_LOCATION_019
		c=a|b;c=c&a;d=a^c;c=c*c;d=c|d;
	}
	if(selector == 0x014) {
		//@@: location REVOLVING_LOCATION_020
		d=c*b;c=c+a;c=d+a;a=a&d;d=a+c;
	}
	if(selector == 0x015) {
		//@@: location REVOLVING_LOCATION_021
		a=a|c;d=d^b;b=a*c;b=c^d;a=a&d;
	}
	if(selector == 0x016) {
		//@@: location REVOLVING_LOCATION_022
		b=b*d;d=d^d;a=c^d;b=b*d;d=c^d;
	}
	if(selector == 0x017) {
		//@@: location REVOLVING_LOCATION_023
		a=d&d;c=a^c;b=b+d;d=d|c;c=d^a;
	}
	if(selector == 0x018) {
		//@@: location REVOLVING_LOCATION_024
		b=a^c;b=a|d;a=a&d;c=b&d;d=d&c;
	}
	if(selector == 0x019) {
		//@@: location REVOLVING_LOCATION_025
		c=d*b;d=d|a;c=b&c;a=d&b;b=a*b;
	}
	if(selector == 0x01a) {
		//@@: location REVOLVING_LOCATION_026
		d=b*b;b=a|c;a=a*b;c=b^c;d=a&d;
	}
	if(selector == 0x01b) {
		//@@: location REVOLVING_LOCATION_027
		b=d+a;c=a^c;a=d*a;d=a|b;a=b^d;
	}
	if(selector == 0x01c) {
		//@@: location REVOLVING_LOCATION_028
		b=d&b;a=a*a;a=a^a;d=b|d;b=d|b;
	}
	if(selector == 0x01d) {
		//@@: location REVOLVING_LOCATION_029
		b=b|b;b=d*a;d=c^b;c=b&d;a=a*d;
	}
	if(selector == 0x01e) {
		//@@: location REVOLVING_LOCATION_030
		d=d+c;b=c|d;a=a+d;a=d*d;a=b&d;
	}
	if(selector == 0x01f) {
		//@@: location REVOLVING_LOCATION_031
		b=a|d;d=a^a;c=d+c;c=c+d;a=d^c;
	}
	if(selector == 0x020) {
		//@@: location REVOLVING_LOCATION_032
		d=a&c;a=c|a;d=b|a;a=a|b;b=c+b;
	}
	if(selector == 0x021) {
		//@@: location REVOLVING_LOCATION_033
		c=c*d;c=c|d;a=b*c;b=d*a;d=a*a;
	}
	if(selector == 0x022) {
		//@@: location REVOLVING_LOCATION_034
		d=a&a;b=b|d;a=a^c;c=c*d;c=d^d;
	}
	if(selector == 0x023) {
		//@@: location REVOLVING_LOCATION_035
		b=a|a;a=a*a;a=c|c;b=a+a;c=b&d;
	}
	if(selector == 0x024) {
		//@@: location REVOLVING_LOCATION_036
		a=d&b;a=a&b;b=b^b;c=c^c;b=b&c;
	}
	if(selector == 0x025) {
		//@@: location REVOLVING_LOCATION_037
		c=c^b;b=d&c;d=d|c;a=d*b;b=a|a;
	}
	if(selector == 0x026) {
		//@@: location REVOLVING_LOCATION_038
		a=b&b;d=d*c;c=d|d;a=c|a;a=d*c;
	}
	if(selector == 0x027) {
		//@@: location REVOLVING_LOCATION_039
		c=a|b;b=a+b;b=c+c;a=d+c;b=c+a;
	}
	if(selector == 0x028) {
		//@@: location REVOLVING_LOCATION_040
		b=b&c;c=d&c;d=d+d;c=a+b;d=a&c;
	}
	if(selector == 0x029) {
		//@@: location REVOLVING_LOCATION_041
		d=b|c;b=d+d;d=a^a;d=a|d;d=c^c;
	}
	if(selector == 0x02a) {
		//@@: location REVOLVING_LOCATION_042
		d=d|c;d=b^c;c=b+d;d=b^d;b=a^d;
	}
	if(selector == 0x02b) {
		//@@: location REVOLVING_LOCATION_043
		c=a*d;d=a^b;d=a*d;a=a*a;b=d^b;
	}
	if(selector == 0x02c) {
		//@@: location REVOLVING_LOCATION_044
		c=a&d;c=d&d;b=b|b;b=b&a;c=b|a;
	}
	if(selector == 0x02d) {
		//@@: location REVOLVING_LOCATION_045
		a=a&c;b=c+b;a=c*b;c=c^d;a=d&c;
	}
	if(selector == 0x02e) {
		//@@: location REVOLVING_LOCATION_046
		c=c^c;b=d^c;c=b*c;d=a+d;a=a^d;
	}
	if(selector == 0x02f) {
		//@@: location REVOLVING_LOCATION_047
		b=d*d;d=a^d;a=b|d;d=b^a;b=d+d;
	}
	if(selector == 0x030) {
		//@@: location REVOLVING_LOCATION_048
		c=a&d;a=b*b;a=a+d;a=d*a;b=a|c;
	}
	if(selector == 0x031) {
		//@@: location REVOLVING_LOCATION_049
		b=d&b;a=c+b;c=b&c;b=c&b;a=b*a;
	}
	if(selector == 0x032) {
		//@@: location REVOLVING_LOCATION_050
		c=c^d;a=a*a;a=c&c;b=b&b;b=b|b;
	}
	if(selector == 0x033) {
		//@@: location REVOLVING_LOCATION_051
		b=a+c;a=c&c;d=a*c;a=c+b;a=c|a;
	}
	if(selector == 0x034) {
		//@@: location REVOLVING_LOCATION_052
		b=c^c;d=b*a;d=b^a;a=a&b;b=b+c;
	}
	if(selector == 0x035) {
		//@@: location REVOLVING_LOCATION_053
		d=c|c;d=a|b;d=b&c;a=a*d;c=c|d;
	}
	if(selector == 0x036) {
		//@@: location REVOLVING_LOCATION_054
		b=b*d;b=c&a;c=c|a;b=c|a;c=a+c;
	}
	if(selector == 0x037) {
		//@@: location REVOLVING_LOCATION_055
		c=d^d;a=a|a;d=d+b;d=c|b;d=d^a;
	}
	if(selector == 0x038) {
		//@@: location REVOLVING_LOCATION_056
		c=c|b;b=c*d;b=d+a;a=b*b;d=a&b;
	}
	if(selector == 0x039) {
		//@@: location REVOLVING_LOCATION_057
		a=b&c;d=c^b;b=a&b;d=d+a;a=d&c;
	}
	if(selector == 0x03a) {
		//@@: location REVOLVING_LOCATION_058
		b=b+d;c=c&b;d=a+a;b=b*b;a=a^d;
	}
	if(selector == 0x03b) {
		//@@: location REVOLVING_LOCATION_059
		d=d^c;a=a*c;b=a|a;b=b&a;b=a|c;
	}
	if(selector == 0x03c) {
		//@@: location REVOLVING_LOCATION_060
		b=c^a;b=d&b;d=a|a;b=b&b;d=c&c;
	}
	if(selector == 0x03d) {
		//@@: location REVOLVING_LOCATION_061
		b=c+b;c=c^d;a=d&b;d=c*d;c=d|a;
	}
	if(selector == 0x03e) {
		//@@: location REVOLVING_LOCATION_062
		a=d+c;a=c&d;a=b|b;b=d*d;a=b*d;
	}
	if(selector == 0x03f) {
		//@@: location REVOLVING_LOCATION_063
		d=a*b;a=c&b;b=d+b;d=b+b;d=c&d;
	}
	if(selector == 0x040) {
		//@@: location REVOLVING_LOCATION_064
		d=d|d;c=c|b;a=c^d;b=a*c;d=b&d;
	}
	if(selector == 0x041) {
		//@@: location REVOLVING_LOCATION_065
		b=c&a;c=a+a;a=c|c;d=c|c;a=b&a;
	}
	if(selector == 0x042) {
		//@@: location REVOLVING_LOCATION_066
		d=c*c;b=d+a;c=d*c;b=a^b;d=d&c;
	}
	if(selector == 0x043) {
		//@@: location REVOLVING_LOCATION_067
		d=a&c;d=d|a;d=a+a;b=d^d;d=c*c;
	}
	if(selector == 0x044) {
		//@@: location REVOLVING_LOCATION_068
		d=b+b;d=a^b;b=b|c;d=c+c;a=b+d;
	}
	if(selector == 0x045) {
		//@@: location REVOLVING_LOCATION_069
		b=d^a;a=d*d;d=d&b;a=a+a;d=c|a;
	}
	if(selector == 0x046) {
		//@@: location REVOLVING_LOCATION_070
		b=a^a;a=c*b;d=c|d;d=b|d;c=b+c;
	}
	if(selector == 0x047) {
		//@@: location REVOLVING_LOCATION_071
		c=d|b;b=b&b;b=c^c;d=b^b;d=b^d;
	}
	if(selector == 0x048) {
		//@@: location REVOLVING_LOCATION_072
		a=c^c;c=c|a;d=a+b;c=c&a;b=c|b;
	}
	if(selector == 0x049) {
		//@@: location REVOLVING_LOCATION_073
		c=b|b;d=b*d;c=b+d;a=a^b;a=d&b;
	}
	if(selector == 0x04a) {
		//@@: location REVOLVING_LOCATION_074
		d=b&b;a=a+a;c=b+a;c=a|b;d=b+d;
	}
	if(selector == 0x04b) {
		//@@: location REVOLVING_LOCATION_075
		b=b^b;b=b^c;a=d+d;b=c^a;d=d^a;
	}
	if(selector == 0x04c) {
		//@@: location REVOLVING_LOCATION_076
		c=b*a;d=d&a;a=c|d;c=c&b;b=d+c;
	}
	if(selector == 0x04d) {
		//@@: location REVOLVING_LOCATION_077
		d=a&a;d=c^b;a=c^b;b=b&b;c=a*c;
	}
	if(selector == 0x04e) {
		//@@: location REVOLVING_LOCATION_078
		d=c^b;a=a+a;b=b|b;b=d^d;a=a^a;
	}
	if(selector == 0x04f) {
		//@@: location REVOLVING_LOCATION_079
		d=a|a;c=b^d;b=b*d;b=d^a;c=b^a;
	}
	if(selector == 0x050) {
		//@@: location REVOLVING_LOCATION_080
		d=a|c;b=d^b;b=b*b;c=c^c;b=a+a;
	}
	if(selector == 0x051) {
		//@@: location REVOLVING_LOCATION_081
		d=d+c;b=c*d;d=b^b;b=b^c;c=b+a;
	}
	if(selector == 0x052) {
		//@@: location REVOLVING_LOCATION_082
		c=c^c;c=d^c;c=c^a;b=c&b;b=c^c;
	}
	if(selector == 0x053) {
		//@@: location REVOLVING_LOCATION_083
		b=a+a;c=d+b;a=c+b;c=d&d;d=d|c;
	}
	if(selector == 0x054) {
		//@@: location REVOLVING_LOCATION_084
		c=b^c;a=a|d;a=a*d;d=a|c;b=c&c;
	}
	if(selector == 0x055) {
		//@@: location REVOLVING_LOCATION_085
		b=b^c;d=a+d;d=a+d;d=a&c;a=d+d;
	}
	if(selector == 0x056) {
		//@@: location REVOLVING_LOCATION_086
		c=b&b;b=d^c;d=d^b;d=a+b;a=d|c;
	}
	if(selector == 0x057) {
		//@@: location REVOLVING_LOCATION_087
		d=d&d;c=d+b;d=b*c;a=a&d;d=c*a;
	}
	if(selector == 0x058) {
		//@@: location REVOLVING_LOCATION_088
		a=a*b;b=d|d;a=a+b;c=b^a;b=c*a;
	}
	if(selector == 0x059) {
		//@@: location REVOLVING_LOCATION_089
		a=a+a;c=c*d;d=a*b;c=b&d;a=c|c;
	}
	if(selector == 0x05a) {
		//@@: location REVOLVING_LOCATION_090
		b=c&d;d=b|a;d=b*b;d=b^c;a=a^b;
	}
	if(selector == 0x05b) {
		//@@: location REVOLVING_LOCATION_091
		b=b^d;a=c|a;b=c|a;b=c&a;d=d+b;
	}
	if(selector == 0x05c) {
		//@@: location REVOLVING_LOCATION_092
		d=a^d;d=d^a;a=d+d;b=b^a;d=d&d;
	}
	if(selector == 0x05d) {
		//@@: location REVOLVING_LOCATION_093
		d=c+c;d=d*a;a=d|a;c=a^a;b=a|d;
	}
	if(selector == 0x05e) {
		//@@: location REVOLVING_LOCATION_094
		b=a+c;c=c*a;a=d^c;a=c^d;b=a&b;
	}
	if(selector == 0x05f) {
		//@@: location REVOLVING_LOCATION_095
		d=b*b;b=c&c;c=d+d;d=d&b;a=b*b;
	}
	if(selector == 0x060) {
		//@@: location REVOLVING_LOCATION_096
		d=a&d;b=d+a;b=b*b;d=d&c;a=d^d;
	}
	if(selector == 0x061) {
		//@@: location REVOLVING_LOCATION_097
		b=a&d;c=b*a;d=a*a;c=a^d;b=c|b;
	}
	if(selector == 0x062) {
		//@@: location REVOLVING_LOCATION_098
		d=d^d;d=b&c;d=c^c;d=b*a;b=a|c;
	}
	if(selector == 0x063) {
		//@@: location REVOLVING_LOCATION_099
		c=b*d;a=b|c;d=a*b;a=c^d;b=a^d;
	}

	//@@: } REVOLVINGCHECKERPLUGIN_WORK_NESTED_BODY 
	return true;
}
#pragma optimize("", on)
#endif
bool RevolvingCheckerPlugin_Work()
{
    gnetDebug3("0x%08X - Work", REVOLVING_CHECKER_ID);

	//@@: location REVOLVINGCHECKERPLUGIN_WORK_ENTRY
	sm_rcTrashValue   -= sysTimer::GetSystemMsTime();
	// Create some garbage code that will provide instructions to hook
	// onto, in addition to making the invocation of each of the guards
	// somewhat pseudo-random
	gnetDebug3("0x%08X - Performing Calculatiosn", REVOLVING_CHECKER_ID);

	bool isMultiplayer = NetworkInterface::IsAnySessionActive() 
		&& NetworkInterface::IsGameInProgress() 
		&& !NetworkInterface::IsInSinglePlayerPrivateSession();
	// We only care about doing this in multiplayer
	float x1  = 0;
	float x0  = 0;
	float x2  = 0;
	if(isMultiplayer)
	{
		
		x0 = (float)fwRandom::GetRandomNumber();
		x1 = (float)fwRandom::GetRandomNumber();
		sm_rcTrashValue-=CountTrailingZeros((u32)x1);
		//@@: range REVOLVINGCHECKERPLUGIN_WORK_BODY {
		// I need to generate more garbage instructions, so I picked a bunch
		// of floating point arithmetic. This should safe guard any compiler optimizations,
		// as I think they were borking me in earlier game versions.
		x2 = (float)fwRandom::GetRandomNumber();
		x1 = (x0 + (((x2 * x1) - sm_rcTrashValue)/(x1 * x0)));		
		x2 = FPInvSqrtFast(FPAbs(x1)) * (float)fwRandom::GetRandomNumber();
		x1 = FPExpt(x0) * FPLog10(FPAbs(x2));
		bool isSameAsFloat = FloatSameSignAsFloat(&x1, &x2);
		bool isEqualsFloatInt = FloatEqualsFloatAsInt(&x1, (int)x0);
		sm_rcTrashValue+=FPPack(x1, (u32)fwRandom::GetRandomNumber(), (u32)fwRandom::GetRandomNumber());
		x0 = (x1 - (((x0 / x2) + sm_rcTrashValue)*(x2 * x0)));	
		x1 = FPExpt(x0) * FPLog10(FPAbs(x2));
		x2 = FPInvSqrtFast(FPAbs(x0)) * (float)fwRandom::GetRandomNumber();
		//@@: } REVOLVINGCHECKERPLUGIN_WORK_BODY 
		if(isSameAsFloat || isEqualsFloatInt)
		{
			x1 = (x0 - (((x0 / x2) + sm_rcTrashValue)*(x2 * x0)));	
		}
#if RSG_PC
		RevolvingCheckerPlugin_NestedWork((int)x0,(int)x1,(int)x2, sm_rcTrashValue);
#endif
		
	}

	//@@: location REVOLVINGCHECKERPLUGIN_WORK_EXIT
	sm_rcTrashValue = (int)x1 + (int)x2 - sm_rcTrashValue;

    return true;
}

bool RevolvingCheckerPlugin_OnSuccess()
{
	gnetDebug3("0x%08X - OnSuccess", REVOLVING_CHECKER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location REVOLVINGCHECKERPLUGIN_ONSUCCESS
	sm_rcTrashValue   -= sysTimer::GetSystemMsTime() + fwRandom::GetRandomNumber();
	return true;
}

bool RevolvingCheckerPlugin_OnFailure()
{
	gnetDebug3("0x%08X - OnFailure", REVOLVING_CHECKER_ID);
	// Make this different enough so the compiler doesn't collapse this with another function
	//@@: location REVOLVINGCHECKERPLUGIN_ONFAILURE
	sm_rcTrashValue   += sysTimer::GetSystemMsTime() - fwRandom::GetRandomNumber();
	return true;
}

bool RevolvingCheckerPlugin_Init()
{
	// Since there's no real initialization here necessary, we can just assign our
	// creation function, our destroy function, and just register the object 
	// accordingly

	RageSecPluginParameters rs;
	RageSecPluginExtendedInformation ei;
	ei.startTime		= sysTimer::GetSystemMsTime();
	ei.frequencyInMs	= REVOLVING_CHECKER_POLLING_MS;
	rs.version.major	= PLUGIN_VERSION_MAJOR;
	rs.version.minor	= PLUGIN_VERSION_MINOR;
	rs.type				= EXECUTE_PERIODIC;
	rs.extendedInfo		= ei;

	rs.Create			= &RevolvingCheckerPlugin_Create;
	rs.Configure		= &RevolvingCheckerPlugin_Configure;
	rs.Destroy			= NULL;
	rs.DoWork			= &RevolvingCheckerPlugin_Work;
	rs.OnSuccess		= &RevolvingCheckerPlugin_OnSuccess;
	rs.OnCancel			= NULL;
	rs.OnFailure		= &RevolvingCheckerPlugin_OnFailure;
	
	// Register the function
	// TO CREATE A NEW ID, GO TO WWW.RANDOM.ORG/BYTES AND GENERATE 
	// FOUR BYTES TO REPRESENT THE ID.
	
	rageSecPluginManager::RegisterPluginFunction(&rs,REVOLVING_CHECKER_ID);
	return true;
}

#endif // RAGE_SEC_TASK_REVOLVING_CHECKER

#endif // RAGE_SEC