///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Wind.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	18 Mar 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WIND_H
#define WIND_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
//#include "pheffects/winddisturbancebox.h"
//#include "pheffects/winddisturbancecyclone.h"
#include "pheffects/winddisturbancedownwash.h"
#include "pheffects/winddisturbanceexplosion.h"
#include "pheffects/winddisturbancehemisphere.h"
#include "pheffects/winddisturbancesphere.h"
#include "pheffects/winddisturbancethermal.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

//#define MAX_WIND_BOXES				(16)
//#define MAX_WIND_CYCLONES			(8)
#define MAX_WIND_DOWNWASHES			(4)
#define MAX_WIND_EXPLOSIONS			(8)
#define MAX_WIND_DIREXPLOSIONS		(8)
#define MAX_WIND_HEMISPHERES		(16)
#define MAX_WIND_SPHERES			(16)
#define MAX_WIND_THERMALS			(64)

//#define MAX_WATER_CYCLONES			(32)


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

namespace rage
{
	class bkBank;
}


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CWind
{
	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	public: ///////////////////////////

		// main functions
		void 				Init						(unsigned initMode);
		void 				Shutdown					(unsigned shutdownMode);

		void 				Update						(float windSpeed, float waterSpeed);

#if __BANK
		void				InitLevelWidgets			();
		void				ShutdownLevelWidgets		();
		void				RenderDebug					();
#endif

		void				AddDownwash					(Vec3V_In vPos, const phDownwashSettings& downwashSettings);
		void				AddExplosion				(Vec3V_In vPos, const phExplosionSettings& explosionSettings);
		void				AddDirExplosion				(Vec3V_In vPos, const phDirExplosionSettings& explosionSettings);
		void				AddHemisphere				(Vec3V_In vPos, const phHemisphereSettings& hemisphereSettings);
		void				AddSphere					(Vec3V_In vPos, const phSphereSettings& sphereSettings);

		s32					AddThermal					(Vec3V_In vPos, const phThermalSettings& thermalSettings);
		void				RemoveThermal				(s32 thermalIndex);

		Vec3V_Out			GetWindDirection			() {return Vec3V(FPSin(m_currWindDir), FPCos(m_currWindDir), 0.0f);}
		void				SetWindDirectionOverride	(float windDir) {m_overrideWindDir = windDir;}

		
	private: //////////////////////////

		void				UpdateWindDir				();
		void 				UpdateDisturbances			();

#if __BANK
		void 				UpdateSettings				();
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

//		phWindBoxGroup*									m_pBoxGroup;
//		phWindCycloneGroup*								m_pCycloneWindGroup;
//		phWindCycloneGroup*								m_pCycloneWaterGroup;
		phWindDownwashGroup*							m_pDownwashGroup;
		phWindExplosionGroup*							m_pExplosionGroup;
		phWindDirExplosionGroup*						m_pDirExplosionGroup;
		phWindHemisphereGroup*							m_pHemisphereGroup;
		phWindSphereGroup*								m_pSphereGroup;
		phWindThermalGroup*								m_pThermalGroup;

		float											m_currWindDir;
		float											m_targetWindDir;
		float											m_overrideWindDir;

};


///////////////////////////////////
// EXTERNS
///////////////////////////////////

extern bank_float WIND_HELI_WIND_DIST_FORCE;
extern bank_float WIND_HELI_WIND_DIST_Z_FADE_THRESH_MIN;
extern bank_float WIND_HELI_WIND_DIST_Z_FADE_THRESH_MAX;
extern bank_float WIND_VEH_WIND_DIST_MULT;
extern bank_float WIND_EXPLOSION_WIND_DIST_FORCE_MULT;
extern bank_float WIND_DIREXPLOSION_WIND_DIST_FORCE_MULT;


#endif // WIND_H
