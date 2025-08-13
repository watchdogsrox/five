//
// name:		viewports.h
// description:	Class controlling the game viewports
//
#ifndef INC_VIEWPORTMGR_H_
#define INC_VIEWPORTMGR_H_

#include "camera/viewports/Viewport.h"
#include "renderer/RenderPhases/RenderPhase.h"
#include "fwrenderer/renderthread.h"

#define MAX_VIEWPORT_STACK_SIZE 3 // if exceeded DONT increase (unless you are sure) : its likely somebody is forgetting to pop their viewport!

#if __DEV
	#define LERPING_TESTBED // test harness for DW.
#endif // #if __DEV

// Frefs
class CRenderPhaseCascadeShadows;
class CRenderPhaseEntitySelect;
class CRenderPhaseHeight;
class CRenderPhaseScanned;
class CRenderPhaseDeferredLighting_LightsToScreen;
class CViewport;
class CRenderPhaseMirrorReflection;
class RenderPhaseSeeThrough;

// Defines
#define VIEWPORT_LEVEL_INIT_FADE_TIME	20	// ms
#define VIEWPORT_LEVEL_EXIT_FADE_TIME	26	// ms
//#define VIEWPORT_INTRO_FADE_WAIT		0.25f	// the time to wait to trigger a fade in at game startup ( normalised  0 - 1 )

#define DRAW_WIDESCREEN_BORDERS_ON_TOP_ALWAYS // there are two possible ways to draw widescreen borders... can test both methods out via this.

//--------------------------
// A collection of viewports
class CViewportManager
{
	friend class dlCmdRenderPhasesDrawInit;

public:
	CViewportManager()	
	{	
		m_pGameViewport				= NULL;
		m_pPrimaryOrthoViewport		= NULL;
		m_pPlayerSettingsViewport	= NULL;
		m_gameViewportIndex			= -1;	
		Reset(); 
	}

	~CViewportManager() { }

	void				Reset();
	void				Init();
	void				Shutdown();

	void				ShutdownRenderPhases();
	void				RecomputeViewports();

#if RSG_PC
	static void			DeviceLost();
	static void			DeviceReset();
#endif

	void				InitSession();
	void				ShutdownSession();

	void				Process();

	CViewport*			GetViewport(s32 index)		const		{ return m_viewports[index];							}
	CViewport*			GetSortedViewport(s32 index)	const	{ return GetViewport(m_sortedViewportIndicies[index]);	}

	CViewport*			GetCurrentViewport() const;
	void				PushViewport(CViewport *pViewport);
	void				PopViewport();
	void				ResetViewportStack();
	s32					GetViewportStackDepth()	const	{ return m_stackSize; } // during the game it is normally 1 i.e. there is one viewport on the stack.

	s32					ComputeRenderOrder();
	s32					GetNumViewports()		const	{ return m_viewports.GetCount();						}
	s32					GetNumSortedViewports()	const	{ return m_sortedViewportIndicies.GetCount();			}

	void				ComputeCompositeRenderSettings(CViewport *pVp,CRenderPhase *pPhase,CRenderSettings *pSetMe);
	void				SetDefaultGlobalRenderSettings();

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	static int			ComputeReflectionResolution(int width, int height, int reflectionQuality);
#if RSG_PC
	static void			ComputeVendorMSAA(grcTextureFactory::CreateParams& params);
#endif
#endif

	void				ProcessRenderPhaseCreationAndDestruction();
	void				CreateFinalScreenRenderPhaseList();
	void				BuildRenderLists();
	void				RenderPhases();

	void				SortFinalRenderList();

	void				ResetRenderPhaseLists();

	void				AddRenderPhase(CRenderPhase* pPhase);
	void				SetVisibilityFlagForAllPhases(const u16 flag);
	void				ClearVisibilityFlagForAllPhases(const u16 flag);
	void				ToggleVisibilityFlagForAllPhases(const u16 flag);

	grcViewport*		CalcViewportForFragManager();
	grcViewport*		GetFragManagerViewport()							{ return &m_fragManagerViewport;				}
	const Matrix34*		GetFragManagerMatrix()								{ return (Matrix34*)&m_fragManagerViewport.GetCameraMtx(); }

	bool				AreWidescreenBordersActive()				const	{ return m_bWidescreenBorders;					}
	void				SetWidescreenBorders(bool b, s32 duration = 1000);
	bool				IsUsersMonitorWideScreen();	
	bool				DoesViewportExist(s32 viewportId)			const	{ return (FindByIdNoAssert(viewportId) != NULL); }

	CViewport*			Add(CViewport *pViewport);			
	CViewport*			Set(s32 i, CViewport *pViewport);	
	void				Delete(s32 idx);					
	CViewport*			FindById(s32 id)			const;					
	CViewport*			FindByIdNoAssert(s32 id)	const;			
	void				DeleteById(s32 id);				
	
	void				RecacheViewports(); // DW - this could be done a bit better - refactor one day, but for now we need raw speed right now.

	void				CacheGameViewport();
	void				CachePrimaryOrthoViewport();
	void				CachePlayerSettingsViewport();
	void				CacheGameViewportIndex();

	const grcViewport*	GetCurrentGameGrcViewport() const			{ return &m_cachedGameGrcViewport[gRenderThreadInterface.GetCurrentBuffer()]; }
	const grcViewport*	GetUpdateGameGrcViewport() const			{ return &m_cachedGameGrcViewport[gRenderThreadInterface.GetUpdateBuffer()]; }
	const grcViewport*	GetRenderGameGrcViewport() const			{ return &m_cachedGameGrcViewport[gRenderThreadInterface.GetRenderBuffer()]; }
	void				CacheGameGrcViewport(const grcViewport& viewport);

	const camFrame&		GetCurrentGameViewportFrame() const			{ return m_cachedGameViewportFrame[gRenderThreadInterface.GetCurrentBuffer()]; }
	const camFrame&		GetUpdateGameViewportFrame() const			{ return m_cachedGameViewportFrame[gRenderThreadInterface.GetUpdateBuffer()]; }
	const camFrame&		GetRenderGameViewportFrame() const			{ return m_cachedGameViewportFrame[gRenderThreadInterface.GetRenderBuffer()]; }
	void				CacheGameViewportFrame(const camFrame& frame);

	CViewport*			GetGameViewport()			const			{ return m_pGameViewport;													} 
	CViewport*			GetPrimaryOrthoViewport()	const			{ Assert(m_pPrimaryOrthoViewport);		return m_pPrimaryOrthoViewport;		} 
	CViewport*			GetPlayerSettingsViewport()	const			{ Assert(m_pPlayerSettingsViewport);	return m_pPlayerSettingsViewport;	} 

	bool                PrimaryOrthoViewportExists()                { return (m_pPrimaryOrthoViewport!=NULL);                                   } 

	s32					GetGameViewportIndex()		const			{ Assert(m_gameViewportIndex>=0);		return m_gameViewportIndex;			} 

	CViewport*			GetClosestActive3DSceneViewport(Vector3 &v, float *pDistSqr = NULL);
//	CViewport*			GetClosestEffectiveActive3DSceneViewport(Vector3 &v, float *pEffectiveDistSqr);

	s32					GetTimeWideScreenBordersCommandSet()		{ return m_timeWideScreenBordersCommandSet; }
	s32					GetTimeScrollOnWideScreenBorders()			{ return m_timeScrollOnWideScreenBorders;	}

#if RSG_PC
	static float		GetHalfEyeSeparation()						{ return sm_HalfEyeSeparation; }
#endif

#if __DEBUG
	void				CheckSortedRenderPhaseListValid();
#endif

#if __BANK
	void				RecreateRenderPhases();
//	void				ToggleDeferredStandardLighting();
	void				SetupRenderPhaseWidgets(bkBank& bank);
#endif // __BANK

#if __WIN32PC
	float				GetAspectRatio();
#endif // __WIN32PC

private:
#ifdef LERPING_TESTBED
	void				LerpTestBed();
#endif

public:
	static	void			InitDLCCommands();
	static  void			DLCRenderPhaseDrawInit();
private: 
	static void				RenderPhasesDrawInit(void);
	static void				CopyWidgetSettingsToPhaseCB();
	static void				UpdatePhaseNameListCB();

	static const	int				MAX_RENDER_PHASES_IN_USE=32;
private:
	grcViewport			m_cachedGameGrcViewport[2];
	camFrame			m_cachedGameViewportFrame[2];

	atArray<CViewport*> m_viewports;								// a collection of viewports registered with the manager.
	atArray<s32>		m_sortedViewportIndicies;					// in order in which viewports will be rendered

	CViewport*			m_pGameViewport;							// Cached access to specific viewports
	CViewport*			m_pPrimaryOrthoViewport;					// Cached access to specific viewports
	CViewport*			m_pPlayerSettingsViewport;					// Cached access to specific viewports

	s32					m_gameViewportIndex;						// Cached access to specific viewports

	s32					m_stackSize;								// the current size fo the stack
	CViewport*			m_stackViewports[MAX_VIEWPORT_STACK_SIZE];  // A simple stack of pushed viewports

	grcViewport			m_fragManagerViewport;						// *Temp hack* for review... Specialised handling of viewports for fragmanager

	// To understand the difference between these two variables please see code..
	bool				m_bWidescreenBorders;						// are we displaying widescreen borders?... this doesnt mean we are on a widescreen telly though!
	bool				m_bDrawWidescreenBorders;					// are we to draw actual black borders on top of the screen? 
	
	s32					m_timeWideScreenBordersCommandSet;			// When where we told to start OR stop displaying widescreen borders?
	s32					m_timeScrollOnWideScreenBorders;			// How long does it take the borders to scroll on / off

#if RSG_PC
	static float		sm_HalfEyeSeparation;
#endif

#if __DEV
	bool m_bPrintPhaseList; // make true to see what is going on with this list.
#endif
}; // CViewportManager

//---------------------------------------------------------------------------------------
inline void CViewportManager::SetVisibilityFlagForAllPhases(const u16 flag)
{
	int count = RENDERPHASEMGR.GetRenderPhaseCount();
	for (int x=0; x<count; x++)
	{
		RENDERPHASEMGR.GetRenderPhase(x).GetEntityVisibilityMask().SetFlag( flag );
	}
}

//---------------------------------------------------------------------------------------
inline void CViewportManager::ClearVisibilityFlagForAllPhases(const u16 flag)
{
	int count = RENDERPHASEMGR.GetRenderPhaseCount();
	for (int x=0; x<count; x++)
	{
		RENDERPHASEMGR.GetRenderPhase(x).GetEntityVisibilityMask().ClearFlag( flag );
	}
}

//---------------------------------------------------------------------------------------
inline void CViewportManager::ToggleVisibilityFlagForAllPhases(const u16 flag)
{
	int count = RENDERPHASEMGR.GetRenderPhaseCount();
	for (int x=0; x<count; x++)
	{
		RENDERPHASEMGR.GetRenderPhase(x).GetEntityVisibilityMask().ToggleFlag( flag );
	}
}

//---------------------------------------------------------------------------------------
// Get the current vieport in 'context' from the viewport stack.
inline CViewport*  CViewportManager::GetCurrentViewport() const
{
	Assert(m_stackSize>0);
#if __DEV
	if (m_stackSize<=0) 
		return NULL;
#endif
	return m_stackViewports[m_stackSize-1];
}

//-------------------------------------------------------------------------------
inline CViewport*	CViewportManager::Add(CViewport *pViewport)			
{ 
	m_viewports.PushAndGrow(pViewport); 
	RecacheViewports(); 
	return pViewport;	
}

//-------------------------------------------------------------------------------
inline CViewport*	CViewportManager::Set(s32 i, CViewport *pViewport)	
{ 
	m_viewports[i] = pViewport; 	
	RecacheViewports(); 
	return pViewport;				
}

//-------------------------------------------------------------------------------
inline void		CViewportManager::Delete(s32 idx)					
{ 
	delete m_viewports[idx]; 
	m_viewports.Delete(idx);	
	RecacheViewports();				
}

//-------------------------------------------------------------------------------
inline CViewport*	CViewportManager::FindById(s32 id) const					
{ 
	Assertf(id>0,"you are trying to access a NULL viewport!"); 
	for (s32 i=0;i<m_viewports.GetCount();i++) 
	{
		if (m_viewports[i]->GetId()==id) 
			return m_viewports[i]; 
	} 
	return NULL; 
}

//-------------------------------------------------------------------------------
inline CViewport*	CViewportManager::FindByIdNoAssert(s32 id) const			
{ 
	for (s32 i=0;i<m_viewports.GetCount();i++) 
	{
		if (m_viewports[i]->GetId()==id) 
			return m_viewports[i];
	} 
	return NULL; 
}

//-------------------------------------------------------------------------------
inline void		CViewportManager::DeleteById(s32 id)				
{ 
	Assertf(id>0,"you are trying to delete a NULL viewport!"); 
	for (s32 i=0;i<m_viewports.GetCount();i++) 
	{
		if (m_viewports[i]->GetId()==id) 
		{
			Delete(i); 
			return;
		}
	} 
}

//-------------------------------------------------------------------------------
inline void	CViewportManager::CacheGameViewport()					
{ 
	m_pGameViewport = NULL;			
	for (s32 i=0;i<m_viewports.GetCount();i++)
	{
		if (m_viewports[i]->IsGame())	
		{
			m_pGameViewport				= m_viewports[i];
			return;
		}
	} 
}

//-------------------------------------------------------------------------------
inline void	CViewportManager::CachePrimaryOrthoViewport()			
{ 
	m_pPrimaryOrthoViewport = NULL;	
	for (s32 i=0;i<m_viewports.GetCount();i++) 
	{
		if (m_viewports[i]->IsPrimaryOrtho())	
		{
			m_pPrimaryOrthoViewport		= m_viewports[i];	
			return;
		}
	} 
}

//-------------------------------------------------------------------------------
inline void	CViewportManager::CachePlayerSettingsViewport()		
{ 
	m_pPlayerSettingsViewport	= NULL;	
	for (s32 i=0;i<m_viewports.GetCount();i++) 
	{
		if (m_viewports[i]->IsPlayerSettings())		
		{
			m_pPlayerSettingsViewport	= m_viewports[i];	
			return;
		}		
	} 
}

//-------------------------------------------------------------------------------
inline void	CViewportManager::CacheGameViewportIndex()			
{ 
	m_gameViewportIndex = -1;			
	for (s32 i=0;i<m_viewports.GetCount();i++) 
	{
		if (m_viewports[i]->IsGame())					
		{
			m_gameViewportIndex			= i;	
			return;
		}
	} 
}

//-------------------------------------------------------------------------------
inline void CViewportManager::CacheGameGrcViewport(const grcViewport& viewport)
{
	m_cachedGameGrcViewport[gRenderThreadInterface.GetUpdateBuffer()] = viewport;
}

//-------------------------------------------------------------------------------
inline void CViewportManager::CacheGameViewportFrame(const camFrame& frame)
{
	m_cachedGameViewportFrame[gRenderThreadInterface.GetUpdateBuffer()].CloneFrom(frame);
}

extern CViewportManager gVpMan;

// wrapper class for interfacing with the game skeleton
class CViewportManagerWrapper
{
public:

    static void Process();
    static void	CreateFinalScreenRenderPhaseList();
};

extern CRenderPhaseScanned *g_SceneToGBufferPhaseNew;
extern CRenderPhaseEntitySelect *g_RenderPhaseEntitySelectNew;
extern CRenderPhaseCascadeShadows *g_CascadeShadowsNew;
extern CRenderPhaseHeight *g_RenderPhaseHeightNew;
extern CRenderPhaseDeferredLighting_LightsToScreen *g_DefLighting_LightsToScreen;
extern CRenderPhaseScanned *g_ReflectionMapPhase;
extern CRenderPhaseMirrorReflection *g_MirrorReflectionPhase;
extern RenderPhaseSeeThrough *g_SeeThroughPhase;

#endif // !INC_VIEWPORTMGR_H_
