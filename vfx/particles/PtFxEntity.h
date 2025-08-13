///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxEntity.cpp
//	BY	: 	Alex Hadjadj (and Mark Nicholson)
//	FOR	:	Rockstar North
//	ON	:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef PTFX_ENTITY_H
#define PTFX_ENTITY_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
#include "diag/tracker.h"
#include "system/criticalsection.h"

// framework
#include "fwtl/pool.h"

// game
#include "Scene/Entity.h"
#include "Vfx/Particles/PtFxDefines.h"


#if PTFX_ALLOW_INSTANCE_SORTING

///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

#define PTFX_SORTED_VEHICLE_CAM_DIR_OFFSET						(ScalarVConstant<FLOAT_TO_INT(0.8f)>())
#define PTFX_SORTED_VEHICLE_CAM_POS_OFFSET						(ScalarVConstant<FLOAT_TO_INT(0.0009f)>())
#define PTFX_SORTED_VEHICLE_MIN_DIST_FROM_CENTER				(ScalarVConstant<FLOAT_TO_INT(0.6f)>())
///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

namespace rage 
{
	class dlCmdBase;
	class ptxEffectInst;
};

class CEntityDrawInfo;
class CRenderPhase;


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CPtFxSortedEntity
///////////////////////////////////////////////////////////////////////////////

class CPtFxSortedEntity : public CEntity
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		FW_REGISTER_CLASS_POOL(CPtFxSortedEntity);

								CPtFxSortedEntity	(const eEntityOwnedBy ownedBy, ptxEffectInst* pFxInst);
								~CPtFxSortedEntity	()								{ delete m_pDrawHandler; m_pDrawHandler = NULL; };

		virtual FASTRETURNCHECK(const spdAABB &) GetBoundBox			(spdAABB& box) const;

		void					SetDrawHandler		(fwDrawData *drawdata)			{ m_pDrawHandler = drawdata; }
		virtual fwDrawData*		AllocateDrawHandler	(rmcDrawable* pDrawable);
	
		void					SetFxInst			(ptxEffectInst* pFxInst);
		ptxEffectInst*			GetFxInst			() const						{return m_pFxInst;}
	
	protected: ////////////////////////

								CPtFxSortedEntity	(const eEntityOwnedBy ownedBy)	: CEntity(ownedBy)	{}


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	protected: ////////////////////////

		ptxEffectInst*		m_pFxInst;

};



///////////////////////////////////////////////////////////////////////////////
//  CPtFxSortedManager
///////////////////////////////////////////////////////////////////////////////

class CPtFxSortedManager
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

								CPtFxSortedManager				();
								~CPtFxSortedManager				();

		void					Init							();
		void					InitLevel						();
		void					Shutdown						();
		void					ShutdownLevel					();

static	void					Add								(ptxEffectInst* pFxInst);
		void					Remove							(ptxEffectInst* pFxInst);

		void					AddToRenderList					(u32 renderFlags, s32 entityListIndex);	
		void					CleanUp							();	


	private: //////////////////////////


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////		

	private: //////////////////////////

static	atArray<CPtFxSortedEntity*>	m_activeEntities;
static	sysCriticalSectionToken m_SortedEntityListCS;


};


#endif // PTFX_ALLOW_INSTANCE_SORTING

#endif // PTFX_ENTITY_H


