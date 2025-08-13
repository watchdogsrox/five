#ifndef DEBUG_CUTSCENE_EDITING_H_
#define DEBUG_CUTSCENE_EDITING_H_

#if __BANK

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //
#include "bank\console.h"

namespace DebugEditing
{
	class CutsceneEditing
	{
	private:
	public:
		static void InitCommands();

		static bool m_Initialized;

	private:
		static void GetCutsceneState(CONSOLE_ARG_LIST);
		static void LightIdCommand(CONSOLE_ARG_LIST);
	};

}
#endif

#endif //DEBUG_CUTSCENE_EDITING_H_
