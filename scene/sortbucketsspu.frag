
#include "system/dependency.h"
#include "streamer/sortbucketsspu.cpp" 

using rage::sysDependency;

SPUFRAG_DECL(bool, sortbucketsspu, sysDependency&);
SPUFRAG_IMPL(bool, sortbucketsspu, sysDependency& dependency) {
	return sortbucketsspu_Dependency( dependency);
}
