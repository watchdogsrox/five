#include "system/dependency.h"
#include "CopyOffMatrixSet.cpp"

using rage::sysDependency;

SPUFRAG_DECL(bool, CopyOffMatrixSetSPU, sysDependency&);
SPUFRAG_IMPL(bool, CopyOffMatrixSetSPU, sysDependency& dependency) {
	return CopyOffMatrixSetSPU_Dependency( dependency );
}
