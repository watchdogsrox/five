//
// filename:	CompEntityModelInfo.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "modelInfo\CompEntityModelInfo.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------


void CCompEntityModelInfo::CopyAnimsData(atArray<CCompEntityAnims> &animsData){

	u32 animsSize = animsData.GetCount();
	m_anims.Reset();
	m_anims.Reserve(animsSize);
	m_anims.Resize(animsSize);

	// copy data for each anim
	for(u32 i=0; i< animsSize; i++){
		m_anims[i] = animsData[i];			// deep copy		
	}
}