//
// name:		viewports.h
// description:	Class controlling the game viewports
//
#ifndef INC_VIEWPORTS_H_
#define INC_VIEWPORTS_H_

#include "grcore/viewport.h"

#include "camera/helpers/Frame.h"

#define SCREEN_WIDTH	(GRCDEVICE.GetWidth())
#define SCREEN_HEIGHT	(GRCDEVICE.GetHeight())

#define VIEWPORT_DEFAULT_FOVY		52.5f
#define VIEWPORT_DEFAULT_NEAR		0.5f
#define VIEWPORT_DEFAULT_FAR		800.0f  

enum eViewportOnScreenCode
{
	VIEWPORT_ONSCREEN_CODE_BAD				= -1,
	VIEWPORT_ONSCREEN_CODE_WHOLLY_ONSCREEN,
	VIEWPORT_ONSCREEN_CODE_WHOLLY_OFFSCREEN,
	VIEWPORT_ONSCREEN_CODE_PARTLY_ONSCREEN,
	VIEWPORT_ONSCREEN_CODE_PARTLY_OFFSCREEN	= VIEWPORT_ONSCREEN_CODE_PARTLY_ONSCREEN // just to be handy
};

class CEntity;
class CInteriorProxy;

//-------------------------------------------------------------------------------
// Generically interpolates 2D coordinates.
// OK, I've called it a LERP ( linear interpolation )
// in actual fact it can be non linear too... sorry about that naming misnomer
class CLerp2d
{
public:
	enum eStyles
	{
		BAD = -1,
		SMOOTHED,
		LINEAR,
		MAX_STYLES
	};

	CLerp2d()	{ 	Init();		}
	~CLerp2d()	{ 	Shutdown();	}

	void		Init();
	void		Shutdown()	{}
	bool		IsActive() { return m_bIsActive; };
	void 		Get(Vector2& res);
	void		Set(const Vector2& start, const Vector2& end, s32 duration, eStyles type = SMOOTHED);

private:
	Vector2		m_from;					// the 2d location to lerp from.
	Vector2		m_to;					// the 2d location to lerp to.
	s32			m_timeStart;			// cached time that it started
	s32			m_duration;				// duration (ms)
	s32			m_style;				// a speed style... linear, smoothed etc...
	bool		m_bIsActive;			// active or not?
};

//------------------------------------------------------
// A viewport, contains the rage viewport and other info
class CViewport
{
public:
	// Priorities of render order of viewports
	enum eViewportPriority
	{ // intermediate priorities can be computed and used.
			VIEWPORT_PRIORITY_1ST	=	10000, // never insert before this... THIS IS HIGHEST PRIORITY, WILL BE RENDERED FIRST
			VIEWPORT_PRIORITY_2ND	=	 9000, 
			VIEWPORT_PRIORITY_3RD	=	 8000, 
			VIEWPORT_PRIORITY_4TH	=	 7000, 
			VIEWPORT_PRIORITY_5TH	=	 6000, 
			VIEWPORT_PRIORITY_6TH	=	 5000, 
			VIEWPORT_PRIORITY_7TH	=	 4000, 
			VIEWPORT_PRIORITY_8TH	=	 3000, 
			VIEWPORT_PRIORITY_9TH	=	 2000,
			VIEWPORT_PRIORITY_LAST	=	 1000 // never insert after this... THIS IS LOWEST PRIORITY, WILL BE RENDERED LAST
	};

	CViewport();
	virtual ~CViewport();

	virtual void InitSession();
	virtual void ShutdownSession();

	void Process();

	bool IsActive() const				{	return m_bIsActive;	}
	void SetActive(bool b)				{	m_bIsActive = b;	}

	bool IsOffScreen()					{ return false; } // TODO(DW) //return m_grcViewport.GetWindow().GetNormHeight()
	float GetArea()						{ return  m_grcViewport.GetWindow().GetNormHeight() * m_grcViewport.GetWindow().GetNormWidth(); }

	float GetLastWidth()				{ return m_grcViewport.GetWidth() / m_grcViewport.GetWindow().GetNormWidth(); }
	float GetLastHeight()				{ return m_grcViewport.GetHeight() / m_grcViewport.GetWindow().GetNormHeight(); }

	s32 GetPriority()					{	return m_priority;	}
	void  SetPriority(s32 p)			{	m_priority = p;		}

	void ResetWindow();
	virtual void UpdateAspectRatio();
	// Fade interface to a viewport

	// Lerp (window movement) support
	CLerp2d*	GetLerp2dTopLeft()	{ return &m_lerp[0]; }
	CLerp2d*	GetLerp2dBotRight()	{ return &m_lerp[1]; }
	void		LerpTo(Vector2 *pTopLeft, Vector2 *pBotRight, s32 time, CLerp2d::eStyles moveStyle = CLerp2d::SMOOTHED);
	
	const camFrame&	GetFrame() const	{ return m_Frame; }

	void SetActiveLastFrame(bool bSet)	{ m_bWasActiveLastFrame = bSet; }
	bool WasActiveLastFrame()			{ return m_bWasActiveLastFrame; }

	eViewportOnScreenCode GetOnScreenStatus();		// Is the viewport on / part on / off the screen	

	void SetUnitOrtho() { m_grcViewport.Ortho(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f); }
//	void SetUnitOrtho() { m_grcViewport.Screen(); }

	virtual bool IsRenderable()					{ return true; }
	virtual bool IsUsedForNetworking()			{ return false;}

	// For review once camera settles down...
	virtual bool IsGame() const					{ return false; }
	virtual bool IsPrimaryOrtho() const			{ return false; }
	virtual bool IsRadar()	 const				{ return false; }
	virtual bool IsWeaponAiming()	 const		{ return false; }
	virtual bool IsScripted() const				{ return false; }
	virtual bool IsPlayerSettings() const		{ return false; }

	const grcViewport& GetGrcViewport() const	{ return m_grcViewport; }
	grcViewport& GetGrcViewport()			{ return m_grcViewport; }
	grcViewport& GetNonConstGrcViewport()	{ Assert( ((sysIpcCurrentThreadId)sysIpcGetCurrentThreadId()) == m_threadId); return m_grcViewport; }
	s32 GetId()								{ return m_id; }

	void SetDrawWidescreenBorders(bool b)			{ m_bDrawWidescreenBorders = b;    }
	bool IsDrawingWidescreenBorders()				{ return m_bDrawWidescreenBorders; }

	void SetInteriorData(CInteriorProxy* pIntProxy, u32 roomIdx) {m_pInteriorProxy = pIntProxy; m_roomIdx = roomIdx;}
	CInteriorProxy* GetInteriorProxy() {return m_pInteriorProxy;}
	u32 GetRoomIdx() {return m_roomIdx;}
	
	const CEntity*	GetAttachEntity() const						{ return m_pAttachEntity; }
	void			SetAttachEntity(const CEntity *pEntity)		{ m_pAttachEntity = pEntity; }

	void PropagateCameraFrame(const camFrame& frame);
	void PropagateRevisedCameraFarClip(float farClip);

	static void		DrawBoxOnCurrentViewport(Color32 c);

protected:
	void			ProcessWindow();

	void			ForcePortalTrackerRescan(const Vector3& cameraPosition);

	grcViewport		m_grcViewport;					// A viewport contains a GRCViewport to show how it explicity is presenting our viewport to RAGE. ( it works in a differnt space to our game )
	CLerp2d			m_lerp[2];						// topleft, bottom right
	camFrame		m_Frame;						// Our copy of the frame of the camera that is used for this viewport.
	const CEntity	*m_pAttachEntity;				// A possibly NULL attach Entity setup here to assist the portal tracker ( it should NOT need to be regrefed )
	s32				m_id;							// A unique Id for this viewport
	s32				m_priority;						// the higher the priority the sooner it is rendered
	u32				m_roomIdx;						// room idx of the interior, if the camera is within a room
	CInteriorProxy*	m_pInteriorProxy;				// ptr to interior if the camera of this viewport is within a room
	bool			m_bIsActive					: 1;// if true then it will be rendered at Game::Render, even if non-active it can still be used manually.
	bool			m_bWasActiveLastFrame		: 1;// Was this viewport active last frame... if not and is active now then the viewport knows to (at the right time) create renderphases...
	bool			m_bDrawWidescreenBorders	: 1;// Do you want to draw black borders on TOP of the viewport as it is presented?

	u32				m_LastDeviceTimestamp;			// A time stamp of the last synchnonization point with the display device

	// store thread viewport was created in
#if __DEV
	sysIpcCurrentThreadId	m_threadId;
#endif

	static s32	sm_IdViewports;					// The id which gets used when a viewport is created ( it just keeps incrementing )
};



//------------------------------------------------------
class CViewportGame : public CViewport
{
public:
	CViewportGame()	: CViewport()	{ m_lastZoneUpdatePos.Zero(); }
	~CViewportGame()				{}
	virtual void InitSession();
	virtual bool IsUsedForNetworking()			{ return true;	}
	Vector3& GetLastZoneUpdatePos() { return m_lastZoneUpdatePos; }

	virtual bool IsGame() const	{ return true; }
	
private:
	Vector3		m_lastZoneUpdatePos;
};

//------------------------------------------------------
class CViewportPrimaryOrtho : public CViewport
{
private:
	bool gbFirstTimeOnlyFade;
public:
	CViewportPrimaryOrtho()				{ SetUnitOrtho(); gbFirstTimeOnlyFade = true; }
	~CViewportPrimaryOrtho()			{}
	virtual void InitSession();
	virtual void ShutdownSession();
	virtual bool IsRenderable()			{ return false; }

	virtual bool IsPrimaryOrtho() const	{ return true; }

	void SetFadeFirstTime()				{ gbFirstTimeOnlyFade = true; }
};

//----------------------------------------------------------
// A Viewport for rendering a 3D scene in the frontend.
class CViewportFrontend3DScene : public CViewport
{
public:
	CViewportFrontend3DScene()	{}
	~CViewportFrontend3DScene() {}

	virtual void InitSession();

	virtual bool IsPlayerSettings() const { return true; }
	virtual bool IsUsedForNetworking() { return true;	}
};


#endif // !INC_VIEWPORTS_H_
