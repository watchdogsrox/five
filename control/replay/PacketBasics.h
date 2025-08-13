#ifndef PACKETBASICS_H
#define PACKETBASICS_H

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "replay_channel.h"
//#include "Misc/ReplayPacket.h"

#include "fwscene/world/InteriorLocation.h"
#include "vector/vector3.h"
#include "vector/quaternion.h"
#include "vectormath/legacyconvert.h"

#include "control/replay/ReplaySupportClasses.h"
#include "control/replay/ReplayTrackingInfo.h"
#include "system/FileMgr.h"
#include "fwscene/world/InteriorLocation.h"
#include "control/replay/Compression/Quantization.h"

class CPhysical;

//////////////////////////////////////////////////////////////////////////
template<typename COMPRESSEDTYPE>
class converter
{
public:
	converter(COMPRESSEDTYPE minCompressed, COMPRESSEDTYPE maxCompressed, float min, float max)
		: m_min(min)
		, m_max(max)
	{
		float compressedRange = (float)maxCompressed - (float)minCompressed;
		float valueRange = max - min;
		m_compressFactor = compressedRange / valueRange;
		m_decompressFactor = 1.0f / m_compressFactor;
	}

	COMPRESSEDTYPE	Compress(float value) const 
	{	
		ASSERT_ONLY(const float TOLERANCE = SMALL_FLOAT);
		replayAssertf((m_min <= value || AreNearlyEqual(m_min, value, TOLERANCE)) && (value <= m_max || AreNearlyEqual(m_max, value, TOLERANCE)), "Value out of range min: %f, max: %f, value: %f", m_min, m_max, value);
		value = Clamp(value, m_min, m_max);
		return (COMPRESSEDTYPE)(value * m_compressFactor);	
	}
	float			Decompress(COMPRESSEDTYPE value) const	
	{	
		float result = (float)value * m_decompressFactor;
		replayAssertf(m_min <= result && result <= m_max, "Value out of range min: %f, max: %f, value: %f", m_min, m_max, (float)value);
		return result;
	}

	float GetMin() const
	{
		return m_min;
	}

	float GetMax() const
	{
		return m_max;
	}

private:
	float			m_min;
	float			m_max;
	float			m_compressFactor;
	float			m_decompressFactor;
};

extern converter<u8> u8_f32_0_to_3_0;
extern converter<u8> u8_f32_0_to_5;
extern converter<s8> s8_f32_n127_to_127;
extern converter<u8> u8_f32_0_to_65;
extern converter<s16> s16_f32_n650_to_650;
extern converter<s8> s8_f32_n1_to_1;
extern converter<u8> u8_f32_0_to_2;
extern converter<s8> s8_f32_nPI_to_PI;
extern converter<s8> s8_f32_n20_to_20;
extern converter<s16> s16_f32_n4_to_4;
extern converter<s16> s16_f32_n35_to_35;

//////////////////////////////////////////////////////////////////////////
template<typename INTERNALTYPE, const converter<INTERNALTYPE>& CONVERTER>
class valComp
{
public:
	operator float() const
	{
		return ToFloat();
	}

	float ToFloat() const
	{
		return CONVERTER.Decompress(m_value);
	}

	void operator=(const float srcVal)
	{
		m_value = CONVERTER.Compress(srcVal);
	}

	float GetMin() const
	{
		return CONVERTER.GetMin();
	}

	float GetMax() const
	{
		return CONVERTER.GetMax();
	}

private:
	INTERNALTYPE	m_value;
};


template<typename INTERNALTYPE, const converter<INTERNALTYPE>& CONVERTER>
class valCompTracked
{
public:
	operator float() const
	{
		return CONVERTER.Decompress(m_value);
	}

	void operator=(const float srcVal)
	{
		m_value = CONVERTER.Compress(srcVal);

		if(srcVal < m_minValue)
		{
			m_minValue = srcVal;
			replayDebugf1("***New Minimum %f***", m_minValue);
		}
		else if(srcVal > m_maxValue)
		{
			m_maxValue = srcVal;
			replayDebugf1("***New Maximum %f***", m_maxValue);
		}
	}

private:
	INTERNALTYPE	m_value;

	static float	m_minValue;
	static float	m_maxValue;
};


template<int FLTCOUNT>
class CPacketVector
{
public:
	CPacketVector()
	{
		for(int i = 0; i < FLTCOUNT; ++i)
			m_component[i] = 0.0f;
	}
	CPacketVector(const rage::Vector4& vec)		{	Store(vec);		}
	CPacketVector(const rage::Vec4V& vec)		{	Store(vec);		}
	CPacketVector(const rage::Vector3& vec)		{	Store(vec);		}
	CPacketVector(const rage::Vec3V& vec)		{	Store(vec);		}
	CPacketVector(const rage::Vector2& vec)		{	Store(vec);		}
	CPacketVector(const rage::Vec2V& vec)		{	Store(vec);		}

	void Store(const rage::Vector4& vec)
	{
		CompileTimeAssert(FLTCOUNT == 4);
		for(int i = 0; i < FLTCOUNT; ++i)
			m_component[i] = vec[i];
	}

	void Store(const rage::Vec4V& vec)
	{
		CompileTimeAssert(FLTCOUNT == 4);
		for(int i = 0; i < FLTCOUNT; ++i)
			m_component[i] = vec[i];
	}

	void Store(const Quaternion& q)
	{
		CompileTimeAssert(FLTCOUNT == 4);
		m_component[0] = q.x;
		m_component[1] = q.y;
		m_component[2] = q.z;
		m_component[3] = q.w;
	}

	void Load(rage::Vector4& vec) const
	{
		CompileTimeAssert(FLTCOUNT == 4);
		for(int i = 0; i < FLTCOUNT; ++i)
			vec[i] = m_component[i];
	}

	void Load(rage::Vec4V& vec) const
	{
		CompileTimeAssert(FLTCOUNT == 4);
		for(int i = 0; i < FLTCOUNT; ++i)
			vec[i] = m_component[i];
	}

	void Load(Quaternion& q) const
	{
		CompileTimeAssert(FLTCOUNT == 4);
		q.x = m_component[0];
		q.y = m_component[1];
		q.z = m_component[2];
		q.w = m_component[3];
	}

	void Store(const rage::Vector3& vec)
	{
			CompileTimeAssert(FLTCOUNT == 3);
		for(int i = 0; i < FLTCOUNT; ++i)
			m_component[i] = vec[i];
	}

	void Store(const rage::Vec3V& vec)
	{
		CompileTimeAssert(FLTCOUNT == 3);
		for(int i = 0; i < FLTCOUNT; ++i)
			m_component[i] = vec[i];
	}

	void Load(rage::Vector3& vec) const
	{
		CompileTimeAssert(FLTCOUNT == 3);
		for(int i = 0; i < FLTCOUNT; ++i)
			vec[i] = m_component[i];
	}

	void Load(rage::Vec3V& vec) const
	{
		CompileTimeAssert(FLTCOUNT == 3);
		for(int i = 0; i < FLTCOUNT; ++i)
			vec[i] = m_component[i];
	}

	void Store(const rage::Vector2& vec)
	{
		CompileTimeAssert(FLTCOUNT == 2);
		for(int i = 0; i < FLTCOUNT; ++i)
			m_component[i] = vec[i];
	}

	void Load(rage::Vector2& vec) const
	{
		CompileTimeAssert(FLTCOUNT == 2);
		for(int i = 0; i < FLTCOUNT; ++i)
			vec[i] = m_component[i];
	}

	void Load(Matrix34& vec) const
	{
		for(int i = 0; i < FLTCOUNT; ++i)
			vec.d[i] = m_component[i];
	}


	CPacketVector<FLTCOUNT> operator=(const rage::Vector2& vec) { Store(vec); return *this; }
	CPacketVector<FLTCOUNT> operator=(const rage::Vector3& vec) { Store(vec); return *this; }
	CPacketVector<FLTCOUNT> operator=(const rage::Vec2V& vec) { Store(vec); return *this; }
	CPacketVector<FLTCOUNT> operator=(const rage::Vec3V& vec) { Store(vec); return *this; }

	const	float& operator[] (const int i) const	{	return m_component[i];	}
	float& operator[] (const int i)			{	return m_component[i];	}

	bool operator!=(const CPacketVector<FLTCOUNT>& other) const
	{
		for(int i = 0; i < FLTCOUNT; ++i)
		{
			if(m_component[i] != other.m_component[i])
				return true;
		}
		return false;
	}

	bool operator==(const CPacketVector<FLTCOUNT>& other) const
	{
		for(int i = 0; i < FLTCOUNT; ++i)
		{
			if(m_component[i] != other.m_component[i])
				return false;
		}
		return true;
	}

	operator rage::Vector3() const	{	CompileTimeAssert(FLTCOUNT == 3);	return rage::Vector3(m_component[0], m_component[1], m_component[2]);	}
	operator rage::Vector2() const	{	CompileTimeAssert(FLTCOUNT == 2);	return rage::Vector2(m_component[0], m_component[1]);	}
	operator rage::Vec3V() const	{	CompileTimeAssert(FLTCOUNT == 3);	return rage::Vec3V(m_component[0], m_component[1], m_component[2]);	}
	operator rage::Vec2V() const	{	CompileTimeAssert(FLTCOUNT == 2);	return rage::Vec2V(m_component[0], m_component[1]);	}

private:
	float m_component[FLTCOUNT];
};


//////////////////////////////////////////////////////////////////////////
class CPacketVector3
{
public:
	void Store(const Vector3& rPos)
	{
		m_Position[0] = rPos.x;
		m_Position[1] = rPos.y;
		m_Position[2] = rPos.z;
	}

	void Load(Vector3& rPos) const
	{
		rPos.x = m_Position[0];
		rPos.y = m_Position[1];
		rPos.z = m_Position[2];
	}

	void Load(Matrix34& rPos) const
	{
		Load(rPos.d);
	}

	const	float& operator[] (const int i) const	{	return m_Position[i];	}
			float& operator[] (const int i)			{	return m_Position[i];	}

			bool operator!=(const CPacketVector3& other) const
			{
				return m_Position[0] != other.m_Position[0] || m_Position[1] != other.m_Position[1] || m_Position[2] != other.m_Position[2];
			}
private:
	float	m_Position[3];
};




//////////////////////////////////////////////////////////////////////////
template<typename T>
class CPacketVector3Comp
{
public:
	CPacketVector3Comp(){}
	CPacketVector3Comp(const Vector3& vec)	{	Store(vec);	}
	CPacketVector3Comp(Vec3V_In vec)		{	Store(vec);	}

	void	Store(const Vector3& rPos)
	{
		m_component[0] = rPos.x;
		m_component[1] = rPos.y;
		m_component[2] = rPos.z;
	}

	void	Store(Vec3V_In rPos)
	{
		m_component[0] = rPos[0];
		m_component[1] = rPos[1];
		m_component[2] = rPos[2];
	}

	void	Load(Vector3& rPos) const
	{
		rPos.x = m_component[0];
		rPos.y = m_component[1];
		rPos.z = m_component[2];
	}

	void	Load(Vec3V& rPos) const
	{
		rPos[0] = m_component[0];
		rPos[1] = m_component[1];
		rPos[2] = m_component[2];
	}

	void	Load(Matrix34& rPos) const
	{
		Load(rPos.d);
	}

	void Set(T x, T y, T z)
	{
		m_component[0] = x;
		m_component[1] = y;
		m_component[2] = z;
	}

	void Get(T &x, T &y, T &z)
	{
		x = m_component[0];
		y = m_component[1];
		z = m_component[2];
	}

	operator rage::Vector3() const	{	return rage::Vector3(m_component[0], m_component[1], m_component[2]);	}
	operator rage::Vec3V() const	{	return rage::Vec3V(m_component[0], m_component[1], m_component[2]);		}

private:
	T	m_component[3];
};


//////////////////////////////////////////////////////////////////////////
class CPacketVector4Comp
{
public:
	void	Store(const Vector4& rPos)
	{
		m_Position[0] = Clamp(rPos.x, -1.0f, 1.0f);
		m_Position[1] = Clamp(rPos.y, -1.0f, 1.0f);
		m_Position[2] = Clamp(rPos.z, -1.0f, 1.0f);
		m_Position[3] = Clamp(rPos.w, -1.0f, 1.0f);
	}

	void	Load(Vector4& rPos) const
	{
		rPos.x = m_Position[0];
		rPos.y = m_Position[1];
		rPos.z = m_Position[2];
		rPos.w = m_Position[3];
	}

private:
	valComp<s8, s8_f32_n1_to_1>	m_Position[4];
};


//////////////////////////////////////////////////////////////////////////
class CPacketQuaternionL
{
public:
	void	StoreQuaternion(const Quaternion& quat)
	{ m_quaternion = QuantizeQuaternionS3(quat); }

	void	LoadQuaternion(Quaternion& quat) const
	{ DeQuantizeQuaternionS3(quat, m_quaternion); }

	void	SetZero()			{	m_quaternion = 0;			} //this means the orginal 3x3 part of the matrix contained zero's
	bool	IsZero() const		{	return m_quaternion == 0;	}

	void	Set(u32 val)			{	m_quaternion = val;			}
	u32		Get() const			{	return m_quaternion;		}

	bool operator!=(const CPacketQuaternionL& other) const
	{
		return m_quaternion != other.m_quaternion;
	}
private:
	u32		m_quaternion;
};


//////////////////////////////////////////////////////////////////////////
class CPacketQuaternionH
{
public:
	void	StoreQuaternion(const Quaternion& quat)
	{ m_quaternion = QuantizeQuaternionS3_20(quat);	}

	void	LoadQuaternion(Quaternion& quat) const
	{ DeQuantizeQuaternionS3_20(quat, m_quaternion); }

	void	SetZero()				{	m_quaternion = 0;			}
	bool	IsZeroRotation() const	{	return m_quaternion == 0;	}

	void	Set(u64 val)			{	m_quaternion = val;			}
	u64		Get() const				{	return m_quaternion;		}

	bool operator!=(const CPacketQuaternionH& other) const
	{
		return m_quaternion != other.m_quaternion;
	}
private:
	u64		m_quaternion;
};


//////////////////////////////////////////////////////////////////////////
class CPacketPositionAndQuaternion
{
public:

	CPacketPositionAndQuaternion()	{}
	CPacketPositionAndQuaternion(const Matrix34& mat)	{	StoreMatrix(mat);	}

	void	FetchQuaternionAndPos(Quaternion& rQuat, Vector3& vec) const 
	{
		GetQuaternion(rQuat);

		m_Position.Load(vec);
	}

	void	GetQuaternion(Quaternion& rQuat) const
	{
		Assertf(IsZeroMatrix() == false, "This quaterion is set to zero");
		m_Quaternion.LoadQuaternion(rQuat);
	}

	void	LoadPosition(Vector3& vec) const
	{
		m_Position.Load(vec);
	}

	void	LoadPosition(Matrix34& rMatrix) const
	{
		m_Position.Load(rMatrix);
	}

	void	StoreMatrix(const Matrix34& rMatrix)
	{
		//a matrix is sometimes set to zero to hide objects, this need to be restored correctly in the replay
		if(rMatrix.a.IsZero() && rMatrix.b.IsZero() && rMatrix.c.IsZero())
	 	{
	 		m_Quaternion.SetZero();
	 	}
		else
		{
			Quaternion q;
			rMatrix.ToQuaternion(q);

			m_Quaternion.StoreQuaternion(q);
		}

		m_Position[0] = rMatrix.d.x;
		m_Position[1] = rMatrix.d.y;
		m_Position[2] = rMatrix.d.z;
	}

	void	StorePosQuat(const Vector3& rPos, const Quaternion& rQuat)
	{
		m_Position.Store(rPos);
		m_Quaternion.StoreQuaternion(rQuat);
	}

	void	LoadMatrix(Matrix34& rMatrix) const
	{
		//if this is zero then it means the original matrix should be zero, is sometimes used to hide objects
	 	if(m_Quaternion.IsZero())
	 	{
	 		rMatrix.Zero();
	 		return;
	 	}
	 	else
		{
			Quaternion q;
			m_Quaternion.LoadQuaternion(q);
			rMatrix.FromQuaternion(q);
			replayAssert(rMatrix.IsOrthonormal());
		}

		rMatrix.d.x = m_Position[0];
		rMatrix.d.y = m_Position[1];
		rMatrix.d.z = m_Position[2];
	}

	void	LoadMatrix(Matrix34& rMatrix, CPacketPositionAndQuaternion& nextPosAndRot, float interp) const
	{
		Vector3 rVecNext;
		nextPosAndRot.LoadPosition(rVecNext);

		Quaternion rQuatNext;
		rQuatNext.Identity();

		if(nextPosAndRot.IsZeroMatrix())
		{
			// Best option here is to just zero it out.  Technically there's a point between 'this' frame and the 'next' frame
			// where the bone went to zero but there is no way to know that without recording every single game frame.  Leaving
			// it to only go to zero on the recorded frame means we can see bones stationary as they have no interpolation before
			// disappearing.  See url:bugstar:4219527 for example
			rMatrix.Zero();
			LoadPosition(rMatrix.d);
			return;
		}
		else
		{
			nextPosAndRot.GetQuaternion(rQuatNext);
		}

		//don't interpolate from a zeroed matrix just set it
		if(m_Quaternion.IsZero())
		{
			rMatrix.Zero();
			LoadPosition(rMatrix.d);
			return;
		}
		else
		{
			Quaternion rQuatPrev;
			GetQuaternion(rQuatPrev);

			Quaternion rQuatInterp;
			rQuatPrev.PrepareSlerp(rQuatNext);
			rQuatInterp.Slerp(interp, rQuatPrev, rQuatNext);

			rMatrix.FromQuaternion(rQuatInterp);
			replayAssert(rMatrix.IsOrthonormal());
		}

		Vector3 rVecPrev;
		LoadPosition(rVecPrev);
		rMatrix.d = rVecPrev * (1.0f - interp) + rVecNext * interp;
	}

	bool	IsZeroMatrix() const	{ return m_Quaternion.IsZero();	}

	void	Interpolate(const CPacketPositionAndQuaternion& other, float interp, Matrix34& outMat) const
	{
		Vector3 otherVec;
		other.LoadPosition(otherVec);
		Quaternion otherQuat;
		other.GetQuaternion(otherQuat);

		LoadPosition(outMat);
		Quaternion thisQuat;
		GetQuaternion(thisQuat);


		Quaternion rQuatInterp;
		thisQuat.PrepareSlerp(otherQuat);
		rQuatInterp.Slerp(interp, thisQuat, otherQuat);

		outMat.FromQuaternion(rQuatInterp);
		replayAssert(outMat.IsOrthonormal());


		outMat.d = outMat.d * (1.0f - interp) + otherVec * interp;
	}

private:
	CPacketVector3		m_Position;
	CPacketQuaternionL	m_Quaternion;
};


//////////////////////////////////////////////////////////////////////////
class CPacketPositionAndQuaternion20
{
public:

	void	FetchQuaternionAndPos(Quaternion& rQuat, Vector3& rPos) const
	{
		GetQuaternion(rQuat);

		m_Position.Load(rPos);
	}

	void	GetQuaternion(Quaternion& rQuat) const 
	{
		m_Quaternion.LoadQuaternion(rQuat);
	}

	void	StorePosition(const Vector3& vec)
	{
		m_Position.Store(vec);
	}

	void	LoadPosition(Vector3& vec) const
	{
		m_Position.Load(vec);
	}

	void	LoadPosition(Matrix34& rMatrix) const
	{
		m_Position.Load(rMatrix);
	}

	void	StoreMatrix(const Matrix34& rMatrix)
	{
		//a matrix is sometimes set to zero to hide objects, this need to be restored correctly in the replay
		if(rMatrix.a.IsZero() && rMatrix.b.IsZero() && rMatrix.c.IsZero())
		{
			m_Quaternion.SetZero();
		}
		else
		{
			Quaternion q;
			rMatrix.ToQuaternion(q);
			m_Quaternion.StoreQuaternion(q);
		}

		m_Position[0] = rMatrix.d.x;
		m_Position[1] = rMatrix.d.y;
		m_Position[2] = rMatrix.d.z;
	}

	void	LoadMatrix(Matrix34& rMatrix) const
	{
		//if this is zero then it means the original matrix should be zero, is sometimes used to hide objects
		if(m_Quaternion.IsZeroRotation())
		{
			rMatrix.Zero();
			return;
		}
		else
		{
			Quaternion q;
			m_Quaternion.LoadQuaternion(q);
			rMatrix.FromQuaternion(q);
			replayAssert(rMatrix.IsOrthonormal());
		}

		rMatrix.d.x = m_Position[0];
		rMatrix.d.y = m_Position[1];
		rMatrix.d.z = m_Position[2];
	}

	void	LoadMatrix(Matrix34& rMatrix, CPacketPositionAndQuaternion20 &nextPosAndRot, float interp) const
	{
		Vector3 rVecNext;
		nextPosAndRot.LoadPosition(rVecNext);

		Quaternion rQuatNext;
		rQuatNext.Identity();

		if(nextPosAndRot.IsZeroRotation())
		{
			//don't interpolate to zero matrix
			interp = 0.0f;
		}
		else
		{
			nextPosAndRot.GetQuaternion(rQuatNext);
		}

		//don't interpolate from a zeroed matrix just set it
		if(m_Quaternion.IsZeroRotation())
		{
			rMatrix.Zero();
			LoadPosition(rMatrix.d);
			return;
		}
		else
		{
			Quaternion rQuatPrev;
			GetQuaternion(rQuatPrev);

			Quaternion rQuatInterp;
			rQuatPrev.PrepareSlerp(rQuatNext);
			rQuatInterp.Slerp(interp, rQuatPrev, rQuatNext);

			rMatrix.FromQuaternion(rQuatInterp);
			replayAssert(rMatrix.IsOrthonormal());
		}

		Vector3 rVecPrev;
		LoadPosition(rVecPrev);
		rMatrix.d = rVecPrev * (1.0f - interp) + rVecNext * interp;
	}

	bool	IsZeroRotation() const	{	return m_Quaternion.IsZeroRotation();	}

	void	Interpolate(const CPacketPositionAndQuaternion20& other, float interp, Matrix34& outMat) const
	{
		Vector3 otherVec;
		other.LoadPosition(otherVec);
		Quaternion otherQuat;
		other.GetQuaternion(otherQuat);

		LoadPosition(outMat);
		Quaternion thisQuat;
		GetQuaternion(thisQuat);


		Quaternion rQuatInterp;
		thisQuat.PrepareSlerp(otherQuat);
		rQuatInterp.Slerp(interp, thisQuat, otherQuat);

		outMat.FromQuaternion(rQuatInterp);
		replayAssert(outMat.IsOrthonormal());

		
		outMat.d = outMat.d * (1.0f - interp) + otherVec * interp;
	}

	bool operator !=(const CPacketPositionAndQuaternion20& other) const
	{
		return m_Position != other.m_Position || m_Quaternion != other.m_Quaternion;
	}

private:
	CPacketVector3		m_Position;
	CPacketQuaternionH	m_Quaternion;
};


//========================================================================================================================
// ReplayCompressedFloat.
//========================================================================================================================

#define REPLAY_COMPRESSED_FLOAT_EXPONENT_BIAS 32
#define REPLAY_COMPRESSED_FLOAT_MANTISSA_BIT_MASK ((0x1 << 23) - 1)
#define REPLAY_COMPRESSED_FLOAT_USE_OPTIMISED_VERSION 1

class ReplayCompressedFloat
{
public:
	operator float() const
	{
		u32 fValueAsInt = GetSign() | GetExponent() | GetMantissa();
		return *((float *)&fValueAsInt);
	}
#if !REPLAY_COMPRESSED_FLOAT_USE_OPTIMISED_VERSION
	void operator=(const float srcVal)
	{
		float f = srcVal;
		u32 fValueAsInt = *((u32 *)&f);

		// Extract the sign bot.
		u32 sign = (fValueAsInt  >> 24) & 0x80;
		// Extract the exponent.
		s32 exponent = (fValueAsInt >> 23) & 0xff;
		// Only store exponents in a range [REPLAY_FLOAT_EXPONENT_BIAS, REPLAY_FLOAT_EXPONENT_BIAS + 127].
		u32 exponentClamped = (exponent < REPLAY_COMPRESSED_FLOAT_EXPONENT_BIAS) ? REPLAY_COMPRESSED_FLOAT_EXPONENT_BIAS : exponent;
		u32 exponentToStore = (exponentClamped - REPLAY_COMPRESSED_FLOAT_EXPONENT_BIAS) & 0x7f;
		m_ExponentPlusSign = (u8)sign | (u8)exponentToStore;

		u32 v = (u32)(((float)(fValueAsInt & REPLAY_COMPRESSED_FLOAT_MANTISSA_BIT_MASK)/(float)REPLAY_COMPRESSED_FLOAT_MANTISSA_BIT_MASK)*(float)0xffff);
		m_MantissaHigh = (v >> 8) & 0xff;
		m_MantissaLow = v & 0xff;
	}
#else // !REPLAY_COMPRESSED_FLOAT_USE_OPTIMISED_VERSION
	// Optimized version.
	void operator=(const float srcVal)
	{
		typedef struct REPLAY_FLOAT_DEST_STRUCT
		{
			u16 MantissaLThenHigh;
			u8 ExponentPlusSign;
		} REPLAY_FLOAT_DEST_STRUCT;

		float f = srcVal;
		REPLAY_FLOAT_DEST_STRUCT *pDest = (REPLAY_FLOAT_DEST_STRUCT *)this;
		u32 fValueAsInt = *((u32 *)&f);

		// Extract the sign bot.
		u32 sign = fValueAsInt & 0x80000000;
		// Extract the exponent.
		s32 exponent = (fValueAsInt & (0xff << 23));
		s32 exponentClamped = exponent - (REPLAY_COMPRESSED_FLOAT_EXPONENT_BIAS << 23);
		s32 msk = ~(exponentClamped >> 31) & (0x7f << 23);
		u32 exponentToStoreShiftedDown1 = (exponentClamped & msk);
		pDest->ExponentPlusSign = (sign + exponentToStoreShiftedDown1 + exponentToStoreShiftedDown1) >> 24;

		u32 v = (u32)((float)(fValueAsInt & REPLAY_COMPRESSED_FLOAT_MANTISSA_BIT_MASK)*((float)0xffff/(float)REPLAY_COMPRESSED_FLOAT_MANTISSA_BIT_MASK));
		pDest->MantissaLThenHigh = (u16)v;
	}
#endif // REPLAY_COMPRESSED_FLOAT_USE_OPTIMISED_VERSION

	u32 GetSign() const
	{
		return ((u32)m_ExponentPlusSign & 0x80) << 24;
	}

	u32 GetExponent() const
	{
		return (((u32)m_ExponentPlusSign & 0x7f) + REPLAY_COMPRESSED_FLOAT_EXPONENT_BIAS) << 23;
	}

	u32 GetMantissa() const 
	{
		u32 v = ((u32)m_MantissaHigh << 8) | (u32)m_MantissaLow;
		return (u32) ((float)v*((float)REPLAY_COMPRESSED_FLOAT_MANTISSA_BIT_MASK/(float)0xffff));
	}
private:
	u8 m_MantissaLow;
	u8 m_MantissaHigh;
	u8 m_ExponentPlusSign;
};


//////////////////////////////////////////////////////////////////////////

#define PACKET_EXTENSION_DEFAULT_ALIGNMENT 4

class CPacketExtension
{
public:
	struct PACKET_EXTENSION_GUARD
	{
		u32 m_DataSize;
		u32 m_CRCOfData;
	};
public:
	CPacketExtension() 
	{
		m_VersionAndOffset = 0;
	}
	~CPacketExtension() 
	{
	};
	void Reset()
	{
		m_VersionAndOffset = 0;
	}
	u8 GetVersion() const
	{
		return (m_VersionAndOffset >> 24);
	}
	u32 GetOffset() const
	{
		return (m_VersionAndOffset & ((0x1 << 24) - 1));
	}

	void *BeginWrite(class CPacketBase *pBase, u8 version);
	void EndWrite(class CPacketBase *pBase, u8 *pPtr);
	void *GetReadPtr(class CPacketBase const*pBase) const;
	bool Validate(class CPacketBase const*pBase) const;
	u32 GetTotalDataSize(class CPacketBase const*pBase) const;
	void CloneExt(class CPacketBase const*pSrc, class CPacketBase *pDst, CPacketExtension& pDstExt) const;
	void RecomputeDataCRC(class CPacketBase *pBase);

	void SetVersion(u8 v)
	{
		m_VersionAndOffset = (m_VersionAndOffset & ((0x1 << 24) - 1)) | (((u32)v & 0xff) << 24);
	}
	void SetOffset(u32 offset)
	{
		replayAssertf(offset < ((0x1 << 24)-1), "CPacketExtension::SetOffset()...Offset too big!");
		m_VersionAndOffset = (m_VersionAndOffset & ~((0x1 << 24)-1)) | (offset & ((0x1 << 24)-1));
	}
private:
	u32 m_VersionAndOffset;
};


#define DECLARE_PACKET_EXTENSION(classname) \
	CPacketExtension classname##_packetextension; \

#define GET_PACKET_EXTENSION_VERSION(classname) \
	classname##_packetextension.GetVersion() \

#define PACKET_EXTENSION_RESET(classname) \
	classname##_packetextension.Reset() \


#define GET_PACKET_EXTENSION_READ_PTR_THIS(classname) \
	classname##_packetextension.GetReadPtr(static_cast < class CPacketBase const* >(this)) \

#define GET_PACKET_EXTENSION_READ_PTR_OTHER(pOther, classname) \
	pOther->classname##_packetextension.GetReadPtr(static_cast < class CPacketBase const* >(pOther)) \

#define GET_PACKET_EXTENSION_READ_PTR_EMBEDDED(pBasePacket, pThis, classname) \
	pThis->classname##_packetextension.GetReadPtr(static_cast < class CPacketBase const* >(pBasePacket)) \

#define GET_PACKET_EXTENSION_SIZE(classname) \
	classname##_packetextension.GetTotalDataSize(static_cast < class CPacketBase const* >(this)) \

#define GET_PACKET_EXTENSION_SIZE_OTHER(pOther, classname) \
	pOther->classname##_packetextension.GetTotalDataSize(static_cast < class CPacketBase const* >(pOther)) \

#define BEGIN_PACKET_EXTENSION_WRITE(version, classname) \
	classname##_packetextension.BeginWrite(static_cast < class CPacketBase * >(this), version) \

#define END_PACKET_EXTENSION_WRITE(pPtr, classname) \
	classname##_packetextension.EndWrite(static_cast < class CPacketBase * >(this), reinterpret_cast < u8 * > (pPtr)) \

#define PACKET_EXTENSION_RECOMPUTE_DATA_CRC(classname) \
	classname##_packetextension.RecomputeDataCRC(static_cast < class CPacketBase * >(this)) \

#define BEGIN_PACKET_EXTENSION_WRITE_EX(pBase, version, classname) \
	classname##_packetextension.BeginWrite(static_cast < class CPacketBase * >(pBase), version) \

#define END_PACKET_EXTENSION_WRITE_EX(pBase, pPtr, classname) \
	classname##_packetextension.EndWrite(static_cast < class  CPacketBase * >(pBase), reinterpret_cast < u8 *> (pPtr)) \

#define VALIDATE_PACKET_EXTENSION(classname) \
	classname##_packetextension.Validate(static_cast < class  CPacketBase const * >(this)) \

#define VALIDATE_PACKET_EXTENSION_EX(pBase, classname) \
	classname##_packetextension.Validate(static_cast < class  CPacketBase const * >(pBase)) \

#define CLONE_PACKET_EXTENSION_DATA(pSrc, pDst, classname) \
	pSrc->classname##_packetextension.CloneExt(static_cast < class CPacketBase const *>(pSrc), static_cast < class CPacketBase * >(pDst), pDst->classname##_packetextension) \

//////////////////////////////////////////////////////////////////////////  
class CEntityChecker
{
public:
	static bool		CheckEntityID(const CReplayID& id)	{	replayFatalAssertf(id != ReplayIDInvalid && id != NoEntityID, "Entity ID is wrong for this packet (%d)", id.ToInt());	return id != ReplayIDInvalid && id != NoEntityID;	}

	template<typename ENTITYTYPE>
	static bool		CheckEntity(const CReplayID& id, ENTITYTYPE* pEntity)
	{
		return CheckEntityID(id) && pEntity != NULL;
	}

	static CReplayID ResetVal;
};

class CEntityCheckerAllowNoEntity
{
public:
	static bool		CheckEntityID(const CReplayID&  id)	{	replayFatalAssertf(id != ReplayIDInvalid, "Entity ID is wrong for this packet (%d)", id.ToInt());	return id != ReplayIDInvalid;	}

	template<typename ENTITYTYPE>
	static bool		CheckEntity(const CReplayID& id, ENTITYTYPE* pEntity)
	{
		return CheckEntityID(id) && (id == NoEntityID || pEntity != NULL);
	}

	static CReplayID ResetVal;
};

const u16 InvalidStaticIndex = 0xffff;
//////////////////////////////////////////////////////////////////////////
// Packets which reference entities during playback will inherit from this
template<u8 ENTITYCOUNT, class ENTITYCHECKER1 = CEntityChecker, class ENTITYCHECKER2 = CEntityChecker>
class CPacketEntityInfo
{
public:
	void				SetReplayID(const CReplayID& id, u8 index = 0)	{	replayFatalAssertf(index < ENTITYCOUNT, "");	m_ReplayID[index] = id;		}
	const CReplayID&	GetReplayID(u8 index = 0) const			{	replayFatalAssertf(index < ENTITYCOUNT, "");	return m_ReplayID[index];	}

	static u8			GetNumEntities()						{	return ENTITYCOUNT;			}

	bool				IsStaticGeometryAware() const			{	return false;				}
	void				SetStaticIndex(u16, u8)					{	replayFatalAssertf(false, "Should not be called");					}
	u16					GetStaticIndex(u8) const				{	return InvalidStaticIndex;	}

	bool				ValidatePacket() const	
	{	
		replayAssertf(ENTITYCHECKER1::CheckEntityID(m_ReplayID[0]), "Validation of CPacketEntityInfo failed");
		if(!ENTITYCHECKER1::CheckEntityID(m_ReplayID[0]))	return false;

		if(ENTITYCOUNT > 1)
		{
			for(u8 i = 1; i < ENTITYCOUNT; ++i)
			{
				replayAssertf(ENTITYCHECKER2::CheckEntityID(m_ReplayID[i]), "Validation of CPacketEntityInfo failed");
				if(!ENTITYCHECKER2::CheckEntityID(m_ReplayID[i]))	return false;
			}
		}
		return true;
	}

	template<typename ENTITYTYPE>
	bool				ValidateForPlayback(const CEventInfo<ENTITYTYPE>& info) const
	{
		if(!ENTITYCHECKER1::CheckEntity(m_ReplayID[0], info.GetEntity(0)))	return false;

		if(ENTITYCOUNT > 1)
		{
			for(u8 i = 1; i < ENTITYCOUNT; ++i)
			{
				if(!ENTITYCHECKER2::CheckEntity(m_ReplayID[i], info.GetEntity(1))) return false;
			}
		}
		return true;
	}

	template<typename ENTITYTYPE>
	bool				ValidateForPlayback(const CTrackedEventInfoBase& info) const
	{
		if(!ENTITYCHECKER1::CheckEntity(m_ReplayID[0], info.pEntity))	return false;

		return true;
	}

	void				PrintXML(FileHandle handle) const
	{
		char str[1024];
		for(u8 i = 0; i < ENTITYCOUNT; ++i)
		{
			snprintf(str, 1024, "\t\t<ReplayID %u>0x%8X</ReplayID  %u>\n", i, m_ReplayID[i].ToInt(), i);
			CFileMgr::Write(handle, str, istrlen(str));	
		}
	}

protected:
	CPacketEntityInfo()
	{
		Reset();
	}

	void				Store(const CReplayID& entityID)
	{
		ENTITYCHECKER1::CheckEntityID(entityID);
		Reset();
		m_ReplayID[0] = entityID;
	}

private:
	void				Reset()
	{
		m_ReplayID[0] = ENTITYCHECKER1::ResetVal;

		if(ENTITYCOUNT > 1)
		{
			for(u8 i = 1; i < ENTITYCOUNT; ++i)
				m_ReplayID[i] = ENTITYCHECKER2::ResetVal;
		}
	}

	CReplayID	m_ReplayID[ENTITYCOUNT];
};


template<>
class CPacketEntityInfo<0>
{
public:
	void				SetReplayID(const CReplayID&, u8 = 0)	{}
	const CReplayID&	GetReplayID(u8 = 0) const				{	return NoEntityID;		}

	static u8			GetNumEntities()						{	return 0;		}

	bool				IsStaticGeometryAware() const			{	return false;	}
	void				SetStaticIndex(u16, u8)					{	replayFatalAssertf(false, "Should not be called");					}
	u16					GetStaticIndex(u8) const				{	return InvalidStaticIndex;	}

	bool				ValidatePacket() const					{	return true;	}
	template<typename ENTITYTYPE>
	bool				ValidateForPlayback(const CEventInfo<ENTITYTYPE>& /*info*/) const
	{	return true;	}
	template<typename ENTITYTYPE>
	bool				ValidateForPlayback(const CTrackedEventInfoBase& /*info*/) const
	{	return true;	}
	void				PrintXML(FileHandle) const	{}

	void				Reset()									{}
	void				Store()									{}
protected:
	CPacketEntityInfo(){}
};


//////////////////////////////////////////////////////////////////////////
static const u16 NON_STATIC_BOUNDS_BUILDING = (u16)-2;
template<u8 ENTITYCOUNT, class ENTITYCHECKER1 = CEntityChecker, class ENTITYCHECKER2 = CEntityChecker>
class CPacketEntityInfoStaticAware : public CPacketEntityInfo<ENTITYCOUNT, ENTITYCHECKER1, ENTITYCHECKER2>
{
public:

	bool	IsStaticGeometryAware() const			{	return true;				}

	void	SetStaticIndex(u16 index, u8 i)			{	replayFatalAssertf(i < ENTITYCOUNT, "");	m_staticIndex[i] = index;		}
	u16		GetStaticIndex(u8 i) const				{	replayFatalAssertf(i < ENTITYCOUNT, "");	return m_staticIndex[i];		}

	void	PrintXML(FileHandle handle) const
	{
		char str[1024];
		for(u8 i = 0; i < ENTITYCOUNT; ++i)
		{
			snprintf(str, 1024, "\t\t<m_staticIndex %u>%u</m_staticIndex %u>\n", i, m_staticIndex[i], i);
			CFileMgr::Write(handle, str, istrlen(str));	
		}

		CPacketEntityInfo<ENTITYCOUNT, ENTITYCHECKER1, ENTITYCHECKER2>::PrintXML(handle);
	}

	bool	ValidatePacket() const					{	return CPacketEntityInfo<ENTITYCOUNT, ENTITYCHECKER1, ENTITYCHECKER2>::ValidatePacket();	}

protected:
	CPacketEntityInfoStaticAware()
	{
		for(int i = 0; i < ENTITYCOUNT; ++i)
		{
			m_staticIndex[i] = InvalidStaticIndex;
		}
	}

private:
	u16			m_staticIndex[ENTITYCOUNT];
};

//////////////////////////////////////////////////////////////////////////
class CReplayInteriorProxy
{
public:
	void SetInterior(fwInteriorLocation location);
	void MakeInvalid();

	u32 GetInteriorProxyHash() const { return m_ProxyHash; }
	void SetInteriorProxyHash(const u32 hash) { m_ProxyHash = hash; }

	s32 GetRoomOrPortalIndex() const { return m_roomOrPortalIndex; }
	void SetRoomOrPortalIndex(const s32 id)	{ m_roomOrPortalIndex = static_cast< s16 >(id); }

	bool GetIsPortalToExterior() const { return m_isPortalToExterior; }
	void SetIsPortalToExterior(const bool id)	{ m_isPortalToExterior = id; }

	bool GetIsAttachedToPortal() const { return m_isAttachedToPortal; }
	void SetIsAttachedToPortal(const bool id)	{ m_isAttachedToPortal = id; }

	void GetInteriorPosition(Vector3& pos) const { m_Position.Load(pos); }
	void SetInteriorPosition(const Vector3& pos) { m_Position.Store(pos); }

	fwInteriorLocation GetInteriorLocation() const;

private:
	CPacketVector<3>	m_Position;
	u32					m_ProxyHash;
	u16					m_isAttachedToPortal	: 1;
	u16					m_isPortalToExterior	: 1;
	s16					m_roomOrPortalIndex		: 14;
};


//////////////////////////////////////////////////////////////////////////
class CBasicEntityPacketData : public CPacketEntityInfo<1>
{
public:
	void	GetPosition(Vector3& position) const	{	m_posAndRot.LoadPosition(position);		}
	void	GetMatrix(Matrix34 &mat) const			{	m_posAndRot.LoadMatrix(mat); }

	void	SetFadeOutForward(bool b)				{	m_fadeOutForward = b;		}
	bool	GetFadeOutForward() const				{	return m_fadeOutForward;	}
	void	SetFadeOutBackward(bool b)				{	m_fadeOutBackward = b;		}
	bool	GetFadeOutBackward() const				{	return m_fadeOutBackward;	}

	void	SetCreationUrgent()						{	m_createUrgent = true;		}
	bool	GetCreationUrgent() const				{	return m_createUrgent;		}

	void	SetIsFirstUpdatePacket(bool b)			{	m_firstUpdatePacket = b;	}
	bool	GetIsFirstUpdatePacket() const			{	return m_firstUpdatePacket != 0;	}

	fwInteriorLocation	GetInteriorLocation() const	{   return m_interiorLocation.GetInteriorLocation(); }

	bool	ValidatePacket() const					{	return CPacketEntityInfo::ValidatePacket();	}

	void	PostStore(const CPhysical* pEntity, const FrameRef& currentFrameRef, const u16 previousBlockFrameCount);

protected:
	void	Store(const CPhysical* pEntity, const CReplayID& replayID);
	void	Extract(CPhysical* pEntity, CBasicEntityPacketData const* pNextPacketData, float interp, const CReplayState& flags, bool DoRemoveAndAdd = true, bool ignoreWarpingFlags = false) const;

	void	SetInInterior(bool b)					{	m_inInterior = b;			}
	bool	GetInInterior() const					{	return m_inInterior != 0;	}

	void	SetIsVisible(bool b)					{	m_isVisible = b;			}
	bool	GetIsVisible() const					{	return m_isVisible;			}

	bool	GetWarpedThisFrame() const				{	return m_VersionUsesWarping && m_WarpedThisFrame;	}

	void	PrintXML(FileHandle handle) const
	{
		CPacketEntityInfo::PrintXML(handle);

		char str[1024];
		snprintf(str, 1024, "\t\t<CreationUrgent>%s</CreationUrgent>\n", m_createUrgent ? "True" : "False");
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<IsFirstUpdate>%s</IsFirstUpdate>\n", GetIsFirstUpdatePacket() == true ? "True" : "False");
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<FadeOutForward>%s</FadeOutForward>\n", m_fadeOutForward == true ? "True" : "False");
		CFileMgr::Write(handle, str, istrlen(str));

		snprintf(str, 1024, "\t\t<FadeOutBackward>%s</FadeOutBackward>\n", m_fadeOutBackward == true ? "True" : "False");
		CFileMgr::Write(handle, str, istrlen(str));
	}

private:

	void	Extract(Matrix34& rMatrix, const Quaternion& rQuatNew, const Vector3& rVecNew, float fInterp) const;
	void	RemoveAndAdd(CPhysical* pEntity, const fwInteriorLocation& interiorLoc, bool inInterior, bool bIgnorePhysics = false) const;

	u8								m_inInterior				:1;
	u8								m_isVisible					:1;
	u8								m_isVisibleInReplayCamera	:1;
	u8								m_createUrgent				:1;
	u8								m_fadeOutForward			:1;
	u8								m_fadeOutBackward			:1;
	u8								m_firstUpdatePacket			:1;
    u8								m_wasCutsceneOwned			:1;


	u16								m_lodDistance				:14;

	// There's no version in this packet, use a bit to know if m_WarpedThisFrame is valid to use, stolen from m_lodDistance.
	u16								m_VersionUsesWarping		:1;
	u16								m_WarpedThisFrame			:1;

	CReplayInteriorProxy			m_interiorLocation;

	CPacketPositionAndQuaternion20	m_posAndRot;
};



class CEntityData
{
public:
	static const s32 NUM_FRAMES_TO_FADE = 5;
	static const s32 NUM_FRAMES_BEFORE_SCORCHING = 5;

	CEntityData() : m_iInstID(-1), m_iFrame(0), m_iFadeInCount(0), m_iFadeOutCount(0), m_iScorchedCount(0), m_uPacketID(0)
	{
		for (s32 packet = 0; packet < NUM_FRAMES_TO_FADE; packet++)
		{
			m_apvPacketFadeIn[packet]  = NULL;
			m_apvPacketFadeOut[packet] = NULL;
		}

		for (s32 packet = 0; packet < NUM_FRAMES_BEFORE_SCORCHING; packet++)
		{
			m_apvPacketScorched[packet]  = NULL;
		}
	}

	void Reset()
	{
		m_iInstID		= -1;
		m_iFrame		= 0;
		m_iFadeInCount	= 0;
		m_iFadeOutCount	= 0;
		m_fadeInCount	= 0;
		m_iScorchedCount	= 0;

		for (s32 packet = 0; packet < NUM_FRAMES_TO_FADE; packet++)
		{
			m_apvPacketFadeIn[packet]  = NULL;
			m_apvPacketFadeOut[packet] = NULL;
		}

		for (s32 packet = 0; packet < NUM_FRAMES_BEFORE_SCORCHING; packet++)
		{
			m_apvPacketScorched[packet]  = NULL;
		}
	}

	void* m_apvPacketFadeIn[NUM_FRAMES_TO_FADE];
	void* m_apvPacketFadeOut[NUM_FRAMES_TO_FADE];
	void* m_apvPacketScorched[NUM_FRAMES_BEFORE_SCORCHING];
	s16	  m_iInstID;
	s16	  m_iFrame;
	s8	  m_iFadeInCount;
	s8	  m_iFadeOutCount;
	s8		m_fadeInCount;
	s8	  m_iScorchedCount;
	u8	  m_uPacketID;
	s8	  m_iPad[3];
};

struct ReplayInteriorData
{
	u32	 m_hashName;
	CPacketVector<3>	m_position;
	bool m_disabled;
	bool m_capped;
};


#endif // GTA_REPLAY

#endif

