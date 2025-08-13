///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxPedInfo.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_PEDINFO_H
#define	VFX_PEDINFO_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "fwanimation/boneids.h"
#include "parser/macros.h"

// game
#include "Physics/GtaMaterial.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

class CPed;
class CPedModelInfo;
class CVfxPedInfo;

namespace rage
{
	class parTreeNode;
}


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// limb ids for the second surface effects (must match enum in VfxPedInfo.psc)
enum VfxPedLimbId_e
{
	VFXPEDLIMBID_LEG_LEFT						= 0,
	VFXPEDLIMBID_LEG_RIGHT,
	VFXPEDLIMBID_ARM_LEFT,
	VFXPEDLIMBID_ARM_RIGHT,
	VFXPEDLIMBID_SPINE,

	VFXPEDLIMBID_NUM
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

struct VfxPedFootDecalInfo
{
	s32 m_decalIdBare;
	s32 m_decalIdShoe;
	s32 m_decalIdBoot;
	s32 m_decalIdHeel;
	s32 m_decalIdFlipFlop;
	s32 m_decalIdFlipper;
	s32 m_decalRenderSettingIndexBare;
	s32 m_decalRenderSettingCountBare;
	s32 m_decalRenderSettingIndexShoe;
	s32 m_decalRenderSettingCountShoe;
	s32 m_decalRenderSettingIndexBoot;
	s32 m_decalRenderSettingCountBoot;
	s32 m_decalRenderSettingIndexHeel;
	s32 m_decalRenderSettingCountHeel;
	s32 m_decalRenderSettingIndexFlipFlop;
	s32 m_decalRenderSettingCountFlipFlop;
	s32 m_decalRenderSettingIndexFlipper;
	s32 m_decalRenderSettingCountFlipper;
	float m_decalWidth;
	float m_decalLength;
	u8 m_decalColR;
	u8 m_decalColG;
	u8 m_decalColB;
	u8 m_decalWetColR;
	u8 m_decalWetColG;
	u8 m_decalWetColB;
	float m_decalLife;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedFootPtFxInfo
{
	atHashWithStringNotFinal m_ptFxDryName;
	atHashWithStringNotFinal m_ptFxWetName;
	float m_ptFxWetThreshold;
	float m_ptFxScale;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedFootInfo
{
	atHashWithStringNotFinal m_decalInfoName;
	atHashWithStringNotFinal m_ptFxInfoName;

	const VfxPedFootDecalInfo* GetDecalInfo() const;
	const VfxPedFootPtFxInfo* GetPtFxInfo() const;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedWadeDecalInfo
{
	s32 m_decalId;
	s32 m_decalRenderSettingIndex;
	s32 m_decalRenderSettingCount;
	float m_decalSizeMin;
	float m_decalSizeMax;
	u8 m_decalColR;
	u8 m_decalColG;
	u8 m_decalColB;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedWadePtFxInfo
{
	atHashWithStringNotFinal m_ptFxName;
	float m_ptFxDepthEvoMin;
	float m_ptFxDepthEvoMax;
	float m_ptFxSpeedEvoMin;
	float m_ptFxSpeedEvoMax;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedWadeInfo
{
	atHashWithStringNotFinal m_decalInfoName;
	atHashWithStringNotFinal m_ptFxInfoName;

	const VfxPedWadeDecalInfo* GetDecalInfo() const;
	const VfxPedWadePtFxInfo* GetPtFxInfo() const;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedVfxGroupInfo
{
	VfxGroup_e m_vfxGroup;

	atHashWithStringNotFinal m_footInfoName;
	atHashWithStringNotFinal m_wadeInfoName;

	const VfxPedFootInfo* GetFootInfo() const;
	const VfxPedWadeInfo* GetWadeInfo() const;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedBoneWadeInfo
{
	float m_sizeEvo;
	float m_depthMult;
	float m_speedMult;
	float m_widthRatio;	

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedBoneWaterInfo
{
	float m_sampleSize;
	float m_boneSize;
	bool m_playerOnlyPtFx;
	bool m_splashInPtFxEnabled;
	bool m_splashOutPtFxEnabled;
	bool m_splashWadePtFxEnabled;
	bool m_splashTrailPtFxEnabled;
	bool m_waterDripPtFxEnabled;
	float m_splashInPtFxRange;
	float m_splashOutPtFxRange;
	float m_splashWadePtFxRange;
	float m_splashTrailPtFxRange;
	float m_waterDripPtFxRange;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedSkeletonBoneInfo
{
	eAnimBoneTag m_boneTagA;
	eAnimBoneTag m_boneTagB;

	VfxPedLimbId_e m_limbId;
	
	atHashWithStringNotFinal m_boneWadeInfoName;
	atHashWithStringNotFinal m_boneWaterInfoName;

	const VfxPedBoneWadeInfo* GetBoneWadeInfo() const;
	const VfxPedBoneWaterInfo* GetBoneWaterInfo() const;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedFireInfo
{
	atHashWithStringNotFinal m_ptFxName;
	eAnimBoneTag m_boneTagA;
	eAnimBoneTag m_boneTagB;
	int	m_limbId;

	PAR_SIMPLE_PARSABLE;
};

struct VfxPedParachutePedTrailInfo
{
	atHashWithStringNotFinal m_ptFxName;
	eAnimBoneTag m_boneTagA;
	eAnimBoneTag m_boneTagB;

	PAR_SIMPLE_PARSABLE;
};

#if __DEV
struct VfxPedLiveEditUpdateData
{
	CVfxPedInfo* pOrig;
	u32 hashName;
	CVfxPedInfo* pNew;
};
#endif


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxPedInfo
{	
	friend class CVfxPedInfoMgr;

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		CVfxPedInfo();

		// vfx groups
		int GetVfxGroupNumInfos() const {return m_vfxGroupInfos.GetCount();}
		const VfxPedVfxGroupInfo* GetVfxGroupInfo(int idx) const {return &m_vfxGroupInfos[idx];}
		const VfxPedVfxGroupInfo* GetVfxGroupInfo(VfxGroup_e vfxGroup) const;

		// skeleton bones
		int GetPedSkeletonBoneNumInfos() const {return m_skeletonBoneInfos.GetCount();}
		const VfxPedSkeletonBoneInfo* GetPedSkeletonBoneInfo(int idx) {return &m_skeletonBoneInfos[idx];}

		// foot vfx
		float GetFootVfxRangeSqr() const {return m_footVfxRange*m_footVfxRange;}
		bool GetFootPtFxEnabled() const {return m_footPtFxEnabled;}
		float GetFootPtFxSpeedEvoMin() const {return m_footPtFxSpeedEvoMin;}
		float GetFootPtFxSpeedEvoMax() const {return m_footPtFxSpeedEvoMax;}
		bool GetFootDecalsEnabled() const {return m_footDecalsEnabled;}
//		const VfxPedFootInfo* GetFootVfxInfo(int idx) const {return &m_footVfxInfos[idx];}
		const VfxPedFootInfo* GetFootVfxInfo(VfxGroup_e vfxGroup) const;

		// wade vfx
		bool GetWadePtFxEnabled() const {return m_wadePtFxEnabled;}
		float GetWadePtFxRangeSqr() const {return m_wadePtFxRange*m_wadePtFxRange;}
		bool GetWadeDecalsEnabled() const {return m_wadeDecalsEnabled;}
		float GetWadeDecalsRangeSqr() const {return m_wadeDecalsRange*m_wadeDecalsRange;}
//		const VfxPedWadeInfo* GetWadeVfxInfo(int idx) const {return &m_wadeVfxInfos[idx];}
		const VfxPedWadeInfo* GetWadeVfxInfo(VfxGroup_e vfxGroup) const;

		// blood vfx
		bool GetBloodPoolVfxEnabled() const {return m_bloodPoolVfxEnabled;}
		float GetBloodPoolVfxSizeMultiplier() const {return m_bloodPoolVfxSizeMult;}
		bool GetBloodEntryPtFxEnabled() const {return m_bloodEntryPtFxEnabled;}
		float GetBloodEntryPtFxRangeSqr() const {return m_bloodEntryPtFxRange*m_bloodEntryPtFxRange;}
		float GetBloodEntryPtFxScale() const {return m_bloodEntryPtFxScale;}
		bool GetBloodEntryDecalEnabled() const {return m_bloodEntryDecalEnabled;}
		float GetBloodEntryDecalRangeSqr() const {return m_bloodEntryDecalRange*m_bloodEntryDecalRange;}
		bool GetBloodExitPtFxEnabled() const {return m_bloodExitPtFxEnabled;}
		float GetBloodExitPtFxRangeSqr() const {return m_bloodExitPtFxRange*m_bloodExitPtFxRange;}
		float GetBloodExitPtFxScale() const {return m_bloodExitPtFxScale;}
		bool GetBloodExitDecalEnabled() const {return m_bloodExitDecalEnabled;}
		float GetBloodExitDecalRangeSqr() const {return m_bloodExitDecalRange*m_bloodExitDecalRange;}
		float GetBloodExitProbeRangeSqr() const {return m_bloodExitProbeRange*m_bloodExitProbeRange;}
		float GetBloodExitPosDistHead() const {return m_bloodExitPosDistHead;}
		float GetBloodExitPosDistTorso() const {return m_bloodExitPosDistTorso;}
		float GetBloodExitPosDistLimb() const {return m_bloodExitPosDistLimb;}
		float GetBloodExitPosDistFoot() const {return m_bloodExitPosDistFoot;}

		// shot vfx
		bool GetShotPtFxEnabled() const {return m_shotPtFxEnabled;}
		float GetShotPtFxRangeSqr() const {return m_shotPtFxRange*m_shotPtFxRange;}
		atHashWithStringNotFinal GetShotPtFxName() const {return m_shotPtFxName;}

		// fire vfx
		bool GetFirePtFxEnabled() const {return m_firePtFxEnabled;}
		int GetFirePtFxNumInfos() const {return m_firePtFxInfos.GetCount();}
		const VfxPedFireInfo& GetFirePtFxInfo(int idx) const {return m_firePtFxInfos[idx];}
		float GetFirePtFxSpeedEvoMin() const {return m_firePtFxSpeedEvoMin;}
		float GetFirePtFxSpeedEvoMax() const {return m_firePtFxSpeedEvoMax;}

		// smoulder vfx
		bool GetSmoulderPtFxEnabled() const {return m_smoulderPtFxEnabled;}
		float GetSmoulderPtFxRangeSqr() const {return m_smoulderPtFxRange*m_smoulderPtFxRange;}
		atHashWithStringNotFinal GetSmoulderPtFxName() const {return m_smoulderPtFxName;}
		eAnimBoneTag GetSmoulderPtFxBoneTagA() const {return m_smoulderPtFxBoneTagA;}
		eAnimBoneTag GetSmoulderPtFxBoneTagB() const {return m_smoulderPtFxBoneTagB;}

		// breath vfx
		bool GetBreathPtFxEnabled() const {return m_breathPtFxEnabled;}
		float GetBreathPtFxRangeSqr() const {return m_breathPtFxRange*m_breathPtFxRange;}
		atHashWithStringNotFinal GetBreathPtFxName() const {return m_breathPtFxName;}
		float GetBreathPtFxSpeedEvoMin() const {return m_breathPtFxSpeedEvoMin;}
		float GetBreathPtFxSpeedEvoMax() const {return m_breathPtFxSpeedEvoMax;}
		float GetBreathPtFxTempEvoMin() const {return m_breathPtFxTempEvoMin;}
		float GetBreathPtFxTempEvoMax() const {return m_breathPtFxTempEvoMax;}
		float GetBreathPtFxRateEvoMin() const {return m_breathPtFxRateEvoMin;}
		float GetBreathPtFxRateEvoMax() const {return m_breathPtFxRateEvoMax;}

		// breath water vfx
		bool GetBreathWaterPtFxEnabled() const {return m_breathWaterPtFxEnabled;}
		float GetBreathWaterPtFxRangeSqr() const {return m_breathWaterPtFxRange*m_breathWaterPtFxRange;}
		atHashWithStringNotFinal GetBreathWaterPtFxName() const {return m_breathWaterPtFxName;}
		float GetBreathWaterPtFxSpeedEvoMin() const {return m_breathWaterPtFxSpeedEvoMin;}
		float GetBreathWaterPtFxSpeedEvoMax() const {return m_breathWaterPtFxSpeedEvoMax;}
		float GetBreathWaterPtFxDepthEvoMin() const {return m_breathWaterPtFxDepthEvoMin;}
		float GetBreathWaterPtFxDepthEvoMax() const {return m_breathWaterPtFxDepthEvoMax;}

		// breath scuba vfx
		bool GetBreathScubaPtFxEnabled() const {return m_breathScubaPtFxEnabled;}
		float GetBreathScubaPtFxRangeSqr() const {return m_breathScubaPtFxRange*m_breathScubaPtFxRange;}
		atHashWithStringNotFinal GetBreathScubaPtFxName() const {return m_breathScubaPtFxName;}
		float GetBreathScubaPtFxSpeedEvoMin() const {return m_breathScubaPtFxSpeedEvoMin;}
		float GetBreathScubaPtFxSpeedEvoMax() const {return m_breathScubaPtFxSpeedEvoMax;}
		float GetBreathScubaPtFxDepthEvoMin() const {return m_breathScubaPtFxDepthEvoMin;}
		float GetBreathScubaPtFxDepthEvoMax() const {return m_breathScubaPtFxDepthEvoMax;}

		// splash vfx
		bool GetSplashVfxEnabled() const {return m_splashVfxEnabled;}
		float GetSplashVfxRangeSqr() const {return m_splashVfxRange*m_splashVfxRange;}

		// splash lod vfx
		bool GetSplashLodPtFxEnabled() const {return m_splashLodPtFxEnabled;}
		float GetSplashLodVfxRangeSqr() const {return m_splashLodVfxRange*m_splashLodVfxRange;}
		atHashWithStringNotFinal GetSplashLodPtFxName() const {return m_splashLodPtFxName;}
		float GetSplashLodPtFxSpeedEvoMin() const {return m_splashLodPtFxSpeedEvoMin;}
		float GetSplashLodPtFxSpeedEvoMax() const {return m_splashLodPtFxSpeedEvoMax;}

		// splash entry vfx
		bool GetSplashEntryPtFxEnabled() const {return m_splashEntryPtFxEnabled;}
		float GetSplashEntryVfxRangeSqr() const {return m_splashEntryVfxRange*m_splashEntryVfxRange;}
		atHashWithStringNotFinal GetSplashEntryPtFxName() const {return m_splashEntryPtFxName;}
		float GetSplashEntryPtFxSpeedEvoMin() const {return m_splashEntryPtFxSpeedEvoMin;}
		float GetSplashEntryPtFxSpeedEvoMax() const {return m_splashEntryPtFxSpeedEvoMax;}

		// splash in vfx
		atHashWithStringNotFinal GetSplashInPtFxName() const {return m_splashInPtFxName;}
		float GetSplashInPtFxSpeedThresh() const {return m_splashInPtFxSpeedThresh;}
		float GetSplashInPtFxSpeedEvoMin() const {return m_splashInPtFxSpeedEvoMin;}
		float GetSplashInPtFxSpeedEvoMax() const {return m_splashInPtFxSpeedEvoMax;}

		// splash out vfx
		atHashWithStringNotFinal GetSplashOutPtFxName() const {return m_splashOutPtFxName;}
		float GetSplashOutPtFxSpeedEvoMin() const {return m_splashOutPtFxSpeedEvoMin;}
		float GetSplashOutPtFxSpeedEvoMax() const {return m_splashOutPtFxSpeedEvoMax;}

		// splash wade vfx
		atHashWithStringNotFinal GetSplashWadePtFxName() const {return m_splashWadePtFxName;}
		float GetSplashWadePtFxSpeedEvoMin() const {return m_splashWadePtFxSpeedEvoMin;}
		float GetSplashWadePtFxSpeedEvoMax() const {return m_splashWadePtFxSpeedEvoMax;}

		// water trail vfx
		atHashWithStringNotFinal GetSplashTrailPtFxName() const {return m_splashTrailPtFxName;}
		float GetSplashTrailPtFxSpeedEvoMin() const {return m_splashTrailPtFxSpeedEvoMin;}
		float GetSplashTrailPtFxSpeedEvoMax() const {return m_splashTrailPtFxSpeedEvoMax;}
		float GetSplashTrailPtFxDepthEvoMin() const {return m_splashTrailPtFxDepthEvoMin;}
		float GetSplashTrailPtFxDepthEvoMax() const {return m_splashTrailPtFxDepthEvoMax;}

		// water drip vfx
		int GetWaterDripPtFxNumInfos() const {return m_waterDripPtFxToSkeltonBoneIds.GetCount();}
		int GetWaterDripPtFxToSkeletonBoneId(int waterDripPtFxIdx) const {return m_waterDripPtFxToSkeltonBoneIds[waterDripPtFxIdx];}
		int GetWaterDripPtFxFromSkeletonBoneId(int skeletonBoneIdx) const {return m_waterDripPtFxFromSkeltonBoneIds[skeletonBoneIdx];}
		atHashWithStringNotFinal GetWaterDripPtFxName() const {return m_waterDripPtFxName;}
		float GetWaterDripPtFxTimer() const {return m_waterDripPtFxTimer;}

		// underwater disturbance vfx
		bool GetUnderwaterDisturbPtFxEnabled() const {return m_underwaterDisturbPtFxEnabled;}
		atHashWithStringNotFinal GetUnderwaterDisturbPtFxNameDefault() const {return m_underwaterDisturbPtFxNameDefault;}
		atHashWithStringNotFinal GetUnderwaterDisturbPtFxNameSand() const {return m_underwaterDisturbPtFxNameSand;}
		atHashWithStringNotFinal GetUnderwaterDisturbPtFxNameDirt() const {return m_underwaterDisturbPtFxNameDirt;}
		atHashWithStringNotFinal GetUnderwaterDisturbPtFxNameWater() const {return m_underwaterDisturbPtFxNameWater;}
		atHashWithStringNotFinal GetUnderwaterDisturbPtFxNameFoliage() const {return m_underwaterDisturbPtFxNameFoliage;}
		float GetUnderwaterDisturbPtFxRangeSqr() const {return m_underwaterDisturbPtFxRange*m_underwaterDisturbPtFxRange;}
		float GetUnderwaterDisturbPtFxSpeedEvoMin() const {return m_underwaterDisturbPtFxSpeedEvoMin;}
		float GetUnderwaterDisturbPtFxSpeedEvoMax() const {return m_underwaterDisturbPtFxSpeedEvoMax;}

		// parachute deploy vfx
		bool GetParachuteDeployPtFxEnabled() const {return m_parachuteDeployPtFxEnabled;}
		float GetParachuteDeployPtFxRangeSqr() const {return m_parachuteDeployPtFxRange*m_parachuteDeployPtFxRange;}
		atHashWithStringNotFinal GetParachuteDeployPtFxName() const {return m_parachuteDeployPtFxName;}
		eAnimBoneTag GetParachuteDeployPtFxBoneTag() const {return m_parachuteDeployPtFxBoneTag;}

		// parachute smoke vfx
		bool GetParachuteSmokePtFxEnabled() const {return m_parachuteSmokePtFxEnabled;}
		float GetParachuteSmokePtFxRangeSqr() const {return m_parachuteSmokePtFxRange*m_parachuteSmokePtFxRange;}
		atHashWithStringNotFinal GetParachuteSmokePtFxName() const {return m_parachuteSmokePtFxName;}
		eAnimBoneTag GetParachuteSmokePtFxBoneTag() const {return m_parachuteSmokePtFxBoneTag;}

		// parachute ped trail vfx
		bool GetParachutePedTrailPtFxEnabled() const {return m_parachutePedTrailPtFxEnabled;}
		int GetParachutePedTrailPtFxNumInfos() const {return m_parachutePedTrailPtFxInfos.GetCount();}
		const VfxPedParachutePedTrailInfo& GetParachutePedTrailPtFxInfo(int idx) const {return m_parachutePedTrailPtFxInfos[idx];}

		// parachute canopy trail vfx
		bool GetParachuteCanopyTrailPtFxEnabled() const {return m_parachuteCanopyTrailPtFxEnabled;}
		float GetParachuteCanopyTrailtFxRangeSqr() const {return m_parachuteCanopyTrailPtFxRange*m_parachuteCanopyTrailPtFxRange;}
		atHashWithStringNotFinal GetParachuteCanopyTrailPtFxName() const {return m_parachuteCanopyTrailPtFxName;}


	private: //////////////////////////

		// parser
		PAR_SIMPLE_PARSABLE;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// vfx groups
		atArray<VfxPedVfxGroupInfo> m_vfxGroupInfos;

		// skeleton bones
		atArray<VfxPedSkeletonBoneInfo> m_skeletonBoneInfos;

		// foot vfx
		float m_footVfxRange;
		bool m_footPtFxEnabled;
		float m_footPtFxSpeedEvoMin;
		float m_footPtFxSpeedEvoMax;
		bool m_footDecalsEnabled;

		// wade vfx
		bool m_wadePtFxEnabled;
		float m_wadePtFxRange;
		bool m_wadeDecalsEnabled;
		float m_wadeDecalsRange;

		// blood vfx
		bool m_bloodPoolVfxEnabled;
		bool m_bloodEntryPtFxEnabled;
		float m_bloodEntryPtFxRange;
		float m_bloodEntryPtFxScale;
		bool m_bloodEntryDecalEnabled;
		float m_bloodEntryDecalRange;
		bool m_bloodExitPtFxEnabled;
		float m_bloodExitPtFxRange;
		float m_bloodExitPtFxScale;
		bool m_bloodExitDecalEnabled;
		float m_bloodExitDecalRange;
		float m_bloodExitProbeRange;
		float m_bloodExitPosDistHead;
		float m_bloodExitPosDistTorso;
		float m_bloodExitPosDistLimb;
		float m_bloodExitPosDistFoot;
		float m_bloodPoolVfxSizeMult;

		// shot vfx
		bool m_shotPtFxEnabled;
		float m_shotPtFxRange;
		atHashWithStringNotFinal m_shotPtFxName;

		// fire vfx
		bool m_firePtFxEnabled;
		atArray<VfxPedFireInfo> m_firePtFxInfos;
		float m_firePtFxSpeedEvoMin;
		float m_firePtFxSpeedEvoMax;

		// smoulder vfx
		bool m_smoulderPtFxEnabled;
		float m_smoulderPtFxRange;
		atHashWithStringNotFinal m_smoulderPtFxName;
		eAnimBoneTag m_smoulderPtFxBoneTagA;
		eAnimBoneTag m_smoulderPtFxBoneTagB;

		// breath vfx
		bool m_breathPtFxEnabled;
		float m_breathPtFxRange;
		atHashWithStringNotFinal m_breathPtFxName;
		float m_breathPtFxSpeedEvoMin;
		float m_breathPtFxSpeedEvoMax;
		float m_breathPtFxTempEvoMin;
		float m_breathPtFxTempEvoMax;
		float m_breathPtFxRateEvoMin;
		float m_breathPtFxRateEvoMax;

		// breath water vfx
		bool m_breathWaterPtFxEnabled;
		float m_breathWaterPtFxRange;
		atHashWithStringNotFinal m_breathWaterPtFxName;
		float m_breathWaterPtFxSpeedEvoMin;
		float m_breathWaterPtFxSpeedEvoMax;
		float m_breathWaterPtFxDepthEvoMin;
		float m_breathWaterPtFxDepthEvoMax;

		// breath scuba vfx
		bool m_breathScubaPtFxEnabled;
		float m_breathScubaPtFxRange;
		atHashWithStringNotFinal m_breathScubaPtFxName;
		float m_breathScubaPtFxSpeedEvoMin;
		float m_breathScubaPtFxSpeedEvoMax;
		float m_breathScubaPtFxDepthEvoMin;
		float m_breathScubaPtFxDepthEvoMax;

		// splash vfx
		bool m_splashVfxEnabled;
		float m_splashVfxRange;

		// splash lod vfx
		bool m_splashLodPtFxEnabled;
		float m_splashLodVfxRange;
		atHashWithStringNotFinal m_splashLodPtFxName;
		float m_splashLodPtFxSpeedEvoMin;
		float m_splashLodPtFxSpeedEvoMax;

		// splash entry vfx
		bool m_splashEntryPtFxEnabled;
		float m_splashEntryVfxRange;
		atHashWithStringNotFinal m_splashEntryPtFxName;
		float m_splashEntryPtFxSpeedEvoMin;
		float m_splashEntryPtFxSpeedEvoMax;

		// splash in vfx
		atHashWithStringNotFinal m_splashInPtFxName;
		float m_splashInPtFxSpeedThresh;
		float m_splashInPtFxSpeedEvoMin;
		float m_splashInPtFxSpeedEvoMax;

		// splash out vfx
		atHashWithStringNotFinal m_splashOutPtFxName;
		float m_splashOutPtFxSpeedEvoMin;
		float m_splashOutPtFxSpeedEvoMax;

		// splash wade vfx
		atHashWithStringNotFinal m_splashWadePtFxName;
		float m_splashWadePtFxSpeedEvoMin;
		float m_splashWadePtFxSpeedEvoMax;

		// splash trail vfx
		atHashWithStringNotFinal m_splashTrailPtFxName;
		float m_splashTrailPtFxSpeedEvoMin;
		float m_splashTrailPtFxSpeedEvoMax;
		float m_splashTrailPtFxDepthEvoMin;
		float m_splashTrailPtFxDepthEvoMax;

		// water drip vfx
		atArray<int> m_waterDripPtFxToSkeltonBoneIds;
		atArray<int> m_waterDripPtFxFromSkeltonBoneIds;
		atHashWithStringNotFinal m_waterDripPtFxName;
		float m_waterDripPtFxTimer;

		// underwater disturbance vfx
		bool m_underwaterDisturbPtFxEnabled;
		atHashWithStringNotFinal m_underwaterDisturbPtFxNameDefault;
		atHashWithStringNotFinal m_underwaterDisturbPtFxNameSand;
		atHashWithStringNotFinal m_underwaterDisturbPtFxNameDirt;
		atHashWithStringNotFinal m_underwaterDisturbPtFxNameWater;
		atHashWithStringNotFinal m_underwaterDisturbPtFxNameFoliage;
		float m_underwaterDisturbPtFxRange;
		float m_underwaterDisturbPtFxSpeedEvoMin;
		float m_underwaterDisturbPtFxSpeedEvoMax;

		// parachute deploy vfx
		bool m_parachuteDeployPtFxEnabled;
		float m_parachuteDeployPtFxRange;
		atHashWithStringNotFinal m_parachuteDeployPtFxName;
		eAnimBoneTag m_parachuteDeployPtFxBoneTag;

		// parachute smoke vfx
		bool m_parachuteSmokePtFxEnabled;
		float m_parachuteSmokePtFxRange;
		atHashWithStringNotFinal m_parachuteSmokePtFxName;
		eAnimBoneTag m_parachuteSmokePtFxBoneTag;

		// parachute ped trail vfx
		bool m_parachutePedTrailPtFxEnabled;
		atArray<VfxPedParachutePedTrailInfo> m_parachutePedTrailPtFxInfos;

		// parachute canopy trail vfx
		bool m_parachuteCanopyTrailPtFxEnabled;
		float m_parachuteCanopyTrailPtFxRange;
		atHashWithStringNotFinal m_parachuteCanopyTrailPtFxName;

};

class CVfxPedInfoMgr
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		CVfxPedInfoMgr();

		// parser
		void PreLoadCallback(parTreeNode* pNode);
		void PostLoadCallback();

		void LoadData();

		// foot infos
		VfxPedFootDecalInfo* GetPedFootDecalInfo(u32 hashName);
		VfxPedFootPtFxInfo* GetPedFootPtFxInfo(u32 hashName);
		VfxPedFootInfo* GetPedFootInfo(u32 hashName);

		// wade infos
		VfxPedWadeDecalInfo* GetPedWadeDecalInfo(u32 hashName);
		VfxPedWadePtFxInfo* GetPedWadePtFxInfo(u32 hashName);
		VfxPedWadeInfo* GetPedWadeInfo(u32 hashName);

		// skeleton bone infos
		VfxPedBoneWadeInfo* GetPedBoneWadeInfo(u32 hashName);
		VfxPedBoneWaterInfo* GetPedBoneWaterInfo(u32 hashName);

		CVfxPedInfo* GetInfo(u32 hashName);
		CVfxPedInfo* GetInfo(CPed* pPed);

		void SetupDecalRenderSettings();

#if __DEV
		static void UpdatePedModelInfo(CPedModelInfo* pPedModelInfo);
#endif
	

	private: //////////////////////////

		void SetupWaterDripInfos();

		// parser
		PAR_SIMPLE_PARSABLE;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		atBinaryMap<VfxPedFootDecalInfo*, atHashWithStringNotFinal> m_vfxPedFootDecalInfos;
		atBinaryMap<VfxPedFootPtFxInfo*, atHashWithStringNotFinal> m_vfxPedFootPtFxInfos;
		atBinaryMap<VfxPedFootInfo*, atHashWithStringNotFinal> m_vfxPedFootInfos;

		atBinaryMap<VfxPedWadeDecalInfo*, atHashWithStringNotFinal> m_vfxPedWadeDecalInfos;
		atBinaryMap<VfxPedWadePtFxInfo*, atHashWithStringNotFinal> m_vfxPedWadePtFxInfos;
		atBinaryMap<VfxPedWadeInfo*, atHashWithStringNotFinal> m_vfxPedWadeInfos;

		atBinaryMap<VfxPedBoneWadeInfo*, atHashWithStringNotFinal> m_vfxPedBoneWadeInfos;
		atBinaryMap<VfxPedBoneWaterInfo*, atHashWithStringNotFinal> m_vfxPedBoneWaterInfos;

		atBinaryMap<CVfxPedInfo*, atHashWithStringNotFinal> m_vfxPedInfos;

#if __DEV
		atArray<VfxPedLiveEditUpdateData> m_vfxPedLiveEditUpdateData;
#endif

};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxPedInfoMgr g_vfxPedInfoMgr;


#endif // VFX_PEDINFO_H



