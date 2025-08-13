#include "script\handlers\GameScriptEntity.h"

#include "network/Debug/NetworkDebug.h"
#include "network/Objects/Entities/NetObjPhysical.h"
#include "Objects/object.h"
#include "peds/ped.h"
#include "scene/Physical.h"
#include "script/handlers/GameScriptHandlerNetwork.h"
#include "script/handlers/GameScriptResources.h"
#include "script/script_channel.h"
#include "script/script.h"
#include "script/script_debug.h"

SCRIPT_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

// ================================================================================================================
// CGameScriptHandlerObject - an base class used by all different types of script handler objects
// ================================================================================================================

// Determine whether the object needs to be cleaned up properly. This is not done if the object is still required to persist as a script object
// for the remaining participants, who may still be running the script normally.
bool CGameScriptHandlerObject::IsCleanupRequired() const
{
	netLoggingInterface* log = CTheScripts::GetScriptHandlerMgr().GetLog();

	char logName[30];
	char eventName[20];

	logName[0] = 0;
	eventName[0] = 0;

	GetScriptInfo()->GetLogName(logName, 30);
	sprintf(eventName, "DETACH_%s", GetLogName());

	scriptHandler* pHandler = GetScriptHandler();
	CNetObjGame* pNetObject = static_cast<CNetObjGame*>(GetNetObject());

	gnetAssert(GetScriptInfo());
	gnetAssert(pHandler);

	if (!GetScriptInfo() || !pHandler || !pNetObject || !pHandler->GetNetworkComponent())
		return true;

	bool bObjectIsClone			= pNetObject->IsClone() || pNetObject->IsPendingOwnerChange();
	bool bCloneBecomingAmbient	= pNetObject->IsClone() && !pNetObject->IsScriptObject(); // this happens when we get an update for a clone, clearing its script state	
	bool bScriptHasFinished		= pHandler->GetNetworkComponent()->GetScriptHasFinished();

	if (!bCloneBecomingAmbient)
	{
		if (GetScriptInfo()->IsScriptHostObject())
		{
			bool bRemainingParticipants	= false;

			// if the handler is terminating, the current participants can be out of date, if so use the remote script info instead
			if (pHandler->GetNetworkComponent()->GetState() == scriptHandlerNetComponent::NETSCRIPT_PLAYING)
			{
				bRemainingParticipants = pHandler->GetNetworkComponent()->GetNumParticipants()>1;
			}
			else
			{
				CGameScriptHandlerMgr::CRemoteScriptInfo* pInfo = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(GetScriptInfo()->GetScriptId(), true);

				bRemainingParticipants = (pInfo != 0);
			}

			// leave the script state intact if the object is a clone or if the script is still running with active participants and the object has not been
			// marked as no longer needed.
			bool bLeaveScriptStateOnEntity = (bRemainingParticipants && !GetNoLongerNeeded() && !bScriptHasFinished);

			if (bObjectIsClone || bLeaveScriptStateOnEntity)
			{
				if (GetScriptInfo() && log)
				{
					NetworkLogUtils::WriteLogEvent(*log, eventName, "%s      %s", GetScriptInfo()->GetScriptId().GetLogName(), logName);
					log->WriteDataValue("Script id", "%d", GetScriptInfo()->GetObjectId());
					log->WriteDataValue("Net Object", "%s", pNetObject->GetLogName());
					log->WriteDataValue("Clone", "%s", bObjectIsClone ? "true" : "false");
					log->WriteDataValue("Other participants", "%s", bRemainingParticipants ? "true" : "false");
					log->WriteDataValue("No longer needed", "%s", GetNoLongerNeeded() ? "true" : "false");
					log->WriteDataValue("Script finished", "%s", bScriptHasFinished ? "true" : "false");
				}

				// flag the object to be cleaned up when it becomes local again (it may have a pending owner but not migrate)
				if (bObjectIsClone && !bLeaveScriptStateOnEntity)
				{
					log->WriteDataValue("Flag for cleanup when local", "true");
					pNetObject->SetLocalFlag(CNetObjGame::LOCALFLAG_NOLONGERNEEDED, true);
				}

				return false;
			}
		}
		else if (GetScriptInfo()->IsScriptClientObject())
		{
			bool bParticipantStillActive = false;

			if (!GetNoLongerNeeded() && !bScriptHasFinished)
			{
				// if we have a client created object for an existing participant, we must leave it as a script entity
				int participantSlot = GET_SLOT_FROM_SCRIPT_OBJECT_ID(GetScriptInfo()->GetObjectId());
				const netPlayer* pParticipant = pHandler->GetNetworkComponent()->GetParticipantUsingSlot(participantSlot);

				bParticipantStillActive = (pParticipant && !pParticipant->IsLocal() && !pParticipant->IsLeaving());
			}

			if (bParticipantStillActive || bObjectIsClone)
			{
				if (GetScriptInfo() && log)
				{
					NetworkLogUtils::WriteLogEvent(*log, eventName, "%s      %s", GetScriptInfo()->GetScriptId().GetLogName(), logName);
					log->WriteDataValue("Script id", "%d", GetScriptInfo()->GetObjectId());
					log->WriteDataValue("Net Object", "%s", pNetObject->GetLogName());
					log->WriteDataValue("Clone", "%s", bObjectIsClone ? "true" : "false");
					log->WriteDataValue("Participant still active", "%s", bParticipantStillActive ? "true" : "false");
				}

				return false;
			}
		}
	}

	return true;
}

// ================================================================================================================
// CScriptEntityExtension - an extension given to an entity that is a proper mission entity.
// ================================================================================================================

AUTOID_IMPL(CScriptEntityExtension);

FW_INSTANTIATE_CLASS_POOL(CScriptEntityExtension, CONFIGURED_FROM_FILE, atHashString("CScriptEntityExtension",0xef7129cb));

CScriptEntityExtension::CScriptEntityExtension(CPhysical& targetEntity)
: m_pTargetEntity(&targetEntity)
, m_pHandler(NULL)
, m_bCleanupTargetEntity(true)
, m_bNoLongerNeeded(false)
, m_bIsNetworked(true)
, m_bCanMigrate(true)
, m_bScriptMigration(false)
, m_bExistsOnAllMachines(false)
, m_bStopCloning(false)
, m_bScriptObjectWasGrabbedFromTheWorld(false)
{
	// add this extension to the entity's extension list
	targetEntity.GetExtensionList().Add(*this);

	if (targetEntity.GetNetworkObject())
	{
		static_cast<CNetObjPhysical*>(targetEntity.GetNetworkObject())->SetScriptExtension(this);
	}
}

CScriptEntityExtension::~CScriptEntityExtension()
{
	if (m_pTargetEntity && m_pTargetEntity->GetNetworkObject())
	{
		static_cast<CNetObjPhysical*>(m_pTargetEntity->GetNetworkObject())->SetScriptExtension(NULL);
	}
}

void CScriptEntityExtension::CreateScriptInfo(const scriptIdBase& scrId, const ScriptObjectId& objectId, const HostToken hostToken)
{
	m_ScriptInfo = CGameScriptObjInfo(scrId, objectId, hostToken);
	Assert(m_ScriptInfo.IsValid());
}

void CScriptEntityExtension::SetScriptInfo(const scriptObjInfoBase& info)
{
	m_ScriptInfo = info;
	Assert(m_ScriptInfo.IsValid());
}

scriptObjInfoBase*	CScriptEntityExtension::GetScriptInfo()  
{ 
	if (m_ScriptInfo.IsValid())
		return static_cast<scriptObjInfoBase*>(&m_ScriptInfo); 

	return NULL;
}

const scriptObjInfoBase*	CScriptEntityExtension::GetScriptInfo() const 
{ 
	if (m_ScriptInfo.IsValid())
		return static_cast<const scriptObjInfoBase*>(&m_ScriptInfo); 

	return NULL;
}

void CScriptEntityExtension::SetScriptHandler(scriptHandler* handler) 
{ 	
	FatalAssert(!handler || handler->GetScriptId() == m_ScriptInfo.GetScriptId());
	m_pHandler = handler; 
}

void CScriptEntityExtension::OnRegistration(bool newObject, bool BANK_ONLY(hostObject))
{
	if (AssertVerify(m_pTargetEntity))
	{
		CNetObjGame* netObj = static_cast<CNetObjGame*>(GetNetObject());

#if __BANK
		scriptDisplayf("%s : Register %s script entity with model %s (Entity = 0x%p)", m_pHandler->GetLogName(), newObject ? "new" : "existing", m_pTargetEntity->GetModelName(), m_pTargetEntity);

		if (NetworkInterface::IsGameInProgress())
		{
			netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

			if(log)
			{
				char logName[30];
				char typeName[20];
				char header[100];

				m_ScriptInfo.GetLogName(logName, 30);

				if (m_pTargetEntity->GetIsTypePed())
				{
					formatf(typeName, "PED");
				}
				else if (m_pTargetEntity->GetIsTypeVehicle())
				{
					formatf(typeName, "VEHICLE");
				}
				else if (m_pTargetEntity->GetIsTypeObject())
				{
					if (((CObject*)m_pTargetEntity)->m_nObjectFlags.bIsPickUp)
					{
						formatf(typeName, "PICKUP");
					}
					else
					{
						formatf(typeName, "OBJECT");
					}
				}
				else
				{
					formatf(typeName, "ENTITY");
				}

				formatf(header, "REGISTER_%s_%s_%s", newObject ? "NEW" : "EXISTING", hostObject ? "HOST" : "LOCAL", typeName);

				NetworkLogUtils::WriteLogEvent(*log, header, "%s      %s", m_ScriptInfo.GetScriptId().GetLogName(), logName);

				log->WriteDataValue("Script id", "%d", m_ScriptInfo.GetObjectId());
				log->WriteDataValue("Entity", "%p", m_pTargetEntity);
	
				if (netObj)
				{
					log->WriteDataValue("Net Object",	"%s", netObj->GetLogName());
				}	
			}
		}
#endif // __BANK

		if (newObject)
		{
			m_pTargetEntity->SetupMissionState();

			if (m_pTargetEntity->GetNetworkObject())
			{
				Assertf(m_pHandler->GetNetworkComponent(), "Script %s is not networked but is creating networked objects. This is not permitted.", m_pHandler->GetScriptId().GetLogName());
			}
		}

		if (netObj)
		{
			netObj->OnConversionToScriptObject();
		}
	}
}

void CScriptEntityExtension::OnUnregistration()
{
	if (AssertVerify(m_pTargetEntity))
	{
		scriptDisplayf("%s : Unregister script entity with model %s (Entity = 0x%p)", m_ScriptInfo.GetScriptId().GetLogName(), m_pTargetEntity->GetModelName(), m_pTargetEntity);

#if __ASSERT
		//B*2222339 Temporary debug to trap places when a network player clone ped is removed and report the script stack
		if (NetworkInterface::IsGameInProgress() && m_pTargetEntity->GetIsTypePed())
		{
			if(strstr(m_pTargetEntity->GetModelName(),"_freemode_"))
			{
				scriptDebugf1("%s UNREGISTER_ENTITY Network Freemode model: %s unregistered [ %p ]",
					m_ScriptInfo.GetScriptId().GetLogName(),
					m_pTargetEntity->GetModelName(),
					m_pTargetEntity);
				scrThread::PrePrintStackTrace();
			}
		}
#endif

		if (m_pTargetEntity->GetNetworkObject())
		{
			NETWORK_DEBUG_BREAK_FOR_FOCUS(BREAK_TYPE_FOCUS_NO_LONGER_NEEDED, *m_pTargetEntity->GetNetworkObject());		
		}

		netLoggingInterface *log = CTheScripts::GetScriptHandlerMgr().GetLog();

		char logName[30];
		m_ScriptInfo.GetLogName(logName, 30);

		if (log)
		{
			NetworkLogUtils::WriteLogEvent(*log, "UNREGISTER_ENTITY", "%s      %s", m_ScriptInfo.GetScriptId().GetLogName(), logName);

			log->WriteDataValue("Script id", "%d", m_ScriptInfo.GetObjectId());

			CNetObjGame* pNetObj = m_pTargetEntity->GetNetworkObject();

			if (pNetObj)
			{
				log->WriteDataValue("Net Object","%s", pNetObj ? pNetObj->GetLogName() : "-none-");
				log->WriteDataValue("No longer needed",	"%s", m_bNoLongerNeeded ? "true" : "false");
				log->WriteDataValue("Cleanup",	"%s", m_bCleanupTargetEntity ? "true" : "false");
			}
		}
	}
}

void CScriptEntityExtension::Cleanup()
{
	CPhysical* pPhysical = GetTargetEntity();

	netLoggingInterface* log = CTheScripts::GetScriptHandlerMgr().GetLog();

	if(log)
	{
		char logName[30];
		logName[0] = 0;

		if (gnetVerify(GetScriptInfo()))
		{
			GetScriptInfo()->GetLogName(logName, 30);
		}

		NetworkLogUtils::WriteLogEvent(*log, "CLEANUP_ENTITY", "%s      %s", GetScriptInfo()->GetScriptId().GetLogName(), logName);

		if (GetScriptInfo())
		{
			log->WriteDataValue("Script id", "%d", GetScriptInfo()->GetObjectId());
		}

		log->WriteDataValue("Net Object", "%s", (m_pTargetEntity && m_pTargetEntity->GetNetworkObject()) ? m_pTargetEntity->GetNetworkObject()->GetLogName() : "?");
	}

	m_ScriptInfo.Invalidate();
	m_pHandler = NULL;

	if (gnetVerify(pPhysical))
	{
		// GetCleanupTargetEntity is false, the object is about to be deleted, so there is no need to cleanup the mission state 
		if (GetCleanupTargetEntity())
		{
			if(log)
			{
				log->WriteDataValue("Mission state cleaned up", "true");
			}

			pPhysical->CleanupMissionState();

			Assert(!pPhysical->PopTypeIsMission());
		}
		else
		{
			if(log)
			{
				log->WriteDataValue("Mission state cleaned up", "false");
			}
		}
	}
}

netObject* CScriptEntityExtension::GetNetObject() const 
{ 
	return m_pTargetEntity ? m_pTargetEntity->GetNetworkObject() : NULL; 
}

fwEntity* CScriptEntityExtension::GetEntity() const 
{ 
	return static_cast<fwEntity*>(m_pTargetEntity); 
}

strIndex CScriptEntityExtension::GetStreamingIndex() const
{
	if (m_pTargetEntity)
	{
		strLocalIndex localIndex = CModelInfo::LookupLocalIndex(m_pTargetEntity->GetModelId());
		strIndex streamingIndex = CModelInfo::GetStreamingModule()->GetStreamingIndex(localIndex);
		return(streamingIndex);
	}

	return strIndex(strIndex::INVALID_INDEX);
}

void CScriptEntityExtension::GetMemoryUsage(u32& virtualSize, u32& physicalSize, const strIndex* ignoreList, s32 numIgnores, u32 skipFlag) const
{
	if (m_pTargetEntity)
	{
// #if !__FINAL
// 		if (CScriptDebug::GetCountModelUsage())
// #endif
		{
			strIndex streamingIndex = GetStreamingIndex();

			if(!(strStreamingEngine::GetInfo().GetObjectFlags(streamingIndex) & skipFlag))
			{
				strStreamingEngine::GetInfo().GetObjectAndDependenciesSizes(streamingIndex, virtualSize, physicalSize, ignoreList, numIgnores, true);
			}
		}
	}
}

#if __BANK
void CScriptEntityExtension::DisplayDebugInfo(const char* scriptName) const
{
	if (m_pTargetEntity)
	{
		const Vector3 vTargetEntityPosition = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetPosition());
		if (scriptName)
		{
			grcDebugDraw::AddDebugOutput("%s: %s %s (%f, %f, %f)\n", 
										scriptName, GetEntityTypeString(), 
										CModelInfo::GetBaseModelInfoName(m_pTargetEntity->GetModelId()), 
										vTargetEntityPosition.x, vTargetEntityPosition.y, vTargetEntityPosition.z);
		}
		else
		{
			grcDebugDraw::AddDebugOutput("%s %s (%f, %f, %f)\n", 
										GetEntityTypeString(), 
										CModelInfo::GetBaseModelInfoName(m_pTargetEntity->GetModelId()), 
										vTargetEntityPosition.x, vTargetEntityPosition.y, vTargetEntityPosition.z);
		}
	}
}

void CScriptEntityExtension::SpewDebugInfo(const char* scriptName) const
{
	if (m_pTargetEntity)
	{
		const Vector3 vTargetEntityPosition = VEC3V_TO_VECTOR3(m_pTargetEntity->GetTransform().GetPosition());
		if (scriptName)
		{
			Displayf("%s: %s %s (%f, %f, %f)\n", 
					scriptName, GetEntityTypeString(), 
					CModelInfo::GetBaseModelInfoName(m_pTargetEntity->GetModelId()), 
					vTargetEntityPosition.x, vTargetEntityPosition.y, vTargetEntityPosition.z);
		}
		else
		{
			Displayf("%s %s (%f, %f, %f)\n", 
				GetEntityTypeString(), 
				CModelInfo::GetBaseModelInfoName(m_pTargetEntity->GetModelId()), 
				vTargetEntityPosition.x, vTargetEntityPosition.y, vTargetEntityPosition.z);
		}
	}
}
#endif	//	__BANK

#if __SCRIPT_MEM_DISPLAY
void CScriptEntityExtension::DisplayMemoryUsage(const char* scriptName, const strIndex* ignoreList, s32 numIgnores, u32 skipFlag) const
{
	if (m_pTargetEntity)	//	 && CScriptDebug::GetCountModelUsage())
	{
		u32 virtualSize = 0;
		u32 physicalSize = 0;

		GetMemoryUsage(virtualSize, physicalSize, ignoreList, numIgnores, skipFlag);

		if (virtualSize > 0 || physicalSize > 0)
		{
			bool bDDFlagSet = false;
			if(strStreamingEngine::GetInfo().GetObjectFlags(GetStreamingIndex()) & STRFLAG_DONTDELETE)
			{
				bDDFlagSet = true;
			}

			grcDebugDraw::AddDebugOutput("%s: %s %s Model %dK %dK %s(for this entity, including deps)", 
									  scriptName,
									  GetEntityTypeString(),
									  CModelInfo::GetBaseModelInfoName(m_pTargetEntity->GetModelId()),
									  virtualSize>>10, physicalSize>>10,
									  bDDFlagSet?"(DD)":"");
		}
	}
}

char* CScriptEntityExtension::GetEntityTypeString() const
{
	static char typeStr[10];

	if (m_pTargetEntity)
	{
		switch (m_pTargetEntity->GetType())
		{
		case ENTITY_TYPE_VEHICLE:
			sprintf(typeStr, "Vehicle");
			break;
		case ENTITY_TYPE_PED:
			sprintf(typeStr, "Ped");
			break;
		case ENTITY_TYPE_OBJECT:
			sprintf(typeStr, "Object");
			break;
		default:
			Assert(0);
		}
	}
	else
	{
		sprintf(typeStr, "<invalid>");
	}

	return typeStr;
}
#endif // __SCRIPT_MEM_DISPLAY

#if __DEV
void CScriptEntityExtension::PoolFullCallback(void* pItem)
{
	if (!pItem)
	{
		Printf("ERROR - CScriptEntityExtension Pool FULL!\n");
	}
	else
	{
		CScriptEntityExtension* pScriptEntityExt = static_cast<CScriptEntityExtension*>(pItem);
		const char *pDebugName = pScriptEntityExt->m_pTargetEntity->GetDebugName();
		const char *pName = (pDebugName && pDebugName[0] != '\0') ? pDebugName : pScriptEntityExt->m_pTargetEntity->GetModelName();
		const char *pScriptName = pScriptEntityExt->m_pHandler ? pScriptEntityExt->m_pHandler->GetLogName() : '\0';

		Displayf("Entity: %s, Type: %s, Script: \"%s\", Model Index: %u, Memory Address: %p", 
				 pName, pScriptEntityExt->GetEntityTypeString(), pScriptName, pScriptEntityExt->m_pTargetEntity->GetModelId().GetModelIndex(), pScriptEntityExt);
	}
}
#endif // __DEV
