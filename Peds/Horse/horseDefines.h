#ifndef HORSE_DEFINES_H
#define HORSE_DEFINES_H

#if defined RDR_VERSION && RDR_VERSION >= 300
	#define ENABLE_HORSE 1
#else
	#define ENABLE_HORSE 0
#endif
	
#if ENABLE_HORSE
	#define ENABLE_HORSE_ONLY(x) x
#else 
	#define ENABLE_HORSE_ONLY(x) 
#endif

#endif //HORSE_DEFINES_H