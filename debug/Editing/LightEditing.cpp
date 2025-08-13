// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage
#include "atl/array.h"
#include "bank/bank.h"
#include "bank/console.h"
#include "file/asset.h"
#include "system/system.h"
#include "fwdebug/picker.h"

// game
#include "Debug/Editing/LightEditing.h"
#include "Debug/Rendering/DebugLights.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/viewportmanager.h"
#include "optimisations.h"
#include "renderer/GtaDrawable.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/LightEntity.h"
#include "scene/world/GameWorld.h"
#include "scene/portals/InteriorInst.h"

RENDER_OPTIMISATIONS()

#if __BANK

using namespace DebugEditing;

atArray<LightEditing::PendingLightCommand> LightEditing::mPendingLightChanges;

// ----------------------------------------------------------------------------------------------- //

bool LightEditing::FindLightByGuidCb(void* pItem, void* data)
{
	FindLightByGuidClosure& c = *reinterpret_cast<FindLightByGuidClosure*>(data);

	if (pItem != NULL)
	{
		CLightEntity *pLightEntity = static_cast<CLightEntity*>(pItem);
		u32 guid = DebugLights::GetGuidFromEntity(pLightEntity);
		if (guid == c.guid)
		{
			c.result = pLightEntity;
			return false; // all done
		}
	}
	
	return true;
}

// ----------------------------------------------------------------------------------------------- //

bool LightEditing::ChangeLightToGuid(u32 guid)
{
	if (guid != 0)
	{
		FindLightByGuidClosure c;
		c.guid = guid;
		c.result = NULL;
		CLightEntity::GetPool()->ForAll(&FindLightByGuidCb, &c);
		if (c.result)
		{
			DebugLights::SetCurrentLightEntity(c.result); 
			return true;
		}
	}
	else
	{
		DebugLights::SetCurrentLightEntity(NULL); 
	}

	return false;
}

// ----------------------------------------------------------------------------------------------- //

bool LightEditing::SearchForLightCallback(CEntity* entity, void* data)
{
	CLightEntity* light = reinterpret_cast<CLightEntity*>(entity);
	SearchForLightClosure& c = *reinterpret_cast<SearchForLightClosure*>(data);

	Vector3 lightPos;
	Matrix34 parentTrans(Matrix34::IdentityType);
	Vector3 searchPos = c.searchPos;

	if (!light || !light->GetLight()) 
	{
		return true;
	}

	if (c.searchName != NULL)  // search by name and local space pos
	{
		if (!light->GetParent())
		{
			return true; // no parent, can't search by name
		}

		CBaseModelInfo* inf = light->GetParent()->GetBaseModelInfo();
		if (inf)
		{
			const char* pInfoModelName = inf->GetModelName();
			if ( stricmp(pInfoModelName, c.searchName) != 0)
			{				
				return true; // no name or name didn't match
			}
		}
		else
		{
			return true;
		}

		if ( c.useLocalCoordinates == false )
		{
			Matrix34 interiorTransform(Matrix34::IdentityType);
			CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation(light->GetInteriorLocation());
			if (pIntInst)
			{
				interiorTransform = MAT34V_TO_MATRIX34(pIntInst->GetTransform().GetMatrix());
			}

			interiorTransform.Transform(searchPos);
		}

		parentTrans = MAT34V_TO_MATRIX34(light->GetParent()->GetTransform().GetMatrix());
		light->GetLight()->GetPos(lightPos);  //NOTE: Get the position of the light from the effect, 
											  //not the light entity (which doesn't appear to be used.)
	}
	else
	{
		bool found = light->GetWorldPosition(lightPos);

		if (!found)
		{
			return true;
		}
	}

	//Visibility Test - Preference to a Visible Light.
	bool isVisible = false;
	
	if ( light->GetIsOnScreen() )
	{
		isVisible = true;
	}

	const Vector3& cameraPosition = camInterface::GetPos();
	Vector3 localLightPosition = lightPos;
	Vector3 globalLightPosition = lightPos;
	parentTrans.Transform(globalLightPosition);

	float dist2 = searchPos.Dist2(globalLightPosition);
	float distCamera2 = cameraPosition.Dist2(globalLightPosition);
	if ( c.useLocalCoordinates )
	{
		dist2 = searchPos.Dist2(localLightPosition);
	}

	bool bSetNewLight = false;
	if ( c.isVisible == false && isVisible == true)
		{
			bSetNewLight = true;
		}
	else if ( c.isVisible == isVisible ) 
		{
			if (dist2 < c.dist2ToNearest)
			{
				bSetNewLight = true;
			}
			else if (dist2 == c.dist2ToNearest && distCamera2 < c.dist2ToCamera)
			{
				bSetNewLight = true;
			}
		}

	if ( bSetNewLight )
	{
		bool excluded = false;

		const atArray<u32>& exclusionList = *(c.guidExclusionList);
		for (int exclusionIndex = 0; exclusionIndex < exclusionList.GetCount(); ++exclusionIndex)
		{
			u32 lightGuid = DebugLights::GetGuidFromEntity(light);
			if ( lightGuid == exclusionList[exclusionIndex] )
			{
				excluded = true;
				break;
			}
		}

		if (excluded == false)
		{
			c.nearestGuid = DebugLights::GetGuidFromEntity(light);
			c.nearest = light;
			c.isVisible = isVisible;
			c.dist2ToNearest = dist2;
			c.dist2ToCamera = distCamera2;
		}
	}
	
	return true;
}

// ----------------------------------------------------------------------------------------------- //
u32 LightEditing::SearchForLight(const Vector3& pos, const char* searchName, bool useLocalCoordinates, atArray<u32>* exclusionList)
{
	SearchForLightClosure c;
	c.searchPos = pos;
	c.searchName = searchName;
	c.nearestGuid = 0;
	c.dist2ToNearest = FLT_MAX;
	c.numInRange = 0;
	c.guidExclusionList = exclusionList;
	c.nearest = NULL;
	c.useLocalCoordinates = useLocalCoordinates;

	const Vector3& cameraPosition = camInterface::GetPos();
	static dev_float LIGHT_EDITOR_OBJECT_SCAN_RADIUS = 299.0f;
	const spdSphere scanningSphere(RCC_VEC3V(cameraPosition), ScalarV(LIGHT_EDITOR_OBJECT_SCAN_RADIUS));

	// Scan through all intersecting entities finding all entities before
	fwIsSphereIntersecting intersection(scanningSphere);
	CGameWorld::ForAllEntitiesIntersecting(&intersection, &SearchForLightCallback, (void*)&c, ENTITY_TYPE_MASK_LIGHT, SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_ALL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_DEFAULT);

	return c.nearestGuid;
}

u32 LightEditing::SearchForLight(const Vector3& pos, const char* searchName, bool useLocalCoordinates)
{
	atArray<u32> unusedList;
	return SearchForLight(pos, searchName, useLocalCoordinates, &unusedList);
}

// ----------------------------------------------------------------------------------------------- //

bool LightEditing::SearchForLightParentCallback(CEntity* pEntity, void* data)
{
	SearchForLightParentClosure& c = *reinterpret_cast<SearchForLightParentClosure*>(data);

	if (c.searchName != NULL)  // search by name and local space pos
	{
		CBaseModelInfo* baseInfo = pEntity->GetBaseModelInfo();
		if (!baseInfo)
		{
			return true;
		}

		const char* pInfoModelName = baseInfo->GetModelName();
		if ( stricmp(pInfoModelName, c.searchName) == 0)
		{
			bool isVisible = false;

			if ( pEntity->GetIsOnScreen() )
			{
				isVisible = true;
			}

			const Vector3& cameraPosition = camInterface::GetPos();

			if ( c.isVisible == false && isVisible == true)
			{
				const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
				float distCamera2 = cameraPosition.Dist2(vEntityPosition);

				c.isVisible = isVisible;
				c.dist2ToCamera = distCamera2;

				c.entity = pEntity;
				c.position = vEntityPosition;
			}
			else if ( c.isVisible == isVisible ) 
			{
				const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
				float distCamera2 = cameraPosition.Dist2(vEntityPosition);

				if (distCamera2 < c.dist2ToCamera)
				{
					c.isVisible = isVisible;
					c.dist2ToCamera = distCamera2;

					c.entity = pEntity;
					c.position = vEntityPosition;
				}
			}

			return true; // no name or name didn't match
		}
	}

	return true;
}


// ----------------------------------------------------------------------------------------------- //

bool LightEditing::InitializeGuidMap(void* pItem, void* /*data*/)
{
	if (pItem) 
	{
		CLightEntity* pLight = static_cast<CLightEntity*>(pItem);
		DebugLights::AddLightToGuidMap(pLight);
	}

	return true;
}

// ----------------------------------------------------------------------------------------------- //
u32 LightEditing::AddLight(const Vector3& localPosition, const Vector3& localDirection, const char* parentName, float falloff, float coneOuter)
{
	//Find the parent's current position.  
	SearchForLightParentClosure searchParentInfo;
	searchParentInfo.searchName = parentName;

	//Base this search on the camera's current position. 
	const Vector3& cameraPosition = camInterface::GetPos();
	static dev_float LIGHT_EDITOR_OBJECT_SCAN_RADIUS = 299.0f;

	// Sphere to search for entities inside
	const spdSphere scanningSphere(RCC_VEC3V(cameraPosition), ScalarV(LIGHT_EDITOR_OBJECT_SCAN_RADIUS));

	// Scan through all intersecting entities finding all entities before
	fwIsSphereIntersecting intersection(scanningSphere);
	CGameWorld::ForAllEntitiesIntersecting(&intersection, &SearchForLightParentCallback, (void*)&searchParentInfo, ENTITY_TYPE_MASK_LIGHT | ENTITY_TYPE_MASK_BUILDING | ENTITY_TYPE_MASK_VEHICLE | ENTITY_TYPE_MASK_PED | ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_ALL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_DEFAULT);

	if ( searchParentInfo.entity != NULL )
	{
		if ( CLightEntity::GetPool()->GetNoOfUsedSpaces() < CLightEntity::GetPool()->GetSize() )
		{
			//Create the default light information -- this will get updated at a later pass.
			CLightAttr defaultLightAttributes;

			defaultLightAttributes.Reset(localPosition, localDirection, falloff, coneOuter);

			CEntity* pParent = searchParentInfo.entity;
			CBaseModelInfo* pModelInfo = pParent->GetBaseModelInfo();
			gtaDrawable* pDrawable = (gtaDrawable*)pModelInfo->GetDrawable();
			if ( pDrawable != NULL )
			{
				if ( mPendingLightChanges.GetCount() < mPendingLightChanges.GetCapacity())
				{
					PendingLightCommand lightCommand;
					lightCommand.mType = ADD_LIGHT;
					lightCommand.mEntity = pParent;
					lightCommand.mAttributes = defaultLightAttributes;
					mPendingLightChanges.Push(lightCommand);
				}
			}
			else
			{
				//Unable to find the drawable; possibly it hasn't yet been loaded.
			}
		}
		else
		{
			//Run out of light entities!
		}
	}
	else
	{
		//Unable to find an associated light.
	}

	return 0;
}

// ----------------------------------------------------------------------------------------------- //
void LightEditing::RemoveLight(u32 guid)
{
	FindLightByGuidClosure c;
	c.guid = guid;
	c.result = NULL;
	CLightEntity::GetPool()->ForAll(&FindLightByGuidCb, &c);
	if (c.result)
	{
		CEntity* pParent = c.result->GetParent();
		if ( pParent != NULL )
		{
			CLightAttr* pLightAttributes = c.result->GetLight();
			Vector3 lightPosition;
			pLightAttributes->GetPos(lightPosition);
			Vector3 lightDirection(pLightAttributes->m_direction.x, pLightAttributes->m_direction.y, pLightAttributes->m_direction.z);

			CBaseModelInfo* pModelInfo = pParent->GetBaseModelInfo();
			gtaDrawable* pDrawable = (gtaDrawable*)pModelInfo->GetDrawable();
			if ( pDrawable != NULL )
			{

				for (int lightIndex = 0; lightIndex < pDrawable->m_lights.GetCount(); ++lightIndex)
				{
					CLightAttr& pCurrentAttributes = pDrawable->m_lights[lightIndex];
					const Vector3& pCurrentPosition = pCurrentAttributes.GetPosDirect().GetVector3();
					const Vector3& pCurrentDirection = pCurrentAttributes.m_direction.GetVector3();

					const float distThreshold = (0.01f * 0.01f);
					float distSquared = pCurrentPosition.Dist2(lightPosition);
					float dirSquared = pCurrentDirection.Dist2(lightDirection);

					if( distSquared < distThreshold && dirSquared < distThreshold)
					{

						if ( mPendingLightChanges.GetCount() < mPendingLightChanges.GetCapacity())
						{
							PendingLightCommand lightCommand;
							lightCommand.mType = REMOVE_LIGHT;
							lightCommand.mEntity = pParent;
							lightCommand.mLightIndex = lightIndex;
							mPendingLightChanges.Push(lightCommand);
						}
						else
						{
							//Ran out of space to queue command.
						}
						
						break;
					}
				}
			}
			else
			{
				//Unable to find the drawable.
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void LightEditing::SearchCommand(CONSOLE_ARG_LIST)
{
	const char* lightSearchHelp = "Usage: le_search name x y z [guid exclusion list]";
	const char* lightSearchReturn = "Returns: guid of the found light. 0 indicates no light was found.";
	const bool useLocalCoordinates = false;
	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output(lightSearchHelp);
		output(lightSearchReturn);
		return;
	}

	if (numArgs == 5)
	{
		const char* name = args[0];
		float x = (float) atof(args[1]);
		float y = (float) atof(args[2]);
		float z = (float) atof(args[3]);

		atString exclusionListStr(args[4]);
		atArray<atString> exclusionListArray;
		exclusionListStr.Split(exclusionListArray, ",", true);

		//Convert the exclusionListArray to an array of integers.
		atArray<u32> exclusionListInt;
		exclusionListInt.Reserve(10); 
		for(int arrayIndex = 0; arrayIndex < exclusionListArray.GetCount(); ++arrayIndex)
		{
			u32 guid = strtoul(exclusionListArray[arrayIndex].c_str(), 0, 0 /*radix*/);
			exclusionListInt.PushAndGrow(guid);
		}

		Vector3 searchPos(x,y,z);
		char buf[32];
		formatf(buf, "%d", SearchForLight(searchPos, name, useLocalCoordinates, &exclusionListInt));
		output(buf);
	}
	else if (numArgs == 4)
	{
		const char* name = args[0];
		float x = (float) atof(args[1]);
		float y = (float) atof(args[2]);
		float z = (float) atof(args[3]);

		Vector3 searchPos(x,y,z);
		char buf[32];
		formatf(buf, "%d", SearchForLight(searchPos, name, useLocalCoordinates));
		output(buf);
	}
	else
	{
		output("Invalid arguments!");
		output(lightSearchHelp);
		output(lightSearchReturn);
	}
}


// ----------------------------------------------------------------------------------------------- //

void LightEditing::SearchLocalCommand(CONSOLE_ARG_LIST)
{
	const char* lightSearchHelp = "Usage: le_search_local name x y z [guid exclusion list]";
	const char* lightSearchReturn = "Returns: guid of the found light. 0 indicates no light was found.";
	const bool useLocalCoordinates = true;

	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output(lightSearchHelp);
		output(lightSearchReturn);
		return;
	}

	if (numArgs == 5)
	{
		const char* name = args[0];
		float x = (float) atof(args[1]);
		float y = (float) atof(args[2]);
		float z = (float) atof(args[3]);

		atString exclusionListStr(args[4]);
		atArray<atString> exclusionListArray;
		exclusionListStr.Split(exclusionListArray, ",", true);

		//Convert the exclusionListArray to an array of integers.
		atArray<u32> exclusionListInt;
		exclusionListInt.Reserve(10); 
		for(int arrayIndex = 0; arrayIndex < exclusionListArray.GetCount(); ++arrayIndex)
		{
			u32 guid = strtoul(exclusionListArray[arrayIndex].c_str(), 0, 0 /*radix*/);
			exclusionListInt.PushAndGrow(guid);
		}

		Vector3 searchPos(x,y,z);
		char buf[32];
		formatf(buf, "%d", SearchForLight(searchPos, name, useLocalCoordinates, &exclusionListInt));
		output(buf);
	}
	else if (numArgs == 4)
	{
		const char* name = args[0];
		float x = (float) atof(args[1]);
		float y = (float) atof(args[2]);
		float z = (float) atof(args[3]);

		Vector3 searchPos(x,y,z);
		char buf[32];
		formatf(buf, "%d", SearchForLight(searchPos, name, useLocalCoordinates));
		output(buf);
	}
	else
	{
		output("Invalid arguments!");
		output(lightSearchHelp);
		output(lightSearchReturn);
	}
}


// ----------------------------------------------------------------------------------------------- //
void LightEditing::SelectedCommand(CONSOLE_ARG_LIST)
{
	const char* lightInitHelp = "Usage: le_selected";

	if ( numArgs == 1 && !strcmp(args[0], "-help") )
	{
		output(lightInitHelp);
		return;
	}

	fwEntity *pEntity = g_PickerManager.GetSelectedEntity();
	if(pEntity)
	{
		CLightEntity *pLightEntity = static_cast<CLightEntity*>(pEntity);
		if(pLightEntity)
		{
			fwEntity *pLightParentEntity = pLightEntity->GetParent();
			if(pLightParentEntity)
			{
				output(pLightParentEntity->GetModelName());
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------- //

void LightEditing::EditCommand(CONSOLE_ARG_LIST)
{
	const char* lightEditHelp = "Usage: le_edit [guid]";

	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output("Usage: le_edit [guid]");
		return;
	}

	if (numArgs == 1)
	{
		char buf[32];
		u32 guid = strtoul(args[0], NULL, 0);
		if ( ChangeLightToGuid(guid) == true )
		{
			formatf(buf, "%d", guid);
		}
		else
		{
			formatf(buf, "0");
		}
		
		output(buf);
	}
	else
	{
		output("Invalid arguments!");
		output(lightEditHelp);
	}
}

// ----------------------------------------------------------------------------------------------- //

void LightEditing::CheckGuidExistCommand(CONSOLE_ARG_LIST)
{
	const char* lightEditHelp = "Usage: le_guid_exist [guid]";

	if (numArgs == 1 && !strcmp(args[0], "-help"))
	{
		output("Usage: le_guid_exist [guid]");
		return;
	}

	if (numArgs == 1)
	{
		u32 guid = strtoul(args[0], NULL, 0);
		if ( DebugLights::DoesGuidExist(guid) == true )
		{
			output("1");
		}
		else
		{
			output("0");
		}		
	}
	else
	{
		output("Invalid arguments!");
		output(lightEditHelp);
	}
}

// ----------------------------------------------------------------------------------------------- //
void LightEditing::AddCommand(CONSOLE_ARG_LIST)
{
	const char* lightAddHelp = "Usage: le_add [parent model name] x y z dir_x dir_y dir_z falloff coneOuter";
	const char* lightAddReturns = "Returns: GUID of newly created light.";

	if ( numArgs == 1 && !strcmp(args[0], "-help") )
	{
		output(lightAddHelp);
		output(lightAddReturns);
		return;
	}

	if ( numArgs == 9 )
	{
		const char* parentName = args[0];
		float x = (float) atof(args[1]);
		float y = (float) atof(args[2]);
		float z = (float) atof(args[3]);
		float dirx = (float) atof(args[4]);
		float diry = (float) atof(args[5]);
		float dirz = (float) atof(args[6]);
		float falloff = (float) atof(args[7]);
		float coneOuter = (float) atof(args[8]);

		Vector3 lightPosition(x, y, z);
		Vector3 lightDirection(dirx, diry, dirz);
		char buf[32];
		u32 newLightGUID = AddLight(lightPosition, lightDirection, parentName, falloff, coneOuter);

		formatf(buf, "%d", newLightGUID);
		output(buf);
	}
	else
	{
		output("Invalid arguments!");
		output(lightAddHelp);
		output(lightAddReturns);
	}
}

// ----------------------------------------------------------------------------------------------- //
void LightEditing::InitCommand(CONSOLE_ARG_LIST)
{
	const char* lightInitHelp = "Usage: le_init";

	if ( numArgs == 1 && !strcmp(args[0], "-help") )
	{
		output(lightInitHelp);
		return;
	}

	CLightEntity::GetPool()->ForAll(&InitializeGuidMap, NULL);
}

// ----------------------------------------------------------------------------------------------- //
void LightEditing::ShutdownCommand(CONSOLE_ARG_LIST)
{
	const char* lightShutdownHelp = "Usage: le_shutdown";

	if ( numArgs == 1 && !strcmp(args[0], "-help") )
	{
		output(lightShutdownHelp);
		return;
	}

	DebugLights::ClearLightGuidMap();
}

// ----------------------------------------------------------------------------------------------- //
void LightEditing::RemoveCommand(CONSOLE_ARG_LIST)
{
	const char* lightRemoveHelp = "Usage: le_remove guid";

	if ( numArgs == 1 && !strcmp(args[0], "-help") )
	{
		output(lightRemoveHelp);
		return;
	}

	if ( numArgs == 1 )
	{
		u32 guid = strtoul(args[0], NULL, 0);
		RemoveLight(guid);
	}
	else
	{
		output("Invalid arguments!");
		output(lightRemoveHelp);
	}
}
// ----------------------------------------------------------------------------------------------- //
void PingCommand(UNUSED_CONSOLE_ARG_LIST)
{
	output("rc_ping");
}
// ----------------------------------------------------------------------------------------------- //

void LightEditing::InitCommands()
{
	// Add console commands
	bkConsole::CommandFunctor fn;
	fn.Reset<SearchCommand>();
	bkConsole::AddCommand("le_search", fn);

	fn.Reset<SearchLocalCommand>();
	bkConsole::AddCommand("le_search_local", fn);

	fn.Reset<EditCommand>();
	bkConsole::AddCommand("le_edit", fn);

	fn.Reset<CheckGuidExistCommand>();
	bkConsole::AddCommand("le_guid_exist", fn);
	
	fn.Reset<AddCommand>();
	bkConsole::AddCommand("le_add", fn);

	fn.Reset<RemoveCommand>();
	bkConsole::AddCommand("le_remove", fn);

	fn.Reset<InitCommand>();
	bkConsole::AddCommand("le_init", fn);

	fn.Reset<ShutdownCommand>();
	bkConsole::AddCommand("le_shutdown", fn);

	fn.Reset<PingCommand>();
	bkConsole::AddCommand("rc_ping", fn);

	fn.Reset<SelectedCommand>();
	bkConsole::AddCommand("le_selected", fn);

	mPendingLightChanges.Reserve(LIGHT_EDITING_MAX_LIGHTS);
}

// ----------------------------------------------------------------------------------------------- //

bool LightEditing::IsInLightEditingMode()
{
	//TODO:  Possibly move this variable into the LightEditing class itself.
	return DebugLights::m_debug;
}

// ----------------------------------------------------------------------------------------------- //

void LightEditing::Update()
{
	if ( IsInLightEditingMode() == false )
		return;

	if ( mPendingLightChanges.GetCount() == 0 )
		return;

	atArray<CEntity*> updatedEntities;
	updatedEntities.Reserve(mPendingLightChanges.GetCount());
	for (int commandIndex = 0; commandIndex < mPendingLightChanges.GetCount(); ++commandIndex)
	{
		PendingLightCommand& command = mPendingLightChanges[commandIndex];
		CEntity* pParent = command.mEntity;
		CBaseModelInfo* pModelInfo = pParent->GetBaseModelInfo();
		gtaDrawable* pDrawable = (gtaDrawable*)pModelInfo->GetDrawable();

		if ( pDrawable != NULL )
		{
			if ( command.mType == ADD_LIGHT )
			{
				pDrawable->m_lights.PushAndGrow(command.mAttributes);
			}
			else if ( command.mType == REMOVE_LIGHT )
			{
				pDrawable->m_lights.Delete(command.mLightIndex);
			}

			bool duplicate = false;
			for ( int entityIndex = 0; entityIndex < updatedEntities.GetCount(); ++entityIndex )
			{
				if ( updatedEntities[entityIndex] == command.mEntity )
				{
					duplicate = true;
					break;
				}
			}

			if (duplicate == false)
			{
				updatedEntities.Push(command.mEntity);
			}
		}
	}

 	for ( int entityIndex = 0; entityIndex < updatedEntities.GetCount(); ++entityIndex)
 	{
 		CEntity* pEntity = updatedEntities[entityIndex];
		LightEntityMgr::RemoveAttachedLights(pEntity);
 	}

	LightEntityMgr::Update();

	for ( int entityIndex = 0; entityIndex < updatedEntities.GetCount(); ++entityIndex)
	{
		CEntity* pEntity = updatedEntities[entityIndex];
		LightEntityMgr::AddAttachedLights(pEntity);
		pEntity->m_nFlags.bLightObject = true;
	}

	mPendingLightChanges.ResetCount();
}

#endif
