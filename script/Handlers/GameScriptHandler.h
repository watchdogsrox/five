//
// name:		NetGameScriptHandler.h
// description:	Project specific script handler
// written by:	John Gurney
//

#ifndef GAME_SCRIPT_HANDLER_H
#define GAME_SCRIPT_HANDLER_H

#include <set>

// framework includes
#include "fwscript/scriptHandler.h"
#include "streaming/streamingmodule.h"

// game includes
#include "script/handlers/GameScriptIds.h"
#include "script/script_memory_tracking.h"

class CObject;
class CPhysical;

class CGameScriptHandler: public scriptHandler
{
public:

	static const unsigned MAX_NUM_SCRIPT_HANDLERS = 83;

public:

	CGameScriptHandler(scrThread& scriptThread);
	 ~CGameScriptHandler();

	FW_REGISTER_CLASS_POOL(CGameScriptHandler);

	scriptIdBase& GetScriptId() { return m_ScriptId; }
	const scriptIdBase& GetScriptId() const { return m_ScriptId; }

    CObject* GetScriptCObject(ScriptObjectId objectId);
	CPhysical* GetScriptEntity(ScriptObjectId objectId);
#if __DEV || __BANK
	const char* GetScriptName() const { return m_ScriptName.c_str(); }
#else
	const char* GetScriptName() const { return ""; }
#endif

	virtual bool Update();
	virtual void Shutdown();

	virtual bool Terminate();
	virtual bool RequiresANetworkComponent() const { return false; }
	virtual void CreateNetworkComponent();
	virtual scriptResource* RegisterScriptResource(scriptResource& resource, bool *pbResourceHasJustBeenAddedToList = NULL);
	virtual bool RemoveScriptResource(ScriptResourceId resourceId, bool bDetach = false, bool bAssert = true, ScriptResourceType resourceType = (ScriptResourceType)-1);
	virtual bool RemoveScriptResource(ScriptResourceType type, const ScriptResourceRef ref, bool bDetach = false, bool bAssert = true);

	// Some script resources are referred to in the scripts using their reference, others using the unique id. This should be made consistent.
	// Ids should always be used, because otherwise a resource may be cleaned up twice by a script. This involves changing loads of script commands
	// which is not doable at the moment.
	// 
	// Using references:
	//
	// INT fireId1 = START_SCRIPT_FIRE(..)   // fire created in slot 0, ref is 0.
	// REMOVE_SCRIPT_FIRE(fireId1)			 // fire in slot 0 is removed.
	// INT fireId2 = START_SCRIPT_FIRE(..)	 // fire created in slot 0 again, ref is again 0.
	// REMOVE_SCRIPT_FIRE(fireId1)			 // fire in slot 0 is incorrectly removed. fireId2 is now referring to an invalid fire.
	//
	// Using ids:
	//
	// INT fireId1 = START_SCRIPT_FIRE(..)   // fire created in slot 0, id is 1.
	// REMOVE_SCRIPT_FIRE(fireId1)			 // fire in slot 0 is removed.
	// INT fireId2 = START_SCRIPT_FIRE(..)	 // fire created in slot 0 again, id is 2.
	// REMOVE_SCRIPT_FIRE(fireId1)			 // asserts - no fire resource exists with id 1.
	//

	// Registers a new script resource and returns its reference. Returns an invalid reference if the registering or resource creation fails.
	ScriptResourceRef RegisterScriptResourceAndGetRef(scriptResource& resource);

	// registers a new script resource and returns its id. Returns an invalid id if the registering or resource creation fails.
	ScriptResourceId RegisterScriptResourceAndGetId(scriptResource& resource);

	u32	GetNumRegisteredObjects() const;
	u32	GetNumRegisteredNetworkedObjects() const;

	// fills the given array with all entities held on the script handler's list. Returns the number of entities.
	u32	GetAllEntities(class CEntity* entityArray[], u32 arrayLen);

	// gets the total streaming memory usage of all objects and resources used by the handler
//	void GetStreamingMemoryUsage(u32& virtualSize, u32& physicalSize, const strIndex* ignoreList=NULL, s32 numIgnores=0) const;

	// gets the streaming indices of objects and streaming resources used by the handler
	void GetStreamingIndices(atArray<strIndex>& streamingIndices, u32 skipFlag) const;

#if __BANK
	// displays info on this handler
	virtual void DisplayScriptHandlerInfo() const;

	// displays debug info on the script handler objects and resources
	void DisplayObjectAndResourceInfo() const;

	// spews debug info to the TTY on the script handler objects and resources
	virtual void SpewObjectAndResourceInfo() const;

	// spews debug info to the TTY on the script handler resources
	virtual void SpewAllResourcesOfType(ScriptResourceType resourceType) const;
#endif	//	__BANK

#if __SCRIPT_MEM_CALC
	void DisplayUniqueObjectsForScript(const atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores) const;

	void CalculateMemoryUsage(atArray<strIndex>& streamingIndices, const strIndex* ignoreList, s32 numIgnores, 
		u32 &VirtualForResourcesWithoutStreamingIndex, u32 &PhysicalForResourcesWithoutStreamingIndex,
		bool bDisplayScriptDetails, bool bFilterDetails, u32 skipFlag) const;
#endif //	__SCRIPT_MEM_CALC

	virtual ScriptResourceId GetNextFreeResourceId(scriptResource& resource);

	static const unsigned MAX_UNNETWORKED_OBJECTS = 100;
	void DetachAllUnnetworkedObjects(scriptHandlerObject* objects[MAX_UNNETWORKED_OBJECTS], u32& numObjects);

protected:

	virtual bool DestroyScriptResource(scriptResource& resource);

	virtual void CleanupScriptObject(scriptHandlerObject &object);

#if __DEV
	void FindScriptName();
#endif // __DEV

protected:

	// we need to store the script id here as we may not always be able to construct it from the thread (eg. it may have terminated)
	CGameScriptId	m_ScriptId;

#if __DEV || __BANK
	atString m_ScriptName; // used for debugging
#endif
};

#endif  // GAME_SCRIPT_HANDLER_H
