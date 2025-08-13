///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	MovieManager.h
//
///////////////////////////////////////////////////////////////////////////////

#ifndef MOVIE_MANAGER_H
#define	MOVIE_MANAGER_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "atl/array.h"
#include "atl/map.h"
#include "bink/movie.h"
#include "vectormath/classes.h"
#include "grcore/stateblock.h"
#include "atl/hashstring.h"
#include "system/messagequeue.h"

// game
#include "scene/entity.h"
#include "script/script_memory_tracking.h"
#include "text/TextFile.h"

// replay
#include "control/replay/ReplaySettings.h"

///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class bkCombo;
	class bwMovie;
	class fiStream;
	class audSound;
};


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define MAX_ACTIVE_MOVIES				(8)
#define MAX_SCRIPT_SLOTS				MAX_ACTIVE_MOVIES
#define INVALID_MOVIE_HANDLE			(0)
#define GLOBAL_SCRIPTED_MOVIE_HANDLE	(0xffffffff)
#define INVALID_RT_POOL_IDX				(0xffff)
#define MAX_BINK_BILLBOARDS				(64)

///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CMovieSubtitleContainer  //////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

#define MOVIE_SHOW_SUBTITLE_EVENT	(30)

class CMovieSubtitleEventArg
{
public:

	atHashString m_cName;		// The has name of the textID
	float		 m_fSubtitleDuration;
	PAR_SIMPLE_PARSABLE;
};

// An attempt to re-use cutfevent struct so we can use the same data from XML
class CMovieSubtitleEvent
{
public:

	float					m_fTime;			// Start Time
	int						m_iEventId;			// Event ID, must be 30
	s32						m_iEventArgsIndex;
	CMovieSubtitleEventArg *m_pEventArgsPtr;
	s32						m_iObjectId;
	PAR_SIMPLE_PARSABLE;
};

class CMovieSubtitleContainer
{
public:

	enum
	{
		SUB_LOAD_STATE_UNLOADED = 0,		// Start state, none
		SUB_LOAD_STATE_REQUESTED_TITLES,	// Titles have been requested, use to await completion
		SUB_LOAD_STATE_REQUESTED_TEXTBLOCK,	// TextBlock has been requested, use to await completion
		SUB_LOAD_STATE_LOADED				// Subs are loaded
	};

	atString							m_TextBlockName;
	atArray<CMovieSubtitleEventArg *>	m_pCutsceneEventArgsList;
	atArray<CMovieSubtitleEvent *>		m_pCutsceneEventList; 

	static	int		SortByTime(CMovieSubtitleEvent * const *a, CMovieSubtitleEvent * const *b)
	{
		const CMovieSubtitleEvent *pEventA = *a;
		const CMovieSubtitleEvent *pEventB = *b;
		if( pEventA->m_fTime > pEventB->m_fTime )
		{
			return 1;
		}
		else if( pEventA->m_fTime < pEventB->m_fTime )
		{
			return -1;
		}
		return 0;
	}

	void	SortEventListByTime()
	{
		m_pCutsceneEventList.QSort(0,-1,SortByTime);
	}

	PAR_SIMPLE_PARSABLE;
};


//  CMovieMgr  ////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CMovieMgr
{	
	friend class CNetwork;
	friend class CTVPlaylistManager;
#if GTA_REPLAY
	friend class ReplayMovieController;
	friend class ReplayMovieControllerNew;
#endif

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

								CMovieMgr					();
								~CMovieMgr					();

		void					DumpDebugInfo(const char* title);

		void 					Init		    			(unsigned initMode);
		void 					Shutdown    				(unsigned shutdownMode);

		void 					UpdateFrame					();
		u32						PreloadMovie				(const char* pName) { return PreloadMovie(pName, true); }

		bool					IsHandleValid				(u32 handle) const { return handle != INVALID_MOVIE_HANDLE; }

		int						PreloadMovieFromScript		(const char* pName);
		u32						GetHandleFromScriptSlot		(int slot)	const;

		u32						GetHandle					(int slot)
		{
			return m_movies[slot].GetId();
		}

		void					Release						(u32 id);

		bool					BeginDraw					(u32 id);
		void					EndDraw						(u32 id);

		float					GetTime						(u32 id);	// returns percentage to match SetTime
		float					GetTimeReal					(u32 id);	// returns percentage to match SetTime
		void					SetTime						(u32 id, float percentage);
		float					SetTimeReal					(u32 id, float time);
		void					SetVolume					(u32 id, float vol);
		void					Mute						(u32 id);

		void					SetShouldSkip				(u32 id, bool shouldSkip);

		void					Stop						(u32 id);
		void					Play						(u32 id);

		void					SetLooping					(u32 id, bool bSet);

		bool					IsPlaying					(u32 id) const;
		bool					IsLoaded					(u32 id) const;

		u32						GetWidth					(u32 id) const;
		u32						GetHeight					(u32 id) const;

		float					GetCurrentFrame				(u32 id) const;
		float					GetPlaybackRate				(u32 id) const;
		float					GetNextKeyFrame				(u32 id, float time) const;

		void					MuteAll						();
		void					PauseAll					();
		void					ResumeAll					();

		bool					IsAnyPlaying				() const;

		u32						GetNumMovies				() const;
		u32						GetNumRefs					() const;

		void					AttachAudioToEntity			(u32 handle, CEntity *entity);
		void					SetFrontendAudio			(u32 handle, bool frontend);
		float					GetMovieDurationQuick(const char *pMovieName);

#if __BANK
		void					InitWidgets					();
		void					RenderDebug					();
		void					RenderDebug3D				();
#endif	//	__BANK

#if __SCRIPT_MEM_CALC
		void					AddMemoryAllocationInfo		(void *pAddressOfAllocatedMemory, u32 SizeOfAllocatedMemory);
		void					RemoveMemoryAllocationInfo	(void *pAddressOfMemoryAllocation);
		u32						GetMemoryUsage				(u32 handle) const;
#endif	//	__SCRIPT_MEM_CALC

		static void* Allocate(u32 size, u32 alignment);
		static void	Free(void* ptr);
		static void* AllocateAsync(u32 size, u32 alignment);
		static void	FreeAsync(void* ptr);

		static void* AllocateFromFragCache(u32 size, u32 alignment);
		static void	FreeFromFragCache(void* ptr);

		static void* AllocateTexture(u32 size, u32 alignment, void* owner);
		static void	FreeTexture(void* ptr);

		static void ProcessBinkAllocs();

#if GTA_REPLAY
		static bool	ShouldControlPlaybackRate();
#endif	//GTA_REPLAY

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: ////////////////////////

		enum MovieFlags {
			MOVIEFLAG_PAUSED			= BIT(0),
			MOVIEFLAG_PLAYING			= BIT(1),
			MOVIEFLAG_DELETE			= BIT(2),
			MOVIEFLAG_RENDERING			= BIT(3),
			MOVIEFLAG_WAIT_FOR_AUDIO	= BIT(4),
			MOVIEFLAG_ASYNC_FAIL    	= BIT(5),
			MOVIEFLAG_STOP				= BIT(6),
			MOVIEFLAG_PAUSE				= BIT(7),
			MOVIEFLAG_DOESNT_EXIST		= BIT(8),
		};

		///////////////////////////////////////////////////////////////////////////
		// CMovie
		///////////////////////////////////////////////////////////////////////////
		class CMovie
		{

#if GTA_REPLAY
#define		MAX_MOVIE_FILENAME_LEN	(64)
#endif

#if __BANK
			friend class CTVPlaylistManager;
#endif	//__BANK

		public:
							CMovie					() : m_pMovie(NULL), m_pSound(NULL), m_hashId(INVALID_MOVIE_HANDLE), m_volume(0.0f), m_flags(0), m_refCount(0), m_frontendAudio(false)	{};
							~CMovie					()																	{ Reset();};
			
			void			Play					();
			void			Stop					();
			void			Pause					();
			void			Resume					();

			void			Reset					();
			bool			Preload					(const char* pName, bool bAlpha = false);

			void			StartLoadSubtitles		(const char* pName);
			void			UpdateLoadSubtitles();
			void			ReleaseSubtitles		();
			void			UpdateSubtitles			();
			static void		EnableSubtitles			(bool bOnOff)	{ ms_bSubsEnabled = bOnOff; }
			static void		ResetAllSubtitleText	()
			{
				ms_bSubsEnabled = false;
				for(int i=0;i<NUM_BINK_MOVIE_TEXT_SLOTS;i++)
				{
					ms_TextSlotRefs[i] = 0;
					ms_TextSlotMarkedForDelete[i] = false;
				}
			}
			static void		ProcessCancelledSubtitleRequests();

			void			IncRefCount				()																		{ m_refCount++; };
			void			DecRefCount				()																		{ m_refCount--; };
			u32				GetRefCount				()			const														{ return m_refCount; };		

			void			Release					();

			bool			BeginDraw				();
			void			EndDraw					();
			void			UpdateFrame				();

			float			GetMovieDurationQuick(const char *pName);

			void			SetMovie				(bwMovie* pMovie)														{ m_pMovie = pMovie; }
			void			SetTime					(float percentage);
			float			SetTimeReal(float time);
			float			GetTime					();
			float			GetTimeReal				();

			void			SetId					(u32 id)																{ m_hashId = id; }

#if GTA_REPLAY
			void			SetName					(const char *pName)
			{
				m_MovieFileName[0] = 0;
				if(pName)
				{

					strncpy(m_MovieFileName, pName, MAX_MOVIE_FILENAME_LEN);
				}
			}
#endif


			void			SetVolume				(float volume);
			void			RestoreVolume			();
			void			SetShouldSkip			(bool shouldSkip);

			void			ResetFlags				()																		{ m_flags = 0; }

			void			SetDoesntExist			(bool val)																{ val ? (m_flags |= MOVIEFLAG_DOESNT_EXIST) : (m_flags &= ~MOVIEFLAG_DOESNT_EXIST); }
			bool			GetDoesntExist			()	const																{ return ((m_flags & MOVIEFLAG_DOESNT_EXIST) != 0); }

			void			SetIsRendering			(bool val)																{ val ? (m_flags |= MOVIEFLAG_RENDERING) : (m_flags &= ~MOVIEFLAG_RENDERING); }
			void			SetIsPlaying			(bool val)																{ FastAssert(!GetStopFlag() || !val); val ? (m_flags |= MOVIEFLAG_PLAYING) : (m_flags &= ~MOVIEFLAG_PLAYING); }
			void			SetIsPaused				(bool val)																{ val ? (m_flags |= MOVIEFLAG_PAUSED) : (m_flags &= ~MOVIEFLAG_PAUSED); }
			void			SetAsyncFail			(bool val)																{ val ? (m_flags |= MOVIEFLAG_ASYNC_FAIL) : (m_flags &= ~MOVIEFLAG_ASYNC_FAIL); }
			void			SetStopFlag				(bool val)																{ FastAssert(!IsPlaying() || !val); val ? (m_flags |= MOVIEFLAG_STOP) : (m_flags &= ~MOVIEFLAG_STOP); }
			void			SetPauseFlag			(bool val)																{ val ? (m_flags |= MOVIEFLAG_PAUSE) : (m_flags &= ~MOVIEFLAG_PAUSE); }
			void			MarkForDelete			()																		{ 
																																m_flags |= MOVIEFLAG_DELETE; 
																																ResetDeleteCount();
																															}
			void			UnmarkForDelete			()																		{ m_flags &= ~MOVIEFLAG_DELETE; }

			bool			GetStopFlag				()			const														{ return ((m_flags & MOVIEFLAG_STOP) != 0); }
			bool			GetPauseFlag			()			const														{ return ((m_flags & MOVIEFLAG_PAUSE) != 0); }
			bool			IsMarkedForDelete		()			const														{ return ((m_flags & MOVIEFLAG_DELETE) != 0); }
			bool			IsLoaded				()			const														{ return m_pMovie != NULL && m_pMovie->IsLoaded(); }
			bool			IsPlaying				()			const														{ return ((m_flags & MOVIEFLAG_PLAYING) != 0); }
			bool			HasAsyncFailed          ()			const														{ return ((m_flags & MOVIEFLAG_ASYNC_FAIL) != 0); }
			const bwMovie*	GetMovie				()			const														{ return m_pMovie; }
			bwMovie*		GetMovie				()																		{ return m_pMovie; }
			u32				GetId					()			const														{ return m_hashId; }

#if GTA_REPLAY
			const char		*GetName				()			const														{ return m_MovieFileName;}
			CEntity*		GetEntity				()		    const														{ return m_pEntity;	}
			bool			IsFrontendAudio			()			const														{ return m_frontendAudio; }
#endif

			float			GetVolume				()			const														{ return m_volume; }
			bool			IsFlagSet				(u32 flag)	const														{ return ((m_flags & flag) != 0); }
			u32				GetWidth				()			const;
			u32				GetHeight				()			const;
			s32				GetFramesRemaining		()			const;

			float			GetFrame() const;
			float			GetPlaybackRate() const;
			float			GetNextKeyFrame(float time) const;

			bool			IsRendering				()																		{ return ((m_flags & MOVIEFLAG_RENDERING) != 0); }

			float			ComputeVolumeOffset		()			const;

			void			AttachAudioToEntity		(CEntity* entity) { m_pEntity = entity; }
			void			SetFrontendAudio		(bool frontend) { m_frontendAudio = frontend; }

#if __SCRIPT_MEM_CALC
			u32				GetMemoryUsage			()			const;
			void			AddMemoryAllocationInfo	(void *pAddressOfAllocatedMemory, u32 SizeOfAllocatedMemory);
			bool			RemoveMemoryAllocationInfo(void *pAddressOfAllocatedMemory);
#endif	//	__SCRIPT_MEM_CALC

			static CMovie	*GetLastPlayed()	{ return ms_pLastPlayedMovie; }

			void LoadedAsyncCb(bool* succeeded);

			void		StopSound();

			void		ResetDeleteCount()		{ m_DeleteCounter = 0; }
			void		IncDeleteCount()		{ m_DeleteCounter++; }
			u32			GetDeleteCount()		{ return m_DeleteCounter; }

		private:

			void	WaitTillLoaded();

			void		CreateSound();

			bwMovie*	m_pMovie;
			audSound	*m_pSound;
			RegdEnt		m_pEntity;
			u32			m_hashId;

#if GTA_REPLAY
			char		m_MovieFileName[MAX_MOVIE_FILENAME_LEN];
#endif

			float		m_volume;
			u32			m_flags;
			u32			m_refCount;
			bool		m_frontendAudio;
			u32			m_DeleteCounter;


			// Subtitle storage
			int			m_SubObjIDX;
			int			m_LoadState;
			CMovieSubtitleContainer	*m_pSubtitles;
			int			m_LastEventIndex;	// 0 - start index
			int			m_LastShownIndex;	// -1 - no index
			int			m_TextSlotID;
			static	bool ms_bSubsEnabled;
			static	int ms_TextSlotRefs[NUM_BINK_MOVIE_TEXT_SLOTS];
			static	bool ms_TextSlotMarkedForDelete[NUM_BINK_MOVIE_TEXT_SLOTS];
			static	atFixedArray<int, 16> m_CancelledRequests;
			// Subtitle storage

			static		CMovie	*ms_pLastPlayedMovie;

#if __SCRIPT_MEM_CALC
			static		sysCriticalSectionToken ms_AllocMapMutex;
			atMap< void*, u32 > m_MapOfMemoryAllocations;
#endif	//	__SCRIPT_MEM_CALC
		};
		///////////////////////////////////////////////////////////////////////////

public:
		static void	EnableSubtitles	(bool bOnOff)	{ CMovie::EnableSubtitles(bOnOff); }
private:

		u32						PreloadMovie		(const char* pName, bool hasAlpha);

		CMovie*					GetFreeMovie		();
		CMovie*					FindMovie			(u32 hashId);
		const CMovie*			FindMovie			(u32 hashId) const;

public:
		void					ReleaseAll			();
private:

		void					Delete				(u32 id);
		void					DeleteAll			();

		int						FindFreeScriptSlot	(u32 handle) const;
		void					ReleaseScriptSlot	(u32 handle);

		void					ReleaseOnFailure	(u32 id);

		static void				GetFullName			(char* buffer, const char* movieName);

#if __BANK

		void					PlayMovie			();
		void					StopMovie			();


		static bkCombo*			ms_movieCombo;
		static s32				ms_movieIndex;


public:
		// Made public so the debug renderer can be used by the TV Channel stuff
		static u32				ms_movieSize;
		static bool				ms_debugRender;
		static u32				ms_curMovieHandle;
		static void				InitRenderStateBlocks();
private:

		friend class CBinkDebugTool;

		static grcBlendStateHandle			ms_defBlendState;
		static grcDepthStencilStateHandle	ms_defDepthStencilState;
		static grcRasterizerStateHandle		ms_defRasterizerState;

		static grcBlendStateHandle			ms_debugBlendState;
		static grcDepthStencilStateHandle	ms_debugDepthStencilState;
		static grcRasterizerStateHandle		ms_debugRasterizerState;
#endif	//	__BANK

#if __SCRIPT_MEM_CALC
		static CMovie*						ms_pCurrentMovieForAllocations;
#endif	//	__SCRIPT_MEM_CALC

		u32						m_scriptSlots[MAX_SCRIPT_SLOTS];

		atArray<CMovie>			m_movies;


		struct sAllocData
		{
			u32 size;
			u32 alignment;
			void* ptr;
			bool alloc;
		};
		static sysMessageQueue<sAllocData, 32, true> ms_pendingAllocQueue;
		static sysMessageQueue<sAllocData, 32, true> ms_doneAllocQueue;
};



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CMovieMgr	g_movieMgr;

#if __BANK
class CBinkDebugTool
{

public:

private: 

	static void			Init					(u32 initMode);
	static void			Shutdown				(u32 shutdownMode);

	static void			InitWidgets				();

	static void			Update					();
	static void			RenderDebug				();

	static void			PlaceInfrontOfCamera	();
	static void			RemoveInfrontOfCamera	();
	static void			RemoveAll				();

	static void			AddBillboard			(Vec3V_In vPos, Vec3V_In vForward, Vec3V_In vUp, u32 movieHandle);

	static u32			RequestMoviePlayback	();

	static void			InitRenderStateBlocks	();

	static grcBlendStateHandle			ms_defBlendState;
	static grcDepthStencilStateHandle	ms_defDepthStencilState;
	static grcRasterizerStateHandle		ms_defRasterizerState;

	static grcBlendStateHandle			ms_debugBlendState;
	static grcDepthStencilStateHandle	ms_debugDepthStencilState;
	static grcRasterizerStateHandle		ms_debugRasterizerState;

	struct binkDebugBillboard 
	{
		Vec3V vForward;
		Vec3V vUp;
		Vec3V vPos;
		u32	  movieHandle;
	};

	static atArray<binkDebugBillboard>	ms_billboards;

	static bkCombo*						ms_movieCombo;
	static float						ms_camDistance;
	static s32							ms_movieIndex;
	static float						ms_movieSizeScale;
	static s32							ms_numMovies;
	static u32							ms_numRefs;

	friend class CMovieMgr;

};


#endif


#endif // MOVIE_MANAGER_H
