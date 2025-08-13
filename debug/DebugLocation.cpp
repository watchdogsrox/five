  
#include "DebugLocation.h"

#include "DebugLocation_parser.h"

#include "parser/manager.h"

#include "Peds/ped.h"
#include "camera/caminterface.h"
#include "camera/debug/debugdirector.h"
#include "camera/gameplay/gameplaydirector.h"
#include "camera/helpers/frame.h"
#include "camera/viewports/ViewportManager.h"
#include "game/clock.h"
#include "scene/streamer/scenestreamermgr.h"
#include "scene/world/gameworld.h"
#include "scene/WarpManager.h"
#include "script/script_debug.h"

#define DEBUG_LOCATION_LIST_FILENAME "common:/data/debug/debugLocationList"

PARAM(debugLocationstart, "Start at this location (index of an element in common:/data/debugLocationList.xml from 1 to n");
PARAM(debugLocationStartName, "Start at this location (name of Item in common:/data/debugLocationList.xml), put quotes around location if it has spaces");


void debugLocation::BeamMeUp()
{
	// Set the camera position
	CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
	if(pPlayer)
	{
		pPlayer->Teleport(playerPos, pPlayer->GetCurrentHeading());

#if !__FINAL
		CScriptDebug::SetPlayerCoordsHaveBeenOverridden(true);
#endif	//	!__FINAL

		//g_SceneStreamerMgr.LoadScene(playerPos);

#if __BANK
		CWarpManager::WaitUntilTheSceneIsLoaded(true);
		CWarpManager::FadeCamera(true);
#endif //__BANK
		Vector3 vecVel(VEC3_ZERO);
		CWarpManager::SetWarp(playerPos, vecVel, 0.f, true, true, 600.f);

#if __BANK
		CWarpManager::SetWasDebugStart(true);
#endif //__BANK

		pPlayer->GetPortalTracker()->ScanUntilProbeTrue();
	}	

	// Set the camera
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if ( debugDirector.IsFreeCamActive() == false )
		debugDirector.ActivateFreeCam();

	//Move free camera to desired place.
	camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();

	freeCamFrame.SetWorldMatrix(cameraMtx, false);

	if( g_SceneToGBufferPhaseNew )
	{
		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->ScanUntilProbeTrue();
		g_SceneToGBufferPhaseNew->GetPortalVisTracker()->CPortalTracker::Update( cameraMtx.d );
	}
	

	// Set hour 
	CClock::SetTime(hour, 0, 0);	
}

void debugLocation::Set(Vector3 &pos, Matrix34 &camMtx, u32 h)
{
	playerPos = pos;
	cameraMtx = camMtx;
	hour = h;
}

void debugLocation::Set(float *data)
{
	// Set the camera position
	playerPos = Vector3(data[0], data[1], data[2]);

	// Set the camera matrix

	cameraMtx = Matrix34(	data[3], data[7], data[11], 
							data[4], data[8], data[12], 
							data[5], data[9], data[13],
							data[6], data[10], data[14]);

	// Set hour 
	hour = (u32)data[15];
}

bool debugLocation::SetFromCurrent()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	
	if (debugDirector.IsFreeCamActive() == true)
	{
		// Player position
		CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
		if(!pPlayer)
			return false;

		playerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

		camFrame& freeCamFrame = debugDirector.GetFreeCamFrameNonConst();
		cameraMtx = freeCamFrame.GetWorldMatrix();
		
		hour = CClock::GetHour();
		
	}
	else
	{
		camGameplayDirector& director = camInterface::GetGameplayDirector();

		CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
		if(!pPlayer)
			return false;

		playerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

		const camFrame& freeCamFrame = director.GetFrame();
		cameraMtx = freeCamFrame.GetWorldMatrix();

		hour = CClock::GetHour();
	}
	
	return true;
}

void debugLocation::WriteOut(char *buffer)
{
	// Player position
	sprintf(buffer, "%f,%f,%f", playerPos[0], playerPos[1], playerPos[2]);

	// Output camera matrix
	for (u32 i = 0; i < 12; i++)
	{
		sprintf(buffer, "%s,%f", buffer, cameraMtx.GetElement(i % 4, (i - (i % 4)) / 3));
	}

	// Output hour
	sprintf(buffer, "%s,%f", buffer, (float)hour);
}

void debugLocationList::Init(bkBank* BANK_ONLY(bank))
{
#if __BANK
	m_newLocationName[0]='\0';

	bank->PushGroup("Debug Locations List");

	bank->AddButton("Load", datCallback(MFA(debugLocationList::Load), (datBase*)this));
	bank->AddButton("Save", datCallback(MFA(debugLocationList::Save), (datBase*)this));
	bank->AddButton("Add Current Location", datCallback(MFA(debugLocationList::Add), (datBase*)this));
	bank->AddText("New Loc Name",m_newLocationName, NELEM(m_newLocationName), false);

#endif // __BANK
	
	Load();
	
	currentLocation = -1;
	visiting = false;
	
#if __BANK
	comboList = bank->AddCombo("List",&currentLocation,locationNames.GetCount(),locationNames.GetElements(), datCallback(MFA(debugLocationList::Select), (datBase*)this));

	bkTimeWarp = 30.0f; // 30 seconds
	bank->AddSlider("Time Before Warp",&bkTimeWarp,0.0f,60*5,1.0f);
	bank->AddButton("LetsDoTheTimeWarp",datCallback(MFA(debugLocationList::LetsDoTheTimeWarp), (datBase*)this));

	bank->PopGroup();
#endif // __BANK
}

void debugLocationList::ApplyBootParam()
{
	static bool applied = false;
	
	if( applied == false )
	{
		int startupLoc = 0;
		const char *startupLocName = 0;
		if( PARAM_debugLocationstart.Get(startupLoc) )
		{
			startupLoc--;
			if( startupLoc > -1 && startupLoc < locationList.GetCount() )
			{
				currentLocation = startupLoc;
				Select();
			}
		}
		else if( PARAM_debugLocationStartName.Get( startupLocName ) )
		{
			SetLocationByName( startupLocName );
		}

		applied = true;
	}
}

bool debugLocationList::SetLocationByName(const char* name)
{
	atHashValue hashLocation(name) ;

	for(s32 i = 0; i < locationList.GetCount(); i++)
	{
		if (locationList[i]->name == name)
		{
			currentLocation = i;
			Select();
			return true;
		}
	}

	return false;
}

void debugLocationList::Visit(float timeBeforeWarp, float timeBeforeCallBack)
{
	Assertf(visiting == false, "Calling debugLocationList::Visit while already visiting");
	Assertf(timeBeforeCallBack == 0.0f || timeBeforeCallBack < timeBeforeWarp, "Callback time happens after warp");
	
	timeWarp = timeBeforeWarp;
	timeCallback = timeBeforeCallBack;
	timeAtCurrentLocation = 0.0f;

	visiting = true;
	justStarted = true;
	cbCalled = false;
}

void debugLocationList::Reset()
{
	for(s32 i = 0; i < locationList.GetCount(); i++)
	{
		delete locationList[i];
	}
	locationList.Reset();

#if __BANK
	locationNames.Reset();

	if( comboList )
		comboList->UpdateCombo("List",&currentLocation,locationNames.GetCount(),locationNames.GetElements(), datCallback(MFA(debugLocationList::Select), (datBase*)this));
#endif // __BANK
}

void debugLocationList::Load()
{
	Reset();

	if (ASSET.Exists(DEBUG_LOCATION_LIST_FILENAME, "xml"))
	{
		PARSER.LoadObject(DEBUG_LOCATION_LIST_FILENAME, "xml", *this, &parSettings::sm_StrictSettings);
	}
	
#if __BANK
	for(s32 i = 0; i < locationList.GetCount(); i++)
	{
		char buffer[255];
		
		if( locationList[i]->name.GetCStr() == NULL )
		{
			sprintf(buffer,"DL %d",i);
			locationList[i]->name.SetFromString(buffer);
		}

		locationNames.Grow() = locationList[i]->name.GetCStr();
	}

	if( comboList )
		comboList->UpdateCombo("List",&currentLocation,locationNames.GetCount(),locationNames.GetElements(), datCallback(MFA(debugLocationList::Select), (datBase*)this));
#endif // __BANK
}	

#if __BANK
void debugLocationList::Save()
{
	AssertVerify(PARSER.SaveObject(DEBUG_LOCATION_LIST_FILENAME, "xml", this, parManager::XML));
}
#endif // __BANK

void debugLocationList::Add()
{
	debugLocation *newLoc = rage_new debugLocation;
	if( newLoc->SetFromCurrent() )
	{
		
#if __BANK
		if( m_newLocationName[0]=='\0'){
			char buffer[255];
			sprintf(buffer,"Debug Location %d",locationList.GetCount());
			newLoc->name.SetFromString(buffer);
		}
		else
			newLoc->name.SetFromString(m_newLocationName);
#else
		char buffer[255];
		sprintf(buffer,"Debug Location %d",locationList.GetCount());
		newLoc->name.SetFromString(buffer);
#endif

		locationList.Grow() = newLoc;
#if __BANK
		locationNames.Grow() = newLoc->name.GetCStr();
	
		comboList->UpdateCombo("List",&currentLocation,locationNames.GetCount(),locationNames.GetElements(), datCallback(MFA(debugLocationList::Select), (datBase*)this));
#endif // __BANK
	}
	else
	{
		delete newLoc;
	}
}

void debugLocationList::Select()
{
	locationList[currentLocation]->BeamMeUp();
}

#if __BANK
void debugLocationList::LetsDoTheTimeWarp()
{
	Visit(bkTimeWarp * 1000.0f);
}
#endif // __BANK

void debugLocationList::Update(float time)
{
	ApplyBootParam();
	
	if( visiting )
	{
		if( justStarted )
		{
			justStarted = false;
			currentLocation=0;
			Select();
			timeAtCurrentLocation = time;
		}
		else
		{
			const float timeSinceWarp = time - timeAtCurrentLocation;
			
			if( currentVisitCallback.IsBound() && false == cbCalled && timeSinceWarp > timeCallback )
			{
				visiting = currentVisitCallback(currentLocation);
				cbCalled = true;
			}

			if( visiting )
			{
				if( timeSinceWarp > timeWarp )
				{
					int nextLocation = currentLocation + 1;
					if( nextLocation < locationList.GetCount() )
					{
						currentLocation = nextLocation;
						Select();
						cbCalled = false;
						timeAtCurrentLocation = time;
					}
					else
					{
						// We're done
						visiting = false;
						Displayf("Debug Location traversal finished.");
					}
				}
			}
		}
	}
	
	if( false == visiting )
	{ // Done.
		currentVisitCallback.Reset();
	}
}
