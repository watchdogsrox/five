

#include "spatialdata/aabb.h"

#include "camera/CamInterface.h"
#include "scene/world/GameWorld.h"
#include "scene/LoadScene.h"
#include "scene/FocusEntity.h"
#include "Peds/pedplacement.h"
#include "peds/pedpopulation.h"
#include "vehicles/vehiclepopulation.h"
#include "fwscene/stores/staticboundsstore.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "debug/TextureViewer/TextureViewerPrivate.h" // need this for DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO, although it would be cleaner to provide an accessor ..

#include "vehicles/Automobile.h"
#include "vehicles/cargen.h"
#include "vehicles/VehicleFactory.h"
#include "script/script.h"
#include "system/stack.h"

//////////////////////////////////////////////////////////////////////////
// Warp Manager
//////////////////////////////////////////////////////////////////////////

PARAM(noWarpTimeOfDaySet, "Disable setting Time Of Day when using debug warp functionality");
PARAM(noWarpCameraPosSet, "Disable setting the Camera position when using debug warp functionality");

#include "WarpManager.h"

#define __INCLUDE_DEBUG_SPEW	(1)

RegdPhy					CWarpManager::m_pPhysical;
CWarpManager::eState	CWarpManager::m_eState;
Vector3					CWarpManager::m_Pos;
Vector3					CWarpManager::m_Vel;
float					CWarpManager::m_fHeading;
float					CWarpManager::m_fRadius;
bool					CWarpManager::m_bSnapToGround = false;
u32						CWarpManager::m_StartTime;
bool					CWarpManager::m_StopWarp = false;
bool					CWarpManager::m_bFadeOutPlayer = false;
bool					CWarpManager::m_bFadeOutAtStart = true;
bool					CWarpManager::m_bFadeInWhenComplete = true;
bool					CWarpManager::m_bInstantFillPopulation = true;
bool					CWarpManager::m_bOwnedByScript = false;
bool					CWarpManager::m_bFromMap = false;
scrThreadId				CWarpManager::m_scriptThreadId = THREAD_INVALID;
u32						CWarpManager::m_Flags;

BANK_ONLY(Vector3		CWarpManager::m_CamPos);
BANK_ONLY(Vector3		CWarpManager::m_CamFront);
BANK_ONLY(bool			CWarpManager::m_SetCamPos = false);
BANK_ONLY(s32			CWarpManager::m_ClockHours);
BANK_ONLY(s32			CWarpManager::m_ClockMinutes);
BANK_ONLY(bool			CWarpManager::m_SetTimeOfDay = false);
BANK_ONLY(bool			CWarpManager::m_WasDebugStart = false);

BANK_ONLY(bool			CWarpManager::sm_WaitUntilSceneLoaded = false; )
BANK_ONLY(bool			CWarpManager::sm_FadeCamera = true; )

void	CWarpManager::Update()
{
	if(!IsActive())
		return;

	// Check we're not currently doing a player switch.
	Assertf( g_PlayerSwitch.IsActive() == false, "Trying to Warp while a player switch is active, this is not advised or supported.");

	switch(m_eState)
	{
	case	STATE_FADE_OUT:
		{
			if(m_StopWarp)
			{
				// We haven't started yet and are still faded in, just end
				m_eState = STATE_INACTIVE;
			}
			else
			{
				if(m_bFadeOutAtStart)
				{
					m_bFadeOutPlayer = false;
					BANK_ONLY(if(sm_FadeCamera))
					{
#if __INCLUDE_DEBUG_SPEW
						Displayf("CWarpManager: Starting Fade Down");
#endif	//__INCLUDE_DEBUG_SPEW

						camInterface::FadeOut(600, true);
						if(camInterface::IsFadedOut())
							m_eState = STATE_START_WARP;
					}
					BANK_ONLY(else {m_eState = STATE_START_WARP;})
#if __INCLUDE_DEBUG_SPEW
						Displayf("CWarpManager: Waiting To Start (fade out camera)");
#endif
				}
				else if (m_bFadeOutPlayer && m_pPhysical.Get() != NULL)
				{
					m_pPhysical->GetLodData().SetFadeToZero(true);
					m_eState = STATE_START_WARP;		
				}
				else
				{
					m_eState = STATE_START_WARP;		
				}
			}
		}
		break;

	case	STATE_START_WARP:
		{
			bool	readyToStartWarp = true;/*(!m_bFadeOutAtStart && !m_bFadeOutPlayer) 
										|| (m_bFadeOutPlayer && m_pPhysical.Get() != NULL && 0 == m_pPhysical->GetAlpha())
										|| (m_bFadeOutAtStart && camInterface::IsFadedOut()) 
										BANK_ONLY(|| !sm_FadeCamera);*/
#if __INCLUDE_DEBUG_SPEW
			if(!readyToStartWarp)
			{
				Displayf("CWarpManager: Waiting To Start (fade out or pre warp delays)");
			}
#endif //__INCLUDE_DEBUG_SPEW

			if(readyToStartWarp)
			{

#if __INCLUDE_DEBUG_SPEW
				Displayf("CWarpManager: Starting Warp");
#endif //__INCLUDE_DEBUG_SPEW

				if( m_StopWarp )
				{

#if __INCLUDE_DEBUG_SPEW
					Displayf("CWarpManager: Warp Cancelled - By User");
#endif //__INCLUDE_DEBUG_SPEW

					// We're faded out, but haven't started anything yet
					m_eState = STATE_FADE_IN;
				}
				else if(m_pPhysical.Get() != NULL)
				{
					if(m_pPhysical->GetIsTypePed())
					{
						// Add the root offset and assume that the coords are decent.
						// This should help alleviate MP warp issues where the screen can't be faded out. (B*881685)
						m_Pos.z += static_cast<CPed*>(m_pPhysical.Get())->GetCapsuleInfo()->GetGroundToRootOffset();
					}
					else if(m_pPhysical->GetIsTypeVehicle())
					{
						spdAABB vBB;

						CBaseModelInfo *pModelInfo = m_pPhysical->GetBaseModelInfo();
						if(pModelInfo)
						{
							spdAABB box = pModelInfo->GetBoundingBox();
							const Vector3 boxMin = box.GetMinVector3();
							m_Pos.z -= boxMin.z;
						}
					}

#if __BANK
					DoPreWarpDebugStuff();
#endif	//__BANK

#if __INCLUDE_DEBUG_SPEW
					Displayf("CWarpManager: Teleporting Player to (%f,%f,%f)", m_Pos.x, m_Pos.y, m_Pos.z);
#endif //__INCLUDE_DEBUG_SPEW

					m_pPhysical->Teleport(m_Pos, m_fHeading, false, true, false, true, false, true);
					m_pPhysical->SetHeading(m_fHeading);
					m_Vel.RotateZ(m_fHeading);
					m_pPhysical->SetVelocity(m_Vel);
					Vec3V lookAt = m_pPhysical->GetTransform().GetForward();

#if __INCLUDE_DEBUG_SPEW
					Displayf("CWarpManager: Warping, calling LoadScene.Start()");
#endif //__INCLUDE_DEBUG_SPEW

					if(m_bInstantFillPopulation)
					{
						CPedPopulation::ForcePopulationInit();
						CVehiclePopulation::InstantFillPopulation();
						CTheCarGenerators::InstantFill();
					}

					if(!(m_Flags & FLAG_NO_LOADSCENE))
					{
						s32	flags = CLoadScene::FLAG_INCLUDE_COLLISION;
						// Did we want to only request HD Assets?
						if( m_Flags & FLAG_HD_ASSETS )
						{
							flags |= CLoadScene::FLAG_LONGSWITCH_CUTSCENE;	// To request HD Assets Only
						}

						g_LoadScene.Start( VECTOR3_TO_VEC3V(m_Pos), lookAt, m_fRadius, false, flags, CLoadScene::LOADSCENE_OWNER_WARPMGR );

						// If a script ID was set after SetWarp, pass to Loadscene
						if(m_bOwnedByScript)
						{
							g_LoadScene.SetScriptThreadId(m_scriptThreadId);
						}

						m_StartTime = fwTimer::GetTimeInMilliseconds();
						m_pPhysical->DisableCollision(NULL, true);
						m_pPhysical->SetFixedPhysics(true);
						m_eState = STATE_DO_WARP;
					}
					else
					{
						m_eState = STATE_END_WARP;
					}
				}
				else
				{
					// The player has vanished, just fade back in
					m_eState = STATE_FADE_IN;
				}
			}
			else if(m_StopWarp)
			{

#if __INCLUDE_DEBUG_SPEW
				Displayf("CWarpManager: Warp Cancelled");
#endif //__INCLUDE_DEBUG_SPEW

				Assertf(0, "Failed to start teleport");
				// We're faded out, but haven't started anything yet
				m_eState = STATE_FADE_IN;
			}
		}
		break;

	case	STATE_DO_WARP:
		{
			bool bTerminate = false;

			bool bLoadingComplete = g_LoadScene.IsLoaded();

			bLoadingComplete &= HaveVehicleModsLoaded();

#if __INCLUDE_DEBUG_SPEW && !__NO_OUTPUT
			if(!bLoadingComplete)
			{
#if __BANK && DEBUG_TEXTURE_VIEWER_STREAMINGITERATOR_USAGEMAPS_AUTO
				XPARAM(si_auto); // don't complain if we're running streaming iterators from a commandline during texture analysis
				if (!PARAM_si_auto.Get())
#endif
				{
					Vector3 vFocusPos = CFocusEntityMgr::GetMgr().GetPos();
					Vector3 vPedPos = CGameWorld::FindFollowPlayerCoors();
					Displayf("CWarpManager: Awaiting LoadScene.IsLoaded() - Focus pos (%.2f, %.2f, %.2f), Player pos (%.2f, %.2f, %.2f)", vFocusPos.x, vFocusPos.y, vFocusPos.z, vPedPos.x, vPedPos.y, vPedPos.z);
				}
			}
#endif //__INCLUDE_DEBUG_SPEW


			// See if the load has completed
			if( bLoadingComplete )
			{
				bool canStop = false;

				if(g_LoadScene.IsSafeToQueryInteriorProxy())
				{
					// Check if the interior instance and collision is loaded here (if needs be)
					CInteriorProxy *pInteriorProxy = g_LoadScene.GetInteriorProxy();
					if( pInteriorProxy )
					{
						// Interior Location
						if(pInteriorProxy->GetCurrentState() == CInteriorProxy::PS_FULL_WITH_COLLISIONS)
						{
							canStop = true;
						}
					}
					else
					{
						// Exterior Location
						if( g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(VECTOR3_TO_VEC3V(m_Pos), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER) )
						{
							canStop = true;
						}
					}
				}

				if( canStop )
				{
					bTerminate = true;
				}
			}
			else if( m_StopWarp )
			{
				// We're faded out and are doing the warp
				bTerminate = true;
			}
			else BANK_ONLY(if(!sm_WaitUntilSceneLoaded))
			{
				// Check for timeout
				u32 CurrentTime = fwTimer::GetTimeInMilliseconds();
				if( (CurrentTime - m_StartTime) > MAX_WAIT_TIME_MS )	// 15 seconds
				{
					bTerminate = true;
					Displayf("CWarpManager: Warp has taken too long, timeout!");
				}
			}


			if (bTerminate)
			{

#if __INCLUDE_DEBUG_SPEW
				Displayf("CWarpManager: Warp Complete, calling LoadScene.Stop()");
#endif //__INCLUDE_DEBUG_SPEW

			
				g_LoadScene.Stop();
				if(m_pPhysical.Get() != NULL)
				{
					m_pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
					m_pPhysical->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().GetPosition()));
				}
				m_eState = STATE_END_WARP;
			}
		}
		break;
	case	STATE_END_WARP:
		{

#if __INCLUDE_DEBUG_SPEW
			Displayf("CWarpManager: Ending Warp");
#endif //__INCLUDE_DEBUG_SPEW


			if(m_pPhysical.Get() != NULL)
			{
				// Snap to ground
				if( m_bSnapToGround )
				{
					if( m_pPhysical->GetIsTypePed()  )
					{
						// Place the ped on the ground
						u32 flags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | 
									ArchetypeFlags::GTA_VEHICLE_TYPE | 
									ArchetypeFlags::GTA_OBJECT_TYPE | 
									ArchetypeFlags::GTA_PICKUP_TYPE | 
									ArchetypeFlags::GTA_GLASS_TYPE | 
									ArchetypeFlags::GTA_RIVER_TYPE;

						if( CPedPlacement::FindZCoorForPed(TOP_SURFACE, &m_Pos, NULL, NULL, NULL, 0.5f, 99.5f, flags) )
						{
							m_pPhysical->SetPosition(m_Pos, true, true);
							m_pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
							m_pPhysical->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().GetPosition()));
						}
						// warps from the frontend map rely on height map, which inaccurate near tall buildings...
						else if( m_bFromMap && CPedPlacement::FindZCoorForPed(TOP_SURFACE, &m_Pos, NULL, NULL, NULL, 99.5f, 199.5f, flags) )
						{
							m_pPhysical->SetPosition(m_Pos, true, true);
							m_pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
							m_pPhysical->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().GetPosition()));
						}
#if __BANK				// DEBUG
						else
						{
							Warningf("WarpManager!! CPedPlacement::FindZCoorForPed has failed");
							Displayf("WarpManager: Destination = (%.2f,%.2f,%.2f)", m_Pos.x, m_Pos.y, m_Pos.z );
							if(m_bOwnedByScript)
							{
								scrThread *pThread = scrThread::GetThread(m_scriptThreadId);
								if(pThread)
								{
									Displayf("WarpManager: Called from Script:- %s", pThread->GetScriptName() );
								}
							}
							else
							{
								Displayf("WarpManager: Not Owned By Script");
							}
						}
#endif	//__BANK		// DEBUG

						CPed* pPed = static_cast<CPed*>(m_pPhysical.Get());
						if(pPed && pPed->GetPedAudioEntity())
						{
							pPed->GetPedAudioEntity()->GetFootStepAudio().ResetWetFeet();
						}
					}
					else if(m_pPhysical->GetIsTypeVehicle())
					{
						// Place the vehicle on the ground
						CVehicle *pVehicle = static_cast<CVehicle*>(m_pPhysical.Get());
						bool ret = pVehicle->PlaceOnRoadAdjust();
						if (!ret)
						{
							pVehicle->SetIsFixedWaitingForCollision(true);
						}
					}
				}

				m_pPhysical->EnableCollision();
				m_pPhysical->SetFixedPhysics(false);
			}

#if __BANK
			DoPostWarpDebugStuff();
#endif	//__BANK

			m_eState = STATE_FADE_IN;
		}
		break;

	case	STATE_FADE_IN:
		{
			if (m_bFadeOutPlayer && m_pPhysical.Get() != NULL)
			{
				m_pPhysical->GetLodData().SetFadeToZero(false);
			}

			if (m_bFadeInWhenComplete)
			{
				BANK_ONLY(if(sm_FadeCamera))
				{
#if __INCLUDE_DEBUG_SPEW
					Displayf("CWarpManager: Starting Fade Up");
#endif	//__INCLUDE_DEBUG_SPEW

					camInterface::FadeIn(600);
				}
			}

#if __INCLUDE_DEBUG_SPEW
			Displayf("CWarpManager: Warp Complete");
#endif	//__INCLUDE_DEBUG_SPEW

			m_eState = STATE_INACTIVE;		// Complete
		}
		break;
	default:
		break;
	}
}

bool	CWarpManager::SetWarp(const Vector3 &vecPos, const Vector3 &vecVel, float fHeadingInRadians, bool teleportVehicle, bool bSnapToGround, float fRadius, bool bFromMap)
{
	bool bIsActive = IsActive();
	bool bInSwitch = g_PlayerSwitch.IsActive();

	Assertf( bIsActive == false, "Warp Already in progress - you can't have two warps running at the same time" );
	Assertf( bInSwitch == false, "Player switch in progress - you can't warp while player switch is active" );

#if __INCLUDE_DEBUG_SPEW
	Displayf("CWarpManager::SetWarp() Called");
	sysStack::PrintStackTrace();
#endif	//__INCLUDE_DEBUG_SPEW

	if( !bIsActive && !bInSwitch )
	{
		// Set to fade in/out. Can be overridden after the warp is set
		FadeOutAtStart(true);
		FadeInWhenComplete(true);
		// Set to fade out the player
		FadeOutPlayer(false);
		InstantFillPopulation(false);
		// Set not owned by script, Should be set after SetWarp() is called using SetScriptThreadId()
		m_bOwnedByScript = false;
		m_scriptThreadId = THREAD_INVALID;

		// Copy passed vars
		m_Pos = vecPos;
		m_Vel = vecVel;
		Assertf(fHeadingInRadians >= -PI && fHeadingInRadians <= PI, "Invalid fHeadingInRadians inside SetWarp(): %f",fHeadingInRadians);
		m_fHeading = fHeadingInRadians;
		m_bSnapToGround = bSnapToGround;
		m_StopWarp = false;
		m_pPhysical = NULL;
		m_bFromMap = bFromMap;

#if __BANK
		m_SetCamPos = false;
		m_SetTimeOfDay = false;
#endif	//__BANK

		CVehicle *pVehicle = NULL;
		CPed *pPed = FindPlayerPed();
		if( pPed->GetIsInVehicle() )
		{
			pVehicle = FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true);
		}

		if (pVehicle && teleportVehicle)
		{
			m_pPhysical = pVehicle;
		}
		else
		{
			m_pPhysical = pPed;
		}

		if(Verifyf( m_pPhysical.Get() != NULL , "No Player to warp, canceling warp."))
		{
			m_fRadius = CalculateLoadSceneRadius(fRadius);
			m_Flags = CalculateLoadSceneFlags(m_fRadius);
			m_eState = STATE_FADE_OUT;
			return true;
		}
	}

	return false;
}

void	CWarpManager::StopWarp()
{
	if( IsActive() )
	{
		m_StopWarp = true;
	}
}

float	CWarpManager::CalculateLoadSceneRadius(float &radius)
{
	// If a radius wasn't specified (radius < 0.0f), calculate one from current position 
	if(radius <= 0.0f)
	{
		Vector3 curPos = VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().GetPosition());
		float distance2 = ( curPos - m_Pos ).Mag2();

		// If we get more if these, implement a table lookup
		if( distance2 < (50.0f*50.0f) )
		{
			return 50.0f;
		}

		if( distance2 < (100.0f*100.0f) )
		{
			return 100.0f;
		}
		radius = 600.0f;		// max
	}
	return radius;
}

u32	CWarpManager::CalculateLoadSceneFlags(const float &radius)
{
	u32	flags = 0;

	/*	// For now, don't use this since we might want to warp into an interior that's less than 50m away, and this would be very bad.
	if(radius <= 50.0f)
	{
		flags |= FLAG_NO_LOADSCENE;		// Below 50m, don't perform a loadscene
	}
	*/
	
	if(radius <= 50.0f)
	{
		flags |= FLAG_HD_ASSETS;		// Below 100m, request only HD ASSETS
	}

	return flags;
}

bool CWarpManager::HaveVehicleModsLoaded()
{
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();
	while (i--)
	{
		CVehicle* veh = pool->GetSlot(i);
		if (!veh || !veh->IsPersonalVehicle() || !veh->GetIsVisibleInSomeViewportThisFrame())
			continue;

		if (!veh->GetVariationInstance().HaveModsStreamedIn(veh))
			return false;
	}
	return true;
}

void CWarpManager::SetScriptThreadId(scrThreadId scriptThreadId)
{
	m_bOwnedByScript = true; 
	m_scriptThreadId=scriptThreadId;

#if __INCLUDE_DEBUG_SPEW
	Displayf("CWarpManager: Setting Owned by %s", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif	//__INCLUDE_DEBUG_SPEW
}


#if __BANK

void	CWarpManager::SetWasDebugStart(bool val)
{
	m_WasDebugStart = val;
}

void	CWarpManager::SetPostWarpCameraPos(Vector3 &pos, Vector3 &front)
{
	m_CamPos = pos;
	m_CamFront = front;

	if(!PARAM_noWarpCameraPosSet.Get())
	{
		m_SetCamPos = true;
	}
}

void	CWarpManager::SetPostWarpTimeOfDay(s32 hours, s32 minutes)
{
	m_ClockHours = hours;
	m_ClockMinutes = minutes;

	if(!PARAM_noWarpTimeOfDaySet.Get())
	{
		m_SetTimeOfDay = true;
	}
}


void	CWarpManager::DoPreWarpDebugStuff()
{
	if(!m_WasDebugStart)
	{
		if( m_SetTimeOfDay )
		{
			CClock::SetTime(m_ClockHours, m_ClockMinutes, 0);
		}
	}
}


void	CWarpManager::DoPostWarpDebugStuff()
{
	if(!m_WasDebugStart)
	{
		if( m_SetCamPos )
		{
			camInterface::GetDebugDirector().GetFreeCamFrameNonConst().SetPosition(m_CamPos);
			camInterface::GetDebugDirector().GetFreeCamFrameNonConst().SetWorldMatrixFromFront(m_CamFront);
			camInterface::GetDebugDirector().ActivateFreeCam();
		}
		else if(camInterface::GetDebugDirector().IsFreeCamActive())
		{
			camInterface::GetDebugDirector().DeactivateFreeCam();
		}

	}

	// If this warp was a debugstart warp (from command line), then the next one won't be!
	SetWasDebugStart(false);
}

#endif	// BANK


//////////////////////////////////////////////////////////////////////////
