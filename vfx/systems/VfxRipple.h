/*
///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxRipple.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	03 Jul 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_RIPPLE_H
#define	VFX_RIPPLE_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "grcore/texture.h"
#include "grcore/stateblock.h"
#include "vectormath/classes.h"

// framework
#include "vfx/vfxlist.h"

// game


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class grmShader;
}


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////	

#define VFXRIPPLE_MAX_POINTS			(256)
#define	VFXRIPPLE_MAX_REGISTERED		(32)
#define VFXRIPPLE_NUM_TEXTURES			(4)



///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum VfxRippleTypes_e
{
	VFXRIPPLE_TYPE_PED				= 0,
	VFXRIPPLE_TYPE_BOAT_WAKE,
	VFXRIPPLE_TYPE_HELI,

	VFXRIPPLE_NUM_TYPES
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct RegRippleInfo_s
{
	Vec3V		vPos;
	Vec3V		vDir;
	s32			rippleType;
	float		lifeScale;
	float		ratio;
	bool		flipTex;
};



///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CRipplePoint  /////////////////////////////////////////////////////////////
//
//
//
///////////////////////////////////////////////////////////////////////////////

class CRipplePoint : public vfxListItem
{
	///////////////////////////////////
	// FRIENDS
	///////////////////////////////////

		friend class		CRippleManager;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		u32				m_pad[2];
		Vec3V			m_vPos;
		Vec2V			m_vDir;
		float			m_speed;
		float			m_currLife;
		float			m_maxLife;
		float			m_maxAlpha;
		float			m_peakAlpha;
		float			m_startSize;
		float			m_theta;
		float			m_thetaInit;
		bool			m_flipTex;


}; // CRipplePoint 


//  CRippleManager  ///////////////////////////////////////////////////////////
//
//
//
///////////////////////////////////////////////////////////////////////////////

class CRippleManager
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		void				Update						(float deltaTime);
		void				Render						();

		void				Register					(s32 rippleType, Vec3V_In vPos, Vec3V_In vDir, float lifeScale, float ratio, bool flipTexture);

		// debug functions
#if __BANK
		void				InitWidgets			();
#endif

	private: //////////////////////////

		// shader helpers
		void				InitShader					();
		void				ShutdownShader				();
		void				SetShaderVarsConst			();
		void				StartShader					(s32 i, s32 j);
		void				EndShader					();


		// init helper functions
		void				LoadTextures				(s32 txdSlot);

		// pool access
		CRipplePoint*		GetRipplePointFromPool		();
		void				ReturnRipplePointToPool		(CRipplePoint* pRipplePoint);

		// debug functions
#if __BANK
#if __DEV
		void				InitSettingWidgets			(bkBank* pVfxBank, s32 rippleType);
#endif
#endif

		static void			InitRenderStateBlocks		();

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		grmShader*			m_pShader;	
		grcEffectVar		m_shaderVarIdDecalTexture;
		grcEffectVar		m_shaderVarIdDepthTexture;
		grcEffectVar		m_shaderVarIdOOScreenSize;
		grcEffectVar		m_shaderVarIdProjParams;

		CRipplePoint		m_ripplePoints				[VFXRIPPLE_MAX_POINTS];
		vfxList				m_ripplePointPool;

		vfxList				m_ripplePointList			[VFXRIPPLE_NUM_TYPES][VFXRIPPLE_NUM_TEXTURES];

		s32				m_numRegRipples;
		RegRippleInfo_s		m_pRegRipples				[VFXRIPPLE_MAX_REGISTERED];

		grcTexture*			m_pTexWaterRipplesDiffuse	[VFXRIPPLE_NUM_TYPES][VFXRIPPLE_NUM_TEXTURES];

#if __BANK
		s32				m_numActiveRipples;
		bool				m_disableRipples;
		bool				m_debugRipplePoints;
#endif

		static grcBlendStateHandle			ms_exitBlendState;
		static grcDepthStencilStateHandle	ms_exitDepthStencilState;
		static grcRasterizerStateHandle		ms_exitRasterizerState;

		static grcBlendStateHandle			ms_renderBlendState;
		static grcDepthStencilStateHandle	ms_renderDepthStencilState;
		static grcDepthStencilStateHandle	ms_renderStencilOffDepthStencilState;
		static grcRasterizerStateHandle		ms_renderRasterizerState;

}; // CRippleManager 


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern dev_float VFXRIPPLE_SPEED_THRESH_A			[VFXRIPPLE_NUM_TYPES];
extern dev_float VFXRIPPLE_SPEED_THRESH_B			[VFXRIPPLE_NUM_TYPES];
extern dev_s32 VFXRIPPLE_FREQ_A					[VFXRIPPLE_NUM_TYPES];
extern dev_s32 VFXRIPPLE_FREQ_B					[VFXRIPPLE_NUM_TYPES];


#endif // VFX_RIPPLE_H
*/




