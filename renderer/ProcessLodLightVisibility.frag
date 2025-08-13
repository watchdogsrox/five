#include "system/dependency.h"
#include "system/dma.h"
#include <cell/spurs.h>

using namespace rage;

#include "atl/array.h"
#include "spatialdata/transposedplaneset.h"
#include "system/dependency.h"
#include "vector/color32.h"
#include "vectormath/vec3v.h"

#include "../scene/loader/MapData_Misc.h"
#include "../vfx/misc/LODLightManager.h"
#include "../vfx/misc/Coronas.h"

#define RST_QUERY_ONLY	1
#include "softrasterizer/scan.cpp"
#include "occlusionAsync.cpp"
#include "ProcessLodLightVisibility.cpp"

using rage::sysDependency;

SPUFRAG_DECL(bool, ProcessLodLightVisibility, sysDependency&);
SPUFRAG_IMPL(bool, ProcessLodLightVisibility, sysDependency& dependency) {
	return ProcessLodLightVisibility::RunFromDependency( dependency );
}