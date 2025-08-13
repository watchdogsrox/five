//=====================================================================================================
// name:		Quantization.h
//=====================================================================================================

#ifndef _QUANTIZATION_H_
#define _QUANTIZATION_H_

#include "control\replay\ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/replay_channel.h"
#include "debug/Debug.h"
#include "ai/navmesh/navmesh.h"

template<typename T>
static T Quantize(float fValue, float fMin, float fMax, int iBits)
{
	fValue = Clamp(fValue, fMin, fMax);
	fValue -= fMin;
	fValue *= 1.0f / (fMax - fMin);
	float fRange = (float)((1<<iBits)-1);
	fValue += 0.5f / fRange;      //Reduce rounding errors
	fValue *= fRange;
	return (T)fValue;
}

template<typename T>
static float DeQuantize(T iValue, float fMin, float fMax, int iBits)
{
	float fValue = iValue / (float)((1<<iBits)-1);
	fValue *= (fMax-fMin);
	fValue += fMin;
	return fValue;
}

u8 QuantizeU8(float fValue, float fMin, float fMax, int iBits);
float DeQuantizeU8(u8 iValue, float fMin, float fMax, int iBits);

u32 QuantizeQuaternionS3(const Quaternion &_v);
void DeQuantizeQuaternionS3(Quaternion &v, u32 iVal);

u64 QuantizeQuaternionS3_20(const Quaternion &_v);
void DeQuantizeQuaternionS3_20(Quaternion &v, u64 iVal);

#endif // GTA_REPLAY

#endif // _QUANTISATION_H_