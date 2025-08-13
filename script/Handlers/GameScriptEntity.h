/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    GameScriptEntity.h
// PURPOSE : entity extension containing script info used by script handlers
// AUTHOR :  JohnG
// CREATED : 29.3.10
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef SCRIPT_ENTITY_H
#define SCRIPT_ENTITY_H

// game includes 
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptIds.h"

// framework includes
#include "entity/entity.h"
#include "fwscript/scripthandler.h"

class CPhysical;

// ================================================================================================================
// CGameScriptHandlerObject - an base class used by all different types of script handler objects
// ================================================================================================================

class CGameScriptHandlerObject : public scriptHandlerObject
{
public:

	virtual bool	IsCleanupRequired() const;

	virtual bool GetNoLongerNeeded() const = 0;
	virtual const char* GetLogName() const = 0;
};

// ================================================================================================================
// CScriptEntityExtension - an extension given to an entity that is a proper mission entity.
// ================================================================================================================

enum
{
	SCRIPT_HANDLER_OBJECT_TYPE_ENTITY,
	SCRIPT_HANDLER_OBJECT_TYPE_DOOR,
	SCRIPT_HANDLER_OBJECT_TYPE_PICKUP,
};

class CScriptEntityExtension : public fwExtension, public CGameScriptHandlerObject
{

public:
	EXTENSIONID_DECL(CScriptEntityExtension, fwExtension);

	CScriptEntityExtension(CPhysical& targetEntity);
	~CScriptEntityExtension();

	FW_REGISTER_CLASS_POOL(CScriptEntityExtension); 

	CPhysical* GetTargetEntity() const { return m_pTargetEntity; }
	
	void SetHandler(scriptHandler& handler) { m_pHandler = &handler; }

	virtual unsigned					GetType() const { return SCRIPT_HANDLER_OBJECT_TYPE_ENTITY; }
	virtual void						CreateScriptInfo(const scriptIdBase& scrId, const ScriptObjectId& objectId, const HostToken hostToken);
	virtual void						SetScriptInfo(const scriptObjInfoBase& scrId);
	virtual scriptObjInfoBase*			GetScriptInfo();
	virtual const scriptObjInfoBase*	GetScriptInfo() const;
	virtual void						SetScriptHandler(scriptHandler* handler);
	virtual scriptHandler*				GetScriptHandler() const { return m_pHandler; }
	virtual void						ClearScriptHandler() { m_pHandler = NULL; }
	virtual void						OnRegistration(bool newObject, bool hostObject);
	virtual void						OnUnregistration();
	virtual void						Cleanup();
	virtual netObject*					GetNetObject() const;
	virtual fwEntity*					GetEntity() const;

	virtual const char* GetLogName() const { return "ENTITY"; }

	bool GetCleanupTargetEntity() const	{ return m_bCleanupTargetEntity; }
	void SetCleanupTargetEntity(bool b)	{ m_bCleanupTargetEntity = b; }

	bool GetNoLongerNeeded() const		{ return m_bNoLongerNeeded; }
	void SetNoLongerNeeded(bool b)		{ m_bNoLongerNeeded = b; }

	bool GetIsNetworked() const			{ return m_bIsNetworked; }
	void SetIsNetworked(bool b)			{ m_bIsNetworked = b; }

	bool GetCanMigrate() const			{ return m_bCanMigrate; }
	void SetCanMigrate(bool b)			{ Assert(m_bIsNetworked); m_bCanMigrate = b; }

	bool GetScriptMigration() const		{ return m_bScriptMigration; }
	void SetScriptMigration(bool b)		{ Assert(m_bIsNetworked); m_bScriptMigration = b; }

	bool GetExistsOnAllMachines() const	{ return m_bExistsOnAllMachines; }
	void SetExistsOnAllMachines(bool b) { Assert(m_bIsNetworked); m_bExistsOnAllMachines = b; }

	bool GetStopCloning() const			{ return m_bStopCloning; }
	void SetStopCloning(bool b)			{ Assert(m_bIsNetworked); m_bStopCloning = b; }

	bool GetScriptObjectWasGrabbedFromTheWorld() const		{ return m_bScriptObjectWasGrabbedFromTheWorld; }
	void SetScriptObjectWasGrabbedFromTheWorld(bool b)		{ m_bScriptObjectWasGrabbedFromTheWorld = b; }

	strIndex GetStreamingIndex() const;
	void GetMemoryUsage(u32& virtualSize, u32& physicalSize, const strIndex* ignoreList=NULL, s32 numIgnores=0, u32 skipFlag = STRFLAG_DONTDELETE) const;

#if __BANK
	void DisplayDebugInfo(const char* scriptName) const;
	void SpewDebugInfo(const char* scriptName) const;
	static void PoolFullCallback(void* pItem);
#endif	//	__DEV

#if __SCRIPT_MEM_DISPLAY
	void DisplayMemoryUsage(const char* scriptName, const strIndex* ignoreList=NULL, s32 numIgnores=0, u32 skipFlag = STRFLAG_DONTDELETE) const;
#endif	//	__SCRIPT_MEM_DISPLAY

private:

#if __SCRIPT_MEM_DISPLAY
	char* GetEntityTypeString() const;
#endif //	__SCRIPT_MEM_DISPLAY


	CGameScriptObjInfo	m_ScriptInfo;
	CPhysical*			m_pTargetEntity;	// this must be a physical, because it must be a ped/vehicle/object and needs to have a network object ptr.
	scriptHandler*		m_pHandler;			// the script handler that created this script object	

	bool    m_bCleanupTargetEntity : 1;	// set when we want to cleanup the script state of a target entity when it is unregistered from a script handler
	bool	m_bNoLongerNeeded : 1;		// set when the target entity has been marked as no longer needed by the script		

	// cached network state used when no network game is running
	bool	m_bIsNetworked : 1;
	bool	m_bCanMigrate : 1;
	bool	m_bScriptMigration : 1;
	bool	m_bExistsOnAllMachines : 1;
	bool	m_bStopCloning : 1;
	bool	m_bScriptObjectWasGrabbedFromTheWorld : 1;
};

#endif // SCRIPT_ENTITY_H
