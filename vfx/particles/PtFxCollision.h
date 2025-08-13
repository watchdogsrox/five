///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxCollision.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	05 Mar 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PTFX_COLLISION_H
#define	PTFX_COLLISION_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "crSkeleton/Skeleton.h"
#include "phBound/BoundComposite.h"
#include "physics/inst.h"
#include "rmptfx/ptxeffectinst.h"

// game
#include "scene/RegdRefTypes.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

class CEntity;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define MAX_COLN_PLANES					(32)
#define MAX_COLN_BOUNDS					(16)
#define MAX_COLN_POLYS_BOUND			(256)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum PtFxColnType_e
{
	PTFX_COLN_TYPE_NONE				= 0,
	PTFX_COLN_TYPE_GROUND_PLANE,
	PTFX_COLN_TYPE_GROUND_BOUND,
	PTFX_COLN_TYPE_NEARBY_BOUND,
	PTFX_COLN_TYPE_VEHICLE_PLANE,
	PTFX_COLN_TYPE_UNDERWATER_PLANE,
	PTFX_COLN_TYPE_GROUND_BOUNDS,
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CPtFxColnBase
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

virtual						~CPtFxColnBase					()				{}

		void				Init							();
		void				Update							();

virtual	void				Process							() = 0;

#if __BANK
		void				RenderDebug						();
#endif


	private: //////////////////////////


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	protected: ////////////////////////

		Vec3V				m_vPos;
		PtFxColnType_e		m_colnType;
		float				m_probeDist;
		float				m_range;

		bool				m_isActive;
		bool				m_registeredThisFrame;
		bool				m_needsProcessed;
		bool				m_onlyProcessBVH;

#if __BANK
		Color32				m_renderCol;
#endif

		ptxCollisionSet		m_colnSet;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CPtFxColnPlane : public CPtFxColnBase
{
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CPtFxCollision;


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

virtual	void				Process							();


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////	

};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CPtFxColnBound : public CPtFxColnBase
{
	///////////////////////////////////
	// FRIENDS 
	///////////////////////////////////

	friend class CPtFxCollision;


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

virtual	void				Process							();


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////	

};


//
class CPtFxCollision
{
	friend class CPtFxColnPlane;
	friend class CPtFxColnBound;

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void				Init							();
		void				Shutdown						();
		void				Update							();

#if __BANK
		void				RenderDebug						();
#endif

		void				Register						(ptxEffectInst* pFxInst);
		void				RegisterUserPlane				(ptxEffectInst* pFxInst, Vec3V_In vPos, Vec3V_In vNormal, float range, u8 mtlId);


	private: //////////////////////////

		void				RegisterColnPlane				(ptxEffectInst* pFxInst, Vec3V_In vColnPos, PtFxColnType_e colnType, float probeDist, float colnRange);
		void				RegisterColnBound				(ptxEffectInst* pFxInst, Vec3V_In vColnPos, PtFxColnType_e colnType, float probeDist, float colnRange, bool onlyProcessBVH);

		CPtFxColnPlane*		FindColnPlane					(Vec3V_In vPos, PtFxColnType_e colnType);
		CPtFxColnBound*		FindColnBound					(Vec3V_In vPos, PtFxColnType_e colnType, bool onlyProcessBVH);

static	void				ProcessPlane					(ptxCollisionSet* pColnSet, Vec3V_In vPos, Vec3V_In vNormal, float range, u8 mtlId);
static	void				ProcessUnderWaterPlane			(ptxCollisionSet* pColnSet, Vec3V_In vPos, float range);
static	void				ProcessGroundPlane				(ptxCollisionSet* pColnSet, Vec3V_In vPos, float probeDist, float range);
static	void				ProcessGroundBounds				(ptxCollisionSet* pColnSet, Vec3V_In vPos, float probeDist, float range, bool onlyProcessBVH, bool useColnEntityOnly);
static	void				ProcessNearbyBounds				(ptxCollisionSet* pColnSet, Vec3V_In vPos, float range, bool onlyProcessBVH);
static	void				ProcessVehiclePlane				(ptxCollisionSet* pColnSet, Vec3V_In vPos, float probeDist);

static	void				AddInstBoundsToColnSet			(ptxCollisionSet* pColnSet, s32 maxPolys, phInst* pInst, Vec3V_In vPos, float range, bool onlyProcessBVH);
static	void				AddBoundsToColnSet				(ptxCollisionSet* pColnSet, s32 maxPolys, phInst* pInst, phBound* pBound, Mat34V_In boundMtx, Vec3V_In vPos, float range, bool onlyProcessBVH);
static	void				AddVehBoundsToColnSet			(ptxCollisionSet* pColnSet, phInst* pInst);


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////	

		CPtFxColnPlane		m_colnPlanes					[MAX_COLN_PLANES];
		CPtFxColnBound		m_colnBounds					[MAX_COLN_BOUNDS];


};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////


#endif // PTFX_COLLISION_H











