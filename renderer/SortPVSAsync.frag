#include "system/dependency.h"
#include "SortPVSAsync.cpp" 

using rage::sysDependency;

SPUFRAG_DECL(bool, SortPVSAsync, sysDependency&);
SPUFRAG_IMPL(bool, SortPVSAsync, sysDependency& dependency) {
	return SortPVSAsync_Dependency( dependency);
}