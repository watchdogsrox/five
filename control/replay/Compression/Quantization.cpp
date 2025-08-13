//=====================================================================================================
// name:		Quantization.cpp
//=====================================================================================================

#include "Quantization.h"

#if GTA_REPLAY

//=============================================================================
u8 QuantizeU8(float fValue, float fMin, float fMax, int iBits)
{
	return Quantize<u8>(fValue, fMin, fMax, iBits);
}

//=============================================================================
float DeQuantizeU8(u8 iValue, float fMin, float fMax, int iBits)
{
	return DeQuantize<u8>(iValue, fMin, fMax, iBits);
}

//=============================================================================
u32 QuantizeQuaternionS3(const Quaternion &_v)
{
	Quaternion v;
	v.Set(_v.x, _v.y, _v.z, _v.w);

	v.Normalize();

	//Smallest 3 quantization (Quaternion assumed normalized)
	u32 iLargest = 0;
	float fLargest = v.x;
	float fLargestAbs = Abs(fLargest);

	float fTest = v.y;
	float fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 1;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}

	fTest = v.z;
	fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 2;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}

	fTest = v.w;
	fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 3;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}

	//Do we need to flip the quaternion?
	if ((fLargest > 0) ^ (fLargestAbs > 0))
	{
		v.Negate();
	}

	//Serialize the index of the largest value
	u32 iRet = iLargest<<30;

	float fRemaining[3];
	switch(iLargest)
	{
		case 0:
			fRemaining[0] = v.y;
			fRemaining[1] = v.z;
			fRemaining[2] = v.w;
			break;

		case 1:
			fRemaining[0] = v.x;
			fRemaining[1] = v.z;
			fRemaining[2] = v.w;
			break;

		case 2:
			fRemaining[0] = v.x;
			fRemaining[1] = v.y;
			fRemaining[2] = v.w;
			break;
		default:
			fRemaining[0] = v.x;
			fRemaining[1] = v.y;
			fRemaining[2] = v.z;
			break;
	}


	//Quantize each value from -0.707 to 0.707
	const float fOneOverSqrt2 = 1.0f / sqrtf(2.0f);

	// float precision crap
	if (fRemaining[0] > fOneOverSqrt2 && !Eq(fRemaining[0], fOneOverSqrt2, FLT_EPSILON))
	{
		replayAssert(Abs(fRemaining[0]) <= fOneOverSqrt2);
	}
	if (fRemaining[1] > fOneOverSqrt2 && !Eq(fRemaining[1], fOneOverSqrt2, FLT_EPSILON))
	{
		replayAssert(Abs(fRemaining[1]) <= fOneOverSqrt2);
	}
	if (fRemaining[2] > fOneOverSqrt2 && !Eq(fRemaining[2], fOneOverSqrt2, FLT_EPSILON))
	{
		replayAssert(Abs(fRemaining[2]) <= fOneOverSqrt2);
	}
	u32 iTmpVal;

	iTmpVal = Quantize<u32>(fRemaining[0], -fOneOverSqrt2, fOneOverSqrt2, 10);
	iRet |= iTmpVal<<20;
	iTmpVal = Quantize<u32>(fRemaining[1], -fOneOverSqrt2, fOneOverSqrt2, 10);
	iRet |= iTmpVal<<10;
	iTmpVal = Quantize<u32>(fRemaining[2], -fOneOverSqrt2, fOneOverSqrt2, 10);
	iRet |= iTmpVal;

	return iRet;
}

//=============================================================================
void DeQuantizeQuaternionS3(Quaternion &v, u32 iVal)
{
	//Smallest 3 quantization (Quaternion assumed normalized)

	//Serialize the index of the largest value
	u32 iLargest = iVal>>30;
	float fOthers[3];

	//Quantize each value from -0.707 to 0.707
	const float fOneOverSqrt2 = 1.0f / sqrtf(2.0f);
	float fTotal2=0.0f;

	u32 iTmpVal;
	float fValue;
	const int iTenBits = ((1<<10) - 1);

	iTmpVal = (iVal>>20) & iTenBits;
	fValue = DeQuantize<u32>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2, 10);
	fTotal2 += fValue * fValue;
	fOthers[0] = fValue;
	iTmpVal = (iVal>>10) & iTenBits;
	fValue = DeQuantize<u32>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2, 10);
	fTotal2 += fValue * fValue;
	fOthers[1] = fValue;
	iTmpVal = iVal & iTenBits;
	fValue = DeQuantize<u32>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2, 10);
	fTotal2 += fValue * fValue;
	fOthers[2] = fValue;

	//Recalculate the largest value (quantization defined it as being positive by flipping the quaternion if needed)
	float fLargest = sqrtf(1.0f - fTotal2);

	//Fill the quaternion
	switch(iLargest)
	{
		case 0:
			v.Set(fLargest, fOthers[0], fOthers[1], fOthers[2]);
			break;

		case 1:
			v.Set(fOthers[0], fLargest, fOthers[1], fOthers[2]);
			break;

		case 2:
			v.Set(fOthers[0], fOthers[1], fLargest, fOthers[2]);
			break;

		case 3:
			v.Set(fOthers[0], fOthers[1], fOthers[2], fLargest);
			break;
	}
}

//=============================================================================
u64 QuantizeQuaternionS3_20(const Quaternion &_v)
{
	Quaternion v;
	v.Set(_v.x, _v.y, _v.z, _v.w);

	v.Normalize();

	//Smallest 3 quantization (Quaternion assumed normalized)
	u64 iLargest = 0;
	float fLargest = v.x;
	float fLargestAbs = Abs(fLargest);

	float fTest = v.y;
	float fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 1;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}

	fTest = v.z;
	fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 2;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}

	fTest = v.w;
	fTestAbs = Abs(fTest);
	if (fTestAbs > fLargestAbs)
	{
		iLargest = 3;
		fLargest = fTest;
		fLargestAbs = fTestAbs;
	}

	//Do we need to flip the quaternion?
	if ((fLargest > 0) ^ (fLargestAbs > 0))
	{
		v.Negate();
	}

	//Serialize the index of the largest value
	u64 iRet = iLargest<<60;

	float fRemaining[3];
	switch(iLargest)
	{
		case 0:
			fRemaining[0] = v.y;
			fRemaining[1] = v.z;
			fRemaining[2] = v.w;
			break;

		case 1:
			fRemaining[0] = v.x;
			fRemaining[1] = v.z;
			fRemaining[2] = v.w;
			break;

		case 2:
			fRemaining[0] = v.x;
			fRemaining[1] = v.y;
			fRemaining[2] = v.w;
			break;
		default:
			fRemaining[0] = v.x;
			fRemaining[1] = v.y;
			fRemaining[2] = v.z;
			break;
	}


	//Quantize each value from -0.707 to 0.707
	const float fOneOverSqrt2 = 1.0f / sqrtf(2.0f);

	// float precision crap
	if (fRemaining[0] > fOneOverSqrt2 && !Eq(fRemaining[0], fOneOverSqrt2, FLT_EPSILON))
	{
		replayAssert(Abs(fRemaining[0]) <= fOneOverSqrt2);
	}
	if (fRemaining[1] > fOneOverSqrt2 && !Eq(fRemaining[1], fOneOverSqrt2, FLT_EPSILON))
	{
		replayAssert(Abs(fRemaining[1]) <= fOneOverSqrt2);
	}
	if (fRemaining[2] > fOneOverSqrt2 && !Eq(fRemaining[2], fOneOverSqrt2, FLT_EPSILON))
	{
		replayAssert(Abs(fRemaining[2]) <= fOneOverSqrt2);
	}
	u64 iTmpVal;

	iTmpVal = Quantize<u64>(fRemaining[0], -fOneOverSqrt2, fOneOverSqrt2, 20);
	iRet |= iTmpVal<<40;
	iTmpVal = Quantize<u64>(fRemaining[1], -fOneOverSqrt2, fOneOverSqrt2, 20);
	iRet |= iTmpVal<<20;
	iTmpVal = Quantize<u64>(fRemaining[2], -fOneOverSqrt2, fOneOverSqrt2, 20);
	iRet |= iTmpVal;

	return iRet;
}

//=============================================================================
void DeQuantizeQuaternionS3_20(Quaternion &v, u64 iVal)
{
	//Smallest 3 quantization (Quaternion assumed normalized)

	//Serialize the index of the largest value
	u64 iLargest = iVal>>60;
	float fOthers[3];

	//Quantize each value from -0.707 to 0.707
	const float fOneOverSqrt2 = 1.0f / sqrtf(2.0f);
	float fTotal2=0.0f;

	u64 iTmpVal;
	float fValue;
	const int iTenBits = ((1<<20) - 1);

	iTmpVal = (iVal>>40) & iTenBits;
	fValue = DeQuantize<u64>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2, 20);
	fTotal2 += fValue * fValue;
	fOthers[0] = fValue;
	iTmpVal = (iVal>>20) & iTenBits;
	fValue = DeQuantize<u64>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2, 20);
	fTotal2 += fValue * fValue;
	fOthers[1] = fValue;
	iTmpVal = iVal & iTenBits;
	fValue = DeQuantize<u64>(iTmpVal, -fOneOverSqrt2, fOneOverSqrt2, 20);
	fTotal2 += fValue * fValue;
	fOthers[2] = fValue;

	//Recalculate the largest value (quantization defined it as being positive by flipping the quaternion if needed)
	float fLargest = sqrtf(1.0f - fTotal2);

	//Fill the quaternion
	switch(iLargest)
	{
		case 0:
			v.Set(fLargest, fOthers[0], fOthers[1], fOthers[2]);
			break;

		case 1:
			v.Set(fOthers[0], fLargest, fOthers[1], fOthers[2]);
			break;

		case 2:
			v.Set(fOthers[0], fOthers[1], fLargest, fOthers[2]);
			break;

		case 3:
			v.Set(fOthers[0], fOthers[1], fOthers[2], fLargest);
			break;
	}
}

#endif // GTA_REPLAY