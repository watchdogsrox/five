#ifndef DEBUG_SHADER_EDITING_H_
#define DEBUG_SHADER_EDITING_H_

#if __BANK

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //
#include "bank\console.h"
#include "vector\vector3.h"

class CEntity;

namespace DebugEditing
{
	struct TexChannelVariable
	{
		const char* m_Channel;   //Channel name used in our DCC.
		const char* m_Variable;	//Variable used in the shader that maps to this channel
	};

	class ShaderEditing
	{
	private:

		struct CSearchObjectInfo
		{
			CSearchObjectInfo() : isVisible(false), entity(NULL), searchName(NULL) { } 

			const char* searchName;
			CEntity* entity;
			float dist2ToCamera;
			bool isVisible;
			Vector3 position; 
		};

	public:
		static void InitCommands();
		static bool IsEnabled() { return m_Enabled; }

		static bool m_Enabled;

	private:
		static void LoadTextureCommand(CONSOLE_ARG_LIST);
		static void SetVariableCommand(CONSOLE_ARG_LIST);
		static void EditCommand(CONSOLE_ARG_LIST);
		static void UpdateCommand(CONSOLE_ARG_LIST);
		static void SelectCommand(CONSOLE_ARG_LIST);

		static bool SearchObjectCallback(CEntity* pEntity, void* data);

		static bool m_Initialized;

	
	};

}
#endif

#endif //DEBUG_SHADER_EDITING_H_
