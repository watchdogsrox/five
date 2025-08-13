// =============================================================================================== //
// INCLUDES
// =============================================================================================== //
#include "Bank\Console.h"
#include "Cutscene\CutSceneLightEntity.h"
#include "Debug\Editing\CommonEditing.h"
#include "Debug\Editing\CutsceneEditing.h"

#include "cutfile\Cutfdefines.h"
#include "cutscene\CutSceneManagerNew.h"
#include "cutscene\cutsmanager.h"

RENDER_OPTIMISATIONS()

#if __BANK

using namespace DebugEditing;

bool CutsceneEditing::m_Initialized = false;

void CutsceneEditing::InitCommands()
{
	if ( m_Initialized == true )
		return;

	// Add console commands
	bkConsole::CommandFunctor fn;

	fn.Reset<GetCutsceneState>();
	bkConsole::AddCommand("cs_getstate", fn);

	fn.Reset<LightIdCommand>();
	bkConsole::AddCommand("cs_lightid", fn);

	m_Initialized = true;
}

void CutsceneEditing::GetCutsceneState(CONSOLE_ARG_LIST)
{
	const char* isPlayingCommandHelp = "Usage: cs_getstate";
	const char* isPlayingCommandReturn = "Returns: Current state of the cutscene.";

	if ( numArgs == 1 && !strcmp(args[0],HelpArgument) )
	{
		output(isPlayingCommandHelp);
		output(isPlayingCommandReturn);
		return;
	}

	cutsManager::ECutsceneState state = CutSceneManager::GetInstance()->GetState();
	int stateInt = (int) state;
	
	char buf[32];
	formatf(buf, "%d", stateInt);
	output(buf);
}

void CutsceneEditing::LightIdCommand(CONSOLE_ARG_LIST)
{
	const char* lightIdCommandHelp = "Usage: cs_lightid light_name";
	const char* lightIdCommandReturn = "Returns:  Returns the index of the light in the cutscene's list.  Otherwise, -1.";

	if ( numArgs == 1 && !strcmp(args[0], HelpArgument) )
	{
		output(lightIdCommandHelp);
		output(lightIdCommandReturn);
		return;
	}

	if ( numArgs == 1 )
	{
		char lightName[CUTSCENE_OBJNAMELEN] = {0};
		strcpy(lightName, args[0]);

		int index = 0;
		int length = istrlen(lightName);
		while(index < length) { if(lightName[index] == '-') lightName[index] = ' '; index++; }  //Convert any hyphens into spaces

		index =  CutSceneManager::GetInstance()->GetLightIndex(lightName);

		//NOTE:  The list the tool-side is based on is 1-based.  
		index++;

		char buf[32];
		formatf(buf, "%d", index);
		output(buf);
	}
	else
	{
		output(lightIdCommandHelp);
		output(lightIdCommandReturn);
	}
}

#endif //__BANK