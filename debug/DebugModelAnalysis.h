// ==========================
// debug/DebugModelAnalysis.h
// (c) 2012 RockstarNorth
// ==========================

#ifndef _DEBUG_DEBUGMODELANALYSIS_H_
#define _DEBUG_DEBUGMODELANALYSIS_H_

#if __BANK

#include "atl/array.h"
#include "vectormath/classes.h"

#include "debug/DebugModelAnalysis.h"

namespace rage { class bkBank; }
namespace rage { class fiStream; }
namespace rage { class rmcDrawable; }

class CEntity;

class CDebugModelAnalysisInterface
{
public:
	static void AddWidgets(bkBank& bank);
	static void Update(const CEntity* pEntity);
};

#endif // __BANK
#endif // _DEBUG_DEBUGMODELANALYSIS_H_
