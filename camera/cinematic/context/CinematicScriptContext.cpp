//
// camera/cinematic/CinematicScriptContext.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/context/CinematicScriptContext.h"
#include "camera/system/CameraMetadata.h"
#include "camera/cinematic/shot/CinematicScriptShot.h"

INSTANTIATE_RTTI_CLASS(camCinematicScriptContext,0x965EE0FD)
INSTANTIATE_RTTI_CLASS(camCinematicScriptedRaceCheckPointContext, 0xe9e69f2a)
INSTANTIATE_RTTI_CLASS(camCinematicScriptedMissionCreatorFailContext, 0xe74a6359)

CAMERA_OPTIMISATIONS()

camCinematicScriptContext::camCinematicScriptContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicScriptContextMetadata&>(metadata))
, m_Duration(0)
, m_ScriptThreadId(THREAD_INVALID)
, m_IsShotActive(false)
{
}

bool camCinematicScriptContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const
{
	if(m_Shots.GetCount() > 0)
	{
		return true; 
	}
	return false;
}

bool camCinematicScriptContext::UpdateShotSelector()
{
	//Investigate moving this to later so we update on the first frame a new shot is created
	if(m_Shots.GetCount() == 0)
	{
		return false; 
	}

	PreUpdateShots(); 
	
	bool CanUpdate = false; 

	if((m_CurrentShotIndex == -1) || (m_Metadata.m_CanSelectShotFromControlInput && camInterface::GetCinematicDirector().ShouldChangeCinematicShot()))
	{
		if(m_Shots[0]->IsValid())
		{
			u32 camRef = m_Shots[0]->GetCameraRef(); 
			if(camRef > 0)
			{
				CanUpdate = m_Shots[0]->CreateAndTestCamera(camRef);
				if(CanUpdate)
				{
					m_CurrentShotIndex = 0; 
					m_PreviousCameraChangeTime = fwTimer::GetTimeInMilliseconds();
				}
			}
		}
	}
	else
	{
		u32 time = fwTimer::GetTimeInMilliseconds();

		u32 timeSinceLastChange	= time - m_PreviousCameraChangeTime;

		if(timeSinceLastChange <= GetDuration() )
		{
			CanUpdate = true;
		}
		else
		{
			ClearContext(); 
			DeleteCinematicShot(); 
			CanUpdate = false; 
		}
	}

	return CanUpdate; 
}

bool camCinematicScriptContext::CreateScriptCinematicShot(u32 ShotRef, u32 Duration, scrThreadId scriptThreadId, const CEntity* pAttchEntity, const CEntity* pLookAtEntity)
{
	if(m_ScriptThreadId != THREAD_INVALID && scriptThreadId != m_ScriptThreadId)
	{	
		const char* pThreadName = ""; 
		const char* pNewThreadName = ""; 

		scrThread* pThread = scrThread::GetThread(m_ScriptThreadId); 
		if(pThread)
		{
			pThreadName = pThread->GetScriptName(); 
		}
	
		scrThread* pNewThread = scrThread::GetThread(scriptThreadId); 
		
		if(pNewThread)
		{
			pNewThreadName = pNewThread->GetScriptName(); 
		}
		
		cameraAssertf(0, "CreateScriptCinematicShot: Script: %s is not allowed to create a new cinematc shot because script: %s already created one", pNewThreadName, pThreadName);
		
		return false; 
	}
	
	ClearContext(); 
	DeleteCinematicShot(); 

	atHashWithStringNotFinal shotHash(ShotRef); 
	
	camBaseCinematicShot* pShot = camFactory::CreateObject<camBaseCinematicShot>(shotHash);

	if(cameraVerifyf(pShot, "Failed to create a shot (shot created from script, hash: %u)",shotHash.GetHash()))
		//SAFE_CSTRING(shotHash.GetHash()))
	{
		m_Shots.Grow() = pShot;
		pShot->Init(this, 0, 1.0f); 
		m_Duration = Duration; 
		m_ScriptThreadId = scriptThreadId; 

		pShot->SetAttachEntity(pAttchEntity); 
		pShot->SetLookAtEntity(pLookAtEntity); 
		return true; 
	}
	return false; 
}

bool camCinematicScriptContext::DeleteCinematicShot(u32 ShotRef,scrThreadId scriptThreadId )
{
	if(m_Shots.GetCount() > 0)
	{
		if(m_ScriptThreadId != THREAD_INVALID && scriptThreadId != m_ScriptThreadId)
		{	
			const char* pThreadName = ""; 
			const char* pNewThreadName = ""; 

			scrThread* pThread = scrThread::GetThread(m_ScriptThreadId); 
			if(pThread)
			{
				pThreadName = pThread->GetScriptName(); 
			}

			scrThread* pNewThread = scrThread::GetThread(scriptThreadId); 

			if(pNewThread)
			{
				pNewThreadName = pNewThread->GetScriptName(); 
			}

			cameraAssertf(0, "Script %s is not allowed to delete the current cinematc shot created by script %s", pNewThreadName, pThreadName);

			return false; 
		}
		
		if(m_Shots[0]->GetNameHash() == ShotRef)
		{
			ClearContext(); 
			DeleteCinematicShot(); 
			return true;
		}
	}
	return true; 
}

void camCinematicScriptContext::DeleteCinematicShot()
{
	if(m_Shots.GetCount() > 0)
	{
		delete m_Shots[0]; 
		
		m_Shots[0] = NULL; 
		m_Duration = 0; 
		m_Shots.Delete(0); 
		m_ScriptThreadId = THREAD_INVALID; 
	}
}

bool camCinematicScriptContext::UpdateContext()
{
	scrThread* pThread = scrThread::GetThread(m_ScriptThreadId); 
	
	if(pThread)
	{
		m_IsShotActive = camBaseCinematicContext::UpdateContext(); 
	}
	else
	{
		ClearContext(); 
		DeleteCinematicShot(); 
	}
	
	return m_IsShotActive; 
}

bool camCinematicScriptContext::IsScriptCinematicShotActive(u32 ShotRef) const
{
	if(m_Shots.GetCount() > 0)
	{
		if(m_Shots[0] && m_Shots[0]->GetNameHash() == ShotRef)
		{
			return m_IsShotActive; 
		}
	}
	return false; 
}


void camCinematicScriptContext::ClearContext()
{
	m_IsShotActive = false; 
	camBaseCinematicContext::ClearContext(); 
	DeleteCinematicShot(); 
}

camCinematicScriptedRaceCheckPointContext::camCinematicScriptedRaceCheckPointContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicScriptedRaceCheckPointContextMetadata&>(metadata))
, m_TimePositionWasRegistered(0)
, m_IsValid(false)
{
}


bool camCinematicScriptedRaceCheckPointContext::RegisterPosition(const Vector3& pos)
{
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		if(m_Shots[i]->GetIsClassId(camCinematicScriptRaceCheckPointShot::GetStaticClassId()))
		{
			if(m_Shots[i] && !m_Shots[i]->GetCamera() && fwTimer::GetTimeInMilliseconds() - m_TimePositionWasRegistered > m_Metadata.m_ValidRegistartionDuration)
			{
				camCinematicScriptRaceCheckPointShot* ScriptShot = static_cast<camCinematicScriptRaceCheckPointShot*>(m_Shots[i]); 
				
				ScriptShot->SetPosition(pos); 
				m_TimePositionWasRegistered = fwTimer::GetTimeInMilliseconds();
				m_IsValid = true; 
				return true; 
			}
			return false; 
		}
	}
	return false;
}

void camCinematicScriptedRaceCheckPointContext::PreUpdate()
{
	if(fwTimer::GetTimeInMilliseconds() - m_TimePositionWasRegistered > m_Metadata.m_ValidRegistartionDuration)
	{
		if(m_Shots[0] && !m_Shots[0]->GetCamera())
		{
			m_IsValid = false; 
		}
	}

}

bool camCinematicScriptedRaceCheckPointContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput = true), bool UNUSED_PARAM(shouldConsiderViewMode = true)) const
{
	if(camInterface::GetCinematicDirector().ShouldChangeCinematicShot())
	{
		return false; 
	}

	return m_IsValid; 
}

void camCinematicScriptedRaceCheckPointContext::ClearContext()
{
	m_IsValid = false; 
	m_TimePositionWasRegistered = 0; 

	camBaseCinematicContext::ClearContext(); 
}

camCinematicScriptedMissionCreatorFailContext::camCinematicScriptedMissionCreatorFailContext(const camBaseObjectMetadata& metadata)
: camBaseCinematicContext(metadata)
, m_Metadata(static_cast<const camCinematicScriptedMissionCreatorFailContextMetadata&>(metadata))
{
}

bool camCinematicScriptedMissionCreatorFailContext::IsValid(bool UNUSED_PARAM(shouldConsiderControlInput), bool UNUSED_PARAM(shouldConsiderViewMode)) const 
{
	if(camInterface::GetCinematicDirector().CanActivateMissionCreatorFailContext())
	{
		return true; 
	}

	return false;
}

void camCinematicScriptedMissionCreatorFailContext::ClearContext()
{
	m_ShotHistory.Reset(); 

	camBaseCinematicContext::ClearContext(); 
}