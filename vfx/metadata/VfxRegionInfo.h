///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxRegionInfo.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_REGIONINFO_H
#define	VFX_REGIONINFO_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "script/thread.h"

// game


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define VFXREGIONINFO_DEFAULT_HASHNAME	atHashWithStringNotFinal("VFXREGIONINFO_DEFAULT",0x1F68D415)

#if __BANK
#define MAX_DEBUG_REGION_NAMES		(32)
#endif


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxRegionGpuPtFxInfo
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		CVfxRegionGpuPtFxInfo() {m_gpuPtFxCurrLevel=0.0f; m_gpuPtFxTargetLevel=0.0f;}

		bool Update(bool isCurrRegion);

		bool GetGpuPtFxEnabled() const {return m_gpuPtFxEnabled;}
		bool GetGpuPtFxActive() const;
		atHashWithStringNotFinal GetGpuPtFxName() const {return m_gpuPtFxName;}
		float GetGpuPtFxCurrLevel() const {return m_gpuPtFxCurrLevel;}
		float GetGpuPtFxMaxLevel() const;

	private: //////////////////////////

		// parser
		PAR_SIMPLE_PARSABLE;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	//private: //////////////////////////
	public: ///////////////////////////

		bool m_gpuPtFxEnabled;
		atHashWithStringNotFinal m_gpuPtFxName;
		float m_gpuPtFxSunThresh;
		float m_gpuPtFxWindThresh;
		float m_gpuPtFxTempThresh;
		s32 m_gpuPtFxTimeMin;
		s32 m_gpuPtFxTimeMax;
		float m_gpuPtFxInterpRate;
		float m_gpuPtFxMaxLevelNoWind;
		float m_gpuPtFxMaxLevelFullWind;

		// internal data
		float m_gpuPtFxCurrLevel;
		float m_gpuPtFxTargetLevel;

};

class CVfxRegionInfo
{	
	friend class CVfxRegionInfoMgr;

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		CVfxRegionInfo() {m_pActiveVfxRegionGpuPtFxDropInfo = NULL; m_pActiveVfxRegionGpuPtFxMistInfo = NULL; m_pActiveVfxRegionGpuPtFxGroundInfo = NULL;}

		// wind debris vfx
		bool GetWindDebrisPtFxEnabled() const {return m_windDebrisPtFxEnabled;}
		atHashWithStringNotFinal GetWindDebrisPtFxName() const {return m_windDebrisPtFxName;}

		// gpu particles
		void ResetGpuPtFxInfos();
		bool UpdateGpuPtFxInfos(bool isCurrRegion);
		CVfxRegionGpuPtFxInfo* GetActiveVfxRegionGpuPtFxDropInfo() {return m_pActiveVfxRegionGpuPtFxDropInfo;}
		CVfxRegionGpuPtFxInfo* GetActiveVfxRegionGpuPtFxMistInfo() {return m_pActiveVfxRegionGpuPtFxMistInfo;}
		CVfxRegionGpuPtFxInfo* GetActiveVfxRegionGpuPtFxGroundInfo() {return m_pActiveVfxRegionGpuPtFxGroundInfo;}
#if __ASSERT
		void VerifyGpuPtFxInfos(bool isActiveRegion);
#endif


	private: //////////////////////////

		// parser
		PAR_SIMPLE_PARSABLE;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// windDebris vfx
		bool m_windDebrisPtFxEnabled;
		atHashWithStringNotFinal m_windDebrisPtFxName;

		// gpu particles
		atArray<CVfxRegionGpuPtFxInfo> m_vfxRegionGpuPtFxDropInfos;
		atArray<CVfxRegionGpuPtFxInfo> m_vfxRegionGpuPtFxMistInfos;
		atArray<CVfxRegionGpuPtFxInfo> m_vfxRegionGpuPtFxGroundInfos;

		CVfxRegionGpuPtFxInfo* m_pActiveVfxRegionGpuPtFxDropInfo;
		CVfxRegionGpuPtFxInfo* m_pActiveVfxRegionGpuPtFxMistInfo;
		CVfxRegionGpuPtFxInfo* m_pActiveVfxRegionGpuPtFxGroundInfo;

};

class CVfxRegionInfoMgr
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void LoadData();
		void RemoveScript(scrThreadId scriptThreadId);

		CVfxRegionInfo* GetInfo(atHashWithStringNotFinal hashName);
		void UpdateGpuPtFx(bool isActive, s32 forceRegionIdx);

		s32 GetNumRegionInfos() const {return m_vfxRegionInfos.GetCount();}

		void SetDisableRegionVfx(bool val, scrThreadId scriptThreadId);
		CVfxRegionInfo* GetActiveVfxRegionInfo() {return m_pActiveVfxRegionInfo;}

#if __BANK
		const char* GetRegionInfoName(u32 idx) {return m_vfxRegionNames[idx];}
#endif

	
	private: //////////////////////////

		// parser
		PAR_SIMPLE_PARSABLE;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		atBinaryMap<CVfxRegionInfo*, atHashWithStringNotFinal> m_vfxRegionInfos;

		// internal data
		scrThreadId m_disableRegionVfxScriptThreadId;
		bool m_disableRegionVfx;

		CVfxRegionInfo* m_pActiveVfxRegionInfo;

#if __BANK
		const char* m_vfxRegionNames[MAX_DEBUG_REGION_NAMES];
#endif

};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxRegionInfoMgr g_vfxRegionInfoMgr;


#endif // VFX_REGIONINFO_H
