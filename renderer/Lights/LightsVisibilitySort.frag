#include "system/dependency.h"
#include "system/dma.h"
#include <cell/spurs.h>

using namespace rage;

#include "atl/array.h"
#include "spatialdata/transposedplaneset.h"
#include "system/dependency.h"
#include "vectormath/vec3v.h"

#include "LightsVisibilitySort.cpp"

using rage::sysDependency;

SPUFRAG_DECL(bool, LightsVisibilitySort, sysDependency&);
SPUFRAG_IMPL(bool, LightsVisibilitySort, sysDependency& dependency) {
	return LightsVisibilitySort::RunFromDependency( dependency );
}