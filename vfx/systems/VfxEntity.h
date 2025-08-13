///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxEntity.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	05 Sept 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_ENTITY_H
#define	VFX_ENTITY_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "vectormath/classes.h"

// game
#include "Scene/2dEffect.h"
#include "control/replay/ReplaySettings.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{		
	class ptxEffectInst;
	class ptxEffectRule;
	class fiAsciiTokenizer;
}

class CCompEntityEffectsData;
class CEntity;

struct CDecalAttr;
struct CParticleAttr;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define VFXENTITY_MAX_DECAL_PROBES		(32)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

// VfxEntityGenericInfo_s
struct VfxEntityGenericInfo_s
{
	atHashWithStringNotFinal	tagHashName;

#if __BANK
	char	tagName						[128];
#endif
}; 

// VfxEntityGenericPtFxInfo_s
struct VfxEntityGenericPtFxInfo_s : VfxEntityGenericInfo_s
{
	atHashWithStringNotFinal	ptFxHashName;
	atHashWithStringNotFinal	ptFxAssetHashName;
}; 

// VfxEntityAmbientPtFxInfo_s
struct VfxEntityAmbientPtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
	bool	ignoreRotation;
	bool	alwaysOn;
	float	sunThresh;
	float	rainThresh;
	float	maxWindEvo;
	float	windThresh;
	float	tempMin;
	float	tempMax;
	s32		timeMin;
	s32		timeMax;
	float	tempEvoMin;
	float	tempEvoMax;
	bool	hasRainEvo;
	bool	hasWindEvo;
	float	nightProbMult;
	bool	isTriggered;
	float	triggerTimeMin;
	float	triggerTimeMax;
}; 

// VfxEntityCollisionPtFxInfo_s
struct VfxEntityCollisionPtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
	bool	atCollisionPt;
	float	collImpThreshMin;
	float	collImpThreshMax;

}; 

// VfxEntityShotPtFxInfo_s
struct VfxEntityShotPtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
	bool	atCollisionPt;
}; 

// VfxEntityBreakPtFxInfo_s
struct VfxEntityBreakPtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
};

// VfxEntityDestroyPtFxInfo_s
struct VfxEntityDestroyPtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
	float	noTriggerExpRange;
}; 

// VfxEntityAnimPtFxInfo_s
struct VfxEntityAnimPtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
}; 

// VfxEntityRayFirePtFxInfo_s
struct VfxEntityRayFirePtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
}; 

// VfxEntityInWaterPtFxInfo_s
struct VfxEntityInWaterPtFxInfo_s : VfxEntityGenericPtFxInfo_s
{
	float	speedUpThreshMin;
	float	speedUpThreshMax;
};

// VfxEntityGenericDecalInfo_s
struct VfxEntityGenericDecalInfo_s : VfxEntityGenericInfo_s
{
	s32		renderSettingIndex;
	s32		renderSettingCount;
	float	sizeMin;
	float	sizeMax;
	float	lifeMin;
	float	lifeMax;
	float	fadeInTime;
	u8		colR;
	u8		colG;
	u8		colB;
	float	alphaMin;
	float	alphaMax;
	float	uvMultTime;
	float	uvMultStart;
	float	uvMultEnd;
	s32		numProbes;
	Vec3V	vProbeDir;
	float	probeRadiusMin;
	float	probeRadiusMax;
	float	probeDist;
};

// VfxEntityCollisionDecalInfo_s
struct VfxEntityCollisionDecalInfo_s : VfxEntityGenericDecalInfo_s
{
	bool	atCollisionPt;
	float	collImpThreshMin;
	float	collImpThreshMax;

}; 

// VfxEntityShotDecalInfo_s
struct VfxEntityShotDecalInfo_s : VfxEntityGenericDecalInfo_s
{
	bool	atCollisionPt;
}; 

// VfxEntityBreakDecalInfo_s
struct VfxEntityBreakDecalInfo_s : VfxEntityGenericDecalInfo_s
{
};

// VfxEntityDestroyDecalInfo_s
struct VfxEntityDestroyDecalInfo_s : VfxEntityGenericDecalInfo_s
{
};

// VfxEntityDecalProbe_s
struct VfxEntityDecalProbe_s
{
	const VfxEntityGenericDecalInfo_s* m_pDecalInfo;
	float	m_scale;

	s8		m_vfxProbeId;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxEntityAsyncProbeManager
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////
		
		void Init();
		void Shutdown();
		void Update();

		void TriggerDecalProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, const VfxEntityGenericDecalInfo_s* pDecalInfo, float scale);


	private: //////////////////////////



	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		VfxEntityDecalProbe_s m_decalProbes	[VFXENTITY_MAX_DECAL_PROBES];

};


class CVfxEntity
{	
	friend class CVfxEntityAsyncProbeManager;

#if GTA_REPLAY
	friend class CPacketEntityAnimTrigFx;
	friend class CPacketEntityAnimUpdtFx;
	friend class CPacketEntityFragBreakFx;
	friend class CPacketEntityShotFx;
	friend class CPacketEntityAmbientFx;
	friend class CPacketEntityCollisionFx;
	friend class CPacketPtFxFragmentDestroy;
	friend class CPacketDecalFragmentDestroy;
#endif 

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void							Init						(unsigned initMode);
		void							InitDefinitions				();
		void							Shutdown					(unsigned shutdownMode);

		void							Update						();

		void							RemoveScript				(scrThreadId scriptThreadId);

		void							SetInWaterPtFxDisabled		(bool val, scrThreadId scriptThreadId);

		// queries
		atHashWithStringNotFinal		GetPtFxAssetName			(eParticleType ptFxType, atHashWithStringNotFinal m_tagHashName);
#if __ASSERT
		bool							GetIsInfinitelyLooped		(const CParticleAttr& PtFxAttr);
#endif
		// particle systems
		void							ProcessPtFxEntityAmbient	(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId);
		void							TriggerPtFxEntityCollision	(CEntity* pEntity, const CParticleAttr* pPtFxAttr, Vec3V_In vPos, Vec3V_In vNormal, s32 collidedBoneTag, float accumImpulse);
		void							TriggerPtFxEntityShot		(CEntity* pEntity, const CParticleAttr* pPtFxAttr, Vec3V_In vPos, Vec3V_In vNormal, s32 shotBoneTag);
		void							TriggerPtFxFragmentBreak	(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 brokenBoneTag, s32 parentBoneTag);
		void							TriggerPtFxFragmentDestroy	(CEntity* pEntity, const CParticleAttr* pPtFxAttr, Mat34V_In fxMat);
		void							UpdatePtFxEntityAnim		(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId);
		void							TriggerPtFxEntityAnim		(CEntity* pEntity, const CParticleAttr* pPtFxAttr);
		void							UpdatePtFxEntityRayFire		(CEntity* pEntity, const CCompEntityEffectsData* pRayFireEffectData, s32 effectId);
		void							TriggerPtFxEntityRayFire	(CEntity* pEntity, const CCompEntityEffectsData* pRayFireEffectData);
		void							TriggerPtFxEntityInWater	(CEntity* pEntity, const CParticleAttr* pPtFxAttr);

		void							UpdatePtFxLightShaft		(CEntity* pEntity, const CLightShaft* pLightShaft, s32 effectId);


		// decals
		void							TriggerDecalEntityCollision	(CEntity* pEntity, const CDecalAttr* pDecalAttr, Vec3V_In vPos, Vec3V_In vNormal, s32 collidedBoneTag, float accumImpulse);
		void							TriggerDecalEntityShot		(CEntity* pEntity, const CDecalAttr* pDecalAttr, Vec3V_In vPos, Vec3V_In vNormal, s32 shotBoneTag);
		void							TriggerDecalFragmentBreak	(CEntity* pEntity, const CDecalAttr* pDecalAttr, s32 brokenBoneTag, s32 parentBoneTag);
		void							TriggerDecalFragmentDestroy	(CEntity* pEntity, const CDecalAttr* pDecalAttr, Mat34V_In fxMat);

#if __BANK
		void							InitWidgets					();
#endif


	private: //////////////////////////

		// init helpers
		void							LoadData					();
		void							LoadGenericPtFxData			(fiAsciiTokenizer& token, VfxEntityGenericPtFxInfo_s* pGenericPtFxInfo, char* pTagName);
		void							LoadGenericDecalData		(fiAsciiTokenizer& token, VfxEntityGenericDecalInfo_s* pGenericDecalInfo, char* pTagName);

		// particle effect data table look up helpers
		VfxEntityAmbientPtFxInfo_s*		GetAmbientPtFxInfo			(atHashWithStringNotFinal tagHashName)		{return m_ambientPtFxInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityCollisionPtFxInfo_s*	GetCollisionPtFxInfo		(atHashWithStringNotFinal tagHashName)		{return m_collisionPtFxInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityShotPtFxInfo_s*		GetShotPtFxInfo				(atHashWithStringNotFinal tagHashName)		{return m_shotPtFxInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityBreakPtFxInfo_s*		GetBreakPtFxInfo			(atHashWithStringNotFinal tagHashName)		{return m_breakPtFxInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityDestroyPtFxInfo_s*		GetDestroyPtFxInfo			(atHashWithStringNotFinal tagHashName)		{return m_destroyPtFxInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityAnimPtFxInfo_s*		GetAnimPtFxInfo				(atHashWithStringNotFinal tagHashName)		{return m_animPtFxInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityRayFirePtFxInfo_s*		GetRayFirePtFxInfo			(atHashWithStringNotFinal tagHashName)		{return m_rayfirePtFxInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityInWaterPtFxInfo_s*		GetInWaterPtFxInfo			(atHashWithStringNotFinal tagHashName)		{return m_inWaterPtFxInfoMap.SafeGet(tagHashName.GetHash());}

		// decal data table look up helpers																		
		VfxEntityCollisionDecalInfo_s*	GetCollisionDecalInfo		(atHashWithStringNotFinal tagHashName)		{return m_collisionDecalInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityShotDecalInfo_s*		GetShotDecalInfo			(atHashWithStringNotFinal tagHashName)		{return m_shotDecalInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityBreakDecalInfo_s*		GetBreakDecalInfo			(atHashWithStringNotFinal tagHashName)		{return m_breakDecalInfoMap.SafeGet(tagHashName.GetHash());}
		VfxEntityDestroyDecalInfo_s*	GetDestroyDecalInfo			(atHashWithStringNotFinal tagHashName)		{return m_destroyDecalInfoMap.SafeGet(tagHashName.GetHash());}

		// particle system 
		float							CalcAmbientProbability		(const CParticleAttr* pPtFxAttr, VfxEntityAmbientPtFxInfo_s* pAmbientInfo);
		void							UpdatePtFxEntityAmbient		(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId, VfxEntityAmbientPtFxInfo_s* pAmbientInfo);
		void							TriggerPtFxEntityAmbient	(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId, VfxEntityAmbientPtFxInfo_s* pAmbientInfo);

		// decal helpers
		void							TriggerDecalGeneric			(const VfxEntityGenericDecalInfo_s* pDecalInfo, Vec3V_In vPosWld, float scale);

		// misc helpers
		bool							ShouldTrigger				(atHashWithStringNotFinal tagHashName, s32 probability);
		bool							ShouldUpdate				(atHashWithStringNotFinal tagHashName, s32 probability, CEntity* pEntity, Vec3V_In vPosLcl, s32 effectId);
		float							GetFxEntitySeed				(CEntity* pEntity, Vec3V_In vPosLcl, float divNumber);
		bool							IsPtFxInRange				(ptxEffectRule* pEffectRule, float fxDistSqr);

		// debug helpers
#if __BANK
static	void							Reset						();
#endif

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// file version
		float							m_version;

		// ambient particle effect data
		atBinaryMap<VfxEntityAmbientPtFxInfo_s, atHashWithStringNotFinal> m_ambientPtFxInfoMap;

		// collision particle effect data
		atBinaryMap<VfxEntityCollisionPtFxInfo_s, atHashWithStringNotFinal> m_collisionPtFxInfoMap;

		// shot particle effect data
		atBinaryMap<VfxEntityShotPtFxInfo_s, atHashWithStringNotFinal> m_shotPtFxInfoMap;

		// break particle effect data
		atBinaryMap<VfxEntityBreakPtFxInfo_s, atHashWithStringNotFinal> m_breakPtFxInfoMap;

		// destroy particle effect data
		atBinaryMap<VfxEntityDestroyPtFxInfo_s, atHashWithStringNotFinal> m_destroyPtFxInfoMap;

		// anim particle effect data
		atBinaryMap<VfxEntityAnimPtFxInfo_s, atHashWithStringNotFinal> m_animPtFxInfoMap;

		// ray fire particle effect data
		atBinaryMap<VfxEntityRayFirePtFxInfo_s, atHashWithStringNotFinal> m_rayfirePtFxInfoMap;

		// in water particle effect data
		atBinaryMap<VfxEntityInWaterPtFxInfo_s, atHashWithStringNotFinal> m_inWaterPtFxInfoMap;
	
		// collision decal data
		atBinaryMap<VfxEntityCollisionDecalInfo_s, atHashWithStringNotFinal> m_collisionDecalInfoMap;

		// shot decal data
		atBinaryMap<VfxEntityShotDecalInfo_s, atHashWithStringNotFinal> m_shotDecalInfoMap;

		// break decal data
		atBinaryMap<VfxEntityBreakDecalInfo_s, atHashWithStringNotFinal> m_breakDecalInfoMap;

		// destroy decal data
		atBinaryMap<VfxEntityDestroyDecalInfo_s, atHashWithStringNotFinal> m_destroyDecalInfoMap;

		// 
		CVfxEntityAsyncProbeManager		m_asyncProbeMgr;

		bool m_inWaterPtFxDisabled;
		scrThreadId m_inWaterPtFxDisabledScriptThreadId;


		// debug variables
#if __BANK
		bool							m_bankInitialised;

		bool							m_renderAmbientDebug;
		bool							m_renderEntityDecalProbes;
		bool							m_onlyProcessThisTag;
		char							m_onlyProcessThisTagName	[64];
#endif


}; // CVfxEntity


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxEntity					g_vfxEntity;


#endif // VFX_ENTITY_H



