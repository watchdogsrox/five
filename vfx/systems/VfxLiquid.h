///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxLiquid.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 August 2011
//	WHAT:	Liquid effects 
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_LIQUID_H
#define	VFX_LIQUID_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "atl/hashstring.h"

// game
#include "Physics/GtaMaterial.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXLIQUID_MAX_INFOS				(8)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// the different liquid types
enum VfxLiquidType_e
{
	VFXLIQUID_TYPE_NONE					= -1,

	VFXLIQUID_TYPE_WATER				= 0,
	VFXLIQUID_TYPE_BLOOD,
	VFXLIQUID_TYPE_OIL,
	VFXLIQUID_TYPE_PETROL,
	VFXLIQUID_TYPE_MUD,

	VFXLIQUID_TYPE_NUM
};

// the different types of things that liquid can attach to
enum VfxLiquidAttachType_e
{
	VFXMTL_WET_MODE_WHEEL				= 0,
	VFXMTL_WET_MODE_FOOT,

	VFXMTL_WET_MODE_NUM
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct VfxLiquidInfo_s
{
	VfxGroup_e	vfxGroup;
	float		durationFoot;
	float		durationWheel;
	float		decalLifeFoot;
	float		decalLifeWheel;

	s32			normDecalPoolRenderSettingIndex;
	s32			normDecalPoolRenderSettingCount;
	s32			normDecalTrailInRenderSettingIndex;
	s32			normDecalTrailInRenderSettingCount;
	s32			normDecalTrailMidRenderSettingIndex;
	s32			normDecalTrailMidRenderSettingCount;
	s32			normDecalTrailOutRenderSettingIndex;
	s32			normDecalTrailOutRenderSettingCount;

	s32			soakDecalPoolRenderSettingIndex;
	s32			soakDecalPoolRenderSettingCount;
	s32			soakDecalTrailInRenderSettingIndex;
	s32			soakDecalTrailInRenderSettingCount;
	s32			soakDecalTrailMidRenderSettingIndex;
	s32			soakDecalTrailMidRenderSettingCount;
	s32			soakDecalTrailOutRenderSettingIndex;
	s32			soakDecalTrailOutRenderSettingCount;

	float		inOutDecalSize;

	u8			decalColR;
	u8			decalColG;
	u8			decalColB;
	u8			decalColA;

	atHashWithStringNotFinal	ptFxSplashHashName;
};



///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxLiquid
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		VfxLiquidInfo_s&	GetLiquidInfo				(VfxLiquidType_e liquidType)			{return m_vfxLiquidInfo[liquidType];}

		// debug functions
#if __BANK
 		void				InitWidgets					();
#endif

	private: //////////////////////////

		// init helper functions
		void				LoadData					();

		// debug functions
#if __BANK
static	void				Reset						();
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// file version
		float				m_version;

		// liquid data
		s32					m_numVfxLiquidInfos;
		VfxLiquidInfo_s		m_vfxLiquidInfo				[VFXLIQUID_MAX_INFOS];

		// debug variables
#if __BANK
		bool				m_bankInitialised;
#endif

}; // CVfxLiquid



class CVfxLiquidAttachInfo
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void Init();
		void Update(float dt);
		
		void Set(VfxLiquidAttachType_e liquidAttachType, VfxLiquidType_e liquidType);

		VfxLiquidType_e GetLiquidType() const {return m_liquidType;}
		float GetTimer() const {return m_timer;}
		float GetCurrAlpha() const;
		float GetDecalLife() const;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		float					m_timer;
		VfxLiquidAttachType_e	m_liquidAttachType : 16;
		VfxLiquidType_e			m_liquidType : 16;


};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxLiquid			g_vfxLiquid;



#endif // VFX_LIQUID_H

