///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxExplosion.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	24 February 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_EXPLOSION_H
#define	VFX_EXPLOSION_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "vectormath/classes.h"

// game
#include "Physics/WaterTestHelper.h"
#include "Scene/RegdRefTypes.h"
#include "Weapons/WeaponEnums.h"
#include "Weapons/ExplosionInst.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

class phGtaExplosionInst;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define VFXEXPLOSION_MAX_FIRE_PROBES				(32)
#define VFXEXPLOSION_MAX_TIMED_SCORCH_PROBES		(8)
#define VFXEXPLOSION_MAX_NONTIMED_SCORCH_PROBES		(16)

#define VFXEXPLOSION_NUM_SUB_EFFECTS				(3)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

// VfxExplosionInfo_s
struct VfxExplosionInfo_s
{
	atHashWithStringNotFinal fxHashName[VFXEXPLOSION_NUM_SUB_EFFECTS];
	atHashWithStringNotFinal fxLodHashName;
	atHashWithStringNotFinal fxAirHashName[VFXEXPLOSION_NUM_SUB_EFFECTS];
	atHashWithStringNotFinal fxAirLodHashName;
	float	fxScale;
	bool	createsScorchDecals;

	// fire info
	s32	minFires;
	s32	maxFires;
	float	minFireRange;
	float	maxFireRange;
	float	burnTimeMin;
	float	burnTimeMax;
	float	burnStrengthMin;
	float	burnStrengthMax;
	float	peakTimeMin;
	float	peakTimeMax;
	float	fuelLevel;
	float	fuelBurnRate;

	// alternative entry info
	s32		numAltEntries;

#if __BANK
	char	tagName[32];
#endif
};

struct VfxExplosionFireProbe_s
{
	Vec3V	m_vExplosionPos;
	RegdEnt m_regdExplosionOwner;

	float	m_burnTime;
	float	m_burnStrength;
	float	m_peakTime;
	float	m_fuelLevel;
	float	m_fuelBurnRate;
	u32		m_weaponHash;

	s8		m_vfxProbeId;
};

struct VfxExplosionTimedScorchProbe_s
{
	RegdEnt m_regdExplosionOwner;
	eWeaponEffectGroup m_weaponEffectGroup;

	s8		m_vfxProbeId;
};

struct VfxExplosionInstData_s
{
	Vec3V	m_vPosWld;
	Vec3V	m_vPosLcl;
	Vec3V	m_vDirLcl;
	RegdEnt m_regdOwner;
	RegdEnt	m_regdAttachEntity;
	s32		m_attachBoneIndex;
	VfxExplosionInfo_s* m_pVfxExpInfo;
	float	m_sizeScale;
	ePhysicalType m_physicalType;
	eWeaponEffectGroup m_weaponEffectGroup;
	bool	m_expInstInAir;
	eExplosionTag m_expTag;
};

struct VfxExplosionNonTimedScorchProbes_s
{
	VfxExplosionInstData_s m_expInstData;

	Vec3V 	m_vClosestColnPos;
	Vec3V 	m_vClosestColnNormal;
	float 	m_closestDistSqr;

	s8		m_vfxProbeIds[5];
	bool	m_isActive;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxExplosionAsyncProbeManager
{	

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////
		
		void Init();
		void Shutdown();
		void Update();

		void TriggerFireProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_In vExplosionPos, CEntity* pExplosionOwner, float burnTime, float burnStrength, float peakTime, float fuelLevel, float fuelBurnRate, u32 weaponHash);
		void TriggerTimedScorchProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CEntity* pExplosionOwner, eWeaponEffectGroup weaponEffectGroup);
		void TriggerNonTimedScorchProbes(phGtaExplosionInst* pExpInst, VfxExplosionInfo_s* pVfxExpInfo);


	private: //////////////////////////



	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		VfxExplosionFireProbe_s m_fireProbes						[VFXEXPLOSION_MAX_FIRE_PROBES];
		VfxExplosionTimedScorchProbe_s m_timedScorchProbes			[VFXEXPLOSION_MAX_TIMED_SCORCH_PROBES];
		VfxExplosionNonTimedScorchProbes_s m_nonTimedScorchProbes	[VFXEXPLOSION_MAX_NONTIMED_SCORCH_PROBES];

};

class CVfxExplosion
{	
	friend class CVfxExplosionAsyncProbeManager;

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		void				Update						();

		void				ProcessVfxExplosion			(phGtaExplosionInst* pExpInst, bool isTimedExplosion, float currRadius);

#if __BANK
		void				InitWidgets					();
#endif
		void				Append						(const CDataFileMgr::DataFile*);
	private: //////////////////////////

		// init helper functions
		void				ReadData					(const CDataFileMgr::DataFile*, bool checkForDupes = false);
		void				LoadData					();
		VfxExplosionInfo_s*	GetInfo						(atHashWithStringNotFinal hashName);

		void				CreateFires					(phGtaExplosionInst* pExpInst, VfxExplosionInfo_s* pExpInfo);

		// particle systems
		void				UpdatePtFxExplosion			(phGtaExplosionInst* pExpInst, VfxExplosionInfo_s* pExpInfo, float currRadius);
		void				TriggerPtFxExplosion		(VfxExplosionInstData_s& expInstData, VfxExplosionInfo_s* pExpInfo, Vec3V_In vColnPos, Vec3V_In vColnNormal);
		void				TriggerPtFxExplosionWater	(Vec3V_In vPos, float depth, WaterTestResultType underwaterType, float scale = 1.0f);

#if __BANK
static	void				Reset						();
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// file version
		float				m_version;

		// explosion data
		atBinaryMap<VfxExplosionInfo_s, atHashWithStringNotFinal> m_explosionInfoMap;

		CVfxExplosionAsyncProbeManager m_asyncProbeMgr;

		// debug variables
#if __BANK
		bool				m_bankInitialised;
		bool				m_renderExplosionInfo;
		bool				m_renderExplosionFireProbes;
		bool				m_renderExplosionTimedScorchProbes;
		bool				m_renderExplosionNonTimedScorchProbes;
		bool				m_renderExplosionNonTimedScorchProbeResults;
#endif

}; // CVfxExplosion



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxExplosion			g_vfxExplosion;





#endif // VFX_EXPLOSION_H


///////////////////////////////////////////////////////////////////////////////
