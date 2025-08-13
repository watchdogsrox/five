//
// name:		NetBlenderTrain.h
// written by:	Daniel Yelland
// description:	Network blender used by trains

#ifndef NETBLENDERTRAIN_H
#define NETBLENDERTRAIN_H

// game includes
#include "network/objects/prediction/NetBlenderVehicle.h"

// ===========================================================================================================================
// CNetBlenderSubmarine
// ===========================================================================================================================
class CTrain;

class CNetBlenderTrain : public CNetBlenderVehicle
{
public:

	CNetBlenderTrain(CTrain* train, CVehicleBlenderData* blenderData);
	~CNetBlenderTrain();

	virtual void Update();
	virtual void ProcessPostPhysics();
	virtual void OnOwnerChange(blenderResetFlags resetFlag = RESET_POSITION);

protected:
	
	virtual void OnPredictionPop() const;

private:
	bool UpdateTrainPosition(CTrain* train, const Vector3& vposition, const Vector3& vpredictedposition, float& fdist);
	void UpdateTrainVelocity(CTrain* train, const Vector3& vpositiondelta, float& fdist);

	void DebugUpdateTrainPositionA(const Vector3& vposition, const Vector3& vpredictedposition);
	void DebugUpdateTrainPositionB(CTrain* train);

	static void StoreLocalPedOffset(CTrain* train);
	static void UpdateLocalPedOffset(CTrain* train);
};

#endif  // NETBLENDERSUBMARINE_H
