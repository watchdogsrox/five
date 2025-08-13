#ifndef ISLAND_HOPPER_DATA_H_
#define ISLAND_HOPPER_DATA_H_



class CIslandHopperData
{
public:

	atHashString Name;

	atString Cullbox;
	atString HeightMap;
	atString LodLights;

	atArray<atHashString> IPLsToEnable;
	atArray<atHashString> IMAPsToPreempt;

	PAR_SIMPLE_PARSABLE;
};



#endif // ISLAND_HOPPER_DATA_H_