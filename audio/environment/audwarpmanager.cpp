

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
#include "vehicles/VehicleFactory.h"

AUDIO_ENVIRONMENT_OPTIMISATIONS()
//OPTIMISATIONS_OFF()
//////////////////////////////////////////////////////////////////////////
// Warp Manager
//////////////////////////////////////////////////////////////////////////
#if __BANK

#include "audWarpManager.h"

RegdPhy					audWarpManager::m_pPhysical;
audWarpManager::eState	audWarpManager::m_eState;
Vector3					audWarpManager::m_Pos;
Vector3					audWarpManager::m_Vel;
float					audWarpManager::m_fHeading;
float					audWarpManager::m_fRadius;
bool					audWarpManager::m_bSnapToGround = false;
u32						audWarpManager::m_StartTime;
bool					audWarpManager::m_StopWarp = false;
bool					audWarpManager::m_bFadeOutPlayer = false;
bool					audWarpManager::m_bFadeOutAtStart = true;
bool					audWarpManager::m_bFadeInWhenComplete = true;
bool					audWarpManager::m_bOwnedByScript = false;
scrThreadId				audWarpManager::m_scriptThreadId = THREAD_INVALID;
u32						audWarpManager::m_Flags;

BANK_ONLY(Vector3		audWarpManager::m_CamPos);
BANK_ONLY(Vector3		audWarpManager::m_CamFront);
BANK_ONLY(bool			audWarpManager::m_SetCamPos = false);
BANK_ONLY(s32			audWarpManager::m_ClockHours);
BANK_ONLY(s32			audWarpManager::m_ClockMinutes);
BANK_ONLY(bool			audWarpManager::m_SetTimeOfDay = false);
BANK_ONLY(bool			audWarpManager::m_WasDebugStart = false);

BANK_ONLY(bool			audWarpManager::sm_WaitUntilSceneLoaded = false; )
BANK_ONLY(bool			audWarpManager::sm_FadeCamera = true; )

void	audWarpManager::Update()
{
	if(!IsActive())
		return;

	// Check we're not currently doing a player switch.
	Assertf( g_PlayerSwitch.IsActive() == false, "Trying to Warp while a player switch is active, this is not advised or supported.");

	switch(m_eState)
	{
	case	STATE_START_WARP:
		{
			naDisplayf("-------------------------------------------------");
			naDisplayf("STATE_START_WARP");
			if(m_pPhysical.Get() != NULL)
			{
				if(m_pPhysical->GetIsTypePed())
				{
					// Add the root offset and assume that the coords are decent.
					// This should help alleviate MP warp issues where the screen can't be faded out. (B*881685)
					m_Pos.z += static_cast<CPed*>(m_pPhysical.Get())->GetCapsuleInfo()->GetGroundToRootOffset();
				}
				m_pPhysical->Teleport(m_Pos, m_fHeading, false, true, false, true, false, true);
				m_pPhysical->SetHeading(m_fHeading);
				m_Vel.RotateZ(m_fHeading);
				m_pPhysical->SetVelocity(m_Vel);
				naDisplayf("STATE_START_WARP : Teleported to <%f,%f,%f>",m_Pos.x,m_Pos.y,m_Pos.z);
				m_eState = STATE_PLACE_ON_GROUND;
			}
			else
			{
				// The player has vanished, just fade back in
				naDisplayf("STATE_START_WARP : NULL PHYSICAL, STATE_INACTIVE" );
				m_eState = STATE_INACTIVE;
			}
		}
		break;
	case STATE_PLACE_ON_GROUND: 
		{
			naDisplayf("-------------------------------------------------");
			naDisplayf("STATE_PLACE_ON_GROUND");
			if(m_pPhysical.Get() != NULL)
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

					if( CPedPlacement::FindZCoorForPed(TOP_SURFACE, &m_Pos, NULL, NULL, NULL, 1000.f, 1000.f, flags) )
					{
						m_pPhysical->SetPosition(m_Pos, true, true);
						m_pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
						m_pPhysical->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().GetPosition()));
						m_Pos.z += static_cast<CPed*>(m_pPhysical.Get())->GetCapsuleInfo()->GetGroundToRootOffset();
						m_pPhysical->Teleport(m_Pos, m_fHeading, false, true, false, true, false, true);
						m_pPhysical->SetHeading(m_fHeading);
						m_Vel.RotateZ(m_fHeading);
						m_pPhysical->SetVelocity(m_Vel);
						naDisplayf("STATE_PLACE_ON_GROUND : Found ground Teleported to <%f,%f,%f>",m_Pos.x,m_Pos.y,m_Pos.z);
						Vec3V lookAt = m_pPhysical->GetTransform().GetForward();

						if(!(m_Flags & FLAG_NO_LOADSCENE))
						{
							s32	flags = CLoadScene::FLAG_INCLUDE_COLLISION;
							// Did we want to only request HD Assets?
							if( m_Flags & FLAG_HD_ASSETS )
							{
								flags |= CLoadScene::FLAG_LONGSWITCH_CUTSCENE;	// To request HD Assets Only
							}

							g_LoadScene.Start( VECTOR3_TO_VEC3V(m_Pos), lookAt, m_fRadius, false, flags, CLoadScene::LOADSCENE_OWNER_DEBUG );

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
							m_eState = STATE_INACTIVE;
						}
					}
					else
					{
						CPed* pPed = static_cast<CPed*>(m_pPhysical.Get());
						if(pPed && pPed->GetIsInWater())
						{	
							Vec3V lookAt = m_pPhysical->GetTransform().GetForward();

							if(!(m_Flags & FLAG_NO_LOADSCENE))
							{
								s32	flags = CLoadScene::FLAG_INCLUDE_COLLISION;
								// Did we want to only request HD Assets?
								if( m_Flags & FLAG_HD_ASSETS )
								{
									flags |= CLoadScene::FLAG_LONGSWITCH_CUTSCENE;	// To request HD Assets Only
								}

								g_LoadScene.Start( VECTOR3_TO_VEC3V(m_Pos), lookAt, m_fRadius, false, flags, CLoadScene::LOADSCENE_OWNER_DEBUG );

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
								m_eState = STATE_INACTIVE;
							}
						}
					}
				}
				else
				{
					// The player has vanished, just fade back in
					naDisplayf("STATE_PLACE_ON_GROUND : NOT A PED, STATE_INACTIVE" );
					m_eState = STATE_INACTIVE;
				}
			}
			else
			{
				// The player has vanished, just fade back in
				naDisplayf("STATE_PLACE_ON_GROUND : NULL PHYSICAL, STATE_INACTIVE" );
				m_eState = STATE_INACTIVE;
			}
		}
		break;
	case	STATE_DO_WARP:
		{
			naDisplayf("-------------------------------------------------");
			naDisplayf("STATE_DO_WARP");

			bool bTerminate = false;

			bool bLoadingComplete = g_LoadScene.IsLoaded();

			bLoadingComplete &= HaveVehicleModsLoaded();

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

			if (bTerminate)
			{
				naDisplayf("STATE_DO_WARP : SCENE LOADED : TERMINATING.");
				g_LoadScene.Stop();
				if(m_pPhysical.Get() != NULL)
				{
					m_pPhysical->EnableCollision();
					m_pPhysical->SetFixedPhysics(false);
					m_pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
					m_pPhysical->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().GetPosition()));
				}
				m_eState = STATE_INACTIVE;
			}
		}
		break;

	default:
		break;
	}
}

bool	audWarpManager::SetWarp(const Vector3 &vecPos, const Vector3 &vecVel, float fHeadingInRadians, bool teleportVehicle, bool bSnapToGround, float fRadius)
{
	if(Verifyf(!IsActive(), "You can't have two warps running at the same time"))
	{
		// Check we're not currently doing a player switch.
		Assertf( g_PlayerSwitch.IsActive() == false , "Trying to Warp while a player switch is active, this is not advised or supported.");

		// Set to fade in/out. Can be overridden after the warp is set
		FadeOutAtStart(true);
		FadeInWhenComplete(true);
		// Set to fade out the player
		FadeOutPlayer(false);
		// Set not owned by script, Should be set after SetWarp() is called using SetScriptThreadId()
		m_bOwnedByScript = false;
		m_scriptThreadId = THREAD_INVALID;

		// Copy passed vars
		m_Pos = vecPos;
		m_Vel = vecVel;
		m_fHeading = fHeadingInRadians;
		m_bSnapToGround = bSnapToGround;
		m_StopWarp = false;
		m_pPhysical = NULL;

#if __BANK
		m_SetCamPos = false;
		m_SetTimeOfDay = false;
#endif	//__BANK

		CVehicle *pVehicle = FindPlayerVehicle(CGameWorld::GetMainPlayerInfo(), true);
		if (pVehicle && teleportVehicle)
		{
			m_pPhysical = pVehicle;
		}
		else
		{
			CPed *pPed = FindPlayerPed();
			m_pPhysical = pPed;
		}

		if(Verifyf( m_pPhysical.Get() != NULL , "No Player to warp, canceling warp."))
		{
			m_fRadius = CalculateLoadSceneRadius(fRadius);
			m_Flags = CalculateLoadSceneFlags(m_fRadius);
			m_eState = STATE_START_WARP;
			return true;
		}
	}

	return false;
}

void	audWarpManager::StopWarp()
{
	if( IsActive() )
	{
		m_StopWarp = true;
	}
}

float	audWarpManager::CalculateLoadSceneRadius(float &radius)
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

u32	audWarpManager::CalculateLoadSceneFlags(const float &radius)
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

bool audWarpManager::HaveVehicleModsLoaded()
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



void	audWarpManager::SetWasDebugStart(bool val)
{
	m_WasDebugStart = val;
}

void	audWarpManager::SetPostWarpCameraPos(Vector3 &pos, Vector3 &front)
{
	m_CamPos = pos;
	m_CamFront = front;

}

void	audWarpManager::SetPostWarpTimeOfDay(s32 hours, s32 minutes)
{
	m_ClockHours = hours;
	m_ClockMinutes = minutes;

}


void	audWarpManager::DoPreWarpDebugStuff()
{
	if(!m_WasDebugStart)
	{
		if( m_SetTimeOfDay )
		{
			CClock::SetTime(m_ClockHours, m_ClockMinutes, 0);
		}
	}
}


void	audWarpManager::DoPostWarpDebugStuff()
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
