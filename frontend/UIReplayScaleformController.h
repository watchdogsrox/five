/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : UIReplayScaleformController.h
// PURPOSE : manages the Scaleform scripts required during replay
// AUTHOR  : Andy Keeble
// STARTED : 4/14/2014
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_UI_REPLAY_SCALEFORM_CONTROLLER_H_
#define INC_UI_REPLAY_SCALEFORM_CONTROLLER_H_

#include "control/replay/replaysettings.h"

#if GTA_REPLAY

class CPed;

class CUIReplayScaleformController
{
public:

	struct OverlayType
	{
		enum 
		{
			Telescope,
			Binoculars,
			Turret_Camera,
			Drone_Camera,
			Cellphone_iFruit,
			Cellphone_Badger,
			Cellphone_Facade,

			Noof
		};
	};

	struct ReplayStates
	{
		enum Enum
		{
			InGame,
			InClipMenu,
			InReplay
		};
	};

	enum 
	{
		noofPhoneApps = 8,
		maxSymMovies = 10
	}; 


	CUIReplayScaleformController();
	virtual ~CUIReplayScaleformController();

	void Update();
	void RenderReplay();
	void RenderReplayPhone();

	static void RenderReplayStatic();
	static void RenderReplayPhoneStatic();

	void SetTelescopeMovieID(s32 movieIndex);
	void SetBinocularsMovieID(s32 movieIndex);
	void SetTurretCamMovieID(s32 movieIndex);
	void SetMovieID(u32 overlayType, s32 movieIndex);

	void StoreMovieBeingDrawn(s32 movieIndex);

	bool IsRunningClip() const { return m_replayState == ReplayStates::InReplay; }
	bool ChangedStateThisFrame() const { return m_replayState != m_prevReplayState; }

private:
	void PerformReplayData(CPed *pPlayerPed);
	void PopulateCellphone();
	void AnalyseReplayData();
	void RecordReplayData(CPed *pPlayerPed);
	void DeleteReplayMovie(u8 index);
	bool ReplayMovieCanBeDeleted(s32 movieIndex);
	void ClearFrameMovieData();

	bool IsMovieDrawing(s32 movieIndex) const;
	void ClearOldIndices(s32 movieIndex);

	s32 m_movieIndices[OverlayType::Noof];
	u32 m_movieHashName[OverlayType::Noof];
	s32 m_movieReplayIndices[OverlayType::Noof];
	s32 m_movieBeingDeletedIndex[OverlayType::Noof];

	u8 m_currentOnFlags;
	u8 m_previousOnFlags;

	u8 m_movieToPopulate;
	
	s32 m_moviesDrawnList[maxSymMovies];
	u8 m_noofMoviesBeingDrawn;

	s32 m_moviesToNotDelete[maxSymMovies];
	u8 m_noofMoviesToNotDelete;

	ReplayStates::Enum m_replayState;
	ReplayStates::Enum m_prevReplayState;

	u8 m_IconID[noofPhoneApps];
};

#endif // GTA_REPLAY

#endif  // INC_UI_REPLAY_SCALEFORM_CONTROLLER_H_

// eof

