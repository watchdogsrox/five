#include "system/dependency.h"
#include "system/dma.h"
#include <cell/spurs.h>

using namespace rage;

#include "ProcessLightOcclusionAsync.cpp"

#include "occlusionAsync.cpp"
#include "fwscene/world/InteriorLocation.cpp"

#define RST_QUERY_ONLY	1
#define FRAG_TSKLIB_ONLY
#include "softrasterizer/scan.cpp"

using rage::sysDependency;

SPUFRAG_DECL(bool, ProcessLightOcclusionAsync, sysDependency&);
SPUFRAG_IMPL(bool, ProcessLightOcclusionAsync, sysDependency& dependency) {
	return ProcessLightOcclusionAsync::RunFromDependency( dependency );
}