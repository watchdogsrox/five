//
// camera/cinematic/context/BaseCinematicContext.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/context/BaseCinematicContext.h"

#include "atl/vector.h"

#include "camera/cinematic/CinematicDirector.h"
#include "camera/cinematic/context/CinematicScriptContext.h"
#include "camera/gameplay/aim/AimCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/system/CameraMetadata.h"
#include "fwmaths/random.h"
#include "system/control.h"

INSTANTIATE_RTTI_CLASS(camBaseCinematicContext,0x28F0D976)

CAMERA_OPTIMISATIONS()

const u32 g_GameCameraFallBackDurationForSpectating = 5000;

//atVector<u32> camBaseCinematicContext::m_ShotHistory;
atQueue<u32, CINEMATIC_SHOT_HISTORY_LENGTH> camBaseCinematicContext::m_ShotHistory; 

camBaseCinematicContext::camBaseCinematicContext(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camCinematicContextMetadata&>(metadata))
, m_PreviousCameraChangeTime(0)
, m_CurrentShotIndex(-1)
, m_IsRunningGameCameraAsFallBack(false)
{
	const int numShots = m_Metadata.m_Shots.GetCount();
	for(int i=0; i<numShots; i++)
	{
		if(m_Metadata.m_Shots[i].m_Shot)
		{
			camBaseCinematicShot* pShot = camFactory::CreateObject<camBaseCinematicShot>(m_Metadata.m_Shots[i].m_Shot);
	
			if(cameraVerifyf(pShot, "Failed to create a context (name: %s, hash: %u)",
				SAFE_CSTRING(m_Metadata.m_Shots[i].m_Shot.GetCStr()), m_Metadata.m_Shots[i].m_Shot.GetHash()))
			{
				m_Shots.Grow() = pShot;
				pShot->Init(this, m_Metadata.m_Shots[i].m_Priority, m_Metadata.m_Shots[i].m_ProbabilityWeighting); 
			}
		}
	}
}

camBaseCinematicContext::~camBaseCinematicContext()
{
	
}

CameraSelectorStatus camBaseCinematicContext::CanUpdate()
{
	CameraSelectorStatus currentStatus = CSS_DONT_CHANGE_CAMERA; 

	if(!m_IsRunningGameCameraAsFallBack)
	{
		if(!GetCamera() /*||  !IsCameraValid()*/ || !m_Shots[m_CurrentShotIndex]->IsValid())
		{
			currentStatus = CSS_MUST_CHANGE_CAMERA; 
		}
	}

	return currentStatus; 
}

camBaseCamera* camBaseCinematicContext::GetCamera() 
{
	if(m_CurrentShotIndex >= 0 && m_CurrentShotIndex < m_Shots.GetCount())	
	{
		return m_Shots[m_CurrentShotIndex]->GetCamera(); 
	}
	return NULL; 
}
	
bool camBaseCinematicContext::IsCameraValid()
{
	if(m_CurrentShotIndex >= 0 && m_CurrentShotIndex < m_Shots.GetCount())	
	{
		if(m_Shots[m_CurrentShotIndex]->GetCamera()->IsValid())
		{
			return true;
		}
	}
	return false; 
}

u32 camBaseCinematicContext::GetNumberOfPrefferedShotsForInputType(u32 InputType)
{
	u32 Shots = 0; 

	for(int i = 0; i < m_Metadata.m_PreferredShotToInputSelection.GetCount(); i ++)
	{
		if((u32)m_Metadata.m_PreferredShotToInputSelection[i].m_InputType == InputType)
		{
			Shots++; 
		}
	}

	return Shots; 
}

void camBaseCinematicContext::GetShotHashesForInputType(u32 InputType, atArray<u32>& Shots) const
{
	for(int i = 0; i < m_Metadata.m_PreferredShotToInputSelection.GetCount(); i ++)
	{
		if((u32)m_Metadata.m_PreferredShotToInputSelection[i].m_InputType == InputType)
		{
			Shots.PushAndGrow(m_Metadata.m_PreferredShotToInputSelection[i].m_Shot.GetHash()); 
		}
	}
};

void camBaseCinematicContext::CreateRandomIndexArray(u32 SizeOfArray, atArray<u32>& RandomIndexs)
{
	for (int i = 0; i < SizeOfArray; ++i)
	{
		RandomIndexs.PushAndGrow(i);
	}

	bool swapNumbers = true; 

	for (int i = RandomIndexs.GetCount() - 1; i > 0; --i) 
	{
		if(RandomIndexs.GetCount() == 2)
		{
			if(fwRandom::GetRandomNumberInRange(0 , 10) > 5)
			{
				swapNumbers = false; 
			}
		}

		if(swapNumbers)
		{
			// generate random index
			int w = rand()%i;
			// swap items
			int t = RandomIndexs[i];
			RandomIndexs[i] = RandomIndexs[w];
			RandomIndexs[w] = t;
		}
	}
}


s32 camBaseCinematicContext::SelectShot(bool canUseSameShotTwice)
{
	atVector<u32> SelectableShots;  

	s32 ShotIndex = -1; 

	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		if(m_Shots[i]->IsValid())
		{
			SelectableShots.PushAndGrow(i); 
		}
	}
	u32 NumberOfAvailibleShots = SelectableShots.GetCount(); 
	u32 m_LastShotHash = 0; 
	
	if(!m_ShotHistory.IsEmpty())
	{
		m_LastShotHash = m_ShotHistory.Top(); // [m_ShotHistory.GetCount()-1]; 
	}

	//if a override has been specified then lets try that one first ignore shot history
	if(m_ListOfCamerasAttemptedToCreate.GetCount() == 0)
	{
		if(m_Metadata.m_CanSelectShotFromControlInput && camInterface::GetCinematicDirector().GetUsePreferredMode() != SELECTION_INPUT_NONE)
		{
			u32 NumberOfPreferredShots = GetNumberOfPrefferedShotsForInputType(camInterface::GetCinematicDirector().GetUsePreferredMode()); 
			
			if( NumberOfPreferredShots == 1 )
			{
				atArray<u32> preferredShotHashes; 
				GetShotHashesForInputType(camInterface::GetCinematicDirector().GetUsePreferredMode(), preferredShotHashes);
				
				for(int i=0; i < SelectableShots.GetCount(); i++)
				{
					if(m_Shots[SelectableShots[i]]->m_Metadata.m_Name.GetHash() == preferredShotHashes[0])	
					{
						ShotIndex = SelectableShots[i]; 	
						break; 
					}
				}

				if(ShotIndex != -1)
				{
					camInterface::GetCinematicDirector().SetShouldUsePreferredShot(true);
					return ShotIndex; 

				}
				else
				{
					camInterface::GetCinematicDirector().SetShouldUsePreferredShot(false); 
				}
			}
			else if(NumberOfPreferredShots > 1)
			{
				//lets pick a shot based on history 
				atArray<u32> preferredShotHashes; 
				GetShotHashesForInputType(camInterface::GetCinematicDirector().GetUsePreferredMode(), preferredShotHashes);
				
				atArray<u32> RandomIndexs; 

				CreateRandomIndexArray(NumberOfPreferredShots, RandomIndexs);
				
				//iterate over the array and dont pick the same shot we had last time
				for(int i=0; i < RandomIndexs.GetCount(); i++)
				{
					//dont pick the last historical shot
					if(preferredShotHashes[RandomIndexs[i]] != m_LastShotHash)
					{
						for(int j=0; j < SelectableShots.GetCount(); j++)
						{
							//check that the preferred shot is in our availible shots list
							if(m_Shots[SelectableShots[j]]->m_Metadata.m_Name.GetHash() == preferredShotHashes[RandomIndexs[i]])	
							{
								ShotIndex = SelectableShots[j]; 	
								
								if(ShotIndex != -1)
								{
									camInterface::GetCinematicDirector().SetShouldUsePreferredShot(true);
									return ShotIndex; 
								}
								else
								{
									camInterface::GetCinematicDirector().SetShouldUsePreferredShot(false); 
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		if(m_Metadata.m_CanSelectShotFromControlInput && camInterface::GetCinematicDirector().GetUsePreferredMode() != SELECTION_INPUT_NONE)
		{
			u32 NumberOfPreferredShots = GetNumberOfPrefferedShotsForInputType(camInterface::GetCinematicDirector().GetUsePreferredMode()); 
			
			if(NumberOfPreferredShots > 1)
			{
				atArray<u32> preferredShotHashes; 
				GetShotHashesForInputType(camInterface::GetCinematicDirector().GetUsePreferredMode(), preferredShotHashes);

				atArray<u32> RandomIndexs; 

				CreateRandomIndexArray(NumberOfPreferredShots, RandomIndexs);

				
				for(int i=0; i < RandomIndexs.GetCount(); i++)
				{
					bool shouldConsiderShot = true; 
					//iterate over the array and reject any cameras we tried to create but failed too this create phase	
					for(int k=0; k < m_ListOfCamerasAttemptedToCreate.GetCount(); k++)
					{
						if(m_ListOfCamerasAttemptedToCreate[k] == preferredShotHashes[RandomIndexs[i]])
						{
							shouldConsiderShot = false; 
						}
					}

					if(shouldConsiderShot)
					{
						for(int j=0; j < SelectableShots.GetCount(); j++)
						{
							if(m_Shots[SelectableShots[j]]->m_Metadata.m_Name.GetHash() == preferredShotHashes[RandomIndexs[i]])	
							{
								ShotIndex = SelectableShots[j]; 	

								if(ShotIndex != -1)
								{
									camInterface::GetCinematicDirector().SetShouldUsePreferredShot(true);
									return ShotIndex; 
								}
							}
						}
					}
				}
			}
			//have been through the list but failed to create a preferred shot reset this
			camInterface::GetCinematicDirector().SetShouldUsePreferredShot(false);  
		}

	}

	if(!canUseSameShotTwice)
	{
		//remove the last shot from the list
		if(m_ShotHistory.GetCount() > 0 && SelectableShots.GetCount() > 1)
		{
			for(int i=0; i < SelectableShots.GetCount(); i++)
			{
				if(m_Shots[SelectableShots[i]]->GetMetadata().m_Name.GetHash() == m_LastShotHash)	
				{
					SelectableShots.Delete(i); 
					break; 
				}
			}
		}
		//create a list based on shots we tried last time
		for(int i=0; i < SelectableShots.GetCount(); i++)
		{
			for(int j=0; j < m_ListOfCamerasAttemptedToCreate.GetCount(); j++)
			{
				if(SelectableShots[i] == m_ListOfCamerasAttemptedToCreate[j])
				{
					SelectableShots.Delete(i); 
					i--; 
					break; 
				}
			}
		}
	}
	else
	{
		//return the last historical shot
		if(m_ShotHistory.GetCount() > 0)
		{
			for(int i=0; i < SelectableShots.GetCount(); i++)
			{
				if(m_Shots[SelectableShots[i]]->GetMetadata().m_Name.GetHash()  == m_LastShotHash)	
				{
					ShotIndex = SelectableShots[i]; 
					return ShotIndex; 
				}
			}
		}
	}
	
	if(NumberOfAvailibleShots == 0)
	{
		return ShotIndex; 
	}
	else if(NumberOfAvailibleShots == 1)
	{
		ShotIndex = SelectableShots[0]; 	
	}
	if(NumberOfAvailibleShots > 1)
	{
		//Start with the highest priority shot
		u32 MaxPriority  = 0; 

		for(int i=0; i < SelectableShots.GetCount(); i++)
		{
			if(m_Shots[SelectableShots[i]]->GetPriority() >= MaxPriority )
			{
				MaxPriority = m_Shots[SelectableShots[i]]->GetPriority(); 
			}
		}

		//Keep only the same priority shots
		for(int i=0; i < SelectableShots.GetCount(); i++)
		{
			if(m_Shots[SelectableShots[i]]->GetPriority() != MaxPriority )
			{
				SelectableShots.Delete(i); 
			}
		}
		
		if(SelectableShots.GetCount() > 1)
		{
			
			float ShotProb = camInterface::GetCinematicDirector().GetRandomNumber().GetFloat(); 
			
			//compute the total probability to normalise each value later on
			float totalProbability = 0.0f;
			
			for(int i =0; i < SelectableShots.GetCount(); i++)
			{
				totalProbability += m_Shots[SelectableShots[i]]->GetProbability(); 
			}
			
			ShotProb*=totalProbability; 

			float previousProbabilty = 0.0f; 
			float currentProbabilty = 0.0f; 

			for ( int i = 0; i < SelectableShots.GetCount(); i++)
			{
				 currentProbabilty += (m_Shots[SelectableShots[i]]->GetProbability());

				if(ShotProb >= previousProbabilty && ShotProb <= currentProbabilty )
				{
					return SelectableShots[i]; 
				}
				
				previousProbabilty = currentProbabilty; 

			}
		}
		else if(SelectableShots.GetCount() == 1)
		{
			return SelectableShots[0]; 
		}
		else
		{
			return -1; 
		}
	}
	
	return ShotIndex; 
}

u32 camBaseCinematicContext::ComputeNumOfSelectableShots()
{
	u32 NumShots = 0; 
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		if(m_Shots[i]->IsValid())
		{
			NumShots++;  
		}
	}
	return NumShots; 
}

void camBaseCinematicContext::ComputeShotTargets()
{
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		if(m_Shots[i]->GetAttachEntity() == NULL)	
		{
			m_Shots[i]->SetAttachEntity(); 
		}

		if(m_Shots[i]->GetLookAtEntity() == NULL)	
		{
			m_Shots[i]->SetLookAtEntity(); 
		}
	}
}

void camBaseCinematicContext::ClearAllShots()
{
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		m_Shots[i]->ClearShot(); 
	}
}

void camBaseCinematicContext::ClearUnusedShots(s32 ActiveShot)
{
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		if(i != ActiveShot)
		{
			m_Shots[i]->ClearShot();
		}
	}
}
void camBaseCinematicContext::SetShotStartTimes(u32 Time)
{
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		m_Shots[i]->SetShotStartTime(Time); 
	}
}

bool camBaseCinematicContext::ComputeShouldChangeForHigherPriorityShot(CameraSelectorStatus &currentStatus)
{
	if(!camInterface::GetCinematicDirector().ShouldUsePreferedShot())
	{
		if(m_CurrentShotIndex > -1 && m_CurrentShotIndex < m_Shots.GetCount())
		{
			u32 time = fwTimer::GetTimeInMilliseconds();

			u32 timeSinceLastChange	= time - m_PreviousCameraChangeTime;

			atVector<u32> HigherPriorityShots; 
			
			bool HaveComputedShotTargets = false; 

			for(int i = 0; i < m_Shots.GetCount(); i++)
			{
				if(m_CurrentShotIndex > -1 && m_CurrentShotIndex < m_Shots.GetCount())
				{
					if(m_Shots[i]->GetPriority() > m_Shots[m_CurrentShotIndex]->GetPriority())	
					{
						if(!HaveComputedShotTargets)
						{
							ComputeShotTargets();
							HaveComputedShotTargets = true; 
						}

						if(m_Shots[i]->IsValid() && m_Shots[i]->CanCreate())
						{
							HigherPriorityShots.PushAndGrow(i); 
						}
					}
				}
			}
			
			//Get a list of higher priority shots
			for(int i=0; i < HigherPriorityShots.GetCount(); i++)
			{
				if(!HaveComputedShotTargets)
				{
					ComputeShotTargets();
					HaveComputedShotTargets = true; 
				}
				
				if(m_Shots[HigherPriorityShots[i]])
				{
					u32 minDuration = (u32)(m_Shots[m_CurrentShotIndex]->m_Metadata.m_ShotDurationLimits.x * m_Shots[HigherPriorityShots[i]]->GetMinShotScalar()); 

					if(timeSinceLastChange >= minDuration)
					{
						s32 shotIndex = SelectShot(false); 

						if(TryAndCreateANewCamera(shotIndex))
						{
							currentStatus = CSS_DONT_CHANGE_CAMERA; 
							return true;
						}
					}
				}
			}
		
		}
	}
	return false; 
}

void camBaseCinematicContext::PreUpdateShots()
{
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		bool IsCurrentShot = i == m_CurrentShotIndex; 
		m_Shots[i]->PreUpdate(IsCurrentShot);  
	}
}

bool camBaseCinematicContext::UpdateShotSelector()
{
	//this context might support the game camera as fall back if its running don't run the selector just check this is complete
	if(m_IsRunningGameCameraAsFallBack)
	{
		const u32 time = fwTimer::GetTimeInMilliseconds();

		const bool isSpectating	= (NetworkInterface::IsGameInProgress() && NetworkInterface::IsInSpectatorMode());
		if(isSpectating)
		{
			const camControlHelper* gameplayControlHelper	= NULL;
			const camBaseCamera* gameplayCamera				= camInterface::GetGameplayDirector().GetRenderedCamera();
			if(gameplayCamera)
			{
				if(gameplayCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
				{
					gameplayControlHelper = static_cast<const camThirdPersonCamera*>(gameplayCamera)->GetControlHelper();
				}
				else if(gameplayCamera->GetIsClassId(camAimCamera::GetStaticClassId()))
				{
					gameplayControlHelper = static_cast<const camAimCamera*>(gameplayCamera)->GetControlHelper();
				}
			}

			if(gameplayControlHelper && gameplayControlHelper->IsLookAroundInputPresent())
			{
				//Hold on gameplay if there has been user input recently, so that the user can look-around without interruption.
				m_PreviousCameraChangeTime	= time;
				return true;
			}
		}

		u32 timeSinceLastChange					= time - m_PreviousCameraChangeTime;
		const u32 gameCameraFallBackDuration	= isSpectating ? g_GameCameraFallBackDurationForSpectating : m_Metadata.m_GameCameraFallBackDuration;
		if(timeSinceLastChange < gameCameraFallBackDuration)
		{
			return true;
		}
	}
	
	s32 shotIndex = -1;

	bool hasFoundAGoodCamera = false; 
	
	CameraSelectorStatus currentStatus = GetChangeShotStatus(); 
	
	if(currentStatus != CSS_DONT_CHANGE_CAMERA)
	{
		camInterface::GetCinematicDirector().SetShouldUsePreferredShot(false);
	}
	//Investigate moving this to later so we update on the first frame a new shot is created
	PreUpdateShots(); 

	if(ComputeShouldChangeForHigherPriorityShot(currentStatus))
	{
		return true; 
	}
	
	//update the current shot 
	if(currentStatus != CSS_MUST_CHANGE_CAMERA && GetCurrentShot())
	{
		//Investigate moving this to later so we update on the first frame a new shot is created
		if(GetCurrentShot()->IsValid())
		{
			GetCurrentShot()->Update(); 
		}
	}

	if(currentStatus != CSS_DONT_CHANGE_CAMERA)
	{
		if(currentStatus == CSS_MUST_CHANGE_CAMERA && GetCurrentShot())
		{
			GetCurrentShot()->ClearShot();

			if(currentStatus == CSS_MUST_CHANGE_CAMERA && m_Shots[m_CurrentShotIndex]->m_Metadata.m_ShouldTerminateAtEndOfDuration && m_Shots.GetCount() == 1)
			{
				return false; 
			}
		}	

		ComputeShotTargets(); 
		
		m_NumSelectableShots = ComputeNumOfSelectableShots(); 
		
		bool bShouldTrySameCameraAgain = false; 
		bool HaveTriedSameShotTiwce = false; 
		bool shouldTryAllShots = true; 
		u32 LastShotHash = 0; 

		if(!m_ShotHistory.IsEmpty())
		{
			LastShotHash = m_ShotHistory.Top(); 
		}

		//check that last history shot is not in out shot selection
		if(m_ShotHistory.GetCount() > 0)
		{
			for(int i=0; i < m_Shots.GetCount(); i++)
			{
				if(m_Shots[i]->GetNameHash() == LastShotHash)
				{
					shouldTryAllShots = false; 
				}
			}
		}

		while (currentStatus != CSS_DONT_CHANGE_CAMERA)
		{
			if(!shouldTryAllShots)
			{
				if(m_NumSelectableShots > 1)
				{
					if((u32)m_ListOfCamerasAttemptedToCreate.GetCount() >= (m_NumSelectableShots-1))
					{
						bShouldTrySameCameraAgain = true; 
					}
				}
				else
				{
					if((u32)m_ListOfCamerasAttemptedToCreate.GetCount() == m_NumSelectableShots)
					{
						bShouldTrySameCameraAgain = true; 
					}
				}
			}

			if(bShouldTrySameCameraAgain)
			{
				if(currentStatus != CSS_MUST_CHANGE_CAMERA && m_CurrentShotIndex != -1 && (m_Shots[m_CurrentShotIndex]->IsValid() && m_Shots[m_CurrentShotIndex]->GetCamera()))
				{
					currentStatus = CSS_DONT_CHANGE_CAMERA;
					m_ListOfCamerasAttemptedToCreate.Reset(); 
					return true;
				}
				else
				{
					if(!HaveTriedSameShotTiwce)	
					{
						//change to just attempt the previous shot
						shotIndex = SelectShot(true);
						HaveTriedSameShotTiwce = true; 
					}
				}
			}
			else
			{
				shotIndex = SelectShot(false); 
			}
			
			if(!m_Metadata.m_IsValidToHaveNoCameras)
			{
				cameraAssertf(shotIndex != -1, "cannot find a valid shot to select"); 
			}


			if(shotIndex != -1)
			{
				hasFoundAGoodCamera = TryAndCreateANewCamera(shotIndex); 
				if(hasFoundAGoodCamera)
				{
					currentStatus = CSS_DONT_CHANGE_CAMERA; 
					break; 
				}
				else
				{	
					if(shotIndex > -1)
					{
						m_Shots[shotIndex]->ClearShot(); 	
						m_ListOfCamerasAttemptedToCreate.PushAndGrow(shotIndex); 
						//a preferred shot was selected but this failed to produce a valid camera
						if(camInterface::GetCinematicDirector().ShouldUsePreferedShot())
						{
							camInterface::GetCinematicDirector().SetShouldUsePreferredShot(false);
						}
					}

				}
			}

			if(!hasFoundAGoodCamera)
			{
				if(HaveTriedSameShotTiwce || (shouldTryAllShots && (u32)m_ListOfCamerasAttemptedToCreate.GetCount() == m_NumSelectableShots))
				{
					currentStatus = CSS_DONT_CHANGE_CAMERA;
					m_ListOfCamerasAttemptedToCreate.Reset(); 
					ClearAllShots(); 
					return false;
				}
			}
		}
	}
	else
	{
		hasFoundAGoodCamera = true; 
	}

	if(hasFoundAGoodCamera)
	{
		m_IsRunningGameCameraAsFallBack = false;
	}
	return hasFoundAGoodCamera; 
	
}

bool camBaseCinematicContext::TryAndCreateANewCamera(s32 shotIndex)
{
	bool hasFoundAGoodCamera = false; 

	if(shotIndex >= 0 && shotIndex < m_Shots.GetCount() )
	{
		//try and create a camera
		const u32 cameraHash = m_Shots[shotIndex]->GetCameraRef();
		if(cameraHash != 0)
		{
			hasFoundAGoodCamera = m_Shots[shotIndex]->CreateAndTestCamera(cameraHash);
		}

		if(hasFoundAGoodCamera)
		{	
			//need to delete the previous camera
			if(m_CurrentShotIndex != shotIndex)
			{	
				if(m_CurrentShotIndex >= 0 && m_CurrentShotIndex < m_Shots.GetCount())
				{
					m_Shots[m_CurrentShotIndex]->ClearShot(); 	
				}
			}

			//m_ShotHistory.PushAndGrow(m_Shots[shotIndex]->m_Metadata.m_Name.GetHash());
			if(m_ShotHistory.IsFull())
			{
				m_ShotHistory.PopEnd(); 
			}

			m_ShotHistory.PushTop(m_Shots[shotIndex]->m_Metadata.m_Name.GetHash()); 

			m_CurrentShotIndex = shotIndex; 
			m_ListOfCamerasAttemptedToCreate.Reset(); 
			ClearUnusedShots(m_CurrentShotIndex); 
			//SetShotStartTimes(m_PreviousCameraChangeTime); 
		
			m_PreviousCameraChangeTime = fwTimer::GetTimeInMilliseconds();
		}
	}

	return hasFoundAGoodCamera; 
}

CameraSelectorStatus camBaseCinematicContext::GetChangeShotStatus() const
{
	if((m_CurrentShotIndex == -1) || (m_Metadata.m_CanSelectShotFromControlInput && camInterface::GetCinematicDirector().ShouldChangeCinematicShot()))
	{
		return CSS_MUST_CHANGE_CAMERA; 
	}
	
	//Do not respect the duration if in spectator mode, applies to all spectator cinematic modes in and out of vehicle. 
	//Have to apply a generic fix as we don't have a separate "in vehicle" context when spectating. 
	if(NetworkInterface::IsInSpectatorMode() || !m_Metadata.m_ShouldRespectShotDurations)
	{
		return CSS_DONT_CHANGE_CAMERA; 
	}

	if(m_Shots[m_CurrentShotIndex]->m_Metadata.m_ShouldUseDuration)
	{
		u32 time = fwTimer::GetTimeInMilliseconds();

		u32 timeSinceLastChange	= time - m_PreviousCameraChangeTime;
		
		if(m_Shots.GetCount() > 0 && m_CurrentShotIndex >= 0 && m_CurrentShotIndex < m_Shots.GetCount() )
		{
			u32 minDuration = (u32)m_Shots[m_CurrentShotIndex]->m_Metadata.m_ShotDurationLimits.y; 
			
			if(timeSinceLastChange <= minDuration )
			{
				return CSS_DONT_CHANGE_CAMERA; 
			}
		}
		
		if(m_Shots[m_CurrentShotIndex]->m_Metadata.m_ShouldTerminateAtEndOfDuration)
		{
			cameraDebugf3("Times up Must Change %d", timeSinceLastChange); 
			return CSS_MUST_CHANGE_CAMERA; 
		}
		else
		{
			cameraDebugf3("Times up Can Change %d", timeSinceLastChange); 
			return CSS_CAN_CHANGE_CAMERA;
		}
	}
	else
	{
		return CSS_DONT_CHANGE_CAMERA; 
	}
}

void camBaseCinematicContext::ClearContext()
{
	if(m_CurrentShotIndex>= 0 && m_CurrentShotIndex < m_Shots.GetCount())
	{
		m_Shots[m_CurrentShotIndex]->ClearShot(true);
	}

	m_NumSelectableShots = 0; 
	m_CurrentShotIndex = -1; 
	m_IsRunningGameCameraAsFallBack = false; 
}

bool camBaseCinematicContext::Update()
{
	if (IsValid())
	{
		if(UpdateContext())
		{
			return true; 
		}
	}
	return false; 
}

bool camBaseCinematicContext::UpdateContext()
{
	bool hasSucceeded = false;

	hasSucceeded = UpdateShotSelector();
	
	//failed to create a camera so. If this contexts supports the game camera as a fall back trigger it.
	if(!hasSucceeded && m_Metadata.m_CanUseGameCameraAsFallBack && !m_IsRunningGameCameraAsFallBack)
	{
		m_IsRunningGameCameraAsFallBack = true;
		m_PreviousCameraChangeTime = fwTimer::GetTimeInMilliseconds(); 

		hasSucceeded = true; 

	}

	return hasSucceeded;
}


const camBaseCinematicShot* camBaseCinematicContext::GetCurrentShot() const
{
	if(m_CurrentShotIndex>= 0 && m_CurrentShotIndex < m_Shots.GetCount())
	{
		const camBaseCinematicShot* pShot = m_Shots[m_CurrentShotIndex]; 
		return pShot;
	}

	return NULL; 
}

camBaseCinematicShot* camBaseCinematicContext::GetCurrentShot()
{
	if(m_CurrentShotIndex>= 0 && m_CurrentShotIndex < m_Shots.GetCount())
	{
		camBaseCinematicShot* pShot = m_Shots[m_CurrentShotIndex]; 
		return pShot;
	}

	return NULL; 
}



s32 camBaseCinematicContext::GetShotIndex(const u32 hash) const
{
	for(int i=0; i < m_Shots.GetCount(); i++)
	{
		if(m_Shots[i]->GetIsClassId(hash))
		{
			return i; 
		}
	}
	return -1; 
}

float camBaseCinematicContext::GetShotInitalSlowValue() const
{
	if(m_CurrentShotIndex != -1)
	{
		float initialSlowMo = -1.0f; 

		initialSlowMo = m_Shots[m_CurrentShotIndex]->m_Metadata.m_InitialSlowMoSpeed; 

		return initialSlowMo; 

	}
	return -1.0f;
}

u32 camBaseCinematicContext::GetShotSlowMotionBlendOutDuration() const
{
	if(m_CurrentShotIndex != -1)
	{
		u32 slowMotionBlendOutDuration = 0; 

		slowMotionBlendOutDuration = m_Shots[m_CurrentShotIndex]->m_Metadata.m_SlowMotionBlendOutDuration; 

		return slowMotionBlendOutDuration; 

	}
	return 0;
}

bool camBaseCinematicContext::CanBlendOutSlowMotionOnShotTermination() const
{
	if(m_CurrentShotIndex != -1)
	{
		bool canBlendOutSlowMotion = false; 

		canBlendOutSlowMotion = m_Shots[m_CurrentShotIndex]->m_Metadata.m_CanBlendOutSlowMotionOnShotTermination; 

		return canBlendOutSlowMotion; 

	}
	return false;
}
bool camBaseCinematicContext::IsInitialSlowMoValueValid(float slowMo)const
{
	if(m_CurrentShotIndex > -1)
	{
		if(slowMo < 0.0f)
		{
			return false; 
		}
		else
		{
			if(InRange(slowMo, 0.0f + SMALL_FLOAT, 1.0f))
			{
				return true; 
			}
		}
	}
	return false; 
}


//void camBaseCinematicContext::InitActiveCamera()
//{
	//if(m_UpdatedCamera->GetIsClassId(camCinematicHeliChaseCamera::GetStaticClassId()))
	//{
	//	m_UpdatedCamera->LookAt(m_TargetEntity);
	//	camCinematicHeliChaseCamera* pCam = static_cast<camCinematicHeliChaseCamera*>(m_UpdatedCamera.Get()); 
	//	pCam->SetPathHeading(m_CinematicHeliDesiredPathHeading); 
	//}
	//else if(m_UpdatedCamera->GetIsClassId(camCinematicPedCloseUpCamera::GetStaticClassId()))
	//{
	//	m_UpdatedCamera->LookAt(m_TargetEntity);
	//}
	//else if(m_UpdatedCamera->GetIsClassId(camCinematicVehicleOrbitCamera::GetStaticClassId()))
	//{
	//	camCinematicVehicleOrbitCamera* pCam = static_cast<camCinematicVehicleOrbitCamera*>(m_UpdatedCamera.Get()); 
	//	m_UpdatedCamera->AttachTo(m_TargetEntity);
	//	pCam->Init(); 
	//	pCam->SetShotId(m_CurrentCinematicCamera.CameraMode); 
	//}
	//else if(m_UpdatedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	//{
	//	if(m_TargetEntity->GetIsTypeVehicle())
	//	{
	//		CVehicle* targetVehicle	= (CVehicle*)m_TargetEntity.Get(); 
	//		if(targetVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
	//		{
	//			if(m_CurrentCinematicCamera.CameraRef == m_Metadata.m_TrainRoofMountedCameraRef || m_CurrentCinematicCamera.CameraRef  == m_Metadata.m_TrainPassengerCameraRef)
	//			{				
	//				CTrain* engine	= CTrain::FindEngine(static_cast<CTrain*>(targetVehicle));
	//				{
	//					m_UpdatedCamera->AttachTo(engine);
	//					return;
	//				}
	//			}
	//			else
	//			{
	//				if(m_CurrentCinematicCamera.CameraRef == m_Metadata.m_TrainStationMountedCameraRef)
	//				{
	//					CTrain* caboose	= CTrain::FindCaboose(static_cast<CTrain*>(targetVehicle));
	//					if(caboose)
	//					{
	//						m_UpdatedCamera->AttachTo(caboose);
	//						return;
	//					}
	//				}
	//			}
	//		}
	//		
	//		if(m_CurrentCinematicCamera.CameraRef == m_Metadata.m_PoliceCarMountedCameraRef)
	//		{
	//			camCinematicMountedCamera* pCam = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get()); 
	//			pCam->ShouldLimitRelativePitchAndHeading(true, true); 
	//			pCam->ShouldDisableControlHelper(true); 
	//			pCam->ShouldTerminateForOcclusion(true); 
	//			pCam->SetLookAtBehaviour(LOOK_AT_FOLLOW_PED_RELATIVE_POSITION);
	//			pCam->ShouldTerminateForDistance(true);
	//		}

	//		if(m_CurrentCinematicCamera.CameraRef == m_Metadata.m_PoliceHeliMountedCameraRef)
	//		{
	//			camCinematicMountedCamera* pCam = static_cast<camCinematicMountedCamera*>(m_UpdatedCamera.Get()); 
	//			pCam->ShouldTerminateForOcclusion(true);	
	//			pCam->ShouldTerminateForDistance(true);
	//			pCam->ShouldUseXYDistance(true);
	//		}
	//	}
	//	m_UpdatedCamera->AttachTo(m_TargetEntity);
	//}
	//else if(m_UpdatedCamera->GetIsClassId(camCinematicTrainTrackCamera::GetStaticClassId()))
	//{
	//	m_UpdatedCamera->LookAt(m_TargetEntity);
	//	if(m_TargetEntity)
	//	{	
	//		((camCinematicTrainTrackCamera*)m_UpdatedCamera.Get())->SetMode(m_CurrentCinematicCamera.CameraMode);
	//	}
	//	
	//}
	//else if(m_UpdatedCamera->GetIsClassId(camCinematicCamManCamera::GetStaticClassId()))
	//{
	//	m_UpdatedCamera->LookAt(m_TargetEntity);
	//	((camCinematicCamManCamera*)m_UpdatedCamera.Get())->SetMode(m_CurrentCinematicCamera.CameraMode);
	//}
	//else if(m_UpdatedCamera->GetIsClassId(camCinematicPositionCamera::GetStaticClassId()))
	//{
	//		m_UpdatedCamera->LookAt(camInterface::FindFollowPed());
	//		m_UpdatedCamera->AttachTo(m_TargetEntity);
	//		
	//		if(m_CurrentCinematicCamera.CameraRef == m_Metadata.m_PoliceInCoverCameraRef)
	//		{	
	//			if(m_TargetEntity->GetIsTypePed())
	//			{
	//				Vector3 position; 
	//				float heading; 

	//				const CPed* ped = static_cast<const CPed*>(m_TargetEntity.Get());
	//				/*if(GetHeadingAndPositionOfCover(ped, position, heading))
	//				{
	//					((camCinematicPositionCamera*)m_UpdatedCamera.Get())->OverridePositionAndHeading(position, heading);
	//				}*/
	//			}
	//		}

	//		((camCinematicPositionCamera*)m_UpdatedCamera.Get())->Init();

	//}
	//else if(m_UpdatedCamera->GetIsClassId(camCinematicStuntCamera::GetStaticClassId()))
	//{
	//	m_UpdatedCamera->LookAt(m_TargetEntity); 
	//	Vector3 camPos(VEC3_ZERO); 
	//	CStuntJumpManager::GetCameraPositionForStuntJump(camPos); 
	//	m_UpdatedCamera->GetFrameNonConst().SetPosition(camPos); 
	//	
	//}
	//else if (m_UpdatedCamera->GetIsClassId(camCinematicAnimatedCamera::GetStaticClassId()))
	//{
	//	const CPed* followPed = camInterface::FindFollowPed();
	//	if(followPed)
	//	{
	//		//get the action result and the melee task
	//		const CTaskMeleeActionResult*	pMeleeActionResult = followPed->GetPedIntelligence()->GetTaskMeleeActionResult(); 

	//		const CTaskMelee* pMelee = followPed->GetPedIntelligence()->GetTaskMelee(); 

	//		if(pMelee && pMeleeActionResult && pMeleeActionResult->GetState() == CTaskMeleeActionResult::State_PlayClip)
	//		{
	//			camCinematicAnimatedCamera* pAnimCinematic = static_cast<camCinematicAnimatedCamera*>(m_UpdatedCamera.Get()); 

	//			pAnimCinematic->Init();

	//			//get the clip set from the action result task
	//			const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pMeleeActionResult->GetActionResult()->GetClipSet());

	//			//get the clip id from the task for the correct camera anim Id
	//			fwMvClipId ClipId(pMelee->GetTakeDownCameraAnimId()); 

	//			const crClip* pClip = pClipSet->GetClip(ClipId); 

	//			//set the start time to be the current time of the anim
	//			float StartTime = pMeleeActionResult->GetCurrentClipTime(); 

	//			Vector3 vPedPos = VEC3V_TO_VECTOR3(followPed->GetTransform().GetPosition());

	//			if(pAnimCinematic->SetCameraClip(vPedPos, pClipSet->GetClipDictionaryName(), pClip, StartTime))
	//			{
	//				//calculate where the ped will be at the start of the animation
	//				Vector3 vTargetPos = pMelee->GetStealthKillWorldOffset();
	//				float fHeading = pMelee->GetStealthKillHeading();
	//				Matrix34 targetMat(M34_IDENTITY);

	//				targetMat.d = vTargetPos; 
	//				targetMat.MakeRotateZ(fHeading); 

	//				targetMat.d.z -= 1.0f; 

	//				pAnimCinematic->SetSceneMatrix(targetMat); 
	//			}
	//		}
	//	}
	//}

//}


