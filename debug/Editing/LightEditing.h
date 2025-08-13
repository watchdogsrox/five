#ifndef DEBUG_LIGHT_EDITING_H_
#define DEBUG_LIGHT_EDITING_H_

// =============================================================================================== //
// INCLUDES
// =============================================================================================== //
// rage
#include "atl/array.h"
#include "bank/console.h"
#include "renderer/Lights/Lights.h"
#include "vector/vector3.h"

namespace rage
{
	class bkBank;
	class grmShader;
}

class CLightSource;
class CLightEntity;

#if __BANK

#define LIGHT_EDITING_MAX_LIGHTS 8
namespace DebugEditing
{
	class LightEditing
	{
	public:
		static void InitCommands();
		static bool IsInLightEditingMode();
		static void Update();

	private:
		struct FindLightByGuidClosure
		{
			u32	guid;
			CLightEntity* result;
		};

		struct SearchForLightClosure
		{
			SearchForLightClosure() : isVisible(false), useLocalCoordinates(false) { } 

			Vector3 searchPos;
			const char* searchName;
			u32 nearestGuid;
			float dist2ToNearest;
			float dist2ToCamera;
			int numInRange;
			bool isVisible;
			CLightEntity* nearest;
			atArray<u32>* guidExclusionList;
			bool useLocalCoordinates;
		};

		struct SearchForLightParentClosure
		{
			SearchForLightParentClosure() : isVisible(false), entity(NULL), searchName(NULL) { } 

			const char* searchName;
			CEntity* entity;
			float dist2ToCamera;
			bool isVisible;
			Vector3 position; 
		};

		static bool FindLightByGuidCb(void* pItem, void* data);
		static bool ChangeLightToGuid(u32 guid);

		static bool SearchForLightCallback(CEntity* entity, void* data);
		static bool SearchForLightParentCallback(CEntity* pEntity, void* data);
		static bool InitializeGuidMap(void* pItem, void* data);

		static u32	SearchForLight(const Vector3& pos, const char* searchName, bool useLocalCoordinates, atArray<u32>* exclusionList);
		static u32	SearchForLight(const Vector3& pos, const char* searchName, bool useLocalCoordinates);

		static u32	AddLight(const Vector3& localPosition, const Vector3& localDirection, const char* parentName, float falloff, float coneOuter);
		static void RemoveLight(u32 guid);

		static void AddLightToGuidMap(CLightEntity* entity);
		static void RemoveLightFromGuidMap(CLightEntity* entity);

		static void SearchCommand(CONSOLE_ARG_LIST);
		static void SearchLocalCommand(CONSOLE_ARG_LIST);
		static void EditCommand(CONSOLE_ARG_LIST);
		static void CheckGuidExistCommand(CONSOLE_ARG_LIST);
		static void AddCommand(CONSOLE_ARG_LIST);
		static void RemoveCommand(CONSOLE_ARG_LIST);

		static void InitCommand(CONSOLE_ARG_LIST);
		static void ShutdownCommand(CONSOLE_ARG_LIST);

		static void SelectedCommand(CONSOLE_ARG_LIST);

		enum LightEditingCommand
		{
			ADD_LIGHT,
			REMOVE_LIGHT
		};

		struct PendingLightCommand
		{
			LightEditingCommand mType;
			CEntity* mEntity;
			CLightAttr mAttributes;
			int mLightIndex;
		};

		static atArray<PendingLightCommand> mPendingLightChanges;
	};

}
#endif

#endif //DEBUG_LIGHT_EDITING_H_
