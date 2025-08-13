/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UIReplayScaleformController.cpp
// PURPOSE : manages the Scaleform scripts required during replay
// AUTHOR  : Andy Keeble
// STARTED : 4/14/2014
//
/////////////////////////////////////////////////////////////////////////////////

#include "Frontend/UIReplayScaleformController.h"

#if GTA_REPLAY

#include "atl/string.h"
#include "scene/world/GameWorld.h"
#include "control/replay/ReplayExtensions.h"
#include "control/replay/replay.h"
#include "Peds/Ped.h"
#include "frontend/NewHud.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "text/TextFile.h"

CUIReplayScaleformController::CUIReplayScaleformController()
	: m_noofMoviesBeingDrawn( 0 )
{
	memset(m_movieIndices, -1, sizeof(m_movieIndices));
	memset(m_movieReplayIndices, -1, sizeof(m_movieReplayIndices));
	memset(m_movieBeingDeletedIndex, -1, sizeof(m_movieBeingDeletedIndex));
	memset(m_moviesDrawnList, -1, sizeof(m_moviesDrawnList));
	memset(m_moviesToNotDelete, -1, sizeof(m_moviesToNotDelete));

	m_noofMoviesBeingDrawn = 0;
	m_noofMoviesToNotDelete = 0;

	m_currentOnFlags = 0;
	m_previousOnFlags = 0;

	m_movieToPopulate = OverlayType::Noof;

	m_IconID[0] = 4;
	m_IconID[1] = 2;
	m_IconID[2] = 39;
	m_IconID[3] = 43;
	m_IconID[4] = 5;
	m_IconID[5] = 24;
	m_IconID[6] = 1;
	m_IconID[7] = 6;

	m_replayState = ReplayStates::InGame;
	m_prevReplayState = ReplayStates::InGame;
}

CUIReplayScaleformController::~CUIReplayScaleformController()
{

}

void CUIReplayScaleformController::Update()
{
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
	if (!pPlayerPed)
		return;

	// NOTE: replay states are a bit awkward at the moment. replay menu state is more 'at the start of a replay' as that is when setup stuff is done
	// this will probably get rejigged as the replay system's timings will probably be too

	m_prevReplayState = m_replayState;

	// running a replay
	if(CReplayMgr::IsEditModeActive())	 
	{
		m_replayState = ReplayStates::InReplay;
		if (ReplayReticuleExtension::HasExtension(pPlayerPed))
		{
			PerformReplayData(pPlayerPed);
		}
	}
	// in replay menu
	else if ((CReplayMgr::IsEnabled() && !CReplayMgr::IsEditModeActive() && !CReplayMgr::IsRecordingEnabled()) ||
		CReplayCoordinator::IsPendingNextClip() )
	{
		// check to see if we're entring this state. a good time to reset stuff before playing a new replay
		if (m_replayState != ReplayStates::InClipMenu)
		{
			// delete movies
			for (u8 index = 0; index < OverlayType::Noof; ++index)
			{
				DeleteReplayMovie(index);
			}

			// then reset do not delete movie data (we'll generate it again per replay)
			// if there are any movies set to not delete, clear the array and run
			memset(m_moviesToNotDelete, -1, sizeof(m_moviesToNotDelete));
			m_noofMoviesToNotDelete = 0;
		}

		m_replayState = ReplayStates::InClipMenu;
	}
	// in game
	else
	{
		m_replayState = ReplayStates::InGame;

		AnalyseReplayData();

		if(CReplayMgr::IsEnabled() && !CReplayMgr::IsEditModeActive() && CReplayMgr::IsRecordingEnabled() && pPlayerPed)
		{
			RecordReplayData(pPlayerPed);
		}
	}

	ClearFrameMovieData();
}

void CUIReplayScaleformController::RenderReplay()
{
	if(m_replayState != ReplayStates::InReplay)
		return;

	for (u8 index = 0; index < OverlayType::Cellphone_iFruit; ++index)
	{
		if (m_movieReplayIndices[index] >= 0)
		{
			if (CScaleformMgr::IsMovieActive(m_movieReplayIndices[index]))
			{
				// if not a phone, make sure the movie is full screen
				GFxMovieView::ScaleModeType scaleMode = GFxMovieView::SM_NoBorder;
				if (CHudTools::GetWideScreen())
					scaleMode = GFxMovieView::SM_ShowAll;

				int CurrentRenderID = 1;
				CScaleformMgr::ChangeMovieParams(m_movieReplayIndices[index], Vector2(0,0), Vector2(1,1), scaleMode, CurrentRenderID);

				float fTimer = fwTimer::GetSystemTimeStep();

				CScaleformMgr::RenderMovie(m_movieReplayIndices[index], fTimer, true, true);
			}
		}
	}
}

void CUIReplayScaleformController::RenderReplayPhone()
{
	if(m_replayState != ReplayStates::InReplay)
		return;

	for (u8 index = OverlayType::Cellphone_iFruit; index < OverlayType::Noof; ++index)
	{
		if (m_movieReplayIndices[index] >= 0)
		{
			if (CScaleformMgr::IsMovieActive(m_movieReplayIndices[index]))
			{
				// set movie params here too, makes it a bit more optimised than scrupt_hud doing it per frame
				GFxMovieView::ScaleModeType scaleMode = GFxMovieView::SM_ExactFit;

				int CurrentRenderID = 0;
				CScaleformMgr::ChangeMovieParams(m_movieReplayIndices[index], Vector2(0,0), Vector2(0.2f,0.356f), scaleMode, CurrentRenderID);

				float fTimer = fwTimer::GetSystemTimeStep();

				CScaleformMgr::RenderMovie(m_movieReplayIndices[index], fTimer, true, true);
			}
		}
	}
}

void CUIReplayScaleformController::RenderReplayStatic()
{
	CNewHud::GetReplaySFController().RenderReplay();
}

void CUIReplayScaleformController::RenderReplayPhoneStatic()
{
	CNewHud::GetReplaySFController().RenderReplayPhone();
}

void CUIReplayScaleformController::SetTelescopeMovieID(s32 movieIndex)
{
	ClearOldIndices(movieIndex);

	m_movieIndices[OverlayType::Telescope] = movieIndex;
	m_movieHashName[OverlayType::Telescope] = atStringHash(CScaleformMgr::GetMovieFilename(movieIndex));
}

void CUIReplayScaleformController::SetBinocularsMovieID(s32 movieIndex)
{
	ClearOldIndices(movieIndex);

	m_movieIndices[OverlayType::Binoculars] = movieIndex;
	m_movieHashName[OverlayType::Binoculars] = atStringHash(CScaleformMgr::GetMovieFilename(movieIndex));
}

void CUIReplayScaleformController::SetMovieID(u32 overlayType, s32 movieIndex)
{
	ClearOldIndices(movieIndex);

	m_movieIndices[overlayType] = movieIndex;
	m_movieHashName[overlayType] = atStringHash(CScaleformMgr::GetMovieFilename(movieIndex));
}

void CUIReplayScaleformController::SetTurretCamMovieID(s32 movieIndex)
{
	ClearOldIndices(movieIndex);

	m_movieIndices[OverlayType::Turret_Camera] = movieIndex;
	m_movieHashName[OverlayType::Turret_Camera] = atStringHash(CScaleformMgr::GetMovieFilename(movieIndex));
}

void CUIReplayScaleformController::StoreMovieBeingDrawn(s32 movieIndex)
{
	// this can happen on loading screens when it renders more than updates
	// but we don't care about loading screens!
	if (m_noofMoviesBeingDrawn >= maxSymMovies)
		return;

	// shouldn't be storing movies more than once, as there is only one draw call
	// but consider doing a debug-only check for this later

 	m_moviesDrawnList[m_noofMoviesBeingDrawn] = movieIndex;
 	++m_noofMoviesBeingDrawn;
}

void CUIReplayScaleformController::PerformReplayData(CPed *pPlayerPed)
{
	PopulateCellphone();

	for (u8 index = 0; index < OverlayType::Noof; ++index)
	{
		// sent phone render target as active
		if (index >= OverlayType::Cellphone_iFruit)
		{
			if (m_movieReplayIndices[index] >= 0)
			{
				gRenderTargetMgr.UseRenderTargetForReplay((CRenderTargetMgr::RenderTargetId)0);
			}
		}

		// get if this overlay is meant to be displaying
		switch(index)
		{
		case OverlayType::Telescope:
			{
				m_currentOnFlags |= ReplayHUDOverlayExtension::GetTelescope(pPlayerPed) && CReplayMgr::IsUsingRecordedCamera() ? (1 << index) : 0;
			}
			break;
		case OverlayType::Binoculars:
			{
				m_currentOnFlags |= ReplayHUDOverlayExtension::GetBinoculars(pPlayerPed) && CReplayMgr::IsUsingRecordedCamera() ? (1 << index) : 0;
			}
			break;
		case OverlayType::Turret_Camera:
			{
				m_currentOnFlags |= ReplayHUDOverlayExtension::GetTurretCam(pPlayerPed) && CReplayMgr::IsUsingRecordedCamera() ? (1 << index) : 0;
			}
			break;
		case OverlayType::Drone_Camera:
			{
				m_currentOnFlags |= ReplayHUDOverlayExtension::GetDroneCam(pPlayerPed) && CReplayMgr::IsUsingRecordedCamera() ? (1 << index) : 0;
			}
			break;
		case OverlayType::Cellphone_iFruit:
		case OverlayType::Cellphone_Badger:
		case OverlayType::Cellphone_Facade:
			{
				u8 cellphoneNumber = ReplayHUDOverlayExtension::GetCellphone(pPlayerPed) + OverlayType::Cellphone_iFruit - 1;
				if (cellphoneNumber == index)
					m_currentOnFlags |= (1 << index);
			}
			break;
		default : break;
		};

		// load and destroy movie on if it's recorded as visible
		if (m_currentOnFlags != m_previousOnFlags)
		{
			if ((m_currentOnFlags & (1 << index) ))
			{
				// record how many movies there are now for phones, so we can see if a movie is created to be populated
				// as the movie could already be in use
				// CScaleformMgr::GetNoofMoviesWithAnyState() loops through all movie slots, getting the state...not efficent, but didn't want to mess with CScaleformMgr
				s32 countOfMoviesWithAnyActivity = 0;
				if (index >= OverlayType::Cellphone_iFruit)
				{
					countOfMoviesWithAnyActivity = CScaleformMgr::GetNoofMoviesActiveOrLoading();
				}

				switch(index)
				{
				case OverlayType::Telescope:
					{
						m_movieReplayIndices[index] = CScaleformMgr::CreateMovie("observatory_scope", Vector2(0,0), Vector2(0,0), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE);
					}
					break;
				case OverlayType::Binoculars:
					{ 
						m_movieReplayIndices[index] = CScaleformMgr::CreateMovie("binoculars", Vector2(0,0), Vector2(0,0), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE);
					}
					break;
				case OverlayType::Turret_Camera:
					{
						m_movieReplayIndices[index] = CScaleformMgr::CreateMovie("turret_cam", Vector2(0,0), Vector2(0,0), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE);
					}
					break;
				case OverlayType::Drone_Camera:
					{
						m_movieReplayIndices[index] = CScaleformMgr::CreateMovie("DRONE_CAM", Vector2(0,0), Vector2(0,0), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE);
					}
					break;
				case OverlayType::Cellphone_iFruit:
					{
						m_movieReplayIndices[index] = CScaleformMgr::CreateMovie("CELLPHONE_IFRUIT", Vector2(0,0), Vector2(0,0), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE, false, true);
					}
					break;
				case OverlayType::Cellphone_Badger:
					{
						m_movieReplayIndices[index] = CScaleformMgr::CreateMovie("CELLPHONE_BADGER", Vector2(0,0), Vector2(0,0), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE, false, true);
					}
					break;
				case OverlayType::Cellphone_Facade:
					{
						m_movieReplayIndices[index] = CScaleformMgr::CreateMovie("CELLPHONE_FACADE", Vector2(0,0), Vector2(0,0), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE, false, true);
					}
					break;
				default : break;
				};

				// if the movie is a phone, and is not already drawing in the game ...flag to be populated
				// if the phone already exists, just use the existing screen ...as is the standard we use for things like TVs
				// otherwise it'll be a nightmare to store and restore the phone back for something that will be rarely seen
				if (index >= OverlayType::Cellphone_iFruit)
				{
					if (CScaleformMgr::GetNoofMoviesActiveOrLoading() > countOfMoviesWithAnyActivity)
					{
						m_movieToPopulate = index;
					}
					else
					{
						uiAssertf(m_noofMoviesToNotDelete < maxSymMovies, "CUIReplayScaleformController::PerformReplayData - movies to not delete - out of array range");
						
						m_moviesToNotDelete[m_noofMoviesToNotDelete] = m_movieReplayIndices[index];
						++m_noofMoviesToNotDelete;
					}
				}

				m_movieBeingDeletedIndex[index]  = -1;
			}
			else
			{
				DeleteReplayMovie(index);
			}
		}
	}
}

void CUIReplayScaleformController::PopulateCellphone()
{
	if (m_movieToPopulate < OverlayType::Noof)
	{
		uiAssertf(m_movieToPopulate >= OverlayType::Cellphone_iFruit, "CUIReplayScaleformController::PopulateCellphone - movie not a cellphone. this shouldn't be possible");

		if (CScaleformMgr::IsMovieActive(m_movieReplayIndices[m_movieToPopulate]))
		{
			int cellphoneID = m_movieToPopulate - OverlayType::Cellphone_iFruit;

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SOFT_KEYS_COLOUR", -1))
			{
				u8 hudColour = 18;
				CRGBA rgba = CHudColour::GetRGBA((eHUD_COLOURS)hudColour);
				u8 red = rgba.GetRed();
				u8 green = rgba.GetGreen();
				u8 blue = rgba.GetBlue();

				CScaleformMgr::AddParamInt(2);
				CScaleformMgr::AddParamInt(red);
				CScaleformMgr::AddParamInt(green);
				CScaleformMgr::AddParamInt(blue);
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SOFT_KEYS_COLOUR", -1))
			{
				u8 hudColour = 9;
				CRGBA rgba = CHudColour::GetRGBA((eHUD_COLOURS)hudColour);
				u8 red = rgba.GetRed();
				u8 green = rgba.GetGreen();
				u8 blue = rgba.GetBlue();

				CScaleformMgr::AddParamInt(1);
				CScaleformMgr::AddParamInt(red);
				CScaleformMgr::AddParamInt(green);
				CScaleformMgr::AddParamInt(blue);
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SOFT_KEYS_COLOUR", -1))
			{
				u8 hudColour = 6;
				CRGBA rgba = CHudColour::GetRGBA((eHUD_COLOURS)hudColour);
				u8 red = rgba.GetRed();
				u8 green = rgba.GetGreen();
				u8 blue = rgba.GetBlue();

				CScaleformMgr::AddParamInt(3);
				CScaleformMgr::AddParamInt(red);
				CScaleformMgr::AddParamInt(green);
				CScaleformMgr::AddParamInt(blue);
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_TITLEBAR_TIME", -1))
			{
				CScaleformMgr::AddParamInt(9);
				CScaleformMgr::AddParamInt(50);
				CScaleformMgr::AddParamString((TheText.Get("CELL_926")));
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SLEEP_MODE", -1))
			{
				CScaleformMgr::AddParamInt(0);
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_THEME", -1))
			{
				switch (cellphoneID)
				{
				case 0: CScaleformMgr::AddParamInt(1);
					break;
				case 1: CScaleformMgr::AddParamInt(2);
					break;
				default: CScaleformMgr::AddParamInt(3);
					break;
				};
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_BACKGROUND_IMAGE", -1))
			{
				CScaleformMgr::AddParamInt(0);
				CScaleformMgr::EndMethod();
			}

			// yep, crops up twice ???
			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_PROVIDER_ICON", -1))
			{
				switch (cellphoneID)
				{
				case 0: CScaleformMgr::AddParamInt(1);
					break;
				case 1: CScaleformMgr::AddParamInt(3);
					break;
				default: CScaleformMgr::AddParamInt(2);
					break;
				};
				CScaleformMgr::AddParamInt(5);
				CScaleformMgr::EndMethod();
			}

			for (u8 i = 0; i < noofPhoneApps; ++i)
			{
				if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_DATA_SLOT", -1))
				{
					CScaleformMgr::AddParamInt(1);
					CScaleformMgr::AddParamInt(i);
					CScaleformMgr::AddParamInt(m_IconID[i]);
					CScaleformMgr::AddParamInt(0);
					CScaleformMgr::AddParamString(TheText.Get("Cell_0"));
					CScaleformMgr::EndMethod();
				}
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "DISPLAY_VIEW", -1))
			{
				CScaleformMgr::AddParamInt(1);
				//	CScaleformMgr::AddParamInt(4);
				CScaleformMgr::EndMethod();
			}


			// crops up twice too
			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SLEEP_MODE", -1))
			{
				CScaleformMgr::AddParamInt(0);
				CScaleformMgr::EndMethod();
			}


			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SOFT_KEYS", -1))
			{
				CScaleformMgr::AddParamInt(2);
				CScaleformMgr::AddParamInt(1);
				CScaleformMgr::AddParamInt(2);
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SOFT_KEYS", -1))
			{
				CScaleformMgr::AddParamInt(3);
				CScaleformMgr::AddParamInt(1);
				CScaleformMgr::AddParamInt(4);
				CScaleformMgr::EndMethod();
			}

			if (CScaleformMgr::BeginMethod(m_movieReplayIndices[m_movieToPopulate], SF_BASE_CLASS_SCRIPT, "SET_SOFT_KEYS", -1))
			{
				CScaleformMgr::AddParamInt(1);
				CScaleformMgr::AddParamInt(0);
				CScaleformMgr::AddParamInt(3);
				CScaleformMgr::EndMethod();
			}

			m_movieToPopulate = OverlayType::Noof;
		}
	}
}

void CUIReplayScaleformController::AnalyseReplayData()
{
	for (u8 index = 0; index < OverlayType::Noof; ++index)
	{
		if (m_movieIndices[index] == -1)
			continue;
		
		if (!IsMovieDrawing(m_movieIndices[index]))
			continue;

		// check to see if the movie drawing is the same movie
		if (!(m_previousOnFlags & (1 << index)))
		{
			u32 iFilenameHash = atStringHash(CScaleformMgr::GetMovieFilename(m_movieIndices[index]));
			if (m_movieHashName[index] != iFilenameHash)
			{
				m_movieIndices[index] = -1;
				continue;
			}
		}

		m_currentOnFlags |= (1 << index);
	}
}

void CUIReplayScaleformController::RecordReplayData(CPed *pPlayerPed)
{
	if(!ReplayHUDOverlayExtension::HasExtension(pPlayerPed))
	{
		ReplayHUDOverlayExtension::Add(pPlayerPed);
	}

	if(ReplayHUDOverlayExtension::HasExtension(pPlayerPed))
	{
		// record if it was showing in previous frame, if it's not in the current one ...gets rid of any glitches and not noticeable otherwise

		ReplayHUDOverlayExtension::SetTelescope(pPlayerPed, m_currentOnFlags & (1 << OverlayType::Telescope) || m_previousOnFlags & (1 << OverlayType::Telescope) ? true : false);
		ReplayHUDOverlayExtension::SetBinoculars(pPlayerPed, m_currentOnFlags & (1 << OverlayType::Binoculars) || m_previousOnFlags & (1 << OverlayType::Binoculars) ? true : false);
		ReplayHUDOverlayExtension::SetTurretCam(pPlayerPed, m_currentOnFlags & (1 << OverlayType::Turret_Camera) || m_previousOnFlags & (1 << OverlayType::Turret_Camera) ? true : false);
		ReplayHUDOverlayExtension::SetDroneCam(pPlayerPed, m_currentOnFlags & (1 << OverlayType::Drone_Camera) || m_previousOnFlags & (1 << OverlayType::Drone_Camera) ? true : false);

		u8 cellphoneNumber = 0;
		cellphoneNumber = m_currentOnFlags & (1 << OverlayType::Cellphone_iFruit) ? 1 : cellphoneNumber;
		cellphoneNumber = m_currentOnFlags & (1 << OverlayType::Cellphone_Badger) ? 2 : cellphoneNumber;
		cellphoneNumber = m_currentOnFlags & (1 << OverlayType::Cellphone_Facade) ? 3 : cellphoneNumber;	

		if (cellphoneNumber == 0)
		{
			cellphoneNumber = m_previousOnFlags & (1 << OverlayType::Cellphone_iFruit) ? 1 : cellphoneNumber;
			cellphoneNumber = m_previousOnFlags & (1 << OverlayType::Cellphone_Badger) ? 2 : cellphoneNumber;
			cellphoneNumber = m_previousOnFlags & (1 << OverlayType::Cellphone_Facade) ? 3 : cellphoneNumber;
		}

		ReplayHUDOverlayExtension::SetCellphone(pPlayerPed, cellphoneNumber);
	}
}

void CUIReplayScaleformController::DeleteReplayMovie(u8 index)
{
	// put index into a new array for deleting movies, as it can take a few frames to delete
	// ...but we need to make the main index invalid to not render
	if (m_movieReplayIndices[index] >= 0)
	{
		m_movieBeingDeletedIndex[index] = m_movieReplayIndices[index];
		m_movieReplayIndices[index] = -1;
		m_movieToPopulate = OverlayType::Noof;

		// make sure we don't delete any movies we're using in-game currently (e.g. phone)
		if (ReplayMovieCanBeDeleted(m_movieBeingDeletedIndex[index]))
		{
			// but if we can, request the movie to be deleted if it exists
			if (CScaleformMgr::IsMovieActive(m_movieBeingDeletedIndex[index]))
			{
				CScaleformMgr::RequestRemoveMovie(m_movieBeingDeletedIndex[index] );
				m_movieBeingDeletedIndex[index]  = -1;
			}
		}

	}
}

bool CUIReplayScaleformController::ReplayMovieCanBeDeleted(s32 movieIndex)
{
	for (u8 index = 0; index < maxSymMovies; ++index)
	{
		if (m_moviesToNotDelete[index] == movieIndex)
			return false;
	}
	
	return true;
}

void CUIReplayScaleformController::ClearFrameMovieData()
{
	memset(m_moviesDrawnList, -1, sizeof(maxSymMovies));
	m_noofMoviesBeingDrawn = 0;

	m_previousOnFlags = m_currentOnFlags;
	m_currentOnFlags = 0;
}

bool CUIReplayScaleformController::IsMovieDrawing(s32 movieIndex) const
{
 	for (u8 index = 0; index < m_noofMoviesBeingDrawn; ++index)
 	{
 		if (m_moviesDrawnList[index] == movieIndex)
 			return true;
 	}

	return false;
}

void CUIReplayScaleformController::ClearOldIndices(s32 movieIndex)
{
	for (u8 index = 0; index < m_noofMoviesBeingDrawn; ++index)
	{
		if (m_movieIndices[index] == movieIndex)
			m_movieIndices[index] = -1;
	}
}

#endif // GTA_REPLAY
// eof
