/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ScaleformMgr.h
// PURPOSE : manages the processing of Scaleform movies in the game
// AUTHOR  : Derek Payne
// STARTED : 02/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCALEFORMMGR_H_
#define _SCALEFORMMGR_H_

// Rage headers
#include "Scaleform/scaleform.h"
#include "Streaming/streamingmodule.h"
#include "streaming/streamingrequest.h"
#include "system/criticalsection.h"
#include "text/TextFile.h"
#include "vector/vector3.h"
#include "atl/array.h"
#include "atl/vector.h"
#include "grcore/effect.h"
#include "grcore/stateblock.h"
#include "templates/dblBuf.h"
#include "fwtl/LinkList.h"
#include "atl/inlist.h"

// game headers
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"


namespace rage {
	class parTreeNode;
};


typedef s32 COMPLEX_OBJECT_ID;

#define MAX_CHARS_FOR_TEXT_STRING	(2176) // WAS 1024, but with Adam's permission, we're bumping it up to accommodate some incredibly magniloquent electronic missives.

#define MAX_SCALEFORM_MOVIES	(50)

#define SF_INVALID_MOVIE		(-1)
#define SF_SCALEFORM_INACTIVE	(-2)

#define TRACK_STALE_REFERENCES (0)

// problems abound with FlipBuffers being called multiple times per 'frame'
// this will help us track them down
#define TRACK_FLIP_BUFFERS ( 0 && !__NO_OUTPUT && !__FINAL )

// some CS are still required in rare cases where its easiest to lock rather than do lots of double buffering
// also we need to force an invoke instantly when we shutdown or initialise movies so this ensures we dont
// conflict on the rare occursions when this happens
#define __SCALEFORM_CRITICAL_SECTIONS (1)

#if __SCALEFORM_CRITICAL_SECTIONS
extern atRangeArray<sysCriticalSectionToken,MAX_SCALEFORM_MOVIES> gs_ScaleformMovieCS;
#endif // __SCALEFORM_CRITICAL_SECTIONS


#define MAX_CHARS_IN_SCALEFORM_METHOD_NAME			(50)
#define MAX_CHARS_IN_SCALEFORM_UNIQUE_STRINGS		(100)  // JeffK needs 100 chars
#define MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING	(2000)
#define MAX_NUM_PARAMS_IN_SCALEFORM_METHOD	(22)
#define MAX_ADDITIONAL_TXDS (13)  // max we know about is 13 seperate TXDs used in the pausemenu Gallery screen

#define MAX_STORED_RETURN_VALUES (20)
#define FRAMES_FOR_RETURNED_VALUE_TO_STAY_VALID	(20)

#define SCALEFORM_PREALLOC_XML_FILENAME	"common:/data/ScaleformPreallocation.xml"

// all movies in V have been set up to use this aspect ratio, nothing to do with game resolution
#define ACTIONSCRIPT_STAGE_SIZE_X (1280)
#define ACTIONSCRIPT_STAGE_SIZE_Y (720)

#if __XENON
#define SCALEFORM_MOVIE_PLATFORM_SUFFIX "_360
#elif RSG_DURANGO
#define SCALEFORM_MOVIE_PLATFORM_SUFFIX "_xboxone"
#elif __PS3
#define SCALEFORM_MOVIE_PLATFORM_SUFFIX "_ps3"
#elif RSG_ORBIS
#define SCALEFORM_MOVIE_PLATFORM_SUFFIX "_ps4"
#elif RSG_PC
#define SCALEFORM_MOVIE_PLATFORM_SUFFIX "_pc"
#endif

enum eSCALEFORM_BASE_CLASS
{
	SF_BASE_CLASS_GENERIC = 0,
	SF_BASE_CLASS_SCRIPT,
	SF_BASE_CLASS_HUD,
	SF_BASE_CLASS_MINIMAP,
	SF_BASE_CLASS_WEB,
	SF_BASE_CLASS_CUTSCENE,
	SF_BASE_CLASS_PAUSEMENU,
    SF_BASE_CLASS_STORE,
	SF_BASE_CLASS_GAMESTREAM,
	SF_BASE_CLASS_VIDEO_EDITOR,
	SF_BASE_CLASS_MOUSE,
	SF_BASE_CLASS_TEXT_INPUT,
	SF_MAX_BASE_CLASSES
};

enum eState
{
	SF_MOVIE_STATE_INACTIVE = 0,		// completely off/unused/deleted etc
	SF_MOVIE_STATE_FLAGGED_FOR_USE,		// not set up yet, but flagged to be used
	SF_MOVIE_STATE_STREAMING_MOVIE,		// loading and waiting on movie itself to stream
	SF_MOVIE_STATE_ACTIVE,				// active & considered in use
	SF_MOVIE_STATE_RESTARTING,
	SF_MOVIE_STATE_SET_TO_DELETE_PENDING_FINAL_UPDATE,		// movie is going to get set to be deleted via the RT but can still update and invoke/callback
	SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_1,					// movie is going to get set to be deleted via the RT and can no longer update/callback
	SF_MOVIE_STATE_SET_TO_REMOVE_STAGE_2,					// movie is going to get set to be deleted without any further checks
	SF_MOVIE_STATE_SET_TO_REMOVE_NO_REF_CHECK  // goes into this state after ref check so we can retry on the assert and get others
};

struct ScaleformMovieTxd
{
	char				cTxdName[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];
	char				cUniqueRefString[MAX_CHARS_IN_SCALEFORM_UNIQUE_STRINGS];
	bool				bReportSuccessToActionScript;
	strLocalIndex		iTxdId;
	strRequest			streamingRequest;   
#if !__NO_OUTPUT
	s32					iSpamCounter;
#endif

	// Call this instead of streamingRequest.HasLoaded, because that one and the downloadable texture manager don't play nice together.
	bool				AlreadyLoaded();
};


enum
{
	METHOD_BUILD_BUFFER_UPDATE_THREAD = 0,
	METHOD_BUILD_BUFFER_RENDER_THREAD,
	METHOD_BUILD_BUFFER_EXTERNAL_INTERFACE_UT = METHOD_BUILD_BUFFER_RENDER_THREAD + NUMBER_OF_RENDER_THREADS,
	METHOD_BUILD_BUFFER_EXTERNAL_INTERFACE_RT,

	MAX_METHOD_BUILD_BUFFERS = METHOD_BUILD_BUFFER_EXTERNAL_INTERFACE_RT + NUMBER_OF_RENDER_THREADS
};


enum eMovieOwnerTags
{
	SF_MOVIE_TAGGED_BY_NONE			= 0,
	SF_MOVIE_TAGGED_BY_CODE			= 1 << 1,
	SF_MOVIE_TAGGED_BY_SCRIPT		= 1 << 2,
};

struct ScaleformMovieStruct
{
	// vars accessed and changed on both threads (so these are double buffered)
	CDblBuf<bool>		bPerformGarbageCollection;
	CDblBuf<bool>		bChangeMovieParams;
	CDblBuf<Vector3>	vPos;
	CDblBuf<Vector3>	vOriginalPos;
	CDblBuf<Vector3>	vRot;
	CDblBuf<Vector2> 	vSize;
	CDblBuf<Vector2> 	vOriginalSize;
	CDblBuf<GFxMovieView::ScaleModeType>	eScaleMode;
	CDblBuf<Vector2>	vWorldSize;
	CDblBuf<bool>		bRender3DSolid;
	CDblBuf<bool>		bRender3D;
	CDblBuf<bool>		bIgnoreSuperWidescreenScaling;
	CDblBuf<u8>			iBrightness;

	// vars accessed on both threads but access is managed by state (so threadsafe) and not changed once state is set to active or only set in certain threadsafe situations
	s32					iMovieId;  // never changed once set up in a threadsafe manner
	s32					iParentMovie;
	char				cFilename[RAGE_MAX_PATH];  // never changed once set up in a threadsafe manner
	eState				iState;
	bool				bUpdateMovie;
	u8					iRendered;
	bool				bDontRenderWhilePaused;
	bool				bForceRemoval;

	// vars only accessed on UpdateThread:
	atVector<ScaleformMovieTxd> requestedTxds;
	bool				bRemovableAtInit;
	u8					movieOwnerTag;

#if __DEV
	s32					iNumRefs;
#endif // __DEV
#if __SF_STATS
	atArray<s32>		allRequestedTxds; // A list of all the txds requested by this movie (so we can display info about them)
#endif

	// vars only accessed on RenderThread:
	float				fCurrentTimestep;
	s32					iDependentMovie;  // id of a movie that this movie may share its assets with
	bool				bNeedsToUpdateThisFrame;

	ScaleformMovieTxd&	CreateNewTxdRequest(strLocalIndex iTxdId, const char* pTxdString, const char* uniqueRef, bool reportSuccessToAS);
};

enum eParamType
{
	PARAM_TYPE_INVALID = 0,
	PARAM_TYPE_NUMBER,
	PARAM_TYPE_BOOL,
	PARAM_TYPE_STRING,
	PARAM_TYPE_EXTERNAL_STRING,
	PARAM_TYPE_GFXVALUE
};

struct ScaleformMethodStruct
{
	struct ParamStruct
	{
		union
		{
			atString *pParamString;
			Double ParamNumber;  // double as thats what Scaleform uses :(
			bool bParamBool;
			GFxValue *pParamGfxValue;

			char *pExternalString;  // used by wrapper
		};

		eParamType param_type;
	};

	s8 iMovieId;
	s8 iMovieIdThisInvokeIsLinkedTo;
	char cMethodName[MAX_CHARS_IN_SCALEFORM_METHOD_NAME];

	ParamStruct params[MAX_NUM_PARAMS_IN_SCALEFORM_METHOD];
	u8 iParamCount;
	eSCALEFORM_BASE_CLASS iBaseClass;
	COMPLEX_OBJECT_ID ComplexObjectIdToInvoke;

	s32 iReturnId;
};


struct ScaleformMethodReturnStruct
{
	s32 iUniqueId;
	GFxValue gfxvalue_return_value;
	u8 iFramesToStayValid;
	atString stringData;
};

#if __SF_STATS
struct ScaleformMemoryStats
{
	size_t m_MovieDefTotal, m_MovieDefUsed;
	size_t m_MovieViewTotal, m_MovieViewUsed;
	size_t m_PreallocTotal, m_PreallocUsed, m_PreallocPeak, m_Overalloc;
	size_t m_PeakAllowed;
	bool m_MPSpecific;

	ScaleformMemoryStats();
};
#endif

enum
{
	INVALID_MOVIE_ID = -1
};

class CScaleformMgr
{
public:
	class CreateMovieParams
	{
	public:
		CreateMovieParams(const char* cMovieFilename) :
			cFilename(cMovieFilename),
			vRot(0.0f, 0.0f, 0.0f),
			iParentMovie(-1),
			iDependentMovie(-1),
			eScaleMode(GFxMovieView::SM_ExactFit),
			bStream(false),
			bForceLoad(false),
			bRemovable(true),
			bRequiresMovieView(true),
			movieOwnerTag(SF_MOVIE_TAGGED_BY_CODE),
			bInitiallyLocked(false),
			bDontRenderWhilePaused(false),
			bIgnoreSuperWidescreenScaling(false)
		{

		}

		const char*					cFilename;
		Vector3						vPos;
		Vector3						vRot;
		Vector2						vSize;
		s32							iParentMovie;
		s32							iDependentMovie;
		GFxMovieView::ScaleModeType	eScaleMode;
		bool						bStream;
		bool						bForceLoad;
		bool						bRemovable;
		bool						bRequiresMovieView;
		bool						bIgnoreSuperWidescreenScaling;
		eMovieOwnerTags				movieOwnerTag;
		bool						bInitiallyLocked;
		bool						bDontRenderWhilePaused;
	};

	static  bool	IsFunctionAvailable(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName);

	static void AddCallbackMethodToBuffer(s32 iMovieCalledFrom, const char *pFunctionName, const GFxValue *pArgs, s32 iNumberOfParams);
	static void ExecuteCallbackMethod(s32 iMovieCalledFrom, const char* methodName, const GFxValue* args, s32 iArgCount);

	//
	// General code functions - other code systems can call these methods
	//
	static	s32		CreateMovie(const char *cFilename, Vector2 vPos, Vector2 vSize, bool bRemovable = true, s32 iParentMovie = -1, s32 iDependentMovie = -1, bool bRequiresMovieView = true, eMovieOwnerTags calledBy = SF_MOVIE_TAGGED_BY_CODE, bool bDontRenderWhilePaused = false, bool bIgnoreSuperWidescreenAdjustment = false);
	static	s32		CreateMovie(const char *cFilename, const Vector3& vPos, const Vector3& vRot, Vector2 vSize, bool bRemovable = true, s32 iParentMovie = -1, s32 iDependentMovie = -1, bool bRequiresMovieView = true, eMovieOwnerTags calledBy = SF_MOVIE_TAGGED_BY_CODE, bool bDontRenderWhilePaused = false);
	static	s32		CreateMovie(CreateMovieParams& params);

	static	s32		CreateMovieAndWaitForLoad(const char *cFilename, Vector2 vPos, Vector2 vSize, bool bRemovable = true, s32 iParentMovie = -1, s32 iDependentMovie = -1, bool bRequiresMovieView = true, eMovieOwnerTags calledBy = SF_MOVIE_TAGGED_BY_CODE, bool bDontRenderWhilePaused = false, bool bIgnoreSuperWidescreenAdjustment = false);
	static	s32		CreateMovieAndWaitForLoad(const char *cFilename, const Vector3& vPos, const Vector3& vRot, Vector2 vSize, bool bRemovable = true, s32 iParentMovie = -1, s32 iDependentMovie = -1, bool bRequiresMovieView = true, eMovieOwnerTags calledBy = SF_MOVIE_TAGGED_BY_CODE, bool bDontRenderWhilePaused = false);
	static	s32		CreateMovieAndWaitForLoad(CreateMovieParams& params);

	static	bool	RequestRemoveMovie(s32 iIndex, eMovieOwnerTags calledBy = SF_MOVIE_TAGGED_BY_CODE);
	static	void	SetScriptRequired(s32 iIndex, bool bScriptRequired);

	static inline bool IsMovieIdInRange(s32 iIndex) { return (iIndex >= 0 && iIndex < sm_ScaleformArray.size() && iIndex < MAX_SCALEFORM_MOVIES); }

#if RSG_PC
	static	void	DeviceLost();
	static	void	DeviceReset();

	static	bool	GetMovieShouldIgnoreSuperWidescreen(s32 iIndex);
#endif

	static	void	PrepareUpdateAllMoviesOnRT();
	static	void	EnsureAllMoviesUpdatedOnRT();
	static	void	CleanupMoviesOnRT();
	
	static void		OverrideOriginalMoviePosition(int iMovieID, Vector2 &vPos);

	static  void	UpdateMovieOnly(s32 iIndex, float fTimeStep = 0.0f);
	static	void	DoNotUpdateThisFrame(s32 iIndex);
	static	void	RenderMovie(s32 iIndex, float fTimeStep = 0.0f, bool bUpdateWhenPaused = false, bool bForceUpdateBeforeRender = false, float fAlpha = 1.0f);
	static	void	RequestRenderMovie3D(s32 iIndex, bool bRenderSolid);


	enum class SDC : u8 {
		Default = 0,
		ForceWidescreen = BIT(0),
		UseFakeSafeZoneOnBootup = BIT(1), // ONLY APPLIED ON DURANGO
		UseUnAdjustedSafeZone = BIT(2)
	};


	static	void	SetMovie3DBrightness(s32 iIndex, s32 iBrightness);
	static	bool	SetMovieDisplayConfig(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, SDC flags = SDC::Default);
	static	void	SetComplexObjectDisplayConfig(s32 objectId, eSCALEFORM_BASE_CLASS iBaseClass);

	static Vector3	GetMoviePos(s32 iIndex);
	static Vector2	GetMovieSize(s32 iIndex);
	static char const *GetMovieFilename(s32 const iIndex);

	static const Vector2& GetScreenSize() { return ms_vScreenSize; }

	static  void	RenderWorldSpace();
	static  void	RenderWorldSpaceSolid();

	static  bool    IsMovieRendering(s32 iIndex);
	static	bool	IsMovieActive(s32 iIndex);
	static	bool	IsMovieUpdateable(s32 iIndex);
	static	bool	IsMovieShuttingDown(s32 iIndex);
	static	bool	HasMovieGotAnyState(s32 iIndex);
	static	bool	CanMovieBeRemovedByScript(s32 iIndex);

	static	s32		GetNoofMoviesActiveOrLoading();

	static	bool	DoesMovieExistInImage(char *pFilename);

	static	s32		FindMovieByFilename(const char *pFilename, bool findStreamingMovies = false );

	static void		ScalePosAndSize(Vector2& vPos, Vector2& vSize, float fScalar);
	static void		AffectRenderSizeOnly(s32 iIndex, float fScalar);
	static void		UpdateMovieParams(s32 iIndex);
	static void		UpdateMovieParams(s32 iIndex, GFxMovieView::ScaleModeType eScaleMode);
	static void		UpdateMovieParams(s32 iIndex, const Vector2& vPos, const Vector2& vSize);
	static void		ChangeMovieParams(s32 iIndex, const Vector2& vPos, const Vector2& vSize, GFxMovieView::ScaleModeType eScaleMode, int CurrentRenderID = -1, bool bOverrideSWScaling = false, u8 iLargeRt = 0);
	static void		ChangeMovieParams(s32 iIndex, const Vector3& vPos, const Vector2& vScale, const Vector2& vWorldSize, const Vector3& vRot, GFxMovieView::ScaleModeType eScaleMode, int CurrentRenderID = -1, bool bOverrideSWScaling = false, u8 iLargeRt = 0);


	static	bool	RestartMovie(s32 iIndex, bool bForce, bool bQuick);
	static	void	CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

#define SF_INVALID_PARAM		(-9999)
#define SF_INVALID_PARAM_SCRIPT	(-1)

	static void CallMethod(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, bool bCallInstantly = false);
	static void CallMethodFromScript(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, s32 iOptionalIndex = -1, float fParam1 = SF_INVALID_PARAM, float fParam2 = SF_INVALID_PARAM, float fParam3 = SF_INVALID_PARAM, float fParam4 = SF_INVALID_PARAM, float fParam5 = SF_INVALID_PARAM, const char *pParam1 = NULL, const char *pParam2 = NULL, const char *pParam3 = NULL, const char *pParam4 = NULL, const char *pParam5 = NULL, bool bTextLookUp = false);

	static bool BeginMethod(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName);
	static bool BeginMethod(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, s32 iOptionalIndex);
	static bool BeginMethodOnComplexObject(COMPLEX_OBJECT_ID ComplexObjectId, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName);
	
	static s32 EndMethodReturnValue(const s32 iExistingReturnValue = 0);
	static void EndMethod(bool bCallInstantly = false);  // DO NOT PASS IN TRUE TO THIS UNLESS THE MOVIE YOU ARE CALLING IT ON IS NOT ACTIVE AT ALL (IE DURING INITS ETC)
	static s32 EndMethodInt(bool bCallInstantly = true);  // DO NOT PASS IN TRUE TO THIS UNLESS THE MOVIE YOU ARE CALLING IT ON IS NOT ACTIVE AT ALL (IE DURING INITS ETC)
	static float EndMethodFloat(bool bCallInstantly = true);  // DO NOT PASS IN TRUE TO THIS UNLESS THE MOVIE YOU ARE CALLING IT ON IS NOT ACTIVE AT ALL (IE DURING INITS ETC)
	static bool EndMethodBool(bool bCallInstantly = true);  // DO NOT PASS IN TRUE TO THIS UNLESS THE MOVIE YOU ARE CALLING IT ON IS NOT ACTIVE AT ALL (IE DURING INITS ETC)
	static GFxValue EndMethodGfxValue(bool bCallInstantly = true);  // DO NOT PASS IN TRUE TO THIS UNLESS THE MOVIE YOU ARE CALLING IT ON IS NOT ACTIVE AT ALL (IE DURING INITS ETC)

	static void AddParamInt(s32 iParam);
	static void AddParamInt64(s64 iParam);
	static void AddParamFormattedInt(s64 iParam, const char* pPrefix = "%s");
	static void AddParamFloat(Double fParam);
	static void AddParamBool(bool bParam);
	static void AddParamString(const char *pParam, bool bConvertToHtml = true);
	static void AddParamString(const char *pParam, s32 iBufferLen, bool bConvertToHtml = true);
	static void AddParamLocString(const char* pParam, bool bConvertToHtml = true);
	static void AddParamLocString(u32 hash,			  bool bConvertToHtml = true);
	static void AddParamGfxValue(GFxValue param);

	static bool ClearReturnValue(s32 iReturnId);
	static bool IsReturnValueSet(s32 iReturnId);

	static s32 GetReturnValueInt(s32 iReturnId);
	static float GetReturnValueFloat(s32 iReturnId);
	static bool GetReturnValueBool(s32 iReturnId);
	static const char *GetReturnValueString(s32 iReturnId);

	static void ForceCollectGarbage(s32 iIndex);
	static bool DoesMovieHaveInvokesOrCallbacksPending(s32 iIndex);

	// PURPOSE: RAII class for locking
	struct AutoLock
	{
		explicit AutoLock(s32 iIndex) : m_Index(iIndex) { LockMovie(iIndex); }
		~AutoLock() { UnlockMovie(m_Index); }
		s32 m_Index;
	};

	static void LockMovie(s32 iIndex); // Used only to temporarily lock a movie during initialization. Should never be used during gameplay
	static void UnlockMovie(s32 iIndex); 

	static sysCriticalSectionToken& GetRealSafeZoneToken() { return sm_RealSafeZoneToken; }

	//
	// end of general code functions - public methods below here are to be called only by Scaleform setup/store code
	//

	static	void	Init(unsigned initMode);
	static	void	Shutdown(unsigned shutdownMode);

	static  void	KillAllMovies();

	static	void	UpdateAtEndOfFrame( bool bCalledDuringRegularUpdate ); //  we need to know if we're calling this during a loading screen's KeepAlive Function
	static  void	UpdateMoviesUntilReadyForDeletion();


	static	void	RemoveInvokesForComplexObject(COMPLEX_OBJECT_ID const c_objectId);
	static	void	FlushInvokeList();
	static	void	FlushCallbackList();

#if __DEV
	static	void	PreallocTest(unsigned initMode);
#endif

	static inline void  BeginFrame()
	{
		Assert(!ms_bFrameBegun);
#if TRACK_FLIP_BUFFERS
		++sm_BeginFrameCount; // in lieu of using SystemFrameCount (which may not be incremented), this lets us track extra pumps
#endif

		if(m_staticMovieMgr)
		{
			ms_bFrameBegun = true;
			m_staticMovieMgr->BeginFrame();

			CleanupMoviesOnRT();  // cleanup any movies (ie garbage collection, resize etc)
			CScaleformComplexObjectMgr::PerformAllOutstandingActionsOnRT();  // update any complex objects before we invoke (since we need to set up these items)
			FlushInvokeList();  // flush any buffered invokes
			CScaleformComplexObjectMgr::ReleaseAnyFlaggedObjects();  // release any complex objects
			PrepareUpdateAllMoviesOnRT();  // update the movie at the end
		}
	}

	static void  EndFrame()
	{
		if(m_staticMovieMgr && ms_bFrameBegun)
		{
			EnsureAllMoviesUpdatedOnRT();
			m_staticMovieMgr->EndFrame();
		}
		ms_bFrameBegun = false;
	}

	static inline void ResetMeshCache()
	{
		if(m_staticMovieMgr)
			m_staticMovieMgr->ResetMeshCache();
	}

	static void FlipBuffers( bool bCalledDuringRegularUpdate ); // we need to know if we're calling this during a loading screen's KeepAlive Function

	static void ForceMovieUpdateInstantly(s32 iIndex, bool bUpdate);
	static s32 CreateFontMovie();
	static void RemoveFontMovie();
	static void AddGlobalFontsToLib(GFxFontLib* lib);
	static bool GetMappedFontName( char const * const unmappedName, GString& out_mappedName );
	static bool DoFontsShareMapping( char const * const unmappedNameA, char const * const unmappedNameB );
	static GFxValue GetActionScriptObjectFromRoot(s32 iId);
	static GFxMovieView *GetMovieView(s32 iId);
	static bool VerifyVarType(GFxValue& var, GFxValue::ValueType requiredType, const char* ASSERT_ONLY(varName), GFxValue& ASSERT_ONLY(movieClip));

	static bool TranslateWindowCoordinateToScaleformMovieCoordinate( float const rawX, float const rawY, s32 const movieId, float& out_x, float& out_y );

#if __BANK
	static void InitWidgets();

	static s32 GetMemoryUsage(const s32 iIndex);
	static void DisplayPerformanceInfo();
	static void PrintPreallocInfo( s32 iIndex, int& linesPrinted );

	static void GatherPerformanceInfo();
	static void UpdateWatchedMovie();
	static strLocalIndex GetMovieStreamingIndex(s32 iIndex);
#endif // __BANK
#if __SF_STATS
	static void GatherMemoryStats(s32 iIndex, ScaleformMemoryStats& stats);
	static void DebugPrintMemoryStats(s32 iIndex);
#endif

	static void LoadDLCData(const char* pFileName);
	static void UnloadDLCData(const char* pFileName);
	static void FixupScaleformAllocation(parTreeNode* pNodeMovie, const char* movieName);
	
	static void LoadPreallocationInfo(const char* pFileName);
	static void HandleNewTextureDependencies();

	static sfScaleformManager*		GetMovieMgr() { return m_staticMovieMgr; }
	static void AddTxdRef(s32 iIndex, const char *pTxdString, const char *pUniqueString, bool bGetTxd, bool bAddPendingRefToLoaded);

	static void CallAddTxdRefResponse( s32 iIndex, const char * pTxdString, const char * pUniqueString, bool success );

	static void RemoveTxdRef(s32 iIndex, const char *pTxdString, bool bFromActionScript);

	static void SetForceShutdown(s32 iIndex);

	static GFxMovieView::ScaleModeType GetRequiredScaleMode(Vector2 const& vPos, Vector2 const& vSize, bool bUseShowAll = false);
	static GFxMovieView::ScaleModeType GetRequiredScaleMode(s32 iIndex, bool bUseShowAll = false);
	static bool ShouldUseWidescreen(s32 iIndex) { return sm_ScaleformArray[iIndex].eScaleMode.GetUpdateBuf() == GFxMovieView::SM_ExactFit; }

protected:
	static bool GetTxdAndTextureNameFromText(const char *pText, char *pTxdName, char *pTextureName);
	static char *FindOriginalFilename(const char *pFilename);

private:
	static bool ProcessIndividualInvoke(s32 const c_buffer, s32 const c_index);
	static void HandleXML(parTreeNode* pFontMapNode);
	static void AutoRespondIfNeeded(eSCALEFORM_BASE_CLASS iBaseClass, s32 iMovieCalledFrom, const char *pAutoRespondString);
	static void CheckTxdInStringHasRef(s32 iMovieId, const char *pString);
	static bool DoesMovieHaveInvokesPending(s32 iIndex);
	static bool DoesMovieHaveCallbacksPending(s32 iIndex);

	static s32 CreateMovieInternal(const CreateMovieParams& params);
	static bool BeginMethodInternal(s32 iIndex, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, s32 iOptionalIndex, COMPLEX_OBJECT_ID ComplexObjectId);

	static bool IsChildMovie(s32 iIndex);
	static	void	UpdateMovie(s32 iIndex, float fTimeStep = 0.0f);

	static  void	UpdateAllMoviesStateMachines();

	static	s32		GetMethodBuildBufferIndex();
	static	bool	InitialiseAndSetActive(s32 iIndex);
	static	bool	DestroyMovieNow(s32 iIndex);
	static	void	InitTextures(s32 iIndex, const char *cFilename);
	static	void	InitMovieAssets(s32 iIndex, const char *cFilename, s32 iAssetIndex);
	static	s32		LoadTextures(const char *cFilename);
	static	void	StreamMovie(s32 iIndex, const char *cFilename, bool bStream, bool bForceLoad, s32 iParentMovie, bool bMovieView);
	static	void	UpdateTxdReferences();
	static	void	ClearScaleformArraySlot(s32 iIndex);

	static	bool	IsAnyMovieRender3D(bool& bNeedEmissivePass);
	static	bool	IsAnyMovieRender3DSolid();

	static	bool	IsFullScreenScaleformMovieBlockingInput();

	static	void	RenderWorldSpaceSolidPass();
	static	void	RenderWorldSpaceDiffusePass();
	static	void	RenderWorldSpaceEmissivePass();
	static	void	InitRenderStateBlocks();
	static	s32		GetMovieNumRefs(s32 iId);

	static grcBlendStateHandle			ms_defDiffuseBlendState;
	static grcDepthStencilStateHandle	ms_defDiffuseDepthStencilState;
	static grcRasterizerStateHandle		ms_defDiffuseRasterizerState;

	static grcBlendStateHandle			ms_defEmissiveBlendState;
	static grcDepthStencilStateHandle	ms_defEmissiveDepthStencilState;
	static grcRasterizerStateHandle		ms_defEmissiveRasterizerState;

	static grcBlendStateHandle			ms_defExitBlendState;
	static grcDepthStencilStateHandle	ms_defExitDepthStencilState;
	static grcRasterizerStateHandle		ms_defExitRasterizerState;

#if TRACK_FLIP_BUFFERS
	static u32 sm_BeginFrameCount;
#endif

#if __BANK
private:
	static s32 ms_iDebugMovieId;
	static float ms_emissiveLevel;
	static char ms_cDebugMovieName[256];
	static char ms_cDebugMethod[256];
	static char ms_cDebugLoadedMovieName[256];
	static char ms_cDebugIgnoreMethod[256];
	static Vector2 ms_vDebugMoviePos;
	static Vector2 ms_vDebugMovieSize;
	static bool ms_bVerifyActionScriptMethods;
	static bool ms_bPrintDebugInfoLog;
	static bool ms_bShowOutlineFor3DMovie;
	public: // so we can turn it on from a simple helper function
	static bool ms_bShowOutlineFor2DMovie;
	static bool ms_bShowMemberVariables;
	static bool ms_bFullNames;
	static bool ms_bWildcardNameisRecursive;
	static bool ms_bDrawStandardVariables;
	static Vector2 ms_vTextOverride;
	static int ms_iMaxMemberVariableDepth;
	private:
	static atHashWithStringBank ms_OutlineFilter;
	static bool ms_bOverrideEmissiveLevel;

	static bool ms_bShowPerformanceInfo;
	static bool ms_bShowPreallocationInfo;
	static bool ms_bShowAllPreallocationInfo;
	static bool ms_bShowRefCountInfo;
	static bool	ms_bDebugOutputOn;
	static bool ms_bFilterAwayDebugLog;

	static void ShutdownWidgets();
	static void ShowFontCacheTexture();

private:
	static void OutputListOfActiveScaleformMoviesToLog();
	static void ReadjustDebugMovie();
	static void LoadDebugMovie();
	static void UnloadDebugMovie();
	static void UpdateLoadedDebugMovieName();
	static void CallDebugMethod();
	static void DebugTurnOnOffOutputForAllMovies();
	static void DebugTurnOnOffOutput(s32 iIndex, bool bWasJustInitialized = true);
public:
	static void PassDebugMethodParams(const char *pDebugMethodString);
	static void RenderDebugMovie();
	static bool ms_bShowExtraDebugInfo;

	static GFxResource* ms_watchedResource;
	static void ScaleformAddRefCB(GFxResource* whichRsc, int oldCount, int newCount);
	static void ScaleformDecRefCB(GFxResource* whichRsc, int oldCount, int newCount);

#endif //__BANK

private:
	static GFxValue EndMethodInternal(bool bCallInstantly, bool bStoreReturnedValue);
	static GFxValue CallMethodInternal(s32 iIndex, s32 iIndexMovieLinkedTo, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, const GFxValue *pArgs, s32 iNumberOfParams, bool bCallInstantly, bool bStoreReturnValue, s32 iReturnId, COMPLEX_OBJECT_ID ComplexObjectIdToInvoke);
	static GFxValue AddMethodToBuffer(s32 iIndex, s32 iIndexMovieLinkedTo, eSCALEFORM_BASE_CLASS iBaseClass, const char *pFunctionName, const GFxValue *pArgs, s32 iNumberOfParams, bool bStoreReturnedValue, COMPLEX_OBJECT_ID ComplexObjectIdToInvoke);
	static bool HasExternalInterfaceBeenCalled();

	static atArray<ScaleformMethodStruct> sm_MethodInvokeInfo[2];
	static atArray<ScaleformMethodStruct> sm_MethodCallbackInfo[2];

	static ScaleformMethodReturnStruct sm_ReturnValuesFromInvoke[MAX_STORED_RETURN_VALUES];
	static ScaleformMethodReturnStruct sm_ReturnValuesForAccess[MAX_STORED_RETURN_VALUES];
	static s32 sm_iMethodInvokeInfoBufferInUse;
	static bool ms_bExternalInterfaceCalled[3];
	static bool ms_bFrameBegun;
	static bool ms_bfullScreenBlockedInputLastFrame;

	static sfScaleformManager*		m_staticMovieMgr;

	static Vector2 ms_vScreenSize;

	static ScaleformMethodStruct sm_ScaleformMethodBuild[MAX_METHOD_BUILD_BUFFERS];

	static s32 ms_iGlobalReturnCounterUT;
	static s32 ms_iGlobalReturnCounterRT;
	static s32 ms_iReturnValueCount;

	static s32 ms_iFontMovieId;

	static sysCriticalSectionToken sm_RealSafeZoneToken;
	static sysCriticalSectionToken sm_InvokeFlushToken;

public:
	static atArray<ScaleformMovieStruct>	 sm_ScaleformArray;

	static char sm_LastMovieName[255];
	static char sm_LastMethodName[255];
	static char sm_LastMethodParamName[MAX_CHARS_IN_SCALEFORM_METHOD_CALL_STRING];
	static bool sm_bIsInsideActionScript;

	static char* sm_CurrTextureDictName;

private:

	// helper class to 
	class CSorted3DMovie
	{

	public:

		CSorted3DMovie() : m_fSqrDist(0.0f), m_iMovieId(INVALID_MOVIE_ID) {}

		bool operator >(const CSorted3DMovie& op) const			{ return m_fSqrDist > op.m_fSqrDist; }
		bool operator >=(const CSorted3DMovie& op) const		{ return m_fSqrDist >= op.m_fSqrDist; }

		float m_fSqrDist;
		int m_iMovieId;

	};

	static fwLinkList<CSorted3DMovie>	ms_Sorted3DMovies;
	static Vector3						ms_LastCameraPos;
	typedef fwLink<CSorted3DMovie>		CSorted3DMovieNode;

};


class CScaleformMovieWrapper
{
public:

	struct Param
	{
		Param() {m_data.param_type = PARAM_TYPE_INVALID;}
		Param(u32 value) {m_data.ParamNumber = (double)value; m_data.param_type = PARAM_TYPE_NUMBER;}
		Param(int value) {m_data.ParamNumber = (double)value; m_data.param_type = PARAM_TYPE_NUMBER;}
		Param(float value) {m_data.ParamNumber = (double)value; m_data.param_type = PARAM_TYPE_NUMBER;}
		Param(double value) {m_data.ParamNumber = value; m_data.param_type = PARAM_TYPE_NUMBER;}
		Param(bool value) {m_data.bParamBool = value; m_data.param_type = PARAM_TYPE_BOOL;}
		Param(const char* value,		bool convertToHtml=true) { m_data.pExternalString = const_cast<char*>(value);			m_data.param_type = PARAM_TYPE_EXTERNAL_STRING; m_convertToHtml = convertToHtml;}
		Param(const atString& value,	bool convertToHtml=true) { m_data.pExternalString = const_cast<char*>(value.c_str());	m_data.param_type = PARAM_TYPE_EXTERNAL_STRING; m_convertToHtml = convertToHtml;}
		Param(GFxValue* value) {m_data.pParamGfxValue = value; m_data.param_type = PARAM_TYPE_GFXVALUE;}

		void AddParam();

	private:
		ScaleformMethodStruct::ParamStruct m_data;
		bool m_convertToHtml;
	};

	CScaleformMovieWrapper(bool bUseSystemTime = true);
	CScaleformMovieWrapper(const char* pFilename, eSCALEFORM_BASE_CLASS iBaseClass = SF_BASE_CLASS_GENERIC, bool bUseSystemTime = true);
	~CScaleformMovieWrapper();
public:

	void AffectRenderSize(float fScalar);
	void Render(bool bForceUpdateBeforeRender = false, float fAlpha = 1.0f);

	void CreateMovie(eSCALEFORM_BASE_CLASS iBaseClass, const char* pFilename, Vector2 vPos = Vector2(0.0f, 0.0f), Vector2 vSize = Vector2(1.0f,1.0f), bool bRemovable = true, s32 iParentMovie = INVALID_MOVIE_ID, s32 iDependentMovie = INVALID_MOVIE_ID, bool bRequiresMovieView = true, eMovieOwnerTags calledBy = SF_MOVIE_TAGGED_BY_CODE, bool bIgnoreSuperWidescreenAdjustment = false);
	void CreateMovieAndWaitForLoad(eSCALEFORM_BASE_CLASS iBaseClass, const char* pFilename, Vector2 vPos = Vector2(0.0f, 0.0f), Vector2 vSize = Vector2(1.0f,1.0f), bool bRemovable = true, s32 iParentMovie = INVALID_MOVIE_ID, s32 iDependentMovie = INVALID_MOVIE_ID, bool bRequiresMovieView = true, eMovieOwnerTags calledBy = SF_MOVIE_TAGGED_BY_CODE, bool bIgnoreSuperWidescreenAdjustment = false);
	void SetUseSystemTime(bool bDoesIt) { m_bUseSystemTime = bDoesIt; }
	void RemoveMovie();
	void Clear();

	bool CallMethod(const char* method,
		Param param1=Param(), Param param2=Param(), Param param3=Param(), Param param4=Param());

	bool BeginMethod(const char* method);
	void EndMethod();
	s32 EndMethodReturnValue(const s32 iExistingReturnValue = 0);
	void AddParam(Param param);
	void AddParamString(const char *pParam, bool bConvertToHtml = true);
	void AddParamLocString(const char *pParam, bool bConvertToHtml = true);
	void AddParamLocString(u32 hash,		   bool bConvertToHtml = true);
	s32 GetMovieID() const  { return m_iMovieId; }
	s32* GetMovieIDPtr()	{ return &m_iMovieId;}

	void ForceCollectGarbage() {CScaleformMgr::ForceCollectGarbage(m_iMovieId);}

	bool SetDisplayConfig(CScaleformMgr::SDC flags = CScaleformMgr::SDC::Default);

	bool IsActive() const;
	bool IsFree() const { return m_iMovieId == INVALID_MOVIE_ID; }

	static void Shutdown();

private:
	s32 m_iMovieId;
	eSCALEFORM_BASE_CLASS	m_iBaseClass;
	bool m_bUseSystemTime;
#if TRACK_STALE_REFERENCES
public:
	inlist_node<CScaleformMovieWrapper> m_StaleListNode;
#endif
};

#if TRACK_STALE_REFERENCES
typedef inlist<CScaleformMovieWrapper, &CScaleformMovieWrapper::m_StaleListNode> SFStaleList;
#endif

class sfCallGameFromFlash : public GFxExternalInterface
{
public:
	virtual void Callback(GFxMovieView *pMovieView, const char* methodName, const GFxValue* args, UInt argCount);
};

DEFINE_CLASS_ENUM_FLAG_FUNCTIONS(CScaleformMgr::SDC);

#endif // _SCALEFORMMGR_H_

// eof
