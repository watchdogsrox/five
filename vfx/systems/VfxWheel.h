///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWheel.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	07 May 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_WHEEL_H
#define	VFX_WHEEL_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage

// game
#include "Physics/GtaMaterial.h"
#include "Vfx/Systems/VfxMaterial.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class grcTexture;
	class ptxEffectInst;
}

class CVehicle;
class CVfxVehicleInfo;
class CWheel;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define	VFXRANGE_WHEEL_SKIDMARK				(50.0f)
#define	VFXRANGE_WHEEL_SKIDMARK_SQR			(VFXRANGE_WHEEL_SKIDMARK*VFXRANGE_WHEEL_SKIDMARK)

#define	VFXRANGE_WHEEL_FRICTION				(50.0f)
#define	VFXRANGE_WHEEL_FRICTION_SQR			(VFXRANGE_WHEEL_FRICTION*VFXRANGE_WHEEL_FRICTION)

#define	VFXRANGE_WHEEL_DISPLACEMENT_HI		(50.0f)
#define	VFXRANGE_WHEEL_DISPLACEMENT_HI_SQR	(VFXRANGE_WHEEL_DISPLACEMENT_HI*VFXRANGE_WHEEL_DISPLACEMENT_HI)

#define	VFXRANGE_WHEEL_DISPLACEMENT_LO		(160.0f)
#define	VFXRANGE_WHEEL_DISPLACEMENT_LO_SQR	(VFXRANGE_WHEEL_DISPLACEMENT_LO*VFXRANGE_WHEEL_DISPLACEMENT_LO)

#define	VFXRANGE_WHEEL_BURNOUT				(60.0f)
#define	VFXRANGE_WHEEL_BURNOUT_SQR			(VFXRANGE_WHEEL_BURNOUT*VFXRANGE_WHEEL_BURNOUT)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// wheel ptfx offsets for registered systems that play on the same wheel
enum PtFxWheelOffsets_e
{
	PTFXOFFSET_WHEEL_FRIC_DRY					= 0,
	PTFXOFFSET_WHEEL_DISP_DRY,
	PTFXOFFSET_WHEEL_BURNOUT_DRY,
	PTFXOFFSET_WHEEL_FRIC_WET,
	PTFXOFFSET_WHEEL_DISP_WET,
	PTFXOFFSET_WHEEL_BURNOUT_WET,
	PTFXOFFSET_WHEEL_FIRE,
};

// the tyre states that can produce different effects
enum VfxTyreState_e
{
	VFXTYRESTATE_OK_DRY						= 0,
	VFXTYRESTATE_BURST_DRY,
	VFXTYRESTATE_OK_WET,
	VFXTYRESTATE_BURST_WET,

	VFXTYRESTATE_NUM
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct VfxWheelInfo_s
{
	// skidmark decals
	float	skidmarkSlipThreshMin;
	float	skidmarkSlipThreshMax;
	float	skidmarkPressureThreshMin;
	float	skidmarkPressureThreshMax;

	s32		decal1RenderSettingIndex;
	s32		decal1RenderSettingCount;
	s32		decal2RenderSettingIndex;
	s32		decal2RenderSettingCount;
	s32		decal3RenderSettingIndex;
	s32		decal3RenderSettingCount;

	s32		decalSlickRenderSettingIndex;	// hack for formula vehicle
	s32		decalSlickRenderSettingCount;	// hack for formula vehicle

	u8		decalColR;
	u8		decalColG;
	u8		decalColB;

	// particle effects
	float	ptFxWheelFricThreshMin;
	float	ptFxWheelFricThreshMax;
	atHashWithStringNotFinal	ptFxWheelFric1HashName;
	atHashWithStringNotFinal	ptFxWheelFric2HashName;
	atHashWithStringNotFinal	ptFxWheelFric3HashName;

	float	ptFxWheelDispThreshMin;
	float	ptFxWheelDispThreshMax;
	atHashWithStringNotFinal	ptFxWheelDisp1HashName;
	atHashWithStringNotFinal	ptFxWheelDisp2HashName;
	atHashWithStringNotFinal	ptFxWheelDisp3HashName;
	atHashWithStringNotFinal	ptFxWheelDispLodHashName;

	float	ptFxWheelBurnoutFricMin;
	float	ptFxWheelBurnoutFricMax;
	float	ptFxWheelBurnoutTempMin;
	float	ptFxWheelBurnoutTempMax;
	atHashWithStringNotFinal	ptFxWheelBurnout1HashName;
	atHashWithStringNotFinal	ptFxWheelBurnout2HashName;
	atHashWithStringNotFinal	ptFxWheelBurnout3HashName;

	// lights
	bool	lightEnabled;
	u8		lightColRMin;
	u8		lightColGMin;
	u8		lightColBMin;
	u8		lightColRMax;
	u8		lightColGMax;
	u8		lightColBMax;
	float	lightIntensityMin;
	float	lightIntensityMax;
	float	lightRangeMin;
	float	lightRangeMax;
	float	lightFalloffMin;
	float	lightFalloffMax;

};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxWheel
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void				Init						(unsigned initMode);
		void				Shutdown					(unsigned shutdownMode);

		void				RemoveScript				(scrThreadId scriptThreadId);

		// data table access
		VfxWheelInfo_s*		GetInfo						(VfxTyreState_e tyreState, VfxGroup_e vfxGroup)		{return &m_vfxWheelInfos[tyreState][vfxGroup];}

		// settings
		void				SetWheelPtFxLodRangeScale	(float val, scrThreadId scriptThreadId);
		float				GetWheelPtFxLodRangeScale	()						{return m_wheelPtFxLodRangeScale;}

		void				SetWheelSkidmarkRangeScale	(float val, scrThreadId scriptThreadId);
		float				GetWheelSkidmarkRangeScale	()						{return m_wheelSkidmarkRangeScale;}

		void				SetUseSnowWheelVfxWhenUnsheltered(bool enabled)		{m_useSnowWheelVfxWhenUnsheltered = enabled;}
		bool				GetUseSnowWheelVfxWhenUnsheltered()					{ return m_useSnowWheelVfxWhenUnsheltered;}

		// particle systems (generic)
		void				UpdatePtFxWheelFriction		(CWheel* pWheel, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, float distSqrToCam, float frictionVal, float wetEvo, bool isWet);
		void				UpdatePtFxWheelDisplacement	(CWheel* pWheel, CVehicle* BANK_ONLY(pVehicle), CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, Vec3V_In vWheelNormal, float distSqrToCam, float displacementVal, float wetEvo, bool isWet);
		void				UpdatePtFxWheelBurnout		(CWheel* pWheel, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, float distSqrToCam, float frictionVal, float wetEvo, bool isWet);

		void				TriggerPtFxWheelPuncture	(CWheel* pWheel, CVehicle* pVehicle, Vec3V_In vPos, Vec3V_In vNormal, s32 boneIndex);
		void				TriggerPtFxWheelBurst		(CWheel* pWheel, CVehicle* pVehicle, Vec3V_In vPos, s32 boneIndex, bool dueToWheelSpin, bool dueToFire);

		void				ProcessWheelLights			(CWheel* pWheel, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, float distSqrToCam, float frictionVal);

		// debug functions
#if __BANK
		void				InitWidgets					();
		bool				GetDisableSkidmarks			()						{return m_disableSkidmarks;}
		bool				GetDisableWheelFL			()						{return m_disableWheelFL;}
		bool				GetDisableWheelFR			()						{return m_disableWheelFR;}
		bool				GetDisableWheelML			()						{return m_disableWheelML;}
		bool				GetDisableWheelMR			()						{return m_disableWheelMR;}
		bool				GetDisableWheelRL			()						{return m_disableWheelRL;}
		bool				GetDisableWheelRR			()						{return m_disableWheelRR;}

		bool				GetDisableNoMovementOpt		()						{return m_disableNoMovementOpt;}

		float				GetSkidmarkOverlapPerc		()						{return m_skidmarkOverlapPerc;}

		float				GetWetRoadOverride			()						{return m_wetRoadOverride;}

		bool				GetOutputWheelVfxValues		()						{return m_outputWheelVfxValues;}

		void				RenderDebugWheelMatrices	(CWheel* pWheel, CVehicle* pVehicle);			
#endif

	private: //////////////////////////

		// init helper functions
		void				LoadData					();

		// wheel helper functions
		void				SetWheelFxZoom				(CWheel* pWheel, ptxEffectInst* pFxInst);

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

		// wheel data table
		VfxWheelInfo_s		m_vfxWheelInfos				[VFXTYRESTATE_NUM][NUM_VFX_GROUPS];

		// wheel settings
		scrThreadId			m_wheelPtFxLodRangeScaleScriptThreadId;
		float				m_wheelPtFxLodRangeScale;

		scrThreadId			m_wheelSkidmarkRangeScaleScriptThreadId;
		float				m_wheelSkidmarkRangeScale;

		bool				m_useSnowWheelVfxWhenUnsheltered;

		// debug variables
#if __BANK
		bool				m_bankInitialised;

		bool				m_disableSkidmarks;
		bool				m_disableWheelFricFx;
		bool				m_disableWheelDispFx;
		bool				m_disableWheelBurnoutFx;
		bool				m_disableWheelFL;
		bool				m_disableWheelFR;
		bool				m_disableWheelML;
		bool				m_disableWheelMR;
		bool				m_disableWheelRL;
		bool				m_disableWheelRR;

		float				m_skidmarkOverlapPerc;

		float				m_wetRoadOverride;

		float				m_smokeOverrideTintR;
		float				m_smokeOverrideTintG;
		float				m_smokeOverrideTintB;
		float				m_smokeOverrideTintA;
		bool				m_smokeOverrideTintActive;

		bool				m_outputWheelVfxValues;
		bool				m_outputWheelFricDebug;
		bool				m_outputWheelDispDebug;
		bool				m_outputWheelBurnoutDebug;

		bool				m_renderWheelBoneMatrices;
		bool				m_renderWheelDrawMatrices;

		bool				m_disableNoMovementOpt;
#endif 

}; // CVfxWheel


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxWheel			g_vfxWheel;

extern	dev_float			VFXWHEEL_SKID_WIDTH_MULT;
extern	dev_float			VFXWHEEL_SKID_WIDTH_MULT_BICYCLE;
extern	dev_float			VFXWHEEL_SKID_MAX_ALPHA;

extern	dev_float			VFXWHEEL_WETROAD_DRY_MAX_THRESH;
extern	dev_float			VFXWHEEL_WETROAD_WET_MIN_THRESH;

extern	dev_float			VFXWHEEL_WET_REDUCE_TEMP_MIN;
extern	dev_float			VFXWHEEL_WET_REDUCE_TEMP_MAX;

extern	dev_float			VFXWHEEL_COOL_RATE;




#endif // VFX_WHEEL_H




