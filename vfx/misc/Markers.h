///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Markers.h
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	01 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef MARKERS_H
#define	MARKERS_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// framework
#include "streaming/streamingmodule.h"

// rage
#include "paging/ref.h"
#include "grcore/effect.h"
#include "grcore/stateblock.h"
#include "grmodel/shadergroupvar.h"

// game
#include "Renderer/Color.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class rmcDrawable;
}


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define MARKERS_MAX						(128)								// network games can have many more, but we may not add them to this list as the radar has distance checks in place
#define MARKERS_MANUAL_DEPTH_TEST		(1)									// linked to the shaders flag MANUAL_DEPTH_TEST in radar.fx


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum MarkerType_e
{
	MARKERTYPE_INVALID					= -1,

	MARKERTYPE_CONE						= 0,
	MARKERTYPE_CYLINDER,
	MARKERTYPE_ARROW,
	MARKERTYPE_ARROW_FLAT,
	MARKERTYPE_FLAG,
	MARKERTYPE_RING_FLAG,
	MARKERTYPE_RING,
	MARKERTYPE_PLANE,
	MARKERTYPE_BIKE_LOGO_1,
	MARKERTYPE_BIKE_LOGO_2,
	MARKERTYPE_NUM_0,
	MARKERTYPE_NUM_1,
	MARKERTYPE_NUM_2,
	MARKERTYPE_NUM_3,
	MARKERTYPE_NUM_4,
	MARKERTYPE_NUM_5,
	MARKERTYPE_NUM_6,
	MARKERTYPE_NUM_7,
	MARKERTYPE_NUM_8,
	MARKERTYPE_NUM_9,
	MARKERTYPE_CHEVRON_1,
	MARKERTYPE_CHEVRON_2,
	MARKERTYPE_CHEVRON_3,
	MARKERTYPE_RING_FLAT,
	MARKERTYPE_LAP,
	MARKERTYPE_HALO,
	MARKERTYPE_HALO_POINT,
	MARKERTYPE_HALO_ROTATE,
	MARKERTYPE_SPHERE,
	MARKERTYPE_MONEY,
	MARKERTYPE_LINES,
	MARKERTYPE_BEAST,
	MARKERTYPE_QUESTION_MARK,
	MARKERTYPE_TRANSFORM_PLANE,
	MARKERTYPE_TRANSFORM_HELICOPTER,
	MARKERTYPE_TRANSFORM_BOAT,
	MARKERTYPE_TRANSFORM_CAR,
	MARKERTYPE_TRANSFORM_BIKE,
	MARKERTYPE_TRANSFORM_PUSH_BIKE,
	MARKERTYPE_TRANSFORM_TRUCK,
	MARKERTYPE_TRANSFORM_PARACHUTE,
	MARKERTYPE_TRANSFORM_THRUSTER,
	MARKERTYPE_WARP,
	MARKERTYPE_BOXES,
	MARKERTYPE_PIT_LANE,

	MARKERTYPE_MAX
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

struct MarkerInfo_t
{
	MarkerInfo_t()
	{
		vPos = Vec3V(V_ZERO);
		vDir = Vec3V(V_ZERO);;
		vRot = Vec4V(V_ZERO);;
		vScale = Vec3V(V_ZERO);;
		vClippingPlane = Vec4V(V_ZERO);;

		type = MARKERTYPE_INVALID;

		col = Color32(0xffffffff);

		txdIdx = -1;
		texIdx = -1;
		
		// flags
		bounce = false;
		rotate = false;
		faceCam = false;
		clip = false;
		invert = false;
		usePreAlphaDepth = true;
		matchEntityRotOrder = false;
	}
	Vec3V 					vPos;													// the position of the marker
	Vec3V 					vDir;													// the direction of the marker (only used if faceDir is true)
	Vec4V					vRot;													// additional marker rotations in each axis (applied at the end)
	Vec3V					vScale;													// the scale of the marker
	Vec4V					vClippingPlane;											// clipping plane for marker

	MarkerType_e			type;													// the marker type

	Color32					col;													// the colour of the marker

	s32						txdIdx;													// texture dictionary slot if the texture was override, -1 otherwise
	s32						texIdx;													// texture slot if the texture was ovrriden, -1 otherwise
	
	// flags
	bool					bounce			: 1;									// whether or not this marker bounces
	bool					rotate			: 1;									// whether or not this marker rotates on his Z axis
	bool					faceCam			: 1;									// whether or not this marker faces the camera
	bool					clip			: 1;									// whether or not to use the clippling plane
	bool					invert			: 1;									// whether or not to use an inverted depth test.
	bool					usePreAlphaDepth: 1;									// whether or not to use the pre alph depth buffer.
	bool					matchEntityRotOrder : 1;								// whether the rotation order matches that used by entity rotation queries (for lining up markers with entities)

};


struct Marker_t
{
	MarkerInfo_t			info;													// the marker type

	bool					isActive;												// whether or not this marker is active
	Mat34V					renderMtx;												// the final render matrix of this marker
	
	float					distSqrToCam;											// the square distance to the camera

};


struct MarkerType_t
{
	strStreamingObjectName	modelName;

//	atHashWithStringNotFinal ptFxHashName;

	float					bbMinZ;													// the min z of the bounding box

	pgRef<rmcDrawable>		pDrawable;
	grmShaderGroupVar		diffuseColorVar;
	grmShaderGroupVar		clippingPlaneVar;
	grmShaderGroupVar		targetSizeVar;

#if MARKERS_MANUAL_DEPTH_TEST
	grmShaderGroupVar		distortionParamsVar;
	grmShaderGroupVar		manualDepthTestVar;
	grcEffectVar			depthTextureVar;
#endif

	grcEffectVar			diffuseTextureVar;
	grcTextureIndex			diffuseTexture;
	
#if __BANK
	char					typeName		[32];
#endif

};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CMarkers
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void 				Init						(unsigned initMode);
		void 				InitSessionSync				();
		void				Shutdown					(unsigned shutdownMode);

		void 				Update						();
		void 				Render						();

		s32					Register					(MarkerInfo_t& markerInfo);

		void  				DeleteAll					();

		float				GetBBMinZ					(MarkerType_e markerType) {return m_markerTypes[markerType].bbMinZ;}

		bool 				GetClosestMarkerMtx			(MarkerType_e markerType, Vec3V_In vPos, Mat34V_InOut vMtx);

#if __BANK
		void				InitWidgets					();
		void 				RenderDebug					();

static	bool				g_bDisplayMarkers;

#endif  // #if __BANK

	private: //////////////////////////

		void  				Delete						(s32 id);

		s32 				FindFirstFreeSlot			();

static	int					RenderSorter				(const void* pA, const void* pB);
static 	void				RenderInit					();
static 	void				RenderMarker				(MarkerType_e type, Vec4V_In vColour, Vec4V_In vClippingPlane, Mat34V_In renderMtx, s32 txdIdx, s32 texIdx, bool invert, Vec3V_In vScale, bool usePreAlphaDepth);
static 	void				RenderEnd					();

		void				CalcMtx						(Mat34V_InOut mtx, Vec3V_In vPos, Vec3V_In vDir, bool faceCam, bool rotate);

#if __BANK
static	void				Reload						();
static	void				DebugAdd					();
static	void				DebugRemove					();
#endif

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

#if __BANK
		Vec3V				m_vDebugMarkerPos;					// keep this first in the class to avoid alignment issues

		s32					m_numActiveMarkers;

		bool				m_disableRender;
		bool				m_pointAtPlayer;
		bool				m_disableClippingPlane;
		bool				m_renderBoxOnPickerEntity;
		bool				m_forceEntityRotOrder;

		bool				m_debugRender;
		s32					m_debugRenderId;

		bool				m_debugMarkerActive;
		MarkerType_e		m_debugMarkerType;
		float				m_debugMarkerScaleX;
		float				m_debugMarkerScaleY;
		float				m_debugMarkerScaleZ;
		u32					m_debugMarkerColR;
		u32					m_debugMarkerColG;
		u32					m_debugMarkerColB;
		u32					m_debugMarkerColA;
		bool				m_debugMarkerBounce;
		bool				m_debugMarkerRotate;
		bool				m_debugMarkerFaceCam;
		bool				m_debugMarkerInvert;
		bool				m_debugMarkerFacePlayer;
		bool				m_debugMarkerTrackPlayer;
		bool				m_debugOverrideTexture;
		bool				m_debugMarkerClip;
		float				m_debugMarkerDirX;
		float				m_debugMarkerDirY;
		float				m_debugMarkerDirZ;
		float				m_debugMarkerRotX;
		float				m_debugMarkerRotY;
		float				m_debugMarkerRotZ;
		float				m_debugMarkerRotCamTilt;
		float				m_debugMarkerPlaneNormalX;
		float				m_debugMarkerPlaneNormalY;
		float				m_debugMarkerPlaneNormalZ;
		float				m_debugMarkerPlanePosX;
		float				m_debugMarkerPlanePosY;
		float				m_debugMarkerPlanePosZ;
#endif

		Marker_t			m_markers					[MARKERS_MAX];
		MarkerType_t		m_markerTypes				[MARKERTYPE_MAX];

		float				m_currBounceHeight;
		float				m_currBounce;

		float				m_currRotate;
		
		// common state blocks
		static grcRasterizerStateHandle		ms_rasterizerStateHandle;
		static grcRasterizerStateHandle		ms_invertRasterizerStateHandle;
		static grcDepthStencilStateHandle	ms_depthStencilStateHandle;
		static grcDepthStencilStateHandle	ms_invertDepthStencilStateHandle;
		static grcDepthStencilStateHandle	ms_invertP2DepthStencilStateHandle;
		static grcBlendStateHandle			ms_blendStateHandle;
		static grcBlendStateHandle			ms_invertBlendStateHandle;

		static grcDepthStencilStateHandle	ms_exitDepthStencilStateHandle;
		static grcBlendStateHandle			ms_exitBlendStateHandle;

}; 



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CMarkers		g_markers;


#endif // MARKERS_H


