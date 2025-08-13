#include "system/dependency.h"
#include "system/dma.h"
#include <cell/spurs.h>

using namespace rage;

#include "atl/array.h"
#include "spatialdata/transposedplaneset.h"
#include "system/dependency.h"
#include "vectormath/vec3v.h"

#define RST_QUERY_ONLY	1
#include "softrasterizer/scan.cpp"

#include "../../renderer/occlusionAsync.cpp"
#include "DistantLightsVisibility.cpp"

using rage::sysDependency;

SPUFRAG_DECL(bool, DistantLightsVisibility, sysDependency&);
SPUFRAG_IMPL(bool, DistantLightsVisibility, sysDependency& dependency) {
	return DistantLightsVisibility::RunFromDependency( dependency );
}