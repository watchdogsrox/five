// =============================================================================================== //
// INCLUDES
// =============================================================================================== //
#include "Camera/CamInterface.h"
#include "Debug/Editing/ShaderEditing.h"
#include "debug/GtaPicker.h"
#include "ModelInfo/BaseModelInfo.h"
#include "ModelInfo/ModelInfo.h"
#include "Scene/Entity.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/ShaderEdit.h"

#include "grcore/debugdraw.h"

RENDER_OPTIMISATIONS()

#if __BANK

// ========================================================================================================================
// note: ShaderEditing commands interface with //depot/gta5/tools/dcc/current/max2012/scripts/pipeline/util/shadereditor.ms
// ========================================================================================================================

using namespace DebugEditing;

bool ShaderEditing::m_Enabled = false;
bool ShaderEditing::m_Initialized = false;

void ShaderEditing::InitCommands()
{
	if ( m_Initialized == true )
		return;
	
	// Add console commands
	bkConsole::CommandFunctor fn;
	fn.Reset<LoadTextureCommand>();
	bkConsole::AddCommand("se_loadtexture", fn);

	fn.Reset<EditCommand>();
	bkConsole::AddCommand("se_edit", fn);

	fn.Reset<UpdateCommand>();
	bkConsole::AddCommand("se_update", fn);

	fn.Reset<SelectCommand>();
	bkConsole::AddCommand("se_select", fn);

	m_Initialized = true;
}

void ShaderEditing::LoadTextureCommand(CONSOLE_ARG_LIST)
{
	const char* shaderTextureLoadHelp = "Usage: se_loadtexture shaderName channel previousMaterial filepath";
	const char* shaderTextureLoadReturn = "Returns: zero on failure.  Non-zero on success.";

	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output(shaderTextureLoadHelp);
		output(shaderTextureLoadReturn);
		return;
	}

	if ( numArgs == 4 || numArgs == 5 )
	{
		const char* shaderName = args[0];
		const char* channel = strlwr( (char*) args[1] );
		const char* previousTextureName = strlwr( (char*) args[2] );
		const char* filename = args[3];

		int index = -1;
		if ( numArgs == 5 )
		{
			index = strtol(args[4], NULL, 0);
		}

		SHADEREDIT_VERBOSE_ONLY
		(
			Displayf(
				"> ShaderEditing::LoadTextureCommand(shaderName=\"%s\", channel=\"%s\", previousTextureName=\"%s\", filename=\"%s\", index=%d)",
				shaderName,
				channel,
				previousTextureName,
				filename,
				index
			);

			const int numPending = g_ShaderEdit::GetInstance().GetChangeData().GetCount();

			if (numPending > 0)
			{
				Displayf("> .. current pending updates = %d", numPending);
			}
		);

		if ( stricmp(channel, "null") == 0 )
			channel = NULL;

		//Protect the system from ambiguous materials on the same object.
		atArray<grcTexChangeData*> changeData;
		g_ShaderEdit::GetInstance().FindChangeData(shaderName, channel, previousTextureName, changeData);
		if ( changeData.GetCount() == 1 )
		{
			SHADEREDIT_VERBOSE_ONLY(Displayf("> .. changing texture .."));

			changeData[0]->m_filename = filename;
			changeData[0]->m_ready = true;

			output("");  //Success.
			return; 
		}
		else if ( index >= 0 )
		{
			if ( changeData.GetCount() == 0 )
			{
				g_ShaderEdit::GetInstance().FindChangeData(shaderName, channel, NULL, changeData);

				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. changing specific texture (index=%d) ..", index));
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. found %d candidates with previousTextureName=NULL..", changeData.GetCount()));
			}
			else
			{
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. changing specific texture (index=%d) ..", index));
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. found %d candidates with previousTextureName=\"%s\" ..", changeData.GetCount(), previousTextureName));
			}

			if ( index < changeData.GetCount() )
			{
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. changing specific texture (index=%d) ..", index));

				//Handles the case where the user explicitly specifies which texture to edit.
				changeData[index]->m_filename = filename;
				changeData[index]->m_ready = true;
			}
			else
			{
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. failed!"));
			}

			output("");  //Success.
			return; 
		}
		else
		{
			//If there are no change values to the list, list all textures (send the list back to 3dsmax so the user can pick one)
			if ( changeData.GetCount() == 0 )
			{
				g_ShaderEdit::GetInstance().FindChangeData(shaderName, channel, NULL, changeData);

				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. ambiguous texture reference!"));
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. found %d candidates with previousTextureName=NULL..", changeData.GetCount()));
			}
			else
			{
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. ambiguous texture reference!"));
				SHADEREDIT_VERBOSE_ONLY(Displayf("> .. found %d candidates with previousTextureName=\"%s\" ..", changeData.GetCount(), previousTextureName));
			}

			//Unable to find the texture or there are multiple textures with the same name.
			//Turn this into a comma-separated string for users to determine which texture to base the change off of.
			for(int arrayIndex = 0; arrayIndex < changeData.GetCount(); ++arrayIndex)
			{
				const char* variableName = changeData[arrayIndex]->m_instance->GetVariableName(changeData[arrayIndex]->m_varNum);
				output(variableName);
				output(" - ");
				output(changeData[arrayIndex]->m_filename.c_str());

				if(arrayIndex  != changeData.GetCount()-1)
					output(",");
			}
		}
	}
	else
	{
		output("");
		return;
	}
}

void ShaderEditing::EditCommand(CONSOLE_ARG_LIST)
{
	//Set the ShaderEdit's selected entity
	const char* shaderEditHelp = "Usage: se_edit [name | null]";
	const char* shaderEditReturn = "Returns: guid of the found object. 0 indicates no object was found.";

	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output(shaderEditHelp);
		output(shaderEditReturn);
		return;
	}

	if (numArgs == 1)
	{
		int returnResult = 0;
		const char* name = args[0];

		SHADEREDIT_VERBOSE_ONLY(Displayf("> ShaderEditing::EditCommand(name=\"%s\")", name));

		if ( stricmp(name, "null") == 0 )
		{
			g_PickerManager.AddEntityToPickerResults(NULL, true, true);
			returnResult = 1;
		}
		else
		{
			const Vector3& cameraPosition = camInterface::GetPos();
			static dev_float SHADER_EDITOR_OBJECT_SCAN_RADIUS = 299.0f;

			CSearchObjectInfo searchObjectInfo;
			searchObjectInfo.searchName = name;
			searchObjectInfo.position = cameraPosition;

			// Sphere to search for entities inside
			const spdSphere scanningSphere(RCC_VEC3V(cameraPosition), ScalarV(SHADER_EDITOR_OBJECT_SCAN_RADIUS));

			// Scan through all intersecting entities finding all entities before
			fwIsSphereIntersectingVisible intersection(scanningSphere);
			CGameWorld::ForAllEntitiesIntersecting(
				&intersection, 
				&SearchObjectCallback, 
				(void*)&searchObjectInfo, 
				ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_ANIMATED_BUILDING | ENTITY_TYPE_MASK_VEHICLE | ENTITY_TYPE_MASK_PED | ENTITY_TYPE_MASK_OBJECT, 
				SEARCH_LOCATION_INTERIORS | SEARCH_LOCATION_EXTERIORS, 
				SEARCH_LODTYPE_ALL, 
				SEARCH_OPTION_FORCE_PPU_CODEPATH | SEARCH_OPTION_LARGE,
				WORLDREP_SEARCHMODULE_DEFAULT);

			if ( searchObjectInfo.entity != NULL )
			{
				//Found a matching entity.
				g_PickerManager.AddEntityToPickerResults(searchObjectInfo.entity, true, true);
				returnResult = 1;
			}
		}

		char buf[32];
		formatf(buf, "%d", returnResult);
		output(buf);
	}
	else
	{
		output("Invalid arguments!");
		output(shaderEditHelp);
		output(shaderEditReturn);
	}
}

void ShaderEditing::UpdateCommand(CONSOLE_ARG_LIST)
{
	const char* shaderUpdateHelp = "Usage: se_update.  This command will refresh the Shader Edit's bank widgets.";

	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output(shaderUpdateHelp);
		return;
	}

	if ( numArgs == 0 )
	{
		g_ShaderEdit::GetInstance().InitWidgets();
	}
	else
	{
		output("Invalid arguments!");
		output(shaderUpdateHelp);
	}
}

void ShaderEditing::SelectCommand(CONSOLE_ARG_LIST)
{
	const char* shaderSelectCommand = "Usage: se_select.  This command will toggle on the flash flag for a given material.";

	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output(shaderSelectCommand);
		return;
	}

	if ( numArgs == 3 || numArgs == 4 )
	{
		const char* shaderName = args[0];
		const char* channel = strlwr( (char*) args[1] );
		const char* previousTextureName = strlwr( (char*) args[2] );

		int index = -1;
		if ( numArgs == 4 )
		{
			index = strtol(args[3], NULL, 0);
		}

		SHADEREDIT_VERBOSE_ONLY
		(
			Displayf(
				"> ShaderEditing::SelectCommand(shaderName=\"%s\", channel=\"%s\", previousTextureName=\"%s\",  index=%d)",
				shaderName,
				channel,
				previousTextureName,
				index
			);
		);

		if ( stricmp(channel, "null") == 0 )
			channel = NULL;

		//Protect the system from ambiguous materials on the same object.
		atArray<grcTexChangeData*> changeData;
		g_ShaderEdit::GetInstance().FindChangeData(shaderName, channel, previousTextureName, changeData);
		for(int dataIndex = 0; dataIndex < changeData.GetCount(); ++dataIndex)
		{
			if ( dataIndex != index )
			{
				changeData[dataIndex]->m_instance->UserFlags &= ~grmShader::FLAG_FLASH_INSTANCES;
			}
			else
			{
				changeData[dataIndex]->m_instance->UserFlags |= grmShader::FLAG_FLASH_INSTANCES;
			}
		}
	}
	else
	{
		output("Invalid arguments!");
		output(shaderSelectCommand);
	}
}

bool ShaderEditing::SearchObjectCallback(CEntity* pEntity, void* data)
{
	CSearchObjectInfo& searchInfo = *(reinterpret_cast<CSearchObjectInfo*>(data));

	if ( searchInfo.searchName == NULL )
		return true;

	CBaseModelInfo* baseModelInfo = pEntity->GetBaseModelInfo();
	if ( baseModelInfo == NULL )
		return true;

	const char* modelName = baseModelInfo->GetModelName();
	if ( stricmp(searchInfo.searchName, modelName) == 0 )
	{
		if ( pEntity->GetIsOnScreen() ) 
		{
			const Vector3& cameraPosition = camInterface::GetPos();
			const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			float distCamera2 = cameraPosition.Dist2(vEntityPosition);
			if ( searchInfo.entity == NULL || distCamera2 < searchInfo.dist2ToCamera)
			{
				searchInfo.isVisible = true;
				searchInfo.dist2ToCamera = distCamera2;

				searchInfo.entity = pEntity;
				searchInfo.position = vEntityPosition;
			}
		}

		return true; // no name or name didn't match
	}

	return true;
}

#endif //__BANK